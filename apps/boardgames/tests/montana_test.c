/*
 * Montana: the rules' test (the deal, the aces out; a gap filled by the card following its left neighbour, a 2 in the
 * first column, nothing after a king; the rows' runs; the three redeals, a gap after each run, the same again from the
 * same seed; the end; the notation; whole random games from seeds; the hint). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/montana.h"
#include "patience_check.h"

static void set(int place, int card)
{
    P->n[place] = card != CARD_NONE;
    P->c[place][0] = (uint8_t)card;
}

static int legal(int a, int b) { return mt_legal(P, sol_mv(SM_MOVE, a, b, 1)); }

static void test_deal(void)
{
    mt_new(&G, 4);
    mt_new(&H, 4);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && pc_whole(P, 1, 0));
    int gaps = 0;
    for (int i = 0; i < 52; i++) gaps += !P->n[i];
    CHECK(gaps == 4 && P->n[MT_HAND] == 0 && mt_redeals_left(P) == 3);
}

/* a row in order, the gaps: a deal laid out by hand (rows of one suit each from 2, then shifted) */
static void layout_rows(void)
{
    mt_new(&G, 1);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 13; c++) set(13 * r + c, c < 12 ? CARD(r, c + 2) : CARD_NONE);
    G.start = G.pos;
    G.n = G.moves = 0;
}

static void test_rules(void)
{
    layout_rows();
    CHECK(mt_won(P) && mt_run(P, 2) == 12 && !mt_stuck(P));
    /* row 0: 2 3 4 _ 6 ... : the 5 of spades fills the gap; a king's gap takes nothing */
    set(3, CARD_NONE);
    set(12, S(SPADES, 5));
    CHECK(!mt_won(P) && mt_run(P, 0) == 3);
    CHECK(mt_card_for(P, 3) == S(SPADES, 5) && legal(12, 3) && !legal(14, 3));
    set(13 + 12, CARD_NONE);                    /* row 1 ends ... Q K: its gap after the king */
    CHECK(mt_card_for(P, 25) == CARD_NONE || CARD_RANK(sol_top(P, 24)) != RANK_KING);
    CHECK(mt_card_for(P, 12) == CARD_NONE);     /* after the king of spades */
    CHECK(mt_fill(P, 3).kind == SM_MOVE && mt_fill(P, 3).from == 12);
    CHECK(mt_script(&G, "a13a4") && mt_run(P, 0) == 12);
    /* the first column takes any 2 from elsewhere, not one already there */
    layout_rows();
    set(26, CARD_NONE);                         /* the 2 of diamonds gone from row 2's start */
    set(25, S(DIAMONDS, 2));
    CHECK(legal(25, 26) && !legal(0, 26) && mt_card_for(P, 26) != CARD_NONE);
    CHECK(mt_rules.tap(P, 25, 0, 0).to == 26);
    CHECK(!mt_script(&G, "a1c1") && !mt_script(&G, "e1a1") && !mt_script(&G, "a14a1"));
}

/* the redeals: the cards out of order gathered, dealt again after the runs, a gap right after each run; the same
 * seed the same; three of them, then none */
static void test_redeal(void)
{
    mt_new(&G, 8);
    int run[4];
    for (int r = 0; r < 4; r++) run[r] = mt_run(P, r);
    solpos before = G.pos;
    CHECK(mt_script(&G, "s") && pc_whole(P, 1, 0) && G.moves == 1 && mt_redeals_left(P) == 2);
    for (int r = 0; r < 4; r++) {
        CHECK(mt_run(P, r) >= run[r]);
        for (int c = 0; c < run[r]; c++) CHECK(P->c[13 * r + c][0] == before.c[13 * r + c][0]);
        CHECK(!P->n[13 * r + run[r]]);          /* the gap after the run */
        for (int c = run[r] + 1; c < 13; c++) CHECK(P->n[13 * r + c] == 1);
    }
    mt_new(&H, 8);
    CHECK(mt_script(&H, "s") && !memcmp(&H.pos, &G.pos, sizeof H.pos));
    CHECK(mt_script(&G, "s s") && !mt_redeals_left(P) && !mt_script(&G, "s"));
    CHECK(sol_undo(&G) && sol_undo(&G) && sol_undo(&G) && !memcmp(&G.pos, &before, sizeof before));
}

static void test_end(void)
{
    layout_rows();                              /* every gap after a king, no redeal left: over */
    for (int r = 0; r < 4; r++)                 /* row r: the 2 of suit r, then 3..K of the next suit, the gap */
        for (int c = 1; c < 12; c++) set(13 * r + c, CARD((r + 1) % 4, c + 2));
    CHECK(pc_whole(P, 1, 0) && mt_run(P, 1) == 1);
    P->redeals = 3;
    CHECK(mt_hint(P).kind == SM_NONE && mt_stuck(P) && !mt_won(P));
    P->redeals = 2;
    CHECK(mt_hint(P).kind == SM_REDEAL && !mt_stuck(P));
}

static int sane(const solpos *p)
{
    if (!pc_whole(p, 1, 0) || p->n[MT_HAND]) return 0;
    int gaps = 0;
    for (int i = 0; i < 52; i++) gaps += !p->n[i];
    return gaps == 4 && p->redeals <= 3;
}

static void new_game(solrec *r, uint32_t seed) { mt_new(r, seed); }

int main(void)
{
    test_deal();
    test_rules();
    test_redeal();
    test_end();
    int ends, wins;
    pc_random(&mt_rules, new_game, sane, 300, 600, &ends, &wins);
    printf("ok   300 random games (%d over, %d won)\n", ends, wins);
    int won = pc_hinted(&mt_rules, new_game, 300, 1500);
    printf("ok   the hint alone wins %d deals of 300\n", won);
    printf("montana: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
