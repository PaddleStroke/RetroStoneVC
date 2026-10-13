/*
 * Mystery Face's bot (src/games/mysteryface_bot.c): its registry entry, moves that never depend on the other
 * player's face, the faces put down after an answer, a guess with one face left, then self-play
 * (tests/ai_selfplay.h). Headless: this file plays the screen's part (mysteryface_live(), mysteryface_show()). --full
 * for the 200 games a pairing and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/mysteryface.h"

static mfgame T;                                /* the "screen's" game: the bot reads it */
const mfgame *mysteryface_live(void) { return &T; }
int mysteryface_show(const char *move) { return mf_script(&T, move); }

static int one(const char *m) { return mf_script(&T, m); }
static void reset(void) { mf_new(&T); }
static int actor(void) { return T.pos.turn; }
static int result(void) { return T.pos.phase == MF_OVER ? T.pos.winner : -1; }

/* a random player: a face at random, then a question at random each turn (nothing put down), and one turn in ten a
 * guess among its faces */
static int random_move(ai_rng *r, char *m, int cap)
{
    const mfpos *p = &T.pos;
    if (p->phase == MF_PICK) snprintf(m, (size_t)cap, "p%d e", ai_rand_n(r, MF_FACES));
    else if (p->asked >= 0) snprintf(m, (size_t)cap, "e");
    else if (ai_rand_n(r, 10) == 0) snprintf(m, (size_t)cap, "g%d", ai_rand_n(r, MF_FACES));
    else snprintf(m, (size_t)cap, "q%d e", ai_rand_n(r, MF_QUESTIONS));
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
    const bot *b = bot_find("mysteryface");
    check("registered: bot_find(\"mysteryface\")", b && b->think && b->play == mysteryface_show);

    {   /* picking: a face and the turn ended */
        reset();
        char m[BOT_MOVE_CAP] = "";
        int ok = b->think(0, AI_HARD, m, sizeof m) && one(m) && T.pos.turn == 1 && T.pos.secret[0] >= 0;
        ok &= b->think(1, AI_EASY, m, sizeof m) && one(m) && T.pos.phase == MF_PLAY && T.pos.turn == 0;
        ok &= !b->think(1, AI_HARD, m, sizeof m);   /* not its turn */
        check("each picks a face and ends its turn; not its turn: no move", ok);
    }
    {   /* the other player's face never matters: the same moves whatever it is (answers kept the same) */
        int same = 1;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++)
            for (int other = 0; other < MF_FACES; other++) {
                reset();
                one("p3 e p0 e");
                T.pos.secret[1] = (int8_t)other;
                char m1[BOT_MOVE_CAP] = "", m2[BOT_MOVE_CAP] = "";
                ai_set_seed(5);
                b->think(0, lv, m1, sizeof m1);
                T.pos.secret[1] = (int8_t)((other + 7) % MF_FACES);
                ai_set_seed(5);
                b->think(0, lv, m2, sizeof m2);
                same &= !strcmp(m1, m2) && m1[0] == 'q';
            }
        check("the first question never depends on the other face", same);
    }
    {   /* after an answer, medium and hard put down exactly the faces it rules out, then end the turn */
        int ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++)
            for (int other = 0; other < MF_FACES; other += 5) {
                reset();
                char m[BOT_MOVE_CAP];
                snprintf(m, sizeof m, "p3 e p%d e", other);
                one(m);
                ai_set_seed(9);
                b->think(0, lv, m, sizeof m);
                one(m);
                int q = T.pos.asked;
                for (int k = 0; k < 4 && T.pos.turn == 0; k++) {
                    b->think(0, lv, m, sizeof m);
                    one(m);
                }
                for (int f = 0; f < MF_FACES; f++)
                    ok &= (T.pos.down[0] >> f & 1) == (mf_has(f, q) != mf_has(other, q));
                ok &= T.pos.turn == 1;
            }
        check("after an answer, the faces it rules out go down (and only those), then the turn ends", ok);
    }
    {   /* one face left standing: medium and hard guess it */
        int ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
            reset();
            one("p3 e p9 e");
            T.pos.down[0] = ~(1u << 9) & ((1u << MF_FACES) - 1);
            char m[BOT_MOVE_CAP] = "";
            b->think(0, lv, m, sizeof m);
            ok &= !strcmp(m, "g9");
        }
        check("one face left: it guesses it", ok);
    }
    {   /* the other player has one face left (they win next turn): hard guesses among its faces */
        reset();
        one("p3 e p9 e");
        T.pos.down[1] = ~(1u << 3) & ((1u << MF_FACES) - 1);
        T.pos.down[0] = (1u << 12) - 1;     /* 12 faces left to the bot */
        char m[BOT_MOVE_CAP] = "";
        b->think(0, AI_HARD, m, sizeof m);
        snprintf(name, sizeof name, "the other player sure to win next turn: hard guesses (%s)", m);
        check(name, m[0] == 'g');
    }
    {   /* cancelled: no move */
        char m[BOT_MOVE_CAP] = "";
        reset();
        ai_cancel();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        ai_cancel_clear();
        check("cancelled: no move", !ok);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "mysteryface", .max_plies = 600, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 80},
        {AI_MEDIUM, AI_EASY, 200, 55},
        {AI_HARD, AI_MEDIUM, 200, 42},   /* a game of luck once both ask well: about even */
        {AI_HARD, AI_EASY, 200, 58},
        {AI_HARD, AI_HARD, 40, 0},
    };
    /* the short run (a twentieth of the games: ten a pairing) checks the games; with so few, luck rules: its
     * targets are 30 points looser */
    int quick = 1;
    for (int i = 1; i < argc; i++) quick &= strcmp(argv[i], "--full") != 0;
    for (size_t i = 0; quick && i < sizeof P / sizeof P[0]; i++)
        P[i].min_pct = P[i].min_pct > 30 ? P[i].min_pct - 30 : 0;
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
