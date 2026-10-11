/*
 * Gomoku's rules for RetroStone Board (no SDL): 15 x 15 points, the stones never move, five in a row wins (across,
 * down or diagonal). Freestyle: five or more; standard: exactly five (six or more in a row do not win). A full board
 * with no winner is a draw. Tested by tests/gomoku_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ebony (plays first), 1 ivory.
 * Points: x + y * 15, x from the left, y from the bottom, named by a letter a..o and a number 1..15 from the bottom:
 * "h8 i9 ...".
 */
#ifndef GOMOKU_H
#define GOMOKU_H

#include <stdint.h>

#define GK_N 15
#define GK_PTS (GK_N * GK_N)

enum { GK_EMPTY, GK_EBONY, GK_IVORY };          /* a point: empty, or player p's stone (p + 1) */
enum { GK_FREESTYLE, GK_STANDARD };             /* the variants */
enum { GK_PLAY, GK_WIN0, GK_WIN1, GK_DRAW };

typedef struct {
    int variant, nmoves;
    uint8_t pt[GK_PTS];
    int move[GK_PTS];                           /* the points played, in order */
} gkgame;

void gk_new(gkgame *g, int variant);
int gk_side(const gkgame *g);                   /* the player to move */
int gk_status(const gkgame *g);                 /* GK_PLAY, GK_WIN0, GK_WIN1, GK_DRAW */
int gk_play(gkgame *g, int p);                  /* 0: not allowed (taken, off the board, the game over) */
int gk_undo(gkgame *g);                         /* 0: nothing to undo */
int gk_last(const gkgame *g);                   /* the last point played, or -1 */
/* the winning stones (the line or lines the last stone made): line[p] set; their count, 0 if none */
int gk_line(const gkgame *g, uint8_t line[GK_PTS]);
int gk_point(const char *s, int *len);          /* "h8": the point, else -1; len: its length */
int gk_script(gkgame *g, const char *s);        /* plays "h8 i9 ..."; 0: a bad or illegal move */

#endif
