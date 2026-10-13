/*
 * Chess for two players on one device: the rules (moves, check, mate, draws), standard or Chess960, and seven
 * variants of the rules: crazyhouse, three-check, king of the hill, losing chess, atomic, racing kings, horde. No
 * engine, no clock.
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
 * - CR_ANTI (losing chess, antichess): a capture must be made when one can (en passant too; any capture may be
 *   chosen); the king is an ordinary piece (no check, no castling: it may be taken), a pawn may also promote to a
 *   king. A player who has lost all their pieces, or who cannot move (stalemated), wins. Too few pieces: only
 *   bishops left, each side's all on one colour of squares, the two colours different (they can never meet).
 * - CR_ATOMIC: a capture explodes: the piece taken, the piece taking and every piece but a pawn on the eight squares
 *   round the capture leave the board. A king never captures. A move that blows up its own king is not allowed;
 *   blowing up the other king wins at once (even out of check). Kings side by side give no check. Too few pieces:
 *   as python-chess (a bare king cannot win; one knight, bishop or rook, or two knights, cannot win against a bare
 *   king; bishops that can never explode each other).
 * - CR_RACING (racing kings): no pawns, both armies on the first two ranks; no move may give check (nor leave its
 *   own king in check). The first king to reach the eighth rank wins; when the white king gets there, black has one
 *   more move, and a black king reaching it too makes a draw. Never too few pieces.
 * - CR_HORDE: white has 36 pawns and no king (no check for white: it cannot be mated) against black's usual army;
 *   white's pawns on the first rank may also move two squares (no en passant then). Black wins by taking every
 *   white piece, white by checkmating black. Never too few pieces (the fifty-move rule and repetition still draw).
 * The game over by these rules (three checks given, a king on the hill, a king blown up, a king home, the horde
 * gone), chess_legal() gives no move.
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
enum { CR_CHESS, CR_CRAZYHOUSE, CR_THREECHECK, CR_HILL, CR_ANTI, CR_ATOMIC, CR_RACING, CR_HORDE };
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
int  chess_in_check(const cpos *p, int color);     /* never in losing chess, nor kings side by side in atomic */
/* where a castling king and its rook end (g and f, or c and d) */
#define chess_castle_king(m) SQ(FILE_OF((m).to) > FILE_OF((m).from) ? 6 : 2, RANK_OF((m).from))
#define chess_castle_rook(m) SQ(FILE_OF((m).to) > FILE_OF((m).from) ? 5 : 3, RANK_OF((m).from))

/* A game: every position since the start, so undo is free and repetitions can be counted. */
/* ST_CHECKS: the third check (the side to move lost); ST_HILL: the other king on the hill (the side to move lost) */
/* ST_ANTI: losing chess, the side to move has no piece or no move: it WON; ST_EXPLODED: atomic, the side to move's
 * king blown up (it lost); ST_RACE: a king on the eighth rank, the other cannot follow (that king's side won);
 * ST_RACE_DRAW: both kings there; ST_HORDE: white has no piece left (white lost) */
typedef enum { ST_PLAY, ST_CHECKMATE, ST_STALEMATE, ST_INSUFFICIENT, ST_REPETITION, ST_FIFTY, ST_CHECKS,
               ST_HILL, ST_ANTI, ST_EXPLODED, ST_RACE, ST_RACE_DRAW, ST_HORDE } cstatus;
#define MAX_PLY 2048
typedef struct {
    cpos pos[MAX_PLY + 1];
    cmove move[MAX_PLY];
    int ply;
} cgame;
void game_new(cgame *g);
void game_new960(cgame *g, int n);                 /* from Chess960's start position n */
void game_new_rules(cgame *g, int rules);          /* the rules' start (the standard one, or racing kings',
                                                    * the horde's), played by those rules (CR_*) */
int  chess_over(const cpos *p);                    /* over by the variant's rules: ST_CHECKS, ST_HILL, ST_EXPLODED,
                                                    * ST_RACE, ST_RACE_DRAW, ST_HORDE; else 0 */
int  game_play(cgame *g, cmove m);                 /* 0: the game is too long */
int  game_undo(cgame *g);                          /* 0: nothing to undo */
cstatus game_status(const cgame *g);               /* checkmate: the side to move lost */
#define game_pos(g) (&(g)->pos[(g)->ply])

/* the bot (chess_bot.c, bots.h): the game on the screen, read-only, and one move in the notation played as if tapped
 * (chess_ui.c; a headless test defines both) */
const cgame *chess_live(void);
int chess_show(const char *move);

#endif
