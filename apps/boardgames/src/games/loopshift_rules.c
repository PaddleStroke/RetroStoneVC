/*
 * Row Shift's rules (loopshift.h): the moves, merged; undo; the scramble; the meeting-in-the-middle search for the
 * fewest moves; the hint; the levels.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/loopshift.h"

#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

/* ---- the moves --------------------------------------------------------------------------------------------------- */
void lsh_apply(int8_t *at, int n, int move)
{
    int line = lsh_line(move), k = lsh_amount(move) % n;
    if (line < 0 || line >= 2 * n || !k) return;
    int8_t t[LSH_MAX];
    if (line < n) {                             /* a row, k steps to the right */
        int8_t *r = at + line * n;
        for (int x = 0; x < n; x++) t[(x + k) % n] = r[x];
        memcpy(r, t, (size_t)n);
    } else {                                    /* a column, k steps down */
        int c = line - n;
        for (int y = 0; y < n; y++) t[(y + k) % n] = at[y * n + c];
        for (int y = 0; y < n; y++) at[y * n + c] = t[y];
    }
}

int lsh_solved_at(const int8_t *at, int n, int colour)
{
    for (int p = 0; p < n * n; p++)
        if (colour ? at[p] / n != p / n : at[p] != p) return 0;
    return 1;
}

void lsh_init(lsh_game *g, int n, int colour)
{
    memset(g, 0, sizeof *g);
    g->n = n < LSH_MIN ? LSH_MIN : n > LSH_MAX ? LSH_MAX : n;
    g->colour = !!colour;
    for (int p = 0; p < g->n * g->n; p++) g->at[p] = g->start[p] = (int8_t)p;
}

int lsh_solved(const lsh_game *g) { return lsh_solved_at(g->at, g->n, g->colour); }
int lsh_moves(const lsh_game *g) { return g->nh; }

static int valid(int n, int move)
{
    return lsh_line(move) < 2 * n && lsh_amount(move) >= 1 && lsh_amount(move) < n;
}

int lsh_play(lsh_game *g, int move)
{
    if (!valid(g->n, move)) return 0;
    lsh_apply(g->at, g->n, move);
    if (g->nh && lsh_line(g->hist[g->nh - 1]) == lsh_line(move)) {   /* the same line again: the same move */
        int k = (lsh_amount(g->hist[g->nh - 1]) + lsh_amount(move)) % g->n;
        if (k) g->hist[g->nh - 1] = (uint8_t)lsh_move(lsh_line(move), k);
        else g->nh--;
    } else if (g->nh < LSH_HIST) {
        g->hist[g->nh++] = (uint8_t)move;
    } else {                                    /* a path this long: the oldest step forgotten */
        memmove(g->hist, g->hist + 1, LSH_HIST - 1);
        g->hist[LSH_HIST - 1] = (uint8_t)move;
    }
    return 1;
}

int lsh_undo(lsh_game *g)
{
    if (!g->nh) return -1;
    int m = g->hist[--g->nh];
    lsh_apply(g->at, g->n, lsh_inverse(g->n, m));
    return m;
}

/* a move may follow prev (-1: none) in the canonical order: never the same line; two rows (or two columns) in a row
 * in increasing order */
static int canonical(int n, int prev, int line)
{
    if (prev < 0) return 1;
    if ((prev < n) != (line < n)) return 1;
    return line > prev;
}

static int same_look(const int8_t *a, const int8_t *b, int n, int colour)
{
    for (int p = 0; p < n * n; p++)
        if (colour ? a[p] / n != b[p] / n : a[p] != b[p]) return 0;
    return 1;
}

void lsh_scramble(lsh_game *g, int n, int colour, int depth, uint32_t seed)
{
    lsh_init(g, n, colour);
    n = g->n;
    uint32_t s = pr_seed(seed);
    uint8_t way[LSH_SOL];
    int prev = -1, made = 0;
    if (depth > LSH_SOL) depth = LSH_SOL;
    for (int tries = 0; made < depth && tries < 1000; tries++) {
        int line = pr_below(&s, 2 * n), k = 1 + pr_below(&s, n - 1);
        if (!canonical(n, prev, line)) continue;
        int8_t t[LSH_CELLS];
        memcpy(t, g->at, sizeof t);
        lsh_apply(t, n, lsh_move(line, k));
        if (same_look(t, g->at, n, colour) || lsh_solved_at(t, n, colour)) continue;
        memcpy(g->at, t, sizeof t);
        way[made++] = (uint8_t)lsh_move(line, k);
        prev = line;
    }
    memcpy(g->start, g->at, sizeof g->at);
    g->nsol = made;
    for (int i = 0; i < made; i++) g->sol[i] = (uint8_t)lsh_inverse(n, way[made - 1 - i]);
    g->par = made;
    g->exact = 0;
}

/* ---- the fewest moves: a meeting in the middle --------------------------------------------------------------- */
static uint64_t hash_of(const int8_t *at, int n, int colour)
{
    uint64_t h = 1469598103934665603ull ^ (uint64_t)(n * 2 + colour);
    for (int p = 0; p < n * n; p++) {
        h ^= (uint64_t)(uint8_t)(colour ? at[p] / n : at[p]) + 1;
        h *= 1099511628211ull;
    }
    h ^= h >> 29;
    h *= 0xbf58476d1ce4e5b9ull;
    h ^= h >> 32;
    return h | 1;                               /* 0: an empty slot */
}

static struct {
    uint64_t *key;
    uint8_t *dist;
    size_t cap, used;
    int n, colour, b;
} K = {0};

static uint8_t *table_slot(uint64_t h, int add)
{
    size_t m = K.cap - 1, i = (size_t)(h ^ h >> 31) & m;
    while (K.key[i] && K.key[i] != h) i = (i + 1) & m;
    if (!K.key[i]) {
        if (!add) return NULL;
        K.key[i] = h;
        K.dist[i] = 255;
        K.used++;
    }
    return &K.dist[i];
}

static int table_grow(void)
{
    size_t cap = K.cap ? K.cap * 2 : 1u << 12;
    uint64_t *key = calloc(cap, sizeof *key), *ok = K.key;
    uint8_t *dist = malloc(cap), *od = K.dist;
    if (!key || !dist) { free(key); free(dist); return 0; }
    size_t old = K.cap;
    K.key = key;
    K.dist = dist;
    K.cap = cap;
    K.used = 0;
    for (size_t i = 0; i < old; i++)
        if (ok[i]) *table_slot(ok[i], 1) = od[i];
    free(ok);
    free(od);
    return 1;
}

static int table_lookup(const int8_t *at)
{
    uint8_t *d = K.cap ? table_slot(hash_of(at, K.n, K.colour), 0) : NULL;
    return d ? *d : -1;
}

static int table_full;

static void back_walk(int8_t *at, int depth, int prev)
{
    if (table_full) return;
    if (K.used * 2 >= K.cap && !table_grow()) { table_full = 1; return; }
    uint8_t *d = table_slot(hash_of(at, K.n, K.colour), 1);
    if (*d < depth) return;                     /* reached sooner already: its moves were walked from there */
    *d = (uint8_t)depth;
    if (depth >= K.b) return;
    int n = K.n;
    for (int line = 0; line < 2 * n; line++) {
        if (!canonical(n, prev, line)) continue;
        for (int k = 1; k < n; k++) {
            int8_t t[LSH_CELLS];
            memcpy(t, at, sizeof t);
            lsh_apply(t, n, lsh_move(line, k));
            back_walk(t, depth + 1, line);
        }
    }
}

void lsh_solve_free(void)
{
    free(K.key);
    free(K.dist);
    memset(&K, 0, sizeof K);
}

static int table_for(int n, int colour, int b)
{
    if (K.cap && K.n == n && K.colour == colour && K.b >= b) return 1;
    lsh_solve_free();
    K.n = n;
    K.colour = colour;
    K.b = b;
    if (!table_grow()) return 0;
    int8_t at[LSH_CELLS];
    for (int p = 0; p < n * n; p++) at[p] = (int8_t)p;
    table_full = 0;
    back_walk(at, 0, -1);
    if (table_full) { lsh_solve_free(); return 0; }
    return 1;
}

static struct {
    int n, a, b, best, meet_d;
    uint8_t stack[LSH_SOL], way[LSH_SOL];
    int8_t meet[LSH_CELLS];
} S;

static void fore_walk(const int8_t *at, int d, int prev)
{
    int r = table_lookup(at);
    if (r >= 0) {                               /* within b of the end: nothing below does better */
        if (d + r < S.best) {
            S.best = d + r;
            S.meet_d = d;
            memcpy(S.way, S.stack, (size_t)d);
            memcpy(S.meet, at, sizeof S.meet);
        }
        return;
    }
    if (d + S.b + 1 >= S.best || d >= S.a || d >= LSH_SOL) return;
    int n = S.n;
    for (int line = 0; line < 2 * n; line++) {
        if (!canonical(n, prev, line)) continue;
        for (int k = 1; k < n; k++) {
            int8_t t[LSH_CELLS];
            memcpy(t, at, sizeof t);
            lsh_apply(t, n, lsh_move(line, k));
            S.stack[d] = (uint8_t)lsh_move(line, k);
            fore_walk(t, d + 1, line);
        }
    }
}

int lsh_solve(const int8_t *at, int n, int colour, int a, int b, int upper, uint8_t *path, int max)
{
    if (lsh_solved_at(at, n, colour)) return 0;
    if (!table_for(n, colour, b)) return -1;
    S.n = n;
    S.a = a;
    S.b = K.b;                                  /* a table kept from a deeper search: all the better */
    S.best = upper > 0 ? upper : 1000;
    S.meet_d = -1;
    fore_walk(at, 0, -1);
    if (S.meet_d < 0) return -1;
    int len = S.meet_d;                         /* then down the kept distances to the end */
    if (path) memcpy(path, S.way, (size_t)(len < max ? len : max));
    int8_t cur[LSH_CELLS];
    memcpy(cur, S.meet, sizeof cur);
    for (int r = table_lookup(cur); r > 0;) {
        int found = 0;
        for (int line = 0; line < 2 * n && !found; line++)
            for (int k = 1; k < n && !found; k++) {
                int8_t t[LSH_CELLS];
                memcpy(t, cur, sizeof t);
                lsh_apply(t, n, lsh_move(line, k));
                if (table_lookup(t) == r - 1) {
                    memcpy(cur, t, sizeof cur);
                    if (path && len < max) path[len] = (uint8_t)lsh_move(line, k);
                    len++;
                    r--;
                    found = 1;
                }
            }
        if (!found) return -1;
    }
    return lsh_solved_at(cur, n, colour) ? len : -1;
}

void lsh_depths(int n, int *a, int *b)
{
    static const int A[LSH_MAX + 1] = {0, 0, 0, 5, 4, 3, 3}, B[LSH_MAX + 1] = {0, 0, 0, 5, 4, 3, 3};
    if (n < LSH_MIN) n = LSH_MIN;
    if (n > LSH_MAX) n = LSH_MAX;
    *a = A[n];
    *b = B[n];
}

int lsh_hint(const lsh_game *g)
{
    if (lsh_solved(g)) return -1;
    int on_way = g->nh <= g->nsol && !memcmp(g->hist, g->sol, (size_t)g->nh);
    int known = on_way ? g->nsol - g->nh : g->nh + g->nsol, a, b;
    lsh_depths(g->n, &a, &b);
    uint8_t path[LSH_SOL];
    if (known > 0 && lsh_solve(g->at, g->n, g->colour, a, b, known, path, LSH_SOL) > 0) return path[0];
    if (on_way && g->nh < g->nsol) return g->sol[g->nh];
    if (g->nh) return lsh_inverse(g->n, g->hist[g->nh - 1]);   /* back towards the start's known way */
    if (lsh_solve(g->at, g->n, g->colour, a, b, 0, path, LSH_SOL) > 0) return path[0];
    return -1;
}

void lsh_new(lsh_game *g, int variant, uint32_t seed)
{
    static const int DEPTH[LSH_VARIANTS] = {12, 8, 7, 7, 8, 7, 7, 7};
    if (variant < 0 || variant >= LSH_VARIANTS) variant = 0;
    int n = LSH_MIN + variant % 4, colour = variant >= 4, a, b;
    lsh_scramble(g, n, colour, DEPTH[variant], seed);
    lsh_depths(n, &a, &b);
    uint8_t path[LSH_SOL];
    int len = lsh_solve(g->start, n, colour, a, b, g->nsol, path, LSH_SOL);
    if (len > 0) {
        memcpy(g->sol, path, (size_t)len);
        g->nsol = g->par = len;
        g->exact = 1;
    } else {
        g->exact = g->nsol - 1 <= a + b;
    }
}

/* ---- the notation -------------------------------------------------------------------------------------------- */
int lsh_parse_move(int n, const char *s, int *used)
{
    const char *p = s;
    int col = *p == 'C' || *p == 'c';
    if (!col && *p != 'R' && *p != 'r') return -1;
    p++;
    int line = 0;
    if (*p < '1' || *p > '9') return -1;
    while (*p >= '0' && *p <= '9') line = line * 10 + *p++ - '0';
    int sign = *p == '-' ? -1 : *p == '+' ? 1 : 0;
    if (!sign) return -1;
    p++;
    int k = 0;
    if (*p < '0' || *p > '9') return -1;
    while (*p >= '0' && *p <= '9') k = k * 10 + *p++ - '0';
    if (line < 1 || line > n) return -1;
    k = ((sign * k) % n + n) % n;
    if (!k) return -1;
    if (used) *used = (int)(p - s);
    return lsh_move(line - 1 + (col ? n : 0), k);
}

int lsh_script(lsh_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        int used = 0, m = lsh_parse_move(g->n, s, &used);
        if (m < 0 || !lsh_play(g, m)) return 0;
        s += used;
    }
    return 1;
}

/* ---- the levels ---------------------------------------------------------------------------------------------- */
int lsh_parse(lsh_game *g, const char *s)
{
    if (!s || s[0] < '0' + LSH_MIN || s[0] > '0' + LSH_MAX || (s[1] != 'n' && s[1] != 'c') ||
        (s[2] != 'x' && s[2] != 'b') || s[3] != ':')
        return 0;
    int n = s[0] - '0', N = n * n, seen[LSH_CELLS] = {0};
    lsh_init(g, n, s[1] == 'c');
    g->exact = s[2] == 'x';
    const char *p = s + 4;
    for (int i = 0; i < N; i++, p++) {
        int v = *p >= 'a' && *p <= 'z' ? *p - 'a' : *p >= 'A' && *p <= 'Z' ? *p - 'A' + 26 : -1;
        if (v < 0 || v >= N || seen[v]++) return 0;
        g->at[i] = g->start[i] = (int8_t)v;
    }
    if (*p++ != ':') return 0;
    g->nsol = 0;
    while (p[0] && p[1] && g->nsol < LSH_SOL) {
        int line = p[0] - 'a', k = p[1] - '0';
        if (line < 0 || line >= 2 * n || k < 1 || k >= n) return 0;
        g->sol[g->nsol++] = (uint8_t)lsh_move(line, k);
        p += 2;
    }
    if (*p) return 0;
    g->par = g->nsol;
    return 1;
}

#ifndef LSH_GEN                                 /* not in the levels' tool (tools/loopshift_gen.c): it makes them */
#include "games/loopshift_levels.h"

int lsh_level(lsh_game *g, int level)
{
    return level >= 0 && level < LSH_LEVELS && lsh_parse(g, lsh_levels[level]);
}

int lsh_level_size(int level)
{
    return level >= 0 && level < LSH_LEVELS ? lsh_levels[level][0] - '0' : 0;
}
#endif
