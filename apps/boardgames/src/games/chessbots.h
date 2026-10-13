/*
 * The chess family's bots (chess and its variants, makruk, shogi, xiangqi, janggi, jungle, banqi): what their
 * adapters share (static inline, no state). Each bot's search state is a plain struct: the rules' position, its key,
 * the keys of the positions before it (repetitions), its ply from the root, and an "end" flag:
 *   - a position with no legal move gets one pseudo-move, CB_END, that changes nothing but the flag: the child is then
 *     over() (mate, stalemate, a player stuck) with the game's score, at its true ply, without generating the moves
 *     twice (over() never generates moves);
 *   - a repetition inside the search (the same key, the same player to move, since the last irreversible move) is
 *     scored at once (a draw, or the game's own rule); never at the root (the search must still pick a move there).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef CHESSBOTS_H
#define CHESSBOTS_H

#include <stdint.h>
#include <string.h>

#include "ai.h"

#define CB_END  (1 << 24)                       /* the pseudo-move of a position with no legal move */
#define CB_HIST 32                              /* the keys kept for repetitions (a power of two) */

/* a 64-bit finaliser (splitmix64's): the keys of a square's piece */
static inline uint64_t cb_mix(uint64_t x)
{
    x = (x ^ x >> 30) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ x >> 27) * 0x94d049bb133111ebull;
    return x ^ x >> 31;
}

/* a board's key: each occupied square's piece; salt keeps the games and the extra fields apart */
static inline uint64_t cb_board_key(const uint8_t *sq, int n, uint64_t salt)
{
    uint64_t h = salt;
    for (int s = 0; s < n; s++)
        if (sq[s]) h ^= cb_mix(0x9e3779b97f4a7c15ull * (uint64_t)(s * 256 + sq[s] + 1));
    return h;
}

/* the keys of the positions before this one, the parent last (a ring) */
typedef struct {
    uint8_t n, head;                            /* kept (<= CB_HIST); the next slot */
    uint64_t k[CB_HIST];
} cb_hist;

static inline void cb_hist_clear(cb_hist *h) { h->n = h->head = 0; }

static inline void cb_hist_push(cb_hist *h, uint64_t key)
{
    h->k[h->head] = key;
    h->head = (uint8_t)((h->head + 1) & (CB_HIST - 1));
    if (h->n < CB_HIST) h->n++;
}

/* the position (its key) already seen with the same player to move, within `back` plies (the last irreversible
 * move); every second ply when the turns alternate (step 2), every ply when they may not (step 1) */
static inline int cb_hist_seen(const cb_hist *h, uint64_t key, int back, int step)
{
    int n = back < h->n ? back : h->n;
    for (int d = step; d <= n; d += step)
        if (h->k[(h->head - d) & (CB_HIST - 1)] == key) return 1;
    return 0;
}

/* most valuable victim, least valuable attacker: a capture's order (values in the eval's units) */
static inline int cb_mvvlva(int victim, int attacker) { return 1000 + victim * 8 - attacker / 8; }

static inline int cb_abs(int x) { return x < 0 ? -x : x; }
static inline int cb_max(int a, int b) { return a > b ? a : b; }
static inline int cb_min(int a, int b) { return a < b ? a : b; }
static inline int cb_cheb(int ax, int ay, int bx, int by) { return cb_max(cb_abs(ax - bx), cb_abs(ay - by)); }

/* the levels for the family: easy a shallow search with noise (it sees a capture, misses deeper ideas, sometimes
 * prefers a slightly worse move), medium a few plies and a little noise, hard the node budget (about 0.3 s on a PC;
 * the kit's ms cap stays the phone's safety net) */
typedef struct { long nodes[AI_LEVELS]; int depth[AI_LEVELS], noise[AI_LEVELS]; int tt_bits; } cb_levels;

static inline ai_opts cb_opts(const cb_levels *L, int level, ai_rng *rng)
{
    int l = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    ai_opts o = {.budget = ai_level_budget(l), .rng = rng, .tt_bits = L->tt_bits ? L->tt_bits : 18};
    /* the clock is read every 1024 nodes and these games' nodes are slow (a legal move list each): a tenth of the
     * time kept in hand, so a stop never runs past the level's limit */
    o.budget.ms = o.budget.ms * 9 / 10;
    o.budget.nodes = L->nodes[l];
    o.budget.depth = L->depth[l];
    o.budget.noise = L->noise[l];
    return o;
}

#endif
