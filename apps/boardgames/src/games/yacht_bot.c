/*
 * Yacht's bot (bots.h), every sheet (classic, nordic, generala) and one to six players. The throw is a move: "d" and
 * the five dice in their places, "?" thrown, "-" kept ("d?????", then "d--?-?"), played by yacht_show() with the
 * game's own faces as the taps keep the dice and throw; a score is the category's name ("3k", "fh", "6"...).
 * The player's sense: what a category is worth now is its points, less what that box would bring if it stayed
 * empty (its average over a good player's games: the cost of filling it), and for the upper boxes the change in the
 * chance of the bonus (the points the other upper boxes can still bring, against what the bonus needs). Classic: the
 * extra five of a kind's 100. Generala: a straight, full house or poker made on the first throw scores 5 more.
 *   easy    keeps the most dice alike (a run when a straight is open), scores the most points less half the cost of
 *           the box, both blurred: now and then a careless box;
 *   medium  the dice kept by the throw just ahead only (each way of keeping weighed by every throw of the dice let go,
 *           scored at once after it), the boxes' costs counted at six tenths (a player who fills a box early);
 *   hard    the whole turn worked out: every way of keeping, every throw after it, every way of keeping after that
 *           and the throw after it (the 252 sets of five dice, the 462 sets kept), each end scored by that sense:
 *           the expected best, and scoring early when throwing again is worth less.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/yacht.h"

/* ---- what an empty box brings, on average over a good player's games (its cost when filled now) ------------------
 * started from the averages of the solitaire game played best (classic) and of this bot's own games, then tuned by
 * its games alone (3000 a step, the same throws): classic 243 -> 251 points on average, nordic 236 -> 241,
 * generala 156 -> 159 */
static const float COST[YA_VARIANTS][YC_N] = {
    /*  1     2     3     4      5      6     pair  2pair  3k     4k     full   small  large  str   chance yacht  dbl */
    {5.8f, 8.3f, 7.9f, 9.2f, 14.2f, 16.2f, 0, 0, 21.7f, 13.1f, 19.6f, 29.5f, 29.7f, 0, 26.5f, 16.9f, 0},
    {1.9f, 5.1f, 5.4f, 9.0f, 14.8f, 16.0f, 13.5f, 16.0f, 11.5f, 7.5f, 9.0f, 7.0f, 13.0f, 0, 21.5f, 11.0f, 0},
    {2.0f, 6.5f, 8.0f, 11.5f, 15.0f, 15.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
/* generala's own boxes (YC_STRAIGHT, YC_FULL, YC_FOUR_KIND, YC_YACHT, YC_DOUBLE) */
static const float GEN_COST[YC_N] = {[YC_STRAIGHT] = 14.0f, [YC_FULL] = 22.0f, [YC_FOUR_KIND] = 23.0f,
                                     [YC_YACHT] = 13.0f, [YC_DOUBLE] = 6.0f};
/* the dice an upper box gets on average (a face's count), and the spread */
static const float UP_MEAN[7] = {0, 2.1f, 2.65f, 2.87f, 3.05f, 3.14f, 3.2f};
#define UP_SD 1.05f

static float cost(int variant, int cat)
{
    return variant == YA_GENERALA && !ya_is_upper(cat) ? GEN_COST[cat] : COST[variant][cat];
}

/* the chance of the upper bonus with `have` points and the upper boxes `open` (bit c) still to fill */
static float bonus_chance(int have, unsigned open)
{
    int need = YA_BONUS_NEEDS - have;
    if (need <= 0) return 1;
    float mu = 0, var = 0;
    for (int c = YC_ONES; c <= YC_SIXES; c++)
        if (open >> c & 1) {
            mu += UP_MEAN[c + 1] * (c + 1);
            var += (UP_SD * (c + 1)) * (UP_SD * (c + 1));
        }
    if (var <= 0) return 0;
    return 0.5f * erfcf(-(mu - need + 0.5f) / sqrtf(2 * var));
}

/* ---- the sets of dice: counts of each face, a key, an index --------------------------------------------------- */
#define NSETS 462                               /* sets of 0..5 dice */
#define NFULL 252                               /* of 5 */
#define KEYS 46656                              /* 6^6 */

typedef struct { uint8_t c[7], n; } dset;

typedef struct {
    dset set[NSETS];
    short *index;                               /* key -> set (-1: none) */
    int full[NFULL];                            /* the sets of five */
    /* a set kept: its outcomes (a set of five) and their chances, for the dice thrown */
    int first[NSETS + 1];
    short *to;
    float *pr;
} tables;

static int key_of(const uint8_t c[7])
{
    int k = 0;
    for (int f = 6; f >= 1; f--) k = k * 6 + c[f];
    return k;
}

static int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }

static int tables_make(tables *T)
{
    T->index = malloc(sizeof(short) * KEYS);
    T->to = malloc(sizeof(short) * 4400);
    T->pr = malloc(sizeof(float) * 4400);
    if (!T->index || !T->to || !T->pr) return 0;
    memset(T->index, 0xff, sizeof(short) * KEYS);
    int n = 0, nf = 0;
    uint8_t c[7] = {0};
    for (c[1] = 0; c[1] <= 5; c[1]++)
        for (c[2] = 0; c[1] + c[2] <= 5; c[2]++)
            for (c[3] = 0; c[1] + c[2] + c[3] <= 5; c[3]++)
                for (c[4] = 0; c[1] + c[2] + c[3] + c[4] <= 5; c[4]++)
                    for (c[5] = 0; c[1] + c[2] + c[3] + c[4] + c[5] <= 5; c[5]++)
                        for (c[6] = 0; c[1] + c[2] + c[3] + c[4] + c[5] + c[6] <= 5; c[6]++) {
                            dset *d = &T->set[n];
                            memcpy(d->c, c, sizeof c);
                            d->n = (uint8_t)(c[1] + c[2] + c[3] + c[4] + c[5] + c[6]);
                            T->index[key_of(c)] = (short)n;
                            if (d->n == YA_DICE) T->full[nf++] = n;
                            n++;
                        }
    int m = 0;
    for (int k = 0; k < NSETS; k++) {           /* the throw of the 5 - n dice let go: each set of them */
        T->first[k] = m;
        const dset *keep = &T->set[k];
        int left = YA_DICE - keep->n;
        double all = pow(6, left);
        for (int r = 0; r < NSETS; r++) {
            const dset *roll = &T->set[r];
            if (roll->n != left) continue;
            int ways = fact(left);
            uint8_t sum[7];
            for (int f = 1; f <= 6; f++) {
                ways /= fact(roll->c[f]);
                sum[f] = (uint8_t)(keep->c[f] + roll->c[f]);
            }
            sum[0] = 0;
            T->to[m] = T->index[key_of(sum)];
            T->pr[m++] = (float)(ways / all);
        }
    }
    T->first[NSETS] = m;
    return 1;
}

static void tables_free(tables *T)
{
    free(T->index);
    free(T->to);
    free(T->pr);
}

static void faces_of(const dset *d, uint8_t out[YA_DICE])
{
    int i = 0;
    for (int f = 1; f <= 6; f++)
        for (int k = 0; k < d->c[f]; k++) out[i++] = (uint8_t)f;
}

/* ---- scoring a set of five now ---------------------------------------------------------------------------------- */
typedef struct {
    ypos p;                                     /* the position, the dice to be set */
    int cats[YA_MAX_CATS], ncats;
    float easy;                                 /* easy: the share of a box's cost counted (else 1) */
} judge;

/* the best category for the dice at `throws` (its worth), *cat set; -1e9 if none */
static float score_now(const judge *J, const uint8_t dice[YA_DICE], int throws, int *cat)
{
    ypos q = J->p;
    memcpy(q.dice, dice, YA_DICE);
    q.throws = (uint8_t)throws;
    const int16_t *box = q.box[q.turn];
    int up = ya_upper(&q, q.turn), bonus = ya_bonus_at(q.variant);
    unsigned open_up = 0;
    for (int c = YC_ONES; c <= YC_SIXES; c++)
        if (box[c] < 0) open_up |= 1u << c;
    float extra = q.variant == YA_CLASSIC && ya_five(dice) && box[YC_YACHT] == 50 ? 100 : 0;
    float before = bonus && up < YA_BONUS_NEEDS ? bonus_chance(up, open_up) : 0, best = -1e9f;
    *cat = -1;
    for (int i = 0; i < J->ncats; i++) {
        int c = J->cats[i], pts = ya_points(&q, c);
        if (pts < 0) continue;
        float w = pts + extra - J->easy * cost(q.variant, c);
        if (ya_is_upper(c) && bonus && up < YA_BONUS_NEEDS)
            w += bonus * (bonus_chance(up + pts, open_up & ~(1u << c)) - before);
        if (w > best) best = w, *cat = c;
    }
    return best;
}

/* ---- the turn worked out ---------------------------------------------------------------------------------------- */
typedef struct {
    float late[NSETS];                          /* scoring a set of five after a second or third throw */
    float v2[NSETS];                            /* after the second throw: score, or keep and throw once more */
    float kv3[NSETS], kv2[NSETS];               /* a set kept: the worth of throwing the rest (the last throw; the
                                                 * second) */
} turn_plan;

static float kept_worth(const tables *T, const float *v, int k)
{
    float w = 0;
    for (int m = T->first[k]; m < T->first[k + 1]; m++) w += T->pr[m] * v[T->to[m]];
    return w;
}

/* the best set kept from the five d (fewer than five), by kv; its worth */
static float best_keep(const tables *T, const float *kv, const dset *d, int *keep)
{
    float best = -1e9f;
    uint8_t k[7] = {0};
    *keep = -1;
    for (k[1] = 0; k[1] <= d->c[1]; k[1]++)
        for (k[2] = 0; k[2] <= d->c[2]; k[2]++)
            for (k[3] = 0; k[3] <= d->c[3]; k[3]++)
                for (k[4] = 0; k[4] <= d->c[4]; k[4]++)
                    for (k[5] = 0; k[5] <= d->c[5]; k[5]++)
                        for (k[6] = 0; k[6] <= d->c[6]; k[6]++) {
                            if (k[1] + k[2] + k[3] + k[4] + k[5] + k[6] == YA_DICE) continue;
                            int i = T->index[key_of(k)];
                            if (kv[i] > best) best = kv[i], *keep = i;
                        }
    return best;
}

static void plan_turn(const tables *T, const judge *J, turn_plan *W, int lookahead)
{
    int cat;
    for (int i = 0; i < NFULL; i++) {
        int s = T->full[i];
        uint8_t d[YA_DICE];
        faces_of(&T->set[s], d);
        W->late[s] = score_now(J, d, 2, &cat);
    }
    for (int k = 0; k < NSETS; k++) W->kv3[k] = T->set[k].n < YA_DICE ? kept_worth(T, W->late, k) : -1e9f;
    if (lookahead < 2) return;
    for (int i = 0; i < NFULL; i++) {
        int s = T->full[i], keep;
        float now = W->late[s], on = best_keep(T, W->kv3, &T->set[s], &keep);
        W->v2[s] = now > on ? now : on;
    }
    for (int k = 0; k < NSETS; k++) W->kv2[k] = T->set[k].n < YA_DICE ? kept_worth(T, W->v2, k) : -1e9f;
}

/* ---- easy: the dice alike (or a run), kept ---------------------------------------------------------------------- */
static void easy_keep(const ypos *p, const dset *d, uint8_t keep[7], ai_rng *r)
{
    memset(keep, 0, 7);
    const int16_t *box = p->box[p->turn];
    int straight_open = box[YC_SMALL] < 0 || box[YC_LARGE] < 0 || box[YC_STRAIGHT] < 0;
    int run = 0, best_run = 0, end = 0;
    for (int f = 1; f <= 6; f++) {
        run = d->c[f] ? run + 1 : 0;
        if (run > best_run) best_run = run, end = f;
    }
    int ys = p->variant == YA_GENERALA ? box[YC_STRAIGHT] < 0 : 1;
    if (straight_open && ys && best_run >= 4 - ai_rand_n(r, 2)) {
        for (int f = end - best_run + 1; f <= end; f++) keep[f] = 1;
        return;
    }
    int face = 6;
    for (int f = 6; f >= 1; f--)
        if (d->c[f] > d->c[face]) face = f;
    keep[face] = d->c[face];
    if (d->c[face] == 1) keep[face] = (uint8_t)(face >= 4);   /* nothing alike: a high die, or nothing */
}

static int think(int seat, int level, char *move, int cap)
{
    ypos p = yacht_live()->pos;                 /* the copy, at entry: nothing else is read */
    if (p.over || p.turn != seat || cap < 8) return 0;
    if (!p.throws) {
        strcpy(move, "d?????");
        return 1;
    }
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), ai_mix(p.turns, (uint64_t)p.throws * 8 + p.turn)));
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    tables *T = malloc(sizeof *T);
    turn_plan *W = malloc(sizeof *W);
    judge J = {.p = p};
    J.ncats = ya_cats(p.variant, J.cats);
    J.easy = level == AI_EASY ? 0.5f : level == AI_MEDIUM ? 0.6f : 1;
    if (!T || !W || !tables_make(T)) {
        if (T) tables_free(T);
        free(T);
        free(W);
        return 0;
    }
    uint8_t now[7] = {0};
    for (int i = 0; i < YA_DICE; i++) now[p.dice[i]]++;
    const dset *d = &T->set[T->index[key_of(now)]];
    int cat = -1, keep = -1;
    float here = score_now(&J, p.dice, p.throws, &cat);
    if (p.throws < YA_THROWS) {
        if (level == AI_EASY) {
            uint8_t k[7];
            easy_keep(&p, d, k, &r);
            if (here < 20 + ai_rand_n(&r, 20)) keep = T->index[key_of(k)];   /* scores a good throw at once */
            if (keep >= 0 && T->set[keep].n == YA_DICE) keep = -1;
        } else {
            plan_turn(T, &J, W, level == AI_HARD ? 2 : 1);
            float on = best_keep(T, level == AI_HARD && p.throws == 1 ? W->kv2 : W->kv3, d, &keep);
            if (on <= here) keep = -1;
        }
    }
    if (level == AI_EASY && keep < 0 && ai_rand_n(&r, 4) == 0) {   /* a careless box now and then */
        J.easy = 0;
        int c2;
        score_now(&J, p.dice, p.throws, &c2);
        if (c2 >= 0) cat = c2;
    }
    int ok = 1;
    if (keep >= 0) {                            /* the dice kept in their places, the others thrown */
        uint8_t k[7];
        memcpy(k, T->set[keep].c, sizeof k);
        move[0] = 'd';
        for (int i = 0; i < YA_DICE; i++) {
            int f = p.dice[i];
            move[1 + i] = k[f] ? '-' : '?';
            if (k[f]) k[f]--;
        }
        move[1 + YA_DICE] = 0;
    } else if (cat >= 0) {
        static const char *NAMES[YC_N] = {"1", "2", "3", "4", "5", "6", "p", "pp", "3k", "4k", "fh", "ss", "ls",
                                          "st", "c", "y", "yy"};
        snprintf(move, (size_t)cap, "%s", NAMES[cat]);
    } else ok = 0;
    tables_free(T);
    free(T);
    free(W);
    return ok && !ai_cancelled();
}

static const bot yacht_bot = {.think = think, .play = yacht_show};
REGISTER_BOT(yacht, &yacht_bot)
