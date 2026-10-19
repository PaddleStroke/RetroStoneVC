/*
 * Hex Landscape's levels: src/games/hexland_levels.h. Our own levels: no published or commercial set is used. Each
 * level is a landscape painted by the rules' maker (hxl_make, from fixed seeds: blobs of soft terrains, rivers and rails
 * wandering through, cut into its tiles, shuffled and turned onto the stack, some swapped for strays, quests from its
 * regions, a reserve for the bonuses); the chapters bring in the terrains and rules one by one (meadows and forests;
 * fields and villages, and the perfect tile; rivers; quests; rails and all of it). For each place on the curve a few
 * landscapes are made and measured (measure()): the greedy player's score (the best points at each tile), the hint's
 * player (the hint at each tile), random players, and a beam search's (a strong player who knows the whole stack). The
 * goal (three stars) lies between the best naive score and the beam's, nearer the beam's along the levels: from level
 * 4 no naive player reaches it. Of the landscapes made for a place, the one with the most room above the naive players
 * is kept (the intro levels: the least). Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/hxlgen apps/boardgames/tools/hexland_gen.c -lm && \
 *       /tmp/hxlgen apps/boardgames/tests/hexland_lines.h > apps/boardgames/src/games/hexland_levels.h
 * The difficulty table goes to stderr; "/tmp/hxlgen raw" measures the first landscape of each place with the goal at
 * the greedy score (no choosing: the table before the curve was shaped).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define HXL_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/hexland_rules.c"

#define PER (HXL_LEVELS / HXL_CHAPTERS)
#define TRIES 6
#define BEAM 24
#define KIDS 8

typedef struct { int greedy, hint, random, beam, goal, tiles; } metric;

static int rand_play(const hxl_state *from, uint32_t seed)
{
    static hxl_state s;
    static int moves[HXL_MOVES];
    uint32_t r = pr_seed(seed);
    s = *from;
    while (!hxl_over(&s)) {
        int n = hxl_moves(&s, moves);
        if (!n) break;
        int m = moves[pr_below(&r, n)];
        if (!hxl_place(&s, m / 6, m % 6, NULL)) break;
    }
    return s.score;
}

static int hint_play(const hxl_state *from)
{
    static hxl_state s;
    s = *from;
    int c, k;
    while (!hxl_over(&s) && hxl_hint(&s, &c, &k) && hxl_place(&s, c, k, NULL)) {}
    return s.score;
}

/* ---- the beam: the best few states at each tile, each grown by its best few placements ---------------------------- */
typedef struct { hxl_state s; int value, nl; uint32_t key; int16_t line[HXL_STACK]; } node;
static int16_t best_line[HXL_STACK];             /* the beam's best play: its placements (cell * 6 + turn) */
static int best_nl;

static uint32_t key_of(const hxl_state *s)
{
    uint32_t h = 2166136261u;
    for (int c = 0; c < HXL_CELLS; c++)
        if (s->cell[c]) h = (h ^ (uint32_t)c ^ s->cell[c] << 12) * 16777619u;
    return h ^ (uint32_t)s->ns * 31u;
}

static int beam(const hxl_state *from)
{
    static node cur[BEAM], next[BEAM * (KIDS + 1)];
    static int moves[HXL_MOVES];
    static hxl_state scratch;
    int nc = 1, best = -1;
    cur[0].s = *from;
    cur[0].value = from->score;
    cur[0].nl = 0;
    best_nl = 0;
    while (nc) {
        int nn = 0;
        for (int i = 0; i < nc; i++) {
            hxl_state *s = &cur[i].s;
            if (hxl_over(s)) {
                if (s->score > best) {
                    best = s->score;
                    best_nl = cur[i].nl;
                    memcpy(best_line, cur[i].line, sizeof best_line);
                }
                continue;
            }
            int n = hxl_moves(s, moves), km[KIDS + 1], kv[KIDS + 1], nk = 0;
            for (int j = 0; j < n; j++) {
                int nb, v = 8 * worth(s, moves[j] / 6, moves[j] % 6, &scratch, &nb) + nb, p;
                if (nk < KIDS) p = nk++;
                else if (v <= kv[KIDS - 1]) continue;
                else p = KIDS - 1;
                while (p > 0 && kv[p - 1] < v) { km[p] = km[p - 1]; kv[p] = kv[p - 1]; p--; }
                km[p] = moves[j];
                kv[p] = v;
            }
            int hc, hk;                         /* the hint's placement too (it looks a tile ahead) */
            if (hxl_hint(s, &hc, &hk)) {
                int dup = 0;
                for (int j = 0; j < nk && !dup; j++) dup = km[j] == hc * 6 + hk;
                if (!dup) { km[nk] = hc * 6 + hk; kv[nk++] = 0; }
            }
            for (int j = 0; j < nk; j++) {
                node *x = &next[nn];
                x->s = *s;
                if (!hxl_place(&x->s, km[j] / 6, km[j] % 6, NULL)) continue;
                memcpy(x->line, cur[i].line, sizeof x->line);
                x->nl = cur[i].nl;
                x->line[x->nl++] = (int16_t)km[j];
                x->key = key_of(&x->s);
                int dup = 0;
                for (int q = 0; q < nn && !dup; q++) dup = next[q].key == x->key && next[q].s.score == x->s.score;
                if (dup) continue;
                x->value = 8 * x->s.score + kv[j] % 8;
                nn++;
            }
        }
        /* the best BEAM of them */
        nc = 0;
        for (int k = 0; k < BEAM && k < nn; k++) {
            int b = k;
            for (int q = k + 1; q < nn; q++)
                if (next[q].value > next[b].value) b = q;
            if (b != k) { node t = next[k]; next[k] = next[b]; next[b] = t; }
            cur[nc++] = next[k];
        }
    }
    return best;
}

static metric measure(const hxl_game *g)
{
    metric m;
    m.tiles = g->s.ns;
    m.greedy = hxl_greedy(&g->s);
    m.hint = hint_play(&g->s);
    long r = 0;
    for (int i = 0; i < 4; i++) r += rand_play(&g->s, 1000u + (uint32_t)i);
    m.random = (int)(r / 4);
    m.beam = beam(&g->s);
    m.goal = 0;
    return m;
}

/* a place on the curve: its landscape's spec */
static void spec_of(int level, hxl_spec *sp)
{
    int ch = level / PER, i = level % PER;
    static const int STARTS[8] = {0, 1, 4, 5, 2, 0, 3, 1};
    memset(sp, 0, sizeof *sp);
    sp->radius = i < 10 ? 2 : 3;
    sp->start = STARTS[(i + ch) % 8];
    sp->strays = i < 2 ? 0 : 8 + i;
    sp->reserve = 16;
    switch (ch) {
    case 0:                                     /* meadows and forests */
        sp->terrains = 1u << HXL_MEADOW | 1u << HXL_FOREST;
        if (i == 0) { sp->radius = 1; sp->start = 0; }
        if (i == 1) { sp->radius = 2; sp->start = 2; }
        if (i == 2) { sp->radius = 2; sp->start = 0; }
        if (i == 5) sp->start = 3;              /* a hole to fill: the perfect tile */
        break;
    case 1:                                     /* fields and villages */
        sp->terrains = 0x1e;
        if (i == 0) sp->start = 2;
        break;
    case 2:                                     /* rivers */
        sp->terrains = 0x1e;
        sp->rivers = i < 6 ? 1 : 1 + (i % 3 == 2);
        sp->radius = i < 6 ? 2 : 3;
        break;
    case 3:                                     /* quests */
        sp->terrains = 0x1e;
        sp->rivers = i < 8 ? 0 : 1;
        sp->radius = i < 5 ? 2 : 3;
        sp->quests = i < 2 ? 1 : 2 + i / 7;
        sp->exact = i < 4 ? 0 : 50;
        break;
    default:                                    /* rails, and all of it */
        sp->terrains = 0x1e;
        sp->rails = 1;
        sp->rivers = i < 3 ? 0 : 1 + (i >= 12);
        sp->radius = i < 4 ? 2 : i < 16 ? 3 : 4;
        sp->quests = i < 3 ? 0 : 2 + i / 6;
        sp->exact = 50;
        sp->river_quests = 1;
        sp->strays = 12 + i;
        break;
    }
    if (i == PER - 1 && ch > 0) { sp->radius = ch < 4 ? 3 : 4; sp->start = 3; }   /* the showpiece */
}

/* the goal: the best naive score raised towards the beam's along the levels (never past it); the intro levels (1-3,
 * a chapter's first two) a little under the naive score */
static int goal_of(int level, int intro, const metric *m)
{
    int naive = m->greedy > m->hint ? m->greedy : m->hint, top = m->beam > m->hint ? m->beam : m->hint;
    int goal;
    if (intro) goal = (naive * 19 / 20) / 10 * 10;
    else {
        double k = 0.30 + 0.30 * level / (HXL_LEVELS - 1.0);
        goal = (int)(naive + k * (top - naive)) / 10 * 10;
        if (goal <= naive) goal = (naive / 10 + 1) * 10;
    }
    if (goal > top) goal = top / 10 * 10;
    return goal;
}

int main(int argc, char **argv)
{
    int raw = argc > 1 && !strcmp(argv[1], "raw");
    const char *lines = argc > 1 && !raw ? argv[1] : NULL;
    static hxl_game g, keep;
    static char start[4096], stack[8192], reserve[4096];
    static int16_t keep_line[HXL_STACK];
    double sum[HXL_CHAPTERS][7] = {{0}};
    FILE *lf = lines ? fopen(lines, "w") : NULL;
    if (lf)
        fprintf(lf, "/* Hex Landscape: a play of each level reaching its goal (the levels' tool's beam search, "
                    "tools/hexland_gen.c; do not edit) */\nstatic const char *const HXL_LINES[HXL_LEVELS] = {\n");
    if (!raw) {
        printf("/* Hex Landscape's levels: made by tools/hexland_gen.c (our own landscapes, measured by its players; do "
               "not edit).\n * Each level: its first tiles (\"<q><r><tile>\"), its stack, its reserve (the bonuses' "
               "tiles), the goal: the score for\n * three stars (hexland.h). */\n");
        printf("const hxl_level_t hxl_levels[HXL_LEVELS] = {\n");
    }
    fprintf(stderr, "level ch tiles start greedy hint random beam goal goal/beam room naive-stars\n");
    for (int L = 0; L < HXL_LEVELS; L++) {
        hxl_spec sp;
        spec_of(L, &sp);
        int i = L % PER, intro = i < 2 || L < 3, keep_nl = 0;
        double best = -1;
        metric bm = {0};
        for (int t = 0; t < (raw ? 1 : TRIES); t++) {
            hxl_make(&g, &sp, 7919u * (uint32_t)L + 104729u * (uint32_t)t + 17u);
            metric m = measure(&g);
            int naive = m.greedy > m.hint ? m.greedy : m.hint;
            double room = m.beam > 0 ? (double)(m.beam - naive) / m.beam : 0;
            double score = intro ? -room : room;
            if (t == 0 || score > best) {
                best = score;
                bm = m;
                keep = g;
                keep_nl = best_nl;
                memcpy(keep_line, best_line, sizeof keep_line);
            }
        }
        int naive = bm.greedy > bm.hint ? bm.greedy : bm.hint;
        bm.goal = raw ? bm.greedy / 10 * 10 : goal_of(L, intro, &bm);
        keep.goal = bm.goal;
        int nstars = hxl_stars(naive, bm.goal);
        fprintf(stderr, "%5d %2d %5d %5d %6d %4d %6d %4d %4d %9.2f %4.2f %d\n", L + 1, L / PER + 1, bm.tiles, sp.start,
                bm.greedy, bm.hint, bm.random, bm.beam, bm.goal, bm.beam ? (double)bm.goal / bm.beam : 0,
                bm.beam ? (double)(bm.beam - naive) / bm.beam : 0, nstars);
        double *S = sum[L / PER];
        S[0] += bm.tiles;
        S[1] += bm.beam ? (double)bm.goal / bm.beam : 0;
        S[2] += bm.beam ? (double)(bm.beam - naive) / bm.beam : 0;
        S[3] += naive >= bm.goal;
        S[4] += bm.random >= hxl_goal2(bm.goal);
        S[5] += 1;
        S[6] += bm.goal > 0 ? (double)(bm.goal - naive) / bm.goal : 0;
        if (!raw) {
            hxl_text(&keep, start, stack, reserve, (int)sizeof stack);
            printf("    {\"%s\",\n     \"%s\",\n     \"%s\", %d},\n", start, stack, reserve, bm.goal);
        }
        if (lf) {                               /* the beam's play, if it reaches the goal (the naive one's else) */
            fprintf(lf, "    \"");
            if (bm.beam < bm.goal) { keep_nl = 0; fprintf(lf, "="); }   /* (the hint's play reaches it) */
            for (int j = 0; j < keep_nl; j++) {
                int q, r;
                hxl_qr(keep_line[j] / 6, &q, &r);
                fprintf(lf, "%s%d,%d,%d", j ? " " : "", q, r, keep_line[j] % 6);
            }
            fprintf(lf, "\",\n");
        }
        fflush(stdout);
    }
    if (!raw) printf("};\n");
    if (lf) { fprintf(lf, "};\n"); fclose(lf); }
    fprintf(stderr, "chapter tiles goal/beam room above-naive naive3stars random2stars\n");
    for (int c = 0; c < HXL_CHAPTERS; c++)
        fprintf(stderr, "%7d %5.1f %9.2f %4.2f %11.2f %11.0f %12.0f\n", c + 1, sum[c][0] / sum[c][5],
                sum[c][1] / sum[c][5], sum[c][2] / sum[c][5], sum[c][6] / sum[c][5], sum[c][3], sum[c][4]);
    return 0;
}
