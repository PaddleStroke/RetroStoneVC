/*
 * Ship, Captain and Crew's rules (shipcaptain.h): the rolls, the ship, captain and crew set aside in order, the cargo,
 * the rounds and their chips.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/shipcaptain.h"

static void new_turn(sccpos *p)
{
    memset(p->dice, 0, sizeof p->dice);
    p->aside[0] = p->aside[1] = p->aside[2] = -1;
    p->rolls = 0;
}

void scc_new(sccgame *g, int players)
{
    memset(g, 0, sizeof *g);
    sccpos *p = &g->pos;
    p->players = (uint8_t)(players < 2 ? 2 : players > SCC_MAXP ? SCC_MAXP : players);
    for (int q = 0; q < SCC_MAXP; q++) p->res[q] = p->last_res[q] = -1;
    new_turn(p);
}

int scc_complete(const sccpos *p) { return p->aside[SCC_CREW] >= 0; }

static int is_aside(const sccpos *p, int i) { return p->aside[0] == i || p->aside[1] == i || p->aside[2] == i; }

int scc_cargo(const sccpos *p)
{
    if (!scc_complete(p)) return 0;
    int sum = 0;
    for (int i = 0; i < SCC_DICE; i++)
        if (!is_aside(p, i)) sum += p->dice[i];
    return sum;
}

unsigned scc_free(const sccpos *p)
{
    unsigned m = 0;
    for (int i = 0; i < SCC_DICE; i++)
        if (!is_aside(p, i)) m |= 1u << i;
    return m;
}

/* the round settled: the highest cargo's players win a chip each; the game over at SCC_TARGET */
static void settle(sccpos *p)
{
    int best = 0;
    for (int q = 0; q < p->players; q++) best = p->res[q] > best ? p->res[q] : best;
    p->last_won = 0;
    for (int q = 0; q < p->players; q++) {
        p->last_res[q] = p->res[q];
        if (best > 0 && p->res[q] == best) {
            p->chips[q]++;
            p->last_won |= (uint8_t)(1u << q);
        }
        p->res[q] = -1;
    }
    p->rounds++;
    p->done = 0;
    int top = 0;
    for (int q = 0; q < p->players; q++) top = p->chips[q] > top ? p->chips[q] : top;
    if (top >= SCC_TARGET) {
        p->phase = SCC_OVER;
        for (int q = 0; q < p->players; q++)
            if (p->chips[q] == top) p->winners |= (uint8_t)(1u << q);
        return;
    }
    p->starter = (uint8_t)((p->starter + 1) % p->players);
    p->turn = p->starter;
}

static void end_turn(sccpos *p)
{
    p->end_by = p->turn;
    memcpy(p->end_dice, p->dice, sizeof p->dice);
    memcpy(p->end_aside, p->aside, sizeof p->aside);
    p->end_cargo = (int8_t)scc_cargo(p);
    p->res[p->turn] = (int8_t)scc_cargo(p);
    p->done |= (uint8_t)(1u << p->turn);
    if (p->done == (1u << p->players) - 1) settle(p);
    else p->turn = (uint8_t)((p->turn + 1) % p->players);
    if (p->phase == SCC_PLAY) new_turn(p);
}

int scc_roll(sccgame *g, const int *faces)
{
    sccpos *p = &g->pos;
    if (p->phase != SCC_PLAY || p->rolls >= SCC_ROLLS) return 0;
    unsigned m = scc_free(p);
    for (int i = 0; i < SCC_DICE; i++)
        if ((m >> i & 1) && (faces[i] < 1 || faces[i] > 6)) return 0;
    g->before = *p;
    g->last_stop = 0;
    for (int i = 0; i < SCC_DICE; i++)
        if (m >> i & 1) p->dice[i] = (uint8_t)faces[i];
    p->rolls++;
    for (int k = 0; k < 3; k++) {               /* the ship, then the captain, then the crew, as they come */
        if (p->aside[k] >= 0) continue;
        int want = 6 - k, found = -1;
        for (int i = 0; i < SCC_DICE && found < 0; i++)
            if ((m >> i & 1) && !is_aside(p, i) && p->dice[i] == want) found = i;
        if (found < 0) break;
        p->aside[k] = (int8_t)found;
    }
    if (p->rolls >= SCC_ROLLS) end_turn(p);
    return 1;
}

int scc_can_stop(const sccpos *p) { return p->phase == SCC_PLAY && scc_complete(p) && p->rolls < SCC_ROLLS; }

int scc_stop(sccgame *g)
{
    if (!scc_can_stop(&g->pos)) return 0;
    g->before = g->pos;
    g->last_stop = 1;
    end_turn(&g->pos);
    return 1;
}

int scc_undo(sccgame *g)
{
    if (!g->last_stop) return 0;
    g->pos = g->before;
    g->last_stop = 0;
    return 1;
}

int scc_script(sccgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == 's') {
            if (!scc_stop(g)) return 0;
            s++;
            continue;
        }
        if (*s != 'r') return 0;
        s++;
        int faces[SCC_DICE] = {0};
        unsigned m = scc_free(&g->pos);
        for (int i = 0; i < SCC_DICE; i++) {
            if (!(m >> i & 1)) continue;
            if (*s < '1' || *s > '6') return 0;
            faces[i] = *s++ - '0';
        }
        if (!scc_roll(g, faces)) return 0;
    }
    return 1;
}
