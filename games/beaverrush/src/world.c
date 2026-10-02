/*
 * Beaver Rush: a run, one beaver or up to four (versus): gnawing, the hits, the timer, milestones, stolen
 * branches and the last beaver standing. No drawing, no sound (tests/test_rules.c links it alone).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
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
        b->sent_to = -1;
        if (p < w->players) tree_init(&b->tr, seed);   /* versus: the same seed, the same trunk */
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

int world_standing(const world *w)
{
    int n = 0;
    for (int p = 0; p < w->players; p++) n += w->bv[p].state == BV_PLAY || w->bv[p].state == BV_READY;
    return n;
}

int world_steal_target(const world *w, int p)
{
    int best = -1;
    for (int k = 1; k < w->players; k++) {        /* p+1, p+2, ...: the first of equal scores keeps it */
        int q = (p + k) % w->players;
        if (w->bv[q].state != BV_PLAY) continue;
        if (best < 0 || w->bv[q].score > w->bv[best].score) best = q;
    }
    return best;
}

void world_rank_keys(const world *w, int key[MAX_PLAYERS])
{
    for (int p = 0; p < MAX_PLAYERS; p++) {
        const beaver *b = &w->bv[p];
        int when = b->out_t ? b->out_t : w->t + 1;      /* still standing (or the winner): after everyone */
        key[p] = p < w->players ? when * 4096 + clampi(b->score, 0, 4095) : -1;
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
                if (reroll) tree_reroll(&w->bv[p].tr, reroll);   /* the same re-roll: still the same trees */
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
    /* stolen branches: a milestone sends a branch to the leader among the others still gnawing */
    if (w->players >= 2)
        for (int p = 0; p < w->players; p++) {
            if (!(w->events[p] & EV_MILESTONE) || w->bv[p].state != BV_PLAY) continue;
            int q = world_steal_target(w, p);
            if (q < 0) continue;
            beaver *r = &w->bv[q];
            if (tree_insert_branch(&r->tr, CHIP_SLOT, r->side) >= 0) {
                w->bv[p].stolen_sent++;
                w->bv[p].sent_to = q;
                w->events[p] |= EV_SENT;
                w->events[q] |= EV_STOLEN;
            }
        }
    if (w->over || !w->started) return;
    if (w->players == 1) {
        if (w->bv[0].state != BV_PLAY) w->over = 1;
        return;
    }
    /* versus: a beaver out (bonked, out of breath) is out; the others go on; the last one standing wins */
    for (int p = 0; p < w->players; p++)
        if (w->bv[p].state != BV_PLAY && !w->bv[p].out_t) w->bv[p].out_t = w->t;
    int left = world_standing(w);
    if (left > 1) return;
    w->over = 1;
    int key[MAX_PLAYERS], best = 0, tie = 0;
    world_rank_keys(w, key);
    for (int p = 1; p < w->players; p++) {
        if (key[p] > key[best]) { best = p; tie = 0; }
        else if (key[p] == key[best]) tie = 1;
    }
    w->winner = tie ? -1 : best;
    if (left == 1) set_state(&w->bv[best], BV_WIN);   /* the one left standing */
}

void world_skip(world *w, int logs)
{
    for (int p = 0; p < w->players; p++) {
        beaver *b = &w->bv[p];
        for (int i = 0; i < logs; i++) {
            const seg *s0 = &b->tr.s[0], *s1 = &b->tr.s[1];
            if (s0->branch == b->side || s1->branch == b->side) b->side = other_side(b->side);
            b->golds += s0->gold;
            b->score += 1 + (s0->gold ? GOLD_BONUS : 0);
            b->logs++;
            tree_pop(&b->tr);
        }
        b->level = level_of(b->logs);
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
