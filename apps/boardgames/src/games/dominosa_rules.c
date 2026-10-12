/*
 * Dominosa: the rules (dominosa.h): the grader that reasons as a person does, the solver that counts solutions, the
 * maker, and the player's dominoes with undo. No SDL: tested by tests/dominosa_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/dominosa.h"

#include <string.h>

#include "games/puzrand.h"

#define EDGES (2 * DSA_CELLS)
#define PER_TYPE 48                             /* a domino's places on a grid, at most */

/* ---- the pairs of neighbouring cells of the puzzle in hand ------------------------------------------------------- */
typedef struct { uint8_t a, b, t; } pair;

static struct {
    int n, w, h, cells, ne, nt;
    pair e[EDGES];
    int ce[DSA_CELLS][4], nce[DSA_CELLS];       /* each cell's pairs */
    int te[DSA_TYPES][PER_TYPE], nte[DSA_TYPES];   /* each domino's pairs */
} T;

static void tables_load(const dsa_puz *p)
{
    int w = p->w, h = p->h;
    T.n = p->n, T.w = w, T.h = h, T.cells = w * h, T.ne = 0, T.nt = (p->n + 1) * (p->n + 2) / 2;
    memset(T.nce, 0, sizeof T.nce);
    memset(T.nte, 0, sizeof T.nte);
    for (int c = 0; c < w * h; c++)
        for (int d = 0; d < 2; d++) {
            int o = d ? c + w : c + 1;
            if (d ? c / w >= h - 1 : c % w >= w - 1) continue;
            int t = dsa_type(p->n, p->num[c], p->num[o]), k = T.ne++;
            T.e[k] = (pair){(uint8_t)c, (uint8_t)o, (uint8_t)t};
            T.ce[c][T.nce[c]++] = k;
            T.ce[o][T.nce[o]++] = k;
            if (T.nte[t] < PER_TYPE) T.te[t][T.nte[t]++] = k;
        }
}

/* what the grader knows: the pairs still possible, the dominoes found */
typedef struct {
    uint8_t alive[EDGES], fixed[EDGES];
    uint8_t cdone[DSA_CELLS], tdone[DSA_TYPES];
} state;

static void state_init(state *s)
{
    memset(s->alive, 1, (size_t)T.ne);
    memset(s->fixed, 0, (size_t)T.ne);
    memset(s->cdone, 0, (size_t)T.cells);
    memset(s->tdone, 0, (size_t)T.nt);
}

static void fix(state *s, int k)                /* the pair k is a domino: nothing else on its cells, nor of its kind */
{
    const pair *e = &T.e[k];
    s->fixed[k] = 1;
    s->cdone[e->a] = s->cdone[e->b] = 1;
    s->tdone[e->t] = 1;
    for (int i = 0; i < T.nce[e->a]; i++) if (T.ce[e->a][i] != k) s->alive[T.ce[e->a][i]] = 0;
    for (int i = 0; i < T.nce[e->b]; i++) if (T.ce[e->b][i] != k) s->alive[T.ce[e->b][i]] = 0;
    for (int i = 0; i < T.nte[e->t]; i++) if (T.te[e->t][i] != k) s->alive[T.te[e->t][i]] = 0;
}

static int alive_of_cell(const state *s, int c, int *one)
{
    int k = 0;
    for (int i = 0; i < T.nce[c]; i++)
        if (s->alive[T.ce[c][i]]) k++, *one = T.ce[c][i];
    return k;
}

static int alive_of_type(const state *s, int t, int *one)
{
    int k = 0;
    for (int i = 0; i < T.nte[t]; i++)
        if (s->alive[T.te[t][i]]) k++, *one = T.te[t][i];
    return k;
}

static int broken(const state *s)               /* a cell or a domino with no place left */
{
    int one;
    for (int c = 0; c < T.cells; c++)
        if (!alive_of_cell(s, c, &one)) return 1;
    for (int t = 0; t < T.nt; t++)
        if (!alive_of_type(s, t, &one)) return 1;
    return 0;
}

/* ---- easy: a cell with one pair left; a domino with one place left ----------------------------------------------- */
static int easy(state *s, int *cell)
{
    int one;
    for (int c = 0; c < T.cells; c++)
        if (!s->cdone[c] && alive_of_cell(s, c, &one) == 1) {
            if (cell) *cell = c;
            fix(s, one);
            return 1;
        }
    for (int t = 0; t < T.nt; t++)
        if (!s->tdone[t] && alive_of_type(s, t, &one) == 1) {
            if (cell) *cell = T.e[one].a;
            fix(s, one);
            return 1;
        }
    return 0;
}

static int settle(state *s)                     /* the easy ways until nothing moves; 0: broken */
{
    for (;;) {
        if (broken(s)) return 0;
        if (!easy(s, NULL)) return 1;
    }
}

/* ---- medium ------------------------------------------------------------------------------------------------------ */
static int touches(int k, int c) { return T.e[k].a == c || T.e[k].b == c; }

/* a cell whose pairs are all one domino holds it: that domino is nowhere else */
static int one_kind(state *s)
{
    for (int c = 0; c < T.cells; c++) {
        if (s->cdone[c]) continue;
        int t = -1, same = 1, k = 0;
        for (int i = 0; i < T.nce[c]; i++) {
            int e = T.ce[c][i];
            if (!s->alive[e]) continue;
            k++;
            if (t >= 0 && T.e[e].t != t) same = 0;
            t = T.e[e].t;
        }
        if (k < 2 || !same) continue;
        int ch = 0;
        for (int i = 0; i < T.nte[t]; i++) {
            int e = T.te[t][i];
            if (s->alive[e] && !touches(e, c)) s->alive[e] = 0, ch = 1;
        }
        if (ch) return 1;
    }
    return 0;
}

/* a domino whose places all share a cell lies on it: the cell pairs with nothing else */
static int one_cell(state *s)
{
    for (int t = 0; t < T.nt; t++) {
        if (s->tdone[t]) continue;
        int ca = -1, cb = -1, k = 0;            /* the cells shared so far (at most two) */
        for (int i = 0; i < T.nte[t]; i++) {
            int e = T.te[t][i];
            if (!s->alive[e]) continue;
            if (!k++) {
                ca = T.e[e].a, cb = T.e[e].b;
                continue;
            }
            if (!touches(e, ca)) ca = -1;
            if (!touches(e, cb)) cb = -1;
        }
        if (k < 2) continue;
        int c = ca >= 0 ? ca : cb, ch = 0;
        if (c < 0) continue;
        for (int i = 0; i < T.nce[c]; i++) {
            int e = T.ce[c][i];
            if (s->alive[e] && T.e[e].t != t) s->alive[e] = 0, ch = 1;
        }
        if (ch) return 1;
    }
    return 0;
}

/* a pair that would leave a neighbouring cell with nothing to pair with is not a domino */
static int isolating(state *s)
{
    for (int k = 0; k < T.ne; k++) {
        if (!s->alive[k] || s->fixed[k]) continue;
        int ends[2] = {T.e[k].a, T.e[k].b};
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < T.nce[ends[j]]; i++) {
                const pair *f = &T.e[T.ce[ends[j]][i]];
                int d = f->a == ends[j] ? f->b : f->a;   /* a neighbour */
                if (d == ends[1 - j] || s->cdone[d]) continue;
                int free_one = 0;
                for (int m = 0; m < T.nce[d] && !free_one; m++) {
                    int g = T.ce[d][m];
                    free_one = s->alive[g] && !touches(g, ends[0]) && !touches(g, ends[1]) && T.e[g].t != T.e[k].t;
                }
                if (!free_one) {
                    s->alive[k] = 0;
                    return 1;
                }
            }
    }
    return 0;
}

/* ---- hard: a pair that, followed through with the easy ways, comes to a contradiction (the cells with fewest pairs
 * left first) ------------------------------------------------------------------------------------------------------ */
static int contradiction(state *s)
{
    static state t;
    for (int want = 2; want <= 4; want++)
        for (int c = 0; c < T.cells; c++) {
            int one;
            if (s->cdone[c] || alive_of_cell(s, c, &one) != want) continue;
            for (int i = 0; i < T.nce[c]; i++) {
                int k = T.ce[c][i];
                if (!s->alive[k]) continue;
                t = *s;
                fix(&t, k);
                if (!settle(&t)) {
                    s->alive[k] = 0;
                    return 1;
                }
            }
        }
    return 0;
}

/* one step of the easiest way that works, up to cap: its level; -1: stuck */
static int step(state *s, int cap, int *cell)
{
    if (easy(s, cell)) return DSA_EASY;
    if (cap < DSA_MEDIUM) return -1;
    if (one_kind(s) || one_cell(s) || isolating(s)) return DSA_MEDIUM;
    if (cap < DSA_HARD) return -1;
    if (contradiction(s)) return DSA_HARD;
    return -1;
}

static state S;                                 /* the grader's last state */
static int left_after;                          /* the cells it left unsettled when it got stuck (0: solved) */

static int rate_cap(const dsa_puz *p, int cap)
{
    tables_load(p);
    state_init(&S);
    int level = DSA_EASY;
    for (;;) {
        left_after = 0;
        for (int c = 0; c < T.cells; c++) left_after += !S.cdone[c];
        if (broken(&S)) return DSA_BEYOND;
        if (!left_after) return level;
        int l = step(&S, cap, NULL);
        if (l < 0) return DSA_BEYOND;
        if (l > level) level = l;
    }
}

int dsa_rate(const dsa_puz *p) { return rate_cap(p, DSA_HARD); }

/* ---- counting solutions: the easy ways, then each pair of the cell with the fewest ------------------------------- */
typedef struct { int count, limit; uint8_t *sol; } counter;

static void search(const state *s0, counter *k)
{
    state s = *s0;
    if (!settle(&s)) return;
    int best = -1, bn = 99, one;
    for (int c = 0; c < T.cells; c++) {
        if (s.cdone[c]) continue;
        int a = alive_of_cell(&s, c, &one);
        if (a < bn) bn = a, best = c;
    }
    if (best < 0) {
        if (!k->count++ && k->sol)
            for (int e = 0; e < T.ne; e++)
                if (s.fixed[e]) k->sol[T.e[e].a] = T.e[e].b, k->sol[T.e[e].b] = T.e[e].a;
        return;
    }
    for (int i = 0; i < T.nce[best] && k->count < k->limit; i++) {
        int e = T.ce[best][i];
        if (!s.alive[e]) continue;
        state t = s;
        fix(&t, e);
        search(&t, k);
    }
}

int dsa_count(const dsa_puz *p, int limit, uint8_t *sol)
{
    tables_load(p);
    static state s;
    state_init(&s);
    counter k = {0, limit, sol};
    search(&s, &k);
    return k.count;
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
/* the grid tiled: rows of dominoes lying down, then the two dominoes of a 2 x 2 square turned round, many times */
static void tile(int w, int h, uint8_t *mate, uint32_t *rng)
{
    for (int c = 0; c < w * h; c++) mate[c] = (uint8_t)(c % 2 ? c - 1 : c + 1);
    for (int i = 0; i < 60 * w * h; i++) {
        int x = pr_below(rng, w - 1), y = pr_below(rng, h - 1), c = y * w + x;
        if (mate[c] == c + 1 && mate[c + w] == c + w + 1) {          /* two lying: standing */
            mate[c] = (uint8_t)(c + w), mate[c + w] = (uint8_t)c;
            mate[c + 1] = (uint8_t)(c + w + 1), mate[c + w + 1] = (uint8_t)(c + 1);
        } else if (mate[c] == c + w && mate[c + 1] == c + w + 1) {   /* two standing: lying */
            mate[c] = (uint8_t)(c + 1), mate[c + 1] = (uint8_t)c;
            mate[c + w] = (uint8_t)(c + w + 1), mate[c + w + 1] = (uint8_t)(c + w);
        }
    }
}

/* how far a puzzle is from its level (lower is better): 0 it needs that level's ways; 1000 and more: the grader gets
 * stuck at that level (plus the cells left); 3000: easier ways do */
static int score(const dsa_puz *p, int level, uint8_t *focus)
{
    if (rate_cap(p, level) == DSA_BEYOND) {
        for (int c = 0; c < T.cells; c++) focus[c] = !S.cdone[c];
        return 1000 + left_after;
    }
    memset(focus, 0, DSA_CELLS);
    if (level == DSA_EASY || rate_cap(p, level - 1) == DSA_BEYOND) return 0;
    return 3000;
}

/* a domino turned round, or two swapped (most often one on a cell the grader could not settle) */
static void nudge(dsa_puz *p, const uint8_t *mate, const uint8_t *focus, uint32_t *rng)
{
    int cells = p->w * p->h, a = pr_below(rng, cells);
    if (pr_below(rng, 4)) {
        int nf = 0, pick;
        for (int c = 0; c < cells; c++) nf += focus[c];
        if (nf) {
            pick = pr_below(rng, nf);
            for (int c = 0; c < cells; c++)
                if (focus[c] && !pick--) { a = c; break; }
        }
    }
    int a2 = mate[a];
    if (pr_below(rng, 3) == 0) {                /* turned round */
        uint8_t t = p->num[a];
        p->num[a] = p->num[a2], p->num[a2] = t;
        return;
    }
    int b = pr_below(rng, cells), b2 = mate[b];
    if (b == a || b == a2) return;
    uint8_t x = p->num[a], y = p->num[a2];      /* swapped */
    p->num[a] = p->num[b], p->num[a2] = p->num[b2];
    p->num[b] = x, p->num[b2] = y;
}

void dsa_make(int n, int level, uint32_t seed, dsa_puz *out, uint8_t *sol)
{
    if (n != 4 && n != 6 && n != 8) n = 6;
    if (level < DSA_EASY || level > DSA_HARD) level = DSA_EASY;
    uint32_t s = pr_seed(seed);
    int w = n + 2, h = n + 1, nt = (n + 1) * (n + 2) / 2, best_score = 1 << 30;
    dsa_puz p, q, best;
    uint8_t mate[DSA_CELLS], best_mate[DSA_CELLS], focus[DSA_CELLS], fq[DSA_CELLS];
    p.n = n, p.w = w, p.h = h;
    int lo[DSA_TYPES], hi[DSA_TYPES];
    for (int a = 0; a <= n; a++)
        for (int b = a; b <= n; b++) lo[dsa_type(n, a, b)] = a, hi[dsa_type(n, a, b)] = b;
    for (int attempt = 0; attempt < 400 && (attempt < 20 || best_score >= 1000); attempt++) {
        tile(w, h, mate, &s);
        int deal[DSA_TYPES], k = 0;
        for (int t = 0; t < nt; t++) deal[t] = t;
        pr_shuffle(&s, deal, nt);
        for (int c = 0; c < w * h; c++) {       /* the set dealt onto the dominoes, each turned at random */
            if (mate[c] < c) continue;
            int t = deal[k++], turn = pr_below(&s, 2);
            p.num[c] = (uint8_t)(turn ? hi[t] : lo[t]);
            p.num[mate[c]] = (uint8_t)(turn ? lo[t] : hi[t]);
        }
        int cur = score(&p, level, focus);
        for (int tweak = 0; tweak < 200 && cur; tweak++) {
            q = p;
            nudge(&q, mate, focus, &s);
            int sc = score(&q, level, fq);
            if (sc <= cur) {
                p = q;
                cur = sc;
                memcpy(focus, fq, sizeof focus);
            }
        }
        if (cur < best_score) {
            best_score = cur;
            best = p;
            memcpy(best_mate, mate, sizeof mate);
        }
        if (!cur) break;
    }
    *out = best;
    memcpy(sol, best_mate, DSA_CELLS);
}

/* ---- the player's dominoes --------------------------------------------------------------------------------------- */
void dsa_load(dsa_game *g, int level, const dsa_puz *p, const uint8_t *sol)
{
    memset(g, 0, sizeof *g);
    g->level = level;
    g->p = *p;
    memcpy(g->sol, sol, DSA_CELLS);
}

void dsa_new(dsa_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= DSA_VARIANTS) variant = 0;
    dsa_puz p;
    uint8_t sol[DSA_CELLS];
    dsa_make(dsa_set_of(variant), dsa_level_of(variant), seed, &p, sol);
    dsa_load(g, dsa_level_of(variant), &p, sol);
    g->seed = seed;
}

static int cells_of(const dsa_game *g) { return g->p.w * g->p.h; }

static void set_link(dsa_game *g, int c, int v)
{
    if (g->link[c] == v) return;
    lg2_log(&g->hist, c, g->link[c]);
    g->link[c] = (uint8_t)v;
}

static void unlink_cell(dsa_game *g, int c)
{
    if (!g->link[c]) return;
    int o = g->link[c] - 1;
    set_link(g, o, 0);
    set_link(g, c, 0);
}

static int neighbours(const dsa_game *g, int a, int b)
{
    int w = g->p.w;
    return (a / w == b / w && (a - b == 1 || b - a == 1)) || a - b == w || b - a == w;
}

int dsa_lay(dsa_game *g, int a, int b)
{
    if (a < 0 || b < 0 || a >= cells_of(g) || b >= cells_of(g) || !neighbours(g, a, b)) return 0;
    if (g->link[a] == b + 1) return 0;
    lg2_begin(&g->hist, 6);
    unlink_cell(g, a);
    unlink_cell(g, b);
    set_link(g, a, b + 1);
    set_link(g, b, a + 1);
    return lg2_end(&g->hist);
}

int dsa_lift(dsa_game *g, int c)
{
    if (c < 0 || c >= cells_of(g) || !g->link[c]) return 0;
    lg2_begin(&g->hist, 2);
    unlink_cell(g, c);
    return lg2_end(&g->hist);
}

int dsa_hint(dsa_game *g)
{
    int cells = cells_of(g);
    for (int c = 0; c < cells; c++)             /* a wrong domino first */
        if (g->link[c] && g->link[c] - 1 != g->sol[c]) return dsa_lift(g, c) ? c : -1;
    tables_load(&g->p);
    static state s;
    state_init(&s);
    for (int k = 0; k < T.ne; k++)              /* the right dominoes laid: theirs for sure */
        if (g->link[T.e[k].a] == T.e[k].b + 1 && s.alive[k]) fix(&s, k);
    for (int guard = 0; guard < 4 * DSA_CELLS; guard++) {   /* the easiest ways until a new domino is sure */
        for (int k = 0; k < T.ne; k++)
            if (s.fixed[k] && !g->link[T.e[k].a]) {
                int a = T.e[k].a;
                return dsa_lay(g, a, g->sol[a]) ? a : -1;
            }
        if (broken(&s) || step(&s, DSA_HARD, NULL) < 0) break;
    }
    for (int c = 0; c < cells; c++)             /* stuck: the first domino not laid */
        if (!g->link[c]) return dsa_lay(g, c, g->sol[c]) ? c : -1;
    return -1;
}

static void put_back(void *ctx, int cell, uint32_t was) { ((dsa_game *)ctx)->link[cell] = (uint8_t)was; }

int dsa_undo(dsa_game *g) { return lg2_undo(&g->hist, put_back, g); }

static int type_at(const dsa_game *g, int c)    /* the domino on the cell (-1: none) */
{
    return g->link[c] ? dsa_type(g->p.n, g->p.num[c], g->p.num[g->link[c] - 1]) : -1;
}

int dsa_laid(const dsa_game *g, int t)
{
    int k = 0;
    for (int c = 0; c < cells_of(g); c++)
        if (g->link[c] && g->link[c] - 1 > c && type_at(g, c) == t) k++;
    return k;
}

int dsa_clash(const dsa_game *g, int c)
{
    if (c < 0 || c >= cells_of(g) || !g->link[c]) return 0;
    return dsa_laid(g, type_at(g, c)) > 1;
}

int dsa_solved(const dsa_game *g)
{
    uint8_t seen[DSA_TYPES] = {0};
    for (int c = 0; c < cells_of(g); c++) {
        if (!g->link[c]) return 0;
        int o = g->link[c] - 1;
        if (o < c) continue;
        int t = type_at(g, c);
        if (seen[t]++) return 0;
    }
    return 1;
}

int dsa_script(dsa_game *g, const char *s)
{
    int w = g->p.w, h = g->p.h;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (dsa_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < w * h && k > 0; c++)
                if (g->link[c] - 1 != g->sol[c]) { dsa_lay(g, c, g->sol[c]); k--; }
            continue;
        }
        int len, len2;
        if (*s == 'x') {
            int c = lg2_cell(s + 1, w, h, &len);
            if (c < 0 || !dsa_lift(g, c)) return 0;
            s += 1 + len;
            continue;
        }
        int a = lg2_cell(s, w, h, &len);
        if (a < 0 || s[len] != '-') return 0;
        int b = lg2_cell(s + len + 1, w, h, &len2);
        if (b < 0 || !dsa_lay(g, a, b)) return 0;
        s += len + 1 + len2;
    }
    return 1;
}
