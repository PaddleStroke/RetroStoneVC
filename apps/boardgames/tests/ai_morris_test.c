/*
 * The men's morris games' bot (src/games/morris_bot.c): its registry entry, a few positions (a mill closed, a mill
 * blocked, the removal), the cancel, then self-play at every level in the three variants (tests/ai_selfplay.h).
 * Headless: this file plays the screen's part (morris_live(), morris_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/morris.h"

static mgame T;                                 /* the "screen's" game: the bot reads it */
static int variant;
const mgame *morris_live(void) { return &T; }
int morris_show(const char *move)
{
    int ply = T.ply;
    if (!morris_script(&T, move)) { T.ply = ply; return 0; }
    return T.ply == ply + 1;
}

static void reset(void) { morris_new(&T, variant); }
static int actor(void) { return morris_pos(&T)->side; }
static int result(void)
{
    switch (morris_status(&T)) {
    case MS_PLAY: return -1;
    case MS_FEW: case MS_BLOCKED: return !morris_pos(&T)->side;
    default: return 2;
    }
}

static void name(int pt, char *s)
{
    const mboard *b = morris_board(variant);
    s[0] = (char)('a' + b->x[pt]);
    s[1] = (char)('1' + b->y[pt]);
}

static int random_move(ai_rng *r, char *m, int cap)
{
    mmove l[MORRIS_MAX_MOVES];
    int n = morris_legal(morris_pos(&T), l);
    if (!n || cap < 7) return 0;
    mmove x = l[ai_rand_n(r, n)];
    char *s = m;
    if (x.type == M_TAKE) { *s++ = 'x'; name(x.from, s); s += 2; }
    else if (x.type == M_PLACE) { name(x.to, s); s += 2; }
    else { name(x.from, s); s[2] = '-'; name(x.to, s + 3); s += 5; }
    *s = 0;
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
    const bot *b = bot_find("morris");
    check("registered: bot_find(\"morris\")", b && b->think && b->play == morris_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP];
    variant = MORRIS_NINE;
    {   /* placing: ivory a1 d1, ebony b2 d2 (ebony threatens f2): ivory closes on g1, then breaks ebony's two */
        int all = 1;
        for (int l = AI_MEDIUM; l <= AI_HARD; l++) {
            morris_new(&T, MORRIS_NINE);
            morris_script(&T, "a1 b2 d1 d2");
            all &= b->think(0, l, m, sizeof m) && !strcmp(m, "g1");
            all &= morris_show(m) && b->think(0, l, m, sizeof m) && (!strcmp(m, "xb2") || !strcmp(m, "xd2")) &&
                   morris_show(m);
        }
        check("medium and hard close a mill and remove the right piece", all);
        int seen = 0;
        for (int k = 0; k < 20; k++) {          /* easy: most of the time */
            morris_new(&T, MORRIS_NINE);
            morris_script(&T, "a1 b2 d1 d6");
            ai_set_seed((uint64_t)k);
            seen += b->think(0, AI_EASY, m, sizeof m) && !strcmp(m, "g1");
        }
        ai_set_seed(0);
        char name[80];
        snprintf(name, sizeof name, "easy closes the mill most of the time (%d of 20)", seen);
        check(name, seen >= 12);
        all = 1;
        for (int l = AI_MEDIUM; l <= AI_HARD; l++) {
            morris_new(&T, MORRIS_NINE);
            morris_script(&T, "a1 b2 d1");      /* ebony must stop g1 */
            all &= b->think(1, l, m, sizeof m) && !strcmp(m, "g1");
        }
        check("medium and hard block a mill", all);
    }
    {   /* not its turn: no move; cancelled: no move */
        morris_new(&T, MORRIS_NINE);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work on positions of random games (not checked: the calibration, alone on one core) */
        ai_rng r;
        ai_seed(&r, 1234);
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, nodes = 0, depth = 0, worst = 0;
            int npos = 12;
            for (int k = 0; k < npos; k++) {
                morris_new(&T, MORRIS_NINE);
                for (int i = 0, n = 4 + 3 * k; i < n && result() < 0; i++) {
                    random_move(&r, m, sizeof m);
                    morris_show(m);
                }
                if (result() >= 0) morris_new(&T, MORRIS_NINE);
                ai_stats s;
                mmove x;
                mrb_choose(morris_pos(&T), lv, 5, &x, &s);
                ms += s.ms;
                nodes += s.nodes;
                depth += s.depth;
                if (s.ms > worst) worst = s.ms;
            }
            printf("     %-6s %8.0f nodes %6.1f ms (worst %6.1f) depth %4.1f\n", sp_name(lv), nodes / npos, ms / npos,
                   worst, depth / npos);
        }
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "morris", .max_plies = MORRIS_MAX_PLY - 8, .opening = 2, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = morris_show,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 80},
        {AI_MEDIUM, AI_EASY, 100, 60},
        {AI_HARD, AI_EASY, 60, 75},
        {AI_HARD, AI_MEDIUM, 60, 55},
        {AI_HARD, AI_HARD, 20, 0},
    };
    static const char *NAME[MORRIS_VARIANTS] = {"nine men's", "twelve men's", "six men's"};
    int bad = 0;
    for (variant = 0; variant < MORRIS_VARIANTS; variant++) {
        printf("-- %s\n", NAME[variant]);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
