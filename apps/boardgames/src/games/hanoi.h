/*
 * The Tower of Hanoi for one player: the rules. Discs of graded sizes sit on the left of three pegs, the largest at
 * the bottom. Move the whole tower to another peg: one disc at a time (the top disc of a peg), never a disc on a
 * smaller one. The fewest moves for n discs: 2^n - 1.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 4, 5, 6 or 7 discs. Discs: 1 the smallest .. n the largest. The tower may end on the middle peg or
 * the right one: either is solved. The notation (script, the screenshots, the tests): a move is its two pegs, a b c
 * from the left, "ac ab cb"; "=k" plays the next k moves of the shortest way to the right peg (from the start: the
 * way the tests check).
 */
#ifndef HANOI_H
#define HANOI_H

#include <stdint.h>

#define HN_VARIANTS 4
#define HN_MAX 7
#define HN_HIST 1024

typedef struct {
    int n;
    int8_t disc[3][HN_MAX];                     /* each peg's discs from the bottom */
    int h[3];                                   /* how many */
    uint8_t hist[HN_HIST];                      /* each move: from * 3 + to */
    int nh;
    int moves;
} hn_game;

void hn_new(hn_game *g, int discs);
int  hn_optimum(int discs);                     /* 2^n - 1 */
int  hn_top(const hn_game *g, int peg);         /* its top disc; 0: none */
int  hn_can(const hn_game *g, int from, int to);
int  hn_move(hn_game *g, int from, int to);     /* 0: not allowed */
int  hn_undo(hn_game *g);                       /* 0: nothing to undo */
int  hn_solved(const hn_game *g);
/* the k-th move (0..) of the shortest way to move n discs from peg a to peg b: its pegs */
void hn_way(int n, int a, int b, int k, int *from, int *to);
int  hn_script(hn_game *g, const char *moves);

#endif
