/*
 * Farkle's bot (bots.h), one to six players. A turn's first throw is "d??????" (the faces the game's: "?"); after a
 * throw the bot sets scoring dice aside and either throws the rest or banks, in one move: "a115 d???" (set aside 1, 1,
 * 5, throw the three left) or "a5 b" (set aside a 5, bank), played by farkle_show() as the taps would make it.
 *   easy    sets aside every die that scores and banks once the turn makes a few hundred (its own whim each turn),
 *           as soon as it may; the final round's target now and then forgotten;
 *   medium  a player's rules of thumb: a triple or better taken whole, a single 1 (or 5) when that leaves dice to
 *           throw, all of them when few dice are left; banks by the dice left (300 with one or two, 500 with three,
 *           1000 with four, 2000 with five, 3000 after hot dice); in the final round it plays on until it leads;
 *   hard    the turn worked out: for every turn total (in 50s) and number of dice to throw, the worth of throwing
 *           them (every throw of n dice, each with its best choice of dice to set aside, then the best of banking or
 *           throwing on), from the highest totals down. The worth of a bank: the points (the expected score), or,
 *           when the turn decides the game (the final round, the last turn alone), the chance of winning: enough to
 *           pass the leader (or reach the target) is a win, less is worth nothing.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/farkle.h"

#define STEP 50
#define TOP 12000                               /* the turn's total worked out up to (above: banked) */
#define LEVELS (TOP / STEP + 1)
#define NSETS 924                               /* the sets of 0..6 dice */

/* ---- the throws of n dice: each set of faces, its chance, and the best points with k of its dice -------------- */
typedef struct {
    uint8_t c[7], n;
    float pr;                                   /* its chance among the throws of n dice */
    int16_t best[FK_DICE + 1];                  /* the most points setting aside exactly k of them (0: none) */
} outcome;

typedef struct {
    outcome o[NSETS];
    int first[FK_DICE + 2];                     /* the sets of n dice: o[first[n] .. first[n + 1]) */
} throws;

static int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }

static int value_of(const uint8_t c[7])         /* the points of these dice taken together (0: some die scores not) */
{
    uint8_t d[FK_DICE];
    int n = 0;
    for (int f = 1; f <= 6; f++)
        for (int k = 0; k < c[f]; k++) d[n++] = (uint8_t)f;
    return n ? fk_value(d, n) : 0;
}

static void throws_make(throws *T)
{
    int m = 0;
    for (int n = 0; n <= FK_DICE; n++) {
        T->first[n] = m;
        uint8_t c[7] = {0};
        for (c[1] = 0; c[1] <= n; c[1]++)
            for (c[2] = 0; c[1] + c[2] <= n; c[2]++)
                for (c[3] = 0; c[1] + c[2] + c[3] <= n; c[3]++)
                    for (c[4] = 0; c[1] + c[2] + c[3] + c[4] <= n; c[4]++)
                        for (c[5] = 0; c[1] + c[2] + c[3] + c[4] + c[5] <= n; c[5]++) {
                            c[6] = (uint8_t)(n - c[1] - c[2] - c[3] - c[4] - c[5]);
                            outcome *o = &T->o[m++];
                            memcpy(o->c, c, sizeof c);
                            o->n = (uint8_t)n;
                            int ways = fact(n);
                            for (int f = 1; f <= 6; f++) ways /= fact(c[f]);
                            o->pr = (float)(ways / pow(6, n));
                            memset(o->best, 0, sizeof o->best);
                            uint8_t s[7] = {0};    /* every part of it */
                            for (s[1] = 0; s[1] <= c[1]; s[1]++)
                                for (s[2] = 0; s[2] <= c[2]; s[2]++)
                                    for (s[3] = 0; s[3] <= c[3]; s[3]++)
                                        for (s[4] = 0; s[4] <= c[4]; s[4]++)
                                            for (s[5] = 0; s[5] <= c[5]; s[5]++)
                                                for (s[6] = 0; s[6] <= c[6]; s[6]++) {
                                                    int k = s[1] + s[2] + s[3] + s[4] + s[5] + s[6], v;
                                                    if (k && (v = value_of(s)) > o->best[k]) o->best[k] = (int16_t)v;
                                                }
                        }
    }
    T->first[FK_DICE + 1] = m;
}

/* ---- the stakes: what banking a turn of x points is worth ------------------------------------------------------ */
typedef struct {
    int need;                                   /* 0: the points (expected score); else the points to win */
    int entered;                                /* 1: a bank of any size is allowed (else FK_ENTRY at least) */
} stakes;

static float bank_worth(const stakes *S, int x)  /* -1: not allowed */
{
    if (!S->entered && x < FK_ENTRY) return -1;
    if (!S->need) return (float)x;
    return x >= S->need ? 1.0f : x == S->need - STEP ? 0.5f : 0.0f;   /* one step short: level with the leader */
}

/* w[t][n]: throwing n dice with a turn total of t * STEP, played well */
typedef float plan_t[LEVELS][FK_DICE + 1];

static float go_on(const plan_t w, const stakes *S, int x, int n)   /* after a choice: the best of bank, throw n */
{
    float b = bank_worth(S, x);
    if (x / STEP >= LEVELS || (S->need && x >= S->need)) return b > 0 ? b : 0;
    float t = w[x / STEP][n];
    return t > b ? t : b;
}

static void plan_solve(plan_t w, const throws *T, const stakes *S, int from)
{
    for (int t = LEVELS - 1; t >= from / STEP; t--)
        for (int n = 1; n <= FK_DICE; n++) {
            float sum = 0;
            for (int i = T->first[n]; i < T->first[n + 1]; i++) {
                const outcome *o = &T->o[i];
                float best = 0;                 /* a farkle: the turn lost */
                for (int k = 1; k <= n; k++) {
                    if (!o->best[k]) continue;
                    float v = go_on((const float (*)[FK_DICE + 1])w, S, t * STEP + o->best[k], n - k ? n - k : FK_DICE);
                    if (v > best) best = v;
                }
                sum += o->pr * best;
            }
            w[t][n] = sum;
        }
}

/* ---- the choices of one throw ----------------------------------------------------------------------------------- */
typedef struct { uint8_t c[7]; int k, v; } choice;

static int choices(const fpos *p, choice *out)  /* every set of the throw's dice that scores (by faces) */
{
    uint8_t c[7] = {0}, s[7] = {0};
    for (int i = 0; i < p->nd; i++) c[p->dice[i]]++;
    int n = 0;
    for (s[1] = 0; s[1] <= c[1]; s[1]++)
        for (s[2] = 0; s[2] <= c[2]; s[2]++)
            for (s[3] = 0; s[3] <= c[3]; s[3]++)
                for (s[4] = 0; s[4] <= c[4]; s[4]++)
                    for (s[5] = 0; s[5] <= c[5]; s[5]++)
                        for (s[6] = 0; s[6] <= c[6]; s[6]++) {
                            int v = value_of(s);
                            if (!v) continue;
                            memcpy(out[n].c, s, sizeof s);
                            out[n].k = s[1] + s[2] + s[3] + s[4] + s[5] + s[6];
                            out[n++].v = v;
                        }
    return n;
}

/* what the turn must make to settle the game: the final round (pass the leader), alone the last turn (the target);
 * 0: the points are what counts */
static int turn_need(const fpos *p)
{
    int me = p->turn, mine = p->score[me];
    if (p->players == 1) return p->turns + 1 >= FK_SOLO_TURNS ? FK_TARGET - mine : 0;
    if (p->final_by < 0) return 0;
    int lead = 0;
    for (int q = 0; q < p->players; q++)
        if (q != me && p->score[q] > lead) lead = p->score[q];
    int need = lead - mine + STEP;              /* passing the leader */
    return need > 0 ? need : STEP;
}

/* medium's rules of thumb: the dice set aside */
static int thumb_pick(const choice *ch, int n, int nd)
{
    int best = -1;
    for (int i = 0; i < n; i++) {               /* a combination of three dice or more, whole: the most points */
        int multi = 0;
        for (int f = 1; f <= 6; f++) multi |= ch[i].c[f] >= 3;
        multi |= ch[i].k == FK_DICE;
        if (multi && (best < 0 || ch[i].v > ch[best].v || (ch[i].v == ch[best].v && ch[i].k > ch[best].k))) best = i;
    }
    int all = -1, single = -1;
    for (int i = 0; i < n; i++) {
        if (all < 0 || ch[i].k > ch[all].k || (ch[i].k == ch[all].k && ch[i].v > ch[all].v)) all = i;
        if (ch[i].k == 1 && (single < 0 || ch[i].v > ch[single].v)) single = i;
    }
    if (best >= 0) {                            /* with the singles left over too, when few dice would remain */
        for (int i = 0; i < n; i++)
            if (ch[i].k > ch[best].k && ch[i].v > ch[best].v && nd - ch[i].k <= 2) best = i;
        return best;
    }
    return single >= 0 && nd - 1 >= 3 ? single : all;
}

static int think(int seat, int level, char *move, int cap)
{
    fpos p = farkle_live()->pos;                /* the copy, at entry: nothing else is read */
    if (p.phase == FK_OVER || p.turn != seat || cap < 24) return 0;
    if (p.phase == FK_THROW) {
        strcpy(move, "d??????");
        return 1;
    }
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), ai_mix(p.turns, (uint64_t)p.throws * 8 + p.turn)));
    choice ch[64];
    int n = choices(&p, ch), pick = -1, bank = 0, need = turn_need(&p), entered = p.score[p.turn] > 0;
    if (!n) return 0;                           /* (a farkle ends the turn by itself) */
    if (level == AI_HARD) {
        throws *T = malloc(sizeof *T);
        float (*w)[FK_DICE + 1] = malloc(sizeof(plan_t));
        if (!T || !w) {
            free(T);
            free(w);
            return 0;
        }
        throws_make(T);
        stakes S = {need, entered};
        plan_solve(w, T, &S, p.turn_pts);
        float best = -2;
        for (int i = 0; i < n; i++) {
            int x = p.turn_pts + ch[i].v, left = p.nd - ch[i].k ? p.nd - ch[i].k : FK_DICE;
            float b = bank_worth(&S, x), t = x / STEP < LEVELS && !(need && x >= need) ? w[x / STEP][left] : -1;
            if (b > best) best = b, pick = i, bank = 1;
            if (t > best) best = t, pick = i, bank = 0;
        }
        free(T);
        free(w);
    } else {
        if (level == AI_MEDIUM) pick = thumb_pick(ch, n, p.nd);
        else
            for (int i = 0; i < n; i++)         /* easy: every die that scores */
                if (pick < 0 || ch[i].k > ch[pick].k || (ch[i].k == ch[pick].k && ch[i].v > ch[pick].v)) pick = i;
        int x = p.turn_pts + ch[pick].v, left = p.nd - ch[pick].k;
        static const int AT[FK_DICE + 1] = {3000, 300, 300, 500, 1000, 2000, 3000};
        int at = level == AI_MEDIUM ? AT[left] : 250 + STEP * (int)(ai_mix(ai_session_seed(), p.turns) % 6);
        if (need && (level == AI_MEDIUM || ai_rand_n(&r, 3))) at = need;
        bank = x >= at && (entered || x >= FK_ENTRY);
    }
    if (ai_cancelled()) return 0;
    int len = snprintf(move, (size_t)cap, "a");
    for (int f = 1; f <= 6; f++)
        for (int k = 0; k < ch[pick].c[f]; k++) move[len++] = (char)('0' + f);
    move[len++] = ' ';
    if (bank) move[len++] = 'b';
    else {
        int left = p.nd - ch[pick].k ? p.nd - ch[pick].k : FK_DICE;
        move[len++] = 'd';
        for (int k = 0; k < left; k++) move[len++] = '?';
    }
    move[len] = 0;
    return 1;
}

static const bot farkle_bot = {.think = think, .play = farkle_show};
REGISTER_BOT(farkle, &farkle_bot)
