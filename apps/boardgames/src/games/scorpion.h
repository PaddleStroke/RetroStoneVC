/*
 * Scorpion, the patience of suit runs, for one player: the rules. No SDL; the deal comes from a seed (cards.h's
 * shuffle), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards: seven columns of seven, the first four with three cards face down at the bottom, all the others face up;
 * the last three cards are the reserve. In the tableau ANY face-up card moves, with all the cards lying on it, onto
 * the top card of a column one rank higher of the same suit (the 6 of hearts on the 7 of hearts); only a king (with
 * what lies on it) fills an empty column. A face-down card left on top of a column turns up. A tap on the reserve deals
 * its three cards, one on each of the first three columns, at any time, once. A whole suit in order on top of a
 * column, from the king down to the ace, goes off by itself to a foundation; the four of them there win.
 * Piles: 0..6 the columns, 7..10 the foundations, 11 the reserve. The notation: solitaire.h's ("s" the reserve).
 */
#ifndef SCORPION_H
#define SCORPION_H

#include "games/sollevels.h"

enum { SCP_TAB = 0, SCP_FOUND = 7, SCP_STOCK = 11, SCP_PILES = 12 };

extern const solrules scp_rules;

void scp_new(solrec *r, uint32_t seed);
void scp_deal_cards(solrec *r, int variant, const uint8_t *deck);   /* the pack in the order dealt (seed 0) */
int  scp_legal(const solpos *p, solmove m);
int  scp_play(solrec *r, solmove m);             /* 0: not legal */
int  scp_won(const solpos *p);
solmove scp_hint(const solpos *p);               /* SM_NONE: nothing useful seen */
int  scp_script(solrec *r, const char *moves);   /* 0 at the first bad move */

/* the search (sollevels.h): the moves worth trying, best first; the search itself */
int  scp_cands(const solpos *p, solmove *list);
extern const solsearch scp_search;

#endif
