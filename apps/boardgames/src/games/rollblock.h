/*
 * Roll the Block for one player: the rules. A wooden block of 1 x 1 x 2 stands on a path of stone tiles floating over
 * the table. It rolls over one of its edges, up, down, left or right: standing, it falls flat onto the two tiles that
 * way; lying, it either rolls along onto two tiles beside it or stands up on the tile past its end. It must never hang
 * over the void. Bring it to the hole and stand it up on it: it drops in.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The tiles of the later chapters:
 * - pale cracked tiles bear the block lying, never standing (all its weight on one tile);
 * - brass buttons open and shut bridges (the tiles with a coloured inlay of the button's jewel): a round button when
 *   any part of the block rolls onto it, a cross only under the block standing; each time, the bridges of its colour
 *   change (shut ones open, open ones shut). A bridge shut under the block is a move refused;
 * - a split tile: the block standing on it splits in two cubes, sent to the two rings of the split tile's colour. A
 *   cube rolls one tile at a time (any tile but a shut bridge or the other cube: a cube is light, a cracked tile bears
 *   it, a cross does not feel it; a round button does); one cube moves at a time (choosing the other one is not a
 *   move). Two cubes side by side join again into the block, lying on both.
 * A move: one roll (of the block or of a cube). The par: the fewest moves, by a breadth-first search over the places
 * the block can be (with the bridges' states, the cubes' places).
 * The puzzles: made by rlb_make, for the levels (tools/rollblock_gen.c, offline) and at random alike: a full rectangle
 * of tiles, the start and the hole far apart, the special tiles laid by chance; then tiles taken away, moved or changed
 * one at a time, each change kept when the fewest moves do not drop (and grow, for a tile put back), the special tiles
 * kept needed (a puzzle that could be solved without its bridges, its split tile or its cracked tiles is not kept);
 * last the tiles the block can never touch are taken away. The random variants: 0 easy (plain tiles), 1 medium
 * (cracked tiles, a bridge), 2 hard (all of them, a split tile).
 * The levels: 150 of our own in six chapters: plain tiles, longer ways, cracked tiles, bridges, the split tile, all of
 * them (rollblock_levels.h: the maps and each one's par, checked by the tests).
 * A map as text, rows from the top split by '|': '.' the void, 'o' a tile, 'S' the start (a tile, the block standing
 * on it), 'H' the hole, 'f' a cracked tile, 'a'..'d' a round button of colour 1..4, 'A'..'D' a cross, '1'..'4' a
 * bridge of that colour shut at the start, '5'..'8' one open at the start, 'x' the split tile, 'y' its two rings.
 * Cells x + y * w from the top left. The notation (script, the screenshots, the tests): u r d l a roll (of the cube
 * chosen, when split), 'x' the other cube chosen, "h" a hint (the first move of a shortest way, played), "=" the
 * whole shortest way.
 */
#ifndef ROLLBLOCK_H
#define ROLLBLOCK_H

#include <stdint.h>

#define RLB_MAXW 12
#define RLB_MAXH 12
#define RLB_CELLS (RLB_MAXW * RLB_MAXH)
#define RLB_GROUPS 4
#define RLB_HIST 1024
#define RLB_LEVELS 150
#define RLB_CHAPTERS 6
#define RLB_VARIANTS 3

enum { RLB_UP, RLB_RIGHT, RLB_DOWN, RLB_LEFT };   /* as swipepad.h's ways */
enum { RLB_VOID, RLB_TILE, RLB_FRAGILE, RLB_SOFT, RLB_HEAVY, RLB_BRIDGE, RLB_SPLIT, RLB_RING, RLB_HOLE };
enum { RLB_STAND, RLB_LIE_X, RLB_LIE_Y, RLB_TWO };   /* the block's ways: standing, lying along a row or a column,
                                                    * split in two cubes */
/* what a move did (rlb_try's events) */
enum { RLB_EV_SWITCH = 1, RLB_EV_SPLIT = 2, RLB_EV_JOIN = 4, RLB_EV_WIN = 8 };

typedef struct {
    int w, h;
    uint8_t t[RLB_CELLS];                       /* RLB_* tiles */
    uint8_t grp[RLB_CELLS];                     /* a button's or a bridge's colour 0..3 */
    uint8_t open0;                              /* the bridges open at the start (bit g: colour g) */
    int start, hole, split, ring[2];            /* split, ring: -1 none */
} rlb_map;

typedef struct {
    int8_t x, y, o;                             /* the block's top left cell and way; RLB_TWO: the first cube */
    int8_t x2, y2;                              /* RLB_TWO: the second cube */
    int8_t act;                                 /* RLB_TWO: the cube that moves (0 the first, 1 the second) */
    uint8_t open;                               /* the bridges open now */
} rlb_state;

typedef struct {
    rlb_map m;
    rlb_state s;
    rlb_state hist[RLB_HIST];                   /* the state before each move */
    int nh, moves, par;
} rlb_game;

typedef struct {
    int w, h;
    int fragile;                                /* cracked tiles: 1 */
    int bridges;                                /* colours of buttons and bridges: 0..2 */
    int split;                                  /* a split tile: 1 */
    int iters;                                  /* the changes tried */
} rlb_spec;

int  rlb_parse(rlb_map *m, const char *text);   /* 0: not a map */
void rlb_text(const rlb_map *m, char *out);     /* RLB_CELLS + RLB_MAXH + 1 bytes */
void rlb_begin(rlb_game *g);                    /* the block at the start, standing; no moves */
/* the cells a state covers: how many (1 or 2; RLB_TWO: the two cubes) */
int  rlb_cells(const rlb_state *s, int w, int *c);
/* a roll from s the way dir (the chosen cube's, split): 1 and the state after it in *out (events: RLB_EV_*), 0 refused
 * (the block would hang over the void, stand on a cracked tile, or a cube would run into the other one) */
int  rlb_try(const rlb_map *m, const rlb_state *s, int dir, rlb_state *out, int *events);
int  rlb_move(rlb_game *g, int dir);            /* a roll played (kept for undo); its events + 16, 0: refused */
int  rlb_choose(rlb_game *g);                   /* split: the other cube chosen (no move); 0: not split */
int  rlb_undo(rlb_game *g);                     /* 0: nothing to undo */
int  rlb_solved(const rlb_state *s, const rlb_map *m);
/* the fewest moves from s (0 solved, -1 none), the first one's way and cube (RLB_TWO: 0 the first, 1 the second;
 * may be NULL); *states: how many states the search went through (may be NULL) */
int  rlb_solve(const rlb_map *m, const rlb_state *s, int *dir, int *cube, int *states);
/* a puzzle made by chance to the spec */
void rlb_make(rlb_map *m, const rlb_spec *spec, uint32_t seed);
int  rlb_script(rlb_game *g, const char *moves);
extern const char *const rlb_levels[RLB_LEVELS];
extern const uint8_t rlb_level_par[RLB_LEVELS];
int  rlb_level(rlb_game *g, int level);         /* 0: none */
void rlb_variant(int variant, rlb_spec *spec);
void rlb_random(rlb_game *g, int variant, uint32_t seed);

#endif
