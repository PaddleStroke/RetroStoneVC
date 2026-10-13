/*
 * What the capture games' bot tests share (tests/ai_<id>_test.c for draughts, lasca, alquerque, fanorona, konane,
 * breakthrough, surakarta, bagh-chal, fox and geese, tafl): the checks' report, the opening at every level, the seat
 * not to move, the cancel, and the levels' work on positions of random games (printed: the calibration).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef AI_CAPTURE_H
#define AI_CAPTURE_H

#include "ai_selfplay.h"

static int cp_fails;

static void cp_check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    cp_fails += !ok;
}

/* the start (reset()): every level answers with a move the game takes (one()), the other seat has none, and a
 * cancelled search gives up at once */
static void cp_basics(const char *id, const sp_game *g, int (*show)(const char *))
{
    char name[200];
    const bot *b = bot_find(id);
    snprintf(name, sizeof name, "registered: bot_find(\"%s\"), played by %s_show()", id, id);
    cp_check(name, b && b->think && b->play == show);
    if (!b || !b->think) return;
    for (int l = AI_EASY; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP] = "";
        g->reset();
        int seat = g->actor(), ok = b->think(seat, l, m, sizeof m);
        snprintf(name, sizeof name, "%s opens with a move the game takes (%s)", sp_name(l), m);
        cp_check(name, ok && g->script(m));
    }
    char m[BOT_MOVE_CAP] = "";
    g->reset();
    cp_check("not the seat to move: no move", !b->think(!g->actor(), AI_HARD, m, sizeof m));
    g->reset();
    ai_cancel();
    double t0 = ai_now_ms();
    int ok = b->think(g->actor(), AI_HARD, m, sizeof m);
    double dt = ai_now_ms() - t0;
    ai_cancel_clear();
    cp_check("cancelled: no move, at once", !ok && dt < 50);
}

/* the bot's move in the position on the "screen" at a level ("" none) */
static const char *cp_move(const char *id, int seat, int level)
{
    static char m[BOT_MOVE_CAP];
    m[0] = 0;
    const bot *b = bot_find(id);
    if (!b || !b->think(seat, level, m, sizeof m)) m[0] = 0;
    return m;
}

/* the levels' work (nodes, ms, depth: a whole turn) on positions of random games, `plies` random moves in and more;
 * choose() searches the position on the "screen" (not checked: the calibration, alone on one core) */
static void cp_levels(int argc, char **argv, const sp_game *g, int plies, int (*choose)(int level, ai_stats *s))
{
    int npos = 8;
    for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 40;
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
        ai_rng r;
        ai_seed(&r, 1234);
        double ms = 0, nodes = 0, depth = 0, worst = 0;
        int done = 0;
        for (int k = 0; k < npos; k++) {
            g->reset();
            for (int i = 0, n = plies + k; i < n && g->result() < 0; i++) {
                char m[BOT_MOVE_CAP];
                if (!g->random_move(&r, m, sizeof m) || !g->script(m)) break;
            }
            if (g->result() >= 0) continue;
            ai_stats s;
            if (!choose(lv, &s)) continue;
            ms += s.ms;
            nodes += s.nodes;
            depth += s.depth;
            if (s.ms > worst) worst = s.ms;
            done++;
        }
        if (done)
            printf("     %-6s %8.0f nodes %6.1f ms (worst %6.1f) depth %4.1f  (%d positions)\n", sp_name(lv),
                   nodes / done, ms / done, worst, depth / done, done);
    }
}

/* self-play (sp_run()), but in the short run (make test) a few games a pairing are too few to judge strength: there
 * only the pairings against the random player keep their target; --full judges them all */
static int cp_run(const sp_game *g, const sp_pair *pairs, int n, int argc, char **argv)
{
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    sp_pair p[64];
    for (int i = 0; i < n && i < 64; i++) {
        p[i] = pairs[i];
        if (!full && p[i].b != SP_RANDOM) p[i].min_pct = 0;
    }
    return sp_run(g, p, n < 64 ? n : 64, argc, argv);
}

#endif
