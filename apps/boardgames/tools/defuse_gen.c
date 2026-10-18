/*
 * Defuse the Bomb's levels, made by chance and chosen: src/games/defuse_levels.h. Our own levels: no published set
 * is used. Band by band (the plan below), the rules' generator (df_make, defuse.h) grows ONE circuit (the switches
 * and the battery feeding gates, flip-flops, capacitors, a board timer, shared by the three lamps: POWER, DISARM and
 * DETONATE) and keeps it when the solver finds it a puzzle (df_rate, df_spec_ok):
 *   - the start safe and not won, a way to win, its fewest moves (par) in the band's range, in time;
 *   - a trap: a first move that blows or leaves no way to win (only the first three levels may do without);
 *   - an order that matters: some order of the best way's moves loses;
 *   - the obvious move (the one lighting most of DISARM at once) not the right one;
 *   - one network: most controls reach two lamps or more; no gate stuck; POWER can go out;
 *   - from the capacitors on: time matters (a wait is needed, or the bomb blows by itself when left alone);
 *   - a clear drawing (Logic Gates' router), no repeats.
 * Its difficulty: the par, the first moves that lose, how few first moves start a best way, how many orders lose,
 * time, the size (df_rating's score). From each band's candidates the levels are taken evenly over it, from the
 * easiest; each drawing is searched at length (lg_route_best) and stored after its text.
 * The die tab's puzzles come from the spare candidates (DF_POOL of each difficulty: chapters 1-2, 3-4, 5-6), drawn the
 * same way: a search for a good one takes seconds on a phone.
 * The chapters (20 levels each; the free edition plays the first two): 1 one network (AND, OR, NOT; DISARM of one or
 * two conditions); 2 cables cut for good; 3 flip-flops; 4 capacitors (time); 5 the board timer and XOR; 6 everything.
 *   gcc -O2 -DDF_NO_LEVELS -Iapps/boardgames/src -o /tmp/dfgen apps/boardgames/tools/defuse_gen.c \
 *       apps/boardgames/src/games/defuse_rules.c apps/boardgames/src/games/logicgates_rules.c -lm \
 *       && /tmp/dfgen > apps/boardgames/src/games/defuse_levels.h
 * (deterministic: fixed seeds; -DDF_NO_LEVELS: the rules built without the levels, made here; stats on stderr)
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/defuse.h"

#define K(x) (1u << LG_##x)
#define AON (K(AND) | K(OR) | K(NOT))

typedef struct { int n; df_spec sp; } band;

static band plan[] = {
    /* 1: one network; the first three teach */
    {3, {.sw_lo = 2, .sw_hi = 3, .gates_lo = 2, .gates_hi = 3, .kinds = K(AND) | K(OR), .conds = 1, .fan = 40,
         .par_lo = 1, .par_hi = 2, .easy = 1, .cross = 1}},
    {8, {.sw_lo = 3, .sw_hi = 3, .gates_lo = 3, .gates_hi = 4, .kinds = AON, .conds = 2, .fan = 40, .par_lo = 2,
         .par_hi = 3, .cross = 2}},
    {9, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 4, .gates_hi = 5, .kinds = AON, .conds = 2, .fan = 45, .par_lo = 2,
         .par_hi = 4, .cross = 3}},
    /* 2: cables */
    {10, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 3, .gates_hi = 5, .kinds = AON, .conds = 2, .fuses_lo = 1, .fuses_hi = 2,
          .fan = 45, .par_lo = 2, .par_hi = 4, .cross = 3}},
    {10, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 4, .gates_hi = 6, .kinds = AON, .conds = 2, .fuses_lo = 2, .fuses_hi = 3,
          .fan = 50, .par_lo = 2, .par_hi = 5, .cross = 3}},
    /* 3: flip-flops */
    {10, {.sw_lo = 2, .sw_hi = 3, .gates_lo = 3, .gates_hi = 4, .kinds = AON, .conds = 2, .fuses_hi = 1, .ff_lo = 1,
          .ff_hi = 1, .fan = 45, .par_lo = 2, .par_hi = 5, .cross = 3}},
    {10, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 3, .gates_hi = 5, .kinds = AON | K(XOR), .conds = 2, .fuses_hi = 2,
          .ff_lo = 1, .ff_hi = 2, .fan = 50, .par_lo = 2, .par_hi = 6, .cross = 4}},
    /* 4: capacitors */
    {10, {.sw_lo = 3, .sw_hi = 3, .gates_lo = 3, .gates_hi = 4, .kinds = AON, .conds = 2, .fuses_hi = 1, .cap_lo = 1,
          .cap_hi = 1, .fan = 45, .par_lo = 2, .par_hi = 5, .timing = 1, .cross = 3}},
    {10, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 3, .gates_hi = 5, .kinds = AON, .conds = 2, .fuses_hi = 2, .ff_hi = 1,
          .cap_lo = 1, .cap_hi = 2, .fan = 50, .par_lo = 2, .par_hi = 6, .timing = 1, .cross = 4}},
    /* 5: the board timer, XOR */
    {10, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 3, .gates_hi = 5, .kinds = AON | K(XOR), .need = K(XOR), .conds = 2,
          .fuses_hi = 1, .ff_hi = 1, .timer = 1, .fan = 45, .par_lo = 2, .par_hi = 5, .timing = 1, .cross = 4}},
    {10, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 4, .gates_hi = 6, .kinds = AON | K(XOR), .need = K(XOR), .conds = 3,
          .fuses_hi = 2, .ff_hi = 1, .cap_hi = 1, .timer = 1, .fan = 50, .par_lo = 3, .par_hi = 6, .timing = 1,
          .cross = 4}},
    /* 6: everything */
    {10, {.sw_lo = 3, .sw_hi = 5, .gates_lo = 3, .gates_hi = 5, .kinds = AON | K(XOR) | K(NAND) | K(NOR), .conds = 3,
          .fuses_lo = 1, .fuses_hi = 2, .ff_lo = 1, .ff_hi = 1, .cap_lo = 1, .cap_hi = 1, .timer = 1, .fan = 50,
          .par_lo = 3, .par_hi = 7, .timing = 1, .cross = 5, .secs = 90}},
    {10, {.sw_lo = 4, .sw_hi = 5, .gates_lo = 4, .gates_hi = 6, .kinds = AON | K(XOR) | K(NAND) | K(NOR), .conds = 3,
          .fuses_lo = 1, .fuses_hi = 2, .ff_lo = 0, .ff_hi = 2, .cap_lo = 1, .cap_hi = 1, .timer = 1, .fan = 55,
          .par_lo = 3, .par_hi = 8, .timing = 1, .cross = 6, .secs = 90}},
};
#define BANDS ((int)(sizeof plan / sizeof plan[0]))
#define CAND 6                                  /* candidates per level */

typedef struct { char txt[DF_TEXT]; df_rating r; } cand;

static int by_score(const void *a, const void *b)
{
    float x = ((const cand *)a)->r.score, y = ((const cand *)b)->r.score;
    return x < y ? -1 : x > y;
}

static uint64_t keys[DF_LEVELS * CAND * 4], shapes[DF_LEVELS * CAND * 4];
static int nkeys, nshapes;
#define POOLMAX 400
static cand pool[DF_RANDOM][POOLMAX];
static int npool[DF_RANDOM];

static int known(const uint64_t *a, int n, uint64_t k)
{
    for (int i = 0; i < n; i++)
        if (a[i] == k) return 1;
    return 0;
}

/* the circuit's shape: its text without the switches' start (S as s) */
static uint64_t shape_of(const char *txt)
{
    uint64_t h = 0xcbf29ce484222325ull;
    const char *s = strchr(txt, '~');
    for (s = s ? s : txt; *s && *s != '|'; s++) h = (h ^ (uint8_t)(*s == 'S' ? 's' : *s)) * 0x100000001b3ull;
    return h;
}

int main(int argc, char **argv)
{
    static cand C[DF_LEVELS * CAND], chosen[DF_LEVELS];
    static df_level D;
    int tries_max = argc > 1 ? atoi(argv[1]) : 6000;
    int total = 0;
    for (int b = 0; b < BANDS; b++) total += plan[b].n;
    if (total != DF_LEVELS) {
        fprintf(stderr, "the plan has %d levels, not %d\n", total, DF_LEVELS);
        return 1;
    }
    int out = 0, bad = 0;
    for (int b = 0; b < BANDS; b++) {
        int want = plan[b].n * CAND, nc = 0, t;
        uint32_t seed = 1000003u * (uint32_t)(b + 1);
        for (t = 0; t < tries_max * (b >= BANDS - 3 ? 3 : 1) && nc < want; t++) {
            cand *c = &C[nc];
            if (!df_make(seed + (uint32_t)t, &plan[b].sp, 30, c->txt, sizeof c->txt, &c->r)) continue;
            uint64_t sh = shape_of(c->txt);
            if (known(keys, nkeys, c->r.key) || known(shapes, nshapes, sh)) continue;
            keys[nkeys++] = c->r.key;
            shapes[nshapes++] = sh;
            nc++;
        }
        fprintf(stderr, "band %d: %d candidates (%d seeds)\n", b, nc, t);
        if (nc < plan[b].n) { bad = 1; if (!nc) continue; }
        qsort(C, (size_t)nc, sizeof C[0], by_score);
        static int taken[DF_LEVELS * CAND];
        memset(taken, 0, sizeof taken);
        for (int i = 0; i < plan[b].n; i++) {   /* evenly over the difficulty */
            int k = plan[b].n > 1 ? i * (nc - 1) / (plan[b].n - 1) : 0;
            if (k >= nc) k = nc - 1;
            chosen[out++] = C[k];
            taken[k] = 1;
        }
        int g = b < 5 ? 0 : b < 9 ? 1 : 2;     /* the rest: the die tab's, by difficulty */
        for (int k = 0; k < nc && b > 0; k++)
            if (!taken[k] && npool[g] < POOLMAX) pool[g][npool[g]++] = C[k];
    }
    if (bad) return 1;
    printf("/* Defuse the Bomb's levels: made by tools/defuse_gen.c (do not edit). Each one: the roles, then Logic\n"
           " * Gates' text of the circuit with its stored drawing (defuse.h); then the pars. */\n");
    printf("const char *const df_levels[DF_LEVELS] = {\n");
    static uint8_t pars[DF_LEVELS];
    float st[DF_CHAPTERS][8] = {{0}};
    for (int i = 0; i < DF_LEVELS; i++) {
        char full[DF_TEXT + LG_LAYOUT], err[96];
        if (!df_parse(chosen[i].txt, &D, err, sizeof err)) { fprintf(stderr, "level %d: %s\n", i + 1, err); return 1; }
        lg_route_best(&D.L, 200);
        if (!df_text_with_layout(chosen[i].txt, &D, full, sizeof full) || !df_parse(full, &D, err, sizeof err)) {
            fprintf(stderr, "level %d: its drawing: %s\n", i + 1, err);
            return 1;
        }
        df_rating r;
        df_state s0 = df_start(&D);
        if (df_rate(&D, &s0, &r) <= 0 || r.par != chosen[i].r.par) { fprintf(stderr, "level %d: rated anew\n", i + 1); return 1; }
        pars[i] = (uint8_t)r.par;
        printf("    \"%s\",\n", full);
        float lose = (r.lose_first + r.dead_first) / (float)r.moves_first;
        fprintf(stderr, "%3d ch%d par %d ticks %2d lose %d+%d/%d ways %d/%d first %d greedy %d timing %d dl %2d shared %3d "
                        "gates %d x%d %.1f\n", i + 1, df_chapter(i) + 1, r.par, r.ticks, r.lose_first, r.dead_first,
                r.moves_first, r.ways, r.orders, r.nsol, r.greedy_ok, r.timing, r.deadline, r.shared, r.gates,
                D.L.crossings, r.score);
        float *a = st[df_chapter(i)];
        a[0] += r.par;
        a[1] += lose;
        a[2] += r.nsol;
        a[3] += r.orders ? 1 - r.ways / (float)r.orders : 0;
        a[4] += r.timing;
        a[5] += r.shared;
        a[6] += r.gates;
        a[7] += r.score;
    }
    printf("};\nconst uint8_t df_pars[DF_LEVELS] = {");
    for (int i = 0; i < DF_LEVELS; i++) printf("%s%d", !i ? "\n    " : i % 30 ? ", " : ",\n    ", pars[i]);
    printf("\n};\n");
    fprintf(stderr, "chapter: par, first moves losing, first actions that win, orders losing, timing, shared, parts, score\n");
    for (int c = 0; c < DF_CHAPTERS; c++) {
        float n = DF_PER_CHAPTER, *a = st[c];
        fprintf(stderr, "ch%d: par %.1f  lose %2.0f%%  ways-in %.1f  bad orders %2.0f%%  timing %3.0f%%  shared %3.0f%%  "
                        "parts %.1f  score %.1f\n", c + 1, a[0] / n, 100 * a[1] / n, a[2] / n, 100 * a[3] / n,
                100 * a[4] / n, a[5] / n, a[6] / n, a[7] / n);
    }
    /* the die tab's: from each difficulty's spare candidates, evenly over their difficulty, drawn at length too */
    static uint8_t ppars[DF_RANDOM][DF_POOL];
    printf("const char *const df_pool[DF_RANDOM][DF_POOL] = {\n");
    for (int g = 0; g < DF_RANDOM; g++) {
        if (npool[g] < DF_POOL) { fprintf(stderr, "pool %d: %d only\n", g, npool[g]); return 1; }
        qsort(pool[g], (size_t)npool[g], sizeof pool[g][0], by_score);
        printf("  {\n");
        for (int i = 0; i < DF_POOL; i++) {
            const cand *c = &pool[g][i * (npool[g] - 1) / (DF_POOL - 1)];
            char full[DF_TEXT + LG_LAYOUT], err[96];
            if (!df_parse(c->txt, &D, err, sizeof err)) { fprintf(stderr, "pool %d: %s\n", g, err); return 1; }
            lg_route_best(&D.L, 100);
            df_rating r;
            if (!df_text_with_layout(c->txt, &D, full, sizeof full) || !df_parse(full, &D, err, sizeof err)) return 1;
            df_state s0 = df_start(&D);
            if (df_rate(&D, &s0, &r) <= 0 || r.par != c->r.par) { fprintf(stderr, "pool %d: rated anew\n", g); return 1; }
            ppars[g][i] = (uint8_t)r.par;
            printf("    \"%s\",\n", full);
        }
        printf("  },\n");
        fprintf(stderr, "pool %d: %d spare, %d kept\n", g, npool[g], DF_POOL);
    }
    printf("};\nconst uint8_t df_pool_pars[DF_RANDOM][DF_POOL] = {\n");
    for (int g = 0; g < DF_RANDOM; g++) {
        printf("    {");
        for (int i = 0; i < DF_POOL; i++) printf("%s%d", i ? ", " : "", ppars[g][i]);
        printf("},\n");
    }
    printf("};\n");
    return 0;
}
