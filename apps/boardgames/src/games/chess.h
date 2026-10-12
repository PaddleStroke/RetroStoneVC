/*
 * Chess for two players on one device: the rules (moves, check, mate, draws), standard or Chess960, and three
 * variants of the rules: crazyhouse, three-check, king of the hill. No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Squares: 0 = a1, 7 = h1, 56 = a8, 63 = h8 (rank * 8 + file). A piece: its type (PAWN..KING) and its colour (bit 3).
 *
 * The rules (cpos.rules; Chess960 is CR_CHESS from another start):
 * - CR_CRAZYHOUSE: a piece taken goes to the taker's hand, of the taker's colour; instead of a move a player may drop
 *   a piece from the hand on any empty square (a pawn not on the first or last rank; a drop may check, even mate). A
 *   promoted piece goes back to the hand as a pawn. No draw for too few pieces (none ever leaves the game).
 * - CR_THREECHECK: the third check given wins (checkmate too). Too few pieces: only the two kings left.
 * - CR_HILL: a king reaching d4, e4, d5 or e5 wins (checkmate too). Never too few pieces (a king can walk there).
 * The game over by these rules (three checks given, a king on the hill), chess_legal() gives no move.
 * FEN: the hand in brackets after the board ("...RNBQKBNR[Pn] w"), "~" after a promoted piece ("Q~"); three-check:
 * "3+3" the checks still to give (white's, black's) or "+0+0" the checks given (lichess).
 * The script's drops: "N@f3".
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
/* MV_DROP: a piece from the hand (crazyhouse): `drop` its type, from and to the square it lands on */
enum { MV_CAPTURE = 1, MV_EP = 2, MV_CASTLE = 4, MV_DOUBLE = 8, MV_DROP = 16 };
enum { CR_CHESS, CR_CRAZYHOUSE, CR_THREECHECK, CR_HILL };
#define CHESS_HILL(s)    ((s) == 27 || (s) == 28 || (s) == 35 || (s) == 36)   /* d4 e4 d5 e5 */

typedef struct {
    uint8_t sq[64];
    uint8_t side;               /* who moves */
    uint8_t castle;             /* CASTLE_* still allowed */
    int8_t ep;                  /* the square a pawn just skipped, when an enemy pawn can take it there; else -1 */
    uint8_t rook[4];            /* the file of the rook each CASTLE_* right castles with (a and h, or Chess960's) */
    uint16_t halfmove;          /* moves since the last capture or pawn move (the fifty-move rule) */
    uint16_t fullmove;
    uint8_t rules;              /* CR_* */
    uint8_t hand[2][7];         /* crazyhouse: the pieces each colour holds, by type */
    uint8_t checks[2];          /* three-check: the checks each colour has given */
    uint64_t promoted;          /* crazyhouse: the squares of promoted pieces (bit per square) */
} cpos;

typedef struct { uint8_t from, to, promo, flags, drop; } cmove;
#define MAX_MOVES 512

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
/* ST_CHECKS: the third check (the side to move lost); ST_HILL: the other king on the hill (the side to move lost) */
typedef enum { ST_PLAY, ST_CHECKMATE, ST_STALEMATE, ST_INSUFFICIENT, ST_REPETITION, ST_FIFTY, ST_CHECKS,
               ST_HILL } cstatus;
#define MAX_PLY 2048
typedef struct {
    cpos pos[MAX_PLY + 1];
    cmove move[MAX_PLY];
    int ply;
} cgame;
void game_new(cgame *g);
void game_new960(cgame *g, int n);                 /* from Chess960's start position n */
void game_new_rules(cgame *g, int rules);          /* the standard start, played by those rules (CR_*) */
int  chess_over(const cpos *p);                    /* won by the variant's rules: ST_CHECKS, ST_HILL; else 0 */
int  game_play(cgame *g, cmove m);                 /* 0: the game is too long */
int  game_undo(cgame *g);                          /* 0: nothing to undo */
cstatus game_status(const cgame *g);               /* checkmate: the side to move lost */
#define game_pos(g) (&(g)->pos[(g)->ply])

#endif
