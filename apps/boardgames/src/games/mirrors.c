/*
 * Mirrors in RetroStone Board, for one player: the house plank ruled in cells, dark wooden blocks inlaid in it, a
 * brass lamp, gems, ebony bombs, polished brass mirrors (on a walnut pivot, or loose from the tray under the board)
 * and the beam of light, gold and red, traced again at every change (the rules: mirrors_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The shell's level screen chooses a level (150 in six chapters, mirrors_levels.h), or its die tab a puzzle made at
 * random (the variants). Stars: the moves against par (the fewest actions), a hint caps them (pzkit.h).
 * Tap a pivoting mirror to turn it. Tap an empty cell to stand a loose mirror there ('/'), again to turn it ('\'),
 * again to take it back to the tray; or drag one from the tray to a cell, from a cell to another, or off the board.
 * A lit gem glows; a bomb touched by the beam glows red. The board takes the room the shell gives (game.fill); the
 * shell's tool: the hint (one mirror set right, in gold light). The bar: the moves. Undo takes back one action, all
 * the way to the start.
 * Solved: a gold light runs along the beam from the lamp, and the gems flash.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/mirrors.h"
#include "games/mirrors_levels.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "shell.h"

#define FRAME 0.035f
#define TURN_MS 170                             /* a mirror turning a quarter round */
#define HINT_MS 1400
#define WAVE_STEP 45                            /* the win's light along the beam: ms a cell further on */
#define WAVE_LEN 520
#define FLASH_MS 700

static const uint8_t BRASS[3] = {214, 172, 84}, BLOCK[3] = {74, 50, 36};

static mr_game G;
static mr_beam beam;
static int serial;                              /* the puzzle's number: its board remade */
static struct { int on, cell, from, moved; float x, y, x0, y0; } ptr;   /* from: -1 the tray */
static struct { float deg0; Uint32 t0; } turn[MR_CELLS];
static struct { int cell; Uint32 t0; } hint = {-1, 0};
static struct { int on; Uint32 t0; } win;

static box last_box;
static float bx, by, side, gx, gy, tray_y, tray_h;
static int fr, cs, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_plate, *t_glass, *t_base, *t_rivet, *t_lamp, *t_gem[2], *t_bomb, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a plate lying along x in a w x h picture: polished brass (the light along its upper edge), or glass */
static void paint_plate(uint8_t *o, int w, int h, int glass)
{
    float rad = h * 0.42f;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float dx = fmaxf(rad - px, px - (w - rad)), dy = fmaxf(rad - py, py - (h - rad)), d;
            if (dx > 0 && dy > 0) d = rad - hypotf(dx, dy);
            else d = fminf(fminf(px, w - px), fminf(py, h - py));
            float cov = clampf(d + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float t = py / h, k;                /* across the plate: dark, the bright line, gold, darker */
            uint8_t c[3];
            if (glass) {
                k = 0.85f + 0.3f * expf(-powf((t - 0.32f) / 0.12f, 2));
                const uint8_t g[3] = {196, 226, 236};
                for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(g[i] * k, 0, 255);
                float edge = d < 1.6f ? 1 : 0.62f;
                pzk_over(o + 4 * (y * w + x), c, cov * edge);
                continue;
            }
            k = 0.62f + 0.75f * expf(-powf((t - 0.3f) / 0.13f, 2)) + 0.25f * (1 - t) - 0.1f * fabsf(px / w - 0.5f);
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(BRASS[i] * k + (k > 1.2f ? 40 * (k - 1.2f) : 0), 0, 255);
            if (d < fmaxf(1, h * 0.08f))         /* its edge */
                for (int i = 0; i < 3; i++) c[i] = (uint8_t)(c[i] * 0.55f);
            pzk_over(o + 4 * (y * w + x), c, cov);
        }
    if (glass) {                                /* two small screws at its ends */
        pzk_ball(o, w, h, h * 0.55f, h * 0.5f, h * 0.2f, PZK_DARK, 1, 0.5f);
        pzk_ball(o, w, h, w - h * 0.55f, h * 0.5f, h * 0.2f, PZK_DARK, 1, 0.5f);
    }
}

/* the lamp, facing east: a walnut block, a brass barrel, its lens */
static void paint_lamp(uint8_t *o, int s)
{
    float S = (float)s;
    uint8_t *b = calloc((size_t)s * s, 4);
    if (!b) return;
    int q = (int)(0.78f * S), m = (s - q) / 2;
    uint8_t *blk = calloc((size_t)q * q, 4);
    if (blk) {
        art_board_n(1, 1, q, 0, 0, blk);
        for (int i = 0; i < q * q; i++)
            for (int k = 0; k < 3; k++) blk[4 * i + k] = (uint8_t)(blk[4 * i + k] * 0.55f);
        puz_bevel(blk, q, q, 0.2f * q, fmaxf(2, 0.08f * q), 0.4f);
        art_blend(o, s, s, blk, q, q, m, m);
        free(blk);
    }
    for (int y = 0; y < s; y++)                 /* the barrel: a cylinder lying east-west */
        for (int x = 0; x < s; x++) {
            float px = (x + 0.5f) / S, py = (y + 0.5f) / S;
            if (px < 0.2f || px > 0.8f || fabsf(py - 0.5f) > 0.17f) continue;
            float t = (py - 0.33f) / 0.34f, k = 0.55f + 0.8f * expf(-powf((t - 0.3f) / 0.18f, 2)) + 0.2f * (1 - t);
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(BRASS[i] * k, 0, 255);
            float cov = clampf((0.17f - fabsf(py - 0.5f)) * S + 0.5f, 0, 1) * clampf((px - 0.2f) * S + 0.5f, 0, 1);
            pzk_over(o + 4 * (y * s + x), c, cov);
        }
    pzk_stroke(o, s, s, 0.36f * S, 0.32f * S, 0.36f * S, 0.68f * S, 0.035f * S, PZK_DARK, 0.7f);   /* its rings */
    pzk_stroke(o, s, s, 0.62f * S, 0.32f * S, 0.62f * S, 0.68f * S, 0.035f * S, PZK_DARK, 0.7f);
    const uint8_t lens[3] = {255, 120, 60}, hot[3] = {255, 236, 190};
    pzk_ball(o, s, s, 0.8f * S, 0.5f * S, 0.15f * S, BRASS, 1, 0.5f);
    pzk_ball(o, s, s, 0.8f * S, 0.5f * S, 0.11f * S, lens, 1, 0.3f);
    pzk_ball(o, s, s, 0.82f * S, 0.48f * S, 0.05f * S, hot, 1, 0);
    free(b);
}

/* a gem seen from above: an octagon cut in facets; lit, it burns gold */
static void paint_gem(uint8_t *o, int s, int lit)
{
    const uint8_t dull[3] = {84, 128, 160}, gold[3] = {255, 206, 104};
    const uint8_t *base = lit ? gold : dull;
    float c = s / 2.0f, R = 0.4f * s;
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            float dx = x + 0.5f - c, dy = y + 0.5f - c, a = atan2f(dy, dx), r = hypotf(dx, dy);
            float sector = floorf((a + PUZ_PI) / (PUZ_PI / 4)), mid = -PUZ_PI + (sector + 0.5f) * PUZ_PI / 4;
            float edge = R * cosf(PUZ_PI / 8) / cosf(a - mid);   /* the octagon's rim */
            float cov = clampf(edge - r + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float k = r < 0.5f * edge ? 1.15f : ((int)sector % 2 ? 0.95f : 0.78f) + 0.25f * (-dx - dy) / (2 * R);
            if (lit) k += 0.35f * clampf(1 - r / R, 0, 1);
            uint8_t col[3];
            for (int i = 0; i < 3; i++) col[i] = (uint8_t)clampf(base[i] * k, 0, 255);
            if (edge - r < fmaxf(1, 0.05f * s) || fabsf(r - 0.5f * edge) < fmaxf(0.5f, 0.012f * s))
                for (int i = 0; i < 3; i++) col[i] = (uint8_t)(col[i] * 0.6f);
            pzk_over(o + 4 * (y * s + x), col, cov);
        }
    const uint8_t white[3] = {255, 255, 255};
    pzk_ball(o, s, s, c - 0.12f * s, c - 0.13f * s, 0.05f * s, white, lit ? 0.95f : 0.6f, 0);
}

/* a bomb: an ebony ball, a red lacquer band, a brass cap and its fuse */
static void paint_bomb(uint8_t *o, int s)
{
    float S = (float)s;
    const uint8_t ebony[3] = {44, 38, 36};
    pzk_ball(o, s, s, 0.5f * S, 0.54f * S, 0.3f * S, ebony, 1, 0.9f);
    pzk_arc(o, s, s, 0.5f * S, 0.54f * S, 0.3f * S, 0.36f * S, 0, 360, PZK_RED, 0.95f);
    pzk_stroke(o, s, s, 0.66f * S, 0.32f * S, 0.74f * S, 0.24f * S, 0.06f * S, BRASS, 1);
    pzk_stroke(o, s, s, 0.74f * S, 0.24f * S, 0.8f * S, 0.14f * S, 0.022f * S, PUZ_CREAM, 1);
    const uint8_t white[3] = {255, 255, 255};
    pzk_ball(o, s, s, 0.4f * S, 0.43f * S, 0.06f * S, white, 0.5f, 0);
}

/* the board: the plank, the cells ruled, the blocks raised in dark wood, the tray's groove */
static void paint_board(uint8_t *o, int S, int fr, const mr_game *g, float ox, float oy, int q, float ty, float th)
{
    int n = g->n;
    art_board_n(1, 1, S - 2 * fr, fr, 0, o);
    float gw = (float)n * q;
    for (int i = 0; i <= n; i++) {
        float t = i == 0 || i == n ? fmaxf(2, 0.05f * q) : fmaxf(1, 0.022f * q), a = i == 0 || i == n ? 0.8f : 0.4f;
        float p = i * q, lo = p - t / 2, hi = p + t / 2;
        if (i == 0) lo = 0, hi = t;
        if (i == n) lo = gw - t, hi = gw;
        puz_ink_rect(o, S, S, ox + lo, oy, ox + hi, oy + gw, PUZ_INK, a);
        puz_ink_rect(o, S, S, ox, oy + lo, ox + gw, oy + hi, PUZ_INK, a);
    }
    for (int c = 0; c < n * n; c++) {
        if (g->kind[c] != MR_WALL) continue;
        float x0 = ox + c % n * q + 0.06f * q, y0 = oy + c / n * q + 0.06f * q, w = 0.88f * q, b = fmaxf(2, 0.1f * q);
        for (int y = (int)y0; y < (int)(y0 + w); y++)
            for (int x = (int)x0; x < (int)(x0 + w); x++) {
                uint8_t *p = o + 4 * (y * S + x);
                float ex = x + 0.5f - x0, ey = y + 0.5f - y0, k = 0.5f;
                if (ex < b && ex < ey && ex < w - ey) k = 0.78f;        /* lit from the left and the top */
                else if (ey < b && ey < w - ex) k = 0.82f;
                else if (w - ex < b && w - ex < w - ey) k = 0.36f;
                else if (w - ey < b) k = 0.32f;
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)(BLOCK[i] * k * 2.0f * p[i] / 200.0f);
            }
    }
    if (th > 0) {                               /* the tray: a groove, a slot for each loose mirror */
        const float tone[3] = {0.66f, 0.62f, 0.6f};
        float tw = fminf(gw, (g->loose + 0.6f) * 1.35f * q), tx = ox + (gw - tw) / 2;
        pzk_inlay(o, S, S, tx, ty, tw, th, th * 0.3f, tone, fmaxf(1, 0.04f * q));
    }
}

/* the token: a brass mirror across a wooden tile, the beam turned by it */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.9f), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.05f * w), 0.3f);
    float c = w / 2.0f, W = (float)w;
    const uint8_t red[3] = {236, 92, 40}, core[3] = {255, 232, 160};
    pzk_stroke(b, w, w, 0.06f * W, c, c, c, 0.11f * W, red, 0.55f);
    pzk_stroke(b, w, w, c, c, c, 0.06f * W, 0.11f * W, red, 0.55f);
    pzk_stroke(b, w, w, 0.06f * W, c, c, c, 0.035f * W, core, 1);
    pzk_stroke(b, w, w, c, c, c, 0.06f * W, 0.035f * W, core, 1);
    float d = 0.3f * W;
    pzk_stroke(b, w, w, c - d, c + d, c + d, c - d, 0.075f * W, PZK_DARK, 1);
    pzk_stroke(b, w, w, c - d, c + d, c + d, c - d, 0.055f * W, BRASS, 1);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static SDL_Texture *tex_of(int w, int h, void (*paint)(uint8_t *, int, int, int), int arg)
{
    uint8_t *b = ui_buf(w, h);
    memset(b, 0, (size_t)w * h * 4);
    paint(b, w, h, arg);
    return ui_tex(w, h, b);
}

static void paint_sq(uint8_t *o, int w, int h, int kind)
{
    (void)h;
    if (kind == 0) paint_lamp(o, w);
    else if (kind == 1) paint_gem(o, w, 0);
    else if (kind == 2) paint_gem(o, w, 1);
    else if (kind == 3) paint_bomb(o, w);
    else if (kind == 4) {                       /* the pivot's walnut base */
        pzk_ball(o, w, w, w / 2.0f, w / 2.0f, 0.46f * w, PZK_WALNUT, 1, 0.6f);
        pzk_arc(o, w, w, w / 2.0f, w / 2.0f, 0.4f * w, 0.46f * w, 0, 360, PZK_DARK, 0.5f);
    } else {                                    /* the rivet on the pivot */
        pzk_ball(o, w, w, w / 2.0f, w / 2.0f, 0.42f * w, PZK_DARK, 1, 0);
        pzk_ball(o, w, w, w / 2.0f, w / 2.0f, 0.34f * w, BRASS, 1, 0.8f);
    }
}

static void layout(box b)
{
    last_box = b;
    float S = floorf(fminf(b.w, b.h));
    side = S;
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    fr = (int)(S * FRAME);
    int n = G.n ? G.n : 5;
    float inner = S - 2 * fr;
    cs = (int)(G.loose ? inner / (n + 1.25f) : inner * 0.97f / n);
    tray_h = G.loose ? floorf(0.95f * cs) : 0;
    float total = n * cs + (G.loose ? 0.3f * cs + tray_h : 0);
    float ox = floorf((S - n * cs) / 2), oy = floorf(fr + (inner - total) / 2);
    gx = bx + ox;
    gy = by + oy;
    tray_y = gy + n * cs + 0.3f * cs;
    int key = ((int)S * 64 + serial % 64) * 8 + n;
    if (made_key != key) {
        made_key = key;
        int w = (int)S;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), w, fr, &G, ox, oy, cs, tray_y - by, tray_h);
        t_board = ui_tex(w, w, ui_buf(w, w));
        int pw = (int)(1.12f * cs), ph = (int)fmaxf(4, 0.2f * cs);
        ui_free(&t_plate);
        ui_free(&t_glass);
        t_plate = tex_of(pw, ph, paint_plate, 0);
        t_glass = tex_of(pw, ph, paint_plate, 1);
        SDL_Texture **sq[6] = {&t_lamp, &t_gem[0], &t_gem[1], &t_bomb, &t_base, &t_rivet};
        for (int i = 0; i < 6; i++) {
            int s = i == 4 ? (int)(0.62f * cs) : i == 5 ? (int)fmaxf(4, 0.16f * cs) : cs;
            ui_free(sq[i]);
            *sq[i] = tex_of(s, s, paint_sq, i);
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        token(0, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(&ptr, 0, sizeof ptr);
    memset(turn, 0, sizeof turn);
    hint.cell = -1;
    win.on = 0;
}

static void after(void)
{
    mr_trace(&G, &beam);
    if (mr_solved(&G) && !win.on) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    ui_animate();
}

static void begin(void)
{
    serial++;
    calm();
    mr_trace(&G, &beam);
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    if (!mr_decode(&G, mr_levels[level < 0 || level >= MR_LEVELS ? 0 : level])) mr_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    mr_new(&G, variant < 0 || variant >= MR_VARIANTS ? 0 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int score(void) { return G.moves; }
static int par(void) { return G.par; }
static int level_stars(void) { return pzk_stars(ui_par_stars(G.moves, G.par), G.hints, 0); }

static int status(void) { return mr_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn_of(void) { return 0; }
static Uint32 win_end(void) { return (Uint32)((beam.length + 1) * WAVE_STEP + WAVE_LEN + FLASH_MS); }
static int busy(void) { return win.on && ui_ms() - win.t0 < win_end(); }

static float way_deg(int way) { return way == MR_SLASH ? -45.0f : 45.0f; }

/* a mirror's way changed: it turns there from where it was */
static void turned(int c, int old_way)
{
    turn[c].deg0 = way_deg(old_way);
    turn[c].t0 = ui_ms() ? ui_ms() : 1;
}

static int undo(void)
{
    uint8_t kind[MR_CELLS], way[MR_CELLS];
    memcpy(kind, G.kind, sizeof kind);
    memcpy(way, G.way, sizeof way);
    int c = mr_undo(&G);
    if (c < 0) return 0;
    calm();
    for (int i = 0; i < G.n * G.n; i++)
        if (kind[i] == G.kind[i] && way[i] != G.way[i] && (G.kind[i] == MR_PIVOT || G.kind[i] == MR_LOOSE))
            turned(i, way[i]);
    after();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = mr_script(&G, moves);
    after();
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void cell_mid(int c, float *x, float *y)
{
    *x = gx + (c % G.n + 0.5f) * cs;
    *y = gy + (c / G.n + 0.5f) * cs;
}

static int cell_at(float x, float y)
{
    float ix = x - gx, iy = y - gy;
    if (ix < 0 || iy < 0 || ix >= G.n * cs || iy >= G.n * cs) return -1;
    return (int)(iy / cs) * G.n + (int)(ix / cs);
}

static int in_tray(float x, float y)
{
    return G.loose && y >= tray_y - 0.15f * cs && y <= tray_y + tray_h + 0.3f * cs && x >= gx && x <= gx + G.n * cs;
}

static void slot_mid(int i, float *x, float *y)  /* the tray's slot i */
{
    float pitch = 1.35f * cs;
    *x = gx + G.n * cs / 2.0f + (i - (G.loose - 1) / 2.0f) * pitch;
    *y = tray_y + tray_h / 2;
}

static void draw_plate(SDL_Texture *t, float cx, float cy, float deg, float scale, Uint8 a)
{
    float pw = 1.12f * cs * scale, ph = fmaxf(4, 0.2f * cs) * scale;
    float rad = deg * PUZ_PI / 180;
    ui_shadow_flat(cx + 0.05f * cs, cy + 0.06f * cs, pw * (0.35f + 0.65f * fabsf(cosf(rad))),
                   pw * (0.35f + 0.65f * fabsf(sinf(rad))), 0.35f * a / 255);
    SDL_SetTextureAlphaMod(t, a);
    ui_put_rot(t, cx, cy, pw, ph, deg);
    SDL_SetTextureAlphaMod(t, 255);
}

static float mirror_deg(int c, Uint32 now)
{
    float to = way_deg(G.way[c]);
    if (!turn[c].t0) return to;
    float t = (now - turn[c].t0) / (float)TURN_MS;
    if (t >= 1) { turn[c].t0 = 0; return to; }
    ui_animate();
    float from = turn[c].deg0;
    if (from == to) return to;
    return from + (to - from) * puz_ease(t);
}

/* the beam: three passes of light added to the wood (a wide red glow, an orange one, a pale gold core), each
 * straight run of it in one stroke, a dot where it turns */
static void draw_beam(void)
{
    static const pzk_rgba pass[3] = {{210, 50, 14, 105}, {255, 128, 40, 150}, {255, 240, 188, 245}};
    const float wd[3] = {0.34f, 0.13f, 0.05f}, soft[3] = {0.24f, 0.08f, 0};
    int n = G.n;
    for (int p = 0; p < 3; p++) {
        float w = wd[p] * cs, sf = soft[p] * cs;
        for (int way = 0; way < 2; way++)       /* the rows, then the columns: the runs of half cells lit */
            for (int i = 0; i < n; i++)
                for (int h = 0, run = -1; h <= 2 * n; h++) {
                    int on = 0;
                    if (h < 2 * n) {
                        int j = h / 2, c = way ? i + j * n : j + i * n;
                        on = beam.side[c] >> (way ? (h & 1 ? 3 : 1) : (h & 1 ? 0 : 2)) & 1;
                    }
                    if (on && run < 0) run = h;
                    if (on || run < 0) continue;
                    float a = gx + run / 2.0f * cs, b = gx + h / 2.0f * cs, m = gy + (i + 0.5f) * cs;
                    if (way) pzk_line(gx + (i + 0.5f) * cs, gy + run / 2.0f * cs, gx + (i + 0.5f) * cs,
                                      gy + h / 2.0f * cs, w, sf, pass[p], 1);
                    else pzk_line(a, m, b, m, w, sf, pass[p], 1);
                    run = -1;
                }
        for (int c = 0; c < n * n; c++) {
            int s = beam.side[c], k = G.kind[c];
            if (!s || ((k == MR_EMPTY || k == MR_GEM) && (s == 5 || s == 10 || s == 15))) continue;
            float x, y;
            cell_mid(c, &x, &y);
            pzk_dot(x, y, w, sf, pass[p], 1);
        }
    }
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    int n = G.n;
    ui_put(t_board, bx, by, side, side);
    float x, y, wt = win.on ? (float)(now - win.t0) : -1;
    for (int c = 0; c < n * n; c++) {           /* the pivots' bases under the beam */
        if (G.kind[c] != MR_PIVOT) continue;
        cell_mid(c, &x, &y);
        ui_shadow_flat(x, y, 0.62f * cs, 0.62f * cs, 0.5f);
        ui_put_c(t_base, x, y, 0.62f * cs, 0.62f * cs);
    }
    if (hint.cell >= 0) {                       /* the hint's cell: a gold light, swelling and fading */
        float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
        if (p > 0) {
            cell_mid(hint.cell, &x, &y);
            ui_mark_gold(x, y, 2.4f * cs * p);
            ui_animate();
        } else {
            hint.cell = -1;
        }
    }
    draw_beam();
    for (int c = 0; c < n * n; c++) {           /* the lamp, the gems, the bombs over it */
        cell_mid(c, &x, &y);
        int k = G.kind[c];
        if (k == MR_GEM) {
            if (beam.side[c]) ui_mark_gold(x, y, 1.5f * cs);
            ui_shadow_flat(x, y, 0.7f * cs, 0.7f * cs, 0.45f);
            ui_put_c(t_gem[beam.side[c] ? 1 : 0], x, y, (float)cs, (float)cs);
        } else if (k == MR_BOMB) {
            if (beam.bomb == c) {
                ui_mark_glow(x, y, 1.9f * cs);
                ui_mark_glow(x, y, 1.3f * cs);
            }
            ui_shadow_flat(x, y + 0.04f * cs, 0.6f * cs, 0.6f * cs, 0.5f);
            ui_put_c(t_bomb, x, y, (float)cs, (float)cs);
        } else if (k == MR_LAMP) {
            ui_put_rot(t_lamp, x, y, (float)cs, (float)cs, -90.0 * G.way[c]);
            float lx = x + (G.way[c] == 0 ? 0.3f : G.way[c] == 2 ? -0.3f : 0) * cs,
                  ly = y + (G.way[c] == 1 ? -0.3f : G.way[c] == 3 ? 0.3f : 0) * cs;
            pzk_dot(lx, ly, 0.16f * cs, 0.14f * cs, (pzk_rgba){255, 150, 70, 170}, 1);   /* its lens alight */
        }
    }
    for (int c = 0; c < n * n; c++) {           /* the mirrors and the glass over it */
        int k = G.kind[c];
        if (k != MR_PIVOT && k != MR_LOOSE && k != MR_GLASS) continue;
        if (ptr.on && ptr.moved && ptr.from == c) continue;   /* carried */
        cell_mid(c, &x, &y);
        draw_plate(k == MR_GLASS ? t_glass : t_plate, x, y, k == MR_GLASS ? way_deg(G.way[c]) : mirror_deg(c, now), 1,
                   255);
        if (k == MR_PIVOT) ui_put_c(t_rivet, x, y, 0.16f * cs, 0.16f * cs);
        if (beam.side[c] && k != MR_GLASS) {    /* where the light strikes it */
            pzk_dot(x, y, 0.12f * cs, 0.1f * cs, (pzk_rgba){255, 226, 160, 210}, 1);
        }
    }
    if (G.loose) {                              /* the tray: the loose mirrors left */
        int left = mr_loose_left(&G) - (ptr.on && ptr.moved && ptr.from < 0);
        for (int i = 0; i < left; i++) {
            slot_mid(i, &x, &y);
            draw_plate(t_plate, x, y, 0, 0.95f, 255);
        }
    }
    if (wt >= 0) {                              /* the win: the light along the beam, then the gems */
        for (int c = 0; c < n * n; c++) {
            if (beam.dist[c] == 0xffff) continue;
            cell_mid(c, &x, &y);
            float p = puz_pulse(wt, beam.dist[c] * WAVE_STEP, WAVE_LEN);
            if (p > 0) ui_mark_gold(x, y, 2.0f * cs * p);
            if (G.kind[c] == MR_GEM) {
                float q = puz_pulse(wt, (beam.length + 1) * WAVE_STEP, FLASH_MS + WAVE_LEN);
                if (q > 0) ui_mark_gold(x, y, 3.0f * cs * q);
            }
        }
        if (wt < win_end()) ui_animate();
        else win.on = 0;
    }
    if (ptr.on && ptr.moved) {                  /* a mirror carried, above the finger */
        float cx = ptr.x, cy = ptr.y - 0.55f * cs;
        int c = cell_at(cx, cy);
        if (c >= 0 && G.kind[c] == MR_EMPTY) {
            float mx, my;
            cell_mid(c, &mx, &my);
            ui_mark_select(mx - cs / 2.0f, my - cs / 2.0f, (float)cs, (float)cs);
        }
        int w = ptr.from >= 0 ? G.way[ptr.from] : MR_SLASH;
        draw_plate(t_plate, cx, cy, way_deg(w), 1.12f, 255);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void use_tool(int t);

static void press(float x, float y)
{
    int tool = ui_tool_hit(x, y);
    if (tool >= 0) {
        memset(&ptr, 0, sizeof ptr);
        use_tool(tool);
        return;
    }
    memset(&ptr, 0, sizeof ptr);
    if (mr_solved(&G)) return;
    int c = cell_at(x, y);
    if (c < 0 && !in_tray(x, y)) return;
    ptr.on = 1;
    ptr.cell = c;
    ptr.from = c >= 0 && G.kind[c] == MR_LOOSE ? c : c < 0 && mr_loose_left(&G) > 0 ? -1 : -2;
    ptr.x = ptr.x0 = x;
    ptr.y = ptr.y0 = y;
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    ptr.x = x;
    ptr.y = y;
    if (!ptr.moved && ptr.from > -2 && hypotf(x - ptr.x0, y - ptr.y0) > 0.3f * cs) ptr.moved = 1;
    if (ptr.moved) ui_animate();
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    ptr.on = 0;
    ui_animate();
    if (!ptr.moved) {                           /* a tap */
        int c = ptr.cell;
        if (c < 0 || cell_at(x, y) != c) return;
        int k = G.kind[c], old = G.way[c];
        if (!mr_tap(&G, c)) { ui_sound(SND_DENY); return; }
        if (k == MR_PIVOT || (k == MR_LOOSE && G.kind[c] == MR_LOOSE)) {
            turned(c, old);
            ui_sound(SND_MOVE);
        } else {
            ui_sound(G.kind[c] == MR_LOOSE ? SND_PLACE : SND_TAP);
        }
        after();
        return;
    }
    int to = cell_at(x, y - 0.55f * cs);        /* dropped: on the cell under the mirror */
    if (to < 0) to = cell_at(x, y);
    if (ptr.from >= 0 && to < 0) {              /* off the board: back to the tray */
        mr_set(&G, ptr.from, 2);
        ui_sound(SND_TAP);
        after();
        return;
    }
    if (to < 0 || to == ptr.from) return;
    int ok = ptr.from >= 0 ? mr_move(&G, ptr.from, to) : mr_set(&G, to, MR_SLASH);
    if (!ok) { ui_sound(SND_DENY); return; }
    ui_sound(SND_PLACE);
    after();
}

/* ---- the bar: the moves, the hint's lamp ------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    ui_count c[1] = {{t_small, G.moves}};
    ui_bar_fit(b, c, 1, 0);
}

static void use_hint(void)
{
    if (mr_solved(&G)) { ui_sound(SND_DENY); return; }
    uint8_t way[MR_CELLS], kind[MR_CELLS];
    memcpy(way, G.way, sizeof way);
    memcpy(kind, G.kind, sizeof kind);
    int c = mr_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    if (kind[c] == G.kind[c] && way[c] != G.way[c]) turned(c, way[c]);
    hint.cell = c;
    hint.t0 = ui_ms();
    ui_sound(SND_PLACE);
    after();
}

static void use_tool(int t)
{
    if (t == 0) use_hint();
}

/* ---- the menu's and the picker's cards: a puzzle of the level solved, its beam lit; as many ebony stones as its level
 * under the picker's (one: easy .. four: expert) --------------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int level = variant < 0 ? 1 : variant;
    static mr_game T;
    mr_new(&T, level, 2026u + (uint32_t)level);
    for (int i = 0; i < 40 && !mr_solved(&T); i++) mr_hint(&T);
    mr_beam tb;
    mr_trace(&T, &tb);
    int S = variant < 0 ? size : (int)(0.84f * size), n = T.n, o = (size - S) / 2;
    int f = (int)(S * FRAME), q = (int)((S - 2 * f) * 0.97f / n);
    float ox = (S - n * q) / 2.0f, oy = ox;
    uint8_t *b = calloc((size_t)S * S, 4);
    if (!b) return;
    paint_board(b, S, f, &T, ox, oy, q, 0, 0);
    static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, -1, 0, 1};
    const uint8_t glow[3] = {230, 80, 30}, core[3] = {255, 236, 170};
    float *m = calloc((size_t)S * S, sizeof *m);
    for (int pass = 0; pass < 2 && m; pass++) {
        memset(m, 0, (size_t)S * S * sizeof *m);
        for (int c = 0; c < n * n; c++)
            for (int d = 0; d < 4; d++) {
                if (!(tb.side[c] >> d & 1)) continue;
                float x = ox + (c % n + 0.5f) * q, y = oy + (c / n + 0.5f) * q;
                pzk_stroke_mask(m, S, S, x, y, x + DX[d] * 0.5f * q, y + DY[d] * 0.5f * q, pass ? 0.05f * q : 0.16f * q);
            }
        pzk_mask_paint(b, S, S, m, pass ? core : glow, pass ? 1 : 0.5f);
    }
    free(m);
    for (int c = 0; c < n * n; c++) {
        float x = ox + (c % n + 0.5f) * q, y = oy + (c / n + 0.5f) * q;
        int k = T.kind[c];
        uint8_t *p = calloc((size_t)q * q, 4);
        if (!p) break;
        if (k == MR_GEM) paint_gem(p, q, tb.side[c] != 0);
        else if (k == MR_BOMB) paint_bomb(p, q);
        else if (k == MR_LAMP) {
            uint8_t *l = calloc((size_t)q * q, 4);
            if (l) {
                paint_lamp(l, q);
                for (int yy = 0; yy < q; yy++)  /* turned to its direction */
                    for (int xx = 0; xx < q; xx++) {
                        int sx = xx, sy = yy, d = T.way[c];
                        if (d == 1) sx = q - 1 - yy, sy = xx;
                        else if (d == 2) sx = q - 1 - xx, sy = q - 1 - yy;
                        else if (d == 3) sx = yy, sy = q - 1 - xx;
                        memcpy(p + 4 * (yy * q + xx), l + 4 * (sy * q + sx), 4);
                    }
                free(l);
            }
        } else if (k == MR_PIVOT || k == MR_LOOSE || k == MR_GLASS) {
            float d = 0.36f * q, sgn = T.way[c] == MR_SLASH ? 1 : -1;
            const uint8_t pale[3] = {200, 228, 236};
            pzk_stroke(p, q, q, q / 2.0f - d, q / 2.0f + sgn * d, q / 2.0f + d, q / 2.0f - sgn * d, 0.09f * q,
                       PZK_DARK, 1);
            pzk_stroke(p, q, q, q / 2.0f - d, q / 2.0f + sgn * d, q / 2.0f + d, q / 2.0f - sgn * d, 0.065f * q,
                       k == MR_GLASS ? pale : BRASS, 1);
        }
        art_blend(b, S, S, p, q, q, (int)(x - q / 2.0f), (int)(y - q / 2.0f));
        free(p);
    }
    art_blend(rgba, size, size, b, S, S, o, variant < 0 ? o : 0);
    free(b);
    if (variant < 0) return;
    int d = (int)(0.1f * size), yy = S + (size - S - d) / 2;
    uint8_t *st = calloc((size_t)d * d, 4);
    if (!st) return;
    art_counter(COUNTER_STONE, 1, d, st);
    for (int i = 0; i <= level; i++)
        art_blend(rgba, size, size, st, d, d, (int)(size / 2.0f + (i - level / 2.0f) * 1.25f * d - d / 2.0f), yy);
    free(st);
}

/* a chapter's tab: a small board of its biggest size */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const int SIZE[MR_CHAPTERS] = {5, 6, 7, 7, 8, 8};
    pzk_grid_icon(rgba, size, SIZE[chapter < 0 ? 0 : chapter >= MR_CHAPTERS ? MR_CHAPTERS - 1 : chapter]);
}

/* the board, then the shell's tools: the hint */
static void draw_all(void)
{
    draw();
    ui_tool(0, ICON_HINT, mr_solved(&G) ? UI_DIM : 0);
}

static const game mirrors_game = {
    .id = "mirrors", .order = 402, .variants = MR_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw_all,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn_of, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = 1,
    .levels = MR_LEVELS, .chapters = MR_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .level_stars = level_stars, .score = score, .par = par,
};
REGISTER_GAME(mirrors_game)
