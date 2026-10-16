/*
 * Nurikabe in RetroStone Board, for one player: the house plank ruled in cells, the islands' numbers burned in dark
 * ink, the sea laid in lapis tiles with a ripple (the rules: nurikabe_rules.c; what the logic grids share: bench.h,
 * bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to lay sea in it (again: take it up); hold it a moment to put a dot instead (a note: land). Drag along a
 * row or a column to do the same to a stretch (the cells as the first one was). The shell's tools: what a tap does
 * (sea, or a dot: tap it to swap tap and hold), and the hint (a wrong cell put right, else the next cell reasoning
 * finds, in gold light). A 2 x 2 pool of sea glows red; a number whose island (its dots) is too big turns red, one
 * closed by the sea at its size fades. Undo takes back one action, all the way to the start. The bar: the moves.
 * Solved: a wave of gold light runs over the grid; the stars count the hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/nurikabe_gen.c: nurikabe_levels.h), or
 * its die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/nurikabe.h"
#include "games/nurikabe_levels.h"

static const uint8_t LAPIS[3] = {52, 84, 150}, FOAM[3] = {150, 176, 226};

enum { K_SEA, K_DOT };                          /* the mode tool: what a tap puts */
enum { T_MODE, T_HINT, TOOLS };

static nrk_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int dots;                                /* taps put dots (the mode tool) */
static int serial;

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_sea, *t_dot, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a tile of sea, w x h: lapis with two pale ripples, rounded and bevelled */
static void sea_pic(uint8_t *o, int w, int h)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float fx = (float)x / w, fy = (float)y / h, k = 0.92f + 0.12f * (1 - fy);
            float r1 = fabsf(fy - 0.36f - 0.05f * sinf(fx * 2 * ART_PI * 1.5f));
            float r2 = fabsf(fy - 0.68f - 0.05f * sinf(fx * 2 * ART_PI * 1.5f + 1.7f));
            float ripple = fmaxf(0, 1 - fminf(r1, r2) / 0.035f) * (fx > 0.16f && fx < 0.84f ? 0.55f : 0);
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(LAPIS[i] * k + (FOAM[i] - LAPIS[i] * k) * ripple, 0, 255);
            p[3] = 255;
        }
    puz_bevel(o, w, h, 0.12f * fminf(w, h), fmaxf(2, 0.09f * fminf(w, h)), 0.3f);
}

static void sea_cell_pic(int size, uint8_t *o)  /* a sea tile within a cell (a picture size x size) */
{
    int in = (int)fmaxf(1, 0.05f * size), w = size - 2 * in;
    if (w < 2) return;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    sea_pic(b, w, w);
    art_blend(o, size, size, b, w, w, in, in);
    free(b);
}

static void dot_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.11f, 0.11f);
    art_fig_paint(o, size, f, PUZ_INK, PUZ_INK, 0, 0);
}

static void pic_into(uint8_t *o, int w, int h, void (*paint)(int, uint8_t *), float x, float y, int size)
{
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    paint(size, b);
    art_blend(o, w, h, b, size, size, (int)lroundf(x), (int)lroundf(y));
    free(b);
}

/* the plank with its numbers (a puzzle's), into a picture laid out by g */
static void paint_grid(const bench_geo *g, const nrk_puzzle *p, uint8_t *o)
{
    bench_paint_grid(g, o);
    for (int c = 0; c < p->n * p->n; c++)
        if (p->clue[c]) bench_stain(g, o, c, 0.92f);
}

static void paint_board(const bench_geo *g, uint8_t *o) { paint_grid(g, &G.p, o); }

/* the token: a wooden tile, its lower half sea */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2, sh = w / 2;
    uint8_t *b = calloc((size_t)w * w, 4), *s = calloc((size_t)w * sh, 4);
    if (!b || !s) { free(b); free(s); return; }
    art_board_n(1, 1, w, 0, 0, b);
    sea_pic(s, w, sh);
    art_blend(b, w, w, s, w, sh, 0, w - sh);
    puz_digits(b, w, w, 3, w * 0.5f, w * 0.28f, 0.42f * w, PUZ_INK, 1, 1);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
    free(s);
}

static SDL_Texture *pic_tex(void (*paint)(int, uint8_t *), int q)
{
    memset(ui_buf(q, q), 0, (size_t)q * q * 4);
    paint(q, ui_buf(q, q));
    return ui_tex(q, q, ui_buf(q, q));
}

static void layout(box b)
{
    last_box = b;
    geo.cols = geo.rows = G.p.n;
    int changed = bench_fit(&geo, b), key = serial % 4096 * 16 + G.p.n;
    if (changed || made_key != key) {
        made_key = key;
        ui_free(&t_board);
        t_board = bench_tex(&geo, paint_board);
        ui_free(&t_sea);
        t_sea = pic_tex(sea_cell_pic, geo.cs);
        ui_free(&t_dot);
        t_dot = pic_tex(dot_pic, geo.cs);
    }
    bench_small(&t_small, &made_small, token);
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
    dots = 0;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    nrk_puzzle p;
    if (!nrk_decode(nrk_levels[level < 0 || level >= NRK_LEVELS ? 0 : level], &p) || !nrk_load(&G, &p))
        nrk_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    nrk_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return nrk_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (nrk_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (nrk_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = nrk_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the marks: a tap sea, a hold a dot, a drag as its start ---------------------------------------------------- */
static int st_of(int c) { return G.st[c]; }
static int free_of(int c) { return nrk_free(&G, c); }
static int tap_of(int c) { return nrk_tap_state(&G, c); }
static int hold_of(int c) { return nrk_hold_state(&G, c); }
static int paint(const int *cells, int n, int state) { return nrk_paint(&G, cells, n, state); }
static const bench_marks marks = {st_of, free_of, tap_of, hold_of, paint, NULL, 0};

/* ---- the board on the screen, the tools -------------------------------------------------------------------------- */
static void paint_button(int kind, int size, uint8_t *o)   /* what a tap puts: sea, or a cream dot */
{
    if (kind == K_SEA) {
        int w = (int)(0.8f * size), at = (size - w) / 2;
        uint8_t *b = calloc((size_t)w * w, 4);
        if (!b) return;
        sea_pic(b, w, w);
        art_blend(o, size, size, b, w, w, at, at);
        free(b);
        return;
    }
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.2f, 0.2f);
    art_fig_paint(o, size, f, PUZ_CREAM, PUZ_CREAM, 0, 0);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = nrk_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    static uint16_t real[NRK_CELLS];
    bench_marks_show(&marks, &touch, &geo, dots, G.st, real, n * n);   /* the grid as the finger would leave it */
    for (int c = 0; c < n * n; c++) {
        float x, y, mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        bench_cell_xy(&geo, c, &x, &y);
        int v = G.p.clue[c];
        if (v) {                                /* a number: red too big, faded closed at its size */
            int st = nrk_island(&G, c);
            ui_number_ex(mx, my + 0.02f * q, 0.6f * q, v, st == 1 && !solved ? BENCH_RED : PUZ_INK,
                         st == 2 && !solved ? 110 : 255, UI_NUM_TIGHT);
            continue;
        }
        if (G.st[c] == NRK_DOT) ui_put_c(t_dot, mx, my, (float)q, (float)q);
        if (G.st[c] != NRK_SEA) continue;
        float s = bench_pop(&pop, c, 0.25f);
        ui_put_c(t_sea, mx, my, q * s, q * s);
        if (!solved && nrk_pool(&G, c)) ui_fill(x, y, (float)q, (float)q, 214, 60, 44, 110);
    }
    bench_marks_back(G.st, real, n * n);
    bench_touch_ring(&touch, &geo);
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * q);
    bench_wave_cells(&win, &geo, 2.3f);
    ui_tool_paint(T_MODE, paint_button, dots ? K_DOT : K_SEA, 0.72f, solved ? UI_DIM : dots ? UI_ON : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void tool(int t)
{
    if (t == T_MODE) {
        dots = !dots;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    int c = nrk_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, c);
    bench_flash_start(&pop, c);
    ui_sound(SND_PLACE);
    after(c);
}

static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    if (nrk_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { tool(t); return; }
    bench_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { bench_motion(&touch, &geo, x, y, 1); }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int to, a = bench_marks_release(&marks, &touch, &geo, dots, &to);
    if (a < 0) return;
    if (to == NRK_SEA) bench_flash_start(&pop, a);
    ui_sound(to == NRK_SEA ? SND_PLACE : SND_TAP);
    after(a);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the pictures of puzzles: the menu's and the picker's cards, the chapters' tabs ----------------------------- *
 * A puzzle with the sea of sol laid (all, or the first half of the cells' when half), side x side, at the top of the
 * card (centred: mid) */
static void paint_puzzle(const nrk_puzzle *pp, const uint8_t *sol, int half, int size, int side, int mid,
                         uint8_t *rgba)
{
    nrk_puzzle p = *pp;
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p.n;
    bench_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4);
    if (!b) return;
    paint_grid(&g, &p, b);
    int n = p.n, q = g.cs;
    for (int c = 0; c < n * n; c++) {
        float x = g.ox + c % n * q, y = g.oy + c / n * q;
        if (sol[c] && !p.clue[c] && (!half || c < n * n / 2)) pic_into(b, g.w, g.h, sea_cell_pic, x, y, q);
        if (p.clue[c] && q >= 9) puz_digits(b, g.w, g.h, p.clue[c], x + q / 2.0f, y + q * 0.52f, 0.6f * q, PUZ_INK, 1, 1);
    }
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    nrk_puzzle p;
    uint8_t sol[NRK_CELLS];
    nrk_make(nrk_size(v), nrk_level(v), 2026u + (uint32_t)v, &p, sol);
    paint_puzzle(&p, sol, variant < 0, size, side, variant < 0, rgba);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, nrk_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= NRK_CHAPTERS ? NRK_CHAPTERS - 1 : chapter;
    static nrk_game T;
    nrk_puzzle p;
    if (!nrk_decode(nrk_levels[(ch + 1) * NRK_LEVELS / NRK_CHAPTERS - 1], &p) || !nrk_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, 0, size, (int)(0.92f * size), 1, rgba);
}

static const game nurikabe_game = {
    .id = "nurikabe", .order = 418, .variants = NRK_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {NRK_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = NRK_LEVELS, .chapters = NRK_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(nurikabe_game)
