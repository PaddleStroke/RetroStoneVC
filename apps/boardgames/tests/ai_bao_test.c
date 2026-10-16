/*
 * Bao's bot (src/games/bao_bot.c): its registry entry, a capture that wins it takes, the cancel, the levels' work, then
 * self-play at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part (bao_live(),
 * bao_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/bao.h"

int bao_choose(const bao_pos *p, int level, uint64_t seed, ai_stats *stats);

static bao_game T;
const bao_game *bao_live(void) { return &T; }
static int one(const char *m) { return bao_script(&T, m); }
int bao_show(const char *move) { return one(move); }

static void reset(void) { bao_new(&T); }
static int actor(void) { return bao_now(&T)->turn; }
static int result(void) { return bao_now(&T)->winner; }

static int random_move(ai_rng *r, char *m, int cap)
{
    int mv[2 * BAO_RING], n = bao_moves(bao_now(&T), mv);
    if (!n || cap < 4) return 0;
    bao_name(bao_now(&T), mv[ai_rand_n(r, n)], m);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

int main(int argc, char **argv)
{
    char name[160];
    const bot *b = bot_find("bao");
    check("registered: bot_find(\"bao\")", b && b->think && b->play == bao_show);
    {
        char m[BOT_MOVE_CAP] = "";
        bao_new(&T);
        int ok = b->think(0, AI_HARD, m, sizeof m) && one(m);
        check("a legal first move", ok);
        check("not the seat to move: no move", !b->think(0, AI_HARD, m, sizeof m));
    }
    {   /* ivory can empty ebony's inner row with its one capture (pit 3 anticlockwise, "d2-"), a capture must be
         * taken: every level plays it and wins at once */
        int all = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
            bao_new(&T);
            bao_pos *p = &T.pos[0];
            memset(p->pit, 0, sizeof p->pit);
            p->pit[0][3] = 2, p->pit[0][1] = 1, p->pit[0][12] = 3, p->pit[1][6] = 3, p->pit[1][10] = 4;
            char m[BOT_MOVE_CAP] = "";
            all &= b->think(0, lv, m, sizeof m) && one(m) && bao_now(&T)->winner == 0;
        }
        check("medium and hard take the capture that wins", all);
    }
    {   /* cancelled: hard gives up at once */
        bao_new(&T);
        char m[BOT_MOVE_CAP] = "";
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {
        int npos = 30;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 200;
        ai_rng r;
        ai_seed(&r, 1234);
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, nodes = 0, depth = 0, worst = 0;
            int k = 0;
            while (k < npos) {
                bao_new(&T);
                int plies = ai_rand_n(&r, 40);
                for (int i = 0; i < plies && bao_now(&T)->winner < 0; i++) {
                    char m[8];
                    random_move(&r, m, sizeof m);
                    one(m);
                }
                int mv[2 * BAO_RING];
                if (bao_now(&T)->winner >= 0 || bao_moves(bao_now(&T), mv) < 2) continue;
                ai_stats s;
                bao_choose(bao_now(&T), lv, 5, &s);
                ms += s.ms;
                nodes += s.nodes;
                depth += s.depth;
                if (s.ms > worst) worst = s.ms;
                k++;
            }
            snprintf(name, sizeof name, "     %-6s %8.0f nodes %6.2f ms (worst %6.2f) depth %4.1f", sp_name(lv),
                     nodes / npos, ms / npos, worst, depth / npos);
            puts(name);
        }
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "bao", .max_plies = BAO_MAX_MOVES + 1, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 75},
        {AI_MEDIUM, AI_EASY, 200, 60},
        {AI_HARD, AI_EASY, 200, 70},
        {AI_HARD, AI_MEDIUM, 200, 55},
        {AI_HARD, AI_HARD, 40, 0},
    };
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
