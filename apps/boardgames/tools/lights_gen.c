/*
 * Lights' levels, made by chance and chosen: src/games/lights_levels.h. Our own levels: no published set is used.
 * Each chapter has its sizes (3 x 3 up to 7 x 7) and its range of fewest presses. A candidate: presses drawn by
 * chance on a dark board (so it can always be turned off), often in a pattern (mirrored left to right, top to bottom,
 * both, or turned half or a quarter round: boards that look made); its fewest presses found by the rules' own solver
 * over GF(2) (lmp_solve); kept unless it is a turn or a mirror of one kept already. The difficulty: the fewest presses,
 * then the size, a little more when few lamps are lit for many presses. Each chapter keeps 25 candidates spread evenly
 * over its difficulties, from the easiest; the first level of all is a single press. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/lmpgen apps/boardgames/tools/lights_gen.c && \
 *       /tmp/lmpgen > apps/boardgames/src/games/lights_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define LMP_GEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/lights_rules.c"

#define PER (LMP_LEVELS / LMP_CHAPTERS)
#define POOL 1200
#define TRIES 400000

static const struct { int n[2], lo, hi; } CH[LMP_CHAPTERS] = {
    {{3, 3}, 1, 5}, {{4, 4}, 3, 8}, {{5, 5}, 4, 10}, {{5, 6}, 8, 15}, {{6, 6}, 10, 20}, {{7, 7}, 12, 28},
};

typedef struct {
    int n, par, lit;
    double diff;
    char rows[LMP_CELLS + LMP_MAX + 1], key[LMP_CELLS + 4];
} cand;

static cand pool[POOL];
static int npool;
static uint32_t rng;

/* the key a board shares with its turns and mirrors: the smallest of their texts */
static void canon(int n, const uint8_t *on, char *key)
{
    char best[LMP_CELLS + 4] = {0}, s[LMP_CELLS + 4];
    for (int t = 0; t < 8; t++) {
        int k = 0;
        s[k++] = (char)('0' + n);
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int ux = t & 4 ? y : x, uy = t & 4 ? x : y;
                if (t & 1) ux = n - 1 - ux;
                if (t & 2) uy = n - 1 - uy;
                s[k++] = (char)('0' + on[ux + uy * n]);
            }
        s[k] = 0;
        if (!best[0] || strcmp(s, best) < 0) memcpy(best, s, (size_t)k + 1);
    }
    strcpy(key, best);
}

/* the places a press at (x, y) stands for in pattern `sym`: itself and its images */
static int images(int n, int sym, int x, int y, int *out)
{
    int k = 0, c[8][2] = {{x, y}};
    k = 1;
    if (sym == 1 || sym == 3) c[k][0] = n - 1 - x, c[k++][1] = y;
    if (sym == 2 || sym == 3) c[k][0] = x, c[k++][1] = n - 1 - y;
    if (sym == 3) c[k][0] = n - 1 - x, c[k++][1] = n - 1 - y;
    if (sym == 4 || sym == 5) c[k][0] = n - 1 - x, c[k++][1] = n - 1 - y;
    if (sym == 5) {
        c[k][0] = n - 1 - y, c[k++][1] = x;
        c[k][0] = y, c[k++][1] = n - 1 - x;
    }
    int m = 0;
    for (int i = 0; i < k; i++) {
        int p = c[i][0] + c[i][1] * n, dup = 0;
        for (int j = 0; j < m; j++) dup |= out[j] == p;
        if (!dup) out[m++] = p;
    }
    return m;
}

static int known(const char *key, char (*keys)[LMP_CELLS + 4], int nkeys)
{
    for (int i = 0; i < npool; i++)
        if (!strcmp(pool[i].key, key)) return 1;
    for (int i = 0; i < nkeys; i++)
        if (!strcmp(keys[i], key)) return 1;
    return 0;
}

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0;
}

int main(void)
{
    static char kept[LMP_LEVELS][LMP_CELLS + 4];
    static cand out[LMP_LEVELS];
    int nkept = 0;
    for (int ch = 0; ch < LMP_CHAPTERS; ch++) {
        rng = pr_seed(2000u + (uint32_t)ch);
        npool = 0;
        for (int tries = 0; tries < TRIES && npool < POOL; tries++) {
            int n = CH[ch].n[pr_below(&rng, 2)], N = n * n;
            int sym = pr_below(&rng, 10);       /* 0: none; 1..5: a pattern (more often than not) */
            sym = sym < 4 ? 0 : sym < 6 ? 1 : sym < 7 ? 2 : sym < 8 ? 3 : sym < 9 ? 4 : 5;
            int presses = 1 + pr_below(&rng, CH[ch].hi + 2);
            uint8_t on[LMP_CELLS] = {0}, pressed[LMP_CELLS] = {0};
            for (int k = 0; k < presses; k++) {
                int c = pr_below(&rng, N), im[8], m = images(n, sym, c % n, c / n, im);
                for (int i = 0; i < m; i++) pressed[im[i]] ^= 1;
            }
            for (int c = 0; c < N; c++)
                if (pressed[c]) lmp_toggle(n, on, c);
            int par = lmp_solve(n, on, NULL), lit = 0;
            for (int c = 0; c < N; c++) lit += on[c];
            if (par < CH[ch].lo || par > CH[ch].hi || lit < 2) continue;
            cand c;
            memset(&c, 0, sizeof c);
            c.n = n;
            c.par = par;
            c.lit = lit;
            c.diff = par + 1.5 * (n - 3) + 0.15 * (par - lit / 3.0 > 0 ? par - lit / 3.0 : 0);
            canon(n, on, c.key);
            if (known(c.key, kept, nkept)) continue;
            int k = 0;
            for (int y = 0; y < n; y++) {
                if (y) c.rows[k++] = '|';
                for (int x = 0; x < n; x++) c.rows[k++] = (char)('0' + on[x + y * n]);
            }
            c.rows[k] = 0;
            pool[npool++] = c;
        }
        qsort(pool, (size_t)npool, sizeof *pool, by_diff);
        fprintf(stderr, "chapter %d: %d candidates, difficulty %.1f .. %.1f\n", ch + 1, npool, pool[0].diff,
                pool[npool - 1].diff);
        for (int i = 0, last = -1; i < PER; i++) {
            double q = ch == 0 && i == 0 ? 0 : 0.03 + 0.94 * i / (PER - 1);
            int at = (int)(q * (npool - 1) + 0.5);
            if (at <= last) at = last + 1;      /* never the same one twice */
            if (at >= npool) at = npool - 1;
            last = at;
            out[nkept] = pool[at];
            strcpy(kept[nkept++], pool[at].key);
        }
    }
    printf("/*\n"
           " * Lights' levels: our own, made by tools/lights_gen.c (presses drawn by chance, often in a pattern, the fewest\n"
           " * presses found by the rules' solver; six chapters of 25 from the easiest). Each: the rows of lamps ('1' lit),\n"
           " * then (in the comment) its fewest presses and the lamps lit; the fewest presses again in lmp_level_par (the\n"
           " * tests check them).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "const char *const lmp_levels[LMP_LEVELS] = {\n");
    for (int i = 0; i < LMP_LEVELS; i++)
        printf("    \"%s\", /* %d: %d presses, %d lit */\n", out[i].rows, i + 1, out[i].par, out[i].lit);
    printf("};\n\nconst uint8_t lmp_level_par[LMP_LEVELS] = {");
    for (int i = 0; i < LMP_LEVELS; i++) printf("%s%d,", i % 25 ? " " : "\n    ", out[i].par);
    printf("\n};\n");
    return 0;
}
