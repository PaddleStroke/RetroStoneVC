/*
 * Tracks in RetroStone Board, for one player: the house plank ruled in cells, the counts burned along a darker band
 * above the columns and left of the rows, the track as wooden sleepers and brass rails, the given pieces on darker
 * inlaid cells, the line running off the board at the entry and the exit (the rules: tracks_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The shell's level screen chooses a level (150 in six chapters, tracks_levels.h), or its die tab a puzzle made at
 * random (the variants). Stars: no hint and no mistake 3, up to two of them 2, else 1 (tracks.h, pzkit.h).
 * Drag from cell to cell to lay the track along the finger (it joins the pieces already there). Tap a cell for a
 * piece (the one that best meets its neighbours first), again for the next, and after the sixth the cell is empty
 * again. Hold a cell a moment to cross it (no track here); the cross tool makes taps and drags cross instead.
 * A count is faded when its line has that many pieces, red past it. The board takes the room the shell gives
 * (game.fill); the shell's tools: the hint (one cell set right, in gold light) and the cross mode. The bar: the
 * moves. Undo takes back one action, all the way to the start.
 * Solved: a little train runs the line from the entry to the exit, the cells lighting gold behind it.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/tracks.h"
#include "games/tracks_levels.h"
#include "shell.h"

#define FRAME 0.035f
#define BAND 0.95f                              /* the counts' bands, in cells */
#define HOLD_MS 420
#define HINT_MS 1400
#define RIDE_MS 3400                            /* the train, entry to exit */
#define GLOW_LEN 700

static const uint8_t SLEEPER[3] = {92, 60, 40}, SLEEPER_GIVEN[3] = {58, 38, 28}, RAIL[3] = {222, 182, 92},
                     CLUE[3] = {44, 30, 20}, OVER[3] = {184, 44, 34}, EBONY[3] = {46, 38, 34};
static const int SIDES[4] = {TK_N, TK_E, TK_S, TK_W};
static const int PIECE_OF[6] = {TK_E | TK_W, TK_N | TK_S, TK_N | TK_E, TK_E | TK_S, TK_S | TK_W, TK_N | TK_W};

static tk_game G, P;                            /* P: the game as a stroke being drawn would leave it */
static int serial, crossing;
static struct { int on, moved, len, path[TK_CELLS]; float x0, y0; Uint32 t0; } ptr;
static struct { int cell; Uint32 t0; } hint = {-1, 0};
static struct { int on; Uint32 t0; } win;
static int route[TK_CELLS + 2], route_in[TK_CELLS + 2], nroute;   /* the line in order, each cell's way in */

static box last_box;
static float bx, by, side, gx, gy;
static int fr, cs, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_piece[2][6], *t_cross, *t_loco, *t_wagon[2], *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a piece in a q x q picture: sleepers across the line, two brass rails along it */
static void paint_piece(uint8_t *o, int q, int piece, int given)
{
    float Q = (float)q, g = 0.17f * Q, h = 0.29f * Q;
    const uint8_t *sl = given ? SLEEPER_GIVEN : SLEEPER;
    int e[2], ne = 0;
    for (int i = 0; i < 4; i++)
        if (piece & SIDES[i]) e[ne++] = i;
    if (ne != 2) return;
    static const float DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
    if (e[1] - e[0] == 2) {                     /* straight */
        int vert = e[0] == 0;
        for (int k = 0; k < 4; k++) {
            float t = (k + 0.5f) / 4 * Q;
            if (vert) pzk_stroke(o, q, q, 0.5f * Q - h, t, 0.5f * Q + h, t, 0.055f * Q, sl, 1);
            else pzk_stroke(o, q, q, t, 0.5f * Q - h, t, 0.5f * Q + h, 0.055f * Q, sl, 1);
        }
        for (int s = -1; s <= 1; s += 2) {
            float off = 0.5f * Q + s * g;
            for (int pass = 0; pass < 3; pass++) {
                float r = pass == 0 ? 0.042f * Q : pass == 1 ? 0.028f * Q : 0.01f * Q, d = pass == 2 ? -0.01f * Q : 0;
                const uint8_t *c = pass == 0 ? PZK_DARK : pass == 1 ? RAIL : PUZ_CREAM;
                if (vert) pzk_stroke(o, q, q, off + d, -Q, off + d, 2 * Q, r, c, pass == 2 ? 0.7f : 1);
                else pzk_stroke(o, q, q, -Q, off + d, 2 * Q, off + d, r, c, pass == 2 ? 0.7f : 1);
            }
        }
        return;
    }
    /* a curve round the corner the two sides share */
    float kx = 0.5f * Q * (1 + DX[e[0]] + DX[e[1]]), ky = 0.5f * Q * (1 + DY[e[0]] + DY[e[1]]);
    float a0 = 180;                             /* the quarter of the turn facing the cell (degrees, y up) */
    if (kx <= 0 && ky <= 0) a0 = 270;
    if (kx <= 0 && ky > 0) a0 = 0;
    if (kx > 0 && ky > 0) a0 = 90;
    float R = 0.5f * Q;
    for (int k = 0; k < 3; k++) {
        float a = (a0 + 15 + 30 * k) * PUZ_PI / 180, c = cosf(a), s = -sinf(a);
        pzk_stroke(o, q, q, kx + c * (R - h), ky + s * (R - h), kx + c * (R + h), ky + s * (R + h), 0.055f * Q, sl, 1);
    }
    for (int sg = -1; sg <= 1; sg += 2) {
        float r = R + sg * g;
        pzk_arc(o, q, q, kx, ky, r - 0.042f * Q, r + 0.042f * Q, a0 - 2, a0 + 92, PZK_DARK, 1);
        pzk_arc(o, q, q, kx, ky, r - 0.028f * Q, r + 0.028f * Q, a0 - 2, a0 + 92, RAIL, 1);
        pzk_arc(o, q, q, kx, ky, r - 0.008f * Q, r + 0.012f * Q, a0 - 2, a0 + 92, PUZ_CREAM, 0.6f);
    }
}

/* a cross: two strokes of ink */
static void paint_cross(uint8_t *o, int q)
{
    float Q = (float)q;
    pzk_stroke(o, q, q, 0.3f * Q, 0.3f * Q, 0.7f * Q, 0.7f * Q, 0.05f * Q, EBONY, 0.85f);
    pzk_stroke(o, q, q, 0.7f * Q, 0.3f * Q, 0.3f * Q, 0.7f * Q, 0.05f * Q, EBONY, 0.85f);
}

/* the train seen from above, facing east: the engine (a boiler, its brass bands, the cab) or a wagon */
static void paint_car(uint8_t *o, int w, int h, int kind)
{
    float W = (float)w, H = (float)h;
    const uint8_t red[3] = {176, 48, 38}, jade[3] = {58, 128, 98}, lapis[3] = {52, 84, 150};
    const uint8_t *body = kind == 0 ? red : kind == 1 ? jade : lapis;
    for (int y = 0; y < h; y++)                 /* the chassis: a dark rounded plank */
        for (int x = 0; x < w; x++) {
            float px = x + 0.5f, py = y + 0.5f, rad = 0.2f * H;
            float dx = fmaxf(rad - px, px - (W - rad)), dy = fmaxf(rad - py, py - (H - rad)), d;
            if (dx > 0 && dy > 0) d = rad - hypotf(dx, dy);
            else d = fminf(fminf(px, W - px), fminf(py, H - py));
            pzk_over(o + 4 * (y * w + x), PZK_DARK, clampf(d + 0.5f, 0, 1));
        }
    if (kind == 0) {
        pzk_stroke(o, w, h, 0.2f * W, 0.5f * H, 0.8f * W, 0.5f * H, 0.3f * H, EBONY, 1);   /* the boiler */
        pzk_stroke(o, w, h, 0.22f * W, 0.42f * H, 0.78f * W, 0.42f * H, 0.07f * H, PUZ_CREAM, 0.25f);
        for (int k = 0; k < 3; k++) {
            float x = (0.45f + 0.14f * k) * W;
            pzk_stroke(o, w, h, x, 0.22f * H, x, 0.78f * H, 0.05f * H, RAIL, 1);
        }
        pzk_ball(o, w, h, 0.78f * W, 0.5f * H, 0.13f * H, EBONY, 1, 0.8f);   /* the chimney */
        for (int y = (int)(0.1f * H); y < (int)(0.9f * H); y++)   /* the cab */
            for (int x = (int)(0.06f * W); x < (int)(0.36f * W); x++) {
                float k = 0.85f + 0.3f * (1 - (float)y / H);
                uint8_t c[3] = {(uint8_t)clampf(red[0] * k, 0, 255), (uint8_t)clampf(red[1] * k, 0, 255),
                                (uint8_t)clampf(red[2] * k, 0, 255)};
                pzk_over(o + 4 * (y * w + x), c, 1);
            }
        pzk_stroke(o, w, h, 0.21f * W, 0.14f * H, 0.21f * W, 0.86f * H, 0.03f * H, PZK_DARK, 0.6f);
        return;
    }
    for (int y = (int)(0.12f * H); y < (int)(0.88f * H); y++)    /* a wagon's box, lit from the top */
        for (int x = (int)(0.08f * W); x < (int)(0.92f * W); x++) {
            float k = 0.8f + 0.4f * (1 - (float)y / H);
            uint8_t c[3] = {(uint8_t)clampf(body[0] * k, 0, 255), (uint8_t)clampf(body[1] * k, 0, 255),
                            (uint8_t)clampf(body[2] * k, 0, 255)};
            pzk_over(o + 4 * (y * w + x), c, 1);
        }
    for (int k = 1; k < 4; k++)
        pzk_stroke(o, w, h, (0.08f + 0.21f * k) * W, 0.14f * H, (0.08f + 0.21f * k) * W, 0.86f * H, 0.025f * H, PZK_DARK,
                   0.5f);
}

/* the board: the plank, the counts' bands, the cells ruled, the given cells inlaid darker */
static void paint_board(uint8_t *o, int S, int f, const tk_game *g, float ox, float oy, int q)
{
    int n = g->n;
    art_board_n(1, 1, S - 2 * f, f, 0, o);
    float gw = (float)n * q, band = BAND * q;
    for (int y = f; y < S - f; y++)
        for (int x = f; x < S - f; x++) {
            int in_row = y >= oy && y < oy + gw && x >= ox - band && x < ox;
            int in_col = x >= ox && x < ox + gw && y >= oy - band && y < oy;
            if (!in_row && !in_col) continue;
            uint8_t *p = o + 4 * (y * S + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * 0.86f);
        }
    const float tone[3] = {0.8f, 0.76f, 0.74f};
    for (int c = 0; c < n * n; c++)
        if (g->given[c])
            pzk_inlay(o, S, S, ox + c % n * q + 0.04f * q, oy + c / n * q + 0.04f * q, 0.92f * q, 0.92f * q, 0.1f * q,
                      tone, fmaxf(1, 0.03f * q));
    for (int i = 0; i <= n; i++) {
        float t = i == 0 || i == n ? fmaxf(2, 0.05f * q) : fmaxf(1, 0.02f * q), a = i == 0 || i == n ? 0.8f : 0.32f;
        float p = i * q, lo = p - t / 2, hi = p + t / 2;
        if (i == 0) lo = 0, hi = t;
        if (i == n) lo = gw - t, hi = gw;
        puz_ink_rect(o, S, S, ox + lo, oy, ox + hi, oy + gw, PUZ_INK, a);
        puz_ink_rect(o, S, S, ox, oy + lo, ox + gw, oy + hi, PUZ_INK, a);
    }
}

/* the token: a curve of track on a wooden tile */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.9f), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.05f * w), 0.3f);
    uint8_t *p = calloc((size_t)w * w, 4);
    if (p) {
        paint_piece(p, w, TK_S | TK_E, 0);
        art_blend(b, w, w, p, w, w, 0, 0);
        free(p);
    }
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

/* the cross button's picture (what a tap does): kind 1 a cross, 2 a piece of track */
static void paint_mode(int kind, int size, uint8_t *o)
{
    float s = (float)size;
    if (kind == 1) {
        pzk_stroke(o, size, size, 0.33f * s, 0.33f * s, 0.67f * s, 0.67f * s, 0.05f * s, PUZ_CREAM, 1);
        pzk_stroke(o, size, size, 0.67f * s, 0.33f * s, 0.33f * s, 0.67f * s, 0.05f * s, PUZ_CREAM, 1);
    } else {
        for (int k = 0; k < 4; k++) {          /* a piece of track: pale sleepers, brass rails */
            float x = (0.3f + 0.133f * k) * s;
            pzk_stroke(o, size, size, x, 0.32f * s, x, 0.68f * s, 0.032f * s, PUZ_CREAM, 0.6f);
        }
        pzk_stroke(o, size, size, 0.22f * s, 0.42f * s, 0.78f * s, 0.42f * s, 0.026f * s, PZK_GOLD, 1);
        pzk_stroke(o, size, size, 0.22f * s, 0.58f * s, 0.78f * s, 0.58f * s, 0.026f * s, PZK_GOLD, 1);
    }
}

static SDL_Texture *piece_tex(int q, int piece, int given)
{
    uint8_t *b = ui_buf(q, q);
    memset(b, 0, (size_t)q * q * 4);
    paint_piece(b, q, piece, given);
    return ui_tex(q, q, b);
}

static void layout(box b)
{
    last_box = b;
    float S = floorf(fminf(b.w, b.h));
    side = S;
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    fr = (int)(S * FRAME);
    int n = G.n ? G.n : 6;
    float inner = S - 2 * fr;
    cs = (int)(inner / (n + BAND + 0.45f));
    float ox = floorf(fr + (inner - (n + BAND + 0.45f) * cs) / 2 + BAND * cs + 0.1f * cs);
    gx = bx + ox;
    gy = by + ox;
    int key = ((int)S * 64 + serial % 64) * 16 + n;
    if (made_key != key) {
        made_key = key;
        int w = (int)S;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), w, fr, &G, ox, ox, cs);
        t_board = ui_tex(w, w, ui_buf(w, w));
        for (int k = 0; k < 2; k++)
            for (int i = 0; i < 6; i++) {
                ui_free(&t_piece[k][i]);
                t_piece[k][i] = piece_tex(cs, PIECE_OF[i], k);
            }
        ui_free(&t_cross);
        memset(ui_buf(cs, cs), 0, (size_t)cs * cs * 4);
        paint_cross(ui_buf(cs, cs), cs);
        t_cross = ui_tex(cs, cs, ui_buf(cs, cs));
        int cw = (int)(0.86f * cs), ch = (int)(0.5f * cs);
        SDL_Texture **cars[3] = {&t_loco, &t_wagon[0], &t_wagon[1]};
        for (int i = 0; i < 3; i++) {
            ui_free(cars[i]);
            memset(ui_buf(cw, ch), 0, (size_t)cw * ch * 4);
            paint_car(ui_buf(cw, ch), cw, ch, i);
            *cars[i] = ui_tex(cw, ch, ui_buf(cw, ch));
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
    hint.cell = -1;
    win.on = 0;
}

/* the solution's line from the entry: its cells in order, each one's way in; how many */
static int line_of(const tk_game *g, int *cells, int *ins)
{
    int n = g->n, c = g->a, from = g->a_side, len = 0;
    for (int k = 0; k < n * n && c >= 0; k++) {
        cells[len] = c;
        ins[len++] = from;
        int out = g->sol[c] & ~from;
        if (c == g->b) break;
        int x = c % n + (out == TK_E) - (out == TK_W), y = c / n + (out == TK_S) - (out == TK_N);
        c = x < 0 || y < 0 || x >= n || y >= n ? -1 : x + y * n;
        from = tk_opp(out);
    }
    return len;
}

static void find_route(void) { nroute = line_of(&G, route, route_in); }

static void after(void)
{
    if (tk_solved(&G) && !win.on) {
        win.on = 1;
        win.t0 = ui_ms();
        crossing = 0;
    }
    ui_animate();
}

static void begin(void)
{
    serial++;
    calm();
    crossing = 0;
    find_route();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    if (!tk_decode(&G, tk_levels[level < 0 || level >= TK_LEVELS ? 0 : level])) tk_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    tk_new(&G, variant < 0 || variant >= TK_VARIANTS ? 0 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int score(void) { return G.moves; }
static int level_stars(void) { return pzk_stars(3, G.hints, G.mistakes); }

static int status(void) { return tk_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static Uint32 win_end(void) { return RIDE_MS + GLOW_LEN + 300; }
static int busy(void) { return win.on && ui_ms() - win.t0 < win_end(); }

static int undo(void)
{
    int c = tk_undo(&G);
    if (c < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = tk_script(&G, moves);
    after();
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static int cell_at(float x, float y)
{
    float ix = x - gx, iy = y - gy;
    if (ix < 0 || iy < 0 || ix >= G.n * cs || iy >= G.n * cs) return -1;
    return (int)(iy / cs) * G.n + (int)(ix / cs);
}

static int piece_index(int v)
{
    for (int i = 0; i < 6; i++)
        if (PIECE_OF[i] == v) return i;
    return -1;
}

/* a stub of the line off the board at a cell's side: a part of a straight piece beyond the grid's edge */
static void draw_stub(int c, int sd)
{
    int n = G.n, vert = sd == TK_N || sd == TK_S, i = piece_index(vert ? TK_N | TK_S : TK_E | TK_W);
    float x = gx + c % n * cs, y = gy + c / n * cs, k = 0.22f;
    SDL_Rect src = {0, 0, cs, cs};
    SDL_FRect dst = {x, y, (float)cs, (float)cs};
    if (sd == TK_N) src.h = (int)(k * cs), dst.y = y - k * cs, dst.h = k * cs;
    if (sd == TK_S) src.y = cs - (int)(k * cs), src.h = (int)(k * cs), dst.y = y + cs, dst.h = k * cs;
    if (sd == TK_W) src.w = (int)(k * cs), dst.x = x - k * cs, dst.w = k * cs;
    if (sd == TK_E) src.x = cs - (int)(k * cs), src.w = (int)(k * cs), dst.x = x + cs, dst.w = k * cs;
    SDL_RenderCopyF(ui.r, t_piece[1][i], &src, &dst);
}

/* where the train is, s cells along the line from the entry's edge: the point and the heading (degrees) */
static void ride_at(float s, float *x, float *y, float *deg)
{
    static const float DX[16] = {[TK_N] = 0, [TK_E] = 1, [TK_S] = 0, [TK_W] = -1},
                       DY[16] = {[TK_N] = -1, [TK_E] = 0, [TK_S] = 1, [TK_W] = 0};
    int i = (int)floorf(s);
    if (i < 0) i = 0;
    if (i >= nroute) i = nroute - 1;
    float t = clampf(s - i, 0, 1);
    int c = route[i], in = route_in[i], out = G.sol[c] & ~in, n = G.n;
    float cx = gx + (c % n + 0.5f) * cs, cy = gy + (c / n + 0.5f) * cs, h = 0.5f * cs;
    float ax = cx + DX[in] * h, ay = cy + DY[in] * h, ex = cx + DX[out] * h, ey = cy + DY[out] * h;
    if (in == tk_opp(out)) {
        *x = ax + (ex - ax) * t;
        *y = ay + (ey - ay) * t;
        *deg = atan2f(ey - ay, ex - ax) * 180 / PUZ_PI;
        return;
    }
    float kx = cx + (DX[in] + DX[out]) * h, ky = cy + (DY[in] + DY[out]) * h;
    float a0 = atan2f(ay - ky, ax - kx), a1 = atan2f(ey - ky, ex - kx), da = a1 - a0;
    if (da > PUZ_PI) da -= 2 * PUZ_PI;
    if (da < -PUZ_PI) da += 2 * PUZ_PI;
    float a = a0 + da * t;
    *x = kx + cosf(a) * h;
    *y = ky + sinf(a) * h;
    *deg = (a + (da > 0 ? PUZ_PI / 2 : -PUZ_PI / 2)) * 180 / PUZ_PI;
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    int n = G.n, solved = tk_solved(&G);
    const tk_game *V = ptr.on && ptr.moved && !crossing ? &P : &G;   /* a stroke drawn: shown as it will be */
    ui_put(t_board, bx, by, side, side);
    if (ptr.on && !solved) {                    /* the lines touched: their counts lit */
        int a = ptr.path[ptr.len ? ptr.len - 1 : 0];
        ui_mark_last(gx - BAND * cs, gy + a / n * cs, BAND * cs, (float)cs);
        ui_mark_last(gx + a % n * cs, gy - BAND * cs, (float)cs, BAND * cs);
    }
    if (hint.cell >= 0) {
        float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
        if (p > 0) {
            ui_mark_gold(gx + (hint.cell % n + 0.5f) * cs, gy + (hint.cell / n + 0.5f) * cs, 2.4f * cs * p);
            ui_animate();
        } else {
            hint.cell = -1;
        }
    }
    draw_stub(G.a, G.a_side);
    draw_stub(G.b, G.b_side);
    float wt = win.on ? (float)(now - win.t0) : -1, speed = (nroute + 2.2f) / RIDE_MS;
    for (int c = 0; c < n * n; c++) {
        float x = gx + c % n * cs, y = gy + c / n * cs;
        int v = G.given[c] ? G.sol[c] : V->cell[c];
        if (v == TK_X) {
            if (!solved) ui_put(t_cross, x, y, (float)cs, (float)cs);
            continue;
        }
        int i = piece_index(v & 15);
        if (i >= 0) ui_put(t_piece[G.given[c]][i], x, y, (float)cs, (float)cs);
    }
    if (ptr.on && !ptr.moved && !solved) {      /* a press growing into a cross: its ring */
        float t = (now - ptr.t0) / (float)HOLD_MS;
        int a = ptr.path[0];
        if (t > 0.25f && t < 1 && !crossing && !G.given[a])
            ui_mark_select_round(gx + (a % n + 0.5f) * cs, gy + (a / n + 0.5f) * cs, cs * (0.6f + 0.8f * t));
        if (t < 1) ui_animate();
    }
    for (int way = 0; way < 2; way++)           /* the counts */
        for (int i = 0; i < n; i++) {
            int clue = way ? G.cols[i] : G.rows[i], have = tk_line_count(V, way, i);
            float x = way ? gx + (i + 0.5f) * cs : gx - 0.62f * BAND * cs, y = way ? gy - 0.6f * BAND * cs
                                                                                  : gy + (i + 0.5f) * cs;
            ui_number_ex(x, y, 0.5f * cs, clue, have > clue ? OVER : CLUE, have == clue ? 95 : 255, UI_NUM_TIGHT);
        }
    if (wt >= 0) {                              /* the win: the train along the line, the cells lit behind it */
        float s = wt * speed;
        for (int k = 0; k < nroute; k++) {
            float p = puz_pulse(wt, (k + 0.6f) / speed, GLOW_LEN);
            if (p > 0) ui_mark_gold(gx + (route[k] % n + 0.5f) * cs, gy + (route[k] / n + 0.5f) * cs, 2.2f * cs * p);
        }
        SDL_Texture *cars[3] = {t_loco, t_wagon[0], t_wagon[1]};
        for (int k = 2; k >= 0; k--) {
            float at = s - 0.92f * k;
            if (at < 0.05f || at > nroute - 0.05f) continue;
            float x, y, deg;
            ride_at(at, &x, &y, &deg);
            ui_shadow_flat(x + 0.05f * cs, y + 0.07f * cs, 0.8f * cs, 0.55f * cs, 0.5f);
            ui_put_rot(cars[k], x, y, 0.86f * cs, 0.5f * cs, deg);
        }
        for (int k = 0; k < 5; k++) {           /* the steam, drifting back from the chimney */
            float at = s - 0.25f - 0.3f * k;
            if (at < 0.05f || at > nroute - 0.05f) continue;
            float x, y, deg;
            ride_at(at, &x, &y, &deg);
            float wob = sinf(wt / 90.0f + k) * 0.06f * cs;
            pzk_dot(x + wob, y - wob, (0.16f + 0.07f * k) * cs, 0.12f * cs,
                    (pzk_rgba){246, 238, 220, (Uint8)(105 - 18 * k)}, 0);
        }
        if (wt < win_end()) ui_animate();
        else win.on = 0;
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void preview(void)
{
    P = G;
    if (ptr.len >= 2) tk_lay(&P, ptr.path, ptr.len);
}

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
    if (tk_solved(&G)) return;
    int c = cell_at(x, y);
    if (c < 0) return;
    ptr.on = 1;
    ptr.path[0] = c;
    ptr.len = 1;
    ptr.x0 = x;
    ptr.y0 = y;
    ptr.t0 = ui_ms();
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    if (!ptr.moved && hypotf(x - ptr.x0, y - ptr.y0) > 0.35f * cs) ptr.moved = 1;
    if (!ptr.moved) return;
    int n = G.n, tx = (int)floorf((x - gx) / cs), ty = (int)floorf((y - gy) / cs);
    tx = tx < 0 ? 0 : tx >= n ? n - 1 : tx;
    ty = ty < 0 ? 0 : ty >= n ? n - 1 : ty;
    for (int guard = 0; guard < 2 * n; guard++) {   /* a cell at a time towards the finger */
        int last = ptr.path[ptr.len - 1], lx = last % n, ly = last / n;
        if (lx == tx && ly == ty) break;
        if (abs(tx - lx) >= abs(ty - ly)) lx += tx > lx ? 1 : -1;
        else ly += ty > ly ? 1 : -1;
        int d = lx + ly * n;
        if (ptr.len >= 2 && ptr.path[ptr.len - 2] == d) { ptr.len--; continue; }   /* back over the last cell */
        int seen = 0;
        for (int i = 0; i < ptr.len; i++) seen |= ptr.path[i] == d;
        if (seen || ptr.len >= TK_CELLS) break;
        ptr.path[ptr.len++] = d;
    }
    preview();
    ui_animate();
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!ptr.on) return;
    ptr.on = 0;
    ui_animate();
    int c = ptr.path[0];
    if (!ptr.moved) {                           /* a tap, or a hold */
        if (G.given[c]) { ui_sound(SND_DENY); return; }
        int hold = ui_ms() - ptr.t0 >= HOLD_MS, ok;
        if (crossing || hold) ok = tk_set(&G, c, G.cell[c] == TK_X ? 0 : TK_X);
        else ok = tk_tap(&G, c);
        if (!ok) { ui_sound(SND_DENY); return; }
        ui_sound(G.cell[c] && G.cell[c] != TK_X ? SND_PLACE : SND_TAP);
        after();
        return;
    }
    int ok = crossing ? tk_mark(&G, ptr.path, ptr.len) : tk_lay(&G, ptr.path, ptr.len);
    if (!ok) return;
    ui_sound(crossing ? SND_TAP : SND_PLACE);
    after();
}

/* ---- the bar: the moves, the cross button, the hint's lamp ------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    ui_count c[1] = {{t_small, G.moves}};
    ui_bar_fit(b, c, 1, 0);
}

static void use_hint(void)
{
    int c = tk_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    hint.cell = c;
    hint.t0 = ui_ms();
    ui_sound(SND_PLACE);
    after();
}

static void use_tool(int t)              /* 0 the hint, 1 the cross mode */
{
    if (t == 0) { use_hint(); return; }
    if (tk_solved(&G)) { ui_sound(SND_DENY); return; }
    crossing = !crossing;
    ui_sound(SND_TAP);
    ui_animate();
}

/* ---- the menu's and the picker's cards: a board of the size, its counts, part of its line laid ------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant;
    static tk_game T;
    tk_new(&T, v, 4242u + (uint32_t)v);
    int n = T.n, f = (int)(size * FRAME);
    float inner = (float)(size - 2 * f);
    int q = (int)(inner / (n + BAND + 0.45f));
    float ox = floorf(f + (inner - (n + BAND + 0.45f) * q) / 2 + BAND * q + 0.1f * q);
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b) return;
    paint_board(b, size, f, &T, ox, ox, q);
    uint8_t *p = malloc((size_t)q * q * 4), shown[TK_CELLS] = {0};
    int cells[TK_CELLS], ins[TK_CELLS], len = line_of(&T, cells, ins);
    for (int i = 0; i < len; i++) shown[cells[i]] = 3 * i < (variant < 0 ? 2 : 1) * len;   /* the line begun */
    for (int c = 0; c < n * n && p; c++) {
        if (!T.sol[c] || !(shown[c] || T.given[c])) continue;
        memset(p, 0, (size_t)q * q * 4);
        paint_piece(p, q, T.sol[c], T.given[c]);
        art_blend(b, size, size, p, q, q, (int)(ox + c % n * q), (int)(ox + c / n * q));
    }
    free(p);
    if (q >= 9)
        for (int way = 0; way < 2; way++)
            for (int i = 0; i < n; i++) {
                float x = way ? ox + (i + 0.5f) * q : ox - 0.62f * BAND * q, y = way ? ox - 0.6f * BAND * q
                                                                                    : ox + (i + 0.5f) * q;
                puz_digits(b, size, size, way ? T.cols[i] : T.rows[i], x, y, 0.5f * q, CLUE, 1, 1);
            }
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
}

/* a chapter's tab: a small board of its biggest size */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const int SIZE[TK_CHAPTERS] = {6, 7, 8, 9, 10, 10};
    pzk_grid_icon(rgba, size, SIZE[chapter < 0 ? 0 : chapter >= TK_CHAPTERS ? TK_CHAPTERS - 1 : chapter]);
}

/* the board, then the shell's tools: the hint, the cross mode */
static void draw_all(void)
{
    draw();
    ui_tool(0, ICON_HINT, tk_solved(&G) ? UI_DIM : 0);
    ui_tool_paint(1, paint_mode, crossing ? 1 : 2, 1, tk_solved(&G) ? UI_DIM : crossing ? UI_ON : 0);
}

static const game tracks_game = {
    .id = "tracks", .order = 403, .variants = TK_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw_all,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = 2,
    .levels = TK_LEVELS, .chapters = TK_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .level_stars = level_stars, .score = score,
};
REGISTER_GAME(tracks_game)
