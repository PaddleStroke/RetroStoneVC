/*
 * Beaver Rush: a run, one beaver or two (versus): gnawing, the hits, the timer, milestones and stolen
 * chips. No drawing, no sound (tests/test_rules.c links it alone).
 * (c) 2026 Pierre-Louis Boyer (8BCraft). All rights reserved: games/beaverrush/LICENSE.
 */
#include "br.h"
#include <string.h>

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->players = clampi(players, 1, MAX_PLAYERS);
    w->seed = seed;
    w->winner = -1;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        beaver *b = &w->bv[p];
        b->state = p < w->players ? BV_READY : BV_OFF;
        b->side = SIDE_L;
        b->bar = BAR_START;
        b->gnaw_t = 1000;
        tree_init(&b->tr, seed);                 /* versus: the same seed, the same trunk */
    }
}

static void set_state(beaver *b, int s)
{
    b->state = s;
    b->t = 0;
}

static void gnaw(world *w, int p, int side)
{
    beaver *b = &w->bv[p];
    int *ev = &w->events[p];
    if (side != b->side) *ev |= EV_MOVE;
    b->side = side;
    seg s0 = b->tr.s[0];
    if (s0.branch == side) {                     /* hit B: walked into the branch at head height */
        b->bonk_hit = 2;
        set_state(b, BV_BONK);
        *ev |= EV_BONK;
        return;
    }
    b->last_gnawed = s0.branch | s0.gold << 2 | s0.stolen << 3;
    tree_pop(&b->tr);
    b->logs++;
    b->golds += s0.gold;
    b->score += 1 + (s0.gold ? GOLD_BONUS : 0);
    int32_t add = REFILL + (s0.gold ? GOLD_REFILL : 0);
    b->bar = b->bar > BAR_FULL - add ? BAR_FULL : b->bar + add;
    b->gnaw_t = 0;
    *ev |= EV_GNAW | (s0.gold ? EV_GOLD : 0);
    int lv = level_of(b->logs);
    if (lv != b->level) { b->level = lv; *ev |= EV_LEVEL; }
    if (b->logs % STAGE_LOGS == 0) *ev |= EV_MILESTONE;
    if (b->tr.s[0].branch == side) {             /* hit A: the drop brought a branch onto its head */
        b->bonk_hit = 1;
        set_state(b, BV_BONK);
        *ev |= EV_BONK;
    }
}

void world_step(world *w, const int press[MAX_PLAYERS], uint32_t reroll)
{
    w->t++;
    memset(w->events, 0, sizeof w->events);
    if (!w->started) {
        int any = 0;
        for (int p = 0; p < w->players; p++) any |= press[p] != 0;
        if (any) {
            w->started = 1;
            for (int p = 0; p < w->players; p++) {
                if (reroll) tree_reroll(&w->bv[p].tr, reroll);
                set_state(&w->bv[p], BV_PLAY);
                w->events[p] |= EV_START;
            }
        }
    }
    for (int p = 0; p < w->players; p++) {
        beaver *b = &w->bv[p];
        b->t++;
        b->gnaw_t++;
        if (b->state == BV_PLAY && !w->over && press[p]) gnaw(w, p, press[p]);
    }
    for (int p = 0; p < w->players; p++) {
        beaver *b = &w->bv[p];
        if (b->state != BV_PLAY || w->over) continue;
        b->bar -= drain_per_frame(b->level);
        if (b->bar <= 0) {
            b->bar = 0;
            set_state(b, BV_SLEEP);
            w->events[p] |= EV_SLEEP;
        }
    }
    /* stolen chips: a milestone sends a branch to the rival */
    if (w->players == 2)
        for (int p = 0; p < 2; p++) {
            beaver *r = &w->bv[1 - p];
            if (!(w->events[p] & EV_MILESTONE) || w->bv[p].state != BV_PLAY || r->state != BV_PLAY) continue;
            if (tree_insert_branch(&r->tr, CHIP_SLOT, r->side) >= 0) {
                w->bv[p].stolen_sent++;
                w->events[p] |= EV_SENT;
                w->events[1 - p] |= EV_STOLEN;
            }
        }
    if (w->over || !w->started) return;
    if (w->players == 1) {
        if (w->bv[0].state != BV_PLAY) w->over = 1;
        return;
    }
    int out0 = w->bv[0].state != BV_PLAY, out1 = w->bv[1].state != BV_PLAY;
    if (!out0 && !out1) return;
    w->over = 1;
    if (out0 && out1) {
        int a = w->bv[0].score, b = w->bv[1].score;
        w->winner = a > b ? 0 : b > a ? 1 : -1;
    } else {
        w->winner = out0 ? 1 : 0;
        set_state(&w->bv[w->winner], BV_WIN);
    }
}

int world_dam_logs(const world *w)
{
    int n = 0;
    for (int p = 0; p < w->players; p++) n += w->bv[p].logs;
    return n;
}

int world_stage(const world *w)
{
    int n = 0;
    for (int p = 0; p < w->players; p++)
        if (w->bv[p].logs > n) n = w->bv[p].logs;
    return n / STAGE_LOGS;
}

int medal_of(int score)
{
    return score >= MEDAL_T4 ? 4 : score >= MEDAL_T3 ? 3 : score >= MEDAL_T2 ? 2 : score >= MEDAL_T1 ? 1 : 0;
}
