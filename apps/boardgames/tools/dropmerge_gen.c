/*
 * Fruit Drop's levels: src/games/dropmerge_levels.h. Our own levels: no published set is used. Four chapters of 15:
 * the first jars (the wide jar, goals of a fruit, a score, a count), brass pegs in the jar, narrow jars with fruits
 * already in them (and chains), then everything together with fewer fruits to spare.
 * A candidate: the chapter's jar (a width, a pattern of pegs, a pile of fruits settled in it: none of a kind touching),
 * a goal of the chapter's size (planned per level: S a fruit, P a score, C a count of a fruit, H a chain), a sequence
 * of fruits drawn as the endless game draws them. The rules' player (dmj_aim: it rates each place by what the fruit
 * would land on) plays it with all its fruits, without noise and with noise (16 games), and a player dropping at random
 * plays it 12 times: the drops each game needed to reach the goal. The level's number of fruits is then the fewest that
 * let the noisy player win as often as a curve says (sure at levels 1-3, then from 0.85 at level 4 down to 0.3 at level
 * 60; at most 72), and the random player's share with that many is its triviality: a candidate the random player wins
 * more than a third of the time (a quarter from chapter 2) is passed over. Each level takes, of a dozen candidates, the
 * one nearest the curve. The fewest drops any game needed is the par, and that game's places are kept as the level's
 * way (the test replays it). The metrics go to stderr. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/dmgen apps/boardgames/tools/dropmerge_gen.c -lm && \
 *       /tmp/dmgen > apps/boardgames/src/games/dropmerge_levels.h
 * or faster, the chapters side by side (the same file; the metrics table on stderr):
 *   (/tmp/dmgen head; for c in 0 1 2 3; do /tmp/dmgen $c > /tmp/dm$c & done; wait; cat /tmp/dm[0-3];
 *       /tmp/dmgen tail) > apps/boardgames/src/games/dropmerge_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define DMJ_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/dropmerge_rules.c"

#define PER (DMJ_LEVELS / DMJ_CHAPTERS)
#define CANDS 12
#define TRIES 16
#define RANDOM 12

static const char *const PLAN[DMJ_CHAPTERS] = {
    "SPCSPCSPSCPSCSP",
    "SPCSHPSCPSHCSPS",
    "SHPCSHPSCHSPCHS",
    "SPCHSPSCHPSCHPS",
};

/* the pegs: places as parts of the jar's width (x) and units down (y) */
typedef struct { float x; int y; } spot;
static const spot PEGS[][7] = {
    {{0.3f, 300}, {0.7f, 300}},
    {{0.25f, 260}, {0.5f, 260}, {0.75f, 260}},
    {{0.5f, 330}, {0.3f, 250}, {0.7f, 250}},
    {{0.33f, 220}, {0.67f, 220}, {0.17f, 330}, {0.5f, 330}, {0.83f, 330}},
    {{0.2f, 200}, {0.8f, 200}, {0.36f, 300}, {0.64f, 300}},
    {{0.5f, 300}},
    {{0.15f, 250}, {0.85f, 250}, {0.5f, 190}},
    {{0.25f, 330}, {0.75f, 330}, {0.5f, 240}, {0.12f, 180}, {0.88f, 180}},
};
static const int NPEGS[] = {2, 3, 3, 5, 4, 1, 3, 5};
#define PATTERNS 8

static int relax;                             /* no candidate in reach: the goals eased, step by step */

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* a candidate for level L from the seed's chance */
static void make(dmj_jar *j, int L, uint32_t *rs)
{
    int c = L / PER, i = L % PER;
    float t = i / (float)(PER - 1);
    char g = PLAN[c][i];
    memset(j, 0, sizeof *j);
    static const int WN[] = {280, 300, 320, 340, 360, 400};
    j->width = c == 0 ? 400 : c == 1 ? (pr_below(rs, 3) ? 400 : 360) : c == 2 ? WN[pr_below(rs, 4)] : WN[pr_below(rs, 6)];
    int pegs = c == 1 || (c == 3 && pr_below(rs, 2));
    if (pegs) {
        int p = pr_below(rs, PATTERNS), r = 6 + pr_below(rs, 4);
        if (p == 5) r = 14 + pr_below(rs, 6);
        for (int k = 0; k < NPEGS[p]; k++)
            j->peg[j->npeg++] = (dmj_peg){(int16_t)(PEGS[p][k].x * j->width), (int16_t)PEGS[p][k].y, (int16_t)r};
    }
    int pre = c == 2 ? 4 + pr_below(rs, 7) : c == 3 ? pr_below(rs, 9) : c == 0 && i >= 9 ? pr_below(rs, 4) : 0;
    for (int k = 0; k < pre; k++) {
        int s = 1 + pr_below(rs, c >= 2 ? 7 : 5), r = dmj_radius[s];
        j->pre[j->npre++] = (dmj_pre){(uint8_t)s, (int16_t)(r + pr_below(rs, j->width - 2 * r + 1)),
                                       (int16_t)(DMJ_JH - r - pr_below(rs, 160))};
    }
    j->goal = g == 'S' ? DMJ_SIZE : g == 'P' ? DMJ_SCORE : g == 'C' ? DMJ_COUNT : DMJ_CHAIN;
    switch (j->goal) {
    case DMJ_SIZE: {
        static const int lo[] = {5, 6, 7, 8}, hi[] = {7, 8, 9, 10};
        j->target = clampi(lo[c] + (int)(t * (hi[c] - lo[c] + 0.99f)) + (pr_below(rs, 3) == 0 ? -1 : 0), 4, 10);
        j->target = clampi(j->target - relax, 4, 10);
        if (L < 3) j->target = 5 + L / 2;
        break;
    }
    case DMJ_SCORE: {
        static const int lo[] = {80, 200, 300, 420}, hi[] = {300, 460, 640, 860};
        j->target = (lo[c] + (int)(t * (hi[c] - lo[c])) + pr_below(rs, 60)) / 10 * 10;
        j->target = j->target * (8 - relax) / 80 * 10;
        if (L < 3) j->target = 60;
        break;
    }
    case DMJ_COUNT: {
        j->tsize = clampi(3 + c + pr_below(rs, 3), 3, 7);
        j->target = clampi(2 + c / 2 + pr_below(rs, 2) + (j->tsize <= 4 ? 1 : 0), 2, 5);
        if (L < 3) j->tsize = 3, j->target = 2;
        break;
    }
    default:
        j->target = clampi(2 + (c >= 2) + (t > 0.6f) + (c == 3 && pr_below(rs, 2)), 2, 4);
        break;
    }
    j->seed = pr_next(rs);
    j->nseq = DMJ_SEQ;
    dmj_world w;
    memset(&w, 0, sizeof w);
    w.rng = pr_seed(j->seed);
    w.J.nseq = 0;
    for (int k = 0; k < DMJ_SEQ; k++) j->seq[k] = (uint8_t)fruit_at(&w, k);
}

/* the jar's fruits at the start: none of a kind touching, all well under the rim */
static int sound(const dmj_jar *j)
{
    static dmj_world w;
    dmj_build(&w, j);
    for (int a = 0; a < w.nb; a++) {
        const dmj_ball *p = &w.b[a];
        if (p->y - p->r < 160 * DMJ_FP) return 0;
        for (int b = a + 1; b < w.nb; b++) {
            const dmj_ball *q = &w.b[b];
            int64_t dx = q->x - p->x, dy = q->y - p->y, rs = p->r + q->r + 3 * DMJ_FP;
            if (p->size == q->size && dx * dx + dy * dy <= rs * rs) return 0;
        }
    }
    return 1;
}

typedef struct {
    dmj_jar j;
    double pbot, prand;
    int par, way[DMJ_SEQ];
    int bneed[TRIES], rneed[RANDOM];            /* the drops each game needed (DMJ_SEQ + 1: never) */
} result;

#define NEVER (DMJ_SEQ + 1)
#define MOST 72                                 /* a level gives this many fruits at most */

/* a candidate played with all its fruits: the player without noise (the way), with noise, and at random */
static void play_all(result *r, int L)
{
    static dmj_world w;
    int ways[DMJ_SEQ];
    r->j.nseq = DMJ_SEQ;
    r->par = NEVER;
    for (int k = 0; k <= TRIES; k++) {
        uint32_t rs = pr_seed((uint32_t)(L * 1000 + k));
        dmj_build(&w, &r->j);
        int e = dmj_play(&w, NULL, 0, k == 0 ? 0 : 160 + 40 * (k % 5), &rs, MOST, ways);
        int need = e == DMJ_WON ? w.drops : NEVER;
        if (k) r->bneed[k - 1] = need;
        if (need < r->par) {
            r->par = need;
            memcpy(r->way, ways, sizeof ways);
        }
    }
    for (int k = 0; k < RANDOM; k++) {
        uint32_t rs = pr_seed((uint32_t)(L * 7000 + k));
        dmj_build(&w, &r->j);
        int xs[DMJ_SEQ];
        for (int d = 0; d < DMJ_SEQ; d++) xs[d] = dmj_radius[DMJ_DROPS] + pr_below(&rs, r->j.width - 2 * dmj_radius[DMJ_DROPS]);
        r->rneed[k] = dmj_play(&w, xs, MOST, 0, &rs, MOST, NULL) == DMJ_WON ? w.drops : NEVER;
    }
}

static double share(const int *need, int n, int fruits)
{
    int w = 0;
    for (int i = 0; i < n; i++) w += need[i] <= fruits;
    return (double)w / n;
}

static void level(int L)
{
    int c = L / PER, i = L % PER;
    float t = i / (float)(PER - 1);
    double target = L < 3 ? 1.0 : 0.85 - 0.55 * (L - 3) / (DMJ_LEVELS - 4.0);
    double cap = L < 3 ? 1.01 : L < PER ? 0.34 : 0.25;   /* the random player's share allowed */
    uint32_t rs = pr_seed((uint32_t)(L * 7919 + 13));
    static result best, cand;
    double bestd = 1e9;
    (void)t;
    for (relax = 0; relax < 6 && bestd >= 1e9; relax++)
    for (int k = 0, got = 0; k < CANDS * 6 && got < CANDS; k++) {
        make(&cand.j, L, &rs);
        if (!sound(&cand.j)) continue;
        play_all(&cand, L);
        if (cand.par >= NEVER) continue;        /* out of reach for the player */
        got++;
        /* the fruits given: the fewest that let the noisy player win as often as the curve says */
        int n = cand.par;
        while (n < MOST && share(cand.bneed, TRIES, n) < target - 1e-9) n++;
        if (L < 3) {                            /* the first ones: every game won, fruits to spare */
            int most = cand.par;
            for (int i = 0; i < TRIES; i++) most = cand.bneed[i] < NEVER && cand.bneed[i] > most ? cand.bneed[i] : most;
            n = most + 2 > MOST ? MOST : most + 2;
        }
        cand.j.nseq = n;
        cand.pbot = share(cand.bneed, TRIES, n);
        cand.prand = share(cand.rneed, RANDOM, n);
        double d = fabs(cand.pbot - target) + (cand.prand > cap ? 10 + cand.prand : 0);
        if (L < 3) d = 1 - cand.pbot + cand.par * 0.01;
        if (d < bestd) {
            bestd = d;
            best = cand;
        }
    }
    result *r = &best;
    static const char *GN[] = {"", "size", "score", "count", "chain"};
    fprintf(stderr, "level %2d ch%d %-5s target %4d tsize %d width %3d pegs %d pre %2d fruits %3d par %3d  p_bot %.2f p_rand %.2f (aim %.2f)%s\n",
            L + 1, c + 1, GN[r->j.goal], r->j.target, r->j.tsize, r->j.width, r->j.npeg, r->j.npre, r->j.nseq, r->par,
            r->pbot, r->prand, target, r->prand >= 0.75 ? "  TRIVIAL" : "");
    printf("    /* %2d: %s %d%s, %d fruits, par %d; won %.0f%% by the player, %.0f%% at random */\n", L + 1, GN[r->j.goal],
           r->j.target, r->j.goal == DMJ_COUNT ? " (size)" : "", r->j.nseq, r->par, 100 * r->pbot, 100 * r->prand);
    printf("    {%d, %d, %d, %d, %d,\n     \"", r->j.goal, r->j.tsize, r->j.target, r->j.width, r->par);
    for (int k = 0; k < r->j.nseq; k++) putchar('0' + r->j.seq[k]);
    printf("\",\n     \"");
    for (int k = 0; k < r->j.npeg; k++) printf("%s%d %d %d", k ? " " : "", r->j.peg[k].x, r->j.peg[k].y, r->j.peg[k].r);
    printf("\",\n     \"");
    for (int k = 0; k < r->j.npre; k++) printf("%s%d %d %d", k ? " " : "", r->j.pre[k].size, r->j.pre[k].x, r->j.pre[k].y);
    printf("\",\n     \"");
    for (int k = 0; k < r->par; k++) printf("%s%d", k ? " " : "", r->way[k]);
    printf("\"},\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
    int head = argc > 1 && !strcmp(argv[1], "head"), tail = argc > 1 && !strcmp(argv[1], "tail");
    int only = argc > 1 && !head && !tail ? atoi(argv[1]) : -1;
    if (head || argc == 1) {
        printf("/* Fruit Drop's levels: made by tools/dropmerge_gen.c (our own jars, played by its player; do not edit).\n"
               " * Each: the goal (dropmerge.h DMJ_*), its fruit's size (a count), the target, the jar's width, the par (the\n"
               " * fewest drops the maker's player needed); the fruits; the pegs (x y r); the fruits in the jar (size x y);\n"
               " * the maker's way (each drop's place, units). */\n");
        printf("const dmj_level_def dmj_levels[DMJ_LEVELS] = {\n");
    }
    if (!head && !tail)
        for (int L = 0; L < DMJ_LEVELS; L++)
            if (only < 0 || L / PER == only) level(L);
    if (tail || argc == 1) printf("};\n");
    return 0;
}
