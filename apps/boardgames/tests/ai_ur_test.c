/*
 * The Royal Game of Ur's bot (src/games/ur_bot.c): its position-only throw and move against the rules' own on random
 * games, its registry entry, the throw it makes ("roll"), a capture and the central rosette it takes, the cancel,
 * then self-play at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part (ur_live(),
 * ur_show(): "roll" throws dice of its own, from the session's seed the harness sets before each think, so every game
 * repeats). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/ur.h"

void urb_throw(upos *p, int value);
void urb_move(upos *p, int from);
int urb_choose(const upos *p, int level, uint64_t seed, ai_stats *stats);

static ugame T;                                 /* the "screen's" game: the bot reads it */
const ugame *ur_live(void) { return &T; }

static int roll_value(void)                     /* four dice, from the session's seed and the throws so far */
{
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), T.pos.throws));
    int v = 0;
    for (int i = 0; i < 4; i++) v += ai_rand_n(&r, 2);
    return v;
}

static int one(const char *m)
{
    if (!strcmp(m, "roll")) return T.pos.phase == UR_THROW && ur_throw(&T, roll_value());
    return ur_script(&T, m);
}
int ur_show(const char *move) { return one(move); }

static void reset(void) { ur_new(&T); }
static int actor(void) { return T.pos.turn; }
static int result(void) { return T.pos.winner; }

static int random_move(ai_rng *r, char *m, int cap)
{
    if (cap < 3) return 0;
    if (T.pos.phase == UR_THROW) {              /* the random player's own dice: a number */
        int v = 0;
        for (int i = 0; i < 4; i++) v += ai_rand_n(r, 2);
        m[0] = (char)('0' + v);
        m[1] = 0;
        return 1;
    }
    int mv = ur_movable(&T.pos), n = __builtin_popcount((unsigned)mv), k = n ? ai_rand_n(r, n) : -1;
    if (k < 0) return 0;
    while (k--) mv &= mv - 1;
    m[0] = (char)('a' + __builtin_ctz((unsigned)mv));
    m[1] = 0;
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* a position: pieces of player 0 and 1 on places (lists end with -1), the rest waiting; player 0 to move, a throw */
static void setup(const int *a, const int *b, int roll)
{
    ur_new(&T);
    for (int i = 0; a[i] >= 0; i++) T.pos.n[0][0]--, T.pos.n[0][a[i]]++;
    for (int i = 0; b[i] >= 0; i++) T.pos.n[1][0]--, T.pos.n[1][b[i]]++;
    T.pos.turn = 0;
    T.pos.phase = UR_THROW;
    ur_throw(&T, roll);
}

int main(int argc, char **argv)
{
    char name[160];
    /* the bot's position-only throw and move are the rules' exactly: 2000 random games, every event */
    ai_rng r;
    ai_seed(&r, 7);
    int same = 1, events = 0;
    for (int gn = 0; gn < 2000 && same; gn++) {
        ur_new(&T);
        upos q = T.pos;
        while (T.pos.phase != UR_OVER && same) {
            char m[8];
            random_move(&r, m, sizeof m);
            if (T.pos.phase == UR_THROW) urb_throw(&q, m[0] - '0');
            else urb_move(&q, m[0] - 'a');
            same = ur_script(&T, m) && !memcmp(&q, &T.pos, sizeof q);
            events++;
        }
    }
    printf("     (%d events)\n", events);
    check("throw and move on the position alone = the rules' (captures, rosettes, the end)", same);

    const bot *b = bot_find("ur");
    check("registered: bot_find(\"ur\")", b && b->think && b->play == ur_show);
    {
        char m[BOT_MOVE_CAP] = "";
        ur_new(&T);
        int ok = b->think(0, AI_HARD, m, sizeof m);
        check("a throw to make: \"roll\"", ok && !strcmp(m, "roll"));
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    {   /* player 0 on 5 and 1, player 1 on 7: a 2 takes it (5 -> 7) or gets 1 to... 3; every level but easy takes */
        static const int a[] = {5, 1, -1}, bb[] = {7, -1};
        int all = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
            setup(a, bb, 2);
            char m[BOT_MOVE_CAP] = "";
            all &= b->think(0, lv, m, sizeof m) && !strcmp(m, "f");
        }
        check("medium and hard send a piece back when they can", all);
    }
    {   /* player 0 on 4 and 2, a 4: 4 -> 8, the central rosette (and another throw), not 2 -> 6 */
        static const int a[] = {4, 2, -1}, bb[] = {3, 1, -1};
        int all = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
            setup(a, bb, 4);
            char m[BOT_MOVE_CAP] = "";
            all &= b->think(0, lv, m, sizeof m) && !strcmp(m, "e");
        }
        check("medium and hard take the central rosette", all);
    }
    {   /* cancelled: hard gives up at once, and says so */
        static const int a[] = {5, 2, 9, -1}, bb[] = {3, 6, 10, -1};
        setup(a, bb, 2);
        char m[BOT_MOVE_CAP] = "";
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work on positions of random games, choices only (not checked: the calibration) */
        int npos = 30;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 200;
        ai_seed(&r, 1234);
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, nodes = 0, depth = 0, worst = 0;
            int k = 0;
            while (k < npos) {
                ur_new(&T);
                int plies = 4 + ai_rand_n(&r, 120);
                for (int i = 0; i < plies && T.pos.phase != UR_OVER; i++) {
                    char m[8];
                    random_move(&r, m, sizeof m);
                    ur_script(&T, m);
                }
                if (T.pos.phase != UR_MOVE || __builtin_popcount((unsigned)ur_movable(&T.pos)) < 2) continue;
                ai_stats s;
                urb_choose(&T.pos, lv, 5, &s);
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
        .id = "ur", .max_plies = 3000, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 70},
        {AI_MEDIUM, AI_EASY, 200, 55},
        {AI_HARD, AI_EASY, 200, 60},
        {AI_HARD, AI_MEDIUM, 200, 50},
        {AI_HARD, AI_HARD, 40, 0},
    };
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
