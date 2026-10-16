/*
 * Bao la kujifunza: the rules' test (the board's pits and their rings, the start, captures sown from the left or the
 * right kichwa or along the sowing's way, relay sowing, the capture that must be taken, takasa from the inner row,
 * the end, the notation, undo, whole random games on fixed seeds). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/bao.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static bao_pos P;

static void empty(void)
{
    memset(&P, 0, sizeof P);
    P.winner = -1;
}

static int same(const int *want)                /* player 0's ring: pit, seeds, ... -1; the others empty */
{
    uint8_t w[BAO_RING] = {0};
    for (int i = 0; want[i] >= 0; i += 2) w[want[i]] = (uint8_t)want[i + 1];
    return !memcmp(w, P.pit[0], BAO_RING);
}

static unsigned rnd = 1;
static int rand_n(int n) { rnd = rnd * 1103515245u + 12345u; return (int)((rnd >> 16) % (unsigned)n); }

static void test_board(void)
{
    int seen[8][4] = {{0}}, c, r, c2, r2, pl;
    for (int p = 0; p < 2; p++)
        for (int i = 0; i < BAO_RING; i++) {
            bao_cell(p, i, &c, &r);
            seen[c][r]++;
            CHECK(bao_at(c, r, &pl) == i && pl == p);
            CHECK((r == 1 || r == 2) == (i < 8));          /* the inner rows by the middle */
            CHECK((r >= 2) == p);
            bao_cell(p, (i + 1) & 15, &c2, &r2);           /* the ring: each step to a neighbouring pit */
            CHECK(abs(c - c2) + abs(r - r2) == 1);
            if (i < 8) {                                   /* facing pits share a column */
                bao_cell(!p, 7 - i, &c2, &r2);
                CHECK(c2 == c && abs(r2 - r) == 1);
            }
        }
    for (c = 0; c < 8; c++)
        for (r = 0; r < 4; r++) CHECK(seen[c][r] == 1);
    /* clockwise for both: ivory's inner row goes right (seen from above), ebony's inner row goes left */
    bao_cell(0, 0, &c, &r);
    bao_cell(0, 1, &c2, &r2);
    CHECK(c2 == c + 1);
    bao_cell(1, 0, &c, &r);
    bao_cell(1, 1, &c2, &r2);
    CHECK(c2 == c - 1);
}

static void test_start(void)
{
    bao_start(&P);
    CHECK(bao_seeds(&P, 0) == 32 && bao_seeds(&P, 1) == 32 && P.turn == 0 && P.winner == -1);
    int mv[32], n = bao_moves(&P, mv);
    CHECK(n > 0);
    for (int i = 0; i < n; i++) CHECK(bao_capturing(&P, mv[i]));   /* captures at once: they must be taken */
    CHECK(bao_capturing(&P, bao_mv(0, 1)) && !bao_capturing(&P, bao_mv(0, 0)));   /* 0 -> 2 takes; 0 -> 14 not */
}

static void test_captures(void)
{
    /* the left kimbi: 3 sown anticlockwise ends on 1 (held 1); 3 taken, sown from kichwa 0 rightwards: 0, 1, 2;
     * 2 held one: relay, 3, 4: the end */
    empty();
    P.pit[0][3] = 2, P.pit[0][1] = 1, P.pit[1][6] = 3, P.pit[1][0] = 2;
    CHECK(bao_capturing(&P, bao_mv(3, 0)) && !bao_capturing(&P, bao_mv(3, 1)));
    int mv[32];
    CHECK(bao_moves(&P, mv) == 1 && mv[0] == bao_mv(3, 0));
    bao_trace t;
    bao_apply(&P, bao_mv(3, 0), &t);
    static const int a[] = {0, 1, 1, 3, 3, 1, 4, 1, -1};
    CHECK(same(a) && !P.pit[1][6] && t.captures == 1 && P.turn == 1 && P.winner == -1);
    /* the right kimbi: from kichwa 7 leftwards */
    empty();
    P.pit[0][4] = 2, P.pit[0][6] = 1, P.pit[1][1] = 3, P.pit[1][7] = 2;
    bao_apply(&P, bao_mv(4, 1), NULL);
    static const int b[] = {3, 1, 4, 1, 6, 3, 7, 1, -1};
    CHECK(same(b));
    /* the middle: the way the sowing turned. Clockwise: from kichwa 0 */
    empty();
    P.pit[0][2] = 2, P.pit[0][4] = 1, P.pit[1][3] = 2, P.pit[1][7] = 2;
    bao_apply(&P, bao_mv(2, 1), NULL);
    static const int c[] = {0, 1, 1, 1, 3, 1, 4, 2, -1};
    CHECK(same(c));
    /* anticlockwise: from kichwa 7 */
    empty();
    P.pit[0][5] = 2, P.pit[0][3] = 1, P.pit[1][4] = 2, P.pit[1][0] = 2;
    bao_apply(&P, bao_mv(5, 0), NULL);
    static const int d[] = {3, 2, 4, 1, 6, 1, 7, 1, -1};
    CHECK(same(d));
    /* the other player's inner row emptied: the win */
    empty();
    P.pit[0][3] = 2, P.pit[0][1] = 1, P.pit[1][6] = 3, P.pit[1][10] = 4;
    bao_apply(&P, bao_mv(3, 0), NULL);
    CHECK(P.winner == 0);
}

static void test_takasa(void)
{
    /* no capture: only the inner row's pits of 2 or more, both ways */
    empty();
    P.pit[0][2] = 3, P.pit[0][10] = 2, P.pit[1][0] = 1;
    int mv[32], n = bao_moves(&P, mv);
    CHECK(n == 2 && bao_mv_pit(mv[0]) == 2 && bao_mv_pit(mv[1]) == 2);
    /* the inner row has no pit of 2: the outer row then */
    empty();
    P.pit[0][2] = 1, P.pit[0][10] = 2, P.pit[1][0] = 1;
    n = bao_moves(&P, mv);
    CHECK(n == 2 && bao_mv_pit(mv[0]) == 10);
    /* relay sowing: 6 clockwise to 7, 8 (held 1): 8 taken up, 9, 10: the end */
    empty();
    P.pit[0][6] = 2, P.pit[0][8] = 1, P.pit[1][7] = 1;
    bao_apply(&P, bao_mv(6, 1), NULL);
    static const int a[] = {7, 1, 9, 1, 10, 1, -1};
    CHECK(same(a));
    /* a takasa captures nothing, even when its relay ends on an inner pit facing seeds: 9 clockwise (2) to 10, 11
     * (held 1): relay 2 from 11 to 12, 13 (held 2): relay 3 from 13 to 14, 15, 0 (held 1): 0 faces ebony's 7 */
    empty();
    P.pit[0][9] = 2, P.pit[0][11] = 1, P.pit[0][13] = 2, P.pit[0][0] = 1, P.pit[1][7] = 3, P.pit[1][3] = 1;
    n = bao_moves(&P, mv);
    CHECK(!bao_capturing(&P, bao_mv(9, 1)));
    bao_apply(&P, bao_mv(9, 1), NULL);
    CHECK(P.pit[1][7] == 3 && bao_seeds(&P, 0) == 6);
    /* a player who cannot move loses */
    empty();
    P.pit[0][2] = 2, P.pit[0][4] = 0, P.pit[1][0] = 1, P.pit[1][5] = 1;
    bao_apply(&P, bao_mv(2, 1), NULL);
    CHECK(P.winner == 0);
}

static void test_game(void)
{
    static bao_game G;
    bao_new(&G);
    char name[4];
    int mv[32], n = bao_moves(bao_now(&G), mv);
    bao_name(bao_now(&G), mv[0], name);
    int m;
    CHECK(bao_parse(bao_now(&G), name, &m) == 3 && m == mv[0]);
    CHECK(bao_script(&G, name) && G.n == 1 && bao_now(&G)->turn == 1);
    CHECK(!bao_script(&G, "a2+"));               /* ivory's pit: not the player to move's */
    CHECK(!bao_script(&G, "z9+") && !bao_script(&G, "a3"));
    CHECK(bao_undo(&G) && G.n == 0 && !bao_undo(&G));
    (void)n;
    /* whole random games: the seeds stay 64, a move is always there while it lasts, every game ends */
    int ended = 0, games = 300, longest = 0;
    for (int gn = 0; gn < games; gn++) {
        rnd = (unsigned)gn * 2654435761u + 1;
        bao_new(&G);
        int ok = 1;
        while (bao_now(&G)->winner < 0 && ok) {
            const bao_pos *p = bao_now(&G);
            n = bao_moves(p, mv);
            ok = n > 0 && bao_play(&G, mv[rand_n(n)], NULL);
            ok = ok && bao_seeds(bao_now(&G), 0) + bao_seeds(bao_now(&G), 1) == BAO_SEEDS;
        }
        CHECK(ok);
        ended += bao_now(&G)->winner >= 0;
        if (G.n > longest) longest = G.n;
    }
    CHECK(ended == games);
    printf("     %d random games, the longest %d moves\n", games, longest);
}

int main(void)
{
    test_board();
    test_start();
    test_captures();
    test_takasa();
    test_game();
    printf("bao: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
