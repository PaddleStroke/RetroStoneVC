/*
 * The app's pages (pages.h): the ribbon, the Games page's header and search, the Settings and Stats pages, the theme.
 * Everything is laid out from the screen's short side (s): the same proportions on a phone held either way, a tablet,
 * a window. The texts: help/<lang>/_ui.txt (a title line for the translators, then a line a string, in the order of
 * the U_* below), help/en/_langs.txt (each language's own name), _privacy.txt, _terms.txt, _licences.txt.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "pages.h"
#include "edition.h"
#include "iap.h"
#include "stats.h"
#include "text.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef __ANDROID__
#include <jni.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int page = PAGE_GAMES;
theme_t TH;

/* the strings, help/<lang>/_ui.txt line by line */
enum {
    U_GAMES, U_STATS, U_SETTINGS, U_SEARCH, U_NONE,
    U_SUPPORT_US, U_REVIEW, U_TIP, U_SOURCE, U_REPORT,
    U_APPEARANCE, U_LIGHT, U_DARK, U_SYSTEM,
    U_LANGUAGE, U_LANG_SYSTEM,
    U_GAMEPLAY, U_AWAKE, U_SOUND,
    U_LEGAL, U_PRIVACY, U_TERMS, U_LICENCES, U_VERSION,
    U_WEEK, U_STREAK, U_TODAY, U_CHEER0, U_CHEER1, U_CHEER2, U_CHEER3, U_CHEER4,
    U_LEVEL, U_TO_LEVEL, U_XP, U_FINISHED, U_PROFILE, U_ACHIEVEMENTS, U_LOCKED,
    U_DAYS, U_WEEK_START,
    U_TIP_TITLE, U_TIP_LINE, U_TIP1, U_TIP2, U_TIP3, U_THANKS,
    U_ACH                                       /* then each achievement: its name, what it asks */
};
enum { ACH_FIRST, ACH_XP1, ACH_XP2, ACH_XP3, ACH_STREAK1, ACH_STREAK2, ACH_STREAK3, ACH_HARD, ACH_STARS, ACH_EXPLORER,
       ACH_HUNDRED, ACHS };
static const int ACH_ICON[ACHS] = {ICON_TICK, ICON_BOLT, ICON_MEDAL, ICON_CUP, ICON_FLAME, ICON_FLAME, ICON_FLAME,
                                   ICON_BOT, ICON_STAR, ICON_TILES, ICON_PAR};
#define U_HOWTO (U_ACH + 2 * ACHS)              /* the line after the achievements: How to play */
/* then online play, not there yet (docs/multiplayer-ready.md): the leaderboards' header and tabs, the note, the
 * Settings row and its badge */
enum { U_BOARDS = U_HOWTO + 1, U_FRIENDS, U_WORLD, U_SOON, U_ONLINE, U_LATER };

static const char *ui_str(int i, char *buf, int size)
{
    if (!text_word("_ui", i, buf, size)) buf[0] = 0;
    return buf;
}

/* a string with its %d and %s filled (in order: the numbers a, b, the words w); %% a % */
static const char *fmt(char *out, int size, const char *f, int a, int b, const char *w)
{
    int n = 0, k = 0;
    for (; *f && n < size - 1; f++) {
        if (*f != '%' || !f[1]) { out[n++] = *f; continue; }
        char tmp[32];
        f++;
        if (*f == 'd') snprintf(tmp, sizeof tmp, "%d", k++ ? b : a);
        else if (*f == 's') snprintf(tmp, sizeof tmp, "%s", w ? w : "");
        else snprintf(tmp, sizeof tmp, "%c", *f);
        for (const char *t = tmp; *t && n < size - 1; t++) out[n++] = *t;
    }
    out[n] = 0;
    return out;
}

/* ---- the theme ---------------------------------------------------------------------------------------------------- */
static int theme_mode;                          /* 0 as the device, 1 light, 2 dark ("shell/theme") */
static int awake = 1;                           /* the screen kept on ("shell/awake"; SDL's own default) */
static int shot_mode;                           /* the picture mode: the device's theme is dark, no URL opened */

static const theme_t DARK = {1, {19, 27, 24}, {30, 41, 37}, {45, 59, 53}, {244, 237, 220}, {148, 157, 147},
                             {228, 186, 90}, {204, 160, 64}, {33, 27, 16}, {24, 34, 30}};
static const theme_t LIGHT = {0, {246, 240, 226}, {255, 252, 244}, {229, 220, 199}, {46, 40, 30}, {126, 116, 98},
                              {164, 118, 28}, {212, 166, 68}, {40, 30, 14}, {253, 249, 240}};
static const uint8_t CREAM[3] = {250, 244, 228};
/* the sections' colours, by number (shell.c SEC_*: the house's jade, amber, lapis, red lacquer, plum, teal; the
 * Arcade's gold; Logic's lavender) */
static const uint8_t SEC_C[STATS_SECTIONS][3] = {{86, 176, 138}, {214, 160, 64}, {98, 140, 214}, {212, 92, 72},
                                                 {184, 112, 168}, {84, 178, 192}, {222, 182, 84}, {156, 132, 214}};

/* the device's own theme: 1 dark (unknown: dark, the house's own) */
static int system_dark(void)
{
    if (shot_mode) return 1;
#if defined(__ANDROID__)
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject act = (jobject)SDL_AndroidGetActivity();
    int dark = 1;
    if (env && act) {
        jclass ac = (*env)->GetObjectClass(env, act);
        jmethodID gr = (*env)->GetMethodID(env, ac, "getResources", "()Landroid/content/res/Resources;");
        jobject res = gr ? (*env)->CallObjectMethod(env, act, gr) : NULL;
        if (res) {
            jclass rc = (*env)->GetObjectClass(env, res);
            jmethodID gc = (*env)->GetMethodID(env, rc, "getConfiguration", "()Landroid/content/res/Configuration;");
            jobject conf = gc ? (*env)->CallObjectMethod(env, res, gc) : NULL;
            if (conf) {
                jclass cc = (*env)->GetObjectClass(env, conf);
                jfieldID f = (*env)->GetFieldID(env, cc, "uiMode", "I");
                if (f) dark = ((*env)->GetIntField(env, conf, f) & 0x30) != 0x10;   /* UI_MODE_NIGHT_NO: light */
                (*env)->DeleteLocalRef(env, cc);
                (*env)->DeleteLocalRef(env, conf);
            }
            (*env)->DeleteLocalRef(env, rc);
            (*env)->DeleteLocalRef(env, res);
        }
        (*env)->DeleteLocalRef(env, ac);
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    }
    if (act) (*env)->DeleteLocalRef(env, act);
    return dark;
#elif defined(_WIN32)
    DWORD v = 0, n = sizeof v;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &v, &n) == ERROR_SUCCESS)
        return v == 0;
    return 1;
#else
    return 1;                                   /* TODO(ios): UITraitCollection's userInterfaceStyle (apps/ios) */
#endif
}

static void cache_clear(void);

static void theme_update(void)
{
    int dark = theme_mode == 2 || (theme_mode == 0 && system_dark());
    if (TH.bg[0] && TH.dark == dark) return;
    TH = dark ? DARK : LIGHT;
    cache_clear();
    shell_theme_changed();
}

void pages_set_theme(int mode)
{
    theme_mode = mode < 0 || mode > 2 ? 0 : mode;
    uint8_t b = (uint8_t)theme_mode;
    ui_save_set("shell/theme", &b, 1);
    theme_update();
}

void pages_foreground(void) { theme_update(); }

static void awake_apply(void)
{
    if (awake) SDL_DisableScreenSaver();
    else SDL_EnableScreenSaver();
}

const char *pages_lang_kept(void)
{
    static char code[16];
    int n = ui_save_get("shell/lang", code, sizeof code - 1);
    if (n <= 0) return NULL;
    code[n] = 0;
    return code;
}

void pages_init(void)
{
    uint8_t b = 0;
    shot_mode = save_path() == NULL;            /* (the picture mode keeps nothing: no file) */
    if (ui_save_get("shell/theme", &b, 1) == 1 && b <= 2) theme_mode = b;
    if (ui_save_get("shell/awake", &b, 1) == 1) awake = b != 0;
    awake_apply();
    stats_load();
    theme_update();
    static const char *tips[3];                 /* the tip products (the .conf's TIP_PRODUCTS) */
    static char ids[256];
    snprintf(ids, sizeof ids, "%s", ed_tip_products);
    int n = 0;
    for (char *t = strtok(ids, " "); t && n < 3; t = strtok(NULL, " ")) tips[n++] = t;
    iap_tips_init(tips, n);
}

/* ---- pictures kept: words, rounded shapes, icons ----------------------------------------------------------------- */
#define TXC 192
static struct {
    char s[160];
    int px, style, maxw, w, h;
    uint8_t c[3];
    SDL_Texture *t;
    Uint32 used;
} txc[TXC];
#define RRC 96
static struct { int w, h, r; uint8_t c[4]; SDL_Texture *t; Uint32 used; } rrc[RRC];
#define ICC 64
static struct { int ic, d; SDL_Texture *t; Uint32 used; } icc[ICC];
static Uint32 clk;
static double rot_deg;                          /* ico() turned (ico_rot) */

static void cache_clear(void)
{
    for (int i = 0; i < TXC; i++) ui_free(&txc[i].t);
    for (int i = 0; i < ICC; i++) ui_free(&icc[i].t);
    for (int i = 0; i < RRC; i++) ui_free(&rrc[i].t);
}

enum { T_REG, T_BOLD, T_TITLE };

static int text_w(const char *s, float px, int style, const uint8_t c[3])
{
    return style == T_TITLE ? text_title(s, px, c, NULL, 0, 0, 0, 0) : text_line(s, px, style, c, NULL, 0, 0, 0, 0);
}

/* words as a picture, px pixels, no wider than maxw (made smaller to fit); *w x *h its size (the ink centred in it) */
static SDL_Texture *tx(const char *s, float px, int style, const uint8_t c[3], float maxw, int *w, int *h)
{
    if (!s || !*s || px < 4) return NULL;
    int ipx = (int)px, imw = (int)maxw, old = 0;
    for (int i = 0; i < TXC; i++)
        if (txc[i].t && txc[i].px == ipx && txc[i].style == style && txc[i].maxw == imw && !memcmp(txc[i].c, c, 3) &&
            !strncmp(txc[i].s, s, sizeof txc[i].s - 1)) {
            txc[i].used = ++clk;
            *w = txc[i].w;
            *h = txc[i].h;
            return txc[i].t;
        } else if (txc[i].used < txc[old].used) old = i;
    int tw = text_w(s, px, style, c);
    if (maxw > 0 && tw > maxw && tw > 0) {
        px = fmaxf(6, px * maxw / tw);
        tw = text_w(s, px, style, c);
    }
    int W = tw + 4, H = (int)ceilf(px * 1.55f);
    uint8_t *b = calloc((size_t)W * H, 4);
    if (!b) return NULL;
    int y = (int)((H - 1.25f * px) / 2);
    if (style == T_TITLE) text_title(s, px, c, b, W, H, 2, y);
    else text_line(s, px, style, c, b, W, H, 2, y);
    ui_free(&txc[old].t);
    txc[old].t = ui_tex(W, H, b);
    free(b);
    snprintf(txc[old].s, sizeof txc[old].s, "%s", s);
    txc[old].px = ipx;
    txc[old].style = style;
    txc[old].maxw = imw;
    memcpy(txc[old].c, c, 3);
    txc[old].w = *w = W;
    txc[old].h = *h = H;
    txc[old].used = ++clk;
    return txc[old].t;
}

enum { AL_LEFT, AL_CENTRE, AL_RIGHT };
/* words drawn on one line, centred on cy, from x (or centred on it, or ending at it); returns their width */
static float put(const char *s, float px, int style, const uint8_t c[3], float maxw, float x, float cy, int align)
{
    int w, h;
    SDL_Texture *t = tx(s, px, style, c, maxw, &w, &h);
    if (!t) return 0;
    float x0 = align == AL_LEFT ? x - 2 : align == AL_CENTRE ? x - w / 2.0f : x - w + 2;
    ui_put(t, floorf(x0), floorf(cy - h / 2.0f), (float)w, (float)h);
    return (float)w - 4;
}

static void rr(float x, float y, float w, float h, float rad, const uint8_t c[3], Uint8 a)
{
    int iw = (int)w, ih = (int)h, ir = (int)rad, old = 0;
    if (iw < 1 || ih < 1) return;
    uint8_t key[4] = {c[0], c[1], c[2], a};
    SDL_Texture *t = NULL;
    for (int i = 0; i < RRC && !t; i++)
        if (rrc[i].t && rrc[i].w == iw && rrc[i].h == ih && rrc[i].r == ir && !memcmp(rrc[i].c, key, 4)) {
            rrc[i].used = ++clk;
            t = rrc[i].t;
        } else if (rrc[i].used < rrc[old].used) old = i;
    if (!t) {
        art_roundrect(iw, ih, fminf(rad, fminf(iw, ih) / 2.0f), key, ui_buf(iw, ih));
        ui_free(&rrc[old].t);
        t = rrc[old].t = ui_tex(iw, ih, ui_buf(iw, ih));
        rrc[old].w = iw;
        rrc[old].h = ih;
        rrc[old].r = ir;
        memcpy(rrc[old].c, key, 4);
        rrc[old].used = ++clk;
    }
    ui_put(t, floorf(x), floorf(y), (float)iw, (float)ih);
}

static void disc(float cx, float cy, float d, const uint8_t c[3], Uint8 a) { rr(cx - d / 2, cy - d / 2, d, d, d / 2, c, a); }

static void ico_rot(int ic, float cx, float cy, float d, const uint8_t c[3], double deg);
static void ico(int ic, float cx, float cy, float d, const uint8_t c[3], Uint8 a)
{
    int id = (int)d, old = 0;
    if (id < 2) return;
    SDL_Texture *t = NULL;
    for (int i = 0; i < ICC && !t; i++)
        if (icc[i].t && icc[i].ic == ic && icc[i].d == id) {
            icc[i].used = ++clk;
            t = icc[i].t;
        } else if (icc[i].used < icc[old].used) old = i;
    if (!t) {
        uint8_t *b = calloc((size_t)id * id, 4);
        if (!b) return;
        if (ic >= 1000) shell_tab_icon(ic - 1000, id, b);   /* a section's tab picture, in its own colours */
        else art_icon(ic, id, b);
        ui_free(&icc[old].t);
        t = icc[old].t = ui_tex(id, id, b);
        free(b);
        icc[old].ic = ic;
        icc[old].d = id;
        icc[old].used = ++clk;
    }
    if (!t) return;
    if (c) SDL_SetTextureColorMod(t, c[0], c[1], c[2]);
    SDL_SetTextureAlphaMod(t, a);
    if (rot_deg != 0) ui_put_rot(t, cx, cy, (float)id, (float)id, rot_deg);
    else ui_put(t, floorf(cx - id / 2.0f), floorf(cy - id / 2.0f), (float)id, (float)id);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

static void ico_rot(int ic, float cx, float cy, float d, const uint8_t c[3], double deg)
{
    rot_deg = deg;
    ico(ic, cx, cy, d, c, 255);
    rot_deg = 0;
}

/* ---- the layout: the ribbon, the room, the scroll ----------------------------------------------------------------- */
static box NAV, R;
static int rail;                                /* a wide screen: the ribbon is a rail at the left */
static float S;                                 /* the screen's short side */
static float HEAD;                              /* a page's title row */
static float scroll[PAGES], smax[PAGES], vel;
static struct { int on, moved; float x, y, s; Uint32 t; } grab;

/* the Games page's search */
static struct { int open; char q[96], fold[192]; box field, clear; } SR;
/* the Settings page: the languages' list open; the tip panel */
static int langs_open;
static struct { int open, demo; unsigned seen, thanks; box card, btn[3], close; } TIP;

static void progress_read(void);

box pages_room(void) { return R; }

static float content_w(void) { return floorf(fminf(R.w - 0.1f * S, 1.25f * S)); }
static float content_x(void) { return floorf(R.x + (R.w - content_w()) / 2); }

float pages_games_top(void) { return R.y + HEAD + (SR.open ? floorf(0.14f * S) : 0); }

void pages_layout(void)
{
    S = fminf(ui.w, ui.h);
    rail = ui.w > ui.h;
    if (rail) {
        float nw = floorf(0.2f * S);
        NAV = (box){0, 0, nw, (float)ui.h};
        R = (box){nw, 0, ui.w - nw, (float)ui.h};
    } else {
        float nh = floorf(0.16f * S);
        NAV = (box){0, ui.h - nh, (float)ui.w, nh};
        R = (box){0, 0, (float)ui.w, ui.h - nh};
    }
    HEAD = floorf(0.15f * S);
    float m = floorf(0.05f * S), fh = floorf(0.11f * S);
    SR.field = (box){R.x + m, R.y + HEAD - floorf(0.01f * S), R.w - 2 * m, fh};
    SR.clear = (box){SR.field.x + SR.field.w - fh, SR.field.y, fh, fh};
}

static int in(box b, float x, float y) { return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h; }

void pages_show(int p)
{
    if (p < 0 || p >= PAGES) return;
    if (p != PAGE_GAMES && SR.open) SDL_StopTextInput();
    page = p;
    vel = 0;
    grab.on = 0;
    TIP.open = 0;
    if (p != PAGE_GAMES) scroll[p] = 0;
    if (p == PAGE_STATS) progress_read();
    shell_page_changed();
}

void pages_scroll(float px)
{
    if (page == PAGE_GAMES) return;
    scroll[page] = fmaxf(0, px);
}

/* ---- the ribbon ----------------------------------------------------------------------------------------------------- */
static const int NAV_ICON[PAGES] = {ICON_STATS, ICON_TILES, ICON_GEAR}, NAV_STR[PAGES] = {U_STATS, U_GAMES, U_SETTINGS};

static box nav_item(int p)
{
    if (rail) {
        float ih = floorf(0.26f * S), y0 = floorf((NAV.h - PAGES * ih) / 2);
        return (box){NAV.x, NAV.y + y0 + p * ih, NAV.w, ih};
    }
    float iw = floorf(NAV.w / PAGES);
    return (box){NAV.x + p * iw, NAV.y, iw, NAV.h};
}

static void draw_nav(void)
{
    ui_fill(NAV.x, NAV.y, NAV.w, NAV.h, TH.nav[0], TH.nav[1], TH.nav[2], 255);
    if (rail) ui_fill(NAV.x + NAV.w - 1, NAV.y, 1, NAV.h, TH.line[0], TH.line[1], TH.line[2], 255);
    else ui_fill(NAV.x, NAV.y, NAV.w, 1, TH.line[0], TH.line[1], TH.line[2], 255);
    char b[96];
    for (int p = 0; p < PAGES; p++) {
        box it = nav_item(p);
        float u = rail ? it.h : NAV.h, cx = it.x + it.w / 2, cy = it.y + u * 0.4f, d = floorf(0.34f * u);
        const uint8_t *c = p == page ? TH.accent : TH.dim;
        if (p == page) rr(cx - 0.8f * d, cy - 0.62f * d, 1.6f * d, 1.24f * d, 0.62f * d, TH.gold, TH.dark ? 46 : 60);
        ico(NAV_ICON[p], cx, cy, d, c, 255);
        put(ui_str(NAV_STR[p], b, sizeof b), floorf(0.03f * S), T_TITLE, c, it.w * 0.92f, cx, it.y + u * 0.78f,
            AL_CENTRE);
    }
}

/* a page's title, at the top of its room (scrolled with it) */
static void draw_title(int str, float dy)
{
    char b[96];
    put(ui_str(str, b, sizeof b), floorf(0.072f * S), T_TITLE, TH.text, R.w * 0.6f, R.x + R.w / 2,
        R.y + HEAD * 0.52f - dy, AL_CENTRE);
}

/* ---- the Games page's header: the streak, the title, the search ------------------------------------------------ */
static box chip_box(void)
{
    float h = floorf(0.085f * S), m = floorf(0.05f * S);
    return (box){R.x + m, R.y + floorf(HEAD * 0.52f - h / 2), floorf(1.7f * h), h};
}

static void search_btn(float *cx, float *cy, float *r)
{
    *r = floorf(0.05f * S);
    *cx = R.x + R.w - floorf(0.05f * S) - *r;
    *cy = R.y + HEAD * 0.52f;
}

static void draw_games_header(void)
{
    stats_t st;
    stats_get(&st);
    draw_title(U_GAMES, 0);
    box c = chip_box();                         /* the streak: a flame and the days */
    rr(c.x, c.y, c.w, c.h, c.h / 2, TH.card, 255);
    ico(ICON_FLAME, c.x + c.h * 0.55f, c.y + c.h / 2, c.h * 0.6f, st.streak ? TH.accent : TH.dim, 255);
    char n[16];
    snprintf(n, sizeof n, "%d", st.streak);
    put(n, floorf(c.h * 0.5f), T_TITLE, st.streak ? TH.accent : TH.dim, c.w - c.h, c.x + c.h * 1.05f, c.y + c.h / 2,
        AL_LEFT);
    float sx, sy, sr;
    search_btn(&sx, &sy, &sr);
    ico(SR.open ? ICON_CLOSE : ICON_SEARCH, sx, sy, 1.3f * sr, TH.text, 255);
    if (!SR.open) return;
    box f = SR.field;                           /* the field: the glass, the words (or what to type), clear */
    char b[96];
    rr(f.x, f.y, f.w, f.h, f.h * 0.3f, TH.card, 255);
    ico(ICON_SEARCH, f.x + f.h * 0.5f, f.y + f.h / 2, f.h * 0.42f, TH.dim, 255);
    float tx0 = f.x + f.h * 0.95f, maxw = f.w - 2.1f * f.h, px = floorf(f.h * 0.36f);
    float w = SR.q[0] ? put(SR.q, px, T_REG, TH.text, maxw, tx0, f.y + f.h / 2, AL_LEFT)
                      : put(ui_str(U_SEARCH, b, sizeof b), px, T_REG, TH.dim, maxw, tx0, f.y + f.h / 2, AL_LEFT);
    if (SDL_IsTextInputActive() || shot_mode) {  /* the caret */
        float cx = SR.q[0] ? tx0 + w + 3 : tx0;
        ui_fill(cx, f.y + f.h * 0.27f, fmaxf(2, S * 0.003f), f.h * 0.46f, TH.accent[0], TH.accent[1], TH.accent[2],
                255);
    }
    if (SR.q[0]) ico(ICON_CLOSE, SR.clear.x + SR.clear.w / 2, SR.clear.y + SR.clear.h / 2, f.h * 0.42f, TH.dim, 255);
}

const char *pages_search(void) { return SR.open && SR.fold[0] ? SR.fold : NULL; }

static void search_changed(void)
{
    text_fold(SR.q, SR.fold, sizeof SR.fold);
    char *f = SR.fold;                          /* trimmed (a phone keyboard's space after a word) */
    size_t n = strlen(f), a = strspn(f, " ");
    while (n > a && f[n - 1] == ' ') n--;
    memmove(f, f + a, n - a);
    f[n - a] = 0;
    shell_page_changed();
}

static void search_open(int on)
{
    if (on == SR.open) return;
    SR.open = on;
    SR.q[0] = SR.fold[0] = 0;
    if (on) {
        SDL_Rect r = {(int)SR.field.x, (int)SR.field.y, (int)SR.field.w, (int)SR.field.h};
        SDL_SetTextInputRect(&r);
        SDL_StartTextInput();                   /* (Android, iOS: the keyboard comes up) */
    } else {
        SDL_StopTextInput();
    }
    shell_page_changed();
}

void pages_search_set(const char *s)
{
    page = PAGE_GAMES;
    search_open(1);
    snprintf(SR.q, sizeof SR.q, "%s", s);
    search_changed();
}

void pages_text(const char *s)
{
    if (!SR.open || page != PAGE_GAMES) return;
    size_t n = strlen(SR.q);
    if (n + strlen(s) >= sizeof SR.q) return;
    memcpy(SR.q + n, s, strlen(s) + 1);
    search_changed();
}

/* ---- the Settings page --------------------------------------------------------------------------------------------- */
enum { A_NONE, A_REVIEW, A_TIP, A_SOURCE, A_REPORT, A_THEME, A_LANGS, A_LANG, A_AWAKE, A_SOUND, A_PRIVACY, A_TERMS,
       A_LICENCES, A_HOWTO, A_ONLINE };
typedef struct { float y; int draw; float hx, hy; int act, val; } walk;   /* drawing, or finding a tap's row */

static int tips_sold(void) { return iap_tips_available() > 0 || TIP.demo; }
static int tips_row(void) { return tips_sold() || ed_support_url[0]; }

static void head(walk *k, int str)              /* a section's header: gold capitals */
{
    float h = floorf(0.115f * S);
    if (k->draw) {
        char b[96];
        put(ui_str(str, b, sizeof b), floorf(0.046f * S), T_TITLE, TH.accent, content_w(), content_x(),
            k->y + h * 0.62f, AL_LEFT);
    }
    k->y += h;
}

/* what is not there yet: "Coming later" in a quiet pill (h high) ending at rx, centred on cy; returns its width */
static float later_badge(float rx, float cy, float h, float maxw)
{
    char b[96];
    ui_str(U_LATER, b, sizeof b);
    float px = floorf(0.44f * h), bw = fminf(maxw, text_w(b, px, T_BOLD, TH.dim) + 1.1f * h);
    rr(rx - bw, cy - h / 2, bw, h, h / 2, TH.line, 255);
    put(b, px, T_BOLD, TH.dim, bw - 0.8f * h, rx - bw / 2, cy, AL_CENTRE);
    return bw;
}

enum { R_CHEVRON, R_RADIO, R_TOGGLE, R_DROP, R_OPTION, R_LATER };
/* a row: its icon (in its colour; NULL: the words' colour), its words, its control; on: a radio's choice, a switch's
 * state, the list open. R_LATER: what is not there yet, greyed, "Coming later" at its right (a tap: the note) */
static void row(walk *k, int icon, const uint8_t *ic, const char *label, int kind, int on, int act, int val)
{
    float x = content_x(), w = content_w(), h = floorf((kind == R_OPTION ? 0.105f : 0.13f) * S);
    float gap = floorf((kind == R_OPTION ? 0.01f : 0.022f) * S);
    if (!k->draw) {
        if (k->hy >= k->y && k->hy < k->y + h && k->hx >= x && k->hx < x + w) { k->act = act; k->val = val; }
        k->y += h + gap;
        return;
    }
    if (k->y > R.y + R.h || k->y + h < R.y) { k->y += h + gap; return; }
    int lit = on && (kind == R_RADIO || kind == R_OPTION);
    const uint8_t *fg = lit ? TH.on_gold : kind == R_LATER ? TH.dim : TH.text;
    float cy = k->y + h / 2, ix = x + (kind == R_OPTION ? 0.6f : 0.5f) * h;
    rr(x, k->y, w, h, floorf(0.24f * h), lit ? TH.gold : TH.card, 255);
    if (icon >= 0) ico(icon, ix, cy, floorf(0.42f * h), lit ? TH.on_gold : kind == R_LATER ? TH.dim : ic ? ic : TH.text,
                       255);
    float lx = x + (icon >= 0 ? 1.0f : 0.35f) * h, rx = x + w - 0.45f * h, bw = 0;
    if (kind == R_LATER) bw = later_badge(x + w - 0.3f * h, cy, floorf(0.4f * h), 0.45f * w) - 0.15f * h;
    put(label, floorf((kind == R_OPTION ? 0.33f : 0.3f) * h), T_BOLD, fg, rx - lx - 0.6f * h - bw, lx, cy, AL_LEFT);
    float d = floorf(0.34f * h);
    if (kind == R_CHEVRON) ico(ICON_CHEVRON, rx, cy, d, TH.dim, 255);
    else if (kind == R_DROP) ico_rot(ICON_CHEVRON, rx, cy, d, TH.dim, on ? -90 : 90);   /* the list's arrow */
    else if (kind == R_RADIO || kind == R_OPTION) {
        if (on) {
            disc(rx, cy, d, TH.on_gold, 255);
            ico(ICON_TICK, rx, cy, 0.7f * d, TH.gold, 255);
        } else {
            disc(rx, cy, d, TH.dim, 255);
            disc(rx, cy, d - 2 * fmaxf(2, 0.09f * d), TH.card, 255);
        }
    } else if (kind == R_TOGGLE) {              /* a switch: gold when on */
        float sh = floorf(0.34f * h), sw = floorf(1.8f * sh), sx = x + w - 0.3f * h - sw, kd = floorf(0.76f * sh);
        rr(sx, cy - sh / 2, sw, sh, sh / 2, on ? TH.gold : TH.line, 255);
        disc(on ? sx + sw - sh / 2 : sx + sh / 2, cy, kd, on ? CREAM : TH.dim, 255);
    }
    k->y += h + gap;
}

/* the languages: help/en/_langs.txt, "code name" a line; i: tx_langs' index (-1: as the device) */
static const char *lang_name(const char *code, char *buf, int size)
{
    const char *t = text_get("_langs");
    size_t n = strlen(code);
    for (; t && *t; t += strcspn(t, "\n") + (t[strcspn(t, "\n")] ? 1 : 0))
        if (!strncmp(t, code, n) && t[n] == ' ') {
            int len = (int)strcspn(t + n + 1, "\n");
            if (len >= size) len = size - 1;
            memcpy(buf, t + n + 1, (size_t)len);
            buf[len] = 0;
            return buf;
        }
    snprintf(buf, (size_t)size, "%s", code);
    return buf;
}

static const uint8_t IC_REVIEW[3] = {86, 176, 138}, IC_TIP[3] = {212, 92, 72}, IC_STAR[3] = {232, 188, 72},
                     IC_BUG[3] = {222, 120, 70};

static float settings_walk(walk *k)
{
    char b[160], b2[160];
    k->y = R.y + HEAD - scroll[PAGE_SETTINGS];
    if (k->draw) draw_title(U_SETTINGS, scroll[PAGE_SETTINGS]);
    head(k, U_SUPPORT_US);
    if (ed_review_url[0]) row(k, ICON_REVIEW, IC_REVIEW, ui_str(U_REVIEW, b, sizeof b), R_CHEVRON, 0, A_REVIEW, 0);
    if (tips_row()) row(k, ICON_HEART, IC_TIP, ui_str(U_TIP, b, sizeof b), R_CHEVRON, 0, A_TIP, 0);
    if (ed_source_url[0]) row(k, ICON_STAR, IC_STAR, ui_str(U_SOURCE, b, sizeof b), R_CHEVRON, 0, A_SOURCE, 0);
    if (ed_report_url[0]) row(k, ICON_BUG, IC_BUG, ui_str(U_REPORT, b, sizeof b), R_CHEVRON, 0, A_REPORT, 0);
    head(k, U_APPEARANCE);
    row(k, ICON_SUN, NULL, ui_str(U_LIGHT, b, sizeof b), R_RADIO, theme_mode == 1, A_THEME, 1);
    row(k, ICON_MOON, NULL, ui_str(U_DARK, b, sizeof b), R_RADIO, theme_mode == 2, A_THEME, 2);
    row(k, ICON_AUTO, NULL, ui_str(U_SYSTEM, b, sizeof b), R_RADIO, theme_mode == 0, A_THEME, 0);
    head(k, U_LANGUAGE);
    const char *kept = pages_lang_kept();
    row(k, ICON_GLOBE, NULL, kept ? lang_name(kept, b, sizeof b) : ui_str(U_LANG_SYSTEM, b, sizeof b), R_DROP,
        langs_open, A_LANGS, 0);
    if (langs_open) {
        row(k, -1, NULL, ui_str(U_LANG_SYSTEM, b2, sizeof b2), R_OPTION, !kept, A_LANG, -1);
        const char *t = text_get("_langs");     /* in the list's order */
        for (; t && *t; t += strcspn(t, "\n") + (t[strcspn(t, "\n")] ? 1 : 0)) {
            int n = (int)strcspn(t, " \n");
            if (n <= 0 || n >= 16 || t[n] != ' ') continue;
            char code[16];
            memcpy(code, t, (size_t)n);
            code[n] = 0;
            int li = -1;
            for (int i = 0; i < tx_nlangs; i++)
                if (!strcmp(tx_langs[i].code, code)) li = i;
            if (li < 0) continue;
            row(k, -1, NULL, lang_name(code, b2, sizeof b2), R_OPTION, kept && !strcmp(kept, code), A_LANG, li);
        }
    }
    head(k, U_GAMEPLAY);
    row(k, ICON_HELP, NULL, ui_str(U_HOWTO, b, sizeof b), R_CHEVRON, 0, A_HOWTO, 0);
    row(k, ICON_AWAKE, NULL, ui_str(U_AWAKE, b, sizeof b), R_TOGGLE, awake, A_AWAKE, 0);
    row(k, snd_muted ? ICON_MUTE : ICON_SOUND, NULL, ui_str(U_SOUND, b, sizeof b), R_TOGGLE, !snd_muted, A_SOUND, 0);
    row(k, ICON_GLOBE, NULL, ui_str(U_ONLINE, b, sizeof b), R_LATER, 0, A_ONLINE, 0);
    head(k, U_LEGAL);
    row(k, ICON_SHIELD, NULL, ui_str(U_PRIVACY, b, sizeof b), R_CHEVRON, 0, A_PRIVACY, 0);
    row(k, ICON_DOC, NULL, ui_str(U_TERMS, b, sizeof b), R_CHEVRON, 0, A_TERMS, 0);
    row(k, ICON_CODE, NULL, ui_str(U_LICENCES, b, sizeof b), R_CHEVRON, 0, A_LICENCES, 0);
    float vh = floorf(0.16f * S);                /* the app and its version */
    if (k->draw) {
        put(ed_app_name, floorf(0.04f * S), T_TITLE, TH.dim, content_w(), R.x + R.w / 2, k->y + vh * 0.38f, AL_CENTRE);
        fmt(b2, sizeof b2, ui_str(U_VERSION, b, sizeof b), 0, 0, ed_version[0] ? ed_version : "dev");
        put(b2, floorf(0.032f * S), T_REG, TH.dim, content_w(), R.x + R.w / 2, k->y + vh * 0.7f, AL_CENTRE);
    }
    k->y += vh;
    return k->y + scroll[PAGE_SETTINGS] - R.y;
}

static void set_lang(int li)
{
    const char *code = li >= 0 && li < tx_nlangs ? tx_langs[li].code : NULL;
    ui_save_set("shell/lang", code ? code : "", code ? (int)strlen(code) : 0);
    text_init(code);
    cache_clear();
    shell_text_changed();
}

static void tip_open(void);

static void settings_tap(float x, float y)
{
    walk k = {0, 0, x, y, A_NONE, 0};
    settings_walk(&k);
    if (k.act == A_NONE) return;
    snd_play(SND_TAP);
    switch (k.act) {
    case A_REVIEW: shell_open_url(ed_review_url); break;
    case A_SOURCE: shell_open_url(ed_source_url); break;
    case A_REPORT: shell_open_url(ed_report_url); break;
    case A_TIP:
        if (tips_sold()) tip_open();
        else shell_open_url(ed_support_url);
        break;
    case A_THEME: pages_set_theme(k.val); break;
    case A_LANGS: langs_open = !langs_open; break;
    case A_LANG:
        set_lang(k.val);
        langs_open = 0;
        break;
    case A_AWAKE: {
        awake = !awake;
        uint8_t b = (uint8_t)awake;
        ui_save_set("shell/awake", &b, 1);
        awake_apply();
        break;
    }
    case A_SOUND: shell_set_mute(!snd_muted); break;
    case A_PRIVACY: shell_open_text("_privacy"); break;
    case A_TERMS: shell_open_text("_terms"); break;
    case A_LICENCES: shell_open_text("_licences"); break;
    case A_HOWTO: shell_open_text("_app"); break;
    case A_ONLINE: pages_soon(); break;
    }
}

/* ---- the tip panel (tips sold in the app: three tiers) ------------------------------------------------------------- */
void pages_tips_demo(void) { TIP.demo = 1; }

static void tip_open(void)
{
    TIP.open = 1;
    TIP.seen = iap_tips_done();
}

static const char *tip_price(int i)
{
    static const char *demo[3] = {"0,99 \xe2\x82\xac", "2,99 \xe2\x82\xac", "4,99 \xe2\x82\xac"};
    return iap_tips_available() ? iap_tip_price(i) : demo[i];
}

static void tip_layout(void)
{
    float w = floorf(fminf(0.86f * S, ui.w - 0.1f * S)), bh = floorf(0.12f * S), g = floorf(0.03f * S);
    float h = floorf(0.78f * S);
    TIP.card = (box){floorf((ui.w - w) / 2), floorf((ui.h - h) / 2), w, h};
    float y = TIP.card.y + floorf(0.27f * S);
    for (int i = 0; i < 3; i++) TIP.btn[i] = (box){TIP.card.x + g * 1.5f, y + i * (bh + g), w - 3 * g, bh};
    float r = floorf(0.05f * S);
    TIP.close = (box){TIP.card.x + w - 2.4f * r, TIP.card.y + 0.4f * r, 2 * r, 2 * r};
}

static void draw_tip(void)
{
    char b[160], b2[160];
    tip_layout();
    if (iap_tips_done() != TIP.seen) {          /* a tip went through: thanks */
        TIP.seen = iap_tips_done();
        TIP.thanks = 1;
    }
    ui_fill(0, 0, (float)ui.w, (float)ui.h, 0, 0, 0, 130);
    box c = TIP.card;
    rr(c.x, c.y, c.w, c.h, floorf(0.04f * S), TH.bg, 255);
    float cx = c.x + c.w / 2;
    ico(ICON_HEART, cx, c.y + 0.07f * S, 0.08f * S, IC_TIP, 255);
    put(ui_str(U_TIP_TITLE, b, sizeof b), floorf(0.055f * S), T_TITLE, TH.text, c.w * 0.8f, cx, c.y + 0.15f * S,
        AL_CENTRE);
    put(ui_str(TIP.thanks ? U_THANKS : U_TIP_LINE, b, sizeof b), floorf(0.032f * S), T_REG, TH.dim, c.w * 0.9f, cx,
        c.y + 0.215f * S, AL_CENTRE);
    for (int i = 0; i < 3; i++) {
        box t = TIP.btn[i];
        rr(t.x, t.y, t.w, t.h, t.h * 0.3f, i == 1 ? TH.gold : TH.card, 255);
        const uint8_t *fg = i == 1 ? TH.on_gold : TH.text;
        put(ui_str(U_TIP1 + i, b, sizeof b), floorf(0.36f * t.h), T_BOLD, fg, t.w * 0.6f, t.x + 0.3f * t.h,
            t.y + t.h / 2, AL_LEFT);
        snprintf(b2, sizeof b2, "%s", tip_price(i));
        put(b2[0] ? b2 : "\xe2\x80\xa6", floorf(0.36f * t.h), T_BOLD, fg, t.w * 0.35f, t.x + t.w - 0.3f * t.h,
            t.y + t.h / 2, AL_RIGHT);
    }
    if (iap_busy()) ui_animate();
    ico(ICON_CLOSE, TIP.close.x + TIP.close.w / 2, TIP.close.y + TIP.close.h / 2, TIP.close.w * 0.6f, TH.dim, 255);
}

static void tip_press(float x, float y)
{
    tip_layout();
    for (int i = 0; i < 3; i++)
        if (in(TIP.btn[i], x, y)) {
            int ok = !iap_busy() && iap_tips_available() && iap_tip_buy(i);
            snd_play(ok ? SND_TAP : SND_DENY);
            if (!iap_tips_available()) fprintf(stderr, "tip %d (no store)\n", i);
            return;
        }
    if (in(TIP.close, x, y) || !in(TIP.card, x, y)) {
        snd_play(SND_TAP);
        TIP.open = TIP.thanks = 0;
    }
}

/* ---- the support links elsewhere (a solved puzzle's card): as the Settings page's rows -------------------------- */
int pages_link(int i, int *icon, const uint8_t **colour, int *word)
{
    static const uint8_t IC_REVIEW_L[3] = {232, 188, 72};   /* the review's star: gold */
    switch (i) {
    case PAGES_LINK_REVIEW:
        *icon = ICON_STAR, *colour = IC_REVIEW_L, *word = U_REVIEW;
        return ed_review_url[0] != 0;
    case PAGES_LINK_REPORT:
        *icon = ICON_BUG, *colour = IC_BUG, *word = U_REPORT;
        return ed_report_url[0] != 0;
    case PAGES_LINK_SUPPORT:
        *icon = ICON_HEART, *colour = IC_TIP, *word = U_SUPPORT_US;
        return tips_row();
    }
    return 0;
}

void pages_link_open(int i)
{
    if (i == PAGES_LINK_REVIEW) shell_open_url(ed_review_url);
    else if (i == PAGES_LINK_REPORT) shell_open_url(ed_report_url);
    else if (i == PAGES_LINK_SUPPORT) {
        if (tips_sold()) {
            S = fminf(ui.w, ui.h);              /* (the panel's size, off the menu too) */
            tip_open();
        } else {
            shell_open_url(ed_support_url);
        }
    }
}

int pages_panel_draw(void)
{
    if (!TIP.open) return 0;
    S = fminf(ui.w, ui.h);
    draw_tip();
    return 1;
}

int pages_panel_press(float x, float y)
{
    if (!TIP.open) return 0;
    S = fminf(ui.w, ui.h);
    tip_press(x, y);
    return 1;
}

int pages_panel_key(SDL_Keycode k)
{
    if (!TIP.open) return 0;
    if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) TIP.open = TIP.thanks = 0;
    return 1;
}

/* ---- the note "online play is coming later" ----------------------------------------------------------------------- */
#define SOON_MS 2200                            /* how long it shows (its last 300 ms fading) */
static int soon_on;
static Uint32 soon_at;

void pages_soon(void)
{
    soon_on = 1;
    soon_at = ui_ms();
}

void pages_soon_draw(int on_menu)
{
    if (!soon_on) return;
    Uint32 t = ui_ms() - soon_at;
    if (t > SOON_MS) { soon_on = 0; return; }
    ui_animate();
    S = fminf(ui.w, ui.h);
    Uint8 a = (Uint8)(t > SOON_MS - 300 ? 255 * ((SOON_MS - t) * 8 / 300 + 1) / 9 : 255);   /* (in steps: rr's pictures
                                                                                       * are kept by their alpha) */
    char b[160];
    ui_str(U_SOON, b, sizeof b);
    float h = floorf(0.1f * S), px = floorf(0.034f * S), d = floorf(0.046f * S);
    float cx = on_menu ? R.x + R.w / 2 : ui.w / 2.0f, by = on_menu && !rail ? NAV.y : (float)ui.h;
    int tw, th;
    SDL_Texture *tt = tx(b, px, T_BOLD, TH.bg, 0.86f * ui.w - 1.3f * h, &tw, &th);
    float w = floorf(tw + d + 1.2f * h), y = floorf(by - 0.06f * S - h), x = floorf(cx - w / 2);
    rr(x, y, w, h, h / 2, TH.text, (Uint8)(a * 0.94f));
    ico(ICON_GLOBE, x + 0.5f * h + d / 2, y + h / 2, d, TH.bg, a);
    if (tt) {
        SDL_SetTextureAlphaMod(tt, a);
        ui_put(tt, floorf(x + 0.5f * h + d + 0.25f * h - 2), floorf(y + (h - th) / 2), (float)tw, (float)th);
        SDL_SetTextureAlphaMod(tt, 255);
    }
}

/* ---- the Stats page ----------------------------------------------------------------------------------------------- */
static int prog_n, prog_sec[STATS_SECTIONS], prog_done[STATS_SECTIONS], prog_total[STATS_SECTIONS], three;

static void progress_read(void)
{
    int secs[8];
    prog_n = shell_sections(secs);
    for (int i = 0; i < prog_n && i < STATS_SECTIONS; i++) {
        prog_sec[i] = secs[i];
        shell_progress(secs[i], &prog_done[i], &prog_total[i]);
    }
    three = shell_three_stars();
}

static int ach_done(int a, const stats_t *st)
{
    switch (a) {
    case ACH_FIRST: return st->finished >= 1;
    case ACH_XP1: return st->xp >= 500;
    case ACH_XP2: return st->xp >= 2500;
    case ACH_XP3: return st->xp >= 10000;
    case ACH_STREAK1: return st->best >= 3;
    case ACH_STREAK2: return st->best >= 7;
    case ACH_STREAK3: return st->best >= 30;
    case ACH_HARD: return (st->flags & STATS_BEAT_HARD) != 0;
    case ACH_STARS: return three >= 50;
    case ACH_EXPLORER: {
        int all = prog_n > 0;
        for (int i = 0; i < prog_n; i++) all &= prog_done[i] > 0;
        return all;
    }
    case ACH_HUNDRED: return st->finished >= 100;
    }
    return 0;
}

static const uint8_t FELT[2][3] = {{40, 88, 70}, {46, 100, 78}};   /* the level card: the house's green felt */

/* the leaderboards as online play will have them, greyed: the tabs (friends, the world, this week), a few rows with
 * no one's name, and the note over them. Nothing is online yet: a tap on the card only says so (BOARD: its box). */
static box BOARD;

static float boards_walk(int draw, float x, float y, float w, float hh, float pad, float rad)
{
    char b[160];
    if (draw) {
        put(ui_str(U_BOARDS, b, sizeof b), floorf(0.046f * S), T_TITLE, TH.accent, w * 0.55f, x, y + hh * 0.62f,
            AL_LEFT);
        later_badge(x + w, y + hh * 0.62f, floorf(0.055f * S), w * 0.4f);
    }
    y += hh;
    float tabh = floorf(0.085f * S), rh = floorf(0.1f * S), g = floorf(0.025f * S);
    int nrows = 3;
    float bh = pad + tabh + g + nrows * rh + pad * 0.6f;
    BOARD = (box){x, y, w, bh};
    if (draw && y < R.y + R.h && y + bh > R.y) {
        rr(x, y, w, bh, rad, TH.card, 255);
        float tx0 = x + pad, tw = w - 2 * pad, ty = y + pad, sw = floorf(tw / 3);   /* the tabs: the first one chosen */
        rr(tx0, ty, tw, tabh, tabh / 2, TH.line, 255);
        rr(tx0 + 3, ty + 3, sw - 6, tabh - 6, (tabh - 6) / 2, TH.bg, 255);
        static const int TAB[3] = {U_FRIENDS, U_WORLD, U_WEEK};
        for (int i = 0; i < 3; i++)
            put(ui_str(TAB[i], b, sizeof b), floorf(0.033f * S), T_BOLD, TH.dim, sw * 0.86f, tx0 + (i + 0.5f) * sw,
                ty + tabh / 2, AL_CENTRE);
        static const float NAME_W[3] = {0.42f, 0.3f, 0.36f}, SCORE_W[3] = {0.14f, 0.12f, 0.1f};
        float ry0 = ty + tabh + g;
        for (int i = 0; i < nrows; i++) {        /* a row: the rank, a token, the name and the score left blank */
            float ry = ry0 + i * rh, cy = ry + rh / 2, d = floorf(0.6f * rh), bar = floorf(0.2f * rh);
            if (i) ui_fill(x + pad, ry, tw, 1, TH.line[0], TH.line[1], TH.line[2], 255);
            if (i == nrows / 2) continue;       /* (the note's row: the note alone) */
            rr(x + pad, cy - bar / 2, floorf(0.4f * d), bar, bar / 2, TH.line, 255);   /* the rank */
            disc(x + pad + 1.2f * d, cy, d, TH.line, 255);
            rr(x + pad + 2.0f * d, cy - bar / 2, floorf(NAME_W[i] * tw), bar, bar / 2, TH.line, 255);
            float sw2 = floorf(SCORE_W[i] * tw);
            rr(x + w - pad - sw2, cy - bar / 2, sw2, bar, bar / 2, TH.line, 255);
        }
        ui_str(U_SOON, b, sizeof b);            /* the note, over the rows */
        float nh = floorf(0.085f * S), px = floorf(0.032f * S), d = floorf(0.042f * S);
        float nw = fminf(tw, text_w(b, px, T_BOLD, TH.text) + d + 1.1f * nh);
        float nx = floorf(x + (w - nw) / 2), ny = floorf(ry0 + (nrows * rh - nh) / 2);
        rr(nx - 1, ny - 1, nw + 2, nh + 2, (nh + 2) / 2, TH.line, 255);
        rr(nx, ny, nw, nh, nh / 2, TH.card, 255);
        ico(ICON_GLOBE, nx + 0.45f * nh + d / 2, ny + nh / 2, d, TH.accent, 255);
        put(b, px, T_BOLD, TH.text, nw - d - 1.1f * nh, nx + 0.45f * nh + d + 0.25f * nh, ny + nh / 2, AL_LEFT);
    }
    return y + bh;
}

static float stats_walk(int draw)
{
    char b[160], b2[160], n[32];
    stats_t st;
    stats_get(&st);
    float x = content_x(), w = content_w(), y = R.y + HEAD - scroll[PAGE_STATS], g = floorf(0.03f * S);
    float pad = floorf(0.045f * S), rad = floorf(0.045f * S);
    if (draw) draw_title(U_STATS, scroll[PAGE_STATS]);
    /* the week: the days played, the streak */
    float wh = floorf(0.31f * S);
    if (draw) {
        rr(x, y, w, wh, rad, TH.card, 255);
        float ty = y + pad + 0.03f * S;
        put(ui_str(U_WEEK, b, sizeof b), floorf(0.042f * S), T_TITLE, TH.dim, w * 0.5f, x + pad, ty, AL_LEFT);
        if (st.streak) {                        /* (none: no "0-day streak") */
            fmt(b2, sizeof b2, ui_str(U_STREAK, b, sizeof b), st.streak, 0, NULL);
            float sw = put(b2, floorf(0.042f * S), T_TITLE, TH.accent, w * 0.4f, x + w - pad, ty, AL_RIGHT);
            ico(ICON_FLAME, x + w - pad - sw - 0.04f * S, ty, 0.055f * S, TH.accent, 255);
        }
        char days[96];
        ui_str(U_DAYS, days, sizeof days);
        const char *lab[7];
        char *p = days;
        for (int i = 0; i < 7; i++) {           /* the days' letters, Monday first */
            while (*p == ' ') p++;
            lab[i] = p;
            p += strcspn(p, " ");
            if (*p) *p++ = 0;
        }
        int first = ui_str(U_WEEK_START, b, sizeof b)[0] == '0' ? 6 : 0;   /* the week's first day: 0 Monday, 6 Sunday */
        int at = (st.weekday - first + 7) % 7;   /* today's place in the strip (after it: days to come) */
        float cg = floorf(0.018f * S), cs = floorf(fminf((w - 2 * pad - 6 * cg) / 7, 0.13f * S));
        float cx0 = x + (w - (7 * cs + 6 * cg)) / 2, cy = y + wh - pad - cs;
        for (int i = 0; i < 7; i++) {
            int wd = (first + i) % 7, played = i <= at && (st.last7 >> (at - i) & 1), today = i == at;
            float cx = cx0 + i * (cs + cg);
            rr(cx, cy, cs, cs, cs * 0.28f, played ? TH.gold : TH.line, 255);
            if (played) ico(ICON_TICK, cx + cs / 2, cy + cs / 2, cs * 0.56f, TH.on_gold, 255);
            else put(lab[wd], floorf(0.4f * cs), T_TITLE, today ? TH.accent : TH.dim, cs * 0.9f, cx + cs / 2,
                     cy + cs / 2, AL_CENTRE);
            if (today && !played) {             /* today, not played yet: a gold underline */
                float lw = cs * 0.36f;
                ui_fill(cx + (cs - lw) / 2, cy + cs * 0.82f, lw, fmaxf(2, cs * 0.05f), TH.accent[0], TH.accent[1],
                        TH.accent[2], 255);
            }
        }
    }
    y += wh + g;
    /* today: the games finished, a word to go on */
    float th = floorf(0.2f * S);
    if (draw) {
        rr(x, y, w, th, rad, TH.card, 255);
        float bs = th - 2 * pad * 0.8f, bx = x + pad * 0.8f, by = y + pad * 0.8f;
        rr(bx, by, bs, bs, bs * 0.25f, TH.gold, 255);
        snprintf(n, sizeof n, "%d", st.today);
        put(n, floorf(0.55f * bs), T_TITLE, TH.on_gold, bs * 0.9f, bx + bs / 2, by + bs / 2, AL_CENTRE);
        float lx = bx + bs + pad * 0.8f, lw = x + w - pad - lx;
        put(ui_str(U_TODAY, b, sizeof b), floorf(0.04f * S), T_TITLE, TH.dim, lw, lx, y + th * 0.36f, AL_LEFT);
        int cheer = st.today <= 0 ? 0 : st.today == 1 ? 1 : st.today < 5 ? 2 : st.today < 10 ? 3 : 4;
        put(ui_str(U_CHEER0 + cheer, b, sizeof b), floorf(0.045f * S), T_TITLE, TH.text, lw, lx, y + th * 0.66f,
            AL_LEFT);
    }
    y += th + g;
    /* the level: its number, the bar to the next */
    float lh = floorf(0.36f * S);
    if (draw) {
        const uint8_t *felt = FELT[!TH.dark];
        rr(x, y, w, lh, rad, felt, 255);
        fmt(b2, sizeof b2, ui_str(U_LEVEL, b, sizeof b), st.level, 0, NULL);
        put(b2, floorf(0.095f * S), T_TITLE, CREAM, w * 0.6f, x + pad * 1.2f, y + lh * 0.25f, AL_LEFT);
        snprintf(n, sizeof n, "%u / %u", (unsigned)st.level_xp, (unsigned)st.level_need);
        float xw = put(n, floorf(0.04f * S), T_BOLD, CREAM, w * 0.5f, x + pad * 1.2f, y + lh * 0.5f, AL_LEFT);
        ico(ICON_BOLT, x + pad * 1.2f + xw + 0.035f * S, y + lh * 0.5f, 0.045f * S, TH.gold, 255);
        float ms = floorf(0.17f * S), mx = x + w - pad - ms, my = y + pad;
        rr(mx, my, ms, ms, ms * 0.25f, CREAM, 40);
        ico(ICON_MEDAL, mx + ms / 2, my + ms / 2, ms * 0.66f, CREAM, 255);
        float bw = w - 2.4f * pad, bh = floorf(0.04f * S), bx = x + 1.2f * pad, by = y + lh * 0.68f;
        float f = st.level_need ? (float)st.level_xp / st.level_need : 0;
        rr(bx, by, bw, bh, bh / 2, (const uint8_t[3]){0, 0, 0}, 70);
        if (f > 0) rr(bx, by, fmaxf(bh, bw * f), bh, bh / 2, TH.gold, 255);
        fmt(b2, sizeof b2, ui_str(U_TO_LEVEL, b, sizeof b), (int)(f * 100), st.level + 1, NULL);
        put(b2, floorf(0.036f * S), T_TITLE, CREAM, bw, x + w / 2, y + lh * 0.86f, AL_CENTRE);
    }
    y += lh + g;
    /* two tiles: the XP, the games finished */
    float tw = floorf((w - g) / 2), tth = floorf(0.22f * S);
    for (int i = 0; i < 2 && draw; i++) {
        float tx0 = x + i * (tw + g), cx = tx0 + tw / 2;
        rr(tx0, y, tw, tth, rad, TH.card, 255);
        ui_str(i ? U_FINISHED : U_XP, b, sizeof b);
        float lw = text_w(b, floorf(0.036f * S), T_BOLD, TH.dim), id = floorf(0.05f * S);
        float sx = cx - (lw + id * 1.3f) / 2;
        ico(i ? ICON_CUP : ICON_BOLT, sx + id / 2, y + tth * 0.3f, id, i ? SEC_C[0] : TH.gold, 255);
        put(b, floorf(0.036f * S), T_BOLD, TH.dim, tw - 2 * pad, sx + id * 1.3f, y + tth * 0.3f, AL_LEFT);
        snprintf(n, sizeof n, "%u", (unsigned)(i ? st.finished : st.xp));
        put(n, floorf(0.085f * S), T_TITLE, TH.text, tw - pad, cx, y + tth * 0.66f, AL_CENTRE);
    }
    y += tth;
    /* the sections: done of all, a bar each */
    float hh = floorf(0.115f * S);
    if (draw) put(ui_str(U_PROFILE, b, sizeof b), floorf(0.046f * S), T_TITLE, TH.accent, w, x, y + hh * 0.62f,
                  AL_LEFT);
    y += hh;
    float rh = floorf(0.135f * S), ph = prog_n * rh + pad;
    if (draw) {
        rr(x, y, w, ph, rad, TH.card, 255);
        const char *menu = text_get("_menu");
        for (int i = 0; i < prog_n; i++) {
            int s = prog_sec[i];
            const uint8_t *c = SEC_C[s < STATS_SECTIONS ? s : 0];
            float ry = y + pad * 0.6f + i * rh, ty = ry + rh * 0.32f, id = floorf(0.055f * S);
            const char *t = menu;
            for (int k = 0; t && k < s; k++) {
                t = strchr(t, '\n');
                if (t) t++;
            }
            int len = t ? (int)strcspn(t, "\n") : 0;
            snprintf(b, sizeof b, "%.*s", len < 150 ? len : 150, t ? t : "");
            ico(1000 + s, x + pad + id / 2, ty, id, NULL, 255);
            put(b, floorf(0.037f * S), T_TITLE, TH.text, w * 0.55f, x + pad + id * 1.4f, ty, AL_LEFT);
            snprintf(n, sizeof n, "%d / %d", prog_done[i], prog_total[i]);
            put(n, floorf(0.037f * S), T_TITLE, c, w * 0.35f, x + w - pad, ty, AL_RIGHT);
            float bw = w - 2 * pad, bh = floorf(0.022f * S), by = ry + rh * 0.68f;
            float f = prog_total[i] ? (float)prog_done[i] / prog_total[i] : 0;
            rr(x + pad, by, bw, bh, bh / 2, c, 50);
            if (f > 0) rr(x + pad, by, fmaxf(bh, bw * f), bh, bh / 2, c, 255);
        }
    }
    y += ph;
    y = boards_walk(draw, x, y, w, hh, pad, rad);
    /* the achievements */
    if (draw) put(ui_str(U_ACHIEVEMENTS, b, sizeof b), floorf(0.046f * S), T_TITLE, TH.accent, w, x, y + hh * 0.62f,
                  AL_LEFT);
    y += hh;
    float ah = floorf(0.18f * S);
    for (int a = 0; a < ACHS; a++) {
        if (draw && y < R.y + R.h && y + ah > R.y) {
            int done = ach_done(a, &st);
            rr(x, y, w, ah, rad, TH.card, 255);
            float is = ah - 2 * pad * 0.7f, ix = x + pad * 0.7f, iy = y + pad * 0.7f;
            rr(ix, iy, is, is, is * 0.25f, done ? TH.gold : TH.line, 255);
            ico(ACH_ICON[a], ix + is / 2, iy + is / 2, is * 0.56f, done ? TH.on_gold : TH.dim, 255);
            float lx = ix + is + pad * 0.7f, bw2 = floorf(0.2f * S), lw = x + w - pad - bw2 - lx;
            put(ui_str(U_ACH + 2 * a, b, sizeof b), floorf(0.045f * S), T_TITLE, done ? TH.text : TH.dim, lw, lx,
                y + ah * 0.37f, AL_LEFT);
            put(ui_str(U_ACH + 2 * a + 1, b, sizeof b), floorf(0.034f * S), T_REG, TH.dim, lw, lx, y + ah * 0.67f,
                AL_LEFT);
            float bx = x + w - pad * 0.7f - bw2, bh2 = floorf(0.07f * S), by = y + (ah - bh2) / 2;
            if (done) {
                disc(x + w - pad * 0.7f - bh2 / 2, y + ah / 2, bh2, TH.gold, 255);
                ico(ICON_TICK, x + w - pad * 0.7f - bh2 / 2, y + ah / 2, bh2 * 0.6f, TH.on_gold, 255);
            } else {
                rr(bx, by, bw2, bh2, bh2 * 0.3f, TH.line, 255);
                put(ui_str(U_LOCKED, b, sizeof b), floorf(0.03f * S), T_BOLD, TH.dim, bw2 * 0.9f, bx + bw2 / 2,
                    by + bh2 / 2, AL_CENTRE);
            }
        }
        y += ah + floorf(0.022f * S);
    }
    y += floorf(0.03f * S);
    return y + scroll[PAGE_STATS] - R.y;
}

/* ---- drawing, input ------------------------------------------------------------------------------------------------ */
void pages_draw_bg(void)
{
    ui_fill(0, 0, (float)ui.w, (float)ui.h, TH.bg[0], TH.bg[1], TH.bg[2], 255);
}

static void fling(void)                         /* the page flies on after a quick drag, slowing down */
{
    if (vel == 0 || grab.on) return;
    Uint32 t = SDL_GetTicks();
    float dt = fminf(50, (float)(t - grab.t));
    grab.t = t;
    scroll[page] = fminf(smax[page], fmaxf(0, scroll[page] + vel * dt));
    vel *= powf(0.996f, dt);
    if (fabsf(vel) < 0.02f || scroll[page] <= 0 || scroll[page] >= smax[page]) vel = 0;
    else ui_animate();
}

void pages_draw(void)
{
    if (page == PAGE_GAMES) {
        draw_games_header();
        if (pages_search() && shell_search_hits() == 0) {
            char b[96];
            put(ui_str(U_NONE, b, sizeof b), floorf(0.04f * S), T_REG, TH.dim, R.w * 0.9f, R.x + R.w / 2,
                pages_games_top() + 0.12f * S, AL_CENTRE);
        }
    } else {
        fling();
        SDL_Rect clip = {(int)R.x, (int)R.y, (int)R.w, (int)R.h};
        SDL_RenderSetClipRect(ui.r, &clip);
        float total;
        if (page == PAGE_STATS) total = stats_walk(1);
        else {
            walk k = {0, 1, 0, 0, A_NONE, 0};
            total = settings_walk(&k);
        }
        SDL_RenderSetClipRect(ui.r, NULL);
        smax[page] = fmaxf(0, total - R.h);
        if (scroll[page] > smax[page]) scroll[page] = smax[page];
        if (smax[page] > 0) {                   /* where the page is: a thin bar on the right */
            float vh = R.h, bar = vh * vh / (vh + smax[page]), by = R.y + (vh - bar) * scroll[page] / smax[page];
            ui_fill(R.x + R.w - S * 0.016f, by, S * 0.008f, bar, TH.text[0], TH.text[1], TH.text[2], 60);
        }
    }
    draw_nav();
    if (TIP.open) draw_tip();
}

int pages_press(float x, float y)
{
    if (TIP.open) {
        tip_press(x, y);
        return 1;
    }
    for (int p = 0; p < PAGES; p++)             /* the ribbon */
        if (in(nav_item(p), x, y)) {
            snd_play(SND_TAP);
            if (p != page) pages_show(p);
            else if (p != PAGE_GAMES) scroll[p] = 0;   /* again: back to its top */
            return 1;
        }
    if (in(NAV, x, y)) return 1;
    if (page == PAGE_GAMES) {
        float sx, sy, sr;
        search_btn(&sx, &sy, &sr);
        if (hypotf(x - sx, y - sy) < sr * 1.4f) {
            snd_play(SND_TAP);
            search_open(!SR.open);
            return 1;
        }
        if (in(chip_box(), x, y)) {             /* the streak: its page */
            snd_play(SND_TAP);
            pages_show(PAGE_STATS);
            return 1;
        }
        if (SR.open && in(SR.field, x, y)) {
            if (SR.q[0] && in(SR.clear, x, y)) {
                SR.q[0] = 0;
                search_changed();
            }
            SDL_StartTextInput();               /* the keyboard back up */
            return 1;
        }
        return y < pages_games_top();           /* the header: nobody's */
    }
    grab.on = 1;
    grab.moved = 0;
    grab.x = x;
    grab.y = y;
    grab.s = scroll[page];
    grab.t = SDL_GetTicks();
    vel = 0;
    return 1;
}

void pages_motion(float x, float y)
{
    if (!grab.on || page == PAGE_GAMES) return;
    if (!grab.moved && hypotf(x - grab.x, y - grab.y) > 0.02f * S) grab.moved = 1;
    if (!grab.moved) return;
    float s = fminf(smax[page], fmaxf(0, grab.s - (y - grab.y)));
    Uint32 t = SDL_GetTicks();
    if (t > grab.t) vel = 0.5f * vel + 0.5f * (s - scroll[page]) / (float)(t - grab.t);
    grab.t = t;
    scroll[page] = s;
}

int pages_release(float x, float y)
{
    if (!grab.on) return page != PAGE_GAMES || TIP.open;
    grab.on = 0;
    if (grab.moved) {
        if (SDL_GetTicks() - grab.t > 80) vel = 0;
        grab.t = SDL_GetTicks();
        ui_animate();
        return 1;
    }
    if (page == PAGE_SETTINGS) settings_tap(x, y);
    else if (page == PAGE_STATS && in(R, x, y) && (stats_walk(0), in(BOARD, x, y))) {   /* the leaderboards: not
                                                                                        * there yet */
        snd_play(SND_TAP);
        pages_soon();
    }
    return 1;
}

void pages_wheel(float dy)
{
    if (page == PAGE_GAMES) return;
    vel = 0;
    scroll[page] = fminf(smax[page], fmaxf(0, scroll[page] + dy));
}

int pages_key(SDL_Keycode k)
{
    if (TIP.open) {
        if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) TIP.open = TIP.thanks = 0;
        return 1;
    }
    if (page == PAGE_GAMES) {
        if (!SR.open) return 0;
        if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) { search_open(0); return 1; }
        if (!SDL_IsTextInputActive()) return 0; /* the keyboard put away: the menu's keys again */
        if (k == SDLK_BACKSPACE && SR.q[0]) {
            SR.q[text_utf8_back(SR.q, (int)strlen(SR.q))] = 0;
            search_changed();
        } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
            SDL_StopTextInput();
        }
        return k != SDLK_UP && k != SDLK_DOWN && k != SDLK_PAGEUP && k != SDLK_PAGEDOWN;   /* (the typed letters) */
    }
    if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) {
        pages_show(PAGE_GAMES);
        return 1;
    }
    float step = k == SDLK_DOWN ? 0.15f * S : k == SDLK_UP ? -0.15f * S : k == SDLK_PAGEDOWN ? R.h * 0.8f
                 : k == SDLK_PAGEUP ? -R.h * 0.8f : 0;
    if (step != 0) pages_wheel(step);
    return k != SDLK_m && k != SDLK_h && k != SDLK_F1;   /* (the sound, the app's rules page: the shell's) */
}

