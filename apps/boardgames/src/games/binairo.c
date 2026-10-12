/*
 * Binairo in RetroStone Board, for one player: the house plank ruled in cells, the given discs on cells stained
 * darker, the player's ivory and ebony discs (the house's counters) laid on the wood (the rules: binairo_rules.c;
 * what the logic grids share: logicgrid.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap an empty cell for an ivory disc, again for an ebony one, again to empty it; hold it a moment to go the other
 * way round (ebony first). Drag along a row or a column to do the same to every cell of the stretch that was as the
 * first one. Three alike in a row glow red; a row or a column with too many of a colour, or full and alike another,
 * is tinted red. The button at the right end of the bar: the hint (a wrong disc put right, else the next cell
 * reasoning finds, in gold light). Undo takes back one action, all the way to the start. The bar: the moves.
 * Solved: a wave of gold light runs over the board from the last cell.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/logicgrid.h"
#include "games/binairo.h"
#include "games/puzzles.h"

static bn_game G;
static gl_geo geo;
static gl_touch touch;
static gl_light hint, win;
static int serial;
static struct { int cell; Uint32 t0; } pop = {-1, 0};

static box last_box, barbox;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_disc[2], *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank with the given cells stained (a puzzle's), into a picture laid out by g */
static void paint_board(const gl_geo *g, const bn_puzzle *p, uint8_t *o)
{
    gl_paint(g, o);
    int S = (int)g->side, q = g->cs, in = (int)fmaxf(1, 0.04f * q);
    for (int c = 0; c < p->n * p->n; c++) {
        if (!p->given[c]) continue;
        float x = g->ox + c % p->n * q, y = g->oy + c / p->n * q;
        gl_shade(o, S, S, x + in, y + in, x + q - in, y + q - in, 0.7f);
    }
}

/* a disc (0 ivory, 1 ebony) painted into a picture at (x, y), d across */
static void disc_into(uint8_t *o, int S, int colour, float cx, float cy, int d)
{
    uint8_t *b = calloc((size_t)d * d, 4);
    if (!b) return;
    art_counter(COUNTER_DISC, colour, d, b);
    art_blend(o, S, S, b, d, d, (int)lroundf(cx - d / 2.0f), (int)lroundf(cy - d / 2.0f));
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
    disc_into(b, w, 0, w * 0.32f, w * 0.34f, d);
    disc_into(b, w, 1, w * 0.68f, w * 0.66f, d);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    geo.cols = geo.rows = G.p.n;
    int changed = gl_fit(&geo, b);
    int key = ((int)geo.side * 64 + serial % 64) * 16 + G.p.n;
    if (changed || made_key != key) {
        made_key = key;
        int S = (int)geo.side, d = (int)(0.8f * geo.cs);
        ui_free(&t_board);
        paint_board(&geo, &G.p, ui_buf(S, S));
        t_board = ui_tex(S, S, ui_buf(S, S));
        for (int i = 0; i < 2; i++) {
            ui_free(&t_disc[i]);
            memset(ui_buf(d, d), 0, (size_t)d * d * 4);
            art_counter(COUNTER_DISC, i, d, ui_buf(d, d));
            t_disc[i] = ui_tex(d, d, ui_buf(d, d));
        }
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
    bn_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return bn_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return gl_wave_busy(&win, &geo); }

static void after(int cell)
{
    if (bn_solved(&G) && !win.on) {
        gl_light_start(&win, cell);
        hint.on = 0;
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

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* what the finger would make of the cell c now: a press shows its tap, held the other way round; a drag every cell
 * of the stretch that is as the first was */
static int preview(int c)
{
    int a = touch.a;
    if (!touch.on || !bn_free(&G, a) || !bn_free(&G, c)) return G.st[c];
    if (touch.moved ? !gl_in_run(&geo, c, a, touch.b) || G.st[c] != G.st[a] : c != a) return G.st[c];
    int hold = touch.moved ? touch.held : gl_holding(&touch);
    return hold ? bn_hold_state(&G, a) : bn_tap_state(&G, a);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = bn_solved(&G);
    ui_put(t_board, geo.bx, geo.by, geo.side, geo.side);
    static uint16_t real[BN_CELLS];
    memcpy(real, G.st, sizeof real);            /* the board as the finger would leave it */
    for (int c = 0; c < n * n; c++) G.st[c] = (uint16_t)preview(c);
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
        float mx = gl_mid_x(&geo, c), my = gl_mid_y(&geo, c), s = c == pop.cell ? gl_pop(pop.t0) : 1;
        if (!solved && bn_triple(&G, c)) ui_mark_glow(mx, my, 1.25f * q);
        ui_flat_at(t_disc[v - 1], mx, my, 0.8f * q * s, 1, 0);
    }
    memcpy(G.st, real, sizeof real);
    gl_touch_ring(&touch, &geo);
    gl_hint_draw(&hint, &geo);
    gl_wave_draw(&win, &geo);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (bn_solved(&G)) return;
    gl_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { gl_motion(&touch, &geo, x, y, 1); }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int a = touch.a, kind = gl_release(&touch);
    if (kind == GL_NOTHING) return;
    if (!bn_free(&G, a)) { ui_sound(SND_DENY); return; }
    int hold = kind == GL_HOLD || kind == GL_HOLD_DRAG;
    int to = hold ? bn_hold_state(&G, a) : bn_tap_state(&G, a), cells[BN_MAX], k = 0, run[BN_MAX];
    int nr = kind >= GL_DRAG ? gl_run(&geo, a, touch.b, run) : (run[0] = a, 1);
    for (int i = 0; i < nr; i++)
        if (bn_free(&G, run[i]) && G.st[run[i]] == G.st[a]) cells[k++] = run[i];
    if (!bn_paint(&G, cells, k, to)) return;
    if (to) {
        pop.cell = a;
        pop.t0 = ui_ms();
    }
    ui_sound(to ? SND_PLACE : SND_TAP);
    after(a);
}

/* ---- the bar: the moves, the hint -------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    barbox = b;
    gl_bar_moves(b, t_small, G.hist.moves, 1);
    if (bn_solved(&G)) return;
    float x, y, d;
    gl_bar_spot(b, 0, 1, &x, &y, &d);
    ui_button(x, y, d, ICON_HINT, 0);
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (bn_solved(&G) || !gl_bar_hit(barbox, 0, 1, x, y)) return 0;
    int c = bn_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return 1; }
    gl_light_start(&hint, c);
    pop.cell = c;
    pop.t0 = ui_ms();
    ui_sound(SND_PLACE);
    after(c);
    return 1;
}

/* ---- the menu's and the picker's cards: a puzzle of the size, its given discs (the menu's: half solved); under the
 * picker's, its level in ebony stones ---------------------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? 4 : variant, side = variant < 0 ? size : (int)(0.84f * size);
    bn_puzzle p;
    uint8_t sol[BN_CELLS];
    bn_make(bn_size(v), bn_level(v), 2026u + (uint32_t)v, &p, sol);
    gl_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p.n;
    gl_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)side * side, 4);
    if (!b) return;
    paint_board(&g, &p, b);
    int n = p.n, q = g.cs;
    for (int c = 0; c < n * n; c++) {
        int v2 = p.given[c] ? p.given[c] : variant < 0 && c < n * n / 2 ? sol[c] : 0;
        if (v2) disc_into(b, side, v2 - 1, g.ox + (c % n + 0.5f) * q, g.oy + (c / n + 0.5f) * q, (int)(0.8f * q));
    }
    art_blend(rgba, size, size, b, side, side, (size - side) / 2, variant < 0 ? (size - side) / 2 : 0);
    free(b);
    if (variant >= 0) gl_stones(rgba, size, bn_level(v), side);
}

static const game binairo_game = {
    .id = "binairo", .order = 411, .variants = BN_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(binairo_game)
