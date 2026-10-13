/*
 * Cribbage's bot (src/games/cribbage_bot.c): its registry entry, a view that keeps to what the seat may know (the
 * other hands' unplayed cards and the stock swapped among themselves, keeping the "go"s: the same action), the crib
 * laid away well on known hands, games of three bots to the end, then self-play at every level for two
 * (tests/ai_selfplay.h). Headless: this file plays the screen's part (cribbage_live(), cribbage_show()). --full for
 * the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/cribbage.h"

int cribb_choose(const cgame *g, int seat, int level, uint64_t seed);

static cgame T, U;
static int dealt, NP = 2;
const cgame *cribbage_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        crib_new(&T, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!s[0] || s[0] == '#' || !crib_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int cribbage_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void) { return dealt && T.pos.phase == CP_OVER ? crib_winner(&T.pos) : -1; }

static void name(int a, char *m, int cap)
{
    if (a == CA_NEXT) snprintf(m, (size_t)cap, "n");
    else if (a == CA_CUT) snprintf(m, (size_t)cap, "c");
    else if (a >= CA_PLAY) snprintf(m, (size_t)cap, "p%d", a - CA_PLAY);
    else {
        int k = 0;
        m[k++] = 'd';
        for (int i = 0; i < 6; i++)
            if (a >> i & 1) m[k++] = (char)('0' + i);
        m[k] = 0;
    }
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    int L[80], k = 0;
    for (int a = 1; a <= CA_NEXT; a++)
        if (crib_legal(&T.pos, a)) L[k++] = a;
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
    ai_seed(&r, 3);
    const bot *b = bot_find("cribbage");
    check("registered: bot_find(\"cribbage\")", b && b->think && b->actor && b->play == cribbage_show);

    /* honest: the other hands' unplayed cards and the stock shuffled together (a "go" kept: only cards above it go
     * to that player): the same action */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            NP = 2 + gn % 2;
            dealt = 0;
            char m[BOT_MOVE_CAP];
            snprintf(m, sizeof m, "D%d", 500 + gn);
            one(m);
            for (int k = 0; k < 2 + gn % 12 && T.pos.phase != CP_OVER; k++) random_move(&r, m, sizeof m), one(m);
            const cpos *p = &T.pos;
            if (p->phase != CP_PLAY && p->phase != CP_DISCARD) continue;
            int me = p->turn, acts[3];
            for (int lv = 0; lv < 3; lv++) acts[lv] = cribb_choose(&T, me, lv, 4);
            cgame save = T;
            cpos *w = &T.pos;
            uint8_t pool[60];
            int np = 0;
            for (int i = 0; i < w->nstock; i++) pool[np++] = w->stock[i];
            for (int o = 0; o < NP; o++) {
                if (o == me) continue;
                if (w->phase == CP_DISCARD)
                    for (int i = 0; i < w->nhand[o]; i++) pool[np++] = w->hand[o][i];
            }
            cards_shuffle(pool, np, 31u + (uint32_t)gn);
            int q = 0;
            for (int i = 0; i < w->nstock; i++) w->stock[i] = pool[q++];
            for (int o = 0; o < NP; o++) {
                if (o == me) continue;
                if (w->phase == CP_DISCARD)
                    for (int i = 0; i < w->nhand[o]; i++) w->hand[o][i] = pool[q++];
            }
            /* the play: the others' unplayed cards swapped with the stock's when both are above any go heard */
            if (w->phase == CP_PLAY)
                for (int o = 0; o < NP; o++) {
                    if (o == me) continue;
                    for (int s = 0; s < 4; s++) {
                        if (w->used[o] >> s & 1) continue;
                        int c = w->kept[o][s];
                        for (int i = 0; i < w->nstock; i++)
                            if (crib_value(w->stock[i]) == crib_value(c) && w->stock[i] != c) {
                                w->kept[o][s] = w->stock[i];
                                w->stock[i] = (uint8_t)c;
                                break;
                            }
                    }
                }
            for (int lv = 0; lv < 3; lv++) same &= cribb_choose(&T, me, lv, 4) == acts[lv];
            T = save;
            tried++;
        }
        snprintf(what, sizeof what, "the hidden cards swapped: the same action at every level (%d positions)", tried);
        check(what, same && tried > 30);
    }
    /* the crib: a hand of 5-5-J-Q-4-K, not dealing: keeps the fives and the courts, throws away 4 and K or Q */
    {
        NP = 2;
        crib_new(&T, 2, 9);
        dealt = 1;
        cpos *p = &T.pos;
        static const uint8_t h[6] = {CARD(1, 4), CARD(0, 5), CARD(2, 5), CARD(3, 11), CARD(1, 12), CARD(2, 13)};
        memcpy(p->hand[0], h, 6);
        int a = cribb_choose(&T, 0, AI_HARD, 1);
        check("hard keeps 5-5 with two courts on a hand of 4 5 5 J Q K", (a & 6) == 0 && (a & 1));
    }
    /* three bots to the end */
    {
        int ok = 1, games = 0;
        NP = 3;
        for (int gn = 0; gn < 12 && ok; gn++) {
            crib_new(&T, 3, 900 + (uint32_t)gn);
            dealt = 1;
            while (T.pos.phase != CP_OVER && T.n < CRIB_MAX_ACTS - 4) {
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(ai_mix((uint64_t)gn, (uint64_t)T.n));
                if (!b->think(T.pos.turn, gn % 3, m, sizeof m) || !one(m)) { ok = 0; break; }
            }
            ok &= T.pos.phase == CP_OVER;
            games++;
        }
        snprintf(what, sizeof what, "%d games of three bots end", games);
        check(what, ok);
        NP = 2;
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "cribbage", .max_plies = CRIB_MAX_ACTS - 8, .opening = 1, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 85}, {AI_MEDIUM, AI_EASY, 200, 55}, {AI_HARD, AI_MEDIUM, 200, 50},
        {AI_HARD, AI_EASY, 200, 58},
    };
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
