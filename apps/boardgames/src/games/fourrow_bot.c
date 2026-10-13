/*
 * Four in a row's bot (bots.h): alpha-beta (ai.h) on bitboards, with a transposition table; the evaluation counts
 * threats (empty holes that would make four), weighted by their row (the first player's threats are worth most on
 * the odd rows from the bottom, 1, 3, 5, the second player's on the even rows: who gets them at the end of the game),
 * a threat ready to play, lines of four still open with two discs in them, and the centre columns.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation (the column, "1".."7" from the left), played by fourrow_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/fourrow.h"

/* the bitboard: a column a 7 bits (6 holes from the bottom, a spare above), hole (c, r) at bit c * 7 + r */
#define H1 (FR_ROWS + 1)
#define BIT(c, r) (1ull << ((c) * H1 + (r)))
#define BOTTOM 0x0000040810204081ull            /* the bottom hole of every column */
#define BOARD (BOTTOM * ((1ull << FR_ROWS) - 1))
#define ROWS_ODD (BOTTOM * 0x15ull)             /* rows 1, 3, 5 from the bottom (r 0, 2, 4): the first player's */
#define ROWS_EVEN (BOTTOM * 0x2aull)            /* rows 2, 4, 6 (r 1, 3, 5): the second player's */
#define COL(c) (((1ull << FR_ROWS) - 1) << ((c) * H1))

typedef struct {
    uint64_t disc[2], mask;                     /* each player's discs, all of them */
    int n;                                      /* discs dropped (the side to move: n & 1) */
    int won;                                    /* the player who made four, -1 */
} fbb;

static inline int pop(uint64_t b) { return __builtin_popcountll(b); }

static int four(uint64_t b)                     /* four in a line somewhere: across, on both slants, up */
{
    static const int S[4] = {H1, H1 - 1, H1 + 1, 1};
    for (int i = 0; i < 4; i++) {
        uint64_t m = b & (b >> S[i]);
        if (m & (m >> 2 * S[i])) return 1;
    }
    return 0;
}

/* the empty holes where a disc of b's would make four */
static uint64_t threats(uint64_t b, uint64_t mask)
{
    uint64_t r = (b << 1) & (b << 2) & (b << 3), p;
    static const int S[3] = {H1, H1 - 1, H1 + 1};
    for (int i = 0; i < 3; i++) {
        int s = S[i];
        p = (b << s) & (b << 2 * s);
        r |= p & (b << 3 * s);
        r |= p & (b >> s);
        p = (b >> s) & (b >> 2 * s);
        r |= p & (b << s);
        r |= p & (b >> 3 * s);
    }
    return r & (BOARD ^ mask);
}

/* the lines of four still open to b (none of o's discs in them) holding exactly two of b's */
static int open_twos(uint64_t b, uint64_t o)
{
    static const int S[4] = {1, H1, H1 - 1, H1 + 1};
    uint64_t free_ = BOARD & ~o;
    int n = 0;
    for (int i = 0; i < 4; i++) {
        int s = S[i];
        uint64_t v = free_ & free_ >> s & free_ >> 2 * s & free_ >> 3 * s;   /* the lines' first holes */
        uint64_t x0 = b, x1 = b >> s, x2 = b >> 2 * s, x3 = b >> 3 * s;
        uint64_t s1 = x0 ^ x1, c1 = x0 & x1, s2 = x2 ^ x3, c2 = x2 & x3;
        uint64_t b0 = s1 ^ s2, carry = s1 & s2, b1 = c1 ^ c2 ^ carry, b2 = (c1 & c2) | (c1 & carry) | (c2 & carry);
        n += pop(v & ~b0 & b1 & ~b2);
    }
    return n;
}

static void from_rules(const fr_game *g, fbb *p)
{
    p->disc[0] = p->disc[1] = p->mask = 0;
    for (int r = 0; r < FR_ROWS; r++)
        for (int c = 0; c < FR_COLS; c++)
            if (g->hole[r][c] >= 0) p->disc[g->hole[r][c]] |= BIT(c, r);
    p->mask = p->disc[0] | p->disc[1];
    p->n = g->n;
    p->won = g->winner;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    const fbb *p = st;
    int n = 0;
    if (p->won >= 0 || p->n >= FR_HOLES) return 0;
    for (int c = 0; c < FR_COLS; c++)
        if (!(p->mask & BIT(c, FR_ROWS - 1))) out[n++] = c;
    return n;
}

static void a_make(void *st, int c)
{
    fbb *p = st;
    int me = p->n & 1;
    uint64_t b = (p->mask + BIT(c, 0)) & COL(c);
    p->disc[me] |= b;
    p->mask |= b;
    p->n++;
    if (four(p->disc[me])) p->won = me;
}

static int a_side(const void *st) { return ((const fbb *)st)->n & 1; }

static int a_over(const void *st, int *score)
{
    const fbb *p = st;
    if (p->won >= 0) { *score = p->won == (p->n & 1) ? ai_win(0) : -ai_win(0); return 1; }
    if (p->n >= FR_HOLES) { *score = 0; return 1; }
    return 0;
}

static uint64_t a_hash(const void *st)
{
    const fbb *p = st;
    return ai_mix(p->disc[p->n & 1], p->mask);
}

static int a_order(const void *st, int c)       /* the centre first */
{
    (void)st;
    return 3 - (c > 3 ? c - 3 : 3 - c);
}

/* the side to move's view; about 100 a threat */
static int a_eval(const void *st)
{
    const fbb *p = st;
    int me = p->n & 1;
    uint64_t P = p->disc[me], O = p->disc[!me], playable = (p->mask + BOTTOM) & BOARD;
    uint64_t tp = threats(P, p->mask), to = threats(O, p->mask);
    if (tp & playable) return 5000;             /* a four to make now */
    if (pop(to & playable) > 1) return -5000;   /* two to stop at once */
    int v = 0;
    uint64_t good[2] = {ROWS_ODD, ROWS_EVEN};
    v += 60 * pop(tp) + 90 * pop(tp & good[me]);
    v -= 60 * pop(to) + 90 * pop(to & good[!me]);
    /* two threats one above the other: whoever fills the hole under them, the second one wins */
    v += 120 * pop(tp & (tp >> 1)) - 120 * pop(to & (to >> 1));
    v += 12 * (open_twos(P, O) - open_twos(O, P));
    v += 10 * (pop(P & COL(3)) - pop(O & COL(3))) + 5 * (pop(P & (COL(2) | COL(4))) - pop(O & (COL(2) | COL(4))));
    return v;
}

static const ai_game ADAPTER = {
    .size = sizeof(fbb), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

int frb_choose(const fr_game *g, int level, uint64_t seed, ai_stats *stats)
{
    fbb p;
    from_rules(g, &p);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(&p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 2; o.budget.nodes = 2000; o.budget.noise = 150; break;
    case AI_MEDIUM: o.budget.depth = 6; o.budget.nodes = 50000; o.budget.noise = 40; break;
    default:        o.budget.nodes = 5000000; o.tt_bits = 19; break;   /* about 0.3 s on a PC */
    }
    return ai_ab(&ADAPTER, &p, &o, stats);
}

static int think(int seat, int level, char *move, int cap)
{
    fr_game g = *fourrow_live();                /* the copy, at entry: nothing else is read */
    if (fr_status(&g) != FR_PLAY || fr_turn(&g) != seat || cap < 2) return 0;
    int c = frb_choose(&g, level, ai_session_seed(), NULL);
    if (c < 0 || ai_cancelled()) return 0;
    snprintf(move, (size_t)cap, "%d", c + 1);
    return 1;
}

static const bot fourrow_bot = {.think = think, .play = fourrow_show};
REGISTER_BOT(fourrow, &fourrow_bot)
