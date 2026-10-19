/*
 * Sum Erase in RetroStone Board, for one player: the house plank ruled in cells, a digit burned in each, the targets
 * in the darker bands at the end of the rows and under the columns; an erased digit sinks into a hollow, faded; an
 * ink ring round the digits kept (the rules: sumerase_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to erase its digit (again: back); hold it a moment to ring it instead (a note: kept). Drag along a row or
 * a column to do the same to a stretch (the cells as the first one was). The shell's tools: what a tap does (the
 * eraser, or a ring round a digit, lit: tap it to swap tap and hold), and the hint (a wrong cell put right, else the
 * next cell reasoning finds, in gold light). A target its line makes glows gold; one its line can no longer make turns red. The keys: the arrows
 * move a frame over the cells, Return (or Enter) does what a tap does, Space what a hold does. Undo takes back one
 * action, all the way to the start. The bar: the moves. Solved: a wave of gold light runs over the grid; the stars
 * count the hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/sumerase_gen.c: sumerase_levels.h), or
 * its die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/sumerase.h"
#include "games/sumerase_levels.h"

enum { K_ERASE, K_RING };                       /* the mode tool: what a tap puts */
enum { T_MODE, T_HINT, TOOLS };
#define BAND 1.1f                               /* the targets' bands, in cells */

static sme_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int rings;                               /* taps put rings (the mode tool) */
static int serial;
static int cur = -1;                            /* the keys' frame (a cell), -1: not shown */

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_hole, *t_ring, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a hollow within a cell (a picture size x size): the wood sunk, darker, its walls in shadow at the top left */
static void hole_pic(int size, uint8_t *o)
{
    float in = fmaxf(1, 0.1f * size), r = 0.16f * size, w = size - 2 * in;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float px = x + 0.5f - in, py = y + 0.5f - in;
            float dx = fmaxf(r - px, px - (w - r)), dy = fmaxf(r - py, py - (w - r)), d;
            if (dx > 0 && dy > 0) d = r - hypotf(dx, dy);
            else d = fminf(fminf(px, w - px), fminf(py, w - py));
            float a = clampf(d + 0.5f, 0, 1);
            if (a <= 0) continue;
            float wall = clampf(1 - fminf(px, py) / (0.18f * size), 0, 1);   /* the shade under the top left rim */
            uint8_t *p = o + 4 * ((size_t)y * size + x);
            p[0] = 40;
            p[1] = 26;
            p[2] = 16;
            p[3] = (uint8_t)(a * (70 + 90 * wall));
        }
}

/* an ink ring round a cell's digit */
static void ring_pic(int size, uint8_t *o)
{
    float r = 0.4f * size, t = fmaxf(1.5f, 0.055f * size), c = size / 2.0f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float d = fabsf(hypotf(x + 0.5f - c, y + 0.5f - c) - r), a = clampf(t / 2 + 0.5f - d, 0, 1);
            if (a <= 0) continue;
            uint8_t *p = o + 4 * ((size_t)y * size + x);
            p[0] = BENCH_BLUE[0];
            p[1] = BENCH_BLUE[1];
            p[2] = BENCH_BLUE[2];
            p[3] = (uint8_t)(a * 235);
        }
}

/* the plank of a grid and its bands (no digits: they are drawn on the screen, faded or lit as play goes) */
static void paint_board(const bench_geo *g, uint8_t *o) { bench_paint_grid(g, o); }

static SDL_Texture *pic_tex(void (*paint)(int, uint8_t *), int q)
{
    memset(ui_buf(q, q), 0, (size_t)q * q * 4);
    paint(q, ui_buf(q, q));
    return ui_tex(q, q, ui_buf(q, q));
}

/* a small grid with its targets painted into a picture (size x size): erased digits in their hollows (all of the
 * solution's, or the first `shown` of them in reading order) */
static void paint_puzzle(const sme_puzzle *p, const uint8_t *sol, int shown, int size, int side, int mid,
                         uint8_t *rgba)
{
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p->n;
    g.rm = g.bm = BAND;
    bench_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4), *t = calloc((size_t)g.cs * g.cs, 4);
    if (!b || !t) { free(b); free(t); return; }
    bench_paint_grid(&g, b);
    int n = p->n, q = g.cs, k = 0;
    hole_pic(q, t);
    for (int c = 0; c < n * n; c++) {
        float x = g.ox + c % n * q, y = g.oy + c / n * q;
        int gone = sol[c] && k++ < shown;
        if (gone) art_blend(b, g.w, g.h, t, q, q, (int)x, (int)y);
        if (q >= 9)
            puz_digits(b, g.w, g.h, p->num[c], x + q / 2.0f, y + q * 0.52f, 0.56f * q, PUZ_INK, gone ? 0.25f : 1, 1);
    }
    for (int i = 0; i < n && q >= 9; i++) {
        float bx = g.ox + n * q + BAND * q / 2, by = g.oy + (i + 0.52f) * q;
        puz_digits(b, g.w, g.h, p->row[i], bx, by, 0.5f * q, PUZ_INK, 1, 1);
        puz_digits(b, g.w, g.h, p->col[i], g.ox + (i + 0.5f) * q, g.oy + n * q + BAND * q / 2, 0.5f * q, PUZ_INK, 1, 1);
    }
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
    free(t);
}

/* the token: a 2 x 2 grid, one digit erased, its two targets */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    sme_puzzle p;
    memset(&p, 0, sizeof p);
    p.n = 2;
    const uint8_t num[4] = {3, 5, 4, 2}, sol[4] = {0, 1, 0, 0};
    memcpy(p.num, num, 4);
    sme_sums(&p, sol);
    paint_puzzle(&p, sol, 4, size, size, 1, rgba);
}

static void layout(box b)
{
    last_box = b;
    geo.cols = geo.rows = G.p.n;
    geo.rm = geo.bm = BAND;
    int changed = bench_fit(&geo, b), key = serial % 4096 * 16 + G.p.n;
    if (changed || made_key != key) {
        made_key = key;
        ui_free(&t_board);
        t_board = bench_tex(&geo, paint_board);
        ui_free(&t_hole);
        t_hole = pic_tex(hole_pic, geo.cs);
        ui_free(&t_ring);
        t_ring = pic_tex(ring_pic, geo.cs);
    }
    bench_used(&geo);
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
    rings = 0;
    cur = -1;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    sme_puzzle p;
    uint8_t sol[SME_CELLS];
    if (!sme_decode(sme_levels[level < 0 || level >= SME_LEVELS ? 0 : level], &p, sol) || !sme_load(&G, &p))
        sme_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    sme_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return sme_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (sme_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
        cur = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (sme_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = sme_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the marks: a tap erases, a hold rings, a drag as its start ------------------------------------------------- */
static int st_of(int c) { return G.st[c]; }
static int free_of(int c) { return sme_free(&G, c); }
static int tap_of(int c) { return sme_tap_state(&G, c); }
static int hold_of(int c) { return sme_hold_state(&G, c); }
static int paint(const int *cells, int n, int state) { return sme_paint(&G, cells, n, state); }
static const bench_marks marks = {st_of, free_of, tap_of, hold_of, paint, NULL, 0};

/* ---- the board on the screen, the tools -------------------------------------------------------------------------- */
/* the mode tool's picture when taps ring: a ring round a digit, as the board shows a digit kept (erasing: the shell's
 * eraser) */
static void paint_button(int kind, int size, uint8_t *o)
{
    (void)kind;
    float r = 0.4f * size, t = fmaxf(1.5f, 0.08f * size), c = size / 2.0f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float d = fabsf(hypotf(x + 0.5f - c, y + 0.5f - c) - r), a = clampf(t / 2 + 0.5f - d, 0, 1);
            if (a <= 0) continue;
            uint8_t *p = o + 4 * ((size_t)y * size + x);
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(a * 255);
        }
    puz_digits(o, size, size, 7, c, size * 0.52f, 0.46f * size, PUZ_WHITE, 1, 1);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = sme_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    static uint16_t real[SME_CELLS];
    bench_marks_show(&marks, &touch, &geo, rings, G.st, real, n * n);   /* the grid as the finger would leave it */
    for (int c = 0; c < n * n; c++) {
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        int v = G.p.num[c];
        if (G.st[c] == SME_ERASE) {
            float s = bench_pop(&pop, c, 0.2f);
            ui_put_c(t_hole, mx, my, q * s, q * s);
            ui_number_ex(mx, my + 0.02f * q, 0.5f * q, v, PUZ_INK, 60, UI_NUM_TIGHT);
            continue;
        }
        if (G.st[c] == SME_KEEP) ui_put_c(t_ring, mx, my, (float)q, (float)q);
        ui_number_ex(mx, my + 0.02f * q, 0.56f * q, v, PUZ_INK, 255, UI_NUM_TIGHT);
    }
    for (int way = 0; way < 2; way++)           /* the targets: gold when made, red when out of reach */
        for (int i = 0; i < n; i++) {
            float cx = way ? geo.gx + (i + 0.5f) * q : geo.gx + (n + BAND / 2) * q;
            float cy = way ? geo.gy + (n + BAND / 2) * q : geo.gy + (i + 0.5f) * q;
            int st = sme_line(&G, way, i);
            if (st > 0 && !solved) ui_mark_gold_a(cx, cy, 1.1f * q, 0.7f);
            ui_number_ex(cx, cy + 0.02f * q, 0.5f * q, way ? G.p.col[i] : G.p.row[i],
                         st < 0 && !solved ? BENCH_RED : PUZ_INK, 255, UI_NUM_TIGHT);
        }
    bench_marks_back(G.st, real, n * n);
    bench_touch_ring(&touch, &geo);
    if (cur >= 0 && !solved) {
        float x, y;
        bench_cell_xy(&geo, cur, &x, &y);
        ui_mark_select(x, y, (float)q, (float)q);
    }
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * q);
    bench_wave_cells(&win, &geo, 2.3f);
    if (rings) ui_tool_paint(T_MODE, paint_button, K_RING, 0.72f, (solved ? UI_DIM : UI_ON) | UI_TINT);
    else ui_tool(T_MODE, ICON_ERASER, solved ? UI_DIM : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer, the keys --------------------------------------------------------------------------------------- */
static void tool(int t)
{
    if (t == T_MODE) {
        rings = !rings;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    int c = sme_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, c);
    bench_flash_start(&pop, c);
    ui_sound(SND_PLACE);
    after(c);
}

static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    cur = -1;
    if (sme_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { tool(t); return; }
    bench_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { bench_motion(&touch, &geo, x, y, 1); }

static void done_at(int a, int to)
{
    if (to == SME_ERASE) bench_flash_start(&pop, a);
    ui_sound(to == SME_ERASE ? SND_PLACE : SND_TAP);
    after(a);
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int to, a = bench_marks_release(&marks, &touch, &geo, rings, &to);
    if (a >= 0) done_at(a, to);
}

static void key(SDL_Keycode k)
{
    int n = G.p.n;
    if (sme_solved(&G) || !n) return;
    if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN) {
        if (cur < 0) cur = n * n / 2;
        else {
            int x = cur % n, y = cur / n;
            x += (k == SDLK_RIGHT) - (k == SDLK_LEFT);
            y += (k == SDLK_DOWN) - (k == SDLK_UP);
            cur = (y < 0 ? 0 : y >= n ? n - 1 : y) * n + (x < 0 ? 0 : x >= n ? n - 1 : x);
        }
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (k != SDLK_RETURN && k != SDLK_KP_ENTER && k != SDLK_SPACE) return;
    if (cur < 0) { cur = n * n / 2; ui_animate(); return; }
    int mark = (k == SDLK_SPACE) != rings, to = mark ? sme_hold_state(&G, cur) : sme_tap_state(&G, cur);
    if (sme_paint(&G, &cur, 1, to)) done_at(cur, to);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the pictures of puzzles: the menu's and the picker's cards, the chapters' tabs ----------------------------- */
static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 4 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    sme_puzzle p;
    uint8_t sol[SME_CELLS];
    if (variant < 0) {                          /* the menu's: a 5 x 5 level, half of its digits erased */
        sme_decode(sme_levels[40], &p, sol);
        paint_puzzle(&p, sol, 4, size, side, 1, rgba);
        return;
    }
    sme_make(sme_size(v), 9, sme_level(v), 2026u + (uint32_t)v, 0, &p, sol);
    paint_puzzle(&p, sol, 0, size, side, 0, rgba);
    if (!bare) bench_stones(rgba, size, side, sme_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= SME_CHAPTERS ? SME_CHAPTERS - 1 : chapter;
    sme_puzzle p;
    uint8_t sol[SME_CELLS];
    if (!sme_decode(sme_levels[(ch + 1) * SME_LEVELS / SME_CHAPTERS - 1], &p, sol)) return;
    paint_puzzle(&p, sol, SME_CELLS, size, (int)(0.92f * size), 1, rgba);
}

static const game sumerase_game = {
    .id = "sumerase", .order = 491, .variants = SME_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .axes = {SME_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = SME_LEVELS, .chapters = SME_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(sumerase_game)
