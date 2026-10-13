/*
 * Canfield: the rules' test (the deal; the base rank; building round the corner; the top card or the whole pile;
 * the reserve filling an empty pile; the stock by three and its redeals; the taps; whole random games from seeds;
 * the hint). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/canfield.h"
#include "games/canfield_levels.h"
#include "levels_check.h"
#include "patience_check.h"

static void put(int pile, int down, const int *cards, int n)
{
    for (int k = 0; k < n; k++) P->c[pile][k] = (uint8_t)cards[k];
    P->n[pile] = (uint8_t)n;
    P->down[pile] = (uint8_t)down;
}

static void clear(int base)
{
    cn_new(&G, 1);
    for (int i = 0; i < CN_PILES; i++) P->n[i] = P->down[i] = 0;
    const int b[] = {base};
    put(CN_FOUND, 0, b, 1);
    P->n[CN_STOCK] = P->down[CN_STOCK] = 1;
}

static int legal(int a, int b, int n) { return cn_legal(P, sol_mv(SM_MOVE, a, b, n)); }

static void test_deal(void)
{
    cn_new(&G, 11);
    cn_new(&H, 11);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && pc_whole(P, 1, 1));
    CHECK(P->n[CN_RESERVE] == 13 && P->down[CN_RESERVE] == 12 && P->n[CN_FOUND] == 1);
    for (int t = 0; t < 4; t++) CHECK(P->n[t] == 1 && P->down[t] == 0);
    CHECK(P->n[CN_STOCK] == 34 && P->n[CN_WASTE] == 0 && cn_base(P) == CARD_RANK(P->c[CN_FOUND][0]));
}

static void test_rules(void)
{
    clear(S(HEARTS, 7));                        /* the base: sevens */
    const int a[] = {S(SPADES, 7)}, b[] = {S(HEARTS, 8)}, c[] = {S(CLUBS, RANK_KING)}, d[] = {S(DIAMONDS, RANK_ACE)},
              e[] = {S(HEARTS, 6)};
    put(0, 0, a, 1);
    put(1, 0, b, 1);
    put(2, 0, c, 1);
    put(3, 0, d, 1);
    put(CN_WASTE, 0, e, 1);
    CHECK(legal(0, CN_FOUND + 1, 1) && !legal(3, CN_FOUND + 1, 1));   /* a seven starts a foundation, not an ace */
    CHECK(legal(1, CN_FOUND, 1) && !legal(0, CN_FOUND, 1));           /* the 8 of hearts on the 7 of hearts */
    CHECK(legal(2, 3, 1) && !legal(3, 2, 1));   /* round the corner: a black king on a red ace */
    CHECK(legal(0, 1, 1) && !legal(CN_WASTE, 1, 1));   /* down in alternating colours */
    CHECK(cn_found_for(P, S(SPADES, 7)) == CN_FOUND + 1 && cn_found_for(P, S(HEARTS, 8)) == CN_FOUND);
    CHECK(cn_play(&G, sol_mv(SM_MOVE, 0, 1, 1)));
    /* the 8 of hearts and the 7 of spades: the whole pile moves, or its top card, nothing between */
    const int f[] = {S(CLUBS, 9)};
    put(0, 0, f, 1);
    CHECK(legal(1, 0, 2) && legal(1, CN_FOUND + 1, 1) && !legal(1, CN_FOUND, 2));
    const int g[] = {S(DIAMONDS, 10), S(CLUBS, 9), S(HEARTS, 8)}, h[] = {S(SPADES, 9)};
    put(0, 0, g, 3);
    put(2, 0, h, 1);
    CHECK(legal(0, 2, 1) && !legal(0, 2, 2) && !legal(0, 2, 3));   /* its top card, not two of three */
    CHECK(cn_rules.tap(P, 0, 1, 0).kind == SM_NONE);   /* the middle of a pile: nothing */
    /* a pile left empty: the reserve's top card fills it at once, and the next one turns up */
    clear(S(HEARTS, 7));
    const int r[] = {S(CLUBS, 2), S(SPADES, 3), S(DIAMONDS, 4)}, t0[] = {S(HEARTS, 8)};
    put(CN_RESERVE, 2, r, 3);
    put(0, 0, t0, 1);
    put(1, 0, t0, 1);
    put(2, 0, t0, 1);
    put(3, 0, t0, 1);
    G.start = G.pos;
    CHECK(cn_play(&G, sol_mv(SM_MOVE, 0, CN_FOUND, 1)));
    CHECK(P->n[0] == 1 && P->c[0][0] == S(DIAMONDS, 4) && P->n[CN_RESERVE] == 2 && P->down[CN_RESERVE] == 1);
    CHECK(G.moves == 1 && G.n == 2 && sol_undo(&G) && P->n[CN_RESERVE] == 3);
}

/* the stock: three cards at a time (fewer at its end), the waste back as often as you like */
static void test_stock(void)
{
    cn_new(&G, 5);
    uint8_t stock[34];
    memcpy(stock, P->c[CN_STOCK], 34);
    CHECK(cn_script(&G, "s") && P->n[CN_WASTE] == 3 && sol_top(P, CN_WASTE) == stock[31]);
    for (int i = 0; i < 11; i++) CHECK(cn_script(&G, "s"));
    CHECK(P->n[CN_STOCK] == 0 && P->n[CN_WASTE] == 34 && sol_top(P, CN_WASTE) == stock[0]);
    for (int round = 0; round < 5; round++) {
        CHECK(cn_script(&G, "s") && P->n[CN_STOCK] == 34 && !memcmp(P->c[CN_STOCK], stock, 34));
        for (int i = 0; i < 12; i++) cn_script(&G, "s");
    }
}

static int sane(const solpos *p)
{
    if (!pc_whole(p, 1, 1)) return 0;
    int base = cn_base(p);
    for (int f = CN_FOUND; f < CN_STOCK; f++)
        for (int k = 0; k < p->n[f]; k++)
            if ((CARD_RANK(p->c[f][k]) - base + 13) % 13 != k || CARD_SUIT(p->c[f][k]) != CARD_SUIT(p->c[f][0]))
                return 0;
    for (int t = 0; t < 4; t++) {
        if (p->down[t] || (!p->n[t] && p->n[CN_RESERVE])) return 0;
        for (int k = 1; k < p->n[t]; k++)
            if (CARD_RANK(p->c[t][k]) % 13 + 1 != CARD_RANK(p->c[t][k - 1]) ||
                sol_red(p->c[t][k]) == sol_red(p->c[t][k - 1]))
                return 0;
    }
    return !p->n[CN_RESERVE] || p->down[CN_RESERVE] == p->n[CN_RESERVE] - 1;
}

static void new_game(solrec *r, uint32_t seed) { cn_new(r, seed); }

/* ---- the levels: a deal from its cards is the shuffled deal; every level won by the search ---------------------- */
static void test_levels(void)
{
    static solrec A, B;
    uint8_t deck[104];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, 77);
    cn_new(&A, 77);
    cn_deal_cards(&B, A.variant, deck);
    CHECK(!memcmp(&A.pos, &B.pos, sizeof A.pos));
    CHECK(lc_levels("canfield", cn_levels, CN_LEVELS, 52, cn_deal_cards, &cn_search, 300000) == CN_LEVELS);
}

int main(void)
{
    test_deal();
    test_rules();
    test_stock();
    int ends, wins;
    pc_random(&cn_rules, new_game, sane, 300, 500, &ends, &wins);
    printf("ok   300 random games (%d with no move left, %d won)\n", ends, wins);
    int won = pc_hinted(&cn_rules, new_game, 200, 800);
    printf("ok   the hint alone wins %d deals of 200\n", won);
    test_levels();
    printf("canfield: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
