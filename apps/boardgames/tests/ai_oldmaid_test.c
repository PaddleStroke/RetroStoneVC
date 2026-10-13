/*
 * Old Maid's bot (src/games/oldmaid_bot.c): its registry entry, a draw that never depends on the cards offered (they
 * are face down and shuffled), games of two to six bots to the end, then self-play (tests/ai_selfplay.h; the draws
 * are blind: every level and a random player alike, so the pairings only check that the games are legal and end).
 * Headless: this file plays the screen's part (oldmaid_live(), oldmaid_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/oldmaid.h"

static ogame T;                                 /* the "screen's" game: the bot reads it */
static int pending;                             /* a new game, dealt at its first move from the game's seed */
static void deal_now(uint64_t seed);
const ogame *oldmaid_live(void) { deal_now(ai_session_seed()); return &T; }
int oldmaid_show(const char *move) { return om_script(&T, move); }

static int one(const char *m) { return om_script(&T, m); }
static void deal_now(uint64_t seed)
{
    if (pending) om_new(&T, 2, (uint32_t)seed);
    pending = 0;
}
static void reset(void) { pending = 1; }          /* (the harness sets the game's seed after it) */
static int actor(void) { return pending ? 0 : T.pos.turn; }
static int result(void)
{
    if (pending) return -1;
    if (T.pos.phase != OP_OVER) return -1;
    return T.pos.loser == OM_NONE ? 2 : T.pos.loser == 0 ? 1 : 0;
}
static int random_move(ai_rng *r, char *m, int cap)
{
    deal_now(ai_rand(r));
    int o = om_offerer(&T.pos);
    if (o < 0) return 0;
    snprintf(m, (size_t)cap, "d%d", ai_rand_n(r, T.pos.n[o]));
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
    const bot *b = bot_find("oldmaid");
    check("registered: bot_find(\"oldmaid\")", b && b->think && b->play == oldmaid_show);
    {   /* the cards offered never matter: the same draw with the offered hand in another order */
        int same = 1;
        for (int k = 0; k < 40; k++) {
            om_new(&T, 2 + k % 5, 300 + (uint32_t)k);
            if (T.pos.phase != OP_DRAW) continue;
            int o = om_offerer(&T.pos);
            char m1[BOT_MOVE_CAP] = "", m2[BOT_MOVE_CAP] = "";
            ai_set_seed(8);
            b->think(T.pos.turn, AI_HARD, m1, sizeof m1);
            for (int i = 0; i + 1 < T.pos.n[o]; i++) {
                uint8_t t = T.pos.hand[o][i];
                T.pos.hand[o][i] = T.pos.hand[o][i + 1];
                T.pos.hand[o][i + 1] = t;
            }
            ai_set_seed(8);
            b->think(T.pos.turn, AI_HARD, m2, sizeof m2);
            same &= !strcmp(m1, m2) && m1[0] == 'd' && !b->think((T.pos.turn + 1) % T.pos.players, 0, m2, sizeof m2);
        }
        check("the draw never depends on the cards offered; not its turn: no move", same);
    }
    {   /* two to six bots to the end */
        int ok = 1, games = 0, losers[OM_MAXP + 1] = {0};
        for (int k = 0; k < 100; k++) {
            int np = 2 + k % 5;
            om_new(&T, np, 700 + (uint32_t)k);
            while (T.pos.phase == OP_DRAW && T.n < OM_MAX_ACTS) {
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(ai_mix((uint64_t)k, (uint64_t)T.n));
                if (!b->think(T.pos.turn, k % 3, m, sizeof m) || !one(m)) { ok = 0; break; }
            }
            ok &= T.pos.phase == OP_OVER;
            losers[T.pos.loser == OM_NONE ? OM_MAXP : T.pos.loser]++;
            games++;
        }
        snprintf(name, sizeof name, "%d games of 2 to 6 bots end, one loser each", games);
        check(name, ok && losers[OM_MAXP] == 0);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "oldmaid", .max_plies = OM_MAX_ACTS, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {{AI_EASY, SP_RANDOM, 100, 0}, {AI_HARD, AI_EASY, 100, 0}};
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
