/*
 * Pig, the push-your-luck dice game, for 2 to 6 players on one device: the rules. No SDL, no dice: the caller throws
 * and gives the rules the faces.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * - Pig (PIG_ONE): on your turn, roll one die as many times as you like, each roll adding to the turn's total. A 1
 *   loses the turn's total and ends the turn. Hold instead to bank the total in your score (after one roll at least).
 * - Two-dice pig (PIG_TWO): the same with two dice, their sum added. A single 1 loses the turn's total and ends the
 *   turn; two 1s lose the whole score as well (back to 0).
 * - The first player to hold with 100 or more (PIG_TARGET) wins at once.
 * - Undo takes back a hold, and only the last move (never a roll: the dice would fall again).
 * Notation (script; the screenshots and the tests): "r4" a roll (one die; two dice: "r35"), "h" a hold.
 */
#ifndef PIG_H
#define PIG_H

#include <stdint.h>

#define PIG_MAXP 6
#define PIG_TARGET 100
#define PIG_SEQ 24

enum { PIG_ONE, PIG_TWO };                      /* the variants */
enum { PIG_PLAY, PIG_OVER };
enum { PIG_HELD = 1, PIG_LOST, PIG_WIPED };     /* how a turn ended: held, a 1 (the turn's total lost), two 1s */

typedef struct {
    int16_t score[PIG_MAXP];
    int16_t total;                              /* the turn's total so far */
    uint8_t players, turn, phase, variant;
    uint8_t dice[2], ndice;                     /* the last roll this turn (ndice 0: none yet) */
    uint16_t rolls;                             /* rolls this turn */
    uint8_t seq[PIG_SEQ][2];                    /* this turn's rolls (the last PIG_SEQ of them) */
    uint16_t turns;                             /* turns over since the start */
    /* the last turn's end: how, whose, the points lost (a 1) or banked (a hold), the score lost (two 1s), its dice */
    uint8_t ended, ended_by, end_dice[2];
    int16_t ended_pts, ended_score;
    int8_t winner;                              /* -1: none yet */
} pigpos;

typedef struct {
    pigpos pos, before;                         /* before: the position before the last move (a hold: undo) */
    int last_hold;                              /* 1: the last move was a hold (it can be taken back) */
} piggame;

void pig_new(piggame *g, int variant, int players);
int  pig_ndice(const pigpos *p);                /* 1 or 2 */
int  pig_roll(piggame *g, int d1, int d2);      /* a roll (d2: the second die, two-dice pig); 0: not now */
int  pig_can_hold(const pigpos *p);
int  pig_hold(piggame *g);                      /* 0: not now */
int  pig_undo(piggame *g);                      /* takes back the last hold; 0: none */
int  pig_leader(const pigpos *p);               /* the highest score's player (the first of them) */
int  pig_script(piggame *g, const char *moves); /* 0: a move is bad */

#endif
