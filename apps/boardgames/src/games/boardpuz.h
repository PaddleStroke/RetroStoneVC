/*
 * What the three chessboard puzzles share on the screen (Chess Puzzles, the Knight's Tour, Eight Queens): the house
 * board of n x n squares in its walnut frame, the house chess pieces, round buttons in the bar (the lamp: a hint; the
 * queens' lines: the attacked squares shaded), the bar's small pictures and numbers, and the win's gold light.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="queens" builds
 * queens.c and queens_*.c), so a shared .c file would be left out of one game's build or another's.
 * Squares: file (0 = a, the left from the first player's side) and rank (0 = the bottom row); flip turns the board
 * round (Chess Puzzles: black at the bottom).
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

static const uint8_t BPZ_BTN[3] = {54, 72, 64}, BPZ_BTN_ON[3] = {150, 118, 46}, BPZ_GOLD[3] = {232, 190, 92},
                     BPZ_RED[3] = {196, 64, 48};

typedef struct {
    int n, sq, fr, made_sq, made_n, made_piece;
    float x, y;                                 /* the top left square's corner */
    SDL_Texture *board, *piece[2][7];
} bpz_board;

/* the board of n x n squares as big as it fits in the area, centred; its pictures (and the pieces' when asked) */
static inline void bpz_layout(bpz_board *b, int n, box area, int pieces)
{
    float side = fminf(area.w, area.h);
    b->n = n;
    b->sq = (int)floorf(side / (n + 2 * BPZ_FRAME));
    b->fr = (int)(b->sq * BPZ_FRAME);
    int w = n * b->sq + 2 * b->fr;
    b->x = floorf(area.x + (area.w - w) / 2) + b->fr;
    b->y = floorf(area.y + (area.h - w) / 2) + b->fr;
    if (b->made_sq != b->sq || b->made_n != n || !b->board) {
        b->made_sq = b->sq;
        b->made_n = n;
        ui_free(&b->board);
        art_board_n(n, n, b->sq, b->fr, 1, ui_buf(w, w));
        b->board = ui_tex(w, w, ui_buf(w, w));
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

static inline void bpz_draw_board(const bpz_board *b)
{
    float w = (float)(b->n * b->sq + 2 * b->fr);
    ui_put(b->board, b->x - b->fr, b->y - b->fr, w, w);
}

/* a square's top left corner */
static inline void bpz_xy(const bpz_board *b, int file, int rank, int flip, float *x, float *y)
{
    if (flip) { file = b->n - 1 - file; rank = b->n - 1 - rank; }
    *x = b->x + file * b->sq;
    *y = b->y + (b->n - 1 - rank) * b->sq;
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
    int f = (int)floorf((x - b->x) / b->sq), r = b->n - 1 - (int)floorf((y - b->y) / b->sq);
    if (f < 0 || f >= b->n || r < 0 || r >= b->n) return 0;
    *file = flip ? b->n - 1 - f : f;
    *rank = flip ? b->n - 1 - r : r;
    return 1;
}

/* 1: a light square (the top left one is light, as the house board's; on 8 x 8, a1 dark) */
static inline int bpz_light(int n, int file, int rank) { return !((file + n - 1 - rank) & 1); }

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
    art_style st = {{rgb[0], rgb[1], rgb[2]}, {rgb[0], rgb[1], rgb[2]}, {rgb[0], rgb[1], rgb[2]},
                    {rgb[0], rgb[1], rgb[2]}, 0.1f, 0};
    uint8_t *t = calloc((size_t)size * size, 4);
    if (t) {
        art_render(f, &st, size, size, (float)size, 0, 0, t);
        art_blend(o, size, size, t, size, size, 0, 0);
        free(t);
    }
    art_fig_free(f);
}

/* a stroke from (x0, y0) to (x1, y1), wd wide */
static inline void bpz_stroke(art_fig *f, float x0, float y0, float x1, float y1, float wd)
{
    float dx = x1 - x0, dy = y1 - y0, l = hypotf(dx, dy) + 1e-6f, nx = -dy / l * wd / 2, ny = dx / l * wd / 2;
    const float q[] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    art_poly(f, ART_PART, q, 4);
}

enum { BPZ_LAMP, BPZ_LINES, BPZ_FLAME, BPZ_TICK, BPZ_LOOP, BPZ_SLASH, BPZ_PICS };

/* a picture (size x size, cleared first): the lamp (a hint), the queen's lines (eight rays: the attacked squares),
 * the flame (a streak), the tick (solved), the loop (a closed tour), the slash (between two numbers) */
static inline void bpz_picture(int which, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    switch (which) {
    case BPZ_LAMP:
        art_ellipse(f, ART_PART, 0.5f, 0.42f, 0.2f, 0.2f);
        {
            const float neck[] = {0.38f, 0.55f, 0.62f, 0.55f, 0.59f, 0.68f, 0.41f, 0.68f};
            art_poly(f, ART_PART, neck, 4);
        }
        art_rect(f, ART_PART, 0.41f, 0.71f, 0.59f, 0.75f);
        art_rect(f, ART_PART, 0.44f, 0.78f, 0.56f, 0.83f);
        for (int i = -2; i <= 2; i++) {
            float a = (90 + 38 * i) * PUZ_PI / 180, c = cosf(a), s = -sinf(a);
            bpz_stroke(f, 0.5f + c * 0.27f, 0.42f + s * 0.27f, 0.5f + c * 0.37f, 0.42f + s * 0.37f, 0.05f);
        }
        bpz_paint(o, size, f, PUZ_CREAM);
        return;
    case BPZ_LINES:
        for (int i = 0; i < 8; i++) {
            float a = i * PUZ_PI / 4, c = cosf(a), s = sinf(a), l = i % 2 ? 0.33f : 0.38f;
            bpz_stroke(f, 0.5f + c * 0.12f, 0.5f + s * 0.12f, 0.5f + c * l, 0.5f + s * l, 0.06f);
        }
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.08f, 0.08f);
        bpz_paint(o, size, f, PUZ_CREAM);
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

/* the round buttons at the bar's right end, i = 0 the rightmost: their centre and diameter */
static inline void bpz_button_at(box bar, int i, float *x, float *y, float *d)
{
    *d = 0.84f * ui.unit;
    *x = bar.x + bar.w - *d / 2 - 0.05f * ui.unit - i * (*d + 0.18f * ui.unit);
    *y = bar.y + bar.h / 2;
}

typedef struct { int size; SDL_Texture *disc, *on; } bpz_discs;

/* a round button as the shell's own: a green disc (gold-brown when on), the cream picture; dimmed when it can do
 * nothing now */
static inline void bpz_button(bpz_discs *ds, bpz_pics *p, int which, box bar, int i, int on, int dim)
{
    float x, y, d;
    bpz_button_at(bar, i, &x, &y, &d);
    int s = (int)d;
    if (ds->size != s) {
        ui_free(&ds->disc);
        ui_free(&ds->on);
        art_disc(s, BPZ_BTN, 1, 0, ui_buf(s, s));
        ds->disc = ui_tex(s, s, ui_buf(s, s));
        art_disc(s, BPZ_BTN_ON, 1, 0, ui_buf(s, s));
        ds->on = ui_tex(s, s, ui_buf(s, s));
        ds->size = s;
    }
    SDL_Texture *disc = on ? ds->on : ds->disc, *pic = bpz_pic(p, which, (int)(0.62f * d));
    Uint8 a = dim ? 110 : 255;
    SDL_SetTextureAlphaMod(disc, a);
    SDL_SetTextureAlphaMod(pic, a);
    ui_put_c(disc, x, y, d, d);
    ui_put_c(pic, x, y, 0.62f * d, 0.62f * d);
    SDL_SetTextureAlphaMod(disc, 255);
    SDL_SetTextureAlphaMod(pic, 255);
}

static inline int bpz_button_hit(box bar, int i, float px, float py)
{
    float x, y, d;
    bpz_button_at(bar, i, &x, &y, &d);
    return bar.w > 0 && hypotf(px - x, py - y) <= 0.62f * d;   /* a little beyond the disc: an easy target */
}

/* ---- the win's light: a gold glow on a square, q 0..1 ------------------------------------------------------------ */
static inline void bpz_glow(const bpz_board *b, int file, int rank, int flip, float q)
{
    if (q <= 0) return;
    float x, y;
    bpz_centre(b, file, rank, flip, &x, &y);
    ui_mark_gold(x, y, 1.9f * b->sq * q);
}

/* the menu's and the picker's cards: an n x n board filling the card, its frame thin; the square's size and the
 * top left square's corner */
static inline int bpz_thumb_board(int n, int size, uint8_t *rgba, int *ox)
{
    int fr = (int)fmaxf(2, size * 0.035f), sq = (size - 2 * fr) / n, w = n * sq + 2 * fr;
    uint8_t *b = malloc((size_t)w * w * 4);
    if (!b) return 0;
    art_board_n(n, n, sq, fr, 1, b);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
    *ox = (size - w) / 2 + fr;
    return sq;
}

/* a chess piece blended into a card at (x, y), d pixels */
static inline void bpz_thumb_piece(uint8_t *rgba, int size, int type, int color, int x, int y, int d)
{
    uint8_t *p = calloc((size_t)d * d, 4);
    if (!p) return;
    art_piece(type, color, d, p);
    art_blend(rgba, size, size, p, d, d, x, y);
    free(p);
}

#endif
