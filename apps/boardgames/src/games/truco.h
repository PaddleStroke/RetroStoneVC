/*
 * Truco (the Argentine game), for 2 players, or 4 in two partnerships, on one device (hidden hands): the rules. No SDL;
 * the deal comes from a seed, so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Partners sit across: 0 and 2 against 1 and 3 (two players: 0 against 1). The Spanish pack of 40 (no 8s, 9s:
 * spanish.h). The last player deals first; the mano, the next one, leads and wins the ties; both move on one seat a
 * hand. Three cards each.
 * - The cards, high to low: the as of espadas, the as of bastos, the 7 of espadas, the 7 of oros, the 3s, the 2s, the
 *   ases of copas and oros, the reyes (12), the caballos (11), the sotas (10), the 7s of copas and bastos, the 6s, the
 *   5s, the 4s. Cards of the same strength tie.
 * - Three rounds (bazas): each plays a card in turn, the highest wins the round, its player leads the next. A tie
 *   (parda) between the sides: the same player leads again; partners tied: their side wins it. The hand goes to the
 *   side that wins two rounds; after a parda the next round decides (a parda after a won round: the first round's
 *   winner); three pardas: the mano's side.
 * - Truco: on his turn, before playing, a player may call truco (the hand worth 2); the opponents accept (quiero), refuse
 *   (no quiero: the callers take the hand at its worth before the call) or raise: retruco (3), then vale cuatro (4).
 *   Only the side that accepted the last call may raise it, later, on a turn. "Me voy al mazo": a player gives up the
 *   hand, which the opponents take at its worth (1 without truco).
 * - Envido: in the first round only, before playing his first card, a player may call envido (2), real envido (3) or
 *   falta envido (what the leading side lacks to win; in a game to 30, while both are under 15, the whole game). The
 *   reply: quiero, no quiero, or a higher call (envido at most twice, then real envido, then falta envido): accepted,
 *   the sum (the falta alone); refused, the callers take what had been accepted before the last call (1 at least). Once
 *   the truco is accepted the envido is over; answering a truco, the envido comes first. A player's envido: two cards
 *   of a suit, 20 and their numbers (a figure 0), else his highest number; the best envido wins, the mano's side on a
 *   tie.
 * - Flor (a variant): three cards of one suit, shown at the deal, worth 3 to the side; both sides with flor: the best
 *   flor (20 and the three numbers) takes 3 for each flor. A hand with a flor has no envido. (No contraflor.)
 * - Game to 15 or 30 points.
 * Notation (script): a card plays it ("1e", "Ro": spanish.h), t truco (or the next raise), e envido, r real envido,
 * f falta envido, q quiero, n no quiero, m me voy al mazo, N the next hand; "." one plain action, "#" to the end of the
 * hand, "!" to the end of the game.
 */
#ifndef TRUCO_H
#define TRUCO_H

#include <stdint.h>

#include "cards.h"

#define TR_P 4
#define TR_MAX_ACTS 4096
#define TR_NONE 255
#define TR_PARDA 2

enum { TP_PLAY, TP_HAND, TP_OVER };
enum { TE_OPEN, TE_CALLING, TE_DONE };          /* the envido: still possible, called, over (or none) */
enum { TE_ENVIDO = 1, TE_REAL, TE_FALTA };      /* the envido's calls */
/* an action: its kind and its argument (a card) */
enum { TA_PLAY = 1 << 8, TA_TRUCO = 2 << 8, TA_ENVIDO = 3 << 8, TA_REAL = 4 << 8, TA_FALTA = 5 << 8,
       TA_QUIERO = 6 << 8, TA_NOQUIERO = 7 << 8, TA_MAZO = 8 << 8, TA_NEXT = 9 << 8 };
#define TA_KIND(a) ((a) & 0xff00)
#define TA_ARG(a) ((a) & 0xff)
/* each player's last word (shown at the seats) */
enum { TW_NONE, TW_TRUCO, TW_ENVIDO, TW_REAL, TW_FALTA, TW_QUIERO, TW_NOQUIERO, TW_MAZO, TW_FLOR };

typedef struct {
    uint8_t phase, players, goal, flor;
    uint8_t dealer, mano, turn, deal;
    uint8_t dealt[TR_P][3];                     /* the cards dealt (envido, flor) */
    uint8_t hand[TR_P][3], n[TR_P];
    uint8_t played[3][TR_P];                    /* each round's card of each player (CARD_NONE: not yet) */
    uint8_t round, leader, nplayed;
    uint8_t result[3], best[3];                 /* each round: the winning side or TR_PARDA (TR_NONE: not yet); the
                                                 * player whose card won it */
    /* the truco */
    uint8_t level, owner;                       /* the hand's worth accepted (1..4); the side that may raise it */
    uint8_t call, caller, responder;            /* a call waiting for its answer (0: none): its worth, its side, who
                                                 * answers */
    /* the envido */
    uint8_t env, nenv, chain[4], env_caller, env_responder;
    uint8_t env_value[TR_P];                    /* each player's envido */
    uint8_t env_winner, env_pts;                /* the best envido's player (TR_NONE: not played or refused) */
    uint8_t flor_mask, flor_side, flor_pts;     /* the players with flor; the side that took the flor's points */
    uint8_t word[TR_P], wordn[TR_P];            /* each one's last word; a truco call: its worth */
    /* the hand's end */
    uint8_t hand_side, hand_pts, mazo;          /* the side that took the hand, its points; given up (the player+1) */
    int8_t gained[2][3];                        /* this hand's points: [side][0 envido, 1 flor, 2 truco] */
    int16_t score[2];
    uint8_t winner;
    uint32_t seed;
} tpos;

typedef struct {
    tpos pos;
    uint32_t seed;
    uint16_t act[TR_MAX_ACTS];
    int n;
} tgame;

void truco_new(tgame *g, int players, int goal, int flor, uint32_t seed);
int  truco_legal(const tpos *p, int act);
int  truco_play(tgame *g, int act);             /* 0: not allowed */
int  truco_undo(tgame *g);
int  truco_actor(const tpos *p);                /* the player who must act now (the answer to a call: the responder) */
int  truco_actions(const tpos *p, int *out);    /* every legal action; their count */
int  truco_auto(const tpos *p);                 /* a plain action */
int  truco_script(tgame *g, const char *moves); /* 0: an action is bad */
int  truco_winner(const tpos *p);               /* TP_OVER: the winning side; else -1 */
int  truco_power(int card);                     /* 1 (the 4s) .. 14 (the as of espadas) */
int  truco_envido(const uint8_t *cards);        /* a player's envido (3 cards) */
int  truco_has_flor(const uint8_t *cards);
int  truco_falta(const tpos *p, int side);      /* what a falta envido won by this side is worth */
int  truco_can_envido(const tpos *p);           /* the actor may call an envido now */
int  truco_can_truco(const tpos *p);            /* the actor may call truco (or raise) now */

#endif
