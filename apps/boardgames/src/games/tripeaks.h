/*
 * TriPeaks, the patience of three peaks, for one player: the rules. No SDL; the deal comes from a seed (cards.h's
 * shuffle), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards: 28 in three peaks side by side (rows of 3, 6, 9 and 10 cards, each row lying over half of the one above),
 * only the bottom row face up; one card face up on the waste; the other 23 are the stock, face down. A free card
 * (no card lying over it any more) goes onto the waste if it is one rank higher or lower than the waste's top card,
 * whatever its suit; with TP_WRAP (the classic rule) an ace and a king follow each other too, round the corner. A
 * face-down card turns up as soon as it is free. When no card can go, the stock turns its top card onto the waste
 * (only once through). The streak: the cards played from the peaks in a row, since the stock's last card. Won when
 * the three peaks are cleared; lost when the stock is empty and no card can go. The deals are random: not every one
 * can be won.
 * Piles: 0..27 the peaks (tp_slots: the three tops, then row by row), 28 the stock, 29 the waste.
 *
 * The notation (script), moves separated by spaces: a card of the peaks to the waste "4c" (its row 1..4, its place
 * a..j in the row); "s" the stock; "?" the hint's move; "#n" a new game, deal n (its seed). The cards turning up are
 * SM_TURN moves, marked SM_AUTO.
 */
#ifndef TRIPEAKS_H
#define TRIPEAKS_H

#if defined(SOLITAIRE_H) && SOL_PILES < 32
#error "include tripeaks.h before solitaire.h: it needs more piles"
#endif
#ifndef SOL_PILES
#define SOL_PILES 32
#endif
#include "games/solitaire.h"

enum { TP_WRAP, TP_NOWRAP, TP_VARIANTS };
enum { TP_SLOT = 0, TP_STOCK = 28, TP_WASTE, TP_PILES };
#define TP_SLOTS 28

extern const solrules tp_rules;
extern const solslot tp_slots[TP_SLOTS];
extern const float tp_slot_x[TP_SLOTS];          /* where each card lies across, in cards' widths (the bottom
                                                 * row: 0..9) */

void tp_new(solrec *r, int variant, uint32_t seed);
int  tp_follows(const solpos *p, int card, int on);   /* card can go on `on`: one rank apart (or king, ace: wrap) */
int  tp_legal(const solpos *p, solmove m);
int  tp_play(solrec *r, solmove m);              /* 0: not legal */
int  tp_won(const solpos *p);
int  tp_stuck(const solpos *p);                  /* no move left */
int  tp_streak(const solrec *r);                 /* the cards played from the peaks since the stock's last card */
int  tp_best_streak(const solrec *r);            /* the longest streak of the game */
solmove tp_hint(const solpos *p);                /* SM_NONE: no move left */
int  tp_script(solrec *r, const char *moves);    /* 0 at the first bad move */

#endif
