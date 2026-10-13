/*
 * Alquerque for two players on one device: the rules (steps, captures, chains of captures, the ends of the game).
 * No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 5 x 5 points, a1 .. e5 (aq_x, aq_y: 0..4, a1 at 0, 0; a point is x + 5 y). The lines: every row and
 * column, and the diagonals through the points whose x + y is even (the usual pattern: an X in every 2 x 2 block).
 * A point holds -1 (empty), or the player whose piece is there.
 * The rules: 12 pieces each. Player 0 (ivory, first to move) fills rows 1 and 2 and d3, e3; player 1 (ebony) rows 4
 * and 5 and a3, b3 (each the right half of the middle row, from their side); c3 is empty. A piece steps along a line
 * to the next point, if empty, forwards (straight or slanting) or sideways, never backwards. A piece captures by
 * jumping an enemy piece next to it along a line, in any direction (backwards too), to the empty point just beyond;
 * after a capture the same piece jumps on while it can, a jump at a time (each jump is one move here: `chain` is the
 * point of the piece in the middle of one). Capturing is compulsory: when a player can capture, they must (any
 * capture, not the longest), and a chain goes on to its end. No huffing (no piece is taken for a missed capture).
 * A player who cannot move, or has no pieces, loses. Draws: the same position a third time; 50 moves each (100
 * moves) without a capture.
 * Notation (script, the screenshots and the tests): "c2-c3" (or "c2c3") a step; a capture the same, "b2-d4" (or
 * "b2xd4"); a chain "b2xd4xb4" (each jump one move); separated by spaces: "d3-c3 b4xd2 e1xc3".
 */
#ifndef ALQUERQUE_H
#define ALQUERQUE_H

#include <stdint.h>

#define AQ_SIDE 5
#define AQ_POINTS 25
#define AQ_PIECES 12
#define AQ_MAX_MOVES 96                         /* 12 pieces, 8 ways each at most */
#define AQ_QUIET 100                            /* moves without a capture: a draw */
#define AQ_MAX_PLY 2600                         /* more than a game can last (23 captures, 100 quiet moves between) */

extern const int8_t aq_dx[8], aq_dy[8];         /* the 8 directions: N, NE, E, SE, S, SW, W, NW (y up) */

static inline int aq_x(int pt) { return pt % AQ_SIDE; }
static inline int aq_y(int pt) { return pt / AQ_SIDE; }
int  aq_next(int pt, int d);                    /* the next point along a line in direction d; -1 */
int  aq_point(const char *s);                   /* "c3": its point; -1 */

typedef struct { int8_t from, to, over; } aqmove;       /* over: the piece a capture takes; -1 for a step */

typedef struct {
    int8_t at[AQ_POINTS];
    uint8_t side;                               /* who moves */
    int8_t chain;                               /* the piece that has just captured and captures again; -1 */
    uint8_t left[2];                            /* each player's pieces on the board */
    uint16_t quiet;                             /* moves since the last capture */
} aqpos;

void aq_start(aqpos *p);
int  aq_legal(const aqpos *p, aqmove *out);     /* the legal moves (only captures when there is one); their count */
void aq_make(aqpos *p, aqmove m);               /* plays a (legal) move */

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { AS_PLAY, AS_STUCK, AS_REPETITION, AS_QUIET } aqstatus;
typedef struct {
    aqpos pos[AQ_MAX_PLY + 1];
    aqmove move[AQ_MAX_PLY];
    int ply;
} aqgame;
void aq_new(aqgame *g);
int  aq_play(aqgame *g, aqmove m);              /* 0: the game is too long */
int  aq_undo(aqgame *g);                        /* 0: nothing to undo */
/* AS_STUCK: the player to move cannot move (or has no pieces left) and lost; AS_REPETITION, AS_QUIET: a draw */
aqstatus aq_status(const aqgame *g);
int  aq_script(aqgame *g, const char *moves);   /* plays moves in the notation above; 0: one is bad */
#define aq_pos(g) (&(g)->pos[(g)->ply])

/* the bot (alquerque_bot.c, bots.h), and what the screen (alquerque.c) gives it; a headless test defines those two */
const aqgame *alquerque_live(void);             /* the game on the screen, read-only: the bot copies it at entry */
int alquerque_show(const char *move);           /* main thread: plays a turn ("b2xd4xb4") as taps do, each capture
                                                 * animated; 0: not legal */
struct ai_stats;
/* the side to move's whole turn in g at a level (its moves, encoded, in out; their count; g read at once: the
 * position and the ones before it that may come again) and its notation */
int aqb_turn(const aqgame *g, int level, uint64_t seed, int *out, int max, struct ai_stats *stats);
void aqb_write(const int *moves, int n, char *out, int cap);

#endif
