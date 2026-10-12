/*
 * Pig's rules (pig.h): the rolls, the turn's total, holding, the 1s, the race to 100.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/pig.h"

void pig_new(piggame *g, int variant, int players)
{
    memset(g, 0, sizeof *g);
    pigpos *p = &g->pos;
    p->variant = (uint8_t)(variant == PIG_TWO ? PIG_TWO : PIG_ONE);
    p->players = (uint8_t)(players < 2 ? 2 : players > PIG_MAXP ? PIG_MAXP : players);
    p->winner = -1;
}

int pig_ndice(const pigpos *p) { return p->variant == PIG_TWO ? 2 : 1; }

static void end_turn(pigpos *p, int how, int pts)
{
    p->ended = (uint8_t)how;
    p->ended_by = p->turn;
    p->ended_pts = (int16_t)pts;
    p->end_dice[0] = p->dice[0];
    p->end_dice[1] = p->dice[1];
    p->total = 0;
    p->rolls = 0;
    p->ndice = 0;
    p->turns++;
    p->turn = (uint8_t)((p->turn + 1) % p->players);
}

int pig_roll(piggame *g, int d1, int d2)
{
    pigpos *p = &g->pos;
    int two = p->variant == PIG_TWO;
    if (p->phase != PIG_PLAY || d1 < 1 || d1 > 6 || (two && (d2 < 1 || d2 > 6))) return 0;
    g->before = *p;
    g->last_hold = 0;
    p->dice[0] = (uint8_t)d1;
    p->dice[1] = (uint8_t)(two ? d2 : 0);
    p->ndice = (uint8_t)(two ? 2 : 1);
    if (p->rolls >= PIG_SEQ) memmove(p->seq, p->seq + 1, sizeof p->seq[0] * (PIG_SEQ - 1));
    p->seq[p->rolls < PIG_SEQ ? p->rolls : PIG_SEQ - 1][0] = (uint8_t)d1;
    p->seq[p->rolls < PIG_SEQ ? p->rolls : PIG_SEQ - 1][1] = (uint8_t)(two ? d2 : 0);
    p->rolls++;
    p->ended_score = 0;
    if (two && d1 == 1 && d2 == 1) {            /* two 1s: the whole score lost too */
        p->ended_score = p->score[p->turn];
        p->score[p->turn] = 0;
        end_turn(p, PIG_WIPED, p->total);
    } else if (d1 == 1 || (two && d2 == 1)) end_turn(p, PIG_LOST, p->total);
    else p->total = (int16_t)(p->total + d1 + (two ? d2 : 0));
    return 1;
}

int pig_can_hold(const pigpos *p) { return p->phase == PIG_PLAY && p->rolls > 0; }

int pig_hold(piggame *g)
{
    pigpos *p = &g->pos;
    if (!pig_can_hold(p)) return 0;
    g->before = *p;
    g->last_hold = 1;
    int me = p->turn, pts = p->total;
    p->score[me] = (int16_t)(p->score[me] + pts);
    p->ended_score = 0;
    end_turn(p, PIG_HELD, pts);
    if (p->score[me] >= PIG_TARGET) {
        p->phase = PIG_OVER;
        p->winner = (int8_t)me;
        p->turn = (uint8_t)me;
    }
    return 1;
}

int pig_undo(piggame *g)
{
    if (!g->last_hold) return 0;
    g->pos = g->before;
    g->last_hold = 0;
    return 1;
}

int pig_leader(const pigpos *p)
{
    int w = 0;
    for (int q = 1; q < p->players; q++)
        if (p->score[q] > p->score[w]) w = q;
    return w;
}

int pig_script(piggame *g, const char *s)
{
    int two = g->pos.variant == PIG_TWO;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == 'h') {
            if (!pig_hold(g)) return 0;
            s++;
        } else if (*s == 'r' && s[1] >= '1' && s[1] <= '6') {
            int d2 = two ? s[2] - '0' : 0;
            if (two && (d2 < 1 || d2 > 6)) return 0;
            if (!pig_roll(g, s[1] - '0', d2)) return 0;
            s += two ? 3 : 2;
        } else return 0;
    }
    return 1;
}
