/*
 * In-app purchase (apps/common/iap.h): a stand-in for StoreKit on the desktop, to see the iOS apps' unlock panel in
 * pictures (apps/ios/prepare.sh <app> --preview, the picture mode of the desktop build). Never shipped.
 *   IAP_PREVIEW_PRICE="4,99 €"   the price shown (default "4,99 €", in ASCII "4,99 EUR"; "": not known yet)
 *   IAP_PREVIEW_BUSY=1           a purchase in progress (the buttons dimmed)
 *   IAP_PREVIEW_UNLOCKED=1       bought (nothing locked)
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "iap.h"

#include <stdlib.h>

static int unlocked, changes;

static int env_on(const char *k)
{
    const char *e = getenv(k);
    return e && e[0] == '1';
}

void iap_init(const char *product_id, int unlocked_before)
{
    (void)product_id;
    unlocked = unlocked_before;
}

int iap_available(void) { return 1; }
int iap_unlocked(void) { return unlocked || env_on("IAP_PREVIEW_UNLOCKED"); }
int iap_busy(void) { return env_on("IAP_PREVIEW_BUSY"); }

const char *iap_price_text(void)
{
    const char *e = getenv("IAP_PREVIEW_PRICE");
    return e ? e : "4,99 \xe2\x82\xac";
}

const char *iap_price_ascii(void)
{
    const char *e = getenv("IAP_PREVIEW_PRICE");
    return e ? (e[0] ? "4,99 EUR" : "") : "4,99 EUR";
}

int iap_buy(void)
{
    unlocked = 1;
    changes++;
    return 1;
}

void iap_restore(void) { changes++; }
unsigned iap_changes(void) { return (unsigned)changes; }
