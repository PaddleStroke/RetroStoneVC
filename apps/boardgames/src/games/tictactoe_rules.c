/*
 * Tic-tac-toe: the rules (tictactoe.h). No SDL: tested by tests/tictactoe_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/tictactoe.h"

const int8_t tt_lines[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};

void tt_new(tt_game *g, int ultimate)
{
    memset(g, 0, sizeof *g);
    memset(g->cell, -1, sizeof g->cell);
    memset(g->board, TT_OPEN, sizeof g->board);
    g->ultimate = ultimate;
    g->winner = -1;
}

int tt_cells(const tt_game *g) { return g->ultimate ? 81 : 9; }

int tt_turn(const tt_game *g) { return g->n & 1; }

/* the lines of nine squares (or boards) that `at` completes for its owner: their squares, or 0 */
static uint16_t lines_through(const int8_t *sq, int at)
{
    uint16_t all = 0;
    for (int i = 0; i < 8; i++) {
        const int8_t *l = tt_lines[i];
        if ((l[0] == at || l[1] == at || l[2] == at) && sq[l[0]] == sq[at] && sq[l[1]] == sq[at] && sq[l[2]] == sq[at])
            all |= (uint16_t)(1 << l[0] | 1 << l[1] | 1 << l[2]);
    }
    return all;
}

/* Ultimate: can anyone still make a line of claimed boards? (a line with both players' boards, or a full one, can't) */
static int line_left(const tt_game *g)
{
    for (int i = 0; i < 8; i++) {
        int has[3] = {0, 0, 0};
        for (int k = 0; k < 3; k++) {
            int b = g->board[tt_lines[i][k]];
            if (b != TT_OPEN) has[b]++;
        }
        if (!has[2] && (!has[0] || !has[1])) return 1;
    }
    return 0;
}

int tt_status(const tt_game *g)
{
    if (g->winner >= 0) return g->winner ? TT_WIN1 : TT_WIN0;
    if (g->ultimate) return line_left(g) ? TT_PLAY : TT_DRAW;
    return g->n == 9 ? TT_DRAW : TT_PLAY;
}

int tt_target(const tt_game *g)
{
    if (!g->ultimate || !g->n) return -1;
    int b = g->move[g->n - 1] % 9;
    return g->board[b] == TT_OPEN ? b : -1;
}

int tt_claimed(const tt_game *g, int player)
{
    int n = 0;
    for (int b = 0; b < 9; b++) n += g->board[b] == player;
    return n;
}

int tt_legal(const tt_game *g, int cell)
{
    if (cell < 0 || cell >= tt_cells(g) || g->cell[cell] >= 0 || tt_status(g) != TT_PLAY) return 0;
    if (!g->ultimate) return 1;
    int b = cell / 9, t = tt_target(g);
    return g->board[b] == TT_OPEN && (t < 0 || t == b);
}

int tt_play(tt_game *g, int cell)
{
    if (!tt_legal(g, cell)) return 0;
    int p = tt_turn(g);
    g->cell[cell] = (int8_t)p;
    g->move[g->n++] = (uint8_t)cell;
    if (!g->ultimate) {
        g->line = lines_through(g->cell, cell);
    } else {                                    /* the small board first: claimed, full, or still open */
        int b = cell / 9;
        if (lines_through(g->cell + 9 * b, cell % 9)) {
            g->board[b] = (int8_t)p;
            g->line = lines_through(g->board, b);
        } else {
            int empty = 0;
            for (int s = 0; s < 9; s++) empty += g->cell[9 * b + s] < 0;
            if (!empty) g->board[b] = TT_FULL;
        }
    }
    if (g->line) g->winner = p;
    return 1;
}

/* a mark can only be made in an open board, and the game stops at a win: undoing reopens both */
int tt_undo(tt_game *g)
{
    if (!g->n) return 0;
    int cell = g->move[--g->n];
    g->cell[cell] = -1;
    if (g->ultimate) g->board[cell / 9] = TT_OPEN;
    g->winner = -1;
    g->line = 0;
    return 1;
}

int tt_script(tt_game *g, const char *s)
{
    for (char *end; *s; s = end) {
        long a = strtol(s, &end, 10), b = 0;
        if (end == s) break;
        if (g->ultimate) {
            if (*end != '.') return 0;
            s = end + 1;
            b = strtol(s, &end, 10);
            if (end == s) return 0;
        }
        int cell = g->ultimate ? (int)(a - 1) * 9 + (int)(b - 1) : (int)a - 1;
        if (a < 1 || a > 9 || (g->ultimate && (b < 1 || b > 9)) || !tt_play(g, cell)) return 0;
    }
    return 1;
}
