/*
 * The men's morris games' bot (bots.h): nine, twelve and six men's morris (flying with three pieces where the variant
 * has it). Alpha-beta (ai.h) over the rules' own position (morris_legal, morris_make: pure); a mill's removal is a
 * move of its own by the same player (the search keeps the side, so the sign stays). The evaluation weighs the pieces
 * (on the board and in hand) far above all else, then the mills standing, the lines of two with the third point empty
 * (a mill in the making; two of them sharing a point: a double threat), the pieces that cannot move and the moves
 * each side has (in the moving phase), and a little the crossing points (more lines through them). A player with
 * fewer than three pieces, or with no move, has lost (the rules' end).
 * Easy looks two plies ahead with a lot of noise; medium four plies with a little; hard as deep as its budget allows.
 * Not searched: the draws by repetition and by fifty quiet moves (the search does not see the game's history).
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation, one action at a time: "d2" places, "d2-d3" moves, "xb6" removes (after a
 * mill: the bot is asked again, still the side to move); played by morris_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/morris.h"

/* a move as an int: its type, its from and to points (+ 1: -1 is 0) */
static int enc(mmove m) { return m.type << 10 | (m.from + 1) << 5 | (m.to + 1); }
static mmove dec(int v) { return (mmove){(int8_t)(v >> 10), (int8_t)((v >> 5 & 31) - 1), (int8_t)((v & 31) - 1)}; }

static int pieces(const mpos *p, int c) { return p->hand[c] + p->on[c]; }

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
/* the moves along the lines (moving, not flying): the same as morris_legal's, found from the lines' steps */
static int steps(const mpos *p, int *out)
{
    const mboard *b = morris_board(p->variant);
    int n = 0, c = p->side;
    for (int i = 0; i < b->lines; i++) {
        const int8_t *l = b->line[i];
        for (int k = 0; k < 2 && l[k + 1] >= 0; k++) {
            int a = l[k], d = l[k + 1];
            if (p->at[a] == c && p->at[d] < 0) out[n++] = enc((mmove){M_MOVE, (int8_t)a, (int8_t)d});
            else if (p->at[d] == c && p->at[a] < 0) out[n++] = enc((mmove){M_MOVE, (int8_t)d, (int8_t)a});
        }
    }
    return n;
}

static int a_moves(const void *st, int *out)
{
    const mpos *p = st;
    mmove m[MORRIS_MAX_MOVES * 4];
    if (!p->take && (pieces(p, p->side) < 3 || p->on[0] + p->on[1] == morris_board(p->variant)->points)) return 0;
    if (!p->take && !p->hand[p->side] && !morris_flying(p, p->side)) return steps(p, out);
    int n = morris_legal(p, m);
    for (int i = 0; i < n; i++) out[i] = enc(m[i]);
    return n;
}

static void a_make(void *st, int v) { morris_make(st, dec(v)); }
static int a_side(const void *st) { return ((const mpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const mpos *p = st;
    if (p->take) return 0;
    if (pieces(p, p->side) < 3) { *score = -ai_win(pieces(p, !p->side)); return 1; }
    if (p->on[0] + p->on[1] == morris_board(p->variant)->points) { *score = 0; return 1; }   /* full while placing */
    if (!p->hand[p->side] && !morris_flying(p, p->side)) {   /* moving: no move, a loss */
        int m[MORRIS_MAX_MOVES * 4];
        if (!steps(p, m)) { *score = -ai_win(pieces(p, !p->side)); return 1; }
    }
    return 0;
}

static uint64_t a_hash(const void *st)
{
    const mpos *p = st;
    uint64_t a = 0, b = 0;
    for (int i = 0; i < MORRIS_POINTS; i++) {
        a = a << 2 | (uint64_t)(p->at[i] + 1);
    }
    b = (uint64_t)p->hand[0] | (uint64_t)p->hand[1] << 8 | (uint64_t)p->side << 16 | (uint64_t)p->take << 17 |
        (uint64_t)p->variant << 18;
    return ai_mix(a, b);
}

/* the lines' contents for player c: its mills, its twos with the third point empty, and the points where two such
 * twos meet (a double threat); a line of two (six men's) counts as nothing */
static void lines(const mpos *p, int c, int *mills, int *twos, int *doubles)
{
    const mboard *b = morris_board(p->variant);
    int need[MORRIS_POINTS] = {0};
    *mills = *twos = *doubles = 0;
    for (int i = 0; i < b->lines; i++) {
        const int8_t *l = b->line[i];
        if (l[2] < 0) continue;
        int mine = 0, empty = -1, theirs = 0;
        for (int k = 0; k < 3; k++) {
            int a = p->at[l[k]];
            if (a == c) mine++;
            else if (a < 0) empty = l[k];
            else theirs++;
        }
        if (mine == 3) (*mills)++;
        else if (mine == 2 && !theirs && empty >= 0) {
            (*twos)++;
            if (++need[empty] == 2) (*doubles)++;
        }
    }
}

/* player c's pieces with no empty point next to them (moving), and its moves */
static void mobility(const mpos *p, int c, int *stuck, int *moves)
{
    const mboard *b = morris_board(p->variant);
    uint32_t free_next[MORRIS_POINTS] = {0};
    *stuck = *moves = 0;
    for (int i = 0; i < b->lines; i++) {
        const int8_t *l = b->line[i];
        for (int k = 0; k < 2 && l[k + 1] >= 0; k++) {
            int a = l[k], d = l[k + 1];
            if (p->at[d] < 0) free_next[a] |= 1u << (d & 31);
            if (p->at[a] < 0) free_next[d] |= 1u << (a & 31);
        }
    }
    for (int i = 0; i < b->points; i++)
        if (p->at[i] == c) {
            int k = __builtin_popcount(free_next[i]);
            *moves += k;
            *stuck += !k;
        }
}

/* the side to move's view; 100 a piece */
static int a_eval(const void *st)
{
    const mpos *p = st;
    const mboard *b = morris_board(p->variant);
    int me = p->side, v = 0;
    int pc[2] = {pieces(p, 0), pieces(p, 1)};
    v += 100 * (pc[me] - pc[!me]);
    if (p->take) v += 90;                       /* a removal to make: almost a piece */
    int mi[2], tw[2], db[2];
    lines(p, 0, &mi[0], &tw[0], &db[0]);
    lines(p, 1, &mi[1], &tw[1], &db[1]);
    v += 12 * (mi[me] - mi[!me]) + 18 * (tw[me] - tw[!me]) + 30 * (db[me] - db[!me]);
    int placing = p->hand[0] || p->hand[1];
    for (int c = 0; c < 2; c++) {
        int sign = c == me ? 1 : -1;
        if (morris_flying(p, c)) continue;      /* flying: never stuck */
        int stuck, moves;
        mobility(p, c, &stuck, &moves);
        if (!placing) {
            v += sign * (4 * moves - 10 * stuck);
            if (!moves && c == me) v -= 400;    /* no move: about lost */
        } else {
            v += sign * (2 * moves - 4 * stuck);
        }
    }
    if (placing)                                /* the crossing points: more lines, more chances */
        for (int k = 0; k < b->lines; k++)
            for (int j = 0; j < 3 && b->line[k][j] >= 0; j++) {
                int a = p->at[b->line[k][j]];
                if (a >= 0) v += a == me ? 3 : -3;
            }
    return v;
}

/* the moves that close a mill first, then the removals of pieces in a two (a mill in the making) */
static int a_order(const void *st, int v)
{
    const mpos *p = st;
    mmove m = dec(v);
    if (m.type == M_TAKE) {
        mpos q = *p;
        int before[3], after[3];
        lines(&q, !q.side, &before[0], &before[1], &before[2]);
        q.at[m.from] = -1;
        lines(&q, !q.side, &after[0], &after[1], &after[2]);
        return 10 * (before[1] - after[1]) + 20 * (before[2] - after[2]);
    }
    mpos q = *p;
    if (m.type == M_MOVE) q.at[m.from] = -1;
    q.at[m.to] = (int8_t)p->side;
    return morris_in_mill(&q, m.to) ? 50 : 0;
}

static const ai_game ADAPTER = {
    .size = sizeof(mpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

/* the action for the position (its side to move), at a level; *m set, 0: none */
int mrb_choose(const mpos *p, int level, uint64_t seed, mmove *m, ai_stats *stats)
{
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 2; o.budget.nodes = 4000; o.budget.noise = 120; break;
    case AI_MEDIUM: o.budget.depth = 4; o.budget.nodes = 40000; o.budget.noise = 25; break;
    default:        o.budget.nodes = 200000; break;     /* about 0.3 s on a PC */
    }
    int v = ai_ab(&ADAPTER, p, &o, stats);
    if (v < 0) return 0;
    *m = dec(v);
    return 1;
}

static void name(const mboard *b, int pt, char *s)
{
    s[0] = (char)('a' + b->x[pt]);
    s[1] = (char)('1' + b->y[pt]);
}

static int think(int seat, int level, char *move, int cap)
{
    mpos p = *morris_pos(morris_live());        /* the copy, at entry (the position only): nothing else is read */
    mmove m;
    mmove legal[MORRIS_MAX_MOVES * 4];
    if (p.side != seat || cap < 7 || !morris_legal(&p, legal)) return 0;
    if (!mrb_choose(&p, level, ai_session_seed(), &m, NULL) || ai_cancelled()) return 0;
    const mboard *b = morris_board(p.variant);
    char *s = move;
    if (m.type == M_TAKE) { *s++ = 'x'; name(b, m.from, s); s += 2; }
    else if (m.type == M_PLACE) { name(b, m.to, s); s += 2; }
    else { name(b, m.from, s); s[2] = '-'; name(b, m.to, s + 3); s += 5; }
    *s = 0;
    return 1;
}

static const bot morris_bot = {.think = think, .play = morris_show};
REGISTER_BOT(morris, &morris_bot)
