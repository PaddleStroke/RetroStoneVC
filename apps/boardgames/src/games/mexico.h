/*
 * Mexico (Mäxchen), the bluffing game of two dice under a cup, for 2 to 6 players passing the device round: the rules.
 * No SDL, no dice: the caller throws and gives the rules the faces.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The values of a throw of two dice, read high die first, from the lowest: 31 32 41 42 43 51 52 53 54 61 62 63 64 65,
 * then the doubles 11 22 33 44 55 66, then 21, "Mexico", the highest of all (ranks 0..20).
 * - Each player has lives (3 or 6, the variant). A round: its first player throws the two dice under the cup, looks
 *   at them alone and announces a value, the truth or not. The cup goes to the next player still in, who either
 *   believes it and throws the dice again (unseen, the dice under the cup thrown afresh), then announces a value at
 *   least as high as the last one (true or not), or calls it: the cup is lifted. If the dice show at least the value
 *   announced, the caller loses a life; if less, the announcer does.
 * - The loser starts the next round (if out of lives, the next player still in). A player with no lives left is out;
 *   the last player in wins.
 * Notation (script; the screenshots and the tests): "t53" a throw (the two faces), "s62" a value announced ("s21"
 * Mexico), "c" a call, "n" the next round after the cup is lifted.
 */
#ifndef MEXICO_H
#define MEXICO_H

#include <stdint.h>

#define MX_MAXP 6
#define MX_VALUES 21                            /* the ranks 0..20 */
#define MX_MEXICO 20                            /* 21's rank */
#define MX_CLAIMS 64                            /* the values announced in a round, kept (the latest: the history) */
#define MX_MAX_ACTS 4096

enum { MX_THROW, MX_SAY, MX_REVEAL, MX_OVER };  /* to throw (or to call), to announce, the cup lifted, over */
/* an action: a throw (its two faces), a value announced (its rank), a call, the next round */
enum { MXA_THROW = 1, MXA_SAY, MXA_CALL, MXA_NEXT };
#define MX_ACT(kind, arg) ((uint16_t)((kind) << 8 | (arg)))
#define MX_KIND(a) ((a) >> 8)
#define MX_ARG(a) ((a) & 255)
#define MX_DICE(d1, d2) ((d1) * 8 + (d2))         /* a throw's arg */

typedef struct {
    uint8_t players, turn, phase, starter;
    uint16_t round;
    uint8_t lives[MX_MAXP], start_lives;
    int8_t claim;                               /* the rank to beat (-1: none yet this round) */
    uint8_t claimer;
    uint8_t dice[2];                            /* under the cup: the last throw (0: none yet this round) */
    uint8_t nclaims;
    struct { uint8_t player, rank; } claims[MX_CLAIMS];
    uint8_t caller, loser, held;                /* the call: who called, who lost a life, the value held */
    int8_t winner;                              /* -1: none yet */
} mxpos;

typedef struct {
    mxpos pos;
    uint16_t act[MX_MAX_ACTS];                  /* the actions (the first MX_MAX_ACTS of them) */
    int n;
} mxgame;

int  mx_rank(int d1, int d2);                   /* a throw's rank, 0..20 (either order) */
void mx_dice_of(int rank, int *hi, int *lo);    /* the faces of a rank (21: 2 then 1; a double: the same twice) */
int  mx_number(int rank);                       /* the value as read: 31..65, 11..66, 21 */
int  mx_from_number(int number);                /* a value as read to its rank; -1 if none */
void mx_new(mxgame *g, int players, int lives);
int  mx_next(const mxpos *p, int player);       /* the next player still in after player */
int  mx_alive(const mxpos *p);                  /* how many are still in */
int  mx_legal(const mxpos *p, int act);
int  mx_play(mxgame *g, int act);               /* 0: not allowed */
int  mx_script(mxgame *g, const char *moves);   /* 0: an action is bad */

#endif
