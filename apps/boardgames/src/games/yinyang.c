/*
 * Yin-Yang in RetroStone Board, for one player: the house plank ruled in cells, glossy ivory and ebony stones (the
 * house's go stones), the given ones on cells stained darker (the rules: yinyang_rules.c; what the logic grids share:
 * bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to put an ebony stone, again for ivory, again to empty it; hold it a moment to put an ivory stone (or
 * take it off). Drag along a row or a column to do the same to a stretch (the cells as the first one was). The keys:
 * the arrows move a frame, Enter as a tap, Space as a hold, Tab (or I) gives a hint. The shell's tools: which stone a
 * tap puts first (tap it to swap tap and hold), and the hint (a wrong stone put right, else the next stone reasoning
 * finds, in gold light). Four stones of a colour in a square glow red; a group shut in by the other colour while
 * more of its colour lie elsewhere takes a red tint. Undo takes back one action, all the way to the start. The bar:
 * the moves. Solved: a wave of gold light runs over the grid; the stars count the hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/yinyang_gen.c: yinyang_levels.h), or
 * its die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/yinyang.h"
#include "games/yinyang_levels.h"

#define STONE 0.84f                             /* a stone across, in cells */

enum { T_MODE, T_HINT, TOOLS };

static yyg_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int ivory_first;                         /* taps put ivory (the mode tool) */
static int cur = -1;                            /* the keys' frame (a cell), -1: not shown */
static int serial;

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_stone[2], *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a stone (YYG_IVORY, YYG_EBONY) as the house draws a go stone: player 0 ivory, 1 ebony */
static void stone_pic(int col, int size, uint8_t *o) { art_counter(COUNTER_STONE, col == YYG_EBONY, size, o); }

/* the plank of a puzzle laid out by g: the grid, the given stones' cells stained */
static void paint_grid(const bench_geo *g, const yyg_puzzle *p, uint8_t *o)
{
    bench_paint_grid(g, o);
    for (int c = 0; c < p->n * p->n; c++)
        if (p->given[c]) bench_stain(g, o, c, BENCH_GIVEN_SHADE);
}

static void paint_board(const bench_geo *g, uint8_t *o) { paint_grid(g, &G.p, o); }

/* the stones of st (all, or those of the cells below upto) into a picture laid out by g */
static void paint_stones(const bench_geo *g, int n, const uint8_t *st, int upto, uint8_t *o)
{
    int q = g->cs, d = (int)(STONE * q);
    if (d < 2) return;
    uint8_t *b[2] = {calloc((size_t)d * d, 4), calloc((size_t)d * d, 4)};
    if (b[0] && b[1]) {
        stone_pic(YYG_IVORY, d, b[0]);
        stone_pic(YYG_EBONY, d, b[1]);
        for (int c = 0; c < n * n && c < upto; c++)
            if (st[c] == YYG_IVORY || st[c] == YYG_EBONY)
                art_flat_put(o, g->w, g->h, b[st[c] == YYG_EBONY], d, g->ox + (c % n + 0.5f) * q,
                             g->oy + (c / n + 0.5f) * q);
    }
    free(b[0]);
    free(b[1]);
}

/* the token: an ivory and an ebony stone side by side on a wooden tile */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.w = g.h = w;
    g.cs = w / 2;
    g.ox = g.oy = (w - 2 * g.cs) / 2.0f;
    static const uint8_t ST[4] = {YYG_EBONY, YYG_IVORY, YYG_IVORY, YYG_EBONY};
    paint_stones(&g, 2, ST, 4, b);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static SDL_Texture *stone_tex(int col, int q)
{
    memset(ui_buf(q, q), 0, (size_t)q * q * 4);
    stone_pic(col, q, ui_buf(q, q));
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
        for (int i = 0; i < 2; i++) {
            ui_free(&t_stone[i]);
            t_stone[i] = stone_tex(i ? YYG_EBONY : YYG_IVORY, (int)(STONE * geo.cs));
        }
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
    ivory_first = 0;
    cur = -1;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    yyg_puzzle p;
    if (!yyg_decode(yyg_levels[level < 0 || level >= YYG_LEVELS ? 0 : level], &p) || !yyg_load(&G, &p))
        yyg_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    yyg_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return yyg_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (yyg_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (yyg_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = yyg_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the marks: a tap ebony then ivory, a hold ivory, a drag as its start ---------------------------------------- */
static int st_of(int c) { return G.st[c]; }
static int free_of(int c) { return yyg_free(&G, c); }
static int tap_of(int c) { return yyg_tap_state(&G, c); }
static int hold_of(int c) { return yyg_hold_state(&G, c); }
static int paint(const int *cells, int n, int state) { return yyg_paint(&G, cells, n, state); }
static const bench_marks marks = {st_of, free_of, tap_of, hold_of, paint, NULL, 0};

/* ---- the board on the screen, the tools -------------------------------------------------------------------------- */
static void paint_button(int kind, int size, uint8_t *o)   /* which stone a tap puts first */
{
    int d = (int)(0.8f * size);
    uint8_t *b = calloc((size_t)d * d, 4);
    if (!b) return;
    stone_pic(kind, d, b);
    art_blend(o, size, size, b, d, d, (size - d) / 2, (size - d) / 2);
    free(b);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = yyg_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    static uint16_t real[YYG_CELLS];
    bench_marks_show(&marks, &touch, &geo, ivory_first, G.st, real, n * n);   /* as the finger would leave it */
    for (int c = 0; c < n * n; c++) {
        if (G.st[c] != YYG_IVORY && G.st[c] != YYG_EBONY) continue;
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c), s = bench_pop(&pop, c, 0.25f) * STONE * q;
        ui_shadow_flat(mx + 0.05f * q, my + 0.06f * q, s, s, 0.5f);
        ui_put_c(t_stone[G.st[c] == YYG_EBONY], mx, my, s, s);
    }
    for (int c = 0; c < n * n && !solved; c++) {   /* the faults */
        float x, y;
        bench_cell_xy(&geo, c, &x, &y);
        if (yyg_square(&G, c)) ui_mark_glow(bench_mid_x(&geo, c), bench_mid_y(&geo, c), 1.15f * q);
        else if (yyg_apart(&G, c)) ui_fill(x, y, (float)q, (float)q, 214, 60, 44, 70);
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
    ui_tool_paint(T_MODE, paint_button, ivory_first ? YYG_IVORY : YYG_EBONY, 0.72f, solved ? UI_DIM : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer, the keys --------------------------------------------------------------------------------------- */
static void give_hint(void)
{
    int c = yyg_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, c);
    bench_flash_start(&pop, c);
    ui_sound(SND_PLACE);
    after(c);
}

static void tool(int t)
{
    if (t == T_MODE) {
        ivory_first = !ivory_first;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    give_hint();
}

static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    cur = -1;
    if (yyg_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { tool(t); return; }
    bench_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { bench_motion(&touch, &geo, x, y, 1); }

static void done_at(int a, int to)
{
    if (to != YYG_EMPTY) bench_flash_start(&pop, a);
    ui_sound(to != YYG_EMPTY ? SND_PLACE : SND_TAP);
    after(a);
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int to, a = bench_marks_release(&marks, &touch, &geo, ivory_first, &to);
    if (a >= 0) done_at(a, to);
}

static void key(SDL_Keycode k)
{
    int n = G.p.n;
    if (yyg_solved(&G) || !n) return;
    if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN) {
        if (cur < 0) cur = n * n / 2;
        else {
            int x = cur % n + (k == SDLK_RIGHT) - (k == SDLK_LEFT), y = cur / n + (k == SDLK_DOWN) - (k == SDLK_UP);
            cur = (y < 0 ? 0 : y >= n ? n - 1 : y) * n + (x < 0 ? 0 : x >= n ? n - 1 : x);
        }
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (k == SDLK_TAB || k == SDLK_i) { give_hint(); return; }
    if (k != SDLK_RETURN && k != SDLK_KP_ENTER && k != SDLK_SPACE) return;
    if (cur < 0) { cur = n * n / 2; ui_animate(); return; }
    if (!yyg_free(&G, cur)) { ui_sound(SND_DENY); return; }
    int mark = (k == SDLK_SPACE) != ivory_first, to = mark ? yyg_hold_state(&G, cur) : yyg_tap_state(&G, cur);
    if (yyg_paint(&G, &cur, 1, to)) done_at(cur, to);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the pictures of puzzles: the menu's and the picker's cards, the chapters' tabs ----------------------------- *
 * A puzzle with the given stones and those of sol (all, or the cells of its first half when half), side x side, at
 * the top of the card (centred: mid) */
static void paint_puzzle(const yyg_puzzle *p, const uint8_t *sol, int half, int size, int side, int mid, uint8_t *rgba)
{
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p->n;
    bench_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4);
    if (!b) return;
    paint_grid(&g, p, b);
    uint8_t st[YYG_CELLS];
    for (int c = 0; c < p->n * p->n; c++)
        st[c] = p->given[c] ? p->given[c] : half && c >= p->n * p->n / 2 ? YYG_EMPTY : sol[c];
    paint_stones(&g, p->n, st, p->n * p->n, b);
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
}

/* the level of a size (its k-th, else the nearest): its index */
static int level_of(int n, int k)
{
    int first = -1, last = -1;
    for (int l = 0; l < YYG_LEVELS; l++) {
        int m = atoi(yyg_levels[l]);
        if (m == n && first < 0) first = l;
        if (m == n) last = l;
        if (m > n && first < 0) return l;
    }
    if (first < 0) return YYG_LEVELS - 1;
    return first + k > last ? last : first + k;
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    yyg_puzzle p;
    static yyg_game T;
    int l = variant < 0 ? 12 : level_of(yyg_size(v), 4 + 9 * yyg_level(v));
    if (!yyg_decode(yyg_levels[l], &p) || !yyg_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, variant < 0, size, side, variant < 0 || bare, rgba);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, yyg_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= YYG_CHAPTERS ? YYG_CHAPTERS - 1 : chapter;
    static yyg_game T;
    yyg_puzzle p;
    if (!yyg_decode(yyg_levels[(ch + 1) * YYG_LEVELS / YYG_CHAPTERS - 1], &p) || !yyg_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, 0, size, (int)(0.92f * size), 1, rgba);
}

static const game yinyang_game = {
    .id = "yinyang", .order = 435, .variants = YYG_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .key = key, .players = 1u << 1,
    .axes = {YYG_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = YYG_LEVELS, .chapters = YYG_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(yinyang_game)
