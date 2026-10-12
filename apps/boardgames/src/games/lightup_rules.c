/*
 * Light Up: the rules (lightup.h): the ways of reasoning the solver grades with, the maker, the player's cells and
 * undo. No SDL: tested by tests/lightup_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/lightup.h"

#include <string.h>

#include "games/puzrand.h"

#define OFF 1u                                  /* a cell's values: bit 0 no lamp, bit 1 a lamp */
#define ON 2u
#define BOTH 3u

static const int SIZES[LU_SIZES] = {7, 10, 14};

int lu_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % LU_SIZES]; }
int lu_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

/* ---- what the solver knows of a puzzle: the cells each white cell sees, the walls' neighbours ------------------- */
typedef struct {
    const lu_puzzle *p;
    int n, cells;
    uint8_t nsee[LU_CELLS], see[LU_CELLS][2 * (LU_MAX - 1)];
    uint8_t nnb[LU_CELLS], nb[LU_CELLS][4];
    int nnum;
    uint8_t num[LU_CELLS];                      /* the numbered walls */
    int nsegs;                                  /* the stretches of white cells: a row's, a column's */
    uint16_t seg[LU_CELLS][2];                  /* a white cell's stretches (its row's, its column's) */
    uint8_t seglen[2 * LU_CELLS], segcell[2 * LU_CELLS][LU_MAX];
} ctx;

static int white(const lu_puzzle *p, int c) { return p->wall[c] == LU_WHITE; }

static void ctx_make(ctx *k, const lu_puzzle *p)
{
    int n = p->n;
    k->p = p;
    k->n = n;
    k->cells = n * n;
    k->nnum = 0;
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        k->nsee[c] = k->nnb[c] = 0;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
            k->nb[c][k->nnb[c]++] = (uint8_t)(ny * n + nx);
            if (!white(p, c)) continue;
            for (; nx >= 0 && ny >= 0 && nx < n && ny < n && white(p, ny * n + nx); nx += DX[d], ny += DY[d])
                k->see[c][k->nsee[c]++] = (uint8_t)(ny * n + nx);
        }
        if (p->wall[c] >= 0) k->num[k->nnum++] = (uint8_t)c;
    }
    k->nsegs = 0;
    for (int w = 0; w < 2; w++)
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++) {
                int c = w ? j * n + i : i * n + j;
                if (!white(p, c)) continue;
                if (!j || !white(p, w ? (j - 1) * n + i : i * n + j - 1)) k->seglen[k->nsegs++] = 0;
                int s = k->nsegs - 1;
                k->seg[c][w] = (uint16_t)s;
                k->segcell[s][k->seglen[s]++] = (uint8_t)c;
            }
}

static void grid_of(const ctx *k, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < k->cells; c++) g->d[c] = (uint8_t)(white(k->p, c) ? BOTH : OFF);
}

static int lit(const ctx *k, const bench_grid *g, int c)
{
    if (g->d[c] == ON) return 1;
    for (int i = 0; i < k->nsee[c]; i++)
        if (g->d[k->see[c][i]] == ON) return 1;
    return 0;
}

/* the lamps and the free cells of every stretch of white cells (a row's or a column's, between walls) */
static void tally(const ctx *k, const bench_grid *g, uint8_t *lamps, uint8_t *free_)
{
    memset(lamps, 0, (size_t)k->nsegs);
    memset(free_, 0, (size_t)k->nsegs);
    for (int c = 0; c < k->cells; c++) {
        if (!white(k->p, c)) continue;
        for (int w = 0; w < 2; w++) {
            lamps[k->seg[c][w]] += g->d[c] == ON;
            free_[k->seg[c][w]] += g->d[c] == BOTH;
        }
    }
}

/* easy: a lamp's lines; the numbers' counts; a dark cell with one cell left to light it */
static int easy(bench_grid *g, const ctx *k)
{
    int ch = 0;
    uint8_t lamps[2 * LU_CELLS], free_[2 * LU_CELLS];
    tally(k, g, lamps, free_);
    for (int s = 0; s < k->nsegs; s++) {
        if (lamps[s] > 1) return -1;
        if (!lamps[s] || !free_[s]) continue;
        for (int i = 0; i < k->seglen[s]; i++)
            if (g->d[k->segcell[s][i]] == BOTH) g->d[k->segcell[s][i]] = OFF;
        ch = 1;
    }
    for (int i = 0; i < k->nnum; i++) {
        int w = k->num[i], want = k->p->wall[w], on = 0, nfree = 0;
        for (int j = 0; j < k->nnb[w]; j++) {
            int c = k->nb[w][j];
            if (!white(k->p, c)) continue;
            on += g->d[c] == ON;
            nfree += g->d[c] == BOTH;
        }
        if (on > want || on + nfree < want) return -1;
        if (!nfree || (on != want && on + nfree != want)) continue;
        for (int j = 0; j < k->nnb[w]; j++) {
            int c = k->nb[w][j];
            if (white(k->p, c) && g->d[c] == BOTH) g->d[c] = (uint8_t)(on == want ? OFF : ON);
        }
        ch = 1;
    }
    tally(k, g, lamps, free_);
    for (int c = 0; c < k->cells; c++) {        /* (a lamp put here counts at once: no cell is lit twice) */
        if (!white(k->p, c) || g->d[c] == ON) continue;
        int r = k->seg[c][0], col = k->seg[c][1];
        if (lamps[r] || lamps[col]) continue;
        int nsrc = free_[r] + free_[col] - (g->d[c] == BOTH);
        if (!nsrc) return -1;
        if (nsrc > 1) continue;
        int src = g->d[c] == BOTH ? c : -1;
        for (int w = 0; w < 2 && src < 0; w++)
            for (int i = 0; i < k->seglen[k->seg[c][w]]; i++)
                if (g->d[k->segcell[k->seg[c][w]][i]] == BOTH) src = k->segcell[k->seg[c][w]][i];
        g->d[src] = ON;
        for (int w = 0; w < 2; w++) lamps[k->seg[src][w]]++, free_[k->seg[src][w]]--;
        ch = 1;
    }
    return ch;
}

/* medium: a lamp tried in a free cell, only its own light followed (the cells it sees can hold no lamp): a number
 * left short of room, or a dark cell it does not light left with no lamp to light it, rules the cell out */
static int medium(bench_grid *g, const ctx *k)
{
    static uint8_t is_lit[LU_CELLS], src[LU_CELLS];
    static int seen_by[LU_CELLS], killed[LU_CELLS], stamp[LU_CELLS], dec[LU_CELLS], serial;
    for (int c = 0; c < k->cells; c++) {
        if (!white(k->p, c)) continue;
        is_lit[c] = (uint8_t)lit(k, g, c);
        src[c] = (uint8_t)((g->d[c] & ON) != 0);
        for (int i = 0; i < k->nsee[c]; i++) src[c] += (g->d[k->see[c][i]] & ON) != 0;
    }
    for (int x = 0; x < k->cells; x++) {
        if (g->d[x] != BOTH) continue;
        int id = ++serial, bad = 0;
        seen_by[x] = id;
        for (int i = 0; i < k->nsee[x]; i++) {
            int s = k->see[x][i];
            seen_by[s] = id;
            if (g->d[s] == BOTH) killed[s] = id;
        }
        for (int i = 0; i < k->nsee[x] && !bad; i++) {   /* the dark cells */
            int s = k->see[x][i];
            if (killed[s] != id) continue;
            for (int j = -1; j < k->nsee[s] && !bad; j++) {
                int u = j < 0 ? s : k->see[s][j];
                if (is_lit[u] || seen_by[u] == id) continue;
                if (stamp[u] != id) stamp[u] = id, dec[u] = 0;
                if (++dec[u] >= src[u]) bad = 1;
            }
        }
        for (int i = -1; i < k->nsee[x] && !bad; i++) {  /* the numbers next to the cells that change */
            int s = i < 0 ? x : k->see[x][i];
            if (s != x && killed[s] != id) continue;
            for (int j = 0; j < k->nnb[s] && !bad; j++) {
                int w = k->nb[s][j], want = k->p->wall[w], on = 0, free_ = 0;
                if (want < 0) continue;
                for (int q = 0; q < k->nnb[w]; q++) {
                    int c = k->nb[w][q];
                    if (!white(k->p, c)) continue;
                    if (c == x || g->d[c] == ON) on++;
                    else if (g->d[c] == BOTH && killed[c] != id) free_++;
                }
                if (on > want || on + free_ < want) bad = 1;
            }
        }
        if (bad) {
            g->d[x] = OFF;
            return 1;
        }
    }
    return 0;
}

static int deduce(bench_grid *g, int tier, void *c)
{
    return tier ? medium(g, (const ctx *)c) : easy(g, (const ctx *)c);
}

static int valid(const bench_grid *g, void *c)
{
    const ctx *k = c;
    for (int i = 0; i < k->cells; i++) {
        if (!white(k->p, i)) continue;
        if (!lit(k, g, i)) return 0;
        if (g->d[i] == ON)
            for (int j = 0; j < k->nsee[i]; j++)
                if (g->d[k->see[i][j]] == ON) return 0;
    }
    for (int i = 0; i < k->nnum; i++) {
        int w = k->num[i], on = 0;
        for (int j = 0; j < k->nnb[w]; j++) on += white(k->p, k->nb[w][j]) && g->d[k->nb[w][j]] == ON;
        if (on != k->p->wall[w]) return 0;
    }
    return 1;
}

static bench_rules rules_of(ctx *k, long *work)
{
    return (bench_rules){.vars = k->cells, .tiers = 2, .probe_tiers = 1,
                        .deduce = deduce, .valid = valid, .ctx = k, .work = work};
}

/* the grade, no further than the cap; the solution in sol (may be NULL), where the reasoning ended in end (may be
 * NULL) */
static int grade_work(const lu_puzzle *p, uint8_t *sol, long *work, int cap, bench_grid *end)
{
    static ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, work);
    bench_grid g;
    grid_of(&k, &g);
    int lv = bench_grade(&g, &r, cap);
    if (sol && lv >= BENCH_EASY && lv <= BENCH_HARD)
        for (int c = 0; c < k.cells; c++) sol[c] = g.d[c] == ON;
    if (end) *end = g;
    return lv;
}

int lu_grade(const lu_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD, NULL); }

int lu_count(const lu_puzzle *p, int limit)
{
    static ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid g;
    grid_of(&k, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
/* the lamps: those of sol that are still in white cells kept, more put where it is still dark (in a random order);
 * every wall numbered */
static void light(lu_puzzle *p, uint32_t *s, uint8_t *sol)
{
    static ctx k;
    int cells = p->n * p->n, order[LU_CELLS];
    ctx_make(&k, p);
    bench_grid g;
    grid_of(&k, &g);
    for (int c = 0; c < cells; c++)
        if (white(p, c) && sol[c]) g.d[c] = ON;
    for (int c = 0; c < cells; c++) order[c] = c;
    pr_shuffle(s, order, cells);
    for (int i = 0; i < cells; i++) {
        int c = order[i];
        if (white(p, c) && !lit(&k, &g, c)) g.d[c] = ON;
    }
    for (int c = 0; c < cells; c++) sol[c] = g.d[c] == ON;
    for (int c = 0; c < cells; c++) {
        if (white(p, c)) continue;
        int on = 0;
        for (int j = 0; j < k.nnb[c]; j++) on += sol[k.nb[c][j]];
        p->wall[c] = (int8_t)on;
    }
}

/* a cell made a wall, and its twin turned half round (a lamp there goes; the dark it leaves lit again) */
static void wall_in(lu_puzzle *p, uint32_t *s, uint8_t *sol, int c)
{
    int t = p->n * p->n - 1 - c;
    p->wall[c] = p->wall[t] = LU_WALL;
    sol[c] = sol[t] = 0;
    light(p, s, sol);
}

/* walls by chance (each with its twin turned half round), lamps where it is still dark */
static void layout(int n, uint32_t *s, lu_puzzle *p, uint8_t *sol)
{
    p->n = n;
    int cells = n * n, want = (int)(cells * (n <= 7 ? 0.22f : 0.2f) + 0.5f), walls = 0;
    for (int c = 0; c < cells; c++) p->wall[c] = LU_WHITE, sol[c] = 0;
    while (walls < want) {
        int c = pr_below(s, cells), t = cells - 1 - c;
        if (p->wall[c] != LU_WHITE) continue;
        p->wall[c] = p->wall[t] = LU_WALL;
        walls += c == t ? 1 : 2;
    }
    light(p, s, sol);
}

#define MAKE_BUDGET 60000L                      /* the solver's passes a puzzle may cost (well under a second) */
#define MAKE_LIMIT (3 * MAKE_BUDGET)            /* none at the level yet: more tries, up to this */

typedef struct {
    lu_puzzle *q;
    int8_t num[LU_CELLS];                       /* the walls' numbers */
    long *work;
} maker;

static int maker_grade(void *u, int cap)
{
    maker *m = u;
    return grade_work(m->q, NULL, m->work, cap, NULL);
}

static void maker_clue(void *u, int c, int on)
{
    maker *m = u;
    m->q->wall[c] = on ? m->num[c] : LU_WALL;
}

void lu_make(int n, int level, uint32_t seed, lu_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > LU_MAX) n = 7;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    lu_puzzle best, q;
    uint8_t best_sol[LU_CELLS], qs[LU_CELLS];
    int best_lv = -2;
    maker m = {&q, {0}, &work};
    bench_maker mk = {&m, maker_grade, maker_clue};
    for (int attempt = 0; attempt < 3000; attempt++) {
        layout(n, &s, &q, qs);                  /* every wall numbered: solvable within the level? */
        bench_grid end;
        int lv = grade_work(&q, NULL, &work, level, &end);
        for (int fix = 0; fix < n && lv > level; fix++) {   /* stuck: a wall where it is stuck (no lamp) */
            int stuck[LU_CELLS], ns = 0;
            for (int c = 0; c < n * n; c++)
                if (end.d[c] == BOTH && !qs[c]) stuck[ns++] = c;
            if (!ns) break;
            wall_in(&q, &s, qs, stuck[pr_below(&s, ns)]);
            lv = grade_work(&q, NULL, &work, level, &end);
        }
        if (lv < BENCH_EASY || lv > level) continue;
        int order[LU_CELLS], nw = 0;
        for (int c = 0; c < n * n; c++)
            if (q.wall[c] >= 0) order[nw++] = c;
        memcpy(m.num, q.wall, sizeof m.num);
        pr_shuffle(&s, order, nw);
        lv = bench_reduce(&mk, order, nw, level, &work, MAKE_LIMIT);
        if (lv > best_lv && lv <= level) {
            best_lv = lv;
            best = q;
            memcpy(best_sol, qs, sizeof best_sol);
        }
        if (best_lv == level || (work >= MAKE_LIMIT && best_lv >= BENCH_EASY)) break;
    }
    *p = best;
    memcpy(sol, best_sol, (size_t)n * n);
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
void lu_load(lu_game *g, const lu_puzzle *p, int level)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = level;
    lu_grade(p, g->sol);
}

void lu_new(lu_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= LU_VARIANTS) variant = 0;
    lu_puzzle p;
    uint8_t sol[LU_CELLS];
    lu_make(lu_size(variant), lu_level(variant), seed, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = lu_level(variant);
    memcpy(g->sol, sol, sizeof sol);
    g->seed = seed;
}

int lu_white(const lu_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n && white(&g->p, c); }

int lu_tap_state(const lu_game *g, int c) { return g->st[c] == LU_LAMP ? LU_EMPTY : LU_LAMP; }
int lu_hold_state(const lu_game *g, int c) { return g->st[c] == LU_DOT ? LU_EMPTY : LU_DOT; }

/* the wrong marks on the board: a lamp the solution has not, a dot where it has one (the stars' mistakes) */
static int wrongs(const lu_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++)
        k += white(&g->p, c) && g->st[c] != LU_EMPTY && (g->st[c] == LU_LAMP) != g->sol[c];
    return k;
}

int lu_paint(lu_game *g, const int *cells, int n, int state)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (lu_white(g, cells[i])) bench_set(&g->hist, g->st, cells[i], state);
    return bench_done(&g->hist, wrongs(g));
}

static int put(lu_game *g, int c, int state) { return lu_paint(g, &c, 1, state); }

static int hint_step(lu_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong cell first */
        if (white(&g->p, c) && (g->st[c] == LU_LAMP) != g->sol[c] && g->st[c] != LU_EMPTY)
            return put(g, c, g->sol[c] ? LU_LAMP : LU_DOT) ? c : -1;
    static ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int c = 0; c < cells; c++)
        if (white(&g->p, c) && g->st[c] != LU_EMPTY) d.d[c] = (uint8_t)(g->sol[c] ? ON : OFF);
    int c = bench_next(&d, &r);
    if (c < 0)
        for (int i = 0; i < cells && c < 0; i++)
            if (white(&g->p, i) && g->st[i] == LU_EMPTY) c = i;
    if (c < 0) return -1;
    return put(g, c, g->sol[c] ? LU_LAMP : LU_DOT) ? c : -1;
}

int lu_hint(lu_game *g)
{
    return bench_hinted(&g->hist, hint_step(g));
}

int lu_undo(lu_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}


static int lamp(const lu_game *g, int x, int y) { return g->st[y * g->p.n + x] == LU_LAMP; }

/* the lamps a white cell sees along its lines (itself apart) */
static int lamps_seen(const lu_game *g, int c)
{
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    int n = g->p.n, k = 0;
    for (int d = 0; d < 4; d++)
        for (int x = c % n + DX[d], y = c / n + DY[d]; x >= 0 && y >= 0 && x < n && y < n; x += DX[d], y += DY[d]) {
            if (!white(&g->p, y * n + x)) break;
            k += lamp(g, x, y);
        }
    return k;
}

int lu_lit(const lu_game *g, int c) { return white(&g->p, c) && (g->st[c] == LU_LAMP || lamps_seen(g, c) > 0); }
int lu_clash(const lu_game *g, int c) { return white(&g->p, c) && g->st[c] == LU_LAMP && lamps_seen(g, c) > 0; }

int lu_lamps_round(const lu_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n;
    return (x > 0 && lamp(g, x - 1, y)) + (x < n - 1 && lamp(g, x + 1, y)) + (y > 0 && lamp(g, x, y - 1)) +
           (y < n - 1 && lamp(g, x, y + 1));
}

int lu_solved(const lu_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++) {
        int bad = white(&g->p, c) ? !lu_lit(g, c) || lu_clash(g, c)
                                  : g->p.wall[c] >= 0 && lu_lamps_round(g, c) != g->p.wall[c];
        if (bad) return 0;
    }
    return 1;
}

int lu_script(lu_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (lu_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (white(&g->p, c) && (g->st[c] == LU_LAMP) != g->sol[c]) {
                    put(g, c, g->sol[c] ? LU_LAMP : LU_EMPTY);
                    k--;
                }
            continue;
        }
        int hold = *s == '.', len = 0;
        if (hold) s++;
        int a = bench_cell(s, n, n, &len);
        if (a < 0 || !white(&g->p, a)) return 0;
        s += len;
        int b = a;
        if (*s == '-') {
            b = bench_cell(s + 1, n, n, &len);
            if (b < 0 || (b % n != a % n && b / n != a / n)) return 0;
            s += 1 + len;
        }
        int was = g->st[a], to = hold || b != a ? lu_hold_state(g, a) : lu_tap_state(g, a), cells[LU_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (white(&g->p, c) && g->st[c] == was) cells[k++] = c;
            if (c == b) break;
        }
        if (!lu_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
