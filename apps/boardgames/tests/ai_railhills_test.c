/*
 * Valley Rails' bot (src/games/railhills_bot.c): its registry entry, honesty (the hidden things dealt again: the
 * others' hands, the decks, the face-down hexes give the same move at the greedy levels), games of three and four bots
 * to the end at every level, then self-play at every level for two, in both variants (tests/ai_selfplay.h).
 * Headless: this file plays the screen's part (railhills_live(), railhills_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/railhills.h"

static rh_state T;
static int dealt, NP = 2, VAR;
const rh_state *railhills_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {                               /* the first "move" deals: N<seed> */
        if (s[0] != 'N') return 0;
        rh_cfg c;
        rh_config(&c, NP, VAR);
        rh_new(&T, &c, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    int m;
    if (!rh_parse(s, &m) || !rh_legal(&T, m)) return 0;
    return rh_play(&T, m, NULL);
}
int railhills_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? rh_actor(&T) : 0; }
static int result(void)
{
    if (!dealt || !rh_over(&T)) return -1;
    unsigned w = rh_winners(&T);
    return w == 3 ? 2 : w == 1 ? 0 : 1;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "N%u", ai_rand(r) >> 2); return 1; }
    int mv[2 * RH_MAXC + 64], n = rh_moves(&T, mv), f[2 * RH_MAXC + 64], nf = 0;
    if (!n) return 0;
    for (int i = 0; i < n; i++)                 /* a random player that plays: not ending at once, few surveys */
        if (mv[i] < RH_M_SURVEY || (mv[i] >= RH_M_DELIVER && mv[i] < RH_M_END)) f[nf++] = mv[i];
    char t[16];
    rh_text(nf ? f[ai_rand_n(r, nf)] : mv[ai_rand_n(r, n)], t);
    snprintf(m, (size_t)cap, "%s", t);
    return 1;
}

static int fails;
static void check(const char *what, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

int main(int argc, char **argv)
{
    char what[160];
    ai_rng r;
    ai_seed(&r, 7);
    const bot *b = bot_find("railhills");
    check("registered: bot_find(\"railhills\")", b && b->think && b->play == railhills_show);

    /* honest: what the seat cannot see dealt again gives the planner the same move (it never looks there) */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 80; gn++) {
            NP = 2 + gn % 3;
            VAR = gn % 2;
            dealt = 0;
            char m[BOT_MOVE_CAP];
            snprintf(m, sizeof m, "N%d", 300 + gn);
            one(m);
            for (int k = 0; k < 4 + gn % 20 && !rh_over(&T); k++) {
                random_move(&r, m, sizeof m);
                one(m);
            }
            if (rh_over(&T)) continue;
            int me = rh_actor(&T), a[2];
            for (int lv = 0; lv < 2; lv++) a[lv] = rh_bot_move(&T, lv, 5);
            rh_state save = T;
            rh_determinise(&T, me, 1234 + gn);
            for (int lv = 0; lv < 2; lv++) {
                int m2 = rh_bot_move(&T, lv, 5);
                if (m2 != a[lv]) printf("     game %d level %d: %d then %d (phase %d)\n", gn, lv, a[lv], m2, T.phase);
                same &= m2 == a[lv];
            }
            T = save;
            tried++;
        }
        snprintf(what, sizeof what, "the hidden things dealt again: the same move (%d positions)", tried);
        check(what, same && tried > 50);
    }
    /* three and four bots, both variants, every level, to the end */
    {
        int ok = 1, games = 0;
        for (int gn = 0; gn < 6 && ok; gn++) {
            NP = 3 + gn % 2;
            VAR = gn / 2 % 2;
            dealt = 0;
            char m[BOT_MOVE_CAP];
            snprintf(m, sizeof m, "N%d", 40 + gn);
            one(m);
            int plies = 0;
            while (!rh_over(&T) && plies++ < 3000) {
                m[0] = 0;
                ai_set_seed(ai_mix((uint64_t)gn, (uint64_t)plies));
                int seat = rh_actor(&T);
                if (!b->think(seat, (gn + seat) % 3, m, sizeof m) || !one(m)) {
                    printf("     game %d: \"%s\" refused\n", gn, m);
                    ok = 0;
                    break;
                }
            }
            ok &= rh_over(&T);
            games++;
        }
        snprintf(what, sizeof what, "%d games of three and four bots, both variants, every level, end", games);
        check(what, ok);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "railhills", .max_plies = 3000, .opening = 1, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 90}, {AI_MEDIUM, AI_EASY, 200, 65}, {AI_HARD, AI_MEDIUM, 200, 60},
        {AI_HARD, AI_EASY, 100, 75},
    };
    static const char *vname[] = {"misty valley", "clear valley"};
    int bad = 0;
    NP = 2;
    for (VAR = 0; VAR < 2; VAR++) {
        printf("-- %s\n", vname[VAR]);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
