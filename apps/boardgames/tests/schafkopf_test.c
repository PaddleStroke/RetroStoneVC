/*
 * Schafkopf: the rules' test (the deal, the trumps of each game and their order, following, the bidding and its
 * highest game, the called ace's rules (sought, never thrown away, davonlaufen), schneider, schwarz, Laufende and the
 * tariffs, Ramsch and Durchmarsch, a deal thrown in, undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/schafkopf.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static sfgame G, H;
static sfpos *P = &G.pos;

#define C(s, r) CARD(SUIT_##s, r)
#define O(s) CARD(SUIT_##s, RANK_QUEEN)
#define U(s) CARD(SUIT_##s, RANK_JACK)

static int cards(const char *s, uint8_t *out)
{
    int n = 0;
    for (;;) {
        while (*s == ' ') s++;
        int c = trick_parse(&s);
        if (c == CARD_NONE) return n;
        out[n++] = (uint8_t)c;
    }
}

/* the hands set by hand for a new deal (the bidding to come) */
static void set_hands(const char *const *hs, int variant)
{
    sfk_new(&G, variant, 7);
    for (int q = 0; q < 4; q++) {
        P->t.n[q] = (uint8_t)cards(hs[q], P->t.hand[q]);
        memcpy(P->dealt[q], P->t.hand[q], SFK_HAND);
    }
    G.n = 0;
}

static void test_cards(void)
{
    sfk_new(&G, SFK_PLAIN, 42);
    int seen[256] = {0}, total = 0;
    for (int q = 0; q < 4; q++) {
        CHECK(P->t.n[q] == 8);
        for (int i = 0; i < 8; i++) seen[P->t.hand[q][i]]++, total++;
    }
    int once = 1, pts = 0;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) {
            once &= seen[CARD(s, r)] == (r == 1 || r >= 7);
            if (r == 1 || r >= 7) pts += sfk_points(CARD(s, r));
        }
    CHECK(once && total == 32 && pts == 120 && P->dealer == 3 && P->turn == 0 && P->phase == SFP_BID);
    sfk_new(&H, SFK_PLAIN, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
    /* the trumps: Obers, Unters, hearts; a Solo's suit; the Wenz's Unters alone */
    CHECK(sfk_trump(O(DIAMONDS), SFG_RUF, 0) && sfk_trump(U(DIAMONDS), SFG_RUF, 0) &&
          sfk_trump(C(HEARTS, 7), SFG_RUF, 0) && !sfk_trump(C(CLUBS, RANK_ACE), SFG_RUF, SUIT_CLUBS));
    CHECK(sfk_trump(C(CLUBS, 7), SFG_SOLO, SUIT_CLUBS) && !sfk_trump(C(HEARTS, 7), SFG_SOLO, SUIT_CLUBS));
    CHECK(sfk_trump(U(HEARTS), SFG_WENZ, 0) && !sfk_trump(O(CLUBS), SFG_WENZ, 0) && !sfk_trump(C(HEARTS, 7), SFG_WENZ, 0));
    uint8_t t[4];
    cards("Ah Jd Qd Qc", t);                    /* the club Ober over all */
    CHECK(sfk_winner(t, 4, SFG_RUF, 0) == 3);
    cards("Jd Js Ah Th", t);
    CHECK(sfk_winner(t, 4, SFG_RUF, 0) == 1);
    cards("As Ts Ks 7h", t);
    CHECK(sfk_winner(t, 4, SFG_RUF, 0) == 3 && sfk_winner(t, 4, SFG_WENZ, 0) == 0);
    cards("Ks Qs Ts 9s", t);                    /* Wenz: the Ober in its suit, A 10 K O 9 */
    CHECK(sfk_winner(t, 4, SFG_WENZ, 0) == 2 && sfk_winner(t, 4, SFG_RUF, 0) == 1);
    cards("7c Ah Kc Jh", t);
    CHECK(sfk_winner(t, 4, SFG_SOLO, SUIT_CLUBS) == 3 && sfk_winner(t, 4, SFG_SOLO, SUIT_DIAMONDS) == 3 &&
          sfk_winner(t, 4, SFG_WENZ, 0) == 3);
    /* a hand as shown: trumps first */
    uint8_t h[5] = {C(CLUBS, RANK_ACE), C(HEARTS, 7), O(DIAMONDS), U(CLUBS), C(SPADES, 9)};
    sfk_sort(h, 5, SFG_RUF, 0);
    CHECK(h[0] == O(DIAMONDS) && h[1] == U(CLUBS) && h[2] == C(HEARTS, 7) && h[3] == C(CLUBS, RANK_ACE));
}

static const char *const HANDS[4] = {"Qc Qs Jc Ah Th 7c Kd 8s",   /* 0: a Rufspiel on clubs */
                                     "Qh Qd Js Kh 9h Ac 8c 9s",   /* 1: the club ace: partner */
                                     "Jh Jd 8h 7h Tc Kc 9c As",
                                     "Ts Ks 7s Ad Td 9d 8d 7d"};

static void test_bidding(void)
{
    set_hands(HANDS, SFK_PLAIN);
    CHECK(sfk_game_ok(P, SFG(SFG_RUF, SUIT_CLUBS)) && !sfk_game_ok(P, SFG(SFG_RUF, SUIT_HEARTS)) &&
          sfk_game_ok(P, SFG(SFG_RUF, SUIT_SPADES)) && sfk_game_ok(P, SFG(SFG_RUF, SUIT_DIAMONDS)));
    CHECK(sfk_script(&G, "rc") && P->word[0] == SFW_PLAY && P->turn == 1 && !sfk_game_ok(P, SFG(SFG_RUF, SUIT_CLUBS)));
    CHECK(sfk_script(&G, "p p p") && P->phase == SFP_PLAY && P->declarer == 0 && P->kind == SFG_RUF &&
          P->suit == SUIT_CLUBS && P->partner == 1 && P->turn == 0);
    /* a later Wenz beats the Rufspiel; an equal game: the first */
    set_hands(HANDS, SFK_PLAIN);
    CHECK(sfk_script(&G, "rc p w p") && P->declarer == 2 && P->kind == SFG_WENZ && P->partner == SFK_NONE);
    set_hands(HANDS, SFK_PLAIN);
    CHECK(sfk_script(&G, "sh sc p p") && P->declarer == 0 && P->kind == SFG_SOLO && P->suit == SUIT_HEARTS);
    /* all pass: thrown in, or a Ramsch */
    set_hands(HANDS, SFK_PLAIN);
    CHECK(sfk_script(&G, "p p p p") && P->phase == SFP_DEAL && P->list[0].declarer == SFK_NONE &&
          P->list[0].kind == SFG_NONE && P->total[0] == 0 && P->ndeals == 1);
    set_hands(HANDS, SFK_RAMSCH);
    CHECK(sfk_script(&G, "p p p p") && P->phase == SFP_PLAY && P->kind == SFG_RAMSCH && P->turn == 0);
}

static void test_called_ace(void)
{
    set_hands(HANDS, SFK_PLAIN);
    CHECK(sfk_script(&G, "rc p p p"));
    /* 0 leads the called suit: 1 must play the ace */
    CHECK(sfk_script(&G, "7c") && P->searched == 1 && P->turn == 1);
    CHECK(sfk_can_play(P, C(CLUBS, RANK_ACE)) && !sfk_can_play(P, C(CLUBS, 8)) && !sfk_can_play(P, O(HEARTS)));
    CHECK(sfk_script(&G, "Ac") && P->revealed);
    /* before the search: the ace never thrown away on another suit */
    set_hands(HANDS, SFK_PLAIN);
    CHECK(sfk_script(&G, "rc p p p 8s"));        /* spades led: 1 follows with the 9 */
    CHECK(sfk_can_play(P, C(SPADES, 9)) && !sfk_can_play(P, C(CLUBS, RANK_ACE)));
    const char *const V[4] = {"Qc Qs Jc Ah Th 7c Kd 9s", "Qh Qd Js Kh 9h Ac 8c 8s", "Jh Jd 8h 7h Tc Kc 9c As",
                              "Ts Ks 7s Ad Td 9d 8d 7d"};
    set_hands(V, SFK_PLAIN);
    CHECK(sfk_script(&G, "rc p p p Kd"));        /* diamonds led: 1 has none, may not throw the ace */
    CHECK(P->turn == 1 && !sfk_can_play(P, C(CLUBS, RANK_ACE)) && sfk_can_play(P, C(CLUBS, 8)));
    /* the partner leading: the called suit only with the ace (three of it: no running away) */
    set_hands(HANDS, SFK_PLAIN);
    CHECK(sfk_script(&G, "rc p p p Ah"));        /* 0 leads trumps */
    CHECK(sfk_script(&G, "Kh 8h Ad") && P->t.leader == 0);
    /* davonlaufen: four of the called suit */
    const char *const D[4] = {"Qc Qs Jc Ah Th 7c Kd 8s", "Ac Tc 9c 8c Qh Qd Js Kh", "Jh Jd 8h 7h Kc 9s 9h As",
                              "Ts Ks 7s Ad Td 9d 8d 7d"};
    set_hands(D, SFK_PLAIN);
    CHECK(sfk_script(&G, "rc p p p Qc Qh Jh Ts"));   /* 0 leads the Ober: 1 takes... */
    CHECK(P->t.leader == 0);
    set_hands(D, SFK_PLAIN);
    P->t.leader = 1;
    CHECK(sfk_script(&G, "rc p p p"));
    P->t.leader = 1;
    P->turn = 1;
    CHECK(sfk_can_play(P, C(CLUBS, 8)) && sfk_can_play(P, C(CLUBS, RANK_ACE)));   /* four clubs: run away */
    CHECK(sfk_script(&G, "8c") && P->searched == 1 && sfk_can_play(P, C(CLUBS, 7)) == 0);
    set_hands(HANDS, SFK_PLAIN);
    CHECK(sfk_script(&G, "rc p p p"));
    P->t.leader = 1;
    P->turn = 1;
    CHECK(!sfk_can_play(P, C(CLUBS, 8)) && sfk_can_play(P, C(CLUBS, RANK_ACE)));   /* two clubs: only the ace */
}

static void test_scoring(void)
{
    /* schneider: the declaring side free with 31, the others already with 30 (as 61 and 60 win) */
    CHECK(sfk_schneider(91, 1) && !sfk_schneider(90, 1) && sfk_schneider(30, 0) && !sfk_schneider(31, 0));
    CHECK(sfk_schneider(120, 1) && sfk_schneider(0, 0) && !sfk_schneider(61, 1) && !sfk_schneider(60, 0));
    /* a Rufspiel played out: card points 120, the side's tariff */
    set_hands(HANDS, SFK_PLAIN);
    CHECK(sfk_script(&G, "rc p p p #") && P->phase == SFP_DEAL);
    const sfdeal *r = &P->list[0];
    int sum = 0;
    for (int q = 0; q < 4; q++) sum += r->eyes[q];
    CHECK(sum == 120 && r->side == r->eyes[0] + r->eyes[1] && r->won == (r->side >= 61));
    CHECK(r->value == 10 + 10 * r->schneider + 10 * r->schwarz + 10 * r->runners);
    CHECK(r->score[0] == r->score[1] && r->score[2] == r->score[3] && r->score[0] == -r->score[2] &&
          abs(r->score[0]) == r->value);
    /* Laufende: 0 and 1 hold both club Obers... the clubs Ober, spades Ober (0), then the hearts Ober (1), diamonds
     * Ober (1), then the club Unter (0), spades Unter (1): six with */
    CHECK(sfk_runners((const uint8_t(*)[SFK_HAND])P->dealt, 3, SFG_RUF, SUIT_CLUBS) == 6);
    CHECK(sfk_runners((const uint8_t(*)[SFK_HAND])P->dealt, 1, SFG_RUF, SUIT_CLUBS) == 2);
    CHECK(sfk_runners((const uint8_t(*)[SFK_HAND])P->dealt, 4, SFG_WENZ, 0) == -2);
    CHECK(r->runners == 6);
    /* a Solo: three times the value for the declarer */
    set_hands(HANDS, SFK_PLAIN);
    CHECK(sfk_script(&G, "p p p sd #"));
    r = &P->list[0];
    CHECK(r->declarer == 3 && r->kind == SFG_SOLO && r->score[3] == -3 * r->score[0] && r->score[0] == r->score[1] &&
          abs(r->score[0]) == r->value && r->value >= 50);
    /* a sure Solo: every trump and ace: schwarz */
    const char *const W[4] = {"Qc Qs Qh Qd Jc Js Jh Jd", "Ah Th Kh 9h 8h 7h As Ts", "Ac Tc Kc 9c 8c 7c Ks 9s",
                              "Ad Td Kd 9d 8d 7d 8s 7s"};
    set_hands(W, SFK_PLAIN);
    CHECK(sfk_script(&G, "sh p p p #"));
    r = &P->list[0];
    CHECK(r->won && r->side == 120 && r->schneider && r->schwarz && r->runners == 8 && r->value == 50 + 20 + 80 &&
          r->score[0] == 3 * 150 && r->score[1] == -150);
    /* the Ramsch: the most card points pays each other 10 */
    set_hands(HANDS, SFK_RAMSCH);
    CHECK(sfk_script(&G, "p p p p #"));
    r = &P->list[0];
    int most = 0, payers = 0, zero = 0;
    for (int q = 0; q < 4; q++) most = r->eyes[q] > most ? r->eyes[q] : most;
    for (int q = 0; q < 4; q++) {
        payers += r->losers >> q & 1;
        zero += r->score[q];
        if (r->losers >> q & 1) CHECK(r->eyes[q] == most);
    }
    CHECK(r->march || (payers >= 1 && zero == 0));
    /* Durchmarsch */
    set_hands(W, SFK_RAMSCH);
    CHECK(sfk_script(&G, "p p p p #"));
    r = &P->list[0];
    CHECK(r->march == 1 && r->score[0] == 150 && r->score[2] == -50 && P->total[0] == 150);
}

static void test_undo_notation(void)
{
    sfk_new(&G, SFK_RAMSCH, 9);
    CHECK(sfk_script(&G, ". . . . . . ."));
    sfpos keep = G.pos;
    CHECK(sfk_script(&G, ".") && sfk_undo(&G) && !memcmp(&keep, &G.pos, sizeof keep));
    CHECK(!sfk_script(&G, "zz") && !sfk_script(&G, "rx"));
    sfk_new(&G, SFK_RAMSCH, 9);
    CHECK(sfk_script(&G, "!") && P->phase == SFP_OVER && P->ndeals == 12 && sfk_winners(P));
}

static void test_random(void)
{
    int games[5] = {0}, won = 0, deals = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        int v = seed & 1;
        sfk_new(&G, v, seed);
        uint32_t rs = seed * 7919u;
        int guard = 0;
        while (P->phase != SFP_OVER && guard++ < 3000) {
            int acts[24], n = 0;
            if (P->phase == SFP_PLAY) {
                for (int i = 0; i < P->t.n[P->turn]; i++)
                    if (sfk_can_play(P, P->t.hand[P->turn][i])) acts[n++] = SFA_PLAY | P->t.hand[P->turn][i];
            } else if (P->phase == SFP_BID) {
                for (int k = 0; k < 6; k++) acts[n++] = SFA_PASS;
                for (int g = 0; g < 64; g++)
                    if (sfk_game_ok(P, g) && cards_rand(&rs) % 3 == 0) acts[n++] = SFA_GAME | g;
            } else
                acts[n++] = SFA_NEXT;
            if (!n) { CHECK(0); break; }
            if (!sfk_play(&G, acts[cards_rand(&rs) % (uint32_t)n])) { CHECK(0); break; }
        }
        CHECK(P->phase == SFP_OVER && P->ndeals == 12);
        int tot[4] = {0}, ok = 1;
        for (int d = 0; d < P->ndeals; d++) {
            const sfdeal *r = &P->list[d];
            int sum = 0, eyes = 0;
            for (int q = 0; q < 4; q++) sum += r->score[q], tot[q] += r->score[q], eyes += r->eyes[q];
            ok &= sum == 0 && (r->kind == SFG_NONE || eyes == 120);
            if (r->kind != SFG_NONE && r->kind != SFG_RAMSCH)
                ok &= r->schneider == (r->won ? r->side >= 91 : r->side <= 30);
            games[r->kind]++, won += r->won, deals++;
        }
        for (int q = 0; q < 4; q++) ok &= tot[q] == P->total[q];
        CHECK(ok);
    }
    printf("  300 random games, %d deals: %d thrown in, %d Rufspiel, %d Wenz, %d Solo, %d Ramsch; %d won\n", deals,
           games[0], games[1], games[2], games[3], games[4], won);
}

int main(void)
{
    test_cards();
    test_bidding();
    test_called_ace();
    test_scoring();
    test_undo_notation();
    test_random();
    printf("schafkopf: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
