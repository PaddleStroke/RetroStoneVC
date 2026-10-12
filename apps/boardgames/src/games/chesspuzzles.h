/*
 * Chess Puzzles, for one player: about 10,000 positions from the Lichess puzzle database (public domain), in six packs
 * (five by rating, and the mates). The chess rules are chess.c's; these rules only follow a puzzle's line (no SDL).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A puzzle (Lichess's way): the position is the one BEFORE the opponent's last move. Its first move is the opponent's,
 * played by itself; then the player must find each of their own moves, and the opponent's replies come by themselves.
 * A legal move that is not the one of the line is a mistake (counted, and not played), unless it mates: any mate wins.
 * The player's pawns promote to a queen (the collection holds no puzzle where the player must under-promote).
 * The collection: chesspuzzles_data.h, made by tools/pick_chess_puzzles.py (its record's layout is described there).
 */
#ifndef CHESSPUZZLES_H
#define CHESSPUZZLES_H

#include <stdint.h>

#include "games/chess.h"

enum { CPZ_BEGINNER, CPZ_EASY, CPZ_MEDIUM, CPZ_HARD, CPZ_EXPERT, CPZ_MATES, CPZ_VARIANTS };
#define CPZ_MAX_MOVES 8                         /* the opponent's first move included */

typedef struct {
    cpos start;                                 /* before the opponent's first move */
    cmove move[CPZ_MAX_MOVES];                  /* the line, as chess.c's legal moves */
    int n;                                      /* its moves */
    int rating;
    uint32_t themes;                            /* CPZ_TH_* (chesspuzzles_data.h) */
    char id[6];                                 /* the Lichess puzzle id */
} cpz_puzzle;

typedef struct {
    cpz_puzzle pz;
    int pack, index;
    cpos pos[CPZ_MAX_MOVES + 1];                /* pos[k]: after k moves */
    cmove played[CPZ_MAX_MOVES];                /* the moves played (another mate may stand for the line's last) */
    int ply;                                    /* the moves played: 1 once the opponent's first is on the board */
    int solver;                                 /* the player's colour (the side not to move in the start) */
    int mistakes, hints;                        /* this puzzle's */
    int hint_level;                             /* for the move to find: 0, 1 the piece shown, 2 its square too */
} cpz_game;

enum { CPZ_ILLEGAL, CPZ_WRONG, CPZ_RIGHT, CPZ_SOLVED };

int  cpz_count(int pack);                       /* the puzzles in a pack */
int  cpz_total(void);
int  cpz_load(cpz_puzzle *pz, int pack, int index);   /* 0: not a good record (every one is: the test checks) */
/* a puzzle: its start, the opponent's first move played (ply 1); 0: a bad record */
int  cpz_start(cpz_game *g, int pack, int index);
/* the move from -> to (promotion: a queen; castling: e1g1 or the king onto its rook). CPZ_ILLEGAL: not a legal move
 * (nothing counted); CPZ_WRONG: legal, not the line's (a mistake, not played); CPZ_RIGHT: played, the line goes on
 * (the reply is due: cpz_reply); CPZ_SOLVED: played, the end of the line or a mate */
int  cpz_try(cpz_game *g, int from, int to);
int  cpz_reply_due(const cpz_game *g);          /* 1: the opponent's reply comes next */
cmove cpz_reply(cpz_game *g);                   /* plays it */
int  cpz_solved(const cpz_game *g);
int  cpz_to_find(const cpz_game *g);            /* 1: the player is to move, and the puzzle goes on */
cmove cpz_expected(const cpz_game *g);          /* the line's move the player must find */
int  cpz_hint(cpz_game *g);                     /* the hint's next level (1 the piece, 2 its square); 0: none to give */
int  cpz_undo(cpz_game *g);                     /* back to the player's previous move; 0: nothing to undo */
#define cpz_pos(g) (&(g)->pos[(g)->ply])
/* the moves where the moving piece lands (a castling king: on g or c) */
int  cpz_lands(cmove m);
/* "e2e4 g1f3": the player's moves (the replies by themselves), for the pictures and the tests; 0: a move refused */
int  cpz_script(cpz_game *g, const char *moves);
/* the legal move from -> to in p into *m (promo: the piece a pawn becomes, 0: a queen; castling as cpz_try); 0: none */
int  cpz_find(const cpos *p, int from, int to, int promo, cmove *m);

#endif
