/*
 * Farkle (« 10 000 »), the push-your-luck dice game, for one to six players on one device: the rules (the throws,
 * the scoring dice set aside, banking, farkles, hot dice, the entry, the final round). No SDL, no dice: the caller
 * throws and gives the rules the values.
 * - A turn: throw the six dice. Set aside one or more scoring dice from that throw (every die set aside must score),
 *   then either bank the turn's points or throw the dice left. A throw with no scoring dice is a farkle: the turn's
 *   points are lost and the turn ends. When all six have scored (hot dice), the six may be thrown again.
 * - The scoring (combinations count only within one throw): a 1: 100; a 5: 50; three of a kind: the face x 100
 *   (three 1s: 1000); four of a kind: 1000; five of a kind: 2000; six of a kind: 3000; a straight 1-2-3-4-5-6: 1500;
 *   three pairs (four of a kind and a pair count as three pairs): 1500; two triplets: 2500. The dice set aside score
 *   their best reading (four 1s: three 1s and a 1, 1100).
 * - The entry: a player's first bank must be at least 500 (FK_ENTRY) in one turn.
 * - The end: a player who banks to 10000 or more (FK_TARGET) starts the final round: every other player has one last
 *   turn; then the highest score wins (equal highest: shared).
 * - Alone: reach 10000 within 20 turns (FK_SOLO_TURNS): won at the bank that reaches it, lost after the 20th turn.
 * Notation (script, the screenshots and the tests): "d" and the faces thrown ("d115234"; then as many as the dice
 * left), "a" and the faces set aside ("a115"), then "d..." to throw the rest or "b" to bank: "d115234 a115 d225 a5 b".
 */
#ifndef FARKLE_H
#define FARKLE_H

#include <stdint.h>

#define FK_DICE 6
#define FK_PLAYERS 6
#define FK_TARGET 10000
#define FK_ENTRY 500
#define FK_SOLO_TURNS 20
#define FK_MAX_EVENTS 4096                      /* six cautious players reach about 1300 */

enum { FK_THROW, FK_CHOOSE, FK_OVER };          /* what comes next: a throw, dice to set aside, nothing */
enum { FK_THROWN = 1, FK_FARKLE };              /* a throw: dice to choose; no scoring dice (the turn lost) */

typedef struct {
    int32_t score[FK_PLAYERS];                  /* banked */
    int32_t turn_pts;                           /* this turn's points so far (the dice set aside) */
    uint8_t dice[FK_DICE], nd;                  /* the throw to choose from: nd dice */
    uint8_t aside[FK_DICE], group[FK_DICE], naside;   /* set aside this turn (since the last hot dice): faces,
                                                       * the throw each came from */
    uint8_t players, turn, phase;
    int8_t final_by;                            /* who reached the target first (-1: not yet) */
    uint8_t final_left;                         /* bit p: player p still has their last turn */
    int8_t winner;                              /* -1: none (playing, shared, alone) */
    uint8_t farkles;                            /* the last turn ended by a farkle (1), or by a bank (0) */
    uint16_t turns;                             /* turns over since the start */
    uint16_t throws;                            /* throws in this turn */
} fpos;

enum { FE_THROW, FE_BANK };
typedef struct { uint8_t kind, sel, nd, v[FK_DICE]; } fevent;

/* A game: the position before every event, so undo is free. */
typedef struct {
    fpos pos, past[FK_MAX_EVENTS];
    fevent ev[FK_MAX_EVENTS];
    int n;
    unsigned picked;                            /* the script: the dice set aside ("a..."), for the next throw or
                                                 * bank */
} fgame;

void fk_new(fgame *g, int players);
/* the points of these dice taken together (all of them must score); 0: some die does not score (or none) */
int  fk_value(const uint8_t *dice, int n);
int  fk_sel_value(const fpos *p, unsigned sel);  /* the dice chosen (bit i: die i of the throw); 0: not valid */
unsigned fk_scoring(const fpos *p);             /* the dice of the throw that can score (in some set) */
int  fk_can_bank(const fpos *p, unsigned sel);  /* 1: setting these aside, the turn may be banked */
int  fk_throw(fgame *g, unsigned sel, const int *values);   /* sets sel aside (0 at a turn's start) and throws the
                                                             * rest (all six: a new turn, hot dice): the faces, as
                                                             * many as fk_to_throw; FK_*; 0: not allowed */
int  fk_to_throw(const fpos *p, unsigned sel);  /* how many dice a throw now would throw */
int  fk_bank(fgame *g, unsigned sel);           /* 0: not allowed; 1: banked; 2: banked, the game over */
int  fk_leader(const fpos *p);                  /* the highest score's player (the first of them) */
int  fk_undo(fgame *g);                         /* back to before the last bank, while no dice set aside to throw
                                                 * again came after it (undo never throws again); 0: none */
int  fk_script(fgame *g, const char *moves);    /* plays throws, dice set aside and banks; 0: one is bad */

/* the bot (farkle_bot.c, bots.h) */
const fgame *farkle_live(void);         /* the game on the screen, read-only: the bot copies its position at entry */
/* main thread: plays a move as taps do (animated): "d??????" throws (the game's own faces, "?"), "a115 d???" sets
 * those dice aside and throws the rest, "a5 b" sets aside and banks; 0: not now, or not legal */
int  farkle_show(const char *move);

#endif
