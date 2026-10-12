/*
 * Ship, Captain and Crew, the old tavern dice game, for 2 to 6 players on one device: the rules. No SDL, no dice: the
 * caller throws and gives the rules the faces.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * - A turn: up to three rolls of five dice. You need a 6 (the ship), then a 5 (the captain), then a 4 (the crew), in
 *   that order: a 5 counts only once the ship is in, a 4 only once the captain is (the same roll may bring two or all
 *   three). They are set aside as they come; the other dice are rolled again.
 * - With ship, captain and crew in, the two dice left are the cargo: their sum is the turn's score. With rolls left
 *   you may stop there, or roll both cargo dice again (the last roll counts). Without them after three rolls: no
 *   cargo, nothing scored.
 * - A round: each player plays one turn, from the round's first player (the next player each round). The highest
 *   cargo of the round wins a chip; equal highest cargoes each win one; no cargo at all: no chip.
 * - The first to 5 chips (SCC_TARGET) at the end of a round wins; two reaching it together share the win.
 * - Undo takes back a stop, and only the last move (never a roll: the dice would fall again).
 * Notation (script; the screenshots and the tests): "r" and the faces of the dice rolled, in their places, those set
 * aside left out ("r62153", then "r415"...), "s" a stop.
 */
#ifndef SHIPCAPTAIN_H
#define SHIPCAPTAIN_H

#include <stdint.h>

#define SCC_MAXP 6
#define SCC_DICE 5
#define SCC_ROLLS 3
#define SCC_TARGET 5

enum { SCC_PLAY, SCC_OVER };
enum { SCC_SHIP, SCC_CAPTAIN, SCC_CREW };       /* what is set aside: the 6, the 5, the 4 */

typedef struct {
    uint8_t players, turn, phase, starter;
    uint8_t chips[SCC_MAXP];
    uint8_t dice[SCC_DICE];                     /* the faces (0: not rolled yet this turn) */
    int8_t aside[3];                            /* the die set aside as ship, captain, crew (-1: not yet) */
    uint8_t rolls;                              /* this turn's rolls so far */
    uint8_t done;                               /* bit p: player p has played this round */
    int8_t res[SCC_MAXP];                       /* this round's cargoes (-1: not played yet; 0: none) */
    /* the last round settled: the cargoes, the players who won a chip */
    int8_t last_res[SCC_MAXP];
    uint8_t last_won;
    uint16_t rounds;
    uint8_t winners;                            /* once over: bit p */
    /* the last turn ended: whose, its dice and those set aside, its cargo */
    uint8_t end_by, end_dice[SCC_DICE];
    int8_t end_aside[3], end_cargo;
} sccpos;

typedef struct {
    sccpos pos, before;                         /* before: the position before the last move (a stop: undo) */
    int last_stop;
} sccgame;

void scc_new(sccgame *g, int players);
int  scc_complete(const sccpos *p);             /* 1: ship, captain and crew are in */
int  scc_cargo(const sccpos *p);                /* the cargo's sum (0: not complete) */
unsigned scc_free(const sccpos *p);             /* the dice a roll throws now (bit i: die i) */
int  scc_roll(sccgame *g, const int *faces);    /* faces[i] for each die i the roll throws; 0: not now */
int  scc_can_stop(const sccpos *p);
int  scc_stop(sccgame *g);
int  scc_undo(sccgame *g);                      /* takes back the last stop; 0: none */
int  scc_script(sccgame *g, const char *moves); /* 0: a move is bad */

#endif
