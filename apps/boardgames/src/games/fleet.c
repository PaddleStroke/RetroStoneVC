/*
 * Hidden Fleet in RetroStone Board, for one player: the house plank ruled in cells, the rows' and columns' counts
 * burned along darker bands to the left and on top, the fleet to find laid out small along a band below; the ships
 * in gunmetal, each cell drawn as the part it is once its neighbours tell (a one-cell ship round, an end rounded
 * towards the sea, a middle square to its neighbours; a ship cell not yet told a smaller block), lapis waves on the
 * water; the cells shown at the start stained darker (the rules: fleet_rules.c; what the logic grids share: bench.h,
 * bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell to put a ship cell (again: take it off); hold it a moment to put water instead (a note). Drag along a
 * row or a column to put water in the cells of the stretch that were as the first one (or take it out of a stretch
 * of water). The keys: the arrows move a frame, Enter puts a ship cell, Space water, Tab (or I) gives a hint. The
 * shell's tools: what a tap does (a ship cell, or water: tap it to swap tap and hold), and the hint (a wrong cell put
 * right, else the next cell reasoning finds, in gold light). Ship cells touching at a corner, or in a ship longer
 * than the fleet's longest, glow red; a count turns red past its ship cells and fades once they are all there; a
 * ship of the fleet fades in the band once one of its length lies closed in water. Undo takes back one action, all
 * the way to the start. The bar: the moves. Solved: a wave of gold light runs over the sea; the stars count the hints
 * and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/fleet_gen.c: fleet_levels.h), or its
 * die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/fleet.h"
#include "games/fleet_levels.h"

#define BAND 0.72f                              /* the counts' bands, in cells */
#define TRAY 1.0f                               /* the fleet's band below, in cells */

static const uint8_t HULL[3] = {96, 104, 112}, HULL_INK[3] = {28, 32, 38}, WAVE[3] = {52, 84, 150},
                     COUNT[3] = {44, 30, 20};

enum { K_SHIP, K_WATER };                       /* the mode tool: what a tap puts */
enum { T_MODE, T_HINT, TOOLS };
/* the ship parts drawn: a block (not told yet), a one-cell ship, the ends (rounded up, down, left, right), the
 * middles (across, down) */
enum { P_BLOCK, P_ONE, P_UP, P_DOWN, P_LEFT, P_RIGHT, P_ACROSS, P_TALL, PARTS };

static hfl_game G;
static bench_geo geo;
static bench_touch touch;
static bench_flash hint, pop;
static bench_wave win;
static int water;                               /* taps put water (the mode tool) */
static int cur = -1;                            /* the keys' frame (a cell), -1: not shown */
static int serial;

static box last_box;
static int made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_part[PARTS], *t_wave, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a ship's part as a figure in the unit square */
static void part_fig(art_fig *f, int part)
{
    float pts[64];
    int np = 0;
    if (part == P_BLOCK) {
        art_rect(f, ART_PART, 0.17f, 0.17f, 0.83f, 0.83f);
        return;
    }
    if (part == P_ONE) {
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.36f, 0.36f);
        return;
    }
    if (part == P_ACROSS) {
        art_rect(f, ART_PART, 0.0f, 0.14f, 1.0f, 0.86f);
        return;
    }
    if (part == P_TALL) {
        art_rect(f, ART_PART, 0.14f, 0.0f, 0.86f, 1.0f);
        return;
    }
    /* an end, rounded towards the way it points (P_RIGHT: its ship lies to the left, the bow to the right) */
    pts[np++] = 0.0f, pts[np++] = 0.14f;
    for (int i = 0; i <= 12; i++) {
        float a = -PUZ_PI / 2 + PUZ_PI * i / 12;
        pts[np++] = 0.46f + 0.36f * cosf(a), pts[np++] = 0.5f + 0.36f * sinf(a);
    }
    pts[np++] = 0.0f, pts[np++] = 0.86f;
    for (int i = 0; i < np; i += 2) {           /* turned to point its way */
        float x = pts[i], y = pts[i + 1];
        if (part == P_LEFT) pts[i] = 1 - x;
        else if (part == P_DOWN) pts[i] = 1 - y, pts[i + 1] = x;
        else if (part == P_UP) pts[i] = y, pts[i + 1] = 1 - x;
    }
    art_poly(f, ART_PART, pts, np / 2);
}

static void part_pic(int part, int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    part_fig(f, part);
    art_fig_paint(o, size, f, HULL, HULL_INK, 0.035f, 0.45f);
}

/* water: two waves of lapis */
static void wave_pic(int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    for (int w = 0; w < 2; w++) {
        float pts[48], y0 = 0.42f + 0.18f * w;
        int np = 0;
        for (int i = 0; i <= 10; i++) {
            float x = 0.27f + 0.46f * i / 10;
            pts[np++] = x, pts[np++] = y0 + 0.05f * sinf(i * PUZ_PI / 5);
        }
        for (int i = 10; i >= 0; i--) {
            float x = 0.27f + 0.46f * i / 10;
            pts[np++] = x, pts[np++] = y0 + 0.05f * sinf(i * PUZ_PI / 5) + 0.05f;
        }
        art_poly(f, ART_PART, pts, np / 2);
    }
    art_fig_paint(o, size, f, WAVE, WAVE, 0, 0);
}

/* a picture made by paint(size, rgba) laid over another (w x h) at (x, y), size across */
static void pic_into(uint8_t *o, int w, int h, void (*paint)(int, uint8_t *), float x, float y, int size)
{
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    paint(size, b);
    art_blend(o, w, h, b, size, size, (int)lroundf(x), (int)lroundf(y));
    free(b);
}

static void part_into(uint8_t *o, int w, int h, int part, float x, float y, int size)
{
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    part_pic(part, size, b);
    art_blend(o, w, h, b, size, size, (int)lroundf(x), (int)lroundf(y));
    free(b);
}

/* the part a shown cell, or a cell of the solution, is drawn as */
static int shown_part(int s)
{
    static const int8_t P[] = {P_BLOCK, P_BLOCK, P_ONE, P_UP, P_DOWN, P_LEFT, P_RIGHT, P_BLOCK};
    return P[s < 0 || s > HFL_MIDDLE ? 0 : s];
}

static int fit(bench_geo *g, int n, box b)
{
    g->cols = g->rows = n;
    g->lm = g->tm = BAND;
    g->rm = 0;
    g->bm = TRAY;
    return bench_fit(g, b);
}

/* the plank with its bands, the shown cells stained, their parts (a middle as it lies) or their water */
static void paint_sea(const bench_geo *g, const hfl_puzzle *p, uint8_t *o)
{
    bench_paint_grid(g, o);
    int n = p->n, q = g->cs;
    float W = (float)n * q;
    bench_shade(o, g->w, g->h, g->ox, g->oy + W + 0.12f * q, g->ox + W, g->oy + W + TRAY * q, 0.86f);
    for (int c = 0; c < n * n; c++) {
        int s = p->given[c];
        if (!s) continue;
        float x = g->ox + c % n * q, y = g->oy + c / n * q;
        bench_stain(g, o, c, BENCH_GIVEN_SHADE);
        if (s == HFL_SEA) {
            pic_into(o, g->w, g->h, wave_pic, x, y, q);
            continue;
        }
        int part = shown_part(s);
        if (s == HFL_MIDDLE) {                  /* as it lies, when the cells beside it tell */
            int x0 = c % n, y0 = c / n;
            int across = (x0 > 0 && p->given[c - 1] > HFL_SEA) || (x0 < n - 1 && p->given[c + 1] > HFL_SEA) ||
                         (y0 > 0 && p->given[c - n] == HFL_SEA) || y0 == 0 || y0 == n - 1;
            int down = (y0 > 0 && p->given[c - n] > HFL_SEA) || (y0 < n - 1 && p->given[c + n] > HFL_SEA) ||
                       (x0 > 0 && p->given[c - 1] == HFL_SEA) || x0 == 0 || x0 == n - 1;
            part = across && !down ? P_ACROSS : down && !across ? P_TALL : P_BLOCK;
        }
        part_into(o, g->w, g->h, part, x, y, q);
    }
}

static void paint_board(const bench_geo *g, uint8_t *o) { paint_sea(g, &G.p, o); }

/* the token: a ship of three on a wooden tile, a wave under it */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2, d = w / 3;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    part_into(b, w, w, P_LEFT, 0, w * 0.18f, d);
    part_into(b, w, w, P_ACROSS, (float)d, w * 0.18f, d);
    part_into(b, w, w, P_RIGHT, 2.0f * d, w * 0.18f, d);
    pic_into(b, w, w, wave_pic, w * 0.25f, w * 0.48f, w / 2);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static SDL_Texture *part_tex(int part, int q)
{
    memset(ui_buf(q, q), 0, (size_t)q * q * 4);
    part_pic(part, q, ui_buf(q, q));
    return ui_tex(q, q, ui_buf(q, q));
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
        for (int i = 0; i < PARTS; i++) {
            ui_free(&t_part[i]);
            t_part[i] = part_tex(i, geo.cs);
        }
        ui_free(&t_wave);
        t_wave = pic_tex(wave_pic, geo.cs);
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
    water = 0;
    cur = -1;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    hfl_puzzle p;
    if (!hfl_decode(hfl_levels[level < 0 || level >= HFL_LEVELS ? 0 : level], &p) || !hfl_load(&G, &p))
        hfl_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    hfl_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return hfl_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (hfl_solved(&G) && !win.on) {
        bench_wave_from(&win, cell, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (hfl_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = hfl_script(&G, moves);
    after(G.p.n * G.p.n / 2);
    return ok;
}

/* ---- the marks: a tap a ship cell, a hold water, a drag water ---------------------------------------------------- */
static int st_of(int c) { return G.st[c]; }
static int free_of(int c) { return hfl_free(&G, c); }
static int tap_of(int c) { return hfl_tap_state(&G, c); }
static int hold_of(int c) { return hfl_hold_state(&G, c); }
static int paint(const int *cells, int n, int state) { return hfl_paint(&G, cells, n, state); }
static const bench_marks marks = {st_of, free_of, tap_of, hold_of, paint, NULL, 1};

/* ---- the board on the screen, the tools -------------------------------------------------------------------------- */
static void paint_button(int kind, int size, uint8_t *o)   /* what a tap puts: a ship cell, or water */
{
    if (kind == K_SHIP) part_pic(P_ONE, size, o);
    else wave_pic(size, o);
}

/* the part a player's ship cell is drawn as: what its neighbours (ship cells, water or the rim) tell so far */
static int cell_part(int c)
{
    int n = G.p.n, ship[4], sea[4];
    static const int8_t DX[4] = {0, 0, -1, 1}, DY[4] = {-1, 1, 0, 0};
    for (int d = 0; d < 4; d++) {
        int x = c % n + DX[d], y = c / n + DY[d], o = y * n + x, in = x >= 0 && y >= 0 && x < n && y < n;
        ship[d] = in && G.st[o] == HFL_SHIP;
        sea[d] = !in || G.st[o] == HFL_WATER;
    }
    int ships = ship[0] + ship[1] + ship[2] + ship[3], fleet[HFL_LONG + 1], longest = 1, len = 1;
    hfl_fleet(n, fleet);
    for (int l = 1; l <= HFL_LONG; l++)
        if (fleet[l]) longest = l;
    for (int d = 0; d < 4; d++)                 /* a ship as long as the longest: closed at both ends */
        for (int x = c % n + DX[d], y = c / n + DY[d]; ship[d] && x >= 0 && y >= 0 && x < n && y < n &&
                                                       G.st[y * n + x] == HFL_SHIP;
             x += DX[d], y += DY[d])
            len++;
    if (len >= longest && ships == 1)
        for (int d = 0; d < 4; d++) sea[d] = !ship[d];
    if (!ships) return sea[0] && sea[1] && sea[2] && sea[3] ? P_ONE : P_BLOCK;
    if (ships == 2 && ship[0] && ship[1]) return P_TALL;
    if (ships == 2 && ship[2] && ship[3]) return P_ACROSS;
    if (ships > 1) return P_BLOCK;
    if (ship[1]) return sea[0] ? P_UP : P_TALL;  /* its ship below: rounded up once water or the rim is above */
    if (ship[0]) return sea[1] ? P_DOWN : P_TALL;
    if (ship[3]) return sea[2] ? P_LEFT : P_ACROSS;
    return sea[3] ? P_RIGHT : P_ACROSS;
}

/* the fleet's band: each ship small (its length in parts), those found faded */
static void draw_fleet(void)
{
    int n = G.p.n, q = geo.cs, fleet[HFL_LONG + 1], found[HFL_LONG + 1], units = 0, ships = hfl_fleet(n, fleet);
    hfl_found(&G, found);
    for (int l = 1; l <= HFL_LONG; l++) units += l * fleet[l];
    float span = (float)n * q, u = fminf(0.55f * q, span / (units + 0.5f * (ships - 1))), gap = 0.5f * u;
    float x = geo.gx + (span - (units * u + (ships - 1) * gap)) / 2, y = geo.gy + span + (0.12f + TRAY) * q / 2;
    for (int l = HFL_LONG; l >= 1; l--)
        for (int i = 0; i < fleet[l]; i++) {
            Uint8 a = i < found[l] ? 70 : 255;
            for (int j = 0; j < l; j++) {
                int part = l == 1 ? P_ONE : j == 0 ? P_LEFT : j == l - 1 ? P_RIGHT : P_ACROSS;
                SDL_SetTextureAlphaMod(t_part[part], a);
                ui_put_c(t_part[part], x + (j + 0.5f) * u, y, u, u);
                SDL_SetTextureAlphaMod(t_part[part], 255);
            }
            x += l * u + gap;
        }
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = hfl_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    static uint16_t real[HFL_CELLS];
    bench_marks_show(&marks, &touch, &geo, water, G.st, real, n * n);   /* the sea as the finger would leave it */
    for (int way = 0; way < 2; way++)           /* the counts: red past their ship cells, faded with all of them */
        for (int i = 0; i < n; i++) {
            int want = way ? G.p.col[i] : G.p.row[i], have = hfl_line(&G, way, i);
            float x = way ? geo.gx + (i + 0.5f) * q : geo.gx - BAND * q / 2;
            float y = way ? geo.gy - BAND * q / 2 : geo.gy + (i + 0.5f) * q;
            ui_number_ex(x, y, 0.56f * q, want, have > want && !solved ? BENCH_RED : COUNT, have == want ? 100 : 255,
                         UI_NUM_TIGHT);
        }
    for (int c = 0; c < n * n; c++) {
        if (G.p.given[c]) continue;             /* drawn on the plank */
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        if (G.st[c] == HFL_WATER) ui_put_c(t_wave, mx, my, (float)q, (float)q);
        if (G.st[c] != HFL_SHIP) continue;
        float s = bench_pop(&pop, c, 0.3f);
        ui_put_c(t_part[cell_part(c)], mx, my, q * s, q * s);
    }
    for (int c = 0; c < n * n && !solved; c++)  /* the faults */
        if (hfl_clash(&G, c)) ui_mark_glow(bench_mid_x(&geo, c), bench_mid_y(&geo, c), 1.25f * q);
    draw_fleet();
    bench_marks_back(G.st, real, n * n);
    bench_touch_ring(&touch, &geo);
    if (cur >= 0 && !solved) {
        float x, y;
        bench_cell_xy(&geo, cur, &x, &y);
        ui_mark_select(x, y, (float)q, (float)q);
    }
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * q);
    bench_wave_cells(&win, &geo, 2.3f);
    ui_tool_paint(T_MODE, paint_button, water ? K_WATER : K_SHIP, 0.72f, solved ? UI_DIM : water ? UI_ON : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer, the keys --------------------------------------------------------------------------------------- */
static void give_hint(void)
{
    int c = hfl_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, c);
    bench_flash_start(&pop, c);
    ui_sound(SND_PLACE);
    after(c);
}

static void tool(int t)
{
    if (t == T_MODE) {
        water = !water;
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
    if (hfl_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { tool(t); return; }
    bench_press(&touch, &geo, x, y);
}

static void motion(float x, float y) { bench_motion(&touch, &geo, x, y, 1); }

static void done_at(int a, int to)
{
    if (to == HFL_SHIP) bench_flash_start(&pop, a);
    ui_sound(to == HFL_SHIP ? SND_PLACE : SND_TAP);
    after(a);
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int to, a = bench_marks_release(&marks, &touch, &geo, water, &to);
    if (a >= 0) done_at(a, to);
}

static void key(SDL_Keycode k)
{
    int n = G.p.n;
    if (hfl_solved(&G) || !n) return;
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
    if (!hfl_free(&G, cur)) { ui_sound(SND_DENY); return; }
    int mark = (k == SDLK_SPACE) != water, to = mark ? hfl_hold_state(&G, cur) : hfl_tap_state(&G, cur);
    if (hfl_paint(&G, &cur, 1, to)) done_at(cur, to);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the pictures of puzzles: the menu's and the picker's cards, the chapters' tabs ----------------------------- *
 * A sea with the ships of sol (all, or those of its first half when half), side x side, at the top of the card
 * (centred: mid) */
static void paint_puzzle(const hfl_puzzle *p, const uint8_t *sol, int half, int size, int side, int mid,
                         uint8_t *rgba)
{
    bench_geo g;
    memset(&g, 0, sizeof g);
    fit(&g, p->n, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4);
    if (!b) return;
    paint_sea(&g, p, b);
    int n = p->n, q = g.cs;
    for (int c = 0; c < n * n; c++) {
        if (!sol[c] || p->given[c] || (half && c >= n * n / 2)) continue;
        int s = hfl_part(sol, n, c), part = shown_part(s);
        if (s == HFL_MIDDLE) part = (c % n > 0 && sol[c - 1]) ? P_ACROSS : P_TALL;
        part_into(b, g.w, g.h, part, g.ox + c % n * q, g.oy + c / n * q, q);
    }
    if (q >= 9)
        for (int way = 0; way < 2; way++)
            for (int i = 0; i < n; i++) {
                float x = way ? g.ox + (i + 0.5f) * q : g.ox - BAND * q / 2;
                float y = way ? g.oy - BAND * q / 2 : g.oy + (i + 0.5f) * q;
                puz_digits(b, g.w, g.h, way ? p->col[i] : p->row[i], x, y, 0.56f * q, COUNT, 1, 1);
            }
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
}

/* the level of a size (its k-th, else the nearest): its index */
static int level_of(int n, int k)
{
    int first = -1, last = -1;
    for (int l = 0; l < HFL_LEVELS; l++) {
        int m = atoi(hfl_levels[l]);
        if (m == n && first < 0) first = l;
        if (m == n) last = l;
        if (m > n && first < 0) return l;
    }
    if (first < 0) return HFL_LEVELS - 1;
    return first + k > last ? last : first + k;
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    hfl_puzzle p;
    static hfl_game T;
    int l = variant < 0 ? 12 : level_of(hfl_size(v), 4 + 9 * hfl_level(v));
    if (!hfl_decode(hfl_levels[l], &p) || !hfl_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, variant < 0, size, side, variant < 0 || bare, rgba);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, hfl_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a sea of the size, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= HFL_CHAPTERS ? HFL_CHAPTERS - 1 : chapter;
    static hfl_game T;
    hfl_puzzle p;
    if (!hfl_decode(hfl_levels[(ch + 1) * HFL_LEVELS / HFL_CHAPTERS - 1], &p) || !hfl_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, 0, size, (int)(0.92f * size), 1, rgba);
}

static const game fleet_game = {
    .id = "fleet", .order = 433, .variants = HFL_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .key = key, .players = 1u << 1,
    .axes = {HFL_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = HFL_LEVELS, .chapters = HFL_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(fleet_game)
