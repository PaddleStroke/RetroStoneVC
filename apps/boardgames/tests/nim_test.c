/*
 * The Marienbad game (Nim): the rules' test (src/games/nim_rules.c). The layouts (Marienbad's 1 3 5 7, the random
 * ones), legal and illegal takes, the turn, misere and normal play's ends, undo, the notation, the winning strategy,
 * and whole random games from seeds. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>

#include "games/nim.h"

static int checks, failed;

#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint32_t rng = 12345;
static int rnd(int n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (int)(rng % (uint32_t)n);
}

/* a random legal move: a row with matches, a random non-empty subset of them */
static void random_move(const nim_game *g, int *row, int *mask)
{
    do *row = rnd(g->rows); while (!g->mask[*row]);
    do *mask = g->mask[*row] & rnd(256); while (!*mask);
}

/* plays the strategy against random moves: the strategist (player who) must win from a winning start */
static int strategy_wins(int variant, uint32_t seed, int who)
{
    nim_game g;
    nim_new(&g, variant, seed);
    while (nim_status(&g) == NIM_PLAY) {
        int row, mask;
        if (g.turn == who) {
            if (!nim_best(&g, &row, &mask)) return -1;   /* the strategist faces a lost position */
        } else {
            random_move(&g, &row, &mask);
        }
        if (!nim_play(&g, row, mask)) return -2;
    }
    return nim_status(&g) == NIM_WIN0 + who;
}

int main(void)
{
    nim_game g;

    /* Marienbad: 1, 3, 5, 7 matches from the top; misere; ivory first */
    nim_new(&g, NIM_MARIENBAD, 1);
    CHECK(g.rows == 4 && g.start[0] == 1 && g.start[1] == 3 && g.start[2] == 5 && g.start[3] == 7);
    CHECK(g.mask[3] == 0x7f && nim_left(&g) == 16 && g.misere && g.turn == 0 && nim_status(&g) == NIM_PLAY);
    nim_new(&g, NIM_MARIENBAD_NORMAL, 1);
    CHECK(g.rows == 4 && !g.misere && nim_left(&g) == 16);

    /* the random layouts: 3 to 5 rows of 1 to 8, the shortest at the top, the same for the same seed */
    int ok = 1, rows_seen = 0;
    for (uint32_t seed = 1; seed < 400; seed++) {
        nim_game a, b;
        nim_new(&a, NIM_RANDOM, seed);
        nim_new(&b, NIM_RANDOM_NORMAL, seed);
        ok &= a.rows >= 3 && a.rows <= 5 && a.misere && !b.misere && a.rows == b.rows;
        rows_seen |= 1 << a.rows;
        for (int r = 0; r < a.rows; r++) {
            ok &= a.start[r] >= 1 && a.start[r] <= NIM_LONG && a.start[r] == b.start[r];
            ok &= a.mask[r] == (1 << a.start[r]) - 1;
            if (r) ok &= a.start[r - 1] <= a.start[r];
        }
    }
    CHECK(ok && rows_seen == (1 << 3 | 1 << 4 | 1 << 5));

    /* a take: any number from one row, the matches that are there */
    nim_new(&g, NIM_MARIENBAD, 1);
    CHECK(!nim_legal(&g, 0, 0) && !nim_legal(&g, 0, 2) && !nim_legal(&g, 4, 1) && !nim_legal(&g, -1, 1));
    CHECK(nim_legal(&g, 3, 0x7f) && nim_legal(&g, 2, 0x15) && !nim_legal(&g, 2, 0x20));
    CHECK(nim_play(&g, 2, 0x15) && g.mask[2] == 0x0a && g.taken[0] == 3 && g.turn == 1 && nim_left(&g) == 13);
    CHECK(!nim_play(&g, 2, 0x01) && g.turn == 1);                   /* taken already */
    CHECK(nim_play(&g, 3, 0x40) && g.taken[1] == 1 && g.turn == 0);

    /* misere: who takes the last match loses */
    CHECK(nim_script(&g, "a1 b3 c2 d6") && nim_left(&g) == 0);   /* ebony took the last */
    CHECK(nim_status(&g) == NIM_WIN0 && !nim_play(&g, 0, 1));
    nim_new(&g, NIM_MARIENBAD, 1);
    CHECK(nim_script(&g, "d7 c5 b3") && nim_status(&g) == NIM_PLAY && nim_script(&g, "a1"));
    CHECK(nim_status(&g) == NIM_WIN0);                              /* ebony took the last one */
    /* normal play: who takes the last match wins */
    nim_new(&g, NIM_MARIENBAD_NORMAL, 1);
    CHECK(nim_script(&g, "d7 c5 b3 a1") && nim_status(&g) == NIM_WIN1);
    nim_new(&g, NIM_MARIENBAD_NORMAL, 1);
    CHECK(nim_script(&g, "d7 c5 b3 a1") && !nim_script(&g, "a1"));

    /* the notation: places from the left; a count takes the rightmost; bad words */
    nim_new(&g, NIM_MARIENBAD, 1);
    CHECK(nim_script(&g, "d:036") && g.mask[3] == 0x36 && nim_script(&g, "d2") && g.mask[3] == 0x06);
    CHECK(!nim_script(&g, "e1") && !nim_script(&g, "a2") && !nim_script(&g, "b0") && !nim_script(&g, "x"));

    /* undo: back to the start, the turn and the counts too */
    nim_new(&g, NIM_MARIENBAD, 1);
    CHECK(nim_script(&g, "d3 b:1 c5"));
    CHECK(nim_undo(&g) && g.turn == 0 && g.mask[2] == 0x1f && g.taken[0] == 3 && g.taken[1] == 1);
    CHECK(nim_undo(&g) && nim_undo(&g) && !nim_undo(&g) && nim_left(&g) == 16 && g.taken[0] == 0 && g.turn == 0);
    CHECK(nim_script(&g, "d7 c5 b3 a1") && nim_undo(&g) && nim_status(&g) == NIM_PLAY && g.mask[0] == 1);

    /* the strategy: Marienbad's 1 3 5 7 is lost for the first player (its nim-sum is zero), in both plays */
    nim_new(&g, NIM_MARIENBAD, 1);
    int row, mask;
    CHECK(!nim_best(&g, &row, &mask));
    nim_new(&g, NIM_MARIENBAD_NORMAL, 1);
    CHECK(!nim_best(&g, &row, &mask));
    CHECK(nim_script(&g, "d1") && nim_best(&g, &row, &mask) && row == 0 && mask == 1);   /* 1 3 5 6: take the 1 */
    /* misere's end: three single matches lose for the one to move; a big row is cut to leave an odd number */
    nim_new(&g, NIM_MARIENBAD, 1);
    CHECK(nim_script(&g, "a1 b2 c4 d6") && !nim_best(&g, &row, &mask));
    nim_new(&g, NIM_MARIENBAD, 1);
    CHECK(nim_script(&g, "b2 c4 d3") && nim_best(&g, &row, &mask) && row == 3 && nim_count((uint8_t)mask) == 4);
    nim_new(&g, NIM_MARIENBAD, 1);
    CHECK(nim_script(&g, "a1 b2 c4 d3") && nim_best(&g, &row, &mask) && row == 3 && nim_count((uint8_t)mask) == 3);

    /* the strategy wins from winning starts against random play: the second player at Marienbad, whoever can win a
     * random layout */
    ok = 1;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        ok &= strategy_wins(NIM_MARIENBAD, seed, 1) == 1;
        ok &= strategy_wins(NIM_MARIENBAD_NORMAL, seed, 1) == 1;
        for (int v = NIM_RANDOM; v <= NIM_RANDOM_NORMAL; v++) {
            nim_new(&g, v, seed);
            int first_wins = nim_best(&g, &row, &mask);
            ok &= strategy_wins(v, seed, !first_wins) == 1;
        }
    }
    CHECK(ok);

    /* whole random games from seeds: every match taken once, the counts add up, undo goes back to the start */
    ok = 1;
    for (uint32_t seed = 1; seed <= 500; seed++) {
        int v = (int)(seed % NIM_VARIANTS);
        nim_new(&g, v, seed);
        int total = nim_left(&g), moves = 0;
        while (nim_status(&g) == NIM_PLAY) {
            random_move(&g, &row, &mask);
            int t = g.turn;
            ok &= nim_play(&g, row, mask) && g.turn == !t;
            moves++;
        }
        ok &= g.taken[0] + g.taken[1] == total && g.n == moves && nim_left(&g) == 0;
        int last = (moves - 1) & 1;
        ok &= nim_status(&g) == NIM_WIN0 + (g.misere ? !last : last);
        while (nim_undo(&g)) {}
        ok &= nim_left(&g) == total && g.turn == 0 && g.taken[0] == 0 && g.taken[1] == 0;
    }
    CHECK(ok);

    printf("nim: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
