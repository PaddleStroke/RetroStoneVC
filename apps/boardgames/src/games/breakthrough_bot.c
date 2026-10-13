/*
 * Breakthrough's bot (bots.h): alpha-beta (ai.h) over the rules' position, with quiescence over the captures and the
 * steps onto the far row. The evaluation: the pawns (most of it), how far each has come (more and more near the far
 * row), the home row kept as a wall, pawns guarded from behind, and the races: a pawn one step from the far row wins
 * if it is the side to move's, and loses the game if it is the other's and cannot be taken at once.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("a2-a3", "a2xb3"), played by breakthrough_show().
 */
#include <stdio.h>

#include "bots.h"
#include "games/breakthrough.h"
#include "games/capture_bots.h"

#define RUNNER 20000                            /* a pawn through next move: a near win (eval scale) */

static int row_of(int s, int player) { return player ? 7 - s / 8 : s / 8; }   /* 0 home .. 7 the far row */

/* a pawn of `player` on s that steps onto the far row now (its row 6, a square ahead it may go to) */
static int runs_home(const btpos *p, int s, int player)
{
    int to = s + bt_forward(player), f = s & 7;
    for (int df = -1; df <= 1; df++) {
        if (f + df < 0 || f + df > 7) continue;
        int there = p->at[to + df];
        if (there != player && (df || there < 0)) return 1;
    }
    return 0;
}

/* a pawn of `victim` on s that the other player can take (a pawn of theirs diagonally behind it, from its side) */
static int attacked(const btpos *p, int s, int victim)
{
    int from = s + bt_forward(victim), f = s & 7, foe = !victim;
    if (from < 0 || from >= BT_SQUARES) return 0;
    return (f > 0 && p->at[from - 1] == foe) || (f < 7 && p->at[from + 1] == foe);
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    btmove l[BT_MAX_MOVES];
    int n = bt_legal(st, l);
    for (int i = 0; i < n; i++) out[i] = l[i].from | l[i].to << 6;
    return n;
}

static void a_make(void *st, int m)
{
    btpos *p = st;
    int from = m & 63, to = m >> 6;
    bt_make(p, (btmove){(int8_t)from, (int8_t)to, (int8_t)(p->at[to] >= 0)});
}

static int a_side(const void *st) { return ((const btpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const btpos *p = st;
    int w = bt_winner(p);
    if (w < 0) return 0;
    *score = w == p->side ? ai_win(0) : -ai_win(0);
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const btpos *p = st;
    return cpb_hash(p->at, sizeof p->at, p->side);
}

static const int ADVANCE[8] = {0, 0, 4, 10, 18, 30, 48, 0};

/* one player's pawns, from their side: the pawns, how far they came, the home wall, the guarded ones */
static int army(const btpos *p, int pl)
{
    int v = 0;
    for (int s = 0; s < BT_SQUARES; s++) {
        if (p->at[s] != pl) continue;
        int r = row_of(s, pl), f = s & 7;
        v += 100 + ADVANCE[r];
        if (r == 0) v += f == 1 || f == 2 || f == 5 || f == 6 ? 14 : 8;   /* the wall: the squares it guards most */
        int back = s - bt_forward(pl);
        if (back >= 0 && back < BT_SQUARES &&
            ((f > 0 && p->at[back - 1] == pl) || (f < 7 && p->at[back + 1] == pl)))
            v += 4 + r;                         /* guarded: taking it costs a pawn */
        if (r >= 4 && attacked(p, s, pl)) v -= 2 * r;
    }
    return v;
}

/* the side to move's view; 100 a pawn */
static int a_eval(const void *st)
{
    const btpos *p = st;
    int me = p->side, foe = !me, threats = 0, stoppable = 0;
    for (int s = 0; s < BT_SQUARES; s++) {
        if (p->at[s] == me && row_of(s, me) == 6 && runs_home(p, s, me)) return RUNNER;   /* through next move */
        if (p->at[s] == foe && row_of(s, foe) == 6) {   /* one step from my home row: take it now or lose */
            threats++;
            stoppable += attacked(p, s, foe);
        }
    }
    if (threats > 1 || (threats == 1 && !stoppable)) return -RUNNER;
    return army(p, me) - army(p, foe);
}

/* the far row first, then captures (the more advanced the pawn taken, the better), then the furthest steps */
static int a_order(const void *st, int m)
{
    const btpos *p = st;
    int from = m & 63, to = m >> 6, me = p->side, r = row_of(to, me);
    if (r == 7) return 1000;
    if (p->at[to] >= 0) return 500 + 20 * row_of(to, !me) - (attacked(p, to, me) ? 10 : 0);
    (void)from;
    return r;
}

/* quiescence: the captures and the steps onto the far row */
static int a_noisy(const void *st, int *out)
{
    const btpos *p = st;
    btmove l[BT_MAX_MOVES];
    int n = bt_legal(p, l), k = 0;
    for (int i = 0; i < n; i++)
        if (l[i].capture || row_of(l[i].to, p->side) == 7) out[k++] = l[i].from | l[i].to << 6;
    return k;
}

static const ai_game ADAPTER = {
    .size = sizeof(btpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};

static const cpb_levels LEVELS = {
    .easy_depth = 2, .easy_nodes = 3000, .easy_noise = 120,
    .medium_depth = 4, .medium_nodes = 60000, .medium_noise = 25,
    .hard_nodes = 2000000, .tt_bits = 20,
};

/* the move for the position p (its side to move), at a level: from | to << 6; -1: none */
int btb_choose(const btpos *p, int level, uint64_t seed, ai_stats *stats)
{
    btpos q = *p;
    int m;
    return cpb_turn(&ADAPTER, &q, &LEVELS, level, seed, &m, 1, stats) ? m : -1;
}

static int think(int seat, int level, char *move, int cap)
{
    btpos p = *bt_pos(breakthrough_live());     /* the copy, at entry: nothing else is read */
    if (bt_winner(&p) >= 0 || p.side != seat || cap < 6) return 0;
    int m = btb_choose(&p, level, ai_session_seed(), NULL);
    if (m < 0 || ai_cancelled()) return 0;
    int from = m & 63, to = m >> 6;
    snprintf(move, (size_t)cap, "%c%c%c%c%c", 'a' + from % 8, '1' + from / 8, p.at[to] >= 0 ? 'x' : '-',
             'a' + to % 8, '1' + to / 8);
    return 1;
}

static const bot breakthrough_bot = {.think = think, .play = breakthrough_show};
REGISTER_BOT(breakthrough, &breakthrough_bot)
