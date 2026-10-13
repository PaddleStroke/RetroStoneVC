/*
 * Hex's bot (bots.h), every board (7 x 7, 9 x 9, 11 x 11): Monte Carlo tree search with RAVE (games/rave.h). A
 * playout fills the board at random (a full hex board always has exactly one winner: no need to stop at the win)
 * and answers an intrusion into one of its bridges (two stones with two common empty neighbours: when the other takes
 * one, it takes the other); RAVE credits every cell a player filled in a playout to the tree's move on that cell,
 * which suits hex well (a cell's worth hardly depends on when it was taken).
 * The swap (the second move): the bot as ebony swaps an opening away from the board's edge (easy: one time in two);
 * as ivory it opens on its own edge, away from the corners, a move worth too little to swap.
 * Easy: a few hundred playouts; medium a few thousand; hard as many as its budget allows. Hex has no draw.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("c4", "swap"), played by hex_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/hex.h"
#include "games/rave.h"

/* the six neighbours in turn around a cell (each next to the one before): left, up-left, up-right, right,
 * down-right, down-left */
static const int RC[6] = {-1, 0, 1, 1, 0, -1}, RR[6] = {0, -1, -1, 0, 1, 1};

typedef struct {
    uint8_t cell[HEX_CELLS];                    /* 0 empty, 1 ivory, 2 ebony */
    int n, side, winner, last;
} hxs;

static int at(const hxs *s, int c, int r) { return c >= 0 && r >= 0 && c < s->n && r < s->n ? c + r * s->n : -1; }

/* does player p's group through cell x join its two sides? */
static int joins(const hxs *s, int x, int p)
{
    int n = s->n, stack[HEX_CELLS], top = 0, first = 0, last = 0;
    uint8_t seen[HEX_CELLS] = {0};
    stack[top++] = x;
    seen[x] = 1;
    while (top) {
        int q = stack[--top], c = q % n, r = q / n;
        if (p ? c == 0 : r == 0) first = 1;
        if (p ? c == n - 1 : r == n - 1) last = 1;
        if (first && last) return 1;
        for (int k = 0; k < 6; k++) {
            int y = at(s, c + RC[k], r + RR[k]);
            if (y >= 0 && !seen[y] && s->cell[y] == p + 1) { seen[y] = 1; stack[top++] = y; }
        }
    }
    return 0;
}

/* ---- the search's game (rave.h) ---------------------------------------------------------------------------------- */
static int r_expand(const void *st, int *mv, float *prior, float *weight)
{
    const hxs *s = st;
    int k = 0, n = s->n;
    if (s->winner >= 0) return 0;
    for (int i = 0; i < n * n; i++)
        if (!s->cell[i]) {
            int c = i % n, r = i / n, edge = c == 0 || r == 0 || c == n - 1 || r == n - 1;
            mv[k] = i;
            prior[k] = edge ? 0.4f : 0.5f;      /* the edge: a little less, at first */
            weight[k] = edge ? 3 : 0;
            k++;
        }
    return k;
}

static void r_make(void *st, int m)
{
    hxs *s = st;
    s->cell[m] = (uint8_t)(s->side + 1);
    s->last = m;
    if (joins(s, m, s->side)) s->winner = s->side;
    s->side ^= 1;
}

static int r_side(const void *st) { return ((const hxs *)st)->side; }

static int r_over(const void *st, int *winner)
{
    const hxs *s = st;
    if (s->winner < 0) return 0;
    *winner = s->winner;
    return 1;
}

/* the bridge answer: the other just took x, one of the two common neighbours of two stones of `me`; the other one */
static int save_bridge(const hxs *s, int x, int me)
{
    int n = s->n, c = x % n, r = x / n, nb[6];
    for (int k = 0; k < 6; k++) nb[k] = at(s, c + RC[k], r + RR[k]);
    for (int k = 0; k < 6; k++) {
        int a = nb[k], m = nb[(k + 1) % 6], b = nb[(k + 2) % 6];
        if (a >= 0 && b >= 0 && m >= 0 && s->cell[a] == me + 1 && s->cell[b] == me + 1 && !s->cell[m]) return m;
    }
    return -1;
}

static int r_playout(void *st, ai_rng *rg, int *mv, uint8_t *pl, int cap, int *len)
{
    hxs *s = st;
    int n = s->n, empty[HEX_CELLS], k = 0;
    for (int i = 0; i < n * n; i++)
        if (!s->cell[i]) empty[k++] = i;
    for (int i = k - 1; i > 0; i--) {           /* the order the cells fill in */
        int j = ai_rand_n(rg, i + 1), t = empty[i];
        empty[i] = empty[j];
        empty[j] = t;
    }
    int next = 0, last = s->last, filled = 0;
    *len = 0;
    while (filled < k) {
        int m = last >= 0 ? save_bridge(s, last, s->side) : -1;
        if (m < 0) {
            while (s->cell[empty[next]]) next++;
            m = empty[next];
        }
        s->cell[m] = (uint8_t)(s->side + 1);
        if (*len < cap) { mv[*len] = m; pl[*len] = (uint8_t)s->side; (*len)++; }
        s->side ^= 1;
        last = m;
        filled++;
    }
    /* a full board: ivory joined top and bottom, or ebony left and right */
    int stack[HEX_CELLS], top = 0;
    uint8_t seen[HEX_CELLS] = {0};
    for (int c = 0; c < n; c++)
        if (s->cell[c] == 1) { seen[c] = 1; stack[top++] = c; }
    while (top) {
        int q = stack[--top], c = q % n, r = q / n;
        if (r == n - 1) return 0;
        for (int d = 0; d < 6; d++) {
            int y = at(s, c + RC[d], r + RR[d]);
            if (y >= 0 && !seen[y] && s->cell[y] == 1) { seen[y] = 1; stack[top++] = y; }
        }
    }
    return 1;
}

static const rave_game RG = {
    .size = sizeof(hxs), .moves_max = HEX_CELLS, .expand = r_expand, .make = r_make, .side = r_side,
    .over = r_over, .playout = r_playout, .rave_k = 600, .explore = 0, .expand_at = 2,
};

/* the move for the position (its player to move), at a level: a cell, HEX_SWAP, or HEX_NONE */
int hxb_choose(const hexgame *g, int level, uint64_t seed, ai_stats *stats)
{
    if (g->winner >= 0) return HEX_NONE;
    int n = g->n;
    hxs s = {.n = n, .side = hex_turn(g), .winner = -1, .last = hex_last(g)};
    for (int i = 0; i < n * n; i++) s.cell[i] = g->cell[i];
    uint64_t key = (uint64_t)g->nmoves;
    for (int i = 0; i < n * n; i++) key = ai_mix(key, s.cell[i]);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, key));
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    if (stats) memset(stats, 0, sizeof *stats);
    if (g->nmoves == 0 && level != AI_EASY) {   /* the opening: on its own edge (top or bottom), off the corners */
        int c = 1 + ai_rand_n(&rng, n - 2), r = ai_rand_n(&rng, 2) ? 0 : n - 1;
        return c + r * n;
    }
    if (hex_can_swap(g)) {
        int f = g->move[0], c = f % n, r = f / n, inner = c > 0 && r > 0 && c < n - 1 && r < n - 1;
        if (inner && (level != AI_EASY || ai_rand_n(&rng, 2))) return HEX_SWAP;
    }
    long iters = level == AI_EASY ? 300 : level == AI_MEDIUM ? 3000 : 2000000 / (n * n);   /* hard: about 0.3 s */
    float rate;
    int m = rave_search(&RG, &s, iters, ai_level_budget(level).ms * 9 / 10, &rng, &rate, NULL, stats);
    return m < 0 ? HEX_NONE : m;
}

static int think(int seat, int level, char *move, int cap)
{
    hexgame g = *hex_live();                    /* the copy, at entry: nothing else is read */
    if (g.winner >= 0 || hex_turn(&g) != seat || cap < 5) return 0;
    int m = hxb_choose(&g, level, ai_session_seed(), NULL);
    if (m == HEX_NONE || ai_cancelled()) return 0;
    if (m == HEX_SWAP) snprintf(move, (size_t)cap, "swap");
    else snprintf(move, (size_t)cap, "%c%d", 'a' + m % g.n, m / g.n + 1);
    return 1;
}

static const bot hex_bot = {.think = think, .play = hex_show};
REGISTER_BOT(hex, &hex_bot)
