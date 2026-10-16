/*
 * Number Path's rules: the grid, the maker, the reasoning, the search, the player's numbers, the levels' code
 * (numberpath.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/numberpath.h"
#include "games/numberpath_levels.h"
#include "games/puzrand.h"

static const int DX[8] = {1, 1, 0, -1, -1, -1, 0, 1}, DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};

int nph_cells(const nph_puzzle *p) { return p->w * p->h; }

int nph_touch(const nph_puzzle *p, int a, int b)
{
    int dx = abs(a % p->w - b % p->w), dy = abs(a / p->w - b / p->w);
    return p->way == NPH_SIDE ? dx + dy == 1 : (dx | dy) == 1 && dx <= 1 && dy <= 1;
}

/* the cells touching c: their count */
static int neighbours(const nph_puzzle *p, int c, int *out)
{
    int n = 0, x = c % p->w, y = c / p->w;
    for (int d = 0; d < 8; d++) {
        if (p->way == NPH_SIDE && (d & 1)) continue;
        int nx = x + DX[d], ny = y + DY[d];
        if (nx >= 0 && ny >= 0 && nx < p->w && ny < p->h) out[n++] = ny * p->w + nx;
    }
    return n;
}

void nph_variant(int variant, int *w, int *h, int *way)
{
    static const int S[NPH_SIZES] = {6, 7, 9};
    int v = variant < 0 || variant >= NPH_VARIANTS ? 0 : variant;
    *way = v / NPH_SIZES;
    *w = *h = S[v % NPH_SIZES];
}

/* ---- the path: a snake through the rows, then backbites ------------------------------------------------------ */
void nph_path(int w, int h, int way, uint32_t seed, uint8_t *sol)
{
    nph_puzzle p = {.w = w, .h = h, .way = way};
    int n = w * h, path[NPH_CELLS], at[NPH_CELLS];
    uint32_t s = pr_seed(seed);
    for (int y = 0, k = 0; y < h; y++)
        for (int i = 0; i < w; i++, k++) path[k] = y * w + (y & 1 ? w - 1 - i : i);
    for (int k = 0; k < n; k++) at[path[k]] = k;
    for (long step = 0, steps = 40L * n * n + 2000; step < steps; step++) {
        int head = pr_below(&s, 2), e = path[head ? n - 1 : 0], nb[8], k = neighbours(&p, e, nb);
        int u = nb[pr_below(&s, k)], i = at[u];
        if (head) {                             /* the tail steps to u: the part after u turned round */
            if (i == n - 2) continue;
            for (int a = i + 1, b = n - 1; a < b; a++, b--) {
                int t = path[a];
                path[a] = path[b], path[b] = t;
            }
            for (int a = i + 1; a < n; a++) at[path[a]] = a;
        } else {                                /* the head steps to u: the part before u turned round */
            if (i == 1) continue;
            for (int a = 0, b = i - 1; a < b; a++, b--) {
                int t = path[a];
                path[a] = path[b], path[b] = t;
            }
            for (int a = 0; a < i; a++) at[path[a]] = a;
        }
    }
    for (int k = 0; k < n; k++) sol[path[k]] = (uint8_t)(k + 1);
}

/* ---- the reasoning ------------------------------------------------------------------------------------------- */
typedef struct { uint64_t w[2]; } cells;
static inline void c_set(cells *a, int i) { a->w[i >> 6] |= 1ull << (i & 63); }
static inline int c_has(const cells *a, int i) { return (int)(a->w[i >> 6] >> (i & 63) & 1); }
static inline int c_count(const cells *a) { return __builtin_popcountll(a->w[0]) + __builtin_popcountll(a->w[1]); }
static inline int c_first(const cells *a) { return a->w[0] ? __builtin_ctzll(a->w[0]) : 64 + __builtin_ctzll(a->w[1]); }

/* the steps from cell `from` to each empty cell through empty cells (far: none) */
static void reach(const nph_puzzle *p, const uint8_t *val, int from, int *d)
{
    int n = nph_cells(p), q[NPH_CELLS], h = 0, t = 0, nb[8];
    for (int c = 0; c < n; c++) d[c] = 1 << 20;
    d[from] = 0;
    q[t++] = from;
    while (h < t) {
        int c = q[h++], k = neighbours(p, c, nb);
        for (int i = 0; i < k; i++)
            if (!val[nb[i]] && d[nb[i]] > d[c] + 1) d[nb[i]] = d[c] + 1, q[t++] = nb[i];
    }
}

/* the cells each missing number may take (cand), each number's cell (pos, -1 missing): -1 a contradiction, -2 every
 * cell filled the right way, else 0 */
static int candidates(const nph_puzzle *p, const uint8_t *val, cells *cand, int *pos)
{
    int n = nph_cells(p);
    for (int v = 0; v <= n + 1; v++) pos[v] = -1;
    int filled = 0;
    for (int c = 0; c < n; c++) {
        int v = val[c];
        if (!v) continue;
        if (v > n || pos[v] >= 0) return -1;
        pos[v] = c;
        filled++;
    }
    for (int v = 1; v < n; v++)
        if (pos[v] >= 0 && pos[v + 1] >= 0 && !nph_touch(p, pos[v], pos[v + 1])) return -1;
    if (filled == n) return -2;
    int da[NPH_CELLS], db[NPH_CELLS];
    for (int v = 1; v <= n; v++) cand[v] = (cells){{0, 0}};
    for (int a = 1; a < n;) {
        if (pos[a] < 0) return -1;              /* 1 is always known: never */
        int b = a + 1;
        while (b <= n && pos[b] < 0) b++;
        if (b > n) {                            /* no number known above: only the steps from a */
            reach(p, val, pos[a], da);
            for (int v = a + 1; v <= n; v++)
                for (int c = 0; c < n; c++)
                    if (!val[c] && da[c] <= v - a) c_set(&cand[v], c);
            break;
        }
        if (b > a + 1) {
            reach(p, val, pos[a], da);
            reach(p, val, pos[b], db);
            int pa = (pos[a] % p->w + pos[a] / p->w) & 1;
            for (int v = a + 1; v < b; v++)
                for (int c = 0; c < n; c++) {
                    if (val[c] || da[c] > v - a || db[c] > b - v) continue;
                    if (p->way == NPH_SIDE && ((c % p->w + c / p->w) & 1) != ((pa + v - a) & 1)) continue;
                    c_set(&cand[v], c);
                }
        }
        a = b;
    }
    return 0;
}

/* one round: the findings (cell, number) of the easiest way that finds any (*medium 1: a cell with one number left);
 * their count, 0 none, -1 a contradiction, -2 solved */
static int round_of(const nph_puzzle *p, const uint8_t *val, int *fc, int *fv, int *medium)
{
    int n = nph_cells(p), pos[NPH_CELLS + 2];
    cells cand[NPH_CELLS + 1];
    int r = candidates(p, val, cand, pos);
    if (r < 0) return r;
    int found = 0;
    uint8_t taken[NPH_CELLS];
    memset(taken, 0, sizeof taken);
    for (int v = 1; v <= n; v++) {              /* a number with one cell left */
        if (pos[v] >= 0) continue;
        int k = c_count(&cand[v]);
        if (!k) return -1;
        if (k == 1) {
            int c = c_first(&cand[v]);
            if (taken[c]) return -1;
            taken[c] = 1;
            fc[found] = c, fv[found++] = v;
        }
    }
    *medium = 0;
    if (found) return found;
    int claim[NPH_CELLS + 1];
    memset(claim, 0, sizeof claim);
    for (int c = 0; c < n; c++) {               /* a cell with one number left */
        if (val[c]) continue;
        int k = 0, last = 0;
        for (int v = 1; v <= n; v++)
            if (pos[v] < 0 && c_has(&cand[v], c)) k++, last = v;
        if (!k) return -1;
        if (k == 1) {
            if (claim[last]) return -1;
            claim[last] = 1;
            fc[found] = c, fv[found++] = last;
        }
    }
    *medium = found > 0;
    return found;
}

int nph_reason(const nph_puzzle *p, uint8_t *val, int *steps, int *mediums)
{
    int fc[NPH_CELLS], fv[NPH_CELLS], s = 0, m = 0, r, medium;
    while ((r = round_of(p, val, fc, fv, &medium)) > 0) {
        for (int i = 0; i < r; i++) val[fc[i]] = (uint8_t)fv[i];
        s++;
        m += medium;
    }
    if (steps) *steps = s;
    if (mediums) *mediums = m;
    return r == -2 ? 1 : r;
}

int nph_next(const nph_puzzle *p, const uint8_t *val, int *v)
{
    int fc[NPH_CELLS], fv[NPH_CELLS], medium, r = round_of(p, val, fc, fv, &medium);
    if (r <= 0) return -1;
    *v = fv[0];
    return fc[0];
}

/* ---- the search: the reasoning, then a number tried in each of its cells left --------------------------------- *
 * The reasoning's findings hold in every solution, so trying each cell left for the number with the fewest of them,
 * reasoning again each time, finds every solution. */
typedef struct {
    const nph_puzzle *p;
    int count, limit;
    long nodes, budget;
    uint8_t *sol;
} search;

static void branch(search *s, uint8_t *val)
{
    if (s->count >= s->limit || (s->budget && s->nodes >= s->budget)) return;
    s->nodes++;
    int r = nph_reason(s->p, val, NULL, NULL), n = nph_cells(s->p);
    if (r < 0) return;
    if (r == 1) {
        if (!s->count && s->sol) memcpy(s->sol, val, (size_t)n);
        s->count++;
        return;
    }
    cells cand[NPH_CELLS + 1];
    int pos[NPH_CELLS + 2], best = 0, fewest = 1 << 20;
    if (candidates(s->p, val, cand, pos) < 0) return;
    for (int v = 1; v <= n; v++)
        if (pos[v] < 0 && c_count(&cand[v]) < fewest) fewest = c_count(&cand[v]), best = v;
    for (int c = 0; c < n && s->count < s->limit; c++) {
        if (!best || !c_has(&cand[best], c)) continue;
        uint8_t t[NPH_CELLS];
        memcpy(t, val, (size_t)n);
        t[c] = (uint8_t)best;
        branch(s, t);
    }
}

int nph_count(const nph_puzzle *p, int limit, uint8_t *sol, long budget, long *nodes)
{
    search s = {.p = p, .limit = limit, .budget = budget, .sol = sol};
    uint8_t val[NPH_CELLS];
    memcpy(val, p->given, sizeof val);
    branch(&s, val);
    if (nodes) *nodes = s.nodes;
    return s.count;
}

/* ---- the maker ----------------------------------------------------------------------------------------------- */
static int reasoned(const nph_puzzle *p)
{
    uint8_t val[NPH_CELLS];
    memcpy(val, p->given, sizeof val);
    return nph_reason(p, val, NULL, NULL) == 1;
}

void nph_make(int w, int h, int way, float keep, uint32_t seed, nph_puzzle *p, uint8_t *sol)
{
    uint32_t s = pr_seed(seed ^ 0x5bd1e995u);
    memset(p, 0, sizeof *p);
    p->w = w, p->h = h, p->way = way;
    int n = w * h, order[NPH_CELLS], k = 0;
    nph_path(w, h, way, seed, sol);
    memcpy(p->given, sol, (size_t)n);
    for (int c = 0; c < n; c++)
        if (sol[c] != 1 && sol[c] != n) order[k++] = c;
    pr_shuffle(&s, order, k);
    for (int i = 0; i < k; i++) {
        int c = order[i];
        p->given[c] = 0;
        if (!reasoned(p)) p->given[c] = sol[c];
    }
    int want = (int)(keep * n + 0.5f), have = 0;
    for (int c = 0; c < n; c++) have += p->given[c] != 0;
    pr_shuffle(&s, order, k);
    for (int i = 0; i < k && have < want; i++)
        if (!p->given[order[i]]) p->given[order[i]] = sol[order[i]], have++;
}

/* ---- the levels' code ---------------------------------------------------------------------------------------- */
int nph_encode(const nph_puzzle *p, const uint8_t *sol, char *out, int size)
{
    int n = nph_cells(p), at[NPH_CELLS + 1], o = 0;
    if (p->w < 2 || p->h < 2 || p->w > NPH_MAXW || p->h > NPH_MAXW || size < 10 + n + n / 4 + 2) return 0;
    for (int c = 0; c < n; c++) at[sol[c]] = c;
    out[o++] = (char)('0' + p->w);
    out[o++] = (char)('0' + p->h);
    out[o++] = p->way == NPH_SIDE ? 's' : 'a';
    out[o++] = ':';
    out[o++] = (char)('0' + at[1] / 10);
    out[o++] = (char)('0' + at[1] % 10);
    out[o++] = ':';
    for (int v = 1; v < n; v++) {
        int dx = at[v + 1] % p->w - at[v] % p->w, dy = at[v + 1] / p->w - at[v] / p->w, d = 0;
        while (d < 8 && (DX[d] != dx || DY[d] != dy)) d++;
        if (d == 8) return 0;
        out[o++] = (char)('0' + d);
    }
    out[o++] = ':';
    for (int i = 0; i < (n + 3) / 4; i++) {
        int x = 0;
        for (int b = 0; b < 4; b++)
            if (4 * i + b < n && p->given[at[4 * i + b + 1]]) x |= 1 << b;
        out[o++] = "0123456789abcdef"[x];
    }
    out[o] = 0;
    return o;
}

int nph_decode(const char *s, nph_puzzle *p, uint8_t *sol)
{
    memset(p, 0, sizeof *p);
    if (!s || s[0] < '2' || s[0] > '0' + NPH_MAXW || s[1] < '2' || s[1] > '0' + NPH_MAXW) return 0;
    p->w = s[0] - '0';
    p->h = s[1] - '0';
    if (s[2] != 'a' && s[2] != 's') return 0;
    p->way = s[2] == 's' ? NPH_SIDE : NPH_ALL;
    if (s[3] != ':' || s[4] < '0' || s[4] > '9' || s[5] < '0' || s[5] > '9' || s[6] != ':') return 0;
    int n = nph_cells(p), c = (s[4] - '0') * 10 + s[5] - '0', at[NPH_CELLS + 1];
    if (c >= n) return 0;
    memset(sol, 0, (size_t)n);
    s += 7;
    for (int v = 1; v <= n; v++) {
        if (sol[c]) return 0;
        sol[c] = (uint8_t)v;
        at[v] = c;
        if (v == n) break;
        if (*s < '0' || *s > '7') return 0;
        int d = *s++ - '0', x = c % p->w + DX[d], y = c / p->w + DY[d];
        if (x < 0 || y < 0 || x >= p->w || y >= p->h) return 0;
        c = y * p->w + x;
        if (!nph_touch(p, at[v], c)) return 0;
    }
    if (*s++ != ':') return 0;
    for (int i = 0; i < (n + 3) / 4; i++, s++) {
        int x = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10 : -1;
        if (x < 0) return 0;
        for (int b = 0; b < 4; b++)
            if (x >> b & 1 && 4 * i + b < n) p->given[at[4 * i + b + 1]] = (uint8_t)(4 * i + b + 1);
    }
    return !*s && p->given[at[1]] == 1 && p->given[at[n]] == n;
}

/* ---- the game ------------------------------------------------------------------------------------------------- */
int nph_load(nph_game *g, const nph_puzzle *p, const uint8_t *sol)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    int n = nph_cells(p);
    memcpy(g->sol, sol, (size_t)n);
    for (int c = 0; c < n; c++) {
        if (p->given[c] && p->given[c] != sol[c]) return 0;
        g->val[c] = p->given[c];
    }
    return 1;
}

void nph_new(nph_game *g, int variant, uint32_t seed)
{
    static const float KEEP[NPH_SIZES] = {0.36f, 0.32f, 0.28f};
    int w, h, way;
    nph_puzzle p;
    uint8_t sol[NPH_CELLS];
    nph_variant(variant, &w, &h, &way);
    nph_make(w, h, way, KEEP[variant % NPH_SIZES], seed, &p, sol);
    nph_load(g, &p, sol);
}

int nph_level(nph_game *g, int level)
{
    nph_puzzle p;
    uint8_t sol[NPH_CELLS];
    if (level < 0 || level >= NPH_LEVELS || !nph_levels[level] || !nph_decode(nph_levels[level], &p, sol)) return 0;
    return nph_load(g, &p, sol);
}

int nph_given(const nph_game *g, int c) { return g->p.given[c] != 0; }

int nph_where(const nph_game *g, int v)
{
    for (int c = 0; c < nph_cells(&g->p); c++)
        if (g->val[c] == v) return c;
    return -1;
}

int nph_head(const nph_game *g, int c, int *dir)
{
    int n = nph_cells(&g->p);
    if (c < 0 || c >= n || !g->val[c]) return -1;
    for (int d = 1; d >= -1; d -= 2) {
        int at = c, v = g->val[c];
        for (;;) {
            int nv = v + d;
            if (nv < 1 || nv > n) break;
            int nc = nph_where(g, nv);
            if (nc < 0) {
                *dir = d;
                return at;
            }
            if (!nph_touch(&g->p, at, nc)) break;
            at = nc, v = nv;
        }
    }
    return -1;
}

int nph_wrongs(const nph_game *g)
{
    int k = 0;
    for (int c = 0; c < nph_cells(&g->p); c++) k += g->val[c] && g->val[c] != g->sol[c];
    return k;
}

int nph_clash(const nph_game *g, int c)
{
    int v = g->val[c];
    if (!v) return 0;
    for (int d = -1; d <= 1; d += 2) {
        int o = v + d < 1 ? -1 : nph_where(g, v + d);
        if (o >= 0 && !nph_touch(&g->p, c, o)) return 1;
    }
    return 0;
}

int nph_solved(const nph_game *g)
{
    int n = nph_cells(&g->p), at[NPH_CELLS + 1];
    for (int v = 1; v <= n; v++) at[v] = -1;
    for (int c = 0; c < n; c++) {
        if (!g->val[c] || at[g->val[c]] >= 0) return 0;
        at[g->val[c]] = c;
    }
    for (int v = 1; v < n; v++)
        if (!nph_touch(&g->p, at[v], at[v + 1])) return 0;
    return 1;
}

int nph_place(nph_game *g, int from, const int *to, int n)
{
    int dir, head = nph_head(g, from, &dir), cells = nph_cells(&g->p);
    if (head < 0 || n < 1) return 0;
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++) {
        int c = to[i];
        if (head < 0 || c < 0 || c >= cells || g->val[c] || !nph_touch(&g->p, head, c)) break;
        bench_set(&g->hist, g->val, c, g->val[head] + dir);
        head = nph_head(g, c, &dir);
    }
    return bench_done(&g->hist, nph_wrongs(g));
}

int nph_erase(nph_game *g, const int *cells, int n)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (cells[i] >= 0 && cells[i] < nph_cells(&g->p) && !nph_given(g, cells[i]))
            bench_set(&g->hist, g->val, cells[i], 0);
    return bench_done(&g->hist, nph_wrongs(g));
}

int nph_undo(nph_game *g)
{
    int c = bench_undo_cells(&g->hist, g->val);
    if (c >= 0) bench_undone(&g->hist, nph_wrongs(g));
    return c;
}

/* the next number of the solution to put (no reasoning found one): the first missing number next to a known one */
static int fallback(const nph_game *g, int *v)
{
    int n = nph_cells(&g->p);
    for (int x = 1; x <= n; x++) {
        if (nph_where(g, x) >= 0) continue;
        if ((x > 1 && nph_where(g, x - 1) >= 0) || (x < n && nph_where(g, x + 1) >= 0)) {
            for (int c = 0; c < n; c++)
                if (g->sol[c] == x) {
                    *v = x;
                    return c;
                }
        }
    }
    return -1;
}

static int put_one(nph_game *g, int c, int v)
{
    bench_begin(&g->hist, 1);
    bench_set(&g->hist, g->val, c, v);
    return bench_done(&g->hist, nph_wrongs(g));
}

int nph_hint(nph_game *g, int *put)
{
    int n = nph_cells(&g->p);
    *put = 0;
    if (nph_solved(g)) return -1;
    for (int c = 0; c < n; c++)
        if (g->val[c] && g->val[c] != g->sol[c]) {   /* a wrong number: out */
            nph_erase(g, &c, 1);
            return bench_hinted(&g->hist, c);
        }
    uint8_t val[NPH_CELLS];
    for (int c = 0; c < n; c++) val[c] = (uint8_t)g->val[c];
    int v = 0, c = nph_next(&g->p, val, &v);
    if (c < 0 || g->sol[c] != v) c = fallback(g, &v);
    if (c < 0) return -1;
    put_one(g, c, v);
    *put = 1;
    return bench_hinted(&g->hist, c);
}

int nph_script(nph_game *g, const char *s)
{
    int w = g->p.w, h = g->p.h;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == 'h') {
            int put;
            if (nph_hint(g, &put) < 0) return 0;
            s++;
        } else if (*s == '+') {
            int k = atoi(++s);
            while (*s >= '0' && *s <= '9') s++;
            for (int i = 0; i < k; i++) {       /* the solution's next: the lowest missing beside one known */
                int v = 0, c = fallback(g, &v);
                if (c < 0) break;
                put_one(g, c, v);
            }
        } else if (*s == 'x') {
            int len, c = bench_cell(s + 1, w, h, &len);
            if (c < 0 || nph_given(g, c) || !g->val[c] || !nph_erase(g, &c, 1)) return 0;
            s += 1 + len;
        } else {
            int len, from = bench_cell(s, w, h, &len), to[NPH_CELLS], k = 0;
            if (from < 0) return 0;
            s += len;
            while (*s == '-') {
                int c = bench_cell(s + 1, w, h, &len);
                if (c < 0) return 0;
                to[k++] = c;
                s += 1 + len;
            }
            if (k && !nph_place(g, from, to, k)) return 0;
        }
    }
    return 1;
}
