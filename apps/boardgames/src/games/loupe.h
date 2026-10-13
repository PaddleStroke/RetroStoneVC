/*
 * The loupe, for boards whose cells are small on a phone (go 19 x 19, gomoku 15 x 15 and 19 x 19, hex 11 x 11, word
 * tiles): while a finger is down on the board, a square window beside it shows the cells under it magnified (the
 * ghost stone or the target square in it), so the finger can slide to the right point before it lets go. It sits
 * above the finger (below it for the top player face to face, whose hand comes from the top, or when there is no room
 * above), inside the room the shell gave the board, in a walnut frame.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="go" builds go.c and
 * go_*.c), so a shared .c file would be left out of one game's build or the other's. The game draws what the loupe
 * shows itself, between loupe_begin() and loupe_end(), each point of the board at loupe_map()'s place and its sizes
 * times l.z.
 */
#ifndef LOUPE_H
#define LOUPE_H

#include <math.h>

#include "shell.h"

#define LOUPE_SMALL 0.06f                       /* cells under this part of the screen's short side: the loupe shows */
#define LOUPE_Z 2.3f                            /* its magnification */

typedef struct {
    float fx, fy;                               /* the point of the board it magnifies (the finger's) */
    float cx, cy, side, z, cell;                /* its window: the centre, the side; the magnification; a cell */
} loupe;

/* cells of this size (pixels) are small for a finger */
static inline int loupe_small(float cell) { return cell < LOUPE_SMALL * fminf((float)ui.w, (float)ui.h); }

/* the loupe for a finger at (fx, fy) on cells `cell` wide, `cells` of them across, inside the box room: above the
 * finger, or below it (below = 1: the player's hand comes from the top); smaller (down to 0.6 of its size) where the
 * room on that side is short, on the other side when even that does not fit there */
static inline loupe loupe_at(float fx, float fy, float cell, float cells, int below, box room)
{
    loupe l = {fx, fy, 0, 0, floorf(cells * cell * LOUPE_Z), LOUPE_Z, cell};
    float full = l.side, gap = 0.75f * ui.unit, k = 1.05f;   /* k: the frame round it */
    float up = fy - gap - room.y, dn = room.y + room.h - fy - gap;   /* the room above and below the finger */
    float pref = below ? dn : up, other = below ? up : dn;
    int down = pref < 0.6f * full * k && other > pref ? !below : below;
    l.side = floorf(fmaxf(0.6f * full, fminf(full, (down ? dn : up) / k)));
    float half = l.side * k / 2;
    l.cx = clampf(fx, room.x + half, room.x + room.w - half);
    l.cy = clampf(down ? fy + gap + half : fy - gap - half, room.y + half, room.y + room.h - half);
    return l;
}

/* a point of the board (pixels) as the loupe shows it */
static inline void loupe_map(const loupe *l, float x, float y, float *lx, float *ly)
{
    *lx = l->cx + (x - l->fx) * l->z;
    *ly = l->cy + (y - l->fy) * l->z;
}

/* its frame and its shadow, the table in it; what follows is drawn inside it only, until loupe_end() */
static inline void loupe_begin(const loupe *l)
{
    float s = l->side, x0 = floorf(l->cx - s / 2), y0 = floorf(l->cy - s / 2), e = fmaxf(4, 0.022f * s);
    ui_shadow_flat(l->cx + 0.03f * s, l->cy + 0.05f * s, s * 1.08f, s * 1.08f, 0.8f);
    ui_fill(x0 - e, y0 - e, s + 2 * e, s + 2 * e, 92, 60, 40, 255);   /* a walnut frame */
    ui_fill(x0 - e * 0.4f, y0 - e * 0.4f, s + 0.8f * e, s + 0.8f * e, 52, 34, 22, 255);
    SDL_Rect clip = {(int)x0, (int)y0, (int)s, (int)s};
    SDL_RenderSetClipRect(ui.r, &clip);
    ui_fill(x0, y0, s, s, 30, 60, 44, 255);
}

static inline void loupe_end(void) { SDL_RenderSetClipRect(ui.r, NULL); }

/* a finger put down near the ghost stone on small cells (closer than `slack`) is taken as put down on it: the offset
 * from the finger to the ghost, kept while the finger slides (else 0, 0) */
static inline void loupe_snap(float x, float y, float gx, float gy, float slack, float *dx, float *dy)
{
    int near = hypotf(x - gx, y - gy) < slack;
    *dx = near ? gx - x : 0;
    *dy = near ? gy - y : 0;
}

#endif
