/*
 * Secret Code's bot (src/games/secretcode_bot.c): its registry entry, guesses that never read the code, a code found
 * from any answer, the cancel, then each level alone breaking the same codes in every variant (one player: not the
 * harness's pairings, its kit only): the codes found, the guesses, the time. Headless: this file plays the screen's
 * part (secretcode_live(), secretcode_show()). --full for 200 codes a level and variant (else 20).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"

#include "games/secretcode.h"

#define SP_RANDOM (-1)                          /* a "level": the random guesser */
static const char *sp_name(int level)
{
    return level == SP_RANDOM ? "random" : level == AI_EASY ? "easy" : level == AI_MEDIUM ? "medium" : "hard";
}

static sc_game T;                               /* the "screen's" game: the bot reads it */
static int variant;
static ai_rng deal;

static int rnd(int n) { return ai_rand_n(&deal, n); }
static void make(uint64_t seed)
{
    int8_t code[SC_MAXPEGS];
    ai_seed(&deal, ai_mix(seed, (uint64_t)variant));
    sc_make_code(variant, rnd, code);
    sc_new(&T, variant, code);
}
const sc_game *secretcode_live(void) { return &T; }

static int one(const char *s)
{
    if (sc_status(&T) != SC_PLAY || (int)strlen(s) != T.v.pegs) return 0;
    for (int i = 0; i < T.v.pegs; i++)
        if (!sc_put(&T, i, sc_colour(s[i]))) return 0;
    return sc_check(&T);
}
int secretcode_show(const char *move) { return one(move); }

static void fresh(void) { make(ai_session_seed()); }   /* a new game, its code from the session seed */
static int random_move(ai_rng *r, char *m, int cap)
{
    if (cap <= T.v.pegs) return 0;
    for (int i = 0; i < T.v.pegs; i++) m[i] = sc_letters[ai_rand_n(r, T.v.colours)];
    m[T.v.pegs] = 0;
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
    const bot *b = bot_find("secretcode");
    check("registered: bot_find(\"secretcode\")", b && b->think && b->play == secretcode_show);

    {   /* the code is never read: the same guesses with the code changed under it (answers kept) */
        int same = 1;
        for (int v = 0; v < SC_VARIANTS && same; v++)
            for (int lv = AI_EASY; lv <= AI_HARD && same; lv++) {
                variant = v;
                ai_set_seed(77 + (uint64_t)lv);
                fresh();
                one(v == 2 ? "RYGBW" : "RYGB");
                one(v == 2 ? "KPSRR" : "WKRY");
                sc_game keep = T;
                char m1[BOT_MOVE_CAP] = "", m2[BOT_MOVE_CAP] = "";
                b->think(0, lv, m1, sizeof m1);
                memset(T.code, 0, sizeof T.code);   /* another code, the same answers on the board */
                b->think(0, lv, m2, sizeof m2);
                same = !strcmp(m1, m2) && m1[0];
                T = keep;
            }
        check("the guesses never depend on the code itself", same);
    }
    {   /* not seat 0, or over: no move */
        char m[BOT_MOVE_CAP] = "";
        variant = 1;
        fresh();
        int ok = !b->think(1, AI_HARD, m, sizeof m);
        memcpy(T.guess[0], T.code, sizeof T.code);
        sc_check(&T);
        ok &= sc_status(&T) == SC_SOLVED && !b->think(0, AI_HARD, m, sizeof m);
        check("not its seat, or the code found: no move", ok);
    }
    {   /* every classic code found by hard within 7 guesses (its worst case with this opening is 6 or 7) */
        int worst = 0, all = 1;
        long sum = 0;
        for (int k = 0; k < 1296; k++) {
            int8_t code[SC_MAXPEGS] = {0};
            for (int i = 0, x = k; i < 4; i++, x /= 6) code[i] = (int8_t)(x % 6);
            sc_new(&T, 1, code);
            int n = 0;
            while (sc_status(&T) == SC_PLAY && n < 12) {
                int8_t g[SC_MAXPEGS];
                ai_set_seed((uint64_t)k);
                if (!scb_guess(&T, AI_HARD, ai_session_seed(), g)) break;
                for (int i = 0; i < 4; i++) sc_put(&T, i, g[i]);
                sc_check(&T);
                n++;
            }
            all &= sc_status(&T) == SC_SOLVED;
            sum += n;
            if (n > worst) worst = n;
            if (k % 8 && argc < 2) k += 7;      /* the short run: one code in eight */
        }
        snprintf(name, sizeof name, "hard finds every classic code tried (worst %d guesses, %s)", worst,
                 argc < 2 ? "one code in eight" : "all 1296");
        check(name, all && worst <= 7);
        (void)sum;
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        variant = 2;
        fresh();
        one("RYGBW");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 100);
    }
    if (fails) return 1;

    /* each level alone (one player: the harness's pairings are for two), on fixed codes: the codes found, the guesses
     * they took, the time a guess, every guess accepted by the rules */
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    int games = full ? 200 : 20;
    static const int need[SC_VARIANTS][AI_LEVELS] = {{90, 99, 100}, {90, 99, 100}, {85, 98, 100}};
    for (variant = 0; variant < SC_VARIANTS; variant++) {
        const sc_variant *v = &sc_variants[variant];
        printf("-- variant %d (%d pegs, %d colours, %d rows%s), %d codes a level\n", variant, v->pegs, v->colours,
               v->rows, v->repeats ? "" : ", no repeats", games);
        printf("  %-8s %6s %8s %6s %9s %9s\n", "level", "found", "guesses", "worst", "ms/guess", "max ms");
        for (int lv = SP_RANDOM; lv <= AI_HARD; lv++) {
            int found = 0, worst = 0, moves = 0;
            long guesses = 0;
            double ms = 0, maxms = 0;
            for (int k = 0; k < games; k++) {
                uint64_t seed = ai_mix(0x5c + (uint64_t)variant, (uint64_t)k);   /* the same codes for every level */
                ai_set_seed(seed);
                fresh();
                ai_rng r;
                ai_seed(&r, seed);
                int n = 0;
                while (sc_status(&T) == SC_PLAY) {
                    char m[BOT_MOVE_CAP] = "";
                    ai_set_seed(ai_mix(seed, (uint64_t)n + 1));
                    double t0 = ai_now_ms();
                    int ok = lv == SP_RANDOM ? random_move(&r, m, sizeof m) : b->think(0, lv, m, sizeof m);
                    double dt = ai_now_ms() - t0;
                    if (lv != SP_RANDOM) {
                        ms += dt;
                        moves++;
                        if (dt > maxms) maxms = dt;
                        if (dt > ai_level_budget(lv).ms + 50) {
                            printf("FAIL %s took %.0f ms\n", sp_name(lv), dt);
                            fails++;
                        }
                    }
                    if (!ok || !one(m)) {
                        printf("FAIL %s variant %d code %d: \"%s\" refused\n", sp_name(lv), variant, k, m);
                        fails++;
                        break;
                    }
                    n++;
                }
                if (sc_status(&T) == SC_SOLVED) {
                    found++;
                    guesses += n;
                    if (n > worst) worst = n;
                }
            }
            printf("  %-8s %5d%% %8.2f %6d %9.1f %9.1f\n", sp_name(lv), 100 * found / games,
                   found ? (double)guesses / found : 0, worst, moves ? ms / moves : 0, maxms);
            if (lv >= AI_EASY && 100 * found < need[variant][lv] * games) {
                printf("FAIL %s found %d of %d codes (under %d%%)\n", sp_name(lv), found, games, need[variant][lv]);
                fails++;
            }
        }
    }
    printf("secretcode: %s\n", fails ? "FAILED" : "every guess legal, every game ended, in time");
    return fails != 0;
}
