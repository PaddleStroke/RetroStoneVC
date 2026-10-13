/*
 * Durak (the fool), the Russian card game, for 2 to 6 players on one device passed round: the rules. No SDL; the deal
 * comes from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 36 cards, 6 up to the ace (6 low .. 10, jack, queen, king, ace high). Six cards each; the next card of the stock
 * (the talon) is turned up and laid under it: its suit is the trump, and it is the talon's last card. The player with
 * the lowest trump attacks first, the next player (in turn order) defends.
 * A bout: the attacker leads one card, or several of one rank. The defender beats each attacking card with a higher
 * card of its suit, or with a trump (a trump only with a higher trump), or takes. Once every card on the table is
 * beaten, the throwers may throw in cards of a rank already on the table: the attacker first, then each other player
 * in turn (all but the defender), one turn each, throwing in one or more cards or passing; the defender then beats
 * them, and the round of throwers starts again from the attacker. A bout holds at most 6 attacking cards, and no more
 * than the defender held when it began. When every thrower has passed in a row with every card beaten, the bout is
 * beaten off ("bito"): its cards go out of play, and the defender attacks next. A defender who takes says so; each
 * thrower then has one more turn to throw in cards (to the same limit) and the defender takes them all, and loses the
 * turn to attack: the player after them attacks.
 * After each bout the players draw back up to six from the talon: the attacker first, then the other throwers in
 * order, the defender last. Once the talon is empty, a player with no cards is out (safe). The last player still
 * holding cards is the durak, the fool, and loses; the others share the win. All going out at once: a draw.
 * Perevodnoy (the transfer variant): before beating any card, the defender may pass the attack on by adding a card of
 * the rank of the attacking cards (all of one rank), if the next player holds at least as many cards as the attack then
 * has; that next player defends, and the one who passed it on becomes the attacker.
 * Notation (script): a card "7s" ("6..9, T, J, Q, K, A" and "s h d c"); an attack or a throw-in: its cards in a row
 * ("7s7h"); a defence: the card ("Ks": it beats the first open card it can beat; "Ks/7s": that one); "x<card>" passes
 * the attack on; "t" takes; "p" passes (a thrower); "." one plain action; "#" plain actions to the end of the game.
 */
#ifndef DURAK_H
#define DURAK_H

#include <stdint.h>

#include "cards.h"

#define DUR_MAXP 6
#define DUR_CARDS 36
#define DUR_HAND 36
#define DUR_FULL 6                              /* the hand drawn back up to six; six attacking cards at most */
#define DUR_MAX_ACTS 4096
#define DUR_NONE 255

enum { DUR_CLASSIC, DUR_TRANSFER, DUR_VARIANTS };   /* podkidnoy (throw-in), perevodnoy (transfer) */
enum { DP_ATTACK, DP_DEFEND, DP_THROW, DP_OVER };

/* an action: its kind (top byte), a card (a defence: the card and the attacking slot), or a set of cards (bit i: the
 * card dur_card(i)) */
#define DA_PLAY (1ull << 56)                    /* lead or throw in: the mask */
#define DA_BEAT (2ull << 56)                    /* the card (bits 0..7), on the slot (bits 8..15) */
#define DA_TAKE (3ull << 56)
#define DA_PASS (4ull << 56)
#define DA_SHIFT (5ull << 56)                   /* perevodnoy: the card added */
#define DA_KIND(a) ((a) & (0xffull << 56))
#define DA_MASK(a) ((a) & ((1ull << DUR_CARDS) - 1))
#define DA_CARD(a) ((int)((a) & 0xff))
#define DA_SLOT(a) ((int)((a) >> 8 & 0xff))
#define DA_BEATS(card, slot) (DA_BEAT | (uint64_t)(slot) << 8 | (uint64_t)(card))

typedef struct {
    uint8_t players, variant, phase, turn;
    uint8_t hand[DUR_MAXP][DUR_HAND], n[DUR_MAXP];
    uint8_t talon[DUR_CARDS], ntalon;           /* talon[0] the turned-up trump card (drawn last); drawn from the top */
    uint8_t trump_card, trump;
    uint8_t attacker, defender, start_n;        /* start_n: the defender's cards when the bout began */
    uint8_t att[DUR_FULL], def[DUR_FULL], natt; /* the bout: attacking cards, their beaters (DUR_NONE: open) */
    uint8_t taking, passed;                     /* the defender takes; bit q: q has had their turn in this round of
                                                 * throwers (passed; taking: passed or thrown in) */
    uint8_t discards, bouts;                    /* cards beaten off; bouts over */
    uint8_t out[DUR_MAXP], nout, fool;          /* the players out, in order; the durak (DUR_NONE: none yet, a draw) */
    uint8_t last_end, last_by;                  /* the last bout: 1 beaten off, 2 taken; by whom (the defender) */
    uint32_t seed;
} dpos;

typedef struct {
    dpos pos;
    uint32_t seed;
    uint8_t players, variant;
    uint64_t act[DUR_MAX_ACTS];
    int n;
} dgame;

static inline int dur_strength(int card) { return CARD_RANK(card) == RANK_ACE ? 14 : CARD_RANK(card); }
static inline int dur_index(int card)           /* 0..35 */
{
    return CARD_SUIT(card) * 9 + (CARD_RANK(card) == RANK_ACE ? 8 : CARD_RANK(card) - 6);
}
static inline int dur_card(int i) { return CARD(i / 9, i % 9 == 8 ? RANK_ACE : i % 9 + 6); }

void dur_new(dgame *g, int variant, int players, uint32_t seed);
int  dur_legal(const dpos *p, uint64_t act);
int  dur_play(dgame *g, uint64_t act);          /* 0: not allowed */
int  dur_undo(dgame *g);                        /* back one action (a replay from the deal); 0: none */
int  dur_beats(const dpos *p, int card, int attack);   /* card beats attack */
int  dur_can_throw(const dpos *p, int card);    /* its rank is on the table (a throw-in) */
int  dur_room(const dpos *p);                   /* attacking cards the bout may still take */
int  dur_can_shift(const dpos *p, int card);    /* perevodnoy: the defender may pass the attack on with it */
int  dur_active(const dpos *p, int player);     /* still in the game (holds cards, or may still draw) */
int  dur_next(const dpos *p, int player);       /* the next player still in */
int  dur_open(const dpos *p);                   /* attacking cards not beaten yet */
unsigned dur_winners(const dpos *p);            /* DP_OVER: everyone but the durak (all: a draw) */
void dur_sort(const dpos *p, uint8_t *cards, int n);   /* a hand for the eye: by suit, the trumps last; low to high */
uint64_t dur_auto(const dpos *p);               /* a plain action (the "." of the notation) */
int  dur_script(dgame *g, const char *moves);   /* 0: an action is bad */

/* the bot (durak_bot.c, bots.h) */
const dgame *durak_live(void);                  /* the game on the screen, read-only: the bot copies it at entry */
int durak_show(const char *move);               /* main thread: plays one action of the notation as a tap does */
int durak_open_hands(void);                      /* the hands face up for all (the variants with open hands) */
struct ai_stats;
uint64_t durb_choose(const dgame *g, int seat, int open, int level, uint64_t seed, struct ai_stats *stats);

#endif
