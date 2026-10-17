/*
 * Draw and Guess, a drawing game for 2 to 8 people round one device: the rules. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The players draw in turn. The one drawing sees a word alone (one of DG_WORDS simple things, never twice in a game),
 * then draws it on the device for DG_SECONDS while the others call out their guesses; no letters, no numbers. The
 * first right guess: the guesser 2 points, the drawer 1. Each player draws 3 times (2 from 4 players); the most points
 * win (a tie shares the win).
 * Notation (script; the screenshots and the tests), words separated by spaces, players from 0: l the drawer has seen
 * the word and starts drawing, g<p> player p guessed it, t the time is up, n the next turn.
 */
#ifndef DRAWGUESS_H
#define DRAWGUESS_H

#include <stdint.h>

#define DG_WORDS 300
#define DG_MINP 2
#define DG_MAXP 8
#define DG_VARIANTS (DG_MAXP - DG_MINP + 1)
#define DG_SECONDS 60

enum { DG_LOOK, DG_DRAW, DG_SHOW, DG_OVER };

typedef struct {
    int n, turn, turns, phase;
    int drawer, word, guesser;                  /* this turn's; guesser -1: nobody (yet) */
    int score[DG_MAXP], gain[DG_MAXP];          /* the totals; this turn's points */
    uint16_t deck[DG_WORDS];                    /* the words in the game's order */
    uint32_t rng;
} dg_state;

int  dg_rounds(int n);                          /* how many times each player draws */
void dg_new(dg_state *s, int n, uint32_t seed);
int  dg_begin(dg_state *s);                     /* l */
int  dg_guessed(dg_state *s, int p);            /* g */
int  dg_timeout(dg_state *s);                   /* t */
int  dg_next(dg_state *s);                      /* n */
unsigned dg_winners(const dg_state *s);         /* the best total (bit n: player n) */
int  dg_script(dg_state *s, const char *moves);

#endif
