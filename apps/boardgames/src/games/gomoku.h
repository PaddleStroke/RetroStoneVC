/*
 * Gomoku's rules for RetroStone Board (no SDL): the stones never move, a row of them wins (across, down or diagonal).
 * Freestyle (15 x 15): five or more in a row; standard (15 x 15): exactly five (six or more in a row do not win);
 * six in a row (19 x 19): ebony first places one stone, then each player places two stones a turn, six or more in a
 * row win. A full board with no winner is a draw. Tested by tests/gomoku_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ebony (plays first), 1 ivory.
 * Points: x + y * n (n: the board's side), x from the left, y from the bottom, named by a letter a.. (the i too) and
 * a number 1.. from the bottom: "h8 i9 ...". Each stone is a move (six in a row: a turn is two of them).
 */
#ifndef GOMOKU_H
#define GOMOKU_H

#include <stdint.h>

#define GK_N 15                                 /* the gomoku board (freestyle, standard) */
#define GK_PTS (GK_N * GK_N)
#define GK_MAX 19                               /* the largest board (six in a row) */
#define GK_MAXPTS (GK_MAX * GK_MAX)

enum { GK_EMPTY, GK_EBONY, GK_IVORY };          /* a point: empty, or player p's stone (p + 1) */
enum { GK_FREESTYLE, GK_STANDARD, GK_SIX, GK_VARIANTS };   /* the variants */
enum { GK_PLAY, GK_WIN0, GK_WIN1, GK_DRAW };

typedef struct {
    int variant, n, nmoves;                     /* n: the board's side */
    uint8_t pt[GK_MAXPTS];
    int move[GK_MAXPTS];                        /* the points played, in order */
} gkgame;

int gk_size(int variant);                       /* the board's side: 15, or 19 */
void gk_new(gkgame *g, int variant);
int gk_side(const gkgame *g);                   /* the player to move */
int gk_left(const gkgame *g);                   /* the stones the player to move still places this turn (1 or 2) */
int gk_turn_start(const gkgame *g, int k);      /* the first stone (move index) of the turn stone k belongs to */
int gk_status(const gkgame *g);                 /* GK_PLAY, GK_WIN0, GK_WIN1, GK_DRAW */
int gk_play(gkgame *g, int p);                  /* 0: not allowed (taken, off the board, the game over) */
int gk_undo(gkgame *g);                         /* one stone back; 0: nothing to undo */
int gk_last(const gkgame *g);                   /* the last point played, or -1 */
/* the winning stones (the line or lines the last stone made): line[p] set (n * n of them); their count, 0 if none */
int gk_line(const gkgame *g, uint8_t line[GK_MAXPTS]);
int gk_point(int n, const char *s, int *len);   /* "h8" on an n x n board: the point, else -1; len: its length */
int gk_script(gkgame *g, const char *s);        /* plays "h8 i9 ..."; 0: a bad or illegal move */

#endif
