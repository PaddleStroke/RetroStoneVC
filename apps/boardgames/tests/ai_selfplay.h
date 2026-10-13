/*
 * The bots' self-play harness (tests/ai_<id>_test.c): games of bot against bot and bot against a random player,
 * headless, through the bot's registered think() and the game's notation, on fixed seeds. Every move must be legal
 * (the game's script() check), every game must end, every move must keep to its level's time budget, and each
 * pairing's first player must score at least its target. The games run in parallel (fork, one per core); the results
 * come back through shared memory.
 *   ai_<id>_test               the short run (make test): a twentieth of the games
 *   ai_<id>_test --full        all the games (200 for each game), the table of results
 *   ai_<id>_test -j 8          8 at once (default: the cores; timing is truest with fewer)
 *   ai_<id>_test --dump FILE   each game's moves, a line each (for retrostoneboard --moves: the real script())
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef AI_SELFPLAY_H
#define AI_SELFPLAY_H

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ai.h"
#include "bots.h"

#define SP_RANDOM (-1)                          /* a "level": the random player */
#define SP_MOVES 1024                           /* a game's notation, at most */

typedef struct {
    const char *id;
    int max_plies;                              /* a game longer than this never ends: a failure */
    int opening;                                /* random plies at the start of every game (variety) */
    void (*reset)(void);                        /* a new game in the state the bot reads (<id>_live()) */
    int  (*actor)(void);                        /* the seat to act */
    int  (*result)(void);                       /* -1 playing; else the winner (0, 1), 2 a draw */
    int  (*random_move)(ai_rng *r, char *move, int cap);
    int  (*script)(const char *move);           /* plays it as the game's script() does; 0: refused */
} sp_game;

typedef struct {
    int a, b;                                   /* the levels (SP_RANDOM, AI_EASY..): a is seat 0 in even games */
    int games;                                  /* in the full run */
    int min_pct;                                /* a's score at least (a win 1, a draw 1/2), percent */
} sp_pair;

typedef struct {
    int done, ok, winner, plies;                /* winner: 0 a, 1 b, 2 a draw */
    int moves[AI_LEVELS];
    double ms_sum[AI_LEVELS], ms_max[AI_LEVELS];
    char err[160];
    char notation[SP_MOVES];
} sp_result;

static const char *sp_name(int level)
{
    return level == SP_RANDOM ? "random" : level == AI_EASY ? "easy" : level == AI_MEDIUM ? "medium" : "hard";
}

static void sp_play(const sp_game *g, const bot *b, const sp_pair *p, int k, uint64_t seed, sp_result *r)
{
    ai_rng rng;
    ai_seed(&rng, seed);
    int lv[2] = {k & 1 ? p->b : p->a, k & 1 ? p->a : p->b};   /* the seats' levels */
    g->reset();
    size_t used = 0;
    r->notation[0] = 0;
    for (int ply = 0;; ply++) {
        int res = g->result();
        if (res >= 0) {
            r->winner = res == 2 ? 2 : (res == 0) == !(k & 1) ? 0 : 1;
            r->ok = 1;
            break;
        }
        if (ply >= g->max_plies) { snprintf(r->err, sizeof r->err, "no end after %d plies", ply); break; }
        int seat = g->actor(), level = lv[seat & 1];
        char m[BOT_MOVE_CAP] = "";
        if (ply < g->opening || level == SP_RANDOM) {
            if (!g->random_move(&rng, m, sizeof m)) { snprintf(r->err, sizeof r->err, "no random move"); break; }
        } else {
            ai_set_seed(ai_mix(seed, (uint64_t)ply));
            ai_budget bud = ai_level_budget(level);
            double t0 = ai_now_ms();
            int ok = b->think(seat, level, m, (int)sizeof m);
            double dt = ai_now_ms() - t0;
            r->moves[level]++;
            r->ms_sum[level] += dt;
            if (dt > r->ms_max[level]) r->ms_max[level] = dt;
            if (!ok || !m[0]) {
                snprintf(r->err, sizeof r->err, "ply %d: %s gave no move", ply, sp_name(level));
                break;
            }
            if (bud.ms > 0 && dt > bud.ms + 50) {
                snprintf(r->err, sizeof r->err, "ply %d: %s took %.0f ms (budget %d)", ply, sp_name(level), dt, bud.ms);
                break;
            }
        }
        size_t len = strlen(m);
        if (used + len + 2 < sizeof r->notation) {
            if (used) r->notation[used++] = ' ';
            memcpy(r->notation + used, m, len + 1);
            used += len;
        }
        if (!g->script(m)) {
            snprintf(r->err, sizeof r->err, "ply %d: \"%s\" refused (%s)", ply, m, ply < g->opening ? "opening" :
                     sp_name(level));
            break;
        }
        r->plies = ply + 1;
    }
    r->done = 1;
}

/* runs the pairings; 0: all passed */
static int sp_run(const sp_game *g, const sp_pair *pairs, int npairs, int argc, char **argv)
{
    int full = 0, jobs = (int)sysconf(_SC_NPROCESSORS_ONLN);
    const char *dump = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--full")) full = 1;
        else if (!strcmp(argv[i], "-j") && i + 1 < argc) jobs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump = argv[++i];
    }
    if (jobs < 1) jobs = 1;
    const bot *b = bot_find(g->id);
    if (!b || !b->think) { printf("FAIL %s: no bot registered\n", g->id); return 1; }
    int count[64], first[64], total = 0;
    for (int i = 0; i < npairs && i < 64; i++) {
        count[i] = full ? pairs[i].games : (pairs[i].games + 19) / 20;
        first[i] = total;
        total += count[i];
    }
    sp_result *R = mmap(NULL, sizeof(sp_result) * (size_t)total, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
                        -1, 0);
    int *next = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (R == MAP_FAILED || next == MAP_FAILED) { printf("FAIL mmap\n"); return 1; }
    memset(R, 0, sizeof(sp_result) * (size_t)total);
    *next = 0;
    double t0 = ai_now_ms();
    fflush(stdout);
    for (int w = 0; w < jobs; w++) {
        pid_t pid = fork();
        if (pid < 0) { jobs = w; break; }
        if (pid) continue;
        for (;;) {                              /* a worker: the next game not taken */
            int i = __atomic_fetch_add(next, 1, __ATOMIC_SEQ_CST);
            if (i >= total) _exit(0);
            int pi = 0;
            while (pi + 1 < npairs && i >= first[pi + 1]) pi++;
            int k = i - first[pi];
            sp_play(g, b, &pairs[pi], k, ai_mix(0xb07 + (uint64_t)pi * 1000, (uint64_t)k), &R[i]);
        }
    }
    while (wait(NULL) > 0) {}
    double wall = ai_now_ms() - t0;

    int fails = 0;
    double lv_ms[AI_LEVELS] = {0}, lv_max[AI_LEVELS] = {0};
    long lv_n[AI_LEVELS] = {0};
    printf("%s: %d games (%s), %d at once, %.1f s\n", g->id, total, full ? "full" : "short; --full for all", jobs,
           wall / 1000);
    printf("  %-16s %5s %5s %5s %5s %6s  %s\n", "pairing (a-b)", "games", "a", "b", "draw", "a %", "plies");
    for (int pi = 0; pi < npairs; pi++) {
        int wa = 0, wb = 0, dr = 0, plies = 0, bad = 0;
        for (int i = first[pi]; i < first[pi] + count[pi]; i++) {
            sp_result *r = &R[i];
            if (!r->done || !r->ok) {
                printf("FAIL %s %s-%s game %d: %s\n", g->id, sp_name(pairs[pi].a), sp_name(pairs[pi].b),
                       i - first[pi], r->done ? r->err : "the worker died");
                if (r->notation[0]) printf("     moves: %s\n", r->notation);
                bad++;
                continue;
            }
            wa += r->winner == 0;
            wb += r->winner == 1;
            dr += r->winner == 2;
            plies += r->plies;
            for (int l = 0; l < AI_LEVELS; l++) {
                lv_n[l] += r->moves[l];
                lv_ms[l] += r->ms_sum[l];
                if (r->ms_max[l] > lv_max[l]) lv_max[l] = r->ms_max[l];
            }
        }
        int n = count[pi] - bad, pct = n ? (200 * wa + 100 * dr) / (2 * n) : 0;
        char name[40];
        snprintf(name, sizeof name, "%s-%s", sp_name(pairs[pi].a), sp_name(pairs[pi].b));
        printf("  %-16s %5d %5d %5d %5d %5d%%  %.1f\n", name, count[pi], wa, wb, dr, pct, n ? (double)plies / n : 0);
        if (bad) fails += bad;
        /* a short run's few games are luck: it checks the games are legal, end and keep time; only the full run
         * holds the levels to their targets */
        if (pct < pairs[pi].min_pct && full) {
            printf("FAIL %s %s: a scored %d%%, under %d%%\n", g->id, name, pct, pairs[pi].min_pct);
            fails++;
        }
    }
    printf("  %-8s %8s %9s %9s\n", "level", "moves", "ms/move", "max ms");
    for (int l = 0; l < AI_LEVELS; l++)
        if (lv_n[l])
            printf("  %-8s %8ld %9.1f %9.1f   (budget %d ms)\n", sp_name(l), lv_n[l], lv_ms[l] / lv_n[l], lv_max[l],
                   ai_level_budget(l).ms);
    if (dump) {
        FILE *f = fopen(dump, "w");
        if (f) {
            for (int i = 0; i < total; i++) fprintf(f, "%s\n", R[i].notation);
            fclose(f);
        }
    }
    munmap(R, sizeof(sp_result) * (size_t)total);
    munmap(next, sizeof(int));
    printf("%s: %s\n", g->id, fails ? "FAILED" : "all games legal, ended, in time");
    return fails != 0;
}

#endif
