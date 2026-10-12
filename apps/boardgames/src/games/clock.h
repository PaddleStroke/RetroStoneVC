/*
 * Clock, the patience that plays itself, for one player: the rules. No SDL; the deal comes from a seed (cards.h's
 * shuffle), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards face down in 13 piles of four: twelve round a clock face, one for each hour (the ace at one o'clock, 2 at
 * two... the jack at eleven, the queen at twelve), the kings' pile in the middle. Turn the top card of the middle pile
 * and put it face up under the pile of its hour (a king: under the middle pile); then turn the top card of that pile,
 * and so on. There is no choice to make: the game is pure chance. It is won when every card is face up before the
 * fourth king turns up (the fourth king is then the very last card); lost when the fourth king comes while cards are
 * still face down.
 * Piles: 0..12 the cards turned, one pile a rank (ace 0 .. king 12), 13..25 the face-down piles (ace 13 .. king 25).
 * The notation (script), moves separated by spaces: "s" the next card ("?" the same, the hint's move); "#n" a new
 * game, deal n (its seed).
 */
#ifndef CLOCK_H
#define CLOCK_H

#include "games/solitaire.h"

enum { CK_UP = 0, CK_DOWN = 13, CK_PILES = 26 };

extern const solrules ck_rules;

void ck_new(solrec *r, uint32_t seed);
int  ck_current(const solpos *p);                /* the rank whose face-down pile gives the next card (13: kings) */
int  ck_over(const solpos *p);                   /* no card left to turn there: won or lost */
int  ck_won(const solpos *p);                    /* every card face up */
int  ck_legal(const solpos *p, solmove m);
int  ck_play(solrec *r, solmove m);              /* 0: not legal */
solmove ck_next(const solpos *p);                /* the next move (SM_NONE: over) */
int  ck_script(solrec *r, const char *moves);    /* 0 at the first bad move */

#endif
