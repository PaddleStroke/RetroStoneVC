/*
 * Lights for one player: the rules. A square of lamps, some lit. A press on a lamp turns it and its neighbours above,
 * below, left and right over (lit to dark, dark to lit). Turn every lamp off.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 3 x 3, 5 x 5, 7 x 7. A board is made from presses drawn by chance on dark lamps, so it can always be
 * turned off (by the same presses again); the fewest presses that do it are found over GF(2): a press is its own
 * opposite and the order of presses does not matter, so a board is a sum of presses, solved by Gaussian elimination;
 * when the presses are not independent (5 x 5: four ways), every way is tried for the fewest. A board is kept when
 * its fewest presses lie in its level's range (3 x 3: 4..6; 5 x 5: 9..15; 7 x 7: 16..30), so the grade is the work.
 * The levels: 150 boards of our own in six chapters, from 3 x 3 to 7 x 7 lamps, made by tools/lights_gen.c
 * (lights_levels.h: the lamps lit, and each one's fewest presses, checked by the tests).
 * Lamps: x + y * n from the top left. The notation (script, the screenshots, the tests): a lamp is its column letter
 * a.. and its row 1.. from the top, "c3 b2"; "h" a hint (a press of the fewest way, played); "=" every press of the
 * fewest way (solved).
 */
#ifndef LIGHTS_H
#define LIGHTS_H

#include <stdint.h>

#define LMP_VARIANTS 3
#define LMP_MAX 7
#define LMP_CELLS (LMP_MAX * LMP_MAX)
#define LMP_HIST 2048
#define LMP_LEVELS 150
#define LMP_CHAPTERS 6

typedef struct {
    int n;
    uint8_t on[LMP_CELLS];                      /* 1: lit */
    int16_t hist[LMP_HIST];                     /* the lamps pressed */
    int nh;
    int moves;                                  /* the presses */
    int best;                                   /* the fewest presses from the start */
} lmp_game;

int  lmp_size(int variant);                     /* 3, 5, 7 */
/* the fewest presses that turn the lamps off (press: 1 for each lamp to press; may be NULL); -1: none */
int  lmp_solve(int n, const uint8_t *on, uint8_t *press);
void lmp_make(int variant, uint32_t seed, uint8_t *on);
void lmp_new(lmp_game *g, int variant, uint32_t seed);
void lmp_load(lmp_game *g, int n, const uint8_t *on);
void lmp_toggle(int n, uint8_t *on, int cell);  /* a press on any board */
int  lmp_press(lmp_game *g, int cell);          /* 0: not a lamp */
int  lmp_undo(lmp_game *g);                     /* the lamp pressed again; -1: none */
int  lmp_hint(const lmp_game *g);               /* a lamp of the fewest way now; -1: all off */
int  lmp_solved(const lmp_game *g);
int  lmp_lit(const lmp_game *g);                /* how many are lit */
int  lmp_script(lmp_game *g, const char *moves);
/* a board from rows of lamps, '1' lit, '0' dark, '|' between rows ("010|111|010"); 0: not a square of 2..7 */
int  lmp_parse(lmp_game *g, const char *rows);
/* the designed levels (0..LMP_LEVELS-1): the board, its fewest presses; 0: none */
extern const char *const lmp_levels[LMP_LEVELS];
extern const uint8_t lmp_level_par[LMP_LEVELS];
int  lmp_level(lmp_game *g, int level);

#endif
