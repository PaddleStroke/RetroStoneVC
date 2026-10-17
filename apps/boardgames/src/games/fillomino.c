/*
 * Fillomino in RetroStone Board, for one player: the house plank ruled in cells, the given numbers in dark ink on
 * stained cells and the player's in blue, a wall of ink burned between two cells whose numbers differ (the regions
 * show as they are filled), a row of wooden keys 1..9 beside the board (the rules: fillomino_rules.c; what the logic
 * grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell, then a key: its number goes in (the same key again takes it out); or drag a key onto a cell. The
 * cells of the picked cell's region are lit. The shell's tools: the pencil (notes), the eraser, the lamp (a hint: a
 * wrong number put right, else the easiest cell filled, in gold light) and the eye (clashes shown: a region bigger
 * than its number turns red; on at first). Undo takes back one action, all the way to the start. The bar: the moves.
 * Solved: a wave of gold light from the last cell; the stars count the hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/fillomino_gen.c: fillomino_levels.h),
 * or its die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/fillomino.h"
#include "games/fillomino_levels.h"
#include "shell.h"

static flm_game G;
static bench_dg dg;
static bench_geo geo;
static int serial;

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank of a puzzle laid out by g, the given numbers' cells stained */
static void paint_grid(const bench_geo *g, const flm_puzzle *p, uint8_t *o)
{
    bench_paint_grid(g, o);
    for (int c = 0; c < p->n * p->n; c++)
        if (p->given[c]) bench_stain(g, o, c, BENCH_GIVEN_SHADE);
}

static void paint_board(const bench_geo *g, uint8_t *o) { paint_grid(g, &G.p, o); }

/* the walls between cells whose numbers (val; 0: none) differ, into a picture laid out by g */
static void paint_walls(const bench_geo *g, const uint8_t *val, uint8_t *o)
{
    int n = g->cols, q = g->cs;
    float t = fmaxf(2, 0.09f * q);
    for (int c = 0; c < n * n; c++) {
        float x = g->ox + c % n * q, y = g->oy + c / n * q;
        if (c % n < n - 1 && val[c] && val[c + 1] && val[c] != val[c + 1])
            puz_ink_rect(o, g->w, g->h, x + q - t / 2, y, x + q + t / 2, y + q, PUZ_INK, 0.9f);
        if (c / n < n - 1 && val[c] && val[c + n] && val[c] != val[c + n])
            puz_ink_rect(o, g->w, g->h, x, y + q - t / 2, x + q, y + q + t / 2, PUZ_INK, 0.9f);
    }
}

/* the token: a wooden tile, a region of 3 beside one of 2 */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    puz_ink_rect(b, w, w, 0.48f * w, 0, 0.52f * w, 0.5f * w, PUZ_INK, 0.9f);
    puz_ink_rect(b, w, w, 0.48f * w, 0.48f * w, (float)w, 0.52f * w, PUZ_INK, 0.9f);
    puz_digits(b, w, w, 3, w * 0.25f, w * 0.27f, 0.36f * w, BENCH_GIVEN, 1, 1);
    puz_digits(b, w, w, 3, w * 0.25f, w * 0.75f, 0.36f * w, BENCH_BLUE, 1, 0);
    puz_digits(b, w, w, 2, w * 0.75f, w * 0.27f, 0.36f * w, BENCH_BLUE, 1, 0);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    int n = G.p.n ? G.p.n : 6;
    bench_pad_layout(&dg.pad, b, FLM_TOP);
    geo.cols = geo.rows = n;
    int changed = bench_fit(&geo, dg.pad.board), key = serial % 4096 * 16 + n;
    dg.cs = (float)geo.cs;
    if (changed || made_key != key) {
        made_key = key;
        ui_free(&t_board);
        t_board = bench_tex(&geo, paint_board);
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- the game's side of the digit bench -------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y) { bench_cell_xy(&geo, c, x, y); }
static int cell_at(float x, float y) { return bench_cell_at(&geo, x, y); }
static int editable(int c) { return c >= 0 && c < G.p.n * G.p.n && !G.p.given[c]; }
static int given(int c) { return G.p.given[c] != 0; }
static int val(int c) { return G.val[c]; }
static unsigned notes(int c) { return G.notes[c]; }
static int put(int c, int d) { return flm_put(&G, c, d); }
static int note(int c, int d) { return flm_note(&G, c, d); }
static int erase(int c) { return flm_erase(&G, c); }
static int hint(void) { return flm_hint(&G); }
static int solved(void) { return flm_solved(&G); }
static int clash(int c) { return flm_clash(&G, c); }

/* the light on c while s is picked: the cells of its region */
static int peer(int s, int c)
{
    int n = G.p.n, v = G.val[s];
    if (!v || G.val[c] != v) return 0;
    static uint8_t seen[FLM_CELLS];             /* s's region, found from s */
    static int from = -1, made = -1;
    if (from != s || made != G.hist.moves) {
        int stack[FLM_CELLS], sp = 0;
        memset(seen, 0, sizeof seen);
        seen[s] = 1;
        stack[sp++] = s;
        while (sp) {
            int x = stack[--sp], px = x % n, py = x / n;
            const int nb[4] = {py > 0 ? x - n : -1, px > 0 ? x - 1 : -1, px < n - 1 ? x + 1 : -1,
                               py < n - 1 ? x + n : -1};
            for (int j = 0; j < 4; j++)
                if (nb[j] >= 0 && !seen[nb[j]] && G.val[nb[j]] == v) seen[nb[j]] = 1, stack[sp++] = nb[j];
        }
        from = s;
        made = G.hist.moves;
    }
    return seen[c] ? 46 : 0;
}

static const bench_dg_game digits = {cell_at, cell_xy, NULL, editable, given, val, notes, put, note, erase, hint,
                                     solved, clash, NULL, peer};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void begin(void)
{
    serial++;
    bench_dg_start(&dg, &digits, G.p.n, G.p.n);
    dg.look = BENCH_DG_LOOK;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    flm_puzzle p;
    if (!flm_decode(flm_levels[level < 0 || level >= FLM_LEVELS ? 0 : level], &p) || !flm_load(&G, &p))
        flm_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    flm_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return flm_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&dg.win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static int undo(void)
{
    int c = flm_undo(&G);
    if (c < 0) return 0;
    bench_dg_undone(&dg, c);
    return 1;
}

static int script(const char *moves)
{
    bench_dg_calm(&dg);
    int ok = flm_script(&G, moves);
    if (flm_solved(&G)) bench_dg_after(&dg, G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs;
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    float t = fmaxf(2, 0.09f * q), x, y;        /* the regions as big as their number: a warm wash */
    for (int c = 0; c < n * n; c++)
        if (G.val[c] && flm_region(&G, c) == G.val[c]) {
            bench_cell_xy(&geo, c, &x, &y);
            ui_fill(x, y, (float)q, (float)q, 255, 226, 140, 46);
        }
    bench_dg_marks(&dg);
    for (int c = 0; c < n * n; c++) {           /* the walls between different numbers, meeting at their corners */
        bench_cell_xy(&geo, c, &x, &y);
        if (c % n < n - 1 && G.val[c] && G.val[c + 1] && G.val[c] != G.val[c + 1])
            ui_fill(x + q - t / 2, y - t / 2, t, q + t, 62, 47, 34, 255);
        if (c / n < n - 1 && G.val[c] && G.val[c + n] && G.val[c] != G.val[c + n])
            ui_fill(x - t / 2, y + q - t / 2, q + t, t, 62, 47, 34, 255);
    }
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

/* ---- the pictures of puzzles: the menu's and the picker's cards, the chapters' tabs ----------------------------- *
 * A puzzle with the numbers of shown (0: none), side x side, at the top of the card (centred: mid) */
static void paint_puzzle(const flm_puzzle *p, const uint8_t *shown, int size, int side, int mid, uint8_t *rgba)
{
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p->n;
    bench_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4);
    if (!b) return;
    paint_grid(&g, p, b);
    paint_walls(&g, shown, b);
    int n = p->n, q = g.cs;
    for (int c = 0; c < n * n && q >= 9; c++)
        if (shown[c])
            puz_digits(b, g.w, g.h, shown[c], g.ox + (c % n + 0.5f) * q, g.oy + (c / n + 0.52f) * q, 0.6f * q,
                       p->given[c] ? BENCH_GIVEN : BENCH_BLUE, 1, p->given[c] != 0);
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    flm_puzzle p;
    uint8_t sol[FLM_CELLS], shown[FLM_CELLS] = {0};
    flm_make(flm_size(v), flm_level(v), 2026u + (uint32_t)v, &p, sol);
    for (int c = 0; c < p.n * p.n; c++) shown[c] = p.given[c] || variant < 0 ? sol[c] : 0;
    paint_puzzle(&p, shown, size, side, variant < 0, rgba);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, flm_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= FLM_CHAPTERS ? FLM_CHAPTERS - 1 : chapter;
    static flm_game T;
    flm_puzzle p;
    if (!flm_decode(flm_levels[(ch + 1) * FLM_LEVELS / FLM_CHAPTERS - 1], &p) || !flm_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, size, (int)(0.92f * size), 1, rgba);
}

static const game fillomino_game = {
    .id = "fillomino", .order = 426, .variants = FLM_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {FLM_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = FLM_LEVELS, .chapters = FLM_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = BENCH_TOOLS,
};
REGISTER_GAME(fillomino_game)
