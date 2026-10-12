/*
 * Belote, the French trick-taking game, for four players in two partnerships (partners sit opposite: players 0 and 2
 * against 1 and 3), on one device passed round: the rules. No SDL; a game is its starting pack, its seed (the cuts) and
 * its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 32-card pack (7 to ace). Play goes round in player order (0, 1, 2, 3: the next player is the one after, the
 * dealer's right at a French table, play going counter-clockwise; the screen shows the next player on the viewer's
 * left, as Tarot does); the player after the dealer speaks and leads first, and the dealer moves on one seat each deal
 * (player 3 deals first).
 * - No shuffle between deals, as at a real table: the cards are gathered as they lie, cut, and dealt in the same
 *   packets, so the suits stay bunched from the tricks (bel_gather, bel_cut). The pack is listed from its top (the
 *   first card dealt):
 *   - a deal played out: each team's pile is its tricks stacked as won (the first at the bottom), each trick's cards in
 *     their order of play; then the defenders' pile is put on the takers': the defenders' last trick is on top.
 *   - a deal passed out (all passed, twice in the classic game; four passes in the coinche): each hand as held (sorted,
 *     as on the screen, the left-hand card at the bottom), from the player after the dealer round to the dealer, each
 *     put on the one before; then, classic, the rest of the pack with the turned card where it lies, on its top. (A
 *     deal left unfinished, for a new game: the teams' piles, the trick on the table, the hands, the rest of the pack,
 *     in that order upwards.)
 *   - the cut (the dealer's left-hand neighbour at a French table, here the player before the dealer, cuts, leaving at
 *     least 3 cards in each part): 3..29 cards lifted at a random point (the seed), the lower part put on top.
 *   - a game's first deal comes from the pack the caller gives (bel_start): the UI keeps the last pack of the session
 *     for its next game; the session's very first one comes from bel_warmup: a whole deal from a shuffled pack, never
 *     seen (a random player takes, or bids 80, the others pass; random legal cards), gathered as above.
 * BEL_CLASSIC: 3 then 2 cards each, and the next card turned up. Round one: each in turn takes the turned card's suit
 * as trumps, or passes; round two (all passed): each in turn names another suit, or passes; all pass again: a new
 * deal by the next dealer. Then 3 more cards each (the taker: the turned card and 2). The game: 501 points.
 * BEL_COINCHE: 3, 3 and 2 cards each, nothing turned. An auction: each in turn bids a contract (80 to 160 by tens, or
 * capot, 250) in a suit, higher than the last, or passes, or coinches (doubles) the opponents' last bid; a coinched
 * bidder may surcoinche (quadruple) or pass, and that ends the auction. Three passes after a bid end it; four passes
 * and no bid: a new deal. The game: 1000 points.
 * The play: trumps J 9 A 10 K Q 8 7 (20, 14, 11, 10, 4, 3, 0, 0 points), the other suits A 10 K Q J 9 8 7 (11, 10, 4,
 * 3, 2, 0, 0, 0). Follow suit; void, trump (over-trump the trick's best trump if you can, else any trump), unless your
 * partner holds the trick (then any card); trumps led: always over-trump if you can, even your partner. The last trick
 * scores 10 more (dix de der): 162 in all. Trump king and queen in one hand: belote and rebelote, 20 points to that
 * team, announced (shown) as they are played. Capot (all eight tricks): 250 instead of 162.
 * Classic score: the takers must make more points than the defenders (belote included); then each team scores its
 * points; else they are "dedans": the defenders score 162 (250 if capot) and the takers nothing (each team keeps its
 * belote). Coinche score: the contract is made with at least its value (belote included) and more than 81 (capot: all
 * the tricks); made: the takers score the contract and their points, the defenders their points; failed: the defenders
 * score 160 and the contract. Coinched or surcoinched: the winning side scores 160 and the contract, doubled or
 * quadrupled, the other side nothing (each team keeps its belote). The game ends after the deal in which a team reaches
 * the target; the higher total wins (equal totals: another deal).
 * Notation (script; the screenshots and the tests): t take the turned card, p pass, n<suit> name a suit, b<value><suit>
 * bid (b80h; b250s: capot), x coinche, xx surcoinche, <rank><suit> play a card (7 8 9 T J Q K A; s h d c), N the next
 * deal; "." one plain action, "#" plain actions to the end of the deal.
 */
#ifndef BELOTE_H
#define BELOTE_H

#include <stdint.h>

#include "cards.h"

#define BEL_P 4
#define BEL_HAND 8
#define BEL_MAX_ACTS 4096
#define BEL_LEVELS 10                           /* the coinche bids: 80, 90 .. 160, capot */
#define BEL_CAPOT 9
#define BEL_NONE 255

enum { BEL_CLASSIC, BEL_COINCHE, BEL_VARIANTS };
enum { BP_BID1, BP_BID2, BP_AUCTION, BP_PLAY, BP_DEAL, BP_OVER };
/* an action: its kind (high byte) and its argument: a suit (name), level << 2 | suit (bid), a card (play) */
enum { BA_PASS = 1 << 8, BA_TAKE = 2 << 8, BA_NAME = 3 << 8, BA_BID = 4 << 8, BA_COINCHE = 5 << 8,
       BA_SURCOINCHE = 6 << 8, BA_PLAY = 7 << 8, BA_NEXT = 8 << 8 };
#define BA_KIND(a) ((a) & 0xff00)
#define BA_ARG(a) ((a) & 0xff)
#define BA_BIDOF(level, suit) (BA_BID | (level) << 2 | (suit))

static inline int bel_value(int level) { return level >= BEL_CAPOT ? 250 : 80 + 10 * level; }
static inline int bel_team(int player) { return player & 1; }

typedef struct {
    uint8_t variant, phase, dealer, turn;
    uint8_t deal, dealt;                        /* deals played (scored); every deal so far (passed out too) */
    uint8_t hand[BEL_P][BEL_HAND], n[BEL_P];
    uint8_t pack[32], top;                      /* the deal's pack, cut, top first, and the next card to deal */
    uint8_t cutter, cut;                        /* who cut it, the cards they lifted (0: shuffled, the warm-up) */
    uint8_t heap[2][32], nheap[2];              /* each team's pile as it grows, bottom first: the tricks as won */
    uint8_t turned;                             /* classic: the turned card (CARD_NONE: none, or taken) */
    uint8_t taker, trump;                       /* BEL_NONE while bidding */
    uint8_t passes;                             /* passes in a row */
    uint16_t said[BEL_P];                       /* each player's last word in the bidding (an action; 0: none yet) */
    uint8_t bidder, level, coinche, coincher;   /* coinche: the highest bid, by whom; 0 / 1 coinched / 2 surcoinched */
    uint8_t leader, trick[BEL_P], ntrick, tricks;   /* trick[i]: played by leader + i */
    uint8_t last[BEL_P], last_leader, last_winner;  /* the trick before (BEL_NONE: none) */
    uint8_t won[2];                             /* tricks won by each team */
    int16_t points[2];                          /* card points taken (and the dix de der) */
    uint8_t belote, belote_said;                /* who holds the trump king and queen (BEL_NONE); how many played */
    /* the deal's score (BP_DEAL, BP_OVER) */
    int16_t made_pts[2], score[2], total[2];    /* each team's points (cards, belote; capot 250), score, total */
    uint8_t made, capot;                        /* the contract made; capot: the team that took every trick + 1 */
    uint32_t seed;
} bpos;

typedef struct {
    bpos pos;
    uint32_t seed;
    uint8_t variant;
    uint8_t start[32];                          /* the pack the game began from, top first (before its cut) */
    uint16_t act[BEL_MAX_ACTS];
    int n;
} bgame;

void bel_start(bgame *g, int variant, uint32_t seed, const uint8_t *pack);   /* the first deal from this pack, top
                                                  * first (cut first) */
int  bel_warmup(uint8_t *pack, int variant, uint32_t seed);   /* a session's first pack: a whole deal played from a
                                                  * shuffled pack, unseen, then gathered (1: played out) */
void bel_new(bgame *g, int variant, uint32_t seed);   /* the warm-up, then bel_start */
void bel_gather(const bpos *p, uint8_t *pack);  /* the cards picked up as they lie, top first */
int  bel_cut(uint8_t *pack, uint32_t seed);     /* the cut: 3..29 cards lifted, the lower part on top; returns how
                                                  * many were lifted */
void bel_sort(uint8_t *cards, int n, int trump);   /* a hand as the screen shows it, left to right: spades, hearts,
                                                  * clubs, diamonds, the strongest first (trump: BEL_NONE while
                                                  * bidding) */
int  bel_legal(const bpos *p, int act);
int  bel_moves(const bpos *p, int *acts);       /* every legal action (at most 64); how many */
int  bel_play(bgame *g, int act);               /* 0: not allowed */
int  bel_undo(bgame *g);                        /* back one action (a replay from the deal); 0: none */
int  bel_rank(int card, int trump);             /* its strength in its suit: 1..8 */
int  bel_points(int card, int trump);
int  bel_trick_best(const bpos *p);             /* the index in the trick of the card that holds it */
int  bel_target(const bpos *p);                 /* 501 or 1000 */
int  bel_winner_team(const bpos *p);            /* BP_OVER: 0 or 1; else -1 */
int  bel_auto(const bpos *p);                   /* a plain action (the "." of the notation) */
int  bel_script(bgame *g, const char *moves);   /* 0: an action is bad */

#endif
