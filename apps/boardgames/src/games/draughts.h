/*
 * Draughts for two players on one device: international draughts (10 x 10), English checkers, Russian, Brazilian and
 * Turkish draughts (8 x 8), Frisian draughts (10 x 10), Canadian draughts (12 x 12) and bashni (Russian column
 * draughts, 8 x 8). No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Frisian: men and kings capture along the rows and the files too (over the dark square two squares away, onto the
 * one two further on; a king from afar along them, every second square). The capture of the most value is
 * compulsory: a king is worth more than a man and less than two (k kings are worth less than 2 k men, more than
 * 2 k - 1); of equal value, a king's capture before a man's. A man is crowned ending its move on the far row. Kings
 * move along the diagonals only, from afar. A player with men and kings may not move the same king more than three
 * times in a row without capturing (then another piece moves). Draws: two kings against one, after seven moves each;
 * one king each, after two moves each; the same position a third time.
 * Canadian: the international rules on 12 x 12, 30 men each.
 * Bashni: the Russian rules, but a capture takes only the top piece of the column jumped, which goes under the
 * capturing column (the pieces beneath stay, commanded by their new top piece; a column moves as its top piece, and
 * only the top piece is crowned). A column jumped in a capture is not jumped again in it (the Turkish strike), and
 * the pieces taken go under the capturing column, in the order taken, as the move ends.
 *
 * Squares: y * n + x on an n x n board, (0, 0) the bottom left corner as player 0 sees it, a dark square: the pieces
 * stand on the squares where x + y is even (Turkish draughts: on every square). Player 0 moves first and starts at the
 * bottom; its men go up (y grows). English: dark moves first, so player 0 plays ebony; the others: player 0 plays
 * ivory (white).
 *
 * The square numbers (the notation of the international, English, Frisian and Canadian games): the standard ones, as
 * the ivory player sees the board: 1 the leftmost dark square of the far row, then on to the right and row by row
 * towards the ivory player. Ebony starts on the low numbers (international: 1-20, ivory 31-50; English: 1-12, ivory
 * 21-32; Canadian: 1-30, ivory 43-72), so "32-28" and "11-15" are the usual first moves. Russian, Brazilian, Turkish
 * draughts and bashni write the squares as chess does: a1 the bottom left corner (x 0, y 0), h8 the top right;
 * "c3-d4", "c3:e5" (Turkish: "a3-a4").
 */
#ifndef DRAUGHTS_H
#define DRAUGHTS_H

#include <stdint.h>

enum { DR_INTERNATIONAL, DR_ENGLISH, DR_RUSSIAN, DR_BRAZILIAN, DR_TURKISH, DR_FRISIAN, DR_CANADIAN, DR_BASHNI,
       DR_VARIANTS };
enum { DR_EMPTY, DR_MAN0, DR_MAN1, DR_KING0, DR_KING1 };   /* a piece: its kind and its player */
#define DR_OWNER(pc)    (((pc) - 1) & 1)
#define DR_IS_KING(pc)  ((pc) >= DR_KING0)
#define DR_MAX_SQ       144
#define DR_MAX_STEPS    32              /* a capture takes at most every piece of the other player */
#define DR_MAX_MOVES    1024
#define DR_COLUMNS      64              /* bashni's board */

typedef struct {
    uint8_t sq[DR_MAX_SQ];              /* the pieces (bashni: the top piece of each column) */
    uint8_t side;                       /* who moves */
    uint16_t quiet;                     /* moves (plies) since the last capture or man move: the draw rules */
    uint8_t kmoves[2], kat[2];          /* Frisian: the simple moves in a row of each player's king, and where it is */
    /* bashni: the pieces under each top piece, 2 bits each from the top (piece - 1: bit 0 the owner, bit 1 a king,
     * as column.h), and how many */
    uint8_t nunder[DR_COLUMNS];
    uint64_t under[DR_COLUMNS];
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
/* bashni: the pieces under the top piece on square s, written from the top down ("wb": a man of player 0, then one
 * of player 1; "W B": kings); 0: a bad list or an empty square */
int  dr_stack(dpos *p, int s, const char *under);
int  dr_columns(int variant);                              /* 1: the pieces taken go under the taker (bashni) */
int  dr_prisoners(const dpos *p, int player);              /* bashni: the other's pieces under the player's columns */
int  dr_height(const dpos *p, int s);                      /* the pieces on square s (a column's height) */
uint64_t dr_column(const dpos *p, int s);                  /* the column on s, 2 bits a piece from the top (column.h) */

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

/* the bot (draughts_bot.c, bots.h: every variant), and what the screen (draughts.c) gives it; a headless test defines
 * those two */
const dgame *draughts_live(void);               /* the game on the screen, read-only: the bot reads it at entry */
int draughts_show(const char *move);            /* main thread: plays "26x37x48" as taps do (animated); 0: not legal */
struct ai_stats;
/* the move in the game g (its side to move; g read at once: the position and the ones before it that may come again)
 * at a level, in out; 0: none. dgb_write(): a move in the variant's notation, every landing square */
int dgb_choose(const dgame *g, int level, uint64_t seed, dmove *out, struct ai_stats *stats);
void dgb_write(int variant, const dmove *m, char *out, int cap);

#endif
