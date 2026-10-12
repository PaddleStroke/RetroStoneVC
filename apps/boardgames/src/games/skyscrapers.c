/*
 * Skyscrapers in RetroStone Board, for one player: the house plank ruled in cells, the numbers burned along a darker
 * band on all four edges, the buildings as little wooden towers seen from above (the taller, the larger and the
 * longer its shadow), their heights burned on top; a row of wooden keys under the plank (the rules:
 * skyscrapers_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to pick it (its row and column lit faintly), then a key: its height goes in (the same key again takes
 * it out); or drag a key onto a cell. Tapping the picked cell again raises its building by one (past the tallest:
 * none); holding a cell a moment clears it. The shell's tools: the pencil (notes: the keys then put small heights in
 * the cell, or take them out; a height put in clears that note from its row and column) and the hint (a wrong height
 * put right, else the next cell reasoning finds, in gold light). A height twice in a line turns red; an edge number
 * turns red when its full line shows another count, and fades when it shows its own. A key fades once its height is
 * in every row. Undo takes back one action, all the way to the start. The bar: the moves. Solved: a wave of gold
 * light runs over the town from the last cell; the stars count the hints and the mistakes (bench.h).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/skyscrapers.h"

#define BAND 0.7f                               /* the edge numbers' bands, in cells */

static const uint8_t CLUE[3] = {44, 30, 20};

enum { T_NOTES, T_HINT, TOOLS };

static sky_game G;
static bench_geo geo;
static bench_touch touch;
static bench_pad pad;
static bench_flash hint, pop;
static bench_wave win;
static int sel = -1, notes;
static int serial;

static box last_box;
static int made_key = -1, made_small = -1, tile_size;
static SDL_Texture *t_board, *t_tile, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a wooden tile cut from the plank, bevelled (a tower's top) */
static void tile_pic(int w, int h, uint8_t *o) { bench_cut(o, w, h, 0.14f * w, fmaxf(2, 0.1f * w), 0.36f); }

/* the side of a building's top for a height (1..n) in a cell of q: the taller, the nearer, the larger */
static float tower(int h, int n, int q) { return q * (0.56f + 0.3f * (h - 1) / (float)(n > 1 ? n - 1 : 1)); }

static int fit(bench_geo *g, int n, box b)
{
    g->cols = g->rows = n;
    g->lm = g->tm = g->rm = g->bm = BAND;
    return bench_fit(g, b);
}

/* the town's numbers (and given heights) into a picture laid out by g: the menu's and the picker's cards */
static void paint_clues(const bench_geo *g, const sky_puzzle *p, const uint8_t *heights, uint8_t *o)
{
    int q = g->cs, n = p->n;
    for (int e = 0; e < 4; e++)
        for (int i = 0; i < n; i++) {
            if (!p->clue[e][i]) continue;
            float x = g->ox + (i + 0.5f) * q, y = g->oy + (i + 0.5f) * q;
            if (e == SKY_TOP) y = g->oy - BAND * q / 2;
            if (e == SKY_BOTTOM) y = g->oy + n * q + BAND * q / 2;
            if (e == SKY_LEFT) x = g->ox - BAND * q / 2;
            if (e == SKY_RIGHT) x = g->ox + n * q + BAND * q / 2;
            puz_digits(o, g->w, g->h, p->clue[e][i], x, y, 0.56f * q, CLUE, 1, 1);
        }
    for (int c = 0; c < n * n; c++) {
        if (!heights[c]) continue;
        int t = (int)tower(heights[c], n, q);
        uint8_t *b = calloc((size_t)t * t, 4);
        if (!b) continue;
        tile_pic(t, t, b);
        float x = g->ox + (c % n + 0.5f) * q, y = g->oy + (c / n + 0.5f) * q;
        art_blend(o, g->w, g->h, b, t, t, (int)(x - t / 2.0f), (int)(y - t / 2.0f));
        free(b);
        puz_digits(o, g->w, g->h, heights[c], x, y, 0.5f * q, p->given[c] ? BENCH_GIVEN : BENCH_BLUE, 1,
                   p->given[c] != 0);
    }
}

/* the plank, the given heights' cells stained */
static void paint_board(const bench_geo *g, uint8_t *o)
{
    bench_paint_grid(g, o);
    for (int c = 0; c < G.p.n * G.p.n; c++)
        if (G.p.given[c]) bench_stain(g, o, c, 0.8f);
}

/* the token: a tall tower and a lower one on a wooden tile, their heights burned in */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    bench_shade(b, w, w, 0, 0, (float)w, (float)w, 0.8f);
    for (int i = 0; i < 2; i++) {
        int t = (int)(w * (i ? 0.38f : 0.5f));
        uint8_t *tl = calloc((size_t)t * t, 4);
        if (!tl) break;
        tile_pic(t, t, tl);
        float cx = w * (i ? 0.7f : 0.34f), cy = w * (i ? 0.7f : 0.36f);
        art_blend(b, w, w, tl, t, t, (int)(cx - t / 2.0f), (int)(cy - t / 2.0f));
        free(tl);
        puz_digits(b, w, w, i ? 2 : 4, cx, cy, 0.7f * t, PUZ_INK, 1, 1);
    }
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    int n = G.p.n ? G.p.n : 4;
    bench_pad_layout(&pad, b, n);
    int changed = fit(&geo, n, pad.board), key = serial % 4096 * 16 + n;
    if (changed || made_key != key) {
        made_key = key;
        ui_free(&t_board);
        t_board = bench_tex(&geo, paint_board);
        tile_size = (int)tower(n, n, geo.cs);
        ui_free(&t_tile);
        memset(ui_buf(tile_size, tile_size), 0, (size_t)tile_size * tile_size * 4);
        tile_pic(tile_size, tile_size, ui_buf(tile_size, tile_size));
        t_tile = ui_tex(tile_size, tile_size, ui_buf(tile_size, tile_size));
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(&touch, 0, sizeof touch);
    memset(&pad.drag, 0, sizeof pad.drag);
    hint.at = pop.at = -1;
    win.on = 0;
}

static void start(int variant)
{
    sky_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    sel = -1;
    notes = 0;
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return sky_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (sky_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
        sel = -1;
    }
    ui_animate();
}

static int undo(void)
{
    int c = sky_undo(&G);
    if (c < 0) return 0;
    calm();
    sel = c;
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = sky_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

static void put(int c, int h)                   /* a height in a cell, settling */
{
    if (!sky_put(&G, c, h)) return;
    if (h) bench_flash_start(&pop, c);
    ui_sound(h ? SND_PLACE : SND_TAP);
    after(c);
}

static void enter(int h)                        /* a key: its height (or note) in the picked cell */
{
    if (!sky_free(&G, sel)) { ui_sound(SND_DENY); return; }
    if (notes) {
        if (!sky_note(&G, sel, h)) { ui_sound(SND_DENY); return; }
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    put(sel, SKY_HEIGHT(G.st[sel]) == h ? 0 : h);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = sky_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    int lit = sel >= 0 ? SKY_HEIGHT(G.st[sel]) : 0;
    if (sel >= 0 && !solved)                    /* the picked cell's row and column */
        for (int c = 0; c < n * n; c++) {
            float x, y;
            bench_cell_xy(&geo, c, &x, &y);
            if (c != sel && (c / n == sel / n || c % n == sel % n)) ui_fill(x, y, (float)q, (float)q, 250, 214, 90, 40);
            if (lit && c != sel && SKY_HEIGHT(G.st[c]) == lit) ui_mark_last(x, y, (float)q, (float)q);
        }
    if (sel >= 0 && !solved) {
        float x, y;
        bench_cell_xy(&geo, sel, &x, &y);
        ui_mark_select(x, y, (float)q, (float)q);
    }
    for (int e = 0; e < 4; e++)                 /* the edge numbers */
        for (int i = 0; i < n; i++) {
            int v = G.p.clue[e][i], st = sky_edge_state(&G, e, i);
            if (!v) continue;
            float x = geo.gx + (i + 0.5f) * q, y = geo.gy + (i + 0.5f) * q;
            if (e == SKY_TOP) y = geo.gy - BAND * q / 2;
            if (e == SKY_BOTTOM) y = geo.gy + n * q + BAND * q / 2;
            if (e == SKY_LEFT) x = geo.gx - BAND * q / 2;
            if (e == SKY_RIGHT) x = geo.gx + n * q + BAND * q / 2;
            ui_number_ex(x, y, 0.56f * q, v, st < 0 ? BENCH_RED : CLUE, st > 0 ? 100 : 255, UI_NUM_TIGHT);
        }
    for (int c = 0; c < n * n; c++) {           /* the towers, their shadows first */
        int h = SKY_HEIGHT(G.st[c]);
        if (!h) continue;
        float t = tower(h, n, q) * bench_pop(&pop, c, 0.35f), off = 0.05f * q * h / n;
        ui_shadow_flat(bench_mid_x(&geo, c) + off, bench_mid_y(&geo, c) + off, t * 1.05f, t * 1.05f,
                       0.4f + 0.4f * h / n);
    }
    for (int c = 0; c < n * n; c++) {
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        int h = SKY_HEIGHT(G.st[c]);
        if (!h) {
            for (int k = 1; k <= n; k++) {      /* the notes */
                if (!SKY_NOTE(G.st[c], k)) continue;
                float nx = mx + ((k - 1) % 3 - 1) * 0.3f * q, ny = my + ((k - 1) / 3 - 1) * 0.29f * q;
                ui_number_ex(nx, ny, 0.26f * q, k, k == lit ? BENCH_BLUE : BENCH_NOTE, 255, UI_NUM_TIGHT);
            }
            continue;
        }
        float k = bench_pop(&pop, c, 0.35f), t = tower(h, n, q) * k;
        Uint8 tone = (Uint8)(255 - 70 * (h - 1) / (n > 1 ? n - 1 : 1));   /* the taller, the darker its wood */
        if (G.p.given[c]) tone = (Uint8)(tone * 0.72f);
        SDL_SetTextureColorMod(t_tile, tone, tone, tone);
        ui_put_c(t_tile, mx, my, t, t);
        SDL_SetTextureColorMod(t_tile, 255, 255, 255);
        const uint8_t *ink = G.p.given[c] ? BENCH_GIVEN : !solved && sky_clash(&G, c) ? BENCH_RED : BENCH_BLUE;
        ui_number_ex(mx, my + 0.02f * q, 0.5f * q * k, h, ink, 255, UI_NUM_TIGHT);
    }
    unsigned faded = 0;
    for (int h = 1; h <= n; h++)
        if (sky_count_height(&G, h) >= n) faded |= 1u << h;
    bench_keys(&pad, solved ? 0 : lit, faded, notes);
    float kx, ky;
    int key = bench_carried(&pad, &kx, &ky);
    if (key) {                                  /* a key carried, above the finger */
        int c = bench_cell_at(&geo, kx, ky);
        if (sky_free(&G, c)) {
            float x, y;
            bench_cell_xy(&geo, c, &x, &y);
            ui_mark_select(x, y, (float)q, (float)q);
        }
        bench_key(&pad, key, kx, ky, 1.1f, 255, notes);
    }
    bench_touch_ring(&touch, &geo);
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * q);
    bench_wave_cells(&win, &geo, 2.3f);
    ui_tool(T_NOTES, ICON_PENCIL, solved ? UI_DIM : notes ? UI_ON : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void tool(int t)
{
    if (t == T_NOTES) {
        notes = !notes;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    int c = sky_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    sel = c;
    bench_flash_start(&hint, c);
    bench_flash_start(&pop, c);
    ui_sound(SND_PLACE);
    after(c);
}

static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    memset(&pad.drag, 0, sizeof pad.drag);
    if (sky_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { tool(t); return; }
    if (bench_pad_press(&pad, x, y)) return;    /* a key: on release (a tap), or carried to a cell */
    if (bench_press(&touch, &geo, x, y) < 0 && sel >= 0) {
        sel = -1;                               /* elsewhere: nothing picked */
        ui_animate();
    }
}

static void motion(float x, float y)
{
    if (bench_pad_motion(&pad, x, y)) return;
    bench_motion(&touch, &geo, x, y, 0);
}

static void release(float x, float y)
{
    if (pad.drag.on) {
        int tapped;
        float dx, dy;
        int k = bench_pad_release(&pad, x, y, &tapped, &dx, &dy);
        if (!k) return;
        if (tapped) { enter(k); return; }
        int c = bench_cell_at(&geo, dx, dy);    /* dropped: on the cell under the key */
        if (c < 0) c = bench_cell_at(&geo, x, y);
        if (!sky_free(&G, c)) { if (c >= 0) ui_sound(SND_DENY); return; }
        sel = c;
        enter(k);
        return;
    }
    int c = touch.a, kind = bench_release(&touch);
    if (kind == BENCH_NOTHING) return;
    if (kind == BENCH_HOLD) {                   /* held: the cell cleared */
        sel = c;
        if (!sky_clear(&G, c)) ui_sound(sky_free(&G, c) ? SND_TAP : SND_DENY);
        else ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (c == sel && sky_free(&G, c) && !notes) {   /* the picked cell again: one storey more */
        put(c, (SKY_HEIGHT(G.st[c]) + 1) % (G.p.n + 1));
        return;
    }
    if (sel != c) ui_sound(SND_TAP);
    sel = c;
    ui_animate();
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the menu's and the picker's cards: a town of the size, its numbers (the menu's: half built); under the
 * picker's, its level in ebony stones ---------------------------------------------------------------------------- */
static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 4 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    sky_puzzle p;
    uint8_t sol[SKY_CELLS], heights[SKY_CELLS];
    sky_make(sky_size(v), sky_level(v), 2026u + (uint32_t)v, &p, sol);
    int n = p.n;
    for (int c = 0; c < n * n; c++) heights[c] = p.given[c] ? p.given[c] : variant < 0 && c < n * n / 2 ? sol[c] : 0;
    bench_geo g;
    memset(&g, 0, sizeof g);
    fit(&g, n, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4);
    if (!b) return;
    bench_paint_grid(&g, b);
    paint_clues(&g, &p, heights, b);
    int ox = (size - side) / 2 + (int)g.bx, oy = (variant < 0 ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, sky_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

static const game skyscrapers_game = {
    .id = "skyscrapers", .order = 413, .variants = SKY_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {SKY_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(skyscrapers_game)
