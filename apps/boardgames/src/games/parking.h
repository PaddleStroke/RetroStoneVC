/*
 * Traffic Jam for one player: the rules. A parking lot of 6 x 6 bays, cars (two bays long) and trucks (three) parked
 * in it, each one along a row or a column. A vehicle only drives forwards or backwards, along its own row or column,
 * into empty bays; it never turns. Free the red car: drive it out through the gap in the lot's right side, at the end
 * of its row (the third from the top).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A move: one vehicle driven as far as you like (a vehicle driven again at once stays one move; driven back where it
 * was, no move). The par: the fewest moves, found by a breadth-first search over the positions. Solved: the red car at
 * the right end of its row, by the gap.
 * The puzzles: vehicles parked by chance (the red car in its row, then cars and trucks until the lot is full enough),
 * then every position they can reach is found (their family) with the fewest moves out of each, searched back from the
 * solved ones; the start is a position of the family at the distance wanted. The levels' tool (tools/parking_gen.c)
 * keeps the farthest positions of many families; a random puzzle (variants 0 easy: 5 to 9 moves, 1 medium: 10 to 16,
 * 2 hard: 17 or more) tries some lots of its own, then falls back on a level's family.
 * The levels: 150 of our own in six chapters (parking_levels.h: the lots and each one's par, checked by the tests).
 * A lot as text: its 36 bays row by row from the top left, '.' empty, 'A' the red car, 'B'.. the others (each one's
 * bays in one line). Bays: x 0..5 from the left, y 0..5 from the top. The notation (script, the screenshots, the
 * tests): a vehicle named by a bay it covers, its column a..f and its row 1..6 from the top, then its drive, one letter
 * a bay (u d l r): "c3dd" drives the vehicle on c3 two bays down; "h" a hint (the first move of a shortest way,
 * played), "=" the whole shortest way.
 */
#ifndef PARKING_H
#define PARKING_H

#include <stdint.h>

#define PKJ_N 6
#define PKJ_ROW 2                               /* the red car's row: the gap at its right end */
#define PKJ_CARS 16
#define PKJ_HIST 512
#define PKJ_LEVELS 150
#define PKJ_CHAPTERS 6
#define PKJ_VARIANTS 3
#define PKJ_FAMILY 400000                       /* the most positions a family may have */

typedef struct { int8_t x, y, len, vert; } pkj_car;   /* its top left bay; 2 or 3 long; along a column */

typedef struct {
    int nc;                                     /* the vehicles: 0 the red car */
    pkj_car c[PKJ_CARS];
    struct { int8_t car, from; } hist[PKJ_HIST];   /* each move: the vehicle, where it was (along its line) */
    int nh;
    int last;                                   /* the vehicle moved last (driven again: the same move); -1 */
    int moves;
    int par;                                    /* the fewest moves from the start (0: not known) */
} pkj_game;

int  pkj_parse(pkj_game *g, const char *lot);   /* 0: not a lot */
void pkj_text(const pkj_game *g, char *out);    /* 37 bytes */
int  pkj_at(const pkj_game *g, int x, int y);   /* the vehicle on a bay; -1: empty */
int  pkj_pos(const pkj_car *c);                 /* where it is along its line (x, or y when vert) */
/* the places a vehicle can be driven to in one move, along its line: lo..hi (its own place among them) */
void pkj_range(const pkj_game *g, int car, int *lo, int *hi);
int  pkj_drive(pkj_game *g, int car, int pos);  /* to pos along its line in one move; 0: cannot */
int  pkj_undo(pkj_game *g);                     /* the last move taken back: the vehicle; -1: none */
int  pkj_solved(const pkj_game *g);
/* the fewest moves left (0 solved, -1 never), the first one's vehicle and place (may be NULL) */
int  pkj_solve(const pkj_game *g, int *car, int *pos);
/* a shortest way: each move's vehicle and place (up to max); its length (0 solved, -1 none) */
int  pkj_way(const pkj_game *g, int8_t *car, int8_t *pos, int max);
/* how many vehicles a shortest way moves (the red car counted) */
int  pkj_movers(const pkj_game *g);
/* every position of g's family and its fewest moves out (-1: none): a callback for each; the family's size; 0: more
 * than cap positions (or no memory) */
int  pkj_family(const pkj_game *g, int cap, void (*each)(const pkj_game *pos, int dist, void *ctx), void *ctx);
/* vehicles parked by chance (the red car in its row, short of the gap; then `count` others when they fit) */
void pkj_fill(pkj_game *g, int count, uint32_t *rng);
int  pkj_script(pkj_game *g, const char *moves);
extern const char *const pkj_levels[PKJ_LEVELS];
extern const uint8_t pkj_level_par[PKJ_LEVELS];
int  pkj_level(pkj_game *g, int level);         /* 0: none */
/* a puzzle at random of the variant (its par found); 0: none found (never seen) */
int  pkj_random(pkj_game *g, int variant, uint32_t seed);

#endif
