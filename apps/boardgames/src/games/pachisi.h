/*
 * Pachisi on one device: two players, each with two armies in partnership (the traditional two-player game), or four,
 * an army each, partners sitting opposite (the traditional game): the rules. No SDL, no shells: the caller throws the
 * six cowries and gives the rules the number of mouths up.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: a cross of four arms, each 3 columns of 8 squares, round the charkoni (the centre). Arms, anticlockwise:
 * 0 the bottom, 1 the right, 2 the top, 3 the left; army a starts in the charkoni next to arm a. Player 0 has armies 0
 * and 2 (opposite arms), player 1 armies 1 and 3; four players: player p has army p, players 0 and 2 against 1 and
 * 3 (a side: player & 1), the turn going round the table anticlockwise (0 1 2 3, as the pieces go).
 * A piece's place is counted along its army's path: 0 in the charkoni, not yet out; 1..7 down its arm's middle column;
 * 8 the end of that column (on the track round the board); 8..75 anticlockwise round the board, along the outer
 * columns of every arm and across the arms' ends (the track: 68 squares); 76 its own arm's end
 * again, 77..83 up the middle column; 84 home, back in the charkoni.
 * The throw: six cowries, the mouths up give 0: 25, 1: 10, 2..6: 2..6. A 6, 10 or 25 is a grace: another throw.
 * The rules (as played in India, after the Masters of Games' and the common modern accounts):
 * - Move one piece of either of your armies by the throw, or decline to move.
 * - Each army's first piece leaves the charkoni with any throw; every other piece, and a piece sent back, only with a
 *   grace. A piece leaving the charkoni goes the full throw down its column.
 * - Pieces of one player share squares freely; nothing blocks (a piece passes any others).
 * - Landing on the other player's pieces on an ordinary square sends them all back to the charkoni, and gives another
 *   throw. The 12 castles (the arms' ends, and on each outer column the fourth square from its end) are safe: no
 *   piece may land on a castle where the other player's pieces stand.
 * - A piece reaches home with the exact throw only. No throw is a 1, so a piece may not stop one square short of
 *   home (it could never come in).
 * - The first side with all eight pieces home wins (four players: the partner who brings the last one home). With
 *   four, a player whose own army is home plays the partner's army with their throws.
 * Notation (script, the screenshots and the tests): a number is a throw (25, 10, 2..6), a letter moves a piece of
 * the player to move (a..d its first army's pieces, e..h its second's; four players: a..d the army it plays), '-'
 * declines: "10 a 25 e 3 - 6 b".
 */
#ifndef PACHISI_H
#define PACHISI_H

#include <stdint.h>

#define PA_ARMIES 4
#define PA_PIECES 4                             /* an army's */
#define PA_LAP 68                               /* the track round the board */
#define PA_END 8                                /* the place on its arm's end, where an army's path meets the track */
#define PA_UP 76                                /* the arm's end again, on the way home */
#define PA_HOME 84
#define PA_SQUARES (PA_LAP + PA_ARMIES * 7)     /* the track, then each arm's 7 middle squares (the charkoni: -1) */
#define PA_MAX_EVENTS 8192                      /* throws and moves: far more than a game lasts */
#define PA_DECLINE_EV 30                        /* an event: 0..6 a throw (the mouths up), PA_DECLINE_EV,
                                                 * PA_MOVE_EV + piece a move (a piece: 0..7 of the mover's) */
#define PA_MOVE_EV 32

enum { PA_THROW, PA_MOVE, PA_OVER };            /* what comes next: a throw, a piece to move, nothing */
enum { PT_MOVE = 1, PT_AGAIN, PT_PASS };        /* a throw: choose a piece; no move but a grace (throw again); no
                                                 * move (the turn passes) */

typedef struct {
    uint8_t at[PA_ARMIES][PA_PIECES];           /* each piece's place on its army's path */
    uint8_t turn, phase, roll;                  /* the player to move, PA_THROW / MOVE / OVER, the throw's value */
    uint8_t players;                            /* 2 or 4 */
    uint8_t out;                                /* the armies whose first piece has left the charkoni (bit a) */
    int8_t winner;                              /* -1 */
    uint16_t throws;                            /* throws since the start */
} ppos;

/* A game: the position before every event, so undo is free. */
typedef struct {
    ppos pos, past[PA_MAX_EVENTS];
    uint8_t ev[PA_MAX_EVENTS];
    int n;
} pgame;

void pa_new(pgame *g, int players);           /* 2 or 4 players */
int  pa_value(int mouths);                      /* 25, 10, 2..6 */
int  pa_mouths(int value);                      /* the other way; -1: no such throw */
int  pa_grace(int value);                       /* 6, 10, 25: another throw */
int  pa_army(const ppos *p, int piece);         /* the army of the mover's piece 0..7 (-1: none) */
int  pa_army_home(const ppos *p, int army);     /* an army's pieces home */
int  pa_square(int army, int place);            /* the square (0..PA_SQUARES-1) of an army's place; -1: the charkoni */
int  pa_castle(int square);
/* a square's cell on the 19 x 19 board (x from the left, y from the top; arm 0 at the bottom); the charkoni: 8..10 */
void pa_cell(int square, int *x, int *y);
int  pa_dest(const ppos *p, int piece);         /* where the player to move's piece goes with the throw; -1: nowhere */
int  pa_takes(const ppos *p, int piece);        /* how many pieces that move sends back */
int  pa_movable(const ppos *p);                 /* the pieces that can move (bit i: piece i) */
int  pa_throw(pgame *g, int mouths);            /* PT_*; 0: not now */
int  pa_move(pgame *g, int piece);              /* 0: not allowed; else 1 + the pieces it sent back */
int  pa_decline(pgame *g);                      /* the throw not used; 0: not now */
int  pa_home(const ppos *p, int side);          /* a side's pieces home (side 0: armies 0 and 2, 1: 1 and 3) */
int  pa_undo(pgame *g);                         /* back to the last move's choice (with the throws after it) */
int  pa_script(pgame *g, const char *moves);    /* plays throws and moves in the notation above; 0: one is bad */

#endif
