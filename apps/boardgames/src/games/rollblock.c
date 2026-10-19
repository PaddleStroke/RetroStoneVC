/*
 * Roll the Block in RetroStone Board, for one player: stone slabs floating over the green table, seen in the diamond
 * view (the map turned a quarter of a right angle, seen from above at a slant: each slab a diamond, its two front
 * edges, its shadow on the felt), the block a piece of the house wood, drawn in three dimensions so it truly rolls over
 * its edges. The hole a dark well in a brass frame; cracked tiles pale and crazed; the buttons brass (a round one, a
 * cross) with a jewel of their colour, the bridges walnut planks inlaid with that colour (a shut one a faint ghost of
 * itself); the split tile two brass rings, its two targets a ring each (the rules: rollblock_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The map's ways on the screen: up (the rules' RLB_UP) runs up and right, right down and right, down down and left,
 * left up and left. Swipe on the board (the way of the four closest to the swipe's), tap one of the four arrows (the
 * shell's tools, turned to the diagonals), tap beside the block (it rolls that way), or use the arrow keys (each the
 * way an eighth of a turn clockwise from it: up rolls up and right). A roll it cannot make: the block tips a little and
 * settles back, its arrow dimmed. Split in two cubes: the one chosen glows; tap the other cube, its tool (two cubes) or
 * Space to choose it. The lamp: the hint (the next move of a shortest way, its arrow in gold light; from a dead end,
 * the hint first takes back the moves that led there). Undo takes back one move, all the way to the start. The bar:
 * the moves, and at its right end the par (the fewest possible) by a small block. Solved: the block sinks into the
 * hole, gold light rising from it.
 * The view fits the tiles themselves (isoview.h); a map too big for a readable block is drawn at it, and the view
 * follows the block (the chosen cube).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/isoview.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/rollblock.h"
#include "games/swipepad.h"
#include "shell.h"

#define KB 0.29f                                /* half a diamond's height on the screen, of its width (a cell) */
#define KZ 0.50f                                /* a unit of height on the screen */
#define TH 0.34f                                /* a slab's thickness */
#define ROLL_MS 170
#define TIP_MS 260
#define SINK_MS 800
#define GLOW_MS 900
#define SPLIT_MS 360
#define BRIDGE_MS 280

enum { B_CUBE = SWP_KNOBS, BTNS };              /* the arrows and the lamp, then the cube's choice */

static const uint8_t STONE[3] = {188, 180, 166}, CRACKED[3] = {226, 212, 182}, WALNUT[3] = {120, 80, 50};
static const uint8_t JEWEL[RLB_GROUPS][3] = {{62, 160, 112}, {66, 108, 196}, {232, 170, 52}, {196, 60, 44}};

static rlb_game G;
static int serial, hint_way = -1, down = -1;
static swp_swipe sw;
static struct { int on, dir; rlb_state from; Uint32 t0; } roll;   /* a roll drawn from `from` the way dir */
static struct { int on, dir; Uint32 t0; } tip;
static struct { Uint32 t0; int on; } split_fx, win;
static Uint32 bridge_t0[RLB_GROUPS];

static box last_box;
static float ox, oy, cell;                      /* the map's picture's top left on the screen, a diamond's width */
static float ex0, ey0, exw, exh;                /* the tiles' picture in the map's (extent(), in cells) */
static int scroll;                              /* the map bigger than its room: the view follows the block */
static int bw, bh, made_cell = -1, made_serial = -1, made_small = -1, made_wood = -1;
static SDL_Texture *t_board, *t_bridge[4], *t_long, *t_square, *t_small;

/* ---- the diamond view ------------------------------------------------------------------------------------------- */
/* a point of the map (x, y in cells from the top left, z up) in a picture of cells c, the map mh rows high: the
 * top corner of cell (0, 0) at (mh c / 2, 0) */
static void iso(float x, float y, float z, float c, int mh, float *sx, float *sy)
{
    *sx = (x - y + mh) * 0.5f * c;
    *sy = (x + y) * KB * c - z * KZ * c;
}

static float edge_len(float c) { return c * hypotf(0.5f, KB); }   /* a cell's side on the screen */
#define AAK (KB / (0.25f + KB * KB))            /* a distance along a cell's top, to pixels across its edge */

/* ---- painting ---------------------------------------------------------------------------------------------------- */
static void px_over(uint8_t *p, const uint8_t rgb[3], float a)
{
    if (a <= 0) return;
    if (a > 1) a = 1;
    float da = p[3] / 255.0f, oa = a + da * (1 - a);
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)((rgb[i] * a + p[i] * da * (1 - a)) / oa + 0.5f);
    p[3] = (uint8_t)(oa * 255 + 0.5f);
}

static float rrect(float px, float py, float x0, float y0, float x1, float y1, float r)
{
    float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, hx = (x1 - x0) / 2 - r, hy = (y1 - y0) / 2 - r;
    float dx = fabsf(px - cx) - hx, dy = fabsf(py - cy) - hy;
    return hypotf(fmaxf(dx, 0), fmaxf(dy, 0)) + fminf(fmaxf(dx, dy), 0) - r;
}

/* an ellipse ring (or disc: r0 0): the distance to its band */
static float ell(float px, float py, float cx, float cy, float rx, float ry)
{
    return hypotf((px - cx) / rx, (py - cy) / ry);
}

/* what is on a slab's top at (px, py) of the top seen square (side c, from its back corner): the hole, a crack
 * pattern, a button, the split tile's rings, a ring, a bridge's pins; painted over p */
static void mark_px(uint8_t *p, float px, float py, float c, int t, int grp)
{
    float cx = c / 2, cy = c / 2;
    static const uint8_t brass[3] = {214, 170, 84}, dark[3] = {22, 18, 16}, ink[3] = {96, 80, 60};
    if (t == RLB_HOLE) {                        /* a dark well in a brass frame */
        float d = rrect(px, py, cx - 0.36f * c, cy - 0.36f * c, cx + 0.36f * c, cy + 0.36f * c, 0.05f * c);
        if (d < 0.5f && d > -0.07f * c)
            px_over(p, brass, clampf(0.5f - d, 0, 1) * clampf(d + 0.07f * c + 0.5f, 0, 1));
        if (d < -0.07f * c) {                   /* its two far walls a little lit, its floor dark */
            uint8_t w[3];
            float k = clampf(fminf(px, py) / c * 2.2f - 0.35f, 0, 1);
            for (int i = 0; i < 3; i++) w[i] = (uint8_t)(dark[i] + 46 * (1 - k) * (1 - k));
            px_over(p, w, 1);
        }
    } else if (t == RLB_FRAGILE) {              /* crazed: dark hairlines */
        float u = px / c, v = py / c;
        float a = fabsf(sinf(u * 9.1f + 2.3f * sinf(v * 7.7f)) * 0.5f + 0.5f - v);
        float b = fabsf(u - 0.5f - 0.25f * sinf(v * 6.0f + u * 3));
        float l = fminf(a, b * 1.3f);
        if (l < 0.03f && u > 0.08f && u < 0.92f && v > 0.08f && v < 0.92f) px_over(p, ink, 0.55f * (1 - l / 0.03f));
    } else if (t == RLB_SOFT || t == RLB_HEAVY) {
        float d;
        if (t == RLB_SOFT) d = (ell(px, py, cx, cy, 0.27f * c, 0.27f * c) - 1) * 0.27f * c;
        else {                                  /* a cross of two bars */
            float u = (px - cx) / c, v = (py - cy) / c, s = 0.7071f;
            float a1 = fabsf(u * s + v * s), a2 = fabsf(u * s - v * s);
            float b1 = fmaxf(a1 - 0.07f, a2 - 0.3f), b2 = fmaxf(a2 - 0.07f, a1 - 0.3f);
            d = fminf(b1, b2) * c;
        }
        if (d < 0.5f) {
            float k = 1 + 0.25f * clampf(-(px - cx + py - cy) / (0.3f * c), -1, 1);
            uint8_t b[3];
            for (int i = 0; i < 3; i++) b[i] = (uint8_t)clampf(brass[i] * k, 0, 255);
            px_over(p, b, clampf(0.5f - d, 0, 1));
            if (d > -0.03f * c) px_over(p, PZT_DARK, 0.5f * clampf(0.5f - d, 0, 1));
        }
        float j = (ell(px, py, cx, cy, 0.11f * c, 0.11f * c) - 1) * 0.11f * c;
        if (j < 0.5f) {
            float k = 1 + 0.4f * clampf(-(px - cx + py - cy) / (0.1f * c), -1, 1);
            uint8_t b[3];
            for (int i = 0; i < 3; i++) b[i] = (uint8_t)clampf(JEWEL[grp][i] * k, 0, 255);
            px_over(p, b, clampf(0.5f - j, 0, 1));
        }
    } else if (t == RLB_SPLIT || t == RLB_RING) {   /* brass rings: two side by side, or one */
        int n = t == RLB_SPLIT ? 2 : 1;
        for (int k = 0; k < n; k++) {
            float rx = (n == 2 ? 0.16f : 0.3f) * c, ccx = n == 2 ? cx + (k ? 0.19f : -0.19f) * c : cx;
            float d = fabsf(ell(px, py, ccx, cy, rx, rx) - 1) * rx - 0.035f * c;
            if (d < 0.5f) px_over(p, brass, (t == RLB_RING ? 0.75f : 1) * clampf(0.5f - d, 0, 1));
        }
    } else if (t == RLB_BRIDGE) {               /* two brass pins (the inlay is drawn with the bridge) */
        for (int k = 0; k < 2; k++) {
            float dd = hypotf(px - (k ? 0.88f : 0.12f) * c, py - cy) - 0.04f * c;
            if (dd < 0.5f) px_over(p, brass, clampf(0.5f - dd, 0, 1));
        }
    }
}

/* one slab, its top's back corner at (X0, Y0) of the picture o (W x H), cells c wide: its top in its colour,
 * bevelled and grained, with its mark; fx, fy: its front right (along x) and front left (along y) edges shown */
static void paint_tile(uint8_t *o, int W, int H, float X0, float Y0, float c, const uint8_t col[3], int t, int grp,
                       int fx, int fy)
{
    float S = edge_len(c), g = 0.035f * S, tw = S - 2 * g, lo = g / S, hi = 1 - g / S, hc = 0.5f * c, kc = KB * c;
    int wood = t == RLB_BRIDGE;
    for (int y = (int)floorf(Y0); y < (int)ceilf(Y0 + 2 * kc + TH * KZ * c) + 1; y++)
        for (int x = (int)floorf(X0 - hc); x < (int)ceilf(X0 + hc); x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            float dx = x + 0.5f - X0, dy = y + 0.5f - Y0;
            float u = (dx / hc + dy / kc) / 2, v = (dy / kc - dx / hc) / 2;
            float fu = u * S - g, fv = v * S - g;
            uint8_t *p = o + 4 * ((size_t)y * W + x), cc[3];
            float n = art_noise(x, y), d = rrect(fu, fv, 0, 0, tw, tw, 0.08f * S) * AAK;
            float uu = -1, zz = 1, k0 = 0;
            if (dx < 0 && fy) {                 /* the front left edge: v = hi */
                uu = hi + dx / hc;
                k0 = 0.74f;
            } else if (dx >= 0 && fx) {         /* the front right edge: u = hi */
                uu = hi - dx / hc;
                k0 = 0.56f;
            }
            if (k0 > 0) zz = ((uu + hi) * kc - dy) / (KZ * c);
            if (k0 > 0 && zz < 0.06f && d > -1) {   /* the edge, under the top's rim */
                float cov = clampf(fminf(fminf((uu - lo) * hc, (hi - uu) * hc), (zz + TH) * KZ * c) + 0.5f, 0, 1);
                float f = clampf(-zz / TH, 0, 1), k = (k0 - 0.14f * f) * (1 + 0.06f * n);
                for (int i = 0; i < 3; i++) cc[i] = (uint8_t)clampf(col[i] * k, 0, 255);
                px_over(p, cc, cov);
            }
            if (d < 0.5f) {                     /* the top */
                float grain = wood ? 0.05f * sinf(fv * 0.55f + 2 * sinf(fu * 0.07f)) : 0;
                float e = -d, b = 0.08f * S, k = 1 + 0.07f * n + grain;
                if (e < b) k += (fu < tw / 2 && fv < tw / 2 ? 0.12f : fu + fv < tw ? 0.06f : -0.12f) * (1 - e / b);
                for (int i = 0; i < 3; i++) cc[i] = (uint8_t)clampf(col[i] * k, 0, 255);
                px_over(p, cc, clampf(0.5f - d, 0, 1));
                if (t != RLB_TILE) mark_px(p, fu + g, fv + g, S, t, grp);
            }
        }
}

static int is_floor_tile(int t) { return t != RLB_VOID && t != RLB_BRIDGE; }
static int floor_at(const rlb_map *m, int x, int y)
{
    return x >= 0 && y >= 0 && x < m->w && y < m->h && is_floor_tile(m->t[x + y * m->w]);
}

/* the map's picture: (w + h) c / 2 x board_h(h + w) c */
static float board_h(int wh) { return wh * KB + TH * KZ + 0.24f; }

/* the map: every slab but the bridges' (drawn apart: they come and go), their shadows on the table under them */
static void paint_board(const rlb_map *m, float c, int W, int H, uint8_t *o, int with_bridges, unsigned open)
{
    memset(o, 0, (size_t)W * H * 4);
    static const uint8_t shade[3] = {0, 0, 0};
    float S = edge_len(c), hc = 0.5f * c, kc = KB * c, drop = TH * KZ * c + 0.12f * c;
    for (int y = 0; y < m->h; y++)              /* the shadows */
        for (int x = 0; x < m->w; x++) {
            int cc = x + y * m->w, t = m->t[cc];
            if (t == RLB_VOID || (t == RLB_BRIDGE && !(with_bridges && (open >> m->grp[cc] & 1)))) continue;
            float X0, Y0;
            iso((float)x, (float)y, 0, c, m->h, &X0, &Y0);
            X0 += 0.04f * c;
            Y0 += drop;
            for (int yy = (int)Y0; yy < Y0 + 2 * kc + 1; yy++)
                for (int xx = (int)(X0 - hc); xx < X0 + hc + 1; xx++) {
                    if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
                    float dx = xx + 0.5f - X0, dy = yy + 0.5f - Y0;
                    float u = (dx / hc + dy / kc) / 2, v = (dy / kc - dx / hc) / 2;
                    float d = rrect(u * S, v * S, 0, 0, S, S, 0.2f * S);
                    if (d < 0) px_over(o + 4 * ((size_t)yy * W + xx), shade, 0.22f * clampf(-d / (0.15f * S), 0, 1));
                }
        }
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++) {
            int cc = x + y * m->w, t = m->t[cc];
            int on = is_floor_tile(t) || (t == RLB_BRIDGE && with_bridges && (open >> m->grp[cc] & 1));
            if (!on) continue;
            const uint8_t *col = t == RLB_FRAGILE ? CRACKED : t == RLB_BRIDGE ? WALNUT : STONE;
            float X0, Y0;
            iso((float)x, (float)y, 0, c, m->h, &X0, &Y0);
            paint_tile(o, W, H, X0, Y0, c, col, t, m->grp[cc], !floor_at(m, x + 1, y), !floor_at(m, x, y + 1));
        }
}

/* a face of the house wood for the block: w x h pixels, its rim darker, a little lit at the top left */
static void paint_wood(uint8_t *o, int w, int h)
{
    pzt_plank(o, w, h, 3);
    static const uint8_t tone[3] = {214, 164, 104};
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            pzt_tint(p, tone, 1);
            float e = fminf(fminf((float)x, (float)(w - 1 - x)), fminf((float)y, (float)(h - 1 - y))) / (0.09f * w);
            float k = e < 1 ? 0.62f + 0.38f * e : 1;
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k, 0, 255);
            p[3] = 255;
        }
}

/* a standing block (1 x 1 x zh) painted in the picture o (W x H): its footprint's back corner at (X0, Y0), cells c
 * wide; its top and two front faces of the wood, three samples a pixel each way */
static void paint_iso_block(uint8_t *o, int W, int H, float X0, float Y0, float c, float zh)
{
    int n = (int)fmaxf(8, edge_len(c));
    uint8_t *top = malloc((size_t)n * n * 4), *side = malloc((size_t)n * 2 * n * 4);
    if (top && side) {
        paint_wood(top, n, n);
        paint_wood(side, n, 2 * n);
        float hc = 0.5f * c, kc = KB * c, e = 0.04f, hi = 1 - e;
        for (int y = (int)(Y0 - zh * KZ * c) - 1; y < Y0 + 2 * kc + 1; y++)
            for (int x = (int)(X0 - hc) - 1; x < X0 + hc + 1; x++) {
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                float acc[4] = {0};
                for (int s = 0; s < 9; s++) {
                    float dx = x + (s % 3 + 0.5f) / 3 - X0, dy = y + (s / 3 + 0.5f) / 3 - Y0;
                    float dt = dy + zh * KZ * c, u = (dx / hc + dt / kc) / 2, v = (dt / kc - dx / hc) / 2, k, a, b;
                    const uint8_t *src;
                    if (u >= e && u <= hi && v >= e && v <= hi) {
                        src = top + 4 * ((int)(v * (n - 1)) * n + (int)(u * (n - 1)));
                        k = 1;
                    } else {
                        a = dx < 0 ? hi + dx / hc : hi - dx / hc;
                        b = ((a + hi) * kc - dy) / (KZ * c);
                        if (a < e || a > hi || b < 0 || b > zh) continue;
                        src = side + 4 * ((int)((1 - b / zh) * (2 * n - 1)) * n + (int)(a * (n - 1)));
                        k = dx < 0 ? 0.76f : 0.56f;
                    }
                    for (int i = 0; i < 3; i++) acc[i] += src[i] * k;
                    acc[3] += 1;
                }
                if (acc[3] > 0) {
                    uint8_t cc[3];
                    for (int i = 0; i < 3; i++) cc[i] = (uint8_t)clampf(acc[i] / acc[3], 0, 255);
                    px_over(o + 4 * ((size_t)y * W + x), cc, acc[3] / 9);
                }
            }
    }
    free(top);
    free(side);
}

/* ---- the block in three dimensions ---------------------------------------------------------------------------- */
typedef struct { float x, y, z; } v3;

static int map_h = 1;                           /* the map's height (cells) */

static void proj(v3 p, float *sx, float *sy)
{
    iso(p.x, p.y, p.z, cell, map_h, sx, sy);
    *sx += ox;
    *sy += oy;
}

/* a box x0..x1, y0..y1, 0..zh (cells) rolled by `deg` the way dir over its leading edge, lifted by lift; its faces
 * seen drawn in the house wood, shaded by the light from above and the left */
static void draw_box(float x0, float y0, float x1, float y1, float zh, int dir, float deg, float lift, float alpha)
{
    v3 c[8];
    for (int i = 0; i < 8; i++) c[i] = (v3){i & 1 ? x1 : x0, i & 2 ? y1 : y0, i & 4 ? zh : 0};
    static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
    float a = deg * PUZ_PI / 180, ca = cosf(a), sa = sinf(a);
    int dx = dir >= 0 ? DX[dir] : 0, dy = dir >= 0 ? DY[dir] : 0;
    float px = dx > 0 ? x1 : x0, py = dy > 0 ? y1 : y0;
    v3 n[6] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    for (int i = 0; i < 8 + 6; i++) {
        v3 *p = i < 8 ? &c[i] : &n[i - 8];
        int vec = i >= 8;
        if (dir >= 0 && deg != 0) {
            float u = dx ? (p->x - (vec ? 0 : px)) * dx : (p->y - (vec ? 0 : py)) * dy, z = p->z;
            float u2 = u * ca + z * sa, z2 = -u * sa + z * ca;
            if (dx) p->x = (vec ? 0 : px) + u2 * dx;
            else p->y = (vec ? 0 : py) + u2 * dy;
            p->z = z2;
        }
        if (!vec) p->z += lift;
    }
    /* each face: four corners (the first edge the shorter), its normal */
    static const int F[6][4] = {{0, 2, 6, 4}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 5, 7, 6}};
    float L[3] = {0.12f, 0.5f, 0.86f};
    for (int f = 0; f < 6; f++) {
        int o[4];                               /* its corners turned to face outwards: (B - A) x (D - A) along n */
        memcpy(o, F[f], sizeof o);
        {
            v3 A = c[o[0]], B = c[o[1]], D = c[o[3]];
            float ux = B.x - A.x, uy = B.y - A.y, uz = B.z - A.z, wx = D.x - A.x, wy = D.y - A.y, wz = D.z - A.z;
            float dot = (uy * wz - uz * wy) * n[f].x + (uz * wx - ux * wz) * n[f].y + (ux * wy - uy * wx) * n[f].z;
            if (dot < 0) { int t = o[1]; o[1] = o[3]; o[3] = t; }
        }
        float sx[4], sy[4], area = 0;
        for (int k = 0; k < 4; k++) proj(c[o[k]], &sx[k], &sy[k]);
        for (int k = 0; k < 4; k++) area += sx[k] * sy[(k + 1) % 4] - sx[(k + 1) % 4] * sy[k];
        if (area <= 0.5f) continue;            /* seen from behind */
        v3 A = c[o[0]], B = c[o[1]], D = c[o[3]];
        float e1 = fabsf(B.x - A.x) + fabsf(B.y - A.y) + fabsf(B.z - A.z),
              e2 = fabsf(D.x - A.x) + fabsf(D.y - A.y) + fabsf(D.z - A.z);
        int square = fabsf(e1 - e2) < 0.01f, swap = e1 > e2 + 0.01f;
        float lit = n[f].x * L[0] + n[f].y * L[1] + n[f].z * L[2];
        Uint8 s = (Uint8)clampf(255 * (0.42f + 0.6f * fmaxf(0, lit)), 0, 255);
        SDL_Color col = {s, s, s, (Uint8)(255 * alpha)};
        static const float U[4] = {0, 1, 1, 0}, V[4] = {0, 0, 1, 1};
        SDL_Vertex v[4];
        for (int k = 0; k < 4; k++)
            v[k] = (SDL_Vertex){{sx[k], sy[k]}, col, {swap ? V[k] : U[k], swap ? U[k] : V[k]}};
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_Texture *t = square ? t_square : t_long;
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_RenderGeometry(ui.r, t, v, 4, idx, 6);
    }
}

/* the soft shadow of a footprint x0..x1, y0..y1 on the slabs: three dark diamonds, each a little wider */
static void draw_footprint_shadow(float x0, float y0, float x1, float y1, float k)
{
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
    for (int l = 0; l < 3; l++) {
        float e = 0.02f + 0.06f * l, q[4][2] = {{x0 - e, y0 - e}, {x1 + e, y0 - e}, {x1 + e, y1 + e}, {x0 - e, y1 + e}};
        SDL_Vertex v[4];
        for (int i = 0; i < 4; i++) {
            float sx, sy;
            proj((v3){q[i][0] + 0.05f, q[i][1] + 0.05f, 0}, &sx, &sy);
            v[i] = (SDL_Vertex){{sx, sy}, {0, 0, 0, (Uint8)(k * (60 - 15 * l))}, {0, 0}};
        }
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(ui.r, NULL, v, 4, idx, 6);
    }
}

static void inset(float *b)                     /* a hair inside its cells: the slabs' edges show */
{
    b[0] += 0.05f;
    b[1] += 0.05f;
    b[2] -= 0.05f;
    b[3] -= 0.05f;
}

/* a state's boxes: the block (one) or the cubes (two): their footprints and height */
static int boxes(const rlb_state *s, float b[2][5])
{
    if (s->o == RLB_TWO) {
        float k[2][2] = {{s->x, s->y}, {s->x2, s->y2}};
        for (int i = 0; i < 2; i++) {
            b[i][0] = k[i][0]; b[i][1] = k[i][1]; b[i][2] = k[i][0] + 1; b[i][3] = k[i][1] + 1; b[i][4] = 1;
            inset(b[i]);
        }
        return 2;
    }
    b[0][0] = s->x;
    b[0][1] = s->y;
    b[0][2] = s->x + (s->o == RLB_LIE_X ? 2 : 1);
    b[0][3] = s->y + (s->o == RLB_LIE_Y ? 2 : 1);
    b[0][4] = s->o == RLB_STAND ? 2 : 1;
    inset(b[0]);
    return 1;
}

/* ---- the arrows: the shell's arrow turned to the map's ways on the screen (the diagonals) ----------------------- */
/* the tools' order: down (down left), left (up left), up (up right), right (down right), then the lamp */
static const int TOOL_WAY[SWP_KNOBS] = {RLB_DOWN, RLB_LEFT, RLB_UP, RLB_RIGHT, SWP_HINT};

static int tool_hit(float x, float y)
{
    int i = ui_tool_hit(x, y);
    return i < 0 || i >= SWP_KNOBS ? -1 : TOOL_WAY[i];
}

/* the arrow (ICON_ARROW, pointing right) turned clockwise to the way's diagonal, sampled smoothly */
static void diag_arrow(int way, int size, uint8_t *o)
{
    static const float DEG[4] = {-45, 45, 135, 225};
    uint8_t *a = calloc((size_t)size * size, 4);
    if (a) {
        art_icon(ICON_ARROW, size, a);
        float r = DEG[way & 3] * PUZ_PI / 180, cr = cosf(r), sr = sinf(r), m = (size - 1) / 2.0f;
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                float dx = x - m, dy = y - m, sx = m + dx * cr + dy * sr, sy = m - dx * sr + dy * cr, acc[4] = {0};
                int x0 = (int)floorf(sx), y0 = (int)floorf(sy);
                for (int k = 0; k < 4; k++) {   /* bilinear, alpha weighted */
                    int xx = x0 + (k & 1), yy = y0 + (k >> 1);
                    if (xx < 0 || yy < 0 || xx >= size || yy >= size) continue;
                    float w = ((k & 1) ? sx - x0 : 1 - (sx - x0)) * ((k >> 1) ? sy - y0 : 1 - (sy - y0));
                    const uint8_t *p = a + 4 * ((size_t)yy * size + xx);
                    float al = w * p[3] / 255.0f;
                    for (int i = 0; i < 3; i++) acc[i] += p[i] * al;
                    acc[3] += al;
                }
                uint8_t *q = o + 4 * ((size_t)y * size + x);
                for (int i = 0; i < 3; i++) q[i] = acc[3] > 0 ? (uint8_t)clampf(acc[i] / acc[3], 0, 255) : 0;
                q[3] = (uint8_t)clampf(acc[3] * 255, 0, 255);
            }
    }
    free(a);
}

static void tools_draw(unsigned can, int lit, int hint, int down_)
{
    for (int i = 0; i < SWP_KNOBS; i++) {
        int k = TOOL_WAY[i];
        if (k == SWP_HINT) {
            ui_tool(i, ICON_HINT, (hint ? 0 : UI_DIM) | (down_ == k ? UI_ON : 0));
            continue;
        }
        int how = UI_TINT | (k == lit ? UI_LIT : 0) | (can >> k & 1 ? 0 : UI_DIM) | (down_ == k ? UI_ON : 0);
        ui_tool_paint(i, diag_arrow, k, 0.62f, how);
    }
}

/* a swipe (dx, dy on the screen): the way of the four closest to it (each way a diagonal: its quarter) */
static int diag_way(float dx, float dy)
{
    if (dx >= 0) return dy < 0 ? RLB_UP : RLB_RIGHT;
    return dy > 0 ? RLB_DOWN : RLB_LEFT;
}

/* ---- the layout ------------------------------------------------------------------------------------------------- */
static void block_pic(int size, uint8_t *rgba)  /* a small standing block (the token) */
{
    float c = 0.5f * size;
    paint_iso_block(rgba, size, size, size / 2.0f, (size + 2 * KZ * c - 2 * KB * c) / 2, c, 2);
}

/* the cube tool's picture: two wooden cubes side by side */
static void cubes_pic(int kind, int size, uint8_t *rgba)
{
    (void)kind;
    float c = 0.44f * size, y0 = (size + KZ * c - 2 * KB * c) / 2;
    paint_iso_block(rgba, size, size, 0.27f * size, y0, c, 1);
    paint_iso_block(rgba, size, size, 0.73f * size, y0, c, 1);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    block_pic(size, rgba);
}

/* the tiles' picture in cells (isoview.h), bridges too: room for the block standing on the back tiles, the edges in
 * front */
static void extent(const rlb_map *m, float *x0, float *y0, float *w, float *h)
{
    iso_span p;
    iso_span_init(&p);
    for (int c = 0; c < m->w * m->h; c++)
        if (m->t[c] != RLB_VOID) iso_span_add(&p, c % m->w, c / m->w);
    iso_span_box(&p, m->h, KB, 2 * KZ, TH * KZ + 0.34f, x0, y0, w, h);
}

/* a state's focus on the map (the block's middle, the chosen cube's) */
static void focus_of(const rlb_state *s, float *fx, float *fy)
{
    float b[2][5];
    boxes(s, b);
    int i = s->o == RLB_TWO && s->act ? 1 : 0;
    *fx = (b[i][0] + b[i][2]) / 2;
    *fy = (b[i][1] + b[i][3]) / 2;
}

/* the picture's place: the tiles centred in the room, or when they are bigger, the block (at fx, fy on the map) kept
 * in the middle, the map's edges kept in */
static void follow(float fx, float fy)
{
    float sx, sy;
    iso(fx, fy, 0, cell, map_h, &sx, &sy);
    ox = iso_follow(last_box.x, last_box.w, ex0 * cell, exw * cell, sx);
    oy = iso_follow(last_box.y, last_box.h, ey0 * cell, exh * cell, sy);
}

static void layout(box b)
{
    last_box = b;
    map_h = G.m.h;
    extent(&G.m, &ex0, &ey0, &exw, &exh);
    cell = iso_cell(exw, exh, b.w, b.h, 1.4f * ui.unit);   /* never too small to read on a phone */
    scroll = exw * cell > b.w + 0.5f || exh * cell > b.h + 0.5f;
    bw = (int)ceilf((G.m.w + G.m.h) * 0.5f * cell) + 1;
    bh = (int)ceilf(board_h(G.m.w + G.m.h) * cell) + 1;
    float fx, fy;
    focus_of(&G.s, &fx, &fy);
    follow(fx, fy);
    if (made_cell != (int)cell || made_serial != serial) {
        made_cell = (int)cell;
        made_serial = serial;
        uint8_t *o = malloc((size_t)bw * bh * 4);
        ui_free(&t_board);
        if (o) {
            paint_board(&G.m, cell, bw, bh, o, 0, 0);
            t_board = ui_tex(bw, bh, o);
        }
        free(o);
        int sw_ = (int)ceilf(cell) + 1, sh_ = (int)((2 * KB + TH * KZ) * cell) + 3;   /* one bridge's slab */
        o = malloc((size_t)sw_ * sh_ * 4);
        for (int k = 0; k < 4; k++) {          /* its front right edge shown (bit 0), its front left (bit 1) */
            ui_free(&t_bridge[k]);
            if (!o) continue;
            memset(o, 0, (size_t)sw_ * sh_ * 4);
            paint_tile(o, sw_, sh_, 0.5f * cell, 0, cell, WALNUT, RLB_BRIDGE, 0, k & 1, k >> 1 & 1);
            t_bridge[k] = ui_tex(sw_, sh_, o);
        }
        free(o);
    }
    int wood = (int)fmaxf(8, edge_len(cell));
    if (made_wood != wood) {
        made_wood = wood;
        ui_free(&t_long);
        ui_free(&t_square);
        paint_wood(ui_buf(wood, 2 * wood), wood, 2 * wood);
        t_long = ui_tex(wood, 2 * wood, ui_buf(wood, 2 * wood));
        paint_wood(ui_buf(wood, wood), wood, wood);
        t_square = ui_tex(wood, wood, ui_buf(wood, wood));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));   /* the par's flag, as in the other level puzzles */
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
    ui_tools_shown(G.m.split >= 0 ? BTNS : SWP_KNOBS);
}

/* ---- the position ----------------------------------------------------------------------------------------------- */
static void calm(void)
{
    roll.on = tip.on = split_fx.on = win.on = 0;
    hint_way = down = -1;
    sw.on = 0;
    memset(bridge_t0, 0, sizeof bridge_t0);
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!rlb_level(&G, level)) rlb_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    rlb_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int solved(void) { return rlb_solved(&G.s, &G.m); }
static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return ui_par_stars(G.moves, par()); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (roll.on && now - roll.t0 < ROLL_MS) || (win.on && now - win.t0 < SINK_MS + GLOW_MS / 2) ||
           (split_fx.on && now - split_fx.t0 < SPLIT_MS);
}

static void push(int way)
{
    if (solved() || way < 0) return;
    rlb_state before = G.s;
    int ev = rlb_move(&G, way);
    hint_way = -1;
    if (!ev) {
        tip.on = 1;
        tip.dir = way;
        tip.t0 = ui_ms();
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    ev -= 16;
    roll.on = 1;
    roll.dir = way;
    roll.from = before;
    roll.t0 = ui_ms();
    ui_sound(SND_MOVE);
    ui_sound_later(SND_PLACE, ROLL_MS - 20);
    if (ev & RLB_EV_SWITCH)
        for (int g = 0; g < RLB_GROUPS; g++)
            if ((before.open ^ G.s.open) >> g & 1) bridge_t0[g] = roll.t0 + ROLL_MS;
    if (ev & RLB_EV_SPLIT) {
        split_fx.on = 1;
        split_fx.t0 = roll.t0 + ROLL_MS;
        ui_sound_later(SND_TAP, ROLL_MS);
    }
    if (ev & RLB_EV_JOIN) ui_sound_later(SND_CAPTURE, ROLL_MS);
    if (ev & RLB_EV_WIN) {
        win.on = 1;
        win.t0 = roll.t0 + ROLL_MS;
        ui_sound_later(SND_DROP, ROLL_MS + 200);
    }
    ui_animate();
}

static void choose(void)
{
    if (!rlb_choose(&G)) { ui_sound(SND_DENY); return; }
    hint_way = -1;
    ui_sound(SND_TAP);
    ui_animate();
}

static void key(SDL_Keycode k)
{
    if (k == SDLK_SPACE || k == SDLK_TAB) { choose(); return; }
    int w = swp_key_way(k);
    if (w >= 0) push(w);
}

/* the hint: the next move of a shortest way (the cube that makes it chosen); from a dead end, back first */
static void hint(void)
{
    if (solved()) { ui_sound(SND_DENY); return; }
    int d, cube, n = rlb_solve(&G.m, &G.s, &d, &cube, NULL);
    while (n < 0 && rlb_undo(&G)) n = rlb_solve(&G.m, &G.s, &d, &cube, NULL);
    roll.on = 0;
    if (n <= 0) { ui_sound(SND_DENY); return; }
    if (G.s.o == RLB_TWO) G.s.act = (int8_t)cube;
    hint_way = d;
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    calm();
    return rlb_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = rlb_script(&G, moves);
    if (solved()) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}


/* ---- drawing ---------------------------------------------------------------------------------------------------- */
static void cell_at(float x, float y, float *sx, float *sy) { proj((v3){x, y, 0}, sx, sy); }

/* sink: lift is how far it went into the hole; what is under the floor not drawn (the box cut at the floor) */
static void draw_state(const rlb_state *s, int dir, float deg, float lift, int sink, Uint32 now)
{
    float b[2][5];
    int n = boxes(s, b);
    if (n == 2 && b[1][0] + b[1][1] < b[0][0] + b[0][1]) {   /* the farther cube first */
        float t[5];
        memcpy(t, b[0], sizeof t);
        memcpy(b[0], b[1], sizeof t);
        memcpy(b[1], t, sizeof t);
    }
    for (int i = 0; i < n; i++) {
        float cx, cy;
        cell_at((b[i][0] + b[i][2]) / 2, (b[i][1] + b[i][3]) / 2, &cx, &cy);
        if (!sink) draw_footprint_shadow(b[i][0], b[i][1], b[i][2], b[i][3], lift > 0 ? 0.6f : 1);
        int act = s->o == RLB_TWO && (int)b[i][0] == (s->act ? s->x2 : s->x) &&
                  (int)b[i][1] == (s->act ? s->y2 : s->y);
        if (act && !solved()) ui_mark_gold_a(cx, cy, 1.3f * cell, 0.55f + 0.2f * sinf(now / 260.0f));
        int moving = s->o != RLB_TWO || act;
        if (sink) {
            if (b[i][4] + lift > 0.02f) draw_box(b[i][0], b[i][1], b[i][2], b[i][3], b[i][4] + lift, -1, 0, 0, 1);
        } else draw_box(b[i][0], b[i][1], b[i][2], b[i][3], b[i][4], moving ? dir : -1, moving ? deg : 0, lift, 1);
    }
}

static void draw(void)
{
    if (made_cell < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    if (scroll) {                               /* the view follows the block as it rolls; the map cut at the room */
        float fx, fy;
        focus_of(&G.s, &fx, &fy);
        if (roll.on && now - roll.t0 < ROLL_MS) {
            float t = puz_ease((now - roll.t0) / (float)ROLL_MS), gx, gy;
            focus_of(&roll.from, &gx, &gy);
            fx = gx + (fx - gx) * t;
            fy = gy + (fy - gy) * t;
        }
        follow(fx, fy);
        SDL_Rect clip = {(int)last_box.x, (int)last_box.y, (int)last_box.w, (int)last_box.h};
        SDL_RenderSetClipRect(ui.r, &clip);
    }
    ui_put(t_board, ox, oy, (float)bw, (float)bh);
    for (int y = 0; y < G.m.h; y++)             /* the bridges, back to front: open, shut (a ghost), changing */
        for (int x = 0; x < G.m.w; x++) {
            int c = x + y * G.m.w, g = G.m.grp[c];
            if (G.m.t[c] != RLB_BRIDGE) continue;
            int open = G.s.open >> g & 1;
            float f = bridge_t0[g] && now > bridge_t0[g] ? clampf((now - bridge_t0[g]) / (float)BRIDGE_MS, 0, 1)
                    : bridge_t0[g] ? 0 : 1;
            float a = open ? f : 1 - f;         /* how far open */
            if (bridge_t0[g] && f < 1) ui_animate();
            SDL_Texture *t = t_bridge[(!floor_at(&G.m, x + 1, y)) | (!floor_at(&G.m, x, y + 1)) << 1];
            float sx, sy, drop = (1 - a) * 0.5f * cell;
            cell_at((float)x, (float)y, &sx, &sy);
            SDL_SetTextureAlphaMod(t, (Uint8)(255 * (0.16f + 0.84f * a)));
            ui_put(t, sx - 0.5f * cell, sy + drop, ceilf(cell) + 1, (int)((2 * KB + TH * KZ) * cell) + 3);
            SDL_SetTextureAlphaMod(t, 255);
            {                                   /* the inlay of its colour, across the plank */
                float cx, cy, ex = 0.3f * 0.5f * cell, ey = 0.3f * KB * cell;
                cell_at(x + 0.5f, y + 0.5f, &cx, &cy);
                cy += drop;
                pzk_rgba col = {JEWEL[g][0], JEWEL[g][1], JEWEL[g][2], (Uint8)(60 + 170 * a)};
                pzk_line(cx - ex, cy - ey, cx + ex, cy + ey, 0.075f * cell, 0, col, 0);
            }
        }
    if (hint_way >= 0 && !roll.on) {            /* the hint: the roll's way lit from the block */
        rlb_state n;
        if (rlb_try(&G.m, &G.s, hint_way, &n, NULL)) {
            int cs[2], k = rlb_cells(&n, G.m.w, cs);
            for (int i = 0; i < k; i++)
                if (n.o != RLB_TWO || i == n.act) {
                    float hx, hy;
                    cell_at(cs[i] % G.m.w + 0.5f, cs[i] / G.m.w + 0.5f, &hx, &hy);
                    ui_mark_gold(hx, hy, 1.0f * cell);
                }
        }
    }
    float wt = win.on ? (float)(int)(now - win.t0) : -1;
    if (roll.on) {
        float t = (now - roll.t0) / (float)ROLL_MS;
        if (t >= 1) roll.on = 0;
        else {
            draw_state(&roll.from, roll.dir, 90 * puz_ease(t), 0, 0, now);
            ui_animate();
        }
    }
    if (!roll.on) {
        float deg = 0;
        int dir = -1;
        if (tip.on) {
            float t = (now - tip.t0) / (float)TIP_MS;
            if (t >= 1) tip.on = 0;
            else { deg = 16 * sinf(PUZ_PI * t); dir = tip.dir; ui_animate(); }
        }
        if (win.on && wt >= 0) {                /* sinking into the hole: what goes under the floor hidden */
            float s = 2.3f * puz_ease(clampf(wt / SINK_MS, 0, 1));
            int hx = G.m.hole % G.m.w, hy = G.m.hole / G.m.w;
            draw_state(&G.s, -1, 0, -s, 1, now);
            float q = puz_pulse(wt, SINK_MS * 0.5f, GLOW_MS), gx, gy;
            cell_at(hx + 0.5f, hy + 0.5f, &gx, &gy);
            if (q > 0) ui_mark_gold(gx, gy, (1.2f + 1.6f * q) * cell * q);
            if (wt < SINK_MS + GLOW_MS) ui_animate();
        } else if (split_fx.on && now - split_fx.t0 < SPLIT_MS) {   /* the cubes dropping onto their rings */
            float t = clampf((now - split_fx.t0) / (float)SPLIT_MS, 0, 1), gx, gy;
            cell_at(G.m.split % G.m.w + 0.5f, G.m.split / G.m.w + 0.5f, &gx, &gy);
            ui_mark_gold(gx, gy, 1.9f * cell * (1 - t));
            draw_state(&G.s, -1, 0, 1.6f * (1 - puz_ease(t)), 0, now);
            ui_animate();
        } else {
            split_fx.on = 0;
            draw_state(&G.s, dir, deg, 0, 0, now);
        }
    }
    if (scroll) SDL_RenderSetClipRect(ui.r, NULL);
    if (G.s.o == RLB_TWO) ui_animate();         /* the chosen cube's glow breathes */
    unsigned can = 0;
    for (int w = 0; w < 4; w++) {
        rlb_state n;
        if (!solved() && rlb_try(&G.m, &G.s, w, &n, NULL)) can |= 1u << w;
    }
    tools_draw(can, roll.on ? -1 : hint_way, !solved(), down);
    if (G.m.split >= 0) ui_tool_paint(B_CUBE, cubes_pic, 0, 0.72f, down == B_CUBE + 100 ? UI_ON : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------ */
static int on_cube(float x, float y)            /* the cube under the finger (split): 0, 1; else -1 */
{
    if (G.s.o != RLB_TWO) return -1;
    float k[2][2] = {{G.s.x, G.s.y}, {G.s.x2, G.s.y2}};
    for (int i = 0; i < 2; i++) {
        float cx, cy;
        proj((v3){k[i][0] + 0.5f, k[i][1] + 0.5f, 0.5f}, &cx, &cy);
        if (fabsf(x - cx) < 0.45f * cell && fabsf(y - cy) < 0.5f * cell) return i;
    }
    return -1;
}

/* a tap beside the block: it rolls the way of the four closest to the tap (the chosen cube, split); on it: refused */
static void tap_at(float x, float y)
{
    int c = on_cube(x, y);
    if (c >= 0) {
        if (c != G.s.act) choose();
        return;
    }
    float b[2][5];
    int n = boxes(&G.s, b), i = G.s.o == RLB_TWO ? G.s.act : 0;
    (void)n;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;   /* the block's outline on the screen */
    for (int k = 0; k < 8; k++) {
        float sx, sy;
        proj((v3){k & 1 ? b[i][2] : b[i][0], k & 2 ? b[i][3] : b[i][1], k & 4 ? b[i][4] : 0}, &sx, &sy);
        x0 = fminf(x0, sx); x1 = fmaxf(x1, sx); y0 = fminf(y0, sy); y1 = fmaxf(y1, sy);
    }
    if (x > x0 && x < x1 && y > y0 && y < y1) { ui_sound(SND_DENY); return; }   /* on the block */
    push(diag_way(x - (x0 + x1) / 2, y - (y0 + y1) / 2));
}

static void press(float x, float y)
{
    sw.on = 0;
    if (G.m.split >= 0 && ui_tool_hit(x, y) == B_CUBE) { down = B_CUBE + 100; ui_animate(); return; }
    down = tool_hit(x, y);
    if (down >= 0) { ui_animate(); return; }
    if (!solved()) swp_begin(&sw, x, y);
}

static void motion(float x, float y)
{
    if (swp_follow(&sw, x, y, 0.6f * ui.unit) >= 0) push(diag_way(x - sw.x0, y - sw.y0));
}

static void release(float x, float y)
{
    if (down >= 0) {
        int k = down;
        down = -1;
        ui_animate();
        if (k == B_CUBE + 100) {
            if (ui_tool_hit(x, y) == B_CUBE) choose();
            return;
        }
        if (tool_hit(x, y) != k) return;
        if (k == SWP_HINT) hint();
        else if (solved()) ui_sound(SND_DENY);
        else push(k);
        return;
    }
    int tap;
    if (swp_end(&sw, x, y, 0.3f * ui.unit, &tap) >= 0) push(diag_way(x - sw.x0, y - sw.y0));
    else if (tap && !solved()) tap_at(sw.x0, sw.y0);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.moves, t_small, par(), solved() && G.moves <= par());
}

/* ---- the shell's pictures: a map with the block standing at its start ------------------------------------------- */
static void paint_map(const rlb_map *m, int size, uint8_t *rgba, unsigned open)
{
    float e0, f0, ew, eh;
    extent(m, &e0, &f0, &ew, &eh);
    float c = floorf(fminf(size * 0.96f / ew, size * 0.96f / eh));
    if (c < 3) return;
    int W = (int)ceilf((m->w + m->h) * 0.5f * c) + 1, H = (int)ceilf(board_h(m->w + m->h) * c) + 1;
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    paint_board(m, c, W, H, o, 1, open);
    int x0 = (int)floorf((size - ew * c) / 2 - e0 * c), y0 = (int)floorf((size - eh * c) / 2 - f0 * c);
    art_blend(rgba, size, size, o, W, H, x0, y0);
    free(o);
    float sx, sy;                               /* the block standing at the start */
    iso(m->start % m->w + 0.05f, m->start / m->w + 0.05f, 0, c, m->h, &sx, &sy);
    paint_iso_block(rgba, size, size, x0 + sx, y0 + sy, 0.9f * c, 2);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    rlb_game T;
    if (variant < 0) rlb_level(&T, 30);
    else rlb_random(&T, variant, 9u + (uint32_t)variant);
    paint_map(&T.m, size, rgba, T.m.open0);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    rlb_game T;
    if (rlb_level(&T, level)) paint_map(&T.m, size, rgba, T.m.open0);
}

/* a chapter's tab: a small map showing its tiles */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const ICON[RLB_CHAPTERS] = {
        "oSo|ooo|oHo", "Soo.|o.oo|ooH.|o.oo", "Sfo|fof|ofH", "Sa.|oo1|..H", "Sx.|...|yHy", "Sfa|x.1|yHy",
    };
    rlb_map m;
    if (chapter >= 0 && chapter < RLB_CHAPTERS && rlb_parse(&m, ICON[chapter])) paint_map(&m, size, rgba, 0xf);
}

static const game rollblock_game = {
    .id = "rollblock", .order = 446, .variants = RLB_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = RLB_LEVELS, .chapters = RLB_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1, .tools = BTNS,
};
REGISTER_GAME(rollblock_game)
