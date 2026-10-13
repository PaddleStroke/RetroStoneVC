/*
 * Monte Carlo tree search with RAVE (rapid action value estimation, "all moves as first") for the games where a
 * stone, once set down, stays and its point's worth hardly depends on when it was played: go and hex. The bots of
 * those two games share it (go_bot.c, hex_bot.c); static functions, no state outside the search's own allocations.
 * It needs what ai.h's ai_mcts lacks for them: the playouts' moves credited to the tree's same moves (RAVE: a few
 * thousand playouts already rank the moves), priors from the game's knowledge (virtual wins and visits), and nodes
 * expanded in one go with all their moves.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The game gives (two players, 0 and 1; moves 0..moves_max-1):
 *   expand(st, mv, prior, weight): the moves of the tree at this state, each with a prior win rate (0..1) for the
 *       player to move and a weight (virtual playouts at that rate; 0: none); their count (0: the game is over).
 *   make(st, m), side(st), over(st, &winner): a move; the player to move; the game over (1, the winner).
 *   playout(st, rng, mv, pl, cap, &len): plays on to the end, writes the moves played and their players (up to cap,
 *       the moves that count for RAVE only), returns the winner.
 * The search runs `iterations` playouts (the deterministic budget), stopped earlier by `ms` or ai_cancelled().
 */
#ifndef RAVE_H
#define RAVE_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"

typedef struct {
    size_t size;
    int moves_max;
    int (*expand)(const void *st, int *mv, float *prior, float *weight);
    void (*make)(void *st, int m);
    int (*side)(const void *st);
    int (*over)(const void *st, int *winner);
    int (*playout)(void *st, ai_rng *r, int *mv, uint8_t *pl, int cap, int *len);
    float rave_k;                               /* RAVE weighs rn / (rn + n + rn n / rave_k): a half at rave_k visits */
    float explore;                              /* UCT's exploration term (0: none, RAVE explores) */
    int expand_at;                              /* a leaf grows its children at this many visits */
} rave_game;

typedef struct {
    int move, first, count, visits;             /* first: the first child (count of them; 0: a leaf) */
    float n, w, rn, rw;                         /* visits and wins (priors in), RAVE's visits and wins */
} rave_node;

typedef struct {
    rave_node *t;
    long used, cap;
} rave_tree;

static int rave_grow(rave_tree *T, int k)       /* room for k more nodes; 0: none (the tree stops growing) */
{
    if (T->used + k <= T->cap) return 1;
    long cap = T->cap ? T->cap * 2 : 4096;
    while (cap < T->used + k) cap *= 2;
    if (cap > (1l << 22)) return 0;             /* about 130 MB: enough */
    rave_node *t = realloc(T->t, sizeof *t * (size_t)cap);
    if (!t) return 0;
    T->t = t;
    T->cap = cap;
    return 1;
}

static int rave_expand(const rave_game *g, rave_tree *T, long node, const void *st, int *mv, float *pr, float *wt)
{
    int k = g->expand(st, mv, pr, wt);
    if (k <= 0 || !rave_grow(T, k)) return 0;
    rave_node *nd = &T->t[node];
    nd->first = (int)T->used;
    nd->count = k;
    for (int i = 0; i < k; i++) {
        rave_node *c = &T->t[T->used++];
        memset(c, 0, sizeof *c);
        c->move = mv[i];
        c->first = -1;
        c->n = wt[i];
        c->w = wt[i] * pr[i];
    }
    return k;
}

static long rave_pick(const rave_game *g, const rave_tree *T, long node)
{
    const rave_node *nd = &T->t[node];
    float lp = nd->visits > 0 ? logf((float)nd->visits + 1) : 0;
    long best = nd->first;
    float bv = -1e30f;
    for (int i = 0; i < nd->count; i++) {
        const rave_node *c = &T->t[nd->first + i];
        float q = c->n > 0 ? c->w / c->n : 0.5f;
        float v = q;
        if (c->rn > 0) {
            float beta = c->rn / (c->rn + c->n + c->rn * c->n / g->rave_k);   /* the minimum-error schedule */
            v = (1 - beta) * q + beta * (c->rw / c->rn);
        }
        if (g->explore > 0) v += g->explore * sqrtf(lp / (c->n + 1));
        if (v > bv) { bv = v; best = nd->first + i; }
    }
    return best;
}

/* the search: the move to play (-1: none), its win rate in *rate (0..1); visits: each root move's visits (moves_max
 * of them, may be NULL) */
static int rave_search(const rave_game *g, const void *root, long iterations, int ms, ai_rng *r, float *rate,
                       int *visits, ai_stats *stats)
{
    double t0 = ai_now_ms();
    rave_tree T = {0};
    int depth_cap = 1024, best = -1;
    int *mv = malloc(sizeof(int) * (size_t)g->moves_max * 2);
    float *pr = malloc(sizeof(float) * (size_t)g->moves_max * 2), *wt = pr ? pr + g->moves_max : NULL;
    long *path = malloc(sizeof(long) * (size_t)depth_cap);
    uint8_t *path_side = malloc((size_t)depth_cap);
    int *seq = malloc(sizeof(int) * 4096);
    uint8_t *seq_pl = malloc(4096);
    uint8_t *first = malloc((size_t)g->moves_max);
    uint8_t *st = malloc(g->size);
    if (rate) *rate = 0.5f;
    if (stats) memset(stats, 0, sizeof *stats);
    if (!mv || !pr || !path || !path_side || !seq || !seq_pl || !first || !st || !rave_grow(&T, 1)) goto done;
    T.t[0] = (rave_node){.move = -1, .first = -1};
    T.used = 1;
    if (!rave_expand(g, &T, 0, root, mv, pr, wt)) goto done;
    long it = 0;
    for (; it < iterations; it++) {
        if ((it & 31) == 0 && (ai_cancelled() || (ms > 0 && ai_now_ms() - t0 >= ms))) break;
        memcpy(st, root, g->size);
        long node = 0;
        int depth = 0, winner, len = 0;
        path[depth] = 0;
        path_side[depth] = (uint8_t)g->side(st);
        /* down the tree */
        int ended = 0;
        for (;;) {
            rave_node *nd = &T.t[node];
            if (!nd->count) {
                if (node && nd->visits + 1 >= g->expand_at && !g->over(st, &winner))
                    rave_expand(g, &T, node, st, mv, pr, wt);
                nd = &T.t[node];
                if (!nd->count) break;
            }
            long c = rave_pick(g, &T, node);
            int m = T.t[c].move;
            if (len < 4096) { seq[len] = m; seq_pl[len] = path_side[depth]; len++; }
            g->make(st, m);
            node = c;
            if (depth + 1 >= depth_cap) break;
            path[++depth] = node;
            path_side[depth] = (uint8_t)g->side(st);
            if (g->over(st, &winner)) { ended = 1; break; }
        }
        int tree_len = len, plen = 0;
        if (!ended) {
            winner = g->playout(st, r, seq + len, seq_pl + len, 4096 - len, &plen);
            len += plen;
        }
        /* up: each node's visits and wins (for the player who moved into it), and its children's RAVE: the moves
         * the player to move there played first afterwards */
        memset(first, 0, (size_t)g->moves_max);
        for (int i = len - 1; i >= tree_len; i--)
            if (seq[i] >= 0 && seq[i] < g->moves_max) first[seq[i]] = (uint8_t)(seq_pl[i] + 1);
        for (int d = depth; d >= 0; d--) {
            rave_node *nd = &T.t[path[d]];
            nd->visits++;
            if (d) {
                nd->n += 1;
                nd->w += winner == path_side[d - 1];
            }
            if (d < tree_len && seq[d] >= 0 && seq[d] < g->moves_max) first[seq[d]] = (uint8_t)(seq_pl[d] + 1);
            int me = path_side[d] + 1;
            for (int i = 0; i < nd->count; i++) {
                rave_node *c = &T.t[nd->first + i];
                if (c->move >= 0 && c->move < g->moves_max && first[c->move] == me) {
                    c->rn += 1;
                    c->rw += winner == path_side[d];
                }
            }
        }
    }
    /* the root's most visited move */
    rave_node *rt = &T.t[0];
    int bv = -1;
    for (int i = 0; i < rt->count; i++) {
        rave_node *c = &T.t[rt->first + i];
        if (visits && c->move >= 0 && c->move < g->moves_max) visits[c->move] = c->visits;
        if (c->visits > bv) {
            bv = c->visits;
            best = c->move;
            if (rate) *rate = c->n > 0 ? c->w / c->n : 0.5f;
        }
    }
    if (stats) {
        stats->nodes = it;
        stats->score = rate ? (int)lroundf(*rate * 100) : 0;
        stats->ms = ai_now_ms() - t0;
        stats->stopped = it < iterations;
    }
done:
    free(T.t); free(mv); free(pr); free(path); free(path_side); free(seq); free(seq_pl); free(first); free(st);
    return best;
}

#endif
