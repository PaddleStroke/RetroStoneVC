/*
 * Liar's Dice, the traditional bluffing game of dice under cups, for 2 to 6 players passing the device round: the
 * rules. No SDL; the dice of each round come from the game's seed (undo and the tests replay them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Each player has five dice under a cup. A round: everyone throws their dice, hidden; each player in turn, looking at
 * their own dice only, either bids that among ALL the dice on the table there are at least N showing face F, higher
 * than the last bid (more dice, or as many dice of a higher face), or calls the last bid a lie. Ones are wild: they
 * count as the face of the bid, so nobody bids on ones (faces 2 to 6). On a call every cup is lifted and the dice are
 * counted: if there are at least N (the ones included), the bid holds and the caller loses a die; if not, the bidder
 * does. The loser starts the next round (if out, the next player still in). A player without dice is out; the last
 * player with dice wins.
 * Notation (script; the screenshots and the tests): NxF a bid (3x4: at least three fours), "l" calls a lie, "n" the
 * next round after the reveal, "#" a plain action (a bid one step up, or a call when the bid is past what the dice
 * make likely).
 */
#ifndef LIARSDICE_H
#define LIARSDICE_H

#include <stdint.h>

#define LD_MAXP 6
#define LD_DICE 5
#define LD_BIDS 64                              /* the bids of a round, kept (the history shown) */
#define LD_MAX_ACTS 4096

enum { LP_BID, LP_REVEAL, LP_OVER };            /* LP_REVEAL: a call, every die shown, until the next round */
/* an action: a bid (count, face), a call, the next round */
enum { LA_BID = 1, LA_LIAR, LA_NEXT };
#define LD_ACT(kind, count, face) ((uint16_t)((kind) << 12 | (count) << 4 | (face)))
#define LD_KIND(a) ((a) >> 12)
#define LD_COUNT(a) ((a) >> 4 & 0xff)
#define LD_FACE(a) ((a) & 15)

typedef struct {
    uint8_t players, turn, phase, round, starter;
    uint8_t ndice[LD_MAXP];                     /* dice left (0: out) */
    uint8_t dice[LD_MAXP][LD_DICE];             /* this round's faces (the first ndice); after a call, the loser's
                                                 * lost die is still there, past ndice, for the reveal */
    uint8_t count, face, bidder;                /* the bid to beat (count 0: none yet) */
    uint8_t nbids;
    struct { uint8_t player, count, face; } bids[LD_BIDS];
    uint8_t caller, loser, found, held;         /* the call: who called, who lost a die, the dice found, the bid
                                                 * held */
    uint8_t winner;
    uint32_t seed;
} lpos;

typedef struct {
    lpos pos;
    uint32_t seed;
    uint8_t players;
    uint16_t act[LD_MAX_ACTS];
    int n;
} lgame;

void ld_new(lgame *g, int players, uint32_t seed);
int  ld_total(const lpos *p);                   /* all the dice on the table */
int  ld_counts(const lpos *p, int face);        /* the dice showing face (the ones included) */
int  ld_raises(const lpos *p, int count, int face);   /* 1: a bid higher than the last */
int  ld_legal(const lpos *p, int act);
int  ld_play(lgame *g, int act);                /* 0: not allowed */
int  ld_undo(lgame *g);
int  ld_next(const lpos *p, int player);        /* the next player still in after player */
int  ld_alive(const lpos *p);                   /* how many still have dice */
int  ld_auto(const lpos *p);                    /* a plain action (the "#" of the notation) */
int  ld_script(lgame *g, const char *moves);    /* 0: an action is bad */

/* the bot (liarsdice_bot.c, bots.h) */
const lgame *liarsdice_live(void);      /* the game on the screen, read-only: the bot copies its own view at entry */
/* main thread: plays a move as taps do: "3x4" bids, "l" calls, "n" starts the next round; 0: not now, or not legal */
int  liarsdice_show(const char *move);

#endif
