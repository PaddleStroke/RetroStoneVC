/*
 * The Tower of Hanoi for one player: the rules. Discs of graded sizes stand on three pegs, never a disc on a smaller
 * one. Move one disc at a time (the top disc of a peg), never onto a smaller one, until every disc stands on the goal
 * peg as one tower. The classic start: the whole tower on the left peg; the fewest moves for n discs: 2^n - 1.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The levels: 100 of our own in five chapters, one a number of discs (3 to 7: 12, 18, 20, 25 and 25 levels), made by
 * tools/hanoi_gen.c (hanoi_levels.h): the discs scattered over the pegs, a goal peg, the fewest moves; each chapter
 * ends with the classic tower. Random play: the variants are 4, 5, 6 or 7 discs scattered by chance and a goal peg
 * drawn, never one the largest disc already stands on (so at least 2^(n-1) moves).
 * The fewest moves from any position to a tower on peg t, and the next move of that way: the largest disc not on t
 * must go there, so first every smaller disc onto the third peg; the count adds 2^(k-1) for each disc k that moves
 * (the known recursion), and the way is unique.
 * Discs: 1 the smallest .. n the largest. Pegs a b c from the left. The classic game (hn_new) is solved on the middle
 * peg or the right one; a level and a random game on their goal peg. The notation (script, the screenshots, the
 * tests): a move is its two pegs, "ac ab cb"; "=k" plays the next k moves of the shortest way from here ("=": all).
 */
#ifndef HANOI_H
#define HANOI_H

#include <stdint.h>

#define HN_VARIANTS 4
#define HN_MAX 7
#define HN_HIST 1024
#define HN_LEVELS 100
#define HN_CHAPTERS 5

typedef struct {
    int n;
    int8_t disc[3][HN_MAX];                     /* each peg's discs from the bottom */
    int h[3];                                   /* how many */
    int goal;                                   /* the goal peg; -1: the middle or the right one (the classic game) */
    int par;                                    /* the fewest moves from the start */
    uint8_t hist[HN_HIST];                      /* each move: from * 3 + to */
    int nh;
    int moves;
} hn_game;

void hn_new(hn_game *g, int discs);             /* the classic tower on the left peg */
/* any position: disc d (1..n) on peg[d - 1]; goal 0..2 (-1: the middle or the right one) */
void hn_set(hn_game *g, int discs, const int8_t *peg, int goal);
void hn_random(hn_game *g, int discs, uint32_t seed);   /* discs scattered by chance, a goal peg */
int  hn_optimum(int discs);                     /* 2^n - 1 */
int  hn_top(const hn_game *g, int peg);         /* its top disc; 0: none */
int  hn_can(const hn_game *g, int from, int to);
int  hn_move(hn_game *g, int from, int to);     /* 0: not allowed */
int  hn_undo(hn_game *g);                       /* 0: nothing to undo */
int  hn_solved(const hn_game *g);
int  hn_target(const hn_game *g);              /* the peg the shortest way gathers the tower on */
int  hn_left(const hn_game *g);                /* the fewest moves from here */
int  hn_hint(const hn_game *g, int *from, int *to);   /* the next move of the shortest way; 0: solved */
/* the k-th move (0..) of the shortest way to move n discs from peg a to peg b: its pegs */
void hn_way(int n, int a, int b, int k, int *from, int *to);
int  hn_script(hn_game *g, const char *moves);
/* the designed levels: "c:aab", the goal peg, then each disc's peg from the largest; their fewest moves */
int  hn_parse(hn_game *g, const char *level);
extern const char *const hn_levels[HN_LEVELS];
extern const uint8_t hn_level_par[HN_LEVELS];
int  hn_level(hn_game *g, int level);           /* 0: none */
int  hn_chapter_of(int level);                  /* the chapters: one a number of discs */
int  hn_chapter_start(int chapter);             /* its first level */

#endif
