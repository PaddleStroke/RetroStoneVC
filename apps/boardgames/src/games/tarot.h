/*
 * French Tarot (« Tarot »), for 3, 4 or 5 players on one device: the rules, after the French Tarot Federation's
 * (FFT). No SDL; a game is its starting pack, its seed (the cuts, where the dog's cards fall) and its actions (undo
 * replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 78 cards (cards.h DECK_TAROT). Players 0..n-1 sit in turn order (the next player is the one after: the
 * dealer's right, play going counter-clockwise as at the table). A game: as many scored deals as players (a cancelled
 * deal does not count, but the deal moves on); the highest total wins.
 * - No shuffle between deals, as at a real table: the cards are gathered as they lie, cut, and dealt in threes, so
 *   suits and trumps stay bunched from the tricks (tarot_gather, tarot_cut). The pack is listed from its top (the
 *   first card dealt):
 *   - a deal played out: each side's pile is its aside at the bottom (Petite, Garde: the taker's discard; Garde sans:
 *     the dog, the taker's; Garde contre: the dog, the defence's), then its tricks stacked as won, each trick's cards
 *     in their order of play. The Excuse lies in its owner's side's pile, in the trick it was played in; the small
 *     card given for it goes from the top of the owner's side's pile onto the other side's pile as soon as the owner's
 *     side has one (at once, or at its next trick won; never, if it wins none). Five players: still two sides. Then
 *     the defence's pile is put on the taker's: the defence's last trick is on top.
 *   - a cancelled deal (a Petit sec, all passed): each hand as held (sorted, as on the screen), from the player after
 *     the dealer round to the dealer, each put on the one before, then the dog on top. (A deal left unfinished, for a
 *     new game: the sides' piles, the trick on the table, the hands, the dog or the discard, in that order upwards.)
 *   - the cut (FFT: the dealer's left-hand neighbour, here the player before the dealer, cuts, lifting or leaving more
 *     than 3 cards): 4..74 cards lifted at a random point (the seed), the lower part put on top.
 *   - a game's first deal comes from the pack the caller gives (tarot_start): the UI keeps the last pack of the
 *     session for its next game; the session's very first one comes from tarot_warmup: a whole deal from a shuffled
 *     pack, never seen (the first player takes a Petite, the others pass, a random legal call, discard and play),
 *     gathered as above. Any number of players: always the same 78 cards.
 * - The deal: in packets of three, from the player after the dealer round the table; the dog's cards (6; 3 with five
 *   players) go to it one at a time between two packets, never with the first nor the last packet. 3 players: 24
 *   cards each; 4: 18; 5: 15. A player whose only trump is the Petit (no other trump, no Excuse: the « Petit sec »)
 *   shows it and the deal is cancelled (TP_VOID).
 * - The bidding: once round the table from the player after the dealer, each one passes or bids above the bids before:
 *   Petite (the « Prise ») x1, Garde x2, Garde sans (le chien) x4, Garde contre (le chien) x6. Everyone passes: the
 *   deal is cancelled (TP_VOID).
 * - Five players: the taker calls a king (holding all four: a queen; all the queens too: a knight; then a jack). Its
 *   holder is the taker's partner, secret until the card is played; the taker may call their own king, or the king
 *   may lie in the dog: the taker plays alone, one against four.
 * - The dog: Petite and Garde, it is turned up for all to see (TP_DOG), the taker takes it and discards as many cards
 *   (TP_DISCARD): never a king nor an oudler (the 21, the Petit, the Excuse), a trump only when they cannot do
 *   otherwise (those trumps are shown). Garde sans: the dog, unseen, counts for the taker; Garde contre: for the
 *   defence.
 * - The play: the player after the dealer leads; follow the suit led; void, trump; a trump must beat the highest trump
 *   in the trick when it can (trumps led too); no trump: any card. The Excuse may be played at any time and never
 *   wins: its owner keeps it and gives the trick's winner a card worth half a point instead (counted at the end); led,
 *   the next card sets the suit. In the last trick it goes to the trick's winner, unless its side has won every trick
 *   before: then it wins that trick (the chelem).
 * - A poignée (10, 13, 15 trumps with four players; 13, 15, 18 with three; 8, 10, 13 with five; the Excuse counts as
 *   a trump only when it completes it) may be shown just before the player's first card: the lowest trumps, exactly
 *   the number required. 20, 30, 40 points to the side that wins the contract.
 * - The count, in half points (182 in all, 91 points): an oudler or a king 4.5, a queen 3.5, a knight 2.5, a jack 1.5,
 *   any other card 0.5. The taker's side needs 56 points with no oudler, 51 with one, 41 with two, 36 with three.
 *   The deal's score: (25 + the difference) x the bid's multiplier, a half point in the difference rounded up; the
 *   Petit in the last trick (petit au bout): 10 x the multiplier to the side that wins that trick; the poignées; a
 *   side that wins every trick (an unannounced chelem): 200. Each defender pays it (or is paid it); the taker
 *   receives it from each; five players with a partner: the taker twice, the partner once. Zero-sum.
 * Notation (script; the screenshots and the tests), separated by spaces: p pass, b1..b4 a bid (Petite, Garde, Garde
 * sans, Garde contre), k<card> call, t take the dog, x<card> discard, r<card> take a discarded card back, f the
 * discard is done, g1..g3 a poignée, <card> play it, n the next deal; "." a plain action (".12": twelve), "#" plain
 * actions to the end of the deal. Cards: s, h, d, c (spades, hearts, diamonds, clubs) and 1..14 (jack 11, knight 12,
 * queen 13, king 14), a1..a21 the trumps, e the Excuse.
 */
#ifndef TAROT_H
#define TAROT_H

#include <stdint.h>

#include "cards.h"

#define TAROT_MAXP 5
#define TAROT_HAND 30                           /* 24 cards and the dog of 6, while the taker discards */
#define TAROT_NONE 255
#define TAROT_DOG 5                             /* deal_to: the dog */
#define TAROT_MAX_ACTS 16384
#define TAROT_PETIT CARD(SUIT_TRUMP, 1)
#define TAROT_21 CARD(SUIT_TRUMP, 21)

enum { TB_PASS, TB_PETITE, TB_GARDE, TB_SANS, TB_CONTRE, TB_NONE = 255 };
enum { TP_BID, TP_CALL, TP_DOG, TP_DISCARD, TP_PLAY, TP_SCORE, TP_VOID, TP_OVER };
/* an action: its kind (high byte) and its argument (a bid, a card, a poignée's level) */
enum { TA_BID = 1, TA_CALL, TA_TAKE, TA_DISCARD, TA_RETURN, TA_DONE, TA_POIGNEE, TA_PLAY, TA_NEXT };
#define TACT(kind, arg) ((kind) << 8 | (arg))
#define TA_KIND(a) ((a) >> 8)
#define TA_ARG(a) ((a) & 0xff)

typedef struct {                                /* a deal's count (TP_SCORE, TP_OVER) */
    int16_t half;                               /* the taker's side's points, in half points */
    uint8_t oudlers, oud;                       /* how many; which (bit 0 the Petit, 1 the 21, 2 the Excuse) */
    uint8_t need, made, mult;
    int16_t diff;                               /* whole points, a half rounded up */
    int16_t pab;                                /* petit au bout: +10 the taker's side, -10 the defence, 0 */
    int16_t poignee;                            /* the poignées' bonus, + when the taker's side won the contract */
    int16_t chelem;                             /* +200 the taker's side took every trick, -200 the defence */
    int16_t unit;                               /* what each defender pays (+) or is paid (-) */
} tresult;

typedef struct {
    uint8_t players, deals, handsize, dogsize;
    uint8_t phase, turn, dealer, dealno, played;   /* dealno: the deals dealt (cancelled ones too); played: scored */
    uint8_t hand[TAROT_MAXP][TAROT_HAND], nhand[TAROT_MAXP];   /* sorted (tarot_key) */
    uint8_t dog[6];
    uint8_t ecart[6], necart, aside_attack;     /* the cards put aside (the discard; the dog: Garde sans, contre),
                                                 * counted for the taker's side or not */
    uint8_t deal_to[78], deal_card[78];         /* the deal in order: who got each card (TAROT_DOG: the dog) */
    uint8_t bid[TAROT_MAXP], spoke, taker, contract;
    uint8_t called, partner, revealed;          /* five players: the called card, its holder (the taker: alone), it
                                                 * has been played */
    uint8_t sec;                                /* TP_VOID: the player with the Petit sec, or TAROT_NONE: all passed */
    uint8_t poignee[TAROT_MAXP], shown[TAROT_MAXP][18], nshown[TAROT_MAXP];
    uint8_t trick[TAROT_MAXP], ntrick, leader, tricks;   /* trick[i]: played by (leader + i) % players */
    uint8_t last[TAROT_MAXP], lastlead, lastwin;          /* the last trick completed (tricks > 0) */
    uint8_t pile[TAROT_MAXP][78], npile[TAROT_MAXP], won[TAROT_MAXP];   /* each player's tricks: cards, count */
    uint8_t xowner, xto;                        /* the Excuse kept by xowner, who owes xto's side a half point */
    uint8_t xdebt;                              /* that side still owes the other side's pile its small card */
    uint8_t heap[2][78], nheap[2];              /* each side's pile as it grows (1 the taker's side, 0 the
                                                 * defence), bottom first: the aside, then the tricks as won */
    uint8_t cutter, cut;                        /* who cut this deal's pack, the cards they lifted (0: shuffled) */
    uint8_t pab_by;                             /* the last trick's winner when the Petit is in it (TAROT_NONE) */
    tresult res;
    int16_t total[TAROT_MAXP], score[TAROT_MAXP];   /* the totals; the last deal's scores */
    uint32_t seed;
} tpos;

typedef struct {
    tpos pos;
    uint32_t seed;
    uint8_t players;
    uint8_t start[78];                          /* the pack the game began from, top first (before its cut) */
    uint16_t act[TAROT_MAX_ACTS];
    int n;
} tgame;

void tarot_start(tgame *g, int players, uint32_t seed, const uint8_t *pack);   /* the first deal from this pack,
                                                  * top first (cut first) */
int  tarot_warmup(uint8_t *pack, int players, uint32_t seed);   /* a session's first pack: a whole deal played
                                                  * from a shuffled pack, unseen, then gathered (1: played out) */
void tarot_new(tgame *g, int players, uint32_t seed);           /* the warm-up, then tarot_start */
void tarot_gather(const tpos *p, uint8_t *pack); /* the cards picked up as they lie, top first */
int  tarot_cut(uint8_t *pack, uint32_t seed);    /* the cut: 4..74 cards lifted, the lower part on top; returns
                                                  * how many were lifted */
int  tarot_legal(const tpos *p, int act);
int  tarot_play(tgame *g, int act);              /* 0: not allowed */
int  tarot_undo(tgame *g);                       /* back one action (a replay from the start); 0: none */
int  tarot_key(int card);                        /* the hands' order: spades, hearts, clubs, diamonds, the trumps,
                                                  * the Excuse */
int  tarot_oudler(int card);
int  tarot_value2(int card);                     /* in half points: 9, 7, 5, 3, 1 */
int  tarot_need(int oudlers);                    /* 56, 51, 41, 36 */
int  tarot_mult(int contract);                   /* 1, 2, 4, 6 */
int  tarot_poignee_size(int players, int level); /* the trumps a poignée needs (level 1..3) */
int  tarot_poignee_bonus(int level);             /* 20, 30, 40 */
int  tarot_trumps(const tpos *p, int player);    /* trumps in hand, the Excuse too */
int  tarot_highest(const tpos *p);               /* the highest bid so far (TB_PASS: none) */
int  tarot_call_rank(const tpos *p);             /* the rank the taker calls: king, or queen... */
int  tarot_attack(const tpos *p, int player);    /* 1: the taker or the partner (the rules know; the screen tells
                                                  * the partner only once revealed) */
int  tarot_trick_winner(const tpos *p, const uint8_t *cards, int n, int leader);   /* by the cards alone */
int  tarot_winner(const tpos *p);                /* TP_OVER: the highest total (-1: a tie, or not over) */
int  tarot_auto(const tpos *p);                  /* a plain action (the "." of the notation) */
int  tarot_script(tgame *g, const char *moves);  /* 0: an action is bad */
int  tarot_parse_card(const char **s);           /* "h14" "a21" "e": the card (CARD_NONE: bad) */

#endif
