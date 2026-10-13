/*
 * Bagh-Chal (Nepal's tigers and goats) for two players on one device: the rules (placing the goats, the steps, the
 * tigers' jumps, the ends of the game). No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 5 x 5 points, a1 .. e5 (bc_x, bc_y: 0..4, a1 at 0, 0, from the goats' side; a point is x + 5 y). The
 * lines: every row and column, and the diagonals through the points whose x + y is even (alquerque's board). A point
 * holds -1 (empty), BC_GOAT or BC_TIGER.
 * The rules: the goats (player 0, ivory) move first; the tigers (player 1, ebony) start on the four corners. The
 * goats come in one at a time: while some of the 20 are still to come, the goats' move is to set one down on any
 * empty point; only when all 20 are in may a goat move. A goat steps along a line to the next point, if it is empty,
 * in any direction; it never jumps and never captures. A tiger steps the same way, or jumps a goat next to it along a
 * line to the empty point just beyond, taking it (one goat a move, never a chain; never over a tiger).
 * The end: the tigers win when they have taken 5 goats (the common rule), or when the goats cannot move; the goats
 * win when no tiger can move (trapped). A draw: the same position (the same player to move) the third time, or 50
 * moves each without a capture once every goat is in (setting a goat down counts as progress).
 * Notation (script, the screenshots and the tests): "c3" sets a goat down on c3; "a1-a2" (or "a1a2") a step; a jump
 * the same, "a1-c3" (or "a1xc3"); separated by spaces: "b1 a1-b2 c2".
 */
#ifndef BAGHCHAL_H
#define BAGHCHAL_H

#include <stdint.h>

#define BC_SIDE 5
#define BC_POINTS 25
#define BC_GOATS 20                             /* goats to set down */
#define BC_TIGERS 4
#define BC_TIGERS_WIN 5                         /* goats taken: the tigers won */
#define BC_MAX_MOVES 64                         /* 21 empty points; 4 tigers x 8 ways; goats: 6 empty points x 8 */
#define BC_QUIET 100                            /* moves (both players) without a capture or a goat set down: a draw */
#define BC_MAX_PLY 700                          /* more than a game can last: 20 goats set down, 5 captures, 100
                                                 * quiet moves after each */

enum { BC_GOAT, BC_TIGER };

extern const int8_t bc_dx[8], bc_dy[8];         /* the 8 directions: N, NE, E, SE, S, SW, W, NW (y up) */

static inline int bc_x(int pt) { return pt % BC_SIDE; }
static inline int bc_y(int pt) { return pt / BC_SIDE; }
int  bc_next(int pt, int d);                    /* the next point along a line in direction d; -1 */
int  bc_point(const char *s);                   /* "c3": its point; -1 */

typedef struct { int8_t from, to, over; } bcmove;       /* from -1: a goat set down on `to`; over: the goat a jump
                                                         * takes, -1 for a step */

typedef struct {
    int8_t at[BC_POINTS];
    uint8_t side;                               /* who moves */
    uint8_t placed;                             /* goats set down so far (0..20) */
    uint8_t taken;                              /* goats the tigers took */
    uint16_t quiet;                             /* moves since the last capture or goat set down */
} bcpos;

void bc_start(bcpos *p);
int  bc_legal(const bcpos *p, bcmove *out);     /* the legal moves; their count */
void bc_make(bcpos *p, bcmove m);               /* plays a (legal) move */
static inline int bc_to_place(const bcpos *p) { return BC_GOATS - p->placed; }

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { BS_PLAY, BS_TRAPPED, BS_FIVE, BS_STUCK, BS_REPETITION, BS_QUIET } bcstatus;
typedef struct {
    bcpos pos[BC_MAX_PLY + 1];
    bcmove move[BC_MAX_PLY];
    int ply;
} bcgame;
void bc_new(bcgame *g);
int  bc_play(bcgame *g, bcmove m);              /* 0: the game is too long */
int  bc_undo(bcgame *g);                        /* 0: nothing to undo */
/* BS_TRAPPED: no tiger can move (the goats won); BS_FIVE: 5 goats taken, BS_STUCK: the goats cannot move (the tigers
 * won); BS_REPETITION, BS_QUIET: a draw */
bcstatus bc_status(const bcgame *g);
int  bc_script(bcgame *g, const char *moves);   /* plays moves in the notation above; 0: one is bad */
#define bc_pos(g) (&(g)->pos[(g)->ply])

/* the bot (baghchal_bot.c, bots.h: either side), and what the screen (baghchal.c) gives it; a headless test defines
 * those two */
const bcgame *baghchal_live(void);              /* the game on the screen, read-only: the bot copies it at entry */
int baghchal_show(const char *move);            /* main thread: plays "c3", "a1-a2" as taps do (animated); 0: not
                                                 * legal */
struct ai_stats;
/* the move in the game g (read at once: the position and the ones before it that may come again), encoded; -1 */
int bcb_choose(const bcgame *g, int level, uint64_t seed, struct ai_stats *stats);
void bcb_write(int move, char *out, int cap);   /* an encoded move in the notation */

#endif
