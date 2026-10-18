/*
 * Colour Mixer's levels: src/games/colourmix_levels.h. Our own colours: no published or commercial set is used. Each
 * chapter has its idea (two colours; three; white and black, tints and shades; a pot that holds paint already; premixed
 * tubes; tubes holding few drops) and makes many boards with the rules' maker (cmx_make, from fixed seeds: a recipe
 * drawn among the tubes offered, kept only when no fewer drops reach its colour and no other recipe of as few drops
 * comes near it). A board is kept when its colour is not too near one the chapter has already. Each is measured: the
 * drops, the tubes the recipe uses, the tubes offered, the near misses a little more drops away, and two greedy
 * players (cmx_greedy): the obvious way (each drop from the tube that brings the pot nearest, as the meter shows), and
 * one looking two drops ahead and taking drops back. Each chapter keeps 25: its opening (the first chapter's three, of
 * 2 and 3 drops; the others' two), the easiest; then boards the obvious way does not mix (a drop that first takes the
 * pot further away), spread over their difficulties, the chapter's last few (more each chapter) ones even the
 * two-drop player misses at par; its last level, its showpiece, of the hardest few the most colourful. The per-level
 * table and the chapters' summary go to stderr (-DREPORT: the same for the levels in colourmix_levels.h).
 * Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/cmxgen apps/boardgames/tools/colourmix_gen.c -lm && \
 *       /tmp/cmxgen > apps/boardgames/src/games/colourmix_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define CMX_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/colourmix_rules.c"

#define PER (CMX_LEVELS / CMX_CHAPTERS)
#define MAKES 700
#define SPECS 4
#define R_ (1 << CMX_RED)
#define Y_ (1 << CMX_YELLOW)
#define B_ (1 << CMX_BLUE)
#define W_ (1 << CMX_WHITE)
#define K_ (1 << CMX_BLACK)
#define O_ (1 << CMX_ORANGE)
#define G_ (1 << CMX_GREEN)
#define V_ (1 << CMX_VIOLET)
#define N_ (1 << CMX_BROWN)
#define P_ (1 << CMX_PINK)

/* a level's difficulty: the drops, the tubes its recipe uses, the tubes offered, the near misses, whether a greedy
 * player mixes it adding a drop at a time (gadd), or looking two drops ahead and taking drops back too (gback), and
 * the drops that one ends with */
typedef struct { int par, kinds, tubes, rivals, gback, gadd, gdrops; } metrics;

typedef struct {
    char text[CMX_TEXT];
    int par, kinds;
    float rgb[3];
    double diff, look;
    int hard;                                   /* no greedy player mixes it, and its par is 3 or more */
    metrics x;
} cand;

/* a chapter: its kinds of board, the boards tried, and past its opening, how many of its levels (from its end) even
 * the greedy player looking two drops ahead must miss at par */
typedef struct { cmx_spec s[SPECS]; int tries, deep; } chapter;

static const chapter CH[CMX_CHAPTERS] = {
    {{{R_ | Y_, 2, 3, 2, 2, 0, 0, 0}, {Y_ | B_, 2, 4, 2, 2, 0, 0, 0}, {R_ | B_, 2, 4, 2, 2, 0, 0, 0},
      {R_ | Y_ | B_ | W_, 2, 6, 2, 2, 0, 0, 0}}, 4000, 0},
    {{{R_ | Y_ | B_, 3, 6, 3, 3, 0, 0, 0}, {R_ | Y_ | B_ | W_, 3, 6, 3, 3, 0, 0, 0},
      {R_ | Y_ | B_ | W_, 4, 7, 3, 3, 0, 0, 0}, {R_ | Y_ | B_ | W_, 3, 7, 3, 3, 0, 0, 0}}, 5000, 4},
    {{{R_ | Y_ | B_ | W_ | K_, 2, 5, 2, 2, 0, 0, W_}, {R_ | Y_ | B_ | W_ | K_, 3, 7, 2, 3, 0, 0, W_},
      {R_ | Y_ | B_ | W_ | K_, 4, 8, 3, 3, 0, 0, W_}, {R_ | Y_ | B_ | W_ | K_, 3, 8, 2, 4, 0, 0, W_}}, 5000, 8},
    {{{R_ | Y_ | B_ | W_, 3, 5, 1, 2, 2, 0, 0}, {R_ | Y_ | B_ | W_ | K_, 3, 6, 1, 2, 3, 0, 0},
      {R_ | Y_ | B_ | W_ | K_, 4, 7, 2, 3, 3, 0, 0}, {R_ | Y_ | B_ | W_ | K_, 4, 8, 2, 3, 4, 0, 0}}, 5000, 12},
    {{{R_ | Y_ | B_ | W_ | O_ | G_ | V_, 3, 7, 2, 3, 0, 0, O_}, {R_ | Y_ | B_ | W_ | K_ | N_ | P_, 4, 8, 2, 3, 0, 0, O_},
      {R_ | Y_ | B_ | W_ | O_ | G_ | P_, 4, 8, 2, 3, 0, 0, O_}, {R_ | Y_ | B_ | K_ | O_ | V_ | N_, 4, 9, 3, 4, 0, 0, O_}}, 5000, 16},
    {{{R_ | Y_ | B_ | W_ | K_, 4, 8, 3, 3, 0, 1, 0}, {R_ | Y_ | B_ | W_ | K_ | O_ | G_, 4, 9, 3, 4, 0, 2, 0},
      {R_ | Y_ | B_ | W_ | K_ | V_ | P_, 5, 10, 3, 4, 0, 2, 0}, {R_ | Y_ | B_ | W_ | K_ | O_ | N_, 5, 12, 3, 4, 2, 2, 0}},
     5000, 22},
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

/* ---- the difficulty metrics: greedy players ------------------------------------------------------------------ */

static void measure(const cmx_map *m, int par, metrics *x)
{
    x->par = par;
    x->kinds = 0;
    for (int i = 0; i < m->n; i++) x->kinds += m->recipe[i] > 0;
    x->tubes = m->n;
    x->rivals = cmx_rivals(m, par + 2, 4 * CMX_TOL);
    x->gback = cmx_greedy(m, par, 1, 2, &x->gdrops);
    x->gadd = cmx_greedy(m, par, 0, 1, NULL);
}

/* trivial: the obvious way (a drop at a time from the tube that brings the pot nearest) mixes it, or two drops do */
static int trivial(const metrics *x) { return x->gadd || x->par <= 2; }

/* deep: even the greedy player looking two drops ahead, taking drops back, misses it at par */
static int deep(const metrics *x) { return !x->gback || x->gdrops > x->par; }

static void table(const metrics *x, int n, int per)
{
    fprintf(stderr, "lvl par kinds tubes rivals greedy1 greedy2 (drops)\n");
    for (int l = 0; l < n; l++)
        fprintf(stderr, "%4d %3d %5d %5d %6d %10d %8d (%d)\n", l + 1, x[l].par, x[l].kinds, x[l].tubes, x[l].rivals,
                x[l].gadd, x[l].gback, x[l].gdrops);
    fprintf(stderr, "chapter: par mean (min..max), kinds mean, solved by the greedy players (one drop / two drops, "
                    "taking back); trivial (one drop at a time solves it, or par <= 2); deep (two drops miss par)\n");
    for (int c = 0; c * per < n; c++) {
        double p = 0, k = 0;
        int ga = 0, gb = 0, t = 0, cnt = 0, lo = 99, hi = 0, dp = 0;
        for (int l = c * per; l < n && l < (c + 1) * per; l++, cnt++) {
            p += x[l].par, k += x[l].kinds, ga += x[l].gadd, gb += x[l].gback, t += trivial(&x[l]), dp += deep(&x[l]);
            lo = x[l].par < lo ? x[l].par : lo;
            hi = x[l].par > hi ? x[l].par : hi;
        }
        fprintf(stderr, "  %d: par %4.1f (%d..%d)  kinds %.1f  greedy %d / %d  trivial %d/%d  deep %d\n", c + 1,
                p / cnt, lo, hi, k / cnt, ga, gb, t, cnt, dp);
    }
}

#ifdef REPORT                                   /* -DREPORT: the metrics of the levels in colourmix_levels.h */
#include "games/colourmix_levels.h"
static int report(void)
{
    static metrics x[CMX_LEVELS];
    for (int l = 0; l < CMX_LEVELS; l++) {
        cmx_map m;
        cmx_parse(&m, cmx_levels[l].text);
        measure(&m, cmx_levels[l].par, &x[l]);
    }
    table(x, CMX_LEVELS, PER);
    return 0;
}
#endif

static cand *pool;
static int npool, nchosen;
static float chosen[CMX_LEVELS][3];             /* the colours of the levels kept so far */

/* how colourful a colour is (its spread across the channels) */
static double chroma(const float c[3])
{
    float hi = fmaxf(c[0], fmaxf(c[1], c[2])), lo = fminf(c[0], fminf(c[1], c[2]));
    return hi - lo;
}

/* how far a candidate's colour is from the last few levels kept (the levels' colours varied), its colourfulness a
 * little */
static double apart(const cand *d)
{
    double best = 100;
    for (int i = nchosen - 4 < 0 ? 0 : nchosen - 4; i < nchosen; i++) {
        double e = cmx_de(chosen[i], d->rgb);
        best = e < best ? e : best;
    }
    return best + 20 * d->look;
}

int main(void)
{
#ifdef REPORT
    return report();
#endif
    static const char *const IDEA[CMX_CHAPTERS] = {
        "two colours", "three colours", "tints and shades", "fix this pot", "premixed tubes", "few drops",
    };
    static metrics all[CMX_LEVELS];
    pool = malloc(sizeof *pool * MAKES);
    printf("/* Colour Mixer's levels: made by tools/colourmix_gen.c (our own colours, checked by its search; do not\n"
           " * edit). Each level: its board (colourmix.h's text: the tubes, their drops, the pot's first drops, the\n"
           " * recipe), the par: the fewest drops. */\n"
           "const cmx_level_t cmx_levels[CMX_LEVELS] = {\n");
    for (int c = 0; c < CMX_CHAPTERS; c++) {
        const chapter *ch = &CH[c];
        npool = 0;
        for (int k = 0; k < ch->tries && npool < MAKES; k++) {
            const cmx_spec *sp = &ch->s[k % SPECS];
            cmx_map m;
            int par;
            cmx_make(&m, sp, (uint32_t)(1000003u * (c + 1) + 7919u * k), &par);
            if (par < 1) continue;
            int near = 0;
            for (int i = 0; i < npool && !near; i++) near = cmx_de(pool[i].rgb, m.target) < 4;
            for (int i = 0; i < nchosen && !near; i++) near = cmx_de(chosen[i], m.target) < 4;
            if (near) continue;
            cand *d = &pool[npool];
            cmx_text(&m, d->text);
            d->par = par;
            d->kinds = 0;
            for (int i = 0; i < m.n; i++) d->kinds += m.recipe[i] > 0;
            memcpy(d->rgb, m.target, sizeof d->rgb);
            measure(&m, par, &d->x);
            d->hard = !trivial(&d->x);
            int close = d->x.rivals;
            d->diff = par + 0.6 * d->kinds + 0.12 * m.n + 0.25 * log(1.0 + close) + 1.5 * !d->x.gback +
                      0.5 * (d->x.gback && d->x.gdrops > par);
            d->look = chroma(m.target);
            npool++;
        }
        qsort(pool, npool, sizeof *pool, by_diff);
        fprintf(stderr, "chapter %d (%s): %d boards, diff %.2f .. %.2f\n", c + 1, IDEA[c], npool,
                npool ? pool[0].diff : 0, npool ? pool[npool - 1].diff : 0);
        if (npool < PER) { fprintf(stderr, "too few\n"); return 1; }
        int used[MAKES] = {0}, nopen = c ? 2 : 3, hard[MAKES], nh = 0, at = 0;
        cand *out[PER];
        /* the opening levels (the chapter's idea, easy): the easiest boards, the first chapter's of 2, 2..3, 3 drops */
        for (int o = 0; o < nopen; o++) {
            int pick = -1;
            for (int j = 0; j < npool && pick < 0; j++)
                if (!used[j] && (c || pool[j].par == (o < 2 ? 2 + o : 3))) pick = j;
            for (int j = 0; j < npool && pick < 0; j++)
                if (!used[j]) pick = j;
            used[pick] = 1;
            out[at++] = &pool[pick];
        }
        int deeps[MAKES], nd = 0;               /* the rest: the obvious way misses them; the last few, the deep */
        for (int j = 0; j < npool; j++)
            if (!used[j] && pool[j].hard) {
                hard[nh++] = j;
                if (deep(&pool[j].x)) deeps[nd++] = j;
            }
        int rest = PER - nopen, nde = ch->deep < rest ? ch->deep : rest - 1;
        fprintf(stderr, "chapter %d: %d past the obvious way, %d deep\n", c + 1, nh, nd);
        if (nh < rest || nd < nde) { fprintf(stderr, "too few\n"); return 1; }
        for (int i = 0; i < rest; i++) {
            int dp = i >= rest - nde, *lst = dp ? deeps : hard, nl = dp ? nd : nh, k = dp ? i - (rest - nde) : i;
            int kn = dp ? nde : rest - nde;
            double f = kn > 1 ? (double)k / (kn - 1) : 1, a = (dp ? 0.3 : 0) + (dp ? 0.67 : 0.9) * f;
            int centre = (int)(a * (nl - 1)), span = nl / 14 + 1, pick = -1;
            if (i == rest - 1) { centre = nl - 5; span = 5; }
            for (int j = centre - span; j <= centre + span; j++) {
                if (j < 0 || j >= nl || used[lst[j]]) continue;
                if (pick < 0 || apart(&pool[lst[j]]) > apart(&pool[lst[pick]])) pick = j;
            }
            for (int j = 0; pick < 0 && j < nl; j++)
                if (!used[lst[(centre + j) % nl]]) pick = (centre + j) % nl;
            used[lst[pick]] = 1;
            out[at++] = &pool[lst[pick]];
            memcpy(chosen[nchosen++], pool[lst[pick]].rgb, sizeof chosen[0]);
        }
        for (int i = 0; i < PER; i++) {
            cand *d = out[i];
            printf("    {\"%s\", %d},\n", d->text, d->par);
            all[c * PER + i] = d->x;
        }
    }
    printf("};\n");
    table(all, CMX_LEVELS, PER);
    free(pool);
    return 0;
}
