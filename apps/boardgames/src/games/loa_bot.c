/*
 * Lines of Action's bot (bots.h): alpha-beta (ai.h) over the rules' own position and moves (loa_rules.c: the moves
 * counted along their lines, the groups, the passes, the end). The evaluation: how tightly each player's pieces
 * gather round their centre of mass (beyond the tightest the count allows), their groups (fewer is better), the
 * centre of the board, pieces hemming in the other's, and the captures at hand. A move that joins all of the
 * player's pieces is a win the search sees at once.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("b1-b3", "a2xc4"), played by loa_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/loa.h"

/* ---- the adapter: the state is the rules' lpos; a move is from * 64 + to ----------------------------------------- */
static int a_moves(const void *st, int *out)
{
    const lpos *p = st;
    lmove l[LOA_MAX_MOVES];
    if (p->result != LOA_PLAY) return 0;
    int n = loa_legal(p, p->side, l);
    for (int i = 0; i < n; i++) out[i] = l[i].from * 64 + l[i].to;
    return n;
}

static void a_make(void *st, int m)
{
    lpos *p = st;
    int to = m % 64;
    loa_make(p, (lmove){(int8_t)(m / 64), (int8_t)to, (int8_t)(p->at[to] != 0)});
}

static int a_side(const void *st) { return ((const lpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const lpos *p = st;
    if (p->result == LOA_PLAY) return 0;
    *score = p->result == LOA_DRAW ? 0 : p->result - LOA_WIN0 == p->side ? ai_win(0) : -ai_win(0);
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const lpos *p = st;
    uint64_t h = p->side, w;
    for (int i = 0; i < LOA_SQUARES; i += 8) {
        memcpy(&w, p->at + i, 8);
        h = ai_mix(h, w);
    }
    return h;
}

static int cheb(int a, int b)                   /* squares apart, diagonals counting one */
{
    int dx = a % 8 - b % 8, dy = a / 8 - b / 8;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    return dx > dy ? dx : dy;
}

/* how spread out c's pieces are: the distances to their centre of mass beyond the least n pieces can have, a
 * hundredth of a square a piece; their groups; the centre of the board they hold */
static void shape(const lpos *p, int c, int *spread, int *groups, int *centre, int *n_out)
{
    int sq[LOA_SQUARES], n = 0, sx = 0, sy = 0;
    for (int s = 0; s < LOA_SQUARES; s++)
        if (p->at[s] == c + 1) {
            sq[n++] = s;
            sx += s % 8;
            sy += s / 8;
        }
    *n_out = n;
    *spread = *groups = *centre = 0;
    if (!n) return;
    int cx = (sx + n / 2) / n, cy = (sy + n / 2) / n, com = cy * 8 + cx, sum = 0, cen = 0;
    for (int i = 0; i < n; i++) {
        sum += cheb(sq[i], com);
        int x = sq[i] % 8, y = sq[i] / 8, dx = x < 4 ? 3 - x : x - 4, dy = y < 4 ? 3 - y : y - 4;
        cen += 3 - (dx > dy ? dx : dy);         /* 3 in the middle four, 0 on the edge */
    }
    int least = n - 1 <= 8 ? n - 1 : 8 + 2 * (n - 9);
    *spread = 100 * (sum - least) / n;
    *centre = cen;
    uint64_t left = 0;
    for (int i = 0; i < n; i++) left |= 1ull << sq[i];
    while (left) {
        (*groups)++;
        left &= ~loa_group(p, __builtin_ctzll(left));
    }
}

/* the side to move's view; 100 a square closer a piece */
static int a_eval(const void *st)
{
    const lpos *p = st;
    int me = p->side, s0, g0, c0, n0, s1, g1, c1, n1;
    shape(p, me, &s0, &g0, &c0, &n0);
    shape(p, !me, &s1, &g1, &c1, &n1);
    int v = 2 * (s1 - s0) + 45 * (g1 - g0) + 6 * (c0 - c1);
    v += 8 * (n0 - n1);                         /* a piece more: a little (fewer join more easily, but move less) */
    return v + 20;                              /* the move in hand */
}

static int a_order(const void *st, int m)       /* captures, then moves towards the centre */
{
    const lpos *p = st;
    int from = m / 64, to = m % 64, v = p->at[to] ? 30 : 0;
    int fx = from % 8, fy = from / 8, tx = to % 8, ty = to / 8;
    int df = (fx < 4 ? 3 - fx : fx - 4) + (fy < 4 ? 3 - fy : fy - 4);
    int dt = (tx < 4 ? 3 - tx : tx - 4) + (ty < 4 ? 3 - ty : ty - 4);
    return v + 4 * (df - dt);
}

static const ai_game ADAPTER = {
    .size = sizeof(lpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

/* the move (from * 64 + to) in the game's position, at a level; -1: none */
int lob_choose(const lpos *now, int level, uint64_t seed, ai_stats *stats)
{
    lpos p = *now;
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(&p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    o.budget.ms = o.budget.ms * 5 / 6;                 /* the clock is checked every so many nodes: a margin */
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 1; o.budget.nodes = 2000; o.budget.noise = 150; break;
    case AI_MEDIUM: o.budget.depth = 3; o.budget.nodes = 15000; o.budget.noise = 25; break;
    default:        o.budget.nodes = 120000; break;    /* about 0.3 s on a PC */
    }
    return ai_ab(&ADAPTER, &p, &o, stats);
}

static int think(int seat, int level, char *move, int cap)
{
    const lgame *g = loa_live();
    lpos now = *loa_pos(g);                     /* the copy, at entry: nothing else is read */
    if (now.result != LOA_PLAY || g->ply >= LOA_MAX_PLY || now.side != seat || cap < 6) return 0;
    int m = lob_choose(&now, level, ai_session_seed(), NULL);
    if (m < 0 || ai_cancelled()) return 0;
    int from = m / 64, to = m % 64;
    snprintf(move, (size_t)cap, "%c%c%c%c%c", 'a' + from % 8, '1' + from / 8, now.at[to] ? 'x' : '-', 'a' + to % 8,
             '1' + to / 8);
    return 1;
}

static const bot loa_bot = {.think = think, .play = loa_show};
REGISTER_BOT(loa, &loa_bot)
