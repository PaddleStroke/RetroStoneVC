/*
 * Rikiki: the rules' test (the deals going up and down, the turned card and the deal without trumps, the bids and the
 * dealer's rule, following suit, trumps winning, the score, undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/rikiki.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static rikiki_game G, H;
static rikiki_pos *P = &G.pos;

static void test_deals(void)
{
    static const int max[7] = {0, 0, 0, 13, 13, 10, 8};
    for (int np = 3; np <= 6; np++) {
        CHECK(rikiki_max(np) == max[np]);
        CHECK(rikiki_size(np, 0) == 1 && rikiki_size(np, max[np] - 1) == max[np] && rikiki_size(np, max[np]) ==
              max[np] - 1 && rikiki_size(np, 2 * max[np] - 2) == 1);
        rikiki_new(&G, np, 7u + np);
        CHECK(P->deals == 2 * max[np] - 1 && P->size == 1 && P->dealer == 0 && P->first == 1 && P->turn == 1);
        CHECK(P->turned != CARD_NONE && P->trump == CARD_SUIT(P->turned));
        int deal = 0;
        while (P->phase != RK_OVER) {          /* every deal: the right size, each card once, the turned card apart */
            int seen[256] = {0}, ok = 1;
            CHECK(P->size == rikiki_size(np, deal) && P->dealer == deal % np);
            for (int pl = 0; pl < np; pl++) {
                ok &= P->n[pl] == P->size;
                for (int i = 0; i < P->n[pl]; i++) seen[P->hand[pl][i]]++;
            }
            if (P->turned != CARD_NONE) seen[P->turned]++;
            for (int c = 0; c < 256; c++) ok &= seen[c] <= 1;
            CHECK(ok);
            CHECK((P->turned == CARD_NONE) == (np * P->size == 52));
            CHECK(rikiki_script(&G, "#"));
            deal++;
            if (P->phase == RK_ROUND) CHECK(rikiki_script(&G, "n"));
        }
        CHECK(deal == 2 * max[np] - 1);
    }
    rikiki_new(&G, 4, 3);                       /* four players: the 13-card deal without trumps */
    CHECK(rikiki_script(&G, "# n # n # n # n # n # n # n # n # n # n # n # n"));
    CHECK(P->size == 13 && P->turned == CARD_NONE && P->trump == -1);
    rikiki_new(&G, 4, 3);
    rikiki_new(&H, 4, 3);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
}

/* the bids: 0..size; the dealer bids last and may not make the total the number of cards */
static void test_bids(void)
{
    rikiki_new(&G, 4, 11);
    CHECK(rikiki_script(&G, "# n # n"));         /* the third deal: 3 cards, dealer 2 */
    CHECK(P->size == 3 && P->dealer == 2 && P->turn == 3);
    CHECK(!rikiki_legal(P, RKA_BID | 4) && rikiki_forbidden(P) == -1);
    CHECK(rikiki_script(&G, "b1 b0 b1"));
    CHECK(P->turn == 2 && rikiki_forbidden(P) == 1);
    CHECK(!rikiki_can_bid(P, 1) && rikiki_can_bid(P, 0) && rikiki_can_bid(P, 2) && rikiki_can_bid(P, 3));
    CHECK(!rikiki_script(&G, "b1"));
    CHECK(rikiki_script(&G, "b2"));
    CHECK(P->phase == RK_PLAY && P->turn == 3 && P->leader == 3 && rikiki_bids(P) == 4);
    rikiki_new(&G, 3, 5);                       /* over the total already: the dealer is free */
    CHECK(rikiki_script(&G, "# n"));
    CHECK(P->size == 2 && P->dealer == 1 && P->turn == 2);
    CHECK(rikiki_script(&G, "b2 b2") && rikiki_forbidden(P) == -1 && rikiki_can_bid(P, 0));
}

/* hands set by hand, four players, play phase */
static void set_hands(const char *h0, const char *h1, const char *h2, const char *h3, int trump)
{
    rikiki_new(&G, 4, 3);
    const char *hs[4] = {h0, h1, h2, h3};
    for (int pl = 0; pl < 4; pl++) {
        const char *s = hs[pl];
        P->n[pl] = 0;
        while (*s) {
            while (*s == ' ') s++;
            int c = trick_parse(&s);
            if (c == CARD_NONE) break;
            P->hand[pl][P->n[pl]++] = (uint8_t)c;
        }
        trick_sort(P->hand[pl], P->n[pl]);
        P->bid[pl] = 0;
    }
    P->size = P->n[0];
    P->trump = (int8_t)trump;
    P->phase = RK_PLAY;
    P->turn = P->leader = 1;
    P->first = 1;
    G.n = 0;
}

static void test_tricks(void)
{
    set_hands("2h 3c", "Ac 4d", "Kc 5d", "2s Qd", SUIT_SPADES);
    P->bid[0] = 1, P->bid[1] = 1, P->bid[2] = 0, P->bid[3] = 1;
    CHECK(rikiki_script(&G, "Ac"));
    CHECK(!rikiki_script(&G, "5d") && rikiki_script(&G, "Kc"));   /* follow suit */
    CHECK(rikiki_script(&G, "2s"));             /* void: a trump, not forced */
    CHECK(!rikiki_script(&G, "2h") && rikiki_script(&G, "3c"));
    CHECK(P->last_winner == 3 && P->won[3] == 1 && P->turn == 3);   /* the small trump wins */
    CHECK(rikiki_script(&G, "Qd 2h 4d 5d"));    /* a void, no trump: anything */
    CHECK(P->phase == RK_ROUND && P->won[3] == 2 && P->won[0] == 0);
    CHECK(P->score[0] == -1 && P->score[1] == -1 && P->score[2] == 10 && P->score[3] == -1);
    CHECK(rikiki_points(3, 3) == 13 && rikiki_points(0, 0) == 10);
    CHECK(rikiki_points(2, 5) == -3 && rikiki_points(4, 1) == -3);
    set_hands("2h 3c", "Ac 4d", "Kc 5d", "2s Qd", -1);   /* no trumps: the suit led wins */
    CHECK(rikiki_script(&G, "Ac Kc 2s 3c"));
    CHECK(P->last_winner == 1);
}

static void test_undo_end(void)
{
    rikiki_new(&G, 5, 77);
    CHECK(rikiki_script(&G, "# n . . . . . . ."));
    int n = G.n;
    rikiki_pos before = G.pos;
    CHECK(rikiki_script(&G, ". ."));
    CHECK(rikiki_undo(&G) && rikiki_undo(&G) && G.n == n && !memcmp(&G.pos, &before, sizeof before));
    while (rikiki_undo(&G)) {}
    rikiki_new(&H, 5, 77);
    CHECK(G.n == 0 && !memcmp(&G.pos, &H.pos, sizeof H.pos));
    rikiki_new(&G, 3, 1);
    CHECK(rikiki_script(&G, "!") && P->phase == RK_OVER && !rikiki_legal(P, RKA_NEXT));
    unsigned w = rikiki_winners(P);
    int top = -1000;
    for (int pl = 0; pl < 3; pl++) top = P->total[pl] > top ? P->total[pl] : top;
    for (int pl = 0; pl < 3; pl++) CHECK(((w >> pl) & 1) == (P->total[pl] == top));
}

/* whole games from seeds, random legal actions: the cards kept, nobody fails the dealer's rule, the scores add up */
static void test_random_games(void)
{
    int games = 0;
    for (uint32_t seed = 1; seed <= 200; seed++) {
        int np = 3 + (int)(seed % 4);
        rikiki_new(&G, np, seed * 7919u);
        uint32_t r = seed;
        int ok = 1, steps = 0, sum[RIKIKI_MAXP] = {0};
        while (P->phase != RK_OVER && ok && steps++ < 100000) {
            if (P->phase == RK_ROUND) {
                int bids = 0, won = 0;
                for (int pl = 0; pl < np; pl++) {
                    bids += P->bid[pl], won += P->won[pl];
                    sum[pl] += P->score[pl];
                    ok &= P->total[pl] == sum[pl];
                    ok &= P->score[pl] == rikiki_points(P->bid[pl], P->won[pl]);
                }
                ok &= bids != P->size && won == P->size;
                ok &= rikiki_play(&G, RKA_NEXT);
                continue;
            }
            int pl = P->turn;
            if (P->phase == RK_BID) {
                int b = (int)(cards_rand(&r) % (uint32_t)(P->size + 1));
                if (!rikiki_can_bid(P, b)) b = b ? b - 1 : 1;
                ok &= rikiki_play(&G, RKA_BID | b);
                continue;
            }
            uint8_t legal[RIKIKI_HAND];
            int k = 0;
            for (int i = 0; i < P->n[pl]; i++)
                if (rikiki_can_play(P, P->hand[pl][i])) legal[k++] = P->hand[pl][i];
            ok &= k > 0;
            if (!k) break;
            ok &= rikiki_play(&G, RKA_PLAY | legal[cards_rand(&r) % (uint32_t)k]);
        }
        ok &= P->phase == RK_OVER && P->deal == P->deals;
        CHECK(ok);
        if (!ok) { printf("  seed %u players %d\n", seed, np); break; }
        games++;
    }
    CHECK(games == 200);
    printf("  %d random games\n", games);
}

int main(void)
{
    test_deals();
    test_bids();
    test_tricks();
    test_undo_end();
    test_random_games();
    printf("rikiki: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
