/*
 * Forty Thieves: the rules' test (the deal of two packs; one card at a time, down in suit; any card to an empty
 * column; the eight foundations; the stock once through; the taps; whole random games from seeds; the hint). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/fortythieves.h"
#include "patience_check.h"

static void clear(void)
{
    ft_new(&G, 1);
    for (int i = 0; i < FT_PILES; i++) P->n[i] = P->down[i] = 0;
    P->n[FT_STOCK] = P->down[FT_STOCK] = 1;
}

static void put(int pile, const int *cards, int n)
{
    for (int k = 0; k < n; k++) P->c[pile][k] = (uint8_t)cards[k];
    P->n[pile] = (uint8_t)n;
}

static int legal(int a, int b, int n) { return ft_legal(P, sol_mv(SM_MOVE, a, b, n)); }

static void test_deal(void)
{
    ft_new(&G, 7);
    ft_new(&H, 7);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && pc_whole(P, 2, 1));
    for (int c = 0; c < 10; c++) CHECK(P->n[c] == 4 && P->down[c] == 0);
    CHECK(P->n[FT_STOCK] == 64 && P->down[FT_STOCK] == 64 && P->n[FT_WASTE] == 0);
}

static void test_moves(void)
{
    clear();
    const int a[] = {S(HEARTS, 8), S(SPADES, 9), S(SPADES, 8)}, b[] = {S(SPADES, 10)}, c[] = {S(CLUBS, 10)},
              d[] = {S(HEARTS, 9)};
    put(0, a, 3);
    put(1, b, 1);
    put(2, c, 1);
    put(3, d, 1);
    CHECK(!legal(0, 3, 1) && !legal(0, 1, 1));  /* the 8 of spades: not on a 9 of hearts, not on a 10 */
    CHECK(!legal(0, 1, 2));                     /* never two cards at once (the 9 and 8 of spades on the 10) */
    const int e[] = {S(SPADES, 9)};
    put(5, e, 1);
    CHECK(legal(0, 5, 1) && !legal(5, 0, 1));   /* down in suit */
    CHECK(legal(0, 4, 1));                      /* any card to an empty column */
    CHECK(!legal(FT_STOCK, 0, 1));
}

static void test_more(void)
{
    clear();
    const int a[] = {S(SPADES, 9), S(SPADES, 8)}, b[] = {S(SPADES, 10)}, ace[] = {S(CLUBS, 1)}, ace2[] = {S(CLUBS, 1)},
              two[] = {S(CLUBS, 2)};
    put(0, a, 2);
    put(1, b, 1);
    put(2, ace, 1);
    put(3, ace2, 1);
    put(4, two, 1);
    CHECK(!legal(0, 1, 2) && !legal(0, 1, 1));  /* the 8 of spades is not on a 9 there */
    CHECK(ft_play(&G, sol_mv(SM_MOVE, 0, 5, 1)) && legal(0, 1, 1));
    /* two aces of clubs, two foundations; the 2 goes on either */
    CHECK(ft_found_for(P, S(CLUBS, 1)) == FT_FOUND && ft_play(&G, sol_mv(SM_MOVE, 2, FT_FOUND, 1)));
    CHECK(ft_found_for(P, S(CLUBS, 1)) == FT_FOUND + 1 && ft_play(&G, sol_mv(SM_MOVE, 3, FT_FOUND + 1, 1)));
    CHECK(legal(4, FT_FOUND, 1) && legal(4, FT_FOUND + 1, 1) && !legal(4, FT_FOUND + 2, 1));
    CHECK(!legal(FT_FOUND, 6, 1));              /* a foundation's card stays */
    solmove m = ft_rules.tap(P, 4, 0, 0);
    CHECK(m.kind == SM_MOVE && m.to == FT_FOUND);
}

/* the stock: one card at a time onto the waste, once through, no redeal */
static void test_stock(void)
{
    ft_new(&G, 3);
    uint8_t top = sol_top(P, FT_STOCK);
    CHECK(ft_script(&G, "s") && sol_top(P, FT_WASTE) == top && P->n[FT_STOCK] == 63);
    for (int i = 0; i < 63; i++) CHECK(ft_script(&G, "s"));
    CHECK(P->n[FT_STOCK] == 0 && P->n[FT_WASTE] == 64);
    CHECK(ft_rules.stock(P).kind == SM_NONE && !ft_script(&G, "s"));
    CHECK(!ft_legal(P, sol_mv(SM_REDEAL, FT_WASTE, FT_STOCK, 64)));
}

static int sane(const solpos *p)
{
    if (!pc_whole(p, 2, 1)) return 0;
    for (int f = FT_FOUND; f < FT_STOCK; f++)
        for (int k = 0; k < p->n[f]; k++)
            if (CARD_RANK(p->c[f][k]) != k + 1 || CARD_SUIT(p->c[f][k]) != CARD_SUIT(p->c[f][0])) return 0;
    for (int c = 0; c < 10; c++)
        if (p->down[c]) return 0;
    return p->down[FT_STOCK] == p->n[FT_STOCK] && !p->down[FT_WASTE];
}

static void new_game(solrec *r, uint32_t seed) { ft_new(r, seed); }

int main(void)
{
    test_deal();
    test_moves();
    test_more();
    test_stock();
    int ends, wins;
    pc_random(&ft_rules, new_game, sane, 200, 600, &ends, &wins);
    printf("ok   200 random games (%d with no move left, %d won)\n", ends, wins);
    int won = pc_hinted(&ft_rules, new_game, 200, 2000);
    printf("ok   the hint alone wins %d deals of 200\n", won);
    printf("fortythieves: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
