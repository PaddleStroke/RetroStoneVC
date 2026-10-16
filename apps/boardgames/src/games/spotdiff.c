/*
 * Spot the Difference in RetroStone Board, for one player: two grids of coloured tiles on one plank of the house wood
 * (side by side, or one above the other when that makes the tiles bigger: a phone held upright), the same but for a
 * few cells (the rules: spotdiff_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell that differs, in either grid: a gold ring circles it on both. A tap on a cell that does not differ is a
 * mistake (a red glow, the deny sound). The shell's tool: the hint (one difference shown, circled, a gold light pulsing
 * on it). Undo takes back the last find. The bar: a ring for each difference (gold once found) and a gentle clock (not
 * in the stars). Solved: a wave of gold light runs over both grids; the stars count the hints and the mistakes
 * (ui_logic_stars). The tiles: the Okabe-Ito colours, each with its own symbol, so no difference rests on telling
 * red from green. The shell's level screen chooses a level (150 in six chapters, made by tools/spotdiff_gen.c:
 * spotdiff_levels.h), or its die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/spotdiff.h"
#include "games/spotdiff_levels.h"

#define MARGIN 0.4f                             /* room round each grid on the plank, in cells */
#define GAP 0.7f                                /* between the grids */
#define WRONG_MS 650
#define RING_MS 260

static const uint8_t GOLD[3] = {246, 200, 74}, GOLD_INK[3] = {92, 58, 18}, INK_LIGHT[3] = {250, 246, 236},
                     INK_DARK[3] = {30, 26, 24};

enum { T_HINT, TOOLS };

/* two grids w x h on a plank: where they go in its picture */
typedef struct {
    int q, fr, pw, ph, stacked;                 /* a cell, the frame; the plank's picture; one above the other */
    float gx[2], gy[2];                         /* each grid's top left in the picture */
} pair_geo;

static sdf_game G;
static pair_geo geo;
static float bx, by;                            /* the plank on the screen */
static box last_box;
static int made_key = -1, serial;
static SDL_Texture *t_board, *t_ring, *t_hollow;
static int ring_px, hollow_px;

static int hint_k = -1;                         /* the difference a hint showed, its light */
static Uint32 hint_t0;
static int wrong_cell = -1, wrong_side;         /* the last mistake, its glow */
static Uint32 wrong_t0;
static int pop_k = -1;                          /* the ring just put, settling */
static Uint32 pop_t0;
static bench_wave win[2];
static int down_side = -1, down_cell = -1, down_tool = -1;
static Uint32 clock_ms, clock_seen;           /* the clock: the time spent on the puzzle (only while it shows,
 * until it is solved), the last look */

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a colour's own symbol (1..9) painted over a tile picture q x q */
static void paint_symbol(uint8_t *o, int q, int s, const uint8_t ink[3])
{
    art_fig *f = art_fig_new();
    switch (s) {
    case 1: art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.15f, 0.15f); break;
    case 2: {
        const float t[] = {0.5f, 0.29f, 0.72f, 0.69f, 0.28f, 0.69f};
        art_poly(f, ART_PART, t, 3);
        break;
    }
    case 3: art_rect(f, ART_PART, 0.33f, 0.33f, 0.67f, 0.67f); break;
    case 4:
        art_rect(f, ART_PART, 0.435f, 0.27f, 0.565f, 0.73f);
        art_rect(f, ART_PART, 0.27f, 0.435f, 0.73f, 0.565f);
        break;
    case 5: art_star(f, ART_PART, 0.5f, 0.53f, 0.25f); break;
    case 6: {
        const float d[] = {0.5f, 0.25f, 0.75f, 0.5f, 0.5f, 0.75f, 0.25f, 0.5f};
        art_poly(f, ART_PART, d, 4);
        break;
    }
    case 7:
        art_band(f, ART_PART, 0.31f, 0.31f, 0.69f, 0.69f, 0.12f);
        art_band(f, ART_PART, 0.69f, 0.31f, 0.31f, 0.69f, 0.12f);
        break;
    case 8: art_arc(f, ART_PART, 0.5f, 0.5f, 0.12f, 0.22f, 0, 360); break;
    default: art_rect(f, ART_PART, 0.27f, 0.435f, 0.73f, 0.565f); break;
    }
    art_fig_paint(o, q, f, ink, ink, 0, 0);
}

/* a tile q x q: its colour, bevelled, its symbol (symbols 0: too small to show) */
static void paint_tile(uint8_t *o, int q, int c, int s, int symbols)
{
    int in = q >= 10 ? (int)fmaxf(1, 0.06f * q) : 0, t = q - 2 * in;
    if (t < 1) return;
    uint8_t *b = malloc((size_t)t * t * 4);
    if (!b) return;
    for (int i = 0; i < t * t; i++) {
        memcpy(b + 4 * i, sdf_rgb[c], 3);
        b[4 * i + 3] = 255;
    }
    if (t >= 6) puz_bevel(b, t, t, 0.2f * t, fmaxf(1, 0.1f * t), 0.3f);
    if (symbols && s) paint_symbol(b, t, s, sdf_light_ink(c) ? INK_LIGHT : INK_DARK);
    memset(o, 0, (size_t)q * q * 4);
    art_blend(o, q, q, b, t, t, in, in);
    free(b);
}

/* the grids fitted in a box bw x bh; force 0/1: side by side / stacked, -1 the one with bigger cells */
static void pair_fit(pair_geo *g, int w, int h, float bw, float bh, int force)
{
    float S = fminf(bw, bh);
    int fr = (int)fmaxf(2, S * BENCH_FRAME);
    float aw = bw - 2 * fr, ah = bh - 2 * fr;
    float side = fminf(aw / (2 * w + GAP + 2 * MARGIN), ah / (h + 2 * MARGIN));
    float stack = fminf(aw / (w + 2 * MARGIN), ah / (2 * h + GAP + 2 * MARGIN));
    g->stacked = force >= 0 ? force : stack > side * 1.001f;
    int q = (int)(g->stacked ? stack : side);
    if (q < 1) q = 1;
    g->q = q;
    g->fr = fr;
    int m = (int)(MARGIN * q), gap = (int)(GAP * q);
    if (g->stacked) {
        g->pw = w * q + 2 * m + 2 * fr;
        g->ph = 2 * h * q + gap + 2 * m + 2 * fr;
        g->gx[0] = g->gx[1] = (float)(fr + m);
        g->gy[0] = (float)(fr + m);
        g->gy[1] = (float)(fr + m + h * q + gap);
    } else {
        g->pw = 2 * w * q + gap + 2 * m + 2 * fr;
        g->ph = h * q + 2 * m + 2 * fr;
        g->gy[0] = g->gy[1] = (float)(fr + m);
        g->gx[0] = (float)(fr + m);
        g->gx[1] = (float)(fr + m + w * q + gap);
    }
}

/* one grid painted into the plank's picture: its bed a shade darker, ruled, its tiles */
static void paint_grid(uint8_t *o, const pair_geo *g, const sdf_puzzle *p, int side, uint8_t **cache)
{
    int q = g->q, W = g->pw, H = g->ph, symbols = q >= 12;
    float x0 = g->gx[side], y0 = g->gy[side], gw = (float)p->w * q, gh = (float)p->h * q;
    bench_shade(o, W, H, x0, y0, x0 + gw, y0 + gh, 0.9f);
    for (int way = 0; way < 2; way++)
        for (int i = 0, n = way ? p->h : p->w; i <= n; i++) {
            int rim = i == 0 || i == n;
            float t = rim ? fmaxf(1.5f, 0.06f * q) : fmaxf(1, 0.025f * q), at = (float)i * q;
            float lo = i == 0 ? 0 : i == n ? at - t : at - t / 2, hi = lo + t;
            if (way) puz_ink_rect(o, W, H, x0, y0 + lo, x0 + gw, y0 + hi, PUZ_INK, rim ? 0.8f : 0.3f);
            else puz_ink_rect(o, W, H, x0 + lo, y0, x0 + hi, y0 + gh, PUZ_INK, rim ? 0.8f : 0.3f);
        }
    for (int i = 0; i < p->w * p->h; i++) {
        int c, s;
        sdf_cell(p, side, i, &c, &s);
        if (!c) continue;
        uint8_t **t = &cache[c * SDF_SYMBOLS + s];
        if (!*t && (*t = malloc((size_t)q * q * 4)) != NULL) paint_tile(*t, q, c, s, symbols);
        if (*t) art_blend(o, W, H, *t, q, q, (int)x0 + i % p->w * q, (int)y0 + i / p->w * q);
    }
}

/* the plank with both grids (rings: the differences circled too, for a card) */
static void paint_pair(uint8_t *o, const pair_geo *g, const sdf_puzzle *p, int rings)
{
    art_plank(g->pw - 2 * g->fr, g->ph - 2 * g->fr, g->fr, o);
    uint8_t *cache[SDF_COLOURS * SDF_SYMBOLS] = {0};
    for (int side = 0; side < 2; side++) paint_grid(o, g, p, side, cache);
    for (int i = 0; i < SDF_COLOURS * SDF_SYMBOLS; i++) free(cache[i]);
    int d = (int)(1.5f * g->q);
    if (rings <= 0 || d < 4) return;
    uint8_t *r = calloc((size_t)d * d, 4);
    if (!r) return;
    art_ring(d, 0.7f, GOLD, 1, r);
    for (int k = 0; k < p->n && k < rings; k++)
        for (int side = 0; side < 2; side++)
            art_blend(o, g->pw, g->ph, r, d, d, (int)(g->gx[side] + (p->d[k].at % p->w + 0.5f) * g->q - d / 2.0f),
                      (int)(g->gy[side] + (p->d[k].at / p->w + 0.5f) * g->q - d / 2.0f));
    free(r);
}

/* the ring round a difference found: gold in a thin dark rim, d across */
static void paint_ring(int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    art_ring(d, 0.66f, GOLD_INK, 0.85f, o);
    int e = (int)(0.05f * d), g = d - 2 * e;
    if (g < 2) return;
    uint8_t *b = calloc((size_t)g * g, 4);
    if (!b) return;
    art_ring(g, (0.33f * d + e) / (g / 2.0f), GOLD, 1, b);
    art_blend(o, d, d, b, g, g, e, e);
    free(b);
}

/* the token: an orange tile with its triangle, circled in gold */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int q = (int)(0.62f * size);
    uint8_t *t = malloc((size_t)q * q * 4), *r = malloc((size_t)size * size * 4);
    if (t && r) {
        paint_tile(t, q, 2, 2, 1);
        art_blend(rgba, size, size, t, q, q, (size - q) / 2, (size - q) / 2);
        paint_ring(size, r);
        art_blend(rgba, size, size, r, size, size, 0, 0);
    }
    free(t);
    free(r);
}

static SDL_Texture *ring_tex(int d)
{
    uint8_t *b = ui_buf(d, d);
    paint_ring(d, b);
    return ui_tex(d, d, b);
}

static void layout(box b)
{
    last_box = b;
    if (G.p.w < 4) return;
    pair_geo g;
    pair_fit(&g, G.p.w, G.p.h, b.w, b.h, -1);
    bx = floorf(b.x + (b.w - g.pw) / 2);
    by = floorf(b.y + (b.h - g.ph) / 2);
    int key = (serial % 4096) * 4096 + g.q * 2 + g.stacked;
    if (made_key == key && g.pw == geo.pw && g.ph == geo.ph) return;
    made_key = key;
    geo = g;
    ui_free(&t_board);
    uint8_t *o = ui_buf(g.pw, g.ph);
    paint_pair(o, &geo, &G.p, 0);
    t_board = ui_tex(g.pw, g.ph, o);
    int d = (int)(1.45f * g.q);
    if (d != ring_px) {
        ring_px = d;
        ui_free(&t_ring);
        t_ring = ring_tex(d);
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    hint_k = pop_k = wrong_cell = -1;
    down_side = down_cell = down_tool = -1;
    win[0].on = win[1].on = 0;
}

static void begin(const sdf_puzzle *p)
{
    sdf_load(&G, p);
    serial++;
    made_key = -1;
    calm();
    clock_ms = 0;
    clock_seen = ui_ms();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    sdf_puzzle p;
    if (level < 0 || level >= SDF_LEVELS || !sdf_decode(sdf_levels[level], &p)) sdf_make_variant(&p, 0, 1);
    begin(&p);
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    sdf_puzzle p;
    sdf_make_variant(&p, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin(&p);
}

static int status(void) { return sdf_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win[0]) || bench_wave_busy(&win[1]); }
static int score(void) { return G.taps; }
static int stars(void) { return ui_logic_stars(G.hints, G.mistakes); }

static void after(int k)
{
    if (k >= 0) {
        pop_k = k;
        pop_t0 = ui_ms();
    }
    if (sdf_solved(&G) && !win[0].on) {
        int at = k >= 0 ? G.p.d[k].at : 0;
        for (int s = 0; s < 2; s++) bench_wave_from(&win[s], at, G.p.w, G.p.h);
        hint_k = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (sdf_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = sdf_script(&G, moves);
    after(G.nfound ? G.hist[G.nfound - 1] : -1);
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void cell_mid(int side, int i, float *x, float *y)
{
    *x = bx + geo.gx[side] + (i % G.p.w + 0.5f) * geo.q;
    *y = by + geo.gy[side] + (i / G.p.w + 0.5f) * geo.q;
}

static void draw(void)
{
    if (made_key < 0 || !t_board) layout(last_box);
    if (!t_board) return;
    Uint32 now = ui_ms();
    if (!sdf_solved(&G) && now - clock_seen < 1500) clock_ms += now - clock_seen;   /* away: the clock waits */
    clock_seen = now;
    ui_put(t_board, bx, by, (float)geo.pw, (float)geo.ph);
    float q = (float)geo.q;
    for (int k = 0; k < G.p.n; k++) {          /* the rings round the differences found */
        if (!G.found[k]) continue;
        float s = 1;
        if (k == pop_k) {
            float t = (now - pop_t0) / (float)RING_MS;
            if (t < 1) {
                s = 1 + 0.45f * (1 - puz_ease(t));
                ui_animate();
            }
        }
        for (int side = 0; side < 2; side++) {
            float cx, cy;
            cell_mid(side, G.p.d[k].at, &cx, &cy);
            ui_put_c(t_ring, cx, cy, ring_px * s, ring_px * s);
        }
    }
    if (hint_k >= 0) {                          /* a hint's gold light, pulsing on both */
        float t = (float)(now - hint_t0), p = puz_pulse(fmodf(t, 750), 0, 750);
        if (t > 3 * 750) hint_k = -1;
        else {
            for (int side = 0; side < 2; side++) {
                float cx, cy;
                cell_mid(side, G.p.d[hint_k].at, &cx, &cy);
                ui_mark_gold_a(cx, cy, 2.4f * q, 0.25f + 0.75f * p);
            }
            ui_animate();
        }
    }
    if (wrong_cell >= 0) {                      /* a mistake: a red glow fading */
        float t = (now - wrong_t0) / (float)WRONG_MS;
        if (t >= 1) wrong_cell = -1;
        else {
            float cx, cy;
            cell_mid(wrong_side, wrong_cell, &cx, &cy);
            ui_mark_glow(cx, cy, q * (1.5f - 0.5f * t));
            ui_animate();
        }
    }
    for (int side = 0; side < 2; side++)        /* the win: a wave of gold light over both */
        for (int i = 0; win[side].on && i < G.p.w * G.p.h; i++) {
            float cx, cy;
            cell_mid(side, i, &cx, &cy);
            bench_wave_at(&win[side], i % G.p.w + 0.5f, i / G.p.w + 0.5f, cx, cy, 2.2f * q);
        }
    bench_wave_tick(&win[0]);
    bench_wave_tick(&win[1]);
    ui_tool(T_HINT, ICON_HINT, sdf_solved(&G) ? UI_DIM : 0);
}

/* the bar: a ring a difference (gold: found), then the clock at its right end */
static void bar(int player, box b)
{
    (void)player;
    box r = ui_bar_countdown(b, (int)(clock_ms / 1000));
    int n = G.p.n;
    if (n < 1) return;
    float d = fminf(0.52f * ui.unit, r.w / (n * 1.22f)), cy = r.y + r.h / 2;
    if (d < 4) return;
    if ((int)d != hollow_px || !t_hollow) {
        hollow_px = (int)d;
        ui_free(&t_hollow);
        uint8_t *o = ui_buf(hollow_px, hollow_px);
        memset(o, 0, (size_t)hollow_px * hollow_px * 4);
        art_ring(hollow_px, 0.62f, PUZ_CREAM, 0.45f, o);
        t_hollow = ui_tex(hollow_px, hollow_px, o);
    }
    for (int k = 0; k < n; k++) {
        float cx = r.x + (k + 0.5f) * d * 1.22f;
        if (k < G.nfound) {
            ui_put_c(t_ring, cx, cy, d * 1.1f, d * 1.1f);
            ui_mark_gold_a(cx, cy, 0.9f * d, 0.6f);
        } else {
            ui_put_c(t_hollow, cx, cy, d, d);
        }
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int cell_at(float x, float y, int *side)
{
    for (int s = 0; s < 2; s++) {
        float ix = x - bx - geo.gx[s], iy = y - by - geo.gy[s];
        if (ix < 0 || iy < 0 || ix >= G.p.w * geo.q || iy >= G.p.h * geo.q) continue;
        *side = s;
        return (int)(iy / geo.q) * G.p.w + (int)(ix / geo.q);
    }
    return -1;
}

static void press(float x, float y)
{
    down_side = down_cell = down_tool = -1;
    if (sdf_solved(&G)) return;
    down_tool = ui_tool_hit(x, y);
    if (down_tool >= 0) return;
    down_cell = cell_at(x, y, &down_side);
}

static void motion(float x, float y)
{
    (void)x;
    (void)y;
}

static void release(float x, float y)
{
    if (down_tool == T_HINT && ui_tool_hit(x, y) == T_HINT) {
        int k = sdf_hint(&G);
        down_tool = -1;
        if (k < 0) {
            ui_sound(SND_DENY);
            return;
        }
        hint_k = k;
        hint_t0 = ui_ms();
        ui_sound(SND_PLACE);
        after(k);
        return;
    }
    int side, c = cell_at(x, y, &side);
    if (down_cell < 0 || c != down_cell) {
        down_cell = -1;
        return;
    }
    down_cell = -1;
    int k = sdf_tap(&G, c);
    if (k == -2) return;                        /* one found before */
    if (k < 0) {
        wrong_cell = c;
        wrong_side = side;
        wrong_t0 = ui_ms();
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    ui_sound(SND_PLACE);
    after(k);
}

/* ---- the pictures for the shell: the menu's card, the picker's, the chapters' tabs ------------------------------- */
/* a puzzle's pair of grids into a card size x size (rings: the first differences circled) */
static void pair_pic(const sdf_puzzle *p, int size, int rings, uint8_t *rgba)
{
    pair_geo g;
    pair_fit(&g, p->w, p->h, (float)size, (float)size, 0);
    uint8_t *b = malloc((size_t)g.pw * g.ph * 4);
    if (!b) return;
    paint_pair(b, &g, p, rings);
    art_blend(rgba, size, size, b, g.pw, g.ph, (size - g.pw) / 2, (size - g.ph) / 2);
    free(b);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    sdf_puzzle p;
    if (variant < 0) {                          /* the menu's: the heart, two of its differences found */
        sdf_grid(&p, SDF_DRAWING, 8, 8, 0, 3);
        sdf_differ(&p, 3, 0, 3);
        pair_pic(&p, size, 2, rgba);
        return;
    }
    sdf_make_variant(&p, variant, 2026u + (uint32_t)variant);
    pair_pic(&p, size, 0, rgba);
}

/* a chapter's tab: its first level's grid alone */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= SDF_CHAPTERS ? SDF_CHAPTERS - 1 : chapter;
    sdf_puzzle p;
    if (!sdf_decode(sdf_levels[ch * SDF_LEVELS / SDF_CHAPTERS], &p)) return;
    p.n = 0;
    pair_geo g;
    int fr = (int)fmaxf(2, size * BENCH_FRAME), q = (size - 2 * fr) / (p.w > p.h ? p.w : p.h);
    if (q < 1) return;
    memset(&g, 0, sizeof g);
    g.q = q;
    g.fr = fr;
    g.pw = p.w * q + 2 * fr;
    g.ph = p.h * q + 2 * fr;
    g.gx[0] = g.gy[0] = (float)fr;
    uint8_t *b = malloc((size_t)g.pw * g.ph * 4);
    if (!b) return;
    art_plank(g.pw - 2 * fr, g.ph - 2 * fr, fr, b);
    uint8_t *cache[SDF_COLOURS * SDF_SYMBOLS] = {0};
    paint_grid(b, &g, &p, 0, cache);
    for (int i = 0; i < SDF_COLOURS * SDF_SYMBOLS; i++) free(cache[i]);
    art_blend(rgba, size, size, b, g.pw, g.ph, (size - g.pw) / 2, (size - g.ph) / 2);
    free(b);
}

static const game spotdiff_game = {
    .id = "spotdiff", .order = 471, .variants = SDF_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = SDF_LEVELS, .chapters = SDF_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(spotdiff_game)
