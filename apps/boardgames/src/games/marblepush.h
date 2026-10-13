/*
 * Marble Push's rules for RetroStone Board (no SDL): a hexagon of 61 holes (5 a side), 14 marbles each; a move takes
 * 1, 2 or 3 of the player's marbles in a row one hole in one of the six directions, along their row (in line: it may
 * push the opponent's marbles) or sideways (broadside: every marble to an empty hole); the first to push 6 of the
 * opponent's marbles off the board wins. Tested by tests/marblepush_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ebony (moves first, the traditional dark side) at the bottom, 1 ivory at the top.
 * Holes: 0..60, row by row from the bottom, left to right in each row. The rows a..i (from player 0's side) hold
 * 5, 6, 7, 8, 9, 8, 7, 6, 5 holes; the hole's diagonal 1..9 from the left: row a holds a1..a5, row e e1..e9, row i
 * i5..i9 (a hole is its row and diagonal: "e5" the middle). The directions, seen from player 0's side: e, ne, nw, w,
 * sw, se (a row: e and w; ne: one row up and one diagonal on, nw: one row up, the same diagonal).
 *
 * A move in line pushes when the marbles straight ahead are the opponent's and fewer than the mover's (2 push 1,
 * 3 push 1 or 2) and the hole beyond them is empty or off the board (the last one pushed then leaves the board). A
 * player's own marble, or no gap, among or right after the pushed ones blocks the push; a marble cannot be moved off
 * the board by its own player; there are no sideways pushes.
 *
 * Starting layouts: 0 the standard one (rows a and b full, c3 c4 c5; the other side the same turned half round),
 * 1 the Belgian daisy (two clusters of 7 each, a hole and its 6 neighbours: player 0 round b2 and h8, player 1 round
 * b5 and h5), 2 the German daisy (player 0 round c2 and g8, player 1 round c6 and g4).
 *
 * Draws (no rule says when a game of this kind must end, so the house's): the same position a third time with the same
 * player to move; 50 moves each (100 moves) without a marble pushed off; the player to move having no move at all.
 *
 * Notation (script, the screenshots and the tests): the marbles moved, one to three holes (only the two ends
 * are needed: "c3c5" is c3 c4 c5), then ':' and the direction: "a1:ne", "b2b4:nw", "c3c5:e". Moves separated by
 * spaces.
 */
#ifndef MARBLEPUSH_H
#define MARBLEPUSH_H

#include <stdint.h>

#define MP_HOLES 61
#define MP_ROWS 9
#define MP_MARBLES 14
#define MP_WIN 6                                /* marbles pushed off to win */
#define MP_QUIET 100                            /* moves without a marble pushed off: a draw */
#define MP_MAX_MOVES 600                        /* 14 singles x 6, 2 x 42 lines x 6 */
#define MP_MAX_PLY 1250                         /* more than a game can last (11 push-offs, < 100 moves between) */
#define MP_VARIANTS 3

enum { MP_E, MP_NE, MP_NW, MP_W, MP_SW, MP_SE };   /* the directions; the opposite of d: (d + 3) % 6 */
enum { MP_STANDARD, MP_BELGIAN, MP_GERMAN };

typedef struct {
    int8_t cell[3];                             /* the marbles moved: in line, from the back to the front; sideways,
                                                 * along their row */
    int8_t n, dir;
    int8_t push;                                /* the opponent's marbles pushed (0..2) */
    int8_t off;                                 /* 1: the last of them leaves the board */
} mpmove;

typedef struct {
    uint8_t cell[MP_HOLES];                     /* 0 empty, else the player + 1 */
    uint8_t side;                               /* who moves */
    uint8_t lost[2];                            /* each player's marbles pushed off */
    uint16_t quiet;                             /* moves since a marble was last pushed off */
} mppos;

/* the board */
int mp_row(int h);                              /* 0..8 (a..i) */
int mp_diag(int h);                             /* 0..8 (1..9) */
int mp_at(int row, int diag);                   /* the hole there, or -1 */
int mp_next(int h, int dir);                    /* the next hole that way, or -1 (off the board) */
/* the holes from a to b in a straight line (at most 3: a, b two apart at most), from a; their number, 0: none */
int mp_line(int a, int b, int8_t cells[3]);

/* a position */
void mp_start(mppos *p, int variant);
int mp_legal(const mppos *p, mpmove *out);      /* the legal moves, their number */
/* the legal moves of exactly these marbles (any order), their number */
int mp_moves_of(const mppos *p, const int8_t *cells, int n, mpmove *out);
void mp_make(mppos *p, const mpmove *m);        /* plays a legal move */
int mp_same(const mpmove *a, const mpmove *b);  /* the same marbles the same way */
/* a move's destination for each marble moved (in line: also the opponent's pushed; -1: off the board); count */
int mp_moving(const mpmove *m, int8_t from[5], int8_t to[5]);

/* a game: every position since the start (undo, repetitions) */
enum { MP_PLAY, MP_WIN0, MP_WIN1, MP_DRAW_REPEAT, MP_DRAW_QUIET, MP_DRAW_STUCK };
typedef struct {
    mppos pos[MP_MAX_PLY + 1];
    mpmove move[MP_MAX_PLY];
    int ply, variant;
} mpgame;

void mp_new(mpgame *g, int variant);
int mp_play(mpgame *g, const mpmove *m);        /* 0: not allowed (the game is over, or the move illegal) */
int mp_undo(mpgame *g);                         /* 0: nothing to undo */
int mp_status(const mpgame *g);                 /* MP_PLAY, MP_WIN0 ... */
#define mp_pos(g) (&(g)->pos[(g)->ply])

/* the notation */
int mp_hole(const char *s, int *len);           /* "e5": the hole (len: its length), else -1 */
void mp_name(int h, char out[3]);
int mp_script(mpgame *g, const char *s);        /* plays "c3c5:ne a1:e ..."; 0: a bad or illegal move */

/* the bot (marblepush_bot.c, bots.h) */
const mpgame *marblepush_live(void);            /* the game on the screen, read-only: the bot copies it at entry */
int marblepush_show(const char *move);          /* main thread: plays "c3c5:ne" as taps do (animated); 0: not legal */

#endif
