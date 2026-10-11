/*
 * Crazy Eights for 2 to 6 players on one device: the rules. No SDL; the deal comes from a seed (cards.h's shuffle), so
 * a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * CE_EIGHTS, Crazy Eights: the 52 cards; 7 each for two players, 5 each for more. Play a card of the suit or the rank
 * of the discard's top card, or an eight (any time), and name a suit. A player who cannot play draws from the stock,
 * one card at a time, until they can (drawing is only for a player who cannot play); with no stock left (the
 * discard, but its top card, is shuffled into a new one; nothing left at all) they pass. Points left in hand: eight
 * 50, king, queen, jack 10, ace 1, the others their number. The game goes to 100.
 * CE_LAST, Last Card: the action deck (108); 7 each. Play a card of the colour or the number (or symbol) of the top
 * card, or a wild (name a colour). Skip: the next player misses their turn; reverse: the direction changes (two
 * players: a skip); draw two: the next player draws 2 and misses their turn; wild draw four: name a colour, the next
 * player draws 4 and misses their turn. It may be played only by a player with no card of the colour to follow (the
 * device checks it: no challenge). Draw two and draw four do not stack. A player may draw one card instead of
 * playing: they may play that card at once (only it), or keep it and pass; a card drawn that cannot be played ends
 * the turn. Down to one card: the player must call "last card" (before playing their second last card); if they
 * forget, the next player may catch them before doing anything else: they draw 2. Points: numbers their value,
 * skip, reverse, draw two 20, wilds 50. The game goes to 500.
 * Both: the first to empty their hand wins the round and scores the points left in the others' hands (a draw two or
 * draw four played last still makes the next player draw). The starter card is a plain one (an eight, an action card
 * or a wild goes back under the stock). Nobody able to play or draw (everyone passed in a row): the round ends, the
 * fewest points in hand win it, scoring the difference with each other hand. The deal moves on one seat a round.
 * Notation (script; the screenshots and the tests): p<i> play the i-th card of the hand (as shown: sorted), c<s>
 * choose suit or colour s (0..3: spades, hearts, diamonds, clubs; red, jade, amber, lapis), d draw, x pass,
 * l call "last card", k catch, n the next round; a one plain action, # plain actions to the end of the round,
 * * to the end of the game.
 */
#ifndef CRAZYEIGHTS_H
#define CRAZYEIGHTS_H

#include <stdint.h>

#include "cards.h"

#define CE_MAXP 6
#define CE_HAND 112
#define CE_MAX_ACTS 12000
#define CE_NONE 255
#define CE_GOAL_EIGHTS 100
#define CE_GOAL_LAST 500

enum { CE_EIGHTS, CE_LAST, CE_VARIANTS };
enum { CP_PLAY, CP_CHOOSE, CP_ROUND, CP_OVER };
/* an action: its kind (high byte) and an argument (a card, a suit) */
enum { CA_PLAY = 1 << 8, CA_CHOOSE = 2 << 8, CA_DRAW = 3 << 8, CA_PASS = 4 << 8, CA_CALL = 5 << 8, CA_CATCH = 6 << 8,
       CA_NEXT = 7 << 8 };
#define CA_KIND(a) ((a) & 0xff00)
#define CA_ARG(a) ((a) & 0xff)

typedef struct {
    uint8_t variant, players, deck, phase;
    uint8_t turn, dir;                          /* dir 0: forward (the next player: turn + 1); 1: reversed */
    uint8_t dealer, round;
    uint8_t hand[CE_MAXP][CE_HAND], n[CE_MAXP]; /* sorted: by suit (colour), then rank; wilds last */
    uint8_t stock[CARDS_MAX], pile[CARDS_MAX];  /* the stock, its top last; the discard, its top last */
    int16_t nstock, npile;
    uint8_t suit;                               /* the suit (colour) to follow: the top card's, or the one named */
    uint8_t drawn;                              /* last card: the card drawn this turn (CE_NONE: none yet) */
    uint8_t drew;                               /* cards drawn this turn */
    uint8_t called[CE_MAXP];                    /* last card: called, holding one card */
    uint8_t exposed;                            /* last card: the player who may be caught (CE_NONE) */
    uint8_t acted;                              /* the player to move has done something (no catching now) */
    uint8_t passes;                             /* passes in a row with nothing left to draw */
    uint8_t winner, blocked;                    /* the round's winner; it ended blocked */
    uint8_t reshuffles;
    int16_t score[CE_MAXP], gain;               /* the totals; the round's points (to the winner) */
    int16_t left[CE_MAXP];                      /* the points left in each hand at the round's end */
    uint32_t seed;
} cpos;

typedef struct {
    cpos pos;
    uint32_t seed;
    uint8_t variant, players;
    uint16_t act[CE_MAX_ACTS];
    int n, floor;                               /* floor: undo stops there (a turn ended, a card was drawn) */
} cgame;

void ce_new(cgame *g, int variant, int players, uint32_t seed);
int  ce_legal(const cpos *p, int act);
int  ce_play(cgame *g, int act);                /* 0: not allowed */
int  ce_undo(cgame *g);                         /* back one action (a replay from the deal), not past the floor */
int  ce_playable(const cpos *p, int card);      /* the player to move may play this card now */
int  ce_can_draw(const cpos *p);                /* a card is left to draw (the stock, or the discard to shuffle) */
int  ce_points(const cpos *p, int card);
int  ce_hand_points(const cpos *p, int player);
int  ce_next(const cpos *p, int from, int steps);  /* the player `steps` seats on, in the direction of play */
int  ce_goal(const cpos *p);
int  ce_winner(const cpos *p);                  /* CP_OVER: the winner (-1: not over) */
int  ce_auto(const cpos *p);                    /* a plain action (the "a" of the notation) */
int  ce_script(cgame *g, const char *moves);    /* 0: an action is bad */

#endif
