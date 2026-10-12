/*
 * Binairo in RetroStone Board, for one player: the house plank ruled in cells, the given discs on cells stained
 * darker, the player's ivory and ebony discs (the house's counters) laid on the wood (the rules: binairo_rules.c;
 * what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap an empty cell for an ivory disc, again for an ebony one, again to empty it; hold it a moment to go the other
 * way round (ebony first). Drag along a row or a column to do the same to every cell of the stretch that was as the
 * first one. Three alike in a row glow red; a row or a column with too many of a colour, or full and alike another,
 * is tinted red. The shell's tool: the hint (a wrong disc put right, else the next cell reasoning finds, in gold
 * light). Undo takes back one action, all the way to the start. The bar: the moves. Solved: a wave of gold light runs
 * over the board from the last cell; the stars count the hints and the mistakes (bench.h).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/binairo.h"

enum { T_HINT, TOOLS };

static bn_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int serial;

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_disc[2], *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank with the given cells stained (a puzzle's), into a picture laid out by g */
static void paint_givens(const bench_geo *g, const bn_puzzle *p, uint8_t *o)
{
    bench_paint_grid(g, o);
    for (int c = 0; c < p->n * p->n; c++)
        if (p->given[c]) bench_stain(g, o, c, 0.7f);
}

static void paint_board(const bench_geo *g, uint8_t *o) { paint_givens(g, &G.p, o); }

/* a disc (0 ivory, 1 ebony) painted into a picture (w x h) centred at (cx, cy), d across */
static void disc_into(uint8_t *o, int w, int h, int colour, float cx, float cy, int d)
{
    uint8_t *b = calloc((size_t)d * d, 4);
    if (!b) return;
    art_counter(COUNTER_DISC, colour, d, b);
    art_blend(o, w, h, b, d, d, (int)lroundf(cx - d / 2.0f), (int)lroundf(cy - d / 2.0f));
    free(b);
}

/* the token: an ivory and an ebony disc on a wooden tile */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2, d = (int)(w * 0.46f);
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    disc_into(b, w, w, 0, w * 0.32f, w * 0.34f, d);
    disc_into(b, w, w, 1, w * 0.68f, w * 0.66f, d);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    geo.cols = geo.rows = G.p.n;
    int changed = bench_fit(&geo, b);
    int key = serial % 4096 * 16 + G.p.n;
    if (changed || made_key != key) {
        made_key = key;
        int d = (int)(0.8f * geo.cs);
        ui_free(&t_board);
        t_board = bench_tex(&geo, paint_board);
        for (int i = 0; i < 2; i++) {
            ui_free(&t_disc[i]);
            memset(ui_buf(d, d), 0, (size_t)d * d * 4);
            art_counter(COUNTER_DISC, i, d, ui_buf(d, d));
            t_disc[i] = ui_tex(d, d, ui_buf(d, d));
        }
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

static void start(int variant)
{
    bn_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return bn_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (bn_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (bn_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = bn_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the marks: a tap ivory first, a hold ebony first; a drag as its start ------------------------------------- */
static int st_of(int c) { return G.st[c]; }
static int free_of(int c) { return bn_free(&G, c); }
static int tap_of(int c) { return bn_tap_state(&G, c); }
static int hold_of(int c) { return bn_hold_state(&G, c); }
static int paint(const int *cells, int n, int state) { return bn_paint(&G, cells, n, state); }
static const bench_marks marks = {st_of, free_of, tap_of, hold_of, paint, NULL, 0};

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = bn_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    static uint16_t real[BN_CELLS];
    bench_marks_show(&marks, &touch, &geo, 0, G.st, real, n * n);   /* the board as the finger would leave it */
    if (!solved)
        for (int way = 0; way < 2; way++)       /* a line too full of a colour, or alike another: tinted red */
            for (int i = 0; i < n; i++) {
                if (!bn_line_over(&G, way, i) && !bn_line_twin(&G, way, i)) continue;
                if (way) ui_fill(geo.gx + i * q, geo.gy, (float)q, (float)n * q, 214, 60, 44, 50);
                else ui_fill(geo.gx, geo.gy + i * q, (float)n * q, (float)q, 214, 60, 44, 50);
            }
    for (int c = 0; c < n * n; c++) {
        int v = G.st[c];
        if (!v) continue;
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c), s = bench_pop(&pop, c, 0.35f);
        if (!solved && bn_triple(&G, c)) ui_mark_glow(mx, my, 1.25f * q);
        ui_flat_at(t_disc[v - 1], mx, my, 0.8f * q * s, 1, 0);
    }
    bench_marks_back(G.st, real, n * n);
    bench_touch_ring(&touch, &geo);
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * q);
    bench_wave_cells(&win, &geo, 2.3f);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    if (bn_solved(&G)) return;
    if (ui_tool_hit(x, y) == T_HINT) {
        int c = bn_hint(&G);
        if (c < 0) { ui_sound(SND_DENY); return; }
        bench_flash_start(&hint, c);
        bench_flash_start(&pop, c);
        ui_sound(SND_PLACE);
        after(c);
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
    if (a < 0) return;
    if (to) bench_flash_start(&pop, a);
    ui_sound(to ? SND_PLACE : SND_TAP);
    after(a);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the menu's and the picker's cards: a puzzle of the size, its given discs (the menu's: half solved); under the
 * picker's, its level in ebony stones ---------------------------------------------------------------------------- */
static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 4 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    bn_puzzle p;
    uint8_t sol[BN_CELLS];
    bn_make(bn_size(v), bn_level(v), 2026u + (uint32_t)v, &p, sol);
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p.n;
    bench_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4);
    if (!b) return;
    paint_givens(&g, &p, b);
    int n = p.n, q = g.cs;
    for (int c = 0; c < n * n; c++) {
        int v2 = p.given[c] ? p.given[c] : variant < 0 && c < n * n / 2 ? sol[c] : 0;
        if (v2) disc_into(b, g.w, g.h, v2 - 1, g.ox + (c % n + 0.5f) * q, g.oy + (c / n + 0.5f) * q, (int)(0.8f * q));
    }
    int ox = (size - side) / 2 + (int)g.bx, oy = (variant < 0 ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, bn_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

static const game binairo_game = {
    .id = "binairo", .order = 411, .variants = BN_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {BN_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(binairo_game)
