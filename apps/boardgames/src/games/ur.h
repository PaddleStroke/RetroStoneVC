/*
 * The Royal Game of Ur for two players on one device: the rules (Irving Finkel's reconstruction, the one played
 * today). No SDL, no dice: the caller throws and gives the rules the value.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 20 squares, 3 rows of 8 with a gap: each player's own row has 4 squares at one end and 2 at the other,
 * the middle row (8 squares) is shared. A piece's place is counted along its player's path: 0 off the board, waiting;
 * 1..4 its own 4 squares (towards the board's end), 5..12 the shared middle row (the other way), 13..14 its own 2
 * squares; 15 off the board, borne off. The rosettes: 4 and 14 (each player's own) and 8 (the middle of the shared
 * row, the central rosette).
 * The rules:
 * - Throw four tetrahedral dice, each with two of its four tips marked: the marked tips pointing up count (0..4).
 * - Move one piece that many squares along its path: a waiting piece enters (a throw of 2 puts it on square 2).
 * - A piece may not land on its own player's piece. Landing on the other player's piece in the shared row sends it
 *   back off the board, except on the central rosette, where a piece is safe (and so it cannot be landed on).
 * - Landing on a rosette gives another throw.
 * - A piece leaves the board with the exact number only (from 14: a 1; from 12: a 3).
 * - A throw of 0, or a throw no piece can use, is lost: the turn passes.
 * - The first to bear off all seven pieces wins.
 * Notation (script, the screenshots and the tests): a digit 0..4 is a throw; a letter moves the piece on that place
 * of the player to move: a (0) a waiting piece, b..o the places 1..14: "2 a 3 c 0 4 a".
 */
#ifndef UR_H
#define UR_H

#include <stdint.h>

#define UR_PIECES 7
#define UR_OFF 15                               /* borne off */
#define UR_SHARED_LO 5                          /* the shared row: 5..12 */
#define UR_SHARED_HI 12
#define UR_CENTRE 8                             /* the central rosette (safe) */
#define UR_MAX_EVENTS 4096                      /* events kept for undo (a power of 2): longer, the first go */
#define UR_MOVE_EV 16                           /* an event: 0..4 a throw, UR_MOVE_EV + place a move */

enum { UR_THROW, UR_MOVE, UR_OVER };            /* what comes next: a throw, a piece to move, nothing */
enum { UT_MOVE = 1, UT_PASS };                  /* a throw: choose a piece; no move (0, or nothing can go) */

typedef struct {
    uint8_t n[2][UR_OFF + 1];                   /* each player's pieces on each place: 0 waiting, 1..14 (0 or 1),
                                                 * 15 borne off */
    uint8_t turn, phase, roll;                  /* the player to move, UR_THROW / MOVE / OVER, the last throw */
    int8_t winner;                              /* -1 */
    uint16_t throws;                            /* throws since the start */
} upos;

/* A game: the position before every event, so undo is free. */
typedef struct {
    upos pos, past[UR_MAX_EVENTS];
    uint8_t ev[UR_MAX_EVENTS];
    int n;
} ugame;

void ur_new(ugame *g);
int  ur_rosette(int place);                     /* 4, 8, 14 */
int  ur_shared(int place);                      /* 5..12 */
/* the square of a player's place 1..14: col 0..7 from the 4-square end, row 0 player 1's, 1 shared, 2 player 0's;
 * 0: not on the board */
int  ur_cell(int player, int place, int *col, int *row);
int  ur_dest(const upos *p, int from);          /* where the player to move's piece on `from` goes; -1: nowhere */
int  ur_takes(const upos *p, int from);         /* 1: that move sends a piece of the other player back */
int  ur_movable(const upos *p);                 /* the places whose piece can move (bit i: place i) */
int  ur_throw(ugame *g, int value);             /* UT_*; 0: not now */
int  ur_move(ugame *g, int from);               /* 0: not allowed; else 1 + the pieces it sent back */
int  ur_undo(ugame *g);                         /* back to the last move's choice (with the throws after it) */
int  ur_script(ugame *g, const char *moves);    /* plays throws and moves in the notation above; 0: one is bad */

/* the bot (ur_bot.c, bots.h) */
const ugame *ur_live(void);             /* the game on the screen, read-only: the bot copies its position at entry */
/* main thread: plays a move as a tap does (animated): "roll" throws the dice (the game's own), a letter moves that
 * piece; 0: not now, or not legal */
int  ur_show(const char *move);

#endif
