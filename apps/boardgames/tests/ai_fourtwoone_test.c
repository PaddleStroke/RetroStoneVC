/*
 * 421's bot (src/games/fourtwoone_bot.c), two to six players: its registry entry, the throws it makes ("d???", then
 * "d" with "-" for the dice kept), a few choices of the discharge (a 4-2-1 kept, stopping on it as the leader), the
 * cancel; then self-play at every level for two to six players (tests/ai_selfplay.h; the one left with the chips
 * loses, so the other side wins; three players or more: the even players one level, the odd ones the other).
 * Headless: this file plays the screen's part (fourtwoone_live(), fourtwoone_show(): the faces of a bot's throw from
 * the session's seed). --full for the 200 games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_dice.h"

#include "games/fourtwoone.h"

static tgame T;                                 /* the "screen's" game: the bot reads it */
static int NP;
const tgame *fourtwoone_live(void) { return &T; }

static int one(const char *m)
{
    char f[BOT_MOVE_CAP];
    dice_fill(m, f, sizeof f, (uint64_t)T.n);
    return fto_script(&T, f);
}
int fourtwoone_show(const char *move) { return one(move); }

static void reset(void) { fto_new(&T, NP); }
static int actor(void) { return T.pos.turn; }
static int result(void) { return T.pos.phase != FTO_OVER ? -1 : (T.pos.loser & 1) ^ 1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    const tpos *p = &T.pos;
    if (cap < 6) return 0;
    if (fto_can_stop(p) && !ai_rand_n(r, 3)) {
        strcpy(m, "s");
        return 1;
    }
    unsigned kept = p->throws ? (unsigned)ai_rand_n(r, 7) : 0;
    m[0] = 'd';
    for (int i = 0; i < FTO_DICE; i++) m[1 + i] = kept >> i & 1 ? '-' : (char)('1' + ai_rand_n(r, 6));
    m[1 + FTO_DICE] = 0;
    return 1;
}

/* the discharge, player 0 to lead: chips 5 / 6 (two players) */
static void discharge(void)
{
    NP = 2;
    reset();
    tpos *p = &T.pos;
    p->phase = FTO_DISCHARGE;
    p->pot = 0;
    p->chips[0] = 5;
    p->chips[1] = 6;
    p->turn = p->leader = 0;
    p->limit = FTO_THROWS;
}

int main(int argc, char **argv)
{
    int full = dice_full(argc, argv);
    const bot *b = bot_find("fourtwoone");
    dice_check("registered: bot_find(\"fourtwoone\")", b && b->think && b->play == fourtwoone_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP];
    int ok = 1;
    NP = 3;
    reset();
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "d???");
    dice_check("the charge: \"d???\" at every level", ok);
    dice_check("not its turn: no move", !b->think(1, AI_HARD, m, sizeof m));
    {   /* the discharge's leader throws 4-2-1 at once: every level stops (the others get one throw) */
        discharge();
        fto_script(&T, "d421");
        ok = 1;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "s");
        dice_check("4-2-1 first: stops, every level", ok);
    }
    {   /* 4 2 6: medium and hard keep the 4 and the 2 */
        discharge();
        fto_script(&T, "d426");
        ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
            ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "d--?");
            printf("     (4 2 6: %s plays %s)\n", sp_name(lv), m);
        }
        dice_check("4 2 6: keeps 4 and 2 (medium, hard)", ok);
    }
    {   /* player 1 after a leader who stopped on 2-2-1 (the nenette, the worst throw) in one throw: anything but
         * 2-2-1 wins the round, so player 1 has one throw and nothing to choose */
        discharge();
        fto_script(&T, "d221 s");
        ok = T.pos.turn == 1 && T.pos.limit == 1 && b->think(1, AI_HARD, m, sizeof m) && !strcmp(m, "d???");
        dice_check("after a leader's one throw: one throw", ok);
    }
    {   /* cancelled: no move, at once */
        discharge();
        fto_script(&T, "d135");
        ai_cancel();
        double t0 = ai_now_ms();
        ok = !b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        dice_check("cancelled: no move, at once", ok && dt < 50);
    }
    if (dice_fails) return 1;

    int bad = 0;
    for (NP = 2; NP <= FTO_PLAYERS; NP++) {
        if (!full && NP > 3) continue;
        printf("== %d players\n", NP);
        const sp_game G = {
            .id = "fourtwoone", .max_plies = 6000, .opening = 0, .reset = reset, .actor = actor, .result = result,
            .random_move = random_move, .script = one,
        };
        const int f = full;                     /* the short run (10 games) only checks the play */
        const sp_pair P[] = {
            {AI_EASY, SP_RANDOM, dice_games(200), f ? 55 : 0},
            {AI_MEDIUM, AI_EASY, dice_games(200), f ? 47 : 0},
            {AI_HARD, AI_EASY, dice_games(200), f ? 50 : 0},
            {AI_HARD, AI_MEDIUM, dice_games(200), f ? 47 : 0},
        };
        bad |= sp_run(&G, P, 4, argc, argv);
    }
    return bad;
}
