/*
 * Golf: the rules' test (the deal from a seed, the scoring of both variants: column pairs, columns of three, the
 * doubling of the round's ender; the end of a round, the end of a game, illegal actions, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/golf.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static ggame G, H;
static gpos *P = &G.pos;

/* the deal: the same seed, the same cards; every card of the pack once */
static void test_deal(void)
{
    golf_new(&G, GOLF_CLASSIC, 4, 42);
    golf_new(&H, GOLF_CLASSIC, 4, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(P->rows == 2 && P->cols == 3 && P->slots == 6 && P->phase == GP_REVEAL && P->turn == 0);
    CHECK(P->nstock == 52 - 24 - 1 && P->npile == 1);
    int seen[256] = {0}, n = 0;
    for (int pl = 0; pl < 4; pl++)
        for (int s = 0; s < 6; s++) seen[P->card[pl][s]]++, n++;
    for (int i = 0; i < P->nstock; i++) seen[P->stock[i]]++, n++;
    seen[P->pile[0]]++, n++;
    int once = 1;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++) once &= seen[CARD(s, r)] == 1;
    CHECK(n == 52 && once && P->up[0] == 0);
    golf_new(&H, GOLF_CLASSIC, 4, 43);
    CHECK(memcmp(H.pos.card, G.pos.card, sizeof G.pos.card) != 0);
    golf_new(&G, GOLF_CLASSIC, 6, 7);           /* five or six players: two packs */
    CHECK(P->nstock == 104 - 36 - 1);
    golf_new(&G, GOLF_NUMBER, 6, 42);           /* number golf: 150 cards, -2 x5, -1 x10, 0 x15, 1..12 x10 */
    CHECK(P->rows == 3 && P->cols == 4 && P->slots == 12 && P->nstock == 150 - 72 - 1);
    int count[15] = {0};
    for (int pl = 0; pl < 6; pl++)
        for (int s = 0; s < 12; s++) count[NUMBER_VALUE(P->card[pl][s]) + 2]++;
    for (int i = 0; i < P->nstock; i++) count[NUMBER_VALUE(P->stock[i]) + 2]++;
    count[NUMBER_VALUE(P->pile[0]) + 2]++;
    int ok = count[0] == 5 && count[1] == 10 && count[2] == 15;
    for (int v = 1; v <= 12; v++) ok &= count[v + 2] == 10;
    CHECK(ok);
}

static void grid(int pl, const int *cards, unsigned up)
{
    for (int s = 0; s < P->slots; s++) P->card[pl][s] = (uint8_t)cards[s];
    P->up[pl] = (uint16_t)up;
    P->gone[pl] = 0;
}

/* classic: ace 1, jack and queen 10, king 0; a column of two of the same rank scores 0 */
static void test_classic_score(void)
{
    golf_new(&G, GOLF_CLASSIC, 2, 1);
    const int a[6] = {CARD(0, 13), CARD(1, 5), CARD(2, 12), CARD(3, 13), CARD(0, 5), CARD(1, 11)};
    grid(0, a, 0x3f);
    CHECK(golf_score(P, 0) == 20);              /* kings: 0, fives: 0, queen and jack: 20 */
    const int b[6] = {CARD(0, 1), CARD(1, 7), CARD(2, 10), CARD(3, 1), CARD(0, 8), CARD(1, 12)};
    grid(1, b, 0x3f);
    CHECK(golf_score(P, 1) == 0 + 15 + 20);     /* the aces pair; 7 + 8; 10 + queen */
    grid(1, b, 0x01 | 0x02);                    /* face up only: the ace and the 7 */
    CHECK(golf_score(P, 1) == 8 && golf_down(P, 1) == 4);
    CHECK(golf_value(P, CARD(2, 1)) == 1 && golf_value(P, CARD(2, 13)) == 0 && golf_value(P, CARD(2, 11)) == 10);
}

/* number golf: a column of three of one value goes to the discard as soon as it shows */
static void test_columns(void)
{
    golf_new(&G, GOLF_NUMBER, 2, 3);
    const int a[12] = {NUMBER_CARD(5), NUMBER_CARD(1), NUMBER_CARD(2), NUMBER_CARD(3),
                       NUMBER_CARD(5), NUMBER_CARD(2), NUMBER_CARD(3), NUMBER_CARD(4),
                       NUMBER_CARD(9), NUMBER_CARD(3), NUMBER_CARD(4), NUMBER_CARD(6)};
    grid(0, a, 0x0ff);
    P->phase = GP_DRAW;
    P->turn = 0;
    P->stock[P->nstock - 1] = NUMBER_CARD(5);
    int pile = P->npile;
    CHECK(golf_script(&G, "s w8"));
    CHECK(P->gone[0] == (1 | 1 << 4 | 1 << 8));
    CHECK(P->npile == pile + 4 && NUMBER_VALUE(P->pile[P->npile - 1]) == 5 && NUMBER_VALUE(P->pile[pile]) == 9);
    CHECK(golf_score(P, 0) == 1 + 2 + 3 + 2 + 3 + 4);   /* the columns left that show: 1 2 3, 2 3 4 */
    CHECK(P->turn == 1 && P->phase == GP_DRAW);
    CHECK(!golf_legal(P, GA_SWAP | 0));         /* not a turn's start: draw first */
}

/* number golf's round end: the ender (all cards up) doubles unless strictly the lowest (and above zero) */
static int round_with(int ender_cards_low, int tie)
{
    static const int lo[12] = {0, 0, 1, 1, 1, 1, 0, 0, 2, 2, 2, 3};          /* 13 */
    static const int hi[12] = {1, 2, 3, 4, 2, 3, 4, 5, 0, 0, 0, 1};          /* 25 */
    int a[12], b[12];
    for (int s = 0; s < 12; s++) {
        a[s] = NUMBER_CARD(ender_cards_low ? lo[s] : hi[s]);
        b[s] = NUMBER_CARD(tie ? hi[s] : ender_cards_low ? hi[s] : lo[s]);
    }
    golf_new(&G, GOLF_NUMBER, 2, 5);
    grid(0, a, 0x7ff);                          /* all up but the last */
    grid(1, b, 0xffe);                          /* all up but the first */
    P->phase = GP_DRAW;
    P->turn = 0;
    P->stock[P->nstock - 1] = (uint8_t)a[11];   /* the same card again: the sums stay */
    P->stock[P->nstock - 2] = (uint8_t)b[0];
    CHECK(golf_script(&G, "s w11"));
    CHECK(P->ender == 0 && P->turn == 1 && P->phase == GP_DRAW);    /* the other player's last turn */
    CHECK(golf_script(&G, "s w0"));
    CHECK(P->phase == GP_ROUND && P->round == 1);
    return P->last[0];
}

static void test_doubling(void)
{
    CHECK(round_with(0, 0) == 50 && P->doubled && P->last[1] == 13 && P->total[0] == 50 && P->total[1] == 13);
    CHECK(round_with(1, 0) == 13 && !P->doubled && P->last[1] == 25);  /* strictly the lowest: as it is */
    CHECK(round_with(0, 1) == 50 && P->doubled);                       /* a tie for the lowest: doubled */
    /* zero or less: never doubled */
    golf_new(&G, GOLF_NUMBER, 2, 5);
    static const int z[12] = {-2, -1, 0, 1, -2, -1, 0, 1, 0, 1, -1, -2},  /* -6 */
                     y[12] = {-2, -2, -2, -2, -1, -1, -1, -1, -2, -2, -2, -2};   /* -20 */
    int a[12], b[12];
    for (int s = 0; s < 12; s++) a[s] = NUMBER_CARD(z[s]), b[s] = NUMBER_CARD(y[s]);
    grid(0, a, 0x7ff);
    grid(1, b, 0xffe);
    P->phase = GP_DRAW;
    P->stock[P->nstock - 1] = (uint8_t)a[11];
    P->stock[P->nstock - 2] = (uint8_t)b[0];
    CHECK(golf_script(&G, "s w11 s w0"));
    CHECK(P->phase == GP_ROUND && P->last[0] == -6 && P->last[1] == -20 && !P->doubled);
}

/* the turn: draw, then swap or discard and turn one up; a card taken from the discard must be swapped */
static void test_turn(void)
{
    golf_new(&G, GOLF_CLASSIC, 3, 11);
    CHECK(!golf_legal(P, GA_STOCK) && golf_legal(P, GA_REVEAL | 2));
    CHECK(golf_script(&G, "r0 r4"));
    CHECK(P->turn == 1 && P->phase == GP_REVEAL && P->up[0] == (1 | 1 << 4));
    CHECK(!golf_script(&G, "r0 r0"));           /* the same card twice: no */
    golf_new(&G, GOLF_CLASSIC, 3, 11);
    CHECK(golf_script(&G, "r0 r1 r0 r1 r0 r1"));
    CHECK(P->phase == GP_DRAW && P->turn == 0);
    uint8_t top = P->pile[P->npile - 1], old = P->card[0][5];
    CHECK(golf_script(&G, "t") && P->held == top && !golf_legal(P, GA_DISCARD));
    CHECK(golf_script(&G, "w5") && P->card[0][5] == top && P->pile[P->npile - 1] == old && (P->up[0] >> 5 & 1));
    CHECK(P->turn == 1 && P->phase == GP_DRAW);
    uint8_t drawn = P->stock[P->nstock - 1];
    CHECK(golf_script(&G, "s d") && P->pile[P->npile - 1] == drawn && P->phase == GP_FLIP);
    CHECK(!golf_legal(P, GA_FLIP | 0) && golf_script(&G, "f3") && P->turn == 2);
}

/* the round: once a player is all face up, the others play once more; then the next round, the next first player */
static void test_round(void)
{
    golf_new(&G, GOLF_CLASSIC, 3, 21);
    CHECK(golf_script(&G, "r0 r1 r0 r1 r0 r1"));
    CHECK(golf_script(&G, "s w2 s w2 s w2 s w3 s w3 s w3 s w4 s w4 s w4"));
    CHECK(P->phase == GP_DRAW && P->turn == 0 && P->ender == GOLF_NONE);
    CHECK(golf_script(&G, "s w5"));
    CHECK(P->ender == 0 && P->turn == 1 && P->phase == GP_DRAW);
    CHECK(golf_script(&G, "s d f5"));
    CHECK(P->turn == 2 && P->phase == GP_DRAW);
    CHECK(golf_script(&G, "s w5"));
    CHECK(P->phase == GP_ROUND && P->round == 1 && P->up[1] == 0x3f && P->up[2] == 0x3f);
    for (int pl = 0; pl < 3; pl++) CHECK(P->total[pl] == golf_score(P, pl) && P->last[pl] == P->total[pl]);
    CHECK(!golf_legal(P, GA_STOCK) && golf_script(&G, "n"));
    CHECK(P->phase == GP_REVEAL && P->first == 1 && P->turn == 1 && P->up[0] == 0 && P->round == 1);
}

/* number golf's first player: the highest sum showing after the reveal */
static void test_first(void)
{
    golf_new(&G, GOLF_NUMBER, 3, 9);
    CHECK(golf_script(&G, "r0 r1 r0 r1 r0 r1"));
    int best = 0;
    for (int pl = 1; pl < 3; pl++)
        if (golf_score(P, pl) > golf_score(P, best)) best = pl;
    CHECK(P->phase == GP_DRAW && P->turn == best);
}

/* whole games: nine holes; number golf to 100; the lowest total wins */
static void test_game(void)
{
    golf_new(&G, GOLF_CLASSIC, 4, 77);
    int rounds = 0;
    while (P->phase != GP_OVER && rounds < 20) {
        CHECK(golf_script(&G, "#"));
        rounds++;
        if (P->phase == GP_ROUND) CHECK(golf_script(&G, "n"));
    }
    CHECK(P->phase == GP_OVER && P->round == GOLF_HOLES && rounds == GOLF_HOLES);
    int w = golf_winner(P);
    for (int pl = 0; pl < 4 && w >= 0; pl++) CHECK(pl == w || P->total[pl] > P->total[w]);
    golf_new(&G, GOLF_NUMBER, 5, 78);
    rounds = 0;
    while (P->phase != GP_OVER && rounds < 60) {
        CHECK(golf_script(&G, "#"));
        rounds++;
        if (P->phase == GP_ROUND) {
            for (int pl = 0; pl < 5; pl++) CHECK(P->total[pl] < GOLF_LIMIT);
            CHECK(golf_script(&G, "n"));
        }
    }
    int top = -1000;
    for (int pl = 0; pl < 5; pl++) top = P->total[pl] > top ? P->total[pl] : top;
    CHECK(P->phase == GP_OVER && top >= GOLF_LIMIT);
    CHECK(!golf_legal(P, GA_NEXT) && !golf_legal(P, GA_STOCK));
    w = golf_winner(P);
    for (int pl = 0; pl < 5 && w >= 0; pl++) CHECK(pl == w || P->total[pl] > P->total[w]);
    printf("ok   number golf, 5 players: %d rounds, the winner %d\n", rounds, w);
}

/* undo: back one action at a time, all the way to the deal */
static void test_undo(void)
{
    golf_new(&G, GOLF_CLASSIC, 3, 99);
    golf_new(&H, GOLF_CLASSIC, 3, 99);
    CHECK(golf_script(&G, "r0 r1 r0 r1 r0 r1 # n r2 r3"));
    gpos mid = G.pos;
    CHECK(golf_script(&G, "r2 r3 r2 r3 s"));
    for (int i = 0; i < 5; i++) CHECK(golf_undo(&G));
    CHECK(!memcmp(&G.pos, &mid, sizeof mid));
    while (golf_undo(&G)) {}
    CHECK(G.n == 0 && !memcmp(&G.pos, &H.pos, sizeof H.pos));
}

int main(void)
{
    test_deal();
    test_classic_score();
    test_columns();
    test_doubling();
    test_turn();
    test_round();
    test_first();
    test_game();
    test_undo();
    printf("golf: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
