/*
 * Gin Rummy, for 2 players on one device: the rules. No SDL; the deal comes from a seed (cards.h's shuffle), so a
 * game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 52 cards, 10 each; ace 1 (always low), 2..10 their value, jack, queen and king 10. Melds: three or four cards
 * of one rank (a set), or three or more of one suit in sequence (a run, the ace below the 2 only). Deadwood: the cards
 * in no meld, counted at their value (the hand is arranged at its best by itself).
 * The first upcard: the non-dealer may take it, else the dealer; if both pass, the non-dealer draws from the stock.
 * A turn: draw the top card of the stock or of the discard pile, then discard one (not the one just taken from the
 * discard pile). A player whose deadwood is 10 or less once they have discarded may knock as they discard (here the
 * discard goes on the pile face up): the hand ends. Gin: no deadwood at all.
 * The end of a hand: the knocker shows their melds; the defender shows theirs and lays off deadwood cards on the
 * knocker's melds (not after gin). The knocker scores the difference of the deadwood; if the defender has as little
 * or less (an undercut), the defender scores the difference and 25. Gin scores 25 and the defender's deadwood. When
 * only two cards are left in the stock after a discard, the hand is dead: nobody scores, the same dealer deals again.
 * The deal alternates; the first dealer is player 1 (player 0 is offered the first upcard).
 * The game: to 100. Then the winner adds 100 (the game bonus) and each player 25 for every hand they won (the box or
 * line bonus); if the loser won no hand at all (a shutout), the winner's total is doubled.
 * Notation (script; the screenshots and the tests): s draw from the stock, t take the discard, p pass (the first
 * upcard), d<card> discard, k<card> knock, n the next hand; a card is its rank (A 2..9 T J Q K) and suit (s h d c):
 * "d7h", "kQs"; "#" plays plain moves to the end of the hand.
 */
#ifndef GINRUMMY_H
#define GINRUMMY_H

#include <stdint.h>

#include "cards.h"

#define GIN_GOAL 100
#define GIN_GAME_BONUS 100
#define GIN_BOX 25
#define GIN_BONUS 25                            /* gin, and an undercut */
#define GIN_KNOCK 10                            /* the most deadwood to knock with */
#define GIN_MAX_ACTS 4096

enum { GN_OFFER, GN_DRAW, GN_DISCARD, GN_END, GN_OVER };
/* an action: its kind (high byte) and a card (low byte) */
enum { GNA_STOCK = 1 << 8, GNA_TAKE = 2 << 8, GNA_PASS = 3 << 8, GNA_NEXT = 4 << 8, GNA_DISCARD = 5 << 8,
       GNA_KNOCK = 6 << 8 };
#define GNA_KIND(a) ((a) & 0xff00)
#define GNA_CARD(a) ((a) & 0xff)

/* a hand arranged: its melds one after the other (mlen each), then its deadwood; deadwood points */
typedef struct {
    uint8_t card[11], n;                        /* the melds' cards, then the deadwood's */
    uint8_t nm, mlen[4];
    uint8_t dead;
} gin_arr;

enum { GR_NONE, GR_KNOCK, GR_GIN, GR_UNDERCUT, GR_DEAD };
typedef struct {
    uint8_t kind, knocker, winner;
    int16_t points, bonus;                      /* to the winner (the bonus is in the points) */
    gin_arr arr[2];                             /* each player's hand arranged (the defender's: before layoffs) */
    uint8_t nlay, lay[11], layto[11];           /* the defender's cards laid off, on which of the knocker's melds */
    uint8_t dead[2];                            /* the deadwood counted (the defender's after the layoffs) */
} gin_result;

typedef struct {
    uint8_t dealer, phase, turn;
    uint8_t hand[2][11], n[2];
    uint8_t stock[52], nstock;                  /* the top last */
    uint8_t pile[52], npile;
    uint8_t taken;                              /* taken from the discard pile this turn (cannot go back) */
    uint8_t passes;                             /* the first upcard refused (2: by both) */
    int16_t score[2], final[2];                 /* the points; at the end, with the bonuses */
    uint8_t boxes[2], shutout;
    uint16_t hands;
    gin_result res;                             /* the last hand's end */
    uint32_t seed;
} gnpos;

typedef struct {
    gnpos pos;
    uint32_t seed;
    uint16_t act[GIN_MAX_ACTS];
    int n;
} gngame;

int  gin_value(int card);                       /* ace 1 .. 10, courts 10 */
/* the best arrangement of n cards (the least deadwood); its deadwood */
int  gin_arrange(const uint8_t *cards, int n, gin_arr *out);
/* the deadwood left with n cards less `card` (n - 1 cards), at best */
int  gin_without(const uint8_t *cards, int n, int card);
void gin_new(gngame *g, uint32_t seed);
int  gin_legal(const gnpos *p, int act);
int  gin_can_knock(const gnpos *p);             /* the player to move may knock with some discard */
int  gin_play(gngame *g, int act);              /* 0: not allowed */
int  gin_undo(gngame *g);                       /* back one action (a replay from the deal); 0: none */
int  gin_winner(const gnpos *p);                /* GN_OVER: the winner (-1: not over) */
int  gin_auto(const gnpos *p);                  /* a plain action (the "#" of the notation): the best discard,
                                                 * a knock as soon as it can */
int  gin_card_name(const char *s, int *card);   /* "7h": the card; the letters read (0: bad) */
int  gin_script(gngame *g, const char *moves);  /* 0: an action is bad */

#endif
