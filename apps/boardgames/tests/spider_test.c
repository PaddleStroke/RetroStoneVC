/*
 * Spider: the rules' test (the deal in one, two and four suits; the moves' legality: any suit on a rank higher, only
 * runs of one suit together; dealing a row, never onto an empty column; complete runs going to the foundations by
 * themselves; undo; whole random games from seeds, and games played by the hint alone). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/spider.h"
#include "games/spider_levels.h"
#include "levels_check.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static solrec G, H;
static solpos *P = &G.pos;

#define S(s, r) CARD(SUIT_##s, r)

/* how many of each card there are: 104 in all, the variant's suits */
static int counts(const solpos *p, int count[4][14])
{
    int n = 0;
    memset(count, 0, sizeof(int) * 4 * 14);
    for (int i = 0; i < p->npiles; i++)
        for (int k = 0; k < p->n[i]; k++) count[CARD_SUIT(p->c[i][k])][CARD_RANK(p->c[i][k])]++, n++;
    return n;
}

static int whole(const solpos *p)
{
    int c[4][14], want[SP_VARIANTS][4] = {{8, 0, 0, 0}, {4, 4, 0, 0}, {2, 2, 2, 2}};
    if (counts(p, c) != 104) return 0;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (c[s][r] != want[p->variant][s]) return 0;
    return 1;
}

static void test_deal(void)
{
    for (int v = 0; v < SP_VARIANTS; v++) {
        sp_new(&G, v, 42 + v);
        CHECK(whole(P));
        for (int c = 0; c < 10; c++) CHECK(P->n[c] == (c < 4 ? 6 : 5) && P->down[c] == P->n[c] - 1);
        CHECK(P->n[SPI_STOCK] == 50 && P->down[SPI_STOCK] == 50);
    }
    sp_new(&H, SP_2SUITS, 44);
    CHECK(memcmp(&G.pos, &H.pos, sizeof G.pos) != 0);
    sp_new(&H, SP_4SUITS, 44);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
}

static void clear(int v)
{
    sp_new(&G, v, 1);
    for (int i = 0; i < SPI_PILES; i++) P->n[i] = P->down[i] = 0;
}

static void put(int pile, int down, const int *cards, int n)
{
    for (int k = 0; k < n; k++) P->c[pile][k] = (uint8_t)cards[k];
    P->n[pile] = (uint8_t)n;
    P->down[pile] = (uint8_t)down;
}

static int legal(int a, int b, int n) { return sp_legal(P, sol_mv(SM_MOVE, a, b, n)); }

static void test_moves(void)
{
    clear(SP_4SUITS);
    const int a[] = {S(CLUBS, 3), S(HEARTS, 9), S(HEARTS, 8), S(HEARTS, 7)},
              b[] = {S(SPADES, 10)}, c[] = {S(DIAMONDS, 8)}, d[] = {S(HEARTS, 9), S(SPADES, 8)}, e[] = {S(CLUBS, 9)};
    put(0, 1, a, 4);
    put(1, 0, b, 1);
    put(2, 0, c, 1);
    put(3, 0, d, 2);
    put(4, 0, e, 1);
    CHECK(legal(0, 1, 3));                      /* a run of one suit, on any suit a rank higher */
    CHECK(legal(0, 2, 1) && !legal(0, 2, 2));
    CHECK(!legal(3, 1, 2) && legal(3, 2, 0) == 0);   /* hearts and spades mixed: not together */
    CHECK(legal(3, 5, 1) && legal(0, 5, 3) && !legal(0, 5, 4));   /* an empty column: any run; no face-down card */
    CHECK(legal(0, 4, 2) && !legal(0, 4, 1) && !legal(4, 0, 1));   /* hearts on a club: fine */
    CHECK(!legal(0, SPI_FOUND, 3) && !legal(SPI_STOCK, 0, 1));
    CHECK(sp_play(&G, sol_mv(SM_MOVE, 0, 1, 3)) && P->n[0] == 1 && P->down[0] == 0 && P->n[1] == 4);
    /* the tap: onto its own suit first */
    const int f[] = {S(SPADES, 9)}, g[] = {S(HEARTS, 9)};
    put(6, 0, f, 1);
    put(7, 0, g, 1);
    solmove m = sp_rules.tap(P, 3, 1, 0);       /* the 8 of spades: the 9 of hearts (col 3's own), the 9 of spades */
    CHECK(m.kind == SM_MOVE && m.to == 6);
    CHECK(sp_rules.tap(P, 3, 1, 1).kind == SM_NONE);
}

/* the stock: a row of ten, face up, onto every column; never while a column is empty */
static void test_deals(void)
{
    sp_new(&G, SP_2SUITS, 7);
    uint8_t stock[50];
    memcpy(stock, P->c[SPI_STOCK], 50);
    for (int row = 0; row < 5; row++) {
        int before[10];
        for (int c = 0; c < 10; c++) before[c] = P->n[c];
        CHECK(sp_script(&G, "s"));
        for (int c = 0; c < 10; c++)
            CHECK(P->n[c] == before[c] + 1 && sol_top(P, c) == stock[49 - 10 * row - c] && P->down[c] < P->n[c]);
        CHECK(P->n[SPI_STOCK] == 40 - 10 * row);
    }
    CHECK(!sp_script(&G, "s") && sp_rules.stock(P).kind == SM_NONE);
    sp_new(&G, SP_2SUITS, 7);
    P->n[3] = P->down[3] = 0;                   /* an empty column: no deal */
    CHECK(!sp_legal(P, sol_mv(SM_DEAL, SPI_STOCK, 0, 10)) && sp_rules.stock(P).kind == SM_NONE);
    P->c[3][0] = S(SPADES, 1), P->n[3] = 1;
    CHECK(sp_legal(P, sol_mv(SM_DEAL, SPI_STOCK, 0, 10)));
}

/* a run of one suit from king to ace goes up by itself; undo brings it back with the move */
static void test_runs(void)
{
    clear(SP_1SUIT);
    int run[13];
    for (int k = 0; k < 13; k++) run[k] = S(SPADES, 13 - k);
    const int below[] = {S(SPADES, 5), S(SPADES, 13), S(SPADES, 12), S(SPADES, 11), S(SPADES, 10), S(SPADES, 9),
                         S(SPADES, 8), S(SPADES, 7)};
    put(0, 1, below, 8);                        /* a face-down 5, then K..7 */
    put(1, 0, run + 7, 6);                      /* 6..A */
    G.start = G.pos;
    G.n = G.moves = 0;
    CHECK(sp_script(&G, "21"));
    CHECK(P->n[0] == 1 && P->down[0] == 0 && P->n[1] == 0 && P->n[SPI_FOUND] == 13);
    CHECK(G.moves == 1 && G.n == 2 && (G.mv[1].kind & SM_AUTO));
    CHECK(sol_undo(&G) && G.n == 0 && P->n[0] == 8 && P->down[0] == 1 && P->n[SPI_FOUND] == 0);
    /* the two suits: a run of mixed suits stays */
    clear(SP_2SUITS);
    const int mixed[] = {S(SPADES, 13), S(SPADES, 12), S(HEARTS, 11), S(SPADES, 10), S(SPADES, 9), S(SPADES, 8),
                         S(SPADES, 7), S(SPADES, 6), S(SPADES, 5), S(SPADES, 4), S(SPADES, 3), S(SPADES, 2)},
              ace[] = {S(SPADES, 1)};
    put(0, 0, mixed, 12);
    put(1, 0, ace, 1);
    CHECK(sp_script(&G, "21") && P->n[0] == 13 && P->n[SPI_FOUND] == 0 && !sp_won(P));
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
    solmove s = sp_rules.stock(p);
    if (s.kind != SM_NONE) out[k++] = s;
    for (int a = 0; a < 10; a++)
        for (int b = 0; b < SPI_STOCK; b++)
            for (int n = 1; n <= sol_up(p, a); n++)
                if (sp_legal(p, sol_mv(SM_MOVE, a, b, n))) out[k++] = sol_mv(SM_MOVE, a, b, n);
    return k;
}

static int sane(const solpos *p)
{
    if (!whole(p)) return 0;
    for (int c = 0; c < 10; c++)
        if (p->down[c] > p->n[c] || (p->n[c] && p->down[c] == p->n[c])) return 0;
    for (int f = SPI_FOUND; f < SPI_STOCK; f++)
        if (p->n[f] && (p->n[f] != 13 || !sol_run(p, f, 0, 1) || CARD_RANK(p->c[f][0]) != RANK_KING)) return 0;
    return 1;
}

static void test_random(void)
{
    solmove list[1024];
    int ok = 1, runs = 0, won = 0, first_won = 0;
    for (uint32_t seed = 1; seed <= 240; seed++) {
        rng = seed * 31u;
        sp_new(&G, (int)(seed % 3), seed);
        solpos start = G.pos;
        int by_hint = seed % 2 == 0;
        for (int step = 0; step < 600 && !sp_won(P); step++) {
            solmove h = sp_hint(P);
            if (h.kind != SM_NONE && !sp_legal(P, h)) ok = 0;
            int n = all_moves(P, list);
            if (!n || (by_hint && h.kind == SM_NONE)) break;
            if (!sp_play(&G, by_hint ? h : list[rnd(n)])) ok = 0;
            if (!sane(P)) { ok = 0; printf("seed %u step %d: not sane\n", seed, step); break; }
        }
        runs += sol_on_found(P) / 13;
        if (sp_won(P) && by_hint && !first_won) {
            first_won = (int)seed;
            printf("ok   seed %u (variant %u) won by the hint alone in %d moves\n", seed, seed % 3, G.moves);
        }
        won += sp_won(P);
        while (sol_undo(&G)) {}
        if (memcmp(&G.pos, &start, sizeof start) || G.n || G.moves) ok = 0;
    }
    CHECK(ok);
    printf("ok   240 random games (%d runs completed, %d games won)\n", runs, won);
}

static void test_won(void)
{
    clear(SP_2SUITS);
    for (int f = 0; f < 7; f++) {
        for (int k = 0; k < 13; k++) P->c[SPI_FOUND + f][k] = (uint8_t)CARD(f % 2 ? SUIT_HEARTS : SUIT_SPADES, 13 - k);
        P->n[SPI_FOUND + f] = 13;
    }
    int run[13];
    for (int k = 0; k < 13; k++) run[k] = S(HEARTS, 13 - k);
    put(0, 0, run, 12);
    put(1, 0, run + 12, 1);
    CHECK(whole(P) && !sp_won(P));
    CHECK(sp_script(&G, "21") && sp_won(P) && P->n[0] == 0);
}

/* ---- the levels: a deal from its cards is the shuffled deal; every level won by the search ---------------------- */
static void test_levels(void)
{
    static solrec A, B;
    uint8_t deck[104];
    sp_pack(SP_2SUITS, deck);
    cards_shuffle(deck, 104, 77);
    sp_new(&A, SP_2SUITS, 77);
    sp_deal_cards(&B, A.variant, deck);
    CHECK(!memcmp(&A.pos, &B.pos, sizeof A.pos));
    CHECK(lc_levels("spider", sp_levels, SPI_LEVELS, 104, sp_deal_cards, &sp_search, 1000000) == SPI_LEVELS);
}

int main(void)
{
    test_deal();
    test_moves();
    test_deals();
    test_runs();
    test_random();
    test_won();
    test_levels();
    printf("spider: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
