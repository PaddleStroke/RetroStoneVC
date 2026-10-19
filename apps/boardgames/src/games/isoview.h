/*
 * The diamond view's fit, shared by Dice Roll and Roll the Block: the map's tiles' own span (not the whole grid's
 * diamond, whose empty corners wasted most of a phone's width), a cell never smaller than a readable minimum, and
 * when the map is then bigger than its room, a camera that follows the piece (clamped to the map's edges).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The frame: a point (x, y, z) of the map at ((x - y + mh) c / 2, (x + y) kb c - z kz c), cell (0, 0)'s top corner
 * at (mh c / 2, 0), as the games' iso().
 */
#ifndef ISOVIEW_H
#define ISOVIEW_H

#include <limits.h>
#include <math.h>

typedef struct { int u0, u1, s0, s1; } iso_span;   /* x - y and x + y over the map's tiles */

static inline void iso_span_init(iso_span *p)
{
    p->u0 = p->s0 = INT_MAX;
    p->u1 = p->s1 = INT_MIN;
}

static inline void iso_span_add(iso_span *p, int x, int y)
{
    if (x - y < p->u0) p->u0 = x - y;
    if (x - y > p->u1) p->u1 = x - y;
    if (x + y < p->s0) p->s0 = x + y;
    if (x + y > p->s1) p->s1 = x + y;
}

/* the tiles' picture in cells (left x0, top y0, w x h) in the frame above: above, room over the back tiles (a piece
 * standing on them), below, under the front ones (their edges and shadows) */
static inline void iso_span_box(const iso_span *p, int mh, float kb, float above, float below, float *x0, float *y0,
                                float *w, float *h)
{
    if (p->u0 > p->u1) {                        /* no tile */
        *x0 = *y0 = 0;
        *w = *h = 1;
        return;
    }
    *x0 = (p->u0 + mh - 1) * 0.5f - 0.1f;
    *w = (p->u1 - p->u0 + 2) * 0.5f + 0.2f;
    *y0 = p->s0 * kb - above;
    *h = (p->s1 - p->s0 + 2) * kb + above + below;
}

/* the cell: the biggest the room takes (w x h cells in bw x bh); well under cmin, cmin (the map then scrolls: not
 * for a hair, the whole map in sight is worth a cell a little under it) */
static inline float iso_cell(float w, float h, float bw, float bh, float cmin)
{
    float c = floorf(fminf(bw / w, bh / h));
    return c < 0.9f * cmin ? floorf(cmin) : c;
}

/* one axis of the picture's offset: the room b0..b0 + bl, the picture's tiles p0..p0 + pl (pixels in the picture):
 * centred when they fit, else the focus f (pixels in the picture) brought to the middle, the edges kept in */
static inline float iso_follow(float b0, float bl, float p0, float pl, float f)
{
    if (pl <= bl) return floorf(b0 + (bl - pl) / 2 - p0);
    float o = b0 + bl / 2 - f, lo = b0 + bl - (p0 + pl), hi = b0 - p0;
    return roundf(o < lo ? lo : o > hi ? hi : o);
}

#endif
