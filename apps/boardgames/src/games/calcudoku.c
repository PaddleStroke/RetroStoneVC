/*
 * Calcudoku in RetroStone Board, for one player: the house plank ruled in n x n cells, the cages' thick lines, each
 * cage's target and operation burned into its first cell (the signs drawn, not written), the player's digits in blue
 * ink, a row of wooden keys 1..n under the board (the rules: calcudoku_rules.c; what the logic grids share: bench.h,
 * bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell, then a key: its digit goes in (the same key again takes it out); or drag a key onto a cell. The cell's
 * row and column are lit faintly, its cage a little more, and every cell holding its digit warmly (a key tapped with
 * no empty cell picked lights its digit's cells instead). The shell's tools: the pencil (notes: the keys then put
 * small digits in the cell, or take them out), the eraser, the lamp (a hint: a wrong digit put right, else a cell
 * that reasoning finds, in gold light) and the eye (clashes shown: a digit twice in a line, a full cage missing its
 * target, in red; on at first). Undo takes back one action, all the way to the start. The bar: the moves. Solved: a
 * wave of gold light runs over the board from the last cell filled; the stars count the hints and the mistakes
 * (bench.h).
 *
 * The shell's level screen chooses a level (150 in six chapters, calcudoku_levels.h), or its die tab a puzzle made at
 * random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/calcudoku.h"
#include "games/calcudoku_levels.h"
#include "shell.h"

static cdk_game G;
static bench_dg dg;
static int serial;

static box last_box;
static float bx, by;
static int q, fr, made_key = -1, made_small;
static SDL_Texture *t_board, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* an operation's sign, ink on the wood: a unit square's figure */
static void sign_fig(art_fig *f, int op)
{
    const float w = 0.17f;
    switch (op) {
    case CDK_ADD:
        art_band(f, ART_PART, 0.14f, 0.5f, 0.86f, 0.5f, w);
        art_band(f, ART_PART, 0.5f, 0.14f, 0.5f, 0.86f, w);
        break;
    case CDK_SUB: art_band(f, ART_PART, 0.14f, 0.5f, 0.86f, 0.5f, w); break;
    case CDK_MUL:
        art_band(f, ART_PART, 0.22f, 0.22f, 0.78f, 0.78f, w);
        art_band(f, ART_PART, 0.78f, 0.22f, 0.22f, 0.78f, w);
        break;
    default:
        art_band(f, ART_PART, 0.14f, 0.5f, 0.86f, 0.5f, w);
        art_ellipse(f, ART_PART, 0.5f, 0.2f, 0.1f, 0.1f);
        art_ellipse(f, ART_PART, 0.5f, 0.8f, 0.1f, 0.1f);
    }
}

/* a cage's label in a picture: the target, then the sign, from (x, y) its top left, dh the digits' height */
static void label(uint8_t *o, int w, int h, const cdk_cage *k, float x, float y, float dh)
{
    int digits = 1;
    for (int v = k->target; v >= 10; v /= 10) digits++;
    float tw = 0.66f * dh * (digits - 1) + 0.62f * dh;
    puz_digits(o, w, h, k->target, x + tw / 2, y + dh / 2, dh, PUZ_INK, 1, 1);
    if (k->op == CDK_ONE) return;
    art_fig *f = art_fig_new();
    sign_fig(f, k->op);
    art_style st = art_flat(PUZ_INK, PUZ_INK, 0, 0);
    int s = (int)(0.8f * dh + 0.5f);
    art_fig_blend(o, w, h, f, &st, (int)(x + tw + 0.12f * dh), (int)(y + (dh - s) / 2), s, 1);
    art_fig_free(f);
}

/* the plank ruled: thin lines between the cells, thick ones round the cages, the labels */
static void paint_board(uint8_t *o, const cdk_puzzle *p, int c, int f)
{
    int n = p->n, w = n * c + 2 * f;
    art_board_n(n, n, c, f, 0, o);
    float thin = fmaxf(1.0f, c * 0.024f), thick = fmaxf(2.0f, c * 0.075f), lo = (float)f, hi = (float)(w - f);
    for (int i = 1; i < n; i++) {               /* thin */
        float at = f + i * c;
        puz_ink_rect(o, w, w, at - thin / 2, lo, at + thin / 2, hi, PUZ_INK, 0.35f);
        puz_ink_rect(o, w, w, lo, at - thin / 2, hi, at + thin / 2, PUZ_INK, 0.35f);
    }
    for (int y = 0; y < n; y++)                 /* thick, where two cages meet */
        for (int x = 0; x < n; x++) {
            int cell = y * n + x;
            if (x + 1 < n && p->cage_of[cell] != p->cage_of[cell + 1]) {
                float at = f + (x + 1) * c;
                puz_ink_rect(o, w, w, at - thick / 2, fmaxf(lo, f + y * c - thick / 2), at + thick / 2,
                             fminf(hi, f + (y + 1) * c + thick / 2), PUZ_INK, 0.9f);
            }
            if (y + 1 < n && p->cage_of[cell] != p->cage_of[cell + n]) {
                float at = f + (y + 1) * c;
                puz_ink_rect(o, w, w, fmaxf(lo, f + x * c - thick / 2), at - thick / 2,
                             fminf(hi, f + (x + 1) * c + thick / 2), at + thick / 2, PUZ_INK, 0.9f);
            }
        }
    puz_ink_rect(o, w, w, lo, lo, hi, lo + thick, PUZ_INK, 0.9f);   /* round the grid */
    puz_ink_rect(o, w, w, lo, hi - thick, hi, hi, PUZ_INK, 0.9f);
    puz_ink_rect(o, w, w, lo, lo, lo + thick, hi, PUZ_INK, 0.9f);
    puz_ink_rect(o, w, w, hi - thick, lo, hi, hi, PUZ_INK, 0.9f);
    for (int k = 0; k < p->ncages; k++) {
        int cell = p->cage[k].cell[0];
        float lx = f + (cell % n) * c + 0.07f * c + thick / 2, ly = f + (cell / n) * c + 0.06f * c + thick / 2;
        label(o, w, w, &p->cage[k], lx, ly, 0.25f * c);
    }
}

/* the token: two cells of a cage, its label (6 and the sign x) and a blue digit */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int f = (int)fmaxf(1, size * 0.07f), c = (size - 2 * f) / 2, w = 2 * c + 2 * f;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(2, 2, c, f, 0, b);
    float thick = fmaxf(1.5f, c * 0.09f);
    puz_ink_rect(b, w, w, (float)f, f + c - thick / 2, (float)(w - f), f + c + thick / 2, PUZ_INK, 0.9f);
    puz_ink_rect(b, w, w, f + c - thick / 2, (float)f, f + c + thick / 2, (float)(f + c), PUZ_INK, 0.4f);
    cdk_cage k = {CDK_MUL, 2, {0, 1}, 6};
    label(b, w, w, &k, f + 0.1f * c, f + 0.08f * c, 0.36f * c);
    puz_digits(b, w, w, 3, f + 1.5f * c, f + 1.55f * c, 0.7f * c, BENCH_BLUE, 1, 0);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    int n = G.p.n ? G.p.n : 4;
    bench_pad_layout(&dg.pad, b, n);
    box sq = dg.pad.board;
    fr = (int)(sq.w * BENCH_FRAME);
    q = (int)((sq.w - 2 * fr) / n);
    int w = n * q + 2 * fr;
    bx = floorf(sq.x + (sq.w - w) / 2);
    by = floorf(sq.y + (sq.w - w) / 2);
    dg.cs = (float)q;
    int key = (w * 16 + n) * 4096 + serial % 4096;
    if (made_key != key) {
        made_key = key;
        ui_free(&t_board);
        uint8_t *o = ui_buf(w, w);
        paint_board(o, &G.p, q, fr);
        t_board = ui_tex(w, w, o);
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- the game's side of the digit bench -------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y)  /* its top left corner */
{
    *x = bx + fr + (c % G.p.n) * q;
    *y = by + fr + (c / G.p.n) * q;
}

static int cell_at(float x, float y)
{
    int n = G.p.n;
    float ix = x - bx - fr, iy = y - by - fr;
    if (ix < 0 || iy < 0 || ix >= n * q || iy >= n * q) return -1;
    return (int)(iy / q) * n + (int)(ix / q);
}

static int peer(int s, int c)                   /* its cage, then its row and column */
{
    int n = G.p.n;
    return G.p.cage_of[c] == G.p.cage_of[s] ? 52 : c / n == s / n || c % n == s % n ? 30 : 0;
}

static int val(int c) { return G.val[c]; }
static unsigned notes(int c) { return G.notes[c]; }
static int put(int c, int d) { return cdk_put(&G, c, d); }
static int note(int c, int d) { return cdk_note(&G, c, d); }
static int erase(int c) { return cdk_erase(&G, c); }
static int hint(void) { return cdk_hint(&G); }
static int solved(void) { return cdk_solved(&G); }
static int clash(int c) { return cdk_clash(&G, c); }
static int full(int d) { return cdk_count_digit(&G, d) >= G.p.n; }

static const bench_dg_game digits = {cell_at, cell_xy, NULL, NULL, NULL, val, notes, put, note, erase, hint,
                                     solved, clash, full, peer};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void begin(void)
{
    serial++;
    bench_dg_start(&dg, &digits, G.p.n, G.p.n);
    dg.look = (bench_dg_look){0.12f, 0.5f, 0.26f, 0.46f, 0.24f, 0.2f, 0.2f, 0.22f};   /* under the cage's label */
    if (last_box.w > 0) layout(last_box);
}

/* a level of the shell's level screen (calcudoku_levels.h) */
static void start(int level)
{
    cdk_puzzle p;
    level = level < 0 ? 0 : level >= CDK_STAGES ? CDK_STAGES - 1 : level;
    if (cdk_decode(cdk_levels[level], &p)) cdk_load(&G, &p, -1);
    else cdk_new(&G, 0, 1);
    begin();
}

/* a puzzle made at random (the level screen's die tab): a size and a level */
static void start_random(int variant)
{
    cdk_new(&G, variant < 0 || variant >= CDK_VARIANTS ? 0 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return cdk_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&dg.win); }
static int score(void) { return G.hist.moves; }   /* the end card: the moves it took */
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static int undo(void)
{
    int c = cdk_undo(&G);
    if (c < 0) return 0;
    bench_dg_undone(&dg, c);
    return 1;
}

static int script(const char *moves)
{
    bench_dg_calm(&dg);
    int ok = cdk_script(&G, moves);
    if (cdk_solved(&G)) bench_dg_after(&dg, G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the board on the screen, the finger ------------------------------------------------------------------------ */
static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int w = G.p.n * q + 2 * fr;
    ui_put(t_board, bx, by, (float)w, (float)w);
    bench_dg_marks(&dg);
    bench_dg_cells(&dg);
    bench_dg_pad(&dg);
}

static void press(float x, float y) { bench_dg_press(&dg, x, y); }
static void motion(float x, float y) { bench_dg_motion(&dg, x, y); }
static void release(float x, float y) { bench_dg_release(&dg, x, y); }

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the menu's and the picker's cards: a puzzle of the size and level, the level's ebony stones under it (one:
 * easy .. three: hard); the menu's a 5 x 5 puzzle half done ------------------------------------------------------ */
static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int n = variant < 0 ? 5 : cdk_size_of(variant), level = variant < 0 ? CDK_MEDIUM : cdk_level_of(variant);
    cdk_puzzle p;
    cdk_make(n, level, 2026u + (uint32_t)(variant + 1), &p);
    int side = variant < 0 || bare ? size : (int)(0.84f * size);
    int f = (int)(side * BENCH_FRAME), c = (side - 2 * f) / n, w = n * c + 2 * f, o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    paint_board(b, &p, c, f);
    if (variant < 0)
        for (int k = 0; k < n * n; k++)
            if (k % 3 != 1 && k < 15)
                puz_digits(b, w, w, p.sol[k], f + (k % n + 0.5f) * c, f + (k / n + 0.58f) * c, 0.56f * c, BENCH_BLUE,
                           1, 0);
    art_blend(rgba, size, size, b, w, w, o, variant < 0 ? o : 0);
    free(b);
    if (variant >= 0 && !bare) bench_stones(rgba, size, w, level);
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level's cages (its biggest size), without their labels */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= CDK_CHAPTERS ? CDK_CHAPTERS - 1 : chapter;
    static cdk_puzzle p;
    if (!cdk_decode(cdk_levels[(ch + 1) * CDK_STAGES / CDK_CHAPTERS - 1], &p)) return;
    p.ncages = 0;
    int n = p.n, side = (int)(0.92f * size), f = (int)fmaxf(1, side * 0.06f), c = (side - 2 * f) / n, w = n * c + 2 * f;
    uint8_t *b = c > 0 ? calloc((size_t)w * w, 4) : NULL;
    if (!b) return;
    paint_board(b, &p, c, f);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

static const game calcudoku_game = {
    .id = "calcudoku", .order = 445, .variants = CDK_VARIANTS,
    .levels = CDK_STAGES, .chapters = CDK_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {CDK_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .score = score, .level_stars = stars, .fill = 1, .tools = BENCH_TOOLS,
};
REGISTER_GAME(calcudoku_game)
