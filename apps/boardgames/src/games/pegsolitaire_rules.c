/*
 * Peg Solitaire: the rules (pegsolitaire.h). No SDL: tested by tests/pegsolitaire_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/pegsolitaire.h"

#include <string.h>

static const int8_t DIRS[6][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}};   /* x, y */

static int ndirs(const psgame *g) { return g->variant == PS_TRIANGLE ? 6 : 4; }

static int on_board(int variant, int x, int y)
{
    if (x < 0 || y < 0 || x >= PS_SIDE || y >= PS_SIDE) return 0;
    if (variant == PS_TRIANGLE) return y < 5 && x <= y;
    int arm_x = x < 2 || x > 4, arm_y = y < 2 || y > 4;
    if (!(arm_x && arm_y)) return 1;            /* the cross */
    return variant == PS_FRENCH && (x == 1 || x == 5) && (y == 1 || y == 5);   /* its inner corners */
}

void ps_new(psgame *g, int variant)
{
    memset(g, 0, sizeof *g);
    g->variant = variant < 0 || variant >= PS_VARIANTS ? PS_ENGLISH : variant;
    g->rows = g->variant == PS_TRIANGLE ? 5 : 7;
    for (int c = 0; c < PS_CELLS; c++)
        if (on_board(g->variant, c % PS_SIDE, c / PS_SIDE)) {
            g->hole[c] = g->peg[c] = 1;
            g->holes++;
        }
    g->goal = g->variant == PS_ENGLISH ? 3 * PS_SIDE + 3 : g->variant == PS_FRENCH ? 1 * PS_SIDE + 3 : 0;
    g->peg[g->goal] = 0;
    g->pegs = g->holes - 1;
    if (g->variant == PS_FRENCH) g->goal = 5 * PS_SIDE + 3;   /* starts empty at d6, ends at d2 */
}

int ps_jump(const psgame *g, int from, int to, psmove *m)
{
    if (from < 0 || to < 0 || from >= PS_CELLS || to >= PS_CELLS) return 0;
    if (!g->peg[from] || !g->hole[to] || g->peg[to]) return 0;
    int dx = to % PS_SIDE - from % PS_SIDE, dy = to / PS_SIDE - from / PS_SIDE;
    for (int d = 0; d < ndirs(g); d++) {
        if (dx != 2 * DIRS[d][0] || dy != 2 * DIRS[d][1]) continue;
        int over = from + DIRS[d][1] * PS_SIDE + DIRS[d][0];
        if (!g->peg[over]) return 0;
        if (m) *m = (psmove){(int8_t)from, (int8_t)over, (int8_t)to};
        return 1;
    }
    return 0;
}

int ps_moves_from(const psgame *g, int from, psmove *out)
{
    int n = 0;
    if (from < 0 || from >= PS_CELLS || !g->peg[from]) return 0;
    int x = from % PS_SIDE, y = from / PS_SIDE;
    for (int d = 0; d < ndirs(g); d++) {
        int tx = x + 2 * DIRS[d][0], ty = y + 2 * DIRS[d][1];
        if (!on_board(g->variant, tx, ty)) continue;
        if (ps_jump(g, from, ty * PS_SIDE + tx, out ? &out[n] : NULL)) n++;
    }
    return n;
}

int ps_moves(const psgame *g, psmove *out)
{
    int n = 0;
    for (int c = 0; c < PS_CELLS; c++) n += ps_moves_from(g, c, out ? out + n : NULL);
    return n;
}

int ps_play(psgame *g, int from, int to)
{
    psmove m;
    if (!ps_jump(g, from, to, &m) || g->n >= PS_CELLS) return 0;
    g->peg[m.from] = g->peg[m.over] = 0;
    g->peg[m.to] = 1;
    g->pegs--;
    g->hist[g->n++] = m;
    return 1;
}

int ps_undo(psgame *g)
{
    if (!g->n) return 0;
    psmove m = g->hist[--g->n];
    g->peg[m.from] = g->peg[m.over] = 1;
    g->peg[m.to] = 0;
    g->pegs++;
    return 1;
}

int ps_status(const psgame *g)
{
    if (ps_moves(g, NULL)) return PS_PLAY;
    return g->pegs == 1 ? PS_ONE : PS_STUCK;
}

int ps_last(const psgame *g)
{
    if (g->pegs != 1) return -1;
    for (int c = 0; c < PS_CELLS; c++)
        if (g->peg[c]) return c;
    return -1;
}

int ps_gold(const psgame *g) { return ps_last(g) == g->goal; }

int ps_cell(const psgame *g, const char *s)
{
    if (s[0] < 'a' || s[0] > 'g' || s[1] < '1' || s[1] > '0' + g->rows) return -1;
    int x = s[0] - 'a', y = g->rows - (s[1] - '0'), c = y * PS_SIDE + x;
    return g->hole[c] ? c : -1;
}

void ps_name(const psgame *g, int cell, char out[3])
{
    out[0] = (char)('a' + cell % PS_SIDE);
    out[1] = (char)('0' + g->rows - cell / PS_SIDE);
    out[2] = 0;
}

int ps_script(psgame *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (strlen(s) < 4) return 0;
        int a = ps_cell(g, s), b = ps_cell(g, s + 2);
        if (a < 0 || b < 0 || !ps_play(g, a, b)) return 0;
        s += 4;
    }
    return 1;
}
