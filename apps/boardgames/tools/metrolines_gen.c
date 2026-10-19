/*
 * Metro Lines' levels: src/games/metrolines_levels.h. Our own cities: no published or commercial map is used.
 * Each level follows a recipe (the map's size, its rivers, bays and lakes, the stations and how often they come, the
 * lines and tunnels at hand, how fast passengers come, rush hours), tried with fixed seeds; each city made is played
 * by the rules' two planners: the naive one (the nearest way to join each new station, gifts in a fixed order) and
 * the one that looks ahead (each choice tried and played on for a while). Their passengers delivered measure the
 * level: the goals (1, 2 and 3 stars) are set from them by the level's band:
 * - gentle (levels 1-3): the naive planner reaches 3 stars;
 * - thought (then up to 12): the naive planner gets a star or two, never three;
 * - hard (13-24): the naive planner fails even the first goal, the one that looks ahead reaches the last;
 * - hardest (25-30): the same, the last goal near the best the one that looks ahead did.
 * Kept per level: the city where the two planners differ most, and its run in time (the planner reaching the last
 * goal in the band's minutes). The planner's run to the last goal is stored as the level's plan (the tests play it
 * again and check it wins). Deterministic.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/mtlgen apps/boardgames/tools/metrolines_gen.c && \
 *       /tmp/mtlgen > apps/boardgames/src/games/metrolines_levels.h
 * The levels' table (each level's measures, the chapters' summary) goes to stderr.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define MTL_GEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/metrolines_rules.c"

typedef struct {
    int w, h, ns, every;                        /* stations, seconds between two */
    int rivers, bays, lakes;
    int lines0, lines_max, tunnels0, gifts;
    int spawn0, spawn_min, ramp;                /* ticks, ticks, seconds */
    int rush_every, rush_len;                   /* seconds */
    int rare, band;
} recipe;

/*   w   h  ns  ev  rv by lk  l0 lm t0 gf  sp0 spm ramp  rush  rare band */
static const recipe R[MTL_LEVELS] = {
    {8, 10, 7, 26, 0, 0, 0, 2, 3, 0, 0, 64, 11, 240, 0, 0, 0, 0},   /* 1: a few stations, no water */
    {8, 10, 9, 22, 0, 0, 0, 2, 4, 0, 0, 64, 11, 240, 0, 0, 0, 0},
    {8, 11, 10, 20, 0, 0, 1, 3, 4, 0, 0, 62, 11, 240, 0, 0, 1, 0},
    {9, 11, 11, 19, 0, 1, 0, 3, 5, 0, 0, 58, 9, 220, 0, 0, 1, 1},
    {9, 12, 12, 18, 0, 0, 2, 3, 5, 0, 0, 58, 9, 220, 0, 0, 1, 1},
    {9, 12, 13, 17, 0, 1, 1, 3, 5, 0, 0, 56, 9, 220, 0, 0, 2, 1},   /* 6: the chapter's showpiece */
    {9, 12, 11, 19, 1, 0, 0, 3, 5, 2, 1, 58, 9, 220, 0, 0, 1, 1},   /* 7: a river, tunnels */
    {9, 12, 12, 18, 1, 0, 0, 3, 5, 2, 1, 58, 9, 220, 0, 0, 1, 1},
    {9, 13, 13, 17, 1, 1, 0, 3, 5, 2, 1, 56, 9, 220, 0, 0, 1, 1},
    {10, 13, 14, 16, 1, 0, 1, 3, 6, 2, 1, 56, 9, 220, 0, 0, 2, 1},
    {10, 13, 15, 15, 1, 0, 0, 3, 6, 1, 1, 56, 9, 220, 0, 0, 2, 1},
    {10, 13, 15, 15, 1, 1, 0, 3, 6, 1, 1, 54, 8, 200, 0, 0, 2, 2},
    {10, 13, 14, 16, 2, 0, 0, 3, 6, 3, 1, 54, 8, 200, 0, 0, 2, 2},   /* 13: two rivers */
    {10, 13, 15, 15, 1, 0, 2, 3, 6, 2, 1, 54, 8, 200, 0, 0, 2, 2},
    {10, 14, 16, 15, 2, 1, 0, 3, 6, 3, 1, 54, 8, 200, 0, 0, 2, 2},
    {10, 14, 16, 14, 1, 1, 1, 3, 6, 2, 1, 52, 8, 200, 0, 0, 2, 2},
    {10, 14, 17, 14, 2, 0, 1, 3, 6, 3, 1, 52, 8, 200, 0, 0, 3, 2},
    {11, 14, 18, 13, 2, 1, 1, 3, 7, 3, 1, 52, 8, 200, 0, 0, 3, 2},
    {10, 13, 13, 16, 0, 1, 2, 2, 2, 0, 0, 56, 9, 200, 0, 0, 2, 2},   /* 19: two lines only, no tunnels */
    {10, 13, 14, 15, 1, 0, 0, 2, 3, 2, 0, 54, 8, 200, 0, 0, 2, 2},   /* few lines, few tunnels */
    {10, 14, 15, 15, 0, 2, 1, 2, 3, 0, 0, 54, 8, 200, 0, 0, 2, 2},
    {10, 14, 15, 14, 1, 0, 1, 2, 3, 2, 1, 52, 8, 200, 0, 0, 2, 2},
    {11, 14, 16, 14, 0, 2, 2, 3, 3, 0, 0, 52, 8, 200, 0, 0, 3, 2},
    {11, 14, 17, 13, 2, 0, 0, 2, 3, 4, 0, 52, 8, 200, 0, 0, 3, 2},
    {11, 15, 17, 13, 1, 0, 1, 3, 7, 2, 1, 50, 7, 180, 50, 12, 3, 3},   /* 25: rush hours */
    {11, 15, 18, 13, 2, 0, 0, 3, 7, 3, 1, 50, 7, 180, 50, 12, 3, 3},
    {11, 15, 19, 12, 1, 1, 1, 3, 7, 2, 1, 50, 7, 180, 45, 12, 3, 3},
    {12, 16, 20, 12, 2, 1, 0, 3, 7, 3, 1, 48, 7, 180, 45, 12, 4, 3},
    {12, 16, 21, 11, 2, 0, 2, 3, 6, 3, 1, 48, 7, 180, 40, 12, 4, 3},
    {12, 16, 22, 11, 3, 0, 0, 3, 7, 4, 1, 48, 7, 180, 40, 12, 4, 3},
};

static const int MINUTES[4][2] = {{2, 3}, {3, 4}, {3, 5}, {4, 6}};   /* the planner's time to the last goal, by band */

static void bay(mtl_level *L, uint32_t *r)       /* a bay: a river from one side that stops inside the map */
{
    int k = L->nriver;
    mtl_make_river(L, r, pr_below(r, 2));
    int keep = 4 + pr_below(r, 2);
    if (pr_below(r, 2)) {                       /* from the other side: the points the other way round */
        for (int i = 0; i < L->rn[k] / 2; i++) {
            int j = L->rn[k] - 1 - i;
            int8_t tx = L->rx[k][i], ty = L->ry[k][i];
            L->rx[k][i] = L->rx[k][j], L->ry[k][i] = L->ry[k][j];
            L->rx[k][j] = tx, L->ry[k][j] = ty;
        }
    }
    L->rn[k] = keep;
}

static void lake(mtl_level *L, uint32_t *r)
{
    int k = L->nlake++;
    L->lrx[k] = (int8_t)(4 + pr_below(r, 4));
    L->lry[k] = (int8_t)(4 + pr_below(r, 4));
    L->lx[k] = (int8_t)(L->lrx[k] + 4 + pr_below(r, L->w * 4 - 2 * L->lrx[k] - 8 > 1 ? L->w * 4 - 2 * L->lrx[k] - 8 : 1));
    L->ly[k] = (int8_t)(L->lry[k] + 4 + pr_below(r, L->h * 4 - 2 * L->lry[k] - 8 > 1 ? L->h * 4 - 2 * L->lry[k] - 8 : 1));
}

static int make(const recipe *c, uint32_t seed, mtl_level *L)
{
    uint32_t r = pr_seed(seed);
    memset(L, 0, sizeof *L);
    L->w = c->w, L->h = c->h;
    L->lines0 = c->lines0, L->lines_max = c->lines_max, L->tunnels0 = c->tunnels0, L->tunnel_gifts = c->gifts;
    L->week = 40 * MTL_HZ;
    L->spawn0 = c->spawn0, L->spawn_min = c->spawn_min, L->ramp = c->ramp * MTL_HZ;
    L->rush_every = c->rush_every * MTL_HZ, L->rush_len = c->rush_len * MTL_HZ;
    L->seed = pr_next(&r);
    for (int i = 0; i < c->rivers; i++)          /* the second across the first */
        mtl_make_river(L, &r, i == 0 ? (int)pr_below(&r, 3) > 0 : i == 1 ? L->rx[0][0] != -8 : (int)pr_below(&r, 2));
    for (int i = 0; i < c->bays; i++) bay(L, &r);
    for (int i = 0; i < c->lakes; i++) lake(L, &r);
    mtl_make_stations(L, &r, c->ns, c->every * MTL_HZ, c->rare);
    return L->ns >= c->ns - 2;                   /* (a city with much water: a station or two fewer) */
}

static char plan[1 << 16];

typedef struct { int greedy, smart, gt, st, ok; uint32_t seed; double score; } measure;

static int round5(double v) { int x = (int)(v + 2.5); return x - x % 5 > 5 ? x - x % 5 : 5; }

static void goals(const recipe *c, int g, int s, int *out)
{
    switch (c->band) {
    case 0: out[2] = round5((g < s ? g : s) * 0.85); out[1] = round5(out[2] * 0.7); out[0] = round5(out[2] * 0.45); break;
    case 1: out[0] = round5(g * 0.8); out[2] = round5(g + (s - g) * 0.6); out[1] = round5((g + out[2]) / 2.0); break;
    case 2: out[0] = round5(g * 1.06 + 5); out[2] = round5(g + (s - g) * 0.8); out[1] = round5((out[0] + out[2]) / 2.0); break;
    default: out[0] = round5(g * 1.12 + 5); out[2] = round5(s * 0.94); out[1] = round5((out[0] + out[2]) / 2.0); break;
    }
    if (out[1] <= out[0]) out[1] = out[0] + 5;
    if (out[2] <= out[1]) out[2] = out[1] + 5;
}

/* the best of the planner that looks ahead (three horizons): its delivered, the horizon in best_horizon */
static int best_horizon;

static int smart(const mtl_level *L, int *ticks, int max_ticks)
{
    static const int HZN[3] = {15, 25, 40};
    int best = -1;
    for (int h = 0; h < 3; h++) {
        int t, d;
        mtl_bot_horizon = HZN[h] * MTL_HZ;
        d = mtl_bot_run(L, 0, max_ticks, plan, sizeof plan, &t);
        if (d > best) { best = d; *ticks = t; best_horizon = mtl_bot_horizon; }
    }
    return best;
}

int main(void)
{
    static mtl_level chosen[MTL_LEVELS];
    static char plans[MTL_LEVELS][1 << 16];
    measure M[MTL_LEVELS];
    for (int lv = 0; lv < MTL_LEVELS; lv++) {
        const recipe *c = &R[lv];
        measure best = {0};
        best.score = -1e9;
        int nomake = 0;
        for (uint32_t seed = 1; seed <= 24 || (!best.ok && seed <= 60); seed++) {
            mtl_level L;
            if (!make(c, (uint32_t)lv * 1000u + seed, &L)) { nomake++; continue; }
            int gt, st;
            int until = MINUTES[c->band][1] * 60 * MTL_HZ;   /* what each does by the band's last minute */
            int g = mtl_bot_run(&L, 1, until, NULL, 0, &gt);
            int s = smart(&L, &st, until);
            int gl[3];
            goals(c, g, s, gl);
            double ratio = g > 0 ? (double)s / g : 9;
            static const double NEED[4] = {0.0, 1.2, 1.35, 1.5};
            if (g < 10) continue;
            double short_by = ratio < NEED[c->band] ? NEED[c->band] - ratio : 0;   /* (kept if nothing better) */
            /* the planner with the goals: does it win, and when */
            L.goal[0] = gl[0], L.goal[1] = gl[1], L.goal[2] = gl[2];
            int wt, wd;
            mtl_bot_horizon = best_horizon;     /* (the same run as measured: it reaches the goal) */
            wd = mtl_bot_run(&L, 0, 15 * 60 * MTL_HZ, plan, sizeof plan, &wt);
            if (wd < gl[2]) continue;
            double mins = wt / (60.0 * MTL_HZ), lo = MINUTES[c->band][0], hi = MINUTES[c->band][1];
            double score = (ratio > 2.5 ? 2.5 : ratio) - (mins < lo ? lo - mins : mins > hi ? mins - hi : 0) - 20 * short_by;
            if (score > best.score) {
                best = (measure){g, s, gt, wt, 1, seed, score};
                chosen[lv] = L;
                memcpy(plans[lv], plan, sizeof plan);
            }
        }
        M[lv] = best;
        if (!best.ok) { fprintf(stderr, "level %d: no city found\n", lv + 1); return 1; }
        fprintf(stderr, "level %2d band %d seed %2u: naive %4d (%.1f min) ahead %4d  goals %d/%d/%d won at %.1f min\n",
                lv + 1, c->band, best.seed, best.greedy, best.gt / 1500.0, best.smart, chosen[lv].goal[0],
                chosen[lv].goal[1], chosen[lv].goal[2], best.st / 1500.0);
    }
    for (int ch = 0; ch < MTL_CHAPTERS; ch++) {
        double nr = 0, mins = 0;
        int fails = 0, per = MTL_LEVELS / MTL_CHAPTERS;
        for (int lv = ch * per; lv < (ch + 1) * per; lv++) {
            nr += (double)M[lv].greedy / chosen[lv].goal[2];
            mins += M[lv].st / 1500.0;
            fails += M[lv].greedy < chosen[lv].goal[0];
        }
        fprintf(stderr, "chapter %d: the naive planner at %.0f%% of the last goal, fails %d of %d, %.1f min to win\n",
                ch + 1, 100 * nr / per, fails, per, mins / per);
    }
    printf("/* Metro Lines' levels: made by tools/metrolines_gen.c (our own cities, measured by the rules' planners; do\n"
           " * not edit). Each level as text (metrolines_rules.c: mtl_text), then the plan that wins it. */\n");
    printf("const char *const mtl_levels[MTL_LEVELS] = {\n");
    for (int lv = 0; lv < MTL_LEVELS; lv++) {
        char t[4096];
        mtl_text(&chosen[lv], t, sizeof t);
        printf("    \"%s\",\n", t);
    }
    printf("};\n\nconst char *const mtl_plans[MTL_LEVELS] = {\n");
    for (int lv = 0; lv < MTL_LEVELS; lv++) {
        printf("    \"");
        int col = 4;
        for (const char *p = plans[lv]; *p; p++) {
            putchar(*p);
            if (++col > 100 && *p == ' ') { printf("\"\n    \""); col = 4; }
        }
        printf("\",\n");
    }
    printf("};\n\n");
    /* the chapters' pictures: small cities (a few stations; a river; lakes; one line; many stations) */
    static const recipe IC[MTL_CHAPTERS] = {
        {6, 6, 4, 20, 0, 0, 0, 3, 3, 0, 0, 60, 20, 300, 0, 0, 0, 0},
        {6, 6, 4, 20, 1, 0, 0, 3, 3, 2, 0, 60, 20, 300, 0, 0, 0, 0},
        {6, 6, 4, 20, 0, 0, 1, 3, 3, 2, 0, 60, 20, 300, 0, 0, 0, 0},
        {6, 6, 4, 20, 0, 0, 0, 1, 1, 0, 0, 60, 20, 300, 0, 0, 0, 0},
        {6, 6, 6, 20, 1, 0, 0, 3, 3, 2, 0, 60, 20, 300, 0, 0, 2, 0},
    };
    printf("const char *const mtl_icons[MTL_CHAPTERS] = {\n");
    for (int ch = 0; ch < MTL_CHAPTERS; ch++) {
        mtl_level L;
        uint32_t seed = 7;
        while (!make(&IC[ch], 90000u + (uint32_t)ch * 100u + seed, &L)) seed++;
        char t[4096];
        mtl_text(&L, t, sizeof t);
        printf("    \"%s\",\n", t);
    }
    printf("};\n");
    return 0;
}
