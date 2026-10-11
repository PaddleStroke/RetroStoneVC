/*
 * War (la bataille) for 2 to 4 players on one device: the rules. No SDL; the deal and the order of the cards won come
 * from a seed (cards.h's shuffle), so a game is its seed and its taps.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 52 cards are dealt out (three players: 18, 17, 17). Each player turns the top card of their pile; the highest
 * (ace high, then king... 2) takes all the cards turned, under their pile (in no set order). A tie for the highest is
 * a war (la bataille): the tied players each put a card face down, then one face up; the highest of those takes
 * everything, or it is a war again. A player with one card left in a war turns it face up (no card face down); a tied
 * player with none left is out of the war (the last one in takes it all). A player whose pile is empty is out. The
 * game ends when one player has every card. WAR_SHORT: it ends after 26 battles too; the most cards win (a tie: they
 * share the win).
 * Notation (script; the screenshots and the tests): a digit taps that player's pile; b taps the piles to the end of
 * the battle (the war too); # to the end of the game.
 */
#ifndef WAR_H
#define WAR_H

#include <stdint.h>

#include "cards.h"

#define WAR_MAXP 4
#define WAR_SHORT_BATTLES 26
#define WAR_MAX_ACTS 30000
#define WAR_NONE 255

enum { WAR_FULL, WAR_SHORT, WAR_VARIANTS };
enum { WP_TURN, WP_OVER };

typedef struct {
    uint8_t variant, players, phase;
    uint8_t pile[WAR_MAXP][52], n[WAR_MAXP];    /* each pile, its top last */
    uint8_t table[WAR_MAXP][52], nt[WAR_MAXP];  /* the cards turned in this battle, in order */
    uint64_t up[WAR_MAXP];                      /* bit i: table card i face up */
    uint8_t in, done;                           /* bit q: player q plays this step; has played it */
    uint8_t war;                                /* a war: the next card face down, then one face up */
    uint8_t winner;                             /* the last battle's winner (WAR_NONE: none yet) */
    uint16_t battles;
    uint32_t seed;
} wpos;

typedef struct {
    wpos pos;
    uint32_t seed;
    uint8_t variant, players;
    uint8_t act[WAR_MAX_ACTS];
    int n;
} wgame;

void war_new(wgame *g, int variant, int players, uint32_t seed);
int  war_value(int card);                       /* 2..14: ace high */
int  war_legal(const wpos *p, int player);      /* this player may turn a card now */
int  war_play(wgame *g, int player);            /* 0: not allowed */
int  war_undo(wgame *g);                        /* back one tap (a replay from the deal) */
int  war_turn(const wpos *p);                   /* the first player still to turn a card (-1: over) */
int  war_cards(const wpos *p, int player);      /* their pile and their cards on the table */
unsigned war_winners(const wpos *p);            /* WP_OVER: the most cards (bit n: player n) */
int  war_script(wgame *g, const char *moves);   /* 0: a tap is bad */

#endif
