/*
 * Prickle Patch: the soft sprite layer. The console has 128 sprites (32 on a line); a garden full of pests needs
 * hundreds. So the swarm, the small shots, the flying gems and the poofs are drawn by the CPU into the 8x8 tiles of a
 * BG layer (BG2), like a bitmap but only where something is: each frame, every 8x8 cell of the screen that an object
 * touches gets a tile from a pool (SOFT_POOL tiles, BG2's tile 0 stays clear), the objects' pixels are painted into it
 * under what is there (the first drawn is in front: draw.c draws the nearest first, so a full pool leaves out the far
 * ones), the pool's used tiles are uploaded (rs_tiles_load8) and the cells' map entries set;
 * the cells used the frame before and not now are cleared. The layer's map is anchored on the world (like the
 * garden's): its scroll is the camera's. One palette for all of them (BG palette 2, make_art.py SOFT_PAL); a hit's
 * white flash and an elite's gold are colour remaps. A full pool drops what does not fit (counted: soft_overflow);
 * the frame-cost and soak tests watch it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include <string.h>

#define CELLS_MAX (128 * 64)
static uint16_t cell_tile[CELLS_MAX];      /* scratch: the pool tile of a map cell this frame (0: none; all 0 between frames) */
static uint8_t px[SOFT_POOL][64];          /* scratch: the pool's pixels, rebuilt each frame */
static uint16_t cur[SOFT_POOL];            /* scratch: the cells used this frame (map indices) */
static int n_cur;                          /* scratch */
static int cam_x, cam_y, wx0, wy0, wx1, wy1;    /* scratch: the frame's camera and window (world px) */
static int overflow;                       /* scratch: pixels dropped this frame */
/* saved (draw state): what the map shows now */
static uint16_t prev[SOFT_POOL];
static int n_prev, mw = 64, mh = 32, last_used, last_overflow, peak_used, short_frames;
static int pool = SOFT_POOL_CONSOLE;       /* the tiles used: SOFT_POOL_CONSOLE at 320 x 240, SOFT_POOL on a bigger screen */

/* the remaps: index -> index (0 stays clear) */
static const uint8_t REMAP[REMAP_COUNT][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {0, 1, 2, 2, 3, 4, 5, 6, 2, 8, 9, 2, 11, 12, 2, 14},          /* the hit's flash: every colour a step lighter */
    {0, 1, 2, 2, 3, 4, 5, 7, 4, 5, 7, 3, 4, 5, 3, 7},              /* an elite: gold */
};

void soft_setup(int map_w, int map_h)
{
    mw = map_w;
    mh = map_h;
    pool = rs_screen_width() > RS_SCREEN_W || rs_screen_height() > RS_SCREEN_H ? SOFT_POOL : SOFT_POOL_CONSOLE;
    rs_bg_setup(RS_BG2, mw, mh, VR_SOFT);
    n_prev = 0;
    static const uint8_t blank[64];
    rs_tiles_load8(VR_SOFT, blank, 1);     /* tile 0: clear (the logo's tiles share the pool's range on the title) */
}

void soft_begin(int cx, int cy, int x0, int y0, int x1, int y1)
{
    cam_x = cx;
    cam_y = cy;
    wx0 = x0;
    wy0 = y0;
    wx1 = x1;
    wy1 = y1;
    n_cur = 0;
    overflow = 0;
}

void soft_draw(int frame, int x, int y, int flip, int remap)
{
    if ((unsigned)frame >= SF_COUNT) return;
    const pp_soft_def *d = &pp_soft[frame];
    int w = d->w, h = d->h;
    x += flip ? d->oxf : d->ox;                       /* (x, y: the frame's top-left; its pixels are cropped) */
    y += d->oy;
    int ax = x < wx0 ? wx0 : x, ay = y < wy0 ? wy0 : y;
    int bx = x + w > wx1 ? wx1 : x + w, by = y + h > wy1 ? wy1 : y + h;
    if (ax >= bx || ay >= by) return;
    const uint8_t *src = pp_soft_px + d->off;
    const uint8_t *rm = REMAP[(unsigned)remap < REMAP_COUNT ? remap : 0];
    for (int cy = ay >> 3; cy <= (by - 1) >> 3; cy++) {
        int r0 = cy * 8 > ay ? cy * 8 : ay, r1 = cy * 8 + 8 < by ? cy * 8 + 8 : by;
        for (int cx = ax >> 3; cx <= (bx - 1) >> 3; cx++) {
            int c0 = cx * 8 > ax ? cx * 8 : ax, c1 = cx * 8 + 8 < bx ? cx * 8 + 8 : bx;
            int m = (cy & (mh - 1)) * mw + (cx & (mw - 1));
            int t = cell_tile[m];
            if (!t) {
                if (n_cur + 1 >= pool) {      /* the pool is full: these pixels are dropped */
                    overflow += (r1 - r0) * (c1 - c0);
                    continue;
                }
                t = ++n_cur;
                cur[t - 1] = (uint16_t)m;
                cell_tile[m] = (uint16_t)t;
                memset(px[t], 0, 64);
            }
            uint8_t *dst = px[t];
            for (int py = r0; py < r1; py++) {
                const uint8_t *s = src + (py - y) * w;
                uint8_t *o = dst + (py & 7) * 8;
                if (flip) {
                    for (int qx = c0; qx < c1; qx++) {
                        int v = s[w - 1 - (qx - x)];
                        if (v && !o[qx & 7]) o[qx & 7] = rm[v];
                    }
                } else {
                    for (int qx = c0; qx < c1; qx++) {
                        int v = s[qx - x];
                        if (v && !o[qx & 7]) o[qx & 7] = rm[v];
                    }
                }
            }
        }
    }
}

void soft_end(void)
{
    uint16_t *map = rs_bg_map(RS_BG2);
    for (int i = 0; i < n_prev; i++)
        if (!cell_tile[prev[i]]) map[prev[i]] = 0;
    for (int i = 0; i < n_cur; i++) map[cur[i]] = RS_MAP(i + 1, PAL_SOFT, 0, 0, 0);
    /* the used tiles, and tile 0 cleared again each frame (px[0] stays 0): the empty cells never show what was written
     * there before (the title's logo once was: a dash in every cell of the garden) */
    rs_tiles_load8(VR_SOFT, px[0], n_cur + 1);
    for (int i = 0; i < n_cur; i++) {
        cell_tile[cur[i]] = 0;
        prev[i] = cur[i];
    }
    n_prev = n_cur;
    last_used = n_cur;
    last_overflow = overflow;
    if (n_cur > peak_used) peak_used = n_cur;
    if (overflow) short_frames++;
    rs_bg_scroll(RS_BG2, cam_x & (mw * 8 - 1), cam_y & (mh * 8 - 1));
}

int soft_overflow(void) { return short_frames; }
int soft_used(void) { return peak_used; }

#define S(v) rs_state_var("soft." #v, &(v), sizeof(v))
void soft_state(void)
{
    S(prev); S(n_prev); S(mw); S(mh); S(last_used); S(last_overflow); S(peak_used); S(short_frames); S(pool);
}
#undef S
