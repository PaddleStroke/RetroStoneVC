/*
 * Hex Landscape in RetroStone Board, for one player: a little landscape of raised hexagonal tiles seen from a gentle
 * tilt, as a diorama on the table (the rules: hexland_rules.c). Each tile is painted by code at the size it is shown
 * and kept (its meadow with flowers, trees standing up, wheat in rows, houses with their roofs, a river shimmering,
 * rails on their sleepers, its earthen side), so a map of hundreds of tiles draws as that many pictures.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The stack lies on a wooden tray under the map (beside it on a wide screen): the tile to lay, the stack under it and
 * how many are left, the next two. Tap an empty place beside the landscape: the tile hovers there, turned the way that
 * scores best, its edges marked (a green tick where it meets its own terrain, a red cross where it does not); tap it
 * again (or the tray's tile, or the turn tool) to turn it, the tick tool to lay it. It drops with a soft bounce and a
 * puff; points rise from it; a perfect tile shines and sends a tile to the stack; a quest done raises its flag in
 * gold; a forest closed lets birds fly up. The lamp: the hint's placement. Undo: the last tile only. Drag to pan,
 * pinch to zoom (the keys: arrows move the place chosen, Return lays, space turns, + and - zoom, 0 the whole map).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/hexland.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define SQ3 1.7320508f
#define AP 0.8660254f                           /* a hex's apothem (its radius 1) */
#define TILT 0.78f                              /* the view's tilt: the map's depth squashed */
#define THICK 0.20f                             /* a tile's side, of its radius */
#define OVER 0.52f                              /* room above a tile's picture for its trees and roofs */
#define BACK (1u << 30)                         /* a tile's back (the stack) */
#define DROP_MS 460
#define CAM_MS 380
#define TURN_MS 180
#define WIN_MS 1800
enum { T_PLACE, T_TURN, T_HINT, TOOLS };
enum { FX_POP, FX_PUFF, FX_STARS, FX_BIRDS, FX_FLY, FX_FLAG, FX_SKIP };
static const int FX_MS[] = {1100, 620, 1100, 2600, 760, 900, 700};

static hxl_game G;
static int serial, variant_now, best_now;
static int ghost = -1, rot, rot_prev = -1, hint_cell = -1, tool_down = -1;
static Uint32 rot_t0, hint_t0, drop_t0, calm_until;
static int drop_cell = -1, win_on;
static Uint32 win_t0;

typedef struct { int kind, cell, value; Uint32 t0; } fxe;
#define MAX_FX 48
static fxe FX[MAX_FX];
static int nfx;

/* the screen */
static box room, view, tray;
static float tr_cx, tr_cy, tr_r, nx_x[2], nx_y[2], nx_r, cnt_x, cnt_y;   /* the tray's tile, the next ones, the count */
static SDL_Texture *t_tray, *t_par, *t_cup;
static int made_tray_w = -1, made_tray_h = -1, made_small = -1;

/* the camera: the world's point in the middle of the view, pixels a world unit (a hex's radius) */
static float cam_x, cam_y, cam_s = 60, cam_ex, cam_ey, cam_es, cam_tx, cam_ty, cam_ts;
static Uint32 cam_t0;
static int cam_go, user_cam;

/* ---- painting ---------------------------------------------------------------------------------------------------- */
static void px_over(uint8_t *p, float r, float g, float b, float a)
{
    if (a <= 0) return;
    if (a > 1) a = 1;
    float da = p[3] / 255.0f, oa = a + da * (1 - a);
    p[0] = (uint8_t)clampf((r * a + p[0] * da * (1 - a)) / oa + 0.5f, 0, 255);
    p[1] = (uint8_t)clampf((g * a + p[1] * da * (1 - a)) / oa + 0.5f, 0, 255);
    p[2] = (uint8_t)clampf((b * a + p[2] * da * (1 - a)) / oa + 0.5f, 0, 255);
    p[3] = (uint8_t)(oa * 255 + 0.5f);
}

static float hexsdf(float u, float v)          /* < 0 inside the hex of radius 1 (pointy top), in its units */
{
    float au = fabsf(u), av = fabsf(v);
    return fmaxf(au, 0.5f * au + AP * av) - AP;
}

static uint32_t hsh(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9e3779b1u ^ (b * 0x85ebca77u + 0x165667b1u);
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    return h;
}

static float hf(uint32_t a, uint32_t b) { return (hsh(a, b) & 0xffff) / 65535.0f; }

static float smooth(float e0, float e1, float x)
{
    float t = clampf((x - e0) / (e1 - e0), 0, 1);
    return t * t * (3 - 2 * t);
}

/* a polygon (pixels) painted over a picture: lit on the left of splitx, dark on its right, alpha a */
static void poly_fill(uint8_t *o, int W, int H, const float *xy, int n, const float lit[3], const float dark[3],
                      float splitx, float a)
{
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int i = 0; i < n; i++) {
        x0 = fminf(x0, xy[2 * i]), x1 = fmaxf(x1, xy[2 * i]);
        y0 = fminf(y0, xy[2 * i + 1]), y1 = fmaxf(y1, xy[2 * i + 1]);
    }
    for (int y = (int)floorf(y0) < 0 ? 0 : (int)floorf(y0); y < H && y <= (int)y1; y++)
        for (int x = (int)floorf(x0) < 0 ? 0 : (int)floorf(x0); x < W && x <= (int)x1; x++) {
            float c = pzt_cover(xy, n, (float)x, (float)y);
            if (c <= 0) continue;
            float k = smooth(splitx - 1, splitx + 1, x + 0.5f);
            px_over(o + 4 * ((size_t)y * W + x), lit[0] + (dark[0] - lit[0]) * k, lit[1] + (dark[1] - lit[1]) * k,
                    lit[2] + (dark[2] - lit[2]) * k, a * c);
        }
}

static void rect_fill(uint8_t *o, int W, int H, float x0, float y0, float x1, float y1, const float c[3], float a)
{
    const float xy[] = {x0, y0, x1, y0, x1, y1, x0, y1};
    poly_fill(o, W, H, xy, 4, c, c, 1e9f, a);
}

/* a ball (a tree's crown, a bush): lit from the upper left */
static void ball_fill(uint8_t *o, int W, int H, float cx, float cy, float r, const float c[3], float a)
{
    for (int y = (int)(cy - r - 1) < 0 ? 0 : (int)(cy - r - 1); y < H && y <= (int)(cy + r + 1); y++)
        for (int x = (int)(cx - r - 1) < 0 ? 0 : (int)(cx - r - 1); x < W && x <= (int)(cx + r + 1); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = hypotf(dx, dy), cov = clampf(r - d + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float k = 1 + 0.32f * (-dx - dy) / (1.4f * r) - 0.12f * (d / r) * (d / r);
            px_over(o + 4 * ((size_t)y * W + x), c[0] * k, c[1] * k, c[2] * k, a * cov);
        }
}

/* a soft dark ellipse on the ground: a shadow */
static void shade_fill(uint8_t *o, int W, int H, float cx, float cy, float rx, float ry, float a)
{
    for (int y = (int)(cy - ry - 1) < 0 ? 0 : (int)(cy - ry - 1); y < H && y <= (int)(cy + ry + 1); y++)
        for (int x = (int)(cx - rx - 1) < 0 ? 0 : (int)(cx - rx - 1); x < W && x <= (int)(cx + rx + 1); x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, e = dx * dx + dy * dy;
            if (e < 1) px_over(o + 4 * ((size_t)y * W + x), 20, 30, 10, a * (1 - e) * (1 - e));
        }
}

/* ---- a tile's rivers and rails: curves through it -------------------------------------------------------------- */
typedef struct { float p[24][2]; int n; } curve;
typedef struct { curve c[6]; int nc, ends, pond; float pond_r; } paths;

static void edge_mid(int d, float k, float *u, float *v)
{
    float a = d * ART_PI / 3;
    *u = k * AP * cosf(a);
    *v = k * AP * sinf(a);
}

static void make_paths(uint32_t code, int t, paths *P)
{
    int e[6], n = 0;
    memset(P, 0, sizeof *P);
    if (code == BACK) return;
    for (int d = 0; d < 6; d++)
        if (hxl_edge(code, d) == t) e[n++] = d;
    P->ends = n;
    if (!n) return;
    if (n == 2) {                               /* one curve from edge to edge, bending through the middle */
        float au, av, bu, bv;
        edge_mid(e[0], 1.1f, &au, &av);
        edge_mid(e[1], 1.1f, &bu, &bv);
        curve *c = &P->c[P->nc++];
        c->n = 24;
        for (int i = 0; i < 24; i++) {
            float s = i / 23.0f, a = (1 - s) * (1 - s), b = s * s;
            c->p[i][0] = a * au + b * bu;
            c->p[i][1] = a * av + b * bv;
        }
        return;
    }
    for (int i = 0; i < n; i++) {               /* to the middle: a pond, a lake, a junction */
        float au, av;
        edge_mid(e[i], 1.1f, &au, &av);
        curve *c = &P->c[P->nc++];
        c->n = 2;
        c->p[0][0] = au, c->p[0][1] = av;
        c->p[1][0] = 0, c->p[1][1] = 0;
    }
    P->pond = 1;
    P->pond_r = n == 1 ? 0.34f : 0.26f;
}

/* the distance from (u, v) to the paths (a pond counts as its rim pushed out by w), how far along the nearest */
static float paths_dist(const paths *P, float u, float v, float w, float *along)
{
    float best = 1e9f, al = 0;
    for (int k = 0; k < P->nc; k++) {
        const curve *c = &P->c[k];
        float run = 0;
        for (int i = 0; i + 1 < c->n; i++) {
            float ax = c->p[i][0], ay = c->p[i][1], dx = c->p[i + 1][0] - ax, dy = c->p[i + 1][1] - ay;
            float l2 = dx * dx + dy * dy, l = sqrtf(l2), s = l2 > 0 ? clampf(((u - ax) * dx + (v - ay) * dy) / l2, 0, 1) : 0;
            float d = hypotf(u - ax - s * dx, v - ay - s * dy);
            if (d < best) { best = d; al = run + s * l + k * 0.37f; }
            run += l;
        }
    }
    if (P->pond) {
        float d = hypotf(u, v) - P->pond_r + w;
        if (d < best) { best = d; al = 0; }
    }
    if (along) *along = al;
    return best;
}

/* ---- a tile's ground ------------------------------------------------------------------------------------------- */
static const float GROUND[HXL_TERRAINS][3] = {
    {0, 0, 0}, {132, 184, 82}, {78, 124, 60}, {228, 192, 98}, {200, 182, 140}, {132, 184, 82}, {132, 184, 82},
};

static int dominant(uint32_t code)              /* the tile's main soft terrain: its middle */
{
    int best = HXL_MEADOW, bn = 0;
    for (int t = HXL_MEADOW; t <= HXL_VILLAGE; t++) {
        int n = hxl_has(code, t);
        if (n > bn) { bn = n; best = t; }
    }
    return best;
}

/* the soft terrain at (u, v): its sector's, the middle the main one's; *edge: how far (radius units) from a border */
static int terrain_at(uint32_t code, int dom, float u, float v, float *edge, int *other)
{
    float rho = hypotf(u, v), th = atan2f(v, u) + 0.10f * sinf(9 * rho + 1.3f);
    float s = (th + ART_PI / 6) / (ART_PI / 3), fl = floorf(s), fr = s - fl;
    int d = ((int)fl % 6 + 6) % 6, t = hxl_edge(code, d);
    int d2 = fr < 0.5f ? (d + 5) % 6 : (d + 1) % 6, t2 = hxl_edge(code, d2);
    if (hxl_strict(t)) t = HXL_MEADOW;
    if (hxl_strict(t2)) t2 = HXL_MEADOW;
    float e = fminf(fr, 1 - fr) * (ART_PI / 3) * rho;
    float rc = 0.26f + 0.04f * sinf(5 * th), dc = rho - rc;   /* the middle */
    if (dc < 0) {
        if (t != dom) { *edge = -dc; *other = t; } else { *edge = e; *other = t2; }
        return dom;
    }
    if (dc < e && t != dom) { *edge = dc; *other = dom; return t; }
    *edge = e;
    *other = t2;
    return t;
}

static void ground_col(int t, float u, float v, int x, int y, int d, float *c)
{
    const float *g = GROUND[t];
    float k = 1;
    switch (t) {
    case HXL_FOREST:
        k = 0.94f + 0.16f * art_noise(x / 2, y / 2) + 0.06f * art_noise(x, y);
        break;
    case HXL_FIELD: {                           /* rows of wheat */
        float a = (d * 60 + 90) * ART_PI / 180, s = sinf((u * cosf(a) + v * sinf(a)) * 34);
        k = 0.92f + 0.09f * s + 0.08f * art_noise(x, y);
        break;
    }
    case HXL_VILLAGE:
        k = 0.95f + 0.10f * art_noise(x / 2, y / 3) + 0.05f * art_noise(x, y);
        break;
    default:
        k = 0.97f + 0.10f * art_noise(x / 3, y / 3) + 0.06f * art_noise(x, y);
        break;
    }
    for (int i = 0; i < 3; i++) c[i] = g[i] * k;
    if (t == HXL_MEADOW) {                      /* little flowers */
        uint32_t h = hsh((uint32_t)(u * 40 + 400), (uint32_t)(v * 40 + 400));
        if ((h & 255) < 5) {
            float fl[3][3] = {{250, 248, 236}, {250, 220, 90}, {236, 150, 170}};
            int j = h >> 8 & 3;
            if (j < 3) for (int i = 0; i < 3; i++) c[i] = fl[j][i];
        }
    }
}

/* ---- a tile's things standing up ------------------------------------------------------------------------------- */
enum { OB_PINE, OB_TREE, OB_HOUSE, OB_STOOK, OB_BUSH, OB_STOP };
typedef struct { float u, v, s; int kind, var; } thing;

static void paint_thing(uint8_t *o, int W, int H, float x, float y, float rp, const thing *t)
{
    static const float trunk[3] = {98, 68, 44};
    float h = t->s * rp;
    switch (t->kind) {
    case OB_PINE: {
        static const float lit[3] = {84, 148, 74}, dark[3] = {44, 98, 54};
        shade_fill(o, W, H, x + 0.16f * h, y + 0.02f * h, 0.30f * h, 0.10f * h, 0.34f);
        rect_fill(o, W, H, x - 0.045f * h, y - 0.24f * h, x + 0.045f * h, y + 0.01f * h, trunk, 1);
        const float t1[] = {x - 0.32f * h, y - 0.16f * h, x + 0.32f * h, y - 0.16f * h, x, y - 0.72f * h};
        const float t2[] = {x - 0.25f * h, y - 0.46f * h, x + 0.25f * h, y - 0.46f * h, x, y - 1.02f * h};
        poly_fill(o, W, H, t1, 3, lit, dark, x, 1);
        poly_fill(o, W, H, t2, 3, lit, dark, x, 1);
        break;
    }
    case OB_TREE: {
        static const float crown[3] = {96, 156, 66};
        shade_fill(o, W, H, x + 0.18f * h, y + 0.02f * h, 0.32f * h, 0.11f * h, 0.34f);
        rect_fill(o, W, H, x - 0.05f * h, y - 0.36f * h, x + 0.05f * h, y + 0.01f * h, trunk, 1);
        ball_fill(o, W, H, x, y - 0.60f * h, 0.32f * h, crown, 1);
        break;
    }
    case OB_HOUSE: {
        static const float wall[3] = {242, 232, 212}, side[3] = {204, 190, 166}, door[3] = {112, 72, 46},
                           win[3] = {66, 76, 102};
        static const float roofs[3][2][3] = {{{196, 78, 54}, {150, 56, 40}}, {{176, 104, 62}, {132, 76, 46}},
                                              {{104, 112, 128}, {74, 80, 96}}};
        const float(*roof)[3] = roofs[t->var % 3];
        float s = h;
        shade_fill(o, W, H, x + 0.22f * s, y + 0.02f * s, 0.62f * s, 0.16f * s, 0.32f);
        rect_fill(o, W, H, x - 0.40f * s, y - 0.50f * s, x + 0.14f * s, y, wall, 1);
        rect_fill(o, W, H, x + 0.14f * s, y - 0.48f * s, x + 0.42f * s, y - 0.02f * s, side, 1);
        const float gable[] = {x - 0.42f * s, y - 0.49f * s, x + 0.16f * s, y - 0.49f * s, x - 0.13f * s, y - 0.84f * s};
        poly_fill(o, W, H, gable, 3, wall, wall, 1e9f, 1);
        const float rf[] = {x - 0.14f * s, y - 0.88f * s, x + 0.17f * s, y - 0.52f * s, x + 0.47f * s, y - 0.50f * s,
                            x + 0.16f * s, y - 0.86f * s};
        poly_fill(o, W, H, rf, 4, roof[0], roof[1], x + 0.1f * s, 1);
        const float eave[] = {x - 0.46f * s, y - 0.47f * s, x - 0.13f * s, y - 0.88f * s, x - 0.09f * s, y - 0.88f * s,
                              x - 0.40f * s, y - 0.47f * s};
        poly_fill(o, W, H, eave, 4, roof[1], roof[1], 1e9f, 1);
        rect_fill(o, W, H, x - 0.24f * s, y - 0.27f * s, x - 0.11f * s, y, door, 1);
        rect_fill(o, W, H, x - 0.04f * s, y - 0.36f * s, x + 0.07f * s, y - 0.24f * s, win, 1);
        break;
    }
    case OB_STOOK: {
        static const float lit[3] = {240, 206, 112}, dark[3] = {190, 150, 66};
        shade_fill(o, W, H, x + 0.1f * h, y, 0.16f * h, 0.05f * h, 0.3f);
        const float c[] = {x - 0.13f * h, y, x + 0.13f * h, y, x + 0.02f * h, y - 0.38f * h, x - 0.02f * h, y - 0.38f * h};
        poly_fill(o, W, H, c, 4, lit, dark, x, 1);
        break;
    }
    case OB_BUSH: {
        static const float b[3] = {88, 146, 64};
        shade_fill(o, W, H, x + 0.08f * h, y + 0.01f * h, 0.16f * h, 0.05f * h, 0.28f);
        ball_fill(o, W, H, x - 0.05f * h, y - 0.09f * h, 0.11f * h, b, 1);
        ball_fill(o, W, H, x + 0.07f * h, y - 0.08f * h, 0.10f * h, b, 1);
        break;
    }
    default: {                                  /* a buffer stop at a rail's end */
        static const float red[3] = {200, 64, 50}, dk[3] = {120, 40, 34};
        rect_fill(o, W, H, x - 0.16f * h, y - 0.20f * h, x + 0.16f * h, y, red, 1);
        rect_fill(o, W, H, x - 0.16f * h, y - 0.20f * h, x + 0.16f * h, y - 0.15f * h, dk, 1);
        break;
    }
    }
}

/* a tile painted over a picture W x H, its middle at (ax, ay), rp pixels its radius: the side, the ground, its river
 * and rails, then the things standing on it from the back */
static void paint_tile(uint8_t *o, int W, int H, float ax, float ay, float rp, uint32_t code)
{
    float dp = THICK * rp, sy = rp * TILT;
    int x0 = (int)floorf(ax - AP * rp - 1), x1 = (int)ceilf(ax + AP * rp + 1);
    int y0 = (int)floorf(ay - sy - 1), y1 = (int)ceilf(ay + sy + dp + 1);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    paths river, rail;
    make_paths(code, HXL_RIVER, &river);
    make_paths(code, HXL_RAIL, &rail);
    int dom = code == BACK ? 0 : dominant(code);
    const float WR = 0.15f, BANK = 0.045f, RB = 0.13f;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float u = (x + 0.5f - ax) / rp, v = (y + 0.5f - ay) / sy, au = fabsf(u);
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            if (au < AP + 1 / rp) {             /* the side: earth under a grass lip (the back: walnut) */
                float vb = 1 - au / SQ3, dv = (v - vb) * sy;
                if (dv > -2 && dv < dp + 1) {
                    float cov = clampf((AP - au) * rp + 0.5f, 0, 1) * clampf(dp - dv + 0.5f, 0, 1);
                    float k = (u < 0 ? 1.0f : 0.80f) * (1 - 0.28f * clampf(dv / dp, 0, 1));
                    float c[3] = {126, 94, 62};
                    if (code == BACK) c[0] = 108, c[1] = 70, c[2] = 44;
                    else if (dv < 0.2f * dp) c[0] = 86, c[1] = 132, c[2] = 58;
                    else k *= 0.95f + 0.1f * art_noise(x / 2, (int)(dv * 3));
                    px_over(p, c[0] * k, c[1] * k, c[2] * k, cov);
                }
            }
            float sd = hexsdf(u, v), cov = clampf(0.5f - sd * rp * 0.9f, 0, 1);
            if (cov <= 0) continue;
            float c[3];
            if (code == BACK) {                 /* the back: plain wood */
                float g = 0.92f + 0.08f * sinf(u * 26 + 3 * sinf(v * 7)) + 0.06f * art_noise(x, y / 3);
                c[0] = 196 * g, c[1] = 150 * g, c[2] = 102 * g;
            } else {
                float e;
                int other, t = terrain_at(code, dom, u, v, &e, &other), d;
                float th = atan2f(v, u);
                d = ((int)floorf((th + ART_PI / 6) / (ART_PI / 3)) % 6 + 6) % 6;
                ground_col(t, u, v, x, y, d, c);
                float epx = e * rp;
                if (other != t && epx < 1.2f) {  /* a soft border between two terrains */
                    float c2[3], w = 0.5f - 0.5f * epx / 1.2f;
                    ground_col(other, u, v, x, y, d, c2);
                    for (int i = 0; i < 3; i++) c[i] += (c2[i] - c[i]) * w;
                }
                if (river.ends) {               /* the river, its banks, its shimmer */
                    float d0 = paths_dist(&river, u, v, WR, NULL);
                    if (d0 < WR + BANK + 2 / rp) {
                        static const float bank[3] = {150, 162, 96};
                        float bk = clampf((WR + BANK - d0) * rp + 0.5f, 0, 1);
                        for (int i = 0; i < 3; i++) c[i] += (bank[i] - c[i]) * bk;
                        float wa = clampf((WR - d0) * rp + 0.5f, 0, 1);
                        if (wa > 0) {
                            float depth = clampf(1 - d0 / WR, 0, 1);
                            float wc[3] = {104 - 44 * depth, 172 - 46 * depth, 216 - 24 * depth};
                            float sh = sinf(u * 38 + v * 19 + 2.2f * sinf(v * 15 + u * 4));
                            if (sh > 0.8f) for (int i = 0; i < 3; i++) wc[i] += (255 - wc[i]) * (sh - 0.8f) * 2.2f;
                            for (int i = 0; i < 3; i++) c[i] += (wc[i] - c[i]) * wa;
                        }
                    }
                }
                if (rail.ends) {                /* the rails: gravel, sleepers, two steel lines */
                    float al, d0 = paths_dist(&rail, u, v, 0.0f, &al);
                    if (rail.pond) d0 = fminf(d0, paths_dist(&rail, u, v, 1e3f, &al) + 1e3f);
                    if (d0 < RB + 2 / rp) {
                        float g = clampf((RB - d0) * rp + 0.5f, 0, 1), n = 0.9f + 0.2f * art_noise(x, y);
                        float gc[3] = {164 * n, 152 * n, 132 * n};
                        float sl = fmodf(al + 10, 0.115f);
                        if (sl < 0.05f && d0 < 0.105f) gc[0] = 112, gc[1] = 80, gc[2] = 56;
                        float rd = fabsf(d0 - 0.058f), ra = clampf((0.016f - rd) * rp + 0.5f, 0, 1);
                        gc[0] += (214 - gc[0]) * ra, gc[1] += (214 - gc[1]) * ra, gc[2] += (222 - gc[2]) * ra;
                        for (int i = 0; i < 3; i++) c[i] += (gc[i] - c[i]) * g;
                    }
                }
            }
            if (sd > -0.08f) {                  /* the rim: lit at the top left, shaded at the bottom */
                float k = (sd + 0.08f) / 0.08f, l = -(0.45f * u + 0.9f * v);
                float f = 1 + 0.16f * k * clampf(l, -1, 1);
                for (int i = 0; i < 3; i++) c[i] *= f;
            }
            px_over(p, c[0], c[1], c[2], cov);
        }
    if (code == BACK) return;
    /* the things on it, from the back */
    thing th[80];
    int n = 0;
    static const struct { int t; float step, keep, size; } KIND[] = {
        {HXL_FOREST, 0.23f, 0.86f, 0.30f}, {HXL_VILLAGE, 0.36f, 0.85f, 0.27f},
        {HXL_FIELD, 0.40f, 0.30f, 0.30f}, {HXL_MEADOW, 0.44f, 0.16f, 0.30f},
    };
    for (int k = 0; k < 4; k++) {
        if (!hxl_has(code, KIND[k].t) && dom != KIND[k].t) continue;
        float st = KIND[k].step;
        for (int j = -6; j <= 6; j++)
            for (int i = -6; i <= 6; i++) {
                uint32_t hh = hsh(code * 7u + (uint32_t)k, (uint32_t)((j + 8) * 32 + i + 8));
                float u = (i + 0.5f * (j & 1)) * st + ((hh & 255) / 255.0f - 0.5f) * 0.5f * st;
                float v = j * st * 0.87f + ((hh >> 8 & 255) / 255.0f - 0.5f) * 0.5f * st;
                if (hexsdf(u, v) > -0.13f - 0.05f * (k == 1) || (hh >> 16 & 255) / 255.0f > KIND[k].keep) continue;
                float e;
                int other;
                if (terrain_at(code, dom, u, v, &e, &other) != KIND[k].t || e < 0.05f) continue;
                if (river.ends && paths_dist(&river, u, v, 0.15f, NULL) < 0.15f + 0.09f) continue;
                if (rail.ends && paths_dist(&rail, u, v, 0, NULL) < 0.13f + 0.08f) continue;
                if (n >= 80) continue;
                thing *t = &th[n++];
                t->u = u, t->v = v;
                t->var = (int)(hh >> 24);
                t->s = KIND[k].size * (0.86f + 0.28f * hf(hh, 3));
                t->kind = k == 0 ? (t->var & 3 ? OB_PINE : OB_TREE) : k == 1 ? OB_HOUSE : k == 2 ? OB_STOOK : OB_BUSH;
            }
    }
    if (rail.ends == 1 && n < 80) th[n++] = (thing){0, 0.02f, 0.3f, OB_STOP, 0};
    for (int i = 1; i < n; i++)                 /* the back ones first */
        for (int j = i; j > 0 && th[j - 1].v > th[j].v; j--) { thing t = th[j]; th[j] = th[j - 1]; th[j - 1] = t; }
    for (int i = 0; i < n; i++) paint_thing(o, W, H, ax + th[i].u * rp, ay + th[i].v * sy, rp, &th[i]);
}

/* a tile's picture: its size for a radius, its middle in it */
static void tile_box(float rp, int *w, int *h, float *ax, float *ay)
{
    *w = (int)ceilf(SQ3 * rp) + 4;
    *h = (int)ceilf((OVER + 2 * TILT + THICK) * rp) + 4;
    *ax = *w / 2.0f;
    *ay = 2 + (OVER + TILT) * rp;
}

/* ---- the pictures kept: each tile at a few sizes ----------------------------------------------------------------- */
typedef struct { uint32_t code; int b; SDL_Texture *t; int w, h; float ax, ay, rp; Uint32 used; } kept;
#define KEPT 480
static kept K[KEPT];
static Uint32 frame_no;
static int painted;

static int bucket_of(float rp) { return (int)lroundf(logf(fmaxf(rp, 4)) / logf(1.15f)); }

static kept *paint_kept(uint32_t code, int b)
{
    int slot = 0;
    for (int i = 0; i < KEPT; i++) {            /* a free place, else the one used longest ago */
        if (!K[i].t) { slot = i; break; }
        if (K[i].used < K[slot].used) slot = i;
    }
    kept *k = &K[slot];
    ui_free(&k->t);
    k->code = code;
    k->b = b;
    k->rp = powf(1.15f, (float)b);
    tile_box(k->rp, &k->w, &k->h, &k->ax, &k->ay);
    uint8_t *o = ui_buf(k->w, k->h);
    memset(o, 0, (size_t)k->w * k->h * 4);
    paint_tile(o, k->w, k->h, k->ax, k->ay, k->rp, code);
    k->t = ui_tex(k->w, k->h, o);
    k->used = frame_no;
    painted++;
    return k;
}

/* the picture to draw a tile at radius rp: kept at the nearest size; a few new ones a frame, the others drawn from a
 * size near enough until their turn */
static kept *tile_pic(uint32_t code, float rp)
{
    int b = bucket_of(rp);
    kept *near = NULL;
    int nd = 99;
    for (int i = 0; i < KEPT; i++) {
        if (!K[i].t || K[i].code != code) continue;
        if (K[i].b == b) { K[i].used = frame_no; return &K[i]; }
        if (abs(K[i].b - b) < nd) { nd = abs(K[i].b - b); near = &K[i]; }
    }
    if (near && (painted >= 4 || nd <= 1)) {
        near->used = frame_no;
        if (nd > 1 || painted < 4) ui_animate();
        if (painted < 4) return paint_kept(code, b);
        return near;
    }
    return paint_kept(code, b);
}

static void put_tile(uint32_t code, float sx, float sy, float rp, Uint8 alpha, int red)
{
    kept *k = tile_pic(code, rp);
    if (!k || !k->t) return;
    float z = rp / k->rp;
    SDL_SetTextureAlphaMod(k->t, alpha);
    if (red) SDL_SetTextureColorMod(k->t, 255, 160, 150);
    ui_put(k->t, sx - k->ax * z, sy - k->ay * z, k->w * z, k->h * z);
    SDL_SetTextureAlphaMod(k->t, 255);
    SDL_SetTextureColorMod(k->t, 255, 255, 255);
}

/* the flat pictures: a tile's shadow, an empty place's outline, a tick and a cross, a puff, a banner */
static SDL_Texture *t_shadow, *t_ring, *t_tick, *t_cross, *t_puff;
static int made_flat = -1, made_mark = -1;

static void make_flat(float rp)
{
    int b = bucket_of(rp);
    if (b == made_flat) return;
    made_flat = b;
    float r = powf(1.15f, (float)b);
    int w = (int)ceilf(SQ3 * r * 1.3f) + 4, h = (int)ceilf(2 * TILT * r * 1.3f) + 4;
    uint8_t *o = ui_buf(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float u = (x + 0.5f - w / 2.0f) / r, v = (y + 0.5f - h / 2.0f) / (r * TILT), sd = hexsdf(u, v);
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            p[0] = 16, p[1] = 22, p[2] = 10;
            p[3] = (uint8_t)(110 * smooth(0.22f, -0.12f, sd));
        }
    ui_free(&t_shadow);
    t_shadow = ui_tex(w, h, o);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float u = (x + 0.5f - w / 2.0f) / r, v = (y + 0.5f - h / 2.0f) / (r * TILT), sd = hexsdf(u, v);
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            float lw = fmaxf(0.03f, 1.6f / r), ring = clampf((lw - fabsf(sd + 0.07f)) * r + 0.5f, 0, 1);
            float in = sd < -0.07f ? 0.16f : 0;
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(255 * fmaxf(ring, in));
        }
    ui_free(&t_ring);
    t_ring = ui_tex(w, h, o);
}

static void stroke(uint8_t *o, int s, float ax, float ay, float bx, float by, float r, float a)
{
    static const uint8_t white[3] = {255, 255, 255};
    pzk_stroke(o, s, s, ax, ay, bx, by, r, white, a);
}

static void make_marks(int d)
{
    if (d == made_mark || d < 4) return;
    made_mark = d;
    for (int k = 0; k < 2; k++) {
        uint8_t *o = ui_buf(d, d);
        memset(o, 0, (size_t)d * d * 4);
        static const float col[2][3] = {{64, 156, 80}, {204, 70, 58}};
        float c = d / 2.0f, r = 0.46f * d;
        for (int y = 0; y < d; y++)
            for (int x = 0; x < d; x++) {
                float dx = x + 0.5f - c, dy = y + 0.5f - c, dd = hypotf(dx, dy);
                float cov = clampf(r - dd + 0.5f, 0, 1), rim = clampf(dd - (r - 0.12f * d) + 0.5f, 0, 1);
                float l = 1 + 0.2f * (-dx - dy) / r;
                uint8_t *p = o + 4 * ((size_t)y * d + x);
                float w = 0.8f * rim;
                px_over(p, (col[k][0] * l) * (1 - w) + 255 * w, (col[k][1] * l) * (1 - w) + 255 * w,
                        (col[k][2] * l) * (1 - w) + 255 * w, cov);
            }
        float lw = 0.075f * d;
        if (k == 0) {
            stroke(o, d, 0.28f * d, 0.52f * d, 0.44f * d, 0.68f * d, lw, 1);
            stroke(o, d, 0.44f * d, 0.68f * d, 0.73f * d, 0.34f * d, lw, 1);
        } else {
            stroke(o, d, 0.33f * d, 0.33f * d, 0.67f * d, 0.67f * d, lw, 1);
            stroke(o, d, 0.67f * d, 0.33f * d, 0.33f * d, 0.67f * d, lw, 1);
        }
        SDL_Texture **t = k ? &t_cross : &t_tick;
        ui_free(t);
        *t = ui_tex(d, d, o);
    }
    if (!t_puff) {
        static const uint8_t cream[3] = {250, 246, 236};
        uint8_t *o = ui_buf(64, 64);
        art_disc(64, cream, 0.9f, 1.6f, o);
        t_puff = ui_tex(64, 64, o);
    }
}

/* a quest's banner: its terrain's colour (gold done, grey failed), w x h, rounded */
static const uint8_t BANNER[HXL_TERRAINS][3] = {
    {0, 0, 0}, {104, 156, 66}, {50, 112, 60}, {202, 156, 48}, {182, 80, 56}, {58, 120, 190}, {110, 110, 120},
};
typedef struct { int terr, state, w, h; SDL_Texture *t; } banner_t;
static banner_t BN[24];
static int nbn;

static SDL_Texture *banner(int terr, int state, int w, int h)
{
    for (int i = 0; i < nbn; i++)
        if (BN[i].terr == terr && BN[i].state == state && BN[i].w == w && BN[i].h == h) return BN[i].t;
    int i = nbn < 24 ? nbn++ : (int)(frame_no % 24);
    ui_free(&BN[i].t);
    BN[i] = (banner_t){terr, state, w, h, NULL};
    uint8_t c[4] = {BANNER[terr][0], BANNER[terr][1], BANNER[terr][2], 255};
    if (state == HXL_DONE) c[0] = 222, c[1] = 172, c[2] = 54;
    if (state == HXL_FAILED) c[0] = 132, c[1] = 128, c[2] = 122;
    uint8_t *o = ui_buf(w, h);
    art_roundrect(w, h, 0.22f * h, c, o);
    for (int y = 0; y < h; y++)                 /* lit at the top, a pale rim */
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            float k = 1.12f - 0.24f * y / h;
            for (int j = 0; j < 3; j++) p[j] = (uint8_t)clampf(p[j] * k, 0, 255);
        }
    BN[i].t = ui_tex(w, h, o);
    return BN[i].t;
}

/* ---- the camera -------------------------------------------------------------------------------------------------- */
static void world_of(int c, float *wx, float *wy)
{
    int q, r;
    hxl_qr(c, &q, &r);
    *wx = SQ3 * (q + 0.5f * r);
    *wy = 1.5f * r;
}

static void to_screen(float wx, float wy, float *sx, float *sy)
{
    *sx = view.x + view.w / 2 + (wx - cam_x) * cam_s;
    *sy = view.y + view.h / 2 + (wy - cam_y) * cam_s * TILT;
}

static void cell_screen(int c, float *sx, float *sy)
{
    float wx, wy;
    world_of(c, &wx, &wy);
    to_screen(wx, wy, sx, sy);
}

static int pick(float x, float y)
{
    float wx = (x - view.x - view.w / 2) / cam_s + cam_x, wy = (y - view.y - view.h / 2) / (cam_s * TILT) + cam_y;
    float fr = wy / 1.5f, fq = wx / SQ3 - fr / 2, fs = -fq - fr;
    int q = (int)lroundf(fq), r = (int)lroundf(fr), s = (int)lroundf(fs);
    float dq = fabsf(q - fq), dr = fabsf(r - fr), ds = fabsf(s - fs);
    if (dq > dr && dq > ds) q = -r - s;
    else if (dr > ds) r = -q - s;
    return hxl_cell(q, r);
}

static float s_comfort(void) { return 0.09f * fminf(ui.w, ui.h); }
static float s_most(void) { return 0.26f * fminf(view.w, view.h); }
static float s_least(void) { return fmaxf(10, 0.022f * fminf(ui.w, ui.h)); }

static void cam_at(Uint32 now)
{
    if (!cam_go) return;
    float f = (float)(int)(now - cam_t0) / CAM_MS;
    if (f >= 1) {
        cam_x = cam_tx, cam_y = cam_ty, cam_s = cam_ts;
        cam_go = 0;
        return;
    }
    float e = ui_ease(clampf(f, 0, 1));
    cam_x = cam_ex + (cam_tx - cam_ex) * e;
    cam_y = cam_ey + (cam_ty - cam_ey) * e;
    cam_s = cam_es * powf(cam_ts / cam_es, e);
    ui_animate();
}

static void cam_set(float x, float y, float s)
{
    cam_x = cam_tx = x, cam_y = cam_ty = y;
    cam_s = cam_ts = clampf(s, s_least(), s_most());
    cam_go = 0;
}

static void cam_to(float x, float y, float s)
{
    Uint32 now = ui_ms();
    cam_at(now);
    cam_ex = cam_x, cam_ey = cam_y, cam_es = cam_s;
    cam_tx = x, cam_ty = y, cam_ts = clampf(s, s_least(), s_most());
    cam_t0 = now;
    cam_go = 1;
    ui_animate();
}

/* the map and the places beside it, in world units */
static void map_bounds(float *x0, float *y0, float *x1, float *y1)
{
    *x0 = *y0 = 1e9f, *x1 = *y1 = -1e9f;
    for (int c = 0; c < HXL_CELLS; c++)
        if (G.s.cell[c]) {
            float wx, wy;
            world_of(c, &wx, &wy);
            *x0 = fminf(*x0, wx), *x1 = fmaxf(*x1, wx), *y0 = fminf(*y0, wy), *y1 = fmaxf(*y1, wy);
        }
    if (*x0 > *x1) *x0 = *x1 = *y0 = *y1 = 0;
    *x0 -= 2.1f, *x1 += 2.1f, *y0 -= 2.4f, *y1 += 2.5f;   /* a ring of places round it, room for the trees */
}

/* the camera's own aim: the whole map when it fits at a comfortable size, else that size round the last tile */
static void cam_auto(int at_once)
{
    int whole = hxl_over(&G.s);
    if (view.w <= 0) return;
    float x0, y0, x1, y1;
    map_bounds(&x0, &y0, &x1, &y1);
    float fit = fminf(view.w / (x1 - x0), view.h / ((y1 - y0) * TILT));
    float s = fminf(fit, 0.16f * fminf(view.w, view.h)), x = (x0 + x1) / 2, y = (y0 + y1) / 2;
    if (s < s_comfort() && !whole) {
        s = fminf(s_comfort(), s_most());
        float lx = x, ly = y;
        if (G.s.last >= 0) world_of(G.s.last, &lx, &ly);
        float hw = view.w / 2 / s - 1.6f, hh = view.h / 2 / (s * TILT) - 1.6f;   /* the map's middle, kept */
        x = clampf(x, lx - fmaxf(hw, 0), lx + fmaxf(hw, 0));       /* as near as the last tile lets */
        y = clampf(y, ly - fmaxf(hh, 0), ly + fmaxf(hh, 0));
    }
    if (at_once) cam_set(x, y, s);
    else cam_to(x, y, s);
}

/* the camera moved, if it must, to show cell c and the places round it */
static void cam_show(int c)
{
    if (c < 0) return;
    float wx, wy, s = cam_go ? cam_ts : cam_s, x = cam_go ? cam_tx : cam_x, y = cam_go ? cam_ty : cam_y;
    world_of(c, &wx, &wy);
    float hw = view.w / 2 / s - 1.4f, hh = view.h / 2 / (s * TILT) - 1.5f;
    float nx = clampf(x, wx - fmaxf(hw, 0), wx + fmaxf(hw, 0)), ny = clampf(y, wy - fmaxf(hh, 0), wy + fmaxf(hh, 0));
    if (nx != x || ny != y) cam_to(nx, ny, s);
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    room = b;
    float u = ui.unit;
    if (b.h >= b.w) {                           /* upright: the tray under the map */
        float th = clampf(0.2f * b.h, 2.3f * u, 3.4f * u);
        tray = (box){b.x, b.y + b.h - th, b.w, th};
        view = (box){b.x, b.y, b.w, b.h - th - 0.15f * u};
        tr_r = fminf(0.36f * th, 0.16f * b.w);
        tr_cx = tray.x + 0.42f * tray.w;
        tr_cy = tray.y + 0.47f * th;
        nx_r = 0.58f * tr_r;
        nx_x[0] = tray.x + 0.68f * tray.w, nx_x[1] = tray.x + 0.86f * tray.w;
        nx_y[0] = nx_y[1] = tr_cy + 0.12f * tr_r;
        cnt_x = tray.x + 0.15f * tray.w, cnt_y = tr_cy;
    } else {                                    /* wide: the tray on the map's right */
        float tw = clampf(0.22f * b.w, 2.6f * u, 3.6f * u);
        tray = (box){b.x + b.w - tw, b.y, tw, b.h};
        view = (box){b.x, b.y, b.w - tw - 0.15f * u, b.h};
        tr_r = fminf(0.34f * tw, 0.13f * b.h);
        tr_cx = tray.x + tw / 2;
        tr_cy = tray.y + 0.40f * b.h;
        nx_r = 0.58f * tr_r;
        nx_x[0] = tray.x + 0.30f * tw, nx_x[1] = tray.x + 0.72f * tw;
        nx_y[0] = nx_y[1] = tray.y + 0.72f * b.h;
        cnt_x = tr_cx, cnt_y = tray.y + 0.14f * b.h;
    }
    int w = (int)tray.w, h = (int)tray.h;
    if (w != made_tray_w || h != made_tray_h) {
        made_tray_w = w, made_tray_h = h;
        int f = (int)fmaxf(3, 0.22f * u);
        uint8_t *o = malloc((size_t)w * h * 4);
        ui_free(&t_tray);
        if (o && w > 2 * f && h > 2 * f) {
            memset(o, 0, (size_t)w * h * 4);
            art_plank(w - 2 * f, h - 2 * f, f, o);
            puz_bevel(o, w, h, 0.12f * u, 0.06f * u, 0.25f);
            t_tray = ui_tex(w, h, o);
        }
        free(o);
    }
    int small = (int)(0.6f * u);
    if (small != made_small && small > 2) {
        made_small = small;
        ui_free(&t_par);
        ui_free(&t_cup);
        uint8_t *o = ui_buf(small, small);
        memset(o, 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, o);
        t_par = ui_tex(small, small, o);
        memset(o, 0, (size_t)small * small * 4);
        art_icon(ICON_CUP, small, o);
        t_cup = ui_tex(small, small, o);
    }
    if (!user_cam) cam_auto(1);
    else cam_set(cam_x, cam_y, cam_s);
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    nfx = 0;
    drop_cell = -1;
    ghost = hint_cell = tool_down = -1;
    rot = 0;
    rot_prev = -1;
    win_on = 0;
    calm_until = 0;
}

static void best_load(void)
{
    uint8_t b[4 * HXL_VARIANTS] = {0};
    ui_save_get("hexland/best", b, sizeof b);
    int v = variant_now;
    best_now = b[4 * v] | b[4 * v + 1] << 8 | b[4 * v + 2] << 16 | b[4 * v + 3] << 24;
}

static void begin(void)
{
    serial++;
    calm();
    user_cam = 0;
    best_load();
    if (view.w > 0) cam_auto(1);
}

static void start(int level)
{
    if (!hxl_level(&G, level)) hxl_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    variant_now = variant < 0 || variant >= HXL_VARIANTS ? 0 : variant;
    hxl_random(&G, variant_now, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int dropping(Uint32 now) { return drop_cell >= 0 && (int)(now - drop_t0) < DROP_MS; }
static int status(void) { return hxl_over(&G.s) && !dropping(ui_ms()) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.s.score; }
static int par(void) { return G.goal; }
static int level_stars(void) { return hxl_stars(G.s.score, G.goal); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return dropping(now) || (win_on && (int)(now - win_t0) < WIN_MS);
}

static void fx_add(int kind, int cell, int value, Uint32 t0)
{
    if (nfx >= MAX_FX) memmove(FX, FX + 1, sizeof FX[0] * --nfx);
    FX[nfx++] = (fxe){kind, cell, value, t0};
}

/* the turn that suits the current tile best at c: the most points among those allowed, from the one shown */
static int best_turn(int c, int from)
{
    const hxl_item *it = hxl_current(&G.s);
    if (!it) return from;
    int best = -1, bv = -1;
    for (int j = 0; j < 6; j++) {
        int k = (from + j) % 6;
        uint32_t t = hxl_turn(it->tile, k);
        if (!hxl_can(&G.s, c, t)) continue;
        int v = hxl_gain(&G.s, c, t, NULL, NULL);
        if (v > bv) { bv = v; best = k; }
    }
    return best < 0 ? from : best;
}

static void set_rot(int k)
{
    if (k == rot) return;
    rot_prev = rot;
    rot = k;
    rot_t0 = ui_ms();
    ui_animate();
}

static void turn_tile(void)
{
    const hxl_item *it = hxl_current(&G.s);
    if (!it) { ui_sound(SND_DENY); return; }
    int k = rot;
    for (int j = 1; j <= 6; j++)                /* the next turn that looks different */
        if (hxl_turn(it->tile, (rot + j) % 6) != hxl_turn(it->tile, rot) || j == 6) { k = (rot + j) % 6; break; }
    set_rot(k);
    hint_cell = -1;
    ui_sound(SND_TAP);
}

static void choose(int c)
{
    if (c == ghost) { turn_tile(); return; }
    ghost = c;
    hint_cell = -1;
    set_rot(best_turn(c, rot));
    ui_sound(SND_TAP);
    cam_show(c);
    ui_animate();
}

static void best_save(void)
{
    if (ui.level >= 0 || G.s.score <= best_now) return;
    uint8_t b[4 * HXL_VARIANTS] = {0};
    ui_save_get("hexland/best", b, sizeof b);
    int v = variant_now, s = G.s.score;
    for (int i = 0; i < 4; i++) b[4 * v + i] = (uint8_t)(s >> 8 * i);
    ui_save_set("hexland/best", b, sizeof b);
}

static void place(void)
{
    Uint32 now = ui_ms();
    const hxl_item *it = hxl_current(&G.s);
    if (!it || ghost < 0 || (int)(now - calm_until) < 0 || !hxl_can(&G.s, ghost, hxl_turn(it->tile, rot))) {
        ui_sound(SND_DENY);
        return;
    }
    hxl_result r;
    int c = ghost;
    if (!hxl_play(&G, c, rot, &r)) { ui_sound(SND_DENY); return; }
    drop_cell = c;
    drop_t0 = now;
    calm_until = now + DROP_MS / 2;
    Uint32 land = now + DROP_MS * 7 / 10;
    ui_sound(SND_MOVE);
    ui_sound_later(SND_PLACE, DROP_MS * 7 / 10);
    fx_add(FX_PUFF, c, 0, land);
    if (r.points) fx_add(FX_POP, c, r.points, land + 60);
    if (r.perfect) {
        fx_add(FX_STARS, c, 0, land + 80);
        ui_sound_later(SND_END, DROP_MS * 7 / 10 + 120);
    }
    for (int i = 0; i < r.ndone; i++) {
        fx_add(FX_FLAG, G.s.q[r.done[i]].cell, r.done[i], land + 100);
        fx_add(FX_STARS, G.s.q[r.done[i]].cell, 0, land + 160);
    }
    if (r.ndone && !r.perfect) ui_sound_later(SND_END, DROP_MS * 7 / 10 + 160);
    for (int i = 0; i < r.nfail; i++) fx_add(FX_FLAG, G.s.q[r.fail[i]].cell, r.fail[i], land + 100);
    for (int i = 0; i < r.nforest; i++) fx_add(FX_BIRDS, r.forest[i], i, land + 150 + 120 * i);
    for (int i = 0; i < r.bonus; i++) fx_add(FX_FLY, c, i, land + 300 + 140 * i);
    if (r.skipped) fx_add(FX_SKIP, -1, r.skipped, land + 200);
    ghost = hint_cell = -1;
    rot = 0;
    rot_prev = -1;
    if (hxl_over(&G.s)) {
        win_on = 1;
        win_t0 = land + 400;
        best_save();
        cam_auto(0);
    } else if (!user_cam) cam_auto(0);
    else cam_show(c);
    ui_animate();
}

static void hint(void)
{
    int c, k;
    if (hxl_over(&G.s) || !hxl_hint(&G.s, &c, &k)) { ui_sound(SND_DENY); return; }
    ghost = c;
    set_rot(k);
    hint_cell = c;
    hint_t0 = ui_ms();
    cam_show(c);
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    if (!hxl_undo(&G)) return 0;
    calm();
    return 1;
}

/* the game's notation (hexland.h), and "g<q>,<r>,<k>": the tile shown hovering there (the pictures) */
static int script(const char *moves)
{
    calm();
    char buf[64];
    const char *s = moves;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        const char *e = s;
        while (*e && *e != ' ') e++;
        int n = (int)(e - s) < 63 ? (int)(e - s) : 63, q, r, k;
        memcpy(buf, s, (size_t)n);
        buf[n] = 0;
        if (buf[0] == 'g' && sscanf(buf + 1, "%d,%d,%d", &q, &r, &k) == 3) {
            ghost = hxl_cell(q, r);
            rot = ((k % 6) + 6) % 6;
        } else if (!hxl_script(&G, buf)) return 0;
        s = e;
    }
    if (hxl_over(&G.s)) { win_on = 1; win_t0 = ui_ms(); }
    if (view.w > 0) cam_auto(1);
    if (ghost >= 0) cam_show(ghost), cam_at(ui_ms() + CAM_MS);
    return 1;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_flag(const hxl_quest *q, float sx, float sy, float s, float pop, Uint8 alpha)
{
    float bh = fmaxf(0.4f * ui.unit, 0.46f * s), dh = 0.62f * bh, nw = ui_number_w(q->n, dh), plus = q->kind == HXL_ATLEAST;
    float bw = nw + 0.55f * bh + (plus ? 0.5f * dh : 0), px = sx + 0.18f * s, top = sy - 0.62f * s - bh;
    pzk_rgba pole = {70, 52, 36, alpha};
    pzk_line(px, sy - 0.05f * s, px, top - 0.02f * s, fmaxf(1.5f, 0.045f * s), 1, pole, 0);
    float k = 1 + 0.35f * pop;
    float w = bw * k, h = bh * k, bx = px, by = top - (h - bh) / 2;
    SDL_Texture *t = banner(q->terr, q->state, (int)w, (int)h);
    if (t) {
        SDL_SetTextureAlphaMod(t, alpha);
        ui_put(t, bx, by, (int)w, (int)h);
        SDL_SetTextureAlphaMod(t, 255);
    }
    static const uint8_t cream[3] = {255, 250, 238};
    float dx = bx + 0.28f * h + nw * k / 2, dy = by + 0.44f * h;
    ui_number_ex(dx, dy, dh * k, q->n, cream, alpha, 0);
    if (plus) {                                 /* "at least": a plus after it */
        float cx = dx + nw * k / 2 + 0.32f * dh * k, l = 0.2f * dh * k, t2 = fmaxf(1, 0.07f * dh * k);
        ui_fill(cx - l, dy - t2, 2 * l, 2 * t2, 255, 250, 238, alpha);
        ui_fill(cx - t2, dy - l, 2 * t2, 2 * l, 255, 250, 238, alpha);
    }
    if (q->state == HXL_OPEN) {                 /* how far it has come: a thin bar under the digits */
        float f = clampf((float)q->size / q->n, 0, 1), bx0 = bx + 0.2f * h, bw0 = w - 0.4f * h, y = by + 0.8f * h;
        ui_fill(bx0, y, bw0, fmaxf(1, 0.07f * h), 0, 0, 0, (Uint8)(alpha * 0.35f));
        ui_fill(bx0, y, bw0 * fminf(f, 1), fmaxf(1, 0.07f * h), 255, 250, 238, alpha);
    }
}

/* a quest flag for an item still in the stack (the tray), above the tile drawn at (sx, sy) */
static void draw_item_flag(const hxl_item *it, float sx, float sy, float s)
{
    if (!it->qn) return;
    hxl_quest q = {0, it->qterr, it->qkind, it->qn, HXL_OPEN, 0};
    draw_flag(&q, sx, sy, s, 0, 255);
    (void)q.cell;
}

static int quest_shown(int i, Uint32 now, float *pop, Uint8 *alpha)
{
    const hxl_quest *q = &G.s.q[i];
    *pop = 0;
    *alpha = 255;
    for (int j = 0; j < nfx; j++)
        if (FX[j].kind == FX_FLAG && FX[j].value == i) {
            int a = (int)(now - FX[j].t0);
            if (a < 0) {                        /* not yet: as it was */
                if (q->state != HXL_OPEN) *alpha = 255;
                return 1;
            }
            float f = a / (float)FX_MS[FX_FLAG];
            *pop = sinf(ART_PI * clampf(f, 0, 1));
        }
    if (q->state == HXL_FAILED) {
        int left = 0;
        for (int j = 0; j < nfx; j++)
            if (FX[j].kind == FX_FLAG && FX[j].value == i) left = 1;
        if (!left) return 0;
        *alpha = 170;
    }
    return 1;
}

static void draw_bird(float x, float y, float l, float flap, Uint8 a)
{
    pzk_rgba ink = {44, 40, 36, a};
    float w = 0.55f + 0.45f * sinf(flap), dx = l * cosf(w * 0.9f), dy = l * sinf(w * 0.9f);
    pzk_line(x - dx, y - dy, x, y, fmaxf(1.2f, 0.12f * l), 1, ink, 0);
    pzk_line(x, y, x + dx, y - dy, fmaxf(1.2f, 0.12f * l), 1, ink, 0);
}

static void draw_fx(Uint32 now)
{
    int keep = 0;
    float s = cam_s;
    for (int i = 0; i < nfx; i++) {
        fxe *e = &FX[i];
        int age = (int)(now - e->t0);
        if (age > FX_MS[e->kind]) continue;
        FX[keep++] = *e;
        ui_animate();
        if (age < 0) continue;
        float f = age / (float)FX_MS[e->kind], sx = 0, sy = 0;
        if (e->cell >= 0) cell_screen(e->cell, &sx, &sy);
        switch (e->kind) {
        case FX_PUFF:                           /* soft dust round the tile's foot */
            for (int k = 0; k < 9; k++) {
                float a = k * 40 * ART_PI / 180 + 0.3f, r = s * (0.78f + 0.5f * ui_ease(f));
                float px = sx + cosf(a) * r, py = sy + sinf(a) * r * TILT + THICK * s * 0.6f - 0.15f * s * f;
                float d = s * (0.22f + 0.28f * f);
                SDL_SetTextureAlphaMod(t_puff, (Uint8)(170 * (1 - f) * (1 - f)));
                ui_put_c(t_puff, px, py, d, d * 0.8f);
            }
            SDL_SetTextureAlphaMod(t_puff, 255);
            break;
        case FX_POP: {                          /* the points rising */
            static const uint8_t gold[3] = {255, 222, 120}, dark[3] = {40, 28, 16};
            float h = fmaxf(0.5f * ui.unit, 0.55f * s), y = sy - 0.5f * s - 0.7f * s * ui_ease(fminf(1, f * 1.4f));
            Uint8 a = (Uint8)(255 * clampf((1 - f) * 2.5f, 0, 1));
            float w = ui_number_w(e->value, h), x = sx + 0.18f * h, l = 0.2f * h, t = fmaxf(1, 0.07f * h);
            ui_number_ex(x + 1.5f, y + 1.5f, h, e->value, dark, (Uint8)(a * 0.6f), 0);
            ui_number_ex(x, y, h, e->value, gold, a, 0);
            float px = x - w / 2 - 0.32f * h;
            ui_fill(px - l + 1.5f, y - t + 1.5f, 2 * l, 2 * t, 40, 28, 16, (Uint8)(a * 0.6f));
            ui_fill(px - l, y - t, 2 * l, 2 * t, 255, 222, 120, a);
            ui_fill(px - t, y - l, 2 * t, 2 * l, 255, 222, 120, a);
            break;
        }
        case FX_STARS:                          /* a perfect tile, a quest done: gold light, stars flying out */
            ui_mark_gold_a(sx, sy, 2.6f * s, 0.9f * sinf(ART_PI * f));
            for (int k = 0; k < 8; k++) {
                float a = k * ART_PI / 4 + 0.4f, r = s * (0.3f + 1.1f * ui_ease(f));
                float d = s * 0.32f * (1 - f) + 2;
                ui_star(sx + cosf(a) * r, sy + sinf(a) * r * TILT - 0.3f * s - 0.4f * s * f, d, 1);
            }
            break;
        case FX_BIRDS:                          /* a forest closed: birds fly up from it */
            for (int k = 0; k < 5; k++) {
                float ph = hf((uint32_t)e->cell, (uint32_t)k), t = clampf(f * 1.15f - 0.1f * ph, 0, 1);
                float bx = sx + s * ((ph - 0.5f) * 1.2f + 2.4f * t * (0.6f + 0.6f * hf((uint32_t)k, 7)));
                float by = sy - 0.4f * s - s * 2.6f * t * (0.7f + 0.5f * ph) + 0.15f * s * sinf(6 * t + ph * 6);
                Uint8 a = (Uint8)(230 * clampf((1 - t) * 3, 0, 1) * clampf(t * 8, 0, 1));
                draw_bird(bx, by, 0.16f * s * (0.8f + 0.4f * ph), age * 0.028f + ph * 6, a);
            }
            break;
        case FX_FLY: {                          /* a tile won flying to the stack */
            float t = ui_ease(f), x = sx + (cnt_x - sx) * t, y = sy + (cnt_y - sy) * t - sinf(ART_PI * t) * 1.6f * s;
            float r = s * (0.5f + (0.42f * tr_r / s - 0.5f) * t);
            put_tile(BACK, x, y, fmaxf(4, r), (Uint8)(255 * clampf((1 - f) * 4, 0, 1)), 0);
            break;
        }
        default:
            break;
        }
    }
    nfx = keep;
}

static void draw_map(Uint32 now)
{
    SDL_Rect clip, was;
    SDL_bool had = SDL_RenderIsClipEnabled(ui.r);
    SDL_RenderGetClipRect(ui.r, &was);
    clip.x = (int)view.x, clip.y = (int)view.y, clip.w = (int)view.w, clip.h = (int)view.h;
    SDL_RenderSetClipRect(ui.r, &clip);
    float s = cam_s;
    make_flat(s);
    make_marks((int)fmaxf(0.52f * s, 0.4f * ui.unit));
    /* the cells that may show */
    int q0 = 99, q1 = -99, r0 = 99, r1 = -99;
    for (int c = 0; c < HXL_CELLS; c++)
        if (G.s.cell[c]) {
            int q, r;
            hxl_qr(c, &q, &r);
            if (q < q0) q0 = q;
            if (q > q1) q1 = q;
            if (r < r0) r0 = r;
            if (r > r1) r1 = r;
        }
    q0 -= 2, q1 += 2, r0 -= 1, r1 += 1;
    const hxl_item *it = hxl_current(&G.s);
    int show_places = it && !win_on;
    /* the empty places beside the landscape: brighter where the tile fits */
    if (show_places)
        for (int r = r0; r <= r1; r++)
            for (int q = q0 - r1; q <= q1 + r1; q++) {
                int c = hxl_cell(q, r);
                if (c < 0 || !hxl_frontier(&G.s, c)) continue;
                float sx, sy;
                cell_screen(c, &sx, &sy);
                if (sx < view.x - s || sx > view.x + view.w + s || sy < view.y - s || sy > view.y + view.h + s) continue;
                int fits = 0;
                for (int k = 0; k < 6 && !fits; k++) fits = hxl_can(&G.s, c, hxl_turn(it->tile, k));
                Uint8 a = (Uint8)(fits ? 105 : 38);
                const uint8_t *ink = ui_table_ink();
                SDL_SetTextureColorMod(t_ring, ink[0], ink[1], ink[2]);
                SDL_SetTextureAlphaMod(t_ring, a);
                float w = SQ3 * s * 1.3f + 4, h = 2 * TILT * s * 1.3f + 4;
                ui_put_c(t_ring, sx, sy + 0.5f * THICK * s, w * 0.77f, h * 0.77f);
            }
    SDL_SetTextureAlphaMod(t_ring, 255);
    /* the shadows, then the tiles from the back */
    float sw = SQ3 * s * 1.3f + 4, shh = 2 * TILT * s * 1.3f + 4;
    for (int c = 0; c < HXL_CELLS; c++)
        if (G.s.cell[c]) {
            float sx, sy;
            cell_screen(c, &sx, &sy);
            if (sx < view.x - 2 * s || sx > view.x + view.w + 2 * s || sy < view.y - 2 * s || sy > view.y + view.h + 2 * s)
                continue;
            ui_put_c(t_shadow, sx + 0.10f * s, sy + (THICK + 0.12f) * s, sw * 0.86f, shh * 0.86f);
        }
    for (int r = r0; r <= r1; r++)
        for (int q = q0 - (r1 - r0) - 2; q <= q1 + (r1 - r0) + 2; q++) {
            int c = hxl_cell(q, r);
            if (c < 0 || !G.s.cell[c]) continue;
            float sx, sy;
            cell_screen(c, &sx, &sy);
            if (sx < view.x - 1.2f * s || sx > view.x + view.w + 1.2f * s || sy < view.y - 0.5f * s ||
                sy > view.y + view.h + 1.5f * s)
                continue;
            if (c == drop_cell && dropping(now)) {   /* falling into place, a soft bounce */
                float f = (now - drop_t0) / (float)DROP_MS, lift;
                if (f < 0.7f) { float g = f / 0.7f; lift = 0.9f * (1 - g * g); }
                else lift = -0.10f * sinf(ART_PI * (f - 0.7f) / 0.3f) * 0.6f + 0.0f, lift = fabsf(lift);
                put_tile(G.s.cell[c], sx, sy - lift * s, s, (Uint8)(255 * clampf(f * 5, 0, 1)), 0);
                ui_animate();
            } else put_tile(G.s.cell[c], sx, sy, s, 255, 0);
            if (win_on) {                       /* the end: a gold light running over the land */
                float wx, wy;
                world_of(c, &wx, &wy);
                float a = puz_pulse((float)(int)(now - win_t0), 60 * hypotf(wx, wy) + 0, 600);
                if (a > 0) ui_mark_gold_a(sx, sy, 1.9f * s, 0.7f * a);
                if ((int)(now - win_t0) < WIN_MS + 600) ui_animate();
            }
        }
    /* the flags */
    for (int i = 0; i < G.s.nq; i++) {
        float pop;
        Uint8 a;
        if (!quest_shown(i, now, &pop, &a)) continue;
        float sx, sy;
        cell_screen(G.s.q[i].cell, &sx, &sy);
        if (G.s.q[i].cell == drop_cell && dropping(now)) continue;
        draw_flag(&G.s.q[i], sx, sy, s, pop, a);
    }
    /* the tile hovering where it would go, its edges marked */
    if (ghost >= 0 && it && !win_on) {
        float sx, sy;
        cell_screen(ghost, &sx, &sy);
        uint32_t t = hxl_turn(it->tile, rot);
        int ok = hxl_can(&G.s, ghost, t), m, perfect;
        hxl_gain(&G.s, ghost, t, &m, &perfect);
        float lift = (0.30f + 0.05f * sinf(now / 260.0f)) * s;
        SDL_SetTextureAlphaMod(t_shadow, 200);   /* its shadow where it would land */
        ui_put_c(t_shadow, sx + 0.06f * s, sy + 0.1f * s, sw * 0.8f, shh * 0.8f);
        SDL_SetTextureAlphaMod(t_shadow, 255);
        if (hint_cell == ghost) ui_mark_gold_a(sx, sy - lift, 2.3f * s, 0.35f + 0.25f * sinf((now - hint_t0) / 180.0f));
        if (perfect && ok) ui_mark_gold_a(sx, sy - lift, 2.4f * s, 0.55f + 0.2f * sinf(now / 200.0f));
        else if (ok) ui_mark_gold_a(sx, sy - lift, 2.0f * s, 0.18f);
        float tf = rot_prev >= 0 ? clampf((now - rot_t0) / (float)TURN_MS, 0, 1) : 1;
        if (tf < 1) {
            put_tile(hxl_turn(it->tile, rot_prev), sx, sy - lift, s, (Uint8)(235 * (1 - tf)), !ok);
            ui_animate();
        }
        put_tile(t, sx, sy - lift, s * (1 + 0.05f * sinf(ART_PI * tf)), (Uint8)(225 * tf), !ok);
        {                                       /* a pale outline: it hovers, not laid */
            SDL_SetTextureColorMod(t_ring, 255, 236, 170);
            SDL_SetTextureAlphaMod(t_ring, 170);
            ui_put_c(t_ring, sx, sy - lift, sw * 0.77f, shh * 0.77f);
            SDL_SetTextureAlphaMod(t_ring, 255);
        }
        if (it->qn) draw_item_flag(it, sx, sy - lift, s);
        float d = fmaxf(0.52f * s, 0.4f * ui.unit);
        for (int k = 0; k < 6; k++) {
            int n = hxl_nb(ghost, k);
            if (n < 0 || !G.s.cell[n]) continue;
            int a = hxl_edge(t, k), b = hxl_edge(G.s.cell[n], hxl_opp(k));
            float mx = sx + 0.80f * AP * s * cosf(k * ART_PI / 3), my = sy - lift + 0.80f * AP * s * sinf(k * ART_PI / 3) * TILT;
            SDL_Texture *mk = a == b ? t_tick : t_cross;
            float z = a != b && (hxl_strict(a) || hxl_strict(b)) ? 1.15f + 0.12f * sinf(now / 110.0f) : 1;
            if (z > 1) ui_animate();
            ui_put_c(mk, mx, my, d * z, d * z);
        }
        ui_animate();
    }
    draw_fx(now);
    if (had) SDL_RenderSetClipRect(ui.r, &was);
    else SDL_RenderSetClipRect(ui.r, NULL);
}

static void draw_tray(Uint32 now)
{
    if (t_tray) ui_put(t_tray, tray.x, tray.y, tray.w, tray.h);
    const hxl_item *it = hxl_current(&G.s);
    int left = hxl_left(&G.s);
    /* the stack: backs under the tile to lay, the whole pile centred on the tray's place */
    int pile = left - 1 > 3 ? 3 : left - 1 < 0 ? 0 : left - 1;
    float step = THICK * tr_r * 0.8f, cy = tr_cy - 0.5f * (pile * step + THICK * tr_r);
    for (int i = pile; i >= 1; i--) put_tile(BACK, tr_cx, cy + i * step, tr_r, 255, 0);
    if (it) {
        float tf = rot_prev >= 0 ? clampf((now - rot_t0) / (float)TURN_MS, 0, 1) : 1;
        if (tf < 1) {
            put_tile(hxl_turn(it->tile, rot_prev), tr_cx, cy, tr_r, (Uint8)(255 * (1 - tf)), 0);
            ui_animate();
        }
        put_tile(hxl_turn(it->tile, rot), tr_cx, cy, tr_r * (1 + 0.06f * sinf(ART_PI * tf)), (Uint8)(255 * tf), 0);
        draw_item_flag(it, tr_cx, cy, tr_r);
    } else if (left <= 0) {
        float sx = tr_cx, sy = cy;
        SDL_SetTextureAlphaMod(t_ring, 120);
        SDL_SetTextureColorMod(t_ring, 60, 40, 26);
        ui_put_c(t_ring, sx, sy, (SQ3 * tr_r * 1.3f + 4) * 0.77f, (2 * TILT * tr_r * 1.3f + 4) * 0.77f);
        SDL_SetTextureAlphaMod(t_ring, 255);
    }
    for (int i = 0; i < 2; i++) {               /* the next two */
        if (G.s.top + 1 + i >= G.s.ns) break;
        const hxl_item *nx = &G.s.stack[G.s.top + 1 + i];
        put_tile(nx->tile, nx_x[i], nx_y[i], nx_r, 225, 0);
        draw_item_flag(nx, nx_x[i], nx_y[i], nx_r);
    }
    /* how many are left; a skipped tile's cross */
    float dh = 0.62f * ui.unit;
    ui_number_ex(cnt_x, cnt_y, dh, left, PUZ_INK, 230, 0);
    for (int j = 0; j < nfx; j++)
        if (FX[j].kind == FX_SKIP && (int)(now - FX[j].t0) >= 0) {
            float f = (now - FX[j].t0) / (float)FX_MS[FX_SKIP];
            ui_put_c(t_cross, tr_cx + tr_r, tr_cy - tr_r * 0.8f, 0.5f * tr_r, 0.5f * tr_r);
            (void)f;
        }
    if (ui.level < 0 && best_now > 0 && t_cup) {   /* the best so far at this size */
        float bx = cnt_x, by = room.h >= room.w ? cnt_y + 0.8f * ui.unit : cnt_y + 0.75f * ui.unit, sz = 0.42f * ui.unit;
        float w = ui_number_w(best_now, 0.4f * ui.unit);
        SDL_SetTextureColorMod(t_cup, PUZ_INK[0], PUZ_INK[1], PUZ_INK[2]);
        ui_put_c(t_cup, bx - w / 2 - 0.1f * ui.unit, by, sz, sz);
        SDL_SetTextureColorMod(t_cup, 255, 255, 255);
        ui_number_ex(bx + sz / 2, by, 0.4f * ui.unit, best_now, PUZ_INK, 200, UI_NUM_REGULAR);
    }
}

static void draw(void)
{
    Uint32 now = ui_ms();
    frame_no++;
    painted = 0;
    cam_at(now);
    draw_map(now);
    draw_tray(now);
    const hxl_item *it = hxl_current(&G.s);
    int ok = it && ghost >= 0 && hxl_can(&G.s, ghost, hxl_turn(it->tile, rot));
    ui_tool(T_PLACE, ICON_TICK, ok ? UI_LIT : UI_DIM);
    ui_tool(T_TURN, ICON_REPLAY, (it ? 0 : UI_DIM) | (tool_down == T_TURN ? UI_ON : 0));
    ui_tool(T_HINT, ICON_HINT, (it ? 0 : UI_DIM) | (tool_down == T_HINT ? UI_ON : 0));
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.s.score, t_par, G.goal, G.s.score >= G.goal);
}

/* ---- the pointer: a tap chooses, one finger drags the map, two pinch ------------------------------------------- */
#define MAX_FINGERS 2
static struct { int id; float x, y; } fg[MAX_FINGERS];
static int nfg, moved, pinching, mouse_down;
static float g_x0, g_y0, g_cx0, g_cy0, g_d0, g_s0, g_wx, g_wy;

static float slop(void) { return 0.016f * fminf(ui.w, ui.h); }

static int in_box(box b, float x, float y) { return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h; }

static void tap_at(float x, float y)
{
    if (in_box(tray, x, y)) {
        if (hypotf(x - tr_cx, (y - tr_cy) / TILT) < 1.2f * tr_r && hxl_current(&G.s)) turn_tile();
        return;
    }
    if (!in_box(view, x, y) || win_on || !hxl_current(&G.s)) return;
    int c = pick(x, y);
    if (c >= 0 && hxl_frontier(&G.s, c)) choose(c);
}

static void tool(int t)
{
    if (t == T_PLACE) place();
    else if (t == T_TURN) turn_tile();
    else if (t == T_HINT) hint();
}

static void drag_begin(float x, float y)
{
    moved = pinching = 0;
    g_x0 = x, g_y0 = y;
    cam_at(ui_ms());
    cam_go = 0;
    g_cx0 = cam_x, g_cy0 = cam_y;
}

static void drag_move(float x, float y)
{
    if (!moved && hypotf(x - g_x0, y - g_y0) > slop() && in_box(view, g_x0, g_y0)) moved = 1;
    if (moved) {
        user_cam = 1;
        cam_set(g_cx0 - (x - g_x0) / cam_s, g_cy0 - (y - g_y0) / (cam_s * TILT), cam_s);
        ui_animate();
    }
}

static void press(float x, float y)
{
    tool_down = ui_tool_hit(x, y);
    if (tool_down >= 0) { ui_animate(); return; }
    mouse_down = 1;
    drag_begin(x, y);
}

static void motion(float x, float y)
{
    if (mouse_down) drag_move(x, y);
}

static void release(float x, float y)
{
    if (tool_down >= 0) {
        int t = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) == t) tool(t);
        return;
    }
    if (!mouse_down) return;
    mouse_down = 0;
    if (!moved) tap_at(x, y);
}

static void finger(int id, int phase, float x, float y)
{
    int k = -1;
    for (int i = 0; i < nfg; i++)
        if (fg[i].id == id) k = i;
    if (phase == FINGER_DOWN) {
        if (!nfg) {
            int t = ui_tool_hit(x, y);
            if (t >= 0) { tool_down = t; ui_animate(); return; }
        }
        if (k >= 0 || nfg >= MAX_FINGERS) return;
        fg[nfg].id = id, fg[nfg].x = x, fg[nfg].y = y;
        nfg++;
        if (nfg == 1) drag_begin(x, y);
        else {                                  /* a pinch: the point between the fingers stays under them */
            g_d0 = fmaxf(1, hypotf(fg[1].x - fg[0].x, fg[1].y - fg[0].y));
            g_s0 = cam_s;
            float mx = (fg[0].x + fg[1].x) / 2, my = (fg[0].y + fg[1].y) / 2;
            g_wx = (mx - view.x - view.w / 2) / cam_s + cam_x;
            g_wy = (my - view.y - view.h / 2) / (cam_s * TILT) + cam_y;
            pinching = moved = 1;
        }
        return;
    }
    if (k < 0) {
        if (phase == FINGER_UP && tool_down >= 0) {
            int t = tool_down;
            tool_down = -1;
            ui_animate();
            if (ui_tool_hit(x, y) == t) tool(t);
        }
        return;
    }
    fg[k].x = x, fg[k].y = y;
    if (phase == FINGER_MOVE) {
        if (nfg == 2) {
            float d = fmaxf(1, hypotf(fg[1].x - fg[0].x, fg[1].y - fg[0].y));
            float s = clampf(g_s0 * d / g_d0, s_least(), s_most());
            float mx = (fg[0].x + fg[1].x) / 2, my = (fg[0].y + fg[1].y) / 2;
            user_cam = 1;
            cam_set(g_wx - (mx - view.x - view.w / 2) / s, g_wy - (my - view.y - view.h / 2) / (s * TILT), s);
            ui_animate();
        } else drag_move(x, y);
        return;
    }
    int single = nfg == 1 && !moved && !pinching;
    fg[k] = fg[--nfg];
    if (single) tap_at(x, y);
    else if (nfg == 1) drag_begin(fg[0].x, fg[0].y), moved = 1;
}

/* the keys: the arrows move the place chosen to the nearest empty place that way (the map follows) */
static void key(SDL_Keycode k)
{
    if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN) {
        if (!hxl_current(&G.s) || win_on) return;
        float dx = k == SDLK_LEFT ? -1 : k == SDLK_RIGHT ? 1 : 0, dy = k == SDLK_UP ? -1 : k == SDLK_DOWN ? 1 : 0;
        float ox, oy;
        if (ghost >= 0) world_of(ghost, &ox, &oy);
        else ox = cam_x - dx * 3, oy = cam_y - dy * 3;
        int best = -1;
        float bd = 1e9f;
        for (int c = 0; c < HXL_CELLS; c++) {
            if (c == ghost || !hxl_frontier(&G.s, c)) continue;
            float wx, wy;
            world_of(c, &wx, &wy);
            float ax = wx - ox, ay = wy - oy, along = ax * dx + ay * dy, across = fabsf(ax * dy - ay * dx);
            if (along <= 0.3f) continue;
            float d = along + 2.2f * across;
            if (d < bd) { bd = d; best = c; }
        }
        if (best >= 0) choose(best);
        else ui_sound(SND_DENY);
    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) place();
    else if (k == SDLK_SPACE) turn_tile();
    else if (k == SDLK_PLUS || k == SDLK_EQUALS || k == SDLK_KP_PLUS || k == SDLK_PAGEUP) {
        user_cam = 1;
        cam_to(cam_go ? cam_tx : cam_x, cam_go ? cam_ty : cam_y, (cam_go ? cam_ts : cam_s) * 1.35f);
    } else if (k == SDLK_MINUS || k == SDLK_KP_MINUS || k == SDLK_PAGEDOWN) {
        user_cam = 1;
        cam_to(cam_go ? cam_tx : cam_x, cam_go ? cam_ty : cam_y, (cam_go ? cam_ts : cam_s) / 1.35f);
    } else if (k == SDLK_0 || k == SDLK_HOME) {
        user_cam = 0;
        cam_auto(0);
    }
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* tiles laid at cells painted into a picture size x size, fitted (their radius at most most * size) */
static void paint_cells(const uint32_t *tile, const int *cells, int n, int size, uint8_t *rgba, float most)
{
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int i = 0; i < n; i++) {
        float wx, wy;
        world_of(cells[i], &wx, &wy);
        x0 = fminf(x0, wx - AP), x1 = fmaxf(x1, wx + AP);
        y0 = fminf(y0, (wy - 1 - OVER / TILT) * TILT), y1 = fmaxf(y1, (wy + 1) * TILT + THICK);
    }
    if (n <= 0) return;
    float rp = fminf(0.94f * size / (x1 - x0), 0.94f * size / (y1 - y0));
    rp = fminf(rp, most * size);
    float ox = size / 2.0f - rp * (x0 + x1) / 2, oy = size / 2.0f - rp * (y0 + y1) / 2;
    int order[64];
    for (int i = 0; i < n && i < 64; i++) order[i] = i;
    for (int i = 1; i < n && i < 64; i++)       /* the back ones first */
        for (int j = i; j > 0 && cells[order[j - 1]] / HXL_N > cells[order[j]] / HXL_N; j--) {
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    for (int i = 0; i < n && i < 64; i++) {     /* soft shadows on the table */
        float wx, wy;
        world_of(cells[order[i]], &wx, &wy);
        float cx = ox + wx * rp + 0.1f * rp, cy = oy + wy * TILT * rp + (THICK + 0.12f) * rp;
        for (int y = (int)(cy - rp) < 0 ? 0 : (int)(cy - rp); y < size && y < (int)(cy + rp); y++)
            for (int x = (int)(cx - rp) < 0 ? 0 : (int)(cx - rp); x < size && x < (int)(cx + rp); x++) {
                float sd = hexsdf((x + 0.5f - cx) / rp, (y + 0.5f - cy) / (rp * TILT));
                float a = 0.4f * smooth(0.2f, -0.12f, sd);
                if (a > 0) px_over(rgba + 4 * ((size_t)y * size + x), 16, 22, 10, a);
            }
    }
    for (int i = 0; i < n && i < 64; i++) {
        float wx, wy;
        world_of(cells[order[i]], &wx, &wy);
        paint_tile(rgba, size, size, ox + wx * rp, oy + wy * TILT * rp, rp, tile[order[i]]);
    }
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    uint32_t t = hxl_tile_parse("ffmmmf");
    int c = HXL_MID;
    paint_cells(&t, &c, 1, size, rgba, 0.5f);
}

/* a small landscape of the maker's: its first n cells from the middle out */
static void paint_land_cells(int radius, int n, uint32_t seed, int size, uint8_t *rgba, float most)
{
    hxl_game *g = malloc(sizeof *g);
    if (!g) return;
    hxl_spec sp = {radius, 0x1e, 1, 0, 2, 0, 0, 0, 0, 0};
    hxl_make(g, &sp, seed);
    /* the landscape whole: the first tiles laid, the stack's put back where they fit best */
    uint32_t tiles[64];
    int cells[64], k = 0;
    static const int8_t ORDER[19][2] = {{0, 0}, {1, 0}, {0, 1}, {-1, 1}, {-1, 0}, {0, -1}, {1, -1}, {2, -1}, {2, 0},
                                        {1, 1}, {0, 2}, {-1, 2}, {-2, 2}, {-2, 1}, {-2, 0}, {-1, -1}, {0, -2},
                                        {1, -2}, {2, -2}};
    for (int i = 0; i < n && i < 19; i++) {
        int c = hxl_cell(ORDER[i][0], ORDER[i][1]);
        if (!g->s.cell[c]) {                    /* laid as the hint would */
            int hc, hk;
            (void)hc, (void)hk;
            const hxl_item *it = hxl_current(&g->s);
            if (!it) break;
            int best = 0, bv = -1;
            for (int t = 0; t < 6; t++)
                if (hxl_can(&g->s, c, hxl_turn(it->tile, t))) {
                    int v = hxl_gain(&g->s, c, hxl_turn(it->tile, t), NULL, NULL);
                    if (v > bv) { bv = v; best = t; }
                }
            if (bv < 0 || !hxl_place(&g->s, c, best, NULL)) continue;
        }
        tiles[k] = g->s.cell[c];
        cells[k++] = c;
    }
    paint_cells(tiles, cells, k, size, rgba, most);
    free(g);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    static const int N[HXL_VARIANTS] = {4, 7, 12};
    if (variant >= 0) {
        paint_land_cells(2, N[variant < HXL_VARIANTS ? variant : 0], 41u, size, rgba, 0.22f);
        return;
    }
    paint_land_cells(2, 10, 23u, size, rgba, 0.3f);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    hxl_game *g = malloc(sizeof *g);
    if (!g) return;
    if (hxl_level(g, level)) {
        uint32_t tiles[64];
        int cells[64], n = 0;
        for (int c = 0; c < HXL_CELLS && n < 64; c++)
            if (g->s.cell[c]) { tiles[n] = g->s.cell[c]; cells[n++] = c; }
        paint_cells(tiles, cells, n, size, rgba, 0.24f);
    }
    free(g);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const T[HXL_CHAPTERS] = {"fffmmm", "vvwwwv", "rmfrmf", "ffffmf", "tmwtmw"};
    uint32_t t = hxl_tile_parse(T[chapter < 0 || chapter >= HXL_CHAPTERS ? 0 : chapter]);
    int c = HXL_MID;
    paint_cells(&t, &c, 1, size, rgba, 0.48f);
    if (chapter == 3) {                         /* the quests: a gold banner on its pole */
        static const uint8_t pole[3] = {70, 52, 36}, cream[3] = {255, 250, 238};
        float px = 0.62f * size, top = 0.12f * size;
        pzk_stroke(rgba, size, size, px, 0.5f * size, px, top, 0.025f * size, pole, 1);
        uint8_t gold[4] = {222, 172, 54, 255};
        int bw = (int)(0.3f * size), bh = (int)(0.2f * size);
        if (bw > 2 && bh > 2) {
            uint8_t *b = malloc((size_t)bw * bh * 4);
            if (b) {
                art_roundrect(bw, bh, 0.22f * bh, gold, b);
                art_blend(rgba, size, size, b, bw, bh, (int)px, (int)top);
                free(b);
            }
            puz_digits(rgba, size, size, 8, px + bw / 2.0f, top + bh * 0.48f, 0.62f * bh, cream, 1, 1);
        }
    }
}

static const game hexland_game = {
    .id = "hexland", .order = 462, .variants = HXL_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .finger = finger, .key = key,
    .levels = HXL_LEVELS, .chapters = HXL_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(hexland_game)
