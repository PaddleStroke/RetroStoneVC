/*
 * Poker Run, a single-player poker roguelite: the rules. No SDL; a run is its seed and its actions (the shell's record
 * replays the taps; script() and the tests replay the actions).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A run is a series of blinds, three to an ante (small, big, boss), each a score to reach. In a blind: a hand of 8
 * cards from your own 52-card deck, 4 hands and 3 discards. Play up to 5 cards as a poker hand: its score is (the hand
 * type's chips + the scoring cards' chips) x the hand type's mult, changed by the cards' enhancements and the charms
 * (up to 5, passive, left to right). Reach the target before the hands run out, or the run is over. Between blinds,
 * coins (the blind's reward, a coin for each hand left, interest: one for each 5 coins held, at most 5) buy charms,
 * tomes (a hand type up a level), enhancements for your cards and packs (one of three) in the shop. A boss blind has
 * a twist. Beat the boss of ante 8 to win; the run may go on (endless) after.
 *
 * All numbers are integers (the mult too: a times-mult is rounded), so a run is the same on every device.
 *
 * Notation (script; the screenshots, the tests): b begin the blind; p<hex> play the hand's cards (bit i: card i as
 * shown); d<hex> discard them; o0 / o1 sort the hand by rank / by suit; c collect the coins; k<i> buy shop item i;
 * x<i> sell charm i; m<i><j> move charm i to place j; r reroll the shop; n leave the shop; q<i> take a pack's choice i; s skip the pack; e<hex> enhance
 * the cards chosen (bit i of the cards shown); w go on (endless) after the win; f finish the run there. "#<n>": the
 * bot plays n actions ("#" one); "#b", "#B", "#s", "#c": the bot plays to the next blind, boss blind, shop, coins
 * counted ("#w": to the win; all stop at the win or the end).
 */
#ifndef POKERRUN_H
#define POKERRUN_H

#include <stdint.h>

#include "cards.h"

#define PKR_DECK_MAX 96
#define PKR_HAND_MAX 12
#define PKR_PLAY_MAX 5
#define PKR_CHARMS 5
#define PKR_SHOP 5                              /* three items, then two packs */
#define PKR_SHOP_ITEMS 3
#define PKR_PACK_MAX 4
#define PKR_PICK 8                              /* the cards offered to an enhancement */
#define PKR_STEPS 160
#define PKR_MAX_ACTS 6000
#define PKR_WIN_ANTE 8
#define PKR_NONE 255

/* the hand types, weakest first */
enum { HT_HIGH, HT_PAIR, HT_TWO, HT_TRIPS, HT_STRAIGHT, HT_FLUSH, HT_FULL, HT_QUADS, HT_SFLUSH, HT_N };
/* the cards' enhancements */
enum { EN_NONE, EN_GILDED, EN_CRYSTAL, EN_IRON, EN_CLOVER, EN_ETCHED, EN_EMBER, EN_PRISM, EN_N };
/* the charms */
enum {
    CH_RUBY, CH_JET, CH_TOPAZ, CH_JADE, CH_PEBBLE, CH_FEATHER, CH_LENS, CH_COMPASS, CH_SEAL, CH_BELL,
    CH_MIRROR, CH_KNOT, CH_LADDER, CH_SHELL, CH_PINES, CH_CANDLE, CH_PALM, CH_WORRY, CH_ANCHOR, CH_LAST,
    CH_WATER, CH_SAND, CH_MOTH, CH_PURSE, CH_BREATH, CH_SNOW, CH_SCRAP, CH_IVY, CH_TALLY, CH_POCKET,
    CH_GLOVE, CH_SHEARS, CH_DOOR, CH_PIG, CH_LEDGER, CH_HAGGLE, CH_CLOVER, CH_RAG, CH_HAMMER, CH_GONG,
    CH_N
};
/* the bosses */
enum { BO_FADE, BO_ROPE, BO_FIST, BO_WELL, BO_WALL, BO_FOG, BO_TIDE, BO_GATE, BO_TOLL, BO_RUST, BO_N };
/* what a charm does, for its picture's rim: chips, mult, times mult, coins, the rules */
enum { CK_CHIPS, CK_MULT, CK_XMULT, CK_COINS, CK_RULE };
/* the phases */
enum { PH_BLIND, PH_PLAY, PH_CASH, PH_SHOP, PH_PACK, PH_ENH, PH_WON, PH_LOST, PH_DONE };
/* the shop's items */
enum { IT_NONE, IT_CHARM, IT_TOME, IT_ENH, IT_PACK };
enum { PK_CHARM, PK_TOME, PK_CARD, PK_N };      /* the packs */

/* the actions: kind << 12 | argument */
enum { A_BEGIN = 1, A_PLAY, A_DISCARD, A_SORT, A_CASH, A_BUY, A_SELL, A_REROLL, A_LEAVE, A_PICK, A_SKIP, A_ENH,
       A_ENDLESS, A_FINISH, A_MOVE };          /* A_MOVE: from * 8 + to (a charm moved along the row) */
#define PKR_ACT(k, a) ((uint16_t)((k) << 12 | ((a) & 0xfff)))
#define PKR_KIND(x) ((x) >> 12)
#define PKR_ARG(x) ((x) & 0xfff)

typedef struct { uint8_t card, enh; } pkr_card;  /* card: cards.h's byte (DECK_FRENCH) */
typedef struct { uint8_t id; uint8_t price; int16_t val; } pkr_charm;   /* val: a growing charm's count */
typedef struct { uint8_t kind, id, price, sold; pkr_card c; } pkr_item;  /* id: a charm, a hand type, an enh, a pack */

/* one step of a hand's scoring, for the animation: what added what, and the chips and mult after it */
enum { SS_BASE, SS_CARD, SS_HELD, SS_CHARM, SS_DEBUFF, SS_AGAIN, SS_BREAK, SS_COIN };
enum { SV_CHIPS = 1, SV_MULT, SV_XMULT, SV_COINS, SV_NONE };
typedef struct {
    uint8_t kind, what;                         /* SS_*, SV_* */
    int8_t idx;                                 /* the played card (SS_CARD, SS_DEBUFF, SS_AGAIN, SS_BREAK: its place
                                                 * among the played), the held card (SS_HELD: its place in the hand
                                                 * after the play's cards left), the charm (SS_CHARM) */
    uint8_t src;                                /* SS_CARD: 0 the rank, 1 the enhancement, 2+ a charm (src - 2) */
    int32_t value;                              /* the amount (SV_XMULT: in hundredths) */
    int64_t chips, mult;                        /* after the step */
} pkr_step;

typedef struct {
    uint8_t type, level, n;                     /* the hand type, its level, how many cards played */
    uint8_t played[PKR_PLAY_MAX];               /* their deck indexes, in the hand's order */
    uint8_t scoring;                            /* bit i: played card i scores */
    uint8_t broke;                              /* bit i: played card i broke (a crystal) */
    int nsteps;
    pkr_step step[PKR_STEPS];
    int64_t chips, mult, total;
    int coins;                                  /* coins gained (or lost) by the hand */
} pkr_log;

typedef struct {
    uint32_t rng;
    uint8_t phase, ante, blind, boss, boss_suit, endless, won, sort;
    int32_t coins;
    pkr_card deck[PKR_DECK_MAX];
    uint8_t ndeck;
    /* the blind */
    uint8_t draw[PKR_DECK_MAX], ndraw;          /* the draw pile (deck indexes, its top last) */
    uint8_t hand[PKR_HAND_MAX], nhand;          /* the hand (deck indexes, as shown) */
    uint8_t hid[PKR_DECK_MAX];                  /* the fog's cards dealt face down (by deck index) */
    uint8_t hands_left, discards_left, hand_size, hands_played, discards_used;
    int64_t score, target;
    /* the run */
    uint8_t level[HT_N];
    uint16_t played_n[HT_N];
    pkr_charm charm[PKR_CHARMS];
    uint8_t ncharms;
    pkr_item shop[PKR_SHOP];
    uint8_t rerolls;
    pkr_item pack[PKR_PACK_MAX];
    uint8_t npack, pack_kind;
    uint8_t pick[PKR_PICK], npick, pick_enh, pick_max;   /* an enhancement's cards offered (deck indexes) */
    uint8_t boss_order[BO_N], boss_next;        /* the bosses to come, shuffled */
    /* the cash-out: the blind's reward, the hands left, interest, the gilded cards, the charms */
    int16_t cash_blind, cash_hands, cash_interest, cash_gilded, cash_charms;
    int64_t best_hand;                          /* the best hand's score this run */
    uint8_t blinds_beaten;
    uint64_t seen;                              /* the charms seen this run (bit: id) */
    pkr_log log;                                /* the last hand played */
} pkr_state;

typedef struct {
    pkr_state s;
    uint32_t seed;
    uint16_t act[PKR_MAX_ACTS];
    int n;
} pkr_game;

/* the tables */
typedef struct { uint8_t kind, rarity, price; } pkr_charm_info;
extern const pkr_charm_info pkr_charms[CH_N];
extern const int16_t pkr_hand_base[HT_N][4];    /* chips, mult, chips a level, mult a level */

/* hand evaluation: n cards (1..5; suits as masks, bit s: suit s; ranks 1..13), the type and the scoring cards */
int  pkr_eval(const uint8_t *rank, const uint8_t *suits, int n, uint8_t *scoring);
int  pkr_rank_chips(int rank);                  /* 2..10, faces 10, ace 11 */
int  pkr_is_face(int rank);
int64_t pkr_target(int ante, int blind, int boss);
int  pkr_hand_chips(const pkr_state *s, int type);
int  pkr_hand_mult(const pkr_state *s, int type);
int  pkr_has(const pkr_state *s, int charm);    /* how many of that charm are equipped */
int  pkr_price(const pkr_state *s, const pkr_item *it);   /* with the haggler */
int  pkr_reroll_cost(const pkr_state *s);
int  pkr_sell_value(const pkr_charm *c);
int  pkr_interest(const pkr_state *s);          /* the interest for the coins held now */

void pkr_new(pkr_game *g, uint32_t seed);
int  pkr_legal(const pkr_state *s, int act);
int  pkr_play(pkr_game *g, int act);            /* 0: not allowed */
int  pkr_apply(pkr_state *s, int act);          /* the same on a state alone (no history) */
/* the score a play would make (mask: the hand's cards), without playing it; the log filled (log may be NULL). The
 * clover's chances are drawn from a copy of the random state, as the play would draw them. */
int64_t pkr_preview(const pkr_state *s, unsigned mask, pkr_log *log);
int  pkr_preview_type(const pkr_state *s, unsigned mask);   /* the hand type of the cards chosen (-1: none) */
int  pkr_over(const pkr_state *s);              /* 1: the run is over (lost, or finished after a win) */
int  pkr_script(pkr_game *g, const char *moves);   /* 0: an action is bad */
int  pkr_notation(int act, char *buf, int cap);
int  pkr_bot(const pkr_state *s);               /* a sensible action (0: none) */
void pkr_card_suits(const pkr_state *s, int deck_index, uint8_t *rank, uint8_t *suits);   /* as the eval sees it */

#endif
