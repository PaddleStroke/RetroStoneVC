/*
 * TriPeaks: the rules' test (the deal; the covers; one rank up or down, round the corner or not; cards turning up
 * when uncovered; the stock; the streak; undo; the notation; the hint not peeking under the backs; whole random
 * games from seeds, the hint always legal and the end found exactly; deals solved by a small search and played again
 * from their scripts). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/tripeaks.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static solrec G, H;
static solpos *P = &G.pos;

#define S(s, r) CARD(SUIT_##s, r)
enum { R1 = 0, R2 = 3, R3 = 9, R4 = 18 };       /* each row's first place */

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

static void put(int pile, int card, int down)
{
    P->c[pile][0] = (uint8_t)card;
    P->n[pile] = 1;
    P->down[pile] = (uint8_t)down;
}

static void waste(int card) { P->c[TP_WASTE][P->n[TP_WASTE] - 1] = (uint8_t)card; }
static int go(int pile) { return tp_legal(P, sol_mv(SM_MOVE, pile, TP_WASTE, 1)); }

static int any_move(const solpos *p)
{
    for (int a = 0; a < TP_PILES; a++)
        if (tp_legal(p, sol_mv(SM_MOVE, a, TP_WASTE, 1))) return 1;
    return tp_legal(p, sol_mv(SM_DRAW, TP_STOCK, TP_WASTE, 1));
}

static void test_deal(void)
{
    tp_new(&G, TP_WRAP, 42);
    tp_new(&H, TP_WRAP, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    for (int i = 0; i < TP_SLOTS; i++) CHECK(P->n[i] == 1 && P->down[i] == (i < R4));
    CHECK(P->n[TP_STOCK] == 23 && P->down[TP_STOCK] == 23 && P->n[TP_WASTE] == 1 && P->down[TP_WASTE] == 0);
    CHECK(whole(P) && G.n == 0 && G.moves == 0 && tp_streak(&G) == 0);
    tp_new(&H, TP_NOWRAP, 43);
    CHECK(H.pos.variant == TP_NOWRAP && memcmp(H.pos.c, G.pos.c, sizeof G.pos.c) != 0);
    /* the covers: the peaks' tops under two cards each; the middle row's cards under two of the third row */
    CHECK(tp_slots[R1 + 1].cover[0] == R2 + 2 && tp_slots[R1 + 1].cover[1] == R2 + 3);
    CHECK(tp_slots[R2 + 3].cover[0] == R3 + 4 && tp_slots[R2 + 3].cover[1] == R3 + 5);
    CHECK(tp_slots[R3 + 8].cover[0] == R4 + 8 && tp_slots[R3 + 8].cover[1] == R4 + 9);
    CHECK(tp_slots[R4 + 9].cover[0] == -1 && tp_slots[R4 + 9].row == 3 && tp_slots[R4 + 9].col == 9);
    for (int i = 0; i < TP_SLOTS; i++) {        /* each card lies halfway between the two over it */
        if (tp_slots[i].cover[0] < 0) continue;
        CHECK(tp_slot_x[i] * 2 == tp_slot_x[tp_slots[i].cover[0]] + tp_slot_x[tp_slots[i].cover[1]]);
        CHECK(tp_slots[tp_slots[i].cover[0]].row == tp_slots[i].row + 1);
    }
}

static void test_follow(void)
{
    tp_new(&G, TP_WRAP, 3);
    put(R4 + 0, S(SPADES, 5), 0);
    put(R4 + 1, S(HEARTS, 7), 0);
    put(R4 + 2, S(CLUBS, 6), 0);
    put(R4 + 3, S(DIAMONDS, 1), 0);
    put(R4 + 4, S(SPADES, 12), 0);
    put(R4 + 5, S(HEARTS, 13), 0);
    waste(S(DIAMONDS, 6));
    CHECK(go(R4 + 0) && go(R4 + 1) && !go(R4 + 2) && !go(R4 + 3));     /* 5 and 7 on a 6, whatever the suit */
    CHECK(!go(R3 + 0) && !go(TP_STOCK) && !go(TP_WASTE));               /* face down, covered; not piles of the peaks */
    waste(S(CLUBS, 13));
    CHECK(go(R4 + 3) && go(R4 + 4) && !go(R4 + 5));                     /* wrap: an ace on a king */
    waste(S(CLUBS, 1));
    CHECK(go(R4 + 5) && !go(R4 + 4));
    P->variant = TP_NOWRAP;
    CHECK(!go(R4 + 5));
    waste(S(CLUBS, 13));
    CHECK(!go(R4 + 3) && go(R4 + 4));
    CHECK(tp_follows(P, S(SPADES, 2), S(SPADES, 1)) && !tp_follows(P, S(SPADES, 2), S(SPADES, 2)));
}

/* played: the card on the waste; a card under it turns up once both cards over it have gone */
static void test_play(void)
{
    tp_new(&G, TP_WRAP, 3);
    put(R4 + 0, S(SPADES, 5), 0);
    put(R4 + 1, S(HEARTS, 4), 0);
    put(R3 + 0, S(CLUBS, 9), 1);
    waste(S(DIAMONDS, 6));
    G.start = *P;
    CHECK(tp_script(&G, "4a") && !P->n[R4] && sol_top(P, TP_WASTE) == S(SPADES, 5) && P->n[TP_WASTE] == 2);
    CHECK(P->down[R3] == 1 && G.n == 1 && G.moves == 1 && tp_streak(&G) == 1);
    CHECK(tp_script(&G, "4b") && P->down[R3] == 0 && G.n == 3 && G.moves == 2);
    CHECK(G.mv[2].kind == (SM_TURN | SM_AUTO) && G.mv[2].from == R3 && tp_streak(&G) == 2);
    CHECK(!tp_script(&G, "3a"));                /* a 9 on a 4 */
    CHECK(sol_undo(&G) && P->down[R3] == 1 && P->n[R4 + 1] == 1 && G.n == 1 && tp_streak(&G) == 1);
    CHECK(tp_script(&G, "4b") && tp_script(&G, "s") && tp_streak(&G) == 0 && tp_best_streak(&G) == 2);
    CHECK(P->n[TP_STOCK] == 22 && P->n[TP_WASTE] == 4);
    /* the stock: once through */
    tp_new(&G, TP_WRAP, 8);
    uint8_t stock[23];
    memcpy(stock, P->c[TP_STOCK], 23);
    for (int i = 0; i < 23; i++) CHECK(tp_script(&G, "s") && sol_top(P, TP_WASTE) == stock[22 - i]);
    CHECK(!P->n[TP_STOCK] && tp_rules.stock(P).kind == SM_NONE && !tp_script(&G, "s"));
    CHECK(G.moves == 23 && tp_streak(&G) == 0 && whole(P));
    /* the taps: a card that goes, or nothing */
    tp_new(&G, TP_WRAP, 3);
    put(R4 + 0, S(SPADES, 5), 0);
    put(R4 + 1, S(SPADES, 9), 0);
    waste(S(DIAMONDS, 6));
    CHECK(tp_rules.tap(P, R4 + 0, 0, 0).kind == SM_MOVE && tp_rules.tap(P, R4 + 1, 0, 0).kind == SM_NONE);
    CHECK(tp_rules.tap(P, R3 + 0, 0, 0).kind == SM_NONE && !tp_rules.pickable(P, R3 + 0, 0));
}

/* the hint: the longest run it can see; it does not count on the cards under the backs */
static void test_hint(void)
{
    tp_new(&G, TP_NOWRAP, 3);
    for (int i = 0; i < TP_SLOTS; i++) P->n[i] = 0;
    put(R4 + 0, S(SPADES, 5), 0);               /* 5: the end */
    put(R4 + 4, S(HEARTS, 7), 0);               /* 7, then 8 and 9 */
    put(R4 + 5, S(CLUBS, 8), 0);
    put(R4 + 6, S(CLUBS, 9), 0);
    put(R4 + 9, S(CLUBS, 4), 0);
    put(R3 + 8, S(DIAMONDS, 3), 1);              /* under the 4: a 3, but face down */
    waste(S(DIAMONDS, 6));
    solmove h = tp_hint(P);
    CHECK(h.kind == SM_MOVE && h.from == R4 + 4 && tp_legal(P, h));
    tp_new(&G, TP_NOWRAP, 3);
    for (int i = 0; i < TP_SLOTS; i++) P->n[i] = 0;
    put(R4 + 0, S(SPADES, 5), 0);
    put(R4 + 2, S(SPADES, 7), 0);
    put(R4 + 3, S(SPADES, 1), 0);
    put(R3 + 1, S(DIAMONDS, 8), 1);              /* the 7 uncovers a card, the 5 nothing: the 7 */
    waste(S(DIAMONDS, 6));
    h = tp_hint(P);
    CHECK(h.kind == SM_MOVE && h.from == R4 + 2);
    waste(S(DIAMONDS, 12));                     /* nothing goes: the stock */
    h = tp_hint(P);
    CHECK(h.kind == SM_DRAW && tp_legal(P, h));
    P->n[TP_STOCK] = 0;
    CHECK(tp_hint(P).kind == SM_NONE && tp_stuck(P) && !any_move(P));
}

static void test_notation(void)
{
    tp_new(&G, TP_WRAP, 11);
    CHECK(!tp_script(&G, "1a") && !tp_script(&G, "5a") && !tp_script(&G, "4k") && !tp_script(&G, "z"));
    CHECK(tp_script(&G, "#12") && G.seed == 12 && G.n == 0 && P->n[TP_STOCK] == 23);
    CHECK(tp_script(&G, "s s ?") && G.moves == 3);
}

/* ---- whole games: random legal moves; the hint always legal; the end exactly when no move is left ------------- */
static void test_random(void)
{
    int games = 0, won = 0, ok = 1, best = 0;
    for (uint32_t seed = 1; seed <= 400; seed++) {
        tp_new(&G, seed % 2 ? TP_WRAP : TP_NOWRAP, seed);
        solpos start = G.pos;
        uint32_t rng = seed;
        for (int step = 0; step < 200 && !tp_won(P); step++) {
            solmove h = tp_hint(P);
            if (h.kind == SM_NONE) {
                if (any_move(P)) ok = 0, printf("FAIL seed %u: no hint, but a move\n", seed);
                CHECK(tp_stuck(P));
                break;
            }
            if (!tp_legal(P, h)) ok = 0, printf("FAIL seed %u: the hint is not legal\n", seed);
            solmove list[32];
            int n = 0;
            for (int a = 0; a < TP_PILES; a++)
                if (tp_legal(P, sol_mv(SM_MOVE, a, TP_WASTE, 1))) list[n++] = sol_mv(SM_MOVE, a, TP_WASTE, 1);
            if (tp_rules.stock(P).kind != SM_NONE) list[n++] = tp_rules.stock(P);
            solmove m = cards_rand(&rng) % 4 ? h : list[cards_rand(&rng) % (uint32_t)n];
            if (!tp_play(&G, m)) ok = 0;
            if (!whole(P)) ok = 0, printf("FAIL seed %u: cards lost\n", seed);
            for (int i = 0; i < TP_SLOTS; i++)  /* free cards face up, covered ones face down (row 4 always up) */
                if (P->n[i] && P->down[i] == sol_slot_free(P, tp_slots, TP_SLOT, i)) ok = 0;
        }
        games++;
        won += tp_won(P);
        if (tp_best_streak(&G) > best) best = tp_best_streak(&G);
        int moves = G.moves;
        for (int k = 0; k < moves; k++)
            if (!sol_undo(&G)) ok = 0;
        if (memcmp(&G.pos, &start, sizeof start) || G.n || G.moves) ok = 0, printf("FAIL seed %u: undo\n", seed);
    }
    CHECK(ok);
    printf("ok   %d random games (%d won, the longest streak %d)\n", games, won, best);
}

/* ---- a deal solved: a depth-first search knowing every card, then the moves played again from the script ----- */
static int nodes;
static solmove path[64];

static int solve(solrec *r, int depth)
{
    if (tp_won(&r->pos)) return depth;
    if (++nodes > 400000 || depth >= 64) return -1;
    solmove list[32];
    int n = 0;
    for (int a = 0; a < TP_SLOTS; a++)
        if (tp_legal(&r->pos, sol_mv(SM_MOVE, a, TP_WASTE, 1))) list[n++] = sol_mv(SM_MOVE, a, TP_WASTE, 1);
    if (r->pos.n[TP_STOCK]) list[n++] = sol_mv(SM_DRAW, TP_STOCK, TP_WASTE, 1);
    for (int i = 0; i < n; i++) {
        solpos save = r->pos;
        int sn = r->n, sm = r->moves;
        tp_play(r, list[i]);
        path[depth] = list[i];
        int d = solve(r, depth + 1);
        if (d >= 0) return d;
        r->pos = save;
        r->n = sn;
        r->moves = sm;
    }
    return -1;
}

static void test_solved(void)
{
    int solved = 0;
    tp_new(&G, TP_WRAP, 0);
    for (uint32_t seed = 1; seed <= 40 && solved < 3; seed++) {
        tp_new(&H, TP_WRAP, seed);
        nodes = 0;
        int n = solve(&H, 0);
        if (n < 0) continue;
        char script[512], mv[4];
        sprintf(script, "#%u", seed);
        for (int i = 0; i < n; i++) {
            if (path[i].kind == SM_DRAW) strcpy(mv, "s");
            else sol_slot_write(tp_slots, path[i].from - TP_SLOT, mv);
            strcat(script, " ");
            strcat(script, mv);
        }
        CHECK(tp_script(&G, script) && tp_won(P) && G.moves == n && !tp_stuck(P));
        if (!solved) printf("ok   seed %u solved in %d moves: %s\n", seed, n, script);
        solved++;
    }
    CHECK(solved >= 2);
}

int main(void)
{
    test_deal();
    test_follow();
    test_play();
    test_hint();
    test_notation();
    test_random();
    test_solved();
    printf("tripeaks: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
