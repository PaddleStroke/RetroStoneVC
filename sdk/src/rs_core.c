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
static uint16_t g_fb[RS_SCREEN_W * RS_SCREEN_H];
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
static struct { char name[96]; void *data; size_t size; } g_loaded[MAX_LOADED];
static int      g_loaded_n;
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
const void *rs_asset(const char *name, size_t *size)
{
    for (int i = 0; i < g_loaded_n; i++)
        if (!strcmp(g_loaded[i].name, name)) {
            if (size) *size = g_loaded[i].size;
            return g_loaded[i].data;
        }
    if (g_file_fn && g_loaded_n < MAX_LOADED) {
        size_t sz = 0;
        void *d = g_file_fn(name, &sz);
        if (d) {
            snprintf(g_loaded[g_loaded_n].name, sizeof g_loaded[0].name, "%s", name);
            g_loaded[g_loaded_n].data = d;
            g_loaded[g_loaded_n].size = sz;
            g_loaded_n++;
            rs_log("asset %s loaded from the data directory (%u bytes)", name, (unsigned)sz);
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

void rs_host_reset(void)
{
    ppu_reset();
    apu_reset();
    text_reset();
    memset(g_pad, 0, sizeof g_pad);
    memset(g_pad_prev, 0, sizeof g_pad_prev);
    memset(g_warned, 0, sizeof g_warned);
    rs_srand(0x2545F491u);
    g_frame = 0;
}

int rs_host_init(const rs_game *game)
{
    g_game = game;
    rs_host_reset();
    if (rs_option("strict")) g_strict = rs_option_int("strict", g_strict);
    rs_log("RetroStone VC %s: %s %s%s", RS_VERSION, game->name, game->version ? game->version : "",
           g_strict ? " (strict)" : "");
    assets_check();
    if (game->init) game->init();
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

void rs_host_frame(void)
{
    uint64_t t0 = rs_host_time_us();
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
    memcpy(g_pad_prev, g_pad, sizeof g_pad);
    g_frame++;
}

const uint16_t *rs_host_framebuffer(void) { return g_fb; }
const int16_t *rs_host_audio(int *frames)
{
    if (frames) *frames = g_audio_frames;
    return g_audio;
}
const rs_host_stats *rs_host_stats_last(void) { return &g_stats; }
