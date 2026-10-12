/*
 * Peg Solitaire: the rules (pegsolitaire.h): the boards, the jumps, undo, the starts (classic, levels, random), the
 * search and the hint. No SDL: tested by tests/pegsolitaire_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/pegsolitaire.h"

#include <stdlib.h>
#include <string.h>

#include "games/pegsolitaire_levels.h"
#include "games/puzrand.h"

#define HINT_BUDGET 400000L                     /* the positions a hint may search, each way */

static const int8_t DIRS[6][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}};   /* x, y */
static const int8_t SIZE[PS_BOARDS][2] = {{7, 7}, {7, 7}, {5, 5}, {9, 9}, {9, 9}, {6, 6}};   /* cols, rows */

static int ndirs(const psgame *g) { return g->tri ? 6 : 4; }

static int on_board(int board, int x, int y)
{
    if (x < 0 || y < 0 || x >= SIZE[board][0] || y >= SIZE[board][1]) return 0;
    switch (board) {
    case PS_TRIANGLE:
    case PS_TRIANGLE6: return x <= y;
    case PS_DIAMOND: return abs(x - 4) + abs(y - 4) <= 4;
    case PS_WIEGLEB: return (x >= 3 && x <= 5) || (y >= 3 && y <= 5);
    default: {
        int arm_x = x < 2 || x > 4, arm_y = y < 2 || y > 4;
        if (!(arm_x && arm_y)) return 1;        /* the cross */
        return board == PS_FRENCH && (x == 1 || x == 5) && (y == 1 || y == 5);   /* its inner corners */
    }
    }
}

void ps_board(psgame *g, int board)
{
    memset(g, 0, sizeof *g);
    g->board = board < 0 || board >= PS_BOARDS ? PS_ENGLISH : board;
    g->tri = g->board == PS_TRIANGLE || g->board == PS_TRIANGLE6;
    g->cols = SIZE[g->board][0];
    g->rows = SIZE[g->board][1];
    g->goal = -1;
    for (int c = 0; c < PS_CELLS; c++)
        if (on_board(g->board, c % PS_SIDE, c / PS_SIDE)) {
            g->hole[c] = 1;
            g->holes++;
        }
}

void ps_new(psgame *g, int board)
{
    /* the hole empty at the start, the goal: English the middle; French d6 and d2; the triangles their top; the
     * diamond and Wiegleb's board their middle */
    static const int8_t EMPTY[PS_BOARDS][2] = {{3, 3}, {3, 1}, {0, 0}, {4, 4}, {4, 4}, {0, 0}};
    static const int8_t GOAL[PS_BOARDS][2] = {{3, 3}, {3, 5}, {0, 0}, {4, 4}, {4, 4}, {0, 0}};
    ps_board(g, board);
    int b = g->board;
    memcpy(g->peg, g->hole, sizeof g->peg);
    g->peg[EMPTY[b][1] * PS_SIDE + EMPTY[b][0]] = 0;
    g->pegs = g->holes - 1;
    g->goal = GOAL[b][1] * PS_SIDE + GOAL[b][0];
    memcpy(g->start, g->peg, sizeof g->start);
}

int ps_index(const psgame *g, int cell)
{
    if (cell < 0 || cell >= PS_CELLS || !g->hole[cell]) return -1;
    int k = 0;
    for (int c = 0; c < cell; c++) k += g->hole[c];
    return k;
}

uint64_t ps_mask(const psgame *g)
{
    uint64_t m = 0;
    for (int c = 0, k = 0; c < PS_CELLS; c++)
        if (g->hole[c]) {
            if (g->peg[c]) m |= 1ull << k;
            k++;
        }
    return m;
}

int ps_set(psgame *g, int board, int goal, uint64_t pegs, const char *sol)
{
    ps_board(g, board);
    if (goal < 0 || goal >= PS_CELLS || !g->hole[goal]) return 0;
    g->goal = goal;
    for (int c = 0, k = 0; c < PS_CELLS; c++)
        if (g->hole[c]) {
            g->peg[c] = (uint8_t)(pegs >> k & 1);
            g->pegs += g->peg[c];
            k++;
        }
    if (!g->pegs) return 0;
    memcpy(g->start, g->peg, sizeof g->start);
    if (!sol) return 1;
    psgame t = *g;                              /* the solution played on a copy: it must end in the goal */
    if (!ps_script(&t, sol) || !ps_gold(&t)) return 0;
    memcpy(g->sol, t.hist, sizeof g->sol);
    g->nsol = t.n;
    return 1;
}

int ps_level(psgame *g, int level)
{
    if (level < 0 || level >= PS_LEVELS) level = 0;
    const ps_level_t *L = &ps_levels[level];
    return L->sol && ps_set(g, L->board, L->goal, L->pegs, L->sol);
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
        if (tx < 0 || ty < 0 || tx >= PS_SIDE || ty >= PS_SIDE) continue;
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
    if (!ps_jump(g, from, to, &m) || g->n >= PS_MAXH) return 0;
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
    if (g->pegs == 1) return PS_ONE;
    return ps_moves(g, NULL) ? PS_PLAY : PS_STUCK;
}

int ps_last(const psgame *g)
{
    if (g->pegs != 1) return -1;
    for (int c = 0; c < PS_CELLS; c++)
        if (g->peg[c]) return c;
    return -1;
}

int ps_gold(const psgame *g) { return g->goal >= 0 && ps_last(g) == g->goal; }

int ps_stars(const psgame *g)
{
    int s = 3 - !ps_gold(g) - (g->hints > 0);
    return s < 1 ? 1 : s;
}

/* ---- a random start: one marble in a goal, jumps played backwards ------------------------------------------------ */
void ps_random(psgame *g, int board, uint32_t seed, int back)
{
    uint32_t rs = pr_seed(seed);
    ps_board(g, board);
    int cells[PS_MAXH], k = 0;
    for (int c = 0; c < PS_CELLS; c++)
        if (g->hole[c]) cells[k++] = c;
    g->goal = cells[pr_below(&rs, k)];
    g->peg[g->goal] = 1;
    g->pegs = 1;
    if (back <= 0) back = g->holes / 4 + pr_below(&rs, g->holes / 6 + 1);
    psmove rev[PS_MAXH];
    int nrev = 0;
    while (nrev < back && nrev < PS_MAXH - 1) {
        psmove can[PS_MAX_MOVES];
        int nc = 0;
        for (int c = 0; c < PS_CELLS; c++) {    /* a marble on c, as if it had jumped in from a over b */
            if (!g->peg[c]) continue;
            for (int d = 0; d < ndirs(g); d++) {
                int x = c % PS_SIDE, y = c / PS_SIDE, bx = x - DIRS[d][0], by = y - DIRS[d][1];
                int ax = x - 2 * DIRS[d][0], ay = y - 2 * DIRS[d][1];
                if (ax < 0 || ay < 0 || ax >= PS_SIDE || ay >= PS_SIDE) continue;
                int a = ay * PS_SIDE + ax, b = by * PS_SIDE + bx;
                if (g->hole[a] && g->hole[b] && !g->peg[a] && !g->peg[b] && nc < PS_MAX_MOVES)
                    can[nc++] = (psmove){(int8_t)a, (int8_t)b, (int8_t)c};
            }
        }
        if (!nc) break;
        psmove m = can[pr_below(&rs, nc)];
        g->peg[m.to] = 0;
        g->peg[m.from] = g->peg[m.over] = 1;
        g->pegs++;
        rev[nrev++] = m;
    }
    memcpy(g->start, g->peg, sizeof g->start);
    for (int i = 0; i < nrev; i++) g->sol[i] = rev[nrev - 1 - i];
    g->nsol = nrev;
}

/* ---- the search: depth first, the positions known to lead nowhere kept in a table -------------------------------- */
static struct {
    int holes, cell[PS_MAXH], nj, target;
    struct { uint8_t a, b, c; } j[PS_MAXH * 6];
    uint64_t *dead;
    int bits;
    long used, steps, budget;
    psmove *path;
} S = {.bits = 18};

void ps_solver_size(int bits)
{
    if (bits < 10) bits = 10;
    if (bits > 28) bits = 28;
    free(S.dead);
    S.dead = NULL;
    S.bits = bits;
}

static uint64_t *slot(uint64_t st)
{
    uint64_t mask = (1ull << S.bits) - 1, i = (st * 0x9e3779b97f4a7c15ull) >> (64 - S.bits);
    while (S.dead[i] && S.dead[i] != st) i = (i + 1) & mask;
    return &S.dead[i];
}

static int dfs(uint64_t st, int pegs, int depth)
{
    if (pegs == 1) return S.target < 0 || st == 1ull << S.target;
    if (++S.steps > S.budget) return -1;
    if (*slot(st) == st) return 0;
    for (int i = 0; i < S.nj; i++) {
        uint64_t a = 1ull << S.j[i].a, b = 1ull << S.j[i].b, c = 1ull << S.j[i].c;
        if (!(st & a) || !(st & b) || (st & c)) continue;
        if (S.path) S.path[depth] = (psmove){(int8_t)S.cell[S.j[i].a], (int8_t)S.cell[S.j[i].b],
                                             (int8_t)S.cell[S.j[i].c]};
        int r = dfs(st ^ a ^ b ^ c, pegs - 1, depth + 1);
        if (r) return r;
    }
    if (S.used < (3l << S.bits) / 4) {          /* a dead end, noted (the table never fuller than three quarters) */
        uint64_t *s = slot(st);
        if (!*s) { *s = st; S.used++; }
    }
    return 0;
}

int ps_solve(const psgame *g, int target, psmove *path, long budget)
{
    if (g->pegs < 1) return 0;
    if (!S.dead) S.dead = malloc(sizeof(uint64_t) << S.bits);
    if (!S.dead) return -1;
    memset(S.dead, 0, sizeof(uint64_t) << S.bits);
    S.used = 0;
    S.holes = 0;
    int idx[PS_CELLS];
    for (int c = 0; c < PS_CELLS; c++) {
        idx[c] = g->hole[c] ? S.holes : -1;
        if (g->hole[c]) S.cell[S.holes++] = c;
    }
    S.nj = 0;
    for (int c = 0; c < PS_CELLS; c++) {        /* every jump of the board, as holes' bits */
        if (!g->hole[c]) continue;
        for (int d = 0; d < ndirs(g); d++) {
            int x = c % PS_SIDE + DIRS[d][0], y = c / PS_SIDE + DIRS[d][1], x2 = x + DIRS[d][0], y2 = y + DIRS[d][1];
            if (x2 < 0 || y2 < 0 || x2 >= PS_SIDE || y2 >= PS_SIDE) continue;
            int b = y * PS_SIDE + x, t = y2 * PS_SIDE + x2;
            if (!g->hole[b] || !g->hole[t]) continue;
            S.j[S.nj].a = (uint8_t)idx[c];
            S.j[S.nj].b = (uint8_t)idx[b];
            S.j[S.nj++].c = (uint8_t)idx[t];
        }
    }
    S.target = target >= 0 && target < PS_CELLS ? idx[target] : -1;
    if (target >= 0 && S.target < 0) return 0;
    S.steps = 0;
    S.budget = budget;
    S.path = path;
    return dfs(ps_mask(g), g->pegs, 0);
}

/* ---- the hint -------------------------------------------------------------------------------------------------- */
/* 1: the game so far is the known way's beginning (the marbles as it leaves them) */
static int on_track(const psgame *g)
{
    if (g->n >= g->nsol) return 0;
    uint8_t p[PS_CELLS];
    memcpy(p, g->start, sizeof p);
    for (int i = 0; i < g->n; i++) {
        psmove m = g->sol[i];
        p[m.from] = p[m.over] = 0;
        p[m.to] = 1;
    }
    return !memcmp(p, g->peg, sizeof p);
}

int ps_hint(psgame *g, psmove *m)
{
    if (ps_status(g) != PS_PLAY) return 0;
    if (on_track(g)) {
        *m = g->sol[g->n];
        g->hints++;
        return 1;
    }
    static psmove path[PS_MAXH];
    int r = ps_solve(g, g->goal, path, HINT_BUDGET);
    if (r == 1) {                               /* a new known way: the game so far, then the one found */
        memcpy(g->sol, g->hist, sizeof(psmove) * (size_t)g->n);
        memcpy(g->sol + g->n, path, sizeof(psmove) * (size_t)(g->pegs - 1));
        g->nsol = g->n + g->pegs - 1;
        *m = path[0];
        g->hints++;
        return 1;
    }
    int r2 = ps_solve(g, -1, path, HINT_BUDGET);
    if (r2 == 1) {
        *m = path[0];
        g->hints++;
        return 2;
    }
    return r == 0 && r2 == 0 ? 0 : -1;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int ps_cell(const psgame *g, const char *s)
{
    if (s[0] < 'a' || s[0] >= 'a' + g->cols || s[1] < '1' || s[1] > '0' + g->rows) return -1;
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
