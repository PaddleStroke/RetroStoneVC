/*
 * Canasta, for 4 players in two partnerships (or 2 players, each alone) on one device: the rules. No SDL; the deal
 * comes from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The cards: two 52-card packs and four jokers (108). Jokers and 2s are wild. Card values: joker 50, 2 and ace 20,
 * king to 8 10, 7 to 4 5, a black 3 5. Partners sit across: players 0 and 2 against 1 and 3 (a side is player & 1;
 * with two players a side is a player). A side's melds are on the table for both partners.
 * The deal: 11 cards each (2 players: 15). The next card starts the discard pile; a wild card or a red 3 turned up is
 * covered by the next card, and the pile is frozen. The first dealer is the last player: player 0 starts; the deal
 * moves on one seat a hand.
 * Red 3s are bonus cards: a red 3 dealt or drawn is laid down at once beside the side's melds, and another card is
 * drawn instead (dealt ones before play starts); one taken in the discard pile is laid down with no replacement. Each
 * is worth 100, all four 800; against the side if it has made no meld when the hand ends.
 * A turn: draw the top card of the stock (2 players: two cards) or take the whole discard pile; meld if you like;
 * discard one card (that ends the turn).
 * Melds: three or more cards of one rank (aces, 4s to kings), at least two of them natural and at most three wild. A
 * side has one meld of a rank: cards of that rank go on it. A canasta is a meld of seven or more cards: natural (no
 * wild card) 500, mixed 300. Black 3s may be melded only by a player going out (three or four, no wild card).
 * The initial meld: a side's first melds, laid in one turn, must be worth (their cards' values) 50 at least while the
 * side's score is 0 to 1495, 90 from 1500, 120 from 3000 (15 while it is below zero). A player whose side has not
 * melded yet may lay the melds of the initial meld before drawing (to show them before taking the pile, the pile's
 * other cards never count); once they have drawn from the stock, the count is checked when they discard.
 * Taking the discard pile: its top card must go into a meld at once (with cards from the hand, or on a meld of the
 * side), then the rest of the pile goes to the hand. Never when its top card is a black 3 (it blocks the pile) or a
 * wild card. The pile is frozen while a wild card is in it (a wild card discarded freezes it, laid crosswise), and
 * for a side that has not melded yet: it is then taken only with a natural pair of the top card's rank from the hand.
 * When the side has not melded yet, the top card with the cards of its meld (and the melds laid before drawing) must
 * reach the initial meld's count.
 * Going out: getting rid of the last card (melding them all, or melding all but one and discarding it), only once
 * the side has a canasta (2 players: two canastas); a player may not meld their hand down to fewer than two cards
 * otherwise. Going out scores 100; 200 going out concealed (melding the whole hand in one turn, having melded nothing
 * before this hand and adding nothing to the side's earlier melds, without taking the pile); then the initial meld's
 * count is not asked.
 * The stock empty: a player who can take the discard pile by putting its top card on a meld of their side must take
 * it; one who cannot (or will not, when not forced) ends the hand. The last card drawn being a red 3 ends the hand too.
 * The score of a hand, each side: the values of its melded cards, the canastas' bonuses, the red 3s, going out; less
 * the values of the cards left in its players' hands. The game ends after the hand in which a side reaches 5000: the
 * highest total wins (a tie: shared).
 * Notation (script; the screenshots and the tests): s draw from the stock (or end the hand, the stock empty), t take
 * the pile ("t" alone: on the side's meld; "tKh.Kd": with these cards), m meld ("mKh.Kd.Ks"; "m8:2c.X": on the 8s),
 * d discard ("d7c"), n the next hand; cards: rank (A 2..9 T J Q K) and suit (s h d c), X a joker; a one plain
 * action, "#" plain actions to the end of the hand, "*" to the end of the game.
 */
#ifndef CANASTA_H
#define CANASTA_H

#include <stdint.h>

#include "cards.h"

#define CA_CARDS 108
#define CA_MAXP 4
#define CA_GOAL 5000
#define CA_RANKS 14                             /* melds by rank: 1 (aces), 3 (black 3s), 4..13 */
#define CA_MELD 16                              /* the most cards in a meld (eight naturals and three wild cards) */
#define CA_ACT 16                               /* the most cards in an action */
#define CA_MAX_ACTS 16384
#define CA_NONE 255

enum { CP_DRAW, CP_MELD, CP_HAND, CP_OVER };    /* the turn before / after drawing; a hand's end; the game's */
enum { CA_STOCK = 1, CA_TAKE, CA_MELD_IT, CA_DISCARD, CA_NEXT };
typedef struct { uint8_t kind, rank, n, card[CA_ACT]; } ca_act;   /* rank: the meld (0: the cards' own) */

typedef struct {                                /* a side's score for a hand */
    int16_t melds, bonus, red3, out, minus, total;
    uint8_t natural, mixed;                     /* its canastas */
} ca_score;

typedef struct {
    uint8_t players, dealer, turn, phase;
    uint8_t hand[CA_MAXP][CA_CARDS], n[CA_MAXP];        /* in the order they came */
    uint8_t stock[CA_CARDS], nstock;                    /* the top last */
    uint8_t pile[CA_CARDS], npile;                      /* the top last */
    uint8_t frozen, freezer;                            /* frozen for all; the card that froze it (CA_NONE) */
    uint8_t meld[2][CA_RANKS][CA_MELD], mn[2][CA_RANKS];   /* each side's melds, the cards in the order laid */
    uint8_t mn0[2][CA_RANKS];                           /* the melds at the turn's start */
    uint8_t red3[2][4], nred3[2];
    uint8_t opened[2];                                  /* the side's initial meld made (in an earlier turn) */
    uint8_t melded[CA_MAXP];                            /* the player has melded in this hand (earlier turns) */
    uint8_t took, touched, now;                         /* this turn: took the pile; added to an earlier meld; melded */
    uint8_t out, concealed;                             /* the hand's end: who went out (CA_NONE) */
    uint8_t drew[2], ndrew;                             /* this turn's cards from the stock (kept, not red 3s) */
    int16_t score[2];
    ca_score res[2];                                    /* the last hand */
    uint16_t hands;
    uint32_t seed;
} capos;

typedef struct {
    capos pos;
    uint32_t seed;
    uint8_t players;
    ca_act act[CA_MAX_ACTS];
    int n;
} cagame;

int  ca_wild(int card);
int  ca_red3(int card);
int  ca_black3(int card);
int  ca_value(int card);                        /* joker 50, 2 and ace 20, K..8 10, 7..4 5, black 3 5 */
int  ca_side(const capos *p, int player);
int  ca_need(const capos *p, int side);         /* the initial meld's count (0: made) */
int  ca_meld_points(const capos *p, int side);  /* its melds' card values */
int  ca_canastas(const capos *p, int side);
int  ca_can_out(const capos *p, int side);      /* enough canastas to go out */
int  ca_frozen_for(const capos *p, int side);   /* the pile frozen for this side */
int  ca_forced(const capos *p);                 /* the stock empty and the top card fits a meld: take it */
/* a meld's rank for cards put on it (rank 0: their own; -1: none fits) */
int  ca_meld_rank(const uint8_t *cards, int n, int rank);
int  ca_legal(const capos *p, const ca_act *a);
void ca_new(cagame *g, int players, uint32_t seed);
int  ca_play(cagame *g, const ca_act *a);       /* 0: not allowed */
int  ca_undo(cagame *g);                        /* back one action (a replay from the deal); 0: none */
int  ca_winner(const capos *p);                 /* CP_OVER: the winning side (-1: a tie, or not over) */
unsigned ca_winners(const capos *p);            /* CP_OVER: the players who share the win */
int  ca_auto(const capos *p, ca_act *a);        /* a plain action (the "a" of the notation); 0: none */
int  ca_card_name(const char *s, int *card);    /* "Kh", "X": the card; the letters read (0: bad) */
int  ca_script(cagame *g, const char *moves);   /* 0: an action is bad */

#endif
