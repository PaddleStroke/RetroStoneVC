/*
 * Sudoku in RetroStone Board, for one player: the house plank ruled in 9 x 9 cells, the boxes' thick lines, the
 * clues in dark ink on cells stained darker and the player's digits in blue ink on the bare wood, a row of wooden keys
 * 1..9 under the board (the rules: sudoku_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell, then a key: its digit goes in (the same key again takes it out); or drag a key onto a cell. The cell's
 * row, column and box are lit faintly, and every cell holding its digit warmly (a key tapped with no empty cell picked
 * lights its digit's cells instead). The shell's tools: the pencil (notes: the keys then put small digits in the
 * cell's corners, or take them out), the eraser (the cell's digit, or its notes), the lamp (a hint: a wrong digit
 * put right, else the easiest cell filled, in gold light) and the eye (clashes shown: a digit already in its row,
 * column or box turns red; on at first, as the house shows a king in check). A key whose nine digits are all on the
 * board fades. Undo takes back one action (a digit, a note, an erasure, a hint), all the way to the start.
 * The bar: the moves. Solved: a wave of gold light runs over the board from the last cell filled; the stars count the
 * hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, sudoku_levels.h: singles with many clues first, a
 * digit tried and followed last), or its die tab a puzzle made at random at one of the four levels.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/sudoku.h"
#include "games/sudoku_levels.h"
#include "shell.h"

static su_game G;
static bench_dg dg;

static box last_box;
static float gx, gy, gw;
static int cs, fr, made_w = -1, made_small = -1, serial, made_serial = -1;
static SDL_Texture *t_board, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank ruled: thin lines between the cells, thick ones round the boxes; the clues' cells (given: NULL none)
 * stained */
static void paint_board(uint8_t *o, int q, int f, const uint8_t *given)
{
    int w = 9 * q + 2 * f;
    art_board_n(9, 9, q, f, 0, o);
    float in = fmaxf(1, 0.04f * q);
    for (int c = 0; given && c < SU_CELLS; c++)
        if (given[c]) {
            float x = f + c % 9 * q, y = f + c / 9 * q;
            bench_shade(o, w, w, x + in, y + in, x + q - in, y + q - in, BENCH_GIVEN_SHADE);
        }
    for (int i = 0; i <= 9; i++) {
        int thick = i % 3 == 0;
        float th = thick ? fmaxf(2.0f, q * 0.07f) : fmaxf(1.0f, q * 0.026f), a = thick ? 0.88f : 0.5f;
        float p = f + i * q, lo = (float)f, hi = (float)(w - f);
        float c0 = fmaxf(lo, p - th / 2), c1 = fminf(hi, p + th / 2);
        if (i == 0) c1 = lo + th, c0 = lo;
        if (i == 9) c0 = hi - th, c1 = hi;
        puz_ink_rect(o, w, w, c0, lo, c1, hi, PUZ_INK, a);
        puz_ink_rect(o, w, w, lo, c0, hi, c1, PUZ_INK, a);
    }
}

/* the token: a corner of a grid, a clue in dark ink and two digits in blue */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int f = (int)fmaxf(1, size * 0.07f), q = (size - 2 * f) / 3, w = 3 * q + 2 * f;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(3, 3, q, f, 0, b);
    bench_shade(b, w, w, f + q + 1.0f, f + q + 1.0f, f + 2.0f * q - 1, f + 2.0f * q - 1, BENCH_GIVEN_SHADE);
    for (int i = 1; i < 3; i++) {
        float p = f + i * q, th = fmaxf(1, q * 0.05f);
        puz_ink_rect(b, w, w, p - th / 2, (float)f, p + th / 2, (float)(w - f), PUZ_INK, 0.55f);
        puz_ink_rect(b, w, w, (float)f, p - th / 2, (float)(w - f), p + th / 2, PUZ_INK, 0.55f);
    }
    static const int cells[3][3] = {{0, 0, 1}, {1, 1, 5}, {2, 2, 9}};   /* x, y, digit */
    for (int i = 0; i < 3; i++)
        puz_digits(b, w, w, cells[i][2], f + (cells[i][0] + 0.5f) * q, f + (cells[i][1] + 0.52f) * q, 0.68f * q,
                   i == 1 ? BENCH_GIVEN : BENCH_BLUE, 1, i == 1);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    bench_pad_layout(&dg.pad, b, 9);
    box sq = dg.pad.board;
    fr = (int)(sq.w * BENCH_FRAME);
    cs = (int)((sq.w - 2 * fr) / 9);
    gw = (float)(9 * cs + 2 * fr);
    gx = floorf(sq.x + (sq.w - gw) / 2);
    gy = floorf(sq.y + (sq.w - gw) / 2);
    dg.cs = (float)cs;
    if (made_w != (int)gw || made_serial != serial) {
        made_w = (int)gw;
        made_serial = serial;
        int w = (int)gw;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), cs, fr, G.given);
        t_board = ui_tex(w, w, ui_buf(w, w));
    }
    bench_pad_used(&dg.pad);
    bench_small(&t_small, &made_small, token);
}

/* ---- the game's side of the digit bench -------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y)  /* its top left corner */
{
    *x = gx + fr + (c % 9) * cs;
    *y = gy + fr + (c / 9) * cs;
}

static int cell_at(float x, float y)
{
    float ix = x - gx - fr, iy = y - gy - fr;
    if (ix < 0 || iy < 0 || ix >= 9 * cs || iy >= 9 * cs) return -1;
    return (int)(iy / cs) * 9 + (int)(ix / cs);
}

static int peer(int a, int b)
{
    return a / 9 == b / 9 || a % 9 == b % 9 || (a / 27 == b / 27 && a % 9 / 3 == b % 9 / 3) ? 38 : 0;
}

static int editable(int c) { return c >= 0 && c < SU_CELLS && !G.given[c]; }
static int given(int c) { return G.given[c]; }
static int val(int c) { return G.val[c]; }
static unsigned notes(int c) { return G.notes[c]; }
static int put(int c, int d) { return su_put(&G, c, d); }
static int note(int c, int d) { return su_note(&G, c, d); }
static int erase(int c) { return su_erase(&G, c); }
static int hint(void) { return su_hint(&G); }
static int solved(void) { return su_solved(&G); }
static int clash(int c) { return su_clash(&G, c); }
static int full(int d) { return su_count_digit(&G, d) >= 9; }

static const bench_dg_game digits = {cell_at, cell_xy, NULL, editable, given, val, notes, put, note, erase, hint,
                                     solved, clash, full, peer};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void begin(void)
{
    serial++;
    bench_dg_start(&dg, &digits, 9, 9);
    dg.look = BENCH_DG_LOOK;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    if (!su_decode(&G, su_levels[level < 0 || level >= SU_LEVELS ? 0 : level])) su_new(&G, SU_EASY, 1);
    begin();
}

static void start_random(int variant)           /* the die tab: a puzzle made at random at the variant's level */
{
    su_new(&G, variant < 0 || variant >= SU_VARIANTS ? SU_EASY : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return su_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&dg.win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static int undo(void)
{
    int c = su_undo(&G);
    if (c < 0) return 0;
    bench_dg_undone(&dg, c);
    return 1;
}

static int script(const char *moves)
{
    bench_dg_calm(&dg);
    int ok = su_script(&G, moves);
    if (su_solved(&G)) bench_dg_after(&dg, 40);
    return ok;
}

/* ---- the board on the screen, the finger ------------------------------------------------------------------------ */
static void draw(void)
{
    if (made_w < 0) layout(last_box);
    ui_put(t_board, gx, gy, gw, gw);
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

/* ---- the menu's and the picker's cards: a puzzle of the level (its clues) and as many ebony stones as its level
 * (one: easy .. four: expert); the menu's a puzzle half done ----------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int level = variant < 0 ? SU_MEDIUM : variant;
    uint8_t p[SU_CELLS], s[SU_CELLS];
    su_make(level, 2026u + (uint32_t)level, p, s);
    int side = variant < 0 ? size : (int)(0.84f * size);
    int f = (int)(side * BENCH_FRAME), q = (side - 2 * f) / 9, w = 9 * q + 2 * f, o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    paint_board(b, q, f, p);
    for (int c = 0; c < SU_CELLS; c++) {
        float cx = f + (c % 9 + 0.5f) * q, cy = f + (c / 9 + 0.54f) * q;
        if (p[c]) puz_digits(b, w, w, p[c], cx, cy, 0.64f * q, BENCH_GIVEN, 1, 1);
        else if (variant < 0 && c % 7 < 3 && c < 50) puz_digits(b, w, w, s[c], cx, cy, 0.64f * q, BENCH_BLUE, 1, 0);
    }
    art_blend(rgba, size, size, b, w, w, o, variant < 0 ? o : 0);
    free(b);
    if (variant >= 0) bench_stones(rgba, size, w, level);
}

/* a chapter's tab: the board of its middle level, its boxes ruled and its clues' cells inked: fewer as the chapters
 * go */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= SU_CHAPTERS ? SU_CHAPTERS - 1 : chapter;
    uint8_t p[SU_CELLS];
    const char *s = su_levels[ch * (SU_LEVELS / SU_CHAPTERS) + SU_LEVELS / SU_CHAPTERS / 2];
    for (int c = 0; c < SU_CELLS; c++) p[c] = s[c] != '.';
    int side = (int)(0.8f * size), f = (int)fmaxf(1, side * 0.05f), q = (side - 2 * f) / 9, w = 9 * q + 2 * f;
    if (q < 2) return;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(9, 9, q, f, 0, b);
    for (int c = 0; c < SU_CELLS; c++)
        if (p[c]) {
            float x = f + c % 9 * q, y = f + c / 9 * q;
            puz_ink_rect(b, w, w, x + 0.5f, y + 0.5f, x + q - 0.5f, y + q - 0.5f, BENCH_GIVEN, 0.7f);
        }
    for (int i = 0; i <= 9; i += 3) {
        float pp = f + i * q, t = fmaxf(1, 0.4f * q);
        puz_ink_rect(b, w, w, pp - t / 2, (float)f, pp + t / 2, (float)(w - f), PUZ_INK, 0.85f);
        puz_ink_rect(b, w, w, (float)f, pp - t / 2, (float)(w - f), pp + t / 2, PUZ_INK, 0.85f);
    }
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

static const game sudoku_game = {
    .id = "sudoku", .order = 350, .variants = SU_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .score = score, .level_stars = stars, .fill = 1, .tools = BENCH_TOOLS,
    .levels = SU_LEVELS, .chapters = SU_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
};
REGISTER_GAME(sudoku_game)
