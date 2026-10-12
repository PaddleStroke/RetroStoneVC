/*
 * Eight Queens' rules (queens.h): the queens put on and taken off, the attacks, every solution of a board found by a
 * search (one queen a rank), the fixed queen drawn from them, the levels, the hint.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/queens.h"

#include <string.h>

#include "games/puzrand.h"
#include "games/queens_levels.h"

#define QN_MOST 724                             /* the solutions of 10 x 10, the most of the boards */

int qn_size(int variant) { static const int N[QN_VARIANTS] = {5, 6, 8, 10}; return N[variant]; }

/* ---- every solution of an n x n board, in order, kept ------------------------------------------------------------ */
static struct { int n, count; uint64_t key[QN_MOST]; } all;

static void place(int n, int rank, unsigned cols, unsigned d1, unsigned d2, uint64_t key)
{
    if (rank == n) {
        if (all.count < QN_MOST) all.key[all.count] = key;
        all.count++;
        return;
    }
    for (int f = 0; f < n; f++) {
        unsigned c = 1u << f, a = 1u << (f + rank), b = 1u << (f - rank + n);
        if ((cols & c) || (d1 & a) || (d2 & b)) continue;
        place(n, rank + 1, cols | c, d1 | a, d2 | b, key | (uint64_t)f << (4 * rank));
    }
}

static void solve_all(int n)
{
    if (all.n == n) return;
    all.n = n;
    all.count = 0;
    place(n, 0, 0, 0, 0, 0);
}

int qn_solutions(int n)
{
    solve_all(n);
    return all.count;
}

int qn_solution(int n, int i, uint64_t *key)
{
    solve_all(n);
    if (i < 0 || i >= all.count || i >= QN_MOST) return 0;
    *key = all.key[i];
    return 1;
}

static int file_in(uint64_t key, int rank) { return (int)(key >> (4 * rank) & 15); }

/* ---- the board --------------------------------------------------------------------------------------------------- */
void qn_empty(qn_game *g, int n, int given)
{
    memset(g, 0, sizeof *g);
    g->n = n;
    g->given = given;
    if (given >= 0) {
        g->q[given] = g->fixed[given] = 1;
        g->count = 1;
    }
}

void qn_new(qn_game *g, int variant, uint32_t seed, const uint64_t *found, int nfound)
{
    int n = qn_size(variant < 0 || variant >= QN_VARIANTS ? QN_8 : variant);
    uint32_t rs = pr_seed(seed);
    solve_all(n);
    int fresh = 0;
    for (int i = 0; i < all.count; i++) {
        int seen = 0;
        for (int j = 0; j < nfound && !seen; j++) seen = found[j] == all.key[i];
        fresh += !seen;
    }
    int pick = pr_below(&rs, fresh ? fresh : all.count), chosen = 0;
    for (int i = 0; i < all.count; i++) {
        int seen = 0;
        for (int j = 0; j < nfound && !seen && fresh; j++) seen = found[j] == all.key[i];
        if (seen) continue;
        if (pick-- == 0) { chosen = i; break; }
    }
    int rank = pr_below(&rs, n);
    qn_empty(g, n, rank * n + file_in(all.key[chosen], rank));
}

int qn_load(qn_game *g, const char *rows)
{
    char grid[QN_MAX][QN_MAX];
    int n = 0, h = 0, x = 0;
    memset(g, 0, sizeof *g);
    for (const char *p = rows;; p++) {
        if (*p == '|' || !*p) {
            if (!h) n = x;
            if (x != n || !n) return 0;
            h++;
            x = 0;
            if (!*p) break;
            continue;
        }
        if ((*p != '.' && *p != '#' && *p != 'Q') || x >= QN_MAX || h >= QN_MAX) return 0;
        grid[h][x++] = *p;
    }
    if (h != n || n < 4) return 0;
    qn_empty(g, n, -1);
    for (int y = 0; y < n; y++)                 /* row y from the top: rank n - 1 - y */
        for (int f = 0; f < n; f++) {
            int s = (n - 1 - y) * n + f;
            g->hole[s] = grid[y][f] == '#';
            if (grid[y][f] == 'Q') {
                g->q[s] = g->fixed[s] = 1;
                g->count++;
                if (g->given < 0) g->given = s;
            }
        }
    return 1;
}

int qn_level(qn_game *g, int level)
{
    if (level < 0 || level >= QN_LEVELS) level = 0;
    return qn_levels[level] && qn_load(g, qn_levels[level]);
}

int qn_attacks(int n, int a, int b)
{
    if (a == b || a < 0 || b < 0) return 0;
    int fa = a % n, ra = a / n, fb = b % n, rb = b / n;
    return fa == fb || ra == rb || fa - ra == fb - rb || fa + ra == fb + rb;
}

int qn_attacked(const qn_game *g, int sq)
{
    int k = 0;
    for (int s = 0; s < g->n * g->n; s++) k += g->q[s] && qn_attacks(g->n, s, sq);
    return k;
}

int qn_clash(const qn_game *g, int sq) { return g->q[sq] && qn_attacked(g, sq) > 0; }

int qn_solved(const qn_game *g)
{
    if (g->count != g->n) return 0;
    for (int s = 0; s < g->n * g->n; s++)
        if (qn_clash(g, s)) return 0;
    return 1;
}

uint64_t qn_key(const qn_game *g)
{
    uint64_t key = 0;
    for (int s = 0; s < g->n * g->n; s++)
        if (g->q[s]) key |= (uint64_t)(s % g->n) << (4 * (s / g->n));
    return key;
}

int qn_toggle(qn_game *g, int sq)
{
    if (sq < 0 || sq >= g->n * g->n || g->fixed[sq] || g->hole[sq] || g->nhist >= QN_HISTORY) return 0;
    g->q[sq] = !g->q[sq];
    g->count += g->q[sq] ? 1 : -1;
    if (g->q[sq] && qn_attacked(g, sq)) g->mistakes++;   /* put where a queen attacks it */
    g->hist[g->nhist++] = (int16_t)sq;
    g->moves++;
    return 1;
}

int qn_undo(qn_game *g)
{
    if (!g->nhist) return 0;
    int sq = g->hist[--g->nhist];
    g->q[sq] = !g->q[sq];
    g->count += g->q[sq] ? 1 : -1;
    g->moves--;
    return 1;
}

int qn_stars(const qn_game *g) { return !g->hints && !g->mistakes ? 3 : g->hints + g->mistakes <= 2 ? 2 : 1; }

/* ---- the solutions a board allows, the hint ---------------------------------------------------------------------- */
/* solution i keeps off the holes */
static int fits(const qn_game *g, int i)
{
    for (int r = 0; r < g->n; r++)
        if (g->hole[r * g->n + file_in(all.key[i], r)]) return 0;
    return 1;
}

/* solution i holds every queen on the board (or, fixed_only, every fixed queen) */
static int holds(const qn_game *g, int i, int fixed_only)
{
    for (int s = 0; s < g->n * g->n; s++)
        if ((fixed_only ? g->fixed[s] : g->q[s]) && file_in(all.key[i], s / g->n) != s % g->n) return 0;
    return 1;
}

int qn_count(const qn_game *g, uint64_t *key)
{
    solve_all(g->n);
    int k = 0;
    for (int i = 0; i < all.count; i++)
        if (fits(g, i) && holds(g, i, 1)) {
            if (!k && key) *key = all.key[i];
            k++;
        }
    return k;
}

/* a solution holding every queen on the board (one not in found[] first); -1: none */
static int reachable(const qn_game *g, const uint64_t *found, int nfound)
{
    int any = -1;
    for (int i = 0; i < all.count; i++) {
        if (!fits(g, i) || !holds(g, i, 0)) continue;
        int seen = 0;
        for (int j = 0; j < nfound && !seen; j++) seen = found[j] == all.key[i];
        if (!seen) return i;
        if (any < 0) any = i;
    }
    return any;
}

int qn_hint(qn_game *g, const uint64_t *found, int nfound, int *remove)
{
    *remove = 0;
    if (qn_solved(g)) return -1;
    solve_all(g->n);
    int n = g->n, i = reachable(g, found, nfound);
    g->hints++;
    if (i >= 0)                                 /* the lowest rank the solution still needs */
        for (int r = 0; r < n; r++) {
            int s = r * n + file_in(all.key[i], r);
            if (!g->q[s]) return s;
        }
    *remove = 1;                                /* a queen to take off: one whose leaving opens a solution */
    int best = -1, best_score = -1;
    for (int s = 0; s < n * n; s++) {
        if (!g->q[s] || g->fixed[s]) continue;
        g->q[s] = 0;
        int opens = reachable(g, found, nfound) >= 0, hits = 0;
        g->q[s] = 1;
        for (int t = 0; t < n * n; t++) hits |= g->fixed[t] && qn_attacks(n, s, t);
        int score = opens * 4 + (hits ? 2 : 0) + (qn_clash(g, s) ? 1 : 0);
        if (score > best_score) { best_score = score; best = s; }
    }
    return best;
}

int qn_script(qn_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == '?') {                       /* the hint followed */
            int rm, h = qn_hint(g, NULL, 0, &rm);
            if (h < 0 || !qn_toggle(g, h)) return 0;
            s++;
            continue;
        }
        int f = s[0] - 'a', r = 0, i = 1;
        while (s[i] >= '0' && s[i] <= '9') r = r * 10 + (s[i++] - '0');
        r--;
        if (f < 0 || f >= g->n || r < 0 || r >= g->n || !qn_toggle(g, r * g->n + f)) return 0;
        s += i;
    }
    return 1;
}
