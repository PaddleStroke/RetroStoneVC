/*
 * Star Battle in RetroStone Board, for one player: the house plank cut into regions, each region its own wood tone
 * (two touching regions never the same), thick ink between regions and fine lines between cells, gold stars, ink dots
 * (the rules: starbattle_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell: a star; again: a dot (sure to be empty, a note); again: nothing. Drag along a row or a column from an
 * empty cell to dot the empty cells of the run, or from a dot to take the dots away. Two stars touching, or too many
 * in a row, column or region, turn red. The lamp at the right of the bar: a hint (a wrong star or dot taken away, else
 * the easiest star placed, in gold light). Undo takes back one action, all the way to the start. The bar: the moves.
 * Solved: a wave of gold light from the last star.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/logicgrid2_ui.h"
#include "games/puzzles.h"
#include "games/starbattle.h"
#include "shell.h"

#define FRAME 0.035f
#define POP_MS 220

static const uint8_t STAR_GOLD[3] = {236, 190, 70}, STAR_EDGE[3] = {120, 76, 20};

static sbx_game G;
static int serial;
static struct { int on, a, b, moved, to; } ptr;
static struct { int cell; Uint32 t0; } hint = {-1, 0}, pop = {-1, 0};
static lg2_wave win;
static lg2_bar barl;
static lg2_buttons buttons;

static box last_box;
static float bx, by, side, gx, gy, cs;
static int fr, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_star, *t_dot, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a five-pointed star in a unit square */
static void star_fig(art_fig *f)
{
    float p[20];
    for (int i = 0; i < 10; i++) {
        float a = (-90 + 36 * i) * PUZ_PI / 180, r = i % 2 ? 0.2f : 0.47f;
        p[2 * i] = 0.5f + r * cosf(a);
        p[2 * i + 1] = 0.53f + r * sinf(a);
    }
    art_poly(f, ART_PART, p, 10);
}

static void paint_star(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    star_fig(f);
    art_style st = race_style(STAR_GOLD, STAR_EDGE, 0.035f, 0.75f);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

/* the regions' tones: each region a tone none of its neighbours before it has */
static void region_tones(const sbx_puz *p, int *tone)
{
    int n = p->n;
    for (int r = 0; r < n; r++) {
        unsigned used = 0;
        for (int c = 0; c < n * n; c++) {
            if (p->region[c] != r) continue;
            int nb[4] = {c % n ? c - 1 : -1, c % n < n - 1 ? c + 1 : -1, c - n, c + n};
            for (int i = 0; i < 4; i++)
                if (nb[i] >= 0 && nb[i] < n * n && p->region[nb[i]] < r) used |= 1u << tone[p->region[nb[i]]];
        }
        int t = (r * 5 + 1) % LG2_TONES;
        for (int i = 0; i < LG2_TONES && (used >> t & 1); i++) t = (t + 1) % LG2_TONES;
        tone[r] = t;
    }
}

/* the board: the plank, each region tinted, fine lines between cells, thick ink between regions and round the grid */
static void paint_board(uint8_t *o, int S, int f, const sbx_puz *p, float q, float ox, float oy)
{
    int n = p->n, tone[SBX_MAX];
    lg2_plank(o, S, S, f);
    region_tones(p, tone);
    for (int y = (int)oy; y < (int)ceilf(oy + n * q); y++)
        for (int x = (int)ox; x < (int)ceilf(ox + n * q); x++) {
            int cx = (int)((x + 0.5f - ox) / q), cy = (int)((y + 0.5f - oy) / q);
            if (cx < 0 || cy < 0 || cx >= n || cy >= n || x >= S || y >= S) continue;
            const float *t = LG2_TONE[tone[p->region[cy * n + cx]]];
            uint8_t *px = o + 4 * ((size_t)y * S + x);
            for (int i = 0; i < 3; i++) px[i] = (uint8_t)clampf(px[i] * t[i], 0, 255);
        }
    float thin = fmaxf(1, 0.025f * q), thick = fmaxf(2.5f, 0.09f * q);
    for (int i = 1; i < n; i++)                 /* the fine lines */
        for (int j = 0; j < n; j++) {
            float a = ox + i * q, b = oy + j * q;
            if (p->region[j * n + i - 1] == p->region[j * n + i])
                puz_ink_rect(o, S, S, a - thin / 2, b, a + thin / 2, b + q, PUZ_INK, 0.3f);
            a = ox + j * q, b = oy + i * q;
            if (p->region[(i - 1) * n + j] == p->region[i * n + j])
                puz_ink_rect(o, S, S, a, b - thin / 2, a + q, b + thin / 2, PUZ_INK, 0.3f);
        }
    for (int i = 0; i <= n; i++)                /* the borders */
        for (int j = 0; j < n; j++) {
            float a = ox + i * q, b = oy + j * q, lo = a - thick / 2, hi = a + thick / 2;
            if (i == 0 || i == n || p->region[j * n + i - 1] != p->region[j * n + i]) {
                if (i == 0) lo = a, hi = a + thick;
                if (i == n) lo = a - thick, hi = a;
                puz_ink_rect(o, S, S, lo, b - thick / 2 * (j > 0), hi, b + q + thick / 2 * (j < n - 1), PUZ_INK, 0.9f);
            }
            a = ox + j * q, b = oy + i * q, lo = b - thick / 2, hi = b + thick / 2;
            if (i == 0 || i == n || p->region[(i - 1) * n + j] != p->region[i * n + j]) {
                if (i == 0) lo = b, hi = b + thick;
                if (i == n) lo = b - thick, hi = b;
                puz_ink_rect(o, S, S, a - thick / 2 * (j > 0), lo, a + q + thick / 2 * (j < n - 1), hi, PUZ_INK, 0.9f);
            }
        }
}

/* the token: a gold star on a small tile */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f);
    uint8_t *b = malloc((size_t)w * w * 4), *s = malloc((size_t)w * w * 4);
    if (!b || !s) { free(b); free(s); return; }
    lg2_tile(b, w, w, 0.16f * w, LG2_TONE[1]);
    int ss = (int)(0.86f * w);
    paint_star(ss, s);
    art_blend(b, w, w, s, ss, ss, (w - ss) / 2, (w - ss) / 2);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
    free(s);
}

static void layout(box b)
{
    last_box = b;
    int n = G.p.n ? G.p.n : 8;
    side = floorf(fminf(b.w, b.h));
    bx = floorf(b.x + (b.w - side) / 2);
    by = floorf(b.y + (b.h - side) / 2);
    fr = (int)(side * FRAME);
    cs = floorf((side - 2 * fr - 0.25f * side / n) / n);
    float o = floorf((side - n * cs) / 2);
    gx = bx + o;
    gy = by + o;
    int key = ((int)side * 16 + n) * 64 + serial % 64;
    if (made_key != key) {
        made_key = key;
        int w = (int)side;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), w, fr, &G.p, cs, o, o);
        t_board = ui_tex(w, w, ui_buf(w, w));
        int s = (int)(0.86f * cs);
        ui_free(&t_star);
        paint_star(s, ui_buf(s, s));
        t_star = ui_tex(s, s, ui_buf(s, s));
        int d = (int)fmaxf(4, 0.22f * cs);
        ui_free(&t_dot);
        art_disc(d, PUZ_INK, 1, 0, ui_buf(d, d));
        t_dot = ui_tex(d, d, ui_buf(d, d));
    }
    lg2_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    hint.cell = pop.cell = -1;
    win.on = 0;
    memset(&ptr, 0, sizeof ptr);
}

static void start(int variant)
{
    sbx_new(&G, variant < 0 || variant >= SBX_VARIANTS ? 3 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return sbx_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return lg2_wave_busy(&win, G.p.n, G.p.n); }

static void after(int cell)
{
    if (sbx_solved(&G) && !win.on) lg2_wave_start(&win, (float)(cell % G.p.n), (float)(cell / G.p.n));
    ui_animate();
}

static int undo(void)
{
    if (sbx_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = sbx_script(&G, moves);
    if (sbx_solved(&G)) after(0);
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static int cell_at(float x, float y)
{
    int n = G.p.n, cx = (int)floorf((x - gx) / cs), cy = (int)floorf((y - gy) / cs);
    if (cx < 0 || cy < 0 || cx >= n || cy >= n) return -1;
    return cy * n + cx;
}

/* the run from a towards the finger, along a's row or column (whichever the finger has gone further along) */
static int run_end(int a, float x, float y)
{
    int n = G.p.n, ax = a % n, ay = a / n;
    int fx = (int)floorf((x - gx) / cs), fy = (int)floorf((y - gy) / cs);
    fx = fx < 0 ? 0 : fx >= n ? n - 1 : fx;
    fy = fy < 0 ? 0 : fy >= n ? n - 1 : fy;
    return abs(fx - ax) >= abs(fy - ay) ? ay * n + fx : fy * n + ax;
}

static int in_run(int c, int a, int b)
{
    int n = G.p.n, x = c % n, y = c / n;
    int x0 = a % n < b % n ? a % n : b % n, x1 = a % n < b % n ? b % n : a % n;
    int y0 = a / n < b / n ? a / n : b / n, y1 = a / n < b / n ? b / n : a / n;
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, solved = sbx_solved(&G);
    Uint32 now = ui_ms();
    ui_put(t_board, bx, by, side, side);
    if (hint.cell >= 0 && !lg2_hint_glow(hint.t0, gx + (hint.cell % n + 0.5f) * cs, gy + (hint.cell / n + 0.5f) * cs, cs))
        hint.cell = -1;
    int from = ptr.on && ptr.moved ? G.mark[ptr.a] : -1;
    for (int c = 0; c < n * n; c++) {
        float mx = gx + (c % n + 0.5f) * cs, my = gy + (c / n + 0.5f) * cs;
        lg2_wave_cell(&win, (float)(c % n), (float)(c / n), mx, my, cs);
        int m = G.mark[c];
        if (from >= 0 && in_run(c, ptr.a, ptr.b) && m == from) m = ptr.to;   /* the run as it will be */
        if (m == SBX_STAR) {
            float k = 1;
            if (c == pop.cell) {
                float t = (now - pop.t0) / (float)POP_MS;
                if (t < 1) { k = 1 + 0.35f * (1 - puz_ease(t)); ui_animate(); }
            }
            int bad = !solved && sbx_clash(&G, c);
            if (bad) ui_mark_glow(mx, my, 1.3f * cs);
            ui_shadow_flat(mx, my + 0.04f * cs, 0.6f * cs, 0.6f * cs, 0.45f);
            if (bad) SDL_SetTextureColorMod(t_star, 255, 110, 90);
            ui_put_c(t_star, mx, my, 0.86f * cs * k, 0.86f * cs * k);
            SDL_SetTextureColorMod(t_star, 255, 255, 255);
        } else if (m == SBX_DOT) {
            ui_put_c(t_dot, mx, my, 0.22f * cs, 0.22f * cs);
        }
    }
    lg2_wave_tick(&win, n, n);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (sbx_solved(&G)) return;
    int c = cell_at(x, y);
    if (c < 0) return;
    ptr.on = 1;
    ptr.a = ptr.b = c;
    ptr.to = G.mark[c] == SBX_DOT ? SBX_NONE : SBX_DOT;
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    int b = run_end(ptr.a, x, y);
    if (b != ptr.a && G.mark[ptr.a] != SBX_STAR) ptr.moved = 1;   /* a run starts from an empty cell or a dot */
    if (ptr.moved) ptr.b = b;
    ui_animate();
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!ptr.on) return;
    ptr.on = 0;
    ui_animate();
    int c = ptr.a;
    if (ptr.moved) {
        if (sbx_run(&G, ptr.a, ptr.b, ptr.to)) ui_sound(SND_TAP);
        return;
    }
    if (!sbx_tap(&G, c)) return;
    if (G.mark[c] == SBX_STAR) {
        pop.cell = c;
        pop.t0 = ui_ms();
        ui_sound(SND_PLACE);
    } else {
        ui_sound(SND_TAP);
    }
    after(c);
}

/* ---- the bar: the moves, the lamp -------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    lg2_bar_draw(&barl, &buttons, b, t_small, G.hist.moves, !sbx_solved(&G));
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (sbx_solved(&G) || !lg2_bar_lamp(&barl, x, y)) return 0;
    int c = sbx_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return 1; }
    hint.cell = c;
    hint.t0 = ui_ms();
    if (G.mark[c] == SBX_STAR) pop.cell = c, pop.t0 = hint.t0;
    ui_sound(SND_PLACE);
    after(c);
    return 1;
}

/* ---- the menu's and the picker's cards: a puzzle of the size and level, the stars of its first row (the menu's: of
 * its top half), and as many ebony stones as its level ------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? 4 : variant, n = sbx_size_of(v);
    sbx_puz p;
    uint8_t sol[SBX_CELLS];
    sbx_make(n, sbx_stars_of(v), sbx_level_of(v), 2026u + (uint32_t)v, &p, sol);
    int S = variant < 0 ? size : (int)(0.84f * size), f = (int)(S * FRAME);
    float q = floorf((S - 2 * f - 0.25f * S / n) / n), o = floorf((S - n * q) / 2);
    uint8_t *b = calloc((size_t)S * S, 4);
    int ss = (int)(0.86f * q);
    uint8_t *st = ss > 2 ? malloc((size_t)ss * ss * 4) : NULL;
    if (!b) { free(st); return; }
    paint_board(b, S, f, &p, q, o, o);
    if (st) {
        paint_star(ss, st);
        for (int c = 0; c < n * n; c++)
            if (sol[c] && c / n < (variant < 0 ? n / 2 : 1))
                art_blend(b, S, S, st, ss, ss, (int)(o + (c % n + 0.5f) * q - ss / 2.0f),
                          (int)(o + (c / n + 0.5f) * q - ss / 2.0f));
        free(st);
    }
    int ox = (size - S) / 2;
    art_blend(rgba, size, size, b, S, S, ox, variant < 0 ? ox : 0);
    free(b);
    if (variant >= 0) lg2_stones(rgba, size, S + (size - S - (int)(0.1f * size)) / 2, sbx_level_of(v));
}

static const game starbattle_game = {
    .id = "starbattle", .order = 416, .variants = SBX_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(starbattle_game)
