/*
 * The puzzle bench on the screen: what the logic grids share there (Sudoku, Nonograms, Light Up, Binairo, Tents,
 * Skyscrapers, Futoshiki, Rectangles, Star Battle, Dominosa, Calcudoku, Kakuro, Bridges, Loop). The rules' side is
 * bench.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only, so a shared .c file would
 * be left out of one game's build or another's.
 *
 * The games take the whole room the shell gives (game.fill) and leave their buttons to the shell (game.tools: the
 * hint's lamp, the pencil, the eraser, the eye, a mode; drawn with ui_tool, found with ui_tool_hit). Here:
 * - the wood: a ruled plank fitted to its grid in the room, with room round the grid for the clues (bench_geo), the
 *   house plank at any width and height, wooden tiles in several tones, a tile cut from the plank and bevelled;
 * - the finger on a grid of cells: a tap, a press held (a mark), a drag along a row or a column (bench_touch); the
 *   marks' grids (Light Up, Tents, Binairo, Nonograms) on top of it: what a tap, a hold or a drag makes of the cells,
 *   shown as the finger goes (bench_marks);
 * - the lights: a hint's gold light, a piece set down settling, the win's wave of gold light;
 * - the bar: the moves;
 * - the digit grids (Sudoku, Futoshiki, Calcudoku, Kakuro; Skyscrapers' keys): the board with a row of wooden digit
 *   keys under it (bench_pad); and the whole of their play (bench_dg): a cell picked, a key tapped or carried to a
 *   cell, the tools (the pencil of the notes, the eraser, the hint, the eye of the clashes), given and entered digits
 *   drawn (a clue's in dark ink on a stained cell, the player's in blue);
 * - the pictures for the shell: the level's ebony stones under a picker's card, the bar's small picture.
 */
#ifndef BENCH_UI_H
#define BENCH_UI_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench.h"
#include "games/puzzles.h"
#include "shell.h"

#define BENCH_FRAME 0.035f                      /* a plank's walnut frame, a part of its short side */
#define BENCH_HOLD_MS 420                       /* a press held this long: a mark */
#define BENCH_HINT_MS 1500                      /* a hint's gold light */
#define BENCH_POP_MS 200                        /* a piece set down settles from a little larger */
#define BENCH_WAVE_STEP 50                      /* the win's wave: ms a cell further on */
#define BENCH_WAVE_LEN 560
#define BENCH_GIVEN_SHADE 0.82f                 /* a clue's cell: its wood stained this much darker */

static const uint8_t BENCH_RED[3] = {184, 44, 34}, BENCH_BLUE[3] = {30, 64, 140}, BENCH_GIVEN[3] = {40, 34, 30},
                     BENCH_NOTE[3] = {92, 70, 50};

/* ---- the wood ---------------------------------------------------------------------------------------------------- */
/* the house plank in its walnut frame (fr pixels), w x h: a square board's picture with its middle rows or columns
 * left out (the grain runs across, so the seam does not show) */
static inline void bench_plank(uint8_t *o, int w, int h, int fr)
{
    int s = w > h ? w : h;
    if (w == h) {
        art_board_n(1, 1, s - 2 * fr, fr, 0, o);
        return;
    }
    uint8_t *sq = malloc((size_t)s * s * 4);
    if (!sq) return;
    art_board_n(1, 1, s - 2 * fr, fr, 0, sq);
    for (int y = 0; y < h; y++) {
        int sy = y < h / 2 ? y : s - h + y;
        for (int x = 0; x < w; x++) {
            int sx = x < w / 2 ? x : s - w + x;
            memcpy(o + 4 * ((size_t)y * w + x), sq + 4 * ((size_t)sy * s + sx), 4);
        }
    }
    free(sq);
}

/* the wood darkened by k in a rectangle of a picture (w x h): a band for the clues, a cell stained */
static inline void bench_shade(uint8_t *o, int w, int h, float x0, float y0, float x1, float y1, float k)
{
    for (int y = (int)fmaxf(0, y0); y < (int)fminf((float)h, y1); y++)
        for (int x = (int)fmaxf(0, x0); x < (int)fminf((float)w, x1); x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
        }
}

/* the woods' tones, factors on the house plank (the grain stays): tiles, regions */
#define BENCH_TONES 6
static const float BENCH_TONE[BENCH_TONES][3] = {
    {1.13f, 1.11f, 1.07f},                      /* maple */
    {1.06f, 0.93f, 0.70f},                      /* honey */
    {1.04f, 0.80f, 0.68f},                      /* cherry */
    {1.10f, 1.00f, 0.87f},                      /* beech */
    {0.99f, 1.02f, 0.86f},                      /* olive ash */
    {0.93f, 0.74f, 0.64f},                      /* padauk, faded */
};

/* a wooden tile, w x h: the plank tinted (tone: a factor per channel, the grain stays), rounded (rad) and bevelled */
static inline void bench_tile(uint8_t *o, int w, int h, float rad, const float tone[3])
{
    int s = w > h ? w : h;
    uint8_t *plank = malloc((size_t)s * s * 4);
    if (!plank) return;
    art_board_n(1, 1, s, 0, 0, plank);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x), *q = plank + 4 * ((size_t)y * s + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(q[i] * tone[i], 0, 255);
            p[3] = 255;
        }
    free(plank);
    puz_bevel(o, w, h, rad, fmaxf(2, 0.1f * (w < h ? w : h)), 0.34f);
}

/* a tile cut from the plank as it is (a key, a tower's top), w x h: rounded (rad), bevelled over bevel pixels */
static inline void bench_cut(uint8_t *o, int w, int h, float rad, float bevel, float depth)
{
    int q = w > h ? w : h;
    uint8_t *plank = malloc((size_t)q * q * 4);
    if (!plank) return;
    art_board_n(1, 1, q, 0, 0, plank);
    for (int y = 0; y < h; y++) memcpy(o + 4 * (size_t)y * w, plank + 4 * (size_t)y * q, (size_t)w * 4);
    free(plank);
    puz_bevel(o, w, h, rad, bevel, depth);
}

/* ---- the plank and its grid ------------------------------------------------------------------------------------ */
typedef struct {
    int cols, rows;
    float lm, tm, rm, bm;                       /* room round the grid for the clues, in cells (left, top...) */
    float bx, by;                               /* the plank on the screen */
    int w, h, fr, cs;                           /* its size, its frame, a cell (pixels) */
    float ox, oy;                               /* the grid's corner in the plank's picture */
    float gx, gy;                               /* the grid's corner on the screen */
} bench_geo;

/* the plank fitted to its grid (and the clues' room) as big as the box allows, in the box's middle; 1: its size
 * changed (its pictures are to be made again) */
static inline int bench_fit(bench_geo *g, box b)
{
    float S = fminf(b.w, b.h), cw = g->cols + g->lm + g->rm, ch = g->rows + g->tm + g->bm;
    int fr = (int)(S * BENCH_FRAME), m = fr + (int)(0.015f * S), was_w = g->w, was_h = g->h, was = g->cs;
    if (cw < 1 || ch < 1) return 0;             /* no puzzle yet */
    g->cs = (int)fminf((b.w - 2 * m) / cw, (b.h - 2 * m) / ch);
    g->fr = fr;
    g->w = (int)(cw * g->cs) + 2 * m;
    g->h = (int)(ch * g->cs) + 2 * m;
    g->bx = floorf(b.x + (b.w - g->w) / 2);
    g->by = floorf(b.y + (b.h - g->h) / 2);
    g->ox = m + floorf(g->lm * g->cs);
    g->oy = m + floorf(g->tm * g->cs);
    g->gx = g->bx + g->ox;
    g->gy = g->by + g->oy;
    return g->cs != was || g->w != was_w || g->h != was_h;
}

/* the plank told to the shell as all the board takes (ui_board_used; from layout()) */
static inline void bench_used(const bench_geo *g) { ui_board_used(g->bx, g->by, (float)g->w, (float)g->h); }

/* the plank's picture (w x h): the house wood in its frame, the clue bands a shade darker, the grid ruled (its rim
 * thicker) */
static inline void bench_paint_grid(const bench_geo *g, uint8_t *o)
{
    int W0 = g->w, H0 = g->h, q = g->cs;
    float W = (float)g->cols * q, H = (float)g->rows * q, ox = g->ox, oy = g->oy;
    bench_plank(o, W0, H0, g->fr);
    bench_shade(o, W0, H0, ox - g->lm * q, oy, ox, oy + H, 0.86f);
    bench_shade(o, W0, H0, ox + W, oy, ox + W + g->rm * q, oy + H, 0.86f);
    bench_shade(o, W0, H0, ox, oy - g->tm * q, ox + W, oy, 0.86f);
    bench_shade(o, W0, H0, ox, oy + H, ox + W, oy + H + g->bm * q, 0.86f);
    for (int way = 0; way < 2; way++)
        for (int i = 0, n = way ? g->rows : g->cols; i <= n; i++) {
            int rim = i == 0 || i == n;
            float t = rim ? fmaxf(2, 0.07f * q) : fmaxf(1, 0.028f * q), p = (float)i * q;
            float lo = p - t / 2, hi = p + t / 2;
            if (i == 0) lo = 0, hi = t;
            if (i == n) lo = p - t, hi = p;
            if (way) puz_ink_rect(o, W0, H0, ox, oy + lo, ox + W, oy + hi, PUZ_INK, rim ? 0.85f : 0.45f);
            else puz_ink_rect(o, W0, H0, ox + lo, oy, ox + hi, oy + H, PUZ_INK, rim ? 0.85f : 0.45f);
        }
}

/* a cell of the plank's picture stained darker (a clue's: a given digit, a given disc) */
static inline void bench_stain(const bench_geo *g, uint8_t *o, int c, float k)
{
    float in = fmaxf(1, 0.04f * g->cs), x = g->ox + c % g->cols * g->cs, y = g->oy + c / g->cols * g->cs;
    bench_shade(o, g->w, g->h, x + in, y + in, x + g->cs - in, y + g->cs - in, k);
}

static inline void bench_cell_xy(const bench_geo *g, int c, float *x, float *y)   /* a cell's top left corner */
{
    *x = g->gx + (float)(c % g->cols) * g->cs;
    *y = g->gy + (float)(c / g->cols) * g->cs;
}

static inline float bench_mid_x(const bench_geo *g, int c) { return g->gx + (c % g->cols + 0.5f) * g->cs; }
static inline float bench_mid_y(const bench_geo *g, int c) { return g->gy + (c / g->cols + 0.5f) * g->cs; }

static inline int bench_cell_at(const bench_geo *g, float x, float y)
{
    float ix = x - g->gx, iy = y - g->gy;
    if (ix < 0 || iy < 0 || ix >= g->cols * g->cs || iy >= g->rows * g->cs) return -1;
    return (int)(iy / g->cs) * g->cols + (int)(ix / g->cs);
}

/* the plank's picture made (paint: into a w x h picture) as a texture */
static inline SDL_Texture *bench_tex(const bench_geo *g, void (*paint)(const bench_geo *g, uint8_t *o))
{
    uint8_t *o = ui_buf(g->w, g->h);
    paint(g, o);
    return ui_tex(g->w, g->h, o);
}

/* ---- the finger on the cells: a tap, a press held (a mark), a drag along a row or a column -------------------- */
enum { BENCH_NOTHING, BENCH_TAP, BENCH_HOLD, BENCH_DRAG, BENCH_HOLD_DRAG };
typedef struct {
    int on, a, b, moved, held;                  /* a: the cell pressed; b: the drag's end, in a's row or column */
    float x0, y0;
    Uint32 t0;
} bench_touch;

static inline int bench_press(bench_touch *t, const bench_geo *g, float x, float y)
{
    memset(t, 0, sizeof *t);
    int c = bench_cell_at(g, x, y);
    if (c < 0) return -1;
    t->on = 1;
    t->a = t->b = c;
    t->x0 = x;
    t->y0 = y;
    t->t0 = ui_ms();
    ui_animate();
    return c;
}

/* the finger moves: a drag once it has gone far enough (if runs: drags allowed), to the cell along a's row or column
 * the finger has gone further along */
static inline void bench_motion(bench_touch *t, const bench_geo *g, float x, float y, int runs)
{
    if (!t->on) return;
    if (!t->moved && runs && hypotf(x - t->x0, y - t->y0) > 0.35f * g->cs) {
        t->held = ui_ms() - t->t0 >= BENCH_HOLD_MS;
        t->moved = 1;
    }
    if (t->moved) {
        int ax = t->a % g->cols, ay = t->a / g->cols;
        int bx = (int)floorf((x - g->gx) / g->cs), by = (int)floorf((y - g->gy) / g->cs);
        bx = bx < 0 ? 0 : bx >= g->cols ? g->cols - 1 : bx;
        by = by < 0 ? 0 : by >= g->rows ? g->rows - 1 : by;
        t->b = abs(bx - ax) >= abs(by - ay) ? ay * g->cols + bx : by * g->cols + ax;
    }
    ui_animate();
}

/* 1: the press has been held long enough to mark (and has not moved) */
static inline int bench_holding(const bench_touch *t) { return t->on && !t->moved && ui_ms() - t->t0 >= BENCH_HOLD_MS; }

/* the finger lifted: what it did (BENCH_NOTHING .. BENCH_HOLD_DRAG) */
static inline int bench_release(bench_touch *t)
{
    if (!t->on) return BENCH_NOTHING;
    int held = ui_ms() - t->t0 >= BENCH_HOLD_MS;
    t->on = 0;
    ui_animate();
    return t->moved ? (t->held ? BENCH_HOLD_DRAG : BENCH_DRAG) : held ? BENCH_HOLD : BENCH_TAP;
}

/* the cells from a to b (in a row or a column), a first; how many */
static inline int bench_run(const bench_geo *g, int a, int b, int *out)
{
    int n = 0, w = g->cols, dx = (b % w > a % w) - (b % w < a % w), dy = (b / w > a / w) - (b / w < a / w);
    for (int c = a;; c += dy * w + dx) {
        out[n++] = c;
        if (c == b) break;
    }
    return n;
}

static inline int bench_in_run(const bench_geo *g, int c, int a, int b)
{
    int w = g->cols, x = c % w, y = c / w, ax = a % w, ay = a / w, bx = b % w, by = b / w;
    return x >= (ax < bx ? ax : bx) && x <= (ax < bx ? bx : ax) && y >= (ay < by ? ay : by) && y <= (ay < by ? by : ay);
}

/* a press growing into a mark: a ring swelling round the cell */
static inline void bench_touch_ring(const bench_touch *t, const bench_geo *g)
{
    if (!t->on || t->moved || bench_holding(t)) return;
    float f = (ui_ms() - t->t0) / (float)BENCH_HOLD_MS;
    if (f > 0.25f) ui_mark_select_round(bench_mid_x(g, t->a), bench_mid_y(g, t->a), g->cs * (0.6f + 0.8f * f));
    ui_animate();
}

/* ---- the marks' grids: a tap puts a piece (a lamp, a tent, a disc, a fill), a held press a mark (a dot, grass, the
 * other disc, a cross); a drag does the same to the stretch of cells that were as the first one ----------------- */
typedef struct {
    int (*state)(int c);                        /* a cell's state now */
    int (*free)(int c);                         /* a cell the player may change */
    int (*tap)(int c), (*hold)(int c);          /* what a tap, a held press makes of the cell */
    int (*paint)(const int *cells, int n, int state);   /* the cells set to a state: 0 nothing changed (or paint_run) */
    int (*paint_run)(int a, int b, int state);  /* the run a..b set (the rules choose its cells) */
    int drag_marks;                             /* 1: a drag makes what a held press makes; 0: what its start made */
} bench_marks;

/* what the finger would make of the cell c now (its state; the cell's own if untouched); swap: the mode button swaps
 * what a tap and a hold make */
static inline int bench_marks_preview(const bench_marks *m, const bench_touch *t, const bench_geo *g, int swap, int c)
{
    int a = t->a;
    if (!t->on || !m->free(a) || !m->free(c)) return m->state(c);
    if (t->moved ? !bench_in_run(g, c, a, t->b) || m->state(c) != m->state(a) : c != a) return m->state(c);
    int mark = t->moved && m->drag_marks ? 1 : (t->moved ? t->held : bench_holding(t)) != swap;
    return mark ? m->hold(a) : m->tap(a);
}

/* the states shown as the finger would leave them (n cells, the board's states in st): st is changed, the real ones
 * kept in real for bench_marks_back */
static inline void bench_marks_show(const bench_marks *m, const bench_touch *t, const bench_geo *g, int swap,
                                    uint16_t *st, uint16_t *real, int n)
{
    uint16_t shown[BENCH_VARS];
    for (int c = 0; c < n; c++) shown[c] = (uint16_t)bench_marks_preview(m, t, g, swap, c);
    memcpy(real, st, (size_t)n * sizeof *st);
    memcpy(st, shown, (size_t)n * sizeof *st);
}

static inline void bench_marks_back(uint16_t *st, const uint16_t *real, int n)
{
    memcpy(st, real, (size_t)n * sizeof *st);
}

/* the finger lifted: the cells changed; the cell pressed (and in *to the state put), -1 when nothing changed (a
 * cell that cannot change: the deny sound) */
static inline int bench_marks_release(const bench_marks *m, bench_touch *t, const bench_geo *g, int swap, int *to)
{
    int a = t->a, kind = bench_release(t);
    if (kind == BENCH_NOTHING) return -1;
    if (!m->free(a)) {
        ui_sound(SND_DENY);
        return -1;
    }
    int held = kind == BENCH_HOLD || kind == BENCH_HOLD_DRAG;
    int mark = kind >= BENCH_DRAG && m->drag_marks ? 1 : held != swap;
    int b = kind >= BENCH_DRAG ? t->b : a;
    *to = mark ? m->hold(a) : m->tap(a);
    if (m->paint_run) return m->paint_run(a, b, *to) ? a : -1;
    int run[BENCH_VARS], cells[BENCH_VARS], k = 0, nr = bench_run(g, a, b, run);
    for (int i = 0; i < nr; i++)
        if (m->free(run[i]) && m->state(run[i]) == m->state(a)) cells[k++] = run[i];
    return m->paint(cells, k, *to) ? a : -1;
}

/* ---- the lights: a hint's place, a piece set down, the win ------------------------------------------------------ */
typedef struct { int at; Uint32 t0; } bench_flash;   /* at: a cell, a link, a side (-1: none) */

static inline void bench_flash_start(bench_flash *f, int at)
{
    f->at = at;
    f->t0 = ui_ms();
    ui_animate();
}

/* a hint's place: a gold light d across swelling and fading at (cx, cy); 0 once it has faded (f->at -1) */
static inline int bench_glow(bench_flash *f, float cx, float cy, float d)
{
    if (f->at < 0) return 0;
    float p = puz_pulse((float)(ui_ms() - f->t0), 0, BENCH_HINT_MS);
    if (p <= 0) {
        f->at = -1;
        return 0;
    }
    ui_mark_gold(cx, cy, d * p);
    ui_animate();
    return 1;
}

/* a piece set down (at f->at) settling from a little larger (by k): its scale now; 1 once settled, or elsewhere */
static inline float bench_pop(const bench_flash *f, int at, float k)
{
    if (f->at < 0 || f->at != at) return 1;
    float t = (ui_ms() - f->t0) / (float)BENCH_POP_MS;
    if (t >= 1) return 1;
    ui_animate();
    return 1 + k * (1 - puz_ease(t));
}

/* the win: a wave of gold light from a point of the grid (in cells: fx, fy) running over it */
typedef struct { int on; float fx, fy; Uint32 t0, ms; } bench_wave;

/* from (fx, fy) over a grid cols x rows: it lasts until it has reached the farthest corner */
static inline void bench_wave_start(bench_wave *w, float fx, float fy, int cols, int rows)
{
    float far = 0;
    for (int k = 0; k < 4; k++) far = fmaxf(far, hypotf((k & 1 ? cols : 0) - fx, (k & 2 ? rows : 0) - fy));
    w->on = 1;
    w->fx = fx;
    w->fy = fy;
    w->t0 = ui_ms();
    w->ms = (Uint32)(far * BENCH_WAVE_STEP + BENCH_WAVE_LEN);
    ui_animate();
}

/* the same from a cell of a grid cols wide */
static inline void bench_wave_from(bench_wave *w, int c, int cols, int rows)
{
    bench_wave_start(w, c % cols + 0.5f, c / cols + 0.5f, cols, rows);
}

static inline int bench_wave_busy(const bench_wave *w) { return w->on && ui_ms() - w->t0 < w->ms; }

/* its light on the point (gx, gy) of the grid, drawn at (cx, cy), d across at its height */
static inline void bench_wave_at(const bench_wave *w, float gx, float gy, float cx, float cy, float d)
{
    if (!w->on) return;
    float p = puz_pulse((float)(ui_ms() - w->t0), hypotf(gx - w->fx, gy - w->fy) * BENCH_WAVE_STEP, BENCH_WAVE_LEN);
    if (p > 0) ui_mark_gold(cx, cy, d * p);
}

/* after the drawing: on while it runs */
static inline void bench_wave_tick(bench_wave *w)
{
    if (!w->on) return;
    if (ui_ms() - w->t0 < w->ms) ui_animate();
    else w->on = 0;
}

/* the wave over every cell of a bench_geo's grid, each light d cells across */
static inline void bench_wave_cells(bench_wave *w, const bench_geo *g, float d)
{
    for (int c = 0; w->on && c < g->cols * g->rows; c++)
        bench_wave_at(w, c % g->cols + 0.5f, c / g->cols + 0.5f, bench_mid_x(g, c), bench_mid_y(g, c), d * g->cs);
    bench_wave_tick(w);
}

/* ---- the bar: the moves (the room the shell's tools leave) ------------------------------------------------------ *
 * A bare number, as in the other level puzzles: the shell already draws the player's token, glowing, at the bar's
 * left end, so the game's token again beside the count read as a second, different count. icon: not drawn (kept so
 * the games' calls stay as they are). */
static inline void bench_bar_moves(box b, SDL_Texture *icon, int moves)
{
    (void)icon;
    ui_count c = {NULL, moves};
    ui_bar_fit(b, &c, 1, 0);
}

/* ---- the tools --------------------------------------------------------------------------------------------------- */
enum { BENCH_T_NOTES, BENCH_T_ERASE, BENCH_T_HINT, BENCH_T_CLASH, BENCH_TOOLS };
static const int BENCH_ICON[BENCH_TOOLS] = {ICON_PENCIL, ICON_ERASER, ICON_HINT, ICON_EYE};

/* ---- the digit keys beside a number grid ------------------------------------------------------------------------ *
 * The board (a square) as big as the room allows, and its keys (as many as the digits):
 * - in a tall room (portrait), in a row at the room's foot, next to the shell's tools under it (two rows of bigger
 *   keys when the room has height to spare and the keys are many), the board in the middle of the room above them;
 * - in a wide room (landscape), in an even block at its right (2 x 2 for four keys, three keys wide for more, a short
 *   last row centred), the block and the board side by side in the room's middle.
 * A key tapped gives its digit; a key carried onto a cell, the same there. */
#define BENCH_KEYS 9
typedef struct {
    int nkeys;
    box board;                                  /* the board's square */
    float kx[BENCH_KEYS + 1], ky[BENCH_KEYS + 1], kw, kh, kp;   /* each key's centre (1..nkeys), a key, their pitch */
    float kv;                                   /* how far up and down a key's touch reaches */
    int made_w, made_h;
    SDL_Texture *t_key;
    struct { int on, key, moved; float x, y, x0, y0; } drag;
} bench_pad;

static inline void bench_pad_layout(bench_pad *k, box b, int nkeys)
{
    float u = ui.unit, gap = 0.3f * u;
    nkeys = nkeys < 1 ? 1 : nkeys > BENCH_KEYS ? BENCH_KEYS : nkeys;
    k->nkeys = nkeys;
    int cols = nkeys == 4 ? 2 : nkeys < 3 ? nkeys : 3, rows = (nkeys + cols - 1) / cols;
    float kh1 = floorf(0.98f * u);
    float bw = cols * 1.15f * u, beside = fminf(b.h, b.w - bw - gap), under = fminf(b.w, b.h - kh1 - gap);
    if (beside > under) {                       /* an even block at the board's right: a bigger board */
        float side = floorf(beside);
        k->kp = fminf(fminf(1.3f * u, side / rows), (b.w - side - 2 * gap) / cols);
        bw = cols * k->kp;
        float x0 = floorf(b.x + (b.w - side - gap - bw) / 2);
        k->board = (box){x0, floorf(b.y + (b.h - side) / 2), side, side};
        k->kh = k->kw = floorf(0.9f * k->kp);
        k->kv = k->kp / 2;
        for (int d = 1; d <= nkeys; d++) {
            int r = (d - 1) / cols, in_row = r == rows - 1 ? nkeys - r * cols : cols;
            k->kx[d] = x0 + side + gap + ((d - 1) % cols + 0.5f + (cols - in_row) / 2.0f) * k->kp;
            k->ky[d] = b.y + b.h / 2 + (r - (rows - 1) / 2.0f) * k->kp;
        }
    } else {                                    /* a row (or two) at the room's foot, the board above */
        int two = 0;
        float kh = kh1, kgap = 0.25f * u;
        if (nkeys > 5) {                        /* two rows of bigger keys, if the board keeps its size */
            float kh2 = floorf(1.05f * u);
            two = fminf(b.w, b.h - 2 * kh2 - kgap - 2 * gap) >= under;
            if (two) kh = kh2;
        }
        int per = two ? (nkeys + 1) / 2 : nkeys, nrows = two ? 2 : 1;
        float pad_h = nrows * kh + (nrows - 1) * kgap;
        float side = floorf(fminf(b.w, b.h - pad_h - gap));
        float spare = b.h - side - pad_h - gap;
        float foot = b.y + b.h - fminf(0.1f * u, fmaxf(0, spare));   /* the keys' bottom */
        float top = foot - pad_h;                /* the board in the middle of the room above the keys */
        k->board = (box){floorf(b.x + (b.w - side) / 2), floorf(b.y + (top - gap - b.y - side) / 2), side, side};
        float span = fminf(b.w, fmaxf(side, per * (two ? 1.45f : 1.1f) * u));   /* wider than the board if need be */
        k->kp = span / per;
        k->kh = kh;
        k->kw = floorf(fminf(1.3f * u, 0.88f * k->kp));
        k->kv = two ? (kh + kgap) / 2 : fmaxf(0.62f * kh, k->kp / 2);
        for (int d = 1; d <= nkeys; d++) {
            int r = (d - 1) / per, in_row = r == nrows - 1 ? nkeys - r * per : per;
            k->kx[d] = floorf(b.x + (b.w - span) / 2) + ((d - 1) % per + 0.5f + (per - in_row) / 2.0f) * k->kp;
            k->ky[d] = floorf(top + r * (kh + kgap) + kh / 2);
        }
    }
    int w = (int)k->kw, h = (int)k->kh;
    if (w < 2 || h < 2) return;
    if (k->made_w != w || k->made_h != h || !k->t_key) {
        k->made_w = w;
        k->made_h = h;
        ui_free(&k->t_key);
        uint8_t *o = ui_buf(w, h);
        bench_cut(o, w, h, 0.14f * fminf(w, h), fmaxf(2, 0.09f * fminf(w, h)), 0.32f);
        k->t_key = ui_tex(w, h, o);
    }
}

/* the board and its keys told to the shell as all they take (ui_board_used; from layout()): the board's square, the
 * keys' block, a gap at its right (laid out again in that width, the keys keep their size) */
static inline void bench_pad_used(const bench_pad *k)
{
    float x0 = k->board.x, x1 = x0 + k->board.w, y0 = k->board.y, y1 = y0 + k->board.h;
    for (int d = 1; d <= k->nkeys; d++) {
        x0 = fminf(x0, k->kx[d] - k->kp / 2);
        x1 = fmaxf(x1, k->kx[d] + k->kp / 2);
        y0 = fminf(y0, k->ky[d] - k->kh / 2);
        y1 = fmaxf(y1, k->ky[d] + k->kh / 2);
    }
    if (k->nkeys > 0 && k->kx[1] > k->board.x + k->board.w) x1 += 0.3f * ui.unit;   /* (the keys beside it) */
    ui_board_used(x0, y0, x1 - x0, y1 - y0);
}

static inline int bench_key_at(const bench_pad *k, float x, float y)
{
    for (int d = 1; d <= k->nkeys; d++)
        if (fabsf(x - k->kx[d]) <= k->kp / 2 && fabsf(y - k->ky[d]) <= k->kv) return d;
    return 0;
}

/* a key: the wooden tile, its digit (small in its corner of a cell when notes are on), scaled s, faded a */
static inline void bench_key(const bench_pad *k, int d, float x, float y, float s, Uint8 a, int notes)
{
    SDL_SetTextureAlphaMod(k->t_key, a);
    ui_shadow_flat(x, y, k->kw * s, k->kh * s, a / 255.0f * 0.8f);
    ui_put_c(k->t_key, x, y, k->kw * s, k->kh * s);
    SDL_SetTextureAlphaMod(k->t_key, 255);
    if (notes) {
        float nx = x + ((d - 1) % 3 - 1) * 0.26f * k->kw * s, ny = y + ((d - 1) / 3 - 1) * 0.25f * k->kh * s;
        ui_number_ex(nx, ny, 0.36f * k->kh * s, d, PUZ_INK, a, UI_NUM_TIGHT);
    } else {
        ui_number_ex(x, y, 0.58f * k->kh * s, d, PUZ_INK, a, UI_NUM_TIGHT);
    }
}

/* the keys (lit: a digit in gold light; faded: bit d, all its digits placed) */
static inline void bench_keys(const bench_pad *k, int lit, unsigned faded, int notes)
{
    for (int d = 1; d <= k->nkeys; d++) {
        if (d == lit) ui_mark_gold(k->kx[d], k->ky[d], 1.7f * k->kw);
        int held = k->drag.on && k->drag.key == d && k->drag.moved;
        bench_key(k, d, k->kx[d], k->ky[d], 1, (Uint8)(held ? 90 : faded >> d & 1 ? 105 : 255), notes);
    }
}

/* the key carried, above the finger (0: none) */
static inline int bench_carried(const bench_pad *k, float *x, float *y)
{
    if (!k->drag.on || !k->drag.key || !k->drag.moved) return 0;
    *x = k->drag.x;
    *y = k->drag.y - 0.6f * k->kh;
    return k->drag.key;
}

/* the finger on a key: 1 when it took it (a tap or a carry follows) */
static inline int bench_pad_press(bench_pad *k, float x, float y)
{
    memset(&k->drag, 0, sizeof k->drag);
    int d = bench_key_at(k, x, y);
    if (!d) return 0;
    k->drag.on = 1;
    k->drag.key = d;
    k->drag.x = k->drag.x0 = x;
    k->drag.y = k->drag.y0 = y;
    return 1;
}

/* 1: a key is held (the finger's motion is the key's) */
static inline int bench_pad_motion(bench_pad *k, float x, float y)
{
    if (!k->drag.on) return 0;
    k->drag.x = x;
    k->drag.y = y;
    if (hypotf(x - k->drag.x0, y - k->drag.y0) > 0.3f * k->kw) k->drag.moved = 1;
    ui_animate();
    return 1;
}

/* the finger lifted: the key tapped (*tapped 1) or carried to (*dx, *dy: under the key); 0: no key held (or a tap that
 * slid off its key) */
static inline int bench_pad_release(bench_pad *k, float x, float y, int *tapped, float *dx, float *dy)
{
    if (!k->drag.on) return 0;
    k->drag.on = 0;
    ui_animate();
    *tapped = !k->drag.moved;
    if (*tapped && bench_key_at(k, x, y) != k->drag.key) return 0;
    *dx = x;
    *dy = y - 0.6f * k->kh;
    return k->drag.key;
}

/* ---- the digit grids' play ------------------------------------------------------------------------------------- */
/* the game's side: its cells on the screen and its rules */
typedef struct {
    int (*cell_at)(float x, float y);           /* the cell under a point (-1: none) */
    void (*cell_xy)(int c, float *x, float *y); /* a cell's top left corner */
    int (*pickable)(int c);                     /* a cell the finger may pick (NULL: every one) */
    int (*editable)(int c);                     /* a cell the player fills (NULL: every one) */
    int (*given)(int c);                        /* a clue's digit (NULL: none) */
    int (*val)(int c);                          /* the digit in a cell (0: none) */
    unsigned (*notes)(int c);                   /* its notes: bit d */
    int (*put)(int c, int d), (*note)(int c, int d), (*erase)(int c), (*hint)(void);
    int (*solved)(void);
    int (*clash)(int c);
    int (*full)(int d);                         /* the digit is everywhere it goes: its key fades (NULL: never) */
    int (*peer)(int sel, int c);                /* the light on c while sel is picked: its alpha (0: none) */
} bench_dg_game;

/* where a cell's digit and notes go, in cells from its top left */
typedef struct {
    float dy, dh;                               /* the digit: its centre below the cell's middle, its height */
    float nx, ny, ndx, ndy, nh, nbw;            /* the notes: the first's centre, the steps, the height, a lit note's
                                                 * box's width (its height: ndy) */
} bench_dg_look;
#define BENCH_DG_LOOK ((bench_dg_look){0.02f, 0.64f, 0.19f, 0.2f, 0.31f, 0.30f, 0.27f, 0.3f})

typedef struct {
    const bench_dg_game *g;
    bench_dg_look look;
    int cols, rows;                             /* the grid (the wave's) */
    float cs;                                   /* a cell's side on the screen */
    int sel, lit, notes, clash;                 /* the cell picked, the digit lit, the pencil on, the clashes shown */
    bench_flash pop, hint;
    bench_wave win;
    bench_pad pad;
} bench_dg;

static inline void bench_dg_calm(bench_dg *k)
{
    k->sel = -1;
    k->lit = 0;
    k->pop.at = k->hint.at = -1;
    k->win.on = 0;
    memset(&k->pad.drag, 0, sizeof k->pad.drag);
}

/* a new puzzle: nothing picked, the pencil off, the clashes shown */
static inline void bench_dg_start(bench_dg *k, const bench_dg_game *g, int cols, int rows)
{
    k->g = g;
    k->cols = cols;
    k->rows = rows;
    k->notes = 0;
    k->clash = 1;
    bench_dg_calm(k);
}

static inline int bench_dg_editable(const bench_dg *k, int c)
{
    return c >= 0 && (!k->g->editable || k->g->editable(c));
}

/* an action done at c: solved by it, the wave from there */
static inline void bench_dg_after(bench_dg *k, int c)
{
    if (k->g->solved() && !k->win.on) {
        bench_wave_from(&k->win, c, k->cols, k->rows);
        k->sel = -1;
        k->lit = 0;
    }
    ui_animate();
}

/* undone: the cell it changed picked */
static inline void bench_dg_undone(bench_dg *k, int c)
{
    bench_dg_calm(k);
    k->sel = c;
}

/* a key: its digit (or note) in the picked cell; nothing to fill: the key lights its digit's cells */
static inline void bench_dg_enter(bench_dg *k, int d)
{
    const bench_dg_game *g = k->g;
    int s = k->sel;
    if (!bench_dg_editable(k, s)) {
        k->lit = k->lit == d ? 0 : d;
        k->sel = -1;
        ui_sound(SND_TAP);
        return;
    }
    k->lit = 0;
    if (k->notes) {
        ui_sound(g->val(s) || !g->note(s, d) ? SND_DENY : SND_TAP);
        return;
    }
    if (g->val(s) == d) {                       /* the same digit again: taken out */
        g->put(s, 0);
        ui_sound(SND_TAP);
        return;
    }
    g->put(s, d);
    bench_flash_start(&k->pop, s);
    ui_sound(SND_PLACE);
    bench_dg_after(k, s);
}

static inline void bench_dg_tool(bench_dg *k, int i)
{
    const bench_dg_game *g = k->g;
    switch (i) {
    case BENCH_T_NOTES: k->notes = !k->notes; ui_sound(SND_TAP); break;
    case BENCH_T_CLASH: k->clash = !k->clash; ui_sound(SND_TAP); break;
    case BENCH_T_ERASE: ui_sound(!bench_dg_editable(k, k->sel) || !g->erase(k->sel) ? SND_DENY : SND_TAP); break;
    default: {
        int c = g->hint();
        if (c < 0) { ui_sound(SND_DENY); break; }
        k->sel = c;
        k->lit = 0;
        bench_flash_start(&k->hint, c);
        bench_flash_start(&k->pop, c);
        ui_sound(SND_PLACE);
        bench_dg_after(k, c);
    }
    }
    ui_animate();
}

static inline int bench_dg_hd(const bench_dg *k)  /* the digit lit: the picked cell's, or the key's */
{
    return k->sel >= 0 && k->g->val(k->sel) ? k->g->val(k->sel) : k->lit;
}

/* under the digits: the picked cell's peers, the cells holding the lit digit, the clashes, the picked cell */
static inline void bench_dg_marks(const bench_dg *k)
{
    const bench_dg_game *g = k->g;
    if (g->solved()) return;
    int hd = bench_dg_hd(k);
    float x, y, q = k->cs;
    for (int c = 0; c < k->cols * k->rows; c++) {
        if (g->pickable && !g->pickable(c)) continue;
        g->cell_xy(c, &x, &y);
        int a = k->sel >= 0 && c != k->sel && g->peer ? g->peer(k->sel, c) : 0;
        if (a) ui_fill(x, y, q, q, 250, 214, 90, (Uint8)a);
        if (hd && g->val(c) == hd && c != k->sel) ui_mark_last(x, y, q, q);
        if (k->clash && g->clash(c)) ui_fill(x, y, q, q, 214, 60, 44, 56);
    }
    if (k->sel >= 0) {
        g->cell_xy(k->sel, &x, &y);
        ui_mark_select(x, y, q, q);
    }
}

/* the hint's light, the win's wave, the digits (a clue's in dark ink, bold; the player's in blue, a clash in red) and
 * the notes */
static inline void bench_dg_cells(bench_dg *k)
{
    const bench_dg_game *g = k->g;
    const bench_dg_look *l = &k->look;
    int solved = g->solved(), hd = bench_dg_hd(k);
    float x, y, q = k->cs;
    if (k->hint.at >= 0) {
        g->cell_xy(k->hint.at, &x, &y);
        bench_glow(&k->hint, x + q / 2, y + q / 2, 2.6f * q);
    }
    for (int c = 0; c < k->cols * k->rows; c++) {
        g->cell_xy(c, &x, &y);
        float mx = x + q / 2, my = y + q / 2;
        bench_wave_at(&k->win, c % k->cols + 0.5f, c / k->cols + 0.5f, mx, my, 2.3f * q);
        int v = g->val(c);
        if (v) {
            int given = g->given && g->given(c);
            const uint8_t *ink = given ? BENCH_GIVEN : k->clash && !solved && g->clash(c) ? BENCH_RED : BENCH_BLUE;
            ui_number_ex(mx, my + l->dy * q, l->dh * q * bench_pop(&k->pop, c, 0.4f), v, ink, 255,
                         given ? UI_NUM_TIGHT : UI_NUM_TIGHT | UI_NUM_REGULAR);
            continue;
        }
        unsigned notes = g->notes(c);
        for (int d = 1; notes && d <= k->pad.nkeys; d++) {
            if (!(notes >> d & 1)) continue;
            float nx = x + (l->nx + l->ndx * ((d - 1) % 3)) * q, ny = y + (l->ny + l->ndy * ((d - 1) / 3)) * q;
            if (d == hd) ui_mark_last(nx - l->nbw / 2 * q, ny - l->ndy / 2 * q, l->nbw * q, l->ndy * q);
            ui_number_ex(nx, ny, l->nh * q, d, d == hd ? BENCH_BLUE : BENCH_NOTE, 255, UI_NUM_TIGHT);
        }
    }
    bench_wave_tick(&k->win);
}

/* the keys, the tools (the shell's: the pencil, the eraser, the hint, the eye), and the key carried (the cell under it
 * picked out) */
static inline void bench_dg_pad(bench_dg *k)
{
    const bench_dg_game *g = k->g;
    int solved = g->solved();
    unsigned faded = 0;
    for (int d = 1; g->full && d <= k->pad.nkeys; d++)
        if (g->full(d)) faded |= 1u << d;
    bench_keys(&k->pad, solved ? 0 : bench_dg_hd(k), faded, k->notes);
    int erasable = bench_dg_editable(k, k->sel) && (g->val(k->sel) || g->notes(k->sel));
    int how[BENCH_TOOLS] = {k->notes ? UI_ON : 0, erasable ? 0 : UI_DIM, solved ? UI_DIM : 0, k->clash ? UI_ON : 0};
    for (int i = 0; i < BENCH_TOOLS; i++) ui_tool(i, BENCH_ICON[i], how[i]);
    float kx, ky, x, y;
    int d = bench_carried(&k->pad, &kx, &ky);
    if (!d) return;
    int c = g->cell_at(kx, ky);
    if (bench_dg_editable(k, c)) {
        g->cell_xy(c, &x, &y);
        ui_mark_select(x, y, k->cs, k->cs);
    }
    bench_key(&k->pad, d, kx, ky, 1.1f, 255, k->notes);
}

/* ---- the digit grids' finger: a tool pressed, a cell picked, a key tapped or carried ----------------------------- */
static inline void bench_dg_press(bench_dg *k, float x, float y)
{
    const bench_dg_game *g = k->g;
    memset(&k->pad.drag, 0, sizeof k->pad.drag);
    if (g->solved()) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) {
        bench_dg_tool(k, t);
        return;
    }
    int c = g->cell_at(x, y);
    if (c >= 0) {
        if (g->pickable && !g->pickable(c)) {   /* nothing to pick there */
            ui_sound(SND_DENY);
            k->sel = -1;
        } else {
            if (k->sel != c) ui_sound(SND_TAP);
            k->sel = c;
            k->lit = 0;
        }
        ui_animate();
        return;
    }
    if (bench_pad_press(&k->pad, x, y)) return; /* a key: on release (a tap), or carried to a cell */
    k->sel = -1;                                /* elsewhere: nothing picked */
    k->lit = 0;
    ui_animate();
}

static inline void bench_dg_motion(bench_dg *k, float x, float y) { bench_pad_motion(&k->pad, x, y); }

static inline void bench_dg_release(bench_dg *k, float x, float y)
{
    int tapped;
    float dx, dy;
    int d = bench_pad_release(&k->pad, x, y, &tapped, &dx, &dy);
    if (!d) return;
    if (tapped) {
        bench_dg_enter(k, d);
        return;
    }
    int c = k->g->cell_at(dx, dy);              /* dropped: on the cell under the key */
    if (!bench_dg_editable(k, c)) c = k->g->cell_at(x, y);
    if (!bench_dg_editable(k, c)) {
        ui_sound(SND_DENY);
        return;
    }
    k->sel = c;
    bench_dg_enter(k, d);
}

/* ---- pictures for the shell -------------------------------------------------------------------------------------- */
/* the level's ebony stones (level + 1 of them) in a row under a picker's card's picture (its bottom at top) */
static inline void bench_stones(uint8_t *rgba, int size, int top, int level)
{
    int d = (int)(0.1f * size), y = top + (size - top - d) / 2;
    uint8_t *st = calloc((size_t)d * d, 4);
    if (!st) return;
    art_counter(COUNTER_STONE, 1, d, st);
    for (int i = 0; i <= level; i++)
        art_blend(rgba, size, size, st, d, d, (int)(size / 2.0f + (i - level / 2.0f) * 1.25f * d - d / 2.0f), y);
    free(st);
}

/* the bar's small picture: the game's token at 0.6 unit, made again when the unit or its frame's wood (the theme)
 * changes */
static inline void bench_small(SDL_Texture **t, int *made, void (*token)(int player, int size, uint8_t *rgba))
{
    int small = (int)(0.6f * ui.unit);
    if (*made == small * 2 + art_frame_is_light() && *t) return;
    *made = small * 2 + art_frame_is_light();
    ui_free(t);
    uint8_t *b = calloc((size_t)small * small, 4);
    if (!b) return;
    token(0, small, b);
    *t = ui_tex(small, small, b);
    free(b);
}

#endif
