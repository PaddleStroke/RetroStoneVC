/*
 * Accordion, the patience of one long row, for one player: the rules. No SDL; the deal comes from a seed (cards.h's
 * shuffle), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards dealt face up in one row (read like a line of text on the screen), each a pile of its own. A pile goes on
 * the pile just left of it, or on the third pile to its left, when their top cards are of the same suit or the same
 * rank; the whole pile moves, and the piles on its right close up. The game ends when no pile can move: one pile
 * left wins; the fewer piles left, the better (1: three stars; 2 or 3: two; 4 to 6: one).
 * Piles: 0..51 in the row's order, the ones in play always first (the row closed up). A move: pile `from` onto pile
 * `to` (from - 1 or from - 3), n its cards (1 is taken for the whole pile too: a finger lifts its top card); the piles
 * after it closing up are moves of the game's own (SM_AUTO).
 * The notation (script): a pile's place from 1 then "a" (onto its neighbour) or "c" (onto the third to its left):
 * "12a 9c"; "?" the hint's move; "#n" a new game, deal n.
 */
#ifndef ACCORDION_H
#define ACCORDION_H

#include "games/sollevels.h"

enum { ACD_PILES = 52 };

extern const solrules acd_rules;
extern const solsearch acd_search;               /* for the levels' check: the rules and the moves worth trying */

void acd_new(solrec *r, uint32_t seed);
void acd_deal_cards(solrec *r, int variant, const uint8_t *deck);  /* the pack in the order dealt (seed 0) */
int  acd_legal(const solpos *p, solmove m);
int  acd_play(solrec *r, solmove m);             /* 0: not legal */
int  acd_piles(const solpos *p);                 /* the piles left */
int  acd_stuck(const solpos *p);                 /* no pile can move */
int  acd_won(const solpos *p);                   /* one pile left */
int  acd_stars(int piles);                       /* 3, 2, 1 or 0 for the piles left at the end */
solmove acd_hint(const solpos *p);               /* the first move of the best way the search sees; SM_NONE: stuck */
int  acd_script(solrec *r, const char *moves);   /* 0 at the first bad move */

/* the search: the moves that end with the fewest piles, from p (path: at most 52 moves), looking at `budget`
 * positions at most (*nodes: how many it did); the piles that way leaves (1: a win) */
int  acd_solve(const solpos *p, long budget, solmove *path, int *len, long *nodes);

#endif
