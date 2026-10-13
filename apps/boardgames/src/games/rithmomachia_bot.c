/*
 * Rithmomachia's bot (bots.h), every victory: alpha-beta (ai.h) over the rules' position with captures found from
 * the piece taken outwards (the squares an attacker could stand on, the rays of an assault, the four sides of a
 * siege) instead of the rules' search of the whole board: the same captures (tests/ai_rithmomachia_test.c checks
 * it), many times faster. The ends as the rules have them (rithmomachia_rules.c: the common victory of 8 or 12 pieces,
 * the proper victory of a harmony in the enemy's half, no move, 100 moves without a capture).
 * The evaluation: the pieces taken; for the proper victory also the pieces in the enemy's half and the pairs there
 * that a third piece of one's own could join in a progression; and a little for going forward.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("c4d5", "b14b12"), played by rithmomachia_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/rithmomachia.h"

#define DRAW 150                                /* a draw, for the bot: a piece and a half lost (its contempt) */

typedef struct {
    rhmpos p;
    uint8_t variant;
    uint8_t root;                               /* the bot's side */
} rbpos;

static const int8_t RAY[8][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};

static inline int inside(int x, int y) { return x >= 0 && y >= 0 && x < RHM_W && y < RHM_H; }
static inline const rhmpiece *piece(const rhmpos *p, int s) { return &rhm_piece[p->sq[s] - 1]; }
static inline int owner(const rhmpos *p, int s) { return p->sq[s] ? rhm_piece[p->sq[s] - 1].owner : -1; }

/* 1: player takes the piece on s (rhm_threat's four ways, looked for from s) */
static int taken(const rhmpos *p, int player, int s)
{
    int w = piece(p, s)->value, x = s % RHM_W, y = s / RHM_W, val[12], n = 0;
    /* the pieces that could move onto s: diagonally next to it (a round, a pyramid), two away along a row or a column
     * over an empty square (a triangle, a pyramid), three away over two (a square, a pyramid) */
    for (int d = 0; d < 8; d++) {
        int dx = RAY[d][0], dy = RAY[d][1];
        if (d < 4) {
            if (!inside(x + dx, y + dy)) continue;
            int a = RHM_SQ(x + dx, y + dy);
            if (owner(p, a) == player && (piece(p, a)->shape == RHM_ROUND || piece(p, a)->shape == RHM_PYRAMID))
                val[n++] = piece(p, a)->value;
            continue;
        }
        for (int k = 1; k <= 3; k++) {
            if (!inside(x + k * dx, y + k * dy)) break;
            int a = RHM_SQ(x + k * dx, y + k * dy);
            if (!p->sq[a]) continue;
            if (k > 1 && owner(p, a) == player) {
                int sh = piece(p, a)->shape;
                if (sh == RHM_PYRAMID || sh == (k == 2 ? RHM_TRIANGLE : RHM_SQUARE)) val[n++] = piece(p, a)->value;
            }
            break;
        }
    }
    for (int i = 0; i < n; i++)                 /* meeting */
        if (val[i] == w) return 1;
    for (int d = 0; d < 8; d++) {               /* assault: the first piece along a ray, over gap > 1 empty squares */
        int gap = 0, tx = x + RAY[d][0], ty = y + RAY[d][1];
        while (inside(tx, ty) && !p->sq[RHM_SQ(tx, ty)]) tx += RAY[d][0], ty += RAY[d][1], gap++;
        if (gap < 2 || !inside(tx, ty)) continue;
        int a = RHM_SQ(tx, ty);
        if (owner(p, a) != player) continue;
        int sh = piece(p, a)->shape;
        if ((sh == RHM_PYRAMID || (d < 4 ? sh == RHM_ROUND : sh != RHM_ROUND)) && piece(p, a)->value * gap == w)
            return 1;
    }
    for (int i = 0; i < n; i++)                 /* ambush */
        for (int j = i + 1; j < n; j++)
            if (val[i] + val[j] == w) return 1;
    for (int d = 4; d < 8; d++) {               /* siege */
        int nx = x + RAY[d][0], ny = y + RAY[d][1];
        if (inside(nx, ny) && owner(p, RHM_SQ(nx, ny)) != player) return 0;
    }
    return 1;
}

/* rhm_make with the captures found from each piece */
static void make(rbpos *b, int from, int to)
{
    rhmpos *p = &b->p;
    p->sq[to] = p->sq[from];
    p->sq[from] = 0;
    int me = p->side, gone[RHM_PIECES], n = 0;
    for (int s = 0; s < RHM_N; s++)
        if (owner(p, s) == !me && taken(p, me, s)) gone[n++] = s;
    for (int i = 0; i < n; i++) p->sq[gone[i]] = 0;
    p->taken[me] = (uint8_t)(p->taken[me] + n);
    p->quiet = n ? 0 : (uint8_t)(p->quiet + 1);
    p->proper = b->variant == RHM_PROPER && rhm_find_harmony(p, me, p->harmony);
    p->side ^= 1;
}

int rhb_gen(const rhmpos *p, int *out);       /* the moves (from * 128 + to; out NULL: 1 if any) */

/* the test's check: the bot's move from -> to in the rules' position gives the rules' rhm_make */
int rhb_agrees(const rhmpos *r, int variant, rhmmove m)
{
    rbpos b = {.p = *r, .variant = (uint8_t)variant};
    rhmpos q = *r;
    make(&b, m.from, m.to);
    rhm_make(&q, m, variant);
    return !memcmp(&b.p, &q, sizeof q);
}

/* ---- the adapter: a move is from * 128 + to ---------------------------------------------------------------------- */
int rhb_gen(const rhmpos *p, int *out)
{
    static const int8_t STEP[12][3] = {     /* dx, dy, the squares between that must be empty */
        {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {2, 0, 1}, {-2, 0, 1}, {0, 2, 1}, {0, -2, 1},
        {3, 0, 2}, {-3, 0, 2}, {0, 3, 2}, {0, -3, 2}};
    int n = 0;
    for (int s = 0; s < RHM_N; s++) {
        if (owner(p, s) != p->side) continue;
        int sh = piece(p, s)->shape, x = s % RHM_W, y = s / RHM_W;
        int t0 = sh == RHM_ROUND || sh == RHM_PYRAMID ? 0 : sh == RHM_TRIANGLE ? 4 : 8;
        int t1 = sh == RHM_ROUND ? 4 : sh == RHM_TRIANGLE ? 8 : 12;
        for (int t = t0; t < t1; t++) {
            int dx = STEP[t][0], dy = STEP[t][1], tx = x + dx, ty = y + dy;
            if (!inside(tx, ty) || p->sq[RHM_SQ(tx, ty)]) continue;
            int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0), ok = 1;
            for (int k = 1; k <= STEP[t][2] && ok; k++) ok = !p->sq[RHM_SQ(x + k * sx, y + k * sy)];
            if (!ok) continue;
            if (!out) return 1;                 /* only asked whether there is one */
            out[n++] = s * RHM_N + RHM_SQ(tx, ty);
        }
    }
    return n;
}

/* the ends before the moves: a harmony made, the pieces to take taken (1 and the winner) */
static int decided(const rbpos *b, int *winner)
{
    const rhmpos *p = &b->p;
    int goal = rhm_goal(b->variant);
    if (p->proper) { *winner = !p->side; return 1; }
    for (int k = 0; k < 2; k++)
        if (goal && p->taken[k] >= goal) { *winner = k; return 1; }
    return 0;
}

/* rhm_status: 0 playing, else 1 and the winner (-1: a draw) */
static int ended(const rbpos *b, int *winner)
{
    if (decided(b, winner)) return 1;
    if (!rhb_gen(&b->p, NULL)) { *winner = !b->p.side; return 1; }
    if (b->p.quiet >= RHM_QUIET) { *winner = -1; return 1; }
    return 0;
}

static int a_moves(const void *st, int *out)
{
    const rbpos *b = st;
    int w;
    return decided(b, &w) || b->p.quiet >= RHM_QUIET ? 0 : rhb_gen(&b->p, out);
}

static void a_make(void *st, int m) { make(st, m / RHM_N, m % RHM_N); }
static int a_side(const void *st) { return ((const rbpos *)st)->p.side; }

static int a_over(const void *st, int *score)
{
    const rbpos *b = st;
    int w;
    if (!ended(b, &w)) return 0;
    if (w >= 0) *score = w == b->p.side ? ai_win(0) : -ai_win(0);
    else *score = b->p.side == b->root ? -DRAW : DRAW;
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const rbpos *b = st;
    uint64_t h = (uint64_t)b->p.side << 16 | (uint64_t)b->p.quiet << 8 | b->variant, w;
    for (int i = 0; i < RHM_N; i += 8) {
        memcpy(&w, b->p.sq + i, 8);
        h = ai_mix(h, w);
    }
    return h;
}

/* the middle value that makes lo, b, hi a progression (arithmetic, geometric or harmonic): 1 if one of c's pieces
 * (off the line's ends) has it */
static int middle_exists(const int *vals, int nv, int lo, int hi)
{
    for (int i = 0; i < nv; i++) {
        int b = vals[i];
        if (b > lo && b < hi && (2 * b == lo + hi || b * b == lo * hi || b * (lo + hi) == 2 * lo * hi)) return 1;
    }
    return 0;
}

/* for the proper victory: c's pieces in the enemy's half, and the pairs there on a line, evenly placed for a middle
 * square that is empty or theirs, that a piece of theirs could make a progression with */
static int harmony_hope(const rhmpos *p, int c)
{
    int half[RHM_PIECES], nh = 0, vals[RHM_PIECES], nv = 0, pairs = 0;
    for (int s = 0; s < RHM_N; s++) {
        if (owner(p, s) != c) continue;
        vals[nv++] = piece(p, s)->value;
        int y = s / RHM_W;
        if (c == 0 ? y >= RHM_H / 2 : y < RHM_H / 2) half[nh++] = s;
    }
    for (int i = 0; i < nh && pairs < 6; i++)
        for (int j = i + 1; j < nh && pairs < 6; j++) {
            int a = half[i], z = half[j], dx = z % RHM_W - a % RHM_W, dy = z / RHM_W - a / RHM_W;
            if ((dx & 1) || (dy & 1) || !(dx == 0 || dy == 0 || dx == dy || dx == -dy)) continue;
            int mid = a + dx / 2 + dy / 2 * RHM_W;
            if (p->sq[mid] && owner(p, mid) != c) continue;
            int va = piece(p, a)->value, vz = piece(p, z)->value;
            pairs += middle_exists(vals, nv, va < vz ? va : vz, va < vz ? vz : va);
        }
    return 30 * nh + 70 * pairs;
}

/* the side to move's view; 100 a piece taken */
static int a_eval(const void *st)
{
    const rbpos *b = st;
    const rhmpos *p = &b->p;
    int me = p->side, v = 100 * (p->taken[me] - p->taken[!me]), adv[2] = {0, 0}, thr[2] = {0, 0}, hem[2] = {0, 0};
    for (int s = 0; s < RHM_N; s++) {           /* the ranks gone forward; the pieces each player attacks, hems in */
        if (!p->sq[s]) continue;
        int c = piece(p, s)->owner, x = s % RHM_W, y = s / RHM_W, shut = 0;
        adv[c] += c == 0 ? y : RHM_H - 1 - y;
        if (taken(p, !c, s)) { thr[!c]++; continue; }
        for (int d = 4; d < 8; d++) {           /* the sides of a siege already closed */
            int nx = x + RAY[d][0], ny = y + RAY[d][1];
            shut += !inside(nx, ny) || owner(p, RHM_SQ(nx, ny)) == !c;
        }
        hem[!c] += shut == 3 ? 3 : shut == 2;
    }
    /* the player to move takes what it attacks with any move; of the other's attacks it parries one at most */
    v += 80 * thr[me] - (thr[!me] ? 20 + 60 * (thr[!me] - 1) : 0) + 6 * (adv[me] - adv[!me]);
    v += 12 * (hem[me] - hem[!me]);
    if (b->variant == RHM_PROPER) v += harmony_hope(p, me) - harmony_hope(p, !me);
    /* the bot hunts: each piece of the other's this far from its nearest piece (the other player, in its view, does
     * not mind) */
    int hunt = 0, mine[RHM_PIECES], nm = 0, root = b->root;
    for (int s = 0; s < RHM_N; s++)
        if (owner(p, s) == root) mine[nm++] = s;
    for (int s = 0; s < RHM_N && nm; s++) {
        if (owner(p, s) != !root) continue;
        int near = 99;
        for (int i = 0; i < nm; i++) {
            int dx = abs(mine[i] % RHM_W - s % RHM_W), dy = abs(mine[i] / RHM_W - s / RHM_W);
            int d = dx > dy ? dx : dy;
            if (d < near) near = d;
        }
        hunt += near;
    }
    v += me == root ? -6 * hunt : 6 * hunt;
    if (p->quiet > 40) {                        /* the draw by quiet moves nearing: the bot presses on */
        int press = DRAW * (p->quiet - 40) / (RHM_QUIET - 40);
        v += me == b->root ? -press : press;
    }
    return v;
}

static int a_order(const void *st, int m)       /* forward first */
{
    const rbpos *b = st;
    int dy = m % RHM_N / RHM_W - m / RHM_N / RHM_W;
    return b->p.side ? -dy : dy;
}

static const ai_game ADAPTER = {
    .size = sizeof(rbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

/* the move (from * 128 + to) in the position r of a game of `variant`, at a level; -1: none */
int rhb_choose(const rhmpos *r, int variant, int level, uint64_t seed, ai_stats *stats)
{
    rbpos b = {.p = *r, .variant = (uint8_t)variant, .root = r->side};
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(&b)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    o.budget.ms = o.budget.ms * 5 / 6;                 /* the clock is checked every so many nodes: a margin */
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 2; o.budget.nodes = 3000; o.budget.noise = 25; break;
    case AI_MEDIUM: o.budget.depth = 3; o.budget.nodes = 30000; o.budget.noise = 30; break;
    default:        o.budget.nodes = 70000; break;     /* about 0.3 s on a PC */
    }
    return ai_ab(&ADAPTER, &b, &o, stats);
}

static int name(int s, char *out)               /* "c4", "b14": its length */
{
    int r = s / RHM_W + 1, k = 0;
    out[k++] = (char)('a' + s % RHM_W);
    if (r >= 10) out[k++] = '1';
    out[k++] = (char)('0' + r % 10);
    out[k] = 0;
    return k;
}

static int think(int seat, int level, char *move, int cap)
{
    const rhmgame *g = rithmomachia_live();
    rhmpos now = *rhm_pos(g);                   /* the copy, at entry: nothing else is read */
    int variant = g->variant, w;
    rbpos b = {.p = now, .variant = (uint8_t)variant};
    if (g->ply >= RHM_MAX_PLY || ended(&b, &w) || now.side != seat || cap < 7) return 0;
    int m = rhb_choose(&now, variant, level, ai_session_seed(), NULL);
    if (m < 0 || ai_cancelled()) return 0;
    int k = name(m / RHM_N, move);
    name(m % RHM_N, move + k);
    return 1;
}

static const bot rithmomachia_bot = {.think = think, .play = rithmomachia_show};
REGISTER_BOT(rithmomachia, &rithmomachia_bot)
