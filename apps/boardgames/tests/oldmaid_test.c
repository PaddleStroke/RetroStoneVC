/*
 * Old Maid (le pouilleux): the rules' test (the pack without the jack of clubs, pairs by rank and colour, the deal
 * and its pairs, drawing from the previous player still holding cards, players going out, the loser, whole games
 * from seeds, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/oldmaid.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static ogame G, H;
static opos *P = &G.pos;

/* the 51 cards once each, wherever they are; no pair left in any hand */
static int whole(const opos *p)
{
    int seen[256] = {0}, m = 0;
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++) {
            seen[p->hand[q][i]]++, m++;
            for (int j = i + 1; j < p->n[q]; j++)
                if (om_pairs(p->hand[q][i], p->hand[q][j])) return 0;
        }
    for (int i = 0; i < p->npile; i++) seen[p->pile[i]]++, m++;
    if (m != OM_CARDS || seen[CARD(SUIT_CLUBS, RANK_JACK)]) return 0;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (!(s == SUIT_CLUBS && r == RANK_JACK) && seen[CARD(s, r)] != 1) return 0;
    return p->npile % 2 == 0;
}

static void test_pairs(void)
{
    CHECK(om_pairs(CARD(SUIT_HEARTS, 7), CARD(SUIT_DIAMONDS, 7)));
    CHECK(om_pairs(CARD(SUIT_SPADES, 13), CARD(SUIT_CLUBS, 13)));
    CHECK(!om_pairs(CARD(SUIT_SPADES, 7), CARD(SUIT_HEARTS, 7)));     /* not the same colour */
    CHECK(!om_pairs(CARD(SUIT_SPADES, 7), CARD(SUIT_SPADES, 8)));
    CHECK(!om_pairs(OM_ODD, CARD(SUIT_HEARTS, RANK_JACK)) && !om_pairs(OM_ODD, CARD(SUIT_DIAMONDS, RANK_JACK)));
}

static void test_deal(void)
{
    for (int np = 2; np <= 6; np++) {
        om_new(&G, np, 42);
        om_new(&H, np, 42);
        CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && whole(P) && P->phase == OP_DRAW && P->turn == 0);
        int held = 0;
        for (int q = 0; q < np; q++) held += P->n[q];
        CHECK(held + P->npile == OM_CARDS && held % 2 == 1);   /* pairs gone: an odd count left (the jack) */
        CHECK(om_offerer(P) == np - 1);
    }
}

/* a hand of our own: player 1 holds the jack and a red five, player 0 the other red five */
static void test_draw(void)
{
    om_new(&G, 2, 5);
    P->n[0] = P->n[1] = 0;
    P->npile = 0;
    P->hand[0][P->n[0]++] = CARD(SUIT_HEARTS, 5);
    P->hand[1][P->n[1]++] = OM_ODD;
    P->hand[1][P->n[1]++] = CARD(SUIT_DIAMONDS, 5);
    P->turn = 0;
    int i = P->hand[1][0] == OM_ODD ? 1 : 0;    /* the five */
    CHECK(om_offerer(P) == 1 && !om_play(&G, 2) && !om_play(&G, -1));
    CHECK(om_play(&G, i) && P->n[0] == 0 && P->npile == 2 && P->phase == OP_OVER && P->loser == 1);
    CHECK(om_winners(P) == 1u);
    /* drawing the jack: the turn passes, the jack changes hands */
    om_new(&G, 3, 5);
    P->n[0] = P->n[1] = P->n[2] = 0;
    P->npile = 0;
    P->hand[0][P->n[0]++] = CARD(SUIT_HEARTS, 5);
    P->hand[1][P->n[1]++] = CARD(SUIT_SPADES, 2);
    P->hand[2][P->n[2]++] = OM_ODD;
    P->hand[2][P->n[2]++] = CARD(SUIT_CLUBS, 2);
    P->turn = 0;
    int j = P->hand[2][0] == OM_ODD ? 0 : 1;
    CHECK(om_play(&G, j) && P->n[0] == 2 && P->turn == 1 && om_offerer(P) == 0 && P->phase == OP_DRAW);
    /* player 2 is passed over once out of cards */
    P->n[2] = 0;
    CHECK(om_offerer(P) == 0);
    P->turn = 0;
    CHECK(om_offerer(P) == 1);
}

static void test_games(void)
{
    for (int np = 2; np <= 6; np++) {
        int ok = 1, draws = 0, lost[OM_MAXP] = {0};
        for (uint32_t seed = 1; seed <= 40; seed++) {
            om_new(&G, np, seed * 31u + (uint32_t)np);
            while (P->phase == OP_DRAW) {
                int o = om_offerer(P);
                if (!om_play(&G, (int)((seed + (uint32_t)G.n) % (uint32_t)P->n[o]))) { ok = 0; break; }
                if (!whole(P)) { ok = 0; break; }
            }
            draws += G.n;
            ok &= P->phase == OP_OVER && P->loser < np && P->n[P->loser] == 1 && P->hand[P->loser][0] == OM_ODD;
            ok &= om_winners(P) == (((1u << np) - 1) & ~(1u << P->loser));
            if (P->loser < np) lost[P->loser]++;
        }
        CHECK(ok);
        printf("ok   %d players: 40 games, %d draws; lost by player 0: %d\n", np, draws, lost[0]);
    }
    om_new(&G, 4, 9);
    CHECK(om_script(&G, "d0 d1 d0"));
    int n = G.n;
    opos before = G.pos;
    CHECK(om_script(&G, "d0") && om_undo(&G) && G.n == n && !memcmp(&before, &G.pos, sizeof before));
    CHECK(om_script(&G, "#") && P->phase == OP_OVER && !om_script(&G, "d0"));
    om_new(&G, 2, 9);
    CHECK(!om_script(&G, "d99") && !om_script(&G, "x"));
}

int main(void)
{
    test_pairs();
    test_deal();
    test_draw();
    test_games();
    printf("oldmaid: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
