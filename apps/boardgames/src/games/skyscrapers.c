/*
 * Skyscrapers in RetroStone Board, for one player: the house plank ruled in cells, the numbers burned along a darker
 * band on all four edges, the buildings as little wooden towers seen from above (the taller, the larger and the
 * longer its shadow), their heights burned on top; a row of wooden keys under the plank (the rules:
 * skyscrapers_rules.c; what the logic grids share: logicgrid.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to pick it (its row and column lit faintly), then a key: its height goes in (the same key again takes
 * it out); or drag a key onto a cell. Tapping the picked cell again raises its building by one (past the tallest:
 * none); holding a cell a moment clears it. The buttons at the right end of the bar: the pencil (notes: the keys then
 * put small heights in the cell, or take them out; a height put in clears that note from its row and column) and the
 * hint (a wrong height put right, else the next cell reasoning finds, in gold light). A height twice in a line turns
 * red; an edge number turns red when its full line shows another count, and fades when it shows its own. A key fades
 * once its height is in every row. Undo takes back one action, all the way to the start. The bar: the moves.
 * Solved: a wave of gold light runs over the town from the last cell.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/logicgrid.h"
#include "games/puzzles.h"
#include "games/skyscrapers.h"

#define BAND 0.7f                               /* the edge numbers' bands, in cells */

static const uint8_t ENTRY[3] = {30, 64, 140}, NOTE[3] = {92, 70, 50}, CLUE[3] = {44, 30, 20};

static sky_game G;
static gl_geo geo;
static gl_touch touch;
static gl_light hint, win;
static int sel = -1, notes;
static int serial;
static struct { int cell; Uint32 t0; } pop = {-1, 0};
static struct { int on, key, moved; float x, y, x0, y0; } carry;

static box last_box, barbox;
static float kx0, ky, kp, kw, kh;               /* the keys: the first's centre, their row, step and size */
static int made_key = -1, made_small = -1, tile_size;
static SDL_Texture *t_board, *t_tile, *t_key, *t_small;
static puz_font f_num, f_note;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a wooden tile cut from the plank, bevelled (a tower's top, a key) */
static void tile_pic(int w, int h, uint8_t *o)
{
    int q = w > h ? w : h;
    uint8_t *plank = malloc((size_t)q * q * 4);
    if (!plank) return;
    art_board_n(1, 1, q, 0, 0, plank);
    for (int y = 0; y < h; y++) memcpy(o + 4 * (size_t)y * w, plank + 4 * (size_t)y * q, (size_t)w * 4);
    free(plank);
    puz_bevel(o, w, h, 0.14f * w, fmaxf(2, 0.1f * w), 0.36f);
}

/* the side of a building's top for a height (1..n) in a cell of q: the taller, the nearer, the larger */
static float tower(int h, int n, int q) { return q * (0.56f + 0.3f * (h - 1) / (float)(n > 1 ? n - 1 : 1)); }

static void fit(gl_geo *g, int n, box b, float *keys_y)
{
    float S = floorf(fminf(b.w, b.h)), u = ui.unit, khh = floorf(0.98f * u), P = floorf(S - khh - 0.24f * u);
    g->cols = g->rows = n;
    g->lm = g->tm = g->rm = g->bm = BAND;
    gl_fit(g, (box){floorf(b.x + (b.w - P) / 2), floorf(b.y + (b.h - S) / 2), P, P});
    if (keys_y) *keys_y = floorf(b.y + (b.h - S) / 2) + S - khh / 2;
}

/* the town's numbers (and given heights) into a picture laid out by g: the menu's and the picker's cards */
static void paint_clues(const gl_geo *g, const sky_puzzle *p, const uint8_t *heights, uint8_t *o)
{
    int S = (int)g->side, q = g->cs, n = p->n;
    for (int e = 0; e < 4; e++)
        for (int i = 0; i < n; i++) {
            if (!p->clue[e][i]) continue;
            float x = g->ox + (i + 0.5f) * q, y = g->oy + (i + 0.5f) * q;
            if (e == SKY_TOP) y = g->oy - BAND * q / 2;
            if (e == SKY_BOTTOM) y = g->oy + n * q + BAND * q / 2;
            if (e == SKY_LEFT) x = g->ox - BAND * q / 2;
            if (e == SKY_RIGHT) x = g->ox + n * q + BAND * q / 2;
            puz_digits(o, S, S, p->clue[e][i], x, y, 0.56f * q, CLUE, 1, 1);
        }
    for (int c = 0; c < n * n; c++) {
        if (!heights[c]) continue;
        int t = (int)tower(heights[c], n, q);
        uint8_t *b = calloc((size_t)t * t, 4);
        if (!b) continue;
        tile_pic(t, t, b);
        float x = g->ox + (c % n + 0.5f) * q, y = g->oy + (c / n + 0.5f) * q;
        art_blend(o, S, S, b, t, t, (int)(x - t / 2.0f), (int)(y - t / 2.0f));
        free(b);
        puz_digits(o, S, S, heights[c], x, y, 0.5f * q, p->given[c] ? GL_DARK : ENTRY, 1, p->given[c] != 0);
    }
}

/* the token: a tall tower and a lower one on a wooden tile, their heights burned in */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    gl_shade(b, w, w, 0, 0, (float)w, (float)w, 0.8f);
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
    int was = geo.cs, n = G.p.n;
    fit(&geo, n, b, &ky);
    float u = ui.unit;
    kh = floorf(0.98f * u);
    kp = geo.side / n;
    kw = floorf(fminf(0.92f * u, 0.86f * kp));
    kx0 = geo.bx + kp / 2;
    int key = ((int)geo.side * 64 + serial % 64) * 16 + n;
    if (was != geo.cs || made_key != key) {
        made_key = key;
        int S = (int)geo.side, q = geo.cs;
        ui_free(&t_board);
        gl_paint(&geo, ui_buf(S, S));
        int in = (int)fmaxf(1, 0.04f * q);
        for (int c = 0; c < n * n; c++)         /* the given heights' cells stained */
            if (G.p.given[c])
                gl_shade(ui_buf(S, S), S, S, geo.ox + c % n * q + in, geo.oy + c / n * q + in,
                         geo.ox + (c % n + 1) * q - in, geo.oy + (c / n + 1) * q - in, 0.8f);
        t_board = ui_tex(S, S, ui_buf(S, S));
        tile_size = (int)tower(n, n, q);
        ui_free(&t_tile);
        memset(ui_buf(tile_size, tile_size), 0, (size_t)tile_size * tile_size * 4);
        tile_pic(tile_size, tile_size, ui_buf(tile_size, tile_size));
        t_tile = ui_tex(tile_size, tile_size, ui_buf(tile_size, tile_size));
        int iw = (int)kw, ih = (int)kh;
        ui_free(&t_key);
        memset(ui_buf(iw, ih), 0, (size_t)iw * ih * 4);
        tile_pic(iw, ih, ui_buf(iw, ih));
        t_key = ui_tex(iw, ih, ui_buf(iw, ih));
        puz_font_make(&f_num, (int)fmaxf(0.56f * q, 0.6f * kh), 1);
        puz_font_make(&f_note, (int)fmaxf(6, 0.27f * q), 1);
    }
    int small = (int)(0.6f * u);
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
    memset(&carry, 0, sizeof carry);
    hint.on = win.on = 0;
    pop.cell = -1;
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
static int busy(void) { return gl_wave_busy(&win, &geo); }

static void after(int cell)
{
    if (sky_solved(&G) && !win.on) {
        gl_light_start(&win, cell);
        hint.on = 0;
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
    if (h) {
        pop.cell = c;
        pop.t0 = ui_ms();
    }
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
static int key_at(float x, float y)
{
    if (fabsf(y - ky) > 0.62f * kh || x < geo.bx || x >= geo.bx + G.p.n * kp) return 0;
    return (int)((x - geo.bx) / kp) + 1;
}

static void draw_key(int h, float x, float y, float s, Uint8 a)
{
    SDL_SetTextureAlphaMod(t_key, a);
    ui_shadow_flat(x, y, kw * s, kh * s, a / 255.0f * 0.8f);
    ui_put_c(t_key, x, y, kw * s, kh * s);
    SDL_SetTextureAlphaMod(t_key, 255);
    if (notes) {                                /* notes: the height small, in its corner */
        float nx = x + ((h - 1) % 3 - 1) * 0.26f * kw * s, ny = y + ((h - 1) / 3 - 1) * 0.25f * kh * s;
        puz_number(&f_num, nx, ny, 0.34f * kh * s, h, PUZ_INK, a);
    } else {
        puz_number(&f_num, x, y, 0.56f * kh * s, h, PUZ_INK, a);
    }
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = sky_solved(&G);
    ui_put(t_board, geo.bx, geo.by, geo.side, geo.side);
    int lit = sel >= 0 ? SKY_HEIGHT(G.st[sel]) : 0;
    if (sel >= 0 && !solved)                    /* the picked cell's row and column */
        for (int c = 0; c < n * n; c++) {
            float x, y;
            gl_cell_xy(&geo, c, &x, &y);
            if (c != sel && (c / n == sel / n || c % n == sel % n)) ui_fill(x, y, (float)q, (float)q, 250, 214, 90, 40);
            if (lit && c != sel && SKY_HEIGHT(G.st[c]) == lit) ui_mark_last(x, y, (float)q, (float)q);
        }
    if (sel >= 0 && !solved) {
        float x, y;
        gl_cell_xy(&geo, sel, &x, &y);
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
            puz_number(&f_num, x, y, 0.56f * q, v, st < 0 ? GL_RED : CLUE, st > 0 ? 100 : 255);
        }
    for (int c = 0; c < n * n; c++) {           /* the towers, their shadows first */
        int h = SKY_HEIGHT(G.st[c]);
        if (!h) continue;
        float t = tower(h, n, q) * (c == pop.cell ? gl_pop(pop.t0) : 1), off = 0.05f * q * h / n;
        ui_shadow_flat(gl_mid_x(&geo, c) + off, gl_mid_y(&geo, c) + off, t * 1.05f, t * 1.05f, 0.4f + 0.4f * h / n);
    }
    for (int c = 0; c < n * n; c++) {
        float mx = gl_mid_x(&geo, c), my = gl_mid_y(&geo, c);
        int h = SKY_HEIGHT(G.st[c]);
        if (!h) {
            for (int k = 1; k <= n; k++) {      /* the notes */
                if (!SKY_NOTE(G.st[c], k)) continue;
                float nx = mx + ((k - 1) % 3 - 1) * 0.3f * q, ny = my + ((k - 1) / 3 - 1) * 0.29f * q;
                puz_number(&f_note, nx, ny, 0.26f * q, k, k == lit ? ENTRY : NOTE, 255);
            }
            continue;
        }
        float t = tower(h, n, q) * (c == pop.cell ? gl_pop(pop.t0) : 1);
        Uint8 tone = (Uint8)(255 - 70 * (h - 1) / (n > 1 ? n - 1 : 1));   /* the taller, the darker its wood */
        if (G.p.given[c]) tone = (Uint8)(tone * 0.72f);
        SDL_SetTextureColorMod(t_tile, tone, tone, tone);
        ui_put_c(t_tile, mx, my, t, t);
        SDL_SetTextureColorMod(t_tile, 255, 255, 255);
        const uint8_t *ink = G.p.given[c] ? GL_DARK : !solved && sky_clash(&G, c) ? GL_RED : ENTRY;
        puz_number(&f_num, mx, my + 0.02f * q, 0.5f * q * t / tower(h, n, q), h, ink, 255);
    }
    for (int h = 1; h <= n; h++) {              /* the keys */
        float x = kx0 + (h - 1) * kp;
        if (h == lit && !solved) ui_mark_gold(x, ky, 1.7f * kw);
        int held = carry.on && carry.key == h && carry.moved;
        draw_key(h, x, ky, 1, (Uint8)(held ? 90 : sky_count_height(&G, h) >= n ? 105 : 255));
    }
    if (carry.on && carry.moved) {              /* a key carried, above the finger */
        int c = gl_cell_at(&geo, carry.x, carry.y - 0.6f * kh);
        if (sky_free(&G, c)) {
            float x, y;
            gl_cell_xy(&geo, c, &x, &y);
            ui_mark_select(x, y, (float)q, (float)q);
        }
        draw_key(carry.key, carry.x, carry.y - 0.6f * kh, 1.1f, 255);
    }
    gl_touch_ring(&touch, &geo);
    gl_hint_draw(&hint, &geo);
    gl_wave_draw(&win, &geo);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&carry, 0, sizeof carry);
    if (sky_solved(&G)) return;
    int k = key_at(x, y);
    if (k) {                                    /* a key: on release (a tap), or carried to a cell */
        carry.on = 1;
        carry.key = k;
        carry.x = carry.x0 = x;
        carry.y = carry.y0 = y;
        return;
    }
    if (gl_press(&touch, &geo, x, y) < 0 && sel >= 0) {
        sel = -1;                               /* elsewhere: nothing picked */
        ui_animate();
    }
}

static void motion(float x, float y)
{
    if (carry.on) {
        carry.x = x;
        carry.y = y;
        if (hypotf(x - carry.x0, y - carry.y0) > 0.3f * kw) carry.moved = 1;
        ui_animate();
        return;
    }
    gl_motion(&touch, &geo, x, y, 0);
}

static void release(float x, float y)
{
    if (carry.on) {
        carry.on = 0;
        ui_animate();
        if (!carry.moved) {
            if (key_at(x, y) == carry.key) enter(carry.key);
            return;
        }
        int c = gl_cell_at(&geo, x, y - 0.6f * kh);   /* dropped: on the cell under the key */
        if (c < 0) c = gl_cell_at(&geo, x, y);
        if (!sky_free(&G, c)) { if (c >= 0) ui_sound(SND_DENY); return; }
        sel = c;
        enter(carry.key);
        return;
    }
    int c = touch.a, kind = gl_release(&touch);
    if (kind == GL_NOTHING) return;
    if (kind == GL_HOLD) {                      /* held: the cell cleared */
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

/* ---- the bar: the moves; the pencil, the hint -------------------------------------------------------------------- */
static void paint_button(int kind, int size, uint8_t *o)   /* the notes: a pencil */
{
    (void)kind;
    art_fig *f = art_fig_new();
    art_band(f, ART_PART, 0.40f, 0.60f, 0.72f, 0.28f, 0.15f);
    art_fig_paint(o, size, f, GL_CREAM, GL_DARK, 0.02f, 0.1f);
    f = art_fig_new();
    const float tip[] = {0.347f, 0.547f, 0.453f, 0.653f, 0.27f, 0.73f};
    art_poly(f, ART_PART, tip, 3);
    art_fig_paint(o, size, f, GL_GOLD, GL_DARK, 0.02f, 0.1f);
}

static void bar(int player, box b)
{
    (void)player;
    barbox = b;
    gl_bar_moves(b, t_small, G.hist.moves, 2);
    if (sky_solved(&G)) return;
    float x, y, d;
    gl_bar_spot(b, 1, 2, &x, &y, &d);
    ui_button_paint(x, y, d, paint_button, 0, 1, notes ? UI_ON : 0);
    gl_bar_spot(b, 0, 2, &x, &y, &d);
    ui_button(x, y, d, ICON_HINT, 0);
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (sky_solved(&G)) return 0;
    if (gl_bar_hit(barbox, 1, 2, x, y)) {
        notes = !notes;
        ui_sound(SND_TAP);
        ui_animate();
        return 1;
    }
    if (!gl_bar_hit(barbox, 0, 2, x, y)) return 0;
    int c = sky_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return 1; }
    sel = c;
    gl_light_start(&hint, c);
    pop.cell = c;
    pop.t0 = ui_ms();
    ui_sound(SND_PLACE);
    after(c);
    return 1;
}

/* ---- the menu's and the picker's cards: a town of the size, its numbers (the menu's: half built); under the
 * picker's, its level in ebony stones ---------------------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? 4 : variant, side = variant < 0 ? size : (int)(0.84f * size);
    sky_puzzle p;
    uint8_t sol[SKY_CELLS], heights[SKY_CELLS];
    sky_make(sky_size(v), sky_level(v), 2026u + (uint32_t)v, &p, sol);
    int n = p.n;
    for (int c = 0; c < n * n; c++) heights[c] = p.given[c] ? p.given[c] : variant < 0 && c < n * n / 2 ? sol[c] : 0;
    gl_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = n;
    g.lm = g.tm = g.rm = g.bm = BAND;
    gl_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)side * side, 4);
    if (!b) return;
    gl_paint(&g, b);
    paint_clues(&g, &p, heights, b);
    art_blend(rgba, size, size, b, side, side, (size - side) / 2, variant < 0 ? (size - side) / 2 : 0);
    free(b);
    if (variant >= 0) gl_stones(rgba, size, sky_level(v), side);
}

static const game skyscrapers_game = {
    .id = "skyscrapers", .order = 413, .variants = SKY_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(skyscrapers_game)
