/*
 * Tents: the rules (tents.h): the ways of reasoning the solver grades with, the maker, the player's field and undo.
 * No SDL: tested by tests/tents_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/tents.h"

#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"

#define OFF 1u                                  /* a cell's values: bit 0 grass, bit 1 a tent */
#define ON 2u
#define BOTH 3u

static const int SIZES[TN_SIZES] = {6, 8, 10, 12};

int tn_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % TN_SIZES]; }
int tn_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

/* ---- what the solver knows of a field: each cell's trees, each tree's cells, the cells round each cell ----------- */
typedef struct {
    const tn_puzzle *p;
    int n, cells, ntrees;
    uint8_t trees[TN_CELLS];
    uint8_t ntree[TN_CELLS], tree_of[TN_CELLS][4];   /* a cell's trees (above, below, left, right) */
    uint8_t nspot[TN_CELLS], spot[TN_CELLS][4];      /* a tree's cells (not trees) */
    uint8_t nround[TN_CELLS], round[TN_CELLS][8];    /* the cells touching a cell, corners too */
} ctx;

static void ctx_make(ctx *k, const tn_puzzle *p)
{
    int n = p->n;
    k->p = p;
    k->n = n;
    k->cells = n * n;
    k->ntrees = 0;
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        k->ntree[c] = k->nspot[c] = k->nround[c] = 0;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, ny = y + dy, o = ny * n + nx;
                if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
                k->round[c][k->nround[c]++] = (uint8_t)o;
                if (dx && dy) continue;
                if (p->tree[o] && !p->tree[c]) k->tree_of[c][k->ntree[c]++] = (uint8_t)o;
                if (p->tree[c] && !p->tree[o]) k->spot[c][k->nspot[c]++] = (uint8_t)o;
            }
        if (p->tree[c]) k->trees[k->ntrees++] = (uint8_t)c;
    }
}

static void grid_of(const ctx *k, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < k->cells; c++) g->d[c] = (uint8_t)(k->p->tree[c] ? OFF : BOTH);
}

static int at(int n, int way, int i, int j) { return way ? j * n + i : i * n + j; }

static int has_tent(const ctx *k, const bench_grid *g, int t)
{
    for (int j = 0; j < k->nspot[t]; j++)
        if (g->d[k->spot[t][j]] == ON) return 1;
    return 0;
}

/* the trees surely taken: by a tent with no other tree beside it (1 each); -1 if two tents need the same one */
static int owned(const ctx *k, const bench_grid *g, uint8_t *own)
{
    memset(own, 0, (size_t)k->cells);
    for (int c = 0; c < k->cells; c++) {
        if (g->d[c] != ON || k->p->tree[c]) continue;
        if (!k->ntree[c]) return -1;
        if (k->ntree[c] > 1) continue;
        if (own[k->tree_of[c][0]]++) return -1;
    }
    return 0;
}

/* easy */
static int easy(bench_grid *g, const ctx *k)
{
    int n = k->n, ch = 0, tents = 0;
    for (int c = 0; c < k->cells; c++) {
        if (g->d[c] == BOTH && !k->ntree[c]) g->d[c] = OFF, ch = 1;
        if (g->d[c] != ON || k->p->tree[c]) continue;
        tents++;
        for (int j = 0; j < k->nround[c]; j++) {
            int o = k->round[c][j];
            if (g->d[o] == ON && !k->p->tree[o]) return -1;
            if (g->d[o] == BOTH) g->d[o] = OFF, ch = 1;
        }
    }
    if (tents > k->ntrees) return -1;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            int want = k->p->count[way][i], on = 0, free_ = 0;
            if (want < 0) continue;
            for (int j = 0; j < n; j++) {
                uint8_t d = g->d[at(n, way, i, j)];
                on += d == ON && !k->p->tree[at(n, way, i, j)];
                free_ += d == BOTH;
            }
            if (on > want || on + free_ < want) return -1;
            if (!free_ || (on != want && on + free_ != want)) continue;
            for (int j = 0; j < n; j++)
                if (g->d[at(n, way, i, j)] == BOTH) g->d[at(n, way, i, j)] = (uint8_t)(on == want ? OFF : ON);
            ch = 1;
        }
    for (int i = 0; i < k->ntrees; i++) {
        int t = k->trees[i], left = 0, last = -1;
        if (has_tent(k, g, t)) continue;
        for (int j = 0; j < k->nspot[t]; j++)
            if (g->d[k->spot[t][j]] == BOTH) left++, last = k->spot[t][j];
        if (!left) return -1;
        if (left == 1) g->d[last] = ON, ch = 1;
    }
    uint8_t own[TN_CELLS];
    if (owned(k, g, own) < 0) return -1;
    for (int c = 0; c < k->cells; c++) {
        if (g->d[c] != BOTH) continue;
        int free_tree = 0;
        for (int j = 0; j < k->ntree[c]; j++) free_tree |= !own[k->tree_of[c][j]];
        if (!free_tree) g->d[c] = OFF, ch = 1;
    }
    return ch;
}

/* a line's tents and the most it could still take: stretches of free cells (a tent in every other one); the cells
 * marked in skip (== mark) count as grass, `tent` as a tent */
static void room(const ctx *k, const bench_grid *g, int way, int i, const int *skip, int mark, int tent, int *on,
                 int *most)
{
    int n = k->n, run = 0;
    *on = *most = 0;
    for (int j = 0; j <= n; j++) {
        int c = j < n ? at(n, way, i, j) : -1, d = c < 0 ? 0 : g->d[c];
        if (c >= 0 && k->p->tree[c]) d = OFF;
        if (c == tent) d = ON;
        else if (c >= 0 && d == BOTH && skip[c] == mark) d = OFF;
        if (d == BOTH) { run++; continue; }
        *most += (run + 1) / 2;
        run = 0;
        *on += d == ON;
    }
}

/* medium */
static int medium(bench_grid *g, const ctx *k)
{
    static int mark[TN_CELLS], serial;
    int n = k->n;
    int none = ++serial;                        /* no cell marked */
    for (int way = 0; way < 2; way++)           /* the stretches: just room */
        for (int i = 0; i < n; i++) {
            int want = k->p->count[way][i], on, most;
            if (want < 0) continue;
            room(k, g, way, i, mark, none, -1, &on, &most);
            if (most < want - on) return -1;
            if (most != want - on || most == 0) continue;
            int ch = 0;
            for (int j = 0; j < n;) {
                if (g->d[at(n, way, i, j)] != BOTH) { j++; continue; }
                int len = 0;
                while (j + len < n && g->d[at(n, way, i, j + len)] == BOTH) len++;
                if (len % 2)
                    for (int q = 0; q < len; q++) g->d[at(n, way, i, j + q)] = (uint8_t)(q % 2 ? OFF : ON), ch = 1;
                else if (len == 2)
                    for (int side = -1; side <= 1; side += 2)
                        for (int q = 0; q < 2; q++) {
                            int o = i + side;
                            if (o < 0 || o >= n) continue;
                            int c = at(n, way, o, j + q);
                            if (g->d[c] == BOTH) g->d[c] = OFF, ch = 1;
                        }
                j += len;
            }
            if (ch) return 1;
        }
    uint8_t own[TN_CELLS];
    if (owned(k, g, own) < 0) return -1;
    for (int x = 0; x < k->cells; x++) {        /* a tent tried, the cells round it followed */
        if (g->d[x] != BOTH) continue;
        int id = ++serial, bad = 0;
        for (int j = 0; j < k->nround[x]; j++) mark[k->round[x][j]] = id;
        if (k->ntree[x] == 1 && own[k->tree_of[x][0]]) bad = 1;
        for (int j = 0; j < k->nround[x] && !bad; j++) {   /* the trees beside the cells round it */
            int o = k->round[x][j];
            for (int q = 0; q < k->ntree[o] && !bad; q++) {
                int t = k->tree_of[o][q], left = 0;
                if (has_tent(k, g, t)) continue;
                for (int s = 0; s < k->nspot[t]; s++) {
                    int c = k->spot[t][s];
                    left += c == x || (g->d[c] == BOTH && mark[c] != id);
                }
                if (!left) bad = 1;
            }
        }
        for (int way = 0; way < 2 && !bad; way++) /* its lines and those beside them */
            for (int i = (way ? x % n : x / n) - 1; i <= (way ? x % n : x / n) + 1 && !bad; i++) {
                int want, on, most;
                if (i < 0 || i >= n || (want = k->p->count[way][i]) < 0) continue;
                room(k, g, way, i, mark, id, x, &on, &most);
                if (on > want || most < want - on) bad = 1;
            }
        if (bad) {
            g->d[x] = OFF;
            return 1;
        }
    }
    return 0;
}

static long finds[2];                           /* each tier's finds (tn_effort counts them) */

static int deduce(bench_grid *g, int tier, void *c)
{
    int res = tier ? medium(g, (const ctx *)c) : easy(g, (const ctx *)c);
    if (res > 0) finds[tier ? 1 : 0]++;
    return res;
}


/* every tree its own tent beside it: a matching of trees and tents (each tree tries its cells in turn) */
static int match_tree(const ctx *k, const bench_grid *g, int t, uint8_t *seen, int *tree_at)
{
    for (int j = 0; j < k->nspot[t]; j++) {
        int c = k->spot[t][j];
        if (g->d[c] != ON || seen[c]) continue;
        seen[c] = 1;
        if (tree_at[c] < 0 || match_tree(k, g, tree_at[c], seen, tree_at)) {
            tree_at[c] = t;
            return 1;
        }
    }
    return 0;
}

static int matched(const ctx *k, const bench_grid *g)
{
    int tree_at[TN_CELLS];
    for (int c = 0; c < k->cells; c++) tree_at[c] = -1;
    for (int i = 0; i < k->ntrees; i++) {
        uint8_t seen[TN_CELLS];
        memset(seen, 0, (size_t)k->cells);
        if (!match_tree(k, g, k->trees[i], seen, tree_at)) return 0;
    }
    return 1;
}

static int valid(const bench_grid *g, void *cv)
{
    const ctx *k = cv;
    int n = k->n, tents = 0;
    for (int c = 0; c < k->cells; c++) {
        if (g->d[c] != ON || k->p->tree[c]) continue;
        tents++;
        for (int j = 0; j < k->nround[c]; j++)
            if (g->d[k->round[c][j]] == ON && !k->p->tree[k->round[c][j]]) return 0;
    }
    if (tents != k->ntrees) return 0;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            int on = 0;
            for (int j = 0; j < n; j++) on += g->d[at(n, way, i, j)] == ON && !k->p->tree[at(n, way, i, j)];
            if (k->p->count[way][i] >= 0 && on != k->p->count[way][i]) return 0;
        }
    return matched(k, g);
}

static bench_rules rules_of(ctx *k, long *work)
{
    return (bench_rules){.vars = k->cells, .tiers = 2, .probe_tiers = 1,
                        .deduce = deduce, .valid = valid, .ctx = k, .work = work};
}

static int grade_work(const tn_puzzle *p, uint8_t *sol, long *work, int cap)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, work);
    bench_grid g;
    grid_of(&k, &g);
    int lv = bench_grade(&g, &r, cap);
    if (sol && lv >= BENCH_EASY && lv <= BENCH_HARD)
        for (int c = 0; c < k.cells; c++) sol[c] = g.d[c] == ON && !p->tree[c];
    return lv;
}

int tn_grade(const tn_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD); }

int tn_count(const tn_puzzle *p, int limit)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid g;
    grid_of(&k, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

int tn_effort(const tn_puzzle *p, long *passes, long *mediums)
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

/* ---- a field as text (the levels) -------------------------------------------------------------------------------- */
static const char DIGIT[] = "0123456789abc";

int tn_encode(const tn_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0 || len + 2 * n + n * n + 1 > size) return 0;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) out[len++] = p->count[way][i] < 0 ? '-' : DIGIT[p->count[way][i] % 13];
    for (int c = 0; c < n * n; c++) out[len++] = p->tree[c] ? 'T' : '.';
    out[len] = 0;
    return len;
}

int tn_decode(const char *s, tn_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 4 || n > TN_MAX) return 0;
    p->n = n;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++, s++) {
            const char *d = *s ? strchr(DIGIT, *s) : NULL;
            if (*s != '-' && !d) return 0;
            p->count[way][i] = (int8_t)(*s == '-' ? -1 : d - DIGIT);
        }
    for (int c = 0; c < n * n; c++, s++) {
        if (*s != 'T' && *s != '.') return 0;
        p->tree[c] = *s == 'T';
    }
    return !*s;
}

/* ---- making a field ---------------------------------------------------------------------------------------------- */
/* tents where they touch none, each with a tree beside it, in a random order, about one cell in five */
static void layout(int n, uint32_t *s, tn_puzzle *p, uint8_t *sol)
{
    int cells = n * n, want = cells / 5, tents = 0, order[TN_CELLS];
    memset(p, 0, sizeof *p);
    memset(sol, 0, (size_t)cells);
    p->n = n;
    for (int c = 0; c < cells; c++) order[c] = c;
    pr_shuffle(s, order, cells);
    for (int i = 0; i < cells && tents < want; i++) {
        int c = order[i], x = c % n, y = c / n, ok = !p->tree[c], spots[4], ns = 0;
        for (int dy = -1; dy <= 1 && ok; dy++)
            for (int dx = -1; dx <= 1 && ok; dx++) {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && ny >= 0 && nx < n && ny < n && sol[ny * n + nx]) ok = 0;
            }
        if (!ok) continue;
        static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx >= 0 && ny >= 0 && nx < n && ny < n && !p->tree[ny * n + nx] && !sol[ny * n + nx])
                spots[ns++] = ny * n + nx;
        }
        if (!ns) continue;
        sol[c] = 1;
        p->tree[spots[pr_below(s, ns)]] = 1;
        tents++;
    }
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            int on = 0;
            for (int j = 0; j < n; j++) on += sol[at(n, way, i, j)];
            p->count[way][i] = (int8_t)on;
        }
}

#define MAKE_BUDGET 60000L                      /* the solver's passes a field may cost (well under a second) */

void tn_make(int n, int level, uint32_t seed, tn_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > TN_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    tn_puzzle best, q;
    uint8_t best_sol[TN_CELLS], qs[TN_CELLS];
    int best_lv = -2;
    for (int attempt = 0; attempt < 5000; attempt++) {
        layout(n, &s, &q, qs);
        int lv = grade_work(&q, NULL, &work, level);
        if (lv < BENCH_EASY || lv > level) continue;
        if (lv > best_lv) {
            best_lv = lv;
            best = q;
            memcpy(best_sol, qs, sizeof best_sol);
        }
        if (best_lv == level || (work >= 3 * MAKE_BUDGET && best_lv >= BENCH_EASY)) break;   /* past it: the nearest */
    }
    *p = best;
    memcpy(sol, best_sol, (size_t)n * n);
}

/* ---- the player's field ------------------------------------------------------------------------------------------ */
int tn_load(tn_game *g, const tn_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = tn_grade(p, g->sol);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void tn_new(tn_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= TN_VARIANTS) variant = 0;
    tn_puzzle p;
    uint8_t sol[TN_CELLS];
    tn_make(tn_size(variant), tn_level(variant), seed, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = tn_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
}

int tn_free(const tn_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n && !g->p.tree[c]; }
int tn_tap_state(const tn_game *g, int c) { return g->st[c] == TN_TENT ? TN_EMPTY : TN_TENT; }
int tn_hold_state(const tn_game *g, int c) { return g->st[c] == TN_GRASS ? TN_EMPTY : TN_GRASS; }

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const tn_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++)
        k += tn_free(g, c) && g->st[c] != TN_EMPTY && (g->st[c] == TN_TENT) != g->sol[c];
    return k;
}

int tn_paint(tn_game *g, const int *cells, int n, int state)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (tn_free(g, cells[i])) bench_set(&g->hist, g->st, cells[i], state);
    return bench_done(&g->hist, wrongs(g));
}

static int put(tn_game *g, int c, int state) { return tn_paint(g, &c, 1, state); }

static int hint_step(tn_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong cell first */
        if (tn_free(g, c) && g->st[c] != TN_EMPTY && (g->st[c] == TN_TENT) != g->sol[c])
            return put(g, c, g->sol[c] ? TN_TENT : TN_GRASS) ? c : -1;
    ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int c = 0; c < cells; c++)
        if (tn_free(g, c) && g->st[c] != TN_EMPTY) d.d[c] = (uint8_t)(g->sol[c] ? ON : OFF);
    int c = bench_next(&d, &r);
    for (int i = 0; i < cells && c < 0; i++)
        if (tn_free(g, i) && g->st[i] == TN_EMPTY && g->sol[i]) c = i;
    if (c < 0) return -1;
    return put(g, c, g->sol[c] ? TN_TENT : TN_GRASS) ? c : -1;
}

int tn_hint(tn_game *g)
{
    return bench_hinted(&g->hist, hint_step(g));
}

int tn_undo(tn_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}


int tn_touching(const tn_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n;
    if (g->st[c] != TN_TENT) return 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
            if ((dx || dy) && nx >= 0 && ny >= 0 && nx < n && ny < n && g->st[ny * n + nx] == TN_TENT) return 1;
        }
    return 0;
}

int tn_line_tents(const tn_game *g, int way, int i)
{
    int k = 0;
    for (int j = 0; j < g->p.n; j++) k += g->st[at(g->p.n, way, i, j)] == TN_TENT;
    return k;
}

int tn_tree_lost(const tn_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n, room_ = 0;
    if (!g->p.tree[c]) return 0;
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    for (int d = 0; d < 4; d++) {
        int nx = x + DX[d], ny = y + DY[d];
        if (nx < 0 || ny < 0 || nx >= n || ny >= n || g->p.tree[ny * n + nx]) continue;
        int st = g->st[ny * n + nx];
        if (st == TN_TENT) return 0;
        room_ += st == TN_EMPTY;
    }
    return !room_;
}

int tn_solved(const tn_game *g)
{
    ctx k;
    ctx_make(&k, &g->p);
    bench_grid d;
    memset(&d, 0, sizeof d);
    for (int c = 0; c < k.cells; c++) d.d[c] = (uint8_t)(g->st[c] == TN_TENT && !g->p.tree[c] ? ON : OFF);
    if (!valid(&d, &k)) return 0;
    for (int way = 0; way < 2; way++)           /* the counts not told too (the one solution's) */
        for (int i = 0; i < k.n; i++) {
            int want = 0;
            for (int j = 0; j < k.n; j++) want += g->sol[at(k.n, way, i, j)];
            if (tn_line_tents(g, way, i) != want) return 0;
        }
    return 1;
}

int tn_script(tn_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (tn_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (tn_free(g, c) && (g->st[c] == TN_TENT) != g->sol[c]) put(g, c, g->sol[c] ? TN_TENT : TN_EMPTY), k--;
            continue;
        }
        int hold = *s == '.', len = 0;
        if (hold) s++;
        int a = bench_cell(s, n, n, &len);
        if (!tn_free(g, a)) return 0;
        s += len;
        int b = a;
        if (*s == '-') {
            b = bench_cell(s + 1, n, n, &len);
            if (b < 0 || (b % n != a % n && b / n != a / n)) return 0;
            s += 1 + len;
        }
        int was = g->st[a], to = hold || b != a ? tn_hold_state(g, a) : tn_tap_state(g, a), cells[TN_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (tn_free(g, c) && g->st[c] == was) cells[k++] = c;
            if (c == b) break;
        }
        if (!tn_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
