/*
 * The Game of the Goose's bot (bots.h): the game has no decision at all, only the throw: the bot throws its two dice
 * when its turn comes ("roll", played by goose_show() with the game's own dice, as a tap throws them). The turn is
 * the rules' (the inn's missed turns, the well and the prison passed over). The levels play alike.
 * Runs on the worker thread: it reads the game's turn at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "bots.h"
#include "games/goose.h"

static int think(int seat, int level, char *move, int cap)
{
    (void)level;
    const goo_game *g = goose_live();
    int playing = g->winner < 0, turn = g->turn;   /* the copy, at entry */
    if (!playing || turn != seat || cap < 5) return 0;
    strcpy(move, "roll");
    return 1;
}

static const bot goose_bot = {.think = think, .play = goose_show};
REGISTER_BOT(goose, &goose_bot)
