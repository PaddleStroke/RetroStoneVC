/*
 * Baker's Dozen: the rules' test (the deal, its kings at the bottom; one top card at a time onto any card a rank
 * higher; empty columns staying empty; the foundations; the taps; the notation; the end going up by itself; whole
 * random games from seeds; the hint; the levels). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/bakersdozen.h"
#include "games/bakersdozen_levels.h"
#include "levels_check.h"
#include "patience_check.h"

static void clear(void)
{
    bkd_new(&G, 1);
    for (int i = 0; i < BKD_PILES; i++) P->n[i] = P->down[i] = 0;
}

static void put(int pile, const int *cards, int n)
{
    for (int k = 0; k < n; k++) P->c[pile][k] = (uint8_t)cards[k];
    P->n[pile] = (uint8_t)n;
}

static int legal(int a, int b) { return bkd_legal(P, sol_mv(SM_MOVE, a, b, 1)); }

static void test_deal(void)
{
    bkd_new(&G, 42);
    bkd_new(&H, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && pc_whole(P, 1, 1));
    for (int c = 0; c < 13; c++) {
        CHECK(P->n[c] == 4 && P->down[c] == 0);
        int seen_other = 0;                     /* the kings under the others */
        for (int k = 0; k < 4; k++) {
            if (CARD_RANK(P->c[c][k]) != RANK_KING) seen_other = 1;
            else CHECK(!seen_other);
        }
    }
    /* the others keep their order: a column dealt queen, king, 2, king is king, king, queen, 2 */
    uint8_t deck[52];
    int col0[4] = {S(HEARTS, 12), S(SPADES, 13), S(CLUBS, 2), S(HEARTS, 13)};
    cards_pack(PACK_52, deck);
    for (int r = 0; r < 4; r++)                 /* column 0's cards at places 0, 13, 26, 39: swapped in */
        for (int i = 0; i < 52; i++)
            if (deck[i] == col0[r]) { deck[i] = deck[13 * r]; deck[13 * r] = (uint8_t)col0[r]; break; }
    bkd_deal_cards(&G, 0, deck);
    CHECK(P->c[0][0] == S(SPADES, 13) && P->c[0][1] == S(HEARTS, 13) && P->c[0][2] == S(HEARTS, 12) &&
          P->c[0][3] == S(CLUBS, 2) && pc_whole(P, 1, 1));
}

static void test_moves(void)
{
    clear();
    const int a[] = {S(HEARTS, 5), S(SPADES, 9)}, b[] = {S(DIAMONDS, 10)}, c[] = {S(CLUBS, 10)},
              d[] = {S(HEARTS, RANK_ACE)}, e[] = {S(CLUBS, 6)};
    put(0, a, 2);
    put(1, b, 1);
    put(2, c, 1);
    put(3, d, 1);
    put(12, e, 1);
    CHECK(legal(0, 1) && legal(0, 2));          /* the 9 on any 10 */
    CHECK(!bkd_legal(P, sol_mv(SM_MOVE, 0, 1, 2)));   /* one card at a time */
    CHECK(!legal(1, 0) && !legal(0, 4));        /* not a 10 on a 9; never to an empty column */
    CHECK(legal(3, BKD_FOUND) && !legal(0, BKD_FOUND));
    CHECK(bkd_script(&G, "4h") && P->n[3] == 0 && P->n[BKD_FOUND] == 1);
    CHECK(!legal(BKD_FOUND, 3) && !legal(BKD_FOUND, 0));   /* a card up stays up */
    CHECK(bkd_script(&G, "12") && sol_top(P, 1) == S(SPADES, 9) && sol_top(P, 0) == S(HEARTS, 5));
    CHECK(!bkd_script(&G, "z1") && bkd_script(&G, "1z") && sol_top(P, 12) == S(HEARTS, 5));   /* the 13th: z */
    /* the tap: up first, else onto a card a rank higher (burying the fewest low cards) */
    clear();
    const int f[] = {S(SPADES, 2), S(DIAMONDS, 10)}, g[] = {S(CLUBS, 10)}, h[] = {S(HEARTS, 9)},
              ace[] = {S(SPADES, RANK_ACE)};
    put(0, f, 2);
    put(1, g, 1);
    put(2, h, 1);
    put(3, ace, 1);
    solmove m = bkd_rules.tap(P, 2, 0, 0);
    CHECK(m.kind == SM_MOVE && m.to == 1);      /* the 10 of clubs: no 2 buried under it */
    m = bkd_rules.tap(P, 3, 0, 1);
    CHECK(m.kind == SM_MOVE && P->kind[m.to] == SP_FOUND);
    CHECK(bkd_rules.tap(P, 2, 0, 1).kind == SM_NONE && bkd_rules.tap(P, 0, 0, 0).kind == SM_NONE);
}

/* the cards left can all go up in turn: up by themselves; undo takes them back with the move */
static void test_finish(void)
{
    clear();
    for (int s = 0; s < 4; s++)
        for (int r = 13; r >= 1; r--) P->c[s][P->n[s]++] = (uint8_t)CARD(s, r);
    P->c[4][P->n[4]++] = P->c[0][--P->n[0]];    /* an ace on its own column */
    G.start = G.pos;
    G.n = G.moves = 0;
    CHECK(pc_whole(P, 1, 1));
    CHECK(bkd_script(&G, "5h") && bkd_won(P) && G.moves == 1 && G.n == 52);
    CHECK(sol_undo(&G) && !memcmp(&G.pos, &G.start, sizeof G.pos));
}

static int sane(const solpos *p)
{
    if (!pc_whole(p, 1, 1)) return 0;
    for (int f = BKD_FOUND; f < BKD_PILES; f++)
        for (int k = 0; k < p->n[f]; k++)
            if (CARD_RANK(p->c[f][k]) != k + 1 || CARD_SUIT(p->c[f][k]) != CARD_SUIT(p->c[f][0])) return 0;
    for (int c = 0; c < BKD_FOUND; c++)
        if (p->down[c]) return 0;
    return 1;
}

static void new_game(solrec *r, uint32_t seed) { bkd_new(r, seed); }

static void test_levels(void)
{
    static solrec A, B;
    uint8_t deck[104];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, 77);
    bkd_new(&A, 77);
    bkd_deal_cards(&B, A.variant, deck);
    CHECK(!memcmp(&A.pos, &B.pos, sizeof A.pos));
    CHECK(lc_levels("bakersdozen", bkd_levels, BKD_LEVELS, 52, bkd_deal_cards, &bkd_search, 60000) == BKD_LEVELS);
}

int main(void)
{
    test_deal();
    test_moves();
    test_finish();
    int ends, wins;
    pc_random(&bkd_rules, new_game, sane, 300, 500, &ends, &wins);
    printf("ok   300 random games (%d with no move left, %d won)\n", ends, wins);
    int won = pc_hinted(&bkd_rules, new_game, 200, 1500);
    printf("ok   the hint alone wins %d deals of 200\n", won);
    test_levels();
    printf("bakersdozen: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
