/*
 * Rithmomachia, the medieval "battle of numbers", for two players on one device: a board of 8 x 16 squares, 24 pieces
 * a side (8 rounds, 8 triangles, 7 squares and a pyramid), each carrying a number. No SDL; tested by
 * tests/rithmomachia_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The layout is the classic one of Claude de Boissiere (1556) and Lever and Fulke (1563). The evens (player 0, ivory)
 * have the rounds 2 4 6 8 and 4 16 36 64, the triangles 6 20 42 72 and 9 25 49 81, the squares 15 45 153 and 25 81
 * 169 289, and the pyramid 91 (1 + 4 + 9 + 16 + 25 + 36); the odds (player 1, ebony) the rounds 3 5 7 9 and 9 25 49 81,
 * the triangles 12 30 56 90 and 16 36 64 100, the squares 28 66 120 and 49 121 225 361, and the pyramid 190
 * (16 + 25 + 36 + 49 + 64). The evens move first.
 * Moves (into an empty square, never over a piece): a round one square diagonally; a triangle exactly two squares
 * along a row or a column; a square exactly three; the pyramid as any of them.
 * Captures: after a move, the mover takes every enemy piece that one of these holds against (the capturers stay):
 * - meeting: one of their pieces of the same number could move onto it;
 * - assault: a smaller piece of theirs, along one of its own lines of moving with only empty squares between, times
 *   the number of those empty squares, makes its number;
 * - ambush: two of their pieces that could both move onto it add up to its number;
 * - siege: each of its four neighbours along the rows and columns holds one of their pieces (the edge of the board
 *   closes a side too).
 * A pyramid counts as one piece of its whole number (attacking and attacked).
 * The end: the common victory takes 8 or 12 pieces (rhm_goal, by variant); the proper victory sets three pieces in
 * the enemy's half (the 8 ranks beyond the middle), on one line (a row, a column or a diagonal), equally spaced,
 * their numbers in order along the line in an arithmetic (4 6 8), geometric (4 16 64) or harmonic (3 4 6)
 * progression. A player with no move loses; 100 moves in a row (both players') without a capture: a draw.
 * Squares: x + 8 * y, x the file a..h from player 0's left, y the rank 1..16 from their side. The notation: a move is
 * its two squares, "c4d5", "b3b5" (the rank in one or two digits).
 */
#ifndef RITHMOMACHIA_H
#define RITHMOMACHIA_H

#include <stdint.h>

#define RHM_W 8
#define RHM_H 16
#define RHM_N (RHM_W * RHM_H)
#define RHM_SQ(x, y) ((y) * RHM_W + (x))
#define RHM_PIECES 48                           /* 24 a side: ids 0..23 the evens, 24..47 the odds */
#define RHM_QUIET 100
#define RHM_MAX_PLY 4800                        /* 46 captures at most, 100 moves apart */
#define RHM_MAX_MOVES 320

enum { RHM_ROUND = 1, RHM_TRIANGLE, RHM_SQUARE, RHM_PYRAMID };
enum { RHM_MEETING = 1, RHM_ASSAULT, RHM_AMBUSH, RHM_SIEGE };   /* how a piece is taken */
enum { RHM_COMMON8, RHM_COMMON12, RHM_PROPER, RHM_VARIANTS };     /* take 8, take 12, a harmony */
enum { RHM_PLAY, RHM_WIN0, RHM_WIN1, RHM_DRAW };

typedef struct { uint8_t shape, owner; uint16_t value; } rhmpiece;
extern const rhmpiece rhm_piece[RHM_PIECES];
extern const uint8_t rhm_layers[2][6];          /* the pyramids' layers (0: none) */

typedef struct {
    uint8_t sq[RHM_N];                          /* 0 empty, else a piece's id + 1 */
    uint8_t side;
    uint8_t quiet;                              /* moves since the last capture */
    uint8_t taken[2];                           /* the pieces each player took */
    uint8_t proper;                             /* 1: the player who just moved made a harmony (the proper victory) */
    uint8_t harmony[3];                         /* its three squares */
} rhmpos;

typedef struct { uint8_t from, to; } rhmmove;

typedef struct {
    rhmpos pos[RHM_MAX_PLY + 1];
    rhmmove move[RHM_MAX_PLY];
    int ply, variant;
} rhmgame;

int  rhm_goal(int variant);                     /* the pieces to take (0: the proper victory) */
/* 1: the piece on square a could move to square b (b's own piece, if any, left aside) */
int  rhm_reach(const rhmpos *p, int a, int b);
int  rhm_legal(const rhmpos *p, rhmmove *out);  /* the moves of the side to move; their count */
/* how the piece on square s would be taken by `player` (RHM_MEETING..SIEGE, 0: it is not); attackers: their squares
 * (up to 2), or NULL */
int  rhm_threat(const rhmpos *p, int player, int s, int *attackers);
/* a move without its captures (the pieces it would take: rhm_threat on the result) */
void rhm_step(rhmpos *p, rhmmove m);
void rhm_make(rhmpos *p, rhmmove m, int variant);   /* a move, its captures, the harmony looked for */
/* three of player's pieces in harmony in the enemy's half: 1 and their squares, else 0 */
int  rhm_find_harmony(const rhmpos *p, int player, uint8_t out[3]);
int  rhm_harmonic(int a, int b, int c);         /* 1: a b c (in order along the line) are in a progression */
int  rhm_square(const char *s, int *len);       /* "c12" -> its square (len: the characters read); -1: none */

void rhm_new(rhmgame *g, int variant);
int  rhm_play(rhmgame *g, rhmmove m);           /* 0: not legal, or the game is over */
int  rhm_undo(rhmgame *g);
int  rhm_status(const rhmgame *g);              /* RHM_* */
int  rhm_script(rhmgame *g, const char *s);     /* "c4d5 b13b11": 0 at the first that is not legal */
#define rhm_pos(g) (&(g)->pos[(g)->ply])

/* the bot (rithmomachia_bot.c, bots.h) */
const rhmgame *rithmomachia_live(void);         /* the game on the screen, read-only: the bot copies it at entry */
int rithmomachia_show(const char *move);        /* main thread: plays "c4d5" as a tap does (animated); 0: not legal */

#endif
