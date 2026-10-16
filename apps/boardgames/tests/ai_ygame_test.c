/*
 * Y's bot (src/games/ygame_bot.c): its registry entry, a few positions (the win taken, the other's win stopped, the
 * swap), the cancel, the levels' work, then self-play at every level on the three boards (tests/ai_selfplay.h).
 * Headless: this file plays the screen's part (ygame_live(), ygame_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/ygame.h"

static ygame T;                                 /* the "screen's" game: the bot reads it */
static int size = 9;
const ygame *ygame_live(void) { return &T; }
int ygame_show(const char *move) { return yg_script(&T, move) && strlen(move) > 0; }

static void reset(void) { yg_new(&T, size); }
static int actor(void) { return yg_turn(&T); }
static int result(void) { return T.winner; }

static int random_move(ai_rng *r, char *m, int cap)
{
    int legal[YG_CELLS], n = 0;
    for (int c = 0; c < yg_count(T.n); c++)
        if (yg_legal(&T, c)) legal[n++] = c;
    if (!n) return 0;
    yg_name(legal[ai_rand_n(r, n)], m, cap);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* from `lo` up, every level answers the position (n, moves) with the move `want` */
static int answers(int n, const char *moves, int lo, const char *want)
{
    const bot *b = bot_find("ygame");
    int all = 1;
    for (int l = lo; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP];
        yg_new(&T, n);
        if (!yg_script(&T, moves)) { printf("     bad moves %s\n", moves); return 0; }
        int ok = b->think(yg_turn(&T), l, m, sizeof m) && !strcmp(m, want);
        if (!ok) printf("     %s: %s (want %s)\n", sp_name(l), m, want);
        all &= ok;
    }
    return all;
}

int main(int argc, char **argv)
{
    const bot *b = bot_find("ygame");
    check("registered: bot_find(\"ygame\")", b && b->think && b->play == ygame_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP], nm[160];
    /* 9: ivory joins the left (a5) and the right (e5) through row 5, and comes down to c8; d9 is ebony's: c9 wins */
    check("medium and hard take the win",
          answers(9, "a5 a1 b5 a2 c5 b3 d5 a3 c6 a4 c7 a6 c8 a7 e5 d9", AI_MEDIUM, "c9"));
    check("medium and hard stop the other's win",
          answers(9, "a5 a1 b5 a2 c5 b3 d5 a3 c6 a4 c7 a6 c8 d9 e5", AI_MEDIUM, "c9"));
    /* the swap: a stone off the edge is swapped, one on the edge is not */
    check("hard swaps a central opening", answers(13, "e9", AI_HARD, "swap"));
    {
        yg_new(&T, 13);
        yg_script(&T, "a13");
        check("hard keeps a corner opening", b->think(1, AI_HARD, m, sizeof m) && strcmp(m, "swap") != 0);
    }
    {   /* not its turn: no move; cancelled: no move, at once */
        yg_new(&T, 19);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        yg_script(&T, "a1 b2");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work (not checked: the calibration, alone on one core) */
        static const int SIZES[3] = {9, 13, 19};
        for (int si = 0; si < 3; si++)
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                int n = SIZES[si], npos = 6;
                ai_rng r;
                ai_seed(&r, 99);
                double ms = 0, nodes = 0, worst = 0;
                for (int k = 0; k < npos; k++) {
                    yg_new(&T, n);
                    for (int i = 0, z = 2 + 3 * k; i < z && T.winner < 0; i++) {
                        random_move(&r, m, sizeof m);
                        ygame_show(m);
                    }
                    if (T.winner >= 0) yg_new(&T, n);
                    ai_stats s;
                    ygb_choose(&T, lv, 5, &s);
                    ms += s.ms;
                    nodes += s.nodes;
                    if (s.ms > worst) worst = s.ms;
                }
                snprintf(nm, sizeof nm, "     %2d %-6s %8.0f playouts %6.1f ms (worst %6.1f)", n, sp_name(lv),
                         nodes / npos, ms / npos, worst);
                printf("%s\n", nm);
            }
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "ygame", .max_plies = YG_CELLS + 2, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = ygame_show,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 85},
        {AI_MEDIUM, AI_EASY, 100, 55},
        {AI_HARD, AI_EASY, 60, 80},
        {AI_HARD, AI_MEDIUM, 60, 60},
        {AI_HARD, AI_HARD, 20, 0},
    };
    static const int SIZES[3] = {9, 13, 19};
    int bad = 0;
    for (int si = 0; si < 3; si++) {
        size = SIZES[si];
        printf("-- %d\n", size);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
