/*
 * In-app purchase: one non-consumable "unlock everything" bought inside the app, the same C API for both apps
 * (RetroStone Board: apps/boardgames, RetroStone Arcade: apps/launcher) on every platform. The app's own code decides
 * what the purchase opens (its free set until iap_unlocked(), then everything at once) and draws the panel; a backend
 * talks to the platform's store:
 *
 *   apps/common/iap_none.c          no in-app purchase (the desktop, and Android without IAP=1): iap_available() 0,
 *                                   never unlocked, so the apps behave exactly as their compile-time editions (the
 *                                   "Get it" link to the paid app's store page), no tips
 *   apps/ios/common/iap_storekit.swift   iPhone and iPad: StoreKit 2 (the product's price, the purchase sheet,
 *                                   the transactions checked again at every launch, refunds, Family Sharing)
 *   apps/common/iap_play.c          Android, the app's .conf says IAP=1: Google Play Billing, through its Java class
 *                                   (apps/common/android/java, com.eightbcraft.iap.Billing: the BillingClient);
 *                                   the unlock and the tips (apps/common/README.md)
 *
 * The app keeps the unlock in its own save store and gives it back to iap_init() at the next launch, so a purchase
 * opens everything at once, offline too; the backend then checks the store's transactions and says so through
 * iap_changes() if the purchase is gone (refunded) or found (bought on another device, restored).
 * Every call is cheap and never blocks: poll iap_changes() once a frame. Main thread only.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef IAP_H
#define IAP_H

#ifdef __cplusplus
extern "C" {
#endif

/* starts the backend: product_id (App Store Connect's product id, "com.eightbcraft.retrostoneboard.unlock"; NULL or
 * "": none, nothing sold), unlocked_before: what the app kept from the last run (1: bought) */
void iap_init(const char *product_id, int unlocked_before);

/* 1: this build sells the unlock inside the app (the panel offers Unlock and Restore), 0: it does not (the panel
 * links to the paid app's store page, as before) */
int iap_available(void);

/* 1: everything is unlocked (bought, restored, or kept from the last run until the store says otherwise) */
int iap_unlocked(void);

/* the product's price as the store gives it, ready to show: "4,99 €", "$4.99" (Latin digits, plain spaces); "" while
 * it is not known (the store not reached yet, offline) */
const char *iap_price_text(void);

/* the same price in ASCII, its currency as the ISO code: "4,99 EUR", "USD 4.99" (for a font without the currency
 * signs: the Arcade's); "" while it is not known */
const char *iap_price_ascii(void);

/* 1: a purchase or a restore is in progress (the store's own sheet may be up): the panel's buttons wait */
int iap_busy(void);

/* opens the store's purchase sheet; 0: it cannot now (the product not loaded yet: it is asked again; nothing sold) */
int iap_buy(void);

/* asks the store for the purchases made with this account (another device, a reinstall) */
void iap_restore(void);

/* a number that changes whenever any of the above does (unlocked, the price, busy): compare it with the last one seen
 * once a frame */
unsigned iap_changes(void);


/* ---- tips: consumable products, bought as often as one likes, that open nothing (the Settings page's "Leave a
 * tip": three tiers, small to big). The ids come from the app's .conf (TIP_PRODUCTS). iap_none.c sells none (the page
 * then opens the app's support page, SUPPORT_URL, or hides the row); a backend that sells them (Google Play Billing:
 * iap_play.c) loads their prices and consumes each purchase at once.
 * TODO(billing): the iOS app's StoreKit backend (apps/ios/common) does not sell tips yet; it defines IAP_HAS_TIPS once
 * it does; until then the iOS build gets the inline "none" below. */
#if defined(__IPHONEOS__) && !defined(IAP_HAS_TIPS)
static inline void iap_tips_init(const char *const *product_ids, int n) { (void)product_ids; (void)n; }
static inline int iap_tips_available(void) { return 0; }
static inline const char *iap_tip_price(int tier) { (void)tier; return ""; }
static inline int iap_tip_buy(int tier) { (void)tier; return 0; }
static inline unsigned iap_tips_done(void) { return 0; }
#else
/* starts the tips: the products' ids, n of them (the tiers in order) */
void iap_tips_init(const char *const *product_ids, int n);
/* how many tiers this build sells (0: none: the app shows no tip panel) */
int iap_tips_available(void);
/* tier's price as the store gives it ("0,99 €"); "" while not known */
const char *iap_tip_price(int tier);
/* opens the store's purchase sheet for that tier; 0: it cannot now (iap_busy() is 1 while it works) */
int iap_tip_buy(int tier);
/* the tips bought (and consumed) since the start: the app thanks the player when it grows */
unsigned iap_tips_done(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
