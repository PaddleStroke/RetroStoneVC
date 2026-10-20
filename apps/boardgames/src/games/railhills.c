/*
 * Valley Rails in RetroStone Board, for 2 to 4 players (people and bots): rail lines laid hex by hex across an island
 * of meadows, forests, fields, hills, mountains, a river and lakes, seen as Hex Landscape's diorama (raised tiles at a
 * gentle tilt, painted by code: pines, wheat, houses, the river shimmering), each player's track in their colour. The
 * rules: railhills_rules.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The screen: the map (drag to pan, pinch or the magnifier to zoom), and a wooden tray (under it upright, beside it on
 * a wide screen) with the deck, the two public lines and the hand of the player whose view it is (a contract is a card:
 * the island's shape, its two towns, its value in a coin). Tap a hex: it is chosen, its actions float over it (lay
 * track, with its cost; lay it blind on a face-down hex; survey); tap it again for the first. Tap a contract: its towns
 * glow and its route shows (the coins of rent on it); tap it again to deliver it once it glows. The deck draws three
 * contracts at the turn's start; keep at least one (tap to choose, the tick to keep). The tick tool ends the turn. Keys:
 * the arrows choose a hex, Return lays track there, S surveys it, D draws, 1..8 a contract (again: deliver), Space ends
 * the turn, + and - zoom, 0 the whole map.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cards_table.h"
#include "games/endtable.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/railhills.h"
#include "shell.h"

#define SQ3 1.7320508f
#define AP 0.8660254f                           /* a hex's apothem (its radius 1) */
#define TILT 0.78f                              /* the view's tilt: the map's depth squashed */
#define THICK 0.20f                             /* a tile's side, of its radius */
#define OVER 0.70f                              /* room above a tile's picture for its trees, roofs and peaks */
#define CAM_MS 380
#define LAY_MS 420
#define FLIP_MS 420
#define STEP_MS 110                             /* a train: a hex */
#define REST_MS 260                             /* after each move: the bots' pace */
#define VARIANTS 2
enum { T_DONE, T_ZOOM, TOOLS };

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static rh_state G;
static int variant_now, players_now = 2;
static uint32_t seed_now;
#define MAXM 1600
static int mv[MAXM], nmv, undo_floor;           /* the moves since the start; undo never below the floor */
static int serial;

/* the screen */
static box room, view, tray;
static int sel = -1, selc = -1;                 /* the hex chosen; the contract chosen (hand slot, or 100 + public) */
static unsigned keep_mask;                      /* the offered contracts chosen to keep */
static Uint32 sel_t0, rest_until;

/* animations */
enum { FX_LAY, FX_FLIP, FX_FAIL, FX_POP, FX_TRAIN, FX_COIN, FX_PUFF, FX_OFFER };
typedef struct { int kind, cell, value, player; Uint32 t0, ms; float x, y; } fxe;
#define MAX_FX 64
static fxe FX[MAX_FX];
static int nfx;
static int train[RH_MAXC], ntrain, train_player = -1;
static Uint32 train_t0;
static uint32_t lay_old[RH_MAXC];               /* a hex's picture before the move that animates it */

/* the last turn's summary: who, track laid, contracts delivered, drawn, coins won */
typedef struct { int player, built, delivered, drew, coins, surveyed, failed; Uint32 t0; } summary;
static summary now_sum, last_sum;

/* the camera: the world's point in the middle of the view, pixels a world unit (a hex's radius) */
static float cam_x, cam_y, cam_s = 40, cam_ex, cam_ey, cam_es, cam_tx, cam_ty, cam_ts;
static Uint32 cam_t0;
static int cam_go, user_cam, zoomed;

/* ---- painting (Hex Landscape's way: a tile painted at the size it shows, kept) -------------------------------- */
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

static void shade_fill(uint8_t *o, int W, int H, float cx, float cy, float rx, float ry, float a)
{
    for (int y = (int)(cy - ry - 1) < 0 ? 0 : (int)(cy - ry - 1); y < H && y <= (int)(cy + ry + 1); y++)
        for (int x = (int)(cx - rx - 1) < 0 ? 0 : (int)(cx - rx - 1); x < W && x <= (int)(cx + rx + 1); x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, e = dx * dx + dy * dy;
            if (e < 1) px_over(o + 4 * ((size_t)y * W + x), 20, 30, 10, a * (1 - e) * (1 - e));
        }
}

/* ---- a tile's river and rails: curves through it, from a mask of edges ------------------------------------------ */
typedef struct { float p[24][2]; int n; } curve;
typedef struct { curve c[6]; int nc, ends, pond; float pond_r; } paths;

static void edge_mid(int d, float k, float *u, float *v)
{
    float a = d * ART_PI / 3;
    *u = k * AP * cosf(a);
    *v = k * AP * sinf(a);
}

static void make_paths(int mask, paths *P, float pond_r)
{
    int e[6], n = 0;
    memset(P, 0, sizeof *P);
    for (int d = 0; d < 6; d++)
        if (mask >> d & 1) e[n++] = d;
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
    for (int i = 0; i < n; i++) {               /* to the middle: a pond, a junction, a buffer stop */
        float au, av;
        edge_mid(e[i], 1.1f, &au, &av);
        curve *c = &P->c[P->nc++];
        c->n = 2;
        c->p[0][0] = au, c->p[0][1] = av;
        c->p[1][0] = 0, c->p[1][1] = 0;
    }
    P->pond = pond_r > 0;
    P->pond_r = pond_r;
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

/* a tile's picture code: its land, the river's edges, the rails' edges and owner, a hamlet, a variation */
#define C_TERR(c) ((int)((c) & 15))
#define C_RIV(c) ((int)((c) >> 4 & 63))
#define C_RAIL(c) ((int)((c) >> 10 & 63))
#define C_OWN(c) ((int)((c) >> 16 & 7))
#define C_HAM(c) ((int)((c) >> 19 & 1))
#define C_VAR(c) ((c) >> 20 & 255)
#define T_BACK 15
static uint32_t code_make(int terr, int riv, int rail, int own, int ham, int var)
{
    return (uint32_t)terr | (uint32_t)riv << 4 | (uint32_t)rail << 10 | (uint32_t)(own & 7) << 16 |
           (uint32_t)ham << 19 | (uint32_t)(var & 255) << 20;
}

static const float GROUND[RH_TERRAINS][3] = {
    {0, 0, 0}, {132, 184, 82}, {78, 124, 60}, {228, 192, 98}, {146, 168, 84}, {132, 184, 82}, {152, 142, 128},
    {132, 184, 82}, {200, 182, 140},
};

static float hill_h(uint32_t var, float u, float v)   /* a hill's height: two or three soft humps */
{
    float h = 0;
    for (int k = 0; k < 3; k++) {
        float cu = (hf(var, k * 3 + 1) - 0.5f) * 0.8f, cv = (hf(var, k * 3 + 2) - 0.5f) * 0.7f;
        float r = 0.30f + 0.16f * hf(var, k * 3 + 3), d = hypotf(u - cu, v - cv) / r;
        if (d < 1) h += (1 - d * d) * (1 - d * d) * (k == 0 ? 1.0f : 0.7f);
    }
    return h;
}

static void ground_col(int t, uint32_t var, float u, float v, int x, int y, float *c)
{
    const float *g = GROUND[t];
    float k = 1;
    switch (t) {
    case RH_FOREST:
        k = 0.94f + 0.16f * art_noise(x / 2, y / 2) + 0.06f * art_noise(x, y);
        break;
    case RH_FIELD: {                            /* rows of wheat */
        float a = (var % 3 * 60 + 30) * ART_PI / 180, s = sinf((u * cosf(a) + v * sinf(a)) * 34);
        k = 0.92f + 0.09f * s + 0.08f * art_noise(x, y);
        break;
    }
    case RH_TOWN:
        k = 0.95f + 0.10f * art_noise(x / 2, y / 3) + 0.05f * art_noise(x, y);
        break;
    case RH_HILL: {                             /* humps lit from the upper left */
        float e = 0.02f, h = hill_h(var, u, v), hu = (hill_h(var, u + e, v) - h) / e, hv = (hill_h(var, u, v + e) - h) / e;
        k = clampf(0.92f + 0.16f * h - 0.022f * (0.7f * hu + 1.0f * hv), 0.72f, 1.22f) + 0.05f * art_noise(x / 2, y / 2);
        break;
    }
    case RH_MOUNTAIN:
        k = 0.90f + 0.18f * art_noise(x / 3, y / 2) + 0.08f * art_noise(x, y);
        break;
    default:
        k = 0.97f + 0.10f * art_noise(x / 3, y / 3) + 0.06f * art_noise(x, y);
        break;
    }
    for (int i = 0; i < 3; i++) c[i] = g[i] * k;
    if (t == RH_MEADOW || t == RH_RIVER) {      /* little flowers */
        uint32_t h = hsh((uint32_t)(u * 40 + 400), (uint32_t)(v * 40 + 400));
        if ((h & 255) < 5) {
            float fl[3][3] = {{250, 248, 236}, {250, 220, 90}, {236, 150, 170}};
            int j = h >> 8 & 3;
            if (j < 3) for (int i = 0; i < 3; i++) c[i] = fl[j][i];
        }
    }
}

/* ---- a tile's things standing up --------------------------------------------------------------------------------- */
enum { OB_PINE, OB_TREE, OB_HOUSE, OB_STOOK, OB_BUSH, OB_STOP, OB_PEAK, OB_ROCK, OB_TOWER, OB_REED, OB_MOUND };
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
    case OB_HOUSE:
    case OB_TOWER: {
        static const float wall[3] = {242, 232, 212}, side[3] = {204, 190, 166}, door[3] = {112, 72, 46},
                           win[3] = {66, 76, 102};
        static const float roofs[3][2][3] = {{{196, 78, 54}, {150, 56, 40}}, {{176, 104, 62}, {132, 76, 46}},
                                              {{104, 112, 128}, {74, 80, 96}}};
        const float(*roof)[3] = roofs[t->var % 3];
        float s = h, tall = t->kind == OB_TOWER ? 0.55f * s : 0;
        shade_fill(o, W, H, x + 0.22f * s, y + 0.02f * s, 0.62f * s, 0.16f * s, 0.32f);
        rect_fill(o, W, H, x - 0.40f * s, y - 0.50f * s - tall, x + 0.14f * s, y, wall, 1);
        rect_fill(o, W, H, x + 0.14f * s, y - 0.48f * s - tall, x + 0.42f * s, y - 0.02f * s, side, 1);
        if (t->kind == OB_TOWER) {              /* a tower with a pointed roof: the town's hall */
            static const float slate[2][3] = {{92, 98, 116}, {62, 66, 82}};
            const float sp[] = {x - 0.44f * s, y - 0.48f * s - tall, x + 0.46f * s, y - 0.48f * s - tall,
                                x + 0.0f * s, y - 1.28f * s - tall};
            poly_fill(o, W, H, sp, 3, slate[0], slate[1], x + 0.02f * s, 1);
            rect_fill(o, W, H, x - 0.20f * s, y - 0.86f * s, x - 0.04f * s, y - 0.70f * s, win, 1);
            rect_fill(o, W, H, x - 0.24f * s, y - 0.30f * s, x - 0.08f * s, y, door, 1);
            break;
        }
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
    case OB_PEAK: {                             /* a rocky peak, its snow cap */
        static const float lit[3] = {168, 160, 150}, dark[3] = {104, 98, 94}, snow[3] = {246, 246, 250},
                           snowd[3] = {206, 212, 226};
        float w = 0.62f * h;
        shade_fill(o, W, H, x + 0.25f * h, y + 0.02f * h, 0.72f * w, 0.14f * h, 0.36f);
        const float pk[] = {x - w, y, x + w, y, x + 0.06f * w, y - 1.0f * h};
        poly_fill(o, W, H, pk, 3, lit, dark, x + 0.06f * w, 1);
        const float cap[] = {x - 0.30f * w, y - 0.70f * h, x + 0.06f * w, y - 1.0f * h, x + 0.38f * w, y - 0.70f * h,
                             x + 0.18f * w, y - 0.62f * h, x + 0.02f * w, y - 0.72f * h, x - 0.14f * w, y - 0.62f * h};
        if (t->var & 1) poly_fill(o, W, H, cap, 6, snow, snowd, x + 0.06f * w, 1);
        break;
    }
    case OB_MOUND: {                            /* a grassy hump, lit from the upper left */
        static const float g[3] = {146, 170, 84};
        float rx = 0.50f * h, ry = 0.36f * h;
        shade_fill(o, W, H, x + 0.14f * h, y + 0.02f * h, 1.05f * rx, 0.10f * h, 0.30f);
        for (int yy = (int)(y - ry - 1) < 0 ? 0 : (int)(y - ry - 1); yy < H && yy <= (int)(y + 1); yy++)
            for (int xx = (int)(x - rx - 1) < 0 ? 0 : (int)(x - rx - 1); xx < W && xx <= (int)(x + rx + 1); xx++) {
                float dx = (xx + 0.5f - x) / rx, dy = (yy + 0.5f - y) / ry, e = dx * dx + dy * dy;
                if (e >= 1 || dy > 0) continue;
                float cov = clampf((1 - sqrtf(e)) * rx + 0.5f, 0, 1) * clampf(-dy * ry + 0.5f, 0, 1);
                float k = 0.88f - 0.30f * dx - 0.16f * dy + 0.05f * art_noise(xx, yy);
                px_over(o + 4 * ((size_t)yy * W + xx), g[0] * k, g[1] * k, g[2] * k, cov);
            }
        break;
    }
    case OB_ROCK: {
        static const float r[3] = {150, 144, 136};
        shade_fill(o, W, H, x + 0.06f * h, y + 0.01f * h, 0.14f * h, 0.05f * h, 0.3f);
        ball_fill(o, W, H, x, y - 0.06f * h, 0.10f * h, r, 1);
        break;
    }
    case OB_REED: {
        static const float g[3] = {94, 132, 60}, brown[3] = {120, 84, 50};
        for (int k = -1; k <= 1; k++) {
            rect_fill(o, W, H, x + k * 0.05f * h - 0.012f * h, y - (0.30f - 0.06f * k * k) * h, x + k * 0.05f * h + 0.012f * h,
                      y, g, 1);
            rect_fill(o, W, H, x + k * 0.05f * h - 0.02f * h, y - (0.30f - 0.06f * k * k) * h, x + k * 0.05f * h + 0.02f * h,
                      y - (0.22f - 0.06f * k * k) * h, brown, 1);
        }
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

/* the rails' bed in a player's colour (lightened on the dark ones so the steel and sleepers still show) */
static void bed_colour(int owner, float n, float *c)
{
    const float *f = art_player[owner < 0 ? 0 : owner].fill;
    float g[3] = {164 * n, 152 * n, 132 * n}, k = 0.88f;
    for (int i = 0; i < 3; i++) c[i] = g[i] * (1 - k) + f[i] * k * n;
}

/* a tile painted over a picture W x H, its middle at (ax, ay), rp pixels its radius */
static void paint_tile(uint8_t *o, int W, int H, float ax, float ay, float rp, uint32_t code)
{
    float dp = THICK * rp, sy = rp * TILT;
    int terr = C_TERR(code), back = terr == T_BACK;
    uint32_t var = C_VAR(code);
    int x0 = (int)floorf(ax - AP * rp - 1), x1 = (int)ceilf(ax + AP * rp + 1);
    int y0 = (int)floorf(ay - sy - 1), y1 = (int)ceilf(ay + sy + dp + 1);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    paths river, rail;
    make_paths(back ? 0 : C_RIV(code), &river, terr == RH_LAKE ? 0.62f : 0.30f);
    if (terr == RH_LAKE && !river.ends) river.pond = 1, river.pond_r = 0.62f;
    make_paths(back ? 0 : C_RAIL(code), &rail, 0.001f);
    rail.pond = 0;
    int owner = C_OWN(code) == 7 ? -1 : C_OWN(code);
    const float WR = terr == RH_LAKE ? 0.18f : 0.15f, BANK = 0.045f, RB = 0.17f;
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
                    if (back) c[0] = 108, c[1] = 70, c[2] = 44;
                    else if (dv < 0.2f * dp) c[0] = 86, c[1] = 132, c[2] = 58;
                    else k *= 0.95f + 0.1f * art_noise(x / 2, (int)(dv * 3));
                    px_over(p, c[0] * k, c[1] * k, c[2] * k, cov);
                }
            }
            float sd = hexsdf(u, v), cov = clampf(0.5f - sd * rp * 0.9f, 0, 1);
            if (cov <= 0) continue;
            float c[3];
            if (back) {                         /* face down: honey wood, a compass rose inlaid */
                float g = 0.93f + 0.07f * sinf(u * 26 + 3 * sinf(v * 7)) + 0.05f * art_noise(x, y / 3);
                c[0] = 216 * g, c[1] = 178 * g, c[2] = 126 * g;
                float r = hypotf(u, v), a = atan2f(v, u);
                float star = 0.34f * (0.28f + 0.72f * powf(fabsf(cosf(2 * a + ART_PI / 2 * 0)), 10.0f));
                float in = clampf((star - r) * rp + 0.5f, 0, 1), ring = clampf((0.025f - fabsf(r - 0.44f)) * rp + 0.5f, 0, 1);
                float k = 1 - 0.24f * in - 0.14f * ring;
                if (in > 0 && cosf(a + ART_PI / 4) * cosf(a - ART_PI / 4) > 0) k = 1 - 0.16f * in - 0.14f * ring;
                for (int i = 0; i < 3; i++) c[i] *= k;
            } else {
                ground_col(terr, var, u, v, x, y, c);
                if (river.ends || river.pond) {  /* the water, its banks, its shimmer */
                    float d0 = paths_dist(&river, u, v, WR, NULL);
                    if (d0 < WR + BANK + 2 / rp) {
                        static const float bank[3] = {150, 162, 96};
                        float bk = clampf((WR + BANK - d0) * rp + 0.5f, 0, 1);
                        for (int i = 0; i < 3; i++) c[i] += (bank[i] - c[i]) * bk;
                        float wa = clampf((WR - d0) * rp + 0.5f, 0, 1);
                        if (wa > 0) {
                            float depth = clampf(1 - d0 / WR, 0, 1);
                            if (terr == RH_LAKE) depth = clampf(0.5f + 0.5f * (0.6f - hypotf(u, v)), 0, 1);
                            float wc[3] = {104 - 44 * depth, 172 - 46 * depth, 216 - 24 * depth};
                            float sh = sinf(u * 38 + v * 19 + 2.2f * sinf(v * 15 + u * 4));
                            if (sh > 0.8f) for (int i = 0; i < 3; i++) wc[i] += (255 - wc[i]) * (sh - 0.8f) * 2.2f;
                            for (int i = 0; i < 3; i++) c[i] += (wc[i] - c[i]) * wa;
                        }
                    }
                }
                if (rail.ends) {                /* the rails: a bed in the owner's colour, sleepers, two steel lines */
                    float al, d0 = paths_dist(&rail, u, v, 0.0f, &al);
                    if (d0 < RB + 2 / rp) {
                        float g = clampf((RB - d0) * rp + 0.5f, 0, 1), n = 0.9f + 0.2f * art_noise(x, y), gc[3];
                        bed_colour(owner, n, gc);
                        float edge = clampf((d0 - (RB - 0.025f)) * rp + 0.5f, 0, 1);   /* a dark rim */
                        for (int i = 0; i < 3; i++) gc[i] *= 1 - 0.45f * edge;
                        float sl = fmodf(al + 10, 0.115f);
                        if (sl < 0.05f && d0 < 0.125f) gc[0] = 104, gc[1] = 74, gc[2] = 52;
                        float rd = fabsf(d0 - 0.068f), ra = clampf((0.018f - rd) * rp + 0.5f, 0, 1);
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
    if (back) return;
    /* the things on it, from the back */
    thing th[90];
    int n = 0;
    int kind = terr == RH_FOREST ? 0 : terr == RH_TOWN ? 1 : terr == RH_FIELD ? 2 : terr == RH_MOUNTAIN ? 4 :
               terr == RH_HILL ? 5 : terr == RH_LAKE ? 6 : 3;
    static const struct { float step, keep, size; } KIND[7] = {
        {0.23f, 0.86f, 0.30f}, {0.34f, 0.80f, 0.27f}, {0.40f, 0.30f, 0.30f}, {0.44f, 0.16f, 0.30f},
        {0.52f, 0.95f, 0.62f}, {0.40f, 0.20f, 0.26f}, {0.30f, 0.30f, 0.30f},
    };
    float st = KIND[kind].step;
    for (int j = -6; j <= 6; j++)
        for (int i = -6; i <= 6; i++) {
            uint32_t hh = hsh(var * 7u + (uint32_t)kind * 131u + (uint32_t)terr, (uint32_t)((j + 8) * 32 + i + 8));
            float u = (i + 0.5f * (j & 1)) * st + ((hh & 255) / 255.0f - 0.5f) * 0.5f * st;
            float v = j * st * 0.87f + ((hh >> 8 & 255) / 255.0f - 0.5f) * 0.5f * st;
            if (hexsdf(u, v) > -0.13f - 0.05f * (kind == 1) - 0.12f * (kind == 4) ||
                (hh >> 16 & 255) / 255.0f > KIND[kind].keep)
                continue;
            if (kind == 6 && hypotf(u, v) < 0.62f + 0.06f) continue;   /* reeds round the lake */
            if (river.ends && paths_dist(&river, u, v, 0.15f, NULL) < 0.15f + 0.09f) continue;
            if (rail.ends && paths_dist(&rail, u, v, 0, NULL) < RB + 0.08f) continue;
            if (n >= 90) continue;
            thing *t = &th[n++];
            t->u = u, t->v = v;
            t->var = (int)(hh >> 24);
            t->s = KIND[kind].size * (0.86f + 0.28f * hf(hh, 3));
            t->kind = kind == 0 ? (t->var & 3 ? OB_PINE : OB_TREE) : kind == 1 ? OB_HOUSE : kind == 2 ? OB_STOOK :
                      kind == 4 ? OB_PEAK : kind == 5 ? (t->var % 3 == 0 ? OB_ROCK : OB_BUSH) : kind == 6 ? OB_REED : OB_BUSH;
            if (kind == 4 && (t->var & 3) == 0) t->kind = OB_ROCK, t->s = 0.26f;
        }
    if (terr == RH_TOWN && n < 90) {            /* the town's hall in the middle */
        int k2 = 0;
        for (int i = 0; i < n; i++)
            if (hypotf(th[i].u, th[i].v) > 0.24f) th[k2++] = th[i];
        n = k2;
        th[n++] = (thing){0.02f, 0.02f, 0.30f, OB_TOWER, (int)var};
    }
    if (terr == RH_HILL && n < 86) {            /* the hills themselves: two or three humps */
        int k2 = 0;
        float mu[3], mv[3], ms[3];
        int nm = 2 + (int)(var % 2);
        for (int k = 0; k < nm; k++) {
            mu[k] = (k == 0 ? -0.26f : k == 1 ? 0.24f : -0.02f) + 0.14f * (hf(var, 40 + k) - 0.5f);
            mv[k] = (k == 0 ? -0.10f : k == 1 ? 0.02f : 0.30f) + 0.12f * (hf(var, 50 + k) - 0.5f);
            ms[k] = 0.62f + 0.22f * hf(var, 60 + k);
        }
        for (int i = 0; i < n; i++) {
            int keep = 1;
            for (int k = 0; k < nm; k++)
                if (hypotf(th[i].u - mu[k], (th[i].v - mv[k]) * 1.6f) < 0.30f * ms[k] + 0.06f) keep = 0;
            if (keep) th[k2++] = th[i];
        }
        n = k2;
        for (int k = 0; k < nm; k++) th[n++] = (thing){mu[k], mv[k], ms[k], OB_MOUND, 0};
    }
    if (C_HAM(code) && n < 88) {                /* a hamlet: two houses */
        int k2 = 0;
        for (int i = 0; i < n; i++)
            if (hypotf(th[i].u + 0.12f, th[i].v + 0.05f) > 0.34f) th[k2++] = th[i];
        n = k2;
        th[n++] = (thing){-0.24f, -0.12f, 0.24f, OB_HOUSE, (int)var + 1};
        th[n++] = (thing){0.04f, 0.06f, 0.24f, OB_HOUSE, (int)var + 2};
    }
    if (rail.ends == 1 && n < 90) th[n++] = (thing){0, 0.02f, 0.3f, OB_STOP, 0};
    for (int i = 1; i < n; i++)                 /* the back ones first */
        for (int j = i; j > 0 && th[j - 1].v > th[j].v; j--) { thing t = th[j]; th[j] = th[j - 1]; th[j - 1] = t; }
    for (int i = 0; i < n; i++) paint_thing(o, W, H, ax + th[i].u * rp, ay + th[i].v * sy, rp, &th[i]);
}

static void tile_box(float rp, int *w, int *h, float *ax, float *ay)
{
    *w = (int)ceilf(SQ3 * rp) + 4;
    *h = (int)ceilf((OVER + 2 * TILT + THICK) * rp) + 4;
    *ax = *w / 2.0f;
    *ay = 2 + (OVER + TILT) * rp;
}

/* ---- the pictures kept: each tile at a few sizes ----------------------------------------------------------------- */
typedef struct { uint32_t code; int b; SDL_Texture *t; int w, h; float ax, ay, rp; Uint32 used; } kept;
#define KEPT 520
static kept K[KEPT];
static Uint32 frame_no;
static int painted;

static int bucket_of(float rp) { return (int)lroundf(logf(fmaxf(rp, 4)) / logf(1.15f)); }

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
    if (near && (painted >= 6 || nd <= 1)) {
        near->used = frame_no;
        if (nd > 1 || painted < 6) ui_animate();
        if (painted < 6) return paint_kept(code, b);
        return near;
    }
    return paint_kept(code, b);
}

/* a tile at (sx, sy) (its middle), squeezed sideways by fx (a flip: 0..1), faded */
static void put_tile(uint32_t code, float sx, float sy, float rp, float fx, Uint8 alpha)
{
    kept *k = tile_pic(code, rp);
    if (!k || !k->t) return;
    float z = rp / k->rp;
    SDL_SetTextureAlphaMod(k->t, alpha);
    float w = k->w * z * fx;
    ui_put(k->t, sx - k->ax * z * fx, sy - k->ay * z, w, k->h * z);
    SDL_SetTextureAlphaMod(k->t, 255);
}

/* ---- the flat pictures: a tile's shadow, a hex's outline, a puff, a coin, a locomotive ------------------------- */
static SDL_Texture *t_shadow, *t_ring, *t_puff, *t_coin, *t_cross, *t_pin;
static int made_flat = -1, made_coin = -1;

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
            float lw = fmaxf(0.04f, 2.0f / r), ring = clampf((lw - fabsf(sd + 0.07f)) * r + 0.5f, 0, 1);
            float in = sd < -0.07f ? 0.22f : 0;
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(255 * fmaxf(ring, in));
        }
    ui_free(&t_ring);
    t_ring = ui_tex(w, h, o);
    if (!t_puff) {
        static const uint8_t cream[3] = {250, 246, 236};
        o = ui_buf(64, 64);
        art_disc(64, cream, 0.9f, 1.6f, o);
        t_puff = ui_tex(64, 64, o);
    }
}

/* a gold coin, size across */
static void paint_coin(uint8_t *o, int W, int H, float cx, float cy, float r)
{
    for (int y = (int)(cy - r - 1) < 0 ? 0 : (int)(cy - r - 1); y < H && y <= (int)(cy + r + 1); y++)
        for (int x = (int)(cx - r - 1) < 0 ? 0 : (int)(cx - r - 1); x < W && x <= (int)(cx + r + 1); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = hypotf(dx, dy), cov = clampf(r - d + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float l = 1 + 0.28f * (-dx - dy) / r, rim = d > 0.78f * r ? 0.82f : 1;
            if (fabsf(d - 0.62f * r) < 0.06f * r) rim = 0.86f;
            px_over(o + 4 * ((size_t)y * W + x), 236 * l * rim, 182 * l * rim, 62 * l * rim, cov);
        }
}

static void make_coin(int d)
{
    if (d == made_coin || d < 4) return;
    made_coin = d;
    uint8_t *o = ui_buf(d, d);
    memset(o, 0, (size_t)d * d * 4);
    paint_coin(o, d, d, d / 2.0f, d / 2.0f, 0.46f * d);
    ui_free(&t_coin);
    t_coin = ui_tex(d, d, o);
    memset(o, 0, (size_t)d * d * 4);
    static const uint8_t red[3] = {204, 70, 58}, white[3] = {255, 255, 255};
    art_disc(d, red, 1, 6, o);
    pzk_stroke(o, d, d, 0.32f * d, 0.32f * d, 0.68f * d, 0.68f * d, 0.07f * d, white, 1);
    pzk_stroke(o, d, d, 0.68f * d, 0.32f * d, 0.32f * d, 0.68f * d, 0.07f * d, white, 1);
    ui_free(&t_cross);
    t_cross = ui_tex(d, d, o);
    {                                           /* a map pin: a red head on a dark stem, its point at the bottom */
        int pw = d, ph = (int)(1.4f * d);
        uint8_t *q = ui_buf(pw, ph);
        memset(q, 0, (size_t)pw * ph * 4);
        static const uint8_t ink[3] = {52, 34, 22}, pinred[3] = {214, 60, 46};
        pzk_stroke(q, pw, ph, pw / 2.0f, 0.5f * pw, pw / 2.0f, ph - 1.5f, fmaxf(1, 0.06f * pw), ink, 1);
        pzk_ball(q, pw, ph, pw / 2.0f, 0.36f * pw, 0.33f * pw, ink, 1, 0);
        pzk_ball(q, pw, ph, pw / 2.0f, 0.36f * pw, 0.27f * pw, pinred, 1, 0.45f);
        ui_free(&t_pin);
        t_pin = ui_tex(pw, ph, q);
    }
}

/* a locomotive in a player's colour, facing right: its boiler, cab, chimney and wheels (a figure, the house's style) */
static void paint_loco(uint8_t *o, int size, int player, float x, float y, float s)
{
    art_fig *f = art_fig_new();
    if (!f) return;
    art_rect(f, ART_PART, 0.14f, 0.40f, 0.62f, 0.66f);      /* the boiler */
    art_rect(f, ART_PART, 0.56f, 0.26f, 0.86f, 0.66f);      /* the cab */
    art_rect(f, ART_PART, 0.52f, 0.20f, 0.90f, 0.27f);      /* its roof */
    art_rect(f, ART_PART, 0.20f, 0.24f, 0.30f, 0.40f);      /* the chimney */
    art_rect(f, ART_PART, 0.10f, 0.64f, 0.92f, 0.70f);      /* the frame */
    art_ellipse(f, ART_PART, 0.26f, 0.76f, 0.09f, 0.09f);
    art_ellipse(f, ART_PART, 0.48f, 0.76f, 0.09f, 0.09f);
    art_ellipse(f, ART_PART, 0.74f, 0.74f, 0.12f, 0.12f);
    art_rect(f, ART_MARK, 0.63f, 0.33f, 0.80f, 0.45f);      /* the cab's window */
    art_style st = art_player[player < 0 ? 0 : player % ART_PLAYERS];
    st.line = 0.045f;
    art_fig_blend(o, size, size, f, &st, (int)x, (int)y, (int)s, 1);
    art_fig_free(f);
}

/* ---- the camera ---------------------------------------------------------------------------------------------------- */
static void world_of(int c, float *wx, float *wy)
{
    int col = c % G.w, row = c / G.w;
    *wx = SQ3 * (col + 0.5f * (row & 1));
    *wy = 1.5f * row;
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
    int best = -1;
    float bd = 1e9f;
    for (int c = 0; c < G.nc; c++) {
        if (G.terr[c] == RH_SEA) continue;
        float cx, cy;
        world_of(c, &cx, &cy);
        float d = hexsdf(wx - cx, wy - cy);
        if (d < bd) bd = d, best = c;
    }
    return bd < 0.05f ? best : -1;
}

static float s_most(void) { return 0.30f * fminf(view.w, view.h); }
static float s_least(void) { return fmaxf(8, 0.018f * fminf(ui.w, ui.h)); }

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

static void map_bounds(float *x0, float *y0, float *x1, float *y1)
{
    *x0 = *y0 = 1e9f, *x1 = *y1 = -1e9f;
    for (int c = 0; c < G.nc; c++)
        if (G.terr[c] != RH_SEA) {
            float wx, wy;
            world_of(c, &wx, &wy);
            *x0 = fminf(*x0, wx), *x1 = fmaxf(*x1, wx), *y0 = fminf(*y0, wy), *y1 = fmaxf(*y1, wy);
        }
    if (*x0 > *x1) *x0 = *x1 = *y0 = *y1 = 0;
    *x0 -= 1.0f, *x1 += 1.0f, *y0 -= 1.0f + OVER / TILT, *y1 += 1.0f + THICK / TILT;
}

/* the whole map in the view */
static float fit_scale(void)
{
    float x0, y0, x1, y1;
    map_bounds(&x0, &y0, &x1, &y1);
    return fminf(view.w / (x1 - x0), view.h / ((y1 - y0) * TILT));
}

static void cam_auto(int at_once)
{
    if (view.w <= 0) return;
    float x0, y0, x1, y1;
    map_bounds(&x0, &y0, &x1, &y1);
    float s = fit_scale(), x = (x0 + x1) / 2, y = (y0 + y1) / 2;
    if (at_once) cam_set(x, y, s);
    else cam_to(x, y, s);
}

/* ---- the codes of the map's tiles as they show ------------------------------------------------------------------- */
static int is_wet(int c) { return c >= 0 && (G.terr[c] == RH_RIVER || G.terr[c] == RH_LAKE); }

static uint32_t cell_code(int c)
{
    int t = G.terr[c];
    if (G.hidden[c]) return code_make(T_BACK, 0, 0, 7, 0, c % 16);
    int riv = 0, rail = 0, own = G.owner[c];
    if (t == RH_RIVER || t == RH_LAKE) {
        int n = 0;
        for (int d = 0; d < 6; d++)
            if (is_wet(G.nb[c][d])) riv |= 1 << d, n++;
        if (t == RH_RIVER && n < 2)             /* the river runs on to the sea */
            for (int d = 0; d < 6 && n < 2; d++)
                if (G.nb[c][d] < 0) riv |= 1 << d, n++;
        if (t == RH_LAKE && n > 1) riv = 0;
    }
    if (own >= 0)
        for (int d = 0; d < 6; d++) {
            int b = G.nb[c][d];
            if (b >= 0 && (G.owner[b] == own || G.terr[b] == RH_TOWN)) rail |= 1 << d;
        }
    int var = c * 7 % 16;
    return code_make(t, riv, rail, own < 0 ? 7 : own, G.hamlet[c] && t != RH_TOWN, var);
}

/* ---- the contracts as cards: the island's shape, its towns, the two linked, the value in a coin ---------------- */
typedef struct { int id, w, h, ser; SDL_Texture *t; } cardpic;
#define CARDS_KEPT 48
static cardpic CP[CARDS_KEPT];
static int cp_next;

/* a thin walnut rim round a picture's shape, t pixels */
static void rim(uint8_t *o, int w, int h, int t)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            if (p[3] < 128) continue;
            int e = x < t || y < t || x >= w - t || y >= h - t;
            for (int d = 0; d < 4 && !e; d++) {
                int xx = x + (d == 0 ? -t : d == 1 ? t : 0), yy = y + (d == 2 ? -t : d == 3 ? t : 0);
                e = o[4 * ((size_t)yy * w + xx) + 3] < 128;
            }
            if (e) p[0] = 120, p[1] = 86, p[2] = 56;
        }
}

static void paint_card(int id, int w, int h, uint8_t *o)
{
    static const uint8_t paper[4] = {246, 238, 220, 255};
    memset(o, 0, (size_t)w * h * 4);
    art_roundrect(w, h, 0.10f * w, paper, o);
    /* the island: its land cells as small hexes, the towns as dots */
    float x0, y0, x1, y1;
    map_bounds(&x0, &y0, &x1, &y1);
    x0 += 0.6f, x1 -= 0.6f, y0 += 0.6f + OVER / TILT, y1 -= 0.6f + THICK / TILT;
    float mw = 0.88f * w, mh = 0.62f * h, k = fminf(mw / (x1 - x0), mh / (y1 - y0));
    float ox = (w - k * (x1 - x0)) / 2 - k * x0, oy = 0.06f * h + (mh - k * (y1 - y0)) / 2 - k * y0;
    for (int y = 0; y < (int)(0.72f * h); y++)
        for (int x = 0; x < w; x++) {
            float wx = (x + 0.5f - ox) / k, wy = (y + 0.5f - oy) / k;
            int row = (int)lroundf(wy / 1.5f), best = -1;
            float bd = 9;
            for (int r = row - 1; r <= row + 1; r++) {
                if (r < 0 || r >= G.h) continue;
                int col = (int)lroundf(wx / SQ3 - 0.5f * (r & 1));
                for (int cc = col - 1; cc <= col + 1; cc++) {
                    if (cc < 0 || cc >= G.w) continue;
                    int c = cc + r * G.w;
                    float cx, cy;
                    world_of(c, &cx, &cy);
                    float d = hexsdf(wx - cx, wy - cy);
                    if (d < bd) bd = d, best = c;
                }
            }
            if (best < 0 || G.terr[best] == RH_SEA || bd > 0.02f) continue;
            float a = bd > -0.10f ? 0.55f : 1;
            px_over(o + 4 * ((size_t)y * w + x), 150, 186, 118, a);
        }
    const rh_con *cn = &G.con[id];
    for (int i = 0; i < G.ntown; i++) {
        float cx, cy;
        world_of(G.town[i], &cx, &cy);
        float px = ox + cx * k, py = oy + cy * k, r = fmaxf(1.5f, 0.75f * k);
        int on = i == cn->a || i == cn->b;
        static const uint8_t grey[3] = {120, 112, 100}, ink[3] = {52, 34, 22}, red[3] = {206, 58, 44};
        if (on) {
            pzk_ball(o, w, h, px, py, 2.2f * r, ink, 1, 0);
            pzk_ball(o, w, h, px, py, 1.7f * r, red, 1, 0.3f);
        } else pzk_ball(o, w, h, px, py, 0.9f * r, grey, 0.9f, 0);
    }
    { /* the line between them, dotted */
        float ax, ay, bx, by;
        world_of(G.town[cn->a], &ax, &ay);
        world_of(G.town[cn->b], &bx, &by);
        ax = ox + ax * k, ay = oy + ay * k, bx = ox + bx * k, by = oy + by * k;
        static const uint8_t red[3] = {206, 58, 44};
        float l = hypotf(bx - ax, by - ay), step = fmaxf(3, 0.06f * w);
        for (float s = 2.6f * k; s < l - 2.6f * k; s += step)
            pzk_ball(o, w, h, ax + (bx - ax) * s / l, ay + (by - ay) * s / l, fmaxf(0.8f, 0.018f * w), red, 0.8f, 0);
    }
    /* the value: a coin at the foot */
    float cr = 0.17f * w, cx = w / 2.0f, cy = 0.83f * h;
    paint_coin(o, w, h, cx, cy, fminf(cr * 1.25f, 0.15f * h));
    puz_digits(o, w, h, cn->value, cx, cy, 0.16f * h, PUZ_INK, 1, 1);
    rim(o, w, h, 2);
}

static SDL_Texture *card_tex(int id, int w, int h)
{
    for (int i = 0; i < CARDS_KEPT; i++)
        if (CP[i].t && CP[i].id == id && CP[i].w == w && CP[i].h == h && CP[i].ser == serial) return CP[i].t;
    cardpic *c = &CP[cp_next++ % CARDS_KEPT];
    ui_free(&c->t);
    c->id = id, c->w = w, c->h = h, c->ser = serial;
    uint8_t *o = ui_buf(w, h);
    paint_card(id, w, h, o);
    c->t = ui_tex(w, h, o);
    return c->t;
}

/* a card's back: walnut with a rail and a sun */
static SDL_Texture *t_back;
static int back_w = -1, back_h = -1;
static SDL_Texture *back_tex(int w, int h)
{
    if (w == back_w && h == back_h && t_back) return t_back;
    back_w = w, back_h = h;
    uint8_t *o = ui_buf(w, h);
    static const uint8_t wood[4] = {120, 78, 48, 255}, gold[3] = {226, 184, 92};
    memset(o, 0, (size_t)w * h * 4);
    art_roundrect(w, h, 0.10f * w, wood, o);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            float g = 0.9f + 0.1f * sinf(x * 0.5f + 3 * sinf(y * 0.08f)) + 0.05f * art_noise(x, y / 3);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * g, 0, 255);
        }
    float lw = fmaxf(1, 0.03f * w);
    pzk_stroke(o, w, h, 0.18f * w, 0.62f * h, 0.82f * w, 0.62f * h, lw, gold, 0.9f);
    pzk_stroke(o, w, h, 0.18f * w, 0.70f * h, 0.82f * w, 0.70f * h, lw, gold, 0.9f);
    for (int i = 0; i < 5; i++) {
        float x = (0.24f + 0.13f * i) * w;
        pzk_stroke(o, w, h, x, 0.58f * h, x, 0.74f * h, lw, gold, 0.7f);
    }
    pzk_ball(o, w, h, 0.5f * w, 0.34f * h, 0.13f * w, gold, 0.9f, 0.3f);
    ui_free(&t_back);
    t_back = ui_tex(w, h, o);
    return t_back;
}

/* ---- the little pictures of the tray and the buttons ------------------------------------------------------------- */
enum { PIC_RAIL, PIC_BLIND, PIC_SURVEY, PIC_CARD, PIC_COIN, PIC_LOCO0 };
static void painter(int kind, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    float s = (float)size;
    if (kind == PIC_COIN) { paint_coin(o, size, size, s / 2, s / 2, 0.44f * s); return; }
    if (kind == PIC_CARD) {
        int w = (int)(0.62f * s), h = (int)(0.84f * s);
        uint8_t *b = malloc((size_t)w * h * 4);
        if (!b) return;
        static const uint8_t paper[4] = {246, 238, 220, 255}, red[3] = {206, 58, 44}, grn[3] = {150, 186, 118};
        art_roundrect(w, h, 0.12f * w, paper, b);
        pzk_ball(b, w, h, 0.5f * w, 0.38f * h, 0.34f * w, grn, 1, 0);
        pzk_ball(b, w, h, 0.32f * w, 0.30f * h, 0.09f * w, red, 1, 0);
        pzk_ball(b, w, h, 0.66f * w, 0.46f * h, 0.09f * w, red, 1, 0);
        paint_coin(b, w, h, 0.5f * w, 0.80f * h, 0.15f * w);
        rim(b, w, h, w > 30 ? 2 : 1);
        art_blend(o, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
        free(b);
        return;
    }
    if (kind >= PIC_LOCO0) { paint_loco(o, size, kind - PIC_LOCO0, 0.04f * s, 0.06f * s, 0.92f * s); return; }
    /* a hex with two rails across (blind: on a face-down tile; survey: a spyglass over a face-down tile) */
    float rp = 0.40f * s;
    uint32_t code = kind == PIC_RAIL ? code_make(RH_MEADOW, 0, 1 | 8, 7, 0, 3) : code_make(T_BACK, 0, 0, 7, 0, 3);
    if (kind == PIC_RAIL) code = code_make(RH_MEADOW, 0, 1 | 8, 0, 0, 3);
    paint_tile(o, size, size, s / 2, 0.52f * s, rp, code);
    static const uint8_t steel[3] = {214, 214, 222}, wood[3] = {104, 74, 52}, ink[3] = {52, 34, 22}, glass[3] = {170, 214, 236};
    if (kind == PIC_BLIND) {                    /* rails laid onto the back */
        float y = 0.50f * s;
        for (int i = 0; i < 5; i++) pzk_stroke(o, size, size, (0.22f + 0.14f * i) * s, y - 0.07f * s, (0.22f + 0.14f * i) * s,
                                              y + 0.07f * s, 0.035f * s, wood, 1);
        pzk_stroke(o, size, size, 0.14f * s, y - 0.04f * s, 0.86f * s, y - 0.04f * s, 0.022f * s, steel, 1);
        pzk_stroke(o, size, size, 0.14f * s, y + 0.04f * s, 0.86f * s, y + 0.04f * s, 0.022f * s, steel, 1);
    } else if (kind == PIC_SURVEY) {            /* a spyglass */
        pzk_stroke(o, size, size, 0.24f * s, 0.70f * s, 0.62f * s, 0.34f * s, 0.075f * s, ink, 1);
        pzk_stroke(o, size, size, 0.56f * s, 0.40f * s, 0.74f * s, 0.22f * s, 0.105f * s, ink, 1);
        pzk_ball(o, size, size, 0.75f * s, 0.21f * s, 0.06f * s, glass, 1, 0.2f);
    }
}

static SDL_Texture *pics[PIC_LOCO0 + RH_NP];
static int pic_size[PIC_LOCO0 + RH_NP];
static SDL_Texture *pic(int kind, int size)
{
    if (size < 4) size = 4;
    if (pics[kind] && pic_size[kind] == size) return pics[kind];
    uint8_t *o = ui_buf(size, size);
    painter(kind, size, o);
    ui_free(&pics[kind]);
    pics[kind] = ui_tex(size, size, o);
    pic_size[kind] = size;
    return pics[kind];
}

/* ---- the layout ---------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_tray;
static int made_tray_w = -1, made_tray_h = -1;
static int upright;
static float card_w, card_h;                    /* a contract card in the tray */

static void layout(box b)
{
    room = b;
    float u = ui.unit;
    upright = b.h >= b.w;
    if (upright) {
        float th = clampf(0.19f * b.h, 2.2f * u, 3.0f * u);
        tray = (box){b.x, b.y + b.h - th, b.w, th};
        view = (box){b.x, b.y, b.w, b.h - th - 0.1f * u};
        card_h = 0.82f * th;
        card_w = card_h / 1.32f;
    } else {
        card_w = fminf(1.05f * u, 0.2f * b.h / 1.32f);   /* two columns of cards */
        card_h = card_w * 1.32f;
        float tw = 2 * card_w + 0.62f * u;
        tray = (box){b.x + b.w - tw, b.y, tw, b.h};
        view = (box){b.x, b.y, b.w - tw - 0.1f * u, b.h};
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
    if (!user_cam) cam_auto(1);
    else cam_set(cam_x, cam_y, cam_s);
}

/* ---- the game's flow ------------------------------------------------------------------------------------------------ */
static int over_now(void) { return rh_over(&G); }
static int viewer(void) { return ui.viewer; }

/* 1: a person must act now and the screen is theirs (not a bot's turn, no cover) */
static int my_turn(void)
{
    int a = rh_actor(&G);
    return a >= 0 && !(ui_bot_seats() >> a & 1) && !ui_handing() && a == viewer();
}

static void calm(void)
{
    nfx = 0;
    ntrain = 0;
    train_player = -1;
    sel = selc = -1;
    keep_mask = 0;
    rest_until = 0;
}

static void begin(void)
{
    serial++;
    calm();
    memset(&now_sum, 0, sizeof now_sum);
    memset(&last_sum, 0, sizeof last_sum);
    now_sum.player = G.turn;
    last_sum.player = -1;
    user_cam = zoomed = 0;
    if (room.w > 0) layout(room);
}

static void new_game(void)
{
    rh_cfg c;
    rh_config(&c, players_now, variant_now);
    rh_new(&G, &c, players_now, seed_now);
    nmv = undo_floor = 0;
    begin();
}

static void start(int variant)
{
    variant_now = variant < 0 || variant >= VARIANTS ? 0 : variant;
    players_now = ui.players < 2 ? 2 : ui.players > RH_NP ? RH_NP : ui.players;
    seed_now = (uint32_t)ui_random(0x7fffffff) + 1u;
    new_game();
}

static int fx_running(Uint32 now)
{
    for (int i = 0; i < nfx; i++)
        if ((int)(now - FX[i].t0) < (int)FX[i].ms && FX[i].kind != FX_PUFF) return 1;
    if (ntrain && (int)(now - train_t0) < (int)(ntrain * STEP_MS + 200)) return 1;
    return 0;
}

static int busy(void)
{
    Uint32 now = ui_ms();
    return fx_running(now) || (int)(now - rest_until) < 0;
}

static int status(void)
{
    if (!over_now() || busy()) return GS_PLAY;
    unsigned w = rh_winners(&G);
    if (w & (w - 1)) return GS_DRAW;
    for (int p = 0; p < G.np; p++)
        if (w >> p & 1) return gs_win(p);
    return GS_DRAW;
}

static unsigned winners(void) { return rh_winners(&G); }
static int turn(void) { int a = rh_actor(&G); return a < 0 ? G.turn : a; }

static void fx_add(int kind, int cell, int value, int player, Uint32 t0, Uint32 ms)
{
    if (nfx >= MAX_FX) memmove(FX, FX + 1, sizeof FX[0] * --nfx);
    FX[nfx++] = (fxe){kind, cell, value, player, t0, ms, 0, 0};
}

/* the summary of the turn just ended, shown a moment over the map */
static void turn_ended(int who, int next)
{
    now_sum.player = who;
    now_sum.t0 = ui_ms();
    last_sum = now_sum;
    memset(&now_sum, 0, sizeof now_sum);
    now_sum.player = next;
}

/* a move played: the rules, the animations, the sounds, the hand-off */
static int act(int m, int quiet)
{
    int who = G.turn, phase = G.phase, round = G.round;
    rh_state before = G;
    rh_result res;
    for (int c = 0; c < G.nc; c++) lay_old[c] = G.terr[c] == RH_SEA ? 0 : cell_code(c);
    if (!rh_play(&G, m, &res)) {
        if (!quiet) ui_sound(SND_DENY);
        return 0;
    }
    if (nmv < MAXM) mv[nmv++] = m;
    Uint32 now = ui_ms(), end = now;
    int revealing = 0;
    if (m >= RH_M_BUILD && m < RH_M_SURVEY) {
        int c = m - RH_M_BUILD;
        if (before.hidden[c]) {                 /* turned up first */
            fx_add(FX_FLIP, c, 0, who, now, FLIP_MS);
            revealing = 1;
            end = now + FLIP_MS;
        }
        if (res.failed) {
            fx_add(FX_FAIL, c, 0, who, end, 900);
            ui_sound_later(SND_DENY, (int)(end - now));
            now_sum.failed++;
        } else {
            fx_add(FX_LAY, c, 0, who, end, LAY_MS);
            fx_add(FX_PUFF, c, 0, who, end + LAY_MS - 120, 620);
            ui_sound_later(SND_PLACE, (int)(end - now) + LAY_MS - 140);
            end += LAY_MS;
            now_sum.built++;
            if (res.hamlet) {
                fx_add(FX_POP, c, G.cfg.hamlet_bonus, who, end, 1100);
                ui_sound_later(SND_CHECK, (int)(end - now));
                now_sum.coins += G.cfg.hamlet_bonus;
            }
        }
    } else if (m >= RH_M_SURVEY && m < RH_M_DELIVER) {
        int c = m - RH_M_SURVEY;
        for (int i = 0; i < res.nrev; i++) {
            int rc = res.revealed[i];
            Uint32 t = now + (rc == c ? 0 : 90 + 40 * i);
            fx_add(FX_FLIP, rc, 0, who, t, FLIP_MS);
            if (t + FLIP_MS > end) end = t + FLIP_MS;
        }
        ui_sound(SND_MOVE);
        revealing = 1;
        now_sum.surveyed++;
    } else if (m >= RH_M_DELIVER && m < RH_M_DRAW) {   /* a train runs the route; coins fly */
        ntrain = res.npath < RH_MAXC ? res.npath : RH_MAXC;
        memcpy(train, res.path, ntrain * sizeof(int));
        train_player = who;
        train_t0 = now;
        end = now + ntrain * STEP_MS + 120;
        ui_sound(SND_MOVE);
        ui_sound_later(SND_CHECK, (int)(end - now));
        int dest = ntrain ? train[ntrain - 1] : -1;
        fx_add(FX_POP, dest, res.value - res.rent, who, end, 1200);
        for (int q = 0; q < G.np; q++)
            if (res.paid[q] > 0) fx_add(FX_COIN, dest, res.paid[q], q, end + 150 + 120 * q, 700);
        now_sum.delivered++;
        now_sum.coins += res.value - res.rent;
        selc = -1;
        end += 300;
    } else if (m == RH_M_DRAW) {
        fx_add(FX_OFFER, -1, 0, who, now, 360);
        ui_sound(SND_MOVE);
        keep_mask = 0;
        revealing = 1;
        now_sum.drew = 1;
        end = now + 360;
    } else if (m >= RH_M_KEEP && m < RH_M_END) {
        ui_sound(SND_PLACE);
        keep_mask = 0;
        revealing = 1;
    } else if (m == RH_M_END) ui_sound(SND_TAP);
    rest_until = end + REST_MS;
    sel = -1;
    if (phase == RH_KEEP || revealing) undo_floor = nmv;   /* nothing seen is ever taken back */
    if (G.phase == RH_OVER || G.turn != who || (phase == RH_KEEP && G.phase == RH_KEEP)) {
        if (phase == RH_PLAY || round > 0) turn_ended(who, G.turn);   /* not the first deal */
        undo_floor = nmv;
        if (G.phase != RH_OVER) ui_handoff(G.turn);
    }
    ui_animate();
    return 1;
}

static int undo(void)
{
    if (nmv <= undo_floor || nmv <= 0) return 0;
    int keep = nmv - 1, floor = undo_floor;
    int saved[MAXM];
    memcpy(saved, mv, keep * sizeof(int));
    rh_cfg c;
    rh_config(&c, players_now, variant_now);
    rh_new(&G, &c, players_now, seed_now);
    for (int i = 0; i < keep; i++) rh_play(&G, saved[i], NULL);
    memcpy(mv, saved, keep * sizeof(int));
    nmv = keep;
    undo_floor = floor;
    calm();
    if (now_sum.built > 0) now_sum.built--;
    ui_animate();
    return 1;
}

static int script(const char *moves)
{
    const char *p = moves;
    int ok = 1;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        int m, n = rh_parse(p, &m);
        if (!n) { ok = 0; break; }
        p += n;
        if (!rh_play(&G, m, NULL)) { ok = 0; break; }
        if (nmv < MAXM) mv[nmv++] = m;
    }
    undo_floor = nmv;
    calm();
    ui_handoff(turn());
    ui_animate();
    return ok;
}

/* the bot (railhills_bot.c): the rules' state read-only, and a move played as a tap plays it */
const rh_state *railhills_live(void) { return &G; }

int railhills_show(const char *move)
{
    int m;
    if (!rh_parse(move, &m) || !rh_legal(&G, m)) return 0;
    return act(m, 0);
}

/* ---- drawing the map ----------------------------------------------------------------------------------------------- */
static int contract_of_sel(void)
{
    int v = viewer();
    if (selc < 0 || v < 0) return -1;
    if (selc >= 100) return selc - 100 < RH_PUB ? G.pub[selc - 100] : -1;
    return selc < G.pl[v].nhand ? G.pl[v].hand[selc] : -1;
}

static int sel_move(void)                       /* the move delivering the contract chosen (-1) */
{
    if (selc < 0) return -1;
    return selc >= 100 ? RH_M_PUBLIC + selc - 100 : RH_M_DELIVER + selc;
}

/* what may be done on hex c now: lay track (face up), lay blind (face down), survey */
static int can_lay(int c) { return my_turn() && G.phase == RH_PLAY && !G.hidden[c] && rh_legal(&G, RH_M_BUILD + c); }
static int can_blind(int c) { return my_turn() && G.phase == RH_PLAY && G.hidden[c] && rh_legal(&G, RH_M_BUILD + c); }
static int can_survey(int c) { return my_turn() && G.phase == RH_PLAY && rh_legal(&G, RH_M_SURVEY + c); }

static float fx_age(const fxe *e, Uint32 now) { return (float)(int)(now - e->t0) / e->ms; }

/* how a tile shows now: its code (the old one until a lay or a flip turns it), its lift, its squeeze */
static uint32_t shown_code(int c, Uint32 now, float *lift, float *squeeze, Uint8 *alpha)
{
    *lift = 0, *squeeze = 1, *alpha = 255;
    uint32_t code = cell_code(c);
    for (int i = nfx - 1; i >= 0; i--) {
        const fxe *e = &FX[i];
        if (e->cell != c) continue;
        float f = fx_age(e, now);
        if (e->kind == FX_FLIP && f < 1) {
            if (f < 0) return code_make(T_BACK, 0, 0, 7, 0, c % 16);
            *squeeze = fabsf(cosf(ART_PI * f));
            *lift = 0.35f * sinf(ART_PI * f);
            ui_animate();
            if (f < 0.5f) return code_make(T_BACK, 0, 0, 7, 0, c % 16);
            for (int j = 0; j < nfx; j++)       /* face up: the land, without the track still to come */
                if (FX[j].cell == c && FX[j].kind == FX_LAY && fx_age(&FX[j], now) < 0) return code & ~(63u << 10);
            return code;
        }
        if (e->kind == FX_LAY && f < 1) {
            if (f < 0) return code & ~(63u << 10);
            float g = ui_ease(clampf(f, 0, 1));
            *lift = 0.25f * (1 - g);
            ui_animate();
            return code;
        }
    }
    return code;
}

static void draw_train(Uint32 now)
{
    if (!ntrain || train_player < 0) return;
    float t = (float)(int)(now - train_t0) / STEP_MS;
    if (t > ntrain + 1.5f) { ntrain = 0; return; }
    ui_animate();
    float k = clampf(t, 0, (float)(ntrain - 1));
    int i = (int)floorf(k);
    if (i >= ntrain - 1) i = ntrain - 2 < 0 ? 0 : ntrain - 2;
    float f = k - i, ax, ay, bx, by;
    cell_screen(train[i], &ax, &ay);
    cell_screen(train[i + 1 < ntrain ? i + 1 : i], &bx, &by);
    float x = ax + (bx - ax) * f, y = ay + (by - ay) * f, s = cam_s;
    float d = fmaxf(0.9f * s, 0.5f * ui.unit);
    Uint8 a = (Uint8)(255 * clampf((ntrain + 1.5f - t) * 2, 0, 1));
    /* smoke puffs behind it */
    for (int j = 0; j < 4; j++) {
        float pt = t - 0.35f * (j + 1);
        if (pt < 0) continue;
        float pk = clampf(pt, 0, (float)(ntrain - 1));
        int pi = (int)floorf(pk);
        if (pi >= ntrain - 1) pi = ntrain - 2 < 0 ? 0 : ntrain - 2;
        float pf = pk - pi, px, py, qx, qy;
        cell_screen(train[pi], &px, &py);
        cell_screen(train[pi + 1 < ntrain ? pi + 1 : pi], &qx, &qy);
        float sx = px + (qx - px) * pf, sy2 = py + (qy - py) * pf - 0.55f * s - 0.12f * s * j;
        SDL_SetTextureAlphaMod(t_puff, (Uint8)(a * (0.55f - 0.12f * j)));
        ui_put_c(t_puff, sx, sy2, 0.35f * s * (1 + 0.3f * j), 0.28f * s * (1 + 0.3f * j));
    }
    SDL_SetTextureAlphaMod(t_puff, 255);
    SDL_Texture *lt = pic(PIC_LOCO0 + train_player, (int)d);
    SDL_SetTextureAlphaMod(lt, a);
    float bob = 0.03f * s * sinf(t * 9);
    ui_put_c(lt, x, y - 0.32f * s + bob, d, d);
    SDL_SetTextureAlphaMod(lt, 255);
}

static void draw_fx(Uint32 now)
{
    int keep = 0;
    float s = cam_s;
    make_coin((int)fmaxf(0.5f * s, 0.36f * ui.unit));
    for (int i = 0; i < nfx; i++) {
        fxe *e = &FX[i];
        int age = (int)(now - e->t0);
        if (age > (int)e->ms) continue;
        FX[keep++] = *e;
        ui_animate();
        if (age < 0) continue;
        float f = age / (float)e->ms, sx = 0, sy = 0;
        if (e->cell >= 0) cell_screen(e->cell, &sx, &sy);
        switch (e->kind) {
        case FX_PUFF:
            for (int k = 0; k < 8; k++) {
                float a = k * 45 * ART_PI / 180 + 0.3f, r = s * (0.70f + 0.45f * ui_ease(f));
                float px = sx + cosf(a) * r, py = sy + sinf(a) * r * TILT + THICK * s * 0.6f - 0.15f * s * f;
                float d = s * (0.20f + 0.26f * f);
                SDL_SetTextureAlphaMod(t_puff, (Uint8)(160 * (1 - f) * (1 - f)));
                ui_put_c(t_puff, px, py, d, d * 0.8f);
            }
            SDL_SetTextureAlphaMod(t_puff, 255);
            break;
        case FX_POP: {                          /* coins won rising */
            static const uint8_t gold[3] = {255, 222, 120}, dark[3] = {40, 28, 16};
            float h = fmaxf(0.5f * ui.unit, 0.6f * s), y = sy - 0.5f * s - 0.8f * s * ui_ease(fminf(1, f * 1.4f));
            Uint8 a = (Uint8)(255 * clampf((1 - f) * 2.5f, 0, 1));
            int v = e->value;
            float w = ui_number_w(v < 0 ? -v : v, h), x = sx + 0.3f * h;
            if (t_coin) {
                SDL_SetTextureAlphaMod(t_coin, a);
                ui_put_c(t_coin, x - w / 2 - 0.45f * h, y, 0.8f * h, 0.8f * h);
                SDL_SetTextureAlphaMod(t_coin, 255);
            }
            ui_number_ex(x + 1.5f, y + 1.5f, h, v < 0 ? -v : v, dark, (Uint8)(a * 0.6f), 0);
            ui_number_ex(x, y, h, v < 0 ? -v : v, gold, a, 0);
            break;
        }
        case FX_COIN: {                         /* rent flying to its owner's bar */
            int up;
            box bb = ui_bar_box(e->player, &up);
            float tx = bb.x + 0.6f * bb.h, ty = bb.y + bb.h / 2, g = ui_ease(f);
            float x = sx + (tx - sx) * g, y = sy + (ty - sy) * g - sinf(ART_PI * g) * 1.4f * s;
            float d = fmaxf(0.45f * s, 0.34f * ui.unit);
            for (int k = 0; k < (e->value < 4 ? e->value : 4); k++)
                if (t_coin) ui_put_c(t_coin, x + k * 0.18f * d, y - k * 0.12f * d, d, d);
            break;
        }
        case FX_FAIL:
            if (t_cross) {
                float d = fmaxf(0.7f * s, 0.5f * ui.unit) * (1 + 0.2f * sinf(ART_PI * fminf(1, f * 3)));
                SDL_SetTextureAlphaMod(t_cross, (Uint8)(255 * clampf((1 - f) * 3, 0, 1)));
                ui_put_c(t_cross, sx, sy - 0.2f * s, d, d);
                SDL_SetTextureAlphaMod(t_cross, 255);
            }
            break;
        default:
            break;
        }
    }
    nfx = keep;
}

/* the route a contract would take for the viewer now: its cells (and so its rent) */
static int route_of(int id, int *path, int *np)
{
    int v = viewer();
    if (id < 0 || v < 0) return -1;
    return rh_route(&G, v, G.con[id].a, G.con[id].b, path, np);
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
    float sw = SQ3 * s * 1.3f + 4, shh = 2 * TILT * s * 1.3f + 4;
    for (int c = 0; c < G.nc; c++) {            /* the shadows on the table */
        if (G.terr[c] == RH_SEA) continue;
        float sx, sy;
        cell_screen(c, &sx, &sy);
        if (sx < view.x - 2 * s || sx > view.x + view.w + 2 * s || sy < view.y - 2 * s || sy > view.y + view.h + 2 * s)
            continue;
        ui_put_c(t_shadow, sx + 0.10f * s, sy + (THICK + 0.12f) * s, sw * 0.86f, shh * 0.86f);
    }
    int con = contract_of_sel(), path[RH_MAXC], np = 0;
    if (con >= 0 && route_of(con, path, &np) < 0) np = 0;
    int mine = my_turn() && G.phase == RH_PLAY;
    for (int r = 0; r < G.h; r++)               /* the tiles from the back */
        for (int col = 0; col < G.w; col++) {
            int c = col + r * G.w;
            if (G.terr[c] == RH_SEA) continue;
            float sx, sy;
            cell_screen(c, &sx, &sy);
            if (sx < view.x - 1.2f * s || sx > view.x + view.w + 1.2f * s || sy < view.y - 0.6f * s ||
                sy > view.y + view.h + 1.6f * s)
                continue;
            float lift, sq;
            Uint8 a;
            uint32_t code = shown_code(c, now, &lift, &sq, &a);
            put_tile(code, sx, sy - lift * s, s, sq, a);
            if (mine && (can_lay(c) || can_blind(c))) {   /* where the player to move may lay track */
                SDL_SetTextureColorMod(t_ring, 255, 236, 170);
                SDL_SetTextureAlphaMod(t_ring, (Uint8)(G.hidden[c] ? 120 : 165));
                ui_put_c(t_ring, sx, sy, sw * 0.77f, shh * 0.77f);
                SDL_SetTextureAlphaMod(t_ring, 255);
                SDL_SetTextureColorMod(t_ring, 255, 255, 255);
            }
            if (c == sel) {                     /* the hex chosen: a gold outline, breathing */
                float z = 0.80f + 0.03f * sinf((now - sel_t0) / 180.0f);
                SDL_SetTextureColorMod(t_ring, 255, 206, 84);
                for (int k = 0; k < 2; k++) ui_put_c(t_ring, sx, sy, sw * z, shh * z);
                SDL_SetTextureColorMod(t_ring, 255, 255, 255);
                ui_animate();
            }
        }
    if (con >= 0)                               /* the chosen contract's towns: red pins, as on its card */
        for (int k = 0; k < 2; k++) {
            float sx, sy;
            cell_screen(G.town[k ? G.con[con].b : G.con[con].a], &sx, &sy);
            float d = fmaxf(0.75f * s, 0.42f * ui.unit), bob = 0.08f * s * sinf(now / 260.0f + k);
            make_coin((int)d);
            if (t_pin) ui_put_c(t_pin, sx, sy - 0.75f * s - d * 0.3f + bob, d, d * 1.4f);
            ui_animate();
        }
    /* the chosen contract's route: a dotted line, a coin on each hex of rent */
    if (np > 1 && !ntrain) {
        pzk_rgba red = {206, 58, 44, 220}, cream = {255, 246, 226, 200};
        for (int i = 0; i + 1 < np; i++) {
            float ax, ay, bx, by;
            cell_screen(path[i], &ax, &ay);
            cell_screen(path[i + 1], &bx, &by);
            ay -= 0.05f * s, by -= 0.05f * s;
            pzk_line(ax, ay, bx, by, fmaxf(3, 0.12f * s), 1, cream, 0);
            for (int k = 0; k < 3; k++) {
                float f = (k + 0.5f) / 3;
                pzk_dot(ax + (bx - ax) * f, ay + (by - ay) * f, fmaxf(3, 0.10f * s), 1, red, 0);
            }
        }
        int v = viewer();
        make_coin((int)fmaxf(0.5f * s, 0.36f * ui.unit));
        for (int i = 0; i < np; i++) {
            int o = G.owner[path[i]];
            if (o >= 0 && o != v && t_coin) {
                float sx, sy;
                cell_screen(path[i], &sx, &sy);
                ui_put_c(t_coin, sx, sy - 0.35f * s, 0.5f * s, 0.5f * s);
            }
        }
    }
    /* the chosen hex's actions, floating over it */
    if (sel >= 0 && my_turn() && G.phase == RH_PLAY) {
        float sx, sy;
        cell_screen(sel, &sx, &sy);
        float d = fmaxf(1.1f * ui.unit, 0.9f * s), y = sy - 0.9f * s - 0.6f * d;
        int acts[3], n = 0;
        if (can_lay(sel)) acts[n++] = PIC_RAIL;
        if (can_blind(sel)) acts[n++] = PIC_BLIND;
        if (can_survey(sel)) acts[n++] = PIC_SURVEY;
        if (y - d / 2 < view.y) y = sy + 0.9f * s + 0.6f * d;
        for (int i = 0; i < n; i++) {
            float x = sx + (i - (n - 1) / 2.0f) * 1.15f * d;
            x = clampf(x, view.x + d / 2, view.x + view.w - d / 2);
            ui_button_paint(x, y, d, painter, acts[i], 0.92f, UI_IVORY | (i == 0 ? UI_LIT : 0));
            int cost = acts[i] == PIC_RAIL ? rh_cost(&G, sel) : acts[i] == PIC_BLIND ? -1 : 1;
            float pd = 0.20f * d;                /* its cost: gold pips under it (blind: one, and a question) */
            int pips = cost < 0 ? 1 : cost;
            for (int k = 0; k < pips; k++) {
                float px = x + (k - (pips - 1) / 2.0f) * 1.3f * pd, py = y + 0.56f * d;
                make_coin((int)fmaxf(0.5f * s, 0.36f * ui.unit));
                ui_mark_dot(px, py, pd * 1.2f);
            }
        }
    }
    draw_train(now);
    draw_fx(now);
    if (had) SDL_RenderSetClipRect(ui.r, &was);
    else SDL_RenderSetClipRect(ui.r, NULL);
}

/* the chosen hex's action buttons: their centres (for the finger) */
static int sel_buttons(float *bx, float *by, int *kind, float *d)
{
    if (sel < 0 || !my_turn() || G.phase != RH_PLAY) return 0;
    float sx, sy, s = cam_s;
    cell_screen(sel, &sx, &sy);
    *d = fmaxf(1.1f * ui.unit, 0.9f * s);
    float y = sy - 0.9f * s - 0.6f * *d;
    int n = 0;
    if (can_lay(sel)) kind[n++] = PIC_RAIL;
    if (can_blind(sel)) kind[n++] = PIC_BLIND;
    if (can_survey(sel)) kind[n++] = PIC_SURVEY;
    if (y - *d / 2 < view.y) y = sy + 0.9f * s + 0.6f * *d;
    for (int i = 0; i < n; i++) {
        bx[i] = clampf(sx + (i - (n - 1) / 2.0f) * 1.15f * *d, view.x + *d / 2, view.x + view.w - *d / 2);
        by[i] = y;
    }
    return n;
}

/* ---- the tray: the deck, the public lines, the hand ---------------------------------------------------------------- */
typedef struct { float x, y, w, h; int kind, i; } spot;   /* kind: 0 deck, 1 public, 2 hand */
static spot SP[RH_HAND + RH_PUB + 2];
static int nsp;

static void tray_spots(void)
{
    nsp = 0;
    int v = viewer(), nh = v >= 0 ? G.pl[v].nhand : 0;
    float u = ui.unit, cw = card_w, ch = card_h, gap = 0.12f * u;
    if (upright) {
        float y = tray.y + (tray.h - ch) / 2, x = tray.x + 0.3f * u;
        SP[nsp++] = (spot){x, y, cw, ch, 0, 0};
        x += cw + 0.35f * u;
        for (int j = 0; j < G.cfg.npub && j < RH_PUB; j++, x += cw + gap) SP[nsp++] = (spot){x, y, cw, ch, 1, j};
        x += 0.25f * u;
        float right = tray.x + tray.w - 0.3f * u, room_w = right - x;
        float step = nh > 1 ? fminf(cw + gap, (room_w - cw) / (nh - 1)) : 0;
        for (int i = 0; i < nh; i++) SP[nsp++] = (spot){x + i * step, y, cw, ch, 2, i};
    } else {
        /* two columns: the deck and a public line, the other public line and the hand */
        float x0 = tray.x + 0.25f * u, y = tray.y + 0.32f * u, cg = tray.w - 0.5f * u - 2 * cw;
        int npub = G.cfg.npub < RH_PUB ? G.cfg.npub : RH_PUB, n = 1 + npub + nh, rows = (n + 1) / 2;
        float bottom = tray.y + tray.h - 0.3f * u;
        float rstep = rows > 1 ? fminf(ch + 0.3f * u, (bottom - y - ch) / (rows - 1)) : 0;
        for (int k = 0; k < n; k++) {
            float x = x0 + (k % 2) * (cw + cg), yy = y + (k / 2) * rstep;
            if (k == 0) SP[nsp++] = (spot){x, yy, cw, ch, 0, 0};
            else if (k <= npub) SP[nsp++] = (spot){x, yy, cw, ch, 1, k - 1};
            else SP[nsp++] = (spot){x, yy, cw, ch, 2, k - 1 - npub};
        }
    }
}

static int deliverable(int kind, int i)
{
    if (!my_turn() || G.phase != RH_PLAY) return 0;
    return rh_legal(&G, kind == 1 ? RH_M_PUBLIC + i : RH_M_DELIVER + i);
}

static void draw_card_at(SDL_Texture *t, float x, float y, float w, float h, int lit, int chosen, Uint8 a)
{
    float lift = chosen ? 0.18f * h : 0;
    ui_shadow_flat(x + w / 2, y + h / 2 - lift + 0.05f * h, w, h, 0.5f);
    if (lit) ui_mark_gold_a(x + w / 2, y + h / 2 - lift, 1.5f * h, 0.55f);
    SDL_SetTextureAlphaMod(t, a);
    ui_put(t, x, y - lift, w, h);
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw_tray(Uint32 now)
{
    if (t_tray) ui_put(t_tray, tray.x, tray.y, tray.w, tray.h);
    tray_spots();
    int v = viewer(), cw = (int)card_w, ch = (int)card_h;
    for (int i = 0; i < nsp; i++) {
        spot *p = &SP[i];
        if (p->kind == 0) {                     /* the deck: backs, how many, lit when it may be drawn */
            int n = G.ndeck, pile = n > 3 ? 3 : n;
            int lit = my_turn() && rh_legal(&G, RH_M_DRAW);
            if (lit) ui_mark_gold_a(p->x + p->w / 2, p->y + p->h / 2, 1.6f * p->h, 0.45f + 0.2f * sinf(now / 260.0f)),
                ui_animate();
            for (int k = pile - 1; k >= 0; k--)
                draw_card_at(back_tex(cw, ch), p->x + k * 0.04f * p->w, p->y - k * 0.04f * p->w, p->w, p->h, 0, 0, 255);
            ui_number_ex(p->x + p->w / 2, p->y + p->h * 0.30f, 0.36f * p->h, n, PUZ_CREAM, 235, 0);
        } else if (p->kind == 1) {              /* a public line: gold edged; delivered: an empty place */
            int id = G.pub[p->i];
            if (id < 0) {
                ui_fill(p->x, p->y, p->w, p->h, 40, 26, 16, 60);
                continue;
            }
            int lit = deliverable(1, p->i), chosen = selc == 100 + p->i;
            ui_fill(p->x - 0.05f * p->w, p->y - 0.05f * p->w - (chosen ? 0.18f * p->h : 0), p->w * 1.1f, p->h + 0.1f * p->w,
                    226, 184, 92, 255);
            draw_card_at(card_tex(id, cw, ch), p->x, p->y, p->w, p->h, lit, chosen, 255);
            /* its bonus: a small coin with the number at the top corner */
            float d = 0.36f * p->w;
            make_coin((int)d);
            float cx = p->x + p->w - 0.12f * p->w, cy = p->y + 0.12f * p->w - (chosen ? 0.18f * p->h : 0);
            if (t_coin) ui_put_c(t_coin, cx, cy, d, d);
            ui_number_ex(cx, cy, 0.55f * d, G.cfg.pub_bonus, PUZ_INK, 255, UI_NUM_TIGHT);
        } else {                                /* the hand: the viewer's own (under the cover: nothing) */
            if (v < 0) continue;
            int id = G.pl[v].hand[p->i], lit = deliverable(2, p->i), chosen = selc == p->i;
            draw_card_at(card_tex(id, cw, ch), p->x, p->y, p->w, p->h, lit, chosen, 255);
        }
    }
}

/* the rounds left (this one too), at the map's top right */
static void draw_rounds(void)
{
    float u = ui.unit, d = 0.5f * u, h = 0.42f * u;
    int left = G.cfg.max_rounds - (G.round < 1 ? 1 : G.round) + 1;
    if (left < 0 || over_now()) left = 0;
    float nw = ui_number_w(left, h), w = nw + 1.0f * u, x = view.x + view.w - w - 0.2f * u, y = view.y + 0.2f * u;
    const uint8_t *ink = ui_table_ink();
    SDL_Texture *hg = ct_icon(ICON_HOURGLASS, (int)d, ink[0], ink[1], ink[2]);
    if (hg) ui_put_c(hg, x + 0.35f * u, y + 0.35f * u, d, d);
    ui_number_ex(x + 0.75f * u + nw / 2, y + 0.35f * u, h, left, ink, 230, 0);
}

/* the offered contracts (a draw): big, over the map, a tap chooses, the tick keeps */
static box offer_box(int i, int n)
{
    float w = fminf(view.w / (n + 0.8f), 2.6f * ui.unit), h = w * 1.32f;
    float gap = 0.2f * w, x0 = view.x + (view.w - n * w - (n - 1) * gap) / 2, y0 = view.y + (view.h - h) / 2 - 0.3f * ui.unit;
    return (box){x0 + i * (w + gap), y0, w, h};
}

static void offer_tick(float *cx, float *cy, float *d)
{
    box b = offer_box(0, G.noffer > 0 ? G.noffer : 1);
    *d = 1.25f * ui.unit;
    *cx = view.x + view.w / 2;
    *cy = b.y + b.h + 0.95f * ui.unit;
}

static void draw_offer(Uint32 now)
{
    if (G.phase != RH_KEEP || viewer() < 0 || viewer() != G.turn || (ui_bot_seats() >> G.turn & 1)) return;
    ui_fill(view.x, view.y, view.w, view.h, 20, 14, 8, 120);
    float f = 1;
    for (int i = 0; i < nfx; i++)
        if (FX[i].kind == FX_OFFER) f = clampf(fx_age(&FX[i], now), 0, 1);
    for (int i = 0; i < G.noffer; i++) {
        box b = offer_box(i, G.noffer);
        float g = ui_ease(clampf(f * 1.6f - 0.25f * i, 0, 1));
        b.y += (1 - g) * 0.6f * b.h;
        int on = keep_mask >> i & 1;
        draw_card_at(card_tex(G.offer[i], (int)b.w, (int)b.h), b.x, b.y, b.w, b.h, on, on, (Uint8)(255 * g));
        if (on) ui_button(b.x + b.w - 0.18f * b.w, b.y - 0.18f * b.h + 0.12f * b.w, 0.55f * ui.unit, ICON_TICK, UI_ON);
    }
    float cx, cy, d;
    offer_tick(&cx, &cy, &d);
    int ok = __builtin_popcount(keep_mask) >= G.keep_min;
    ui_button(cx, cy, d, ICON_TICK, ok ? UI_LIT : UI_DIM);
    if (f < 1) ui_animate();
}

/* the summary of the last turn: a pill at the top of the map, a moment */
static void draw_summary(Uint32 now)
{
    if (last_sum.player < 0 || !last_sum.t0) return;
    int age = (int)(now - last_sum.t0);
    int idle = !now_sum.built && !now_sum.delivered && !now_sum.drew && !now_sum.surveyed && !now_sum.failed;
    if (!idle) age = age > 2800 ? age : 2800;   /* the next player acted: it fades */
    if (age < 0 || (age > 3200 && !idle) || over_now()) return;
    if ((int)(now - last_sum.t0) < 3300) ui_animate();
    if (idle && age > 2800) age = 2800;          /* kept while the next player has not acted */
    float a = clampf((3200 - age) / 400.0f, 0, 1) * clampf(age / 200.0f, 0, 1);
    float u = ui.unit, h = 0.8f * u, d = 0.6f * u;
    int items[4][2], n = 0;
    if (last_sum.built) items[n][0] = PIC_RAIL, items[n++][1] = last_sum.built;
    if (last_sum.surveyed) items[n][0] = PIC_SURVEY, items[n++][1] = last_sum.surveyed;
    if (last_sum.drew) items[n][0] = PIC_CARD, items[n++][1] = -1;
    if (last_sum.coins) items[n][0] = PIC_COIN, items[n++][1] = last_sum.coins;
    float w = 1.0f * u + n * 1.35f * u, x = view.x + (view.w - w) / 2, y = view.y + 0.25f * u;
    ui_fill(x, y, w, h, 30, 22, 14, (Uint8)(190 * a));
    SDL_Texture *tk = pic(PIC_LOCO0 + last_sum.player, (int)d);
    SDL_SetTextureAlphaMod(tk, (Uint8)(255 * a));
    ui_put_c(tk, x + 0.5f * u, y + h / 2, d, d);
    SDL_SetTextureAlphaMod(tk, 255);
    for (int i = 0; i < n; i++) {
        float cx = x + 1.05f * u + i * 1.35f * u;
        SDL_Texture *t = pic(items[i][0], (int)d);
        SDL_SetTextureAlphaMod(t, (Uint8)(255 * a));
        ui_put_c(t, cx + 0.2f * u, y + h / 2, d, d);
        SDL_SetTextureAlphaMod(t, 255);
        if (items[i][1] >= 0) ui_number_ex(cx + 0.82f * u, y + h / 2, 0.42f * u, items[i][1], PUZ_CREAM, (Uint8)(255 * a), 0);
    }
}

/* the end: the final table in the end card (each player: their token, contracts delivered, coins, the penalty, the
 * final score) */
static void draw_final(void)
{
    if (!ui_over()) return;
    float u = ui.unit, rh = 1.0f * u, pw = 7.4f * u, ph = rh * G.np + 0.4f * u, x0, y0;
    endt_begin(room, pw, ph, 0, &x0, &y0);
    unsigned w = rh_winners(&G);
    float d = 0.7f * u;
    for (int p = 0; p < G.np; p++) {
        float cy = y0 + 0.2f * u + (p + 0.5f) * rh;
        if (w >> p & 1) ui_mark_gold(x0 + 0.6f * u, cy, 1.6f * d);
        ui_put_c(pic(PIC_LOCO0 + p, (int)d), x0 + 0.6f * u, cy, d, d);
        ui_put_c(pic(PIC_CARD, (int)(0.55f * u)), x0 + 1.6f * u, cy, 0.55f * u, 0.55f * u);
        ui_number(x0 + 2.25f * u, cy, 0.42f * u, G.pl[p].done, 1);
        ui_put_c(pic(PIC_COIN, (int)(0.5f * u)), x0 + 3.2f * u, cy, 0.5f * u, 0.5f * u);
        ui_number(x0 + 3.9f * u, cy, 0.42f * u, G.pl[p].coins, 1);
        int pen = G.pl[p].coins - rh_final(&G, p);
        if (pen > 0) ui_number_signed(x0 + 5.0f * u, cy, 0.42f * u, -pen, 1, 0);
        ui_number_signed(x0 + 6.5f * u, cy, 0.62f * u, rh_final(&G, p), 1, 0);
        if (p < G.np - 1) ct_rule(x0 + 0.3f * u, cy + rh / 2, pw - 0.6f * u);
    }
    endt_done();
}

static void draw(void)
{
    Uint32 now = ui_ms();
    frame_no++;
    painted = 0;
    cam_at(now);
    if (G.w <= 0) return;
    draw_map(now);
    draw_rounds();
    draw_summary(now);
    draw_tray(now);
    draw_offer(now);
    int mine = my_turn() && G.phase == RH_PLAY;
    ui_tool(T_DONE, ICON_TICK, mine ? (G.acted ? UI_LIT : 0) : UI_DIM);
    ui_tool(T_ZOOM, ICON_SEARCH, zoomed ? UI_ON : 0);
    draw_final();
}

static void bar(int player, box b)
{
    int sz = (int)(0.6f * ui.unit);
    ui_count c[3];
    int n = 0;
    c[n++] = (ui_count){pic(PIC_COIN, sz), G.pl[player].coins};
    c[n++] = (ui_count){pic(PIC_CARD, sz), G.pl[player].nhand};
    if (player == G.turn && G.phase == RH_PLAY) c[n++] = (ui_count){pic(PIC_RAIL, sz), G.pts};
    ui_bar_fit(b, c, n, 0);
}

/* ---- the pointer: a tap chooses, one finger drags the map, two pinch --------------------------------------------- */
#define MAX_FINGERS 2
static struct { int id; float x, y; } fg[MAX_FINGERS];
static int nfg, moved, pinching, mouse_down, tool_down = -1;
static float g_x0, g_y0, g_cx0, g_cy0, g_d0, g_s0, g_wx, g_wy;

static float slop(void) { return 0.016f * fminf(ui.w, ui.h); }
static int in_box(box b, float x, float y) { return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h; }

static void choose(int c)
{
    sel = c;
    sel_t0 = ui_ms();
    selc = -1;
    ui_sound(SND_TAP);
    ui_animate();
}

static void do_cell(int c, int kind)
{
    if (kind == PIC_SURVEY) act(RH_M_SURVEY + c, 0);
    else act(RH_M_BUILD + c, 0);
}

static void zoom_toggle(void)
{
    if (zoomed) { zoomed = 0; user_cam = 0; cam_auto(0); return; }
    zoomed = 1;
    user_cam = 1;
    float x = cam_x, y = cam_y;
    int c = sel;
    if (c < 0 && G.last >= 0) c = G.last;
    if (c >= 0) world_of(c, &x, &y);
    cam_to(x, y, fminf(s_most(), 0.11f * fminf(ui.w, ui.h)));
}

static void tap_at(float x, float y)
{
    if (G.phase == RH_KEEP && my_turn()) {      /* choosing the contracts to keep */
        float cx, cy, d;
        offer_tick(&cx, &cy, &d);
        if (ui_button_hit(cx, cy, d, x, y)) {
            if (__builtin_popcount(keep_mask) >= G.keep_min) act(RH_M_KEEP + (int)keep_mask, 0);
            else ui_sound(SND_DENY);
            return;
        }
        for (int i = 0; i < G.noffer; i++)
            if (in_box(offer_box(i, G.noffer), x, y)) {
                keep_mask ^= 1u << i;
                ui_sound(SND_TAP);
                ui_animate();
                return;
            }
        return;
    }
    tray_spots();
    for (int i = nsp - 1; i >= 0; i--) {        /* the tray: the deck, a contract */
        spot *p = &SP[i];
        if (!(x >= p->x && x < p->x + p->w && y >= p->y - 0.2f * p->h && y < p->y + p->h)) continue;
        if (p->kind == 0) {
            if (my_turn() && rh_legal(&G, RH_M_DRAW)) act(RH_M_DRAW, 0);
            else ui_sound(SND_DENY);
            return;
        }
        int code = p->kind == 1 ? 100 + p->i : p->i;
        if (p->kind == 1 && G.pub[p->i] < 0) return;
        if (selc == code && deliverable(p->kind, p->i)) { act(sel_move(), 0); return; }
        if (selc == code) { selc = -1; ui_sound(SND_TAP); ui_animate(); return; }
        selc = code;
        sel = -1;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (!in_box(view, x, y)) return;
    float bx[3], by[3], d;
    int kind[3], n = sel_buttons(bx, by, kind, &d);
    for (int i = 0; i < n; i++)
        if (ui_button_hit(bx[i], by[i], d, x, y)) { do_cell(sel, kind[i]); return; }
    int c = pick(x, y);
    if (c < 0) { sel = -1; selc = -1; ui_animate(); return; }
    if (c == sel && n > 0 && my_turn()) { do_cell(c, kind[0]); return; }
    choose(c);
}

static void tool(int t)
{
    if (t == T_ZOOM) { zoom_toggle(); return; }
    if (t == T_DONE) {
        if (my_turn() && G.phase == RH_PLAY) act(RH_M_END, 0);
        else ui_sound(SND_DENY);
    }
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

static void release_tool(float x, float y)
{
    int t = tool_down;
    tool_down = -1;
    ui_animate();
    if (ui_tool_hit(x, y) == t) tool(t);
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
    if (tool_down >= 0) { release_tool(x, y); return; }
    if (!mouse_down) return;
    mouse_down = 0;
    if (!moved) tap_at(x, y);
}

/* the fingers: one drags, two pinch (a game with finger() is played as a race by the shell's bots: its input is never
 * held, so a person's taps on a bot's turn are dropped here, and busy() paces the bots' moves) */
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
        else {
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
        if (phase == FINGER_UP && tool_down >= 0) release_tool(x, y);
        return;
    }
    fg[k].x = x, fg[k].y = y;
    if (phase == FINGER_MOVE) {
        if (nfg == 2) {
            float d = fmaxf(1, hypotf(fg[1].x - fg[0].x, fg[1].y - fg[0].y));
            float s = clampf(g_s0 * d / g_d0, s_least(), s_most());
            float mx = (fg[0].x + fg[1].x) / 2, my = (fg[0].y + fg[1].y) / 2;
            user_cam = zoomed = 1;
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

/* the keys: the arrows choose a hex (the nearest that way), Return lays track there, S surveys, D draws, 1..8 a
 * contract (again: deliver), P the public lines, Space ends the turn, + - 0 the zoom */
static void key(SDL_Keycode k)
{
    if (k == SDLK_PLUS || k == SDLK_EQUALS || k == SDLK_KP_PLUS || k == SDLK_PAGEUP) {
        user_cam = zoomed = 1;
        cam_to(cam_go ? cam_tx : cam_x, cam_go ? cam_ty : cam_y, (cam_go ? cam_ts : cam_s) * 1.35f);
        return;
    }
    if (k == SDLK_MINUS || k == SDLK_KP_MINUS || k == SDLK_PAGEDOWN) {
        user_cam = zoomed = 1;
        cam_to(cam_go ? cam_tx : cam_x, cam_go ? cam_ty : cam_y, (cam_go ? cam_ts : cam_s) / 1.35f);
        return;
    }
    if (k == SDLK_0 || k == SDLK_HOME) { user_cam = zoomed = 0; cam_auto(0); return; }
    if (!my_turn()) { ui_sound(SND_DENY); return; }
    if (G.phase == RH_KEEP) {
        if (k >= SDLK_1 && k < SDLK_1 + G.noffer) { keep_mask ^= 1u << (k - SDLK_1); ui_sound(SND_TAP); ui_animate(); }
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
            if (__builtin_popcount(keep_mask) >= G.keep_min) act(RH_M_KEEP + (int)keep_mask, 0);
            else ui_sound(SND_DENY);
        }
        return;
    }
    if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN) {
        float dx = k == SDLK_LEFT ? -1 : k == SDLK_RIGHT ? 1 : 0, dy = k == SDLK_UP ? -1 : k == SDLK_DOWN ? 1 : 0;
        float ox = cam_x, oy = cam_y;
        if (sel >= 0) world_of(sel, &ox, &oy);
        else if (G.last >= 0) world_of(G.last, &ox, &oy);
        int best = -1;
        float bd = 1e9f;
        for (int c = 0; c < G.nc; c++) {
            if (c == sel || G.terr[c] == RH_SEA) continue;
            float wx, wy;
            world_of(c, &wx, &wy);
            float ax = wx - ox, ay = wy - oy, along = ax * dx + ay * dy, across = fabsf(ax * dy - ay * dx);
            if (along <= 0.3f) continue;
            float d = along + 2.2f * across;
            if (d < bd) { bd = d; best = c; }
        }
        if (best >= 0) choose(best);
        else ui_sound(SND_DENY);
    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        if (sel >= 0 && rh_legal(&G, RH_M_BUILD + sel)) act(RH_M_BUILD + sel, 0);
        else if (selc >= 0 && rh_legal(&G, sel_move())) act(sel_move(), 0);
        else ui_sound(SND_DENY);
    } else if (k == SDLK_s) {
        if (sel >= 0 && rh_legal(&G, RH_M_SURVEY + sel)) act(RH_M_SURVEY + sel, 0);
        else ui_sound(SND_DENY);
    } else if (k == SDLK_d) {
        if (rh_legal(&G, RH_M_DRAW)) act(RH_M_DRAW, 0);
        else ui_sound(SND_DENY);
    } else if (k == SDLK_SPACE) act(RH_M_END, 0);
    else if (k >= SDLK_1 && k <= SDLK_8) {
        int i = k - SDLK_1, v = viewer();
        if (v < 0 || i >= G.pl[v].nhand) { ui_sound(SND_DENY); return; }
        if (selc == i && rh_legal(&G, RH_M_DELIVER + i)) act(RH_M_DELIVER + i, 0);
        else { selc = i; sel = -1; ui_sound(SND_TAP); ui_animate(); }
    } else if (k == SDLK_p) {
        int j = selc >= 100 ? (selc - 100 + 1) % RH_PUB : 0;
        for (int t = 0; t < RH_PUB; t++, j = (j + 1) % RH_PUB)
            if (G.pub[j] >= 0) break;
        if (G.pub[j] < 0) { ui_sound(SND_DENY); return; }
        if (selc == 100 + j && rh_legal(&G, RH_M_PUBLIC + j)) act(RH_M_PUBLIC + j, 0);
        else { selc = 100 + j; sel = -1; ui_sound(SND_TAP); ui_animate(); }
    }
}

/* ---- the shell's pictures ------------------------------------------------------------------------------------------ */
/* a little island of tiles (offset rows), painted into a picture size x size */
static void paint_patch(const uint32_t *codes, int w, int h, int size, uint8_t *rgba, float most)
{
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++) {
            if (!codes[c + r * w]) continue;
            float wx = SQ3 * (c + 0.5f * (r & 1)), wy = 1.5f * r;
            x0 = fminf(x0, wx - AP), x1 = fmaxf(x1, wx + AP);
            y0 = fminf(y0, (wy - 1 - OVER / TILT) * TILT), y1 = fmaxf(y1, (wy + 1) * TILT + THICK);
        }
    float rp = fminf(fminf(0.94f * size / (x1 - x0), 0.94f * size / (y1 - y0)), most * size);
    float ox = size / 2.0f - rp * (x0 + x1) / 2, oy = size / 2.0f - rp * (y0 + y1) / 2;
    for (int r = 0; r < h; r++)                 /* soft shadows on the table */
        for (int c = 0; c < w; c++) {
            if (!codes[c + r * w]) continue;
            float cx = ox + SQ3 * (c + 0.5f * (r & 1)) * rp + 0.1f * rp, cy = oy + 1.5f * r * TILT * rp + (THICK + 0.12f) * rp;
            for (int y = (int)(cy - rp) < 0 ? 0 : (int)(cy - rp); y < size && y < (int)(cy + rp); y++)
                for (int x = (int)(cx - rp) < 0 ? 0 : (int)(cx - rp); x < size && x < (int)(cx + rp); x++) {
                    float sd = hexsdf((x + 0.5f - cx) / rp, (y + 0.5f - cy) / (rp * TILT));
                    float a = 0.4f * smooth(0.2f, -0.12f, sd);
                    if (a > 0) px_over(rgba + 4 * ((size_t)y * size + x), 16, 22, 10, a);
                }
        }
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            if (codes[c + r * w])
                paint_tile(rgba, size, size, ox + SQ3 * (c + 0.5f * (r & 1)) * rp, oy + 1.5f * r * TILT * rp, rp,
                           codes[c + r * w]);
}

/* the menu's card and the picker's: a small valley, a town, two lines of track (misty: some hexes face down) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    enum { M = RH_MEADOW, F = RH_FOREST, W = RH_FIELD, Hh = RH_HILL, R = RH_RIVER, Mt = RH_MOUNTAIN, T = RH_TOWN };
    /* 4 x 4, odd rows shifted right: directions 0 E, 1 SE, 2 SW, 3 W, 4 NW, 5 NE */
    uint32_t c[16] = {0};
    int misty = variant == 0 || variant < 0;
#define K(i, t, rv, rl, ow) c[i] = code_make(t, rv, rl, ow, 0, i * 5)
    K(0, F, 0, 0, 7);
    K(1, Mt, 0, 0, 7);
    K(2, R, 1 << 2 | 1 << 5, 0, 7);
    K(4, M, 0, 1 << 0 | 1 << 1, 0);
    K(5, T, 0, 0, 7);
    K(6, R, 1 << 5 | 1 << 2, 1 << 3 | 1 << 0, 1);
    K(7, Hh, 0, 1 << 3 | 1 << 2, 1);
    K(8, W, 0, 1 << 5 | 1 << 1, 0);
    K(9, M, 0, 0, 7);
    K(10, R, 1 << 5 | 1 << 2, 0, 7);
    K(11, F, 0, 0, 7);
    K(13, W, 0, 1 << 4 | 1 << 0, 0);
    K(14, misty ? T_BACK : M, 0, 0, 7);
    K(15, misty ? T_BACK : F, 0, 0, 7);
#undef K
    if (misty) c[11] = code_make(T_BACK, 0, 0, 7, 0, 11), c[0] = code_make(T_BACK, 0, 0, 7, 0, 0);
    paint_patch(c, 4, 4, size, rgba, variant < 0 ? 0.20f : 0.17f);
}

static void token(int player, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    paint_loco(rgba, size, player, 0.04f * size, 0.06f * size, 0.92f * size);
}

static const game railhills_game = {
    .id = "railhills", .order = 470, .variants = VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4, .view = VIEW_HIDDEN, .finger = finger, .key = key,
    .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(railhills_game)
