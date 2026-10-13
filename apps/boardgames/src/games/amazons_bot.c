/*
 * The Game of the Amazons' bot (bots.h): alpha-beta (ai.h) over the turn's two steps as moves of their own (the
 * amazon's move, then its arrow, by the same player: the search flips no sign between them, and a turn's thousands of
 * combinations never sit in one list). The evaluation is territory by queen moves: each empty square goes to the
 * player whose amazons reach it in fewer queen moves (a tie: a little to the player to move), plus the amazons' room
 * (the squares around them and along their lines; an amazon walled in costs dear) while the board is open. The turn is
 * found in two searches: the amazon's move (its arrow searched below it), then the arrow from where it went.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation, a whole turn "d1-d7/g7" (or "/g7", the arrow alone, when the amazon has already
 * moved), played by amazons_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/amazons.h"

/* a 12 x 12 board: the 10 x 10 inside a ring of blocked squares, so a slide stops without a bounds check */
#define W 12
#define IN(x, y) (((y) + 1) * W + (x) + 1)
enum { EMPTY, BLOCK, QUEEN0 };                  /* an amazon: QUEEN0 + player */

static const int DIR[8] = {1, -1, W, -W, W + 1, W - 1, -W + 1, -W - 1};

typedef struct {
    uint8_t b[W * W];
    uint8_t q[2][4];                            /* each player's amazons' squares */
    uint8_t side, stage, at, arrows;            /* at: the amazon that moved (stage AM_SHOOT); arrows: burnt so far */
} abpos;

static void from_rules(const ampos *r, abpos *p)
{
    memset(p, 0, sizeof *p);
    memset(p->b, BLOCK, sizeof p->b);
    int n[2] = {0, 0};
    for (int s = 0; s < AM_SQUARES; s++) {
        int i = IN(s % AM_N, s / AM_N), v = r->sq[s];
        p->b[i] = v == AM_EMPTY ? EMPTY : v == AM_ARROW ? BLOCK : (uint8_t)(QUEEN0 + v - AM_AMAZON);
        if (v == AM_ARROW) p->arrows++;
        if (v == AM_AMAZON || v == AM_AMAZON + 1) {
            int c = v - AM_AMAZON;
            if (n[c] < 4) p->q[c][n[c]++] = (uint8_t)i;
        }
    }
    p->side = r->side;
    p->stage = r->stage;
    for (int k = 0; k < 4; k++)
        if (r->stage == AM_SHOOT && p->q[r->side][k] == IN(r->at % AM_N, r->at / AM_N)) p->at = (uint8_t)k;
}

static int rules_sq(int i) { return (i % W - 1) + (i / W - 1) * AM_N; }

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
/* the amazon's move: k * 144 + its square; the arrow: its square */
static int a_moves(const void *st, int *out)
{
    const abpos *p = st;
    int n = 0;
    if (p->stage == AM_SHOOT) {
        int s = p->q[p->side][p->at];
        for (int d = 0; d < 8; d++)
            for (int t = s + DIR[d]; p->b[t] == EMPTY; t += DIR[d]) out[n++] = t;
        return n;
    }
    for (int k = 0; k < 4; k++) {
        int s = p->q[p->side][k];
        for (int d = 0; d < 8; d++)
            for (int t = s + DIR[d]; p->b[t] == EMPTY; t += DIR[d]) out[n++] = k * W * W + t;
    }
    return n;
}

static void a_make(void *st, int m)
{
    abpos *p = st;
    if (p->stage == AM_SHOOT) {
        p->b[m] = BLOCK;
        p->arrows++;
        p->stage = AM_MOVE;
        p->side ^= 1;
        return;
    }
    int k = m / (W * W), to = m % (W * W);
    p->b[p->q[p->side][k]] = EMPTY;
    p->b[to] = (uint8_t)(QUEEN0 + p->side);
    p->q[p->side][k] = (uint8_t)to;
    p->at = (uint8_t)k;
    p->stage = AM_SHOOT;
}

static int a_side(const void *st) { return ((const abpos *)st)->side; }

static int stuck(const abpos *p, int c)         /* no amazon of c's can move */
{
    for (int k = 0; k < 4; k++)
        for (int d = 0; d < 8; d++)
            if (p->b[p->q[c][k] + DIR[d]] == EMPTY) return 0;
    return 1;
}

static int a_over(const void *st, int *score)
{
    const abpos *p = st;
    if (p->stage != AM_MOVE || !stuck(p, p->side)) return 0;
    *score = -ai_win(0);
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const abpos *p = st;
    uint64_t h = (uint64_t)p->side << 8 | (uint64_t)p->stage << 4 | p->at, w;
    for (int i = W; i < W * W - W; i += 8) {
        memcpy(&w, p->b + i, 8);
        h = ai_mix(h, w);
    }
    return h;
}

/* the fewest queen moves from c's amazons to each square (255: never) */
static void queen_dist(const abpos *p, int c, uint8_t *d)
{
    int list[2][W * W], n = 0, level = 0, cur = 0;
    memset(d, 255, W * W);
    for (int k = 0; k < 4; k++) list[0][n++] = p->q[c][k];
    while (n) {
        int m = 0;
        level++;
        for (int i = 0; i < n; i++)
            for (int dd = 0; dd < 8; dd++)
                for (int t = list[cur][i] + DIR[dd]; p->b[t] == EMPTY; t += DIR[dd])
                    if (d[t] > level) {
                        d[t] = (uint8_t)level;
                        list[!cur][m++] = t;
                    }
        cur = !cur;
        n = m;
    }
}

static int room(const abpos *p, int c)          /* the amazons' moves, an amazon walled in counting heavily */
{
    int v = 0;
    for (int k = 0; k < 4; k++) {
        int s = p->q[c][k], m = 0;
        for (int d = 0; d < 8; d++)
            for (int t = s + DIR[d]; p->b[t] == EMPTY; t += DIR[d]) m++;
        v += m > 12 ? 12 + (m - 12) / 3 : m;
        if (!m) v -= 30;
        else if (m < 3) v -= 8;
    }
    return v;
}

/* the side to move's view; 100 an empty square of one's own */
static int a_eval(const void *st)
{
    const abpos *p = st;
    uint8_t d0[W * W], d1[W * W];
    int me = p->side;
    queen_dist(p, me, d0);
    queen_dist(p, !me, d1);
    int terr = 0;
    for (int i = W + 1; i < W * W - W - 1; i++) {
        if (p->b[i] != EMPTY) continue;
        if (d0[i] < d1[i]) terr += 100;
        else if (d1[i] < d0[i]) terr -= 100;
        else if (d0[i] != 255) terr += 15;
    }
    int open = p->arrows < 50 ? 50 - p->arrows : 0;   /* the room weighs while the board is open */
    return terr + (room(p, me) - room(p, !me)) * (8 + open / 2);
}

static int a_order(const void *st, int m)
{
    const abpos *p = st;
    int v = 0;
    if (p->stage == AM_SHOOT) {                 /* an arrow next to the other's amazons, not next to one's own */
        for (int d = 0; d < 8; d++) {
            int x = p->b[m + DIR[d]];
            v += x == QUEEN0 + !p->side ? 12 : x == QUEEN0 + p->side ? -4 : 0;
        }
        return v;
    }
    int to = m % (W * W), from = p->q[p->side][m / (W * W)];
    for (int d = 0; d < 8; d++) {               /* room round the square reached */
        int t = to + DIR[d];
        v += p->b[t] == EMPTY || t == from;
    }
    return v;
}

static const ai_game ADAPTER = {
    .size = sizeof(abpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

/* the turn in the game's position (its player to move), at a level: the amazon's move (from, to) and the arrow
 * (rules' squares; from -1 when the amazon had already moved); 0: none */
int amb_choose(const amgame *g, int level, uint64_t seed, int *from, int *to, int *arrow, ai_stats *stats)
{
    abpos p;
    from_rules(am_pos(g), &p);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(&p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    o.budget.ms = o.budget.ms * 5 / 6;                 /* the clock is checked every so many nodes: a margin */
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 2; o.budget.nodes = 6000; o.budget.noise = 350; break;
    case AI_MEDIUM: o.budget.depth = 2; o.budget.nodes = 8000; o.budget.noise = 60; break;
    default:        o.budget.nodes = 45000; break;     /* about 0.3 s on a PC, both searches */
    }
    *from = *to = *arrow = -1;
    if (stats) memset(stats, 0, sizeof *stats);
    if (p.stage == AM_MOVE) {
        if (stuck(&p, p.side)) return 0;
        ai_opts o1 = o;
        o1.budget.nodes = o.budget.nodes * 3 / 4;
        o1.budget.ms = o.budget.ms * 3 / 4;     /* the clock's safety net shared too */
        double t0 = ai_now_ms();
        int m = ai_ab(&ADAPTER, &p, &o1, stats);
        if (m < 0 || ai_cancelled()) return 0;
        *from = rules_sq(p.q[p.side][m / (W * W)]);
        *to = rules_sq(m % (W * W));
        a_make(&p, m);
        o.budget.nodes /= 4;
        int left = o.budget.ms - (int)(ai_now_ms() - t0);
        o.budget.ms = left > o.budget.ms / 5 ? left : o.budget.ms / 5;
        if (o.budget.depth) o.budget.depth--;
    }
    ai_stats s2;
    int a = ai_ab(&ADAPTER, &p, &o, &s2);
    if (stats) {
        stats->nodes += s2.nodes;
        stats->ms += s2.ms;
    }
    if (a < 0 || ai_cancelled()) return 0;
    *arrow = rules_sq(a);
    return 1;
}

static void name(int s, char *out)              /* "d1", "j10" */
{
    int r = s / AM_N + 1;
    out[0] = (char)('a' + s % AM_N);
    out[1] = (char)(r >= 10 ? '1' : '0' + r);
    out[2] = (char)(r >= 10 ? '0' + r - 10 : 0);
    out[3] = 0;
}

static int think(int seat, int level, char *move, int cap)
{
    const amgame *g = amazons_live();           /* read once, at entry (the main thread leaves it alone meanwhile) */
    const ampos *now = am_pos(g);
    if (am_winner(g) >= 0 || now->side != seat || cap < 12) return 0;
    int from, to, arrow;
    if (!amb_choose(g, level, ai_session_seed(), &from, &to, &arrow, NULL)) return 0;
    char a[4], b[4], c[4];
    name(arrow, c);
    if (from < 0) snprintf(move, (size_t)cap, "/%s", c);
    else {
        name(from, a);
        name(to, b);
        snprintf(move, (size_t)cap, "%s-%s/%s", a, b, c);
    }
    return 1;
}

static const bot amazons_bot = {.think = think, .play = amazons_show};
REGISTER_BOT(amazons, &amazons_bot)
