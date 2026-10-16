/*
 * Nurikabe: the rules (nurikabe.h): the ways of reasoning the solver grades with, the maker, the player's grid and
 * undo. No SDL: tested by tests/nurikabe_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/nurikabe.h"

#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"

#define LAND 1u                                 /* a cell's values: bit 0 land, bit 1 sea */
#define SEA 2u
#define BOTH 3u

static const int SIZES[NRK_SIZES] = {6, 7, 8};

int nrk_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % NRK_SIZES]; }
int nrk_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

/* ---- what the solver knows of a grid: each cell's neighbours, the land's cells ----------------------------------- */
typedef struct {
    const nrk_puzzle *p;
    int n, cells, land;                         /* land: the cells of all the islands (the numbers' sum) */
    uint8_t nn[NRK_CELLS], nb[NRK_CELLS][4];
} ctx;

static void ctx_make(ctx *k, const nrk_puzzle *p)
{
    int n = p->n;
    k->p = p;
    k->n = n;
    k->cells = n * n;
    k->land = 0;
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        k->nn[c] = 0;
        if (y > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - n);
        if (x > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - 1);
        if (x < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + 1);
        if (y < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + n);
        k->land += p->clue[c];
    }
}

static void grid_of(const ctx *k, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < k->cells; c++) g->d[c] = (uint16_t)(k->p->clue[c] ? LAND : BOTH);
}

/* the pieces of land known: each cell's piece (-1: not land), each piece's size and number (0: none yet) */
typedef struct {
    int16_t of[NRK_CELLS];
    int count, size[NRK_CELLS], num[NRK_CELLS];
} pieces;

/* -1: a piece with two numbers, or bigger than its number */
static int land_pieces(const ctx *k, const bench_grid *g, pieces *P)
{
    int stack[NRK_CELLS];
    P->count = 0;
    for (int c = 0; c < k->cells; c++) P->of[c] = -1;
    for (int c = 0; c < k->cells; c++) {
        if (g->d[c] != LAND || P->of[c] >= 0) continue;
        int id = P->count++, sp = 0, size = 0, num = 0;
        P->of[c] = (int16_t)id;
        stack[sp++] = c;
        while (sp) {
            int x = stack[--sp];
            size++;
            if (k->p->clue[x]) {
                if (num) return -1;
                num = k->p->clue[x];
            }
            for (int j = 0; j < k->nn[x]; j++) {
                int o = k->nb[x][j];
                if (g->d[o] == LAND && P->of[o] < 0) P->of[o] = (int16_t)id, stack[sp++] = o;
            }
        }
        if (num && size > num) return -1;
        P->size[id] = size;
        P->num[id] = num;
    }
    return 0;
}

/* the cells an island (piece q, numbered, not complete) may still reach: within its missing cells, through cells not
 * yet known and land with no number, never next to another numbered piece; `blocked` a cell left out (-1: none).
 * Marks them in seen (== id), returns how many (its own cells left out) */
static int reach(const ctx *k, const bench_grid *g, const pieces *P, int q, int blocked, int *seen, int id)
{
    int left = P->num[q] - P->size[q], dist[NRK_CELLS], queue[NRK_CELLS], qh = 0, qt = 0, found = 0;
    for (int c = 0; c < k->cells; c++)
        if (P->of[c] == q) seen[c] = id, dist[c] = 0, queue[qt++] = c;
    while (qh < qt) {
        int x = queue[qh++];
        if (dist[x] >= left) continue;
        for (int j = 0; j < k->nn[x]; j++) {
            int o = k->nb[x][j], ok = 1;
            if (seen[o] == id || o == blocked || g->d[o] == SEA) continue;
            if (g->d[o] == LAND && P->num[P->of[o]]) continue;   /* another island */
            for (int i = 0; i < k->nn[o] && ok; i++) {
                int z = k->nb[o][i];
                if (g->d[z] == LAND && P->num[P->of[z]] && P->of[z] != q) ok = 0;
            }
            if (!ok) continue;
            seen[o] = id;
            dist[o] = dist[x] + 1;
            queue[qt++] = o;
            found++;
        }
    }
    return found;
}

/* a fact found: the cell can only be `to` (all the facts of a pass are read from the grid as it was at its start, then
 * put together: one cell that can be neither is a contradiction) */
static void force(bench_grid *g, int c, unsigned to) { g->d[c] &= (uint16_t)to; }

/* the facts of a pass put in: 1 something new, -1 a contradiction */
static int settled(const bench_grid *was, const bench_grid *g, int cells)
{
    int ch = 0;
    for (int c = 0; c < cells; c++) {
        if (!g->d[c]) return -1;
        ch |= g->d[c] != was->d[c];
    }
    return ch;
}

/* easy */
static int easy(bench_grid *g, const ctx *k)
{
    pieces P;
    if (land_pieces(k, g, &P) < 0) return -1;
    const bench_grid w = *g;                    /* the grid as the pass found it */
    int n = k->n, cells = k->cells, land = 0, sea = 0;
    for (int c = 0; c < cells; c++) land += w.d[c] == LAND, sea += w.d[c] == SEA;
    if (land > k->land || cells - sea < k->land) return -1;
    int exits[NRK_CELLS], exit_at[NRK_CELLS];
    memset(exits, 0, sizeof exits);
    for (int c = 0; c < cells; c++) {
        if (w.d[c] != BOTH) continue;
        int got[4], ng = 0, a = -1;
        for (int j = 0; j < k->nn[c]; j++) {
            int q = P.of[k->nb[c][j]], dup = 0;
            if (q < 0) continue;
            if (P.num[q]) {                     /* round a complete island, between two islands: sea */
                if (P.size[q] == P.num[q] || (a >= 0 && a != q)) force(g, c, SEA);
                a = q;
            }
            for (int i = 0; i < ng; i++) dup |= got[i] == q;
            if (dup) continue;
            got[ng++] = q;
            exits[q]++;
            exit_at[q] = c;
        }
    }
    for (int q = 0; q < P.count; q++) {         /* a piece of land with one way out takes it */
        if (P.num[q] && P.size[q] == P.num[q]) continue;
        if (!exits[q]) return -1;
        if (exits[q] == 1) force(g, exit_at[q], LAND);
    }
    for (int y = 0; y + 1 < n; y++)             /* three sea cells of a square: the fourth is land */
        for (int x = 0; x + 1 < n; x++) {
            int c = y * n + x, sq[4] = {c, c + 1, c + n, c + n + 1}, s = 0, u = -1;
            for (int i = 0; i < 4; i++) {
                s += w.d[sq[i]] == SEA;
                if (w.d[sq[i]] == BOTH) u = sq[i];
            }
            if (s == 4) return -1;
            if (s == 3 && u >= 0) force(g, u, LAND);
        }
    if (sea > 0 && sea < cells) {               /* a piece of sea with one way out takes it */
        int16_t of[NRK_CELLS];
        int stack[NRK_CELLS], np = 0, ways[NRK_CELLS], way[NRK_CELLS];
        for (int c = 0; c < cells; c++) of[c] = -1;
        for (int c = 0; c < cells; c++) {
            if (w.d[c] != SEA || of[c] >= 0) continue;
            int id = np++, sp = 0;
            of[c] = (int16_t)id;
            ways[id] = 0;
            stack[sp++] = c;
            while (sp) {
                int x = stack[--sp];
                for (int j = 0; j < k->nn[x]; j++) {
                    int o = k->nb[x][j];
                    if (w.d[o] == SEA && of[o] < 0) of[o] = (int16_t)id, stack[sp++] = o;
                }
            }
        }
        for (int c = 0; np > 1 && c < cells; c++) {
            if (w.d[c] != BOTH) continue;
            int got[4], ng = 0;
            for (int j = 0; j < k->nn[c]; j++) {
                int o = k->nb[c][j], dup = 0;
                if (w.d[o] != SEA) continue;
                for (int i = 0; i < ng; i++) dup |= got[i] == of[o];
                if (dup) continue;
                got[ng++] = of[o];
                ways[of[o]]++;
                way[of[o]] = c;
            }
        }
        for (int id = 0; np > 1 && id < np; id++) {
            if (!ways[id]) return -1;
            if (ways[id] == 1) force(g, way[id], SEA);
        }
    }
    int seen[NRK_CELLS], hit[NRK_CELLS];        /* a cell no island can reach: sea */
    memset(hit, 0, sizeof hit);
    for (int c = 0; c < cells; c++) seen[c] = -1;
    for (int q = 0; q < P.count; q++) {
        if (!P.num[q] || P.size[q] == P.num[q]) continue;
        reach(k, &w, &P, q, -1, seen, q);
        for (int c = 0; c < cells; c++)
            if (seen[c] == q) hit[c] = 1;
    }
    for (int c = 0; c < cells; c++) {
        if (hit[c]) continue;
        if (w.d[c] == BOTH) force(g, c, SEA);
        else if (w.d[c] == LAND && !P.num[P.of[c]]) return -1;   /* land no number can reach */
    }
    return settled(&w, g, cells);
}

/* the cells not yet known the sea cannot do without (land there would cut it in pieces): the cut points of the sea
 * and the unknown cells (depth first, the low points), each piece's sea counted */
typedef struct {
    const ctx *k;
    const bench_grid *g;
    int disc[NRK_CELLS], low[NRK_CELLS], sub[NRK_CELLS], t, seas;
    uint8_t *cut;
} cuts;

static void cut_dfs(cuts *D, int u, int parent)
{
    const ctx *k = D->k;
    int split = 0, apart = 0;                   /* the pieces with sea cut off below u, their sea */
    D->disc[u] = D->low[u] = ++D->t;
    D->sub[u] = D->g->d[u] == SEA;
    for (int j = 0; j < k->nn[u]; j++) {
        int v = k->nb[u][j];
        if (D->g->d[v] == LAND) continue;
        if (!D->disc[v]) {
            cut_dfs(D, v, u);
            D->sub[u] += D->sub[v];
            if (D->low[v] < D->low[u]) D->low[u] = D->low[v];
            if (D->low[v] >= D->disc[u] && D->sub[v]) split++, apart += D->sub[v];
        } else if (v != parent && D->disc[v] < D->low[u]) {
            D->low[u] = D->disc[v];
        }
    }
    if (D->g->d[u] == BOTH && split + (D->seas - apart > 0) >= 2) D->cut[u] = 1;
}

/* 0: the sea is in pieces already */
static int sea_cuts(const ctx *k, const bench_grid *g, uint8_t *cut)
{
    static cuts D;
    int first = -1;
    memset(cut, 0, (size_t)k->cells);
    memset(D.disc, 0, sizeof D.disc);
    D.k = k;
    D.g = g;
    D.t = D.seas = 0;
    D.cut = cut;
    for (int c = 0; c < k->cells; c++)
        if (g->d[c] == SEA) {
            D.seas++;
            if (first < 0) first = c;
        }
    if (D.seas < 2) return 1;
    cut_dfs(&D, first, -1);
    return D.sub[first] == D.seas;
}

/* medium */
static int medium(bench_grid *g, const ctx *k)
{
    pieces P;
    if (land_pieces(k, g, &P) < 0) return -1;
    const bench_grid w = *g;
    int cells = k->cells, land = 0, sea = 0;
    for (int c = 0; c < cells; c++) land += w.d[c] == LAND, sea += w.d[c] == SEA;
    for (int c = 0; c < cells; c++) {
        if (w.d[c] != BOTH) continue;
        if (land == k->land) force(g, c, SEA);  /* the land all found, or the sea */
        if (cells - sea == k->land) force(g, c, LAND);
        int got[4], ng = 0, num = 0, size = 1;  /* land here would join an island to land: too big */
        for (int j = 0; j < k->nn[c]; j++) {
            int q = P.of[k->nb[c][j]], dup = 0;
            if (q < 0) continue;
            for (int i = 0; i < ng; i++) dup |= got[i] == q;
            if (dup) continue;
            got[ng++] = q;
            size += P.size[q];
            if (P.num[q]) num = num ? -1 : P.num[q];
        }
        if (num > 0 && size > num) force(g, c, SEA);
    }
    uint8_t cut[NRK_CELLS];                     /* a cell the sea cannot do without */
    if (!sea_cuts(k, &w, cut)) return -1;
    for (int c = 0; c < cells; c++)
        if (cut[c]) force(g, c, SEA);
    static int seen[NRK_CELLS], serial;         /* a way out an island cannot do without */
    for (int q = 0; q < P.count; q++) {
        if (!P.num[q] || P.size[q] == P.num[q]) continue;
        int left = P.num[q] - P.size[q], id = ++serial, cand[NRK_CELLS], nc = 0;
        if (reach(k, &w, &P, q, -1, seen, id) < left) return -1;
        for (int c = 0; c < cells; c++) {
            if (w.d[c] != BOTH || seen[c] != id) continue;
            int next = 0;
            for (int j = 0; j < k->nn[c]; j++) next |= P.of[k->nb[c][j]] == q;
            if (next) cand[nc++] = c;
        }
        for (int i = 0; i < nc; i++)
            if (reach(k, &w, &P, q, cand[i], seen, ++serial) < left) force(g, cand[i], LAND);
    }
    return settled(&w, g, cells);
}


static long finds[2];                           /* each tier's finds (nrk_effort counts them) */

static int deduce(bench_grid *g, int tier, void *c)
{
    int res = tier ? medium(g, (const ctx *)c) : easy(g, (const ctx *)c);
    if (res > 0) finds[tier ? 1 : 0]++;
    return res;
}

static int valid(const bench_grid *g, void *cv)
{
    const ctx *k = cv;
    pieces P;
    int n = k->n;
    if (land_pieces(k, g, &P) < 0) return 0;
    for (int q = 0; q < P.count; q++)
        if (!P.num[q] || P.size[q] != P.num[q]) return 0;
    for (int y = 0; y + 1 < n; y++)
        for (int x = 0; x + 1 < n; x++) {
            int c = y * n + x;
            if (g->d[c] == SEA && g->d[c + 1] == SEA && g->d[c + n] == SEA && g->d[c + n + 1] == SEA) return 0;
        }
    uint8_t cut[NRK_CELLS];
    return sea_cuts(k, g, cut);
}

static bench_rules rules_of(ctx *k, long *work)
{
    return (bench_rules){.vars = k->cells, .tiers = 2, .probe_tiers = 1,
                        .deduce = deduce, .valid = valid, .ctx = k, .work = work};
}

static int grade_work(const nrk_puzzle *p, uint8_t *sol, long *work, int cap)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, work);
    bench_grid g;
    grid_of(&k, &g);
    int lv = bench_grade(&g, &r, cap);
    if (sol && lv >= BENCH_EASY && lv <= BENCH_HARD)
        for (int c = 0; c < k.cells; c++) sol[c] = g.d[c] == SEA;
    return lv;
}

int nrk_grade(const nrk_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD); }

int nrk_count(const nrk_puzzle *p, int limit)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid g;
    grid_of(&k, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

int nrk_effort(const nrk_puzzle *p, long *passes, long *mediums)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid g;
    grid_of(&k, &g);
    finds[1] = 0;
    int lv = bench_grade(&g, &r, BENCH_HARD);
    if (passes) *passes = r.passes;
    if (mediums) *mediums = finds[1];
    return lv;
}

/* ---- a puzzle as text (the levels) ------------------------------------------------------------------------------- */
static const char DIGIT[] = ".123456789abcdefghijklmnopqrstuvwxyz";

int nrk_encode(const nrk_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0 || len + n * n + 1 > size) return 0;
    for (int c = 0; c < n * n; c++) out[len++] = p->clue[c] < sizeof DIGIT - 1 ? DIGIT[p->clue[c]] : '?';
    out[len] = 0;
    return len;
}

int nrk_decode(const char *s, nrk_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 3 || n > NRK_MAX) return 0;
    p->n = n;
    for (int c = 0; c < n * n; c++, s++) {
        const char *d = *s ? strchr(DIGIT, *s) : NULL;
        if (!d) return 0;
        p->clue[c] = (uint8_t)(d - DIGIT);
    }
    return !*s;
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
typedef struct {
    int n, cells, count, cap;
    int16_t isle[NRK_CELLS];                    /* a land cell's island (-1: sea) */
    int size[NRK_CELLS], want[NRK_CELLS];
    int16_t at[NRK_CELLS];                      /* where its number is */
} isles;

/* the sea in one piece with c made land */
static int sea_whole(const isles *m, int c)
{
    int stack[NRK_CELLS], sp = 0, seas = 0, got = 0, first = -1, n = m->n;
    uint8_t seen[NRK_CELLS];
    memset(seen, 0, (size_t)m->cells);
    for (int x = 0; x < m->cells; x++)
        if (m->isle[x] < 0 && x != c) {
            seas++;
            if (first < 0) first = x;
        }
    if (!seas) return 0;
    seen[first] = 1;
    stack[sp++] = first;
    while (sp) {
        int x = stack[--sp], px = x % n, py = x / n;
        got++;
        const int nb[4] = {py > 0 ? x - n : -1, px > 0 ? x - 1 : -1, px < n - 1 ? x + 1 : -1, py < n - 1 ? x + n : -1};
        for (int j = 0; j < 4; j++) {
            int o = nb[j];
            if (o < 0 || seen[o] || o == c || m->isle[o] >= 0) continue;
            seen[o] = 1;
            stack[sp++] = o;
        }
    }
    return got == seas;
}

/* c made land: the island it joins (one only, or a new one: -2); -1: it cannot (two islands, too big, the sea cut) */
static int can_land(const isles *m, int c)
{
    int n = m->n, x = c % n, y = c / n, join = -2;
    if (m->isle[c] >= 0) return -1;
    const int nb[4] = {y > 0 ? c - n : -1, x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y < n - 1 ? c + n : -1};
    for (int j = 0; j < 4; j++) {
        int o = nb[j];
        if (o < 0 || m->isle[o] < 0) continue;
        if (join >= 0 && join != m->isle[o]) return -1;
        join = m->isle[o];
    }
    if (join >= 0 && m->size[join] >= m->cap) return -1;
    return sea_whole(m, c) ? join : -1;
}

static void land(isles *m, int c, int join, int want)
{
    if (join < 0) {                             /* a new island: in a place an old one left, or a new place */
        for (join = 0; join < m->count && m->size[join]; join++) {}
        if (join == m->count) m->count++;
        m->size[join] = 0;
        m->want[join] = want;
    }
    m->isle[c] = (int16_t)join;
    m->size[join]++;
}

#define MAKE_BUDGET 25000L                      /* the solver's passes a puzzle may cost (well under a second) */

static int cap_of(int n) { return n <= 6 ? 7 : 9; }

/* islands grown by chance: new ones and the old ones growing towards the size each was given, to about `part` of the
 * cells, then the sea's 2 x 2 pools filled in; each island's number in one of its cells (at). 0: stuck */
static int layout(isles *m, int n, uint32_t *s, int cap, int part)
{
    memset(m, 0, sizeof *m);
    m->n = n;
    m->cells = n * n;
    m->cap = cap;
    for (int c = 0; c < m->cells; c++) m->isle[c] = -1;
    int target = m->cells * part / 100, landed = 0;
    for (int tries = 0; tries < 30 * m->cells && landed < target; tries++) {
        int grow = m->count && pr_below(s, 100) < 75;
        if (grow) {
            int i = pr_below(s, m->count), opts[4 * NRK_CELLS], no = 0;
            if (m->size[i] >= m->want[i]) continue;
            for (int c = 0; c < m->cells; c++) {
                if (m->isle[c] != i) continue;
                int x = c % n, y = c / n;
                const int nb[4] = {y > 0 ? c - n : -1, x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1,
                                   y < n - 1 ? c + n : -1};
                for (int j = 0; j < 4; j++)
                    if (nb[j] >= 0 && m->isle[nb[j]] < 0) opts[no++] = nb[j];
            }
            if (!no) continue;
            int c = opts[pr_below(s, no)];
            if (can_land(m, c) != i) continue;
            land(m, c, i, 0);
            landed++;
        } else {
            int c = pr_below(s, m->cells);
            if (can_land(m, c) != -2) continue;
            int want = 1 + pr_below(s, cap) / 2 + pr_below(s, cap) / 2 + pr_below(s, 2);
            land(m, c, -1, want > cap ? cap : want);
            landed++;
        }
    }
    for (int round = 0; round < m->cells; round++) {  /* the pools: one of their cells made land */
        int pools[NRK_CELLS], np = 0;
        for (int y = 0; y + 1 < n; y++)
            for (int x = 0; x + 1 < n; x++) {
                int c = y * n + x;
                if (m->isle[c] < 0 && m->isle[c + 1] < 0 && m->isle[c + n] < 0 && m->isle[c + n + 1] < 0)
                    pools[np++] = c;
            }
        if (!np) break;
        int c = pools[pr_below(s, np)], sq[4] = {c, c + 1, c + n, c + n + 1}, done = 0;
        pr_shuffle(s, sq, 4);
        for (int i = 0; i < 4 && !done; i++) {
            int j = can_land(m, sq[i]);
            if (j == -1) continue;
            land(m, sq[i], j, 1);
            done = 1;
        }
        if (!done) return 0;
    }
    for (int i = 0; i < m->count; i++) {        /* each island's number: in one of its cells */
        int cells[NRK_CELLS], k = 0;
        for (int c = 0; c < m->cells; c++)
            if (m->isle[c] == i) cells[k++] = c;
        m->at[i] = (int16_t)cells[pr_below(s, k)];
    }
    return 1;
}

static void puzzle_of(const isles *m, nrk_puzzle *p, uint8_t *sol)
{
    memset(p, 0, sizeof *p);
    p->n = m->n;
    for (int c = 0; c < m->cells; c++) sol[c] = m->isle[c] < 0;
    for (int i = 0; i < m->count; i++)
        if (m->size[i]) p->clue[m->at[i]] = (uint8_t)m->size[i];
}

/* graded no further than cap, and where the reasoning got to (end) */
static int grade_end(const nrk_puzzle *p, long *work, int cap, bench_grid *end)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, work);
    r.budget = 6000;                            /* one grade's passes at most */
    grid_of(&k, end);
    return bench_grade(end, &r, cap);
}

/* c (land) made sea: its island kept in one piece (its number moved if it was there; an island of one cell gone), no
 * 2 x 2 pool made, the sea kept in one piece; 1: done */
static int to_sea(isles *m, int c, uint32_t *s)
{
    int i = m->isle[c], n = m->n, x = c % n, y = c / n, wet = 0;
    if (i < 0) return 0;
    const int nb[4] = {y > 0 ? c - n : -1, x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y < n - 1 ? c + n : -1};
    for (int j = 0; j < 4; j++) wet |= nb[j] >= 0 && m->isle[nb[j]] < 0;
    if (!wet) return 0;
    for (int dy = -1; dy <= 0; dy++)
        for (int dx = -1; dx <= 0; dx++) {
            int x0 = x + dx, y0 = y + dy, dry = 0;
            if (x0 < 0 || y0 < 0 || x0 + 1 >= n || y0 + 1 >= n) continue;
            for (int k = 0; k < 4; k++) {
                int o = (y0 + k / 2) * n + x0 + k % 2;
                dry |= o != c && m->isle[o] >= 0;
            }
            if (!dry) return 0;
        }
    int rest[NRK_CELLS], nr = 0;
    for (int o = 0; o < m->cells; o++)
        if (m->isle[o] == i && o != c) rest[nr++] = o;
    if (nr) {                                   /* the rest of the island still in one piece */
        int stack[NRK_CELLS], sp = 0, got = 0;
        uint8_t seen[NRK_CELLS];
        memset(seen, 0, (size_t)m->cells);
        seen[rest[0]] = 1;
        stack[sp++] = rest[0];
        while (sp) {
            int z = stack[--sp], zx = z % n, zy = z / n;
            got++;
            const int zb[4] = {zy > 0 ? z - n : -1, zx > 0 ? z - 1 : -1, zx < n - 1 ? z + 1 : -1,
                               zy < n - 1 ? z + n : -1};
            for (int j = 0; j < 4; j++) {
                int o = zb[j];
                if (o < 0 || o == c || seen[o] || m->isle[o] != i) continue;
                seen[o] = 1;
                stack[sp++] = o;
            }
        }
        if (got != nr) return 0;
        if (m->at[i] == c) m->at[i] = (int16_t)rest[pr_below(s, nr)];
    }
    m->isle[c] = -1;
    m->size[i]--;
    return 1;
}

/* a small change to the islands, most often where the reasoning got stuck (und: those cells): a cell of the sea made
 * land, a cell of land made sea, an island's number moved; 0: none made */
static int mutate(isles *m, uint32_t *s, const int *und, int nu)
{
    for (int tries = 0; tries < 30; tries++) {
        int c = nu && pr_below(s, 100) < 85 ? und[pr_below(s, nu)] : pr_below(s, m->cells);
        if (m->isle[c] < 0) {
            int j = can_land(m, c);
            if (j == -1) continue;
            land(m, c, j, 1);
            if (j < 0) m->at[m->isle[c]] = (int16_t)c;
            return 1;
        }
        int i = m->isle[c];
        if (m->at[i] != c && pr_below(s, 3) == 0) {
            m->at[i] = (int16_t)c;
            return 1;
        }
        if (to_sea(m, c, s)) return 1;
    }
    return 0;
}

/* how far the reasoning gets (cells decided) up to cap: *lv its grade, und the cells left (*nu) */
static int progress(const isles *m, long *work, int cap, int *lv, int *und, int *nu)
{
    nrk_puzzle q;
    uint8_t qs[NRK_CELLS];
    bench_grid end;
    puzzle_of(m, &q, qs);
    *lv = grade_end(&q, work, cap, &end);
    int got = 0;
    *nu = 0;
    for (int c = 0; c < m->cells; c++) {
        if (bench_one(end.d[c])) got++;
        else und[(*nu)++] = c;
    }
    return got;
}

/* a puzzle made: a sea and islands by chance, then changed a little at a time (the change kept when the reasoning
 * gets as far or further) until it is solved at the level (and by no harder way) */
void nrk_make_ex(int n, int level, uint32_t seed, long budget, nrk_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > NRK_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    static isles m, keep, best;
    int best_lv = -2;
    memset(&best, 0, sizeof best);
    best.n = n;
    best.cells = n * n;
    for (int c = 0; c < n * n; c++) best.isle[c] = -1;
    int climb = level < BENCH_HARD ? level : BENCH_MEDIUM;   /* hard: climbed by the medium ways, then tried */
    for (int attempt = 0; attempt < 500; attempt++) {
        if (!layout(&keep, n, &s, cap_of(n), 38 + 3 * level + pr_below(&s, 6))) continue;
        int und[NRK_CELLS], nu, lv, got = progress(&keep, &work, climb, &lv, und, &nu);
        for (int step = 0; step < 40 * n; step++) {
            int sol_lv = lv >= BENCH_EASY && lv <= climb ? lv : -1;
            if (sol_lv < 0 && level == BENCH_HARD && got * 5 >= 3 * keep.cells) {
                int u2[NRK_CELLS], n2;
                progress(&keep, &work, BENCH_HARD, &lv, u2, &n2);
                if (lv == BENCH_HARD) sol_lv = lv;
            }
            if (sol_lv >= 0) {
                if (sol_lv > best_lv) best_lv = sol_lv, best = keep;
                break;
            }
            m = keep;
            if (!mutate(&m, &s, und, nu)) break;
            int u3[NRK_CELLS], n3, lv3, got3 = progress(&m, &work, climb, &lv3, u3, &n3);
            if (got3 >= got || (lv3 >= BENCH_EASY && lv3 <= climb)) {
                keep = m;
                got = got3;
                lv = lv3;
                nu = n3;
                memcpy(und, u3, sizeof u3);
            }
            if (work >= 4 * budget && best_lv >= BENCH_EASY) break;
        }
        if (best_lv == level || (work >= 3 * budget && best_lv >= BENCH_EASY)) break;   /* past it: the nearest */
    }
    if (best_lv < BENCH_EASY && n > 5) {         /* never seen: a smaller one rather than none */
        nrk_make_ex(n - 1, level, seed + 1, budget, p, sol);
        return;
    }
    puzzle_of(&best, p, sol);
}

void nrk_make(int n, int level, uint32_t seed, nrk_puzzle *p, uint8_t *sol)
{
    nrk_make_ex(n, level, seed, MAKE_BUDGET, p, sol);
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
int nrk_load(nrk_game *g, const nrk_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = nrk_grade(p, g->sol);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void nrk_new(nrk_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= NRK_VARIANTS) variant = 0;
    nrk_puzzle p;
    uint8_t sol[NRK_CELLS];
    nrk_make(nrk_size(variant), nrk_level(variant), seed, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = nrk_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
}

int nrk_free(const nrk_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n && !g->p.clue[c]; }
int nrk_tap_state(const nrk_game *g, int c) { return g->st[c] == NRK_SEA ? NRK_EMPTY : NRK_SEA; }
int nrk_hold_state(const nrk_game *g, int c) { return g->st[c] == NRK_DOT ? NRK_EMPTY : NRK_DOT; }

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const nrk_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++)
        k += nrk_free(g, c) && g->st[c] != NRK_EMPTY && (g->st[c] == NRK_SEA) != g->sol[c];
    return k;
}

int nrk_paint(nrk_game *g, const int *cells, int n, int state)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (nrk_free(g, cells[i])) bench_set(&g->hist, g->st, cells[i], state);
    return bench_done(&g->hist, wrongs(g));
}

static int put(nrk_game *g, int c, int state) { return nrk_paint(g, &c, 1, state); }

static int hint_step(nrk_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong cell first */
        if (nrk_free(g, c) && g->st[c] != NRK_EMPTY && (g->st[c] == NRK_SEA) != g->sol[c])
            return put(g, c, g->sol[c] ? NRK_SEA : NRK_DOT) ? c : -1;
    ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int c = 0; c < cells; c++)
        if (nrk_free(g, c) && g->st[c] != NRK_EMPTY) d.d[c] = (uint16_t)(g->sol[c] ? SEA : LAND);
    int c = bench_next(&d, &r);
    if (c >= 0 && !nrk_free(g, c)) c = -1;
    for (int i = 0; i < cells && c < 0; i++)
        if (nrk_free(g, i) && g->st[i] == NRK_EMPTY && g->sol[i]) c = i;
    for (int i = 0; i < cells && c < 0; i++)
        if (nrk_free(g, i) && g->st[i] == NRK_EMPTY) c = i;
    if (c < 0) return -1;
    return put(g, c, g->sol[c] ? NRK_SEA : NRK_DOT) ? c : -1;
}

int nrk_hint(nrk_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int nrk_undo(nrk_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}

int nrk_pool(const nrk_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n;
    if (g->st[c] != NRK_SEA) return 0;
    for (int dy = -1; dy <= 0; dy++)
        for (int dx = -1; dx <= 0; dx++) {
            int x0 = x + dx, y0 = y + dy, all = 1;
            if (x0 < 0 || y0 < 0 || x0 + 1 >= n || y0 + 1 >= n) continue;
            for (int i = 0; i < 4; i++) all &= g->st[(y0 + i / 2) * n + x0 + i % 2] == NRK_SEA;
            if (all) return 1;
        }
    return 0;
}

/* the cells joined to c through cells for which ok(st, clue) holds: how many, and the numbers met (other than c's) */
static int region(const nrk_game *g, int c, int land_only, int *others)
{
    int n = g->p.n, stack[NRK_CELLS], sp = 0, size = 0;
    uint8_t seen[NRK_CELLS];
    memset(seen, 0, (size_t)n * n);
    *others = 0;
    seen[c] = 1;
    stack[sp++] = c;
    while (sp) {
        int x = stack[--sp], px = x % n, py = x / n;
        size++;
        if (x != c && g->p.clue[x]) ++*others;
        const int nb[4] = {py > 0 ? x - n : -1, px > 0 ? x - 1 : -1, px < n - 1 ? x + 1 : -1, py < n - 1 ? x + n : -1};
        for (int j = 0; j < 4; j++) {
            int o = nb[j];
            if (o < 0 || seen[o]) continue;
            int in = land_only ? g->p.clue[o] || g->st[o] == NRK_DOT : g->st[o] != NRK_SEA;
            if (!in) continue;
            seen[o] = 1;
            stack[sp++] = o;
        }
    }
    return size;
}

int nrk_island(const nrk_game *g, int c)
{
    int others, want = g->p.clue[c];
    if (!want) return 0;
    int size = region(g, c, 1, &others);
    if (others || size > want) return 1;
    size = region(g, c, 0, &others);
    return !others && size == want ? 2 : 0;
}

int nrk_solved(const nrk_game *g)
{
    ctx k;
    ctx_make(&k, &g->p);
    bench_grid d;
    memset(&d, 0, sizeof d);
    for (int c = 0; c < k.cells; c++) d.d[c] = (uint16_t)(g->st[c] == NRK_SEA && !g->p.clue[c] ? SEA : LAND);
    return valid(&d, &k);
}

int nrk_script(nrk_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (nrk_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (nrk_free(g, c) && (g->st[c] == NRK_SEA) != g->sol[c]) put(g, c, g->sol[c] ? NRK_SEA : NRK_EMPTY), k--;
            continue;
        }
        int hold = *s == '.', len = 0;
        if (hold) s++;
        int a = bench_cell(s, n, n, &len);
        if (!nrk_free(g, a)) return 0;
        s += len;
        int b = a;
        if (*s == '-') {
            b = bench_cell(s + 1, n, n, &len);
            if (b < 0 || (b % n != a % n && b / n != a / n)) return 0;
            s += 1 + len;
        }
        int was = g->st[a], to = hold ? nrk_hold_state(g, a) : nrk_tap_state(g, a), cells[NRK_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (nrk_free(g, c) && g->st[c] == was) cells[k++] = c;
            if (c == b) break;
        }
        if (!nrk_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
