/*
 * Liar's Dice's bot (src/games/liarsdice_bot.c), two to six players: its registry entry, an opening bid, a call of a
 * bid its own dice show impossible, no call of a bid its own dice make true, "n" after a call, the bot's view (the
 * others' dice changed: the same move), the cancel; then self-play at every level for two to six players
 * (tests/ai_selfplay.h; three players or more: the even players one level, the odd ones the other). Headless: this
 * file plays the screen's part (liarsdice_live(), liarsdice_show()); each game's dice come from its first move's seed
 * (the harness gives reset() none). --full for the 200 games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_dice.h"

#include "games/liarsdice.h"

static lgame T;                                 /* the "screen's" game: the bot reads it */
static int NP, pending;

static void deal_now(uint64_t seed)
{
    if (pending) ld_new(&T, NP, (uint32_t)(seed ^ seed >> 32));
    pending = 0;
}
const lgame *liarsdice_live(void)
{
    deal_now(ai_session_seed());                /* a game's first move a bot's: its dice from that seed */
    return &T;
}
static int one(const char *m) { return ld_script(&T, m); }
int liarsdice_show(const char *move) { return one(move); }

static void reset(void) { pending = 1; }
static int actor(void) { return pending ? 0 : T.pos.turn; }
static int result(void) { return pending || T.pos.phase != LP_OVER ? -1 : T.pos.winner & 1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    deal_now(ai_rand(r) | (uint64_t)ai_rand(r) << 32);
    const lpos *p = &T.pos;
    if (p->phase == LP_REVEAL) {
        snprintf(m, (size_t)cap, "n");
        return 1;
    }
    if (p->count && ai_rand_n(r, 3) == 0) {
        snprintf(m, (size_t)cap, "l");
        return 1;
    }
    for (int tries = 0; tries < 50; tries++) {
        int c = (p->count ? p->count : 1) + ai_rand_n(r, 3), f = 2 + ai_rand_n(r, 5);
        if (ld_raises(p, c, f)) {
            snprintf(m, (size_t)cap, "%dx%d", c, f);
            return 1;
        }
    }
    snprintf(m, (size_t)cap, "l");
    return p->count > 0;
}

static void fresh(uint32_t seed)
{
    ld_new(&T, NP, seed);
    pending = 0;
}

int main(int argc, char **argv)
{
    int full = dice_full(argc, argv);
    const bot *b = bot_find("liarsdice");
    dice_check("registered: bot_find(\"liarsdice\")", b && b->think && b->play == liarsdice_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP], m2[BOT_MOVE_CAP];
    int ok = 1;
    NP = 3;
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
        fresh(11);
        ok &= b->think(0, lv, m, sizeof m) && ld_script(&T, m) && T.n == 1;
    }
    dice_check("an opening bid at every level (a bid the rules take)", ok);
    dice_check("not its turn: no move", !b->think(0, AI_HARD, m, sizeof m));
    {   /* a bid of more dice than its own and all the others' could make: called */
        fresh(12);
        ld_script(&T, "15x6");
        ok = 1;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(1, lv, m, sizeof m) && !strcmp(m, "l");
        dice_check("15 sixes among 15 dice it can't all see backing: called, every level", ok);
    }
    {   /* the opening "1x3": as good as certain among 15 dice: raised, never called */
        fresh(13);
        ld_script(&T, "1x3");
        ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(1, lv, m, sizeof m) && strcmp(m, "l");
        dice_check("a low bid: raised, not called (medium, hard)", ok);
    }
    {   /* the others' dice don't change its move: it sees only its own */
        ok = 1;
        for (int k = 0; k < 40 && ok; k++) {
            NP = 2 + k % 5;
            fresh(100 + (uint32_t)k);
            ld_script(&T, k % 2 ? "2x5" : "");
            int me = T.pos.turn;
            for (int lv = AI_EASY; lv <= AI_HARD && ok; lv++) {
                ai_set_seed(77 + (uint64_t)k);
                ok &= b->think(me, lv, m, sizeof m);
                for (int q = 0; q < NP; q++)
                    if (q != me)
                        for (int i = 0; i < LD_DICE; i++) T.pos.dice[q][i] = (uint8_t)(1 + (T.pos.dice[q][i] + 2) % 6);
                ai_set_seed(77 + (uint64_t)k);
                ok &= b->think(me, lv, m2, sizeof m2) && !strcmp(m, m2);
            }
        }
        dice_check("the others' dice changed: the same move (it never looks)", ok);
    }
    {   /* after a call: "n" from the caller */
        NP = 2;
        fresh(14);
        ld_script(&T, "1x2 l");
        ok = T.pos.phase == LP_REVEAL && b->think(T.pos.turn, AI_HARD, m, sizeof m) && !strcmp(m, "n");
        dice_check("the cups lifted: \"n\"", ok);
    }
    {   /* cancelled: no move */
        NP = 4;
        fresh(15);
        ai_cancel();
        ok = !b->think(0, AI_HARD, m, sizeof m);
        ai_cancel_clear();
        dice_check("cancelled: no move", ok);
    }
    if (dice_fails) return 1;

    int bad = 0;
    for (NP = 2; NP <= LD_MAXP; NP++) {
        if (!full && NP > 3) continue;
        printf("== %d players\n", NP);
        const sp_game G = {
            .id = "liarsdice", .max_plies = 3000, .opening = 0, .reset = reset, .actor = actor, .result = result,
            .random_move = random_move, .script = one,
        };
        const int f = full;                     /* the short run (10 games) only checks the play */
        const sp_pair P[] = {
            {AI_EASY, SP_RANDOM, dice_games(200), f ? 75 : 0},
            {AI_MEDIUM, AI_EASY, dice_games(200), f ? 50 : 0},
            {AI_HARD, AI_EASY, dice_games(200), f ? 55 : 0},
            {AI_HARD, AI_MEDIUM, dice_games(200), f ? 50 : 0},
        };
        bad |= sp_run(&G, P, 4, argc, argv);
    }
    return bad;
}
