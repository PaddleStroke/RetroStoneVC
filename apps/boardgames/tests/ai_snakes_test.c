/*
 * Snakes and ladders' bot (src/games/snakes_bot.c): no decision in the game, so the bot only throws ("roll") when its
 * turn comes, and never otherwise (not its turn, the game over). A very long game, past the rules' history. Then
 * self-play for two to four players: every game ends, every move is the rules' (tests/ai_selfplay.h; the levels play
 * alike, so nobody is expected to win more). Headless: this file plays the screen's part (snakes_live(),
 * snakes_show(): "roll" throws a die of its own, from the session's seed the harness sets before each think, so every
 * game repeats). --full for the 200 games.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/snakes.h"

static sn_game T;                               /* the "screen's" game: the bot reads it */
static int NP;
const sn_game *snakes_live(void) { return &T; }

static int one(const char *m)
{
    if (!strcmp(m, "roll")) {                   /* a die from the session's seed and the throws so far */
        ai_rng r;
        ai_seed(&r, ai_mix(ai_session_seed(), (uint64_t)T.n));
        return sn_play(&T, 1 + ai_rand_n(&r, 6));
    }
    char *end;
    long v = strtol(m, &end, 10);
    return end != m && !*end && sn_play(&T, (int)v);
}
int snakes_show(const char *move) { return one(move); }

static void reset(void) { sn_new_players(&T, NP); }
static int actor(void) { return sn_turn(&T); }
static int result(void) { return T.winner < 0 ? -1 : T.winner & 1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    snprintf(m, (size_t)cap, "%d", 1 + ai_rand_n(r, 6));
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
    const bot *b = bot_find("snakes");
    check("registered: bot_find(\"snakes\")", b && b->think && b->play == snakes_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP];
    NP = 3;
    reset();
    int ok = 1;
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "roll");
    check("its turn: \"roll\" at every level", ok);
    check("not its turn: no move", !b->think(1, AI_EASY, m, sizeof m) && !b->think(2, AI_HARD, m, sizeof m));
    T.winner = 0;
    check("the game over: no move", !b->think(0, AI_HARD, m, sizeof m));
    {   /* a very long game: 3 x SN_MAX_THROWS throws that never reach 100, then the bots' own throws to the end:
         * the rules keep going (the first throws go into a base position), and undo still takes a throw back */
        NP = 4;
        reset();
        ai_rng r;
        ai_seed(&r, 11);
        int ok = 1, undone = 0;
        for (int i = 0; i < 3 * SN_MAX_THROWS && ok; i++) {
            int at = T.pos[T.turn], v;
            for (;;) {                          /* a throw that does not win */
                v = 1 + ai_rand_n(&r, 6);
                int land = sn_hop(at, v), l = sn_link_at(land);
                if ((l < 0 ? land : sn_links[l].to) != SN_LAST) break;
            }
            if (i % 997 == 0) {                 /* undo, then the same throw again */
                int pos[SN_PLAYERS], turn = T.turn;
                memcpy(pos, T.pos, sizeof pos);
                ok = sn_play(&T, v) && sn_undo(&T) && !memcmp(pos, T.pos, sizeof pos) && T.turn == turn &&
                     sn_next(&T) == v;
                undone += ok;
            }
            ok = ok && sn_play(&T, v) && T.n <= SN_MAX_THROWS;
        }
        int ply = 0;
        for (; ok && T.winner < 0 && ply < 5000; ply++) {
            ai_set_seed(ai_mix(77, (uint64_t)ply));
            ok = b->think(T.turn, AI_EASY, m, sizeof m) && one(m);
        }
        char name[160];
        snprintf(name, sizeof name, "a game of %d throws and %d more: it ends, undo works (%d undone)",
                 3 * SN_MAX_THROWS, ply, undone);
        check(name, ok && T.winner >= 0 && undone > 10);
    }

    if (fails) return 1;

    int bad = 0;
    for (NP = 2; NP <= SN_PLAYERS; NP++) {
        printf("== %d players\n", NP);
        const sp_game G = {
            .id = "snakes", .max_plies = 3000, .opening = 0, .reset = reset, .actor = actor, .result = result,
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
