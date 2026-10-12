/*
 * Yukon: the rules' test (the deal; any face-up card moving with what lies on it; kings to empty columns; the
 * foundations; the taps; the end going up by itself; whole random games from seeds; the hint). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/yukon.h"
#include "patience_check.h"

static void clear(void)
{
    yk_new(&G, 1);
    for (int i = 0; i < YK_PILES; i++) P->n[i] = P->down[i] = 0;
}

static void put(int pile, int down, const int *cards, int n)
{
    for (int k = 0; k < n; k++) P->c[pile][k] = (uint8_t)cards[k];
    P->n[pile] = (uint8_t)n;
    P->down[pile] = (uint8_t)down;
}

static int legal(int a, int b, int n) { return yk_legal(P, sol_mv(SM_MOVE, a, b, n)); }

static void test_deal(void)
{
    yk_new(&G, 42);
    yk_new(&H, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && pc_whole(P, 1, 1));
    CHECK(P->n[0] == 1 && P->down[0] == 0);
    for (int c = 1; c < 7; c++) CHECK(P->n[c] == c + 5 && P->down[c] == c);
    for (int f = YK_FOUND; f < YK_PILES; f++) CHECK(P->n[f] == 0);
}

static void test_moves(void)
{
    clear();
    /* column 0: a back, then the 9 of spades, the 2 of hearts, the jack of clubs (not in order) */
    const int a[] = {S(HEARTS, 5), S(SPADES, 9), S(HEARTS, 2), S(CLUBS, 11)}, b[] = {S(DIAMONDS, 10)},
              c[] = {S(HEARTS, 10)}, k[] = {S(CLUBS, 13), S(HEARTS, 4)};
    put(0, 1, a, 4);
    put(1, 0, b, 1);
    put(2, 0, c, 1);
    put(3, 0, k, 2);
    CHECK(legal(0, 1, 3) && legal(0, 2, 3));    /* the 9 of spades with all that lies on it, out of order */
    CHECK(!legal(0, 1, 4));                     /* not the face-down card */
    CHECK(!legal(0, 1, 2) && !legal(0, 1, 1));  /* the 2 and the jack do not go on a red 10 */
    CHECK(legal(3, 4, 2) && !legal(0, 4, 3));   /* only a king to an empty column */
    CHECK(yk_play(&G, sol_mv(SM_MOVE, 0, 1, 3)));
    CHECK(P->n[0] == 1 && P->down[0] == 0 && P->n[1] == 4 && sol_top(P, 1) == S(CLUBS, 11));   /* turned up */
    /* the tap: a lone card to its foundation, else a column (from the next one round) */
    const int ace[] = {S(SPADES, RANK_ACE)};
    put(5, 0, ace, 1);
    solmove m = yk_rules.tap(P, 5, 0, 0);
    CHECK(m.kind == SM_MOVE && P->kind[m.to] == SP_FOUND);
    CHECK(yk_play(&G, m) && yk_rules.tap(P, 0, 0, 1).kind == SM_NONE);
    m = yk_rules.tap(P, 1, 1, 0);               /* the 9 of spades, three cards: onto the red 10 of column 2 */
    CHECK(m.kind == SM_MOVE && m.to == 2 && m.n == 3);
    /* a card back down from a foundation, never to another foundation */
    const int two[] = {S(HEARTS, 3)};
    put(6, 0, two, 1);
    CHECK(!legal(YK_FOUND, 6, 1));              /* an ace does not go on a 3 */
    CHECK(!legal(YK_FOUND, YK_FOUND + 1, 1));
}

/* every card face up: the rest goes up by itself, if it all can; undo takes it back with the move */
static void test_finish(void)
{
    clear();
    for (int s = 0; s < 4; s++)
        for (int r = 13; r >= 1; r--) P->c[s][P->n[s]++] = (uint8_t)CARD(s, r);   /* in suit: they all go up */
    P->c[4][P->n[4]++] = P->c[0][--P->n[0]];    /* the ace of spades on its own column, face down under nothing */
    P->down[3] = 1;
    G.start = G.pos;
    G.n = G.moves = 0;
    CHECK(pc_whole(P, 1, 1));
    CHECK(yk_script(&G, "5h") && !yk_won(P) && G.n == 1);   /* a face-down card left: nothing by itself */
    P->down[3] = 0;
    G.start = G.pos;
    G.n = G.moves = 0;
    CHECK(yk_script(&G, "1h") && yk_won(P) && G.moves == 1 && G.n == 51);
    CHECK(sol_undo(&G) && !memcmp(&G.pos, &G.start, sizeof G.pos));
}

static int sane(const solpos *p)
{
    if (!pc_whole(p, 1, 1)) return 0;
    for (int c = 0; c < 7; c++)
        if (p->down[c] > p->n[c] || (p->n[c] && p->down[c] == p->n[c])) return 0;
    for (int f = YK_FOUND; f < YK_PILES; f++)
        for (int k = 0; k < p->n[f]; k++)
            if (CARD_RANK(p->c[f][k]) != k + 1 || CARD_SUIT(p->c[f][k]) != CARD_SUIT(p->c[f][0])) return 0;
    return 1;
}

static void new_game(solrec *r, uint32_t seed) { yk_new(r, seed); }

int main(void)
{
    test_deal();
    test_moves();
    test_finish();
    int ends, wins;
    pc_random(&yk_rules, new_game, sane, 300, 500, &ends, &wins);
    printf("ok   300 random games (%d with no move left, %d won)\n", ends, wins);
    int won = pc_hinted(&yk_rules, new_game, 200, 1500);
    printf("ok   the hint alone wins %d deals of 200\n", won);
    printf("yukon: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
