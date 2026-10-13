/*
 * What the capture games' bots share (draughts, lasca, alquerque, fanorona, konane, breakthrough, surakarta, bagh-chal,
 * fox and geese, tafl: games/<id>_bot.c): a position's key from its bytes, the levels' budgets, and a whole turn
 * searched move by move (a chain of captures: each jump a move of the rules, the side to move unchanged until it
 * ends). Pure functions over the AI kit (ai.h): safe on the bot's worker thread (bots.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef CAPTURE_BOTS_H
#define CAPTURE_BOTS_H

#include <stdint.h>
#include <string.h>

#include "ai.h"

#define CPB_TURN 40                             /* the moves of one turn, at most (a chain's jumps) */

/* a key for n bytes (the transposition table's): eight at a time through ai_mix */
static inline uint64_t cpb_hash(const void *p, size_t n, uint64_t h)
{
    const uint8_t *b = p;
    while (n >= 8) {
        uint64_t w;
        memcpy(&w, b, 8);
        h = ai_mix(h, w);
        b += 8;
        n -= 8;
    }
    uint64_t w = 0;
    memcpy(&w, b, n);
    return ai_mix(h, w ^ n);
}

/* the positions before the one searched that it may come back to (since the last capture or move that cannot be
 * undone), as their keys: a position seen again in the search is scored as the game's repetition rule scores it (a
 * draw, mostly), so a bot ahead makes progress instead of shuffling, and a bot behind may take the draw. The root
 * itself is never scored so (ply 0). Part of a bot's search state (a copy: no pointers). */
#define CPB_REP 16
typedef struct {
    uint64_t key[CPB_REP];
    uint8_t n, at, ply;
} cpb_rep;

static inline void cpb_rep_clear(cpb_rep *r) { r->n = r->at = 0; }

static inline void cpb_rep_push(cpb_rep *r, uint64_t key)   /* the position being left */
{
    r->key[r->at] = key;
    r->at = (uint8_t)((r->at + 1) % CPB_REP);
    if (r->n < CPB_REP) r->n++;
    if (r->ply < 255) r->ply++;
}

static inline int cpb_rep_seen(const cpb_rep *r, uint64_t key)
{
    if (!r->ply) return 0;
    for (int i = 0; i < r->n; i++)
        if (r->key[i] == key) return 1;
    return 0;
}

/* a level's search: easy a shallow look and noise (it takes what hangs, misses deeper ideas, now and then prefers a
 * worse move), medium a few plies and a little noise, hard the game's node budget (about 0.3 s on a PC) */
typedef struct {
    int easy_depth, medium_depth;
    long easy_nodes, medium_nodes, hard_nodes;
    int easy_noise, medium_noise;               /* hundredths of the eval's unit */
    int tt_bits;
    int chains;                                 /* 1: a capture may go on (the same side moves again) */
} cpb_levels;

static inline ai_opts cpb_opts(const cpb_levels *L, int level, ai_rng *rng)
{
    ai_opts o = {.budget = ai_level_budget(level), .rng = rng, .tt_bits = L->tt_bits};
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:
        o.budget.depth = L->easy_depth;
        o.budget.nodes = L->easy_nodes;
        o.budget.noise = L->easy_noise;
        break;
    case AI_MEDIUM:
        o.budget.depth = L->medium_depth;
        o.budget.nodes = L->medium_nodes;
        o.budget.noise = L->medium_noise;
        break;
    default:
        o.budget.nodes = L->hard_nodes;
        break;
    }
    return o;
}

/* the whole turn of the side to move in st (played into st): a search for each move while the same side is to move
 * (a chain), all within the level's budget (a move with no other choice is played without one; a turn that may go on
 * after its first capture keeps two fifths of the budget for the rest); the moves in out (at most max); their count,
 * 0 if none or cancelled */
static inline int cpb_turn(const ai_game *g, void *st, const cpb_levels *L, int level, uint64_t seed, int *out,
                          int max, ai_stats *stats)
{
    int n = 0, me = g->side(st), score, mv[AI_MAX_MOVES];
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, g->hash ? g->hash(st) : 0));
    if (stats) memset(stats, 0, sizeof *stats);
    ai_opts all = cpb_opts(L, level, &rng);
    double end = all.budget.ms > 0 ? ai_now_ms() + all.budget.ms * 0.9 : 0;   /* (a margin: each search sets up) */
    long nodes = all.budget.nodes, limited = nodes > 0;
    mv[0] = AI_NONE;
    while (n < max && !g->over(st, &score) && g->side(st) == me) {
        int count = g->moves(st, mv), m = count ? mv[0] : AI_NONE;
        if (count > 1) {
            ai_opts o = all;
            int part = L->chains && !n && g->noisy && g->noisy(st, mv) > 0;   /* captures now: a chain may follow */
            if (limited) o.budget.nodes = nodes < 200 ? 200 : part ? nodes * 3 / 5 : nodes;
            if (end > 0) {
                double left = end - ai_now_ms();
                o.budget.ms = left < 1 ? 1 : part ? (int)(left * 3 / 5) + 1 : (int)left + 1;
            }
            ai_stats s;
            m = ai_ab(g, st, &o, &s);
            nodes -= s.nodes;
            if (stats) {                        /* the turn's work: every search of it */
                stats->nodes += s.nodes;
                stats->ms += s.ms;
                if (!stats->depth) stats->depth = s.depth, stats->score = s.score;
                stats->stopped |= s.stopped;
            }
        }
        if (m < 0 || ai_cancelled()) return 0;
        out[n++] = m;
        g->make(st, m);
    }
    return n;
}

#endif
