/*
 * Ring and Fork's rules for RetroStone Board (havannah.h): the stones, the swap, the three shapes, the winner's shape.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The ring: flooded from the rim, through every cell that is not the player's, the cells the flood cannot reach are
 * inside a loop of the player's stones; so is a stone of the player's whose six neighbours are all the player's (the
 * loop round it). A rim cell always floods, so it is never inside. The bridge and the fork: the group through the
 * newest stone, the corners and the sides it touches. Only the player who moved can have made a shape.
 */
#include <stdio.h>
#include <string.h>

#include "games/havannah.h"

const int HV_DX[6] = {1, 1, 0, -1, -1, 0}, HV_DY[6] = {0, -1, -1, 0, 1, 1};

void hv_new(hvgame *g, int n, int swap)
{
    memset(g, 0, sizeof *g);
    g->n = n == 6 ? 6 : 8;
    g->swap = swap != 0;
    g->winner = -1;
}

int hv_count(int n) { return 3 * n * (n - 1) + 1; }

int hv_on(int n, int x, int y)
{
    int m = 2 * n - 2;
    return x >= 0 && y >= 0 && x <= m && y <= m && x + y >= n - 1 && x + y <= 3 * (n - 1);
}

int hv_valid(int n, int cell) { return cell >= 0 && cell < HV_SQ && hv_on(n, cell % HV_W, cell / HV_W); }

/* the cube coordinates from the middle */
static void cube(int n, int cell, int *q, int *r, int *s)
{
    *q = cell % HV_W - (n - 1);
    *r = cell / HV_W - (n - 1);
    *s = -*q - *r;
}

/* the sides a cell is on, as bits (0 top r = -c, 1 q = c, 2 s = -c, 3 bottom r = c, 4 q = -c, 5 s = c) */
static int rims(int n, int cell)
{
    int q, r, s, c = n - 1;
    cube(n, cell, &q, &r, &s);
    return (r == -c) | (q == c) << 1 | (s == -c) << 2 | (r == c) << 3 | (q == -c) << 4 | (s == c) << 5;
}

int hv_corner(int n, int cell)
{
    static const int AT[6] = {1 | 32, 1 | 2, 2 | 4, 4 | 8, 8 | 16, 16 | 32};   /* the two sides meeting there */
    int b = rims(n, cell);
    for (int k = 0; k < 6; k++)
        if (b == AT[k]) return k;
    return -1;
}

int hv_side(int n, int cell)
{
    int b = rims(n, cell);
    for (int k = 0; k < 6; k++)
        if (b == 1 << k) return k;
    return -1;
}

int hv_around(int n, int cell, int nb[6])
{
    int x = cell % HV_W, y = cell / HV_W, k = 0;
    for (int i = 0; i < 6; i++) {
        int a = x + HV_DX[i], b = y + HV_DY[i];
        nb[i] = hv_on(n, a, b) ? b * HV_W + a : -1;
        k += nb[i] >= 0;
    }
    return k;
}

int hv_turn(const hvgame *g) { return g->nmoves & 1; }

int hv_last(const hvgame *g)
{
    if (!g->nmoves) return -1;
    int m = g->move[g->nmoves - 1];
    return m == HV_SWAP ? g->move[0] : m;
}

int hv_ring(int n, const uint8_t *cell, int p, uint8_t *inside)
{
    uint8_t seen[HV_SQ] = {0};
    int queue[HV_SQ], head = 0, tail = 0, nb[6], found = 0, own = p + 1;
    for (int i = 0; i < HV_SQ; i++)             /* the rim's cells that are not the player's */
        if (hv_valid(n, i) && cell[i] != own && hv_around(n, i, nb) < 6) seen[i] = 1, queue[tail++] = i;
    while (head < tail) {
        int c = queue[head++];
        hv_around(n, c, nb);
        for (int k = 0; k < 6; k++)
            if (nb[k] >= 0 && !seen[nb[k]] && cell[nb[k]] != own) seen[nb[k]] = 1, queue[tail++] = nb[k];
    }
    if (inside) memset(inside, 0, HV_SQ);
    for (int i = 0; i < HV_SQ; i++) {
        if (!hv_valid(n, i) || seen[i]) continue;
        int in = cell[i] != own;
        if (!in) {                              /* a stone of the player's: inside when wholly surrounded by them */
            int k = hv_around(n, i, nb), all = k == 6;
            for (int j = 0; j < 6 && all; j++) all = cell[nb[j]] == own;
            in = all;
        }
        if (in) {
            found = 1;
            if (!inside) return 1;
            inside[i] = 1;
        }
    }
    return found;
}

/* the group through a stone: its cells (count), the corners (bits) and sides (bits) it touches */
static int group(int n, const uint8_t *cell, int at, int *cells, int *corners, int *sides)
{
    int own = cell[at], stack[HV_SQ], top = 0, k = 0, nb[6];
    uint8_t seen[HV_SQ] = {0};
    *corners = *sides = 0;
    stack[top++] = at;
    seen[at] = 1;
    while (top) {
        int c = stack[--top], cr = hv_corner(n, c), sd = hv_side(n, c);
        if (cells) cells[k] = c;
        k++;
        if (cr >= 0) *corners |= 1 << cr;
        if (sd >= 0) *sides |= 1 << sd;
        hv_around(n, c, nb);
        for (int j = 0; j < 6; j++)
            if (nb[j] >= 0 && !seen[nb[j]] && cell[nb[j]] == own) seen[nb[j]] = 1, stack[top++] = nb[j];
    }
    return k;
}

static int bits(int v)
{
    int k = 0;
    for (; v; v &= v - 1) k++;
    return k;
}

int hv_shapes(int n, const uint8_t *cell, int p, int at)
{
    int how = hv_ring(n, cell, p, NULL) ? HV_RING : 0, corners, sides;
    uint8_t done[HV_SQ] = {0};
    for (int i = at >= 0 ? at : 0; i < HV_SQ; i++) {
        if (hv_valid(n, i) && cell[i] == p + 1 && !done[i]) {
            int cells[HV_SQ], k = group(n, cell, i, cells, &corners, &sides);
            for (int j = 0; j < k; j++) done[cells[j]] = 1;
            if (bits(corners) >= 2) how |= HV_BRIDGE;
            if (bits(sides) >= 3) how |= HV_FORK;
        }
        if (at >= 0) break;
    }
    return how;
}

int hv_legal(const hvgame *g, int cell)
{
    return g->winner < 0 && hv_valid(g->n, cell) && g->cell[cell] == HV_EMPTY;
}

int hv_can_swap(const hvgame *g) { return g->swap && g->nmoves == 1 && g->winner < 0; }

static void judge(hvgame *g, int p, int at)
{
    int how = hv_shapes(g->n, g->cell, p, at);
    if (how) { g->winner = p; g->how = how; return; }
    if (g->nmoves >= hv_count(g->n) + (g->move[1] == HV_SWAP && g->nmoves > 1)) g->winner = HV_DRAW;
}

int hv_play(hvgame *g, int cell)
{
    if (!hv_legal(g, cell)) return 0;
    int p = hv_turn(g);
    g->cell[cell] = (uint8_t)(p + 1);
    g->move[g->nmoves++] = (int16_t)cell;
    judge(g, p, cell);
    return 1;
}

int hv_swap(hvgame *g)
{
    if (!hv_can_swap(g)) return 0;
    g->cell[g->move[0]] = HV_EBONY;
    g->move[g->nmoves++] = HV_SWAP;
    return 1;
}

int hv_undo(hvgame *g)
{
    if (!g->nmoves) return 0;
    int m = g->move[--g->nmoves];
    if (m == HV_SWAP) g->cell[g->move[0]] = HV_IVORY;
    else g->cell[m] = HV_EMPTY;
    g->winner = -1;                             /* the game stopped at its last move: nobody had won before it */
    g->how = 0;
    return 1;
}

/* ---- the winner's shape ------------------------------------------------------------------------------------------ */
/* within the stones `in`, each one's distance from the sources (src) and the way back */
static void spread(int n, const uint8_t *in, const uint8_t *src, int *dist, int *from)
{
    int queue[HV_SQ], head = 0, tail = 0, nb[6];
    for (int i = 0; i < HV_SQ; i++) {
        dist[i] = -1;
        if (in[i] && src[i]) dist[i] = 0, from[i] = -1, queue[tail++] = i;
    }
    while (head < tail) {
        int c = queue[head++];
        hv_around(n, c, nb);
        for (int j = 0; j < 6; j++)
            if (nb[j] >= 0 && in[nb[j]] && dist[nb[j]] < 0) {
                dist[nb[j]] = dist[c] + 1;
                from[nb[j]] = c;
                queue[tail++] = nb[j];
            }
    }
}

static void walk(const int *from, int c, uint8_t *on)
{
    for (; c >= 0; c = from[c]) on[c] = 1;
}

int hv_shape_cells(const hvgame *g, int *cells)
{
    if (g->winner < 0 || g->winner == HV_DRAW) return 0;
    int n = g->n, own = g->winner + 1, nb[6], last = hv_last(g);
    uint8_t on[HV_SQ] = {0}, in[HV_SQ] = {0}, src[HV_SQ];
    int dist[HV_SQ], from[HV_SQ];
    if (g->how & HV_RING) {                     /* the loop: the stones next to the inside and to the outside */
        uint8_t inside[HV_SQ], outer[HV_SQ] = {0};
        hv_ring(n, g->cell, g->winner, inside);
        uint8_t seen[HV_SQ] = {0};
        int queue[HV_SQ], head = 0, tail = 0;
        for (int i = 0; i < HV_SQ; i++)
            if (hv_valid(n, i) && g->cell[i] != own && hv_around(n, i, nb) < 6) seen[i] = 1, queue[tail++] = i;
        while (head < tail) {
            int c = queue[head++];
            hv_around(n, c, nb);
            for (int k = 0; k < 6; k++)
                if (nb[k] >= 0 && !seen[nb[k]] && g->cell[nb[k]] != own) seen[nb[k]] = 1, queue[tail++] = nb[k];
        }
        for (int i = 0; i < HV_SQ; i++) {
            if (!hv_valid(n, i) || g->cell[i] != own || inside[i]) continue;
            int k = hv_around(n, i, nb), by_in = 0;
            for (int j = 0; j < 6; j++)
                if (nb[j] >= 0) by_in |= inside[nb[j]];
            if (!by_in) continue;
            int by_out = k < 6;
            for (int j = 0; j < 6; j++)
                if (nb[j] >= 0) by_out |= seen[nb[j]];
            outer[i] = 1 + by_out;              /* 2: on the loop's outside too */
        }
        int best = 0;
        for (int i = 0; i < HV_SQ; i++) best = outer[i] > best ? outer[i] : best;
        for (int i = 0; i < HV_SQ; i++)
            if (outer[i] == best && best) on[i] = 1;
    }
    if ((g->how & (HV_BRIDGE | HV_FORK)) && last >= 0 && g->cell[last] == own) {
        int gc[HV_SQ], corners, sides, k = group(n, g->cell, last, gc, &corners, &sides);
        for (int i = 0; i < k; i++) in[gc[i]] = 1;
        if (g->how & HV_BRIDGE) {               /* the shortest way between two corners */
            int bl = -1, bb = 0, bf[HV_SQ];
            for (int a = 0; a < HV_SQ; a++) {
                if (!in[a] || hv_corner(n, a) < 0) continue;
                memset(src, 0, sizeof src);
                src[a] = 1;
                spread(n, in, src, dist, from);
                for (int b = 0; b < HV_SQ; b++)
                    if (b != a && in[b] && hv_corner(n, b) >= 0 && dist[b] >= 0 && (bl < 0 || dist[b] < bl)) {
                        bl = dist[b], bb = b;
                        memcpy(bf, from, sizeof bf);
                    }
            }
            if (bl >= 0) walk(bf, bb, on);
        }
        if (g->how & HV_FORK) {                 /* from the stone nearest three sides, its ways to them */
            int sd[6][HV_SQ], sf[6][HV_SQ];
            for (int e = 0; e < 6; e++) {
                for (int i = 0; i < HV_SQ; i++) src[i] = in[i] && hv_side(n, i) == e;
                spread(n, in, src, sd[e], sf[e]);
            }
            int best = -1, root = -1, tri[3] = {0, 0, 0};
            for (int a = 0; a < 6; a++)
                for (int b = a + 1; b < 6; b++)
                    for (int c = b + 1; c < 6; c++)
                        for (int i = 0; i < HV_SQ; i++) {
                            if (!in[i] || sd[a][i] < 0 || sd[b][i] < 0 || sd[c][i] < 0) continue;
                            int d = sd[a][i] + sd[b][i] + sd[c][i];
                            if (best < 0 || d < best) best = d, root = i, tri[0] = a, tri[1] = b, tri[2] = c;
                        }
            if (root >= 0)
                for (int t = 0; t < 3; t++) walk(sf[tri[t]], root, on);
        }
    }
    int m = 0;
    for (int i = 0; i < HV_SQ; i++)
        if (on[i]) {
            if (cells) cells[m] = i;
            m++;
        }
    return m;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int hv_cell(int n, const char *s, int *len)
{
    if (!strncmp(s, "swap", 4)) {
        *len = 4;
        return HV_SWAP;
    }
    int x = s[0] - 'a', y = 0, k = 1;
    while (k < 4 && s[k] >= '0' && s[k] <= '9') y = y * 10 + s[k++] - '0';
    *len = k;
    if (k == 1 || y < 1 || !hv_on(n, x, y - 1)) return HV_NONE;
    return (y - 1) * HV_W + x;
}

int hv_name(int cell, char *s, int cap)
{
    if (cell == HV_SWAP) return snprintf(s, (size_t)cap, "swap");
    return snprintf(s, (size_t)cap, "%c%d", 'a' + cell % HV_W, cell / HV_W + 1);
}

int hv_script(hvgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len, m = hv_cell(g->n, s, &len);
        if (m == HV_NONE || (m == HV_SWAP ? !hv_swap(g) : !hv_play(g, m))) return 0;
        s += len;
    }
    return 1;
}
