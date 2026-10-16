/*
 * Traffic Jam: the rules (parking.h): lots as text, drives, moves, undo, the breadth-first search for the fewest moves
 * (the par, the hint), a lot's whole family of positions with their distances (the puzzles made at random and the
 * levels' tool), the levels. No SDL: tested by tests/parking_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/parking.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

#define CELLS (PKJ_N * PKJ_N)

int pkj_pos(const pkj_car *c) { return c->vert ? c->y : c->x; }

static void place(pkj_car *c, int pos)
{
    if (c->vert) c->y = (int8_t)pos;
    else c->x = (int8_t)pos;
}

static uint64_t car_mask(const pkj_car *c)
{
    uint64_t m = 0;
    for (int k = 0; k < c->len; k++) m |= 1ull << (c->vert ? c->x + (c->y + k) * PKJ_N : c->x + k + c->y * PKJ_N);
    return m;
}

static uint64_t occupied(const pkj_game *g, int except)
{
    uint64_t m = 0;
    for (int i = 0; i < g->nc; i++)
        if (i != except) m |= car_mask(&g->c[i]);
    return m;
}

static void reset(pkj_game *g)
{
    g->nh = g->moves = 0;
    g->last = -1;
}

int pkj_parse(pkj_game *g, const char *s)
{
    memset(g, 0, sizeof *g);
    if (strlen(s) != CELLS) return 0;
    int at[26];
    memset(at, -1, sizeof at);
    for (int c = 0; c < CELLS; c++) {
        if (s[c] == '.') continue;
        if (s[c] < 'A' || s[c] > 'Z') return 0;
        int l = s[c] - 'A', x = c % PKJ_N, y = c / PKJ_N;
        if (at[l] < 0) {                        /* its first bay: its top left */
            if (g->nc >= PKJ_CARS) return 0;
            at[l] = g->nc;
            g->c[g->nc++] = (pkj_car){(int8_t)x, (int8_t)y, 1, -1};
            continue;
        }
        pkj_car *v = &g->c[at[l]];
        int vert = x == v->x && y == v->y + v->len, along = y == v->y && x == v->x + v->len;
        if (v->vert < 0) v->vert = (int8_t)(vert ? 1 : along ? 0 : -1);
        else if (!(v->vert ? vert : along)) return 0;
        if (v->vert < 0) return 0;
        v->len++;
    }
    if (at[0] != 0 && g->nc) {                  /* the red car first */
        if (at[0] < 0) return 0;
        pkj_car t = g->c[0];
        g->c[0] = g->c[at[0]];
        g->c[at[0]] = t;
    }
    for (int i = 0; i < g->nc; i++)
        if (g->c[i].len < 2 || g->c[i].len > 3 || g->c[i].vert < 0) return 0;
    if (!g->nc || g->c[0].vert || g->c[0].len != 2 || g->c[0].y != PKJ_ROW) return 0;
    reset(g);
    return 1;
}

void pkj_text(const pkj_game *g, char *out)
{
    memset(out, '.', CELLS);
    out[CELLS] = 0;
    for (int i = 0; i < g->nc; i++)
        for (int c = 0; c < CELLS; c++)
            if (car_mask(&g->c[i]) >> c & 1) out[c] = (char)('A' + i);
}

int pkj_at(const pkj_game *g, int x, int y)
{
    if (x < 0 || y < 0 || x >= PKJ_N || y >= PKJ_N) return -1;
    for (int i = 0; i < g->nc; i++)
        if (car_mask(&g->c[i]) >> (x + y * PKJ_N) & 1) return i;
    return -1;
}

void pkj_range(const pkj_game *g, int i, int *lo, int *hi)
{
    if (i < 0 || i >= g->nc) { *lo = 0; *hi = -1; return; }
    uint64_t o = occupied(g, i);
    pkj_car c = g->c[i];
    int p = pkj_pos(&c), a = p, b = p;
    for (;;) {
        place(&c, a - 1);
        if (a - 1 < 0 || (car_mask(&c) & o)) break;
        a--;
    }
    for (;;) {
        place(&c, b + 1);
        if (b + 1 > PKJ_N - c.len || (car_mask(&c) & o)) break;
        b++;
    }
    *lo = a;
    *hi = b;
}

int pkj_drive(pkj_game *g, int i, int pos)
{
    int lo, hi;
    pkj_range(g, i, &lo, &hi);
    if (pos < lo || pos > hi || pos == pkj_pos(&g->c[i])) return 0;
    if (g->last == i && g->nh && g->hist[g->nh - 1].car == i) {   /* driven again at once: the same move */
        place(&g->c[i], pos);
        if (pos == g->hist[g->nh - 1].from) {   /* back where it was: no move at all */
            g->nh--;
            g->moves--;
            g->last = -1;
        }
        return 1;
    }
    if (g->nh >= PKJ_HIST) {                    /* forget the oldest half */
        memmove(g->hist, g->hist + PKJ_HIST / 2, sizeof g->hist / 2);
        g->nh -= PKJ_HIST / 2;
    }
    g->hist[g->nh].car = (int8_t)i;
    g->hist[g->nh].from = (int8_t)pkj_pos(&g->c[i]);
    g->nh++;
    g->moves++;
    g->last = i;
    place(&g->c[i], pos);
    return 1;
}

int pkj_undo(pkj_game *g)
{
    if (!g->nh) return -1;
    g->nh--;
    int i = g->hist[g->nh].car;
    place(&g->c[i], g->hist[g->nh].from);
    g->moves--;
    g->last = -1;
    return i;
}

int pkj_solved(const pkj_game *g) { return g->nc > 0 && g->c[0].x == PKJ_N - 2; }

/* ---- positions as codes: each vehicle's place along its line, 3 bits each (its line and length are the family's) */
typedef struct {
    int nc;
    uint64_t mask[PKJ_CARS][PKJ_N];             /* the bays it covers at each place */
    int8_t top[PKJ_CARS];                       /* its farthest place */
} geo;

static void geo_of(const pkj_game *g, geo *G)
{
    G->nc = g->nc;
    for (int i = 0; i < g->nc; i++) {
        pkj_car c = g->c[i];
        G->top[i] = (int8_t)(PKJ_N - c.len);
        for (int p = 0; p <= G->top[i]; p++) {
            place(&c, p);
            G->mask[i][p] = car_mask(&c);
        }
    }
}

static uint64_t code_of(const pkj_game *g)
{
    uint64_t k = 0;
    for (int i = 0; i < g->nc; i++) k |= (uint64_t)pkj_pos(&g->c[i]) << (3 * i);
    return k;
}

static void load(pkj_game *g, uint64_t k)
{
    for (int i = 0; i < g->nc; i++) place(&g->c[i], (int)(k >> (3 * i) & 7));
}

static int goal(uint64_t k) { return (k & 7) == PKJ_N - 2; }

/* the positions one move away: each one's code, its vehicle and place (out: up to 5 a vehicle) */
static int next_of(const geo *G, uint64_t k, uint64_t *out, int8_t *car, int8_t *pos)
{
    uint64_t o = 0;
    int n = 0;
    for (int i = 0; i < G->nc; i++) o |= G->mask[i][k >> (3 * i) & 7];
    for (int i = 0; i < G->nc; i++) {
        int p = (int)(k >> (3 * i) & 7);
        uint64_t rest = o & ~G->mask[i][p], base = k & ~(7ull << (3 * i));
        for (int q = p - 1; q >= 0 && !(G->mask[i][q] & rest); q--) {
            out[n] = base | (uint64_t)q << (3 * i);
            if (car) car[n] = (int8_t)i, pos[n] = (int8_t)q;
            n++;
        }
        for (int q = p + 1; q <= G->top[i] && !(G->mask[i][q] & rest); q++) {
            out[n] = base | (uint64_t)q << (3 * i);
            if (car) car[n] = (int8_t)i, pos[n] = (int8_t)q;
            n++;
        }
    }
    return n;
}

/* a set of codes: open addressing, each code's index in the order found */
typedef struct {
    uint64_t *key;                              /* code + 1 (0: free) */
    int32_t *idx;
    uint64_t *order;
    int32_t *parent;
    int n, cap, size;
} set;

static void set_free(set *S)
{
    free(S->key);
    free(S->idx);
    free(S->order);
    free(S->parent);
    memset(S, 0, sizeof *S);
}

static int set_new(set *S, int cap, int parents)
{
    memset(S, 0, sizeof *S);
    S->size = 1024;
    while (S->size < 2 * cap) S->size *= 2;
    S->cap = cap;
    S->key = calloc((size_t)S->size, sizeof *S->key);
    S->idx = malloc((size_t)S->size * sizeof *S->idx);
    S->order = malloc((size_t)cap * sizeof *S->order);
    S->parent = parents ? malloc((size_t)cap * sizeof *S->parent) : NULL;
    if (S->key && S->idx && S->order && (S->parent || !parents)) return 1;
    set_free(S);
    return 0;
}

/* the code's index; -1 not there (add: put in, -2 when full) */
static int set_find(set *S, uint64_t k, int add, int parent)
{
    uint64_t h = (k + 1) * 0x9e3779b97f4a7c15ull;
    int s = (int)(h >> 40) & (S->size - 1);
    while (S->key[s]) {
        if (S->key[s] == k + 1) return S->idx[s];
        s = (s + 1) & (S->size - 1);
    }
    if (!add) return -1;
    if (S->n >= S->cap) return -2;
    S->key[s] = k + 1;
    S->idx[s] = S->n;
    S->order[S->n] = k;
    if (S->parent) S->parent[S->n] = parent;
    return S->n++;
}

/* breadth first from g to a solved position: its index in S (S kept for the way back); -1 none, -2 too many */
static int search(const pkj_game *g, set *S, geo *G, int cap)
{
    geo_of(g, G);
    if (!set_new(S, cap, 1)) return -2;
    set_find(S, code_of(g), 1, -1);
    uint64_t nx[PKJ_CARS * PKJ_N];
    for (int at = 0; at < S->n; at++) {
        if (goal(S->order[at])) return at;
        int n = next_of(G, S->order[at], nx, NULL, NULL);
        for (int e = 0; e < n; e++)
            if (set_find(S, nx[e], 1, at) == -2) { set_free(S); return -2; }
    }
    return -1;
}

static int depth(const set *S, int at)
{
    int d = 0;
    for (; S->parent[at] >= 0; at = S->parent[at]) d++;
    return d;
}

int pkj_way(const pkj_game *g, int8_t *car, int8_t *pos, int max)
{
    if (pkj_solved(g)) return 0;
    set S;
    geo G;
    int at = search(g, &S, &G, 100000), n = -1;
    if (at == -2) at = search(g, &S, &G, PKJ_FAMILY);   /* a family that large: once more, with room */
    if (at >= 0) {
        n = depth(&S, at);
        for (int k = n - 1; k >= 0; k--, at = S.parent[at]) {
            uint64_t a = S.order[S.parent[at]], b = S.order[at];
            for (int i = 0; i < g->nc; i++)
                if ((a >> (3 * i) & 7) != (b >> (3 * i) & 7) && k < max) {
                    car[k] = (int8_t)i;
                    pos[k] = (int8_t)(b >> (3 * i) & 7);
                }
        }
    }
    set_free(&S);
    return n;
}

int pkj_solve(const pkj_game *g, int *car, int *pos)
{
    int8_t c[1], p[1];
    int n = pkj_way(g, c, p, 1);
    if (n > 0) {
        if (car) *car = c[0];
        if (pos) *pos = p[0];
    }
    return n;
}

int pkj_movers(const pkj_game *g)
{
    int8_t c[256], p[256];
    int n = pkj_way(g, c, p, 256), seen = 0, count = 0;
    for (int k = 0; k < n && k < 256; k++)
        if (!(seen >> c[k] & 1)) {
            seen |= 1 << c[k];
            count++;
        }
    return count;
}

int pkj_family(const pkj_game *g, int cap, void (*each)(const pkj_game *pos, int dist, void *ctx), void *ctx)
{
    if (cap > PKJ_FAMILY) cap = PKJ_FAMILY;
    set S;
    geo G;
    geo_of(g, &G);
    if (!set_new(&S, cap, 0)) return 0;
    uint64_t nx[PKJ_CARS * PKJ_N];
    set_find(&S, code_of(g), 1, -1);
    for (int at = 0; at < S.n; at++) {
        int n = next_of(&G, S.order[at], nx, NULL, NULL);
        for (int e = 0; e < n; e++)
            if (set_find(&S, nx[e], 1, -1) == -2) { set_free(&S); return 0; }
    }
    /* the fewest moves out: breadth first from every solved position (every move can be driven back) */
    int16_t *dist = malloc((size_t)S.n * sizeof *dist);
    int32_t *queue = malloc((size_t)S.n * sizeof *queue);
    int total = S.n;
    if (!dist || !queue) total = 0;
    else {
        int head = 0, tail = 0;
        for (int i = 0; i < S.n; i++) {
            dist[i] = -1;
            if (goal(S.order[i])) { dist[i] = 0; queue[tail++] = i; }
        }
        while (head < tail) {
            int at = queue[head++], n = next_of(&G, S.order[at], nx, NULL, NULL);
            for (int e = 0; e < n; e++) {
                int j = set_find(&S, nx[e], 0, -1);
                if (j >= 0 && dist[j] < 0) {
                    dist[j] = (int16_t)(dist[at] + 1);
                    queue[tail++] = j;
                }
            }
        }
        pkj_game pos = *g;
        reset(&pos);
        for (int i = 0; i < S.n; i++) {
            load(&pos, S.order[i]);
            pos.par = dist[i];
            each(&pos, dist[i], ctx);
        }
    }
    free(dist);
    free(queue);
    set_free(&S);
    return total;
}

void pkj_fill(pkj_game *g, int count, uint32_t *rng)
{
    memset(g, 0, sizeof *g);
    g->c[g->nc++] = (pkj_car){(int8_t)pr_below(rng, PKJ_N - 2), PKJ_ROW, 2, 0};
    uint64_t o = car_mask(&g->c[0]);
    for (int k = 0; k < count && g->nc < PKJ_CARS; k++)
        for (int tries = 0; tries < 40; tries++) {
            pkj_car c;
            c.len = (int8_t)(pr_below(rng, 4) ? 2 : 3);
            c.vert = (int8_t)pr_below(rng, 2);
            if (c.vert) {
                c.x = (int8_t)pr_below(rng, PKJ_N);
                c.y = (int8_t)pr_below(rng, PKJ_N - c.len + 1);
            } else {                            /* never across the red car's row: it would stay shut */
                c.y = (int8_t)pr_below(rng, PKJ_N - 1);
                if (c.y >= PKJ_ROW) c.y++;
                c.x = (int8_t)pr_below(rng, PKJ_N - c.len + 1);
            }
            if (car_mask(&c) & o) continue;
            o |= car_mask(&c);
            g->c[g->nc++] = c;
            break;
        }
    reset(g);
}

int pkj_script(pkj_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h' || *s == '=') {
            int all = *s++ == '=';
            do {
                int i, p;
                if (pkj_solve(g, &i, &p) <= 0) break;
                g->last = -1;
                pkj_drive(g, i, p);
            } while (all && !pkj_solved(g));
            continue;
        }
        if (*s < 'a' || *s >= 'a' + PKJ_N || s[1] < '1' || s[1] >= '1' + PKJ_N) return 0;
        int i = pkj_at(g, s[0] - 'a', s[1] - '1'), d = 0, n = 0;
        s += 2;
        if (i < 0) return 0;
        for (; *s && strchr("udlr", *s); s++, n++) {
            int along = g->c[i].vert ? (*s == 'u' || *s == 'd') : (*s == 'l' || *s == 'r');
            if (!along) return 0;
            d += *s == 'l' || *s == 'u' ? -1 : 1;
        }
        g->last = -1;                           /* a vehicle named: a move of its own */
        if (!n || !pkj_drive(g, i, pkj_pos(&g->c[i]) + d)) return 0;
    }
    return 1;
}

#ifndef PKJ_GEN                                 /* not in the levels' tool (tools/parking_gen.c): it makes them */
#include "games/parking_levels.h"

int pkj_level(pkj_game *g, int level)
{
    if (level < 0 || level >= PKJ_LEVELS || !pkj_parse(g, pkj_levels[level])) return 0;
    g->par = pkj_level_par[level];
    return 1;
}

/* the positions of a family at the distance wanted: the farthest of them up to hi, drawn among those at it (any:
 * drawn among all of them from lo to hi alike) */
typedef struct { int lo, hi, any, best, seen; uint32_t *rng; pkj_game pick; } wish;

static void consider(const pkj_game *pos, int dist, void *ctx)
{
    wish *w = ctx;
    if (dist < w->lo || dist > w->hi || (!w->any && dist < w->best)) return;
    if (!w->any && dist > w->best) w->seen = 0;
    if (pr_below(w->rng, ++w->seen) == 0) {
        w->pick = *pos;
        w->best = dist;
    }
}

static const int LO[PKJ_VARIANTS] = {5, 10, 17}, HI[PKJ_VARIANTS] = {9, 16, 99};

/* lots of its own first (a few for the hard ones: deep lots are rare), else a position of a hard level's family short
 * of the level's own start (the families of the levels are small: found in a few ms) */
int pkj_random(pkj_game *g, int variant, uint32_t seed)
{
    int v = variant < 0 || variant >= PKJ_VARIANTS ? 0 : variant;
    uint32_t s = pr_seed(seed);
    wish w = {LO[v], HI[v], 0, -1, 0, &s, {0}};
    for (int tries = 0; tries < (v == 2 ? 6 : 40) && w.best < 0; tries++) {
        pkj_game t;
        pkj_fill(&t, 10 + pr_below(&s, 4), &s);
        pkj_family(&t, 30000, consider, &w);
    }
    for (int tries = 0; tries < 40 && w.best < 0; tries++) {
        int l = pr_below(&s, PKJ_LEVELS);
        pkj_game t;
        if (pkj_level_par[l] <= w.lo || !pkj_level(&t, l)) continue;
        w.any = 1;
        w.hi = pkj_level_par[l] - 1 < HI[v] ? pkj_level_par[l] - 1 : HI[v];
        pkj_family(&t, 30000, consider, &w);
    }
    if (w.best < 0) return 0;
    *g = w.pick;
    reset(g);
    g->par = w.best;
    return 1;
}
#endif
