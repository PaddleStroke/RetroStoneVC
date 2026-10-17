/*
 * Lights' levels, made by chance and chosen: src/games/lights_levels.h. Our own levels: no published set is used.
 * Each chapter has its sizes (3 x 3 up to 7 x 7) and its range of fewest presses. A candidate: presses drawn by
 * chance on a dark board (so it can always be turned off), often in a pattern (mirrored left to right, top to bottom,
 * both, or turned half or a quarter round: boards that look made); its fewest presses found by the rules' own solver
 * over GF(2) (lmp_solve); kept unless it is a turn or a mirror of one kept already. The difficulty: the fewest presses,
 * then the size, a little more when few lamps are lit for many presses. Each chapter keeps 25 candidates spread evenly
 * over its difficulties, from the easiest; the first level of all is a single press. Deterministic (fixed seeds).
 * Taste, so the boards look made rather than drawn: nine in ten candidates follow a pattern (the mirrors, the turns,
 * the diagonals too); a lit board is kept only when its lamps reach across the board (no strip left dark, the lamps
 * not pushed to one side, neither almost dark nor almost all lit); two levels in a row seldom share their pattern;
 * and each chapter ends (and has in its middle) a picture drawn by hand (a cross, a ring, a heart, a flower...) when
 * one of its size has its fewest presses within the chapter's range: the showpieces.
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
    int n, par, lit, sym, show;
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
    if (sym == 6 || sym == 7) c[k][0] = y, c[k++][1] = x;
    if (sym == 7) {
        c[k][0] = n - 1 - y, c[k++][1] = n - 1 - x;
        c[k][0] = n - 1 - x, c[k++][1] = n - 1 - y;
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

/* taste: the lit lamps reach across the board (on boards of 5 and more no dark strip at an edge deeper than one row
 * or column), the lamps' middle near the board's, between a fifth and seven tenths of the board lit */
static int tasteful(int n, const uint8_t *on)
{
    int lit = 0, x0 = n, x1 = -1, y0 = n, y1 = -1;
    double sx = 0, sy = 0;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            if (on[x + y * n]) {
                lit++, sx += x, sy += y;
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    if (!lit) return 0;
    double f = (double)lit / (n * n), mx = sx / lit - (n - 1) / 2.0, my = sy / lit - (n - 1) / 2.0;
    if (n == 3) return lit >= 2 && f <= 0.8;
    if (f < 0.2 || f > 0.7) return 0;
    int slack = n >= 5 ? 1 : 0;
    if (x0 > slack || y0 > slack || x1 < n - 1 - slack || y1 < n - 1 - slack) return 0;
    return mx * mx + my * my <= 0.04 * n * n;
}

/* the pictures drawn by hand ('1' lit): the showpieces, used when one has its fewest presses in a chapter's range */
static const char *const PICTURE[] = {
    "010|111|010", "101|010|101", "111|101|111", "101|000|101", "010|101|010",
    "0110|1111|1111|0110", "1001|0110|0110|1001", "1111|1001|1001|1111", "0110|1001|1001|0110", "1010|0101|1010|0101",
    "1001|1111|1111|1001", "0110|0110|1111|1111",
    "00100|00100|11111|00100|00100", "01110|10001|10001|10001|01110", "10001|01010|00100|01010|10001",
    "00100|01010|10001|01010|00100", "01010|11111|11111|01110|00100", "11011|10001|00100|10001|11011",
    "01010|10101|01010|10101|01010", "10101|01110|11011|01110|10101", "00100|01110|11111|01110|00100",
    "011110|100001|100001|100001|100001|011110", "011011|111111|111111|011110|001100|000000",
    "111111|100001|101101|101101|100001|111111", "001100|011110|110011|110011|011110|001100",
    "100001|010010|001100|001100|010010|100001", "010010|111111|010010|010010|111111|010010",
    "001100|001100|111111|111111|001100|001100", "110011|110011|001100|001100|110011|110011",
    "0110110|1111111|1111111|0111110|0011100|0001000|0000000",
    "1111111|1000001|1011101|1010101|1011101|1000001|1111111",
    "0001000|0010100|0101010|1010101|0101010|0010100|0001000",
    "1001001|0101010|0011100|1111111|0011100|0101010|1001001",
    "0011100|0100010|1010101|1000001|1011101|0100010|0011100",
    "0010100|0111110|1101011|0111110|1101011|0111110|0010100",
    "0001000|0011100|0111110|1111111|0011100|0011100|0011100",
    "1100011|1110111|0111110|0011100|0111110|1110111|1100011",
};
#define NPICTURE ((int)(sizeof PICTURE / sizeof PICTURE[0]))

static int picture(const char *rows, uint8_t *on)
{
    int n = (int)strcspn(rows, "|"), k = 0;
    memset(on, 0, LMP_CELLS);
    for (const char *r = rows; *r; r++)
        if (*r != '|') on[k++] = (uint8_t)(*r - '0');
    return k == n * n ? n : 0;
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
            int sym = pr_below(&rng, 20);       /* 0: none; 1..7: a pattern (nine times in ten) */
            sym = sym < 2 ? 0 : sym < 6 ? 1 : sym < 8 ? 2 : sym < 10 ? 3
                : sym < 13 ? 4 : sym < 15 ? 5 : sym < 18 ? 6 : 7;
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
            if (par < CH[ch].lo || par > CH[ch].hi || lit < 2 || !tasteful(n, on)) continue;
            cand c;
            memset(&c, 0, sizeof c);
            c.n = n;
            c.par = par;
            c.lit = lit;
            c.sym = sym;
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
        /* the showpieces: the hand-drawn pictures of the chapter's sizes within its range, the most presses first */
        cand show[NPICTURE];
        int nshow = 0;
        for (int p = 0; p < NPICTURE; p++) {
            uint8_t on[LMP_CELLS];
            int n = picture(PICTURE[p], on);
            if (n != CH[ch].n[0] && n != CH[ch].n[1]) continue;
            int par = lmp_solve(n, on, NULL);
            if (par < CH[ch].lo || par > CH[ch].hi) continue;
            cand c;
            memset(&c, 0, sizeof c);
            c.n = n, c.par = par, c.show = 1, c.sym = -1;
            for (int i = 0; i < n * n; i++) c.lit += on[i];
            c.diff = par + 1.5 * (n - 3);
            canon(n, on, c.key);
            int dup = known(c.key, kept, nkept);
            for (int i = 0; i < nshow; i++) dup |= !strcmp(show[i].key, c.key);
            if (dup) continue;
            strcpy(c.rows, PICTURE[p]);
            show[nshow++] = c;
        }
        for (int i = 1; i < nshow; i++)
            for (int j = i; j > 0 && show[j - 1].par < show[j].par; j--) {
                cand t = show[j - 1];
                show[j - 1] = show[j];
                show[j] = t;
            }
        fprintf(stderr, "chapter %d: %d pictures\n", ch + 1, nshow);
        static int used[POOL];
        memset(used, 0, sizeof used);
        int first = nkept;
        for (int i = 0, last = -1; i < PER; i++) {
            if ((i == PER - 1 && nshow > 0) || (i == PER / 2 && nshow > 1)) {   /* a showpiece: the end, the middle */
                out[nkept] = show[i == PER - 1 ? 0 : 1];
                strcpy(kept[nkept], out[nkept].key);
                nkept++;
                continue;
            }
            double q = ch == 0 && i == 0 ? 0 : 0.03 + 0.94 * i / (PER - 1);
            int at = (int)(q * (npool - 1) + 0.5);
            if (at <= last) at = last + 1;      /* never the same one twice */
            if (at >= npool) at = npool - 1;
            /* not the pattern of the level before: the next one of a near difficulty that differs */
            int prev = nkept > first ? out[nkept - 1].sym : -2;
            for (int d = 0; d < 6 && at + d < npool; d++)
                if (!used[at + d] && pool[at + d].sym != prev && pool[at + d].diff <= pool[at].diff + 0.6) {
                    at += d;
                    break;
                }
            while (used[at] && at + 1 < npool) at++;
            used[at] = 1;
            last = at;
            out[nkept] = pool[at];
            strcpy(kept[nkept++], pool[at].key);
        }
        /* the showpieces in their place by presses: the last stays the chapter's end when it is among its hardest */
        int top = 0;
        for (int i = first; i < nkept; i++)
            if (out[i].par > top) top = out[i].par;
        for (int i = first + 1; i < nkept; i++) {
            if (!out[i].show || (i == nkept - 1 && out[i].par * 5 >= top * 4)) continue;
            cand t = out[i];
            int j = i;
            for (; j > first + 1 && out[j - 1].par > t.par; j--) out[j] = out[j - 1];
            for (; j < nkept - 1 && out[j + 1].par <= t.par && !(j + 1 == nkept - 1 && out[j + 1].show); j++)
                out[j] = out[j + 1];
            out[j] = t;
        }
        for (int i = first; i < nkept; i++) strcpy(kept[i], out[i].key);
    }
    printf("/*\n"
           " * Lights' levels: our own, made by tools/lights_gen.c (presses drawn by chance, often in a pattern, the fewest\n"
           " * presses found by the rules' solver; six chapters of 25 from the easiest, a few pictures drawn by hand).\n"
           " * Each: the rows of lamps ('1' lit), then (in the comment) its fewest presses and the lamps lit; the fewest\n"
           " * presses again in lmp_level_par (the tests check them).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "const char *const lmp_levels[LMP_LEVELS] = {\n");
    for (int i = 0; i < LMP_LEVELS; i++)
        printf("    \"%s\", /* %d: %d presses, %d lit%s */\n", out[i].rows, i + 1, out[i].par, out[i].lit,
               out[i].show ? ", a picture" : "");
    printf("};\n\nconst uint8_t lmp_level_par[LMP_LEVELS] = {");
    for (int i = 0; i < LMP_LEVELS; i++) printf("%s%d,", i % 25 ? " " : "\n    ", out[i].par);
    printf("\n};\n");
    return 0;
}
