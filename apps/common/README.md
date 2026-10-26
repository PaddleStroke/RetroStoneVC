# apps/common: what both apps share

## The in-app purchase (`iap.h`)

One non-consumable product, "unlock everything", bought inside the app. One C API for both apps (RetroStone Board,
RetroStone Arcade) on every platform; the apps' own code decides what it opens and draws the panel:

- **the edition at run time**: the free set (the app's `FREE_GAMES`, and the Board's `FREE_LEVELS`) plays until
  `iap_unlocked()`, then everything opens at once, without a restart. One code path everywhere: where nothing is sold
  in the app, `iap_unlocked()` is always 0 and the compile-time edition alone decides, as before.
- **the panel** (a locked game tapped): `iap_available()` 1: "Unlock everything" with `iap_price_text()`
  (`iap_buy()`; the Arcade's font: `iap_price_ascii()`, "4,99 EUR") and "Restore purchases" (`iap_restore()`), both
  waiting while `iap_busy()`; 0: the "Get it" link to the paid app's store page, as before.
- **the unlock kept**: the app saves it in its own save store and gives it back to `iap_init()` at the next launch
  (offline too); `iap_changes()`, polled once a frame, says when the store answers (bought, restored, refunded).

| backend | where | |
|---|---|---|
| `iap_none.c` | desktop; Android without `IAP=1` | nothing sold in the app: `iap_available()` 0, never unlocked, no tips |
| `apps/ios/common/iap_storekit.swift` | iPhone, iPad | StoreKit 2: the price, the purchase sheet, the transactions checked at every launch (no tips yet) |
| `iap_play.c` + `android/java/com/eightbcraft/iap/Billing.java` | Android, `IAP=1` | Google Play Billing: the unlock and the tips (below) |

### Google Play Billing (Android)

The switch is `IAP=1` in the app's `.conf` (the RetroStone app has it, for its tips): `apps/boardgames/Makefile` then
builds `iap_play.c` into the Android `libmain.so` (the desktop builds keep `iap_none.c`), and
`apps/android/app/build.gradle` adds the Java class and the Play Billing Library (`com.android.billingclient:billing`,
9.1.0; its licences: `apps/boardgames/tools/gen_arcade.py`'s `BILLING`). The products: `TIP_PRODUCTS` (the tips,
consumables) and `IAP_PRODUCT` (the unlock, a non-consumable; none in RetroStone), created in the Play Console with the
same ids.

- **C to Java** (`iap_play.c`, on the app's thread): `Billing.configure(activity, unlock, tips)` at `iap_init` and
  `iap_tips_init`, `Billing.buy(slot)` (-1 the unlock, 0.. a tip), `Billing.restore()`. The class is found through
  the activity's class loader (a native thread's `FindClass` sees only the system's classes).
- **Java** (`Billing.java`, on Android's main thread): one `BillingClient` with pending purchases and the automatic
  reconnection; the products' details (their prices: `getFormattedPrice()` with Latin digits and plain spaces, and an
  ASCII form, "4,99 EUR"); `launchBillingFlow`; `onPurchasesUpdated`; `queryPurchasesAsync` at the start, whenever
  the app comes back to the front and on restore. A tip paid is consumed (`consumeAsync`), then counted; one paid
  later (a pending payment) is found by the next query. The unlock owned is acknowledged and reported; missing from a
  successful query, it is reported gone (a refund).
- **Java to C**: `nativeReady`, `nativePrice`, `nativeBusy`, `nativeUnlocked`, `nativeTipDone` store the state under
  a lock and bump `iap_changes()`; the app reads it once a frame. `iap_tips_available()` is the tiers' count once the
  store is connected (0 without Google Play: the Settings page hides "Leave a tip"); a tier's price is "" until its
  details come (the panel shows "..."), and `iap_tip_buy` refuses until then.
- **The Arcade tab** sells nothing of its own: `tools/gen_arcade.py` leaves `IAP` and `IAP_PRODUCT` out of the
  bundle's `.conf`, so one backend, the shell's, is linked.
- **Testing**: purchases work only in a build signed with the upload key, installed from a Play test track
  (internal testing), with the products created and active in the Console and a license tester's account on the
  phone (Setup > License testing: test cards, nothing charged). A debug build from `build.sh` connects to the store
  but may find no products (another signature, a package Play does not serve it).
