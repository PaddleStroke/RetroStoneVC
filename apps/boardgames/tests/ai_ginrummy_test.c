/*
 * Gin Rummy's bot (src/games/ginrummy_bot.c): its registry entry, a view that keeps to what the seat may know (the
 * other player's unknown cards and the stock swapped among themselves: the same action), knocks and gins taken, then
 * self-play at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part (ginrummy_live(),
 * ginrummy_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/ginrummy.h"

int ginb_choose(const gngame *g, int seat, int level, uint64_t seed);

static gngame T, U;
static int dealt;
const gngame *ginrummy_live(void) { return &T; }

/* one action of the notation, as ginrummy_show() takes it; "D<seed>" deals the test's game (the opening ply) */
static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        gin_new(&T, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!s[0] || s[0] == '#' || !gin_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int ginrummy_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void)
{
    if (!dealt || T.pos.phase != GN_OVER) return -1;
    return gin_winner(&T.pos);
}

static void name(int a, char *m, int cap)
{
    static const char ranks[] = "A23456789TJQK", suits[] = "shdc";
    int c = GNA_CARD(a);
    switch (GNA_KIND(a)) {
    case GNA_STOCK: snprintf(m, (size_t)cap, "s"); break;
    case GNA_TAKE: snprintf(m, (size_t)cap, "t"); break;
    case GNA_PASS: snprintf(m, (size_t)cap, "p"); break;
    case GNA_NEXT: snprintf(m, (size_t)cap, "n"); break;
    default:
        snprintf(m, (size_t)cap, "%c%c%c", GNA_KIND(a) == GNA_KNOCK ? 'k' : 'd', ranks[CARD_RANK(c) - 1],
                 suits[CARD_SUIT(c)]);
    }
}

/* a random player: any legal action (it knocks when it may, half the time) */
static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const gnpos *p = &T.pos;
    int L[32], k = 0;
    static const int kinds[] = {GNA_STOCK, GNA_TAKE, GNA_PASS, GNA_NEXT};
    for (int i = 0; i < 4; i++)
        if (gin_legal(p, kinds[i])) L[k++] = kinds[i];
    if (p->phase == GN_DISCARD) {
        int knock = ai_rand_n(r, 2);
        for (int i = 0; i < p->n[p->turn]; i++) {
            int c = p->hand[p->turn][i];
            if (knock && gin_legal(p, GNA_KNOCK | c)) L[k++] = GNA_KNOCK | c;
            else if (gin_legal(p, GNA_DISCARD | c)) L[k++] = GNA_DISCARD | c;
        }
    }
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
    ai_seed(&r, 5);
    const bot *b = bot_find("ginrummy");
    check("registered: bot_find(\"ginrummy\")", b && b->think && b->actor && b->play == ginrummy_show);

    /* honest: the other player's hand (less the cards it took from the pile) and the stock shuffled together: the
     * same action at every level */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 40; gn++) {
            dealt = 0;
            char m[BOT_MOVE_CAP];
            snprintf(m, sizeof m, "D%d", 100 + gn);
            one(m);
            for (int k = 0; k < 3 + gn % 25 && T.pos.phase < GN_END; k++) random_move(&r, m, sizeof m), one(m);
            if (T.pos.phase >= GN_END) continue;
            const gnpos *p = &T.pos;
            int me = p->turn, ot = 1 - me;
            int act0[3];
            for (int lv = 0; lv < 3; lv++) act0[lv] = ginb_choose(&T, me, lv, 9);
            /* the other's cards taken from the pile stay; the rest pooled with the stock and dealt again */
            gngame save = T;
            uint8_t pool[64];
            int np = 0, taken[128] = {0};
            gngame *t = malloc(sizeof *t);
            gin_new(t, T.seed);
            for (int i = 0; i < T.n; i++) {
                const gnpos *q = &t->pos;
                int a = T.act[i];
                if (GNA_KIND(a) == GNA_NEXT) memset(taken, 0, sizeof taken);
                else if (GNA_KIND(a) == GNA_TAKE && q->turn == ot) taken[q->pile[q->npile - 1]] = 1;
                gin_play(t, a);
            }
            free(t);
            for (int i = 0; i < p->n[ot]; i++)
                if (!taken[p->hand[ot][i]]) pool[np++] = p->hand[ot][i];
            for (int i = 0; i < p->nstock; i++) pool[np++] = p->stock[i];
            cards_shuffle(pool, np, 77u + (uint32_t)gn);
            int q = 0;
            gnpos *w = &T.pos;
            for (int i = 0; i < w->n[ot]; i++)
                if (!taken[w->hand[ot][i]]) w->hand[ot][i] = pool[q++];
            for (int i = 0; i < w->nstock; i++) w->stock[i] = pool[q++];
            for (int lv = 0; lv < 3; lv++) same &= ginb_choose(&T, me, lv, 9) == act0[lv];
            T = save;
            tried++;
        }
        snprintf(what, sizeof what, "the hidden cards swapped: the same action at every level (%d positions)", tried);
        check(what, same && tried > 20);
    }
    /* a hand of gin is knocked at once; 10 or less is knocked by medium */
    {
        gin_new(&T, 1);
        dealt = 1;
        gnpos *p = &T.pos;
        static const uint8_t h[11] = {CARD(0, 1), CARD(0, 2), CARD(0, 3), CARD(1, 5), CARD(2, 5), CARD(3, 5),
                                      CARD(1, 9), CARD(1, 10), CARD(1, 11), CARD(1, 12), CARD(3, 13)};
        p->phase = GN_DISCARD;
        p->turn = 0;
        p->taken = CARD_NONE;
        memcpy(p->hand[0], h, 11);
        p->n[0] = 11;
        int ok = 1;
        for (int lv = 0; lv < 3; lv++) ok &= ginb_choose(&T, 0, lv, 3) == (GNA_KNOCK | CARD(3, 13));
        check("gin: every level knocks, throwing the king of clubs", ok);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "ginrummy", .max_plies = GIN_MAX_ACTS - 8, .opening = 1, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 85}, {AI_MEDIUM, AI_EASY, 200, 55}, {AI_HARD, AI_MEDIUM, 200, 50},
        {AI_HARD, AI_EASY, 200, 58},
    };
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
