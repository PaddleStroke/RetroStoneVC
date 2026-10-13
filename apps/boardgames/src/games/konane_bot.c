/*
 * Konane's bot (bots.h): alpha-beta (ai.h) over the rules' position. Konane is a game of moves left: the player who
 * runs out of jumps first loses, so the evaluation is the mobility (the side to move's jumps against the other's,
 * the other's weighing more: they move next), and the stones that can still jump at all.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("e4" a removal of the opening, "c4-e4" or "c4-g4" a jump), played by
 * konane_show().
 */
#include <stdio.h>

#include "bots.h"
#include "games/capture_bots.h"
#include "games/konane.h"

/* side's jumps (as if it were to move: every length of each), and the stones that have one */
static int mobility(const knpos *p, int side, int *stones)
{
    int n = p->n, jumps = 0;
    *stones = 0;
    for (int s = 0; s < n * n; s++) {
        if (p->at[s] != side) continue;
        int x = s % n, y = s / n, any = 0;
        for (int k = 0; k < 4; k++) {
            int dx = k == 1 ? 1 : k == 3 ? -1 : 0, dy = k == 0 ? 1 : k == 2 ? -1 : 0;
            for (int d = 2;; d += 2) {
                int ox = x + dx * (d - 1), oy = y + dy * (d - 1), tx = x + dx * d, ty = y + dy * d;
                if (tx < 0 || ty < 0 || tx >= n || ty >= n || p->at[ox + n * oy] != !side || p->at[tx + n * ty] >= 0)
                    break;
                jumps++;
                any = 1;
            }
        }
        *stones += any;
    }
    return jumps;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    knmove l[KN_MAX_MOVES];
    int n = kn_legal(st, l);
    for (int i = 0; i < n; i++) out[i] = l[i].from | l[i].to << 7;
    return n;
}

static void a_make(void *st, int m) { kn_make(st, (knmove){(int8_t)(m & 127), (int8_t)(m >> 7)}); }
static int a_side(const void *st) { return ((const knpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const knpos *p = st;
    int stones;
    if (p->opening < 2 || mobility(p, p->side, &stones)) return 0;
    *score = -ai_win(0);                        /* no move: lost */
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const knpos *p = st;
    return cpb_hash(p->at, (size_t)p->n * p->n, p->side | p->opening << 1);
}

/* the side to move's view; 100 a jump to spare */
static int a_eval(const void *st)
{
    const knpos *p = st;
    if (p->opening < 2) return 0;
    int sm, so, mm = mobility(p, p->side, &sm), mo = mobility(p, !p->side, &so);
    return 100 * mm - 130 * mo + 40 * (sm - so);
}

static int a_order(const void *st, int m)       /* the longer jumps first */
{
    int n = ((const knpos *)st)->n, a = m & 127, b = m >> 7, d = a > b ? a - b : b - a;
    return a / n == b / n ? d : d / n;
}

static const ai_game ADAPTER = {
    .size = sizeof(knpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

static const cpb_levels LEVELS = {
    .easy_depth = 2, .easy_nodes = 3000, .easy_noise = 150,
    .medium_depth = 4, .medium_nodes = 30000, .medium_noise = 30,
    .hard_nodes = 180000, .tt_bits = 18,
};

/* the move for the position p (its side to move), at a level: from | to << 7; -1: none */
int knb_choose(const knpos *p, int level, uint64_t seed, ai_stats *stats)
{
    knpos q = *p;
    int m;
    return cpb_turn(&ADAPTER, &q, &LEVELS, level, seed, &m, 1, stats) ? m : -1;
}

static int think(int seat, int level, char *move, int cap)
{
    knpos p = *kn_pos(konane_live());           /* the copy, at entry: nothing else is read */
    int score;
    if (p.side != seat || cap < 8 || a_over(&p, &score)) return 0;
    int m = knb_choose(&p, level, ai_session_seed(), NULL);
    if (m < 0 || ai_cancelled()) return 0;
    int n = p.n, a = m & 127, b = m >> 7;
    if (a == b) snprintf(move, (size_t)cap, "%c%d", 'a' + a % n, a / n + 1);
    else snprintf(move, (size_t)cap, "%c%d-%c%d", 'a' + a % n, a / n + 1, 'a' + b % n, b / n + 1);
    return 1;
}

static const bot konane_bot = {.think = think, .play = konane_show};
REGISTER_BOT(konane, &konane_bot)
