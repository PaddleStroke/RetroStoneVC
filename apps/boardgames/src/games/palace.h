/*
 * The palace boards, for the chess games played on the points of a 9 x 10 board with two palaces (xiangqi, janggi):
 * the plank with its lines and the palaces' diagonals (a river or not, the marks of the starting points or not), the
 * ivory pieces with their character (round discs, or octagons), the board's place on the screen and the point <->
 * pixel mapping, the menu card's part of the board.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions (like grid.h): the build takes a game's own files only, so a shared
 * .c file would be left out of one game's build. Points: x + 9 * y, x the file from the bottom player's left, y the
 * rank from their side (0..9); the palaces are files 3..5, ranks 0..2 and 7..9.
 */
#ifndef PALACE_BOARD_H
#define PALACE_BOARD_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/grid.h"
#include "shell.h"
#include "text.h"

#define PALACE_W 9
#define PALACE_H 10
#define PALACE_FRAME 0.34f                      /* the board's frame, in points' squares */
#define PALACE_RED_INK {176, 30, 26}            /* the red side's characters */

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* a slanting line (the palaces' diagonals, an octagon's sides): a segment's coverage, half-width hw, into the map
 * (stride st, square) */
static inline void palace_seg(float *m, int st, float x0, float y0, float x1, float y1, float hw)
{
    float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    for (int y = (int)(fminf(y0, y1) - hw - 1); y <= (int)(fmaxf(y0, y1) + hw + 1); y++)
        for (int x = (int)(fminf(x0, x1) - hw - 1); x <= (int)(fmaxf(x0, x1) + hw + 1); x++) {
            if (x < 0 || y < 0 || x >= st || y >= st) continue;
            float px = x + 0.5f - x0, py = y + 0.5f - y0, t = fminf(fmaxf((px * dx + py * dy) / len2, 0), 1);
            float c = fminf(fmaxf(hw - hypotf(px - t * dx, py - t * dy) + 0.5f, 0), 1);
            if (c > m[y * st + x]) m[y * st + x] = c;
        }
}

/* the board: the plank in its frame and its lines, the part of the board from point (x0, y0) (its bottom left), nx x
 * ny points of s pixels, frame f. river: the inner files stop between ranks 4 and 5. marks: n points that get the
 * corner marks (the starting points of xiangqi's cannons and soldiers). Lines going on past the part run to the frame
 * (the menu's card). */
static inline void palace_paint(uint8_t *o, int x0, int y0, int nx, int ny, int s, int f, int river,
                                const int (*marks)[2], int n)
{
    static const uint8_t LINE[3] = {34, 24, 16};
    int w = nx * s + 2 * f, h = ny * s + 2 * f, st = w > h ? w : h;
    art_board_n(nx, ny, s, f, 0, o);
    float *m = calloc((size_t)st * st, sizeof *m);   /* square: grid_cover_rect's map */
    if (!m) return;
    float lw = 0.9f + 0.02f * s, edge = 1.8f * lw, mark = 0.09f * s, arm = 0.22f * s;
    float left = (float)f, right = (float)(f + nx * s), top = (float)f, bottom = (float)(f + ny * s);
#define PX(x) (f + ((x) - x0) * s + s / 2.0f)
#define PY(y) (f + (y0 + ny - 1 - (y)) * s + s / 2.0f)
    /* an upright rectangle, cut to the plank */
#define RECT(a, b, c, d) grid_cover_rect(m, st, fmaxf(a, left), fmaxf(b, top), fminf(c, right), fminf(d, bottom))
    for (int y = 0; y < PALACE_H; y++) {        /* the ranks */
        float t = y == 0 || y == PALACE_H - 1 ? edge : lw;
        RECT(PX(0) - edge / 2, PY(y) - t / 2, PX(PALACE_W - 1) + edge / 2, PY(y) + t / 2);
    }
    for (int x = 0; x < PALACE_W; x++) {        /* the files, the inner ones stopping at the river */
        float t = x == 0 || x == PALACE_W - 1 ? edge : lw, e = edge / 2;
        if (t == edge || !river) RECT(PX(x) - t / 2, PY(PALACE_H - 1) - e, PX(x) + t / 2, PY(0) + e);
        else {
            RECT(PX(x) - t / 2, PY(PALACE_H - 1) - e, PX(x) + t / 2, PY(5));
            RECT(PX(x) - t / 2, PY(4), PX(x) + t / 2, PY(0) + e);
        }
    }
    for (int p = 0; p < 2; p++) {               /* the palaces' diagonals */
        int ya = p ? 7 : 0, yb = ya + 2;
        palace_seg(m, st, PX(3), PY(ya), PX(5), PY(yb), lw / 2);
        palace_seg(m, st, PX(5), PY(ya), PX(3), PY(yb), lw / 2);
    }
    for (int i = 0; i < n; i++)                 /* the corner marks */
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2) {
                int x = marks[i][0], y = marks[i][1];
                if (x + sx < 0 || x + sx >= PALACE_W) continue;   /* not past the board's edge */
                float cx = PX(x) + sx * mark, cy = PY(y) + sy * mark, ex = cx + sx * arm, ey = cy + sy * arm;
                RECT(fminf(cx, ex) - lw / 2, cy - lw / 2, fmaxf(cx, ex) + lw / 2, cy + lw / 2);
                RECT(cx - lw / 2, fminf(cy, ey) - lw / 2, cx + lw / 2, fmaxf(cy, ey) + lw / 2);
            }
#undef RECT
#undef PX
#undef PY
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float a = 0.9f * m[y * st + x];
            uint8_t *q = o + 4 * (y * w + x);
            for (int j = 0; j < 3; j++) q[j] = (uint8_t)(q[j] * (1 - a) + LINE[j] * a + 0.5f);
        }
    free(m);
}

/* a piece, size x size: an ivory wooden disc (or an octagon: sides 8), an engraved line round it and the character
 * (a codepoint in help/glyphs.txt) in its side's ink; k: its size (1: a disc 0.9 of the picture across) */
static inline void palace_piece(int sides, float k, int codepoint, const uint8_t ink[3], int size, uint8_t *rgba)
{
    art_style st = art_player[0];
    st.line = fmaxf(0.022f, 1.3f / size);
    art_fig *f = art_fig_new();
    if (sides == 8) {                           /* flat sides up and down, left and right */
        float xy[16], r = 0.45f * k / cosf(3.14159265f / 8);
        for (int i = 0; i < 8; i++) {
            xy[2 * i] = 0.5f + r * cosf(3.14159265f / 8 * (2 * i + 1));
            xy[2 * i + 1] = 0.5f + r * sinf(3.14159265f / 8 * (2 * i + 1));
        }
        art_poly(f, ART_PART, xy, 8);
    } else art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.45f * k, 0.45f * k);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
    uint8_t *layer = calloc((size_t)size * size, 4);
    if (!layer) return;
    if (sides == 8) {                           /* the engraved octagon */
        float *m = calloc((size_t)size * size, sizeof *m), c = size / 2.0f;
        float r = 0.37f * k * size / cosf(3.14159265f / 8), hw = 0.018f * k * size;
        if (m) {
            for (int i = 0; i < 8; i++) {
                float a0 = 3.14159265f / 8 * (2 * i + 1), a1 = a0 + 3.14159265f / 4;
                palace_seg(m, size, c + r * cosf(a0), c + r * sinf(a0), c + r * cosf(a1), c + r * sinf(a1), hw);
            }
            for (int i = 0; i < size * size; i++) {
                memcpy(layer + 4 * i, ink, 3);
                layer[4 * i + 3] = (uint8_t)(0.9f * 255 * m[i] + 0.5f);
            }
            art_blend(rgba, size, size, layer, size, size, 0, 0);
            free(m);
        }
    } else {                                    /* the engraved ring */
        int d = (int)(0.78f * k * size);
        d -= (size - d) & 1;                    /* centred to the pixel */
        art_ring(d, 0.91f, ink, 0.9f, layer);
        art_blend(rgba, size, size, layer, d, d, (size - d) / 2, (size - d) / 2);
    }
    memset(layer, 0, (size_t)size * size * 4);
    text_glyph(codepoint, 0.5f * k * size, 1, ink, layer, size, size);
    art_blend(rgba, size, size, layer, size, size, 0, 0);
    free(layer);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
typedef struct {
    int sq, fr;                                 /* a point's square and the frame, pixels */
    float bx, by;                               /* the plank's top left corner (inside the frame) */
} palace;

/* the board in the box: its place and size */
static inline void palace_fit(palace *b, box bx)
{
    b->sq = (int)floorf(fminf(bx.w / (PALACE_W + 2 * PALACE_FRAME), bx.h / (PALACE_H + 2 * PALACE_FRAME)));
    b->fr = (int)(b->sq * PALACE_FRAME);
    int w = PALACE_W * b->sq + 2 * b->fr, h = PALACE_H * b->sq + 2 * b->fr;
    b->bx = floorf(bx.x + (bx.w - w) / 2) + b->fr;
    b->by = floorf(bx.y + (bx.h - h) / 2) + b->fr;
}

/* the whole board's picture (w x h: the plank and its frame) as a texture */
static inline SDL_Texture *palace_board_tex(const palace *b, int river, const int (*marks)[2], int n)
{
    int w = PALACE_W * b->sq + 2 * b->fr, h = PALACE_H * b->sq + 2 * b->fr;
    palace_paint(ui_buf(w, h), 0, 0, PALACE_W, PALACE_H, b->sq, b->fr, river, marks, n);
    return ui_tex(w, h, ui_buf(w, h));
}

static inline void palace_board(const palace *b, SDL_Texture *t)
{
    ui_put(t, b->bx - b->fr, b->by - b->fr, (float)(PALACE_W * b->sq + 2 * b->fr),
           (float)(PALACE_H * b->sq + 2 * b->fr));
}

/* a point's centre on the screen */
static inline void palace_xy(const palace *b, int p, float *x, float *y)
{
    int c = p % PALACE_W, r = PALACE_H - 1 - p / PALACE_W;
    if (ui.flipped) { c = PALACE_W - 1 - c; r = PALACE_H - 1 - r; }
    *x = b->bx + c * b->sq + b->sq / 2.0f;
    *y = b->by + r * b->sq + b->sq / 2.0f;
}

/* the point under (x, y), or -1 off the board */
static inline int palace_pt(const palace *b, float x, float y)
{
    int c = (int)floorf((x - b->bx) / b->sq), r = (int)floorf((y - b->by) / b->sq);
    if (c < 0 || c >= PALACE_W || r < 0 || r >= PALACE_H) return -1;
    if (ui.flipped) { c = PALACE_W - 1 - c; r = PALACE_H - 1 - r; }
    return (PALACE_H - 1 - r) * PALACE_W + c;
}

/* the menu's card: n x n points of the board from point (x0, y0) (its bottom left) in the middle of the picture;
 * a piece of side s at column c, row r from the top left goes at (x + c * s, y + r * s) */
typedef struct { int s, x, y; } palace_pic;

static inline palace_pic palace_thumb(int x0, int y0, int n, int river, const int (*marks)[2], int nm, int size,
                                      uint8_t *rgba)
{
    int s = (int)(size / (n + 2 * PALACE_FRAME)), f = (int)(s * PALACE_FRAME), w = n * s + 2 * f, o = (size - w) / 2;
    uint8_t *board = ui_buf(w, w);
    palace_paint(board, x0, y0, n, n, s, f, river, marks, nm);
    art_blend(rgba, size, size, board, w, w, o, o);
    return (palace_pic){s, o + f, o + f};
}

#endif
