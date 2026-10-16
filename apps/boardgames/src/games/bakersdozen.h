/*
 * Baker's Dozen, the patience of thirteen columns, for one player: the rules. No SDL; the deal comes from a seed
 * (cards.h's shuffle), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards, all dealt face up: thirteen columns of four, each column's kings moved under its other cards at the deal
 * (the order of the others kept). Build the four foundations up by suit from the ace to the king. Only a column's top
 * card moves, one at a time: onto the top card of another column one rank higher, whatever its suit (a 6 on any 7),
 * or up to its foundation. An empty column stays empty. A card on a foundation stays there. Once the cards left can
 * all go up in turn, they go up by themselves.
 * Piles: 0..12 the columns, 13..16 the foundations. The notation: solitaire.h's, the columns 1..9, 0 for the tenth, then
 * x, y, z for the last three.
 */
#ifndef BAKERSDOZEN_H
#define BAKERSDOZEN_H

#include "games/sollevels.h"

enum { BKD_TAB = 0, BKD_FOUND = 13, BKD_PILES = 17 };

extern const solrules bkd_rules;

void bkd_new(solrec *r, uint32_t seed);
void bkd_deal_cards(solrec *r, int variant, const uint8_t *deck);   /* the pack in the order dealt (seed 0) */
int  bkd_legal(const solpos *p, solmove m);
int  bkd_play(solrec *r, solmove m);             /* 0: not legal */
int  bkd_won(const solpos *p);
solmove bkd_hint(const solpos *p);               /* SM_NONE: nothing useful seen */
int  bkd_script(solrec *r, const char *moves);   /* 0 at the first bad move */

/* the search (sollevels.h): the moves worth trying, best first; the search itself */
int  bkd_cands(const solpos *p, solmove *list);
extern const solsearch bkd_search;

#endif
