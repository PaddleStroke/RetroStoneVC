/*
 * Sea Battle (Bataille navale), for two on one device: the rules. No SDL; the random fleets come from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A 10 x 10 sea each. The fleet: five ships of 5, 4, 3, 3 and 2 squares (the common modern fleet); or the paper
 * fleet, ten ships of 4, 3, 3, 2, 2, 2, 1, 1, 1, 1 that may not touch, not even at a corner. Each player places their
 * fleet in secret (player 0 first), in lines across or down, inside the sea, never two on one square. Then, in turn
 * (player 0 first), a player fires one shot at the other's sea, or, in the salvo games, as many shots as they have
 * ships still afloat (never more than the squares not yet shot). Each shot is a miss or a hit; a ship whose squares
 * are all hit is sunk, and shown. The first to sink the whole fleet wins.
 * Notation (script; the screenshots and the tests), words separated by spaces: r a new random fleet for the player
 * placing, <ship>=<square><h|v> a ship placed (its first square, the top or left end: 0=a1h), ok the fleet ready, then
 * the shots: a square (e5), a salvo's squares joined by + (a1+b2+c3), * one turn of shots at random (placing: ok),
 * # the rest of the game so. Squares: the column a..j from the left, the row 1..10 from the top.
 */
#ifndef SEABATTLE_H
#define SEABATTLE_H

#include <stdint.h>

#define SB_N 10
#define SB_CELLS (SB_N * SB_N)
#define SB_MAXSHIPS 10
#define SB_HIST 256

enum { SB_CLASSIC, SB_SALVO, SB_PAPER, SB_PAPER_SALVO, SB_VARIANTS };
enum { SP_PLACE, SP_SHOOT, SP_OVER };
enum { SHOT_NONE, SHOT_MISS, SHOT_HIT };

typedef struct { int8_t x, y; uint8_t len, vert; } sbship;     /* its first square (top or left), across or down */

typedef struct {
    uint8_t variant, nships, salvo, apart;      /* apart: ships may not touch (the paper fleet) */
    uint8_t len[SB_MAXSHIPS];
    uint8_t phase, turn, winner;
    sbship ship[2][SB_MAXSHIPS];                /* each player's fleet */
    uint8_t shot[2][SB_CELLS];                  /* the shots fired at each player's sea (SHOT_*) */
    uint8_t last[2][SB_MAXSHIPS], nlast[2];     /* the last shots fired at each player's sea */
    uint32_t rng;                               /* the random fleets */
} sbpos;

typedef struct {
    sbpos pos;
    sbpos hist[SB_HIST];                        /* the positions before each action since the last hand-off */
    int n;                                      /* (undo never goes past the hand-off) */
} sbgame;

void sb_new(sbgame *g, int variant, uint32_t seed);
/* a fleet's squares: 1 if it lies inside the sea, no two ships on a square (and none touching: apart) */
int  sb_valid(const sbpos *p, const sbship *fleet);
/* the ship of the fleet on a square (-1: none) */
int  sb_ship_at(const sbpos *p, const sbship *fleet, int cell);
int  sb_cells(const sbship *s, int *cells);     /* its squares; how many */
void sb_random(const sbpos *p, uint32_t *rng, sbship *fleet);   /* a valid fleet at random */
int  sb_sunk(const sbpos *p, int player, int ship);   /* the player's ship has all its squares hit */
int  sb_afloat(const sbpos *p, int player);     /* the player's ships not sunk */
int  sb_shots(const sbpos *p);                  /* how many shots the player to move fires this turn */

/* the actions (each one undoable back to the last hand-off) */
int  sb_set(sbgame *g, const sbship *fleet);    /* SP_PLACE: the fleet of the player placing, moved; 0: not valid */
int  sb_reroll(sbgame *g);                      /* SP_PLACE: a new random fleet */
int  sb_ready(sbgame *g);                       /* SP_PLACE: the fleet placed: the hand-off */
int  sb_fire(sbgame *g, const int *cells, int n);   /* SP_SHOOT: n == sb_shots(), squares not yet shot */
int  sb_undo(sbgame *g);
int  sb_script(sbgame *g, const char *moves);   /* 0: a word is bad */

#endif
