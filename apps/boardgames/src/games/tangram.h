/*
 * Tangram for one player: the rules. The seven traditional tans, cut from a square: two large triangles, a medium
 * one, two small ones, a square and a parallelogram. Lay all seven, without overlapping, to fill a silhouette.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Units: a small triangle's short side is 1. The tans then are: large triangles with sides 2, 2 (area 2 each), the
 * medium one 2 along its long side (area 1), small ones 1, 1 (area 1/2), the square 1, the parallelogram 1 by 1
 * (sides 1 and the square root of 2): area 8 in all. The silhouettes are our own (tangram_figures.h): each made by
 * laying the seven tans on the grid of whole units, at right angles, so that every edge runs along the grid or across
 * a cell's diagonal; the silhouette is their union. A silhouette is thus a set of quarter cells (each cell cut by its
 * two diagonals into four), 32 of them, checked by tests/tangram_test.c (no two tans overlap, they hold together).
 * Three sets of 20: animals, people, things. A piece turns by eighths of a turn about its middle; the parallelogram
 * also turns over. Laid with its corners on the grid (a piece let go is drawn to the nearest grid point, when it
 * stands at right angles), it covers whole quarter cells. Solved: the seven cover exactly the silhouette's quarters
 * (any of the two large triangles in either large place, a mirrored solution as good as the made one).
 * The table (units): the silhouette at the top, its grid's corner at (fx, 0); the tray under it, where the tans lie at
 * first, each turned at random by quarter turns.
 * The notation (script, the screenshots, the tests): "s5" the set's fifth silhouette (from 1); "h" a hint (a tan moved
 * into its place); "=" hints until solved; "r3" turns tan 3 an eighth (0..6: the large triangles, the medium, the
 * small ones, the square, the parallelogram), "f6" turns the parallelogram over; "m3:4,1" moves tan 3 so that its
 * first corner lies on the grid point (4, 1) of the silhouette.
 */
#ifndef TANGRAM_H
#define TANGRAM_H

#include <stdint.h>

#define TGM_PIECES 7
#define TGM_SETS 3
#define TGM_PER_SET 20
#define TGM_FIGURES (TGM_SETS * TGM_PER_SET)
#define TGM_MAXC 12                             /* a silhouette's cells across and down, at most */
#define TGM_HIST 1024

enum { TGM_L, TGM_M, TGM_S, TGM_Q, TGM_P };

typedef struct { float x, y; int rot, flip; } tgm_place;   /* its middle (units), eighths of a turn, turned over */

typedef struct {
    int figure, tw, th;                         /* the silhouette, its cells */
    uint32_t seed;
    float fx, table_w, table_h;                 /* its corner on the table; the table */
    uint8_t target[TGM_MAXC][TGM_MAXC];         /* its quarters: bit q of the cell (x, y): 0 top, 1 right, 2 bottom,
                                                 * 3 left */
    tgm_place slot[TGM_PIECES];                 /* the made solution's places (the hint's) */
    tgm_place p[TGM_PIECES];
    struct { int8_t piece; tgm_place was; } hist[TGM_HIST];
    int nh, moves;
} tgm_game;

extern const char *const tgm_figures[TGM_FIGURES];

int  tgm_type(int piece);                       /* TGM_L, TGM_L, TGM_M, TGM_S, TGM_S, TGM_Q, TGM_P */
/* a piece's corners on the table (units): how many (3 or 4) */
int  tgm_corners(int piece, const tgm_place *pl, float *xy);
/* a figure's tans: their places from its description (the silhouette's grid corner at the origin); 0: bad */
int  tgm_parse(const char *desc, tgm_place *out);
/* the quarters a placed piece covers (on the grid: right angles, corners on grid points): cells relative to the
 * silhouette's corner; how many (0: not on the grid). qx, qy, qq: up to 8 each */
int  tgm_quarters(const tgm_game *g, int piece, const tgm_place *pl, int *qx, int *qy, int *qq);
void tgm_new(tgm_game *g, int set, int index, uint32_t seed);
int  tgm_snap(const tgm_game *g, int piece, tgm_place *pl);   /* drawn to the grid at right angles: 1 moved */
int  tgm_move(tgm_game *g, int piece, float x, float y);   /* its middle there (kept on the table), snapped */
int  tgm_turn(tgm_game *g, int piece);          /* an eighth of a turn, about its middle */
int  tgm_flip(tgm_game *g, int piece);          /* the parallelogram turned over; 0 for the others */
int  tgm_undo(tgm_game *g);                     /* the piece put back; -1: none */
int  tgm_in_place(const tgm_game *g, int piece);   /* it covers a place of the made solution exactly */
int  tgm_solved(const tgm_game *g);
int  tgm_hint(tgm_game *g);                     /* a piece moved into its place: which; -1 none */
int  tgm_script(tgm_game *g, const char *moves);

#endif
