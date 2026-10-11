/*
 * Chinese checkers' rules for RetroStone Board (no SDL): the six-pointed star of 121 holes, two, three, four or six
 * players in its points with 10 marbles each, a step to an adjacent hole or a chain of jumps over single marbles, no
 * captures; the first to fill the opposite point wins. Tested by tests/halma_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Holes: 0..120, row by row from the bottom tip, left to right in each row. The rows (17) hold 1, 2, 3, 4, 13, 12, 11,
 * 10, 9, 10, 11, 12, 13, 4, 3, 2, 1 holes. A hole's place: its row (0 at the bottom) and x2, its column on a grid of
 * half spacings (-12..12, 0 in the middle): its six neighbours are x2 +- 2 in its row and x2 +- 1 in the rows above and
 * below. The star's points, clockwise from the bottom: 0 the bottom one (rows 0..3), 1 lower left, 2 upper left, 3 the
 * top one (rows 13..16), 4 upper right, 5 lower right; point k's opposite is k + 3 (mod 6).
 *
 * The players: 0 (ivory, moves first) always starts in the bottom point; the others follow clockwise round the star,
 * and so does the turn. Each goes to the point opposite their own.
 * - 2 players: points 0 and 3 (ivory bottom, ebony top); the four side points start empty.
 * - 3 players: every other point, 0, 2 and 4; each heads for an empty point.
 * - 4 players: two pairs of opposite points, 0, 1, 3 and 4 (the two others empty).
 * - 6 players: all six.
 *
 * The anti-blocking rule: a player wins as soon as the point they must fill is full and at least one of its marbles is
 * theirs (marbles of others left there count for them): a marble kept at home cannot block anyone. The first to do it
 * wins and the game ends. Two players: should the player to move have no move at all (all their marbles walled in),
 * the game is drawn. Three or more: a player with no move passes, and the game is drawn only when nobody can move.
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
#define HL_POINTS 6
#define HL_PLAYERS 6
#define HL_HIST 1024                            /* the moves kept for undo (beyond: the oldest are forgotten) */

enum { HL_EMPTY, HL_IVORY, HL_EBONY };          /* a hole: empty, or player p's marble (p + 1) */
enum { HL_PLAY = -1, HL_WIN0, HL_WIN1, HL_DRAW, HL_WIN2, HL_WIN3, HL_WIN4, HL_WIN5 };
static inline int hl_win(int player) { return player < 2 ? HL_WIN0 + player : HL_WIN2 + player - 2; }
static inline int hl_winner(int status)         /* the winner, or -1 (playing, a draw) */
{
    return status == HL_WIN0 || status == HL_WIN1 ? status - HL_WIN0 : status >= HL_WIN2 ? status - HL_WIN2 + 2 : -1;
}

typedef struct {
    int n;                                      /* the holes on the way: the start, each landing, the end */
    uint8_t hole[HL_HOLES];
} hl_path;

typedef struct {
    uint8_t cell[HL_HOLES];
    int players;                                /* 2, 3, 4 or 6 */
    int turn;                                   /* the player to move */
    int ply;                                    /* the moves played */
    int nhist;                                  /* the last moves, for undo */
    hl_path hist[HL_HIST];
    uint8_t who[HL_HIST];                       /* the player who played each */
} hl_game;

/* the board */
int hl_row(int h);                              /* 0..16 from the bottom */
int hl_x2(int h);                               /* -12..12 */
int hl_at(int x2, int row);                     /* the hole there, or -1 */
int hl_around(int h, int nb[6]);                /* its neighbours (2 to 6) */
int hl_point(int h);                            /* the star's point it is in (0..5), or -1: the middle */
int hl_home(int h);                             /* two players: the one starting there (0 bottom, 1 top), or -1 */
int hl_goal(int player, int h);                 /* two players: 1: h is in the point this player must fill */

/* the game */
void hl_new(hl_game *g);                        /* two players */
void hl_new_players(hl_game *g, int players);   /* 2, 3, 4 or 6 (else 2) */
int hl_seat(int players, int player);           /* the point the player starts in */
int hl_dest(const hl_game *g, int player, int h);   /* 1: h is in the point this player must fill */
int hl_turn(const hl_game *g);
/* every hole the marble on `from` may reach this turn (a step or a chain of jumps); their number */
int hl_targets(const hl_game *g, int from, int *to);
/* the way there (a step, or the shortest chain of jumps); 0: none */
int hl_route(const hl_game *g, int from, int to, hl_path *p);
int hl_valid(const hl_game *g, const hl_path *p);   /* 1: the player to move may play this exact way */
int hl_play(hl_game *g, const hl_path *p);      /* 0: not allowed */
int hl_undo(hl_game *g);                        /* 0: nothing to undo */
const hl_path *hl_last(const hl_game *g);       /* the last move, or NULL */
int hl_status(const hl_game *g);                /* HL_PLAY, HL_DRAW, or hl_win(the winner) */
int hl_arrived(const hl_game *g, int player);   /* the player's marbles in the point they must fill */

/* the notation */
int hl_hole(const char *s, int *len);           /* "e5": the hole (len: its length), else -1 */
void hl_name(int h, char out[4]);
int hl_script(hl_game *g, const char *s);       /* plays "c4-c5 ..."; 0: a bad or illegal move */

#endif
