/*
 * Tangram for one player: the rules. The seven traditional tans, cut from a square: two large triangles, a medium
 * one, two small ones, a square and a parallelogram. Lay all seven, without overlapping, to fill a silhouette.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Units: a small triangle's leg is 1. The large triangles have legs 2, the medium one legs r2 (the square root of 2),
 * the square sides 1, the parallelogram sides 1 and r2: area 8 in all. A tan turns by eighths of a turn and the
 * parallelogram also turns over, so its corners lie at (a + b r2) / 4 for whole a, b: the figures are written so,
 * exactly (tangram_figures.h).
 *
 * The figures (levels) are our own, in six chapters: shapes, people, animals, boats and houses, things, letters and
 * numbers. Each is written as a placement of the seven tans (kind, eighths, turned over, its first corner); its
 * silhouette is their union. tests/tangram_test.c checks every one: the seven tans, no two overlapping, area 8, all
 * held together edge to edge, no hole.
 *
 * Solved: the tans cover the silhouette, whatever the arrangement: both are sampled TGM_RES times a unit, and the
 * samples where they differ (or two tans overlap) must stay under a small tolerance.
 *
 * A tan let go is drawn to a place that fits: one of its corners onto a corner of the silhouette's tans, of another
 * tan, or a point along an edge (at 1/2, 1, r2/2, r2 or 2 from its end), whichever lays the most corners together;
 * nothing near: a corner onto the grid of half units.
 *
 * The table (units): the silhouette's box at (fx, fy), the tray beside it (under it, or at its right for a wide room).
 * The notation (script, the screenshots, the tests): "h" a hint (a tan moved into its place); "=" hints until solved;
 * "r3" turns tan 3 an eighth (0..6: the large triangles, the medium, the small ones, the square, the
 * parallelogram); "f6" turns the parallelogram over; "m3:1.5,2" moves tan 3's middle to (1.5, 2) from the
 * silhouette's corner (then it is drawn to a fitting place); "n2" a new game of level 2 (from 1).
 */
#ifndef TANGRAM_H
#define TANGRAM_H

#include <stdint.h>

#define TGM_PIECES 7
#define TGM_CHAPTERS 6
#define TGM_LEVELS 98
#define TGM_HIST 1024
#define TGM_RES 16                              /* samples a unit, for the check */
#define TGM_MAXU 9                              /* a silhouette's box: units at most */

enum { TGM_L, TGM_M, TGM_S, TGM_Q, TGM_P };

typedef struct { double x, y; int rot, flip; } tgm_place;   /* its middle (units), eighths of a turn, turned over */

/* a figure: its chapter, then each tan: kind, eighths, turned over, its first corner (ax + bx r2) / 4, (ay + by r2) / 4 */
typedef struct { uint8_t chapter; int8_t t[TGM_PIECES][7]; } tgm_figure;
extern const tgm_figure tgm_figures[TGM_LEVELS];

typedef struct {
    int level;
    uint32_t seed;
    double fx, fy, fw, fh;                      /* the silhouette's box on the table */
    double tray_x, tray_y;                      /* the tray's corner */
    int table_w, table_h, tray_right;
    tgm_place slot[TGM_PIECES];                 /* the made solution's places (the hint's) */
    tgm_place p[TGM_PIECES];
    struct { int8_t piece; tgm_place was; } hist[TGM_HIST];
    int nh, moves, hints, solved;
} tgm_game;

int  tgm_type(int piece);                       /* TGM_L, TGM_L, TGM_M, TGM_S, TGM_S, TGM_Q, TGM_P */
int  tgm_chapter(int level);
/* a piece's corners on the table (units): how many (3 or 4) */
int  tgm_corners(int piece, const tgm_place *pl, double *xy);
/* a level's tans, its silhouette's corner at the origin; its box */
void tgm_solution(int level, tgm_place *out, double *w, double *h);
void tgm_new(tgm_game *g, int level, uint32_t seed);
void tgm_arrange(tgm_game *g, int tray_right); /* the tray under the silhouette (0) or at its right (1) */
int  tgm_inside(const tgm_game *g, double x, double y);   /* a point of the table in the silhouette */
int  tgm_snap(const tgm_game *g, int piece, tgm_place *pl);   /* drawn to a fitting place: 1 moved */
int  tgm_move(tgm_game *g, int piece, double x, double y);    /* its middle there (kept on the table), snapped */
int  tgm_turn(tgm_game *g, int piece);          /* an eighth of a turn, about its middle */
int  tgm_flip(tgm_game *g, int piece);          /* the parallelogram turned over; 0 for the others */
int  tgm_undo(tgm_game *g);                     /* the piece put back; -1: none */
int  tgm_in_place(const tgm_game *g, int piece);   /* it lies exactly on a place of the made solution */
int  tgm_check(const tgm_game *g);              /* the samples off (the silhouette against the tans) */
int  tgm_solved(const tgm_game *g);
int  tgm_hint(tgm_game *g);                     /* a piece moved into its place: which; -1 none */
int  tgm_stars(const tgm_game *g);              /* 3 without a hint, 2 with one or two, else 1 */
int  tgm_script(tgm_game *g, const char *moves);

#endif
