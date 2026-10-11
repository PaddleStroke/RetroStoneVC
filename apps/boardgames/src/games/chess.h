/*
 * Chess for two players on one device: the rules (moves, check, mate, draws), standard or Chess960. No engine, no
 * clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Squares: 0 = a1, 7 = h1, 56 = a8, 63 = h8 (rank * 8 + file). A piece: its type (PAWN..KING) and its colour (bit 3).
 */
#ifndef CHESS_H
#define CHESS_H

#include <stdint.h>

enum { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum { WHITE, BLACK };
#define PC(color, type)  ((uint8_t)((color) << 3 | (type)))
#define TYPE(p)          ((p) & 7)
#define COLOR(p)         ((p) >> 3)
#define SQ(file, rank)   ((rank) * 8 + (file))
#define FILE_OF(s)       ((s) & 7)
#define RANK_OF(s)       ((s) >> 3)

/* the castling rights: K with the rook on the h side of the king (the king ends on g), Q on the a side (on c) */
enum { CASTLE_WK = 1, CASTLE_WQ = 2, CASTLE_BK = 4, CASTLE_BQ = 8 };
/* MV_CASTLE goes from the king's square onto its own rook's square (Chess960's way: one move, never ambiguous) */
enum { MV_CAPTURE = 1, MV_EP = 2, MV_CASTLE = 4, MV_DOUBLE = 8 };

typedef struct {
    uint8_t sq[64];
    uint8_t side;               /* who moves */
    uint8_t castle;             /* CASTLE_* still allowed */
    int8_t ep;                  /* the square a pawn just skipped, when an enemy pawn can take it there; else -1 */
    uint8_t rook[4];            /* the file of the rook each CASTLE_* right castles with (a and h, or Chess960's) */
    uint16_t halfmove;          /* moves since the last capture or pawn move (the fifty-move rule) */
    uint16_t fullmove;
} cpos;

typedef struct { uint8_t from, to, promo, flags; } cmove;
#define MAX_MOVES 256

void chess_start(cpos *p);
void chess_start960(cpos *p, int n);               /* Chess960's start position n (0..959; 518: the standard one) */
int  chess_from_fen(cpos *p, const char *fen);     /* 0: not a FEN. Castling: KQkq, or the rooks' files (HFhf) */
int  chess_legal(const cpos *p, cmove *out);       /* the legal moves; their count */
void chess_make(cpos *p, cmove m);                 /* plays a (legal) move */
int  chess_attacked(const cpos *p, int sq, int by);
int  chess_king(const cpos *p, int color);         /* the king's square */
int  chess_in_check(const cpos *p, int color);
/* where a castling king and its rook end (g and f, or c and d) */
#define chess_castle_king(m) SQ(FILE_OF((m).to) > FILE_OF((m).from) ? 6 : 2, RANK_OF((m).from))
#define chess_castle_rook(m) SQ(FILE_OF((m).to) > FILE_OF((m).from) ? 5 : 3, RANK_OF((m).from))

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { ST_PLAY, ST_CHECKMATE, ST_STALEMATE, ST_INSUFFICIENT, ST_REPETITION, ST_FIFTY } cstatus;
#define MAX_PLY 2048
typedef struct {
    cpos pos[MAX_PLY + 1];
    cmove move[MAX_PLY];
    int ply;
} cgame;
void game_new(cgame *g);
void game_new960(cgame *g, int n);                 /* from Chess960's start position n */
int  game_play(cgame *g, cmove m);                 /* 0: the game is too long */
int  game_undo(cgame *g);                          /* 0: nothing to undo */
cstatus game_status(const cgame *g);               /* checkmate: the side to move lost */
#define game_pos(g) (&(g)->pos[(g)->ply])

#endif
