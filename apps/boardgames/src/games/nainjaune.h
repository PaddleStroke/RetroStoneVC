/*
 * Le Nain jaune (the Yellow Dwarf), the French card game of the board with five compartments, for 3 to 6 players on
 * one device: the rules. No SDL; each deal comes from a seed (cards.h's shuffle), so a game is its seed and its
 * actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board's five compartments: the ten of diamonds, the jack of clubs, the queen of spades, the king of hearts, and
 * in the middle the seven of diamonds, the yellow dwarf. Each player starts with 100 chips. At each deal, every player
 * stakes 1 chip on the ten, 2 on the jack, 3 on the queen, 4 on the king and 5 on the seven (15 in all).
 * The deal: the 52 cards, dealt one at a time from the dealer's left (the dealer moves one seat round the table each
 * deal; player 0 leads the first); 15 cards each for 3 players, 12 for 4, 9 for 5, 8 for 6; the cards left over
 * (7, 4, 7, 4) are the talon, face down, out of play.
 * The play: ace (1) low, king (13) high, suits do not count. The player on the dealer's left leads any card, then goes
 * on with the next rank up (an ace, a two, a three...) as long as they hold it; when they do not ("without four"),
 * the next player round the table who holds it goes on, and so on. A king ends the sequence: who played it leads
 * again. When nobody holds the next rank, the last to have played leads again. Holding the next rank, you must play
 * it (one of them, your choice); the players who cannot are passed over at once (they would say "without").
 * Playing one of the five cards takes all the chips of its compartment.
 * The first player with no cards left ends the deal: every other player pays them one chip per card still in their
 * hand, and pays its stake again (1 to 5) into the compartment of each of the five cards they still hold. The chips
 * nobody took stay in the compartments for the next deal. Grand opera (NJ_CLASSIC; not in NJ_NO_OPERA): a player who
 * goes out before any other player has played a card in the deal also takes every compartment.
 * A player who cannot pay pays what they have. The game ends when every player has dealt twice, or sooner when a
 * player cannot stake the 15 chips of a new deal; the most chips wins (a tie: shared).
 * Notation (script; the screenshots and the tests): a card, its rank and suit ("7d", "td" the ten, "jc", "qs", "kh",
 * "ah"), played by the player to move; "n" the next deal; "." one plain play (the lowest card it may play), "#" plain
 * plays to the end of the deal.
 */
#ifndef NAINJAUNE_H
#define NAINJAUNE_H

#include <stdint.h>

#include "cards.h"

#define NJ_MAXP 6
#define NJ_HAND 15
#define NJ_CHIPS 100                            /* each player's chips at the start */
#define NJ_STAKES 15                            /* 1 + 2 + 3 + 4 + 5 a deal */
#define NJ_MAX_ACTS 2048
#define NJ_NEXT 254                             /* the action: the next deal */

enum { NJ_TEN, NJ_JACK, NJ_QUEEN, NJ_KING, NJ_SEVEN, NJ_BOXES };   /* the compartments; stake: index + 1 */
extern const uint8_t nj_box_card[NJ_BOXES];
enum { NJ_CLASSIC, NJ_NO_OPERA, NJ_VARIANTS };
enum { NP_PLAY, NP_DEALT, NP_OVER };            /* NP_DEALT: a deal is over, its summary shows */

typedef struct {
    uint8_t players, variant, phase, turn, dealer, deals, lead;   /* deals: dealt so far; lead: who leads (NP_PLAY,
                                                                   * need 0) */
    uint8_t need;                               /* the rank to play next; 0: any card (a new sequence) */
    uint8_t last;                               /* who played last */
    uint8_t hand[NJ_MAXP][NJ_HAND], n[NJ_MAXP];
    uint8_t talon[NJ_HAND], ntalon;
    uint8_t played[52], nplayed;                /* the cards played this deal, in order */
    uint8_t seq;                                /* where the current sequence starts in played[] */
    uint16_t played_by;                         /* bit p: player p has played a card this deal */
    int16_t chips[NJ_MAXP], box[NJ_BOXES];
    /* the last deal's settling (the summary): who went out, the cards paid (+ the winner's take), the compartments
     * taken, the stakes paid again for the five cards still held; grand opera */
    int8_t out;
    int16_t got_cards[NJ_MAXP], got_boxes[NJ_MAXP], paid_again[NJ_MAXP];
    uint8_t opera;
    uint32_t seed;
} njpos;

typedef struct {
    njpos pos;
    uint32_t seed;
    uint8_t variant, players;
    uint8_t act[NJ_MAX_ACTS];
    int n, floor;                               /* floor: undo stops there (the device passed, a deal began) */
} njgame;

void nj_new(njgame *g, int variant, int players, uint32_t seed);
int  nj_box_of(int card);                       /* the compartment of one of the five cards, or -1 */
int  nj_legal(const njpos *p, int act);
int  nj_play(njgame *g, int act);               /* a card of the player to move, or NJ_NEXT; 0: not allowed */
int  nj_undo(njgame *g);                        /* back one action (a replay), not past the floor; 0: none */
int  nj_auto(const njpos *p);                   /* a plain action: the lowest card it may play, or NJ_NEXT */
unsigned nj_winners(const njpos *p);            /* NP_OVER: the most chips (bit n: player n) */
int  nj_cards_each(int players);
int  nj_deals(int players);                     /* the deals in a game */
int  nj_script(njgame *g, const char *moves);   /* 0: an action is bad */

#endif
