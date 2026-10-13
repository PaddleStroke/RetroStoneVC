/*
 * Spades, for four players in two partnerships on one device (hidden hands): the rules. No SDL; the deal comes from
 * a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Partners sit across: players 0 and 2 against 1 and 3. The 52 cards, 13 each. The round's first player (one seat
 * on each round) bids first and leads the first trick. Each player bids how many tricks they expect to take, 1 to
 * 13, or nil (0): to take none at all. The partners' bids (but nil) add up to their side's contract.
 * Follow suit if you can; otherwise play anything. Spades are always trumps: the highest spade wins the trick, or
 * else the highest card of the suit led (ace high); its winner leads the next. Spades may not be led until one has
 * been played on another suit (broken), unless the leader holds nothing but spades.
 * The score, each side: contract made (the tricks of its players who did not bid nil, at least the contract): 10 a
 * trick bid, and 1 for each trick over (a bag); not made: minus 10 a trick bid. A nil bidder's tricks never count for
 * the contract; they count as bags. A nil: 100 if its bidder took no trick, else minus 100. Each time a side has
 * gathered 10 bags: minus 100 (and those 10 bags are gone). After the round in which a side reaches 500 the game ends,
 * the higher score winning (equal scores: another round).
 * Notation (script): b<n> bids (b0: nil), <card> plays one ("2c", "Qs", "Th"; tricks.h), n the next round, "." one
 * plain action (a bid of the hand's sure tricks; the first legal card), "#" plain actions to the round's end, "!"
 * to the game's end.
 */
#ifndef SPADES_H
#define SPADES_H

#include <stdint.h>

#include "cards.h"

#define SPADES_P 4
#define SPADES_HAND 13
#define SPADES_MAX_ACTS 16384
#define SPADES_LIMIT 500
#define SPADES_NIL 0
#define SPADES_NOBID 255

enum { SP_BID, SP_PLAY, SP_ROUND, SP_OVER };
/* an action: its kind (high byte) and a bid or a card */
enum { SA_BID = 1 << 8, SA_PLAY = 2 << 8, SA_NEXT = 3 << 8 };
#define SA_KIND(a) ((a) & 0xff00)
#define SA_ARG(a) ((a) & 0xff)

typedef struct {
    uint8_t phase, turn, round, first;
    uint8_t hand[SPADES_P][SPADES_HAND], n[SPADES_P];
    uint8_t bid[SPADES_P], won[SPADES_P];       /* the bids (SPADES_NOBID: not yet); tricks taken this round */
    uint8_t trick[SPADES_P], ntrick, leader;
    uint8_t last[SPADES_P], last_leader, last_winner, tricks;
    uint8_t broken;
    int16_t score[2], bags[2];                  /* a side: 0 (players 0 and 2), 1 (1 and 3) */
    int16_t pts[2], over[2], nil[SPADES_P];     /* the last round: each side's points, its bags; each nil's +-100 */
    uint8_t penalty[2];                         /* the last round: a side's 10 bags paid (-100 each) */
    uint32_t seed;
} spos;

typedef struct {
    spos pos;
    uint32_t seed;
    uint16_t act[SPADES_MAX_ACTS];
    int n;
} sgame;

void spades_new(sgame *g, uint32_t seed);
int  spades_legal(const spos *p, int act);
int  spades_play(sgame *g, int act);            /* 0: not allowed */
int  spades_undo(sgame *g);                     /* back one action (a replay from the deal); 0: none */
int  spades_can_play(const spos *p, int card);
int  spades_contract(const spos *p, int side);  /* the side's bids added (nil: 0) */
int  spades_side_won(const spos *p, int side);  /* the tricks of its players who did not bid nil */
int  spades_winner(const spos *p);              /* SP_OVER: the winning side (0 or 1; -1: not over) */
int  spades_auto(const spos *p);                /* a plain action (the "." of the notation) */
int  spades_script(sgame *g, const char *moves);   /* 0: an action is bad */

/* the bot (spades_bot.c, bots.h), and what the screen (spades.c) gives it; a headless test defines those two */
const sgame *spades_live(void);   /* the game on the screen, read-only: the bot copies it at entry */
int spades_show(const char *move);   /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
struct tb_state;
int spb_choose(const sgame *g, int seat, int level, uint64_t seed, struct ai_stats *stats);   /* the action; 0: none */
void spb_view(const sgame *g, int me, struct tb_state *v, sgame *scratch);   /* seat me's knowledge (trickbot.h) */

#endif
