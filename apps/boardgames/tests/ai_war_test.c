/*
 * War's bot (src/games/war_bot.c): its registry entry, its card turned only when it may, games of two to four bots to
 * the end, then self-play (tests/ai_selfplay.h; war has no decisions: every level and a random player alike, so the
 * pairings only check that the games are legal and end). Headless: this file plays the screen's part (war_live(),
 * war_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/war.h"

static wgame T;                                 /* the "screen's" game: the bot reads it */
static int variant = WAR_SHORT;
static int pending;                             /* a new game, dealt at its first move from the game's seed */
static void deal_now(uint64_t seed);
const wgame *war_live(void) { deal_now(ai_session_seed()); return &T; }
int war_show(const char *move) { return war_script(&T, move); }

static int one(const char *m) { return war_script(&T, m); }
static void deal_now(uint64_t seed)
{
    if (pending) war_new(&T, variant, 2, (uint32_t)seed);
    pending = 0;
}
static void reset(void) { pending = 1; }          /* (the harness sets the game's seed after it) */
static int actor(void) { return pending ? 0 : war_turn(&T.pos); }
static int result(void)
{
    if (pending) return -1;
    unsigned w = war_winners(&T.pos);
    return !w ? -1 : w == 3 ? 2 : w == 1 ? 0 : 1;
}
static int random_move(ai_rng *r, char *m, int cap)
{
    deal_now(ai_rand(r));
    snprintf(m, (size_t)cap, "%d", war_turn(&T.pos));
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
    const bot *b = bot_find("war");
    check("registered: bot_find(\"war\")", b && b->think && b->play == war_show);
    {   /* its card only when it may turn one */
        char m[BOT_MOVE_CAP] = "";
        war_new(&T, WAR_FULL, 3, 5);
        int ok = b->think(0, AI_EASY, m, sizeof m) && !strcmp(m, "0") && one(m);
        ok &= !b->think(0, AI_HARD, m, sizeof m);   /* already turned this step */
        ok &= b->think(2, AI_HARD, m, sizeof m) && !strcmp(m, "2");
        check("it turns its card when it may, and only then", ok);
    }
    {   /* two to four bots, both variants, to the end (the full game: within the rules' 30000 taps) */
        int ok = 1, longest = 0, capped = 0, games = 0;
        for (int k = 0; k < 60; k++) {
            int np = 2 + k % 3, v = k / 3 % 2 ? WAR_SHORT : WAR_FULL;
            war_new(&T, v, np, 100 + (uint32_t)k);
            int taps = 0;
            while (war_turn(&T.pos) >= 0 && T.n < WAR_MAX_ACTS) {
                char m[BOT_MOVE_CAP] = "";
                int s = war_turn(&T.pos);
                if (!b->think(s, k % 3, m, sizeof m) || !one(m)) { ok = 0; break; }
                taps++;
            }
            capped += T.n >= WAR_MAX_ACTS;
            ok &= T.pos.phase == WP_OVER || T.n >= WAR_MAX_ACTS;
            if (taps > longest) longest = taps;
            games++;
        }
        snprintf(name, sizeof name, "%d games of 2 to 4 bots end (longest %d taps; %d at the rules' cap of %d taps)",
                 games, longest, capped, WAR_MAX_ACTS);
        check(name, ok);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "war", .max_plies = 4 * WAR_SHORT_BATTLES * 30, .opening = 0, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {{AI_EASY, SP_RANDOM, 100, 0}, {AI_HARD, AI_EASY, 100, 0}};
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
