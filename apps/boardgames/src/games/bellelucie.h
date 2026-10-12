/*
 * La Belle Lucie, the patience of eighteen fans, for one player: the rules. No SDL; the deal and the redeals come from
 * a seed (cards.h's shuffle, solitaire.h's), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards face up in 18 fans of three (the last one a single card). Build the four foundations up by suit from the
 * ace to the king. Only the top card of a fan moves: onto a foundation, or onto the top card of another fan that is
 * one rank higher of the same suit. A fan left empty stays empty. Twice in a game, the cards of the fans may be
 * gathered, shuffled and dealt again in fans of three. On the last deal (after the second redeal) comes the merci:
 * once, one card buried in a fan may be drawn out (the cards over it close up); it waits on its own place until it is
 * played onto a foundation or a fan. Won when all the cards are on the foundations; over when no move is left. Once
 * the cards left can all go up in turn, they go up by themselves.
 * Piles: 0..17 the fans, 18..21 the foundations, 22 the merci's place, 23 the hand (the cards gathered for a redeal; a
 * tap there redeals). The position's `redeals`: the redeals made, and one more once the merci has been used.
 *
 * As solmoves: a redeal is SM_REDEAL from the hand to itself (no card: the count), then the fans gathered on the
 * hand, then dealt out by sol_deal_out (all SM_AUTO); the merci, asked for as the move of the n cards from the
 * buried one up to the merci's place, is SM_REDEAL again, the cards over it lifted onto the hand, the card to its
 * place, the others back. The notation (script), moves separated by spaces: the fans a..r, the merci's place x;
 * "ab" the top card of fan a onto fan b, "ah" to its foundation, "xh", "xb"; "!c1" the merci on the card at index 1
 * (from 0, the bottom) of fan c; "s" a redeal; "?" the hint's move; "#n" a new game, deal n (its seed).
 */
#ifndef BELLELUCIE_H
#define BELLELUCIE_H

#include "games/solitaire.h"

enum { LB_FAN = 0, LB_FOUND = 18, LB_MERCI = 22, LB_HAND = 23, LB_PILES = 24 };
#define LB_FANS 18
#define LB_REDEALS 2

extern const solrules lb_rules;

void lb_new(solrec *r, uint32_t seed);
int  lb_redeals_left(const solpos *p);
int  lb_merci_open(const solpos *p);             /* the merci can be used now */
int  lb_legal(const solpos *p, solmove m);
int  lb_play(solrec *r, solmove m);              /* 0: not legal */
int  lb_won(const solpos *p);
int  lb_stuck(const solpos *p);                  /* over: no move left */
solmove lb_hint(const solpos *p);                /* SM_NONE: none */
int  lb_script(solrec *r, const char *moves);    /* 0 at the first bad move */

#endif
