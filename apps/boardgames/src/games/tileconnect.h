/*
 * Tile Connect for one player: the rules. Mahjong tiles lie in a rectangle of cells. Two identical tiles are taken
 * away together when a line can join them in at most three straight strokes (two turns) through empty cells; the line
 * may also run round the outside of the rectangle (one cell beyond its edge). Take every tile away.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Cells: x + y * w from the top left; the line's points in "outside" coordinates, -1..w and -1..h. Some levels have
 * blocked cells (a wooden block: no tile, no line). Some let the tiles fall after each pair is taken (TCN_DOWN: down
 * their column; TCN_LEFT: to the left of their row; TCN_CENTRE: each half of a row towards the middle); a blocked
 * cell holds the tiles above it (a fall stops at a block).
 * Every board can be cleared without a shuffle: it is made by playing it out first. The rectangle full of tiles not
 * yet painted, two tiles that a line can join are taken away at a time (by chance), the tiles falling as they would in
 * play, until it is empty; then each pair taken gets a face, the same for both of its tiles. The way the line runs
 * depends only on where tiles are, not on their faces, so taking the pairs in that order clears the board (the
 * "plan": each tile's step in it, kept by the tile's identity, which falls with it).
 * Faces come in pairs; a face may come two or three times (four or six tiles), and then which two go together
 * matters: a wrong choice can leave tiles that cannot be joined. The shuffle deals the faces left again into the same
 * cells, made the same way, so the board can be cleared again (it costs stars). The hint shows a pair on a way to
 * clear the board (the plan while it is followed, else the solver's), or the shuffle when no way is left.
 * The notation (script, the screenshots, the tests): a cell is its column letter a.. and its row 1.. from the top;
 * "c2-f2" takes those two tiles; "h" takes the hint's pair (a hint used); "s" shuffles; "=n" takes the next n pairs of
 * a way that clears the board ("=*" all of them); "+n" the first pair found, n times.
 */
#ifndef TILECONNECT_H
#define TILECONNECT_H

#include <stdint.h>

#define TCN_MAXW 12
#define TCN_MAXH 8
#define TCN_CELLS (TCN_MAXW * TCN_MAXH)
#define TCN_EMPTY 255
#define TCN_BLOCK 254
#define TCN_FACES 42                            /* games/mahjong.h's MJ_FACES: the faces of the house's tiles */
#define TCN_HIST 128
#define TCN_VARIANTS 4                          /* random play: 6 x 4, 8 x 6, 10 x 7, 12 x 8 */
#define TCN_LEVELS 150
#define TCN_CHAPTERS 6

enum { TCN_STILL, TCN_DOWN, TCN_LEFT, TCN_CENTRE, TCN_FALLS };   /* where the tiles fall after each pair */

typedef struct { int8_t x, y; } tcn_pt;

typedef struct {
    int w, h, fall;
    uint8_t face[TCN_CELLS];                    /* a face 0..TCN_FACES-1, TCN_EMPTY or TCN_BLOCK */
    uint8_t id[TCN_CELLS];                      /* the tile's identity (it falls with it); TCN_EMPTY: none */
} tcn_board;

typedef struct {
    uint8_t face[TCN_CELLS], id[TCN_CELLS], step[TCN_CELLS];
    int plan;                                   /* the plan's next step while it is followed; -1: left */
} tcn_snap;

typedef struct {
    tcn_board b;
    uint8_t step[TCN_CELLS];                    /* by identity: the plan's step that takes the tile */
    int plan;                                   /* the next step while the plan is followed, -1 once left */
    int tiles, pairs;                           /* the tiles left; the pairs at the start */
    int hints, shuffles;                        /* used (undo does not take them back) */
    uint32_t rng;                               /* the shuffles' chance */
    tcn_snap hist[TCN_HIST];
    int nh;
} tcn_game;

/* a level of the designed set: its size, its fall, a character a cell for the faces ('a'.. 'z', 'A'..'P'; '#'
 * blocked) and another for the plan's step that takes the tile ('0'..'9', 'A'..'Z', 'a'..'z'; '#') */
typedef struct { uint8_t w, h, fall; const char *faces, *order; } tcn_lvl;
extern const tcn_lvl tcn_levels[TCN_LEVELS];

void tcn_size(int variant, int *w, int *h);
/* a line joining cells a and b (tiles or not) through empty cells, at most two turns, the shortest: its points (a,
 * the turns, b; 2..4 of them) in path (may be NULL); 0: none */
int  tcn_connect(const tcn_board *b, int a, int b2, tcn_pt *path, int *np);
int  tcn_can_take(const tcn_board *b, int a, int b2);   /* two tiles of one face that a line joins */
/* the tiles of b fall (b->fall); from[cell] (may be NULL): the cell each tile came from (TCN_EMPTY: no tile) */
void tcn_fall(tcn_board *b, uint8_t *from);
int  tcn_find_pair(const tcn_board *b, int *a, int *b2);   /* the first pair that can be taken; 0: none */
int  tcn_count_pairs(const tcn_board *b);
/* the board cleared by a search (budget: positions): 1 it can be (*a, *b2 the first pair of the way found; may be
 * NULL), 0 it cannot, -1 not known within the budget; *nodes the positions tried (may be NULL) */
int  tcn_solve(const tcn_board *b, long budget, int *a, int *b2, long *nodes);
/* where the tiles are now, played out by chance (seed): each identity's step (by identity); 0: it got stuck */
int  tcn_plan(const tcn_board *b, uint32_t seed, uint8_t *step);
/* a board made: w x h, the fall, blocked cells (blocks[cell] 1; may be NULL), faces: how many different faces (at
 * least a third of the pairs, so a face comes at most three times); 0: it failed */
int  tcn_make(tcn_game *g, int w, int h, int fall, const uint8_t *blocks, int faces, uint32_t seed);
void tcn_new(tcn_game *g, int variant, uint32_t seed);   /* a random puzzle of the variant (no fall, no blocks) */
int  tcn_level(tcn_game *g, int level);         /* 0: no such level */
int  tcn_load(tcn_game *g, int w, int h, int fall, const char *faces, const char *order);   /* 0: bad */
int  tcn_take(tcn_game *g, int a, int b2, uint8_t *from);   /* 0: not a pair that can be taken; from: tcn_fall's */
enum { TCN_HINT_NONE, TCN_HINT_PAIR, TCN_HINT_SHUFFLE };
int  tcn_hint(const tcn_game *g, int *a, int *b2);   /* TCN_HINT_*: a pair to take, or the shuffle */
int  tcn_shuffle(tcn_game *g);                  /* the faces left dealt again so that they clear; 0: cannot */
int  tcn_undo(tcn_game *g);                     /* a pair or a shuffle taken back; 0: nothing to undo */
int  tcn_won(const tcn_game *g);
int  tcn_stuck(const tcn_game *g);              /* tiles left, and no pair that can be taken */
int  tcn_script(tcn_game *g, const char *moves);   /* 0 at the first bad move */
int  tcn_cell(const char *s, int w, int h, int *n);   /* "c2": the cell (n: characters read); -1: none */

#endif
