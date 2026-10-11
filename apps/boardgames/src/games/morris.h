/*
 * The men's morris games for two players on one device: the rules (placing, moving, flying, mills, removals, draws).
 * Nine men's morris, twelve men's morris (the same board and its four corner diagonals), six men's morris (two squares).
 * No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The boards (morris_board): points on a grid (x, y from a1 at 0, 0: 7 x 7, six men's 5 x 5), numbered rank by rank
 * from a1. Nine and twelve men's: a1 d1 g1 b2 d2 f2 c3 d3 e3 a4 b4 c4 e4 f4 g4 c5 d5 e5 b6 d6 f6 a7 d7 g7; six men's:
 * a1 c1 e1 b2 c2 d2 a3 b3 d3 e3 b4 c4 d4 a5 c5 e5. The board's lines join the points next to each other: a line of
 * three is a mill; six men's also has four lines of two (between its squares). A point holds -1 (empty) or its player
 * (0 ivory, 1 ebony).
 * The rules: 9 pieces each (twelve men's 12, six men's 6), player 0 first. Placing, then moving along the lines
 * (flying, to any empty point, with exactly 3 left: not in six men's). A mill (3 in a line) removes one opponent piece
 * that is not in a mill, unless all of them are (then any); two mills at once still remove one. A player loses with
 * fewer than 3 pieces, or with no move. Draws: the board full while placing (twelve men's: 24 pieces, no mill); the
 * same position a third time; 50 moves each without a mill in the moving phase.
 * Notation (script, the screenshots and the tests): "d2" places a piece, "d2-d3" (or "d2d3") moves one, "xb6" removes
 * one after a mill (also written on the end: "d2xb6", "d2-d3xb6"); separated by spaces: "a1 d6 d1 f4 g1 xd6".
 */
#ifndef MORRIS_H
#define MORRIS_H

#include <stdint.h>

#define MORRIS_POINTS 24                        /* the most points (nine and twelve men's) */
#define MORRIS_LINES 20                         /* the most lines (twelve men's: 16 and 4 diagonals) */
#define MORRIS_MAX_MOVES 64
#define MORRIS_MAX_PLY 2048                     /* more than a game can last (the 50-move rule, 22 removals at most) */
#define MORRIS_QUIET 100                        /* moves without a mill: a draw (50 each) */

enum { MORRIS_NINE, MORRIS_TWELVE, MORRIS_SIX, MORRIS_VARIANTS };

typedef struct {
    int cells;                                  /* the grid: cells x cells */
    int points, lines, pieces;                  /* pieces: each player's */
    int fly;                                    /* 1: a player down to three pieces flies */
    int8_t x[MORRIS_POINTS], y[MORRIS_POINTS];
    int8_t line[MORRIS_LINES][3];               /* the points along each line; a line of two: -1 third */
} mboard;

const mboard *morris_board(int variant);

enum { M_PLACE, M_MOVE, M_TAKE };
typedef struct { int8_t type, from, to; } mmove;        /* place: to; move: from, to; take: from (the piece removed) */

typedef struct {
    int8_t at[MORRIS_POINTS];
    uint8_t variant;
    uint8_t side;                               /* who plays (or removes a piece) */
    uint8_t take;                               /* 1: the side just made a mill and removes an opponent piece */
    uint8_t hand[2];                            /* pieces still to place */
    uint8_t on[2];                              /* pieces on the board */
    uint16_t quiet;                             /* moves since the last mill */
} mpos;

void morris_start(mpos *p, int variant);
int  morris_legal(const mpos *p, mmove *out);   /* the legal actions (placings, moves or removals); their count */
void morris_make(mpos *p, mmove m);             /* plays a (legal) action */
int  morris_in_mill(const mpos *p, int point);  /* the piece on the point is in a mill */
int  morris_flying(const mpos *p, int player);  /* the player has 3 pieces left, all placed, and may fly */
int  morris_point(int variant, const char *s);  /* "d2": its point on the variant's board; -1 */

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { MS_PLAY, MS_FEW, MS_BLOCKED, MS_REPETITION, MS_QUIET, MS_FULL } mstatus;
typedef struct {
    mpos pos[MORRIS_MAX_PLY + 1];
    mmove move[MORRIS_MAX_PLY];
    int ply;
} mgame;
void morris_new(mgame *g, int variant);
int  morris_play(mgame *g, mmove m);            /* 0: the game is too long */
int  morris_undo(mgame *g);                     /* 0: nothing to undo */
mstatus morris_status(const mgame *g);          /* MS_FEW, MS_BLOCKED: the side to move lost; the rest draws */
int  morris_script(mgame *g, const char *moves);   /* plays moves in the notation above; 0: one is bad */
#define morris_pos(g) (&(g)->pos[(g)->ply])

#endif
