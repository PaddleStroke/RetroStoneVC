/*
 * Golf, the patience (not the card game for several players, golf.h), for one player: the rules. No SDL; the deal
 * comes from a seed (cards.h's shuffle), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards: seven columns of five, all face up; one card face up on the waste; the other 16 are the stock. The top
 * card of a column goes onto the waste if it is one rank higher or lower than the waste's top card, whatever its suit.
 * PG_NOWRAP (the classic rule): an ace and a king do not follow each other (only a queen goes on a king, only a two
 * on an ace); PG_WRAP: they do, round the corner. When no card can go, the stock turns its top card onto the waste
 * (only once through). Won when the columns are cleared; over when the stock is empty and no card can go.
 * The score, as in golf the fewer the better: the cards left in the columns; once they are all cleared, minus the
 * cards still in the stock. The deals are random: not every one can be won.
 * Piles: 0..6 the columns, 7 the stock, 8 the waste.
 *
 * The notation (script), moves separated by spaces: a column's top card to the waste "3" (the columns 1..7); "s" the
 * stock; "?" the hint's move; "#n" a new game, deal n (its seed).
 */
#ifndef GOLFPATIENCE_H
#define GOLFPATIENCE_H

#include "games/sollevels.h"

enum { PG_NOWRAP, PG_WRAP, PG_VARIANTS };
enum { PG_TAB = 0, PG_STOCK = 7, PG_WASTE = 8, PG_PILES = 9 };

extern const solrules pg_rules;

void pg_new(solrec *r, int variant, uint32_t seed);
void pg_deal_cards(solrec *r, int variant, const uint8_t *deck);   /* the pack in the order dealt (seed 0) */
int  pg_follows(const solpos *p, int card, int on);   /* card can go on `on`: one rank apart (wrap: king, ace too) */
int  pg_legal(const solpos *p, solmove m);
int  pg_play(solrec *r, solmove m);              /* 0: not legal */
int  pg_won(const solpos *p);
int  pg_stuck(const solpos *p);                  /* over: no move left */
int  pg_score(const solpos *p);                  /* the cards left in the columns, or minus the stock's once won */
solmove pg_hint(const solpos *p);                /* SM_NONE: no move left */
int  pg_script(solrec *r, const char *moves);    /* 0 at the first bad move */


/* the search (sollevels.h): the moves worth trying, best first; the search itself */
int  pg_cands(const solpos *p, solmove *list);
extern const solsearch pg_search;

#endif
