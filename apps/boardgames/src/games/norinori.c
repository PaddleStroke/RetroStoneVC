/*
 * Norinori in RetroStone Board, for one player: the house plank ruled in cells, the regions' walls burned in dark
 * ink, ebony tiles laid on the shaded cells (two side by side join into one domino), a dot of ink on those the player
 * keeps unshaded (the rules: norinori_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to shade it (again: take the tile off); hold it a moment to put a dot instead (a note: unshaded). Drag
 * along a row or a column to do the same to a stretch (the cells as the first one was). The keys: the arrows move a
 * frame, Enter shades, Space puts a dot, Tab (or I) gives a hint. The shell's tools: what a tap does (shade, or a dot:
 * tap it to swap tap and hold), and the hint (a wrong cell put right, else the next cell reasoning finds, in gold
 * light). Three shaded cells in a row or a bend, a lone shaded cell walled in by dots, and a region with more than two
 * shaded cells glow red. Undo takes back one action, all the way to the start. The bar: the moves. Solved: a wave of
 * gold light runs over the grid; the stars count the hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/norinori_gen.c: norinori_levels.h), or
 * its die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/norinori.h"
#include "games/norinori_levels.h"

static const uint8_t EBONY[3] = {62, 56, 54};

enum { K_SHADE, K_DOT };                        /* the mode tool: what a tap puts */
enum { T_MODE, T_HINT, TOOLS };

static nrn_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int dots;                                /* taps put dots (the mode tool) */
static int cur = -1;                            /* the keys' frame (a cell), -1: not shown */
static int serial;

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_tile, *t_wide, *t_tall, *t_dot, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* an ebony tile w x h cells of q (a domino: 2 x 1 or 1 x 2) within its picture: dark, with a faint grain, rounded and
 * bevelled */
static void tile_wh(int q, int cw, int ch, uint8_t *o)
{
    int W = cw * q, H = ch * q, in = (int)fmaxf(1, 0.06f * q), w = W - 2 * in, h = H - 2 * in;
    if (w < 2 || h < 2) return;
    uint8_t *b = calloc((size_t)w * h, 4);
    if (!b) return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = b + 4 * ((size_t)y * w + x);
            float k = 0.92f + 0.18f * (1 - (float)y / h) + 0.035f * sinf(y * 0.9f + 0.4f * sinf(x * 0.21f));
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(EBONY[i] * k, 0, 255);
            p[3] = 255;
        }
    puz_bevel(b, w, h, 0.14f * q, fmaxf(2, 0.09f * q), 0.34f);
    if (cw * ch == 2) {                         /* a domino: a faint line across its middle, a pip each side */
        float t = fmaxf(1, 0.03f * q);
        if (cw == 2)
            puz_ink_rect(b, w, h, w / 2.0f - t / 2, 0.22f * q, w / 2.0f + t / 2, h - 0.22f * q, PUZ_CREAM, 0.12f);
        else puz_ink_rect(b, w, h, 0.22f * q, h / 2.0f - t / 2, w - 0.22f * q, h / 2.0f + t / 2, PUZ_CREAM, 0.12f);
    }
    art_blend(o, W, H, b, w, h, in, in);
    free(b);
}

static void tile_pic(int size, uint8_t *o) { tile_wh(size, 1, 1, o); }

static void dot_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.1f, 0.1f);
    art_fig_paint(o, size, f, PUZ_INK, PUZ_INK, 0, 0);
}

/* the plank of a puzzle laid out by g: the grid, the regions' walls in thick ink */
static void paint_grid(const bench_geo *g, const nrn_puzzle *p, uint8_t *o)
{
    bench_paint_grid(g, o);
    int n = p->n, q = g->cs;
    float t = fmaxf(2, 0.09f * q), x0 = g->ox, y0 = g->oy, x1 = x0 + n * q, y1 = y0 + n * q;
    for (int c = 0; c < n * n; c++) {           /* each wall a little longer, to meet the next; never past the rim */
        float x = g->ox + c % n * q, y = g->oy + c / n * q;
        if (c % n < n - 1 && p->region[c] != p->region[c + 1])
            puz_ink_rect(o, g->w, g->h, x + q - t / 2, fmaxf(y0, y - t / 2), x + q + t / 2, fminf(y1, y + q + t / 2),
                         PUZ_INK, 0.9f);
        if (c / n < n - 1 && p->region[c] != p->region[c + n])
            puz_ink_rect(o, g->w, g->h, fmaxf(x0, x - t / 2), y + q - t / 2, fminf(x1, x + q + t / 2), y + q + t / 2,
                         PUZ_INK, 0.9f);
    }
}

static void paint_board(const bench_geo *g, uint8_t *o) { paint_grid(g, &G.p, o); }

/* the token: an ebony domino across a wall on a wooden tile */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2, h = w / 2;
    uint8_t *b = calloc((size_t)w * w, 4), *t = calloc((size_t)w * h, 4);
    if (!b || !t) { free(b); free(t); return; }
    art_board_n(1, 1, w, 0, 0, b);
    puz_ink_rect(b, w, w, h - 0.04f * w, 0, h + 0.04f * w, (float)w, PUZ_INK, 0.9f);
    tile_wh(h, 2, 1, t);
    art_blend(b, w, w, t, w, h, 0, h / 2);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
    free(t);
}

static SDL_Texture *wh_tex(int q, int cw, int ch)
{
    int W = cw * q, H = ch * q;
    memset(ui_buf(W, H), 0, (size_t)W * H * 4);
    tile_wh(q, cw, ch, ui_buf(W, H));
    return ui_tex(W, H, ui_buf(W, H));
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
        t_tile = wh_tex(geo.cs, 1, 1);
        ui_free(&t_wide);
        t_wide = wh_tex(geo.cs, 2, 1);
        ui_free(&t_tall);
        t_tall = wh_tex(geo.cs, 1, 2);
        ui_free(&t_dot);
        t_dot = pic_tex(dot_pic, geo.cs);
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
    dots = 0;
    cur = -1;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    nrn_puzzle p;
    if (!nrn_decode(nrn_levels[level < 0 || level >= NRN_LEVELS ? 0 : level], &p) || !nrn_load(&G, &p))
        nrn_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    nrn_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return nrn_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (nrn_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (nrn_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = nrn_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the marks: a tap shades, a hold puts a dot, a drag as its start -------------------------------------------- */
static int st_of(int c) { return G.st[c]; }
static int free_of(int c) { return nrn_free(&G, c); }
static int tap_of(int c) { return nrn_tap_state(&G, c); }
static int hold_of(int c) { return nrn_hold_state(&G, c); }
static int paint(const int *cells, int n, int state) { return nrn_paint(&G, cells, n, state); }
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
    int n = G.p.n, q = geo.cs, solved = nrn_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    static uint16_t real[NRN_CELLS];
    bench_marks_show(&marks, &touch, &geo, dots, G.st, real, n * n);   /* the grid as the finger would leave it */
    int over[NRN_CELLS];
    for (int r = 0; r < G.p.regions; r++) over[r] = nrn_region(&G, r) == 1;
    for (int c = 0; c < n * n; c++) {
        float x, y, mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        bench_cell_xy(&geo, c, &x, &y);
        if (G.st[c] == NRN_DOT) ui_put_c(t_dot, mx, my, (float)q, (float)q);
        if (G.st[c] != NRN_SHADE) continue;
        int mate = nrn_partner(&G, c);
        if (mate >= 0 && mate < c) continue;    /* drawn with its partner */
        if (mate == c + 1) ui_put(t_wide, x, y, 2.0f * q, (float)q);
        else if (mate == c + n) ui_put(t_tall, x, y, (float)q, 2.0f * q);
        else {
            float s = bench_pop(&pop, c, 0.25f);
            ui_put_c(t_tile, mx, my, q * s, q * s);
        }
    }
    for (int c = 0; c < n * n && !solved; c++)  /* the faults */
        if (G.st[c] == NRN_SHADE && (nrn_crowded(&G, c) || over[G.p.region[c]])) {
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
    int c = nrn_hint(&G);
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
    if (nrn_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { tool(t); return; }
    bench_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { bench_motion(&touch, &geo, x, y, 1); }

static void done_at(int a, int to)
{
    if (to == NRN_SHADE) bench_flash_start(&pop, a);
    ui_sound(to == NRN_SHADE ? SND_PLACE : SND_TAP);
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
    if (nrn_solved(&G) || !n) return;
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
    int mark = (k == SDLK_SPACE) != dots, to = mark ? nrn_hold_state(&G, cur) : nrn_tap_state(&G, cur);
    if (nrn_paint(&G, &cur, 1, to)) done_at(cur, to);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the pictures of puzzles: the menu's and the picker's cards, the chapters' tabs ----------------------------- *
 * A puzzle with the shading of sol (all, or the first half of the cells' when half), side x side, at the top of the
 * card (centred: mid) */
static void paint_puzzle(const nrn_puzzle *p, const uint8_t *sol, int half, int size, int side, int mid, uint8_t *rgba)
{
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p->n;
    bench_fit(&g, (box){0, 0, (float)side, (float)side});
    int q = g.cs, n = p->n;
    uint8_t *b = calloc((size_t)g.w * g.h, 4), *t = calloc((size_t)q * q * 2, 4);
    if (!b || !t) { free(b); free(t); return; }
    paint_grid(&g, p, b);
    for (int c = 0; c < n * n; c++) {
        if (!sol[c] || (half && c >= n * n / 2)) continue;
        int x = c % n, y = c / n, right = x < n - 1 && sol[c + 1], down = y < n - 1 && sol[c + n];
        int left = x > 0 && sol[c - 1], up = y > 0 && sol[c - n];
        if (left || up) continue;               /* drawn from its partner */
        int cw = right ? 2 : 1, ch = down ? 2 : 1;
        if (half && (c + (cw - 1) + (ch - 1) * n) >= n * n / 2) cw = ch = 1;
        memset(t, 0, (size_t)q * q * 2 * 4);
        tile_wh(q, cw, ch, t);
        art_blend(b, g.w, g.h, t, cw * q, ch * q, (int)(g.ox + x * q), (int)(g.oy + y * q));
    }
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
    free(t);
}

/* the level of a size (its k-th, else the nearest): its index */
static int level_of(int n, int k)
{
    int first = -1, last = -1;
    for (int l = 0; l < NRN_LEVELS; l++) {
        int m = atoi(nrn_levels[l]);
        if (m == n && first < 0) first = l;
        if (m == n) last = l;
        if (m > n && first < 0) return l;
    }
    if (first < 0) return NRN_LEVELS - 1;
    return first + k > last ? last : first + k;
}

/* the menu's card (variant -1: a level of the first chapter, half shaded) and the picker's (bare: no stones) */
static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    nrn_puzzle p;
    static nrn_game T;
    int l = variant < 0 ? 12 : level_of(nrn_size(v), 4 + 9 * nrn_level(v));
    if (!nrn_decode(nrn_levels[l], &p) || !nrn_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, variant < 0, size, side, variant < 0 || bare, rgba);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, nrn_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= NRN_CHAPTERS ? NRN_CHAPTERS - 1 : chapter;
    static nrn_game T;
    nrn_puzzle p;
    if (!nrn_decode(nrn_levels[(ch + 1) * NRN_LEVELS / NRN_CHAPTERS - 1], &p) || !nrn_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, 0, size, (int)(0.92f * size), 1, rgba);
}

static const game norinori_game = {
    .id = "norinori", .order = 428, .variants = NRN_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .key = key, .players = 1u << 1,
    .axes = {NRN_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = NRN_LEVELS, .chapters = NRN_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(norinori_game)
