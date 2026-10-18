/*
 * Corridor for two or four players on one device: the rules (pawn steps and jumps, walls, the path that must stay
 * open, the end). No SDL, no engine.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 9 x 9 squares, a1 at the bottom left, a square x + 9 y (x the file a..i, y the rank 1..9, from 0).
 * The pawns start in the middle of their edge and race to the opposite one: two players, 0 (ivory) on e1 for rank 9,
 * 1 (ebony) on e9 for rank 1; four players, in turn round the board: 0 bottom (e1, to rank 9), 1 left (a5, to the i
 * file), 2 top (e9, to rank 1), 3 right (i5, to the a file). Player 0 moves first.
 * Walls: each is two squares long and lies in the grooves between squares. A wall is named by the slot at its middle,
 * the corner where four squares meet: slot x + 8 y (x, y 0..7) touches the squares (x, y), (x + 1, y), (x, y + 1),
 * (x + 1, y + 1). A horizontal wall there runs between ranks y and y + 1 across files x and x + 1; a vertical one
 * between files x and x + 1 along ranks y and y + 1. Walls may not overlap or cross, and no wall may leave a pawn
 * without a way to its goal. Each player has 10 walls (two players) or 5 (four players).
 * A turn: move one's pawn, or place a wall. A pawn steps to a square next to it (not diagonally) across no wall. Onto
 * a pawn: it jumps straight over it, if no wall, no edge and no pawn is behind it; else it goes to the square beside
 * that pawn, left or right of the way it came (across no wall). No jumping over two pawns.
 * The first pawn to reach its goal wins. A player who can neither move nor place a wall passes. Nobody home after 100
 * turns each: a draw.
 * Notation (script, the screenshots, the tests, the bots): "e2" the pawn to e2; "e3h", "e3v" a wall at the slot whose
 * lower left square is e3 (files a..h, ranks 1..8); "-" a pass; separated by spaces.
 */
#ifndef CORRIDOR_H
#define CORRIDOR_H

#include <stdint.h>

#define CR_N 9
#define CR_CELLS (CR_N * CR_N)
#define CR_SLOTS 64                             /* 8 x 8 wall slots */
#define CR_PLAYERS 4
#define CR_TURNS 100                            /* each, then a draw */
#define CR_MAX_PLY (CR_TURNS * CR_PLAYERS)
#define CR_DRAW 4                               /* crpos.winner: nobody */
#define CR_FAR 255                              /* cr_dist(): no way */

enum { CR_NONE, CR_H, CR_V };                   /* a slot's wall */
enum { CR_UP, CR_RIGHT, CR_DOWN, CR_LEFT };     /* up: towards rank 9 */

/* a move: a square (0..80), a wall CR_WALL(slot, CR_H / CR_V), or CR_PASS */
#define CR_WALL0 100
#define CR_WALL(slot, o) (CR_WALL0 + (slot) * 2 + (o) - 1)
#define CR_IS_WALL(m) ((m) >= CR_WALL0 && (m) < CR_WALL0 + 2 * CR_SLOTS)
#define CR_SLOT(m) (((m) - CR_WALL0) / 2)
#define CR_ORIENT(m) (((m) - CR_WALL0) % 2 + 1)
#define CR_PASS 250
#define CR_MAX_MOVES (8 + 2 * CR_SLOTS)

typedef struct {
    uint8_t pawn[CR_PLAYERS];                   /* squares */
    uint8_t left[CR_PLAYERS];                   /* walls still to place */
    uint8_t wall[CR_SLOTS];                     /* CR_NONE, CR_H, CR_V */
    uint8_t players, turn;
    int8_t winner;                              /* -1 playing; a player; CR_DRAW */
    uint8_t pad;
    uint16_t turns;                             /* turns played */
} crpos;

void cr_start(crpos *p, int players);           /* 2 or 4 */
int  cr_square(const char *s);                  /* "e3": its square; -1 */
int  cr_goal(const crpos *p, int player, int sq);   /* 1: sq is on that player's goal edge */
int  cr_blocked(const crpos *p, int sq, int dir);   /* 1: a wall or the edge on that side of sq */
int  cr_step(int sq, int dir);                  /* the square next to sq that way; -1 off the board */
int  cr_pawn_moves(const crpos *p, uint8_t *out);   /* where the pawn to move may go; their count (at most 8) */
/* 1: a wall fits there (on the board, no overlap, no crossing) */
int  cr_wall_fits(const crpos *p, int slot, int orient);
/* 1: the player to move may place it (it fits, a wall left, every pawn keeps a way to its goal) */
int  cr_wall_ok(const crpos *p, int slot, int orient);
int  cr_dist(const crpos *p, int player);       /* the fewest steps to the goal, walls only (CR_FAR: none) */
/* the same, and the squares of one shortest way (path[0] the pawn's square, path[n] on the goal); n, or -1 */
int  cr_path(const crpos *p, int player, uint8_t *path);
int  cr_moves(const crpos *p, int *out);        /* every legal move of the player to move (CR_PASS alone if none) */
int  cr_legal(const crpos *p, int move);
void cr_apply(crpos *p, int move);              /* plays a legal move: the turn, the end */
int  cr_parse(const crpos *p, const char *s, int *len);   /* a move's notation: the move, -1 bad */
void cr_name(int move, char *out);              /* "e2", "e3h", "-" (4 bytes) */

/* A game: every position since the start, so undo is free. */
typedef struct {
    crpos pos[CR_MAX_PLY + 1];
    int16_t move[CR_MAX_PLY];
    int ply;
} crgame;
void cr_new(crgame *g, int players);
int  cr_play(crgame *g, int move);              /* 0: not allowed */
int  cr_undo(crgame *g);
int  cr_script(crgame *g, const char *moves);   /* 0: one is bad (those before it played) */
#define cr_pos(g) (&(g)->pos[(g)->ply])

/* the bot (corridor_bot.c, bots.h) */
const crgame *corridor_live(void);              /* the game on the screen, read-only: the bot copies it at entry */
int corridor_show(const char *move);            /* main thread: a move as a tap plays it (animated); 0: not legal */

#endif
