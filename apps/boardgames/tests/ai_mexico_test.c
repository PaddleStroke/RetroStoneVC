/*
 * Mexico's bot (src/games/mexico_bot.c), three and six lives, two to six players: its registry entry, the throw it
 * makes ("t??"), the truth told when the throw reaches the value, a lie when it must, a call of a value hardly ever
 * thrown, its view (the dice under the cup changed before it throws: the same move), "n" after a call, the cancel;
 * then self-play at every level for both lives and two to six players (tests/ai_selfplay.h; three players or more:
 * the even players one level, the odd ones the other). Headless: this file plays the screen's part (mexico_live(),
 * mexico_show(): the faces of a bot's throw from the session's seed). --full for the 200 games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_dice.h"

#include "games/mexico.h"

static mxgame T;                                /* the "screen's" game: the bot reads it */
static int NP, LIVES;
const mxgame *mexico_live(void) { return &T; }

static int one(const char *m)
{
    char f[BOT_MOVE_CAP];
    dice_fill(m, f, sizeof f, (uint64_t)T.n);
    return mx_script(&T, f);
}
int mexico_show(const char *move) { return one(move); }

static void reset(void) { mx_new(&T, NP, LIVES); }
static int actor(void) { return T.pos.turn; }
static int result(void) { return T.pos.phase != MX_OVER ? -1 : T.pos.winner & 1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    const mxpos *p = &T.pos;
    switch (p->phase) {
    case MX_REVEAL: snprintf(m, (size_t)cap, "n"); return 1;
    case MX_THROW:
        if (p->claim >= 0 && ai_rand_n(r, 3) == 0) snprintf(m, (size_t)cap, "c");
        else snprintf(m, (size_t)cap, "t%d%d", 1 + ai_rand_n(r, 6), 1 + ai_rand_n(r, 6));
        return 1;
    case MX_SAY: {
        int lo = p->claim < 0 ? 0 : p->claim;
        snprintf(m, (size_t)cap, "s%d", mx_number(lo + ai_rand_n(r, MX_VALUES - lo)));
        return 1;
    }
    }
    return 0;
}

int main(int argc, char **argv)
{
    int full = dice_full(argc, argv);
    const bot *b = bot_find("mexico");
    dice_check("registered: bot_find(\"mexico\")", b && b->think && b->play == mexico_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP], m2[BOT_MOVE_CAP];
    int ok = 1;
    NP = 3, LIVES = 3;
    reset();
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "t??");
    dice_check("a round's start: \"t??\" at every level", ok);
    dice_check("not its turn: no move", !b->think(1, AI_HARD, m, sizeof m));
    {   /* thrown 5-3 with 4-2 to beat: the truth, "s53" (medium, hard) */
        reset();
        mx_script(&T, "t42 s42 t53");
        ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(1, lv, m, sizeof m) && !strcmp(m, "s53");
        dice_check("a throw over the value: the truth (medium, hard)", ok);
    }
    {   /* thrown 3-1 with 6-5 to beat: a lie at 6-5 or a little more, at every level */
        reset();
        mx_script(&T, "t65 s65 t31");
        ok = 1;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            ok &= b->think(1, lv, m, sizeof m);
            int r = mx_from_number(atoi(m + 1));
            ok &= m[0] == 's' && r >= mx_from_number(65) && r <= mx_from_number(65) + 2;
        }
        dice_check("a throw under the value: a lie just over it, every level", ok);
    }
    {   /* Mexico announced over 6-6: medium and hard call */
        reset();
        mx_script(&T, "t66 s66 t21 s21");
        ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(2, lv, m, sizeof m) && !strcmp(m, "c");
        dice_check("Mexico over 6-6: called (medium, hard)", ok);
    }
    {   /* 3-2 announced first: believed (thrown again) by medium and hard */
        reset();
        mx_script(&T, "t32 s32");
        ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(1, lv, m, sizeof m) && !strcmp(m, "t??");
        dice_check("3-2 announced: believed (medium, hard)", ok);
    }
    {   /* the dice under the cup changed before its own throw: the same move (it never looks) */
        ok = 1;
        for (int k = 0; k < 60 && ok; k++) {
            NP = 2 + k % 5;
            reset();
            static const char *SEQ[] = {"t31 s54", "t66 s66", "t21 s21", "t42 s61 t11 s11"};
            mx_script(&T, SEQ[k % 4]);
            int me = T.pos.turn;
            for (int lv = AI_EASY; lv <= AI_HARD && ok; lv++) {
                ai_set_seed(5 + (uint64_t)k);
                ok &= b->think(me, lv, m, sizeof m);
                T.pos.dice[0] = (uint8_t)(1 + (T.pos.dice[0] + k) % 6);
                T.pos.dice[1] = (uint8_t)(1 + (T.pos.dice[1] + 2 * k + 1) % 6);
                ai_set_seed(5 + (uint64_t)k);
                ok &= b->think(me, lv, m2, sizeof m2) && !strcmp(m, m2);
            }
        }
        dice_check("the dice under the cup changed: the same call or throw (it never looks)", ok);
    }
    {   /* after a call: "n" */
        NP = 2;
        reset();
        mx_script(&T, "t42 s42 c");
        ok = T.pos.phase == MX_REVEAL && b->think(T.pos.turn, AI_EASY, m, sizeof m) && !strcmp(m, "n");
        dice_check("the cup lifted: \"n\"", ok);
    }
    if (dice_fails) return 1;

    int bad = 0;
    for (LIVES = 3; LIVES <= 6; LIVES += 3)
        for (NP = 2; NP <= MX_MAXP; NP++) {
            if (!full && NP > 3) continue;
            printf("== %d lives, %d players\n", LIVES, NP);
            const sp_game G = {
                .id = "mexico", .max_plies = 4000, .opening = 0, .reset = reset, .actor = actor, .result = result,
                .random_move = random_move, .script = one,
            };
            const int f = full;                 /* the short run (10 games) only checks the play */
            const sp_pair P[] = {
                {AI_EASY, SP_RANDOM, dice_games(200), f ? 70 : 0},
                {AI_MEDIUM, AI_EASY, dice_games(200), f ? 52 : 0},
                {AI_HARD, AI_EASY, dice_games(200), f ? 52 : 0},
                {AI_HARD, AI_MEDIUM, dice_games(200), f ? 50 : 0},
            };
            bad |= sp_run(&G, P, 4, argc, argv);
        }
    return bad;
}
