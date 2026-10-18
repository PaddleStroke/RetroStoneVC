/*
 * Cross Math: the rules (mathcross.h): the equations, the reasoning that grades a puzzle and gives the hints, the
 * count of solutions, the maker, the player's tiles and undo. No SDL: tested by tests/mathcross_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/mathcross.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

/* ---- the equations ----------------------------------------------------------------------------------------------- */
static int is_op(int k) { return k >= MCR_ADD && k <= MCR_DIV; }
static int hi_op(int k) { return k == MCR_MUL || k == MCR_DIV; }
static int operands(const mcr_eq *e) { return e->len == 7 ? 3 : 2; }

static int apply(int a, int op, int b, int *r)
{
    switch (op) {
    case MCR_ADD: *r = a + b; return 1;
    case MCR_SUB: *r = a - b; return a >= b;
    case MCR_MUL: *r = a * b; return 1;
    case MCR_DIV: if (b <= 0 || a % b) return 0; *r = a / b; return 1;
    }
    return 0;
}

int mcr_eval(const int *n, const int *op, int k, int *res)
{
    int t;
    if (k == 2) return apply(n[0], op[0], n[1], res);
    if (hi_op(op[1]) && !hi_op(op[0])) return apply(n[1], op[1], n[2], &t) && apply(n[0], op[0], t, res);
    return apply(n[0], op[0], n[1], &t) && apply(t, op[1], n[2], res);
}

static void eq_ops(const mcr_puzzle *p, const mcr_eq *e, int *ops)
{
    for (int i = 1, n = 0; i < e->len - 2; i += 2) ops[n++] = p->kind[e->cell[i]];
}

int mcr_true(const mcr_puzzle *p, const mcr_eq *e, const int *nums)
{
    int ops[2], r, k = operands(e);
    eq_ops(p, e, ops);
    return nums[k] >= 1 && mcr_eval(nums, ops, k, &r) && r == nums[k];
}

/* the runs of cells across (way 0) and down (way 1): each of more than one cell an equation */
int mcr_parse(mcr_puzzle *p, mcr_eq *eq)
{
    int w = p->w, h = p->h, n = 0;
    uint8_t in[MCR_CELLS];
    memset(in, 0, sizeof in);
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < (way ? w : h); i++)
            for (int j = 0; j < (way ? h : w);) {
                int c = way ? j * w + i : i * w + j;
                if (p->kind[c] == MCR_EMPTY) { j++; continue; }
                int len = 0;
                while (j + len < (way ? h : w) && p->kind[way ? (j + len) * w + i : i * w + j + len] != MCR_EMPTY) len++;
                if (len > 1) {
                    if ((len != 5 && len != 7) || n >= MCR_EQS) return 0;
                    mcr_eq *e = &eq[n++];
                    e->len = len;
                    for (int k = 0; k < len; k++) {
                        int d = way ? (j + k) * w + i : i * w + j + k, kd = p->kind[d];
                        e->cell[k] = d;
                        in[d]++;
                        if (k % 2 == 0 ? kd != MCR_NUM : k == len - 2 ? kd != MCR_EQ : !is_op(kd)) return 0;
                    }
                }
                j += len;
            }
    for (int c = 0; c < w * h; c++)             /* every cell in an equation; only numbers cross */
        if (p->kind[c] != MCR_EMPTY && (!in[c] || (in[c] > 1 && p->kind[c] != MCR_NUM))) return 0;
    return n;
}

/* ---- the reasoning ----------------------------------------------------------------------------------------------- */
typedef struct {
    const mcr_puzzle *p;
    int neq;
    mcr_eq eq[MCR_EQS];
    int8_t eqs_of[MCR_CELLS][2];
    int nv;
    int16_t tv[MCR_GAPS];                       /* the tray's values, each once */
} solver;

typedef struct {
    int16_t val[MCR_CELLS];                     /* a number cell's number, 0 not known */
    int8_t left[MCR_GAPS];                      /* the tiles of each value still in the tray */
} sstate;

static int solver_init(solver *S, sstate *st, const mcr_puzzle *p)
{
    memset(S, 0, sizeof *S);
    memset(st, 0, sizeof *st);
    S->p = p;
    mcr_puzzle q = *p;
    S->neq = mcr_parse(&q, S->eq);
    if (!S->neq) return 0;
    memset(S->eqs_of, -1, sizeof S->eqs_of);
    for (int e = 0; e < S->neq; e++)
        for (int i = 0; i < S->eq[e].len; i += 2) {
            int c = S->eq[e].cell[i];
            S->eqs_of[c][S->eqs_of[c][0] >= 0] = (int8_t)e;
        }
    for (int c = 0; c < p->w * p->h; c++) {
        if (p->kind[c] != MCR_NUM) continue;
        if (!p->gap[c]) { st->val[c] = p->val[c]; continue; }
        int v = 0;
        while (v < S->nv && S->tv[v] != p->val[c]) v++;
        if (v == S->nv) {
            if (S->nv >= MCR_GAPS) return 0;
            S->tv[S->nv++] = p->val[c];
        }
        st->left[v]++;
    }
    return 1;
}

/* the equation's numbers as known (0: not known) */
static int eq_nums(const solver *S, const sstate *st, const mcr_eq *e, int *nums)
{
    (void)S;
    int k = 0;
    for (int i = 0; i < e->len; i += 2) nums[k++] = st->val[e->cell[i]];
    return k;
}

/* the unknown numbers of an equation (from slot i on) given tiles of the tray: 1 when some way makes it true */
static int fill(const solver *S, const mcr_eq *e, int *nums, int k, int i, int8_t *left)
{
    if (i == k) return mcr_true(S->p, e, nums);
    if (nums[i]) return fill(S, e, nums, k, i + 1, left);
    for (int v = 0; v < S->nv; v++) {
        if (left[v] <= 0) continue;
        left[v]--;
        nums[i] = S->tv[v];
        int ok = fill(S, e, nums, k, i + 1, left);
        left[v]++;
        if (ok) {
            nums[i] = 0;
            return 1;
        }
    }
    nums[i] = 0;
    return 0;
}

/* the tray's values (bit v) that can go in an unknown cell, every equation through it still possible */
static uint64_t cands(const solver *S, sstate *st, int c)
{
    uint64_t m = 0;
    for (int v = 0; v < S->nv; v++) {
        if (st->left[v] <= 0) continue;
        st->left[v]--;
        st->val[c] = S->tv[v];
        int ok = 1;
        for (int j = 0; j < 2 && ok && S->eqs_of[c][j] >= 0; j++) {
            const mcr_eq *e = &S->eq[S->eqs_of[c][j]];
            int nums[4], k = eq_nums(S, st, e, nums);
            ok = fill(S, e, nums, k, 0, st->left);
        }
        st->val[c] = 0;
        st->left[v]++;
        if (ok) m |= 1ull << v;
    }
    return m;
}

static void assign(const solver *S, sstate *st, int c, int v)
{
    st->val[c] = S->tv[v];
    st->left[v]--;
}

/* one step of reasoning: 1 a cell found (*cell, *tier: 0 an equation missing one number, 1 the others), 0 stuck, -1
 * a contradiction */
static int step(const solver *S, sstate *st, int *cell, int *tier)
{
    const mcr_puzzle *p = S->p;
    for (int e = 0; e < S->neq; e++) {          /* an equation missing one number */
        const mcr_eq *q = &S->eq[e];
        int nums[4], k = eq_nums(S, st, q, nums), miss = -1, n = 0;
        for (int i = 0; i < k; i++)
            if (!nums[i]) miss = i, n++;
        if (n != 1) continue;
        int found = -1, ways = 0;
        for (int v = 0; v < S->nv; v++) {
            if (st->left[v] <= 0) continue;
            nums[miss] = S->tv[v];
            if (mcr_true(p, q, nums)) found = v, ways++;
        }
        if (!ways) return -1;
        if (ways == 1) {
            *cell = q->cell[2 * miss];
            *tier = 0;
            assign(S, st, *cell, found);
            return 1;
        }
    }
    uint64_t can[MCR_CELLS];
    int open = 0;
    for (int c = 0; c < p->w * p->h; c++) {     /* a gap one value fits */
        if (p->kind[c] != MCR_NUM || st->val[c]) continue;
        open++;
        can[c] = cands(S, st, c);
        if (!can[c]) return -1;
        if (!(can[c] & (can[c] - 1))) {
            *cell = c;
            *tier = 1;
            assign(S, st, c, __builtin_ctzll(can[c]));
            return 1;
        }
    }
    if (!open) return 0;
    for (int v = 0; v < S->nv; v++) {           /* a value only so many gaps take: as many as its tiles */
        if (st->left[v] <= 0) continue;
        int n = 0, first = -1;
        for (int c = 0; c < p->w * p->h; c++)
            if (p->kind[c] == MCR_NUM && !st->val[c] && (can[c] >> v & 1)) n++, first = first < 0 ? c : first;
        if (n < st->left[v]) return -1;
        if (n == st->left[v]) {
            *cell = first;
            *tier = 1;
            assign(S, st, first, v);
            return 1;
        }
    }
    return 0;
}

static int all_known(const solver *S, const sstate *st)
{
    for (int c = 0; c < S->p->w * S->p->h; c++)
        if (S->p->kind[c] == MCR_NUM && !st->val[c]) return 0;
    return 1;
}

int mcr_reason(const mcr_puzzle *p, int *medium)
{
    static solver S;
    sstate st;
    int cell, tier, mediums = 0;
    if (medium) *medium = 0;
    if (!solver_init(&S, &st, p)) return 0;
    for (;;) {
        if (all_known(&S, &st)) break;
        int r = step(&S, &st, &cell, &tier);
        if (r <= 0) return 0;
        mediums += tier;
    }
    for (int e = 0; e < S.neq; e++) {
        int nums[4];
        eq_nums(&S, &st, &S.eq[e], nums);
        if (!mcr_true(p, &S.eq[e], nums)) return 0;
    }
    if (medium) *medium = mediums;
    return 1;
}

static int count_in(const solver *S, sstate *st, int limit)
{
    const mcr_puzzle *p = S->p;
    int best = -1, bn = 99;
    uint64_t bm = 0;
    for (int c = 0; c < p->w * p->h; c++) {
        if (p->kind[c] != MCR_NUM || st->val[c]) continue;
        uint64_t m = cands(S, st, c);
        int n = __builtin_popcountll(m);
        if (!n) return 0;
        if (n < bn) bn = n, best = c, bm = m;
    }
    if (best < 0) {
        for (int e = 0; e < S->neq; e++) {
            int nums[4];
            eq_nums(S, st, &S->eq[e], nums);
            if (!mcr_true(p, &S->eq[e], nums)) return 0;
        }
        return 1;
    }
    int count = 0;
    for (int v = 0; v < S->nv && count < limit; v++) {
        if (!(bm >> v & 1)) continue;
        sstate t = *st;
        assign(S, &t, best, v);
        count += count_in(S, &t, limit - count);
    }
    return count;
}

int mcr_count(const mcr_puzzle *p, int limit)
{
    static solver S;
    sstate st;
    if (!solver_init(&S, &st, p)) return 0;
    return count_in(&S, &st, limit);
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
typedef struct {
    mcr_puzzle p;
    uint8_t dirs[MCR_CELLS];                    /* a number's equations: bit 0 across, bit 1 down */
} sheet;

/* a step of an equation as the make likes it: small sums, factors and quotients of the tables, no x 1, no / 1 */
static int nice_step(const mcr_make *m, int a, int op, int b, int r)
{
    switch (op) {
    case MCR_ADD: return r <= m->most;
    case MCR_SUB: return a <= m->most && r >= 1;
    case MCR_MUL: return a >= 2 && b >= 2 && a <= m->factor && b <= m->factor;
    case MCR_DIV: return b >= 2 && b <= m->factor && r >= 2 && r <= m->factor;
    }
    return 0;
}

static int nice(const mcr_make *m, const int *n, const int *op, int k, int *res)
{
    int t, r;
    if (k == 2) return mcr_eval(n, op, 2, &r) && nice_step(m, n[0], op[0], n[1], r) && (*res = r) >= 1;
    if (hi_op(op[1]) && !hi_op(op[0])) {
        if (!apply(n[1], op[1], n[2], &t) || !nice_step(m, n[1], op[1], n[2], t)) return 0;
        if (!apply(n[0], op[0], t, &r) || !(op[0] == MCR_ADD ? r <= 2 * m->most : r >= 1)) return 0;
    } else {
        if (!apply(n[0], op[0], n[1], &t) || !nice_step(m, n[0], op[0], n[1], t)) return 0;
        if (!apply(t, op[1], n[2], &r)) return 0;
        if (hi_op(op[1]) ? !nice_step(m, t, op[1], n[2], r) : op[1] == MCR_ADD ? r > 2 * m->most : r < 1) return 0;
    }
    *res = r;
    return r >= 1 && r <= 999;
}

static int random_op(const mcr_make *m, uint32_t *s)
{
    int ops[4], n = 0;
    for (int k = MCR_ADD; k <= MCR_DIV; k++)
        if (m->ops >> k & 1) ops[n++] = k;
    return n ? ops[pr_below(s, n)] : MCR_ADD;
}

/* the numbers of an equation (k operands) with the operations and some numbers fixed (fix[i]: 0 free): 1 found */
static int numbers(const mcr_make *m, uint32_t *s, int k, int *op, const int *fix, int *out)
{
    int top = m->most > m->factor * m->factor ? m->most : m->factor * m->factor;
    for (int t = 0; t < 60; t++) {
        for (int i = 0; i < k - 1; i++) op[i] = random_op(m, s);
        int n[4], freev[3], nf = 0;
        for (int i = 0; i < k; i++) {
            n[i] = fix[i];
            if (fix[i]) continue;
            freev[nf++] = i;
            int by_mul = (i > 0 && hi_op(op[i - 1])) || (i < k - 1 && op[i] == MCR_MUL);
            n[i] = by_mul ? 2 + pr_below(s, m->factor - 1) : 1 + pr_below(s, m->most);
        }
        int solve = nf ? freev[pr_below(s, nf)] : -1, got[1000], ng = 0;
        for (int v = 1; v <= (solve < 0 ? 1 : top); v++) {
            if (solve >= 0) n[solve] = v;
            int r;
            if (!nice(m, n, op, k, &r) || (fix[k] && r != fix[k])) continue;
            got[ng++] = v;
        }
        if (!ng) continue;
        if (solve >= 0) n[solve] = got[pr_below(s, ng)];
        int r;
        nice(m, n, op, k, &r);
        for (int i = 0; i < k; i++) out[i] = n[i];
        out[k] = r;
        return 1;
    }
    return 0;
}

/* an equation laid from (x, y) along way (0 across, 1 down), len cells: 1 when it fits and its numbers were found */
static int lay(sheet *m, const mcr_make *mk, uint32_t *s, int x, int y, int way, int len)
{
    mcr_puzzle *p = &m->p;
    int w = p->w, h = p->h, dx = !way, dy = way, k = len == 7 ? 3 : 2, fix[4] = {0, 0, 0, 0}, nfix = 0;
    int ex = x + dx * (len - 1), ey = y + dy * (len - 1);
    if (x < 0 || y < 0 || ex >= w || ey >= h) return 0;
    if (x - dx >= 0 && y - dy >= 0 && p->kind[(y - dy) * w + x - dx] != MCR_EMPTY) return 0;
    if (ex + dx < w && ey + dy < h && p->kind[(ey + dy) * w + ex + dx] != MCR_EMPTY) return 0;
    for (int i = 0; i < len; i++) {
        int cx = x + dx * i, cy = y + dy * i, c = cy * w + cx;
        if (p->kind[c] != MCR_EMPTY) {
            if (i % 2 || i == len - 2 || p->kind[c] != MCR_NUM || (m->dirs[c] >> way & 1)) return 0;
            fix[i / 2] = p->val[c];
            nfix++;
            continue;
        }
        for (int side = -1; side <= 1; side += 2) {   /* nothing beside a new cell */
            int nx = cx + dy * side, ny = cy + dx * side;
            if (nx >= 0 && ny >= 0 && nx < w && ny < h && p->kind[ny * w + nx] != MCR_EMPTY) return 0;
        }
    }
    if (nfix > 2) return 0;
    int op[2], out[4];
    if (!numbers(mk, s, k, op, fix, out)) return 0;
    for (int i = 0; i < len; i++) {
        int c = (y + dy * i) * w + x + dx * i;
        if (i % 2 == 0) {
            p->kind[c] = MCR_NUM;
            p->val[c] = (int16_t)out[i / 2];
            m->dirs[c] |= (uint8_t)(1 << way);
        } else {
            p->kind[c] = (uint8_t)(i == len - 2 ? MCR_EQ : op[i / 2]);
        }
    }
    return 1;
}

/* the board cut to its cells */
static void crop(mcr_puzzle *p)
{
    int x0 = p->w, y0 = p->h, x1 = -1, y1 = -1;
    for (int c = 0; c < p->w * p->h; c++)
        if (p->kind[c] != MCR_EMPTY) {
            int x = c % p->w, y = c / p->w;
            x0 = x < x0 ? x : x0, x1 = x > x1 ? x : x1, y0 = y < y0 ? y : y0, y1 = y > y1 ? y : y1;
        }
    if (x1 < 0) return;
    mcr_puzzle q;
    memset(&q, 0, sizeof q);
    q.w = x1 - x0 + 1;
    q.h = y1 - y0 + 1;
    for (int y = 0; y < q.h; y++)
        for (int x = 0; x < q.w; x++) {
            int a = (y + y0) * p->w + x + x0, b = y * q.w + x;
            q.kind[b] = p->kind[a];
            q.val[b] = p->val[a];
            q.gap[b] = p->gap[a];
        }
    *p = q;
}

void mcr_make_puzzle(const mcr_make *mk, uint32_t seed, mcr_puzzle *out)
{
    uint32_t s = pr_seed(seed);
    static sheet m, best;
    int best_score = -1;
    for (int attempt = 0; attempt < 60; attempt++) {
        memset(&m, 0, sizeof m);
        m.p.w = mk->w;
        m.p.h = mk->h;
        int len = pr_below(&s, 100) < mk->three ? 7 : 5, way = mk->w >= mk->h ? 0 : 1;
        int x = way ? mk->w / 2 - 1 + pr_below(&s, 3) : pr_below(&s, mk->w - len + 1);
        int y = way ? pr_below(&s, mk->h - len + 1) : mk->h / 2 - 1 + pr_below(&s, 3);
        if (!lay(&m, mk, &s, x, y, way, len)) continue;
        int neq = 1;
        for (int fails = 0; neq < mk->eqs && fails < 400; fails++) {
            int c = pr_below(&s, mk->w * mk->h);
            if (m.p.kind[c] != MCR_NUM || m.dirs[c] == 3) continue;
            way = m.dirs[c] & 1 ? 1 : 0;
            len = pr_below(&s, 100) < mk->three ? 7 : 5;
            int k = len == 7 ? 3 : 2, slot = pr_below(&s, k + 1);
            int sx = c % mk->w - (!way) * 2 * slot, sy = c / mk->w - way * 2 * slot;
            if (lay(&m, mk, &s, sx, sy, way, len)) neq++;
        }
        if (neq < mk->eqs) continue;
        /* the gaps: numbers taken away in a random order while reasoning still finds them all */
        int cells[MCR_CELLS], n = 0, ngap = 0;
        for (int c = 0; c < mk->w * mk->h; c++)
            if (m.p.kind[c] == MCR_NUM) cells[n++] = c;
        pr_shuffle(&s, cells, n);
        int want = (n * mk->gaps + 50) / 100;
        if (want > MCR_GAPS) want = MCR_GAPS;
        for (int i = 0; i < n && ngap < want; i++) {
            m.p.gap[cells[i]] = 1;
            if (mcr_reason(&m.p, NULL)) ngap++;
            else m.p.gap[cells[i]] = 0;
        }
        int score = ngap * 100 / (want ? want : 1);
        if (score > best_score) best_score = score, best = m;
        if (score >= 90 && ngap >= 2) break;
    }
    if (best_score < 0) {                       /* never laid: one equation */
        memset(&best, 0, sizeof best);
        best.p.w = 5;
        best.p.h = 1;
        const uint8_t k[5] = {MCR_NUM, MCR_ADD, MCR_NUM, MCR_EQ, MCR_NUM};
        memcpy(best.p.kind, k, 5);
        best.p.val[0] = 1, best.p.val[2] = 2, best.p.val[4] = 3, best.p.gap[4] = 1;
    }
    *out = best.p;
    crop(out);
}

mcr_make mcr_variant_make(int variant)
{
    static const mcr_make V[MCR_VARIANTS] = {
        {7, 7, 4, 1u << MCR_ADD | 1u << MCR_SUB, 20, 10, 0, 45},
        {9, 9, 6, 1u << MCR_ADD | 1u << MCR_SUB | 1u << MCR_MUL | 1u << MCR_DIV, 50, 10, 0, 55},
        {11, 11, 8, 1u << MCR_ADD | 1u << MCR_SUB | 1u << MCR_MUL | 1u << MCR_DIV, 99, 12, 40, 65},
    };
    return V[variant < 0 || variant >= MCR_VARIANTS ? 0 : variant];
}

/* ---- a puzzle as text -------------------------------------------------------------------------------------------- */
static const char KIND[] = ".#+-*/=";

int mcr_encode(const mcr_puzzle *p, char *out, int size)
{
    int len = snprintf(out, (size_t)size, "%dx%d:", p->w, p->h);
    for (int c = 0; c < p->w * p->h && len < size - 1; c++) out[len++] = KIND[p->kind[c]];
    if (len < size - 1) out[len++] = ':';
    int first = 1;
    for (int c = 0; c < p->w * p->h; c++) {
        if (p->kind[c] != MCR_NUM) continue;
        int k = snprintf(out + len, (size_t)(size - len), "%s%s%d", first ? "" : ",", p->gap[c] ? "?" : "", p->val[c]);
        if (k < 0 || len + k >= size) return 0;
        len += k;
        first = 0;
    }
    out[len] = 0;
    return len;
}

int mcr_decode(const char *s, mcr_puzzle *p)
{
    memset(p, 0, sizeof *p);
    char *e;
    long w = strtol(s, &e, 10);
    if (*e != 'x') return 0;
    long h = strtol(e + 1, &e, 10);
    if (*e != ':' || w < 1 || h < 1 || w > MCR_MAXW || h > MCR_MAXH) return 0;
    p->w = (int)w;
    p->h = (int)h;
    s = e + 1;
    for (int c = 0; c < p->w * p->h; c++) {
        const char *k = s[c] ? strchr(KIND, s[c]) : NULL;
        if (!k) return 0;
        p->kind[c] = (uint8_t)(k - KIND);
    }
    s += p->w * p->h;
    if (*s != ':') return 0;
    s++;
    for (int c = 0; c < p->w * p->h; c++) {
        if (p->kind[c] != MCR_NUM) continue;
        if (*s == ',') s++;
        if (*s == '?') p->gap[c] = 1, s++;
        long v = strtol(s, &e, 10);
        if (e == s || v < 1 || v > 999) return 0;
        p->val[c] = (int16_t)v;
        s = e;
    }
    return *s == 0;
}

/* ---- the player's tiles ------------------------------------------------------------------------------------------ */
int mcr_load(mcr_game *g, const mcr_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->neq = mcr_parse(&g->p, g->eq);
    for (int c = 0; c < p->w * p->h; c++)
        if (p->kind[c] == MCR_NUM && p->gap[c] && g->ngap < MCR_GAPS) g->gaps[g->ngap++] = c;
    g->ntray = g->ngap;
    for (int i = 0; i < g->ngap; i++) g->tray[i] = p->val[g->gaps[i]];
    for (int i = 1; i < g->ntray; i++)          /* the tray in order */
        for (int j = i; j > 0 && g->tray[j - 1] > g->tray[j]; j--) {
            int16_t t = g->tray[j];
            g->tray[j] = g->tray[j - 1];
            g->tray[j - 1] = t;
        }
    memset(g->at, -1, sizeof g->at);
    return g->neq > 0 && g->ngap > 0;
}

void mcr_new(mcr_game *g, int variant, uint32_t seed)
{
    mcr_puzzle p;
    mcr_make m = mcr_variant_make(variant);
    mcr_make_puzzle(&m, seed, &p);
    mcr_load(g, &p);
}

int mcr_gap_of(const mcr_game *g, int cell)
{
    for (int i = 0; i < g->ngap; i++)
        if (g->gaps[i] == cell) return i;
    return -1;
}

int mcr_holder(const mcr_game *g, int t)
{
    for (int i = 0; i < g->ngap; i++)
        if (g->at[i] == t) return i;
    return -1;
}

int mcr_tile_free(const mcr_game *g, int t) { return t >= 0 && t < g->ntray && mcr_holder(g, t) < 0; }

int mcr_value(const mcr_game *g, int cell)
{
    if (g->p.kind[cell] != MCR_NUM) return 0;
    if (!g->p.gap[cell]) return g->p.val[cell];
    int i = mcr_gap_of(g, cell);
    return i >= 0 && g->at[i] >= 0 ? g->tray[(int)g->at[i]] : 0;
}

int mcr_eq_state(const mcr_game *g, int e)
{
    const mcr_eq *q = &g->eq[e];
    int nums[4], k = 0;
    for (int i = 0; i < q->len; i += 2) {
        nums[k] = mcr_value(g, q->cell[i]);
        if (!nums[k++]) return 0;
    }
    return mcr_true(&g->p, q, nums) ? 1 : -1;
}

static int wrong_eqs(const mcr_game *g)
{
    int n = 0;
    for (int e = 0; e < g->neq; e++) n += mcr_eq_state(g, e) < 0;
    return n;
}

static void push(mcr_game *g)
{
    if (g->nh == MCR_HIST) {
        memmove(g->hist[0], g->hist[1], sizeof g->hist[0] * (MCR_HIST - 1));
        g->nh--;
    }
    memcpy(g->hist[g->nh++], g->at, sizeof g->at);
    g->moves++;
}

static void settle(mcr_game *g)                 /* after an action: a full equation gone wrong is a mistake */
{
    int w = wrong_eqs(g);
    if (w > g->wrong_eqs) g->mistakes += w - g->wrong_eqs;
    g->wrong_eqs = w;
}

int mcr_put(mcr_game *g, int i, int t)
{
    if (i < 0 || i >= g->ngap || t < 0 || t >= g->ntray || g->at[i] == t) return 0;
    push(g);
    int j = mcr_holder(g, t);
    if (j >= 0) g->at[j] = -1;
    g->at[i] = (int8_t)t;
    settle(g);
    return 1;
}

int mcr_take(mcr_game *g, int i)
{
    if (i < 0 || i >= g->ngap || g->at[i] < 0) return 0;
    push(g);
    g->at[i] = -1;
    settle(g);
    return 1;
}

int mcr_move(mcr_game *g, int i, int j)
{
    if (i < 0 || j < 0 || i >= g->ngap || j >= g->ngap || i == j || g->at[i] < 0) return 0;
    push(g);
    int8_t t = g->at[i];
    g->at[i] = g->at[j];
    g->at[j] = t;
    settle(g);
    return 1;
}

int mcr_undo(mcr_game *g)
{
    if (!g->nh) return 0;
    memcpy(g->at, g->hist[--g->nh], sizeof g->at);
    g->moves--;
    g->wrong_eqs = wrong_eqs(g);
    return 1;
}

/* the gap i put right: a tile of its number from the tray (or from a gap where it is wrong) */
static int right(mcr_game *g, int i)
{
    int want = g->p.val[g->gaps[i]], pick = -1;
    for (int t = 0; t < g->ntray && pick < 0; t++)
        if (g->tray[t] == want && mcr_tile_free(g, t)) pick = t;
    for (int t = 0; t < g->ntray && pick < 0; t++) {
        int h = mcr_holder(g, t);
        if (g->tray[t] == want && h >= 0 && g->p.val[g->gaps[h]] != want) pick = t;
    }
    return pick >= 0 && mcr_put(g, i, pick) ? i : -1;
}

static int is_right(const mcr_game *g, int i) { return g->at[i] >= 0 && g->tray[(int)g->at[i]] == g->p.val[g->gaps[i]]; }

static int hint_step(mcr_game *g)
{
    for (int i = 0; i < g->ngap; i++)           /* a wrong tile first */
        if (g->at[i] >= 0 && !is_right(g, i)) return right(g, i);
    static solver S;
    sstate st;
    mcr_puzzle q = g->p;                        /* the right tiles as given; the reasoning finds the next gap */
    for (int i = 0; i < g->ngap; i++)
        if (is_right(g, i)) q.gap[g->gaps[i]] = 0;
    int cell, tier;
    if (solver_init(&S, &st, &q) && step(&S, &st, &cell, &tier) > 0) {
        int i = mcr_gap_of(g, cell);
        if (i >= 0) return right(g, i);
    }
    for (int i = 0; i < g->ngap; i++)
        if (!is_right(g, i)) return right(g, i);
    return -1;
}

int mcr_hint(mcr_game *g)
{
    int i = hint_step(g);
    if (i >= 0) g->hints++;
    return i;
}

int mcr_solved(const mcr_game *g)
{
    if (!g->neq || !g->ngap) return 0;
    for (int i = 0; i < g->ngap; i++)
        if (g->at[i] < 0) return 0;
    for (int e = 0; e < g->neq; e++)
        if (mcr_eq_state(g, e) != 1) return 0;
    return 1;
}

static int cell_at(const mcr_game *g, const char **s)
{
    const char *t = *s;
    if (*t < 'a' || *t >= 'a' + g->p.w) return -1;
    int x = *t++ - 'a', y = 0;
    if (*t < '1' || *t > '9') return -1;
    while (*t >= '0' && *t <= '9') y = y * 10 + *t++ - '0';
    if (y < 1 || y > g->p.h) return -1;
    *s = t;
    return (y - 1) * g->p.w + x;
}

int mcr_script(mcr_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (mcr_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int i = 0; i < g->ngap && k > 0; i++)
                if (!is_right(g, i)) right(g, i), k--;
            continue;
        }
        int c = cell_at(g, &s), i = c < 0 ? -1 : mcr_gap_of(g, c);
        if (i < 0) return 0;
        if (*s == '=') {
            s++;
            int v = 0, t = -1;
            while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0';
            for (int k = 0; k < g->ntray && t < 0; k++)
                if (g->tray[k] == v && mcr_tile_free(g, k)) t = k;
            for (int k = 0; k < g->ntray && t < 0; k++)
                if (g->tray[k] == v) t = k;
            if (t < 0 || !mcr_put(g, i, t)) return 0;
        } else if (*s == '>') {
            s++;
            int d = cell_at(g, &s), j = d < 0 ? -1 : mcr_gap_of(g, d);
            if (!mcr_move(g, i, j)) return 0;
        } else if (!mcr_take(g, i)) {
            return 0;
        }
    }
    return 1;
}
