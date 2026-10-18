/*
 * Domino Chain in RetroStone Board, for one player: a board of the house wood in its walnut frame, its felt squares
 * sunk green, seen from the front and above so the pieces stand up in three dimensions (the rules:
 * dominochain_rules.c). The given dominoes ivory with ebony pips, the stock's ebony with ivory pips, the start red
 * lacquer over a brass arrow, long dominoes twice as tall, a turn a curve of three small dominoes, a splitter a brass
 * capped block, the bell brass on a walnut stand. The push: each piece tips over its edge, faster as it goes, in turn
 * along the chain; a piece hit from the side rocks and stays; the bell swings and rings out in gold rings. Missed: the
 * pieces left standing glow red, then everything stands up again.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The tools: the push, the lamp (a hint: a piece put, or one taken back, in gold light), then the stock's pieces, each
 * with how many are left (the one chosen on the gold disc). Tap a free wooden square: the piece chosen goes there,
 * turned the way the chain would hit it. Tap a piece put: it turns; after its last way, it goes back to the stock.
 * Small dots show the squares the chain reaches now and stops at. Undo takes back one change, all the way to the
 * start. The bar: the pieces put, and at its right end the par (the fewest) by a small domino. Solved: gold light
 * runs down the fallen chain to the bell.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/dominochain.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define STEP_MS 150                             /* a step of the chain */
#define FALL_MS 260                             /* a piece tipping over */
#define WOBBLE_MS 300
#define RING_MS 1100
#define MISS_MS 900                             /* the standing pieces glowing red */
#define RISE_MS 380
#define WIN_MS 1400
#define FRAME 0.3f
#define KZ 0.55f                                /* a unit of height up the screen, of a square */
#define SH 0.10f                                /* and across it */
#define TOP 0.62f                               /* room above the board for the pieces standing, of a square */

enum { T_PUSH, T_HINT, T_KIND };                /* the tools: the push, the lamp, then the stock's kinds */

static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
static const uint8_t IVORY[3] = {238, 230, 210}, EBONY[3] = {46, 40, 38}, RED[3] = {182, 46, 36},
                     BRASS[3] = {214, 170, 84}, WALNUT[3] = {108, 72, 47};

static dmc_game G;
static int serial, kind_sel, tool_down = -1, spin[DMC_CELLS], glow_c = -1, glow_take;
static Uint32 glow_t0;
static dmc_trace tr;
static struct { int on, ok; Uint32 t0, end; } run;   /* a push on its way (end: when its last piece lies) */
static struct { int on; Uint32 t0; } rise, win;

static box last_box;
static float bx0, by0, q;                       /* the board's first square's top left on the screen, a square */
static int made_q = -1, made_serial = -1, made_small = -1;
static SDL_Texture *t_board, *t_face[3], *t_bell, *t_small;

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* a domino's face, w x h: its colour, a rim, a line across the middle, pips (a and b on its halves) */
static void paint_face(const uint8_t body[3], const uint8_t pip[3], int a, int b, int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    float r = 0.16f * w;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float px = x + 0.5f, py = y + 0.5f, dx = fmaxf(r - px, px - (w - r)), dy = fmaxf(r - py, py - (h - r)), d;
            if (dx > 0 && dy > 0) d = r - hypotf(dx, dy);
            else d = fminf(fminf(px, w - px), fminf(py, h - py));
            float k = 1 + 0.04f * art_noise(x, y * 2) - (d < 0.07f * w ? 0.12f : 0);
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(body[i] * k, 0, 255);
            pzk_over(o + 4 * ((size_t)y * w + x), c, clampf(d + 0.5f, 0, 1));
        }
    puz_ink_rect(o, w, h, 0.18f * w, 0.49f * h, 0.82f * w, 0.51f * h + 1, pip, 0.8f);
    static const float P[7][2] = {{0.5f, 0.5f}, {0.28f, 0.25f}, {0.72f, 0.75f}, {0.28f, 0.75f}, {0.72f, 0.25f},
                                  {0.28f, 0.5f}, {0.72f, 0.5f}};
    static const int USE[7][7] = {{0}, {0}, {1, 2}, {0, 1, 2}, {1, 2, 3, 4}, {0, 1, 2, 3, 4}, {1, 2, 3, 4, 5, 6}};
    for (int half = 0; half < 2; half++) {
        int n = half ? b : a;
        for (int i = 0; i < n && i < 6; i++) {
            const float *p = P[n == 1 ? 0 : USE[n][i]];
            float cx = p[0] * w, cy = (half * 0.5f + p[1] * 0.5f) * h;
            pzk_ball(o, w, h, cx, cy, 0.075f * w + 0.4f, pip, 1, 0);
        }
    }
}

/* the bell's half width t of the way down: a dome flaring to a lip */
static float bell_w(float t) { return 0.07f + 0.13f * t * t + 0.06f * (t > 0.85f ? (t - 0.85f) / 0.15f : 0); }

/* the brass bell on its walnut stand, s x s */
static void paint_bell(int s, uint8_t *o)
{
    memset(o, 0, (size_t)s * s * 4);
    const float stand[] = {0.18f, 0.84f, 0.82f, 0.84f, 0.78f, 0.94f, 0.22f, 0.94f};
    const float post[] = {0.47f, 0.08f, 0.53f, 0.08f, 0.53f, 0.86f, 0.47f, 0.86f};
    const float beam[] = {0.24f, 0.08f, 0.76f, 0.08f, 0.76f, 0.14f, 0.24f, 0.14f};
    float bell[2 * 24];
    int n = 0;
    for (int i = 0; i <= 10; i++) {             /* the right side down, then the left side up */
        float t = i / 10.0f, y = 0.18f + 0.52f * t, hw = bell_w(t);
        bell[2 * n] = 0.5f + hw;
        bell[2 * n + 1] = y;
        n++;
    }
    for (int i = 10; i >= 0; i--) {
        float t = i / 10.0f, y = 0.18f + 0.52f * t, hw = bell_w(t);
        bell[2 * n] = 0.5f - hw;
        bell[2 * n + 1] = y;
        n++;
    }
    for (int i = 0; i < 2 * n; i++) bell[i] *= s;
    float st[8], po[8], be[8];
    for (int i = 0; i < 8; i++) { st[i] = stand[i] * s; po[i] = post[i] * s; be[i] = beam[i] * s; }
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            uint8_t *p = o + 4 * ((size_t)y * s + x);
            float k = 1 + 0.06f * art_noise(x, y);
            uint8_t wal[3] = {(uint8_t)(WALNUT[0] * k), (uint8_t)(WALNUT[1] * k), (uint8_t)(WALNUT[2] * k)};
            pzk_over(p, wal, pzt_cover(st, 4, (float)x, (float)y));
            pzk_over(p, wal, pzt_cover(po, 4, (float)x, (float)y));
            pzk_over(p, wal, pzt_cover(be, 4, (float)x, (float)y));
            float cov = pzt_cover(bell, n, (float)x, (float)y);
            if (cov > 0) {
                float u = (x + 0.5f) / s - 0.5f, lit = 1.15f - 2.2f * fabsf(u + 0.06f) + 0.1f * ((y + 0.5f) / s);
                uint8_t c[3];
                for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(BRASS[i] * lit, 0, 255);
                pzk_over(p, c, cov);
            }
        }
    pzk_ball(o, s, s, 0.5f * s, 0.74f * s, 0.05f * s, BRASS, 1, 1);   /* the clapper */
    pzk_ball(o, s, s, 0.5f * s, 0.17f * s, 0.035f * s, BRASS, 1, 1);
}

/* a small domino standing, its face on (the bars' token, the par, the stock's domino) */
static void paint_token(int size, uint8_t *o)
{
    int w = (int)(0.52f * size), h = (int)(0.9f * size);
    memset(o, 0, (size_t)size * size * 4);
    if (w < 3 || h < 3) return;
    uint8_t *f = malloc((size_t)w * h * 4);
    if (!f) return;
    paint_face(IVORY, EBONY, 3, 5, w, h, f);
    art_blend(o, size, size, f, w, h, (size - w) / 2, (size - h) / 2);
    free(f);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    paint_token(size, rgba);
}

/* the stock's pieces on their tools: a domino, a turn (three small ones on a curve), a long one, a splitter */
static void paint_kind(int kind, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    if (kind == DMC_K_DOMINO || kind == DMC_K_LONG) {
        int w = (int)((kind ? 0.34f : 0.46f) * size), h = (int)((kind ? 0.94f : 0.8f) * size);
        uint8_t *f = malloc((size_t)w * h * 4);
        if (f) {
            paint_face(EBONY, IVORY, kind ? 2 : 1, kind ? 3 : 2, w, h, f);
            art_blend(o, size, size, f, w, h, (size - w) / 2, (size - h) / 2);
        }
        free(f);
    } else if (kind == DMC_K_TURN) {
        for (int i = 0; i < 3; i++) {
            float a = (0.15f + 0.35f * i) * PUZ_PI / 2, cx = 0.16f + 0.62f * cosf(a), cy = 0.84f - 0.62f * sinf(a);
            float ux = cosf(a), uy = -sinf(a), vx = -uy, vy = ux;   /* across the curve, along it */
            float hw = 0.16f, ht = 0.055f;
            const float pts[] = {(cx - ux * hw - vx * ht) * size, (cy - uy * hw - vy * ht) * size,
                                 (cx + ux * hw - vx * ht) * size, (cy + uy * hw - vy * ht) * size,
                                 (cx + ux * hw + vx * ht) * size, (cy + uy * hw + vy * ht) * size,
                                 (cx - ux * hw + vx * ht) * size, (cy - uy * hw + vy * ht) * size};
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++)
                    pzk_over(o + 4 * ((size_t)y * size + x), EBONY, pzt_cover(pts, 4, (float)x, (float)y));
        }
    } else {
        const float sq[] = {0.24f * size, 0.24f * size, 0.76f * size, 0.24f * size, 0.76f * size, 0.76f * size,
                            0.24f * size, 0.76f * size};
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++)
                pzk_over(o + 4 * ((size_t)y * size + x), EBONY, pzt_cover(sq, 4, (float)x, (float)y));
        pzk_ball(o, size, size, 0.5f * size, 0.5f * size, 0.15f * size, BRASS, 1, 1);
        pzk_stroke(o, size, size, 0.08f * size, 0.5f * size, 0.22f * size, 0.5f * size, 0.04f * size, BRASS, 1);
        pzk_stroke(o, size, size, 0.78f * size, 0.5f * size, 0.92f * size, 0.5f * size, 0.04f * size, BRASS, 1);
    }
}

/* the board: the plank in its frame f, w x h squares of s pixels, the felt squares sunk, the start's brass arrow */
static void paint_board(const dmc_map *m, int s, int f, uint8_t *o)
{
    int W = m->w * s + 2 * f, H = m->h * s + 2 * f;
    art_plank(m->w * s, m->h * s, f, o);
    float t = fmaxf(1, 0.025f * s);
    for (int c = 0; c < m->w * m->h; c++) {
        float x = f + c % m->w * s, y = f + c / m->w * s;
        if (m->t[c] == DMC_FELT) {
            art_felt(o, W, H, (int)x, (int)y, (int)x + s, (int)y + s);
            static const uint8_t dark[3] = {10, 20, 14};
            float e = 0.08f * s;
            art_tint_rect(o, W, H, x, y, x + s, y + e, dark, 0.45f);
            art_tint_rect(o, W, H, x, y, x + e, y + s, dark, 0.35f);
            continue;
        }
        art_tint_rect(o, W, H, x, y, x + s, y + t, PUZ_INK, 0.16f);   /* the squares ruled faintly */
        art_tint_rect(o, W, H, x, y, x + t, y + s, PUZ_INK, 0.16f);
        if (m->t[c] == DMC_START) {             /* a brass arrow under the start, its way */
            int d = m->o[c];
            float cx = x + s / 2.0f, cy = y + s / 2.0f, ux = (float)DX[d], uy = (float)DY[d], vx = -uy, vy = ux;
            const float a[] = {cx + ux * 0.44f * s, cy + uy * 0.44f * s, cx + ux * 0.18f * s + vx * 0.2f * s,
                               cy + uy * 0.18f * s + vy * 0.2f * s, cx + ux * 0.18f * s - vx * 0.2f * s,
                               cy + uy * 0.18f * s - vy * 0.2f * s};
            for (int py = (int)y; py < (int)y + s; py++)
                for (int px = (int)x; px < (int)x + s; px++)
                    pzk_over(o + 4 * ((size_t)py * W + px), BRASS, pzt_cover(a, 3, (float)px, (float)py));
        }
    }
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    last_box = b;
    q = floorf(fminf(b.w / (G.m.w + 2 * FRAME), b.h / (G.m.h + 2 * FRAME + TOP)));
    bx0 = floorf(b.x + (b.w - G.m.w * q) / 2);
    by0 = floorf(b.y + (b.h - (G.m.h - TOP) * q) / 2);
    int s = (int)q;
    if (made_q != s || made_serial != serial) {
        made_q = s;
        made_serial = serial;
        int f = (int)(FRAME * s), W = G.m.w * s + 2 * f, H = G.m.h * s + 2 * f;
        uint8_t *o = malloc((size_t)W * H * 4);
        ui_free(&t_board);
        if (o) {
            paint_board(&G.m, s, f, o);
            t_board = ui_tex(W, H, o);
        }
        free(o);
        int fw = (int)(0.62f * s), fh = s;
        static const uint8_t *const BODY[3] = {IVORY, EBONY, RED}, *const PIP[3] = {EBONY, IVORY, IVORY};
        for (int i = 0; i < 3; i++) {
            ui_free(&t_face[i]);
            paint_face(BODY[i], PIP[i], 2 + i, 4 - i, fw, fh, ui_buf(fw, fh));
            t_face[i] = ui_tex(fw, fh, ui_buf(fw, fh));
        }
        int bs = (int)(1.3f * s);
        ui_free(&t_bell);
        paint_bell(bs, ui_buf(bs, bs));
        t_bell = ui_tex(bs, bs, ui_buf(bs, bs));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        paint_token(small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
    int last = 0;
    for (int k = 0; k < DMC_KINDS; k++)
        if (G.m.stock[k]) last = k;
    ui_tools_shown(T_KIND + last + 1);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    run.on = rise.on = win.on = 0;
    tool_down = glow_c = -1;
}

static void pick_kind(void)                     /* the kind chosen: still in the stock, else the first that is */
{
    if (kind_sel >= 0 && kind_sel < DMC_KINDS && G.s.left[kind_sel]) return;
    for (int k = 0; k < DMC_KINDS; k++)
        if (G.s.left[k]) { kind_sel = k; return; }
    for (int k = 0; k < DMC_KINDS; k++)
        if (G.m.stock[k]) { kind_sel = k; return; }
    kind_sel = 0;
}

static void begin(void)
{
    serial++;
    calm();
    memset(spin, 0, sizeof spin);
    kind_sel = -1;
    pick_kind();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!dmc_level(&G, level)) dmc_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    dmc_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return G.done && !run.on ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.s.placed; }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return pzk_stars(ui_par_stars(G.s.placed, par()), G.hints, 0); }

static Uint32 run_len(void) { return (Uint32)(tr.steps * STEP_MS + FALL_MS + (tr.bell >= 0 ? RING_MS / 2 : 0)); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return run.on || rise.on || (win.on && now - win.t0 < WIN_MS);
}

static void push(void)
{
    if (G.done || run.on) { ui_sound(SND_DENY); return; }
    dmc_state before = G.s;
    run.ok = dmc_push(&G, &tr);
    G.s = before;
    run.on = 1;
    run.t0 = ui_ms();
    run.end = run_len() + (run.ok ? 0 : MISS_MS);
    glow_c = -1;
    ui_sound(SND_TAP);
    for (int i = 1; i < tr.n; i++) ui_sound_later(SND_MOVE, (int)(tr.fall[i].step * STEP_MS + FALL_MS * 0.7f));
    if (tr.bell >= 0) ui_sound_later(SND_END, (int)(tr.bell * STEP_MS + 60));
    ui_animate();
}

static void run_over(void)                      /* the push's end: solved, or everything stands up again */
{
    run.on = 0;
    if (run.ok) {
        win.on = 1;
        win.t0 = ui_ms();
    } else {
        rise.on = 1;
        rise.t0 = ui_ms();
        ui_sound(SND_DENY);
    }
    ui_animate();
}

/* the way a piece put on c is turned: as the chain would hit it, else along its neighbours */
static int auto_way(int c, int kind)
{
    uint8_t way[DMC_CELLS];
    dmc_open_hits(&G.m, &G.s, way);
    int d = way[c], x = c % G.m.w, y = c / G.m.w;
    if (kind == DMC_K_SPLIT) return 0;
    if (d == 255) {                             /* no hit: the way the wood runs */
        int across = 0, down = 0;
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (nx < 0 || ny < 0 || nx >= G.m.w || ny >= G.m.h || G.m.t[nx + ny * G.m.w] == DMC_FELT) continue;
            if (k & 1) across++;
            else down++;
        }
        d = across >= down ? 1 : 2;
        if (kind != DMC_K_TURN) return d & 1;
    }
    if (kind != DMC_K_TURN) return d & 1;
    int in = (d + 2) & 3, best = (d + 1) & 3, score = -1;
    for (int t = 0; t < 2; t++) {               /* the turn towards a piece or wood */
        int out = (d + (t ? 3 : 1)) & 3, nx = x + DX[out], ny = y + DY[out], s = 0;
        if (nx >= 0 && ny >= 0 && nx < G.m.w && ny < G.m.h) {
            int o, n = nx + ny * G.m.w, p = dmc_piece(&G.m, &G.s, n, &o);
            s = p >= DMC_DOMINO ? 2 : p == DMC_WOOD ? 1 : 0;
        }
        if (s > score) { score = s; best = out; }
    }
    return ((in + 1) & 3) == best ? in : best;   /* the corner joining the side it comes in by and the way out */
}

static void hint(void)
{
    if (G.done || run.on) { ui_sound(SND_DENY); return; }
    int c, kind, o, h = dmc_hint(&G.m, &G.s, G.par, &c, &kind, &o);
    if (h == 1 && dmc_put(&G, c, kind, o)) { glow_take = 0; spin[c] = 0; }
    else if (h == 2 && dmc_take(&G, c)) glow_take = 1;
    else { ui_sound(SND_DENY); return; }
    G.hints++;
    glow_c = c;
    glow_t0 = ui_ms();
    pick_kind();
    ui_sound(h == 1 ? SND_PLACE : SND_TAP);
    ui_animate();
}

static int undo(void)
{
    calm();
    int ok = dmc_undo(&G);
    pick_kind();
    return ok;
}

static int script(const char *moves)
{
    calm();
    int ok = dmc_script(&G, moves);
    pick_kind();
    if (G.done) {
        dmc_run(&G.m, &G.s, &tr);
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- the pieces in three dimensions ------------------------------------------------------------------------------ */
static void proj(float x, float y, float z, float *sx, float *sy)
{
    *sx = bx0 + (x + SH * z) * q;
    *sy = by0 + (y - KZ * z) * q;
}

typedef struct { float x, y, z; } v3;

/* a box standing on (cx, cy) (squares), its thin side along (fx, fy), w across and h high, tipped by phi (radians)
 * over its front bottom edge; its faces lit from the top left, its broad faces with tex (NULL: plain) */
static void draw_box(float cx, float cy, float fx, float fy, float t, float w, float h, float phi, const uint8_t rgb[3],
                     SDL_Texture *tex, float alpha)
{
    float sx = -fy, sy = fx, cp = cosf(phi), sp = sinf(phi);
    v3 P[8];
    for (int i = 0; i < 8; i++) {
        float a = (i & 1 ? 0.5f : -0.5f) * t, b = (i & 2 ? 0.5f : -0.5f) * w, c = i & 4 ? h : 0;
        float a0 = a - t / 2, A = a0 * cp + c * sp, C = -a0 * sp + c * cp;
        P[i] = (v3){cx + fx * (t / 2 + A) + sx * b, cy + fy * (t / 2 + A) + sy * b, C};
    }
    /* the faces: corner indices, the normal (in the box's frame, turned with it), the broad faces textured */
    static const int F[5][4] = {{1, 3, 7, 5}, {2, 0, 4, 6}, {3, 2, 6, 7}, {0, 1, 5, 4}, {4, 5, 7, 6}};
    float nf[3] = {fx * cp, fy * cp, -sp}, nz[3] = {fx * sp, fy * sp, cp}, ns[3] = {sx, sy, 0};
    const float *N[5] = {nf, NULL, ns, NULL, nz};
    float L[3] = {-0.33f, 0.3f, 0.89f}, view[3] = {-SH, KZ, 1};   /* the light: from the top left, in front */
    for (int k = 0; k < 5; k++) {
        float n[3];
        if (k == 1) { n[0] = -nf[0]; n[1] = -nf[1]; n[2] = -nf[2]; }
        else if (k == 3) { n[0] = -ns[0]; n[1] = -ns[1]; n[2] = 0; }
        else { n[0] = N[k][0]; n[1] = N[k][1]; n[2] = N[k][2]; }
        if (n[0] * view[0] + n[1] * view[1] + n[2] * view[2] <= 0.001f) continue;
        float lit = 0.6f + 0.42f * fmaxf(0, n[0] * L[0] + n[1] * L[1] + n[2] * L[2]);
        SDL_Vertex v[4];
        int broad = (k == 0 || k == 1) && tex;
        for (int j = 0; j < 4; j++) {
            const v3 *p = &P[F[k][j]];
            float x, y;
            proj(p->x, p->y, p->z, &x, &y);
            int ci = F[k][j];
            float u = ci & 2 ? 1.0f : 0.0f, vv = ci & 4 ? 0.0f : 1.0f;
            Uint8 col[3];
            for (int i = 0; i < 3; i++) col[i] = (Uint8)clampf((broad ? 255 : rgb[i]) * lit, 0, 255);
            v[j] = (SDL_Vertex){{x, y}, {col[0], col[1], col[2], (Uint8)(255 * alpha)}, {u, vv}};
        }
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(ui.r, broad ? tex : NULL, v, 4, idx, 6);
    }
}

/* its shadow on the board: the box's corners cast down and to the right */
static void draw_shadow(float cx, float cy, float fx, float fy, float t, float w, float h, float phi)
{
    float sx = -fy, sy = fx, cp = cosf(phi), sp = sinf(phi), pts[16];
    for (int i = 0; i < 8; i++) {
        float a = (i & 1 ? 0.5f : -0.5f) * t, b = (i & 2 ? 0.5f : -0.5f) * w, c = i & 4 ? h : 0;
        float a0 = a - t / 2, A = a0 * cp + c * sp, C = -a0 * sp + c * cp;
        float x = cx + fx * (t / 2 + A) + sx * b + 0.32f * C, y = cy + fy * (t / 2 + A) + sy * b + 0.22f * C;
        proj(x, y, 0, &pts[2 * i], &pts[2 * i + 1]);
    }
    int hull[9], nh = 0, first = 0;             /* the hull of the eight (gift wrapping) */
    for (int i = 1; i < 8; i++)
        if (pts[2 * i] < pts[2 * first] ||
            (pts[2 * i] == pts[2 * first] && pts[2 * i + 1] < pts[2 * first + 1]))
            first = i;
    int p = first;
    do {
        hull[nh++] = p;
        int nx = (p + 1) % 8;
        for (int i = 0; i < 8; i++) {
            float cr = (pts[2 * nx] - pts[2 * p]) * (pts[2 * i + 1] - pts[2 * p + 1]) -
                       (pts[2 * nx + 1] - pts[2 * p + 1]) * (pts[2 * i] - pts[2 * p]);
            if (cr < 0) nx = i;
        }
        p = nx;
    } while (p != first && nh < 8);
    SDL_Vertex v[8];
    int idx[18], ni = 0;
    for (int i = 0; i < nh; i++) v[i] = (SDL_Vertex){{pts[2 * hull[i]], pts[2 * hull[i] + 1]}, {0, 0, 0, 56}, {0, 0}};
    for (int i = 1; i + 1 < nh; i++) { idx[ni++] = 0; idx[ni++] = i; idx[ni++] = i + 1; }
    if (ni) SDL_RenderGeometry(ui.r, NULL, v, nh, idx, ni);
}

/* how far a piece has tipped at t ms into the push (radians, 0 standing), the way it falls (*way), its wobble */
static float tipped(int c, float t, int *way, float *wob, int part, int parts)
{
    *wob = 0;
    if (!run.on && !rise.on && !win.on) return 0;
    for (int i = 0; i < tr.n; i++)
        if (tr.fall[i].c == c) {
            *way = tr.fall[i].way;
            float t0 = tr.fall[i].step * STEP_MS + (float)part * STEP_MS / parts;
            float f = clampf((t - t0) / FALL_MS, 0, 1);
            float a = (PUZ_PI / 2 - 0.06f) * f * f;
            if (rise.on) a *= 1 - puz_ease((ui_ms() - rise.t0) / (float)RISE_MS);
            return a;
        }
    for (int i = 0; i < tr.nwob; i++)
        if (tr.wob[i].c == c) {
            float f = (t - tr.wob[i].step * STEP_MS) / WOBBLE_MS;
            if (f > 0 && f < 1) { *way = tr.wob[i].way; *wob = 0.16f * sinf(PUZ_PI * 2 * f) * (1 - f); }
        }
    return 0;
}

typedef struct { float key; int c, part; } item;

static int by_key(const void *a, const void *b)
{
    float d = ((const item *)a)->key - ((const item *)b)->key;
    return d < 0 ? -1 : d > 0;
}

/* a turn's small domino i (0..2) on its curve: its place and the way along the curve (from side `in`) */
static void turn_mini(int c, int corner, int in, int i, float *x, float *y, float *fx, float *fy)
{
    static const float CX[4] = {1, 1, 0, 0}, CY[4] = {0, 1, 1, 0};   /* the corner's point in the square */
    static const float MX[4] = {0.5f, 1, 0.5f, 0}, MY[4] = {0, 0.5f, 1, 0.5f};   /* each side's middle */
    int out = in == corner ? (corner + 1) & 3 : corner;
    float ox = CX[corner], oy = CY[corner];
    float a0 = atan2f(MY[in] - oy, MX[in] - ox), a1 = atan2f(MY[out] - oy, MX[out] - ox);
    if (a1 - a0 > PUZ_PI) a1 -= 2 * PUZ_PI;
    if (a0 - a1 > PUZ_PI) a1 += 2 * PUZ_PI;
    float u = (0.2f + 0.3f * i), a = a0 + (a1 - a0) * u, sgn = a1 > a0 ? 1.0f : -1.0f;
    *x = c % G.m.w + ox + 0.5f * cosf(a);
    *y = c / G.m.w + oy + 0.5f * sinf(a);
    *fx = -sinf(a) * sgn;
    *fy = cosf(a) * sgn;
}

static void draw_piece(int c, int part, float t, int shadow)
{
    int o, p = dmc_piece(&G.m, &G.s, c, &o), mine = G.s.t[c] != DMC_WOOD, way = -1;
    float x = c % G.m.w + 0.5f, y = c / G.m.w + 0.5f, wob;
    const uint8_t *body = mine ? EBONY : IVORY;
    SDL_Texture *face = t_face[mine ? 1 : 0];
    if (p == DMC_DOMINO || p == DMC_LONG || p == DMC_START) {
        float phi = tipped(c, t, &way, &wob, 0, 1);
        if (way < 0) way = p == DMC_START ? o : o ? 1 : 2;
        float fx = (float)DX[way], fy = (float)DY[way], h = p == DMC_LONG ? 1.9f : 0.92f;
        if (wob != 0) phi = wob;
        if (p == DMC_START) { body = RED; face = t_face[2]; }
        if (shadow) draw_shadow(x - fx * 0.1f, y - fy * 0.1f, fx, fy, 0.2f, 0.62f, h, phi);
        else draw_box(x - fx * 0.1f, y - fy * 0.1f, fx, fy, 0.2f, 0.62f, h, phi, body, face, 1);
    } else if (p == DMC_TURN) {
        int corner = o, in = corner;            /* standing: as from its first side; fallen: from where it was hit */
        if (run.on || rise.on || win.on)
            for (int i = 0; i < tr.n; i++)
                if (tr.fall[i].c == c) in = tr.fall[i].way == corner ? (corner + 1) & 3 : corner;
        float mx, my, fx, fy;
        turn_mini(c, corner, in, part, &mx, &my, &fx, &fy);
        float phi = tipped(c, t, &way, &wob, part, 3);
        if (wob != 0) phi = wob;
        if (shadow) draw_shadow(mx - fx * 0.06f, my - fy * 0.06f, fx, fy, 0.12f, 0.34f, 0.5f, phi);
        else draw_box(mx - fx * 0.06f, my - fy * 0.06f, fx, fy, 0.12f, 0.34f, 0.5f, phi, body, NULL, 1);
    } else if (p == DMC_SPLIT) {
        float phi = tipped(c, t, &way, &wob, 0, 1);
        if (way < 0) way = 2;
        float fx = (float)DX[way], fy = (float)DY[way];
        if (wob != 0) phi = wob;
        if (shadow) draw_shadow(x - fx * 0.21f, y - fy * 0.21f, fx, fy, 0.42f, 0.42f, 0.42f, phi);
        else {
            draw_box(x - fx * 0.21f, y - fy * 0.21f, fx, fy, 0.42f, 0.42f, 0.42f, phi, body, NULL, 1);
            if (phi < 0.3f) {                   /* its brass cap: a disc and its two ways across */
                float sx, sy;
                proj(x, y, 0.42f * cosf(phi), &sx, &sy);
                ui_mark_gold_a(sx, sy, 0.3f * q, 0.25f);
                pzk_rgba br = {BRASS[0], BRASS[1], BRASS[2], 255};
                pzk_dot(sx, sy, 0.16f * q, 1, br, 0);
                float ax = way & 1 ? 0 : 0.18f * q, ay = way & 1 ? 0.18f * q * KZ : 0;
                pzk_line(sx - ax, sy - ay, sx + ax, sy + ay, 0.05f * q, 1, br, 0);
            }
        }
    } else if (p == DMC_BELL && !shadow) {
        float sx, sy, swing = 0;
        proj(x, y, 0, &sx, &sy);
        if (tr.bell >= 0 && (run.on || win.on)) {
            float f = (t - tr.bell * STEP_MS) / RING_MS;
            if (f > 0 && f < 1) swing = 18 * sinf(PUZ_PI * 5 * f) * (1 - f);
            if (f > 0 && f < 1)
                for (int k = 0; k < 3; k++) {
                    float g = f * 1.4f - 0.2f * k;
                    if (g > 0 && g < 1) ui_mark_gold_a(sx, sy - 0.55f * q, (0.6f + 1.6f * g) * q, 0.6f * (1 - g));
                }
        }
        ui_shadow_flat(sx + 0.06f * q, sy, 0.8f * q, 0.3f * q, 0.5f);
        ui_put_rot(t_bell, sx, sy - 0.5f * q, 1.3f * q, 1.3f * q, swing);
    }
}

static void draw(void)
{
    if (made_q < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    float t = run.on ? (float)(int)(now - run.t0) : 1e9f;
    if (run.on && t >= run.end) { run_over(); t = 1e9f; }
    if (rise.on && now - rise.t0 >= RISE_MS) rise.on = 0;
    int f = (int)(FRAME * q), wh = G.m.w * G.m.h;
    ui_put(t_board, bx0 - f, by0 - f, G.m.w * q + 2 * f, G.m.h * q + 2 * f);
    if (!run.on && !rise.on && !G.done) {       /* where the chain stops now: small dots */
        uint8_t way[DMC_CELLS];
        dmc_open_hits(&G.m, &G.s, way);
        for (int c = 0; c < wh; c++)
            if (way[c] != 255) ui_mark_dot(bx0 + (c % G.m.w + 0.5f) * q, by0 + (c / G.m.w + 0.5f) * q, 0.3f * q);
    }
    if (glow_c >= 0) {                          /* a hint's piece in gold light (taken back: its square) */
        float g = (now - glow_t0) / 1600.0f;
        if (g < 1) {
            ui_mark_gold_a(bx0 + (glow_c % G.m.w + 0.5f) * q, by0 + (glow_c / G.m.w + 0.5f) * q, 1.5f * q,
                           (1 - g) * (glow_take ? 0.6f : 1));
            ui_animate();
        } else glow_c = -1;
    }
    item it[3 * DMC_CELLS];
    int n = 0;
    for (int c = 0; c < wh; c++) {
        int o, p = dmc_piece(&G.m, &G.s, c, &o);
        if (p < DMC_DOMINO) continue;
        int parts = p == DMC_TURN ? 3 : 1;
        for (int k = 0; k < parts; k++) {
            float y = (float)(c / G.m.w), x = (float)(c % G.m.w), wob;
            int w = -1;
            float phi = tipped(c, t, &w, &wob, k, parts);
            if (phi > 0.2f && w >= 0) y += 0.5f * DY[w] * sinf(phi);   /* a piece lying down: where it lies */
            it[n++] = (item){y + 0.001f * x + (phi > 1.2f ? -0.4f : 0), c, k};
        }
    }
    qsort(it, (size_t)n, sizeof *it, by_key);
    for (int i = 0; i < n; i++) draw_piece(it[i].c, it[i].part, t, 1);
    for (int i = 0; i < n; i++) draw_piece(it[i].c, it[i].part, t, 0);
    if (run.on || rise.on || win.on) ui_animate();
    if (!run.on && rise.on && !run.ok) {        /* missed: the pieces left standing glow red */
        float g = (now - rise.t0) / (float)RISE_MS;
        for (int c = 0; c < wh; c++) {
            int o, p = dmc_piece(&G.m, &G.s, c, &o);
            if (p >= DMC_DOMINO && !tr.fallen[c]) {
                float sx, sy;
                proj(c % G.m.w + 0.5f, c / G.m.w + 0.5f, 0.4f, &sx, &sy);
                ui_mark_glow(sx, sy, 1.1f * q * (1 - 0.5f * g));
            }
        }
    }
    if (run.on && !run.ok && t > run.end - MISS_MS)
        for (int c = 0; c < wh; c++) {
            int o, p = dmc_piece(&G.m, &G.s, c, &o);
            if (p >= DMC_DOMINO && !tr.fallen[c]) {
                float sx, sy;
                proj(c % G.m.w + 0.5f, c / G.m.w + 0.5f, 0.4f, &sx, &sy);
                ui_mark_glow(sx, sy, 1.1f * q);
            }
        }
    if (win.on) {                               /* gold light down the fallen chain */
        float wt = (float)(int)(now - win.t0);
        if (wt < WIN_MS) {
            for (int i = 0; i < tr.n; i++) {
                int c = tr.fall[i].c;
                float a = puz_pulse(wt, 50.0f * tr.fall[i].step, 520), sx, sy;
                proj(c % G.m.w + 0.5f, c / G.m.w + 0.5f, 0.1f, &sx, &sy);
                if (a > 0) ui_mark_gold_a(sx, sy, 1.3f * q, a);
            }
            ui_animate();
        }
    }
    /* the tools: the push (lit when the chain would do it all), the lamp, the stock */
    int ready = !G.done && !run.on && dmc_run(&G.m, &G.s, NULL);
    ui_tool(T_PUSH, ICON_PLAY, (G.done ? UI_DIM : ready ? UI_LIT : 0) | (tool_down == T_PUSH ? UI_ON : 0));
    ui_tool(T_HINT, ICON_HINT, (G.done ? UI_DIM : 0) | (tool_down == T_HINT ? UI_ON : 0));
    for (int k = 0; k < DMC_KINDS; k++) {
        float cx, cy, d;
        if (!ui_tool_at(T_KIND + k, &cx, &cy, &d)) continue;
        int left = G.s.left[k];
        int how = UI_IVORY | (k == kind_sel && left ? UI_ON : 0) | (left ? 0 : UI_DIM);
        ui_tool_paint(T_KIND + k, paint_kind, k, 0.8f, how);
        static const uint8_t cream[3] = {246, 238, 220}, ink[3] = {40, 28, 20};
        float nh = 0.36f * d, nx = cx + 0.36f * d, ny = cy + 0.34f * d;
        pzk_dot(nx, ny, 0.42f * d, 1, (pzk_rgba){ink[0], ink[1], ink[2], 220}, 0);
        ui_number_ex(nx, ny, nh, left, cream, (Uint8)(left ? 255 : 120), UI_NUM_TIGHT);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int cell_at(float x, float y)            /* the square under the finger (a piece standing: its top too) */
{
    float fx = (x - bx0) / q, fy = (y - by0) / q;
    int cx = (int)floorf(fx), cy = (int)floorf(fy);
    if (cx >= 0 && cy >= 0 && cx < G.m.w && cy < G.m.h) {
        int c = cx + cy * G.m.w, o;
        if (G.m.t[c] != DMC_FELT || dmc_piece(&G.m, &G.s, c, &o) >= DMC_DOMINO) return c;
    }
    int up = (int)floorf(fy + 0.5f);            /* a tap on a standing piece's upper part */
    if (cx >= 0 && up >= 0 && cx < G.m.w && up < G.m.h) {
        int c = cx + up * G.m.w, o;
        if (dmc_piece(&G.m, &G.s, c, &o) >= DMC_DOMINO) return c;
    }
    return cx >= 0 && cy >= 0 && cx < G.m.w && cy < G.m.h ? cx + cy * G.m.w : -1;
}

static void tap_cell(int c)
{
    if (c < 0 || G.done) return;
    if (G.s.t[c] != DMC_WOOD) {                 /* a piece put: turned, then back to the stock */
        int t = G.s.t[c], ways = t == DMC_TURN ? 4 : t == DMC_SPLIT ? 1 : 2;
        if (++spin[c] >= ways) {
            dmc_take(&G, c);
            spin[c] = 0;
            ui_sound(SND_TAP);
        } else {
            dmc_turn_piece(&G, c, t == DMC_TURN ? (G.s.o[c] + 1) & 3 : !G.s.o[c]);
            ui_sound(SND_MOVE);
        }
        pick_kind();
        return;
    }
    if (!dmc_free(&G.m, &G.s, c)) { ui_sound(SND_DENY); return; }
    pick_kind();
    if (!G.s.left[kind_sel]) { ui_sound(SND_DENY); return; }
    if (dmc_put(&G, c, kind_sel, auto_way(c, kind_sel))) {
        spin[c] = 0;
        ui_sound(SND_PLACE);
        pick_kind();
    } else ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    tool_down = ui_tool_hit(x, y);
    ui_animate();
}

static void motion(float x, float y) { (void)x; (void)y; }

static void release(float x, float y)
{
    if (busy()) { tool_down = -1; return; }
    if (tool_down >= 0) {
        int k = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) != k) return;
        if (k == T_PUSH) push();
        else if (k == T_HINT) hint();
        else if (G.done) ui_sound(SND_DENY);
        else if (G.s.left[k - T_KIND]) { kind_sel = k - T_KIND; ui_sound(SND_TAP); }
        else ui_sound(SND_DENY);
        return;
    }
    tap_cell(cell_at(x, y));
    ui_animate();
}

static void key(SDL_Keycode k)
{
    if (k == SDLK_SPACE || k == SDLK_RETURN) push();
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two(b, NULL, G.s.placed, t_small, par(), G.done && G.s.placed <= par());
}

/* ---- the shell's pictures: the board and its pieces seen from above ---------------------------------------------- */
static void paint_map(const dmc_map *m, int size, uint8_t *rgba)
{
    int s = (int)(size * 0.94f / (fmaxf((float)m->w, (float)m->h) + 2 * FRAME)), f = (int)(FRAME * s);
    if (s < 3) return;
    int W = m->w * s + 2 * f, H = m->h * s + 2 * f;
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    paint_board(m, s, f, o);
    for (int c = 0; c < m->w * m->h; c++) {     /* each piece from above: a bar across its way, a curve, a block */
        int t = m->t[c], d = m->o[c];
        float x = f + c % m->w * s, y = f + c / m->w * s, cx = x + s / 2.0f, cy = y + s / 2.0f;
        const uint8_t *col = t == DMC_START ? RED : IVORY;
        if (t == DMC_DOMINO || t == DMC_LONG || t == DMC_START) {
            int across = t == DMC_START ? d & 1 : d;   /* falling across: thin along x */
            float hw = across ? 0.1f * s : 0.32f * s, hh = across ? 0.32f * s : 0.1f * s;
            if (t == DMC_START) { hw *= 1.1f; hh *= 1.1f; }
            float ax = hw > hh ? hw - hh : 0, ay = hh > hw ? hh - hw : 0;
            pzk_stroke(o, W, H, cx - ax, cy - ay, cx + ax, cy + ay, fminf(hw, hh) + 0.5f, col, 1);
            if (t == DMC_LONG) pzk_ball(o, W, H, cx, cy, 0.08f * s, BRASS, 1, 0);
        } else if (t == DMC_TURN) {
            static const float CX[4] = {1, 1, 0, 0}, CY[4] = {0, 1, 1, 0};
            static const float A0[4] = {180, 90, 0, 270};   /* the quarter inside the square, from its corner */
            float a0 = A0[d & 3];
            pzk_arc(o, W, H, x + CX[d & 3] * s, y + CY[d & 3] * s, 0.42f * s, 0.58f * s, a0, a0 + 90, col, 1);
        } else if (t == DMC_SPLIT) {
            pzk_ball(o, W, H, cx, cy, 0.24f * s, col, 1, 0.5f);
            pzk_ball(o, W, H, cx, cy, 0.1f * s, BRASS, 1, 0);
        } else if (t == DMC_BELL) {
            pzk_ball(o, W, H, cx, cy, 0.3f * s, BRASS, 1, 1);
            pzk_ball(o, W, H, cx, cy, 0.09f * s, WALNUT, 1, 0);
        }
    }
    art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    free(o);
}

/* the menu's card: a chain falling, seen from above at a slant: dominoes tipping one after the other */
static void thumb(int variant, int size, uint8_t *rgba)
{
    dmc_game *t = malloc(sizeof *t);
    if (!t) return;
    if (variant < 0) dmc_level(t, 30);
    else dmc_random(t, variant, 7u + (uint32_t)variant);
    paint_map(&t->m, size, rgba);
    free(t);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    dmc_game *t = malloc(sizeof *t);
    if (t && dmc_level(t, level)) paint_map(&t->m, size, rgba);
    free(t);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const ICON[DMC_CHAPTERS] = {
        "Rh.hB", "Rh2|#.v|#.B", "RH#hB", "#B#|#v#|.+.|#v#|#U#", "RH#3|1+.v|B##v", "Rh2#|##+3|B1#v|##4.",
    };
    dmc_map m;
    if (chapter >= 0 && chapter < DMC_CHAPTERS && dmc_parse(&m, ICON[chapter], "0000")) paint_map(&m, size, rgba);
}

static const game dominochain_game = {
    .id = "dominochain", .order = 459, .variants = DMC_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = DMC_LEVELS, .chapters = DMC_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1,
    .tools = T_KIND + DMC_KINDS,
};
REGISTER_GAME(dominochain_game)
