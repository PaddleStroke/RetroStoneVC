/*
 * In-app purchase: one non-consumable "unlock everything" bought inside the app, the same C API for both apps
 * (RetroStone Board: apps/boardgames, RetroStone Arcade: apps/launcher) on every platform. The app's own code decides
 * what the purchase opens (its free set until iap_unlocked(), then everything at once) and draws the panel; a backend
 * talks to the platform's store:
 *
 *   apps/common/iap_none.c          no in-app purchase (desktop, Android today): iap_available() 0, never unlocked,
 *                                   so the apps behave exactly as their compile-time editions (the "Get it" link
 *                                   to the paid app's store page)
 *   apps/ios/common/iap_storekit.swift   iPhone and iPad: StoreKit 2 (the product's price, the purchase sheet,
 *                                   the transactions checked again at every launch, refunds, Family Sharing)
 *   (apps/common/iap_play.c)        the slot for Google Play Billing on Android, not written yet: a Java side
 *                                   (BillingClient) called through JNI, as ArcadeActivity's review prompt is; the
 *                                   apps' builds take it instead of iap_none.c when the app's .conf says IAP=1
 *                                   (apps/common/README.md)
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

/* 1: a purchase or a restore is in progress (the store's own sheet may be up): the panel's buttons wait */
int iap_busy(void);

/* opens the store's purchase sheet; 0: it cannot now (the product not loaded yet: it is asked again; nothing sold) */
int iap_buy(void);

/* asks the store for the purchases made with this account (another device, a reinstall) */
void iap_restore(void);

/* a number that changes whenever any of the above does (unlocked, the price, busy): compare it with the last one seen
 * once a frame */
unsigned iap_changes(void);

#ifdef __cplusplus
}
#endif

#endif
