/*
 * Hitori in RetroStone Board, for one player: the house plank ruled in cells, a number burned in each, ebony tiles laid
 * on the shaded cells, an ink ring round the numbers kept (the rules: hitori_rules.c; what the logic grids share:
 * bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to shade it (again: take the tile off); hold it a moment to ring it instead (a note: kept). Drag along a
 * row or a column to do the same to a stretch (the cells as the first one was). The shell's tools: what a tap does
 * (shade, or ring: tap it to swap tap and hold), and the hint (a wrong cell put right, else the next cell reasoning
 * finds, in gold light). Shaded cells touching glow red, a ringed number ringed again in its line turns red, and the
 * cells cut off from the others by the shading take a red tint. Undo takes back one action, all the way to the start.
 * The bar: the moves. Solved: a wave of gold light runs over the grid; the stars count the hints and the mistakes
 * (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/hitori_gen.c: hitori_levels.h), or its
 * die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/hitori.h"
#include "games/hitori_levels.h"

static const uint8_t EBONY[3] = {62, 56, 54}, EBONY_DIGIT[3] = {150, 140, 132};

enum { K_SHADE, K_RING };                       /* the mode tool: what a tap puts */
enum { T_MODE, T_HINT, TOOLS };

static hto_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int rings;                               /* taps put rings (the mode tool) */
static int serial;

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_tile, *t_ring, *t_shade, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* an ebony tile within a cell (a picture size x size): dark, rounded and bevelled */
static void tile_pic(int size, uint8_t *o)
{
    int in = (int)fmaxf(1, 0.05f * size), w = size - 2 * in;
    if (w < 2) return;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = b + 4 * ((size_t)y * w + x);
            float k = 0.9f + 0.2f * (1 - (float)y / w) + 0.04f * sinf(x * 0.7f + y * 0.13f);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(EBONY[i] * k, 0, 255);
            p[3] = 255;
        }
    puz_bevel(b, w, w, 0.12f * w, fmaxf(2, 0.09f * w), 0.34f);
    art_blend(o, size, size, b, w, w, in, in);
    free(b);
}

/* a tile's shadow on the plank: the tile's rounded square, soft-edged (drawn a little down and right of it) */
static void shade_pic(int size, uint8_t *o)
{
    float c = size / 2.0f, h = 0.4f * size, r = 0.12f * size, soft = 0.09f * size;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = fmaxf(fabsf(x + 0.5f - c) - (h - r), 0), dy = fmaxf(fabsf(y + 0.5f - c) - (h - r), 0);
            float d = hypotf(dx, dy) - r, t = clampf(0.5f - d / soft, 0, 1);
            o[4 * ((size_t)y * size + x) + 3] = (uint8_t)(t * t * (3 - 2 * t) * 120);
        }
}

/* an ink ring round a cell's number */
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

/* the plank of a grid (no numbers: they are drawn on the screen, faded or red as play goes) */
static void paint_board(const bench_geo *g, uint8_t *o) { bench_paint_grid(g, o); }

/* the token: a wooden tile, half of it under an ebony tile */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2, h = w / 2;
    uint8_t *b = calloc((size_t)w * w, 4), *t = calloc((size_t)h * h, 4);
    if (!b || !t) { free(b); free(t); return; }
    art_board_n(1, 1, w, 0, 0, b);
    tile_pic(h, t);
    art_blend(b, w, w, t, h, h, 0, 0);
    art_blend(b, w, w, t, h, h, h, h);
    puz_digits(b, w, w, 2, w * 0.75f, w * 0.27f, 0.36f * w, PUZ_INK, 1, 1);
    puz_digits(b, w, w, 1, w * 0.25f, w * 0.77f, 0.36f * w, PUZ_INK, 1, 1);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
    free(t);
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
        ui_free(&t_tile);
        t_tile = pic_tex(tile_pic, geo.cs);
        ui_free(&t_ring);
        t_ring = pic_tex(ring_pic, geo.cs);
        ui_free(&t_shade);
        t_shade = pic_tex(shade_pic, geo.cs);
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
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    hto_puzzle p;
    if (!hto_decode(hto_levels[level < 0 || level >= HTO_LEVELS ? 0 : level], &p) || !hto_load(&G, &p))
        hto_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    hto_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return hto_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (hto_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (hto_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = hto_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the marks: a tap shades, a hold rings, a drag as its start ------------------------------------------------- */
static int st_of(int c) { return G.st[c]; }
static int free_of(int c) { return hto_free(&G, c); }
static int tap_of(int c) { return hto_tap_state(&G, c); }
static int hold_of(int c) { return hto_hold_state(&G, c); }
static int paint(const int *cells, int n, int state) { return hto_paint(&G, cells, n, state); }
static const bench_marks marks = {st_of, free_of, tap_of, hold_of, paint, NULL, 0};

/* ---- the board on the screen, the tools -------------------------------------------------------------------------- */
static void paint_button(int kind, int size, uint8_t *o)   /* what a tap puts: an ebony tile, or a ring */
{
    if (kind == K_SHADE) tile_pic(size, o);
    else ring_pic(size, o);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = hto_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    static uint16_t real[HTO_CELLS];
    bench_marks_show(&marks, &touch, &geo, rings, G.st, real, n * n);   /* the grid as the finger would leave it */
    for (int c = 0; c < n * n; c++) {
        float x, y, mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        bench_cell_xy(&geo, c, &x, &y);
        int v = G.p.num[c];
        if (G.st[c] == HTO_SHADE) {
            float s = bench_pop(&pop, c, 0.25f);
            ui_put_c(t_shade, mx + 0.05f * q * s, my + 0.07f * q * s, q * s, q * s);   /* laid on the plank */
            ui_put_c(t_tile, mx, my, q * s, q * s);
            if (!solved && hto_touching(&G, c)) ui_fill(x, y, (float)q, (float)q, 214, 60, 44, 120);
            ui_number_ex(mx, my + 0.02f * q, 0.5f * q * s, v, EBONY_DIGIT, 150, UI_NUM_TIGHT);
            continue;
        }
        if (!solved && hto_apart(&G, c)) ui_fill(x, y, (float)q, (float)q, 214, 60, 44, 60);
        if (G.st[c] == HTO_RING) ui_put_c(t_ring, mx, my, (float)q, (float)q);
        ui_number_ex(mx, my + 0.02f * q, 0.56f * q, v, !solved && hto_twin(&G, c) ? BENCH_RED : PUZ_INK, 255,
                     UI_NUM_TIGHT);
    }
    bench_marks_back(G.st, real, n * n);
    bench_touch_ring(&touch, &geo);
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * q);
    bench_wave_cells(&win, &geo, 2.3f);
    ui_tool_paint(T_MODE, paint_button, rings ? K_RING : K_SHADE, 0.72f, solved ? UI_DIM : rings ? UI_ON : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void tool(int t)
{
    if (t == T_MODE) {
        rings = !rings;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    int c = hto_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, c);
    bench_flash_start(&pop, c);
    ui_sound(SND_PLACE);
    after(c);
}

static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    if (hto_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { tool(t); return; }
    bench_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { bench_motion(&touch, &geo, x, y, 1); }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int to, a = bench_marks_release(&marks, &touch, &geo, rings, &to);
    if (a < 0) return;
    if (to == HTO_SHADE) bench_flash_start(&pop, a);
    ui_sound(to == HTO_SHADE ? SND_PLACE : SND_TAP);
    after(a);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the pictures of puzzles: the menu's and the picker's cards, the chapters' tabs ----------------------------- *
 * A puzzle with the shading of sol (all, or the first half of the cells' when half), side x side, at the top of the
 * card (centred: mid) */
static void paint_puzzle(const hto_puzzle *p, const uint8_t *sol, int half, int size, int side, int mid, uint8_t *rgba)
{
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p->n;
    bench_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4), *t = calloc((size_t)g.cs * g.cs, 4);
    if (!b || !t) { free(b); free(t); return; }
    bench_paint_grid(&g, b);
    int n = p->n, q = g.cs;
    tile_pic(q, t);
    for (int c = 0; c < n * n; c++) {
        float x = g.ox + c % n * q, y = g.oy + c / n * q;
        int dark = sol[c] && (!half || c < n * n / 2);
        if (dark) art_blend(b, g.w, g.h, t, q, q, (int)x, (int)y);
        if (q >= 9)
            puz_digits(b, g.w, g.h, p->num[c], x + q / 2.0f, y + q * 0.52f, 0.56f * q, dark ? EBONY_DIGIT : PUZ_INK,
                       dark ? 0.6f : 1, 1);
    }
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
    free(t);
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    hto_puzzle p;
    uint8_t sol[HTO_CELLS];
    hto_make(hto_size(v), hto_level(v), 2026u + (uint32_t)v, &p, sol);
    paint_puzzle(&p, sol, variant < 0, size, side, variant < 0, rgba);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, hto_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= HTO_CHAPTERS ? HTO_CHAPTERS - 1 : chapter;
    static hto_game T;
    hto_puzzle p;
    if (!hto_decode(hto_levels[(ch + 1) * HTO_LEVELS / HTO_CHAPTERS - 1], &p) || !hto_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, 0, size, (int)(0.92f * size), 1, rgba);
}

static const game hitori_game = {
    .id = "hitori", .order = 419, .variants = HTO_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {HTO_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = HTO_LEVELS, .chapters = HTO_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(hitori_game)
