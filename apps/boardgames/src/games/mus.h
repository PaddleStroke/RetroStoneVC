/*
 * Mus, the Basque card game, for four players in two partnerships on one device (hidden hands): the rules. No SDL; the
 * deal comes from a seed, so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Partners sit across: players 0 and 2 against 1 and 3. The Spanish pack of 40 (spanish.h), the common rules with
 * eight kings: the 3s count as kings (reyes) and the 2s as aces, everywhere. Player 3 deals first; the mano (the first
 * hand, who speaks first and wins every tie) is the next player; then both move on one seat a hand.
 * - Four cards each. Mus: in turn from the mano each says mus (to change cards) or cuts (no hay mus). Four mus: each in
 *   turn lays away 1 to 4 cards and gets as many from the stock (the cards laid away are shuffled into a new stock when
 *   it runs out, except the ones just laid away); then mus again. One cut: the lances start.
 * - The four lances, in order: Grande (the highest cards: rey, caballo, sota, 7 .. 4, as), Chica (the lowest), Pares
 *   (a pair 1, three alike, medias 2, two pairs or four alike, duples 3; higher kinds first, then higher cards), Juego
 *   (the points: a figure 10, the others their number; juego from 31: 31 the best, then 32, 40, 37, 36, 35, 34, 33),
 *   or Punto when nobody has juego (the most points). In Pares and Juego only the players who have them speak: when only
 *   one side has them there is no bet, and when nobody has them the lance is skipped.
 * - Betting (envites), in turn from the mano: pass, or bet (envido: 2, or more). Facing a bet, the next opponent (then
 *   his partner) accepts it (quiero), raises it (n more), goes for the whole game (ordago) or refuses (no quiero; both
 *   must refuse). Refused: the bettors score at once 1, or the last bet that had been accepted before the raise, and
 *   they take the lance. All pass: the lance is played "en paso". An accepted ordago: the hands are shown at once and
 *   the lance's winner wins the game.
 * - The count, at the end of the hand, lance by lance: Grande and Chica the accepted bet, or 1 en paso; Pares and Juego
 *   the accepted bet and the values of both partners' pares or juego (juego: 31 is 3, any other 2), Punto the bet and 1.
 *   A refused lance: its winners also count their pares or juego. The first side to reach 40 points wins, even in the
 *   middle of the count (or at once, by points scored in the betting).
 * Notation (script): m mus, c cut, d<cards> lay away ("d4o5c"), p pass, b<n> bet or raise by n, o ordago, q quiero,
 * n no quiero, N the next hand; "." one plain action, "#" plain actions to the end of the hand, "!" to the end.
 */
#ifndef MUS_H
#define MUS_H

#include <stdint.h>

#include "cards.h"

#define MUS_P 4
#define MUS_GOAL 40
#define MUS_MAX_ACTS 4096
#define MUS_NONE 255
#define MUS_MAX_BET 40                          /* a bet's total at most */

enum { MP_MUS, MP_DISCARD, MP_BET, MP_HAND, MP_OVER };
enum { ML_GRANDE, ML_CHICA, ML_PARES, ML_JUEGO, ML_N };   /* ML_JUEGO: juego, or punto (mpos.punto) */
/* how a lance went: still to come, en paso, a bet accepted, refused, only one side had it, nobody had it, an ordago
 * accepted */
enum { LS_WAIT, LS_PASO, LS_SEEN, LS_DEJE, LS_ONE, LS_NONE, LS_ORDAGO };
/* a player's last word (shown at the seats) */
enum { MW_NONE, MW_MUS, MW_CUT, MW_PASS, MW_BET, MW_RAISE, MW_ORDAGO, MW_QUIERO, MW_NOQUIERO };
/* an action: its kind and its argument (a bet's amount; the cards laid away: bit i, the hand's card i) */
enum { MA_MUS = 1 << 8, MA_CUT = 2 << 8, MA_DISCARD = 3 << 8, MA_PASS = 4 << 8, MA_BET = 5 << 8, MA_ORDAGO = 6 << 8,
       MA_QUIERO = 7 << 8, MA_NOQUIERO = 8 << 8, MA_NEXT = 9 << 8 };
#define MA_KIND(a) ((a) & 0xff00)
#define MA_ARG(a) ((a) & 0xff)
enum { MUS_NO_PARES, MUS_PAR, MUS_MEDIAS, MUS_DUPLES };

typedef struct {
    uint8_t phase, dealer, mano, turn, deal;
    uint8_t hand[MUS_P][4];                     /* sorted: the highest first */
    uint8_t stock[40], nstock, laid[40], nlaid; /* laid: the cards laid away (the discards) */
    uint8_t refills, rounds;                    /* stocks made of the discards; rounds of changing cards */
    uint8_t took[MUS_P];                        /* the cards each took in the last change */
    uint8_t word[MUS_P], wordn[MUS_P];          /* each one's last word and its amount */
    uint8_t lance, punto;                       /* the lance being bet; punto: nobody has juego */
    uint8_t ls[ML_N], lamount[ML_N], lside[ML_N];   /* each lance: LS_*, the bet accepted or the points of the
                                                     * refusal, the side that took it (refused, one side, ordago) */
    uint8_t lwin[ML_N], lpts[ML_N];             /* the count: each lance's winning side (MUS_NONE: none, or not
                                                 * counted) and the points it scored then */
    uint8_t eligible;                           /* the players who speak in this lance (bit n) */
    uint8_t bet, prev, bettor, ordago, refused; /* the bet on the table, the one accepted before it, the side that
                                                 * made it; refused: the players who said no quiero (bit n) */
    int8_t gained[2];                           /* this hand's points, each side */
    int16_t score[2];
    uint8_t winner;                             /* MP_OVER: the winning side */
    uint32_t seed;
} mpos;

typedef struct {
    mpos pos;
    uint32_t seed;
    uint16_t act[MUS_MAX_ACTS];
    int n;
} mgame;

void mus_new(mgame *g, uint32_t seed);
int  mus_legal(const mpos *p, int act);
int  mus_play(mgame *g, int act);               /* 0: not allowed */
int  mus_undo(mgame *g);                        /* back one action (a replay from the start); 0: none */
int  mus_actions(const mpos *p, int *out);      /* every legal action (the bets: a few amounts); their count */
int  mus_auto(const mpos *p);                   /* a plain action (the "." of the notation) */
int  mus_script(mgame *g, const char *moves);   /* 0: an action is bad */
int  mus_winner(const mpos *p);                 /* MP_OVER: the winning side (0 or 1); else -1 */

/* the hands */
int  mus_value(int card);                       /* 1, 4..7, 10, 11, 12 (a 3 is a rey 12, a 2 an as 1) */
int  mus_points(const uint8_t *h);              /* the juego count */
int  mus_pares(const uint8_t *h, int *key);     /* MUS_NO_PARES..MUS_DUPLES; key: higher is better in its kind */
int  mus_has(const mpos *p, int lance, int player);   /* this player plays the lance (pares, juego: has them) */
int  mus_jugada(const mpos *p, int lance, int player);   /* the value of his pares or juego (0: none) */
int  mus_cmp(int lance, int punto, const uint8_t *a, const uint8_t *b);   /* > 0: a is better (no tie-break) */
int  mus_best(const mpos *p, int lance, unsigned players);   /* the best of these (ties: the nearer to the mano) */
void mus_sort(uint8_t *h);

/* the bot (mus_bot.c, bots.h) */
const mgame *mus_live(void);                    /* the game on the screen, read-only: the bot copies it at entry */
int mus_show(const char *move);                 /* main thread: plays one action of the notation as a tap does */
int musb_choose(const mgame *g, int seat, int level, uint64_t seed);   /* its action, 0: none */

#endif
