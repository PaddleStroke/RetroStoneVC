/*
 * Golf's bot (bots.h), six-card golf and number golf, 2 to 6 players: information-set Monte Carlo tree search
 * (ai_mcts) over the round, each iteration dealing the cards nobody has seen (the pack minus the cards face up, the
 * discard and the card in hand) at random into the face-down places and the stock; heuristic playouts, cut short and
 * scored by what shows plus the face-down cards at their average. Runs on the worker thread: it copies the game at
 * entry and reads only what its seat may know (everything face up, the discard, the card it drew). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The playouts: take the discard when it is low or makes a column; a card drawn goes where it saves the most (over a
 * high card face up, a face-down card's average, or the place that makes a column), else it is discarded and a card
 * turned up; nobody ends the round without the lowest score unless forced. The reward: the round's score against the
 * others' (the lower the better). The next round waits for a human tap (actor() -1); think() answers "n" there.
 * Its moves are the game's notation ("r3", "s", "t", "w2", "d", "f5", "n"), played by golf_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/cardbot.h"
#include "games/golf.h"

#define GOLF_HARD_ITERS 18000                   /* about 0.3 s a move on a PC (six-card golf; number golf half) */
#define GOLF_CUTOFF 40                          /* playout actions before the estimate */

typedef struct { gpos p; uint8_t me; } gf_st;

static int up(const gpos *p, int pl, int s) { return p->up[pl] >> s & 1; }
static int alive(const gpos *p, int pl, int s) { return !(p->gone[pl] >> s & 1); }

static void g_apply(gpos *p, int a)
{
    ggame g;
    g.pos = *p;
    g.n = 0;
    if (golf_play(&g, a)) *p = g.pos;
}

static int g_moves(const void *s, int *out)
{
    const gpos *p = &((const gf_st *)s)->p;
    static const int kinds[] = {GA_STOCK, GA_TAKE, GA_DISCARD};
    int k = 0;
    if (p->phase == GP_ROUND || p->phase == GP_OVER) return 0;
    for (int i = 0; i < 3; i++)
        if (golf_legal(p, kinds[i])) out[k++] = kinds[i];
    for (int sl = 0; sl < p->slots; sl++) {
        if (golf_legal(p, GA_REVEAL | sl)) out[k++] = GA_REVEAL | sl;
        if (golf_legal(p, GA_SWAP | sl)) out[k++] = GA_SWAP | sl;
        if (golf_legal(p, GA_FLIP | sl)) out[k++] = GA_FLIP | sl;
    }
    return k;
}

static void g_make(void *s, int m) { g_apply(&((gf_st *)s)->p, m); }
static int g_side(const void *s) { return ((const gf_st *)s)->p.turn; }

/* the face-down cards' average (the pack's) */
static float avg_down(const gpos *p) { return p->variant == GOLF_NUMBER ? 5.07f : 6.54f; }

/* the scores as rewards: each against the others' average */
static void g_reward(const gpos *p, const float *score, float *rw)
{
    float scale = p->variant == GOLF_NUMBER ? 30.0f : 16.0f;
    for (int q = 0; q < p->players; q++) {
        float others = 0;
        for (int o = 0; o < p->players; o++)
            if (o != q) others += score[o];
        others /= (float)(p->players - 1);
        float x = 0.5f + (others - score[q]) / (2 * scale);
        rw[q] = x < 0 ? 0 : x > 1 ? 1 : x;
    }
}

static int g_result(const void *s, float *rw)
{
    const gpos *p = &((const gf_st *)s)->p;
    if (p->phase != GP_ROUND && p->phase != GP_OVER) return 0;
    float sc[GOLF_MAXP];
    for (int q = 0; q < p->players; q++) sc[q] = p->last[q];
    g_reward(p, sc, rw);
    return 1;
}

/* a grid's score as it stands, card c put face up in slot s (s < 0: none), the face-down cards at the average: the
 * columns of two of a rank (six-card golf) and of three of a value (number golf) score 0 */
static float grid_guess(const gpos *p, int pl, int s, int c)
{
    uint8_t card[GOLF_SLOTS];
    unsigned upm = p->up[pl], gone = p->gone[pl];
    memcpy(card, p->card[pl], p->slots);
    if (s >= 0) card[s] = (uint8_t)c, upm |= 1u << s;
    float sum = 0, avg = avg_down(p);
    for (int col = 0; col < p->cols; col++) {
        int same = 1, n = 0;
        float part = 0;
        for (int row = 0; row < p->rows; row++) {
            int x = row * p->cols + col;
            if (gone >> x & 1) { same = 0; continue; }
            n++;
            if (!(upm >> x & 1)) { part += avg; same = 0; continue; }
            part += (float)golf_value(p, card[x]);
            int differ = p->variant == GOLF_NUMBER ? card[x] != card[col] : CARD_RANK(card[x]) != CARD_RANK(card[col]);
            if (row && (gone >> col & 1 || !(upm >> col & 1) || differ)) same = 0;
        }
        if (!(same && n == p->rows)) sum += part;
    }
    return sum;
}

static float g_guess(const gpos *p, int q) { return grid_guess(p, q, -1, 0); }

static int g_estimate(const void *s, float *rw)
{
    const gpos *p = &((const gf_st *)s)->p;
    float sc[GOLF_MAXP];
    for (int q = 0; q < p->players; q++) sc[q] = g_guess(p, q);
    g_reward(p, sc, rw);
    return 1;
}

/* ---- the playouts' heuristic ------------------------------------------------------------------------------------ */
/* the score of the player's grid with card c in slot s (the face-down others at the average) */
static float with_card(const gpos *p, int pl, int s, int c) { return grid_guess(p, pl, s, c); }

/* the best place for card c: the slot and the score it makes (lower is better) */
static int best_slot(const gpos *p, int pl, int c, float *score)
{
    int best = -1;
    float bs = 1e9f;
    for (int s = 0; s < p->slots; s++) {
        if (!alive(p, pl, s)) continue;
        float v = with_card(p, pl, s, c);
        if (v < bs) bs = v, best = s;
    }
    *score = bs;
    return best;
}

/* going out (the last card up) only with the lowest score in sight */
static int ends_badly(const gpos *p, int pl, int s)
{
    if (p->ender != GOLF_NONE || golf_down(p, pl) != 1 || up(p, pl, s)) return 0;
    float mine = grid_guess(p, pl, -1, 0);   /* (the last card at the average) */
    for (int q = 0; q < p->players; q++)
        if (q != pl && g_guess(p, q) <= mine) return 1;
    return 0;
}

static int g_policy(const gpos *p, ai_rng *r, int eps)
{
    int pl = p->turn;
    float now = g_guess(p, pl);
    switch (p->phase) {
    case GP_REVEAL: {
        int L[GOLF_SLOTS], k = 0;
        for (int s = 0; s < p->slots; s++)
            if (alive(p, pl, s) && !up(p, pl, s)) L[k++] = s;
        return k ? GA_REVEAL | L[ai_rand_n(r, k)] : AI_NONE;
    }
    case GP_DRAW: {
        if (!p->npile) return GA_STOCK;
        float sc;
        int s = best_slot(p, pl, p->pile[p->npile - 1], &sc);
        if (s >= 0 && now - sc > (p->variant == GOLF_NUMBER ? 3.5f : 2.5f) && !ends_badly(p, pl, s) &&
            ai_rand_n(r, 100) >= eps)
            return GA_TAKE;
        return golf_legal(p, GA_STOCK) ? GA_STOCK : GA_TAKE;
    }
    case GP_HOLD: {
        float sc;
        int s = best_slot(p, pl, p->held, &sc);
        if (s >= 0 && ends_badly(p, pl, s) && !p->taken) s = -1;
        if (eps && ai_rand_n(r, 100) < eps) {
            int L[GOLF_SLOTS], k = 0;
            for (int x = 0; x < p->slots; x++)
                if (alive(p, pl, x)) L[k++] = x;
            if (k) s = L[ai_rand_n(r, k)];
        }
        if (s >= 0 && (p->taken || sc < now - 0.5f)) return GA_SWAP | s;
        if (!p->taken) return GA_DISCARD;
        return GA_SWAP | (s >= 0 ? s : 0);
    }
    case GP_FLIP: {
        int L[GOLF_SLOTS], k = 0, safe[GOLF_SLOTS], ks = 0;
        for (int s = 0; s < p->slots; s++)
            if (alive(p, pl, s) && !up(p, pl, s)) {
                L[k++] = s;
                if (!ends_badly(p, pl, s)) safe[ks++] = s;
            }
        if (ks) return GA_FLIP | safe[ai_rand_n(r, ks)];
        return k ? GA_FLIP | L[ai_rand_n(r, k)] : AI_NONE;
    }
    default: return AI_NONE;
    }
}

static int g_playout(const void *s, ai_rng *r) { return g_policy(&((const gf_st *)s)->p, r, 8); }

/* ---- the view and the deal of the unseen ------------------------------------------------------------------------ */
static void g_determinise(void *s, int seat, ai_rng *r)
{
    (void)seat;
    gf_st *st = s;
    gpos *p = &st->p;
    uint8_t pack[CARDS_MAX * 2], seen[CARDS_MAX * 3], pool[CARDS_MAX * 2];
    int npack = cards_pack(p->variant == GOLF_NUMBER ? PACK_NUMBER : PACK_52, pack), ns = 0;
    if (p->variant == GOLF_CLASSIC && p->players > 4) npack += cards_pack(PACK_52, pack + npack);
    for (int q = 0; q < p->players; q++)
        for (int sl = 0; sl < p->slots; sl++)
            if (alive(p, q, sl)) seen[ns++] = p->card[q][sl];
    for (int i = 0; i < p->npile; i++) seen[ns++] = p->pile[i];
    if (p->held != CARD_NONE) seen[ns++] = p->held;
    int np = cdb_unseen(pack, npack, seen, ns, pool);
    cdb_shuffle(pool, np, r);
    for (int q = 0; q < p->players; q++)
        for (int sl = 0; sl < p->slots; sl++)
            if (p->card[q][sl] == CDB_HIDDEN && np > 0) p->card[q][sl] = pool[--np];
    cdb_fill(p->stock, p->nstock, pool, &np);
    p->seed = ai_rand(r);
}

/* the seat's view: every card face down hidden (the seat's own too), the stock hidden, the seed gone */
static void g_view(const gpos *live, int me, gf_st *v)
{
    v->p = *live;
    v->me = (uint8_t)me;
    gpos *p = &v->p;
    for (int q = 0; q < p->players; q++)
        for (int sl = 0; sl < p->slots; sl++)
            if (alive(p, q, sl) && !up(p, q, sl)) p->card[q][sl] = CDB_HIDDEN;
    memset(p->stock, CDB_HIDDEN, (size_t)p->nstock);
    p->seed = 0;
}

int glfb_choose(const gpos *live, int seat, int level, uint64_t seed, ai_stats *stats)
{
    if (stats) memset(stats, 0, sizeof *stats);
    if (live->phase == GP_ROUND) return GA_NEXT;
    if (live->phase == GP_OVER || live->turn != seat) return 0;
    gf_st *v = malloc(sizeof *v);
    if (!v) return golf_auto(live);
    g_view(live, seat, v);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)live->up[seat] * 131 + live->npile * 7 + live->nstock));
    int mv[AI_MAX_MOVES], n = g_moves(v, mv), a = n ? mv[0] : 0;
    if (n > 1) {
        if (cdb_slip(level, &r)) {
            gf_st w = *v;
            g_determinise(&w, seat, &r);
            a = g_policy(&w.p, &r, CDB_SLIP_EPS);
        } else {
            ai_game g = {
                .size = sizeof(gf_st), .moves = g_moves, .make = g_make, .side = g_side, .players = live->players,
                .result = g_result, .playout = g_playout, .estimate = g_estimate, .cutoff = GOLF_CUTOFF,
                .determinise = g_determinise,
            };
            long hard = live->variant == GOLF_NUMBER ? GOLF_HARD_ITERS / 2 : GOLF_HARD_ITERS;
            int m = cdb_search(&g, v, level, hard, &r, stats);
            if (m != AI_NONE) a = m;
        }
    }
    free(v);
    return a > 0 && golf_legal(live, a) ? a : golf_auto(live);
}

static int think(int seat, int level, char *move, int cap)
{
    ggame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *golf_live();                          /* the copy, at entry: nothing else is read */
    int a = glfb_choose(&g->pos, seat, level, ai_mix(ai_session_seed(), (uint64_t)g->n), NULL), ok = 1;
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    int s = GA_SLOT(a);
    switch (GA_KIND(a)) {
    case GA_REVEAL: snprintf(move, (size_t)cap, "r%d", s); break;
    case GA_STOCK: snprintf(move, (size_t)cap, "s"); break;
    case GA_TAKE: snprintf(move, (size_t)cap, "t"); break;
    case GA_SWAP: snprintf(move, (size_t)cap, "w%d", s); break;
    case GA_DISCARD: snprintf(move, (size_t)cap, "d"); break;
    case GA_FLIP: snprintf(move, (size_t)cap, "f%d", s); break;
    case GA_NEXT: snprintf(move, (size_t)cap, "n"); break;
    default: ok = 0;
    }
    return ok;
}

static int actor(void)
{
    const gpos *p = &golf_live()->pos;
    return p->phase == GP_ROUND || p->phase == GP_OVER ? -1 : p->turn;
}

static const bot golf_bot = {.think = think, .actor = actor, .play = golf_show};
REGISTER_BOT(golf, &golf_bot)
