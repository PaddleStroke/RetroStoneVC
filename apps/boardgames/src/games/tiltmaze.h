/*
 * Tilt Maze for one player: the rules. Coloured marbles lie in a tray walled into a maze, with holes of their colours.
 * Tilt the tray up, down, left or right: every marble rolls that way until a wall or another marble stops it. A marble
 * rolling over a hole of its colour drops in and fills it (the others then roll over it); one rolling over a hole of
 * another colour drops in and sticks there: the puzzle cannot be finished (undo). Get every marble into a hole of its
 * colour in as few tilts as possible.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The tiles of the later chapters:
 * - a coloured gate lets the marbles of its colour through, and stops the others as a wall would;
 * - a one-way tile lets marbles roll onto it only in its arrow's way;
 * - a lidded hole is shut (the marbles roll over its lid) until every open hole of its colour is filled: once the
 *   marbles come to rest after that tilt, its lid lifts, and a marble resting on it drops in.
 * Walls stand between squares (and blocks fill a square). A move: one tilt; a tilt that moves no marble is refused.
 * The par: the fewest tilts, by a breadth-first search over the marbles' places (marbles of one colour alike).
 * The puzzles: made by tlm_make for the levels (tools/tiltmaze_gen.c, offline) and at random alike: walls laid with
 * a symmetry, the holes, marbles and special tiles by chance; then walls, holes and marbles changed one at a time
 * while the fewest tilts grow. The special tiles must matter (taken away, the fewest tilts change).
 * A map as text: rows from the top split by '/', two characters a square: what lies there, then its walls ('0' none,
 * '1' on its right side, '2' below it, '3' both). What lies there: '.' the floor, '#' a block, 'R' 'G' 'B' 'Y' a
 * marble of colour 0..3, 'r' 'g' 'b' 'y' a hole of that colour, '1'..'4' a lidded hole of colour 0..3, 'k' 'l' 'm' 'n'
 * a gate of colour 0..3, '^' '>' 'v' '<' a one-way tile. Squares x + y * w from the top left. The notation (script,
 * the screenshots, the tests): u r d l a tilt, "=" the rest of a shortest way, "=k" its next k tilts.
 */
#ifndef TILTMAZE_H
#define TILTMAZE_H

#include <stdint.h>

#define TLM_MAXW 9
#define TLM_MAXH 11
#define TLM_CELLS (TLM_MAXW * TLM_MAXH)
#define TLM_MAXM 6                              /* marbles (and holes) at most */
#define TLM_COLOURS 4
#define TLM_HIST 1024
#define TLM_LONGEST (TLM_MAXW + TLM_MAXH)
#define TLM_LEVELS 150
#define TLM_CHAPTERS 6
#define TLM_VARIANTS 3

enum { TLM_UP, TLM_RIGHT, TLM_DOWN, TLM_LEFT };   /* as swipepad.h's ways */
enum { TLM_FLOOR, TLM_BLOCK, TLM_HOLE, TLM_LID, TLM_GATE, TLM_ONEWAY };
enum { TLM_WALL_R = 1, TLM_WALL_D = 2 };

typedef struct {
    int w, h;
    uint8_t t[TLM_CELLS];                       /* TLM_* tiles */
    uint8_t k[TLM_CELLS];                       /* a hole's, a lid's or a gate's colour; a one-way tile's way */
    uint8_t wall[TLM_CELLS];                    /* TLM_WALL_R | TLM_WALL_D */
    int nm;                                     /* the marbles, by colour */
    uint8_t col[TLM_MAXM], start[TLM_MAXM];
} tlm_map;

typedef struct {
    uint8_t at[TLM_MAXM];                       /* each marble's square (in a hole: the hole's) */
    uint8_t in;                                 /* bit i: marble i dropped into a hole */
    uint8_t stuck;                              /* bit i: ... a hole of another colour */
} tlm_state;

/* a tilt's rolls, for the animation: marble i's squares rolled (its last one where it stopped or dropped in), then
 * dropped (1: into a hole on the way; 2: when a lid lifted under it) */
typedef struct {
    int len[TLM_MAXM], drop[TLM_MAXM];
    uint8_t from[TLM_MAXM];
} tlm_trace;

typedef struct {
    tlm_map m;
    tlm_state s;
    tlm_state hist[TLM_HIST];
    int nh, moves, par;
} tlm_game;

typedef struct {
    int w, h;
    int sym;                                    /* the walls' symmetry: 0 none, 1 left-right, 2 a half turn, 3 up-down;
                                                 * -1 drawn */
    int walls;                                  /* walls between squares, percent */
    int blocks;                                 /* blocks */
    int marbles[TLM_COLOURS];                   /* the marbles of each colour */
    int gates, oneway, lids;
    int lo, hi;                                 /* the par sought */
    int iters;
    int tries;                                  /* the trays begun afresh at most (0: 40) */
} tlm_spec;

int  tlm_parse(tlm_map *m, const char *text);   /* 0: not a map */
void tlm_text(const tlm_map *m, char *out);     /* 3 TLM_CELLS bytes */
int  tlm_wall(const tlm_map *m, int c, int dir);   /* a wall (or the edge) on square c's side `dir` */
int  tlm_lid_open(const tlm_map *m, const tlm_state *s, int c);   /* a lidded hole: its lid lifted */
/* a tilt from s: 1 and the state after it in *out (tr: its rolls; may be NULL); 0 no marble moves (or one is stuck) */
int  tlm_try(const tlm_map *m, const tlm_state *s, int dir, tlm_state *out, tlm_trace *tr);
int  tlm_solved(const tlm_map *m, const tlm_state *s);
int  tlm_dead(const tlm_state *s);              /* a marble stuck in another colour's hole */
/* the fewest tilts from s (0 solved, -1 none, -2 too big a search), the first one's way (may be NULL); *states: the
 * states the search went through (may be NULL) */
int  tlm_solve(const tlm_map *m, const tlm_state *s, int *dir, int *states);
void tlm_begin(tlm_game *g);
int  tlm_move(tlm_game *g, int dir);            /* 1 played (kept for undo), 0 refused */
int  tlm_undo(tlm_game *g);
int  tlm_hint(const tlm_game *g);               /* the next tilt of a shortest way; -1: none from here */
int  tlm_script(tlm_game *g, const char *moves);
int  tlm_make(tlm_map *m, const tlm_spec *spec, uint32_t seed);   /* 1: the par in range (else the best kept) */
void tlm_variant(int variant, tlm_spec *spec);
void tlm_random(tlm_game *g, int variant, uint32_t seed);
extern const char *const tlm_levels[TLM_LEVELS];
extern const uint8_t tlm_level_par[TLM_LEVELS];
int  tlm_level(tlm_game *g, int level);

#endif
