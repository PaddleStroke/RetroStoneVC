/*
 * Four in a row: the rules' test (src/games/fourrow_rules.c). Wins in the four directions and at the edges, a full
 * column, the draw, undo. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>

#include "games/fourrow.h"

static int checks, failed;

#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* plays "4 4 3 5" (columns 1-7) from the start; 0: a move was refused */
static int play(fr_game *g, const char *s)
{
    fr_new(g);
    for (char *end; *s; s = end) {
        long c = strtol(s, &end, 10);
        if (end == s) break;
        if (fr_play(g, (int)c - 1) < 0) return 0;
    }
    return 1;
}

static uint64_t bits(const int (*hole)[2], int n)
{
    uint64_t m = 0;
    for (int i = 0; i < n; i++) m |= FR_BIT(hole[i][0], hole[i][1]);
    return m;
}

/* the board full, nobody four in a row: rows of 0011001 and 1100110, one on the other */
static const char *DRAW =
    "1 3 2 4 5 7 6 1 3 2 4 5 7 6  1 3 2 4 5 7 6 1 3 2 4 5 7 6  1 3 2 4 5 7 6 1 3 2 4 5 7 6";

int main(void)
{
    fr_game g;

    /* the start */
    fr_new(&g);
    CHECK(fr_status(&g) == FR_PLAY && fr_turn(&g) == 0 && fr_left(&g, 0) == 21 && fr_left(&g, 1) == 21);
    CHECK(fr_play(&g, 3) == 0 && fr_play(&g, 3) == 1 && fr_play(&g, 2) == 0);   /* the row a disc lands in */
    CHECK(fr_turn(&g) == 1 && fr_left(&g, 0) == 19 && fr_left(&g, 1) == 20);
    CHECK(fr_play(&g, -1) < 0 && fr_play(&g, FR_COLS) < 0 && g.n == 3);

    /* across, on the bottom row from the left edge */
    CHECK(play(&g, "1 1 2 2 3 3 4") && fr_status(&g) == FR_WIN0);
    CHECK(g.line == bits((const int[][2]){{0, 0}, {1, 0}, {2, 0}, {3, 0}}, 4));

    /* up, at the right edge */
    CHECK(play(&g, "7 1 7 1 7 1 7") && fr_status(&g) == FR_WIN0);
    CHECK(g.line == bits((const int[][2]){{6, 0}, {6, 1}, {6, 2}, {6, 3}}, 4));

    /* up, to the top of the board */
    CHECK(play(&g, "1 1 1 2 1 2 1 3 1") && fr_status(&g) == FR_WIN0);
    CHECK(g.line == bits((const int[][2]){{0, 2}, {0, 3}, {0, 4}, {0, 5}}, 4));

    /* a slant rising to the right, from the bottom left corner */
    CHECK(play(&g, "1 2 2 3 4 3 3 4 5 4 4") && fr_status(&g) == FR_WIN0);
    CHECK(g.line == bits((const int[][2]){{0, 0}, {1, 1}, {2, 2}, {3, 3}}, 4));

    /* a slant falling to the right, to the bottom right corner: player 1 wins */
    CHECK(play(&g, "1 7 6 6 5 4 5 5 4 3 4 4") && fr_status(&g) == FR_WIN1);
    CHECK(g.line == bits((const int[][2]){{6, 0}, {5, 1}, {4, 2}, {3, 3}}, 4));

    /* five in a row: all five light up; nothing more after a win */
    CHECK(play(&g, "1 1 2 2 4 4 5 5 3") && fr_status(&g) == FR_WIN0);
    CHECK(g.line == bits((const int[][2]){{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}}, 5));
    CHECK(fr_play(&g, 6) < 0 && g.n == 9);

    /* a full column is refused, the turn stays */
    CHECK(play(&g, "1 1 1 1 1 1") && fr_status(&g) == FR_PLAY);
    CHECK(fr_play(&g, 0) < 0 && g.n == 6 && fr_turn(&g) == 0 && fr_play(&g, 1) == 0);
    CHECK(!play(&g, "2 2 2 2 2 2 2"));

    /* the draw: the board full, nobody four in a row */
    CHECK(play(&g, DRAW) && g.n == FR_HOLES && fr_status(&g) == FR_DRAW && !g.line);
    CHECK(fr_left(&g, 0) == 0 && fr_left(&g, 1) == 0 && fr_play(&g, 0) < 0);

    /* undo: the win taken back, then every disc to the start */
    CHECK(play(&g, "1 1 2 2 3 3 4") && fr_undo(&g));
    CHECK(fr_status(&g) == FR_PLAY && !g.line && g.hole[0][3] == -1 && g.height[3] == 0 && fr_turn(&g) == 0);
    CHECK(fr_play(&g, 3) == 0 && fr_status(&g) == FR_WIN0);
    while (fr_undo(&g)) {}
    fr_game fresh;
    fr_new(&fresh);
    CHECK(g.n == 0 && !fr_undo(&g) && fr_status(&g) == FR_PLAY && fr_turn(&g) == 0);
    for (int c = 0; c < FR_COLS; c++) CHECK(g.height[c] == 0);
    for (int r = 0; r < FR_ROWS; r++)
        for (int c = 0; c < FR_COLS; c++) CHECK(g.hole[r][c] == fresh.hole[r][c]);

    printf("fourrow: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
