/*
 * Senet: the rules' test (the snake path, the start, the extra throws, swaps, protected pairs, blocking threes, the
 * House of Beauty, the House of Water, the exact exits, moving back, a lost throw, the win, undo, the notation, whole
 * random games on fixed seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/senet.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static senet_game G;
static const senet_pos *P = &G.pos;

/* a position: the squares of each player's pieces (0-terminated; the others borne off), the player to throw */
static void setup(const int *a, const int *b, int turn)
{
    senet_new(&G, SENET_FIVE);
    memset(G.pos.sq, 0, sizeof G.pos.sq);
    const int *l[2] = {a, b};
    for (int pl = 0; pl < 2; pl++) {
        int n = 0;
        for (int i = 0; l[pl] && l[pl][i]; i++, n++) G.pos.sq[l[pl][i]] = (uint8_t)(1 + pl);
        G.pos.off[pl] = (uint8_t)(G.pos.pieces - n);
    }
    G.pos.turn = (uint8_t)turn;
}

static unsigned rnd = 1;
static int rand_n(int n) { rnd = rnd * 1103515245u + 12345u; return (int)((rnd >> 16) % (unsigned)n); }
static int sticks(void) { int v = 0; for (int i = 0; i < 4; i++) v += rand_n(2); return v ? v : 5; }

static void test_path(void)
{
    int seen[3][10] = {{0}}, c, r, pc = 0, pr = 0;
    for (int s = 1; s <= 30; s++) {
        CHECK(senet_cell(s, &c, &r));
        seen[r][c]++;
        if (s > 1) CHECK(abs(c - pc) + abs(r - pr) == 1);   /* each step to a neighbouring square */
        pc = c, pr = r;
    }
    for (r = 0; r < 3; r++)
        for (c = 0; c < 10; c++) CHECK(seen[r][c] == 1);
    CHECK(senet_cell(1, &c, &r) && c == 0 && r == 0);
    CHECK(senet_cell(11, &c, &r) && c == 9 && r == 1);
    CHECK(senet_cell(15, &c, &r) && c == 5 && r == 1);
    CHECK(senet_cell(26, &c, &r) && c == 5 && r == 2);
    CHECK(senet_cell(30, &c, &r) && c == 9 && r == 2);
    CHECK(!senet_cell(0, &c, &r) && !senet_cell(31, &c, &r));
}

static void test_start(void)
{
    senet_new(&G, SENET_FIVE);
    int n[2] = {0, 0};
    for (int s = 1; s <= 30; s++)
        if (P->sq[s]) { n[P->sq[s] - 1]++; CHECK(s <= 10 && P->sq[s] == (s & 1 ? 1 : 2)); }
    CHECK(n[0] == 5 && n[1] == 5 && P->turn == 0 && P->phase == SENET_THROW && P->winner == -1);
    senet_new(&G, SENET_SEVEN);
    n[0] = n[1] = 0;
    for (int s = 1; s <= 30; s++)
        if (P->sq[s]) { n[P->sq[s] - 1]++; CHECK(s <= 14); }
    CHECK(n[0] == 7 && n[1] == 7 && P->pieces == 7);
    CHECK(senet_again(1) && senet_again(4) && senet_again(5) && !senet_again(2) && !senet_again(3));
    /* the opening: every piece alone (the others' between them), so ivory's 9 can swap with ebony's 10 */
    senet_new(&G, SENET_FIVE);
    CHECK(senet_throw(&G, 1) == SENET_T_MOVE);
    CHECK(senet_takes(P, 9) && senet_dest(P, 9) == 10);
    CHECK(senet_move(&G, 9) == 2 && P->sq[10] == 1 && P->sq[9] == 2);
    CHECK(P->turn == 0 && P->phase == SENET_THROW);   /* a 1: another throw */
    CHECK(senet_throw(&G, 2) == SENET_T_MOVE && senet_move(&G, 10) && P->turn == 1);   /* a 2: the turn passes */
}

static void test_swaps(void)
{
    static const int a1[] = {5, 0}, b1[] = {8, 0};
    setup(a1, b1, 0);
    senet_throw(&G, 3);
    CHECK(senet_dest(P, 5) == 8 && senet_takes(P, 5));
    senet_move(&G, 5);
    CHECK(P->sq[8] == 1 && P->sq[5] == 2 && P->turn == 1);
    /* two in a row are protected: no landing on either */
    static const int a2[] = {5, 20, 0}, b2[] = {8, 9, 0};   /* 20: a forward move, so no going back */
    setup(a2, b2, 0);
    senet_throw(&G, 3);
    CHECK(senet_dest(P, 5) == -1);
    CHECK(senet_protected(P, 8) && senet_protected(P, 9));
    /* ...but they can be jumped over: 5 + 5 = 10 */
    setup(a2, b2, 0);
    senet_throw(&G, 5);
    CHECK(senet_dest(P, 5) == 10 && !senet_takes(P, 5));
    /* three in a row block the way: 5 + 5 would pass 7, 8, 9 */
    static const int a3[] = {5, 20, 0}, b3[] = {7, 8, 9, 0};
    setup(a3, b3, 0);
    senet_throw(&G, 5);
    CHECK(senet_dest(P, 5) == -1);
    /* own pieces: never landed on, but jumped over */
    static const int a4[] = {5, 7, 0}, b4[] = {20, 0};
    setup(a4, b4, 0);
    senet_throw(&G, 2);
    CHECK(senet_dest(P, 5) == -1 && senet_dest(P, 7) == 9);
    setup(a4, b4, 0);
    senet_throw(&G, 3);
    CHECK(senet_dest(P, 5) == 8);
}

static void test_houses(void)
{
    /* 26: no throw jumps it; from 24 a 2 lands on it, a 3 cannot go */
    static const int a1[] = {24, 0}, b1[] = {2, 0};
    setup(a1, b1, 0);
    senet_throw(&G, 2);
    CHECK(senet_dest(P, 24) == 26);
    setup(a1, b1, 0);
    senet_throw(&G, 3);                         /* 24 cannot go forwards: back to 21 */
    CHECK(P->back && senet_dest(P, 24) == 21);
    /* 27: the water, back to 15 */
    static const int a2[] = {26, 0}, b2[] = {2, 0};
    setup(a2, b2, 0);
    senet_throw(&G, 1);
    CHECK(senet_dest(P, 26) == 27);
    senet_move(&G, 26);
    CHECK(P->sq[15] == 1 && !P->sq[26] && !P->sq[27]);
    /* 15 taken: the first free square before it */
    static const int a3[] = {26, 0}, b3[] = {15, 14, 0};
    setup(a3, b3, 0);
    senet_throw(&G, 1);
    senet_move(&G, 26);
    CHECK(P->sq[13] == 1 && P->sq[15] == 2 && P->sq[14] == 2);
    /* the exits: 26 a 5, 28 a 3, 29 a 2, 30 a 1; more: no */
    static const int sq[4] = {26, 28, 29, 30}, need[4] = {5, 3, 2, 1};
    for (int i = 0; i < 4; i++) {
        int a[] = {sq[i], 0};
        static const int b[] = {2, 0};
        setup(a, b, 0);
        senet_throw(&G, need[i]);
        CHECK(senet_dest(P, sq[i]) == SENET_OFF);
        if (need[i] < 5) {
            setup(a, b, 0);
            senet_throw(&G, need[i] + 1);
            CHECK(senet_dest(P, sq[i]) != SENET_OFF);
        }
    }
    /* 28 moves on to 29 with a 1 */
    static const int a4[] = {28, 0}, b4[] = {2, 0};
    setup(a4, b4, 0);
    senet_throw(&G, 1);
    CHECK(senet_dest(P, 28) == 29);
}

static void test_back_and_lost(void)
{
    /* no forward move: back. Ivory on 30 throws 3 (30 needs a 1): back to 27, the water, then 15 */
    static const int a1[] = {30, 0}, b1[] = {2, 0};
    setup(a1, b1, 0);
    CHECK(senet_throw(&G, 3) == SENET_T_MOVE && P->back && senet_dest(P, 30) == 27);
    senet_move(&G, 30);
    CHECK(P->sq[15] == 1 && P->turn == 1);
    /* a forward move exists: no going back */
    static const int a2[] = {30, 10, 0}, b2[] = {2, 0};
    setup(a2, b2, 0);
    senet_throw(&G, 3);
    CHECK(!P->back && senet_dest(P, 30) == -1 && senet_dest(P, 10) == 13);
    /* nothing at all: the throw is lost, even a 5 */
    static const int a3[] = {1, 0}, b3[] = {2, 3, 4, 5, 6, 0};
    setup(a3, b3, 0);
    CHECK(senet_throw(&G, 5) == SENET_T_PASS && P->turn == 1 && P->phase == SENET_THROW);
    /* backwards a swap too: ivory 30 throws 4, 26 holds ebony alone */
    static const int a4[] = {30, 0}, b4[] = {26, 2, 0};
    setup(a4, b4, 0);
    senet_throw(&G, 4);
    CHECK(P->back && senet_takes(P, 30));
    senet_move(&G, 30);
    CHECK(P->sq[26] == 1 && P->sq[30] == 2);
}

static void test_win_undo(void)
{
    static const int a1[] = {30, 0}, b1[] = {2, 0};
    setup(a1, b1, 0);
    senet_throw(&G, 1);
    CHECK(senet_move(&G, 30) && P->winner == 0 && P->phase == SENET_OVER && P->off[0] == 5);
    CHECK(!senet_throw(&G, 2));
    /* undo: back to the choice before the last move, its throw kept */
    senet_new(&G, SENET_FIVE);
    CHECK(senet_script(&G, "1 s9 2 s10"));
    CHECK(P->turn == 1 && G.n == 4);
    CHECK(senet_undo(&G) && G.n == 3 && P->phase == SENET_MOVE && P->roll == 2);
    CHECK(senet_undo(&G) && G.n == 1 && P->phase == SENET_MOVE && P->roll == 1);
    CHECK(senet_undo(&G) && G.n == 0);
    CHECK(!senet_undo(&G));
    CHECK(!senet_script(&G, "1 s2"));           /* ebony's piece: not the player to move's */
    senet_new(&G, SENET_FIVE);
    CHECK(!senet_script(&G, "6") && !senet_script(&G, "x"));
}

/* whole games of random moves: they end, nothing is lost or made, the history keeps every position */
static void test_random(void)
{
    int ended = 0, total = 0;
    for (int gn = 0; gn < 400; gn++) {
        rnd = (unsigned)gn * 7919u + 3;
        senet_new(&G, gn & 1 ? SENET_SEVEN : SENET_FIVE);
        int ok = 1;
        for (int ply = 0; ply < 20000 && P->phase != SENET_OVER && ok; ply++) {
            if (P->phase == SENET_THROW) { ok = senet_throw(&G, sticks()) != 0; continue; }
            uint32_t m = senet_movable(P);
            int n = __builtin_popcount(m), k = rand_n(n);
            ok = n > 0;
            while (ok && k--) m &= m - 1;
            if (ok) ok = senet_move(&G, __builtin_ctz(m)) != 0;
            int c[2] = {P->off[0], P->off[1]};
            for (int s = 1; s <= 30; s++)
                if (P->sq[s]) c[P->sq[s] - 1]++;
            ok = ok && c[0] == P->pieces && c[1] == P->pieces && !P->sq[SENET_WATER];
        }
        CHECK(ok);
        total++;
        ended += P->phase == SENET_OVER && P->winner >= 0;
    }
    CHECK(ended == total);
}

int main(void)
{
    test_path();
    test_start();
    test_swaps();
    test_houses();
    test_back_and_lost();
    test_win_undo();
    test_random();
    printf("senet: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
