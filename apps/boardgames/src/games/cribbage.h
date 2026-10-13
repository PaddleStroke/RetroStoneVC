/*
 * Cribbage, for 2 (or 3) players on one device: the rules. No SDL; the deal comes from a seed (cards.h's shuffle),
 * so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 52 cards; ace 1, 2..10 their value, jack, queen and king 10 (runs go by rank, the ace low). Each hand: the
 * dealer deals 6 cards each (3 players: 5 each, and one from the pack to the crib); each player lays away 2 (3
 * players: 1) to the crib, the dealer's extra hand, from the dealer's left round to the dealer. The player on the
 * dealer's left cuts the starter (a jack: his heels, 2 to the dealer) and leads the play.
 * The play: in turn, each lays a card and says the count (the cards' total, never past 31): 15 scores 2, 31 scores
 * 2, a pair 2 (three of a kind 6, four 12), a run of 3 or more among the last cards 1 a card. A player who cannot
 * lay a card says "go" (here at once, by itself); when nobody can, the last to lay one scores 1 (the go, or the last
 * card of all) unless the count is 31; the count starts again from the next player who still has cards.
 * The show, from the dealer's left round to the dealer, then the dealer's crib, each with the starter: every
 * combination of cards making 15 scores 2, each pair 2, each run of 3 or more 1 a card, four cards of the hand of
 * one suit 4 (5 with the starter; the crib only counts the five), the jack of the starter's suit 1 (his nobs).
 * The first to 121 wins at once, whenever it is reached (the count stops there). The deal passes to the left.
 * The first dealer is the last player, so player 0 cuts and leads the first hand.
 * Notation (script; the screenshots and the tests): d<slots> lay these slots of the hand away (d01, d5), c cut,
 * p<slot> play a card of the four kept (sorted by rank), n go on (the next count of the show, the next deal);
 * "#" plays plain moves until the show starts (in the show: to the next deal).
 */
#ifndef CRIBBAGE_H
#define CRIBBAGE_H

#include <stdint.h>

#include "cards.h"

#define CRIB_MAXP 3
#define CRIB_GOAL 121
#define CRIB_MAX_ACTS 4096
#define CRIB_MAX_EV 40
#define CRIB_NONE 255

enum { CP_DISCARD, CP_CUT, CP_PLAY, CP_SHOW, CP_OVER };
/* an action: 1..63 lays the hand's slots (bit i: slot i) away; CA_PLAY + slot; CA_CUT; CA_NEXT */
#define CA_PLAY 64
#define CA_CUT 128
#define CA_NEXT 129

/* what an action scored, in order: the kind, who, the points; mask: the cards (the play: how many of the last cards
 * of the count; the show: bit i of the five counted, the four of the hand or crib then the starter, bit 4) */
enum { CE_HEELS, CE_FIFTEEN, CE_THIRTYONE, CE_PAIR, CE_RUN, CE_GO, CE_LAST, CE_FLUSH, CE_NOBS };
enum { CS_PLAY, CS_CUT, CS_SHOW };              /* where an event comes from */
/* count: the show's count it belongs to; leap: the leap of a peg that pegs it */
typedef struct { uint8_t pl, kind, pts, mask, src, count, leap; } crib_ev;
/* a peg leaping: the back peg of a player from behind `from` (the front peg) to `to` */
typedef struct { uint8_t pl; int16_t from, to; } crib_leap;
#define CRIB_MAX_LEAPS 6

typedef struct {
    uint8_t players, ndeal, dealer, phase, turn;
    uint8_t hand[CRIB_MAXP][6], nhand[CRIB_MAXP];   /* the cards dealt (sorted), before laying away */
    uint8_t kept[CRIB_MAXP][4];                 /* the four kept for the play and the show (sorted) */
    uint8_t used[CRIB_MAXP];                    /* bit i: kept[i] played */
    uint8_t crib[4], ncrib;
    uint8_t stock[52], nstock;                  /* the pack left, its top last */
    uint8_t starter;                            /* CARD_NONE until cut */
    uint8_t seq[16], by[16], nseq;              /* the current count's cards in order, and who laid each */
    uint8_t count, last;                        /* the count; the last to lay a card (CRIB_NONE) */
    uint8_t old;                                /* cards of the earlier counts of this hand (turned down) */
    uint8_t shown;                              /* the show: counts scored (the hands in order, then the crib) */
    int16_t score[CRIB_MAXP], back[CRIB_MAXP];  /* the front peg, the back peg (the score before the last points) */
    uint8_t nev;
    crib_ev ev[CRIB_MAX_EV];                    /* what the last action scored */
    uint8_t nleap;
    crib_leap leap[CRIB_MAX_LEAPS];             /* the pegs it moved, in order */
    uint16_t hands;                             /* hands dealt so far */
    uint32_t seed;
} cpos;

typedef struct {
    cpos pos;
    uint32_t seed;
    uint8_t players;
    uint8_t act[CRIB_MAX_ACTS];
    int n;
} cgame;

int  crib_value(int card);                      /* ace 1 .. 10, courts 10 */
void crib_new(cgame *g, int players, uint32_t seed);
int  crib_legal(const cpos *p, int act);
int  crib_play(cgame *g, int act);              /* 0: not allowed */
int  crib_undo(cgame *g);                       /* back one action (a replay from the deal); 0: none */
int  crib_away(const cpos *p);                  /* how many cards each lays away (2, or 1 for three players) */
int  crib_can_play(const cpos *p, int pl);      /* 1: the player has a card that fits under 31 */
int  crib_fits(const cpos *p, int pl, int slot);   /* 1: kept[slot] is in hand and fits */
/* 1: the player said "go" in this count: the turn went past them since the last card (cards in hand, none fits) */
int  crib_said_go(const cpos *p, int pl);
/* the show's items of four cards and the starter (crib: the crib's flush rule), in counting order: fifteens, pairs,
 * runs, the flush, his nobs; their total */
int  crib_show(const uint8_t *four, int starter, int is_crib, crib_ev *items, int *n);
/* the count of the show k (0..players-1 the hands from the dealer's left, players: the crib): whose, its cards */
int  crib_show_of(const cpos *p, int k, const uint8_t **cards);
int  crib_winner(const cpos *p);                /* CP_OVER: who reached 121 (-1: not over) */
int  crib_auto(const cpos *p);                  /* a plain action (the "#" of the notation) */
int  crib_script(cgame *g, const char *moves);  /* 0: an action is bad */

/* the screen's game (cribbage.c), for its bot (cribbage_bot.c) */
const cgame *cribbage_live(void);               /* read-only: the bot copies it at entry */
int cribbage_show(const char *move);            /* main thread: one action of the notation, as a tap plays it */

#endif
