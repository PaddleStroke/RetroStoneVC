/*
 * Corner Blocks for two to four players: the rules (no SDL). Each player has the same 21 blocks (polyominoes of one to
 * five squares: 89 squares in all) to lay on the board, one a turn. A player's first block covers their start square;
 * each next one must touch at least one of their own blocks corner to corner, and never along an edge (other
 * players' blocks may touch it any way). A player who cannot lay a block is out (their turn is skipped from then on);
 * the game ends when nobody can lay one. The score: the squares laid, 15 more for laying all 21 blocks, and 5 more if
 * the single square was the last one laid. The highest score wins; a tie is shared.
 * Two players: a 14 x 14 board, the start squares inside it (e10 for ivory, j5 for ebony: 5 squares in from two
 * opposite corners). Three or four: 20 x 20, the start squares in the corners, in turn round the board: 0 ivory the
 * bottom left, 1 ebony the top left, 2 red the top right, 3 jade the bottom right.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Squares: x the column from the left (a..), y the row from the top (1..): square y * CBL_MAXN + x. A block (a..u, see
 * cornerblocks_rules.c) is turned by its orientation o 0..7: mirrored left to right when o >= 4, then turned o % 4
 * quarters clockwise; its squares are then set with their box's top left on the anchor square.
 * Notation (script, the screenshots, the tests, the bots): the block's letter, its orientation, the anchor: "t3c12";
 * separated by spaces. A move as an int: (block * 8 + o) * CBL_SQ + anchor.
 */
#ifndef CORNERBLOCKS_H
#define CORNERBLOCKS_H

#include <stdint.h>

#define CBL_BLOCKS 21
#define CBL_ALL ((1u << CBL_BLOCKS) - 1)
#define CBL_MAXN 20
#define CBL_SQ (CBL_MAXN * CBL_MAXN)
#define CBL_SEATS 4
#define CBL_MAX_PLY (CBL_BLOCKS * CBL_SEATS)
#define CBL_MOVES (CBL_BLOCKS * 8 * CBL_SQ)     /* the moves' range (their encoding) */
#define CBL_MONO 0                              /* the single square */

typedef struct { int8_t x[5], y[5]; uint8_t n, w, h; } cbl_shape;

typedef struct {
    uint8_t n, seats, turn, over;               /* the board's side, the players, the player to move, the end */
    uint8_t cell[CBL_SQ];                       /* 0 empty, p + 1 */
    uint32_t left[CBL_SEATS];                   /* the blocks still in hand (bit k: block k) */
    uint8_t last[CBL_SEATS];                    /* the last block laid (255: none) */
    uint8_t out[CBL_SEATS];                     /* 1: cannot lay a block any more */
    uint16_t laid[CBL_SEATS];                   /* the squares laid */
} cblpos;

/* the blocks (block k's squares, k 0..20 a..u) turned (o 0..7); the shape's squares from its box's top left */
void cbl_shape_of(int block, int o, cbl_shape *s);
int  cbl_size(int block);                       /* its squares */
int  cbl_unique(int block, int o);              /* 1: the first orientation giving this shape */
int  cbl_same(int block, int o);                /* the first orientation giving the same shape as o */
int  cbl_start(const cblpos *p, int seat);      /* its start square */

void cbl_new(cblpos *p, int seats);             /* 2 (14 x 14), 3 or 4 (20 x 20) */
/* 1: the player to move may lay that block, turned o, with its box's top left on the anchor */
int  cbl_fits(const cblpos *p, int block, int o, int anchor);
int  cbl_legal(const cblpos *p, int move);
int  cbl_moves(const cblpos *p, int *out, int cap);   /* every legal move of the player to move (distinct shapes) */
int  cbl_can_move(const cblpos *p, int seat);   /* 1: that player could lay a block now */
/* the squares where the player could set a block's corner: empty, touching one of theirs corner to corner and none
 * along an edge (the first move: the start square); bit array of CBL_SQ, their count */
int  cbl_corners(const cblpos *p, int seat, uint8_t *mark);
void cbl_apply(cblpos *p, int move);            /* plays a legal move: the turn passes to the next player who can
                                                 * play (those who cannot are out), or the game ends */
int  cbl_score(const cblpos *p, int seat);
unsigned cbl_winners(const cblpos *p);          /* the best score's players (bit n), once over */

int  cbl_move(int block, int o, int anchor);
int  cbl_block(int move);
int  cbl_orient(int move);
int  cbl_anchor(int move);
int  cbl_parse(const cblpos *p, const char *s, int *len);   /* a move's notation: the move, -1 bad */
void cbl_name(const cblpos *p, int move, char *out);        /* "t3c12" (8 bytes) */

/* A game: every position since the start, so undo is free. */
typedef struct {
    cblpos pos[CBL_MAX_PLY + 1];
    int32_t move[CBL_MAX_PLY];
    int ply;
} cblgame;
void cbl_newgame(cblgame *g, int seats);
int  cbl_play(cblgame *g, int move);            /* 0: not allowed */
int  cbl_undo(cblgame *g);
int  cbl_script(cblgame *g, const char *moves); /* 0: one is bad (those before it played) */
#define cbl_pos(g) (&(g)->pos[(g)->ply])

/* the bot (cornerblocks_bot.c, bots.h) */
const cblgame *cornerblocks_live(void);         /* the game on the screen, read-only: the bot copies it at entry */
int cornerblocks_show(const char *move);        /* main thread: a move as a tap plays it (animated); 0: not legal */
struct ai_stats;
int cblb_choose(const cblpos *p, int level, uint64_t seed);   /* the move, -1 none */

#endif
