/*
 * Map Quiz in RetroStone Board, for one player: a map of the world, a continent or a country's states; a place's
 * name comes up at the top ("Find: Peru") and the player taps it on the map (the maps and the quiz: mapquiz_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The map: a sea of soft blue-grey in a walnut frame, the land in parchment tones (neighbours differ), the places
 * not asked muted, the disputed areas in neutral grey (never asked, never named), the borders in the house ink; drawn
 * by the GPU as triangles cut once (SDL_RenderGeometry), the lines as thin quads. A place found turns green; a wrong
 * tap flashes the place tapped red and names it a moment; the third miss on a prompt shows the place asked, pulsing,
 * and marks it orange. A place too small to tap (Vatican, Pacific islands, Singapore) gets a round marker, the nearest
 * marker under the finger winning. Two fingers pinch to zoom and pan, one drags; the tools zoom in, out and back to
 * the whole map; the keys move a cursor (arrows), tap with it (Return, space) and zoom (+, -, Page Up, Page Down).
 * Stars: few misses and a lenient time (mq_stars); each quiz keeps its best time. Levels in chapters by continent,
 * the world, then countries' states; the die tab plays a whole map in a random order.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/mapquiz.h"
#include "shell.h"
#include "text.h"

/* ---- the colours ------------------------------------------------------------------------------------------------- */
static const uint8_t SEA[3] = {164, 186, 197}, SEA_EDGE[3] = {140, 164, 178};
static const uint8_t TONES[8][3] = {{238, 222, 188}, {225, 204, 160}, {244, 232, 206}, {216, 194, 150},
                                    {232, 213, 175}, {210, 188, 146}, {241, 226, 196}, {222, 202, 168}};
static const uint8_t OTHER[3] = {206, 200, 186}, GREY[3] = {176, 176, 174};
static const uint8_t FOUND[3] = {126, 176, 112}, MISSED[3] = {228, 150, 66}, RED[3] = {204, 72, 58};
static const uint8_t INK[3] = {84, 56, 36}, GOLD[3] = {236, 194, 96}, CREAM[3] = {250, 244, 230};
static const uint8_t PLATE[3] = {58, 40, 28};

#define FLASH_MS 650                            /* a wrong place's red flash */
#define LABEL_MS 1500                           /* its name shown */
#define REVEAL_MS 1700                          /* the answer shown after the last miss */
#define FILL_MS 380                             /* a place found turning green */
#define MAX_FINGERS 2

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static mq_map M;
static int loaded = -1;
static mq_quiz Q;
static int16_t set[MQ_MAX_REG];
static Uint32 t_start, t_end;                   /* the quiz's clock, ui_play_ms (t_end: 0 while it runs) */
static uint32_t best_ms;                        /* the best time kept for this quiz (0: none) */
static int new_best;

static box room, banner, view;                  /* the game's room; the prompt's plate; the map's window */
static float fit;                               /* pixels a grid unit, the whole map shown */
static float zoom = 1, cx, cy;                  /* the view: zoom (1: the whole map), its middle (grid units) */
static float tzoom = 1, tcx, tcy;               /* where it is going */
static float ezoom = 1, ecx, ecy;               /* where it was as it set off, at view_t: the view at any time is a
                                                 * function of ui_ms() since (not of the frames drawn meanwhile: a
                                                 * game played again draws fewer) */
static Uint32 view_t;
static float zmax = 8, zmin = 1;                /* zmin: the map covers the window (its edges never show) */
static float hx, hy, hz = 1;                    /* the level's own view: its places framed */
static int need_home;                           /* a new quiz: its view once the window is known */

static int flash_r = -1, label_r = -1, reveal_r = -1, found_r = -1;
static Uint32 flash_t0, label_t0, reveal_t0, found_t0;
static float label_x, label_y;

static SDL_Texture *t_panel, *t_prompt, *t_label, *t_disc, *t_ring, *t_found, *t_miss, *t_clock, *t_best;
static int panel_w, panel_h, prompt_key = -1, prompt_w, prompt_h, label_key = -1, label_w, label_h, small_px;

static SDL_Vertex *vx;                          /* the fills' corners (each ring's points) */
static int *fidx, nfidx, nfidx_main;           /* the fills' triangles, the biggest region first; the insets' after */
static SDL_Vertex *lv;                          /* the lines' quads */
static int *lidx, lcap;

/* the pointer: one finger (or the mouse) drags, two pinch */
static struct { int id; float x, y; } fg[MAX_FINGERS];
static int nfg, gesture_moved, pinching;
static float g_x0, g_y0, g_cx0, g_cy0, g_d0, g_z0, g_mx, g_my;
static int drag_on;

static struct { int on; float x, y; Uint32 t; } cur;   /* the keys' cursor */

/* ---- the view ---------------------------------------------------------------------------------------------------- */
static float scale_of(float z) { return fit * z; }

static void clamp_view(float *x, float *y, float z)
{
    float s = scale_of(z), hw = view.w / 2 / s, hh = view.h / 2 / s;
    *x = M.w <= 2 * hw ? M.w / 2.0f : clampf(*x, hw, M.w - hw);
    *y = M.h <= 2 * hh ? M.h / 2.0f : clampf(*y, hh, M.h - hh);
}

static void to_screen(float gx, float gy, float *sx, float *sy)
{
    float s = scale_of(zoom);
    *sx = view.x + view.w / 2 + (gx - cx) * s;
    *sy = view.y + view.h / 2 + (gy - cy) * s;
}

static void to_grid(float sx, float sy, float *gx, float *gy)
{
    float s = scale_of(zoom);
    *gx = cx + (sx - view.x - view.w / 2) / s;
    *gy = cy + (sy - view.y - view.h / 2) / s;
}

static void view_set(float x, float y, float z)   /* at once (a finger) */
{
    z = clampf(z, zmin, zmax);
    clamp_view(&x, &y, z);
    cx = tcx = ecx = x;
    cy = tcy = ecy = y;
    zoom = tzoom = ezoom = z;
}

static void view_at(Uint32 now)                 /* the ease at `now`: there, eased from (ecx, ecy, ezoom) */
{
    Sint32 dt = (Sint32)(now - view_t);
    float k = dt <= 0 ? 1 : dt > 4000 ? 0 : expf(-(float)dt / 70.0f);
    zoom = clampf(tzoom + (ezoom - tzoom) * k, zmin, zmax);
    cx = tcx + (ecx - tcx) * k;
    cy = tcy + (ecy - tcy) * k;
    clamp_view(&cx, &cy, zoom);
}

static void view_go(float x, float y, float z)    /* eased there (a button, a key) */
{
    Uint32 now = ui_ms();
    if (zoom != tzoom || cx != tcx || cy != tcy) view_at(now);   /* (on the way: it sets off from where it is now) */
    ezoom = zoom, ecx = cx, ecy = cy;
    tzoom = clampf(z, zmin, zmax);
    tcx = x;
    tcy = y;
    clamp_view(&tcx, &tcy, tzoom);
    view_t = now;
    ui_animate();
}

/* zoom by k keeping the point under (sx, sy) where it is */
static void zoom_at(float sx, float sy, float k, int eased)
{
    float gx, gy, z = clampf(tzoom * k, zmin, zmax);
    to_grid(sx, sy, &gx, &gy);
    float s = scale_of(z);
    float x = gx - (sx - view.x - view.w / 2) / s, y = gy - (sy - view.y - view.h / 2) / s;
    if (eased) view_go(x, y, z);
    else view_set(x, y, z);
}

static void view_step(void)
{
    if (zoom == tzoom && cx == tcx && cy == tcy) return;
    view_at(ui_ms());
    if (fabsf(zoom - tzoom) < 1e-3f && fabsf(cx - tcx) < 0.05f && fabsf(cy - tcy) < 0.05f) {
        zoom = ezoom = tzoom, cx = ecx = tcx, cy = ecy = tcy;
        return;
    }
    ui_animate();
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the frame and the sea: a walnut frame round a sea darker towards its edges */
static void paint_panel(uint8_t *o, int w, int h, int fr)
{
    art_plank(w - 2 * fr, h - 2 * fr, fr, o);
    float mx = (w - 1) / 2.0f, my = (h - 1) / 2.0f;
    for (int y = fr; y < h - fr; y++)
        for (int x = fr; x < w - fr; x++) {
            float dx = (x - mx) / mx, dy = (y - my) / my, d = fminf(1, sqrtf(dx * dx + dy * dy) / 1.25f);
            float k = d * d, n = art_noise(x / 3, y / 3) * 4;
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)clampf(SEA[c] + (SEA_EDGE[c] - SEA[c]) * k + n, 0, 255);
            p[3] = 255;
        }
    /* an inner shadow under the frame */
    int sh = fr / 2 + 1;
    for (int y = fr; y < h - fr; y++)
        for (int x = fr; x < w - fr; x++) {
            int e = x - fr;
            e = e < w - fr - 1 - x ? e : w - fr - 1 - x;
            e = e < y - fr ? e : y - fr;
            e = e < h - fr - 1 - y ? e : h - fr - 1 - y;
            if (e >= sh) continue;
            float k = 0.78f + 0.22f * e / sh;
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] * k);
        }
}

static void paint_disc(int size, uint8_t *o, float r_in)
{
    memset(o, 0, (size_t)size * size * 4);
    float c = (size - 1) / 2.0f, R = size / 2.0f - 0.5f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float d = sqrtf((x - c) * (x - c) + (y - c) * (y - c));
            float a = clampf(R - d + 0.5f, 0, 1);
            if (r_in > 0) a *= clampf(d - r_in * R + 0.5f, 0, 1);
            uint8_t *p = o + 4 * ((size_t)y * size + x);
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(a * 255);
        }
}

/* a small picture for the bar: 0 a place found (a green pin), 1 a miss (a red cross), 2 the clock, 3 the best (a gold
 * star) */
static void paint_small(int kind, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    if (kind == 0) {
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.38f, 0.38f);
        art_fig_paint(o, size, f, FOUND, INK, 0.07f, 0.4f);
    } else if (kind == 1) {
        art_band(f, ART_PART, 0.2f, 0.2f, 0.8f, 0.8f, 0.2f);
        art_band(f, ART_PART, 0.8f, 0.2f, 0.2f, 0.8f, 0.2f);
        art_fig_paint(o, size, f, RED, INK, 0.05f, 0.2f);
    } else if (kind == 2) {
        art_fig_free(f);
        art_icon(ICON_HOURGLASS, size, o);
        for (int i = 0; i < size * size; i++) o[4 * i] = CREAM[0], o[4 * i + 1] = CREAM[1], o[4 * i + 2] = CREAM[2];
        return;
    } else {
        art_star(f, ART_PART, 0.5f, 0.54f, 0.44f);
        art_fig_paint(o, size, f, GOLD, INK, 0.05f, 0.4f);
    }
}

/* the tools: a magnifier with a plus (0) or a minus (1); the whole map (2): four corners */
static void paint_tool(int kind, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    const uint8_t W[3] = {255, 255, 255};
    if (kind < 2) {
        art_arc(f, ART_PART, 0.42f, 0.42f, 0.22f, 0.3f, 0, 360);
        art_band(f, ART_PART, 0.62f, 0.62f, 0.86f, 0.86f, 0.12f);
        art_band(f, ART_PART, 0.3f, 0.42f, 0.54f, 0.42f, 0.07f);
        if (!kind) art_band(f, ART_PART, 0.42f, 0.3f, 0.42f, 0.54f, 0.07f);
    } else {
        const float c[4][2] = {{0.18f, 0.18f}, {0.82f, 0.18f}, {0.18f, 0.82f}, {0.82f, 0.82f}};
        for (int i = 0; i < 4; i++) {
            float sx = c[i][0] < 0.5f ? 1 : -1, sy = c[i][1] < 0.5f ? 1 : -1;
            art_band(f, ART_PART, c[i][0], c[i][1], c[i][0] + sx * 0.24f, c[i][1], 0.09f);
            art_band(f, ART_PART, c[i][0], c[i][1], c[i][0], c[i][1] + sy * 0.24f, 0.09f);
        }
        art_rect(f, ART_PART, 0.38f, 0.38f, 0.62f, 0.62f);
    }
    art_fig_paint(o, size, f, W, W, 0, 0);
}

/* ---- the colours of the places ----------------------------------------------------------------------------------- */
static void mix(uint8_t *o, const uint8_t *a, const uint8_t *b, float t)
{
    for (int c = 0; c < 3; c++) o[c] = (uint8_t)(a[c] + (b[c] - a[c]) * clampf(t, 0, 1) + 0.5f);
}

/* a place's colour now: its state, the effects on it */
static void colour_of(int r, Uint32 now, uint8_t o[3])
{
    const mq_reg *g = &M.reg[r];
    if (g->kind == MQ_DISPUTED) {
        memcpy(o, GREY, 3);
        return;
    }
    const uint8_t *base = TONES[g->tone & 7];
    uint8_t t[3];
    if (g->kind == MQ_OTHER || !Q.in[r]) {     /* land not asked: muted */
        mix(t, base, OTHER, g->kind == MQ_OTHER ? 0.85f : 0.62f);
        memcpy(o, t, 3);
    } else if (Q.state[r] == MQ_FOUND) {
        uint8_t f[3];
        mix(f, FOUND, base, 0.18f);
        if (r == found_r && now < found_t0 + FILL_MS) {
            float k = (float)(now - found_t0) / FILL_MS;
            uint8_t lit[3] = {196, 232, 170};
            mix(o, lit, f, k);
        } else memcpy(o, f, 3);
    } else if (Q.state[r] == MQ_MISSED) {
        mix(o, MISSED, base, 0.15f);
    } else memcpy(o, base, 3);
    if (r == flash_r && now < flash_t0 + FLASH_MS) {   /* a wrong one: red, fading */
        float k = 1 - (float)(now - flash_t0) / FLASH_MS;
        mix(t, o, RED, k * (0.6f + 0.4f * (k > 0.5f)));
        memcpy(o, t, 3);
    }
    if (r == reveal_r && now < reveal_t0 + REVEAL_MS) {   /* the answer: pulsing */
        float k = 0.5f + 0.5f * sinf((float)(now - reveal_t0) / 1000.0f * 2 * ART_PI * 2.2f);
        mix(t, o, GOLD, 0.25f + 0.6f * k);
        memcpy(o, t, 3);
    }
}

/* ---- the map loaded, its triangles in drawing order -------------------------------------------------------------- */
static void map_drop(void)
{
    if (loaded >= 0) mq_map_free(&M);
    loaded = -1;
    free(vx), free(fidx);
    vx = NULL, fidx = NULL;
    nfidx = nfidx_main = 0;
}

static int map_use(int map)
{
    if (loaded == map) return 1;
    map_drop();
    if (!mq_map_load(&M, map)) return 0;
    loaded = map;
    vx = calloc((size_t)(M.nrp > 0 ? M.nrp : 1), sizeof(SDL_Vertex));
    fidx = malloc(sizeof(int) * (size_t)(M.nfi > 0 ? M.nfi : 1));
    if (!vx || !fidx) {
        map_drop();
        return 0;
    }
    for (int pass = 0; pass < 2; pass++) {      /* the main map, then the insets (over their frames) */
        for (int o = 0; o < M.nreg; o++) {
            const mq_reg *g = &M.reg[M.order[o]];
            if (!!(g->flags & MQ_IN_INSET) != pass) continue;
            memcpy(fidx + nfidx, M.fi + g->i0, sizeof(int) * (size_t)g->ni);
            nfidx += g->ni;
        }
        if (!pass) nfidx_main = nfidx;
    }
    return 1;
}

/* the zoom's limits and the level's own view, for the window as it is now: the map always covers the window (its
 * edges never show: a wide map pans sideways in portrait); the places asked framed (their box and a margin, held in
 * the window, a little tighter), the land filling the window all round as the map covers it */
static void frame_home(void)
{
    if (loaded < 0 || view.w <= 0 || view.h <= 0) return;
    fit = fminf(view.w / M.w, view.h / M.h);
    zmin = fmaxf(view.w / M.w, view.h / M.h) / fit;
    zmax = fmaxf(zmin * 4, loaded < 6 ? 10 : 7);   /* the world and the continents: more */
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int pass = 0; pass < 2 && x1 < x0; pass++)   /* the main map's places first, the insets' if none */
        for (int i = 0; i < Q.n; i++) {
            const mq_reg *g = &M.reg[Q.ask[i]];
            if (!pass && (g->flags & MQ_IN_INSET)) continue;
            /* its biggest piece (Spain without the Canaries, the United States without Hawaii) */
            float best = -1, bx0 = g->lx, by0 = g->ly, bx1 = g->lx, by1 = g->ly;
            for (int j = 0; j < g->nring; j++) {
                const mq_ring *R = &M.ring[g->ring0 + j];
                const float *p = M.rp + 2 * R->first;
                float a = 0, rx0 = 1e9f, ry0 = 1e9f, rx1 = -1e9f, ry1 = -1e9f;
                for (int k = 0, l = R->count - 1; k < R->count; l = k++) {
                    a += p[2 * l] * p[2 * k + 1] - p[2 * k] * p[2 * l + 1];
                    rx0 = fminf(rx0, p[2 * k]), ry0 = fminf(ry0, p[2 * k + 1]);
                    rx1 = fmaxf(rx1, p[2 * k]), ry1 = fmaxf(ry1, p[2 * k + 1]);
                }
                if (R->outer && fabsf(a) > best) best = fabsf(a), bx0 = rx0, by0 = ry0, bx1 = rx1, by1 = ry1;
            }
            x0 = fminf(x0, bx0), y0 = fminf(y0, by0), x1 = fmaxf(x1, bx1), y1 = fmaxf(y1, by1);
        }
    if (x1 < x0) x0 = y0 = 0, x1 = (float)M.w, y1 = (float)M.h;
    float m = 0.06f * fmaxf(x1 - x0, y1 - y0) + 0.01f * fmaxf(M.w, M.h);
    x0 -= m, y0 -= m, x1 += m, y1 += m;
    /* the places held in the window (a little tighter: the margin may crop), never less than the map covering it */
    float sc = fmaxf(view.w / (x1 - x0), view.h / (y1 - y0)), sf = fminf(view.w / (x1 - x0), view.h / (y1 - y0));
    float s = fminf(sc, 1.15f * sf);
    hz = clampf(s / fit, zmin, zmax * 0.6f > zmin ? zmax * 0.6f : zmin);
    hx = (x0 + x1) / 2;
    hy = (y0 + y1) / 2;
    clamp_view(&hx, &hy, hz);
}

/* ---- starting ---------------------------------------------------------------------------------------------------- */
static void save_key(char *k, int size)
{
    if (ui.level >= 0) snprintf(k, (size_t)size, "mapquiz/L%d", ui.level);
    else snprintf(k, (size_t)size, "mapquiz/R%d", loaded);
}

static void calm(void)
{
    flash_r = label_r = reveal_r = found_r = -1;
    drag_on = 0;
    nfg = 0;
    pinching = 0;
}

static void layout(box b);

static void begin(int map, const char *codes)
{
    calm();
    if (!map_use(map)) return;
    int n = mq_set(&M, codes, set);
    mq_quiz_start(&Q, &M, set, n, (uint32_t)ui_random(0x7fffffff) + 1u);
    t_start = ui_play_ms();
    t_end = 0;
    new_best = 0;
    char k[32];
    save_key(k, sizeof k);
    uint8_t v[4] = {0};
    best_ms = ui_save_get(k, v, 4) == 4 ? (uint32_t)v[0] | (uint32_t)v[1] << 8 | (uint32_t)v[2] << 16 | (uint32_t)v[3] << 24
                                        : 0;
    prompt_key = -1;
    if (room.w > 0) layout(room);
    frame_home();
    need_home = view.w <= 0;
    view_set(hx, hy, hz);
}

static void start(int level)                    /* a designed level */
{
    const mq_level_def *L = mq_level(level);
    if (!L) L = mq_level(0);
    begin(L->map, L->codes);
}

static void start_random(int variant)           /* the die tab: a whole map */
{
    begin(variant >= 0 && variant < MQ_MAPS ? variant : 0, NULL);
}

static uint32_t elapsed(void)
{
    Uint32 e = t_end ? t_end : ui_play_ms();
    return e > t_start ? e - t_start : 0;
}

static void finished(void)
{
    t_end = ui_play_ms();
    if (t_end == t_start) t_end = t_start + 1;
    uint32_t ms = elapsed();
    if (!best_ms || ms < best_ms) {
        new_best = best_ms != 0;
        best_ms = ms;
        char k[32];
        save_key(k, sizeof k);
        uint8_t v[4] = {(uint8_t)ms, (uint8_t)(ms >> 8), (uint8_t)(ms >> 16), (uint8_t)(ms >> 24)};
        ui_save_set(k, v, 4);
    }
}

static int status(void) { return loaded >= 0 && mq_done(&Q) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return mq_stars(&Q, elapsed()); }
static int busy(void) { return found_r >= 0 && ui_ms() < found_t0 + FILL_MS; }

/* ---- a tap on the map -------------------------------------------------------------------------------------------- */
static float marker_r(void) { return fmaxf(9, 0.024f * fminf(ui.w, ui.h)); }

/* the regions shown as markers: asked in this quiz, too small to tap at this zoom */
static int has_marker(int r, float s)
{
    const mq_reg *g = &M.reg[r];
    if (!Q.in[r] || g->kind != MQ_ASK) return 0;
    float ext = fmaxf(g->x1 - g->x0, g->y1 - g->y0) * s;
    float rm = marker_r();
    return !g->nring || ext < 0.9f * rm || g->area * s * s < 0.3f * rm * rm;   /* a speck, or islands strewn wide */
}

static void show_label(int r, float x, float y)
{
    label_r = r;
    label_t0 = ui_ms();
    label_x = x;
    label_y = y;
}

/* the view moved so a place shows (its point off the window) */
static void bring(int r)
{
    float sx, sy, m = 0.12f * fminf(view.w, view.h);
    to_screen(M.reg[r].lx, M.reg[r].ly, &sx, &sy);
    if (sx >= view.x + m && sx <= view.x + view.w - m && sy >= view.y + m && sy <= view.y + view.h - m) return;
    view_go(M.reg[r].lx, M.reg[r].ly, tzoom);
}

static void tap_region(int r, float sx, float sy)
{
    if (loaded < 0 || mq_done(&Q)) return;
    int t = mq_target(&Q);
    int res = mq_tap(&Q, &M, r);
    Uint32 now = ui_ms();
    switch (res) {
    case MQ_RIGHT:
        found_r = r;
        found_t0 = now;
        flash_r = -1;
        ui_sound(SND_PLACE);
        break;
    case MQ_WRONG:
        flash_r = r;
        flash_t0 = now;
        show_label(r, sx, sy);
        ui_sound(SND_DENY);
        break;
    case MQ_GIVEN:
        flash_r = r;
        flash_t0 = now;
        show_label(r, sx, sy);
        reveal_r = t;
        reveal_t0 = now;
        bring(t);
        ui_sound(SND_DENY);
        break;
    case MQ_OUT:
        show_label(r, sx, sy);
        ui_sound(SND_TAP);
        break;
    default:
        if (r >= 0) ui_sound(SND_DENY);
        break;
    }
    if (mq_done(&Q) && !t_end) finished();
    ui_animate();
}

static void tap_at(float sx, float sy)
{
    if (sx < view.x || sy < view.y || sx > view.x + view.w || sy > view.y + view.h) return;
    float s = scale_of(zoom), rm = marker_r(), bd = 1e30f;
    int mk = -1;
    for (int i = 0; i < M.nreg; i++) {          /* the nearest marker */
        if (!has_marker(i, s)) continue;
        float mx, my;
        to_screen(M.reg[i].lx, M.reg[i].ly, &mx, &my);
        float d = (mx - sx) * (mx - sx) + (my - sy) * (my - sy);
        if (d < bd) bd = d, mk = i;
    }
    /* on a marker: it; on land: that place; at sea: a marker near enough, else the coast within a finger's slack */
    float gx, gy;
    to_grid(sx, sy, &gx, &gy);
    int r = mk >= 0 && bd <= (0.75f * rm) * (0.75f * rm) ? mk : mq_hit(&M, gx, gy, 0);
    if (r < 0 && mk >= 0 && bd <= (1.8f * rm) * (1.8f * rm)) r = mk;
    if (r < 0) r = mq_hit(&M, gx, gy, 0.012f * fminf(ui.w, ui.h) / s);
    tap_region(r, sx, sy);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static float slop(void) { return 0.014f * fminf(ui.w, ui.h); }

static void tool(int t)
{
    float mx = view.x + view.w / 2, my = view.y + view.h / 2;
    if (t == 0) zoom_at(mx, my, 1.8f, 1);
    else if (t == 1) zoom_at(mx, my, 1 / 1.8f, 1);
    else view_go(hx, hy, hz);
    ui_sound(SND_TAP);
}

static void press(float x, float y)
{
    cur.on = 0;
    int t = ui_tool_hit(x, y);
    if (t >= 0) {
        tool(t);
        return;
    }
    drag_on = 1;
    gesture_moved = 0;
    g_x0 = x, g_y0 = y, g_cx0 = cx, g_cy0 = cy;
}

static void motion(float x, float y)
{
    if (!drag_on) return;
    if (!gesture_moved && hypotf(x - g_x0, y - g_y0) > slop()) gesture_moved = 1;
    if (gesture_moved) {
        float s = scale_of(zoom);
        view_set(g_cx0 - (x - g_x0) / s, g_cy0 - (y - g_y0) / s, zoom);
    }
}

static void release(float x, float y)
{
    if (!drag_on) return;
    drag_on = 0;
    if (!gesture_moved) tap_at(x, y);
}

static void pinch_begin(void)
{
    g_d0 = fmaxf(1, hypotf(fg[1].x - fg[0].x, fg[1].y - fg[0].y));
    g_z0 = zoom;
    float mx = (fg[0].x + fg[1].x) / 2, my = (fg[0].y + fg[1].y) / 2;
    to_grid(mx, my, &g_mx, &g_my);
    pinching = 1;
    gesture_moved = 1;
}

static void finger(int id, int phase, float x, float y)
{
    int k = -1;
    for (int i = 0; i < nfg; i++)
        if (fg[i].id == id) k = i;
    if (phase == FINGER_DOWN) {
        cur.on = 0;
        if (!nfg) {
            int t = ui_tool_hit(x, y);
            if (t >= 0) {
                tool(t);
                return;
            }
        }
        if (k >= 0 || nfg >= MAX_FINGERS) return;
        fg[nfg].id = id, fg[nfg].x = x, fg[nfg].y = y;
        nfg++;
        if (nfg == 1) {
            gesture_moved = 0;
            pinching = 0;
            g_x0 = x, g_y0 = y, g_cx0 = cx, g_cy0 = cy;
        } else pinch_begin();
        return;
    }
    if (k < 0) return;
    fg[k].x = x, fg[k].y = y;
    if (phase == FINGER_MOVE) {
        if (nfg == 2) {
            float d = fmaxf(1, hypotf(fg[1].x - fg[0].x, fg[1].y - fg[0].y));
            float z = clampf(g_z0 * d / g_d0, zmin, zmax), s = scale_of(z);
            float mx = (fg[0].x + fg[1].x) / 2, my = (fg[0].y + fg[1].y) / 2;
            view_set(g_mx - (mx - view.x - view.w / 2) / s, g_my - (my - view.y - view.h / 2) / s, z);
        } else {
            if (!gesture_moved && hypotf(x - g_x0, y - g_y0) > slop()) gesture_moved = 1;
            if (gesture_moved) {
                float s = scale_of(zoom);
                view_set(g_cx0 - (x - g_x0) / s, g_cy0 - (y - g_y0) / s, zoom);
            }
        }
        return;
    }
    /* lifted */
    int single = nfg == 1 && !gesture_moved && !pinching;
    fg[k] = fg[--nfg];
    if (single) tap_at(x, y);
    else if (nfg == 1) {                        /* the other finger goes on dragging */
        g_x0 = fg[0].x, g_y0 = fg[0].y, g_cx0 = cx, g_cy0 = cy;
    }
}

static void key(SDL_Keycode k)
{
    if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN) {
        if (!cur.on) {
            cur.on = 1;
            cur.x = view.x + view.w / 2;
            cur.y = view.y + view.h / 2;
        }
        float st = 0.02f * fminf(view.w, view.h);
        cur.x += k == SDLK_LEFT ? -st : k == SDLK_RIGHT ? st : 0;
        cur.y += k == SDLK_UP ? -st : k == SDLK_DOWN ? st : 0;
        cur.t = ui_ms();
        ui_animate();
    } else if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_KP_ENTER) {
        if (cur.on) tap_at(cur.x, cur.y);
    } else if (k == SDLK_PLUS || k == SDLK_EQUALS || k == SDLK_KP_PLUS || k == SDLK_PAGEUP) {
        zoom_at(cur.on ? cur.x : view.x + view.w / 2, cur.on ? cur.y : view.y + view.h / 2, 1.8f, 1);
    } else if (k == SDLK_MINUS || k == SDLK_KP_MINUS || k == SDLK_PAGEDOWN) {
        zoom_at(cur.on ? cur.x : view.x + view.w / 2, cur.on ? cur.y : view.y + view.h / 2, 1 / 1.8f, 1);
    } else if (k == SDLK_HOME || k == SDLK_0) view_go(hx, hy, hz);
}

/* the cursor held: it glides, and at the window's edge the map follows */
static void cursor_step(void)
{
    if (!cur.on) return;
    const Uint8 *ks = SDL_GetKeyboardState(NULL);
    int dx = ks[SDL_SCANCODE_RIGHT] - ks[SDL_SCANCODE_LEFT], dy = ks[SDL_SCANCODE_DOWN] - ks[SDL_SCANCODE_UP];
    Uint32 now = ui_ms();
    float dt = fminf(0.05f, (now - cur.t) / 1000.0f);
    cur.t = now;
    if (!dx && !dy) return;
    float v = 0.45f * fminf(view.w, view.h) * dt;
    cur.x += dx * v;
    cur.y += dy * v;
    float m = 0.06f * fminf(view.w, view.h), s = scale_of(zoom);
    float px = cur.x < view.x + m ? cur.x - view.x - m : cur.x > view.x + view.w - m ? cur.x - view.x - view.w + m : 0;
    float py = cur.y < view.y + m ? cur.y - view.y - m : cur.y > view.y + view.h - m ? cur.y - view.y - view.h + m : 0;
    if (px || py) view_set(cx + px / s, cy + py / s, zoom);
    cur.x = clampf(cur.x, view.x + 2, view.x + view.w - 2);
    cur.y = clampf(cur.y, view.y + 2, view.y + view.h - 2);
    ui_animate();
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    room = b;
    float sm = fminf(b.w, b.h), bh = floorf(clampf(0.09f * sm, 30, 0.12f * b.h)), gap = floorf(0.012f * sm);
    banner = (box){b.x, b.y, b.w, bh};
    box panel = {b.x, b.y + bh + gap, b.w, b.h - bh - gap};
    int fr = (int)fmaxf(3, 0.016f * sm);
    view = (box){panel.x + fr, panel.y + fr, panel.w - 2 * fr, panel.h - 2 * fr};
    int w = (int)panel.w, h = (int)panel.h;
    if (w != panel_w || h != panel_h || !t_panel) {
        panel_w = w, panel_h = h;
        ui_free(&t_panel);
        if (w > 2 * fr && h > 2 * fr) {
            paint_panel(ui_buf(w, h), w, h, fr);
            t_panel = ui_tex(w, h, ui_buf(w, h));
        }
    }
    if (loaded >= 0) {
        frame_home();
        zoom = clampf(zoom, zmin, zmax);
        tzoom = clampf(tzoom, zmin, zmax);
        if (need_home && view.w > 0) {
            need_home = 0;
            view_set(hx, hy, hz);
        }
        clamp_view(&cx, &cy, zoom);
        clamp_view(&tcx, &tcy, tzoom);
    }
    int d = (int)fmaxf(8, 2 * marker_r());
    ui_free(&t_disc), ui_free(&t_ring);
    paint_disc(d, ui_buf(d, d), 0);
    t_disc = ui_tex(d, d, ui_buf(d, d));
    paint_disc(d, ui_buf(d, d), 0.74f);
    t_ring = ui_tex(d, d, ui_buf(d, d));
    int q = (int)fmaxf(12, 0.05f * sm);
    if (q != small_px) {
        small_px = q;
        SDL_Texture **t[4] = {&t_found, &t_miss, &t_clock, &t_best};
        for (int i = 0; i < 4; i++) {
            ui_free(t[i]);
            paint_small(i, q, ui_buf(q, q));
            *t[i] = ui_tex(q, q, ui_buf(q, q));
        }
    }
    prompt_key = -1;
    label_key = -1;
}

/* ---- the words: the prompt, a place's name ----------------------------------------------------------------------- */
static int name_of(int r, char *buf, int size)
{
    buf[0] = 0;
    if (r < 0 || M.reg[r].name < 0) return 0;
    if (!text_word("mapquiz_words", 1 + M.reg[r].name, buf, size) || !buf[0]) return 0;
    return 1;
}

/* text into a texture: pieces (text, bold, colour) in a row, fitted to maxw */
static SDL_Texture *line_tex(const char *const *s, const int *bold, const uint8_t (*rgb)[3], int n, float px, float maxw,
                             int *w, int *h)
{
    float tw = 0;
    for (int i = 0; i < n; i++) tw += text_line(s[i], px, bold[i], rgb[i], NULL, 0, 0, 0, 0);
    if (tw > maxw && tw > 0) px *= maxw / tw;
    tw = 0;
    int ws[4];
    for (int i = 0; i < n; i++) tw += ws[i] = text_line(s[i], px, bold[i], rgb[i], NULL, 0, 0, 0, 0);
    int W = (int)ceilf(tw) + 4, H = (int)ceilf(px * 1.5f) + 2;
    if (W < 2 || H < 2) return NULL;
    uint8_t *o = ui_buf(W, H);
    memset(o, 0, (size_t)W * H * 4);
    int x = 2;
    for (int i = 0; i < n; i++) {
        text_line(s[i], px, bold[i], rgb[i], o, W, H, x, (int)(0.12f * px));
        x += ws[i];
    }
    *w = W, *h = H;
    return ui_tex(W, H, o);
}

static void make_prompt(void)
{
    int t = mq_target(&Q), key = t * 7 + (int)banner.w * 131;
    if (key == prompt_key && t_prompt) return;
    prompt_key = key;
    ui_free(&t_prompt);
    if (t < 0) return;
    char tpl[128], nm[160], pre[128], post[128];
    if (!text_word("mapquiz_words", 0, tpl, sizeof tpl) || !strstr(tpl, "%s")) snprintf(tpl, sizeof tpl, "%%s");
    if (!name_of(t, nm, sizeof nm)) return;
    const char *p = strstr(tpl, "%s");
    snprintf(pre, sizeof pre, "%.*s", (int)(p - tpl), tpl);
    snprintf(post, sizeof post, "%s", p + 2);
    const char *s[3] = {pre, nm, post};
    const int bold[3] = {0, 1, 0};
    const uint8_t rgb[3][3] = {{250, 244, 230}, {236, 194, 96}, {250, 244, 230}};
    t_prompt = line_tex(s, bold, rgb, 3, 0.46f * banner.h, banner.w - 0.5f * banner.h, &prompt_w, &prompt_h);
}

static void make_label(void)
{
    int key = label_r;
    if (key == label_key && t_label) return;
    label_key = key;
    ui_free(&t_label);
    char nm[160];
    if (!name_of(label_r, nm, sizeof nm)) return;
    const char *s[1] = {nm};
    const int bold[1] = {1};
    const uint8_t rgb[1][3] = {{250, 244, 230}};
    t_label = line_tex(s, bold, rgb, 1, 0.032f * fminf(ui.w, ui.h) + 6, 0.8f * view.w, &label_w, &label_h);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void plate(float x, float y, float w, float h, Uint8 a)
{
    float r = fminf(h * 0.3f, 10);
    ui_fill(x + r, y, w - 2 * r, h, PLATE[0], PLATE[1], PLATE[2], a);
    ui_fill(x, y + r, r, h - 2 * r, PLATE[0], PLATE[1], PLATE[2], a);
    ui_fill(x + w - r, y + r, r, h - 2 * r, PLATE[0], PLATE[1], PLATE[2], a);
}

static int line_room(int n)
{
    if (n <= lcap) return 1;
    int c = n * 2;
    SDL_Vertex *v = realloc(lv, sizeof(SDL_Vertex) * (size_t)c * 4);
    if (!v) return 0;
    lv = v;
    int *ix = realloc(lidx, sizeof(int) * (size_t)c * 6);
    if (!ix) return 0;
    lidx = ix;
    lcap = c;
    return 1;
}

/* the lines of the arcs with their inset bit `inset` (coasts, borders), as thin quads */
static void draw_lines(int inset)
{
    float base = fmaxf(1.0f, 0.0013f * fminf(ui.w, ui.h)) * (0.85f + 0.15f * fminf(zoom, 3));
    float x0 = view.x - 4, y0 = view.y - 4, x1 = view.x + view.w + 4, y1 = view.y + view.h + 4;
    int nq = 0;
    for (int a = 0; a < M.narc; a++) {
        int k = M.ak[a];
        if (!!(k & MQ_ARC_INSET) != inset || (k & 3) == MQ_EDGE) continue;
        float bx0, by0, bx1, by1;
        to_screen(M.ab[4 * a], M.ab[4 * a + 1], &bx0, &by0);
        to_screen(M.ab[4 * a + 2], M.ab[4 * a + 3], &bx1, &by1);
        if (bx1 < x0 || bx0 > x1 || by1 < y0 || by0 > y1) continue;
        int kind = k & 3, n = M.as[a + 1] - M.as[a];
        float wd = kind == MQ_COAST ? base * 1.15f : kind == MQ_BORDER ? base * 1.25f : base * 0.8f;
        Uint8 al = kind == MQ_COAST ? 150 : kind == MQ_BORDER ? 215 : 120;
        const uint8_t *ink = kind == MQ_COAST ? (const uint8_t[3]){74, 92, 104} : INK;
        if (!line_room(nq + n)) return;
        float px = 0, py = 0;
        for (int i = 0; i < n; i++) {
            float sx, sy;
            to_screen(M.ap[2 * (M.as[a] + i)], M.ap[2 * (M.as[a] + i) + 1], &sx, &sy);
            if (i > 0) {
                float dx = sx - px, dy = sy - py, L = sqrtf(dx * dx + dy * dy);
                if (L < 0.01f) continue;
                float nx = -dy / L * wd / 2, ny = dx / L * wd / 2;
                /* run a little past each end: the joins close */
                float ex = dx / L * wd * 0.35f, ey = dy / L * wd * 0.35f;
                SDL_Vertex *q = lv + 4 * nq;
                SDL_Color c = {ink[0], ink[1], ink[2], al};
                q[0] = (SDL_Vertex){{px - ex + nx, py - ey + ny}, c, {0, 0}};
                q[1] = (SDL_Vertex){{px - ex - nx, py - ey - ny}, c, {0, 0}};
                q[2] = (SDL_Vertex){{sx + ex - nx, sy + ey - ny}, c, {0, 0}};
                q[3] = (SDL_Vertex){{sx + ex + nx, sy + ey + ny}, c, {0, 0}};
                int *ix = lidx + 6 * nq, b = 4 * nq;
                ix[0] = b, ix[1] = b + 1, ix[2] = b + 2, ix[3] = b, ix[4] = b + 2, ix[5] = b + 3;
                nq++;
            }
            px = sx, py = sy;
        }
    }
    if (nq) SDL_RenderGeometry(ui.r, NULL, lv, 4 * nq, lidx, 6 * nq);
}

static void draw_map(Uint32 now)
{
    float s = scale_of(zoom);
    SDL_Rect clip = {(int)view.x, (int)view.y, (int)view.w, (int)view.h};
    SDL_RenderSetClipRect(ui.r, &clip);
    /* each ring's points on the screen, in its place's colour */
    for (int r = 0; r < M.nreg; r++) {
        const mq_reg *g = &M.reg[r];
        uint8_t c[3];
        colour_of(r, now, c);
        SDL_Color col = {c[0], c[1], c[2], 255};
        for (int j = 0; j < g->nring; j++) {
            const mq_ring *R = &M.ring[g->ring0 + j];
            for (int i = R->first; i < R->first + R->count; i++) {
                float sx, sy;
                to_screen(M.rp[2 * i], M.rp[2 * i + 1], &sx, &sy);
                vx[i] = (SDL_Vertex){{sx, sy}, col, {0, 0}};
            }
        }
    }
    SDL_RenderGeometry(ui.r, NULL, vx, M.nrp, fidx, nfidx_main);
    draw_lines(0);
    /* the insets: their frames over the main map, then their places */
    const mq_map_def *d = mq_def(loaded);
    for (int f = 0; d && f < d->nframes; f++) {
        float ax, ay, bx, by;
        to_screen(d->frames[4 * f], d->frames[4 * f + 1], &ax, &ay);
        to_screen(d->frames[4 * f + 2], d->frames[4 * f + 3], &bx, &by);
        ui_fill(ax, ay, bx - ax, by - ay, SEA[0], SEA[1], SEA[2], 255);
        float t = fmaxf(1, 0.0016f * fminf(ui.w, ui.h));
        ui_fill(ax, ay, bx - ax, t, INK[0], INK[1], INK[2], 170);
        ui_fill(ax, by - t, bx - ax, t, INK[0], INK[1], INK[2], 170);
        ui_fill(ax, ay, t, by - ay, INK[0], INK[1], INK[2], 170);
        ui_fill(bx - t, ay, t, by - ay, INK[0], INK[1], INK[2], 170);
    }
    if (nfidx > nfidx_main) {
        SDL_RenderGeometry(ui.r, NULL, vx, M.nrp, fidx + nfidx_main, nfidx - nfidx_main);
        draw_lines(1);
    }
    /* the markers of the small places asked */
    float rm = marker_r();
    for (int o = 0; o < M.nreg; o++) {
        int r = M.order[o];
        if (!has_marker(r, s)) continue;
        float mx, my;
        to_screen(M.reg[r].lx, M.reg[r].ly, &mx, &my);
        if (mx < view.x - rm || my < view.y - rm || mx > view.x + view.w + rm || my > view.y + view.h + rm) continue;
        uint8_t c[3];
        colour_of(r, now, c);
        float d0 = 0.95f * rm;
        SDL_SetTextureColorMod(t_disc, c[0], c[1], c[2]);
        ui_put_c(t_disc, mx, my, d0, d0);
        SDL_SetTextureColorMod(t_ring, INK[0], INK[1], INK[2]);
        ui_put_c(t_ring, mx, my, d0 + 2, d0 + 2);
        if (r == reveal_r && now < reveal_t0 + REVEAL_MS) ui_mark_gold_a(mx, my, 2.6f * rm, 0.9f);
    }
    SDL_SetTextureColorMod(t_disc, 255, 255, 255);
    SDL_SetTextureColorMod(t_ring, 255, 255, 255);
    /* the answer shown: a gold light on it */
    if (reveal_r >= 0 && now < reveal_t0 + REVEAL_MS) {
        float mx, my;
        to_screen(M.reg[reveal_r].lx, M.reg[reveal_r].ly, &mx, &my);
        float k = 0.5f + 0.5f * sinf((float)(now - reveal_t0) / 1000.0f * 2 * ART_PI * 2.2f);
        ui_mark_gold_a(mx, my, 3.2f * rm, 0.35f + 0.5f * k);
    }
    /* the name of a place tapped wrongly */
    if (label_r >= 0 && now < label_t0 + LABEL_MS) {
        make_label();
        if (t_label) {
            float a = clampf((float)(label_t0 + LABEL_MS - now) / 300.0f, 0, 1);
            float lw = (float)label_w, lh = (float)label_h, pad = 0.35f * lh;
            float x = clampf(label_x - lw / 2, view.x + pad, view.x + view.w - lw - pad);
            float y = label_y - 2.2f * lh;
            if (y < view.y + pad) y = label_y + 1.2f * lh;
            plate(x - pad, y - pad * 0.4f, lw + 2 * pad, lh + pad * 0.8f, (Uint8)(225 * a));
            SDL_SetTextureAlphaMod(t_label, (Uint8)(255 * a));
            ui_put(t_label, x, y, lw, lh);
            SDL_SetTextureAlphaMod(t_label, 255);
        }
    } else label_r = -1;
    if (cur.on) {                               /* the keys' cursor */
        float r = 0.026f * fminf(ui.w, ui.h);
        ui_mark_select_round(cur.x, cur.y, 2 * r);
        ui_fill(cur.x - r, cur.y - 1, 2 * r, 2, INK[0], INK[1], INK[2], 230);
        ui_fill(cur.x - 1, cur.y - r, 2, 2 * r, INK[0], INK[1], INK[2], 230);
    }
    SDL_RenderSetClipRect(ui.r, NULL);
}

/* mm:ss in digits, a colon of two dots; its width */
static float draw_time(float x, float cy, float h, uint32_t ms, const uint8_t rgb[3], int draw)
{
    int sec = (int)(ms / 1000), m = sec / 60, s = sec % 60;
    float wm = ui_number_w(m, h), w2 = ui_number_w(10, h), dot = fmaxf(2, 0.13f * h), gap = 0.18f * h;
    float total = wm + gap * 2 + dot + w2;
    if (draw) {
        ui_number_ex(x + wm / 2, cy, h, m, rgb, 255, UI_NUM_TIGHT);
        float dx = x + wm + gap;
        ui_fill(dx, cy - 0.22f * h, dot, dot, rgb[0], rgb[1], rgb[2], 255);
        ui_fill(dx, cy + 0.12f * h, dot, dot, rgb[0], rgb[1], rgb[2], 255);
        float sx = dx + dot + gap;
        ui_number_ex(sx + ui_number_w(s / 10, h) / 2, cy, h, s / 10, rgb, 255, UI_NUM_TIGHT);
        ui_number_ex(sx + w2 - ui_number_w(s % 10, h) / 2, cy, h, s % 10, rgb, 255, UI_NUM_TIGHT);
    }
    return total;
}

static void draw_banner(Uint32 now)
{
    plate(banner.x, banner.y, banner.w, banner.h, 235);
    float t = fmaxf(1, 0.04f * banner.h);
    ui_fill(banner.x + 0.3f * banner.h, banner.y + banner.h - 2 * t, banner.w - 0.6f * banner.h, t, GOLD[0], GOLD[1],
            GOLD[2], 200);
    float cyb = banner.y + banner.h * 0.47f;
    if (!mq_done(&Q)) {
        make_prompt();
        if (t_prompt) ui_put(t_prompt, banner.x + (banner.w - prompt_w) / 2, cyb - prompt_h / 2.0f, (float)prompt_w,
                             (float)prompt_h);
        return;
    }
    /* done: the time, and the best kept (in gold when it is new) */
    (void)now;
    float h = 0.5f * banner.h, q = 0.62f * banner.h, g = 0.25f * banner.h;
    float w1 = draw_time(0, 0, h, elapsed(), CREAM, 0), w2 = draw_time(0, 0, h, best_ms, GOLD, 0);
    float total = q + g * 0.6f + w1 + 2 * g + q + g * 0.6f + w2, x = banner.x + (banner.w - total) / 2;
    ui_put_c(t_clock, x + q / 2, cyb, q, q);
    x += q + g * 0.6f;
    x += draw_time(x, cyb, h, elapsed(), CREAM, 1) + 2 * g;
    ui_put_c(t_best, x + q / 2, cyb, q, q);
    x += q + g * 0.6f;
    draw_time(x, cyb, h, best_ms, new_best ? GOLD : CREAM, 1);
}

static void draw(void)
{
    if (loaded < 0) return;
    if (!t_panel || fit <= 0) layout(room);
    Uint32 now = ui_ms();
    view_step();
    cursor_step();
    if (t_panel) ui_put(t_panel, view.x - (panel_w - view.w) / 2, view.y - (panel_h - view.h) / 2, (float)panel_w,
                        (float)panel_h);
    draw_map(now);
    draw_banner(now);
    if ((flash_r >= 0 && now < flash_t0 + FLASH_MS) || (label_r >= 0 && now < label_t0 + LABEL_MS) ||
        (reveal_r >= 0 && now < reveal_t0 + REVEAL_MS) || (found_r >= 0 && now < found_t0 + FILL_MS))
        ui_animate();
    ui_tool_paint(0, paint_tool, 0, 0.62f, UI_TINT | (tzoom >= zmax - 0.01f ? UI_DIM : 0));
    ui_tool_paint(1, paint_tool, 1, 0.62f, UI_TINT | (tzoom <= zmin * 1.01f ? UI_DIM : 0));
    int home = fabsf(tzoom - hz) < 0.01f * hz && fabsf(tcx - hx) < 1 && fabsf(tcy - hy) < 1;
    ui_tool_paint(2, paint_tool, 2, 0.62f, UI_TINT | (home ? UI_DIM : 0));
}

/* the bar: the places found, the misses; the best time kept for this quiz (a star, the time) */
static void bar(int player, box b)
{
    (void)player;
    if (loaded < 0) return;
    int found = 0;
    for (int i = 0; i < Q.n; i++) found += Q.state[Q.ask[i]] == MQ_FOUND;
    ui_count c[2] = {{t_found, found}, {t_miss, Q.misses}};
    int best = best_ms > 0 && !mq_done(&Q);
    float d = ui_bar_fit(b, c, 2, best ? 3 : 0), ax, ay, bx, by;
    if (!best) return;
    ui_bar_spot(b, 2, d, &ax, &ay);
    ui_bar_spot(b, 0, d, &bx, &by);
    float h = 0.48f * d, q = 0.62f * d, w = draw_time(0, 0, h, best_ms, CREAM, 0), x = (ax + bx) / 2 - (q + 0.2f * d + w) / 2;
    ui_put_c(t_best, x + q / 2, ay, q, q);
    draw_time(x + q + 0.2f * d, ay, h, best_ms, CREAM, 1);
}

/* ---- the moves in words (the tests, the pictures) ----------------------------------------------------------------
 * a: the place asked, found; w: a wrong place (the first other one asked); a code: that place tapped; @code: the view
 * zoomed onto that place; z+, z-: zoom in, out */
static int script(const char *moves)
{
    char tok[40];
    const char *s = moves;
    int ok = 1;
    while (*s && ok) {
        while (*s == ' ') s++;
        int n = 0;
        while (*s && *s != ' ' && n < 39) tok[n++] = *s++;
        tok[n] = 0;
        if (!n) break;
        int t = mq_target(&Q), r = -1;
        float sx = view.x + view.w / 2, sy = view.y + view.h / 2;
        if (!strcmp(tok, "a")) r = t;
        else if (!strcmp(tok, "w")) {
            for (int i = 0; i < Q.n && r < 0; i++)
                if (Q.ask[i] != t) r = Q.ask[i];
        } else if (!strcmp(tok, "z+")) {
            zoom_at(sx, sy, 1.8f, 0);
            continue;
        } else if (!strcmp(tok, "z-")) {
            zoom_at(sx, sy, 1 / 1.8f, 0);
            continue;
        } else if (tok[0] == '@') {
            int k = mq_find(&M, tok + 1);
            if (k < 0) {
                ok = 0;
                break;
            }
            view_set(M.reg[k].lx, M.reg[k].ly, fminf(zmax, 3.5f));
            continue;
        } else r = mq_find(&M, tok);
        if (r < 0 || t < 0) {
            ok = 0;
            break;
        }
        to_screen(M.reg[r].lx, M.reg[r].ly, &sx, &sy);
        tap_region(r, sx, sy);
    }
    found_r = -1;                               /* at rest: no fill still turning */
    return ok;
}

/* ---- the pictures for the shell ---------------------------------------------------------------------------------- */
typedef struct { int highlight, only, ink; } pic_ctx;   /* only: the map's set alone; ink: all in ink (an outline) */

static int code_in(const char *list, const char *code)   /* a code in a list of codes between spaces */
{
    size_t n = strlen(code);
    for (const char *f = list; (f = strstr(f, code)) != NULL; f += n)
        if ((f == list || f[-1] == ' ') && (f[n] == 0 || f[n] == ' ')) return 1;
    return 0;
}

static int in_set(const mq_map *m, int r)       /* the map's set: its continent's countries, or its states */
{
    const char *s = mq_map_set(m->map);
    return m->reg[r].kind == MQ_ASK && (!strcmp(s, "*") || code_in(s, m->reg[r].code));
}

static void pic_colour(const mq_map *m, int r, void *ctx, uint8_t c[4])
{
    const pic_ctx *p = ctx;
    const mq_reg *g = &m->reg[r];
    int in = in_set(m, r);
    c[3] = p->only && !in ? 0 : 255;
    if (p->ink) memcpy(c, INK, 3);
    else if (g->kind == MQ_DISPUTED) memcpy(c, GREY, 3);
    else if (!in) mix(c, TONES[g->tone & 7], OTHER, 0.7f);
    else if (p->highlight && code_in("ESP DEU EGY NGA SAU SWE TUR KEN ITA POL DZA", g->code))
        mix(c, FOUND, TONES[g->tone & 7], 0.18f);
    else memcpy(c, TONES[g->tone & 7], 3);
}

/* a map in a rounded tile of sea, size x size; highlight: a few places found and a red pin (the menu's card) */
static void paint_map(int map, int size, uint8_t *o, int highlight)
{
    memset(o, 0, (size_t)size * size * 4);
    mq_map *m = calloc(1, sizeof *m);
    if (!m) return;
    if (!mq_map_load_ex(m, map, 0)) {
        free(m);
        return;
    }
    pic_ctx c = {highlight, 0, 0};
    if (highlight) {                            /* the menu's card: the Old World filling a tile, a pin on France */
        int fr = mq_find(m, "FRA");
        float s = 0.92f * size / m->h * 1.25f, inset = 0.05f * size;
        float ox = size / 2.0f - (fr >= 0 ? m->reg[fr].lx + 0.05f * m->w : m->w / 2.0f) * s;
        float oy = size / 2.0f - (fr >= 0 ? m->reg[fr].ly + 0.06f * m->h : m->h / 2.0f) * s;
        art_roundrect_put(o, size, size, (int)inset, (int)inset, (int)(size - 2 * inset), (int)(size - 2 * inset),
                          0.1f * size, SEA, 1);
        int ti = (int)inset, tw = size - 2 * ti;
        uint8_t *t = calloc((size_t)size * size, 4), *mask = calloc((size_t)(tw > 0 ? tw : 1) * (tw > 0 ? tw : 1), 4);
        if (t && mask && tw > 0) {             /* the land kept inside the tile's rounded corners */
            mq_paint(m, s, ox, oy, size, size, t, pic_colour, &c);
            art_roundrect(tw, tw, 0.1f * size, (const uint8_t[4]){255, 255, 255, 255}, mask);
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++) {
                    int mx = x - ti, my = y - ti;
                    uint8_t *p = t + 4 * ((size_t)y * size + x);
                    p[3] = mx < 0 || my < 0 || mx >= tw || my >= tw ? 0
                                                                   : (uint8_t)(p[3] * mask[4 * ((size_t)my * tw + mx) + 3] / 255);
                }
            art_blend(o, size, size, t, size, size, 0, 0);
        }
        free(t);
        free(mask);
        if (fr >= 0) {
            float px = (ox + m->reg[fr].lx * s) / size, py = (oy + m->reg[fr].ly * s) / size;
            art_fig *f = art_fig_new();
            art_band(f, ART_PART, px, py, px, py - 0.12f, 0.022f);
            art_ellipse(f, ART_PART, px, py - 0.14f, 0.05f, 0.05f);
            art_fig_paint(o, size, f, RED, INK, 0.01f, 0.5f);
        }
    } else {
        float pad = 0.06f * size, s = fminf((size - 2 * pad) / m->w, (size - 2 * pad) / m->h);
        float w = m->w * s + 2 * pad * 0.6f, h = m->h * s + 2 * pad * 0.6f, x0 = (size - w) / 2, y0 = (size - h) / 2;
        art_roundrect_put(o, size, size, (int)x0, (int)y0, (int)w, (int)h, 0.08f * size, SEA, 1);
        mq_paint(m, s, x0 + pad * 0.6f, y0 + pad * 0.6f, size, size, o, pic_colour, &c);
    }
    mq_map_free(m);
    free(m);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    if (variant < 0) paint_map(0, size, rgba, 1);
    else paint_map(variant < MQ_MAPS ? variant : 0, size, rgba, 0);
}

/* a chapter's tab: its map's set alone (the continent, the country), parchment in an ink outline */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    mq_map *m = calloc(1, sizeof *m);
    if (!m) return;
    if (!mq_map_load_ex(m, mq_chapter_map(chapter), 0)) {
        free(m);
        return;
    }
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int r = 0; r < m->nreg; r++)
        if (in_set(m, r) && m->reg[r].nring && !(m->reg[r].flags & MQ_IN_INSET)) {
            x0 = fminf(x0, m->reg[r].x0), y0 = fminf(y0, m->reg[r].y0);
            x1 = fmaxf(x1, m->reg[r].x1), y1 = fmaxf(y1, m->reg[r].y1);
        }
    if (x1 > x0 && y1 > y0) {
        float s = 0.86f * size / fmaxf(x1 - x0, y1 - y0), ox = size / 2.0f - (x0 + x1) / 2 * s;
        float oy = size / 2.0f - (y0 + y1) / 2 * s, d = fmaxf(1, 0.022f * size);
        pic_ctx ink = {0, 1, 1}, fill = {0, 1, 0};
        for (int k = 0; k < 4; k++)             /* the outline: the land in ink, a little off each way */
            mq_paint(m, s, ox + (k & 1 ? d : -d), oy + (k & 2 ? d : -d), size, size, rgba, pic_colour, &ink);
        mq_paint(m, s, ox, oy, size, size, rgba, pic_colour, &fill);
    }
    mq_map_free(m);
    free(m);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    memset(rgba, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.44f, 0.44f);
    art_fig_paint(rgba, size, f, SEA, INK, 0.05f, 0.4f);
    f = art_fig_new();                          /* a pin on the sea */
    art_band(f, ART_PART, 0.5f, 0.78f, 0.5f, 0.42f, 0.07f);
    art_ellipse(f, ART_PART, 0.5f, 0.36f, 0.16f, 0.16f);
    art_fig_paint(rgba, size, f, RED, INK, 0.04f, 0.5f);
}

static const game mapquiz_game = {
    .id = "mapquiz", .order = 396, .variants = MQ_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key, .finger = finger,
    .levels = MQ_LEVELS, .chapters = MQ_CHAPTERS, .chapter_of = mq_chapter_of, .start_random = start_random,
    .chapter_icon = chapter_icon, .level_stars = level_stars, .levels_open = 5,
    .fill = 1, .tools = 3,
};
REGISTER_GAME(mapquiz_game)
