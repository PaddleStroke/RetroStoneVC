/*
 * Draughts for two players on one device: international draughts (10 x 10), English checkers, Russian, Brazilian and
 * Turkish draughts (8 x 8). No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Squares: y * n + x on an n x n board, (0, 0) the bottom left corner as player 0 sees it, a dark square: the pieces
 * stand on the squares where x + y is even (Turkish draughts: on every square). Player 0 moves first and starts at the
 * bottom; its men go up (y grows). English: dark moves first, so player 0 plays ebony; the others: player 0 plays
 * ivory (white).
 *
 * The square numbers (the notation of the international and English games): the standard ones, as the ivory player
 * sees the board: 1 the leftmost dark square of the far row, then on to the right and row by row towards the ivory
 * player. Ebony starts on the low numbers (international: 1-20, ivory 31-50; English: 1-12, ivory 21-32), so "32-28"
 * and "11-15" are the usual first moves. Russian, Brazilian and Turkish draughts write the squares as chess does: a1
 * the bottom left corner (x 0, y 0), h8 the top right; "c3-d4", "c3:e5" (Turkish: "a3-a4").
 */
#ifndef DRAUGHTS_H
#define DRAUGHTS_H

#include <stdint.h>

enum { DR_INTERNATIONAL, DR_ENGLISH, DR_RUSSIAN, DR_BRAZILIAN, DR_TURKISH, DR_VARIANTS };
enum { DR_EMPTY, DR_MAN0, DR_MAN1, DR_KING0, DR_KING1 };   /* a piece: its kind and its player */
#define DR_OWNER(pc)    (((pc) - 1) & 1)
#define DR_IS_KING(pc)  ((pc) >= DR_KING0)
#define DR_MAX_SQ       100
#define DR_MAX_STEPS    20              /* a capture takes at most every piece of the other player */
#define DR_MAX_MOVES    1024

typedef struct {
    uint8_t sq[DR_MAX_SQ];
    uint8_t side;                       /* who moves */
    uint16_t quiet;                     /* moves (plies) since the last capture or man move: the draw rules */
} dpos;

/* A move: from a square through its landing squares (one for a plain move, one per piece taken for a capture). */
typedef struct {
    uint8_t from, n, ncap, crown;       /* crown: a man becomes a king (ends on the far row; Russian: reaches it) */
    uint8_t path[DR_MAX_STEPS];         /* the landing squares; path[n - 1]: where the move ends */
    uint8_t cap[DR_MAX_STEPS];          /* the pieces taken, in the order they are jumped */
} dmove;

int  dr_size(int variant);                                 /* squares a side */
int  dr_all(int variant);                                  /* 1: every square used (Turkish); 0: the dark ones */
int  dr_pieces(int variant);                               /* each player's pieces at the start */
void dr_start(dpos *p, int variant);
int  dr_legal(const dpos *p, int variant, dmove *out);     /* the legal moves (capturing is compulsory); their count */
void dr_make(dpos *p, int variant, const dmove *m);        /* plays a (legal) move */
int  dr_same(const dmove *a, const dmove *b);              /* 1: the same move (start, end, pieces taken) */
/* the numbered variants (international, English): a square's number to the square (-1: no such number) and back;
 * the others: -1 */
int  dr_square(int variant, int number);
int  dr_number(int variant, int square);
/* the square written at s in the variant's notation ("32", "c3"): the square, -1 if none; *used: the characters read */
int  dr_read(int variant, const char *s, int *used);
/* the move written in s ("32-28", "19x30", "26x37x48": the landing squares on the way, when two routes end on the same
 * square; "c3-d4", "c3:e5" or "c3xe5"): its index in legal, -1 if none; *used: the characters read */
int  dr_parse(const dmove *legal, int nlegal, int variant, const char *s, int *used);
/* a position from lists of squares, "K" before a king: dr_setup(&p, DR_INTERNATIONAL, 0, "31 32 K46", "19"),
 * dr_setup(&p, DR_RUSSIAN, 0, "c3 Ke1", "d4") */
int  dr_setup(dpos *p, int variant, int side, const char *pieces0, const char *pieces1);

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { DR_PLAY, DR_WON0, DR_WON1, DR_REPETITION, DR_QUIET } dstatus;
#define DR_MAX_PLY 2048
typedef struct {
    int variant;
    dpos pos[DR_MAX_PLY + 1];
    dmove move[DR_MAX_PLY];
    int ply;
} dgame;
void dr_new(dgame *g, int variant);
int  dr_play(dgame *g, const dmove *m);                    /* 0: the game is too long */
int  dr_undo(dgame *g);                                    /* 0: nothing to undo */
dstatus dr_status(const dgame *g);                         /* no move (or no piece) left: the side to move lost */
#define dr_pos(g) (&(g)->pos[(g)->ply])

#endif
