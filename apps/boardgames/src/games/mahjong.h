/*
 * Mahjong Solitaire for one player: the rules. The 144 tiles of a Mahjong set lie stacked in a layout; the player
 * takes away matching pairs of free tiles until none is left. A tile is free when no tile lies on it, and its left
 * or its right side is open (no tile right beside it on its own layer). Two tiles match when they are the same; any
 * flower matches any flower, any season any season.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The places of a layout are on a grid of half tiles: a tile at (x, y, z) covers x..x+2 and y..y+2 on layer z
 * (0 the table). Tile A lies on tile B when A's layer is B's plus one and they overlap; B's left neighbour is a tile
 * of its layer at x - 2 overlapping its rows (y within 1), its right one at x + 2.
 * The layouts: 0 the Turtle (144 tiles: the classic five layers), 1 the Pyramid (80 tiles: 8 x 6, 6 x 4 and 4 x 2,
 * each layer a tile in from the one under it all round; a smaller set: 40 of the 72 pairs of the full set, by
 * chance).
 * The deal is made backwards, so that it can always be cleared: the layout full, two free places are taken away at
 * a time (by chance) until it is empty, and the pairs of tiles go into those places in that order. Played the other
 * way round, that order clears the deal (sol).
 * No pair left to take: if the tiles left could be cleared in some order (tried by chance), the player may shuffle
 * them (a new deal of those tiles in their places, made the same way, so it can be cleared again; undo takes it
 * back); if not (a tile stuck under another, say), the game is over.
 * The notation (script, the screenshots, the tests): "12-40" takes the tiles in places 12 and 40 (the layout's
 * order), "s" shuffles, "=n" plays the next n pairs of the deal's own solution, "+n" the first free pair found, n
 * times ("+*": as long as there is one).
 */
#ifndef MAHJONG_H
#define MAHJONG_H

#include <stdint.h>

#define MJ_VARIANTS 2
#define MJ_MAX 144
#define MJ_NONE 255
#define MJ_HIST 192                             /* the moves undo can take back (a pair, or a shuffle) */

/* the faces: the three suits 1..9 (dots, bamboo, characters), the winds (east, south, west, north), the dragons
 * (red, green, white), the flowers 1..4, the seasons 1..4 */
enum { MJ_DOTS = 0, MJ_BAMBOO = 9, MJ_CHARS = 18, MJ_WINDS = 27, MJ_DRAGONS = 31, MJ_FLOWERS = 34, MJ_SEASONS = 38,
       MJ_FACES = 42 };
enum { MJ_PLAY, MJ_WON, MJ_STUCK, MJ_LOST };    /* STUCK: no pair, but a shuffle can save it; LOST: it cannot */

typedef struct { int8_t x, y, z; } mjpos;

typedef struct {
    int variant, n;                             /* the layout's places */
    mjpos pos[MJ_MAX];
    int16_t above[MJ_MAX][4], left[MJ_MAX][2], right[MJ_MAX][2];   /* the places on it, beside it (-1: none) */
    uint8_t face[MJ_MAX];                       /* the tile in each place; MJ_NONE: taken away */
    int tiles;                                  /* the tiles left */
    uint8_t sol[MJ_MAX];                        /* the deal's order: the pairs sol[2k], sol[2k + 1] clear it */
    int nsol;
    int dead;                                   /* no pair, and no order clears the tiles left */
    uint8_t hist[MJ_HIST][MJ_MAX];              /* the faces before each move */
    int nh;
} mjgame;

int  mj_layout(int variant, mjpos *pos);        /* a layout's places; how many */
int  mj_match(int a, int b);                    /* 1: these faces make a pair */
/* a new deal of the layout, from a source of chance (rnd(n): 0..n-1; the shell's ui_random): 0 if it failed */
int  mj_deal(mjgame *g, int variant, int (*rnd)(int));
int  mj_free(const mjgame *g, int i);           /* 1: a tile there, free */
int  mj_can_take(const mjgame *g, int a, int b);
int  mj_take(mjgame *g, int a, int b);          /* 0: not a free matching pair */
int  mj_hint(const mjgame *g, int *a, int *b);  /* a free matching pair (the first found); 0: none */
int  mj_pairs(const mjgame *g);                 /* how many free matching pairs */
int  mj_shuffle(mjgame *g, int (*rnd)(int));    /* the tiles left dealt again into their places; 0: cannot */
int  mj_undo(mjgame *g);                        /* 0: nothing to undo */
int  mj_status(const mjgame *g);                /* MJ_* */
int  mj_script(mjgame *g, const char *moves, int (*rnd)(int));   /* 0 at the first bad move */

#endif
