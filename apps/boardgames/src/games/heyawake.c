/*
 * Heyawake in RetroStone Board, for one player: the house plank ruled in cells, the rooms' walls burned in dark ink,
 * each room's number in its top left cell, ebony tiles laid on the shaded cells, a dot of ink on those the player
 * keeps unshaded (the rules: heyawake_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to shade it (again: take the tile off); hold it a moment to put a dot instead (a note: unshaded). Drag
 * along a row or a column to do the same to a stretch (the cells as the first one was). The shell's tools: what a tap
 * does (shade, or a dot: tap it to swap tap and hold), and the hint (a wrong cell put right, else the next cell
 * reasoning finds, in gold light). Shaded cells touching glow red, cells cut off from the others by the shading take a
 * red tint, and so do dots in a line through three rooms; a room's number turns red when it has too many shaded
 * cells and fades when it has just enough (a 0 stays). Undo takes back one action, all the way to the start. The bar:
 * the moves. Solved: a wave of gold light runs over the grid; the stars count the hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/heyawake_gen.c: heyawake_levels.h), or
 * its die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/heyawake.h"
#include "games/heyawake_levels.h"

static const uint8_t EBONY[3] = {62, 56, 54}, EBONY_DIGIT[3] = {150, 140, 132};

enum { K_SHADE, K_DOT };                        /* the mode tool: what a tap puts */
enum { T_MODE, T_HINT, TOOLS };

static hyk_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int dots;                                /* taps put dots (the mode tool) */
static int serial;
static int corner[HYK_CELLS];                   /* each room's top left cell */

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_tile, *t_dot, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* an ebony tile within a cell (a picture size x size): dark, with a faint grain, rounded and bevelled */
static void tile_pic(int size, uint8_t *o)
{
    int in = (int)fmaxf(1, 0.05f * size), w = size - 2 * in;
    if (w < 2) return;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = b + 4 * ((size_t)y * w + x);
            float k = 0.92f + 0.18f * (1 - (float)y / w) + 0.035f * sinf(y * 0.9f + 0.4f * sinf(x * 0.21f));
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(EBONY[i] * k, 0, 255);
            p[3] = 255;
        }
    puz_bevel(b, w, w, 0.12f * w, fmaxf(2, 0.09f * w), 0.34f);
    art_blend(o, size, size, b, w, w, in, in);
    free(b);
}

static void dot_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.1f, 0.1f);
    art_fig_paint(o, size, f, PUZ_INK, PUZ_INK, 0, 0);
}

/* the plank of a puzzle laid out by g: the grid, the rooms' walls in thick ink */
static void paint_grid(const bench_geo *g, const hyk_puzzle *p, uint8_t *o)
{
    bench_paint_grid(g, o);
    int n = p->n, q = g->cs;
    float t = fmaxf(2, 0.09f * q), x0 = g->ox, y0 = g->oy, x1 = x0 + n * q, y1 = y0 + n * q;
    for (int c = 0; c < n * n; c++) {           /* each wall a little longer, to meet the next; never past the rim */
        float x = g->ox + c % n * q, y = g->oy + c / n * q;
        if (c % n < n - 1 && p->room[c] != p->room[c + 1])
            puz_ink_rect(o, g->w, g->h, x + q - t / 2, fmaxf(y0, y - t / 2), x + q + t / 2, fminf(y1, y + q + t / 2),
                         PUZ_INK, 0.9f);
        if (c / n < n - 1 && p->room[c] != p->room[c + n])
            puz_ink_rect(o, g->w, g->h, fmaxf(x0, x - t / 2), y + q - t / 2, fminf(x1, x + q + t / 2), y + q + t / 2,
                         PUZ_INK, 0.9f);
    }
}

static void paint_board(const bench_geo *g, uint8_t *o) { paint_grid(g, &G.p, o); }

/* the token: a wooden tile in two rooms, an ebony tile in one, a 1 */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2, h = w / 2;
    uint8_t *b = calloc((size_t)w * w, 4), *t = calloc((size_t)h * h, 4);
    if (!b || !t) { free(b); free(t); return; }
    art_board_n(1, 1, w, 0, 0, b);
    tile_pic(h, t);
    art_blend(b, w, w, t, h, h, h, h);
    puz_ink_rect(b, w, w, h - 0.04f * w, 0, h + 0.04f * w, (float)w, PUZ_INK, 0.9f);
    puz_digits(b, w, w, 1, w * 0.25f, w * 0.27f, 0.36f * w, PUZ_INK, 1, 1);
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
        ui_free(&t_dot);
        t_dot = pic_tex(dot_pic, geo.cs);
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
    dots = 0;
    for (int r = 0; r < G.p.rooms; r++) corner[r] = -1;
    for (int c = 0; c < G.p.n * G.p.n; c++)
        if (corner[G.p.room[c]] < 0) corner[G.p.room[c]] = c;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    hyk_puzzle p;
    if (!hyk_decode(hyk_levels[level < 0 || level >= HYK_LEVELS ? 0 : level], &p) || !hyk_load(&G, &p))
        hyk_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    hyk_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return hyk_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (hyk_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (hyk_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = hyk_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the marks: a tap shades, a hold puts a dot, a drag as its start -------------------------------------------- */
static int st_of(int c) { return G.st[c]; }
static int free_of(int c) { return hyk_free(&G, c); }
static int tap_of(int c) { return hyk_tap_state(&G, c); }
static int hold_of(int c) { return hyk_hold_state(&G, c); }
static int paint(const int *cells, int n, int state) { return hyk_paint(&G, cells, n, state); }
static const bench_marks marks = {st_of, free_of, tap_of, hold_of, paint, NULL, 0};

/* ---- the board on the screen, the tools -------------------------------------------------------------------------- */
static void paint_button(int kind, int size, uint8_t *o)   /* what a tap puts: an ebony tile, or a cream dot */
{
    if (kind == K_SHADE) {
        tile_pic(size, o);
        return;
    }
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.2f, 0.2f);
    art_fig_paint(o, size, f, PUZ_CREAM, PUZ_CREAM, 0, 0);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = hyk_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    static uint16_t real[HYK_CELLS];
    bench_marks_show(&marks, &touch, &geo, dots, G.st, real, n * n);   /* the grid as the finger would leave it */
    for (int c = 0; c < n * n; c++) {
        float x, y, mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        bench_cell_xy(&geo, c, &x, &y);
        if (G.st[c] == HYK_SHADE) {
            float s = bench_pop(&pop, c, 0.25f);
            ui_put_c(t_tile, mx, my, q * s, q * s);
            if (!solved && hyk_touching(&G, c)) ui_fill(x, y, (float)q, (float)q, 214, 60, 44, 120);
            continue;
        }
        if (!solved && (hyk_apart(&G, c) || hyk_long(&G, c))) ui_fill(x, y, (float)q, (float)q, 214, 60, 44, 60);
        if (G.st[c] != HYK_DOT) continue;
        if (corner[G.p.room[c]] == c && G.p.num[G.p.room[c]] >= 0)   /* beside the room's number */
            ui_put_c(t_dot, x + 0.8f * q, y + 0.8f * q, 0.7f * q, 0.7f * q);
        else ui_put_c(t_dot, mx, my, (float)q, (float)q);
    }
    for (int r = 0; r < G.p.rooms; r++) {       /* the numbers: red too many, faded just enough */
        int c = corner[r], v = G.p.num[r];
        if (v < 0 || c < 0) continue;
        int st = hyk_room(&G, r), dark = G.st[c] == HYK_SHADE;
        const uint8_t *ink = dark ? EBONY_DIGIT : st == 1 && !solved ? BENCH_RED : PUZ_INK;
        ui_number_ex(bench_mid_x(&geo, c), bench_mid_y(&geo, c) + 0.02f * q, 0.56f * q, v, ink,
                     st == 2 && v > 0 && !solved && !dark ? 110 : 255, UI_NUM_TIGHT);
    }
    bench_marks_back(G.st, real, n * n);
    bench_touch_ring(&touch, &geo);
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * q);
    bench_wave_cells(&win, &geo, 2.3f);
    ui_tool_paint(T_MODE, paint_button, dots ? K_DOT : K_SHADE, 0.72f, solved ? UI_DIM : dots ? UI_ON : 0);
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
    int c = hyk_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, c);
    bench_flash_start(&pop, c);
    ui_sound(SND_PLACE);
    after(c);
}

static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    if (hyk_solved(&G)) return;
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
    if (to == HYK_SHADE) bench_flash_start(&pop, a);
    ui_sound(to == HYK_SHADE ? SND_PLACE : SND_TAP);
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
static void paint_puzzle(const hyk_puzzle *p, const uint8_t *sol, int half, int size, int side, int mid, uint8_t *rgba)
{
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p->n;
    bench_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4), *t = calloc((size_t)g.cs * g.cs, 4);
    if (!b || !t) { free(b); free(t); return; }
    paint_grid(&g, p, b);
    int n = p->n, q = g.cs, seen[HYK_CELLS];
    tile_pic(q, t);
    for (int r = 0; r < p->rooms; r++) seen[r] = 0;
    for (int c = 0; c < n * n; c++) {
        float x = g.ox + c % n * q, y = g.oy + c / n * q;
        int dark = sol[c] && (!half || c < n * n / 2), r = p->room[c];
        if (dark) art_blend(b, g.w, g.h, t, q, q, (int)x, (int)y);
        if (!seen[r] && p->num[r] >= 0 && q >= 9)
            puz_digits(b, g.w, g.h, p->num[r], x + q / 2.0f, y + q * 0.52f, 0.56f * q, dark ? EBONY_DIGIT : PUZ_INK,
                       dark ? 0.6f : 1, 1);
        seen[r] = 1;
    }
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
    free(t);
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    hyk_puzzle p;
    uint8_t sol[HYK_CELLS];
    hyk_make(hyk_size(v), hyk_level(v), 2026u + (uint32_t)v, &p, sol);
    paint_puzzle(&p, sol, variant < 0, size, side, variant < 0, rgba);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, hyk_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= HYK_CHAPTERS ? HYK_CHAPTERS - 1 : chapter;
    static hyk_game T;
    hyk_puzzle p;
    if (!hyk_decode(hyk_levels[(ch + 1) * HYK_LEVELS / HYK_CHAPTERS - 1], &p) || !hyk_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, 0, size, (int)(0.92f * size), 1, rgba);
}

static const game heyawake_game = {
    .id = "heyawake", .order = 427, .variants = HYK_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {HYK_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = HYK_LEVELS, .chapters = HYK_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(heyawake_game)
