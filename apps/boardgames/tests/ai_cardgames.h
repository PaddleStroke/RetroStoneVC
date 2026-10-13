/*
 * What the tests of the shedding, fishing and betting games' bots share (tests/ai_<id>_test.c for the games on
 * games/cardbot.h): the check line and the duplicate-deal runs (tests/ai_tricks.h), the levels' timing table, a whole
 * game played out by bots to its end.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef AI_CARDGAMES_H
#define AI_CARDGAMES_H

#include "ai_selfplay.h"
#include "ai_tricks.h"

#include <time.h>

static inline int cg_full(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--full")) return 1;
    return 0;
}

/* a closer look (the environment): CG_SCALE=k plays k times the full run's games; CG_ONLY=label runs only the
 * runs whose label holds it */
static inline void cg_scale(sp_pair *P, int n)
{
    const char *e = getenv("CG_SCALE");
    for (int i = 0; e && i < n; i++) P[i].games *= atoi(e);
}

static inline int cg_only(const char *label)
{
    const char *e = getenv("CG_ONLY");
    return !e || strstr(label, e) != NULL;
}

/* the levels' work: `setup(k, r)` sets a position (returns the seat to think, -1: none, try again), `choose` thinks;
 * prints each level's iterations and processor times, checks hard keeps to its budget */
/* the processor's time (other jobs share the machine: the wall clock would count their turns too) */
static inline double cg_cpu_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static inline void cg_timing(const char *name, int npos, int (*setup)(int k, ai_rng *r),
                             int (*choose)(int seat, int level, uint64_t seed, ai_stats *st))
{
    ai_rng r;
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
        double ms = 0, worst = 0, nodes = 0;
        int done = 0;
        ai_seed(&r, 99 + (uint64_t)lv);
        for (int k = 0, tries = 0; done < npos && tries < npos * 50; tries++, k++) {
            int seat = setup(k, &r);
            if (seat < 0) continue;
            ai_stats s;
            memset(&s, 0, sizeof s);
            double t0 = cg_cpu_ms();
            choose(seat, lv, 5 + (uint64_t)k, &s);
            double dt = cg_cpu_ms() - t0;
            ms += dt, nodes += s.nodes, done++;
            if (dt > worst) worst = dt;
        }
        if (!done) done = 1;
        printf("     %-12s %-6s %7.0f iterations %6.1f ms cpu (worst %6.1f)\n", name, sp_name(lv), nodes / done, ms / done,
               worst);
        if (lv == AI_HARD) {
            char line[160];
            snprintf(line, sizeof line, "%s: hard keeps to its budget (worst %.0f ms)", name, worst);
            tt_check(line, worst <= ai_level_budget(AI_HARD).ms + 50);
        }
    }
}

/* a whole game, bots of the given levels round the table (level[seat & 1]), to its end: 1 it ended, every move taken */
static inline int cg_whole(const sp_game *g, const bot *b, uint32_t deal, int la, int lb, int max_plies)
{
    ai_rng r;
    ai_seed(&r, deal);
    g->reset();
    char m[BOT_MOVE_CAP];
    snprintf(m, sizeof m, "D%u", deal);
    if (!g->script(m)) return 0;
    for (int ply = 0; ply < max_plies; ply++) {
        if (g->result() >= 0) return 1;
        int seat = g->actor(), lv = seat & 1 ? lb : la;
        m[0] = 0;
        if (lv == SP_RANDOM) {
            if (!g->random_move(&r, m, sizeof m)) return 0;
        } else {
            ai_set_seed(ai_mix(deal, (uint64_t)ply));
            if (!b->think(seat, lv, m, sizeof m)) { printf("     ply %d: no move\n", ply); return 0; }
        }
        if (!g->script(m)) { printf("     ply %d: \"%s\" refused\n", ply, m); return 0; }
    }
    printf("     no end after %d plies\n", max_plies);
    return 0;
}

#endif
