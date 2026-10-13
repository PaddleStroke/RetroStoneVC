/*
 * Shut the Box's bot (bots.h), every row of tiles (1..9, 1..10, 1..12) and one to four players. The throw is a move,
 * "d??" (two dice) or "d?" (one die, once the tiles 7 and up are shut), played by shutthebox_show() with the game's
 * own dice, as a tap on the felt throws them; after a throw the bot chooses the tiles to shut ("8", "1+7").
 *   easy    a player's habit, loosely kept: the fewest tiles, the highest among them; now and then any set at
 *           random, and one die or two on a whim;
 *   medium  that habit, kept: the fewest tiles, the highest first; one die once the open tiles add up to 6 or less;
 *   hard    the whole turn worked out (every set of open tiles a position: 512, 1024 or 4096 of them): each choice,
 *           tiles and dice, the one with the best chance of winning. Alone: the chance of shutting the box. With
 *           others: the scores already made (to beat), and for each player still to play after us the spread of
 *           scores a good player makes (the expected-score player's, worked out the same way), so that the first
 *           player plays for a low score and the last one plays to beat the best one exactly.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation (shutthebox.h), the faces left to the game: "d??" or "d?" a throw, "1+7" tiles.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/shutthebox.h"

#define STATES (1 << (STB_MAX_TILES + 1))       /* an open set: bits 1..12 */
#define MAX_SUM 78                              /* 1 + 2 + ... + 12 */

/* every set of tiles making 1..12, by its sum (a set of distinct tiles <= 12 adding up to 12 at most) */
typedef struct {
    unsigned set[13][24];
    int n[13];
} sets;

static void sets_make(sets *s, int tiles)
{
    memset(s, 0, sizeof *s);
    for (unsigned m = 2; m < 1u << (tiles + 1); m += 2) {
        int t = stb_sum(m);
        if (t <= 12 && s->n[t] < 24) s->set[t][s->n[t]++] = m;
    }
}

/* the throw's sums: two dice (2..12, in 36ths) and one (1..6, in 6ths) */
static const int TWO[13] = {0, 0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1};

static int one_die_ok(unsigned open) { return !(open & ~stb_full(6)); }

/* ---- the turn worked out ------------------------------------------------------------------------------------------
 * v[m]: what the open set m is worth at a throw, played well; u[s]: a turn ended on s points; box: the box shut.
 * Every set m reached from `open` (its subsets) in order (a subset is a smaller number). */
typedef struct {
    double v[STATES];
    double u[MAX_SUM + 1], box;
    const sets *s;
} plan;

/* the best set to shut for the sum t from m (-1: none), and its worth */
static double best_shut(const plan *P, unsigned m, int t, unsigned *pick)
{
    double best = -1e18;
    int found = 0;
    for (int i = 0; i < P->s->n[t]; i++) {
        unsigned set = P->s->set[t][i];
        if (set & ~m) continue;
        double w = (m & ~set) ? P->v[m & ~set] : P->box;
        if (!found || w > best) best = w, found = 1, *pick = set;
    }
    if (!found) {
        *pick = 0;
        return P->u[stb_sum(m)];
    }
    return best;
}

static double throw_worth(const plan *P, unsigned m, int nd)
{
    double w = 0;
    unsigned pick;
    if (nd == 1)
        for (int t = 1; t <= 6; t++) w += best_shut(P, m, t, &pick) / 6;
    else
        for (int t = 2; t <= 12; t++) w += TWO[t] * best_shut(P, m, t, &pick) / 36;
    return w;
}

static void plan_solve(plan *P, unsigned open)
{
    for (unsigned m = 2; m <= open; m += 2) {
        if (m & ~open) continue;
        double w = throw_worth(P, m, 2);
        if (one_die_ok(m)) {
            double w1 = throw_worth(P, m, 1);
            if (w1 > w) w = w1;
        }
        P->v[m] = w;
    }
}

static int plan_dice(const plan *P, unsigned m)  /* the better throw: 1 or 2 dice */
{
    return one_die_ok(m) && throw_worth(P, m, 1) > throw_worth(P, m, 2) ? 1 : 2;
}

/* the spread of a whole turn's scores, from the full row, by the expected-score player: out[s] (out[0]: shut) */
static void spread(plan *P, int tiles, double *out)
{
    unsigned full = stb_full(tiles);
    for (int s = 0; s <= MAX_SUM; s++) P->u[s] = -s;
    P->box = 0;
    plan_solve(P, full);
    double *pr = calloc(STATES, sizeof *pr);
    memset(out, 0, sizeof(double) * (MAX_SUM + 1));
    if (!pr) return;
    pr[full] = 1;
    for (unsigned m = full; m >= 2; m -= 2) {
        if ((m & ~full) || pr[m] <= 0) continue;
        int nd = plan_dice(P, m);
        for (int t = nd == 1 ? 1 : 2; t <= (nd == 1 ? 6 : 12); t++) {
            double p = pr[m] * (nd == 1 ? 1.0 / 6 : TWO[t] / 36.0);
            unsigned pick;
            best_shut(P, m, t, &pick);
            if (pick) pr[m & ~pick] += p;
            else out[stb_sum(m)] += p;
        }
    }
    out[0] += pr[0];
    free(pr);
}

/* the worth of each ending for the player to move: its chance of winning (alone: of shutting the box) */
static void stakes(plan *P, const spos *p)
{
    int me = p->turn, later = 0, best = 1000;
    for (int q = 0; q < p->players; q++) {
        if (q == me) continue;
        if (p->played >> q & 1) best = stb_score(p, q) < best ? stb_score(p, q) : best;
        else later++;
    }
    double X[MAX_SUM + 1];
    if (later) spread(P, p->tiles, X);
    for (int s = 0; s <= MAX_SUM; s++) {
        double w = p->players == 1 ? 0 : s < best ? 1 : s == best ? 0.5 : 0;
        if (w > 0 && later) {
            double beat = 0;                    /* a later player ends above us (or level: half) */
            for (int x = 0; x <= MAX_SUM; x++) beat += x > s ? X[x] : x == s ? X[x] / 2 : 0;
            for (int k = 0; k < later; k++) w *= beat;
        }
        P->u[s] = w - s * 1e-6;                 /* the lower score among equal chances */
    }
    P->box = 1;
}

/* ---- the habit: the fewest tiles, the highest among them ------------------------------------------------------- */
static unsigned habit(const sets *S, unsigned open, int t, ai_rng *r, int wild)
{
    unsigned best = 0, any[24];
    int n = 0;
    for (int i = 0; i < S->n[t]; i++) {
        unsigned set = S->set[t][i];
        if (set & ~open) continue;
        any[n++] = set;
        int a = __builtin_popcount(set), b = __builtin_popcount(best);
        if (!best || a < b || (a == b && 31 - __builtin_clz(set) > 31 - __builtin_clz(best))) best = set;
    }
    if (n && wild) return any[ai_rand_n(r, n)];
    return best;
}

static int think(int seat, int level, char *move, int cap)
{
    spos p = shutthebox_live()->pos;            /* the copy, at entry: nothing else is read */
    if (p.phase == STB_OVER || p.turn != seat || cap < 32) return 0;
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), ai_mix(p.throws, (uint64_t)p.turn)));
    unsigned open = p.open[p.turn];
    sets *S = malloc(sizeof *S);
    plan *P = level >= AI_HARD ? malloc(sizeof *P) : NULL;
    if (!S || (level >= AI_HARD && !P)) {
        free(S);
        free(P);
        return 0;
    }
    sets_make(S, p.tiles);
    if (P) {
        P->s = S;
        stakes(P, &p);
        plan_solve(P, open);
    }
    int ok = 1;
    if (p.phase == STB_THROW) {
        int one = 0;
        if (stb_one_die(&p)) {
            if (P) one = plan_dice(P, open) == 1;
            else if (level == AI_MEDIUM) one = stb_sum(open) <= 6;
            else one = ai_rand_n(&r, 2);
        }
        snprintf(move, (size_t)cap, one ? "d?" : "d??");
    } else {
        int t = stb_total(&p);
        unsigned pick = 0;
        if (P) best_shut(P, open, t, &pick);
        else pick = habit(S, open, t, &r, level <= AI_EASY && ai_rand_n(&r, 3) == 0);
        if (!pick) ok = 0;                      /* (a throw with no set ends the turn by itself) */
        int len = 0;
        move[0] = 0;
        for (int k = 1; k <= STB_MAX_TILES && ok; k++)
            if (pick >> k & 1) len += snprintf(move + len, (size_t)(cap - len), len ? "+%d" : "%d", k);
    }
    free(S);
    free(P);
    return ok && !ai_cancelled();
}

static const bot shutthebox_bot = {.think = think, .play = shutthebox_show};
REGISTER_BOT(shutthebox, &shutthebox_bot)
