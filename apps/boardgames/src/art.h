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

/* ---- figures: shapes in a unit square (y down), painted back to front -----------------------------------------------
 * Each PART shape: its fill (lit from the upper left, as round as its bounding box) and its outline, which becomes an
 * inner line where it crosses a shape painted before it; a MARK shape (an eye, a slit) is painted in the mark
 * colour. */
typedef struct art_fig art_fig;
typedef struct {
    float fill[3], outer[3], inner[3], mark[3];
    float light;                                    /* how much the light shapes the fill (0: flat) */
    float line;                                     /* the outline's width, unit square (0: none) */
} art_style;
enum { ART_PART, ART_MARK };

art_fig *art_fig_new(void);
void art_fig_free(art_fig *f);
void art_poly(art_fig *f, int role, const float *xy, int n);            /* n points: x, y, x, y... */
void art_ellipse(art_fig *f, int role, float cx, float cy, float rx, float ry);
void art_rect(art_fig *f, int role, float x0, float y0, float x1, float y1);
void art_arc(art_fig *f, int role, float cx, float cy, float r0, float r1, float deg0, float deg1);  /* a band */
/* w x h pixels; scale: pixels per unit; the unit square's (0, 0) at pixel (ox, oy) */
void art_render(const art_fig *f, const art_style *st, int w, int h, float scale, float ox, float oy, uint8_t *rgba);

/* ---- the house style: every game uses these, for one look across the app --------------------------------------------
 * Two players everywhere: 0 ivory, 1 ebony (chess's white and black, go's stones, the discs...). */
extern const art_style art_player[2];

enum { COUNTER_DISC, COUNTER_STONE, COUNTER_MAN, COUNTER_KING };
/* a round piece of a player, seen from above, size x size: DISC a flat counter with a rim (reversi, four in a row,
 * morris), STONE a glossy go stone, MAN a draughts man (rings), KING a man with a crown */
void art_counter(int kind, int player, int size, uint8_t *rgba);

/* the shadow of a flat piece lying on the wood (discs, stones, men): as wide as the piece, its edge soft; the shell's
 * ui_shadow_flat() draws it, a game's thumbnail can blend it */
void art_flat_shadow(int size, uint8_t *rgba);

/* a die showing 1..6, a player's colour (ivory with ebony pips, or ebony with ivory pips), seen from above,
 * size x size */
void art_die(int value, int player, int size, uint8_t *rgba);

/* ---- ready-made pictures ----------------------------------------------------------------------------------------- */
/* a chess piece (games/chess.h PAWN..KING, WHITE / BLACK), size x size */
void art_piece(int type, int color, int size, uint8_t *rgba);

/* the wooden board: 8 x 8 squares of sq pixels in a frame of fr pixels; (8 * sq + 2 * fr) squared */
void art_board(int sq, int fr, uint8_t *rgba);
/* the same wood, n x m squares (light / dark: checkered; plain: one wood, no squares) */
void art_board_n(int nx, int ny, int sq, int fr, int checkered, uint8_t *rgba);

enum { ICON_UNDO, ICON_FLIP, ICON_NEW, ICON_HOME, ICON_F2F, ICON_PASS, ICON_SOUND, ICON_MUTE, ICON_HELP, ICON_CLOSE,
       ICONS };
/* a round button's icon (white: tint it), size x size */
void art_icon(int which, int size, uint8_t *rgba);

/* a rounded rectangle filled with one colour, its edge smoothed */
void art_roundrect(int w, int h, float radius, const uint8_t rgba_fill[4], uint8_t *rgba);

/* a disc fading from alpha a0 at its centre to 0 at its rim (power: how fast), or a hard disc (power 0) */
void art_disc(int size, const uint8_t rgb[3], float a0, float power, uint8_t *rgba);

/* a ring: inner radius r_in (0..1 of the radius), smoothed edges */
void art_ring(int size, float r_in, const uint8_t rgb[3], float a, uint8_t *rgba);

/* paints src (sw x sh) over dst (dw x dh) at (x, y): alpha blending, clipped */
void art_blend(uint8_t *dst, int dw, int dh, const uint8_t *src, int sw, int sh, int x, int y);

#endif
