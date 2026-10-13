/*
 * Durak's bot (bots.h): podkidnoy and perevodnoy, hidden or open hands, 2 to 6 players. Information-set Monte Carlo
 * tree search (ai_mcts) over the rest of the game, each iteration dealing the cards the seat has not seen at random
 * into the others' hands and the talon, consistently with what it saw: the cards a defender picked up (in their hand
 * until played), the turned-up trump (at the talon's bottom, then in the hand of whoever drew it), the cards beaten
 * off. With open hands only the talon's order is unknown; once it is empty the game is of perfect information, and
 * between the last two players the bot searches it with alpha-beta (ai_ab: deeper than the sampling). Runs on the
 * worker thread: it copies the game at entry and reads only what its seat may know. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The search's moves: an attack or a throw-in of cards of one rank (any of its suits), a defence of the first open
 * card (with any card that beats it), a transfer, take, pass. The playouts: attack with the cheapest cards (side suits
 * low, trumps kept), beat with the cheapest card that beats or take, throw in cheap cards (all of them on a taker),
 * transfer cheaply. The reward: the durak loses, everyone else wins (all out at once: a draw).
 * Its moves are the game's notation ("7s7h", "Ks/7s", "x7d", "t", "p"), played by durak_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/cardbot.h"
#include "games/durak.h"
#include "games/tricks.h"

#define DUR_HARD_ITERS 18000                    /* about 0.3 s a move on a PC */
#define DUR_CUTOFF 100
#define DM_PLAY (1 << 20)                       /* | rank << 4 | suits (bit s: suit s) */
#define DM_BEAT (2 << 20)                       /* | the card's index (dur_index), the first open card */
#define DM_TAKE (3 << 20)
#define DM_PASS (4 << 20)
#define DM_SHIFT (5 << 20)                      /* | the card's index */
#define DM_KIND(m) ((m) & (7 << 20))

typedef struct {
    dpos p;
    uint64_t gone;                              /* the cards beaten off (bit dur_index) */
    uint8_t me, open;                           /* the observer; open hands */
} dk_st;

static int popc(unsigned m) { return __builtin_popcount(m); }
static int find(const dpos *p, int pl, int c)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] == c) return i;
    return -1;
}
static int first_open(const dpos *p)
{
    for (int i = 0; i < p->natt; i++)
        if (p->def[i] == DUR_NONE) return i;
    return -1;
}

/* the search's move as the rules' action (0: none) */
static uint64_t real(const dpos *p, int m)
{
    switch (DM_KIND(m)) {
    case DM_PLAY: {
        uint64_t a = 0;
        int rank = m >> 4 & 15;
        for (int s = 0; s < 4; s++)
            if (m >> s & 1) a |= 1ull << dur_index(CARD(s, rank));
        return DA_PLAY | a;
    }
    case DM_BEAT: {
        int slot = first_open(p);
        return slot < 0 ? 0 : DA_BEATS(dur_card(m & 63), slot);
    }
    case DM_TAKE: return DA_TAKE;
    case DM_PASS: return DA_PASS;
    case DM_SHIFT: return DA_SHIFT | (uint64_t)dur_card(m & 63);
    default: return 0;
    }
}

static void d_apply(dpos *p, uint64_t a)
{
    dgame g;
    g.pos = *p;
    g.n = 0;
    if (dur_play(&g, a)) *p = g.pos;
}

/* the suits of each rank the player holds (rank 1..13), throwable or all */
static void rank_suits(const dpos *p, int pl, int throw_only, unsigned *m)
{
    memset(m, 0, 14 * sizeof *m);
    for (int i = 0; i < p->n[pl]; i++) {
        int c = p->hand[pl][i];
        if (c >= CDB_HIDDEN || (throw_only && !dur_can_throw(p, c))) continue;
        m[CARD_RANK(c)] |= 1u << CARD_SUIT(c);
    }
}

static int d_moves(const void *s, int *out)
{
    const dpos *p = &((const dk_st *)s)->p;
    int k = 0, pl = p->turn;
    switch (p->phase) {
    case DP_ATTACK:
    case DP_THROW: {
        unsigned m[14];
        int room = dur_room(p);
        rank_suits(p, pl, p->phase == DP_THROW, m);
        if (p->phase == DP_THROW) out[k++] = DM_PASS;
        for (int r = 1; r <= 13; r++)
            for (unsigned sub = m[r]; sub; sub = (sub - 1) & m[r])
                if (popc(sub) <= room) out[k++] = DM_PLAY | r << 4 | (int)sub;
        return k;
    }
    case DP_DEFEND: {
        int slot = first_open(p);
        if (slot < 0) return 0;
        for (int i = 0; i < p->n[pl]; i++) {
            int c = p->hand[pl][i];
            if (c >= CDB_HIDDEN) continue;
            if (dur_beats(p, c, p->att[slot])) out[k++] = DM_BEAT | dur_index(c);
            if (dur_can_shift(p, c)) out[k++] = DM_SHIFT | dur_index(c);
        }
        if (!p->taking) out[k++] = DM_TAKE;
        return k;
    }
    default: return 0;
    }
}

static void d_make(void *s, int m)   /* (the cards beaten off matter only at the root: its deal) */
{
    dpos *p = &((dk_st *)s)->p;
    d_apply(p, real(p, m));
}

static int d_side(const void *s) { return ((const dk_st *)s)->p.turn; }

static int d_result(const void *s, float *rw)
{
    const dpos *p = &((const dk_st *)s)->p;
    if (p->phase != DP_OVER) return 0;
    for (int q = 0; q < p->players; q++) rw[q] = p->fool == DUR_NONE ? 0.7f : q == p->fool ? 0 : 1;
    return 1;
}

/* a card's cost to give away: the trumps dear */
static int cost(const dpos *p, int c) { return (CARD_SUIT(c) == p->trump) * 20 + dur_strength(c); }

/* a long game cut short: the likelier durak holds more cards and fewer trumps */
static int d_estimate(const void *s, float *rw)
{
    const dpos *p = &((const dk_st *)s)->p;
    float bad[DUR_MAXP], sum = 0;
    for (int q = 0; q < p->players; q++) {
        bad[q] = 0;
        if (!dur_active(p, q)) continue;
        float b = (float)p->n[q] + 1;
        for (int i = 0; i < p->n[q]; i++) b -= cost(p, p->hand[q][i]) >= 30 ? 0.6f : 0;
        bad[q] = b < 0.5f ? 0.5f : b;
        sum += bad[q];
    }
    for (int q = 0; q < p->players; q++) rw[q] = sum > 0 ? 1 - bad[q] / sum : 0.5f;
    return 1;
}

/* ---- the playouts' heuristic ------------------------------------------------------------------------------------ */
static int d_policy(const dpos *p, ai_rng *r, int eps)
{
    int pl = p->turn, late = !p->ntalon;
    if (eps && ai_rand_n(r, 100) < eps) {
        int mv[AI_MAX_MOVES];
        dk_st t;
        t.p = *p;
        int n = d_moves(&t, mv);
        return n ? mv[ai_rand_n(r, n)] : AI_NONE;
    }
    switch (p->phase) {
    case DP_ATTACK: {
        int best = -1;
        for (int i = 0; i < p->n[pl]; i++)
            if (best < 0 || cost(p, p->hand[pl][i]) < cost(p, best)) best = p->hand[pl][i];
        if (best < 0) return AI_NONE;
        unsigned m[14];
        rank_suits(p, pl, 0, m);
        int rank = CARD_RANK(best);
        unsigned sub = 1u << CARD_SUIT(best);
        for (int s = 0; s < 4; s++)              /* its fellows of the rank, but the trumps */
            if (m[rank] >> s & 1 && s != p->trump && popc(sub) < dur_room(p)) sub |= 1u << s;
        return DM_PLAY | rank << 4 | (int)sub;
    }
    case DP_DEFEND: {
        int slot = first_open(p), best = -1;
        if (slot < 0) return AI_NONE;
        for (int i = 0; i < p->n[pl]; i++) {
            int c = p->hand[pl][i];
            if (dur_can_shift(p, c) && CARD_SUIT(c) != p->trump) return DM_SHIFT | dur_index(c);
            if (dur_beats(p, c, p->att[slot]) && (best < 0 || cost(p, c) < cost(p, best))) best = c;
        }
        if (best < 0 || (!late && cost(p, best) >= 32 && dur_open(p) > 1)) return DM_TAKE;
        return DM_BEAT | dur_index(best);
    }
    case DP_THROW: {
        int room = dur_room(p);
        unsigned m[14];
        rank_suits(p, pl, 1, m);
        int bestr = -1, bc = 99;
        for (int rk = 1; rk <= 13; rk++) {
            if (!m[rk]) continue;
            for (int s = 0; s < 4; s++) {
                if (!(m[rk] >> s & 1)) continue;
                int c = cost(p, CARD(s, rk));
                if (c < bc) bc = c, bestr = rk;
            }
        }
        if (bestr < 0 || !room) return DM_PASS;
        if (!p->taking && !late && bc >= 12) return DM_PASS;   /* keep the high cards while the talon lasts */
        unsigned sub = 0;
        for (int s = 0; s < 4; s++)
            if (m[bestr] >> s & 1 && (s != p->trump || late) && popc(sub) < room) sub |= 1u << s;
        if (!sub) return DM_PASS;
        return DM_PLAY | bestr << 4 | (int)sub;
    }
    default: return AI_NONE;
    }
}

static int d_playout(const void *s, ai_rng *r) { return d_policy(&((const dk_st *)s)->p, r, 6); }

/* ---- the view and the deal of the unseen ------------------------------------------------------------------------ */
static void d_determinise(void *s, int seat, ai_rng *r)
{
    (void)seat;
    dk_st *st = s;
    dpos *p = &st->p;
    uint64_t seen = st->gone;
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++)
            if (p->hand[q][i] < CDB_HIDDEN) seen |= 1ull << dur_index(p->hand[q][i]);
    for (int i = 0; i < p->natt; i++) {
        seen |= 1ull << dur_index(p->att[i]);
        if (p->def[i] != DUR_NONE) seen |= 1ull << dur_index(p->def[i]);
    }
    for (int i = 0; i < p->ntalon; i++)
        if (p->talon[i] < CDB_HIDDEN) seen |= 1ull << dur_index(p->talon[i]);
    uint8_t pool[DUR_CARDS];
    int np = 0;
    for (int i = 0; i < DUR_CARDS; i++)
        if (!(seen >> i & 1)) pool[np++] = (uint8_t)dur_card(i);
    cdb_shuffle(pool, np, r);
    for (int q = 0; q < p->players; q++) cdb_fill(p->hand[q], p->n[q], pool, &np);
    cdb_fill(p->talon, p->ntalon, pool, &np);
    p->seed = ai_rand(r);
}

/* seat me's view of the game: the deal's actions replayed for what everyone saw (the cards picked up, beaten off,
 * the trump card drawn); its own hand; the others' hands hidden but the cards it knows there (open hands: all) */
static void d_view(const dgame *g, int me, int open, dk_st *v)
{
    const dpos *live = &g->pos;
    memset(v, 0, sizeof *v);
    v->p = *live;
    v->me = (uint8_t)me;
    v->open = (uint8_t)open;
    uint64_t known[DUR_MAXP] = {0}, table = 0;
    dgame *t = malloc(sizeof *t);
    if (t) {                                    /* only what everyone saw is read */
        dur_new(t, g->variant, g->players, g->seed);
        for (int i = 0; i < g->n; i++) {
            uint64_t a = g->act[i];
            int pl = t->pos.turn, bouts = t->pos.bouts, talon = t->pos.ntalon;
            uint64_t cards = DA_KIND(a) == DA_PLAY ? DA_MASK(a)
                           : DA_KIND(a) == DA_BEAT || DA_KIND(a) == DA_SHIFT ? 1ull << dur_index(DA_CARD(a)) : 0;
            known[pl] &= ~cards;
            table |= cards;
            dur_play(t, a);
            const dpos *q = &t->pos;
            if (q->bouts != bouts) {
                if (q->last_end == 2) known[q->last_by] |= table;
                else v->gone |= table;
                table = 0;
            }
            if (talon && !q->ntalon)            /* the turned-up trump drawn: whoever holds it */
                for (int x = 0; x < q->players; x++)
                    if (find(q, x, q->trump_card) >= 0) known[x] |= 1ull << dur_index(q->trump_card);
        }
        free(t);
    }
    dpos *p = &v->p;
    for (int q = 0; q < p->players; q++) {
        if (q == me || open) continue;
        int k = 0;
        for (int i = 0; i < DUR_CARDS && k < p->n[q]; i++)
            if (known[q] >> i & 1) p->hand[q][k++] = (uint8_t)dur_card(i);
        memset(p->hand[q] + k, CDB_HIDDEN, (size_t)(p->n[q] - k));
    }
    for (int i = 0; i < p->ntalon; i++) p->talon[i] = i == 0 ? p->trump_card : CDB_HIDDEN;
    p->seed = 0;
}

/* ---- open hands, the talon empty, two players left: alpha-beta ------------------------------------------------- */
static int ab_over(const void *s, int *score)
{
    const dpos *p = &((const dk_st *)s)->p;
    if (p->phase != DP_OVER) return 0;
    *score = p->fool == DUR_NONE ? 0 : p->fool == p->turn ? -ai_win(0) : ai_win(0);
    return 1;
}

static int strength(const dpos *p, int q)
{
    int s = -p->n[q] * 100;
    for (int i = 0; i < p->n[q]; i++) s += cost(p, p->hand[q][i]) * 4;
    return s;
}

static int ab_eval(const void *s)
{
    const dpos *p = &((const dk_st *)s)->p;
    int me = p->turn, other = me;
    for (int q = 0; q < p->players; q++)
        if (q != me && dur_active(p, q)) other = q;
    return strength(p, me) - strength(p, other);
}

static uint64_t ab_hash(const void *s)
{
    const dpos *p = &((const dk_st *)s)->p;
    uint64_t h = (uint64_t)p->phase * 31 + p->turn + ((uint64_t)p->taking << 5) + ((uint64_t)p->passed << 8);
    for (int q = 0; q < p->players; q++) {
        uint64_t m = 0;
        for (int i = 0; i < p->n[q]; i++) m |= 1ull << dur_index(p->hand[q][i]);
        h = ai_mix(h, m + (uint64_t)q);
    }
    for (int i = 0; i < p->natt; i++) h = ai_mix(h, (uint64_t)p->att[i] << 8 | p->def[i]);
    return ai_mix(h, (uint64_t)p->attacker << 4 | p->defender);
}

static int ab_order(const void *s, int m)
{
    const dpos *p = &((const dk_st *)s)->p;
    switch (DM_KIND(m)) {
    case DM_BEAT: return 1000 - cost(p, dur_card(m & 63)) * 10;
    case DM_PLAY: return 500 - (m >> 4 & 15) * 10 + popc((unsigned)m & 15) * 20;
    case DM_SHIFT: return 800;
    case DM_PASS: return 400;
    default: return 0;
    }
}

static int endgame(const dk_st *v)
{
    const dpos *p = &v->p;
    if (!v->open || p->ntalon) return 0;
    int left = 0;
    for (int q = 0; q < p->players; q++) left += dur_active(p, q);
    return left == 2;
}

/* the action for seat at a level (0: nothing to do); open: the hands are face up for everyone */
uint64_t durb_choose(const dgame *g, int seat, int open, int level, uint64_t seed, ai_stats *stats)
{
    const dpos *live = &g->pos;
    if (stats) memset(stats, 0, sizeof *stats);
    if (live->phase == DP_OVER || live->turn != seat) return 0;
    dk_st *v = malloc(sizeof *v);
    if (!v) return dur_auto(live);
    d_view(g, seat, open, v);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 977 + live->ntalon));
    int mv[AI_MAX_MOVES], n = d_moves(v, mv), m = n ? mv[0] : AI_NONE;
    if (n > 1) {
        if (cdb_slip(level, &r)) {
            dk_st w = *v;
            d_determinise(&w, seat, &r);
            m = d_policy(&w.p, &r, CDB_SLIP_EPS);
        } else if (endgame(v) && level > AI_EASY) {
            ai_game G = {
                .size = sizeof(dk_st), .moves = d_moves, .make = d_make, .side = d_side, .over = ab_over,
                .eval = ab_eval, .hash = ab_hash, .order = ab_order, .unit = 100,
            };
            ai_opts o = {.budget = ai_level_budget(level), .rng = &r, .tt_bits = 18};
            o.budget.nodes = level >= AI_HARD ? 150000 : 30000;   /* about 0.3 s on a PC at hard */
            int x = ai_ab(&G, v, &o, stats);
            if (x != AI_NONE) m = x;
        } else {
            ai_game G = {
                .size = sizeof(dk_st), .moves = d_moves, .make = d_make, .side = d_side, .players = live->players,
                .result = d_result, .playout = d_playout, .estimate = d_estimate, .cutoff = DUR_CUTOFF,
                .determinise = d_determinise,
            };
            /* open hands: only the talon to deal, the samples agree more: more of them */
            int x = cdb_search(&G, v, level, open ? DUR_HARD_ITERS * 3 / 2 : DUR_HARD_ITERS, &r, stats);
            if (x != AI_NONE) m = x;
        }
    }
    uint64_t a = m == AI_NONE ? 0 : real(live, m);
    free(v);
    return a && dur_legal(live, a) ? a : dur_auto(live);
}

static int think(int seat, int level, char *move, int cap)
{
    dgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *durak_live();                         /* the copy, at entry: nothing else is read */
    int open = durak_open_hands();
    uint64_t a = durb_choose(g, seat, open, level, ai_mix(ai_session_seed(), (uint64_t)g->n), NULL);
    const dpos *p = &g->pos;
    int ok = a && !ai_cancelled() && cap >= 16;
    char c[3], d[3];
    if (ok)
        switch (DA_KIND(a)) {
        case DA_PLAY: {
            int k = 0;
            for (int i = 0; i < DUR_CARDS && k + 3 < cap; i++)
                if (DA_MASK(a) >> i & 1) trick_name(dur_card(i), move + k), k += 2;
            move[k] = 0;
            break;
        }
        case DA_BEAT:
            trick_name(DA_CARD(a), c), trick_name(p->att[DA_SLOT(a)], d);
            snprintf(move, (size_t)cap, "%s/%s", c, d);
            break;
        case DA_SHIFT: trick_name(DA_CARD(a), c), snprintf(move, (size_t)cap, "x%s", c); break;
        case DA_TAKE: snprintf(move, (size_t)cap, "t"); break;
        case DA_PASS: snprintf(move, (size_t)cap, "p"); break;
        default: ok = 0;
        }
    free(g);
    return ok;
}

static int actor(void)
{
    const dpos *p = &durak_live()->pos;
    return p->phase == DP_OVER ? -1 : p->turn;
}

static const bot durak_bot = {.think = think, .actor = actor, .play = durak_show};
REGISTER_BOT(durak, &durak_bot)
