/*
 * Calcudoku: the rules (calcudoku.h): the cages' arithmetic, the solver that grades (pzbench.h) and the one that
 * counts, the maker, and the player's grid with its notes and undo. No SDL: tested by tests/calcudoku_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/calcudoku.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

int cdk_value(int op, const int *v, int k)
{
    int r = v[0];
    switch (op) {
    case CDK_ONE: return k == 1 ? r : -1;
    case CDK_ADD: for (int i = 1; i < k; i++) r += v[i]; return r;
    case CDK_MUL: for (int i = 1; i < k; i++) r *= v[i]; return r;
    case CDK_SUB: return k == 2 ? abs(v[0] - v[1]) : -1;
    default:
        if (k != 2) return -1;
        {
            int hi = v[0] > v[1] ? v[0] : v[1], lo = v[0] + v[1] - hi;
            return lo && hi % lo == 0 ? hi / lo : -1;
        }
    }
}

/* ---- the cages' ways: every filling of a cage that makes its target ------------------------------------------- */
typedef struct {
    const cdk_puzzle *p;
    int n;
    /* each cage: its lines (rows 0..n-1, columns n..2n-1) and which of its cells lie on each */
    uint8_t nlines[CDK_CELLS], line[CDK_CELLS][2 * CDK_CAGE], on[CDK_CELLS][2 * CDK_CAGE];
} ctx_t;

typedef struct {
    const cdk_cage *k;
    const uint16_t *d;
    uint8_t same[CDK_CAGE];                     /* bit j: cell j shares a line with this one (no same digit) */
    int v[CDK_CAGE];
    uint16_t uni[CDK_CAGE];                     /* the digits each cell takes in some way */
    uint16_t must[2 * CDK_CAGE];                /* the digits every way puts on each of the cage's lines */
    const uint8_t *on;
    int nlines, ways;
} scan_t;

static void scan_rec(scan_t *s, int i, int acc)
{
    const cdk_cage *k = s->k;
    if (i == k->n) {
        if (cdk_value(k->op, s->v, k->n) != k->target) return;
        s->ways++;
        for (int j = 0; j < k->n; j++) s->uni[j] |= (uint16_t)(1u << s->v[j]);
        for (int l = 0; l < s->nlines; l++) {
            unsigned m = 0;
            for (int j = 0; j < k->n; j++)
                if (s->on[l] >> j & 1) m |= 1u << s->v[j];
            s->must[l] &= (uint16_t)m;
        }
        return;
    }
    unsigned m = s->d[k->cell[i]];
    for (int j = 0; j < i; j++)
        if (s->same[i] >> j & 1) m &= ~(1u << s->v[j]);
    int left = k->n - i - 1;
    for (; m; m &= m - 1) {
        int d = pzb_low(m), a = acc;
        if (k->op == CDK_ADD) {
            a = acc + d;
            if (a + left > k->target) break;        /* the digits come up in order: larger ones only worse */
            if (a + left * CDK_MAX < k->target) continue;
        } else if (k->op == CDK_MUL) {
            a = acc * d;
            if (k->target % a) continue;
        }
        s->v[i] = d;
        scan_rec(s, i + 1, a);
    }
}

/* the cage's ways under the candidates d: 0 when it has none */
static int scan(scan_t *s, const ctx_t *x, int c, const uint16_t *d)
{
    const cdk_cage *k = &x->p->cage[c];
    int n = x->n;
    s->k = k;
    s->d = d;
    s->ways = 0;
    s->nlines = x->nlines[c];
    s->on = x->on[c];
    for (int i = 0; i < k->n; i++) {
        s->uni[i] = 0;
        s->same[i] = 0;
        for (int j = 0; j < k->n; j++)
            if (j != i && (k->cell[i] / n == k->cell[j] / n || k->cell[i] % n == k->cell[j] % n))
                s->same[i] |= (uint8_t)(1u << j);
    }
    for (int l = 0; l < s->nlines; l++) s->must[l] = 0xffff;
    scan_rec(s, 0, k->op == CDK_MUL ? 1 : 0);
    return s->ways;
}

static void ctx_init(ctx_t *x, const cdk_puzzle *p)
{
    int n = p->n;
    x->p = p;
    x->n = n;
    for (int c = 0; c < p->ncages; c++) {
        const cdk_cage *k = &p->cage[c];
        int nl = 0;
        for (int i = 0; i < k->n; i++) {
            int ls[2] = {k->cell[i] / n, n + k->cell[i] % n};
            for (int t = 0; t < 2; t++) {
                int l = 0;
                while (l < nl && x->line[c][l] != ls[t]) l++;
                if (l == nl) { x->line[c][nl] = (uint8_t)ls[t]; x->on[c][nl] = 0; nl++; }
                x->on[c][l] |= (uint8_t)(1u << i);
            }
        }
        x->nlines[c] = (uint8_t)nl;
    }
}

static int line_cell(int n, int l, int i) { return l < n ? l * n + i : i * n + (l - n); }

/* ---- the solver's tiers ------------------------------------------------------------------------------------------ */
static int cut(pzb_grid *g, int c, unsigned keep)
{
    unsigned m = g->d[c] & keep;
    if (m == g->d[c]) return 0;
    g->d[c] = (uint16_t)m;
    return 1;
}

static int deduce(pzb_grid *g, int tier, void *cx)
{
    ctx_t *x = cx;
    int n = x->n, found = 0;
    scan_t s;
    if (tier == 0) {
        for (int c = 0; c < n * n; c++) {       /* a digit placed: not again in its row and column */
            if (!pzb_single(g->d[c])) continue;
            for (int i = 0; i < n; i++) {
                int r = c / n * n + i, k = i * n + c % n;
                if (r != c) found |= cut(g, r, ~(unsigned)g->d[c]);
                if (k != c) found |= cut(g, k, ~(unsigned)g->d[c]);
            }
        }
        for (int l = 0; l < 2 * n; l++)         /* a digit with one place left in a line */
            for (int d = 1; d <= n; d++) {
                int at = -1, places = 0;
                for (int i = 0; i < n; i++)
                    if (g->d[line_cell(n, l, i)] >> d & 1) places++, at = line_cell(n, l, i);
                if (!places) return -1;
                if (places == 1 && g->d[at] != 1u << d) g->d[at] = (uint16_t)(1u << d), found = 1;
            }
        if (found) return 1;
        for (int c = 0; c < x->p->ncages; c++) { /* a cage's ways */
            if (!scan(&s, x, c, g->d)) return -1;
            for (int i = 0; i < s.k->n; i++) found |= cut(g, s.k->cell[i], s.uni[i]);
        }
        return found;
    }
    for (int c = 0; c < x->p->ncages; c++) {    /* the digits a cage must put on a line: not elsewhere on it */
        if (!scan(&s, x, c, g->d)) return -1;
        for (int l = 0; l < s.nlines; l++) {
            if (!s.must[l]) continue;
            for (int i = 0; i < n; i++) {
                int cell = line_cell(n, x->line[c][l], i);
                if (x->p->cage_of[cell] != c) found |= cut(g, cell, ~(unsigned)s.must[l]);
            }
        }
    }
    if (found) return 1;
    for (int l = 0; l < 2 * n; l++) {           /* naked and hidden pairs and triples in a line */
        int cells[CDK_MAX], m = 0;
        for (int i = 0; i < n; i++)
            if (!pzb_single(g->d[line_cell(n, l, i)])) cells[m++] = line_cell(n, l, i);
        for (int size = 2; size <= 3; size++)
            for (unsigned set = 0; set < 1u << m; set++) {
                if (pzb_bits(set) != size) continue;
                unsigned uni = 0;
                for (int i = 0; i < m; i++)
                    if (set >> i & 1) uni |= g->d[cells[i]];
                if (pzb_bits(uni) < size) return -1;
                if (pzb_bits(uni) == size)
                    for (int i = 0; i < m; i++)
                        if (!(set >> i & 1)) found |= cut(g, cells[i], ~uni);
            }
        unsigned digits = 0;                    /* hidden: size digits whose places are size cells */
        for (int i = 0; i < m; i++) digits |= g->d[cells[i]];
        for (unsigned ds = digits; ds; ds = (ds - 1) & digits) {
            int size = pzb_bits(ds);
            if (size < 2 || size > 3) continue;
            unsigned places = 0;
            for (int i = 0; i < m; i++)
                if (g->d[cells[i]] & ds) places |= 1u << i;
            if (pzb_bits(places) == size)
                for (int i = 0; i < m; i++)
                    if (places >> i & 1) found |= cut(g, cells[i], ds);
        }
        if (found) return 1;
    }
    return found;
}

static int valid(const pzb_grid *g, void *cx)
{
    const ctx_t *x = cx;
    const cdk_puzzle *p = x->p;
    for (int c = 0; c < p->ncages; c++) {
        int v[CDK_CAGE];
        for (int i = 0; i < p->cage[c].n; i++) v[i] = pzb_low(g->d[p->cage[c].cell[i]]);
        if (cdk_value(p->cage[c].op, v, p->cage[c].n) != p->cage[c].target) return 0;
    }
    return 1;
}

static void rules_of(pzb_rules *r, ctx_t *x, const cdk_puzzle *p, pzb_grid *g)
{
    ctx_init(x, p);
    memset(r, 0, sizeof *r);
    r->vars = p->n * p->n;
    r->tiers = 2;
    r->probe_tiers = 1;
    r->deduce = deduce;
    r->valid = valid;
    r->ctx = x;
    r->budget = 4000;
    unsigned all = ((1u << (p->n + 1)) - 1) & ~1u;
    for (int c = 0; c < r->vars; c++) g->d[c] = (uint16_t)all;
}

static int rate_upto(const cdk_puzzle *p, int most, long budget)
{
    pzb_rules r;
    ctx_t x;
    pzb_grid g;
    rules_of(&r, &x, p, &g);
    if (budget) r.budget = budget;
    return pzb_grade(&g, &r, most);
}

int cdk_rate(const cdk_puzzle *p) { return rate_upto(p, PZB_HARD, 0); }

/* ---- counting: depth first, the cell with the fewest digits first, the easy deductions between ------------------ */
static int count_rec(pzb_grid *g, pzb_rules *r, int limit)
{
    int used = 0;
    if (pzb_settle(g, r, 0, &used) < 0) return 0;
    int best = -1, bn = 99;
    for (int c = 0; c < r->vars; c++) {
        int b = pzb_bits(g->d[c]);
        if (b > 1 && b < bn) bn = b, best = c;
    }
    if (best < 0) return r->valid(g, r->ctx);
    int found = 0;
    for (unsigned m = g->d[best]; m && found < limit; m &= m - 1) {
        pzb_grid t = *g;
        t.d[best] = (uint16_t)(m & -m);
        found += count_rec(&t, r, limit - found);
    }
    return found;
}

int cdk_count(const cdk_puzzle *p, int limit)
{
    pzb_rules r;
    ctx_t x;
    pzb_grid g;
    rules_of(&r, &x, p, &g);
    r.budget = 0;
    return count_rec(&g, &r, limit);
}

/* ---- the maker -------------------------------------------------------------------------------------------------- */
/* a Latin square by chance: row after row, each a random perfect matching of the columns to the digits they still
 * lack (a Latin rectangle always grows by a row) */
static int match_try(int c, int n, const unsigned *can, int *dig_of, int *col_of, unsigned *seen, const int *order)
{
    for (int k = 0; k < n; k++) {
        int d = order[(c * 7 + k) % n];
        if (!(can[c] >> d & 1) || (*seen >> d & 1)) continue;
        *seen |= 1u << d;
        if (col_of[d] < 0 || match_try(col_of[d], n, can, dig_of, col_of, seen, order)) {
            dig_of[c] = d;
            col_of[d] = c;
            return 1;
        }
    }
    return 0;
}

static void latin(uint32_t *rng, int n, uint8_t *sol)
{
    unsigned used[CDK_MAX] = {0};
    for (int r = 0; r < n; r++) {
        int dig_of[CDK_MAX], col_of[CDK_MAX], order[CDK_MAX], cols[CDK_MAX];
        unsigned can[CDK_MAX];
        for (int i = 0; i < n; i++) order[i] = i, cols[i] = i, col_of[i] = dig_of[i] = -1;
        pr_shuffle(rng, order, n);
        pr_shuffle(rng, cols, n);
        for (int c = 0; c < n; c++) can[c] = ((1u << n) - 1) & ~used[c];
        for (int k = 0; k < n; k++) {
            unsigned seen = 0;
            match_try(cols[k], n, can, dig_of, col_of, &seen, order);
        }
        for (int c = 0; c < n; c++) {
            sol[r * n + c] = (uint8_t)(dig_of[c] + 1);
            used[c] |= 1u << dig_of[c];
        }
    }
}

/* the cages' sizes by level: how many in a hundred want 1, 2, 3, 4 cells */
static const int SIZE_WANT[CDK_LEVELS][4] = {{14, 52, 28, 6}, {5, 42, 40, 13}, {2, 30, 46, 22}};

static void cages(uint32_t *rng, int n, int level, cdk_puzzle *p)
{
    int cells = n * n, order[CDK_CELLS], cage_n[CDK_CELLS];
    memset(p->cage_of, 0xff, sizeof p->cage_of);
    for (int c = 0; c < cells; c++) order[c] = c;
    pr_shuffle(rng, order, cells);
    int nc = 0;
    for (int k = 0; k < cells; k++) {
        int c = order[k];
        if (p->cage_of[c] != 0xff) continue;
        int roll = pr_below(rng, 100), want = 1;
        for (int t = 0; t < 4 && roll >= SIZE_WANT[level][t]; t++) roll -= SIZE_WANT[level][t], want++;
        if (want > 4) want = 4;
        int members[CDK_CAGE] = {c}, m = 1;
        p->cage_of[c] = (uint8_t)nc;
        while (m < want) {
            int cand[4 * CDK_CAGE], nn = 0;
            for (int i = 0; i < m; i++) {
                int x = members[i] % n, y = members[i] / n;
                const int nb[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
                for (int t = 0; t < 4; t++)
                    if (nb[t][0] >= 0 && nb[t][0] < n && nb[t][1] >= 0 && nb[t][1] < n &&
                        p->cage_of[nb[t][1] * n + nb[t][0]] == 0xff)
                        cand[nn++] = nb[t][1] * n + nb[t][0];
            }
            if (!nn) break;
            int pick = cand[pr_below(rng, nn)];
            p->cage_of[pick] = (uint8_t)nc;
            members[m++] = pick;
        }
        cage_n[nc] = want;                       /* what it wanted (1: a single on purpose) */
        p->cage[nc].n = (uint8_t)m;
        nc++;
    }
    /* a cell left alone though it wanted company: joined to a small neighbouring cage */
    for (int c = 0; c < cells; c++) {
        int k = p->cage_of[c];
        if (p->cage[k].n != 1 || cage_n[k] == 1) continue;
        int x = c % n, y = c / n, best = -1, bn = CDK_CAGE;
        const int nb[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
        for (int t = 0; t < 4; t++) {
            if (nb[t][0] < 0 || nb[t][0] >= n || nb[t][1] < 0 || nb[t][1] >= n) continue;
            int o = p->cage_of[nb[t][1] * n + nb[t][0]];
            if (p->cage[o].n < bn) bn = p->cage[o].n, best = o;
        }
        if (best >= 0) {
            p->cage_of[c] = (uint8_t)best;
            p->cage[best].n++;
            p->cage[k].n = 0;
        }
    }
    /* numbered again without the empty ones; the cells in reading order */
    int map[CDK_CELLS], m = 0;
    for (int k = 0; k < nc; k++) map[k] = p->cage[k].n ? m++ : -1;
    p->ncages = m;
    for (int k = 0; k < m; k++) p->cage[k].n = 0;
    for (int c = 0; c < cells; c++) {
        int k = map[p->cage_of[c]];
        p->cage_of[c] = (uint8_t)k;
        p->cage[k].cell[p->cage[k].n++] = (uint8_t)c;
    }
}

static void operations(uint32_t *rng, cdk_puzzle *p)
{
    for (int k = 0; k < p->ncages; k++) {
        cdk_cage *g = &p->cage[k];
        int v[CDK_CAGE];
        for (int i = 0; i < g->n; i++) v[i] = p->sol[g->cell[i]];
        if (g->n == 1) {
            g->op = CDK_ONE;
        } else if (g->n == 2) {
            int hi = v[0] > v[1] ? v[0] : v[1], lo = v[0] + v[1] - hi, roll = pr_below(rng, 100);
            if (hi % lo == 0 && roll < 55) g->op = CDK_DIV;
            else if ((roll = pr_below(rng, 100)) < 50) g->op = CDK_SUB;
            else g->op = roll < 75 ? CDK_ADD : CDK_MUL;
        } else {
            g->op = pr_below(rng, 2) ? CDK_MUL : CDK_ADD;
        }
        g->target = cdk_value(g->op, v, g->n);
    }
}

/* one step harder, maybe: two neighbouring cages joined (four cells at most), or a cage of two given a looser
 * operation. 0: none found */
static int harden(uint32_t *rng, cdk_puzzle *p)
{
    int n = p->n;
    for (int tries = 0; tries < 40; tries++) {
        int c = pr_below(rng, n * n), a = p->cage_of[c];
        cdk_cage *A = &p->cage[a];
        if (pr_below(rng, 4) == 0) {            /* a looser operation */
            if (A->n < 2) continue;
            int v[CDK_CAGE], old = A->op;
            for (int i = 0; i < A->n; i++) v[i] = p->sol[A->cell[i]];
            A->op = (uint8_t)(A->n > 2 ? (old == CDK_ADD ? CDK_MUL : CDK_ADD) : pr_below(rng, 2) ? CDK_ADD : CDK_MUL);
            if (A->op == old) continue;
            A->target = cdk_value(A->op, v, A->n);
            return 1;
        }
        int x = c % n, y = c / n, dir = pr_below(rng, 4);
        int nx = x + (dir == 0) - (dir == 1), ny = y + (dir == 2) - (dir == 3);
        if (nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
        int b = p->cage_of[ny * n + nx];
        if (b == a || A->n + p->cage[b].n > CDK_CAGE) continue;
        cdk_cage *B = &p->cage[b];
        int cells[2 * CDK_CAGE], m = 0, v[CDK_CAGE];
        for (int i = 0; i < A->n; i++) cells[m++] = A->cell[i];
        for (int i = 0; i < B->n; i++) cells[m++] = B->cell[i];
        for (int i = 1; i < m; i++)              /* reading order */
            for (int j = i; j > 0 && cells[j] < cells[j - 1]; j--) {
                int t = cells[j];
                cells[j] = cells[j - 1];
                cells[j - 1] = t;
            }
        A->n = (uint8_t)m;
        for (int i = 0; i < m; i++) {
            A->cell[i] = (uint8_t)cells[i];
            p->cage_of[cells[i]] = (uint8_t)a;
            v[i] = p->sol[cells[i]];
        }
        if (m == 2) {
            int hi = v[0] > v[1] ? v[0] : v[1], lo = v[0] + v[1] - hi;
            A->op = (uint8_t)(hi % lo == 0 && pr_below(rng, 2) ? CDK_DIV : CDK_SUB);
        } else {
            A->op = (uint8_t)(pr_below(rng, 2) ? CDK_MUL : CDK_ADD);
        }
        A->target = cdk_value(A->op, v, m);
        int last = --p->ncages;                 /* the last cage takes B's number */
        if (b != last) {
            p->cage[b] = p->cage[last];
            for (int i = 0; i < p->cage[b].n; i++) p->cage_of[p->cage[b].cell[i]] = (uint8_t)b;
        }
        return 1;
    }
    return 0;
}

/* a puzzle the easier levels solve, made harder step by step: each step kept while those levels still solve it
 * (so it stays unique), until one needs trials. 1: done */
static int make_hard(uint32_t *rng, int n, cdk_puzzle *p)
{
    for (;;) {
        latin(rng, n, p->sol);
        cages(rng, n, CDK_MEDIUM, p);
        operations(rng, p);
        int r = rate_upto(p, CDK_MEDIUM, 0);
        if (r >= 0 && r <= CDK_MEDIUM) break;
    }
    int expensive = 0;
    for (int step = 0; step < 80 && expensive < 6; step++) {
        cdk_puzzle was = *p;
        if (!harden(rng, p)) break;
        int r = rate_upto(p, CDK_MEDIUM, 0);
        if (r >= 0 && r <= CDK_MEDIUM) continue;
        expensive++;
        if (rate_upto(p, CDK_HARD, 1500) == CDK_HARD) return 1;
        *p = was;
    }
    return 0;
}

void cdk_make(int n, int level, uint32_t seed, cdk_puzzle *p)
{
    uint32_t rng = pr_seed(seed);
    if (level < 0 || level >= CDK_LEVELS) level = CDK_EASY;
    if (n < 3 || n > CDK_MAX) n = 4;
    p->n = n;
    if (level == CDK_HARD) {
        while (!make_hard(&rng, n, p)) {}
        return;
    }
    cdk_puzzle near;
    int near_level = -1;
    near.n = n;
    for (int tries = 0; tries < 400; tries++) {
        if (tries % 4 == 0) latin(&rng, n, p->sol);
        cages(&rng, n, level, p);
        operations(&rng, p);
        int r = rate_upto(p, level, 0);         /* no trials for the easier levels: quicker */
        if (r == level) return;
        if (r >= 0 && r < PZB_GUESS && r > near_level) near = *p, near_level = r;  /* the hardest below, kept */
    }
    if (near_level >= 0) { *p = near; return; }
    for (;;) {                                   /* never left without one: single cells until it is easy */
        latin(&rng, n, p->sol);
        cages(&rng, n, CDK_EASY, p);
        operations(&rng, p);
        int r = cdk_rate(p);
        if (r >= 0 && r < PZB_GUESS) return;
    }
}

/* ---- the player's grid ------------------------------------------------------------------------------------------ */
void cdk_new(cdk_game *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    if (variant < 0 || variant >= CDK_VARIANTS) variant = 0;
    g->variant = variant;
    g->level = cdk_level_of(variant);
    g->seed = seed;
    cdk_make(cdk_size_of(variant), g->level, seed, &g->p);
    pzb_hist_clear(&g->hist);
}

static void restore(void *cx, int at, uint32_t was)
{
    cdk_game *g = cx;
    g->val[at] = (uint8_t)(was >> 16);
    g->notes[at] = (uint16_t)was;
}

static void set(cdk_game *g, int c, int val, unsigned notes)
{
    if (g->val[c] == val && g->notes[c] == notes) return;
    pzb_log(&g->hist, c, (uint32_t)g->val[c] << 16 | g->notes[c]);
    g->val[c] = (uint8_t)val;
    g->notes[c] = (uint16_t)notes;
}

static int cells(const cdk_game *g) { return g->p.n * g->p.n; }

int cdk_put(cdk_game *g, int c, int d)
{
    int n = g->p.n;
    if (c < 0 || c >= cells(g) || d < 0 || d > n) return 0;
    if (d && g->val[c] == d) return 0;
    if (!d && !g->val[c]) return 0;
    pzb_begin(&g->hist, 2 * n + 1);
    set(g, c, d, d ? 0 : g->notes[c]);
    if (d)
        for (int i = 0; i < n; i++) {           /* the note d goes from the row and the column */
            int r = c / n * n + i, k = i * n + c % n;
            if (g->notes[r] >> d & 1) set(g, r, g->val[r], g->notes[r] & ~(1u << d));
            if (g->notes[k] >> d & 1) set(g, k, g->val[k], g->notes[k] & ~(1u << d));
        }
    return pzb_end(&g->hist);
}

int cdk_note(cdk_game *g, int c, int d)
{
    if (c < 0 || c >= cells(g) || d < 1 || d > g->p.n || g->val[c]) return 0;
    pzb_begin(&g->hist, 1);
    set(g, c, 0, g->notes[c] ^ (1u << d));
    return pzb_end(&g->hist);
}

int cdk_erase(cdk_game *g, int c)
{
    if (c < 0 || c >= cells(g) || (!g->val[c] && !g->notes[c])) return 0;
    pzb_begin(&g->hist, 1);
    set(g, c, 0, g->val[c] ? g->notes[c] : 0);
    return pzb_end(&g->hist);
}

int cdk_hint(cdk_game *g)
{
    int n = g->p.n, N = cells(g);
    for (int c = 0; c < N; c++)                 /* a wrong digit first */
        if (g->val[c] && g->val[c] != g->p.sol[c]) return cdk_put(g, c, g->p.sol[c]) ? c : -1;
    pzb_rules r;
    ctx_t x;
    pzb_grid s;
    rules_of(&r, &x, &g->p, &s);
    for (int c = 0; c < N; c++)
        if (g->val[c]) s.d[c] = (uint16_t)(1u << g->val[c]);
    int used = 0;
    pzb_settle(&s, &r, 1, &used);               /* what reasoning finds from here: the first such cell */
    int best = -1, bn = 99;
    for (int c = 0; c < N; c++) {
        if (g->val[c]) continue;
        int b = pzb_bits(s.d[c]);
        if (b < bn) bn = b, best = c;
    }
    (void)n;
    if (best < 0) return -1;
    return cdk_put(g, best, g->p.sol[best]) ? best : -1;
}

int cdk_undo(cdk_game *g) { return pzb_undo(&g->hist, restore, g); }

int cdk_clash(const cdk_game *g, int c)
{
    int n = g->p.n, d = g->val[c];
    if (!d) return 0;
    for (int i = 0; i < n; i++) {
        int r = c / n * n + i, k = i * n + c % n;
        if ((r != c && g->val[r] == d) || (k != c && g->val[k] == d)) return 1;
    }
    const cdk_cage *k = &g->p.cage[g->p.cage_of[c]];
    int v[CDK_CAGE];
    for (int i = 0; i < k->n; i++)
        if (!(v[i] = g->val[k->cell[i]])) return 0;
    return cdk_value(k->op, v, k->n) != k->target;
}

int cdk_solved(const cdk_game *g)
{
    for (int c = 0; c < cells(g); c++)
        if (g->val[c] != g->p.sol[c]) return 0;
    return 1;
}

int cdk_count_digit(const cdk_game *g, int d)
{
    int k = 0;
    for (int c = 0; c < cells(g); c++) k += g->val[c] == d;
    return k;
}

int cdk_script(cdk_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (cdk_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < cells(g) && k > 0; c++)
                if (g->val[c] != g->p.sol[c]) { cdk_put(g, c, g->p.sol[c]); k--; }
            continue;
        }
        int len, c = pzb_cell(s, n, n, &len);
        if (c < 0 || (s[len] != '=' && s[len] != '\'') || s[len + 1] < '0' || s[len + 1] > '9') return 0;
        int ok = s[len] == '=' ? cdk_put(g, c, s[len + 1] - '0') : cdk_note(g, c, s[len + 1] - '0');
        if (!ok) return 0;
        s += len + 2;
    }
    return 1;
}
