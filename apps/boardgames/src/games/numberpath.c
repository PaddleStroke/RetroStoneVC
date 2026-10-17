/*
 * Number Path in RetroStone Board, for one player: the house plank ruled in cells, the given numbers burned in dark ink
 * on stained cells, the player's in blue, a faint line joining each number to the next once they touch, in all the
 * room the shell gives (the rules: numberpath_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a number: the end of its run (the numbers in a row touching it) is picked, in gold, the cells where the next
 * number can go dotted; tap one of them to put it there, and on. Or drag from a number through empty cells: the
 * numbers follow the finger (back over the last cell takes it out again), put in when it lifts, one action. From a
 * number whose next is already there, the run goes down (the number before). The eraser (a tool) makes taps and drags
 * take the player's numbers out; the lamp takes out a wrong number, else puts in the next number reasoning finds, in
 * gold light. A number whose next or last is on the board without touching it shows in red. 1 and the last number
 * are ringed. Undo takes back one action, all the way to the start. The bar: the moves. Solved: a wave of gold light;
 * the stars count the hints and the mistakes (bench.h). The shell's level screen chooses a level (300 in six
 * chapters of 50, the two ways in turn, each tile showing its way; tools/numberpath_gen.c), or its die tab a puzzle
 * made at random (a way, a size).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/numberpath.h"
#include "games/pzkit.h"
#include "shell.h"

enum { T_ERASE, T_HINT, TOOLS };

static nph_game G, T;                           /* T: the game as the finger would leave it */
static bench_geo geo;
static bench_flash hint, pop;
static bench_wave win;
static int sel = -1, erasing, serial;
static struct { int on, from, k, moved, last; int run[NPH_CELLS]; } stroke;

static box last_box;
static int made_key = -1, made_small = -1, made_ring = -1;
static SDL_Texture *t_board, *t_small, *t_ring;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void paint_board(const bench_geo *g, uint8_t *o)
{
    bench_paint_grid(g, o);
    for (int c = 0; c < g->cols * g->rows; c++)
        if (G.p.given[c]) bench_stain(g, o, c, BENCH_GIVEN_SHADE);
}

/* a small grid w x h in a picture (its plank, frame and lines), its cells q pixels from (ox, oy) */
static int grid_pic(int w, int h, int size, uint8_t *rgba, int *ox, int *oy)
{
    int f = (int)fmaxf(1, 0.05f * size), q = (size - 2 * f) / (w > h ? w : h);
    int pw = w * q + 2 * f, ph = h * q + 2 * f;
    uint8_t *b = malloc((size_t)pw * ph * 4);
    if (!b || q < 2) {
        free(b);
        return 0;
    }
    bench_plank(b, pw, ph, f);
    float t = fmaxf(1, 0.05f * q);
    for (int i = 1; i < w; i++)
        puz_ink_rect(b, pw, ph, f + i * q - t / 2, (float)f, f + i * q + t / 2, (float)(ph - f), PUZ_INK, 0.45f);
    for (int i = 1; i < h; i++)
        puz_ink_rect(b, pw, ph, (float)f, f + i * q - t / 2, (float)(pw - f), f + i * q + t / 2, PUZ_INK, 0.45f);
    *ox = (size - pw) / 2 + f;
    *oy = (size - ph) / 2 + f;
    art_blend(rgba, size, size, b, pw, ph, (size - pw) / 2, (size - ph) / 2);
    free(b);
    return q;
}

/* a path through cells in a picture: a stroke from centre to centre (cells x + y * w) */
static void path_pic(uint8_t *rgba, int size, int w, int q, int ox, int oy, const int *cells, int n,
                     const uint8_t rgb[3], float wd, float a)
{
    for (int i = 0; i + 1 < n; i++)
        pzk_stroke(rgba, size, size, ox + (cells[i] % w + 0.5f) * q, oy + (cells[i] / w + 0.5f) * q,
                   ox + (cells[i + 1] % w + 0.5f) * q, oy + (cells[i + 1] / w + 0.5f) * q, wd * q / 2, rgb, a);
}

/* the token: a 3 x 3 grid, a snake through it from a ring to a dot */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int ox, oy, q = grid_pic(3, 3, size, rgba, &ox, &oy);
    if (!q) return;
    static const int snake[] = {0, 1, 2, 5, 4, 3, 6, 7, 8};
    path_pic(rgba, size, 3, q, ox, oy, snake, 9, BENCH_BLUE, 0.22f, 0.9f);
    pzk_arc(rgba, size, size, ox + 0.5f * q, oy + 0.5f * q, 0.24f * q, 0.36f * q, 0, 360, BENCH_GIVEN, 1);
    pzk_ball(rgba, size, size, ox + 2.5f * q, oy + 2.5f * q, 0.26f * q, BENCH_GIVEN, 1, 0);
}

static void layout(box b)
{
    last_box = b;
    geo.cols = G.p.w ? G.p.w : 5;
    geo.rows = G.p.h ? G.p.h : 5;
    geo.lm = geo.tm = geo.rm = geo.bm = 0;
    int changed = bench_fit(&geo, b), key = serial % 4096 * 128 + geo.cols * 10 + geo.rows;
    if (changed || made_key != key) {
        made_key = key;
        ui_free(&t_board);
        t_board = bench_tex(&geo, paint_board);
    }
    if (made_ring != geo.cs) {                  /* the ring round 1 and the last number */
        made_ring = geo.cs;
        int s = geo.cs;
        ui_free(&t_ring);
        memset(ui_buf(s, s), 0, (size_t)s * s * 4);
        pzk_arc(ui_buf(s, s), s, s, s / 2.0f, s / 2.0f, 0.36f * s, 0.41f * s, 0, 360, BENCH_GIVEN, 0.85f);
        t_ring = ui_tex(s, s, ui_buf(s, s));
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(&stroke, 0, sizeof stroke);
    hint.at = pop.at = -1;
    win.on = 0;
}

static void begin(void)
{
    serial++;
    calm();
    sel = -1;
    erasing = 0;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    if (!nph_level(&G, level)) nph_new(&G, 0, 1);   /* never: every level is tested */
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    nph_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return nph_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (nph_solved(&G) && !win.on) {
        bench_wave_from(&win, cell < 0 ? 0 : cell, G.p.w, G.p.h);
        sel = -1;
        erasing = 0;
    }
    ui_animate();
}

static int undo(void)
{
    int c = nph_undo(&G);
    if (c < 0) return 0;
    calm();
    if (sel >= 0 && !G.val[sel]) sel = -1;
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = nph_script(&G, moves);
    after(nph_cells(&G.p) / 2);
    return ok;
}

/* the game as the stroke would leave it (T) */
static void preview(void)
{
    T = G;
    if (stroke.on && stroke.k) nph_place(&T, stroke.from, stroke.run, stroke.k);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = nph_cells(&G.p), q = geo.cs, solved = nph_solved(&G), at[NPH_CELLS + 2];
    preview();
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    for (int v = 0; v <= n + 1; v++) at[v] = -1;
    for (int c = 0; c < n; c++)
        if (T.val[c]) at[T.val[c]] = c;
    pzk_rgba line = {BENCH_BLUE[0], BENCH_BLUE[1], BENCH_BLUE[2], 165};
    for (int v = 1; v < n; v++) {               /* the path: a short link across each meeting of a number and the next */
        if (at[v] < 0 || at[v + 1] < 0 || !nph_touch(&T.p, at[v], at[v + 1])) continue;
        float ax = bench_mid_x(&geo, at[v]), ay = bench_mid_y(&geo, at[v]);
        float bx = bench_mid_x(&geo, at[v + 1]), by = bench_mid_y(&geo, at[v + 1]);
        float dx = bx - ax, dy = by - ay, l = hypotf(dx, dy), k = 0.36f * q / l;
        pzk_line(ax + dx * k, ay + dy * k, bx - dx * k, by - dy * k, 0.1f * q, 0.035f * q, line, 0);
    }
    int dir = 0, head = -1;
    if (!solved && !erasing) {                  /* the run's end picked, where its next number can go */
        int from = stroke.on ? (stroke.k ? stroke.run[stroke.k - 1] : stroke.from) : sel;
        head = from >= 0 ? nph_head(&T, from, &dir) : -1;
        if (head >= 0) {
            float x, y;
            bench_cell_xy(&geo, head, &x, &y);
            ui_mark_select(x, y, (float)q, (float)q);
            for (int c = 0; c < n; c++)
                if (!T.val[c] && nph_touch(&T.p, head, c))
                    ui_mark_dot(bench_mid_x(&geo, c), bench_mid_y(&geo, c), 0.22f * q);
        }
    }
    if (hint.at >= 0) bench_glow(&hint, bench_mid_x(&geo, hint.at), bench_mid_y(&geo, hint.at), 2.6f * q);
    for (int c = 0; c < n; c++) {
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        bench_wave_at(&win, c % G.p.w + 0.5f, c / G.p.w + 0.5f, mx, my, 2.3f * q);
        int v = T.val[c];
        if (G.p.given[c] && (v == 1 || v == n)) ui_put_c(t_ring, mx, my, (float)q, (float)q);
        if (!v) continue;
        int given = nph_given(&T, c);
        const uint8_t *ink = given ? BENCH_GIVEN : !solved && nph_clash(&T, c) ? BENCH_RED : BENCH_BLUE;
        float s = bench_pop(&pop, c, 0.35f);
        ui_number_ex(mx, my + 0.02f * q, (v >= 10 ? 0.46f : 0.52f) * q * s, v, ink, 255,
                     given ? UI_NUM_TIGHT : UI_NUM_TIGHT | UI_NUM_REGULAR);
    }
    bench_wave_tick(&win);
    ui_tool(T_ERASE, ICON_ERASER, solved ? UI_DIM : erasing ? UI_ON : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void erase_at(int c)
{
    if (c < 0 || !G.val[c] || nph_given(&G, c)) return;
    if (nph_erase(&G, &c, 1)) ui_sound(SND_TAP);
}

static void press(float x, float y)
{
    memset(&stroke, 0, sizeof stroke);
    if (nph_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t == T_ERASE) {
        erasing = !erasing;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (t == T_HINT) {
        int put, c = nph_hint(&G, &put);
        if (c < 0) {
            ui_sound(SND_DENY);
            return;
        }
        bench_flash_start(&hint, c);
        if (put) {
            bench_flash_start(&pop, c);
            sel = c;
        } else if (sel == c) sel = -1;
        ui_sound(put ? SND_PLACE : SND_TAP);
        after(c);
        return;
    }
    int c = bench_cell_at(&geo, x, y), dir;
    if (c < 0) {
        sel = -1;
        ui_animate();
        return;
    }
    stroke.last = c;
    if (erasing) {
        stroke.on = 1;
        erase_at(c);
        ui_animate();
        return;
    }
    if (G.val[c]) {                             /* a number: a tap picks its run, a drag grows it */
        stroke.on = 1;
        stroke.from = c;
    } else {                                    /* an empty cell: the next number of the run picked */
        int head = sel >= 0 ? nph_head(&G, sel, &dir) : -1;
        if (head < 0 || !nph_touch(&G.p, head, c)) {
            ui_sound(SND_DENY);
            return;
        }
        stroke.on = 1;
        stroke.from = sel;
        stroke.run[stroke.k++] = c;
        stroke.moved = 1;
    }
    ui_animate();
}

static void motion(float x, float y)
{
    if (!stroke.on) return;
    int c = bench_cell_at(&geo, x, y);
    if (c < 0 || c == stroke.last) return;
    stroke.last = c;
    stroke.moved = 1;
    if (erasing) {
        erase_at(c);
        ui_animate();
        return;
    }
    int dir, back = stroke.k >= 2 ? stroke.run[stroke.k - 2] : nph_head(&G, stroke.from, &dir);
    if (stroke.k && c == back) {                /* back over the last cell: it goes */
        stroke.k--;
        ui_animate();
        return;
    }
    preview();
    int from = stroke.k ? stroke.run[stroke.k - 1] : stroke.from, head = nph_head(&T, from, &dir);
    if (head < 0 || T.val[c] || !nph_touch(&T.p, head, c)) return;
    stroke.run[stroke.k++] = c;
    preview();
    int got = 0;
    for (int i = 0; i < stroke.k; i++) got += T.val[stroke.run[i]] != 0;
    if (got < stroke.k) stroke.k--;             /* refused: not a step of the run */
    else ui_sound_soft(SND_PLACE, 0.4f);
    ui_animate();
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!stroke.on) return;
    stroke.on = 0;
    ui_animate();
    if (erasing) return;
    if (!stroke.k) {
        if (G.val[stroke.from]) {               /* a number tapped (or a drag that put nothing): its run picked */
            int dir;
            sel = stroke.from;
            ui_sound(nph_head(&G, sel, &dir) >= 0 ? SND_TAP : SND_DENY);
        }
        return;
    }
    int last = stroke.run[stroke.k - 1];
    if (!nph_place(&G, stroke.from, stroke.run, stroke.k)) {
        ui_sound(SND_DENY);
        return;
    }
    sel = last;
    bench_flash_start(&pop, last);
    ui_sound(SND_PLACE);
    after(last);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the pictures for the shell ---------------------------------------------------------------------------------- */
/* a puzzle in a picture: its grid, the givens burned in (when the cells are big enough), the first `steps` numbers'
 * path drawn */
static void puzzle_pic(const nph_puzzle *p, const uint8_t *sol, int steps, int size, uint8_t *rgba)
{
    int ox, oy, w = p->w, n = w * p->h, q = grid_pic(w, p->h, size, rgba, &ox, &oy), at[NPH_CELLS + 1];
    if (!q) return;
    for (int c = 0; c < n; c++) at[sol[c]] = c;
    if (steps > 1) path_pic(rgba, size, w, q, ox, oy, at + 1, steps, BENCH_BLUE, 0.16f, 0.5f);
    for (int c = 0; q >= 12 && c < n; c++) {
        int v = p->given[c] ? p->given[c] : sol[c] <= steps ? sol[c] : 0;
        if (v) puz_digits(rgba, size, size, v, ox + (c % w + 0.5f) * q, oy + (c / w + 0.52f) * q, 0.46f * q,
                          p->given[c] ? BENCH_GIVEN : BENCH_BLUE, 1, p->given[c] != 0);
    }
}

/* the menu's card: the first level, its run begun; a variant's: a puzzle of its size and way */
static void thumb(int variant, int size, uint8_t *rgba)
{
    nph_puzzle p;
    uint8_t sol[NPH_CELLS];
    if (variant < 0) {
        if (nph_decode(nph_levels[0], &p, sol)) puzzle_pic(&p, sol, 9, size, rgba);
        return;
    }
    int w, h, way;
    nph_variant(variant, &w, &h, &way);
    nph_make(w, h, way, 0.4f, 2026u + (uint32_t)variant, &p, sol);
    puzzle_pic(&p, sol, 0, size, rgba);
}

/* the picker's ways: a 3 x 3 grid, the middle cell joined to the cells it touches */
static void way_pic(int way, int size, uint8_t *rgba)
{
    int ox, oy, q = grid_pic(3, 3, size, rgba, &ox, &oy);
    if (!q) return;
    float cx = ox + 1.5f * q, cy = oy + 1.5f * q;
    for (int c = 0; c < 9; c++) {
        int dx = c % 3 - 1, dy = c / 3 - 1;
        if (c == 4 || (way == NPH_SIDE && dx && dy)) continue;
        pzk_stroke(rgba, size, size, cx, cy, cx + dx * 0.78f * q, cy + dy * 0.78f * q, 0.09f * q, PZK_GOLD, 1);
        pzk_ball(rgba, size, size, cx + dx * q, cy + dy * q, 0.18f * q, BENCH_BLUE, 1, 0.3f);
    }
    pzk_ball(rgba, size, size, cx, cy, 0.24f * q, BENCH_GIVEN, 1, 0.3f);
}

/* the picker's sizes: a grid of the size */
static void size_pic(int i, int size, uint8_t *rgba)
{
    int w, h, way;
    nph_variant(i, &w, &h, &way);
    pzk_grid_icon(rgba, size, w);
}

/* a chapter's tab: a grid of its biggest size */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const int N[NPH_CHAPTERS] = {5, 6, 7, 8, 9, 9};
    pzk_grid_icon(rgba, size, N[chapter < 0 ? 0 : chapter >= NPH_CHAPTERS ? NPH_CHAPTERS - 1 : chapter]);
}

/* a level's tile: its grid, the given numbers' cells stained, the way it touches in gold over it */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    nph_puzzle p;
    uint8_t sol[NPH_CELLS];
    int ox, oy, q;
    if (level < 0 || level >= NPH_LEVELS || !nph_decode(nph_levels[level], &p, sol)) return;
    if (!(q = grid_pic(p.w, p.h, size, rgba, &ox, &oy))) return;
    for (int c = 0; c < nph_cells(&p); c++)
        if (p.given[c]) {
            float x = (float)(ox + c % p.w * q), y = (float)(oy + c / p.w * q);
            bench_shade(rgba, size, size, x + 1, y + 1, x + q - 1, y + q - 1, 0.72f);
        }
    float cx = size / 2.0f, cy = size / 2.0f, r = 0.3f * size;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if ((!dx && !dy) || (p.way == NPH_SIDE && dx && dy)) continue;
            float k = dx && dy ? 0.75f * r : r;
            pzk_stroke(rgba, size, size, cx, cy, cx + dx * k, cy + dy * k, 0.035f * size, PZK_GOLD, 0.95f);
        }
    pzk_ball(rgba, size, size, cx, cy, 0.07f * size, PZK_GOLD, 1, 0.3f);
}

static const game numberpath_game = {
    .id = "numberpath", .order = 461, .variants = NPH_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {2, NPH_SIZES}, .axis_pic = {way_pic, size_pic},
    .levels = NPH_LEVELS, .chapters = NPH_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .level_thumb = level_thumb, .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(numberpath_game)
