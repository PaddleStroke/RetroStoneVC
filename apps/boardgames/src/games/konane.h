/*
 * Konane (Hawaiian checkers) for two players on one device: the rules (the opening removals, the jumps, the end). No
 * engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: n x n squares (6, 8 or 10: the variants), a1 at the bottom left (player 0's side); a square is x + n y
 * (x from the left, y from the bottom). At the start every square holds a stone, the two colours alternating like a
 * chessboard's squares: player 0 (ebony, black, first to play) on the squares whose x + y is odd (the light squares
 * of the house's board, whose a1 is dark), player 1 (ivory) on the others. A square holds -1 (empty) or a player.
 * The opening: player 0 removes one of their own stones, from the four middle squares or a corner (two of each are
 * theirs); then player 1 removes one of theirs next to that hole (orthogonally). These are moves (from == to).
 * Then a move is a jump: a stone jumps orthogonally over an enemy stone next to it onto the empty square just beyond,
 * taking the stone jumped. The same stone may jump again, straight on in the same direction (never turning), as far
 * as it can; it may stop after any jump (each length is one move here).
 * The end: a player who cannot move on their turn loses. There are no draws: every jump takes a stone, so a game
 * ends, and it ends only when a player has no move.
 * Notation (script, the screenshots and the tests): a removal "e4"; a jump "c4-e4" (or "c4xe4"), a longer one
 * "c4-g4" or "c4-e4-g4"; separated by spaces: "e4 e5 c4-e4".
 */
#ifndef KONANE_H
#define KONANE_H

#include <stdint.h>

#define KN_VARIANTS 3                           /* 6 x 6, 8 x 8, 10 x 10 */
#define KN_MAX_N 10
#define KN_MAX_SQ 100
#define KN_MAX_MOVES 800                        /* 50 stones, 4 ways, up to 4 lengths each */
#define KN_MAX_PLY 104                          /* the two removals, then a stone taken each move */

int kn_size(int variant);                       /* the board's side: 6, 8, 10 */

typedef struct { int8_t from, to; } knmove;     /* a removal: from == to */

typedef struct {
    int8_t at[KN_MAX_SQ];
    uint8_t n;                                  /* the board's side */
    uint8_t side;                               /* who moves */
    uint8_t opening;                            /* 0: player 0's removal, 1: player 1's, 2: the jumps */
    uint8_t took[2];                            /* the stones each player has taken */
} knpos;

int  kn_square(int n, const char *s, int *len); /* "e4", "j10": its square; -1. len: the characters read */
int  kn_owner(int n, int sq);                   /* the player whose stone starts there */
void kn_start(knpos *p, int n);
int  kn_legal(const knpos *p, knmove *out);     /* the legal moves; their count (0: the player to move lost) */
int  kn_jumped(const knpos *p, knmove m, int8_t *out);   /* the stones a jump takes, in order; their count */
void kn_make(knpos *p, knmove m);               /* plays a (legal) move */

/* A game: every position since the start, so undo is free. */
typedef struct {
    knpos pos[KN_MAX_PLY + 1];
    knmove move[KN_MAX_PLY];
    int ply;
} kngame;
void kn_new(kngame *g, int n);
int  kn_play(kngame *g, knmove m);              /* 0: the game is too long */
int  kn_undo(kngame *g);                        /* 0: nothing to undo */
int  kn_winner(const kngame *g);                /* the winner (the player to move has no move); -1: the game goes on */
int  kn_script(kngame *g, const char *moves);   /* plays moves in the notation above; 0: one is bad */
#define kn_pos(g) (&(g)->pos[(g)->ply])

#endif
