/*
 * War's bot (bots.h): war has no decision to make (nobody knows the order of their pile; the cards won go under it in
 * no set order), so the bot only turns its card when it is its turn to; every level plays alike.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its move is the game's notation: the digit of its own pile ("1"), played by war_show() (the card turned as a tap
 * does). The actor is the game's turn(): the first player still to turn a card in this step.
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/war.h"

static int think(int seat, int level, char *move, int cap)
{
    (void)level;
    wpos p = war_live()->pos;                   /* the copy, at entry: nothing else is read */
    if (!war_legal(&p, seat) || cap < 2 || ai_cancelled()) return 0;
    snprintf(move, (size_t)cap, "%d", seat);
    return 1;
}

static const bot war_bot = {.think = think, .play = war_show};
REGISTER_BOT(war, &war_bot)
