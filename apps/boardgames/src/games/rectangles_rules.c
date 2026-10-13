/*
 * Rectangles: the rules (rectangles.h): each number's possible rectangles, the grader that reasons on them as a
 * person does, the solver that counts solutions, the maker, and the player's rectangles with undo. No SDL: tested by
 * tests/rectangles_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/rectangles.h"

#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"

#define MAXCAND 12000

/* ---- sets of cells (256 bits) ------------------------------------------------------------------------------------ */
typedef struct { uint64_t m[4]; } set;

static int s_meet(const set *a, const set *b)
{
    return ((a->m[0] & b->m[0]) | (a->m[1] & b->m[1]) | (a->m[2] & b->m[2]) | (a->m[3] & b->m[3])) != 0;
}
static void s_add(set *a, int c) { a->m[c >> 6] |= 1ull << (c & 63); }
static int s_empty(const set *a) { return !(a->m[0] | a->m[1] | a->m[2] | a->m[3]); }
static int s_eq(const set *a, const set *b) { return !memcmp(a, b, sizeof *a); }
static int s_within(const set *a, const set *b)   /* a inside b */
{
    for (int i = 0; i < 4; i++)
        if (a->m[i] & ~b->m[i]) return 0;
    return 1;
}

/* ---- the numbers and their possible rectangles ------------------------------------------------------------------ */
typedef struct { set cells; uint8_t x, y, w, h; } cand;

typedef struct {
    int n, nclue, nc;
    int at[RCT_CLUES], area[RCT_CLUES];         /* each number's cell and value */
    int first[RCT_CLUES + 1];                   /* its rectangles: cand[first[i]] .. cand[first[i + 1] - 1] */
    set all;                                    /* the grid's cells */
    cand c[MAXCAND];
} board;

typedef struct {
    uint8_t alive[MAXCAND];
    int left[RCT_CLUES];
} state;

static board B;                                 /* one puzzle at a time (the maker, the grader, the hint) */

static set rect_cells(int n, int x, int y, int w, int h)
{
    set s = {{0}};
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) s_add(&s, j * n + i);
    return s;
}

/* 0: too many numbers or rectangles */
static int board_load(board *b, const rct_puz *p)
{
    int n = p->n;
    b->n = n;
    b->nclue = b->nc = 0;
    b->all = rect_cells(n, 0, 0, n, n);
    set clues = {{0}};
    for (int c = 0; c < n * n; c++)
        if (p->clue[c]) {
            if (b->nclue >= RCT_CLUES) return 0;
            b->at[b->nclue] = c;
            b->area[b->nclue++] = p->clue[c];
            s_add(&clues, c);
        }
    for (int i = 0; i < b->nclue; i++) {
        b->first[i] = b->nc;
        int cx = b->at[i] % n, cy = b->at[i] / n, a = b->area[i];
        for (int w = 1; w <= a && w <= n; w++) {
            if (a % w || a / w > n) continue;
            int h = a / w;
            for (int y = cy - h + 1; y <= cy; y++)
                for (int x = cx - w + 1; x <= cx; x++) {
                    if (x < 0 || y < 0 || x + w > n || y + h > n) continue;
                    set s = rect_cells(n, x, y, w, h), other = clues;
                    other.m[b->at[i] >> 6] &= ~(1ull << (b->at[i] & 63));
                    if (s_meet(&s, &other)) continue;   /* another number inside */
                    if (b->nc >= MAXCAND) return 0;
                    b->c[b->nc++] = (cand){s, (uint8_t)x, (uint8_t)y, (uint8_t)w, (uint8_t)h};
                }
        }
    }
    b->first[b->nclue] = b->nc;
    return 1;
}

static void state_init(const board *b, state *s)
{
    memset(s->alive, 1, (size_t)b->nc);
    for (int i = 0; i < b->nclue; i++) s->left[i] = b->first[i + 1] - b->first[i];
}

static void kill(state *s, int k, int i)
{
    if (!s->alive[k]) return;
    s->alive[k] = 0;
    s->left[i]--;
}

static void keep_only(const board *b, state *s, int i, int k)   /* number i's rectangle is k */
{
    for (int j = b->first[i]; j < b->first[i + 1]; j++)
        if (j != k) kill(s, j, i);
}

static int done(const board *b, const state *s)
{
    for (int i = 0; i < b->nclue; i++)
        if (s->left[i] != 1) return 0;
    return 1;
}

/* ---- what each number reaches (U: the cells some rectangle of it covers) and is sure of (I: the cells all of them
 * cover); 0: broken (a number with no rectangle left, a cell nobody can cover, two numbers sure of a cell) ---------- */
static set U[RCT_CLUES], I[RCT_CLUES], twice, owned;

static int look(const board *b, const state *s)
{
    set once = {{0}};
    memset(&twice, 0, sizeof twice);
    memset(&owned, 0, sizeof owned);
    int ok = 1;
    for (int i = 0; i < b->nclue; i++) {
        if (!s->left[i]) return 0;
        set u = {{0}}, in = b->all;
        for (int k = b->first[i]; k < b->first[i + 1]; k++)
            if (s->alive[k])
                for (int w = 0; w < 4; w++) u.m[w] |= b->c[k].cells.m[w], in.m[w] &= b->c[k].cells.m[w];
        U[i] = u;
        I[i] = in;
        for (int w = 0; w < 4; w++) {
            twice.m[w] |= once.m[w] & u.m[w];
            once.m[w] |= u.m[w];
            if (owned.m[w] & in.m[w]) ok = 0;
            owned.m[w] |= in.m[w];
        }
    }
    return ok && s_eq(&once, &b->all);
}

/* easy: the cells a number is sure of are its own, so no other number's rectangle covers them (a number with one
 * rectangle left is sure of all of it). 1 if anything changed; *broken as look() */
static int overlap(const board *b, state *s, int *broken)
{
    *broken = !look(b, s);
    if (*broken) return 0;
    int ch = 0;
    for (int i = 0; i < b->nclue; i++) {
        set theirs;                             /* the cells other numbers are sure of */
        for (int w = 0; w < 4; w++) theirs.m[w] = owned.m[w] & ~I[i].m[w];
        for (int k = b->first[i]; k < b->first[i + 1]; k++)
            if (s->alive[k] && s_meet(&b->c[k].cells, &theirs)) kill(s, k, i), ch = 1;
    }
    return ch;
}

/* medium: a cell only one number reaches is covered by that number's rectangle */
static int only_reach(const board *b, state *s, int *broken)
{
    *broken = !look(b, s);
    if (*broken) return 0;
    int ch = 0;
    for (int i = 0; i < b->nclue; i++) {
        set only;
        for (int w = 0; w < 4; w++) only.m[w] = U[i].m[w] & ~twice.m[w];
        if (s_empty(&only)) continue;
        for (int k = b->first[i]; k < b->first[i + 1]; k++)
            if (s->alive[k] && !s_within(&only, &b->c[k].cells)) kill(s, k, i), ch = 1;
    }
    return ch;
}

/* medium: a rectangle that would leave another number with no rectangle is not the one (after look()) */
static int blocking(const board *b, state *s)
{
    for (int i = 0; i < b->nclue; i++) {
        if (s->left[i] < 2) continue;
        for (int k = b->first[i]; k < b->first[i + 1]; k++) {
            if (!s->alive[k]) continue;
            const set *r = &b->c[k].cells;
            for (int j = 0; j < b->nclue; j++) {
                if (j == i || !s_meet(&U[j], r)) continue;
                int free_one = 0;
                for (int m = b->first[j]; m < b->first[j + 1] && !free_one; m++)
                    free_one = s->alive[m] && !s_meet(&b->c[m].cells, r);
                if (!free_one) {
                    kill(s, k, i);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int settle(const board *b, state *s)     /* overlaps and lone reaches until nothing moves; 0: broken */
{
    int broken;
    for (;;) {
        if (overlap(b, s, &broken)) continue;
        if (broken) return 0;
        if (only_reach(b, s, &broken)) continue;
        return !broken;
    }
}

/* hard: a rectangle that would leave some cell with no way to be covered is not the one */
static int starving(const board *b, state *s)
{
    for (int i = 0; i < b->nclue; i++) {
        if (s->left[i] < 2) continue;
        for (int k = b->first[i]; k < b->first[i + 1]; k++) {
            if (!s->alive[k]) continue;
            const set *r = &b->c[k].cells;
            set cover = *r;
            for (int j = 0; j < b->nclue; j++) {
                if (j == i) continue;
                for (int m = b->first[j]; m < b->first[j + 1]; m++)
                    if (s->alive[m] && !s_meet(&b->c[m].cells, r))
                        for (int w = 0; w < 4; w++) cover.m[w] |= b->c[m].cells.m[w];
            }
            if (!s_eq(&cover, &b->all)) {
                kill(s, k, i);
                return 1;
            }
        }
    }
    return 0;
}

/* hard: a rectangle that, followed through with the easier ways, comes to a contradiction */
static int contradiction(const board *b, state *s)
{
    static state t;
    for (int want = 2; want <= 4; want++)       /* the numbers with fewest rectangles first (four at most: a person tries a
                                                 * few ways, not dozens) */
        for (int i = 0; i < b->nclue; i++) {
            if (s->left[i] != want) continue;
            for (int k = b->first[i]; k < b->first[i + 1]; k++) {
                if (!s->alive[k]) continue;
                memcpy(t.alive, s->alive, (size_t)b->nc);
                memcpy(t.left, s->left, sizeof t.left[0] * (size_t)b->nclue);
                keep_only(b, &t, i, k);
                if (!settle(b, &t)) {
                    kill(s, k, i);
                    return 1;
                }
            }
        }
    return 0;
}

/* one step of the easiest way that works, up to cap: its level; -1: stuck; -2: broken */
static int step(const board *b, state *s, int cap)
{
    int broken;
    if (overlap(b, s, &broken)) return RCT_EASY;
    if (broken) return -2;
    if (cap < RCT_MEDIUM) return -1;
    if (only_reach(b, s, &broken) || blocking(b, s)) return RCT_MEDIUM;
    if (broken) return -2;
    if (cap < RCT_HARD) return -1;
    if (starving(b, s) || contradiction(b, s)) return RCT_HARD;
    return -1;
}

static state S;                                /* the grader's last state (the maker reads where it got stuck) */
static int effort;                              /* its steps, weighed: 1 an easy one, 3 a medium one, 10 a hard one */

static int rate_cap(const rct_puz *p, int cap)
{
    if (!board_load(&B, p)) return RCT_BEYOND;
    state_init(&B, &S);
    int level = RCT_EASY;
    effort = 0;
    for (;;) {
        int l = step(&B, &S, cap);
        if (l == -2) return RCT_BEYOND;
        effort += l == RCT_EASY ? 1 : l == RCT_MEDIUM ? 3 : l == RCT_HARD ? 10 : 0;
        if (l == -1) return done(&B, &S) ? level : RCT_BEYOND;
        if (l > level) level = l;
    }
}

int rct_rate(const rct_puz *p) { return rate_cap(p, RCT_HARD); }

int rct_effort(const rct_puz *p, int *steps)
{
    int r = rate_cap(p, RCT_HARD);
    if (steps) *steps = effort;
    return r;
}

/* ---- the levels' code: "<n>:" and a character a cell, row by row: '.' none, '1'..'9', then 'a' 10, 'b' 11... ---- */
int rct_encode(const rct_puz *p, char *out, int size)
{
    int n = p->n, k = snprintf(out, (size_t)size, "%d:", n);
    if (k < 0 || k + n * n + 1 > size) return 0;
    for (int c = 0; c < n * n; c++) {
        int v = p->clue[c];
        out[k++] = (char)(!v ? '.' : v < 10 ? '0' + v : 'a' + v - 10);
    }
    out[k] = 0;
    return 1;
}

int rct_decode(const char *s, rct_puz *p)
{
    int n = 0, total = 0;
    while (*s >= '0' && *s <= '9') n = n * 10 + *s++ - '0';
    if (*s++ != ':' || n < 2 || n > RCT_MAX) return 0;
    memset(p, 0, sizeof *p);
    p->n = n;
    for (int c = 0; c < n * n; c++, s++) {
        int v = *s == '.' ? 0 : *s >= '1' && *s <= '9' ? *s - '0' : -1;
        if (*s >= 'a' && *s <= 'z') v = *s - 'a' + 10;
        if (v < 0) return 0;
        p->clue[c] = (uint8_t)v;
        total += v;
    }
    return !*s && total == n * n;
}

/* ---- counting solutions: the easy ways, then each rectangle of the number with the fewest ------------------------ */
typedef struct { int count, limit; uint32_t *sol; } counter;

static void search(const board *b, state *s, counter *k)
{
    if (!settle(b, s)) return;
    int best = -1;
    for (int i = 0; i < b->nclue; i++)
        if (s->left[i] > 1 && (best < 0 || s->left[i] < s->left[best])) best = i;
    if (best < 0) {
        if (!k->count++ && k->sol)
            for (int i = 0; i < b->nclue; i++)
                for (int m = b->first[i]; m < b->first[i + 1]; m++)
                    if (s->alive[m]) {
                        const cand *c = &b->c[m];
                        uint32_t r = rct_pack(c->x, c->y, c->w, c->h);
                        for (int y = c->y; y < c->y + c->h; y++)
                            for (int x = c->x; x < c->x + c->w; x++) k->sol[y * b->n + x] = r;
                    }
        return;
    }
    for (int m = b->first[best]; m < b->first[best + 1] && k->count < k->limit; m++) {
        if (!s->alive[m]) continue;
        state t = *s;
        keep_only(b, &t, best, m);
        search(b, &t, k);
    }
}

int rct_count(const rct_puz *p, int limit, uint32_t *sol)
{
    static board cb;
    if (!board_load(&cb, p)) return 0;
    static state s;
    state_init(&cb, &s);
    int total = 0;
    for (int c = 0; c < p->n * p->n; c++) total += p->clue[c];
    if (total != p->n * p->n) return 0;         /* the areas do not add up to the grid */
    counter k = {0, limit, sol};
    search(&cb, &s, &k);
    return k.count;
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
static const int MAX_AREA[RCT_SIZES] = {10, 12, 14, 16};

/* the grid cut into rectangles by chance (reading order: each first free cell the corner of a new one) */
static void cut(int n, int maxa, uint32_t *rng, uint32_t *own)
{
    memset(own, 0, sizeof own[0] * RCT_CELLS);
    for (int c = 0; c < n * n; c++) {
        if (own[c]) continue;
        int x = c % n, y = c / n, run = 0;
        while (x + run < n && !own[y * n + x + run]) run++;
        int bw = 1, bh = 1;
        for (int tries = 0; tries < 12; tries++) {
            int w = 1 + pr_below(rng, run < maxa ? run : maxa), most = maxa / w, h = 1;
            if (most > n - y) most = n - y;
            int want = 1 + pr_below(rng, most);
            while (h < want) {                  /* as deep as the cells below are free */
                int ok = 1;
                for (int i = 0; i < w; i++) ok &= !own[(y + h) * n + x + i];
                if (!ok) break;
                h++;
            }
            int a = w * h;
            if (a < 2) continue;
            int score = 4 * (w > h ? h : w) - (w > h ? w : h);   /* squarish ones more often */
            int best = 4 * (bw > bh ? bh : bw) - (bw > bh ? bw : bh);
            if (bw * bh < 2 || score + pr_below(rng, 6) > best) bw = w, bh = h;
            if (pr_below(rng, 3) == 0) break;
        }
        uint32_t r = rct_pack(x, y, bw, bh);
        for (int j = y; j < y + bh; j++)
            for (int i = x; i < x + bw; i++) own[j * n + i] = r;
    }
}

void rct_make(int n, int level, uint32_t seed, rct_puz *out, uint32_t *sol)
{
    n = n < 4 ? 4 : n > RCT_MAX ? RCT_MAX : n;
    int si = 0;                                 /* the biggest area: as for the variants' size at or above n */
    while (si < RCT_SIZES - 1 && rct_sizes[si] < n) si++;
    if (level < RCT_EASY || level > RCT_HARD) level = RCT_EASY;
    uint32_t s = pr_seed(seed);
    static uint32_t own[RCT_CELLS], best_own[RCT_CELLS];
    rct_puz p, best;
    int best_r = -1;
    for (int attempt = 0; attempt < 1500; attempt++) {
        cut(n, MAX_AREA[si], &s, own);
        memset(&p, 0, sizeof p);
        p.n = n;
        for (int c = 0; c < n * n; c++) {       /* a number at a random cell of each rectangle */
            int x, y, w, h;
            rct_unpack(own[c], &x, &y, &w, &h);
            if (y * n + x != c) continue;
            int at = (y + pr_below(&s, h)) * n + x + pr_below(&s, w);
            p.clue[at] = (uint8_t)(w * h);
        }
        for (int fix = 0; fix < 24; fix++) {
            int r = rate_cap(&p, level);
            if (r == level) {
                *out = p;
                memcpy(sol, own, sizeof own);
                return;
            }
            if (r < level) {
                if (r > best_r) {
                    best_r = r;
                    best = p;
                    memcpy(best_own, own, sizeof own);
                }
                break;
            }
            /* stuck (several solutions, or beyond the grader): a number left in doubt moved within its rectangle */
            int doubt[RCT_CLUES], nd = 0;
            for (int i = 0; i < B.nclue; i++)
                if (S.left[i] > 1) doubt[nd++] = i;
            if (!nd) break;
            int at = B.at[doubt[pr_below(&s, nd)]], x, y, w, h;
            rct_unpack(own[at], &x, &y, &w, &h);
            if (w * h < 2) break;
            int to = at;
            while (to == at) to = (y + pr_below(&s, h)) * n + x + pr_below(&s, w);
            p.clue[to] = p.clue[at];
            p.clue[at] = 0;
        }
    }
    if (best_r < 0) {                           /* nothing solvable found (never seen): every cell its own number */
        memset(&best, 0, sizeof best);
        best.n = n;
        for (int c = 0; c < n * n; c++) best.clue[c] = 1, best_own[c] = rct_pack(c % n, c / n, 1, 1);
    }
    *out = best;
    memcpy(sol, best_own, sizeof best_own);
}

/* ---- the player's rectangles ------------------------------------------------------------------------------------- */
void rct_load(rct_game *g, int level, const rct_puz *p, const uint32_t *sol)
{
    memset(g, 0, sizeof *g);
    g->n = p->n;
    g->level = level;
    g->p = *p;
    memcpy(g->sol, sol, sizeof g->sol);
}

int rct_open(rct_game *g, const char *code)
{
    rct_puz p;
    static uint32_t sol[RCT_CELLS];
    if (!rct_decode(code, &p) || rct_count(&p, 2, sol) != 1) return 0;
    rct_load(g, -1, &p, sol);
    return 1;
}

void rct_new(rct_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= RCT_VARIANTS) variant = 0;
    rct_puz p;
    static uint32_t sol[RCT_CELLS];
    rct_make(rct_size_of(variant), rct_level_of(variant), seed, &p, sol);
    rct_load(g, rct_level_of(variant), &p, sol);
    g->seed = seed;
}

/* a cell where a rectangle has its top left corner */
static int is_corner(const rct_game *g, int c)
{
    int x, y, w, h;
    rct_unpack(g->own[c], &x, &y, &w, &h);
    return y * g->n + x == c;
}

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const rct_game *g)
{
    int k = 0;
    for (int c = 0; c < g->n * g->n; c++)
        k += g->own[c] && g->own[c] != g->sol[c] && is_corner(g, c);
    return k;
}

static void set_cell(rct_game *g, int c, uint32_t r)
{
    if (g->own[c] == r) return;
    bench_log(&g->hist, c, g->own[c]);
    g->own[c] = r;
}

static void clear_rect(rct_game *g, uint32_t r)
{
    int x, y, w, h;
    rct_unpack(r, &x, &y, &w, &h);
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (g->own[j * g->n + i] == r) set_cell(g, j * g->n + i, 0);
}

int rct_draw(rct_game *g, int a, int b)
{
    int n = g->n;
    if (a < 0 || b < 0 || a >= n * n || b >= n * n) return 0;
    int x0 = a % n < b % n ? a % n : b % n, x1 = a % n < b % n ? b % n : a % n;
    int y0 = a / n < b / n ? a / n : b / n, y1 = a / n < b / n ? b / n : a / n;
    uint32_t r = rct_pack(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    bench_begin(&g->hist, 2 * n * n);
    for (int y = y0; y <= y1; y++)              /* the rectangles under it go */
        for (int x = x0; x <= x1; x++) {
            uint32_t o = g->own[y * n + x];
            if (o && o != r) clear_rect(g, o);
        }
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) set_cell(g, y * n + x, r);
    return bench_done(&g->hist, wrongs(g));
}

int rct_erase(rct_game *g, int c)
{
    if (c < 0 || c >= g->n * g->n || !g->own[c]) return 0;
    bench_begin(&g->hist, g->n * g->n);
    clear_rect(g, g->own[c]);
    return bench_done(&g->hist, wrongs(g));
}

int rct_good(const rct_game *g, uint32_t r)
{
    if (!r) return 0;
    int x, y, w, h, found = 0;
    rct_unpack(r, &x, &y, &w, &h);
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) {
            int v = g->p.clue[j * g->n + i];
            if (v && (found || v != w * h)) return 0;
            if (v) found = 1;
        }
    return found;
}

int rct_solved(const rct_game *g)
{
    for (int c = 0; c < g->n * g->n; c++)
        if (!g->own[c] || !rct_good(g, g->own[c])) return 0;
    return 1;
}

static int corner_of(int n, uint32_t r)
{
    int x, y, w, h;
    rct_unpack(r, &x, &y, &w, &h);
    return y * n + x;
}

static int draw_sol(rct_game *g, uint32_t r)
{
    int x, y, w, h, n = g->n;
    rct_unpack(r, &x, &y, &w, &h);
    return rct_draw(g, y * n + x, (y + h - 1) * n + x + w - 1);
}

static int hint_step(rct_game *g)
{
    int n = g->n;
    for (int c = 0; c < n * n; c++)             /* a wrong rectangle first */
        if (g->own[c] && g->own[c] != g->sol[c]) {
            int at = corner_of(n, g->own[c]);
            return rct_erase(g, c) ? at : -1;
        }
    if (!board_load(&B, &g->p)) return -1;
    static state s;
    state_init(&B, &s);
    for (int i = 0; i < B.nclue; i++)           /* the right rectangles drawn: theirs for sure */
        if (g->own[B.at[i]])
            for (int k = B.first[i]; k < B.first[i + 1]; k++) {
                const cand *c = &B.c[k];
                if (rct_pack(c->x, c->y, c->w, c->h) == g->own[B.at[i]]) keep_only(&B, &s, i, k);
            }
    for (int guard = 0; guard < 4 * RCT_CELLS; guard++) {   /* the easiest ways until a number is sure */
        for (int i = 0; i < B.nclue; i++)
            if (s.left[i] == 1 && !g->own[B.at[i]]) {
                int at = B.at[i];
                return draw_sol(g, g->sol[at]) ? corner_of(n, g->sol[at]) : -1;
            }
        if (step(&B, &s, RCT_HARD) < 0) break;
    }
    for (int c = 0; c < n * n; c++)             /* stuck: the first rectangle not drawn */
        if (!g->own[c]) return draw_sol(g, g->sol[c]) ? corner_of(n, g->sol[c]) : -1;
    return -1;
}

static void put_back(void *ctx, int cell, uint32_t was) { ((rct_game *)ctx)->own[cell] = was; }

int rct_hint(rct_game *g)
{
    return bench_hinted(&g->hist, hint_step(g));
}

int rct_undo(rct_game *g)
{
    int c = bench_undo(&g->hist, put_back, g);
    bench_undone(&g->hist, wrongs(g));
    return c;
}


int rct_script(rct_game *g, const char *s)
{
    int n = g->n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (rct_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (g->p.clue[c] && g->own[c] != g->sol[c]) { draw_sol(g, g->sol[c]); k--; }
            continue;
        }
        int len, len2;
        if (*s == 'x') {
            int c = bench_cell(s + 1, n, n, &len);
            if (c < 0 || !rct_erase(g, c)) return 0;
            s += 1 + len;
            continue;
        }
        int a = bench_cell(s, n, n, &len);
        if (a < 0 || s[len] != '-') return 0;
        int b = bench_cell(s + len + 1, n, n, &len2);
        if (b < 0 || !rct_draw(g, a, b)) return 0;
        s += len + 1 + len2;
    }
    return 1;
}
