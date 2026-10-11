/*
 * Président (President), the climbing card game, for 3 to 6 players on one device passed round: the rules. No SDL;
 * the deal comes from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 52 cards dealt one at a time (some players may get one more). Ranks from 3 (low) up to king, ace, then 2 (the
 * highest); suits do not matter. The leader plays one card, or two, three or four of one rank; each in turn then plays
 * as many cards of a higher rank, or passes (a player who passed may play again when the turn comes back). When all
 * the others have passed in a row, the trick is over and the last player who played leads (if they are out of cards,
 * the next player who still has some). A 2 ends the trick at once: its player leads again. A player with no cards
 * left is out; the order they go out in gives the places and the roles for the next deal: Président, Vice-président
 * (four players or more), Neutre (the middle places), Vice-trou (four or more) and Trou (the last).
 * From the second deal on: the Trou leads and is dealt to first; the Trou gives their two best cards to the Président
 * and the Vice-trou their best card to the Vice-président (at once), then the Président gives back any two cards and
 * the Vice-président any one (their choice, in private).
 * Places score: the first players-1 points, the next one less... the last 0. Five deals; the highest total wins (a
 * tie for it: they share the win).
 * Notation (script; the screenshots and the tests): a run of one rank's characters plays that many of it (7, 77, 777;
 * 3 4 5 6 7 8 9 T J Q K A 2; the lowest suits held first), p pass, g<rank><suit> give a card (gKs; suits s h d c),
 * N the next deal; "." one plain action, "#" plain actions to the end of the deal.
 */
#ifndef PRESIDENT_H
#define PRESIDENT_H

#include <stdint.h>

#include "cards.h"

#define PRES_MAXP 6
#define PRES_HAND 20                            /* 52 cards for 3 players: 18, 17, 17; and 2 given */
#define PRES_DEALS 5
#define PRES_MAX_ACTS 8192
#define PRES_NONE 255

enum { PR_NONE, PR_PRES, PR_VICE, PR_NEUTRAL, PR_VICETROU, PR_TROU };
enum { PP_GIVE, PP_PLAY, PP_DEAL, PP_OVER };
/* an action: its kind and its cards: a rank and a mask of suits (play), a card (give) */
enum { PA_PLAY = 1 << 12, PA_PASS = 2 << 12, PA_GIVE = 3 << 12, PA_NEXT = 4 << 12 };
#define PA_KIND(a) ((a) & 0xf000)
#define PA_RANK(a) ((a) >> 4 & 15)
#define PA_MASK(a) ((a) & 15)
#define PA_PLAYOF(rank, mask) (PA_PLAY | (rank) << 4 | (mask))

/* a rank's strength: 3 lowest (1) .. king 11, ace 12, 2 the highest (13) */
static inline int pres_strength(int rank) { return rank == 1 ? 12 : rank == 2 ? 13 : rank - 2; }

typedef struct {
    uint8_t players, phase, deal, turn, shuffles;
    uint8_t hand[PRES_MAXP][PRES_HAND], n[PRES_MAXP];
    uint8_t role[PRES_MAXP];                    /* this deal's roles (PR_NONE in the first deal) */
    uint8_t out[PRES_MAXP], nout;               /* the players out of cards, in order */
    uint8_t giver, receiver, give_left;         /* the exchange: who gives back now, to whom, how many still */
    uint8_t got[PRES_MAXP][2], ngot[PRES_MAXP]; /* the cards each player was given in the exchange */
    uint8_t top_rank, top_count, top_player;    /* the trick: the last play (count 0: none, a lead), by whom */
    uint8_t passes, passed;                     /* passes in a row since that play; bit p: p passed since */
    uint8_t pile[52], npile, plays[52], nplays; /* the trick's cards in order, and the size of each play */
    int16_t score[PRES_MAXP], last[PRES_MAXP];  /* the totals; the last deal's points */
    uint32_t seed;
} ppos;

typedef struct {
    ppos pos;
    uint32_t seed;
    uint8_t players;
    uint16_t act[PRES_MAX_ACTS];
    int n;
} pgame;

void pres_new(pgame *g, int players, uint32_t seed);
int  pres_legal(const ppos *p, int act);
int  pres_play(pgame *g, int act);              /* 0: not allowed */
int  pres_undo(pgame *g);                       /* back one action (a replay from the deal); 0: none */
int  pres_active(const ppos *p, int player);    /* still holds cards (in the play) */
int  pres_place(const ppos *p, int player);     /* the place they went out in (0 first), -1: still in */
int  pres_points(int players, int place);
int  pres_best(const ppos *p, int player);      /* the index of their best card */
unsigned pres_winners(const ppos *p);           /* PP_OVER: the highest total's players (bit p) */
int  pres_auto(const ppos *p);                  /* a plain action (the "." of the notation) */
int  pres_script(pgame *g, const char *moves);  /* 0: an action is bad */

#endif
