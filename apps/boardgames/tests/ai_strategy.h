/*
 * What the strategy games' bot tests share (tests/ai_<id>_test.c for mancala, amazons, lines of action, Chinese
 * checkers, marble push, spread, rithmomachia): the checks' report, the start at every level, the seat not to move, the
 * cancel, the levels' work on positions of random games (printed: the calibration), and the self-play run whose short
 * form (make test) only holds the pairings against the random player to their target (a few games are too few to
 * judge the others; --full judges them all).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef AI_STRATEGY_H
#define AI_STRATEGY_H

#include <time.h>

#include "ai_selfplay.h"

static int st_fails;

static void st_check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    st_fails += !ok;
}

static int st_full(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--full")) return 1;
    return 0;
}

/* the bot's move for `seat` in the position on the "screen", at a level ("": none) */
static const char *st_move(const char *id, int seat, int level)
{
    static char m[BOT_MOVE_CAP];
    const bot *b = bot_find(id);
    m[0] = 0;
    if (!b || !b->think(seat, level, m, sizeof m)) m[0] = 0;
    return m;
}

/* from the start (reset()): registered with its show(), every level answers with a move the game takes, another seat
 * gets nothing, and a cancelled search gives up at once */
static void st_basics(const char *id, const sp_game *g, int (*show)(const char *), int seats)
{
    char name[200];
    const bot *b = bot_find(id);
    snprintf(name, sizeof name, "registered: bot_find(\"%s\"), played by %s_show()", id, id);
    st_check(name, b && b->think && b->play == show);
    if (!b || !b->think) return;
    for (int l = AI_EASY; l <= AI_HARD; l++) {
        g->reset();
        const char *m = st_move(id, g->actor(), l);
        snprintf(name, sizeof name, "%s starts with a move the game takes (%s)", sp_name(l), m);
        st_check(name, m[0] && g->script(m));
    }
    g->reset();
    st_check("not the seat to move: no move", !st_move(id, (g->actor() + 1) % seats, AI_HARD)[0]);
    g->reset();
    ai_cancel();
    double t0 = ai_now_ms();
    char m[BOT_MOVE_CAP] = "";
    int ok = b->think(g->actor(), AI_HARD, m, sizeof m);
    double dt = ai_now_ms() - t0;
    ai_cancel_clear();
    st_check("cancelled: no move, at once", !ok && dt < 50);
}

/* the levels' work on positions of random games (`plies` random moves in, and more each time): choose() searches the
 * position on the "screen" and fills the stats (not checked: the calibration; truest alone on one core) */
static void st_levels(int argc, char **argv, const sp_game *g, int plies, int (*choose)(int level, ai_stats *s))
{
    int npos = st_full(argc, argv) ? 40 : 8;
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
        ai_rng r;
        ai_seed(&r, 1234);
        double ms = 0, nodes = 0, depth = 0, worst = 0;
        int done = 0;
        double cpu = 0;
        for (int k = 0; k < npos; k++) {
            g->reset();
            for (int i = 0, n = plies + k; i < n && g->result() < 0; i++) {
                char m[BOT_MOVE_CAP];
                if (!g->random_move(&r, m, sizeof m) || !g->script(m)) break;
            }
            if (g->result() >= 0) continue;
            ai_stats s;
            clock_t c0 = clock();
            if (!choose(lv, &s)) continue;
            cpu += (double)(clock() - c0) * 1000 / CLOCKS_PER_SEC;
            ms += s.ms;
            nodes += s.nodes;
            depth += s.depth;
            if (s.ms > worst) worst = s.ms;
            done++;
        }
        if (done)
            printf("     %-6s %8.0f nodes %6.1f ms (worst %6.1f; cpu %6.1f) depth %4.1f  (%d positions)\n",
                   sp_name(lv), nodes / done, ms / done, worst, cpu / done, depth / done, done);
    }
}

/* sp_run(), the short run judging only the pairings against the random player */
static int st_run(const sp_game *g, const sp_pair *pairs, int n, int argc, char **argv)
{
    if (getenv("AI_LEVELS_ONLY")) return 0;      /* the calibration alone */
    int full = st_full(argc, argv);
    sp_pair p[64];
    if (n > 64) n = 64;
    for (int i = 0; i < n; i++) {
        p[i] = pairs[i];
        if (!full && p[i].b != SP_RANDOM) p[i].min_pct = 0;
    }
    return sp_run(g, p, n, argc, argv);
}

#endif
