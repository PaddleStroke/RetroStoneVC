# apps/common: what both apps share

## The in-app purchase (`iap.h`)

One non-consumable product, "unlock everything", bought inside the app. One C API for both apps (RetroStone Board,
RetroStone Arcade) on every platform; the apps' own code decides what it opens and draws the panel:

- **the edition at run time**: the free set (the app's `FREE_GAMES`, and the Board's `FREE_LEVELS`) plays until
  `iap_unlocked()`, then everything opens at once, without a restart. One code path everywhere: where nothing is sold
  in the app, `iap_unlocked()` is always 0 and the compile-time edition alone decides, as before.
- **the panel** (a locked game tapped): `iap_available()` 1: "Unlock everything" with `iap_price_text()`
  (`iap_buy()`) and "Restore purchases" (`iap_restore()`), both waiting while `iap_busy()`; 0: the "Get it" link to
  the paid app's store page, as before.
- **the unlock kept**: the app saves it in its own save store and gives it back to `iap_init()` at the next launch
  (offline too); `iap_changes()`, polled once a frame, says when the store answers (bought, restored, refunded).

| backend | where | |
|---|---|---|
| `iap_none.c` | desktop, Android today | nothing sold in the app: `iap_available()` 0, never unlocked |
| `apps/ios/common/iap_storekit.swift` | iPhone, iPad | StoreKit 2: the price, the purchase sheet, the transactions checked at every launch |
| `iap_play.c` (not written yet) | Android | Google Play Billing (below) |

### The slot for Google Play Billing (Android)

Not written yet. When it is, it is one backend file plus a build switch, nothing else to change in the apps:

1. `apps/common/iap_play.c`: the eight functions of `iap.h` over JNI, as the Arcade's review prompt calls
   `ArcadeActivity.requestReview` (`SDL_AndroidGetJNIEnv()`, `SDL_AndroidGetActivity()`); a small Java class beside the
   activity (`com.eightbcraft.iap.Billing`: Play Billing Library's `BillingClient`, `queryProductDetailsAsync`,
   `launchBillingFlow`, `queryPurchasesAsync` at launch, `acknowledgePurchase`) calling back into C with the
   state; the state read by C on the main loop's thread (a lock, or SDL's event queue).
2. The switch: `IAP=1` in the app's `.conf`. `apps/boardgames/Makefile` already picks `iap_play.c` instead of
   `iap_none.c` when it says so (and stops with a clear message while the file does not exist); Gradle would add the
   Billing library and the Java class on the same key, and `IAP_PRODUCT` names the Play product.
3. The edition: a free `.conf` with `IAP=1` is the one free app with the unlock inside, as on iOS.
