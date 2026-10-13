/*
 * Reversi's bot (bots.h): alpha-beta (ai.h) over the rules' position with bitboard moves; the evaluation weighs
 * mobility (moves now and the frontier: discs next to empty squares give the other player moves), the corners, the
 * squares next to an empty corner (they give it away), stable discs (that can never turn: from the corners, along
 * full lines) and, near the end, the discs; the last moves are searched to the end (the search's depth reaches it).
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("d3": the column a..h, the row 1..8 from the top), played by reversi_show().
 */
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/reversi.h"

#define FA 0x0101010101010101ull                /* the column a */
#define FH 0x8080808080808080ull                /* the column h */
#define R1 0x00000000000000ffull                /* the row 1 (the top) */
#define R8 0xff00000000000000ull                /* the row 8 */
#define CORNERS 0x8100000000000081ull

static const int DIRS[8] = {1, -1, 8, -8, 9, 7, -7, -9};

static inline uint64_t sh(uint64_t b, int d)  /* every disc one step in direction d (off the board: gone) */
{
    switch (d) {
    case 1:  return (b << 1) & ~FA;
    case -1: return (b >> 1) & ~FH;
    case 8:  return b << 8;
    case -8: return b >> 8;
    case 9:  return (b << 9) & ~FA;
    case 7:  return (b << 7) & ~FH;
    case -7: return (b >> 7) & ~FA;
    default: return (b >> 9) & ~FH;
    }
}

static inline int pop(uint64_t b) { return __builtin_popcountll(b); }

/* the squares P can play against O (as rv_legal) */
uint64_t rvb_moves(uint64_t P, uint64_t O)
{
    uint64_t empty = ~(P | O), m = 0;
    for (int i = 0; i < 8; i++) {
        int d = DIRS[i];
        uint64_t x = sh(P, d) & O;
        x |= sh(x, d) & O;
        x |= sh(x, d) & O;
        x |= sh(x, d) & O;
        x |= sh(x, d) & O;
        x |= sh(x, d) & O;
        m |= sh(x, d) & empty;
    }
    return m;
}

/* the discs P's disc on s turns (as rv_flips) */
uint64_t rvb_flips(uint64_t P, uint64_t O, int s)
{
    uint64_t f = 0, b = 1ull << s;
    for (int i = 0; i < 8; i++) {
        uint64_t line = 0, x = sh(b, DIRS[i]);
        while (x & O) { line |= x; x = sh(x, DIRS[i]); }
        if (x & P) f |= line;
    }
    return f;
}

/* rv_make, with the bitboard moves */
void rvb_make(rpos *p, int s)
{
    int me = p->side;
    uint64_t f = rvb_flips(p->disc[me], p->disc[!me], s);
    p->disc[me] |= f | 1ull << s;
    p->disc[!me] &= ~f;
    p->passed = 0;
    if (rvb_moves(p->disc[!me], p->disc[me])) p->side = !me;
    else if (rvb_moves(p->disc[me], p->disc[!me])) p->passed = 1;
    else p->over = 1;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    const rpos *p = st;
    if (p->over) return 0;
    int n = 0;
    for (uint64_t m = rvb_moves(p->disc[p->side], p->disc[!p->side]); m; m &= m - 1) out[n++] = __builtin_ctzll(m);
    return n;
}

static void a_make(void *st, int m) { rvb_make(st, m); }
static int a_side(const void *st) { return ((const rpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const rpos *p = st;
    if (!p->over) return 0;
    int d = pop(p->disc[p->side]) - pop(p->disc[!p->side]);
    *score = d > 0 ? ai_win(d) : d < 0 ? -ai_win(-d) : 0;
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const rpos *p = st;
    return ai_mix(p->disc[0], ai_mix(p->disc[1], (uint64_t)p->side));
}

/* the squares whose whole line in direction d (both ways) is filled */
static uint64_t full_lines(uint64_t filled, int d, uint64_t edge_back, uint64_t edge_fwd)
{
    uint64_t a = filled, b = filled;
    for (int k = 0; k < 7; k++) {
        a &= sh(a, d) | edge_back;              /* filled back to the edge */
        b &= sh(b, -d) | edge_fwd;              /* and forward to the other */
    }
    return a & b;
}

/* P's discs that can never turn: along each of the four lines, the line full, or an edge or a stable disc of P's next
 * to it on one side (from the corners outwards) */
static uint64_t stable(uint64_t P, uint64_t O)
{
    uint64_t filled = P | O, E = FA | FH | R1 | R8;
    uint64_t h = full_lines(filled, 1, FA, FH) | FA | FH;
    uint64_t v = full_lines(filled, 8, R1, R8) | R1 | R8;
    uint64_t d9 = full_lines(filled, 9, FA | R1, FH | R8) | E;
    uint64_t d7 = full_lines(filled, 7, FH | R1, FA | R8) | E;
    uint64_t s = 0;
    for (int k = 0; k < 16; k++) {
        uint64_t n = P & (h | sh(s, 1) | sh(s, -1)) & (v | sh(s, 8) | sh(s, -8)) & (d9 | sh(s, 9) | sh(s, -9))
                     & (d7 | sh(s, 7) | sh(s, -7));
        if (n == s) break;
        s = n;
    }
    return s;
}

/* the squares next to each corner: X diagonally, C along the edges (corner order a1 h1 a8 h8) */
static const int CORNER_SQ[4] = {0, 7, 56, 63};
static const uint64_t X_SQ[4] = {1ull << 9, 1ull << 14, 1ull << 49, 1ull << 54};
static const uint64_t C_SQ[4] = {1ull << 1 | 1ull << 8, 1ull << 6 | 1ull << 15, 1ull << 48 | 1ull << 57,
                                 1ull << 55 | 1ull << 62};

/* the side to move's view; about 100 a good square */
static int a_eval(const void *st)
{
    const rpos *p = st;
    uint64_t P = p->disc[p->side], O = p->disc[!p->side], empty = ~(P | O);
    int discs = pop(P | O);
    int mp = pop(rvb_moves(P, O)), mo = pop(rvb_moves(O, P));
    int v = 0;
    v += 900 * (mp - mo) / (mp + mo + 2);       /* mobility */
    uint64_t near = sh(empty, 1) | sh(empty, -1) | sh(empty, 8) | sh(empty, -8) | sh(empty, 9) | sh(empty, 7)
                    | sh(empty, -7) | sh(empty, -9);
    int fp = pop(P & near), fo = pop(O & near);
    v += 500 * (fo - fp) / (fp + fo + 2);       /* the frontier */
    v += 900 * (pop(P & CORNERS) - pop(O & CORNERS));
    for (int c = 0; c < 4; c++)                 /* next to an empty corner: it gives the corner away */
        if (empty >> CORNER_SQ[c] & 1) {
            v -= 300 * (pop(P & X_SQ[c]) - pop(O & X_SQ[c]));
            v -= 120 * (pop(P & C_SQ[c]) - pop(O & C_SQ[c]));
        }
    if (discs > 20) v += 70 * (pop(stable(P, O)) - pop(stable(O, P)));
    if (discs > 44) v += (discs - 44) * 12 * (pop(P) - pop(O));   /* near the end: the discs themselves */
    return v;
}

static int a_order(const void *st, int m)       /* corners first, the squares next to an empty corner last */
{
    const rpos *p = st;
    uint64_t b = 1ull << m, empty = ~(p->disc[0] | p->disc[1]);
    if (b & CORNERS) return 20;
    for (int c = 0; c < 4; c++)
        if ((empty >> CORNER_SQ[c] & 1) && (b & (X_SQ[c] | C_SQ[c]))) return b & X_SQ[c] ? -20 : -8;
    return 0;
}

static const ai_game ADAPTER = {
    .size = sizeof(rpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

/* the move for the position p (its side to move), at a level; -1: none */
int rvb_choose(const rpos *p, int level, uint64_t seed, ai_stats *stats)
{
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 2; o.budget.nodes = 3000; o.budget.noise = 400; break;
    case AI_MEDIUM: o.budget.depth = 4; o.budget.nodes = 40000; o.budget.noise = 60; break;
    default:        o.budget.nodes = 1500000; break;    /* about 0.3 s on a PC */
    }
    return ai_ab(&ADAPTER, p, &o, stats);
}

static int think(int seat, int level, char *move, int cap)
{
    rpos p = *rv_pos(reversi_live());           /* the copy, at entry: nothing else is read */
    if (p.over || p.side != seat || cap < 3) return 0;
    int s = rvb_choose(&p, level, ai_session_seed(), NULL);
    if (s < 0 || ai_cancelled()) return 0;
    move[0] = (char)('a' + s % 8);
    move[1] = (char)('1' + s / 8);
    move[2] = 0;
    return 1;
}

static const bot reversi_bot = {.think = think, .play = reversi_show};
REGISTER_BOT(reversi, &reversi_bot)
