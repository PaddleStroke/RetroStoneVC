/*
 * Jigsaw's rules (no SDL; tests/jigsaw_test.c links them): a picture cut into a grid of pieces with tabs and blanks,
 * each inner edge its own curve (Bézier segments, a knob of random shape and side, made from the puzzle's seed). The
 * pieces wait in a tray, then lie on the table; dropped near their place they lock into it, near a neighbour lying
 * the right way round they join it, and joined pieces move (and turn) together as one group.
 *
 * The table's units are the picture's cells: the picture covers [0, cols] x [0, rows], a piece's place is its cell's
 * centre (i + 0.5, j + 0.5). A piece's position is its cell's centre wherever it lies; rot its quarter turns
 * clockwise (0: upright, the only way it fits). The screen's tray and its scrolling are the game's, not the rules'.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef JIGSAW_H
#define JIGSAW_H

#include <stdint.h>

#define JIG_MAX 160                             /* pieces at most (cols x rows) */
#define JIG_EDGES (2 * JIG_MAX)                 /* inner edges at most */
#define JIG_EDGE_PTS 48                         /* points on one knobbed edge (its end left out) */
#define JIG_OUTLINE (4 * JIG_EDGE_PTS)          /* points on a piece's outline at most */
#define JIG_HIST 400                            /* undo's steps kept */
#define JIG_COUNTS 5                            /* the piece counts asked for (the variants' sizes) */
#define JIG_VARIANTS (2 * JIG_COUNTS)           /* a count x upright or turned */
#define JIG_TOL 0.3f                            /* how near (cells) a piece must come to snap */
#define JIG_MARGIN 0.3f                         /* how far a knob reaches out of its cell (cells) */
extern const int jig_counts[JIG_COUNTS];        /* 12, 24, 48, 96, 150 */

enum { JIG_TRAY, JIG_TABLE, JIG_PLACED };       /* where a piece is */
enum { JIG_SNAP = 1, JIG_PLACE = 2, JIG_DONE = 4 };   /* what a drop did */

/* one inner edge: its knob's side (+1: it bulges towards +x or +y, -1 the other way) and shape: the knob's centre
 * along the edge (c), its neck's half width (n), its bulb's radius (r) and its centre's height (h), its tilt (t), the
 * edge's shoulders' waves (w1, w2) */
typedef struct { float c, n, r, h, t, w1, w2; int8_t sign; } jig_knob;

typedef struct {
    float x, y;                                 /* its cell's centre on the table */
    int8_t rot, where;                          /* quarter turns clockwise; JIG_* */
    int16_t group;                              /* the pieces joined share it (the lowest piece's index) */
} jig_piece;

typedef struct {                                /* what a move changes (undo keeps a copy) */
    jig_piece p[JIG_MAX];
    int16_t tray[JIG_MAX];                      /* the tray's pieces, in its order */
    int16_t z[JIG_MAX];                         /* the table's pieces, bottom to top */
    int16_t ntray, nz, placed, moves;
} jig_pos;

typedef struct {
    int pic, count, rotate;                     /* the picture (jigsaw_pictures.h), jig_counts[count], turned pieces */
    int cols, rows, n;
    uint32_t seed;
    jig_knob hk[JIG_EDGES];                     /* the edge on line y = j (1..rows-1) over column i: [(j-1)*cols+i] */
    jig_knob vk[JIG_EDGES];                     /* the edge on line x = i (1..cols-1) beside row j: [j*(cols-1)+i-1] */
    jig_pos s;
    jig_pos hist[JIG_HIST];
    int nh, hints;
} jig_game;

/* the pictures (jigsaw_pictures.h): the tarot's 1898 trumps and courts (cards.h's cards_tarot_art), then the
 * paintings (art/jigsaw/<file>.jpg, embedded by tools/gen_art.py as jigsaw_art[]) */
enum { JS_TRUMPS, JS_COURTS, JS_JAPAN, JS_IMPRESSION, JS_POST, JS_MASTERS, JS_ROMANTIC, JS_SETS };
typedef struct {
    int16_t tarot;                              /* its cards_tarot_art index, or -1: a painting */
    const char *file;                           /* a painting's file in art/jigsaw (no .jpg) */
    int16_t w, h;                               /* its size, pixels */
    uint8_t set;                                /* JS_* */
    int16_t died;                               /* its artist's death (0: the tarot, 1898) */
} jig_picture;
const jig_picture *jig_pic(int i);
int  jig_pictures(void);                        /* how many */
int  jig_levels(void);
int  jig_chapters(void);
int  jig_chapter_of(int level);
int  jig_level_pic(int level);
int  jig_level_count(int level);                /* its jig_counts index */
int  jig_level_rotate(int level);
int  jig_chapter_first(int chapter);            /* its first level */

/* the grid for a picture w x h about `pieces` pieces: cells as square as can be, the count near */
void jig_grid(int w, int h, int pieces, int *cols, int *rows);
/* a new puzzle: picture pic, jig_counts[count] pieces, turned pieces or not; the knobs and the tray's order from
 * seed */
void jig_new(jig_game *g, int pic, int count, int rotate, uint32_t seed);
void jig_load(jig_game *g, int level);          /* a designed level */
void jig_random(jig_game *g, int variant, uint32_t seed);   /* variant: count * 2 + rotate; the picture at random */

/* piece k's outline (its place in the picture, cells), clockwise, as polygon points x, y...: the count */
int  jig_outline(const jig_game *g, int k, float *xy);
/* one knobbed edge from (0, 0) to (1, 0), out towards +v: JIG_EDGE_PTS points u, v (the end (1, 0) left out) */
void jig_edge(const jig_knob *k, float *uv);
int  jig_is_edge(const jig_game *g, int k);     /* on the picture's border */
int  jig_solved(const jig_game *g);
/* piece k's place: its cell's centre */
void jig_home(const jig_game *g, int k, float *x, float *y);

/* the moves (each undoable): piece k (its whole group) dropped with k's centre at (x, y): from the tray it comes out
 * alone; then it snaps to its place (upright and near) and to its neighbours (the same turn, near); JIG_* flags */
int  jig_drop(jig_game *g, int k, float x, float y);
int  jig_to_tray(jig_game *g, int k, int at);   /* a lone piece back into the tray at index at; 0: not allowed */
int  jig_rotate(jig_game *g, int k);            /* its group a quarter turn clockwise about k (turned puzzles) */
int  jig_hint_pick(const jig_game *g);          /* the piece a hint places (-1: none): one by the pieces placed */
int  jig_hint(jig_game *g);                     /* places it and its group (counted); the piece, or -1 */
int  jig_undo(jig_game *g);
/* the stars: the logic puzzles' rule (3: none, 2: two at most, else 1) on the hints and the time: past the target
 * (8 s a piece, 12 turned) one, past twice it three */
int  jig_slow(const jig_game *g, int seconds);
int  jig_stars(const jig_game *g, int seconds);
int  jig_target_s(const jig_game *g);
/* moves in the notation: p<k> its place (turned upright first), m<k>@x,y dropped there (cells), t<k>@i back in the
 * tray at i, r<k> turned, h a hint, u undo, E every border piece placed, N<n> the first n pieces (row by row) placed */
int  jig_script(jig_game *g, const char *moves);

#endif
