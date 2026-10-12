/*
 * The Knight's Tour's rules (knightstour.h): the moves, undo, the solver (Warnsdorff's order, a depth-first search,
 * cut short when some square could no longer be reached and left), the start drawn from the seed, the hint.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/knightstour.h"

#include <string.h>

#include "games/puzrand.h"

#define HINT_BUDGET 40000                       /* the search's steps for each move the hint weighs */
#define START_BUDGET 2000000L

static const int8_t JUMP[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};

int kt_size(int variant) { return variant == KT_5 ? 5 : variant == KT_6 ? 6 : 8; }

/* the squares a knight reaches from s on an n x n board; their count */
static int jumps(int n, int s, int8_t *to)
{
    int f = s % n, r = s / n, k = 0;
    for (int i = 0; i < 8; i++) {
        int x = f + JUMP[i][0], y = r + JUMP[i][1];
        if (x >= 0 && x < n && y >= 0 && y < n) to[k++] = (int8_t)(y * n + x);
    }
    return k;
}

void kt_new_at(kt_game *g, int n, int start)
{
    memset(g, 0, sizeof *g);
    g->n = n;
    g->path[0] = (int8_t)start;
    g->order[start] = 1;
    g->len = 1;
}

static int cur(const kt_game *g) { return g->path[g->len - 1]; }

int kt_can(const kt_game *g, int sq)
{
    if (sq < 0 || sq >= g->n * g->n || g->order[sq]) return 0;
    int c = cur(g), df = sq % g->n - c % g->n, dr = sq / g->n - c / g->n;
    return (df * df + dr * dr) == 5;
}

int kt_moves(const kt_game *g, int8_t *to)
{
    int8_t all[8];
    int n = jumps(g->n, cur(g), all), k = 0;
    for (int i = 0; i < n; i++)
        if (!g->order[all[i]]) to[k++] = all[i];
    return k;
}

int kt_move(kt_game *g, int sq)
{
    if (!kt_can(g, sq)) return 0;
    g->path[g->len++] = (int8_t)sq;
    g->order[sq] = (int8_t)g->len;
    return 1;
}

int kt_undo(kt_game *g)
{
    if (g->len <= 1) return 0;
    g->order[g->path[--g->len]] = 0;
    return 1;
}

int kt_done(const kt_game *g) { return g->len == g->n * g->n; }

int kt_closed(const kt_game *g)
{
    if (!kt_done(g)) return 0;
    int a = g->path[0], b = cur(g), df = a % g->n - b % g->n, dr = a / g->n - b / g->n;
    return df * df + dr * dr == 5;
}

int kt_stuck(const kt_game *g)
{
    int8_t to[8];
    return !kt_done(g) && !kt_moves(g, to);
}

int kt_onward(const kt_game *g, int sq)
{
    int8_t all[8];
    int n = jumps(g->n, sq, all), k = 0;
    for (int i = 0; i < n; i++)
        if (!g->order[all[i]] && all[i] != sq) k++;
    return k;
}

/* ---- the search -------------------------------------------------------------------------------------------------- */
typedef struct {
    int n, total;
    uint8_t seen[KT_MAX];
    int8_t nb[KT_MAX][8], nnb[KT_MAX];
    long steps, budget;
    int8_t *out;
} search;

/* every square left must still be entered and left again (but the last): links to the squares left, or to the
 * knight's square. None: lost; one: it must be the last, and only one can be. */
static int reachable(const search *s, int at, int left)
{
    if (left <= 1) return 1;
    int ends = 0;
    for (int u = 0; u < s->n * s->n; u++) {
        if (s->seen[u]) continue;
        int d = 0;
        for (int i = 0; i < s->nnb[u] && d < 2; i++) {
            int v = s->nb[u][i];
            d += !s->seen[v] || v == at;
        }
        if (!d || (d == 1 && ++ends > 1)) return 0;
    }
    return 1;
}

static int degree(const search *s, int u)
{
    int d = 0;
    for (int i = 0; i < s->nnb[u]; i++) d += !s->seen[s->nb[u][i]];
    return d;
}

/* Warnsdorff's order: the fewest onward moves first; on a tie, the square further from the middle */
static int order(const search *s, int at, int8_t *c)
{
    int k = 0, key[8];
    for (int i = 0; i < s->nnb[at]; i++) {
        int v = s->nb[at][i];
        if (s->seen[v]) continue;
        float m = (s->n - 1) / 2.0f, dx = v % s->n - m, dy = v / s->n - m;
        int kv = degree(s, v) * 1000 - (int)(10 * (dx * dx + dy * dy));
        int j = k++;
        for (; j > 0 && key[j - 1] > kv; j--) { key[j] = key[j - 1]; c[j] = c[j - 1]; }
        key[j] = kv;
        c[j] = (int8_t)v;
    }
    return k;
}

static int dfs(search *s, int at, int depth)
{
    if (depth == s->total) return 1;
    if (++s->steps > s->budget) return -1;
    if (!reachable(s, at, s->total - depth)) return 0;
    int8_t c[8];
    int k = order(s, at, c);
    for (int i = 0; i < k; i++) {
        s->seen[c[i]] = 1;
        if (s->out) s->out[depth] = c[i];
        int r = dfs(s, c[i], depth + 1);
        s->seen[c[i]] = 0;
        if (r) return r;
    }
    return 0;
}

static void search_init(search *s, const kt_game *g, long budget, int8_t *out)
{
    memset(s, 0, sizeof *s);
    s->n = g->n;
    for (int u = 0; u < g->n * g->n; u++) {
        s->nnb[u] = (int8_t)jumps(g->n, u, s->nb[u]);
        s->seen[u] = g->order[u] != 0;
    }
    s->total = g->n * g->n - g->len;
    s->budget = budget;
    s->out = out;
}

int kt_solve(const kt_game *g, int8_t *out, long budget)
{
    static search s;
    search_init(&s, g, budget, out);
    return dfs(&s, cur(g), 0);
}

void kt_new(kt_game *g, int variant, uint32_t seed)
{
    int n = kt_size(variant), squares[KT_MAX], k = 0;
    uint32_t rs = pr_seed(seed);
    for (int s = 0; s < n * n; s++)
        if (n % 2 == 0 || (s % n + s / n) % 2 == 0) squares[k++] = s;   /* odd boards: the corners' colour */
    pr_shuffle(&rs, squares, k);
    for (int i = 0; i < k; i++) {
        kt_new_at(g, n, squares[i]);
        if (kt_solve(g, NULL, START_BUDGET) == 1) return;
    }
    kt_new_at(g, n, 0);                         /* never: a corner always has a tour */
}

int kt_hint(kt_game *g)
{
    if (kt_done(g) || kt_stuck(g)) return -2;
    static search s;
    search_init(&s, g, 0, NULL);
    int8_t c[8];
    int k = order(&s, cur(g), c), unsure = -1;
    for (int i = 0; i < k; i++) {
        kt_move(g, c[i]);
        int r = kt_solve(g, NULL, HINT_BUDGET);
        kt_undo(g);
        if (r == 1) { g->hints++; return c[i]; }
        if (r < 0 && unsure < 0) unsure = c[i];
    }
    if (unsure >= 0) g->hints++;                /* the search ran out of time: Warnsdorff's rule alone */
    return unsure;
}

int kt_script(kt_game *g, const char *s)
{
    int first = 1;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == '?') {                       /* the hint followed */
            int h = kt_hint(g);
            if (h < 0 || !kt_move(g, h)) return 0;
            s++;
            first = 0;
            continue;
        }
        int f = s[0] - 'a', r = 0, i = 1;
        while (s[i] >= '0' && s[i] <= '9') r = r * 10 + (s[i++] - '0');
        r--;
        if (f < 0 || f >= g->n || r < 0 || r >= g->n) return 0;
        int sq = r * g->n + f;
        if (first && g->len == 1 && !kt_can(g, sq)) kt_new_at(g, g->n, sq);   /* the start named */
        else if (!kt_move(g, sq)) return 0;
        first = 0;
        s += i;
    }
    return 1;
}
