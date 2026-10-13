/*
 * Shogi (Japanese chess) for two players on one device: the rules (the moves, promotions, drops and their limits,
 * check, mate, repetition), on the 9 x 9 board or mini shogi's 5 x 5 (Gosho Takagi's: a king, gold, silver, bishop and
 * rook each, a pawn in front of the king; the promotion zone the last rank). No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Squares: row * 9 + col, as sente (player 0, black, moving first, at the bottom) sees the board: row 0 the far rank
 * (USI "a"), col 0 the left file (USI "9", mini shogi "5"); an n x n board uses rows and columns 0..n-1. Sente's
 * pieces go up (the row falls), gote's (player 1) down.
 * A piece: its kind (SG_PAWN..SG_KING), SG_PROMOTED, its owner (SG_GOTE). The pieces in hand: counts by kind.
 *
 * The notation (USI): files 1..n from the right, ranks a.. from the top, as sente sees the board: "7g7f" a move,
 * "8h2b+" a move that promotes, "P*5e" a drop (P L N S G B R).
 *
 * The end: a player with no legal move (checkmate, or none at all) loses. The same position (the board, the hands, the
 * player to move) a fourth time: a draw (sennichite; mini shogi: sente loses), unless one player gave check with every
 * move since the first time: that player loses (perpetual check). No impasse rule (jishogi): two kings safe in the
 * far camps play on.
 */
#ifndef SHOGI_H
#define SHOGI_H

#include <stdint.h>

enum { SG_EMPTY, SG_PAWN, SG_LANCE, SG_KNIGHT, SG_SILVER, SG_GOLD, SG_BISHOP, SG_ROOK, SG_KING };
enum { SG_STANDARD, SG_MINI, SG_VARIANTS };     /* 9 x 9, 5 x 5 */
#define SG_PROMOTED      16
#define SG_GOTE          32
#define SG_PC(owner, kind) ((uint8_t)((owner) * SG_GOTE | (kind)))
#define SG_KIND(pc)      ((pc) & 15)
#define SG_FACE(pc)      ((pc) & 31)            /* the kind and the promotion: what the piece shows */
#define SG_OWNER(pc)     ((pc) >> 5)
#define SG_SQ(col, row)  ((row) * 9 + (col))
#define SG_COL(s)        ((s) % 9)
#define SG_ROW(s)        ((s) / 9)

typedef struct {
    uint8_t sq[81];
    uint8_t hand[2][8];                         /* the pieces in hand, by kind (SG_PAWN..SG_ROOK) */
    uint8_t side;                               /* who moves */
    uint8_t n;                                  /* the board's side: 9, or 5 (mini shogi) */
} spos;

/* a move: from a square to a square (promo: it promotes), or a drop (drop: the kind put down; from unused) */
typedef struct { uint8_t from, to, drop, promo; } smove;
#define SG_MAX_MOVES 640                        /* the most known in a legal position: 593 */

void sg_start(spos *p, int variant);
/* a position in SFEN: "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1" (upper case: sente; "+P" a
 * promoted pawn; then the side, "b" sente or "w" gote; then the hands, "-" or "2Pb"); the board 9 or 5 ranks of as
 * many squares ("rbsgk/4p/5/P4/KGSBR b - 1"). 0: not one. */
int  sg_from_sfen(spos *p, const char *sfen);
int  sg_legal(const spos *p, smove *out);       /* the legal moves; their count */
void sg_make(spos *p, smove m);                 /* plays a (legal) move */
int  sg_attacked(const spos *p, int sq, int by);
int  sg_king(const spos *p, int player);        /* the king's square; -1: none */
int  sg_in_check(const spos *p, int player);
int  sg_zone(const spos *p, int player, int row);   /* 1: the row is in the player's promotion zone */
/* the move written in s ("7g7f", "8h2b+", "P*5e") on an n x n board: its index in legal, -1 if none; *used: the
 * characters read */
int  sg_parse(int n, const smove *legal, int nlegal, const char *s, int *used);
void sg_usi(int n, smove m, char out[8]);       /* the move in USI */

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { SG_PLAY, SG_WON0, SG_WON1, SG_DRAW } sstatus;
#define SG_MAX_PLY 2048
typedef struct {
    spos pos[SG_MAX_PLY + 1];
    smove move[SG_MAX_PLY];
    int ply;
} sgame;
void sg_new(sgame *g, int variant);
int  sg_play(sgame *g, smove m);                /* 0: the game is too long */
int  sg_undo(sgame *g);                         /* 0: nothing to undo */
sstatus sg_status(const sgame *g);
#define sg_pos(g) (&(g)->pos[(g)->ply])

/* the bot (shogi_bot.c, bots.h): the game on the screen, read-only, and one move in USI played as if tapped (shogi.c;
 * a headless test defines both) */
const sgame *shogi_live(void);
int shogi_show(const char *move);

#endif
