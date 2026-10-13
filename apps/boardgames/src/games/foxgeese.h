/*
 * Fox and geese for two players on one device: the rules (steps, jumps, chains of jumps, the ends of the game).
 * No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: the cross of 33 points on a 7 x 7 grid (fg_x, fg_y: 0..6, a1 at 0, 0; a point is on the cross when its
 * column or its row is 2..4), numbered rank by rank from c1: c1 d1 e1 c2 d2 e2 a3 .. g3 a4 .. g4 a5 .. g5 c6 d6 e6
 * c7 d7 e7. The lines: every row and column, and the diagonals through the points whose x + y is even (the usual
 * pattern: an X in every 2 x 2 block of the cross). A point holds -1 (empty), FG_GOOSE or FG_FOX.
 * The rules: 13 geese (player 0, ivory) on c1..e2 and the whole third row; the fox (player 1, ebony) in the middle,
 * d4. The geese move first. A goose steps along a line to the next point, forwards (towards rank 7) or sideways,
 * never backwards; it never jumps. The fox steps along a line in any direction, or jumps a goose next to it along a
 * line to the empty point just beyond, taking it. Jumping is not compulsory, but a fox that has jumped goes on
 * jumping while it can, a jump at a time (each jump is one move here: `chain` says the fox is in the middle of one).
 * The geese win when the fox cannot move; the fox wins when fewer than 9 geese are left (it took 5), or when the
 * geese cannot move. Draws: the same position a third time; 50 moves each without a capture or a goose going forwards.
 * Notation (script, the screenshots and the tests): "c3-c4" (or "c3c4") a step; a jump the same, "d4-d2" (or
 * "d4xd2"); a chain of jumps "d4xd2xb2" (each jump one move); separated by spaces: "c3-c4 d4-c5 c4-b4".
 */
#ifndef FOXGEESE_H
#define FOXGEESE_H

#include <stdint.h>

#define FG_POINTS 33
#define FG_GEESE 13
#define FG_FOX_WINS 9                           /* fewer geese than this: the fox won */
#define FG_MAX_MOVES 72                         /* 13 geese, 5 ways each at most */
#define FG_QUIET 100                            /* moves without a capture or a goose going forwards: a draw */
#define FG_MAX_PLY 8800                         /* more than a game can last (83 forward moves and captures at most) */

enum { FG_GOOSE, FG_FOX };

extern const int8_t fg_x[FG_POINTS], fg_y[FG_POINTS];
extern const int8_t fg_dx[8], fg_dy[8];         /* the 8 directions: N, NE, E, SE, S, SW, W, NW (y up) */
extern int8_t fg_next[FG_POINTS][8];            /* the next point along a line in a direction; -1 (fg_init) */

typedef struct { int8_t from, to, over; } fgmove;       /* over: the goose a jump takes; -1 for a step */

typedef struct {
    int8_t at[FG_POINTS];
    uint8_t side;                               /* who moves */
    uint8_t chain;                              /* 1: the fox has just jumped and jumps again */
    int8_t fox;                                 /* the fox's point */
    uint8_t geese;                              /* geese left */
    uint16_t quiet;                             /* moves since the last capture or forward move of a goose */
} fgpos;

void fg_init(void);                             /* the lines (called by fg_new) */
int  fg_point(const char *s);                   /* "d4": its point; -1 */
void fg_start(fgpos *p);
int  fg_legal(const fgpos *p, fgmove *out);     /* the legal moves; their count */
void fg_make(fgpos *p, fgmove m);               /* plays a (legal) move */

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { FS_PLAY, FS_TRAPPED, FS_FEW, FS_STUCK, FS_REPETITION, FS_QUIET } fgstatus;
typedef struct {
    fgpos pos[FG_MAX_PLY + 1];
    fgmove move[FG_MAX_PLY];
    int ply;
} fggame;
void fg_new(fggame *g);
int  fg_play(fggame *g, fgmove m);              /* 0: the game is too long */
int  fg_undo(fggame *g);                        /* 0: nothing to undo */
/* FS_TRAPPED: the fox cannot move (the geese won); FS_FEW: fewer than FG_FOX_WINS geese, FS_STUCK: the geese cannot
 * move (the fox won); FS_REPETITION, FS_QUIET: a draw */
fgstatus fg_status(const fggame *g);
int  fg_script(fggame *g, const char *moves);   /* plays moves in the notation above; 0: one is bad */
#define fg_pos(g) (&(g)->pos[(g)->ply])

/* the bot (foxgeese_bot.c, bots.h: either side), and what the screen (foxgeese.c) gives it; a headless test defines
 * those two */
const fggame *foxgeese_live(void);              /* the game on the screen, read-only: the bot copies it at entry */
int foxgeese_show(const char *move);            /* main thread: plays a turn ("d4xd2xb2") as taps do, each jump
                                                 * animated; 0: not legal */
struct ai_stats;
/* the side to move's whole turn at a level (its moves, encoded, in out; their count; g read at once: the position
 * and the ones before it that may come again) and its notation */
int fgb_turn(const fggame *g, int level, uint64_t seed, int *out, int max, struct ai_stats *stats);
void fgb_write(const int *moves, int n, char *out, int cap);

#endif
