/*
 * Colour Mixer's levels: src/games/colourmix_levels.h. Our own colours: no published or commercial set is used. Each
 * chapter has its idea (two colours; three; white and black, tints and shades; a pot that holds paint already; premixed
 * tubes; tubes holding few drops) and makes many boards with the rules' maker (cmx_make, from fixed seeds: a recipe
 * drawn among the tubes offered, kept only when no fewer drops reach its colour and no other recipe of as few drops
 * comes near it). A board is kept when its colour is not too near one the chapter has already. The difficulty: the
 * drops, the tubes the recipe uses, the tubes offered, the near misses a little more drops away. Each chapter keeps 25
 * spread over its difficulties (chapter 1 from the very easiest: a drop of each of two colours); the chapter's last
 * level, its showpiece, the hardest of the few most colourful. Deterministic (fixed seeds).
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
#define MAKES 300
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

typedef struct {
    char text[CMX_TEXT];
    int par, kinds;
    float rgb[3];
    double diff, look;
} cand;

typedef struct { cmx_spec s[SPECS]; int tries; } chapter;

static const chapter CH[CMX_CHAPTERS] = {
    {{{R_ | Y_, 2, 3, 2, 2, 0, 0, 0}, {Y_ | B_, 2, 4, 2, 2, 0, 0, 0}, {R_ | B_, 2, 4, 2, 2, 0, 0, 0},
      {R_ | Y_ | B_ | W_, 2, 6, 2, 2, 0, 0, 0}}, 1600},
    {{{R_ | Y_ | B_, 3, 6, 3, 3, 0, 0, 0}, {R_ | Y_ | B_ | W_, 3, 6, 3, 3, 0, 0, 0},
      {R_ | Y_ | B_ | W_, 4, 7, 3, 3, 0, 0, 0}, {R_ | Y_ | B_ | W_, 3, 7, 3, 3, 0, 0, 0}}, 2400},
    {{{R_ | Y_ | B_ | W_ | K_, 2, 5, 2, 2, 0, 0, W_}, {R_ | Y_ | B_ | W_ | K_, 3, 7, 2, 3, 0, 0, W_},
      {R_ | Y_ | B_ | W_ | K_, 4, 8, 3, 3, 0, 0, W_}, {R_ | Y_ | B_ | W_ | K_, 3, 8, 2, 4, 0, 0, W_}}, 2400},
    {{{R_ | Y_ | B_ | W_, 1, 4, 1, 2, 2, 0, 0}, {R_ | Y_ | B_ | W_ | K_, 2, 5, 1, 2, 3, 0, 0},
      {R_ | Y_ | B_ | W_ | K_, 3, 7, 2, 3, 3, 0, 0}, {R_ | Y_ | B_ | W_ | K_, 2, 7, 2, 3, 4, 0, 0}}, 2400},
    {{{R_ | Y_ | B_ | W_ | O_ | G_ | V_, 2, 6, 2, 3, 0, 0, O_}, {R_ | Y_ | B_ | W_ | K_ | N_ | P_, 3, 7, 2, 3, 0, 0, O_},
      {R_ | Y_ | B_ | W_ | O_ | G_ | P_, 3, 8, 2, 3, 0, 0, O_}, {R_ | Y_ | B_ | K_ | O_ | V_ | N_, 4, 9, 3, 4, 0, 0, O_}}, 2400},
    {{{R_ | Y_ | B_ | W_ | K_, 4, 8, 3, 3, 0, 1, 0}, {R_ | Y_ | B_ | W_ | K_ | O_ | G_, 4, 9, 3, 4, 0, 2, 0},
      {R_ | Y_ | B_ | W_ | K_ | V_ | P_, 5, 10, 3, 4, 0, 2, 0}, {R_ | Y_ | B_ | W_ | K_ | O_ | N_, 5, 12, 3, 4, 2, 2, 0}},
     2400},
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

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
    static const char *const IDEA[CMX_CHAPTERS] = {
        "two colours", "three colours", "tints and shades", "fix this pot", "premixed tubes", "few drops",
    };
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
            int close = cmx_rivals(&m, par + 2, 4 * CMX_TOL);
            d->diff = par + 0.6 * d->kinds + 0.12 * m.n + 0.25 * log(1.0 + close);
            d->look = chroma(m.target);
            npool++;
        }
        qsort(pool, npool, sizeof *pool, by_diff);
        fprintf(stderr, "chapter %d (%s): %d boards, diff %.2f .. %.2f\n", c + 1, IDEA[c], npool,
                npool ? pool[0].diff : 0, npool ? pool[npool - 1].diff : 0);
        if (npool < PER) { fprintf(stderr, "too few\n"); return 1; }
        int used[MAKES] = {0};
        for (int i = 0; i < PER; i++) {
            double f = (double)i / (PER - 1), lo = c ? 0.08 : 0.0, at = lo + (0.97 - lo) * f;
            int centre = (int)(at * (npool - 1)), span = npool / 14 + 1, pick = -1;
            if (i == PER - 1) { centre = npool - 5; span = 5; }
            for (int j = centre - span; j <= centre + span; j++) {
                if (j < 0 || j >= npool || used[j]) continue;
                if (pick < 0 || apart(&pool[j]) > apart(&pool[pick])) pick = j;
            }
            for (int j = 0; pick < 0 && j < npool; j++)
                if (!used[(centre + j) % npool]) pick = (centre + j) % npool;
            used[pick] = 1;
            memcpy(chosen[nchosen++], pool[pick].rgb, sizeof chosen[0]);
            cand *d = &pool[pick];
            printf("    {\"%s\", %d},\n", d->text, d->par);
            fprintf(stderr, "  %3d %-32s par %2d kinds %d rgb %3d %3d %3d diff %.2f\n", c * PER + i + 1, d->text,
                    d->par, d->kinds, (int)(255 * d->rgb[0]), (int)(255 * d->rgb[1]), (int)(255 * d->rgb[2]),
                    d->diff);
        }
    }
    printf("};\n");
    free(pool);
    return 0;
}
