/*
 * Pyramid: the rules' test (the deal; free cards; kings and pairs making 13; the stock, the turned card and the
 * waste; one pass or three; undo; the notation; whole random games from seeds, the hint always legal and the end
 * found exactly; deals solved by a small search and played again from their scripts). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/pyramid.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static solrec G, H;
static solpos *P = &G.pos;

#define S(s, r) CARD(SUIT_##s, r)
#define AT(r, c) (PY_SLOT + (r) * ((r) + 1) / 2 + (c))   /* row 0..6, place 0..row */

static int whole(const solpos *p)
{
    int seen[256] = {0}, n = 0;
    for (int i = 0; i < p->npiles; i++)
        for (int k = 0; k < p->n[i]; k++) seen[p->c[i][k]]++, n++;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (seen[CARD(s, r)] != 1) return 0;
    return n == 52;
}

static void put(int pile, int card)
{
    P->c[pile][0] = (uint8_t)card;
    P->n[pile] = 1;
}

static int pair(int a, int b) { return py_legal(P, sol_mv(SM_MOVE, a, b, 1)); }

/* every legal move, found the slow way: the pairs and kings, the stock */
static int any_move(const solpos *p)
{
    for (int a = 0; a < PY_PILES; a++)
        for (int b = 0; b < PY_PILES; b++)
            if (py_legal(p, sol_mv(SM_MOVE, a, b, 1))) return 1;
    return py_legal(p, sol_mv(SM_DRAW, PY_STOCK, PY_TURN, 1)) ||
           py_legal(p, sol_mv(SM_REDEAL, PY_WASTE, PY_STOCK, p->n[PY_WASTE] + p->n[PY_TURN]));
}

static void test_deal(void)
{
    py_new(&G, PY_ONCE, 42);
    py_new(&H, PY_ONCE, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    for (int i = 0; i < PY_SLOTS; i++) CHECK(P->n[i] == 1 && P->down[i] == 0 && P->kind[i] == SP_TAB);
    CHECK(P->n[PY_STOCK] == 24 && P->down[PY_STOCK] == 24 && !P->n[PY_TURN] && !P->n[PY_WASTE] && !P->n[PY_OUT]);
    CHECK(whole(P) && G.n == 0 && G.moves == 0 && py_passes(P) == 1);
    py_new(&H, PY_THRICE, 43);
    CHECK(py_passes(&H.pos) == 3 && memcmp(H.pos.c, G.pos.c, sizeof G.pos.c) != 0);
    /* the covers: each card under the two below it; the bottom row free */
    CHECK(py_slots[AT(0, 0)].cover[0] == AT(1, 0) && py_slots[AT(0, 0)].cover[1] == AT(1, 1));
    CHECK(py_slots[AT(4, 2)].cover[0] == AT(5, 2) && py_slots[AT(4, 2)].cover[1] == AT(5, 3));
    CHECK(py_slots[AT(6, 6)].cover[0] == -1 && py_slots[AT(6, 6)].row == 6 && py_slots[AT(6, 6)].col == 6);
    for (int i = 0; i < PY_SLOTS; i++) CHECK(py_free(P, PY_SLOT + i) == (py_slots[i].row == 6));
    CHECK(!py_free(P, PY_TURN) && !py_free(P, PY_WASTE) && !py_free(P, PY_STOCK) && !py_free(P, PY_OUT));
}

static void test_pairs(void)
{
    py_new(&G, PY_ONCE, 7);
    put(AT(6, 0), S(SPADES, 12));
    put(AT(6, 1), S(HEARTS, 1));
    put(AT(6, 2), S(CLUBS, 13));
    put(AT(6, 3), S(DIAMONDS, 11));
    put(AT(6, 4), S(SPADES, 2));
    put(AT(6, 5), S(HEARTS, 7));
    put(AT(6, 6), S(HEARTS, 6));
    put(AT(5, 0), S(DIAMONDS, 1));
    CHECK(pair(AT(6, 0), AT(6, 1)) && pair(AT(6, 1), AT(6, 0)));        /* queen and ace */
    CHECK(pair(AT(6, 3), AT(6, 4)) && pair(AT(6, 5), AT(6, 6)));        /* jack and 2, 7 and 6 */
    CHECK(!pair(AT(6, 0), AT(6, 4)) && !pair(AT(6, 1), AT(6, 1)));      /* 12 + 2; the same card */
    CHECK(!pair(AT(6, 0), AT(5, 0)));                                   /* the ace under: not free */
    CHECK(!pair(AT(6, 2), AT(6, 0)) && pair(AT(6, 2), PY_OUT));         /* a king goes alone */
    CHECK(!pair(AT(6, 0), PY_OUT) && !pair(AT(5, 0), PY_OUT) && !pair(PY_OUT, AT(6, 0)));
    CHECK(py_rules.tap(P, AT(6, 2), 0, 0).kind == SM_MOVE && py_rules.tap(P, AT(6, 2), 0, 0).to == PY_OUT);
    CHECK(py_rules.tap(P, AT(6, 0), 0, 0).kind == SM_NONE);             /* a queen waits for her ace */
    CHECK(py_rules.pickable(P, AT(6, 0), 0) && !py_rules.pickable(P, AT(5, 0), 0));
    /* played: both off, as one move; the ace under now free once its other cover goes too */
    CHECK(py_play(&G, sol_mv(SM_MOVE, AT(6, 0), AT(6, 1), 1)));
    CHECK(!P->n[AT(6, 0)] && !P->n[AT(6, 1)] && P->n[PY_OUT] == 2 && G.moves == 1 && G.n == 2);
    CHECK(G.mv[1].kind == (SM_MOVE | SM_AUTO) && py_free(P, AT(5, 0)));
    CHECK(py_play(&G, sol_mv(SM_MOVE, AT(6, 2), PY_OUT, 1)) && G.moves == 2 && P->n[PY_OUT] == 3);
    CHECK(sol_top(P, PY_OUT) == S(CLUBS, 13));
    CHECK(!py_play(&G, sol_mv(SM_MOVE, AT(6, 0), PY_OUT, 1)));          /* gone already */
    CHECK(sol_undo(&G) && G.moves == 1 && P->n[AT(6, 2)] == 1 && P->n[PY_OUT] == 2);
    CHECK(sol_undo(&G) && G.moves == 0 && G.n == 0 && P->n[AT(6, 0)] == 1 && P->n[AT(6, 1)] == 1);
    CHECK(!sol_undo(&G));
}

/* the stock: a card turned; the next one puts it on the waste; both free; once through, or three times */
static void test_stock(void)
{
    py_new(&G, PY_ONCE, 5);
    uint8_t stock[24];
    memcpy(stock, P->c[PY_STOCK], 24);
    CHECK(py_script(&G, "s") && P->n[PY_TURN] == 1 && sol_top(P, PY_TURN) == stock[23] && P->n[PY_STOCK] == 23);
    CHECK(P->down[PY_STOCK] == 23 && P->down[PY_TURN] == 0 && G.moves == 1 && G.n == 1);
    CHECK(py_free(P, PY_TURN) && !py_free(P, PY_WASTE));
    CHECK(py_script(&G, "s") && P->n[PY_TURN] == 1 && P->n[PY_WASTE] == 1 && sol_top(P, PY_WASTE) == stock[23]);
    CHECK(sol_top(P, PY_TURN) == stock[22] && G.moves == 2 && G.n == 3 && (G.mv[2].kind & SM_AUTO));
    CHECK(sol_undo(&G) && P->n[PY_TURN] == 1 && !P->n[PY_WASTE] && sol_top(P, PY_TURN) == stock[23]);
    for (int i = 0; i < 23; i++) CHECK(py_script(&G, "s"));
    CHECK(!P->n[PY_STOCK] && P->n[PY_WASTE] == 23 && P->n[PY_TURN] == 1);
    CHECK(py_rules.stock(P).kind == SM_NONE && !py_script(&G, "s"));   /* once through */
    /* three times: the waste (and the turned card) back as the stock, in its order, twice */
    py_new(&G, PY_THRICE, 5);
    for (int pass = 0; pass < 3; pass++) {
        CHECK(py_passes(P) == 3 - pass);
        for (int i = 0; i < 24; i++) CHECK(py_script(&G, "s"));
        CHECK(!P->n[PY_STOCK] && P->n[PY_TURN] == 1 && P->n[PY_WASTE] == 23);
        if (pass < 2) {
            solmove m = py_rules.stock(P);
            CHECK(m.kind == SM_REDEAL && m.n == 24);
            CHECK(py_script(&G, "s") && P->n[PY_STOCK] == 24 && P->down[PY_STOCK] == 24 && !P->n[PY_TURN]);
            CHECK(!memcmp(P->c[PY_STOCK], stock, 24) && P->redeals == pass + 1);
        }
    }
    CHECK(py_rules.stock(P).kind == SM_NONE && py_passes(P) == 1 && G.moves == 3 * 24 + 2);
    /* the turned card and the waste's pair */
    py_new(&G, PY_ONCE, 5);
    P->c[PY_STOCK][23] = S(SPADES, 4);
    P->c[PY_STOCK][22] = S(HEARTS, 9);
    G.start = *P;
    CHECK(py_script(&G, "s s") && pair(PY_TURN, PY_WASTE) && py_script(&G, "t+w"));
    CHECK(!P->n[PY_TURN] && !P->n[PY_WASTE] && P->n[PY_OUT] == 2);
    CHECK(sol_undo(&G) && P->n[PY_TURN] == 1 && P->n[PY_WASTE] == 1);
}

static void test_notation(void)
{
    py_new(&G, PY_ONCE, 9);
    put(AT(6, 0), S(SPADES, 12));
    put(AT(6, 6), S(HEARTS, 1));
    put(AT(6, 3), S(HEARTS, 13));
    char out[8];
    py_write(P, sol_mv(SM_MOVE, AT(6, 0), AT(6, 6), 1), out);
    CHECK(!strcmp(out, "7a+7g"));
    py_write(P, sol_mv(SM_MOVE, AT(6, 3), PY_OUT, 1), out);
    CHECK(!strcmp(out, "7d"));
    py_write(P, sol_mv(SM_MOVE, PY_TURN, PY_WASTE, 1), out);
    CHECK(!strcmp(out, "t+w"));
    py_write(P, sol_mv(SM_DRAW, PY_STOCK, PY_TURN, 1), out);
    CHECK(!strcmp(out, "s"));
    G.start = *P;
    CHECK(py_script(&G, "7a+7g 7d") && P->n[PY_OUT] == 3 && G.moves == 2);
    CHECK(!py_script(&G, "7b") && !py_script(&G, "8a") && !py_script(&G, "7a+") && !py_script(&G, "x"));
    CHECK(py_script(&G, "#77") && G.seed == 77 && G.n == 0 && P->n[PY_STOCK] == 24);
}

/* ---- whole games: random legal moves; the hint always legal; the end exactly when no move is left ------------- */
static void test_random(void)
{
    int games = 0, won = 0, ok = 1;
    for (uint32_t seed = 1; seed <= 400; seed++) {
        py_new(&G, seed % 2 ? PY_ONCE : PY_THRICE, seed);
        solpos start = G.pos;
        uint32_t rng = seed;
        for (int step = 0; step < 400 && !py_won(P); step++) {
            solmove h = py_hint(P);
            if (h.kind == SM_NONE) {
                if (any_move(P)) ok = 0, printf("FAIL seed %u: no hint, but a move\n", seed);
                CHECK(py_stuck(P));
                break;
            }
            if (!py_legal(P, h)) ok = 0, printf("FAIL seed %u: the hint is not legal\n", seed);
            solmove list[64];
            int n = 0;
            for (int a = 0; a < PY_PILES; a++)
                for (int b = 0; b < PY_PILES; b++)
                    if (n < 63 && py_legal(P, sol_mv(SM_MOVE, a, b, 1))) list[n++] = sol_mv(SM_MOVE, a, b, 1);
            if (py_rules.stock(P).kind != SM_NONE) list[n++] = py_rules.stock(P);
            solmove m = cards_rand(&rng) % 3 ? h : list[cards_rand(&rng) % (uint32_t)n];
            if (!py_play(&G, m)) ok = 0;
            if (!whole(P)) ok = 0, printf("FAIL seed %u: cards lost\n", seed);
        }
        games++;
        won += py_won(P);
        if (py_won(P)) CHECK(!py_stuck(P) && sol_slots_left(P, 0, PY_SLOTS) == 0);
        int moves = G.moves;
        for (int k = 0; k < moves; k++)
            if (!sol_undo(&G)) ok = 0;
        if (memcmp(&G.pos, &start, sizeof start) || G.n || G.moves) ok = 0, printf("FAIL seed %u: undo\n", seed);
    }
    CHECK(ok);
    printf("ok   %d random games (%d won)\n", games, won);
}

/* ---- a deal solved: a depth-first search, then the moves played again from the script ------------------------- */
static int nodes;
static solmove path[200];

static int solve(solrec *r, int depth)
{
    if (py_won(&r->pos)) return depth;
    if (++nodes > 300000 || depth >= 200) return -1;
    const solpos *p = &r->pos;
    solmove list[80];
    int n = 0;
    for (int a = 0; a < PY_PILES; a++)
        for (int b = a; b < PY_PILES; b++)
            if (n < 79 && py_legal(p, sol_mv(SM_MOVE, a, b, 1))) list[n++] = sol_mv(SM_MOVE, a, b, 1);
    if (py_rules.stock(p).kind != SM_NONE) list[n++] = py_rules.stock(p);
    for (int i = 0; i < n; i++) {
        solpos save = r->pos;
        int sn = r->n, sm = r->moves;
        py_play(r, list[i]);
        path[depth] = list[i];
        int d = solve(r, depth + 1);
        if (d >= 0) return d;
        r->pos = save;
        r->n = sn;
        r->moves = sm;
    }
    return -1;
}

static solrec Q;

static void test_solved(void)
{
    int solved = 0;
    for (uint32_t seed = 1; seed <= 60 && solved < 3; seed++) {
        py_new(&Q, PY_THRICE, seed);
        nodes = 0;
        int n = solve(&Q, 0);
        if (n < 0) continue;
        char script[2048] = "", mv[8];
        py_new(&H, PY_THRICE, seed);
        solpos at = H.pos;
        sprintf(script, "#%u", seed);
        for (int i = 0; i < n; i++) {
            py_write(&at, path[i], mv);
            strcat(script, " ");
            strcat(script, mv);
            solrec tmp;
            tmp.pos = at;
            tmp.start = at;
            tmp.n = tmp.moves = 0;
            py_play(&tmp, path[i]);
            at = tmp.pos;
        }
        CHECK(py_script(&H, script) && py_won(&H.pos) && H.moves == n && H.seed == seed);
        if (!solved) printf("ok   seed %u solved in %d moves: %s\n", seed, n, script);
        solved++;
    }
    CHECK(solved >= 1);
}

int main(void)
{
    test_deal();
    test_pairs();
    test_stock();
    test_notation();
    test_random();
    test_solved();
    printf("pyramid: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
