/*
 * Futoshiki: the rules (futoshiki.h): the solver that counts solutions, the grader that solves the way a person does,
 * the maker, and the player's grid with its notes and undo. No SDL: tested by tests/futoshiki_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/futoshiki.h"

#include <string.h>

#include "games/puzrand.h"

/* ---- the pencil: the digits each cell can still hold ------------------------------------------------------------ */
typedef struct {
    const fsk_puz *p;
    int n;
    uint16_t cand[FSK_CELLS];                   /* bit d: d still possible (a placed cell: its digit alone) */
    uint8_t v[FSK_CELLS];                       /* placed digits (0: not yet) */
} pen;

static int ucell(int n, int u, int j) { return u < n ? u * n + j : j * n + u - n; }   /* rows 0..n-1, then columns */

static void place(pen *s, int c, int d)
{
    int n = s->n, r = c / n, k = c % n;
    s->v[c] = (uint8_t)d;
    s->cand[c] = (uint16_t)(1u << d);
    for (int j = 0; j < n; j++) {
        if (j != k) s->cand[r * n + j] &= (uint16_t)~(1u << d);
        if (j != r) s->cand[j * n + k] &= (uint16_t)~(1u << d);
    }
}

static int pen_load(pen *s, const fsk_puz *p, const uint8_t *grid)
{
    s->p = p;
    s->n = p->n;
    uint16_t all = (uint16_t)(((1u << p->n) - 1) << 1);
    for (int c = 0; c < FSK_CELLS; c++) s->cand[c] = all, s->v[c] = 0;
    for (int c = 0; c < p->n * p->n; c++)
        if (grid[c]) {
            if (grid[c] > p->n || !(s->cand[c] & 1u << grid[c])) return 0;
            place(s, c, grid[c]);
        }
    return 1;
}

static int broken(const pen *s)                 /* a cell with nothing left, a digit with no place in a line */
{
    int n = s->n;
    for (int c = 0; c < n * n; c++)
        if (!s->cand[c]) return 1;
    for (int u = 0; u < 2 * n; u++) {
        unsigned seen = 0;
        for (int j = 0; j < n; j++) seen |= s->cand[ucell(n, u, j)];
        if (bench_bits(seen) < n) return 1;
    }
    return 0;
}

static int all_placed(const pen *s)
{
    for (int c = 0; c < s->n * s->n; c++)
        if (!s->v[c]) return 0;
    return 1;
}

static int naked_single(pen *s, int *cell)
{
    for (int c = 0; c < s->n * s->n; c++)
        if (!s->v[c] && bench_bits(s->cand[c]) == 1) {
            if (cell) *cell = c;
            place(s, c, bench_low(s->cand[c]));
            return 1;
        }
    return 0;
}

static int hidden_single(pen *s, int *cell)
{
    int n = s->n;
    for (int u = 0; u < 2 * n; u++)
        for (int d = 1; d <= n; d++) {
            int at = -1, k = 0;
            for (int j = 0; j < n; j++) {
                int c = ucell(n, u, j);
                if (s->cand[c] >> d & 1) at = c, k++;
            }
            if (k != 1 || s->v[at]) continue;
            if (cell) *cell = at;
            place(s, at, d);
            return 1;
        }
    return 0;
}

/* the signs: a cell less than its neighbour holds less than the greatest digit the neighbour can hold, and the other
 * way round; all the signs once over (chains follow as the step repeats) */
static int bounds(pen *s)
{
    int n = s->n, ch = 0;
    for (int way = 0; way < 2; way++)
        for (int c = 0; c < n * n; c++) {
            int r = s->p->rel[way][c];
            if (!r || (way ? c / n >= n - 1 : c % n >= n - 1)) continue;
            int o = way ? c + n : c + 1, lo = r == FSK_LT ? c : o, hi = r == FSK_LT ? o : c;
            if (!s->cand[lo] || !s->cand[hi]) return 1;
            unsigned ml = s->cand[lo] & ((1u << bench_high(s->cand[hi])) - 1);
            unsigned mh = s->cand[hi] & ~((2u << bench_low(s->cand[lo])) - 1);
            if (ml != s->cand[lo] || mh != s->cand[hi]) ch = 1;
            s->cand[lo] = (uint16_t)ml;
            s->cand[hi] = (uint16_t)mh;
        }
    return ch;
}

static int remove_cand(pen *s, int c, unsigned m)
{
    if (!(s->cand[c] & m)) return 0;
    s->cand[c] &= (uint16_t)~m;
    return 1;
}

/* k cells of a line holding only k digits between them: those digits not elsewhere in the line */
static int naked(pen *s, int k)
{
    int n = s->n;
    for (int u = 0; u < 2 * n; u++) {
        unsigned open = 0;
        for (int j = 0; j < n; j++)
            if (!s->v[ucell(n, u, j)]) open |= 1u << j;
        if (bench_bits(open) <= k) continue;
        for (unsigned set = 1; set < 1u << n; set++) {
            if (bench_bits(set) != k || (set & ~open)) continue;
            unsigned uni = 0;
            for (unsigned m = set; m; m &= m - 1) uni |= s->cand[ucell(n, u, bench_low(m))];
            if (bench_bits(uni) != k) continue;
            int ch = 0;
            for (unsigned m = open & ~set; m; m &= m - 1) ch |= remove_cand(s, ucell(n, u, bench_low(m)), uni);
            if (ch) return 1;
        }
    }
    return 0;
}

/* k digits whose places in a line are only k cells: those cells hold nothing else */
static int hidden(pen *s, int k)
{
    int n = s->n;
    for (int u = 0; u < 2 * n; u++) {
        unsigned free_d = 0, pl[FSK_MAX + 1] = {0};
        for (int d = 1; d <= n; d++) {
            int placed = 0;
            for (int j = 0; j < n; j++) {
                int c = ucell(n, u, j);
                if (s->v[c] == d) placed = 1;
                if (s->cand[c] >> d & 1) pl[d] |= 1u << j;
            }
            if (!placed) free_d |= 1u << d;
        }
        if (bench_bits(free_d) <= k) continue;
        for (unsigned set = 2; set < 2u << n; set += 2) {
            if (bench_bits(set) != k || (set & ~free_d)) continue;
            unsigned cells = 0;
            for (unsigned m = set; m; m &= m - 1) cells |= pl[bench_low(m)];
            if (bench_bits(cells) != k) continue;
            int ch = 0;
            for (unsigned m = cells; m; m &= m - 1) ch |= remove_cand(s, ucell(n, u, bench_low(m)), ~set);
            if (ch) return 1;
        }
    }
    return 0;
}

/* a fish of size k (2 the X-wing, 3 the swordfish): a digit's places in k rows all in the same k columns, so not
 * elsewhere in those columns (and the same with columns and rows swapped) */
static int fish(pen *s, int k)
{
    int n = s->n;
    for (int d = 1; d <= n; d++)
        for (int way = 0; way < 2; way++) {
            unsigned pl[FSK_MAX];
            for (int i = 0; i < n; i++) {
                pl[i] = 0;
                for (int j = 0; j < n; j++) {
                    int c = ucell(n, way * n + i, j);
                    if (s->cand[c] >> d & 1 && !s->v[c]) pl[i] |= 1u << j;
                }
            }
            for (unsigned set = 1; set < 1u << n; set++) {
                if (bench_bits(set) != k) continue;
                unsigned uni = 0;
                int ok = 1;
                for (unsigned m = set; m; m &= m - 1) {
                    unsigned q = pl[bench_low(m)];
                    if (bench_bits(q) < 2 || bench_bits(q) > k) ok = 0;
                    uni |= q;
                }
                if (!ok || bench_bits(uni) != k) continue;
                int ch = 0;
                for (unsigned m = uni; m; m &= m - 1)
                    for (int i = 0; i < n; i++)
                        if (!(set >> i & 1)) {
                            int c = ucell(n, way * n + i, bench_low(m));
                            if (!s->v[c]) ch |= remove_cand(s, c, 1u << d);
                        }
                if (ch) return 1;
            }
        }
    return 0;
}

/* the easy ways until nothing moves; 0: a contradiction */
static int settle(pen *s)
{
    for (;;) {
        if (broken(s)) return 0;
        if (naked_single(s, NULL) || hidden_single(s, NULL) || bounds(s)) continue;
        return 1;
    }
}

/* a digit that, followed through with the easy ways alone, leaves a contradiction: not there (the cells with fewest
 * digits first) */
static int contradiction(pen *s)
{
    int n = s->n;
    for (int k = 2; k <= n; k++)
        for (int c = 0; c < n * n; c++) {
            if (s->v[c] || bench_bits(s->cand[c]) != k) continue;
            for (unsigned m = s->cand[c]; m; m &= m - 1) {
                pen t = *s;
                place(&t, c, bench_low(m));
                if (!settle(&t)) {
                    s->cand[c] &= (uint16_t)~(m & -m);
                    return 1;
                }
            }
        }
    return 0;
}

/* one step of the easiest way that works, up to the level cap: its level; -1: stuck */
static int step(pen *s, int cap, int *cell)
{
    if (naked_single(s, cell) || hidden_single(s, cell) || bounds(s)) return FSK_EASY;
    if (cap < FSK_MEDIUM) return -1;
    if (naked(s, 2) || hidden(s, 2)) return FSK_MEDIUM;
    if (cap < FSK_HARD) return -1;
    if (naked(s, 3) || hidden(s, 3) || fish(s, 2) || naked(s, 4) || hidden(s, 4) || fish(s, 3) || contradiction(s))
        return FSK_HARD;
    return -1;
}

static int rate_cap(const fsk_puz *p, int cap)
{
    pen s;
    if (!pen_load(&s, p, p->g)) return FSK_BEYOND;
    int level = 0;
    for (;;) {
        if (broken(&s)) return FSK_BEYOND;
        if (all_placed(&s)) return level;
        int l = step(&s, cap, NULL);
        if (l < 0) return FSK_BEYOND;
        if (l > level) level = l;
    }
}

int fsk_rate(const fsk_puz *p) { return rate_cap(p, FSK_HARD); }

/* ---- counting solutions: the easy ways, then a guess in the cell with the fewest digits left -------------------- */
typedef struct {
    int count, limit;
    uint8_t *sol;
    uint32_t *rng;                              /* filling a square by chance: the digits tried in a random order */
} counter;

static void search(counter *k, pen *s)
{
    if (!settle(s)) return;
    int best = -1, bn = 99, n = s->n;
    for (int c = 0; c < n * n; c++)
        if (!s->v[c] && bench_bits(s->cand[c]) < bn) bn = bench_bits(s->cand[c]), best = c;
    if (best < 0) {
        if (!k->count++ && k->sol) memcpy(k->sol, s->v, FSK_CELLS);
        return;
    }
    int ds[FSK_MAX], nd = 0;
    for (unsigned m = s->cand[best]; m; m &= m - 1) ds[nd++] = bench_low(m);
    if (k->rng) pr_shuffle(k->rng, ds, nd);
    for (int i = 0; i < nd && k->count < k->limit; i++) {
        pen t = *s;
        place(&t, best, ds[i]);
        search(k, &t);
    }
}

int fsk_count(const fsk_puz *p, int limit, uint8_t *sol)
{
    pen s;
    if (!pen_load(&s, p, p->g)) return 0;
    counter k = {0, limit, sol, NULL};
    search(&k, &s);
    return k.count;
}

static void latin(int n, uint32_t *rng, uint8_t *out)
{
    fsk_puz empty;
    memset(&empty, 0, sizeof empty);
    empty.n = n;
    pen s;
    pen_load(&s, &empty, empty.g);
    counter k = {0, 1, out, rng};
    memset(out, 0, FSK_CELLS);
    search(&k, &s);
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
static void clue_set(fsk_puz *p, const uint8_t *full, int id, int on)
{
    int n = p->n, nn = n * n;
    if (id < nn) {
        p->g[id] = on ? full[id] : 0;
        return;
    }
    int way = id >= 2 * nn, c = id - nn - way * nn, o = way ? c + n : c + 1;
    p->rel[way][c] = !on ? FSK_NONE : full[c] < full[o] ? FSK_LT : FSK_GT;
}

void fsk_make(int n, int level, uint32_t seed, fsk_puz *out, uint8_t *sol)
{
    if (n < FSK_MIN || n > FSK_MAX) n = FSK_MIN;
    if (level < FSK_EASY || level > FSK_HARD) level = FSK_EASY;
    uint32_t s = pr_seed(seed);
    int nn = n * n, best_r = -1;
    fsk_puz p, best;
    uint8_t full[FSK_CELLS], best_sol[FSK_CELLS];
    for (int attempt = 0; attempt < 200; attempt++) {
        latin(n, &s, full);
        memset(&p, 0, sizeof p);
        p.n = n;
        int ids[3 * FSK_CELLS], nid = 0, nsigns = 0, gone[3 * FSK_CELLS], ngone = 0;
        for (int id = 0; id < 3 * nn; id++) {
            int way = id >= 2 * nn, c = id - nn - way * nn;
            if (id >= nn && (way ? c / n >= n - 1 : c % n >= n - 1)) continue;   /* no neighbour there */
            clue_set(&p, full, id, 1);
            ids[nid++] = id;
            nsigns += id >= nn;
        }
        pr_shuffle(&s, ids, nn);                /* the digits taken away first: the signs carry the puzzle */
        pr_shuffle(&s, ids + nn, nid - nn);
        for (int i = 0; i < nid; i++) {
            if (ids[i] >= nn && nsigns <= n) break;   /* a few signs always stay */
            clue_set(&p, full, ids[i], 0);
            /* hard: only the solution's uniqueness here (the grader's slowest ways are tried once, below) */
            if (fsk_count(&p, 2, NULL) != 1 || (level < FSK_HARD && rate_cap(&p, level) > level)) {
                clue_set(&p, full, ids[i], 1);
                continue;
            }
            gone[ngone++] = ids[i];
            nsigns -= ids[i] >= nn;
        }
        int r = fsk_rate(&p);
        while (r == FSK_BEYOND && ngone) {      /* beyond the grader: the last clues taken given back */
            clue_set(&p, full, gone[--ngone], 1);
            r = fsk_rate(&p);
        }
        if (r == level) {
            *out = p;
            memcpy(sol, full, FSK_CELLS);
            return;
        }
        if (r <= level && r > best_r) {         /* the nearest yet, should none be found */
            best_r = r;
            best = p;
            memcpy(best_sol, full, FSK_CELLS);
        }
    }
    *out = best;
    memcpy(sol, best_sol, FSK_CELLS);
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
void fsk_load(fsk_game *g, int level, const fsk_puz *p, const uint8_t *sol)
{
    memset(g, 0, sizeof *g);
    g->n = p->n;
    g->level = level;
    g->p = *p;
    memcpy(g->sol, sol, FSK_CELLS);
    memcpy(g->val, p->g, FSK_CELLS);
}

void fsk_new(fsk_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= FSK_VARIANTS) variant = 0;
    fsk_puz p;
    uint8_t sol[FSK_CELLS];
    fsk_make(fsk_size_of(variant), fsk_level_of(variant), seed, &p, sol);
    fsk_load(g, fsk_level_of(variant), &p, sol);
    g->seed = seed;
}

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const fsk_game *g)
{
    int k = 0;
    for (int c = 0; c < g->n * g->n; c++)
        k += g->val[c] && g->val[c] != g->sol[c];
    return k;
}

static void change(fsk_game *g, int c, int val, unsigned notes)
{
    if (g->val[c] == val && g->notes[c] == notes) return;
    bench_log(&g->hist, c, (uint32_t)(g->val[c] | g->notes[c] << 8));
    g->val[c] = (uint8_t)val;
    g->notes[c] = (uint8_t)notes;
}

static int in_grid(const fsk_game *g, int c) { return c >= 0 && c < g->n * g->n; }

int fsk_put(fsk_game *g, int c, int d)
{
    int n = g->n;
    if (!in_grid(g, c) || d < 0 || d > n || g->p.g[c] || g->val[c] == d) return 0;
    bench_begin(&g->hist, 2 * n + 2);
    change(g, c, d, 0);
    if (d)
        for (int j = 0; j < n; j++) {
            int a = c / n * n + j, b = j * n + c % n;
            if (a != c && g->notes[a] >> d & 1) change(g, a, g->val[a], g->notes[a] & ~(1u << d));
            if (b != c && g->notes[b] >> d & 1) change(g, b, g->val[b], g->notes[b] & ~(1u << d));
        }
    return bench_done(&g->hist, wrongs(g));
}

int fsk_note(fsk_game *g, int c, int d)
{
    if (!in_grid(g, c) || d < 1 || d > g->n || g->p.g[c] || g->val[c]) return 0;
    bench_begin(&g->hist, 1);
    change(g, c, 0, g->notes[c] ^ (1u << d));
    return bench_done(&g->hist, wrongs(g));
}

int fsk_erase(fsk_game *g, int c)
{
    if (!in_grid(g, c) || g->p.g[c]) return 0;
    if (g->val[c]) return fsk_put(g, c, 0);
    if (!g->notes[c]) return 0;
    bench_begin(&g->hist, 1);
    change(g, c, 0, 0);
    return bench_done(&g->hist, wrongs(g));
}

static int hint_step(fsk_game *g)
{
    int n = g->n;
    for (int c = 0; c < n * n; c++)             /* a wrong digit first */
        if (g->val[c] && g->val[c] != g->sol[c]) return fsk_put(g, c, g->sol[c]) ? c : -1;
    pen s;
    if (!pen_load(&s, &g->p, g->val)) return -1;
    int c = -1;
    for (int guard = 0; guard < 400 && c < 0; guard++) {   /* the easiest ways until a cell is found */
        int cell = -1;
        if (broken(&s) || step(&s, FSK_HARD, &cell) < 0) break;
        if (cell >= 0 && !g->val[cell]) c = cell;
    }
    if (c < 0) {                                /* stuck: the empty cell with the fewest digits left */
        int bn = 99;
        for (int i = 0; i < n * n; i++)
            if (!g->val[i] && bench_bits(s.cand[i]) < bn) bn = bench_bits(s.cand[i]), c = i;
    }
    if (c < 0) return -1;
    return fsk_put(g, c, g->sol[c]) ? c : -1;
}

static void put_back(void *ctx, int cell, uint32_t was)
{
    fsk_game *g = ctx;
    g->val[cell] = (uint8_t)(was & 0xff);
    g->notes[cell] = (uint8_t)(was >> 8);
}

int fsk_hint(fsk_game *g)
{
    return bench_hinted(&g->hist, hint_step(g));
}

int fsk_undo(fsk_game *g)
{
    int c = bench_undo(&g->hist, put_back, g);
    bench_undone(&g->hist, wrongs(g));
    return c;
}


int fsk_clash(const fsk_game *g, int c)
{
    int n = g->n, d = g->val[c];
    if (!in_grid(g, c) || !d) return 0;
    for (int j = 0; j < n; j++) {
        int a = c / n * n + j, b = j * n + c % n;
        if ((a != c && g->val[a] == d) || (b != c && g->val[b] == d)) return 1;
    }
    for (int way = 0; way < 2; way++)          /* the signs on its four sides */
        for (int side = 0; side < 2; side++) {
            int first = side ? c : c - (way ? n : 1);
            if (first < 0 || (way ? first / n >= n - 1 : first % n >= n - 1) || (!way && first / n != c / n)) continue;
            int r = g->p.rel[way][first], o = first + (way ? n : 1);
            int a = g->val[first], b = g->val[o];
            if (!r || !a || !b) continue;
            if ((r == FSK_LT && a >= b) || (r == FSK_GT && a <= b)) return 1;
        }
    return 0;
}

int fsk_solved(const fsk_game *g)
{
    for (int c = 0; c < g->n * g->n; c++)
        if (g->val[c] != g->sol[c]) return 0;
    return 1;
}

int fsk_count_digit(const fsk_game *g, int d)
{
    int k = 0;
    for (int c = 0; c < g->n * g->n; c++) k += g->val[c] == d;
    return k;
}

int fsk_script(fsk_game *g, const char *s)
{
    int n = g->n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (fsk_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (!g->val[c]) { fsk_put(g, c, g->sol[c]); k--; }
            continue;
        }
        int len, c = bench_cell(s, n, n, &len);
        if (c < 0 || (s[len] != '=' && s[len] != '\'') || s[len + 1] < '0' || s[len + 1] > '9') return 0;
        int ok = s[len] == '=' ? fsk_put(g, c, s[len + 1] - '0') : fsk_note(g, c, s[len + 1] - '0');
        if (!ok) return 0;
        s += len + 2;
    }
    return 1;
}
