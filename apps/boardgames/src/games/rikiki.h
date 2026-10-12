/*
 * Rikiki (Oh Hell), for 3 to 6 players on one device (hidden hands): the rules. No SDL; the deal comes from a seed
 * (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 52 cards. The deals go up then down: 1 card each, 2, ... up to the most (13, or as many as the pack allows: 10
 * with five players, 8 with six), then back down to 1: 2 x max - 1 deals. After each deal the next card of the pack is
 * turned: its suit is trumps; when no card is left (four players, 13 each) the deal is played without trumps.
 * The dealer turns one seat on each deal; the player after the dealer bids first and leads the first trick. Each
 * player in turn bids how many tricks they will take, exactly (0 to the cards in hand); the dealer bids last and may
 * not make the bids add up to the number of cards each (somebody must fail: the French "dealer's rule").
 * Follow suit if you can; otherwise play anything (no obligation to trump). The highest trump wins, else the highest
 * card of the suit led (ace high); the winner leads the next trick.
 * The score: the bid made exactly scores 10 and 1 a trick taken; missed, minus 1 for each trick too many or too few.
 * After the last deal the highest total wins (a tie: shared).
 * Notation (script): b<n> bids, <card> plays one ("2c", "Qs", "Th"; tricks.h), n the next deal, "." one plain
 * action, "#" plain actions to the deal's end, "!" to the game's end.
 */
#ifndef RIKIKI_H
#define RIKIKI_H

#include <stdint.h>

#include "cards.h"

#define RIKIKI_MAXP 6
#define RIKIKI_HAND 13
#define RIKIKI_MAX_ACTS 4096
#define RIKIKI_NOBID 255

enum { RK_BID, RK_PLAY, RK_ROUND, RK_OVER };
/* an action: its kind (high byte) and a bid or a card */
enum { RKA_BID = 1 << 8, RKA_PLAY = 2 << 8, RKA_NEXT = 3 << 8 };
#define RKA_KIND(a) ((a) & 0xff00)
#define RKA_ARG(a) ((a) & 0xff)

typedef struct {
    uint8_t players, phase, turn, deal, deals, max;   /* deal: 0..deals-1; max: the most cards each */
    uint8_t size, dealer, first;                /* the cards each this deal; the dealer; who bids first and leads */
    uint8_t turned;                             /* the turned card (CARD_NONE: none, no trumps) */
    int8_t trump;                               /* its suit (-1: no trumps) */
    uint8_t hand[RIKIKI_MAXP][RIKIKI_HAND], n[RIKIKI_MAXP];
    uint8_t bid[RIKIKI_MAXP], won[RIKIKI_MAXP];  /* the bids (RIKIKI_NOBID: not yet), the tricks taken */
    uint8_t trick[RIKIKI_MAXP], ntrick, leader;
    uint8_t last[RIKIKI_MAXP], last_leader, last_winner, tricks;
    int16_t score[RIKIKI_MAXP], total[RIKIKI_MAXP];   /* the last deal's points; the totals */
    uint32_t seed;
} rikiki_pos;

typedef struct {
    rikiki_pos pos;
    uint32_t seed;
    uint8_t players;
    uint16_t act[RIKIKI_MAX_ACTS];
    int n;
} rikiki_game;

void rikiki_new(rikiki_game *g, int players, uint32_t seed);
int  rikiki_max(int players);                   /* the most cards each */
int  rikiki_size(int players, int deal);        /* the cards each in a deal */
int  rikiki_legal(const rikiki_pos *p, int act);
int  rikiki_play(rikiki_game *g, int act);      /* 0: not allowed */
int  rikiki_undo(rikiki_game *g);               /* back one action (a replay from the deal); 0: none */
int  rikiki_can_play(const rikiki_pos *p, int card);
int  rikiki_can_bid(const rikiki_pos *p, int bid);
int  rikiki_forbidden(const rikiki_pos *p);     /* the bid the dealer may not make now (-1: none) */
int  rikiki_bids(const rikiki_pos *p);          /* the bids so far, added */
int  rikiki_points(int bid, int won);           /* a deal's score */
unsigned rikiki_winners(const rikiki_pos *p);   /* RK_OVER: the highest total (bit n: player n; ties) */
int  rikiki_auto(const rikiki_pos *p);          /* a plain action (the "." of the notation) */
int  rikiki_script(rikiki_game *g, const char *moves);   /* 0: an action is bad */

#endif
