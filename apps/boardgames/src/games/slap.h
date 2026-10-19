/*
 * Bataille corse (Egyptian Ratscrew), for 2 to 4 players round one device: the rules. No SDL; the deal comes from a
 * seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The rules as played here:
 * - The 52 cards are dealt out face down, one at a time from player 0 (three players: 18, 17, 17). Nobody looks at
 *   their pile. Player 0 starts; play goes round in player order.
 * - In turn, each player turns the top card of their pile onto the centre pile.
 * - A face card or an ace demands tribute: the next player must turn cards until they turn a face card or an ace
 *   themselves, with at most 4 tries for an ace, 3 for a king, 2 for a queen, 1 for a jack. If they turn one in
 *   time, the next player owes tribute in their turn; if not, the player of the face card wins the centre pile: once
 *   the last try has been seen (a moment for slaps), they gather it under their pile and play next.
 * - A slap: any player may slap at any time. A double (the top card of the same rank as the one under it) wins the
 *   centre pile for the first who slaps, who plays next. SLAP_SANDWICH also counts a sandwich (the top card of the
 *   same rank as the third one). A wrong slap (no double, no sandwich) costs a card: the slapper's top card goes face
 *   down under the centre pile (a card face down never makes a double).
 * - A player whose pile is empty no longer turns cards, but may slap to come back; a wrong slap with no card to give
 *   puts them out for good. A tribute owed by a player who runs out goes to the player of the face card. Alone with
 *   cards, a player turns cards alone (a face card wins the pile at once).
 * - The one who gathers all 52 cards wins (or the last one not out).
 * Notation (script): p the player to move turns a card, s<n> player n slaps, c the pile gathered (tribute unpaid);
 * "." one plain action (a slap by the next player when it is right, a gathering, or a card turned), "#" plain actions
 * to the end of the game (at most 20000).
 */
#ifndef SLAP_H
#define SLAP_H

#include <stdint.h>

#include "cards.h"

#define SLAP_MAXP 4
#define SLAP_MAX_ACTS 32768
#define SLAP_NONE 255

enum { SLAP_DOUBLE, SLAP_SANDWICH, SLAP_VARIANTS };
enum { SLP_PLAY, SLP_CLAIM, SLP_OVER };
/* an action: its kind (high byte), the slapper */
enum { SLA_PLAY = 1 << 8, SLA_SLAP = 2 << 8, SLA_CLAIM = 3 << 8 };
#define SLA_KIND(a) ((a) & 0xff00)
#define SLA_ARG(a) ((a) & 0xff)
/* the last thing that happened to the centre pile (for the pictures) */
enum { SLE_NONE, SLE_PLAY, SLE_TAKE, SLE_PENALTY, SLE_OUT };

typedef struct {
    uint8_t variant, players, phase, turn;      /* turn: who turns a card (SLP_CLAIM: who gathers the pile) */
    uint8_t pile[SLAP_MAXP][52], n[SLAP_MAXP];  /* each pile, its top last */
    uint8_t out;                                /* bit q: out for good */
    uint8_t centre[52], nc, buried;             /* the centre pile, its top last; the penalty cards face down under
                                                 * it (they never make a double) */
    uint8_t owed, claimer;                      /* the tries left to the player paying tribute; who demands it */
    uint8_t lastp;                              /* who turned the last card */
    uint8_t event, who, how;                    /* the last event (SLE_*), its player; a take: 1 a slap, 2 tribute */
    uint8_t winner;
    uint32_t seed;
} slpos;

typedef struct {
    slpos pos;
    uint32_t seed;
    uint8_t variant, players;
    uint16_t act[SLAP_MAX_ACTS];
    int n;
} slgame;

void slap_new(slgame *g, int variant, int players, uint32_t seed);
int  slap_tries(int card);                       /* the tribute a card demands: ace 4, king 3, queen 2, jack 1; 0 */
int  slap_right(const slpos *p);                 /* a slap now would be right (a double, or a sandwich) */
int  slap_legal(const slpos *p, int act);
int  slap_play(slgame *g, int act);              /* 0: not allowed */
int  slap_undo(slgame *g);                       /* back one action (a replay from the deal); 0: none */
int  slap_touch(const slpos *p, int q, int on_pile);   /* what a touch in player q's zone does (on their pile or
                                                 * not): a card turned, the pile gathered, a slap; 0: nothing (out,
                                                 * the game over, no pile to slap: a late slap costs nothing) */
int  slap_auto(const slpos *p);                  /* a plain action (the "." of the notation) */
int  slap_script(slgame *g, const char *moves);  /* 0: an action is bad; an action's "/n" (a bot's: the actions
                                                 * it saw) is let pass */

/* the bot (slap_bot.c, bots.h), and what the screen (slap.c) gives it; a headless test defines all three */
const slgame *slap_live(void);                  /* the game on the screen, read-only: the bot copies it at entry */
int slap_show(const char *move);                /* main thread: "p/57", "c/57", "s1/57" as touches do (57: the
                                                 * actions the bot saw, slgame.n); a move the game has moved past,
                                                 * or a slap no longer right (a human was quicker), does nothing;
                                                 * 0: not legal */
int slap_realtime(void);                        /* 1: the bot waits its reaction time before a slap (0: the tests) */

#endif
