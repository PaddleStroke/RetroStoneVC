/*
 * In-app purchase (iap.h) on Android: Google Play Billing, through the Java class com.eightbcraft.iap.Billing
 * (apps/common/android/java: the Billing Library's BillingClient). Built instead of iap_none.c into the Android build
 * of an app whose .conf says IAP=1 (apps/boardgames/Makefile; apps/android/app/build.gradle adds the Java class and the
 * library on the same key); the desktop builds of that app keep iap_none.c.
 *
 * The C side calls Java (configure, buy, restore) on the app's own thread, through the activity's class loader (a
 * native thread's FindClass sees only the system's classes). Java answers on Android's main thread through the
 * native methods at the end, which only store the state under a lock: the app reads it on its own thread, once a
 * frame (iap_changes), as iap.h asks. The unlock (iap_init's product, if any) and the tips (iap_tips_init's) share
 * one store connection and one "busy" (the purchase sheet is one at a time).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "iap.h"

#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "SDL.h"

#define TIPS_MAX 8

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct {
    char unlock_id[128];
    char tip_id[TIPS_MAX][128];
    int ntips;
    int ready;                                  /* the store connected and able to sell */
    int unlocked;
    int busy;
    unsigned tips_done;
    unsigned changes;
    char price[TIPS_MAX + 1][48], ascii[TIPS_MAX + 1][48];   /* [0]: the unlock, [1 + tier]: the tips */
} st;

/* what the app reads (copies: the strings are kept until they change, for the app's frame) */
static char price_out[TIPS_MAX + 1][48], ascii_out[TIPS_MAX + 1][48];

/* ---- calling Java ---------------------------------------------------------------------------------------------------- */

static jclass billing_class(JNIEnv *env, jobject activity)
{
    jclass ac = (*env)->GetObjectClass(env, activity);
    jmethodID gl = (*env)->GetMethodID(env, ac, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject loader = (*env)->CallObjectMethod(env, activity, gl);
    jclass lc = (*env)->FindClass(env, "java/lang/ClassLoader");
    jmethodID load = (*env)->GetMethodID(env, lc, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring name = (*env)->NewStringUTF(env, "com.eightbcraft.iap.Billing");
    jclass c = (jclass)(*env)->CallObjectMethod(env, loader, load, name);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        c = NULL;
    }
    (*env)->DeleteLocalRef(env, name);
    (*env)->DeleteLocalRef(env, lc);
    (*env)->DeleteLocalRef(env, loader);
    (*env)->DeleteLocalRef(env, ac);
    return c;
}

/* Billing.configure(activity, the unlock's id, the tips' ids): again whenever iap_init or iap_tips_init changes them */
static void configure(void)
{
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject activity = env ? (jobject)SDL_AndroidGetActivity() : NULL;
    if (!activity) return;
    jclass c = billing_class(env, activity);
    if (c) {
        jmethodID m = (*env)->GetStaticMethodID(env, c, "configure",
                                                "(Landroid/app/Activity;Ljava/lang/String;[Ljava/lang/String;)V");
        pthread_mutex_lock(&lock);
        char unlock[128];
        char tips[TIPS_MAX][128];
        int n = st.ntips;
        memcpy(unlock, st.unlock_id, sizeof unlock);
        memcpy(tips, st.tip_id, sizeof tips);
        pthread_mutex_unlock(&lock);
        jclass sc = (*env)->FindClass(env, "java/lang/String");
        jobjectArray arr = (*env)->NewObjectArray(env, n, sc, NULL);
        for (int i = 0; i < n; i++) {
            jstring s = (*env)->NewStringUTF(env, tips[i]);
            (*env)->SetObjectArrayElement(env, arr, i, s);
            (*env)->DeleteLocalRef(env, s);
        }
        jstring u = (*env)->NewStringUTF(env, unlock);
        if (m) (*env)->CallStaticVoidMethod(env, c, m, activity, u, arr);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            SDL_Log("iap: Billing.configure failed");
        }
        (*env)->DeleteLocalRef(env, u);
        (*env)->DeleteLocalRef(env, arr);
        (*env)->DeleteLocalRef(env, sc);
        (*env)->DeleteLocalRef(env, c);
    } else {
        SDL_Log("iap: com.eightbcraft.iap.Billing not found (the app built without IAP=1?)");
    }
    (*env)->DeleteLocalRef(env, activity);
}

/* Billing.buy(slot) or Billing.restore() (name "buy" with slot, or "restore"); 0: Java could not be reached */
static int call(const char *name, int slot)
{
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject activity = env ? (jobject)SDL_AndroidGetActivity() : NULL;
    if (!activity) return 0;
    int ok = 0;
    jclass c = billing_class(env, activity);
    if (c) {
        int buy = !strcmp(name, "buy");
        jmethodID m = (*env)->GetStaticMethodID(env, c, name, buy ? "(I)V" : "()V");
        if (m) {
            if (buy) (*env)->CallStaticVoidMethod(env, c, m, (jint)slot);
            else (*env)->CallStaticVoidMethod(env, c, m);
            ok = !(*env)->ExceptionCheck(env);
        }
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        (*env)->DeleteLocalRef(env, c);
    }
    (*env)->DeleteLocalRef(env, activity);
    return ok;
}

/* ---- iap.h: the unlock --------------------------------------------------------------------------------------------- */

void iap_init(const char *product_id, int unlocked_before)
{
    pthread_mutex_lock(&lock);
    snprintf(st.unlock_id, sizeof st.unlock_id, "%s", product_id ? product_id : "");
    st.unlocked = st.unlock_id[0] && unlocked_before;
    st.price[0][0] = st.ascii[0][0] = 0;
    st.changes++;
    pthread_mutex_unlock(&lock);
    configure();
}

int iap_available(void)
{
    pthread_mutex_lock(&lock);
    int r = st.ready && st.unlock_id[0];
    pthread_mutex_unlock(&lock);
    return r;
}

int iap_unlocked(void)
{
    pthread_mutex_lock(&lock);
    int r = st.unlocked;
    pthread_mutex_unlock(&lock);
    return r;
}

static const char *price_of(int k, int want_ascii)
{
    pthread_mutex_lock(&lock);
    char *out = want_ascii ? ascii_out[k] : price_out[k];
    memcpy(out, want_ascii ? st.ascii[k] : st.price[k], sizeof st.price[k]);
    pthread_mutex_unlock(&lock);
    return out;
}

const char *iap_price_text(void) { return price_of(0, 0); }
const char *iap_price_ascii(void) { return price_of(0, 1); }

int iap_busy(void)
{
    pthread_mutex_lock(&lock);
    int r = st.busy;
    pthread_mutex_unlock(&lock);
    return r;
}

/* the sheet for slot (-1 the unlock, 0.. a tip), once its price is known (the product loaded); 0: not now */
static int buy(int slot)
{
    pthread_mutex_lock(&lock);
    int k = slot + 1;
    int ok = st.ready && !st.busy && st.price[k][0];
    if (ok) {
        st.busy = 1;
        st.changes++;
    }
    pthread_mutex_unlock(&lock);
    if (ok && !call("buy", slot)) {
        pthread_mutex_lock(&lock);
        st.busy = 0;
        st.changes++;
        pthread_mutex_unlock(&lock);
        ok = 0;
    }
    return ok;
}

int iap_buy(void)
{
    pthread_mutex_lock(&lock);
    int has = st.unlock_id[0] != 0;
    pthread_mutex_unlock(&lock);
    return has && buy(-1);
}

void iap_restore(void)
{
    pthread_mutex_lock(&lock);
    int go = st.ready && !st.busy;
    if (go) {
        st.busy = 1;
        st.changes++;
    }
    pthread_mutex_unlock(&lock);
    if (go && !call("restore", 0)) {
        pthread_mutex_lock(&lock);
        st.busy = 0;
        st.changes++;
        pthread_mutex_unlock(&lock);
    }
}

unsigned iap_changes(void)
{
    pthread_mutex_lock(&lock);
    unsigned r = st.changes;
    pthread_mutex_unlock(&lock);
    return r;
}

/* ---- iap.h: the tips ----------------------------------------------------------------------------------------------- */

void iap_tips_init(const char *const *product_ids, int n)
{
    if (n > TIPS_MAX) n = TIPS_MAX;
    pthread_mutex_lock(&lock);
    st.ntips = 0;
    for (int i = 0; i < n; i++)
        if (product_ids[i] && product_ids[i][0])
            snprintf(st.tip_id[st.ntips++], sizeof st.tip_id[0], "%s", product_ids[i]);
    for (int i = 1; i <= TIPS_MAX; i++) st.price[i][0] = st.ascii[i][0] = 0;
    st.changes++;
    pthread_mutex_unlock(&lock);
    configure();
}

/* the tiers sold: all of them once the store is connected (their prices may still be on their way: the panel shows
 * "..." until then); 0 without Google Play (the Settings page then hides the row, or opens SUPPORT_URL) */
int iap_tips_available(void)
{
    pthread_mutex_lock(&lock);
    int r = st.ready ? st.ntips : 0;
    pthread_mutex_unlock(&lock);
    return r;
}

const char *iap_tip_price(int tier)
{
    if (tier < 0 || tier >= TIPS_MAX) return "";
    return price_of(1 + tier, 0);
}

int iap_tip_buy(int tier)
{
    pthread_mutex_lock(&lock);
    int ok = tier >= 0 && tier < st.ntips;
    pthread_mutex_unlock(&lock);
    return ok && buy(tier);
}

unsigned iap_tips_done(void)
{
    pthread_mutex_lock(&lock);
    unsigned r = st.tips_done;
    pthread_mutex_unlock(&lock);
    return r;
}

/* ---- Java's answers (com.eightbcraft.iap.Billing's native methods, on Android's main thread) ---------------------- */

JNIEXPORT void JNICALL Java_com_eightbcraft_iap_Billing_nativeReady(JNIEnv *env, jclass cls, jint ready)
{
    (void)env;
    (void)cls;
    pthread_mutex_lock(&lock);
    st.ready = ready != 0;
    st.changes++;
    pthread_mutex_unlock(&lock);
}

JNIEXPORT void JNICALL Java_com_eightbcraft_iap_Billing_nativePrice(JNIEnv *env, jclass cls, jint slot, jstring text,
                                                                     jstring ascii)
{
    (void)cls;
    int k = slot + 1;
    if (k < 0 || k > TIPS_MAX) return;
    const char *t = text ? (*env)->GetStringUTFChars(env, text, NULL) : NULL;
    const char *a = ascii ? (*env)->GetStringUTFChars(env, ascii, NULL) : NULL;
    pthread_mutex_lock(&lock);
    snprintf(st.price[k], sizeof st.price[k], "%s", t ? t : "");
    snprintf(st.ascii[k], sizeof st.ascii[k], "%s", a ? a : "");
    st.changes++;
    pthread_mutex_unlock(&lock);
    if (t) (*env)->ReleaseStringUTFChars(env, text, t);
    if (a) (*env)->ReleaseStringUTFChars(env, ascii, a);
}

JNIEXPORT void JNICALL Java_com_eightbcraft_iap_Billing_nativeBusy(JNIEnv *env, jclass cls, jint busy)
{
    (void)env;
    (void)cls;
    pthread_mutex_lock(&lock);
    st.busy = busy != 0;
    st.changes++;
    pthread_mutex_unlock(&lock);
}

JNIEXPORT void JNICALL Java_com_eightbcraft_iap_Billing_nativeUnlocked(JNIEnv *env, jclass cls, jint owned)
{
    (void)env;
    (void)cls;
    pthread_mutex_lock(&lock);
    if (st.unlock_id[0] && st.unlocked != (owned != 0)) {
        st.unlocked = owned != 0;
        st.changes++;
    }
    pthread_mutex_unlock(&lock);
}

JNIEXPORT void JNICALL Java_com_eightbcraft_iap_Billing_nativeTipDone(JNIEnv *env, jclass cls)
{
    (void)env;
    (void)cls;
    pthread_mutex_lock(&lock);
    st.tips_done++;
    st.changes++;
    pthread_mutex_unlock(&lock);
}
