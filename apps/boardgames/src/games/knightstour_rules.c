/*
 * The Knight's Tour's rules (knightstour.h): the moves, undo, the solver (Warnsdorff's order, a depth-first search,
 * cut short when some square could no longer be reached and left), the start drawn from the seed, the levels, the
 * hint.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/knightstour.h"

#include <string.h>

#include "games/knightstour_levels.h"
#include "games/puzrand.h"

#define HINT_BUDGET 40000                       /* the search's steps for each move the hint weighs */
#define START_BUDGET 2000000L

static const int8_t JUMP[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};

int kt_size(int variant) { return variant == KT_5 ? 5 : variant == KT_6 ? 6 : 8; }

/* the squares (not holes) a knight reaches from s; their count */
static int jumps(const kt_game *g, int s, uint8_t *to)
{
    int f = s % g->w, r = s / g->w, k = 0;
    for (int i = 0; i < 8; i++) {
        int x = f + JUMP[i][0], y = r + JUMP[i][1];
        if (x >= 0 && x < g->w && y >= 0 && y < g->h && !g->hole[y * g->w + x]) to[k++] = (uint8_t)(y * g->w + x);
    }
    return k;
}

int kt_setup(kt_game *g, int w, int h, const uint8_t *hole, int start)
{
    memset(g, 0, sizeof *g);
    if (w < 1 || h < 1 || w > KT_MAXW || h > KT_MAXW || start < 0 || start >= w * h) return 0;
    g->w = w;
    g->h = h;
    for (int s = 0; s < w * h; s++) {
        g->hole[s] = hole && hole[s];
        g->cells += !g->hole[s];
    }
    if (g->hole[start]) return 0;
    g->path[0] = (uint8_t)start;
    g->order[start] = 1;
    g->len = 1;
    return 1;
}

void kt_new_at(kt_game *g, int n, int start) { kt_setup(g, n, n, NULL, start); }

int kt_load(kt_game *g, const char *rows)
{
    uint8_t hole[KT_MAX];
    int w = 0, h = 0, x = 0, start = -1, starts = 0;
    char grid[KT_MAXW][KT_MAXW];
    memset(g, 0, sizeof *g);
    for (const char *p = rows;; p++) {
        if (*p == '|' || !*p) {
            if (!h) w = x;
            if (x != w || !w) return 0;
            h++;
            x = 0;
            if (!*p) break;
            continue;
        }
        if ((*p != '.' && *p != '#' && *p != 'N') || x >= KT_MAXW || h >= KT_MAXW) return 0;
        grid[h][x++] = *p;
    }
    for (int y = 0; y < h; y++)                 /* row y from the top: rank h - 1 - y */
        for (int f = 0; f < w; f++) {
            int s = (h - 1 - y) * w + f;
            hole[s] = grid[y][f] == '#';
            if (grid[y][f] == 'N') { start = s; starts++; }
        }
    return starts == 1 && kt_setup(g, w, h, hole, start);
}

int kt_level(kt_game *g, int level)
{
    if (level < 0 || level >= KT_LEVELS) level = 0;
    return kt_load(g, kt_levels[level]);
}

static int cur(const kt_game *g) { return g->path[g->len - 1]; }

int kt_can(const kt_game *g, int sq)
{
    if (sq < 0 || sq >= g->w * g->h || g->hole[sq] || g->order[sq]) return 0;
    int c = cur(g), df = sq % g->w - c % g->w, dr = sq / g->w - c / g->w;
    return (df * df + dr * dr) == 5;
}

int kt_moves(const kt_game *g, uint8_t *to)
{
    uint8_t all[8];
    int n = jumps(g, cur(g), all), k = 0;
    for (int i = 0; i < n; i++)
        if (!g->order[all[i]]) to[k++] = all[i];
    return k;
}

int kt_done(const kt_game *g) { return g->len == g->cells; }

int kt_stuck(const kt_game *g)
{
    uint8_t to[8];
    return !kt_done(g) && !kt_moves(g, to);
}

int kt_move(kt_game *g, int sq)
{
    if (!kt_can(g, sq)) return 0;
    g->path[g->len++] = (uint8_t)sq;
    g->order[sq] = (uint8_t)g->len;
    if (kt_stuck(g)) g->dead_ends++;
    return 1;
}

int kt_undo(kt_game *g)
{
    if (g->len <= 1) return 0;
    g->order[g->path[--g->len]] = 0;
    return 1;
}

int kt_closed(const kt_game *g)
{
    if (!kt_done(g) || g->len < 2) return 0;
    int a = g->path[0], b = cur(g), df = a % g->w - b % g->w, dr = a / g->w - b / g->w;
    return df * df + dr * dr == 5;
}

int kt_onward(const kt_game *g, int sq)
{
    uint8_t all[8];
    int n = jumps(g, sq, all), k = 0;
    for (int i = 0; i < n; i++)
        if (!g->order[all[i]] && all[i] != sq) k++;
    return k;
}

int kt_stars(const kt_game *g) { return !g->hints && !g->dead_ends ? 3 : g->hints + g->dead_ends <= 2 ? 2 : 1; }

/* ---- the search -------------------------------------------------------------------------------------------------- */
typedef struct {
    int w, h, total;
    uint8_t seen[KT_MAX];                       /* visited, or a hole */
    uint8_t nb[KT_MAX][8], nnb[KT_MAX];
    long steps, budget;
    uint8_t *out;
} search;

/* every square left must still be entered and left again (but the last): links to the squares left, or to the
 * knight's square. None: lost; one: it must be the last, and only one can be. */
static int reachable(const search *s, int at, int left)
{
    if (left <= 1) return 1;
    int ends = 0;
    for (int u = 0; u < s->w * s->h; u++) {
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
static int order(const search *s, int at, uint8_t *c)
{
    int k = 0, key[8];
    for (int i = 0; i < s->nnb[at]; i++) {
        int v = s->nb[at][i];
        if (s->seen[v]) continue;
        float mx = (s->w - 1) / 2.0f, my = (s->h - 1) / 2.0f, dx = v % s->w - mx, dy = v / s->w - my;
        int kv = degree(s, v) * 1000 - (int)(10 * (dx * dx + dy * dy));
        int j = k++;
        for (; j > 0 && key[j - 1] > kv; j--) { key[j] = key[j - 1]; c[j] = c[j - 1]; }
        key[j] = kv;
        c[j] = (uint8_t)v;
    }
    return k;
}

static int dfs(search *s, int at, int depth)
{
    if (depth == s->total) return 1;
    if (++s->steps > s->budget) return -1;
    if (!reachable(s, at, s->total - depth)) return 0;
    uint8_t c[8];
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

static void search_init(search *s, const kt_game *g, long budget, uint8_t *out)
{
    memset(s, 0, sizeof *s);
    s->w = g->w;
    s->h = g->h;
    for (int u = 0; u < g->w * g->h; u++) {
        s->nnb[u] = (uint8_t)jumps(g, u, s->nb[u]);
        s->seen[u] = g->order[u] != 0 || g->hole[u];
    }
    s->total = g->cells - g->len;
    s->budget = budget;
    s->out = out;
}

int kt_solve(const kt_game *g, uint8_t *out, long budget)
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
    uint8_t c[8];
    int k = order(&s, cur(g), c), unsure = -1, dead = g->dead_ends;
    for (int i = 0; i < k; i++) {
        kt_move(g, c[i]);
        int r = kt_solve(g, NULL, HINT_BUDGET);
        kt_undo(g);
        g->dead_ends = dead;                    /* a look ahead, not a move */
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
        if (f < 0 || f >= g->w || r < 0 || r >= g->h) return 0;
        int sq = r * g->w + f;
        if (first && g->len == 1 && !kt_can(g, sq) && !g->hole[sq]) {   /* the start named */
            kt_game t = *g;
            if (!kt_setup(g, t.w, t.h, t.hole, sq)) return 0;
        } else if (!kt_move(g, sq)) return 0;
        first = 0;
        s += i;
    }
    return 1;
}
