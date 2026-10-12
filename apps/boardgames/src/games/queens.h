/*
 * Eight Queens, for one player: N queens on an N x N board, none attacking another (the rules, no SDL).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Four boards: N = 5, 6, 8, 10. A new game sets one queen of a solution drawn from the seed (one not found yet this
 * session when there is one), fixed in place: the player places the others (a tap puts a queen on a square, or takes
 * it off). Queens may be put anywhere, attacked or not; those attacking each other are shown. Solved: N queens, no
 * two on one row, column or diagonal. The hint: a square to fill that still leads to a solution (preferring one not
 * found yet), or, when none can be reached any more, a queen to take off.
 * Squares: rank * n + file (a1 = 0, the bottom left). A solution's key: each rank's file, 4 bits each.
 */
#ifndef QUEENS_H
#define QUEENS_H

#include <stdint.h>

enum { QN_5, QN_6, QN_8, QN_10, QN_VARIANTS };
#define QN_MAX 10
#define QN_SQUARES (QN_MAX * QN_MAX)
#define QN_HISTORY 512

typedef struct {
    int n;
    uint8_t q[QN_SQUARES];                      /* 1: a queen */
    int count;
    int given;                                  /* the fixed queen's square */
    int16_t hist[QN_HISTORY];                   /* the squares tapped, for undo */
    int nhist;
    int moves, hints;
} qn_game;

int  qn_size(int variant);
/* a new game: the fixed queen from a solution not in found[] (nfound keys) when there is one */
void qn_new(qn_game *g, int variant, uint32_t seed, const uint64_t *found, int nfound);
void qn_empty(qn_game *g, int n, int given);    /* a board with that fixed queen (-1: none; the tests) */
int  qn_toggle(qn_game *g, int sq);             /* a queen put on or taken off; 0: the fixed one, or off the board */
int  qn_undo(qn_game *g);                       /* 0: nothing to undo */
int  qn_attacks(int n, int a, int b);           /* 1: queens on a and b attack each other */
int  qn_attacked(const qn_game *g, int sq);     /* how many queens (not one on sq) attack sq */
int  qn_clash(const qn_game *g, int sq);        /* a queen on sq, attacked by another */
int  qn_solved(const qn_game *g);
uint64_t qn_key(const qn_game *g);              /* the placement's key (when solved) */
int  qn_solutions(int n);                       /* how many solutions the board has (5: 10, 6: 4, 8: 92, 10: 724) */
/* the hint: a square to put a queen on (*remove 0), or a queen to take off (*remove 1); -1: solved. Prefers a
 * solution not in found[]. */
int  qn_hint(qn_game *g, const uint64_t *found, int nfound, int *remove);
int  qn_script(qn_game *g, const char *s);      /* "a1 c2 e3": squares tapped; "?": the hint followed */

#endif
