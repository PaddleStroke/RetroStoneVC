/*
 * Briscola, the Italian trick-taking game, for 2 players or 4 in two partnerships, on one device: the rules. No SDL;
 * the deal comes from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The Italian pack of 40 (games/italian.h). Four players: partners sit across, players 0 and 2 against 1 and 3. Three
 * cards each; the next card is turned face up, the briscola: its suit is trumps; it lies under the stock and is the
 * last card drawn. Player 0 leads the first trick. Each player in turn plays any card: there is no need to follow
 * suit. The highest trump wins the trick, or else the highest card of the suit led. The order, high to low: ace, 3,
 * re, cavallo, fante, 7, 6, 5, 4, 2. The winner takes the trick; then, while the stock lasts, each player draws a card,
 * the winner first, and the winner leads the next trick. When all the cards are played, each side counts its tricks:
 * ace 11, three 10, re 4, cavallo 3, fante 2, the others nothing: 120 in all. 61 or more wins; 60 each is a draw. A
 * game is one deal.
 * Notation (script): a card ("1b", italian.h) plays it; "." a plain action (the first card in hand), "#" plain
 * actions to the end.
 */
#ifndef BRISCOLA_H
#define BRISCOLA_H

#include <stdint.h>

#include "games/italian.h"

#define BRISCOLA_MAXP 4
#define BRISCOLA_HAND 3
#define BRISCOLA_TOTAL 120
#define BRISCOLA_NONE 255

enum { BR_PLAY, BR_OVER };

typedef struct {
    uint8_t players, phase, turn, leader;
    uint8_t hand[BRISCOLA_MAXP][BRISCOLA_HAND], n[BRISCOLA_MAXP];
    uint8_t stock[IT_CARDS], nstock;            /* drawn from its end; stock[0] is the briscola, face up */
    uint8_t briscola, trump;                    /* the turned card, its suit */
    uint8_t trick[BRISCOLA_MAXP], ntrick;       /* the cards played to the trick, from the leader on */
    uint8_t last[BRISCOLA_MAXP], last_leader, last_winner, tricks;   /* the last trick won */
    uint8_t drew[BRISCOLA_MAXP], ndrew;         /* the cards drawn after it, from the winner on */
    uint8_t took[2][IT_CARDS], ntook[2];        /* each side's tricks (0: players 0 and 2, 1: players 1 and 3) */
    int16_t points[2];
    uint32_t seed;
} brpos;

typedef struct {
    brpos pos;
    uint32_t seed;
    uint8_t players;
    uint8_t act[IT_CARDS];
    int n;
} brgame;

void briscola_new(brgame *g, int players, uint32_t seed);
int  briscola_legal(const brpos *p, int card);
int  briscola_play(brgame *g, int card);        /* 0: not allowed */
int  briscola_undo(brgame *g);                  /* back one card (a replay from the deal); 0: none */
int  briscola_points(int card);                 /* its points: 11, 10, 4, 3, 2 or 0 */
int  briscola_strength(int card);               /* its place in the order: 10 the ace .. 1 the 2 */
int  briscola_trick_winner(const uint8_t *cards, int n, int trump);   /* the index of the card that wins */
int  briscola_side(const brpos *p, int player);
int  briscola_winner(const brpos *p);           /* BR_OVER: the winning side (-1: not over, 2: a draw) */
int  briscola_auto(const brpos *p);             /* a plain action (the "." of the notation) */
int  briscola_script(brgame *g, const char *moves);   /* 0: a card is bad */

#endif
