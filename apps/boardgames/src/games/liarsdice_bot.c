/*
 * Liar's Dice's bot (bots.h), two to six players. It sees what its seat sees: its own dice, how many dice each player
 * has left and the bids of the round; never the others' dice (nor the round's seed they come from).
 * The chance that a bid holds, from its seat: its own dice of the face and its 1s (wild), and each die it can't see a
 * third likely to count (the face or a 1). A call loses if the bid holds; a raise loses if the next player calls it
 * and it does not hold, and the next player calls a bid the more readily the less likely it looks without our dice.
 * It calls when calling risks less than its safest raise (the face its dice back best, raised by the least).
 * Opening a round it bids its best face at the count that still holds three times in four.
 *   easy    no reading of the others' bids, its sense of the risks blurred (a call or a raise on a whim, now and
 *           then a bid on a face it lacks);
 *   medium  that reckoning, sharp;
 *   hard    also reads the bids: a player who bid a face likely holds one of it (its last bid's face, a die each).
 * After a call: "n", the next round (the caller has the cups lifted and starts it when the reveal is over).
 * Runs on the worker thread: it copies its own view of the position at entry and touches nothing else (bots.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: "3x4" a bid (at least three fours), "l" a call, "n" the next round.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/liarsdice.h"

/* what a seat knows: its own dice, the dice left, the bids */
typedef struct {
    int me, players, total, mine[7], ndice[LD_MAXP];
    int count, face, bidder;
    int read[7];                                /* hard: the dice read from the others' bids, by face */
} view;

static void view_of(const lpos *p, int me, int reading, view *v)
{
    memset(v, 0, sizeof *v);
    v->me = me;
    v->players = p->players;
    v->total = ld_total(p);
    for (int q = 0; q < p->players; q++) v->ndice[q] = p->ndice[q];
    for (int i = 0; i < p->ndice[me]; i++) v->mine[p->dice[me][i]]++;   /* its own cup only */
    v->count = p->count;
    v->face = p->face;
    v->bidder = p->bidder;
    if (!reading) return;
    for (int q = 0; q < p->players; q++) {      /* each other player's last bid this round: a die of that face */
        if (q == me || !p->ndice[q]) continue;
        for (int i = p->nbids - 1; i >= 0; i--)
            if (p->bids[i].player == q) {
                v->read[p->bids[i].face]++;
                break;
            }
    }
}

/* P(X >= k), X binomial(n, 1/3) */
static double at_least(int n, int k)
{
    if (k <= 0) return 1;
    if (k > n) return 0;
    double p = 0, term = pow(2.0 / 3, n);       /* P(X = 0) */
    for (int x = 0; x <= n; x++) {
        if (x >= k) p += term;
        term *= (double)(n - x) / (x + 1) * 0.5;   /* (n - x)/(x + 1) * (1/3)/(2/3) */
    }
    return p;
}

/* the chance that "count x face" holds, from the seat */
static double holds(const view *v, int count, int face)
{
    int read = 0;
    for (int f = 2; f <= 6; f++) read += v->read[f];
    int known = v->mine[face] + v->mine[1] + v->read[face];
    int unseen = v->total - v->ndice[v->me] - read;
    return at_least(unseen < 0 ? 0 : unseen, count - known);
}

/* the chance a bid looks true to someone who sees none of our dice (their own: a third each, like the rest) */
static double looks(const view *v, int count) { return at_least(v->total, count); }

/* the least raise on each face; the one most likely to hold (-1: none) */
static int safest_raise(const view *v, int *count, double *pr, ai_rng *r, int wild)
{
    int best = -1;
    double bp = -1;
    for (int f = 2; f <= 6; f++) {
        int c = !v->count ? 1 : f > v->face ? v->count : v->count + 1;
        if (c > v->total) continue;
        double p = holds(v, c, f) + (wild ? 0.25 * ai_rand_f(r) : 0) + 1e-6 * f;
        if (p > bp) bp = p, best = f, *count = c;
    }
    if (best >= 0) *pr = holds(v, *count, best);
    return best;
}

static int think(int seat, int level, char *move, int cap)
{
    const lpos *live = &liarsdice_live()->pos;   /* at entry: its own view (view_of), the phase, the round */
    int phase = live->phase, turn = live->turn, rnd = live->round, nbids = live->nbids, have = live->ndice[seat];
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    view v;
    view_of(live, seat, level == AI_HARD, &v);
    if (cap < 8 || turn != seat) return 0;
    if (phase == LP_REVEAL) {
        strcpy(move, "n");
        return 1;
    }
    if (phase != LP_BID || !have) return 0;
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), ai_mix(rnd, (uint64_t)nbids * 8 + seat)));
    int easy = level == AI_EASY;
    if (!v.count) {                             /* opening: its best face, at the count that holds 3 times in 4 */
        int face = 2;
        for (int f = 3; f <= 6; f++)
            if (v.mine[f] >= v.mine[face]) face = f;
        if (easy && ai_rand_n(&r, 4) == 0) face = 2 + ai_rand_n(&r, 5);
        int c = 1;
        while (c + 1 <= v.total && holds(&v, c + 1, face) >= (easy ? 0.5 + 0.4 * ai_rand_f(&r) : 0.75)) c++;
        snprintf(move, (size_t)cap, "%dx%d", c, face);
        return !ai_cancelled();
    }
    int c = 0;
    double pb = 0, p_last = holds(&v, v.count, v.face);
    int f = safest_raise(&v, &c, &pb, &r, easy && ai_rand_n(&r, 5) < 2);
    double lose_call = p_last, lose_raise = 1;
    if (f >= 0) {
        double call_next = 0.25 + 0.65 * (1 - looks(&v, c));   /* how readily the next player calls it */
        lose_raise = (1 - pb) * call_next;
    }
    if (easy) lose_raise *= 0.3 + 1.4 * ai_rand_f(&r);
    if (easy && ai_rand_n(&r, 10) == 0) lose_raise = 2 * ai_rand_f(&r);   /* a call or a raise on a whim */
    if (f >= 0 && lose_raise <= lose_call) snprintf(move, (size_t)cap, "%dx%d", c, f);
    else strcpy(move, "l");
    return !ai_cancelled();
}

static const bot liarsdice_bot = {.think = think, .play = liarsdice_show};
REGISTER_BOT(liarsdice, &liarsdice_bot)
