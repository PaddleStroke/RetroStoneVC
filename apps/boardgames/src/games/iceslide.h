/*
 * Ice Slide for one player: the rules. A puck on a sheet of ice ruled in squares, rocks here and there, and a hole.
 * Push the puck up, down, left or right: it slides until a rock or the edge of the ice stops it. If it passes over the
 * hole on the way, it drops in: the puzzle is solved. Get it into the hole in as few pushes as possible.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 0 easy (7 x 6 squares, a shortest way of 4 to 6 pushes), 1 medium (8 x 7, 7 to 10), 2 hard (10 x 9,
 * 11 to 16). The par: the fewest pushes from the start (a search over every place the puck can rest).
 * The generator: the par sought drawn first, any in the variant's range alike; then rocks laid by chance, a start drawn
 * among the empty squares, and for every other empty square as the hole a search from the start: a hole whose shortest
 * way is that par is kept (drawn among them), else the rocks are laid again. A layout with too few places to rest (a
 * puzzle with no choices) is drawn again. The slowest puzzle takes about 10 ms on a PC.
 * The levels: 150 boards of our own in six chapters, the ice growing from 5 x 5 to 10 x 9 and the par from 2 to 18
 * pushes, made by tools/iceslide_gen.c (iceslide_levels.h: the rows, and each one's par checked by the tests).
 * Places: x + y * w from the top left. The notation (script, the screenshots, the tests): u r d l for the pushes,
 * "=" plays the rest of a shortest way from where the puck is, "=k" its next k pushes.
 */
#ifndef ICESLIDE_H
#define ICESLIDE_H

#include <stdint.h>

#define ICS_VARIANTS 3
#define ICS_MAXW 10
#define ICS_MAXH 9
#define ICS_CELLS (ICS_MAXW * ICS_MAXH)
#define ICS_HIST 1024
#define ICS_LONGEST 64                          /* the longest way a search reports */
#define ICS_LEVELS 150
#define ICS_CHAPTERS 6

enum { ICS_UP, ICS_RIGHT, ICS_DOWN, ICS_LEFT };

typedef struct {
    int w, h;
    uint8_t rock[ICS_CELLS];
    int goal;
} ics_board;

typedef struct {
    ics_board b;
    int variant;
    int start, puck;                            /* puck: ICS's place, or the goal once it dropped in */
    int par;                                    /* the fewest pushes from the start */
    int8_t hist[ICS_HIST];                      /* the puck's place before each push */
    int nh;
    int moves;
} ics_game;

/* the variants' sizes and ranges */
void ics_variant(int variant, int *w, int *h, int *lo, int *hi, int *rocks);
/* where a push from `from` the way `dir` ends (the goal when it passes over it); how many squares it slid in *len
 * (0: it cannot move that way) */
int  ics_slide(const ics_board *b, int from, int dir, int *len);
/* the fewest pushes from `from` into the hole (-1: it cannot get there); the pushes in way[] (up to ICS_LONGEST) */
int  ics_solve(const ics_board *b, int from, int8_t *way);
/* the places the puck can rest at, reached from `from` (the hole not counted): how many */
int  ics_reach(const ics_board *b, int from);
void ics_new(ics_game *g, int variant, uint32_t seed);
/* a board from rows of text ('#' a rock, 'P' the puck, 'O' the hole, '.' ice; '|' between rows may be left out) */
int  ics_load(ics_game *g, int w, int h, const char *rows);
int  ics_parse(ics_game *g, const char *rows);  /* the same, its size read from the rows ("..#|P.O") */
/* the designed levels (0..ICS_LEVELS-1): the board, its par; 0: none */
extern const char *const ics_levels[ICS_LEVELS];
extern const uint8_t ics_level_par[ICS_LEVELS];
int  ics_level(ics_game *g, int level);
int  ics_can(const ics_game *g, int dir);
int  ics_move(ics_game *g, int dir);           /* the squares slid; 0: not allowed (solved, or blocked) */
int  ics_undo(ics_game *g);                    /* 0: nothing to undo */
int  ics_solved(const ics_game *g);
int  ics_hint(const ics_game *g);              /* the next push of a shortest way; -1: the hole cannot be reached
                                                * from here (undo first) */
int  ics_script(ics_game *g, const char *moves);

#endif
