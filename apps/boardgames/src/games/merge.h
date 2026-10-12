/*
 * Merge for one player: the rules. Numbered tiles on a square grid. A push (up, right, down or left) slides every tile
 * as far as it goes that way; two tiles of the same number that meet merge into one of twice the number (a tile merges
 * once a push), and that number is added to the score. After every push that moved something, a new tile (2, or now
 * and then 4) appears on an empty square. Make a tile of the target number; then go on as long as you like. The game is
 * over when no push can move anything.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 0 4 x 4 (the target: power 11), 1 5 x 5 (power 11: more room, easier), 2 3 x 3 (256: hard). A tile is kept as
 * its power of two (1: 2, 2: 4, 3: 8 ...), 0 an empty square. Places: x + y * n from the top left.
 * Chance: the new tiles come from the game's own seeded numbers (puzrand.h), kept with each position: undo takes them
 * back too, so a push undone and made again brings the same tile (undo cannot fish for a better one). Undo goes back
 * all the way to the start (the oldest half forgotten past MRG_HIST pushes).
 * The notation (script, the screenshots, the tests): u r d l for the pushes; "=k" plays k pushes of the hint (a short
 * look ahead); "#e,e,e...": sets the grid, n * n powers row by row (0 empty), the score 0, no history.
 */
#ifndef MERGE_H
#define MERGE_H

#include <stdint.h>

#define MRG_VARIANTS 3
#define MRG_MAX 5
#define MRG_CELLS (MRG_MAX * MRG_MAX)
#define MRG_HIST 4096
#define MRG_TOP 20                              /* the largest power a tile can show */

enum { MRG_UP, MRG_RIGHT, MRG_DOWN, MRG_LEFT };

typedef struct {
    uint8_t c[MRG_CELLS];
    uint32_t score, rng;
} mrg_pos;

typedef struct {
    int n, variant, target;                     /* target: the power to make (11 on 4 x 4 and 5 x 5) */
    mrg_pos p;
    int moves;
    mrg_pos hist[MRG_HIST];
    int nh;
} mrg_game;

/* what a push did, for the animation: each tile's place before (to[place]: its place after, -1 no tile), the places
 * where two tiles merged (merged[place] 1), the new tile's place (-1: none) */
typedef struct {
    int8_t to[MRG_CELLS];
    uint8_t merged[MRG_CELLS];
    int spawn;
    int gain;
} mrg_push;

void mrg_variant(int variant, int *n, int *target);
/* a push on a grid alone: out (may be in), the points made; 0: nothing moved */
int  mrg_slide(const uint8_t *in, int n, int dir, uint8_t *out, int *gain, mrg_push *how);
void mrg_new(mrg_game *g, int variant, uint32_t seed);
int  mrg_can(const mrg_game *g, int dir);
int  mrg_any(const mrg_game *g);               /* 1: some push still moves */
int  mrg_move(mrg_game *g, int dir, mrg_push *how);   /* 0: nothing moved (no new tile) */
int  mrg_undo(mrg_game *g);
int  mrg_top(const mrg_game *g);               /* the largest power on the grid */
int  mrg_reached(const mrg_game *g);           /* the target is (or was) made */
int  mrg_just_reached(const mrg_game *g);      /* the last push made the target for the first time */
int  mrg_over(const mrg_game *g);              /* no push moves anything */
int  mrg_hint(const mrg_game *g);              /* a good push (a short look ahead); -1: none */
int  mrg_script(mrg_game *g, const char *moves);

#endif
