/*
 * Same Game's levels: src/games/samegame_levels.h. Our own levels: no published set is used. Six chapters, the board
 * growing: 6 x 6 in 3 colours, 8 x 8 in 3, 10 x 10 in 3, 12 x 10 in 4, 15 x 10 in 4, 20 x 12 in 5.
 * Each level comes from a theme, so that a chapter is a designed mix and not twenty-five boards of noise alike:
 *   - classic: a board made backwards by the rules (sm_make: every board can be cleared), its way kept for the hint;
 *   - mirror: patches of colour (a random field smoothed into blobs), the right half the left's mirror image;
 *   - picture: a symmetric motif (rings, a diamond, chevrons, a checker of blocks, stripes) with a sprinkle of
 *     symmetric flecks; each chapter's last level is its showpiece, the picture where greedy play falls furthest
 *     short of the best (a tie: the higher score).
 * A mirror or picture board is kept only when a depth-first search finds a way to clear it (the way kept for the
 * hint), every colour twice or more and no tile alone with nothing to pair. Its best score sought by the rules
 * (sm_reference: a beam search with a wide beam, a greedy game, the way kept), and a greedy game's alone (a beam of
 * one). The goal: nine tenths of the best (sm_goal_of). The difficulty: how far the greedy play falls short of the
 * best. Each chapter mixes the themes (classic and mirror in turn, pictures at a third and two thirds), spread
 * evenly over each theme's difficulties and ordered by that spread (the themes interleaved, each from its easiest),
 * the showpiece last. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/smgen apps/boardgames/tools/samegame_gen.c && \
 *       /tmp/smgen > apps/boardgames/src/games/samegame_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define SM_GEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/samegame_rules.c"

#define PER (SM_LEVELS / SM_CHAPTERS)
#define POOL 26                                 /* the candidates of each theme a chapter draws */
#define PICS 5

static const struct { int w, h, colours, beam; } CH[SM_CHAPTERS] = {
    {6, 6, 3, 256}, {8, 8, 3, 256}, {10, 10, 3, 160}, {12, 10, 4, 128}, {15, 10, 4, 96}, {20, 12, 5, 64},
};

enum { T_CLASSIC, T_MIRROR, T_PICTURE, THEMES };
static const char *const THEME_NAME[THEMES] = {"classic", "mirror", "picture"};

typedef struct {
    int best, greedy, goal, theme, used;
    double diff, q;                             /* q: its place in its theme's difficulties */
    char text[SM_W * SM_H * 3 + 4];
} cand;

static cand pool[THEMES][POOL];
static int npool[THEMES];

static int by_diff(const void *a, const void *b)
{
    double d = (*(cand *const *)a)->diff - (*(cand *const *)b)->diff;
    return d < 0 ? -1 : d > 0;
}

static int by_q(const void *a, const void *b)
{
    double d = (*(cand *const *)a)->q - (*(cand *const *)b)->q;
    return d < 0 ? -1 : d > 0;
}

/* ---- a way to clear, found depth first (the rules' clear_dfs, the whole way kept) ------------------------------- */
static int clear_way(const sm_board *b, int w, int h, search *s, uint8_t (*path)[2], int depth, int *len)
{
    if (board_empty(b)) {
        *len = depth;
        return 1;
    }
    if (s->budget-- <= 0 || depth >= SM_HIST) return 0;
    int count[8] = {0};
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++) count[b->c[x][y]]++;
    for (int c = 1; c < 8; c++)
        if (count[c] == 1) return 0;
    uint64_t k = board_key(b, w, h);
    int i = (int)(k & (uint64_t)s->mask);
    if (s->seen[i] == k) return 0;
    s->seen[i] = k;
    uint8_t done[SM_W][SM_H] = {{0}};
    int gx[SM_W * SM_H / 2], gy[SM_W * SM_H / 2], gs[SM_W * SM_H / 2], ng = 0;
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h && b->c[x][y]; y++) {
            if (done[x][y]) continue;
            int n = sm_group(b, w, h, x, y, done);
            if (n < 2) continue;
            int key = count[b->c[x][y]] * 256 - n, j = ng++;
            while (j > 0 && gs[j - 1] > key) { gx[j] = gx[j - 1], gy[j] = gy[j - 1], gs[j] = gs[j - 1]; j--; }
            gx[j] = x, gy[j] = y, gs[j] = key;
        }
    for (int j = 0; j < ng && s->budget > 0; j++) {
        sm_board nb = *b;
        uint8_t m[SM_W][SM_H] = {{0}};
        sm_group(&nb, w, h, gx[j], gy[j], m);
        take(&nb, w, h, m);
        path[depth][0] = (uint8_t)gx[j];
        path[depth][1] = (uint8_t)gy[j];
        if (clear_way(&nb, w, h, s, path, depth + 1, len)) return 1;
    }
    return 0;
}

/* ---- the themed boards ----------------------------------------------------------------------------------------- */
static void mirror_fill(sm_board *b, int w, int h, int colours, int passes, uint32_t *r)
{
    int hw = (w + 1) / 2;
    for (int x = 0; x < hw; x++)
        for (int y = 0; y < h; y++) b->c[x][y] = (int8_t)(1 + pr_below(r, colours));
    for (int p = 0; p < passes; p++) {          /* smoothing: a tile often takes its neighbours' most common colour */
        sm_board o = *b;
        for (int x = 0; x < hw; x++)
            for (int y = 0; y < h; y++) {
                int count[8] = {0}, best = o.c[x][y];
                static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (int j = 0; j < 4; j++) {
                    int qx = x + d[j][0], qy = y + d[j][1];
                    if (qx >= hw) qx = w - 1 - qx;  /* across the middle: the mirror image */
                    if (qx < 0 || qy < 0 || qy >= h) continue;
                    count[o.c[qx][qy]]++;
                }
                for (int c = 1; c <= colours; c++)
                    if (count[c] > count[best]) best = c;
                if (pr_below(r, 10) < 6) b->c[x][y] = (int8_t)best;
            }
    }
    for (int x = hw; x < w; x++) memcpy(b->c[x], b->c[w - 1 - x], SM_H);
}

static void picture_fill(sm_board *b, int w, int h, int colours, int kind, int flecks, uint32_t *r)
{
    int shift = pr_below(r, colours);
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++) {
            int dx2 = 2 * x - (w - 1), dy2 = 2 * y - (h - 1);   /* twice the distance from the middle */
            int ax = dx2 < 0 ? -dx2 : dx2, ay = dy2 < 0 ? -dy2 : dy2, v;
            switch (kind) {
            case 0: v = (ax > ay ? ax : ay) / 4; break;              /* rings */
            case 1: v = (ax + ay) / 4; break;                         /* a diamond */
            case 2: v = (ax / 2 + y) / 2; break;                      /* chevrons */
            case 3: v = (x / 2 + y / 2); break;                       /* a checker of blocks */
            default: v = y / 2 + (ax <= 2); break;                    /* stripes, a pillar down the middle */
            }
            b->c[x][y] = (int8_t)(1 + (v + shift) % colours);
        }
    int hw = (w + 1) / 2;
    for (int i = 0; i < flecks; i++) {          /* symmetric flecks: a tile and its mirror image */
        int x = pr_below(r, hw), y = pr_below(r, h), c = 1 + pr_below(r, colours);
        b->c[x][y] = b->c[w - 1 - x][y] = (int8_t)c;
    }
}

static int themed(sm_game *g, int theme, int w, int h, int colours, int kind, uint32_t seed)
{
    static uint8_t path[SM_HIST][2];
    memset(g, 0, sizeof *g);
    g->w = w, g->h = h, g->colours = colours;
    uint32_t r = pr_seed(seed);
    if (theme == T_MIRROR) mirror_fill(&g->b, w, h, colours, 1 + pr_below(&r, 2), &r);
    else picture_fill(&g->b, w, h, colours, kind, w * h / 10 + pr_below(&r, w * h / 10), &r);
    int count[8] = {0};
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++) count[g->b.c[x][y]]++;
    for (int c = 1; c <= colours; c++)
        if (count[c] < 2) return 0;
    g->start = g->b;
    if (sm_over(g)) return 0;
    search s = {calloc(1 << 18, sizeof(uint64_t)), (1 << 18) - 1, 150000};
    int len = 0, ok = clear_way(&g->start, w, h, &s, path, 0, &len);
    free(s.seen);
    if (!ok) return 0;
    g->nway = len;
    memcpy(g->way, path, (size_t)len * 2);
    return 1;
}

static void measure(sm_game *g, int beam, int theme, cand *c)
{
    c->best = sm_reference(g, beam);
    c->greedy = sm_best(&g->start, g->w, g->h, 1);
    c->goal = sm_goal_of(c->best);
    c->diff = c->best > 0 ? 1.0 - (double)c->greedy / c->best : 0;
    c->theme = theme;
    c->used = 0;
    sm_text(g, c->text);
}

/* the unused candidate of a theme at quantile q of its difficulties */
static cand *at_quantile(int theme, double q)
{
    static cand *list[POOL];
    int n = 0;
    for (int i = 0; i < npool[theme]; i++) list[n++] = &pool[theme][i];
    if (!n) return NULL;
    qsort(list, (size_t)n, sizeof *list, by_diff);
    int at = (int)(q * (n - 1) + 0.5);
    for (int d = 0; d < n; d++) {
        if (at + d < n && !list[at + d]->used) return list[at + d];
        if (at - d >= 0 && !list[at - d]->used) return list[at - d];
    }
    return NULL;
}

int main(void)
{
    static cand *out[SM_LEVELS];
    static cand store[SM_LEVELS];
    static sm_game g;
    int nout = 0;
    for (int ch = 0; ch < SM_CHAPTERS; ch++) {
        int w = CH[ch].w, h = CH[ch].h, cols = CH[ch].colours;
        memset(npool, 0, sizeof npool);
        for (int i = 0; i < POOL; i++) {
            sm_make(&g, w, h, cols, 6000u + 1000u * (uint32_t)ch + (uint32_t)i);
            measure(&g, CH[ch].beam, T_CLASSIC, &pool[T_CLASSIC][npool[T_CLASSIC]++]);
        }
        for (uint32_t i = 0; i < 400 && npool[T_MIRROR] < POOL; i++)
            if (themed(&g, T_MIRROR, w, h, cols, 0, 20000u + 1000u * (uint32_t)ch + i))
                measure(&g, CH[ch].beam, T_MIRROR, &pool[T_MIRROR][npool[T_MIRROR]++]);
        /* pictures: each motif in turn, a few flecks' draws of each */
        for (uint32_t i = 0; i < 400 && npool[T_PICTURE] < PICS * 2; i++)
            if (themed(&g, T_PICTURE, w, h, cols, (int)((i + (uint32_t)ch) % PICS), 40000u + 1000u * (uint32_t)ch + i))
                measure(&g, CH[ch].beam, T_PICTURE, &pool[T_PICTURE][npool[T_PICTURE]++]);
        fprintf(stderr, "chapter %d (%d x %d, %d colours): %d classic, %d mirror, %d pictures\n", ch + 1, w, h, cols,
                npool[T_CLASSIC], npool[T_MIRROR], npool[T_PICTURE]);
        /* the showpiece: the picture where greedy play falls furthest short */
        cand *sp = NULL;
        for (int i = 0; i < npool[T_PICTURE]; i++)
            if (!sp || pool[T_PICTURE][i].diff > sp->diff ||
                (pool[T_PICTURE][i].diff == sp->diff && pool[T_PICTURE][i].best > sp->best)) sp = &pool[T_PICTURE][i];
        if (sp) sp->used = 1;
        int first = nout, slots = PER - (sp != NULL);
        int npic = npool[T_PICTURE] - (sp != NULL) >= 2 ? 2 : npool[T_PICTURE] - (sp != NULL);
        int nmir = npool[T_MIRROR] < (slots - npic) / 2 ? npool[T_MIRROR] : (slots - npic) / 2;
        int ncla = slots - npic - nmir, want[THEMES] = {ncla, nmir, npic};
        for (int th = 0; th < THEMES; th++)
            for (int i = 0; i < want[th]; i++) {
                double q = want[th] > 1 ? 0.04 + 0.92 * i / (want[th] - 1) : 0.5;
                if (ch == 0) q *= 0.8;          /* the first chapter: the gentler part */
                cand *c = at_quantile(th, q);
                c->used = 1;
                store[nout] = *c;
                store[nout].q = q + 0.001 * th;
                out[nout] = &store[nout];
                nout++;
            }
        qsort(out + first, (size_t)(nout - first), sizeof *out, by_q);   /* the themes interleaved */
        if (sp) {
            store[nout] = *sp;
            out[nout] = &store[nout];
            nout++;
        }
    }
    printf("/*\n"
           " * Same Game's levels: our own, made by tools/samegame_gen.c (classic boards made backwards so that each can be\n"
           " * cleared, mirrored patches of colour and symmetric pictures that a search clears; the best score sought by the\n"
           " * rules' beam search; six chapters of 25). Each: the rows from the top (each tile's colour), then after a space\n"
           " * the way to clear it (each removal a place: its column and its row from the bottom, as letters); in the comment\n"
           " * the theme, the best score found and a greedy player's; the goals (nine tenths of the best) in sm_level_goal.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "const char *const sm_levels[SM_LEVELS] = {\n");
    for (int i = 0; i < nout; i++)
        printf("    \"%s\", /* %d: %s, best %d, greedy %d */\n", out[i]->text, i + 1, THEME_NAME[out[i]->theme],
               out[i]->best, out[i]->greedy);
    printf("};\n\nconst uint16_t sm_level_goal[SM_LEVELS] = {");
    for (int i = 0; i < nout; i++) printf("%s%d,", i % 15 ? " " : "\n    ", out[i]->goal);
    printf("\n};\n");
    return nout == SM_LEVELS ? 0 : 1;
}
