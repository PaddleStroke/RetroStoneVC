/*
 * Crazy Eights and Last Card: the rules' test (the deal, matching, the eights and wilds, drawing, the action cards,
 * the wild draw four's limit, "last card" and the catch, scoring, undo's floor, whole games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/crazyeights.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static cgame G, H;
static cpos *P = &G.pos;

/* every card of the pack exactly once, wherever it is */
static int whole(const cpos *p)
{
    uint8_t pack[CARDS_MAX];
    int n = cards_pack(p->variant == CE_LAST ? PACK_ACTION : PACK_52, pack), want[256] = {0}, seen[256] = {0}, m = 0;
    for (int i = 0; i < n; i++) want[pack[i]]++;
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++) seen[p->hand[q][i]]++, m++;
    for (int i = 0; i < p->nstock; i++) seen[p->stock[i]]++, m++;
    for (int i = 0; i < p->npile; i++) seen[p->pile[i]]++, m++;
    return m == n && !memcmp(want, seen, sizeof want);
}

static void hand(int pl, const int *cards, int n)
{
    for (int i = 0; i < n; i++) P->hand[pl][i] = (uint8_t)cards[i];
    P->n[pl] = (uint8_t)n;
}

/* a position to try a rule on: the discard's top (and the suit to follow), player 0 to move; the cards set aside
 * stay out of the count */
static void table(int variant, int players, int topcard)
{
    ce_new(&G, variant, players, 99);
    P->pile[P->npile - 1] = (uint8_t)topcard;
    P->suit = (uint8_t)CARD_SUIT(topcard);
    P->turn = 0;
}

static void test_deal(void)
{
    ce_new(&G, CE_EIGHTS, 2, 42);
    ce_new(&H, CE_EIGHTS, 2, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(P->n[0] == 7 && P->n[1] == 7 && P->nstock == 52 - 14 - 1 && P->npile == 1 && P->turn == 0);
    CHECK(whole(P) && CARD_RANK(P->pile[0]) != 8 && P->suit == CARD_SUIT(P->pile[0]));
    ce_new(&G, CE_EIGHTS, 4, 42);
    CHECK(P->n[0] == 5 && P->n[3] == 5 && whole(P));
    ce_new(&G, CE_LAST, 6, 7);
    CHECK(P->n[5] == 7 && P->nstock == 108 - 42 - 1 && whole(P));
    CHECK(CARD_SUIT(P->pile[0]) != SUIT_JOKER && CARD_RANK(P->pile[0]) <= 9);
    int sorted = 1;                             /* the hands are sorted: wilds last */
    for (int i = 1; i < P->n[0]; i++)
        if (CARD_SUIT(P->hand[0][i - 1]) == SUIT_JOKER && CARD_SUIT(P->hand[0][i]) != SUIT_JOKER) sorted = 0;
    CHECK(sorted);
    for (int s = 1; s < 200; s++) {             /* the starter is never special */
        ce_new(&G, s & 1 ? CE_LAST : CE_EIGHTS, 2 + s % 5, (uint32_t)s);
        int c = P->pile[0];
        if (s & 1) CHECK(CARD_SUIT(c) < 4 && CARD_RANK(c) <= 9);
        else CHECK(CARD_RANK(c) != 8);
    }
}

static void test_eights(void)
{
    table(CE_EIGHTS, 3, CARD(SUIT_HEARTS, 5));
    const int h[] = {CARD(SUIT_SPADES, 5), CARD(SUIT_SPADES, 9), CARD(SUIT_HEARTS, 12), CARD(SUIT_CLUBS, 8)};
    hand(0, h, 4);
    CHECK(ce_playable(P, CARD(SUIT_SPADES, 5)) && !ce_playable(P, CARD(SUIT_SPADES, 9)));
    CHECK(ce_playable(P, CARD(SUIT_HEARTS, 12)) && ce_playable(P, CARD(SUIT_CLUBS, 8)));
    CHECK(!ce_legal(P, CA_DRAW) && !ce_legal(P, CA_PASS) && !ce_legal(P, CA_CALL));   /* able: no drawing */
    CHECK(ce_play(&G, CA_PLAY | CARD(SUIT_CLUBS, 8)) && P->phase == CP_CHOOSE && P->turn == 0);
    table(CE_EIGHTS, 3, CARD(SUIT_HEARTS, 5));
    hand(0, h, 4);
    CHECK(ce_play(&G, CA_PLAY | CARD(SUIT_CLUBS, 8)));
    CHECK(!ce_legal(P, CA_PLAY | CARD(SUIT_SPADES, 5)) && ce_legal(P, CA_CHOOSE | SUIT_DIAMONDS));
    CHECK(ce_play(&G, CA_CHOOSE | SUIT_DIAMONDS) && P->suit == SUIT_DIAMONDS && P->turn == 1 && P->phase == CP_PLAY);
    const int h1[] = {CARD(SUIT_SPADES, 8 - 1), CARD(SUIT_DIAMONDS, 2)};
    hand(1, h1, 2);
    CHECK(!ce_playable(P, CARD(SUIT_SPADES, 7)) && ce_playable(P, CARD(SUIT_DIAMONDS, 2)));
    /* nothing to play: draw, one at a time, until able */
    const int h2[] = {CARD(SUIT_SPADES, 7), CARD(SUIT_CLUBS, 2)};
    hand(1, h2, 2);
    P->stock[P->nstock - 1] = CARD(SUIT_CLUBS, 3);
    P->stock[P->nstock - 2] = CARD(SUIT_DIAMONDS, 10);
    CHECK(ce_legal(P, CA_DRAW) && !ce_legal(P, CA_PASS));
    CHECK(ce_play(&G, CA_DRAW) && P->n[1] == 3 && P->turn == 1 && ce_legal(P, CA_DRAW));
    CHECK(ce_play(&G, CA_DRAW) && P->n[1] == 4 && !ce_legal(P, CA_DRAW) && ce_playable(P, CARD(SUIT_DIAMONDS, 10)));
    CHECK(ce_undo(&G) == 0);                    /* a card drawn: no going back */
    /* the rank matches too */
    table(CE_EIGHTS, 2, CARD(SUIT_HEARTS, 5));
    const int h3[] = {CARD(SUIT_CLUBS, 5), CARD(SUIT_CLUBS, 6)};
    hand(0, h3, 2);
    CHECK(ce_play(&G, CA_PLAY | CARD(SUIT_CLUBS, 5)) && P->suit == SUIT_CLUBS && P->turn == 1);
    /* points: eight 50, courts 10, ace 1 */
    CHECK(ce_points(P, CARD(0, 8)) == 50 && ce_points(P, CARD(1, 13)) == 10 && ce_points(P, CARD(2, 1)) == 1 &&
          ce_points(P, CARD(3, 7)) == 7);
}

static void test_actions(void)
{
    /* skip, three players: player 1 misses their turn */
    table(CE_LAST, 3, CARD(1, 4));
    const int h[] = {CARD(1, ACTION_SKIP), CARD(1, ACTION_REVERSE), CARD(1, ACTION_DRAW2), CARD(2, 4), CARD(3, 9),
                     ACTION_WILD, ACTION_WILD4};
    hand(0, h, 7);
    CHECK(!ce_playable(P, CARD(3, 9)) && ce_playable(P, CARD(2, 4)) && ce_playable(P, ACTION_WILD));
    CHECK(!ce_playable(P, ACTION_WILD4));       /* it holds the colour to follow */
    CHECK(ce_play(&G, CA_PLAY | CARD(1, ACTION_SKIP)) && P->turn == 2);
    /* reverse, three players: back the other way */
    table(CE_LAST, 3, CARD(1, 4));
    hand(0, h, 7);
    CHECK(ce_play(&G, CA_PLAY | CARD(1, ACTION_REVERSE)) && P->dir == 1 && P->turn == 2);
    CHECK(ce_next(P, 2, 1) == 1);
    /* reverse, two players: a skip */
    table(CE_LAST, 2, CARD(1, 4));
    hand(0, h, 7);
    CHECK(ce_play(&G, CA_PLAY | CARD(1, ACTION_REVERSE)) && P->turn == 0);
    /* draw two: the next player draws 2 and is skipped; it does not stack */
    table(CE_LAST, 3, CARD(1, 4));
    hand(0, h, 7);
    int n1 = P->n[1];
    CHECK(ce_play(&G, CA_PLAY | CARD(1, ACTION_DRAW2)));
    CHECK(P->n[1] == n1 + 2 && P->turn == 2);
    const int h2[] = {CARD(0, ACTION_DRAW2), CARD(3, 3)};
    hand(2, h2, 2);
    CHECK(ce_playable(P, CARD(0, ACTION_DRAW2)));   /* the symbol matches, but... */
    CHECK(ce_play(&G, CA_PLAY | CARD(0, ACTION_DRAW2)) && P->n[0] == 6 + 2 && P->n[1] == n1 + 2);   /* ...only the
                                                                                                   * next draws */
    /* wild: name a colour; wild draw four with no card of the colour */
    table(CE_LAST, 3, CARD(1, 4));
    const int w[] = {CARD(2, 5), ACTION_WILD4};
    hand(0, w, 2);
    CHECK(ce_playable(P, ACTION_WILD4));
    n1 = P->n[1];
    CHECK(ce_play(&G, CA_PLAY | ACTION_WILD4) && P->phase == CP_CHOOSE && P->n[1] == n1);
    CHECK(ce_play(&G, CA_CHOOSE | 3) && P->suit == 3 && P->n[1] == n1 + 4 && P->turn == 2);
    CHECK(P->exposed == 0);                     /* one card left, not called */
    /* after a wild, the colour only (not the wild's "rank") */
    const int c2[] = {CARD(0, 0), CARD(3, 1)};
    hand(2, c2, 2);
    CHECK(!ce_playable(P, CARD(0, 0)) && ce_playable(P, CARD(3, 1)));
    /* points */
    CHECK(ce_points(P, ACTION_WILD) == 50 && ce_points(P, CARD(0, ACTION_SKIP)) == 20 && ce_points(P, CARD(1, 7)) == 7);
}

static void test_draw_last(void)
{
    table(CE_LAST, 3, CARD(1, 4));
    const int h[] = {CARD(2, 7), CARD(3, 8), CARD(1, 9)};
    hand(0, h, 3);
    CHECK(ce_legal(P, CA_DRAW) && !ce_legal(P, CA_PASS));   /* drawing instead of playing: allowed */
    P->stock[P->nstock - 1] = CARD(1, 2);
    CHECK(ce_play(&G, CA_DRAW) && P->drawn == CARD(1, 2) && P->turn == 0);
    CHECK(!ce_playable(P, CARD(1, 9)) && ce_playable(P, CARD(1, 2)) && ce_legal(P, CA_PASS) && !ce_legal(P, CA_DRAW));
    CHECK(ce_play(&G, CA_PASS) && P->turn == 1 && P->n[0] == 4);
    /* a card drawn that cannot be played: the turn ends */
    table(CE_LAST, 3, CARD(1, 4));
    hand(0, h, 3);
    P->stock[P->nstock - 1] = CARD(0, 2);
    CHECK(ce_play(&G, CA_DRAW) && P->turn == 1 && P->n[0] == 4);
}

static void test_last_card(void)
{
    /* forgotten: the next player catches before playing, 2 cards */
    table(CE_LAST, 3, CARD(1, 4));
    const int h[] = {CARD(1, 5), CARD(2, 6)};
    hand(0, h, 2);
    CHECK(ce_legal(P, CA_CALL));
    CHECK(ce_play(&G, CA_PLAY | CARD(1, 5)) && P->n[0] == 1 && P->exposed == 0 && P->turn == 1);
    CHECK(ce_legal(P, CA_CATCH));
    CHECK(ce_play(&G, CA_CATCH) && P->n[0] == 3 && P->exposed == CE_NONE && P->turn == 1 && !ce_legal(P, CA_CATCH));
    /* too late once the next player has acted */
    table(CE_LAST, 3, CARD(1, 4));
    hand(0, h, 2);
    CHECK(ce_play(&G, CA_PLAY | CARD(1, 5)));
    const int h1[] = {CARD(1, 7), CARD(3, 3), CARD(3, 4)};
    hand(1, h1, 3);
    CHECK(ce_play(&G, CA_PLAY | CARD(1, 7)) && P->turn == 2 && !ce_legal(P, CA_CATCH) && P->exposed == CE_NONE);
    /* called: safe */
    table(CE_LAST, 3, CARD(1, 4));
    hand(0, h, 2);
    CHECK(ce_play(&G, CA_CALL) && P->called[0] && !ce_legal(P, CA_CALL));
    CHECK(ce_play(&G, CA_PLAY | CARD(1, 5)) && P->exposed == CE_NONE && !ce_legal(P, CA_CATCH));
    /* undo within the turn (a call, an eight before naming the suit), never past a turn or a card drawn */
    for (uint32_t s = 1; s < 60; s++) {
        ce_new(&G, CE_LAST, 3, s);
        while (P->phase == CP_PLAY && !ce_legal(P, CA_CALL)) ce_play(&G, ce_auto(P));
        if (P->phase != CP_PLAY) continue;
        int n = G.n;
        CHECK(ce_play(&G, CA_CALL) && ce_undo(&G) && G.n == n && !P->called[P->turn] && ce_undo(&G) == 0);
        break;
    }
    /* crazy eights has no call */
    table(CE_EIGHTS, 3, CARD(1, 4));
    const int e[] = {CARD(1, 5), CARD(2, 6)};
    hand(0, e, 2);
    CHECK(!ce_legal(P, CA_CALL) && ce_play(&G, CA_PLAY | CARD(1, 5)) && !ce_legal(P, CA_CATCH));
}

static void test_scoring(void)
{
    table(CE_EIGHTS, 3, CARD(1, 4));
    const int h[] = {CARD(1, 5)}, a[] = {CARD(0, 8), CARD(2, 13)}, b[] = {CARD(3, 1), CARD(3, 9)};
    hand(0, h, 1);
    hand(1, a, 2);
    hand(2, b, 2);
    CHECK(ce_play(&G, CA_PLAY | CARD(1, 5)) && P->phase == CP_ROUND && P->winner == 0);
    CHECK(P->gain == 50 + 10 + 1 + 9 && P->score[0] == 70 && P->left[1] == 60 && P->left[2] == 10);
    CHECK(ce_play(&G, CA_NEXT) && P->phase == CP_PLAY && P->round == 1 && P->turn == 1 && P->score[0] == 70);
    /* last card: a draw two played last still counts */
    table(CE_LAST, 2, CARD(1, 4));
    const int l[] = {CARD(1, ACTION_DRAW2)}, m[] = {CARD(0, 3)};
    hand(0, l, 1);
    hand(1, m, 1);
    P->stock[P->nstock - 1] = CARD(0, 9);
    P->stock[P->nstock - 2] = ACTION_WILD;
    CHECK(ce_play(&G, CA_PLAY | CARD(1, ACTION_DRAW2)) && P->phase == CP_ROUND && P->gain == 3 + 9 + 50);
    /* the goal: 500 */
    table(CE_LAST, 2, CARD(1, 4));
    hand(0, l, 1);
    P->score[0] = 490;
    CHECK(ce_play(&G, CA_PLAY | CARD(1, ACTION_DRAW2)) && P->phase == CP_OVER && ce_winner(P) == 0);
}

/* whole games, from seeds: the pack stays whole, every game ends */
static void test_games(void)
{
    for (int v = 0; v < CE_VARIANTS; v++)
        for (int np = 2; np <= 6; np++) {
            int ok = 1, rounds = 0, actions = 0, catches = 0;
            for (uint32_t seed = 1; seed <= 6; seed++) {
                ce_new(&G, v, np, seed * 7919u + (uint32_t)np);
                while (P->phase != CP_OVER && G.n < CE_MAX_ACTS - 1) {
                    int a = ce_auto(P);
                    if (CA_KIND(a) == CA_CALL && G.n % 3 == 0)    /* forgets to call, now and then */
                        for (int i = 0; i < P->n[P->turn]; i++)
                            if (ce_playable(P, P->hand[P->turn][i])) { a = CA_PLAY | P->hand[P->turn][i]; break; }
                    if (CA_KIND(a) == CA_CATCH) catches++;
                    if (!ce_play(&G, a)) { ok = 0; break; }
                    if (CA_KIND(a) == CA_NEXT) rounds++;
                    if (!whole(P)) { ok = 0; break; }
                }
                actions += G.n;
                ok &= P->phase == CP_OVER && ce_winner(P) >= 0 && P->score[ce_winner(P)] >= ce_goal(P);
            }
            CHECK(ok);
            printf("ok   %s, %d players: %d rounds, %d actions, %d caught in 6 games\n",
                   v ? "last card" : "crazy eights", np, rounds + 6, actions, catches);
        }
    /* the script: the same game */
    ce_new(&G, CE_LAST, 4, 5);
    ce_new(&H, CE_LAST, 4, 5);
    CHECK(ce_script(&G, "*"));
    for (int i = 0; i < G.n; i++) ce_play(&H, G.act[i]);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && G.pos.phase == CP_OVER);
    ce_new(&G, CE_EIGHTS, 2, 5);
    CHECK(!ce_script(&G, "p99") && !ce_script(&G, "z"));
}

int main(void)
{
    test_deal();
    test_eights();
    test_actions();
    test_draw_last();
    test_last_card();
    test_scoring();
    test_games();
    printf("crazyeights: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
