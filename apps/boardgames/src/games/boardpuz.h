/*
 * What the three chessboard puzzles share on the screen (Chess Puzzles, the Knight's Tour, Eight Queens): the house
 * board of nx x ny squares in its walnut frame (squares cut away: holes, dark recesses in the wood), the house chess
 * pieces (the board turned a quarter when that makes its squares bigger), the queens' lines (a tool's picture: the
 * attacked squares shaded), the bar's small pictures and numbers, and the win's gold light.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="queens" builds
 * queens.c and queens_*.c), so a shared .c file would be left out of one game's build or another's.
 * Squares: file (0 = a, the left from the first player's side) and rank (0 = the bottom row); a square's index (holes)
 * rank * nx + file; flip turns the board round (Chess Puzzles: black at the bottom).
 */
#ifndef BOARDPUZ_H
#define BOARDPUZ_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "games/chess.h"
#include "games/puzzles.h"
#include "shell.h"

#define BPZ_FRAME 0.32f                         /* the frame, in squares */

static const uint8_t BPZ_GOLD[3] = {232, 190, 92}, BPZ_RED[3] = {196, 64, 48};

typedef struct {
    int nx, ny, sq, fr, turned, made_sq, made_nx, made_ny, made_turned, made_piece;
    uint32_t made_holes;                        /* the holes painted (a hash: 0 none) */
    float x, y;                                 /* the top left square's corner on the screen */
    SDL_Texture *board, *piece[2][7];
} bpz_board;

/* a hole: a square cut out of the board, a recess in the wood (its floor dark, its upper and left walls in shade, its
 * lower and right walls lit), sq x sq at (x0, y0) of a picture w wide */
static inline void bpz_carve(uint8_t *o, int w, int x0, int y0, int sq)
{
    static const float FLOOR[3] = {70, 48, 32}, SHADE[3] = {36, 25, 17}, LIT[3] = {132, 96, 64};
    float b = fmaxf(1.5f, sq * 0.11f);
    for (int y = 0; y < sq; y++)
        for (int x = 0; x < sq; x++) {
            float dl = x + 0.5f, dt = y + 0.5f, dr = sq - dl, db = sq - dt;
            float near = fminf(dl, dt), far = fminf(dr, db), m = fminf(near, far);
            float soft = 0.78f + 0.22f * clampf((near - b) / (2 * b), 0, 1);   /* the floor's shadow by the walls */
            const float *wall = near <= far ? SHADE : LIT;
            float t = clampf(b - m + 0.5f, 0, 1);
            uint8_t *p = o + 4 * ((y0 + y) * w + x0 + x);
            for (int i = 0; i < 3; i++) {       /* one dark wood whatever the square's, a little of its grain kept */
                float c = FLOOR[i] * soft + (wall[i] - FLOOR[i] * soft) * t;
                p[i] = (uint8_t)clampf(c * (0.9f + 0.2f * p[i] / 255.0f), 0, 255);
            }
        }
}

static inline uint32_t bpz_holes_hash(int cells, const uint8_t *holes)
{
    uint32_t h = 2166136261u;
    for (int i = 0; holes && i < cells; i++) h = (h ^ (holes[i] ? 1u + (uint32_t)i : 0u)) * 16777619u;
    return h | 1;
}

/* where a square (file, rank) of an nx x ny board stands on the screen: its column and row from the top left; turned:
 * the board a quarter turn clockwise (its ranks across the screen, rank 0 at the left; file 0 at the top) */
static inline void bpz_screen(int nx, int ny, int turned, int file, int rank, int *col, int *row)
{
    (void)nx;
    if (turned) { *col = rank; *row = file; }
    else { *col = file; *row = ny - 1 - rank; }
}

/* the board picture: nx x ny squares of sq pixels in a frame of fr (turned: as on the screen, ny wide), the holes
 * (rank * nx + file; NULL: none) carved */
static inline void bpz_paint_board_turned(int nx, int ny, int turned, int sq, int fr, const uint8_t *holes, uint8_t *o)
{
    int cols = turned ? ny : nx, rows = turned ? nx : ny, w = cols * sq + 2 * fr;
    art_board_n(cols, rows, sq, fr, 1, o);
    for (int r = 0; holes && r < ny; r++)
        for (int f = 0; f < nx; f++) {
            int c, y;
            if (!holes[r * nx + f]) continue;
            bpz_screen(nx, ny, turned, f, r, &c, &y);
            bpz_carve(o, w, fr + c * sq, fr + y * sq, sq);
        }
}

static inline void bpz_paint_board(int nx, int ny, int sq, int fr, const uint8_t *holes, uint8_t *o)
{
    bpz_paint_board_turned(nx, ny, 0, sq, fr, holes, o);
}

/* the board of nx x ny squares as big as it fits in the area (the shell's whole room), centred, turned a quarter
 * when that makes the squares bigger (ui_grid_fit); its pictures (and the pieces' when asked) */
static inline void bpz_layout_holes(bpz_board *b, int nx, int ny, box area, int pieces, const uint8_t *holes)
{
    ui_grid g = ui_grid_fit(area, nx, ny, BPZ_FRAME);
    b->nx = nx;
    b->ny = ny;
    b->turned = g.turned;
    b->sq = (int)floorf(g.cell);
    if (b->sq < 2) b->sq = 2;
    b->fr = (int)(b->sq * BPZ_FRAME);
    int cols = b->turned ? ny : nx, rows = b->turned ? nx : ny;
    int w = cols * b->sq + 2 * b->fr, h = rows * b->sq + 2 * b->fr;
    uint32_t hh = holes ? bpz_holes_hash(nx * ny, holes) : 0;
    b->x = floorf(area.x + (area.w - w) / 2) + b->fr;
    b->y = floorf(area.y + (area.h - h) / 2) + b->fr;
    if (b->made_sq != b->sq || b->made_nx != nx || b->made_ny != ny || b->made_holes != hh ||
        b->made_turned != b->turned || !b->board) {
        b->made_sq = b->sq;
        b->made_nx = nx;
        b->made_ny = ny;
        b->made_holes = hh;
        b->made_turned = b->turned;
        ui_free(&b->board);
        bpz_paint_board_turned(nx, ny, b->turned, b->sq, b->fr, holes, ui_buf(w, h));
        b->board = ui_tex(w, h, ui_buf(w, h));
    }
    if (pieces && b->made_piece != b->sq) {
        b->made_piece = b->sq;
        for (int c = 0; c < 2; c++)
            for (int t = PAWN; t <= KING; t++) {
                ui_free(&b->piece[c][t]);
                art_piece(t, c, b->sq, ui_buf(b->sq, b->sq));
                b->piece[c][t] = ui_tex(b->sq, b->sq, ui_buf(b->sq, b->sq));
            }
    }
}

static inline void bpz_layout(bpz_board *b, int n, box area, int pieces)
{
    bpz_layout_holes(b, n, n, area, pieces, NULL);
}

static inline void bpz_draw_board(const bpz_board *b)
{
    int cols = b->turned ? b->ny : b->nx, rows = b->turned ? b->nx : b->ny;
    ui_put(b->board, b->x - b->fr, b->y - b->fr, (float)(cols * b->sq + 2 * b->fr), (float)(rows * b->sq + 2 * b->fr));
}

/* a square's top left corner */
static inline void bpz_xy(const bpz_board *b, int file, int rank, int flip, float *x, float *y)
{
    if (flip) { file = b->nx - 1 - file; rank = b->ny - 1 - rank; }
    int c, r;
    bpz_screen(b->nx, b->ny, b->turned, file, rank, &c, &r);
    *x = b->x + c * b->sq;
    *y = b->y + r * b->sq;
}

/* a square's centre */
static inline void bpz_centre(const bpz_board *b, int file, int rank, int flip, float *x, float *y)
{
    bpz_xy(b, file, rank, flip, x, y);
    *x += b->sq / 2.0f;
    *y += b->sq / 2.0f;
}

/* the square under (x, y): 0 when off the board */
static inline int bpz_at(const bpz_board *b, float x, float y, int flip, int *file, int *rank)
{
    int c = (int)floorf((x - b->x) / b->sq), r = (int)floorf((y - b->y) / b->sq), f, k;
    if (b->turned) { f = r; k = c; }
    else { f = c; k = b->ny - 1 - r; }
    if (f < 0 || f >= b->nx || k < 0 || k >= b->ny) return 0;
    *file = flip ? b->nx - 1 - f : f;
    *rank = flip ? b->ny - 1 - k : k;
    return 1;
}

/* 1: a light square (the top left one is light, as the house board's; on 8 x 8, a1 dark); ny: the board's rows */
static inline int bpz_light(int ny, int file, int rank) { return !((file + ny - 1 - rank) & 1); }

/* the same on the board as it stands on the screen (turned or not) */
static inline int bpz_light_on(const bpz_board *b, int file, int rank)
{
    int c, r;
    bpz_screen(b->nx, b->ny, b->turned, file, rank, &c, &r);
    return !((c + r) & 1);
}

/* a standing piece at (x, y), a square's size (scaled by k), with its shadow; faded by alpha (0..255) */
static inline void bpz_piece_at(const bpz_board *b, int type, int color, float x, float y, float k, Uint8 alpha)
{
    SDL_Texture *t = b->piece[color][type];
    if (!t) return;
    float s = b->sq * k;
    x += (b->sq - s) / 2;
    y += (b->sq - s) / 2;
    if (alpha == 255) ui_shadow(x, y, s);
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put(t, x, y, s, s);
    SDL_SetTextureAlphaMod(t, 255);
}

/* a square tinted (the attacked squares: a shade) */
static inline void bpz_tint(const bpz_board *b, int file, int rank, int flip, Uint8 r, Uint8 g, Uint8 bl, Uint8 a)
{
    float x, y;
    bpz_xy(b, file, rank, flip, &x, &y);
    ui_fill(x, y, (float)b->sq, (float)b->sq, r, g, bl, a);
}

/* ---- pictures drawn by code: a figure in a unit square, painted flat in one colour ------------------------------- */
static inline void bpz_paint(uint8_t *o, int size, art_fig *f, const uint8_t rgb[3])
{
    art_fig_paint(o, size, f, rgb, NULL, 0, 0);
}

/* a stroke from (x0, y0) to (x1, y1), wd wide */
static inline void bpz_stroke(art_fig *f, float x0, float y0, float x1, float y1, float wd)
{
    float dx = x1 - x0, dy = y1 - y0, l = hypotf(dx, dy) + 1e-6f, nx = -dy / l * wd / 2, ny = dx / l * wd / 2;
    const float q[] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    art_poly(f, ART_PART, q, 4);
}

enum { BPZ_LINES, BPZ_FLAME, BPZ_TICK, BPZ_LOOP, BPZ_SLASH, BPZ_PICS };

/* a picture (size x size, cleared first): the queen's lines (eight rays: the attacked squares, white: a button's
 * picture, tinted by the shell), the flame (a streak), the tick (solved), the loop (a closed tour), the slash (between
 * two numbers). Its signature is the shell's ui_painter. */
static inline void bpz_picture(int which, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    switch (which) {
    case BPZ_LINES:
        for (int i = 0; i < 8; i++) {
            float a = i * PUZ_PI / 4, c = cosf(a), s = sinf(a), l = i % 2 ? 0.33f : 0.38f;
            bpz_stroke(f, 0.5f + c * 0.12f, 0.5f + s * 0.12f, 0.5f + c * l, 0.5f + s * l, 0.06f);
        }
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.08f, 0.08f);
        bpz_paint(o, size, f, PUZ_WHITE);
        return;
    case BPZ_FLAME: {
        float pts[2 * 24];
        for (int i = 0; i < 24; i++) {          /* a drop, its point up, a little bent */
            float t = i / 24.0f * 2 * PUZ_PI, r = 0.3f, x = sinf(t), y = cosf(t);
            float tip = y < 0 ? -y : 0;
            pts[2 * i] = 0.5f + r * x * (1 - 0.75f * tip * tip) + 0.06f * tip * tip;
            pts[2 * i + 1] = 0.6f + r * y * (y < 0 ? 1.75f : 0.8f);
        }
        art_poly(f, ART_PART, pts, 24);
        bpz_paint(o, size, f, BPZ_GOLD);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.68f, 0.12f, 0.14f);
        bpz_paint(o, size, f, BPZ_RED);
        return;
    }
    case BPZ_TICK: {
        static const float tick[] = {0.18f, 0.52f, 0.29f, 0.41f, 0.43f, 0.55f, 0.72f, 0.24f, 0.83f, 0.35f,
                                     0.43f, 0.76f};
        art_poly(f, ART_PART, tick, 6);
        bpz_paint(o, size, f, PUZ_CREAM);
        return;
    }
    case BPZ_LOOP:
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.26f, 0.38f, 0, 360);
        bpz_paint(o, size, f, BPZ_GOLD);
        return;
    default:
        bpz_stroke(f, 0.36f, 0.86f, 0.64f, 0.14f, 0.1f);
        bpz_paint(o, size, f, PUZ_CREAM);
        return;
    }
}

/* the pictures as textures, remade when the size changes */
typedef struct { int size; SDL_Texture *t[BPZ_PICS]; } bpz_pics;

static inline SDL_Texture *bpz_pic(bpz_pics *p, int which, int size)
{
    if (p->size != size) {
        for (int i = 0; i < BPZ_PICS; i++) ui_free(&p->t[i]);
        p->size = size;
    }
    if (!p->t[which]) {
        bpz_picture(which, size, ui_buf(size, size));
        p->t[which] = ui_tex(size, size, ui_buf(size, size));
    }
    return p->t[which];
}

/* ---- the bar ----------------------------------------------------------------------------------------------------- */
/* a number's width in the bar */
static inline float bpz_number_w(int value)
{
    int digits = 1;
    for (int v = value; v >= 10; v /= 10) digits++;
    return (0.74f * (digits - 1) + 0.62f) * 0.46f * ui.unit;
}

/* a number in cream at x (its left), the bar's height: the x after it */
static inline float bpz_bar_number(float x, float cy, int value)
{
    float w = bpz_number_w(value);
    ui_number(x + w / 2, cy, 0.46f * ui.unit, value < 0 ? 0 : value, 1);
    return x + w;
}

/* a small picture (0.6 unit) at x: the x after it */
static inline float bpz_bar_pic(bpz_pics *p, int which, float x, float cy, float k)
{
    float s = 0.6f * ui.unit * k;
    ui_put(bpz_pic(p, which, (int)(0.6f * ui.unit)), x, cy - s / 2, s, s);
    return x + s + 0.1f * ui.unit;
}

/* ---- the win's light: a gold glow on a square, q 0..1 ------------------------------------------------------------ */
static inline void bpz_glow(const bpz_board *b, int file, int rank, int flip, float q)
{
    if (q <= 0) return;
    float x, y;
    bpz_centre(b, file, rank, flip, &x, &y);
    ui_mark_gold(x, y, 1.9f * b->sq * q);
}

/* the menu's, the picker's and the level screen's cards: an nx x ny board filling the card, its frame thin, the holes
 * carved (NULL: none); the square's size, the top left square's corner (*ox, *oy) */
static inline int bpz_thumb_board_holes(int nx, int ny, const uint8_t *holes, int size, uint8_t *rgba, int *ox,
                                        int *oy)
{
    int big = nx > ny ? nx : ny, fr = (int)fmaxf(2, size * 0.035f), sq = (size - 2 * fr) / big;
    if (sq < 1) return 0;
    int w = nx * sq + 2 * fr, h = ny * sq + 2 * fr;
    uint8_t *b = malloc((size_t)w * h * 4);
    if (!b) return 0;
    bpz_paint_board(nx, ny, sq, fr, holes, b);
    art_blend(rgba, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    free(b);
    *ox = (size - w) / 2 + fr;
    *oy = (size - h) / 2 + fr;
    return sq;
}

static inline int bpz_thumb_board(int n, int size, uint8_t *rgba, int *ox)
{
    int oy;
    return bpz_thumb_board_holes(n, n, NULL, size, rgba, ox, &oy);
}

/* a chess piece blended into a card at (x, y), d pixels */
static inline void bpz_thumb_piece(uint8_t *rgba, int size, int type, int color, int x, int y, int d)
{
    if (d < 2) return;
    uint8_t *p = calloc((size_t)d * d, 4);
    if (!p) return;
    art_piece(type, color, d, p);
    art_blend(rgba, size, size, p, d, d, x, y);
    free(p);
}

/* the stars of the logic puzzles: 3 with no hint and no mistake, 2 with up to two hints, else 1 */
static inline int bpz_logic_stars(int hints, int mistakes) { return !hints && !mistakes ? 3 : hints <= 2 ? 2 : 1; }

#endif
