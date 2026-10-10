/*
 * The board games' pictures, drawn by code at the size they are shown (sharp on any screen, no image files):
 * a tiny anti-aliased rasteriser of shapes (polygons, ellipses) with outlines and soft lighting.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Every picture is RGBA, 4 bytes a pixel, straight (not premultiplied) alpha, rows top to bottom.
 */
#ifndef ART_H
#define ART_H

#include <stdint.h>

/* a chess piece (chess.h PAWN..KING, WHITE / BLACK), size x size */
void art_piece(int type, int color, int size, uint8_t *rgba);

/* the wooden board: 8 x 8 squares of sq pixels in a frame of fr pixels; (8 * sq + 2 * fr) squared */
void art_board(int sq, int fr, uint8_t *rgba);

enum { ICON_UNDO, ICON_FLIP, ICON_NEW, ICONS };
/* a round button's icon (cream), size x size */
void art_icon(int which, int size, uint8_t *rgba);

/* a rounded rectangle filled with one colour, its edge smoothed */
void art_roundrect(int w, int h, float radius, const uint8_t rgba_fill[4], uint8_t *rgba);

/* a disc fading from alpha a0 at its centre to 0 at its rim (power: how fast), or a hard disc (power 0) */
void art_disc(int size, const uint8_t rgb[3], float a0, float power, uint8_t *rgba);

/* a ring: inner radius r_in (0..1 of the radius), smoothed edges */
void art_ring(int size, float r_in, const uint8_t rgb[3], float a, uint8_t *rgba);

#endif
