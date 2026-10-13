/*
 * Scopa, the Italian fishing game, for 2 players or 4 in two partnerships, on one device: the rules. No SDL; the deal
 * comes from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The Italian pack of 40 (games/italian.h): 1..7, fante 8, cavallo 9, re 10, the values the captures add up. Four
 * players: partners sit across, players 0 and 2 against 1 and 3; their captures count together. A round: the dealer
 * (one seat on each round; player 0 plays first in the first round) gives 3 cards each and 4 face up on the table
 * (three or four re among those 4: dealt again); then, each time every hand is empty, 3 more each until the pack is
 * done. A turn: play one card. If a table card has its value it takes one such card (the player picks which); else, if
 * table cards add up to its value, it takes them (the player picks the set); a card that can take must take, and a
 * single card always comes before a sum. A card that takes nothing stays on the table. Clearing the table is a scopa
 * (a point), except with the very last card of the round. At the end of the round the cards left on the table go to
 * the side that took last (no scopa). The round's points, each side: most cards, most coins (denari), the settebello
 * (the 7 of coins), the primiera (the best card of each suit: 7 21, 6 18, ace 16, 5 15, 4 14, 3 13, 2 12, courts 10;
 * a side without all four suits has none), one point each, none on a tie; and a point for each scopa. The game ends
 * after the round in which a side reaches 11: the higher total wins (equal: another round).
 * Notation (script): a card ("7d", italian.h) plays it, taking its only possible capture (several: the first);
 * "7d:3c4s" names the cards it takes; n the next round; "." a plain action (the first card with a capture, else the
 * first card), "#" plain actions to the round's end, "!" to the game's end.
 */
#ifndef SCOPA_H
#define SCOPA_H

#include <stdint.h>

#include "games/italian.h"

#define SCOPA_MAXP 4
#define SCOPA_HAND 3
#define SCOPA_TABLE 24                          /* never reached: a table that large always has a match */
#define SCOPA_TARGET 11
#define SCOPA_MAX_ACTS 8192
#define SCOPA_NONE 255

enum { SC_PLAY, SC_ROUND, SC_OVER };
enum { SCORE_CARDS, SCORE_COINS, SCORE_SETTEBELLO, SCORE_PRIMIERA, SCORE_KINDS };
/* an action: SCA_NEXT, or a card (low byte) and the table cards it takes (bit i: table[i], from bit 8) */
#define SCA_NEXT 0xffffffffu
#define SCA_CARD(a) ((int)((a) & 0xff))
#define SCA_TAKE(a) ((uint32_t)(a) >> 8)
#define SCA(card, take) ((uint32_t)(card) | (uint32_t)(take) << 8)

typedef struct {
    uint8_t players, phase, turn, round, dealer;
    uint8_t hand[SCOPA_MAXP][SCOPA_HAND], n[SCOPA_MAXP];
    uint8_t table[SCOPA_TABLE], ntable;
    uint8_t stock[IT_CARDS], nstock;            /* the rest of the pack, dealt from its end */
    uint8_t took[SCOPA_MAXP][IT_CARDS], ntook[SCOPA_MAXP];   /* each player's captures, in order */
    uint8_t scope[SCOPA_MAXP], scopa_card[SCOPA_MAXP][IT_CARDS];   /* each player's scopas: their cards */
    uint8_t last_taker;                         /* SCOPA_NONE: nobody has taken yet this round */
    uint8_t deals;                              /* hands dealt this round */
    /* the last action: who played what, what it took, a scopa; the cards swept at the round's end, to whom */
    uint8_t lp, lcard, ltook[SCOPA_TABLE], nltook, lscopa, swept[SCOPA_TABLE], nswept, sweeper;
    /* the last round's count, each side (0: players 0 and 2, 1: players 1 and 3) */
    uint8_t count[2][SCORE_KINDS], won[2][SCORE_KINDS], rscope[2];
    int16_t pts[2], score[2];
    uint32_t seed;
} scpos;

typedef struct {
    scpos pos;
    uint32_t seed;
    uint8_t players;
    uint32_t act[SCOPA_MAX_ACTS];
    int n;
} scgame;

void scopa_new(scgame *g, int players, uint32_t seed);
/* the captures a card may make (bit masks of the table), at most max of them; their number (0: it takes nothing) */
int  scopa_captures(const scpos *p, int card, uint32_t *masks, int max);
int  scopa_legal(const scpos *p, uint32_t act);
int  scopa_play(scgame *g, uint32_t act);       /* 0: not allowed */
int  scopa_undo(scgame *g);                     /* back one action (a replay from the deal); 0: none */
int  scopa_value(int card);                     /* its value in a capture: 1..10 */
int  scopa_primiera_value(int card);            /* its points in the primiera */
int  scopa_primiera(const scpos *p, int side);  /* the side's primiera (0: a suit missing) */
int  scopa_side(const scpos *p, int player);    /* 0 or 1 */
int  scopa_winner(const scpos *p);              /* SC_OVER: the winning side (-1: not over) */
uint32_t scopa_auto(const scpos *p);            /* a plain action (the "." of the notation) */
int  scopa_script(scgame *g, const char *moves);   /* 0: an action is bad */

/* the bot (scopa_bot.c, bots.h) */
const scgame *scopa_live(void);                 /* the game on the screen, read-only: the bot copies it at entry */
int scopa_show(const char *move);               /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
uint32_t scob_choose(const scpos *live, int seat, int level, uint64_t seed, struct ai_stats *stats);   /* its action */

#endif
