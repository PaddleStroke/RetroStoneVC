/*
 * Sudoku: the rules (sudoku.h): the solver that counts solutions, the grader that solves the way a person does, the
 * maker, and the player's grid with its notes and undo. No SDL: tested by tests/sudoku_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/sudoku.h"

#include <string.h>

#include "games/puzrand.h"

#define ALL 0x3feu                              /* the digits 1..9 as bits */

static uint8_t UNIT[27][9];                     /* rows 0..8, columns 9..17, boxes 18..26 */
static uint8_t PEER[SU_CELLS][20];
static uint8_t UNITS_OF[SU_CELLS][3];
static int ready;

static int box_of(int c) { return c / 27 * 3 + c % 9 / 3; }

static void init(void)
{
    if (ready) return;
    for (int i = 0; i < 9; i++)
        for (int j = 0; j < 9; j++) {
            UNIT[i][j] = (uint8_t)(i * 9 + j);
            UNIT[9 + i][j] = (uint8_t)(j * 9 + i);
            UNIT[18 + i][j] = (uint8_t)((i / 3 * 3 + j / 3) * 9 + i % 3 * 3 + j % 3);
        }
    for (int c = 0; c < SU_CELLS; c++) {
        int n = 0;
        for (int p = 0; p < SU_CELLS; p++)
            if (p != c && (p / 9 == c / 9 || p % 9 == c % 9 || box_of(p) == box_of(c))) PEER[c][n++] = (uint8_t)p;
        UNITS_OF[c][0] = (uint8_t)(c / 9);
        UNITS_OF[c][1] = (uint8_t)(9 + c % 9);
        UNITS_OF[c][2] = (uint8_t)(18 + box_of(c));
    }
    ready = 1;
}

static int bits(unsigned m) { return __builtin_popcount(m); }
static int low(unsigned m) { return __builtin_ctz(m); }

/* ---- counting solutions: depth first, the cell with the fewest digits left first -------------------------------- */
typedef struct {
    uint8_t g[SU_CELLS];
    uint16_t row[9], col[9], box[9];            /* the digits used */
    int count, limit;
    uint8_t *sol;
    uint32_t *rng;                              /* filling a grid by chance: the digits tried in a random order */
} counter;

static void search(counter *k)
{
    int best = -1, bn = 10;
    unsigned bm = 0;
    for (int c = 0; c < SU_CELLS; c++) {
        if (k->g[c]) continue;
        unsigned m = ALL & ~(k->row[c / 9] | k->col[c % 9] | k->box[box_of(c)]);
        int n = bits(m);
        if (n < bn) { bn = n; best = c; bm = m; if (n <= 1) break; }
    }
    if (best < 0) {                             /* full: a solution */
        if (!k->count++ && k->sol) memcpy(k->sol, k->g, SU_CELLS);
        return;
    }
    int ds[9], nd = 0;
    for (unsigned m = bm; m; m &= m - 1) ds[nd++] = low(m);
    if (k->rng) pr_shuffle(k->rng, ds, nd);
    int r = best / 9, cl = best % 9, b = box_of(best);
    for (int i = 0; i < nd && k->count < k->limit; i++) {
        unsigned bit = 1u << ds[i];
        k->g[best] = (uint8_t)ds[i];
        k->row[r] |= bit, k->col[cl] |= bit, k->box[b] |= bit;
        search(k);
        k->row[r] &= ~bit, k->col[cl] &= ~bit, k->box[b] &= ~bit;
    }
    k->g[best] = 0;
}

static int counter_load(counter *k, const uint8_t *grid)
{
    memset(k, 0, sizeof *k);
    for (int c = 0; c < SU_CELLS; c++) {
        int d = grid[c];
        if (!d) continue;
        unsigned bit = 1u << d;
        if (d > 9 || (k->row[c / 9] | k->col[c % 9] | k->box[box_of(c)]) & bit) return 0;   /* a clash */
        k->g[c] = (uint8_t)d;
        k->row[c / 9] |= bit, k->col[c % 9] |= bit, k->box[box_of(c)] |= bit;
    }
    return 1;
}

int su_count(const uint8_t *grid, int limit, uint8_t *sol)
{
    counter k;
    if (!counter_load(&k, grid)) return 0;
    k.limit = limit;
    k.sol = sol;
    search(&k);
    return k.count;
}

static void fill(uint32_t *rng, uint8_t *out)
{
    counter k;
    memset(&k, 0, sizeof k);
    k.limit = 1;
    k.sol = out;
    k.rng = rng;
    search(&k);
}

/* ---- grading: the ways a person reasons, easiest first ----------------------------------------------------------- */
typedef struct {
    uint8_t v[SU_CELLS];
    uint16_t cand[SU_CELLS];                    /* the digits still possible in an empty cell */
} pencil;

static void place(pencil *s, int c, int d)
{
    s->v[c] = (uint8_t)d;
    s->cand[c] = 0;
    for (int i = 0; i < 20; i++) s->cand[PEER[c][i]] &= (uint16_t)~(1u << d);
}

static int pencil_load(pencil *s, const uint8_t *grid)
{
    memset(s->v, 0, sizeof s->v);
    for (int c = 0; c < SU_CELLS; c++) s->cand[c] = ALL;
    for (int c = 0; c < SU_CELLS; c++)
        if (grid[c]) {
            if (!(s->cand[c] & 1u << grid[c])) return 0;
            place(s, c, grid[c]);
        }
    return 1;
}

/* a digit's places in a unit, as bits of the unit's 9 cells */
static unsigned places(const pencil *s, int u, int d)
{
    unsigned m = 0;
    for (int j = 0; j < 9; j++)
        if (s->cand[UNIT[u][j]] >> d & 1) m |= 1u << j;
    return m;
}

static int has_digit(const pencil *s, int u, int d)
{
    for (int j = 0; j < 9; j++)
        if (s->v[UNIT[u][j]] == d) return 1;
    return 0;
}

static int naked_single(pencil *s, int *cell)
{
    for (int c = 0; c < SU_CELLS; c++)
        if (!s->v[c] && bits(s->cand[c]) == 1) {
            if (cell) *cell = c;
            place(s, c, low(s->cand[c]));
            return 1;
        }
    return 0;
}

static int hidden_single(pencil *s, int *cell)
{
    for (int u = 0; u < 27; u++)
        for (int d = 1; d <= 9; d++) {
            unsigned m = places(s, u, d);
            if (bits(m) != 1) continue;
            int c = UNIT[u][low(m)];
            if (cell) *cell = c;
            place(s, c, d);
            return 1;
        }
    return 0;
}

static int remove_cand(pencil *s, int c, unsigned m)
{
    if (!(s->cand[c] & m)) return 0;
    s->cand[c] &= (uint16_t)~m;
    return 1;
}

/* a digit's places in a box all in one row or column: not elsewhere in that line; in a line all in one box: not
 * elsewhere in that box */
static int locked(pencil *s)
{
    for (int b = 18; b < 27; b++)
        for (int d = 1; d <= 9; d++) {
            unsigned m = places(s, b, d);
            if (!m) continue;
            int r0 = -1, c0 = -1, rows = 1, cols = 1;
            for (unsigned k = m; k; k &= k - 1) {
                int c = UNIT[b][low(k)];
                if (r0 >= 0 && c / 9 != r0) rows = 0;
                if (c0 >= 0 && c % 9 != c0) cols = 0;
                r0 = c / 9, c0 = c % 9;
            }
            int ch = 0;
            for (int j = 0; j < 9; j++) {
                int rc = UNIT[r0][j], cc = UNIT[9 + c0][j];
                if (rows && box_of(rc) != b - 18) ch |= remove_cand(s, rc, 1u << d);
                if (cols && box_of(cc) != b - 18) ch |= remove_cand(s, cc, 1u << d);
            }
            if (ch) return 1;
        }
    for (int u = 0; u < 18; u++)
        for (int d = 1; d <= 9; d++) {
            unsigned m = places(s, u, d);
            if (!m) continue;
            int b0 = -1, one = 1;
            for (unsigned k = m; k; k &= k - 1) {
                int bx = box_of(UNIT[u][low(k)]);
                if (b0 >= 0 && bx != b0) one = 0;
                b0 = bx;
            }
            if (!one) continue;
            int ch = 0;
            for (int j = 0; j < 9; j++) {
                int c = UNIT[18 + b0][j];
                if (!(u < 9 ? c / 9 == u : c % 9 == u - 9)) ch |= remove_cand(s, c, 1u << d);
            }
            if (ch) return 1;
        }
    return 0;
}

/* n cells of a unit holding only n digits between them: those digits not elsewhere in the unit */
static int naked(pencil *s, int n)
{
    for (int u = 0; u < 27; u++) {
        unsigned empty = 0;
        for (int j = 0; j < 9; j++)
            if (!s->v[UNIT[u][j]]) empty |= 1u << j;
        if (bits(empty) <= n) continue;
        for (unsigned set = 1; set < 512; set++) {
            if (bits(set) != n || (set & ~empty)) continue;
            unsigned uni = 0;
            for (unsigned k = set; k; k &= k - 1) uni |= s->cand[UNIT[u][low(k)]];
            if (bits(uni) != n) continue;
            int ch = 0;
            for (unsigned k = empty & ~set; k; k &= k - 1) ch |= remove_cand(s, UNIT[u][low(k)], uni);
            if (ch) return 1;
        }
    }
    return 0;
}

/* n digits whose places in a unit are only n cells: those cells hold nothing else */
static int hidden(pencil *s, int n)
{
    for (int u = 0; u < 27; u++) {
        unsigned free_d = 0, pl[10];
        for (int d = 1; d <= 9; d++) {
            pl[d] = places(s, u, d);
            if (pl[d] && !has_digit(s, u, d)) free_d |= 1u << d;
        }
        if (bits(free_d) <= n) continue;
        for (unsigned set = 2; set < 1024; set += 2) {
            if (bits(set) != n || (set & ~free_d)) continue;
            unsigned cells = 0;
            for (unsigned k = set; k; k &= k - 1) cells |= pl[low(k)];
            if (bits(cells) != n) continue;
            int ch = 0;
            for (unsigned k = cells; k; k &= k - 1) ch |= remove_cand(s, UNIT[u][low(k)], ALL & ~set);
            if (ch) return 1;
        }
    }
    return 0;
}

/* a fish of size n (2 the X-wing, 3 the swordfish): a digit's places in n rows all in the same n columns, so not
 * elsewhere in those columns (and the same with columns and rows swapped) */
static int fish(pencil *s, int n)
{
    for (int d = 1; d <= 9; d++)
        for (int way = 0; way < 2; way++) {
            unsigned pl[9];
            for (int i = 0; i < 9; i++) pl[i] = places(s, way * 9 + i, d);
            for (unsigned set = 1; set < 512; set++) {
                if (bits(set) != n) continue;
                unsigned uni = 0;
                int ok = 1;
                for (unsigned k = set; k; k &= k - 1) {
                    unsigned m = pl[low(k)];
                    if (bits(m) < 2 || bits(m) > n) ok = 0;
                    uni |= m;
                }
                if (!ok || bits(uni) != n) continue;
                int ch = 0;
                for (unsigned k = uni; k; k &= k - 1)
                    for (int i = 0; i < 9; i++) {
                        if (set >> i & 1) continue;
                        int c = way ? UNIT[low(k)][i] : UNIT[9 + low(k)][i];   /* the cross line's i-th cell */
                        ch |= remove_cand(s, c, 1u << d);
                    }
                if (ch) return 1;
            }
        }
    return 0;
}

static int sees(int a, int b) { return a != b && (a / 9 == b / 9 || a % 9 == b % 9 || box_of(a) == box_of(b)); }

/* a cell of two digits xy, seeing a cell xz and a cell yz: whichever it holds, one of them is z; so no cell seeing
 * both of them is z */
static int xy_wing(pencil *s)
{
    for (int p = 0; p < SU_CELLS; p++) {
        unsigned pm = s->cand[p];
        if (s->v[p] || bits(pm) != 2) continue;
        for (int i = 0; i < 20; i++) {
            int a = PEER[p][i];
            unsigned am = s->cand[a];
            if (s->v[a] || bits(am) != 2 || bits(am & pm) != 1) continue;
            unsigned z = am & ~pm, bm = (pm & ~am) | z;
            for (int j = 0; j < 20; j++) {
                int b = PEER[p][j];
                if (b == a || s->v[b] || s->cand[b] != bm) continue;
                int ch = 0;
                for (int c = 0; c < SU_CELLS; c++)
                    if (!s->v[c] && c != p && sees(c, a) && sees(c, b)) ch |= remove_cand(s, c, z);
                if (ch) return 1;
            }
        }
    }
    return 0;
}

/* one step of the easiest way that works; its level; -1: stuck */
static int step(pencil *s)
{
    if (naked_single(s, NULL) || hidden_single(s, NULL)) return 0;
    if (locked(s) || naked(s, 2) || hidden(s, 2)) return 1;
    if (naked(s, 3) || hidden(s, 3) || fish(s, 2)) return 2;
    if (naked(s, 4) || hidden(s, 4) || fish(s, 3) || xy_wing(s)) return 3;
    return -1;
}

int su_rate(const uint8_t *grid)
{
    init();
    pencil s;
    if (!pencil_load(&s, grid)) return 4;
    int level = 0;
    for (;;) {
        int left = 0;
        for (int c = 0; c < SU_CELLS; c++)
            if (!s.v[c]) {
                left++;
                if (!s.cand[c]) return 4;       /* a contradiction: no solution */
            }
        if (!left) return level;
        int l = step(&s);
        if (l < 0) return 4;
        if (l > level) level = l;
    }
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
static int clues_floor(int level) { return level == SU_EASY ? 36 : level == SU_MEDIUM ? 28 : 0; }
/* the grades a level takes: easy 0, medium 1, hard 2 or 3, expert 4 */
static const int LOW[SU_VARIANTS] = {0, 1, 2, 4}, CAP[SU_VARIANTS] = {0, 1, 3, 4};

void su_make(int level, uint32_t seed, uint8_t *puzzle, uint8_t *sol)
{
    init();
    if (level < 0 || level > SU_EXPERT) level = SU_EASY;
    uint32_t s = pr_seed(seed);
    uint8_t p[SU_CELLS], full[SU_CELLS], best[SU_CELLS], best_sol[SU_CELLS];
    int best_r = -1;
    for (int attempt = 0; attempt < 600; attempt++) {
        fill(&s, full);
        memcpy(p, full, SU_CELLS);
        int order[41], clues = SU_CELLS;
        for (int i = 0; i < 41; i++) order[i] = i;
        pr_shuffle(&s, order, 41);
        for (int i = 0; i < 41; i++) {
            int a = order[i], b = SU_CELLS - 1 - a, n = a == b ? 1 : 2;
            if (clues - n < clues_floor(level)) continue;
            uint8_t ka = p[a], kb = p[b];
            p[a] = p[b] = 0;
            if (su_count(p, 2, NULL) != 1 || (CAP[level] < 4 && su_rate(p) > CAP[level])) {
                p[a] = ka, p[b] = kb;
                continue;
            }
            clues -= n;
        }
        int r = su_rate(p);
        if (r >= LOW[level] && r <= CAP[level]) {
            memcpy(puzzle, p, SU_CELLS);
            memcpy(sol, full, SU_CELLS);
            return;
        }
        if (r > best_r) {   /* the nearest yet, should none be found */
            best_r = r;
            memcpy(best, p, SU_CELLS);
            memcpy(best_sol, full, SU_CELLS);
        }
    }
    memcpy(puzzle, best, SU_CELLS);
    memcpy(sol, best_sol, SU_CELLS);
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
void su_load(su_game *g, int level, const uint8_t *puzzle, const uint8_t *sol)
{
    init();
    memset(g, 0, sizeof *g);
    g->level = level;
    for (int c = 0; c < SU_CELLS; c++) {
        g->given[c] = puzzle[c] != 0;
        g->val[c] = puzzle[c];
        g->sol[c] = sol[c];
    }
}

void su_new(su_game *g, int level, uint32_t seed)
{
    uint8_t p[SU_CELLS], s[SU_CELLS];
    su_make(level, seed, p, s);
    su_load(g, level, p, s);
    g->seed = seed;
}

/* the history (bench.h): each cell's state before a change (its digit, its notes), an action undone whole */
static void change(su_game *g, int c, int val, unsigned notes)
{
    if (g->val[c] == val && g->notes[c] == notes) return;
    bench_log(&g->hist, c, (uint32_t)g->val[c] << 16 | g->notes[c]);
    g->val[c] = (uint8_t)val;
    g->notes[c] = (uint16_t)notes;
}

/* the wrong digits on the board (the stars' mistakes) */
static int wrongs(const su_game *g)
{
    int k = 0;
    for (int c = 0; c < SU_CELLS; c++) k += g->val[c] && g->val[c] != g->sol[c];
    return k;
}

int su_put(su_game *g, int c, int d)
{
    if (c < 0 || c >= SU_CELLS || d < 0 || d > 9 || g->given[c] || g->val[c] == d) return 0;
    bench_begin(&g->hist, 24);
    change(g, c, d, 0);
    if (d)
        for (int i = 0; i < 20; i++) {
            int p = PEER[c][i];
            if (g->notes[p] >> d & 1) change(g, p, g->val[p], g->notes[p] & ~(1u << d));
        }
    return bench_done(&g->hist, wrongs(g));
}

int su_note(su_game *g, int c, int d)
{
    if (c < 0 || c >= SU_CELLS || d < 1 || d > 9 || g->given[c] || g->val[c]) return 0;
    bench_begin(&g->hist, 24);
    change(g, c, 0, g->notes[c] ^ (1u << d));
    return bench_done(&g->hist, wrongs(g));
}

int su_erase(su_game *g, int c)
{
    if (c < 0 || c >= SU_CELLS || g->given[c]) return 0;
    if (g->val[c]) return su_put(g, c, 0);
    if (!g->notes[c]) return 0;
    bench_begin(&g->hist, 24);
    change(g, c, 0, 0);
    return bench_done(&g->hist, wrongs(g));
}

static int hint_step(su_game *g)
{
    for (int c = 0; c < SU_CELLS; c++)          /* a wrong digit first */
        if (g->val[c] && g->val[c] != g->sol[c]) return su_put(g, c, g->sol[c]) ? c : -1;
    pencil s;
    if (!pencil_load(&s, g->val)) return -1;
    int c = -1;
    if (!naked_single(&s, &c) && !hidden_single(&s, &c)) {   /* no single: the cell with the fewest digits left */
        int bn = 10;
        for (int i = 0; i < SU_CELLS; i++)
            if (!g->val[i] && bits(s.cand[i]) < bn) bn = bits(s.cand[i]), c = i;
    }
    if (c < 0) return -1;
    return su_put(g, c, g->sol[c]) ? c : -1;
}

int su_hint(su_game *g)
{
    return bench_hinted(&g->hist, hint_step(g));
}

static void put_back(void *ctx, int c, uint32_t was)
{
    su_game *g = ctx;
    g->val[c] = (uint8_t)(was >> 16);
    g->notes[c] = (uint16_t)was;
}

int su_undo(su_game *g)
{
    int c = bench_undo(&g->hist, put_back, g);
    bench_undone(&g->hist, wrongs(g));
    return c;
}


int su_clash(const su_game *g, int c)
{
    init();
    int d = g->val[c];
    if (!d) return 0;
    for (int i = 0; i < 20; i++)
        if (g->val[PEER[c][i]] == d) return 1;
    return 0;
}

int su_solved(const su_game *g)
{
    for (int c = 0; c < SU_CELLS; c++)
        if (g->val[c] != g->sol[c]) return 0;
    return 1;
}

int su_count_digit(const su_game *g, int d)
{
    int n = 0;
    for (int c = 0; c < SU_CELLS; c++) n += g->val[c] == d;
    return n;
}

int su_cell(const char *s)
{
    if (s[0] < 'a' || s[0] > 'i' || s[1] < '1' || s[1] > '9') return -1;
    return (s[1] - '1') * 9 + s[0] - 'a';
}

int su_script(su_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (su_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int n = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
            for (int c = 0; c < SU_CELLS && n > 0; c++)
                if (!g->val[c]) { su_put(g, c, g->sol[c]); n--; }
            continue;
        }
        int c = su_cell(s);
        if (c < 0 || (s[2] != '=' && s[2] != '\'') || s[3] < '0' || s[3] > '9') return 0;
        int ok = s[2] == '=' ? su_put(g, c, s[3] - '0') : su_note(g, c, s[3] - '0');
        if (!ok) return 0;
        s += 4;
    }
    return 1;
}
