/*
 * Y's bot (bots.h), every board (9, 13, 19): Monte Carlo tree search with RAVE (games/rave.h), as hex's. A playout
 * fills the board at random (a full Y board always has exactly one winner: no need to stop at the win) and answers an
 * intrusion into one of its bridges (two stones with two common empty neighbours: when the other takes one, it takes
 * the other); the full board's winner by a union-find of its groups and the sides each touches. RAVE credits every
 * cell a player filled in a playout to the tree's move on that cell.
 * The swap (the second move): the bot as ebony swaps an opening off the edge (easy: one time in two); as ivory it
 * opens on an edge, away from the corners, a move worth too little to swap.
 * Easy: a few hundred playouts; medium a few thousand; hard as many as its budget allows. Y has no draw.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("c4", "swap"), played by ygame_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/rave.h"
#include "games/ygame.h"

/* the six neighbours in turn around a cell (each next to the one before): left, up-left, up-right, right,
 * down-right, down-left */
static const int RC[6] = {-1, -1, 0, 1, 1, 0}, RR[6] = {0, -1, -1, 0, 1, 1};

typedef struct {
    int16_t nb[YG_CELLS][6];                    /* the neighbours in turn round each cell, -1 off the board */
    uint8_t sides[YG_CELLS];
} ygt;

typedef struct {
    const ygt *t;                               /* the board's tables (the search's caller keeps them) */
    uint8_t cell[YG_CELLS];                     /* 0 empty, 1 ivory, 2 ebony */
    int n, count, side, winner, last;
} ygs;

static void tables(ygt *t, int n)
{
    for (int i = 0, count = yg_count(n); i < count; i++) {
        int r = yg_row(i), c = yg_col(i);
        for (int k = 0; k < 6; k++) {
            int x = c + RC[k], y = r + RR[k];
            t->nb[i][k] = (int16_t)(y >= 0 && y < n && x >= 0 && x <= y ? yg_index(x, y) : -1);
        }
        t->sides[i] = (uint8_t)yg_sides(n, i);
    }
}

/* does player p's group through cell x touch the three sides? */
static int joins(const ygs *s, int x, int p)
{
    int stack[YG_CELLS], top = 0, sides = 0;
    uint8_t seen[YG_CELLS] = {0};
    stack[top++] = x;
    seen[x] = 1;
    while (top) {
        int q = stack[--top];
        sides |= s->t->sides[q];
        if (sides == YG_ALL) return 1;
        for (int k = 0; k < 6; k++) {
            int y = s->t->nb[q][k];
            if (y >= 0 && !seen[y] && s->cell[y] == p + 1) { seen[y] = 1; stack[top++] = y; }
        }
    }
    return 0;
}

/* ---- the search's game (rave.h) ---------------------------------------------------------------------------------- */
static int r_expand(const void *st, int *mv, float *prior, float *weight)
{
    const ygs *s = st;
    int k = 0;
    if (s->winner >= 0) return 0;
    for (int i = 0; i < s->count; i++)
        if (!s->cell[i]) {
            int edge = s->t->sides[i] != 0;
            mv[k] = i;
            prior[k] = edge ? 0.4f : 0.5f;      /* the edge: a little less, at first */
            weight[k] = edge ? 3 : 0;
            k++;
        }
    return k;
}

static void r_make(void *st, int m)
{
    ygs *s = st;
    s->cell[m] = (uint8_t)(s->side + 1);
    s->last = m;
    if (joins(s, m, s->side)) s->winner = s->side;
    s->side ^= 1;
}

static int r_side(const void *st) { return ((const ygs *)st)->side; }

static int r_over(const void *st, int *winner)
{
    const ygs *s = st;
    if (s->winner < 0) return 0;
    *winner = s->winner;
    return 1;
}

/* the bridge answer: the other just took x, one of the two common neighbours of two stones of `me`; the other one */
static int save_bridge(const ygs *s, int x, int me)
{
    const int16_t *nb = s->t->nb[x];
    for (int k = 0; k < 6; k++) {
        int a = nb[k], m = nb[(k + 1) % 6], b = nb[(k + 2) % 6];
        if (a >= 0 && b >= 0 && m >= 0 && s->cell[a] == me + 1 && s->cell[b] == me + 1 && !s->cell[m]) return m;
    }
    return -1;
}

static int root_of(int16_t *up, int x)
{
    while (up[x] != x) x = up[x] = up[up[x]];
    return x;
}

/* the full board's winner: ivory's groups (a union-find, each root the sides its group touches); a Y, or ebony's */
static int full_winner(const ygs *s)
{
    int16_t up[YG_CELLS];
    uint8_t sides[YG_CELLS];
    for (int i = 0; i < s->count; i++) {
        up[i] = (int16_t)i;
        sides[i] = s->t->sides[i];
        if (s->cell[i] != 1) continue;
        for (int k = 0; k < 3; k++) {           /* left, up-left, up-right: the neighbours already seen */
            int y = s->t->nb[i][k];
            if (y < 0 || s->cell[y] != 1) continue;
            int a = root_of(up, i), b = root_of(up, y);
            if (a != b) { up[a] = (int16_t)b; sides[b] |= sides[a]; }
        }
        if (sides[root_of(up, i)] == YG_ALL) return 0;
    }
    return 1;
}

static int r_playout(void *st, ai_rng *rg, int *mv, uint8_t *pl, int cap, int *len)
{
    ygs *s = st;
    int empty[YG_CELLS], k = 0;
    for (int i = 0; i < s->count; i++)
        if (!s->cell[i]) empty[k++] = i;
    for (int i = k - 1; i > 0; i--) {           /* the order the cells fill in */
        int j = ai_rand_n(rg, i + 1), t = empty[i];
        empty[i] = empty[j];
        empty[j] = t;
    }
    int next = 0, last = s->last;
    *len = 0;
    for (int filled = 0; filled < k; filled++) {
        int m = last >= 0 ? save_bridge(s, last, s->side) : -1;
        if (m < 0) {
            while (s->cell[empty[next]]) next++;
            m = empty[next];
        }
        s->cell[m] = (uint8_t)(s->side + 1);
        if (*len < cap) { mv[*len] = m; pl[*len] = (uint8_t)s->side; (*len)++; }
        s->side ^= 1;
        last = m;
    }
    return full_winner(s);
}

static const rave_game RG = {
    .size = sizeof(ygs), .moves_max = YG_CELLS, .expand = r_expand, .make = r_make, .side = r_side,
    .over = r_over, .playout = r_playout, .rave_k = 600, .explore = 0, .expand_at = 2,
};

/* the move for the position (its player to move), at a level: a cell, YG_SWAP, or YG_NONE */
int ygb_choose(const ygame *g, int level, uint64_t seed, ai_stats *stats)
{
    if (g->winner >= 0) return YG_NONE;
    int n = g->n, count = yg_count(n);
    ygt t;
    tables(&t, n);
    ygs s = {.t = &t, .n = n, .count = count, .side = yg_turn(g), .winner = -1, .last = yg_last(g)};
    memcpy(s.cell, g->cell, sizeof s.cell);
    uint64_t key = (uint64_t)g->nmoves;
    for (int i = 0; i < count; i++) key = ai_mix(key, s.cell[i]);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, key));
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    if (stats) memset(stats, 0, sizeof *stats);
    if (g->nmoves == 0 && level != AI_EASY) {   /* the opening: on the bottom side, off the corners */
        int c = 2 + ai_rand_n(&rng, n - 4);
        return yg_index(c, n - 1);
    }
    if (yg_can_swap(g)) {
        int inner = !t.sides[g->move[0]];
        if (inner && (level != AI_EASY || ai_rand_n(&rng, 2))) return YG_SWAP;
    }
    /* hard: about 0.3 s (the small board's playouts are cheap, its tree is not) */
    long iters = level == AI_EASY ? 300 : level == AI_MEDIUM ? 3000 : count > 70 ? 1400000 / count : 20000;
    float rate;
    int m = rave_search(&RG, &s, iters, ai_level_budget(level).ms * 85 / 100, &rng, &rate, NULL, stats);
    return m < 0 ? YG_NONE : m;
}

static int think(int seat, int level, char *move, int cap)
{
    ygame g = *ygame_live();                    /* the copy, at entry: nothing else is read */
    if (g.winner >= 0 || yg_turn(&g) != seat || cap < 5) return 0;
    int m = ygb_choose(&g, level, ai_session_seed(), NULL);
    if (m == YG_NONE || ai_cancelled()) return 0;
    yg_name(m, move, cap);
    return 1;
}

static const bot ygame_bot = {.think = think, .play = ygame_show};
REGISTER_BOT(ygame, &ygame_bot)
