/*
 * The Game of the Goose's bot (src/games/goose_bot.c): no decision in the game, so the bot only throws ("roll") when
 * its turn comes (the rules' turn: the inn, the well and the prison), and never otherwise. Then self-play for two to
 * six players: every game ends, every move is the rules' (tests/ai_selfplay.h; the levels play alike). Headless: this
 * file plays the screen's part (goose_live(), goose_show(): "roll" throws dice of its own, from the session's seed
 * the harness sets before each think, so every game repeats). --full for the 200 games.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/goose.h"

static goo_game T;                              /* the "screen's" game: the bot reads it */
static int NP;
const goo_game *goose_live(void) { return &T; }

static int one(const char *m)
{
    if (!strcmp(m, "roll")) {                   /* dice from the session's seed and the throws so far */
        ai_rng r;
        ai_seed(&r, ai_mix(ai_session_seed(), (uint64_t)T.n));
        int a = 1 + ai_rand_n(&r, 6);
        return goo_play(&T, a, 1 + ai_rand_n(&r, 6));
    }
    return goo_script(&T, m);
}
int goose_show(const char *move) { return one(move); }

static void reset(void) { goo_new(&T, NP); }
static int actor(void) { return goo_turn(&T); }
static int result(void) { return T.winner < 0 ? -1 : T.winner & 1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    snprintf(m, (size_t)cap, "%d%d", 1 + ai_rand_n(r, 6), 1 + ai_rand_n(r, 6));
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
    const bot *b = bot_find("goose");
    check("registered: bot_find(\"goose\")", b && b->think && b->play == goose_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP];
    NP = 4;
    reset();
    int ok = 1;
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "roll");
    check("its turn: \"roll\" at every level", ok);
    check("not its turn: no move", !b->think(1, AI_EASY, m, sizeof m) && !b->think(3, AI_HARD, m, sizeof m));
    goo_script(&T, "45");                       /* player 0: 9 from the start, to 53; player 1 next */
    check("the turn goes on: the next player's", goo_turn(&T) == 1 && b->think(1, AI_EASY, m, sizeof m));
    T.winner = 1;
    check("the game over: no move", !b->think(1, AI_HARD, m, sizeof m));
    if (fails) return 1;

    int bad = 0;
    for (NP = 2; NP <= GOO_PLAYERS; NP++) {
        printf("== %d players\n", NP);
        const sp_game G = {
            .id = "goose", .max_plies = 6000, .opening = 0, .reset = reset, .actor = actor, .result = result,
            .random_move = random_move, .script = one,
        };
        const sp_pair P[] = {
            {AI_EASY, SP_RANDOM, 200, 0},
            {AI_HARD, AI_EASY, 200, 0},
        };
        bad |= sp_run(&G, P, 2, argc, argv);
    }
    return bad;
}
