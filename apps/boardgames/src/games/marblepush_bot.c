/*
 * Marble Push's bot (bots.h), every starting layout: alpha-beta (ai.h) with a quiescence over the pushes off the
 * board, on an 11 x 11 grid of the hexagon's rows and diagonals (the holes off the hexagon marked, so a step needs no
 * bounds check); the moves as the rules make them (marblepush_rules.c: one to three marbles in a row, in line with
 * pushes of fewer, or sideways). The evaluation: the marbles pushed off; the centre (each marble's rings from the
 * middle hole); cohesion (neighbours of one's own colour); marbles on the edge with an enemy next to them (in danger).
 * The rules' draws by a repeated position are left to the rules (the search does not see them); 100 moves without a
 * marble off is seen.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("c3c5:ne", "a1:e": the marbles' two ends, the direction), played by
 * marblepush_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/marblepush.h"

#define G 11                                    /* the grid: (row + 1) * 11 + diagonal + 1 */
#define AT(r, d) (((r) + 1) * G + (d) + 1)
enum { EMPTY, P0, P1, OFF };                    /* a hole: empty, a player's marble (player + 1), off the board */

static const int STEP[6] = {1, G + 1, G, -1, -G - 1, -G};   /* e ne nw w sw se (marblepush.h) */

typedef struct {
    uint8_t c[G * G];
    uint8_t side, lost[2];
    uint16_t quiet;
} mbpos;

static int valid(int r, int d) { return r >= 0 && r < 9 && d >= 0 && d < 9 && d - r <= 4 && r - d <= 4; }

static void from_rules(const mppos *q, mbpos *p)
{
    memset(p, 0, sizeof *p);
    for (int r = -1; r <= 9; r++)
        for (int d = -1; d <= 9; d++) p->c[AT(r, d)] = valid(r, d) ? EMPTY : OFF;
    for (int h = 0; h < MP_HOLES; h++) p->c[AT(mp_row(h), mp_diag(h))] = q->cell[h];
    p->side = q->side;
    p->lost[0] = q->lost[0];
    p->lost[1] = q->lost[1];
    p->quiet = q->quiet;
}

/* ---- the moves: ((first * 3 + axis) * 3 + marbles - 1) * 6 + direction ------------------------------------------- */
static int code(int first, int ax, int n, int dir) { return ((first * 3 + ax) * 3 + n - 1) * 6 + dir; }

/* the move of the n marbles from `first` along axis ax, in direction dir: legal? (*off: it pushes one off) */
static int try_move(const mbpos *p, int first, int ax, int n, int dir, int *off)
{
    int them = P0 + !p->side;
    *off = 0;
    if (n > 1 && (dir == ax || dir == ax + 3)) {   /* in line: from the front marble on */
        int front = dir == ax ? first + (n - 1) * STEP[ax] : first, x = front + STEP[dir], k = 0;
        while (p->c[x] == them && k < n) { k++; x += STEP[dir]; }
        if (k >= n) return 0;                   /* not outnumbered */
        if (p->c[x] == OFF) {
            if (!k) return 0;                   /* one's own marble off the board */
            *off = 1;
            return 1;
        }
        return p->c[x] == EMPTY;
    }
    for (int i = 0; i < n; i++)                 /* sideways (one marble: any way) */
        if (p->c[first + i * STEP[ax] + STEP[dir]] != EMPTY) return 0;
    return 1;
}

/* the moves (only those pushing a marble off: offs); their count (AI_MAX_MOVES at most) */
static int gen(const mbpos *p, int *out, int offs)
{
    int n = 0, me = P0 + p->side, off;
    for (int h = G; h < G * G - G && n < AI_MAX_MOVES - 6; h++) {
        if (p->c[h] != me) continue;
        if (!offs)
            for (int d = 0; d < 6; d++)
                if (try_move(p, h, 0, 1, d, &off)) out[n++] = code(h, 0, 1, d);
        for (int ax = 0; ax < 3; ax++)
            for (int k = 2; k <= 3 && n < AI_MAX_MOVES - 6; k++) {
                if (p->c[h + (k - 1) * STEP[ax]] != me) break;
                for (int d = 0; d < 6; d++)
                    if ((!offs || d == ax || d == ax + 3) && try_move(p, h, ax, k, d, &off) && (!offs || off))
                        out[n++] = code(h, ax, k, d);
            }
    }
    return n;
}

static void make(mbpos *p, int m)
{
    int dir = m % 6, n = m / 6 % 3 + 1, ax = m / 18 % 3, first = m / 54, me = P0 + p->side, off = 0;
    if (n > 1 && (dir == ax || dir == ax + 3)) {
        int front = dir == ax ? first + (n - 1) * STEP[ax] : first, back = dir == ax ? first : first + (n - 1) *
                    STEP[ax], x = front + STEP[dir], k = 0;
        while (p->c[x] == P0 + !p->side) { k++; x += STEP[dir]; }
        if (k) {                                /* the pushed ones move up one: the last onto x, or off */
            if (p->c[x] == OFF) { p->lost[!p->side]++; off = 1; }
            else p->c[x] = (uint8_t)(P0 + !p->side);
        }
        p->c[front + STEP[dir]] = (uint8_t)me;
        p->c[back] = EMPTY;
    } else {
        for (int i = 0; i < n; i++) p->c[first + i * STEP[ax]] = EMPTY;
        for (int i = 0; i < n; i++) p->c[first + i * STEP[ax] + STEP[dir]] = (uint8_t)me;
    }
    p->quiet = off ? 0 : (uint16_t)(p->quiet + 1);
    p->side ^= 1;
}

/* the test's check: the bot's moves in the rules' position are the rules' (as many, each making the same position) */
int mpb_agrees(const mppos *q)
{
    mbpos p, a, b;
    from_rules(q, &p);
    mpmove l[MP_MAX_MOVES];
    int n = mp_legal(q, l), mv[AI_MAX_MOVES], k = gen(&p, mv, 0);
    if (n != k) return 0;
    for (int i = 0; i < n; i++) {
        mppos r = *q;
        mp_make(&r, &l[i]);
        from_rules(&r, &b);
        int found = 0;
        for (int j = 0; j < k && !found; j++) {
            a = p;
            make(&a, mv[j]);
            found = !memcmp(&a, &b, sizeof a);
        }
        if (!found) return 0;
    }
    return 1;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int decided(const mbpos *p) { return p->lost[0] >= MP_WIN || p->lost[1] >= MP_WIN || p->quiet >= MP_QUIET; }

static int a_moves(const void *st, int *out) { return decided(st) ? 0 : gen(st, out, 0); }
static int a_noisy(const void *st, int *out) { return decided(st) ? 0 : gen(st, out, 1); }
static void a_make(void *st, int m) { make(st, m); }
static int a_side(const void *st) { return ((const mbpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const mbpos *p = st;
    int out[AI_MAX_MOVES], me = p->side;
    if (p->lost[!me] >= MP_WIN) *score = ai_win(0);
    else if (p->lost[me] >= MP_WIN) *score = -ai_win(0);
    else if (p->quiet >= MP_QUIET || !gen(p, out, 0)) *score = 0;
    else return 0;
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const mbpos *p = st;
    uint64_t h = (uint64_t)p->side | (uint64_t)p->lost[0] << 8 | (uint64_t)p->lost[1] << 16, w;
    for (int i = G; i < G * G - G; i += 8) {
        memcpy(&w, p->c + i, 8);
        h = ai_mix(h, w);
    }
    return h;
}

static int ring(int i)                          /* rings from the middle hole (e5): 0..4 */
{
    int dr = i / G - 5, dd = i % G - 5, de = dr - dd;
    dr = dr < 0 ? -dr : dr;
    dd = dd < 0 ? -dd : dd;
    de = de < 0 ? -de : de;
    return dr > dd ? (dr > de ? dr : de) : (dd > de ? dd : de);
}

/* the side to move's view; 100 a ring nearer the middle for one marble */
static int a_eval(const void *st)
{
    const mbpos *p = st;
    int me = p->side, centre[2] = {0, 0}, bond[2] = {0, 0}, danger[2] = {0, 0};
    for (int i = G; i < G * G - G; i++) {
        int c = p->c[i];
        if (c != P0 && c != P1) continue;
        int who = c - P0, r = ring(i), foes = 0, edge = 0;
        centre[who] += 4 - r;
        for (int d = 0; d < 6; d++) {
            int x = p->c[i + STEP[d]];
            if (x == c && d < 3) bond[who]++;
            foes += x == P0 + !who;
            edge |= x == OFF;
        }
        if (edge && foes) danger[who] += foes;
    }
    int v = 1500 * (p->lost[!me] - p->lost[me]);
    v += 60 * (centre[me] - centre[!me]) + 25 * (bond[me] - bond[!me]) - 45 * (danger[me] - danger[!me]);
    return v;
}

static int a_order(const void *st, int m)       /* the pushes off, the pushes, the threes and twos */
{
    const mbpos *p = st;
    int dir = m % 6, n = m / 6 % 3 + 1, ax = m / 18 % 3, first = m / 54;
    if (n > 1 && (dir == ax || dir == ax + 3)) {
        int front = dir == ax ? first + (n - 1) * STEP[ax] : first, x = front + STEP[dir], k = 0;
        while (p->c[x] == P0 + !p->side) { k++; x += STEP[dir]; }
        if (k) return p->c[x] == OFF ? 60 : 20 + k;
    }
    int to = first + STEP[dir];
    return n * 2 + (ring(first) - ring(to)) * 3;
}

static const ai_game ADAPTER = {
    .size = sizeof(mbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};

/* the move in the rules' position q, at a level: its marbles' two ends (rules' holes) and its direction; 0: none */
int mpb_choose(const mppos *q, int level, uint64_t seed, int *a, int *b, int *dir, ai_stats *stats)
{
    mbpos p;
    from_rules(q, &p);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(&p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    o.budget.ms = o.budget.ms * 5 / 6;                 /* the clock is checked every so many nodes: a margin */
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 1; o.budget.nodes = 3000; o.budget.noise = 150; break;
    case AI_MEDIUM: o.budget.depth = 3; o.budget.nodes = 40000; o.budget.noise = 40; break;
    default:        o.budget.nodes = 250000; break;    /* about 0.3 s on a PC */
    }
    int m = ai_ab(&ADAPTER, &p, &o, stats);
    if (m < 0) return 0;
    int n = m / 6 % 3 + 1, ax = m / 18 % 3, first = m / 54, last = first + (n - 1) * STEP[ax];
    *dir = m % 6;
    *a = mp_at(first / G - 1, first % G - 1);
    *b = mp_at(last / G - 1, last % G - 1);
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    const mpgame *g = marblepush_live();
    mppos now = *mp_pos(g);                     /* the copy, at entry: nothing else is read */
    static const char *const DIR[6] = {"e", "ne", "nw", "w", "sw", "se"};
    if (g->ply >= MP_MAX_PLY || now.side != seat || cap < 9) return 0;
    if (now.lost[0] >= MP_WIN || now.lost[1] >= MP_WIN || now.quiet >= MP_QUIET) return 0;
    int a, b, dir;
    if (!mpb_choose(&now, level, ai_session_seed(), &a, &b, &dir, NULL) || ai_cancelled()) return 0;
    char x[3], y[3];
    mp_name(a, x);
    mp_name(b, y);
    snprintf(move, (size_t)cap, "%s%s:%s", x, a == b ? "" : y, DIR[dir]);
    return 1;
}

static const bot marblepush_bot = {.think = think, .play = marblepush_show};
REGISTER_BOT(marblepush, &marblepush_bot)
