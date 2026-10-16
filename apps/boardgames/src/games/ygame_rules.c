/*
 * Y's rules for RetroStone Board (ygame.h): the stones, the swap, the groups and the sides they touch.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The win: a stone set down joins the groups round it into one; the player wins when that group touches the three
 * sides (a search through it from the new stone). Only the player who moved can have made a Y.
 */
#include <stdio.h>
#include <string.h>

#include "games/ygame.h"

static const int DC[6] = {-1, 1, -1, 0, 0, 1}, DR[6] = {0, 0, -1, -1, 1, 1};

void yg_new(ygame *g, int n)
{
    memset(g, 0, sizeof *g);
    g->n = n == 9 || n == 19 ? n : 13;
    g->winner = -1;
}

int yg_count(int n) { return n * (n + 1) / 2; }
int yg_index(int c, int r) { return r * (r + 1) / 2 + c; }

int yg_row(int cell)
{
    int r = 0;
    while (yg_index(0, r + 1) <= cell) r++;
    return r;
}

int yg_col(int cell) { return cell - yg_index(0, yg_row(cell)); }

int yg_turn(const ygame *g) { return g->nmoves & 1; }

int yg_last(const ygame *g)
{
    if (!g->nmoves) return -1;
    int m = g->move[g->nmoves - 1];
    return m == YG_SWAP ? g->move[0] : m;
}

int yg_around(int n, int cell, int nb[6])
{
    int r = yg_row(cell), c = cell - yg_index(0, r), k = 0;
    for (int i = 0; i < 6; i++) {
        int x = c + DC[i], y = r + DR[i];
        if (y >= 0 && y < n && x >= 0 && x <= y) nb[k++] = yg_index(x, y);
    }
    return k;
}

int yg_sides(int n, int cell)
{
    int r = yg_row(cell), c = cell - yg_index(0, r);
    return (c == 0 ? YG_LEFT : 0) | (c == r ? YG_RIGHT : 0) | (r == n - 1 ? YG_BOTTOM : 0);
}

int yg_group(const ygame *g, int cell, int *cells, int *count)
{
    int n = g->n, own = g->cell[cell], stack[YG_CELLS], top = 0, k = 0, sides = 0, nb[6];
    uint8_t seen[YG_CELLS] = {0};
    if (own == YG_EMPTY) {
        if (count) *count = 0;
        return 0;
    }
    stack[top++] = cell;
    seen[cell] = 1;
    while (top) {
        int q = stack[--top];
        if (cells) cells[k] = q;
        k++;
        sides |= yg_sides(n, q);
        for (int m = yg_around(n, q, nb), j = 0; j < m; j++)
            if (!seen[nb[j]] && g->cell[nb[j]] == own) {
                seen[nb[j]] = 1;
                stack[top++] = nb[j];
            }
    }
    if (count) *count = k;
    return sides;
}

/* within one group (`in`), each stone's distance (in stones) from a side and the way back to it */
static void spread(const ygame *g, const uint8_t *in, int side, int *dist, int *from)
{
    int n = g->n, cells = yg_count(n), queue[YG_CELLS], head = 0, tail = 0, nb[6];
    for (int i = 0; i < cells; i++) {
        dist[i] = -1;
        if (in[i] && yg_sides(n, i) & side) {
            dist[i] = 1;
            from[i] = -1;
            queue[tail++] = i;
        }
    }
    while (head < tail) {
        int p = queue[head++];
        for (int m = yg_around(n, p, nb), j = 0; j < m; j++)
            if (in[nb[j]] && dist[nb[j]] < 0) {
                dist[nb[j]] = dist[p] + 1;
                from[nb[j]] = p;
                queue[tail++] = nb[j];
            }
    }
}

/* the winner's Y: from the stone nearest all three sides (the fewest stones in all), its three ways to them */
int yg_chain(const ygame *g, int *cells)
{
    if (g->winner < 0) return 0;
    int n = g->n, count = yg_count(n), group[YG_CELLS], k, root = -1;
    for (int i = 0; i < count && root < 0; i++)
        if (g->cell[i] == g->winner + 1 && yg_group(g, i, NULL, NULL) == YG_ALL) root = i;
    if (root < 0) return 0;
    uint8_t in[YG_CELLS] = {0}, on[YG_CELLS] = {0};
    yg_group(g, root, group, &k);
    for (int i = 0; i < k; i++) in[group[i]] = 1;
    static const int SIDE[3] = {YG_LEFT, YG_RIGHT, YG_BOTTOM};
    int dist[3][YG_CELLS], from[3][YG_CELLS], best = -1, top = 0;
    for (int s = 0; s < 3; s++) spread(g, in, SIDE[s], dist[s], from[s]);
    for (int i = 0; i < count; i++)
        if (in[i]) {
            int d = dist[0][i] + dist[1][i] + dist[2][i];
            if (best < 0 || d < top) best = i, top = d;
        }
    int m = 0;
    for (int s = 0; s < 3; s++)
        for (int q = best; q >= 0; q = from[s][q])
            if (!on[q]) {
                on[q] = 1;
                if (cells) cells[m] = q;
                m++;
            }
    return m;
}

int yg_legal(const ygame *g, int cell)
{
    return g->winner < 0 && cell >= 0 && cell < yg_count(g->n) && g->cell[cell] == YG_EMPTY;
}

int yg_can_swap(const ygame *g) { return g->nmoves == 1 && g->winner < 0; }

int yg_play(ygame *g, int cell)
{
    if (!yg_legal(g, cell)) return 0;
    int p = yg_turn(g);
    g->cell[cell] = (uint8_t)(p + 1);
    g->move[g->nmoves++] = cell;
    if (yg_group(g, cell, NULL, NULL) == YG_ALL) g->winner = p;
    return 1;
}

int yg_swap(ygame *g)
{
    if (!yg_can_swap(g)) return 0;
    g->cell[g->move[0]] = YG_EBONY;
    g->move[g->nmoves++] = YG_SWAP;
    return 1;
}

int yg_undo(ygame *g)
{
    if (!g->nmoves) return 0;
    int m = g->move[--g->nmoves];
    if (m == YG_SWAP) g->cell[g->move[0]] = YG_IVORY;
    else g->cell[m] = YG_EMPTY;
    g->winner = -1;                             /* the game stopped at the winning move: before it, nobody had won */
    return 1;
}

int yg_cell(int n, const char *s, int *len)
{
    if (!strncmp(s, "swap", 4)) {
        *len = 4;
        return YG_SWAP;
    }
    int c = s[0] - 'a', r = 0, k = 1;
    while (k < 4 && s[k] >= '0' && s[k] <= '9') r = r * 10 + s[k++] - '0';
    *len = k;
    if (k == 1 || r < 1 || r > n || c < 0 || c >= r) return YG_NONE;
    return yg_index(c, r - 1);
}

int yg_name(int cell, char *s, int cap)
{
    if (cell == YG_SWAP) return snprintf(s, (size_t)cap, "swap");
    return snprintf(s, (size_t)cap, "%c%d", 'a' + yg_col(cell), yg_row(cell) + 1);
}

int yg_script(ygame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len, m = yg_cell(g->n, s, &len);
        if (m == YG_NONE || (m == YG_SWAP ? !yg_swap(g) : !yg_play(g, m))) return 0;
        s += len;
    }
    return 1;
}
