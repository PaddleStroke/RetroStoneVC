/*
 * 421's bot (bots.h), two to six players. The throw is a move: "d" and the three dice in their places, "?" thrown, "-"
 * kept ("d???", then "d-?-"), played by fourtwoone_show() with the game's own faces as the taps keep the dice and
 * throw; "s" stops (the discharge, before the round's limit). The charge has nothing to choose: one throw each.
 * In the discharge each turn is worked out over its throws (the 56 sets of three dice, every set kept, every throw
 * of the others), each ending weighed by how the round would end for it:
 *   easy    keeps the 4, 2, 1 it has, or the 1s, or the dice alike (now and then any die), stops on a fiche or better;
 *   medium  a good player's throw: each ending by its rank among all throws and its worth in chips, kept and
 *           stopped by that alone;
 *   hard    each ending by the round: against the throws already made (fewer throws and an earlier turn win a tie)
 *           and, for each player still to throw, the spread of results a good player makes in the throws allowed
 *           (the leader's own count of throws sets it: stopping on a strong first throw leaves the others one throw);
 *           the round's best gives its worth in chips away (good: going out wins), the worst takes the chips.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/fourtwoone.h"

/* ---- the sets of three dice: counts, a key, their rank; the sets kept and their throws ------------------------ */
#define NSETS 84                                /* sets of 0..3 dice */
#define NFULL 56
#define KEYS 4096                               /* 4^6 */

typedef struct {
    uint8_t c[7], n;
    int rank, value;                            /* sets of three: fto_rank, fto_value */
    float pct;                                  /* sets of three: the share of single throws it beats (equal: half) */
} tset;

typedef struct {
    tset set[NSETS];
    short index[KEYS];
    int full[NFULL];
    int first[NSETS + 1];                       /* a set kept: its outcomes (sets of three) and their chances */
    short to[512];
    float pr[512];
} tables;

static int key_of(const uint8_t c[7])
{
    int k = 0;
    for (int f = 6; f >= 1; f--) k = k * 4 + c[f];
    return k;
}

static void dice_of(const uint8_t c[7], uint8_t d[FTO_DICE])
{
    int i = 0;
    for (int f = 1; f <= 6; f++)
        for (int k = 0; k < c[f]; k++) d[i++] = (uint8_t)f;
}

static void tables_make(tables *T)
{
    memset(T->index, 0xff, sizeof T->index);
    int n = 0, nf = 0;
    uint8_t c[7] = {0};
    for (c[1] = 0; c[1] <= 3; c[1]++)
        for (c[2] = 0; c[1] + c[2] <= 3; c[2]++)
            for (c[3] = 0; c[1] + c[2] + c[3] <= 3; c[3]++)
                for (c[4] = 0; c[1] + c[2] + c[3] + c[4] <= 3; c[4]++)
                    for (c[5] = 0; c[1] + c[2] + c[3] + c[4] + c[5] <= 3; c[5]++)
                        for (c[6] = 0; c[1] + c[2] + c[3] + c[4] + c[5] + c[6] <= 3; c[6]++) {
                            tset *s = &T->set[n];
                            memcpy(s->c, c, sizeof c);
                            s->n = (uint8_t)(c[1] + c[2] + c[3] + c[4] + c[5] + c[6]);
                            if (s->n == FTO_DICE) {
                                uint8_t d[FTO_DICE];
                                dice_of(c, d);
                                s->rank = fto_rank(d);
                                s->value = fto_value(d);
                                T->full[nf++] = n;
                            }
                            T->index[key_of(c)] = (short)n++;
                        }
    int m = 0;
    for (int k = 0; k < NSETS; k++) {           /* the dice let go, thrown: each way, 1/6 a die */
        T->first[k] = m;
        const tset *keep = &T->set[k];
        int left = FTO_DICE - keep->n, ways = 1;
        for (int i = 0; i < left; i++) ways *= 6;
        for (int code = 0; code < ways; code++) {
            uint8_t sum[7];
            memcpy(sum, keep->c, sizeof sum);
            for (int i = 0, x = code; i < left; i++, x /= 6) sum[1 + x % 6]++;
            int to = T->index[key_of(sum)], j;
            for (j = T->first[k]; j < m && T->to[j] != to; j++) {}
            if (j == m) T->to[m] = (short)to, T->pr[m++] = 0;
            T->pr[j] += 1.0f / ways;
        }
    }
    T->first[NSETS] = m;
    for (int i = 0; i < NFULL; i++) {           /* the share of single throws each beats */
        tset *s = &T->set[T->full[i]];
        float below = 0;
        for (int j = T->first[0]; j < T->first[1]; j++) {
            int r = T->set[T->to[j]].rank;
            below += r < s->rank ? T->pr[j] : r == s->rank ? T->pr[j] / 2 : 0;
        }
        s->pct = below;
    }
}

/* ---- a turn worked out: v[t][s], the set of three s after t throws (1..3), played well; u[t][s] the ending's worth
 * (stopping there, or made to by the limit) */
typedef struct {
    float u[FTO_THROWS + 1][NSETS], v[FTO_THROWS + 1][NSETS];
    float kv[FTO_THROWS + 1][NSETS];            /* a set kept after t throws: the worth of throwing the rest */
} turn_plan;

static float kept_worth(const tables *T, const float *v, int k)
{
    float w = 0;
    for (int j = T->first[k]; j < T->first[k + 1]; j++) w += T->pr[j] * v[T->to[j]];
    return w;
}

/* the best set kept from s (fewer than three) after t throws, by kv[t]; its worth */
static float best_keep(const tables *T, const turn_plan *W, int t, int s, int *keep)
{
    const tset *d = &T->set[s];
    float best = -1e9f;
    uint8_t k[7] = {0};
    *keep = -1;
    for (k[1] = 0; k[1] <= d->c[1]; k[1]++)
        for (k[2] = 0; k[2] <= d->c[2]; k[2]++)
            for (k[3] = 0; k[3] <= d->c[3]; k[3]++)
                for (k[4] = 0; k[4] <= d->c[4]; k[4]++)
                    for (k[5] = 0; k[5] <= d->c[5]; k[5]++)
                        for (k[6] = 0; k[6] <= d->c[6]; k[6]++) {
                            if (k[1] + k[2] + k[3] + k[4] + k[5] + k[6] == FTO_DICE) continue;
                            int i = T->index[key_of(k)];
                            if (W->kv[t][i] > best) best = W->kv[t][i], *keep = i;
                        }
    return best;
}

/* v and kv from the endings u, for a turn of `limit` throws at most */
static void plan_turn(const tables *T, turn_plan *W, int limit)
{
    for (int t = limit; t >= 1; t--) {
        for (int i = 0; i < NFULL; i++) {
            int s = T->full[i], keep;
            W->v[t][s] = W->u[t][s];
            if (t < limit) {
                float on = best_keep(T, W, t, s, &keep);
                if (on > W->v[t][s]) W->v[t][s] = on;
            }
        }
        if (t > 1)
            for (int k = 0; k < NSETS; k++)
                W->kv[t - 1][k] = T->set[k].n < FTO_DICE ? kept_worth(T, W->v[t], k) : -1e9f;
    }
}

/* a good player's results in a turn of `limit` throws: out[t][s] the chance of ending on s after t throws */
static void spread(const tables *T, turn_plan *W, int limit, float out[FTO_THROWS + 1][NSETS])
{
    for (int t = 1; t <= FTO_THROWS; t++)
        for (int i = 0; i < NFULL; i++) W->u[t][T->full[i]] = T->set[T->full[i]].pct;
    plan_turn(T, W, limit);
    float at[NSETS];
    memset(out, 0, sizeof(float) * (FTO_THROWS + 1) * NSETS);
    memset(at, 0, sizeof at);
    for (int j = T->first[0]; j < T->first[1]; j++) at[T->to[j]] += T->pr[j];   /* the first throw */
    for (int t = 1; t <= limit; t++) {
        float next[NSETS];
        memset(next, 0, sizeof next);
        for (int i = 0; i < NFULL; i++) {
            int s = T->full[i], keep = -1;
            if (at[s] <= 0) continue;
            float on = t < limit ? best_keep(T, W, t, s, &keep) : -1e9f;
            if (t == limit || W->u[t][s] >= on) {
                out[t][s] += at[s];
                continue;
            }
            for (int j = T->first[keep]; j < T->first[keep + 1]; j++) next[T->to[j]] += at[s] * T->pr[j];
        }
        memcpy(at, next, sizeof at);
    }
}

/* ---- hard: the round's stakes ------------------------------------------------------------------------------------ */
static void stakes(const tables *T, turn_plan *W, const tpos *p)
{
    int me = p->turn, later = 0, leader = me == p->leader, played_best = 0;
    for (int q = 0; q < p->players; q++)
        if ((p->in >> q & 1) && !(p->done >> q & 1) && q != me) later++;
    for (int i = 0; i < p->norder; i++) {
        int v = fto_value(p->res[p->order[i]]);
        played_best = v > played_best ? v : played_best;
    }
    float (*dist)[FTO_THROWS + 1][NSETS] = malloc(sizeof(float) * (FTO_THROWS + 1) * (FTO_THROWS + 1) * NSETS);
    if (!dist) return;
    turn_plan *tmp = malloc(sizeof *tmp);
    if (!tmp) { free(dist); return; }
    for (int L = 1; L <= FTO_THROWS; L++)       /* the others' spread, by the throws they may make */
        if (later && (leader ? 1 : L == p->limit)) spread(T, tmp, L, dist[L]);
    free(tmp);
    int chips = p->chips[me];
    for (int t = 1; t <= p->limit; t++)
        for (int i = 0; i < NFULL; i++) {
            int s = T->full[i], R = T->set[s].rank, best = 1, worst = 1;
            for (int k = 0; k < p->norder; k++) {   /* the throws made: they won equal ties by playing first */
                int q = p->order[k], Rq = fto_rank(p->res[q]), tq = p->res_throws[q];
                int mine_better = R > Rq || (R == Rq && t < tq);
                best &= mine_better;
                worst &= !mine_better;
            }
            float pb = best, pw = worst;
            if (later) {
                int L = leader ? t : p->limit;
                float lower = 0;                /* a later player ends below us (equal: we played first) */
                for (int tt = 1; tt <= L; tt++)
                    for (int j = 0; j < NFULL; j++) {
                        int s2 = T->full[j], R2 = T->set[s2].rank;
                        if (R > R2 || (R == R2 && t <= tt)) lower += dist[L][tt][s2];
                    }
                for (int k = 0; k < later; k++) pb *= lower, pw *= 1 - lower;
            }
            int give = T->set[s].value < chips ? T->set[s].value : chips;
            float gain = give + (T->set[s].value >= chips ? 6.0f : 0);   /* the last chips given: out, a win */
            float take = played_best > 2 ? played_best : 2.2f;
            W->u[t][s] = pb * gain - pw * take;
        }
    free(dist);
}

/* ---- easy: the 4-2-1 it has, the 1s, the dice alike ---------------------------------------------------------- */
static void easy_keep(const tset *d, uint8_t k[7], ai_rng *r)
{
    memset(k, 0, 7);
    if (ai_rand_n(r, 5) == 0) {                 /* any die */
        int f = 1 + ai_rand_n(r, 6);
        k[f] = d->c[f] ? 1 : 0;
        return;
    }
    if (d->c[1] >= 2) {                         /* 1-1: a fiche to come */
        k[1] = 2;
        return;
    }
    if ((d->c[4] > 0) + (d->c[2] > 0) + (d->c[1] > 0) >= 2) {   /* two of 4, 2, 1 */
        k[4] = d->c[4] > 0, k[2] = d->c[2] > 0, k[1] = d->c[1] > 0;
        return;
    }
    for (int f = 6; f >= 1; f--)                /* a pair */
        if (d->c[f] >= 2) {
            k[f] = 2;
            return;
        }
    k[1] = d->c[1] > 0;                         /* a lone 1 */
}

static int think(int seat, int level, char *move, int cap)
{
    tpos p = fourtwoone_live()->pos;            /* the copy, at entry: nothing else is read */
    if (p.phase == FTO_OVER || p.turn != seat || cap < 8) return 0;
    if (!p.throws) {
        strcpy(move, "d???");
        return 1;
    }
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), ai_mix(p.rounds, (uint64_t)p.throws * 8 + p.turn)));
    tables *T = malloc(sizeof *T);
    turn_plan *W = malloc(sizeof *W);
    if (!T || !W) {
        free(T);
        free(W);
        return 0;
    }
    tables_make(T);
    uint8_t now[7] = {0};
    for (int i = 0; i < FTO_DICE; i++) now[p.dice[i]]++;
    int s = T->index[key_of(now)], keep = -1, t = p.throws;
    if (level == AI_EASY) {
        uint8_t k[7];
        int stop_at = 800 + ai_rand_n(&r, 2) * 100;   /* a fiche or three alike (or better), or 1-1-1 / 4-2-1 */
        if (T->set[s].rank < stop_at || !fto_can_stop(&p)) {
            easy_keep(&T->set[s], k, &r);
            if (k[1] + k[2] + k[3] + k[4] + k[5] + k[6] < FTO_DICE) keep = T->index[key_of(k)];
        }
    } else {
        if (level == AI_HARD) stakes(T, W, &p);
        else
            for (int tt = 1; tt <= FTO_THROWS; tt++)
                for (int i = 0; i < NFULL; i++) {
                    const tset *e = &T->set[T->full[i]];
                    W->u[tt][T->full[i]] = 0.6f * e->pct + 0.04f * e->value;
                }
        plan_turn(T, W, p.limit);
        float on = best_keep(T, W, t, s, &keep);
        if (W->u[t][s] >= on) keep = -1;
    }
    int ok = 1;
    if (keep >= 0) {
        uint8_t k[7];
        memcpy(k, T->set[keep].c, sizeof k);
        move[0] = 'd';
        for (int i = 0; i < FTO_DICE; i++) {
            int f = p.dice[i];
            move[1 + i] = k[f] ? '-' : '?';
            if (k[f]) k[f]--;
        }
        move[1 + FTO_DICE] = 0;
    } else if (fto_can_stop(&p)) strcpy(move, "s");
    else ok = 0;
    free(T);
    free(W);
    return ok && !ai_cancelled();
}

static const bot fourtwoone_bot = {.think = think, .play = fourtwoone_show};
REGISTER_BOT(fourtwoone, &fourtwoone_bot)
