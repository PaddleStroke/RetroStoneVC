/*
 * Ship, Captain and Crew's bot (src/games/shipcaptain_bot.c), two to six players: its registry entry, the rolls it
 * makes ("r?????", fewer "?" as the ship, captain and crew come in), its stops (a full cargo kept, a poor one rolled
 * again, the last player beating the best cargo), the cancel; then self-play at every level for two to six players
 * (tests/ai_selfplay.h; three players or more: the even players one level, the odd ones the other). Headless: this
 * file plays the screen's part (shipcaptain_live(), shipcaptain_show(): the faces of a bot's roll from the session's
 * seed). --full for the 200 games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_dice.h"

#include "games/shipcaptain.h"

static sccgame T;                               /* the "screen's" game: the bot reads it */
static int NP;
const sccgame *shipcaptain_live(void) { return &T; }

static int one(const char *m)
{
    char f[BOT_MOVE_CAP];
    dice_fill(m, f, sizeof f, ((uint64_t)T.pos.rounds * 8 + T.pos.turn) * 4 + T.pos.rolls);
    return scc_script(&T, f);
}
int shipcaptain_show(const char *move) { return one(move); }

static void reset(void) { scc_new(&T, NP); }
static int actor(void) { return T.pos.turn; }
static int result(void) { return T.pos.phase != SCC_OVER ? -1 : dice_side(T.pos.winners); }

static int random_move(ai_rng *r, char *m, int cap)
{
    if (scc_can_stop(&T.pos) && ai_rand_n(r, 2)) {
        snprintf(m, (size_t)cap, "s");
        return 1;
    }
    int n = __builtin_popcount(scc_free(&T.pos)), len = 0;
    m[len++] = 'r';
    while (n--) m[len++] = (char)('1' + ai_rand_n(r, 6));
    m[len] = 0;
    return 1;
}

int main(int argc, char **argv)
{
    int full = dice_full(argc, argv);
    const bot *b = bot_find("shipcaptain");
    dice_check("registered: bot_find(\"shipcaptain\")", b && b->think && b->play == shipcaptain_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP];
    int ok = 1;
    NP = 2;
    reset();
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "r?????");
    scc_script(&T, "r61234");                   /* the ship in: four dice to roll */
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "r????");
    dice_check("rolls: \"r?????\", then one \"?\" less with the ship in, every level", ok);
    dice_check("not its turn: no move", !b->think(1, AI_HARD, m, sizeof m));
    reset();
    scc_script(&T, "r65466");                   /* the crew in, a cargo of 12 at once */
    ok = 1;
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "s");
    dice_check("a cargo of 12: stops, every level", ok);
    reset();
    scc_script(&T, "r65411");                   /* a cargo of 2: rolled again */
    ok = 1;
    for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "r??");
    dice_check("a cargo of 2: rolls the cargo again (medium, hard)", ok);
    reset();
    scc_script(&T, "r65455 s r65441");          /* player 0 stopped on 10; player 1 has 5: rolls again */
    ok = 1;
    for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(1, lv, m, sizeof m) && !strcmp(m, "r??");
    reset();
    scc_script(&T, "r65455 s r65465");          /* player 1 has 11 over 10: stops */
    for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(1, lv, m, sizeof m) && !strcmp(m, "s");
    dice_check("the last player: under the best cargo rolls on, over it stops (medium, hard)", ok);
    {   /* cancelled: no move */
        reset();
        ai_cancel();
        ok = !b->think(0, AI_HARD, m, sizeof m);
        ai_cancel_clear();
        dice_check("cancelled: no move", ok);
    }
    if (dice_fails) return 1;

    int bad = 0;
    for (NP = 2; NP <= SCC_MAXP; NP++) {
        if (!full && NP > 3) continue;
        printf("== %d players\n", NP);
        const sp_game G = {
            .id = "shipcaptain", .max_plies = 6000, .opening = 0, .reset = reset, .actor = actor, .result = result,
            .random_move = random_move, .script = one,
        };
        /* the short run (10 games) only checks the play. The only choice is to stop on a cargo or roll it again, and
         * the levels mostly choose alike: the luck decides (1000 games a pairing: hard-medium 49-50%, hard-easy
         * 50-55%), so the floors only catch a broken player */
        const int f = full;
        const sp_pair P[] = {
            {AI_EASY, SP_RANDOM, dice_games(200), f ? 52 : 0},
            {AI_MEDIUM, AI_EASY, dice_games(200), f ? 43 : 0},
            {AI_HARD, AI_EASY, dice_games(200), f ? 43 : 0},
            {AI_HARD, AI_MEDIUM, dice_games(200), f ? 43 : 0},
        };
        bad |= sp_run(&G, P, 4, argc, argv);
    }
    return bad;
}
