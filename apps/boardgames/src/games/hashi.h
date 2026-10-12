/*
 * Bridges (Hashi) for one player: the rules. Islands lie on a grid of n x n points (7, 10, 13 or 16), each with
 * a number. Join them with bridges, straight across or down between two islands with nothing between them; one or two
 * bridges between the same two islands; no bridge crosses another. Each island ends with as many bridges as its
 * number, and all the islands are joined into one group. Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: the four sizes, each at three levels (7 easy, 7 medium, 7 hard, 10 easy...). A puzzle is made from a
 * seed: islands set down one from another along the grid's lines, joined by one or two bridges as they come (so they
 * are all joined), a few bridges more where they fit; the numbers counted. Then, where the reasoning is stuck,
 * bridges added, doubled or taken away until the solver finishes it at its level and not below. The level is the
 * reasoning the solver needs (bench.h):
 *   0 easy: counting: an island's bridges against its number and what its neighbours can still take (a 4 with two
 *     neighbours: two double bridges); a bridge that is sure blocks those it would cross;
 *   1 medium: also the group: no bridges that would close a group of islands off from the others, and a group with
 *     one way out must take it;
 *   2 hard: also trials (a bridge that leads, by these ways, to a contradiction).
 * The links: the pairs of islands that see each other (the nearest island to the right or below), numbered by the
 * first island's place, row by row, the one to the right first. The notation (script, the screenshots, the tests):
 * an island is its column letter a.. and its row 1.. from the top; "a1-a4" adds a bridge between the islands a1 and
 * a4 (a third time: none), "a1-a4=2" sets two, "h" a hint, "+n" puts the next n links right.
 */
#ifndef HASHI_H
#define HASHI_H

#include <stdint.h>

#include "games/bench.h"

#define HSH_MAX 16
#define HSH_CELLS (HSH_MAX * HSH_MAX)
#define HSH_ISLANDS 110
#define HSH_LINKS 220
#define HSH_SIZES 4
#define HSH_LEVELS 3
#define HSH_VARIANTS (HSH_SIZES * HSH_LEVELS)

enum { HSH_EASY, HSH_MEDIUM, HSH_HARD };

static const int HSH_SIZE[HSH_SIZES] = {7, 10, 13, 16};
static inline int hsh_size_of(int variant) { return HSH_SIZE[variant / HSH_LEVELS % HSH_SIZES]; }
static inline int hsh_level_of(int variant) { return variant % HSH_LEVELS; }

typedef struct {
    int n, nisl, nlink;
    uint8_t ix[HSH_ISLANDS], iy[HSH_ISLANDS], need[HSH_ISLANDS];
    int16_t at[HSH_CELLS];                      /* the island on a point, or -1 */
    uint8_t la[HSH_LINKS], lb[HSH_LINKS];       /* a link's islands: la left of or above lb */
    uint8_t sol[HSH_LINKS];                     /* the bridges of the solution, 0..2 */
} hsh_puzzle;

typedef struct {
    int variant, level;
    uint32_t seed;
    hsh_puzzle p;
    uint8_t br[HSH_LINKS];                      /* the bridges now */
    bench_hist hist;
} hsh_game;

/* the links of a puzzle from its islands (p->at, ix, iy, nisl set): nlink, la, lb */
void hsh_links(hsh_puzzle *p);
int  hsh_cross(const hsh_puzzle *p, int a, int b);   /* 1: the links a and b cross */
int  hsh_link(const hsh_puzzle *p, int i, int j);    /* the link between two islands, or -1 */
int  hsh_count(const hsh_puzzle *p, int limit);
int  hsh_rate(const hsh_puzzle *p);
void hsh_make(int n, int level, uint32_t seed, hsh_puzzle *p);

void hsh_new(hsh_game *g, int variant, uint32_t seed);
int  hsh_blocked(const hsh_game *g, int link);  /* the bridge it would cross (a link), or -1 */
int  hsh_cycle(hsh_game *g, int link);          /* 0 -> 1 -> 2 -> 0 bridges; 0: blocked (a bridge across) */
int  hsh_set(hsh_game *g, int link, int count); /* 0: blocked or the same */
int  hsh_hint(hsh_game *g);                     /* a wrong bridge taken away, else one found by reasoning: the link */
int  hsh_undo(hsh_game *g);                     /* a link it changed; -1: none */
int  hsh_has(const hsh_game *g, int island);    /* its bridges now */
int  hsh_solved(const hsh_game *g);             /* every number met, all joined (the one solution) */
int  hsh_script(hsh_game *g, const char *moves);

#endif
