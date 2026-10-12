/*
 * Box Pusher for one player: the rules. A storehouse of walls, crates and marked places, as many places as crates.
 * The keeper walks one cell at a time up, down, left or right, and pushes a crate in front of it if the cell beyond
 * is free: crates are never pulled, never pushed two at a time. Bring every crate onto a place.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The levels are our own (boxpusher_levels.h): rooms drawn by chance, their crates set on the places, then the start
 * found by pulling them back (a search over every position reached by pulls, the farthest one kept), each checked by
 * the push-optimal solver here; then chosen and graded by the pushes they need and how often the crate pushed
 * changes (tools/boxpusher_gen.c made them). Three chapters of 20, the easiest first.
 * Cells: x + y * w from the top left. An action (a step, a walk to a cell, a push, a walk and pushes) is undone as a
 * whole. The counts: moves (every step the keeper takes, pushes too) and pushes.
 * The level's rows: '#' a wall, ' ' floor, '.' a place, '$' a crate, '*' a crate on a place, '@' the keeper, '+' the
 * keeper on a place, '_' outside; rows split by '|'.
 * The notation (script, the screenshots, the tests): steps u d l r (a push when a crate is ahead), "h" a hint (the
 * keeper walks to the next push of a shortest way and makes it), "=" every push of a shortest way (solved).
 */
#ifndef BOXPUSHER_H
#define BOXPUSHER_H

#include <stdint.h>

#define BP_MAXW 12
#define BP_MAXH 12
#define BP_CELLS (BP_MAXW * BP_MAXH)
#define BP_BOXES 6
#define BP_HIST 8192
#define BP_LEVELS 60
#define BP_CHAPTERS 3
#define BP_PER_CHAPTER 20

enum { BP_FLOOR, BP_WALL, BP_OUT };
enum { BP_UP, BP_DOWN, BP_LEFT, BP_RIGHT };

typedef struct {
    int level, w, h;
    uint8_t map[BP_CELLS];                      /* BP_FLOOR, BP_WALL, BP_OUT */
    uint8_t goal[BP_CELLS], box[BP_CELLS];
    int nbox, player;
    uint8_t hist[BP_HIST];                      /* each step: its way | 4 a push | 8 an action's first step */
    int nh;
    int moves, pushes;
} bp_game;

extern const char *const bp_levels[BP_LEVELS];
extern const int16_t bp_par[BP_LEVELS];         /* each level's fewest pushes */

int  bp_load(bp_game *g, const char *rows);     /* 0: not a level */
void bp_new(bp_game *g, int level);             /* a level 0..59 */
int  bp_dx(int dir);
int  bp_dy(int dir);
int  bp_free(const bp_game *g, int cell);       /* floor without a crate */
int  bp_can(const bp_game *g, int dir);         /* the keeper can step (or push) that way */
void bp_begin(bp_game *g);                      /* the next step begins an action */
int  bp_step(bp_game *g, int dir);              /* 0: cannot; 1 a step; 2 a push */
/* the keeper's shortest walk to a cell, around the crates: its ways (dirs); how many; -1 none */
int  bp_path(const bp_game *g, int to, uint8_t *dirs, int max);
int  bp_walk(bp_game *g, int to);               /* walked there as one action; 0: no way */
int  bp_undo(bp_game *g);                       /* the last action taken back; 0: none */
int  bp_solved(const bp_game *g);
/* the fewest pushes left (a search over pushes, dead cells cut away), the first push: its crate's cell and way (may
 * be NULL); -1: no way (a crate stuck), -2: too many positions to search */
int  bp_solve(const bp_game *g, int *box, int *dir);
int  bp_hint(bp_game *g);                       /* the hint played (a walk and a push, one action): its crate's new
                                                 * cell; -1: none */
int  bp_script(bp_game *g, const char *moves);

#endif
