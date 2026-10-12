/*
 * Klotski's levels: src/games/klotski_levels.h. Our own levels: no published or traditional set is used. A tray:
 * blocks laid by chance (the great block, six, five or four blocks of 1 x 2 standing or lying, small ones until two
 * cells are left); its whole family of positions found by the rules (klt_family: every position the blocks can reach,
 * and the fewest moves out of each, searched back from the solved ones). A family met before, or the mirror of one
 * (read from the right), is left out; so is a family whose great block never gets out (most of the small ones). Few
 * families are both large and deep, so each one gives many levels: at every distance from the way out, a few
 * positions drawn among those at that distance (the great block in the upper half when one is: the way out looks
 * far). The difficulty: the fewest moves, a little more in a large family (more ways to go wrong). Each chapter has
 * its range of moves and keeps 25 candidates spread evenly over its difficulties, from the easiest; never two at one
 * distance from one family, never a position or its mirror twice. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/kltgen apps/boardgames/tools/klotski_gen.c -lm && \
 *       /tmp/kltgen > apps/boardgames/src/games/klotski_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define KLT_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/klotski_rules.c"

#define PER (KLT_LEVELS / KLT_CHAPTERS)
#define LAYS 6000
#define SAMPLES 3                               /* the positions kept a distance a family */
#define FAR 256
#define MAXC 60000

static const int LO[KLT_CHAPTERS] = {3, 10, 20, 35, 50, 70}, HI[KLT_CHAPTERS] = {9, 19, 34, 49, 69, 255};

typedef struct { int np; klt_piece p[KLT_PIECES]; } tray;

typedef struct {
    char text[KLT_PIECES * 4 + 1];
    uint64_t key;                               /* the position's, its mirror's alike */
    int dist, size, fam;
    double diff;
} cand;

static cand cands[MAXC];
static int ncand;
static uint64_t *sigs;
static int nsigs;
static uint32_t rng;

/* the family being looked at: its size, its farthest position, and at each distance a few positions drawn (two
 * reservoirs: the great block high, and any) */
typedef struct {
    uint64_t sig;
    int size, far;
    int seen[FAR][2];
    tray pick[FAR][2][SAMPLES];
} look;

/* blocks laid by chance: the great block, m of 1 x 2 (standing or lying), small ones until two cells are left */
static int lay(klt_piece *p, uint32_t *s)
{
    static const int SETS[3][2] = {{6, 2}, {5, 4}, {4, 6}};
    const int *set = SETS[pr_below(s, 3)];
    int np = 0;
    int8_t at[KLT_W * KLT_H];
    memset(at, -1, sizeof at);
    for (int kind = 0; kind < 3; kind++) {
        int count = kind == 0 ? 1 : kind == 1 ? set[0] : set[1];
        for (int k = 0; k < count; k++) {
            int t = kind == 0 ? KLT_BIG : kind == 2 ? KLT_SMALL : pr_below(s, 2) ? KLT_TALL : KLT_WIDE, placed = 0;
            for (int tries = 0; tries < 60 && !placed; tries++) {
                if (kind == 1 && tries == 30) t = t == KLT_TALL ? KLT_WIDE : KLT_TALL;
                p[np].type = (int8_t)t;
                p[np].x = (int8_t)pr_below(s, KLT_W - PW[t] + 1);
                p[np].y = (int8_t)pr_below(s, KLT_H - PH[t] + 1);
                if (!fits(at, p, np, p[np].x, p[np].y)) continue;
                for (int dy = 0; dy < PH[t]; dy++)
                    for (int dx = 0; dx < PW[t]; dx++) at[p[np].x + dx + (p[np].y + dy) * KLT_W] = (int8_t)np;
                np++;
                placed = 1;
            }
            if (!placed) return 0;
        }
    }
    return valid(p, np) ? np : 0;
}

static uint64_t mirror_code(const klt_piece *src, int np)
{
    klt_piece p[KLT_PIECES];
    memcpy(p, src, sizeof p);
    for (int i = 0; i < np; i++) p[i].x = (int8_t)(KLT_W - PW[p[i].type] - p[i].x);
    return code_of(p, np);
}

static uint64_t key_of(const klt_piece *p, int np)
{
    uint64_t a = code_of(p, np), b = mirror_code(p, np);
    return a < b ? a : b;
}

static void first_pass(const klt_game *pos, int dist, void *ctx)
{
    look *L = ctx;
    uint64_t m = key_of(pos->p, pos->np);
    if (!L->sig || m < L->sig) L->sig = m;
    if (dist > L->far) L->far = dist;
    L->size++;
}

static int big_y(const klt_piece *p, int np)
{
    for (int i = 0; i < np; i++)
        if (p[i].type == KLT_BIG) return p[i].y;
    return 0;
}

static void second_pass(const klt_game *pos, int dist, void *ctx)
{
    look *L = ctx;
    if (dist < LO[0] || dist >= FAR) return;
    int high = big_y(pos->p, pos->np) <= 1, n = ++L->seen[dist][high];
    int slot = n <= SAMPLES ? n - 1 : pr_below(&rng, n);
    if (slot >= SAMPLES) return;
    L->pick[dist][high][slot].np = pos->np;
    memcpy(L->pick[dist][high][slot].p, pos->p, sizeof pos->p);
}

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0;
}

int main(void)
{
    static look L;
    static klt_game g;
    int failed = 0, stuck = 0, far_hist[20] = {0};
    rng = pr_seed(4000);
    sigs = malloc(LAYS * sizeof *sigs);
    if (!sigs) return 1;
    for (int lay_no = 0; lay_no < LAYS; lay_no++) {
        memset(&g, 0, sizeof g);
        g.np = lay(g.p, &rng);
        if (!g.np) { failed++; continue; }
        memset(&L, 0, sizeof L);
        if (!klt_family(&g, first_pass, &L)) { failed++; continue; }
        int met = 0;
        for (int i = 0; i < nsigs && !met; i++) met = sigs[i] == L.sig;
        if (met) continue;
        sigs[nsigs++] = L.sig;
        far_hist[L.far / 10 < 19 ? L.far / 10 : 19]++;
        if (L.far < LO[0]) { stuck++; continue; }
        klt_family(&g, second_pass, &L);
        for (int d = LO[0]; d < FAR && d <= L.far; d++) {
            int high = L.seen[d][1] > 0, n = L.seen[d][high] < SAMPLES ? L.seen[d][high] : SAMPLES;
            for (int s = 0; s < n && ncand < MAXC; s++) {
                tray *t = &L.pick[d][high][s];
                klt_game pos;
                pos.np = t->np;
                memcpy(pos.p, t->p, sizeof pos.p);
                cand *c = &cands[ncand++];
                klt_text(&pos, c->text);
                c->key = key_of(t->p, t->np);
                c->dist = d;
                c->size = L.size;
                c->fam = nsigs - 1;
                c->diff = d + 0.8 * log2((double)L.size) + (high ? 0 : 0.5) + 0.01 * s;
            }
        }
        if (lay_no % 500 == 0) fprintf(stderr, "%d trays, %d families, %d candidates\n", lay_no, nsigs, ncand);
    }
    fprintf(stderr, "%d families, %d trays not laid or too large, %d families stuck; the farthest:", nsigs, failed,
            stuck);
    for (int i = 0; i < 20; i++) fprintf(stderr, " %d+:%d", 10 * i, far_hist[i]);
    fprintf(stderr, "\n");
    qsort(cands, (size_t)ncand, sizeof *cands, by_diff);
    static cand out[KLT_LEVELS], pool[MAXC];
    int nout = 0;
    for (int c = 0; c < KLT_CHAPTERS; c++) {
        int npool = 0;
        for (int i = 0; i < ncand; i++)
            if (cands[i].dist >= LO[c] && cands[i].dist <= HI[c]) pool[npool++] = cands[i];
        fprintf(stderr, "chapter %d: %d candidates, %d .. %d moves\n", c + 1, npool, npool ? pool[0].dist : 0,
                npool ? pool[npool - 1].dist : 0);
        for (int i = 0, last = -1; i < PER && npool; i++) {
            double q = c == 0 && i == 0 ? 0 : 0.03 + 0.94 * i / (PER - 1);
            int at = (int)(q * (npool - 1) + 0.5);
            if (at <= last) at = last + 1;
            for (int bad = 1; bad && at < npool - 1;) {   /* not one distance of a family twice, nor a position */
                bad = 0;
                for (int k = 0; k < nout; k++)
                    bad |= out[k].key == pool[at].key || (out[k].fam == pool[at].fam && out[k].dist == pool[at].dist);
                if (bad) at++;
            }
            if (at >= npool) at = npool - 1;
            last = at;
            out[nout++] = pool[at];
        }
    }
    printf("/*\n"
           " * Klotski's levels: our own, made by tools/klotski_gen.c (blocks laid by chance, every position of their family\n"
           " * and its fewest moves found by the rules, six chapters of 25 from the easiest). Each: the blocks (B the great\n"
           " * block, T standing, W lying, S small; their top left cell, row then column), then (in the comment) its fewest\n"
           " * moves and its family's size; the fewest moves again in klt_level_par (the tests check them).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "const char *const klt_levels[KLT_LEVELS] = {\n");
    for (int i = 0; i < nout; i++)
        printf("    \"%s\", /* %d: %d moves, a family of %d */\n", out[i].text, i + 1, out[i].dist, out[i].size);
    printf("};\n\nconst uint8_t klt_level_par[KLT_LEVELS] = {");
    for (int i = 0; i < nout; i++) printf("%s%d,", i % 25 ? " " : "\n    ", out[i].dist);
    printf("\n};\n\nconst uint32_t klt_level_family[KLT_LEVELS] = {");
    for (int i = 0; i < nout; i++) printf("%s%d,", i % 15 ? " " : "\n    ", out[i].size);
    printf("\n};\n");
    free(sigs);
    for (int i = 0; i < nout; i++)
        for (int k = 0; k < i; k++)
            if (out[i].key == out[k].key) return 1;
    return nout == KLT_LEVELS ? 0 : 1;
}
