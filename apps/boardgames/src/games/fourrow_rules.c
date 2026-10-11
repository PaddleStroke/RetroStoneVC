/*
 * Four in a row: the rules (fourrow.h). No SDL: tested by tests/fourrow_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/fourrow.h"

void fr_new(fr_game *g)
{
    memset(g, 0, sizeof *g);
    memset(g->hole, -1, sizeof g->hole);
    g->winner = -1;
}

int fr_turn(const fr_game *g) { return g->n & 1; }

int fr_status(const fr_game *g)
{
    if (g->winner >= 0) return g->winner ? FR_WIN1 : FR_WIN0;
    return g->n == FR_HOLES ? FR_DRAW : FR_PLAY;
}

int fr_left(const fr_game *g, int player) { return FR_HOLES / 2 - (g->n + !player) / 2; }

static int owner(const fr_game *g, int c, int r)
{
    return c < 0 || c >= FR_COLS || r < 0 || r >= FR_ROWS ? -1 : g->hole[r][c];
}

/* the lines of four or more through the disc just dropped at (c, r): their discs, or 0 */
static uint64_t lines_through(const fr_game *g, int c, int r)
{
    static const int dir[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    int p = g->hole[r][c];
    uint64_t all = 0;
    for (int d = 0; d < 4; d++) {
        int dx = dir[d][0], dy = dir[d][1], n = 1;
        uint64_t m = FR_BIT(c, r);
        for (int k = 1; owner(g, c + k * dx, r + k * dy) == p; k++, n++) m |= FR_BIT(c + k * dx, r + k * dy);
        for (int k = 1; owner(g, c - k * dx, r - k * dy) == p; k++, n++) m |= FR_BIT(c - k * dx, r - k * dy);
        if (n >= 4) all |= m;
    }
    return all;
}

int fr_play(fr_game *g, int col)
{
    if (col < 0 || col >= FR_COLS || fr_status(g) != FR_PLAY || g->height[col] >= FR_ROWS) return -1;
    int r = g->height[col]++, p = fr_turn(g);
    g->hole[r][col] = (int8_t)p;
    g->move[g->n++] = (uint8_t)col;
    g->line = lines_through(g, col, r);
    if (g->line) g->winner = p;
    return r;
}

/* the game stops at a win, so the position before the last disc had no winner */
int fr_undo(fr_game *g)
{
    if (!g->n) return 0;
    int col = g->move[--g->n];
    g->hole[--g->height[col]][col] = -1;
    g->winner = -1;
    g->line = 0;
    return 1;
}
