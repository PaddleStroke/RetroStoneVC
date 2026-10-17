/*
 * Happy Families' bot (src/games/families_bot.c): its registry entry, every question legal, a card it heard someone
 * was given asked of them, games of three to six bots to the end, then self-play for two (tests/ai_selfplay.h).
 * Headless: this file plays the screen's part (families_live(), families_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/families.h"

static fam_game T;                                 /* the "screen's" game: the bot reads it */
static int pending;                             /* a new game, dealt at its first move from the game's seed */
static void deal_now(uint64_t seed);
const fam_game *families_live(void) { deal_now(ai_session_seed()); return &T; }
int families_show(const char *move) { return fam_script(&T, move); }

static int one(const char *m) { return fam_script(&T, m); }
static void deal_now(uint64_t seed)
{
    if (pending) fam_new(&T, 2, (uint32_t)seed);
    pending = 0;
}
static void reset(void) { pending = 1; }          /* (the harness sets the game's seed after it) */
static int actor(void) { return pending ? 0 : T.pos.turn; }
static int result(void)
{
    if (pending) return -1;
    unsigned w = fam_winners(&T.pos);
    return !w ? -1 : w == 3 ? 2 : w == 1 ? 0 : 1;
}
static int random_move(ai_rng *r, char *m, int cap)
{
    deal_now(ai_rand(r));
    const fpos *p = &T.pos;
    int legal[FAM_MAXP * FAM_CARDS], n = 0;
    for (int q = 0; q < p->players; q++)
        for (int f = 0; f < FAM_FAMILIES; f++)
            for (int k = 0; k < FAM_MEMBERS; k++)
                if (fam_legal(p, FAM_ASK(q, FAM_CARD(f, k)))) legal[n++] = q * 100 + f * 10 + k;
    if (!n) return 0;
    snprintf(m, (size_t)cap, "a%03d", legal[ai_rand_n(r, n)]);
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
    const bot *b = bot_find("families");
    check("registered: bot_find(\"families\")", b && b->think && b->play == families_show);
    {   /* a card it saw given: medium and hard ask its holder (or another holder they know of: a sure card) */
        int ok = 1, tried = 0;
        for (int k = 0; k < 400 && tried < 20; k++) {
            fam_new(&T, 3, 50 + (uint32_t)k);
            /* random questions until one is answered "given"; the turn then passes by a miss to the next player,
             * who needs that card */
            ai_rng r;
            ai_seed(&r, (uint64_t)k);
            char m[BOT_MOVE_CAP];
            for (int i = 0; i < 6 && T.pos.phase == FP_ASK; i++) {
                random_move(&r, m, sizeof m);
                one(m);
            }
            const fpos *p = &T.pos;
            if (p->phase != FP_ASK || p->ask_from == FAM_NONE || p->answer != FAM_GIVEN) continue;
            int got = p->ask_from, card = p->ask_card, me = (got + 1) % 3;
            if (p->turn != got) continue;
            /* hand the turn to `me` by a miss of `got`'s (a question for a card nobody else holds) */
            int moved = 0;
            for (int f = 0; f < FAM_FAMILIES && !moved; f++)
                for (int x = 0; x < FAM_MEMBERS && !moved; x++)
                    for (int q = 0; q < 3 && !moved; q++) {
                        int c = FAM_CARD(f, x), a = FAM_ASK(q, c);
                        if (!fam_legal(p, a) || fam_holds(p, q, c) || p->stock[p->nstock - 1] == c) continue;
                        moved = fam_play(&T, a);
                    }
            if (!moved || p->turn != me || !fam_has_family(p, me, FAM_FAMILY(card)) || fam_holds(p, me, card) ||
                !fam_holds(p, got, card))
                continue;
            tried++;
            for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
                ai_set_seed(3);
                b->think(me, lv, m, sizeof m);
                int q = m[1] - '0', c = FAM_CARD(m[2] - '0', m[3] - '0');   /* that card, or another as sure */
                if (!fam_holds(p, q, c)) printf("     %s asked \"%s\": a miss\n", sp_name(lv), m), ok = 0;
            }
        }
        snprintf(name, sizeof name, "a card heard given: medium and hard ask a holder they know of (%d tries)", tried);
        check(name, ok && tried >= 5);
    }
    {   /* three to six bots of every level, to the end */
        int ok = 1, games = 0;
        long questions = 0;
        for (int k = 0; k < 60; k++) {
            int np = 3 + k % 4;
            fam_new(&T, np, 400 + (uint32_t)k);
            while (T.pos.phase == FP_ASK && T.n < FAM_MAX_ACTS) {
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(ai_mix((uint64_t)k, (uint64_t)T.n));
                if (!b->think(T.pos.turn, (T.pos.turn + k) % 3, m, sizeof m) || !one(m)) { ok = 0; break; }
            }
            ok &= T.pos.phase == FP_OVER;
            questions += T.n;
            games++;
        }
        snprintf(name, sizeof name, "%d games of 3 to 6 bots end (%.0f questions a game)", games,
                 (double)questions / games);
        check(name, ok);
    }
    {   /* four at the table (where listening pays most): one player of a level against three of another, each seat
         * in turn; its share of the wins (a shared win split), 25% if they were alike */
        int full = 0;
        for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
        static const int duel[][3] = {   /* the one, the three, the share it must reach (percent) */
            {AI_EASY, SP_RANDOM, 60}, {AI_MEDIUM, AI_EASY, 30}, {AI_HARD, AI_MEDIUM, 40}, {AI_HARD, AI_EASY, 45},
        };
        int games = full ? 400 : 40;
        for (int d = 0; d < 4; d++) {
            double share = 0;
            for (int k = 0; k < games; k++) {
                int one_seat = k % 4;
                fam_new(&T, 4, 7000 + (uint32_t)k);
                ai_rng r;
                ai_seed(&r, (uint64_t)k);
                while (T.pos.phase == FP_ASK && T.n < FAM_MAX_ACTS) {
                    char m[BOT_MOVE_CAP] = "";
                    int s = T.pos.turn, lv = s == one_seat ? duel[d][0] : duel[d][1];
                    ai_set_seed(ai_mix((uint64_t)k, (uint64_t)T.n));
                    if (lv == SP_RANDOM) random_move(&r, m, sizeof m);
                    else b->think(s, lv, m, sizeof m);
                    if (!one(m)) break;
                }
                unsigned w = fam_winners(&T.pos);
                if (w >> one_seat & 1) share += 1.0 / __builtin_popcount(w);
            }
            snprintf(name, sizeof name, "four players, one %s against three %s: it wins %.0f%% (at least %d%%)",
                     sp_name(duel[d][0]), sp_name(duel[d][1]), 100 * share / games, duel[d][2]);
            check(name, 100 * share / games >= duel[d][2]);
        }
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "families", .max_plies = FAM_MAX_ACTS, .opening = 0, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    /* two players: most cards lie in the stock, few questions tell anything: mostly luck (the four-player table
     * above is where the levels part) */
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 58},
        {AI_MEDIUM, AI_EASY, 200, 40},
        {AI_HARD, AI_MEDIUM, 200, 40},
        {AI_HARD, AI_EASY, 200, 42},
    };
    /* the short run (a twentieth of the games: ten a pairing) checks the games; with so few, luck rules: its
     * targets are 30 points looser */
    int quick = 1;
    for (int i = 1; i < argc; i++) quick &= strcmp(argv[i], "--full") != 0;
    for (size_t i = 0; quick && i < sizeof P / sizeof P[0]; i++)
        P[i].min_pct = P[i].min_pct > 30 ? P[i].min_pct - 30 : 0;
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
