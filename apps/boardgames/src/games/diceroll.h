/*
 * Dice Roll for one player: the rules. An ivory die stands on a path of stone tiles floating over the table. It rolls
 * over one of its bottom edges onto the next tile, up, down, left or right, and a new face comes up. Bring it to the
 * goal tile with the goal's number on top.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The die: opposite faces add up to 7; at the start 1 on top, 2 facing down the map (south, +y), 3 facing right
 * (east, +x), unless the map says otherwise. Its 24 ways of lying are rotations of the model (dcr_rot): the face of
 * model axis +z is 1, +y 2, +x 3, -x 4, -y 5, -z 6.
 * The tiles of the later chapters:
 * - a number tile (its pips inlaid in lapis): the die may roll onto it only with that number coming up on top;
 * - an odd tile (one ring) takes the die only with an odd number on top, an even tile (two rings) an even one;
 * - an arrow tile is crossed one way only: the die rolls onto it and off it along its arrow, never another way.
 * The goal tile is a plain tile for every other face; the die on it with the goal's number on top: solved.
 * A move: one roll. The par: the fewest rolls, by a breadth-first search over the die's places and ways of lying.
 * The puzzles: made by dcr_make, for the levels (tools/diceroll_gen.c, offline) and at random alike: a symmetric
 * board (a mirror, a half turn or a quarter turn), its holes and special tiles laid in symmetric sets; then sets of
 * cells changed one at a time while the fewest rolls do not drop. The random variants: 0 easy (number tiles), 1
 * medium (odd and even tiles too), 2 hard (arrows too).
 * The levels: 150 of our own in six chapters (diceroll_levels.h: the maps and each one's par, checked by the tests).
 * A map as text: three digits (the goal's number, the start's top and south faces), ':', then rows from the top
 * split by '|': '.' the void, 'o' a tile, 'S' the start (a tile), 'G' the goal, '1'..'6' a number tile, 'i' an odd
 * tile, 'e' an even tile, '^' '>' 'v' '<' an arrow (up, right, down, left on the map). Cells x + y * w from the top
 * left. The notation (script, the screenshots, the tests): u r d l a roll, "h" a hint (the first roll of a shortest
 * way, played), "=" the whole shortest way.
 */
#ifndef DICEROLL_H
#define DICEROLL_H

#include <stdint.h>

#define DCR_MAXW 9
#define DCR_MAXH 9
#define DCR_CELLS (DCR_MAXW * DCR_MAXH)
#define DCR_HIST 1024
#define DCR_LEVELS 150
#define DCR_CHAPTERS 6
#define DCR_VARIANTS 3
#define DCR_TEXT (DCR_CELLS + DCR_MAXH + 8)

enum { DCR_UP, DCR_RIGHT, DCR_DOWN, DCR_LEFT };  /* as swipepad.h's ways: up is -y on the map */
enum { DCR_VOID, DCR_TILE, DCR_NUM, DCR_ODD, DCR_EVEN, DCR_ARROW, DCR_GOAL };

typedef struct {
    int w, h;
    uint8_t t[DCR_CELLS];                       /* DCR_* tiles */
    uint8_t k[DCR_CELLS];                       /* a number tile's number 1..6, an arrow's way */
    int start, goal, face;                      /* face: the number the goal wants on top */
    int o0;                                     /* the die's way at the start (0..23) */
} dcr_map;

typedef struct { int8_t x, y, o; } dcr_state;

typedef struct {
    dcr_map m;
    dcr_state s;
    dcr_state hist[DCR_HIST];                   /* the state before each roll */
    int nh, moves, par;
} dcr_game;

typedef struct {
    int w, h;
    int holes;                                  /* symmetric sets of void cells laid at first */
    int nums, parity, arrows;                   /* symmetric sets of each special tile laid at first */
    int iters;                                  /* the changes tried */
} dcr_spec;

/* the die's 24 ways: o's rotation (model to map; m[row][col], entries -1, 0, 1), its faces on top, south, east */
const int8_t *dcr_rot(int o);                   /* 9 entries */
int  dcr_top(int o);
int  dcr_south(int o);
int  dcr_east(int o);
int  dcr_roll(int o, int dir);                  /* the way after a roll */
int  dcr_way_of(int top, int south);            /* -1: no such way */

int  dcr_parse(dcr_map *m, const char *text);   /* 0: not a map */
void dcr_text(const dcr_map *m, char *out);     /* DCR_TEXT bytes */
void dcr_begin(dcr_game *g);
/* may the die roll from s the way dir: 1 and the state after it in *out */
int  dcr_try(const dcr_map *m, const dcr_state *s, int dir, dcr_state *out);
int  dcr_move(dcr_game *g, int dir);            /* 1: rolled (kept for undo), 0: refused */
int  dcr_undo(dcr_game *g);
int  dcr_solved(const dcr_map *m, const dcr_state *s);
/* the fewest rolls from s (0 solved, -1 none), the first one's way (may be NULL); *states: the states the search
 * went through, *ways: how many shortest ways there are (capped at 1000) (both may be NULL) */
int  dcr_solve(const dcr_map *m, const dcr_state *s, int *dir, int *states, int *ways);
void dcr_make(dcr_map *m, const dcr_spec *spec, uint32_t seed);
int  dcr_script(dcr_game *g, const char *moves);
extern const char *const dcr_levels[DCR_LEVELS];
extern const uint8_t dcr_level_par[DCR_LEVELS];
int  dcr_level(dcr_game *g, int level);         /* 0: none */
void dcr_variant(int variant, dcr_spec *spec);
void dcr_random(dcr_game *g, int variant, uint32_t seed);

#endif
