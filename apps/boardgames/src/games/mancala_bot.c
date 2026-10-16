/*
 * Mancala's bot (bots.h), Oware and Kalah: alpha-beta (ai.h) over a small copy of the position (the pits, the stores,
 * the player to move), sown as the rules sow it (mancala_rules.c: Oware's skip of the pit sown, its captures, the
 * grand slam, the duty to feed, the end over 24; Kalah's store, extra turn and capture opposite, the sweep). The
 * evaluation: the stores, then the seeds each side keeps (Kalah: they go to their side's player at the end; Oware: a
 * reserve that feeds the long sowings), the other side's pits of 1 or 2 seeds (ready to be taken: Oware) and the pits
 * whose last seed would fall in the store (Kalah: an extra turn). The rules' end by a repeated position is left to the
 * rules (the search does not see it).
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation (the pit sown, "1".."12"), played by mancala_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/mancala.h"

typedef struct {
    uint8_t s[MC_SLOTS];                        /* the pits 0..11, the stores 12, 13 */
    uint8_t turn, over, kalah;
} mcbpos;

static int side_sum(const mcbpos *p, int player)
{
    int n = 0;
    for (int i = 6 * player; i < 6 * player + 6; i++) n += p->s[i];
    return n;
}

static int legal(const mcbpos *p, int pit)      /* as mc_legal */
{
    if (p->over || MC_OWNER(pit) != p->turn || !p->s[pit]) return 0;
    if (!p->kalah && !side_sum(p, !p->turn)) return p->s[pit] >= 6 * (p->turn + 1) - pit;
    return 1;
}

static int next_slot(int kalah, int s, int player)
{
    if (!kalah) return (s + 1) % MC_PITS;
    int n = s == 5 ? MC_STORE : s == MC_STORE ? 6 : s == 11 ? MC_STORE + 1 : s == MC_STORE + 1 ? 0 : s + 1;
    return n == MC_STORE + !player ? next_slot(kalah, n, player) : n;
}

static void sweep(mcbpos *p)
{
    for (int i = 0; i < MC_PITS; i++) {
        p->s[MC_STORE + MC_OWNER(i)] += p->s[i];
        p->s[i] = 0;
    }
    p->over = 1;
}

/* mc_play without the history (no repetition, no move limit) */
static void mcb_make(mcbpos *p, int pit)
{
    int me = p->turn, s = pit, n = p->s[pit];
    p->s[pit] = 0;
    for (int i = 0; i < n; i++) {
        do s = next_slot(p->kalah, s, me);
        while (s == pit && !p->kalah);         /* Oware only: Kalah sows the emptied pit too */
        p->s[s]++;
    }
    if (!p->kalah) {
        int k = s, got = 0;
        for (; k >= 0 && k < MC_PITS && MC_OWNER(k) != me && (p->s[k] == 2 || p->s[k] == 3); k--) got += p->s[k];
        if (got && got != side_sum(p, !me))     /* not a grand slam: take them */
            for (k = s; k >= 0 && k < MC_PITS && MC_OWNER(k) != me && (p->s[k] == 2 || p->s[k] == 3); k--) {
                p->s[MC_STORE + me] += p->s[k];
                p->s[k] = 0;
            }
        p->turn = (uint8_t)!me;
        if (p->s[MC_STORE + me] > 24) { p->over = 1; return; }
        int any = 0;
        for (int i = 0; i < MC_PITS && !any; i++) any = legal(p, i);
        if (!any) sweep(p);
    } else {
        int again = s == MC_STORE + me;
        if (!again && s < MC_PITS && MC_OWNER(s) == me && p->s[s] == 1 && p->s[11 - s]) {
            p->s[MC_STORE + me] += p->s[11 - s] + 1;
            p->s[11 - s] = p->s[s] = 0;
        }
        p->turn = (uint8_t)(again ? me : !me);
        if (!side_sum(p, 0) || !side_sum(p, 1)) sweep(p);
    }
}

static void mcb_from(const mc_game *g, mcbpos *p)
{
    const mc_pos *q = mc_now(g);
    memcpy(p->s, q->seeds, MC_SLOTS);
    p->turn = q->turn;
    p->over = q->over;
    p->kalah = g->variant == MC_KALAH;
}

/* the test's check: the bot's sowing of `pit` in `before` gives the rules' position `after`: 1 the same, 2 the rules
 * ended the game (a repetition, the move limit) where the bot plays on, 0 different */
int mcb_agrees(const mc_game *before, const mc_game *after, int pit)
{
    mcbpos a, b;
    mcb_from(before, &a);
    mcb_from(after, &b);
    mcb_make(&a, pit);
    if (!memcmp(&a, &b, sizeof a)) return 1;
    return b.over && !a.over ? 2 : 0;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    const mcbpos *p = st;
    int n = 0;
    for (int i = 6 * p->turn; i < 6 * p->turn + 6; i++)
        if (legal(p, i)) out[n++] = i;
    return n;
}

static void a_make(void *st, int m) { mcb_make(st, m); }
static int a_side(const void *st) { return ((const mcbpos *)st)->turn; }

static int a_over(const void *st, int *score)
{
    const mcbpos *p = st;
    if (!p->over) return 0;
    int d = p->s[MC_STORE + p->turn] - p->s[MC_STORE + !p->turn];
    *score = d > 0 ? ai_win(d) : d < 0 ? -ai_win(-d) : 0;
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const mcbpos *p = st;
    uint64_t a, b = 0;
    memcpy(&a, p->s, 8);
    memcpy(&b, p->s + 8, MC_SLOTS - 8);
    return ai_mix(a, ai_mix(b, (uint64_t)p->turn << 1 | p->kalah));
}

/* the side to move's view; 100 a seed in the store */
static int a_eval(const void *st)
{
    const mcbpos *p = st;
    int me = p->turn, v = 100 * (p->s[MC_STORE + me] - p->s[MC_STORE + !me]);
    int mine = side_sum(p, me), theirs = side_sum(p, !me);
    if (p->kalah) {
        v += 45 * (mine - theirs);              /* the seeds left go to their side's player */
        for (int i = 0; i < 6; i++) {           /* a pit that ends in the store: an extra turn */
            int a = 6 * me + i, b = 6 * !me + i;
            v += 30 * (p->s[a] == 6 - i) - 30 * (p->s[b] == 6 - i);
        }
    } else {
        v += 12 * (mine - theirs);              /* a reserve: long sowings, and the other must be fed */
        for (int i = 0; i < 6; i++) {           /* pits of 1 or 2: a sowing makes them 2 or 3, to take */
            int a = 6 * me + i, b = 6 * !me + i;
            v += 18 * (p->s[b] == 1 || p->s[b] == 2) - 18 * (p->s[a] == 1 || p->s[a] == 2);
        }
        if (!theirs) v += 40;
        if (!mine) v -= 40;
    }
    return v;
}

static int a_order(const void *st, int m)       /* the extra turns, then the captures, then the long sowings */
{
    const mcbpos *p = st;
    int n = p->s[m], me = p->turn;
    if (p->kalah) {
        int to_store = 6 * me + 6 - m;          /* the seeds to reach the store */
        if (n == to_store) return 50;
        int land = (m + n) % 13;                /* roughly: where the last seed falls on a short sowing */
        if (n < to_store && p->s[land] == 0 && p->s[11 - land]) return 20 + p->s[11 - land];
        return 0;
    }
    int land = (m + n + n / 12) % MC_PITS;
    if (n < 12 && MC_OWNER(land) != me && (p->s[land] == 1 || p->s[land] == 2)) return 30;
    return n > 11 ? 10 : 0;
}

static const ai_game ADAPTER = {
    .size = sizeof(mcbpos), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .unit = 100,
};

/* the pit to sow (0..11) in the game's position (its player to move), at a level; -1: none */
int mcb_choose(const mc_game *g, int level, uint64_t seed, ai_stats *stats)
{
    mcbpos pos, *p = &pos;
    mcb_from(g, p);
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, a_hash(p)));
    ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .tt_bits = 18};
    o.budget.ms = o.budget.ms * 5 / 6;                 /* the clock is checked every so many nodes: a margin */
    switch (level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM) {
    case AI_EASY:   o.budget.depth = 2; o.budget.nodes = 2000; o.budget.noise = 250; break;
    case AI_MEDIUM: o.budget.depth = 6; o.budget.nodes = 40000; o.budget.noise = 50; break;
    default:        o.budget.nodes = 1500000; o.tt_bits = 19; break;   /* about 0.3 s on a PC */
    }
    return ai_ab(&ADAPTER, p, &o, stats);
}

static int think(int seat, int level, char *move, int cap)
{
    const mc_game *g = mancala_live();          /* read once, at entry (the main thread leaves it alone meanwhile) */
    const mc_pos *now = mc_now(g);
    if (now->over || now->turn != seat || cap < 3) return 0;
    int m = mcb_choose(g, level, ai_session_seed(), NULL);
    if (m < 0 || ai_cancelled()) return 0;
    snprintf(move, (size_t)cap, "%d", m + 1);
    return 1;
}

static const bot mancala_bot = {.think = think, .play = mancala_show};
REGISTER_BOT(mancala, &mancala_bot)
