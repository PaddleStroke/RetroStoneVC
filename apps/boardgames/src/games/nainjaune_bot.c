/*
 * Le Nain jaune's bot (bots.h), every variant and player count: information-set Monte Carlo tree search (ai.h's
 * ai_mcts with determinise()) over the deal, from what its seat may know: its own hand, the cards played, and the
 * ranks each player was seen to lack (passed over when that rank was asked for: "without"). Each iteration deals
 * the unseen cards (the other hands and the talon) at random, keeping to those.
 * Runs on the worker thread: it copies the game at entry and reads only that copy. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   The reward: the chips a player wins in the deal (the compartments taken, the cards paid, grand opera) against
 *   the others'.
 *   The playouts (and easy's play): one of the five cards when the rank asked for allows it; a lead from the
 *   longest run in hand (it keeps the lead and empties the hand), the lowest among equals.
 * Levels: easy plays the playouts' choice with a slip now and then; medium a short search; hard a longer one (about
 * 0.3 s a card on a PC). Cards of one rank that are none of the five are alike: one of them is searched.
 * Its moves are the game's notation ("7d", "qs", "n"), played by nainjaune_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/nainjaune.h"

#define NJB_HARD_ITERS 6000

typedef struct {
    njgame g;                                   /* the deal (determinised: every hand dealt) */
    int16_t chips0[NJ_MAXP];                    /* the chips at the root */
    uint16_t lacks[NJ_MAXP];                    /* bit r: the player holds no card of rank r */
    uint8_t unseen[52], nunseen;                /* the cards the observer has not seen */
    uint8_t me;
} nsim;

static int has_rank(const njpos *p, int q, int r)
{
    for (int i = 0; i < p->n[q]; i++)
        if (CARD_RANK(p->hand[q][i]) == r) return 1;
    return 0;
}

static int s_moves(const void *st, int *out)
{
    const nsim *s = st;
    const njpos *p = &s->g.pos;
    if (p->phase != NP_PLAY) return 0;
    int q = p->turn, k = 0;
    uint16_t plain = 0;                         /* a plain card of that rank already offered */
    for (int i = 0; i < p->n[q]; i++) {
        int c = p->hand[q][i], r = CARD_RANK(c);
        if (p->need && r != p->need) continue;
        if (nj_box_of(c) < 0) {
            if (plain >> r & 1) continue;
            plain |= (uint16_t)(1u << r);
        }
        out[k++] = c;
    }
    return k;
}

static void s_make(void *st, int m) { nj_play(&((nsim *)st)->g, m); }
static int s_side(const void *st) { return ((const nsim *)st)->g.pos.turn; }

static int s_result(const void *st, float *rw)
{
    const nsim *s = st;
    const njpos *p = &s->g.pos;
    if (p->phase == NP_PLAY) return 0;
    int n = p->players, sum = 0;
    for (int q = 0; q < n; q++) sum += p->chips[q] - s->chips0[q];
    for (int q = 0; q < n; q++) {
        int d = p->chips[q] - s->chips0[q];
        float others = (float)(sum - d) / (n - 1), v = 0.5f + (d - others) / 40.0f;
        rw[q] = v < 0 ? 0 : v > 1 ? 1 : v;
    }
    return 1;
}

/* the playouts' choice (and easy's): one of the five when it may; a lead from the longest run, the lowest first */
static int heuristic(const nsim *s, ai_rng *r, int eps)
{
    int mv[64], n = s_moves(s, mv);
    if (n <= 1) return n ? mv[0] : AI_NONE;
    for (int i = 0; i < n; i++)                 /* one of the five: never missed */
        if (nj_box_of(mv[i]) >= 0) return mv[i];
    if (eps && ai_rand_n(r, 100) < eps) return mv[ai_rand_n(r, n)];
    const njpos *p = &s->g.pos;
    int q = p->turn, best = -(1 << 20), bm = mv[0];
    for (int i = 0; i < n; i++) {
        int c = mv[i], rk = CARD_RANK(c), w = nj_box_of(c) >= 0 ? 1000 : 0;
        if (!p->need) {
            int run = 1;
            while (rk + run <= RANK_KING && has_rank(p, q, rk + run)) run++;
            w += run * 20 - rk + (rk + run - 1 == RANK_KING ? 30 : 0);   /* a run up to the king keeps the lead */
        }
        if (w > best) best = w, bm = c;
    }
    return bm;
}

static int s_playout(const void *st, ai_rng *r) { return heuristic(st, r, 15); }

/* deals the unseen cards: the other hands (keeping to the ranks they lack), then the talon */
static void s_determinise(void *st, int seat, ai_rng *r)
{
    nsim *s = st;
    njpos *p = &s->g.pos;
    (void)seat;
    uint8_t pool[52];
    int n = s->nunseen;
    memcpy(pool, s->unseen, (size_t)n);
    for (int i = n - 1; i > 0; i--) {
        int j = ai_rand_n(r, i + 1);
        uint8_t t = pool[i];
        pool[i] = pool[j];
        pool[j] = t;
    }
    int order[NJ_MAXP], no = 0;
    for (int q = 0; q < p->players; q++)
        if (q != s->me) order[no++] = q;
    for (int i = 1; i < no; i++)                /* the most constrained first */
        for (int j = i; j > 0 && __builtin_popcount(s->lacks[order[j]]) > __builtin_popcount(s->lacks[order[j - 1]]);
             j--) {
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    uint8_t used[52] = {0};
    for (int k = 0; k < no; k++) {
        int q = order[k], got = 0;
        for (int pass = 0; pass < 2 && got < p->n[q]; pass++)   /* pass 1: anything (the view was too strict) */
            for (int i = 0; i < n && got < p->n[q]; i++) {
                if (used[i] || (!pass && s->lacks[q] >> CARD_RANK(pool[i]) & 1)) continue;
                used[i] = 1;
                p->hand[q][got++] = pool[i];
            }
    }
    int t = 0;
    for (int i = 0; i < n && t < p->ntalon; i++)
        if (!used[i]) p->talon[t++] = pool[i];
}

static const ai_game GAME = {
    .size = sizeof(nsim), .moves = s_moves, .make = s_make, .side = s_side, .result = s_result,
    .playout = s_playout, .determinise = s_determinise,
};

/* the view of seat me: its hand, the cards played, the ranks each was seen to lack (the deal replayed) */
static void view_of(const njgame *g, int me, nsim *s)
{
    memset(s, 0, sizeof *s);
    s->g = *g;
    s->me = (uint8_t)me;
    const njpos *p = &g->pos;
    for (int q = 0; q < p->players; q++) s->chips0[q] = p->chips[q];
    njgame *t = malloc(sizeof *t);
    if (t) {
        nj_new(t, g->variant, g->players, g->seed);
        for (int i = 0; i < g->n; i++) {
            int a = g->act[i];
            const njpos *q = &t->pos;
            int who = q->turn, r = a == NJ_NEXT ? 0 : CARD_RANK(a);
            if (a == NJ_NEXT) memset(s->lacks, 0, sizeof s->lacks);
            nj_play(t, a);
            if (a == NJ_NEXT || q->phase != NP_PLAY || r == RANK_KING || r == 0) continue;
            if (q->need) {                      /* passed over: from the player on to the holder */
                for (int o = who; o != q->turn; o = (o + 1) % q->players) s->lacks[o] |= (uint16_t)(1u << (r + 1));
            } else
                for (int o = 0; o < q->players; o++) s->lacks[o] |= (uint16_t)(1u << (r + 1));
        }
        free(t);
    }
    uint8_t seen[128] = {0};
    for (int i = 0; i < p->n[me]; i++) seen[p->hand[me][i]] = 1;
    for (int i = 0; i < p->nplayed; i++) seen[p->played[i]] = 1;
    uint8_t pack[52];
    int n = cards_pack(PACK_52, pack);
    for (int i = 0; i < n; i++)
        if (!seen[pack[i]]) s->unseen[s->nunseen++] = pack[i];
    /* the hidden hands and the talon as the view holds them: nothing but their counts is real */
    njpos *v = &s->g.pos;
    for (int q = 0; q < v->players; q++)
        if (q != me) memset(v->hand[q], CARD_NONE, sizeof v->hand[q]);
    memset(v->talon, CARD_NONE, sizeof v->talon);
}

/* the action for seat at a level (-1: nothing to do) */
int njb_choose(const njgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const njpos *p = &g->pos;
    if (p->phase == NP_DEALT) return NJ_NEXT;
    if (p->phase != NP_PLAY || p->turn != seat) return -1;
    nsim *s = malloc(sizeof *s);
    if (!s) return nj_auto(p);
    view_of(g, seat, s);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 19 + (uint64_t)seat));
    int mv[64], n = s_moves(s, mv), a;
    if (n <= 1) a = n ? mv[0] : -1;
    else if (level == AI_EASY) a = heuristic(s, &r, 25);
    else {
        ai_opts o = {.budget = ai_level_budget(level), .rng = &r, .uct_c = 0.6f};
        o.budget.nodes = level == AI_MEDIUM ? NJB_HARD_ITERS / 12 : NJB_HARD_ITERS;
        o.budget.ms = o.budget.ms * 7 / 10;
        a = ai_mcts(&GAME, s, &o, stats);
    }
    free(s);
    return a >= 0 && nj_legal(p, a) ? a : nj_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    njgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *nainjaune_live();                     /* the copy, at entry: nothing else is read */
    int a = njb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (a < 0 || ai_cancelled() || cap < 4) return 0;
    if (a == NJ_NEXT) snprintf(move, (size_t)cap, "n");
    else snprintf(move, (size_t)cap, "%c%c", "a23456789tjqk"[CARD_RANK(a) - 1], "shdc"[CARD_SUIT(a)]);
    return 1;
}

static int actor(void)
{
    const njpos *p = &nainjaune_live()->pos;
    return p->phase == NP_PLAY ? p->turn : -1;
}

static const bot nainjaune_bot = {.think = think, .actor = actor, .play = nainjaune_show};
REGISTER_BOT(nainjaune, &nainjaune_bot)
