/*
 * Makruk (Thai chess) for two players on one device: the rules (the moves, check, mate, stalemate, the counting
 * rules). No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Squares: 0 = a1, 7 = h1, 56 = a8, 63 = h8 (rank * 8 + file), rank 1 on player 0's side. A piece: its kind
 * (MK_BIA..MK_KHUN), its colour (bit 3: MK_WHITE 0, ivory, moves first; MK_BLACK 1, ebony), MK_PROMOTED (bit 4) on a
 * Met that was a Bia.
 * The start (the classical setup, the kings not facing each other): white Rua a1 h1, Ma b1 g1, Khon c1 f1, Khun d1,
 * Met e1, Bia on the third rank; black the same turned round: Rua a8 h8, Ma b8 g8, Khon c8 f8, Met d8, Khun e8, Bia on
 * the sixth rank. Each Khun stands on its owner's left of the Met.
 * The moves: Khun one square any way (no castling); Met one square diagonally; Khon one square diagonally or one
 * straight forwards; Ma as a chess knight; Rua as a chess rook; Bia one square straight forwards (no double step, no
 * en passant), capturing one square diagonally forwards. A Bia reaching the sixth rank (white; black: the third) is
 * promoted at once to a Met (a promoted Met moves as a Met). Check, checkmate (the player mated lost), stalemate (a
 * draw), no move may leave the mover's Khun attacked.
 * The counting (the honour rules), once no Bia (unpromoted) is left on the board, counted by the device:
 * - the board's honour: the count starts with the next player to move, 1 for each of their moves; when it reaches
 *   64 and the other player has not mated with their next move, the game is a draw;
 * - the pieces' honour: when a player has only their Khun left (and the other more), that player counts instead,
 *   starting from the number of pieces on the board (both Khun included) plus one with their first move; the limit:
 *   8 with two Rua, 16 with one, 22 with two Khon, 32 with two Ma, 44 with one Khon, else 64 (the stronger side's
 *   pieces when the count starts; a promoted Met counts as a Met). The limit and the count stay if pieces are taken
 *   later; the count passes the limit (or starts past it): a draw.
 * Other draws: only the two Khun left; the same position (the same player to move) the third time.
 * Notation (script, the screenshots and the tests): a move is its two squares, "e3e4" (or "e3-e4", "e3xe4"); a
 * promotion needs nothing more; separated by spaces: "e3e4 d6d5 e4d5".
 */
#ifndef MAKRUK_H
#define MAKRUK_H

#include <stdint.h>

enum { MK_EMPTY, MK_BIA, MK_MA, MK_KHON, MK_RUA, MK_MET, MK_KHUN };
enum { MK_WHITE, MK_BLACK };
#define MK_PROMOTED 16
#define MK_PC(color, kind)  ((uint8_t)((color) << 3 | (kind)))
#define MK_KIND(p)          ((p) & 7)
#define MK_COLOR(p)         (((p) >> 3) & 1)
#define MK_SQ(file, rank)   ((rank) * 8 + (file))
#define MK_FILE(s)          ((s) & 7)
#define MK_RANK(s)          ((s) >> 3)
#define MK_PROMO_RANK(color) ((color) == MK_WHITE ? 5 : 2)   /* the sixth rank, from the player's side */

#define MK_MAX_MOVES 128
#define MK_MAX_PLY 4096                          /* the game ends as a draw there (in practice never reached) */

enum { MK_COUNT_NONE, MK_COUNT_BOARD, MK_COUNT_PIECES };

typedef struct {
    uint8_t sq[64];
    uint8_t side;                               /* who moves */
    uint8_t count_kind;                         /* MK_COUNT_*: the honour count running */
    uint8_t counter;                            /* the player who counts (their moves) */
    uint8_t count;                              /* the count so far */
    uint8_t limit;                              /* the count's limit: reached on the counter's turn, a draw */
    uint16_t quiet;                             /* moves since the last capture or Bia move (repetitions) */
} mkpos;

typedef struct { uint8_t from, to, promo, taken; } mkmove;     /* promo: a Bia becomes a Met; taken: the piece */

int  mk_square(const char *s);                  /* "e3": its square; -1 */
void mk_start(mkpos *p);
int  mk_legal(const mkpos *p, mkmove *out);     /* the legal moves; their count */
void mk_make(mkpos *p, mkmove m);               /* plays a (legal) move */
int  mk_attacked(const mkpos *p, int sq, int by);
int  mk_king(const mkpos *p, int color);        /* the Khun's square */
int  mk_in_check(const mkpos *p, int color);
/* while a count runs: the moves the player who is not counting still has to give mate; -1: no count */
int  mk_moves_left(const mkpos *p);

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { MS_PLAY, MS_CHECKMATE, MS_STALEMATE, MS_BARE, MS_COUNT, MS_REPETITION, MS_LONG } mkstatus;
typedef struct {
    mkpos pos[MK_MAX_PLY + 1];
    mkmove move[MK_MAX_PLY];
    int ply;
} mkgame;
void mk_new(mkgame *g);
int  mk_play(mkgame *g, mkmove m);              /* 0: the game is too long */
int  mk_undo(mkgame *g);                        /* 0: nothing to undo */
/* MS_CHECKMATE: the player to move lost; MS_STALEMATE, MS_BARE, MS_COUNT, MS_REPETITION, MS_LONG: a draw */
mkstatus mk_status(const mkgame *g);
int  mk_script(mkgame *g, const char *moves);   /* plays moves in the notation above; 0: one is bad */
#define mk_pos(g) (&(g)->pos[(g)->ply])

/* the bot (makruk_bot.c, bots.h): the game on the screen, read-only, and one move ("e3e4") played as if tapped
 * (makruk.c; a headless test defines both) */
const mkgame *makruk_live(void);
int makruk_show(const char *move);

#endif
