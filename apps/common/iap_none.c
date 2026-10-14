/*
 * In-app purchase (iap.h): the backend for the builds that sell nothing inside the app (desktop, Android today). Never
 * available, never unlocked: the apps keep their compile-time editions and the "Get it" link to the paid app.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "iap.h"

void iap_init(const char *product_id, int unlocked_before)
{
    (void)product_id;
    (void)unlocked_before;
}

int iap_available(void) { return 0; }
int iap_unlocked(void) { return 0; }
const char *iap_price_text(void) { return ""; }
const char *iap_price_ascii(void) { return ""; }
int iap_busy(void) { return 0; }
int iap_buy(void) { return 0; }
void iap_restore(void) {}
unsigned iap_changes(void) { return 0; }
