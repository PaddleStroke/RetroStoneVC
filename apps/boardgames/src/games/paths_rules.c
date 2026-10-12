/*
 * Paths: the rules (paths.h): the solver that counts the solutions, the maker (paths grown at random over the grid,
 * kept when the solution is the only one), and the player's paths drawn by strokes, with undo and hints. No SDL:
 * tested by tests/paths_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/paths.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

#define NK_SOLVE 32                             /* the most pairs the solver takes (the maker starts with more) */
#define NK_TRIES 300                            /* the maker's tries before it takes more pairs than wanted */


static int nbr(int n, int c, int d)
{
    int x = c % n + (d == 1) - (d == 3), y = c / n + (d == 2) - (d == 0);
    return x < 0 || y < 0 || x >= n || y >= n ? -1 : y * n + x;
}

static int adjacent(int n, int a, int b)
{
    int dx = a % n - b % n, dy = a / n - b / n;
    return dx * dx + dy * dy == 1;
}

void nk_variant(int v, int *n, int *lo, int *hi)
{
    static const int size[NK_VARIANTS] = {5, 7, 9, 12}, low[NK_VARIANTS] = {3, 5, 7, 10},
                     high[NK_VARIANTS] = {5, 8, 10, 13};
    if (v < 0 || v >= NK_VARIANTS) v = 1;
    *n = size[v];
    *lo = low[v];
    *hi = high[v];
}

/* ---- the solver --------------------------------------------------------------------------------------------------
 * The links between neighbouring squares (on: one path goes from one to the other; off: not) and the colours each
 * square may still take. Deductions, until nothing more is found: a dot has one link, any other square two (when it
 * has them, its other links are off; when it has only as many left as it needs, they are on); two linked squares
 * share their colour, two squares that can't share one are not linked; a link that would close a loop is off; each
 * colour's two dots must be joined through squares that may take that colour (the others lose it). Then a guess (a
 * square's colour, or a link), and each guess followed through. */
typedef struct {
    uint8_t e[NK_CELLS];                        /* bits 0..3: linked up, right, down, left; 4..7: not */
    uint32_t col[NK_CELLS];                     /* the colours it may take */
} pstate;

typedef struct {
    int n, nc, pairs, count, limit, aborted;
    long steps, budget;
    int nb[NK_CELLS][4];
    uint8_t need[NK_CELLS];
    int dot[NK_SOLVE][2];
} solver;

static int link_set(const solver *s, pstate *p, int a, int d, int on, int *changed)
{
    int b = s->nb[a][d], o = (d + 2) & 3;
    unsigned want = on ? 1u << d : 16u << d;
    if (p->e[a] & want) return 1;
    if (p->e[a] & (on ? 16u << d : 1u << d)) return 0;
    p->e[a] |= (uint8_t)want;
    if (b >= 0) p->e[b] |= (uint8_t)(on ? 1u << o : 16u << o);
    *changed = 1;
    return 1;
}

static const uint8_t POP4[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

static int root(int *par, int x)
{
    while (par[x] != x) x = par[x] = par[par[x]];
    return x;
}

/* a depth-first walk over the squares that may take a colour (through links not off), from its first dot: each
 * square's order, the lowest order its subtree reaches back to, its parent */
typedef struct {
    const solver *s;
    const pstate *p;
    uint32_t bit;
    int t;
    int disc[NK_CELLS], low[NK_CELLS], up[NK_CELLS];
} sep_walk;

static void sep_dfs(sep_walk *w, int a, int parent)
{
    w->disc[a] = w->low[a] = ++w->t;
    w->up[a] = parent;
    for (int d = 0; d < 4; d++) {
        int b = w->s->nb[a][d];
        if (b < 0 || (w->p->e[a] >> (d + 4) & 1) || !(w->p->col[b] & w->bit)) continue;
        if (!w->disc[b]) {
            sep_dfs(w, b, a);
            if (w->low[b] < w->low[a]) w->low[a] = w->low[b];
        } else if (b != parent && w->disc[b] < w->low[a]) {
            w->low[a] = w->disc[b];
        }
    }
}

static int propagate(const solver *s, pstate *p)
{
    int nc = s->nc, changed = 1, par[NK_CELLS], queue[NK_CELLS];
    uint32_t cm[NK_CELLS];
    uint8_t seen[NK_CELLS];
    while (changed) {
        changed = 0;
        for (int a = 0; a < nc; a++) {         /* the links each square needs */
            int on = POP4[p->e[a] & 15], off = POP4[p->e[a] >> 4], unk = 4 - on - off;
            if (on > s->need[a] || on + unk < s->need[a]) return 0;
            if (!unk || (on != s->need[a] && on + unk != s->need[a])) continue;
            int to_on = on != s->need[a];
            for (int d = 0; d < 4; d++)
                if (!(p->e[a] >> d & 0x11) && !link_set(s, p, a, d, to_on, &changed)) return 0;
        }
        for (int a = 0; a < nc; a++) par[a] = a, cm[a] = ~0u;   /* the pieces of paths linked so far: no loop */
        for (int a = 0; a < nc; a++)
            for (int d = 1; d <= 2; d++)
                if (p->e[a] >> d & 1) {
                    int x = root(par, a), y = root(par, s->nb[a][d]);
                    if (x == y) return 0;
                    par[x] = y;
                }
        for (int a = 0; a < nc; a++) par[a] = root(par, a);   /* each square straight to its piece */
        for (int a = 0; a < nc; a++) cm[par[a]] &= p->col[a];   /* a piece has one colour */
        for (int a = 0; a < nc; a++) {
            uint32_t m = cm[par[a]];
            if (!m) return 0;
            if (m != p->col[a]) p->col[a] = m, changed = 1;
        }
        for (int a = 0; a < nc; a++)           /* no link closing a loop, nor between two colours */
            for (int d = 1; d <= 2; d++) {
                int b = s->nb[a][d];
                if (b >= 0 && !(p->e[a] >> d & 0x11) &&
                    (par[a] == par[b] || !(p->col[a] & p->col[b])) && !link_set(s, p, a, d, 0, &changed))
                    return 0;
            }
        if (changed) continue;
        for (int c = 0; c < s->pairs; c++) {   /* each colour's dots joined through squares that may take it */
            uint32_t bit = 1u << c;
            if (par[s->dot[c][0]] == par[s->dot[c][1]]) continue;   /* linked already */
            int qh = 0, qt = 0;
            memset(seen, 0, (size_t)nc);
            seen[s->dot[c][0]] = 1;
            queue[qt++] = s->dot[c][0];
            while (qh < qt) {
                int a = queue[qh++];
                for (int d = 0; d < 4; d++) {
                    int b = s->nb[a][d];
                    if (b < 0 || seen[b] || (p->e[a] >> (d + 4) & 1) || !(p->col[b] & bit)) continue;
                    seen[b] = 1;
                    queue[qt++] = b;
                }
            }
            if (!seen[s->dot[c][1]]) return 0;
            for (int a = 0; a < nc; a++)
                if (!seen[a] && (p->col[a] & bit)) {
                    p->col[a] &= ~bit;
                    if (!p->col[a]) return 0;
                    changed = 1;
                }
            /* the squares every way between the two dots passes through: that colour */
            sep_walk w;
            w.s = s, w.p = p, w.bit = bit, w.t = 0;
            memset(w.disc, 0, sizeof w.disc[0] * (size_t)nc);
            sep_dfs(&w, s->dot[c][0], -1);
            for (int v = w.up[s->dot[c][1]], child = s->dot[c][1]; v >= 0 && v != s->dot[c][0];
                 child = v, v = w.up[v])
                if (w.low[child] >= w.disc[v] && p->col[v] != bit) {
                    p->col[v] = bit;
                    changed = 1;
                }
        }
    }
    return 1;
}

static void search(solver *s, pstate *p)
{
    if (s->count >= s->limit || s->aborted) return;
    if (++s->steps > s->budget) { s->aborted = 1; return; }
    if (!propagate(s, p)) return;
    int best = -1, bn = 99;
    for (int a = 0; a < s->nc; a++) {           /* a path's open end (or a dot alone) with the fewest ways on */
        int on = POP4[p->e[a] & 15], unk = 4 - on - POP4[p->e[a] >> 4];
        if (on < s->need[a] && (on > 0 || s->need[a] == 1) && unk < bn) bn = unk, best = a;
    }
    if (best >= 0) {                            /* exactly one of its open sides gets the link */
        for (int d = 0; d < 4 && s->count < s->limit && !s->aborted; d++) {
            if (p->e[best] >> d & 0x11) continue;
            int ch;
            pstate q = *p;
            for (int k = 0; k < 4; k++)
                if (!(q.e[best] >> k & 0x11)) link_set(s, &q, best, k, k == d, &ch);
            search(s, &q);
        }
        return;
    }
    for (int a = 0; a < s->nc; a++)             /* no open end: a link not yet known */
        for (int d = 1; d <= 2; d++)
            if (s->nb[a][d] >= 0 && !(p->e[a] >> d & 0x11)) {
                int ch;
                pstate q = *p;
                link_set(s, &q, a, d, 1, &ch);
                search(s, &q);
                q = *p;
                link_set(s, &q, a, d, 0, &ch);
                search(s, &q);
                return;
            }
    s->count++;                                 /* every link known: one solution */
}

int nk_count(int n, int pairs, const uint8_t (*end)[2], int limit, long budget)
{
    static solver s;
    static pstate p;
    memset(&s, 0, sizeof s);
    memset(&p, 0, sizeof p);
    s.n = n;
    s.nc = n * n;
    s.pairs = pairs;
    s.limit = limit;
    s.budget = budget;
    for (int a = 0; a < s.nc; a++) {
        s.need[a] = 2;
        p.col[a] = pairs >= 32 ? 0xffffffffu : (1u << pairs) - 1;
        for (int d = 0; d < 4; d++) {
            s.nb[a][d] = nbr(n, a, d);
            if (s.nb[a][d] < 0) p.e[a] |= (uint8_t)(16u << d);
        }
    }
    for (int c = 0; c < pairs; c++)
        for (int k = 0; k < 2; k++) {
            s.dot[c][k] = end[c][k];
            s.need[end[c][k]] = 1;
            p.col[end[c][k]] = 1u << c;
        }
    search(&s, &p);
    return s.aborted && s.count < limit ? -1 : s.count;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
typedef struct {
    int n, np, lmax;
    int own[NK_CELLS];                          /* the path on each square (-1: none yet) */
    int len[NK_CELLS];
    uint8_t cell[NK_CELLS][NK_CELLS];           /* each path's squares, end to end */
} cover;

/* 1: square x, put at the end of path p that is square e, would touch the path elsewhere */
static int would_touch(const cover *v, int p, int e, int x)
{
    for (int d = 0; d < 4; d++) {
        int u = nbr(v->n, x, d);
        if (u >= 0 && u != e && v->own[u] == p) return 1;
    }
    return 0;
}

static void reverse(cover *v, int p)
{
    for (int i = 0, j = v->len[p] - 1; i < j; i++, j--) {
        uint8_t t = v->cell[p][i];
        v->cell[p][i] = v->cell[p][j];
        v->cell[p][j] = t;
    }
}

/* paths p and q joined by p's end ep and q's end eq (0 first, 1 last): 1 if they can be (beside each other, and the
 * joined path touches itself nowhere) */
static int can_join(const cover *v, int p, int ep, int q, int eq)
{
    int a = v->cell[p][ep ? v->len[p] - 1 : 0], b = v->cell[q][eq ? v->len[q] - 1 : 0];
    if (!adjacent(v->n, a, b)) return 0;
    for (int i = 0; i < v->len[q]; i++) {
        int y = v->cell[q][i];
        for (int d = 0; d < 4; d++) {
            int z = nbr(v->n, y, d);
            if (z >= 0 && v->own[z] == p && !(y == b && z == a)) return 0;
        }
    }
    return 1;
}

static void join(cover *v, int p, int ep, int q, int eq)
{
    if (!ep) reverse(v, p);                     /* p's joining end last, q's first */
    if (eq) reverse(v, q);
    for (int i = 0; i < v->len[q]; i++) {
        v->cell[p][v->len[p]++] = v->cell[q][i];
        v->own[v->cell[q][i]] = p;
    }
    int last = --v->np;                         /* q goes: the last path takes its place */
    if (q != last) {
        v->len[q] = v->len[last];
        memcpy(v->cell[q], v->cell[last], (size_t)v->len[last]);
        for (int i = 0; i < v->len[q]; i++) v->own[v->cell[q][i]] = q;
    }
}

/* every join of two paths (both ends each) that can be made, the joined length at most max: how many; the first
 * `want`-th of them made (want < 0: none) */
static int joins(cover *v, int max, int only_short, int want)
{
    int k = 0;
    for (int p = 0; p < v->np; p++)
        for (int q = p + 1; q < v->np; q++) {
            if (v->len[p] + v->len[q] > max) continue;
            if (only_short && v->len[p] >= 3 && v->len[q] >= 3) continue;
            for (int e = 0; e < 4; e++)
                if (can_join(v, p, e >> 1, q, e & 1)) {
                    if (k++ == want) { join(v, p, e >> 1, q, e & 1); return k; }
                }
        }
    return k;
}

/* a path (s; only_short: a short one) whose end is beside the middle of another (p, its square b at i): p cut at b,
 * s joined to the part of p that keeps b (from p's first square: head, or the rest), the other part a path of its
 * own (when not only_short, at least 3 long); one picked at random. 0: none can be */
static int split_join(cover *v, uint32_t *rng, int only_short)
{
    static int cand[NK_CELLS * 16];
    int n = v->n, nc = 0, pos[NK_CELLS];
    for (int s = 0; s < v->np; s++) {
        if (only_short && v->len[s] >= 3) continue;
        for (int se = 0; se < 2; se++) {
            int x = v->cell[s][se ? v->len[s] - 1 : 0];
            for (int d = 0; d < 4; d++) {
                int b = nbr(n, x, d), p = b >= 0 ? v->own[b] : -1;
                if (p < 0 || p == s) continue;
                for (int k = 0; k < v->len[p]; k++) pos[v->cell[p][k]] = k;
                int i = pos[b];
                if (i == 0 || i == v->len[p] - 1) continue;
                for (int head = 0; head < 2; head++) {
                    int lo = head ? 0 : i, hi = head ? i : v->len[p] - 1, ok = 1;
                    int rest = head ? v->len[p] - 1 - i : i;   /* the part left over */
                    if (!only_short && (rest < 3 || v->len[s] + hi - lo + 1 > v->lmax)) continue;
                    for (int k = 0; k < v->len[s] && ok; k++) {   /* s touches the kept part only at x-b */
                        int y = v->cell[s][k];
                        for (int e = 0; e < 4 && ok; e++) {
                            int z = nbr(n, y, e);
                            if (z >= 0 && v->own[z] == p && pos[z] >= lo && pos[z] <= hi && !(y == x && z == b)) ok = 0;
                        }
                    }
                    if (ok) cand[nc++] = ((s * 2 + se) * NK_CELLS + b) * 2 + head;
                }
            }
        }
    }
    if (!nc) return 0;
    int c = cand[pr_below(rng, nc)], head = c & 1, b = c / 2 % NK_CELLS, se = c / 2 / NK_CELLS % 2,
        s = c / 2 / NK_CELLS / 2, p = v->own[b], i = 0;
    while (v->cell[p][i] != b) i++;
    int q = v->np++;                            /* the part of p left over: a path of its own */
    if (head) {
        v->len[q] = v->len[p] - i - 1;
        memcpy(v->cell[q], v->cell[p] + i + 1, (size_t)v->len[q]);
        v->len[p] = i + 1;
    } else {
        v->len[q] = i;
        memcpy(v->cell[q], v->cell[p], (size_t)i);
        memmove(v->cell[p], v->cell[p] + i, (size_t)(v->len[p] - i));
        v->len[p] -= i;
    }
    for (int k = 0; k < v->len[q]; k++) v->own[v->cell[q][k]] = q;
    join(v, p, head, s, se);
    return 1;
}

/* the grid covered by paths that touch themselves nowhere, about target of them (lo to hi); 0: failed */
static int make_cover(cover *v, int n, int target, int lo, int hi, uint32_t *rng)
{
    int nc = n * n, order[NK_CELLS];
    int lmax = 2 * nc / target + 2;
    v->n = n;
    v->lmax = lmax;
    v->np = 0;
    for (int i = 0; i < nc; i++) v->own[i] = -1, order[i] = i;
    pr_shuffle(rng, order, nc);
    for (int i = 0; i < nc; i++) {             /* grown at random */
        int s = order[i];
        if (v->own[s] >= 0) continue;
        int p = v->np++, want = 3 + pr_below(rng, lmax - 2);
        v->len[p] = 1;
        v->cell[p][0] = (uint8_t)s;
        v->own[s] = p;
        while (v->len[p] < want) {
            int cand[8], nd = 0;
            for (int end = 0; end < 2; end++) {
                int e = v->cell[p][end ? v->len[p] - 1 : 0];
                for (int d = 0; d < 4; d++) {
                    int x = nbr(n, e, d);
                    if (x >= 0 && v->own[x] < 0 && !would_touch(v, p, e, x)) cand[nd++] = x * 2 + end;
                }
            }
            if (!nd) break;
            int pick = cand[pr_below(rng, nd)], x = pick / 2;
            if (pick & 1) v->cell[p][v->len[p]++] = (uint8_t)x;
            else {
                memmove(v->cell[p] + 1, v->cell[p], (size_t)v->len[p]);
                v->cell[p][0] = (uint8_t)x;
                v->len[p]++;
            }
            v->own[x] = p;
        }
    }
    for (int guard = 0; guard < 4 * nc; guard++) {   /* the short ones joined to a neighbour */
        int k = joins(v, nc, 1, -1);
        if (k) {
            joins(v, nc, 1, pr_below(rng, k));
            continue;
        }
        if (!split_join(v, rng, 1)) break;
    }
    for (int p = 0; p < v->np; p++)
        if (v->len[p] < 3) return 0;
    for (int guard = 0; v->np > target && guard < 40 * nc; guard++) {   /* fewer, longer paths */
        int k = joins(v, lmax, 0, -1);
        if (k) {
            joins(v, lmax, 0, pr_below(rng, k));
            continue;
        }
        if (!split_join(v, rng, 0)) break;   /* none: the paths' ends moved about */
    }
    return v->np >= lo && v->np <= hi && v->np <= NK_SOLVE;
}

/* 1: the puzzle a cover makes (its paths' ends as dots) is solved by the deductions alone, without a guess (so its
 * solution is the only one) */
static int deduced(const cover *v)
{
    uint8_t end[NK_SOLVE][2];
    for (int p = 0; p < v->np; p++) {
        end[p][0] = v->cell[p][0];
        end[p][1] = v->cell[p][v->len[p] - 1];
    }
    return nk_count(v->n, v->np, (const uint8_t (*)[2])end, 2, 1) == 1;
}

int nk_make(int variant, uint32_t seed, uint8_t *colour, uint8_t (*end)[2], int *pairs)
{
    static cover v, w;
    static int cand[NK_SOLVE * NK_SOLVE * 4];
    int n, lo, hi;
    nk_variant(variant, &n, &lo, &hi);
    int nc = n * n, many = hi + 1 + n / 2;      /* first more paths than wanted: easily deduced */
    uint32_t rng = pr_seed(seed);
    for (int tries = 1; tries <= 2 * NK_TRIES; tries++) {
        if (!make_cover(&v, n, many, lo, NK_SOLVE, &rng)) continue;
        if (!deduced(&v)) continue;
        int target = lo + pr_below(&rng, hi - lo + 1), lmax = 5 * nc / (2 * target) + 2, moves = 0;
        while (v.np > target) {                 /* then paths joined, as long as the solution stays the only one */
            int k = 0;
            for (int p = 0; p < v.np; p++)
                for (int q = p + 1; q < v.np; q++)
                    if (v.len[p] + v.len[q] <= lmax)
                        for (int e = 0; e < 4; e++)
                            if (can_join(&v, p, e >> 1, q, e & 1)) cand[k++] = (p * NK_SOLVE + q) * 4 + e;
            pr_shuffle(&rng, cand, k);
            int joined = 0;
            for (int i = 0; i < k && i < 16 && !joined; i++) {
                int e = cand[i] & 3, q = cand[i] / 4 % NK_SOLVE, p = cand[i] / 4 / NK_SOLVE;
                w = v;
                join(&w, p, e >> 1, q, e & 1);
                if (deduced(&w)) v = w, joined = 1;
            }
            for (int i = 0; i < 8 && !joined; i++) {   /* none: a path's end moved about, then joins again */
                w = v;
                if (!split_join(&w, &rng, 0)) break;
                if (deduced(&w)) v = w, joined = 1;
            }
            if (!joined || ++moves > 4 * many) break;
        }
        if (v.np > (tries > NK_TRIES ? NK_COLOURS : hi)) continue;   /* many tries: more pairs will do */
        int perm[NK_COLOURS];                   /* the colours given in a random order */
        for (int i = 0; i < v.np; i++) perm[i] = i;
        pr_shuffle(&rng, perm, v.np);
        for (int p = 0; p < v.np; p++) {
            int c = perm[p];
            end[c][0] = v.cell[p][0];
            end[c][1] = v.cell[p][v.len[p] - 1];
            for (int i = 0; i < v.len[p]; i++) colour[v.cell[p][i]] = (uint8_t)c;
        }
        *pairs = v.np;
        return tries;
    }
    for (int y = 0; y < n; y++) {               /* never seen: each row a pair (the deductions solve it) */
        end[y][0] = (uint8_t)(y * n);
        end[y][1] = (uint8_t)(y * n + n - 1);
        for (int x = 0; x < n; x++) colour[y * n + x] = (uint8_t)y;
    }
    *pairs = n;
    return 2 * NK_TRIES + 1;
}

/* ---- the player's paths ------------------------------------------------------------------------------------------ */
static void own_make(nk_game *g)
{
    int nc = g->n * g->n;
    memcpy(g->own, g->dot, (size_t)nc);
    for (int c = 0; c < g->pairs; c++)
        for (int i = 0; i < g->len[c]; i++) g->own[g->path[c][i]] = (uint8_t)(c + 1);
}

static void snap_save(const nk_game *g, nk_snap *s)
{
    int k = 0;
    for (int c = 0; c < NK_COLOURS; c++) {
        s->len[c] = c < g->pairs ? g->len[c] : 0;
        memcpy(s->cells + k, g->path[c], s->len[c]);
        k += s->len[c];
    }
}

static void snap_load(nk_game *g, const nk_snap *s)
{
    int k = 0;
    for (int c = 0; c < g->pairs; c++) {
        g->len[c] = s->len[c];
        memcpy(g->path[c], s->cells + k, s->len[c]);
        k += s->len[c];
    }
}

void nk_load(nk_game *g, int n, int pairs, const uint8_t *colour, const uint8_t (*end)[2])
{
    memset(g, 0, sizeof *g);
    g->n = n;
    g->pairs = pairs;
    g->stroke = -1;
    for (int c = 0; c < pairs; c++) {
        g->end[c][0] = end[c][0];
        g->end[c][1] = end[c][1];
        g->dot[end[c][0]] = g->dot[end[c][1]] = (uint8_t)(c + 1);
        int prev = -1, cur = end[c][0];         /* the solution's path, walked from the first dot */
        while (cur >= 0 && g->slen[c] < n * n) {
            g->spath[c][g->slen[c]++] = (uint8_t)cur;
            if (cur == end[c][1]) break;
            int next = -1;
            for (int d = 0; d < 4 && next < 0; d++) {
                int u = nbr(n, cur, d);
                if (u >= 0 && u != prev && colour[u] == c) next = u;
            }
            prev = cur;
            cur = next;
        }
    }
    own_make(g);
}

void nk_new(nk_game *g, int variant, uint32_t seed)
{
    uint8_t colour[NK_CELLS], end[NK_COLOURS][2];
    int pairs, n, lo, hi;
    nk_variant(variant, &n, &lo, &hi);
    nk_make(variant, seed, colour, end, &pairs);
    nk_load(g, n, pairs, colour, (const uint8_t (*)[2])end);
}

static int index_in(const nk_game *g, int c, int cell)
{
    for (int i = 0; i < g->len[c]; i++)
        if (g->path[c][i] == cell) return i;
    return -1;
}

int nk_head(const nk_game *g, int c) { return g->len[c] ? g->path[c][g->len[c] - 1] : -1; }

int nk_linked(const nk_game *g, int c)
{
    if (g->len[c] < 2) return 0;
    int a = g->path[c][0], b = g->path[c][g->len[c] - 1];
    return a != b && g->dot[a] == c + 1 && g->dot[b] == c + 1;
}

/* the paths during a stroke: the others as they were, each cut where the stroke's path has gone */
static void rebuild(nk_game *g)
{
    int c = g->stroke;
    uint8_t keep_len = g->len[c], keep[NK_CELLS], on[NK_CELLS] = {0};
    memcpy(keep, g->path[c], keep_len);
    snap_load(g, &g->before);
    g->len[c] = keep_len;
    memcpy(g->path[c], keep, keep_len);
    for (int i = 0; i < keep_len; i++) on[keep[i]] = 1;
    for (int k = 0; k < g->pairs; k++) {
        if (k == c) continue;
        for (int i = 0; i < g->len[k]; i++)
            if (on[g->path[k][i]]) { g->len[k] = (uint8_t)i; break; }
    }
    own_make(g);
}

int nk_begin(nk_game *g, int cell)
{
    if (g->stroke >= 0) nk_end(g);
    if (cell < 0 || cell >= g->n * g->n) return -1;
    int c = -1, at = -1;
    if (g->dot[cell]) c = g->dot[cell] - 1;     /* a dot: its path starts again from it */
    for (int k = 0; k < g->pairs && c < 0; k++)
        if ((at = index_in(g, k, cell)) >= 0) c = k;
    if (c < 0) return -1;
    snap_save(g, &g->before);
    g->stroke = c;
    if (at >= 0) g->len[c] = (uint8_t)(at + 1);   /* a path's square: cut back to it */
    else {
        g->path[c][0] = (uint8_t)cell;
        g->len[c] = 1;
    }
    rebuild(g);
    return c;
}

/* one square on from the stroke's head; 0: it can't go there */
static int step(nk_game *g, int x)
{
    int c = g->stroke, h = nk_head(g, c);
    if (x < 0 || !adjacent(g->n, h, x)) return 0;
    int at = index_in(g, c, x);
    if (at >= 0) {                              /* back along itself */
        g->len[c] = (uint8_t)(at + 1);
        rebuild(g);
        return 1;
    }
    if (nk_linked(g, c)) return 0;
    if (g->dot[x] && g->dot[x] != c + 1) return 0;
    g->path[c][g->len[c]++] = (uint8_t)x;
    rebuild(g);
    return 1;
}

int nk_toward(nk_game *g, int cell)
{
    int c = g->stroke, n = g->n, k = 0;
    if (c < 0 || cell < 0 || cell >= n * n) return 0;
    for (int guard = 0; guard < 2 * n; guard++) {
        int h = nk_head(g, c);
        if (h == cell) break;
        int dx = cell % n - h % n, dy = cell / n - h / n;
        int x = abs(dx) >= abs(dy) ? h + (dx > 0 ? 1 : -1) : h + (dy > 0 ? n : -n);
        if (!step(g, x)) break;
        k++;
    }
    return k;
}

static void room(nk_game *g)
{
    if (g->nh < NK_HIST) return;
    memmove(g->hist, g->hist + NK_HIST / 2, (NK_HIST / 2) * sizeof g->hist[0]);   /* forget the oldest half */
    g->nh -= NK_HIST / 2;
}

int nk_end(nk_game *g)
{
    if (g->stroke < 0) return 0;
    g->stroke = -1;
    for (int c = 0; c < g->pairs; c++)         /* a path of its dot alone: no path */
        if (g->len[c] == 1) g->len[c] = 0;
    nk_snap now;
    snap_save(g, &now);
    if (!memcmp(now.len, g->before.len, sizeof now.len)) {
        int total = 0;
        for (int c = 0; c < NK_COLOURS; c++) total += now.len[c];
        if (!memcmp(now.cells, g->before.cells, (size_t)total)) return 0;
    }
    room(g);
    g->hist[g->nh++] = g->before;
    g->moves++;
    return 1;
}

int nk_right(const nk_game *g, int c)
{
    int L = g->len[c];
    if (L != g->slen[c]) return 0;
    int fwd = 1, back = 1;
    for (int i = 0; i < L; i++) {
        fwd &= g->path[c][i] == g->spath[c][i];
        back &= g->path[c][i] == g->spath[c][L - 1 - i];
    }
    return fwd || back;
}

int nk_free(const nk_game *g)
{
    int k = 0;
    for (int i = 0; i < g->n * g->n; i++) k += !g->own[i];
    return k;
}

int nk_solved(const nk_game *g)
{
    for (int c = 0; c < g->pairs; c++)
        if (!nk_linked(g, c)) return 0;
    return !nk_free(g);
}

int nk_hint(nk_game *g)
{
    if (g->stroke >= 0) nk_end(g);
    if (nk_solved(g)) return -1;
    int best = -1;
    for (int c = 0; c < g->pairs; c++)         /* the shortest pair not right */
        if (!nk_right(g, c) && (best < 0 || g->slen[c] < g->slen[best])) best = c;
    if (best < 0) return -1;
    snap_save(g, &g->before);
    g->stroke = best;
    g->len[best] = g->slen[best];
    memcpy(g->path[best], g->spath[best], g->slen[best]);
    rebuild(g);
    nk_end(g);
    return best;
}

int nk_undo(nk_game *g)
{
    if (g->stroke >= 0) nk_end(g);
    if (!g->nh) return 0;
    snap_load(g, &g->hist[--g->nh]);
    g->moves--;
    own_make(g);
    return 1;
}

int nk_cell(const nk_game *g, const char *s, int *used)
{
    if (s[0] < 'a' || s[0] >= 'a' + g->n || s[1] < '0' || s[1] > '9') return -1;
    int y = s[1] - '0', k = 2;
    if (s[2] >= '0' && s[2] <= '9') y = y * 10 + s[2] - '0', k = 3;
    if (y < 1 || y > g->n) return -1;
    if (used) *used = k;
    return (y - 1) * g->n + s[0] - 'a';
}

int nk_script(nk_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == '?') {
            if (nk_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '=') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            while (k-- > 0)
                if (nk_hint(g) < 0) break;
            continue;
        }
        int used, cell = nk_cell(g, s, &used);
        if (cell < 0 || nk_begin(g, cell) < 0) return 0;
        s += used;
        while (*s == '-') {
            int to = nk_cell(g, s + 1, &used);
            if (to < 0) { nk_end(g); return 0; }
            nk_toward(g, to);
            if (nk_head(g, g->stroke) != to) { nk_end(g); return 0; }
            s += 1 + used;
        }
        nk_end(g);
    }
    return 1;
}
