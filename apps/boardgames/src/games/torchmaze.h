/*
 * Torch Maze for one player: the rules. A little explorer stands in a maze in the dark, its torch lighting only a few
 * cells round it. Walk to the exit: stairs going down, whose glow shows through the dark. What the torch has shown
 * stays sketched on the explorer's map, so exploring builds the map.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The maze: w x h cells (some of them rock, outside its shape: a disc, a cross, a ring...), an edge between two
 * neighbours: a wall, open, or one of:
 * - a door of a colour: shut until the explorer holds that colour's key (picked up by walking onto it; kept);
 * - a one-way door: walked through one way only (its way);
 * - a gate (a portcullis): open or shut; every pressure plate, walked onto, turns every gate (the open ones shut, the
 *   shut ones open);
 * A cell may hold a key, a brazier (walked onto: lit for good, its light shows its part of the maze for good), an oil
 * flask (walked onto: the torch filled up again), a pressure plate, the exit.
 * The torch's fuel (some levels): a step burns one; its light shrinks with it; out, the explorer cannot walk on (undo).
 * The map's memory: a cell the light has shown stays sketched; in some levels only for `fade` steps after the light
 * last showed it, then it fades (a lit brazier's cells never fade). Undo takes the explorer back, never the memory.
 * A move: a step (one cell) or a run (steps on along a corridor, round its bends, to the next junction, room, door or
 * thing). The steps: those of the walk as it stands (undo takes them back). The best: the fewest steps knowing the
 * whole maze (a breadth-first search over the explorer's cell, keys, gates, flasks and fuel); the par: that times the
 * level's allowance (exploring costs steps), rounded.
 * The puzzles: made by tmz_make, for the levels (tools/torchmaze_gen.c, offline) and at random alike: a shape, rooms,
 * a maze grown through it (the newest cell or one at random: long corridors or many short ones), loops opened, then
 * the start and the exit chosen where an explorer is led astray, then doors and their keys, gates and plates, one-way
 * doors, flasks and the fuel, braziers. tmz_explore plays a sensible explorer who sees only what its torch shows (the
 * difficulty: how far it walks, whether it fails).
 * A map as text: "w,h,r,fuel,fade;" (r: the light's radius in tenths of a cell), then the cells two to a character
 * (tmz_alpha, three bits each: in the maze, open to the right, open below), ';', then the things: S start, X exit,
 * K<colour> key, B brazier, O oil, P plate, then a cell's number; D<colour><r|d> door, W<way><r|d> one-way, G<0 shut|
 * 1 open><r|d> gate, then the cell whose right or lower edge it is; space separated. Cells x + y * w from the top left.
 * The notation (script, the screenshots, the tests): u r d l a step, U R D L a run, "h" a hint (counted), "=" the rest
 * of a shortest way, "=k" its next k steps.
 */
#ifndef TORCHMAZE_H
#define TORCHMAZE_H

#include <stdint.h>

#define TMZ_MAXW 19
#define TMZ_MAXH 19
#define TMZ_CELLS (TMZ_MAXW * TMZ_MAXH)
#define TMZ_HIST 2048
#define TMZ_LEVELS 150
#define TMZ_CHAPTERS 6
#define TMZ_VARIANTS 3
#define TMZ_COLOURS 3
#define TMZ_FLASKS 3
#define TMZ_BRAZIERS 4
#define TMZ_TEXT (TMZ_CELLS / 2 + 512)
#define TMZ_PATH 1024

enum { TMZ_UP, TMZ_RIGHT, TMZ_DOWN, TMZ_LEFT };
enum { TMZ_WALL, TMZ_OPEN, TMZ_DOOR, TMZ_ONEWAY, TMZ_GATE };              /* edges */
enum { TMZ_NONE, TMZ_KEY, TMZ_BRAZIER, TMZ_OIL, TMZ_PLATE, TMZ_EXIT };    /* things in cells */

typedef struct {
    int w, h;
    uint8_t in[TMZ_CELLS];                      /* 1: a cell of the maze; 0: rock */
    uint8_t e[2][TMZ_CELLS];                    /* the edge to the right (0) and below (1): TMZ_WALL.. */
    uint8_t k[2][TMZ_CELLS];                    /* a door's colour, a one-way door's way, a gate open at first (1) */
    uint8_t item[TMZ_CELLS], ik[TMZ_CELLS];     /* TMZ_NONE..; a key's colour, a brazier's or a flask's number */
    int start, exit;
    int radius;                                 /* the torch's light, tenths of a cell */
    int fuel;                                   /* the torch's fuel when full (steps); 0: no fuel */
    int fade;                                   /* the steps a cell stays on the map; 0: for good */
    int nflask, nbraz, ncol;                    /* flasks, braziers, key colours */
} tmz_map;

typedef struct {
    int16_t at;
    uint8_t keys, gates, flasks, face;          /* keys held (bits); the gates turned (0, 1); flasks taken; facing */
    int16_t fuel, steps;
} tmz_state;

typedef struct {
    tmz_map m;
    tmz_state s;
    tmz_state hist[TMZ_HIST];                   /* the state before each move (a step or a run) */
    int nh;
    uint16_t tick;                              /* the map's clock: a step or an undo each */
    uint16_t first[TMZ_CELLS], last[TMZ_CELLS]; /* when the light first and last showed each cell (0: never) */
    uint8_t lit;                                /* the braziers lit (bits) */
    int best, par, hints;
    int allow;                                  /* the par's allowance, hundredths */
} tmz_game;

/* the result of a search: the fewest steps (-1 none), the cells of a shortest way (path[0] the next cell) */
typedef struct { int steps, n, states; int16_t path[TMZ_PATH]; } tmz_way;

typedef struct {
    int w, h, shape;                            /* shape: 0 a rectangle, 1 a disc, 2 a diamond, 3 a cross, 4 a ring, 5
                                                 * two halls, 6 a hall's corners cut */
    int newest;                                 /* percent: the maze grown from its newest cell (long corridors) */
    int braid;                                  /* percent of the dead ends opened into loops */
    int rooms;                                  /* rooms laid first */
    int colours, gates, plates, opens, oneways, flasks, slack, braziers;
    int radius, fade;
    int tries;                                  /* starts and exits tried */
} tmz_spec;

typedef struct {
    int best;                                   /* the fewest steps */
    int naive[2];                               /* a sensible explorer's steps (two ways of choosing; -1 failed) */
    int deadends, states;
    double ratio, score;                        /* the explorers' steps over the best; the difficulty */
} tmz_metric;

extern const char tmz_alpha[65];
int  tmz_parse(tmz_map *m, const char *text);   /* 0: not a map */
void tmz_text(const tmz_map *m, char *out);     /* TMZ_TEXT bytes */
int  tmz_next(const tmz_map *m, int c, int dir);   /* the cell next to c that way, -1 none (rock, the edge) */
int  tmz_opening(const tmz_map *m, int c, int dir);   /* 1: not a wall that way (a door, a gate count) */
int  tmz_can(const tmz_map *m, const tmz_state *s, int dir);   /* 1: the explorer may step that way */
float tmz_light(const tmz_map *m, const tmz_state *s);         /* the torch's radius now, cells (0: out) */
/* the light from (x0, y0) reaches (x1, y1) (cells: cell (i, j) spans i..i+1, j..j+1): no shut edge crossed (doors
 * shut, one-way doors, walls, rock; gates let light through), but an edge whose line lies within `tol` of (x1, y1)
 * across it (the light on a wall's top) */
int  tmz_ray(const tmz_map *m, const tmz_state *s, float x0, float y0, float x1, float y1, float tol);
void tmz_begin(tmz_game *g);
int  tmz_step(tmz_game *g, int dir);            /* 1: stepped (kept for undo) */
int  tmz_run(tmz_game *g, int dir, int16_t *cells, int *n);   /* the steps of a run (0 refused); cells walked */
int  tmz_undo(tmz_game *g);
int  tmz_solved(const tmz_game *g);
int  tmz_remembered(const tmz_game *g, int c, float *alpha);  /* on the map, and how clearly (0..1) */
void tmz_solve(const tmz_map *m, const tmz_state *s, tmz_way *w);
int  tmz_script(tmz_game *g, const char *moves);
/* a sensible explorer seeing only its torch's light, from the start; lambda: how much it heads for the exit's glow;
 * the steps it walks to the exit (-1: it fails: trapped, the fuel out, lost) */
int  tmz_explore(const tmz_map *m, double lambda, int cap);
void tmz_measure(const tmz_map *m, tmz_metric *mt);
int  tmz_make(tmz_map *m, const tmz_spec *spec, uint32_t seed);   /* 1: made */
void tmz_variant(int variant, tmz_spec *spec);
void tmz_random(tmz_game *g, int variant, uint32_t seed);
extern const char *const tmz_levels[TMZ_LEVELS];
extern const uint16_t tmz_level_best[TMZ_LEVELS];
extern const uint8_t tmz_allow[TMZ_CHAPTERS];   /* the par's allowance by chapter, tenths */
int  tmz_level(tmz_game *g, int level);         /* 0: none */
int  tmz_par_of(int best, int allow);           /* the par: best times allow hundredths, rounded */

#endif
