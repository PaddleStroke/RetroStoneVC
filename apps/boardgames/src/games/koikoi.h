/*
 * Koi-Koi, the hanafuda game for 2 players on one device: the rules. No SDL; the deal comes from a seed (cards.h's
 * shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The deck: 48 cards, 12 months of 4 (HANA(month 0..11, index 0..3); the index orders a month's cards from the best:
 * bright or animal, then ribbon or animal, then the plains). 5 brights (January's crane, March's curtain, August's
 * moon, November's rain man, December's phoenix), 9 animals, 10 ribbons (3 poetry ribbons, red with writing: January,
 * February, March; 3 blue: June, September, October; 4 plain red: April, May, July, November), 24 plains (November's
 * lightning among them).
 * The deal: 8 cards to each player, 8 face up on the field (four of one month on the field: dealt again), the rest the
 * stock. The dealer plays first; the first dealer is player 0, then whoever won the last round (a drawn round: the
 * same dealer).
 * A turn: play a card of your hand on the field. A field card of the same month: both are captured; two of them: you
 * choose one; three: all four are captured; none: the card stays on the field. Then the top card of the stock is
 * turned and played the same way.
 * The yaku (役): Goko, the 5 brights, 10; Shiko, 4 brights without the rain man, 8; Ame-Shiko, 4 with him, 7; Sanko,
 * 3 brights without him, 5; Inoshikacho, boar, deer and butterflies, 5; Akatan, the 3 poetry ribbons, 5; Aotan, the 3
 * blue ribbons, 5; Hanami-zake, the curtain and the sake cup, 5; Tsukimi-zake, the moon and the sake cup, 5 (both
 * always count); Tane, 5 animals, 1 and 1 more for each one after; Tan, 5 ribbons, the same; Kasu, 10 plains, the
 * same. The sake cup is an animal and a plain too. Only the best of the four bright yaku counts; the others add up.
 * Koi-koi: whoever forms a yaku, or raises their yaku's points, at the end of their turn stops (the round ends, they
 * score) or calls koi-koi (the round goes on; to stop later they must raise their points again). With no card left in
 * hand, they stop. The points: the yaku added up, doubled at 7 or more, doubled again if the other player had called
 * koi-koi. Both hands played out with nobody stopping: nobody scores.
 * Teshi (four cards of a month in the hand dealt) and Kuttsuki (four pairs of months in it): the round is won at once,
 * 6 points (the dealer's hand looked at first).
 * The game: 12 rounds, a month each (KK_FULL), or 6 (KK_HALF). The most points win.
 * Notation (script; the screenshots and the tests): a card is its month 1..12 and a..d (its index): "1a" the crane,
 * "8a" the moon. p<card> play from the hand, t<card> take this field card (a choice), d turn the stock's top card, k
 * koi-koi, s stop, n the next round; "." plays one plain action (the first card, the first match, stop), "#" plain
 * actions to the end of the round, "*" to the end of the game.
 */
#ifndef KOIKOI_H
#define KOIKOI_H

#include <stdint.h>

#include "cards.h"

#define KK_CARDS 48
#define KK_HAND 8
#define KK_FIELD 24                             /* the field's places (cards keep their place; holes are filled) */
#define KK_NONE 255
#define KK_MAX_ACTS 2048
#define KK_INSTANT 6                            /* teshi, kuttsuki */
#define HANA(month, index) ((uint8_t)((month) * 4 + (index)))
#define HANA_MONTH(c) ((c) / 4)
#define HANA_INDEX(c) ((c) % 4)

enum { HT_BRIGHT, HT_ANIMAL, HT_RIBBON, HT_PLAIN, HTYPES };
enum { HR_NONE, HR_POETRY, HR_BLUE, HR_RED };
enum { M_PINE, M_PLUM, M_CHERRY, M_WISTERIA, M_IRIS, M_PEONY, M_CLOVER, M_PAMPAS, M_CHRYSANTHEMUM, M_MAPLE,
       M_WILLOW, M_PAULOWNIA };
#define KK_CRANE HANA(M_PINE, 0)
#define KK_WARBLER HANA(M_PLUM, 0)
#define KK_CURTAIN HANA(M_CHERRY, 0)
#define KK_CUCKOO HANA(M_WISTERIA, 0)
#define KK_BRIDGE HANA(M_IRIS, 0)
#define KK_BUTTERFLIES HANA(M_PEONY, 0)
#define KK_BOAR HANA(M_CLOVER, 0)
#define KK_MOON HANA(M_PAMPAS, 0)
#define KK_GEESE HANA(M_PAMPAS, 1)
#define KK_SAKE HANA(M_CHRYSANTHEMUM, 0)
#define KK_DEER HANA(M_MAPLE, 0)
#define KK_RAINMAN HANA(M_WILLOW, 0)
#define KK_SWALLOW HANA(M_WILLOW, 1)
#define KK_LIGHTNING HANA(M_WILLOW, 3)
#define KK_PHOENIX HANA(M_PAULOWNIA, 0)

int kk_type(int card);                          /* HT_* */
int kk_ribbon(int card);                        /* HR_* (HR_NONE: not a ribbon) */

/* the yaku */
enum { Y_GOKO, Y_SHIKO, Y_AMESHIKO, Y_SANKO, Y_INOSHIKACHO, Y_AKATAN, Y_AOTAN, Y_HANAMI, Y_TSUKIMI, Y_TANE, Y_TAN,
       Y_KASU, Y_TESHI, Y_KUTTSUKI, YAKU_N };
/* the yaku of these captured cards: each one's points into pts (YAKU_N, 0: not formed; may be NULL); their sum */
int kk_yaku(const uint8_t *cards, int n, int *pts);
int kk_instant(const uint8_t *hand, int n);     /* a hand dealt: Y_TESHI, Y_KUTTSUKI, or -1 */

enum { KK_FULL, KK_HALF, KK_VARIANTS };         /* 12 rounds, 6 */
enum { KP_PLAY, KP_CHOOSE, KP_DRAW, KP_DECIDE, KP_ROUND, KP_OVER };
/* an action: its kind (high byte) and a card (low byte) */
enum { KA_PLAY = 1 << 8, KA_TAKE = 2 << 8, KA_DRAW = 3 << 8, KA_KOI = 4 << 8, KA_STOP = 5 << 8, KA_NEXT = 6 << 8 };
#define KA_KIND(a) ((a) & 0xff00)
#define KA_CARD(a) ((a) & 0xff)

enum { KR_NONE, KR_STOP, KR_DRAW, KR_INSTANT };
typedef struct {
    uint8_t kind, winner;                       /* KR_*; the winner (KK_NONE: nobody) */
    int8_t pts[YAKU_N];                         /* the yaku that scored */
    uint8_t base, seven, koi;                   /* their sum; doubled at 7 or more; doubled for the other's koi-koi */
    int16_t points;                             /* to the winner */
} kk_result;

typedef struct {
    uint8_t rounds, round, dealer, turn, phase;
    uint8_t hand[2][KK_HAND], nh[2];            /* each hand sorted */
    uint8_t field[KK_FIELD];                    /* KK_NONE: an empty place */
    uint8_t deck[KK_CARDS], ndeck;              /* the stock, its top last */
    uint8_t cap[2][KK_CARDS], ncap[2];          /* the cards captured, in order */
    uint8_t held, from_deck;                    /* the card waiting for its match to be chosen (KP_CHOOSE) */
    uint8_t drawn;                              /* the card last turned from the stock (KK_NONE) */
    uint8_t koi[2];                             /* koi-koi calls this round */
    uint8_t base[2];                            /* the yaku points at the last call: a stop needs more */
    uint8_t redeals;
    int16_t score[2];
    kk_result res;                              /* the last round's end */
    uint32_t seed;
} kpos;

typedef struct {
    kpos pos;
    uint32_t seed;
    uint8_t variant;
    uint16_t act[KK_MAX_ACTS];
    int n;
} kgame;

void kk_new(kgame *g, int variant, uint32_t seed);
int  kk_legal(const kpos *p, int act);
int  kk_play(kgame *g, int act);                /* 0: not allowed */
int  kk_undo(kgame *g);                         /* back one action (a replay from the deal); 0: none */
int  kk_matches(const kpos *p, int card, int *slots);   /* the field's places holding cards of its month */
int  kk_points(const kpos *p, int player);      /* the player's yaku points now (not doubled) */
int  kk_winner(const kpos *p);                  /* KP_OVER: the most points (-1: a tie, or not over) */
int  kk_auto(const kpos *p);                    /* a plain action (the "#" of the notation) */
int  kk_card_name(const char *s, int *card);    /* "8a": its length (0: not a card) */
int  kk_script(kgame *g, const char *moves);    /* 0: an action is bad */

/* the bot (koikoi_bot.c, bots.h) */
const kgame *koikoi_live(void);                 /* the game on the screen, read-only: the bot copies it at entry */
int koikoi_show(const char *move);              /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
int kkb_choose(const kpos *live, int seat, int level, uint64_t seed, struct ai_stats *stats);

#endif
