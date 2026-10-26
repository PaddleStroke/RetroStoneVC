/*
 * In-app purchase on Android: Google Play Billing, for apps/common/iap_play.c (the C side of apps/common/iap.h).
 * Built into an app whose .conf says IAP=1 (apps/android/app/build.gradle adds this folder and the Billing library).
 *
 * The products: the tips (consumables, bought again and again: each purchase is consumed at once and counted) and,
 * optionally, the unlock (one non-consumable, acknowledged). The C side calls configure() with their ids (again when
 * they change), buy() and restore(); this class answers through the native methods below, which only store the state
 * for the C side to read on its own thread (the Billing Library calls back on the main thread).
 *
 *  - the connection: once, with the library's automatic reconnection; ready() tells C the store can sell.
 *  - the products' details (their prices) are asked for once ready, and again when a purchase is tried before they
 *    came (offline at start-up).
 *  - the purchases: those the purchase sheet returns (onPurchasesUpdated) and those found again (queryPurchasesAsync:
 *    at start, whenever the app comes back to the front, and on restore()): a tip bought (and, after a pending
 *    payment, paid) is consumed then counted; the unlock owned is acknowledged and reported; the unlock missing from
 *    a successful query is reported gone (refunded).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
package com.eightbcraft.iap;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

import com.android.billingclient.api.AcknowledgePurchaseParams;
import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.ConsumeParams;
import com.android.billingclient.api.PendingPurchasesParams;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.PurchasesUpdatedListener;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryPurchasesParams;

import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Currency;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public final class Billing implements PurchasesUpdatedListener, BillingClientStateListener {
    /* the C side (iap_play.c): slot -1 is the unlock, 0.. the tips in tier order */
    private static native void nativeReady(int ready);
    private static native void nativePrice(int slot, String text, String ascii);
    private static native void nativeBusy(int busy);
    private static native void nativeUnlocked(int owned);
    private static native void nativeTipDone();

    private static Billing self;

    private final Activity activity;
    private final BillingClient client;
    private String unlock = "";
    private String[] tips = new String[0];
    private ProductDetails unlockDetails;
    private ProductDetails[] tipDetails = new ProductDetails[0];
    private final Set<String> consuming = new HashSet<>();    /* the tokens being consumed: each counted once */
    private boolean restoring;

    /* ---- called by iap_play.c, on the app's native thread ---------------------------------------------------------- */

    /* the products: unlock ("" none) and the tips; the first call starts the store */
    public static void configure(final Activity a, final String unlock, final String[] tips) {
        a.runOnUiThread(() -> {
            if (self == null) self = new Billing(a);
            self.unlock = unlock == null ? "" : unlock;
            self.tips = tips == null ? new String[0] : tips;
            self.unlockDetails = null;
            self.tipDetails = new ProductDetails[self.tips.length];
            if (self.client.isReady()) {
                self.queryProducts();
                self.queryPurchases();
            }
        });
    }

    /* the purchase sheet for a product (slot -1: the unlock); nativeBusy(0) when it is over, whatever happened */
    public static void buy(final int slot) {
        final Billing b = self;
        if (b == null) {
            nativeBusy(0);
            return;
        }
        b.activity.runOnUiThread(() -> b.launch(slot));
    }

    /* the purchases made with this account, asked for again (another device, a reinstall) */
    public static void restore() {
        final Billing b = self;
        if (b == null) {
            nativeBusy(0);
            return;
        }
        b.activity.runOnUiThread(() -> {
            b.restoring = true;
            if (b.client.isReady()) b.queryPurchases();
            else {
                b.restoring = false;
                nativeBusy(0);
            }
        });
    }

    /* ---- the store ---------------------------------------------------------------------------------------------------- */

    private Billing(Activity a) {
        activity = a;
        client = BillingClient.newBuilder(a.getApplicationContext())
                .setListener(this)
                .enablePendingPurchases(PendingPurchasesParams.newBuilder().enableOneTimeProducts().build())
                .enableAutoServiceReconnection()
                .build();
        client.startConnection(this);
        /* back in front (a payment finished elsewhere, a refund): the purchases again */
        a.getApplication().registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks() {
            @Override public void onActivityResumed(Activity x) {
                if (x == activity && client.isReady()) queryPurchases();
            }
            @Override public void onActivityCreated(Activity x, Bundle b) {}
            @Override public void onActivityStarted(Activity x) {}
            @Override public void onActivityPaused(Activity x) {}
            @Override public void onActivityStopped(Activity x) {}
            @Override public void onActivitySaveInstanceState(Activity x, Bundle b) {}
            @Override public void onActivityDestroyed(Activity x) {}
        });
    }

    @Override
    public void onBillingSetupFinished(BillingResult r) {
        boolean ok = r.getResponseCode() == BillingClient.BillingResponseCode.OK;
        nativeReady(ok ? 1 : 0);
        if (ok) {
            queryProducts();
            queryPurchases();
        }
    }

    @Override
    public void onBillingServiceDisconnected() {
        /* the library reconnects by itself (enableAutoServiceReconnection) on the next call */
    }

    private void queryProducts() {
        List<QueryProductDetailsParams.Product> list = new ArrayList<>();
        if (!unlock.isEmpty()) list.add(product(unlock));
        for (String t : tips) list.add(product(t));
        if (list.isEmpty()) return;
        final String u = unlock;
        final String[] ts = tips;
        client.queryProductDetailsAsync(QueryProductDetailsParams.newBuilder().setProductList(list).build(),
            (r, res) -> activity.runOnUiThread(() -> {
                if (r.getResponseCode() != BillingClient.BillingResponseCode.OK || res == null) return;
                if (u != unlock || ts != tips) return;                  /* configured again meanwhile */
                for (ProductDetails d : res.getProductDetailsList()) {
                    ProductDetails.OneTimePurchaseOfferDetails o = d.getOneTimePurchaseOfferDetails();
                    if (o == null) continue;
                    int slot = d.getProductId().equals(unlock) ? -1 : index(tips, d.getProductId());
                    if (slot == -1) unlockDetails = d;
                    else if (slot >= 0) tipDetails[slot] = d;
                    else continue;
                    nativePrice(slot, latin(o.getFormattedPrice()), ascii(o));
                }
            }));
    }

    private static QueryProductDetailsParams.Product product(String id) {
        return QueryProductDetailsParams.Product.newBuilder().setProductId(id)
                .setProductType(BillingClient.ProductType.INAPP).build();
    }

    private void launch(int slot) {
        ProductDetails d = slot < 0 ? unlockDetails : slot < tipDetails.length ? tipDetails[slot] : null;
        if (d == null || !client.isReady()) {
            if (client.isReady()) queryProducts();                     /* the prices may come now */
            nativeBusy(0);
            return;
        }
        BillingFlowParams p = BillingFlowParams.newBuilder().setProductDetailsParamsList(Collections.singletonList(
                BillingFlowParams.ProductDetailsParams.newBuilder().setProductDetails(d).build())).build();
        BillingResult r = client.launchBillingFlow(activity, p);
        if (r.getResponseCode() != BillingClient.BillingResponseCode.OK) nativeBusy(0);
    }

    /* the purchase sheet's answer */
    @Override
    public void onPurchasesUpdated(BillingResult r, List<Purchase> purchases) {
        int code = r.getResponseCode();
        if (code == BillingClient.BillingResponseCode.OK && purchases != null) {
            for (Purchase p : purchases) handle(p);
        } else if (code == BillingClient.BillingResponseCode.ITEM_ALREADY_OWNED) {
            queryPurchases();                                           /* the unlock found again, or a tip left */
        }
        nativeBusy(0);
    }

    private void queryPurchases() {
        client.queryPurchasesAsync(QueryPurchasesParams.newBuilder().setProductType(BillingClient.ProductType.INAPP)
                .build(), (r, purchases) -> activity.runOnUiThread(() -> {
            if (r.getResponseCode() == BillingClient.BillingResponseCode.OK && purchases != null) {
                boolean owned = false;
                for (Purchase p : purchases) {
                    if (p.getPurchaseState() == Purchase.PurchaseState.PURCHASED && p.getProducts().contains(unlock))
                        owned = true;
                    handle(p);
                }
                if (!unlock.isEmpty() && !owned) nativeUnlocked(0);    /* refunded, or never bought */
            }
            if (restoring) {
                restoring = false;
                nativeBusy(0);
            }
        }));
    }

    /* one purchase: a tip paid is consumed, then counted; the unlock paid is reported and acknowledged. A pending one
     * (a payment not made yet) waits: it comes back through queryPurchases once paid. */
    private void handle(Purchase p) {
        if (p.getPurchaseState() != Purchase.PurchaseState.PURCHASED) return;
        List<String> ids = p.getProducts();
        if (!unlock.isEmpty() && ids.contains(unlock)) {
            nativeUnlocked(1);
            if (!p.isAcknowledged())
                client.acknowledgePurchase(AcknowledgePurchaseParams.newBuilder()
                        .setPurchaseToken(p.getPurchaseToken()).build(), res -> {});
            return;
        }
        boolean tip = false;
        for (String t : tips) tip |= ids.contains(t);
        final String token = p.getPurchaseToken();
        if (!tip || !consuming.add(token)) return;
        client.consumeAsync(ConsumeParams.newBuilder().setPurchaseToken(token).build(), (res, tok) ->
            activity.runOnUiThread(() -> {
                consuming.remove(token);
                if (res.getResponseCode() == BillingClient.BillingResponseCode.OK) nativeTipDone();
            }));
    }

    /* ---- the prices as the app's fonts draw them ---------------------------------------------------------------------- */

    private static int index(String[] a, String s) {
        for (int i = 0; i < a.length; i++)
            if (a[i].equals(s)) return i;
        return -2;
    }

    /* Latin digits and plain spaces (the app's fonts: some locales write other digits, and no-break spaces) */
    private static String latin(String s) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < s.length(); ) {
            int c = s.codePointAt(i);
            i += Character.charCount(c);
            if (Character.isDigit(c)) b.append((char) ('0' + Character.digit(c, 10)));
            else if (Character.isSpaceChar(c)) b.append(' ');
            else if (c != 0x200e && c != 0x200f && c != 0x061c) b.appendCodePoint(c);   /* no direction marks */
        }
        return b.toString().trim();
    }

    /* the price in ASCII, its currency as the ISO code: "4,99 EUR" (the Arcade's font has no currency signs) */
    private static String ascii(ProductDetails.OneTimePurchaseOfferDetails o) {
        String code = o.getPriceCurrencyCode();
        NumberFormat f = NumberFormat.getNumberInstance();
        int digits = 2;
        try {
            digits = Math.max(0, Currency.getInstance(code).getDefaultFractionDigits());
        } catch (IllegalArgumentException e) { /* an unknown code: two digits */ }
        f.setMinimumFractionDigits(digits);
        f.setMaximumFractionDigits(digits);
        f.setGroupingUsed(false);
        String n = latin(f.format(o.getPriceAmountMicros() / 1e6)).replaceAll("[^0-9.,]", "");
        return n + " " + code;
    }
}
