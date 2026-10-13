/*
 * Euchre, for four players in two partnerships on one device (hidden hands): the rules. No SDL; the deal comes from
 * a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Partners sit across: players 0 and 2 against 1 and 3. The 24 cards 9, 10, J, Q, K, A of each suit. Player 3 deals
 * first, then the deal goes round. Five cards each (packets of 3 and 2, then 2 and 3), the next card of the four left
 * (the kitty) turned up.
 * - Making trumps, from the dealer's left. Round one: order it up (the turned card's suit is trumps, and the dealer
 *   takes the card into his hand and lays one away, face down) or pass. All four pass: the card is turned down; round
 *   two: name another suit, or pass. Stick the dealer: in round two the dealer may not pass. The maker may go alone:
 *   his partner lays his cards down and sits the hand out (when the dealer's partner goes alone, the dealer sits out
 *   and does not take the card).
 * - Trumps, high to low: the right bower (the jack of trumps), the left bower (the other jack of the same colour: a
 *   trump, not of its own suit), A, K, Q, 10, 9. Other suits A, K, Q, (J), 10, 9. The dealer's left leads (the next
 *   one if he sits out); follow suit if you can (the left bower follows trumps), else anything; the highest trump, or
 *   the highest card of the suit led, wins; the winner leads next.
 * - The score: the makers take 3 or 4 tricks: 1 point; all 5 (a march): 2; alone, all 5: 4. Fewer than 3: euchred,
 *   the defenders score 2. The first side to 10 points wins.
 * Notation (script): o orders it up (oa: alone), p passes, n<suit> names a suit (nha: hearts, alone), d<card> lays a
 * card away (the dealer), <card> plays one ("Jh", "Ts"; tricks.h), N the next hand; "." one plain action, "#" plain
 * actions to the end of the hand, "!" to the end of the game.
 */
#ifndef EUCHRE_H
#define EUCHRE_H

#include <stdint.h>

#include "cards.h"

#define EU_P 4
#define EU_HAND 6                               /* 5, and the dealer's sixth before laying one away */
#define EU_MAX_ACTS 8192
#define EU_GOAL 10
#define EU_NONE 255

enum { EP_ROUND1, EP_ROUND2, EP_DISCARD, EP_PLAY, EP_HAND, EP_OVER };
/* an action: its kind and its argument (a card; a suit | alone << 4) */
enum { EA_ORDER = 1 << 8, EA_PASS = 2 << 8, EA_NAME = 3 << 8, EA_DISCARD = 4 << 8, EA_PLAY = 5 << 8,
       EA_NEXT = 6 << 8, EA_ALONE = 16 };
#define EA_KIND(a) ((a) & 0xff00)
#define EA_ARG(a) ((a) & 0xff)
/* each player's word this hand */
enum { EW_NONE, EW_PASS, EW_ORDER, EW_NAME };

typedef struct {
    uint8_t phase, dealer, turn, deal;
    uint8_t hand[EU_P][EU_HAND], n[EU_P];
    uint8_t kitty[4], up, turned_down;          /* the four left; the turned card (kitty[0]); turned down (round 2) */
    uint8_t word[EU_P];                         /* EW_* */
    uint8_t maker, trump, alone, out;           /* out: the player sitting out (EU_NONE) */
    uint8_t laid;                               /* the dealer's card laid away (EU_NONE) */
    uint8_t trick[EU_P], who[EU_P], ntrick, leader, tricks;
    uint8_t last[EU_P], last_who[EU_P], nlast, last_winner;
    uint8_t won[2], took[EU_P];                 /* each side's tricks this hand, each player's */
    int8_t pts[2];                              /* the last hand's points */
    int16_t score[2];
    uint32_t seed;
} epos;

typedef struct {
    epos pos;
    uint32_t seed;
    uint16_t act[EU_MAX_ACTS];
    int n;
} egame;

void euchre_new(egame *g, uint32_t seed);
int  euchre_legal(const epos *p, int act);
int  euchre_play(egame *g, int act);            /* 0: not allowed */
int  euchre_undo(egame *g);                     /* back one action (a replay from the start); 0: none */
int  euchre_suit(int card, int trump);          /* the suit it follows (the left bower: trumps) */
int  euchre_power(int card, int trump);         /* its strength (a trump over any other card) */
int  euchre_trick_best(const uint8_t *cards, int n, int trump);   /* the index of the card that wins */
int  euchre_can_play(const epos *p, int card);
int  euchre_sitting(const epos *p, int player); /* this player sits the hand out */
void euchre_sort(uint8_t *cards, int n, int trump);   /* as shown, left to right (trump EU_NONE: no trumps yet) */
int  euchre_winner(const epos *p);              /* EP_OVER: the winning side (0 or 1); else -1 */
int  euchre_auto(const epos *p);                /* a plain action (the "." of the notation) */
int  euchre_script(egame *g, const char *moves);   /* 0: an action is bad */

/* the bot (euchre_bot.c, bots.h), and what the screen (euchre.c) gives it; a headless test defines those two */
const egame *euchre_live(void);   /* the game on the screen, read-only: the bot copies it at entry */
int euchre_show(const char *move);   /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
struct tb_state;
int eub_choose(const egame *g, int seat, int level, uint64_t seed, struct ai_stats *stats);   /* the action; 0: none */
void eub_view(const egame *g, int me, struct tb_state *v, egame *scratch);   /* seat me's knowledge (trickbot.h) */

#endif
