/*
 * Canasta: the rules' test (the cards and their values; the deal and the red 3s; melds and their limits; the initial
 * meld's count; taking the discard pile, frozen or not, blocked by a black 3; going out and its canastas; the score
 * of a hand; the stock running out; undo; the notation; whole games from seeds, 4 and 2 players). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/canasta.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static cagame G, H;
static capos *P = &G.pos;

/* cards from names: "Kh Kd X" */
static int cards(const char *s, uint8_t *out)
{
    int n = 0, c;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int k = ca_card_name(s, &c);
        if (!k) return -1;
        out[n++] = (uint8_t)c;
        s += k;
    }
    return n;
}

static void set_hand(int pl, const char *s) { P->n[pl] = (uint8_t)cards(s, P->hand[pl]); }

static void set_pile(const char *s) { P->npile = (uint8_t)cards(s, P->pile); }

static void set_meld(int side, int rank, const char *s)
{
    P->mn[side][rank] = (uint8_t)cards(s, P->meld[side][rank]);
    P->mn0[side][rank] = P->mn[side][rank];
    P->opened[side] = 1;
}

/* a fresh position, nothing on the table: player 0 to draw */
static void blank(int players)
{
    ca_new(&G, players, 7);
    memset(P->mn, 0, sizeof P->mn);
    memset(P->mn0, 0, sizeof P->mn0);
    memset(P->nred3, 0, sizeof P->nred3);
    P->npile = 0;
    P->frozen = 0;
    P->freezer = CA_NONE;
    P->turn = 0;
    P->phase = CP_DRAW;
    G.n = 0;
}

static ca_act act(int kind, int rank, const char *s)
{
    ca_act a;
    memset(&a, 0, sizeof a);
    a.kind = (uint8_t)kind;
    a.rank = (uint8_t)rank;
    a.n = s ? (uint8_t)cards(s, a.card) : 0;
    return a;
}

static int legal(int kind, int rank, const char *s)
{
    ca_act a = act(kind, rank, s);
    return ca_legal(P, &a);
}

static int play(int kind, int rank, const char *s)
{
    ca_act a = act(kind, rank, s);
    return ca_play(&G, &a);
}

static int count_all(const capos *p)
{
    int n = p->nstock + p->npile + p->nred3[0] + p->nred3[1];
    for (int q = 0; q < p->players; q++) n += p->n[q];
    for (int s = 0; s < 2; s++)
        for (int r = 0; r < CA_RANKS; r++) n += p->mn[s][r];
    return n;
}

static void test_cards(void)
{
    int c;
    CHECK(ca_card_name("X", &c) == 1 && ca_wild(c) && ca_value(c) == 50);
    CHECK(ca_card_name("2c", &c) == 2 && ca_wild(c) && ca_value(c) == 20);
    CHECK(ca_card_name("Ah", &c) == 2 && !ca_wild(c) && ca_value(c) == 20);
    CHECK(ca_card_name("Ks", &c) == 2 && ca_value(c) == 10 && ca_card_name("8d", &c) && ca_value(c) == 10);
    CHECK(ca_card_name("7d", &c) && ca_value(c) == 5 && ca_card_name("4c", &c) && ca_value(c) == 5);
    CHECK(ca_card_name("3s", &c) && ca_black3(c) && !ca_red3(c) && ca_value(c) == 5);
    CHECK(ca_card_name("3h", &c) && ca_red3(c) && !ca_black3(c));
    CHECK(ca_value(CARD(SUIT_JOKER, 1)) == 50 && ca_wild(CARD(SUIT_JOKER, 1)) && !ca_black3(CARD(SUIT_JOKER, 1)));
    CHECK(!ca_card_name("Zh", &c) && !ca_card_name("Kx", &c));
}

static void test_deal(void)
{
    for (int players = 2; players <= 4; players += 2)
        for (uint32_t seed = 1; seed < 40; seed++) {
            ca_new(&G, players, seed);
            ca_new(&H, players, seed);
            CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
            CHECK(count_all(P) == CA_CARDS);
            int seen[256] = {0};
            for (int q = 0; q < players; q++) {
                CHECK(P->n[q] == (players == 2 ? 15 : 11));
                for (int i = 0; i < P->n[q]; i++) {
                    seen[P->hand[q][i]]++;
                    CHECK(!ca_red3(P->hand[q][i]));
                }
            }
            for (int i = 0; i < P->nstock; i++) seen[P->stock[i]]++;
            for (int i = 0; i < P->npile; i++) seen[P->pile[i]]++;
            for (int s = 0; s < 2; s++)
                for (int i = 0; i < P->nred3[s]; i++) { seen[P->red3[s][i]]++; CHECK(ca_red3(P->red3[s][i])); }
            int twice = 1;
            for (int s = 0; s < 4; s++)
                for (int r = 1; r <= 13; r++) twice &= seen[CARD(s, r)] == 2;
            CHECK(twice && seen[CARD(SUIT_JOKER, 0)] == 2 && seen[CARD(SUIT_JOKER, 1)] == 2);
            int top = P->pile[P->npile - 1];
            CHECK(!ca_wild(top) && !ca_red3(top));
            CHECK(P->frozen == (P->npile > 1));
            CHECK(P->turn == 0 && P->phase == CP_DRAW && P->dealer == players - 1);
        }
}

static void test_melds(void)
{
    blank(4);
    set_hand(0, "Kh Kd Ks Qh Qd X 2c 2d 2h 7c 7d 3s 3c 3s 5h 5d");
    P->phase = CP_MELD;
    P->opened[0] = 1;
    CHECK(legal(CA_MELD_IT, 0, "Kh Kd Ks"));
    CHECK(legal(CA_MELD_IT, 0, "Kh Kd X"));         /* two naturals and a wild card */
    CHECK(!legal(CA_MELD_IT, 0, "Kh X 2c"));        /* one natural: no */
    CHECK(!legal(CA_MELD_IT, 0, "Kh Kd"));          /* two cards: no */
    CHECK(!legal(CA_MELD_IT, 0, "Kh Kd Qh"));       /* two ranks: no */
    CHECK(!legal(CA_MELD_IT, 0, "X 2c 2d"));        /* wild cards alone: no rank */
    CHECK(legal(CA_MELD_IT, 0, "Kh Kd Ks X 2c 2d"));   /* three wild cards at most */
    CHECK(!legal(CA_MELD_IT, 0, "Kh Kd Ks X 2c 2d 2h"));
    CHECK(!legal(CA_MELD_IT, 0, "3s 3c 3s"));       /* black 3s: only going out */
    CHECK(!legal(CA_MELD_IT, 0, "Kh Kd As"));       /* a card not held */
    CHECK(play(CA_MELD_IT, 0, "Kh Kd X") && P->mn[0][13] == 3);
    CHECK(legal(CA_MELD_IT, 0, "Ks") && legal(CA_MELD_IT, 13, "2c"));   /* on the meld */
    CHECK(!legal(CA_MELD_IT, 12, "Ks"));            /* not on the queens */
    CHECK(play(CA_MELD_IT, 13, "2c 2d") && P->mn[0][13] == 5);
    CHECK(!legal(CA_MELD_IT, 13, "2h"));            /* a fourth wild card: no */
    CHECK(legal(CA_MELD_IT, 0, "Ks"));
    /* before drawing: melds only for a side that has not melded */
    blank(4);
    set_hand(0, "Kh Kd Ks Qh Qd Qc 7c");
    CHECK(legal(CA_MELD_IT, 0, "Kh Kd Ks"));
    P->opened[0] = 1;
    CHECK(!legal(CA_MELD_IT, 0, "Kh Kd Ks"));
    CHECK(!legal(CA_DISCARD, 0, "7c"));             /* draw first */
}

static void test_initial(void)
{
    blank(4);
    CHECK(ca_need(P, 0) == 50);
    P->score[0] = -20;
    CHECK(ca_need(P, 0) == 15);
    P->score[0] = 1500;
    CHECK(ca_need(P, 0) == 90);
    P->score[0] = 2995;
    CHECK(ca_need(P, 0) == 90);
    P->score[0] = 3000;
    CHECK(ca_need(P, 0) == 120);
    P->score[0] = 0;
    set_hand(0, "Kh Kd Ks Qh Qd Qc 7c 7d 9s");
    P->phase = CP_MELD;
    CHECK(play(CA_MELD_IT, 0, "Kh Kd Ks"));        /* 30: not enough yet */
    CHECK(!legal(CA_DISCARD, 0, "9s"));
    CHECK(play(CA_MELD_IT, 0, "Qh Qd Qc"));        /* 60 */
    CHECK(legal(CA_DISCARD, 0, "9s"));
    CHECK(play(CA_DISCARD, 0, "9s") && P->opened[0] && P->turn == 1);
    CHECK(P->melded[0] && !P->melded[2]);
    /* partners share: player 2 may add to the kings */
    P->turn = 2;
    P->phase = CP_MELD;
    set_hand(2, "Kc 4h 5h");
    CHECK(legal(CA_MELD_IT, 0, "Kc"));
    CHECK(!legal(CA_MELD_IT, 0, "4h 5h Kc"));
}

static void test_pile(void)
{
    /* not melded yet: frozen, a natural pair; the count with the top card */
    blank(4);
    set_hand(0, "Kh Kd Ah Ad X 9c 9d 4s 5s");
    set_pile("7c 8h Ks");
    CHECK(ca_frozen_for(P, 0) && !P->frozen);
    CHECK(!legal(CA_TAKE, 0, "Kh X"));             /* frozen for the side: a natural pair */
    CHECK(!legal(CA_TAKE, 0, "Kh Kd"));            /* 30: short of 50 */
    CHECK(legal(CA_TAKE, 0, "Kh Kd X"));           /* 80 */
    CHECK(play(CA_MELD_IT, 0, "Ah Ad X"));         /* laid before drawing: 60, the pile's top counts on */
    CHECK(legal(CA_TAKE, 0, "Kh Kd"));
    CHECK(play(CA_TAKE, 0, "Kh Kd") && P->mn[0][13] == 3 && P->npile == 0 && P->phase == CP_MELD);
    CHECK(P->n[0] == 4 + 2);                       /* 9c 9d 4s 5s, and 7c 8h */
    CHECK(play(CA_DISCARD, 0, "4s") && P->opened[0]);
    /* melded: not frozen; a natural and a wild card, or on the meld */
    blank(4);
    set_meld(0, 9, "9h 9d 9s");
    set_hand(0, "8h X 4c 5c 6c");
    set_pile("4d 8c");
    CHECK(!ca_frozen_for(P, 0));
    CHECK(legal(CA_TAKE, 0, "8h X"));
    CHECK(!legal(CA_TAKE, 0, NULL));               /* no meld of 8s */
    set_pile("4d 9c");
    CHECK(legal(CA_TAKE, 0, NULL));                /* on the 9s */
    CHECK(!legal(CA_TAKE, 0, "8h X"));             /* not the top card's rank */
    /* a wild card in the pile: frozen for all */
    set_pile("2h 9c");
    P->frozen = 1;
    CHECK(!legal(CA_TAKE, 0, NULL));
    set_hand(0, "9h 9d 4c 5c 6c");
    CHECK(legal(CA_TAKE, 0, "9h 9d"));
    CHECK(play(CA_TAKE, 0, "9h 9d") && !P->frozen && P->mn[0][9] == 6);
    /* the top card a black 3, or a wild card: no */
    blank(4);
    set_meld(0, 9, "9h 9d 9s");
    set_hand(0, "3s 3c 9c 4c 5c");
    set_pile("9d 3c");
    CHECK(!legal(CA_TAKE, 0, NULL) && !legal(CA_TAKE, 0, "3s 3c"));
    set_pile("9d 2c");
    CHECK(!legal(CA_TAKE, 0, NULL));
    /* a wild card discarded freezes the pile; a red 3 in the pile goes beside the melds */
    blank(4);
    set_meld(0, 9, "9h 9d 9s");
    set_hand(0, "2c 4c 5c 6c");
    P->phase = CP_MELD;
    set_pile("8d");
    CHECK(play(CA_DISCARD, 0, "2c") && P->frozen && P->freezer == CARD(SUIT_CLUBS, 2));
    blank(4);
    set_meld(0, 9, "9h 9d 9s");
    set_hand(0, "4c 5c 6c");
    set_pile("3h Kc 9c");
    CHECK(play(CA_TAKE, 0, NULL) && P->nred3[0] == 1 && P->n[0] == 4);
}

static void test_out(void)
{
    /* no canasta: not down to fewer than two cards */
    blank(4);
    set_meld(0, 9, "9h 9d 9s 9c 9h X");
    set_hand(0, "Kh Kd Ks 5c");
    P->phase = CP_MELD;
    CHECK(!legal(CA_MELD_IT, 0, "Kh Kd Ks"));
    CHECK(legal(CA_MELD_IT, 9, "2c") == 0);         /* not held */
    set_hand(0, "Kh Kd Ks 9c 5c");
    CHECK(legal(CA_MELD_IT, 9, "9c"));              /* a canasta: 7 cards */
    CHECK(play(CA_MELD_IT, 9, "9c") && ca_canastas(P, 0) == 1);
    CHECK(legal(CA_MELD_IT, 0, "Kh Kd Ks"));        /* now down to one card, then out */
    CHECK(play(CA_MELD_IT, 0, "Kh Kd Ks") && P->n[0] == 1);
    set_hand(1, "Qh Qd");
    set_hand(2, "4h 5h");
    set_hand(3, "Jh");
    P->nred3[0] = 1;
    P->red3[0][0] = CARD(SUIT_HEARTS, 3);
    CHECK(play(CA_DISCARD, 0, "5c") && P->phase == CP_HAND && P->out == 0 && !P->concealed);
    const ca_score *a = &P->res[0], *b = &P->res[1];
    CHECK(a->mixed == 1 && a->natural == 0 && a->bonus == 300);
    CHECK(a->melds == 6 * 10 + 50 + 3 * 10 && a->red3 == 100 && a->out == 100 && a->minus == 10);
    CHECK(a->total == 90 + 50 + 300 + 100 + 100 - 10 && P->score[0] == a->total);
    CHECK(b->melds == 0 && b->minus == 30 && b->total == -30);
    /* a natural canasta, all four red 3s, and a side without melds: its red 3s count against it */
    blank(4);
    set_meld(0, 7, "7h 7d 7s 7c 7h 7d 7s");
    set_hand(0, "5c 6c");
    P->phase = CP_MELD;
    P->nred3[0] = 4;
    P->nred3[1] = 0;
    set_hand(1, "");
    set_hand(2, "");
    set_hand(3, "");
    CHECK(play(CA_DISCARD, 0, "6c") && P->turn == 1);
    P->nred3[1] = 2;
    P->phase = CP_MELD;
    P->turn = 3;
    set_hand(3, "Kh Kd Ks");
    P->turn = 0;
    set_hand(0, "5c");
    CHECK(play(CA_DISCARD, 0, "5c") && P->phase == CP_HAND);
    CHECK(P->res[0].natural == 1 && P->res[0].bonus == 500 && P->res[0].red3 == 800 && P->res[0].out == 100);
    CHECK(P->res[1].red3 == -200 && P->res[1].minus == 30);
    /* concealed: the whole hand in one turn, nothing melded before */
    blank(4);
    set_hand(0, "Kh Kd Ks Kc Kh Kd Ks Qh Qd Qc 4c");
    P->phase = CP_MELD;
    CHECK(play(CA_MELD_IT, 0, "Kh Kd Ks Kc Kh Kd Ks"));   /* a natural canasta (70) */
    CHECK(play(CA_MELD_IT, 0, "Qh Qd Qc") && P->n[0] == 1);
    CHECK(play(CA_DISCARD, 0, "4c") && P->phase == CP_HAND && P->concealed && P->res[0].out == 200);
    /* concealed going out needs no count: 2 players, two canastas */
    blank(2);
    set_hand(0, "4h 4d 4s 4c 4h 4d 4s 5h 5d 5s 5c 5h 5d 5s 6c");
    P->phase = CP_MELD;
    P->score[0] = 3000;                             /* 120 asked, but concealed */
    CHECK(play(CA_MELD_IT, 0, "4h 4d 4s 4c 4h 4d 4s"));
    CHECK(ca_can_out(P, 0) == 0);                   /* two players: two canastas */
    CHECK(play(CA_MELD_IT, 0, "5h 5d 5s 5c 5h 5d 5s") && ca_can_out(P, 0));
    CHECK(play(CA_DISCARD, 0, "6c") && P->phase == CP_HAND && P->concealed && P->res[0].out == 200);
    /* black 3s going out */
    blank(4);
    set_meld(0, 9, "9h 9d 9s 9c 9h 9d 9s");
    set_hand(0, "3s 3c 3s 7h");
    P->phase = CP_MELD;
    CHECK(play(CA_MELD_IT, 0, "3s 3c 3s") && P->mn[0][3] == 3);
    CHECK(play(CA_DISCARD, 0, "7h") && P->phase == CP_HAND && P->res[0].melds == 70 + 15);
    /* going out by melding the last cards, no discard */
    blank(4);
    set_meld(0, 9, "9h 9d 9s 9c 9h 9d 9s");
    set_hand(0, "Kh Kd Ks");
    P->phase = CP_MELD;
    CHECK(play(CA_MELD_IT, 0, "Kh Kd Ks") && P->phase == CP_HAND && P->out == 0);
}

static void test_stock(void)
{
    /* a red 3 drawn: laid down, another card drawn */
    blank(4);
    P->opened[0] = 1;
    P->nstock = (uint8_t)cards("7c 3d", P->stock);   /* the top last */
    set_hand(0, "4h 5h");
    set_pile("8s");
    CHECK(play(CA_STOCK, 0, NULL) && P->nred3[0] == 1 && P->n[0] == 3 && P->hand[0][2] == CARD(SUIT_CLUBS, 7));
    /* two players: two cards */
    blank(2);
    P->nstock = (uint8_t)cards("7c 6c 5c", P->stock);
    set_hand(0, "4h");
    CHECK(play(CA_STOCK, 0, NULL) && P->n[0] == 3 && P->nstock == 1);
    /* the last card a red 3: the hand ends */
    blank(4);
    P->nstock = (uint8_t)cards("3h", P->stock);
    set_hand(0, "4h 5h");
    CHECK(play(CA_STOCK, 0, NULL) && P->phase == CP_HAND && P->out == CA_NONE);
    /* the stock empty: forced to take a pile that fits a meld, else the hand ends */
    blank(4);
    set_meld(0, 9, "9h 9d 9s");
    P->nstock = 0;
    set_hand(0, "4h 5h");
    set_pile("Kd 9c");
    CHECK(ca_forced(P) && !legal(CA_STOCK, 0, NULL) && legal(CA_TAKE, 0, NULL));
    set_pile("Kd 8c");
    CHECK(!ca_forced(P) && legal(CA_STOCK, 0, NULL));
    CHECK(play(CA_STOCK, 0, NULL) && P->phase == CP_HAND && P->out == CA_NONE);
    CHECK(play(CA_NEXT, 0, NULL) && P->phase == CP_DRAW && P->dealer == 0 && P->turn == 1 && P->hands == 1);
}

static void test_undo_script(void)
{
    ca_new(&G, 4, 11);
    capos start = G.pos;
    CHECK(ca_script(&G, "a a a a a a"));
    capos mid = G.pos;
    CHECK(ca_script(&G, "a a"));
    CHECK(ca_undo(&G) && ca_undo(&G) && !memcmp(&G.pos, &mid, sizeof mid));
    while (G.n) ca_undo(&G);
    CHECK(!memcmp(&G.pos, &start, sizeof start) && !ca_undo(&G));
    /* the notation by hand */
    blank(4);
    set_hand(0, "Kh Kd Ks X 4c 9d");
    P->phase = CP_MELD;
    P->opened[0] = 1;
    CHECK(ca_script(&G, "mKh.Kd.Ks mK:X d9d") && P->mn[0][13] == 4 && P->turn == 1);
    CHECK(!ca_script(&G, "q") && !ca_script(&G, "mZh") && !ca_script(&G, "d4c"));   /* not player 1's card */
    ca_new(&G, 2, 5);
    CHECK(ca_script(&G, "#") && (P->phase == CP_HAND || P->phase == CP_OVER));
}

/* whole games, plain play from seeds: every card always somewhere, a winner once a side has 5000 */
static void test_games(void)
{
    for (int players = 2; players <= 4; players += 2)
        for (uint32_t seed = 1; seed <= 24; seed++) {
            ca_new(&G, players, seed * 7919u);
            int ok = 1, steps = 0, hands = 0, outs = 0;
            ca_act a;
            while (P->phase != CP_OVER && steps < 200000) {
                if (!ca_auto(P, &a) || !ca_play(&G, &a)) { ok = 0; break; }
                steps++;
                if (count_all(P) != CA_CARDS) { ok = 0; break; }
                if (P->phase == CP_HAND || P->phase == CP_OVER) {
                    hands++;
                    outs += P->out != CA_NONE;
                    int sum = 0;
                    for (int s = 0; s < 2; s++) sum += P->res[s].total;
                    (void)sum;
                }
                if (G.n >= CA_MAX_ACTS - 4) {       /* a long game: go on from here */
                    capos keep = G.pos;
                    G.n = 0;
                    G.pos = keep;
                }
            }
            CHECK(ok && P->phase == CP_OVER);
            CHECK(P->score[0] >= CA_GOAL || P->score[1] >= CA_GOAL);
            int w = ca_winner(P);
            unsigned m = ca_winners(P);
            CHECK(w < 0 ? m == (1u << players) - 1 : (m & 1) == (unsigned)(w == 0));
            CHECK(hands >= 2 && outs >= 1);
            if (seed == 1) printf("  %d players, seed %u: %d hands, %d gone out, %d - %d\n", players, seed, hands, outs,
                                  P->score[0], P->score[1]);
        }
}

int main(void)
{
    test_cards();
    test_deal();
    test_melds();
    test_initial();
    test_pile();
    test_out();
    test_stock();
    test_undo_script();
    test_games();
    printf("canasta: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
