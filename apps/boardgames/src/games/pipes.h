/*
 * Pipes for one player: the rules. A square grid of pipe tiles (an end, a straight, a bend or a tee on each) and one
 * water source. A tap turns a tile a quarter turn clockwise. Solved when every tile joins one network fed by the source
 * and no pipe ends open (each pipe meets a pipe of its neighbour, none runs into the board's edge): the water then
 * fills every tile.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 5 x 5, 7 x 7, 9 x 9, 11 x 11, and 9 x 9 wrapped (its edges joined: a pipe leaving the right edge comes
 * in at the left, the bottom at the top). The source sits in the middle.
 * The maker: a tree spanning the grid, grown by chance from the source (an edge to a tile not yet in it, picked at
 * random among all such edges; no tile with four pipes, so no crosses), the tiles read from it, then each turned by
 * chance. A puzzle is kept when the deduction solver (pp_deduce) finds every tile without guessing, which also means
 * it has one solution; after many tries the maker takes one that does not (rare, and still solvable: any network
 * that fills every tile counts). The deductions, as a person makes them: a tile's turns that fit what is known of
 * its sides; a pipe that would close a loop; a group of joined tiles that would be shut off from the rest (it must
 * keep a way out: its last one is forced open).
 * The levels (pipes_levels.h, made by tools/pipes_gen.c): 150 puzzles in six chapters, 4 x 4 to 11 x 11, the last
 * ones wrapped; each kept as its size and a character a tile (its solution's sides and the turns that scramble it).
 * Stars: the moves (each a quarter turn) against par, the fewest quarter turns from the start to the one solution
 * (pp_load works it out); a hint caps them (pzkit.h's rule).
 * Tiles: x + y * n from the top left. Sides: bit 0 up (north), 1 right, 2 down, 3 left. The notation (script, the
 * screenshots, the tests): a tile is its column letter a.. and its row 1.. from the top; "c4" turns it once, "c4*3"
 * three times (one action each), "?" a hint, "=n" puts the next n tiles that are wrong right (reading order, one
 * action each).
 */
#ifndef PIPES_H
#define PIPES_H

#include <stdint.h>

#define PP_VARIANTS 5
#define PP_MAX 11
#define PP_TILES (PP_MAX * PP_MAX)
#define PP_HIST 4096
#define PP_LEVELS 150
#define PP_CHAPTERS 6

enum { PP_UP = 1, PP_RIGHT = 2, PP_DOWN = 4, PP_LEFT = 8 };
enum { PP_END, PP_STRAIGHT, PP_BEND, PP_TEE };   /* the shapes, drawn turned: end up; straight up-down; bend
                                                 * up-right; tee up-right-down */

typedef struct { uint8_t tile, was; } pp_change;

typedef struct {
    int n, wrap, source;
    uint8_t sol[PP_TILES];                      /* the sides with a pipe, in the maker's solution */
    uint8_t now[PP_TILES];                      /* the sides with a pipe now (the tile as the player turned it) */
    pp_change hist[PP_HIST];                    /* each action's tile and its sides before it */
    int nh, moves;
    int par;                                    /* the fewest quarter turns from the start to the solution */
    int hints;                                  /* the hints given (undo does not take them back) */
} pp_game;

void pp_variant(int variant, int *n, int *wrap);
uint8_t pp_turn(uint8_t sides, int k);          /* the sides turned k quarter turns clockwise */
int  pp_shape(uint8_t sides, int *k);           /* the shape (PP_END..PP_TEE), and the turns from its drawing */
/* the tile beside t on side d (0 up .. 3 left); -1 off the board (never when wrapped) */
int  pp_next(int n, int wrap, int t, int d);

/* a puzzle: the solution's sides and the source; how many tries it took (tries > PP_TRIES: not deducible) */
#define PP_TRIES 60
int  pp_make(int n, int wrap, uint32_t seed, uint8_t *sol, int *source);
/* the deduction solver: from the tiles' shapes (sides: any turn of each), what can be found without guessing; out (may
 * be NULL): the sides found (0xff: a tile not found); returns the tiles found (n * n: solved) or -1: a contradiction */
int  pp_deduce(int n, int wrap, const uint8_t *sides, uint8_t *out);
/* the solutions (any turn of each tile making one network with no open end), counted up to limit */
int  pp_count(int n, int wrap, const uint8_t *sides, int limit);
/* how hard the deductions are: the fewest kinds of them that find every tile (0: each tile's own sides; 1: also no
 * loop; 2: also every group of joined tiles keeps a way out; 3: also two groups never joined into one shut off), -1:
 * none does; *rounds: the passes of deductions it took */
int  pp_grade(int n, int wrap, const uint8_t *sides, int *rounds);
/* a level kept as text: "<n>[w]:" then a character a tile (PP_CODE[sides of the solution * 4 + turns: the start is
 * the solution turned that many quarter turns]); the source in the middle. pp_decode: 0 when it is not one */
#define PP_CODE "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_"
int  pp_encode(const pp_game *g, char *out, int size);
int  pp_decode(pp_game *g, const char *code);

void pp_new(pp_game *g, int variant, uint32_t seed);
/* a puzzle set up from its solution and its start (moves, hints 0, its par worked out) */
void pp_load(pp_game *g, int n, int wrap, int source, const uint8_t *sol, const uint8_t *now);
int  pp_par(int n, const uint8_t *sol, const uint8_t *now);   /* the fewest quarter turns from now to sol */
int  pp_tap(pp_game *g, int t);                 /* a quarter turn clockwise: one action; 0: not a tile, or solved */
int  pp_hint(pp_game *g);                       /* a wrong tile turned right (next to the water if it can): the tile;
                                                 * -1: none. One action, counted in hints */
int  pp_undo(pp_game *g);                       /* the last action taken back: its tile; -1: none */
/* the water: each tile's distance from the source along joined pipes (-1: dry), and the tile it comes from (-1: the
 * source, or dry); how many tiles are wet. from may be NULL. */
int  pp_flow(const pp_game *g, int *dist, int *from);
int  pp_open(const pp_game *g, int t, int d);   /* 1: tile t has a pipe on side d that meets no pipe */
int  pp_solved(const pp_game *g);
int  pp_wrong(const pp_game *g);                /* the tiles not as in the solution (a straight either way) */
int  pp_tile(const pp_game *g, const char *s);  /* "c4": its tile; -1: none */
int  pp_script(pp_game *g, const char *moves);  /* 0 at the first bad one */

#endif
