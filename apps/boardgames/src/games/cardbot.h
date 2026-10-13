/*
 * What the card bots of the shedding, fishing and betting games share (crazy eights, president, golf, durak, big two,
 * tiến lên, scopa, koi-koi, mus, truco): the levels' work, a shuffle, the search run. No SDL; static inline, so each
 * bot file takes it alone. The trick games have their own core (trickbot.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The pattern each bot follows (bots.h's thread rules):
 *   - think() copies the live game at entry, builds the seat's VIEW: the rules' own position with every card the seat
 *     cannot see replaced by CDB_HIDDEN (the others' hands, the stock), and its seed replaced (a deal's seed would
 *     tell the stock's order and the next reshuffle);
 *   - the search (ai_mcts) gets determinise(): the unseen cards (the pack minus what the seat has seen) dealt at
 *     random into the hidden places, consistently with what the seat knows (cards it saw picked up, its own passes);
 *   - moves are played with the rules' own functions on a copy of the position (never the live game).
 * Levels: hard runs the game's hard_iters (calibrated to about 0.3 s a move on a PC), medium a sixth, easy a fortieth
 * (50 to 200) and, a third of the time, the playouts' quick heuristic move itself, a little careless (a human-like
 * slip).
 */
#ifndef CARDBOT_H
#define CARDBOT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"

#define CDB_HIDDEN 254                          /* a card the seat cannot see (CARD_NONE is 255) */

static inline long cdb_iters(int level, long hard)
{
    if (level <= AI_EASY) return hard / 40 < 50 ? 50 : hard / 40 > 200 ? 200 : hard / 40;
    if (level == AI_MEDIUM) return hard / 6 > 200 ? hard / 6 : 200;
    return hard;
}

/* the easy level's slips: a third of its moves are the quick heuristic's, played with some carelessness (a random
 * move CDB_SLIP_EPS percent of the time) */
#define CDB_SLIP_EPS 25
static inline int cdb_slip(int level, ai_rng *r) { return level <= AI_EASY && ai_rand_n(r, 100) < 33; }

static inline void cdb_shuffle(uint8_t *a, int n, ai_rng *r)
{
    for (int i = n - 1; i > 0; i--) {
        int j = ai_rand_n(r, i + 1);
        uint8_t t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}

/* the pack minus the cards seen (each seen card takes one copy out: packs of pairs); the rest into out, its count */
static inline int cdb_unseen(const uint8_t *pack, int npack, const uint8_t *seen, int nseen, uint8_t *out)
{
    uint8_t cnt[256];
    memset(cnt, 0, sizeof cnt);
    for (int i = 0; i < npack; i++) cnt[pack[i]]++;
    for (int i = 0; i < nseen; i++)
        if (seen[i] < CDB_HIDDEN && cnt[seen[i]]) cnt[seen[i]]--;
    int n = 0;
    for (int c = 0; c < 256; c++)
        for (int k = 0; k < cnt[c]; k++) out[n++] = (uint8_t)c;
    return n;
}

/* fills the CDB_HIDDEN places of a[0..n-1] from the pool (taken from its end); the places left when it runs dry keep
 * CDB_HIDDEN (a faulty view: never stall) */
static inline void cdb_fill(uint8_t *a, int n, uint8_t *pool, int *npool)
{
    for (int i = 0; i < n; i++)
        if (a[i] == CDB_HIDDEN && *npool > 0) a[i] = pool[--*npool];
}

/* the search: ai_mcts with the level's iterations (the AI kit's ms caps stay the safety net) */
static inline int cdb_search(const ai_game *g, const void *root, int level, long hard, ai_rng *rng, ai_stats *stats)
{
    ai_opts o = {.budget = ai_level_budget(level), .rng = rng, .uct_c = 0.7f};
    o.budget.nodes = cdb_iters(level, hard);
    return ai_mcts(g, root, &o, stats);
}

#endif
