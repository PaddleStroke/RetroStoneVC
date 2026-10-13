/*
 * Banqi (Chinese dark chess) for two players on one device, Taiwanese rules: the 32 xiangqi pieces face down on half
 * a xiangqi board (4 x 8 squares), shuffled. No SDL; tested by tests/banqi_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A turn: turn a face-down piece over, or move one of your face-up pieces one square along a row or a column, to an
 * empty square or onto an enemy's face-up piece it may capture. The ranks, highest first: general, advisor, elephant,
 * chariot, horse, cannon, soldier; a piece captures one of its own rank or lower, but the soldier captures the general
 * (and soldiers) and the general cannot capture a soldier. The cannon captures by jumping exactly one piece (face up
 * or down, either colour) along a line, any distance, onto an enemy's face-up piece of any rank; it steps like the
 * others to an empty square. Face-down pieces cannot be captured.
 * Colours: the first piece turned over gives its colour to the player who turned it; the other player has the other.
 * Players are the colours here: 0 red, 1 black. Before the first flip the player to move counts as red; if they turn
 * a black piece they are black, and red moves next.
 * The end: a player whose sixteen pieces are all captured, or who has no move, loses; 50 turns in a row (both players'
 * counted) without a flip or a capture: a draw.
 * Squares: x + 8 * y, x the file a..h along the long side, y the rank 1..4. The notation: "c2" turns c2 over, "c2c3"
 * moves (or captures).
 */
#ifndef BANQI_H
#define BANQI_H

#include <stdint.h>

#define BQ_W 8
#define BQ_H 4
#define BQ_N (BQ_W * BQ_H)
#define BQ_SQ(x, y) ((y) * BQ_W + (x))
#define BQ_X(s) ((s) % BQ_W)
#define BQ_Y(s) ((s) / BQ_W)

/* the types, by rank (a lower number ranks higher) */
enum { BQ_EMPTY, BQ_GENERAL, BQ_ADVISOR, BQ_ELEPHANT, BQ_CHARIOT, BQ_HORSE, BQ_CANNON, BQ_SOLDIER };
#define BQ_HIDDEN 0x40                          /* a face-down piece */
#define BQ_PC(colour, type) ((uint8_t)((colour) << 3 | (type)))
#define BQ_TYPE(pc) ((pc) & 7)
#define BQ_COLOUR(pc) (((pc) >> 3) & 1)

typedef struct {
    uint8_t sq[BQ_N];                           /* 0 empty, else a piece (| BQ_HIDDEN: face down) */
    uint8_t side;                               /* the colour to move */
    uint8_t decided;                            /* 1: the first piece has been turned over */
    uint8_t quiet;                              /* turns since the last flip or capture */
    uint8_t taken[2];                           /* each colour's pieces captured */
} bqpos;

enum { BQ_FLIP, BQ_STEP, BQ_TAKE };
typedef struct { uint8_t kind, from, to, pc; } bqmove;   /* pc: the piece turned over, or the piece captured */
#define BQ_MAX_MOVES 96

#define BQ_QUIET 50                             /* turns without a flip or a capture: a draw */
#define BQ_MAX_PLY 4096                         /* 32 flips and 31 captures, at most 50 turns apart */

enum { BQ_PLAY, BQ_WIN0, BQ_WIN1, BQ_DRAW };

typedef struct {
    bqpos pos[BQ_MAX_PLY + 1];
    bqmove move[BQ_MAX_PLY];
    int ply;
} bqgame;

int  bq_beats(int attacker, int target);        /* 1: a piece of this type may capture that one by a step */
int  bq_legal(const bqpos *p, bqmove *out);     /* the legal moves of the side to move; their count */
void bq_make(bqpos *p, bqmove m);
void bq_deal(bqpos *p, uint32_t seed);          /* the 32 pieces face down, shuffled by the seed */
int  bq_square(const char *s);                  /* "c2" -> its square; -1: not one */

void bq_new(bqgame *g, uint32_t seed);
int  bq_play(bqgame *g, bqmove m);              /* 0: not legal, or the game is over */
int  bq_undo(bqgame *g);
int  bq_status(const bqgame *g);                /* BQ_* */
/* the moves in the notation ("c2 c2c3 d1"; "@7" first: the deal of seed 7): 0 at the first that is not legal */
int  bq_script(bqgame *g, const char *s);
#define bq_pos(g) (&(g)->pos[(g)->ply])

/* the bot (banqi_bot.c, bots.h): the game on the screen, read-only (the bot reads only what is face up, and the
 * pieces taken), and one move ("c2", "c2c3") played as if tapped (banqi.c; a headless test defines both) */
const bqgame *banqi_live(void);
int banqi_show(const char *move);

#endif
