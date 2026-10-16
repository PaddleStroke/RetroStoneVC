/*
 * Roll the Block in RetroStone Board, for one player: stone slabs floating over the green table, seen from the front
 * and above (each slab's top and its front edge, its shadow on the felt), the block a piece of the house wood, drawn in
 * three dimensions so it truly rolls over its edges. The hole a dark well in a brass frame; cracked tiles pale and
 * crazed; the buttons brass (a round one, a cross) with a jewel of their colour, the bridges walnut planks inlaid with
 * that colour (a shut one a faint ghost of itself); the split tile two brass rings, its two targets a ring each (the
 * rules: rollblock_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Swipe on the board, tap one of the four arrows (the shell's tools), tap beside the block (it rolls that way), or use
 * the arrow keys. A roll it cannot make: the block tips a little and settles back, its arrow dimmed. Split in two
 * cubes: the one chosen glows; tap the other cube, its tool (two cubes) or Space to choose it. The lamp: the hint (the
 * next move of a shortest way, its arrow in gold light; from a dead end, the hint first takes back the moves that led
 * there). Undo takes back one move, all the way to the start. The bar: the moves, and at its right end the par (the
 * fewest possible) by a small block. Solved: the block sinks into the hole, gold light rising from it.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/rollblock.h"
#include "games/swipepad.h"
#include "shell.h"

#define KY 0.74f                                /* a cell's depth on the screen, of its width */
#define KZ 0.62f                                /* a unit of height on the screen */
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
static float ox, oy, cell;                      /* the map's top left on the screen (its surface), a cell's width */
static int bw, bh, made_cell = -1, made_serial = -1, made_small = -1, made_wood = -1;
static SDL_Texture *t_board, *t_bridge, *t_plank_top, *t_long, *t_square, *t_small;

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

/* an ellipse ring (or disc: r0 0) seen foreshortened: the distance to its band */
static float ell(float px, float py, float cx, float cy, float rx, float ry)
{
    return hypotf((px - cx) / rx, (py - cy) / ry);
}

/* one slab at (x0, y0) of the picture o (W x H): its top (c x KY c) in its colour, bevelled and grained, then its
 * front edge (c x TH KZ c) darker; front: draw the front edge */
static void paint_slab(uint8_t *o, int W, int H, float x0, float y0, float c, const uint8_t col[3], int front, int wood)
{
    float g = 0.035f * c, tw = c - 2 * g, td = KY * c - 2 * g, fh = TH * KZ * c;
    for (int y = (int)floorf(y0); y < (int)ceilf(y0 + KY * c + (front ? fh : 0)); y++)
        for (int x = (int)floorf(x0); x < (int)ceilf(x0 + c); x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            float px = x + 0.5f - x0 - g, py = y + 0.5f - y0 - g;
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            float n = art_noise(x, y), grain = wood ? 0.05f * sinf(py * 0.32f + 2 * sinf(px * 0.04f)) : 0;
            if (py < td) {                      /* the top */
                float d = rrect(px, py, 0, 0, tw, td, 0.08f * c);
                if (d > 0.5f) continue;
                float e = -d, b = 0.08f * c, k = 1 + 0.07f * n + grain;
                if (e < b) k += (px < tw / 2 && py < td / 2 ? 0.12f : px + py < tw ? 0.06f : -0.12f) * (1 - e / b);
                uint8_t cc[3];
                for (int i = 0; i < 3; i++) cc[i] = (uint8_t)clampf(col[i] * k, 0, 255);
                px_over(p, cc, clampf(0.5f - d, 0, 1));
            } else if (front && py < td + fh + g) {   /* the front edge, darker below */
                float d = rrect(px, py, 0, td - 0.1f * c, tw, td + fh + g, 0.06f * c);
                if (d > 0.5f) continue;
                float f = (py - td) / (fh + g), k = (0.66f - 0.16f * f) * (1 + 0.06f * n + grain);
                uint8_t cc[3];
                for (int i = 0; i < 3; i++) cc[i] = (uint8_t)clampf(col[i] * k, 0, 255);
                px_over(p, cc, clampf(0.5f - d, 0, 1));
            }
        }
}

/* what is on a slab's top: the hole, a crack pattern, a button, the split tile's rings, a ring, a bridge's inlay */
static void paint_mark(uint8_t *o, int W, int H, float x0, float y0, float c, int t, int grp)
{
    float g = 0.035f * c, cx = x0 + c / 2, cy = y0 + g + (KY * c - 2 * g) / 2;
    static const uint8_t brass[3] = {214, 170, 84}, dark[3] = {22, 18, 16}, ink[3] = {96, 80, 60};
    for (int y = (int)(cy - 0.5f * c); y < cy + 0.5f * c; y++)
        for (int x = (int)(x0); x < x0 + c; x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            float px = x + 0.5f, py = y + 0.5f;
            if (t == RLB_HOLE) {                /* a dark well in a brass frame */
                float d = rrect(px, py, cx - 0.36f * c, cy - 0.36f * KY * c, cx + 0.36f * c, cy + 0.36f * KY * c,
                                0.05f * c);
                if (d < 0.5f && d > -0.07f * c)
                    px_over(p, brass, clampf(0.5f - d, 0, 1) * clampf(d + 0.07f * c + 0.5f, 0, 1));
                if (d < -0.07f * c) {
                    uint8_t w[3];
                    float k = clampf((py - (cy - 0.3f * KY * c)) / (0.6f * KY * c), 0, 1);
                    for (int i = 0; i < 3; i++) w[i] = (uint8_t)(dark[i] + 40 * k * k);
                    px_over(p, w, 1);
                }
            } else if (t == RLB_FRAGILE) {      /* crazed: dark hairlines */
                float u = (px - x0) / c, v = (py - y0) / (KY * c);
                float a = fabsf(sinf(u * 9.1f + 2.3f * sinf(v * 7.7f)) * 0.5f + 0.5f - v);
                float b = fabsf(u - 0.5f - 0.25f * sinf(v * 6.0f + u * 3));
                float l = fminf(a, b * 1.3f);
                if (l < 0.03f && u > 0.08f && u < 0.92f && v > 0.08f && v < 0.92f)
                    px_over(p, ink, 0.55f * (1 - l / 0.03f));
            } else if (t == RLB_SOFT || t == RLB_HEAVY) {
                float d;
                if (t == RLB_SOFT) d = (ell(px, py, cx, cy, 0.27f * c, 0.27f * KY * c) - 1) * 0.27f * c;
                else {                          /* a cross of two bars */
                    float u = (px - cx) / c, v = (py - cy) / (KY * c), s = 0.7071f;
                    float a1 = fabsf(u * s + v * s), a2 = fabsf(u * s - v * s), l1 = fabsf(u * s - v * s),
                          l2 = fabsf(u * s + v * s);
                    float b1 = fmaxf(a1 - 0.07f, l1 - 0.3f), b2 = fmaxf(a2 - 0.07f, l2 - 0.3f);
                    d = fminf(b1, b2) * c;
                }
                if (d < 0.5f) {
                    float k = 1 + 0.25f * clampf(-(px - cx + py - cy) / (0.3f * c), -1, 1);
                    uint8_t b[3];
                    for (int i = 0; i < 3; i++) b[i] = (uint8_t)clampf(brass[i] * k, 0, 255);
                    px_over(p, b, clampf(0.5f - d, 0, 1));
                    if (d > -0.03f * c) px_over(p, PZT_DARK, 0.5f * clampf(0.5f - d, 0, 1));
                }
                float j = (ell(px, py, cx, cy, 0.11f * c, 0.11f * KY * c) - 1) * 0.11f * c;
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
                    float d = fabsf(ell(px, py, ccx, cy, rx, rx * KY) - 1) * rx - 0.035f * c;
                    if (d < 0.5f) px_over(p, brass, (t == RLB_RING ? 0.75f : 1) * clampf(0.5f - d, 0, 1));
                }
            } else if (t == RLB_BRIDGE) {       /* the inlay across the plank, in the button's colour */
                float d = rrect(px, py, x0 + 0.16f * c, cy - 0.07f * KY * c, x0 + 0.84f * c, cy + 0.07f * KY * c,
                                0.02f * c);
                if (d < 0.5f) px_over(p, JEWEL[grp], 0.9f * clampf(0.5f - d, 0, 1));
                for (int k = 0; k < 2; k++) {   /* two brass pins */
                    float dd = hypotf(px - (x0 + (k ? 0.88f : 0.12f) * c), (py - cy) / KY) - 0.04f * c;
                    if (dd < 0.5f) px_over(p, brass, clampf(0.5f - dd, 0, 1));
                }
            }
        }
}

static int is_floor_tile(int t) { return t != RLB_VOID && t != RLB_BRIDGE; }

/* the map: every slab but the bridges' (drawn apart: they come and go), their shadows on the table under them */
static void paint_board(const rlb_map *m, float c, int W, int H, uint8_t *o, int with_bridges, unsigned open)
{
    memset(o, 0, (size_t)W * H * 4);
    static const uint8_t shade[3] = {0, 0, 0};
    float fh = TH * KZ * c;
    for (int y = 0; y < m->h; y++)              /* the shadows */
        for (int x = 0; x < m->w; x++) {
            int cc = x + y * m->w, t = m->t[cc];
            if (t == RLB_VOID || (t == RLB_BRIDGE && !(with_bridges && (open >> m->grp[cc] & 1)))) continue;
            float sx = x * c + 0.06f * c, sy = y * KY * c + fh + 0.16f * c;
            for (int yy = (int)sy; yy < sy + KY * c; yy++)
                for (int xx = (int)sx; xx < sx + c; xx++) {
                    if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
                    float d = rrect(xx + 0.5f, yy + 0.5f, sx, sy, sx + c, sy + KY * c, 0.2f * c);
                    if (d < 0) px_over(o + 4 * ((size_t)yy * W + xx), shade, 0.22f * clampf(-d / (0.15f * c), 0, 1));
                }
        }
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++) {
            int cc = x + y * m->w, t = m->t[cc];
            int on = is_floor_tile(t) || (t == RLB_BRIDGE && with_bridges && (open >> m->grp[cc] & 1));
            if (!on) continue;
            int below = y + 1 < m->h ? m->t[cc + m->w] : RLB_VOID;
            int front = !is_floor_tile(below);
            const uint8_t *col = t == RLB_FRAGILE ? CRACKED : t == RLB_BRIDGE ? WALNUT : STONE;
            paint_slab(o, W, H, x * c, y * KY * c, c, col, front, t == RLB_BRIDGE);
            paint_mark(o, W, H, x * c, y * KY * c, c, t, m->grp[cc]);
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

/* ---- the block in three dimensions ---------------------------------------------------------------------------- */
typedef struct { float x, y, z; } v3;

static int map_w = 1;                           /* the map's width (cells), for the lean */

/* a point on the screen: things standing high lean a little outwards from the map's middle (their inner sides show) */
static void proj(v3 p, float *sx, float *sy)
{
    *sx = ox + (p.x + 0.13f * p.z * (p.x - map_w / 2.0f) / fmaxf(map_w / 2.0f, 2)) * cell;
    *sy = oy + p.y * KY * cell - p.z * KZ * cell;
}

/* a box x0..x1, y0..y1, 0..zh (cells) rolled by `deg` the way dir over its leading edge, lifted by lift (sunk: below
 * 0); its faces seen drawn in the house wood, shaded by the light from above, the left and the front */
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
    float L[3] = {-0.33f, 0.42f, 0.84f};
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
        Uint8 s = (Uint8)clampf(255 * (0.4f + 0.62f * fmaxf(0, lit)), 0, 255);
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

/* ---- the layout ------------------------------------------------------------------------------------------------- */
static void block_pic(int size, uint8_t *rgba)  /* a small standing block (the token, the bar) */
{
    int w = (int)(0.42f * size), top = (int)(0.3f * size), fh = (int)(0.6f * size);
    int x0 = (size - w) / 2, y0 = (size - top - fh) / 2;
    uint8_t *b = malloc((size_t)w * fh * 4);
    if (!b) return;
    paint_wood(b, w, fh);
    for (int i = 0; i < w * fh; i++)
        for (int k = 0; k < 3; k++) b[4 * i + k] = (uint8_t)(b[4 * i + k] * 0.72f);
    art_blend(rgba, size, size, b, w, fh, x0, y0 + top);
    paint_wood(b, w, top);
    art_blend(rgba, size, size, b, w, top, x0, y0);
    free(b);
}

/* the cube tool's picture: two wooden cubes side by side */
static void cubes_pic(int kind, int size, uint8_t *rgba)
{
    (void)kind;
    int s = (int)(0.4f * size);
    uint8_t *b = s > 2 ? malloc((size_t)s * s * 4) : NULL;
    if (!b) return;
    paint_wood(b, s, s);
    art_blend(rgba, size, size, b, s, s, (int)(0.06f * size), (int)(0.3f * size));
    art_blend(rgba, size, size, b, s, s, (int)(0.54f * size), (int)(0.3f * size));
    free(b);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    block_pic(size, rgba);
}

static float extent_h(int h) { return h * KY + TH * KZ + 2 * KZ + 0.25f; }   /* the map's height, in cells */

static void layout(box b)
{
    last_box = b;
    map_w = G.m.w;
    cell = floorf(fminf(b.w / (G.m.w + 0.3f), b.h / extent_h(G.m.h)));
    bw = (int)(G.m.w * cell);
    bh = (int)((G.m.h * KY + TH * KZ + 0.3f) * cell);
    ox = floorf(b.x + (b.w - bw) / 2);
    oy = floorf(b.y + (b.h - extent_h(G.m.h) * cell) / 2 + 2 * KZ * cell);
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
        int sw_ = (int)cell, sh_ = (int)((KY + TH * KZ) * cell) + 2;   /* one bridge's slab */
        o = malloc((size_t)sw_ * sh_ * 4);
        ui_free(&t_bridge);
        ui_free(&t_plank_top);
        if (o) {
            memset(o, 0, (size_t)sw_ * sh_ * 4);
            paint_slab(o, sw_, sh_, 0, 0, cell, WALNUT, 1, 1);
            t_bridge = ui_tex(sw_, sh_, o);
            memset(o, 0, (size_t)sw_ * sh_ * 4);
            paint_slab(o, sw_, sh_, 0, 0, cell, WALNUT, 0, 1);
            t_plank_top = ui_tex(sw_, sh_, o);
        }
        free(o);
    }
    int wood = (int)fmaxf(8, cell);
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
        block_pic(small, ui_buf(small, small));
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
static void draw_state(const rlb_state *s, int dir, float deg, float lift, int sink, Uint32 now)
{
    float b[2][5];
    int n = boxes(s, b);
    if (n == 2 && b[1][1] < b[0][1]) {          /* the farther cube first */
        float t[5];
        memcpy(t, b[0], sizeof t);
        memcpy(b[0], b[1], sizeof t);
        memcpy(b[1], t, sizeof t);
    }
    for (int i = 0; i < n; i++) {
        float cx = ox + (b[i][0] + b[i][2]) / 2 * cell, cy = oy + (b[i][1] + b[i][3]) / 2 * KY * cell;
        if (!sink) ui_shadow_flat(cx + 0.05f * cell, cy + 0.05f * cell, (b[i][2] - b[i][0]) * cell * 0.95f,
                                  (b[i][3] - b[i][1]) * KY * cell * 0.95f, 0.5f);
        int act = s->o == RLB_TWO && (int)b[i][0] == (s->act ? s->x2 : s->x) &&
                  (int)b[i][1] == (s->act ? s->y2 : s->y);
        if (act && !solved()) ui_mark_gold_a(cx, cy, 1.5f * cell, 0.55f + 0.2f * sinf(now / 260.0f));
        int moving = s->o != RLB_TWO || act;
        draw_box(b[i][0], b[i][1], b[i][2], b[i][3], b[i][4], moving ? dir : -1, moving ? deg : 0, lift, 1);
    }
}

static void draw(void)
{
    if (made_cell < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
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
            int below = y + 1 < G.m.h ? G.m.t[c + G.m.w] : RLB_VOID;
            SDL_Texture *t = is_floor_tile(below) ? t_plank_top : t_bridge;
            float sx = ox + x * cell, sy = oy + y * KY * cell + (1 - a) * 0.5f * cell;
            float h = (KY + TH * KZ) * cell + 2;
            SDL_SetTextureAlphaMod(t, (Uint8)(255 * (0.16f + 0.84f * a)));
            ui_put(t, sx, sy, cell, h);
            SDL_SetTextureAlphaMod(t, 255);
            {                                   /* the inlay of its colour */
                float cx = sx + cell / 2, cy = sy + (KY * cell) / 2;
                pzk_rgba col = {JEWEL[g][0], JEWEL[g][1], JEWEL[g][2], (Uint8)(60 + 170 * a)};
                pzk_line(cx - 0.32f * cell, cy, cx + 0.32f * cell, cy, 0.1f * KY * cell, 0, col, 0);
            }
        }
    if (hint_way >= 0 && !roll.on) {            /* the hint: the roll's way lit from the block */
        rlb_state n;
        if (rlb_try(&G.m, &G.s, hint_way, &n, NULL)) {
            int cs[2], k = rlb_cells(&n, G.m.w, cs);
            for (int i = 0; i < k; i++)
                if (n.o != RLB_TWO || i == n.act)
                    ui_mark_gold(ox + (cs[i] % G.m.w + 0.5f) * cell, oy + (cs[i] / G.m.w + 0.5f) * KY * cell,
                                 1.2f * cell);
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
            SDL_Rect clip = {0, 0, ui.w, (int)(oy + (hy + 1) * KY * cell - 0.05f * cell)};
            if (s < 2.25f) {
                SDL_RenderSetClipRect(ui.r, &clip);
                draw_state(&G.s, -1, 0, -s, 1, now);
                SDL_RenderSetClipRect(ui.r, NULL);
            }
            float q = puz_pulse(wt, SINK_MS * 0.5f, GLOW_MS);
            if (q > 0)
                ui_mark_gold(ox + (hx + 0.5f) * cell, oy + (hy + 0.5f) * KY * cell, (1.4f + 1.8f * q) * cell * q);
            if (wt < SINK_MS + GLOW_MS) ui_animate();
        } else if (split_fx.on && now - split_fx.t0 < SPLIT_MS) {   /* the cubes dropping onto their rings */
            float t = clampf((now - split_fx.t0) / (float)SPLIT_MS, 0, 1);
            int sx = G.m.split % G.m.w, sy = G.m.split / G.m.w;
            ui_mark_gold(ox + (sx + 0.5f) * cell, oy + (sy + 0.5f) * KY * cell, 2.2f * cell * (1 - t));
            draw_state(&G.s, -1, 0, 1.6f * (1 - puz_ease(t)), 0, now);
            ui_animate();
        } else {
            split_fx.on = 0;
            draw_state(&G.s, dir, deg, 0, 0, now);
        }
    }
    if (G.s.o == RLB_TWO) ui_animate();         /* the chosen cube's glow breathes */
    unsigned can = 0;
    for (int w = 0; w < 4; w++) {
        rlb_state n;
        if (!solved() && rlb_try(&G.m, &G.s, w, &n, NULL)) can |= 1u << w;
    }
    swp_tools_draw(can, roll.on ? -1 : hint_way, !solved(), down);
    if (G.m.split >= 0) ui_tool_paint(B_CUBE, cubes_pic, 0, 0.72f, down == B_CUBE + 100 ? UI_ON : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------ */
static int on_cube(float x, float y)            /* the cube under the finger (split): 0, 1; else -1 */
{
    if (G.s.o != RLB_TWO) return -1;
    float k[2][2] = {{G.s.x, G.s.y}, {G.s.x2, G.s.y2}};
    for (int i = 0; i < 2; i++) {
        float cx = ox + (k[i][0] + 0.5f) * cell, cy = oy + (k[i][1] + 0.5f) * KY * cell - 0.4f * KZ * cell;
        if (fabsf(x - cx) < 0.6f * cell && fabsf(y - cy) < 0.75f * cell) return i;
    }
    return -1;
}

/* a tap beside the block: it rolls that way (the chosen cube, split) */
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
    float cx = ox + (b[i][0] + b[i][2]) / 2 * cell, cy = oy + (b[i][1] + b[i][3]) / 2 * KY * cell;
    float hw = (b[i][2] - b[i][0]) / 2 * cell, hh = (b[i][3] - b[i][1]) / 2 * KY * cell;
    float dx = (x - cx) / fmaxf(hw, 1), dy = (y - cy + 0.4f * b[i][4] * KZ * cell) / fmaxf(hh, 1);
    if (fabsf(dx) < 1 && fabsf(dy) < 1.6f) { ui_sound(SND_DENY); return; }   /* on the block */
    push(fabsf(dx) > fabsf(dy) ? (dx > 0 ? RLB_RIGHT : RLB_LEFT) : (dy > 0 ? RLB_DOWN : RLB_UP));
}

static void press(float x, float y)
{
    sw.on = 0;
    if (G.m.split >= 0 && ui_tool_hit(x, y) == B_CUBE) { down = B_CUBE + 100; ui_animate(); return; }
    down = swp_tool_hit(x, y);
    if (down >= 0) { ui_animate(); return; }
    if (!solved()) swp_begin(&sw, x, y);
}

static void motion(float x, float y)
{
    int w = swp_follow(&sw, x, y, 0.6f * ui.unit);
    if (w >= 0) push(w);
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
        if (swp_tool_hit(x, y) != k) return;
        if (k == SWP_HINT) hint();
        else if (solved()) ui_sound(SND_DENY);
        else push(k);
        return;
    }
    int tap, w = swp_end(&sw, x, y, 0.3f * ui.unit, &tap);
    if (w >= 0) push(w);
    else if (tap && !solved()) tap_at(sw.x0, sw.y0);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two(b, NULL, G.moves, t_small, par(), solved() && G.moves <= par());
}

/* ---- the shell's pictures: a map with the block standing at its start ------------------------------------------- */
static void paint_map(const rlb_map *m, int size, uint8_t *rgba, unsigned open)
{
    float c = floorf(fminf(size * 0.94f / m->w, size * 0.94f / extent_h(m->h)));
    if (c < 3) return;
    int W = (int)(m->w * c), H = (int)((m->h * KY + TH * KZ + 0.3f) * c);
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    paint_board(m, c, W, H, o, 1, open);
    int x0 = (size - W) / 2, y0 = (int)((size - extent_h(m->h) * c) / 2 + 2 * KZ * c);
    art_blend(rgba, size, size, o, W, H, x0, y0);
    free(o);
    /* the block: its front (two units high) and its top */
    int sx = m->start % m->w, sy = m->start / m->w, bwp = (int)c, top = (int)(KY * c), fh = (int)(2 * KZ * c);
    uint8_t *b = malloc((size_t)bwp * (top + fh) * 4);
    if (!b) return;
    paint_wood(b, bwp, fh);
    for (int i = 0; i < bwp * fh; i++)
        for (int k = 0; k < 3; k++) b[4 * i + k] = (uint8_t)(b[4 * i + k] * 0.74f);
    int bx = x0 + (int)(sx * c), by = y0 + (int)(sy * KY * c) - fh;
    art_blend(rgba, size, size, b, bwp, fh, bx, by + top);
    paint_wood(b, bwp, top);
    art_blend(rgba, size, size, b, bwp, top, bx, by);
    free(b);
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
