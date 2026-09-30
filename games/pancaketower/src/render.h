/*
 * Pancake Tower: the run-time pancake renderer (render.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
 */
#ifndef PT_RENDER_H
#define PT_RENDER_H

#include "pt.h"

#define RING_ROWS 32                /* BG2's map rows (a ring: world tile row k is map row (-(k + 1)) & 31) */

void render_init(void);
/* layer l on BG2 map row ring_row, its centre px = the plate's centre in map pixels, over map columns
 * [col0, col0 + ncols) (a player's half in 2 players) */
void render_row(int player, int ring_row, const layer *l, int centre_px, int col0, int ncols);
void render_clear_row(int ring_row, int col0, int ncols);
/* a layer into a 128x8 box of OBJ tiles (two 64x8 sprites) at `tile` (relative to VR_OBJ); box_x0 = the map x of
 * the box's left column, x0 = the layer's left in the same coordinates; *pal: 0 tower colours, 1 toppings */
int  render_layer_sprite(int tile, const layer *l, int box_x0, int x0, int squash, int *pal);
/* a falling piece rotated by a (64 steps per turn) into a 128x16 box (two 64x16 sprites) */
void render_piece(int tile, const cut_piece *pc, int a);
int  render_piece_fits(int w, int a);
int  pancake_px(int ax, int y, int x0, int w, int n, int flags, int sprite, int cut, int centre_tc);
int  topping_px(int kind, int ax, int y, int x0, int w);

#endif
