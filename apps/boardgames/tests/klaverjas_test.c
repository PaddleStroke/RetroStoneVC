/*
 * Klaverjas: the rules' test (the deal, the cards' points and order, Rotterdam play: following, trumping over a
 * partner, going over, no undertrumping; roem in a trick, the bidding and its forced name, made, nat and pit, the 16
 * deals, undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/klaverjas.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static kvgame G, H;
static kvpos *P = &G.pos;

#define C(s, r) CARD(SUIT_##s, r)

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

/* a deal set by hand, trumps named by `taker`, the trick so far led by `leader` */
static void set_play(const char *const *hs, int taker, int trump, int leader, const char *trick)
{
    kvj_new(&G, 7);
    for (int q = 0; q < 4; q++) P->t.n[q] = (uint8_t)cards(hs[q], P->t.hand[q]);
    P->taker = (uint8_t)taker;
    P->trump = (uint8_t)trump;
    P->phase = KP_PLAY;
    P->t.leader = (uint8_t)leader;
    P->t.ntrick = (uint8_t)cards(trick, P->t.trick);
    P->turn = (uint8_t)tk4_turn(&P->t);
    G.n = 0;
}

static void test_deal(void)
{
    kvj_new(&G, 42);
    int seen[256] = {0}, total = 0;
    for (int q = 0; q < 4; q++) {
        CHECK(P->t.n[q] == 8);
        for (int i = 0; i < 8; i++) seen[P->t.hand[q][i]]++, total++;
    }
    int once = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) once &= seen[CARD(s, r)] == (r == 1 || r >= 7);
    CHECK(once && total == 32);
    CHECK(P->dealer == 3 && P->turn == 0 && P->phase == KP_BID && P->t.leader == 0);
    kvj_new(&H, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
    /* the points: 152 in the cards whatever the trumps */
    for (int tr = 0; tr < 4; tr++) {
        int sum = 0;
        for (int s = 0; s < 4; s++)
            for (int r = 1; r <= 13; r++)
                if (r == 1 || r >= 7) sum += kvj_points(CARD(s, r), tr);
        CHECK(sum == 152);
    }
    CHECK(kvj_points(C(HEARTS, RANK_JACK), SUIT_HEARTS) == 20 && kvj_points(C(HEARTS, 9), SUIT_HEARTS) == 14 &&
          kvj_points(C(HEARTS, RANK_JACK), SUIT_SPADES) == 2 && kvj_points(C(HEARTS, 9), SUIT_SPADES) == 0);
    /* the order: trumps J 9 A 10 K Q 8 7, the others A 10 K Q J 9 8 7 */
    uint8_t t1[4] = {C(SPADES, RANK_ACE), C(SPADES, 10), C(HEARTS, 7), C(SPADES, RANK_KING)};
    CHECK(kvj_winner(t1, 4, SUIT_HEARTS) == 2 && kvj_winner(t1, 4, SUIT_CLUBS) == 0);
    uint8_t t2[4] = {C(HEARTS, RANK_ACE), C(HEARTS, 9), C(HEARTS, RANK_JACK), C(HEARTS, 10)};
    CHECK(kvj_winner(t2, 4, SUIT_HEARTS) == 2 && kvj_winner(t2, 4, SUIT_SPADES) == 0);
    uint8_t t3[4] = {C(HEARTS, 9), C(HEARTS, RANK_ACE), C(DIAMONDS, RANK_ACE), C(HEARTS, 10)};
    CHECK(kvj_winner(t3, 4, SUIT_HEARTS) == 0);
    /* a hand as shown: trumps first, the strongest first; the colours alternating */
    uint8_t h[6] = {C(SPADES, 7), C(HEARTS, RANK_ACE), C(HEARTS, RANK_JACK), C(CLUBS, 10), C(HEARTS, 9),
                    C(DIAMONDS, RANK_KING)};
    kvj_sort(h, 6, SUIT_HEARTS);
    CHECK(h[0] == C(HEARTS, RANK_JACK) && h[1] == C(HEARTS, 9) && h[2] == C(HEARTS, RANK_ACE));
    CHECK(h[3] == C(SPADES, 7) && h[4] == C(DIAMONDS, RANK_KING) && h[5] == C(CLUBS, 10));
}

static void test_play(void)
{
    /* clubs trumps. Player 1 void in spades, his partner (3) winning: he must still trump (Rotterdams) */
    const char *const A[4] = {"As Ks", "Jc 7c Ah", "Qs 7s", "8h 9h"};
    set_play(A, 0, SUIT_CLUBS, 3, "Ts");        /* 3 led the ten of spades: 0 follows suit */
    CHECK(P->turn == 0 && kvj_can_play(P, C(SPADES, RANK_ACE)) && kvj_can_play(P, C(SPADES, RANK_KING)));
    set_play(A, 0, SUIT_CLUBS, 3, "Ts 7s");     /* 3 led, his partner 1 cannot follow: he trumps all the same */
    CHECK(P->turn == 1 && kvj_can_play(P, C(CLUBS, RANK_JACK)) && kvj_can_play(P, C(CLUBS, 7)) &&
          !kvj_can_play(P, C(HEARTS, RANK_ACE)));
    /* a trump already in the trick: over it if you can */
    const char *const B[4] = {"As", "Jc 8c Ah", "9c Ks", "Qs"};
    set_play(B, 0, SUIT_CLUBS, 3, "Qs Ac");     /* 0 trumped with the ace; 1 holds the jack (over) and the 8 */
    CHECK(P->turn == 1 && kvj_can_play(P, C(CLUBS, RANK_JACK)) && !kvj_can_play(P, C(CLUBS, 8)) &&
          !kvj_can_play(P, C(HEARTS, RANK_ACE)));
    /* unable to go over: a card of another suit, not a lower trump */
    const char *const D[4] = {"As", "8c 7c Ah", "Kd", "Qs"};
    set_play(D, 0, SUIT_CLUBS, 3, "Qs Jc");
    CHECK(P->turn == 1 && kvj_can_play(P, C(HEARTS, RANK_ACE)) && !kvj_can_play(P, C(CLUBS, 8)));
    const char *const E[4] = {"As", "8c 7c", "Kd", "Qs"};   /* only lower trumps: undertrump */
    set_play(E, 0, SUIT_CLUBS, 3, "Qs Jc");
    CHECK(kvj_can_play(P, C(CLUBS, 8)) && kvj_can_play(P, C(CLUBS, 7)));
    /* trumps led: higher if you can, any trump else; no trump: anything */
    const char *const F[4] = {"Jc", "9c 8c Ah", "Kd As", "Qs"};
    set_play(F, 0, SUIT_CLUBS, 0, "Ac");
    CHECK(P->turn == 1 && kvj_can_play(P, C(CLUBS, 9)) && !kvj_can_play(P, C(CLUBS, 8)) &&
          !kvj_can_play(P, C(HEARTS, RANK_ACE)));
    set_play(F, 0, SUIT_CLUBS, 1, "9c");
    CHECK(P->turn == 2 && kvj_can_play(P, C(DIAMONDS, RANK_KING)) && kvj_can_play(P, C(SPADES, RANK_ACE)));
    const char *const G2[4] = {"Jc", "7c 8c Ah", "Kd As", "Qs"};
    set_play(G2, 0, SUIT_CLUBS, 0, "9c");
    CHECK(kvj_can_play(P, C(CLUBS, 7)) && kvj_can_play(P, C(CLUBS, 8)) && !kvj_can_play(P, C(HEARTS, RANK_ACE)));
    /* follow suit when you can */
    const char *const H2[4] = {"Jc", "7s Ac", "Kd", "Qs"};
    set_play(H2, 0, SUIT_CLUBS, 0, "Ts");
    CHECK(kvj_can_play(P, C(SPADES, 7)) && !kvj_can_play(P, C(CLUBS, RANK_ACE)));
}

static void test_roem(void)
{
    uint8_t t[4];
    cards("7h 8h 9h As", t);
    CHECK(kvj_roem(t, 4, SUIT_CLUBS) == 20);
    cards("7h 8h 9h Th", t);
    CHECK(kvj_roem(t, 4, SUIT_CLUBS) == 50);
    cards("Qc Kc 7s 8d", t);
    CHECK(kvj_roem(t, 4, SUIT_CLUBS) == 20 && kvj_roem(t, 4, SUIT_SPADES) == 0);   /* stuk: trumps only */
    cards("Jc Qc Kc 7s", t);
    CHECK(kvj_roem(t, 4, SUIT_CLUBS) == 40);    /* a sequence of three and stuk */
    cards("Jc Qc Kc Ac", t);
    CHECK(kvj_roem(t, 4, SUIT_CLUBS) == 70 && kvj_roem(t, 4, SUIT_HEARTS) == 50);
    cards("Kc Ac 7c 8c", t);                     /* the ace is high: K A 7 8 is no sequence of four */
    CHECK(kvj_roem(t, 4, SUIT_HEARTS) == 0);
    cards("Jc Jh Js Jd", t);
    CHECK(kvj_roem(t, 4, SUIT_CLUBS) == 200);
    cards("Ac Ah As Ad", t);
    CHECK(kvj_roem(t, 4, SUIT_CLUBS) == 100);
    cards("9c 9h 9s 9d", t);
    CHECK(kvj_roem(t, 4, SUIT_CLUBS) == 0);
    cards("Tc Jc Qc 9c", t);
    CHECK(kvj_roem(t, 4, SUIT_HEARTS) == 50);
}

static void test_bidding(void)
{
    kvj_new(&G, 11);
    CHECK(P->turn == 0 && kvj_script(&G, "p p") && P->turn == 2 && P->word[0] == KW_PASS);
    CHECK(kvj_script(&G, "ts") && P->phase == KP_PLAY && P->taker == 2 && P->trump == SUIT_SPADES && P->turn == 0);
    kvj_new(&G, 11);
    CHECK(kvj_script(&G, "p p p p") && kvj_must_name(P) && P->turn == 0 && !kvj_legal(P, KA_PASS));
    CHECK(kvj_script(&G, "th") && P->taker == 0 && P->trump == SUIT_HEARTS && P->t.leader == 0);
    /* the trumps sorted first in every hand */
    int ok = 1;
    for (int q = 0; q < 4; q++)
        for (int i = 1; i < P->t.n[q]; i++)
            ok &= !(CARD_SUIT(P->t.hand[q][i]) == SUIT_HEARTS && CARD_SUIT(P->t.hand[q][i - 1]) != SUIT_HEARTS);
    CHECK(ok);
}

/* a whole deal played by plain actions, and its score */
static void test_scoring(void)
{
    kvj_new(&G, 5);
    CHECK(kvj_script(&G, "tc #") && P->phase == KP_DEAL && P->ndeals == 1);
    const kvdeal *r = &P->list[0];
    CHECK(r->eyes[0] + r->eyes[1] == 162);
    int all = 162 + r->roem[0] + r->roem[1], mine = r->eyes[0] + r->roem[0];
    CHECK(r->made == (2 * mine > all));
    if (r->made) CHECK(r->score[0] == mine && r->score[1] == r->eyes[1] + r->roem[1]);
    else CHECK(r->score[0] == 0 && r->score[1] == all);
    CHECK(P->total[0] == r->score[0] && P->total[1] == r->score[1]);
    CHECK(P->turn == 1 && kvj_script(&G, "N") && P->dealer == 0 && P->turn == 1 && P->phase == KP_BID);
    /* pit: every trick to one team, 100 more roem; a nat: everything to the others */
    const char *const A[4] = {"Jc 9c Ac Tc As Ah Ad Kc", "7s 8s 9s 7h 8h 9h 7d 8d", "Qc Ts Ks Th Kh Td Kd 8c",
                              "7c Qs Js Qh Jh Qd Jd 9d"};
    set_play(A, 0, SUIT_CLUBS, 0, "");
    CHECK(kvj_script(&G, "#") && P->phase == KP_DEAL);
    r = &P->list[0];
    CHECK(r->pit == 1 && r->made && r->eyes[0] == 162 && r->roem[0] >= 100 && r->score[1] == 0);
    set_play(A, 1, SUIT_CLUBS, 0, "");          /* the same cards, the other team playing: nat */
    CHECK(kvj_script(&G, "#") && P->list[0].made == 0 && P->list[0].score[1] == 0 &&
          P->list[0].score[0] == 162 + P->list[0].roem[0] + P->list[0].roem[1]);
}

static void test_undo_notation(void)
{
    kvj_new(&G, 9);
    CHECK(kvj_script(&G, "p tc . . ."));
    kvpos keep = G.pos;
    CHECK(kvj_script(&G, ".") && kvj_undo(&G) && !memcmp(&keep, &G.pos, sizeof keep));
    CHECK(!kvj_script(&G, "zz") && !kvj_script(&G, "tx") && !kvj_script(&G, "p"));
    kvj_new(&G, 9);
    CHECK(kvj_script(&G, "!") && P->phase == KP_OVER && P->ndeals == 16 && kvj_winners(P));
}

/* whole games by random legal actions: every deal 162 card points, the totals adding up */
static void test_random(void)
{
    int nat = 0, pit = 0, deals = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        kvj_new(&G, seed);
        uint32_t rs = seed * 7919u;
        int guard = 0;
        while (P->phase != KP_OVER && guard++ < 5000) {
            int acts[16], n = 0;
            if (P->phase == KP_PLAY)
                for (int i = 0; i < P->t.n[P->turn]; i++) {
                    if (kvj_can_play(P, P->t.hand[P->turn][i])) acts[n++] = KA_PLAY | P->t.hand[P->turn][i];
                }
            else if (P->phase == KP_BID) {
                if (kvj_legal(P, KA_PASS)) acts[n++] = KA_PASS, acts[n++] = KA_PASS, acts[n++] = KA_PASS;
                for (int s = 0; s < 4; s++) acts[n++] = KA_TRUMP | s;
            } else
                acts[n++] = KA_NEXT;
            if (!n) break;
            int a = acts[cards_rand(&rs) % (uint32_t)n];
            if (!kvj_play(&G, a)) { CHECK(0); break; }
        }
        CHECK(P->phase == KP_OVER && P->ndeals == 16);
        int tot[2] = {0, 0}, ok = 1;
        for (int d = 0; d < P->ndeals; d++) {
            const kvdeal *r = &P->list[d];
            ok &= r->eyes[0] + r->eyes[1] == 162 && r->roem[0] % 10 == 0 && r->roem[1] % 10 == 0;
            ok &= r->score[0] + r->score[1] == 162 + r->roem[0] + r->roem[1];
            tot[0] += r->score[0], tot[1] += r->score[1];
            nat += !r->made, pit += r->pit != 0, deals++;
        }
        CHECK(ok && tot[0] == P->total[0] && tot[1] == P->total[1]);
    }
    printf("  300 random games, %d deals: %d nat, %d pit\n", deals, nat, pit);
}

int main(void)
{
    test_deal();
    test_play();
    test_roem();
    test_bidding();
    test_scoring();
    test_undo_notation();
    test_random();
    printf("klaverjas: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
