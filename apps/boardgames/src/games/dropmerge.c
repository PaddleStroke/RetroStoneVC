/*
 * Fruit Drop in RetroStone Board, for one player: a glass jar on a wooden stand, a brass rail along its top with a
 * dropper holding the next fruit (the rules: dropmerge_rules.c). Drag along the jar (or tap) to move the dropper; let
 * go to drop. Two fruits of a kind that touch pop into the next one with a squash, a flash and a splash of juice, the
 * points rising; a chain of merges shows its count. The stand shows the eleven fruits in order, those made so far lit.
 * A dashed line marks the rim: it glows amber when the fruits come near it and red while one stays above it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The physics runs on the shell's play clock (ui_play_ms: still while the game is away or covered): the world steps
 * DMJ_HZ times a second from the game's start, and a
 * drop made before the dropper is ready waits in the game for its tick, so a game played again from its record (and
 * the pictures) comes out the same. No undo (physics): the restart tool plays the same jar again from the start. The
 * lamp (a hint) lights a gold line where the fruit would best fall. The bar: endless, the score and the best; a
 * level, what is left of the goal and the fruits left. Stars: endless by the score; a level by the drops against the
 * maker's, no more than the hints allow.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/dropmerge.h"
#include "games/puzrand.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "shell.h"

/* the view, units: the widest jar with its glass, the rail and the next fruit above it, the stand under it */
#define GL 10.0f                                /* the glass's thickness */
#define VX0 (-GL - 18)
#define VX1 (DMJ_WIDE + GL + 18)
#define VY0 (-160.0f)
#define RAILY (-100.0f)
#define STAND 74.0f                             /* the stand's height under the glass */
#define VY1 (DMJ_JH + GL + STAND + 6)
#define VW (VX1 - VX0)
#define VH (VY1 - VY0)
#define PAD 1.3f                                /* a fruit's picture: room round it for the stem and the leaf */
#define HINT_MS 2600
#define END_MS 1400
#define WIN_MS 1500
#define NFX 48
#define NSQ 40

enum { T_RESTART, T_HINT, T_COUNT };

typedef struct { uint8_t base[3], juice[3]; float gloss; } fruit_t;
static const fruit_t FR[DMJ_SIZES + 1] = {
    {{0, 0, 0}, {0, 0, 0}, 0},
    {{186, 16, 36}, {230, 40, 70}, 1.0f},      /* cherry */
    {{222, 38, 50}, {250, 80, 90}, 0.8f},      /* strawberry */
    {{98, 50, 146}, {170, 90, 210}, 0.55f},    /* grape */
    {{242, 128, 22}, {255, 170, 60}, 0.7f},    /* mandarin */
    {{244, 212, 46}, {255, 236, 110}, 0.75f},  /* lemon */
    {{200, 30, 34}, {255, 110, 90}, 0.95f},    /* apple */
    {{250, 172, 126}, {255, 200, 160}, 0.35f}, /* peach */
    {{188, 200, 66}, {226, 236, 120}, 0.6f},   /* pear */
    {{112, 72, 44}, {236, 230, 214}, 0.2f},    /* coconut */
    {{198, 206, 120}, {196, 236, 140}, 0.5f},  /* melon */
    {{48, 132, 50}, {246, 70, 80}, 0.8f},      /* watermelon */
};

static dmj_game G;
static int serial, made_serial = -1, made_k = -1, made_small = -1;
static Uint32 t0;
static int32_t base_tick;
static int drop_x = -1, dragging, tool_down = -1, pend, pend_x, seen_ev, variant;
static int best[DMJ_VARIANTS], best_read;
static Uint32 hint_t0, end_t0, last_land_snd;
static int hint_x = -1, biggest_seen, shown_end;
static struct { int type, size, pts, chain; float x, y; Uint32 t; uint16_t id; } fx[NFX];
static int nfx;
static struct { uint16_t id; Uint32 t; float amp; int land; } sq[NSQ];
static int nsq;
static struct { int n; Uint32 t; } chainfx;
static Uint32 big_t;                            /* a new biggest fruit: its place on the stand glows */

static box last_box;
static float k, ox, oy, jx;                     /* pixels a unit; the view's top left; the jar's left inside (units) */
static SDL_Texture *t_body[DMJ_SIZES + 1], *t_shade[DMJ_SIZES + 1], *t_spec[DMJ_SIZES + 1], *t_icon[DMJ_SIZES + 1];
static SDL_Texture *t_back, *t_front, *t_rail, *t_peg, *t_dot, *t_shadow, *t_chain, *t_star, *t_tok,
                   *t_goal, *t_left;
static int back_w, back_h, peg_d;
static float back_x0, back_y0;                  /* the back's top left (units in the view) */

/* ---- the clock ------------------------------------------------------------------------------------------------- */
static int32_t tick_at(Uint32 now) { return base_tick + (int32_t)((int64_t)(int32_t)(now - t0) * DMJ_HZ / 1000); }
static double tick_pos(Uint32 now) { return base_tick + (double)(int32_t)(now - t0) * DMJ_HZ / 1000.0; }
static Uint32 tick_ms(int32_t tick) { return t0 + (Uint32)((int64_t)(tick - base_tick) * 1000 / DMJ_HZ); }

static void dropped(int x)
{
    if (dmj_drop(&G.w, x)) ui_sound_soft(SND_DROP, 0.55f);
}

/* the world stepped to now; a drop waiting is made at the dropper's tick */
static void advance(Uint32 now)
{
    int32_t target = tick_at(now);
    for (int guard = 0; guard < 200000; guard++) {
        if (pend && dmj_can_drop(&G.w)) {
            pend = 0;
            dropped(pend_x);
        }
        if (pend && G.w.end != DMJ_PLAYING) pend = 0;
        if (G.w.tick >= target) break;
        if (G.w.end == DMJ_OVERFLOW) { G.w.tick = target; break; }
        int32_t stop = target;
        if (pend && G.w.ready > G.w.tick && G.w.ready < stop) stop = G.w.ready;
        if (G.w.rest) dmj_run(&G.w, stop - G.w.tick);
        else dmj_step(&G.w);
    }
}

static void rebase(void)
{
    t0 = ui_play_ms();
    base_tick = G.w.tick;
    seen_ev = G.w.nev;
    nfx = nsq = 0;
    pend = dragging = 0;
    tool_down = -1;
    hint_x = -1;
    end_t0 = 0;
    shown_end = 0;
    chainfx.n = 0;
    biggest_seen = G.w.biggest;
    big_t = 0;
}

/* ---- painting -------------------------------------------------------------------------------------------------- */
static float hash2(int x, int y, int s)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}

static float vnoise(float x, float y, int s)  /* smooth value noise 0..1 */
{
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    float a = hash2(xi, yi, s), b = hash2(xi + 1, yi, s), c = hash2(xi, yi + 1, s), d = hash2(xi + 1, yi + 1, s);
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
}

static void mix3(float *c, const float *o, float t)
{
    for (int i = 0; i < 3; i++) c[i] += (o[i] - c[i]) * t;
}

/* a fruit's own colour at a point of its skin: (x, y, z) on the unit sphere, y down, z towards the eye */
static void albedo(int s, float x, float y, float z, float *c)
{
    const uint8_t *b = FR[s].base;
    c[0] = b[0], c[1] = b[1], c[2] = b[2];
    float u = atan2f(x, z), v = asinf(clampf(y, -1, 1));
    switch (s) {
    case 1: {                                   /* cherry: darker low down */
        float d = 0.85f + 0.15f * (1 - y);
        for (int i = 0; i < 3; i++) c[i] *= d;
        break;
    }
    case 2: {                                   /* strawberry: seeds in rows */
        float gu = u * 7 / PUZ_PI, gv = (v + 1.6f) * 4.2f;
        int row = (int)floorf(gv);
        float cu = gu + (row & 1) * 0.5f, du = cu - floorf(cu) - 0.5f, dv = gv - row - 0.5f;
        float dd = sqrtf(du * du + dv * dv * 1.6f);
        if (y > -0.7f && dd < 0.2f) {
            const float seed[3] = {250, 214, 120}, rim[3] = {150, 20, 30};
            mix3(c, dd < 0.13f ? seed : rim, dd < 0.13f ? 0.9f : 0.6f);
        }
        if (y > 0.4f) mix3(c, (const float[]){200, 20, 40}, (y - 0.4f) * 0.4f);
        break;
    }
    case 3: {                                   /* grape: a dusty bloom */
        float n = vnoise(x * 6 + 3, y * 6, 3);
        mix3(c, (const float[]){150, 120, 190}, 0.18f * n + 0.12f * (1 - z));
        break;
    }
    case 4:
    case 5: {                                   /* citrus: pores */
        float n = vnoise(x * 26, y * 26, s) * 0.6f + vnoise(x * 9, y * 9, s + 7) * 0.4f;
        for (int i = 0; i < 3; i++) c[i] *= 0.9f + 0.16f * n;
        if (s == 5) mix3(c, (const float[]){230, 226, 90}, 0.25f * clampf(-y, 0, 1));
        break;
    }
    case 6: {                                   /* apple: a yellow blush in streaks low on one side */
        float st = vnoise(u * 9, v * 1.5f, 6), side = clampf(0.55f * (y + 0.2f) - 0.5f * x, 0, 1);
        mix3(c, (const float[]){236, 196, 70}, clampf(side * (0.6f + 0.6f * st), 0, 0.85f));
        for (int i = 0; i < 3; i++) c[i] *= 0.94f + 0.1f * vnoise(u * 20, v * 3, 16);
        break;
    }
    case 7: {                                   /* peach: a red cheek, the crease */
        float cheek = clampf(0.7f * (x + 0.2f) + 0.25f * vnoise(x * 3, y * 3, 7), 0, 1);
        mix3(c, (const float[]){226, 74, 62}, 0.8f * cheek * cheek);
        float cr = x - 0.18f * sinf((y + 1) * 1.4f);
        float k2 = expf(-cr * cr / 0.004f) * clampf(0.6f - y, 0, 1);
        for (int i = 0; i < 3; i++) c[i] *= 1 - 0.32f * k2;
        break;
    }
    case 8: {                                   /* pear: freckles */
        float gu = u * 9, gv = v * 9;       /* round freckles, one by chance in a cell */
        int cu = (int)floorf(gu), cv = (int)floorf(gv);
        for (int a = -1; a <= 1; a++)
            for (int b2 = -1; b2 <= 1; b2++) {
                int pu = cu + a, pv = cv + b2;
                if (hash2(pu, pv, 81) > 0.55f) continue;
                float fu = pu + hash2(pu, pv, 82), fv = pv + hash2(pu, pv, 83), rr = 0.07f + 0.06f * hash2(pu, pv, 84);
                float d = hypotf(gu - fu, gv - fv);
                if (d < rr) mix3(c, (const float[]){140, 120, 54}, 0.55f * clampf((rr - d) / 0.04f, 0, 1));
            }
        mix3(c, (const float[]){220, 170, 70}, 0.3f * clampf(x + y, 0, 1));
        break;
    }
    case 9: {                                   /* coconut: fibres, three eyes */
        float f = vnoise(u * 30, v * 4, 9) * 0.7f + vnoise(u * 70, v * 9, 19) * 0.3f;
        for (int i = 0; i < 3; i++) c[i] *= 0.7f + 0.55f * f;
        for (int e = 0; e < 3; e++) {
            float ex = 0.12f * cosf(e * 2.094f) + 0.05f, ey = -0.62f + 0.12f * sinf(e * 2.094f);
            float d = hypotf(x - ex, y - ey);
            if (d < 0.07f) mix3(c, (const float[]){46, 28, 18}, clampf((0.07f - d) / 0.02f, 0, 1));
        }
        break;
    }
    case 10: {                                  /* melon: a pale net */
        float n = vnoise(u * 5.5f, v * 5.5f, 10), m = vnoise(u * 11, v * 11, 20);
        float net = fminf(fabsf(n - 0.5f), fabsf(m - 0.5f) * 1.4f);
        if (net < 0.05f) mix3(c, (const float[]){236, 232, 196}, clampf((0.05f - net) / 0.02f, 0, 1) * 0.85f);
        break;
    }
    case 11: {                                  /* watermelon: dark wavy stripes */
        float w = sinf(u * 6.5f + 0.5f * sinf(v * 9) + 0.25f * sinf(v * 23 + u * 3));
        if (w > 0.15f) mix3(c, (const float[]){22, 74, 30}, clampf((w - 0.15f) / 0.12f, 0, 1));
        break;
    }
    }
}

/* a capsule's (stem's) coverage at (x, y): from a to b, half width hw */
static float capsule(float x, float y, float ax, float ay, float bx, float by, float hw, float px)
{
    float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, t = l2 > 0 ? ((x - ax) * dx + (y - ay) * dy) / l2 : 0;
    t = clampf(t, 0, 1);
    float d = hypotf(x - ax - t * dx, y - ay - t * dy);
    return clampf((hw - d) / px + 0.5f, 0, 1);
}

/* a leaf from a to b, its widest wd: coverage, and how far across it (-1..1) */
static float leaf(float x, float y, float ax, float ay, float bx, float by, float wd, float px, float *across)
{
    float dx = bx - ax, dy = by - ay, l = hypotf(dx, dy);
    float t = ((x - ax) * dx + (y - ay) * dy) / (l * l), d = ((x - ax) * dy - (y - ay) * dx) / l;
    if (t <= 0 || t >= 1) return 0;
    float half = wd * sinf(PUZ_PI * powf(t, 0.8f));
    *across = d / (half + 1e-4f);
    return clampf((half - fabsf(d)) / px + 0.5f, 0, 1);
}

/* the stem, the leaf, the calyx (in the fruit's own frame, -PAD..PAD; px: a pixel): their colour over c, coverage */
static float extras(int s, float x, float y, float px, float *c)
{
    static const float STEM[3] = {104, 78, 40}, GREEN[3] = {70, 140, 44}, DGREEN[3] = {40, 100, 34}, TAN[3] = {150, 140, 80};
    float a, across = 0, best = 0;
    float col[3] = {0, 0, 0};
#define LAY(cov, rgb, shade)                                                                                         \
    do {                                                                                                             \
        float cv_ = (cov);                                                                                           \
        if (cv_ > 0) {                                                                                               \
            for (int i_ = 0; i_ < 3; i_++) col[i_] += ((rgb)[i_] * (shade) - col[i_]) * cv_;                          \
            best = best + (1 - best) * cv_;                                                                          \
        }                                                                                                            \
    } while (0)
    switch (s) {
    case 1:
        a = capsule(x, y, 0.02f, -0.82f, 0.18f, -1.08f, 0.055f, px);
        LAY(a, STEM, 1);
        a = capsule(x, y, 0.18f, -1.08f, 0.42f, -1.24f, 0.05f, px);
        LAY(a, STEM, 1.1f);
        a = leaf(x, y, 0.3f, -1.18f, 0.78f, -1.08f, 0.13f, px, &across);
        LAY(a, across < 0 ? GREEN : DGREEN, 1);
        break;
    case 2:
        for (int i = 0; i < 5; i++) {
            float ang = -PUZ_PI / 2 + (i - 2) * 0.62f;
            a = leaf(x, y, 0, -0.86f, 0.62f * cosf(ang), -0.86f + 0.5f * sinf(ang) + 0.32f, 0.12f, px, &across);
            LAY(a, i & 1 ? DGREEN : GREEN, 1 - 0.15f * across);
        }
        a = capsule(x, y, 0, -0.86f, 0.04f, -1.15f, 0.05f, px);
        LAY(a, GREEN, 0.9f);
        break;
    case 3:
    case 8:
        a = capsule(x, y, 0, -0.88f, 0.08f, -1.2f, s == 3 ? 0.05f : 0.06f, px);
        LAY(a, STEM, 1);
        break;
    case 4:
        a = leaf(x, y, 0.04f, -0.94f, 0.55f, -1.16f, 0.16f, px, &across);
        LAY(a, across < 0 ? GREEN : DGREEN, 1);
        a = capsule(x, y, 0, -0.9f, 0.03f, -1.02f, 0.06f, px);
        LAY(a, STEM, 0.9f);
        break;
    case 5:
        a = capsule(x, y, 0, -0.95f, 0, -1.05f, 0.08f, px);
        LAY(a, FR[5].base, 0.86f);
        break;
    case 6:
        a = capsule(x, y, 0, -0.74f, 0.08f, -1.18f, 0.05f, px);
        LAY(a, STEM, 1);
        a = leaf(x, y, 0.07f, -1.02f, 0.62f, -1.2f, 0.17f, px, &across);
        LAY(a, across < 0 ? GREEN : DGREEN, 1);
        break;
    case 7:
        a = leaf(x, y, 0.0f, -0.96f, -0.5f, -1.2f, 0.14f, px, &across);
        LAY(a, across < 0 ? GREEN : DGREEN, 1);
        break;
    case 10:
        a = capsule(x, y, 0, -0.95f, 0, -1.14f, 0.045f, px);
        LAY(a, TAN, 1);
        a = capsule(x, y, -0.12f, -1.12f, 0.12f, -1.12f, 0.04f, px);
        LAY(a, TAN, 1);
        break;
    }
#undef LAY
    if (best > 0)
        for (int i = 0; i < 3; i++) c[i] = col[i] / best;
    return best;
}

/* the fruit's own picture (it turns with it): its skin, the stem and the leaf; D pixels for PAD units round */
static void paint_body(int s, int D, uint8_t *o)
{
    float R = D / (2 * PAD), m = D / 2.0f, px = 1 / R;
    for (int py = 0; py < D; py++)
        for (int qx = 0; qx < D; qx++) {
            float acc[3] = {0, 0, 0}, cov = 0, ex[3] = {0, 0, 0}, exa = 0;
            for (int sy = 0; sy < 2; sy++)
                for (int sx = 0; sx < 2; sx++) {
                    float x = (qx + 0.25f + 0.5f * sx - m) / R, y = (py + 0.25f + 0.5f * sy - m) / R, r2 = x * x + y * y;
                    if (r2 < 1) {
                        float c[3];
                        albedo(s, x, y, sqrtf(1 - r2), c);
                        for (int i = 0; i < 3; i++) acc[i] += c[i];
                        cov += 1;
                    }
                }
            float x = (qx + 0.5f - m) / R, y = (py + 0.5f - m) / R;
            exa = extras(s, x, y, px, ex);
            uint8_t *p = o + 4 * ((size_t)py * D + qx);
            float a = cov / 4, col[3];
            for (int i = 0; i < 3; i++) col[i] = a > 0 ? acc[i] / cov : 0;
            float out = exa + a * (1 - exa);
            for (int i = 0; i < 3; i++) {
                float v = out > 0 ? (ex[i] * exa + col[i] * a * (1 - exa)) / out : 0;
                p[i] = (uint8_t)clampf(v, 0, 255);
            }
            p[3] = (uint8_t)(255 * clampf(out, 0, 1) + 0.5f);
        }
}

/* the light on a fruit (it never turns): spec 0 the shade (black, by how dark), 1 the shine (white) */
static float light_at(int s, float x, float y, int spec)
{
    float r2 = x * x + y * y;
    if (r2 >= 0.996f) {                         /* at the edge: the light just inside it */
        float f = sqrtf(0.996f / r2);
        x *= f, y *= f, r2 = 0.996f;
    }
    float z = sqrtf(1 - r2);
    const float L[3] = {-0.48f, -0.62f, 0.62f}, H[3] = {-0.3f, -0.4f, 0.87f};
    float nl = x * L[0] + y * L[1] + z * L[2], nh = x * H[0] + y * H[1] + z * H[2];
    float g = FR[s].gloss;
    if (!spec) {
        float lit = 0.52f + 0.6f * fmaxf(0, nl) + 0.12f * clampf(0.6f * x + 0.6f * y, 0, 1);   /* a bounce low right */
        float rim = clampf((sqrtf(r2) - 0.86f) / 0.14f, 0, 1);
        return clampf(1 - lit, 0, 1) * 0.9f + 0.18f * rim * rim;
    }
    nh = fmaxf(0, nh);
    return clampf(g * (0.85f * powf(nh, 70) + 0.22f * powf(nh, 8)) + 0.05f * g * clampf(-y - 0.2f, 0, 1), 0, 1);
}

static void paint_light(int s, int D, int spec, uint8_t *o)
{
    float R = D / (2 * PAD), m = D / 2.0f;
    float grow = (1 + 1.5f / R) * (1 + 1.5f / R);
    for (int py = 0; py < D; py++)
        for (int qx = 0; qx < D; qx++) {
            float a = 0, cov = 0;
            for (int sy = 0; sy < 2; sy++)
                for (int sx = 0; sx < 2; sx++) {
                    float x = (qx + 0.25f + 0.5f * sx - m) / R, y = (py + 0.25f + 0.5f * sy - m) / R;
                    a += light_at(s, x, y, spec);
                    cov += x * x + y * y < grow;  /* a pixel past the edge: no light rim where the fruit is drawn turned */
                }
            a *= fminf(1, cov / 2);
            uint8_t *p = o + 4 * ((size_t)py * D + qx);
            p[0] = p[1] = p[2] = spec ? 255 : 0;
            p[3] = (uint8_t)(255 * clampf(a / 4, 0, 1));
        }
}

/* the whole fruit lit, into a picture (icons, thumbnails): D pixels across for PAD units, centred at (cx, cy) */
static void paint_fruit_at(uint8_t *dst, int W, int H, int s, float cx, float cy, int D, float tilt)
{
    if (D < 4 || s < 1 || s > DMJ_SIZES) return;
    uint8_t *b = malloc((size_t)D * D * 4);
    if (!b) return;
    paint_body(s, D, b);
    float R = D / (2 * PAD), m = D / 2.0f;
    for (int py = 0; py < D; py++)
        for (int qx = 0; qx < D; qx++) {
            uint8_t *p = b + 4 * ((size_t)py * D + qx);
            float x = (qx + 0.5f - m) / R, y = (py + 0.5f - m) / R;
            if (x * x + y * y > 1.02f) continue;
            float sh = light_at(s, x, y, 0), sp = light_at(s, x, y, 1);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * (1 - sh) + (255 - p[i] * (1 - sh)) * sp, 0, 255);
        }
    /* a soft shadow first */
    float sr = R * 1.02f, scx = cx + 0.1f * R, scy = cy + 0.16f * R;
    for (int y = (int)fmaxf(0, scy - sr - 4); y < H && y < scy + sr + 4; y++)
        for (int x = (int)fmaxf(0, scx - sr - 4); x < W && x < scx + sr + 4; x++) {
            float d = hypotf(x + 0.5f - scx, y + 0.5f - scy);
            float a = 0.38f * clampf((sr + 0.12f * R - d) / (0.3f * R), 0, 1);
            if (a > 0) pzk_over(dst + 4 * ((size_t)y * W + x), (const uint8_t[]){0, 0, 0}, a);
        }
    if (tilt != 0) art_blend_rot(dst, W, H, b, D, D, cx, cy, tilt);
    else art_blend(dst, W, H, b, D, D, (int)lroundf(cx - m), (int)lroundf(cy - m));
    free(b);
}

static void paint_icon(int s, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    float rr = (float)dmj_radius[s] / dmj_radius[DMJ_SIZES];
    int D = (int)(size * (0.62f + 0.32f * rr));
    paint_fruit_at(rgba, size, size, s, size * 0.48f, size * 0.53f, D, 0);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    memset(rgba, 0, (size_t)size * size * 4);
    paint_fruit_at(rgba, size, size, 6, size * 0.48f, size * 0.54f, (int)(size * 0.98f), 0);
}

/* a horizontal brass rod (the rail), lit as a cylinder: from x0 to x1 at cy, radius r */
static void paint_rod(uint8_t *o, int W, int H, float x0, float x1, float cy, float r)
{
    for (int y = (int)fmaxf(0, cy - r - 2); y < H && y <= cy + r + 2; y++)
        for (int x = (int)fmaxf(0, x0 - r); x < W && x <= x1 + r; x++) {
            float dx = x + 0.5f < x0 ? x0 - x - 0.5f : x + 0.5f > x1 ? x + 0.5f - x1 : 0, dy = y + 0.5f - cy;
            float d = hypotf(dx, dy), cov = clampf(r - d + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float q = dy / r, kk = 0.62f + 0.5f * (1 - fabsf(q)) - 0.25f * q + 0.35f * expf(-powf((q + 0.45f) * 4, 2));
            uint8_t c[3] = {(uint8_t)clampf(224 * kk, 0, 255), (uint8_t)clampf(172 * kk, 0, 255), (uint8_t)clampf(76 * kk, 0, 255)};
            pzk_over(o + 4 * ((size_t)y * W + x), c, cov);
        }
}

/* a brass stud (a peg), d across */
static void paint_peg(int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    float r = d * 0.42f, c = d / 2.0f;
    for (int y = 0; y < d; y++)
        for (int x = 0; x < d; x++) {
            float dx = x + 0.5f - c - 0.08f * d, dy = y + 0.5f - c - 0.1f * d, ds = hypotf(dx, dy);
            float sa = 0.45f * clampf((r * 1.1f - ds) / (0.2f * r), 0, 1);
            if (sa > 0) pzk_over(o + 4 * (y * d + x), (const uint8_t[]){0, 0, 0}, sa);
        }
    for (int y = 0; y < d; y++)
        for (int x = 0; x < d; x++) {
            float X = (x + 0.5f - c) / r, Y = (y + 0.5f - c) / r, r2 = X * X + Y * Y, dd = sqrtf(r2) * r;
            float cov = clampf(r - dd + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float z = sqrtf(fmaxf(0, 1 - r2)), nl = -0.45f * X - 0.6f * Y + 0.66f * z;
            float kk = 0.45f + 0.65f * fmaxf(0, nl), sp = powf(fmaxf(0, -0.3f * X - 0.4f * Y + 0.87f * z), 40);
            float ring = r2 > 0.62f && r2 < 0.76f ? 0.8f : 1;
            uint8_t col[3];
            const float B[3] = {222, 170, 74};
            for (int i = 0; i < 3; i++) col[i] = (uint8_t)clampf(B[i] * kk * ring + 255 * sp * 0.8f, 0, 255);
            pzk_over(o + 4 * (y * d + x), col, cov);
        }
}

/* the jar's back, the glass, the stand (the jar `width` units wide; kk pixels a unit): W x H from (x0, y0) units */
static void jar_extent(int width, float *x0, float *y0, float *x1, float *y1)
{
    *x0 = -GL - 16;
    *x1 = width + GL + 16;
    *y0 = -DMJ_TOP - 6;
    *y1 = DMJ_JH + GL + STAND + 6;
}

static void paint_back(const dmj_jar *J, float kk, int W, int H, float X0, float Y0, uint8_t *o)
{
    memset(o, 0, (size_t)W * H * 4);
    float w = (float)J->width;
#define PX(u) (((u) - X0) * kk)
#define PY(u) (((u) - Y0) * kk)
    /* the stand: a plank in its frame, a shadow under it */
    int sx0 = (int)PX(-GL - 12), sx1 = (int)PX(w + GL + 12), sy0 = (int)PY(DMJ_JH + GL - 2), sy1 = (int)PY(DMJ_JH + GL + STAND);
    for (int y = sy0; y < H; y++)
        for (int x = sx0; x < sx1 + (int)(4 * kk) && x < W; x++) {
            if (x < 0 || y < 0) continue;
            float d = fmaxf(0, (float)(y - sy1)) / (3 * kk), e = fminf(x - sx0, sx1 + 4 * kk - x) / (3 * kk);
            float a = 0.4f * clampf(1 - d, 0, 1) * clampf(e, 0, 1);
            if (y >= sy1 - (int)kk && a > 0) pzk_over(o + 4 * ((size_t)y * W + x), (const uint8_t[]){0, 0, 0}, a);
        }
    int fr = (int)fmaxf(2, 5 * kk), pw = sx1 - sx0 - 2 * fr, ph = sy1 - sy0 - 2 * fr;
    if (pw > 4 && ph > 4) {
        uint8_t *pl = malloc((size_t)(pw + 2 * fr) * (ph + 2 * fr) * 4);
        if (pl) {
            art_plank(pw, ph, fr, pl);
            art_blend(o, W, H, pl, pw + 2 * fr, ph + 2 * fr, sx0, sy0);
            free(pl);
        }
    }
    /* the inside: dark glass, lighter at the top, two soft reflections */
    int ix0 = (int)PX(0), ix1 = (int)ceilf(PX(w)), iy0 = (int)PY(-DMJ_TOP), iy1 = (int)ceilf(PY(DMJ_JH));
    for (int y = iy0; y < iy1 && y < H; y++)
        for (int x = ix0; x < ix1 && x < W; x++) {
            if (x < 0 || y < 0) continue;
            float t = (float)(y - iy0) / (iy1 - iy0), u = (float)(x - ix0) / (ix1 - ix0);
            float c[3] = {46 + (20 - 46) * t, 70 + (36 - 70) * t, 82 + (44 - 82) * t};
            float sheen = 0.05f * expf(-powf((u - 0.2f) * 7, 2)) + 0.03f * expf(-powf((u - 0.78f) * 11, 2));
            float edge = fminf(x - ix0, ix1 - x) / kk, shade = 1 - 0.4f * expf(-edge / 8);
            float vig = 1 - 0.18f * powf(fabsf(u - 0.5f) * 2, 2);
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf((c[i] * vig + 255 * sheen) * shade, 0, 255);
            p[3] = 255;
        }
    /* the glass: the two walls and the floor, thick and pale, their edges catching the light */
    const uint8_t GLS[3] = {196, 228, 236};
    for (int y = (int)PY(-DMJ_TOP - 4); y < (int)ceilf(PY(DMJ_JH + GL)) && y < H; y++)
        for (int x = (int)PX(-GL); x < (int)ceilf(PX(w + GL)) && x < W; x++) {
            if (x < 0 || y < 0) continue;
            float ux = x + 0.5f, uy = y + 0.5f;
            float dl = PX(0) - ux, dr = ux - PX(w), db = uy - PY(DMJ_JH);
            float in = fmaxf(fmaxf(dl, dr), db);          /* > 0: in the glass */
            if (in <= -0.5f) continue;
            float th = GL * kk;
            float top = uy - PY(-DMJ_TOP);                /* the lip, rounded */
            float rr = th / 2;
            float cov = clampf(in + 0.5f, 0, 1);
            if (top < rr) {
                float cxl = PX(-GL / 2), cxr = PX(w + GL / 2), cyy = PY(-DMJ_TOP) + rr;
                float d = fminf(hypotf(ux - cxl, uy - cyy), hypotf(ux - cxr, uy - cyy));
                cov = fminf(cov, clampf(rr - d + 0.5f, 0, 1));
                if (top < 0) cov = 0;
            }
            if (cov <= 0) continue;
            float across = dl > 0 ? dl / th : dr > 0 ? dr / th : db / th;   /* 0 inside face .. 1 outside face */
            float hl = 0.55f * expf(-powf((across - 0.22f) * 7, 2)) + 0.35f * expf(-powf((across - 0.85f) * 9, 2));
            float ed = expf(-powf((across - 0.96f) * 14, 2)) + 0.6f * expf(-powf(across * 14, 2));   /* its faces: dark lines */
            float a = clampf(0.42f + 0.4f * hl + 0.45f * ed, 0, 0.95f);
            uint8_t c[3];
            for (int i = 0; i < 3; i++)
                c[i] = (uint8_t)clampf(GLS[i] * (0.8f + 0.35f * hl) * (1 - 0.55f * clampf(ed, 0, 1)), 0, 255);
            pzk_over(o + 4 * ((size_t)y * W + x), c, a * cov);
        }
    /* the stand's slots for the eleven fruits */
    float slot = (w + 2 * GL + 16) / DMJ_SIZES, cy = DMJ_JH + GL + STAND / 2;
    float sr = fminf(slot * 0.42f, STAND * 0.34f);
    for (int s = 1; s <= DMJ_SIZES; s++) {
        float cx = -GL - 8 + (s - 0.5f) * slot;
        float pcx = PX(cx), pcy = PY(cy), r = sr * kk;
        for (int y = (int)(pcy - r - 2); y <= (int)(pcy + r + 2); y++)
            for (int x = (int)(pcx - r - 2); x <= (int)(pcx + r + 2); x++) {
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                float f = art_recess(x + 0.5f - pcx, y + 0.5f - pcy, r, 0.12f * r, 0.62f, 0.25f, 0.2f);
                uint8_t *p = o + 4 * ((size_t)y * W + x);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * f, 0, 255);
            }
    }
#undef PX
#undef PY
}

/* the glass's front: soft streaks of light over everything inside */
static void paint_front(const dmj_jar *J, float kk, int W, int H, float X0, float Y0, uint8_t *o)
{
    memset(o, 0, (size_t)W * H * 4);
    float w = (float)J->width;
    int ix0 = (int)((0 - X0) * kk), ix1 = (int)((w - X0) * kk), iy0 = (int)((-DMJ_TOP - Y0) * kk), iy1 = (int)((DMJ_JH - Y0) * kk);
    for (int y = iy0; y < iy1 && y < H; y++)
        for (int x = ix0; x < ix1 && x < W; x++) {
            if (x < 0 || y < 0) continue;
            float u = (float)(x - ix0) / (ix1 - ix0), t = (float)(y - iy0) / (iy1 - iy0);
            float a = 0.13f * expf(-powf((u - 0.07f) * 26, 2)) + 0.07f * expf(-powf((u - 0.15f) * 40, 2)) +
                      0.06f * expf(-powf((u - 0.93f) * 30, 2));
            a *= 0.6f + 0.4f * (1 - t);
            a += 0.05f * expf(-powf(((y - iy0) / kk - 10) / 6, 2));
            if (a > 0.004f) pzk_over(o + 4 * ((size_t)y * W + x), (const uint8_t[]){255, 255, 255}, a);
        }
}

static void paint_dot(int d, uint8_t *o)        /* a soft white dot */
{
    for (int y = 0; y < d; y++)
        for (int x = 0; x < d; x++) {
            float r = hypotf(x + 0.5f - d / 2.0f, y + 0.5f - d / 2.0f) / (d / 2.0f);
            uint8_t *p = o + 4 * (y * d + x);
            p[0] = p[1] = p[2] = 255;
            float a = clampf(1 - r, 0, 1);
            p[3] = (uint8_t)(255 * a * a * (3 - 2 * a));
        }
}

static void paint_shadow(int d, uint8_t *o)     /* a fruit's soft shadow */
{
    for (int y = 0; y < d; y++)
        for (int x = 0; x < d; x++) {
            float r = hypotf(x + 0.5f - d / 2.0f, y + 0.5f - d / 2.0f) / (d / 2.0f);
            uint8_t *p = o + 4 * (y * d + x);
            p[0] = p[1] = p[2] = 0;
            p[3] = (uint8_t)(255 * clampf((1 - r) / 0.22f, 0, 1));
        }
}

/* the chain's mark: two links */
static void paint_chain(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    float s = (float)size;
    const uint8_t G2[3] = {236, 194, 96};
    for (int i = 0; i < 2; i++) {
        float cx = (0.36f + 0.28f * i) * s, cy = (0.6f - 0.2f * i) * s;
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                float u = (dx + dy) * 0.7071f, v = (dx - dy) * 0.7071f;
                float e = hypotf(u / (0.26f * s), v / (0.15f * s));
                float d = fabsf(e - 1) * 0.18f * s;
                float cov = clampf(0.05f * s - d + 0.5f, 0, 1);
                if (cov > 0) pzk_over(o + 4 * (y * size + x), G2, cov);
            }
    }
}

static void paint_star(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    float c = size / 2.0f, R = size * 0.46f, r = R * 0.45f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float a = 0;
            for (int sy = 0; sy < 3; sy++)
                for (int sx = 0; sx < 3; sx++) {
                    float dx = x + (sx + 0.5f) / 3 - c, dy = y + (sy + 0.5f) / 3 - c;
                    float ang = atan2f(dx, -dy), d = hypotf(dx, dy);
                    float seg = fmodf(ang + 2 * PUZ_PI + PUZ_PI / 5, 2 * PUZ_PI / 5) - PUZ_PI / 5;
                    float t = fabsf(seg) / (PUZ_PI / 5), lim = R * r / (r + (R - r) * t * 1.0f);
                    if (d < lim) a += 1;
                }
            pzk_over(o + 4 * (y * size + x), PZK_GOLD, a / 9);
        }
}

/* ---- the layout -------------------------------------------------------------------------------------------- */
static float sx_of(float u) { return ox + (u + jx - VX0) * k; }   /* a point of the jar (units) on the screen */
static float sy_of(float u) { return oy + (u - VY0) * k; }

static SDL_Texture *tex_of(int w, int h, void (*paint)(int, uint8_t *), int d)
{
    (void)w, (void)h;
    paint(d, ui_buf(d, d));
    return ui_tex(d, d, ui_buf(d, d));
}

static void layout(box b)
{
    last_box = b;
    k = fminf(b.w / VW, b.h / VH);
    ox = floorf(b.x + (b.w - VW * k) / 2);
    oy = floorf(b.y + (b.h - VH * k) / 2);
    jx = (DMJ_WIDE - G.w.J.width) / 2.0f;
    ui_board_used(ox, oy, VW * k, VH * k);
    int kq = (int)(k * 64);
    if (made_k != kq) {
        made_k = kq;
        for (int s = 1; s <= DMJ_SIZES; s++) {
            int D = (int)ceilf(2 * dmj_radius[s] * k * PAD) + 2;
            ui_free(&t_body[s]);
            ui_free(&t_shade[s]);
            ui_free(&t_spec[s]);
            uint8_t *o = malloc((size_t)D * D * 4);
            if (!o) continue;
            paint_body(s, D, o);
            t_body[s] = ui_tex(D, D, o);
            paint_light(s, D, 0, o);
            t_shade[s] = ui_tex(D, D, o);
            paint_light(s, D, 1, o);
            t_spec[s] = ui_tex(D, D, o);
            free(o);
        }
        ui_free(&t_dot);
        t_dot = tex_of(0, 0, paint_dot, 64);
        ui_free(&t_shadow);
        t_shadow = tex_of(0, 0, paint_shadow, 96);
        int sd = (int)ceilf(STAND * 0.62f * k);
        for (int s = 1; s <= DMJ_SIZES; s++) {
            ui_free(&t_icon[s]);
            paint_icon(s, sd, ui_buf(sd, sd));
            t_icon[s] = ui_tex(sd, sd, ui_buf(sd, sd));
        }
        int cd = (int)ceilf(34 * k);
        ui_free(&t_chain);
        paint_chain(cd, ui_buf(cd, cd));
        t_chain = ui_tex(cd, cd, ui_buf(cd, cd));
        int rw = (int)ceilf(VW * k), rh = (int)ceilf(14 * k);
        uint8_t *o = calloc((size_t)rw * rh, 4);
        if (o) {
            paint_rod(o, rw, rh, 6 * k, rw - 6 * k, rh / 2.0f, 2.6f * k);
            for (int e = 0; e < 2; e++) {   /* the rail's ends: brass knobs */
                float cx = e ? rw - 6 * k : 6 * k;
                for (int y = 0; y < rh; y++)
                    for (int x = (int)(cx - 6 * k); x <= (int)(cx + 6 * k); x++) {
                        if (x < 0 || x >= rw) continue;
                        float dx = (x + 0.5f - cx) / (5 * k), dy = (y + 0.5f - rh / 2.0f) / (5 * k), r2 = dx * dx + dy * dy;
                        float cov = clampf((1 - sqrtf(r2)) * 5 * k + 0.5f, 0, 1);
                        if (cov <= 0) continue;
                        float z = sqrtf(fmaxf(0, 1 - r2)), kk = 0.5f + 0.6f * fmaxf(0, -0.45f * dx - 0.6f * dy + 0.66f * z);
                        uint8_t c[3] = {(uint8_t)clampf(222 * kk + 60 * powf(z, 30), 0, 255),
                                        (uint8_t)clampf(170 * kk + 60 * powf(z, 30), 0, 255), (uint8_t)clampf(74 * kk, 0, 255)};
                        pzk_over(o + 4 * ((size_t)y * rw + x), c, cov);
                    }
            }
            ui_free(&t_rail);
            t_rail = ui_tex(rw, rh, o);
            free(o);
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small && small > 2) {
        made_small = small;
        ui_free(&t_star);
        paint_star(small, ui_buf(small, small));
        t_star = ui_tex(small, small, ui_buf(small, small));
        ui_free(&t_tok);
        paint_icon(4, small, ui_buf(small, small));
        t_tok = ui_tex(small, small, ui_buf(small, small));
        ui_free(&t_left);
        paint_icon(1, small, ui_buf(small, small));
        t_left = ui_tex(small, small, ui_buf(small, small));
        made_serial = -1;
    }
    if (made_serial != serial) {
        made_serial = serial;
        float x0, y0, x1, y1;
        jar_extent(G.w.J.width, &x0, &y0, &x1, &y1);
        back_x0 = x0, back_y0 = y0;
        back_w = (int)ceilf((x1 - x0) * k), back_h = (int)ceilf((y1 - y0) * k);
        uint8_t *o = malloc((size_t)back_w * back_h * 4);
        if (o) {
            paint_back(&G.w.J, k, back_w, back_h, x0, y0, o);
            ui_free(&t_back);
            t_back = ui_tex(back_w, back_h, o);
            paint_front(&G.w.J, k, back_w, back_h, x0, y0, o);
            ui_free(&t_front);
            t_front = ui_tex(back_w, back_h, o);
            free(o);
        }
        int pr = 0;
        for (int i = 0; i < G.w.J.npeg; i++) pr = G.w.J.peg[i].r > pr ? G.w.J.peg[i].r : pr;
        ui_free(&t_peg);
        if (pr) {
            peg_d = (int)ceilf(2.4f * pr * k) + 2;
            paint_peg(peg_d, ui_buf(peg_d, peg_d));
            t_peg = ui_tex(peg_d, peg_d, ui_buf(peg_d, peg_d));
        }
        ui_free(&t_goal);
        if (small > 2) {
            int gs = G.w.J.goal == DMJ_SIZE ? G.w.J.target : G.w.J.goal == DMJ_COUNT ? G.w.J.tsize : 0;
            if (gs) paint_icon(gs, small, ui_buf(small, small));
            else if (G.w.J.goal == DMJ_CHAIN) paint_chain(small, ui_buf(small, small));
            else paint_star(small, ui_buf(small, small));
            t_goal = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

/* ---- the game --------------------------------------------------------------------------------------------------- */
static void read_best(void)
{
    if (best_read) return;
    best_read = 1;
    uint8_t b[4 * DMJ_VARIANTS];
    int n = ui_save_get("dropmerge/best", b, sizeof b);
    for (int v = 0; v < DMJ_VARIANTS && 4 * v + 4 <= n; v++)
        best[v] = b[4 * v] | b[4 * v + 1] << 8 | b[4 * v + 2] << 16 | (b[4 * v + 3] & 0x7f) << 24;
}

static void keep_best(void)
{
    if (G.w.J.goal != DMJ_ENDLESS || G.w.score <= best[variant]) return;
    best[variant] = G.w.score;
    uint8_t b[4 * DMJ_VARIANTS];
    for (int v = 0; v < DMJ_VARIANTS; v++)
        for (int i = 0; i < 4; i++) b[4 * v + i] = (uint8_t)(best[v] >> (8 * i));
    ui_save_set("dropmerge/best", b, sizeof b);
}

static void begin(void)
{
    serial++;
    rebase();
    drop_x = G.w.J.width * DMJ_FP / 2;
    if (last_box.w > 0) layout(last_box);
    ui_animate();
}

static void start(int level)
{
    if (!dmj_level(&G, level)) dmj_level(&G, 0);
    begin();
}

static void start_random(int v)
{
    read_best();
    variant = v < 0 || v >= DMJ_VARIANTS ? 0 : v;
    dmj_endless(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int over_shown(Uint32 now) { return end_t0 && now - end_t0 >= (G.w.end == DMJ_WON ? WIN_MS : END_MS); }

static int status(void)
{
    Uint32 now = ui_play_ms();
    advance(now);
    if (G.w.end == DMJ_PLAYING) return GS_PLAY;
    if (G.w.J.goal == DMJ_ENDLESS) return GS_WIN0;
    return G.w.end == DMJ_WON ? GS_WIN0 : GS_WIN1;
}
static int turn(void) { return 0; }
static int score(void) { return G.w.J.goal == DMJ_ENDLESS ? G.w.score : G.w.drops; }
static int par(void) { return G.w.J.goal == DMJ_ENDLESS ? 0 : G.w.J.par; }
static int level_stars(void)
{
    if (G.w.J.goal == DMJ_ENDLESS) return dmj_stars(&G);
    return pzk_stars(dmj_stars(&G), G.hints, 0);
}

static int busy(void)
{
    Uint32 now = ui_play_ms();
    advance(now);
    if (pend) return 1;
    if (G.w.end == DMJ_PLAYING) return !G.w.rest;
    if (!end_t0) end_t0 = tick_ms(G.w.end_tick) > now ? now : tick_ms(G.w.end_tick);
    return !over_shown(now) || (G.w.end == DMJ_WON && !G.w.rest);
}

static void restart(void)
{
    dmj_restart(&G);
    rebase();
    ui_sound(SND_TAP);
    ui_animate();
}

static void hint(void)
{
    Uint32 now = ui_play_ms();
    advance(now);
    if (!dmj_can_drop(&G.w) && !(G.w.end == DMJ_PLAYING && G.w.cur)) { ui_sound(SND_DENY); return; }
    int x = dmj_aim(&G.w, 0, NULL);
    if (hint_x != x || now - hint_t0 > HINT_MS) G.hints++;
    hint_x = x;
    hint_t0 = now;
    drop_x = x;
    ui_sound(SND_TAP);
    ui_animate();
}

/* the moves at once, a last drop made as a finger would (its fall and its merges animating: the pictures' --at) */
static int script(const char *moves)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", moves);
    char *last = strrchr(buf, ' ');
    last = last ? last + 1 : buf;
    int live = -1;
    if (last[0] == 'd' && last[1] >= '0' && last[1] <= '9') {
        live = atoi(last + 1);
        *last = 0;
    } else if (last[0] == 'b' && !last[1]) {   /* "b": where the hint aims */
        live = 1 << 20;
        *last = 0;
    }
    int ok = dmj_script(&G, buf);
    if (live >= 0 && G.w.end == DMJ_PLAYING) {
        for (int t = 0; t < 20 * DMJ_HZ && G.w.end == DMJ_PLAYING && !(dmj_can_drop(&G.w) && G.w.rest); t++)
            dmj_step(&G.w);
        while (G.w.end == DMJ_PLAYING && G.w.cur && !dmj_can_drop(&G.w)) dmj_step(&G.w);
        rebase();
        if (dmj_can_drop(&G.w)) {
            drop_x = live == 1 << 20 ? dmj_aim(&G.w, 0, NULL) : live * DMJ_FP;
            dropped(drop_x);
        } else
            ok = 0;
        keep_best();
        return ok;
    }
    rebase();
    keep_best();
    return ok;
}

/* ---- the effects --------------------------------------------------------------------------------------------- */
static void squash(uint16_t id, Uint32 t, float amp, int land)
{
    int i = nsq < NSQ ? nsq++ : (int)(t % NSQ);
    sq[i].id = id, sq[i].t = t, sq[i].amp = amp, sq[i].land = land;
}

static void take_events(Uint32 now)
{
    int from = seen_ev > G.w.nev - DMJ_EVENTS ? seen_ev : G.w.nev - DMJ_EVENTS;
    for (int i = from; i < G.w.nev; i++) {
        const dmj_event *e = &G.w.ev[i % DMJ_EVENTS];
        Uint32 t = tick_ms(e->tick);
        int fresh = (int32_t)(now - t) < 200;
        if (e->type == DMJ_E_MERGE || e->type == DMJ_E_GONE) {
            int j = nfx < NFX ? nfx++ : i % NFX;
            fx[j].type = e->type, fx[j].size = e->size, fx[j].pts = e->pts, fx[j].chain = e->chain;
            fx[j].x = e->x / 16.0f, fx[j].y = e->y / 16.0f, fx[j].t = t;
            fx[j].id = (uint16_t)i;
            if (e->type == DMJ_E_MERGE) squash(e->id, t, 1, 0);
            if (e->chain >= 2) chainfx.n = e->chain, chainfx.t = t;
            if (fresh) {
                if (e->size >= 8 || e->type == DMJ_E_GONE) ui_sound(SND_END);
                else ui_sound_soft(SND_CAPTURE, 0.5f + 0.06f * e->size);
            }
        } else if (e->type == DMJ_E_LAND) {
            float f = clampf((float)e->force / (8 * DMJ_FP), 0, 1);
            squash(e->id, t, 0.25f + 0.75f * f, 1);
            if (fresh && f > 0.15f && now - last_land_snd > 90) {
                ui_sound_soft(SND_PLACE, 0.25f + 0.5f * f);
                last_land_snd = now;
            }
        }
    }
    seen_ev = G.w.nev;
    if (G.w.biggest > biggest_seen) {
        biggest_seen = G.w.biggest;
        big_t = now;
    }
}

/* a fruit's squash now: its width and height factors */
static void squash_of(uint16_t id, Uint32 now, float *fw, float *fh)
{
    *fw = *fh = 1;
    for (int i = 0; i < nsq; i++) {
        if (sq[i].id != id) continue;
        float t = (float)(int32_t)(now - sq[i].t);
        if (t < 0 || t > 520) continue;
        float a;
        if (sq[i].land) a = 0.13f * sq[i].amp * expf(-t / 90) * cosf(t / 520 * 2 * PUZ_PI * 2.2f);
        else a = 0.2f * expf(-t / 130) * sinf(t / 300 * 2 * PUZ_PI + 0.6f);
        *fw *= 1 + a;
        *fh *= 1 - a;
        ui_animate();
    }
}

/* ---- drawing -------------------------------------------------------------------------------------------------- */
static void put_quad(SDL_Texture *t, float cx, float cy, float w, float h, float ang, Uint8 shade, Uint8 alpha)
{
    if (!t) return;
    SDL_Vertex v[4];
    const float U[4] = {0, 1, 1, 0}, V[4] = {0, 0, 1, 1}, X[4] = {-1, 1, 1, -1}, Y[4] = {-1, -1, 1, 1};
    float c = cosf(ang), s = sinf(ang);
    for (int i = 0; i < 4; i++) {
        float px = X[i] * c - Y[i] * s, py = X[i] * s + Y[i] * c;   /* the corner turned, then squashed on the screen */
        v[i] = (SDL_Vertex){{cx + px * w / 2, cy + py * h / 2}, {shade, shade, shade, alpha}, {U[i], V[i]}};
    }
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(ui.r, t, v, 4, idx, 6);
}

/* a fruit on the screen: (cx, cy) its centre, r its radius (pixels), ang its turn, fw fh its squash */
static void put_fruit(int s, float cx, float cy, float r, float ang, float fw, float fh, Uint8 shade, Uint8 alpha)
{
    float d = 2 * r * PAD;
    put_quad(t_body[s], cx, cy, d * fw, d * fh, ang, shade, alpha);
    if (t_shade[s]) {
        SDL_SetTextureAlphaMod(t_shade[s], alpha);
        ui_put_c(t_shade[s], cx, cy, d * fw, d * fh);
        SDL_SetTextureAlphaMod(t_shade[s], 255);
    }
    if (t_spec[s]) {
        SDL_SetTextureAlphaMod(t_spec[s], alpha);
        ui_put_c(t_spec[s], cx, cy, d * fw, d * fh);
        SDL_SetTextureAlphaMod(t_spec[s], 255);
    }
}

/* the height of the game's own digits on the screen (units): whole pixels, the same every frame. The shell keeps the
 * digits of the last 6 heights only and makes 10 new textures (each digit drawn by the font) for any other: numbers
 * whose height changed every frame (a popping chain count, a merge's points sized by its fruit, several at once) made
 * dozens of textures a frame while fruits merged (up to 40; the likely cause of a crash on a phone while they merged) */
static float num_h(float units) { return fmaxf(14, roundf(units * k)); }

static void soft_dot(float cx, float cy, float d, const uint8_t *rgb, float a, int add)
{
    if (!t_dot || a <= 0) return;
    SDL_SetTextureBlendMode(t_dot, add ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(t_dot, rgb[0], rgb[1], rgb[2]);
    SDL_SetTextureAlphaMod(t_dot, (Uint8)(255 * clampf(a, 0, 1)));
    ui_put_c(t_dot, cx, cy, d, d);
}

static float slot_x(int s) { return -GL - 8 + (s - 0.5f) * ((G.w.J.width + 2 * GL + 16) / DMJ_SIZES); }
static float slot_r(void) { return fminf((G.w.J.width + 2 * GL + 16) / DMJ_SIZES * 0.42f, STAND * 0.34f); }

static void draw_stand(Uint32 now)
{
    float cy = sy_of(DMJ_JH + GL + STAND / 2), r = slot_r() * k, d = STAND * 0.62f * k;
    int target = G.w.J.goal == DMJ_SIZE ? G.w.J.target : G.w.J.goal == DMJ_COUNT ? G.w.J.tsize : 0;
    int top = G.w.biggest;
    for (int i = 0; i < G.w.nb; i++) top = G.w.b[i].size > top ? G.w.b[i].size : top;
    for (int s = 1; s <= DMJ_SIZES; s++) {
        float cx = sx_of(slot_x(s)), dd = d * fminf(1, 2 * r / d * 1.05f);
        int lit = s <= top || s <= DMJ_DROPS;
        if (s == target) {
            float g = 0.55f + 0.3f * sinf(now / 260.0f);
            ui_mark_gold_a(cx, cy, 2.6f * r, G.w.end == DMJ_WON ? 1 : g);
            ui_animate();
        }
        if (s == G.w.biggest && big_t && now - big_t < 1400) {
            float f = (now - big_t) / 1400.0f;
            ui_mark_gold_a(cx, cy, (2.2f + 1.6f * f) * r, 1 - f);
            ui_animate();
        }
        if (!t_icon[s]) continue;
        if (lit) {
            SDL_SetTextureColorMod(t_icon[s], 255, 255, 255);
            SDL_SetTextureAlphaMod(t_icon[s], 255);
        } else {
            SDL_SetTextureColorMod(t_icon[s], 40, 30, 24);
            SDL_SetTextureAlphaMod(t_icon[s], 150);
        }
        float pop = s == G.w.biggest && big_t && now - big_t < 500 ? 1 + 0.25f * sinf((now - big_t) / 500.0f * PUZ_PI) : 1;
        ui_put_c(t_icon[s], cx, cy, dd * pop, dd * pop);
        SDL_SetTextureColorMod(t_icon[s], 255, 255, 255);
        SDL_SetTextureAlphaMod(t_icon[s], 255);
    }
}

static void draw_line(Uint32 now)               /* the rim line, dashed: amber near, red while a fruit is above */
{
    float y = sy_of(0), x0 = sx_of(0), x1 = sx_of((float)G.w.J.width);
    float near = 0;
    for (int i = 0; i < G.w.nb; i++) {
        const dmj_ball *b = &G.w.b[i];
        if (b->fresh) continue;
        float top = (float)(b->y - b->r) / DMJ_FP;
        if (top < 40) near = fmaxf(near, clampf(1 - top / 40, 0, 1));
    }
    float danger = clampf((float)G.w.overmax / DMJ_OVER, 0, 1);
    uint8_t c[3] = {236, 226, 206};
    float a = 0.45f;
    if (danger > 0 || G.w.end == DMJ_OVERFLOW) {
        float p = 0.6f + 0.4f * sinf(now / 90.0f);
        c[0] = 255, c[1] = 70, c[2] = 54;
        a = 0.75f + 0.25f * p;
        float gl = G.w.end == DMJ_OVERFLOW ? 1 : 0.4f + 0.6f * danger;
        pzk_line(x0, y, x1, y, 5 * k, 7 * k, (pzk_rgba){255, 50, 30, (Uint8)(150 * gl * p)}, 1);
        ui_animate();
    } else if (near > 0) {
        c[0] = 246, c[1] = 190, c[2] = 80;
        a = 0.45f + 0.4f * near;
    }
    float dash = 10 * k, gap = 7 * k, th = fmaxf(1.5f, 1.6f * k);
    for (float x = x0 + 3 * k; x < x1 - 2 * k; x += dash + gap) {
        float e = fminf(x + dash, x1 - 2 * k);
        ui_fill(x, y - th / 2, e - x, th, c[0], c[1], c[2], (Uint8)(255 * a));
    }
}

static void draw(void)
{
    if (made_k < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_play_ms();
    advance(now);
    take_events(now);
    if (G.w.end != DMJ_PLAYING && !end_t0) end_t0 = now;
    if (G.w.end != DMJ_PLAYING && !shown_end) {
        shown_end = 1;
        keep_best();
        if (G.w.end == DMJ_OVERFLOW || G.w.end == DMJ_OUT) ui_sound(G.w.J.goal == DMJ_ENDLESS ? SND_END : SND_DENY);
    }
    double tp = tick_pos(now);
    float frac = G.w.rest ? 1 : clampf((float)(tp - G.w.tick + 1), 0, 1);
    ui_put(t_back, sx_of(back_x0), sy_of(back_y0), (float)back_w, (float)back_h);
    draw_stand(now);
    /* the pegs */
    for (int i = 0; i < G.w.J.npeg && t_peg; i++) {
        const dmj_peg *p = &G.w.J.peg[i];
        float dd = 2.4f * p->r * k;
        ui_put_c(t_peg, sx_of(p->x), sy_of(p->y), dd, dd);
    }
    /* the fruits: their shadows, then them */
    float over_a = G.w.end == DMJ_OVERFLOW && end_t0 ? clampf((now - end_t0) / (float)END_MS, 0, 1) : 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < G.w.nb; i++) {
            const dmj_ball *b = &G.w.b[i];
            float x = sx_of((b->px + (b->x - b->px) * frac) / (float)DMJ_FP), y = sy_of((b->py + (b->y - b->py) * frac) / (float)DMJ_FP);
            float r = (float)b->r / DMJ_FP * k, fw, fh;
            squash_of(b->id, now, &fw, &fh);
            if (pass == 0) {
                SDL_SetTextureAlphaMod(t_shadow, 90);
                ui_put_c(t_shadow, x + 0.12f * r, y + 0.2f * r, 2.25f * r * fw, 2.2f * r * fh);
                SDL_SetTextureAlphaMod(t_shadow, 255);
                continue;
            }
            Uint8 shade = 255;
            if (over_a > 0) {
                int above = b->y - b->r < 0;
                shade = (Uint8)(255 * (1 - (above ? 0.15f : 0.45f) * over_a));
            }
            put_fruit(b->size, x, y, r, (float)b->ang * 2 * PUZ_PI / 65536, fw, fh, shade, 255);
            if (b->over > 0 && G.w.end == DMJ_PLAYING) {
                float g = clampf((float)b->over / DMJ_OVER, 0, 1) * (0.6f + 0.4f * sinf(now / 80.0f));
                ui_mark_glow(x, y, 2.4f * r * (0.8f + 0.2f * g));
                ui_animate();
            } else if (G.w.end == DMJ_OVERFLOW && b->y - b->r < 0) {
                ui_mark_glow(x, y, 2.4f * r);
            }
        }
    /* the merges: a flash, juice, the points rising */
    for (int i = 0; i < nfx; i++) {
        float t = (float)(int32_t)(now - fx[i].t);
        if (t < 0 || t > 900) continue;
        ui_animate();
        float x = sx_of(fx[i].x), y = sy_of(fx[i].y), r = dmj_radius[fx[i].size] * k;
        const uint8_t *jc = FR[fx[i].size].juice;
        if (t < 260) {
            float f = t / 260;
            soft_dot(x, y, 2.4f * r * (0.7f + 0.7f * f), (const uint8_t[]){255, 255, 240}, 0.85f * (1 - f) * (1 - f), 1);
            if (fx[i].type == DMJ_E_GONE) ui_mark_gold_a(x, y, 6 * r * (0.5f + f), 1 - f);
        }
        if (t < 620) {
            float f = t / 620;
            int n = 9 + fx[i].size;
            for (int j = 0; j < n; j++) {
                float ang = (j + art_hash(fx[i].id + i, j) * 0.8f) * 2 * PUZ_PI / n, sp = (0.7f + 0.6f * art_hash(j, i + 7)) * r * 2.2f;
                float tt = t / 1000;
                float px = x + cosf(ang) * (r * 0.7f + sp * tt * 1.6f), py = y + sinf(ang) * (r * 0.7f + sp * tt * 1.6f) +
                                                                             900 * k * tt * tt;
                float ds = (0.12f + 0.1f * art_hash(i, j + 3)) * r * (1 - 0.5f * f) + 2;
                soft_dot(px, py, ds * 2, jc, 0.95f * (1 - f), 0);
            }
        }
        if (fx[i].pts > 0) {
            float f = t / 900, h = num_h(26);   /* one height for all (the shell keeps its digits by height) */
            float a = f < 0.7f ? 1 : (1 - f) / 0.3f;
            float yy = y - r - 10 * k - 40 * k * puz_ease(f);
            ui_number_ex(x + 0.04f * h, yy + 0.05f * h, h, fx[i].pts, (const uint8_t[]){40, 24, 12}, (Uint8)(120 * a), 0);
            ui_number_ex(x, yy, h, fx[i].pts, (const uint8_t[]){255, 244, 214}, (Uint8)(255 * a), 0);
        }
    }
    draw_line(now);
    ui_put(t_front, sx_of(back_x0), sy_of(back_y0), (float)back_w, (float)back_h);
    /* the rail, the dropper and its fruit, where it would fall */
    ui_put(t_rail, ox, sy_of(RAILY) - 7 * k, VW * k, 14 * k);
    int cur = G.w.cur;
    float dx = sx_of((float)drop_x / DMJ_FP);
    if (G.w.end == DMJ_PLAYING && cur) {
        int32_t xs = dmj_clamp(&G.w, cur, drop_x);
        dx = sx_of((float)xs / DMJ_FP);
        float grow = G.w.tick >= G.w.ready ? 1 : clampf(1 - (float)(G.w.ready - tp) / 18, 0, 1);
        float r = dmj_radius[cur] * k, fy = sy_of(DMJ_SPAWN);
        int what;
        int32_t ly = dmj_land(&G.w, cur, xs, &what);
        float gy = sy_of((float)ly / DMJ_FP);
        if (grow >= 1 && gy > fy + r) {         /* the guide: a faint dotted line down to where it lands */
            for (float y = fy + r + 6 * k; y < gy - 4 * k; y += 9 * k)
                ui_fill(dx - 0.8f * k, y, 1.6f * k, 4 * k, 255, 255, 255, 60);
            soft_dot(dx, gy + r * 0.92f, 1.6f * r, (const uint8_t[]){255, 255, 255}, 0.07f, 1);
        }
        if (hint_x >= 0 && now - hint_t0 < HINT_MS) {
            float g = 0.55f + 0.45f * sinf((now - hint_t0) / 140.0f), hx = sx_of((float)hint_x / DMJ_FP);
            int32_t hy = dmj_land(&G.w, cur, hint_x, &what);
            pzk_line(hx, fy, hx, sy_of((float)hy / DMJ_FP), 5 * k, 3 * k, (pzk_rgba){255, 210, 100, (Uint8)(170 * g)}, 1);
            ui_mark_gold_a(hx, sy_of((float)hy / DMJ_FP), 2.6f * r, g);
            ui_animate();
        }
        /* the claw: a brass slider on the rail, a rod down to the fruit */
        pzk_line(dx, sy_of(RAILY), dx, fy - r * grow + 2 * k, 2.2f * k, 0.8f * k, (pzk_rgba){196, 146, 60, 255}, 0);
        ui_fill(dx - 7 * k, sy_of(RAILY) - 4.5f * k, 14 * k, 9 * k, 150, 106, 44, 255);
        ui_fill(dx - 6 * k, sy_of(RAILY) - 3.5f * k, 12 * k, 3 * k, 236, 196, 110, 255);
        if (grow > 0) put_fruit(cur, dx, fy, r * puz_ease(grow), 0, 1, 1, 255, 255);
        if (grow < 1) ui_animate();
    } else {
        ui_fill(dx - 7 * k, sy_of(RAILY) - 4.5f * k, 14 * k, 9 * k, 150, 106, 44, 255);
        ui_fill(dx - 6 * k, sy_of(RAILY) - 3.5f * k, 12 * k, 3 * k, 236, 196, 110, 255);
    }
    /* the next fruit: on an ivory medallion at the top right; the chain at the top left */
    if (G.w.next) {
        float cx = sx_of((float)G.w.J.width - 4) - 22 * k, cy = sy_of(-136), d = 40 * k;
        if (-jx + G.w.J.width + GL + 18 < DMJ_WIDE) cx = sx_of((float)G.w.J.width + GL) - 20 * k;
        ui_button_paint(cx, cy, d, NULL, 0, 1, UI_IVORY);
        float rr = fminf(dmj_radius[G.w.next], 22) * k * 0.72f;
        put_fruit(G.w.next, cx, cy + 0.05f * d, rr, 0, 1, 1, 255, 255);
    }
    if (chainfx.n >= 2 && now - chainfx.t < 1600) {
        float f = (now - chainfx.t) / 1600.0f, a = f < 0.75f ? 1 : (1 - f) / 0.25f;
        float pop = 1 + 0.3f * expf(-(now - chainfx.t) / 120.0f);
        float cx = sx_of(0) + 26 * k, cy = sy_of(-136), d = 34 * k * pop;
        SDL_SetTextureAlphaMod(t_chain, (Uint8)(255 * a));
        ui_put_c(t_chain, cx, cy, d, d);
        SDL_SetTextureAlphaMod(t_chain, 255);
        float h = num_h(28);                    /* steady (the mark pops, not the digits: see num_h) */
        ui_number_ex(cx + 0.9f * d + 0.04f * h, cy + 0.05f * h, h, chainfx.n, (const uint8_t[]){40, 24, 12}, (Uint8)(120 * a), 0);
        ui_number_ex(cx + 0.9f * d, cy, h, chainfx.n, PZK_GOLD, (Uint8)(255 * a), 0);
        ui_animate();
    }
    if (G.w.end == DMJ_WON && end_t0) {         /* solved: a gold light sweeps up the jar */
        float t = (float)(int32_t)(now - end_t0);
        if (t < WIN_MS + 400) {
            float f = clampf(t / WIN_MS, 0, 1);
            for (int i = 0; i < G.w.nb; i++) {
                const dmj_ball *b = &G.w.b[i];
                float yy = (float)b->y / DMJ_FP, a = puz_pulse(f * (DMJ_JH + 120) - (DMJ_JH - yy), 0, 120);
                if (a > 0) ui_mark_gold_a(sx_of((float)b->x / DMJ_FP), sy_of(yy), 2.6f * b->r / DMJ_FP * k, a);
            }
            ui_animate();
        }
    }
    if (!G.w.rest || busy() || pend) ui_animate();
    ui_tool(T_RESTART, ICON_REPLAY, (G.w.end == DMJ_OVERFLOW || G.w.end == DMJ_OUT ? UI_LIT : G.w.drops ? 0 : UI_DIM) |
                                    (tool_down == T_RESTART ? UI_ON : 0));
    ui_tool(T_HINT, ICON_HINT, (G.w.end != DMJ_PLAYING ? UI_DIM : 0) | (tool_down == T_HINT ? UI_ON : 0));
}

/* ---- the pointer ---------------------------------------------------------------------------------------------- */
static int x_at(float x) { return (int)lroundf(((x - ox) / k + VX0 - jx) * DMJ_FP); }

static int on_board(float x, float y)
{
    return x >= ox && x <= ox + VW * k && y >= sy_of(VY0) && y <= sy_of(DMJ_JH + GL);
}

static void press(float x, float y)
{
    tool_down = ui_tool_hit(x, y);
    if (tool_down >= 0) { ui_animate(); return; }
    if (G.w.end != DMJ_PLAYING || !on_board(x, y)) return;
    dragging = 1;
    drop_x = x_at(x);
    if (G.w.cur) drop_x = dmj_clamp(&G.w, G.w.cur, drop_x);
    ui_animate();
}

static void motion(float x, float y)
{
    (void)y;
    if (!dragging) return;
    drop_x = x_at(x);
    if (G.w.cur) drop_x = dmj_clamp(&G.w, G.w.cur, drop_x);
    ui_animate();
}

static void let_go(void)
{
    Uint32 now = ui_play_ms();
    advance(now);
    if (G.w.end != DMJ_PLAYING || !G.w.cur) { ui_sound(SND_DENY); return; }
    if (dmj_can_drop(&G.w)) dropped(drop_x);
    else {
        pend = 1;
        pend_x = drop_x;
    }
    hint_x = -1;
    ui_animate();
}

static void release(float x, float y)
{
    if (tool_down >= 0) {
        int t = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) != t) return;
        if (t == T_RESTART) restart();
        else hint();
        return;
    }
    if (!dragging) return;
    dragging = 0;
    let_go();
}

static void key(SDL_Keycode kc)
{
    if (kc == SDLK_r) { restart(); return; }
    if (G.w.end != DMJ_PLAYING || !G.w.cur) return;
    int step = 12 * DMJ_FP;
    if (kc == SDLK_LEFT) drop_x = dmj_clamp(&G.w, G.w.cur, drop_x - step);
    else if (kc == SDLK_RIGHT) drop_x = dmj_clamp(&G.w, G.w.cur, drop_x + step);
    else if (kc == SDLK_DOWN || kc == SDLK_SPACE || kc == SDLK_RETURN) let_go();
    ui_animate();
}

static void bar(int player, box b)
{
    (void)player;
    if (G.w.J.goal == DMJ_ENDLESS) {
        int top = best[variant] > G.w.score ? best[variant] : G.w.score;
        ui_bar_fit(b, (ui_count[]){{t_tok, G.w.score}, {t_star, top}}, 2, 0);
        return;
    }
    int left = dmj_goal_left(&G.w);
    if (G.w.J.goal == DMJ_SIZE) left = left > 0;
    ui_bar_fit(b, (ui_count[]){{t_goal, left}, {t_left, G.w.J.nseq - G.w.drops}}, 2, 0);
}

/* ---- the shell's pictures ----------------------------------------------------------------------------------------- */
/* a jar drawn small (pixels only: the menu's thread too): the glass, the stand, its pegs and fruits */
static void paint_scene(const dmj_jar *J, int goal_icon, int size, uint8_t *rgba)
{
    dmj_world *w = malloc(sizeof *w);
    if (!w) return;
    dmj_build(w, J);
    float kk = size * 0.94f / (DMJ_JH + DMJ_TOP + GL + STAND + 12);
    float x0, y0, x1, y1;
    jar_extent(J->width, &x0, &y0, &x1, &y1);
    int W = (int)ceilf((x1 - x0) * kk), H = (int)ceilf((y1 - y0) * kk);
    uint8_t *o = W > 8 && H > 8 && W <= size && H <= size ? malloc((size_t)W * H * 4) : NULL;
    if (o) {
        paint_back(J, kk, W, H, x0, y0, o);
        for (int i = 0; i < J->npeg; i++) {
            int d = (int)ceilf(2.4f * J->peg[i].r * kk) + 2;
            uint8_t *p = malloc((size_t)d * d * 4);
            if (!p) continue;
            paint_peg(d, p);
            art_blend(o, W, H, p, d, d, (int)((J->peg[i].x - x0) * kk - d / 2.0f), (int)((J->peg[i].y - y0) * kk - d / 2.0f));
            free(p);
        }
        for (int i = 0; i < w->nb; i++) {
            const dmj_ball *b = &w->b[i];
            float r = (float)b->r / DMJ_FP * kk;
            paint_fruit_at(o, W, H, b->size, ((float)b->x / DMJ_FP - x0) * kk, ((float)b->y / DMJ_FP - y0) * kk,
                           (int)(2 * r * PAD), (float)b->ang * 360 / 65536);
        }
        uint8_t *f = malloc((size_t)W * H * 4);
        if (f) {
            paint_front(J, kk, W, H, x0, y0, f);
            art_blend(o, W, H, f, W, H, 0, 0);
            free(f);
        }
        if (goal_icon) {                        /* a level's goal, big in the jar's empty top */
            float gx = (J->width / 2.0f - x0) * kk, gy = (DMJ_JH * 0.3f - y0) * kk, span = J->width * kk;
            int fs = J->goal == DMJ_SIZE ? J->target : J->goal == DMJ_COUNT ? J->tsize : 0;
            const uint8_t CREAM[3] = {250, 238, 210};
            if (fs) {
                float d = span * (J->goal == DMJ_COUNT ? 0.5f : 0.62f) * PAD;
                paint_fruit_at(o, W, H, fs, gx, gy, (int)d, 0);
                if (J->goal == DMJ_COUNT) puz_digits(o, W, H, J->target, gx, gy + 0.5f * d, 0.3f * span, CREAM, 1, 1);
            } else if (J->goal == DMJ_CHAIN) {
                int cd = (int)(0.5f * span);
                uint8_t *c = cd > 4 ? malloc((size_t)cd * cd * 4) : NULL;
                if (c) {
                    paint_chain(cd, c);
                    art_blend(o, W, H, c, cd, cd, (int)(gx - cd / 2.0f), (int)(gy - 0.62f * cd));
                    free(c);
                }
                puz_digits(o, W, H, J->target, gx, gy + 0.3f * span, 0.3f * span, CREAM, 1, 1);
            } else
                puz_digits(o, W, H, J->target, gx, gy, 0.26f * span, PZK_GOLD, 1, 1);
        }
        art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    }
    free(o);
    free(w);
}

static const dmj_pre THUMB_PRE[] = {
    {9, 100, 380}, {7, 260, 400}, {5, 352, 420}, {6, 200, 300}, {4, 60, 280}, {3, 300, 300}, {2, 360, 300},
    {1, 130, 230}, {8, 300, 200}, {3, 40, 200}, {10, 160, 120}, {5, 330, 100}, {2, 60, 90}, {4, 250, 60},
    {1, 370, 40}, {6, 60, 30}, {2, 200, 20},
};

static void thumb(int v, int size, uint8_t *rgba)
{
    dmj_jar j;
    memset(&j, 0, sizeof j);
    j.goal = DMJ_ENDLESS;
    j.width = v < 0 ? DMJ_WIDE : dmj_widths[v % DMJ_VARIANTS];
    int n = 0;
    for (int i = 0; i < (int)(sizeof THUMB_PRE / sizeof THUMB_PRE[0]); i++) {
        dmj_pre p = THUMB_PRE[i];
        if (p.x * j.width / DMJ_WIDE < dmj_radius[p.size]) continue;
        p.x = (int16_t)(p.x * j.width / DMJ_WIDE);
        if (v >= 1 && p.size >= 8) p.size = (uint8_t)(p.size - 2);
        j.pre[n++] = p;
    }
    j.npre = n;
    paint_scene(&j, 0, size, rgba);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    dmj_jar j;
    if (level < 0 || level >= DMJ_LEVELS || !dmj_load(&j, &dmj_levels[level])) return;
    paint_scene(&j, 1, size, rgba);
}

/* a chapter's tab: a cherry and a strawberry; a fruit on brass pegs; a narrow jar; a watermelon */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    float s = (float)size;
    switch (chapter) {
    case 0:
        paint_fruit_at(rgba, size, size, 1, 0.3f * s, 0.6f * s, (int)(0.5f * s), 0);
        paint_fruit_at(rgba, size, size, 2, 0.64f * s, 0.52f * s, (int)(0.62f * s), 0);
        break;
    case 1: {
        int d = (int)(0.3f * s);
        uint8_t *p = malloc((size_t)d * d * 4);
        if (p) {
            paint_peg(d, p);
            art_blend(rgba, size, size, p, d, d, (int)(0.16f * s), (int)(0.62f * s));
            art_blend(rgba, size, size, p, d, d, (int)(0.56f * s), (int)(0.62f * s));
            free(p);
        }
        paint_fruit_at(rgba, size, size, 4, 0.5f * s, 0.38f * s, (int)(0.6f * s), 0);
        break;
    }
    case 2:
        for (int y = (int)(0.1f * s); y < (int)(0.9f * s); y++)
            for (int x = (int)(0.3f * s); x < (int)(0.7f * s); x++) {
                int edge = x < 0.36f * s || x > 0.64f * s || y > 0.84f * s;
                pzk_over(rgba + 4 * ((size_t)y * size + x), edge ? (const uint8_t[]){196, 228, 236} : (const uint8_t[]){30, 52, 62},
                         edge ? 0.7f : 0.9f);
            }
        paint_fruit_at(rgba, size, size, 6, 0.5f * s, 0.62f * s, (int)(0.36f * s), 0);
        paint_fruit_at(rgba, size, size, 3, 0.47f * s, 0.36f * s, (int)(0.24f * s), 0);
        break;
    default:
        paint_fruit_at(rgba, size, size, 11, 0.5f * s, 0.52f * s, (int)(0.86f * s), 0);
        break;
    }
}

static const game dropmerge_game = {
    .id = "dropmerge", .order = 462, .variants = DMJ_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = NULL, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .score = score, .par = par, .level_stars = level_stars,
    .levels = DMJ_LEVELS, .chapters = DMJ_CHAPTERS, .chapter_of = dmj_chapter_of, .start_random = start_random,
    .chapter_icon = chapter_icon, .level_thumb = level_thumb, .fill = 1, .tools = T_COUNT,
};
REGISTER_GAME(dropmerge_game)
