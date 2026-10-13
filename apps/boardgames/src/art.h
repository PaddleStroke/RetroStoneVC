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
/* the figure rendered w x w (its unit square) and blended over a picture (pw x ph) at (x, y), faded (alpha 0..1) */
void art_fig_blend(uint8_t *rgba, int pw, int ph, const art_fig *f, const art_style *st, int x, int y, int w,
                   float alpha);
/* a stroke: a band from (x0, y0) to (x1, y1), wd wide, its ends square */
void art_band(art_fig *f, int role, float x0, float y0, float x1, float y1, float wd);
/* an arrow head at (x, y) pointing at deg (0 right, 90 down), l long */
void art_head(art_fig *f, int role, float x, float y, float deg, float l);
/* a five-pointed star centred on (cx, cy), its points r from the centre, one pointing up */
void art_star(art_fig *f, int role, float cx, float cy, float r);
/* a flat style: one fill colour, its outline `line` wide (of the unit square) in ink (marks in ink too), lit by
 * `light` (0: flat) */
art_style art_flat(const uint8_t fill[3], const uint8_t ink[3], float line, float light);
/* a figure painted flat (art_flat; ink NULL: the fill's) over a picture size x size (its unit square), then freed:
 * a picture drawn colour by colour */
void art_fig_paint(uint8_t *rgba, int size, art_fig *f, const uint8_t fill[3], const uint8_t ink[3], float line,
                   float light);

/* ---- the house style: every game uses these, for one look across the app --------------------------------------------
 * Two players in most games: 0 ivory, 1 ebony (chess's white and black, go's stones, the discs...). The games for
 * more players (ludo, halma...) go on with 2 red lacquer, 3 jade, 4 amber, 5 lapis: a player's pieces, counters and
 * dice all take these colours. */
#define ART_PLAYERS 6
extern const art_style art_player[ART_PLAYERS];
int art_player_dark(int player);                    /* 1: a dark colour (light marks on it: pips, eyes) */
/* the house ink (lines, seams, marks painted on the wood) and its pale ink (marks on a dark inlay) */
static const uint8_t ART_INK[3] = {84, 56, 36}, ART_PALE[3] = {244, 226, 192};
#define ART_PI 3.14159265f
/* a steady pseudo-random 0..1 for (a, b): a thrown lot's place and tilt, the same every time it is drawn */
float art_hash(int a, int b);

enum { COUNTER_DISC, COUNTER_STONE, COUNTER_MAN, COUNTER_KING };
/* a round piece of a player, seen from above, size x size: DISC a flat counter with a rim (reversi, four in a row,
 * morris), STONE a glossy go stone, MAN a draughts man (rings), KING a man with a crown */
void art_counter(int kind, int player, int size, uint8_t *rgba);

/* the shadow of a flat piece lying on the wood (discs, stones, men): as wide as the piece, its edge soft; the shell's
 * ui_shadow_flat() draws it, a game's thumbnail can blend it */
void art_flat_shadow(int size, uint8_t *rgba);
/* a flat piece's picture (d x d) blended into a picture (w x h, a thumbnail) centred on (cx, cy), its shadow under it
 * towards the lower right, as on the board */
void art_flat_put(uint8_t *rgba, int w, int h, const uint8_t *piece, int d, float cx, float cy);

/* a die showing 1..6, a player's colour (ivory with ebony pips, or ebony with ivory pips), seen from above,
 * size x size */
void art_die(int value, int player, int size, uint8_t *rgba);
/* the same painted into a picture (w x h) with its flat shadow, centred on (cx, cy), turned by deg (a thumbnail's
 * dice, a button's picture) */
void art_die_blend(uint8_t *rgba, int w, int h, float cx, float cy, int size, int player, int value, float deg);

/* ---- ready-made pictures ----------------------------------------------------------------------------------------- */
/* a chess piece (games/chess.h PAWN..KING, WHITE / BLACK), size x size */
void art_piece(int type, int color, int size, uint8_t *rgba);
/* the same figure in any style (the court cards' figures in their suit's colour), size x size */
void art_piece_style(int type, const art_style *st, int size, uint8_t *rgba);

/* the wooden board: 8 x 8 squares of sq pixels in a frame of fr pixels; (8 * sq + 2 * fr) squared */
void art_board(int sq, int fr, uint8_t *rgba);
/* the same wood, n x m squares (light / dark: checkered; plain: one wood, no squares) */
void art_board_n(int nx, int ny, int sq, int fr, int checkered, uint8_t *rgba);
/* one plain plank, any shape: w x h pixels of wood inside a walnut frame of fr pixels ((w + 2 fr) x (h + 2 fr) in
 * all), its grain at the scale of a square plank of its short side (art_board_n's plain wood sets it by the square:
 * finer grain): a tray under a grid, a field stretched over the whole room */
void art_plank(int w, int h, int fr, uint8_t *rgba);
/* an inlaid tile in a picture (pw x ph) of wood: the wood under a rounded rectangle (x0, y0, w x h, corners rad;
 * pixels) tinted by tone (a factor per channel, so the grain stays), a darker seam round it, seam pixels wide: the
 * squares of the race games, a scorecard, a groove */
void art_inlay(uint8_t *rgba, int pw, int ph, float x0, float y0, float w, float h, float rad, const float tone[3],
               float seam);

/* the green felt of the dice games over a rectangle [x0, x1) x [y0, y1) of a picture (w x h): fine fibres, darker
 * towards its edges (under the walls) */
void art_felt(uint8_t *rgba, int w, int h, int x0, int y0, int x1, int y1);
/* a rectangle (pixels, any fractions) tinted rgb at alpha a over a picture (w x h), the pixels it covers in part
 * tinted in part, clipped: a line ruled on a board, a well's dark edges */
void art_tint_rect(uint8_t *rgba, int w, int h, float x0, float y0, float x1, float y1, const uint8_t rgb[3], float a);
/* a dice cup upside down, seen from the side, size x size, a band of the player's colour round it near its mouth:
 * ART_CUP_LEATHER stitched brown leather (liar's dice), ART_CUP_WALNUT turned walnut, a flared lip, two brass rings
 * (mexico) */
enum { ART_CUP_LEATHER, ART_CUP_WALNUT };
void art_cup(int kind, int player, int size, uint8_t *rgba);
/* a steady noise for a pixel, -0.5..0.5 (fibres, grain) */
float art_noise(int x, int y);
/* a round hole carved in the wood (its radius r and bevelled rim b, pixels), lit from the upper left: the factor for
 * the wood's colour (dx, dy) pixels from its centre (1: untouched). Its floor `deep` dark, its far wall lit by
 * wall_lit, its rim by rim_lit (marbles' recesses 0.4, 0.24, 0.22; pegs' holes 0.42, 0.24, 0.2) */
float art_recess(float dx, float dy, float r, float b, float deep, float wall_lit, float rim_lit);

/* ICON_DEVICE: pass the device (a device and two arrows going round it: the hand-off cover); ICON_PLAY: start;
 * ICON_HINT: a lamp, lit (a hint); ICON_TICK: done, ready, check; ICON_ARROW: an arrow pointing right (next, on);
 * ICON_PUZZLE: a jigsaw piece (the menu's puzzles); ICON_GRID: nine tiles (the levels); ICON_STAR: a star;
 * ICON_REPLAY: an arrow going round (the same puzzle again); ICON_PAR: a flag on its pole (the fewest moves);
 * ICON_EYE: an eye (look, show); ICON_PENCIL: a pencil (write notes); ICON_ERASER: an eraser (rub out);
 * ICON_HOURGLASS: an hourglass (turns left before a draw); ICON_MENU: three bars (the menu's settings) */
enum { ICON_UNDO, ICON_FLIP, ICON_NEW, ICON_HOME, ICON_F2F, ICON_PASS, ICON_SOUND, ICON_MUTE, ICON_HELP, ICON_CLOSE,
       ICON_DEVICE, ICON_PLAY, ICON_HINT, ICON_TICK, ICON_ARROW, ICON_PUZZLE, ICON_GRID, ICON_STAR, ICON_REPLAY, ICON_PAR,
       ICON_EYE, ICON_PENCIL, ICON_ERASER, ICON_HOURGLASS, ICON_MENU, ICONS };
/* a round button's icon (white: tint it), size x size */
void art_icon(int which, int size, uint8_t *rgba);

/* a rounded rectangle filled with one colour, its edge smoothed */
void art_roundrect(int w, int h, float radius, const uint8_t rgba_fill[4], uint8_t *rgba);
/* the same, rw x rh, rgb at alpha a, painted over a picture (w x h) at (x, y): a square of a board, a rail */
void art_roundrect_put(uint8_t *rgba, int w, int h, int x, int y, int rw, int rh, float radius, const uint8_t rgb[3],
                       float a);

/* a disc fading from alpha a0 at its centre to 0 at its rim (power: how fast), or a hard disc (power 0) */
void art_disc(int size, const uint8_t rgb[3], float a0, float power, uint8_t *rgba);

/* a ring: inner radius r_in (0..1 of the radius), smoothed edges */
void art_ring(int size, float r_in, const uint8_t rgb[3], float a, uint8_t *rgba);

/* paints src (sw x sh) over dst (dw x dh) at (x, y): alpha blending, clipped */
void art_blend(uint8_t *dst, int dw, int dh, const uint8_t *src, int sw, int sh, int x, int y);
/* the same centred on (cx, cy) and turned by deg: bilinear */
void art_blend_rot(uint8_t *dst, int dw, int dh, const uint8_t *src, int sw, int sh, float cx, float cy, float deg);

#endif
