/*
 * Thermometers in RetroStone Board, for one player: the house plank ruled in cells, glass thermometers laid on it, red
 * lacquer rising in them from the bulbs, the rows' and columns' counts burned along darker bands to the left and on
 * top (the rules: thermometers_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to fill its thermometer up to it (tap a filled cell: emptied from there up); hold it a moment to cross it
 * and the cells above it (a note: empty; again: the cross taken out). Drag along a row or a column to cross a stretch
 * (the cells as the first one was). The shell's tools: what a tap does (fill, or cross: tap it to swap tap and hold),
 * and the hint (a wrong cell put right, else the next cell reasoning finds, in gold light). A count turns red past its
 * filled cells and fades once they are all there. Undo takes back one action, all the way to the start. The bar: the
 * moves. Solved: a wave of gold light runs over the grid; the stars count the hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/thermometers_gen.c:
 * thermometers_levels.h), or its die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/thermometers.h"
#include "games/thermometers_levels.h"

#define BAND 0.72f                              /* the counts' bands, in cells */
#define TUBE 0.2f                               /* a tube's half width, in cells */
#define BULB 0.33f                              /* a bulb's radius */
#define RED_W 0.11f                             /* the red's half width in a tube */

static const uint8_t GLASS[3] = {238, 234, 222}, GLASS_INK[3] = {96, 82, 70}, RED[3] = {176, 44, 34},
                     RED_DARK[3] = {104, 20, 14}, RED_LIGHT[3] = {232, 120, 96}, COUNT[3] = {44, 30, 20};

enum { K_FILL, K_CROSS };                       /* the mode tool: what a tap puts */
enum { T_MODE, T_HINT, TOOLS };

static thm_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int crosses;                             /* taps put crosses (the mode tool) */
static int serial;

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_bulb, *t_cross, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the distance from (x, y) to the segment from (ax, ay) to (bx, by) */
static float seg_dist(float x, float y, float ax, float ay, float bx, float by)
{
    float dx = bx - ax, dy = by - ay, l = dx * dx + dy * dy;
    float t = l > 0 ? clampf(((x - ax) * dx + (y - ay) * dy) / l, 0, 1) : 0;
    return hypotf(x - ax - t * dx, y - ay - t * dy);
}

/* the first `upto` cells of thermometer t as one smooth shape on a picture laid out by g: a tube r wide each side of
 * their centres, a ball rb round the bulb's; filled in fill, its rim (line wide) in ink, a little light on its upper
 * side */
static void paint_shape(const bench_geo *g, const thm_puzzle *p, int t, int upto, float r, float rb,
                        const uint8_t fill[3], const uint8_t ink[3], float line, uint8_t *o)
{
    int n = p->n, q = g->cs;
    float cx[2 * THM_MAX], cy[2 * THM_MAX], x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int k = 0; k < upto; k++) {
        int c = p->cell[t][k];
        cx[k] = g->ox + (c % n + 0.5f) * q;
        cy[k] = g->oy + (c / n + 0.5f) * q;
        x0 = fminf(x0, cx[k]), y0 = fminf(y0, cy[k]), x1 = fmaxf(x1, cx[k]), y1 = fmaxf(y1, cy[k]);
    }
    float m = fmaxf(r, rb) + 2;
    for (int y = (int)fmaxf(0, y0 - m); y < (int)fminf((float)g->h, y1 + m); y++)
        for (int x = (int)fmaxf(0, x0 - m); x < (int)fminf((float)g->w, x1 + m); x++) {
            /* (ux, uy): across the nearest part, -1..1 (the ball: both ways); (hx, hy): where the light glints */
            float px = x + 0.5f, py = y + 0.5f, d = hypotf(px - cx[0], py - cy[0]) - rb;
            float ux = (px - cx[0]) / rb, uy = (py - cy[0]) / rb, hx = -0.35f, hy = -0.35f;
            for (int k = 1; k < upto; k++) {
                float e = seg_dist(px, py, cx[k - 1], cy[k - 1], cx[k], cy[k]) - r;
                if (e < d) {
                    d = e;
                    int down = cx[k] == cx[k - 1];
                    float lo = down ? fminf(cy[k], cy[k - 1]) : fminf(cx[k], cx[k - 1]),
                          hi = down ? fmaxf(cy[k], cy[k - 1]) : fmaxf(cx[k], cx[k - 1]);
                    float jx = down ? cx[k] : clampf(px, lo, hi), jy = down ? clampf(py, lo, hi) : cy[k];
                    ux = (px - jx) / r, uy = (py - jy) / r;   /* past a joint: round it, as the bend's elbow */
                    hx = down ? -0.4f : 0, hy = down ? 0 : -0.4f;
                }
            }
            float cover = clampf(0.5f - d, 0, 1);
            if (cover <= 0) continue;
            /* a round glass rod: darker towards its sides, a bright glint along its upper left */
            float roundness = sqrtf(fmaxf(0, 1 - ux * ux - uy * uy));
            float glint = powf(fmaxf(0, 1 - 3 * hypotf(ux - hx, uy - hy)), 2);
            float rim = clampf(d + line + 0.5f, 0, 1),
                  light = 0.84f + 0.2f * roundness - 0.06f * clampf(ux + uy, -1, 1) + 0.35f * glint;
            uint8_t *pp = o + 4 * ((size_t)y * g->w + x);
            for (int i = 0; i < 3; i++) {
                float body = clampf(fill[i] * light, 0, 255), v = body + (ink[i] - body) * rim;
                pp[i] = (uint8_t)(pp[i] + (v - pp[i]) * cover);
            }
            pp[3] = (uint8_t)fmaxf(pp[3], 255 * cover);
        }
}

/* the thermometers of p on a picture laid out by g: the glass, and the red in the cells of full (may be NULL) */
static void paint_thermos(const bench_geo *g, const thm_puzzle *p, const uint8_t *full, uint8_t *o)
{
    float q = (float)g->cs;
    for (int t = 0; t < p->thermos; t++) {
        paint_shape(g, p, t, p->len[t], TUBE * q, BULB * q, GLASS, GLASS_INK, fmaxf(1, 0.05f * q), o);
        int upto = 0;
        while (full && upto < p->len[t] && full[p->cell[t][upto]]) upto++;
        if (upto)
            paint_shape(g, p, t, upto, RED_W * q, (BULB - 0.08f) * q, RED, RED_DARK, fmaxf(1, 0.03f * q), o);
    }
}

/* a filled bulb (the screen's), a picture size x size */
static void bulb_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    float r = BULB - 0.08f;
    art_ellipse(f, ART_PART, 0.5f, 0.5f, r, r);
    art_fig_paint(o, size, f, RED, RED_DARK, 0.025f, 0.5f);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.42f, 0.41f, 0.07f, 0.05f);
    art_fig_paint(o, size, f, RED_LIGHT, RED_LIGHT, 0, 0);
}

/* a cross: two ink strokes */
static void cross_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    const float a[] = {0.36f, 0.32f, 0.68f, 0.64f, 0.64f, 0.68f, 0.32f, 0.36f};
    const float b[] = {0.64f, 0.32f, 0.68f, 0.36f, 0.36f, 0.68f, 0.32f, 0.64f};
    art_poly(f, ART_PART, a, 4);
    art_poly(f, ART_PART, b, 4);
    art_fig_paint(o, size, f, PUZ_INK, PUZ_INK, 0, 0);
}

static int fit(bench_geo *g, int n, box b)
{
    g->cols = g->rows = n;
    g->lm = g->tm = BAND;
    g->rm = g->bm = 0;
    return bench_fit(g, b);
}

static void paint_board(const bench_geo *g, uint8_t *o)
{
    bench_paint_grid(g, o);
    paint_thermos(g, &G.p, NULL, o);
}

/* the token: a short thermometer half filled on a wooden tile */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    for (int layer = 0; layer < 2; layer++) {
        art_fig *f = art_fig_new();
        float r = layer ? 0.06f : 0.11f, rb = layer ? 0.15f : 0.2f, top = layer ? 0.45f : 0.2f;
        art_rect(f, ART_PART, 0.5f - r, top, 0.5f + r, 0.72f);
        art_ellipse(f, ART_PART, 0.5f, top, r, r);
        art_ellipse(f, ART_PART, 0.5f, 0.72f, rb, rb);
        if (layer) art_fig_paint(b, w, f, RED, RED_DARK, 0.02f, 0.5f);
        else art_fig_paint(b, w, f, GLASS, GLASS_INK, 0.03f, 0.45f);
    }
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
    int changed = fit(&geo, G.p.n, b), key = serial % 4096 * 16 + G.p.n;
    if (changed || made_key != key) {
        made_key = key;
        ui_free(&t_board);
        t_board = bench_tex(&geo, paint_board);
        ui_free(&t_bulb);
        t_bulb = pic_tex(bulb_pic, geo.cs);
        ui_free(&t_cross);
        t_cross = pic_tex(cross_pic, geo.cs);
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
    crosses = 0;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    static thm_puzzle p;
    if (!thm_decode(thm_levels[level < 0 || level >= THM_LEVELS ? 0 : level], &p) || !thm_load(&G, &p))
        thm_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    thm_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return thm_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (thm_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (thm_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = thm_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the finger: what it would do now (a tap fills, a hold crosses, a drag crosses a stretch) ------------------- */
/* the action the finger makes, as it is now: its run (*a, *b) and the state; 0: none */
static int action(int *a, int *b, int *state)
{
    if (!touch.on) return 0;
    int mark = touch.moved ? 1 : bench_holding(&touch) != crosses;
    *a = touch.a;
    *b = touch.moved ? touch.b : touch.a;
    *state = mark ? thm_hold_state(&G, *a) : thm_tap_state(&G, *a);
    return 1;
}

/* ---- the board on the screen, the tools -------------------------------------------------------------------------- */
static void paint_button(int kind, int size, uint8_t *o)   /* what a tap puts: red, or a cross */
{
    if (kind == K_FILL) bulb_pic(size, o);
    else cross_pic(size, o);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = thm_solved(&G), a, b, state;
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    uint16_t st[THM_CELLS];                     /* the grid as the finger would leave it */
    memcpy(st, G.st, sizeof st);
    if (action(&a, &b, &state)) thm_apply(&G.p, st, a, b, state);
    for (int way = 0; way < 2; way++)           /* the counts: red past their cells, faded with all of them */
        for (int i = 0; i < n; i++) {
            int want = G.p.count[way][i], have = 0;
            if (want < 0) continue;
            for (int j = 0; j < n; j++) have += st[way ? j * n + i : i * n + j] == THM_FULL;
            float x = way ? geo.gx + (i + 0.5f) * q : geo.gx - BAND * q / 2;
            float y = way ? geo.gy - BAND * q / 2 : geo.gy + (i + 0.5f) * q;
            ui_number_ex(x, y, 0.56f * q, want, have > want ? BENCH_RED : COUNT, have == want ? 100 : 255,
                         UI_NUM_TIGHT);
        }
    float rw = RED_W * q;
    for (int c = 0; c < n * n; c++) {           /* the red: a bar from the cell below, a bulb */
        if (st[c] != THM_FULL) continue;
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        int p = thm_prev(&G.p, c);
        if (p < 0) {
            float s = bench_pop(&pop, c, 0.2f);
            ui_put_c(t_bulb, mx, my, q * s, q * s);
            continue;
        }
        float px = bench_mid_x(&geo, p), py = bench_mid_y(&geo, p);
        float x0 = fminf(mx, px) - rw, y0 = fminf(my, py) - rw, x1 = fmaxf(mx, px) + rw, y1 = fmaxf(my, py) + rw;
        ui_fill(x0, y0, x1 - x0, y1 - y0, RED[0], RED[1], RED[2], 255);
        if (mx == px) ui_fill(x0 + 0.35f * rw, y0, 0.45f * rw, y1 - y0, RED_LIGHT[0], RED_LIGHT[1], RED_LIGHT[2], 110);
        else ui_fill(x0, y0 + 0.35f * rw, x1 - x0, 0.45f * rw, RED_LIGHT[0], RED_LIGHT[1], RED_LIGHT[2], 110);
    }
    for (int c = 0; c < n * n; c++)
        if (st[c] == THM_CROSS) ui_put_c(t_cross, bench_mid_x(&geo, c), bench_mid_y(&geo, c), (float)q, (float)q);
    bench_touch_ring(&touch, &geo);
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * q);
    bench_wave_cells(&win, &geo, 2.3f);
    ui_tool_paint(T_MODE, paint_button, crosses ? K_CROSS : K_FILL, 0.72f, solved ? UI_DIM : crosses ? UI_ON : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void tool(int t)
{
    if (t == T_MODE) {
        crosses = !crosses;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    int c = thm_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, c);
    bench_flash_start(&pop, c);
    ui_sound(SND_PLACE);
    after(c);
}

static void press(float x, float y)
{
    memset(&touch, 0, sizeof touch);
    if (thm_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { tool(t); return; }
    bench_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { bench_motion(&touch, &geo, x, y, 1); }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int a = touch.a, b = touch.moved ? touch.b : touch.a, kind = bench_release(&touch);
    if (kind == BENCH_NOTHING) return;
    int mark = kind >= BENCH_DRAG ? 1 : (kind == BENCH_HOLD) != crosses;
    int state = mark ? thm_hold_state(&G, a) : thm_tap_state(&G, a);
    if (!thm_act(&G, a, b, state)) return;
    if (state == THM_FULL) bench_flash_start(&pop, G.p.cell[G.p.of[a]][0]);
    ui_sound(state == THM_FULL ? SND_PLACE : SND_TAP);
    after(a);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the pictures of puzzles: the menu's and the picker's cards, the chapters' tabs ----------------------------- *
 * A puzzle with the red of sol (all, or the cells of the first half's thermometers when half), side x side, at the
 * top of the card (centred: mid) */
static void paint_puzzle(const thm_puzzle *p, const uint8_t *sol, int half, int size, int side, int mid, uint8_t *rgba)
{
    bench_geo g;
    memset(&g, 0, sizeof g);
    fit(&g, p->n, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4), full[THM_CELLS];
    if (!b) return;
    int n = p->n, q = g.cs;
    for (int c = 0; c < n * n; c++) full[c] = sol[c] && (!half || p->of[c] < p->thermos / 2);
    bench_paint_grid(&g, b);
    paint_thermos(&g, p, full, b);
    if (q >= 9)
        for (int way = 0; way < 2; way++)
            for (int i = 0; i < n; i++) {
                float x = way ? g.ox + (i + 0.5f) * q : g.ox - BAND * q / 2;
                float y = way ? g.oy - BAND * q / 2 : g.oy + (i + 0.5f) * q;
                puz_digits(b, g.w, g.h, p->count[way][i], x, y, 0.56f * q, COUNT, 1, 1);
            }
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    static thm_puzzle p;
    uint8_t sol[THM_CELLS];
    thm_make(thm_size(v), thm_level(v), 2026u + (uint32_t)v, &p, sol);
    paint_puzzle(&p, sol, variant < 0, size, side, variant < 0, rgba);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, thm_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= THM_CHAPTERS ? THM_CHAPTERS - 1 : chapter;
    static thm_game T;
    static thm_puzzle p;
    if (!thm_decode(thm_levels[(ch + 1) * THM_LEVELS / THM_CHAPTERS - 1], &p) || !thm_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, 0, size, (int)(0.92f * size), 1, rgba);
}

static const game thermometers_game = {
    .id = "thermometers", .order = 420, .variants = THM_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {THM_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = THM_LEVELS, .chapters = THM_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(thermometers_game)
