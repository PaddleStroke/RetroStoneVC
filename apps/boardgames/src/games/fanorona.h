/*
 * Fanorona (fanoron-tsivy, Madagascar) for two players on one device: the rules (paika moves, captures by approach
 * and by withdrawal, chains of captures, the ends of the game). No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 9 x 5 points, a1 .. i5 (fn_x: 0..8 from the left, fn_y: 0..4 from player 0's side; a point is
 * x + 9 y). The lines: every row and column, and the diagonals through the points whose x + y is even (the strong
 * points, the corners among them). A point holds -1 (empty), or the player whose piece is there.
 * The start: 22 pieces each. Player 0 (ivory, first to move) fills rows 1 and 2, player 1 (ebony) rows 4 and 5; the
 * middle row from the left: ebony, ivory, ebony, ivory, empty (e3, the centre), ebony, ivory, ebony, ivory.
 * A move: a piece steps along a line to the next point, if it is empty.
 * - Approach: the point just beyond, in the same direction, holds an enemy piece: that piece and every enemy piece
 *   right behind it on the line, up to the first empty point, own piece or the edge, are taken.
 * - Withdrawal: the point just behind the piece's start, in the opposite direction, holds an enemy piece: that piece
 *   and the unbroken enemy line behind it are taken.
 * - A step that could do both: the player chooses one (two moves here, FN_APPROACH and FN_WITHDRAW).
 * - Capturing is compulsory: when a player can capture at the start of a turn, they must (any capture); a step that
 *   takes nothing (paika, FN_PAIKA) is only allowed when no capture is.
 * - After a capture the same piece may capture again, in the same turn, one step at a time (each one a move here:
 *   `chain` is the piece in the middle of a turn), but never onto a point it has already stood on this turn (its
 *   start among them), and never twice running in the same direction. Going on is optional: FN_STOP ends the turn.
 *   The turn ends by itself when the piece has no further capture.
 * - No restriction on the first turn of the game (some sources allow the first player a single capture only).
 * The ends: a player who has no pieces left, or cannot move, loses. Draws: the same position (same player to move,
 * at the start of a turn) a third time; 50 moves each (FN_QUIET paika moves) without a capture.
 * Notation (script, the screenshots and the tests): "d2-e3" (or "d2e3") a move; a capture the same, with "a"
 * (approach) or "w" (withdrawal) after its end when the step could do both ("d3-e3w"); a chain "d3-e3w-e4"; each
 * word one turn: a chain that could go on stops at the space, unless it ends with "+" ("f2-e3+": the next word goes on
 * from e3); "stop" ends a chain. Separated by spaces: "d3-e3w d4-d3 c2-d2".
 */
#ifndef FANORONA_H
#define FANORONA_H

#include <stdint.h>

#define FN_COLS 9
#define FN_ROWS 5
#define FN_POINTS 45
#define FN_PIECES 22
#define FN_MAX_MOVES 360                        /* 22 pieces, 8 ways each, approach and withdrawal (fewer in fact) */
#define FN_MAX_TAKE 8                           /* the longest line a capture takes */
#define FN_QUIET 100                            /* moves without a capture: a draw */
#define FN_MAX_PLY 6000                         /* more than a game can last (repetitions end it long before) */

extern const int8_t fn_dx[8], fn_dy[8];         /* the 8 directions: N, NE, E, SE, S, SW, W, NW (y up) */

static inline int fn_x(int pt) { return pt % FN_COLS; }
static inline int fn_y(int pt) { return pt / FN_COLS; }
int  fn_next(int pt, int d);                    /* the next point along a line in direction d; -1 */
int  fn_point(const char *s);                   /* "e3": its point; -1 */

enum { FN_PAIKA, FN_APPROACH, FN_WITHDRAW, FN_STOP };
typedef struct { int8_t from, to, kind; } fnmove;   /* FN_STOP: from = to = the piece that stops */

typedef struct {
    int8_t at[FN_POINTS];
    uint8_t side;                               /* who moves */
    int8_t chain;                               /* the piece in the middle of a turn of captures; -1 */
    int8_t dir;                                 /* its last step's direction; -1 */
    uint8_t left[2];                            /* each player's pieces on the board */
    uint16_t quiet;                             /* moves since the last capture */
    uint64_t seen;                              /* the points the chain's piece has stood on this turn (bits) */
} fnpos;

void fn_start(fnpos *p);
int  fn_legal(const fnpos *p, fnmove *out);     /* the legal steps (only captures when there is one); their count */
int  fn_taken(const fnpos *p, fnmove m, int8_t *out);   /* the pieces a step takes, the nearest first; their count */
int  fn_dir(int from, int to);                  /* the direction of a step between neighbours; -1 */
void fn_make(fnpos *p, fnmove m);               /* plays a (legal) move, FN_STOP too (in a chain) */

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { FS_PLAY, FS_LOST, FS_REPETITION, FS_QUIET } fnstatus;
typedef struct {
    fnpos pos[FN_MAX_PLY + 1];
    fnmove move[FN_MAX_PLY];
    int ply;
} fngame;
void fn_new(fngame *g);
int  fn_play(fngame *g, fnmove m);              /* 0: the game is too long */
int  fn_undo(fngame *g);                        /* 0: nothing to undo */
/* FS_LOST: the player to move has no pieces or no move, and lost; FS_REPETITION, FS_QUIET: a draw */
fnstatus fn_status(const fngame *g);
int  fn_script(fngame *g, const char *moves);   /* plays moves in the notation above; 0: one is bad */
#define fn_pos(g) (&(g)->pos[(g)->ply])

#endif
