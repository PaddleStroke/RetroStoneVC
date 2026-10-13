/*
 * Ludo's bot (src/games/ludo_bot.c), both rules and two to four players: its position-only throw and move against the
 * rules' own on random games, its registry entry, the throw it makes ("roll"), a capture, the cancel, then self-play
 * at every level for each rule and count (tests/ai_selfplay.h; with three or four players the seats take turns by
 * twos: the even players one level, the odd ones the other, swapped every other game). Headless: this file plays
 * the screen's part (ludo_live(), ludo_show(): "roll" throws a die of its own, from the session's seed the harness
 * sets before each think, so every game repeats). --full for the 200 games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/ludo.h"

void ldb_throw(lpos *p, int value);
void ldb_move(lpos *p, int token);
int ldb_choose(const lpos *p, int level, uint64_t seed, ai_stats *stats);

static lgame T;                                 /* the "screen's" game: the bot reads it */
static int V, NP;                               /* the rules and the players of the games played */
const lgame *ludo_live(void) { return &T; }

static int one(const char *m)
{
    if (!strcmp(m, "roll")) {                   /* a die from the session's seed and the throws so far */
        ai_rng r;
        ai_seed(&r, ai_mix(ai_session_seed(), T.pos.throws));
        return T.pos.phase == LUDO_THROW && ludo_throw(&T, 1 + ai_rand_n(&r, 6));
    }
    return ludo_script(&T, m);
}
int ludo_show(const char *move) { return one(move); }

static void reset(void) { ludo_new(&T, V, NP); }
static int actor(void) { return T.pos.turn; }
/* the winner's side (even or odd players). A game the rules can no longer go on with (LUDO_MAX_EVENTS: a long game of
 * petits chevaux, four players) is scored by the tokens' way, and counted (capped) */
static int *capped;
static int result(void)
{
    if (T.pos.winner >= 0) return T.pos.winner & 1;
    if (T.n < LUDO_MAX_EVENTS - 1) return -1;
    __atomic_fetch_add(capped, 1, __ATOMIC_SEQ_CST);
    int best = 0, bv = -1;
    for (int pl = 0; pl < NP; pl++) {
        int v = 0;
        for (int t = 0; t < LUDO_TOKENS; t++) v += T.pos.at[pl][t] + 1;
        if (v > bv) bv = v, best = pl;
    }
    return best & 1;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (cap < 3) return 0;
    if (T.pos.phase == LUDO_THROW) {            /* the random player's own die */
        m[0] = (char)('1' + ai_rand_n(r, 6));
        m[1] = 0;
        return 1;
    }
    int mv = ludo_movable(&T.pos), n = __builtin_popcount((unsigned)mv), k = n ? ai_rand_n(r, n) : -1;
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

int main(int argc, char **argv)
{
    char name[200];
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    /* the bot's position-only throw and move are the rules' exactly: random games of every rule and count */
    ai_rng r;
    ai_seed(&r, 7);
    int same = 1, events = 0;
    for (int gn = 0; gn < 1200 && same; gn++) {
        V = gn & 1;
        NP = 2 + gn / 2 % 3;
        reset();
        lpos q = T.pos;
        while (T.pos.phase != LUDO_OVER && same && T.n < LUDO_MAX_EVENTS) {
            char m[8];
            random_move(&r, m, sizeof m);
            if (T.pos.phase == LUDO_THROW) ldb_throw(&q, m[0] - '0');
            else ldb_move(&q, m[0] - 'a');
            same = ludo_script(&T, m) && !memcmp(&q, &T.pos, sizeof q);
            events++;
        }
    }
    printf("     (%d events)\n", events);
    check("throw and move on the position alone = the rules' (both rules, 2..4 players)", same);

    const bot *b = bot_find("ludo");
    check("registered: bot_find(\"ludo\")", b && b->think && b->play == ludo_show);
    {
        char m[BOT_MOVE_CAP] = "";
        V = LUDO_CLASSIC, NP = 4;
        reset();
        check("a throw to make: \"roll\"", b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "roll"));
        check("not the seat to move: no move", !b->think(2, AI_HARD, m, sizeof m));
    }
    {   /* player 0: a on 10, b on 20; player 1's token on its own square 30 (the track's 4, 4 squares after a): a 4
         * takes it, b would only walk; every level but easy takes */
        int all = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
            V = LUDO_CLASSIC, NP = 2;
            reset();
            T.pos.at[0][0] = 10;
            T.pos.at[0][1] = 20;
            T.pos.at[1][0] = 40;                /* 40 + 26: the track's 14, 4 after a's 10 */
            ludo_throw(&T, 4);
            char m[BOT_MOVE_CAP] = "";
            all &= b->think(0, lv, m, sizeof m) && !strcmp(m, "a");
        }
        check("medium and hard catch a token in reach", all);
    }
    {   /* cancelled: hard gives up at once, and says so */
        V = LUDO_CLASSIC, NP = 4;
        reset();
        T.pos.at[0][0] = 10;
        T.pos.at[0][1] = 3;
        ludo_throw(&T, 6);
        char m[BOT_MOVE_CAP] = "";
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work on positions of random games, choices only (not checked: the calibration) */
        int npos = full ? 100 : 20;
        ai_seed(&r, 1234);
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0;
            int k = 0;
            while (k < npos) {
                V = k & 1, NP = 2 + k % 3;
                reset();
                int plies = 4 + ai_rand_n(&r, 300);
                for (int i = 0; i < plies && T.pos.phase != LUDO_OVER; i++) {
                    char m[8];
                    random_move(&r, m, sizeof m);
                    ludo_script(&T, m);
                }
                if (T.pos.phase != LUDO_MOVE || __builtin_popcount((unsigned)ludo_movable(&T.pos)) < 2) continue;
                ai_stats s;
                double t0 = ai_now_ms();
                ldb_choose(&T.pos, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt;
                if (dt > worst) worst = dt;
                k++;
            }
            printf("     %-6s %6.2f ms a choice (worst %6.2f)\n", sp_name(lv), ms / npos, worst);
        }
    }
    if (fails) return 1;

    static const struct { int v, np, games; } RUNS[] = {
        {LUDO_CLASSIC, 2, 200}, {LUDO_CLASSIC, 3, 200}, {LUDO_CLASSIC, 4, 200},
        {LUDO_CHEVAUX, 2, 200}, {LUDO_CHEVAUX, 3, 200}, {LUDO_CHEVAUX, 4, 200},
    };
    int bad = 0, only = -1;
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--run")) only = atoi(argv[i + 1]);
    capped = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    for (size_t i = 0; i < sizeof RUNS / sizeof RUNS[0]; i++) {
        if (only >= 0 && (int)i != only) continue;
        V = RUNS[i].v;
        NP = RUNS[i].np;
        int g = RUNS[i].games;
        snprintf(name, sizeof name, "== %s, %d players", V ? "petits chevaux" : "ludo", NP);
        puts(name);
        const sp_game G = {
            .id = "ludo", .max_plies = 6000, .opening = 0, .reset = reset, .actor = actor, .result = result,
            .random_move = random_move, .script = one,
        };
        const sp_pair P[] = {
            {AI_EASY, SP_RANDOM, g, 55},     /* luck counts: loose floors (the short run is 10 games) */
            {AI_MEDIUM, AI_EASY, g, 40},
            {AI_HARD, AI_EASY, g, 40},
            {AI_HARD, AI_MEDIUM, g, 30},
        };
        *capped = 0;
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
        if (*capped)
            printf("     %d games stopped by the rules' LUDO_MAX_EVENTS (%d), scored by the tokens' way\n", *capped,
                   LUDO_MAX_EVENTS);
    }
    return bad;
}
