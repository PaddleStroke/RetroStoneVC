/*
 * Pinochle, two-handed with a stock, or four-handed partnership auction pinochle, on one device (hidden hands): the
 * rules. No SDL; the deal comes from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays
 * them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The pack: 48 cards, two of each 9, 10, jack, queen, king and ace in the four suits. In a suit, high to low: ace, 10,
 * king, queen, jack, 9; of two equal cards in a trick, the first played wins. The highest trump wins a trick, or else
 * the highest card of the suit led. Counters, won in tricks: ace, 10 and king 1 point each; the last trick 1 more (25
 * a deal). Melds (points): run (ace, 10, king, queen, jack of trumps) 15, royal marriage (king and queen of trumps) 4,
 * marriage (king and queen of another suit) 2, dix (9 of trumps) 1, pinochle (queen of spades and jack of diamonds) 4,
 * double pinochle 30, four aces of different suits 10, kings 8, queens 6, jacks 4 (four players: all eight of a rank
 * 100, 80, 60, 40; two runs 150).
 * Two players: 12 cards each; the next card turned up for trumps lies under the stock (a 9: the dealer scores 1). The
 * non-dealer leads; while the stock lasts any card may be played. The winner of a trick may meld once, laying the
 * cards face up (they stay in play from the table), scored at once; then draws the top card, the loser the next (the
 * last one is the turned card). A meld needs a card from the hand; a card on the table may serve again in a meld of
 * another kind (marriages and the run are one kind, the arounds another, the pinochles a third), never twice in one
 * kind. Melding the dix while the turned card is still there exchanges them. Once the stock is gone, the melds go back
 * to the hands and the last 12 tricks are strict: follow suit and beat the trick if you can; void, trump (and beat a
 * trump) if you can. Each deal adds the counters; the first to 100 after a deal wins (equal: another deal).
 * Four players, partners across (0 and 2 against 1 and 3): 12 cards each. From the dealer's left each bids (20 at
 * least, more than the last bid) or passes for the deal; the last bidder (the dealer when all others pass) names
 * trumps. Every player's melds are counted and shown; the bidder leads; all 12 tricks are strict. A side's melds count
 * only if it takes a trick. The bidding side scores its melds and counters if they reach the bid, else it is set: the
 * bid off its score. The other side scores its melds and counters. The first to 150 wins (the bidding side first).
 * Notation (script): a card ("As", "Th", "9c": tricks.h) plays it; M and a meld's cards ("MKsQs", "MAsTsKsQsJs",
 * "M9h") melds them, x passes the meld; b<n> bids, p passes; t<suit> names trumps (ts th td tc); g goes on from the
 * melds; n the next deal; "." a plain action, "#" plain actions to the deal's end, "!" to the game's end.
 */
#ifndef PINOCHLE_H
#define PINOCHLE_H

#include <stdint.h>

#include "cards.h"

#define PN_MAXP 4
#define PN_HAND 12
#define PN_PACK 48
#define PN_STOCK 24
#define PN_MAX_ACTS 4096
#define PN_NONE 255
#define PN_MIN_BID 20
#define PN_TARGET2 100
#define PN_TARGET4 150

enum { PP_BID, PP_TRUMP, PP_MELDS, PP_PLAY, PP_MELD, PP_ROUND, PP_OVER };
/* the melds; their kinds (a card serves once a kind) */
enum { PM_RUN, PM_ROYAL, PM_MARRIAGE, PM_DIX, PM_PINOCHLE, PM_DOUBLE, PM_ACES, PM_KINGS, PM_QUEENS, PM_JACKS, PM_KINDS };
enum { PK_MARRIAGE = 1, PK_DIX = 2, PK_PINOCHLE = 4, PK_AROUND = 8, PK_TABLE = 16, PK_PUBLIC = 32 };   /* a card's flags */
/* an action: its kind (high byte) and its argument */
enum { PA_PLAY = 1 << 8, PA_MELD = 2 << 8, PA_SKIP = 3 << 8, PA_BID = 4 << 8, PA_PASS = 5 << 8, PA_TRUMP = 6 << 8,
       PA_GO = 7 << 8, PA_NEXT = 8 << 8 };
#define PA_KIND(a) ((a) & 0xff00)
#define PA_ARG(a) ((a) & 0xff)
#define PA_MELD_OF(kind, suit) (PA_MELD | (kind) << 2 | (suit))

typedef struct {
    uint8_t players, phase, turn, dealer, round, leader, trump, up;
    uint8_t hand[PN_MAXP][PN_HAND], flag[PN_MAXP][PN_HAND], n[PN_MAXP];   /* flag: PK_* (two players) */
    uint8_t stock[PN_STOCK], nstock;            /* two players: drawn from its end; stock[0] the turned card */
    uint8_t trick[PN_MAXP], ntrick;
    uint8_t last[PN_MAXP], last_leader, last_winner, tricks;
    uint8_t drew[2];                            /* the cards drawn after the last trick: its winner's, the loser's */
    uint8_t won[PN_MAXP];                       /* tricks taken this deal */
    uint8_t last_meld, last_melder;             /* two players: the last meld (PM_*; PN_NONE: none this turn) */
    uint8_t bid[PN_MAXP], passed, high, bidder; /* four players: each bid (0: none), who passed (bits), the bid */
    uint8_t mcards[PN_MAXP][PN_HAND], nm[PN_MAXP];   /* four players: each one's melded cards */
    int16_t meldpts[PN_MAXP];                   /* four players: each one's melds; two: melds scored this deal */
    int16_t counters[2], melds[2], pts[2];      /* the deal by side: counters (and the last trick), melds, points */
    int16_t score[2];
    uint8_t set, dix;                           /* the bidding side was set; the dealer turned the dix */
    uint32_t seed;
} pino_pos;

typedef struct {
    pino_pos pos;
    uint32_t seed;
    uint8_t players;
    uint16_t act[PN_MAX_ACTS];
    int n;
} pino_game;

void pinochle_new(pino_game *g, int players, uint32_t seed);
int  pinochle_legal(const pino_pos *p, int act);
int  pinochle_play(pino_game *g, int act);      /* 0: not allowed */
int  pinochle_undo(pino_game *g);
int  pinochle_side(const pino_pos *p, int player);
int  pinochle_can_play(const pino_pos *p, int card);
int  pinochle_power(int card, int trump, int led);   /* its strength in a trick: 0 can not win */
int  pinochle_order(int card);                  /* in its suit: 9 1 .. ace 6 */
int  pinochle_counter(int card);                /* ace, 10, king: 1 */
int  pinochle_meld_value(int kind, int players);
int  pinochle_meld_cards(int kind, int suit, int trump, uint8_t *out);   /* the cards a meld takes; their number */
int  pinochle_meld_of(const uint8_t *cards, int n, int trump);   /* the meld action these cards make; 0: none */
int  pinochle_can_meld(const pino_pos *p);      /* two players: the winner has a meld to make */
int  pinochle_hand_melds(const uint8_t *h, int n, int trump, uint8_t *cards, int *ncards);   /* four players */
int  pinochle_winner(const pino_pos *p);        /* PP_OVER: the winning side (-1: not over) */
int  pinochle_auto(const pino_pos *p);          /* a plain action (the "." of the notation) */
int  pinochle_script(pino_game *g, const char *moves);   /* 0: an action is bad */
int  pinochle_name(const pino_pos *p, int act, char *out, int cap);   /* an action in the notation */
/* the cards a strict trick allows (follow and beat; void: trump and beat a trump; else any): their number */
int  pinochle_strict(const uint8_t *h, int n, const uint8_t *trick, int ntrick, int trump, uint8_t *out);

/* the bot (pinochle_bot.c, bots.h), and what the screen (pinochle.c) gives it; a headless test defines those two */
const pino_game *pinochle_live(void);
int pinochle_show(const char *move);
struct ai_stats;
struct tb_state;
int pnb_choose(const pino_game *g, int seat, int level, uint64_t seed, struct ai_stats *stats);   /* 0: none */
void pnb_view(const pino_game *g, int me, struct tb_state *v, pino_game *scratch);

#endif
