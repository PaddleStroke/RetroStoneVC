/*
 * Lights: the rules (lights.h): the presses, the fewest way over GF(2), the boards made from presses, undo. No SDL:
 * tested by tests/lights_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/lights.h"

#include <string.h>

#include "games/puzrand.h"

static const int LOW[LMP_VARIANTS] = {4, 9, 16}, HIGH[LMP_VARIANTS] = {6, 15, 30};

int lmp_size(int variant) { return variant <= 0 ? 3 : variant == 1 ? 5 : 7; }

void lmp_toggle(int n, uint8_t *on, int c)
{
    int x = c % n, y = c / n;
    on[c] ^= 1;
    if (x > 0) on[c - 1] ^= 1;
    if (x < n - 1) on[c + 1] ^= 1;
    if (y > 0) on[c - n] ^= 1;
    if (y < n - 1) on[c + n] ^= 1;
}

/* the presses as a matrix over GF(2): row i (a lamp) has bit j when press j turns lamp i over; the lamps lit are the
 * column after the presses' (bit N). Eliminated to reduced rows; the free presses (no pivot) give the other ways. */
int lmp_solve(int n, const uint8_t *on, uint8_t *press)
{
    int N = n * n;
    uint64_t row[LMP_CELLS];
    for (int i = 0; i < N; i++) {
        uint8_t t[LMP_CELLS] = {0};
        lmp_toggle(n, t, i);                    /* symmetric: press i turns lamp j iff press j turns lamp i */
        row[i] = (uint64_t)(on[i] & 1) << N;
        for (int j = 0; j < N; j++)
            if (t[j]) row[i] |= 1ull << j;
    }
    int pivot_of[LMP_CELLS], r = 0;
    for (int c = 0; c < N; c++) {
        pivot_of[c] = -1;
        int p = -1;
        for (int i = r; i < N && p < 0; i++)
            if (row[i] >> c & 1) p = i;
        if (p < 0) continue;
        uint64_t t = row[p];
        row[p] = row[r];
        row[r] = t;
        for (int i = 0; i < N; i++)
            if (i != r && (row[i] >> c & 1)) row[i] ^= row[r];
        pivot_of[c] = r++;
    }
    for (int i = r; i < N; i++)
        if (row[i] >> N & 1) return -1;         /* 0 = 1: no way */
    int freev[LMP_CELLS], nf = 0;
    for (int c = 0; c < N; c++)
        if (pivot_of[c] < 0) freev[nf++] = c;
    int best = -1;
    uint64_t best_x = 0;
    for (uint32_t m = 0; m < (1u << nf); m++) { /* every value of the free presses: a way each */
        uint64_t x = 0;
        for (int k = 0; k < nf; k++)
            if (m >> k & 1) x |= 1ull << freev[k];
        for (int c = 0; c < N; c++) {
            if (pivot_of[c] < 0) continue;
            uint64_t rw = row[pivot_of[c]];
            int v = (int)(rw >> N & 1);
            for (int k = 0; k < nf; k++)
                if ((rw >> freev[k] & 1) && (x >> freev[k] & 1)) v ^= 1;
            if (v) x |= 1ull << c;
        }
        int w = __builtin_popcountll(x);
        if (best < 0 || w < best) { best = w; best_x = x; }
    }
    if (press)
        for (int c = 0; c < N; c++) press[c] = (uint8_t)(best_x >> c & 1);
    return best;
}

void lmp_make(int variant, uint32_t seed, uint8_t *on)
{
    if (variant < 0 || variant >= LMP_VARIANTS) variant = 1;
    int n = lmp_size(variant);
    uint32_t s = pr_seed(seed);
    for (;;) {
        memset(on, 0, LMP_CELLS);
        for (int c = 0; c < n * n; c++)
            if (pr_below(&s, 2)) lmp_toggle(n, on, c);
        int k = lmp_solve(n, on, NULL);
        if (k >= LOW[variant] && k <= HIGH[variant]) return;
    }
}

void lmp_load(lmp_game *g, int n, const uint8_t *on)
{
    memset(g, 0, sizeof *g);
    g->n = n;
    memcpy(g->on, on, (size_t)n * n);
    g->best = lmp_solve(n, on, NULL);
}

void lmp_new(lmp_game *g, int variant, uint32_t seed)
{
    uint8_t on[LMP_CELLS];
    lmp_make(variant, seed, on);
    lmp_load(g, lmp_size(variant < 0 || variant >= LMP_VARIANTS ? 1 : variant), on);
}

int lmp_parse(lmp_game *g, const char *rows)
{
    uint8_t on[LMP_CELLS];
    int n = 0, k = 0, y = 0;
    while (rows[n] && rows[n] != '|') n++;
    if (n < 2 || n > LMP_MAX) return 0;
    for (const char *p = rows;; p++) {
        if (*p == '0' || *p == '1') {
            if (k >= n || y >= n) return 0;
            on[k++ + y * n] = (uint8_t)(*p - '0');
            continue;
        }
        if (*p != '|' && *p) return 0;
        if (k != n) return 0;
        y++;
        k = 0;
        if (!*p) break;
    }
    if (y != n) return 0;
    lmp_load(g, n, on);
    return 1;
}

#ifndef LMP_GEN                                 /* not in the levels' tool (tools/lights_gen.c): it makes them */
#include "games/lights_levels.h"

int lmp_level(lmp_game *g, int level)
{
    return level >= 0 && level < LMP_LEVELS && lmp_parse(g, lmp_levels[level]);
}
#endif

int lmp_press(lmp_game *g, int c)
{
    if (c < 0 || c >= g->n * g->n) return 0;
    if (g->nh >= LMP_HIST) {                    /* forget the oldest half */
        memmove(g->hist, g->hist + LMP_HIST / 2, sizeof g->hist / 2);
        g->nh -= LMP_HIST / 2;
    }
    lmp_toggle(g->n, g->on, c);
    g->hist[g->nh++] = (int16_t)c;
    g->moves++;
    return 1;
}

int lmp_undo(lmp_game *g)
{
    if (!g->nh) return -1;
    int c = g->hist[--g->nh];
    lmp_toggle(g->n, g->on, c);
    g->moves--;
    return c;
}

int lmp_solved(const lmp_game *g) { return lmp_lit(g) == 0; }

int lmp_lit(const lmp_game *g)
{
    int k = 0;
    for (int c = 0; c < g->n * g->n; c++) k += g->on[c];
    return k;
}

int lmp_hint(const lmp_game *g)
{
    uint8_t press[LMP_CELLS];
    if (lmp_solved(g) || lmp_solve(g->n, g->on, press) <= 0) return -1;
    for (int c = 0; c < g->n * g->n; c++)       /* the first lit row's: the press nearest the top */
        if (press[c]) return c;
    return -1;
}

int lmp_script(lmp_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h' || *s == '=') {
            int all = *s++ == '=';
            do {
                int c = lmp_hint(g);
                if (c < 0) break;
                lmp_press(g, c);
            } while (all);
            continue;
        }
        if (*s < 'a' || *s >= 'a' + g->n) return 0;
        int x = *s++ - 'a', y = 0;
        if (*s < '1' || *s > '9') return 0;
        while (*s >= '0' && *s <= '9') y = y * 10 + *s++ - '0';
        if (y < 1 || y > g->n) return 0;
        lmp_press(g, x + (y - 1) * g->n);
    }
    return 1;
}
