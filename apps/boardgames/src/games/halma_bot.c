/*
 * Chinese checkers' bot (bots.h), two, three, four and six players. Every marble is weighed by its distance to the
 * far tip of the point it must fill (steps on the star's grid; the point full: 0 1 1 2 2 2 3 3 3 3), and a little
 * more for the marbles furthest behind (so none is left at home).
 *   Two players: alpha-beta (ai.h) on the difference of the two players' distances; the moves that go back more
 *   than one step are left out of the search (all of them when nothing else is left), the wins (the rules' anti-
 *   blocking rule: the point full, one marble of one's own in it at least) seen by the search.
 *   Three or more: a search of the bot's own moves only, two or three ahead (the others' marbles stand still in it):
 *   the best few at each step are looked at further, the plan's first move played; a move that wins at once first.
 * The board's geometry (neighbours, distances, the points) is worked out at each think, in the search's own memory.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation, the marble's hole and the hole it reaches ("c4-e6": the rules find the shortest
 * chain of jumps), played by halma_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/halma.h"

typedef struct {
    int8_t nb[HL_HOLES][6];                     /* the neighbour each way (halma_rules.c's order), -1 none */
    uint8_t dist[HL_PLAYERS][HL_HOLES];         /* steps to the far tip of the player's point to fill */
    uint8_t goal[HL_PLAYERS][HL_MARBLES];       /* the holes of that point */
    int players;
} hgeo;

typedef struct {
    const hgeo *geo;                            /* the search's own geometry (never the game's live data) */
    uint8_t cell[HL_HOLES];                     /* 0 empty, else player + 1 */
    uint8_t side;
    int8_t winner;                              /* -1 none, -2 a draw */
} hbpos;

static int axial_dist(int a, int b)             /* steps between two holes on the star's grid */
{
    int ca = hl_row(a) - 8, aa = (hl_x2(a) - ca) / 2, ba = -aa - ca;
    int cb = hl_row(b) - 8, ab = (hl_x2(b) - cb) / 2, bb = -ab - cb;
    int d1 = abs(aa - ab), d2 = abs(ba - bb), d3 = abs(ca - cb);
    return d1 > d2 ? (d1 > d3 ? d1 : d3) : (d2 > d3 ? d2 : d3);
}

static void build(hgeo *g, int players)
{
    static const int DX[6] = {2, 1, -1, -2, -1, 1}, DY[6] = {0, 1, 1, 0, -1, -1};
    int centre = hl_at(0, 8);
    g->players = players;
    for (int h = 0; h < HL_HOLES; h++)
        for (int d = 0; d < 6; d++) g->nb[h][d] = (int8_t)hl_at(hl_x2(h) + DX[d], hl_row(h) + DY[d]);
    for (int p = 0; p < players; p++) {
        int point = (hl_seat(players, p) + 3) % HL_POINTS, tip = -1, far = -1, k = 0;
        for (int h = 0; h < HL_HOLES; h++)
            if (hl_point(h) == point) {
                g->goal[p][k++] = (uint8_t)h;
                int d = axial_dist(h, centre);
                if (d > far) { far = d; tip = h; }
            }
        for (int h = 0; h < HL_HOLES; h++) g->dist[p][h] = (uint8_t)axial_dist(h, tip);
    }
}

/* the point player p must fill is full with one of p's marbles in it at least */
static int filled(const hbpos *s, int p)
{
    int mine = 0;
    for (int i = 0; i < HL_MARBLES; i++) {
        int c = s->cell[s->geo->goal[p][i]];
        if (!c) return 0;
        mine |= c == p + 1;
    }
    return mine;
}

/* the holes the marble on `from` reaches (a step, or chains of jumps over single marbles); their count */
static int reach(const hbpos *s, int from, uint8_t *to)
{
    const hgeo *g = s->geo;
    uint8_t listed[HL_HOLES] = {0}, landed[HL_HOLES] = {0}, queue[HL_HOLES];
    int n = 0, head = 0, tail = 0;
    listed[from] = landed[from] = 1;
    for (int d = 0; d < 6; d++) {
        int a = g->nb[from][d];
        if (a >= 0 && !s->cell[a]) { listed[a] = 1; to[n++] = (uint8_t)a; }
    }
    queue[tail++] = (uint8_t)from;
    while (head < tail) {
        int a = queue[head++];
        for (int d = 0; d < 6; d++) {
            int over = g->nb[a][d], b = over < 0 ? -1 : g->nb[over][d];
            if (b < 0 || over == from || !s->cell[over] || s->cell[b] || landed[b]) continue;
            landed[b] = 1;
            if (!listed[b]) { listed[b] = 1; to[n++] = (uint8_t)b; }
            queue[tail++] = (uint8_t)b;
        }
    }
    return n;
}

static int can_move(const hbpos *s, int p)
{
    for (int h = 0; h < HL_HOLES; h++) {
        if (s->cell[h] != p + 1) continue;
        for (int d = 0; d < 6; d++) {
            int a = s->geo->nb[h][d], b = a < 0 ? -1 : s->geo->nb[a][d];
            if (a >= 0 && (!s->cell[a] || (b >= 0 && !s->cell[b]))) return 1;
        }
    }
    return 0;
}

/* the move from -> to by the player to move; the win (the mover first, then round the star, as the rules) */
static void play(hbpos *s, int from, int to, int next)
{
    int me = s->side, n = s->geo->players;
    s->cell[to] = s->cell[from];
    s->cell[from] = 0;
    for (int k = 0; k < n && s->winner == -1; k++)
        if (filled(s, (me + k) % n)) s->winner = (int8_t)((me + k) % n);
    s->side = (uint8_t)next;
}

/* how far player p's marbles are from home: 16 a step, and the square of each one's steps (the laggards) */
static int far_of(const hbpos *s, int p)
{
    int v = 0;
    for (int h = 0; h < HL_HOLES; h++)
        if (s->cell[h] == p + 1) {
            int d = s->geo->dist[p][h];
            v += 16 * d + d * d;
        }
    return v;
}

/* ---- two players: the adapter (a move: from * 128 + to) ---------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    const hbpos *s = st;
    if (s->winner != -1) return 0;
    int n = 0, me = s->side;
    const uint8_t *dist = s->geo->dist[me];
    for (int pass = 0; pass < 2 && !n; pass++)  /* the first pass leaves out the long ways back */
        for (int h = 0; h < HL_HOLES; h++) {
            if (s->cell[h] != me + 1) continue;
            uint8_t to[HL_HOLES];
            int k = reach(s, h, to);
            for (int i = 0; i < k && n < AI_MAX_MOVES; i++) {
                if (!pass && dist[to[i]] > dist[h] + 1) continue;
                out[n++] = h * 128 + to[i];
            }
        }
    return n;
}

static void a_make(void *st, int m)
{
    hbpos *s = st;
    play(s, m / 128, m % 128, !s->side);
}

static int a_side(const void *st) { return ((const hbpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const hbpos *s = st;
    if (s->winner >= 0) { *score = s->winner == s->side ? ai_win(0) : -ai_win(0); return 1; }
    if (!can_move(s, s->side)) { *score = 0; return 1; }   /* walled in: the rules call it a draw */
    return 0;
}

static uint64_t a_hash(const void *st)
{
    const hbpos *s = st;
    uint64_t h = s->side, w;
    for (int i = 0; i + 8 <= HL_HOLES; i += 8) {
        memcpy(&w, s->cell + i, 8);
        h = ai_mix(h, w);
    }
    return ai_mix(h, s->cell[HL_HOLES - 1]);
}

static int a_eval(const void *st)               /* 100 a step nearer than the other */
{
    const hbpos *s = st;
    return (far_of(s, !s->side) - far_of(s, s->side)) * 100 / 16;
}

static int a_order(const void *st, int m)       /* the longest way forward first */
{
    const hbpos *s = st;
    const uint8_t *d = s->geo->dist[s->side];
    return 4 * (d[m / 128] - d[m % 128]) + (d[m / 128] > 4);
}

static const ai_game ADAPTER = {
    .size = sizeof(hbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

/* ---- three or more: the bot's own moves, a few ahead --------------------------------------------------------- */
typedef struct { int from, to, score; } plan;

static int by_score(const void *a, const void *b) { return ((const plan *)b)->score - ((const plan *)a)->score; }

/* the bot's moves in s, each scored by the position it makes (a win: far above the rest) */
static int own_moves(const hbpos *s, int me, plan *out)
{
    int n = 0;
    for (int h = 0; h < HL_HOLES; h++) {
        if (s->cell[h] != me + 1) continue;
        uint8_t to[HL_HOLES];
        int k = reach(s, h, to);
        for (int i = 0; i < k && n < AI_MAX_MOVES; i++) {
            hbpos t = *s;
            t.side = (uint8_t)me;
            play(&t, h, to[i], me);
            out[n++] = (plan){h, to[i], t.winner == me ? 1 << 24 : t.winner >= 0 ? -(1 << 24) : -far_of(&t, me)};
        }
    }
    return n;
}

#define WIN (1 << 24)

/* the best score one more own move reaches from s (every move looked at) */
static int best_next(const hbpos *s, int me, plan *m, long *work)
{
    int n = own_moves(s, me, m), best = -far_of(s, me);   /* no move: where it stands */
    *work += n;
    for (int i = 0; i < n; i++)
        if (i == 0 || m[i].score > best) best = m[i].score;
    return best;
}

/* the move: easy, the best by the position it makes, give or take two or three steps; medium, by the best its next
 * move can make too (every pair looked at), give or take a little; hard, the best twelve of those by a third move
 * after the best sixteen second ones. A move that wins at once is played; going straight back where the bot's last
 * move came from costs a step (no rocking to and fro while it waits). */
static int many_choose(const hbpos *s, int level, int back_from, int back_to, ai_rng *rng, int *from, int *to,
                       ai_stats *stats)
{
    int me = s->side;
    long work = 0;
    plan *m1 = malloc(sizeof(plan) * AI_MAX_MOVES * 3), *m2 = m1 + AI_MAX_MOVES, *m3 = m2 + AI_MAX_MOVES;
    int *val = malloc(sizeof(int) * AI_MAX_MOVES);
    if (!m1 || !val) { free(m1); free(val); return 0; }
    int n = own_moves(s, me, m1), bi = 0;
    if (!n) { free(m1); free(val); return 0; }
    qsort(m1, (size_t)n, sizeof *m1, by_score);
    work += n;
    for (int i = 0; i < n; i++) {
        int v = m1[i].score;
        if (v >= WIN) { val[i] = 1 << 28; continue; }
        if (v <= -WIN) { val[i] = -(1 << 28); continue; }
        if (level <= AI_EASY) val[i] = 5 * v + ai_rand_n(rng, 201);     /* a step: 80 */
        else {
            hbpos t = *s;
            play(&t, m1[i].from, m1[i].to, me);
            int v2 = best_next(&t, me, m2, &work);
            val[i] = v2 >= WIN ? (1 << 27) + v : 4 * v2 + v + (level == AI_MEDIUM ? ai_rand_n(rng, 33) : 0);
        }
        if (m1[i].from == back_from && m1[i].to == back_to) val[i] -= 80;
        if (ai_cancelled()) break;
    }
    for (int i = 1; i < n; i++)
        if (val[i] > val[bi]) bi = i;
    if (level >= AI_HARD && val[bi] < (1 << 27) && !ai_cancelled()) {
        int top[12], nt = 0;                    /* the best twelve so far, looked at a move further */
        for (int k = 0; k < 12 && k < n; k++) {
            int b = -1;
            for (int i = 0; i < n; i++) {
                int taken = 0;
                for (int j = 0; j < nt; j++) taken |= top[j] == i;
                if (!taken && (b < 0 || val[i] > val[b])) b = i;
            }
            if (b < 0 || val[b] <= -(1 << 27)) break;
            top[nt++] = b;
        }
        int bv = -(1 << 30);
        for (int k = 0; k < nt && !ai_cancelled(); k++) {
            int i = top[k];
            hbpos t = *s;
            play(&t, m1[i].from, m1[i].to, me);
            int n2 = own_moves(&t, me, m2), best3 = -(1 << 30);
            work += n2;
            qsort(m2, (size_t)n2, sizeof *m2, by_score);
            for (int j = 0; j < n2 && j < 16; j++) {
                if (m2[j].score <= -WIN) continue;
                hbpos u = t;
                play(&u, m2[j].from, m2[j].to, me);
                int v3 = m2[j].score >= WIN ? WIN : best_next(&u, me, m3, &work);
                if (v3 > best3) best3 = v3;
            }
            if (best3 == -(1 << 30)) best3 = -far_of(&t, me);
            int v = 4 * best3 + (val[i] - m1[i].score) / 2 + m1[i].score;   /* the third, the second, the first */
            if (m1[i].from == back_from && m1[i].to == back_to) v -= 160;
            if (v > bv) { bv = v; bi = i; }
        }
    }
    *from = m1[bi].from;
    *to = m1[bi].to;
    if (stats) stats->nodes = work;
    free(m1);
    free(val);
    return 1;
}

/* the test's check: the holes the marble on `from` reaches in the rules' game g, as the bot finds them */
int hlb_targets(const hl_game *g, int from, int *to)
{
    hgeo *geo = malloc(sizeof *geo);
    if (!geo) return -1;
    build(geo, g->players);
    hbpos s = {.geo = geo, .side = (uint8_t)g->turn, .winner = -1};
    memcpy(s.cell, g->cell, HL_HOLES);
    uint8_t t[HL_HOLES];
    int n = s.cell[from] == g->turn + 1 ? reach(&s, from, t) : 0;
    for (int i = 0; i < n; i++) to[i] = t[i];
    free(geo);
    return n;
}

/* ---- the choice ---------------------------------------------------------------------------------------------- */
/* the move for the player to move in the rules' game g, at a level (from, to: holes); 0: none */
int hlb_choose(const hl_game *g, int level, uint64_t seed, int *from, int *to, ai_stats *stats)
{
    double t0 = ai_now_ms();
    hgeo *geo = malloc(sizeof *geo);
    if (!geo) return 0;
    build(geo, g->players);
    hbpos s = {.geo = geo, .side = (uint8_t)g->turn, .winner = -1};
    memcpy(s.cell, g->cell, HL_HOLES);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(&s)));
    if (stats) memset(stats, 0, sizeof *stats);
    int ok = 0;
    if (g->players == 2) {
        ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
        o.budget.ms = o.budget.ms * 5 / 6;             /* the clock is checked every so many nodes: a margin */
        switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
        case AI_EASY:   o.budget.depth = 1; o.budget.nodes = 3000; o.budget.noise = 250; break;
        case AI_MEDIUM: o.budget.depth = 2; o.budget.nodes = 20000; o.budget.noise = 60; break;
        default:        o.budget.nodes = 600000; break;    /* about 0.3 s on a PC */
        }
        int m = ai_ab(&ADAPTER, &s, &o, stats);
        if (m >= 0) { *from = m / 128; *to = m % 128; ok = 1; }
    } else {
        int back_from = -1, back_to = -1;       /* the bot's last move, the other way round */
        for (int i = g->nhist - 1; i >= 0; i--)
            if (g->who[i] == g->turn) {
                back_from = g->hist[i].hole[g->hist[i].n - 1];
                back_to = g->hist[i].hole[0];
                break;
            }
        ok = many_choose(&s, level, back_from, back_to, &rng, from, to, stats);
    }
    if (stats) stats->ms = ai_now_ms() - t0;
    free(geo);
    return ok && !ai_cancelled();
}

static int think(int seat, int level, char *move, int cap)
{
    const hl_game *live = halma_live();
    hl_game *g = malloc(sizeof *g);             /* the copy, at entry (a big struct: its history) */
    if (!g) return 0;
    *g = *live;
    int ok = 0, from, to;
    if (hl_status(g) == HL_PLAY && g->turn == seat && cap >= 8 &&
        hlb_choose(g, level, ai_session_seed(), &from, &to, NULL)) {
        char a[4], b[4];
        hl_name(from, a);
        hl_name(to, b);
        snprintf(move, (size_t)cap, "%s-%s", a, b);
        ok = 1;
    }
    free(g);
    return ok;
}

static const bot halma_bot = {.think = think, .play = halma_show};
REGISTER_BOT(halma, &halma_bot)
