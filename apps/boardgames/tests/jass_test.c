/*
 * Jass (the Schieber): the rules' test (the deal, the points in each game, the cards' order, following and trumping,
 * no undertrumping, the Puur kept back; schieben, the weis and its best team, stöck, match, the multipliers, the
 * target reached first, undo, the notation; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/jass.h"
#include "games/tricks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static jsgame G, H;
static jspos *P = &G.pos;

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

/* a deal set by hand in the given game, the trick so far led by `leader` */
static void set_play(const char *const *hs, int mode, int leader, const char *trick)
{
    jas_new(&G, JASS_1000, 7);
    for (int q = 0; q < 4; q++) P->t.n[q] = (uint8_t)cards(hs[q], P->t.hand[q]);
    P->mode = (uint8_t)mode;
    P->chooser = (uint8_t)leader;
    P->phase = JP_PLAY;
    P->t.leader = (uint8_t)leader;
    P->t.ntrick = (uint8_t)cards(trick, P->t.trick);
    P->turn = (uint8_t)tk4_turn(&P->t);
    G.n = 0;
}

static void test_cards(void)
{
    jas_new(&G, JASS_1000, 42);
    int seen[256] = {0}, total = 0;
    for (int q = 0; q < 4; q++) {
        CHECK(P->t.n[q] == 9);
        for (int i = 0; i < 9; i++) seen[P->t.hand[q][i]]++, total++;
    }
    int once = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) once &= seen[CARD(s, r)] == (r == 1 || r >= 6);
    CHECK(once && total == 36 && P->dealer == 3 && P->turn == 0 && P->phase == JP_CHOOSE);
    jas_new(&H, JASS_1000, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos));
    for (int m = 0; m < JM_MODES; m++) {        /* 152 in the cards in every game */
        int sum = 0;
        for (int s = 0; s < 4; s++)
            for (int r = 1; r <= 13; r++)
                if (r == 1 || r >= 6) sum += jas_points(CARD(s, r), m);
        CHECK(sum == 152);
    }
    CHECK(jas_points(C(HEARTS, RANK_JACK), SUIT_HEARTS) == 20 && jas_points(C(HEARTS, 9), SUIT_HEARTS) == 14);
    CHECK(jas_points(C(HEARTS, 8), JM_OBEN) == 8 && jas_points(C(HEARTS, 6), JM_UNDE) == 11 &&
          jas_points(C(HEARTS, RANK_ACE), JM_UNDE) == 0 && jas_points(C(HEARTS, RANK_ACE), JM_OBEN) == 11);
    uint8_t t[4];
    cards("Ah 9h Jh Kh", t);
    CHECK(jas_winner(t, 4, SUIT_HEARTS) == 2 && jas_winner(t, 4, SUIT_SPADES) == 0 && jas_winner(t, 4, JM_OBEN) == 0);
    cards("Ah 6h 7s Kh", t);
    CHECK(jas_winner(t, 4, JM_UNDE) == 1 && jas_winner(t, 4, JM_OBEN) == 0 && jas_winner(t, 4, SUIT_SPADES) == 2);
    cards("9h Ah Kh Qh", t);
    CHECK(jas_winner(t, 4, SUIT_HEARTS) == 0 && jas_winner(t, 4, SUIT_CLUBS) == 1);
    CHECK(jas_mult(JASS_1000, JM_OBEN) == 1 && jas_mult(JASS_2500, SUIT_DIAMONDS) == 1 &&
          jas_mult(JASS_2500, SUIT_HEARTS) == 1 && jas_mult(JASS_2500, SUIT_SPADES) == 2 &&
          jas_mult(JASS_2500, SUIT_CLUBS) == 2 && jas_mult(JASS_2500, JM_OBEN) == 3 && jas_mult(JASS_2500, JM_UNDE) == 3);
    CHECK(jas_target(JASS_1000) == 1000 && jas_target(JASS_2500) == 2500);
}

static void test_play(void)
{
    /* hearts trumps; spades led: follow, or trump instead */
    const char *const A[4] = {"As", "Ks 6h 7c", "Qs", "Js"};
    set_play(A, SUIT_HEARTS, 0, "Ts");
    CHECK(P->turn == 1 && jas_can_play(P, C(SPADES, RANK_KING)) && jas_can_play(P, C(HEARTS, 6)) &&
          !jas_can_play(P, C(CLUBS, 7)));
    /* no undertrumping: a trump already in the trick */
    const char *const B[4] = {"As", "Ks 6h 7c Ah", "Qs", "Js"};
    set_play(B, SUIT_HEARTS, 3, "Ts Kh");
    CHECK(P->turn == 1 && !jas_can_play(P, C(HEARTS, 6)) && jas_can_play(P, C(HEARTS, RANK_ACE)) &&
          jas_can_play(P, C(SPADES, RANK_KING)) && !jas_can_play(P, C(CLUBS, 7)));
    const char *const D[4] = {"As", "6h 7c", "Qs", "Js"};   /* void: anything but a lower trump */
    set_play(D, SUIT_HEARTS, 3, "Ts Kh");
    CHECK(!jas_can_play(P, C(HEARTS, 6)) && jas_can_play(P, C(CLUBS, 7)));
    const char *const E[4] = {"As", "6h 7h", "Qs", "Js"};   /* only trumps left: under it all the same */
    set_play(E, SUIT_HEARTS, 3, "Ts Kh");
    CHECK(jas_can_play(P, C(HEARTS, 6)) && jas_can_play(P, C(HEARTS, 7)));
    /* trumps led: a trump; the Puur alone need not be played */
    const char *const F[4] = {"As", "Jh 7c As", "Qs", "Js"};
    set_play(F, SUIT_HEARTS, 0, "9h");
    CHECK(jas_can_play(P, C(CLUBS, 7)) && jas_can_play(P, C(HEARTS, RANK_JACK)));
    const char *const F2[4] = {"As", "Jh 6h 7c", "Qs", "Js"};
    set_play(F2, SUIT_HEARTS, 0, "9h");
    CHECK(!jas_can_play(P, C(CLUBS, 7)) && jas_can_play(P, C(HEARTS, 6)) && jas_can_play(P, C(HEARTS, RANK_JACK)));
    /* obenabe: follow suit */
    set_play(A, JM_OBEN, 0, "Ts");
    CHECK(jas_can_play(P, C(SPADES, RANK_KING)) && !jas_can_play(P, C(HEARTS, 6)));
}

static void test_weis(void)
{
    uint8_t h[9];
    jsmeld m[JAS_MELDS];
    int k;
    int32_t key, key2;
    int n = cards("6h 7h 8h 9h Ks Qs Js Ac Ad", h);
    CHECK(jas_melds(h, n, SUIT_CLUBS, m, &k, &key) == 70 && k == 2);
    n = cards("Jh Js Jc Jd 9h 9s 9c 9d Th", h);
    CHECK(jas_melds(h, n, SUIT_CLUBS, m, &k, &key) == 370 && k == 3);   /* 9 10 J of hearts too */
    n = cards("6h 7h 8h 9h Th Jh Qh Kh Ah", h);
    CHECK(jas_melds(h, n, SUIT_CLUBS, m, &k, &key) == 300 && k == 1);
    n = cards("6h 7h 8h 6s 7s 8s 6c 7c 8c", h);
    CHECK(jas_melds(h, n, SUIT_CLUBS, m, &k, &key) == 60 && k == 3);
    n = cards("Kh Ks Kc Kd 7s", h);
    CHECK(jas_melds(h, n, SUIT_CLUBS, m, &k, &key) == 100);
    n = cards("Th Jh Qh Kh 7s", h);
    CHECK(jas_melds(h, n, SUIT_CLUBS, m, &k, &key2) == 50 && key > key2);
    /* equal points: four of a kind before a sequence; the higher top; a trump sequence */
    n = cards("Ah Kh Qh Jh Th", h);
    jas_melds(h, n, SUIT_CLUBS, m, &k, &key);
    n = cards("Ks Kh Kc Kd", h);
    jas_melds(h, n, SUIT_CLUBS, m, &k, &key2);
    CHECK(key2 > key);
    n = cards("7h 8h 9h", h);
    jas_melds(h, n, SUIT_CLUBS, m, &k, &key);
    n = cards("7s 8s 9s Ts", h);
    jas_melds(h, n, SUIT_CLUBS, m, &k, &key2);
    CHECK(key2 > key);
    n = cards("7c 8c 9c", h);
    jas_melds(h, n, SUIT_CLUBS, m, &k, &key2);
    CHECK(key2 > key);
    /* a deal: two declare, only the best team scores, all its weis */
    const char *const A[4] = {"6h 7h 8h As Ks Qs Ac Kc Qc", "7s 8s 9s Ts Ad Kd Qd Jd 6c",
                              "9h Th Jh Qh Kh 6s Js Jc 8c", "Ah 6d 7d 8d 9d Td 9c Tc 7c"};
    set_play(A, SUIT_CLUBS, 0, "");
    P->t.leader = 0;
    CHECK(jas_can_weis(P) && jas_script(&G, "w") && P->declared[0] && P->wpts[0] == 60 && P->turn == 0);
    CHECK(!jas_can_weis(P) && jas_script(&G, "As"));
    CHECK(jas_can_weis(P) && jas_script(&G, "w 7s"));   /* 1: 7 8 9 10 of spades 50, A K Q J of diamonds 50 */
    CHECK(P->wpts[1] == 100 && jas_script(&G, "w 6s"));   /* 2: 9 10 J Q K of hearts 100 */
    CHECK(P->wpts[2] == 100 && jas_script(&G, "w Ah"));   /* 3: 6 7 8 9 10 of diamonds 100 */
    CHECK(P->wpts[3] == 100 && P->t.tricks == 1);
    /* the best single meld: 2's hearts and 3's diamonds, 100 each; hearts higher (K over 10): team 0 */
    CHECK(P->weis_team == 0 && P->weis[0] == 160 && P->weis[1] == 0 && P->total[0] == 160);
}

static void test_stock_match(void)
{
    /* clubs trumps: 0 holds the king and queen; plays them both: 20 */
    const char *const A[4] = {"Jc 9c Ac Kc Qc Tc 8c 7c 6c", "6s 7s 8s 9s Ts Js Qs Ks As",
                              "6h 7h 8h 9h Th Jh Qh Kh Ah", "6d 7d 8d 9d Td Jd Qd Kd Ad"};
    set_play(A, SUIT_CLUBS, 0, "");
    P->stock_pl = 0;
    CHECK(jas_script(&G, "Kc #") && P->phase == JP_DEAL && P->stock[0] == 20 && P->stock_done);
    /* the others' weis: nine in sequence each, equal: the first in play (1) wins it for his team, 1's and 3's */
    const jsdeal *r = &P->list[0];
    CHECK(r->match == 1 && r->eyes[0] == 157 && r->eyes[1] == 0 && r->score[0] == 257 + 20);
    CHECK(r->weis[1] == 600 && r->weis[0] == 0 && r->score[1] == 600 && P->total[0] == 277 && P->total[1] == 600);
    /* the multipliers: clubs x2 in the long game; 0's sequence in trumps beats the others': his and 2's count */
    jas_new(&G, JASS_2500, 7);
    for (int q = 0; q < 4; q++) P->t.n[q] = (uint8_t)cards(A[q], P->t.hand[q]);
    CHECK(jas_script(&G, "tc #") && P->list[0].mult == 2 && P->list[0].weis[0] == 600 &&
          P->list[0].score[0] == 2 * (257 + 600 + 20) && P->total[0] == 1754 && P->total[1] == 0);
    /* schieben: the partner chooses, the first player still leads */
    jas_new(&G, JASS_1000, 3);
    CHECK(jas_script(&G, "p") && P->turn == 2 && P->pushed && !jas_legal(P, JA_PUSH));
    CHECK(jas_script(&G, "u") && P->chooser == 2 && P->mode == JM_UNDE && P->turn == 0 && P->t.leader == 0);
}

static void test_end(void)
{
    jas_new(&G, JASS_1000, 5);
    P->total[0] = 990;
    CHECK(jas_script(&G, "#") && P->total[0] >= 1000 && P->phase == JP_OVER && P->first == 0 && jas_winners(P) == 5);
    /* the target reached by weis first wins, whatever the tricks */
    int got = 0;
    for (uint32_t seed = 1; seed < 200 && !got; seed++) {
        jas_new(&G, JASS_1000, seed);
        P->total[0] = 990, P->total[1] = 990;
        if (!jas_script(&G, "#")) break;
        if (P->list[0].weis[1] && P->list[0].eyes[0] > P->list[0].eyes[1]) got = 1;
    }
    CHECK(got && P->first == 1);
}

static void test_undo_notation(void)
{
    jas_new(&G, JASS_1000, 9);
    CHECK(jas_script(&G, "th . . ."));
    jspos keep = G.pos;
    CHECK(jas_script(&G, ".") && jas_undo(&G) && !memcmp(&keep, &G.pos, sizeof keep));
    CHECK(!jas_script(&G, "zz") && !jas_script(&G, "tx") && !jas_script(&G, "p"));
    jas_new(&G, JASS_2500, 9);
    CHECK(jas_script(&G, "!") && P->phase == JP_OVER && jas_winners(P) && P->total[P->first] >= 2500);
}

static void test_random(void)
{
    int weis = 0, stock = 0, match = 0, deals = 0, pushed = 0;
    for (uint32_t seed = 1; seed <= 200; seed++) {
        int v = seed & 1;
        jas_new(&G, v, seed);
        uint32_t rs = seed * 7919u;
        int guard = 0;
        while (P->phase != JP_OVER && guard++ < 4000) {
            int acts[16], n = 0;
            if (P->phase == JP_PLAY) {
                for (int i = 0; i < P->t.n[P->turn]; i++)
                    if (jas_can_play(P, P->t.hand[P->turn][i])) acts[n++] = JA_PLAY | P->t.hand[P->turn][i];
                if (jas_can_weis(P)) for (int k = 0; k < 4; k++) acts[n++] = JA_WEIS;
            } else if (P->phase == JP_CHOOSE) {
                for (int m = 0; m < JM_MODES; m++) acts[n++] = JA_MODE | m;
                if (!P->pushed) acts[n++] = JA_PUSH;
            } else
                acts[n++] = JA_NEXT;
            if (!n) break;
            if (!jas_play(&G, acts[cards_rand(&rs) % (uint32_t)n])) { CHECK(0); break; }
        }
        CHECK(P->phase == JP_OVER && P->first != JAS_NONE && P->total[P->first] >= jas_target(v));
        int tot[2] = {0, 0}, ok = 1;
        for (int d = 0; d < P->ndeals; d++) {
            const jsdeal *r = &P->list[d];
            ok &= r->eyes[0] + r->eyes[1] == 157 && !(r->weis[0] && r->weis[1]);
            ok &= r->score[0] + r->score[1] ==
                  (157 + (r->match ? 100 : 0) + r->weis[0] + r->weis[1] + r->stock[0] + r->stock[1]) * r->mult;
            tot[0] += r->score[0], tot[1] += r->score[1];
            weis += r->weis[0] + r->weis[1] > 0, stock += r->stock[0] + r->stock[1] > 0, match += r->match != 0;
            pushed += r->pushed, deals++;
        }
        CHECK(ok && tot[0] == P->total[0] && tot[1] == P->total[1]);
    }
    printf("  200 random games, %d deals: %d weis, %d stock, %d match, %d pushed\n", deals, weis, stock, match, pushed);
}

int main(void)
{
    test_cards();
    test_play();
    test_weis();
    test_stock_match();
    test_end();
    test_undo_notation();
    test_random();
    printf("jass: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
