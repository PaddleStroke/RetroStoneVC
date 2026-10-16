/*
 * Senet's bot (src/games/senet_bot.c): its registry entry, the throw it makes ("roll"), a swap and a piece borne off
 * it takes, the cancel, then self-play at every level (tests/ai_selfplay.h), both variants. Headless: this file plays
 * the screen's part (senet_live(), senet_show(): "roll" throws sticks of its own, from the session's seed the harness
 * sets before each think, so every game repeats). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/senet.h"

int senet_choose(const senet_pos *p, int level, uint64_t seed, ai_stats *stats);

static senet_game T;                            /* the "screen's" game: the bot reads it */
static int variant;
const senet_game *senet_live(void) { return &T; }

static int sticks(ai_rng *r)
{
    int v = 0;
    for (int i = 0; i < 4; i++) v += ai_rand_n(r, 2);
    return v ? v : 5;
}

static int roll_value(void)                     /* four sticks, from the session's seed and the throws so far */
{
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), T.pos.throws));
    return sticks(&r);
}

static int one(const char *m)
{
    if (!strcmp(m, "roll")) return T.pos.phase == SENET_THROW && senet_throw(&T, roll_value());
    return senet_script(&T, m);
}
int senet_show(const char *move) { return one(move); }

static void reset(void) { senet_new(&T, variant); }
static int actor(void) { return T.pos.turn; }
static int result(void) { return T.pos.winner; }

static int random_move(ai_rng *r, char *m, int cap)
{
    if (cap < 4) return 0;
    if (T.pos.phase == SENET_THROW) {           /* the random player's own sticks: a number */
        snprintf(m, (size_t)cap, "%d", sticks(r));
        return 1;
    }
    uint32_t mv = senet_movable(&T.pos);
    int n = __builtin_popcount(mv), k = n ? ai_rand_n(r, n) : -1;
    if (k < 0) return 0;
    while (k--) mv &= mv - 1;
    snprintf(m, (size_t)cap, "s%d", __builtin_ctz(mv));
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* a position: squares of player 0 and 1 (lists end with 0; the others borne off); player 0 to move, a throw */
static void setup(const int *a, const int *b, int roll)
{
    senet_new(&T, SENET_FIVE);
    memset(T.pos.sq, 0, sizeof T.pos.sq);
    int n[2] = {0, 0};
    for (int i = 0; a[i]; i++, n[0]++) T.pos.sq[a[i]] = 1;
    for (int i = 0; b[i]; i++, n[1]++) T.pos.sq[b[i]] = 2;
    T.pos.off[0] = (uint8_t)(5 - n[0]);
    T.pos.off[1] = (uint8_t)(5 - n[1]);
    senet_throw(&T, roll);
}

int main(int argc, char **argv)
{
    char name[160];
    const bot *b = bot_find("senet");
    check("registered: bot_find(\"senet\")", b && b->think && b->play == senet_show);
    {
        char m[BOT_MOVE_CAP] = "";
        senet_new(&T, SENET_FIVE);
        int ok = b->think(0, AI_HARD, m, sizeof m);
        check("a throw to make: \"roll\"", ok && !strcmp(m, "roll"));
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    {   /* ivory on 12 and 3, ebony alone on 15 and far ahead on 22, 23: a 3 swaps 12 with 15 */
        static const int a[] = {12, 3, 0}, bb[] = {15, 22, 23, 0};
        int all = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
            setup(a, bb, 3);
            char m[BOT_MOVE_CAP] = "";
            all &= b->think(0, lv, m, sizeof m) && !strcmp(m, "s12");
        }
        check("medium and hard swap a lone piece back when they can", all);
    }
    {   /* ivory on 28 and 10, a 3: 28 leaves the board */
        static const int a[] = {28, 10, 0}, bb[] = {20, 21, 0};
        int all = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
            setup(a, bb, 3);
            char m[BOT_MOVE_CAP] = "";
            all &= b->think(0, lv, m, sizeof m) && !strcmp(m, "s28");
        }
        check("medium and hard bear a piece off", all);
    }
    {   /* cancelled: hard gives up at once, and says so */
        static const int a[] = {5, 9, 12, 0}, bb[] = {6, 10, 14, 0};
        setup(a, bb, 2);
        char m[BOT_MOVE_CAP] = "";
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work on positions of random games */
        int npos = 30;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 200;
        ai_rng r;
        ai_seed(&r, 1234);
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, nodes = 0, depth = 0, worst = 0;
            int k = 0;
            while (k < npos) {
                senet_new(&T, k & 1);
                int plies = 4 + ai_rand_n(&r, 150);
                for (int i = 0; i < plies && T.pos.phase != SENET_OVER; i++) {
                    char m[8];
                    random_move(&r, m, sizeof m);
                    senet_script(&T, m);
                }
                if (T.pos.phase != SENET_MOVE || __builtin_popcount(senet_movable(&T.pos)) < 2) continue;
                ai_stats s;
                senet_choose(&T.pos, lv, 5, &s);
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
        .id = "senet", .max_plies = 6000, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 65},
        {AI_MEDIUM, AI_EASY, 200, 55},
        {AI_HARD, AI_EASY, 200, 60},
        {AI_HARD, AI_MEDIUM, 200, 45},           /* the sticks decide much: hard only edges medium */
        {AI_HARD, AI_HARD, 40, 0},
    };
    int bad = 0;
    for (variant = 0; variant < SENET_VARIANTS; variant++) {
        printf("-- variant %d (%d pieces)\n", variant, variant ? 7 : 5);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
