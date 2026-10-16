/*
 * Flood's rules: the board, a move, the solver (the par and the hint), the levels' code (flood.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/flood.h"
#include "games/flood_levels.h"
#include "games/puzrand.h"

/* ---- the board ----------------------------------------------------------------------------------------------- */
void fld_variant(int variant, int *n, int *k)
{
    static const int N[FLD_VARIANTS] = {6, 10, 14}, K[FLD_VARIANTS] = {4, 5, 6};
    int v = variant < 0 || variant >= FLD_VARIANTS ? 0 : variant;
    *n = N[v];
    *k = K[v];
}

int fld_limit(int par)
{
    int more = (par + 4) / 5;
    return par + (more < 2 ? 2 : more);
}

void fld_deal(fld_board *b, int n, int k, uint32_t seed)
{
    uint32_t s = pr_seed(seed);
    n = n < 2 ? 2 : n > FLD_MAX ? FLD_MAX : n;
    k = k < 2 ? 2 : k > FLD_COLOURS ? FLD_COLOURS : k;
    memset(b, 0, sizeof *b);
    b->n = n;
    b->k = k;
    for (;;) {
        unsigned seen = 0;
        for (int i = 0; i < n * n; i++) seen |= 1u << (b->c[i] = (uint8_t)pr_below(&s, k));
        if (seen == (1u << k) - 1) return;
    }
}

int fld_region(const fld_board *b, uint8_t *in)
{
    int n = b->n, st[FLD_CELLS], sp = 0, count = 0, c0 = b->c[0];
    memset(in, 0, (size_t)n * n);
    in[0] = 1;
    st[sp++] = 0;
    while (sp) {
        int i = st[--sp], x = i % n, y = i / n;
        count++;
        int nb[4] = {x > 0 ? i - 1 : -1, x < n - 1 ? i + 1 : -1, y > 0 ? i - n : -1, y < n - 1 ? i + n : -1};
        for (int d = 0; d < 4; d++)
            if (nb[d] >= 0 && !in[nb[d]] && b->c[nb[d]] == c0) in[nb[d]] = 1, st[sp++] = nb[d];
    }
    return count;
}

int fld_fill(fld_board *b, uint8_t *in, int c)
{
    int n = b->n, before = 0;
    if (c < 0 || c >= b->k || c == b->c[0]) return 0;
    for (int i = 0; i < n * n; i++)
        if (in[i]) b->c[i] = (uint8_t)c, before++;
    return fld_region(b, in) - before;
}

int fld_flooded(const fld_board *b)
{
    for (int i = 1; i < b->n * b->n; i++)
        if (b->c[i] != b->c[0]) return 0;
    return 1;
}

/* ---- the patches: the solver's graph ------------------------------------------------------------------------- */
#define NW 4                                    /* 256 bits: patches */
typedef struct { uint64_t w[NW]; } bits;

static inline void b_or(bits *a, const bits *b) { for (int i = 0; i < NW; i++) a->w[i] |= b->w[i]; }
static inline int b_any(const bits *a) { return (a->w[0] | a->w[1] | a->w[2] | a->w[3]) != 0; }
static inline void b_set(bits *a, int i) { a->w[i >> 6] |= 1ull << (i & 63); }
static inline bits b_and(const bits *a, const bits *b)
{
    bits r;
    for (int i = 0; i < NW; i++) r.w[i] = a->w[i] & b->w[i];
    return r;
}
static inline bits b_andnot(const bits *a, const bits *b)
{
    bits r;
    for (int i = 0; i < NW; i++) r.w[i] = a->w[i] & ~b->w[i];
    return r;
}
static inline int b_eq(const bits *a, const bits *b) { return !memcmp(a, b, sizeof *a); }

typedef struct {
    int np, k, cells;
    uint8_t col[FLD_CELLS];
    int size[FLD_CELLS];
    bits adj[FLD_CELLS], ofcol[FLD_COLOURS], all;
    int start;                                  /* the corner's patch */
} graph;

static void graph_make(graph *g, const fld_board *b)
{
    int n = b->n, lab[FLD_CELLS], st[FLD_CELLS];
    memset(g, 0, sizeof *g);
    g->k = b->k;
    g->cells = n * n;
    for (int i = 0; i < n * n; i++) lab[i] = -1;
    for (int i = 0; i < n * n; i++) {
        if (lab[i] >= 0) continue;
        int p = g->np++, sp = 0;
        g->col[p] = b->c[i];
        lab[i] = p;
        st[sp++] = i;
        while (sp) {
            int j = st[--sp], x = j % n, y = j / n;
            g->size[p]++;
            int nb[4] = {x > 0 ? j - 1 : -1, x < n - 1 ? j + 1 : -1, y > 0 ? j - n : -1, y < n - 1 ? j + n : -1};
            for (int d = 0; d < 4; d++)
                if (nb[d] >= 0 && lab[nb[d]] < 0 && b->c[nb[d]] == b->c[i]) lab[nb[d]] = p, st[sp++] = nb[d];
        }
    }
    for (int i = 0; i < n * n; i++) {
        int x = i % n, y = i / n;
        if (x < n - 1 && lab[i + 1] != lab[i]) b_set(&g->adj[lab[i]], lab[i + 1]), b_set(&g->adj[lab[i + 1]], lab[i]);
        if (y < n - 1 && lab[i + n] != lab[i]) b_set(&g->adj[lab[i]], lab[i + n]), b_set(&g->adj[lab[i + n]], lab[i]);
    }
    for (int p = 0; p < g->np; p++) {
        b_set(&g->ofcol[g->col[p]], p);
        b_set(&g->all, p);
    }
    g->start = lab[0];
}

/* a state of the search: the region (its patches), the patches touching it, the moves, the cells flooded */
typedef struct {
    bits F, fr;
    int len, cells, h;
    uint8_t path[FLD_MOVES];
} state;

static void adj_of(const graph *g, const bits *set, bits *out)
{
    for (int i = 0; i < NW; i++)
        for (uint64_t m = set->w[i]; m; m &= m - 1) b_or(out, &g->adj[i * 64 + __builtin_ctzll(m)]);
}

static int cells_of(const graph *g, const bits *set)
{
    int s = 0;
    for (int i = 0; i < NW; i++)
        for (uint64_t m = set->w[i]; m; m &= m - 1) s += g->size[i * 64 + __builtin_ctzll(m)];
    return s;
}

/* colour c chosen: 0 when it floods nothing */
static int st_move(const graph *g, state *s, int c)
{
    bits a = b_and(&s->fr, &g->ofcol[c]);
    if (!b_any(&a)) return 0;
    b_or(&s->F, &a);
    adj_of(g, &a, &s->fr);
    s->fr = b_andnot(&s->fr, &s->F);
    s->cells += cells_of(g, &a);
    if (s->len < FLD_MOVES) s->path[s->len] = (uint8_t)c;
    s->len++;
    return 1;
}

/* the moves that flood every patch left of their colour: always as good as any, played at once */
static void st_forced(const graph *g, state *s)
{
    for (int again = 1; again;) {
        again = 0;
        for (int c = 0; c < g->k; c++) {
            bits left = b_andnot(&g->ofcol[c], &s->F);
            if (!b_any(&left)) continue;
            bits away = b_andnot(&left, &s->fr);
            if (!b_any(&away) && st_move(g, s, c)) again = 1;
        }
    }
}

/* a bound on the moves left: the colours left, and the farthest patch's distance */
static int st_bound(const graph *g, const state *s)
{
    int colours = 0;
    for (int c = 0; c < g->k; c++) {
        bits left = b_andnot(&g->ofcol[c], &s->F);
        colours += b_any(&left);
    }
    if (!colours) return 0;
    bits seen = s->F, layer = s->fr;
    b_or(&seen, &layer);
    int d = 1;
    for (;;) {
        bits rest = b_andnot(&g->all, &seen);
        if (!b_any(&rest)) break;
        bits next = {{0}};
        adj_of(g, &layer, &next);
        next = b_andnot(&next, &seen);
        if (!b_any(&next)) break;
        b_or(&seen, &next);
        layer = next;
        d++;
    }
    return d > colours ? d : colours;
}

static void st_start(const graph *g, state *s)
{
    memset(s, 0, sizeof *s);
    b_set(&s->F, g->start);
    s->fr = g->adj[g->start];
    s->cells = g->size[g->start];
    st_forced(g, s);
    s->h = st_bound(g, s);
}

static int st_done(const graph *g, const state *s) { return b_eq(&s->F, &g->all); }

static uint64_t st_key(const state *s)
{
    uint64_t h = 0x9e3779b97f4a7c15ull;
    for (int i = 0; i < NW; i++) h = (h ^ s->F.w[i]) * 0xff51afd7ed558ccdull, h ^= h >> 29;
    return h;
}

/* ranked: the fewest moves to come first, then the most cells flooded */
static int st_rank(const void *a, const void *b)
{
    const state *x = a, *y = b;
    int fx = x->len + x->h, fy = y->len + y->h;
    if (fx != fy) return fx - fy;
    return y->cells - x->cells;
}

/* the beam search: the best way found (out, its length); -1: none */
static int beam(const graph *g, int width, uint8_t *out, int max)
{
    state *cur = malloc(sizeof(state) * (size_t)width), *next = malloc(sizeof(state) * (size_t)width * FLD_COLOURS);
    uint64_t *keys = malloc(sizeof(uint64_t) * (size_t)width * FLD_COLOURS);
    if (!cur || !next || !keys) {
        free(cur), free(next), free(keys);
        return -1;
    }
    int nc = 1, best = FLD_MOVES + 1;
    st_start(g, &cur[0]);
    if (st_done(g, &cur[0])) {
        best = cur[0].len;
        memcpy(out, cur[0].path, (size_t)(best < max ? best : max));
    }
    while (nc > 0) {
        int nn = 0;
        for (int i = 0; i < nc; i++) {
            if (cur[i].len + cur[i].h >= best) continue;
            for (int c = 0; c < g->k; c++) {
                state t = cur[i];
                if (!st_move(g, &t, c)) continue;
                st_forced(g, &t);
                if (t.len >= FLD_MOVES) continue;
                if (st_done(g, &t)) {
                    if (t.len < best) {
                        best = t.len;
                        memcpy(out, t.path, (size_t)(best < max ? best : max));
                    }
                    continue;
                }
                t.h = st_bound(g, &t);
                if (t.len + t.h >= best) continue;
                uint64_t key = st_key(&t);
                int dup = 0;
                for (int j = 0; j < nn && !dup; j++)
                    if (keys[j] == key && b_eq(&next[j].F, &t.F)) {
                        dup = 1;
                        if (t.len < next[j].len) next[j] = t;
                    }
                if (dup) continue;
                keys[nn] = key;
                next[nn++] = t;
            }
        }
        qsort(next, (size_t)nn, sizeof(state), st_rank);
        nc = nn < width ? nn : width;
        memcpy(cur, next, sizeof(state) * (size_t)nc);
    }
    free(cur), free(next), free(keys);
    return best <= FLD_MOVES ? best : -1;
}

/* ---- the exact search: iterative deepening on the bound, a table of the regions met -------------------------- */
#define TT_BITS 16
typedef struct { uint64_t key; uint8_t g, iter; } tt_entry;

typedef struct {
    const graph *g;
    tt_entry *tt;
    long nodes, budget;
    int bound, iter, found;
    uint8_t path[FLD_MOVES];
} exact;

static int dfs(exact *e, const state *s)
{
    if (++e->nodes > e->budget) return 0;
    if (st_done(e->g, s)) {
        e->found = s->len;
        memcpy(e->path, s->path, (size_t)s->len);
        return 1;
    }
    if (s->len + s->h > e->bound) return 0;
    uint64_t key = st_key(s);
    tt_entry *t = &e->tt[key & ((1u << TT_BITS) - 1)];
    if (t->key == key && t->iter == e->iter && t->g <= s->len) return 0;
    t->key = key;
    t->iter = (uint8_t)e->iter;
    t->g = (uint8_t)s->len;
    state kids[FLD_COLOURS];
    int nk = 0;
    for (int c = 0; c < e->g->k; c++) {
        state k = *s;
        if (!st_move(e->g, &k, c)) continue;
        st_forced(e->g, &k);
        k.h = st_bound(e->g, &k);
        if (k.len + k.h > e->bound) continue;
        kids[nk++] = k;
    }
    qsort(kids, (size_t)nk, sizeof(state), st_rank);
    for (int i = 0; i < nk; i++)
        if (dfs(e, &kids[i])) return 1;
    return 0;
}

/* a way shorter than `best` (into out): its length, or `best` when none is shorter; *proved: the search finished */
static int exact_search(const graph *g, long budget, int best, uint8_t *out, int max, int *proved)
{
    exact e;
    memset(&e, 0, sizeof e);
    e.g = g;
    e.budget = budget;
    e.tt = calloc((size_t)1 << TT_BITS, sizeof(tt_entry));
    *proved = 0;
    if (!e.tt) return best;
    state s;
    st_start(g, &s);
    for (e.bound = s.len + s.h; e.bound < best; e.bound++) {
        e.iter++;
        if (dfs(&e, &s)) {
            best = e.found;
            memcpy(out, e.path, (size_t)(best < max ? best : max));
            *proved = 1;
            break;
        }
        if (e.nodes > e.budget) break;
    }
    if (e.bound >= best && e.nodes <= e.budget) *proved = 1;
    free(e.tt);
    return best;
}

int fld_solve(const fld_board *b, uint8_t *out, int max, int width, long exact_nodes, int *proved)
{
    graph g;
    graph_make(&g, b);
    uint8_t way[FLD_MOVES];
    int len = beam(&g, width < 1 ? 1 : width, way, FLD_MOVES), ok = 0;
    if (len < 0) len = FLD_MOVES;
    if (exact_nodes > 0) len = exact_search(&g, exact_nodes, len, way, FLD_MOVES, &ok);
    if (proved) *proved = ok;
    memcpy(out, way, (size_t)(len < max ? len : max));
    return len;
}

int fld_greedy(const fld_board *start)
{
    fld_board b = *start;
    uint8_t in[FLD_CELLS];
    fld_region(&b, in);
    int moves = 0;
    while (!fld_flooded(&b) && moves < FLD_MOVES) {
        int best = -1, most = 0;
        for (int c = 0; c < b.k; c++) {
            fld_board t = b;
            uint8_t tin[FLD_CELLS];
            memcpy(tin, in, sizeof tin);
            int got = fld_fill(&t, tin, c);
            if (got > most) most = got, best = c;
        }
        if (best < 0) break;
        fld_fill(&b, in, best);
        moves++;
    }
    return moves;
}

/* ---- the levels' code ------------------------------------------------------------------------------------------ */
int fld_encode(const fld_board *b, const uint8_t *way, int len, char *out, int size)
{
    int n = b->n, p = 0;
    if (n < 2 || n > FLD_MAX || len < 0 || 7 + n * n + len + 1 > size) return 0;
    if (n >= 10) out[p++] = (char)('0' + n / 10);
    out[p++] = (char)('0' + n % 10);
    out[p++] = ':';
    out[p++] = (char)('0' + b->k);
    out[p++] = ':';
    for (int i = 0; i < n * n; i++) out[p++] = (char)('0' + b->c[i]);
    out[p++] = ':';
    for (int i = 0; i < len; i++) out[p++] = (char)('0' + way[i]);
    out[p] = 0;
    return p;
}

int fld_decode(const char *s, fld_board *b, uint8_t *way, int *len)
{
    memset(b, 0, sizeof *b);
    int n = 0;
    while (*s >= '0' && *s <= '9') n = n * 10 + *s++ - '0';
    if (*s++ != ':' || n < 2 || n > FLD_MAX || *s < '2' || *s > '0' + FLD_COLOURS) return 0;
    b->n = n;
    b->k = *s++ - '0';
    if (*s++ != ':') return 0;
    for (int i = 0; i < n * n; i++, s++) {
        if (*s < '0' || *s >= '0' + b->k) return 0;
        b->c[i] = (uint8_t)(*s - '0');
    }
    if (*s++ != ':') return 0;
    int l = 0;
    for (; *s; s++) {
        if (*s < '0' || *s >= '0' + b->k || l >= FLD_MOVES) return 0;
        way[l++] = (uint8_t)(*s - '0');
    }
    *len = l;
    return 1;
}

/* ---- the game -------------------------------------------------------------------------------------------------- */
static void reset(fld_game *g)
{
    g->b = g->start;
    fld_region(&g->b, g->in);
    g->nmoves = 0;
    g->hints = 0;
}

/* the beam's width and the exact search's nodes for a board of n x n (a phone makes a random one well under a
 * second) */
static void effort(int n, int *width, long *nodes)
{
    *width = 300;
    *nodes = n <= 10 ? 100000 : 0;
}

void fld_new(fld_game *g, int variant, uint32_t seed)
{
    int n, k, width;
    long nodes;
    memset(g, 0, sizeof *g);
    fld_variant(variant, &n, &k);
    fld_deal(&g->start, n, k, seed);
    effort(n, &width, &nodes);
    g->par = fld_solve(&g->start, g->way, FLD_MOVES, width, nodes, NULL);
    g->limit = fld_limit(g->par);
    reset(g);
}

int fld_load(fld_game *g, const fld_board *b, const uint8_t *way, int len)
{
    memset(g, 0, sizeof *g);
    g->start = *b;
    fld_board t = *b;
    uint8_t in[FLD_CELLS];
    fld_region(&t, in);
    for (int i = 0; i < len; i++)
        if (!fld_fill(&t, in, way[i])) return 0;
    if (!fld_flooded(&t) || len > FLD_MOVES) return 0;
    memcpy(g->way, way, (size_t)len);
    g->par = len;
    g->limit = fld_limit(len);
    reset(g);
    return 1;
}

int fld_level(fld_game *g, int level)
{
    fld_board b;
    uint8_t way[FLD_MOVES];
    int len;
    if (level < 0 || level >= FLD_LEVELS || !fld_levels[level]) return 0;
    if (!fld_decode(fld_levels[level], &b, way, &len)) return 0;
    return fld_load(g, &b, way, len);
}

int fld_done(const fld_game *g) { return fld_flooded(&g->b); }
int fld_out(const fld_game *g) { return !fld_done(g) && g->nmoves >= g->limit; }

int fld_can(const fld_game *g, int c)
{
    if (c < 0 || c >= g->b.k || c == g->b.c[0] || fld_done(g) || g->nmoves >= g->limit) return 0;
    int n = g->b.n;
    for (int i = 0; i < n * n; i++) {
        if (!g->in[i] || g->b.c[i] == c) continue;
        int x = i % n, y = i / n;
        if ((x > 0 && !g->in[i - 1] && g->b.c[i - 1] == c) || (x < n - 1 && !g->in[i + 1] && g->b.c[i + 1] == c) ||
            (y > 0 && !g->in[i - n] && g->b.c[i - n] == c) || (y < n - 1 && !g->in[i + n] && g->b.c[i + n] == c))
            return 1;
    }
    return 0;
}

int fld_play(fld_game *g, int c)
{
    if (!fld_can(g, c)) return 0;
    int got = fld_fill(&g->b, g->in, c);
    g->moves[g->nmoves++] = (uint8_t)c;
    return got;
}

int fld_undo(fld_game *g)
{
    if (!g->nmoves) return 0;
    int n = g->nmoves - 1, hints = g->hints;
    reset(g);
    for (int i = 0; i < n; i++) {
        fld_fill(&g->b, g->in, g->moves[i]);
        g->nmoves++;
    }
    g->hints = hints;
    return 1;
}

int fld_hint(fld_game *g)
{
    if (fld_done(g) || fld_out(g)) return -1;
    if (g->nmoves == 0 && g->par > 0) return g->way[0];   /* at the start: the best way known */
    int width;
    long nodes;
    effort(g->b.n, &width, &nodes);
    uint8_t way[FLD_MOVES];
    int len = fld_solve(&g->b, way, FLD_MOVES, width, 0, NULL);
    if (len <= 0 || len >= FLD_MOVES || !fld_can(g, way[0])) return -1;
    return way[0];
}

int fld_script(fld_game *g, const char *s)
{
    for (; *s; s++) {
        if (*s == ' ' || *s == ',') continue;
        if (*s == 'u') {
            if (!fld_undo(g)) return 0;
        } else if (*s == 'h') {
            int c = fld_hint(g);
            if (c < 0 || !fld_play(g, c)) return 0;
            g->hints++;
        } else if (*s >= '1' && *s <= '0' + g->b.k) {
            if (!fld_play(g, *s - '1')) return 0;
        } else
            return 0;
    }
    return 1;
}
