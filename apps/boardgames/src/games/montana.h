/*
 * Montana (also called Gaps), the patience of the four gaps, for one player: the rules. No SDL; the deal and the
 * redeals come from a seed (cards.h's shuffle, solitaire.h's), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards face up in four rows of 13; the four aces are taken out, which leaves four gaps. A gap is filled with the
 * card one rank higher than the card to its left, of the same suit (a 6 of clubs after the 5 of clubs); a gap in the
 * first column takes any 2; nothing fills a gap after a king. Arrange each row from 2 to king in one suit (the gap
 * then at its end). Three times in a game, the cards not yet in order (a row in order runs from a 2 in the first
 * column on) are gathered, shuffled and dealt again into the places after each row's run, leaving one gap right
 * after it. Won when the four rows are in order; over when no gap can be filled and no redeal is left. The deals are
 * random: not every one can be won.
 * Piles: 0..51 the places, row by row (row r, column c: 13r + c), 52 the hand (the cards gathered for a redeal; a tap
 * there redeals). The position's `redeals`: the redeals made.
 *
 * As solmoves: a card to a gap is a move of one card between two places; a redeal is SM_REDEAL from the hand to itself
 * (no card: the count), then the cards gathered on the hand, then dealt out by sol_deal_out (all SM_AUTO). The
 * notation (script), moves separated by spaces: a place is its row a..d and its column 1..13 ("b7"); "b7c1" the card of
 * b7 to the gap c1; "s" a redeal; "?" the hint's move; "#n" a new game, deal n (its seed).
 */
#ifndef MONTANA_H
#define MONTANA_H

#include "games/solitaire.h"

enum { MT_SLOT = 0, MT_HAND = 52, MT_PILES = 53 };
#define MT_REDEALS 3

extern const solrules mt_rules;

void mt_new(solrec *r, uint32_t seed);
int  mt_run(const solpos *p, int row);           /* the cards in order at the start of the row (2, 3, ... one suit) */
int  mt_card_for(const solpos *p, int gap);      /* the card that fills this gap (CARD_NONE: none; a 2: any) */
int  mt_redeals_left(const solpos *p);
int  mt_legal(const solpos *p, solmove m);
int  mt_play(solrec *r, solmove m);              /* 0: not legal */
int  mt_won(const solpos *p);
int  mt_stuck(const solpos *p);                  /* over: no move left */
solmove mt_fill(const solpos *p, int gap);       /* the move that fills a gap (SM_NONE: none) */
solmove mt_hint(const solpos *p);                /* SM_NONE: none */
int  mt_script(solrec *r, const char *moves);    /* 0 at the first bad move */

#endif
