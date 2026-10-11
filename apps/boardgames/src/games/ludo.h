/*
 * Ludo for two players on one device: the rules (the track, the yards, the home columns, the captures, the safe
 * squares, the throws). No SDL, no dice: the caller throws and gives the rules the value.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The track: 52 squares, numbered clockwise from player 0's start square (0); player 1 starts opposite, at 26. A
 * token's place is counted from its own start square: -1 in its yard, 0..50 on the track, 51..55 its home column,
 * 56 home (the centre): 56 steps from the start square to the centre.
 * The rules (the common, simple ones):
 * - Only a 6 brings a token out of its yard, onto its start square (a 6 may also move a token on the board).
 * - A 6 gives another throw; the third 6 in a row is lost and the turn ends.
 * - Landing on the other player's tokens sends them back to their yard, except on a safe square: the four start
 *   squares and the four stars (8 squares after each start), where tokens of both players may stand together.
 * - Two tokens of one player on one square do not block: anyone may pass them or land there (a capture takes both).
 * - After a full lap a token turns into its own home column and reaches the centre with the exact number only.
 * - A throw with no legal move is lost (after a 6, the player still throws again).
 * - The first to bring all four tokens home wins.
 * Notation (script, the screenshots and the tests): a digit is a throw, a letter a..d moves that token of the player
 * to move; a throw with no move needs nothing more: "6 a 4 a 3 6 b 2 b".
 */
#ifndef LUDO_H
#define LUDO_H

#include <stdint.h>

#define LUDO_TOKENS 4
#define LUDO_TRACK 52
#define LUDO_COLUMN 51                          /* the first square of the home column */
#define LUDO_HOME 56                            /* the centre */
#define LUDO_STAR 8                             /* a star 8 squares after each start square */
#define LUDO_MAX_EVENTS 4096                    /* throws and moves: far more than a game lasts */
#define LUDO_MOVE_EV 16                         /* an event: 1..6 a throw, LUDO_MOVE_EV + token a move */

enum { LUDO_THROW, LUDO_MOVE, LUDO_OVER };      /* what comes next: a throw, a token to move, nothing */
enum { LT_MOVE = 1, LT_AGAIN, LT_PASS, LT_SIXES };  /* a throw: choose a token; no move but a 6 (throw again);
                                                 * no move (the turn passes); the third 6 (the turn passes) */

typedef struct {
    int8_t at[2][LUDO_TOKENS];                  /* each token's place, from its own start square (-1: the yard) */
    uint8_t turn, phase, die, sixes;            /* the player to move, LUDO_THROW / MOVE / OVER, the last throw,
                                                 * the 6s thrown in a row this turn */
    int8_t winner;                              /* -1 */
    uint16_t throws;                            /* throws since the start */
} lpos;

/* A game: the position before every event, so undo is free. */
typedef struct {
    lpos pos, past[LUDO_MAX_EVENTS];
    uint8_t ev[LUDO_MAX_EVENTS];
    int n;
} lgame;

void ludo_new(lgame *g);
int  ludo_square(int player, int at);           /* the track square (0..51) of a token at `at`; -1 off the track */
int  ludo_safe(int square);                     /* a start square or a star */
int  ludo_dest(const lpos *p, int token);       /* where the player to move's token goes with the die; -1: nowhere */
int  ludo_movable(const lpos *p);               /* the tokens that can move (bit i: token i) */
int  ludo_throw(lgame *g, int value);           /* LT_*; 0: not now */
int  ludo_move(lgame *g, int token);            /* 0: not allowed; else 1 + the tokens it sent home */
int  ludo_undo(lgame *g);                       /* back to the last move's choice (with the throws after it); 0: none */
int  ludo_home(const lpos *p, int player);      /* the player's tokens home */
int  ludo_script(lgame *g, const char *moves);  /* plays throws and moves in the notation above; 0: one is bad */

#endif
