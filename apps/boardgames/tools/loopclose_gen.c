/*
 * Close the Loops' levels: src/games/loopclose_levels.h. Our own boards: no published or commercial set is used.
 * Each chapter has its idea (small boards of plain loops; tees and crosses; ends; bigger boards; quarter-turn
 * mandalas on square boards; all of it on the largest boards) and makes many with the rules' maker (lcl_make, from
 * fixed seeds: a symmetric design of closed shapes, its tiles turned at random). A board is kept when it has the
 * chapter's feature and its design was not met before (turned or mirrored either). The difficulty: the tiles, the
 * fewest taps (the par, by the rules' exact search), less when many ways close it. Each chapter keeps 25 spread over
 * its difficulties (chapter 1 from the very easiest); of the boards near each place on the curve the one that looks
 * best is kept (the board well used, shapes of several kinds); the chapter's last level, its showpiece, the best
 * looking of its hardest few. Chapter 1 leaves some tiles as they are, so its first boards take a few taps. Deterministic
 * (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/lclgen apps/boardgames/tools/loopclose_gen.c -lm && \
 *       /tmp/lclgen > apps/boardgames/src/games/loopclose_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define LCL_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/loopclose_rules.c"

#define PER (LCL_LEVELS / LCL_CHAPTERS)
#define MAKES 500                               /* boards kept a chapter, at most */

typedef struct {
    char text[LCL_TEXT];
    int par, cells;
    double diff, look;
} cand;

/* a chapter: the sizes (w .. w + gw, h .. h + gh; square: h = w), the symmetries (bits), fill, walls, stubs; need:
 * 0 plain loops only, 1 tees or crosses, 2 ends, 3 any */
typedef struct { int w, gw, h, gh, square, syms, fill_lo, fill_hi, walls, stubs, need; } chapter;

static const chapter CH[LCL_CHAPTERS] = {
    {3, 2, 3, 3, 0, 3, 45, 70, 0, 0, 0},
    {4, 2, 5, 2, 0, 11, 45, 65, 30, 0, 1},
    {5, 2, 6, 2, 0, 11, 40, 60, 10, 30, 2},
    {7, 1, 8, 2, 0, 11, 40, 60, 15, 15, 3},
    {7, 2, 7, 2, 1, 4, 40, 60, 15, 10, 3},
    {9, 1, 10, 3, 0, 11, 40, 60, 15, 15, 3},
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

/* ---- near-duplicates: the design (each tile's kind, however turned) under the eight turns and mirrors ---------- */
static char (*kept)[LCL_TEXT];
static int nkept;

static int kind(int m)                          /* the smallest of its turns */
{
    int k = m;
    for (int r = 0; r < 4; r++, m = lcl_rot(m))
        if (m < k) k = m;
    return k;
}

static void form_of(const lcl_map *m, int tr, char *out)
{
    int w = tr & 4 ? m->h : m->w, h = tr & 4 ? m->w : m->h, n = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sx = tr & 4 ? y : x, sy = tr & 4 ? x : y;
            if (tr & 1) sx = m->w - 1 - sx;
            if (tr & 2) sy = m->h - 1 - sy;
            out[n++] = "0123456789abcdef"[kind(m->start[sx + sy * m->w])];
        }
        out[n++] = '|';
    }
    out[n] = 0;
}

static int met(const lcl_map *m)
{
    char l[8][LCL_TEXT];
    for (int tr = 0; tr < 8; tr++) form_of(m, tr, l[tr]);
    for (int i = 0; i < nkept; i++)
        for (int tr = 0; tr < 8; tr++)
            if (!strcmp(kept[i], l[tr])) return 1;
    strcpy(kept[nkept++], l[0]);
    return 0;
}

static int degree(int m) { return (m & 1) + (m >> 1 & 1) + (m >> 2 & 1) + (m >> 3 & 1); }

static int has(const lcl_map *m, int need, double *look)
{
    int n[5] = {0};
    for (int c = 0; c < m->w * m->h; c++) n[degree(m->start[c])]++;
    int kinds = (n[1] > 0) + (n[2] > 0) + (n[3] > 0) + (n[4] > 0);
    *look = 1.0 - (double)n[0] / (m->w * m->h) + 0.15 * kinds;
    if (need == 0) return !n[1] && !n[3] && !n[4];
    if (need == 1) return n[3] + n[4] >= 2 && !n[1];
    if (need == 2) return n[1] >= 2;
    return n[1] + n[3] + n[4] >= 3;
}

int main(void)
{
    kept = malloc((size_t)LCL_CHAPTERS * MAKES * 4 * sizeof *kept);
    static cand pool[MAKES];
    static cand out[LCL_LEVELS];
    printf("/* Close the Loops' levels: made by tools/loopclose_gen.c (our own designs; do not edit). Each level: its "
           "board\n * (rows from the top split by '|', a tile's arms at the start in hexadecimal: loopclose.h), its "
           "par: the fewest taps. */\n");
    for (int ch = 0; ch < LCL_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(5300u + (uint32_t)ch);
        int n = 0;
        for (int k = 0; k < 80 * MAKES && n < MAKES; k++) {
            int syms[4], ns = 0;
            for (int s = 0; s < 4; s++)
                if (C->syms >> s & 1) syms[ns++] = s;
            lcl_spec sp = {C->w + pr_below(&r, C->gw + 1), C->h + pr_below(&r, C->gh + 1), syms[pr_below(&r, ns)],
                           C->fill_lo + pr_below(&r, C->fill_hi - C->fill_lo + 1), C->walls, C->stubs};
            if (C->square) sp.h = sp.w;
            sp.mix = ch == 0 ? 35 + pr_below(&r, 66) : 0;   /* (the first boards: some tiles left as they are) */
            lcl_map m;
            if (!lcl_make(&m, &sp, pr_next(&r))) continue;
            double look;
            if (!has(&m, C->need, &look) || met(&m)) continue;
            long ways;
            int par = lcl_solve(m.w, m.h, m.start, NULL, &ways);
            lcl_text(&m, pool[n].text);
            pool[n].par = par;
            pool[n].cells = m.w * m.h;
            pool[n].diff = 0.3 * m.w * m.h + 0.8 * par - 2.0 * log10((double)ways);
            pool[n].look = look;
            n++;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d boards\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        int lo = ch == 0 ? 0 : n / 10;
        double step = (double)(n - 1 - lo) / (PER - 1);
        for (int k = 0, last = -1; k < PER; k++) {
            int mid = lo + (int)(k * step + 0.5), a = mid - (int)(step / 2), b = mid + (int)(step / 2), i = -1;
            if (k == 0 && ch == 0) a = b = 0;
            if (k == PER - 1) a = n - 1 - n / 25 - 2, b = n - 1;
            if (a < lo) a = lo;
            if (a <= last) a = last + 1;
            if (b > n - 1) b = n - 1;
            if (b < a) b = a;
            for (int j = a; j <= b; j++)
                if (i < 0 || pool[j].look > pool[i].look) i = j;
            last = i;
            out[ch * PER + k] = pool[i];
        }
        fprintf(stderr, "chapter %d: %d boards, par %d .. %d, squares %d .. %d\n", ch + 1, n, out[ch * PER].par,
                out[ch * PER + PER - 1].par, out[ch * PER].cells, out[ch * PER + PER - 1].cells);
    }
    printf("const lcl_level_t lcl_levels[LCL_LEVELS] = {\n");
    for (int l = 0; l < LCL_LEVELS; l++) printf("    {\"%s\", %d},\n", out[l].text, out[l].par);
    printf("};\n");
    return 0;
}
