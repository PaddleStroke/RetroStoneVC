/*
 * Pipes: the rules (pipes.h): the maker (a spanning tree grown by chance), the deduction solver that keeps only the
 * puzzles found without guessing, the counter of solutions, the water's way from the source, and the player's tiles
 * with undo. No SDL: tested by tests/pipes_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/pipes.h"

#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"

static int opp(int d) { return (d + 2) & 3; }
static int bits(unsigned m) { return __builtin_popcount(m); }

void pp_variant(int v, int *n, int *wrap)
{
    static const int sizes[PP_VARIANTS] = {5, 7, 9, 11, 9};
    if (v < 0 || v >= PP_VARIANTS) v = 1;
    *n = sizes[v];
    *wrap = v == 4;
}

uint8_t pp_turn(uint8_t s, int k)
{
    k &= 3;
    return (uint8_t)(((s << k) | (s >> (4 - k))) & 15);
}

int pp_shape(uint8_t s, int *k)
{
    static const uint8_t drawn[4] = {PP_UP, PP_UP | PP_DOWN, PP_UP | PP_RIGHT, PP_UP | PP_RIGHT | PP_DOWN};
    for (int shape = 0; shape < 4; shape++)
        for (int i = 0; i < 4; i++)
            if (pp_turn(drawn[shape], i) == s) {
                if (k) *k = i;
                return shape;
            }
    if (k) *k = 0;
    return PP_END;
}

int pp_next(int n, int wrap, int t, int d)
{
    int x = t % n, y = t / n;
    x += d == 1 ? 1 : d == 3 ? -1 : 0;
    y += d == 2 ? 1 : d == 0 ? -1 : 0;
    if (wrap) return (y + n) % n * n + (x + n) % n;
    return x < 0 || y < 0 || x >= n || y >= n ? -1 : y * n + x;
}

/* ---- the deduction solver ---------------------------------------------------------------------------------------
 * Each tile keeps the set of its turns still possible (cand: bit m for the sides m), each side whether it is known
 * (known) and open (open: a pipe there, meeting the neighbour's). The deductions run until nothing more is found. */
typedef struct {
    uint16_t cand[PP_TILES];
    uint8_t known[PP_TILES], open[PP_TILES], deg[PP_TILES];
} dstate;

typedef struct { int n, wrap, nt; int nb[PP_TILES][4]; } board;

static void board_init(board *b, int n, int wrap)
{
    b->n = n;
    b->wrap = wrap;
    b->nt = n * n;
    for (int t = 0; t < b->nt; t++)
        for (int d = 0; d < 4; d++) b->nb[t][d] = pp_next(n, wrap, t, d);
}

static void dstate_init(const board *b, dstate *s, const uint8_t *sides)
{
    memset(s, 0, sizeof *s);
    for (int t = 0; t < b->nt; t++) {
        for (int k = 0; k < 4; k++) s->cand[t] |= (uint16_t)(1u << pp_turn(sides[t], k));
        s->deg[t] = (uint8_t)bits(sides[t]);
        for (int d = 0; d < 4; d++)
            if (b->nb[t][d] < 0) s->known[t] |= (uint8_t)(1u << d);   /* the board's edge: closed */
    }
}

/* a side found: both tiles learn it; 0: it contradicts what was known */
static int set_side(const board *b, dstate *s, int t, int d, int is_open, int *changed)
{
    int u = b->nb[t][d];
    unsigned bit = 1u << d;
    if (s->known[t] & bit) return !!(s->open[t] & bit) == is_open;
    s->known[t] |= (uint8_t)bit;
    if (is_open) s->open[t] |= (uint8_t)bit;
    if (u >= 0) {
        unsigned ub = 1u << opp(d);
        if (s->known[u] & ub) {
            if (!!(s->open[u] & ub) != is_open) return 0;
        } else {
            s->known[u] |= (uint8_t)ub;
            if (is_open) s->open[u] |= (uint8_t)ub;
        }
    } else if (is_open) {
        return 0;
    }
    *changed = 1;
    return 1;
}

/* the kinds of deductions used (pp_grade: fewer of them), the passes made */
static int rule_level = 3, rounds_made;

static int root(int *p, int x)
{
    while (p[x] != x) x = p[x] = p[p[x]];
    return x;
}

/* runs the deductions; 0: a contradiction */
static int propagate(const board *b, dstate *s)
{
    int nt = b->nt, changed = 1;
    int par[PP_TILES], size[PP_TILES], out[PP_TILES], ways[PP_TILES];
    while (changed) {
        changed = 0;
        rounds_made++;
        /* the tiles: the turns that fit the known sides; the sides all those turns agree on */
        for (int t = 0; t < nt; t++) {
            uint16_t c = 0;
            unsigned all = 15, any = 0;
            for (unsigned m = 0; m < 16; m++) {
                if (!(s->cand[t] >> m & 1)) continue;
                if ((m & s->known[t]) != (s->open[t] & s->known[t])) continue;
                c |= (uint16_t)(1u << m);
                all &= m;
                any |= m;
            }
            if (!c) return 0;
            if (c != s->cand[t]) s->cand[t] = c, changed = 1;
            for (int d = 0; d < 4; d++) {
                if (s->known[t] >> d & 1) continue;
                if (all >> d & 1) { if (!set_side(b, s, t, d, 1, &changed)) return 0; }
                else if (!(any >> d & 1)) { if (!set_side(b, s, t, d, 0, &changed)) return 0; }
            }
        }
        if (changed || rule_level < 1) continue;
        /* the groups of tiles joined by open sides: a loop is a contradiction */
        for (int t = 0; t < nt; t++) par[t] = t, size[t] = 1;
        for (int t = 0; t < nt; t++)
            for (int d = 1; d <= 2; d++) {     /* each side once: right and down */
                if (!(s->open[t] >> d & 1)) continue;
                int u = b->nb[t][d], a = root(par, t), c = root(par, u);
                if (a == c) return 0;
                par[a] = c;
                size[c] += size[a];
            }
        /* a side between two tiles of one group would close a loop: closed */
        for (int t = 0; t < nt; t++)
            for (int d = 0; d < 4; d++) {
                if (s->known[t] >> d & 1) continue;
                if (root(par, t) == root(par, b->nb[t][d]) && !set_side(b, s, t, d, 0, &changed)) return 0;
            }
        if (changed || rule_level < 2) continue;
        /* each group needs a way out (unless it is the whole board): its tiles' unknown sides, as many as each tile
         * can still open */
        for (int t = 0; t < nt; t++) out[t] = ways[t] = 0;
        for (int t = 0; t < nt; t++) {
            int cap = s->deg[t] - bits(s->open[t]), unk = 4 - bits(s->known[t]), r = root(par, t);
            if (cap > 0) {
                out[r] += cap < unk ? cap : unk;
                ways[r] += unk;
            }
        }
        for (int t = 0; t < nt && !changed; t++) {
            int r = root(par, t);
            if (r != t || size[r] == nt) continue;
            if (out[r] == 0) return 0;
            if (ways[r] != 1) continue;
            for (int u = 0; u < nt; u++)        /* its one way out: open */
                if (root(par, u) == r && s->deg[u] > bits(s->open[u]))
                    for (int d = 0; d < 4; d++)
                        if (!(s->known[u] >> d & 1) && !set_side(b, s, u, d, 1, &changed)) return 0;
        }
        if (changed || rule_level < 3) continue;
        /* a side joining two groups that would then have no way out: closed (two ends facing each other...) */
        for (int t = 0; t < nt; t++)
            for (int d = 1; d <= 2; d++) {
                if (s->known[t] >> d & 1) continue;
                int u = b->nb[t][d], ra = root(par, t), rb = root(par, u);
                if (size[ra] + size[rb] == nt) continue;
                int left = 0;
                for (int v = 0; v < nt && !left; v++) {
                    int rv = root(par, v);
                    if (rv != ra && rv != rb) continue;
                    int cap = s->deg[v] - bits(s->open[v]) - (v == t || v == u), unk = 0;
                    for (int e = 0; e < 4; e++) {
                        if (s->known[v] >> e & 1) continue;
                        int w = root(par, b->nb[v][e]);
                        if (w != ra && w != rb) unk++;
                    }
                    left += cap < unk ? cap : unk;
                }
                if (!left && !set_side(b, s, t, d, 0, &changed)) return 0;
            }
    }
    return 1;
}

static int found(const board *b, const dstate *s)
{
    int k = 0;
    for (int t = 0; t < b->nt; t++) k += bits(s->cand[t]) == 1;
    return k;
}

int pp_grade(int n, int wrap, const uint8_t *sides, int *rounds)
{
    int grade = -1;
    for (int level = 0; level <= 3 && grade < 0; level++) {
        rule_level = level;
        rounds_made = 0;
        if (pp_deduce(n, wrap, sides, NULL) == n * n) grade = level;
    }
    rule_level = 3;
    if (rounds) *rounds = rounds_made;
    return grade;
}

int pp_deduce(int n, int wrap, const uint8_t *sides, uint8_t *out)
{
    board b;
    dstate s;
    board_init(&b, n, wrap);
    dstate_init(&b, &s, sides);
    if (!propagate(&b, &s)) return -1;
    if (out)
        for (int t = 0; t < b.nt; t++) out[t] = bits(s.cand[t]) == 1 ? (uint8_t)__builtin_ctz(s.cand[t]) : 0xff;
    return found(&b, &s);
}

static int count(const board *b, dstate *s, int limit)
{
    if (!propagate(b, s)) return 0;
    int best = -1, bn = 99;
    for (int t = 0; t < b->nt; t++) {
        int k = bits(s->cand[t]);
        if (k > 1 && k < bn) bn = k, best = t;
    }
    if (best < 0) return 1;                     /* every tile found, no loop: one network */
    int total = 0;
    for (unsigned m = 0; m < 16 && total < limit; m++) {
        if (!(s->cand[best] >> m & 1)) continue;
        dstate c = *s;
        c.cand[best] = (uint16_t)(1u << m);
        total += count(b, &c, limit - total);
    }
    return total;
}

int pp_count(int n, int wrap, const uint8_t *sides, int limit)
{
    board b;
    static dstate s;
    board_init(&b, n, wrap);
    dstate_init(&b, &s, sides);
    return count(&b, &s, limit);
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
/* a tree over the grid grown from the source by edges picked at random, no tile with four pipes; 0: stuck */
static int grow(int n, int wrap, int source, uint32_t *rng, uint8_t *sol)
{
    int nt = n * n, in[PP_TILES] = {0}, have = 1;
    static int edge[PP_TILES * 4];
    int ne = 0;
    memset(sol, 0, (size_t)nt);
    in[source] = 1;
    for (int d = 0; d < 4; d++) edge[ne++] = source * 4 + d;
    while (have < nt) {
        if (!ne) return 0;
        int i = pr_below(rng, ne), e = edge[i], t = e / 4, d = e % 4, u = pp_next(n, wrap, t, d);
        if (u < 0 || in[u] || bits(sol[t]) >= 3) {
            edge[i] = edge[--ne];
            continue;
        }
        sol[t] |= (uint8_t)(1u << d);
        sol[u] |= (uint8_t)(1u << opp(d));
        in[u] = 1;
        have++;
        edge[i] = edge[--ne];
        for (int k = 0; k < 4; k++)
            if (k != opp(d)) edge[ne++] = u * 4 + k;
    }
    return 1;
}

int pp_make(int n, int wrap, uint32_t seed, uint8_t *sol, int *source)
{
    uint32_t rng = pr_seed(seed);
    int nt = n * n, src = n / 2 * n + n / 2, tries;
    uint8_t t[PP_TILES];
    *source = src;
    for (tries = 1; tries <= PP_TRIES; tries++) {
        while (!grow(n, wrap, src, &rng, t)) {}
        memcpy(sol, t, (size_t)nt);
        if (pp_deduce(n, wrap, t, NULL) == nt) return tries;
    }
    return tries;                               /* the last one, not found by deduction alone */
}

/* ---- the player's tiles ------------------------------------------------------------------------------------------ */
int pp_par(int n, const uint8_t *sol, const uint8_t *now)
{
    int par = 0;
    for (int t = 0; t < n * n; t++) {
        int k = 0;
        while (k < 4 && pp_turn(now[t], k) != sol[t]) k++;
        par += k < 4 ? k : 0;
    }
    return par;
}

void pp_load(pp_game *g, int n, int wrap, int source, const uint8_t *sol, const uint8_t *now)
{
    memset(g, 0, sizeof *g);
    g->n = n;
    g->wrap = wrap;
    g->source = source;
    memcpy(g->sol, sol, (size_t)n * n);
    memcpy(g->now, now, (size_t)n * n);
    g->par = pp_par(n, sol, now);
}

int pp_encode(const pp_game *g, char *out, int size)
{
    int n = g->n, k = snprintf(out, (size_t)size, "%d%s:", n, g->wrap ? "w" : "");
    if (k < 0 || k + n * n + 1 > size) return 0;
    for (int t = 0; t < n * n; t++) {
        int turns = 0;
        while (turns < 3 && pp_turn(g->sol[t], turns) != g->now[t]) turns++;
        out[k++] = PP_CODE[g->sol[t] * 4 + turns];
    }
    out[k] = 0;
    return 1;
}

int pp_decode(pp_game *g, const char *s)
{
    int n = 0, wrap = 0;
    while (*s >= '0' && *s <= '9') n = n * 10 + *s++ - '0';
    if (*s == 'w') wrap = 1, s++;
    if (*s++ != ':' || n < 2 || n > PP_MAX || (int)strlen(s) != n * n) return 0;
    uint8_t sol[PP_TILES], now[PP_TILES];
    for (int t = 0; t < n * n; t++) {
        const char *c = strchr(PP_CODE, s[t]);
        if (!c || !s[t]) return 0;
        int v = (int)(c - PP_CODE);
        sol[t] = (uint8_t)(v / 4);
        now[t] = pp_turn(sol[t], v % 4);
    }
    pp_load(g, n, wrap, n / 2 * n + n / 2, sol, now);
    return 1;
}

void pp_new(pp_game *g, int variant, uint32_t seed)
{
    int n, wrap, source;
    uint8_t sol[PP_TILES], now[PP_TILES];
    pp_variant(variant, &n, &wrap);
    pp_make(n, wrap, seed, sol, &source);
    uint32_t rng = pr_seed(seed ^ 0x51ed2701u);
    do {
        for (int t = 0; t < n * n; t++) now[t] = pp_turn(sol[t], pr_below(&rng, 4));
        pp_load(g, n, wrap, source, sol, now);
    } while (pp_solved(g));
}

static void room(pp_game *g)
{
    if (g->nh < PP_HIST) return;
    memmove(g->hist, g->hist + PP_HIST / 2, (PP_HIST / 2) * sizeof g->hist[0]);   /* forget the oldest half */
    g->nh -= PP_HIST / 2;
}

static int set(pp_game *g, int t, uint8_t sides)
{
    if (g->now[t] == sides) return 0;
    room(g);
    g->hist[g->nh++] = (pp_change){(uint8_t)t, g->now[t]};
    g->now[t] = sides;
    g->moves++;
    return 1;
}

int pp_tap(pp_game *g, int t)
{
    if (t < 0 || t >= g->n * g->n || pp_solved(g)) return 0;
    if (!set(g, t, pp_turn(g->now[t], 1))) return 0;   /* a tile that a turn leaves as it is (none: no cross) */
    return 1;
}

int pp_hint(pp_game *g)
{
    int nt = g->n * g->n, dist[PP_TILES], pick = -1;
    if (pp_solved(g)) return -1;
    pp_flow(g, dist, NULL);
    for (int t = 0; t < nt && pick < 0; t++) {  /* a wrong tile the water reaches in the solution, next to it */
        if (g->now[t] == g->sol[t]) continue;
        for (int d = 0; d < 4; d++) {
            int u = pp_next(g->n, g->wrap, t, d);
            if (u >= 0 && dist[u] >= 0 && (g->sol[t] >> d & 1) && g->now[u] == g->sol[u]) { pick = t; break; }
        }
    }
    for (int t = 0; t < nt && pick < 0; t++)
        if (g->now[t] != g->sol[t]) pick = t;
    if (pick < 0) return -1;
    set(g, pick, g->sol[pick]);
    g->hints++;
    return pick;
}

int pp_undo(pp_game *g)
{
    if (!g->nh) return -1;
    pp_change h = g->hist[--g->nh];
    g->now[h.tile] = h.was;
    g->moves--;
    return h.tile;
}

static int joined(const pp_game *g, int t, int d, int *u)
{
    if (!(g->now[t] >> d & 1)) return 0;
    *u = pp_next(g->n, g->wrap, t, d);
    return *u >= 0 && (g->now[*u] >> opp(d) & 1);
}

int pp_flow(const pp_game *g, int *dist, int *from)
{
    int nt = g->n * g->n, q[PP_TILES], qh = 0, qt = 0;
    for (int t = 0; t < nt; t++) {
        dist[t] = -1;
        if (from) from[t] = -1;
    }
    dist[g->source] = 0;
    q[qt++] = g->source;
    while (qh < qt) {
        int t = q[qh++], u;
        for (int d = 0; d < 4; d++)
            if (joined(g, t, d, &u) && dist[u] < 0) {
                dist[u] = dist[t] + 1;
                if (from) from[u] = t;
                q[qt++] = u;
            }
    }
    return qt;
}

int pp_open(const pp_game *g, int t, int d)
{
    int u;
    return (g->now[t] >> d & 1) && !joined(g, t, d, &u);
}

int pp_solved(const pp_game *g)
{
    int nt = g->n * g->n, dist[PP_TILES];
    for (int t = 0; t < nt; t++)
        for (int d = 0; d < 4; d++)
            if (pp_open(g, t, d)) return 0;
    return pp_flow(g, dist, NULL) == nt;
}

int pp_wrong(const pp_game *g)
{
    int k = 0;
    for (int t = 0; t < g->n * g->n; t++) k += g->now[t] != g->sol[t];
    return k;
}

int pp_tile(const pp_game *g, const char *s)
{
    if (s[0] < 'a' || s[0] >= 'a' + g->n || s[1] < '1' || s[1] > '9') return -1;
    int y = s[1] - '0';
    if (s[2] >= '0' && s[2] <= '9') y = y * 10 + s[2] - '0';
    if (y < 1 || y > g->n) return -1;
    return (y - 1) * g->n + s[0] - 'a';
}

int pp_script(pp_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == '?') {
            if (pp_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '=') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int t = 0; t < g->n * g->n && k > 0; t++)
                if (g->now[t] != g->sol[t]) { set(g, t, g->sol[t]); k--; }
            continue;
        }
        int t = pp_tile(g, s);
        if (t < 0) return 0;
        s += t / g->n + 1 >= 10 ? 3 : 2;
        int times = 1;
        if (*s == '*') {
            times = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) times = times * 10 + *s - '0';
        }
        for (int i = 0; i < times; i++)
            if (!pp_tap(g, t)) return 0;
    }
    return 1;
}
