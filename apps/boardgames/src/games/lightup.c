/*
 * Light Up in RetroStone Board, for one player: the house plank ruled in cells, the walls stained dark with their
 * numbers in cream, brass lamps whose light warms every cell they reach (the rules: lightup_rules.c; what the logic
 * grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a white cell to put a lamp in it (again: take it out); hold it a moment to put a dot instead (a note: no lamp
 * here). Drag along a row or a column to put dots in the cells of the stretch that were as the first one (or take
 * them out of a stretch of dots). The shell's tools: what a tap does (a lamp, or a dot: tap it to swap tap and hold),
 * and the hint (a wrong cell put right, else the next cell reasoning finds, in gold light). Two lamps that light each
 * other glow red; a number with too many lamps round it turns red, one with all its lamps fades. Undo takes back one
 * action, all the way to the start. The bar: the moves. Solved: a wave of gold light runs over the board from the
 * last cell; the stars count the hints and the mistakes (bench.h).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/lightup.h"

static const uint8_t BRASS[3] = {190, 140, 62}, GLASS[3] = {255, 238, 176}, NUMBER[3] = {246, 230, 196};

enum { K_LAMP, K_DOT };                         /* the mode tool: what a tap puts */
enum { T_MODE, T_HINT, TOOLS };

static lu_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int dots;                                /* taps put dots (the mode tool) */
static int serial;                              /* the puzzle's number: its board remade */

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_lamp, *t_dot, *t_glow, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a brass lamp seen from above: a glass globe in a brass ring, lit */
static void lamp_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.36f, 0.36f);
    art_fig_paint(o, size, f, BRASS, PUZ_INK, 0.03f, 0.45f);
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

/* the cells a puzzle's lamps light, warmed (a picture's: w wide, h high, the grid at ox, oy, cells of q) */
static void warm(uint8_t *o, int w, int h, float ox, float oy, int q, int x, int y, float a)
{
    float x0 = ox + x * q, y0 = oy + y * q;
    for (int py = (int)y0; py < (int)(y0 + q) && py < h; py++)
        for (int px = (int)x0; px < (int)(x0 + q) && px < w; px++) {
            uint8_t *p = o + 4 * ((size_t)py * w + px);
            p[0] = (uint8_t)(p[0] + (255 - p[0]) * a);
            p[1] = (uint8_t)(p[1] + (222 - p[1]) * a);
            p[2] = (uint8_t)(p[2] + (130 - p[2]) * a * 0.6f);
        }
}

/* the plank with its walls (a puzzle's), into a picture laid out by g */
static void paint_walls(const bench_geo *g, const lu_puzzle *p, uint8_t *o)
{
    bench_paint_grid(g, o);
    for (int c = 0; c < p->n * p->n; c++)
        if (p->wall[c] != LU_WHITE) bench_stain(g, o, c, 0.3f);
}

static void paint_board(const bench_geo *g, uint8_t *o) { paint_walls(g, &G.p, o); }

/* the token: a wooden tile with a lamp lit on it */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    warm(b, w, w, 0, 0, w, 0, 0, 0.4f);
    lamp_pic(w, b);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
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
    int changed = bench_fit(&geo, b);
    int key = serial % 4096 * 16 + G.p.n;
    if (changed || made_key != key) {
        made_key = key;
        int q = geo.cs, gs = (int)(1.6f * q);   /* the light round a lamp */
        static const uint8_t WARM[3] = {255, 206, 96};
        ui_free(&t_board);
        t_board = bench_tex(&geo, paint_board);
        ui_free(&t_lamp);
        t_lamp = pic_tex(lamp_pic, q);
        ui_free(&t_dot);
        t_dot = pic_tex(dot_pic, q);
        ui_free(&t_glow);
        art_disc(gs, WARM, 0.55f, 1.6f, ui_buf(gs, gs));
        t_glow = ui_tex(gs, gs, ui_buf(gs, gs));
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
    lu_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    dots = 0;
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return lu_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)                     /* an action done: solved by it, the wave from that cell */
{
    if (lu_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (lu_undo(&G) < 0) return 0;
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

/* ---- the marks: a tap a lamp, a hold a dot, a drag dots ---------------------------------------------------------- */
static int st_of(int c) { return G.st[c]; }
static int free_of(int c) { return lu_white(&G, c); }
static int tap_of(int c) { return lu_tap_state(&G, c); }
static int hold_of(int c) { return lu_hold_state(&G, c); }
static int paint(const int *cells, int n, int state) { return lu_paint(&G, cells, n, state); }
static const bench_marks marks = {st_of, free_of, tap_of, hold_of, paint, NULL, 1};

/* ---- the board on the screen, the tools ---------------------------------------------------------------------- */
static void paint_button(int kind, int size, uint8_t *o)   /* what a tap puts: a lamp, or a cream dot */
{
    if (kind == K_LAMP) { lamp_pic(size, o); return; }
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.2f, 0.2f);
    art_fig_paint(o, size, f, PUZ_CREAM, PUZ_CREAM, 0, 0);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = lu_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    static uint16_t real[LU_CELLS];
    bench_marks_show(&marks, &touch, &geo, dots, G.st, real, n * n);   /* the light as the finger would leave it */
    for (int c = 0; c < n * n; c++) {
        float x, y;
        bench_cell_xy(&geo, c, &x, &y);
        if (G.p.wall[c] != LU_WHITE) continue;
        if (lu_lit(&G, c)) ui_fill(x, y, (float)q, (float)q, 255, 214, 110, solved ? 130 : 105);
        else ui_fill(x, y, (float)q, (float)q, 40, 30, 50, 66);   /* the dark: a cool shade */
    }
    for (int c = 0; c < n * n; c++) {
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        int w = G.p.wall[c];
        if (w >= 0) {                           /* a number: red with too many lamps, faded with all of them */
            int k = lu_lamps_round(&G, c);
            ui_number_ex(mx, my + 0.02f * q, 0.62f * q, w, k > w ? BENCH_RED : NUMBER, k == w && w ? 110 : 255,
                         UI_NUM_TIGHT);
            continue;
        }
        if (w != LU_WHITE) continue;
        if (G.st[c] == LU_DOT) ui_put_c(t_dot, mx, my, (float)q, (float)q);
        if (G.st[c] != LU_LAMP) continue;
        if (!solved && lu_clash(&G, c)) ui_mark_glow(mx, my, 1.3f * q);
        float s = bench_pop(&pop, c, 0.35f);
        ui_put_c(t_glow, mx, my, 1.6f * q, 1.6f * q);
        ui_shadow_flat(mx, my, 0.72f * q * s, 0.72f * q * s, 0.6f);
        ui_put_c(t_lamp, mx, my, q * s, q * s);
    }
    bench_marks_back(G.st, real, n * n);
    bench_touch_ring(&touch, &geo);
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * q);
    bench_wave_cells(&win, &geo, 2.3f);
    ui_tool_paint(T_MODE, paint_button, dots ? K_DOT : K_LAMP, 0.66f, solved ? UI_DIM : dots ? UI_ON : 0);
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
    int c = lu_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, c);
    bench_flash_start(&pop, c);
    ui_sound(SND_PLACE);
    after(c);
}

static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    if (lu_solved(&G)) return;
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
    if (to == LU_LAMP) bench_flash_start(&pop, a);
    ui_sound(to == LU_LAMP ? SND_PLACE : SND_TAP);
    after(a);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the menu's and the picker's cards: a puzzle of the size, its lamps lit (the menu's: half of them); under the
 * picker's, its level in ebony stones --------------------------------------------------------------------------- */
static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    lu_puzzle p;
    uint8_t sol[LU_CELLS];
    lu_make(lu_size(v), lu_level(v), 2026u + (uint32_t)v, &p, sol);
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p.n;
    bench_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4);
    if (!b) return;
    paint_walls(&g, &p, b);
    lu_game T;
    memset(&T, 0, sizeof T);
    T.p = p;
    int n = p.n, q = g.cs;
    for (int c = 0; c < n * n; c++)
        if (sol[c] && (variant >= 0 || c < n * n / 2)) T.st[c] = LU_LAMP;
    uint8_t *lamp = calloc((size_t)q * q, 4);
    for (int c = 0; c < n * n; c++) {
        float x = g.ox + c % n * q, y = g.oy + c / n * q;
        if (lu_lit(&T, c)) warm(b, g.w, g.h, g.ox, g.oy, q, c % n, c / n, 0.38f);
        if (p.wall[c] >= 0 && q >= 9)
            puz_digits(b, g.w, g.h, p.wall[c], x + q / 2.0f, y + q * 0.52f, 0.62f * q, NUMBER, 1, 1);
        if (T.st[c] == LU_LAMP && lamp) {
            memset(lamp, 0, (size_t)q * q * 4);
            lamp_pic(q, lamp);
            art_blend(b, g.w, g.h, lamp, q, q, (int)x, (int)y);
        }
    }
    free(lamp);
    int ox = (size - side) / 2 + (int)g.bx, oy = (variant < 0 ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, lu_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

static const game lightup_game = {
    .id = "lightup", .order = 410, .variants = LU_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {LU_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(lightup_game)
