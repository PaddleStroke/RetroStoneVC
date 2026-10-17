/*
 * The Sudoku family's level tools' taste (the tools *sudoku_gen.c): what makes a level look set by a hand.
 *   - Symmetry: the clues (or a killer's cages) keep one of the setters' symmetries, varied along a chapter: a half
 *     turn (the classic), a quarter turn, a mirror left to right, a mirror on the diagonal, both mirrors.
 *   - Balance: no row, column or region left bare, no region crowded while another is empty.
 *   - Jigsaw regions: few one-cell spurs and long snakes, few regions left as plain boxes.
 * Static inline, for the tools only (the game never makes levels this way).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef SUDOKU_GEN_CRAFT_H
#define SUDOKU_GEN_CRAFT_H

#include <math.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/sudokuvar.h"

enum { CR_ROT2, CR_ROT4, CR_MIRROR, CR_DIAG, CR_BOTH, CR_MODES };

/* the cell c's images under the symmetry's group (c itself first; may repeat): their count */
static inline int cr_images(int mode, int c, int *out)
{
    int r = c / 9, k = c % 9, n = 0;
    out[n++] = c;
    switch (mode) {
    case CR_ROT4:
        out[n++] = k * 9 + 8 - r;
        out[n++] = (8 - r) * 9 + 8 - k;
        out[n++] = (8 - k) * 9 + r;
        break;
    case CR_MIRROR: out[n++] = r * 9 + 8 - k; break;
    case CR_DIAG: out[n++] = k * 9 + r; break;
    case CR_BOTH:
        out[n++] = r * 9 + 8 - k;
        out[n++] = (8 - r) * 9 + k;
        out[n++] = (8 - r) * 9 + 8 - k;
        break;
    default: out[n++] = (8 - r) * 9 + 8 - k; break;
    }
    return n;
}

typedef struct {
    int n;
    int len[SV_CELLS];
    int cell[SV_CELLS][4];
} cr_orbits;

static inline void cr_orbits_of(int mode, cr_orbits *o)
{
    uint8_t done[SV_CELLS] = {0};
    o->n = 0;
    for (int c = 0; c < SV_CELLS; c++) {
        if (done[c]) continue;
        int im[4], m = cr_images(mode, c, im), *dst = o->cell[o->n], len = 0;
        for (int i = 0; i < m; i++) {
            int dup = 0;
            for (int j = 0; j < len; j++) dup |= dst[j] == im[i];
            if (!dup) dst[len++] = im[i], done[im[i]] = 1;
        }
        o->len[o->n++] = len;
    }
}

/* sv_make_givens with the clues taken away a whole orbit at a time (so the symmetry holds) */
static inline int cr_givens(const sv_shape *s, int level, int floor, int mode, uint32_t *rng, const uint8_t *full,
                            uint8_t *p, long budget, long work)
{
    cr_orbits o;
    cr_orbits_of(mode, &o);
    memcpy(p, full, SV_CELLS);
    int order[SV_CELLS], clues = SV_CELLS;
    uint8_t gone[SV_CELLS] = {0}, keep[4];
    long spent = 0;
    for (int i = 0; i < o.n; i++) order[i] = i;
    pr_shuffle(rng, order, o.n);
    for (int cap = level < SV_HARD ? level : SV_HARD; cap <= level; cap++)
        for (int i = 0; i < o.n; i++) {
            int q = order[i], n = o.len[q];
            if (gone[q] || clues - n < floor || (work && spent > work)) continue;
            for (int j = 0; j < n; j++) keep[j] = p[o.cell[q][j]], p[o.cell[q][j]] = 0;
            long ps;
            int g = sv_grade(s, p, cap, NULL, &ps, budget);
            spent += ps;
            if (g < 0 || g > cap) {
                for (int j = 0; j < n; j++) p[o.cell[q][j]] = keep[j];
                continue;
            }
            clues -= n;
            gone[q] = 1;
        }
    return sv_grade(s, p, SV_EXPERT, NULL, NULL, budget);
}

/* clues put back from the solution, whole orbits, until there are `target` */
static inline void cr_more_clues(int mode, uint8_t *p, const uint8_t *sol, int target, uint32_t *rng)
{
    cr_orbits o;
    cr_orbits_of(mode, &o);
    int order[SV_CELLS], n = 0;
    for (int i = 0; i < o.n; i++) order[i] = i;
    pr_shuffle(rng, order, o.n);
    for (int c = 0; c < SV_CELLS; c++) n += p[c] != 0;
    for (int i = 0; i < o.n && n < target; i++) {
        int q = order[i];
        if (p[o.cell[q][0]]) continue;
        for (int j = 0; j < o.len[q]; j++) p[o.cell[q][j]] = sol[o.cell[q][j]];
        n += o.len[q];
    }
}

/* the clues' balance: their spread over the regions (0: even; higher: lopsided), and the fewest clues any row,
 * column or region has (in *bare) */
static inline double cr_balance(const uint8_t *region, const uint8_t *p, int *bare)
{
    int reg[9] = {0}, row[9] = {0}, col[9] = {0}, total = 0;
    for (int c = 0; c < SV_CELLS; c++)
        if (p[c]) reg[region[c]]++, row[c / 9]++, col[c % 9]++, total++;
    double mean = total / 9.0, var = 0;
    int least = 9;
    for (int i = 0; i < 9; i++) {
        var += (reg[i] - mean) * (reg[i] - mean) + 0.5 * ((row[i] - mean) * (row[i] - mean) +
                                                          (col[i] - mean) * (col[i] - mean));
        if (reg[i] < least) least = reg[i];
        if (row[i] < least) least = row[i];
        if (col[i] < least) least = col[i];
    }
    if (bare) *bare = least;
    return sqrt(var / 18.0);
}

/* a jigsaw's regions' ugliness: one-cell spurs, long snakes (a region's box far wider or taller than 3), plain 3 x 3
 * boxes left as they were. 0: lovely */
static inline int cr_regions_ugly(const uint8_t *reg)
{
    int ugly = 0, x0[9], x1[9], y0[9], y1[9];
    for (int r = 0; r < 9; r++) x0[r] = y0[r] = 9, x1[r] = y1[r] = -1;
    for (int c = 0; c < SV_CELLS; c++) {
        int x = c % 9, y = c / 9, r = reg[c], same = 0;
        same += x > 0 && reg[c - 1] == r;
        same += x < 8 && reg[c + 1] == r;
        same += y > 0 && reg[c - 9] == r;
        same += y < 8 && reg[c + 9] == r;
        ugly += same <= 1 ? 2 : 0;              /* a spur hanging by one side */
        if (x < x0[r]) x0[r] = x;
        if (x > x1[r]) x1[r] = x;
        if (y < y0[r]) y0[r] = y;
        if (y > y1[r]) y1[r] = y;
    }
    for (int r = 0; r < 9; r++) {
        int w = x1[r] - x0[r] + 1, h = y1[r] - y0[r] + 1;
        if (w > 5) ugly += (w - 5) * 3;
        if (h > 5) ugly += (h - 5) * 3;
        if (w == 3 && h == 3) ugly += 3;        /* still a plain box */
    }
    return ugly;
}

/* sv_make_cages with every merge made at once with its images under the symmetry (the cages' pattern keeps it) */
static inline int cr_cages(sv_shape *s, int level, int most, int mode, uint32_t *rng, const uint8_t *full,
                           long budget, long work)
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
            if (lab[a] == lab[b]) continue;
            if (work && spent > work) break;
            int ia[4], ib[4], n = cr_images(mode, a, ia);
            cr_images(mode, b, ib);
            /* the labels each image joins, grouped: lbl[] with a little union-find */
            int lbl[8], par[8], nl = 0;
            for (int j = 0; j < n; j++) {
                int la = lab[ia[j]], lb = lab[ib[j]], xa = -1, xb = -1;
                for (int t = 0; t < nl; t++) {
                    if (lbl[t] == la) xa = t;
                    if (lbl[t] == lb) xb = t;
                }
                if (xa < 0) lbl[nl] = la, par[nl] = nl, xa = nl++;
                if (xb < 0) lbl[nl] = lb, par[nl] = nl, xb = nl++;
                while (par[xa] != xa) xa = par[xa];
                while (par[xb] != xb) xb = par[xb];
                if (xa != xb) par[xb] = xa;
            }
            int ok = 1, root[8];
            for (int t = 0; t < nl; t++) {
                int x = t;
                while (par[x] != x) x = par[x];
                root[t] = x;
            }
            for (int t = 0; t < nl && ok; t++) {
                if (root[t] != t) continue;
                int sz = 0;
                unsigned dg = 0;
                for (int u = 0; u < nl; u++) {
                    if (root[u] != t) continue;
                    sz += size[lbl[u]];
                    if (dg & digits[lbl[u]]) ok = 0;
                    dg |= digits[lbl[u]];
                }
                if (sz > cap) ok = 0;
            }
            if (!ok) continue;
            memcpy(keep, lab, sizeof lab);
            for (int t = 0; t < nl; t++)
                if (root[t] != t)
                    for (int c = 0; c < SV_CELLS; c++)
                        if (lab[c] == lbl[t]) lab[c] = (uint8_t)lbl[root[t]];
            sv_cages_from(s, lab, full);
            sv_shape_init(s);
            long ps;
            int g = sv_grade(s, NULL, lv, NULL, &ps, budget);
            spent += ps;
            if (g < 0 || g > lv) {
                memcpy(lab, keep, sizeof lab);
                continue;
            }
            for (int t = 0; t < nl; t++)
                if (root[t] != t) {
                    size[lbl[root[t]]] += size[lbl[t]];
                    digits[lbl[root[t]]] |= digits[lbl[t]];
                }
        }
    }
    sv_cages_from(s, lab, full);
    sv_shape_init(s);
    return sv_grade(s, NULL, SV_EXPERT, NULL, NULL, budget);
}

/* the symmetry the i-th level of a chapter of 25 keeps: mostly the classic half turn, the others mixed in, and the
 * chapter's last (its showpiece) a quarter turn or both mirrors. allow: the modes the game takes (bits) */
static inline int cr_mode_for(int i, int chapter, unsigned allow)
{
    static const int mix[25] = {CR_ROT2, CR_MIRROR, CR_ROT2, CR_DIAG, CR_ROT2, CR_BOTH, CR_ROT2, CR_ROT4, CR_MIRROR,
                                CR_ROT2, CR_DIAG, CR_ROT2, CR_ROT4, CR_ROT2, CR_MIRROR, CR_BOTH, CR_ROT2, CR_DIAG,
                                CR_ROT2, CR_ROT4, CR_MIRROR, CR_ROT2, CR_BOTH, CR_ROT2, CR_ROT4};
    int m = i % 25 == 24 ? (chapter & 1 ? CR_BOTH : CR_ROT4) : mix[(i + chapter * 7) % 24];
    if (!(allow >> m & 1)) m = allow >> CR_ROT4 & 1 && i % 25 == 24 ? CR_ROT4 : CR_ROT2;
    return m;
}

#endif
