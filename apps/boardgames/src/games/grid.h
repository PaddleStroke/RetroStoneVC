/*
 * The go board, for every game played with go stones on its points (go, gomoku): the wood with its lines and star
 * points, the stones with their flat shadows, the point <-> pixel mapping, the last stone's ring, and the two-tap ghost
 * stone (a tap shows a pale stone on the nearest point, a second tap on it plays it; a drag moves it, its release
 * does not play).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="go" builds
 * go.c and go_*.c), so a shared .c file would be left out of one game's build or the other's. Each game keeps its own
 * grid (the textures are made for its board).
 * Points: x + y * n, x from the left, y from the bottom (player 0's side). A point's stone: 0 empty, else player + 1.
 */
#ifndef GRID_H
#define GRID_H

#include <math.h>
#include <stdlib.h>

#include "shell.h"

#define GRID_FRAME 0.035f                       /* the board's frame, a part of its side */
#define GRID_GHOST_A 150

typedef struct {
    int n;                                      /* points a side */
    int sq, fr;                                 /* a point's square and the frame, pixels */
    float ox, oy;                               /* the board's picture, its top left corner */
    int made_sq, made_n;
    SDL_Texture *t_board, *t_stone[2], *t_ring;
    struct { int on, pt, down, armed, moved; } ghost;
} grid;

typedef int (*grid_legal)(int pt);              /* 1: a stone may go on pt */

/* player 0 plays ebony: the house's second colour */
static inline int grid_art(int player) { return player ? 0 : 1; }

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* coverage of the lines and star points (0..1 a pixel, the most of what covers it), then inked in once */
static inline void grid_cover_rect(float *m, int w, float x0, float y0, float x1, float y1)
{
    for (int y = (int)floorf(y0); y < (int)ceilf(y1); y++)
        for (int x = (int)floorf(x0); x < (int)ceilf(x1); x++) {
            if (x < 0 || y < 0 || x >= w || y >= w) continue;
            float cx = fminf(x + 1, x1) - fmaxf(x, x0), cy = fminf(y + 1, y1) - fmaxf(y, y0), c = cx * cy;
            if (c > m[y * w + x]) m[y * w + x] = c;
        }
}

static inline void grid_cover_disc(float *m, int w, float cx, float cy, float r)
{
    for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
        for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
            if (x < 0 || y < 0 || x >= w || y >= w) continue;
            float d = hypotf(x + 0.5f - cx, y + 0.5f - cy), c = fminf(fmaxf(r - d + 0.5f, 0), 1);
            if (c > m[y * w + x]) m[y * w + x] = c;
        }
}

/* the star points: the corners' and the middle's (9 x 9, 13 x 13, 15 x 15), nine on 19 x 19 */
static inline int grid_hoshi(int size, int xy[9][2])
{
    int a = size == 9 ? 2 : 3, b = size - 1 - a, c = size / 2, k = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            int x = i == 0 ? a : i == 1 ? c : b, y = j == 0 ? a : j == 1 ? c : b;
            if (size == 19 || (i != 1 && j != 1) || (i == 1 && j == 1)) { xy[k][0] = x; xy[k][1] = y; k++; }
        }
    return k;
}

/* the board: the plank in its frame (size x size points of s pixels, frame f), the lines, the star points; corner:
 * only the top left corner of a board, its lines running on to the frame (the menu's card) */
static inline void grid_paint(uint8_t *o, int size, int s, int f, int corner)
{
    static const uint8_t INK[3] = {34, 24, 16};
    int w = size * s + 2 * f;
    art_board_n(size, size, s, f, 0, o);
    float *m = calloc((size_t)w * w, sizeof *m);
    if (!m) return;
    float lw = 0.9f + 0.02f * s, edge = 1.8f * lw, c0 = f + s / 2.0f, c1 = c0 + (size - 1) * s;
    float end = corner ? (float)(f + size * s) : c1 + edge / 2;
    for (int i = 0; i < size; i++) {
        float c = c0 + i * s, t = (i == 0 || (i == size - 1 && !corner)) ? edge : lw;
        grid_cover_rect(m, w, c - t / 2, c0 - edge / 2, c + t / 2, end);
        grid_cover_rect(m, w, c0 - edge / 2, c - t / 2, end, c + t / 2);
    }
    int xy[9][2], k = corner ? 1 : grid_hoshi(size, xy);
    if (corner) xy[0][0] = xy[0][1] = 3;
    for (int i = 0; i < k; i++)
        grid_cover_disc(m, w, c0 + xy[i][0] * s, c0 + xy[i][1] * s, fmaxf(1.6f * lw, 0.11f * s));
    for (int i = 0; i < w * w; i++) {
        float a = 0.9f * m[i];
        for (int j = 0; j < 3; j++) o[4 * i + j] = (uint8_t)(o[4 * i + j] * (1 - a) + INK[j] * a + 0.5f);
    }
    free(m);
}

static inline void grid_token(int player, int size, uint8_t *rgba)
{
    art_counter(COUNTER_STONE, grid_art(player), size, rgba);
}

static inline SDL_Texture *grid_stone_tex(int player, int size)
{
    grid_token(player, size, ui_buf(size, size));
    return ui_tex(size, size, ui_buf(size, size));
}

/* a card's board (the menu's: a corner of nn x nn points; the picker's: the whole board), then its stones: column c
 * and row r from the top left */
typedef struct { int s, x0, y0; } grid_pic;

static inline grid_pic grid_thumb(int nn, int corner, int size, uint8_t *rgba)
{
    int f = (int)(size * (corner ? 0.06f : 0.04f)), s = (size - 2 * f) / nn, w = nn * s + 2 * f, o = (size - w) / 2;
    uint8_t *board = ui_buf(w, w);
    grid_paint(board, nn, s, f, corner);
    art_blend(rgba, size, size, board, w, w, o, o);
    return (grid_pic){s, o + f, o + f};
}

static inline void grid_thumb_stone(grid_pic p, int c, int r, int player, int size, uint8_t *rgba)
{
    uint8_t *one = ui_buf(p.s, p.s);
    grid_token(player, p.s, one);
    art_blend(rgba, size, size, one, p.s, p.s, p.x0 + c * p.s, p.y0 + r * p.s);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static inline void grid_layout(grid *g, box b)
{
    int n = g->n;
    g->sq = (int)(b.w * (1 - 2 * GRID_FRAME) / n);
    g->sq -= !(g->sq & 1);                      /* odd: the points on pixel centres, the lines crisp */
    g->fr = (int)((b.w - n * g->sq) / 2);       /* the frame takes what is left: the board fills its square */
    float bw = (float)(n * g->sq + 2 * g->fr);
    g->ox = floorf(b.x + (b.w - bw) / 2);
    g->oy = floorf(b.y + (b.h - bw) / 2);
    if (g->made_sq == g->sq && g->made_n == n) return;
    g->made_sq = g->sq;
    g->made_n = n;
    int w = n * g->sq + 2 * g->fr;
    ui_free(&g->t_board);
    grid_paint(ui_buf(w, w), n, g->sq, g->fr, 0);
    g->t_board = ui_tex(w, w, ui_buf(w, w));
    for (int p = 0; p < 2; p++) {
        ui_free(&g->t_stone[p]);
        g->t_stone[p] = grid_stone_tex(p, g->sq);
    }
    int rs = (int)fmaxf(8, 0.46f * g->sq);
    static const uint8_t WHITE[3] = {255, 255, 255};
    ui_free(&g->t_ring);
    art_ring(rs, 0.70f, WHITE, 1, ui_buf(rs, rs));
    g->t_ring = ui_tex(rs, rs, ui_buf(rs, rs));
}

static inline void grid_xy(const grid *g, int pt, float *x, float *y)
{
    int n = g->n, c = pt % n, r = n - 1 - pt / n;   /* r: rows from the top */
    if (ui.flipped) { c = n - 1 - c; r = n - 1 - r; }
    *x = g->ox + g->fr + c * g->sq + g->sq / 2.0f;
    *y = g->oy + g->fr + r * g->sq + g->sq / 2.0f;
}

/* the nearest point, or -1 off the board */
static inline int grid_pt(const grid *g, float x, float y)
{
    int n = g->n, c = (int)floorf((x - g->ox - g->fr) / g->sq), r = (int)floorf((y - g->oy - g->fr) / g->sq);
    if (c < 0 || r < 0 || c >= n || r >= n) return -1;
    if (ui.flipped) { c = n - 1 - c; r = n - 1 - r; }
    return c + (n - 1 - r) * n;
}

static inline void grid_board(const grid *g)
{
    float bw = (float)(g->n * g->sq + 2 * g->fr);
    ui_put(g->t_board, g->ox, g->oy, bw, bw);
}

/* a stone of the player, size pixels across (a point's: g->sq), alpha 0..255 */
static inline void grid_stone(const grid *g, int player, float cx, float cy, float size, int alpha)
{
    SDL_Texture *t = g->t_stone[player];
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);
    ui_put_c(t, cx, cy, size, size);
    SDL_SetTextureAlphaMod(t, 255);
}

/* the stones on the points (0 empty, else player + 1), the shadows first: a stone's shadow falls on the wood only;
 * dim (or NULL): the stones shown faded, without a shadow; skip: a point the game draws itself (-1: none) */
static inline void grid_stones(const grid *g, const uint8_t *pt, const uint8_t *dim, int skip)
{
    float x, y;
    for (int p = 0; p < g->n * g->n; p++) {
        if (!pt[p] || (dim && dim[p]) || p == skip) continue;
        grid_xy(g, p, &x, &y);
        ui_shadow_flat(x, y, (float)g->sq, (float)g->sq, 1);
    }
    for (int p = 0; p < g->n * g->n; p++) {
        if (!pt[p] || p == skip) continue;
        grid_xy(g, p, &x, &y);
        grid_stone(g, pt[p] - 1, x, y, (float)g->sq, dim && dim[p] ? 100 : 255);
    }
}

/* the last stone played (the player's, on pt): a small ring */
static inline void grid_last(const grid *g, int pt, int player)
{
    float x, y, rs = fmaxf(8, 0.46f * g->sq);
    grid_xy(g, pt, &x, &y);
    if (player == 0) SDL_SetTextureColorMod(g->t_ring, 236, 228, 212);
    else SDL_SetTextureColorMod(g->t_ring, 58, 50, 44);
    ui_put_c(g->t_ring, x, y, rs, rs);
}

/* ---- the ghost stone --------------------------------------------------------------------------------------------- */
static inline void grid_calm(grid *g) { g->ghost.on = g->ghost.down = 0; }

static inline void grid_ghost(const grid *g, int player)
{
    if (!g->ghost.on) return;
    float x, y;
    grid_xy(g, g->ghost.pt, &x, &y);
    grid_stone(g, player, x, y, (float)g->sq, GRID_GHOST_A);
}

/* the ghost to pt if a stone may go there (else none: loud, the refusal's sound) */
static inline void grid_aim(grid *g, int pt, grid_legal legal, int loud)
{
    g->ghost.on = pt >= 0 && legal(pt);
    g->ghost.pt = pt;
    if (!g->ghost.on && loud) ui_sound(SND_DENY);
}

static inline void grid_press(grid *g, float x, float y, grid_legal legal)
{
    int pt = grid_pt(g, x, y);
    if (pt < 0) { g->ghost.on = 0; return; }
    g->ghost.down = 1;
    g->ghost.moved = 0;
    g->ghost.armed = g->ghost.on && g->ghost.pt == pt;   /* the second tap on the ghost plays it */
    if (!g->ghost.armed) grid_aim(g, pt, legal, 1);
    ui_animate();
}

static inline void grid_motion(grid *g, float x, float y, grid_legal legal)
{
    if (!g->ghost.down) return;
    int pt = grid_pt(g, x, y);
    if (pt < 0 || pt == g->ghost.pt) return;
    g->ghost.moved = 1;
    grid_aim(g, pt, legal, 0);
    ui_animate();
}

/* the point to play now (the second tap on the ghost), else -1 */
static inline int grid_release(grid *g)
{
    if (!g->ghost.down) return -1;
    g->ghost.down = 0;
    if (g->ghost.armed && !g->ghost.moved && g->ghost.on) return g->ghost.pt;
    if (g->ghost.moved && !g->ghost.on) ui_sound(SND_DENY);
    return -1;
}

#endif
