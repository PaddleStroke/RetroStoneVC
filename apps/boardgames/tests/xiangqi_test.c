/*
 * Xiangqi's rules test (src/games/xiangqi_rules.c, no SDL): each piece's moves (the blocked elephant, the hobbled
 * horse, the cannon's screen, the soldier before and after the river, the palace), the flying general, check, mate,
 * the stalemate that loses, the repetition draw, undo, and the move counts from the start (perft).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/xiangqi.h"

static int fails, checks;
static xpos P;
static xmove M[XQ_MAX_MOVES];
static int N;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* a position (player `side` to move) and its legal moves in M[0..N) */
static void pos(int side, const char *pieces0, const char *pieces1)
{
    if (!xq_setup(&P, side, pieces0, pieces1)) printf("bad setup %s / %s\n", pieces0, pieces1);
    N = xq_legal(&P, M);
}

static int has(const char *move) { return xq_parse(M, N, move) >= 0; }

/* the moves of the piece on `from`, as "from to" points written in s: exactly these ("b0a2 b0c2") */
static int only(const char *from, const char *moves)
{
    int f = xq_point(from), count = 0, want = 0;
    for (int i = 0; i < N; i++) count += M[i].from == f;
    for (const char *s = moves; *s;) {
        if (*s == ' ') { s++; continue; }
        if (!has(s)) return 0;
        want++;
        s += 4;
    }
    return count == want;
}

static long perft(const xpos *p, int depth)
{
    xmove m[XQ_MAX_MOVES];
    int n = xq_legal(p, m);
    if (depth == 1) return n;
    long total = 0;
    for (int i = 0; i < n; i++) {
        xpos q = *p;
        xq_make(&q, m[i]);
        total += perft(&q, depth - 1);
    }
    return total;
}

static xgame G;

/* plays the moves on a fresh game; 0: one of them is not legal */
static int game(const char *moves)
{
    xq_new(&G);
    for (const char *s = moves; *s;) {
        if (*s == ' ') { s++; continue; }
        xmove m[XQ_MAX_MOVES];
        int k = xq_parse(m, xq_legal(xq_pos(&G), m), s);
        if (k < 0) return 0;
        xq_play(&G, m[k]);
        s += 4;
    }
    return 1;
}

int main(void)
{
    /* the start: 44, 1920, 79666 positions after 1, 2, 3 moves */
    xq_start(&P);
    CHECK(xq_general(&P, 0) == xq_point("e0") && xq_general(&P, 1) == xq_point("e9"));
    CHECK(perft(&P, 1) == 44);
    CHECK(perft(&P, 2) == 1920);
    CHECK(perft(&P, 3) == 79666);

    /* the general and the advisor stay in the palace; the general never steps onto the open file of the other */
    pos(0, "Kd0 Af0", "Kf9");
    CHECK(only("d0", "d0d1 d0e0") && only("f0", "f0e1"));
    pos(0, "Kd1", "Ke9");
    CHECK(!has("d1e1") && has("d1d0") && has("d1d2"));   /* the flying general */

    /* the elephant: two points diagonally, not when its eye is taken, never over the river */
    pos(0, "Kd0 Ec0 Hd1", "Kf9");
    CHECK(only("c0", "c0a2"));
    pos(0, "Kd0 Ee2", "Kf9");
    CHECK(only("e2", "e2c0 e2g0 e2c4 e2g4"));
    pos(0, "Kd0 Ec4", "Kf9");
    CHECK(only("c4", "c4a2 c4e2"));             /* not a6 or e6 */
    pos(1, "Kd0", "Kf9 Ec5");
    CHECK(only("c5", "c5a7 c5e7"));

    /* the horse: hobbled by a piece on its leg */
    pos(0, "Kd0 Hb0", "Kf9");
    CHECK(only("b0", "b0a2 b0c2 b0d1"));
    pos(0, "Kd0 Hb0 Pb1", "Kf9");
    CHECK(only("b0", "b0d1"));
    pos(0, "Kd0 He4 Pe5 Pd4", "Kf9");
    CHECK(only("e4", "e4d2 e4f2 e4g3 e4g5"));   /* its up and left legs taken */

    /* the chariot and the cannon: the cannon moves like the chariot, takes over exactly one piece */
    pos(0, "Kd0 Rb2 Cb3", "Kf9 Hb7 Rb9");
    CHECK(has("b2a2") && has("b2i2") && !has("b2b3"));
    CHECK(has("b3b6") && !has("b3b7") && has("b3b9") && !has("b3b8"));
    pos(0, "Kd0 Ca0", "Kf9 Ra5 Ra7");
    CHECK(!has("a0a5") && has("a0a7") && has("a0a4"));
    pos(0, "Kd0 Ca0", "Kf9 Ra1");
    CHECK(!has("a0a1"));                        /* no screen, no capture */

    /* the soldier: forward only before the river, sideways too after it, never back */
    pos(0, "Kd0 Pe3", "Kf9");
    CHECK(only("e3", "e3e4"));
    pos(0, "Kd0 Pe5", "Kf9");
    CHECK(only("e5", "e5e6 e5d5 e5f5"));
    pos(0, "Kd0 Pa9", "Kf9");
    CHECK(only("a9", "a9b9"));
    pos(1, "Kd0", "Kf9 Pe4");
    CHECK(only("e4", "e4e3 e4d4 e4f4"));

    /* a piece between the generals may not leave the file */
    pos(0, "Ke0 Ce5", "Ke9");
    CHECK(has("e5e8") && has("e5e1") && !has("e5d5") && !has("e5a5"));

    /* check: each kind of piece gives it, and every legal move answers it */
    pos(1, "Kd0 Rd5", "Ke9");                    /* no check */
    CHECK(!xq_in_check(&P, 1));
    pos(1, "Kd0 Re5", "Ke9 Ae8 Af9");            /* the advisor shields it */
    CHECK(!xq_in_check(&P, 1));
    pos(1, "Kd0 Re5 Ce4", "Ke9 Ae8");            /* two screens: no check */
    CHECK(!xq_in_check(&P, 1));
    pos(1, "Kd0 Ce5", "Ke9 Ae8");                /* the cannon over the advisor */
    CHECK(xq_in_check(&P, 1));
    pos(1, "Kd0 Hd7", "Ke9");                    /* the horse, its leg d8 free */
    CHECK(xq_in_check(&P, 1));
    pos(1, "Kd0 Hd7", "Ke9 Ad8");                /* hobbled */
    CHECK(!xq_in_check(&P, 1));
    pos(1, "Kd0 Pe8", "Ke9");                    /* a soldier ahead */
    CHECK(xq_in_check(&P, 1));
    pos(1, "Kd0 Pd9", "Ke9");                    /* a soldier beside */
    CHECK(xq_in_check(&P, 1));
    pos(1, "Ke0 Re5", "Ke9 Rh8");
    CHECK(xq_in_check(&P, 1) && N > 0);
    {
        int all = 1;
        for (int i = 0; i < N; i++) {
            xpos q = P;
            xq_make(&q, M[i]);
            all &= !xq_in_check(&q, 1);
        }
        CHECK(all && has("e9d9") && has("e9f9") && has("h8e8") && !has("h8h0"));
    }

    /* mate: the two chariots; the side to move lost */
    pos(1, "Kd0 Ra9 Rb8", "Ke9");
    CHECK(N == 0 && xq_in_check(&P, 1));
    /* stalemate: no move but not in check, and that loses too */
    pos(1, "Ke0 Rc8", "Kd9");
    CHECK(N == 0 && !xq_in_check(&P, 1));
    {
        xq_new(&G);
        G.pos[0] = P;
        CHECK(xq_status(&G) == XQ_STALEMATE);
        xq_setup(&G.pos[0], 1, "Kd0 Ra9 Rb8", "Ke9");
        CHECK(xq_status(&G) == XQ_MATE);
    }

    /* a few moves from the start (the cannon takes a soldier over its screen), then undo all the way */
    CHECK(game("h2e2 h9g7 h0g2 b9c7"));
    CHECK(xq_status(&G) == XQ_PLAY && !xq_in_check(xq_pos(&G), 0));
    CHECK(game("h2e2 e6e5 e2e5 h9g7 e5e8"));
    CHECK(G.move[2].cap == XQ_PC(1, XQ_SOLDIER) && xq_pos(&G)->sq[xq_point("e8")] == XQ_PC(0, XQ_CANNON));
    {
        xpos start;
        xq_start(&start);
        int n = 0;
        while (xq_undo(&G)) n++;
        CHECK(n == 5 && !memcmp(xq_pos(&G), &start, sizeof start) && !xq_undo(&G));
    }

    /* the same position a third time: a draw; a capture in between starts the count again */
    CHECK(game("h0g2 h9g7 g2h0 g7h9 h0g2 h9g7 g2h0"));
    CHECK(xq_status(&G) == XQ_PLAY);
    CHECK(game("h0g2 h9g7 g2h0 g7h9 h0g2 h9g7 g2h0 g7h9"));
    CHECK(xq_status(&G) == XQ_REPETITION);
    xq_undo(&G);
    CHECK(xq_status(&G) == XQ_PLAY);

    /* sixty moves each without a capture: a draw */
    xq_new(&G);
    G.pos[0].quiet = XQ_QUIET_PLIES - 1;
    CHECK(xq_status(&G) == XQ_PLAY);
    {
        xmove m[XQ_MAX_MOVES];
        int n = xq_legal(xq_pos(&G), m);
        xq_play(&G, m[xq_parse(m, n, "h0g2")]);
        CHECK(xq_status(&G) == XQ_QUIET);
    }

    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails != 0;
}
