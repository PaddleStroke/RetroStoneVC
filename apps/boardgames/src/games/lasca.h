/*
 * Lasca (Emanuel Lasker, 1911) for two players on one device: the rules (draughts with columns: the pieces taken go
 * under the column that took them). No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 7 x 7 squares, a1 .. g7 (ls_x, ls_y: 0..6, a1 at 0, 0 in player 0's left corner; a square is x + 7 y);
 * the 25 squares where x + y is even are used (the light ones: the corners). A square holds a column of pieces, 0 to
 * 22 high: ls_top(col) its top piece, the one in command. A piece: its owner (LS_OWNER) and its rank (LS_OFFICER).
 * The rules: 11 soldiers each, on the three rows nearest their player. Player 0 (ivory) moves first. A column moves as
 * its top piece does: a soldier one square diagonally forward, an officer one square diagonally either way. It
 * captures by jumping a column commanded by the other player next to it onto the empty square just beyond (a soldier
 * forwards only, an officer either way), and takes only that column's top piece, which goes under the capturing
 * column (the pieces beneath stay, commanded now by their new top piece). After a capture the same column jumps on
 * while it can, a jump at a time (each jump is one move here: `chain` is the square of the column in the middle of
 * one), but never over the same square twice in one turn. Capturing is compulsory (any capture, not the longest),
 * and a chain goes on to its end. A soldier on top of its column that reaches the far row becomes an officer; in a
 * capture, that ends the move. A player who cannot move (no column, or every one blocked) loses. Draws: the same
 * position a third time; 40 moves each (LS_QUIET plies) without a capture or a soldier moving.
 * Notation (script, the screenshots and the tests): "c3-d4" (or "c3d4") a step; a capture the same, "c3-e5" (or
 * "c3xe5"); a chain "c3xe5xc7" (each jump one move); separated by spaces: "c3-d4 e5xc3".
 */
#ifndef LASCA_H
#define LASCA_H

#include <stdint.h>

#define LS_SIDE 7
#define LS_SQUARES 49
#define LS_PIECES 11
#define LS_MAX_MOVES 48                         /* 11 columns, 4 ways each at most */
#define LS_QUIET 80
#define LS_MAX_PLY 2048

#define LS_OWNER(pc)   ((pc) & 1)
#define LS_OFFICER(pc) (((pc) >> 1) & 1)
#define LS_SOLDIER0 0
#define LS_SOLDIER1 1
#define LS_OFFICER0 2
#define LS_OFFICER1 3

static inline int ls_x(int s) { return s % LS_SIDE; }
static inline int ls_y(int s) { return s / LS_SIDE; }
static inline int ls_used(int s) { return !((ls_x(s) + ls_y(s)) & 1); }
int  ls_square(const char *s);                  /* "c3": its square (a used one); -1 */

/* a column: its pieces from the top, 2 bits each (piece i at bits 2 i, 2 i + 1), and its height */
typedef struct { uint64_t pc; uint8_t h; } lscol;
static inline int ls_top(lscol c) { return (int)(c.pc & 3); }
static inline int ls_piece(lscol c, int i) { return (int)(c.pc >> (2 * i) & 3); }   /* i: 0 the top */

typedef struct { int8_t from, to, over; } lsmove;   /* over: the column a capture jumps; -1 for a step */

typedef struct {
    lscol col[LS_SQUARES];
    uint8_t side;                               /* who moves */
    int8_t chain;                               /* the column that has just captured and captures again; -1 */
    uint64_t jumped;                            /* the squares jumped in this chain (bit per square) */
    uint16_t quiet;                             /* moves since the last capture or soldier move */
} lspos;

void ls_start(lspos *p);
int  ls_legal(const lspos *p, lsmove *out);     /* the legal moves (only captures when there is one); their count */
void ls_make(lspos *p, lsmove m);               /* plays a (legal) move */
int  ls_columns(const lspos *p, int player);    /* the columns the player commands */
int  ls_prisoners(const lspos *p, int player);  /* the other player's pieces held in them */
/* a position from lists of columns, each written from the top down: "a1:Ws c3:wb e5:B" (w b soldiers, W B officers,
 * w and W player 0's); 0: a bad list */
int  ls_setup(lspos *p, int side, const char *columns);

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { LS_PLAY, LS_STUCK, LS_REPETITION, LS_QUIET_DRAW } lsstatus;
typedef struct {
    lspos pos[LS_MAX_PLY + 1];
    lsmove move[LS_MAX_PLY];
    int ply;
} lsgame;
void ls_new(lsgame *g);
int  ls_play(lsgame *g, lsmove m);              /* 0: the game is too long */
int  ls_undo(lsgame *g);                        /* 0: nothing to undo */
/* LS_STUCK: the player to move cannot move and lost; LS_REPETITION, LS_QUIET_DRAW: a draw */
lsstatus ls_status(const lsgame *g);
int  ls_script(lsgame *g, const char *moves);   /* plays moves in the notation above; 0: one is bad */
#define ls_pos(g) (&(g)->pos[(g)->ply])

#endif
