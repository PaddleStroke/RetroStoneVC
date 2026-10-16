/*
 * Go Problems, for one player: life and death in a corner of the 19 x 19 board, Black to play. Kill (or capture) the
 * marked white stones, or make the marked black stones live. The stones follow go's rules (go_rules.c: captures, no
 * suicide, no position twice); a small solver of its own (no SDL) reads the corner and answers.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A problem: a window of w x h points in the corner (go's a1 at its corner: point x + 19 * y), its stones, and the
 * points that are outside the problem: empty, never played (marked on the board). A stone next to one of them can never
 * be taken (the walls round the corner are alive). The marked group (the key: one of its stones) belongs to the
 * defender: White in a kill, Black in a life problem. The attacker wins when the key stone is taken; the defender wins
 * when the key's group is safe for good (two true eyes: Benson's test), or it reaches a point outside (it ran away), or
 * both players pass in a row (seki: it stays).
 *
 * The solver: a bounded search over the corner's points (and passes), proven results only (an attacker's win, a
 * defender's, or not known within the depth), with a table of positions seen. The levels (goproblems_levels.h, made by
 * tools/goproblems_gen.c) have exactly one first move that wins, every other first move proven to lose; their depth is
 * the plies of the main line (the device's replies resist the longest). After each of the player's moves the solver
 * checks the win still stands within the depth (and a little slack); a move that loses it is a mistake: the reply
 * that refutes it is shown, then both are taken back.
 */
#ifndef GOPROBLEMS_H
#define GOPROBLEMS_H

#include <stdint.h>

#include "games/go.h"

#define GPZ_LEVELS 150
#define GPZ_CHAPTERS 6
#define GPZ_MAXW 13                             /* the window's most columns and rows */
#define GPZ_S 16                                /* a row of the solver's board: an edge column, the window, outside */
#define GPZ_CELLS (GPZ_S * GPZ_S)
#define GPZ_CELL(x, y) ((x) + 1 + ((y) + 1) * GPZ_S)
#define GPZ_X(c) ((c) % GPZ_S - 1)
#define GPZ_Y(c) ((c) / GPZ_S - 1)
#define GPZ_PASS 0                              /* a pass (cell 0 is off the board) */
#define GPZ_PROOF 15                            /* the depth that proves a level (tools/goproblems_gen.c) */
#define GPZ_SLACK 2                             /* the plies a player may take beyond the main line's */
#define GPZ_MAXMOVES 172
#define GPZ_LINE 40                             /* the main line's most plies */

enum { GPZ_EMPTY, GPZ_BLACK, GPZ_WHITE, GPZ_EDGE, GPZ_FREE };   /* a cell; FREE: outside the problem */
enum { GPZ_KILL, GPZ_LIVE };                    /* Black kills the white key group / makes the black one live */
enum { GPZ_UNK, GPZ_AWIN, GPZ_DWIN };           /* a result: not known, the attacker's, the defender's */

typedef struct {
    uint8_t c[GPZ_CELLS];
    uint64_t hash;                              /* of the stones */
} gpz_board;

typedef struct {
    int w, h, kind, key, first, depth;          /* key, first: cells; depth: the main line's plies */
    int nline;                                  /* the main line: Black's quickest win, White's longest resistance */
    uint8_t line[GPZ_LINE];                     /* its moves (cells, GPZ_PASS), White's replies included */
    int att, def;                               /* the colours: GPZ_BLACK / GPZ_WHITE */
    gpz_board start;
} gpz_problem;

/* ---- the solver -------------------------------------------------------------------------------------------------- */
/* a stone of colour on cell: 0 not allowed (taken, or suicide), else 1 + the stones it captured */
int  gpz_play(gpz_board *b, int cell, int colour);
/* the problem's end in b: GPZ_AWIN, GPZ_DWIN or 0 (it goes on) */
int  gpz_status(const gpz_problem *pz, const gpz_board *b);
/* 1: the block on cell can never be taken (Benson's test, on the solver's board) */
int  gpz_benson(const gpz_board *b, int cell);
typedef struct { int move, val, depth; } gpz_eval;   /* a move, its result, the plies it needs (this one counted) */
/* every legal move of side (passes: a pass came just before) after the positions in path (superko; the last is b's),
 * each searched up to maxdepth plies (this one counted): their count. nodes: the search's (may be NULL) */
int  gpz_analyse(const gpz_problem *pz, const gpz_board *b, int side, int passes, const uint64_t *path, int npath,
                 int maxdepth, gpz_eval *out, long *nodes);
/* the same for the position alone: the result for side to move (and the plies, the best move) */
int  gpz_prove(const gpz_problem *pz, const gpz_board *b, int side, int passes, const uint64_t *path, int npath,
               int maxdepth, int *depth, int *move);
void gpz_new_search(void);                      /* forget the positions seen (a new problem) */

/* ---- the problems ------------------------------------------------------------------------------------------------ */
/* "K 6 5 7 1,1 2,0 ..XOO,/..." : the kind (K kill, L live), w, h, the depth, the key and the first move (x,y), then
 * the rows from y 0 up (. empty, , outside, X black, O white) split by '/', then the main line's moves (x,y or p
 * a pass); 0: not a good one */
int  gpz_parse(gpz_problem *pz, const char *s);
/* the same written out (64 bytes or more) */
void gpz_write(const gpz_problem *pz, char *out, int size);
int  gpz_level_load(gpz_problem *pz, int level);   /* level 0..GPZ_LEVELS-1 */
int  gpz_level_chapter(int level);
int  gpz_pt(int cell);                          /* the go point of a cell */
int  gpz_cell(int pt);                          /* the cell of a go point (0: outside the window) */

/* ---- a problem played -------------------------------------------------------------------------------------------- */
enum { GPZ_ILLEGAL, GPZ_WRONG, GPZ_RIGHT, GPZ_SOLVED };
typedef struct {
    gpz_problem pz;
    gogame g;                                   /* the stones, with go's rules */
    int level;                                  /* -1: a random one */
    int solved, failed;                         /* the end reached: the player's win, or the line lost (a mistake
                                                 * shown, taken back by gpz_undo) */
    int mistakes, hints;
    int hint, hint_ply;                         /* the move hinted (a cell, GPZ_PASS), at the ply (-1: none) */
} gpz_game;

void gpz_begin(gpz_game *g, const gpz_problem *pz, int level);
int  gpz_level(gpz_game *g, int level);        /* 0: no such level */
int  gpz_to_move(const gpz_game *g);           /* 1: the player's turn, the problem not over */
/* the player's move (a cell or GPZ_PASS), then the reply. GPZ_ILLEGAL: not allowed (nothing counted); GPZ_WRONG: a
 * mistake (counted), the move and its refutation both played (gpz_undo takes them back); GPZ_RIGHT: played and
 * answered; GPZ_SOLVED: played (and answered: a pass that leaves the black group alive) */
int  gpz_try(gpz_game *g, int cell);
int  gpz_hint(gpz_game *g);                    /* a right move (counted once a turn); -1: none */
int  gpz_undo(gpz_game *g);                    /* back to the player's previous turn; 0: nothing to undo */
int  gpz_stars(const gpz_game *g);
int  gpz_cell_at(const gpz_game *g, int cell); /* what is on a cell now: GPZ_EMPTY, BLACK, WHITE, FREE */
int  gpz_key_group(const gpz_game *g, uint8_t mark[GPZ_CELLS]);   /* the key's group now marked; its stones */
/* "c2 b1 pass": the player's moves (the replies by themselves, a mistake taken back); "#n" first: level n + 1 */
int  gpz_script(gpz_game *g, const char *moves);

#endif
