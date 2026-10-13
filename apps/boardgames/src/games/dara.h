/*
 * Dara, the Nigerian game of lines (the Hausa and Zarma of Niger and Nigeria), for two players on one device: a board
 * of 5 rows of 6 holes, 12 pieces each. No SDL; tested by tests/dara_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Placing: the players take turns setting one piece in any empty hole, but never so that three (or more) of their
 * pieces stand in a row along a row or a column. A player who cannot place passes; once neither can, the pieces
 * left in hand are out of the game.
 * Moving: then each turn moves one piece one hole along a row or a column to an empty hole. A move that makes exactly
 * three of the mover's pieces in a row (a row of four does not count) takes one enemy piece, any one; two rows made
 * at once still take one.
 * The end: a player left with two pieces, or with no move on their turn, loses; 100 moves in a row (both players'
 * counted, after the placing) without a capture: a draw.
 * Players: 0 ivory (places and moves first), 1 ebony. Holes: x + 6 * y, x the column a..f from player 0's left, y the
 * row 1..5 from their side. The notation: "c3" places (or, after a row of three, takes the piece on c3; "xc3" too),
 * "c3c4" moves.
 */
#ifndef DARA_H
#define DARA_H

#include <stdint.h>

#define DAR_W 6
#define DAR_H 5
#define DAR_N (DAR_W * DAR_H)
#define DAR_AT(x, y) ((y) * DAR_W + (x))
#define DAR_PIECES 12
#define DAR_QUIET 100                           /* moves without a capture: a draw */
#define DAR_MAX_PLY 3000
#define DAR_MAX_MOVES 64

enum { DAR_PLACE, DAR_MOVE, DAR_TAKE };         /* the phase; the moves' kinds */
enum { DAR_PLAY, DAR_WIN0, DAR_WIN1, DAR_DRAW };

typedef struct {
    uint8_t c[DAR_N];                           /* 0 empty, else player + 1 */
    uint8_t side, phase;
    uint8_t hand[2];                            /* pieces still to place */
    uint8_t quiet;                              /* moves since the last capture (moving) */
    uint32_t line;                              /* the row of three just made (bit k: hole k), while it takes */
} darpos;

typedef struct { uint8_t kind, from, to; } darmove;   /* a placement and a capture: to only */

typedef struct {
    darpos pos[DAR_MAX_PLY + 1];
    darmove move[DAR_MAX_PLY];
    int ply;
} dargame;

int  dar_can_place(const darpos *p, int player, int hole);   /* empty, and no three in a row */
uint32_t dar_line(const darpos *p, int hole);   /* the rows of exactly three through the piece on hole (bits) */
int  dar_legal(const darpos *p, darmove *out);  /* the moves of the side to move; their count */
void dar_make(darpos *p, darmove m);
int  dar_count(const darpos *p, int player);    /* on the board */
int  dar_hole(const char *s);                   /* "c3" -> its hole; -1: not one */

void dar_new(dargame *g);
int  dar_play(dargame *g, darmove m);           /* 0: not legal, or the game is over */
int  dar_undo(dargame *g);
int  dar_status(const dargame *g);              /* DAR_* */
int  dar_script(dargame *g, const char *s);     /* "c3 d3 c3c4 xd3": 0 at the first that is not legal */
#define dar_pos(g) (&(g)->pos[(g)->ply])

/* the bot (dara_bot.c, bots.h), and what the screen (dara.c) gives it; a headless test defines those two */
const dargame *dara_live(void);                 /* the game on the screen, read-only: the bot copies its position */
int  dara_show(const char *move);               /* main thread: one action ("c3", "c3c4", "xd3"), animated; 0: not
                                                 * legal */
struct ai_stats;
/* the bot's action for the side to move at a level (AI_EASY..AI_HARD); 0: none */
int  drb_choose(const darpos *p, int level, uint64_t seed, darmove *m, struct ai_stats *stats);

#endif
