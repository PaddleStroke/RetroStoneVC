/*
 * FreeCell: the rules' test (the numbered deals against the well-known deal 1; the moves' legality, the free cells,
 * the foundations, the runs' limits through free cells and empty columns; the cards going up by themselves; undo;
 * whole random games from seeds, and games played by the hint alone). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/freecell.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static solrec G;
static solpos *P = &G.pos;

#define S(s, r) CARD(SUIT_##s, r)

static void name(int card, char *out)
{
    out[0] = "?A23456789TJQK"[CARD_RANK(card)];
    out[1] = "SHDC"[CARD_SUIT(card)];
    out[2] = 0;
}

/* deal 1, as everyone knows it: row by row across the eight columns */
static void test_deal(void)
{
    static const char *const one =
        "JD 2D 9H JC 5D 7H 7C 5H "
        "KD KC 9S 5S AD QC KH 3H "
        "2S KS 9D QD JS AS AH 3C "
        "4C 5C TS QH 4H AC 4D 7S "
        "3S TD 4S TH 8H 2C JH 7D "
        "6D 8S 8D QS 6C 3D 8C TC "
        "6S 9C 2H 6H";
    uint8_t d[52];
    fc_deal(1, d);
    char got[52 * 3 + 1] = "", c[3];
    for (int i = 0; i < 52; i++) {
        name(d[i], c);
        strcat(got, c);
        strcat(got, i < 51 ? " " : "");
    }
    CHECK(!strcmp(got, one));
    if (strcmp(got, one)) printf("deal 1: %s\n", got);
    fc_new(&G, 1);
    CHECK(G.seed == 1 && P->n[0] == 7 && P->n[3] == 7 && P->n[4] == 6 && P->n[7] == 6);
    CHECK(P->c[0][0] == S(DIAMONDS, 11) && sol_top(P, 0) == S(SPADES, 6) && sol_top(P, 7) == S(CLUBS, 10));
    int seen[256] = {0}, all = 1;
    for (int i = 0; i < 8; i++)
        for (int k = 0; k < P->n[i]; k++) seen[P->c[i][k]]++;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) all &= seen[CARD(s, r)] == 1;
    CHECK(all);
    fc_deal(11982, d);                          /* the one deal of the 32000 that cannot be solved: still a deal */
    fc_new(&G, 11982);
    CHECK(G.seed == 11982 && P->c[0][0] == d[0]);
    CHECK(fc_script(&G, "#617") && G.seed == 617 && G.n == 0);
}

static void clear(void)
{
    fc_new(&G, 1);
    for (int i = 0; i < FC_PILES; i++) P->n[i] = 0;
}

static void put(int pile, const int *cards, int n)
{
    for (int k = 0; k < n; k++) P->c[pile][k] = (uint8_t)cards[k];
    P->n[pile] = (uint8_t)n;
}

static int legal(int a, int b, int n) { return fc_legal(P, sol_mv(SM_MOVE, a, b, n)); }

static void test_moves(void)
{
    clear();
    const int a[] = {S(SPADES, 8)}, b[] = {S(HEARTS, 7)}, c[] = {S(DIAMONDS, 8)}, k[] = {S(CLUBS, 4)};
    put(0, a, 1);
    put(1, b, 1);
    put(2, c, 1);
    put(3, k, 1);
    CHECK(legal(1, 0, 1) && !legal(1, 2, 1) && !legal(0, 2, 1));
    CHECK(legal(3, 4, 1));                      /* any card to an empty column */
    CHECK(legal(3, FC_CELL, 1) && legal(3, FC_CELL + 3, 1));
    CHECK(fc_play(&G, sol_mv(SM_MOVE, 3, FC_CELL, 1)));
    CHECK(!legal(0, FC_CELL, 1));               /* one card a cell */
    CHECK(!legal(FC_CELL, FC_CELL + 1, 1));     /* not from cell to cell */
    CHECK(legal(FC_CELL, 5, 1) && legal(FC_CELL, 0, 1) == 0);
    /* the foundations: up by suit from the ace; a card there stays */
    const int ace[] = {S(CLUBS, 1)}, two[] = {S(CLUBS, 2)}, two2[] = {S(HEARTS, 2)};
    put(5, ace, 1);
    put(6, two, 1);
    put(7, two2, 1);
    CHECK(!legal(6, FC_FOUND, 1) && legal(5, FC_FOUND + 1, 1));
    CHECK(fc_play(&G, sol_mv(SM_MOVE, 5, FC_FOUND + 1, 1)));
    CHECK(P->n[FC_FOUND + 1] == 2 && P->n[6] == 0);   /* the ace went up, and the two after it by itself */
    CHECK(!legal(7, FC_FOUND + 1, 1));
    CHECK(!legal(FC_FOUND + 1, 4, 1) && !legal(FC_FOUND + 1, FC_CELL + 1, 1));
}

/* runs: (free cells + 1) x 2^(empty columns), the column it goes to not counted */
static void test_supermoves(void)
{
    clear();
    int run[13], n = 0;
    for (int r = 13; r >= 1; r--) run[n++] = CARD(r % 2 ? SUIT_HEARTS : SUIT_SPADES, r);   /* K..A alternating */
    put(0, run, 13);
    const int t[] = {S(CLUBS, 8)}, filler[] = {S(DIAMONDS, 12)};
    put(1, t, 1);                               /* takes the run from the red 7 (7 cards) */
    for (int i = 2; i < 8; i++) put(i, filler, 1);
    CHECK(fc_most(P, 1) == 5);                  /* four free cells, no empty column */
    CHECK(!legal(0, 1, 7) && !legal(0, 1, 6));  /* the 7..A run: 7 cards */
    const int t6[] = {S(DIAMONDS, 7)};          /* a red 7 to take a 6..A run (6 cards) */
    put(2, t6, 1);
    CHECK(!legal(0, 2, 6) && legal(0, 2, 5) == 0);   /* 6 cards from the black 6: too many with 5 */
    P->n[7] = 0;                                /* an empty column: 5 x 2 = 10 */
    CHECK(fc_most(P, 1) == 10 && fc_most(P, 7) == 5);
    CHECK(legal(0, 1, 7) && !legal(0, 1, 8));
    CHECK(legal(0, 7, 5) && !legal(0, 7, 6));  /* to the empty column itself: 5 */
    P->c[FC_CELL][0] = S(DIAMONDS, 13), P->n[FC_CELL] = 1;
    P->c[FC_CELL + 1][0] = S(CLUBS, 13), P->n[FC_CELL + 1] = 1;
    CHECK(fc_most(P, 1) == 6 && fc_most(P, 7) == 3);  /* two cells: (2 + 1) x 2, (2 + 1) x 1 */
    CHECK(legal(0, 1, 6) == 0 && !legal(0, 1, 7));
    P->n[6] = 0;                                /* two empty columns: 3 x 4 */
    CHECK(fc_most(P, 1) == 12 && fc_most(P, 6) == 6);
    CHECK(fc_play(&G, sol_mv(SM_MOVE, 0, 1, 7)) && P->n[0] == 6);
    CHECK(P->n[1] == 7 && sol_top(P, 1) == S(SPADES, 2) && sol_found_rank(P, SUIT_HEARTS) == 1);   /* the ace went up */
    /* the tap: the whole run that fits */
    solmove m = fc_rules.tap(P, 0, 4, 0);
    CHECK(m.kind == SM_MOVE && m.n == 2);
}

static uint32_t rng = 1;
static int rnd(int n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (int)(rng % (uint32_t)n);
}

static int all_moves(const solpos *p, solmove *out)
{
    int k = 0;
    for (int a = 0; a < FC_PILES; a++)
        for (int b = 0; b < FC_PILES; b++)
            for (int n = 1; n <= p->n[a]; n++)
                if (fc_legal(p, sol_mv(SM_MOVE, a, b, n))) out[k++] = sol_mv(SM_MOVE, a, b, n);
    return k;
}

static int sane(const solpos *p)
{
    int seen[256] = {0}, n = 0;
    for (int i = 0; i < FC_PILES; i++) {
        if (p->kind[i] == SP_CELL && p->n[i] > 1) return 0;
        for (int k = 0; k < p->n[i]; k++) seen[p->c[i][k]]++, n++;
        if (p->kind[i] == SP_FOUND)
            for (int k = 0; k < p->n[i]; k++)
                if (CARD_RANK(p->c[i][k]) != k + 1 || CARD_SUIT(p->c[i][k]) != CARD_SUIT(p->c[i][0])) return 0;
    }
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (seen[CARD(s, r)] != 1) return 0;
    return n == 52;
}

static void test_random(void)
{
    solmove list[1024];
    int ok = 1, won = 0, first_won = 0;
    for (int deal = 1; deal <= 300; deal++) {
        rng = (uint32_t)deal * 7919u;
        fc_new(&G, deal);
        solpos start = G.pos;
        int by_hint = deal % 3 == 0;            /* a third of them played by the hint alone */
        for (int step = 0; step < 500 && !fc_won(P); step++) {
            solmove h = fc_hint(P);
            if (h.kind != SM_NONE && !fc_legal(P, h)) ok = 0;
            int n = all_moves(P, list);
            if (!n || (by_hint && h.kind == SM_NONE)) break;
            if (!fc_play(&G, by_hint ? h : list[rnd(n)])) ok = 0;
            if (!sane(P)) { ok = 0; printf("deal %d step %d: not sane\n", deal, step); break; }
        }
        if (fc_won(P) && by_hint && !first_won)
            first_won = deal, printf("ok   deal %d won by the hint alone in %d moves\n", deal, G.moves);
        won += fc_won(P);
        while (sol_undo(&G)) {}
        if (memcmp(&G.pos, &start, sizeof start) || G.n || G.moves) ok = 0;
    }
    CHECK(ok);
    printf("ok   300 random games (%d won)\n", won);
}

/* a game won: the last cards up by themselves */
static void test_won(void)
{
    clear();
    for (int s = 0; s < 4; s++) {
        for (int r = 1; r <= 12; r++) P->c[FC_FOUND + s][r - 1] = (uint8_t)CARD(s, r);
        P->n[FC_FOUND + s] = 12;
    }
    const int kings[] = {S(SPADES, 13), S(HEARTS, 13)}, k2[] = {S(DIAMONDS, 13)}, k3[] = {S(CLUBS, 13)};
    put(0, kings, 2);
    put(1, k2, 1);
    put(FC_CELL, k3, 1);
    G.start = G.pos;
    G.n = G.moves = 0;
    CHECK(!fc_won(P) && fc_script(&G, "1h"));
    CHECK(fc_won(P) && G.moves == 1 && G.n == 4);
    CHECK(sol_undo(&G) && !fc_won(P) && G.n == 0);
}

int main(void)
{
    test_deal();
    test_moves();
    test_supermoves();
    test_random();
    test_won();
    printf("freecell: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
