/*
 * Golf, the patience: the rules' test (the deal; one rank up or down, round the corner or not; the stock once
 * through; the end and the score; the notation; whole random games from seeds; the hint against a full search on
 * small cases; deals the hint wins played again from their scripts). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/golfpatience.h"
#include "patience_check.h"

static int go(int col) { return pg_legal(P, sol_mv(SM_MOVE, col, PG_WASTE, 1)); }

static void top(int col, int card) { P->c[col][P->n[col] - 1] = (uint8_t)card; }
static void waste(int card) { P->c[PG_WASTE][P->n[PG_WASTE] - 1] = (uint8_t)card; }

static void test_deal(void)
{
    pg_new(&G, PG_NOWRAP, 9);
    pg_new(&H, PG_NOWRAP, 9);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos) && pc_whole(P, 1, 1));
    for (int c = 0; c < 7; c++) CHECK(P->n[c] == 5 && P->down[c] == 0);
    CHECK(P->n[PG_STOCK] == 16 && P->down[PG_STOCK] == 16 && P->n[PG_WASTE] == 1);
    CHECK(pg_score(P) == 35 && !pg_won(P) && !pg_stuck(P));
}

static void test_rules(void)
{
    pg_new(&G, PG_NOWRAP, 9);
    waste(S(HEARTS, 8));
    top(0, S(CLUBS, 7));
    top(1, S(SPADES, 9));
    top(2, S(DIAMONDS, 8));
    top(3, S(HEARTS, 10));
    CHECK(go(0) && go(1) && !go(2) && !go(3));  /* one up or one down, any suit */
    CHECK(!pg_legal(P, sol_mv(SM_MOVE, PG_STOCK, PG_WASTE, 1)) && !pg_legal(P, sol_mv(SM_MOVE, 0, 1, 1)));
    waste(S(HEARTS, RANK_KING));
    top(0, S(CLUBS, RANK_ACE));
    top(1, S(SPADES, RANK_QUEEN));
    CHECK(!go(0) && go(1));                     /* the classic rule: no ace on a king; a queen yes */
    pg_new(&H, PG_WRAP, 9);
    H.pos.c[PG_WASTE][0] = S(HEARTS, RANK_KING);
    H.pos.c[0][4] = S(CLUBS, RANK_ACE);
    CHECK(pg_legal(&H.pos, sol_mv(SM_MOVE, 0, PG_WASTE, 1)));   /* round the corner */
    /* the tap and the notation */
    pg_new(&G, PG_NOWRAP, 9);
    waste(S(HEARTS, 8));
    top(4, S(CLUBS, 9));
    CHECK(pg_rules.tap(P, 4, 4, 0).kind == SM_MOVE && pg_rules.tap(P, 4, 3, 0).kind == SM_NONE);
    CHECK(pg_script(&G, "5") && P->n[4] == 4 && sol_top(P, PG_WASTE) == S(CLUBS, 9) && pg_score(P) == 34);
    CHECK(!pg_script(&G, "8") && !pg_script(&G, "x"));
}

/* the stock: once through, then the end when nothing goes; the score once won */
static void test_end(void)
{
    pg_new(&G, PG_NOWRAP, 9);
    for (int i = 0; i < 16; i++) CHECK(pg_script(&G, "s"));
    CHECK(!pg_script(&G, "s") && pg_rules.stock(P).kind == SM_NONE);
    for (int c = 0; c < 7; c++) top(c, S(CLUBS, 5));
    waste(S(HEARTS, RANK_KING));
    CHECK(pg_stuck(P) && pg_hint(P).kind == SM_NONE);
    pg_new(&G, PG_NOWRAP, 9);
    for (int c = 0; c < 7; c++) P->n[c] = 0;
    P->n[0] = 1;
    P->c[0][0] = S(SPADES, 7);
    waste(S(SPADES, 6));
    CHECK(pg_script(&G, "1") && pg_won(P) && !pg_stuck(P) && pg_score(P) == -16);
}

/* the hint's look ahead against a full search, on columns cut down to few cards */
static int full(const solpos *p, int on)
{
    int best = 0;
    for (int c = 0; c < 7; c++) {
        if (!p->n[c] || !pg_follows(p, sol_top(p, c), on)) continue;
        solpos q = *p;
        sol_apply(&q, sol_mv(SM_MOVE, c, PG_WASTE, 1));
        int v = 1 + full(&q, sol_top(&q, PG_WASTE));
        if (v > best) best = v;
    }
    return best;
}

static void test_hint(void)
{
    int ok = 1;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        pg_new(&G, seed % 2, seed);
        for (int c = 0; c < 7; c++) P->n[c] = (uint8_t)(1 + seed % 2);
        solmove h = pg_hint(P);
        int best = full(P, sol_top(P, PG_WASTE));
        if (!best) { ok &= h.kind == SM_DRAW; continue; }
        solpos q = *P;
        sol_apply(&q, h);
        ok &= h.kind == SM_MOVE && 1 + full(&q, sol_top(&q, PG_WASTE)) == best;
    }
    CHECK(ok);
}

static int sane(const solpos *p)
{
    if (!pc_whole(p, 1, 1)) return 0;
    for (int c = 0; c < 7; c++)
        if (p->down[c]) return 0;
    return p->down[PG_STOCK] == p->n[PG_STOCK] && !p->down[PG_WASTE];
}

static void new_game(solrec *r, uint32_t seed) { pg_new(r, seed % 2, seed); }

int main(void)
{
    test_deal();
    test_rules();
    test_end();
    test_hint();
    int ends, wins;
    pc_random(&pg_rules, new_game, sane, 400, 200, &ends, &wins);
    printf("ok   400 random games (%d over, %d won)\n", ends, wins);
    CHECK(ends + wins == 400);
    int won = pc_hinted(&pg_rules, new_game, 400, 200);
    printf("ok   the hint alone wins %d deals of 400\n", won);
    CHECK(won > 0);
    /* a deal the hint wins, played again from its script */
    for (uint32_t seed = 1; seed <= 400; seed++) {
        new_game(&G, seed);
        char script[600] = "";
        while (!pg_won(P)) {
            solmove h = pg_hint(P);
            if (h.kind == SM_NONE) break;
            char m[4] = {h.kind == SM_DRAW ? 's' : (char)('1' + h.from), ' ', 0};
            strcat(script, m);
            pg_play(&G, h);
        }
        if (!pg_won(P)) continue;
        new_game(&H, seed);
        CHECK(pg_script(&H, script) && pg_won(&H.pos) && !memcmp(&H.pos, &G.pos, sizeof H.pos));
        printf("ok   deal %u (%s) won: %s\n", seed, seed % 2 ? "round the corner" : "classic", script);
        break;
    }
    printf("golfpatience: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
