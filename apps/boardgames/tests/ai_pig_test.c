/*
 * Pig's bot (src/games/pig_bot.c), one die and two dice, two to six players: its registry entry, the rolls it makes
 * ("r?", "r??"), its holds (at the levels' totals, a hold that wins, the end race), the cancel; then self-play at
 * every level for both variants and two to six players (tests/ai_selfplay.h; three players or more: the even
 * players one level, the odd ones the other). Headless: this file plays the screen's part (pig_live(), pig_show():
 * the faces of a bot's roll from the session's seed). --full for the 200 games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_dice.h"

#include "games/pig.h"

static piggame T;                               /* the "screen's" game: the bot reads it */
static int V, NP;
const piggame *pig_live(void) { return &T; }

static int one(const char *m)
{
    char f[BOT_MOVE_CAP];
    dice_fill(m, f, sizeof f, (uint64_t)T.pos.turns * 64 + T.pos.rolls);
    return pig_script(&T, f);
}
int pig_show(const char *move) { return one(move); }

static void reset(void) { pig_new(&T, V, NP); }
static int actor(void) { return T.pos.turn; }
static int result(void) { return T.pos.phase != PIG_OVER ? -1 : T.pos.winner & 1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    if (pig_can_hold(&T.pos) && ai_rand_n(r, 3) == 0) snprintf(m, (size_t)cap, "h");
    else if (V == PIG_TWO) snprintf(m, (size_t)cap, "r%d%d", 1 + ai_rand_n(r, 6), 1 + ai_rand_n(r, 6));
    else snprintf(m, (size_t)cap, "r%d", 1 + ai_rand_n(r, 6));
    return 1;
}

/* player 0 to play with these scores and a turn's total so far (one roll made) */
static void at(int mine, int other, int total)
{
    NP = 2;
    reset();
    T.pos.score[0] = (int16_t)mine;
    T.pos.score[1] = (int16_t)other;
    T.pos.total = (int16_t)total;
    T.pos.rolls = 1;
}

int main(int argc, char **argv)
{
    int full = dice_full(argc, argv);
    const bot *b = bot_find("pig");
    dice_check("registered: bot_find(\"pig\")", b && b->think && b->play == pig_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP];
    int ok = 1;
    V = PIG_ONE, NP = 3;
    reset();
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "r?");
    V = PIG_TWO;
    reset();
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "r??");
    dice_check("a turn's first move: a roll (\"r?\", two dice \"r??\") at every level", ok);
    dice_check("not its turn: no move", !b->think(1, AI_HARD, m, sizeof m));
    V = PIG_ONE;
    at(40, 40, 21);
    ok = b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "h");
    at(40, 40, 20);
    ok &= b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "r?");
    dice_check("hard, level: holds at 21, not at 20", ok);
    at(20, 60, 21);                             /* 40 behind: holds at 26 */
    ok = b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "r?");
    at(20, 60, 26);
    ok &= b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "h");
    dice_check("hard, behind: risks more (holds at 26 with 40 to make up)", ok);
    at(30, 80, 30);
    ok = b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "r?");
    dice_check("hard, the leader at 80: rolls on to win", ok);
    ok = 1;
    for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
        at(95, 10, 5);
        ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "h");
    }
    at(10, 10, 20);
    ok &= b->think(0, AI_MEDIUM, m, sizeof m) && !strcmp(m, "h");
    dice_check("a hold that wins (medium, hard); medium holds at 20", ok);
    {   /* cancelled: no move */
        at(0, 0, 3);
        ai_cancel();
        ok = !b->think(0, AI_HARD, m, sizeof m);
        ai_cancel_clear();
        dice_check("cancelled: no move", ok);
    }
    if (dice_fails) return 1;

    int bad = 0;
    for (V = PIG_ONE; V <= PIG_TWO; V++)
        for (NP = 2; NP <= PIG_MAXP; NP++) {
            if (!full && NP > 3) continue;
            printf("== %s, %d players\n", V ? "two dice" : "one die", NP);
            const sp_game G = {
                .id = "pig", .max_plies = 6000, .opening = 0, .reset = reset, .actor = actor, .result = result,
                .random_move = random_move, .script = one,
            };
            const int f = full;                 /* the short run (10 games) only checks the play */
            const sp_pair P[] = {
                /* two dice: holding at random is no bad play */
                {AI_EASY, SP_RANDOM, dice_games(200), f ? (V == PIG_TWO ? 58 : 66) : 0},
                {AI_MEDIUM, AI_EASY, dice_games(200), f ? (V == PIG_TWO ? 48 : 52) : 0},
                {AI_HARD, AI_EASY, dice_games(200), f ? (V == PIG_TWO ? 48 : 52) : 0},
                {AI_HARD, AI_MEDIUM, dice_games(200), f ? 46 : 0},
            };
            bad |= sp_run(&G, P, 4, argc, argv);
        }
    return bad;
}
