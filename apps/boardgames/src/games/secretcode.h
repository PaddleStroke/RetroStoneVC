/*
 * Secret Code for one player: the rules. The app hides a code of coloured pegs; the player guesses it, row after row.
 * After each guess, small key pegs answer: an ebony one for each peg of the right colour in the right hole, an ivory
 * one for each other peg of a colour the code has (each peg of the code answers once). The code found: solved; the
 * rows used up: failed, and the code is shown.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 0 gentle (4 pegs, 6 colours, 10 guesses, no colour twice in the code), 1 classic (4 pegs, 6 colours,
 * 10 guesses, colours may repeat), 2 harder (5 pegs, 8 colours, 12 guesses, colours may repeat). A guess may always
 * repeat colours. The colours: 0..7, their letters in the moves' notation R Y G B W K P S (red lacquer, amber, jade,
 * lapis, ivory, ebony, plum, sky). The notation (script, the screenshots, the tests): the guesses, "RRGB YWKB", each
 * checked once full; a shorter last word leaves those pegs in the row being filled.
 */
#ifndef SECRETCODE_H
#define SECRETCODE_H

#include <stdint.h>

#define SC_VARIANTS 3
#define SC_MAXPEGS 5
#define SC_MAXCOLOURS 8
#define SC_MAXROWS 12
#define SC_HIST 64                              /* the pegs placed in the row being filled that undo can take back */

enum { SC_PLAY, SC_SOLVED, SC_FAILED };

typedef struct { int pegs, colours, rows, repeats; } sc_variant;
extern const sc_variant sc_variants[SC_VARIANTS];
extern const char sc_letters[SC_MAXCOLOURS + 1];    /* "RYGBWKPS" */

typedef struct {
    sc_variant v;
    int8_t code[SC_MAXPEGS];
    int8_t guess[SC_MAXROWS][SC_MAXPEGS];       /* the guesses checked, then the row being filled (-1: empty) */
    uint8_t black[SC_MAXROWS], white[SC_MAXROWS];   /* the key pegs of each guess checked */
    int n;                                      /* the guesses checked: guess[n] is the row being filled */
    struct { int8_t hole, was; } hist[SC_HIST];     /* the row being filled: each peg placed, what it replaced */
    int nh;
} sc_game;

/* the key pegs for a guess: right colour right hole (black), right colour elsewhere (white) */
void sc_score(const int8_t *code, const int8_t *guess, int pegs, int *black, int *white);
/* a code for the variant from a source of chance (rnd(n): 0..n-1; the shell's ui_random) */
void sc_make_code(int variant, int (*rnd)(int), int8_t *code);
void sc_new(sc_game *g, int variant, const int8_t *code);
int  sc_put(sc_game *g, int hole, int colour);  /* a peg in the row being filled; 0: not allowed */
int  sc_undo(sc_game *g);                       /* the last peg placed in the row taken back: its hole; -1: none */
int  sc_full(const sc_game *g);                 /* 1: the row being filled is full */
int  sc_check(sc_game *g);                      /* the full row checked (its key pegs); 0: not full, or over */
int  sc_status(const sc_game *g);               /* SC_* */
int  sc_left(const sc_game *g);                 /* the guesses left (this one included) */
int  sc_colour(char letter);                    /* a letter of the notation: its colour; -1: none */

#endif
