/*
 * Columns of draughts pieces, for the games where the pieces taken go under the column that took them (lasca, and
 * draughts' bashni): the house's men and kings stacked, each piece beneath peeking out under the one above it, the
 * deep ones closer together (a tall column stays on its square).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions (as grid.h: the build takes a game's own files only). A column: its
 * pieces from the top, 2 bits each (piece i at bits 2 i, 2 i + 1: bit 0 its owner, bit 1 a king or an officer), and
 * its height.
 */
#ifndef COLUMN_H
#define COLUMN_H

#include <math.h>
#include <stdint.h>

#include "shell.h"

#define COLUMN_RISE 0.1f                        /* a piece's rise over the one beneath it, in squares: the top five */
#define COLUMN_RISE_DEEP 0.035f                 /* deeper down, closer together */
#define COLUMN_TOP_LAYERS 4

static inline int column_piece(uint64_t pc, int i) { return (int)(pc >> (2 * i) & 3); }   /* i: 0 the top */

/* how high the piece k from the top of a column h high stands over the column's foot, in squares */
static inline float column_rise(int h, int k)
{
    float r = 0;
    for (int j = k; j < h - 1; j++) r += j < COLUMN_TOP_LAYERS ? COLUMN_RISE : COLUMN_RISE_DEEP;
    return r;
}

/* a column h high standing on (cx, cy) (its foot): each piece the picture t[owner][king], size across; sq: the
 * square's side (the rises); lifted (pixels), faded (alpha 0..1); a piece turned round for the top player face to
 * face */
static inline void column_at(uint64_t pc, int h, SDL_Texture *t[2][2], float cx, float cy, float sq, float size,
                             float lift, float alpha)
{
    if (!h) return;
    ui_shadow_flat(cx, cy, size, size, alpha * (1 - fminf(lift / sq, 0.6f)));
    for (int k = h - 1; k >= 0; k--) {
        int p = column_piece(pc, k);
        SDL_Texture *one = t[p & 1][p >> 1];
        float y = cy - lift - column_rise(h, k) * sq;
        SDL_SetTextureAlphaMod(one, (Uint8)(255 * alpha));
        if (ui_upside(p & 1)) ui_put_rot(one, cx, y, size, size, 180);
        else ui_put_c(one, cx, y, size, size);
        SDL_SetTextureAlphaMod(one, 255);
    }
}

/* the same on a card (the thumbnails, size x size): the house's counters d across, the square's side s, the foot on
 * (cx, cy) */
static inline void column_thumb(uint8_t *rgba, int size, uint64_t pc, int h, float cx, float cy, int s, int d)
{
    for (int k = h - 1; k >= 0; k--) {
        int p = column_piece(pc, k);
        uint8_t *one = ui_buf(d, d);
        if (k == h - 1) {
            art_flat_shadow(d, one);
            art_blend(rgba, size, size, one, d, d, (int)(cx + 0.06f * d - d / 2.0f), (int)(cy + 0.1f * d - d / 2.0f));
        }
        art_counter(p >> 1 ? COUNTER_KING : COUNTER_MAN, p & 1, d, one);
        art_blend(rgba, size, size, one, d, d, (int)(cx - d / 2.0f + 0.5f),
                  (int)(cy - column_rise(h, k) * s - d / 2.0f + 0.5f));
    }
}

#endif
