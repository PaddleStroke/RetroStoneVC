/*
 * Valley Harvest in RetroStone Board, two to four players: a valley of raised hexagonal tiles at a gentle tilt, in
 * Hex Landscape's look (its trees, wheat, rivers and roofs painted by code here, for this valley's land), each
 * player's village growing on it (the rules: valley_rules.c, valley.h; the bot: valley_bot.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The room (game.fill): the valley, and a wooden panel under it (beside it on a wide screen) with the market (each
 * good's stock, a buy and a sell button with their prices), the player to move's goods, coins and workers, the round,
 * and a line telling what a tap shows (a land's harvest, a building's cost) or what the last turn did. The tools:
 * the six buildings and the tick. A turn: tap a land in gold light (its harvest shows over it), tap it again or the
 * tick: the worker walks there and the goods fly to the bar. Then a building tool shows where it may go (tap a place,
 * then again or the tick, or the tool again) and the market's buttons trade a good at a time; the tick ends the
 * turn. Land turned up flips over; ruins send coins; a workshop's rent flies to its owner. Drag pans, pinch zooms
 * (the keys: the arrows move the cursor, Return is the tick, 1..6 the buildings, + and - zoom, 0 the whole valley).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/valley.h"
#include "shell.h"
/* ---- painting (the look of Hex Landscape's tiles, painted for this valley's land) -------------------------------- */
#define SQ3 1.7320508f
#define AP 0.8660254f                           /* a hex's apothem (its radius 1) */
#define TILT 0.78f                              /* the view's tilt: the map's depth squashed */
#define THICK 0.20f                             /* a tile's side, of its radius */
#define OVER 0.62f                              /* room above a tile's picture for its trees and roofs */

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

/* an ellipse lit from the upper left (a rock, a log's end) */
static void oval_fill(uint8_t *o, int W, int H, float cx, float cy, float rx, float ry, const float c[3], float a)
{
    for (int y = (int)(cy - ry - 1) < 0 ? 0 : (int)(cy - ry - 1); y < H && y <= (int)(cy + ry + 1); y++)
        for (int x = (int)(cx - rx - 1) < 0 ? 0 : (int)(cx - rx - 1); x < W && x <= (int)(cx + rx + 1); x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, d = sqrtf(dx * dx + dy * dy);
            float cov = clampf((1 - d) * fminf(rx, ry) + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float k = 1 + 0.30f * (-dx - dy) / 1.4f - 0.12f * d * d;
            px_over(o + 4 * ((size_t)y * W + x), c[0] * k, c[1] * k, c[2] * k, a * cov);
        }
}

static void shade_fill(uint8_t *o, int W, int H, float cx, float cy, float rx, float ry, float a)
{
    for (int y = (int)(cy - ry - 1) < 0 ? 0 : (int)(cy - ry - 1); y < H && y <= (int)(cy + ry + 1); y++)
        for (int x = (int)(cx - rx - 1) < 0 ? 0 : (int)(cx - rx - 1); x < W && x <= (int)(cx + rx + 1); x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, e = dx * dx + dy * dy;
            if (e < 1) px_over(o + 4 * ((size_t)y * W + x), 20, 30, 10, a * (1 - e) * (1 - e));
        }
}

/* ---- a tile's water and paths: curves through it --------------------------------------------------------------- */
typedef struct { float p[24][2]; int n; } curve;
typedef struct { curve c[6]; int nc, ends, pond; float pond_r; } paths;

static void edge_mid(int d, float k, float *u, float *v)
{
    float a = d * ART_PI / 3;
    *u = k * AP * cosf(a);
    *v = k * AP * sinf(a);
}

/* the curves to the edges of mask (bit d: edge d): one bending curve for two, else spokes to a pond in the middle
 * (pond: a pond even for one edge or none, a spring or a lake) */
static void make_paths(unsigned mask, int pond, paths *P)
{
    int e[6], n = 0;
    memset(P, 0, sizeof *P);
    for (int d = 0; d < 6; d++)
        if (mask >> d & 1) e[n++] = d;
    P->ends = n;
    if (n == 2) {
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
    for (int i = 0; i < n; i++) {
        float au, av;
        edge_mid(e[i], 1.1f, &au, &av);
        curve *c = &P->c[P->nc++];
        c->n = 2;
        c->p[0][0] = au, c->p[0][1] = av;
        c->p[1][0] = 0, c->p[1][1] = 0;
    }
    if (n > 2 || pond) {
        P->pond = 1;
        P->pond_r = n == 0 ? 0.40f : n == 1 ? 0.32f : 0.26f;
        P->ends = n ? n : 1;
    }
}

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

/* ---- a tile: its code ----------------------------------------------------------------------------------------- */
enum { K_FOREST, K_FIELD, K_HILL, K_RIVER, K_BACK };
#define TK(kind, bld, own, water, road, var, ruin) \
    ((uint32_t)(kind) | (uint32_t)(bld) << 3 | (uint32_t)(own) << 6 | (uint32_t)(water) << 9 | (uint32_t)(road) << 15 | \
     (uint32_t)(var) << 21 | (uint32_t)(ruin) << 27)
#define TK_KIND(t) ((int)((t) & 7))
#define TK_BLD(t) ((int)((t) >> 3 & 7))
#define TK_OWN(t) ((int)((t) >> 6 & 7))
#define TK_WATER(t) ((unsigned)((t) >> 9 & 63))
#define TK_ROAD(t) ((unsigned)((t) >> 15 & 63))
#define TK_VAR(t) ((uint32_t)((t) >> 21 & 63))
#define TK_RUIN(t) ((int)((t) >> 27 & 1))

static const float GROUND[4][3] = {{78, 124, 60}, {228, 192, 98}, {158, 148, 118}, {132, 184, 82}};

static void ground_col(int k, float u, float v, int x, int y, float *c)
{
    const float *g = GROUND[k];
    float m = 1;
    switch (k) {
    case K_FOREST: m = 0.94f + 0.16f * art_noise(x / 2, y / 2) + 0.06f * art_noise(x, y); break;
    case K_FIELD: {                             /* rows of wheat */
        float s = sinf((u * 0.5f + v * 0.866f) * 34);
        m = 0.92f + 0.09f * s + 0.08f * art_noise(x, y);
        break;
    }
    case K_HILL: {                              /* stony ground with tufts of grass */
        float gr = sinf(u * 5.1f + 1.3f) * sinf(v * 4.3f + 0.4f) + 0.25f * art_noise(x / 3, y / 3);
        m = 0.93f + 0.14f * art_noise(x / 2, y / 2) + 0.08f * art_noise(x, y);
        for (int i = 0; i < 3; i++) c[i] = g[i] * m;
        if (gr > 0.25f) {
            static const float grass[3] = {122, 160, 78};
            float w = clampf((gr - 0.25f) * 3, 0, 0.85f);
            for (int i = 0; i < 3; i++) c[i] += (grass[i] * (0.95f + 0.1f * art_noise(x, y)) - c[i]) * w;
        }
        return;
    }
    default: m = 0.97f + 0.10f * art_noise(x / 3, y / 3) + 0.06f * art_noise(x, y); break;
    }
    for (int i = 0; i < 3; i++) c[i] = g[i] * m;
    if (k == K_RIVER) {                         /* a meadow by the water: little flowers */
        uint32_t h = hsh((uint32_t)(u * 40 + 400), (uint32_t)(v * 40 + 400));
        if ((h & 255) < 5) {
            static const float fl[3][3] = {{250, 248, 236}, {250, 220, 90}, {236, 150, 170}};
            int j = h >> 8 & 3;
            if (j < 3) for (int i = 0; i < 3; i++) c[i] = fl[j][i];
        }
    }
}

/* ---- things standing up ------------------------------------------------------------------------------------------ */
enum { OB_PINE, OB_TREE, OB_STOOK, OB_BUSH, OB_ROCK, OB_RUIN, OB_BUILD };
typedef struct { float u, v, s; int kind, var; } thing;

static void roof_cols(int own, float lit[3], float dark[3])
{
    const float *f = art_player[own < 0 || own >= ART_PLAYERS ? 2 : own].fill;
    for (int i = 0; i < 3; i++) {
        lit[i] = clampf(f[i] * 1.10f + 12, 0, 255);
        dark[i] = f[i] * 0.70f;
    }
}

/* a house: its walls w wide (of s), the gable, the roof in the owner's colour (lit, dark), a door, a window or two */
static void paint_house(uint8_t *o, int W, int H, float x, float y, float s, float w, const float wall[3],
                        const float side[3], const float roof[3], const float roofd[3], int door)
{
    static const float doorc[3] = {112, 72, 46}, win[3] = {66, 76, 102};
    float l = 0.74f * w;                        /* the front's part left of x */
    shade_fill(o, W, H, x + 0.22f * s, y + 0.02f * s, (w + 0.08f) * s, 0.16f * s, 0.32f);
    rect_fill(o, W, H, x - l * s, y - 0.50f * s, x + (w - l) * s, y, wall, 1);
    rect_fill(o, W, H, x + (w - l) * s, y - 0.48f * s, x + (w - l + 0.28f) * s, y - 0.02f * s, side, 1);
    float gx0 = x - (l + 0.02f) * s, gx1 = x + (w - l + 0.02f) * s, gm = (gx0 + gx1) / 2;
    const float gable[] = {gx0, y - 0.49f * s, gx1, y - 0.49f * s, gm, y - 0.84f * s};
    poly_fill(o, W, H, gable, 3, wall, wall, 1e9f, 1);
    const float rf[] = {gm, y - 0.88f * s, gx1 + 0.01f * s, y - 0.52f * s, gx1 + 0.31f * s, y - 0.50f * s,
                        gm + 0.30f * s, y - 0.86f * s};
    poly_fill(o, W, H, rf, 4, roof, roofd, gx1, 1);
    const float eave[] = {gx0 - 0.04f * s, y - 0.47f * s, gm, y - 0.88f * s, gm + 0.04f * s, y - 0.88f * s,
                          gx0 + 0.02f * s, y - 0.47f * s};
    poly_fill(o, W, H, eave, 4, roofd, roofd, 1e9f, 1);
    if (door) rect_fill(o, W, H, gm - 0.07f * s, y - 0.27f * s, gm + 0.06f * s, y, doorc, 1);
    rect_fill(o, W, H, gm + 0.11f * s, y - 0.36f * s, gm + 0.21f * s, y - 0.24f * s, win, 1);
    if (w > 0.7f) rect_fill(o, W, H, gm - 0.29f * s, y - 0.36f * s, gm - 0.19f * s, y - 0.24f * s, win, 1);
}

static void paint_building(uint8_t *o, int W, int H, float x, float y, float s, int kind, int own)
{
    float roof[3], roofd[3];
    roof_cols(own, roof, roofd);
    static const float wall[3] = {242, 232, 212}, side[3] = {204, 190, 166};
    switch (kind) {
    case VL_HOUSE:
        paint_house(o, W, H, x, y, s, 0.54f, wall, side, roof, roofd, 1);
        break;
    case VL_MANOR: {                            /* a tall house and a long one in front */
        static const float tw[3] = {236, 222, 196}, ts[3] = {196, 180, 152};
        paint_house(o, W, H, x - 0.26f * s, y - 0.16f * s, 0.92f * s, 0.50f, tw, ts, roof, roofd, 0);
        paint_house(o, W, H, x + 0.10f * s, y, 0.98f * s, 0.86f, wall, side, roof, roofd, 1);
        break;
    }
    case VL_SAW: {                              /* a plank shed, logs piled beside it */
        static const float pw[3] = {176, 122, 76}, ps[3] = {138, 92, 56}, logc[3] = {150, 104, 62},
                           ring[3] = {222, 186, 132};
        paint_house(o, W, H, x - 0.12f * s, y, s, 0.60f, pw, ps, roof, roofd, 1);
        static const float L[3][2] = {{0.44f, 0.0f}, {0.64f, 0.0f}, {0.54f, -0.17f}};
        for (int i = 0; i < 3; i++) {
            float lx = x + L[i][0] * s, ly = y + L[i][1] * s - 0.09f * s;
            oval_fill(o, W, H, lx, ly, 0.10f * s, 0.10f * s, logc, 1);
            oval_fill(o, W, H, lx - 0.01f * s, ly - 0.01f * s, 0.06f * s, 0.06f * s, ring, 1);
        }
        break;
    }
    case VL_MILL: {                             /* a windmill: a tower, its cap, four sails */
        static const float tw[3] = {238, 228, 206}, ts[3] = {196, 184, 160}, sail[3] = {250, 246, 236},
                           door[3] = {112, 72, 46};
        static const uint8_t arm[3] = {110, 80, 52};
        static const float armf[3] = {110, 80, 52};
        shade_fill(o, W, H, x + 0.2f * s, y, 0.42f * s, 0.12f * s, 0.32f);
        const float tower[] = {x - 0.26f * s, y, x + 0.26f * s, y, x + 0.17f * s, y - 0.80f * s, x - 0.17f * s,
                               y - 0.80f * s};
        poly_fill(o, W, H, tower, 4, tw, ts, x + 0.05f * s, 1);
        const float cap[] = {x - 0.23f * s, y - 0.78f * s, x + 0.23f * s, y - 0.78f * s, x, y - 1.04f * s};
        poly_fill(o, W, H, cap, 3, roof, roofd, x, 1);
        rect_fill(o, W, H, x - 0.06f * s, y - 0.22f * s, x + 0.06f * s, y, door, 1);
        float hx = x, hy = y - 0.78f * s;
        for (int k = 0; k < 4; k++) {
            float a = (k * 90 + 30) * ART_PI / 180, ca = cosf(a), sa = sinf(a), L = 0.58f * s, wd = 0.13f * s;
            const float bl[] = {hx + ca * 0.12f * s, hy + sa * 0.12f * s, hx + ca * L, hy + sa * L,
                                hx + ca * L - sa * wd, hy + sa * L + ca * wd,
                                hx + ca * 0.12f * s - sa * wd, hy + sa * 0.12f * s + ca * wd};
            poly_fill(o, W, H, bl, 4, sail, sail, 1e9f, 0.95f);
            pzk_stroke(o, W, H, hx, hy, hx + ca * L, hy + sa * L, fmaxf(0.5f, 0.02f * s), arm, 1);
        }
        ball_fill(o, W, H, hx, hy, 0.045f * s, armf, 1);
        break;
    }
    case VL_QUARRY: {                           /* a stone hut and cut blocks */
        static const float sw[3] = {190, 184, 172}, ss[3] = {150, 144, 134}, blk[3] = {210, 204, 192};
        paint_house(o, W, H, x - 0.12f * s, y, s, 0.54f, sw, ss, roof, roofd, 1);
        rect_fill(o, W, H, x + 0.40f * s, y - 0.17f * s, x + 0.64f * s, y, blk, 1);
        rect_fill(o, W, H, x + 0.64f * s, y - 0.16f * s, x + 0.70f * s, y, ss, 1);
        rect_fill(o, W, H, x + 0.45f * s, y - 0.33f * s, x + 0.65f * s, y - 0.17f * s, blk, 1);
        rect_fill(o, W, H, x + 0.65f * s, y - 0.32f * s, x + 0.70f * s, y - 0.17f * s, ss, 1);
        break;
    }
    default:
        break;
    }
}

static void paint_thing(uint8_t *o, int W, int H, float x, float y, float rp, const thing *t, int own)
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
    case OB_ROCK: {
        static const float g[2][3] = {{176, 170, 160}, {150, 142, 130}};
        const float *c = g[t->var & 1];
        shade_fill(o, W, H, x + 0.10f * h, y + 0.01f * h, 0.30f * h, 0.08f * h, 0.32f);
        oval_fill(o, W, H, x, y - 0.12f * h, 0.26f * h, 0.18f * h, c, 1);
        if (t->var & 2) oval_fill(o, W, H, x + 0.21f * h, y - 0.05f * h, 0.13f * h, 0.09f * h, c, 1);
        break;
    }
    case OB_RUIN: {                             /* the old stones of the ruins found here */
        static const float st[3] = {216, 206, 186}, sd[3] = {172, 162, 142};
        shade_fill(o, W, H, x + 0.12f * h, y, 0.40f * h, 0.09f * h, 0.3f);
        rect_fill(o, W, H, x - 0.32f * h, y - 0.66f * h, x - 0.18f * h, y, st, 1);
        rect_fill(o, W, H, x - 0.18f * h, y - 0.62f * h, x - 0.13f * h, y, sd, 1);
        rect_fill(o, W, H, x + 0.10f * h, y - 0.38f * h, x + 0.24f * h, y, st, 1);
        rect_fill(o, W, H, x + 0.24f * h, y - 0.36f * h, x + 0.29f * h, y, sd, 1);
        rect_fill(o, W, H, x - 0.38f * h, y - 0.74f * h, x + 0.02f * h, y - 0.64f * h, sd, 1);
        oval_fill(o, W, H, x + 0.36f * h, y - 0.05f * h, 0.10f * h, 0.06f * h, sd, 1);
        break;
    }
    default:
        paint_building(o, W, H, x, y, h, t->var, own);
        break;
    }
}

/* a tile painted over a picture W x H, its middle at (ax, ay), rp pixels its radius: the side, the ground, the water
 * and the road, then the things standing on it from the back */
static void paint_tile(uint8_t *o, int W, int H, float ax, float ay, float rp, uint32_t code)
{
    int kind = TK_KIND(code), bld = TK_BLD(code), own = TK_OWN(code);
    float dp = THICK * rp, sy = rp * TILT;
    int x0 = (int)floorf(ax - AP * rp - 1), x1 = (int)ceilf(ax + AP * rp + 1);
    int y0 = (int)floorf(ay - sy - 1), y1 = (int)ceilf(ay + sy + dp + 1);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    paths river, road;
    make_paths(kind == K_RIVER ? TK_WATER(code) : 0, kind == K_RIVER, &river);
    make_paths(bld == VL_ROAD ? TK_ROAD(code) : 0, 0, &road);
    int road_on = bld == VL_ROAD;
    const float WR = 0.17f, BANK = 0.05f, RW = 0.11f;
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
                    if (kind == K_BACK) c[0] = 108, c[1] = 70, c[2] = 44;
                    else if (dv < 0.2f * dp) c[0] = 86, c[1] = 132, c[2] = 58;
                    else k *= 0.95f + 0.1f * art_noise(x / 2, (int)(dv * 3));
                    px_over(p, c[0] * k, c[1] * k, c[2] * k, cov);
                }
            }
            float sd = hexsdf(u, v), cov = clampf(0.5f - sd * rp * 0.9f, 0, 1);
            if (cov <= 0) continue;
            float c[3];
            if (kind == K_BACK) {               /* face down: walnut, a carved hexagon and a leaf in its middle */
                float g = 0.92f + 0.08f * sinf(u * 26 + 3 * sinf(v * 7)) + 0.06f * art_noise(x, y / 3);
                c[0] = 150 * g, c[1] = 104 * g, c[2] = 66 * g;
                float in = hexsdf(u / 0.62f, v / 0.62f) * 0.62f, groove = clampf(1 - fabsf(in) * rp / 1.6f, 0, 1);
                float k = 1 - 0.30f * groove + (in < 0 && in > -0.03f ? 0.12f : 0);
                float lu = u * 1.0f, lv = v * 1.0f, leaf = fabsf(lu) * 2.6f + fabsf(lv) * 1.2f - 0.36f + 0.25f * fabsf(lv);
                if (leaf < 0) k *= 0.82f + 0.1f * clampf(-leaf * 8, 0, 1) * (fabsf(lu) < 0.012f ? 0 : 1);
                for (int i = 0; i < 3; i++) c[i] *= k;
            } else {
                ground_col(kind, u, v, x, y, c);
                if (bld >= VL_HOUSE) {          /* a yard of trodden earth round a building */
                    float r = hypotf(u, v * 1.15f) - 0.46f;
                    if (r < 0.06f) {
                        static const float yard[3] = {178, 160, 116};
                        float w = clampf((0.06f - r) / 0.12f, 0, 0.85f), n = 0.94f + 0.12f * art_noise(x, y);
                        for (int i = 0; i < 3; i++) c[i] += (yard[i] * n - c[i]) * w;
                    }
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
                if (road_on) {                  /* a road of packed earth: to the village, a little square */
                    float d0 = fminf(road.ends ? paths_dist(&road, u, v, 0, NULL) : 9, hypotf(u, v) - 0.14f);
                    if (d0 < RW + 2 / rp) {
                        static const float dirt[3] = {196, 168, 120}, edge[3] = {150, 124, 84};
                        float a = clampf((RW - d0) * rp + 0.5f, 0, 1), n = 0.93f + 0.14f * art_noise(x, y);
                        float e = clampf((d0 - (RW - 0.03f)) * rp, 0, 1);
                        for (int i = 0; i < 3; i++) c[i] += ((dirt[i] + (edge[i] - dirt[i]) * e) * n - c[i]) * a;
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
    if (kind == K_BACK) return;
    /* the things on it, from the back */
    thing th[80];
    int n = 0;
    static const struct { float step, keep, size; int kind; } KIND[4] = {
        {0.23f, 0.86f, 0.30f, OB_PINE}, {0.40f, 0.34f, 0.30f, OB_STOOK}, {0.34f, 0.58f, 0.30f, OB_ROCK},
        {0.44f, 0.20f, 0.30f, OB_BUSH},
    };
    uint32_t var = TK_VAR(code) * 131u + (uint32_t)kind;
    float clear = bld >= VL_HOUSE ? 0.58f : 0;  /* room for the building */
    float st = KIND[kind].step;
    for (int j = -6; j <= 6; j++)
        for (int i = -6; i <= 6; i++) {
            uint32_t hh = hsh(var, (uint32_t)((j + 8) * 32 + i + 8));
            float u = (i + 0.5f * (j & 1)) * st + ((hh & 255) / 255.0f - 0.5f) * 0.5f * st;
            float v = j * st * 0.87f + ((hh >> 8 & 255) / 255.0f - 0.5f) * 0.5f * st;
            if (hexsdf(u, v) > -0.13f || (hh >> 16 & 255) / 255.0f > KIND[kind].keep) continue;
            if (clear > 0 && hypotf(u, v * 1.15f) < clear) continue;
            if (river.ends && paths_dist(&river, u, v, WR, NULL) < WR + 0.09f) continue;
            if (road_on && fminf(road.ends ? paths_dist(&road, u, v, 0, NULL) : 9, hypotf(u, v) - 0.14f) < RW + 0.07f)
                continue;
            if (TK_RUIN(code) && hypotf(u - 0.05f, v - 0.1f) < 0.38f) continue;
            if (n >= 78) continue;
            thing *t = &th[n++];
            t->u = u, t->v = v;
            t->var = (int)(hh >> 24);
            t->s = KIND[kind].size * (0.86f + 0.28f * hf(hh, 3));
            t->kind = KIND[kind].kind;
            if (t->kind == OB_PINE && !(t->var & 3)) t->kind = OB_TREE;
        }
    if (TK_RUIN(code)) th[n++] = (thing){0.05f, 0.12f, 0.36f, OB_RUIN, 0};
    if (bld >= VL_HOUSE) th[n++] = (thing){-0.04f, 0.16f, bld == VL_MANOR ? 0.62f : 0.66f, OB_BUILD, bld};
    for (int i = 1; i < n; i++)                 /* the back ones first */
        for (int j = i; j > 0 && th[j - 1].v > th[j].v; j--) { thing t = th[j]; th[j] = th[j - 1]; th[j - 1] = t; }
    for (int i = 0; i < n; i++) paint_thing(o, W, H, ax + th[i].u * rp, ay + th[i].v * sy, rp, &th[i], own);
}

/* a tile's picture: its size for a radius, its middle in it */
static void tile_box(float rp, int *w, int *h, float *ax, float *ay)
{
    *w = (int)ceilf(SQ3 * rp) + 4;
    *h = (int)ceilf((OVER + 2 * TILT + THICK) * rp) + 4;
    *ax = *w / 2.0f;
    *ay = 2 + (OVER + TILT) * rp;
}

/* ---- the pictures kept: tiles at a few sizes; buildings, workers and icons at the size shown --------------------- */
typedef struct { uint32_t code; int b; SDL_Texture *t; int w, h; float ax, ay, rp; Uint32 used; } kept;
#define KEPT 220
static kept K[KEPT];
static Uint32 frame_no;
static int painted;

static int bucket_of(float rp) { return (int)lroundf(logf(fmaxf(rp, 4)) / logf(1.12f)); }

static kept *paint_kept(uint32_t code, int b)
{
    int slot = 0;
    for (int i = 0; i < KEPT; i++) {
        if (!K[i].t) { slot = i; break; }
        if (K[i].used < K[slot].used) slot = i;
    }
    kept *k = &K[slot];
    ui_free(&k->t);
    k->code = code;
    k->b = b;
    k->rp = powf(1.12f, (float)b);
    tile_box(k->rp, &k->w, &k->h, &k->ax, &k->ay);
    uint8_t *o = ui_buf(k->w, k->h);
    memset(o, 0, (size_t)k->w * k->h * 4);
    paint_tile(o, k->w, k->h, k->ax, k->ay, k->rp, code);
    k->t = ui_tex(k->w, k->h, o);
    k->used = frame_no;
    painted++;
    return k;
}

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
    if (near && painted >= 6) {                 /* a few new ones a frame; the others from a near size meanwhile */
        near->used = frame_no;
        ui_animate();
        return near;
    }
    return paint_kept(code, b);
}

/* a tile at (sx, sy) of radius rp; xs: squeezed across (a flip), lift: raised */
static void put_tile(uint32_t code, float sx, float sy, float rp, Uint8 alpha, float xs)
{
    kept *k = tile_pic(code, rp);
    if (!k || !k->t) return;
    float z = rp / k->rp;
    SDL_SetTextureAlphaMod(k->t, alpha);
    ui_put(k->t, sx - k->ax * z * xs, sy - k->ay * z, k->w * z * xs, k->h * z);
    SDL_SetTextureAlphaMod(k->t, 255);
}

/* the other pictures: a building (kind, owner), a worker (player), an icon; keyed and kept at the size drawn */
enum { PIC_BUILD, PIC_WORKER, PIC_ICON, PIC_MARKET };
enum { IC_WOOD, IC_GRAIN, IC_STONE, IC_COIN, IC_STAR, IC_PAWN, IC_GOODS };
typedef struct { uint32_t key; SDL_Texture *t; int w, h; float ax, ay; Uint32 used; } pic;
#define PICS 120
static pic PC[PICS];

static const uint8_t WHITE[3] = {255, 255, 255};

/* a worker in a unit square (its feet at the bottom middle): a round head on a bell of a body, the player's lacquer */
static void paint_worker(uint8_t *o, int w, int h, float x, float y, float s, int player)
{
    art_fig *f = art_fig_new();
    const float body[] = {0.36f, 0.42f, 0.64f, 0.42f, 0.70f, 0.62f, 0.80f, 0.94f, 0.20f, 0.94f, 0.30f, 0.62f};
    art_poly(f, ART_PART, body, 6);
    art_ellipse(f, ART_PART, 0.5f, 0.27f, 0.15f, 0.15f);
    art_style st = art_player[player < 0 || player >= ART_PLAYERS ? 0 : player];
    st.line = 0.045f;
    uint8_t *tmp = malloc((size_t)w * h * 4);
    if (tmp) {
        memset(tmp, 0, (size_t)w * h * 4);
        art_render(f, &st, w, h, s, x, y, tmp);
        art_blend(o, w, h, tmp, w, h, 0, 0);
        free(tmp);
    }
    art_fig_free(f);
}

/* an icon in a unit square: wood (two logs), grain (a sheaf), stone (a rock), a coin, a star (points), a worker */
static void paint_icon(uint8_t *o, int size, int which, int player)
{
    float s = (float)size;
    switch (which) {
    case IC_WOOD: {
        static const float bark[3] = {132, 88, 52}, cut[3] = {226, 190, 136}, ring[3] = {196, 154, 100};
        for (int i = 0; i < 2; i++) {
            float y = s * (0.38f + 0.26f * i), x0 = s * 0.12f, x1 = s * 0.74f;
            rect_fill(o, size, size, x0, y - 0.13f * s, x1, y + 0.13f * s, bark, 1);
            oval_fill(o, size, size, x1, y, 0.10f * s, 0.13f * s, cut, 1);
            oval_fill(o, size, size, x1, y, 0.05f * s, 0.065f * s, ring, 1);
        }
        break;
    }
    case IC_GRAIN: {
        static const float lit[3] = {244, 210, 110}, dark[3] = {196, 152, 62}, band[3] = {150, 104, 52};
        for (int k = -2; k <= 2; k++) {         /* stalks fanned out, ears at their tops */
            float a = k * 0.17f, bx = 0.5f * s, by = 0.86f * s, tx = bx + sinf(a) * 0.5f * s, ty = by - cosf(a) * 0.62f * s;
            pzk_stroke(o, size, size, bx, by, tx, ty, fmaxf(0.6f, 0.025f * s), (const uint8_t[]){196, 152, 62}, 1);
            oval_fill(o, size, size, tx, ty - 0.02f * s, 0.06f * s, 0.12f * s, k & 1 ? dark : lit, 1);
        }
        rect_fill(o, size, size, 0.40f * s, 0.58f * s, 0.60f * s, 0.65f * s, band, 1);
        break;
    }
    case IC_STONE: {
        static const float g1[3] = {178, 172, 162}, g2[3] = {146, 138, 126};
        oval_fill(o, size, size, 0.42f * s, 0.58f * s, 0.30f * s, 0.24f * s, g1, 1);
        oval_fill(o, size, size, 0.68f * s, 0.68f * s, 0.18f * s, 0.15f * s, g2, 1);
        break;
    }
    case IC_COIN: {
        static const float rim[3] = {196, 146, 40}, face[3] = {246, 202, 84}, mark[3] = {214, 164, 52};
        oval_fill(o, size, size, 0.5f * s, 0.5f * s, 0.40f * s, 0.40f * s, rim, 1);
        oval_fill(o, size, size, 0.5f * s, 0.5f * s, 0.31f * s, 0.31f * s, face, 1);
        oval_fill(o, size, size, 0.5f * s, 0.5f * s, 0.13f * s, 0.13f * s, mark, 1);
        break;
    }
    case IC_GOODS: {                           /* a log, a sheaf and a stone together */
        int h = size / 2 + 1;
        uint8_t *t = malloc((size_t)h * h * 4);
        if (!t) break;
        for (int i = 0; i < 3; i++) {
            memset(t, 0, (size_t)h * h * 4);
            paint_icon(t, h, i, 0);
            static const float AT[3][2] = {{0.02f, 0.30f}, {0.48f, 0.02f}, {0.46f, 0.48f}};
            art_blend(o, size, size, t, h, h, (int)(AT[i][0] * s), (int)(AT[i][1] * s));
        }
        free(t);
        break;
    }
    case IC_STAR: {
        art_fig *f = art_fig_new();
        art_star(f, ART_PART, 0.5f, 0.54f, 0.44f);
        art_fig_paint(o, size, f, (const uint8_t[]){240, 196, 72}, (const uint8_t[]){150, 104, 30}, 0.04f, 0.4f);
        break;
    }
    default:
        paint_worker(o, size, size, 0, 0, s, player);
        break;
    }
}

/* the market's buttons: a plus (buy) or a minus (sell), white, at the top left of the disc */
static void paint_market(int kind, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    float c = 0.24f, w = 0.075f, l = 0.15f;
    art_rect(f, ART_PART, c - l, c - w, c + l, c + w);
    if (!kind) art_rect(f, ART_PART, c - w, c - l, c + w, c + l);
    art_fig_paint(rgba, size, f, WHITE, NULL, 0, 0);
}

static pic *pic_get(int type, int a, int b, int size)
{
    if (size < 2) size = 2;
    if (size > 1024) size = 1024;
    uint32_t key = (uint32_t)type << 28 | (uint32_t)(a & 15) << 24 | (uint32_t)(b & 15) << 20 | (uint32_t)size;
    int slot = 0;
    for (int i = 0; i < PICS; i++) {
        if (PC[i].t && PC[i].key == key) { PC[i].used = frame_no; return &PC[i]; }
        if (!PC[i].t) slot = i;
        else if (PC[slot].t && PC[i].used < PC[slot].used) slot = i;
    }
    pic *p = &PC[slot];
    ui_free(&p->t);
    p->key = key;
    p->used = frame_no;
    float s = (float)size;
    if (type == PIC_BUILD) {                    /* size: the building's height unit (its s) */
        p->w = (int)(1.5f * s) + 6, p->h = (int)(1.6f * s) + 6;
        p->ax = 0.64f * s + 3, p->ay = 1.42f * s + 3;
    } else if (type == PIC_WORKER) {
        p->w = (int)(0.62f * s) + 4, p->h = size + 4;
        p->ax = p->w / 2.0f, p->ay = s * 0.94f + 2;
    } else {
        p->w = p->h = size;
        p->ax = p->ay = s / 2;
    }
    uint8_t *o = ui_buf(p->w, p->h);
    memset(o, 0, (size_t)p->w * p->h * 4);
    if (type == PIC_BUILD) paint_building(o, p->w, p->h, p->ax, p->ay, s, a, b);
    else if (type == PIC_WORKER) paint_worker(o, p->w, p->h, p->ax - 0.5f * s, 2, s, a);
    else if (type == PIC_ICON) paint_icon(o, size, a, b);
    else paint_market(a, size, o);
    p->t = ui_tex(p->w, p->h, o);
    return p;
}

static void pic_at(pic *p, float x, float y, float z, Uint8 alpha)
{
    if (!p || !p->t) return;
    SDL_SetTextureAlphaMod(p->t, alpha);
    ui_put(p->t, x - p->ax * z, y - p->ay * z, p->w * z, p->h * z);
    SDL_SetTextureAlphaMod(p->t, 255);
}

static SDL_Texture *icon_tex(int which, int player, float d)
{
    pic *p = pic_get(PIC_ICON, which, player, (int)lroundf(d));
    return p ? p->t : NULL;
}

static void icon_at(int which, int player, float cx, float cy, float d, Uint8 alpha)
{
    pic *p = pic_get(PIC_ICON, which, player, (int)lroundf(d));
    if (p) pic_at(p, cx, cy, 1, alpha);
}

/* the flat pictures: a tile's shadow and its outline (the places, the owners' rims) */
static SDL_Texture *t_shadow, *t_ring;
static int made_flat = -1;

static void make_flat(float rp)
{
    int b = bucket_of(rp);
    if (b == made_flat) return;
    made_flat = b;
    float r = powf(1.12f, (float)b);
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
            float lw = fmaxf(0.045f, 2.2f / r), ring = clampf((lw - fabsf(sd + 0.08f)) * r + 0.5f, 0, 1);
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(255 * ring);
        }
    ui_free(&t_ring);
    t_ring = ui_tex(w, h, o);
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
enum { T_ROAD, T_HOUSE, T_SAW, T_MILL, T_QUARRY, T_MANOR, T_OK, TOOLS };
static const int TOOL_BLD[6] = {VL_ROAD, VL_HOUSE, VL_SAW, VL_MILL, VL_QUARRY, VL_MANOR};

static vl_game G;
static int variant_now;
static int sel = -1, mode, peek, tool_down = -1;    /* the cell chosen; the building chosen (0 none); a cost shown */

/* the effects: a worker walking out or home, a building dropping, a tile turning up, an icon flying, points rising */
enum { FX_HOP, FX_HOME, FX_DROP, FX_FLIP, FX_FLY, FX_POP };
#define HOP_MS 420
#define DROP_MS 460
#define FLIP_MS 520
#define FLY_MS 640
#define POP_MS 1000
typedef struct { int kind, a, b, c, d; Uint32 t0; int ms; } fxe;
#define MAX_FX 64
static fxe FX[MAX_FX];
static int nfx;
static Uint32 busy_until;

/* where a flying icon starts and lands: a cell, a player's bar, a good's place in the market */
#define AT_BAR(p) (100 + (p))
#define AT_MARKET(g) (200 + (g))

/* the last turn, told in the panel: who, and what each action gave */
typedef struct { int player, n; int16_t m[16]; int8_t amount[16]; } turnlog;
static turnlog log_now, log_last;

static void fx_add(int kind, int a, int b, int c, int d, Uint32 t0, int ms)
{
    if (nfx >= MAX_FX) return;
    FX[nfx++] = (fxe){kind, a, b, c, d, t0, ms};
    if (kind != FX_POP && t0 + (Uint32)ms > busy_until) busy_until = t0 + (Uint32)ms;
    ui_animate();
}

static int busy(void) { return (int)(busy_until - ui_ms()) > 0; }

static void calm(void)
{
    nfx = 0;
    busy_until = ui_ms();
    sel = -1;
    mode = peek = 0;
    tool_down = -1;
}

static const vl_state *S(void) { return &G.s; }

static int home_of(int p)                       /* the player's home: their first house */
{
    for (int c = 0; c < VL_CELLS; c++)
        if (G.s0.own[c] == p && G.s0.bld[c] == VL_HOUSE) return c;
    return 0;
}

/* ---- the screen ------------------------------------------------------------------------------------------------ */
static box room, view, panel;
static int wide;
static float cam_x, cam_y, cam_s = 40, cam_ex, cam_ey, cam_es, cam_tx, cam_ty, cam_ts;
static Uint32 cam_t0;
static int cam_go, user_cam;
/* the panel's places */
static float mk_x[3], mk_y[3], mk_buy[3][2], mk_sell[3][2], mk_d, mk_icon, pip_w;
static box row_player, row_info;
static SDL_Texture *t_panel;
static int made_pw = -1, made_ph = -1;

static void world_of(int c, float *wx, float *wy)
{
    *wx = SQ3 * (vl_q[c] + 0.5f * vl_r[c]);
    *wy = 1.5f * vl_r[c];
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
    int c = vl_cell(q, r);
    return c >= 0 && vl_in(S(), c) ? c : -1;
}

static float s_fit(void)
{
    float R = (float)S()->radius, w = SQ3 * (2 * R + 1) + 0.4f, h = (1.5f * 2 * R + 2) * TILT + THICK + 1.0f;
    return fminf(view.w / w, view.h / h);
}
static float s_most(void) { return fmaxf(s_fit() * 2.6f, 0.9f * ui.unit); }
static float s_least(void) { return s_fit() * 0.8f; }

static void cam_at(Uint32 now)
{
    if (!cam_go) return;
    float f = (float)(int)(now - cam_t0) / 380;
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
    float R = (float)S()->radius * 1.6f;
    cam_x = cam_tx = clampf(x, -R * SQ3 / 1.6f, R * SQ3 / 1.6f), cam_y = cam_ty = clampf(y, -R, R);
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

static void cam_home(int at_once)               /* the whole valley (its roofs a little room above) */
{
    float y = -0.25f;
    if (at_once) cam_set(0, y, s_fit());
    else cam_to(0, y, s_fit());
}

static void layout(box b)
{
    room = b;
    float u = ui.unit;
    wide = b.w > b.h * 1.15f;
    if (!wide) {                                /* upright: the panel under the valley */
        float ph = clampf(0.32f * b.h, 3.3f * u, 3.6f * u);
        panel = (box){b.x, b.y + b.h - ph, b.w, ph};
        view = (box){b.x, b.y, b.w, b.h - ph - 0.1f * u};
        float pad = 0.18f * u, cw = (panel.w - 2 * pad) / 3;
        mk_d = fminf(0.92f * u, cw * 0.34f);
        mk_icon = fminf(0.72f * u, cw * 0.26f);
        float my = panel.y + pad + 0.5f * u;
        for (int g = 0; g < 3; g++) {
            float x0 = panel.x + pad + g * cw;
            mk_x[g] = x0 + 0.06f * cw + mk_icon / 2, mk_y[g] = my - 0.08f * u;
            mk_buy[g][0] = x0 + 0.46f * cw, mk_buy[g][1] = my;
            mk_sell[g][0] = x0 + 0.80f * cw, mk_sell[g][1] = my;
        }
        pip_w = cw * 0.9f;
        row_player = (box){panel.x + pad, my + 0.72f * u, panel.w - 2 * pad, 0.8f * u};
        row_info = (box){panel.x + pad, row_player.y + row_player.h + 0.05f * u, panel.w - 2 * pad,
                         panel.y + panel.h - pad - (row_player.y + row_player.h + 0.05f * u)};
    } else {                                    /* wide: the panel on the valley's right */
        float pw = clampf(0.32f * b.w, 3.6f * u, 4.6f * u);
        panel = (box){b.x + b.w - pw, b.y, pw, b.h};
        view = (box){b.x, b.y, b.w - pw - 0.1f * u, b.h};
        float pad = 0.18f * u, rh = fminf(1.25f * u, (panel.h - 2 * pad) / 7.0f);
        row_player = (box){panel.x + pad, panel.y + pad, panel.w - 2 * pad, 2.1f * rh};
        mk_d = fminf(0.92f * u, 0.8f * rh);
        mk_icon = fminf(0.72f * u, 0.62f * rh);
        for (int g = 0; g < 3; g++) {
            float y = row_player.y + row_player.h + 0.2f * u + (g + 0.5f) * rh * 1.15f;
            mk_x[g] = panel.x + pad + 0.1f * u + mk_icon / 2, mk_y[g] = y - 0.12f * rh;
            mk_buy[g][0] = panel.x + 0.52f * panel.w, mk_buy[g][1] = y;
            mk_sell[g][0] = panel.x + 0.80f * panel.w, mk_sell[g][1] = y;
        }
        pip_w = 0.30f * panel.w;
        float iy = mk_y[2] + 0.75f * rh + 0.2f * u;
        row_info = (box){panel.x + pad, iy, panel.w - 2 * pad, fmaxf(0.8f * u, panel.y + panel.h - pad - iy)};
    }
    int w = (int)panel.w, h = (int)panel.h;
    if (w != made_pw || h != made_ph) {
        made_pw = w, made_ph = h;
        int f = (int)fmaxf(3, 0.2f * u);
        uint8_t *o = malloc((size_t)w * h * 4);
        ui_free(&t_panel);
        if (o && w > 2 * f && h > 2 * f) {
            memset(o, 0, (size_t)w * h * 4);
            art_plank(w - 2 * f, h - 2 * f, f, o);
            puz_bevel(o, w, h, 0.12f * u, 0.06f * u, 0.25f);
            t_panel = ui_tex(w, h, o);
        }
        free(o);
    }
    ui_board_used(b.x, b.y, b.w, b.h);
    if (!user_cam) cam_home(1);
    else cam_set(cam_x, cam_y, cam_s);
}

/* ---- playing ----------------------------------------------------------------------------------------------------- */
static void begin(void)
{
    calm();
    user_cam = 0;
    log_now = (turnlog){.player = S()->cur};
    log_last = (turnlog){.player = -1};
    if (room.w > 0) layout(room);
}

static void start(int variant)
{
    variant_now = variant < 0 || variant >= VL_VARIANTS ? 0 : variant;
    int players = ui.players >= 2 && ui.players <= VL_PLAYERS ? ui.players : 2;
    uint32_t seed = (uint32_t)ui_random(1 << 30);
    vl_begin(&G, players, variant_now, seed);
    begin();
}

static int status(void)
{
    if (!S()->over) return GS_PLAY;
    unsigned w = vl_winners(S());
    if (w & (w - 1)) return GS_DRAW;
    for (int p = 0; p < S()->players; p++)
        if (w >> p & 1) return gs_win(p);
    return GS_DRAW;
}

static unsigned winners(void) { return S()->over ? vl_winners(S()) : 0; }
static int turn(void) { return S()->cur; }

static void log_add(int m, int amount)
{
    if (log_now.n < 16) {
        log_now.m[log_now.n] = (int16_t)m;
        log_now.amount[log_now.n++] = (int8_t)amount;
    }
}

/* an action played as a tap does: the rules, then its effects on the screen */
static int act(int m)
{
    vl_state before = G.s;
    vl_effect e;
    if (!vl_play(&G, m, &e)) { ui_sound(SND_DENY); return 0; }
    Uint32 now = ui_ms(), t = now;
    int p = e.player;
    sel = -1;
    mode = peek = 0;
    switch (e.kind) {
    case 0: {                                   /* a harvest: the worker walks out, the goods fly to the bar */
        fx_add(FX_HOP, p, home_of(p), e.cell, 0, t, HOP_MS);
        int t1 = HOP_MS - 60;
        for (int i = 0; i < e.nrev; i++) fx_add(FX_FLIP, e.revealed[i], 0, 0, 0, t + 120, FLIP_MS);
        if (e.nrev) t1 += FLIP_MS;
        for (int i = 0; i < e.amount; i++)
            fx_add(FX_FLY, e.good, e.cell, AT_BAR(p), 0, t + (Uint32)t1 + (Uint32)i * 110, FLY_MS);
        for (int i = 0; i < e.nrented; i++)
            fx_add(FX_FLY, IC_COIN, e.rented[i], AT_BAR(S()->own[e.rented[i]]), 0, t + (Uint32)t1 + 200 + (Uint32)i * 140,
                   FLY_MS);
        if (e.good < 0 && e.amount == 0) fx_add(FX_FLY, IC_COIN, e.cell, AT_BAR(p), 0, t + (Uint32)t1, FLY_MS);
        if (e.gift) fx_add(FX_FLY, IC_COIN, e.cell, AT_BAR(p), 0, t + (Uint32)t1 + 300, FLY_MS);
        ui_sound(SND_MOVE);
        ui_sound_later(SND_PLACE, HOP_MS);
        log_add(m, e.amount);
        break;
    }
    case 1:                                     /* resting: a coin */
        fx_add(FX_FLY, IC_COIN, home_of(p), AT_BAR(p), 0, t, FLY_MS);
        ui_sound(SND_TAP);
        log_add(m, 1);
        break;
    case 2: {                                   /* a building drops in; land beside it turns up */
        int k = m / 64;
        fx_add(FX_DROP, e.cell, k, 0, 0, t, DROP_MS);
        if (VL.pts[k]) fx_add(FX_POP, e.cell, VL.pts[k], 0, 0, t + DROP_MS - 100, POP_MS);
        for (int i = 0; i < e.nrev; i++) fx_add(FX_FLIP, e.revealed[i], 0, 0, 0, t + DROP_MS - 80 + (Uint32)i * 130, FLIP_MS);
        for (int i = 0; i < e.nrev; i++)
            if (S()->gift[e.revealed[i]] == 2 && before.gift[e.revealed[i]] == 1)
                for (int j = 0; j < 3; j++)
                    fx_add(FX_FLY, IC_COIN, e.revealed[i], AT_BAR(p), 0,
                           t + DROP_MS + FLIP_MS + (Uint32)i * 130 + (Uint32)j * 120, FLY_MS);
        ui_sound_later(SND_PLACE, DROP_MS - 120);
        log_add(m, VL.pts[k]);
        break;
    }
    case 3:                                     /* a good bought: it flies from the market, the coins to it */
        fx_add(FX_FLY, e.good, AT_MARKET(e.good), AT_BAR(p), 0, t, FLY_MS);
        fx_add(FX_FLY, IC_COIN, AT_BAR(p), AT_MARKET(e.good), 0, t + 60, FLY_MS);
        ui_sound(SND_TAP);
        log_add(m, e.amount);
        break;
    case 4:
        fx_add(FX_FLY, e.good, AT_BAR(p), AT_MARKET(e.good), 0, t, FLY_MS);
        fx_add(FX_FLY, IC_COIN, AT_MARKET(e.good), AT_BAR(p), 0, t + 60, FLY_MS);
        ui_sound(SND_TAP);
        log_add(m, e.amount);
        break;
    default:                                    /* the turn's end; at a round's end the workers walk home */
        if (e.round_end)
            for (int c = 0; c < VL_CELLS; c++)
                if (before.wk[c] >= 0) fx_add(FX_HOME, before.wk[c], c, home_of(before.wk[c]), 0, t, HOP_MS + 80);
        log_last = log_now;
        log_now = (turnlog){.player = S()->cur};
        ui_sound(SND_MOVE);
        break;
    }
    return 1;
}

static void relog(void)                         /* the logs after an undo or a script: rebuilt from the moves */
{
    log_now = (turnlog){.player = -1};
    log_last = (turnlog){.player = -1};
    vl_state s = G.s0;
    for (int i = 0; i < G.n; i++) {
        vl_effect e;
        int m = G.mv[i];
        vl_apply(&s, m, &e);
        if (log_now.player < 0) log_now.player = e.player;
        if (m == VL_MV_END) {
            log_last = log_now;
            log_now = (turnlog){.player = s.cur};
        } else if (log_now.n < 16) {
            log_now.m[log_now.n] = (int16_t)m;
            log_now.amount[log_now.n++] = (int8_t)(e.kind == 2 ? VL.pts[m / 64] : e.amount);
        }
    }
    if (log_now.player < 0) log_now.player = s.cur;
}

const vl_game *valley_live(void) { return &G; }

int valley_show(const char *move)
{
    int len, m = vl_parse(move, &len);
    if (m < 0 || move[len] || !vl_legal(S(), m)) return 0;
    return act(m);
}

static int undo(void)
{
    if (!vl_undo(&G)) return 0;
    calm();
    relog();
    return 1;
}

static int script(const char *moves)
{
    int ok = vl_script(&G, moves);
    calm();
    relog();
    return ok;
}

static int history(int i, char *move, int cap)
{
    if (i >= 0 && i < G.n && move && cap > 0) {
        char b[8];
        vl_name(G.mv[i], b);
        strncpy(move, b, (size_t)cap - 1);
        move[cap - 1] = 0;
    }
    return G.n;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static int people_turn(void)                    /* a person's turn, nothing moving: the marks and the input */
{
    return !S()->over && !(ui_bot_seats() >> S()->cur & 1) && !busy();
}

static uint32_t code_of(int c)
{
    const vl_state *s = S();
    if (s->hidden[c]) return TK(K_BACK, 0, 7, 0, 0, c, 0);
    int kind = s->terr[c], own = s->own[c] < 0 ? 7 : s->own[c];
    unsigned water = 0, road = 0;
    if (kind == VL_RIVER) {
        int n = 0, out = -1;
        for (int d = 0; d < 6; d++) {
            int x = vl_nb[c][d];
            if (x >= 0 && vl_in(s, x) && !s->hidden[x] && s->terr[x] == VL_RIVER) water |= 1u << d, n++;
            else if ((x < 0 || !vl_in(s, x)) && out < 0) out = d;
        }
        if (n <= 1 && out >= 0) water |= 1u << out;   /* at the valley's rim: it flows on out of it */
    }
    if (s->bld[c] == VL_ROAD)
        for (int d = 0; d < 6; d++) {
            int x = vl_nb[c][d];
            if (x >= 0 && vl_in(s, x) && s->own[x] == s->own[c]) road |= 1u << d;
        }
    return TK(kind, s->bld[c], own, water, road, c, s->gift[c] == 2);
}

static float fx_f(const fxe *e, Uint32 now) { return ((int)(now - e->t0)) / (float)e->ms; }

static const fxe *fx_find(int kind, int cell, Uint32 now)
{
    for (int i = 0; i < nfx; i++)
        if (FX[i].kind == kind && (kind == FX_HOP ? FX[i].c : FX[i].a) == cell && fx_f(&FX[i], now) < 1) return &FX[i];
    return NULL;
}

static void draw_worker(int p, float sx, float sy, float s, Uint8 alpha)
{
    pic *w = pic_get(PIC_WORKER, p, 0, (int)lroundf(0.6f * s));
    if (alpha == 255) ui_shadow_flat(sx + 0.04f * s, sy, 0.3f * s, 0.12f * s, 0.5f);
    pic_at(w, sx, sy, 1, alpha);
}

static float bscale(int k) { return k == VL_HOUSE ? 0.84f : k == VL_MILL ? 0.70f : k == VL_MANOR ? 0.76f : 0.78f; }

static void draw_building(int k, int own, float sx, float sy, float s, float lift, Uint8 alpha)
{
    pic *b = pic_get(PIC_BUILD, k, own, (int)lroundf(bscale(k) * s));
    pic_at(b, sx - 0.04f * s, sy + 0.16f * s * TILT - lift, 1, alpha);
}

/* a little badge above a land: its good and how many */
static void draw_badge(int good, int n, float sx, float sy, float s, Uint8 alpha)
{
    float d = fmaxf(0.36f * s, 0.34f * ui.unit), h = 0.8f * d;
    static const uint8_t cream[3] = {255, 246, 222}, dark[3] = {40, 28, 16};
    float y = sy - 0.62f * s, w = ui_number_w(n, h);
    float x0 = sx - (d + 0.08f * d + w) / 2;
    ui_fill(x0 - 0.12f * d, y - 0.58f * d, d + w + 0.32f * d, 1.16f * d, 40, 30, 20, (Uint8)(150 * alpha / 255));
    icon_at(good, 0, x0 + d / 2, y, d, alpha);
    ui_number_ex(x0 + d + 0.08f * d + w / 2 + 1, y + 1, h, n, dark, (Uint8)(alpha * 0.6f), 0);
    ui_number_ex(x0 + d + 0.08f * d + w / 2, y, h, n, cream, alpha, 0);
}

static int build_place(int k, int c)            /* the player to move may build k at c now */
{
    return S()->phase == VL_ACT && vl_can_pay(S(), S()->cur, k) && vl_can_build(S(), S()->cur, k, c);
}

static void draw_map(Uint32 now)
{
    SDL_Rect clip = {(int)view.x, (int)view.y, (int)view.w, (int)view.h}, was;
    SDL_bool had = SDL_RenderIsClipEnabled(ui.r);
    SDL_RenderGetClipRect(ui.r, &was);
    SDL_RenderSetClipRect(ui.r, &clip);
    const vl_state *s = S();
    float z = cam_s;
    make_flat(z);
    float sw = SQ3 * z * 1.3f + 4, shh = 2 * TILT * z * 1.3f + 4;
    for (int c = 0; c < VL_CELLS; c++)          /* the shadows on the table */
        if (vl_in(s, c)) {
            float sx, sy;
            cell_screen(c, &sx, &sy);
            ui_put_c(t_shadow, sx + 0.10f * z, sy + (THICK + 0.12f) * z, sw * 0.86f, shh * 0.86f);
        }
    for (int c = 0; c < VL_CELLS; c++) {        /* the tiles from the back, each with what stands on it */
        if (!vl_in(s, c)) continue;
        float sx, sy;
        cell_screen(c, &sx, &sy);
        if (sx < view.x - 1.5f * z || sx > view.x + view.w + 1.5f * z || sy < view.y - 0.6f * z ||
            sy > view.y + view.h + 1.6f * z)
            continue;
        const fxe *fl = fx_find(FX_FLIP, c, now);
        if (fl) {                               /* turning up: the back narrows, the face widens */
            float f = clampf(fx_f(fl, now), 0, 1), lift = 0.3f * z * sinf(ART_PI * f);
            uint32_t code = f < 0.5f ? TK(K_BACK, 0, 7, 0, 0, c, 0) : code_of(c);
            put_tile(code, sx, sy - lift, z, 255, fmaxf(0.04f, fabsf(1 - 2 * f)));
            ui_animate();
            continue;
        }
        put_tile(code_of(c), sx, sy, z, 255, 1);
        if (s->own[c] >= 0 && !s->hidden[c]) { /* the owner's rim */
            const float *pc = art_player[s->own[c]].fill;
            SDL_SetTextureColorMod(t_ring, 30, 20, 12);
            SDL_SetTextureAlphaMod(t_ring, 120);
            ui_put_c(t_ring, sx, sy + 0.02f * z, sw * 0.80f, shh * 0.80f);
            SDL_SetTextureColorMod(t_ring, (Uint8)pc[0], (Uint8)pc[1], (Uint8)pc[2]);
            SDL_SetTextureAlphaMod(t_ring, 235);
            ui_put_c(t_ring, sx, sy, sw * 0.78f, shh * 0.78f);
            SDL_SetTextureColorMod(t_ring, 255, 255, 255);
            SDL_SetTextureAlphaMod(t_ring, 255);
        }
        if (s->bld[c] >= VL_HOUSE) {
            const fxe *dr = fx_find(FX_DROP, c, now);
            float lift = 0, f = 1;
            if (dr) {
                f = clampf(fx_f(dr, now), 0, 1);
                float g = f / 0.75f;           /* falling, then a little bounce */
                lift = f < 0.75f ? 1.4f * z * (1 - g * g) : 0.06f * z * sinf(ART_PI * (f - 0.75f) / 0.25f);
                ui_animate();
            }
            int k = dr && s->bld[c] == VL_MANOR && f < 0.5f ? VL_MANOR : s->bld[c];
            draw_building(k, s->own[c], sx, sy, z, lift, (Uint8)(255 * clampf(f * 4, 0, 1)));
        }
        if (s->wk[c] >= 0 && !fx_find(FX_HOP, c, now)) draw_worker(s->wk[c], sx + 0.02f * z, sy + 0.12f * z, z, 255);
    }
    /* the marks: what the person to move may do */
    if (people_turn()) {
        float pulse = 0.5f + 0.5f * sinf(now / 260.0f);
        if (s->phase == VL_HARVEST) {
            for (int c = 0; c < VL_CELLS; c++) {
                if (!vl_harvestable(s, c, s->cur)) continue;
                float sx, sy;
                cell_screen(c, &sx, &sy);
                ui_mark_gold_a(sx, sy, 1.7f * z, c == sel ? 0.8f : 0.2f + 0.1f * pulse);
                SDL_SetTextureColorMod(t_ring, 255, 226, 130);   /* a gold rim round the lands in reach */
                SDL_SetTextureAlphaMod(t_ring, (Uint8)(c == sel ? 255 : 120 + 80 * pulse));
                ui_put_c(t_ring, sx, sy, sw * (c == sel ? 0.80f : 0.74f), shh * (c == sel ? 0.80f : 0.74f));
                SDL_SetTextureColorMod(t_ring, 255, 255, 255);
                SDL_SetTextureAlphaMod(t_ring, 255);
                if (c == sel) draw_worker(s->cur, sx, sy + 0.12f * z - 0.12f * z * pulse, z, 210);
                if (!s->hidden[c]) draw_badge(s->terr[c], vl_yield(s, c), sx, sy, z, c == sel ? 255 : 215);
            }
            if (sel >= 0 && !vl_harvestable(s, sel, s->cur)) {   /* a land looked at, out of reach */
                float sx, sy;
                cell_screen(sel, &sx, &sy);
                ui_mark_select_round(sx, sy, 1.3f * z);
            }
            ui_animate();
        } else if (mode) {
            for (int c = 0; c < VL_CELLS; c++) {
                if (!build_place(mode, c)) continue;
                float sx, sy;
                cell_screen(c, &sx, &sy);
                if (c == sel) {
                    ui_mark_gold_a(sx, sy, 1.8f * z, 0.55f + 0.2f * pulse);
                    if (mode >= VL_HOUSE) draw_building(mode, s->cur, sx, sy, z, 0.12f * z * pulse, 170);
                } else ui_mark_dot(sx, sy - 0.05f * z, fmaxf(0.34f * z, 0.3f * ui.unit));
            }
            ui_animate();
        } else if (sel >= 0) {
            float sx, sy;
            cell_screen(sel, &sx, &sy);
            ui_mark_select_round(sx, sy, 1.3f * z);
        }
    }
    /* the workers walking */
    for (int i = 0; i < nfx; i++) {
        const fxe *e = &FX[i];
        if (e->kind != FX_HOP && e->kind != FX_HOME) continue;
        float f = fx_f(e, now);
        if (f >= 1) continue;
        ui_animate();
        f = clampf(f, 0, 1);
        float ax, ay, bx, by, g = ui_ease(f);
        cell_screen(e->b, &ax, &ay);
        cell_screen(e->c, &bx, &by);
        float hop = 1.3f * z * sinf(ART_PI * f) * fminf(1, hypotf(bx - ax, by - ay) / (3 * z) + 0.3f);
        float x = ax + (bx - ax) * g, y = ay + (by - ay) * g + 0.12f * z - hop;
        draw_worker(e->a, x, y, z, (Uint8)(e->kind == FX_HOME ? 255 * clampf((1 - f) * 4, 0, 1) : 255));
    }
    if (had) SDL_RenderSetClipRect(ui.r, &was);
    else SDL_RenderSetClipRect(ui.r, NULL);
}

static void at_pos(int code, float *x, float *y)
{
    if (code >= AT_MARKET(0)) { *x = mk_x[code - AT_MARKET(0)], *y = mk_y[code - AT_MARKET(0)]; return; }
    if (code >= AT_BAR(0)) {
        int up;
        box b = ui_bar_box(code - AT_BAR(0), &up);
        *x = b.x + b.w * 0.4f, *y = b.y + b.h / 2;
        return;
    }
    cell_screen(code, x, y);
    *y -= 0.35f * cam_s;
}

static void draw_fx(Uint32 now)
{
    int keep = 0;
    for (int i = 0; i < nfx; i++) {
        fxe *e = &FX[i];
        float f = fx_f(e, now);
        if (f >= 1) continue;
        FX[keep++] = *e;
        ui_animate();
        if (f < 0) continue;
        if (e->kind == FX_FLY) {
            float ax, ay, bx, by, g = ui_ease(f);
            at_pos(e->b, &ax, &ay);
            at_pos(e->c, &bx, &by);
            float d = 0.55f * ui.unit * (1 + 0.25f * sinf(ART_PI * f));
            float x = ax + (bx - ax) * g, y = ay + (by - ay) * g - sinf(ART_PI * f) * 0.12f * hypotf(bx - ax, by - ay);
            icon_at(e->a, 0, x, y, d, (Uint8)(255 * clampf((1 - f) * 6, 0, 1)));
        } else if (e->kind == FX_POP) {
            static const uint8_t gold[3] = {255, 222, 120}, dark[3] = {40, 28, 16};
            float sx, sy, z = cam_s;
            cell_screen(e->a, &sx, &sy);
            float h = fmaxf(0.5f * ui.unit, 0.5f * z), y = sy - 0.9f * z - 0.7f * z * ui_ease(fminf(1, f * 1.4f));
            Uint8 a = (Uint8)(255 * clampf((1 - f) * 2.5f, 0, 1));
            float w = ui_number_w(e->b, h), x = sx + 0.18f * h, l = 0.2f * h, t = fmaxf(1, 0.07f * h);
            ui_number_ex(x + 1.5f, y + 1.5f, h, e->b, dark, (Uint8)(a * 0.6f), 0);
            ui_number_ex(x, y, h, e->b, gold, a, 0);
            float px = x - w / 2 - 0.32f * h;
            ui_fill(px - l + 1.5f, y - t + 1.5f, 2 * l, 2 * t, 40, 28, 16, (Uint8)(a * 0.6f));
            ui_fill(px - l, y - t, 2 * l, 2 * t, 255, 222, 120, a);
            ui_fill(px - t, y - l, 2 * t, 2 * l, 255, 222, 120, a);
        }
    }
    nfx = keep;
}

/* digits in the panel's ink (dark on the wood) */
static void ink_number(float cx, float cy, float h, int v, Uint8 a, int red)
{
    static const uint8_t redc[3] = {176, 40, 30};
    ui_number_ex(cx, cy, h, v, red ? redc : PUZ_INK, a, 0);
}

/* an icon and its number, from x; returns the x after them */
static float count_at(float x, float cy, float d, int icon, int player, int v, Uint8 a, int red)
{
    icon_at(icon, player, x + d / 2, cy, d, a);
    float h = 0.62f * d, w = ui_number_w(v, h);
    ink_number(x + d + 0.06f * d + w / 2, cy, h, v, a, red);
    return x + d + 0.06f * d + w + 0.3f * d;
}

static int market_ok(int m) { return people_turn() && S()->phase == VL_ACT && vl_legal(S(), m); }

static void draw_panel(Uint32 now)
{
    (void)now;
    const vl_state *s = S();
    float u = ui.unit;
    if (t_panel) ui_put(t_panel, panel.x, panel.y, panel.w, panel.h);
    /* the market */
    for (int g = 0; g < 3; g++) {
        icon_at(g, 0, mk_x[g], mk_y[g], mk_icon, 255);
        float pd = fminf(pip_w / VL_STOCK_MAX * 0.7f, 0.2f * u), py = mk_y[g] + 0.5f * mk_icon + 0.6f * pd;
        float px0 = mk_x[g] - mk_icon / 2;
        if (wide) py = mk_y[g] + 0.48f * mk_icon + 0.5f * pd;
        for (int i = 0; i < VL_STOCK_MAX; i++) {    /* the stock: a pip a good */
            float px = px0 + (i + 0.5f) * (pd * 1.35f);
            if (i < s->stock[g]) ui_fill(px - pd / 2, py - pd / 2, pd, pd, 120, 84, 50, 235);
            else ui_fill(px - pd / 2, py - pd / 2, pd, pd, 84, 56, 36, 70);
        }
        int bp = vl_buy_price(s, g), sp = vl_sell_price(s, g);
        int okb = market_ok(VL_MV_BUY(g)), oks = market_ok(VL_MV_SELL(g));
        ui_button_paint(mk_buy[g][0], mk_buy[g][1], mk_d, paint_market, 0, 1, UI_TINT | (okb ? 0 : UI_DIM));
        ui_button_paint(mk_sell[g][0], mk_sell[g][1], mk_d, paint_market, 1, 1, UI_TINT | (oks ? 0 : UI_DIM));
        const uint8_t *ink = ui_table_ink();     /* the buttons' own ink: cream on the dark discs, dark on the light ones */
        float nh = 0.44f * mk_d, nx = 0.12f * mk_d, ny = 0.08f * mk_d;
        if (bp) ui_number_ex(mk_buy[g][0] + nx, mk_buy[g][1] + ny, nh, bp, ink, okb ? 255 : 110, 0);
        if (sp) ui_number_ex(mk_sell[g][0] + nx, mk_sell[g][1] + ny, nh, sp, ink, oks ? 255 : 110, 0);
    }
    /* the player to move: goods, coins, workers left; the round */
    int p = s->over ? -1 : s->cur;
    box r = row_player;
    float d = fminf(0.62f * u, 0.62f * r.h), cy = r.y + r.h / 2, x = r.x;
    if (wide) d = fminf(0.62f * u, r.w / 7.2f);
    if (wide) cy = r.y + r.h * 0.2f;
    if (p >= 0) {
        float td = 1.25f * d;
        pic *w = pic_get(PIC_WORKER, p, 0, (int)td);
        ui_mark_gold_a(x + 0.4f * td, cy, 1.4f * td, 0.5f);
        pic_at(w, x + 0.4f * td, cy + 0.45f * td, 1, 255);
        x += 0.95f * td;
        for (int g = 0; g < 3; g++) {
            int red = mode && VL.cost[mode][g] > s->p[p].goods[g];
            x = count_at(x, cy, d, g, 0, s->p[p].goods[g], 255, red);
        }
        if (wide) x = r.x, cy = r.y + r.h * 0.55f;   /* a wide panel: the coins and the workers on a line below */
        x = count_at(x, cy, d, IC_COIN, 0, s->p[p].coins, 255, 0);
    }
    float wy = cy, wx = x + 0.1f * d;
    if (p >= 0)                                 /* the workers: those still to send bright */
        for (int i = 0; i < s->p[p].workers; i++) {
            pic *w = pic_get(PIC_WORKER, p, 0, (int)(0.85f * d));
            pic_at(w, wx + (i + 0.5f) * 0.55f * d, wy + 0.38f * d, 1, i < s->p[p].workers - s->p[p].placed ? 255 : 70);
        }
    float rd = fminf(0.2f * u, (r.w * (wide ? 0.6f : 0.28f)) / (s->rounds * 1.5f)), rx = r.x + r.w - s->rounds * rd * 1.5f;
    if (wide) wy = r.y + r.h * 0.9f;
    if (!s->over) {                             /* the round's first player, before the rounds */
        pic *f = pic_get(PIC_WORKER, s->start, 0, (int)(0.7f * d));
        pic_at(f, rx - 0.5f * d, wy + 0.3f * d, 1, 255);
    }
    for (int i = 0; i < s->rounds; i++) {       /* the rounds: those played, this one in gold light */
        float px = rx + (i + 0.5f) * rd * 1.5f;
        if (i == s->round && !s->over) ui_mark_gold_a(px, wy, 3 * rd, 0.9f);
        if (i < s->round || s->over) ui_fill(px - rd / 2, wy - rd / 2, rd, rd, 120, 84, 50, 235);
        else if (i == s->round) ui_fill(px - rd / 2, wy - rd / 2, rd, rd, 230, 180, 60, 255);
        else ui_fill(px - rd / 2, wy - rd / 2, rd, rd, 84, 56, 36, 80);
    }
    /* the line: a building's cost, a land's harvest, else what this turn (or the last) did */
    box b = row_info;
    float ld = fminf(0.56f * u, 0.7f * b.h), ly = b.y + b.h / 2, lx = b.x + 0.1f * u;
    int k = mode ? mode : peek;
    if (k && p >= 0) {
        if (k >= VL_HOUSE) draw_building(k, p, lx + 0.5f * ld, ly + 0.3f * ld - 0.16f * ld * TILT, 1.15f * ld, 0, 255);
        if (k == VL_ROAD) {                     /* a road: a strip of earth */
            static const uint8_t dirt[3] = {196, 168, 120};
            pzk_line(lx, ly + 0.2f * ld, lx + ld, ly - 0.2f * ld, 0.28f * ld, 1.5f, (pzk_rgba){dirt[0], dirt[1], dirt[2], 255},
                     0);
        }
        lx += 1.3f * ld;
        for (int g = 0; g < 3; g++)
            if (VL.cost[k][g]) lx = count_at(lx, ly, ld, g, 0, VL.cost[k][g], 255, VL.cost[k][g] > s->p[p].goods[g]);
        if (VL.pts[k]) {
            lx += 0.2f * ld;
            lx = count_at(lx, ly, ld, IC_STAR, 0, VL.pts[k], 255, 0);
        }
        int left = vl_left(s, p, k);
        if (left >= 0) {                        /* the pieces still in hand */
            pic *w = pic_get(PIC_BUILD, k == VL_ROAD ? VL_HOUSE : k, p, (int)(0.55f * ld));
            float bx = b.x + b.w - 1.4f * ld;
            if (k >= VL_HOUSE) pic_at(w, bx, ly + 0.3f * ld, 1, 200);
            ink_number(bx + 0.7f * ld, ly, 0.6f * ld, left, 220, !left);
        }
    } else if (sel >= 0 && p >= 0 && !s->hidden[sel] && s->terr[sel] < VL_RIVER) {   /* a land: its harvest now */
        pic *w = pic_get(PIC_WORKER, p, 0, (int)ld);
        pic_at(w, lx + 0.3f * ld, ly + 0.47f * ld, 1, 255);
        lx += 0.8f * ld;
        lx = count_at(lx, ly, ld, s->terr[sel], 0, vl_yield(s, sel), 255, 0);
        for (int dd = 0; dd < 6; dd++) {        /* a workshop of another's beside it: a coin for its owner */
            int n = vl_nb[sel][dd];
            if (n >= 0 && s->bld[n] == vl_shop_of(s->terr[sel]) && s->own[n] != p && s->own[n] >= 0) {
                pic *o = pic_get(PIC_WORKER, s->own[n], 0, (int)(0.8f * ld));
                pic_at(o, lx + 0.3f * ld, ly + 0.4f * ld, 1, 255);
                lx = count_at(lx + 0.6f * ld, ly, 0.8f * ld, IC_COIN, 0, VL.rent, 255, 0);
            }
        }
    } else {
        const turnlog *L = log_now.n ? &log_now : &log_last;
        if (L->player >= 0 && L->n) {
            pic *w = pic_get(PIC_WORKER, L->player, 0, (int)ld);
            pic_at(w, lx + 0.3f * ld, ly + 0.47f * ld, 1, 255);
            lx += 0.85f * ld;
            for (int i = 0; i < L->n && lx < b.x + b.w - ld; i++) {
                int m = L->m[i];
                float sd = 0.8f * ld;
                if (m < VL_CELLS)
                    lx = count_at(lx, ly, sd, G.s.terr[m] < VL_RIVER ? G.s.terr[m] : IC_COIN, 0, L->amount[i], 255, 0);
                else if (m == VL_REST) lx = count_at(lx, ly, sd, IC_COIN, 0, 1, 255, 0);
                else if (m >= VL_MV_SELL(0) || m >= VL_MV_BUY(0)) {
                    int sell = m >= VL_MV_SELL(0), g = m - (sell ? VL_MV_SELL(0) : VL_MV_BUY(0));
                    pic *mk = pic_get(PIC_MARKET, sell, 0, (int)(0.7f * sd));
                    if (mk && mk->t) {
                        SDL_SetTextureColorMod(mk->t, PUZ_INK[0], PUZ_INK[1], PUZ_INK[2]);
                        pic_at(mk, lx + 0.35f * sd, ly - 0.05f * sd, 1, 255);
                        SDL_SetTextureColorMod(mk->t, 255, 255, 255);
                    }
                    icon_at(g, 0, lx + 0.65f * sd, ly, sd, 255);
                    lx += 1.45f * sd;
                } else {
                    int kk = m / 64;
                    float by = ly + 0.35f * sd - 0.16f * sd * TILT;
                    if (kk >= VL_HOUSE) draw_building(kk, L->player, lx + 0.5f * sd, by, 1.1f * sd, 0, 255);
                    else {
                        static const uint8_t dirt[3] = {176, 140, 92};
                        pzk_line(lx + 0.1f * sd, ly + 0.25f * sd, lx + 0.9f * sd, ly - 0.25f * sd, 0.26f * sd, 1.5f,
                                 (pzk_rgba){dirt[0], dirt[1], dirt[2], 255}, 0);
                    }
                    lx += 1.15f * sd;
                }
            }
        }
    }
}

/* the tools: a building's picture in the player's colour (kind = building tool + 8 * player) */
static void paint_tool(int kind, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    int t = kind & 7, p = kind >> 3, k = TOOL_BLD[t < 6 ? t : 0];
    float s = (float)size;
    if (k == VL_ROAD) {                         /* a road of earth winding up, a milestone in the player's colour */
        static const uint8_t edge[3] = {140, 112, 74}, dirt[3] = {210, 182, 134};
        pzk_stroke(rgba, size, size, 0.14f * s, 0.86f * s, 0.5f * s, 0.52f * s, 0.15f * s, edge, 1);
        pzk_stroke(rgba, size, size, 0.5f * s, 0.52f * s, 0.86f * s, 0.18f * s, 0.15f * s, edge, 1);
        pzk_stroke(rgba, size, size, 0.14f * s, 0.86f * s, 0.5f * s, 0.52f * s, 0.11f * s, dirt, 1);
        pzk_stroke(rgba, size, size, 0.5f * s, 0.52f * s, 0.86f * s, 0.18f * s, 0.11f * s, dirt, 1);
        float c[3], d[3];
        roof_cols(p, c, d);
        oval_fill(rgba, size, size, 0.74f * s, 0.62f * s, 0.11f * s, 0.15f * s, c, 1);
        return;
    }
    float bs = (k == VL_MILL ? 0.58f : k == VL_MANOR ? 0.62f : 0.72f) * s;
    paint_building(rgba, size, size, 0.42f * s, 0.84f * s, bs, k, p);
}

static void draw_tools(void)
{
    const vl_state *s = S();
    int p = s->cur, me = people_turn();
    for (int i = 0; i < 6; i++) {
        int k = TOOL_BLD[i], how = 0;
        if (mode == k || peek == k) how = UI_ON;
        else if (me && s->phase == VL_ACT && sel >= 0 && !mode) how = build_place(k, sel) ? UI_LIT : UI_DIM;
        else if (!me || s->phase != VL_ACT || !vl_can_pay(s, p, k)) how = UI_DIM;
        else {
            int any = 0;
            for (int c = 0; c < VL_CELLS && !any; c++) any = vl_can_build(s, p, k, c);
            if (!any) how = UI_DIM;
        }
        ui_tool_paint(i, paint_tool, i + 8 * p, 0.86f, how | UI_IVORY);
    }
    int ok = 0;
    if (me) {
        if (s->phase == VL_HARVEST) {
            int mv[VL_MAXMOVES];
            ok = (sel >= 0 && vl_harvestable(s, sel, p)) || (vl_moves(s, mv) == 1 && mv[0] == VL_REST);
        } else ok = 1;
    }
    ui_tool(T_OK, ICON_TICK, ok ? UI_LIT : UI_DIM);
}

/* the final table in the end card: each player's buildings' points, coins' points and total, the best first */
static void draw_final(void)
{
    const vl_state *s = S();
    float u = ui.unit, rh = 0.85f * u, w = 6.4f * u, h = s->players * rh + 0.3f * u;
    box r = ui_end_room(w, h);
    if (r.w <= 0) return;
    float k = fminf(1, fminf(r.w / w, r.h / h));
    rh *= k;
    int order[VL_PLAYERS];
    for (int i = 0; i < s->players; i++) order[i] = i;
    for (int i = 1; i < s->players; i++)
        for (int j = i; j > 0 && vl_score(s, order[j]) * 1000 + s->p[order[j]].coins >
                                     vl_score(s, order[j - 1]) * 1000 + s->p[order[j - 1]].coins; j--) {
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    unsigned win = vl_winners(s);
    static const uint8_t cream[3] = {250, 242, 222};
    const uint8_t *ink = ui_ink_dark() ? PUZ_INK : cream;
    for (int i = 0; i < s->players; i++) {
        int p = order[i];
        float y = r.y + (r.h - s->players * rh) / 2 + (i + 0.5f) * rh, x = r.x + (r.w - w * k) / 2, d = 0.62f * rh;
        if (win >> p & 1) ui_mark_gold_a(x + 0.5f * rh, y, 1.3f * rh, 0.8f);
        pic *t = pic_get(PIC_WORKER, p, 0, (int)(0.9f * rh));
        pic_at(t, x + 0.5f * rh, y + 0.42f * rh, 1, 255);
        x += 1.2f * rh;
        int bp = s->p[p].pp, cp = s->p[p].coins / VL.coins_per_pt;
        icon_at(IC_STAR, 0, x + d / 2, y, d, 255);
        ui_number_ex(x + 1.6f * d, y, 0.62f * d, bp, ink, 255, 0);
        x += 2.6f * d;
        icon_at(IC_COIN, 0, x + d / 2, y, d, 255);
        ui_number_ex(x + 1.6f * d, y, 0.62f * d, cp, ink, 255, 0);
        x += 2.8f * d;
        ui_number_ex(x + 0.8f * d, y, 0.95f * d, vl_score(s, p), ink, 255, 0);
    }
}

static void draw(void)
{
    Uint32 now = ui_ms();
    frame_no++;
    painted = 0;
    cam_at(now);
    draw_map(now);
    draw_panel(now);
    draw_fx(now);
    draw_tools();
    if (ui_over()) draw_final();
}

static void bar(int player, box b)
{
    const vl_state *s = S();
    float d = 0.6f * ui.unit;
    ui_count c[5] = {
        {icon_tex(IC_STAR, 0, d), vl_score(s, player)}, {icon_tex(VL_WOOD, 0, d), s->p[player].goods[0]},
        {icon_tex(VL_GRAIN, 0, d), s->p[player].goods[1]}, {icon_tex(VL_STONE, 0, d), s->p[player].goods[2]},
        {icon_tex(IC_COIN, 0, d), s->p[player].coins},
    };
    if (b.w >= 5.4f * ui.unit) { ui_bar_fit(b, c, 5, 0); return; }
    c[1] = (ui_count){icon_tex(IC_GOODS, 0, d), s->p[player].goods[0] + s->p[player].goods[1] + s->p[player].goods[2]};
    c[2] = c[4];
    ui_bar_fit(b, c, 3, 0);   /* a narrow bar: the points, all the goods, the coins */
}

/* ---- the pointer: a tap chooses, one finger drags the valley, two pinch ---------------------------------------- */
static int only_place(int k)                    /* the one place k may go (-1: none or several) */
{
    int at = -1;
    for (int c = 0; c < VL_CELLS; c++)
        if (build_place(k, c)) {
            if (at >= 0) return -1;
            at = c;
        }
    return at;
}

static void tap_cell(int c)
{
    const vl_state *s = S();
    if (!people_turn()) return;
    if (c < 0) { sel = -1; ui_animate(); return; }
    if (s->phase == VL_HARVEST) {
        peek = 0;
        if (!vl_harvestable(s, c, s->cur)) {   /* out of reach or taken: no, but its harvest shows in the panel */
            ui_sound(SND_DENY);
            sel = !s->hidden[c] && s->terr[c] < VL_RIVER ? c : -1;
            ui_animate();
            return;
        }
        if (sel == c) act(c);
        else { sel = c; ui_sound(SND_TAP); ui_animate(); }
        return;
    }
    if (mode) {
        if (!build_place(mode, c)) { ui_sound(SND_DENY); return; }
        if (sel == c) act(VL_MV_BUILD(mode, c));
        else { sel = c; ui_sound(SND_TAP); ui_animate(); }
        return;
    }
    sel = sel == c ? -1 : c;                    /* no building chosen: the place is marked, a tool then builds there */
    ui_sound(SND_TAP);
    ui_animate();
}

static void tool(int t)
{
    const vl_state *s = S();
    if (!people_turn()) return;
    if (t == T_OK) {
        if (s->phase == VL_HARVEST) {
            int mv[VL_MAXMOVES];
            if (sel >= 0 && vl_harvestable(s, sel, s->cur)) act(sel);
            else if (vl_moves(s, mv) == 1 && mv[0] == VL_REST) act(VL_REST);
            else ui_sound(SND_DENY);
        } else if (mode && sel >= 0 && build_place(mode, sel)) act(VL_MV_BUILD(mode, sel));
        else act(VL_MV_END);
        return;
    }
    int k = TOOL_BLD[t];
    if (s->phase != VL_ACT) {                   /* not now: its cost shows */
        peek = peek == k ? 0 : k;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (mode == k) {
        if (sel >= 0 && build_place(k, sel)) act(VL_MV_BUILD(k, sel));
        else { mode = 0; ui_sound(SND_TAP); }
        ui_animate();
        return;
    }
    if (sel >= 0 && mode == 0 && build_place(k, sel)) {   /* a place marked first: build there at once */
        act(VL_MV_BUILD(k, sel));
        return;
    }
    mode = k;
    peek = 0;
    if (sel >= 0 && !build_place(k, sel)) sel = -1;
    if (sel < 0) sel = only_place(k);
    ui_sound(SND_TAP);
    ui_animate();
}

static int market_hit(float x, float y)         /* a buy (0..2) or a sell (3..5) button, else -1 */
{
    for (int g = 0; g < 3; g++) {
        if (ui_button_hit(mk_buy[g][0], mk_buy[g][1], mk_d, x, y)) return g;
        if (ui_button_hit(mk_sell[g][0], mk_sell[g][1], mk_d, x, y)) return 3 + g;
    }
    return -1;
}

static void market(int b)
{
    int m = b < 3 ? VL_MV_BUY(b) : VL_MV_SELL(b - 3);
    if (!people_turn()) return;
    if (!market_ok(m)) { ui_sound(SND_DENY); return; }
    act(m);
}

#define MAX_FINGERS 2
static struct { int id; float x, y; } fg[MAX_FINGERS];
static int nfg, moved, pinching, mouse_down, mk_down = -1;
static float g_x0, g_y0, g_cx0, g_cy0, g_d0, g_s0, g_wx, g_wy;

static float slop(void) { return 0.016f * fminf(ui.w, ui.h); }
static int in_box(box b, float x, float y) { return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h; }

static void tap_at(float x, float y)
{
    if (in_box(panel, x, y)) {
        if (!people_turn()) return;
        if (in_box(row_info, x, y) || in_box(row_player, x, y)) { peek = 0; ui_animate(); }
        return;
    }
    if (in_box(view, x, y)) tap_cell(pick(x, y));
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

static int down_on_button(float x, float y)
{
    tool_down = ui_tool_hit(x, y);
    if (tool_down >= 0) { ui_animate(); return 1; }
    mk_down = market_hit(x, y);
    return mk_down >= 0;
}

static int up_on_button(float x, float y)
{
    if (tool_down >= 0) {
        int t = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) == t) tool(t);
        return 1;
    }
    if (mk_down >= 0) {
        int b = mk_down;
        mk_down = -1;
        if (market_hit(x, y) == b) market(b);
        return 1;
    }
    return 0;
}

static void press(float x, float y)
{
    if (down_on_button(x, y)) return;
    mouse_down = 1;
    drag_begin(x, y);
}

static void motion(float x, float y)
{
    if (mouse_down) drag_move(x, y);
}

static void release(float x, float y)
{
    if (up_on_button(x, y)) return;
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
        if (!nfg && down_on_button(x, y)) return;
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
        if (phase == FINGER_UP) up_on_button(x, y);
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

/* the keys: the arrows move the cursor (the cell chosen; at first the player's home), Return or space the tick, 1..6
 * the buildings, + and - zoom, 0 the whole valley */
static void key(SDL_Keycode k)
{
    const vl_state *s = S();
    if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN) {
        if (!people_turn()) return;
        int c = sel >= 0 ? sel : home_of(s->cur);   /* the cursor starts at the player's home */
        int d0 = k == SDLK_RIGHT ? 0 : k == SDLK_LEFT ? 3 : k == SDLK_UP ? 5 : 1;   /* up: NE, else NW; down: SE, else SW */
        int d1 = k == SDLK_UP ? 4 : k == SDLK_DOWN ? 2 : d0, n = vl_nb[c][d0];
        if (n < 0 || !vl_in(s, n)) n = vl_nb[c][d1];
        if (n < 0 || !vl_in(s, n)) { ui_sound(SND_DENY); return; }
        sel = n;
        ui_sound(SND_TAP);
        ui_animate();
    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
        if (s->phase == VL_HARVEST && sel >= 0 && vl_harvestable(s, sel, s->cur)) tap_cell(sel), tap_cell(sel);
        else tool(T_OK);
    } else if (k >= SDLK_1 && k <= SDLK_6) tool((int)(k - SDLK_1));
    else if (k == SDLK_PLUS || k == SDLK_EQUALS || k == SDLK_KP_PLUS || k == SDLK_PAGEUP) {
        user_cam = 1;
        cam_to(cam_go ? cam_tx : cam_x, cam_go ? cam_ty : cam_y, (cam_go ? cam_ts : cam_s) * 1.3f);
    } else if (k == SDLK_MINUS || k == SDLK_KP_MINUS || k == SDLK_PAGEDOWN) {
        user_cam = 1;
        cam_to(cam_go ? cam_tx : cam_x, cam_go ? cam_ty : cam_y, (cam_go ? cam_ts : cam_s) / 1.3f);
    } else if (k == SDLK_0 || k == SDLK_HOME) {
        user_cam = 0;
        cam_home(0);
    }
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* a few tiles painted into a picture size x size, fitted: cells (axial q, r), their codes, a building on each */
typedef struct { int8_t q, r; uint32_t code; int bld, own; } little;

static void paint_little(const little *L, int n, int size, uint8_t *rgba, float most)
{
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int i = 0; i < n; i++) {
        float wx = SQ3 * (L[i].q + 0.5f * L[i].r), wy = 1.5f * L[i].r;
        x0 = fminf(x0, wx - AP), x1 = fmaxf(x1, wx + AP);
        y0 = fminf(y0, (wy - 1) * TILT - 0.8f), y1 = fmaxf(y1, (wy + 1) * TILT + THICK);
    }
    float rp = fminf(fminf(0.94f * size / (x1 - x0), 0.94f * size / (y1 - y0)), most * size);
    float ox = size / 2.0f - rp * (x0 + x1) / 2, oy = size / 2.0f - rp * (y0 + y1) / 2;
    for (int i = 0; i < n; i++) {               /* (in rows from the back: as given) */
        float wx = SQ3 * (L[i].q + 0.5f * L[i].r), wy = 1.5f * L[i].r;
        float cx = ox + wx * rp, cy = oy + wy * TILT * rp;
        paint_tile(rgba, size, size, cx, cy, rp, L[i].code);
        if (L[i].bld >= VL_HOUSE)
            paint_building(rgba, size, size, cx - 0.04f * rp, cy + 0.16f * rp * TILT, bscale(L[i].bld) * rp,
                           L[i].bld, L[i].own);
        if (L[i].bld < 0) paint_worker(rgba, size, size, cx - 0.25f * rp, cy - 0.38f * rp, 0.5f * rp, L[i].own);
    }
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    const little L[] = {
        {0, -1, TK(K_FOREST, 0, 7, 0, 0, 3, 0), 0, 0},
        {1, -1, TK(variant == 2 ? K_HILL : K_BACK, 0, 7, 0, 0, 4, 0), 0, 0},
        {-1, 0, TK(K_FIELD, VL_MILL, 2, 0, 0, 5, 0), VL_MILL, 2},
        {0, 0, TK(K_RIVER, 0, 7, 1u << 0 | 1u << 4, 0, 6, 0), 0, 0},
        {1, 0, TK(K_FIELD, 0, 7, 0, 0, 7, 0), -1, 3},
        {-1, 1, TK(K_HILL, VL_HOUSE, 5, 0, 0, 8, 0), VL_HOUSE, 5},
        {0, 1, TK(K_FOREST, VL_SAW, 3, 0, 0, 9, 0), VL_SAW, 3},
    };
    paint_little(L, variant == 1 ? 5 : 7, size, rgba, 0.3f);
}

static void token(int player, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    paint_worker(rgba, size, size, 0.08f * size, 0.04f * size, 0.88f * size, player);
}

/* the bars: two players at the bottom and the top; three or four by their homes' sides of the valley */
static void home(int player, float *fx, float *fy)
{
    static const float AT[5][4][2] = {
        {{0}}, {{0}}, {{1, 1}, {0, 0}}, {{0.5f, 1}, {0, 0}, {1, 0}}, {{1, 1}, {0, 1}, {0, 0}, {1, 0}},
    };
    int n = ui.players >= 2 && ui.players <= 4 ? ui.players : 2, p = player < n ? player : 0;
    *fx = AT[n][p][0];
    *fy = AT[n][p][1];
}

static const game valley_game = {
    .id = "valley", .order = 236, .variants = VL_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners, .key = key, .finger = finger,
    .players = 1u << 2 | 1u << 3 | 1u << 4, .home = home, .fill = 1, .tools = TOOLS, .history = history,
};
REGISTER_GAME(valley_game)
