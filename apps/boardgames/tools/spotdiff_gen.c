/*
 * Spot the Difference's levels, composed and chosen: src/games/spotdiff_levels.h. Our own grids: the game's own
 * drawings (spotdiff_rules.c) and this tool's small sprites, laid out in themed scenes, and mosaics drawn from
 * symmetric rules (a medallion in a frame, a quilt of four blocks), so every grid looks drawn, not dealt:
 *   - the scenes: a meadow (sun, clouds, birds, flowers), a night (moon, stars, fireflies), the sea (gulls, waves,
 *     fish, weed), snow (flakes, firs, rocks) and a desert (sun, dunes, cacti); the drawing stands on the ground,
 *     the sprites keep a cell apart, nothing of the drawing's colours in its sky or ground;
 *   - the mosaics: rings, diamonds, crosses and stars, mirrored both ways; the quilts' blocks in two colourings.
 * The differences come from the game's own maker (sdf_differ). Each candidate is checked (n differences on distinct
 * cells, never a colour-only change that some eyes cannot see) and ranked by the effort of finding them all
 * (sdf_effort: the kinds of change, how busy their places are, the size); near-duplicates (the same grid mirrored)
 * are dropped. Each chapter takes its levels evenly from a part of its ranked candidates, then goes from its easiest
 * to its hardest:
 *   1  8 x 8 drawings on the wood, 3 differences, plain ones (every drawing once);
 *   2  10 x 10 drawings in a scene, 4 differences;
 *   3  12 x 12 scenes with more sprites, 5 differences, symbols changing too;
 *   4  12 x 12 mosaics and quilts, 5 differences;
 *   5  14 x 14 busy scenes, 6 differences, subtler;
 *   6  14 x 14 busy scenes and many-coloured mosaics in turn, 7 differences, mostly symbols and colours in busy
 *      places.
 * Deterministic (fixed seeds): run it again after a change to the makers and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/sdfgen apps/boardgames/tools/spotdiff_gen.c \
 *       apps/boardgames/src/games/spotdiff_rules.c -lm && /tmp/sdfgen > apps/boardgames/src/games/spotdiff_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/spotdiff.h"

#define CANDS 240                               /* candidates made for each chapter */
#define PER (SDF_LEVELS / SDF_CHAPTERS)

static const char *const PIC_NAME[SDF_PICS] = {
    "heart", "house", "tree", "boat", "cat", "fish", "sun", "flower", "mushroom", "apple", "star", "moon", "rain",
    "key", "cup", "umbrella", "bird", "butterfly", "car", "rocket", "ghost", "duck", "leaf", "crown", "bell",
    "ice cream",
};

/* ---- the sprites: '.' none; k black, o orange, s sky blue, g bluish green, y yellow, b blue, r vermilion,
 * p reddish purple, w white ------------------------------------------------------------------------------------ */
typedef struct { int w, h; const char *row[4]; } sprite;
enum {
    SP_CLOUD, SP_CLOUD2, SP_BIRD, SP_SUN, SP_FLOWER, SP_TULIP, SP_BUTTERCUP, SP_SHROOM, SP_MOON, SP_SPARKLE, SP_DOT,
    SP_FIREFLY, SP_ROCK, SP_FISH, SP_MINNOW, SP_WEED, SP_BUBBLE, SP_GULL, SP_FLAKE, SP_FIR, SP_CACTUS, SP_ORANGE_SUN,
    SP_HEART, SP_MOTH, NSP
};
static const sprite SPR[NSP] = {
    {4, 2, {".ww.", "wwww"}},                   /* cloud */
    {6, 2, {"..ww..", "wwwwww"}},               /* long cloud */
    {3, 2, {"k.k", ".k."}},                     /* bird */
    {3, 3, {".y.", "yyy", ".y."}},              /* sun */
    {3, 2, {"p.p", ".y."}},                     /* flower */
    {3, 2, {"r.r", "rrr"}},                     /* tulip */
    {1, 1, {"y"}},                              /* buttercup */
    {3, 2, {"rrr", ".w."}},                     /* mushroom */
    {3, 3, {".yy", "yy.", ".yy"}},              /* moon */
    {3, 3, {"y.y", ".y.", "y.y"}},              /* sparkle */
    {1, 1, {"w"}},                              /* a star */
    {1, 1, {"y"}},                              /* firefly */
    {3, 2, {".k.", "kkk"}},                     /* rock */
    {4, 2, {"ooo.", "o.oo"}},                   /* fish */
    {2, 1, {"yy"}},                             /* minnow */
    {1, 3, {"g", "g", "g"}},                    /* weed */
    {1, 1, {"w"}},                              /* bubble */
    {3, 1, {"w.w"}},                            /* gull */
    {3, 3, {"w.w", ".w.", "w.w"}},              /* flake */
    {3, 3, {".g.", "ggg", ".k."}},              /* fir */
    {3, 3, {"g.g", "ggg", ".g."}},              /* cactus */
    {3, 3, {".o.", "ooo", ".o."}},              /* orange sun */
    {3, 2, {"r.r", ".r."}},                     /* small heart */
    {3, 2, {"p.p", "ppp"}},                     /* moth */
};

static int letter(char ch)
{
    static const char KEY[] = ".kosgybrpw";
    const char *at = strchr(KEY, ch);
    return at && ch ? (int)(at - KEY) : 0;
}

/* the themes: the sky and the ground, a sky feature in a top corner, what flies and what grows, the ground's lines */
enum { TX_PLAIN, TX_WAVES, TX_DUNES, TX_FURROWS };
typedef struct {
    const char *name;
    int sky, ground, line, feature, texture, low;   /* line: the texture's colour; low: the share of sky (%) */
    int up[4], nup, down[4], ndown;
} theme;
enum { TH_MEADOW, TH_NIGHT, TH_SEA, TH_SNOW, TH_DESERT, TH_FIELD, NTH };
static const theme TH[NTH] = {
    {"meadow", 3, 4, 0, SP_SUN, TX_PLAIN, 70, {SP_CLOUD, SP_CLOUD2, SP_BIRD}, 3,
     {SP_FLOWER, SP_TULIP, SP_BUTTERCUP, SP_SHROOM}, 4},
    {"night", 6, 1, 0, SP_MOON, TX_PLAIN, 70, {SP_DOT, SP_SPARKLE, SP_DOT}, 3, {SP_FIREFLY, SP_MOTH, SP_FIREFLY}, 3},
    {"sea", 3, 6, 9, SP_ORANGE_SUN, TX_WAVES, 45, {SP_CLOUD, SP_BIRD, SP_CLOUD2}, 3,
     {SP_FISH, SP_MINNOW, SP_WEED, SP_BUBBLE}, 4},
    {"snow", 6, 9, 0, SP_MOON, TX_PLAIN, 60, {SP_FLAKE, SP_DOT, SP_FLAKE}, 3, {SP_FIR, SP_ROCK, SP_FIR}, 3},
    {"desert", 3, 5, 2, SP_ORANGE_SUN, TX_DUNES, 60, {SP_BIRD, SP_CLOUD}, 2, {SP_CACTUS, SP_ROCK, SP_CACTUS}, 3},
    {"field", 9, 2, 7, SP_SUN, TX_FURROWS, 55, {SP_BIRD, SP_HEART}, 2, {SP_TULIP, SP_SHROOM, SP_FLOWER}, 3},
};

static void sprite_colours(const sprite *sp, unsigned *used)
{
    for (int y = 0; y < sp->h; y++)
        for (int x = 0; x < sp->w; x++) *used |= 1u << letter(sp->row[y][x]);
}

/* the sprite at (x, y) when it fits in rows [top, bottom) and keeps a cell from what is taken; 1: put */
static int place(sdf_puzzle *p, uint8_t *taken, const sprite *sp, int x, int y, int top, int bottom)
{
    if (x < 0 || y < top || x + sp->w > p->w || y + sp->h > bottom) return 0;
    for (int yy = y - 1; yy <= y + sp->h; yy++)
        for (int xx = x - 1; xx <= x + sp->w; xx++)
            if (xx >= 0 && yy >= 0 && xx < p->w && yy < p->h && taken[yy * p->w + xx]) return 0;
    for (int yy = 0; yy < sp->h; yy++)
        for (int xx = 0; xx < sp->w; xx++) {
            int c = letter(sp->row[yy][xx]);
            taken[(y + yy) * p->w + x + xx] = 1;
            if (c) p->base[(y + yy) * p->w + x + xx] = (uint8_t)c;
        }
    return 1;
}

/* a scene: its theme, the drawing pic (-1 none) standing on the ground, `many` sprites tried; 0 when the drawing's
 * colours clash with the theme */
static int scene(sdf_puzzle *p, int side, int th, int pic, int many, uint32_t seed)
{
    const theme *t = &TH[th];
    uint32_t s = pr_seed(seed ^ 0x5ce9e5u);
    sdf_puzzle d;
    unsigned used = 0, clash = 1u << t->sky | 1u << t->ground | (t->line ? 1u << t->line : 0);
    if (pic >= 0) {
        sdf_grid(&d, SDF_DRAWING, 8, 8, pic, seed);
        for (int i = 0; i < 64; i++) used |= 1u << d.base[i];
        if (used & clash & ~1u) return 0;
    }
    for (int i = 0; i < t->nup; i++) sprite_colours(&SPR[t->up[i]], &used);
    for (int i = 0; i < t->ndown; i++) sprite_colours(&SPR[t->down[i]], &used);
    memset(p, 0, sizeof *p);
    p->w = p->h = side;
    int w = side, h = side, horizon = h * t->low / 100 + pr_below(&s, 2);
    if (pic >= 0 && horizon < 8) horizon = 8;
    if (horizon > h - 2) horizon = h - 2;
    uint8_t taken[SDF_CELLS] = {0};
    for (int i = 0; i < w * h; i++) p->base[i] = (uint8_t)(i / w >= horizon ? t->ground : t->sky);
    int phase = pr_below(&s, 6);
    for (int y = horizon; y < h; y++)       /* the ground's lines */
        for (int x = 0; x < w; x++) {
            int on = 0;
            if (t->texture == TX_WAVES) on = (y - horizon) % 3 == 1 && (x + phase + y) % 4 == 0;
            if (t->texture == TX_DUNES) on = (y - horizon) % 3 == 2 && (x + phase + (y - horizon) / 3 * 2) % 6 < 3;
            if (t->texture == TX_FURROWS) on = (y - horizon) % 2 == 1 && (x + phase) % 7 != 0;
            if (on) p->base[y * w + x] = (uint8_t)t->line;
        }
    if (t->texture == TX_FURROWS)           /* the furrows: lines, kept clear of the sprites */
        for (int y = horizon; y < h; y++)
            for (int x = 0; x < w; x++)
                if (p->base[y * w + x] == t->line) taken[y * w + x] = 0;
    int mirror = pr_below(&s, 2), fx = mirror ? w - 3 - pr_below(&s, 2) : pr_below(&s, 2);
    place(p, taken, &SPR[t->feature], fx, pr_below(&s, 2), 0, horizon);
    if (pic >= 0) {                         /* the drawing standing on the horizon, away from the sun's side */
        int lo = mirror ? 0 : (w - 8) / 2, hi = mirror ? (w - 8) / 2 : w - 8;
        int ox = lo + pr_below(&s, hi - lo + 1), oy = horizon - 8 + (pr_below(&s, 3) == 0);
        if (oy + 8 > h) oy = h - 8;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                if (d.base[y * 8 + x]) {
                    p->base[(oy + y) * w + ox + x] = d.base[y * 8 + x];
                    taken[(oy + y) * w + ox + x] = 1;
                }
    }
    for (int i = 0; i < many * 8; i++) {    /* the sprites, a fair share above and below */
        int up = i % 2 == 0, at;
        const sprite *sp = &SPR[up ? t->up[pr_below(&s, t->nup)] : t->down[pr_below(&s, t->ndown)]];
        at = place(p, taken, sp, pr_below(&s, w), up ? pr_below(&s, horizon) : horizon + pr_below(&s, h - horizon),
                   up ? 0 : horizon, up ? horizon : h);
        if (at && --many <= 0) break;
    }
    return 1;
}

/* a mosaic: a medallion in its frame (both drawn from a rule, mirrored both ways), or a quilt of four such blocks in
 * two colourings */
static int rule(int r, int a, int b)
{
    switch (r) {
    case 0: return a > b ? a : b;                     /* square rings */
    case 1: return a + b;                             /* diamonds */
    case 2: return a < b ? a : b;                     /* a cross */
    case 3: return (a > b ? a : b) + 2 * (a == b);    /* rings, their corners lit */
    case 4: return a > b ? a - b : b - a;             /* a saltire */
    default: return (a * a + b * b) / 3;              /* round rings */
    }
}

static void mosaic(sdf_puzzle *p, int side, int k, int quilt, uint32_t seed)
{
    uint32_t s = pr_seed(seed ^ 0x305a1cu);
    int all[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9}, seq[2][16];
    pr_shuffle(&s, all, 9);
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 16; i++) {
            int c;
            do c = all[pr_below(&s, k)];
            while (i > 0 && c == seq[j][i - 1]);
            seq[j][i] = c;
        }
    if (!quilt) seq[1][0] = seq[0][0];
    memset(p, 0, sizeof *p);
    p->w = p->h = side;
    int T = quilt ? side / 2 : side, outer = pr_below(&s, 6), inner = pr_below(&s, 6), r = 1 + pr_below(&s, T / 4 + 1);
    int bw = 1 + (pr_below(&s, 3) == 0);
    for (int y = 0; y < side; y++)
        for (int x = 0; x < side; x++) {
            int u = x % T, v = y % T, a = abs(2 * u - (T - 1)) / 2, b = abs(2 * v - (T - 1)) / 2;
            int odd = quilt && (x / T + y / T) % 2, f;
            if (rule(inner, a, b) <= r) f = rule(inner, a, b) / bw;
            else f = 1 + r / bw + rule(outer, a, b) / bw;
            p->base[y * side + x] = (uint8_t)seq[odd][f % 16];
        }
}

/* ---- choosing ----------------------------------------------------------------------------------------------- */
enum { K_DRAWING, K_SCENE, K_BUSY, K_MOSAIC, K_MIX };
typedef struct { int kind, side, n, sprites; float subtle, lo, hi; } chapter;
static const chapter CH[SDF_CHAPTERS] = {
    {K_DRAWING, 8, 3, 0, 0.0f, 0, 1},
    {K_SCENE, 10, 4, 4, 0.25f, 0, 0.8f},
    {K_SCENE, 12, 5, 7, 0.45f, 0.1f, 0.9f},
    {K_MOSAIC, 12, 5, 0, 0.55f, 0.1f, 0.9f},
    {K_BUSY, 14, 6, 16, 0.75f, 0.1f, 0.9f},
    {K_MIX, 14, 7, 20, 0.9f, 0.2f, 1},
};

typedef struct {
    sdf_puzzle p;
    int kind, pic, theme;
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

/* kind: K_DRAWING, K_SCENE / K_BUSY (a theme), K_MOSAIC (theme: 0 a medallion, 1 a quilt) */
static int make(cand *c, const chapter *ch, int kind, int pic, int th, uint32_t seed)
{
    if (kind == K_DRAWING) sdf_grid(&c->p, SDF_DRAWING, ch->side, ch->side, pic, seed);
    else if (kind == K_MOSAIC) mosaic(&c->p, ch->side, ch->side >= 14 ? 5 + (int)(seed % 2) : 4, th, seed);
    else {
        if (kind == K_BUSY && seed % 4 == 0) pic = -1;   /* a busy scene without a drawing, now and then */
        if (!scene(&c->p, ch->side, th, pic, ch->sprites, seed)) return 0;
    }
    if (!sdf_differ(&c->p, ch->n, ch->subtle, seed) || !sdf_check(&c->p)) return 0;
    c->kind = kind;
    c->pic = pic;
    c->theme = th;
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
        if (ch->kind == K_DRAWING) {            /* every drawing once (the last one left out), its middle candidate */
            for (int pic = 0; pic < PER; pic++) {
                cand some[9];
                int k = 0;
                for (uint32_t seed = 1; k < 9 && seed < 500; seed++)
                    if (make(&some[k], ch, K_DRAWING, pic, 0, 1000u * (ci + 1) + 37u * pic + seed)) k++;
                qsort(some, (size_t)k, sizeof some[0], by_effort);
                C[n++] = some[k / 2];
            }
        } else {
            for (uint32_t seed = 1; n < CANDS; seed++) {
                int kind = ch->kind == K_MIX ? (seed % 2 ? K_BUSY : K_MOSAIC) : ch->kind;
                int th = kind == K_MOSAIC ? (int)(seed / 2 % 2) : (int)(seed % NTH);
                int pic = (int)(seed * 7 % SDF_PICS);
                if (make(&C[n], ch, kind, pic, th, 100000u * (ci + 1) + seed)) {
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
           " * Spot the Difference's levels: our own, composed by tools/spotdiff_gen.c (the game's drawings and the\n"
           " * tool's sprites in themed scenes, and symmetric mosaics; the differences from the game's maker; each\n"
           " * checked: its differences on distinct cells, every colour-only one fair for every eye), six chapters of\n"
           " * 25 from the easiest. Each: \"<w>x<h>:\", a digit a cell (its colour) row by row, \":\", then each\n"
           " * difference as its cell (three digits), its side, its colour and its symbol (spotdiff.h, sdf_decode); in\n"
           " * the comment its grid, its differences and the effort of finding them.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "static const char *const sdf_levels[SDF_LEVELS] = {\n");
    for (int i = 0; i < out; i++) {
        char s[SDF_CELLS + 80], what[64];
        cand *c = &chosen[i];
        if (!sdf_encode(&c->p, s, sizeof s)) return 1;
        int kinds[SDF_KINDS] = {0};
        for (int k = 0; k < c->p.n; k++) kinds[sdf_kind(&c->p, k)]++;
        if (c->kind == K_DRAWING) snprintf(what, sizeof what, "drawing %s", PIC_NAME[c->pic]);
        else if (c->kind == K_MOSAIC) snprintf(what, sizeof what, "%s", c->theme ? "quilt" : "medallion");
        else snprintf(what, sizeof what, "%s%s%s%s", c->kind == K_BUSY ? "busy " : "", TH[c->theme].name,
                      c->pic >= 0 ? " " : "", c->pic >= 0 ? PIC_NAME[c->pic] : "");
        printf("    \"%s\",\n    /* %d: %dx%d %s, %d differences (%d symbol, %d colour), effort %.1f */\n", s, i + 1,
               c->p.w, c->p.h, what, c->p.n, kinds[SDF_SYMBOL], kinds[SDF_COLOUR], c->effort);
    }
    printf("};\n");
    return 0;
}
