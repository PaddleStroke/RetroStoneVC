/*
 * Chinese checkers' rules for RetroStone Board (no SDL): the star, the steps and the chains of jumps, the win with the
 * anti-blocking rule (halma.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/halma.h"

static const int COUNT[HL_ROWS] = {1, 2, 3, 4, 13, 12, 11, 10, 9, 10, 11, 12, 13, 4, 3, 2, 1};
static const int DX[6] = {2, 1, -1, -2, -1, 1}, DY[6] = {0, 1, 1, 0, -1, -1};   /* round the hole */

static int8_t grid[HL_ROWS][25];                /* [row][x2 + 12]: the hole, or -1 */
static int8_t row_of[HL_HOLES], x2_of[HL_HOLES];
static int ready;

static void init(void)
{
    if (ready) return;
    ready = 1;
    memset(grid, -1, sizeof grid);
    int h = 0;
    for (int r = 0; r < HL_ROWS; r++)
        for (int i = 0; i < COUNT[r]; i++, h++) {
            row_of[h] = (int8_t)r;
            x2_of[h] = (int8_t)(2 * i - (COUNT[r] - 1));
            grid[r][x2_of[h] + 12] = (int8_t)h;
        }
}

/* ---- the board ------------------------------------------------------------------------------------------------- */
int hl_row(int h) { init(); return row_of[h]; }
int hl_x2(int h) { init(); return x2_of[h]; }

int hl_at(int x2, int row)
{
    init();
    if (row < 0 || row >= HL_ROWS || x2 < -12 || x2 > 12) return -1;
    return grid[row][x2 + 12];
}

static int toward(int h, int d) { return hl_at(x2_of[h] + DX[d], row_of[h] + DY[d]); }

int hl_around(int h, int nb[6])
{
    int k = 0;
    for (int d = 0; d < 6; d++) {
        int n = toward(h, d);
        if (n >= 0) nb[k++] = n;
    }
    return k;
}

int hl_home(int h) { init(); return row_of[h] <= 3 ? 0 : row_of[h] >= 13 ? 1 : -1; }

int hl_goal(int player, int h) { return hl_home(h) == !player; }

/* the three axes of the hexagonal grid (a, b, c, adding up to 0): the middle hexagon is where none is beyond 4; a
 * point is where one of them is (the bottom point: the row's, c <= -5) */
int hl_point(int h)
{
    init();
    int c = row_of[h] - 8, a = (x2_of[h] - c) / 2, b = -a - c;
    return c <= -5 ? 0 : b >= 5 ? 1 : a <= -5 ? 2 : c >= 5 ? 3 : b <= -5 ? 4 : a >= 5 ? 5 : -1;
}

/* ---- the players ----------------------------------------------------------------------------------------------- */
static const int8_t SEAT[HL_PLAYERS + 1][HL_PLAYERS] = {
    [2] = {0, 3}, [3] = {0, 2, 4}, [4] = {0, 1, 3, 4}, [6] = {0, 1, 2, 3, 4, 5},
};

static int valid_count(int players) { return players >= 2 && players <= HL_PLAYERS && players != 5; }

int hl_seat(int players, int player)
{
    if (!valid_count(players)) players = 2;
    return player >= 0 && player < players ? SEAT[players][player] : -1;
}

int hl_dest(const hl_game *g, int player, int h)
{
    int k = hl_seat(g->players, player);
    return k >= 0 && hl_point(h) == (k + 3) % HL_POINTS;
}

/* ---- moves ----------------------------------------------------------------------------------------------------- */
void hl_new_players(hl_game *g, int players)
{
    init();
    memset(g, 0, sizeof *g);
    g->players = valid_count(players) ? players : 2;
    for (int p = 0; p < g->players; p++)
        for (int h = 0; h < HL_HOLES; h++)
            if (hl_point(h) == SEAT[g->players][p]) g->cell[h] = (uint8_t)(p + 1);
}

void hl_new(hl_game *g) { hl_new_players(g, 2); }

int hl_turn(const hl_game *g) { return g->turn; }

/* the holes the marble on `from` reaches by chains of jumps (the marble lifted: its own hole is empty, and visited):
 * prev[h] the hole it jumps from to land on h (a shortest chain), -1 unreached; from itself: from */
static void jumps(const hl_game *g, int from, int16_t *prev)
{
    int queue[HL_HOLES], head = 0, tail = 0;
    for (int h = 0; h < HL_HOLES; h++) prev[h] = -1;
    prev[from] = (int16_t)from;
    queue[tail++] = from;
    while (head < tail) {
        int a = queue[head++];
        for (int d = 0; d < 6; d++) {
            int over = toward(a, d), b = over < 0 ? -1 : toward(over, d);
            if (b < 0 || over == from || !g->cell[over] || g->cell[b] || prev[b] >= 0) continue;
            prev[b] = (int16_t)a;
            queue[tail++] = b;
        }
    }
}

static int is_step(const hl_game *g, int from, int to)
{
    for (int d = 0; d < 6; d++)
        if (toward(from, d) == to) return !g->cell[to];
    return 0;
}

static int movable(const hl_game *g, int from)
{
    return from >= 0 && from < HL_HOLES && g->cell[from] == hl_turn(g) + 1;
}

int hl_targets(const hl_game *g, int from, int *to)
{
    init();
    if (!movable(g, from)) return 0;
    int16_t prev[HL_HOLES];
    int k = 0;
    jumps(g, from, prev);
    for (int h = 0; h < HL_HOLES; h++)
        if (h != from && (prev[h] >= 0 || is_step(g, from, h))) to[k++] = h;
    return k;
}

int hl_route(const hl_game *g, int from, int to, hl_path *p)
{
    init();
    if (!movable(g, from) || to < 0 || to >= HL_HOLES || to == from) return 0;
    if (is_step(g, from, to)) {
        p->n = 2;
        p->hole[0] = (uint8_t)from;
        p->hole[1] = (uint8_t)to;
        return 1;
    }
    int16_t prev[HL_HOLES];
    jumps(g, from, prev);
    if (prev[to] < 0) return 0;
    int n = 1;
    for (int h = to; h != from; h = prev[h]) n++;
    p->n = n;
    for (int h = to, i = n - 1; i >= 0; h = prev[h], i--) p->hole[i] = (uint8_t)h;
    return 1;
}

/* the hole jumped over going from a to b (two holes apart in a line), or -1 */
static int jumped(int a, int b)
{
    for (int d = 0; d < 6; d++) {
        int over = toward(a, d);
        if (over >= 0 && toward(over, d) == b) return over;
    }
    return -1;
}

int hl_valid(const hl_game *g, const hl_path *p)
{
    init();
    if (p->n < 2 || p->n > HL_HOLES || !movable(g, p->hole[0])) return 0;
    int from = p->hole[0];
    for (int i = 1; i < p->n; i++)
        if (p->hole[i] >= HL_HOLES) return 0;
    if (p->n == 2 && is_step(g, from, p->hole[1])) return 1;
    uint8_t seen[HL_HOLES] = {0};
    seen[from] = 1;
    for (int i = 1; i < p->n; i++) {
        int a = p->hole[i - 1], b = p->hole[i], over = jumped(a, b);
        if (over < 0 || over == from || !g->cell[over] || seen[b] || (g->cell[b] && b != from)) return 0;
        seen[b] = 1;
    }
    return 1;
}

static int can_move(const hl_game *g, int player);

/* who moves after the player: the next round the star; three or more: the next who can move (the others pass; no one:
 * the next, and the game is drawn). Two: the other, always (no move: drawn) */
static int next_player(const hl_game *g, int player)
{
    int n = g->players;
    for (int k = 1; n > 2 && k <= n; k++)
        if (can_move(g, (player + k) % n)) return (player + k) % n;
    return (player + 1) % n;
}

int hl_play(hl_game *g, const hl_path *p)
{
    if (hl_status(g) != HL_PLAY || !hl_valid(g, p)) return 0;
    int from = p->hole[0], to = p->hole[p->n - 1];
    g->cell[to] = g->cell[from];
    g->cell[from] = HL_EMPTY;
    if (g->nhist == HL_HIST) {                  /* full: forget the oldest move */
        memmove(g->hist, g->hist + 1, sizeof g->hist[0] * (HL_HIST - 1));
        memmove(g->who, g->who + 1, HL_HIST - 1);
        g->nhist--;
    }
    g->who[g->nhist] = (uint8_t)g->turn;
    g->hist[g->nhist++] = *p;
    g->ply++;
    g->turn = next_player(g, g->turn);
    return 1;
}

int hl_undo(hl_game *g)
{
    if (!g->nhist) return 0;
    const hl_path *p = &g->hist[--g->nhist];
    int from = p->hole[0], to = p->hole[p->n - 1];
    g->cell[from] = g->cell[to];
    g->cell[to] = HL_EMPTY;
    g->ply--;
    g->turn = g->who[g->nhist];
    return 1;
}

const hl_path *hl_last(const hl_game *g) { return g->nhist ? &g->hist[g->nhist - 1] : NULL; }

int hl_arrived(const hl_game *g, int player)
{
    int n = 0;
    for (int h = 0; h < HL_HOLES; h++) n += hl_dest(g, player, h) && g->cell[h] == player + 1;
    return n;
}

/* the point the player must fill is full, and holds one of their marbles at least */
static int filled(const hl_game *g, int player)
{
    for (int h = 0; h < HL_HOLES; h++)
        if (hl_dest(g, player, h) && !g->cell[h]) return 0;
    return hl_arrived(g, player) > 0;
}

/* can the player move at all? */
static int can_move(const hl_game *g, int player)
{
    for (int h = 0; h < HL_HOLES; h++) {
        if (g->cell[h] != player + 1) continue;
        for (int d = 0; d < 6; d++) {
            int a = toward(h, d), b = a < 0 ? -1 : toward(a, d);
            if (a >= 0 && (!g->cell[a] || (b >= 0 && !g->cell[b]))) return 1;
        }
    }
    return 0;
}

int hl_status(const hl_game *g)
{
    init();
    int n = g->players, last = g->nhist ? g->who[g->nhist - 1] : (g->turn + n - 1) % n;
    for (int k = 0; k < n; k++)                 /* the player who just moved first, then round the star */
        if (filled(g, (last + k) % n)) return hl_win((last + k) % n);
    return can_move(g, g->turn) ? HL_PLAY : HL_DRAW;   /* the one to move can move, if anyone can */
}

/* ---- the notation ---------------------------------------------------------------------------------------------- */
int hl_hole(const char *s, int *len)
{
    init();
    if (s[0] < 'a' || s[0] > 'm' || s[1] < '1' || s[1] > '9') return -1;
    int r = s[1] - '0', k = 2;
    if (s[2] >= '0' && s[2] <= '9') r = r * 10 + (s[2] - '0'), k = 3;
    int i = s[0] - 'a';
    if (r < 1 || r > HL_ROWS || i >= COUNT[r - 1]) return -1;
    *len = k;
    return hl_at(2 * i - (COUNT[r - 1] - 1), r - 1);
}

void hl_name(int h, char out[4])
{
    init();
    int r = row_of[h];
    out[0] = (char)('a' + (x2_of[h] + COUNT[r] - 1) / 2);
    if (r + 1 >= 10) {
        out[1] = '1';
        out[2] = (char)('0' + (r + 1) % 10);
        out[3] = 0;
    } else {
        out[1] = (char)('1' + r);
        out[2] = 0;
    }
}

int hl_script(hl_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        hl_path p = {0};
        for (;;) {
            int len, h = hl_hole(s, &len);
            if (h < 0 || p.n >= HL_HOLES) return 0;
            p.hole[p.n++] = (uint8_t)h;
            s += len;
            if (*s != '-') break;
            s++;
        }
        if (*s && *s != ' ') return 0;
        if (p.n == 2 && !hl_valid(g, &p) && !hl_route(g, p.hole[0], p.hole[1], &p)) return 0;
        if (!hl_play(g, &p)) return 0;
    }
    return 1;
}
