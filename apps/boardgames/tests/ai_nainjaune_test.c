/*
 * Le Nain jaune's bot (src/games/nainjaune_bot.c): its registry entry, a view that keeps to what the seat may know
 * (the other hands and the talon dealt again, keeping to the ranks each was seen to lack: the same card), one of the
 * five played when the rank asked for allows it, games of three to six bots in both variants to the end, then
 * self-play at every level for three (tests/ai_selfplay.h: seats 0 and 2 play one level, seat 1 the other, and
 * the other way round in the next game; the side holding the most chips wins). The game leaves the players few
 * choices (the leads, which of a rank): the luck of the deal outweighs them, so the targets are modest.
 * Headless: this file plays the screen's part (nainjaune_live(), nainjaune_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "cards.h"
#include "games/nainjaune.h"

int njb_choose(const njgame *g, int seat, int level, uint64_t seed, ai_stats *stats);

static njgame T, U;
static int dealt, NP = 4, VAR = NJ_CLASSIC;
const njgame *nainjaune_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        nj_new(&T, VAR, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!s[0] || strchr(".#", s[0]) || !nj_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int nainjaune_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void)
{
    if (!dealt || T.pos.phase != NP_OVER) return -1;
    unsigned w = nj_winners(&T.pos), half = 0;
    for (int q = 0; q < NP; q++)
        if (w >> q & 1) half |= 1u << (q & 1);
    return half == 3 ? 2 : half == 2;
}

static void name(int a, char *m, int cap)
{
    if (a == NJ_NEXT) snprintf(m, (size_t)cap, "n");
    else snprintf(m, (size_t)cap, "%c%c", "a23456789tjqk"[CARD_RANK(a) - 1], "shdc"[CARD_SUIT(a)]);
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const njpos *p = &T.pos;
    if (p->phase == NP_DEALT) { name(NJ_NEXT, m, cap); return 1; }
    int L[NJ_HAND], k = 0;
    for (int i = 0; i < p->n[p->turn]; i++)
        if (nj_legal(p, p->hand[p->turn][i])) L[k++] = p->hand[p->turn][i];
    if (!k) return 0;
    name(L[ai_rand_n(r, k)], m, cap);
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
    ai_seed(&r, 17);
    const bot *b = bot_find("nainjaune");
    check("registered: bot_find(\"nainjaune\")", b && b->think && b->actor && b->play == nainjaune_show);

    /* honest: the other hands and the talon dealt again (counts kept; at the deal's start, before anyone is seen
     * lacking a rank): the same card at every level */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 40; gn++) {
            NP = 3 + gn % 4;
            VAR = gn % NJ_VARIANTS;
            nj_new(&T, VAR, NP, 600 + (uint32_t)gn);
            dealt = 1;
            const njpos *p = &T.pos;
            int me = p->turn, acts[3];
            for (int lv = 0; lv < 3; lv++) acts[lv] = njb_choose(&T, me, lv, 3, NULL);
            njgame save = T;
            njpos *w = &T.pos;
            uint8_t pool[52];
            int n = 0;
            for (int q = 0; q < NP; q++)
                if (q != me)
                    for (int i = 0; i < w->n[q]; i++) pool[n++] = w->hand[q][i];
            for (int i = 0; i < w->ntalon; i++) pool[n++] = w->talon[i];
            cards_shuffle(pool, n, 3u + (uint32_t)gn);
            int k = 0;
            for (int q = 0; q < NP; q++)
                if (q != me)
                    for (int i = 0; i < w->n[q]; i++) w->hand[q][i] = pool[k++];
            for (int i = 0; i < w->ntalon; i++) w->talon[i] = pool[k++];
            for (int lv = 0; lv < 3; lv++) same &= njb_choose(&T, me, lv, 3, NULL) == acts[lv];
            T = save;
            tried++;
        }
        snprintf(what, sizeof what, "the hidden cards dealt again: the same card at every level (%d positions)", tried);
        check(what, same && tried >= 40);
    }
    /* one of the five: the 7 of diamonds asked for with a 7 of spades in hand too: the 7 of diamonds */
    {
        NP = 4;
        nj_new(&T, NJ_CLASSIC, 4, 5);
        dealt = 1;
        njpos *p = &T.pos;
        p->turn = 0;
        p->need = 7;
        static const uint8_t h[4] = {CARD(SUIT_SPADES, 7), CARD(SUIT_DIAMONDS, 7), CARD(SUIT_CLUBS, 2),
                                     CARD(SUIT_HEARTS, 9)};
        memcpy(p->hand[0], h, 4);
        p->n[0] = 4;
        int ok = 1;
        for (int lv = 0; lv < 3; lv++) {
            int a = njb_choose(&T, 0, lv, 1, NULL);
            if (a != CARD(SUIT_DIAMONDS, 7)) printf("     level %d: %d\n", lv, a), ok = 0;
        }
        check("the 7 of diamonds played before the 7 of spades, every level", ok);
    }
    /* three to six bots, both variants, to the end */
    {
        int ok = 1, games = 0;
        for (int gn = 0; gn < 16 && ok; gn++) {
            NP = 3 + gn % 4;
            VAR = gn / 4 % NJ_VARIANTS;
            nj_new(&T, VAR, NP, 70 + (uint32_t)gn);
            dealt = 1;
            while (T.pos.phase != NP_OVER && T.n < NJ_MAX_ACTS - 4) {
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(ai_mix((uint64_t)gn, (uint64_t)T.n));
                if (!b->think(T.pos.turn, gn % 3, m, sizeof m) || !one(m)) { ok = 0; break; }
            }
            ok &= T.pos.phase == NP_OVER;
            games++;
        }
        snprintf(what, sizeof what, "%d games of three to six bots, both variants, end", games);
        check(what, ok);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "nainjaune", .max_plies = NJ_MAX_ACTS - 8, .opening = 1, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 52}, {AI_MEDIUM, AI_EASY, 200, 45}, {AI_HARD, AI_MEDIUM, 200, 50},
        {AI_HARD, AI_EASY, 200, 54},
    };
    NP = 3;
    VAR = NJ_CLASSIC;
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
