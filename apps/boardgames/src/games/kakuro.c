/*
 * Kakuro in RetroStone Board, for one player: the house plank ruled in n x n squares, the dark ones stained walnut
 * with a diagonal across them, a run's clue burned in cream on its side of it (the run across in the upper right, the
 * run down in the lower left), the player's digits in blue ink, a row of wooden keys 1..9 under the board (the rules:
 * kakuro_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a white square, then a key: its digit goes in (the same key again takes it out); or drag a key onto a square.
 * The square's two runs are lit faintly, their clues in gold, and every square holding its digit warmly (a key tapped
 * with nothing picked lights its digit's squares instead). The shell's tools: the pencil (notes), the eraser, the
 * lamp (a hint: a wrong digit put right, else a square that reasoning finds, in gold light) and the eye (clashes
 * shown: a digit twice in a run, a run adding up past its clue or full and wrong, in red; on at first). Undo takes
 * back one action, all the way to the start. The bar: the moves. Solved: a wave of gold light runs over the board
 * from the last square filled; the stars count the hints and the mistakes (bench.h).
 *
 * The shell's level screen chooses a level (150 in six chapters, kakuro_levels.h), or its die tab a puzzle made at
 * random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/kakuro.h"
#include "games/kakuro_levels.h"
#include "shell.h"

static const uint8_t CLUE_INK[3] = {244, 230, 200};

static kkr_game G;
static bench_dg dg;
static int serial;

static box last_box;
static float bx, by;
static int q, fr, made_key = -1, made_small;
static SDL_Texture *t_board, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a dark square: the plank stained walnut, a lighter diagonal, its clues in cream (across up right, down low left) */
static void dark_square(uint8_t *o, int w, int x0, int y0, int c, int across, int down)
{
    for (int y = y0; y < y0 + c; y++)
        for (int x = x0; x < x0 + c; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            p[0] = (uint8_t)(p[0] * 0.36f);
            p[1] = (uint8_t)(p[1] * 0.27f);
            p[2] = (uint8_t)(p[2] * 0.22f);
        }
    float th = fmaxf(1.0f, c * 0.035f);
    if (across || down)
        for (int y = y0; y < y0 + c; y++)       /* the diagonal, from the top left down to the bottom right */
            for (int x = x0; x < x0 + c; x++) {
                float d = fabsf((x + 0.5f - x0) - (y + 0.5f - y0)) / 1.4142f, a = clampf(th / 2 + 0.5f - d, 0, 1);
                uint8_t *p = o + 4 * ((size_t)y * w + x);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] + (CLUE_INK[i] - p[i]) * a * 0.55f);
            }
    float dh = 0.3f * c;
    if (across) puz_digits(o, w, w, across, x0 + 0.68f * c, y0 + 0.3f * c, dh, CLUE_INK, 1, 1);
    if (down) puz_digits(o, w, w, down, x0 + 0.32f * c, y0 + 0.72f * c, dh, CLUE_INK, 1, 1);
}

static void paint_board(uint8_t *o, const kkr_puzzle *p, int c, int f)
{
    int n = p->n, w = n * c + 2 * f;
    art_board_n(n, n, c, f, 0, o);
    float thin = fmaxf(1.0f, c * 0.026f), lo = (float)f, hi = (float)(w - f);
    for (int i = 1; i < n; i++) {
        float at = f + i * c;
        puz_ink_rect(o, w, w, at - thin / 2, lo, at + thin / 2, hi, PUZ_INK, 0.45f);
        puz_ink_rect(o, w, w, lo, at - thin / 2, hi, at + thin / 2, PUZ_INK, 0.45f);
    }
    for (int k = 0; k < n * n; k++)
        if (!p->white[k]) dark_square(o, w, f + (k % n) * c, f + (k / n) * c, c, p->across[k], p->down[k]);
    for (int i = 1; i < n; i++) {               /* the lines again between dark squares: a seam */
        float at = f + i * c;
        for (int k = 0; k < n; k++) {
            if (!p->white[k * n + i - 1] && !p->white[k * n + i])
                puz_ink_rect(o, w, w, at - thin / 2, f + k * c, at + thin / 2, f + (k + 1) * c, PUZ_INK, 0.6f);
            if (!p->white[(i - 1) * n + k] && !p->white[i * n + k])
                puz_ink_rect(o, w, w, f + k * c, at - thin / 2, f + (k + 1) * c, at + thin / 2, PUZ_INK, 0.6f);
        }
    }
}

/* the token: a dark square with its clues, two white squares beside and under it, a blue digit */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int f = (int)fmaxf(1, size * 0.07f), c = (size - 2 * f) / 2, w = 2 * c + 2 * f;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(2, 2, c, f, 0, b);
    dark_square(b, w, f, f, c, 4, 9);
    dark_square(b, w, f + c, f + c, c, 0, 0);
    puz_digits(b, w, w, 3, f + 1.5f * c, f + 0.55f * c, 0.7f * c, BENCH_BLUE, 1, 0);
    puz_digits(b, w, w, 8, f + 0.5f * c, f + 1.55f * c, 0.7f * c, BENCH_BLUE, 1, 0);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    int n = G.p.n ? G.p.n : 6;
    bench_pad_layout(&dg.pad, b, 9);
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
    bench_pad_used(&dg.pad);
    bench_small(&t_small, &made_small, token);
}

/* ---- the game's side of the digit bench -------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y)
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

static int in_run(int c, int r)
{
    const kkr_run *k = &G.run[r];
    for (int i = 0; i < k->len; i++)
        if (k->cell[i] == c) return 1;
    return 0;
}

static int peer(int s, int c) { return in_run(c, G.run_of[s][0]) || in_run(c, G.run_of[s][1]) ? 40 : 0; }
static int white(int c) { return c >= 0 && c < G.p.n * G.p.n && G.p.white[c]; }
static int val(int c) { return G.val[c]; }
static unsigned notes(int c) { return G.notes[c]; }
static int put(int c, int d) { return kkr_put(&G, c, d); }
static int note(int c, int d) { return kkr_note(&G, c, d); }
static int erase(int c) { return kkr_erase(&G, c); }
static int hint(void) { return kkr_hint(&G); }
static int solved(void) { return kkr_solved(&G); }
static int clash(int c) { return kkr_clash(&G, c); }

static const bench_dg_game digits = {cell_at, cell_xy, white, white, NULL, val, notes, put, note, erase, hint,
                                     solved, clash, NULL, peer};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void begin(void)
{
    serial++;
    bench_dg_start(&dg, &digits, G.p.n, G.p.n);
    dg.look = BENCH_DG_LOOK;
    if (last_box.w > 0) layout(last_box);
}

/* a level of the shell's level screen (kakuro_levels.h) */
static void start(int level)
{
    kkr_puzzle p;
    level = level < 0 ? 0 : level >= KKR_STAGES ? KKR_STAGES - 1 : level;
    if (kkr_decode(kkr_levels[level], &p)) kkr_load(&G, &p, -1);
    else kkr_new(&G, 0, 1);
    begin();
}

/* a puzzle made at random (the level screen's die tab): a size and a level */
static void start_random(int variant)
{
    kkr_new(&G, variant < 0 || variant >= KKR_VARIANTS ? 0 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return kkr_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&dg.win); }
static int score(void) { return G.hist.moves; }   /* the end card: the moves it took */
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static int undo(void)
{
    int c = kkr_undo(&G);
    if (c < 0) return 0;
    bench_dg_undone(&dg, c);
    return 1;
}

static int script(const char *moves)
{
    bench_dg_calm(&dg);
    int ok = kkr_script(&G, moves);
    if (kkr_solved(&G)) bench_dg_after(&dg, G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the board on the screen, the finger ------------------------------------------------------------------------ */
static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int w = G.p.n * q + 2 * fr;
    ui_put(t_board, bx, by, (float)w, (float)w);
    if (dg.sel >= 0 && !kkr_solved(&G))
        for (int t = 0; t < 2; t++) {           /* the picked square's two runs: their clues in gold */
            float x, y;
            cell_xy(G.run[G.run_of[dg.sel][t]].clue, &x, &y);
            ui_mark_gold_a(t ? x + 0.32f * q : x + 0.68f * q, t ? y + 0.72f * q : y + 0.3f * q, 0.9f * q, 0.8f);
        }
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

/* ---- the menu's and the picker's cards: a puzzle of the size and level, the level's ebony stones under it; the
 * menu's an 8 x 8 puzzle half done -------------------------------------------------------------------------------- */
static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int n = variant < 0 ? 8 : kkr_size_of(variant), level = variant < 0 ? KKR_MEDIUM : kkr_level_of(variant);
    kkr_puzzle p;
    kkr_make(n, level, 2026u + (uint32_t)(variant + 1), &p);
    int side = variant < 0 || bare ? size : (int)(0.84f * size);
    int f = (int)(side * BENCH_FRAME), c = (side - 2 * f) / n, w = n * c + 2 * f, o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    paint_board(b, &p, c, f);
    if (variant < 0)
        for (int k = 0; k < n * n / 2; k++)
            if (p.white[k] && k % 3)
                puz_digits(b, w, w, p.sol[k], f + (k % n + 0.5f) * c, f + (k / n + 0.54f) * c, 0.62f * c, BENCH_BLUE,
                           1, 0);
    art_blend(rgba, size, size, b, w, w, o, variant < 0 ? o : 0);
    free(b);
    if (variant >= 0 && !bare) bench_stones(rgba, size, w, level);
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level's pattern (its biggest size), the dark squares without their clues */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= KKR_CHAPTERS ? KKR_CHAPTERS - 1 : chapter;
    kkr_puzzle p;
    if (!kkr_decode(kkr_levels[(ch + 1) * KKR_STAGES / KKR_CHAPTERS - 1], &p)) return;
    memset(p.across, 0, sizeof p.across);
    memset(p.down, 0, sizeof p.down);
    int n = p.n, side = (int)(0.92f * size), f = (int)fmaxf(1, side * 0.06f), c = (side - 2 * f) / n, w = n * c + 2 * f;
    uint8_t *b = c > 0 ? calloc((size_t)w * w, 4) : NULL;
    if (!b) return;
    paint_board(b, &p, c, f);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

static const game kakuro_game = {
    .id = "kakuro", .order = 446, .variants = KKR_VARIANTS,
    .levels = KKR_STAGES, .chapters = KKR_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {KKR_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .score = score, .level_stars = stars, .fill = 1, .tools = BENCH_TOOLS,
};
REGISTER_GAME(kakuro_game)
