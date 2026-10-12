/*
 * Klaverjas, the Dutch trick game for four in two partnerships (players 0 and 2 against 1 and 3), on one device passed
 * round: the rules, the Rotterdam way (always trump when you cannot follow). No SDL; a game is its seed and its
 * actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 32-card pack (7 to ace), French suits; 8 cards each (packets of 3, 2, 3). Player 3 deals first, the deal moving
 * on one seat each time; the player after the dealer (voorhand) speaks first and leads the first trick.
 * - Trumps: from voorhand round, each in turn names a trump suit or passes; the first to name one plays (his team must
 *   make the game). All four passed: voorhand must name one (verplicht).
 * - Trumps rank J 9 A 10 K Q 8 7 (20, 14, 11, 10, 4, 3, 0, 0 points); the other suits A 10 K Q J 9 8 7 (11, 10, 4, 3,
 *   2, 0, 0, 0). The last trick: 10 more, 162 in all.
 * - Play: follow suit. Void: trump, even over your partner's winning card, and over the trick's best trump if you can;
 *   unable to go over, a card of another suit if you have one (no undertrumping), else a lower trump. Trumps led: a
 *   higher trump than the trick's best if you can.
 * - Roem, in each trick as it lies, to the team that takes it: three in sequence in one suit (7 8 9 10 J Q K A) 20,
 *   four 50; the trump king and queen (stuk) 20, with a sequence too; four aces, kings, queens or tens 100, four jacks
 *   200. Pit (all eight tricks to one team): 100 more roem.
 * - The playing team must make more than half of all the points (both teams' cards and roem): then each team scores
 *   its own; else nat: the other team scores them all (162 and every roem).
 * - 16 deals; the higher total wins, equal totals share the win.
 * Notation (script): t<suit> names trumps ("th"), p passes, <card> plays one ("Jh", "Ts"; tricks.h), N the next deal;
 * "." one plain action, "#" plain actions to the end of the deal, "!" to the end of the game.
 */
#ifndef KLAVERJAS_H
#define KLAVERJAS_H

#include <stdint.h>

#include "cards.h"
#include "games/trick4.h"

#define KVJ_HAND 8
#define KVJ_DEALS 16
#define KVJ_MAX_ACTS 2048
#define KVJ_NONE 255
#define KVJ_TOTAL 162

enum { KP_BID, KP_PLAY, KP_DEAL, KP_OVER };
/* an action: its kind and its argument (a suit, a card) */
enum { KA_PASS = 1 << 8, KA_TRUMP = 2 << 8, KA_PLAY = 3 << 8, KA_NEXT = 4 << 8 };
#define KA_KIND(a) ((a) & 0xff00)
#define KA_ARG(a) ((a) & 0xff)
enum { KW_NONE, KW_PASS, KW_TRUMP };            /* each player's word in the bidding */

typedef struct {                                /* a deal, once scored */
    uint8_t taker, trump, made, pit;            /* pit: the team that took every trick + 1 (0: none) */
    int16_t eyes[2], roem[2], score[2];         /* each team's card points, roem (and pit), score */
} kvdeal;

typedef struct {
    uint8_t phase, deal, dealer, turn;
    tk4_table t;
    uint8_t word[TK4_P], passes;
    uint8_t taker, trump;                       /* KVJ_NONE while bidding */
    int16_t roem[2];                            /* this deal's roem, by team */
    int16_t trick_roem;                         /* the last trick's */
    kvdeal list[KVJ_DEALS];
    uint8_t ndeals;
    int16_t total[2];
    uint32_t seed;
} kvpos;

typedef struct {
    kvpos pos;
    uint32_t seed;
    uint16_t act[KVJ_MAX_ACTS];
    int n;
} kvgame;

void kvj_new(kvgame *g, uint32_t seed);
int  kvj_legal(const kvpos *p, int act);
int  kvj_play(kvgame *g, int act);              /* 0: not allowed */
int  kvj_undo(kvgame *g);                       /* back one action (a replay from the start); 0: none */
int  kvj_can_play(const kvpos *p, int card);    /* the player to move may play it */
int  kvj_points(int card, int trump);
int  kvj_power(int card, int trump);            /* its strength in its suit (higher wins) */
int  kvj_roem(const uint8_t *cards, int n, int trump);   /* the roem lying in a trick */
int  kvj_winner(const uint8_t *cards, int n, int trump); /* the index of the card that takes the trick */
void kvj_sort(uint8_t *h, int n, int trump);    /* a hand as shown (trump KVJ_NONE: while bidding) */
int  kvj_must_name(const kvpos *p);             /* 1: all have passed, voorhand must name trumps */
int  kvj_winners(const kvpos *p);               /* KP_OVER: the winning players (bit n), both of a team */
int  kvj_auto(const kvpos *p);                  /* a plain action (the "." of the notation) */
int  kvj_script(kvgame *g, const char *moves);  /* 0: an action is bad */

#endif
