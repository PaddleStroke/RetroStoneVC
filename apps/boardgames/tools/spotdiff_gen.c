/*
 * Spot the Difference's levels, made by chance and chosen: src/games/spotdiff_levels.h. Our own grids, from the game's
 * own makers (spotdiff_rules.c): the pixel-art drawings on the bare wood, then on scenes, then busy patterns. Each
 * candidate is checked (n differences on distinct cells, never a colour-only change that some eyes cannot see) and
 * ranked by the effort of finding them all (sdf_effort: the kinds of change, how busy their places are, the size);
 * near-duplicates (the same grid mirrored) are dropped. Each chapter takes its levels evenly from a part of its
 * ranked candidates, then goes from its easiest to its hardest:
 *   1  8 x 8 drawings on the wood, 3 differences, plain ones (every drawing once);
 *   2  10 x 10 drawings on a scene, 4 differences;
 *   3  12 x 12 scenes, 5 differences, symbols changing too;
 *   4  12 x 12 kaleidoscopes, 5 differences;
 *   5  14 x 14 blobs of colour, 6 differences, subtler;
 *   6  14 x 14 blobs and speckles, 7 differences, mostly symbols and colours in busy places.
 * Deterministic (fixed seeds): run it again after a change to the makers and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/sdfgen apps/boardgames/tools/spotdiff_gen.c \
 *       apps/boardgames/src/games/spotdiff_rules.c -lm && /tmp/sdfgen > apps/boardgames/src/games/spotdiff_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/spotdiff.h"

#define CANDS 240                               /* candidates made for each chapter */
#define PER (SDF_LEVELS / SDF_CHAPTERS)

static const char *const PIC_NAME[SDF_PICS] = {
    "heart", "house", "tree", "boat", "cat", "fish", "sun", "flower", "mushroom", "apple", "star", "moon", "rain",
    "key", "cup", "umbrella", "bird", "butterfly", "car", "rocket", "ghost", "duck", "leaf", "crown", "bell",
    "ice cream",
};
static const char *const KIND_NAME[] = {"drawing", "scene", "kaleidoscope", "blobs", "speckles"};

typedef struct { int kind, side, n; float subtle, lo, hi; } chapter;
static const chapter CH[SDF_CHAPTERS] = {
    {SDF_DRAWING, 8, 3, 0.0f, 0, 1},
    {SDF_SCENE, 10, 4, 0.25f, 0, 0.8f},
    {SDF_SCENE, 12, 5, 0.45f, 0.1f, 0.9f},
    {SDF_PATTERN, 12, 5, 0.55f, 0.1f, 0.9f},
    {SDF_BLOBS, 14, 6, 0.75f, 0.1f, 0.9f},
    {-1, 14, 7, 0.9f, 0.2f, 1},                 /* blobs and speckles in turn */
};

typedef struct {
    sdf_puzzle p;
    int kind, pic;
    float effort;
    char key[SDF_CELLS + 1];
} cand;

static cand C[CANDS];
static char seen[SDF_LEVELS][SDF_CELLS + 1];
static int nseen;

/* the grid as text, the least of it and its mirror (a mirrored grid is the same level) */
static void key_of(const sdf_puzzle *p, char *out)
{
    char a[SDF_CELLS + 1], b[SDF_CELLS + 1];
    for (int y = 0; y < p->h; y++)
        for (int x = 0; x < p->w; x++) {
            a[y * p->w + x] = (char)('0' + p->base[y * p->w + x]);
            b[y * p->w + x] = (char)('0' + p->base[y * p->w + p->w - 1 - x]);
        }
    a[p->w * p->h] = b[p->w * p->h] = 0;
    strcpy(out, strcmp(a, b) < 0 ? a : b);
}

static int by_effort(const void *x, const void *y)
{
    float a = ((const cand *)x)->effort, b = ((const cand *)y)->effort;
    return a < b ? -1 : a > b;
}

static int make(cand *c, const chapter *ch, int kind, int pic, uint32_t seed)
{
    sdf_grid(&c->p, kind, ch->side, ch->side, pic, seed);
    if (!sdf_differ(&c->p, ch->n, ch->subtle, seed) || !sdf_check(&c->p)) return 0;
    c->kind = kind;
    c->pic = pic;
    c->effort = sdf_effort(&c->p);
    key_of(&c->p, c->key);
    for (int i = 0; i < nseen; i++)
        if (!strcmp(seen[i], c->key)) return 0;
    return 1;
}

int main(void)
{
    static cand chosen[SDF_LEVELS];
    int out = 0;
    for (int ci = 0; ci < SDF_CHAPTERS; ci++) {
        const chapter *ch = &CH[ci];
        int n = 0;
        if (ch->kind == SDF_DRAWING) {          /* every drawing once (the last one left out), its middle candidate */
            for (int pic = 0; pic < PER; pic++) {
                cand some[9];
                int k = 0;
                for (uint32_t seed = 1; k < 9 && seed < 500; seed++)
                    if (make(&some[k], ch, SDF_DRAWING, pic, 1000u * (ci + 1) + 37u * pic + seed)) k++;
                qsort(some, (size_t)k, sizeof some[0], by_effort);
                C[n++] = some[k / 2];
            }
        } else {
            for (uint32_t seed = 1; n < CANDS; seed++) {
                int kind = ch->kind >= 0 ? ch->kind : seed % 2 ? SDF_BLOBS : SDF_NOISE;
                int pic = (int)(seed * 7 % SDF_PICS);
                if (make(&C[n], ch, kind, pic, 100000u * (ci + 1) + seed)) {
                    int dup = 0;
                    for (int i = 0; i < n && !dup; i++) dup = !strcmp(C[i].key, C[n].key);
                    if (!dup) n++;
                }
            }
        }
        qsort(C, (size_t)n, sizeof C[0], by_effort);
        for (int i = 0; i < PER; i++) {
            int at = n == PER ? i : (int)((ch->lo + (ch->hi - ch->lo) * i / (PER - 1.0f)) * (n - 1) + 0.5f);
            chosen[out] = C[at];
            strcpy(seen[nseen++], C[at].key);
            out++;
        }
    }
    printf("/*\n"
           " * Spot the Difference's levels: our own, made by tools/spotdiff_gen.c (the game's makers; each checked: its\n"
           " * differences on distinct cells, every colour-only one fair for every eye), six chapters of 25 from the\n"
           " * easiest. Each: \"<w>x<h>:\", a digit a cell (its colour) row by row, \":\", then each difference as its\n"
           " * cell (three digits), its side, its colour and its symbol (spotdiff.h, sdf_decode); in the comment its\n"
           " * grid, its differences and the effort of finding them.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "static const char *const sdf_levels[SDF_LEVELS] = {\n");
    for (int i = 0; i < out; i++) {
        char s[SDF_CELLS + 80];
        cand *c = &chosen[i];
        if (!sdf_encode(&c->p, s, sizeof s)) return 1;
        int kinds[SDF_KINDS] = {0};
        for (int k = 0; k < c->p.n; k++) kinds[sdf_kind(&c->p, k)]++;
        printf("    \"%s\",\n    /* %d: %dx%d %s%s%s, %d differences (%d symbol, %d colour), effort %.1f */\n", s, i + 1,
               c->p.w, c->p.h, KIND_NAME[c->kind], c->kind <= SDF_SCENE ? " " : "",
               c->kind <= SDF_SCENE ? PIC_NAME[c->pic] : "", c->p.n, kinds[SDF_SYMBOL], kinds[SDF_COLOUR], c->effort);
    }
    printf("};\n");
    return 0;
}
