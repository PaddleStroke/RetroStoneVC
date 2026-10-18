/*
 * Paint Roller for one player: the rules. A ball wet with paint lies on a wooden floor walled with walnut blocks.
 * Pushed up, down, left or right, it rolls until a wall (or the floor's edge) stops it, painting every tile it rolls
 * over. Paint the whole floor.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The tile the ball starts on is painted. The tiles of the later chapters:
 * - a double tile (a double groove round it) needs two coats: the ball must roll onto it twice;
 * - portals come in pairs (brass rings with a jewel of the pair's colour): a ball rolling into one comes out of the
 *   other and rolls on the same way; portals take no paint. A ball stopped on a portal leaves it as any tile;
 * - an arrow tile is crossed one way: the ball rolls onto it and off it along its arrow only (against or across it,
 *   it stops before it, as before a wall).
 * A push that moves nothing is no move. A move: one push. The par: the fewest pushes, by a breadth-first search over
 * the ball's places and the paint (each tile's coats).
 * The puzzles: made by pnr_make, for the levels (tools/paintroller_gen.c, offline) and at random alike: a symmetric
 * floor (a mirror or a half turn), its walls, double tiles, portals and arrows laid in symmetric sets; then sets
 * changed one at a time while the fewest pushes do not drop (the floor always paintable whole). The random variants:
 * 0 small, 1 medium (double tiles), 2 large (portals and arrows too).
 * The levels: 150 of our own in six chapters (paintroller_levels.h: the maps and each one's par, checked by the tests).
 * A map as text, rows from the top split by '|': '#' a wall, '.' a tile, 'S' the start (a tile), '2' a double tile,
 * 'A' 'B' 'C' the two portals of a pair, '^' '>' 'v' '<' an arrow (up, right, down, left). Cells x + y * w from the
 * top left. The notation: u r d l a push, "h" a hint (the first push of a shortest way, played), "=" all of it.
 */
#ifndef PAINTROLLER_H
#define PAINTROLLER_H

#include <stdint.h>

#define PNR_MAXW 10
#define PNR_MAXH 12
#define PNR_CELLS (PNR_MAXW * PNR_MAXH)
#define PNR_WORDS 3                             /* the paint's bits: a coat a tile (two for a double tile) */
#define PNR_HIST 1024
#define PNR_LEVELS 150
#define PNR_CHAPTERS 6
#define PNR_VARIANTS 3
#define PNR_TEXT (PNR_CELLS + PNR_MAXH + 1)
#define PNR_PATH (PNR_CELLS * 4)

enum { PNR_UP, PNR_RIGHT, PNR_DOWN, PNR_LEFT };  /* as swipepad.h's ways */
enum { PNR_WALL, PNR_TILE, PNR_DOUBLE, PNR_PORTAL, PNR_ARROW };

typedef struct {
    int w, h;
    uint8_t t[PNR_CELLS];                       /* PNR_* */
    uint8_t k[PNR_CELLS];                       /* a portal's pair 0..2, an arrow's way */
    int16_t other[PNR_CELLS];                   /* a portal's partner */
    int16_t bit[PNR_CELLS][2];                  /* each coat's bit (-1: none) */
    int start, nbits;
} pnr_map;

typedef struct {
    uint64_t paint[PNR_WORDS];
    int pos;
} pnr_state;

typedef struct {
    pnr_map m;
    pnr_state s;
    pnr_state hist[PNR_HIST];
    int nh, moves, par;
} pnr_game;

typedef struct {
    int w, h;
    int rolls;                                  /* the rolls of the ball carving the floor at first */
    int doubles, portals, arrows;               /* symmetric sets of each special tile laid at first */
    int iters;                                  /* the changes tried */
    int cap;                                    /* the search's states at most */
} pnr_spec;

int  pnr_parse(pnr_map *m, const char *text);   /* 0: not a map */
void pnr_text(const pnr_map *m, char *out);     /* PNR_TEXT bytes */
void pnr_begin(pnr_game *g);
int  pnr_coats(const pnr_map *m, const pnr_state *s, int c);   /* the coats tile c has (0..2) */
int  pnr_need(const pnr_map *m, int c);         /* the coats it needs (0 a wall or a portal, 1, 2) */
/* a push from s the way dir: 1 and the state after it (0: nothing moves); path: the cells the ball goes through in
 * order, the start first (a portal's two cells both, one after the other), *n how many (both may be NULL) */
int  pnr_try(const pnr_map *m, const pnr_state *s, int dir, pnr_state *out, int *path, int *n);
int  pnr_move(pnr_game *g, int dir);            /* 1: pushed (kept for undo), 0: refused */
int  pnr_undo(pnr_game *g);
int  pnr_solved(const pnr_map *m, const pnr_state *s);
/* the fewest pushes from s (0 solved, -1 none or more than cap states), the first one's way (may be NULL);
 * *states: the states the search went through (may be NULL) */
int  pnr_solve(const pnr_map *m, const pnr_state *s, int cap, int *dir, int *states);
void pnr_make(pnr_map *m, const pnr_spec *spec, uint32_t seed);
int  pnr_script(pnr_game *g, const char *moves);
extern const char *const pnr_levels[PNR_LEVELS];
extern const uint8_t pnr_level_par[PNR_LEVELS];
int  pnr_level(pnr_game *g, int level);         /* 0: none */
void pnr_variant(int variant, pnr_spec *spec);
void pnr_random(pnr_game *g, int variant, uint32_t seed);

#endif
