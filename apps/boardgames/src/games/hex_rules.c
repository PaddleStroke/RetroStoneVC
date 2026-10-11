/*
 * Hex's rules for RetroStone Board (hex.h): the stones, the swap, the chains from side to side.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/hex.h"

static const int DC[6] = {-1, 1, 0, 1, -1, 0}, DR[6] = {0, 0, -1, -1, 1, 1};

void hex_new(hexgame *g, int n)
{
    memset(g, 0, sizeof *g);
    g->n = n == 7 || n == 9 ? n : 11;
    g->winner = -1;
}

int hex_turn(const hexgame *g) { return g->nmoves & 1; }

int hex_mirror(int n, int cell) { return cell / n + cell % n * n; }

int hex_last(const hexgame *g)
{
    if (!g->nmoves) return -1;
    int m = g->move[g->nmoves - 1];
    return m == HEX_SWAP ? hex_mirror(g->n, g->move[0]) : m;
}

int hex_around(int n, int cell, int nb[6])
{
    int c = cell % n, r = cell / n, k = 0;
    for (int i = 0; i < 6; i++) {
        int x = c + DC[i], y = r + DR[i];
        if (x >= 0 && y >= 0 && x < n && y < n) nb[k++] = x + y * n;
    }
    return k;
}

/* is the cell on the player's first side (ivory: the top row, ebony: the left column), or on the last? */
static int first_side(int n, int player, int cell) { return player ? cell % n == 0 : cell / n == 0; }
static int last_side(int n, int player, int cell) { return player ? cell % n == n - 1 : cell / n == n - 1; }

/* the player's shortest chain joining the two sides (a search through their stones from the first side); its length */
static int chain(const hexgame *g, int player, int *cells)
{
    int n = g->n, from[HEX_CELLS], queue[HEX_CELLS], head = 0, tail = 0, nb[6];
    for (int i = 0; i < n * n; i++) {
        from[i] = -2;
        if (g->cell[i] == player + 1 && first_side(n, player, i)) {
            from[i] = -1;
            queue[tail++] = i;
        }
    }
    while (head < tail) {
        int p = queue[head++];
        if (last_side(n, player, p)) {
            int k = 0;
            for (int q = p; q >= 0; q = from[q]) {
                if (cells) cells[k] = q;
                k++;
            }
            return k;
        }
        for (int m = hex_around(n, p, nb), j = 0; j < m; j++)
            if (from[nb[j]] == -2 && g->cell[nb[j]] == player + 1) {
                from[nb[j]] = p;
                queue[tail++] = nb[j];
            }
    }
    return 0;
}

int hex_chain(const hexgame *g, int *cells) { return g->winner < 0 ? 0 : chain(g, g->winner, cells); }

int hex_legal(const hexgame *g, int cell)
{
    return g->winner < 0 && cell >= 0 && cell < g->n * g->n && g->cell[cell] == HEX_EMPTY;
}

int hex_can_swap(const hexgame *g) { return g->nmoves == 1 && g->winner < 0; }

int hex_play(hexgame *g, int cell)
{
    if (!hex_legal(g, cell)) return 0;
    int p = hex_turn(g);
    g->cell[cell] = (uint8_t)(p + 1);
    g->move[g->nmoves++] = cell;
    if (chain(g, p, NULL)) g->winner = p;      /* only the player who moved can have joined their sides */
    return 1;
}

int hex_swap(hexgame *g)
{
    if (!hex_can_swap(g)) return 0;
    int first = g->move[0];
    g->cell[first] = HEX_EMPTY;
    g->cell[hex_mirror(g->n, first)] = HEX_EBONY;
    g->move[g->nmoves++] = HEX_SWAP;
    return 1;
}

int hex_undo(hexgame *g)
{
    if (!g->nmoves) return 0;
    int m = g->move[--g->nmoves];
    if (m == HEX_SWAP) {
        g->cell[hex_mirror(g->n, g->move[0])] = HEX_EMPTY;
        g->cell[g->move[0]] = HEX_IVORY;
    } else {
        g->cell[m] = HEX_EMPTY;
    }
    g->winner = -1;                             /* the game stopped at the winning move: before it, nobody had won */
    return 1;
}

int hex_cell(int n, const char *s, int *len)
{
    if (!strncmp(s, "swap", 4)) {
        *len = 4;
        return HEX_SWAP;
    }
    int c = s[0] - 'a', r = 0, k = 1;
    while (k < 4 && s[k] >= '0' && s[k] <= '9') r = r * 10 + s[k++] - '0';
    *len = k;
    if (c < 0 || c >= n || k == 1 || r < 1 || r > n) return HEX_NONE;
    return c + (r - 1) * n;
}

int hex_script(hexgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len, m = hex_cell(g->n, s, &len);
        if (m == HEX_NONE || (m == HEX_SWAP ? !hex_swap(g) : !hex_play(g, m))) return 0;
        s += len;
    }
    return 1;
}
