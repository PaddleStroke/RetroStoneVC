/*
 * Mirrors for one player: the rules. A grid with a lamp that sends out a beam of light, gems to light, bombs the
 * light must never touch and wooden blocks that stop it. Some cells hold brass mirrors on a pivot, set across the
 * cell one way (/) or the other (\): the player turns them. The player also has a few loose mirrors to stand on empty
 * cells. A mirror turns the beam by a right angle; a glass plate (the harder levels) lets half the light through and
 * turns the other half. Solved when the beam lights every gem and touches no bomb.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The puzzles are made from a seed: a beam's path drawn by chance (straight runs, then a mirror turning it), the gems
 * set along it (the farthest first: the whole path is needed), the bombs beside it, a few mirrors that only mislead;
 * the pivoting mirrors are then turned by chance and the loose ones taken off the board. A puzzle is kept only when a
 * search (mr_solve: it follows the beam and tries each choice it meets) finds one way only, and when that search has
 * enough work to do for the level:
 *   0 easy: 5 x 5, pivoting mirrors only;  1 medium: 6 x 6, one loose mirror, a bomb;
 *   2 hard: 7 x 7, two loose mirrors, bombs;  3 expert: 8 x 8, three loose mirrors, a glass plate, more bombs.
 * Cells: x + y * n from the top left. Directions: 0 east, 1 north, 2 west, 3 south. A mirror's way: 0 '/', 1 '\'.
 * The notation (script, the screenshots, the tests): a cell is its column letter a.. and its row 1.. from the top;
 * "c4" taps it (a pivot: turned; an empty cell: a loose mirror stood '/', then '\', then taken off), "c4s" sets it '/',
 * "c4b" sets it '\', "c4e" takes a loose mirror off, "c4>e2" moves a loose mirror; "h" a hint; "=" hints to the end.
 */
#ifndef MIRRORS_H
#define MIRRORS_H

#include <stdint.h>

#define MR_VARIANTS 4
#define MR_MAX 8
#define MR_CELLS (MR_MAX * MR_MAX)
#define MR_HIST 2048

enum { MR_EMPTY, MR_WALL, MR_LAMP, MR_GEM, MR_BOMB, MR_PIVOT, MR_LOOSE, MR_GLASS };   /* what a cell holds */
enum { MR_SLASH, MR_BACK };                     /* a mirror's way: '/' '\' */

typedef struct { uint8_t cell, kind, way, first; } mr_change;

typedef struct {
    int n, level;
    uint8_t kind[MR_CELLS];                     /* MR_* (MR_LOOSE: a loose mirror the player stood there) */
    uint8_t way[MR_CELLS];                      /* a mirror's or a glass's way; the lamp's direction */
    int lamp;                                   /* the lamp's cell */
    int loose;                                  /* the loose mirrors the puzzle gives */
    int8_t sol[MR_CELLS];                       /* the solution: -1 does not matter, 0 '/', 1 '\', 2 nothing there */
    uint8_t order[MR_CELLS];                    /* the cells sol decides, in the beam's order */
    int norder;
    mr_change hist[MR_HIST];
    int nh, moves;
} mr_game;

/* the beam: what it crosses (bit d of side[c]: it goes through side d of cell c, to or from its middle), how far
 * along it reaches a cell's middle (cells; 0xffff: never), the gems it lights, the bomb it touches */
typedef struct {
    uint8_t side[MR_CELLS];
    uint16_t dist[MR_CELLS];
    int gems, lit, bomb;                        /* bomb: its cell, or -1 */
    int length;                                 /* the farthest dist */
} mr_beam;

static inline int mr_reflect(int dir, int way) { return way == MR_SLASH ? dir ^ 1 : 3 - dir; }

void mr_trace(const mr_game *g, mr_beam *b);
int  mr_solved(const mr_game *g);
int  mr_loose_left(const mr_game *g);           /* loose mirrors still in the tray */
/* the ways to solve the puzzle as it was given (the loose mirrors off the board), counted up to limit; the first in
 * g->sol and g->order; *work: the choices the search tried */
int  mr_solve(mr_game *g, int limit, long *work);
void mr_new(mr_game *g, int level, uint32_t seed);

int  mr_tap(mr_game *g, int cell);              /* 0: nothing to do there */
int  mr_set(mr_game *g, int cell, int to);      /* to: 0 '/', 1 '\', 2 a loose mirror taken off; 0: not allowed */
int  mr_move(mr_game *g, int from, int to);     /* a loose mirror carried to an empty cell (its way kept) */
int  mr_hint(mr_game *g);                       /* one step towards the solution: the cell changed; -1: none */
int  mr_undo(mr_game *g);                       /* the last action taken back: a cell it changed; -1: none */
int  mr_cell(const mr_game *g, const char *s);  /* "c4": its cell; -1: none */
int  mr_script(mr_game *g, const char *moves);  /* 0 at the first bad one */

#endif
