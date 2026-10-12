/*
 * Schafkopf, the Bavarian trick game for four, on one device passed round: the rules. No SDL; a game is its seed and
 * its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 32-card pack (7 to ace) in French suits for the Bavarian ones: clubs for acorns (Eichel), spades for leaves
 * (Gras), hearts for hearts (Herz), diamonds for bells (Schellen); the queens are the Obers, the jacks the Unters, the
 * aces the Sau. 8 cards each (packets of 4). Player 3 deals first, the deal moving on one seat each time; the player
 * after the dealer speaks first and leads the first trick.
 * - The bidding: each in turn passes (weiter) or says what he would play: a Rufspiel (calling the ace of a suit he
 *   holds a card of, not hearts, not holding that ace), a Wenz, or a Solo in a suit. The highest game plays: Solo, then
 *   Wenz, then Rufspiel; between equal ones the first to speak. Everyone passed: the deal is thrown in (SFK_PLAIN) or
 *   played as a Ramsch (SFK_RAMSCH).
 * - Trumps: the four Obers (clubs, spades, hearts, diamonds), the four Unters in that order, then hearts A 10 K 9 8 7;
 *   in a Solo its suit takes the place of hearts; in the Wenz the Unters alone. The other suits A 10 K (O) (U) 9 8 7.
 * - The called ace's holder is the declarer's partner, unknown till it shows. When the called suit is led the first
 *   time he must play the ace; until then he may not throw it away on another suit, and may lead that suit only with
 *   the ace, unless he holds four or more of it (davonlaufen: then any of them, and the ace is free).
 * - Card points A 11, 10 10, K 4, O 3, U 2: 120. The declaring side wins with 61; schneider: the losers 30 or less;
 *   schwarz: the losers took no trick.
 * - The tariffs, in points: Rufspiel 10, Solo and Wenz 50; schneider and schwarz 10 more each; the Laufende (the
 *   unbroken run of top trumps from the club Ober (Wenz: Unter) down held by one side, 3 or more, in the Wenz 2 or
 *   more) 10 each. Rufspiel: each winner + the value, each loser - the value; Solo, Wenz: the declarer three times it.
 * - Ramsch: the Rufspiel's trumps, each for himself; the one with the most card points pays 10 to each other (equal:
 *   the one with more tricks; still equal, each of them pays); taking every trick (Durchmarsch) wins 50 from each.
 * - 12 deals; the highest total wins (equal totals share the win).
 * Not played: doubling (Kontra, Re), Tout, Sie, the Geier and other house games.
 * Notation (script): p passes, r<suit> a Rufspiel calling that suit's ace ("rc"), w a Wenz, s<suit> a Solo ("sh"),
 * <card> plays one ("Qc", "Ah"; tricks.h), N the next deal; "." one plain action, "#" plain actions to the end of the
 * deal, "!" to the end of the game.
 */
#ifndef SCHAFKOPF_H
#define SCHAFKOPF_H

#include <stdint.h>

#include "cards.h"
#include "games/trick4.h"

#define SFK_HAND 8
#define SFK_DEALS 12
#define SFK_MAX_ACTS 2048
#define SFK_NONE 255

enum { SFK_PLAIN, SFK_RAMSCH, SFK_VARIANTS };
enum { SFP_BID, SFP_PLAY, SFP_DEAL, SFP_OVER };
/* the games, by rank (a game: its kind | its suit << 4: the ace called, the Solo's trumps) */
enum { SFG_NONE, SFG_RUF, SFG_WENZ, SFG_SOLO, SFG_RAMSCH };
#define SFG(kind, suit) ((kind) | (suit) << 4)
#define SFG_KIND(g) ((g) & 15)
#define SFG_SUIT(g) ((g) >> 4 & 3)
/* an action: its kind and its argument (a game, a card) */
enum { SFA_PASS = 1 << 8, SFA_GAME = 2 << 8, SFA_PLAY = 3 << 8, SFA_NEXT = 4 << 8 };
#define SFA_KIND(a) ((a) & 0xff00)
#define SFA_ARG(a) ((a) & 0xff)
enum { SFW_NONE, SFW_PASS, SFW_PLAY };          /* each player's word in the bidding */

typedef struct {                                /* a deal, once scored */
    uint8_t declarer, kind, suit, partner;      /* declarer SFK_NONE: thrown in (or a Ramsch) */
    uint8_t won, schneider, schwarz, runners;   /* runners: the Laufende counted (0: too few) */
    uint8_t losers, march;                      /* Ramsch: who pays (bit n); the player who took every trick + 1 */
    int16_t eyes[TK4_P];                        /* each player's card points */
    int16_t side;                               /* the declaring side's card points */
    int16_t value, score[TK4_P];
} sfdeal;

typedef struct {
    uint8_t variant, phase, deal, dealer, turn;
    tk4_table t;
    uint8_t dealt[TK4_P][SFK_HAND];             /* the hands as dealt (the Laufende) */
    uint8_t word[TK4_P], want[TK4_P], spoke;    /* the bidding: each one's word, the game wanted (secret) */
    uint8_t declarer, kind, suit, partner;      /* the game played; the called ace's holder (SFK_NONE) */
    uint8_t searched;                           /* the trick the called suit was first led in + 1 (0: not yet) */
    uint8_t revealed;                           /* the called ace has been played */
    sfdeal list[SFK_DEALS];
    uint8_t ndeals;
    int16_t total[TK4_P];
    uint32_t seed;
} sfpos;

typedef struct {
    sfpos pos;
    uint32_t seed;
    uint8_t variant;
    uint16_t act[SFK_MAX_ACTS];
    int n;
} sfgame;

void sfk_new(sfgame *g, int variant, uint32_t seed);
int  sfk_legal(const sfpos *p, int act);
int  sfk_play(sfgame *g, int act);              /* 0: not allowed */
int  sfk_undo(sfgame *g);                       /* back one action (a replay from the start); 0: none */
int  sfk_can_play(const sfpos *p, int card);    /* the player to move may play it */
int  sfk_game_ok(const sfpos *p, int game);     /* the player to move may say this game */
int  sfk_points(int card);
int  sfk_trump(int card, int kind, int suit);   /* a trump in this game */
int  sfk_power(int card, int kind, int suit);   /* its strength in its suit, trumps above (higher wins) */
int  sfk_winner(const uint8_t *cards, int n, int kind, int suit);   /* the index of the card that takes the trick */
int  sfk_runners(const uint8_t (*hands)[SFK_HAND], unsigned side, int kind, int suit);   /* the side's Laufende:
                                                  * with (> 0) or without (< 0) */
void sfk_sort(uint8_t *h, int n, int kind, int suit);   /* a hand as shown */
int  sfk_side(const sfpos *p);                  /* the declaring side (bit n) */
int  sfk_winners(const sfpos *p);               /* SFP_OVER: the players with the highest total (bit n) */
int  sfk_auto(const sfpos *p);                  /* a plain action (the "." of the notation) */
int  sfk_script(sfgame *g, const char *moves);  /* 0: an action is bad */

#endif
