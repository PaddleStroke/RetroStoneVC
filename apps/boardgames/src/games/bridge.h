/*
 * Contract bridge, for four players in two partnerships on one device (hidden hands): the rules. No SDL; the deals
 * come from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Partners sit across: players 0 and 2 (side 0, "north-south") against 1 and 3 (side 1, "east-west"). Play goes
 * round 0, 1, 2, 3 (clockwise on the screen). The 52 cards, 13 each.
 * - The auction: the dealer calls first, then each in turn: a bid (a level 1..7 and a strain: clubs, diamonds,
 *   hearts, spades, no trumps, in that order of rank; each bid higher than the last), pass, double (the opponents'
 *   last bid, not doubled yet) or redouble (one's own side's bid, doubled by the opponents). Three passes after a bid
 *   end it: the last bid is the contract (doubled or redoubled if so); the declarer is the player of that side who
 *   first named its strain. Four passes and no bid: passed out.
 * - The play: the player after the declarer leads; then the dummy (the declarer's partner) lays its hand face up and
 *   the declarer plays it too. Follow suit if you can; the highest trump wins the trick, else the highest card of the
 *   suit led (ace high); its winner leads the next. The declarer needs 6 tricks more than the level.
 * - The score (the international laws): contracted tricks (clubs, diamonds 20 a trick; hearts, spades 30; no trumps
 *   40 for the first, 30 for the others), doubled x2, redoubled x4; overtricks (their trick value, or 100 / 200 a
 *   trick doubled, 200 / 400 redoubled, not vulnerable / vulnerable); 50 (100 redoubled) for making a doubled
 *   contract; slams (small 500 / 750, grand 1000 / 1500); undertricks to the defenders (50 / 100 a trick; doubled
 *   100, 200, 200, then 300 each, not vulnerable, 200 then 300 each, vulnerable; redoubled twice that).
 *   - BRG_CHICAGO (the default): four deals, each scored by itself as in duplicate: a game (100 trick points or more
 *     in one deal) 300 / 500, a part-score 50. The dealer moves on one seat each deal (player 0 first); a passed-out
 *     deal is dealt again by the same dealer. Vulnerable: nobody on the first deal, the dealer's side on the second
 *     and the third, both sides on the fourth.
 *   - BRG_RUBBER: the trick points go "below the line" and add up to a game at 100 (a game won: both sides' part-
 *     scores start again from 0); a side that has won a game is vulnerable; the rubber ends when a side has won two:
 *     700 (two games to none) or 500 (two to one). Honours, rubber only: one hand holding the five top trumps (ace,
 *     king, queen, jack, 10) 150, four of them 100; at no trumps, the four aces 150; to that hand's side, whatever the
 *     result. The dealer moves on one seat each deal, passed out too.
 *   The game ends after the fourth deal (Chicago) or the rubber; the higher total wins (equal: a draw).
 * Notation (script): b<level><c|d|h|s|n> a bid (b3n: three no trumps), p pass, x double, xx redouble, <card> plays
 * one ("2c", "Qs", "Th"; tricks.h), N the next deal, "." one plain action (a modest bid or a pass; the first legal
 * card), "#" plain actions to the deal's end, "!" to the game's end.
 */
#ifndef BRIDGE_H
#define BRIDGE_H

#include <stdint.h>

#include "cards.h"

#define BRG_P 4
#define BRG_HAND 13
#define BRG_MAX_ACTS 16384
#define BRG_CALLS 330                           /* the longest auction there can be: 319 calls */
#define BRG_DEALS 64                            /* the deals kept for the score sheet */
#define BRG_NONE 255

enum { BRG_CHICAGO, BRG_RUBBER, BRG_VARIANTS };
enum { BRG_AUCTION, BRG_PLAY, BRG_SCORE, BRG_OVER };   /* BRG_SCORE: a deal is over (scored, or passed out) */
enum { BRG_CLUBS, BRG_DIAMONDS, BRG_HEARTS, BRG_SPADES, BRG_NT, BRG_STRAINS };
/* the calls: pass 0, the bids 1..35 (level 1 clubs .. level 7 no trumps), double, redouble */
#define BRG_PASS 0
#define BRG_BID(level, strain) ((level) * 5 - 4 + (strain))
#define BRG_LEVEL(call) (((call) - 1) / 5 + 1)
#define BRG_STRAIN(call) (((call) - 1) % 5)
#define BRG_DOUBLE 36
#define BRG_REDOUBLE 37
/* an action: its kind (high byte) and a call or a card */
enum { BRG_ACALL = 1 << 8, BRG_APLAY = 2 << 8, BRG_ANEXT = 3 << 8 };
#define BRG_KIND(a) ((a) & 0xff00)
#define BRG_ARG(a) ((a) & 0xff)

static inline int brg_side(int player) { return player & 1; }
/* a strain's suit in the card kit (no trumps: -1) */
static inline int brg_suit(int strain)
{
    static const int8_t s[5] = {SUIT_CLUBS, SUIT_DIAMONDS, SUIT_HEARTS, SUIT_SPADES, -1};
    return s[strain];
}

/* a deal on the score sheet */
typedef struct {
    uint8_t dealer, declarer, level, strain, dbl, taken;   /* dbl 0, 1 doubled, 2 redoubled; taken: the declarer's
                                                             * side's tricks */
    uint8_t vul;                                /* bit s: side s was vulnerable */
    uint8_t game;                               /* rubber: the side that won a game with this deal, + 1 (0: none) */
    uint8_t rubber;                             /* rubber: the side that won the rubber with it, + 1 */
    int16_t below[2], above[2];                 /* the trick points (below the line), the rest (above) */
    int16_t honours[2];                         /* rubber: the honours (in above too) */
    int16_t bonus;                              /* rubber: the rubber's bonus (in above too) */
} brg_deal;

typedef struct {
    uint8_t variant, phase, turn, dealer;
    uint8_t deals;                              /* the deals scored */
    uint16_t dealt;                             /* every deal so far (passed out too): the shuffles */
    uint8_t hand[BRG_P][BRG_HAND], n[BRG_P], orig[BRG_P][BRG_HAND];   /* orig: as dealt (sorted) */
    uint8_t call[BRG_CALLS];                    /* the auction so far, from the dealer */
    uint16_t ncalls;
    uint8_t bid, bidder, dbl, passes;           /* the last bid (0: none), its player, doubled; passes in a row */
    uint8_t first[2][BRG_STRAINS];              /* the first player of each side who named each strain */
    uint8_t declarer, level, strain, contract_dbl;   /* the contract (level 0: none yet) */
    uint8_t trick[BRG_P], ntrick, leader;
    uint8_t last[BRG_P], last_leader, last_winner, tricks;
    uint8_t won[BRG_P];                         /* the tricks each player won this deal */
    uint8_t keeper[2];                          /* who keeps each side's tricks in front of them: the declarer; the
                                                 * defender who won the defenders' first trick (BRG_NONE: none yet) */
    uint8_t passed_out;                         /* BRG_SCORE: the deal was passed out */
    uint8_t games[2];                           /* rubber: games won */
    int16_t part[2];                            /* rubber: below the line in the game being played */
    int32_t total[2];
    brg_deal rec[BRG_DEALS];
    uint8_t nrec;
    uint32_t seed;
} brg_pos;

typedef struct {
    brg_pos pos;
    uint32_t seed;
    uint16_t act[BRG_MAX_ACTS];
    int n;
} brg_game;

void brg_new(brg_game *g, int variant, uint32_t seed);
int  brg_legal(const brg_pos *p, int act);
int  brg_play(brg_game *g, int act);            /* 0: not allowed */
int  brg_undo(brg_game *g);                     /* back one action (a replay from the deal); 0: none */
int  brg_can_call(const brg_pos *p, int call);
int  brg_can_play(const brg_pos *p, int card);
int  brg_dummy(const brg_pos *p);               /* the dummy (BRG_NONE: no contract) */
int  brg_dummy_shown(const brg_pos *p);         /* 1: the opening lead is made, the dummy lies face up */
int  brg_controller(const brg_pos *p);          /* who holds the device: the player to move, or the declarer for the
                                                 * dummy */
int  brg_vulnerable(const brg_pos *p, int side);   /* in the deal being played */
int  brg_winner(const brg_pos *p);              /* BRG_OVER: the winning side (0 or 1; 2: a draw; -1: not over) */
/* a contract's score (the international laws). Made: the trick points (below), the declarer's side's other points
 * (overtricks, the insult, slams; duplicate: the game or part-score bonus). Down: the defenders' (*above_def). */
void brg_score(int level, int strain, int dbl, int vul, int taken, int duplicate, int *below, int *above_decl,
               int *above_def);
int  brg_honours(const uint8_t hand[BRG_HAND], int n, int strain);   /* 150, 100 or 0 */
int  brg_auto(const brg_pos *p);                /* a plain action (the "." of the notation) */
int  brg_script(brg_game *g, const char *moves);   /* 0: an action is bad */

#endif
