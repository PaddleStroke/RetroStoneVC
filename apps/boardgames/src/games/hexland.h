/*
 * Hex Landscape for one player: the rules. Hexagonal tiles are drawn from a stack, one at a time, and laid next to the
 * landscape already there, turned as the player likes. Each of a tile's six edges carries a terrain: meadow, forest,
 * field, village, river or rail. An edge that meets a neighbour of the same terrain scores; a tile whose six edges all
 * meet their six neighbours is perfect (more points, one more tile on the stack). Rivers and rails must go on: their
 * edges may only meet their own kind (or nothing yet). Some tiles carry a quest for the region of one of their
 * terrains: exactly n tiles (the region closed at n), or at least n. A quest done scores and puts tiles on the stack;
 * a region that grows past an exact n, or closes short, fails it. The run ends when the stack is empty, or a level's objectives are all met.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The map: axial coordinates (q, r) from the first tile, pointy-top hexes, in a square array HXL_N x HXL_N (a cell's
 * index c = (q + HXL_N / 2) + (r + HXL_N / 2) * HXL_N); a tile may lie up to HXL_REACH steps from the middle. Directions
 * 0..5: east, south-east, south-west, west, north-west, north-east (y grows down); edge d of a tile faces direction d.
 * A tile: six terrains, 3 bits each (edge d at bits 3d); turning it by k sixths clockwise moves edge d to d + k.
 * A tile's edges of the same terrain are one piece of that terrain (they join through the tile): a region is the
 * pieces of a terrain joined edge to edge, its size the tiles in it, open while one of its edges faces an empty cell.
 * Text: a tile is six letters from edge 0 (m meadow, f forest, w field, v village, r river, t rail), a quest after it
 * ("=f12": a forest of exactly 12; "+v8": a village of at least 8). A level: its first tiles ("<q><r><tile>" with q, r
 * as letters 'A' + 12 + q...), its stack and its reserve (the tiles the bonuses add, in order), space separated, and
 * its objectives and a score goal (random play: the score for three stars). Moves (script): "q,r,k" a tile laid at (q, r) turned k sixths; "h" the hint's
 * placement; "=" the hint's placements to the end.
 */
#ifndef HEXLAND_H
#define HEXLAND_H

#include <stdint.h>

#define HXL_N 48
#define HXL_CELLS (HXL_N * HXL_N)
#define HXL_MID (HXL_N / 2 + HXL_N / 2 * HXL_N)  /* the first tile's cell */
#define HXL_REACH 22
#define HXL_STACK 200                           /* the stack's entries, the bonuses too */
#define HXL_RES 80                              /* the reserve: the bonuses' tiles */
#define HXL_START 24                            /* a level's first tiles, at most */
#define HXL_QUESTS 32
#define HXL_LEVELS 100
#define HXL_CHAPTERS 5
#define HXL_VARIANTS 3
#define HXL_MOVES (HXL_CELLS * 6)

enum { HXL_NONE, HXL_MEADOW, HXL_FOREST, HXL_FIELD, HXL_VILLAGE, HXL_RIVER, HXL_RAIL, HXL_TERRAINS };
enum { HXL_EXACT, HXL_ATLEAST };                /* a quest's kind */
enum { HXL_OPEN, HXL_DONE, HXL_FAILED };        /* a quest's state */

#define HXL_MATCH 10                            /* points: an edge that meets its own terrain */
#define HXL_PERFECT 30                          /* more for a perfect tile (and HXL_PERFECT_TILES on the stack) */
#define HXL_PERFECT_TILES 1
#define HXL_QUEST 50                            /* a quest done (and HXL_QUEST_TILES on the stack) */
#define HXL_QUEST_TILES 3

static inline int hxl_edge(uint32_t t, int d) { return (int)(t >> (3 * d) & 7); }
static inline int hxl_strict(int terrain) { return terrain == HXL_RIVER || terrain == HXL_RAIL; }
static inline int hxl_opp(int d) { return (d + 3) % 6; }
uint32_t hxl_turn(uint32_t t, int k);           /* turned k sixths clockwise */
int  hxl_has(uint32_t t, int terrain);          /* how many edges of that terrain */

typedef struct {
    uint32_t tile;
    uint8_t qterr, qkind, qn;                   /* its quest (qn 0: none) */
} hxl_item;

typedef struct {
    int16_t cell;                               /* the tile that carried it */
    uint8_t terr, kind, n, state, size;         /* size: the region's size now */
} hxl_quest;

/* a level's objectives (the first the main one): a region of a terrain of at least n tiles (REGION), a region closed
 * at exactly n (EXACT), n perfect tiles, n quest flags done, a score of n. Once met, met for good. Text: "f6" a forest
 * of 6, "=v5" a village of exactly 5, "p3" three perfect tiles, "q2" two quests, "s400" a score of 400. */
enum { HXL_OB_REGION, HXL_OB_EXACT, HXL_OB_PERFECT, HXL_OB_QUESTS, HXL_OB_SCORE };
#define HXL_OBJS 3
typedef struct { uint8_t kind, terr; uint16_t n; } hxl_obj;

typedef struct {
    uint32_t cell[HXL_CELLS];                   /* the tiles laid (0: none) */
    hxl_item stack[HXL_STACK];
    int ns, top;                                /* the stack's entries; the next to draw */
    hxl_item res[HXL_RES];
    int nr, rtop;                               /* the reserve; the next it gives */
    hxl_quest q[HXL_QUESTS];
    int nq;
    int score, placed, perfects, done, failed, skipped, matches;
    int last;                                   /* the last cell laid (-1) */
    hxl_obj obj[HXL_OBJS];
    int nobj;
    unsigned met;                               /* bit i: objective i met */
} hxl_state;

/* what a placement did (for the screen): its points, the edges that met, perfect, tiles added, quests done or failed
 * (their indexes), forests closed (a cell of each), tiles set aside that fitted nowhere */
typedef struct {
    int points, matches, perfect, bonus;
    int ndone, nfail, nforest, skipped;
    int done[8], fail[8], forest[8];
    unsigned obj_met;                           /* objectives met by this placement */
} hxl_result;

typedef struct {
    hxl_state s, prev;
    int can_undo, goal, start_tiles;
} hxl_game;

/* the map */
int  hxl_cell(int q, int r);                    /* -1: off the map */
void hxl_qr(int c, int *q, int *r);
int  hxl_nb(int c, int d);                      /* the neighbour (-1 off the map) */
int  hxl_reach(int c);                          /* steps from the middle */
int  hxl_frontier(const hxl_state *s, int c);   /* 1: empty, within reach, next to a tile */
/* a tile (already turned) at c: 1 if it may lie there; its points and edges met (perfect: all six) without quests */
int  hxl_can(const hxl_state *s, int c, uint32_t tile);
int  hxl_gain(const hxl_state *s, int c, uint32_t tile, int *matches, int *perfect);
/* the region of terrain t through cell c: its size (0: none), *open 1 while it can still grow (may be NULL) */
int  hxl_region(const hxl_state *s, int c, int t, int *open);
const hxl_item *hxl_current(const hxl_state *s);   /* the tile to lay (NULL: the stack is empty) */
int  hxl_left(const hxl_state *s);              /* tiles left on the stack, the current one too */
int  hxl_over(const hxl_state *s);
/* the current tile laid at c turned k: 1 done (the result filled in, may be NULL), 0 refused. Tiles that fit nowhere
 * after it are set aside at once. */
int  hxl_place(hxl_state *s, int c, int k, hxl_result *res);
/* every placement of the current tile: moves m = c * 6 + k into out (HXL_MOVES); how many */
int  hxl_moves(const hxl_state *s, int *out);
int  hxl_fits(const hxl_state *s, uint32_t tile);   /* 1: it fits somewhere */
/* the hint: the placement that looks best (its points now and what it leaves the next tile); 0: none */
int  hxl_hint(const hxl_state *s, int *c, int *k);
/* the greedy player (the best points at each tile, then the hint's tie-breaks): its final score */
int  hxl_greedy(const hxl_state *s);
int  hxl_stars(int score, int goal);            /* 3 at the goal, 2 at four fifths of it, else 1 */
int  hxl_goal2(int goal);

/* the game: undo is the last placement only */
int  hxl_play(hxl_game *g, int c, int k, hxl_result *res);
int  hxl_undo(hxl_game *g);
int  hxl_script(hxl_game *g, const char *moves);

/* text */
uint32_t hxl_tile_parse(const char *s);         /* six letters; 0 bad */
void hxl_tile_text(uint32_t t, char *out);      /* seven bytes */
int  hxl_items_parse(const char *s, hxl_item *out, int cap);   /* -1 bad */
typedef struct { const char *start, *stack, *reserve, *goals; uint16_t goal; } hxl_level_t;
int  hxl_objs_parse(const char *s, hxl_obj *out, int cap);   /* -1 bad */
void hxl_objs_text(const hxl_obj *o, int n, char *out, int cap);
/* objective i: how far it has come (0..n), and a cell of the region that counts (-1 none) */
int  hxl_obj_progress(const hxl_state *s, int i, int *cell);
/* the largest region of terrain t: its size, a cell of it (may be NULL) */
int  hxl_largest(const hxl_state *s, int t, int *cell);
int  hxl_lost(const hxl_state *s);              /* a level over without its main objective */
/* a level's stars: all objectives 3, the main one and half of them 2, the main one 1, else 0; without objectives
 * (at random) by the score against the goal (hxl_stars) */
int  hxl_level_stars(const hxl_state *s, int goal);
int  hxl_load(hxl_game *g, const hxl_level_t *lv);   /* 0: bad */

/* the puzzles' maker: a landscape painted on a disc of hexes (blobs of soft terrains, rivers and rails wandering
 * through it), cut into its tiles; the first ones laid as `start` says, the others shuffled and turned at random onto
 * the stack, some swapped for strays of another landscape; quests drawn from its regions; a reserve from a third */
typedef struct {
    int radius;                                 /* the landscape: a disc of this many rings round its middle */
    unsigned terrains;                          /* bit t: soft terrain t may be used */
    int rivers, rails;                          /* how many wander through it */
    int start;                                  /* the first tiles: 0 the middle, 1 a line of 3, 2 the middle and its
                                                 * ring, 3 the ring alone (a hole), 4 a triangle, 5 a line of 5 */
    int strays;                                 /* stack tiles swapped for others, percent */
    int quests;                                 /* quests on the stack */
    int exact;                                  /* percent of them exact */
    int reserve;                                /* the reserve's tiles */
    int river_quests;                           /* 1: a river may carry one */
} hxl_spec;
void hxl_make(hxl_game *g, const hxl_spec *sp, uint32_t seed);
void hxl_variant(int variant, hxl_spec *sp);
void hxl_random(hxl_game *g, int variant, uint32_t seed);
extern const hxl_level_t hxl_levels[HXL_LEVELS];
int  hxl_level(hxl_game *g, int level);         /* 0: none */
/* a level's text from a game as made (the levels' tool): start, stack, reserve into buffers of cap bytes */
void hxl_text(const hxl_game *g, char *start, char *stack, char *reserve, int cap);

#endif
