/*
 * Bataille corse's bot (src/games/slap_bot.c): its registry entry, a double slapped by hard (and the pile won), never
 * a wrong slap, its turn played otherwise, the reaction time (real: about half a second for hard), games of two to
 * four bots to the end, then self-play (tests/ai_selfplay.h: the bots act on their turn, so a level's strength here is
 * the doubles it sees; the race against a human's hand is the reaction time). Headless: this file plays the screen's
 * part (slap_live(), slap_show(), slap_realtime(): no waiting but in the timing check). --full for 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "ai_selfplay.h"

#include "games/slap.h"

static slgame T;                                /* the "screen's" game: the bot reads it */
static int variant, realtime;
static uint32_t deal = 1;
static int pending;                             /* a new game, dealt at its first move from the game's seed */
static void deal_now(uint64_t seed);
const slgame *slap_live(void) { deal_now(ai_session_seed()); return &T; }
int slap_show(const char *move) { return slap_script(&T, move); }
int slap_realtime(void) { return realtime; }

static int one(const char *m) { return slap_script(&T, m); }
static void deal_now(uint64_t seed)
{
    if (pending) slap_new(&T, variant, 2, (uint32_t)seed);
    pending = 0;
}
static void reset(void) { pending = 1; }          /* (the harness sets the game's seed after it) */
static int actor(void) { return pending ? 0 : T.pos.turn; }
static int result(void) { return pending ? -1 : T.pos.phase == SLP_OVER ? T.pos.winner : -1; }

/* a random player: on its turn it turns a card (or gathers), but slaps at random one time in four (often wrongly) */
static int random_move(ai_rng *r, char *m, int cap)
{
    deal_now(ai_rand(r));
    const slpos *p = &T.pos;
    if (ai_rand_n(r, 4) == 0 && p->nc) snprintf(m, (size_t)cap, "s%d", p->turn);
    else snprintf(m, (size_t)cap, p->phase == SLP_CLAIM ? "c" : "p");
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* a game played until a double shows on a turn: 1 found */
static int to_double(uint32_t seed)
{
    slap_new(&T, SLAP_DOUBLE, 2, seed);
    for (int k = 0; k < 5000 && T.pos.phase != SLP_OVER; k++) {
        if (slap_right(&T.pos)) return 1;
        one(T.pos.phase == SLP_CLAIM ? "c" : "p");
    }
    return 0;
}

int main(int argc, char **argv)
{
    char name[160];
    const bot *b = bot_find("slap");
    check("registered: bot_find(\"slap\")", b && b->think && b->play == slap_show);
    {   /* a double on its turn: hard always slaps it (and wins the pile); easy and medium now and then miss it */
        int hard = 1, tried = 0, missed[AI_LEVELS] = {0};
        for (uint32_t s = 1; tried < 200 && s < 5000; s++) {
            if (!to_double(s)) continue;
            tried++;
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(s * 3 + (uint64_t)lv);
                b->think(T.pos.turn, lv, m, sizeof m);
                missed[lv] += m[0] != 's';
                if (lv == AI_HARD) {
                    int me = T.pos.turn;
                    hard &= m[0] == 's' && one(m) && T.pos.nc == 0 && T.pos.turn == me;
                }
            }
        }
        snprintf(name, sizeof name, "a double on its turn: hard slaps all %d (the pile won); easy misses %d, medium %d",
                 tried, missed[AI_EASY], missed[AI_MEDIUM]);
        check(name, hard && tried == 200 && missed[AI_EASY] > missed[AI_MEDIUM] && missed[AI_MEDIUM] > 0);
    }
    {   /* never a wrong slap; its turn played otherwise */
        int ok = 1;
        for (uint32_t s = 1; s < 40; s++) {
            slap_new(&T, s & 1 ? SLAP_SANDWICH : SLAP_DOUBLE, 2 + s % 3, s);
            for (int k = 0; k < 3000 && T.pos.phase != SLP_OVER && ok; k++) {
                char m[BOT_MOVE_CAP] = "";
                int right = slap_right(&T.pos), phase = T.pos.phase, seen = T.n;
                ai_set_seed(ai_mix(s, (uint64_t)k));
                ok &= b->think(T.pos.turn, k % 3, m, sizeof m) == 1;
                ok &= m[0] == 's' ? right : m[0] == (phase == SLP_CLAIM ? 'c' : 'p');
                ok &= strchr(m, '/') && atoi(strchr(m, '/') + 1) == seen;   /* the actions it saw */
                ok &= one(m);
            }
        }
        check("never a wrong slap; otherwise its card turned, or the pile gathered; the actions seen in the move", ok);
    }
    {   /* the reaction time, waited for real: hard about half a second, easy about a second or more */
        double ms[AI_LEVELS] = {0};
        int n[AI_LEVELS] = {0};
        realtime = 1;
        for (uint32_t s = 1; s < 400; s++) {
            if (!to_double(s)) continue;
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                if (n[lv] >= 3) continue;
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(s * 7 + (uint64_t)lv);
                double t0 = ai_now_ms();
                b->think(T.pos.turn, lv, m, sizeof m);
                if (m[0] != 's') continue;
                ms[lv] += ai_now_ms() - t0;
                n[lv]++;
            }
            if (n[0] >= 3 && n[1] >= 3 && n[2] >= 3) break;
        }
        realtime = 0;
        for (int lv = 0; lv < AI_LEVELS; lv++) ms[lv] /= n[lv] ? n[lv] : 1;
        snprintf(name, sizeof name, "reaction: easy %.0f ms, medium %.0f ms, hard %.0f ms", ms[0], ms[1], ms[2]);
        check(name, ms[0] >= 900 && ms[0] <= 1550 && ms[1] >= 600 && ms[1] <= 1000 && ms[2] >= 380 && ms[2] <= 600);
        /* and cancelled while it waits: no move, at once */
        realtime = 1;
        to_double(3);
        while (!slap_right(&T.pos)) to_double(deal++);
        ai_cancel();
        char m[BOT_MOVE_CAP] = "";
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_EASY, m, sizeof m);
        ai_cancel_clear();
        realtime = 0;
        check("cancelled while it waits: no move, at once", !ok && ai_now_ms() - t0 < 100);
    }
    {   /* two to four bots of every level, both variants, to the end */
        int ok = 1, games = 0;
        for (uint32_t s = 0; s < 60; s++) {
            slap_new(&T, s & 1 ? SLAP_SANDWICH : SLAP_DOUBLE, 2 + s % 3, 3000 + s);
            for (int k = 0; k < 20000 && T.pos.phase != SLP_OVER; k++) {
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(ai_mix(s, (uint64_t)k));
                if (!b->think(T.pos.turn, (T.pos.turn + (int)s) % 3, m, sizeof m) || !one(m)) { ok = 0; break; }
            }
            ok &= T.pos.phase == SLP_OVER;
            games++;
        }
        snprintf(name, sizeof name, "%d games of 2 to 4 bots end", games);
        check(name, ok);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "slap", .max_plies = 20000, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 75},
        {AI_MEDIUM, AI_EASY, 200, 50},
        {AI_HARD, AI_MEDIUM, 200, 50},
        {AI_HARD, AI_EASY, 200, 55},
    };
    /* the short run (a twentieth of the games: ten a pairing) checks the games; with so few, luck rules: its
     * targets are 30 points looser */
    int quick = 1;
    for (int i = 1; i < argc; i++) quick &= strcmp(argv[i], "--full") != 0;
    for (size_t i = 0; quick && i < sizeof P / sizeof P[0]; i++)
        P[i].min_pct = P[i].min_pct > 30 ? P[i].min_pct - 30 : 0;
    int bad = 0;
    for (variant = 0; variant < SLAP_VARIANTS; variant++) {
        printf("-- %s\n", variant ? "doubles and sandwiches" : "doubles");
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
