/*
 * Senet's bot (bots.h): expectiminimax (ai_expecti) over the rules' position, the four throwing sticks a chance node
 * (1..5: 4, 6, 4, 1, 1 ways in 16; a 1, 4 or 5 throws again, so a turn may be several moves deep). The evaluation
 * weighs each piece's way along the path, the pieces borne off, pairs (safe from a swap), threes (a wall the other
 * player cannot pass), and the lone pieces the other player can reach with their next throw.
 * The throw itself is a move: "roll", played by senet_show() with the game's own sticks, as a tap throws them.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: "s<n>", the square of the piece moved.
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/senet.h"

static const int WAYS[6] = {0, 4, 6, 4, 1, 1};   /* the throws 1..5, in 16 */
#define PASS 0                                  /* a throw no piece can use: a move of its own in the search */

/* ---- the evaluation ---------------------------------------------------------------------------------------------- */
static int owner(const senet_pos *p, int s) { return s >= 1 && s <= SENET_SQ ? p->sq[s] - 1 : -1; }

/* what `att` (to throw next) can expect to gain by swapping a lone piece of the other player back: for each throw,
 * the longest way back it sends one (a rough reach: no blocks, no House rules) */
static int threat(const senet_pos *p, int att)
{
    int sum = 0;
    for (int r = 1; r <= 5; r++) {
        int best = 0;
        for (int s = 1; s + r <= SENET_SQ; s++) {
            if (owner(p, s) != att || owner(p, s + r) != !att || senet_protected(p, s + r)) continue;
            if (s < SENET_BEAUTY && s + r > SENET_BEAUTY) continue;
            best = r > best ? r : best;
        }
        sum += WAYS[r] * best;
    }
    return sum * 10 / 16;
}

static int side_value(const senet_pos *p, int me)
{
    int v = p->off[me] * 360, run = 0;
    for (int s = 1; s <= SENET_SQ + 1; s++) {
        int mine = owner(p, s) == me;
        if (mine) {
            v += 10 * s;
            if (senet_protected(p, s)) v += 12;
            if (s >= 28) v += 8;                /* one exact throw from leaving */
            if (s == SENET_BEAUTY) v += 4;
        }
        if (mine) run++;
        else {
            if (run >= 3) v += 25;              /* a wall */
            run = 0;
        }
    }
    return v;
}

/* the side to move's view */
static int eval_pos(const senet_pos *p)
{
    int me = p->turn, op = !me;
    int v = side_value(p, me) - side_value(p, op);
    if (p->phase == SENET_MOVE && !senet_movable(p)) return v - threat(p, op);   /* lost: the other throws next */
    if (p->phase == SENET_MOVE) {               /* the throw is known: a swap it makes now */
        uint32_t m = senet_movable(p);
        for (int s = 1; s <= SENET_SQ; s++)
            if (m >> s & 1 && senet_takes(p, s)) return v + 10 * p->roll;
        return v;
    }
    return v + threat(p, me);                   /* to throw: the swaps the sticks may give */
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    const senet_pos *p = st;
    if (p->phase != SENET_MOVE) return 0;
    int n = 0;
    uint32_t m = senet_movable(p);
    for (int s = SENET_SQ; s >= 1; s--)
        if (m >> s & 1) out[n++] = s;
    if (!n) out[n++] = PASS;
    return n;
}

static int a_chance(const void *st, int *out, int *w)
{
    const senet_pos *p = st;
    if (p->phase != SENET_THROW) return 0;
    for (int v = 1; v <= 5; v++) out[v - 1] = v, w[v - 1] = WAYS[v];
    return 5;
}

static void a_make(void *st, int m)
{
    senet_pos *p = st;
    if (p->phase == SENET_THROW) {              /* the throw: the move to choose (or PASS) comes next */
        senet_pos q = *p;
        senet_roll(&q, m);
        if (q.phase == SENET_MOVE) *p = q;
        else {                                  /* lost: PASS is its move, the turn passing there */
            p->roll = (uint8_t)m;
            p->throws++;
            p->phase = SENET_MOVE;
            p->back = 0;
        }
    } else if (m == PASS) {
        p->turn ^= 1;
        p->phase = SENET_THROW;
    } else {
        senet_step(p, m);
    }
}

static int a_side(const void *st) { return ((const senet_pos *)st)->turn; }

static int a_over(const void *st, int *score)
{
    const senet_pos *p = st;
    if (p->winner < 0) return 0;
    int margin = p->pieces - p->off[!p->winner];
    *score = p->winner == p->turn ? ai_win(margin) : -ai_win(margin);
    return 1;
}

static int a_eval(const void *st) { return eval_pos(st); }

static uint64_t a_hash(const void *st)
{
    const senet_pos *p = st;
    uint64_t a = 0, b = 0;
    for (int s = 1; s <= SENET_SQ; s++) {
        a = a << 1 | (p->sq[s] == 1);
        b = b << 1 | (p->sq[s] == 2);
    }
    return ai_mix(ai_mix(a, b), (uint64_t)(p->turn | p->phase << 1 | p->roll << 3 | p->off[0] << 6 |
                                           p->off[1] << 9 | p->back << 12));
}

static int a_order(const void *st, int m)       /* off the board, swaps, then the furthest piece */
{
    const senet_pos *p = st;
    if (m == PASS) return 0;
    int to = senet_dest(p, m);
    return (to == SENET_OFF ? 2000 : 0) + (senet_takes(p, m) ? 1000 : 0) + (to == SENET_WATER ? -500 : 0) + m;
}

static const ai_game ADAPTER = {
    .size = sizeof(senet_pos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .chance = a_chance, .unit = 10,   /* unit: a square of the way */
};

/* the piece to move (its square) for the position p (a throw made), at a level; 0: none */
int senet_choose(const senet_pos *p, int level, uint64_t seed, ai_stats *stats)
{
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 16};
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 1; o.budget.nodes = 2000; o.budget.noise = 600; break;
    case AI_MEDIUM: o.budget.depth = 3; o.budget.nodes = 15000; o.budget.noise = 60; break;
    default:        o.budget.depth = 0; o.budget.nodes = 60000; break;    /* about 0.05 s on a PC (more buys little) */
    }
    if (stats) memset(stats, 0, sizeof *stats);
    int mv[AI_MAX_MOVES];
    if (a_moves(p, mv) == 1) return mv[0];      /* nothing to choose */
    int m = ai_expecti(&ADAPTER, p, &o, stats);
    return m < 0 ? mv[0] : m;
}

static int think(int seat, int level, char *move, int cap)
{
    senet_pos p = senet_live()->pos;            /* the copy, at entry: nothing else is read */
    if (p.phase == SENET_OVER || p.turn != seat || cap < 5) return 0;
    if (p.phase == SENET_THROW) {               /* the sticks are the game's: the bot only throws them */
        strcpy(move, "roll");
        return 1;
    }
    int s = senet_choose(&p, level, ai_session_seed(), NULL);
    if (s <= 0 || ai_cancelled()) return 0;
    snprintf(move, (size_t)cap, "s%d", s);
    return 1;
}

static const bot senet_bot = {.think = think, .play = senet_show};
REGISTER_BOT(senet, &senet_bot)
