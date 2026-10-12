/*
 * Light Up in RetroStone Board, for one player: the house plank ruled in cells, the walls stained dark with their
 * numbers in cream, brass lamps whose light warms every cell they reach (the rules: lightup_rules.c; what the logic
 * grids share: logicgrid.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a white cell to put a lamp in it (again: take it out); hold it a moment to put a dot instead (a note: no lamp
 * here). Drag along a row or a column to put dots in the cells of the stretch that were as the first one (or take
 * them out of a stretch of dots). The buttons at the right end of the bar: what a tap does (a lamp, or a dot: tap it
 * to swap tap and hold), and the hint (a wrong cell put right, else the next cell reasoning finds, in gold light).
 * Two lamps that light each other glow red; a number with too many lamps round it turns red, one with all its
 * lamps fades. Undo takes back one action, all the way to the start. The bar: the moves. Solved: a wave of gold light
 * runs over the board from the last cell.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/logicgrid.h"
#include "games/lightup.h"
#include "games/puzzles.h"

static const uint8_t BRASS[3] = {190, 140, 62}, GLASS[3] = {255, 238, 176}, NUMBER[3] = {246, 230, 196};

enum { K_LAMP, K_DOT };                     /* the bar's mode button: what a tap puts */

static lu_game G;
static gl_geo geo;
static gl_touch touch;
static gl_light hint, win;
static int dots;                                /* taps put dots (the bar's button) */
static int serial;                              /* the puzzle's number: its board remade */
static struct { int cell; Uint32 t0; } pop = {-1, 0};

static box last_box, barbox;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_lamp, *t_dot, *t_glow, *t_small;
static puz_font f_num;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a brass lamp seen from above: a glass globe in a brass ring, lit */
static void lamp_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.36f, 0.36f);
    art_fig_paint(o, size, f, BRASS, GL_DARK, 0.03f, 0.45f);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.26f, 0.26f);
    art_fig_paint(o, size, f, GLASS, BRASS, 0.025f, 0.35f);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.08f, 0.08f);
    art_fig_paint(o, size, f, BRASS, BRASS, 0, 0.3f);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.4f, 0.38f, 0.06f, 0.04f);
    art_fig_paint(o, size, f, PUZ_WHITE, PUZ_WHITE, 0, 0);
}

static void dot_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.11f, 0.11f);
    art_fig_paint(o, size, f, PUZ_INK, PUZ_INK, 0, 0);
}

/* the cells a puzzle's lamps light, warmed (a picture's: board o, side S, the grid at ox, oy, cells of q) */
static void warm(uint8_t *o, int S, float ox, float oy, int q, int x, int y, float a)
{
    float x0 = ox + x * q, y0 = oy + y * q;
    for (int py = (int)y0; py < (int)(y0 + q) && py < S; py++)
        for (int px = (int)x0; px < (int)(x0 + q) && px < S; px++) {
            uint8_t *p = o + 4 * ((size_t)py * S + px);
            p[0] = (uint8_t)(p[0] + (255 - p[0]) * a);
            p[1] = (uint8_t)(p[1] + (222 - p[1]) * a);
            p[2] = (uint8_t)(p[2] + (130 - p[2]) * a * 0.6f);
        }
}

/* the plank with its walls (a puzzle's: n x n), into a picture laid out by g */
static void paint_board(const gl_geo *g, const lu_puzzle *p, uint8_t *o)
{
    gl_paint(g, o);
    int S = (int)g->side, q = g->cs, in = (int)fmaxf(1, 0.03f * q);
    for (int c = 0; c < p->n * p->n; c++) {
        if (p->wall[c] == LU_WHITE) continue;
        float x = g->ox + c % p->n * q, y = g->oy + c / p->n * q;
        gl_shade(o, S, S, x + in, y + in, x + q - in, y + q - in, 0.3f);
    }
}

/* the token: a wooden tile with a lamp lit on it */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    warm(b, w, 0, 0, w, 0, 0, 0.4f);
    lamp_pic(w, b);
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
        int S = (int)geo.side, q = geo.cs;
        ui_free(&t_board);
        paint_board(&geo, &G.p, ui_buf(S, S));
        t_board = ui_tex(S, S, ui_buf(S, S));
        ui_free(&t_lamp);
        memset(ui_buf(q, q), 0, (size_t)q * q * 4);
        lamp_pic(q, ui_buf(q, q));
        t_lamp = ui_tex(q, q, ui_buf(q, q));
        ui_free(&t_dot);
        memset(ui_buf(q, q), 0, (size_t)q * q * 4);
        dot_pic(q, ui_buf(q, q));
        t_dot = ui_tex(q, q, ui_buf(q, q));
        int gs = (int)(1.6f * q);              /* the light round a lamp */
        static const uint8_t WARM[3] = {255, 206, 96};
        ui_free(&t_glow);
        art_disc(gs, WARM, 0.55f, 1.6f, ui_buf(gs, gs));
        t_glow = ui_tex(gs, gs, ui_buf(gs, gs));
        puz_font_make(&f_num, (int)(0.62f * q), 1);
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
    lu_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    dots = 0;
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return lu_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return gl_wave_busy(&win, &geo); }

static void after(int cell)                     /* an action done: solved by it, the wave from that cell */
{
    if (lu_solved(&G) && !win.on) {
        gl_light_start(&win, cell);
        hint.on = 0;
    }
    ui_animate();
}

static int undo(void)
{
    int c = lu_undo(&G);
    if (c < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = lu_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* what the finger would make of the cell c now (its state; the cell's own if untouched): a press shows its tap, held
 * its mark; a drag every cell of the stretch that is as the first was */
static int preview(int c)
{
    int a = touch.a;
    if (!touch.on || !lu_white(&G, a) || !lu_white(&G, c)) return G.st[c];
    if (touch.moved ? !gl_in_run(&geo, c, a, touch.b) || G.st[c] != G.st[a] : c != a) return G.st[c];
    if (touch.moved) return lu_hold_state(&G, a);   /* a drag: marks */
    return gl_holding(&touch) == dots ? lu_tap_state(&G, a) : lu_hold_state(&G, a);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = lu_solved(&G);
    ui_put(t_board, geo.bx, geo.by, geo.side, geo.side);
    static uint16_t shown[LU_CELLS], real[LU_CELLS];
    for (int c = 0; c < n * n; c++) shown[c] = (uint16_t)preview(c);
    memcpy(real, G.st, sizeof real);            /* the light as the finger would leave it */
    memcpy(G.st, shown, sizeof shown);
    for (int c = 0; c < n * n; c++) {
        float x, y;
        gl_cell_xy(&geo, c, &x, &y);
        if (G.p.wall[c] != LU_WHITE) continue;
        if (lu_lit(&G, c)) ui_fill(x, y, (float)q, (float)q, 255, 214, 110, solved ? 130 : 105);
        else ui_fill(x, y, (float)q, (float)q, 40, 30, 50, 66);   /* the dark: a cool shade */
    }
    for (int c = 0; c < n * n; c++) {
        float mx = gl_mid_x(&geo, c), my = gl_mid_y(&geo, c);
        int w = G.p.wall[c];
        if (w >= 0) {                           /* a number: red with too many lamps, faded with all of them */
            int k = lu_lamps_round(&G, c);
            puz_number(&f_num, mx, my + 0.02f * q, 0.62f * q, w, k > w ? GL_RED : NUMBER, k == w && w ? 110 : 255);
            continue;
        }
        if (w != LU_WHITE) continue;
        if (shown[c] == LU_DOT) ui_put_c(t_dot, mx, my, (float)q, (float)q);
        if (shown[c] != LU_LAMP) continue;
        if (!solved && lu_clash(&G, c)) ui_mark_glow(mx, my, 1.3f * q);
        float s = c == pop.cell ? gl_pop(pop.t0) : 1;
        ui_put_c(t_glow, mx, my, 1.6f * q, 1.6f * q);
        ui_shadow_flat(mx, my, 0.72f * q * s, 0.72f * q * s, 0.6f);
        ui_put_c(t_lamp, mx, my, q * s, q * s);
    }
    memcpy(G.st, real, sizeof real);
    gl_touch_ring(&touch, &geo);
    gl_hint_draw(&hint, &geo);
    gl_wave_draw(&win, &geo);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (lu_solved(&G)) return;
    gl_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { gl_motion(&touch, &geo, x, y, 1); }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int a = touch.a, kind = gl_release(&touch);
    if (kind == GL_NOTHING) return;
    if (!lu_white(&G, a)) { ui_sound(SND_DENY); return; }
    int mark = kind >= GL_DRAG || (kind == GL_HOLD) != dots;   /* a drag marks; the button swaps tap and hold */
    int to = mark ? lu_hold_state(&G, a) : lu_tap_state(&G, a), cells[LU_MAX], k = 0, run[LU_MAX];
    int nr = kind >= GL_DRAG ? gl_run(&geo, a, touch.b, run) : (run[0] = a, 1);
    for (int i = 0; i < nr; i++)
        if (lu_white(&G, run[i]) && G.st[run[i]] == G.st[a]) cells[k++] = run[i];
    if (!lu_paint(&G, cells, k, to)) return;
    if (to == LU_LAMP) {
        pop.cell = a;
        pop.t0 = ui_ms();
    }
    ui_sound(to == LU_LAMP ? SND_PLACE : SND_TAP);
    after(a);
}

/* ---- the bar: the moves; what a tap does, the hint ------------------------------------------------------------- */
static void paint_button(int kind, int size, uint8_t *o)   /* what a tap puts: a lamp, or a cream dot */
{
    if (kind == K_LAMP) { lamp_pic(size, o); return; }
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.2f, 0.2f);
    art_fig_paint(o, size, f, GL_CREAM, GL_CREAM, 0, 0);
}

static void bar(int player, box b)
{
    (void)player;
    barbox = b;
    gl_bar_moves(b, t_small, G.hist.moves, 2);
    if (lu_solved(&G)) return;
    float x, y, d;
    gl_bar_spot(b, 1, 2, &x, &y, &d);
    ui_button_paint(x, y, d, paint_button, dots ? K_DOT : K_LAMP, 0.66f, dots ? UI_ON : 0);
    gl_bar_spot(b, 0, 2, &x, &y, &d);
    ui_button(x, y, d, ICON_HINT, 0);
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (lu_solved(&G)) return 0;
    if (gl_bar_hit(barbox, 1, 2, x, y)) {
        dots = !dots;
        ui_sound(SND_TAP);
        ui_animate();
        return 1;
    }
    if (!gl_bar_hit(barbox, 0, 2, x, y)) return 0;
    int c = lu_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return 1; }
    gl_light_start(&hint, c);
    pop.cell = c;
    pop.t0 = ui_ms();
    ui_sound(SND_PLACE);
    after(c);
    return 1;
}

/* ---- the menu's and the picker's cards: a puzzle of the size, its lamps lit (the menu's: half of them); under the
 * picker's, its level in ebony stones --------------------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 ? size : (int)(0.84f * size);
    lu_puzzle p;
    uint8_t sol[LU_CELLS];
    lu_make(lu_size(v), lu_level(v), 2026u + (uint32_t)v, &p, sol);
    gl_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p.n;
    gl_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)side * side, 4);
    if (!b) return;
    paint_board(&g, &p, b);
    lu_game T;
    memset(&T, 0, sizeof T);
    T.p = p;
    int n = p.n, q = g.cs;
    for (int c = 0; c < n * n; c++)
        if (sol[c] && (variant >= 0 || c < n * n / 2)) T.st[c] = LU_LAMP;
    uint8_t *lamp = calloc((size_t)q * q, 4);
    for (int c = 0; c < n * n; c++) {
        float x = g.ox + c % n * q, y = g.oy + c / n * q;
        if (lu_lit(&T, c)) warm(b, side, g.ox, g.oy, q, c % n, c / n, 0.38f);
        if (p.wall[c] >= 0 && q >= 9)
            puz_digits(b, side, side, p.wall[c], x + q / 2.0f, y + q * 0.52f, 0.62f * q, NUMBER, 1, 1);
        if (T.st[c] == LU_LAMP && lamp) {
            memset(lamp, 0, (size_t)q * q * 4);
            lamp_pic(q, lamp);
            art_blend(b, side, side, lamp, q, q, (int)x, (int)y);
        }
    }
    free(lamp);
    art_blend(rgba, size, size, b, side, side, (size - side) / 2, variant < 0 ? (size - side) / 2 : 0);
    free(b);
    if (variant >= 0) gl_stones(rgba, size, lu_level(v), side);
}

static const game lightup_game = {
    .id = "lightup", .order = 410, .variants = LU_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(lightup_game)
