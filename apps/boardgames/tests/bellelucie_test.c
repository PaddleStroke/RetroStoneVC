/*
 * La Belle Lucie: the rules' test (the deal in fans of three; only the top card, down in suit; empty fans stay empty;
 * the two redeals, shuffled, in threes, the same again from the same seed, undone at once; the merci on the last deal,
 * once; the end; the notation; whole random games from seeds; the hint). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/bellelucie.h"
#include "patience_check.h"

static void clear(void)
{
    lb_new(&G, 1);
    for (int i = 0; i < LB_PILES; i++) P->n[i] = P->down[i] = 0;
}

static void put(int pile, const int *cards, int n)
{
    for (int k = 0; k < n; k++) P->c[pile][k] = (uint8_t)cards[k];
    P->n[pile] = (uint8_t)n;
}

static int legal(int a, int b, int n) { return lb_legal(P, sol_mv(SM_MOVE, a, b, n)); }

static void test_deal(void)
{
    lb_new(&G, 3);
    lb_new(&H, 3);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && pc_whole(P, 1, 1));
    for (int f = 0; f < 17; f++) CHECK(P->n[f] == 3 && P->down[f] == 0);
    CHECK(P->n[17] == 1 && lb_redeals_left(P) == 2 && !lb_merci_open(P));
}

static void test_moves(void)
{
    clear();
    const int a[] = {S(SPADES, 4), S(HEARTS, 9)}, b[] = {S(HEARTS, 10)}, c[] = {S(CLUBS, 10)}, d[] = {S(HEARTS, 1)},
              e[] = {S(HEARTS, 2), S(CLUBS, 5)};
    put(0, a, 2);
    put(1, b, 1);
    put(2, c, 1);
    put(3, d, 1);
    put(4, e, 2);
    CHECK(legal(0, 1, 1) && !legal(0, 2, 1));   /* down in suit only */
    CHECK(!legal(1, 0, 1) && !legal(0, 1, 2));  /* not up; only the top card */
    CHECK(!legal(0, 5, 1));                     /* an empty fan stays empty */
    CHECK(legal(3, LB_FOUND, 1) && !legal(0, LB_FOUND, 1));
    CHECK(!legal(4, LB_MERCI, 2));              /* no merci before the last deal */
    solmove m = lb_rules.tap(P, 0, 1, 0);
    CHECK(m.kind == SM_MOVE && m.to == 1);
    CHECK(lb_rules.tap(P, 0, 0, 0).kind == SM_NONE);   /* a buried card: nothing yet */
    CHECK(lb_script(&G, "dh") && P->n[3] == 0 && P->n[LB_FOUND] == 1);
    CHECK(!lb_script(&G, "fh") && !lb_script(&G, "ac"));
    CHECK(lb_script(&G, "ab") && sol_top(P, 1) == S(HEARTS, 9) && sol_top(P, 0) == S(SPADES, 4));
}

/* the redeals: twice, every card of the fans gathered and dealt again in threes, shuffled; the same seed, the same
 * shuffle; the merci after them, once */
static void test_redeal(void)
{
    lb_new(&G, 5);
    CHECK(lb_script(&G, "?"));                  /* whatever the hint does first */
    lb_new(&G, 5);
    solpos before = G.pos;
    CHECK(lb_script(&G, "s") && pc_whole(P, 1, 1) && G.moves == 1 && lb_redeals_left(P) == 1);
    CHECK(memcmp(P->c, before.c, sizeof before.c) != 0 && P->n[LB_HAND] == 0);
    for (int f = 0; f < 17; f++) CHECK(P->n[f] == 3);
    CHECK(P->n[17] == 1);
    lb_new(&H, 5);
    CHECK(lb_script(&H, "s") && !memcmp(&H.pos, &G.pos, sizeof H.pos));
    CHECK(sol_undo(&G) && !memcmp(&G.pos, &before, sizeof before));
    /* cards up first: fewer fans dealt */
    clear();
    const int f0[] = {S(HEARTS, 1)}, f1[] = {S(SPADES, 9), S(SPADES, 2), S(CLUBS, 4), S(CLUBS, 7)},
              f2[] = {S(DIAMONDS, 8), S(HEARTS, 3)};
    put(0, f0, 1);
    put(5, f1, 4);
    put(9, f2, 2);
    G.start = G.pos;
    G.n = G.moves = 0;
    CHECK(lb_script(&G, "ah s") && P->n[0] == 3 && P->n[1] == 3 && P->n[2] == 0 && P->n[5] == 0);
    CHECK(lb_script(&G, "s") && P->redeals == 2 && lb_merci_open(P) && !lb_script(&G, "s"));
    /* the merci: a buried card drawn out, the cards over it closing up; then it plays from its place; only once */
    int fan = P->n[0] == 3 ? 0 : 1, buried = P->c[fan][0], o1 = P->c[fan][1], o2 = P->c[fan][2];
    char s[8] = {'!', (char)('a' + fan), '0', 0};
    CHECK(lb_script(&G, s) && sol_top(P, LB_MERCI) == buried && P->n[fan] == 2);
    CHECK(P->c[fan][0] == o1 && P->c[fan][1] == o2 && !lb_merci_open(P) && G.moves == 4);
    CHECK(!lb_legal(P, sol_mv(SM_MOVE, 1 - fan, LB_MERCI, 2)));
    CHECK(sol_undo(&G) && lb_merci_open(P) && P->n[fan] == 3 && P->c[fan][0] == buried);
}

/* the end: no move, no redeal, no merci; or won */
static void test_end(void)
{
    clear();
    const int a[] = {S(SPADES, 5), S(SPADES, 4)};
    put(0, a, 2);
    P->redeals = 3;                             /* (both redeals and the merci used) */
    CHECK(lb_stuck(P) && lb_hint(P).kind == SM_NONE);
    P->redeals = 2;
    CHECK(!lb_stuck(P) && lb_hint(P).kind == SM_MOVE && lb_hint(P).to == LB_MERCI);
    clear();
    for (int s = 0; s < 4; s++)
        for (int r = 13; r >= 1; r--) P->c[s][P->n[s]++] = (uint8_t)CARD(s, r);   /* in suit: they all go up */
    G.start = G.pos;
    G.n = G.moves = 0;
    CHECK(lb_script(&G, "ah") && lb_won(P) && G.moves == 1 && G.n == 52);
}

static int sane(const solpos *p)
{
    if (!pc_whole(p, 1, 1) || p->n[LB_HAND] || p->n[LB_MERCI] > 1) return 0;
    for (int f = LB_FOUND; f < LB_MERCI; f++)
        for (int k = 0; k < p->n[f]; k++)
            if (CARD_RANK(p->c[f][k]) != k + 1 || CARD_SUIT(p->c[f][k]) != CARD_SUIT(p->c[f][0])) return 0;
    return p->redeals <= 3;
}

static void new_game(solrec *r, uint32_t seed) { lb_new(r, seed); }

int main(void)
{
    test_deal();
    test_moves();
    test_redeal();
    test_end();
    int ends, wins;
    pc_random(&lb_rules, new_game, sane, 400, 400, &ends, &wins);
    printf("ok   400 random games (%d over, %d won)\n", ends, wins);
    int won = pc_hinted(&lb_rules, new_game, 400, 400);
    printf("ok   the hint alone wins %d deals of 400\n", won);
    printf("bellelucie: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
