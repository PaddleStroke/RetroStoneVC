/*
 * Star Battle in RetroStone Board, for one player: the house plank cut into regions, each region its own wood tone
 * (two touching regions never the same), thick ink between regions and fine lines between cells, gold stars, ink dots
 * (the rules: starbattle_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell: a star; again: a dot (sure to be empty, a note); again: nothing. Drag along a row or a column from an
 * empty cell to dot the empty cells of the run, or from a dot to take the dots away. Two stars touching, or too many
 * in a row, column or region, turn red. The shell's tool, the lamp: a hint (a wrong star or dot taken away, else
 * the easiest star placed, in gold light). Undo takes back one action, all the way to the start. The bar: the moves.
 * Solved: a wave of gold light from the last star; the stars count the hints and the mistakes (bench.h).
 *
 * The shell's level screen chooses a level (150 in six chapters, starbattle_levels.h), or its die tab a puzzle made at
 * random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/starbattle.h"
#include "games/starbattle_levels.h"
#include "shell.h"

static const uint8_t STAR_GOLD[3] = {236, 190, 70}, STAR_EDGE[3] = {120, 76, 20};

static sbx_game G;
static int serial;
static bench_geo geo;                           /* the cells (the finger's runs) */
static bench_touch touch;
static bench_flash hint = {-1, 0}, pop = {-1, 0};
static bench_wave win;

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
    art_style st = art_flat(STAR_GOLD, STAR_EDGE, 0.035f, 0.75f);
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
        int t = (r * 5 + 1) % BENCH_TONES;
        for (int i = 0; i < BENCH_TONES && (used >> t & 1); i++) t = (t + 1) % BENCH_TONES;
        tone[r] = t;
    }
}

/* a wall into the walls' mask: each pixel's coverage, the most of the walls over it (so a joint is inked once, with
 * no darker knot where two walls overlap) */
static void wall_rect(float *m, int S, float x0, float y0, float x1, float y1)
{
    for (int y = (int)floorf(y0); y < (int)ceilf(y1); y++)
        for (int x = (int)floorf(x0); x < (int)ceilf(x1); x++) {
            if (x < 0 || y < 0 || x >= S || y >= S) continue;
            float k = (fminf(x + 1, x1) - fmaxf(x, x0)) * (fminf(y + 1, y1) - fmaxf(y, y0));
            float *v = m + (size_t)y * S + x;
            if (k > *v) *v = k;
        }
}

/* a box blur of a mask, r pixels each way, along rows (dx 1) or columns (dx S): a running sum */
static void blur_line(const float *src, float *dst, int S, int along_rows, int r)
{
    for (int l = 0; l < S; l++) {
        const float *s = along_rows ? src + (size_t)l * S : src + l;
        float *d = along_rows ? dst + (size_t)l * S : dst + l;
        size_t st = along_rows ? 1 : (size_t)S;
        float sum = 0;
        for (int i = -r; i <= r; i++) sum += i >= 0 && i < S ? s[i * st] : 0;
        for (int i = 0; i < S; i++) {
            d[i * st] = sum / (2 * r + 1);
            int a = i + r + 1, b = i - r;
            sum += (a < S ? s[a * st] : 0) - (b >= 0 ? s[b * st] : 0);
        }
    }
}

/* the walls inked from their mask, each casting a soft shadow down and to the right into the cells beside it */
static void ink_walls(uint8_t *o, int S, float *m, float q)
{
    float *a = calloc((size_t)S * S, sizeof *a), *b = calloc((size_t)S * S, sizeof *b);
    if (a && b) {
        int d = (int)fmaxf(1, 0.05f * q), r = (int)fmaxf(1, 0.06f * q);
        for (int y = 0; y < S; y++)             /* the mask moved down and right, then softened */
            for (int x = 0; x < S; x++) a[(size_t)y * S + x] = x >= d && y >= d ? m[(size_t)(y - d) * S + x - d] : 0;
        blur_line(a, b, S, 1, r);
        blur_line(b, a, S, 0, r);
        for (size_t i = 0; i < (size_t)S * S; i++) {
            float k = 1 - 0.32f * a[i] * (1 - m[i]);
            for (int c = 0; c < 3; c++) o[4 * i + c] = (uint8_t)(o[4 * i + c] * k);
        }
    }
    free(a);
    free(b);
    for (size_t i = 0; i < (size_t)S * S; i++)
        if (m[i] > 0)
            for (int c = 0; c < 3; c++) o[4 * i + c] = (uint8_t)(o[4 * i + c] + (PUZ_INK[c] - o[4 * i + c]) * 0.9f * m[i]);
}

/* the board: the plank, each region tinted, fine lines between cells, thick ink between regions and round the grid */
static void paint_board(uint8_t *o, int S, int f, const sbx_puz *p, float q, float ox, float oy)
{
    int n = p->n, tone[SBX_MAX];
    float *m = calloc((size_t)S * S, sizeof *m);
    if (!m) return;
    bench_plank(o, S, S, f);
    region_tones(p, tone);
    for (int y = (int)oy; y < (int)ceilf(oy + n * q); y++)
        for (int x = (int)ox; x < (int)ceilf(ox + n * q); x++) {
            int cx = (int)((x + 0.5f - ox) / q), cy = (int)((y + 0.5f - oy) / q);
            if (cx < 0 || cy < 0 || cx >= n || cy >= n || x >= S || y >= S) continue;
            const float *t = BENCH_TONE[tone[p->region[cy * n + cx]]];
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
                wall_rect(m, S, lo, b - thick / 2 * (j > 0), hi, b + q + thick / 2 * (j < n - 1));
            }
            a = ox + j * q, b = oy + i * q, lo = b - thick / 2, hi = b + thick / 2;
            if (i == 0 || i == n || p->region[(i - 1) * n + j] != p->region[i * n + j]) {
                if (i == 0) lo = b, hi = b + thick;
                if (i == n) lo = b - thick, hi = b;
                wall_rect(m, S, a - thick / 2 * (j > 0), lo, a + q + thick / 2 * (j < n - 1), hi);
            }
        }
    ink_walls(o, S, m, q);
    free(m);
}

/* the token: a gold star on a small tile */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f);
    uint8_t *b = malloc((size_t)w * w * 4), *s = malloc((size_t)w * w * 4);
    if (!b || !s) { free(b); free(s); return; }
    bench_tile(b, w, w, 0.16f * w, BENCH_TONE[1]);
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
    fr = (int)(side * BENCH_FRAME);
    cs = floorf((side - 2 * fr - 0.25f * side / n) / n);
    float o = floorf((side - n * cs) / 2);
    gx = bx + o;
    gy = by + o;
    geo = (bench_geo){.cols = n, .rows = n, .cs = (int)cs, .gx = gx, .gy = gy};
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
    bench_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    hint.at = pop.at = -1;
    win.on = 0;
    memset(&touch, 0, sizeof touch);
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

/* a level of the shell's level screen (starbattle_levels.h) */
static void start(int level)
{
    level = level < 0 ? 0 : level >= SBX_STAGES ? SBX_STAGES - 1 : level;
    if (!sbx_open(&G, sbx_levels[level])) sbx_new(&G, 0, 1);
    begin();
}

/* a puzzle made at random (the level screen's die tab): a size and a level */
static void start_random(int variant)
{
    sbx_new(&G, variant < 0 || variant >= SBX_VARIANTS ? 3 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return sbx_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (sbx_solved(&G) && !win.on) bench_wave_from(&win, cell, G.p.n, G.p.n);
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
static int running(void) { return touch.on && touch.moved && touch.b != touch.a; }   /* a run along the cells */

static int run_to(void) { return G.mark[touch.a] == SBX_DOT ? SBX_NONE : SBX_DOT; }   /* what it makes of them */

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, solved = sbx_solved(&G);
    ui_put(t_board, bx, by, side, side);
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * cs);
    int from = running() ? G.mark[touch.a] : -1;
    for (int c = 0; c < n * n; c++) {
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        bench_wave_at(&win, c % n + 0.5f, c / n + 0.5f, mx, my, 2.3f * cs);
        int m = G.mark[c];
        if (from >= 0 && bench_in_run(&geo, c, touch.a, touch.b) && m == from) m = run_to();   /* the run to be */
        if (m == SBX_STAR) {
            float k = bench_pop(&pop, c, 0.35f);
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
    bench_wave_tick(&win);
    ui_tool(0, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    if (sbx_solved(&G)) return;
    if (ui_tool_hit(x, y) == 0) {               /* the shell's tool: the hint */
        int c = sbx_hint(&G);
        if (c < 0) { ui_sound(SND_DENY); return; }
        bench_flash_start(&hint, c);
        if (G.mark[c] == SBX_STAR) bench_flash_start(&pop, c);
        ui_sound(SND_PLACE);
        after(c);
        return;
    }
    bench_press(&touch, &geo, x, y);
}

/* a run starts from an empty cell or a dot */
static void motion(float x, float y) { bench_motion(&touch, &geo, x, y, touch.on && G.mark[touch.a] != SBX_STAR); }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int run = running(), to = run_to(), c = touch.a;
    if (bench_release(&touch) == BENCH_NOTHING) return;
    if (run) {
        if (sbx_run(&G, touch.a, touch.b, to)) ui_sound(SND_TAP);
        return;
    }
    if (!sbx_tap(&G, c)) return;
    if (G.mark[c] == SBX_STAR) {
        bench_flash_start(&pop, c);
        ui_sound(SND_PLACE);
    } else {
        ui_sound(SND_TAP);
    }
    after(c);
}

/* ---- the bar: the moves ------------------------------------------------------------------------------------------ */
static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the menu's and the picker's cards: a puzzle of the size and level, the stars of its first row (the menu's: of
 * its top half), and as many ebony stones as its level ------------------------------------------------------------- */
/* a puzzle on a card S wide: the regions, the solution's stars of its first `rows` rows */
static void paint_card(const sbx_puz *p, const uint8_t *sol, int rows, int S, uint8_t *b)
{
    int n = p->n, f = (int)(S * BENCH_FRAME);
    float q = floorf((S - 2 * f - 0.25f * S / n) / n), o = floorf((S - n * q) / 2);
    int ss = (int)(0.86f * q);
    paint_board(b, S, f, p, q, o, o);
    uint8_t *st = ss > 2 && rows ? malloc((size_t)ss * ss * 4) : NULL;
    if (!st) return;
    paint_star(ss, st);
    for (int c = 0; c < n * n; c++)
        if (sol[c] && c / n < rows)
            art_blend(b, S, S, st, ss, ss, (int)(o + (c % n + 0.5f) * q - ss / 2.0f),
                      (int)(o + (c / n + 0.5f) * q - ss / 2.0f));
    free(st);
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 4 : variant, n = sbx_size_of(v);
    sbx_puz p;
    uint8_t sol[SBX_CELLS];
    sbx_make(n, sbx_stars_of(v), sbx_level_of(v), 2026u + (uint32_t)v, &p, sol);
    int S = variant < 0 || bare ? size : (int)(0.84f * size);
    uint8_t *b = calloc((size_t)S * S, 4);
    if (!b) return;
    paint_card(&p, sol, variant < 0 ? n / 2 : 1, S, b);
    int ox = (size - S) / 2;
    art_blend(rgba, size, size, b, S, S, ox, variant < 0 ? ox : 0);
    free(b);
    if (variant >= 0 && !bare) bench_stones(rgba, size, S, sbx_level_of(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level's regions (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= SBX_CHAPTERS ? SBX_CHAPTERS - 1 : chapter;
    static sbx_game T;
    if (!sbx_open(&T, sbx_levels[(ch + 1) * SBX_STAGES / SBX_CHAPTERS - 1])) return;
    int S = (int)(0.92f * size);
    uint8_t *b = calloc((size_t)S * S, 4);
    if (!b) return;
    paint_card(&T.p, T.sol, T.p.n, S, b);
    art_blend(rgba, size, size, b, S, S, (size - S) / 2, (size - S) / 2);
    free(b);
}

static const game starbattle_game = {
    .id = "starbattle", .order = 416, .variants = SBX_VARIANTS,
    .levels = SBX_STAGES, .chapters = SBX_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {SBX_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .score = score, .level_stars = stars, .fill = 1, .tools = 1,
};
REGISTER_GAME(starbattle_game)
