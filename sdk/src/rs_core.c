/*
 * RetroStone VC SDK: runtime core (frame loop, input, save RAM, RNG,
 * options, assets, logging, strict mode, host API).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "rs_internal.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#define MAX_OPTS 32
#define MAX_LOADED 64

static const rs_game *g_game;
static uint32_t g_frame;
/* the widest screen, or the tallest (a tall screen is RS_SCREEN_W wide) */
#define FB_PIXELS (RS_SCREEN_W_MAX * RS_SCREEN_H > RS_SCREEN_W * RS_SCREEN_H_MAX ? RS_SCREEN_W_MAX * RS_SCREEN_H \
                                                                             : RS_SCREEN_W * RS_SCREEN_H_MAX)
static uint16_t g_fb[FB_PIXELS];
static int      g_req_width = RS_SCREEN_W;      /* the width the frontend asked for (rs_host_set_width) */
static int      g_req_height = RS_SCREEN_H;     /* the height it asked for (rs_host_set_height; past RS_SCREEN_H_MAX
                                                 * too: the display's shape, for a zoomed screen's columns) */
static int      g_tall_max;                     /* the running game's rs_screen_tall() (0: none) */
static int      g_zoom;                         /* ... it was rs_screen_zoom(): the sides cropped */
static int      g_fill = 1;                     /* rs_screen_fill (0: every column, fitted to the width) */
static int      g_focus = -1;                   /* rs_screen_focus_x (-1: the middle) */
static int16_t  g_audio[(RS_AUDIO_RATE / RS_FPS + 2) * 2];
static int      g_audio_frames;
static uint16_t g_pad[RS_PAD_MAX], g_pad_prev[RS_PAD_MAX];
static int      g_pad_conn[RS_PAD_MAX] = {1, 0, 0, 0};
static uint8_t  g_sram[RS_SRAM_SIZE];
static int      g_sram_dirty;
static rs_rng   g_rng = {0x2545F491u};
static rs_log_fn  g_log_fn;
static rs_file_fn g_file_fn;
static char     g_opt_key[MAX_OPTS][32], g_opt_val[MAX_OPTS][96];
static int      g_opt_n;
static struct { char name[96]; void *data; size_t size; char origin[200]; } g_loaded[MAX_LOADED];
static int      g_loaded_n;
static char g_origin[200];
void rs_host_set_file_origin(const char *where) { snprintf(g_origin, sizeof g_origin, "%s", where ? where : ""); }
const char *rs_asset_origin(const char *name)
{
    for (int i = 0; i < g_loaded_n; i++)
        if (!strcmp(g_loaded[i].name, name)) return g_loaded[i].origin[0] ? g_loaded[i].origin : "data folder";
    return "embedded";
}
#ifdef RS_DEBUG
static int      g_strict = 1;
#else
static int      g_strict = 0;
#endif
static int      g_warned[RS_WARN_KIND_COUNT];
static rs_host_stats g_stats;

/* ---- time -------------------------------------------------------------- */
uint64_t rs_host_time_us(void)
{
#ifdef _WIN32
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (uint64_t)(c.QuadPart * 1000000.0 / (double)f.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
#endif
}

/* ---- logging ------------------------------------------------------------ */
static void log_line(const char *prefix, const char *fmt, va_list ap)
{
    char buf[512];
    size_t n = (size_t)snprintf(buf, sizeof buf, "%s", prefix);
    vsnprintf(buf + n, sizeof buf - n, fmt, ap);
    if (g_log_fn) g_log_fn(buf);
    else fprintf(stderr, "%s\n", buf);
}
void rs_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_line("[rs] ", fmt, ap);
    va_end(ap);
}
void rs_set_strict(int on) { g_strict = on; }
int rs_strict(void) { return g_strict; }
void rs_warn(int kind, const char *fmt, ...)
{
    if ((unsigned)kind >= RS_WARN_KIND_COUNT) return;
    if (g_warned[kind]++ || !g_strict) return;
    va_list ap;
    va_start(ap, fmt);
    log_line("[rs strict] ", fmt, ap);
    va_end(ap);
}
int rs_warn_count(int kind) { return (unsigned)kind < RS_WARN_KIND_COUNT ? g_warned[kind] : 0; }

/* ---- options ---------------------------------------------------------------- */
void rs_host_set_option(const char *k, const char *v)
{
    for (int i = 0; i < g_opt_n; i++)
        if (!strcmp(g_opt_key[i], k)) {
            snprintf(g_opt_val[i], sizeof g_opt_val[i], "%s", v);
            return;
        }
    if (g_opt_n >= MAX_OPTS) return;
    snprintf(g_opt_key[g_opt_n], sizeof g_opt_key[0], "%s", k);
    snprintf(g_opt_val[g_opt_n], sizeof g_opt_val[0], "%s", v);
    g_opt_n++;
}
const char *rs_option(const char *k)
{
    for (int i = 0; i < g_opt_n; i++)
        if (!strcmp(g_opt_key[i], k)) return g_opt_val[i];
    return NULL;
}
int rs_option_int(const char *k, int fb)
{
    const char *v = rs_option(k);
    return v ? atoi(v) : fb;
}

/* ---- assets ------------------------------------------------------------- */
void rs_host_set_file_loader(rs_file_fn fn) { g_file_fn = fn; }
static const void *rs_asset_reload_(const char *name, size_t *size, int again);
const void *rs_asset(const char *name, size_t *size)
{
    for (int i = 0; i < g_loaded_n; i++)
        if (!strcmp(g_loaded[i].name, name)) {
            if (size) *size = g_loaded[i].size;
            return g_loaded[i].data;
        }
    return rs_asset_reload_(name, size, 0);
}

static int g_dev_key;
void rs_host_set_dev_key(int key) { g_dev_key = key; }
int rs_dev_key(void) { return g_dev_key; }
void rs_perf(rs_perf_info *out)
{
    out->update_us = g_stats.update_us;
    out->render_us = g_stats.render_us;
    out->sprites = g_stats.sprites;
    out->max_sprites_line = g_stats.max_sprites_line;
}

const void *rs_asset_reload(const char *name, size_t *size)
{
    for (int i = 0; i < g_loaded_n; i++)
        if (!strcmp(g_loaded[i].name, name)) {       /* drop the cached copy (the old data is kept:
                                                        pointers to it may still be in use) */
            g_loaded[i] = g_loaded[--g_loaded_n];
            break;
        }
    return rs_asset_reload_(name, size, 1);
}

static const void *rs_asset_reload_(const char *name, size_t *size, int again)
{
    (void)again;
    if (g_file_fn && g_loaded_n < MAX_LOADED) {
        size_t sz = 0;
        void *d = g_file_fn(name, &sz);
        if (d) {
            snprintf(g_loaded[g_loaded_n].name, sizeof g_loaded[0].name, "%s", name);
            g_loaded[g_loaded_n].data = d;
            g_loaded[g_loaded_n].size = sz;
            snprintf(g_loaded[g_loaded_n].origin, sizeof g_loaded[0].origin, "%s", g_origin);
            g_loaded_n++;
            rs_log("asset %s loaded from %s (%u bytes)", name, g_origin[0] ? g_origin : "the data directory", (unsigned)sz);
            if (size) *size = sz;
            return d;
        }
    }
    if (g_game && g_game->assets)
        for (const rs_asset_entry *e = g_game->assets; e->name; e++)
            if (!strcmp(e->name, name)) {
                if (size) *size = e->size;
                return e->data;
            }
    if (size) *size = 0;
    return NULL;
}
static void assets_check(void)
{
    size_t total = 0;
    if (!g_game || !g_game->assets) return;
    for (const rs_asset_entry *e = g_game->assets; e->name; e++) {
        total += e->size;
        if (e->size > 512 * 1024)
            rs_warn(RS_WARN_BANK, "asset %s is %u bytes (bank guideline 512 KiB)", e->name, (unsigned)e->size);
    }
    if (total > 8u * 1024 * 1024)
        rs_warn(RS_WARN_CART, "assets total %u bytes (cartridge guideline 8 MiB)", (unsigned)total);
}

/* ---- input -------------------------------------------------------------- */
void rs_host_set_pad(int p, uint16_t b, int conn)
{
    if ((unsigned)p >= RS_PAD_MAX) return;
    g_pad[p] = b;
    g_pad_conn[p] = conn;
}
uint16_t rs_pad(int p) { return (unsigned)p < RS_PAD_MAX ? g_pad[p] : 0; }
uint16_t rs_pad_pressed(int p) { return (unsigned)p < RS_PAD_MAX ? (uint16_t)(g_pad[p] & ~g_pad_prev[p]) : 0; }
uint16_t rs_pad_released(int p) { return (unsigned)p < RS_PAD_MAX ? (uint16_t)(~g_pad[p] & g_pad_prev[p]) : 0; }
int rs_pad_connected(int p) { return (unsigned)p < RS_PAD_MAX ? g_pad_conn[p] : 0; }
static uint8_t g_pad_dev[RS_PAD_MAX];
void rs_host_set_pad_device(int p, int d)
{
    if ((unsigned)p < RS_PAD_MAX && d >= RS_DEVICE_PAD && d <= RS_DEVICE_KEYBOARD2) g_pad_dev[p] = (uint8_t)d;
}
int rs_pad_device(int p) { return (unsigned)p < RS_PAD_MAX ? g_pad_dev[p] : RS_DEVICE_PAD; }

/* ---- touch (docs/spec.md "Touch") ---------------------------------------------------------------------------------
 * The frontend gives the fingers down (rs_host_set_touches); at the start of each frame touch_step() matches them to
 * the slots by id and works out the edges and gestures. Not part of save states: a load forgets the fingers. */
static rs_host_touch g_tin[RS_TOUCH_MAX];       /* what the frontend gave */
static int g_tin_n;
static rs_touch_point g_touch[RS_TOUCH_MAX];
static long g_touch_id[RS_TOUCH_MAX];
static uint8_t g_touch_busy[RS_TOUCH_MAX];      /* down, or released this frame */
#define HIST_N (RS_TOUCH_SWIPE_FRAMES + 1)
static int16_t g_hist[RS_TOUCH_MAX][HIST_N][2];   /* each finger's recent positions (by frames % HIST_N): swipes */

void rs_host_set_touches(int n, const rs_host_touch *t)
{
    g_tin_n = 0;
    for (int i = 0; i < n && t && g_tin_n < RS_TOUCH_MAX; i++) {
        int dup = 0;
        for (int k = 0; k < g_tin_n; k++) dup |= g_tin[k].id == t[i].id;
        if (!dup) g_tin[g_tin_n++] = t[i];
    }
}

static void touch_reset(void)                   /* the slots (the fingers the frontend gave stay down) */
{
    memset(g_touch, 0, sizeof g_touch);
    memset(g_touch_busy, 0, sizeof g_touch_busy);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void touch_step(void)
{
    int w = rs_screen_width(), h = rs_screen_height();
    uint8_t seen[RS_TOUCH_MAX] = {0};
    for (int s = 0; s < RS_TOUCH_MAX; s++) {
        rs_touch_point *p = &g_touch[s];
        if (g_touch_busy[s] && !p->down) g_touch_busy[s] = 0;       /* released last frame: the slot is free */
        if (!g_touch_busy[s]) { memset(p, 0, sizeof *p); continue; }
        p->pressed = p->released = p->tap = p->swipe = 0;
        int f = -1;
        for (int i = 0; i < g_tin_n; i++)
            if (!seen[i] && g_tin[i].id == g_touch_id[s]) { f = i; break; }
        if (f < 0) {                                                /* lifted */
            p->down = 0;
            p->released = 1;
            p->tap = !p->moved && p->frames <= RS_TOUCH_TAP_FRAMES;
            continue;
        }
        seen[f] = 1;
        p->x = (int16_t)clampi(g_tin[f].x, 0, w - 1);
        p->y = (int16_t)clampi(g_tin[f].y, 0, h - 1);
        if (p->frames < 0xffff) p->frames++;
        int dx = p->x - p->x0, dy = p->y - p->y0;
        if (dx * dx + dy * dy > RS_TOUCH_SLOP * RS_TOUCH_SLOP) p->moved = 1;
        /* a swipe: RS_TOUCH_SWIPE_DIST from where the finger was at most RS_TOUCH_SWIPE_FRAMES ago (a flick, also
           from a finger held for a while), mostly along one axis (the other at most half as far); once per touch */
        g_hist[s][p->frames % HIST_N][0] = p->x;
        g_hist[s][p->frames % HIST_N][1] = p->y;
        int from = p->frames > RS_TOUCH_SWIPE_FRAMES ? p->frames - RS_TOUCH_SWIPE_FRAMES : 0;
        for (int j = from; j < p->frames && !p->swiped; j++) {
            int sx = p->x - g_hist[s][j % HIST_N][0], sy = p->y - g_hist[s][j % HIST_N][1];
            int bx = sx < 0 ? -sx : sx, by = sy < 0 ? -sy : sy;
            if (bx < RS_TOUCH_SWIPE_DIST && by < RS_TOUCH_SWIPE_DIST) continue;
            if (by >= bx && bx * 2 <= by) p->swiped = sy < 0 ? RS_SWIPE_UP : RS_SWIPE_DOWN;
            else if (bx > by && by * 2 <= bx) p->swiped = sx < 0 ? RS_SWIPE_LEFT : RS_SWIPE_RIGHT;
            p->swipe = p->swiped;
        }
    }
    for (int i = 0; i < g_tin_n; i++) {                             /* new fingers: a free slot each */
        if (seen[i]) continue;
        int s = 0;
        while (s < RS_TOUCH_MAX && g_touch_busy[s]) s++;
        if (s == RS_TOUCH_MAX) break;                               /* full: it gets a slot once one is free */
        rs_touch_point *p = &g_touch[s];
        memset(p, 0, sizeof *p);
        g_touch_busy[s] = 1;
        g_touch_id[s] = g_tin[i].id;
        p->x = p->x0 = (int16_t)clampi(g_tin[i].x, 0, w - 1);
        p->y = p->y0 = (int16_t)clampi(g_tin[i].y, 0, h - 1);
        p->down = p->pressed = 1;
        g_hist[s][0][0] = p->x;
        g_hist[s][0][1] = p->y;
    }
}

const rs_touch_point *rs_touch(int i) { return (unsigned)i < RS_TOUCH_MAX && g_touch_busy[i] ? &g_touch[i] : NULL; }
int rs_touch_count(void)
{
    int n = 0;
    for (int s = 0; s < RS_TOUCH_MAX; s++) n += g_touch_busy[s] && g_touch[s].down;
    return n;
}
const rs_touch_point *rs_touch_first(void)
{
    const rs_touch_point *best = NULL;
    for (int s = 0; s < RS_TOUCH_MAX; s++)
        if (g_touch_busy[s] && g_touch[s].down && (!best || g_touch[s].frames > best->frames)) best = &g_touch[s];
    return best;
}
static int touch_at(const rs_touch_point *p, int *x, int *y)
{
    if (x) *x = p->x;
    if (y) *y = p->y;
    return 1;
}
int rs_touch_tap(int *x, int *y)
{
    for (int s = 0; s < RS_TOUCH_MAX; s++)
        if (g_touch_busy[s] && g_touch[s].tap) return touch_at(&g_touch[s], x, y);
    return 0;
}
int rs_touch_swipe(int *x, int *y)
{
    for (int s = 0; s < RS_TOUCH_MAX; s++)
        if (g_touch_busy[s] && g_touch[s].swipe) {
            if (x) *x = g_touch[s].x0;
            if (y) *y = g_touch[s].y0;
            return g_touch[s].swipe;
        }
    return 0;
}
int rs_touch_hold(int *x, int *y)
{
    for (int s = 0; s < RS_TOUCH_MAX; s++) {
        const rs_touch_point *p = &g_touch[s];
        if (g_touch_busy[s] && p->down && !p->moved && p->frames >= RS_TOUCH_HOLD_FRAMES) return touch_at(p, x, y);
    }
    return 0;
}
int rs_touch_half(int x) { return x < rs_screen_width() / 2 ? RS_TOUCH_LEFT : RS_TOUCH_RIGHT; }
int rs_touch_halves(void)
{
    int m = 0;
    for (int s = 0; s < RS_TOUCH_MAX; s++)
        if (g_touch_busy[s] && g_touch[s].down) m |= rs_touch_half(g_touch[s].x);
    return m;
}

/* ---- save RAM ------------------------------------------------------------ */
uint8_t *rs_sram(void) { return g_sram; }
void rs_sram_commit(void) { g_sram_dirty = 1; }
uint8_t *rs_host_sram(void) { return g_sram; }
int rs_host_sram_dirty(int clear)
{
    int d = g_sram_dirty;
    if (clear) g_sram_dirty = 0;
    return d;
}

/* ---- RNG (xorshift32) ------------------------------------------------------- */
void rs_rng_seed(rs_rng *r, uint32_t s) { r->s = s ? s : 0x2545F491u; }
uint32_t rs_rng_next(rs_rng *r)
{
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return r->s = x;
}
int rs_rng_range(rs_rng *r, int n) { return n > 0 ? (int)(((uint64_t)rs_rng_next(r) * (uint32_t)n) >> 32) : 0; }
void rs_srand(uint32_t s) { rs_rng_seed(&g_rng, s); }
uint32_t rs_rand(void) { return rs_rng_next(&g_rng); }
int rs_rand_range(int n) { return rs_rng_range(&g_rng, n); }

/* ---- host lifecycle ------------------------------------------------------ */
void rs_host_set_log(rs_log_fn fn) { g_log_fn = fn; }
uint32_t rs_frame_count(void) { return g_frame; }

/* ---- widescreen ------------------------------------------------------------------------------------------- */
static int clamp_width(int w) { return w < RS_SCREEN_W_MIN ? RS_SCREEN_W_MIN : w > RS_SCREEN_W_MAX ? RS_SCREEN_W_MAX : w; }
void rs_host_set_width(int w) { g_req_width = clamp_width(w); }
int rs_host_width_for(const rs_game *g, int w)
{
    int mx = g ? clamp_width(g->max_width) : RS_SCREEN_W_MAX;    /* no game (unit tests): any width */
    w = clamp_width(w);
    return w < mx ? w : mx;
}
int rs_host_width_for_aspect(int num, int den)
{
    if (num <= 0 || den <= 0) return RS_SCREEN_W;
    return clamp_width((int)(((long)RS_SCREEN_H * num * 2 + den) / (2L * den)));
}

/* ---- tall screen --------------------------------------------------------------------------------------------- */
static int clamp_height(int h) { return h < RS_SCREEN_H ? RS_SCREEN_H : h > RS_SCREEN_H_MAX ? RS_SCREEN_H_MAX : h; }
/* the request keeps the display's shape past RS_SCREEN_H_MAX (a 9:21 phone: 746), for a zoomed screen's columns */
#define REQ_H_MAX (RS_SCREEN_W * 4)
static int clamp_req_height(int h) { return h < RS_SCREEN_H ? RS_SCREEN_H : h > REQ_H_MAX ? REQ_H_MAX : h; }
void rs_host_set_height(int h) { g_req_height = clamp_req_height(h); }
int rs_host_height_for(const rs_game *g, int w, int h)
{
    if (rs_host_width_for(g, w) != RS_SCREEN_W) return RS_SCREEN_H;   /* tall at 320 columns only */
    int mx = !g ? RS_SCREEN_H_MAX : g == g_game && g_tall_max ? g_tall_max : RS_SCREEN_H;
    h = clamp_height(h);
    return h < mx ? h : mx;
}
/* the game's opt-in, from its init(): the height now, the stage centred (ppu_reset's origin for this height) */
int rs_screen_tall(int max_height)
{
    g_tall_max = clamp_height(max_height);
    ppu_set_height(rs_host_height_for(g_game, g_req_width, g_req_height));
    rs_screen_origin_v(rs_screen_vmargin());
    return rs_screen_height();
}
int rs_screen_zoom(int height)
{
    g_zoom = 1;
    return rs_screen_tall(height);
}

/* the columns a zoomed screen shows: its height filling the display's (req_h lines at 320 columns), even, rounded
 * down; every column when it is not zoomed, not tall, or the display not taller than the screen */
static int visible_w(int w, int h, int req_h)
{
    if (!g_zoom || !g_fill || w != RS_SCREEN_W || h <= RS_SCREEN_H || req_h <= h) return w;
    int v = (int)((long)RS_SCREEN_W * h / req_h) & ~1;
    return v < RS_SCREEN_W / 2 ? RS_SCREEN_W / 2 : v;
}
int rs_screen_visible_x0(void)
{
    int w = rs_screen_width(), v = visible_w(w, rs_screen_height(), g_req_height);
    int x = g_focus < 0 ? (w - v) / 2 : g_focus - v / 2;
    return x < 0 ? 0 : x > w - v ? w - v : x;
}
int rs_screen_visible_x1(void)
{
    return rs_screen_visible_x0() + visible_w(rs_screen_width(), rs_screen_height(), g_req_height);
}
void rs_screen_focus_x(int x) { g_focus = x < 0 ? -1 : x; }
void rs_screen_fill(int on) { g_fill = !!on; }

void rs_host_reset(void)
{
    ppu_set_width(rs_host_width_for(g_game, g_req_width));
    g_tall_max = 0;                             /* 240 lines until the game opts in (rs_screen_tall) */
    g_zoom = 0;
    g_fill = 1;
    g_focus = -1;
    ppu_set_height(RS_SCREEN_H);
    ppu_reset();
    apu_reset();
    text_reset();
    memset(g_pad, 0, sizeof g_pad);
    memset(g_pad_prev, 0, sizeof g_pad_prev);
    touch_reset();
    memset(g_warned, 0, sizeof g_warned);
    rs_srand(0x2545F491u);
    g_frame = 0;
}

int rs_host_init(const rs_game *game)
{
    g_game = game;
    rs_host_reset();
    if (rs_option("strict")) g_strict = rs_option_int("strict", g_strict);
    if (rs_screen_width() != RS_SCREEN_W)
        rs_log("RetroStone VC %s: %s %s%s, %dx%d", RS_VERSION, game->name, game->version ? game->version : "",
               g_strict ? " (strict)" : "", rs_screen_width(), RS_SCREEN_H);
    else
        rs_log("RetroStone VC %s: %s %s%s", RS_VERSION, game->name, game->version ? game->version : "",
               g_strict ? " (strict)" : "");
    assets_check();
    if (game->init) game->init();
    if (rs_screen_height() != RS_SCREEN_H) rs_log("tall screen: %dx%d", rs_screen_width(), rs_screen_height());
    return 0;
}

void rs_host_shutdown(void)
{
    if (g_game && g_game->shutdown) g_game->shutdown();
    apu_shutdown();
    for (int i = 0; i < g_loaded_n; i++) free(g_loaded[i].data);
    g_loaded_n = 0;
    g_game = NULL;
}

void rs_host_render(void) { ppu_render(g_fb); }

int rs_host_change_width(int width) { return rs_host_change_size(width, g_req_height); }

int rs_host_change_size(int width, int height)
{
    const rs_game *g = g_game;
    int cur = rs_screen_width(), cur_h = rs_screen_height();
    if (!g) { rs_host_set_width(width); rs_host_set_height(height); return cur; }
    int want = rs_host_width_for(g, width), want_h = rs_host_height_for(g, width, height);
    /* (a zoomed screen: the same size may show other columns on a phone of another shape: a restart too) */
    int same_cols = visible_w(cur, cur_h, g_req_height) == visible_w(want, want_h, clamp_req_height(height));
    if (want == cur && want_h == cur_h && same_cols) {
        g_req_width = clamp_width(width);
        g_req_height = clamp_req_height(height);
        return cur;
    }
    size_t size = rs_host_state_size(g);
    void *buf = size ? malloc(size) : NULL;
    if (!buf || !rs_host_state_save(buf, size)) {
        rs_log("the size stays %dx%d: the game has no save state to restart from", cur, cur_h);
        free(buf);
        return cur;
    }
    int old_req = g_req_width, old_req_h = g_req_height;
    rs_host_shutdown();
    g_req_width = clamp_width(width);
    g_req_height = clamp_req_height(height);
    rs_host_init(g);
    if (rs_host_state_load(buf, size)) {           /* refused at the new size: back to the old one */
        rs_log("the state did not load at %dx%d: back to %dx%d", rs_screen_width(), rs_screen_height(), cur, cur_h);
        rs_host_shutdown();
        g_req_width = old_req;
        g_req_height = old_req_h;
        rs_host_init(g);
        rs_host_state_load(buf, size);
    }
    free(buf);
    return rs_screen_width();
}

void rs_host_frame(void)
{
    uint64_t t0 = rs_host_time_us();
    touch_step();
    if (g_game) {
        if (g_game->update) g_game->update();
        if (g_game->draw) g_game->draw();
    }
    uint64_t t1 = rs_host_time_us();
    ppu_render(g_fb);
    uint64_t t2 = rs_host_time_us();
    uint64_t a = (uint64_t)g_frame * RS_AUDIO_RATE / RS_FPS;
    uint64_t b = (uint64_t)(g_frame + 1) * RS_AUDIO_RATE / RS_FPS;
    g_audio_frames = (int)(b - a);
    apu_render(g_audio, g_audio_frames);
    uint64_t t3 = rs_host_time_us();
    g_stats.update_us = (uint32_t)(t1 - t0);
    g_stats.render_us = (uint32_t)(t2 - t1);
    g_stats.audio_us = (uint32_t)(t3 - t2);
    g_stats.sprites = ppu_last_sprites();
    g_stats.max_sprites_line = ppu_last_max_line();
    g_dev_key = 0;
    memcpy(g_pad_prev, g_pad, sizeof g_pad);
    g_frame++;
}

/* ---- save states: the frame counter, the global RNG and the pads (their edges must survive) ----------------- */
const rs_game *core_game(void) { return g_game; }

const char *core_asset_name(const void *data)
{
    if (!data) return NULL;
    for (int i = 0; i < g_loaded_n; i++)
        if (g_loaded[i].data == data) return g_loaded[i].name;
    if (g_game && g_game->assets)
        for (const rs_asset_entry *e = g_game->assets; e->name; e++)
            if (e->data == data) return e->name;
    return NULL;
}

const void *core_asset_find(const char *name, size_t *size)
{
    for (int i = 0; i < g_loaded_n; i++)
        if (!strcmp(g_loaded[i].name, name)) { *size = g_loaded[i].size; return g_loaded[i].data; }
    if (g_game && g_game->assets)
        for (const rs_asset_entry *e = g_game->assets; e->name; e++)
            if (!strcmp(e->name, name)) { *size = e->size; return e->data; }
    *size = 0;
    return NULL;
}

void core_state_save(rs_wr *w)
{
    wr_u32(w, g_frame);
    wr_u32(w, g_rng.s);
    for (int p = 0; p < RS_PAD_MAX; p++) {
        wr_u16(w, g_pad[p]);
        wr_u16(w, g_pad_prev[p]);
        wr_u8(w, (uint8_t)g_pad_conn[p]);
    }
}

int core_state_load(rs_rd *r, int apply)
{
    uint32_t frame = rd_u32(r), rng = rd_u32(r);
    uint16_t pad[RS_PAD_MAX], prev[RS_PAD_MAX];
    uint8_t conn[RS_PAD_MAX];
    for (int p = 0; p < RS_PAD_MAX; p++) {
        pad[p] = rd_u16(r);
        prev[p] = rd_u16(r);
        conn[p] = rd_u8(r);
    }
    if (r->err || !rng) return -1;          /* xorshift never reaches 0 */
    if (!apply) return 0;
    g_frame = frame;
    g_rng.s = rng;
    for (int p = 0; p < RS_PAD_MAX; p++) {
        g_pad[p] = pad[p];
        g_pad_prev[p] = prev[p];
        g_pad_conn[p] = conn[p] ? 1 : 0;
    }
    g_dev_key = 0;
    touch_reset();                          /* touches are not saved: a finger still down is a new touch */
    return 0;
}

const uint16_t *rs_host_framebuffer(void) { return g_fb; }
const int16_t *rs_host_audio(int *frames)
{
    if (frames) *frames = g_audio_frames;
    return g_audio;
}
const rs_host_stats *rs_host_stats_last(void) { return &g_stats; }
