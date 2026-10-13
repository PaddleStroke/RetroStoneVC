/*
 * Snakes and ladders' bot (bots.h): the game has no decision at all, only the throw: the bot throws when its turn
 * comes ("roll", played by snakes_show() with the game's own die, as a tap throws it). The levels play alike.
 * Runs on the worker thread: it reads the game's turn at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "bots.h"
#include "games/snakes.h"

static int think(int seat, int level, char *move, int cap)
{
    (void)level;
    const sn_game *g = snakes_live();
    int playing = g->winner < 0, turn = g->turn;   /* the copy, at entry */
    if (!playing || turn != seat || cap < 5) return 0;
    strcpy(move, "roll");
    return 1;
}

static const bot snakes_bot = {.think = think, .play = snakes_show};
REGISTER_BOT(snakes, &snakes_bot)
