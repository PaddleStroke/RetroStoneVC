/*
 * Xiangqi's bot (src/games/xiangqi_bot.c): its registry entry, a free horse, a mate in one, the cancel, the levels'
 * work, then self-play at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (xiangqi_live(), xiangqi_show()). --full for the 200 games a pairing and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/xiangqi.h"

int xqb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats);
void xqb_root(const xgame *g, void *root);

static xgame T;
const xgame *xiangqi_live(void) { return &T; }

static int one(const char *s)
{
    xmove l[XQ_MAX_MOVES];
    if (xq_status(&T) != XQ_PLAY || strlen(s) != 4) return 0;
    int k = xq_parse(l, xq_legal(xq_pos(&T), l), s);
    return k >= 0 && xq_play(&T, l[k]);
}
int xiangqi_show(const char *move) { return one(move); }

static void reset(void) { xq_new(&T); }
static int actor(void) { return xq_pos(&T)->side; }
static int result(void)
{
    xstatus s = xq_status(&T);
    return s == XQ_PLAY ? -1 : s == XQ_MATE || s == XQ_STALEMATE ? !xq_pos(&T)->side : 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    xmove l[XQ_MAX_MOVES];
    int n = xq_legal(xq_pos(&T), l);
    if (!n || cap < 5) return 0;
    xmove x = l[ai_rand_n(r, n)];
    sprintf(m, "%c%c%c%c", 'a' + XQ_X(x.from), '0' + XQ_Y(x.from), 'a' + XQ_X(x.to), '0' + XQ_Y(x.to));
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

static void solve(const char *what, const char *red, const char *black, const char *want, int from)
{
    const bot *b = bot_find("xiangqi");
    for (int l = from; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP] = "", name[160];
        T.ply = 0;
        xq_setup(&T.pos[0], 0, red, black);
        int ok = b->think(0, l, m, sizeof m);
        snprintf(name, sizeof name, "%s (%s: %s)", what, sp_name(l), m);
        check(name, ok && (want ? strstr(want, m) != NULL : 1) && one(m) && (want || result() == 0));
    }
}

int main(int argc, char **argv)
{
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    const bot *b = bot_find("xiangqi");
    check("registered: bot_find(\"xiangqi\")", b && b->think && b->play == xiangqi_show);
    {
        char m[BOT_MOVE_CAP] = "";
        xq_new(&T);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    solve("the free horse taken", "Ke0 Ra4", "Kd9 He4", "a4e4", AI_EASY);
    solve("no move left to black in one (mate or stalemate: both win)", "Kd0 Ra8 Rb7", "Ke9", NULL, AI_EASY);
    {
        char m[BOT_MOVE_CAP] = "";
        xq_new(&T);
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {
        int npos = full ? 24 : 8;
        ai_rng r;
        ai_seed(&r, 1234);
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, nodes = 0, depth = 0, worst = 0;
            int done = 0;
            for (int k = 0; k < npos; k++) {
                xq_new(&T);
                for (int i = 0, n = 6 + 3 * k; i < n && xq_status(&T) == XQ_PLAY; i++) {
                    char m[8];
                    random_move(&r, m, 8);
                    one(m);
                }
                if (xq_status(&T) != XQ_PLAY) continue;
                static uint64_t root[1024];
                xqb_root(&T, root);
                ai_stats s;
                char m[8];
                xqb_choose(root, lv, 5, m, &s);
                ms += s.ms;
                nodes += s.nodes;
                depth += s.depth;
                if (s.ms > worst) worst = s.ms;
                done++;
            }
            if (done)
                printf("     %-6s %8.0f nodes %6.1f ms (worst %6.1f) depth %4.1f\n", sp_name(lv), nodes / done,
                       ms / done, worst, depth / done);
        }
    }
    if (fails) return 1;
    static const sp_game G = {
        .id = "xiangqi", .max_plies = 900, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 65},
        {AI_MEDIUM, AI_EASY, 50, 50},
        {AI_HARD, AI_EASY, 50, 65},
        {AI_HARD, AI_MEDIUM, 50, 50},
    };
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
