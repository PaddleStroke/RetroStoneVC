/*
 * Spread's bot (bots.h): alpha-beta (ai.h) on bitboards of the 7 x 7 board (a cell a bit, x + 7 * y as the rules
 * number them), every board with its blocked cells. The moves: one clone per empty cell next to a piece of one's own
 * (from which piece makes no difference), and the jumps. The evaluation: the pieces, the pieces that can still be
 * turned (next to an empty cell the other player reaches), the empty cells only one player reaches; the end as the
 * rules count it (the board full, 50 moves without a new piece, or a player without a move: the other takes the
 * empty cells).
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("a1b2" a clone, "a1c3" a jump), played by spread_show().
 */
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/spread.h"

#define FULL ((1ull << SPD_CELLS) - 1)
#define COL_A 0x0040810204081ull                /* x = 0 on every row */
#define COL_G (COL_A << 6)
#define CLONE 63                                /* a clone's "from": any piece next to the cell */

typedef struct {
    uint64_t own[2], block;
    uint8_t side, idle;
} sdbpos;

static inline int pop(uint64_t b) { return __builtin_popcountll(b); }

static inline uint64_t grow(uint64_t b)         /* b and the 8 cells round each of its cells */
{
    uint64_t r = (b | ((b << 1) & ~COL_A) | ((b >> 1) & ~COL_G)) & FULL;
    return (r | (r << 7) | (r >> 7)) & FULL;
}

static uint64_t empty_of(const sdbpos *p) { return ~(p->own[0] | p->own[1] | p->block) & FULL; }

static void from_rules(const spdpos *r, sdbpos *p)
{
    memset(p, 0, sizeof *p);
    for (int s = 0; s < SPD_CELLS; s++) {
        if (r->c[s] == SPD_P0) p->own[0] |= 1ull << s;
        else if (r->c[s] == SPD_P1) p->own[1] |= 1ull << s;
        else if (r->c[s] == SPD_BLOCK) p->block |= 1ull << s;
    }
    p->side = r->side;
    p->idle = r->idle;
}

/* ---- the adapter: a move is from * 64 + to (from CLONE: a clone) ------------------------------------------------- */
static int gen(const sdbpos *p, int c, int *out)
{
    uint64_t empty = empty_of(p), mine = p->own[c];
    int n = 0;
    for (uint64_t t = grow(mine) & empty; t; t &= t - 1) {
        if (out) out[n] = CLONE * 64 + __builtin_ctzll(t);
        n++;
    }
    for (uint64_t f = mine; f; f &= f - 1) {
        int s = __builtin_ctzll(f);
        uint64_t near = grow(1ull << s);
        for (uint64_t t = grow(near) & ~near & empty; t; t &= t - 1) {
            if (out) out[n] = s * 64 + __builtin_ctzll(t);
            n++;
        }
    }
    return n;
}

static int over_now(const sdbpos *p)            /* the rules' end: full, idle, or the player to move stuck */
{
    uint64_t empty = empty_of(p);
    if (!empty || p->idle >= SPD_IDLE) return 1;
    uint64_t mine = p->own[p->side], g1 = grow(mine);
    return !(grow(g1) & empty);                 /* nothing within two cells */
}

static int a_moves(const void *st, int *out)
{
    const sdbpos *p = st;
    return over_now(p) ? 0 : gen(p, p->side, out);
}

static void a_make(void *st, int m)
{
    sdbpos *p = st;
    int me = p->side, from = m / 64, to = m % 64;
    uint64_t b = 1ull << to, turned = grow(b) & p->own[!me];
    if (from != CLONE) {
        p->own[me] &= ~(1ull << from);
        p->idle++;
    } else p->idle = 0;
    p->own[me] |= b | turned;
    p->own[!me] &= ~turned;
    p->side ^= 1;
}

static int a_side(const void *st) { return ((const sdbpos *)st)->side; }

static int final_diff(const sdbpos *p)          /* the side to move's count minus the other's, as the rules count */
{
    int me = p->side, a = pop(p->own[me]), b = pop(p->own[!me]);
    uint64_t empty = empty_of(p);
    if (empty && p->idle < SPD_IDLE) b += pop(empty);   /* stuck: the other player fills the board */
    return a - b;
}

static int a_over(const void *st, int *score)
{
    const sdbpos *p = st;
    if (!over_now(p)) return 0;
    int d = final_diff(p);
    *score = d > 0 ? ai_win(d) : d < 0 ? -ai_win(-d) : 0;
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const sdbpos *p = st;
    return ai_mix(p->own[p->side], ai_mix(p->own[!p->side], (uint64_t)p->side << 8 | p->idle));
}

/* the side to move's view; 100 a piece */
static int a_eval(const void *st)
{
    const sdbpos *p = st;
    int me = p->side;
    uint64_t empty = empty_of(p), A = p->own[me], B = p->own[!me];
    uint64_t reachA = grow(grow(A)) & empty, reachB = grow(grow(B)) & empty;
    int v = 100 * (pop(A) - pop(B));
    /* pieces next to an empty cell the other reaches can be turned: the one to move gets there first */
    v -= 12 * pop(A & grow(reachB)) - 18 * pop(B & grow(reachA));
    v += 10 * (pop(reachA & ~reachB) - pop(reachB & ~reachA));
    return v;
}

static int a_order(const void *st, int m)       /* clones that turn many, then jumps that turn many and open little */
{
    const sdbpos *p = st;
    int me = p->side, from = m / 64, to = m % 64;
    int turned = pop(grow(1ull << to) & p->own[!me]);
    if (from == CLONE) return 3 * turned + 2;
    uint64_t left = grow(1ull << from) & p->own[me] & ~(1ull << from);
    return 3 * turned - pop(left) / 2;
}

/* the test's check: in the rules' position r, the bot's moves are the rules' (a clone once per cell), each one makes
 * the rules' position, and the end and its count are the rules' (over: the rules say the game is over; diff: the
 * final count, the player to move's minus the other's) */
int sdb_agrees(const spdpos *r, int over, int diff)
{
    sdbpos p, q;
    from_rules(r, &p);
    spdmove l[SPD_MAX_MOVES];
    int n = spd_legal(r, l), mv[AI_MAX_MOVES], k = gen(&p, p.side, mv), clones = 0;
    uint64_t seen = 0;
    for (int i = 0; i < n; i++) {
        int d = spd_dist(l[i].from, l[i].to), m = d == 1 ? CLONE * 64 + l[i].to : l[i].from * 64 + l[i].to, found = 0;
        if (d == 1 && !(seen >> l[i].to & 1)) clones++, seen |= 1ull << l[i].to;
        for (int j = 0; j < k && !found; j++) found = mv[j] == m;
        spdpos a = *r;
        spd_make(&a, l[i]);
        sdbpos b;
        from_rules(&a, &b);
        q = p;
        a_make(&q, m);
        if (!found || memcmp(&q, &b, sizeof q)) return 0;
    }
    int jumps = 0;
    for (int i = 0; i < n; i++) jumps += spd_dist(l[i].from, l[i].to) == 2;
    if (k != clones + jumps || over_now(&p) != over) return 0;
    return !over || final_diff(&p) == diff;
}

static const ai_game ADAPTER = {
    .size = sizeof(sdbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

/* the move in the position r (its player to move), at a level: from (the clone's: a piece of the player's next to
 * the cell) and to; 0: none */
int sdb_choose(const spdpos *r, int level, uint64_t seed, int *from, int *to, ai_stats *stats)
{
    sdbpos p;
    from_rules(r, &p);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(&p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    o.budget.ms = o.budget.ms * 5 / 6;                 /* the clock is checked every so many nodes: a margin */
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 1; o.budget.nodes = 2000; o.budget.noise = 150; break;
    case AI_MEDIUM: o.budget.depth = 3; o.budget.nodes = 30000; o.budget.noise = 40; break;
    default:        o.budget.nodes = 1200000; break;   /* about 0.3 s on a PC */
    }
    int m = ai_ab(&ADAPTER, &p, &o, stats);
    if (m < 0) return 0;
    *to = m % 64;
    *from = m / 64;
    if (*from == CLONE) *from = __builtin_ctzll(grow(1ull << *to) & p.own[p.side]);
    return 1;
}

static int think(int seat, int level, char *move, int cap)
{
    const spdgame *g = spread_live();
    spdpos now = *spd_pos(g);                   /* the copy, at entry: nothing else is read */
    sdbpos p;
    from_rules(&now, &p);
    if (g->ply >= SPD_MAX_PLY || over_now(&p) || now.side != seat || cap < 5) return 0;
    int from, to;
    if (!sdb_choose(&now, level, ai_session_seed(), &from, &to, NULL) || ai_cancelled()) return 0;
    move[0] = (char)('a' + from % SPD_N);
    move[1] = (char)('1' + from / SPD_N);
    move[2] = (char)('a' + to % SPD_N);
    move[3] = (char)('1' + to / SPD_N);
    move[4] = 0;
    return 1;
}

static const bot spread_bot = {.think = think, .play = spread_show};
REGISTER_BOT(spread, &spread_bot)
