/*
 * Doppelkopf, for four on one device passed round (hidden hands, hidden partners): the rules, after the German
 * Doppelkopf League's tournament rules (DDV). No SDL; a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The double pack of 48 cards: 9, 10, J, Q, K, A of each suit, twice; French suits. 12 cards each (packets of 3).
 * Player 3 deals first, the deal moving on one seat each time; the player after the dealer speaks first and leads.
 * - Reservations: each in turn says healthy (gesund) or that he has a reservation (Vorbehalt), and which game, kept
 *   secret: a Solo (queens, jacks, a suit, or fleshless) or, holding both queens of clubs, a marriage (Hochzeit, which
 *   is his to announce or not). The first solo in turn plays; else the marriage; else the normal game. A player with
 *   both queens of clubs who says nothing plays a silent solo (scored as a solo).
 * - The normal game: trumps the 10 of hearts (Dulle), the queens (clubs, spades, hearts, diamonds), the jacks (in the
 *   same order), then diamonds A 10 K 9; the other suits A 10 K 9 (hearts A K 9). Of two equal cards the first played
 *   wins. The holders of the queens of clubs (the Alten) are Re, the others Kontra: unknown till shown or announced.
 * - The marriage: the first trick of the first three won by another player makes him the bride's partner; none: the
 *   bride plays alone, scored as a solo.
 * - The solos, one against three: queens (the queens alone are trumps), jacks (the jacks alone), a suit (as the normal
 *   game, that suit in place of diamonds), fleshless (no trumps; every suit A 10 K Q J 9).
 * - Follow suit (trumps are one suit); void: any card.
 * - Announcements, by a player for his own party (Re, or Kontra), before playing his card while holding at least: Re
 *   or Kontra 11 cards, no 90 (keine 90) 10, no 60 9, no 30 8, schwarz 7; each one includes those below. Re or Kontra
 *   in reply to the other party's announcement: one card later than that announcement's own limit. A marriage moves
 *   the limits on by the tricks it took to find the partner; none before.
 * - Card points A 11, 10 10, K 4, Q 3, J 2: 240. Re wins with 121 (120 if only Kontra was announced), Kontra with
 *   120 (121 if only Kontra was announced); a party that announced no 90 (60, 30, schwarz) must keep the other under
 *   90 (60, 30, without a trick), else the other wins; both failing such announcements: nobody wins.
 * - The game value, for the winners: won 1; the losers under 90, 60, 30, schwarz: 1 each; Re announced 2, Kontra
 *   announced 2; each no 90, 60, 30, schwarz announced 1; against the other's no 90 120 reached, against no 60 90,
 *   against no 30 60, against schwarz 30: 1 each.
 * - Extra points (Sonderpunkte), normal games and marriages only, to the party: a fox caught (an opponent's ace of
 *   diamonds in a trick it takes), a Doppelkopf (a trick of 40 or more), Karlchen (the last trick taken with the jack of
 *   clubs), against the Alten (Kontra wins): 1 each; the two parties' extra points cancel.
 * - The score: normal: each Re player + (the value if Re won, - if Kontra won, + Re's extra points - Kontra's), each
 *   Kontra player the opposite. A solo: the soloist three times, each other once.
 * - 12 or 24 deals (DKO_SHORT, DKO_LONG); the highest total wins (equal: shared). No compulsory solos.
 * Notation (script): g healthy, w a marriage, q a queens' solo, j a jacks' solo, f fleshless, s<suit> a suit solo
 * ("sc"), a announces (the next level for the player's party), <card> plays one ("Qc", "Th"; tricks.h), N the next
 * deal; "." one plain action, "#" plain actions to the end of the deal, "!" to the end of the game.
 */
#ifndef DOPPELKOPF_H
#define DOPPELKOPF_H

#include <stdint.h>

#include "cards.h"
#include "games/trick4.h"

#define DKO_HAND 12
#define DKO_MAX_ACTS 4096
#define DKO_DEALS_MAX 24
#define DKO_NONE 255

enum { DKO_SHORT, DKO_LONG, DKO_VARIANTS };
enum { DKP_RESERVE, DKP_PLAY, DKP_DEAL, DKP_OVER };
/* the games (and the reservations: 0 healthy, else the game wanted) */
enum { DKG_NORMAL, DKG_WEDDING, DKG_SILENT, DKG_QUEENS, DKG_JACKS, DKG_FLESHLESS, DKG_SUIT, DKG_TYPES = DKG_SUIT + 4 };
/* an action: its kind and its argument (a reservation, a level, a card) */
enum { DKA_SAY = 1 << 8, DKA_ANNOUNCE = 2 << 8, DKA_PLAY = 3 << 8, DKA_NEXT = 4 << 8 };
#define DKA_KIND(a) ((a) & 0xff00)
#define DKA_ARG(a) ((a) & 0xff)
enum { DKW_NONE, DKW_HEALTHY, DKW_RESERVED };   /* each player's word */
/* the announcements' levels: Re or Kontra, no 90, no 60, no 30, schwarz */
enum { DKL_NONE, DKL_PARTY, DKL_90, DKL_60, DKL_30, DKL_BLACK };

typedef struct {                                /* a deal, once scored */
    uint8_t game, soloist, re, solo;            /* solo: scored as one (the soloist alone) */
    uint8_t ann[2];                             /* Re's and Kontra's announcements */
    int16_t eyes[2];                            /* card points */
    uint8_t tricks[2];
    int8_t winner;                              /* 0 Re, 1 Kontra, -1 nobody */
    uint8_t value;                              /* the game value for the winners */
    uint8_t fox[2], doko[2], karl[2], alten;    /* extra points by party; against the Alten (Kontra won) */
    int8_t re_points;                           /* each Re player's points (a solo: the opponents' unit) */
    int16_t score[TK4_P];
} dkdeal;

typedef struct {
    uint8_t variant, phase, deal, dealer, turn;
    tk4_table t;
    uint8_t said[TK4_P], choice[TK4_P], spoke; /* the reservations: each one's word, the game wanted (secret) */
    uint8_t game, soloist, re;                  /* the game; the soloist or bride (DKO_NONE); the Re party (bit n) */
    uint8_t open;                               /* the parties settled (a marriage: once the partner is found) */
    uint8_t shift;                              /* the announcements' limits moved on by a marriage (tricks) */
    uint8_t ann[2], level[TK4_P];               /* each party's announcement; each player's own */
    uint8_t qc[TK4_P];                          /* the queens of clubs each one has played */
    uint8_t hist[DKO_HAND][TK4_P], hist_leader[DKO_HAND];   /* the tricks played (the extra points) */
    dkdeal list[DKO_DEALS_MAX];
    uint8_t ndeals;
    int16_t total[TK4_P];
    uint32_t seed;
} dkpos;

typedef struct {
    dkpos pos;
    uint32_t seed;
    uint8_t variant;
    uint16_t act[DKO_MAX_ACTS];
    int n;
} dkgame;

void dko_new(dkgame *g, int variant, uint32_t seed);
int  dko_deals(int variant);                    /* 12 or 24 */
int  dko_legal(const dkpos *p, int act);
int  dko_play(dkgame *g, int act);              /* 0: not allowed */
int  dko_undo(dkgame *g);                       /* back one action (a replay from the start); 0: none */
int  dko_points(int card);
int  dko_trump(int card, int game);             /* a trump in this game */
int  dko_power(int card, int game);             /* its strength in its suit, trumps above (higher wins) */
int  dko_winner(const uint8_t *cards, int n, int game);   /* the index of the card that takes the trick (the first
                                                  * of two equal ones) */
int  dko_can_play(const dkpos *p, int card);    /* the player to move may play it */
int  dko_say_ok(const dkpos *p, int choice);    /* the player to move may say this reservation (0: healthy) */
int  dko_party(const dkpos *p, int pl);         /* 0 Re, 1 Kontra (as it is) */
int  dko_known(const dkpos *p, int pl);         /* what the table knows of a player's party: -1 nothing, 0 Re, 1 Kontra */
int  dko_announce_ok(const dkpos *p);           /* the player to move may announce the next level now */
int  dko_is_solo(int game);                     /* a solo by choice (queens, jacks, fleshless, a suit) */
void dko_sort(uint8_t *h, int n, int game);     /* a hand as shown */
int  dko_winners(const dkpos *p);               /* DKP_OVER: the players with the highest total (bit n) */
int  dko_auto(const dkpos *p);                  /* a plain action (the "." of the notation) */
int  dko_script(dkgame *g, const char *moves);  /* 0: an action is bad */

#endif
