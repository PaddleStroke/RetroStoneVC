/*
 * FreeCell, the patience of free cells, for one player: the rules. No SDL; the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The numbered deals: deal n is the one the well-known public generator gives (a linear congruential generator
 * seeded with n: x = x * 214013 + 2531011 mod 2^31, each number x >> 16; the cards ace to king, clubs, diamonds,
 * hearts, spades within a rank; dealt row by row onto eight columns), so deal 1 here is deal 1 everywhere. The deals
 * go from 1 to 32000; every one of them can be solved but 11982.
 * The 52 cards face up in eight columns (7, 7, 7, 7, 6, 6, 6, 6). Build the four foundations up by suit from the
 * ace. In the tableau a card goes on one a rank higher of the other colour; each of the four free cells holds one
 * card; any card may go to an empty column. Runs move as if a card at a time through the free cells and the empty
 * columns: at most (free cells + 1) x 2^(empty columns), not counting the column the run goes to. A card on a
 * foundation stays there. After each move the cards no longer needed in the tableau go up by themselves (aces and
 * twos; a higher card once both foundations of the other colour are at least one rank below it).
 * Piles: 0..7 the columns, 8..11 the free cells, 12..15 the foundations.
 * The notation: solitaire.h's ("#n": deal n).
 */
#ifndef FREECELL_H
#define FREECELL_H

#include "games/sollevels.h"

enum { FC_TAB = 0, FC_CELL = 8, FC_FOUND = 12, FC_PILES = 16 };
#define FC_DEALS 32000

extern const solrules fc_rules;

void fc_deal(int deal, uint8_t out[52]);         /* the deal's cards in the order dealt (column i % 8) */
void fc_new(solrec *r, int deal);
void fc_deal_cards(solrec *r, int variant, const uint8_t *deck);   /* the pack in the order dealt (52; deal 0) */
int  fc_legal(const solpos *p, solmove m);
int  fc_play(solrec *r, solmove m);              /* 0: not legal */
int  fc_won(const solpos *p);
int  fc_most(const solpos *p, int to);           /* the most cards a run can move to pile `to` */
solmove fc_hint(const solpos *p);
int  fc_script(solrec *r, const char *moves);

/* the search (sollevels.h): the moves worth trying, best first; the search itself */
int  fc_cands(const solpos *p, solmove *list);
extern const solsearch fc_search;

#endif
