/*
 * 421 (quatre cent vingt et un), the French café dice game, for two to six players on one device: the rules (the
 * combinations and their ranking, the charge, the discharge, the chips, the end). No SDL, no dice: the caller throws
 * and gives the rules the values. The most common rules, as chosen here:
 * - Three dice and eleven chips (FTO_CHIPS), all in the pot at the start.
 * - The combinations, best first, and what each is worth in chips: 4-2-1: 10; 1-1-1: 7; the "fiches" 1-1-6, 6-6-6:
 *   6; 1-1-5, 5-5-5: 5; 1-1-4, 4-4-4: 4; 1-1-3, 3-3-3: 3; 1-1-2, 2-2-2: 2 (a fiche 1-1-x beats the three x of the
 *   same worth); the runs 6-5-4, 5-4-3, 4-3-2, 3-2-1: 2; any other throw: 1, the higher dice the better (6-6-5 beats
 *   6-6-4, then 6-5-3...); the nénette 2-2-1 last of all.
 * - A round: each player throws in turn round the table, from the round's leader (the first round: Ivory; then the
 *   loser of the last round). Equal throws: the one made in fewer throws is better, then the one made first.
 * - The charge: one throw each. The round's loser (the worst throw) takes from the pot as many chips as the best
 *   throw is worth (what is left, at the end). When the pot is empty, the discharge begins.
 * - The discharge: up to three throws each, keeping any dice between them; the leader may stop after one or two, and
 *   the others may then throw no more times than the leader did. The round's winner (the best throw) gives the loser
 *   as many of their own chips as their throw is worth (all they have, if fewer).
 * - Out of chips: a player with none at the end of the charge, or who gives away their last chip in the discharge,
 *   is out of the game, safe: they have won. The game ends when one player holds all the chips: that player has lost
 *   (also at the end of the charge, if one player took them all), and everyone else has won. The first player out
 *   is recorded too (first_out).
 * Notation (script, the screenshots and the tests): a throw "d" and the three dice in their places, a digit for a die
 * thrown, "-" for a die kept ("d421", then "d4-1"); "s": stop (end the turn before the limit): "d651 d221 d43- s".
 */
#ifndef FOURTWOONE_H
#define FOURTWOONE_H

#include <stdint.h>

#define FTO_PLAYERS 6
#define FTO_CHIPS 11
#define FTO_DICE 3
#define FTO_THROWS 3
#define FTO_MAX_EVENTS 4096
#define FTO_THROW_EV 0x8000u                    /* an event: a throw (| the faces, 3 bits each), else a stop */

enum { FTO_CHARGE, FTO_DISCHARGE, FTO_OVER };

typedef struct {
    uint8_t chips[FTO_PLAYERS], pot;
    uint8_t players, phase;
    uint8_t in;                                 /* bit p: player p still in the game (has not gone out) */
    uint8_t turn, leader;                       /* the player to throw; the round's first */
    uint8_t done;                               /* bit p: player p has played this round */
    uint8_t order[FTO_PLAYERS], norder;         /* who has played this round, in order */
    uint8_t dice[FTO_DICE], kept, throws;       /* the player to throw's dice; bit i: die i kept; throws so far */
    uint8_t limit;                              /* the most throws this round */
    uint8_t res[FTO_PLAYERS][FTO_DICE], res_throws[FTO_PLAYERS];  /* this round's results */
    /* the last round settled (the picture): who won and lost it, how many chips went, from the pot or not, the
     * results then and who played it */
    int8_t last_win, last_lose;
    uint8_t last_amount, last_pot, last_in;
    uint8_t last_res[FTO_PLAYERS][FTO_DICE], last_throws[FTO_PLAYERS], last_order[FTO_PLAYERS];
    uint16_t rounds;
    int8_t loser;                               /* the player left holding all the chips (-1: none yet) */
    int8_t first_out;                           /* the first player out (-1: none yet) */
} tpos;

/* A game: the position before every event, so undo is free. */
typedef struct {
    tpos pos, past[FTO_MAX_EVENTS];
    uint16_t ev[FTO_MAX_EVENTS];
    int n;
} tgame;

void fto_new(tgame *g, int players);
int  fto_rank(const uint8_t d[FTO_DICE]);       /* higher is better (the nénette 0) */
int  fto_value(const uint8_t d[FTO_DICE]);      /* what a throw is worth in chips */
/* 1: a's result better than b's in this round (the combination, then fewer throws, then played first) */
int  fto_better(const tpos *p, int a, int b);
int  fto_best(const tpos *p);                   /* this round's best and worst so far (-1: nobody yet) */
int  fto_worst(const tpos *p);
int  fto_throw(tgame *g, unsigned kept, const int values[FTO_DICE]);  /* the faces of the dice not kept; 0: not now */
int  fto_keep(tgame *g, unsigned kept);         /* the dice to keep for the next throw (no event); 0: not now */
int  fto_can_stop(const tpos *p);               /* 1: the player to throw may stop now */
int  fto_stop(tgame *g);                        /* ends the turn before the limit; 0: not now */
int  fto_undo(tgame *g);                        /* back to before the last stop (the throws after it dropped);
                                                 * 0: none */
unsigned fto_winners(const tpos *p);            /* once over: every player but the loser */
int  fto_script(tgame *g, const char *moves);   /* plays throws and stops in the notation; 0: one is bad */

/* the bot (fourtwoone_bot.c, bots.h) */
const tgame *fourtwoone_live(void);     /* the game on the screen, read-only: the bot copies its position at entry */
/* main thread: plays a move as taps do (animated): "d-?-" keeps the dice marked "-" and throws the others (the
 * game's own faces, "?"), "s" stops; 0: not now, or not legal */
int  fourtwoone_show(const char *move);

#endif
