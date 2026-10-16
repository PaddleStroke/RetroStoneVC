/*
 * Accordion: the rules' test (the deal; a pile onto its neighbour or the third to its left on the same suit or rank;
 * the row closing up; the end, the stars; the notation; the search and the hint; whole random games from seeds; the
 * levels, each one's win played). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <time.h>

#include "games/accordion.h"
#include "games/accordion_levels.h"
#include "levels_check.h"
#include "patience_check.h"

/* a row of these cards (the rest of the pack after them, so that the pack stays whole) */
static void row(const int *cards, int n)
{
    uint8_t deck[52];
    int k = 0, used[256] = {0};
    for (int i = 0; i < n; i++) deck[k++] = (uint8_t)cards[i], used[cards[i]] = 1;
    uint8_t pack[104];
    cards_pack(PACK_52, pack);
    for (int i = 0; i < 52; i++)
        if (!used[pack[i]]) deck[k++] = pack[i];
    acd_deal_cards(&G, 0, deck);
}

static void test_deal(void)
{
    acd_new(&G, 42);
    acd_new(&H, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && pc_whole(P, 1, 1));
    for (int i = 0; i < ACD_PILES; i++) CHECK(P->n[i] == 1 && P->down[i] == 0);
    CHECK(acd_piles(P) == 52 && !acd_won(P));
}

static void test_moves(void)
{
    /* 5S 9H 2D 5C 9D: the 5 of clubs on the 5 of spades (third left) or not on the 2 of diamonds (neighbour) */
    const int a[] = {S(SPADES, 5), S(HEARTS, 9), S(DIAMONDS, 2), S(CLUBS, 5), S(DIAMONDS, 9)};
    row(a, 5);
    CHECK(pc_whole(P, 1, 1));
    CHECK(acd_legal(P, sol_mv(SM_MOVE, 3, 0, 1)) && !acd_legal(P, sol_mv(SM_MOVE, 3, 2, 1)));
    CHECK(acd_legal(P, sol_mv(SM_MOVE, 4, 3, 1)) == 0 && acd_legal(P, sol_mv(SM_MOVE, 4, 1, 1)));   /* 9 on 9 */
    CHECK(!acd_legal(P, sol_mv(SM_MOVE, 4, 2, 1)) && !acd_legal(P, sol_mv(SM_MOVE, 4, 0, 1)));   /* 2 or 4 left */
    CHECK(!acd_legal(P, sol_mv(SM_MOVE, 2, 1, 1)) && !acd_legal(P, sol_mv(SM_MOVE, 1, 0, 1)));
    /* the tap: the third pile first */
    solmove m = acd_rules.tap(P, 3, 0, 0);
    CHECK(m.kind == SM_MOVE && m.to == 0);
    CHECK(acd_script(&G, "4c"));                /* 5C on 5S: the row closes up */
    CHECK(acd_piles(P) == 51 && P->n[0] == 2 && sol_top(P, 0) == S(CLUBS, 5) && sol_top(P, 3) == S(DIAMONDS, 9));
    CHECK(G.moves == 1 && pc_whole(P, 1, 1));
    /* now 5C 9H 2D 9D: the 9 of diamonds on the 2 of diamonds (suit), on the 5 of clubs's pile (third): no */
    CHECK(acd_legal(P, sol_mv(SM_MOVE, 3, 2, 1)) && !acd_legal(P, sol_mv(SM_MOVE, 3, 0, 1)));
    CHECK(acd_script(&G, "4a") && sol_top(P, 2) == S(DIAMONDS, 9) && P->n[2] == 2 && acd_piles(P) == 50);
    /* the whole pile moves: 9D (on 2D) onto 9H */
    CHECK(acd_script(&G, "3a") && P->n[1] == 3 && P->c[1][0] == S(HEARTS, 9) && P->c[1][1] == S(DIAMONDS, 2));
    CHECK(!acd_script(&G, "1a") && !acd_script(&G, "2c"));
    CHECK(sol_undo(&G) && sol_undo(&G) && sol_undo(&G) && acd_piles(P) == 52 && !sol_undo(&G));
}

static void test_end(void)
{
    CHECK(acd_stars(1) == 3 && acd_stars(2) == 2 && acd_stars(3) == 2 && acd_stars(4) == 1 && acd_stars(6) == 1);
    CHECK(acd_stars(7) == 0 && acd_stars(30) == 0);
    /* a row of 13 spades then the rest: the spades all go onto each other */
    int a[13];
    for (int r = 0; r < 13; r++) a[r] = S(SPADES, r + 1);
    row(a, 13);
    for (int i = 0; i < 12; i++) CHECK(acd_script(&G, "2a"));
    CHECK(P->n[0] == 13 && acd_piles(P) == 40 && pc_whole(P, 1, 1));
    /* stuck: a row in which no card matches its neighbour or the third to its left */
    const int b[] = {S(SPADES, 2), S(HEARTS, 3), S(DIAMONDS, 4), S(CLUBS, 5)};
    acd_new(&G, 1);
    for (int i = 0; i < ACD_PILES; i++) P->n[i] = 0;
    for (int i = 0; i < 4; i++) P->c[i][0] = (uint8_t)b[i], P->n[i] = 1;
    CHECK(acd_stuck(P) && acd_piles(P) == 4 && acd_hint(P).kind == SM_NONE);
    P->c[3][0] = (uint8_t)S(CLUBS, 2);          /* 2C on 2S (third left) */
    CHECK(!acd_stuck(P) && acd_hint(P).kind == SM_MOVE);
}

/* the search: a win it finds plays out to one pile; the hint is always legal */
static void test_search(void)
{
    static solrec r;
    solmove path[ACD_PILES];
    int wins = 0;
    clock_t t0 = clock();
    for (int seed = 1; seed <= 40; seed++) {
        acd_new(&r, (uint32_t)seed);
        int len;
        int left = acd_solve(&r.pos, 50000, path, &len, NULL);
        for (int i = 0; i < len; i++) CHECK(acd_play(&r, path[i]));
        CHECK(acd_piles(&r.pos) == left);
        wins += left == 1;
    }
    printf("ok   the search wins %d deals of 40 (%.1fs)\n", wins, (double)(clock() - t0) / CLOCKS_PER_SEC);
    t0 = clock();
    for (int i = 0; i < 100; i++) {             /* the hint's search: quick */
        acd_new(&r, (uint32_t)(1000 + i));
        solmove h = acd_hint(&r.pos);
        CHECK(h.kind == SM_MOVE && acd_legal(&r.pos, h));
    }
    double ms = (double)(clock() - t0) * 1000 / CLOCKS_PER_SEC / 100;
    printf("ok   a hint takes %.2f ms\n", ms);
    CHECK(ms < 50);
}

static int sane(const solpos *p)
{
    if (!pc_whole(p, 1, 1)) return 0;
    int n = acd_piles(p);
    for (int i = n; i < ACD_PILES; i++)
        if (p->n[i]) return 0;                  /* the row closed up */
    return 1;
}

static void new_game(solrec *r, uint32_t seed) { acd_new(r, seed); }

static void test_levels(void)
{
    static solrec A, B;
    uint8_t deck[104];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, 77);
    acd_new(&A, 77);
    acd_deal_cards(&B, A.variant, deck);
    CHECK(!memcmp(&A.pos, &B.pos, sizeof A.pos));
    CHECK(lc_levels("accordion", acd_levels, ACD_LEVELS, 52, acd_deal_cards, &acd_search, 400000) == ACD_LEVELS);
}

int main(void)
{
    test_deal();
    test_moves();
    test_end();
    test_search();
    int ends, wins;
    pc_random(&acd_rules, new_game, sane, 200, 100, &ends, &wins);
    printf("ok   200 random games (%d with no move left, %d won)\n", ends, wins);
    int won = pc_hinted(&acd_rules, new_game, 100, 60);
    printf("ok   the hint alone wins %d deals of 100\n", won);
    test_levels();
    printf("accordion: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
