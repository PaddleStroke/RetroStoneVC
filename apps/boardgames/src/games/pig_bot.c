/*
 * Pig's bot (bots.h), both variants (one die, two dice) and two to six players. The roll is a move, "r?" (two dice:
 * "r??"), played by pig_show() with the game's own dice as a tap rolls them; "h" holds.
 * The others' score that counts is the best of them (the one who may win first).
 *   easy    holds once the turn's total reaches its whim of the turn (10 to 24; two dice: 8 to 22), or after a few
 *           rolls; most of the time it sees a hold that wins;
 *   medium  holds at 20 (two dice: at 18), or as soon as holding wins;
 *   hard    holds as a near-best player does: as soon as holding wins; when it or the leader has 71 or more, it rolls
 *           on to win (the end race); else it holds at 21 plus an eighth of the gap to the leader (keeping pace:
 *           behind, it risks more; ahead, less). Two dice: at the total where a roll's gain (8 points 25 times in 36)
 *           no longer pays for its risks (the turn's total 10 times in 36, the whole score once in 36), plus a
 *           tenth of the gap; the end race once the leader has 75.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/pig.h"

/* 1: hold now */
static int holds(const pigpos *p, int level, ai_rng *r)
{
    int me = p->turn, i = p->score[me], k = p->total, j = 0, two = p->variant == PIG_TWO;
    for (int q = 0; q < p->players; q++)
        if (q != me && p->score[q] > j) j = p->score[q];
    if (!pig_can_hold(p)) return 0;
    if (i + k >= PIG_TARGET && (level > AI_EASY || ai_rand_n(r, 5))) return 1;
    switch (level) {
    case AI_EASY: {
        ai_rng whim;                            /* the same whim all the turn */
        ai_seed(&whim, ai_mix((uint64_t)p->turns * 7 + (uint64_t)me, (uint64_t)(i * 131 + j)));
        int at = (two ? 8 : 10) + ai_rand_n(&whim, 15), rolls = 2 + ai_rand_n(&whim, 4);
        return k >= at || p->rolls >= rolls;
    }
    case AI_MEDIUM: return k >= (two ? 18 : 20);
    default:
        if (!two) {
            if (i >= 71 || j >= 71) return 0;   /* the end race: on to 100 */
            int gap = j - i;
            return k >= 21 + (gap >= 0 ? gap + 4 : gap - 4) / 8;
        } else {
            if (j >= 80) return 0;
            int gap = j - i, at = 18 + (gap >= 0 ? gap + 4 : gap - 4) / 8;
            at = at < 10 ? 10 : at > 30 ? 30 : at;
            return k >= at;
        }
    }
}

static int think(int seat, int level, char *move, int cap)
{
    pigpos p = pig_live()->pos;                 /* the copy, at entry: nothing else is read */
    if (p.phase != PIG_PLAY || p.turn != seat || cap < 4) return 0;
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), ai_mix(p.turns, p.rolls)));
    strcpy(move, holds(&p, level, &r) ? "h" : p.variant == PIG_TWO ? "r??" : "r?");
    return !ai_cancelled();
}

static const bot pig_bot = {.think = think, .play = pig_show};
REGISTER_BOT(pig, &pig_bot)
