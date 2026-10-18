/*
 * Tents in RetroStone Board, for one player: the house plank ruled in cells, the trees painted on it, the rows' and
 * columns' tent counts burned along darker bands to the left and on top, amber tents and tufts of grass (the rules:
 * tents_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to pitch a tent (again: take it down); hold it a moment to put grass instead (a note: no tent here).
 * Drag along a row or a column to put grass in the cells of the stretch that were as the first one (or take it out of
 * a stretch of grass). The shell's tools: what a tap does (a tent, or grass: tap it to swap tap and hold), and the
 * hint (a wrong cell put right, else the next cell reasoning finds, in gold light). Tents touching glow red, and so
 * does a tree left with no room for its tent; a count turns red past its tents and fades once they are all there.
 * Undo takes back one action, all the way to the start. The bar: the moves. Solved: a wave of gold light runs over
 * the field; the stars count the hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/tents_gen.c: tents_levels.h), or its
 * die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/tents.h"
#include "games/tents_levels.h"

#define BAND 0.72f                              /* the counts' bands, in cells */

static const uint8_t LEAF[3] = {72, 136, 80}, LEAF_DARK[3] = {34, 74, 44}, TRUNK[3] = {112, 74, 48},
                     CANVAS[3] = {222, 160, 70}, CANVAS_DARK[3] = {120, 66, 30}, DOOR[3] = {64, 40, 26},
                     GRASS[3] = {104, 156, 64}, COUNT[3] = {44, 30, 20};

enum { K_TENT, K_GRASS };                       /* the mode tool: what a tap puts */
enum { T_MODE, T_HINT, TOOLS };

static tn_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int grass;                               /* taps put grass (the mode tool) */
static int serial;

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_tent, *t_grass, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a tree seen from the side: a round crown of leaves on a short trunk */
static void tree_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, 0.44f, 0.56f, 0.56f, 0.86f);
    art_fig_paint(o, size, f, TRUNK, PUZ_INK, 0.03f, 0.3f);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.35f, 0.5f, 0.2f, 0.18f);
    art_ellipse(f, ART_PART, 0.65f, 0.5f, 0.2f, 0.18f);
    art_ellipse(f, ART_PART, 0.5f, 0.34f, 0.24f, 0.22f);
    art_fig_paint(o, size, f, LEAF, LEAF_DARK, 0.035f, 0.5f);
}

/* a tent: an amber canvas, its dark door, its pole's tip */
static void tent_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, 0.48f, 0.14f, 0.52f, 0.3f);
    art_fig_paint(o, size, f, TRUNK, PUZ_INK, 0.02f, 0);
    f = art_fig_new();
    const float body[] = {0.5f, 0.2f, 0.86f, 0.8f, 0.14f, 0.8f};
    art_poly(f, ART_PART, body, 3);
    art_fig_paint(o, size, f, CANVAS, CANVAS_DARK, 0.04f, 0.4f);
    f = art_fig_new();
    const float door[] = {0.5f, 0.42f, 0.62f, 0.8f, 0.38f, 0.8f};
    art_poly(f, ART_PART, door, 3);
    art_fig_paint(o, size, f, DOOR, DOOR, 0, 0);
}

/* grass: a tuft of three blades */
static void grass_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    const float a[] = {0.36f, 0.66f, 0.30f, 0.40f, 0.44f, 0.66f};
    const float b[] = {0.45f, 0.66f, 0.50f, 0.32f, 0.56f, 0.66f};
    const float c[] = {0.56f, 0.66f, 0.70f, 0.42f, 0.64f, 0.66f};
    art_poly(f, ART_PART, a, 3);
    art_poly(f, ART_PART, b, 3);
    art_poly(f, ART_PART, c, 3);
    art_fig_paint(o, size, f, GRASS, LEAF_DARK, 0.02f, 0.2f);
}

/* a picture made by paint(size, rgba) laid over another (w x h) at (x, y), size across */
static void pic_into(uint8_t *o, int w, int h, void (*paint)(int, uint8_t *), float x, float y, int size)
{
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    paint(size, b);
    art_blend(o, w, h, b, size, size, (int)lroundf(x), (int)lroundf(y));
    free(b);
}

/* a soft oval of shade on the plank (w x h) round (cx, cy): the ground under a tree, darkest at its middle */
static void ground_shadow(uint8_t *o, int w, int h, float cx, float cy, float rx, float ry, float alpha)
{
    int x0 = (int)fmaxf(0, cx - rx), x1 = (int)fminf((float)w - 1, cx + rx);
    int y0 = (int)fmaxf(0, cy - ry), y1 = (int)fminf((float)h - 1, cy + ry);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, d = sqrtf(dx * dx + dy * dy);
            if (d >= 1) continue;
            float t = d < 0.3f ? 1 : (1 - d) / 0.7f, k = 1 - alpha * t * t * (3 - 2 * t);
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int j = 0; j < 3; j++) p[j] = (uint8_t)(p[j] * k);
        }
}

/* the plank with its bands and its trees (a field's), into a picture laid out by g: each tree on its own shade */
static void paint_field(const bench_geo *g, const tn_puzzle *p, uint8_t *o)
{
    bench_paint_grid(g, o);
    int q = g->cs;
    for (int c = 0; c < p->n * p->n; c++) {
        if (!p->tree[c]) continue;
        float x = g->ox + c % p->n * q, y = g->oy + c / p->n * q;
        ground_shadow(o, g->w, g->h, x + 0.55f * q, y + 0.86f * q, 0.34f * q, 0.09f * q, 0.42f);
        pic_into(o, g->w, g->h, tree_pic, x, y, q);
    }
}

static void paint_board(const bench_geo *g, uint8_t *o) { paint_field(g, &G.p, o); }

/* the token: a tent beside a tree on a wooden tile */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2, d = (int)(w * 0.62f);
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    pic_into(b, w, w, tree_pic, w * 0.02f, w * 0.06f, d);
    pic_into(b, w, w, tent_pic, w * 0.36f, w * 0.34f, d);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static int fit(bench_geo *g, int n, box b)
{
    g->cols = g->rows = n;
    g->lm = g->tm = BAND;
    g->rm = g->bm = 0;
    return bench_fit(g, b);
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
    int changed = fit(&geo, G.p.n, b), key = serial % 4096 * 16 + G.p.n;
    if (changed || made_key != key) {
        made_key = key;
        ui_free(&t_board);
        t_board = bench_tex(&geo, paint_board);
        ui_free(&t_tent);
        t_tent = pic_tex(tent_pic, geo.cs);
        ui_free(&t_grass);
        t_grass = pic_tex(grass_pic, geo.cs);
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
    grass = 0;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    tn_puzzle p;
    if (!tn_decode(tn_levels[level < 0 || level >= TN_LEVELS ? 0 : level], &p) || !tn_load(&G, &p)) tn_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    tn_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return tn_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (tn_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (tn_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = tn_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the marks: a tap a tent, a hold grass, a drag grass --------------------------------------------------------- */
static int st_of(int c) { return G.st[c]; }
static int free_of(int c) { return tn_free(&G, c); }
static int tap_of(int c) { return tn_tap_state(&G, c); }
static int hold_of(int c) { return tn_hold_state(&G, c); }
static int paint(const int *cells, int n, int state) { return tn_paint(&G, cells, n, state); }
static const bench_marks marks = {st_of, free_of, tap_of, hold_of, paint, NULL, 1};

/* ---- the board on the screen, the tools -------------------------------------------------------------------------- */
static void paint_button(int kind, int size, uint8_t *o)   /* what a tap puts: a tent, or grass */
{
    (kind == K_TENT ? tent_pic : grass_pic)(size, o);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = tn_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    static uint16_t real[TN_CELLS];
    bench_marks_show(&marks, &touch, &geo, grass, G.st, real, n * n);   /* the field as the finger would leave it */
    for (int way = 0; way < 2; way++)           /* the counts: red past their tents, faded with all of them */
        for (int i = 0; i < n; i++) {
            int want = G.p.count[way][i], have = tn_line_tents(&G, way, i);
            if (want < 0) continue;
            float x = way ? geo.gx + (i + 0.5f) * q : geo.gx - BAND * q / 2;
            float y = way ? geo.gy - BAND * q / 2 : geo.gy + (i + 0.5f) * q;
            ui_number_ex(x, y, 0.56f * q, want, have > want ? BENCH_RED : COUNT, have == want ? 100 : 255,
                         UI_NUM_TIGHT);
        }
    for (int c = 0; c < n * n; c++) {
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        if (G.p.tree[c]) {
            if (!solved && tn_tree_lost(&G, c)) ui_mark_glow(mx, my, 1.25f * q);
            continue;
        }
        if (G.st[c] == TN_GRASS) ui_put_c(t_grass, mx, my, (float)q, (float)q);
        if (G.st[c] != TN_TENT) continue;
        if (!solved && tn_touching(&G, c)) ui_mark_glow(mx, my, 1.25f * q);
        float s = bench_pop(&pop, c, 0.35f);
        ui_shadow_flat(mx + 0.05f * q, my + 0.3f * q * s, 0.76f * q * s, 0.2f * q * s, 0.5f);   /* on the ground, as a tree's */
        ui_put_c(t_tent, mx, my, q * s, q * s);
    }
    bench_marks_back(G.st, real, n * n);
    bench_touch_ring(&touch, &geo);
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * q);
    bench_wave_cells(&win, &geo, 2.3f);
    ui_tool_paint(T_MODE, paint_button, grass ? K_GRASS : K_TENT, 0.72f, solved ? UI_DIM : grass ? UI_ON : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void tool(int t)
{
    if (t == T_MODE) {
        grass = !grass;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    int c = tn_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, c);
    bench_flash_start(&pop, c);
    ui_sound(SND_PLACE);
    after(c);
}

static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    if (tn_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { tool(t); return; }
    bench_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { bench_motion(&touch, &geo, x, y, 1); }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int to, a = bench_marks_release(&marks, &touch, &geo, grass, &to);
    if (a < 0) return;
    if (to == TN_TENT) bench_flash_start(&pop, a);
    ui_sound(to == TN_TENT ? SND_PLACE : SND_TAP);
    after(a);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the pictures of fields: the menu's and the picker's cards (a field of the size with its tents; the menu's: half
 * of them), the chapters' tabs ----------------------------------------------------------------------------------
 * The picker has two axes (game.axes): the sizes (the field at the medium level), then the levels (the shell's
 * ebony stones). A field with the tents of sol (all, or the first half), side x side, at the top of the card
 * (centred: mid) */
static void paint_field_pic(const tn_puzzle *pp, const uint8_t *sol, int half, int size, int side, int mid,
                            uint8_t *rgba)
{
    tn_puzzle p = *pp;
    bench_geo g;
    memset(&g, 0, sizeof g);
    fit(&g, p.n, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4);
    if (!b) return;
    paint_field(&g, &p, b);
    int n = p.n, q = g.cs;
    for (int c = 0; c < n * n; c++)
        if (sol[c] && (!half || c < n * n / 2))
            pic_into(b, g.w, g.h, tent_pic, g.ox + c % n * q, g.oy + c / n * q, q);
    if (q >= 9)
        for (int way = 0; way < 2; way++)
            for (int i = 0; i < n; i++) {
                float x = way ? g.ox + (i + 0.5f) * q : g.ox - BAND * q / 2;
                float y = way ? g.oy - BAND * q / 2 : g.oy + (i + 0.5f) * q;
                puz_digits(b, g.w, g.h, p.count[way][i], x, y, 0.56f * q, COUNT, 1, 1);
            }
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
}

static void field_pic(int v, int size, int side, int half, uint8_t *rgba)
{
    tn_puzzle p;
    uint8_t sol[TN_CELLS];
    tn_make(tn_size(v), tn_level(v), 2026u + (uint32_t)v, &p, sol);
    paint_field_pic(&p, sol, half, size, side, half, rgba);
}

/* the menu's card (variant -1); a variant's (with its level in ebony stones under it) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 ? size : (int)(0.84f * size);
    field_pic(v, size, side, variant < 0, rgba);
    if (variant >= 0) bench_stones(rgba, size, side, tn_level(v));
}

/* the picker's sizes: the field at the medium level, the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { field_pic(i * 3 + 1, size, size, 0, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= TN_CHAPTERS ? TN_CHAPTERS - 1 : chapter;
    static tn_game T;
    tn_puzzle p;
    if (!tn_decode(tn_levels[(ch + 1) * TN_LEVELS / TN_CHAPTERS - 1], &p) || !tn_load(&T, &p)) return;
    paint_field_pic(&p, T.sol, 0, size, (int)(0.92f * size), 1, rgba);
}

static const game tents_game = {
    .id = "tents", .order = 412, .variants = TN_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {TN_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = TN_LEVELS, .chapters = TN_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(tents_game)
