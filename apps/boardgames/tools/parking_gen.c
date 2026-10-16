/*
 * Traffic Jam's levels: src/games/parking_levels.h. Our own levels: no published or commercial set is used. A lot:
 * vehicles parked by chance by the rules (pkj_fill: the red car in its row, then 5 to 14 cars and trucks where they
 * fit; each lot from a seed of its own); its whole family of positions found by the rules (pkj_family: every
 * position its vehicles can reach, and the fewest moves out of each, searched back from the solved ones). The
 * family's farthest position is its candidate (the one whose text comes first among them, so a family met twice gives
 * the same one, met once only). The difficulty: the fewest moves, and a little more for each vehicle a shortest way
 * moves (pkj_movers: more of the lot to think about). Each chapter has its range of moves and keeps 25 candidates
 * spread evenly over its difficulties; chapter 1 wants few vehicles in the lot (a first look that is easy to read).
 * Deterministic (fixed seeds).
 *   gcc -O2 -fopenmp -Iapps/boardgames/src -o /tmp/pkjgen apps/boardgames/tools/parking_gen.c -lm && \
 *       /tmp/pkjgen > apps/boardgames/src/games/parking_levels.h
 * (-fopenmp: on all the cores; without it the same levels, slower. /tmp/pkjgen stats [lots]: the candidates'
 * moves, counted.)
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define PKJ_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/parking_rules.c"

#define PER (PKJ_LEVELS / PKJ_CHAPTERS)
static int LOTS = 150000;
#define MAXC 400000

static const int LO[PKJ_CHAPTERS] = {3, 7, 11, 16, 21, 27}, HI[PKJ_CHAPTERS] = {6, 10, 15, 20, 26, 99};

typedef struct {
    char text[PKJ_N * PKJ_N + 1];
    int par, movers, cars;
    double diff;
} cand;

static cand *cands;
static int ncand;

/* the family's farthest position whose text comes first */
typedef struct { int far; char text[PKJ_N * PKJ_N + 1]; pkj_game pick; } look;

/* a position's text with its vehicles lettered in the order their first bays come (the same lot, any order) */
static void canon(const pkj_game *g, char *out)
{
    pkj_text(g, out);
    char map[26];
    memset(map, 0, sizeof map);
    char next = 'B';
    for (int c = 0; c < PKJ_N * PKJ_N; c++) {
        if (out[c] == '.' || out[c] == 'A') continue;
        int l = out[c] - 'A';
        if (!map[l]) map[l] = next++;
        out[c] = map[l];
    }
}

static void each(const pkj_game *pos, int dist, void *ctx)
{
    look *L = ctx;
    if (dist < L->far) return;
    char t[PKJ_N * PKJ_N + 1];
    canon(pos, t);
    if (dist > L->far || strcmp(t, L->text) < 0) {
        L->far = dist;
        strcpy(L->text, t);
        L->pick = *pos;
    }
}

static int seen(const char *t)                  /* a hash of the texts kept */
{
    static char (*tab)[PKJ_N * PKJ_N + 1];
    enum { SIZE = 1 << 20 };
    if (!tab) tab = calloc(SIZE, sizeof *tab);
    unsigned h = 2166136261u;
    for (const char *p = t; *p; p++) h = (h ^ (unsigned char)*p) * 16777619u;
    for (h &= SIZE - 1; tab[h][0]; h = (h + 1) & (SIZE - 1))
        if (!strcmp(tab[h], t)) return 1;
    strcpy(tab[h], t);
    return 0;
}

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

int main(int argc, char **argv)
{
    int stats = argc > 1 && !strcmp(argv[1], "stats");
    if (argc > 2) LOTS = atoi(argv[2]);
    cands = malloc(MAXC * sizeof *cands);
    static int hist[128];
    /* each lot from its own seed: the lots can be looked at in any order, on several threads (-fopenmp), and the
     * candidates come out the same (sorted by difficulty, then text) */
#pragma omp parallel for schedule(dynamic, 64)
    for (int lot = 0; lot < LOTS; lot++) {
        uint32_t rng = pr_seed(20261016u + 7919u * (uint32_t)lot);
        pkj_game g;
        pkj_fill(&g, 5 + pr_below(&rng, 10), &rng);
        look L = {-1, "", {0}};
        if (!pkj_family(&g, 30000, each, &L) && !pkj_family(&g, PKJ_FAMILY, each, &L)) continue;
        if (L.far < LO[0]) continue;
        int fresh;
#pragma omp critical(kept)
        fresh = !seen(L.text);
        if (!fresh) continue;
        cand c;
        strcpy(c.text, L.text);
        c.par = L.far;
        pkj_game p;                             /* (from its text: the same whichever lot found it) */
        pkj_parse(&p, L.text);
        c.movers = pkj_movers(&p);
        c.cars = p.nc;
        c.diff = c.par + 0.6 * c.movers;
#pragma omp critical(kept)
        if (ncand < MAXC) {
            cands[ncand++] = c;
            hist[c.par < 127 ? c.par : 127]++;
        }
    }
    if (stats) {
        for (int d = 0; d < 128; d++)
            if (hist[d]) fprintf(stderr, "%3d moves: %d\n", d, hist[d]);
        return 0;
    }
    qsort(cands, (size_t)ncand, sizeof *cands, by_diff);
    printf("/* Traffic Jam's levels: made by tools/parking_gen.c (our own lots, found by its search; do not edit). "
           "Each\n"
           " * lot's 36 bays row by row, and its par: the fewest moves. */\n");
    static cand out[PKJ_LEVELS];
    for (int ch = 0; ch < PKJ_CHAPTERS; ch++) {
        int pool[MAXC / 4], n = 0;
        for (int i = 0; i < ncand && n < MAXC / 4; i++) {
            const cand *c = &cands[i];
            if (c->par < LO[ch] || c->par > HI[ch]) continue;
            if (ch == 0 && (c->cars > 8 || c->movers > 4)) continue;   /* few vehicles, few of them moving */
            if (ch == 1 && c->cars > 11) continue;
            pool[n++] = i;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d candidates\n", ch + 1, n); return 1; }
        for (int k = 0; k < PER; k++) {         /* spread evenly over the pool, from its easiest to its hardest */
            int i = pool[(int)((double)k * (n - 1) / (PER - 1) + 0.5)];
            out[ch * PER + k] = cands[i];
        }
        fprintf(stderr, "chapter %d: %d candidates, moves %d .. %d\n", ch + 1, n, out[ch * PER].par,
                out[ch * PER + PER - 1].par);
    }
    printf("const char *const pkj_levels[PKJ_LEVELS] = {\n");
    for (int l = 0; l < PKJ_LEVELS; l++) printf("    \"%s\",\n", out[l].text);
    printf("};\n\nconst uint8_t pkj_level_par[PKJ_LEVELS] = {");
    for (int l = 0; l < PKJ_LEVELS; l++) printf("%s%d,", l % 25 ? " " : "\n    ", out[l].par);
    printf("\n};\n");
    return 0;
}
