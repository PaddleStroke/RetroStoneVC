/*
 * Tapa in RetroStone Board, for one player: the house plank ruled in cells, the clue cells stained darker with their
 * numbers burned in (one to four, set about the cell), ebony tiles laid on the shaded cells and joined where they
 * touch into one wall, a dot of ink on the cells the player keeps unshaded (the rules: tapa_rules.c; what the logic
 * grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to shade it (again: take the tile off); hold it a moment to put a dot instead (a note: unshaded). Drag
 * along a row or a column to do the same to a stretch (the cells as the first one was). The keys: the arrows move a
 * frame, Enter shades, Space puts a dot, Tab (or I) gives a hint. The shell's tools: what a tap does (shade, or a dot:
 * tap it to swap tap and hold), and the hint (a wrong cell put right, else the next cell reasoning finds, in gold
 * light). A clue whose ring can no longer give its runs turns red; one whose runs are all there, with each of its
 * neighbours decided (shaded or a dot), fades. An all shaded 2 x 2 glows red. Undo takes back one action, all the way
 * to the start. The bar: the moves. Solved: a wave of gold light runs over the grid; the stars count the hints and
 * the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/tapa_gen.c: tapa_levels.h), or its die
 * tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/tapa.h"
#include "games/tapa_levels.h"

static const uint8_t EBONY[3] = {62, 56, 54};

enum { K_SHADE, K_DOT };                        /* the mode tool: what a tap puts */
enum { T_MODE, T_HINT, TOOLS };

static tpa_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int dots;                                /* taps put dots (the mode tool) */
static int cur = -1;                            /* the keys' frame (a cell), -1: not shown */
static int serial;

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_tile, *t_dot, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* an ebony tile within a cell (a picture size x size): dark, with a faint grain, rounded and bevelled */
static void tile_pic(int size, uint8_t *o)
{
    int in = (int)fmaxf(1, 0.06f * size), w = size - 2 * in;
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
    puz_bevel(b, w, w, 0.14f * w, fmaxf(2, 0.09f * w), 0.34f);
    art_blend(o, size, size, b, w, w, in, in);
    free(b);
}

static void dot_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.1f, 0.1f);
    art_fig_paint(o, size, f, PUZ_INK, PUZ_INK, 0, 0);
}

/* where a clue's numbers sit in its cell (parts of a cell: x, y) and their height, by how many */
static void clue_spot(int count, int i, float *x, float *y, float *h)
{
    static const float AT[4][4][2] = {
        {{0.5f, 0.52f}},
        {{0.32f, 0.34f}, {0.68f, 0.7f}},
        {{0.3f, 0.32f}, {0.7f, 0.32f}, {0.5f, 0.72f}},
        {{0.3f, 0.3f}, {0.7f, 0.3f}, {0.3f, 0.72f}, {0.7f, 0.72f}},
    };
    static const float H[4] = {0.62f, 0.44f, 0.38f, 0.36f};
    int k = count < 1 ? 1 : count > 4 ? 4 : count;
    *x = AT[k - 1][i][0];
    *y = AT[k - 1][i][1];
    *h = H[k - 1];
}

/* the plank of a puzzle laid out by g: the grid, the clue cells stained (their numbers come on top, or here when
 * burn: the pictures) */
static void paint_grid(const bench_geo *g, const tpa_puzzle *p, int burn, uint8_t *o)
{
    bench_paint_grid(g, o);
    int n = p->n, q = g->cs;
    for (int c = 0; c < n * n; c++) {
        if (p->count[c] < 0) continue;
        bench_stain(g, o, c, BENCH_GIVEN_SHADE);
        if (!burn || q < 9) continue;
        for (int i = 0; i < p->count[c]; i++) {
            float x, y, h;
            clue_spot(p->count[c], i, &x, &y, &h);
            puz_digits(o, g->w, g->h, p->num[c][i], g->ox + (c % n + x) * q, g->oy + (c / n + y) * q, h * q,
                       PUZ_INK, 1, 1);
        }
    }
}

static void paint_board(const bench_geo *g, uint8_t *o) { paint_grid(g, &G.p, 0, o); }

/* the wall of a shading sh (1: shaded) into a picture laid out by g: ebony tiles joined where they touch; only the
 * cells below `upto` */
static void paint_wall(const bench_geo *g, int n, const uint8_t *sh, int upto, uint8_t *o)
{
    int q = g->cs;
    uint8_t *t = calloc((size_t)q * q, 4);
    if (!t) return;
    tile_pic(q, t);
    for (int c = 0; c < n * n && c < upto; c++) {
        if (!sh[c]) continue;
        float x = g->ox + c % n * q, y = g->oy + c / n * q, in = fmaxf(1, 0.06f * q) + 0.12f * q;
        art_blend(o, g->w, g->h, t, q, q, (int)x, (int)y);
        if (c % n < n - 1 && sh[c + 1] && c + 1 < upto)
            puz_ink_rect(o, g->w, g->h, x + q - in, y + 0.3f * q, x + q + in, y + 0.7f * q, EBONY, 1);
        if (c / n < n - 1 && sh[c + n] && c + n < upto)
            puz_ink_rect(o, g->w, g->h, x + 0.3f * q, y + q - in, x + 0.7f * q, y + q + in, EBONY, 1);
    }
    free(t);
}

/* the token: a clue 3 1 beside a bend of wall, on a wooden tile */
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
    g.cs = w / 3;
    g.ox = g.oy = (w - 3 * g.cs) / 2.0f;
    static const uint8_t SH[9] = {1, 1, 1, 0, 0, 1, 1, 0, 0};
    paint_wall(&g, 3, SH, 9, b);
    float q = (float)g.cs;
    puz_digits(b, w, w, 3, g.ox + 1.32f * q, g.oy + 1.34f * q, 0.44f * q, PUZ_INK, 1, 1);
    puz_digits(b, w, w, 1, g.ox + 1.68f * q, g.oy + 1.7f * q, 0.44f * q, PUZ_INK, 1, 1);
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
    cur = -1;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    tpa_puzzle p;
    if (!tpa_decode(tpa_levels[level < 0 || level >= TPA_LEVELS ? 0 : level], &p) || !tpa_load(&G, &p))
        tpa_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    tpa_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return tpa_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (tpa_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (tpa_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = tpa_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the marks: a tap shades, a hold puts a dot, a drag as its start -------------------------------------------- */
static int st_of(int c) { return G.st[c]; }
static int free_of(int c) { return tpa_free(&G, c); }
static int tap_of(int c) { return tpa_tap_state(&G, c); }
static int hold_of(int c) { return tpa_hold_state(&G, c); }
static int paint(const int *cells, int n, int state) { return tpa_paint(&G, cells, n, state); }
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
    int n = G.p.n, q = geo.cs, solved = tpa_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    static uint16_t real[TPA_CELLS];
    bench_marks_show(&marks, &touch, &geo, dots, G.st, real, n * n);   /* the grid as the finger would leave it */
    for (int c = 0; c < n * n; c++) {           /* the clues: red broken, faded kept */
        if (G.p.count[c] < 0) continue;
        int st = tpa_clue(&G, c);
        float x0, y0;
        bench_cell_xy(&geo, c, &x0, &y0);
        for (int i = 0; i < G.p.count[c]; i++) {
            float x, y, h;
            clue_spot(G.p.count[c], i, &x, &y, &h);
            ui_number_ex(x0 + x * q, y0 + y * q, h * q, G.p.num[c][i], st == 1 && !solved ? BENCH_RED : PUZ_INK,
                         st == 2 && !solved ? 110 : 255, UI_NUM_TIGHT);
        }
    }
    for (int c = 0; c < n * n; c++) {
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        if (G.st[c] == TPA_DOT) ui_put_c(t_dot, mx, my, (float)q, (float)q);
        if (G.st[c] != TPA_SHADE) continue;
        float s = bench_pop(&pop, c, 0.25f);
        ui_put_c(t_tile, mx, my, q * s, q * s);
    }
    for (int c = 0; c < n * n; c++) {           /* the wall joined where its tiles touch */
        if (G.st[c] != TPA_SHADE) continue;
        float x, y, in = fmaxf(1, 0.06f * q) + 0.12f * q;
        bench_cell_xy(&geo, c, &x, &y);
        if (c % n < n - 1 && G.st[c + 1] == TPA_SHADE)
            ui_fill(x + q - in, y + 0.3f * q, 2 * in, 0.4f * q, EBONY[0], EBONY[1], EBONY[2], 255);
        if (c / n < n - 1 && G.st[c + n] == TPA_SHADE)
            ui_fill(x + 0.3f * q, y + q - in, 0.4f * q, 2 * in, EBONY[0], EBONY[1], EBONY[2], 255);
    }
    for (int c = 0; c < n * n && !solved; c++)  /* the faults */
        if (tpa_square(&G, c)) {
            float x, y;
            bench_cell_xy(&geo, c, &x, &y);
            ui_fill(x, y, (float)q, (float)q, 214, 60, 44, 110);
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
    ui_tool_paint(T_MODE, paint_button, dots ? K_DOT : K_SHADE, 0.72f, solved ? UI_DIM : dots ? UI_ON : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer, the keys --------------------------------------------------------------------------------------- */
static void give_hint(void)
{
    int c = tpa_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, c);
    bench_flash_start(&pop, c);
    ui_sound(SND_PLACE);
    after(c);
}

static void tool(int t)
{
    if (t == T_MODE) {
        dots = !dots;
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
    if (tpa_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { tool(t); return; }
    bench_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { bench_motion(&touch, &geo, x, y, 1); }

static void done_at(int a, int to)
{
    if (to == TPA_SHADE) bench_flash_start(&pop, a);
    ui_sound(to == TPA_SHADE ? SND_PLACE : SND_TAP);
    after(a);
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int to, a = bench_marks_release(&marks, &touch, &geo, dots, &to);
    if (a >= 0) done_at(a, to);
}

static void key(SDL_Keycode k)
{
    int n = G.p.n;
    if (tpa_solved(&G) || !n) return;
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
    if (!tpa_free(&G, cur)) { ui_sound(SND_DENY); return; }
    int mark = (k == SDLK_SPACE) != dots, to = mark ? tpa_hold_state(&G, cur) : tpa_tap_state(&G, cur);
    if (tpa_paint(&G, &cur, 1, to)) done_at(cur, to);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the pictures of puzzles: the menu's and the picker's cards, the chapters' tabs ----------------------------- *
 * A puzzle with the wall of sol (all, or the cells of its first half when half), side x side, at the top of the card
 * (centred: mid) */
static void paint_puzzle(const tpa_puzzle *p, const uint8_t *sol, int half, int size, int side, int mid, uint8_t *rgba)
{
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p->n;
    bench_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4);
    if (!b) return;
    paint_grid(&g, p, 1, b);
    paint_wall(&g, p->n, sol, half ? p->n * p->n / 2 : p->n * p->n, b);
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
}

/* the level of a size (its k-th, else the nearest): its index */
static int level_of(int n, int k)
{
    int first = -1, last = -1;
    for (int l = 0; l < TPA_LEVELS; l++) {
        int m = atoi(tpa_levels[l]);
        if (m == n && first < 0) first = l;
        if (m == n) last = l;
        if (m > n && first < 0) return l;
    }
    if (first < 0) return TPA_LEVELS - 1;
    return first + k > last ? last : first + k;
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    tpa_puzzle p;
    static tpa_game T;
    int l = variant < 0 ? 12 : level_of(tpa_size(v), 4 + 9 * tpa_level(v));
    if (!tpa_decode(tpa_levels[l], &p) || !tpa_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, variant < 0, size, side, variant < 0 || bare, rgba);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, tpa_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= TPA_CHAPTERS ? TPA_CHAPTERS - 1 : chapter;
    static tpa_game T;
    tpa_puzzle p;
    if (!tpa_decode(tpa_levels[(ch + 1) * TPA_LEVELS / TPA_CHAPTERS - 1], &p) || !tpa_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, 0, size, (int)(0.92f * size), 1, rgba);
}

static const game tapa_game = {
    .id = "tapa", .order = 434, .variants = TPA_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .key = key, .players = 1u << 1,
    .axes = {TPA_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = TPA_LEVELS, .chapters = TPA_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(tapa_game)
