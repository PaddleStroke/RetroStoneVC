/*
 * Cribbage: the rules' test (the deal from a seed, the show's counting: fifteens, pairs, runs and double runs,
 * flushes in the hand and the crib, his nobs; the play: 15, 31, pairs, runs, the go and the last card; his heels;
 * the turn of the play; the end at 121; three players; undo; whole random games from seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/cribbage.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static cgame G, H;
static cpos *P = &G.pos;

enum { S = SUIT_SPADES, He = SUIT_HEARTS, D = SUIT_DIAMONDS, C = SUIT_CLUBS };

static int show(int a, int b, int c, int d, int st, int crib)
{
    uint8_t four[4] = {(uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d};
    crib_ev it[CRIB_MAX_EV];
    int n, sum = 0, t = crib_show(four, st, crib, it, &n);
    for (int i = 0; i < n; i++) sum += it[i].pts;
    return sum == t ? t : -1;
}

static int count_kind(int a, int b, int c, int d, int st, int kind)
{
    uint8_t four[4] = {(uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d};
    crib_ev it[CRIB_MAX_EV];
    int n, k = 0;
    crib_show(four, st, 0, it, &n);
    for (int i = 0; i < n; i++) k += it[i].kind == kind;
    return k;
}

static void test_deal(void)
{
    crib_new(&G, 2, 42);
    crib_new(&H, 2, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(P->phase == CP_DISCARD && P->dealer == 1 && P->turn == 0 && P->nhand[0] == 6 && P->nhand[1] == 6);
    CHECK(P->nstock == 40 && P->ncrib == 0 && P->starter == CARD_NONE);
    int seen[256] = {0};
    for (int q = 0; q < 2; q++)
        for (int i = 0; i < 6; i++) seen[P->hand[q][i]]++;
    for (int i = 0; i < P->nstock; i++) seen[P->stock[i]]++;
    int once = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) once &= seen[CARD(s, r)] == 1;
    CHECK(once);
    int sorted = 1;
    for (int i = 1; i < 6; i++) sorted &= CARD_RANK(P->hand[0][i - 1]) <= CARD_RANK(P->hand[0][i]);
    CHECK(sorted);
    crib_new(&H, 2, 43);
    CHECK(memcmp(H.pos.hand, G.pos.hand, sizeof G.pos.hand) != 0);
    crib_new(&G, 3, 5);                         /* three: 5 each, one to the crib from the pack */
    CHECK(P->ndeal == 5 && P->ncrib == 1 && P->nstock == 52 - 15 - 1 && P->dealer == 2 && P->turn == 0);
    CHECK(crib_away(P) == 1 && crib_legal(P, 1 << 4) && !crib_legal(P, 1 << 5) && !crib_legal(P, 3));
}

static void test_show(void)
{
    CHECK(crib_value(CARD(S, 1)) == 1 && crib_value(CARD(S, 10)) == 10 && crib_value(CARD(S, RANK_KING)) == 10);
    /* the 29 hand: three fives and the jack of the starter's suit, the fourth five cut */
    CHECK(show(CARD(S, 5), CARD(He, 5), CARD(D, 5), CARD(C, RANK_JACK), CARD(C, 5), 0) == 29);
    CHECK(count_kind(CARD(S, 5), CARD(He, 5), CARD(D, 5), CARD(C, RANK_JACK), CARD(C, 5), CE_FIFTEEN) == 8);
    CHECK(count_kind(CARD(S, 5), CARD(He, 5), CARD(D, 5), CARD(C, RANK_JACK), CARD(C, 5), CE_PAIR) == 6);
    CHECK(show(CARD(S, 5), CARD(He, 5), CARD(D, 5), CARD(C, 5), CARD(C, 10), 0) == 28);
    /* 4 5 5 6 6: four runs of three, two pairs, four fifteens */
    CHECK(show(CARD(He, 4), CARD(He, 5), CARD(He, 6), CARD(D, 6), CARD(S, 5), 0) == 24);
    CHECK(count_kind(CARD(He, 4), CARD(He, 5), CARD(He, 6), CARD(D, 6), CARD(S, 5), CE_RUN) == 4);
    /* nothing: all even, no run, no pair, no flush */
    CHECK(show(CARD(S, 2), CARD(He, 4), CARD(D, 6), CARD(C, 8), CARD(D, RANK_KING), 0) == 0);
    /* a run of five: 5 points, and 15s: 3+5+7, 4+5+6... */
    CHECK(show(CARD(S, 3), CARD(He, 4), CARD(D, 5), CARD(C, 6), CARD(D, 7), 0) == 5 + 2 * 2);
    /* flush: four in the hand 4, five 5; the crib only five */
    CHECK(show(CARD(He, 1), CARD(He, 3), CARD(He, 9), CARD(He, RANK_QUEEN), CARD(S, 7), 0) == 4);
    CHECK(show(CARD(He, 1), CARD(He, 3), CARD(He, 9), CARD(He, RANK_QUEEN), CARD(S, 7), 1) == 0);
    CHECK(show(CARD(He, 1), CARD(He, 3), CARD(He, 9), CARD(He, RANK_QUEEN), CARD(He, 7), 1) == 5);
    CHECK(show(CARD(He, 1), CARD(He, 3), CARD(He, 9), CARD(He, RANK_QUEEN), CARD(He, 7), 0) == 5);
    /* his nobs: the jack of the starter's suit, in the hand (not the starter itself) */
    CHECK(show(CARD(D, RANK_JACK), CARD(S, 2), CARD(He, 9), CARD(C, 7), CARD(D, 1), 0) == 1);
    CHECK(show(CARD(D, 1), CARD(S, 2), CARD(He, 9), CARD(C, 7), CARD(D, RANK_JACK), 0) == 0);
    /* 7 7 8 8 + 9? double double run of 7-8-9: 4 runs x3 = 12, pairs 4, fifteens 7+8 x4 = 8: 24 */
    CHECK(show(CARD(S, 7), CARD(He, 7), CARD(S, 8), CARD(He, 8), CARD(C, 9), 0) == 24);
    /* the items list the cards: a fifteen's mask adds up to 15 */
    uint8_t four[4] = {CARD(S, 10), CARD(He, 5), CARD(D, 2), CARD(C, 3)};
    crib_ev it[CRIB_MAX_EV];
    int n, t = crib_show(four, CARD(D, 9), 0, it, &n);
    CHECK(t == 4 && n == 2 && it[0].kind == CE_FIFTEEN && it[0].mask == (1 | 2));   /* 10+5, 10+2+3 */
}

/* sets up a play by hand: the four kept of each, a dealer */
static void set_play(int dealer, const int *a, const int *b)
{
    crib_new(&G, 2, 1);
    P->dealer = (uint8_t)dealer;
    for (int i = 0; i < 4; i++) P->kept[0][i] = (uint8_t)a[i], P->kept[1][i] = (uint8_t)b[i];
    P->used[0] = P->used[1] = 0;
    P->nhand[0] = P->nhand[1] = 0;
    P->phase = CP_PLAY;
    P->turn = (uint8_t)(1 - dealer);
    P->starter = CARD(C, 2);
    G.n = 0;
}

static int pts(int kind)
{
    for (int i = 0; i < P->nev; i++)
        if (P->ev[i].kind == kind) return P->ev[i].pts;
    return 0;
}

static void test_play(void)
{
    const int a[4] = {CARD(S, 5), CARD(He, 7), CARD(S, 7), CARD(D, 6)};
    const int b[4] = {CARD(C, 10), CARD(He, 8), CARD(C, 7), CARD(D, RANK_KING)};
    set_play(1, a, b);
    CHECK(crib_play(&G, CA_PLAY + 0) && P->count == 5 && P->turn == 1 && P->nev == 0);   /* 5 */
    CHECK(crib_play(&G, CA_PLAY + 0) && P->count == 15 && pts(CE_FIFTEEN) == 2 && P->score[1] == 2);   /* 10: 15 */
    CHECK(P->turn == 0 && P->back[1] == 0);
    CHECK(crib_play(&G, CA_PLAY + 1) && P->count == 22 && P->nev == 0);   /* 7 */
    CHECK(crib_play(&G, CA_PLAY + 2) && pts(CE_PAIR) == 2);   /* 7: a pair */
    /* 29: nobody can lay a card under 31 (0 has 7 and 6, 1 has 8 and king): the go to 1 */
    CHECK(P->count == 0 && pts(CE_GO) == 1 && P->score[1] == 5 && P->turn == 0 && P->old == 4);
    CHECK(!crib_play(&G, CA_PLAY + 0));         /* played already */
    CHECK(crib_play(&G, CA_PLAY + 2) && P->count == 7 && P->turn == 1);    /* 7 */
    CHECK(crib_play(&G, CA_PLAY + 1) && P->count == 15 && pts(CE_FIFTEEN) == 2 && P->score[1] == 7);   /* 8 */
    CHECK(crib_play(&G, CA_PLAY + 3) && P->count == 21 && pts(CE_RUN) == 3 && P->score[0] == 3);   /* 6: 7 8 6 */
    CHECK(crib_play(&G, CA_PLAY + 3) && pts(CE_THIRTYONE) == 2 && pts(CE_LAST) == 0 && P->score[1] == 9);   /* king: 31 */
    CHECK(P->phase == CP_SHOW && P->shown == 1);
}

static void test_play_more(void)
{
    /* three of a kind and four: 6 and 12; 31 scores 2 and no go */
    const int a[4] = {CARD(S, 3), CARD(He, 3), CARD(S, 9), CARD(D, 9)};
    const int b[4] = {CARD(C, 3), CARD(D, 3), CARD(C, 9), CARD(He, RANK_QUEEN)};
    set_play(1, a, b);
    CHECK(crib_play(&G, CA_PLAY + 0) && crib_play(&G, CA_PLAY + 0) && pts(CE_PAIR) == 2);
    CHECK(crib_play(&G, CA_PLAY + 1) && pts(CE_PAIR) == 6 && P->score[0] == 6);
    CHECK(crib_play(&G, CA_PLAY + 1) && pts(CE_PAIR) == 12 && P->score[1] == 14 && P->count == 12);
    CHECK(crib_play(&G, CA_PLAY + 2) && P->count == 21);   /* 9 */
    CHECK(crib_play(&G, CA_PLAY + 3) && P->count == 0 && pts(CE_THIRTYONE) == 2 && pts(CE_GO) == 0);   /* queen: 31 */
    CHECK(P->score[1] == 16 && P->turn == 0);
    CHECK(crib_play(&G, CA_PLAY + 3) && P->turn == 1);    /* 9 */
    CHECK(crib_play(&G, CA_PLAY + 2) && pts(CE_PAIR) == 2 && pts(CE_LAST) == 1 && P->phase == CP_SHOW);
    /* a run in any order among the last cards; broken by a card that does not fit */
    const int c[4] = {CARD(S, 4), CARD(He, 2), CARD(S, 6), CARD(D, 1)};
    const int d[4] = {CARD(C, 3), CARD(D, 5), CARD(C, 9), CARD(He, 1)};
    set_play(1, c, d);
    CHECK(crib_play(&G, CA_PLAY + 0) && crib_play(&G, CA_PLAY + 1) == 1 && P->nev == 0);   /* 4, 5 */
    CHECK(crib_play(&G, CA_PLAY + 2) && pts(CE_RUN) == 3);   /* 6: 4 5 6, and 15 */
    CHECK(P->nev == 2 && P->nleap == 1 && P->leap[0].pl == 0 && P->leap[0].from == 0 && P->leap[0].to == 5);
    CHECK(P->ev[0].leap == 0 && P->ev[1].leap == 0 && P->back[0] == 0);
    CHECK(crib_play(&G, CA_PLAY + 0) && pts(CE_RUN) == 4 && P->score[1] == 4);   /* 3: 3 4 5 6 */
    CHECK(crib_play(&G, CA_PLAY + 1) && pts(CE_RUN) == 5 && P->score[0] == 10);   /* 2: 2..6 */
    CHECK(P->count == 20);
    CHECK(crib_play(&G, CA_PLAY + 3) && pts(CE_RUN) == 6);   /* ace: 1..6, 21 */
    CHECK(crib_play(&G, CA_PLAY + 3) && pts(CE_RUN) == 0 && pts(CE_PAIR) == 2);   /* the other ace: 22, a pair */
    /* 1 has the 9 left: 31? 22 + 9 = 31 */
    CHECK(P->turn == 1 && crib_play(&G, CA_PLAY + 2) && pts(CE_THIRTYONE) == 2 && P->phase == CP_SHOW);
}

/* one player cannot go: the other lays on; the go; who leads the next count */
static void test_go(void)
{
    const int a[4] = {CARD(S, 10), CARD(He, 10), CARD(S, RANK_KING), CARD(D, 1)};
    const int b[4] = {CARD(C, 9), CARD(D, 2), CARD(C, 1), CARD(He, 3)};
    set_play(1, a, b);
    CHECK(crib_play(&G, CA_PLAY + 0) && crib_play(&G, CA_PLAY + 0) && P->count == 19);   /* 10, 9 */
    CHECK(crib_play(&G, CA_PLAY + 1) && P->count == 29 && P->turn == 1);   /* 10: 0 has king and ace; 1 can lay 2 */
    CHECK(!crib_play(&G, CA_PLAY + 3));         /* the 3: past 31 */
    CHECK(crib_play(&G, CA_PLAY + 1) && pts(CE_THIRTYONE) == 2);   /* 2: 31 */
    /* 1 has the ace and 3 left; 0 the king and ace: 0 leads (after 1 laid the last) */
    CHECK(P->count == 0 && P->turn == 0);
    CHECK(crib_play(&G, CA_PLAY + 2) && P->turn == 1);   /* king */
    CHECK(crib_play(&G, CA_PLAY + 2) && crib_play(&G, CA_PLAY + 3) && crib_play(&G, CA_PLAY + 3));
    CHECK(P->phase == CP_SHOW);
}

/* the deal round the show and the next deal; his heels */
static void test_hand(void)
{
    for (uint32_t seed = 1; seed < 400; seed++) {   /* a seed whose starter is a jack */
        crib_new(&G, 2, seed);
        if (CARD_RANK(G.pos.stock[G.pos.nstock - 1]) == RANK_JACK) break;
    }
    CHECK(crib_script(&G, "d01 d23"));
    CHECK(P->phase == CP_CUT && P->turn == 0 && P->ncrib == 4);
    CHECK(crib_script(&G, "c") && P->score[1] == 2 && P->ev[0].kind == CE_HEELS && P->phase == CP_PLAY && P->turn == 0);
    CHECK(crib_script(&G, "#") && P->phase == CP_SHOW && P->shown == 1);
    int before = P->score[1];
    CHECK(crib_script(&G, "n") && P->shown == 2 && P->turn == 1);   /* the dealer's hand */
    CHECK(crib_script(&G, "n") && P->shown == 3 && P->turn == 1);   /* the crib */
    CHECK(P->score[1] >= before);
    CHECK(crib_script(&G, "n") && P->phase == CP_DISCARD && P->dealer == 0 && P->turn == 1 && P->hands == 2);
    CHECK(!crib_script(&G, "x"));
}

/* the game ends at 121 at once, in the middle of the play */
static void test_end(void)
{
    const int a[4] = {CARD(S, 5), CARD(He, 7), CARD(S, 7), CARD(D, 6)};
    const int b[4] = {CARD(C, 10), CARD(He, 8), CARD(C, 7), CARD(D, RANK_KING)};
    set_play(1, a, b);
    P->score[1] = 120;
    CHECK(crib_play(&G, CA_PLAY + 0) && crib_play(&G, CA_PLAY + 0));   /* 15 for 2: 122, kept at 121 */
    CHECK(P->phase == CP_OVER && P->score[1] == CRIB_GOAL && crib_winner(P) == 1 && P->back[1] == 120);
    CHECK(!crib_legal(P, CA_PLAY + 1) && !crib_legal(P, CA_NEXT));
}

static void test_undo(void)
{
    crib_new(&G, 2, 9);
    CHECK(crib_script(&G, "d01 d45 c p0 p0"));
    cpos before = G.pos;
    CHECK(crib_script(&G, "p1"));
    CHECK(crib_undo(&G) && !memcmp(&before, &G.pos, sizeof before));
    while (crib_undo(&G)) {}
    crib_new(&H, 2, 9);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof H.pos) && G.n == 0);
}

/* whole random games: every action legal, the cards all accounted for, the scores only going up, the end at 121 */
static void test_random(int players)
{
    int ended = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        crib_new(&G, players, seed);
        uint32_t r = seed * 7919u;
        int steps = 0, ok = 1;
        int16_t last[CRIB_MAXP] = {0};
        while (P->phase != CP_OVER && steps++ < 3000 && ok) {
            int acts[64], n = 0;
            for (int a = 1; a < 130; a++)
                if (crib_legal(P, a)) acts[n++] = a;
            if (!n) { ok = 0; break; }
            r = r * 1103515245u + 12345u;
            ok = crib_play(&G, acts[(r >> 8) % (unsigned)n]);
            for (int q = 0; q < players; q++) {
                ok &= P->score[q] >= last[q] && P->score[q] <= CRIB_GOAL;
                last[q] = P->score[q];
            }
            if (P->phase == CP_PLAY) {          /* the count never past 31; the cards all there */
                int seen[256] = {0}, cnt = 0;
                ok &= P->count <= 31;
                for (int q = 0; q < players; q++)
                    for (int i = 0; i < 4; i++) seen[P->kept[q][i]]++, cnt++;
                for (int i = 0; i < P->ncrib; i++) seen[P->crib[i]]++, cnt++;
                for (int i = 0; i < P->nstock; i++) seen[P->stock[i]]++, cnt++;
                seen[P->starter]++, cnt++;
                ok &= cnt == 52;
                for (int c = 0; c < 256; c++) ok &= seen[c] <= 1;
            }
        }
        CHECK(ok);
        ended += P->phase == CP_OVER && crib_winner(P) >= 0;
    }
    CHECK(ended == 300);
}

static void test_three(void)
{
    crib_new(&G, 3, 11);
    CHECK(crib_script(&G, "d0 d0 d0") && P->phase == CP_CUT && P->ncrib == 4 && P->turn == 0);
    CHECK(crib_script(&G, "c") && P->phase == CP_PLAY);
    CHECK(crib_script(&G, "#") && P->phase == CP_SHOW);
    CHECK(crib_script(&G, "n n n") && P->shown == 4);
    CHECK(crib_script(&G, "n") && P->dealer == 0 && P->turn == 1);
}

int main(void)
{
    test_deal();
    test_show();
    test_play();
    test_play_more();
    test_go();
    test_hand();
    test_end();
    test_undo();
    test_three();
    test_random(2);
    test_random(3);
    printf("cribbage: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
