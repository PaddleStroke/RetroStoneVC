/*
 * The shogi pieces and board as shogi.c paints them (the wooden wedges and their characters, the ruled plank with its
 * four star points), for Shogi Mate. Written once, here, as static inline functions; shogi.c has its own copy of the
 * same painting (static), which could move here so the two games share it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef SHOGIMATE_ART_H
#define SHOGIMATE_ART_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/grid.h"
#include "games/shogi.h"
#include "shell.h"
#include "text.h"

#define SMA_KING1 (SG_KING | SG_PROMOTED)       /* the slot of gote's king (王): no king promotes */

static inline int sma_tex_of(int pc) { return SG_KIND(pc) == SG_KING && SG_OWNER(pc) ? SMA_KING1 : SG_FACE(pc); }

static inline int sma_glyph(int ix)
{
    switch (ix) {
    case SG_PAWN: return 0x6B69;                /* 歩 */
    case SG_LANCE: return 0x9999;               /* 香 */
    case SG_KNIGHT: return 0x6842;              /* 桂 */
    case SG_SILVER: return 0x9280;              /* 銀 */
    case SG_GOLD: return 0x91D1;                /* 金 */
    case SG_BISHOP: return 0x89D2;              /* 角 */
    case SG_ROOK: return 0x98DB;                /* 飛 */
    case SG_KING: return 0x7389;                /* 玉 */
    case SMA_KING1: return 0x738B;              /* 王 */
    case SG_PAWN | SG_PROMOTED: return 0x3068;  /* と */
    case SG_LANCE | SG_PROMOTED: return 0x674F; /* 杏 */
    case SG_KNIGHT | SG_PROMOTED: return 0x572D;    /* 圭 */
    case SG_SILVER | SG_PROMOTED: return 0x5168;    /* 全 */
    case SG_BISHOP | SG_PROMOTED: return 0x99AC;    /* 馬 */
    case SG_ROOK | SG_PROMOTED: return 0x9F8D;  /* 龍 */
    default: return 0;
    }
}

/* the wedge's width (a part of the square) by rank: the king the largest, the pawn the smallest */
static inline float sma_width(int kind)
{
    static const float w[9] = {0, 0.66f, 0.70f, 0.72f, 0.75f, 0.75f, 0.78f, 0.78f, 0.81f};
    return w[kind & 15];
}

/* a piece, size x size, pointing up (or down): a pentagon of light wood, a bevel, its character in ink */
static inline void sma_piece(int ix, int down, int size, uint8_t *rgba)
{
    static const art_style wood = {{238, 210, 152}, {70, 46, 24}, {188, 150, 96}, {0, 0, 0}, 0.30f, 0};
    static const uint8_t black[3] = {26, 18, 12}, red[3] = {184, 30, 24};
    art_style s = wood;
    s.line = fmaxf(0.018f, 1.1f / size);
    float w = sma_width(SG_KIND(ix == SMA_KING1 ? SG_KING : ix)), h = w * 1.1f, y0 = 0.5f - h / 2, e = 0.05f * w;
    const float outer[] = {0.5f, y0, 0.5f + 0.40f * w, y0 + 0.24f * h, 0.5f + 0.5f * w, y0 + h,
                           0.5f - 0.5f * w, y0 + h, 0.5f - 0.40f * w, y0 + 0.24f * h};
    const float inner[] = {0.5f, y0 + 1.6f * e, 0.5f + 0.40f * w - e, y0 + 0.24f * h + 0.6f * e,
                           0.5f + 0.5f * w - e, y0 + h - e, 0.5f - 0.5f * w + e, y0 + h - e,
                           0.5f - 0.40f * w + e, y0 + 0.24f * h + 0.6f * e};
    float pts[2][10];
    for (int i = 0; i < 10; i++) {              /* turned round: the light stays on the upper left */
        pts[0][i] = down ? 1 - outer[i] : outer[i];
        pts[1][i] = down ? 1 - inner[i] : inner[i];
    }
    art_fig *f = art_fig_new();
    art_poly(f, ART_PART, pts[0], 5);
    art_poly(f, ART_PART, pts[1], 5);
    art_render(f, &s, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
    uint8_t *g = calloc((size_t)size * size, 4);
    if (!g) return;
    int promoted = ix != SMA_KING1 && (ix & SG_PROMOTED);
    if (text_glyph(sma_glyph(ix), 0.62f * w * size, 1, promoted ? red : black, g, size, size)) {
        if (down)                               /* the character turned round with the piece */
            for (int i = 0, j = size * size - 1; i < j; i++, j--) {
                uint32_t t;
                memcpy(&t, g + 4 * i, 4);
                memcpy(g + 4 * i, g + 4 * j, 4);
                memcpy(g + 4 * j, &t, 4);
            }
        int dy = (int)lroundf((y0 + 0.58f * h - 0.5f) * size);
        art_blend(rgba, size, size, g, size, size, 0, down ? -dy : dy);
    }
    free(g);
}

/* the board: the plank in its frame, n x n squares of s pixels inside a margin, the lines, the four star points;
 * its side in pixels (o NULL: only that) */
static inline int sma_board(uint8_t *o, int n, int s, int f)
{
    static const uint8_t INK[3] = {40, 28, 18};
    int k = s < 75 ? 1 : (int)(0.02f * s + 0.5f), m = n * k, w = n * (s + 2 * k) + 2 * f;
    if (!o) return w;
    art_board_n(n, n, s + 2 * k, f, 0, o);
    float *cov = calloc((size_t)w * w, sizeof *cov);
    if (!cov) return w;
    float lw = 0.8f + 0.018f * s, c0 = (float)(f + m);
    grid_lines(cov, w, c0, c0, n, n, s, lw, 1.6f * lw, 0.8f * lw, 0);
    for (int i = 0; i < 4 && n == 9; i++)
        grid_cover_disc(cov, w, c0 + (i & 1 ? 6 : 3) * s, c0 + (i & 2 ? 6 : 3) * s, fmaxf(1.5f * lw, 0.055f * s));
    grid_ink(o, w, w, cov, w, INK, 0.88f);
    free(cov);
    return w;
}

#endif
