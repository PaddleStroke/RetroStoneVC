/*
 * Dara's bot (bots.h): alpha-beta (ai.h) over the rules' own position (dar_legal, dar_make: pure), the placing
 * included (a player who cannot place passes inside dar_make: the search follows side()). A row's capture is a move
 * of its own by the same player. The evaluation: the pieces (on the board and in hand) far above all, then the rows
 * of three each side could make with one step (a threat; two or more: the other cannot stop them all), the pieces
 * standing two in a row with room for a third, and the moves each side has. A player left with two pieces, or with
 * no move, has lost; a hundred moves without a capture draw (the search counts them from the position's own count).
 * Easy looks two plies ahead with a lot of noise; medium four with a little; hard as deep as its budget allows.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation, one action at a time: "c3" places, "c3c4" moves, "xd3" takes; played by
 * dara_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/dara.h"

static int enc(darmove m) { return m.kind << 10 | m.from << 5 | m.to; }
static darmove dec(int v) { return (darmove){(uint8_t)(v >> 10), (uint8_t)(v >> 5 & 31), (uint8_t)(v & 31)}; }

static int count(const darpos *p, int c) { return dar_count(p, c) + p->hand[c]; }

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int lost(const darpos *p)                /* moving: the side to move has two pieces, or no move */
{
    if (p->phase != DAR_MOVE) return 0;
    if (dar_count(p, p->side) < 3) return 1;
    darmove m[DAR_MAX_MOVES];
    return !dar_legal(p, m);
}

static int a_moves(const void *st, int *out)
{
    const darpos *p = st;
    darmove m[DAR_MAX_MOVES];
    if (lost(p) || (p->phase == DAR_MOVE && p->quiet >= DAR_QUIET)) return 0;
    int n = dar_legal(p, m);
    for (int i = 0; i < n; i++) out[i] = enc(m[i]);
    return n;
}

static void a_make(void *st, int v) { dar_make(st, dec(v)); }
static int a_side(const void *st) { return ((const darpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const darpos *p = st;
    if (p->phase != DAR_MOVE) return 0;
    if (lost(p)) { *score = -ai_win(dar_count(p, !p->side)); return 1; }
    if (p->quiet >= DAR_QUIET) { *score = 0; return 1; }
    return 0;
}

static uint64_t a_hash(const darpos *p)
{
    uint64_t a = 0;
    for (int h = 0; h < DAR_N; h++) a = a << 2 | p->c[h];
    return ai_mix(a, (uint64_t)p->side | (uint64_t)p->phase << 1 | (uint64_t)p->hand[0] << 3 |
                     (uint64_t)p->hand[1] << 8);
}
static uint64_t a_hash_v(const void *st) { return a_hash(st); }

/* the length of who's run through hole h along (dx, dy), h counted as who's and `gone` as empty */
static int run_len(const darpos *p, int h, int gone, int who, int dx, int dy)
{
    int n = 1, x0 = h % DAR_W, y0 = h / DAR_W;
    for (int s = -1; s <= 1; s += 2)
        for (int x = x0 + s * dx, y = y0 + s * dy; x >= 0 && y >= 0 && x < DAR_W && y < DAR_H;
             x += s * dx, y += s * dy) {
            int q = DAR_AT(x, y);
            if (q == gone || p->c[q] != who) break;
            n++;
        }
    return n;
}

static const int8_t DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};

/* player c's one-step rows of three: the empty holes a piece of c's next to it can step into to make one; and the
 * pairs (two of c's side by side in a line, an empty hole at one end at least) */
static void threats(const darpos *p, int c, int *rows, int *pairs)
{
    int who = c + 1;
    *rows = *pairs = 0;
    for (int h = 0; h < DAR_N; h++) {
        if (p->c[h]) {
            if (p->c[h] != who) continue;
            int x = h % DAR_W, y = h / DAR_W;   /* a pair: h and its right (or upper) neighbour */
            if (x + 1 < DAR_W && p->c[h + 1] == who &&
                ((x > 0 && !p->c[h - 1]) || (x + 2 < DAR_W && !p->c[h + 2])))
                (*pairs)++;
            if (y + 1 < DAR_H && p->c[h + DAR_W] == who &&
                ((y > 0 && !p->c[h - DAR_W]) || (y + 2 < DAR_H && !p->c[h + 2 * DAR_W])))
                (*pairs)++;
            continue;
        }
        int x = h % DAR_W, y = h / DAR_W, made = 0;
        for (int d = 0; d < 4 && !made; d++) {  /* a piece of c's that steps into h */
            int fx = x + DX[d], fy = y + DY[d];
            if (fx < 0 || fy < 0 || fx >= DAR_W || fy >= DAR_H || p->c[DAR_AT(fx, fy)] != who) continue;
            int f = DAR_AT(fx, fy);
            made = run_len(p, h, f, who, 1, 0) == 3 || run_len(p, h, f, who, 0, 1) == 3;
        }
        *rows += made;
    }
}

/* the side to move's view; 100 a piece */
static int a_eval(const void *st)
{
    const darpos *p = st;
    int me = p->side, v = 100 * (count(p, me) - count(p, !me));
    if (p->phase == DAR_TAKE) return v + 90;
    int r[2], pr[2];
    threats(p, 0, &r[0], &pr[0]);
    threats(p, 1, &r[1], &pr[1]);
    if (p->phase == DAR_MOVE) {
        v += r[me] ? 60 : 0;                    /* a row to make now: about a piece (the other may take first) */
        v -= r[!me] > 1 ? 70 : r[!me] ? 35 : 0; /* theirs: one can be stopped, two hardly */
        v += 10 * (r[me] - r[!me]);
        darmove m[DAR_MAX_MOVES];
        darpos q = *p;
        int mine = dar_legal(&q, m);
        q.side ^= 1;
        int theirs = dar_legal(&q, m);
        v += 3 * (mine - theirs);
    } else {
        v += 14 * (r[me] - r[!me]);
    }
    v += 8 * (pr[me] - pr[!me]);
    return v;
}

static int a_order(const void *st, int v)       /* the steps that make a row first; the captures of a pair */
{
    const darpos *p = st;
    darmove m = dec(v);
    if (m.kind == DAR_MOVE) {
        int who = p->c[m.from];
        return (run_len(p, m.to, m.from, who, 1, 0) == 3 || run_len(p, m.to, m.from, who, 0, 1) == 3) ? 50 : 0;
    }
    if (m.kind == DAR_TAKE) {
        int r0, p0, r1, p1;
        threats(p, !p->side, &r0, &p0);
        darpos q = *p;
        q.c[m.to] = 0;
        threats(&q, !p->side, &r1, &p1);
        return 10 * (r0 - r1) + 3 * (p0 - p1);
    }
    return 0;
}

static const ai_game ADAPTER = {
    .size = sizeof(darpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash_v, .order = a_order, .unit = 100,
};

/* the action for the position (its side to move), at a level; 0: none */
int drb_choose(const darpos *p, int level, uint64_t seed, darmove *m, ai_stats *stats)
{
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    o.budget.ms = o.budget.ms * 85 / 100;       /* a margin under the level's time (a slow or busy machine) */
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 2; o.budget.nodes = 3000; o.budget.noise = 120; break;
    case AI_MEDIUM: o.budget.depth = 4; o.budget.nodes = 30000; o.budget.noise = 25; break;
    default:        o.budget.nodes = 250000; break;     /* about 0.3 s on a PC */
    }
    int v = ai_ab(&ADAPTER, p, &o, stats);
    if (v < 0) return 0;
    *m = dec(v);
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    darpos p = *dar_pos(dara_live());           /* the copy, at entry (the position only): nothing else is read */
    darmove m, legal[DAR_MAX_MOVES];
    if (p.side != seat || cap < 5 || lost(&p) || (p.phase == DAR_MOVE && p.quiet >= DAR_QUIET) ||
        !dar_legal(&p, legal))
        return 0;
    if (!drb_choose(&p, level, ai_session_seed(), &m, NULL) || ai_cancelled()) return 0;
    if (m.kind == DAR_MOVE)
        snprintf(move, (size_t)cap, "%c%d%c%d", 'a' + m.from % DAR_W, 1 + m.from / DAR_W, 'a' + m.to % DAR_W,
                 1 + m.to / DAR_W);
    else
        snprintf(move, (size_t)cap, "%s%c%d", m.kind == DAR_TAKE ? "x" : "", 'a' + m.to % DAR_W, 1 + m.to / DAR_W);
    return 1;
}

static const bot dara_bot = {.think = think, .play = dara_show};
REGISTER_BOT(dara, &dara_bot)
