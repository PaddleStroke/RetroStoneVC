/*
 * Euchre: the rules' test (the 24-card deal, the bowers, following suit with the left bower, ordering up and the
 * dealer's card, naming a suit, stick the dealer, going alone and the player sitting out, the points: 1, a march 2, a
 * lone march 4, euchred 2; the end at 10, undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/euchre.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static egame G, H;
static epos *P = &G.pos;

#define C(s, r) CARD(SUIT_##s, r)
#define J(s) CARD(SUIT_##s, RANK_JACK)

static void test_deal(void)
{
    euchre_new(&G, 42);
    int seen[256] = {0}, total = 0;
    for (int pl = 0; pl < 4; pl++) {
        CHECK(P->n[pl] == 5);
        for (int i = 0; i < 5; i++) seen[P->hand[pl][i]]++, total++;
    }
    for (int i = 0; i < 4; i++) seen[P->kitty[i]]++, total++;
    int once = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) once &= seen[CARD(s, r)] == (r == 1 || r >= 9);
    CHECK(once && total == 24 && P->up == P->kitty[0]);
    CHECK(P->dealer == 3 && P->turn == 0 && P->phase == EP_ROUND1);
    euchre_new(&H, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
}

static void test_cards(void)
{
    /* hearts trumps: the right bower, the left bower (the jack of diamonds), then A K Q 10 9 */
    CHECK(euchre_power(J(HEARTS), SUIT_HEARTS) > euchre_power(J(DIAMONDS), SUIT_HEARTS));
    CHECK(euchre_power(J(DIAMONDS), SUIT_HEARTS) > euchre_power(C(HEARTS, RANK_ACE), SUIT_HEARTS));
    CHECK(euchre_suit(J(DIAMONDS), SUIT_HEARTS) == SUIT_HEARTS && euchre_suit(J(CLUBS), SUIT_HEARTS) == SUIT_CLUBS);
    CHECK(euchre_suit(J(SPADES), SUIT_CLUBS) == SUIT_CLUBS && euchre_suit(J(DIAMONDS), SUIT_DIAMONDS) == SUIT_DIAMONDS);
    uint8_t t1[4] = {C(DIAMONDS, RANK_ACE), J(DIAMONDS), C(DIAMONDS, RANK_KING), C(HEARTS, 9)};
    CHECK(euchre_trick_best(t1, 4, SUIT_HEARTS) == 1);   /* the left bower is no diamond: a trump, over the 9 */
    CHECK(euchre_trick_best(t1, 3, SUIT_CLUBS) == 0 && euchre_trick_best(t1 + 2, 2, SUIT_HEARTS) == 1);
    CHECK(euchre_trick_best(t1, 4, SUIT_DIAMONDS) == 1 && euchre_trick_best(t1, 4, SUIT_SPADES) == 0);
    uint8_t t2[3] = {C(CLUBS, 9), J(SPADES), J(CLUBS)};
    CHECK(euchre_trick_best(t2, 3, SUIT_CLUBS) == 2 && euchre_trick_best(t2, 3, SUIT_SPADES) == 1);
    CHECK(euchre_trick_best(t2, 3, SUIT_HEARTS) == 2);   /* no trump: the jack of clubs is high in clubs */
    uint8_t h[5] = {C(HEARTS, 9), J(DIAMONDS), C(SPADES, RANK_ACE), J(HEARTS), C(DIAMONDS, RANK_ACE)};
    euchre_sort(h, 5, SUIT_HEARTS);
    CHECK(h[0] == J(HEARTS) && h[1] == J(DIAMONDS) && h[2] == C(HEARTS, 9) && h[3] == C(SPADES, RANK_ACE) &&
          h[4] == C(DIAMONDS, RANK_ACE));
}

/* hands set by hand (5 each), the turned card, dealer 3: round one, player 0 to speak */
static void set_hands(const char *const *hs, const char *up)
{
    euchre_new(&G, 3);
    for (int pl = 0; pl < 4; pl++) {
        const char *s = hs[pl];
        P->n[pl] = 0;
        while (*s) {
            while (*s == ' ') s++;
            int c = trick_parse(&s);
            if (c == CARD_NONE) break;
            P->hand[pl][P->n[pl]++] = (uint8_t)c;
        }
        euchre_sort(P->hand[pl], P->n[pl], EU_NONE);
    }
    const char *s = up;
    P->up = P->kitty[0] = (uint8_t)trick_parse(&s);
    G.n = 0;
}

static const char *const HANDS[4] = {"Jh Jd Ah Kh Qh", "9s Ts Qs Ks As", "9c Tc Qc Kc Ac", "Jc Js 9d Td Qd"};

static void test_making(void)
{
    /* player 0 orders hearts up; the dealer (3) takes the 9 of hearts and lays a card away */
    set_hands(HANDS, "9h");
    CHECK(!euchre_legal(P, EA_NAME | SUIT_SPADES) && euchre_script(&G, "o"));
    CHECK(P->phase == EP_DISCARD && P->turn == 3 && P->n[3] == 6 && P->maker == 0 && P->trump == SUIT_HEARTS);
    CHECK(!euchre_script(&G, "dAh") && euchre_script(&G, "dQd") && P->laid == C(DIAMONDS, RANK_QUEEN));
    CHECK(P->phase == EP_PLAY && P->turn == 0 && P->leader == 0 && P->n[3] == 5);
    /* every trick to player 0's side: a march, 2 points */
    CHECK(euchre_script(&G, "#") && P->won[0] == 5 && P->pts[0] == 2 && P->score[0] == 2 && P->phase == EP_HAND);
    CHECK(P->turn == 1 && euchre_script(&G, "N") && P->dealer == 0 && P->turn == 1);
    /* round two: all pass, the turned suit may not be named; stick the dealer */
    set_hands(HANDS, "9h");
    CHECK(euchre_script(&G, "p p p p") && P->phase == EP_ROUND2 && P->turned_down && P->turn == 0);
    CHECK(!euchre_legal(P, EA_NAME | SUIT_HEARTS) && !euchre_legal(P, EA_ORDER) && euchre_script(&G, "p p p"));
    CHECK(P->turn == 3 && !euchre_legal(P, EA_PASS) && euchre_script(&G, "nd"));
    CHECK(P->phase == EP_PLAY && P->maker == 3 && P->trump == SUIT_DIAMONDS && P->n[3] == 5 && P->turn == 0);
    /* following suit: hearts led; no heart: anything (the jack of spades too) */
    CHECK(euchre_script(&G, "Ah") && P->turn == 1 && euchre_script(&G, "9s Tc"));
    CHECK(euchre_can_play(P, J(SPADES)) && euchre_script(&G, "9d") && P->last_winner == 3);
    /* the dealer's partner goes alone: the dealer sits out, keeps his five cards and the turned card stays */
    set_hands(HANDS, "9h");
    CHECK(euchre_script(&G, "p oa") && P->maker == 1 && P->alone && P->out == 3 && P->phase == EP_PLAY);
    CHECK(P->n[3] == 5 && P->turn == 0 && euchre_sitting(P, 3));
}

static void test_points(void)
{
    /* alone, every trick: 4 */
    set_hands(HANDS, "9h");
    CHECK(euchre_script(&G, "oa") && P->out == 2 && P->phase == EP_DISCARD && euchre_script(&G, "dQd #"));
    CHECK(P->won[0] == 5 && P->pts[0] == 4 && P->score[0] == 4);
    /* euchred: player 1 names spades, the bowers are with player 0: the makers take fewer than 3, 2 to the others */
    static const char *const e[4] = {"Js Jc As Ah Kh", "9s Ts 9c Tc 9h", "Qs Ks Ac Kc Qc", "9d Td Qd Kd Ad"};
    set_hands(e, "Jh");
    CHECK(euchre_script(&G, "p p p p p ns #") && P->maker == 1);
    CHECK(P->won[1] < 3 && P->pts[0] == 2 && P->pts[1] == 0 && P->score[0] == 2);
    /* 3 tricks: 1 point */
    static const char *const f[4] = {"Jh Ah Kh 9c Tc", "Ac Kc Qc 9d Td", "9s Ts Qs Ks As", "Jc Js Qd Kd Ad"};
    set_hands(f, "9h");
    CHECK(euchre_script(&G, "o dQd #") && P->won[0] == 3 && P->pts[0] == 1);
    /* the end at 10 */
    set_hands(HANDS, "9h");
    P->score[0] = 8;
    CHECK(euchre_script(&G, "o dQd #") && P->phase == EP_OVER && euchre_winner(P) == 0 && !euchre_legal(P, EA_NEXT));
}

static void test_undo(void)
{
    euchre_new(&G, 77);
    CHECK(euchre_script(&G, ". . . . . ."));
    int n = G.n;
    epos before = G.pos;
    CHECK(euchre_script(&G, ". . ."));
    CHECK(euchre_undo(&G) && euchre_undo(&G) && euchre_undo(&G) && G.n == n && !memcmp(&G.pos, &before, sizeof before));
    while (euchre_undo(&G)) {}
    euchre_new(&H, 77);
    CHECK(G.n == 0 && !memcmp(&G.pos, &H.pos, sizeof H.pos));
}

/* whole games: random legal actions */
static void test_random_games(void)
{
    int games = 0, hands = 0, alone = 0, euchred = 0, marches = 0, round2 = 0;
    for (uint32_t seed = 1; seed <= 400; seed++) {
        euchre_new(&G, seed * 15485863u);
        uint32_t r = seed;
        int ok = 1, steps = 0, plain = seed % 3 == 0;
        while (P->phase != EP_OVER && ok && steps++ < 100000) {
            if (P->phase == EP_HAND) {
                int m = P->maker & 1;
                ok &= P->won[0] + P->won[1] == 5 && (P->pts[0] == 0) != (P->pts[1] == 0);
                hands++, alone += P->alone, euchred += P->won[m] < 3, marches += P->won[m] == 5;
                round2 += P->turned_down;
                ok &= euchre_play(&G, EA_NEXT);
                continue;
            }
            int cand[64], k = 0;
            if (plain) cand[k++] = euchre_auto(P);
            else {
                static const int base[] = {EA_ORDER, EA_ORDER | EA_ALONE, EA_PASS, EA_PASS, EA_PASS, EA_PASS};
                for (int i = 0; i < 6; i++) cand[k++] = base[i];
                for (int s = 0; s < 4; s++) cand[k++] = EA_NAME | s, cand[k++] = EA_NAME | s | EA_ALONE;
                for (int i = 0; i < P->n[P->turn]; i++)
                    cand[k++] = EA_PLAY | P->hand[P->turn][i], cand[k++] = EA_DISCARD | P->hand[P->turn][i];
            }
            int m = 0;
            for (int i = 0; i < k; i++)
                if (euchre_legal(P, cand[i])) cand[m++] = cand[i];
            ok &= m > 0;
            if (!m) break;
            if (P->phase == EP_PLAY) ok &= P->turn != P->out;
            ok &= euchre_play(&G, cand[cards_rand(&r) % (uint32_t)m]);
        }
        ok &= P->phase == EP_OVER && (P->score[0] >= 10) != (P->score[1] >= 10);
        ok &= P->score[euchre_winner(P)] >= 10;
        CHECK(ok);
        if (!ok) {
            printf("  seed %u: %d %d, phase %d, %d actions\n", seed, P->score[0], P->score[1], P->phase, G.n);
            break;
        }
        games++;
    }
    CHECK(games == 400);
    printf("  %d random games, %d hands: %d alone, %d marches, %d euchred, %d in round two\n", games, hands, alone,
           marches, euchred, round2);
}

int main(void)
{
    test_deal();
    test_cards();
    test_making();
    test_points();
    test_undo();
    test_random_games();
    printf("euchre: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
