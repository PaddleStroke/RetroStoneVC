/*
 * Nonograms in RetroStone Board, for one player: the house plank ruled in squares (a thicker line every five), the
 * clues burned along a darker band to the left of the rows and above the columns, the squares filled in ebony ink or
 * crossed (the rules: nonogram_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a square to fill it (again: empty); hold it a moment to cross it instead. The button at the right end of the
 * bar turns taps into crosses and back (it shows what a tap does: an ebony square, or a cross). Drag along a row or a
 * column to do the same to the whole run: the squares in the state the first one was (a fill run passes over the
 * crosses). A line whose filled squares make its clue has its clue faded. Undo takes back one tap or run, all the way
 * to the start. The bar: the moves.
 * Solved: the crosses go, the picture comes out in its colours, and a gold light runs over it.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/nonogram.h"
#include "games/puzzles.h"
#include "shell.h"

#define FRAME 0.035f
#define SLOT_W 0.68f                            /* a row clue's number: its width, in squares */
#define SLOT_H 0.58f                            /* a column clue's number: its height */
#define HOLD_MS 420                             /* a press held this long crosses */
#define COLOUR_MS 700                           /* solved: the picture's colours coming out */
#define WAVE_STEP 40
#define WAVE_LEN 520

static const uint8_t EBONY[3] = {46, 38, 34}, CLUE[3] = {44, 30, 20};

static ng_game G;
static int crossing;                            /* taps cross (the bar's button) */
static int serial;                              /* the puzzle's number: its board remade */
static struct { int on, a, b, to, moved; float x0, y0; Uint32 t0; } ptr;
static struct { int on; Uint32 t0; } win;

static box last_box, barbox;
static float bx, by, side, gx0, gy0;            /* the board's corner; the grid's */
static int fr, cs, max_r, max_c, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_cross, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void clue_sizes(const ng_game *g, int *r, int *c)
{
    *r = *c = 1;
    for (int i = 0; i < g->n; i++) {
        if (g->runs[0][i] > *r) *r = g->runs[0][i];
        if (g->runs[1][i] > *c) *c = g->runs[1][i];
    }
}

/* the square's size for a board of side S: the grid and its clue bands fit inside the frame */
static int square_size(const ng_game *g, float S, int r, int c)
{
    float inner = S - 2 * (int)(S * FRAME) - 0.025f * S;
    return (int)fminf(inner / (g->n + SLOT_W * r + 0.15f), inner / (g->n + SLOT_H * c + 0.15f));
}

/* the board: the plank, the clue bands a shade darker, the grid's lines (thicker every five and round it) */
static void paint_board(uint8_t *o, int S, int f, int n, int q, int r, int c, float ox, float oy)
{
    art_board_n(1, 1, S - 2 * f, f, 0, o);
    float lw = SLOT_W * r * q, th = SLOT_H * c * q, gw = (float)n * q;
    for (int y = f; y < S - f; y++)            /* the bands */
        for (int x = f; x < S - f; x++) {
            int in_row = y >= oy && y < oy + gw && x >= ox - lw - 0.1f * q && x < ox;
            int in_col = x >= ox && x < ox + gw && y >= oy - th - 0.1f * q && y < oy;
            if (!in_row && !in_col) continue;
            uint8_t *p = o + 4 * (y * S + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * 0.86f);
        }
    for (int i = 0; i <= n; i++) {
        int thick = i % 5 == 0 || i == n;
        float t = thick ? fmaxf(2, 0.075f * q) : fmaxf(1, 0.03f * q), a = thick ? 0.85f : 0.45f, p = i * q;
        float lo = p - t / 2, hi = p + t / 2;
        if (i == 0) lo = 0, hi = t;
        if (i == n) lo = gw - t, hi = gw;
        puz_ink_rect(o, S, S, ox + lo, oy - (thick ? th : 0), ox + hi, oy + gw, PUZ_INK, a);   /* columns */
        puz_ink_rect(o, S, S, ox - (thick ? lw : 0), oy + lo, ox + gw, oy + hi, PUZ_INK, a);   /* rows */
    }
}

/* a cross: two strokes of ink */
static void cross_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_band(f, ART_PART, 0.24f, 0.24f, 0.76f, 0.76f, 0.11f);
    art_band(f, ART_PART, 0.76f, 0.24f, 0.24f, 0.76f, 0.11f);
    art_style st = art_flat(EBONY, EBONY, 0, 0);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

/* the bar's button: what a tap does (0 fill: an ebony square; 1 cross) */
static void paint_mode(int kind, int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    if (kind) {
        art_band(f, ART_PART, 0.32f, 0.32f, 0.68f, 0.68f, 0.1f);
        art_band(f, ART_PART, 0.68f, 0.32f, 0.32f, 0.68f, 0.1f);
        art_fig_paint(o, size, f, PUZ_CREAM, NULL, 0, 0.1f);
        return;
    }
    art_rect(f, ART_PART, 0.3f, 0.3f, 0.7f, 0.7f);
    art_fig_paint(o, size, f, EBONY, PUZ_CREAM, 0.035f, 0.1f);
}

/* the token: a little picture (the heart) on a wooden tile, its clue band */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    static const char *const heart[5] = {".r.r.", "rrrrr", "rrrrr", ".rrr.", "..r.."};
    int q = (int)(size / 6.0f), w = 5 * q + q / 2, o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    puz_bevel(b, w, w, 0.1f * w, fmaxf(1, 0.05f * w), 0.3f);
    float m = q / 4.0f;
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++)
            if (heart[y][x] != '.')
                puz_ink_rect(b, w, w, m + x * q + 0.5f, m + y * q + 0.5f, m + (x + 1) * q - 0.5f, m + (y + 1) * q - 0.5f,
                             ng_rgb[2], 1);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    float S = floorf(fminf(b.w, b.h));
    side = S;
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    fr = (int)(S * FRAME);
    clue_sizes(&G, &max_r, &max_c);
    cs = square_size(&G, S, max_r, max_c);
    float lw = SLOT_W * max_r * cs, th = SLOT_H * max_c * cs, inner = S - 2 * fr;
    float ox = floorf(fr + (inner - lw - G.n * cs) / 2 + lw), oy = floorf(fr + (inner - th - G.n * cs) / 2 + th);
    gx0 = bx + ox;
    gy0 = by + oy;
    int key = ((int)S * 64 + serial % 64) * 4 + G.n % 4;
    if (made_key != key) {
        made_key = key;
        int w = (int)S;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), w, fr, G.n, cs, max_r, max_c, ox, oy);
        t_board = ui_tex(w, w, ui_buf(w, w));
        ui_free(&t_cross);
        memset(ui_buf(cs, cs), 0, (size_t)cs * cs * 4);
        cross_pic(cs, ui_buf(cs, cs));
        t_cross = ui_tex(cs, cs, ui_buf(cs, cs));
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
    win.on = 0;
}

static void start(int variant)
{
    ng_new(&G, variant < 0 || variant >= NG_VARIANTS ? 1 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    crossing = 0;
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return ng_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static Uint32 win_end(void) { return (Uint32)(COLOUR_MS + 2 * G.n * WAVE_STEP + WAVE_LEN); }
static int busy(void) { return win.on && ui_ms() - win.t0 < win_end(); }

static void after(void)
{
    if (ng_solved(&G) && !win.on) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    ui_animate();
}

static int undo(void)
{
    calm();
    return ng_undo(&G) >= 0;
}

static int script(const char *moves)
{
    calm();
    int ok = ng_script(&G, moves);
    after();
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static int square_at(float x, float y)
{
    float ix = x - gx0, iy = y - gy0;
    if (ix < 0 || iy < 0 || ix >= G.n * cs || iy >= G.n * cs) return -1;
    return (int)(iy / cs) * G.n + (int)(ix / cs);
}

/* the run from a towards the finger's square, along a's row or column (whichever the finger has gone further along) */
static int run_end(int a, float x, float y)
{
    int n = G.n, ax = a % n, ay = a / n;
    int bxi = (int)floorf((x - gx0) / cs), byi = (int)floorf((y - gy0) / cs);
    bxi = bxi < 0 ? 0 : bxi >= n ? n - 1 : bxi;
    byi = byi < 0 ? 0 : byi >= n ? n - 1 : byi;
    return abs(bxi - ax) >= abs(byi - ay) ? ay * n + bxi : byi * n + ax;
}

static int in_run(int s, int a, int b)
{
    int n = G.n, x = s % n, y = s / n;
    int x0 = a % n < b % n ? a % n : b % n, x1 = a % n < b % n ? b % n : a % n;
    int y0 = a / n < b / n ? a / n : b / n, y1 = a / n < b / n ? b / n : a / n;
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    int n = G.n, solved = ng_solved(&G);
    ui_put(t_board, bx, by, side, side);
    float wt = win.on ? (float)(now - win.t0) : solved ? 1e9f : -1;
    float colour = wt >= 0 ? clampf(wt / COLOUR_MS, 0, 1) : 0;
    int hold = ptr.on && !ptr.moved && now - ptr.t0 >= HOLD_MS;   /* held: this press crosses */
    if (ptr.on && !solved) {                    /* the lines being touched: their clues lit */
        int a = ptr.a;
        ui_mark_last(gx0 - SLOT_W * max_r * cs, gy0 + a / n * cs, SLOT_W * max_r * cs, (float)cs);
        ui_mark_last(gx0 + a % n * cs, gy0 - SLOT_H * max_c * cs, (float)cs, SLOT_H * max_c * cs);
    }
    for (int s = 0; s < n * n; s++) {
        float x = gx0 + s % n * cs, y = gy0 + s / n * cs;
        int st = G.sq[s];
        if (ptr.on && in_run(s, ptr.a, ptr.b) && G.sq[s] == G.sq[ptr.a]) {   /* the run as it will be */
            st = hold ? (G.sq[s] == NG_CROSS ? NG_EMPTY : NG_CROSS) : ptr.to;
        }
        float in = fmaxf(1, 0.05f * cs);
        if (st == NG_FILL || (colour > 0 && G.sol[s])) {
            const uint8_t *c = ng_rgb[G.colour[s] ? G.colour[s] : 1];
            float k = colour;
            if (!G.colour[s]) k = 0;
            uint8_t r = (uint8_t)(EBONY[0] + (c[0] - EBONY[0]) * k), g = (uint8_t)(EBONY[1] + (c[1] - EBONY[1]) * k),
                    bb = (uint8_t)(EBONY[2] + (c[2] - EBONY[2]) * k);
            ui_fill(x + in, y + in, cs - 2 * in, cs - 2 * in, r, g, bb, 255);
        } else if (st == NG_CROSS && colour < 1) {
            SDL_SetTextureAlphaMod(t_cross, (Uint8)(200 * (1 - colour)));
            ui_put(t_cross, x, y, (float)cs, (float)cs);
            SDL_SetTextureAlphaMod(t_cross, 255);
        }
    }
    if (ptr.on && !ptr.moved && !solved && !hold) {   /* a press growing into a cross: its ring */
        float t = (now - ptr.t0) / (float)HOLD_MS;
        if (t > 0.25f) {
            float x = gx0 + ptr.a % n * cs + cs / 2.0f, y = gy0 + ptr.a / n * cs + cs / 2.0f;
            ui_mark_select_round(x, y, cs * (0.6f + 0.8f * t));
        }
        ui_animate();
    }
    for (int way = 0; way < 2; way++)           /* the clues: right of the row band, at the foot of the column band */
        for (int i = 0; i < n; i++) {
            int k = G.runs[way][i];
            Uint8 a = ng_line_done(&G, way, i) ? 95 : 255;
            for (int j = 0; j < k || (j == 0 && k == 0); j++) {
                int v = k ? G.clue[way][i][j] : 0, from_end = (k ? k : 1) - 1 - j;
                float x, y;
                if (way == 0) {
                    x = gx0 - (from_end + 0.5f) * SLOT_W * cs - 0.08f * cs;
                    y = gy0 + (i + 0.5f) * cs;
                } else {
                    x = gx0 + (i + 0.5f) * cs;
                    y = gy0 - (from_end + 0.5f) * SLOT_H * cs - 0.06f * cs;
                }
                ui_number_ex(x, y, 0.56f * cs * (v >= 10 ? 0.86f : 1), v, CLUE, a, UI_NUM_TIGHT);
            }
        }
    if (wt >= 0 && win.on) {                    /* the gold light over the picture, corner to corner */
        for (int s = 0; s < n * n; s++) {
            if (!G.sol[s]) continue;
            float q = puz_pulse(wt, COLOUR_MS * 0.6f + (s % n + s / n) * WAVE_STEP, WAVE_LEN);
            if (q > 0) ui_mark_gold(gx0 + (s % n + 0.5f) * cs, gy0 + (s / n + 0.5f) * cs, 2.4f * cs * q);
        }
        if (wt < win_end()) ui_animate();
        else win.on = 0;
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int target(int a, int cross)             /* what a stroke from a makes of the squares as a is */
{
    int want = cross ? NG_CROSS : NG_FILL;
    return G.sq[a] == want ? NG_EMPTY : want;
}

static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (ng_solved(&G)) return;
    int s = square_at(x, y);
    if (s < 0) return;
    ptr.on = 1;
    ptr.a = ptr.b = s;
    ptr.to = target(s, crossing);
    ptr.x0 = x;
    ptr.y0 = y;
    ptr.t0 = ui_ms();
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    if (!ptr.moved && hypotf(x - ptr.x0, y - ptr.y0) > 0.35f * cs) {
        if (ui_ms() - ptr.t0 >= HOLD_MS) ptr.to = target(ptr.a, 1);   /* held, then dragged: a run of crosses */
        ptr.moved = 1;
    }
    if (ptr.moved) ptr.b = run_end(ptr.a, x, y);
    ui_animate();
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!ptr.on) return;
    ptr.on = 0;
    int to = ptr.to;
    if (!ptr.moved && ui_ms() - ptr.t0 >= HOLD_MS) to = target(ptr.a, 1);
    if (!ng_paint(&G, ptr.a, ptr.b, to)) { ui_animate(); return; }
    ui_sound(to == NG_FILL ? SND_PLACE : SND_TAP);
    after();
}

/* ---- the bar: the moves, and what a tap does ---------------------------------------------------------------------- */
static void mode_button(float *x, float *y, float *d)
{
    *d = 0.84f * ui.unit;
    *x = barbox.x + barbox.w - *d / 2 - 0.05f * ui.unit;
    *y = barbox.y + barbox.h / 2;
}

static void bar(int player, box b)
{
    (void)player;
    barbox = b;
    ui_bar_number(b, t_small, G.moves);
    if (ng_solved(&G)) return;
    float x, y, d;
    mode_button(&x, &y, &d);
    ui_button_paint(x, y, d, paint_mode, crossing, 1, crossing ? UI_ON : 0);
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    float kx, ky, d;
    mode_button(&kx, &ky, &d);
    if (!barbox.w || ng_solved(&G) || !ui_button_hit(kx, ky, d, x, y)) return 0;
    crossing = !crossing;
    ui_sound(SND_TAP);
    ui_animate();
    return 1;
}

/* ---- the menu's and the picker's cards: a picture of the size, half found ---------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    ng_game T;
    static const int pics[NG_VARIANTS] = {0, 6, 13};
    int v = variant < 0 ? 1 : variant;
    ng_load_picture(&T, pics[v]);
    int r, c;
    clue_sizes(&T, &r, &c);
    int f = (int)(size * FRAME), q = square_size(&T, (float)size, r, c), n = T.n;
    float lw = SLOT_W * r * q, th = SLOT_H * c * q, inner = (float)(size - 2 * f);
    float ox = floorf(f + (inner - lw - n * q) / 2 + lw), oy = floorf(f + (inner - th - n * q) / 2 + th);
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b) return;
    paint_board(b, size, f, n, q, r, c, ox, oy);
    for (int s = 0; s < n * n; s++) {
        float x = ox + s % n * q, y = oy + s / n * q, in = fmaxf(0.5f, 0.06f * q);
        int shown = variant >= 0 || s / n < n * 2 / 3;   /* the menu's: the top two thirds found */
        if (T.sol[s] && shown) puz_ink_rect(b, size, size, x + in, y + in, x + q - in, y + q - in, EBONY, 1);
    }
    if (q >= 9)
        for (int way = 0; way < 2; way++)
            for (int i = 0; i < n; i++)
                for (int j = 0; j < T.runs[way][i]; j++) {
                    int from_end = T.runs[way][i] - 1 - j;
                    float x = way ? ox + (i + 0.5f) * q : ox - (from_end + 0.5f) * SLOT_W * q;
                    float y = way ? oy - (from_end + 0.5f) * SLOT_H * q : oy + (i + 0.5f) * q;
                    puz_digits(b, size, size, T.clue[way][i][j], x, y, 0.56f * q, CLUE, 1, 1);
                }
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
}

static const game nonogram_game = {
    .id = "nonogram", .order = 355, .variants = NG_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(nonogram_game)
