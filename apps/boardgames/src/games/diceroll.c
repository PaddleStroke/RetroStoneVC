/*
 * Dice Roll in RetroStone Board, for one player: stone tiles floating over the green table in the diamond view (the
 * map turned an eighth of a turn, seen from above at a slant: each tile a diamond with its front edges and its
 * shadow), an ivory die with ebony pips (the 1 in red lacquer) drawn in three dimensions, so it truly rolls over its
 * edge onto the next tile. The goal: a brass frame round the pips of the number it wants, in gold; number tiles: the
 * pips inlaid in lapis; odd and even tiles: jade with one ivory ring or two; arrows: brass (the rules:
 * diceroll_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The map's ways on the screen: up (the rules' DCR_UP) runs up and right, right down and right, down down and left,
 * left up and left. Swipe on the board (the closest of the four), tap beside the die (it rolls that way), tap one of
 * the four arrows (the shell's tools, along the tiles' edges), or use the arrow keys (up rolls up and right). A roll
 * it cannot make: the die tips a little and settles back, its arrow dimmed. The lamp: the hint (the next roll of a
 * shortest way, its tile and its arrow in gold light; from a dead end, it first takes back the rolls that led
 * there). Undo takes back one roll, all the way to the start. The bar: the rolls, and at its right end the par by a
 * small die. Solved: the die hops on the goal in a swell of gold light.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/diceroll.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/swipepad.h"
#include "shell.h"

#define KB 0.29f                                /* half a diamond's height on the screen, of its width (a cell) */
#define KZ 0.50f                                /* a unit of height on the screen, of a cell */
#define TH 0.30f                                /* a tile's thickness */
#define DS 0.80f                                /* the die's side, of a cell */
#define ROLL_MS 190
#define TIP_MS 240
#define WIN_MS 1100

static const uint8_t STONE[3] = {190, 182, 168}, LAPIS[3] = {58, 96, 178}, JADE[3] = {52, 146, 104},
                     BRASS[3] = {214, 170, 84}, GOLD[3] = {246, 202, 92}, IVORY[3] = {246, 238, 220},
                     EBONY[3] = {40, 32, 26}, LACQUER[3] = {186, 46, 36}, WELL[3] = {58, 40, 28};

static dcr_game G;
static int serial, hint_way = -1, down = -1;
static swp_swipe sw;
static struct { int on, dir; dcr_state from; Uint32 t0; } roll;
static struct { int on, dir; Uint32 t0; } tip;
static struct { int on; Uint32 t0; } win;

static box last_box;
static float ox, oy, cell;                      /* the map's picture's top left on the screen, a diamond's width */
static int bw, bh, map_h = 1, made_cell = -1, made_serial = -1, made_face = -1, made_small = -1;
static SDL_Texture *t_board, *t_face[7], *t_small;

/* ---- the diamond view ------------------------------------------------------------------------------------------- */
/* a point of the map (x, y in cells from the top left, z up, in cells) in a picture of cells c, the map mh rows high:
 * the top corner of cell (0, 0) at (mh c / 2, 0) */
static void iso(float x, float y, float z, float c, int mh, float *sx, float *sy)
{
    *sx = (x - y + mh) * 0.5f * c;
    *sy = (x + y) * KB * c - z * KZ * c;
}

static void proj(float x, float y, float z, float *sx, float *sy)
{
    iso(x, y, z, cell, map_h, sx, sy);
    *sx += ox;
    *sy += oy;
}

/* the screen's direction of each of the map's ways (degrees, 0 right, 90 down) */
static float way_deg(int way)
{
    static const float D[4] = {-30.1f, 30.1f, 149.9f, 210.1f};
    return D[way & 3];
}

/* the way whose screen direction is closest to (dx, dy) */
static int way_of(float dx, float dy)
{
    int best = 0;
    float bd = -1e9f;
    for (int w = 0; w < 4; w++) {
        float a = way_deg(w) * PUZ_PI / 180, d = dx * cosf(a) + dy * sinf(a);
        if (d > bd) { bd = d; best = w; }
    }
    return best;
}

/* ---- painting ---------------------------------------------------------------------------------------------------- */
static float rbox(float u, float v, float lo, float hi, float r)   /* a rounded square lo..hi: its distance */
{
    float c = (lo + hi) / 2, h = (hi - lo) / 2 - r, dx = fabsf(u - c) - h, dy = fabsf(v - c) - h;
    return hypotf(fmaxf(dx, 0), fmaxf(dy, 0)) + fminf(fmaxf(dx, dy), 0) - r;
}

static void mix(float *acc, const uint8_t rgb[3], float a)   /* an opaque colour laid over acc by a */
{
    for (int i = 0; i < 3; i++) acc[i] += (rgb[i] - acc[i]) * clampf(a, 0, 1);
}

/* the pips of a die's face n (1..6) at (u, v) of the face: how much a pip covers it (aa: the edge's softness) */
static float pips(int n, float u, float v, float r, float aa)
{
    static const float P[7][6][2] = {
        {{0}},
        {{0.5f, 0.5f}},
        {{0.27f, 0.27f}, {0.73f, 0.73f}},
        {{0.27f, 0.27f}, {0.5f, 0.5f}, {0.73f, 0.73f}},
        {{0.27f, 0.27f}, {0.73f, 0.27f}, {0.27f, 0.73f}, {0.73f, 0.73f}},
        {{0.27f, 0.27f}, {0.73f, 0.27f}, {0.5f, 0.5f}, {0.27f, 0.73f}, {0.73f, 0.73f}},
        {{0.27f, 0.25f}, {0.27f, 0.5f}, {0.27f, 0.75f}, {0.73f, 0.25f}, {0.73f, 0.5f}, {0.73f, 0.75f}},
    };
    float best = 0;
    for (int i = 0; i < n && n <= 6; i++) {
        float d = hypotf(u - P[n][i][0], v - P[n][i][1]) - r;
        best = fmaxf(best, clampf(0.5f - d / aa, 0, 1));
    }
    return best;
}

/* a die's face n at (u, v): ivory, its rim shaded (a rounded edge), ebony pips (the 1 big, in red lacquer) */
static void face_rgb(int n, float u, float v, float aa, float *acc)
{
    float e = fminf(fminf(u, 1 - u), fminf(v, 1 - v));
    for (int i = 0; i < 3; i++) acc[i] = IVORY[i] * (0.80f + 0.20f * clampf(e / 0.12f, 0, 1));
    mix(acc, n == 1 ? LACQUER : EBONY, pips(n, u, v, n == 1 ? 0.14f : 0.088f, aa));
}

/* a tile's top at (u, v) (0..1 across it): stone, its rim bevelled, and its mark */
static void top_rgb(const dcr_map *m, int c, float u, float v, float aa, float n, float *acc)
{
    int t = c < 0 ? DCR_TILE : m->t[c], k = c < 0 ? 0 : m->k[c];
    float e = -rbox(u, v, 0, 1, 0.08f), b = 0.08f, f = 1 + 0.06f * n;
    if (e < b) f += (u + v < 1 ? 0.10f : -0.12f) * (1 - e / b);
    for (int i = 0; i < 3; i++) acc[i] = STONE[i] * f;
    if (t == DCR_NUM || t == DCR_ODD || t == DCR_EVEN) {   /* an inlay: lapis (numbers), jade (odd, even) */
        float d = rbox(u, v, 0.15f, 0.85f, 0.1f);
        const uint8_t *col = t == DCR_NUM ? LAPIS : JADE;
        uint8_t dk[3];
        for (int i = 0; i < 3; i++) dk[i] = (uint8_t)(col[i] * 0.55f);
        mix(acc, dk, 0.5f - (d - 0.02f) / aa);
        uint8_t in[3];
        float lit = 1 + 0.12f * clampf((1 - u - v) * 2, -1, 1);
        for (int i = 0; i < 3; i++) in[i] = (uint8_t)clampf(col[i] * lit, 0, 255);
        mix(acc, in, 0.5f - d / aa);
        if (t == DCR_NUM) {                     /* the pips, the inlay's area mapped to a face */
            float fu = (u - 0.15f) / 0.7f, fv = (v - 0.15f) / 0.7f;
            mix(acc, IVORY, pips(k, fu, fv, k == 1 ? 0.13f : 0.085f, aa / 0.7f));
        } else {                                /* one ring (odd) or two (even) */
            float best = 0;
            for (int i = 0; i < (t == DCR_ODD ? 1 : 2); i++) {
                float cx = t == DCR_ODD ? 0.5f : i ? 0.66f : 0.34f, cy = cx;
                float d2 = fabsf(hypotf(u - cx, v - cy) - (t == DCR_ODD ? 0.17f : 0.12f)) - 0.035f;
                best = fmaxf(best, clampf(0.5f - d2 / aa, 0, 1));
            }
            mix(acc, IVORY, best);
        }
    } else if (t == DCR_ARROW) {                /* a brass arrow along its way, outlined dark */
        static const float AX[4] = {0, 1, 0, -1}, AY[4] = {-1, 0, 1, 0};
        float du = u - 0.5f, dv = v - 0.5f, a = du * AX[k] + dv * AY[k], bb = fabsf(-du * AY[k] + dv * AX[k]);
        float shaft = fmaxf(bb - 0.075f, fmaxf(-0.32f - a, a - 0.06f));
        float head = fmaxf(-0.02f - a, (bb - (0.36f - a) * 0.85f) * 0.76f);
        float d = fminf(shaft, head);
        uint8_t dk[3] = {96, 66, 30};
        mix(acc, dk, 0.5f - (d - 0.025f) / aa);
        float lit = 1 + 0.18f * clampf((1 - u - v) * 2, -1, 1);
        uint8_t br[3];
        for (int i = 0; i < 3; i++) br[i] = (uint8_t)clampf(BRASS[i] * lit, 0, 255);
        mix(acc, br, 0.5f - d / aa);
    } else if (t == DCR_GOAL) {                 /* a brass frame, a dark well, the goal's pips in gold */
        float d = rbox(u, v, 0.1f, 0.9f, 0.1f);
        mix(acc, BRASS, 0.5f - d / aa);
        float w = rbox(u, v, 0.18f, 0.82f, 0.06f);
        uint8_t well[3];
        float k2 = 1 - 0.3f * clampf(1 - fminf(u, v) * 4, 0, 1);
        for (int i = 0; i < 3; i++) well[i] = (uint8_t)(WELL[i] * k2);
        mix(acc, well, 0.5f - w / aa);
        float fu = (u - 0.18f) / 0.64f, fv = (v - 0.18f) / 0.64f;
        mix(acc, GOLD, pips(m->face, fu, fv, m->face == 1 ? 0.14f : 0.09f, aa / 0.64f));
    }
}

/* one tile at cell (x, y) in the picture o (W x H): its top's back corner at (X0, Y0), cells c wide; fx, fy: its
 * front right (along x) and front left (along y) edges shown. Two by two samples a pixel. */
static void paint_tile(uint8_t *o, int W, int H, float X0, float Y0, float c, const dcr_map *m, int cc, int fx,
                       int fy)
{
    float hc = 0.5f * c, kc = KB * c, g = 0.035f, aa = 1.6f / c;
    for (int y = (int)floorf(Y0) - 1; y < (int)ceilf(Y0 + 2 * kc + TH * KZ * c) + 1; y++)
        for (int x = (int)floorf(X0 - hc) - 1; x < (int)ceilf(X0 + hc) + 1; x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            float acc[4] = {0}, n = art_noise(x, y);
            for (int s = 0; s < 4; s++) {
                float dx = x + 0.25f + 0.5f * (s & 1) - X0, dy = y + 0.25f + 0.5f * (s >> 1) - Y0;
                float u = (dx / hc + dy / kc) / 2, v = (dy / kc - dx / hc) / 2, col[3];
                float tu = (u - g) / (1 - 2 * g), tv = (v - g) / (1 - 2 * g);
                if (tu >= 0 && tu <= 1 && tv >= 0 && tv <= 1 && rbox(tu, tv, 0, 1, 0.08f) <= 0) {
                    top_rgb(m, cc, tu, tv, aa, n, col);
                } else {                        /* an edge: under the top's front rims */
                    float sv = 1 - dx / hc, su = 1 + dx / hc, z;
                    if (dx >= 0 && fx && sv >= g && sv <= 1 - g) {
                        z = ((1 - g + sv) * kc - dy) / (KZ * c);
                        if (z > 0.01f || z < -TH) continue;
                        for (int i = 0; i < 3; i++) col[i] = STONE[i] * (0.58f - 0.12f * (-z / TH)) * (1 + 0.05f * n);
                    } else if (dx < 0 && fy && su >= g && su <= 1 - g) {
                        z = ((su + 1 - g) * kc - dy) / (KZ * c);
                        if (z > 0.01f || z < -TH) continue;
                        for (int i = 0; i < 3; i++) col[i] = STONE[i] * (0.76f - 0.12f * (-z / TH)) * (1 + 0.05f * n);
                    } else continue;
                }
                for (int i = 0; i < 3; i++) acc[i] += col[i];
                acc[3] += 1;
            }
            if (acc[3] <= 0) continue;
            uint8_t rgb[3];
            for (int i = 0; i < 3; i++) rgb[i] = (uint8_t)clampf(acc[i] / acc[3], 0, 255);
            pzk_over(o + 4 * ((size_t)y * W + x), rgb, acc[3] / 4);
        }
}

static int is_tile(const dcr_map *m, int x, int y)
{
    return x >= 0 && y >= 0 && x < m->w && y < m->h && m->t[x + y * m->w] != DCR_VOID;
}

static float board_h(int wh) { return wh * KB + TH * KZ + 0.24f; }   /* the map's picture's height, in cells */

/* the map: its shadows on the table, then its tiles back to front */
static void paint_board(const dcr_map *m, float c, int W, int H, uint8_t *o)
{
    memset(o, 0, (size_t)W * H * 4);
    static const uint8_t shade[3] = {0, 0, 0};
    float hc = 0.5f * c, kc = KB * c, drop = TH * KZ * c + 0.12f * c;
    for (int cc = 0; cc < m->w * m->h; cc++) {
        if (m->t[cc] == DCR_VOID) continue;
        float X0, Y0;
        iso((float)(cc % m->w), (float)(cc / m->w), 0, c, m->h, &X0, &Y0);
        X0 += 0.04f * c;
        Y0 += drop;
        for (int yy = (int)Y0; yy < Y0 + 2 * kc + 1; yy++)
            for (int xx = (int)(X0 - hc); xx < X0 + hc + 1; xx++) {
                if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
                float dx = xx + 0.5f - X0, dy = yy + 0.5f - Y0;
                float u = (dx / hc + dy / kc) / 2, v = (dy / kc - dx / hc) / 2, d = rbox(u, v, 0, 1, 0.2f);
                if (d < 0) pzk_over(o + 4 * ((size_t)yy * W + xx), shade, 0.2f * clampf(-d / 0.15f, 0, 1));
            }
    }
    for (int s = 0; s < m->w + m->h - 1; s++)
        for (int x = 0; x < m->w; x++) {
            int y = s - x;
            if (y < 0 || y >= m->h || m->t[x + y * m->w] == DCR_VOID) continue;
            float X0, Y0;
            iso((float)x, (float)y, 0, c, m->h, &X0, &Y0);
            paint_tile(o, W, H, X0, Y0, c, m, x + y * m->w, !is_tile(m, x + 1, y), !is_tile(m, x, y + 1));
        }
}

/* ---- the die ---------------------------------------------------------------------------------------------------- */
/* a model face (0..5: +z, -z, +x, -x, +y, -y): its number, its normal, its two directions across (u, v) */
static const int FACE_N[6] = {1, 6, 3, 4, 2, 5};
static const float FN[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
static const float FU[6][3] = {{1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {-1, 0, 0}, {1, 0, 0}};
static const float FV[6][3] = {{0, 1, 0}, {0, -1, 0}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}};

static void rot3(const int8_t *R, const float *a, float *b)
{
    for (int i = 0; i < 3; i++) b[i] = R[3 * i] * a[0] + R[3 * i + 1] * a[1] + R[3 * i + 2] * a[2];
}

/* a point of the die (p: from its centre, model, in sides) on the map: its way o, standing on cell (cx, cy), rolled
 * deg towards dir over its bottom edge, lifted by lift (cells) */
static void die_point(const float *p, int o, float cx, float cy, int dir, float deg, float lift, float *w)
{
    float q[3];
    rot3(dcr_rot(o), p, q);
    w[0] = cx + 0.5f + q[0] * DS;
    w[1] = cy + 0.5f + q[1] * DS;
    w[2] = DS / 2 + q[2] * DS;
    if (dir >= 0 && deg != 0) {                 /* turned round the bottom edge ahead */
        static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
        float a = deg * PUZ_PI / 180, ca = cosf(a), sa = sinf(a);
        int ax = DX[dir] ? 0 : 1, sgn = DX[dir] + DY[dir];   /* ax: the axis it moves along is y (1) or x (0) */
        float piv = (ax ? cy : cx) + 0.5f + sgn * DS / 2, s = ((ax ? w[1] : w[0]) - piv) * sgn, z = w[2];
        float s2 = s * ca + z * sa, z2 = -s * sa + z * ca;
        if (ax) w[1] = piv + s2 * sgn;
        else w[0] = piv + s2 * sgn;
        w[2] = z2;
    }
    w[2] += lift;
}

static void dir_of(const float *n, int o, int dir, float deg, float *w)   /* a direction, turned as the die */
{
    static const float Z[3] = {0, 0, 0};
    float a[3], b[3];
    die_point(n, o, 0, 0, dir, deg, 0, a);
    die_point(Z, o, 0, 0, dir, deg, 0, b);
    for (int i = 0; i < 3; i++) w[i] = (a[i] - b[i]) / DS;
}

/* the die on the screen: its faces turned to the eye, each its picture, lit from the upper left */
static void draw_die(int o, float cx, float cy, int dir, float deg, float lift, float glow)
{
    static const float EYE[3] = {1, 1, 2 * KB / KZ}, L[3] = {-0.25f, 0.30f, 0.92f};
    for (int f = 0; f < 6; f++) {
        float n[3];
        dir_of(FN[f], o, dir, deg, n);
        if (n[0] * EYE[0] + n[1] * EYE[1] + n[2] * EYE[2] <= 0.01f) continue;
        float lit = n[0] * L[0] + n[1] * L[1] + n[2] * L[2];
        Uint8 s = (Uint8)clampf(255 * (0.5f + 0.52f * fmaxf(0, lit) + 0.25f * glow), 0, 255);
        SDL_Vertex v[4];
        static const float U[4] = {0, 1, 1, 0}, V[4] = {0, 0, 1, 1};
        for (int k = 0; k < 4; k++) {
            float p[3], w[3], sx, sy;
            for (int i = 0; i < 3; i++) p[i] = FN[f][i] * 0.5f + (U[k] - 0.5f) * FU[f][i] + (V[k] - 0.5f) * FV[f][i];
            die_point(p, o, cx, cy, dir, deg, lift, w);
            proj(w[0], w[1], w[2], &sx, &sy);
            v[k] = (SDL_Vertex){{sx, sy}, {s, s, s, 255}, {U[k], V[k]}};
        }
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(ui.r, t_face[FACE_N[f]], v, 4, idx, 6);
    }
}

/* its soft shadow on the tiles: three dark diamonds, each a little wider (k: how dark) */
static void draw_shadow(float x0, float y0, float k)
{
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
    for (int l = 0; l < 3; l++) {
        float e = 0.5f - DS / 2 - 0.03f - 0.05f * l, q[4][2] = {{x0 + e, y0 + e}, {x0 + 1 - e, y0 + e},
                                                               {x0 + 1 - e, y0 + 1 - e}, {x0 + e, y0 + 1 - e}};
        SDL_Vertex v[4];
        for (int i = 0; i < 4; i++) {
            float sx, sy;
            proj(q[i][0] + 0.06f, q[i][1] + 0.06f, 0, &sx, &sy);
            v[i] = (SDL_Vertex){{sx, sy}, {0, 0, 0, (Uint8)clampf(k * (62 - 16 * l), 0, 255)}, {0, 0}};
        }
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(ui.r, NULL, v, 4, idx, 6);
    }
}

/* the die painted in pixels (the pictures of the shell: the menu's card, the bar's): its way o, its footprint's top
 * corner at (X0, Y0) in o (W x H), cells c wide; three samples a pixel each way */
static void paint_die(uint8_t *out, int W, int H, float X0, float Y0, float c, int o)
{
    float hc = 0.5f * c, kc = KB * c, m = (1 - DS) / 2, aa = 2.0f / (DS * c);
    const int8_t *R = dcr_rot(o);
    for (int y = (int)(Y0 - DS * KZ * c) - 2; y < Y0 + 2 * kc + 2; y++)
        for (int x = (int)(X0 - hc) - 2; x < X0 + hc + 2; x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            float acc[3] = {0}, cov = 0;
            for (int s = 0; s < 9; s++) {
                float dx = x + (s % 3 + 0.5f) / 3 - X0, dy = y + (s / 3 + 0.5f) / 3 - Y0, p[3], k;
                /* the top (z = DS), then the two front faces (x = 1 - m, y = 1 - m); p: the model's point (sides
                 * from the die's centre) */
                float dt = dy + DS * KZ * c, u = (dx / hc + dt / kc) / 2, v = (dt / kc - dx / hc) / 2;
                if (u >= m && u <= 1 - m && v >= m && v <= 1 - m) {
                    p[0] = (u - 0.5f) / DS; p[1] = (v - 0.5f) / DS; p[2] = 0.5f; k = 1;
                } else {
                    float px, py, pz;
                    if (dx >= 0) { px = 1 - m; py = px - dx / hc; k = 0.66f; }
                    else { py = 1 - m; px = py + dx / hc; k = 0.84f; }
                    pz = ((px + py) * kc - dy) / (KZ * c);
                    if (px < m || px > 1 - m || py < m || py > 1 - m || pz < 0 || pz > DS) continue;
                    p[0] = dx >= 0 ? 0.5f : (px - 0.5f) / DS;
                    p[1] = dx >= 0 ? (py - 0.5f) / DS : 0.5f;
                    p[2] = pz / DS - 0.5f;
                }
                float q[3];                     /* the map's point back to the model: R^T */
                for (int i = 0; i < 3; i++) q[i] = R[i] * p[0] + R[3 + i] * p[1] + R[6 + i] * p[2];
                int f = fabsf(q[2]) > 0.499f ? (q[2] > 0 ? 0 : 1) : fabsf(q[0]) > 0.499f ? (q[0] > 0 ? 2 : 3)
                                                                                       : (q[1] > 0 ? 4 : 5);
                float fu = 0.5f + q[0] * FU[f][0] + q[1] * FU[f][1] + q[2] * FU[f][2];
                float fv = 0.5f + q[0] * FV[f][0] + q[1] * FV[f][1] + q[2] * FV[f][2], col[3];
                face_rgb(FACE_N[f], fu, fv, aa, col);
                for (int i = 0; i < 3; i++) acc[i] += col[i] * k;
                cov += 1;
            }
            if (cov <= 0) continue;
            uint8_t rgb[3];
            for (int i = 0; i < 3; i++) rgb[i] = (uint8_t)clampf(acc[i] / cov, 0, 255);
            pzk_over(out + 4 * ((size_t)y * W + x), rgb, cov / 9);
        }
}

/* ---- the arrows: drawn along the tiles' edges (the map's ways on the screen) ------------------------------------- */
static const int TOOL_WAY[SWP_KNOBS] = {DCR_DOWN, DCR_LEFT, DCR_UP, DCR_RIGHT, SWP_HINT};

static void way_arrow(int way, int size, uint8_t *o)
{
    static const uint8_t white[3] = {255, 255, 255};
    float a = way_deg(way) * PUZ_PI / 180, ca = cosf(a), sa = sinf(a);
    art_fig *f = art_fig_new();
    art_band(f, ART_PART, 0.5f - 0.3f * ca, 0.5f - 0.3f * sa, 0.5f + 0.12f * ca, 0.5f + 0.12f * sa, 0.13f);
    art_head(f, ART_PART, 0.5f + 0.34f * ca, 0.5f + 0.34f * sa, way_deg(way), 0.34f);
    art_fig_paint(o, size, f, white, NULL, 0, 0);
}

static int tool_hit(float x, float y)
{
    int i = ui_tool_hit(x, y);
    return i < 0 || i >= SWP_KNOBS ? -1 : TOOL_WAY[i];
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void die_pic(int size, uint8_t *rgba)   /* a small die (the bar, the token) */
{
    float c = size / (1.0f + 0.1f);
    paint_die(rgba, size, size, size / 2.0f, (size - (2 * KB + DS * KZ) * c) / 2 + DS * KZ * c, c, 0);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    die_pic(size, rgba);
}

static float extent_w(const dcr_map *m) { return (m->w + m->h) * 0.5f + 0.2f; }   /* in cells */
static float extent_h(const dcr_map *m) { return board_h(m->w + m->h) + DS * KZ + 0.5f; }

static void make_faces(int n)
{
    for (int v = 1; v <= 6; v++) {
        uint8_t *p = ui_buf(n, n);
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                float acc[3];
                face_rgb(v, (x + 0.5f) / n, (y + 0.5f) / n, 1.5f / n, acc);
                uint8_t *q = p + 4 * ((size_t)y * n + x);
                for (int i = 0; i < 3; i++) q[i] = (uint8_t)clampf(acc[i], 0, 255);
                q[3] = 255;
            }
        ui_free(&t_face[v]);
        t_face[v] = ui_tex(n, n, p);
    }
}

static void layout(box b)
{
    last_box = b;
    map_h = G.m.h;
    cell = floorf(fminf(b.w / extent_w(&G.m), b.h / extent_h(&G.m)));
    bw = (int)ceilf((G.m.w + G.m.h) * 0.5f * cell) + 1;
    bh = (int)ceilf(board_h(G.m.w + G.m.h) * cell) + 1;
    ox = floorf(b.x + (b.w - bw) / 2);
    oy = floorf(b.y + (b.h - extent_h(&G.m) * cell) / 2 + (DS * KZ + 0.3f) * cell);
    if (made_cell != (int)cell || made_serial != serial) {
        made_cell = (int)cell;
        made_serial = serial;
        uint8_t *o = malloc((size_t)bw * bh * 4);
        ui_free(&t_board);
        if (o) {
            paint_board(&G.m, cell, bw, bh, o);
            t_board = ui_tex(bw, bh, o);
        }
        free(o);
    }
    int face = (int)fmaxf(16, 0.9f * cell);
    if (made_face != face) {
        made_face = face;
        make_faces(face);
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        die_pic(small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    roll.on = tip.on = win.on = 0;
    hint_way = down = -1;
    sw.on = 0;
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!dcr_level(&G, level)) dcr_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    dcr_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int solved(void) { return dcr_solved(&G.m, &G.s); }
static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return ui_par_stars(G.moves, par()); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (roll.on && now - roll.t0 < ROLL_MS) || (win.on && now - win.t0 < WIN_MS);
}

static void push(int way)
{
    if (solved() || way < 0) return;
    dcr_state before = G.s;
    hint_way = -1;
    if (!dcr_move(&G, way)) {
        tip.on = 1;
        tip.dir = way;
        tip.t0 = ui_ms();
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    roll.on = 1;
    roll.dir = way;
    roll.from = before;
    roll.t0 = ui_ms();
    ui_sound(SND_MOVE);
    ui_sound_later(SND_PLACE, ROLL_MS - 20);
    if (solved()) {
        win.on = 1;
        win.t0 = roll.t0 + ROLL_MS;
        ui_sound_later(SND_DICE, ROLL_MS + 120);
    }
    ui_animate();
}

static void key(SDL_Keycode k)
{
    int w = swp_key_way(k);
    if (w >= 0) push(w);
}

/* the hint: the next roll of a shortest way; from a dead end, back first */
static void hint(void)
{
    if (solved()) { ui_sound(SND_DENY); return; }
    int d, n = dcr_solve(&G.m, &G.s, &d, NULL, NULL);
    while (n < 0 && dcr_undo(&G)) n = dcr_solve(&G.m, &G.s, &d, NULL, NULL);
    roll.on = 0;
    if (n <= 0) { ui_sound(SND_DENY); return; }
    hint_way = d;
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    calm();
    return dcr_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = dcr_script(&G, moves);
    if (solved()) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void cell_centre(int c, float *sx, float *sy) { proj(c % G.m.w + 0.5f, c / G.m.w + 0.5f, 0, sx, sy); }

static void draw(void)
{
    if (made_cell < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_board, ox, oy, (float)bw, (float)bh);
    float gx, gy;
    cell_centre(G.m.goal, &gx, &gy);
    if (!solved()) ui_mark_gold_a(gx, gy, 1.1f * cell, 0.32f + 0.12f * sinf(now / 420.0f));   /* the goal breathes */
    if (!solved()) ui_animate();
    if (hint_way >= 0 && !roll.on) {            /* the hint: the tile the die rolls onto, in gold light */
        dcr_state n;
        if (dcr_try(&G.m, &G.s, hint_way, &n)) {
            float hx, hy;
            cell_centre(n.x + n.y * G.m.w, &hx, &hy);
            ui_mark_gold(hx, hy, 1.0f * cell);
        }
    }
    if (roll.on && now - roll.t0 >= ROLL_MS) roll.on = 0;
    if (roll.on) {
        float t = puz_ease((now - roll.t0) / (float)ROLL_MS);
        static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
        draw_shadow(roll.from.x + DX[roll.dir] * t, roll.from.y + DY[roll.dir] * t, 0.8f);
        draw_die(roll.from.o, roll.from.x, roll.from.y, roll.dir, 90 * t, 0, 0);
        ui_animate();
    } else {
        float deg = 0, lift = 0, glow = 0;
        int dir = -1;
        if (tip.on) {
            float t = (now - tip.t0) / (float)TIP_MS;
            if (t >= 1) tip.on = 0;
            else { deg = 14 * sinf(PUZ_PI * t); dir = tip.dir; ui_animate(); }
        }
        if (win.on) {                           /* two hops on the goal, gold light swelling under it */
            float wt = (float)(int)(now - win.t0);
            if (wt >= 0 && wt < WIN_MS) {
                float f = wt / (WIN_MS * 0.6f);
                if (f < 1) lift = 0.45f * fabsf(sinf(PUZ_PI * 2 * f)) * (1 - 0.45f * (f > 0.5f));
                float q = puz_pulse(wt, 0, WIN_MS);
                glow = q;
                ui_mark_gold(gx, gy, (1.2f + 1.8f * q) * cell * (0.4f + 0.6f * q));
                ui_animate();
            } else if (wt >= WIN_MS) glow = 0.15f;
        }
        draw_shadow(G.s.x, G.s.y, 1 - 0.8f * lift);
        draw_die(G.s.o, G.s.x, G.s.y, dir, deg, lift, glow);
    }
    unsigned can = 0;
    for (int w = 0; w < 4; w++) {
        dcr_state n;
        if (!solved() && dcr_try(&G.m, &G.s, w, &n)) can |= 1u << w;
    }
    for (int i = 0; i < SWP_KNOBS; i++) {
        int k = TOOL_WAY[i];
        if (k == SWP_HINT) {
            ui_tool(i, ICON_HINT, (solved() ? UI_DIM : 0) | (down == k ? UI_ON : 0));
            continue;
        }
        int lit = !roll.on && k == hint_way;
        ui_tool_paint(i, way_arrow, k, 0.62f,
                      UI_TINT | (lit ? UI_LIT : 0) | (can >> k & 1 ? 0 : UI_DIM) | (down == k ? UI_ON : 0));
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void tap_at(float x, float y)            /* beside the die: it rolls that way; on it: refused */
{
    float cx, cy;
    proj(G.s.x + 0.5f, G.s.y + 0.5f, DS / 2, &cx, &cy);
    if (fabsf(x - cx) < 0.45f * cell && fabsf(y - cy) < 0.5f * cell) { ui_sound(SND_DENY); return; }
    push(way_of(x - cx, y - cy));
}

static void press(float x, float y)
{
    sw.on = 0;
    down = tool_hit(x, y);
    if (down >= 0) { ui_animate(); return; }
    if (!solved()) swp_begin(&sw, x, y);
}

static void motion(float x, float y)
{
    if (swp_follow(&sw, x, y, 0.6f * ui.unit) >= 0) push(way_of(x - sw.x0, y - sw.y0));
}

static void release(float x, float y)
{
    if (down >= 0) {
        int k = down;
        down = -1;
        ui_animate();
        if (tool_hit(x, y) != k) return;
        if (k == SWP_HINT) hint();
        else if (solved()) ui_sound(SND_DENY);
        else push(k);
        return;
    }
    int tap;
    if (swp_end(&sw, x, y, 0.3f * ui.unit, &tap) >= 0) push(way_of(x - sw.x0, y - sw.y0));
    else if (tap && !solved()) tap_at(sw.x0, sw.y0);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two(b, NULL, G.moves, t_small, par(), solved() && G.moves <= par());
}

/* ---- the shell's pictures: a map with the die at its start ------------------------------------------------------- */
static void paint_map(const dcr_map *m, int size, uint8_t *rgba)
{
    float c = floorf(fminf(size * 0.96f / extent_w(m), size * 0.96f / extent_h(m)));
    if (c < 3) return;
    int W = (int)ceilf((m->w + m->h) * 0.5f * c) + 1, H = (int)ceilf(board_h(m->w + m->h) * c) + 1;
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    paint_board(m, c, W, H, o);
    int x0 = (size - W) / 2, y0 = (int)((size - extent_h(m) * c) / 2 + (DS * KZ + 0.3f) * c);
    art_blend(rgba, size, size, o, W, H, x0, y0);
    free(o);
    float sx, sy;
    iso((float)(m->start % m->w), (float)(m->start / m->w), 0, c, m->h, &sx, &sy);
    paint_die(rgba, size, size, x0 + sx, y0 + sy, c, m->o0);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    dcr_game *t = malloc(sizeof *t);
    if (!t) return;
    if (variant < 0) dcr_level(t, 30);
    else                                        /* a puzzle of the variant near square: its card reads well */
        for (uint32_t s = 5; s < 25; s++) {
            dcr_random(t, variant, s + 40u * (uint32_t)variant);
            if (abs(t->m.w - t->m.h) <= 1) break;
        }
    paint_map(&t->m, size, rgba);
    free(t);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    dcr_game *t = malloc(sizeof *t);
    if (t && dcr_level(t, level)) paint_map(&t->m, size, rgba);
    free(t);
}

/* a chapter's tab: a small map showing its tiles */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const ICON[DCR_CHAPTERS] = {
        "112:So|oG", "312:S3|oG", "512:S5o|o.o|ooG", "212:Si|eG", "112:S>|oG", "612:S3|>e|oG",
    };
    dcr_map m;
    if (chapter >= 0 && chapter < DCR_CHAPTERS && dcr_parse(&m, ICON[chapter])) paint_map(&m, size, rgba);
}

static const game diceroll_game = {
    .id = "diceroll", .order = 447, .variants = DCR_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = DCR_LEVELS, .chapters = DCR_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1, .tools = SWP_KNOBS,
};
REGISTER_GAME(diceroll_game)
