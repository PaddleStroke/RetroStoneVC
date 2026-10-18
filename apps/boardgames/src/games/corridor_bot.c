/*
 * Corridor's bot (bots.h): alpha-beta (ai.h) over the pawn's steps and a choice of walls, on the difference of the
 * shortest ways home. Two players: the usual negamax. Four: "paranoid", the bot against the three others together
 * (side() 0 the bot, 1 everyone else; the kit flips the sign only when the side changes).
 * The walls searched are those that cut an edge of an opponent's shortest way (the first steps of it for the levels
 * below hard), the only ones that lengthen it at once; a wall is checked against the path rule only for the players
 * whose way it cuts (a wall off a pawn's shortest way never shuts it in). The evaluation: the opponents' best
 * distance against the bot's, the walls in hand, a little for the move.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("e2", "e3h", "-"), played by corridor_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/corridor.h"

typedef struct {
    crpos p;
    uint8_t me;                                 /* the bot's seat */
    uint8_t reach;                              /* the opponents' path edges searched for walls (0: all) */
    int16_t wv;                                 /* a wall in hand's worth (a step: 100) */
} cbpos;

/* ---- the squares as an 81-bit set (two words): a flood for the distances, far quicker than the rules' search -- */
typedef struct { uint64_t lo, hi; } bb;
typedef struct { bb dir[4]; } opens;            /* the squares whose side is open, each way (CR_UP...) */

static inline bb b_and(bb a, bb b) { return (bb){a.lo & b.lo, a.hi & b.hi}; }
static inline bb b_or(bb a, bb b) { return (bb){a.lo | b.lo, a.hi | b.hi}; }
static inline int b_any(bb a) { return (a.lo | a.hi) != 0; }
static inline int b_eq(bb a, bb b) { return a.lo == b.lo && a.hi == b.hi; }
static inline bb b_shl(bb a, int n) { return (bb){a.lo << n, a.hi << n | a.lo >> (64 - n)}; }
static inline bb b_shr(bb a, int n) { return (bb){a.lo >> n | a.hi << (64 - n), a.hi >> n}; }
static inline bb b_bit(int s) { return s < 64 ? (bb){1ull << s, 0} : (bb){0, 1ull << (s - 64)}; }
static inline void b_clear(bb *a, int s)
{
    if (s < 64) a->lo &= ~(1ull << s);
    else a->hi &= ~(1ull << (s - 64));
}

static bb rank_of(int y) { bb r = {0, 0}; for (int x = 0; x < CR_N; x++) r = b_or(r, b_bit(x + CR_N * y)); return r; }
static bb file_of(int x) { bb r = {0, 0}; for (int y = 0; y < CR_N; y++) r = b_or(r, b_bit(x + CR_N * y)); return r; }

static void wall_off(opens *o, int slot, int orient)
{
    int sx = slot % 8, sy = slot / 8, a = sx + CR_N * sy;
    if (orient == CR_H) {
        b_clear(&o->dir[CR_UP], a), b_clear(&o->dir[CR_UP], a + 1);
        b_clear(&o->dir[CR_DOWN], a + CR_N), b_clear(&o->dir[CR_DOWN], a + CR_N + 1);
    } else {
        b_clear(&o->dir[CR_RIGHT], a), b_clear(&o->dir[CR_RIGHT], a + CR_N);
        b_clear(&o->dir[CR_LEFT], a + 1), b_clear(&o->dir[CR_LEFT], a + CR_N + 1);
    }
}

static void opens_of(const crpos *p, opens *o)
{
    bb all = {~0ull, (1ull << (CR_CELLS - 64)) - 1};
    bb top = rank_of(CR_N - 1), bottom = rank_of(0), right = file_of(CR_N - 1), left = file_of(0);
    o->dir[CR_UP] = (bb){all.lo & ~top.lo, all.hi & ~top.hi};
    o->dir[CR_DOWN] = (bb){all.lo & ~bottom.lo, all.hi & ~bottom.hi};
    o->dir[CR_RIGHT] = (bb){all.lo & ~right.lo, all.hi & ~right.hi};
    o->dir[CR_LEFT] = (bb){all.lo & ~left.lo, all.hi & ~left.hi};
    for (int s = 0; s < CR_SLOTS; s++)
        if (p->wall[s]) wall_off(o, s, p->wall[s]);
}

static bb goal_of(const crpos *p, int player)
{
    int side = p->players == 2 ? (player ? 2 : 0) : player;
    return side == 0 ? rank_of(CR_N - 1) : side == 1 ? file_of(CR_N - 1) : side == 2 ? rank_of(0) : file_of(0);
}

/* the fewest steps from sq to the goal, walls only (CR_FAR: none) */
static int flood(const opens *o, int sq, bb goal)
{
    bb r = b_bit(sq);
    for (int d = 0;; d++) {
        if (b_any(b_and(r, goal))) return d;
        bb n = b_or(r, b_or(b_or(b_shl(b_and(r, o->dir[CR_UP]), CR_N), b_shr(b_and(r, o->dir[CR_DOWN]), CR_N)),
                            b_or(b_shl(b_and(r, o->dir[CR_RIGHT]), 1), b_shr(b_and(r, o->dir[CR_LEFT]), 1))));
        if (b_eq(n, r)) return CR_FAR;
        r = n;
    }
}

static int dist(const crpos *p, const opens *o, int player) { return flood(o, p->pawn[player], goal_of(p, player)); }

/* the two slots (and orientation) whose wall would cut the edge from a to b (next squares); out: up to 2 */
static int cutters(int a, int b, int *slot, int *orient)
{
    int x = a % CR_N, y = a / CR_N, bx = b % CR_N, by = b / CR_N, n = 0;
    if (bx == x) {                              /* up or down: a horizontal wall between the ranks */
        int ry = by > y ? y : by;
        for (int sx = x - 1; sx <= x; sx++)
            if (sx >= 0 && sx < 8 && ry < 8) slot[n] = sx + 8 * ry, orient[n++] = CR_H;
    } else {                                    /* sideways: a vertical wall between the files */
        int rx = bx > x ? x : bx;
        for (int sy = y - 1; sy <= y; sy++)
            if (sy >= 0 && sy < 8 && rx < 8) slot[n] = rx + 8 * sy, orient[n++] = CR_V;
    }
    return n;
}

/* the slots a player's shortest way crosses (bit slot * 2 + orient - 1 of 128), the edges' list for walls */
typedef struct { uint64_t cut[2]; } cutset;

static void mark_path(const crpos *p, int player, cutset *c, int *cand, int *nc, uint64_t *seen, int reach)
{
    uint8_t path[CR_CELLS];
    int n = cr_path(p, player, path);
    c->cut[0] = c->cut[1] = 0;
    for (int i = 0; i < n; i++) {
        int s[2], o[2], k = cutters(path[i], path[i + 1], s, o);
        for (int j = 0; j < k; j++) {
            int b = s[j] * 2 + o[j] - 1;
            c->cut[b >> 6] |= 1ull << (b & 63);
            if (!cand || (reach && i >= reach) || seen[b >> 6] >> (b & 63) & 1) continue;
            seen[b >> 6] |= 1ull << (b & 63);
            cand[(*nc)++] = CR_WALL(s[j], o[j]);
        }
    }
}

static int a_moves(const void *st, int *out)
{
    const cbpos *s = st;
    const crpos *p = &s->p;
    if (p->winner >= 0) return 0;
    uint8_t to[8];
    int n = cr_pawn_moves(p, to);
    for (int i = 0; i < n; i++) out[i] = to[i];
    int me = p->turn;
    if (p->left[me]) {
        cutset cut[CR_PLAYERS];
        int cand[4 * 2 * CR_CELLS], nc = 0;
        uint64_t seen[2] = {0, 0};
        for (int i = 0; i < p->players; i++)    /* the opponents' ways give the candidates; every way is checked */
            mark_path(p, i, &cut[i], i == me ? NULL : cand, &nc, seen, s->reach);
        opens base;
        opens_of(p, &base);
        for (int k = 0; k < nc; k++) {
            int m = cand[k], slot = CR_SLOT(m), o = CR_ORIENT(m), b = slot * 2 + o - 1, ok = 1;
            if (!cr_wall_fits(p, slot, o)) continue;
            opens w = base;
            wall_off(&w, slot, o);
            for (int i = 0; i < p->players && ok; i++)
                if (cut[i].cut[b >> 6] >> (b & 63) & 1) ok = dist(p, &w, i) != CR_FAR;
            if (ok) out[n++] = m;
        }
    }
    if (!n) return cr_moves(p, out);           /* boxed in: every wall, or the pass */
    return n;
}

static void a_make(void *st, int m) { cr_apply(&((cbpos *)st)->p, m); }

static int a_side(const void *st)
{
    const cbpos *s = st;
    return s->p.turn != s->me;
}

static int a_over(const void *st, int *score)
{
    const cbpos *s = st;
    if (s->p.winner < 0) return 0;
    if (s->p.winner == CR_DRAW) { *score = 0; return 1; }
    int mine = s->p.winner == s->me ? ai_win(0) : -ai_win(0);
    *score = a_side(st) ? -mine : mine;
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const cbpos *s = st;
    uint64_t h = 0x9e3779b97f4a7c15ull ^ s->p.turn, w;
    for (int i = 0; i < CR_SLOTS; i += 8) {
        memcpy(&w, s->p.wall + i, 8);
        h = ai_mix(h, w);
    }
    memcpy(&w, s->p.pawn, 8);                   /* the pawns and the walls in hand */
    return ai_mix(h, w);
}

/* the bot's view: the closest opponent's way against its own, the walls in hand (each level its worth, wv: a bot that
 * values them low spends them all early and is then walled in), its own way a little more (no dawdling), the move */
static int value(const crpos *p, int me, int wv)
{
    opens o;
    opens_of(p, &o);
    int dme = dist(p, &o, me), dop = CR_FAR, wop = 0;
    for (int i = 0; i < p->players; i++)
        if (i != me) {
            int d = dist(p, &o, i);
            if (d < dop) dop = d;
            wop += p->left[i];
        }
    if (p->players > 2) wop = (wop + 1) / (p->players - 1);
    int v = 100 * (dop - dme) + wv * (p->left[me] - wop) - 15 * dme;
    if (p->turn == me) v += 40;                 /* the move in hand */
    if (!p->left[me] && !wop) v += 60 * (dop - dme);   /* a plain race: the distances are the game */
    return v;
}

static int a_eval(const void *st)
{
    const cbpos *s = st;
    int v = value(&s->p, s->me, s->wv);
    return a_side(st) ? -v : v;
}

/* the steps that bring the pawn home sooner, then the walls that lengthen the leader's way most */
static int a_order(const void *st, int m)
{
    const cbpos *s = st;
    const crpos *p = &s->p;
    if (m == CR_PASS) return 0;
    opens o;
    opens_of(p, &o);
    if (!CR_IS_WALL(m)) {
        if (cr_goal(p, p->turn, m)) return 100000;
        return 2000 - 100 * flood(&o, m, goal_of(p, p->turn));
    }
    wall_off(&o, CR_SLOT(m), CR_ORIENT(m));     /* the distances after it (the same before for every wall) */
    int best = 0;
    for (int i = 0; i < p->players; i++)
        if (i != p->turn) {
            int d = dist(p, &o, i);
            if (d > best) best = d;
        }
    return 100 * best - 1000 * dist(p, &o, p->turn);
}

static const ai_game ADAPTER = {
    .size = sizeof(cbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

/* the move for the player to move in the position, at a level; -1 none */
int crb_choose(const crpos *now, int level, uint64_t seed, ai_stats *stats)
{
    cbpos s;
    memset(&s, 0, sizeof s);
    s.p = *now;
    s.me = now->turn;
    s.wv = (int16_t)(level >= AI_HARD ? 120 : level == AI_MEDIUM ? 80 : 40);
    if (now->winner >= 0 || ai_cancelled()) return -1;
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(&s)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    o.budget.ms = o.budget.ms * 5 / 6;          /* the clock is checked every so many nodes: a margin */
    int four = now->players > 2;
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 1; o.budget.noise = 160; s.reach = 2; break;
    case AI_MEDIUM:
        o.budget.depth = 2;
        o.budget.nodes = 6000;
        o.budget.noise = 30;
        s.reach = 4;
        break;
    /* hard: two players as deep as about 0.25 s on a PC goes; four, two plies (deeper, the paranoid search sees the
     * three others ganging up on it as they never would, and plays worse) */
    default:        o.budget.nodes = 120000; o.budget.depth = four ? 2 : 0; break;
    }
    int m = ai_ab(&ADAPTER, &s, &o, stats);
    if (ai_cancelled()) return -1;
    if (m < 0 || !cr_legal(now, m)) {           /* never stall: the first legal move */
        int all[CR_MAX_MOVES];
        m = cr_moves(now, all) ? all[0] : -1;
    }
    return m;
}

static int think(int seat, int level, char *move, int cap)
{
    crpos now = *cr_pos(corridor_live());       /* read once, at entry (the main thread leaves it alone meanwhile) */
    if (now.winner >= 0 || now.turn != seat || cap < 4) return 0;
    int m = crb_choose(&now, level, ai_session_seed(), NULL);
    if (m < 0) return 0;
    cr_name(m, move);
    return 1;
}

static const bot corridor_bot = {.think = think, .play = corridor_show};
REGISTER_BOT(corridor, &corridor_bot)
