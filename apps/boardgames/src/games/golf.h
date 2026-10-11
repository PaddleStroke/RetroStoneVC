/*
 * Golf, the card game, for 2 to 6 players on one device: the rules. No SDL; the deal comes from a seed (cards.h's
 * shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * GOLF_CLASSIC, six-card golf: the 52 cards (two packs shuffled together for 5 or 6 players), 2 rows of 3 face down
 * each. Nine holes (rounds); the first player moves on one seat each hole. Ace 1, 2..10 their value, jack and queen
 * 10, king 0; a column of two cards of the same rank scores 0.
 * GOLF_NUMBER, number golf: the number deck of 150 cards (-2 x5, -1 x10, 0 x15, 1..12 x10 each), 3 rows of 4 face
 * down. The player with the highest sum showing after the two cards turned up starts the round. A column of three
 * visible cards of the same value is discarded at once (and at the end of the round, when all are turned up). The
 * player who ended the round scores double if their score is above zero and not strictly the lowest. The game ends
 * after the round in which someone reaches 100.
 * Both: each player turns two cards up at the start of a round (in turn, from the first player). A turn: draw from
 * the stock or take the discard's top card; swap it for one of your cards (which goes face up on the discard), or,
 * drawn from the stock, discard it and turn one of your face-down cards up. Once a player has all their cards face
 * up, everybody else has one more turn; then every card is turned up and the round is scored. The lowest total wins.
 * Notation (script; the screenshots and the tests): r<slot> turn a card up at the start, s draw from the stock, t take
 * the discard, w<slot> swap, d discard the drawn card, f<slot> turn a card up after discarding, n the next round;
 * "#" plays plain moves (draw, then the first face-down card) to the end of the round. Slots: row by row from the top
 * left, 0..5 or 0..11.
 */
#ifndef GOLF_H
#define GOLF_H

#include <stdint.h>

#include "cards.h"

#define GOLF_MAXP 6
#define GOLF_SLOTS 12
#define GOLF_MAX_ACTS 8192
#define GOLF_HOLES 9                            /* classic: rounds in a game */
#define GOLF_LIMIT 100                          /* number golf: the game ends when a total reaches it */
#define GOLF_NONE 255

enum { GOLF_CLASSIC, GOLF_NUMBER, GOLF_VARIANTS };
enum { GP_REVEAL, GP_DRAW, GP_HOLD, GP_FLIP, GP_ROUND, GP_OVER };
/* an action: its kind (high bits) and a slot (low bits) */
enum { GA_REVEAL = 1 << 4, GA_STOCK = 2 << 4, GA_TAKE = 3 << 4, GA_SWAP = 4 << 4, GA_DISCARD = 5 << 4,
       GA_FLIP = 6 << 4, GA_NEXT = 7 << 4 };
#define GA_KIND(a) ((a) & 0xf0)
#define GA_SLOT(a) ((a) & 0x0f)

typedef struct {
    uint8_t variant, players, rows, cols, slots, deck;
    uint8_t phase, turn, held, taken;           /* taken: the held card came from the discard (it must be swapped) */
    uint8_t first, revealed, ender, round;      /* the round's first player; cards turned up so far in the reveal;
                                                 * who ended the round (GOLF_NONE) */
    uint8_t card[GOLF_MAXP][GOLF_SLOTS];
    uint16_t up[GOLF_MAXP], gone[GOLF_MAXP];    /* bit i: slot i face up; discarded (number golf's columns) */
    uint8_t stock[CARDS_MAX * 2], pile[CARDS_MAX * 2];   /* the stock, its top last; the discard, its top last */
    int16_t nstock, npile;
    int16_t total[GOLF_MAXP], last[GOLF_MAXP];  /* the totals; the last round's scores */
    uint8_t doubled;                            /* the last round's ender scored double */
    uint8_t reshuffles;
    uint32_t seed;
} gpos;

typedef struct {
    gpos pos;
    uint32_t seed;
    uint8_t variant, players;
    uint8_t act[GOLF_MAX_ACTS];
    int n;
} ggame;

void golf_new(ggame *g, int variant, int players, uint32_t seed);
int  golf_legal(const gpos *p, int act);
int  golf_play(ggame *g, int act);               /* 0: not allowed */
int  golf_undo(ggame *g);                        /* back one action (a replay from the deal); 0: none */
int  golf_value(const gpos *p, int card);        /* a card's points */
int  golf_score(const gpos *p, int player);      /* the player's cards that are face up, scored (columns too) */
int  golf_down(const gpos *p, int player);       /* how many of the player's cards are face down */
int  golf_winner(const gpos *p);                 /* GP_OVER: the lowest total (-1: a tie for it, or not over) */
int  golf_auto(const gpos *p);                   /* a plain action (the "#" of the notation) */
int  golf_script(ggame *g, const char *moves);   /* 0: an action is bad */

#endif
