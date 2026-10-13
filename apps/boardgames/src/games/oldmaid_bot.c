/*
 * Old Maid's bot (bots.h): the hand it draws from is shuffled before each draw and lies face down, so no card can be
 * told from another: the bot draws one at random (the same at every level; the pairs go down by themselves). It
 * reads nothing but the number of cards offered.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its move is the game's notation: "d3", the offered hand's fourth card, played by oldmaid_show() (drawn as a tap
 * does).
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/oldmaid.h"

static int think(int seat, int level, char *move, int cap)
{
    (void)level;
    const opos *live = &oldmaid_live()->pos;
    int turn = live->turn, phase = live->phase, o = om_offerer(live);   /* the copy, at entry: no card read */
    int n = o >= 0 ? live->n[o] : 0;
    if (phase != OP_DRAW || turn != seat || n < 1 || cap < 4 || ai_cancelled()) return 0;
    ai_rng r;
    ai_seed(&r, ai_mix(ai_session_seed(), (uint64_t)seat * 53 + (uint64_t)n));
    snprintf(move, (size_t)cap, "d%d", ai_rand_n(&r, n));
    return 1;
}

static const bot oldmaid_bot = {.think = think, .play = oldmaid_show};
REGISTER_BOT(oldmaid, &oldmaid_bot)
