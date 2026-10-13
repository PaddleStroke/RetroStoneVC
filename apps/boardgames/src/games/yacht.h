/*
 * Yacht, the traditional five-dice category game, for one to six players on one device: the rules (the throws, the
 * dice kept, the categories and their points, the bonuses, the end). No SDL, no dice: the caller throws and gives
 * the rules the values. Three variants:
 * - Classic: thirteen categories. Ones to sixes (the sum of that face); a bonus of 35 when they make 63 or more;
 *   three of a kind and four of a kind (the sum of all five dice); full house (three and two) 25; small straight
 *   (four in a row) 30; large straight (five in a row) 40; five of a kind 50; chance (the sum).
 *   Extra five of a kind: when the five-of-a-kind box already holds 50, every further five of a kind scores a bonus
 *   of 100. The joker rule (forced): once the five-of-a-kind box is filled (50 or 0), a five of a kind must go in
 *   its face's upper box if that is empty; else in any empty lower box, where full house, small and large straight
 *   score in full; else in any empty upper box (0).
 * - Nordic (Scandinavian Yatzy): fifteen categories. Ones to sixes; a bonus of 50 at 63 or more; one pair, two pairs
 *   (two different), three of a kind, four of a kind (the dice that make them); small straight 1-2-3-4-5: 15; large
 *   straight 2-3-4-5-6: 20; full house (three and two of different faces): the sum; chance; yatzy 50.
 * - Generala (Latin America): eleven categories. Ones to sixes; straight (1-2-3-4-5, 2-3-4-5-6 or 3-4-5-6-1) 20; full
 *   house 30; four of a kind (poker) 40; generala (five of a kind) 50; double generala 100 (a second five of a kind,
 *   only once the generala box holds 50; else it can only be scratched, 0). Made on the turn's first throw
 *   ("served"): straight, full house and poker score 5 more, and a served generala wins the game at once.
 *   No upper bonus.
 * A turn: up to three throws; before the second and the third, any dice may be kept (the others thrown again). Then
 * the player scores the dice in one empty category (0 is allowed: a scratch). The game ends when every player has
 * filled every category: the highest total wins (equal highest: a draw). Alone, a total of YA_SOLO_GOAL or more
 * (classic, nordic; generala: YA_SOLO_GENERALA, or a served generala) wins.
 * Notation (script, the screenshots and the tests): a throw "d" and the five dice in their places, a digit for a die
 * thrown, "-" for a die kept ("d33215", then "d33-6-"); a score: the category's name ("1".."6", "p" pair, "pp" two
 * pairs, "3k", "4k", "fh" full house, "ss" small straight, "ls" large straight, "c" chance, "y" five of a kind /
 * yatzy / generala, "st" generala's straight, "yy" double generala): "d33215 d33-6- d33-3- 3".
 */
#ifndef YACHT_H
#define YACHT_H

#include <stdint.h>

#define YA_DICE 5
#define YA_PLAYERS 6
#define YA_THROWS 3                             /* throws in a turn */
#define YA_MAX_CATS 15
#define YA_MAX_EVENTS (YA_PLAYERS * YA_MAX_CATS * (YA_THROWS + 1))
#define YA_THROW_EV 0x8000u                     /* an event: a throw (| the five faces, 3 bits each), else a score
                                                 * (the category) */
#define YA_SOLO_GOAL 200
#define YA_SOLO_GENERALA 150

enum { YA_CLASSIC, YA_NORDIC, YA_GENERALA, YA_VARIANTS };
enum {
    YC_ONES, YC_TWOS, YC_THREES, YC_FOURS, YC_FIVES, YC_SIXES,
    YC_PAIR, YC_TWO_PAIRS, YC_THREE_KIND, YC_FOUR_KIND, YC_FULL, YC_SMALL, YC_LARGE, YC_STRAIGHT, YC_CHANCE, YC_YACHT,
    YC_DOUBLE, YC_N
};

typedef struct {
    int16_t box[YA_PLAYERS][YC_N];              /* the points in each category; -1: empty */
    uint8_t extra[YA_PLAYERS];                  /* classic: the extra fives of a kind (100 each) */
    uint8_t dice[YA_DICE];                      /* the faces (1..6; 0 before the first throw) */
    uint8_t kept;                               /* bit i: die i kept (not thrown again) */
    uint8_t variant, players, turn, throws;     /* throws: in this turn (0..3) */
    uint8_t over, served;                       /* served: the game won by a served generala */
    int8_t winner;                              /* -1: none (playing, a draw, alone) */
    uint16_t turns;                             /* turns played (scored) since the start */
} ypos;

/* A game: the position before every event, so undo is free. */
typedef struct {
    ypos pos, past[YA_MAX_EVENTS];
    uint16_t ev[YA_MAX_EVENTS];
    int n;
} ygame;

void ya_new(ygame *g, int variant, int players);
int  ya_cats(int variant, int *cats);           /* the variant's categories, in the sheet's order; how many */
int  ya_is_upper(int cat);                      /* ones..sixes */
int  ya_bonus_at(int variant);                  /* the upper bonus: what it is worth (0: none) */
#define YA_BONUS_NEEDS 63
/* the points of the dice in a category by the variant's rules, nothing else (no joker, not served) */
int  ya_base(int variant, int cat, const uint8_t dice[YA_DICE]);
int  ya_five(const uint8_t dice[YA_DICE]);      /* 1: five of a kind */
/* the player to move scoring the dice now in cat: the points (joker, served), or -1: not allowed (filled, before
 * a throw, the joker forbids it, over) */
int  ya_points(const ypos *p, int cat);
int  ya_throw(ygame *g, unsigned kept, const int values[YA_DICE]);  /* the faces of the dice not kept; 0: not now;
                                                                     * 1: thrown; 2: thrown, the game won */
int  ya_keep(ygame *g, unsigned kept);          /* which dice to keep for the next throw; 0: not now */
int  ya_score(ygame *g, int cat);               /* 0: not allowed; 1: scored; 2: scored, the game over */
int  ya_upper(const ypos *p, int player);       /* ones..sixes added up */
int  ya_bonus(const ypos *p, int player);       /* the upper bonus earned (0: not, or not yet) */
int  ya_total(const ypos *p, int player);
int  ya_filled(const ypos *p, int player);      /* the categories filled */
int  ya_undo(ygame *g);                         /* back to before the last score (the throws after it dropped);
                                                 * 0: none */
int  ya_cat_by_name(const char *name, int len); /* the notation's names; -1: none */
int  ya_script(ygame *g, const char *moves);    /* plays throws and scores in the notation; 0: one is bad */

/* the bot (yacht_bot.c, bots.h) */
const ygame *yacht_live(void);          /* the game on the screen, read-only: the bot copies its position at entry */
/* main thread: plays a move as taps do (animated): "d--?-?" keeps the dice marked "-" and throws the others (the
 * game's own faces, "?"), a category's name scores it; 0: not now, or not legal */
int  yacht_show(const char *move);

#endif
