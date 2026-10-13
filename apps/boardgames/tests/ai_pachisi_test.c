/*
 * Pachisi's bot (src/games/pachisi_bot.c), two and four players: its position-only throw, move and decline against
 * the rules' own on random games, its registry entry, the throw it makes ("roll"), a capture, the cancel, a very long
 * game (past the rules' history), then self-play at every level (tests/ai_selfplay.h; four players: partners 0 and 2
 * one level, 1 and 3 the other, swapped every other game). Headless: this file plays the screen's part
 * (pachisi_live(), pachisi_show(): "roll" throws cowries of its own, from the session's seed the harness sets before
 * each think, so every game repeats). --full for the 200 games and the tables; --run n for one player count only
 * (0: two, 1: four).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/pachisi.h"

void pcb_throw(ppos *p, int mouths);
void pcb_move(ppos *p, int piece);
void pcb_decline(ppos *p);
int pcb_choose(const ppos *p, int level, uint64_t seed, ai_stats *stats);

static pgame T;                                 /* the "screen's" game: the bot reads it */
static int NP;
const pgame *pachisi_live(void) { return &T; }

static int cowries(ai_rng *r)
{
    int m = 0;
    for (int i = 0; i < 6; i++) m += ai_rand_n(r, 2);
    return m;
}

static int one(const char *m)
{
    if (!strcmp(m, "roll")) {                   /* cowries from the session's seed and the throws so far */
        ai_rng r;
        ai_seed(&r, ai_mix(ai_session_seed(), T.pos.throws));
        return T.pos.phase == PA_THROW && pa_throw(&T, cowries(&r));
    }
    return pa_script(&T, m);
}
int pachisi_show(const char *move) { return one(move); }

static void reset(void) { pa_new(&T, NP); }
static int actor(void) { return T.pos.turn; }
static int result(void) { return T.pos.winner < 0 ? -1 : T.pos.winner & 1; }

/* the random player: its own cowries, a piece that can move at random (it never declines) */
static int random_move(ai_rng *r, char *m, int cap)
{
    if (cap < 4) return 0;
    if (T.pos.phase == PA_THROW) {
        snprintf(m, (size_t)cap, "%d", pa_value(cowries(r)));
        return 1;
    }
    int mv = pa_movable(&T.pos), n = __builtin_popcount((unsigned)mv), k = n ? ai_rand_n(r, n) : -1;
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
    int full = 0, only = -1;
    for (int i = 1; i < argc; i++) {
        full |= !strcmp(argv[i], "--full");
        if (!strcmp(argv[i], "--run") && i + 1 < argc) only = atoi(argv[i + 1]);
    }
    /* the bot's position-only throw, move and decline are the rules' exactly: random games, both counts */
    ai_rng r;
    ai_seed(&r, 7);
    int same = 1, events = 0;
    for (int gn = 0; gn < 600 && same; gn++) {
        NP = gn & 1 ? 4 : 2;
        reset();
        ppos q = T.pos;
        while (T.pos.phase != PA_OVER && same && T.n < PA_MAX_EVENTS - 1) {
            char m[8];
            if (T.pos.phase == PA_MOVE && ai_rand_n(&r, 20) == 0) {
                strcpy(m, "-");
                pcb_decline(&q);
            } else {
                random_move(&r, m, sizeof m);
                if (T.pos.phase == PA_THROW) pcb_throw(&q, pa_mouths(atoi(m)));
                else pcb_move(&q, m[0] - 'a');
            }
            same = pa_script(&T, m) && !memcmp(&q, &T.pos, sizeof q);
            events++;
        }
    }
    printf("     (%d events)\n", events);
    check("throw, move and decline on the position alone = the rules' (two and four players)", same);

    const bot *b = bot_find("pachisi");
    check("registered: bot_find(\"pachisi\")", b && b->think && b->play == pachisi_show);
    {
        char m[BOT_MOVE_CAP] = "";
        NP = 4;
        reset();
        check("a throw to make: \"roll\"", b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "roll"));
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    {   /* two players: army 0's a on 20 (the track's 12), b on 27 (19), army 1's piece on its 14 (the track's 23, no
         * castle): a 4 takes it with b; a would walk, e (army 2) would come out */
        int all = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
            NP = 2;
            reset();
            T.pos.at[0][0] = 20;                /* track (20 - 8) = 12 */
            T.pos.at[0][1] = 27;                /* track 19 */
            T.pos.at[1][0] = 14;                /* track 17 + 14 - 8 = 23: four squares after b */
            T.pos.out = 3;
            pa_throw(&T, 4);                    /* a 4 */
            char m[BOT_MOVE_CAP] = "";
            all &= b->think(0, lv, m, sizeof m) && !strcmp(m, "b");
        }
        check("medium and hard take a piece in reach", all);
    }
    {   /* cancelled: hard gives up at once, and says so */
        NP = 2;
        reset();
        T.pos.at[0][0] = 20;
        T.pos.at[0][1] = 30;
        T.pos.at[2][0] = 40;
        T.pos.out = 5;
        pa_throw(&T, 3);
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
                NP = k & 1 ? 4 : 2;
                reset();
                int plies = 4 + ai_rand_n(&r, 400);
                for (int i = 0; i < plies && T.pos.phase != PA_OVER; i++) {
                    char m[8];
                    random_move(&r, m, sizeof m);
                    pa_script(&T, m);
                }
                if (T.pos.phase != PA_MOVE) continue;
                ai_stats s;
                double t0 = ai_now_ms();
                pcb_choose(&T.pos, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt;
                if (dt > worst) worst = dt;
                k++;
            }
            printf("     %-6s %6.2f ms a choice (worst %6.2f)\n", sp_name(lv), ms / npos, worst);
        }
    }
    {   /* a very long game: four players, 3 x PA_MAX_EVENTS events of throws declined, then bots of random levels
         * (easy, medium) to the end: the rules keep going (their history forgets its oldest events), and undo still
         * takes a choice back */
        NP = 4;
        reset();
        int ok = 1, undone = 0, lv[PA_ARMIES];
        for (int i = 0; i < 3 * PA_MAX_EVENTS / 2 && ok; i++) ok = pa_throw(&T, 3) && pa_decline(&T);
        ai_seed(&r, 11);
        for (int i = 0; i < PA_ARMIES; i++) lv[i] = ai_rand_n(&r, 2);
        int ply = 0;
        for (; ok && T.pos.phase != PA_OVER && ply < 30000; ply++) {
            char m[BOT_MOVE_CAP] = "";
            int seat = T.pos.turn;
            ai_set_seed(ai_mix(77, (uint64_t)ply));
            ok = b->think(seat, lv[seat], m, sizeof m);
            if (ok && T.pos.phase == PA_MOVE && ply % 7 == 0) {   /* undo, then the same choice again */
                ppos before = T.pos;
                ok = one(m) && pa_undo(&T) && !memcmp(&before, &T.pos, sizeof before);
                undone += ok;
            }
            ok = ok && one(m) && T.n <= PA_MAX_EVENTS;
        }
        char name[160];
        snprintf(name, sizeof name, "a game of %d events and %d more: it ends, undo works (%d undone)",
                 3 * PA_MAX_EVENTS, ply, undone);
        check(name, ok && T.pos.phase == PA_OVER && undone > 10);
    }

    if (fails) return 1;

    static const int COUNTS[] = {2, 4};
    int bad = 0;
    for (int i = 0; i < 2; i++) {
        if (only >= 0 && i != only) continue;
        NP = COUNTS[i];
        printf("== %d players\n", NP);
        const sp_game G = {
            .id = "pachisi", .max_plies = PA_MAX_EVENTS - 2, .opening = 0, .reset = reset, .actor = actor,
            .result = result, .random_move = random_move, .script = one,
        };
        const sp_pair P[] = {
            {AI_EASY, SP_RANDOM, 200, 70},
            {AI_MEDIUM, AI_EASY, 200, 50},
            {AI_HARD, AI_EASY, 200, 55},
            {AI_HARD, AI_MEDIUM, 200, 50},
        };
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
