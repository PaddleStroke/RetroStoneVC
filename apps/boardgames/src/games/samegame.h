/*
 * Same Game for one player: the rules. A grid full of coloured blocks. Remove a group of two or more blocks of one
 * colour joined side by side: the blocks above fall into the gaps, and empty columns close up to the left. A group of
 * n blocks scores (n - 2)^2; clearing the whole board adds SM_BONUS. The game ends when no group is left.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 0 10 x 10 blocks in 3 colours, 1 15 x 10 in 4, 2 20 x 12 in 5. Every board can be cleared: it is made
 * backwards from the empty board, a group at a time put back where its removal would leave the board as it was (a
 * group of two or three, upright or across, never touching its own colour; a new column when it fills one), until the
 * grid is full; the way back is kept (the hint follows it while the board is still on it).
 * The levels: 150 of our own in six chapters, the board growing from 6 x 6 in 3 colours to 20 x 12 in 5, made by
 * tools/samegame_gen.c (samegame_levels.h: each board with its way to clear and its goal). The stars: three at the goal
 * (nine tenths of the best score found: two beam searches keeping the most promising boards at each removal, their
 * eye on the score and on what the colours left could still make, all of them or the most plentiful one kept for the
 * end; a greedy game; the way kept), two at half of it or with no more than a tenth of the tiles left, one otherwise. A random board gets
 * its goal the same way, from a narrower search made when it is dealt.
 * Places: x from the left, y from the bottom (0..). The notation (script, the screenshots, the tests): a removal is the
 * place tapped, its column a letter and its row a number from the bottom, "c1 a4"; "=k" plays the next k removals of
 * the way that clears the board (while the board is on it; "=": all of it); "~" takes the first group from the bottom
 * left, again and again until none is left (a careless game).
 */
#ifndef SAMEGAME_H
#define SAMEGAME_H

#include <stdint.h>

#define SM_VARIANTS 3
#define SM_W 20
#define SM_H 12
#define SM_HIST (SM_W * SM_H / 2 + 1)
#define SM_BONUS 1000
#define SM_LEVELS 150
#define SM_CHAPTERS 6

typedef struct { int8_t c[SM_W][SM_H]; } sm_board;   /* each column from the bottom; 0: empty */

typedef struct {
    int w, h, colours;
    sm_board b, start;
    int score, removed;                         /* removed: the groups taken */
    struct { sm_board b; int score; } hist[SM_HIST];
    int nh;
    /* the way that clears the board from the start: each removal's place, in turn */
    uint8_t way[SM_HIST][2];
    int nway;
    int goal;                                   /* the score for three stars (0: none) */
} sm_game;

void sm_new(sm_game *g, int variant, uint32_t seed);
void sm_make(sm_game *g, int w, int h, int colours, uint32_t seed);   /* any size, 2..7 colours */
/* the best final score a beam search of `width` boards finds from b (the bonus counted) */
int  sm_best(const sm_board *b, int w, int h, int width);
/* the best score known for g's start: the beam search's, a greedy game's (a beam of one), the kept way's */
int  sm_reference(const sm_game *g, int width);
int  sm_goal_of(int best);                      /* the goal from it: nine tenths */
/* 3 at the goal, 2 at half of it or with a tenth of the tiles left at most, else 1 (no goal: 3) */
int  sm_stars(const sm_game *g);
/* a board as text: its rows from the top ('|' between), each tile's colour 1..7; then a space and the way to clear
 * it, each removal a place as two letters (its column, its row from the bottom: "ab" is a2); 0: not a board */
int  sm_parse(sm_game *g, const char *text);
void sm_text(const sm_game *g, char *out);      /* the start and the way (SM_W * SM_H * 3 bytes at most) */
extern const char *const sm_levels[SM_LEVELS];
extern const uint16_t sm_level_goal[SM_LEVELS];
int  sm_level(sm_game *g, int level);           /* 0: none */
/* the group at (x, y): its places marked in mask (when not NULL, [x][y] 1), its size (0: no block there) */
int  sm_group(const sm_board *b, int w, int h, int x, int y, uint8_t mask[SM_W][SM_H]);
int  sm_points(int n);                          /* (n - 2)^2 */
int  sm_remove(sm_game *g, int x, int y);       /* the blocks removed (0: not a group of two or more) */
int  sm_undo(sm_game *g);
int  sm_over(const sm_game *g);                 /* no group left */
int  sm_cleared(const sm_game *g);
int  sm_blocks(const sm_game *g);
/* the hint: a group to take, (x, y) one of its places; 2: it keeps the board clearable (on the kept way, or a way
 * found from here); 1: no way to clear found, the group scoring most; 0: no group */
int  sm_hint(const sm_game *g, int *x, int *y);
/* a way to clear b from here, found by a search of at most `cap` boards: its first group's place; 1 found, 0 none
 * (or not within cap) */
int  sm_solve(const sm_board *b, int w, int h, int cap, int *x, int *y);
int  sm_script(sm_game *g, const char *moves);

#endif
