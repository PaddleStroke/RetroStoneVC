/*
 * Clock: the rules' test (the deal; the pile whose turn it is, read from the position; each card to its hour; the
 * end at the fourth king, won or lost; undo; whole games from seeds against a plain simulation of the rules; how
 * often it comes out, about one deal in thirteen). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/clock.h"
#include "patience_check.h"

static void test_deal(void)
{
    ck_new(&G, 2);
    ck_new(&H, 2);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && pc_whole(P, 1, 1));
    for (int r = 0; r < 13; r++) CHECK(P->n[CK_UP + r] == 0 && P->n[CK_DOWN + r] == 4 && P->down[CK_DOWN + r] == 4);
    CHECK(ck_current(P) == 13 && !ck_over(P));
}

/* the game as the rules tell it, with a pointer: a plain simulation; 1: won */
static int simulate(uint32_t seed, int *turned)
{
    uint8_t pile[13][4], n[13];
    ck_new(&H, seed);
    for (int r = 0; r < 13; r++) {
        n[r] = 4;
        memcpy(pile[r], H.pos.c[CK_DOWN + r], 4);
    }
    int at = 12;
    *turned = 0;
    while (n[at]) {
        int c = pile[at][--n[at]];
        (*turned)++;
        at = CARD_RANK(c) - 1;
    }
    return *turned == 52;
}

static void test_games(void)
{
    int ok = 1, won = 0;
    for (uint32_t seed = 1; seed <= 2000; seed++) {
        int turned, w = simulate(seed, &turned), k = 0;
        ck_new(&G, seed);
        solpos start = G.pos;
        while (!ck_over(P)) {
            solmove m = ck_next(P), h = ck_rules.hint(P);
            int pile = CK_DOWN + ck_current(P) - 1, card = sol_top(P, pile);
            if (m.kind != SM_MOVE || memcmp(&m, &h, sizeof m) || m.from != pile || !ck_play(&G, m)) { ok = 0; break; }
            if (sol_top(P, CK_UP + CARD_RANK(card) - 1) != card) ok = 0;   /* face up under its hour */
            k++;
        }
        if (k != turned || ck_won(P) != w || ck_next(P).kind != SM_NONE) ok = 0;
        if (P->n[CK_UP + 12] != 4) ok = 0;      /* it always ends with the fourth king */
        if (w && !pc_whole(P, 1, 1)) ok = 0;
        if (w && !won) printf("ok   deal %u comes out (52 cards turned)\n", seed);
        if (seed == 3) printf("ok   deal 3: %d cards turned\n", turned);
        won += w;
        while (sol_undo(&G)) {}
        if (memcmp(&G.pos, &start, sizeof start)) ok = 0;
    }
    CHECK(ok);
    printf("ok   2000 deals played to the end: %d won (about 1 in 13 expected: %d)\n", won, 2000 / 13);
    CHECK(won > 2000 / 13 / 2 && won < 2000 / 13 * 2);
}

static void test_script(void)
{
    ck_new(&G, 2);
    CHECK(ck_script(&G, "s s ? s") && G.moves == 4 && ck_current(P) != 0);
    CHECK(!ck_script(&G, "x"));
    for (int i = 0; i < 60 && !ck_over(P); i++) ck_script(&G, "s");
    CHECK(ck_over(P) && !ck_script(&G, "s"));
    /* a tap anywhere: the next card; nothing can be picked up */
    ck_new(&G, 2);
    CHECK(ck_rules.tap(P, 3, 0, 0).from == CK_DOWN + 12 && !ck_rules.pickable(P, CK_DOWN + 12, 3));
}

int main(void)
{
    test_deal();
    test_games();
    test_script();
    printf("clock: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
