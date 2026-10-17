/*
 * Nonograms in RetroStone Board, for one player: the house plank ruled in squares (a thicker line every five), the
 * clues burned along a darker band to the left of the rows and above the columns, the squares filled in ebony ink or
 * crossed (the rules: nonogram_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a square to fill it (again: empty); hold it a moment to cross it instead. The shell's tools: the mode (taps
 * cross instead, and back: it shows what a tap does, an ebony square or a cross) and the lamp (a hint: a wrong mark put
 * right, else a square the lines force filled, in gold light). Drag along a row or a column to do the same to the
 * whole run: the squares in the state the first one was (a fill run passes over the crosses). A line whose filled
 * squares make its clue has its clue faded. Undo takes back one tap or run, all the way to the start. The bar: the
 * moves. Solved: the crosses go, the picture comes out in its colours, and a gold light runs over it; the stars count
 * the hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 of the house's pictures in six chapters, nonogram_levels.h: 5 x 5 up
 * to 15 x 15; a solved level's tile shows its picture, an unsolved one only its empty grid), or its die tab a picture
 * drawn by chance (5 x 5, 10 x 10 or 15 x 15).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/nonogram.h"
#include "games/nonogram_levels.h"
#include "shell.h"

#define SLOT_W 0.68f                            /* a row clue's number: its width, in squares */
#define SLOT_H 0.58f                            /* a column clue's number: its height */
#define COLOUR_MS 700                           /* solved: the picture's colours coming out */
#define WAVE_STEP 40
#define WAVE_LEN 520
#define MENU_LEVEL 60                           /* the menu's card: a level's picture (the 10 x 10 cat) half found */

enum { T_MODE, T_HINT, TOOLS };

static const uint8_t EBONY[3] = {46, 38, 34}, CLUE[3] = {44, 30, 20};

static ng_game G;
static int crossing;                            /* taps cross (the mode tool) */
static int serial;                              /* the puzzle's number: its board remade */
static bench_geo geo;                           /* the squares (the finger's runs) */
static bench_touch touch;
static bench_flash hint = {-1, 0}, pop = {-1, 0};   /* at: a square */
static struct { int on; Uint32 t0; } win;

static box last_box;
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
    float inner = S - 2 * (int)(S * BENCH_FRAME) - 0.025f * S;
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

/* the mode tool: what a tap does (0 fill: an ebony square; 1 cross) */
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
    fr = (int)(S * BENCH_FRAME);
    clue_sizes(&G, &max_r, &max_c);
    cs = square_size(&G, S, max_r, max_c);
    float lw = SLOT_W * max_r * cs, th = SLOT_H * max_c * cs, inner = S - 2 * fr;
    float ox = floorf(fr + (inner - lw - G.n * cs) / 2 + lw), oy = floorf(fr + (inner - th - G.n * cs) / 2 + th);
    gx0 = bx + ox;
    gy0 = by + oy;
    geo = (bench_geo){.cols = G.n, .rows = G.n, .cs = cs, .gx = gx0, .gy = gy0};
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
    memset(&touch, 0, sizeof touch);
    hint.at = pop.at = -1;
    win.on = 0;
}

static void begin(void)
{
    serial++;
    calm();
    crossing = 0;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen: one of the house's pictures */
{
    int l = level < 0 || level >= NG_LEVELS ? 0 : level;
    if (!ng_load_picture(&G, &ng_levels[l], l)) ng_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* the die tab: a picture drawn by chance of the variant's size */
{
    ng_new(&G, variant < 0 || variant >= NG_VARIANTS ? 1 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return ng_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static Uint32 win_end(void) { return (Uint32)(COLOUR_MS + 2 * G.n * WAVE_STEP + WAVE_LEN); }
static int busy(void) { return win.on && ui_ms() - win.t0 < win_end(); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

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

/* ---- the marks: a tap fills (or crosses: the mode tool), a hold crosses; a drag as its start -------------------- */
static int target(int a, int cross)             /* what a stroke from a makes of the squares as a is */
{
    int want = cross ? NG_CROSS : NG_FILL;
    return G.sq[a] == want ? NG_EMPTY : want;
}

static int st_of(int s) { return G.sq[s]; }
static int free_of(int s) { return s >= 0 && s < G.n * G.n; }
static int tap_of(int s) { return target(s, crossing); }
static int hold_of(int s) { return target(s, 1); }
static int paint_run(int a, int b, int to) { return ng_paint(&G, a, b, to); }
static const bench_marks marks = {st_of, free_of, tap_of, hold_of, NULL, paint_run, 0};

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    int n = G.n, solved = ng_solved(&G);
    ui_put(t_board, bx, by, side, side);
    float wt = win.on ? (float)(now - win.t0) : solved ? 1e9f : -1;
    float colour = wt >= 0 ? clampf(wt / COLOUR_MS, 0, 1) : 0;
    if (touch.on && !solved) {                  /* the lines being touched: their clues lit */
        int a = touch.a;
        ui_mark_last(gx0 - SLOT_W * max_r * cs, gy0 + a / n * cs, SLOT_W * max_r * cs, (float)cs);
        ui_mark_last(gx0 + a % n * cs, gy0 - SLOT_H * max_c * cs, (float)cs, SLOT_H * max_c * cs);
    }
    if (hint.at >= 0)                           /* the hint's square: a gold light under it */
        bench_glow(&hint, gx0 + (hint.at % n + 0.5f) * cs, gy0 + (hint.at / n + 0.5f) * cs, 2.6f * cs);
    for (int s = 0; s < n * n; s++) {
        float x = gx0 + s % n * cs, y = gy0 + s / n * cs;
        int st = bench_marks_preview(&marks, &touch, &geo, 0, s);   /* the run as it will be */
        float k = bench_pop(&pop, s, 0.25f), in = fmaxf(1, 0.05f * cs), d = (cs - 2 * in) * k;
        if (st == NG_FILL || (colour > 0 && G.sol[s])) {
            const uint8_t *c = ng_rgb[G.colour[s] ? G.colour[s] : 1];
            float t = G.colour[s] ? colour : 0;
            uint8_t r = (uint8_t)(EBONY[0] + (c[0] - EBONY[0]) * t), g = (uint8_t)(EBONY[1] + (c[1] - EBONY[1]) * t),
                    bb = (uint8_t)(EBONY[2] + (c[2] - EBONY[2]) * t);
            float x0 = x + cs / 2.0f - d / 2, y0 = y + cs / 2.0f - d / 2, e = fmaxf(1, 0.09f * d);
            ui_fill(x0, y0, d, d, r, g, bb, 255);
            ui_fill(x0, y0, d, e, 255, 248, 230, 46);              /* a soft bevel: lit along the top and left, */
            ui_fill(x0, y0 + e, e, d - e, 255, 248, 230, 30);
            ui_fill(x0, y0 + d - e, d, e, 0, 0, 0, 70);            /* in shade along the foot and right */
            ui_fill(x0 + d - e, y0, e, d - e, 0, 0, 0, 50);
        } else if (st == NG_CROSS && colour < 1) {
            SDL_SetTextureAlphaMod(t_cross, (Uint8)(200 * (1 - colour)));
            ui_put_c(t_cross, x + cs / 2.0f, y + cs / 2.0f, cs * k, cs * k);
            SDL_SetTextureAlphaMod(t_cross, 255);
        }
    }
    if (!solved) bench_touch_ring(&touch, &geo);   /* a press growing into a cross: its ring */
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
    ui_tool_paint(T_MODE, paint_mode, crossing, 1, solved ? UI_DIM : crossing ? UI_ON : 0);   /* what a tap does */
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    if (ng_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t == T_MODE) {                          /* taps fill, or cross */
        crossing = !crossing;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (t == T_HINT) {                          /* a wrong mark put right, else a square the lines force */
        int s = ng_hint(&G);
        if (s < 0) {
            ui_sound(SND_DENY);
            return;
        }
        bench_flash_start(&hint, s);
        bench_flash_start(&pop, s);
        ui_sound(G.sq[s] == NG_FILL ? SND_PLACE : SND_TAP);
        after();
        return;
    }
    bench_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { bench_motion(&touch, &geo, x, y, 1); }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int to, a = bench_marks_release(&marks, &touch, &geo, 0, &to);
    if (a < 0) {
        ui_animate();
        return;
    }
    ui_sound(to == NG_FILL ? SND_PLACE : SND_TAP);
    after();
}

/* ---- the bar: the moves ------------------------------------------------------------------------------------------ */
static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the cards: a picture on its board, its clues; part of it found (shown: 0 none, else the rows above it) ----- */
static void card(const ng_game *T, int size, int shown, uint8_t *rgba)
{
    int r, c, n = T->n;
    clue_sizes(T, &r, &c);
    int f = (int)(size * BENCH_FRAME), q = square_size(T, (float)size, r, c);
    float lw = SLOT_W * r * q, th = SLOT_H * c * q, inner = (float)(size - 2 * f);
    float ox = floorf(f + (inner - lw - n * q) / 2 + lw), oy = floorf(f + (inner - th - n * q) / 2 + th);
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b) return;
    paint_board(b, size, f, n, q, r, c, ox, oy);
    for (int s = 0; s < n * n; s++) {
        float x = ox + s % n * q, y = oy + s / n * q, in = fmaxf(0.5f, 0.06f * q);
        if (T->sol[s] && s / n < shown) puz_ink_rect(b, size, size, x + in, y + in, x + q - in, y + q - in, EBONY, 1);
    }
    if (q >= 9)
        for (int way = 0; way < 2; way++)
            for (int i = 0; i < n; i++)
                for (int j = 0; j < T->runs[way][i]; j++) {
                    int from_end = T->runs[way][i] - 1 - j;
                    float x = way ? ox + (i + 0.5f) * q : ox - (from_end + 0.5f) * SLOT_W * q;
                    float y = way ? oy - (from_end + 0.5f) * SLOT_H * q : oy + (i + 0.5f) * q;
                    puz_digits(b, size, size, T->clue[way][i][j], x, y, 0.56f * q, CLUE, 1, 1);
                }
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
}

/* the menu's card: a level's picture, its top two thirds found; the random tab's: a picture drawn by chance of the
 * size, its top half found */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static ng_game T;
    if (variant < 0) {
        if (!ng_load_picture(&T, &ng_levels[MENU_LEVEL], MENU_LEVEL)) return;
        card(&T, size, T.n * 2 / 3, rgba);
        return;
    }
    ng_new(&T, variant, 2026u + (uint32_t)variant);
    card(&T, size, T.n / 2, rgba);
}

/* a small plank (S x S, in rgba at o) ruled n x n, its squares painted (pic: the colours, NULL: none) */
static void plank_pic(uint8_t *rgba, int size, int n, const uint8_t *colour, int inked)
{
    int S = (int)(0.86f * size), f = (int)fmaxf(1, 0.06f * S), q = (S - 2 * f) / n, w = n * q + 2 * f;
    if (q < 1) return;
    uint8_t *b = malloc((size_t)w * w * 4);
    if (!b) return;
    art_board_n(1, 1, w - 2 * f, f, 0, b);
    for (int s = 0; colour && s < n * n; s++) {
        if (!colour[s]) continue;
        float x = f + s % n * q, y = f + s / n * q;
        puz_ink_rect(b, w, w, x, y, x + q, y + q, inked ? EBONY : ng_rgb[colour[s]], 1);
    }
    if (!colour || inked)                       /* an empty grid (or a chapter's): its lines, thicker every five */
        for (int i = 1; i < n; i++) {
            float p = f + i * q, t = i % 5 ? fmaxf(1, 0.08f * q) : fmaxf(1, 0.2f * q);
            float a = i % 5 ? 0.45f : 0.8f;
            puz_ink_rect(b, w, w, p - t / 2, (float)f, p + t / 2, (float)(w - f), PUZ_INK, a);
            puz_ink_rect(b, w, w, (float)f, p - t / 2, (float)(w - f), p + t / 2, PUZ_INK, a);
        }
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

/* a level's tile: solved, its picture in its colours; else only its empty grid (its size), never the picture */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    static ng_game T;
    if (level < 0 || level >= NG_LEVELS || !ng_load_picture(&T, &ng_levels[level], level)) return;
    plank_pic(rgba, size, T.n, ui_level_best(level) > 0 ? T.colour : NULL, 0);
}

/* a chapter's tab: the grid of its size (5 x 5 .. 15 x 15), a few squares inked (more as the chapters go: never a
 * level's picture) */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const int SIZE[NG_CHAPTERS] = {5, 8, 10, 12, 15, 15};
    int ch = chapter < 0 ? 0 : chapter >= NG_CHAPTERS ? NG_CHAPTERS - 1 : chapter, n = SIZE[ch];
    uint8_t ink[NG_SQ] = {0};
    for (int i = 0; i <= ch; i++)               /* a stair down the left: one step a chapter */
        for (int x = 0; x <= i && i < n; x++) ink[i * n + x] = 1;
    plank_pic(rgba, size, n, ink, 1);
}

static const game nonogram_game = {
    .id = "nonogram", .order = 355, .variants = NG_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
    .levels = NG_LEVELS, .chapters = NG_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb,
};
REGISTER_GAME(nonogram_game)
