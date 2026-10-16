/*
 * The Sudoku variants' rules, shared by Killer Sudoku, Jigsaw Sudoku and Diagonal Sudoku (killersudoku*,
 * jigsawsudoku*, diagonalsudoku*): a 9 x 9 grid whose units (rows, columns, nine regions, maybe the two long
 * diagonals) each hold the digits 1..9 once, and maybe cages (Killer: cells whose digits add up to the cage's sum, no
 * digit twice in a cage). No SDL: the rules and their tests use it. The screen's side is sudokuvar_ui.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions (the build takes a game's own files only, and a test links its
 * game's _rules.c alone), and without any state of its own: the menu's pictures are made on several threads.
 *
 * - The shape (sv_shape): the regions (the 3 x 3 boxes, or a jigsaw's), the diagonals, the cages; sv_shape_init makes
 *   the units, each cell's peers (its units' cells and its cage's: never the same digit) and the sums (the cages', and
 *   those the rule of 45 gives: a unit's digits add up to 45, so the cells of a unit outside the cages wholly inside
 *   it, or the cells outside it of the cages that reach into it, add up to what is left).
 * - The solver, on the bench (bench.h): its deductions in three tiers, the way Sudoku's are:
 *   tier 0: the singles (a cell's digit taken from its peers; a digit with one place left in a unit), and each cage's
 *     ways of making its sum (a cell keeps only the digits some way gives it);
 *   tier 1: locked candidates (a digit's places in a unit all in another unit: not elsewhere in that one), naked and
 *     hidden pairs, the digits every way of a cage uses (not elsewhere in a unit holding the whole cage), the rule
 *     of 45 on one unit;
 *   tier 2: naked and hidden triples and quads, the X-wing and the swordfish, the XY-wing, the rule of 45 on two or
 *     three rows or columns side by side.
 *   The grade (sv_grade): SV_EASY tier 0 alone, SV_MEDIUM tiers 0-1, SV_HARD tier 2 too, SV_EXPERT a digit tried and
 *   followed (tiers 0-1) to a contradiction; SV_STUCK beyond. A puzzle graded 0..3 has that one solution.
 * - The makers: a full grid filled by chance; then clues taken away in pairs round the centre (sv_make_givens), or
 *   neighbouring cages merged (sv_make_cages), while the puzzle stays within its level.
 * - The player's grid (sv_game): digits, notes, clashes, hints, undo and the stars' counts (bench_hist).
 * - The levels' code: the regions "123..." (81 digits 1..9), the cages' joins and sums, the clues ('.' or 1..9).
 * - The notation (script, the screenshots, the tests): a cell is its column letter a..i and its row 1..9 from the top;
 *   "e5=7" puts 7 in e5, "e5=0" empties it, "e5'7" turns the note 7 on or off, "h" a hint, "+n" fills the next n
 *   empty cells (reading order) with their digits.
 */
#ifndef SUDOKUVAR_H
#define SUDOKUVAR_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "games/bench.h"
#include "games/puzrand.h"

#define SV_CELLS 81
#define SV_ALL 0x3feu                           /* the digits 1..9 as bits */
#define SV_UNITS 29                             /* rows, columns, regions, two diagonals */
#define SV_PEERS 40
#define SV_CAGE 9                               /* the most cells in a cage (the makers keep them smaller) */
#define SV_DERIVED 4                            /* the most cells in a sum the rule of 45 gives */
#define SV_SUMS 200
#define SV_LEVELS 4                             /* the difficulties */

enum { SV_EASY, SV_MEDIUM, SV_HARD, SV_EXPERT, SV_STUCK };

typedef struct {
    uint8_t n, sum;
    uint8_t cell[SV_CAGE];                      /* in reading order: cell[0] carries the sum */
} sv_cage;

/* a sum some cells make: a cage's (no digit twice), or one the rule of 45 gives (no digit twice where two cells see
 * each other) */
typedef struct {
    uint8_t n, sum, tier, distinct;             /* distinct: every two of its cells see each other */
    uint8_t cell[SV_CAGE];
    uint16_t same[SV_CAGE];                     /* bit j: cell j sees cell i */
    uint8_t nunits, unit[3];                    /* the units holding all its cells */
} sv_sum;

typedef struct {
    uint8_t region[SV_CELLS];                   /* 0..8 */
    uint8_t diag;                               /* 1: the two long diagonals are units */
    int ncages;                                 /* 0: no cages */
    uint8_t cage_of[SV_CELLS];
    sv_cage cage[SV_CELLS];
    /* made by sv_shape_init */
    int nunits;
    uint8_t unit[SV_UNITS][9];                  /* rows 0..8, columns 9..17, regions 18..26, diagonals 27, 28 */
    uint8_t nunits_of[SV_CELLS], units_of[SV_CELLS][5];
    uint8_t npeers[SV_CELLS], peer[SV_CELLS][SV_PEERS];
    uint64_t see[SV_CELLS][2], umask[SV_UNITS][2];
    int nsums;
    sv_sum sum[SV_SUMS];
} sv_shape;

static inline int sv_in(const uint64_t *m, int c) { return (int)(m[c >> 6] >> (c & 63) & 1); }
static inline void sv_add(uint64_t *m, int c) { m[c >> 6] |= 1ull << (c & 63); }
static inline int sv_sees(const sv_shape *s, int a, int b) { return sv_in(s->see[a], b); }
static inline int sv_box(int c) { return c / 27 * 3 + c % 9 / 3; }

/* the regions: the 3 x 3 boxes */
static inline void sv_boxes(sv_shape *s)
{
    for (int c = 0; c < SV_CELLS; c++) s->region[c] = (uint8_t)sv_box(c);
}

/* ---- the shape: units, peers, sums ------------------------------------------------------------------------------- */
static inline void sv_sum_units(const sv_shape *s, sv_sum *k)
{
    k->nunits = 0;
    for (int t = 0; t < s->nunits_of[k->cell[0]]; t++) {
        int u = s->units_of[k->cell[0]][t], all = 1;
        for (int i = 1; i < k->n && all; i++) all = sv_in(s->umask[u], k->cell[i]);
        if (all && k->nunits < 3) k->unit[k->nunits++] = (uint8_t)u;
    }
}

static inline void sv_sum_add(sv_shape *s, const uint64_t *cells, int sum, int tier)
{
    sv_sum k;
    memset(&k, 0, sizeof k);
    for (int c = 0; c < SV_CELLS; c++)
        if (sv_in(cells, c)) k.cell[k.n++] = (uint8_t)c;
    if (!k.n || k.n > SV_DERIVED || sum < k.n || sum > 9 * k.n || s->nsums >= SV_SUMS) return;
    k.sum = (uint8_t)sum;
    k.tier = (uint8_t)tier;
    k.distinct = 1;
    for (int i = 0; i < k.n; i++)
        for (int j = 0; j < k.n; j++)
            if (i != j && sv_sees(s, k.cell[i], k.cell[j])) k.same[i] |= (uint16_t)(1u << j);
            else if (i != j) k.distinct = 0;
    sv_sum_units(s, &k);
    for (int i = 0; i < s->nsums; i++)          /* the same cells already: nothing new */
        if (s->sum[i].n == k.n && !memcmp(s->sum[i].cell, k.cell, k.n)) return;
    s->sum[s->nsums++] = k;
}

/* the rule of 45 on the cells m (k units' worth: their digits add up to 45 k): what is left inside it once the cages
 * wholly inside are taken away, what reaches outside it of the cages reaching into it */
static inline void sv_derive(sv_shape *s, const uint64_t *m, int k, int tier)
{
    uint64_t in[2] = {0, 0}, out[2] = {0, 0};
    int sum_in = 0, sum_touch = 0;
    for (int q = 0; q < s->ncages; q++) {
        const sv_cage *g = &s->cage[q];
        int inside = 0;
        for (int i = 0; i < g->n; i++) inside += sv_in(m, g->cell[i]);
        if (!inside) continue;
        sum_touch += g->sum;
        if (inside == g->n) {
            sum_in += g->sum;
            for (int i = 0; i < g->n; i++) sv_add(in, g->cell[i]);
        }
        for (int i = 0; i < g->n; i++)
            if (!sv_in(m, g->cell[i])) sv_add(out, g->cell[i]);
    }
    uint64_t left[2] = {m[0] & ~in[0], m[1] & ~in[1]};
    sv_sum_add(s, left, 45 * k - sum_in, tier);
    sv_sum_add(s, out, sum_touch - 45 * k, tier);
}

/* the units, the peers and the sums, from the regions, diag and the cages */
static inline void sv_shape_init(sv_shape *s)
{
    int nu = 0;
    for (int i = 0; i < 9; i++, nu++)
        for (int j = 0; j < 9; j++) s->unit[nu][j] = (uint8_t)(i * 9 + j);
    for (int i = 0; i < 9; i++, nu++)
        for (int j = 0; j < 9; j++) s->unit[nu][j] = (uint8_t)(j * 9 + i);
    for (int r = 0; r < 9; r++, nu++)
        for (int c = 0, j = 0; c < SV_CELLS && j < 9; c++)
            if (s->region[c] == r) s->unit[nu][j++] = (uint8_t)c;
    if (s->diag) {
        for (int j = 0; j < 9; j++) s->unit[nu][j] = (uint8_t)(j * 10);
        nu++;
        for (int j = 0; j < 9; j++) s->unit[nu][j] = (uint8_t)(8 + j * 8);
        nu++;
    }
    s->nunits = nu;
    memset(s->umask, 0, sizeof s->umask);
    memset(s->nunits_of, 0, sizeof s->nunits_of);
    for (int u = 0; u < nu; u++)
        for (int j = 0; j < 9; j++) {
            int c = s->unit[u][j];
            sv_add(s->umask[u], c);
            if (s->nunits_of[c] < 5) s->units_of[c][s->nunits_of[c]++] = (uint8_t)u;
        }
    for (int c = 0; c < SV_CELLS; c++) {
        uint64_t *m = s->see[c];
        m[0] = m[1] = 0;
        for (int t = 0; t < s->nunits_of[c]; t++) {
            m[0] |= s->umask[s->units_of[c][t]][0];
            m[1] |= s->umask[s->units_of[c][t]][1];
        }
        if (s->ncages) {
            const sv_cage *g = &s->cage[s->cage_of[c]];
            for (int i = 0; i < g->n; i++) sv_add(m, g->cell[i]);
        }
        m[c >> 6] &= ~(1ull << (c & 63));
        s->npeers[c] = 0;
        for (int p = 0; p < SV_CELLS; p++)
            if (sv_in(m, p) && s->npeers[c] < SV_PEERS) s->peer[c][s->npeers[c]++] = (uint8_t)p;
    }
    s->nsums = 0;
    for (int q = 0; q < s->ncages && s->nsums < SV_SUMS; q++) {
        sv_sum *k = &s->sum[s->nsums++];
        memset(k, 0, sizeof *k);
        k->n = s->cage[q].n;
        k->sum = s->cage[q].sum;
        k->distinct = 1;
        memcpy(k->cell, s->cage[q].cell, k->n);
        for (int i = 0; i < k->n; i++) k->same[i] = (uint16_t)(((1u << k->n) - 1) & ~(1u << i));
        sv_sum_units(s, k);
    }
    if (!s->ncages) return;
    for (int u = 0; u < 27; u++) sv_derive(s, s->umask[u], 1, 1);
    for (int way = 0; way < 2; way++)           /* two or three rows (columns) side by side */
        for (int k = 2; k <= 3; k++)
            for (int i = 0; i + k <= 9; i++) {
                uint64_t m[2] = {0, 0};
                for (int j = 0; j < k; j++) {
                    m[0] |= s->umask[way * 9 + i + j][0];
                    m[1] |= s->umask[way * 9 + i + j][1];
                }
                sv_derive(s, m, k, 2);
            }
}

/* the cages from a labelling of the cells (lab: any number per cage, the same within one) and the solution's sums;
 * 0: a cage too big */
static inline int sv_cages_from(sv_shape *s, const uint8_t *lab, const uint8_t *sol)
{
    int map[256];
    for (int i = 0; i < 256; i++) map[i] = -1;
    s->ncages = 0;
    for (int c = 0; c < SV_CELLS; c++) {
        int q = map[lab[c]];
        if (q < 0) {
            q = map[lab[c]] = s->ncages++;
            s->cage[q].n = 0;
            s->cage[q].sum = 0;
        }
        sv_cage *g = &s->cage[q];
        if (g->n >= SV_CAGE) return 0;
        g->cell[g->n++] = (uint8_t)c;
        g->sum = (uint8_t)(g->sum + sol[c]);
        s->cage_of[c] = (uint8_t)q;
    }
    return 1;
}

/* ---- a sum's ways ------------------------------------------------------------------------------------------------ */
typedef struct {
    const uint16_t *m;                          /* each cell's digits */
    int k;
    unsigned cover;
    uint16_t uni[SV_CAGE];                      /* the digits each cell takes in some way */
    unsigned must;                              /* the digits every way uses */
    int any;
} sv_ways;

/* the set of digits S (k of them, one a cell): can the cells take them, and which cell can take which */
static inline void sv_ways_try(sv_ways *w, unsigned S)
{
    int ds[9], nd = 0, k = w->k;
    for (unsigned x = S; x; x &= x - 1) ds[nd++] = bench_low(x);
    unsigned lm[SV_CAGE];
    for (int i = 0; i < k; i++) {
        lm[i] = 0;
        for (int j = 0; j < k; j++)
            if (w->m[i] >> ds[j] & 1) lm[i] |= 1u << j;
        if (!lm[i]) return;
    }
    uint8_t f[512], b[512];                     /* f[x]: cells 0.. take the digits x; b[x]: cells ..k-1 take them */
    unsigned full = (1u << k) - 1;
    memset(f, 0, full + 1);
    memset(b, 0, full + 1);
    f[0] = b[0] = 1;
    for (unsigned x = 0; x <= full; x++) {
        int i = bench_bits(x);
        if (i >= k) continue;
        if (f[x])
            for (unsigned y = lm[i] & ~x; y; y &= y - 1) f[x | (y & -y)] = 1;
        if (b[x])
            for (unsigned y = lm[k - 1 - i] & ~x; y; y &= y - 1) b[x | (y & -y)] = 1;
    }
    if (!f[full]) return;
    w->any = 1;
    w->must &= S;
    for (unsigned x = 0; x < full; x++) {
        if (!f[x]) continue;
        int i = bench_bits(x);
        for (unsigned y = lm[i] & ~x; y; y &= y - 1) {
            unsigned bit = y & -y;
            if (b[full & ~x & ~bit]) w->uni[i] |= (uint16_t)(1u << ds[bench_low(bit)]);
        }
    }
}

/* the sets of `left` digits from `from` up (of the cells' digits) adding up to sum */
static inline void sv_ways_sets(sv_ways *w, int from, int left, int sum, unsigned S)
{
    if (!left) {
        if (!sum) sv_ways_try(w, S);
        return;
    }
    for (int d = from; d <= 9; d++) {
        if (left * d + left * (left - 1) / 2 > sum) break;
        if (!(w->cover >> d & 1)) continue;
        if (d + (left - 1) * 9 - (left - 1) * (left - 2) / 2 < sum) continue;
        sv_ways_sets(w, d + 1, left - 1, sum - d, S | 1u << d);
    }
}

/* the cells may hold a digit twice where they do not see each other: every filling tried (a few cells only) */
static inline void sv_ways_any(sv_ways *w, const sv_sum *k, int i, int left, int *v)
{
    if (i == k->n) {
        if (left) return;
        w->any = 1;
        for (int j = 0; j < k->n; j++) w->uni[j] |= (uint16_t)(1u << v[j]);
        return;
    }
    unsigned m = w->m[i];
    for (int j = 0; j < i; j++)
        if (k->same[i] >> j & 1) m &= ~(1u << v[j]);
    int rest = k->n - i - 1;
    for (; m; m &= m - 1) {
        int d = bench_low(m);
        if (d + rest > left) break;
        if (d + 9 * rest < left) continue;
        v[i] = d;
        sv_ways_any(w, k, i + 1, left - d, v);
    }
}

/* the sum's ways under the candidates d: 0 when it has none */
static inline int sv_sum_ways(const sv_sum *k, const uint16_t *d, sv_ways *w)
{
    uint16_t m[SV_CAGE];
    for (int i = 0; i < k->n; i++) m[i] = d[k->cell[i]];
    memset(w, 0, sizeof *w);
    w->m = m;
    w->k = k->n;
    w->must = SV_ALL;
    for (int i = 0; i < k->n; i++) w->cover |= m[i];
    if (k->distinct) {
        sv_ways_sets(w, 1, k->n, k->sum, 0);
    } else {
        int v[SV_CAGE];
        sv_ways_any(w, k, 0, k->sum, v);
        w->must = 0;
    }
    w->m = NULL;
    if (!w->any) w->must = 0;
    return w->any;
}

/* ---- the deductions ---------------------------------------------------------------------------------------------- */
static inline int sv_cut(bench_grid *g, int c, unsigned keep)
{
    unsigned m = g->d[c] & keep;
    if (m == g->d[c]) return 0;
    g->d[c] = (uint16_t)m;
    return 1;
}

/* the sums of a tier: each cell keeps the digits some way gives it; with musts, the digits every way uses leave the
 * rest of a unit holding the whole sum. -1 a sum with no way */
static inline int sv_sums(bench_grid *g, const sv_shape *s, int tier, int musts)
{
    int found = 0;
    sv_ways w;
    for (int q = 0; q < s->nsums; q++) {
        const sv_sum *k = &s->sum[q];
        if (k->tier != tier) continue;
        if (!sv_sum_ways(k, g->d, &w)) return -1;
        for (int i = 0; i < k->n; i++) found |= sv_cut(g, k->cell[i], w.uni[i]);
        if (!musts || !w.must) continue;
        for (int t = 0; t < k->nunits; t++)
            for (int j = 0; j < 9; j++) {
                int c = s->unit[k->unit[t]][j], mine = 0;
                for (int i = 0; i < k->n && !mine; i++) mine = k->cell[i] == c;
                if (!mine) found |= sv_cut(g, c, ~w.must);
            }
        if (found) return 1;
    }
    return found;
}

/* a digit's places in a unit all in another unit: not elsewhere in that one */
static inline int sv_locked(bench_grid *g, const sv_shape *s)
{
    for (int u = 0; u < s->nunits; u++)
        for (int d = 1; d <= 9; d++) {
            unsigned bit = 1u << d;
            uint64_t p[2] = {0, 0};
            int first = -1, n = 0, done = 0;
            for (int j = 0; j < 9; j++) {
                int c = s->unit[u][j];
                if (!(g->d[c] & bit)) continue;
                if (bench_one(g->d[c])) done = 1;
                sv_add(p, c);
                if (first < 0) first = c;
                n++;
            }
            if (done || n < 2) continue;
            for (int t = 0; t < s->nunits_of[first]; t++) {
                int v = s->units_of[first][t], ch = 0;
                if (v == u || (p[0] & ~s->umask[v][0]) || (p[1] & ~s->umask[v][1])) continue;
                for (int j = 0; j < 9; j++) {
                    int c = s->unit[v][j];
                    if (!sv_in(p, c)) ch |= sv_cut(g, c, ~bit);
                }
                if (ch) return 1;
            }
        }
    return 0;
}

/* the next set of as many bits (the sets of n of m in turn: from (1 << n) - 1 while below 1 << m) */
static inline unsigned sv_next_set(unsigned x)
{
    unsigned c = x & -x, r = x + c;
    return (((r ^ x) >> 2) / c) | r;
}

/* n cells of a unit holding only n digits between them: those digits not elsewhere in the unit */
static inline int sv_naked(bench_grid *g, const sv_shape *s, int n)
{
    for (int u = 0; u < s->nunits; u++) {
        int cells[9], m = 0;
        for (int j = 0; j < 9; j++)
            if (!bench_one(g->d[s->unit[u][j]])) cells[m++] = s->unit[u][j];
        if (m <= n) continue;
        for (unsigned set = (1u << n) - 1; set < 1u << m; set = sv_next_set(set)) {
            unsigned uni = 0;
            for (int i = 0; i < m; i++)
                if (set >> i & 1) uni |= g->d[cells[i]];
            if (bench_bits(uni) < n) return -1;
            if (bench_bits(uni) != n) continue;
            int ch = 0;
            for (int i = 0; i < m; i++)
                if (!(set >> i & 1)) ch |= sv_cut(g, cells[i], ~uni);
            if (ch) return 1;
        }
    }
    return 0;
}

/* n digits whose places in a unit are only n cells: those cells hold nothing else */
static inline int sv_hidden(bench_grid *g, const sv_shape *s, int n)
{
    for (int u = 0; u < s->nunits; u++) {
        unsigned free_d = 0, decided = 0, pl[10] = {0};
        for (int j = 0; j < 9; j++) {
            unsigned m = g->d[s->unit[u][j]];
            if (bench_one(m)) decided |= m;
            else
                for (unsigned x = m; x; x &= x - 1) pl[bench_low(x)] |= 1u << j;
        }
        for (int d = 1; d <= 9; d++)
            if (pl[d] && !(decided >> d & 1)) free_d |= 1u << d;
        int dg[9], fm = 0;
        for (unsigned x = free_d; x; x &= x - 1) dg[fm++] = bench_low(x);
        if (fm <= n) continue;
        for (unsigned pick = (1u << n) - 1; pick < 1u << fm; pick = sv_next_set(pick)) {
            unsigned cells = 0, set = 0;
            for (unsigned x = pick; x; x &= x - 1) cells |= pl[dg[bench_low(x)]], set |= 1u << dg[bench_low(x)];
            if (bench_bits(cells) < n) return -1;
            if (bench_bits(cells) != n) continue;
            int ch = 0;
            for (unsigned x = cells; x; x &= x - 1) ch |= sv_cut(g, s->unit[u][bench_low(x)], set);
            if (ch) return 1;
        }
    }
    return 0;
}

/* a fish of size n (2 the X-wing, 3 the swordfish): a digit's places in n rows all in the same n columns, so not
 * elsewhere in those columns (and the same with columns and rows swapped) */
static inline int sv_fish(bench_grid *g, int n)
{
    for (int d = 1; d <= 9; d++)
        for (int way = 0; way < 2; way++) {
            unsigned pl[9];
            int ln[9], nl = 0;                  /* the lines with 2..n places */
            for (int i = 0; i < 9; i++) {
                pl[i] = 0;
                for (int j = 0; j < 9; j++) {
                    int c = way ? j * 9 + i : i * 9 + j;
                    if (g->d[c] >> d & 1 && !bench_one(g->d[c])) pl[i] |= 1u << j;
                }
                if (bench_bits(pl[i]) >= 2 && bench_bits(pl[i]) <= n) ln[nl++] = i;
            }
            for (unsigned pick = (1u << n) - 1; pick < 1u << nl; pick = sv_next_set(pick)) {
                unsigned uni = 0, set = 0;
                for (unsigned k = pick; k; k &= k - 1) uni |= pl[ln[bench_low(k)]], set |= 1u << ln[bench_low(k)];
                if (bench_bits(uni) != n) continue;
                int ch = 0;
                for (unsigned k = uni; k; k &= k - 1)
                    for (int i = 0; i < 9; i++) {
                        if (set >> i & 1) continue;
                        int c = way ? bench_low(k) * 9 + i : i * 9 + bench_low(k);
                        if (!bench_one(g->d[c])) ch |= sv_cut(g, c, ~(1u << d));
                    }
                if (ch) return 1;
            }
        }
    return 0;
}

/* a cell of two digits xy seeing a cell xz and a cell yz: whichever it holds, one of them is z; so no cell seeing
 * both of them is z */
static inline int sv_xywing(bench_grid *g, const sv_shape *s)
{
    for (int p = 0; p < SV_CELLS; p++) {
        unsigned pm = g->d[p];
        if (bench_bits(pm) != 2) continue;
        for (int i = 0; i < s->npeers[p]; i++) {
            int a = s->peer[p][i];
            unsigned am = g->d[a];
            if (bench_bits(am) != 2 || bench_bits(am & pm) != 1) continue;
            unsigned z = am & ~pm, bm = (pm & ~am) | z;
            for (int j = 0; j < s->npeers[p]; j++) {
                int b = s->peer[p][j];
                if (b == a || g->d[b] != bm) continue;
                int ch = 0;
                for (int c = 0; c < SV_CELLS; c++)
                    if (c != p && c != a && c != b && sv_sees(s, c, a) && sv_sees(s, c, b)) ch |= sv_cut(g, c, ~z);
                if (ch) return 1;
            }
        }
    }
    return 0;
}

/* one pass of a tier (bench_rules.deduce): 1 something found, 0 nothing, -1 a contradiction */
static inline int sv_deduce(bench_grid *g, int tier, void *ctx)
{
    const sv_shape *s = ctx;
    int r;
    if (tier == 0) {
        int found = 0;
        for (int c = 0; c < SV_CELLS; c++) {
            if (!bench_one(g->d[c])) continue;
            for (int i = 0; i < s->npeers[c]; i++) {
                int p = s->peer[c][i];
                if (!(g->d[p] & g->d[c])) continue;
                if (bench_one(g->d[p])) return -1;
                g->d[p] &= (uint16_t)~g->d[c];
                found = 1;
            }
        }
        if (found) return 1;
        for (int u = 0; u < s->nunits; u++)
            for (int d = 1; d <= 9; d++) {
                int n = 0, at = -1;
                for (int j = 0; j < 9; j++)
                    if (g->d[s->unit[u][j]] >> d & 1) n++, at = s->unit[u][j];
                if (!n) return -1;
                if (n == 1 && !bench_one(g->d[at])) {
                    g->d[at] = (uint16_t)(1u << d);
                    return 1;
                }
            }
        return sv_sums(g, s, 0, 0);
    }
    if (tier == 1) {
        if (sv_locked(g, s)) return 1;
        if ((r = sv_naked(g, s, 2)) != 0 || (r = sv_hidden(g, s, 2)) != 0) return r;
        if ((r = sv_sums(g, s, 0, 1)) != 0) return r;   /* the cages' musts */
        return sv_sums(g, s, 1, 1);
    }
    if ((r = sv_naked(g, s, 3)) != 0 || (r = sv_hidden(g, s, 3)) != 0 || (r = sv_fish(g, 2)) != 0) return r;
    if ((r = sv_naked(g, s, 4)) != 0 || (r = sv_hidden(g, s, 4)) != 0 || (r = sv_fish(g, 3)) != 0) return r;
    if (sv_xywing(g, s)) return 1;
    return sv_sums(g, s, 2, 1);
}

/* every cell decided: 1 when each unit holds every digit and each cage its sum (and no digit twice) */
static inline int sv_valid(const bench_grid *g, void *ctx)
{
    const sv_shape *s = ctx;
    for (int u = 0; u < s->nunits; u++) {
        unsigned m = 0;
        for (int j = 0; j < 9; j++) m |= g->d[s->unit[u][j]];
        if (m != SV_ALL) return 0;
    }
    for (int q = 0; q < s->ncages; q++) {
        const sv_cage *k = &s->cage[q];
        unsigned m = 0;
        int sum = 0;
        for (int i = 0; i < k->n; i++) m |= g->d[k->cell[i]], sum += bench_low(g->d[k->cell[i]]);
        if (sum != k->sum || bench_bits(m) != k->n) return 0;
    }
    return 1;
}

/* ---- grading ----------------------------------------------------------------------------------------------------- */
static inline void sv_rules(bench_rules *r, const sv_shape *s, long *work)
{
    memset(r, 0, sizeof *r);
    r->vars = SV_CELLS;
    r->tiers = 3;
    r->probe_tiers = 2;
    r->order = BENCH_PAIRS;
    r->deduce = sv_deduce;
    r->valid = sv_valid;
    r->ctx = (void *)s;
    r->work = work;
}

static inline void sv_grid(const uint8_t *givens, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < SV_CELLS; c++) g->d[c] = givens && givens[c] ? (uint16_t)(1u << givens[c]) : (uint16_t)SV_ALL;
}

/* the grade of the puzzle (givens: its clues, 0 empty; NULL none) no further than cap: SV_EASY..SV_EXPERT, SV_STUCK
 * (beyond cap, stuck, or past the budget of passes, 0 none), -1 no solution. Graded 0..3: sol (may be NULL) its one
 * solution. *passes (may be NULL): the solver's work */
static inline int sv_grade(const sv_shape *s, const uint8_t *givens, int cap, uint8_t *sol, long *passes, long budget)
{
    bench_rules r;
    bench_grid g;
    sv_rules(&r, s, NULL);
    r.budget = budget;
    r.no_trials = 1;
    sv_grid(givens, &g);
    int res = bench_grade(&g, &r, cap < SV_HARD ? cap : BENCH_HARD), grade = res;
    long p = r.passes;
    if (res == BENCH_GUESS && cap >= SV_EXPERT && !bench_over(&r)) {
        r.no_trials = 0;                        /* from where the reasoning stopped: a digit tried */
        res = bench_grade(&g, &r, BENCH_HARD);
        p += r.passes;
        grade = SV_EXPERT;
    }
    if (passes) *passes = p;
    if (res == BENCH_NONE) return -1;
    if (res == BENCH_GUESS) return SV_STUCK;
    if (sol)
        for (int c = 0; c < SV_CELLS; c++) sol[c] = (uint8_t)bench_low(g.d[c]);
    return grade;
}

/* the solutions counted up to limit (a plain search, every tier between the guesses: the tests' check) */
static inline int sv_count(const sv_shape *s, const uint8_t *givens, int limit, uint8_t *sol)
{
    bench_rules r;
    bench_grid g, out;
    long work = 0;
    sv_rules(&r, s, &work);
    r.tiers = 2;
    sv_grid(givens, &g);
    int n = bench_search(&g, &r, limit, &out, NULL, 0);
    if (n && sol)
        for (int c = 0; c < SV_CELLS; c++) sol[c] = (uint8_t)bench_low(out.d[c]);
    return n;
}

/* a full grid filled by chance (the units only, not the cages); 0: none found within the budget */
static inline int sv_fill(const sv_shape *s, uint32_t *rng, uint8_t *out, long budget)
{
    sv_shape t = *s;
    t.ncages = 0;
    t.nsums = 0;
    for (int c = 0; c < SV_CELLS; c++) {        /* the cages' mates out of the peers */
        uint64_t *m = t.see[c];
        m[0] = m[1] = 0;
        for (int k = 0; k < t.nunits_of[c]; k++) {
            m[0] |= t.umask[t.units_of[c][k]][0];
            m[1] |= t.umask[t.units_of[c][k]][1];
        }
        m[c >> 6] &= ~(1ull << (c & 63));
        t.npeers[c] = 0;
        for (int p = 0; p < SV_CELLS; p++)
            if (sv_in(m, p) && t.npeers[c] < SV_PEERS) t.peer[c][t.npeers[c]++] = (uint8_t)p;
    }
    bench_rules r;
    bench_grid g, sol;
    long work = 0;
    sv_rules(&r, &t, &work);
    r.tiers = 1;
    sv_grid(NULL, &g);
    if (bench_search(&g, &r, 1, &sol, rng, budget) != 1) return 0;
    for (int c = 0; c < SV_CELLS; c++) out[c] = (uint8_t)bench_low(sol.d[c]);
    return 1;
}

/* ---- the makers -------------------------------------------------------------------------------------------------- */
/* the clues a level keeps at least (the easy ones many) */
static inline int sv_clues_floor(int level) { return level == SV_EASY ? 36 : level == SV_MEDIUM ? 28 : 0; }

/* clues taken away from the full grid full, two at a time opposite each other round the centre, while the puzzle
 * stays within its level (and keeps floor clues): first all those the ways without trials can spare, then (expert)
 * those a trial can, as long as the solver's passes stay within work (0: no end). The grade it ends at (-1: none)
 * and its clues in p */
static inline int sv_make_givens(const sv_shape *s, int level, int floor, uint32_t *rng, const uint8_t *full,
                                 uint8_t *p, long budget, long work)
{
    memcpy(p, full, SV_CELLS);
    int order[41], clues = SV_CELLS;
    uint8_t gone[41] = {0};
    long spent = 0;
    for (int i = 0; i < 41; i++) order[i] = i;
    pr_shuffle(rng, order, 41);
    for (int cap = level < SV_HARD ? level : SV_HARD; cap <= level; cap++)
        for (int i = 0; i < 41; i++) {
            int a = order[i], b = SV_CELLS - 1 - a, n = a == b ? 1 : 2;
            if (gone[i] || clues - n < floor || (work && spent > work)) continue;
            uint8_t ka = p[a], kb = p[b];
            long ps;
            p[a] = p[b] = 0;
            int g = sv_grade(s, p, cap, NULL, &ps, budget);
            spent += ps;
            if (g < 0 || g > cap) {
                p[a] = ka, p[b] = kb;
                continue;
            }
            clues -= n;
            gone[i] = 1;
        }
    return sv_grade(s, p, SV_EXPERT, NULL, NULL, budget);
}

/* the cages made from a full grid: every cell its own cage at first, then neighbours' cages merged (no digit twice in
 * a cage, at most `most` cells: first into pairs, then threes...) while the puzzle (no clues) stays within its level
 * (expert: first without trials, then with them while the solver's passes stay within work, 0 no end). The grade it
 * ends at; the cages in s */
static inline int sv_make_cages(sv_shape *s, int level, int most, uint32_t *rng, const uint8_t *full, long budget,
                                long work)
{
    uint8_t lab[SV_CELLS], keep[SV_CELLS];
    int size[SV_CELLS];
    unsigned digits[SV_CELLS];
    long spent = 0;
    for (int c = 0; c < SV_CELLS; c++) lab[c] = (uint8_t)c, size[c] = 1, digits[c] = 1u << full[c];
    int edges[144];
    for (int e = 0; e < 144; e++) edges[e] = e;
    for (int pass = 2; pass <= most + (level == SV_EXPERT); pass++) {
        int cap = pass > most ? most : pass, lv = pass > most ? level : level < SV_HARD ? level : SV_HARD;
        pr_shuffle(rng, edges, 144);
        for (int i = 0; i < 144; i++) {
            int e = edges[i], a = e < 72 ? e / 8 * 9 + e % 8 : e - 72, b = e < 72 ? a + 1 : a + 9;
            int la = lab[a], lb = lab[b];
            if (la == lb || size[la] + size[lb] > cap || (digits[la] & digits[lb])) continue;
            if (work && spent > work) break;
            memcpy(keep, lab, sizeof lab);
            for (int c = 0; c < SV_CELLS; c++)
                if (lab[c] == lb) lab[c] = (uint8_t)la;
            sv_cages_from(s, lab, full);
            sv_shape_init(s);
            long ps;
            int g = sv_grade(s, NULL, lv, NULL, &ps, budget);
            spent += ps;
            if (g < 0 || g > lv) {
                memcpy(lab, keep, sizeof lab);
                continue;
            }
            size[la] += size[lb];
            digits[la] |= digits[lb];
        }
    }
    sv_cages_from(s, lab, full);
    sv_shape_init(s);
    return sv_grade(s, NULL, SV_EXPERT, NULL, NULL, budget);
}

/* ---- the levels' code ------------------------------------------------------------------------------------------- */
/* the regions: 81 digits 1..9; 0: not nine regions of nine */
static inline int sv_regions_decode(sv_shape *s, const char *code)
{
    int n[9] = {0};
    for (int c = 0; c < SV_CELLS; c++) {
        if (code[c] < '1' || code[c] > '9') return 0;
        s->region[c] = (uint8_t)(code[c] - '1');
        n[s->region[c]]++;
    }
    for (int r = 0; r < 9; r++)
        if (n[r] != 9) return 0;
    return 1;
}

static inline void sv_regions_encode(const sv_shape *s, char *out)
{
    for (int c = 0; c < SV_CELLS; c++) out[c] = (char)('1' + s->region[c]);
    out[SV_CELLS] = 0;
}

/* the cages: each cell's joins to its cage (1 right, 2 down) as a digit 0..3, then ':', the sums in the order of the
 * cages' first cells, each two digits; the end of the code at *end (may be NULL); 0: not cages */
static inline int sv_cages_decode(sv_shape *s, const char *code, const char **end)
{
    uint8_t lab[SV_CELLS];
    for (int c = 0; c < SV_CELLS; c++) lab[c] = (uint8_t)c;
    for (int c = 0; c < SV_CELLS; c++) {
        if (code[c] < '0' || code[c] > '3') return 0;
        int j = code[c] - '0';
        for (int t = 0; t < 2; t++) {
            int o = t ? c + 9 : c + 1;
            if (!(j >> t & 1)) continue;
            if ((t ? c >= 72 : c % 9 == 8)) return 0;
            int a = lab[c], b = lab[o];          /* the two labels joined */
            for (int x = 0; x < SV_CELLS; x++)
                if (lab[x] == b) lab[x] = (uint8_t)a;
        }
    }
    if (code[SV_CELLS] != ':') return 0;
    uint8_t zero[SV_CELLS] = {0};
    if (!sv_cages_from(s, lab, zero)) return 0;
    const char *p = code + SV_CELLS + 1;
    for (int q = 0; q < s->ncages; q++, p += 2) {
        if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9') return 0;
        s->cage[q].sum = (uint8_t)((p[0] - '0') * 10 + p[1] - '0');
    }
    if (end) *end = p;
    return 1;
}

static inline int sv_cages_encode(const sv_shape *s, char *out, int size)
{
    if (size < SV_CELLS + 2 + 2 * s->ncages) return 0;
    for (int c = 0; c < SV_CELLS; c++) {
        int j = (c % 9 < 8 && s->cage_of[c + 1] == s->cage_of[c]) | (c < 72 && s->cage_of[c + 9] == s->cage_of[c]) << 1;
        out[c] = (char)('0' + j);
    }
    out[SV_CELLS] = ':';
    char *p = out + SV_CELLS + 1;
    for (int q = 0; q < s->ncages; q++, p += 2) {
        p[0] = (char)('0' + s->cage[q].sum / 10);
        p[1] = (char)('0' + s->cage[q].sum % 10);
    }
    *p = 0;
    return 1;
}

/* the clues: 81 characters, '.' or 1..9 */
static inline int sv_clues_decode(uint8_t *p, const char *code)
{
    for (int c = 0; c < SV_CELLS; c++) {
        if (code[c] != '.' && (code[c] < '1' || code[c] > '9')) return 0;
        p[c] = code[c] == '.' ? 0 : (uint8_t)(code[c] - '0');
    }
    return 1;
}

static inline void sv_clues_encode(const uint8_t *p, char *out)
{
    for (int c = 0; c < SV_CELLS; c++) out[c] = p[c] ? (char)('0' + p[c]) : '.';
    out[SV_CELLS] = 0;
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
typedef struct {
    sv_shape s;
    int level;                                  /* SV_EASY..SV_EXPERT */
    uint32_t seed;
    uint8_t given[SV_CELLS];                    /* 1: a clue */
    uint8_t sol[SV_CELLS];
    uint8_t val[SV_CELLS];                      /* the grid now (0: empty) */
    uint16_t notes[SV_CELLS];                   /* bit d: the note d */
    bench_hist hist;
} sv_game;

/* a puzzle on the shape s (its units and sums made here): its clues (NULL: none) and solution */
static inline void sv_load(sv_game *g, const sv_shape *s, int level, const uint8_t *clues, const uint8_t *sol)
{
    memset(g, 0, sizeof *g);
    g->s = *s;
    sv_shape_init(&g->s);
    g->level = level;
    for (int c = 0; c < SV_CELLS; c++) {
        g->given[c] = clues && clues[c];
        g->val[c] = clues ? clues[c] : 0;
        g->sol[c] = sol[c];
    }
}

/* a designed level: the shape and its clues (NULL: none) graded and solved; 0 when it is not a puzzle with one
 * solution the reasoning finds */
static inline int sv_load_level(sv_game *g, const sv_shape *s, const uint8_t *clues)
{
    sv_shape t = *s;
    sv_shape_init(&t);
    uint8_t sol[SV_CELLS];
    int r = sv_grade(&t, clues, SV_EXPERT, sol, NULL, 0);
    if (r < SV_EASY || r > SV_EXPERT) return 0;
    sv_load(g, &t, r, clues, sol);
    return 1;
}

static inline void sv_change(sv_game *g, int c, int val, unsigned notes)
{
    if (g->val[c] == val && g->notes[c] == notes) return;
    bench_log(&g->hist, c, (uint32_t)g->val[c] << 16 | g->notes[c]);
    g->val[c] = (uint8_t)val;
    g->notes[c] = (uint16_t)notes;
}

static inline int sv_wrongs(const sv_game *g)  /* the wrong digits on the board (the stars' mistakes) */
{
    int k = 0;
    for (int c = 0; c < SV_CELLS; c++) k += g->val[c] && g->val[c] != g->sol[c];
    return k;
}

/* a digit (0: empty the cell); that note goes from the cell's peers. 0: not allowed (a clue, the same digit) */
static inline int sv_put(sv_game *g, int c, int d)
{
    if (c < 0 || c >= SV_CELLS || d < 0 || d > 9 || g->given[c] || g->val[c] == d) return 0;
    bench_begin(&g->hist, 48);
    sv_change(g, c, d, 0);
    if (d)
        for (int i = 0; i < g->s.npeers[c]; i++) {
            int p = g->s.peer[c][i];
            if (g->notes[p] >> d & 1) sv_change(g, p, g->val[p], g->notes[p] & ~(1u << d));
        }
    return bench_done(&g->hist, sv_wrongs(g));
}

static inline int sv_note(sv_game *g, int c, int d)  /* a note on or off (an empty cell); 0: not allowed */
{
    if (c < 0 || c >= SV_CELLS || d < 1 || d > 9 || g->given[c] || g->val[c]) return 0;
    bench_begin(&g->hist, 48);
    sv_change(g, c, 0, g->notes[c] ^ (1u << d));
    return bench_done(&g->hist, sv_wrongs(g));
}

static inline int sv_erase(sv_game *g, int c)  /* the digit, else the notes; 0: nothing */
{
    if (c < 0 || c >= SV_CELLS || g->given[c]) return 0;
    if (g->val[c]) return sv_put(g, c, 0);
    if (!g->notes[c]) return 0;
    bench_begin(&g->hist, 48);
    sv_change(g, c, 0, 0);
    return bench_done(&g->hist, sv_wrongs(g));
}

/* a wrong digit put right, else the cell the easiest reasoning decides next filled: the cell; -1: none */
static inline int sv_hint_step(sv_game *g)
{
    for (int c = 0; c < SV_CELLS; c++)
        if (g->val[c] && g->val[c] != g->sol[c]) return sv_put(g, c, g->sol[c]) ? c : -1;
    bench_rules r;
    bench_grid b;
    sv_rules(&r, &g->s, NULL);
    sv_grid(g->val, &b);
    int c = bench_next(&b, &r);
    if (c < 0 || g->val[c])
        for (c = 0; c < SV_CELLS && g->val[c]; c++) {}
    if (c >= SV_CELLS) return -1;
    return sv_put(g, c, g->sol[c]) ? c : -1;
}

static inline int sv_hint(sv_game *g) { return bench_hinted(&g->hist, sv_hint_step(g)); }

static inline void sv_put_back(void *ctx, int c, uint32_t was)
{
    sv_game *g = ctx;
    g->val[c] = (uint8_t)(was >> 16);
    g->notes[c] = (uint16_t)was;
}

static inline int sv_undo(sv_game *g)          /* the last action taken back: a cell it changed; -1: none */
{
    int c = bench_undo(&g->hist, sv_put_back, g);
    bench_undone(&g->hist, sv_wrongs(g));
    return c;
}

/* 1: its digit is also in a peer (a unit's, its cage's), or its cage is full and misses its sum */
static inline int sv_clash(const sv_game *g, int c)
{
    int d = g->val[c];
    if (!d) return 0;
    for (int i = 0; i < g->s.npeers[c]; i++)
        if (g->val[g->s.peer[c][i]] == d) return 1;
    if (!g->s.ncages) return 0;
    const sv_cage *k = &g->s.cage[g->s.cage_of[c]];
    int sum = 0;
    for (int i = 0; i < k->n; i++) {
        if (!g->val[k->cell[i]]) return 0;
        sum += g->val[k->cell[i]];
    }
    return sum != k->sum;
}

static inline int sv_solved(const sv_game *g)
{
    for (int c = 0; c < SV_CELLS; c++)
        if (g->val[c] != g->sol[c]) return 0;
    return 1;
}

static inline int sv_count_digit(const sv_game *g, int d)
{
    int n = 0;
    for (int c = 0; c < SV_CELLS; c++) n += g->val[c] == d;
    return n;
}

static inline int sv_cell(const char *s) { return bench_cell(s, 9, 9, NULL); }

static inline int sv_script(sv_game *g, const char *s)  /* 0 at the first bad move */
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (sv_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int n = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
            for (int c = 0; c < SV_CELLS && n > 0; c++)
                if (!g->val[c]) { sv_put(g, c, g->sol[c]); n--; }
            continue;
        }
        int c = sv_cell(s);
        if (c < 0 || (s[2] != '=' && s[2] != '\'') || s[3] < '0' || s[3] > '9') return 0;
        int ok = s[2] == '=' ? sv_put(g, c, s[3] - '0') : sv_note(g, c, s[3] - '0');
        if (!ok) return 0;
        s += 4;
    }
    return 1;
}

#endif
