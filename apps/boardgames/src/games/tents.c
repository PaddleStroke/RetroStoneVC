/*
 * Tents in RetroStone Board, for one player: the house plank ruled in cells, the trees painted on it, the rows' and
 * columns' tent counts burned along darker bands to the left and on top, amber tents and tufts of grass (the rules:
 * tents_rules.c; what the logic grids share: logicgrid.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to pitch a tent (again: take it down); hold it a moment to put grass instead (a note: no tent here).
 * Drag along a row or a column to put grass in the cells of the stretch that were as the first one (or take it out of
 * a stretch of grass). The buttons at the right end of the bar: what a tap does (a tent, or grass: tap it to swap tap
 * and hold), and the hint (a wrong
 * cell put right, else the next cell reasoning finds, in gold light). Tents touching glow red, and so does a tree
 * left with no room for its tent; a count turns red past its tents and fades once they are all there. Undo takes
 * back one action, all the way to the start. The bar: the moves. Solved: a wave of gold light runs over the field.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/logicgrid.h"
#include "games/puzzles.h"
#include "games/tents.h"

#define BAND 0.72f                              /* the counts' bands, in cells */

static const uint8_t LEAF[3] = {72, 136, 80}, LEAF_DARK[3] = {34, 74, 44}, TRUNK[3] = {112, 74, 48},
                     CANVAS[3] = {222, 160, 70}, CANVAS_DARK[3] = {120, 66, 30}, DOOR[3] = {64, 40, 26},
                     GRASS[3] = {104, 156, 64}, COUNT[3] = {44, 30, 20};

enum { K_TENT, K_GRASS };                   /* the bar's mode button: what a tap puts */

static tn_game G;
static gl_geo geo;
static gl_touch touch;
static gl_light hint, win;
static int grass;                               /* taps put grass (the bar's button) */
static int serial;
static struct { int cell; Uint32 t0; } pop = {-1, 0};

static box last_box, barbox;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_tent, *t_grass, *t_small;
static puz_font f_count;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a tree seen from the side: a round crown of leaves on a short trunk */
static void tree_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, 0.44f, 0.56f, 0.56f, 0.86f);
    art_fig_paint(o, size, f, TRUNK, GL_DARK, 0.03f, 0.3f);
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
    art_fig_paint(o, size, f, TRUNK, GL_DARK, 0.02f, 0);
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

/* a picture made by paint(size, rgba) laid over another (S square) at (x, y), size across */
static void pic_into(uint8_t *o, int S, void (*paint)(int, uint8_t *), float x, float y, int size)
{
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    paint(size, b);
    art_blend(o, S, S, b, size, size, (int)lroundf(x), (int)lroundf(y));
    free(b);
}

/* the plank with its bands and its trees (a field's), into a picture laid out by g */
static void paint_board(const gl_geo *g, const tn_puzzle *p, uint8_t *o)
{
    gl_paint(g, o);
    int S = (int)g->side, q = g->cs;
    for (int c = 0; c < p->n * p->n; c++)
        if (p->tree[c]) pic_into(o, S, tree_pic, g->ox + c % p->n * q, g->oy + c / p->n * q, q);
}

/* the token: a tent beside a tree on a wooden tile */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2, d = (int)(w * 0.62f);
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    pic_into(b, w, tree_pic, w * 0.02f, w * 0.06f, d);
    pic_into(b, w, tent_pic, w * 0.36f, w * 0.34f, d);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static void fit(gl_geo *g, int n, box b)
{
    g->cols = g->rows = n;
    g->lm = g->tm = BAND;
    g->rm = g->bm = 0;
    gl_fit(g, b);
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
    int was = geo.cs;
    fit(&geo, G.p.n, b);
    int key = ((int)geo.side * 64 + serial % 64) * 16 + G.p.n;
    if (was != geo.cs || made_key != key) {
        made_key = key;
        int S = (int)geo.side, q = geo.cs;
        ui_free(&t_board);
        paint_board(&geo, &G.p, ui_buf(S, S));
        t_board = ui_tex(S, S, ui_buf(S, S));
        ui_free(&t_tent);
        t_tent = pic_tex(tent_pic, q);
        ui_free(&t_grass);
        t_grass = pic_tex(grass_pic, q);
        puz_font_make(&f_count, (int)(0.56f * q), 1);
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
    hint.on = win.on = 0;
    pop.cell = -1;
}

static void start(int variant)
{
    tn_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    grass = 0;
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return tn_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return gl_wave_busy(&win, &geo); }

static void after(int cell)
{
    if (tn_solved(&G) && !win.on) {
        gl_light_start(&win, cell);
        hint.on = 0;
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

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static int preview(int c)
{
    int a = touch.a;
    if (!touch.on || !tn_free(&G, a) || !tn_free(&G, c)) return G.st[c];
    if (touch.moved ? !gl_in_run(&geo, c, a, touch.b) || G.st[c] != G.st[a] : c != a) return G.st[c];
    if (touch.moved) return tn_hold_state(&G, a);   /* a drag: marks */
    return gl_holding(&touch) == grass ? tn_tap_state(&G, a) : tn_hold_state(&G, a);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = tn_solved(&G);
    ui_put(t_board, geo.bx, geo.by, geo.side, geo.side);
    static uint16_t real[TN_CELLS];
    memcpy(real, G.st, sizeof real);            /* the field as the finger would leave it */
    for (int c = 0; c < n * n; c++) G.st[c] = (uint16_t)preview(c);
    for (int way = 0; way < 2; way++)           /* the counts: red past their tents, faded with all of them */
        for (int i = 0; i < n; i++) {
            int want = G.p.count[way][i], have = tn_line_tents(&G, way, i);
            if (want < 0) continue;
            float x = way ? geo.gx + (i + 0.5f) * q : geo.gx - BAND * q / 2;
            float y = way ? geo.gy - BAND * q / 2 : geo.gy + (i + 0.5f) * q;
            puz_number(&f_count, x, y, 0.56f * q, want, have > want ? GL_RED : COUNT, have == want ? 100 : 255);
        }
    for (int c = 0; c < n * n; c++) {
        float mx = gl_mid_x(&geo, c), my = gl_mid_y(&geo, c);
        if (G.p.tree[c]) {
            if (!solved && tn_tree_lost(&G, c)) ui_mark_glow(mx, my, 1.25f * q);
            continue;
        }
        if (G.st[c] == TN_GRASS) ui_put_c(t_grass, mx, my, (float)q, (float)q);
        if (G.st[c] != TN_TENT) continue;
        if (!solved && tn_touching(&G, c)) ui_mark_glow(mx, my, 1.25f * q);
        float s = c == pop.cell ? gl_pop(pop.t0) : 1;
        ui_shadow_flat(mx + 0.04f * q, my + 0.22f * q * s, 0.7f * q * s, 0.24f * q * s, 0.55f);
        ui_put_c(t_tent, mx, my, q * s, q * s);
    }
    memcpy(G.st, real, sizeof real);
    gl_touch_ring(&touch, &geo);
    gl_hint_draw(&hint, &geo);
    gl_wave_draw(&win, &geo);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (tn_solved(&G)) return;
    gl_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { gl_motion(&touch, &geo, x, y, 1); }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int a = touch.a, kind = gl_release(&touch);
    if (kind == GL_NOTHING) return;
    if (!tn_free(&G, a)) { ui_sound(SND_DENY); return; }
    int mark = kind >= GL_DRAG || (kind == GL_HOLD) != grass;   /* a drag marks; the button swaps tap and hold */
    int to = mark ? tn_hold_state(&G, a) : tn_tap_state(&G, a), cells[TN_MAX], k = 0, run[TN_MAX];
    int nr = kind >= GL_DRAG ? gl_run(&geo, a, touch.b, run) : (run[0] = a, 1);
    for (int i = 0; i < nr; i++)
        if (tn_free(&G, run[i]) && G.st[run[i]] == G.st[a]) cells[k++] = run[i];
    if (!tn_paint(&G, cells, k, to)) return;
    if (to == TN_TENT) {
        pop.cell = a;
        pop.t0 = ui_ms();
    }
    ui_sound(to == TN_TENT ? SND_PLACE : SND_TAP);
    after(a);
}

/* ---- the bar: the moves; what a tap does, the hint ------------------------------------------------------------- */
static void paint_button(int kind, int size, uint8_t *o)   /* what a tap puts: a tent, or grass */
{
    (kind == K_TENT ? tent_pic : grass_pic)(size, o);
}

static void bar(int player, box b)
{
    (void)player;
    barbox = b;
    gl_bar_moves(b, t_small, G.hist.moves, 2);
    if (tn_solved(&G)) return;
    float x, y, d;
    gl_bar_spot(b, 1, 2, &x, &y, &d);
    ui_button_paint(x, y, d, paint_button, grass ? K_GRASS : K_TENT, 0.72f, grass ? UI_ON : 0);
    gl_bar_spot(b, 0, 2, &x, &y, &d);
    ui_button(x, y, d, ICON_HINT, 0);
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (tn_solved(&G)) return 0;
    if (gl_bar_hit(barbox, 1, 2, x, y)) {
        grass = !grass;
        ui_sound(SND_TAP);
        ui_animate();
        return 1;
    }
    if (!gl_bar_hit(barbox, 0, 2, x, y)) return 0;
    int c = tn_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return 1; }
    gl_light_start(&hint, c);
    pop.cell = c;
    pop.t0 = ui_ms();
    ui_sound(SND_PLACE);
    after(c);
    return 1;
}

/* ---- the menu's and the picker's cards: a field of the size with its tents (the menu's: half of them); under the
 * picker's, its level in ebony stones ---------------------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 ? size : (int)(0.84f * size);
    tn_puzzle p;
    uint8_t sol[TN_CELLS];
    tn_make(tn_size(v), tn_level(v), 2026u + (uint32_t)v, &p, sol);
    gl_geo g;
    memset(&g, 0, sizeof g);
    fit(&g, p.n, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)side * side, 4);
    if (!b) return;
    paint_board(&g, &p, b);
    int n = p.n, q = g.cs;
    for (int c = 0; c < n * n; c++)
        if (sol[c] && (variant >= 0 || c < n * n / 2))
            pic_into(b, side, tent_pic, g.ox + c % n * q, g.oy + c / n * q, q);
    if (q >= 9)
        for (int way = 0; way < 2; way++)
            for (int i = 0; i < n; i++) {
                float x = way ? g.ox + (i + 0.5f) * q : g.ox - BAND * q / 2;
                float y = way ? g.oy - BAND * q / 2 : g.oy + (i + 0.5f) * q;
                puz_digits(b, side, side, p.count[way][i], x, y, 0.56f * q, COUNT, 1, 1);
            }
    art_blend(rgba, size, size, b, side, side, (size - side) / 2, variant < 0 ? (size - side) / 2 : 0);
    free(b);
    if (variant >= 0) gl_stones(rgba, size, tn_level(v), side);
}

static const game tents_game = {
    .id = "tents", .order = 412, .variants = TN_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(tents_game)
