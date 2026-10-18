/*
 * Twist Five's bot (bots.h): alpha-beta (ai.h) over whole turns, a marble and a twist as one move (the twists that give
 * the same board, an empty quarter's or a quarter turned either way to the same marbles, searched once; a marble that
 * makes five alone, its turn ending there). The evaluation counts the rows of five still open: each row holding only
 * one player's marbles is worth more the more it holds (a row of four with the fifth hole empty wins on the next
 * marble; two such holes of the other's cannot both be stopped), and the quarters' middles, which never move, a
 * little more.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: a whole turn "c4/2+", a winning marble alone "c4", the twist alone "/2+" when the
 * marble is already placed; played by twistfive_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/twistfive.h"

typedef struct {
    uint64_t line[TV_LINES];
    uint8_t from[4][9], to[4][2][9];            /* each quarter's holes, and where each goes turned (TV_CW, TV_CCW) */
    uint8_t on[TV_CELLS][12], non[TV_CELLS];    /* the rows through each hole */
} tbgeo;

typedef struct {
    const tbgeo *g;                             /* the search's own tables (never the game's live data) */
    uint64_t b[2];                              /* each player's marbles */
    uint8_t side, count;
    int8_t res;                                 /* -1 playing, the winner, TV_DRAW */
    int8_t fixed;                               /* the marble already placed this turn (only its twist to choose), -1 */
    uint8_t scan;                               /* the evaluation looks for the mover's five in one turn */
} tbpos;

#define BIT(h) (1ull << (h))
static const int MIDDLE[4] = {1 + 4 * TV_N, 4 + 4 * TV_N, 1 + 1 * TV_N, 4 + 1 * TV_N};

static void geo_init(tbgeo *g)
{
    memset(g->non, 0, sizeof g->non);
    for (int l = 0; l < TV_LINES; l++) {
        g->line[l] = 0;
        for (int k = 0; k < 5; k++) {
            int h = tv_line[l][k];
            g->line[l] |= BIT(h);
            g->on[h][g->non[h]++] = (uint8_t)l;
        }
    }
    for (int q = 0; q < 4; q++) {
        int ox = (q & 1) * 3, oy = q & 2 ? 0 : 3, n = 0;
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++, n++) {
                int h = ox + i + TV_N * (oy + j);
                g->from[q][n] = (uint8_t)h;
                g->to[q][TV_CW][n] = (uint8_t)tv_turned(h, q, TV_CW);
                g->to[q][TV_CCW][n] = (uint8_t)tv_turned(h, q, TV_CCW);
            }
    }
}

static uint64_t turn_bits(const tbgeo *g, uint64_t b, int q, int d)
{
    uint64_t out = b;
    for (int n = 0; n < 9; n++) out &= ~BIT(g->from[q][n]);
    for (int n = 0; n < 9; n++)
        if (b & BIT(g->from[q][n])) out |= BIT(g->to[q][d][n]);
    return out;
}

static int five(const tbgeo *g, uint64_t b)
{
    for (int l = 0; l < TV_LINES; l++)
        if ((b & g->line[l]) == g->line[l]) return 1;
    return 0;
}

/* a move: hole * 8 + quarter * 2 + direction (a winning marble: hole * 8, its twist never made) */
static int a_moves(const void *st, int *out)
{
    const tbpos *p = st;
    if (p->res >= 0) return 0;
    const tbgeo *g = p->g;
    uint64_t all = p->b[0] | p->b[1];
    int n = 0;
    for (int h = 0; h < TV_CELLS; h++) {
        if (p->fixed >= 0 ? h != p->fixed : (all >> h & 1)) continue;
        uint64_t me = p->b[p->side] | BIT(h), op = p->b[!p->side];
        if (p->fixed < 0 && five(g, me)) { out[n++] = h * 8; continue; }
        int still = 0;                          /* a twist that changes nothing: once for this marble */
        for (int q = 0; q < 4; q++) {
            uint64_t m0 = turn_bits(g, me, q, TV_CW), o0 = turn_bits(g, op, q, TV_CW);
            if (m0 == me && o0 == op) {
                if (!still) out[n++] = h * 8 + q * 2, still = 1;
                continue;
            }
            out[n++] = h * 8 + q * 2 + TV_CW;
            if (turn_bits(g, me, q, TV_CCW) != m0 || turn_bits(g, op, q, TV_CCW) != o0) out[n++] = h * 8 + q * 2 + TV_CCW;
        }
    }
    return n;
}

static void a_make(void *st, int m)
{
    tbpos *p = st;
    const tbgeo *g = p->g;
    int h = m / 8, q = m / 2 % 4, d = m % 2, me = p->side;
    p->fixed = -1;
    p->b[me] |= BIT(h);
    p->count++;
    p->side ^= 1;
    if (five(g, p->b[me])) { p->res = (int8_t)me; return; }
    p->b[0] = turn_bits(g, p->b[0], q, d);
    p->b[1] = turn_bits(g, p->b[1], q, d);
    int a = five(g, p->b[me]), b = five(g, p->b[!me]);
    p->res = (int8_t)(a && b ? TV_DRAW : a ? me : b ? !me : p->count == TV_CELLS ? TV_DRAW : -1);
}

static int a_side(const void *st) { return ((const tbpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const tbpos *p = st;
    if (p->res < 0) return 0;
    *score = p->res == TV_DRAW ? 0 : p->res == p->side ? ai_win(0) : -ai_win(0);
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const tbpos *p = st;
    return ai_mix(ai_mix(p->b[0], p->b[1]), (uint64_t)p->side << 8 | (uint8_t)p->fixed);
}

/* 1: the player to move has a turn that makes five (theirs only) */
static int win_in_one(const tbpos *p)
{
    const tbgeo *g = p->g;
    uint64_t all = p->b[0] | p->b[1];
    for (int h = 0; h < TV_CELLS; h++) {
        if (all >> h & 1) continue;
        uint64_t me = p->b[p->side] | BIT(h), op = p->b[!p->side];
        if (five(g, me)) return 1;
        for (int q = 0; q < 4; q++)
            for (int d = 0; d < 2; d++)
                if (five(g, turn_bits(g, me, q, d)) && !five(g, turn_bits(g, op, q, d))) return 1;
    }
    return 0;
}

/* the side to move's view; 20 is about a row of three. Hard also looks for the mover's five in one turn (a twist can
 * make one out of scattered marbles): its search stops after its own turn (an odd depth) and would else walk into it */
static int a_eval(const void *st)
{
    static const int W[5] = {0, 1, 5, 20, 70};
    const tbpos *p = st;
    uint64_t me = p->b[p->side], op = p->b[!p->side], holes = 0;
    int v = 0, most = 0;
    for (int l = 0; l < TV_LINES; l++) {
        uint64_t L = p->g->line[l];
        int m = __builtin_popcountll(me & L), o = __builtin_popcountll(op & L);
        if (!o) {
            if (m == 4) return 5000;            /* the fifth hole empty, and the move: five on the next marble */
            v += W[m];
            if (m > most) most = m;
        }
        if (!m) {
            v -= o == 4 ? 150 : W[o];
            if (o == 4) holes |= L & ~op;       /* where the other's fours finish */
        }
    }
    if (__builtin_popcountll(holes) >= 2) v -= 1500;   /* two of them: one marble stops one only */
    if (p->scan && win_in_one(p)) return 5000;
    for (int q = 0; q < 4; q++) v += 4 * ((me >> MIDDLE[q] & 1) - (op >> MIDDLE[q] & 1));
    return v;
}

/* a winning marble first, then the marbles that do most for one's rows and against the other's (the twists left to
 * the killers and the history: weighing each turn here would cost more than it saves) */
static int a_order(const void *st, int m)
{
    static const int W[5] = {0, 2, 6, 20, 60};
    const tbpos *p = st;
    const tbgeo *g = p->g;
    int h = m / 8;
    uint64_t me = p->b[p->side] | BIT(h), op = p->b[!p->side];
    if (p->fixed < 0 && five(g, me)) return 100000;
    int v = 0;
    for (int k = 0; k < g->non[h]; k++) {
        uint64_t L = g->line[g->on[h][k]];
        int a = __builtin_popcountll(me & L), b = __builtin_popcountll(op & L);
        if (!b) v += W[a < 5 ? a : 4];
        if (a == 1) v += W[b];                  /* a row of the other's spoilt */
    }
    return v;
}
static const ai_game ADAPTER = {
    .size = sizeof(tbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 20,
};

/* the turn for the player to move, at a level: the marble's hole (-1: already placed), the quarter and direction (q
 * -1: a winning marble, no twist); 0: none */
int tvb_choose(const tvpos *now, int level, uint64_t seed, int *hole, int *q, int *dir, ai_stats *stats)
{
    if (now->winner >= 0 || ai_cancelled()) return 0;
    tbgeo geo;
    geo_init(&geo);
    tbpos s = {.g = &geo, .side = now->side, .count = now->count, .res = -1, .fixed = -1, .scan = level >= AI_HARD};
    for (int h = 0; h < TV_CELLS; h++)
        if (now->cell[h]) s.b[now->cell[h] - 1] |= BIT(h);
    if (now->stage == TV_TWIST) {               /* the marble is down: take it back, and search its twists only */
        s.b[now->side] &= ~BIT(now->last);
        s.count--;
        s.fixed = now->last;
    }
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(&s)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    o.budget.ms = o.budget.ms * 5 / 6;          /* the clock is checked every so many nodes: a margin */
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 1; o.budget.noise = 220; break;
    case AI_MEDIUM: o.budget.depth = 2; o.budget.nodes = 20000; o.budget.noise = 40; break;
    /* hard: three turns ahead (deeper costs too much on a phone), looking for each leaf's five in one turn */
    default:        o.budget.nodes = 60000; o.budget.depth = 3; break;
    }
    int m = ai_ab(&ADAPTER, &s, &o, stats);
    if (m < 0 || ai_cancelled()) return 0;
    tbpos t = s;
    a_make(&t, m);
    *hole = now->stage == TV_TWIST ? -1 : m / 8;
    int won = t.res == now->side && five(&geo, s.b[now->side] | BIT(m / 8));
    *q = won && now->stage == TV_PLACE ? -1 : m / 2 % 4;
    *dir = m % 2;
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    tvpos now = *tv_pos(twistfive_live());      /* read once, at entry (the main thread leaves it alone meanwhile) */
    if (now.winner >= 0 || now.side != seat || cap < 8) return 0;
    int h, q, d;
    if (!tvb_choose(&now, level, ai_session_seed(), &h, &q, &d, NULL)) return 0;
    char cell[3] = {0};
    if (h >= 0) cell[0] = (char)('a' + h % TV_N), cell[1] = (char)('1' + h / TV_N);
    if (q < 0) snprintf(move, (size_t)cap, "%s", cell);
    else snprintf(move, (size_t)cap, "%s/%d%c", cell, q + 1, d == TV_CW ? '+' : '-');
    return 1;
}

static const bot twistfive_bot = {.think = think, .play = twistfive_show};
REGISTER_BOT(twistfive, &twistfive_bot)
