/*
 * Eight Queens, for one player: N queens on an N x N board, none attacking another (the rules, no SDL).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The random games (the variants): N = 5, 6, 8, 10. A new game sets one queen of a solution drawn from the seed (one
 * not found yet this session when there is one), fixed in place: the player places the others. The levels
 * (queens_levels.h, made by tools/queens_gen.c): boards of 4 x 4 to 10 x 10 with holes (squares cut away, where no
 * queen may stand; the queens' lines run on over them) and fixed queens, each with exactly one solution, found by
 * reasoning alone (the generator's solver never guesses).
 * A tap puts a queen on a square, or takes it off. Queens may be put anywhere, attacked or not; those attacking each
 * other are shown, and a queen put where another attacks it is a mistake (counted). Solved: N queens, no two on one
 * row, column or diagonal. The stars: 3 with no hint and no mistake, 2 with up to two hints, else 1. The hint: a
 * square to fill that still leads to a solution (preferring one not found yet), or, when none can be reached any
 * more, a queen to take off.
 * Squares: rank * n + file (a1 = 0, the bottom left). A solution's key: each rank's file, 4 bits each.
 * A level: its rows from the top, split by '|': '.' a square, '#' a hole, 'Q' a fixed queen.
 */
#ifndef QUEENS_H
#define QUEENS_H

#include <stdint.h>

enum { QN_5, QN_6, QN_8, QN_10, QN_VARIANTS };
#define QN_MAX 10
#define QN_SQUARES (QN_MAX * QN_MAX)
#define QN_HISTORY 512
#define QN_LEVELS 150
#define QN_CHAPTERS 6

typedef struct {
    int n;
    uint8_t q[QN_SQUARES];                      /* 1: a queen */
    uint8_t fixed[QN_SQUARES];                  /* 1: a fixed queen (given) */
    uint8_t hole[QN_SQUARES];                   /* 1: a hole */
    int count;
    int given;                                  /* a fixed queen's square (the random games' only one); -1: none */
    int16_t hist[QN_HISTORY];                   /* the squares tapped, for undo */
    int nhist;
    int moves, hints, mistakes;
} qn_game;

extern const char *const qn_levels[QN_LEVELS];

int  qn_size(int variant);
/* a new game: the fixed queen from a solution not in found[] (nfound keys) when there is one */
void qn_new(qn_game *g, int variant, uint32_t seed, const uint64_t *found, int nfound);
void qn_empty(qn_game *g, int n, int given);    /* a board with that fixed queen (-1: none; the tests) */
int  qn_load(qn_game *g, const char *rows);     /* a level's rows; 0: not a level */
int  qn_level(qn_game *g, int level);           /* level 0..QN_LEVELS-1 */
int  qn_toggle(qn_game *g, int sq);             /* a queen put on or taken off; 0: a fixed one, a hole, off the board */
int  qn_undo(qn_game *g);                       /* 0: nothing to undo */
int  qn_attacks(int n, int a, int b);           /* 1: queens on a and b attack each other */
int  qn_attacked(const qn_game *g, int sq);     /* how many queens (not one on sq) attack sq */
int  qn_clash(const qn_game *g, int sq);        /* a queen on sq, attacked by another */
int  qn_solved(const qn_game *g);
uint64_t qn_key(const qn_game *g);              /* the placement's key (when solved) */
int  qn_solutions(int n);                       /* how many solutions the board has (5: 10, 6: 4, 8: 92, 10: 724) */
int  qn_solution(int n, int i, uint64_t *key); /* the board's solution i (0..qn_solutions - 1) into *key; 0: none */
/* the solutions that keep off the holes and hold every fixed queen (a level: 1); *key the first one's */
int  qn_count(const qn_game *g, uint64_t *key);
/* the hint: a square to put a queen on (*remove 0), or a queen to take off (*remove 1); -1: solved. Prefers a
 * solution not in found[]. */
int  qn_hint(qn_game *g, const uint64_t *found, int nfound, int *remove);
int  qn_stars(const qn_game *g);                /* solved: 1..3 */
int  qn_script(qn_game *g, const char *s);      /* "a1 c2 e3": squares tapped; "?": the hint followed */

#endif
