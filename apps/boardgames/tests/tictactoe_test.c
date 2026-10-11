/*
 * Tic-tac-toe: the rules' test (src/games/tictactoe_rules.c). Classic: a win in every line, the draw. Ultimate: the
 * board a move sends to, the free choice when it is decided, a small board claimed, a small board full and nobody's,
 * the big win, the big draw, undo. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/tictactoe.h"

static int checks, failed;

#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* plays the moves from the start; 0: one was refused */
static int play(tt_game *g, int ultimate, const char *s)
{
    tt_new(g, ultimate);
    return tt_script(g, s);
}

static int cell(int board, int square) { return (board - 1) * 9 + square - 1; }   /* "5.1": board 5, square 1 */

/* the shortest games found by random play: a full small board (board 4, nobody's), a big win (boards 1, 4 and 7 for
 * player 0), a draw with a board still open (no line of boards left for anyone) */
static const char *FULL = "4.1 1.8 8.8 8.3 3.4 4.9 9.1 1.4 4.3 3.8 8.7 7.5 5.8 8.4 4.5 5.4 4.8 8.1 1.6 6.4 4.4 4.2 2.4 "
                          "4.7 7.4 4.6";
static const char *WIN = "1.3 3.7 7.1 1.4 4.5 5.1 1.5 5.7 7.9 9.8 8.1 1.9 9.7 7.7 7.5 5.4 4.9 9.1 1.7 6.5 4.1";
static const char *DRAW = "8.7 7.1 1.9 9.5 5.2 2.3 3.3 3.6 6.5 5.3 3.2 2.1 1.7 7.4 4.3 3.4 4.7 7.5 5.9 9.1 1.2 2.7 "
                          "7.3 3.5 5.7 7.6 6.7 9.9 1.5 5.6 6.3 5.1 1.3 4.8 8.3 8.8 8.6 2.5 5.5 5.8 8.5 5.4";

int main(void)
{
    tt_game g;

    /* classic: every line wins, for either player; nothing more after a win */
    for (int i = 0; i < 8; i++) {
        const int8_t *l = tt_lines[i];
        uint16_t on = (uint16_t)(1 << l[0] | 1 << l[1] | 1 << l[2]);
        int other[3], k = 0;
        for (int s = 0; s < 9 && k < 3; s++) {  /* the other player's three squares, off the line and not a line */
            if (on >> s & 1) continue;
            int line = 0;
            for (int j = 0; k == 2 && j < 8; j++) {
                uint16_t m = (uint16_t)(1 << tt_lines[j][0] | 1 << tt_lines[j][1] | 1 << tt_lines[j][2]);
                line |= m == (1 << other[0] | 1 << other[1] | 1 << s);
            }
            if (!line) other[k++] = s;
        }
        for (int who = 0; who < 2; who++) {     /* player 1's line: player 0 plays off it first */
            tt_new(&g, 0);
            int ok = !who || tt_play(&g, other[2]);
            for (int j = 0; j < 3; j++) ok &= tt_play(&g, l[j]) && (j == 2 || tt_play(&g, other[j]));
            CHECK(ok && tt_status(&g) == (who ? TT_WIN1 : TT_WIN0) && g.line == on);
            for (int s = 0; s < 9; s++) CHECK(!tt_play(&g, s));
        }
    }
    CHECK(play(&g, 0, "1 5 2 6 3") && tt_status(&g) == TT_WIN0 && g.line == 0x7);
    CHECK(play(&g, 0, "1 2 3 5 9 8") && tt_status(&g) == TT_WIN1 && g.line == (1 << 1 | 1 << 4 | 1 << 7));
    /* two lines at once: both light up */
    CHECK(play(&g, 0, "1 2 3 4 9 6 7 8 5") && tt_status(&g) == TT_WIN0);
    CHECK(g.line == (1 << 0 | 1 << 4 | 1 << 8 | 1 << 2 | 1 << 6));

    /* classic: a taken square, a square off the board; the full board without a line is a draw */
    CHECK(play(&g, 0, "5") && !tt_play(&g, 4) && !tt_play(&g, 9) && !tt_play(&g, -1) && tt_turn(&g) == 1);
    CHECK(play(&g, 0, "1 5 9 2 8 7 3 6 4") && tt_status(&g) == TT_DRAW && !g.line && !tt_play(&g, 0));
    CHECK(!play(&g, 0, "1 1") && !play(&g, 0, "0") && !play(&g, 0, "10"));

    /* Ultimate: the first mark anywhere; then the square sends the other player to the board in the same place */
    tt_new(&g, 1);
    CHECK(tt_target(&g) == -1 && tt_legal(&g, cell(1, 1)) && tt_legal(&g, cell(9, 9)));
    CHECK(tt_play(&g, cell(5, 1)) && tt_target(&g) == 0);
    CHECK(!tt_legal(&g, cell(2, 1)) && !tt_legal(&g, cell(5, 2)));
    CHECK(tt_legal(&g, cell(1, 1)) && tt_legal(&g, cell(1, 9)));
    CHECK(!tt_play(&g, cell(5, 5)) && tt_turn(&g) == 1 && tt_play(&g, cell(1, 5)) && tt_target(&g) == 4);
    CHECK(!play(&g, 1, "5.1 2.5") && !play(&g, 1, "5") && !play(&g, 1, "5.") && !play(&g, 1, "5.0"));

    /* a small board claimed: three in a row in board 5 (its top row); then nobody plays in it */
    CHECK(play(&g, 1, "5.1 1.5 5.2 2.5 5.3") && g.board[4] == 0 && tt_claimed(&g, 0) == 1 && tt_claimed(&g, 1) == 0);
    CHECK(tt_status(&g) == TT_PLAY && !g.line && tt_target(&g) == 2);
    /* sent to the claimed board 5: a free choice of the open boards, but not board 5 */
    CHECK(tt_play(&g, cell(3, 5)) && tt_target(&g) == -1);
    CHECK(!tt_legal(&g, cell(5, 5)) && !tt_legal(&g, cell(5, 9)));
    CHECK(tt_legal(&g, cell(9, 9)) && tt_legal(&g, cell(1, 1)));
    CHECK(tt_play(&g, cell(7, 3)) && tt_target(&g) == 2);

    /* a small board full without a line: nobody's, closed; a move sending there gives a free choice */
    CHECK(play(&g, 1, FULL) && g.board[3] == TT_FULL && tt_claimed(&g, 0) == 0 && tt_claimed(&g, 1) == 0);
    CHECK(tt_status(&g) == TT_PLAY && tt_target(&g) == 5);
    CHECK(tt_play(&g, cell(6, 9)) && tt_target(&g) == 8 && tt_play(&g, cell(9, 4)) && tt_target(&g) == -1);
    CHECK(!tt_legal(&g, cell(4, 1)) && tt_legal(&g, cell(2, 1)) && tt_legal(&g, cell(7, 1)));

    /* the big win: boards 1, 4 and 7 for player 0 (ebony has board 5) */
    CHECK(play(&g, 1, WIN) && tt_status(&g) == TT_WIN0 && g.line == (1 << 0 | 1 << 3 | 1 << 6));
    CHECK(g.board[0] == 0 && g.board[3] == 0 && g.board[6] == 0 && tt_claimed(&g, 0) == 3 && tt_claimed(&g, 1) == 1);
    for (int c = 0; c < TT_CELLS; c++) CHECK(!tt_legal(&g, c));

    /* the big draw: a board still open, but every line of boards has both players' or a full one */
    CHECK(play(&g, 1, DRAW) && tt_status(&g) == TT_DRAW && !g.line);
    int open = 0;
    for (int b = 0; b < 9; b++) open += g.board[b] == TT_OPEN;
    CHECK(open == 1);
    for (int c = 0; c < TT_CELLS; c++) CHECK(!tt_legal(&g, c));
    CHECK(tt_undo(&g) && tt_status(&g) == TT_PLAY);

    /* undo: the win taken back (the board reopens, the line goes), then every mark to the start */
    CHECK(play(&g, 1, WIN) && tt_undo(&g) && tt_status(&g) == TT_PLAY && !g.line && g.board[3] == TT_OPEN);
    CHECK(g.cell[cell(4, 1)] == -1 && tt_turn(&g) == 0 && tt_target(&g) == -1 && tt_play(&g, cell(4, 1)));
    CHECK(tt_status(&g) == TT_WIN0);
    CHECK(play(&g, 1, FULL) && tt_undo(&g) && g.board[3] == TT_OPEN && tt_target(&g) == 3);
    CHECK(play(&g, 1, "5.1 1.5 5.2 2.5 5.3 3.5") && tt_target(&g) == -1 && tt_undo(&g) && tt_target(&g) == 2);
    CHECK(tt_undo(&g) && g.board[4] == TT_OPEN && tt_target(&g) == 4);
    while (tt_undo(&g)) {}
    tt_game fresh;
    tt_new(&fresh, 1);
    CHECK(g.n == 0 && !tt_undo(&g) && tt_target(&g) == -1 && tt_turn(&g) == 0);
    CHECK(!memcmp(g.cell, fresh.cell, sizeof g.cell) && !memcmp(g.board, fresh.board, sizeof g.board));
    CHECK(play(&g, 0, "1 5 9 2 8 7 3 6 4") && tt_undo(&g) && tt_status(&g) == TT_PLAY && tt_play(&g, 3));

    printf("tictactoe: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
