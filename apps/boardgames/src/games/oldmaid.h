/*
 * Old Maid (the French "pouilleux") for 2 to 6 players on one device: the rules. No SDL; the deal comes from a seed
 * (cards.h's shuffle), so a game is its seed and its draws.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 52 cards without the jack of clubs: the jack of spades has no partner (the "pouilleux"). A pair is two cards of
 * the same rank and the same colour (the two red sevens, the two black kings). All the cards are dealt, one at a time
 * from player 0; everyone lays their pairs down. Then, in turn from player 0, each player draws a card face down from
 * the hand of the player before them (the previous one still holding cards), whose hand is shuffled first; a pair
 * made goes down at once. A player with no cards left is out: they have won. The last one holding a card, the jack
 * of spades, loses; everyone else wins.
 * Notation (script; the screenshots and the tests): d<i> draw the i-th card of the offered hand (its order as
 * shuffled); # plain draws (the first card) to the end.
 */
#ifndef OLDMAID_H
#define OLDMAID_H

#include <stdint.h>

#include "cards.h"

#define OM_MAXP 6
#define OM_CARDS 51
#define OM_ODD CARD(SUIT_SPADES, RANK_JACK)
#define OM_MAX_ACTS 2048
#define OM_NONE 255

enum { OP_DRAW, OP_OVER };

typedef struct {
    uint8_t players, phase, turn, loser;
    uint8_t hand[OM_MAXP][OM_CARDS], n[OM_MAXP];   /* each hand in its own order (shuffled when offered) */
    uint8_t pile[OM_CARDS];                     /* the pairs laid down, in order */
    int16_t npile;
    uint16_t draws;
    uint32_t seed;
} opos;

typedef struct {
    opos pos;
    uint32_t seed;
    uint8_t players;
    uint8_t act[OM_MAX_ACTS];
    int n;
} ogame;

void om_new(ogame *g, int players, uint32_t seed);
int  om_offerer(const opos *p);                 /* the player the one to move draws from (-1: over) */
int  om_pairs(int a, int b);                    /* the same rank and colour */
int  om_play(ogame *g, int index);              /* draw the offerer's index-th card; 0: not allowed */
int  om_undo(ogame *g);                         /* back one draw (a replay from the deal); 0: none */
unsigned om_winners(const opos *p);             /* OP_OVER: everyone but the loser (bit n: player n) */
int  om_script(ogame *g, const char *moves);    /* 0: a draw is bad */

/* the bot (oldmaid_bot.c, bots.h), and what the screen (oldmaid.c) gives it; a headless test defines both */
const ogame *oldmaid_live(void);                /* the game on the screen, read-only: the bot copies it at entry */
int oldmaid_show(const char *move);             /* main thread: "d3" draws as a tap does; 0: not legal */

#endif
