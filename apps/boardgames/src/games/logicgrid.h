/*
 * What four logic grids share (Light Up, Binairo, Tents, Skyscrapers): the solver that grades a puzzle by the
 * reasoning it needs, the counter of solutions, the player's undo history, the cells' names; and on the screen (the
 * second half, compiled where shell.h is included first: the games' own .c files) the ruled plank, the cell under the
 * finger (a tap, a press held for a mark, a drag along the cells), the bar's buttons, the hint's light and the win's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="tents" builds
 * tents.c and tents_*.c), so a shared .c file would be left out of one game's build or another's.
 *
 * The solver works on a grid of cells, each with the values still possible (bit v: value v; a cell is decided when
 * one is left). A game gives its deductions in tiers (gl_rules.deduce): tier 0 the easy ones (one look at a clue or a
 * line), tier 1 the medium ones (a pattern, a line seen whole); the solver adds a third way of its own, the hard one:
 * a trial (gl_probe) puts a value in a cell, follows the easy deductions from it, and when they end in a
 * contradiction takes that value out (a chain of reasoning, never a guess: nothing is kept but what is proved).
 * gl_grade solves the way a person does, always the easiest way that works first, and tells the hardest it needed:
 * GL_EASY, GL_MEDIUM, GL_HARD; GL_GUESS when even trials are stuck, GL_NONE when there is no solution. Every way is
 * sound, so a puzzle it solves (GL_EASY .. GL_HARD) has one solution and no other: the makers keep only those.
 */
#ifndef LOGICGRID_H
#define LOGICGRID_H

#include <stdint.h>
#include <string.h>

#include "games/puzrand.h"

#define GL_CELLS 256
#define GL_HIST 4096                            /* the changes undo can take back */

enum { GL_NONE = -1, GL_EASY, GL_MEDIUM, GL_HARD, GL_GUESS };

typedef struct { uint8_t d[GL_CELLS]; } gl_grid;  /* each cell's values still possible (bit v: value v) */

typedef struct {
    int cells;
    int tiers;                                  /* the game's tiers of deductions (0 easy, 1 medium) */
    int probe_tiers;                            /* the tiers a trial follows */
    /* one pass of the deductions of a tier: 1 something found, 0 nothing, -1 a contradiction */
    int (*deduce)(gl_grid *g, int tier, const void *ctx);
    int (*valid)(const gl_grid *g, const void *ctx);   /* every cell decided: 1 a solution */
    const void *ctx;
    long *work;                                 /* counts the passes (a maker's budget); may be NULL */
} gl_rules;

static inline int gl_bits(unsigned m) { return __builtin_popcount(m); }
static inline int gl_low(unsigned m) { return __builtin_ctz(m); }
static inline int gl_one(unsigned m) { return m && !(m & (m - 1)); }

static inline int gl_done(const gl_grid *g, int n)
{
    for (int c = 0; c < n; c++)
        if (!gl_one(g->d[c])) return 0;
    return 1;
}

/* the value of a decided cell (-1: not yet) */
static inline int gl_value(const gl_grid *g, int c) { return gl_one(g->d[c]) ? gl_low(g->d[c]) : -1; }

/* tiers 0..top applied until nothing more is found, the easiest first each time: -1 a contradiction, else 0; *used:
 * the hardest tier that found something (left as it was if none did) */
static inline int gl_settle(gl_grid *g, const gl_rules *r, int top, int *used)
{
    for (;;) {
        int t, res = 0;
        for (t = 0; t <= top && !res; t++) res = r->deduce(g, t, r->ctx);
        if (r->work) (*r->work)++;
        if (res < 0) return -1;
        for (int c = 0; c < r->cells; c++)
            if (!g->d[c]) return -1;
        if (!res) return 0;
        if (t - 1 > *used) *used = t - 1;
    }
}

/* the hard way: a value tried in a cell and followed; a contradiction takes it out. 1: one taken out (the solver goes
 * back to the easy ways), 0: none, -1: a cell has none left. *from: the cell to try first (where the last one
 * was found: the cells before it were tried in vain a moment ago) */
static inline int gl_probe(gl_grid *g, const gl_rules *r, int *from)
{
    uint8_t follows[GL_CELLS];                  /* values a trial without contradiction led to: tried, they would
                                                 * lead to no more than it did (no contradiction either) */
    memset(follows, 0, sizeof follows);
    for (int i = 0; i < r->cells; i++) {
        int c = (*from + i) % r->cells;
        unsigned m = g->d[c] & ~follows[c];
        if (gl_one(g->d[c])) continue;
        for (; m; m &= m - 1) {
            int v = gl_low(m), used = 0;
            gl_grid t = *g;
            t.d[c] = (uint8_t)(1u << v);
            int res = gl_settle(&t, r, r->probe_tiers - 1, &used);
            if (!res && gl_done(&t, r->cells) && !r->valid(&t, r->ctx)) res = -1;
            if (res < 0) {
                g->d[c] &= (uint8_t)~(1u << v);
                *from = c;
                return g->d[c] ? 1 : -1;
            }
            for (int x = 0; x < r->cells; x++)
                if (t.d[x] != g->d[x] && gl_one(t.d[x])) follows[x] |= t.d[x];
        }
    }
    return 0;
}

/* the grade of a puzzle (its cells' values as the clues leave them), looking no further than the level cap: past it,
 * cap + 1 (harder, how much is not known: the makers' quick test); in out (may be NULL) the solution, or where
 * the reasoning stopped */
static inline int gl_grade_upto(const gl_grid *start, const gl_rules *r, gl_grid *out, int cap)
{
    gl_grid g = *start;
    int from = 0, level = GL_EASY, top = cap < r->tiers - 1 ? cap : r->tiers - 1;
    for (;;) {
        if (gl_settle(&g, r, top, &level) < 0) return GL_NONE;
        if (gl_done(&g, r->cells)) {
            if (out) *out = g;
            return r->valid(&g, r->ctx) ? level : GL_NONE;
        }
        if (cap < GL_HARD) {
            if (out) *out = g;
            return cap + 1;
        }
        int p = gl_probe(&g, r, &from);
        if (p < 0) return GL_NONE;
        if (!p) {
            if (out) *out = g;                  /* where it is stuck */
            return GL_GUESS;
        }
        level = GL_HARD;
    }
}

static inline int gl_grade(const gl_grid *start, const gl_rules *r, gl_grid *out)
{
    return gl_grade_upto(start, r, out, GL_HARD);
}

/* the solutions counted up to limit (depth first, the easy and medium ways between the guesses); the first found in
 * sol (may be NULL); rng: the values tried in a random order (a grid filled by chance); budget: passes allowed (0: no
 * end), past them the search stops (the count so far) */
static inline int gl_search(const gl_grid *start, const gl_rules *r, int limit, gl_grid *sol, uint32_t *rng,
                            long budget)
{
    gl_grid g = *start;
    int used = 0, count = 0;
    if (budget && r->work && *r->work > budget) return 0;
    if (gl_settle(&g, r, r->tiers - 1, &used) < 0) return 0;
    int best = -1, bn = 99;
    for (int c = 0; c < r->cells; c++) {
        int k = gl_bits(g.d[c]);
        if (k > 1 && k < bn) bn = k, best = c;
    }
    if (best < 0) {
        if (!r->valid(&g, r->ctx)) return 0;
        if (sol) *sol = g;
        return 1;
    }
    int vs[8], nv = 0;
    for (unsigned m = g.d[best]; m; m &= m - 1) vs[nv++] = gl_low(m);
    if (rng) pr_shuffle(rng, vs, nv);
    for (int i = 0; i < nv && count < limit; i++) {
        gl_grid t = g;
        t.d[best] = (uint8_t)(1u << vs[i]);
        count += gl_search(&t, r, limit - count, count ? NULL : sol, rng, budget);
        if (budget && r->work && *r->work > budget) break;
    }
    return count;
}

/* ---- making a puzzle: a full one (every clue), then clues taken away ------------------------------------------- */
typedef struct {
    void *u;
    int (*grade)(void *u, int cap);             /* the puzzle as it is now, graded no further than cap */
    void (*clue)(void *u, int i, int on);       /* clue i taken away (on 0) or put back (on 1) */
} gl_maker;

/* clues taken away in the order given while the puzzle stays solvable within the level: first all those the easy ways
 * can spare, then those the level's own ways can (the costly ones only for the last few: fast); within a pass a clue
 * that cannot go now cannot go later either (fewer clues are never easier). Stops past the budget (the solver's
 * passes); the grade at the end */
static inline int gl_reduce(const gl_maker *m, const int *order, int n, int level, const long *work, long budget)
{
    uint8_t gone[GL_CELLS];
    memset(gone, 0, sizeof gone);
    for (int cap = GL_EASY; cap <= level; cap = cap < level ? level : cap + 1)
        for (int i = 0; i < n && i < GL_CELLS; i++) {
            if (gone[i] || (work && *work > budget)) continue;
            m->clue(m->u, order[i], 0);
            int g = m->grade(m->u, cap);
            if (g < GL_EASY || g > cap) m->clue(m->u, order[i], 1);
            else gone[i] = 1;
        }
    return m->grade(m->u, level);
}

/* the next cell the easiest reasoning decides from what is known (a hint); -1: none (a contradiction, or stuck) */
static inline int gl_next(gl_grid *g, const gl_rules *r)
{
    uint8_t known[GL_CELLS];
    int from = 0;
    for (int c = 0; c < r->cells; c++) known[c] = (uint8_t)gl_one(g->d[c]);
    for (int round = 0; round < 4 * GL_CELLS; round++) {
        int res = 0;
        for (int t = 0; t < r->tiers && !res; t++) res = r->deduce(g, t, r->ctx);
        if (!res) res = gl_probe(g, r, &from);
        if (res <= 0) return -1;
        for (int c = 0; c < r->cells; c++)
            if (!g->d[c]) return -1;
        for (int c = 0; c < r->cells; c++)
            if (!known[c] && gl_one(g->d[c])) return c;
    }
    return -1;
}

/* ---- the player's cells and their undo --------------------------------------------------------------------------- */
typedef struct { uint16_t cell, was; uint8_t first; } gl_change;
typedef struct {
    gl_change h[GL_HIST];
    int n, moves, open;                         /* open: an action begun, nothing changed yet */
} gl_hist;

/* an action begins (room made for `most` changes: the oldest actions forgotten if need be) */
static inline void gl_begin(gl_hist *k, int most)
{
    k->open = 1;
    if (k->n + most <= GL_HIST) return;
    int cut = GL_HIST / 2;
    while (cut < k->n && !k->h[cut].first) cut++;
    memmove(k->h, k->h + cut, (size_t)(k->n - cut) * sizeof k->h[0]);
    k->n -= cut;
}

/* a cell set to a state within the action */
static inline void gl_set(gl_hist *k, uint16_t *cells, int c, int state)
{
    if (cells[c] == state || k->n >= GL_HIST) return;
    k->h[k->n++] = (gl_change){(uint16_t)c, cells[c], (uint8_t)k->open};
    k->open = 0;
    cells[c] = (uint16_t)state;
}

/* the action ends: 1 if it changed something (a move counted) */
static inline int gl_end(gl_hist *k)
{
    int changed = !k->open;
    k->open = 0;
    if (changed) k->moves++;
    return changed;
}

/* the last action taken back: a cell it changed; -1: none */
static inline int gl_undo(gl_hist *k, uint16_t *cells)
{
    int c = -1;
    while (k->n) {
        gl_change h = k->h[--k->n];
        cells[h.cell] = h.was;
        c = h.cell;
        if (h.first) break;
    }
    if (c >= 0) k->moves--;
    return c;
}

/* a cell's name: its column's letter a.., its row 1.. from the top ("c4"); the cell (-1: none) and the characters
 * read */
static inline int gl_cell(const char *s, int w, int h, int *len)
{
    if (s[0] < 'a' || s[0] >= 'a' + w || s[1] < '1' || s[1] > '9') return -1;
    int row = s[1] - '0', k = 2;
    if (s[2] >= '0' && s[2] <= '9') row = row * 10 + s[2] - '0', k = 3;
    if (row < 1 || row > h) return -1;
    if (len) *len = k;
    return (row - 1) * w + s[0] - 'a';
}

#endif

/* ==== the screen (where shell.h was included first) =============================================================== */
#if defined(SHELL_H) && !defined(LOGICGRID_SCREEN)
#define LOGICGRID_SCREEN

#include <math.h>
#include <stdlib.h>

#include "games/puzzles.h"

#define GL_FRAME 0.035f                         /* the plank's frame, a part of its side */
#define GL_HOLD_MS 420                          /* a press held this long: a mark */
#define GL_HINT_MS 1500                         /* a hint's gold light */
#define GL_POP_MS 200                           /* a piece set down settles from a little larger */
#define GL_WAVE_STEP 45                         /* the win's wave: ms a cell further on */
#define GL_WAVE_LEN 560

static const uint8_t GL_DARK[3] = {52, 34, 22}, GL_CREAM[3] = {246, 238, 220},
                     GL_GOLD[3] = {232, 190, 92}, GL_RED[3] = {184, 44, 34};

/* ---- the plank and its grid -------------------------------------------------------------------------------------- */
typedef struct {
    int cols, rows;
    float lm, tm, rm, bm;                       /* room round the grid for the clues, in cells (left, top...) */
    float bx, by, side;                         /* the plank on the screen */
    int fr, cs;                                 /* its frame, a cell (pixels) */
    float ox, oy;                               /* the grid's corner in the plank's picture */
    float gx, gy;                               /* the grid's corner on the screen */
} gl_geo;

/* the plank fitted in a square in the middle of the box, the grid in the plank; 1: the cells changed size */
static inline int gl_fit(gl_geo *g, box b)
{
    float S = floorf(fminf(b.w, b.h));
    int fr = (int)(S * GL_FRAME), was = g->cs;
    float inner = S - 2 * fr - 0.03f * S, w, h;
    g->cs = (int)fminf(inner / (g->cols + g->lm + g->rm), inner / (g->rows + g->tm + g->bm));
    int changed = g->cs != was || S != g->side;
    g->side = S;
    g->fr = fr;
    g->bx = floorf(b.x + (b.w - S) / 2);
    g->by = floorf(b.y + (b.h - S) / 2);
    w = (g->cols + g->lm + g->rm) * g->cs;
    h = (g->rows + g->tm + g->bm) * g->cs;
    g->ox = floorf((S - w) / 2 + g->lm * g->cs);
    g->oy = floorf((S - h) / 2 + g->tm * g->cs);
    g->gx = g->bx + g->ox;
    g->gy = g->by + g->oy;
    return changed;
}

/* the wood darkened by k in a rectangle of a picture (w x h): a band for the clues, a cell stained */
static inline void gl_shade(uint8_t *o, int w, int h, float x0, float y0, float x1, float y1, float k)
{
    for (int y = (int)fmaxf(0, y0); y < (int)fminf((float)h, y1); y++)
        for (int x = (int)fmaxf(0, x0); x < (int)fminf((float)w, x1); x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
        }
}

/* the plank's picture (side x side): the house wood in its frame, the clue bands a shade darker, the grid ruled (its
 * rim thicker) */
static inline void gl_paint(const gl_geo *g, uint8_t *o)
{
    int S = (int)g->side, q = g->cs;
    float W = (float)g->cols * q, H = (float)g->rows * q, ox = g->ox, oy = g->oy;
    art_board_n(1, 1, S - 2 * g->fr, g->fr, 0, o);
    gl_shade(o, S, S, ox - g->lm * q, oy, ox, oy + H, 0.86f);
    gl_shade(o, S, S, ox + W, oy, ox + W + g->rm * q, oy + H, 0.86f);
    gl_shade(o, S, S, ox, oy - g->tm * q, ox + W, oy, 0.86f);
    gl_shade(o, S, S, ox, oy + H, ox + W, oy + H + g->bm * q, 0.86f);
    for (int way = 0; way < 2; way++)
        for (int i = 0, n = way ? g->rows : g->cols; i <= n; i++) {
            int rim = i == 0 || i == n;
            float t = rim ? fmaxf(2, 0.07f * q) : fmaxf(1, 0.028f * q), p = (float)i * q;
            float lo = p - t / 2, hi = p + t / 2;
            if (i == 0) lo = 0, hi = t;
            if (i == n) lo = p - t, hi = p;
            if (way) puz_ink_rect(o, S, S, ox, oy + lo, ox + W, oy + hi, PUZ_INK, rim ? 0.85f : 0.45f);
            else puz_ink_rect(o, S, S, ox + lo, oy, ox + hi, oy + H, PUZ_INK, rim ? 0.85f : 0.45f);
        }
}

static inline void gl_cell_xy(const gl_geo *g, int c, float *x, float *y)   /* a cell's top left corner */
{
    *x = g->gx + (float)(c % g->cols) * g->cs;
    *y = g->gy + (float)(c / g->cols) * g->cs;
}

static inline float gl_mid_x(const gl_geo *g, int c) { return g->gx + (c % g->cols + 0.5f) * g->cs; }
static inline float gl_mid_y(const gl_geo *g, int c) { return g->gy + (c / g->cols + 0.5f) * g->cs; }

static inline int gl_cell_at(const gl_geo *g, float x, float y)
{
    float ix = x - g->gx, iy = y - g->gy;
    if (ix < 0 || iy < 0 || ix >= g->cols * g->cs || iy >= g->rows * g->cs) return -1;
    return (int)(iy / g->cs) * g->cols + (int)(ix / g->cs);
}

/* ---- the finger: a tap, a press held (a mark), a drag along a row or a column ------------------------------------ */
enum { GL_NOTHING, GL_TAP, GL_HOLD, GL_DRAG, GL_HOLD_DRAG };
typedef struct {
    int on, a, b, moved, held;                  /* a: the cell pressed; b: the drag's end, in a's row or column */
    float x0, y0;
    Uint32 t0;
} gl_touch;

static inline int gl_press(gl_touch *t, const gl_geo *g, float x, float y)
{
    memset(t, 0, sizeof *t);
    int c = gl_cell_at(g, x, y);
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
static inline void gl_motion(gl_touch *t, const gl_geo *g, float x, float y, int runs)
{
    if (!t->on) return;
    if (!t->moved && runs && hypotf(x - t->x0, y - t->y0) > 0.35f * g->cs) {
        t->held = ui_ms() - t->t0 >= GL_HOLD_MS;
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
static inline int gl_holding(const gl_touch *t) { return t->on && !t->moved && ui_ms() - t->t0 >= GL_HOLD_MS; }

/* the finger lifted: what it did (GL_*) */
static inline int gl_release(gl_touch *t)
{
    if (!t->on) return GL_NOTHING;
    int held = ui_ms() - t->t0 >= GL_HOLD_MS;
    t->on = 0;
    ui_animate();
    return t->moved ? (t->held ? GL_HOLD_DRAG : GL_DRAG) : held ? GL_HOLD : GL_TAP;
}

/* the cells from a to b (in a row or a column), a first; how many */
static inline int gl_run(const gl_geo *g, int a, int b, int *out)
{
    int n = 0, w = g->cols, dx = (b % w > a % w) - (b % w < a % w), dy = (b / w > a / w) - (b / w < a / w);
    for (int c = a;; c += dy * w + dx) {
        out[n++] = c;
        if (c == b) break;
    }
    return n;
}

static inline int gl_in_run(const gl_geo *g, int c, int a, int b)
{
    int w = g->cols, x = c % w, y = c / w, ax = a % w, ay = a / w, bx = b % w, by = b / w;
    return x >= (ax < bx ? ax : bx) && x <= (ax < bx ? bx : ax) && y >= (ay < by ? ay : by) && y <= (ay < by ? by : ay);
}

/* a press growing into a mark: a ring swelling round the cell */
static inline void gl_touch_ring(const gl_touch *t, const gl_geo *g)
{
    if (!t->on || t->moved || gl_holding(t)) return;
    float f = (ui_ms() - t->t0) / (float)GL_HOLD_MS;
    if (f > 0.25f) ui_mark_select_round(gl_mid_x(g, t->a), gl_mid_y(g, t->a), g->cs * (0.6f + 0.8f * f));
    ui_animate();
}

/* ---- the bar: the moves, and the game's buttons at its right end (the shell's: ui_button) ------------------ */
/* the bar holds the moves and n buttons at its right end; a narrow one (landscape) drops the moves' little picture
 * and makes the buttons smaller, so that nothing overlaps */
static inline int gl_bar_roomy(box b, int n) { return b.w >= (1.9f + 1.05f * n) * ui.unit; }

static inline void gl_bar_moves(box b, SDL_Texture *icon, int moves, int n)
{
    puz_bar_moves(b, gl_bar_roomy(b, n) ? icon : NULL, moves);
}

/* the i-th of n buttons from the bar's right end: its centre and size */
static inline void gl_bar_spot(box b, int i, int n, float *x, float *y, float *d)
{
    float u = ui.unit, room = (b.w - (gl_bar_roomy(b, n) ? 1.9f : 1.1f) * u) / (1.15f * n);
    *d = clampf(room, 0.56f * u, 0.84f * u);
    *x = b.x + b.w - *d / 2 - 0.05f * u - i * 1.15f * *d;
    *y = b.y + b.h / 2;
}

static inline int gl_bar_hit(box b, int i, int n, float x, float y)
{
    float cx, cy, d;
    gl_bar_spot(b, i, n, &cx, &cy, &d);
    return b.w > 0 && ui_button_hit(cx, cy, d, x, y);
}

/* ---- light: a hint's cell, the win's wave ------------------------------------------------------------------------ */
typedef struct {
    int on, from;                               /* from: the cell the light starts at */
    Uint32 t0;
} gl_light;

static inline void gl_light_start(gl_light *l, int from)
{
    l->on = 1;
    l->from = from;
    l->t0 = ui_ms();
    ui_animate();
}

/* the hint: a gold light swelling and fading on its cell */
static inline void gl_hint_draw(gl_light *l, const gl_geo *g)
{
    if (!l->on) return;
    float p = puz_pulse((float)(ui_ms() - l->t0), 0, GL_HINT_MS);
    if (p <= 0) {
        l->on = 0;
        return;
    }
    ui_mark_gold(gl_mid_x(g, l->from), gl_mid_y(g, l->from), 2.6f * g->cs * p);
    ui_animate();
}

static inline Uint32 gl_wave_len(const gl_geo *g) { return (Uint32)((g->cols + g->rows) * GL_WAVE_STEP + GL_WAVE_LEN); }
static inline int gl_wave_busy(const gl_light *l, const gl_geo *g) { return l->on && ui_ms() - l->t0 < gl_wave_len(g); }

/* the win: a wave of gold light running over the board from a cell */
static inline void gl_wave_draw(gl_light *l, const gl_geo *g)
{
    if (!l->on) return;
    float t = (float)(ui_ms() - l->t0);
    for (int c = 0; c < g->cols * g->rows; c++) {
        float dist = hypotf((float)(c % g->cols - l->from % g->cols), (float)(c / g->cols - l->from / g->cols));
        float p = puz_pulse(t, dist * GL_WAVE_STEP, GL_WAVE_LEN);
        if (p > 0) ui_mark_gold(gl_mid_x(g, c), gl_mid_y(g, c), 2.3f * g->cs * p);
    }
    if (t < gl_wave_len(g)) ui_animate();
    else l->on = 0;
}

/* a piece set down at t0: how much larger it is drawn (1 once settled) */
static inline float gl_pop(Uint32 t0)
{
    float t = (ui_ms() - t0) / (float)GL_POP_MS;
    if (t >= 1) return 1;
    ui_animate();
    return 1 + 0.35f * (1 - puz_ease(t));
}

/* ---- the picker's cards: the level as ebony stones under the puzzle (one easy .. three hard) --------------------- */
static inline void gl_stones(uint8_t *rgba, int size, int level, int top)
{
    int d = (int)(0.1f * size), y = top + (size - top - d) / 2;
    uint8_t *st = calloc((size_t)d * d, 4);
    if (!st) return;
    art_counter(COUNTER_STONE, 1, d, st);
    for (int i = 0; i <= level; i++)
        art_blend(rgba, size, size, st, d, d, (int)(size / 2.0f + (i - level / 2.0f) * 1.25f * d - d / 2.0f), y);
    free(st);
}

#endif
