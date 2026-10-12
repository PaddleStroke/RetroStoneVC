/*
 * Rectangles in RetroStone Board, for one player: a darker tray ruled in cells in the house's walnut frame, the
 * numbers burned into it, and the player's rectangles laid on it as wooden tiles in several tones (two touching tiles
 * never the same), each number still showing through its tile (the rules: rectangles_rules.c; what the logic grids
 * share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag from a cell to another: the rectangle with these corners is drawn as you go (its cell count shown above the
 * finger) and laid when you lift it; the tiles under it go. Or tap a cell, then another (the same one again: a single
 * cell). Tap a tile to take it away. A tile holding no number, two numbers, or a number not its area is pale. The
 * shell's tool, the lamp: a hint (a wrong tile taken away, else the easiest right one laid, in gold light). Undo
 * takes back one action, all the way to the start. The bar: the moves. Solved: a wave of gold light over the tiles;
 * the stars count the hints and the mistakes (bench.h).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/race.h"
#include "games/rectangles.h"
#include "shell.h"

#define INSET 0.055f                            /* a tile's gap to its cells' edges, a part of a cell */
#define CACHE 64

static const float TRAY[3] = {0.74f, 0.70f, 0.66f};

static rct_game G;
static int pending = -1, serial;                /* pending: a tapped corner waiting for the other */
static struct { int on, a, b, moved; float x, y; } ptr;
static bench_flash hint = {-1, 0};
static bench_wave win;

static box last_box;
static float bx, by, side, gx, gy, cs;
static int fr, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_small;
static puz_font f_num;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the board: the plank in its frame, the grid's tray darker, ruled in cells */
static void paint_board(uint8_t *o, int S, int f, int n, float q, float ox, float oy)
{
    bench_plank(o, S, S, f);
    float g = n * q;
    for (int y = (int)oy; y < (int)ceilf(oy + g); y++)
        for (int x = (int)ox; x < (int)ceilf(ox + g); x++) {
            if (x < 0 || y < 0 || x >= S || y >= S) continue;
            uint8_t *p = o + 4 * ((size_t)y * S + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * TRAY[i]);
        }
    for (int i = 0; i <= n; i++) {
        int edge = i == 0 || i == n;
        float t = edge ? fmaxf(2, 0.06f * q) : fmaxf(1, 0.03f * q), p = i * q, lo = p - t / 2, hi = p + t / 2;
        if (i == 0) lo = 0, hi = t;
        if (i == n) lo = g - t, hi = g;
        puz_ink_rect(o, S, S, ox + lo, oy, ox + hi, oy + g, PUZ_INK, edge ? 0.8f : 0.32f);
        puz_ink_rect(o, S, S, ox, oy + lo, ox + g, oy + hi, PUZ_INK, edge ? 0.8f : 0.32f);
    }
}

/* the tiles' textures: a size and a tone, kept while used */
typedef struct { int w, h, tone; unsigned used; SDL_Texture *t; } cached;
static cached cache[CACHE];
static unsigned tick;

static SDL_Texture *tile_tex(int w, int h, int tone)
{
    if (w < 4 || h < 4) return NULL;
    int slot = 0;
    for (int i = 0; i < CACHE; i++) {
        if (cache[i].t && cache[i].w == w && cache[i].h == h && cache[i].tone == tone) {
            cache[i].used = ++tick;
            return cache[i].t;
        }
        if (!cache[i].t || (cache[slot].t && cache[i].used < cache[slot].used)) slot = i;
    }
    ui_free(&cache[slot].t);
    uint8_t *b = malloc((size_t)w * h * 4);
    if (!b) return NULL;
    bench_tile(b, w, h, fminf(0.16f * cs, 0.3f * fminf(w, h)), BENCH_TONE[tone]);
    cache[slot] = (cached){w, h, tone, ++tick, ui_tex(w, h, b)};
    free(b);
    return cache[slot].t;
}

/* the token: three tiles of the tones side by side, a number on the big one */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int u = size / 4, g = (int)fmaxf(1, size * 0.03f);
    struct { int x, y, w, h, tone, v; } t[3] = {{0, 0, 2, 2, 1, 4}, {2, 0, 2, 1, 2, 0}, {2, 1, 2, 3, 0, 0}};
    t[2].h = 1;
    int ox = (size - 4 * u) / 2, oy = (size - 2 * u) / 2;
    for (int i = 0; i < 3; i++) {
        int w = t[i].w * u - 2 * g, h = t[i].h * u - 2 * g;
        uint8_t *b = malloc((size_t)w * h * 4);
        if (!b) return;
        bench_tile(b, w, h, 0.18f * u, BENCH_TONE[t[i].tone]);
        if (t[i].v) puz_burn(b, w, h, t[i].v, w / 2.0f, h * 0.53f, 0.62f * h);
        art_blend(rgba, size, size, b, w, h, ox + t[i].x * u + g, oy + t[i].y * u + g);
        free(b);
    }
}

static void layout(box b)
{
    last_box = b;
    int n = G.n ? G.n : 7;
    side = floorf(fminf(b.w, b.h));
    bx = floorf(b.x + (b.w - side) / 2);
    by = floorf(b.y + (b.h - side) / 2);
    fr = (int)(side * BENCH_FRAME);
    cs = floorf((side - 2 * fr - 0.3f * side / n) / n);
    float ox = floorf((side - n * cs) / 2);
    gx = bx + ox;
    gy = by + ox;
    int key = ((int)side * 32 + n) * 64 + serial % 64;
    if (made_key != key) {
        made_key = key;
        int w = (int)side;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), w, fr, n, cs, ox, ox);
        t_board = ui_tex(w, w, ui_buf(w, w));
        for (int i = 0; i < CACHE; i++) ui_free(&cache[i].t);
        puz_font_make(&f_num, (int)(0.6f * cs), 1);
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    pending = -1;
    hint.at = -1;
    win.on = 0;
    memset(&ptr, 0, sizeof ptr);
}

static void start(int variant)
{
    rct_new(&G, variant < 0 || variant >= RCT_VARIANTS ? 3 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return rct_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (rct_solved(&G) && !win.on) {
        bench_wave_start(&win, (cell % G.n) + 0.5f, (cell / G.n) + 0.5f, G.n, G.n);
        pending = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (rct_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = rct_script(&G, moves);
    if (rct_solved(&G)) after(0);
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static int cell_at(float x, float y, int clamp)
{
    int n = G.n, cx = (int)floorf((x - gx) / cs), cy = (int)floorf((y - gy) / cs);
    if (clamp) {
        cx = cx < 0 ? 0 : cx >= n ? n - 1 : cx;
        cy = cy < 0 ? 0 : cy >= n ? n - 1 : cy;
    }
    if (cx < 0 || cy < 0 || cx >= n || cy >= n) return -1;
    return cy * n + cx;
}

/* the tones: each rectangle its own (a steady choice from its place), never the same as a touching one before it */
static int tones_of(uint8_t *tone)
{
    int n = G.n, k = 0;
    for (int c = 0; c < n * n; c++) {
        uint32_t r = G.own[c];
        int x, y, w, h;
        if (!r) continue;
        rct_unpack(r, &x, &y, &w, &h);
        if (y * n + x != c) continue;
        unsigned used = 0;                      /* the tones of the rectangles touching it, already given */
        for (int j = y - 1; j <= y + h; j++)
            for (int i = x - 1; i <= x + w; i++) {
                if (i < 0 || j < 0 || i >= n || j >= n || (i >= x && i < x + w && j >= y && j < y + h)) continue;
                if ((i == x - 1 || i == x + w) && (j == y - 1 || j == y + h)) continue;   /* corners do not touch */
                uint32_t o = G.own[j * n + i];
                if (!o) continue;
                int ox, oy, ow, oh;
                rct_unpack(o, &ox, &oy, &ow, &oh);
                if (oy * n + ox < c) used |= 1u << tone[oy * n + ox];
            }
        int t = (int)(race_hash(c, (int)r) * BENCH_TONES) % BENCH_TONES;
        for (int i = 0; i < BENCH_TONES && (used >> t & 1); i++) t = (t + 1) % BENCH_TONES;
        tone[c] = (uint8_t)t;
        k++;
    }
    return k;
}

static void tile_at(int x, int y, int w, int h, int tone, int good, float lift)
{
    float in = INSET * cs, px = gx + x * cs + in, py = gy + y * cs + in, pw = w * cs - 2 * in, ph = h * cs - 2 * in;
    SDL_Texture *t = tile_tex((int)pw, (int)ph, tone);
    if (!t) return;
    float off = (0.05f + lift) * cs;            /* its shadow: the tile in black, faint, to the lower right */
    SDL_SetTextureColorMod(t, 0, 0, 0);
    SDL_SetTextureAlphaMod(t, good ? 80 : 40);
    ui_put(t, px + 0.6f * off, py + off, pw, ph);
    SDL_SetTextureColorMod(t, good ? 255 : 205, good ? 255 : 200, good ? 255 : 190);
    SDL_SetTextureAlphaMod(t, good ? 255 : 150);
    ui_put(t, px, py - lift * cs, pw, ph);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.n, solved = rct_solved(&G);
    ui_put(t_board, bx, by, side, side);
    static uint8_t tone[RCT_CELLS];
    tones_of(tone);
    for (int c = 0; c < n * n; c++) {           /* the tiles */
        uint32_t r = G.own[c];
        int x, y, w, h;
        if (!r) continue;
        rct_unpack(r, &x, &y, &w, &h);
        if (y * n + x != c) continue;
        tile_at(x, y, w, h, tone[c], solved || rct_good(&G, r), 0);
    }
    if (ptr.on && ptr.moved) {                  /* the rectangle being drawn */
        int a = ptr.a, b = ptr.b;
        int x0 = a % n < b % n ? a % n : b % n, x1 = a % n < b % n ? b % n : a % n;
        int y0 = a / n < b / n ? a / n : b / n, y1 = a / n < b / n ? b / n : a / n;
        ui_mark_select(gx + x0 * cs, gy + y0 * cs, (x1 - x0 + 1) * cs, (y1 - y0 + 1) * cs);
    }
    if (pending >= 0) ui_mark_select(gx + pending % n * cs, gy + pending / n * cs, cs, cs);
    if (hint.at >= 0) {
        uint32_t r = G.own[hint.at];
        int x = hint.at % n, y = hint.at / n, w = 1, h = 1;
        if (r) rct_unpack(r, &x, &y, &w, &h);
        bench_glow(&hint, gx + (x + w / 2.0f) * cs, gy + (y + h / 2.0f) * cs, 2.6f * cs * sqrtf((float)(w * h)));
    }
    for (int c = 0; c < n * n; c++) {           /* the numbers, and the win's light */
        float mx = gx + (c % n + 0.5f) * cs, my = gy + (c / n + 0.5f) * cs;
        bench_wave_at(&win, c % n + 0.5f, c / n + 0.5f, mx, my, 2.3f * cs);
        int v = G.p.clue[c];
        if (v) puz_number(&f_num, mx, my + 0.03f * cs, 0.6f * cs * (v >= 10 ? 0.85f : 1), v, PUZ_INK, 255);
    }
    bench_wave_tick(&win);
    ui_tool(0, ICON_HINT, rct_solved(&G) ? UI_DIM : 0);
    if (ptr.on && ptr.moved) {                  /* its cell count, above the finger */
        int a = ptr.a, b = ptr.b, cnt = (abs(a % n - b % n) + 1) * (abs(a / n - b / n) + 1);
        float d = 1.1f * ui.unit, x = clampf(ptr.x, bx + d / 2, bx + side - d / 2), y = ptr.y - 1.2f * ui.unit;
        if (y < by + d / 2) y = ptr.y + 1.2f * ui.unit;
        SDL_Texture *t = tile_tex((int)d, (int)d, 0);
        if (t) {
            ui_shadow_flat(x, y + 0.05f * d, d, d, 0.6f);
            ui_put_c(t, x, y, d, d);
        }
        puz_number(&f_num, x, y + 0.03f * d, 0.55f * d * (cnt >= 10 ? 0.8f : 1), cnt, PUZ_INK, 255);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (rct_solved(&G)) return;
    if (ui_tool_hit(x, y) == 0) {               /* the shell's tool: the hint */
        pending = -1;
        int c = rct_hint(&G);
        if (c < 0) { ui_sound(SND_DENY); return; }
        bench_flash_start(&hint, c);
        ui_sound(SND_PLACE);
        after(c);
        return;
    }
    int c = cell_at(x, y, 0);
    if (c < 0) {                                /* off the grid: a tapped corner forgotten */
        if (pending >= 0) pending = -1, ui_animate();
        return;
    }
    ptr.on = 1;
    ptr.a = ptr.b = c;
    ptr.x = x;
    ptr.y = y;
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    int c = cell_at(x, y, 1);
    ptr.x = x;
    ptr.y = y;
    if (c != ptr.a) ptr.moved = 1;
    ptr.b = c;
    ui_animate();
}

static void lay(int a, int b)
{
    pending = -1;
    if (!rct_draw(&G, a, b)) { ui_sound(SND_DENY); return; }
    ui_sound(SND_PLACE);
    after(a);
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!ptr.on) return;
    ptr.on = 0;
    ui_animate();
    if (ptr.moved) { lay(ptr.a, ptr.b); return; }
    int c = ptr.a;
    if (pending >= 0) { lay(pending, c); return; }
    if (G.own[c]) {                             /* a tile tapped: taken away */
        rct_erase(&G, c);
        ui_sound(SND_TAP);
        return;
    }
    pending = c;
    ui_sound(SND_TAP);
}

/* ---- the bar: the moves, the lamp -------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the menu's and the picker's cards: a puzzle of the size and level, some tiles laid, and as many ebony stones
 * as its level; the menu's a 7 x 7 half done ----------------------------------------------------------------------- */
static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, n = rct_size_of(v);
    rct_puz p;
    static uint32_t sol[RCT_CELLS];
    rct_make(n, rct_level_of(v), 2026u + (uint32_t)v, &p, sol);
    int S = variant < 0 || bare ? size : (int)(0.84f * size), f = (int)(S * BENCH_FRAME);
    float q = floorf((S - 2 * f - 0.3f * S / n) / n), o = floorf((S - n * q) / 2);
    uint8_t *b = calloc((size_t)S * S, 4);
    if (!b) return;
    paint_board(b, S, f, n, q, o, o);
    int k = 0;
    for (int c = 0; c < n * n; c++) {           /* some tiles: the first few rectangles (the menu's: half) */
        int x, y, w, h;
        rct_unpack(sol[c], &x, &y, &w, &h);
        if (y * n + x != c || (k++ % (variant < 0 ? 2 : 3)) != 0) continue;
        float in = fmaxf(1, INSET * q);
        int tw = (int)(w * q - 2 * in), th = (int)(h * q - 2 * in);
        if (tw < 3 || th < 3) continue;
        uint8_t *t = malloc((size_t)tw * th * 4);
        if (!t) continue;
        bench_tile(t, tw, th, fminf(0.16f * q, 0.3f * fminf(tw, th)), BENCH_TONE[k % BENCH_TONES]);
        art_blend(b, S, S, t, tw, th, (int)(o + x * q + in), (int)(o + y * q + in));
        free(t);
    }
    if (q >= 8)
        for (int c = 0; c < n * n; c++)
            if (p.clue[c])
                puz_digits(b, S, S, p.clue[c], o + (c % n + 0.5f) * q, o + (c / n + 0.55f) * q,
                           0.62f * q * (p.clue[c] >= 10 ? 0.85f : 1), PUZ_INK, 1, 1);
    int ox = (size - S) / 2;
    art_blend(rgba, size, size, b, S, S, ox, variant < 0 ? ox : 0);
    free(b);
    if (variant >= 0 && !bare) bench_stones(rgba, size, S, rct_level_of(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

static const game rectangles_game = {
    .id = "rectangles", .order = 415, .variants = RCT_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {RCT_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .score = score, .level_stars = stars, .fill = 1, .tools = 1,
};
REGISTER_GAME(rectangles_game)
