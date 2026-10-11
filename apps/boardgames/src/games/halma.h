/*
 * Chinese checkers' rules for RetroStone Board (no SDL): the six-pointed star of 121 holes, two players in opposite
 * points with 10 marbles each, a step to an adjacent hole or a chain of jumps over single marbles, no captures; the
 * first to fill the opposite point wins. Tested by tests/halma_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Holes: 0..120, row by row from the bottom tip, left to right in each row. The rows (17) hold 1, 2, 3, 4, 13, 12, 11,
 * 10, 9, 10, 11, 12, 13, 4, 3, 2, 1 holes. A hole's place: its row (0 at the bottom) and x2, its column on a grid of
 * half spacings (-12..12, 0 in the middle): its six neighbours are x2 +- 2 in its row and x2 +- 1 in the rows above and
 * below. Player 0 (ivory, moves first) starts in the bottom point (rows 0..3) and goes to the top one (rows 13..16),
 * player 1 (ebony) the other way; the four side points start empty.
 *
 * The anti-blocking rule: a player wins as soon as the opposite point is full and at least one of its marbles is
 * theirs (the opponent's marbles left there count for them): a marble kept at home cannot block the opponent. Should
 * the player to move have no move at all (all their marbles walled in), the game is drawn.
 *
 * Notation (script): a hole is a letter for its place in its row from the left (a, b, ...) and its row from the
 * bottom (1..17): "a1" the bottom tip, "a5".."m5" the first long row, "a17" the top tip. A move: its holes joined by
 * '-': "c4-c5" (a step), "d4-d6" (a jump), "a4-c6-e6" (a chain, each hop a jump); two holes alone: the step or the
 * shortest chain of jumps between them. Moves are separated by spaces.
 */
#ifndef HALMA_H
#define HALMA_H

#include <stdint.h>

#define HL_HOLES 121
#define HL_ROWS 17
#define HL_MARBLES 10
#define HL_HIST 1024                            /* the moves kept for undo (beyond: the oldest are forgotten) */

enum { HL_EMPTY, HL_IVORY, HL_EBONY };          /* a hole: empty, or player p's marble (p + 1) */
enum { HL_PLAY = -1, HL_WIN0, HL_WIN1, HL_DRAW };

typedef struct {
    int n;                                      /* the holes on the way: the start, each landing, the end */
    uint8_t hole[HL_HOLES];
} hl_path;

typedef struct {
    uint8_t cell[HL_HOLES];
    int ply;                                    /* the moves played (the turn: ply & 1) */
    int nhist;                                  /* the last moves, for undo */
    hl_path hist[HL_HIST];
} hl_game;

/* the board */
int hl_row(int h);                              /* 0..16 from the bottom */
int hl_x2(int h);                               /* -12..12 */
int hl_at(int x2, int row);                     /* the hole there, or -1 */
int hl_around(int h, int nb[6]);                /* its neighbours (2 to 6) */
int hl_home(int h);                             /* the player starting there (0 bottom point, 1 top point), or -1 */
int hl_goal(int player, int h);                 /* 1: h is in the point this player must fill */

/* the game */
void hl_new(hl_game *g);
int hl_turn(const hl_game *g);
/* every hole the marble on `from` may reach this turn (a step or a chain of jumps); their number */
int hl_targets(const hl_game *g, int from, int *to);
/* the way there (a step, or the shortest chain of jumps); 0: none */
int hl_route(const hl_game *g, int from, int to, hl_path *p);
int hl_valid(const hl_game *g, const hl_path *p);   /* 1: the player to move may play this exact way */
int hl_play(hl_game *g, const hl_path *p);      /* 0: not allowed */
int hl_undo(hl_game *g);                        /* 0: nothing to undo */
const hl_path *hl_last(const hl_game *g);       /* the last move, or NULL */
int hl_status(const hl_game *g);                /* HL_PLAY, HL_WIN0, HL_WIN1, HL_DRAW */
int hl_arrived(const hl_game *g, int player);   /* the player's marbles in the point they must fill */

/* the notation */
int hl_hole(const char *s, int *len);           /* "e5": the hole (len: its length), else -1 */
void hl_name(int h, char out[4]);
int hl_script(hl_game *g, const char *s);       /* plays "c4-c5 ..."; 0: a bad or illegal move */

#endif
