/*
 * Jass, the Schieber: Switzerland's card game for four in two partnerships (players 0 and 2 against 1 and 3), on one
 * device passed round: the rules. No SDL; a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 36-card pack (6 to ace), French suits; 9 cards each (packets of 3). Player 3 deals first, the deal moving on one
 * seat each time; the player after the dealer chooses the game and leads the first trick.
 * - The choice: a trump suit, obenabe (no trumps, the aces high) or undeufe (no trumps, the sixes high); or schieben:
 *   the choice passes to the partner, who must make it. The first trick is still led by the one who chose first.
 * - Trumps J (Puur, 20), 9 (Nell, 14), A 11, K 4, Q 3, 10 10, 8, 7, 6; the other suits A K Q J 10 9 8 7 6 (A 11, K 4,
 *   Q 3, J 2, 10 10). Obenabe: every suit A K Q J 10 9 8 7 6, the eights worth 8; undeufe: 6 7 8 9 10 J Q K A, the
 *   sixes worth 11, the eights 8, the aces nothing. The last trick: 5 more, 157 in all; match (all nine tricks): 257.
 * - Play: follow suit, or trump instead. Unable to follow: any card. No undertrumping (a trump lower than one already
 *   in the trick, when another suit was led) unless only trumps are left. Trumps led: a trump, but the Puur need never
 *   be played (holding no other trump: any card). Obenabe and undeufe: follow suit.
 * - Weis, declared with one's first card: three in sequence in one suit 20, four 50, five 100, six 150, seven 200,
 *   eight 250, nine 300; four jacks 200, four nines 150, four aces, kings, queens or tens 100. Only the team with the
 *   best weis scores (the most points; equal: four of a kind before a sequence, the higher top card, a sequence in
 *   trumps, the first in play), all its weis. Stöck: the trump king and queen in one hand, 20 when the second is
 *   played.
 * - JASS_1000: the points as they are, the game to 1000. JASS_2500: multiplied, diamonds and hearts x1, spades and
 *   clubs x2, obenabe and undeufe x3 (the weis and stöck too), the game to 2500. The first team to reach it wins:
 *   stöck and weis count as they come, the tricks at the deal's end (both teams over then: the higher total; equal:
 *   the team of the last trick). The deal is played out.
 * Notation (script): t<suit> trumps ("th"), o obenabe, u undeufe, p schieben, w the weis declared, <card> plays one
 * ("Jh", "6s"; tricks.h), N the next deal; "." one plain action, "#" plain actions to the end of the deal, "!" to the
 * end of the game.
 */
#ifndef JASS_H
#define JASS_H

#include <stdint.h>

#include "cards.h"
#include "games/trick4.h"

#define JAS_HAND 9
#define JAS_DEALS 64
#define JAS_MAX_ACTS 4096
#define JAS_NONE 255
#define JAS_TOTAL 157
#define JAS_MELDS 4

enum { JASS_1000, JASS_2500, JASS_VARIANTS };
enum { JP_CHOOSE, JP_PLAY, JP_DEAL, JP_OVER };
enum { JM_OBEN = 4, JM_UNDE = 5, JM_MODES };    /* the games: the four suits (cards.h), then obenabe and undeufe */
/* an action: its kind and its argument (a game, a card) */
enum { JA_MODE = 1 << 8, JA_PUSH = 2 << 8, JA_WEIS = 3 << 8, JA_PLAY = 4 << 8, JA_NEXT = 5 << 8 };
#define JA_KIND(a) ((a) & 0xff00)
#define JA_ARG(a) ((a) & 0xff)

typedef struct { uint8_t len, top; } jsmeld;    /* a sequence of len (3..9) up to its top card; len 4 | 16: four of top */

typedef struct {                                /* a deal, once scored */
    uint8_t chooser, pushed, mode, mult, match; /* match: the team that took every trick + 1 (0: none) */
    int16_t eyes[2], weis[2], stock[2];         /* as taken (the weis: the best team's only), unmultiplied */
    int16_t score[2];                           /* all of it, multiplied */
} jsdeal;

typedef struct {
    uint8_t variant, phase, deal, dealer, turn;
    tk4_table t;
    uint8_t chooser, pushed, mode;              /* who chose; schoben; the game (JAS_NONE while choosing) */
    uint8_t declared[TK4_P];                    /* the weis declared with the first card */
    int16_t wpts[TK4_P];                        /* each player's weis */
    int32_t wkey[TK4_P];                        /* their best meld's strength */
    jsmeld meld[TK4_P][JAS_MELDS];
    uint8_t nmeld[TK4_P];
    uint8_t weis_team;                          /* the team whose weis counts (JAS_NONE: not yet, or none) */
    uint8_t stock_pl, stock_done;               /* the player holding stöck (JAS_NONE) */
    int16_t weis[2], stock[2];                  /* this deal's, unmultiplied */
    jsdeal list[JAS_DEALS];
    uint8_t ndeals;
    int16_t total[2];
    uint8_t first;                              /* the team that reached the target first (JAS_NONE) */
    uint32_t seed;
} jspos;

typedef struct {
    jspos pos;
    uint32_t seed;
    uint8_t variant;
    uint16_t act[JAS_MAX_ACTS];
    int n;
} jsgame;

void jas_new(jsgame *g, int variant, uint32_t seed);
int  jas_legal(const jspos *p, int act);
int  jas_play(jsgame *g, int act);              /* 0: not allowed */
int  jas_undo(jsgame *g);                       /* back one action (a replay from the start); 0: none */
int  jas_can_play(const jspos *p, int card);    /* the player to move may play it */
int  jas_points(int card, int mode);
int  jas_power(int card, int mode);             /* its strength in its suit (higher wins) */
int  jas_winner(const uint8_t *cards, int n, int mode);   /* the index of the card that takes the trick */
int  jas_mult(int variant, int mode);
int  jas_target(int variant);                   /* 1000 or 2500 */
/* the weis in a hand (melds out, JAS_MELDS at most): its points; *key the best meld's strength */
int  jas_melds(const uint8_t *h, int n, int mode, jsmeld *out, int *count, int32_t *key);
int  jas_meld_points(jsmeld m);
int  jas_can_weis(const jspos *p);              /* the player to move may declare a weis now */
void jas_sort(uint8_t *h, int n, int mode);     /* a hand as shown (mode JAS_NONE: while choosing) */
int  jas_winners(const jspos *p);               /* JP_OVER: the winning team's players (bit n) */
int  jas_auto(const jspos *p);                  /* a plain action (the "." of the notation) */
int  jas_script(jsgame *g, const char *moves);  /* 0: an action is bad */

#endif
