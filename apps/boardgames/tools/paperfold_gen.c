/*
 * Paper Fold's levels: src/games/paperfold_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of sheet (plain small sheets, a patterned front, cut-outs, both sides patterned, cut-outs in
 * larger patterned sheets, all of it on the largest sheets) and makes many with the rules' maker (pfo_make, from
 * fixed seeds: a sheet of symmetric patterns and symmetric cut-outs, folded at random, the paper left never a sliver;
 * the picture left, its par found by the search). A sheet is kept when its par is in the chapter's range, when it has
 * the chapter's feature, and when it was not met before (turned or mirrored either). The difficulty: the fewest
 * folds, more when there are few shortest ways, a little more for the states the search went through. Each chapter keeps 25 spread over its difficulties (chapter 1 from the very
 * easiest: one fold); of the sheets near each place on the curve the one that looks best is kept (a picture of several
 * colours, mirrored or turned into itself, the board well used); the chapter's last level, its showpiece, the best
 * looking of its hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/pfogen apps/boardgames/tools/paperfold_gen.c -lm && \
 *       /tmp/pfogen > apps/boardgames/src/games/paperfold_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define PFO_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/paperfold_rules.c"

#define PER (PFO_LEVELS / PFO_CHAPTERS)
#define MAKES 320                               /* sheets kept a chapter, at most */

typedef struct {
    char sheet[PFO_TEXT], picture[PFO_TEXT];
    int par;
    double diff, look;
} cand;

/* a chapter: the sheet's size w x h (up to grow more each way), the patterns (-1 any, -2 any but plain), the cuts, the
 * folds made, the colours; lo..hi the folds of the par; need: 1 a patterned front, 2 cut-outs, 3 a patterned back */
typedef struct { int w, h, grow, front, back, cuts, folds, colours, lo, hi, need; } chapter;

static const chapter CH[PFO_CHAPTERS] = {
    {3, 3, 1, 0, 0, 0, 2, 2, 1, 2, 0},          /* plain sheets of two colours, small */
    {4, 4, 1, -2, 0, 0, 3, 2, 2, 3, 1},         /* a patterned front */
    {4, 4, 1, -1, 0, 2, 3, 3, 2, 3, 2},         /* cut-outs */
    {4, 4, 1, -2, -2, 0, 4, 3, 3, 4, 3},        /* both sides patterned */
    {5, 5, 0, -2, -1, 2, 4, 4, 3, 4, 2},        /* cut-outs in patterned sheets, larger */
    {5, 5, 1, -2, -2, 2, 6, 4, 4, 6, 3},        /* all of it, the largest */
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->sheet, ((const cand *)b)->sheet);
}

/* ---- near-duplicates: the sheet and picture under the eight turns and mirrors -------------------------------- */
static char (*kept)[2 * PFO_TEXT + 4];
static int nkept;

static void form_of(const pfo_map *m, int tr, char *out)
{
    int w = tr & 4 ? m->h : m->w, h = tr & 4 ? m->w : m->h, n = 0;
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int sx = tr & 4 ? y : x, sy = tr & 4 ? x : y;
                if (tr & 1) sx = m->w - 1 - sx;
                if (tr & 2) sy = m->h - 1 - sy;
                int c = sx + sy * m->w;
                uint8_t v = pass ? m->target[c] : m->start.cell[c];
                if (v == PFO_NONE) out[n++] = '.';
                else if (pass) out[n++] = (char)('A' + v);
                else { out[n++] = (char)('A' + pfo_up(v)); out[n++] = (char)('a' + pfo_down(v)); }
            }
            out[n++] = '|';
        }
        out[n++] = '/';
    }
    out[n] = 0;
}

static int met(const pfo_map *m)
{
    char l[8][2 * PFO_TEXT + 4];
    for (int tr = 0; tr < 8; tr++) form_of(m, tr, l[tr]);
    for (int i = 0; i < nkept; i++)
        for (int tr = 0; tr < 8; tr++)
            if (!strcmp(kept[i], l[tr])) return 1;
    strcpy(kept[nkept++], l[0]);
    return 0;
}

/* ---- the chapter's feature, the looks ------------------------------------------------------------------------ */
static int has(const pfo_map *m, int need)
{
    int fr = 0, bk = 0, holes = 0, f0 = -1, b0 = -1;
    for (int c = 0; c < m->w * m->h; c++) {
        uint8_t v = m->start.cell[c];
        if (v == PFO_NONE) { holes++; continue; }
        if (f0 < 0) { f0 = pfo_up(v); b0 = pfo_down(v); }
        fr |= pfo_up(v) != f0;
        bk |= pfo_down(v) != b0;
    }
    if (need == 0) return !fr && !bk && !holes;
    if (need == 1) return fr;
    if (need == 2) return holes > 0;
    return fr && bk;
}

/* a picture that looks designed: several colours, mirrored or turned into itself, the paper left not a sliver */
static double look_of(const pfo_map *m)
{
    int w = m->w, h = m->h, used[PFO_COLOURS] = {0}, paper = 0, mirx = 1, miry = 1, half = 1;
    for (int c = 0; c < w * h; c++) {
        uint8_t t = m->target[c];
        int x = c % w, y = c / w;
        if (t != PFO_NONE) { used[t] = 1; paper++; }
        mirx &= t == m->target[w - 1 - x + y * w];
        miry &= t == m->target[x + (h - 1 - y) * w];
        half &= t == m->target[w - 1 - x + (h - 1 - y) * w];
    }
    int colours = 0;
    for (int i = 0; i < PFO_COLOURS; i++) colours += used[i];
    double sym = (mirx || miry ? 0.6 : 0) + (half ? 0.4 : 0);
    return 0.5 * (colours > 1) + 0.25 * (colours > 2) + sym * 0.5 + 0.5 * paper / (w * h) -
           (paper < 2 ? 1 : 0);
}

int main(void)
{
    kept = malloc((size_t)PFO_CHAPTERS * MAKES * 40 * sizeof *kept);
    static cand pool[MAKES];
    static cand out[PFO_LEVELS];
    printf("/* Paper Fold's levels: made by tools/paperfold_gen.c (our own sheets, found by its search; do not edit).\n"
           " * Each level: its sheet (two characters a square: the front, the back), the picture to make, the par: the "
           "fewest\n * folds (paperfold.h). */\n");
    for (int ch = 0; ch < PFO_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(7100u + (uint32_t)ch);
        int n = 0;
        for (int k = 0; k < 40 * MAKES && n < MAKES; k++) {
            int g = pr_below(&r, C->grow + 1), gh = pr_below(&r, C->grow + 1);
            int front = C->front == -2 ? 1 + pr_below(&r, PATTERNS - 1) : C->front;
            int back = C->back == -2 ? 1 + pr_below(&r, PATTERNS - 1) : C->back;
            pfo_spec sp = {C->w + g, C->h + gh, front, back, C->cuts ? 1 + pr_below(&r, C->cuts) : 0,
                           C->lo + pr_below(&r, C->folds - C->lo + 1), C->colours};
            if (sp.w > PFO_MAXW) sp.w = PFO_MAXW;
            if (sp.h > PFO_MAXH) sp.h = PFO_MAXH;
            pfo_map m;
            pfo_make(&m, &sp, pr_next(&r));
            int states, par = pfo_solve(&m, &m.start, NULL, &states);
            if (par < C->lo || par > C->hi || !has(&m, C->need) || met(&m)) continue;
            int ways = pfo_ways(&m, 1000);
            pfo_text(&m, pool[n].sheet, pool[n].picture);
            pool[n].par = par;
            pool[n].diff = par + 1.2 / sqrt((double)(ways > 0 ? ways : 1)) + states / 30000.0 + m.w * m.h / 60.0;
            pool[n].look = look_of(&m);
            n++;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d sheets\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        int lo = ch == 0 ? 0 : n / 8;
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
        fprintf(stderr, "chapter %d: %d sheets, folds %d .. %d\n", ch + 1, n, out[ch * PER].par,
                out[ch * PER + PER - 1].par);
    }
    printf("const pfo_level_t pfo_levels[PFO_LEVELS] = {\n");
    for (int l = 0; l < PFO_LEVELS; l++)
        printf("    {\"%s\", \"%s\", %d},\n", out[l].sheet, out[l].picture, out[l].par);
    printf("};\n");
    return 0;
}
