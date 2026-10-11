/*
 * Spider, the patience of two packs, for one player: the rules. No SDL; the deal comes from a seed (cards.h's
 * shuffle), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 104 cards: eight runs of ace to king, all spades (SP_1SUIT), four spades and four hearts (SP_2SUITS), or two whole
 * packs (SP_4SUITS). Ten columns: 6 cards in the first four, 5 in the others, the top one face up; the other 50 are
 * the stock, five rows of ten. In the tableau a card goes on any card a rank higher, whatever its suit, and any card
 * on an empty column; only a run of one suit moves together. A face-down card left on top of a column turns up. The
 * stock deals one card face up onto every column, as long as no column is empty (the classic rule). A whole run of
 * one suit from king to ace on top of a column goes to a foundation by itself; the game is won when all eight have.
 * Piles: 0..9 the columns, 10..17 the foundations (a run each), 18 the stock.
 */
#ifndef SPIDER_H
#define SPIDER_H

#include "games/solitaire.h"

enum { SP_1SUIT, SP_2SUITS, SP_4SUITS, SP_VARIANTS };
enum { SPI_TAB = 0, SPI_FOUND = 10, SPI_STOCK = 18, SPI_PILES = 19 };

extern const solrules sp_rules;

void sp_new(solrec *r, int variant, uint32_t seed);
int  sp_legal(const solpos *p, solmove m);
int  sp_play(solrec *r, solmove m);              /* 0: not legal */
int  sp_won(const solpos *p);
solmove sp_hint(const solpos *p);
int  sp_script(solrec *r, const char *moves);

#endif
