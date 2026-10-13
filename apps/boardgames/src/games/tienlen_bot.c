/*
 * Tiến lên's bot (bots.h), for 2 to 4 players: the climbing games' search (games/shed_bot.h: information-set Monte
 * Carlo tree search over the deal, the unseen cards dealt at random into the others' hands, the cards never dealt
 * out of play; going out first wins, the places count a little).
 * Runs on the worker thread: it copies the game at entry and reads only what its seat may know (its hand, every
 * card played). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 * Its moves are the game's notation (the cards played: "3d4c5h6s7d"; p a pass), played by tienlen_show().
 */
#include "bots.h"
#include "games/tienlen.h"
#include "games/shed_bot.h"

#define HARD_ITERS 15000                        /* about 0.3 s a move on a PC */

uint64_t tienlen_bot_choose(const shed_game *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    return sb_choose(&tln_rules, g, seat, level, 1, HARD_ITERS, seed, stats);
}

static int think(int seat, int level, char *move, int cap)
{
    shed_game *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *tienlen_live();                          /* the copy, at entry: nothing else is read */
    uint64_t a = tienlen_bot_choose(g, seat, level, ai_mix(ai_session_seed(), (uint64_t)g->n), NULL);
    free(g);
    if (!a || ai_cancelled()) return 0;
    return sb_write(&tln_rules, a, move, cap);
}

static int actor(void)
{
    const shed_pos *p = &tienlen_live()->pos;
    return p->phase == SP_PLAY ? p->turn : -1;
}

static const bot tienlen_bot = {.think = think, .actor = actor, .play = tienlen_show};
REGISTER_BOT(tienlen, &tienlen_bot)
