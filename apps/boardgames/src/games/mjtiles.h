/*
 * The Mahjong tiles, drawn by code (shared by Mahjong Solitaire, mahjong.c, and the table game for four, mahjong4.c):
 * ivory faces on a bamboo-green back. The faces: the dots, the bamboo (the one a bird), the characters (the numeral
 * over 萬), the winds 東南西北, the dragons 中, 發 and the white one (a blue frame), the flowers and the seasons (a
 * picture, and their number in a corner). The characters are characters, not words: help/glyphs.txt keeps them in
 * the fonts. The faces are numbered as games/mahjong.h's MJ_DOTS .. MJ_FACES.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="mahjong4" builds
 * mahjong4.c and mahjong4_*.c), so a shared .c file would be left out of one game's build or the other's.
 * - mjt_face_pic(face, w, h, rgba): a face's picture alone (clear around), w x h (h = MJT_ASPECT * w).
 * - mjt_tile_pic(fw, fh, d, rgba): the tile under a face: the face's ivory, a thin ivory layer, the green back, seen
 *   from above and a little to the upper left ((fw + d) x (fh + d): the face at the top left, the back d lower right).
 * - mjt_back_pic(fw, fh, d, rgba): the same tile lying face down (the green back up, the ivory under it).
 * - mjt_shadow_pic(w, h, b, rgba): the soft shadow a tile casts.
 */
#ifndef MJTILES_H
#define MJTILES_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "games/mahjong.h"
#include "shell.h"
#include "text.h"

#define MJT_ASPECT 1.3f                         /* a tile's face: its height to its width */
#define MJT_THICK 0.13f                         /* a tile's thickness, a part of its width */
#define MJT_PI 3.14159265f

/* ---- the colours ------------------------------------------------------------------------------------------------- */
static const float MJT_RED[3] = {178, 40, 34}, MJT_GREEN[3] = {26, 116, 70}, MJT_BLUE[3] = {32, 72, 150};
static const float MJT_IVORY[3] = {248, 243, 228};
static const uint8_t MJT_U_RED[3] = {178, 40, 34}, MJT_U_GREEN[3] = {26, 116, 70}, MJT_U_BLUE[3] = {32, 72, 150},
                     MJT_U_INK[3] = {34, 30, 30};

/* ---- drawing helpers: figures in the face's unit (its width; y down to MJT_ASPECT) ------------------------------- */
/* renders f in `fill` (its marks in `mark`) and paints it over o (w x h); frees f */
static inline void mjt_paint(uint8_t *o, int w, int h, art_fig *f, const float fill[3], const float mark[3],
                             float light)
{
    art_style st;
    for (int i = 0; i < 3; i++) {
        st.fill[i] = fill[i];
        st.outer[i] = fill[i] * 0.45f;
        st.inner[i] = fill[i] * 0.6f;
        st.mark[i] = mark ? mark[i] : MJT_IVORY[i];
    }
    st.light = light;
    st.line = fmaxf(0.012f, 0.9f / w);
    uint8_t *t = malloc((size_t)w * h * 4);
    if (t) {
        art_render(f, &st, w, h, (float)w, 0, 0, t);
        art_blend(o, w, h, t, w, h, 0, 0);
        free(t);
    }
    art_fig_free(f);
}

/* a character centred on (cx, cy) (the face's unit), its em `em` */
static inline void mjt_glyph(uint8_t *o, int w, int h, int cp, float cx, float cy, float em, const uint8_t rgb[3])
{
    int g = (int)(em * w * 1.35f) + 2;
    uint8_t *t = calloc((size_t)g * g, 4);
    if (!t) return;
    text_glyph(cp, em * w, 1, rgb, t, g, g);
    art_blend(o, w, h, t, g, g, (int)lroundf(cx * w - g / 2.0f), (int)lroundf(cy * w - g / 2.0f));
    free(t);
}

/* a band from (x0, y0) to (x1, y1), wide `wd` */
static inline void mjt_band(art_fig *f, int role, float x0, float y0, float x1, float y1, float wd)
{
    float dx = x1 - x0, dy = y1 - y0, l = hypotf(dx, dy) + 1e-6f, nx = -dy / l * wd / 2, ny = dx / l * wd / 2;
    const float q[] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    art_poly(f, role, q, 4);
}

/* a pointed petal or leaf from (cx, cy) out at angle a (degrees, y up), long l, wide wd */
static inline void mjt_petal(art_fig *f, int role, float cx, float cy, float a, float l, float wd)
{
    float c = cosf(a * MJT_PI / 180), s = -sinf(a * MJT_PI / 180), nx = -s, ny = c;
    const float p[] = {cx, cy, cx + c * l * 0.5f + nx * wd / 2, cy + s * l * 0.5f + ny * wd / 2, cx + c * l, cy + s * l,
                       cx + c * l * 0.5f - nx * wd / 2, cy + s * l * 0.5f - ny * wd / 2};
    art_poly(f, role, p, 4);
}

/* a leaf: a pointed oval from (cx, cy) out at angle a (degrees, y up), long l, wide wd */
static inline void mjt_leaf(art_fig *f, int role, float cx, float cy, float a, float l, float wd)
{
    enum { K = 10 };
    float c = cosf(a * MJT_PI / 180), s = -sinf(a * MJT_PI / 180), p[4 * K + 4];
    for (int i = 0; i <= K; i++) {
        float t = (float)i / K, hw = wd / 2 * sinf(MJT_PI * t), x = cx + c * l * t, y = cy + s * l * t;
        p[2 * i] = x - s * hw;
        p[2 * i + 1] = y + c * hw;
        p[4 * K + 2 - 2 * i] = x + s * hw;
        p[4 * K + 3 - 2 * i] = y - c * hw;
    }
    art_poly(f, role, p, 2 * K + 2);
}

/* ---- the faces --------------------------------------------------------------------------------------------------- */
/* the dots: (x, y, radius, colour 0 blue 1 green 2 red) */
typedef struct { float x, y, r; int c; } mjt_spot;
static const mjt_spot MJT_DOTS[9][9] = {
    {{0}},
    {{0.5f, 0.36f, 0.18f, 1}, {0.5f, 0.94f, 0.18f, 0}},
    {{0.25f, 0.30f, 0.14f, 0}, {0.5f, 0.65f, 0.14f, 2}, {0.75f, 1.00f, 0.14f, 1}},
    {{0.29f, 0.38f, 0.16f, 0}, {0.71f, 0.38f, 0.16f, 1}, {0.29f, 0.92f, 0.16f, 1}, {0.71f, 0.92f, 0.16f, 0}},
    {{0.26f, 0.30f, 0.14f, 0}, {0.74f, 0.30f, 0.14f, 1}, {0.5f, 0.65f, 0.14f, 2}, {0.26f, 1.00f, 0.14f, 1},
     {0.74f, 1.00f, 0.14f, 0}},
    {{0.29f, 0.26f, 0.13f, 1}, {0.71f, 0.26f, 0.13f, 1}, {0.29f, 0.70f, 0.13f, 2}, {0.71f, 0.70f, 0.13f, 2},
     {0.29f, 1.04f, 0.13f, 2}, {0.71f, 1.04f, 0.13f, 2}},
    {{0.22f, 0.20f, 0.105f, 1}, {0.5f, 0.32f, 0.105f, 1}, {0.78f, 0.44f, 0.105f, 1}, {0.30f, 0.76f, 0.12f, 2},
     {0.70f, 0.76f, 0.12f, 2}, {0.30f, 1.06f, 0.12f, 2}, {0.70f, 1.06f, 0.12f, 2}},
    {{0.30f, 0.22f, 0.11f, 0}, {0.70f, 0.22f, 0.11f, 0}, {0.30f, 0.50f, 0.11f, 0}, {0.70f, 0.50f, 0.11f, 0},
     {0.30f, 0.80f, 0.11f, 0}, {0.70f, 0.80f, 0.11f, 0}, {0.30f, 1.08f, 0.11f, 0}, {0.70f, 1.08f, 0.11f, 0}},
    {{0.22f, 0.28f, 0.11f, 0}, {0.5f, 0.28f, 0.11f, 0}, {0.78f, 0.28f, 0.11f, 0}, {0.22f, 0.65f, 0.11f, 2},
     {0.5f, 0.65f, 0.11f, 2}, {0.78f, 0.65f, 0.11f, 2}, {0.22f, 1.02f, 0.11f, 1}, {0.5f, 1.02f, 0.11f, 1},
     {0.78f, 1.02f, 0.11f, 1}},
};
static const float *const MJT_SUIT[3] = {MJT_BLUE, MJT_GREEN, MJT_RED};

static inline void mjt_face_dots(uint8_t *o, int w, int h, int n)
{
    if (n == 1) {                               /* the one: a large round flower of rings */
        art_fig *f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.65f, 0.37f, 0.37f);
        art_arc(f, ART_MARK, 0.5f, 0.65f, 0.27f, 0.32f, 0, 360);
        for (int k = 0; k < 12; k++) {
            float a = k * 30 * MJT_PI / 180;
            art_ellipse(f, ART_MARK, 0.5f + 0.345f * cosf(a), 0.65f + 0.345f * sinf(a), 0.018f, 0.018f);
        }
        mjt_paint(o, w, h, f, MJT_GREEN, NULL, 0.25f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.65f, 0.21f, 0.21f);
        art_arc(f, ART_MARK, 0.5f, 0.65f, 0.12f, 0.155f, 0, 360);
        mjt_paint(o, w, h, f, MJT_BLUE, NULL, 0.25f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.65f, 0.075f, 0.075f);
        mjt_paint(o, w, h, f, MJT_RED, NULL, 0.2f);
        return;
    }
    for (int c = 0; c < 3; c++) {
        art_fig *f = art_fig_new();
        int any = 0;
        for (int i = 0; i < n; i++) {
            const mjt_spot *d = &MJT_DOTS[n - 1][i];
            if (d->c != c) continue;
            art_ellipse(f, ART_PART, d->x, d->y, d->r, d->r);
            art_arc(f, ART_MARK, d->x, d->y, d->r * 0.42f, d->r * 0.66f, 0, 360);
            any = 1;
        }
        if (any) mjt_paint(o, w, h, f, MJT_SUIT[c], NULL, 0.25f);
        else art_fig_free(f);
    }
}

/* a bamboo stick: its three joints */
static inline void mjt_stick(art_fig *f, float x, float y0, float y1, float wd)
{
    art_rect(f, ART_PART, x - wd / 2, y0, x + wd / 2, y1);
    float m = (y0 + y1) / 2, j = 0.018f;
    art_rect(f, ART_MARK, x - wd / 2, m - j, x + wd / 2, m + j);
    art_rect(f, ART_MARK, x - wd / 2, y0 + 0.02f, x + wd / 2, y0 + 0.02f + j);
    art_rect(f, ART_MARK, x - wd / 2, y1 - 0.02f - j, x + wd / 2, y1 - 0.02f);
}

/* the sticks: (x, top, bottom, colour 0 blue 1 green 2 red) */
typedef struct { float x, y0, y1; int c; } mjt_stalk;
static const mjt_stalk MJT_STICKS[9][9] = {
    {{0}},
    {{0.5f, 0.14f, 0.56f, 0}, {0.5f, 0.74f, 1.16f, 1}},
    {{0.5f, 0.14f, 0.56f, 0}, {0.3f, 0.74f, 1.16f, 1}, {0.7f, 0.74f, 1.16f, 1}},
    {{0.3f, 0.14f, 0.56f, 0}, {0.7f, 0.14f, 0.56f, 1}, {0.3f, 0.74f, 1.16f, 1}, {0.7f, 0.74f, 1.16f, 0}},
    {{0.22f, 0.14f, 0.56f, 1}, {0.78f, 0.14f, 0.56f, 0}, {0.5f, 0.44f, 0.86f, 2}, {0.22f, 0.74f, 1.16f, 0},
     {0.78f, 0.74f, 1.16f, 1}},
    {{0.22f, 0.14f, 0.56f, 1}, {0.5f, 0.14f, 0.56f, 1}, {0.78f, 0.14f, 0.56f, 1}, {0.22f, 0.74f, 1.16f, 0},
     {0.5f, 0.74f, 1.16f, 0}, {0.78f, 0.74f, 1.16f, 0}},
    {{0.5f, 0.10f, 0.40f, 2}, {0.22f, 0.46f, 0.78f, 1}, {0.5f, 0.46f, 0.78f, 0}, {0.78f, 0.46f, 0.78f, 1},
     {0.22f, 0.86f, 1.18f, 1}, {0.5f, 0.86f, 1.18f, 0}, {0.78f, 0.86f, 1.18f, 1}},
    {{0.17f, 0.14f, 0.56f, 1}, {0.39f, 0.14f, 0.56f, 2}, {0.61f, 0.14f, 0.56f, 2}, {0.83f, 0.14f, 0.56f, 1},
     {0.17f, 0.74f, 1.16f, 1}, {0.39f, 0.74f, 1.16f, 0}, {0.61f, 0.74f, 1.16f, 0}, {0.83f, 0.74f, 1.16f, 1}},
    {{0.22f, 0.10f, 0.40f, 1}, {0.5f, 0.10f, 0.40f, 2}, {0.78f, 0.10f, 0.40f, 0}, {0.22f, 0.50f, 0.80f, 1},
     {0.5f, 0.50f, 0.80f, 2}, {0.78f, 0.50f, 0.80f, 0}, {0.22f, 0.90f, 1.20f, 1}, {0.5f, 0.90f, 1.20f, 2},
     {0.78f, 0.90f, 1.20f, 0}},
};

static inline void mjt_face_bird(uint8_t *o, int w, int h)  /* the one of bamboo: a bird on a perch */
{
    static const float AMBER[3] = {214, 150, 46};
    art_fig *f = art_fig_new();                 /* its tail, fanned out behind it */
    mjt_leaf(f, ART_PART, 0.46f, 0.72f, 160, 0.40f, 0.14f);
    mjt_leaf(f, ART_PART, 0.46f, 0.72f, 185, 0.40f, 0.14f);
    mjt_leaf(f, ART_PART, 0.46f, 0.72f, 210, 0.36f, 0.14f);
    mjt_paint(o, w, h, f, MJT_BLUE, NULL, 0.2f);
    f = art_fig_new();                          /* its legs, its perch */
    mjt_band(f, ART_PART, 0.47f, 0.86f, 0.43f, 1.04f, 0.035f);
    mjt_band(f, ART_PART, 0.57f, 0.86f, 0.60f, 1.04f, 0.035f);
    art_rect(f, ART_PART, 0.22f, 1.03f, 0.84f, 1.08f);
    mjt_paint(o, w, h, f, AMBER, NULL, 0.1f);
    f = art_fig_new();                          /* its body and wing */
    art_ellipse(f, ART_PART, 0.54f, 0.72f, 0.20f, 0.16f);
    mjt_petal(f, ART_MARK, 0.62f, 0.66f, 200, 0.26f, 0.11f);
    static const float WING[3] = {16, 76, 44};
    mjt_paint(o, w, h, f, MJT_GREEN, WING, 0.25f);
    f = art_fig_new();                          /* its head, its eye */
    art_ellipse(f, ART_PART, 0.70f, 0.48f, 0.105f, 0.105f);
    art_ellipse(f, ART_MARK, 0.725f, 0.46f, 0.022f, 0.022f);
    const float crest[] = {0.62f, 0.40f, 0.60f, 0.26f, 0.70f, 0.38f};
    art_poly(f, ART_PART, crest, 3);
    mjt_paint(o, w, h, f, MJT_RED, NULL, 0.2f);
    f = art_fig_new();                          /* its beak */
    const float beak[] = {0.79f, 0.45f, 0.90f, 0.50f, 0.79f, 0.53f};
    art_poly(f, ART_PART, beak, 3);
    mjt_paint(o, w, h, f, AMBER, NULL, 0.1f);
}

static inline void mjt_face_bamboo(uint8_t *o, int w, int h, int n)
{
    if (n == 1) { mjt_face_bird(o, w, h); return; }
    float wd = n == 8 ? 0.10f : 0.12f;
    for (int c = 0; c < 3; c++) {
        art_fig *f = art_fig_new();
        int any = 0;
        for (int i = 0; i < n; i++) {
            const mjt_stalk *s = &MJT_STICKS[n - 1][i];
            if (s->c != c) continue;
            mjt_stick(f, s->x, s->y0, s->y1, wd);
            any = 1;
        }
        float joint[3];
        for (int k = 0; k < 3; k++) joint[k] = MJT_SUIT[c][k] * 0.5f;
        if (any) mjt_paint(o, w, h, f, MJT_SUIT[c], joint, 0.35f);
        else art_fig_free(f);
    }
}

/* 一..九 */
static const int MJT_NUMERAL[9] = {0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D};
static const int MJT_WIND[4] = {0x6771, 0x5357, 0x897F, 0x5317};   /* 東南西北 */
#define MJT_WAN 0x842C                          /* 萬 */
#define MJT_ZHONG 0x4E2D                        /* 中 */
#define MJT_FA 0x767C                           /* 發 */

static inline void mjt_face_flower(uint8_t *o, int w, int h, int k)
{
    static const float PLUM[3] = {214, 96, 128}, ORCHID[3] = {140, 82, 172}, MUM[3] = {226, 162, 40},
                       LEAF[3] = {58, 132, 64}, BROWN[3] = {120, 76, 44}, HEART[3] = {250, 214, 90};
    float cx = 0.54f, cy = 0.70f;
    art_fig *f = art_fig_new();                 /* a stem and leaves */
    if (k == 3) {                               /* bamboo: two stalks, their leaves */
        mjt_stick(f, 0.44f, 0.34f, 1.14f, 0.075f);
        mjt_stick(f, 0.64f, 0.50f, 1.14f, 0.065f);
        mjt_leaf(f, ART_PART, 0.46f, 0.52f, 30, 0.30f, 0.09f);
        mjt_leaf(f, ART_PART, 0.43f, 0.70f, 150, 0.28f, 0.09f);
        mjt_leaf(f, ART_PART, 0.66f, 0.66f, 20, 0.24f, 0.08f);
        float joint[3] = {LEAF[0] * 0.55f, LEAF[1] * 0.55f, LEAF[2] * 0.55f};
        mjt_paint(o, w, h, f, LEAF, joint, 0.3f);
    } else {
        mjt_band(f, ART_PART, cx, cy, cx - 0.05f, 1.14f, 0.04f);
        mjt_leaf(f, ART_PART, cx - 0.03f, 0.98f, k == 1 ? 150 : 200, k == 1 ? 0.34f : 0.24f, 0.10f);
        mjt_leaf(f, ART_PART, cx - 0.04f, 1.04f, k == 1 ? 30 : -20, k == 1 ? 0.34f : 0.22f, 0.09f);
        mjt_paint(o, w, h, f, k == 0 ? BROWN : LEAF, NULL, 0.2f);
        f = art_fig_new();
        if (k == 0)                             /* plum: five round petals */
            for (int i = 0; i < 5; i++) {
                float a = (90 + 72 * i) * MJT_PI / 180;
                art_ellipse(f, ART_PART, cx + 0.135f * cosf(a), cy - 0.135f * sinf(a), 0.105f, 0.105f);
            }
        else if (k == 1)                        /* orchid: three long petals */
            for (int i = 0; i < 3; i++) mjt_leaf(f, ART_PART, cx, cy, 90 + 120 * i, 0.30f, 0.15f);
        else                                    /* chrysanthemum: many narrow petals */
            for (int i = 0; i < 14; i++) mjt_petal(f, ART_PART, cx, cy, i * 360 / 14.0f, 0.27f, 0.07f);
        art_ellipse(f, ART_MARK, cx, cy, 0.05f, 0.05f);
        mjt_paint(o, w, h, f, k == 0 ? PLUM : k == 1 ? ORCHID : MUM, k == 2 ? BROWN : HEART, 0.25f);
    }
    mjt_glyph(o, w, h, '1' + k, 0.2f, 0.2f, 0.3f, MJT_U_RED);
}

static inline void mjt_face_season(uint8_t *o, int w, int h, int k)
{
    static const float SPRING[3] = {70, 150, 66}, SUN[3] = {232, 140, 40}, AUTUMN[3] = {188, 88, 36},
                       SNOW[3] = {70, 120, 190}, BUD[3] = {226, 110, 140}, VEIN[3] = {120, 52, 20};
    float cx = 0.55f, cy = 0.72f;
    art_fig *f = art_fig_new();
    switch (k) {
    case 0:                                     /* spring: a sprout, a bud */
        mjt_band(f, ART_PART, cx, 1.14f, cx, 0.56f, 0.045f);
        mjt_leaf(f, ART_PART, cx, 0.86f, 35, 0.30f, 0.14f);
        mjt_leaf(f, ART_PART, cx, 0.74f, 150, 0.30f, 0.14f);
        mjt_paint(o, w, h, f, SPRING, NULL, 0.25f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, cx, 0.50f, 0.075f, 0.10f);
        mjt_paint(o, w, h, f, BUD, NULL, 0.25f);
        break;
    case 1:                                     /* summer: the sun */
        for (int i = 0; i < 12; i++) mjt_petal(f, ART_PART, cx, cy, i * 30.0f, 0.36f, 0.09f);
        art_ellipse(f, ART_PART, cx, cy, 0.17f, 0.17f);
        mjt_paint(o, w, h, f, SUN, NULL, 0.25f);
        break;
    case 2: {                                   /* autumn: a falling leaf, its veins */
        float x0 = cx - 0.22f, y0 = cy + 0.30f, c = cosf(55 * MJT_PI / 180), s = -sinf(55 * MJT_PI / 180);
        mjt_band(f, ART_PART, x0 - c * 0.12f, y0 - s * 0.12f, x0, y0, 0.035f);   /* its mjt_stalk */
        mjt_leaf(f, ART_PART, x0, y0, 55, 0.66f, 0.40f);
        mjt_band(f, ART_MARK, x0, y0, x0 + c * 0.58f, y0 + s * 0.58f, 0.022f);
        for (int i = 1; i <= 3; i++) {
            float t = 0.15f * i, px = x0 + c * t, py = y0 + s * t, cc = cosf(100 * MJT_PI / 180),
                  ss = -sinf(100 * MJT_PI / 180),
                  c2 = cosf(10 * MJT_PI / 180), s2 = -sinf(10 * MJT_PI / 180), k = 0.13f - 0.02f * i;
            mjt_band(f, ART_MARK, px, py, px + cc * k, py + ss * k, 0.016f);
            mjt_band(f, ART_MARK, px, py, px + c2 * k, py + s2 * k, 0.016f);
        }
        mjt_paint(o, w, h, f, AUTUMN, VEIN, 0.25f);
        break;
    }
    default:                                    /* winter: a snowflake */
        for (int i = 0; i < 3; i++) {
            float a = (90 + 60 * i) * MJT_PI / 180, c = cosf(a) * 0.30f, s = sinf(a) * 0.30f;
            mjt_band(f, ART_PART, cx - c, cy + s, cx + c, cy - s, 0.05f);
        }
        for (int i = 0; i < 6; i++) {
            float a = (90 + 60 * i) * MJT_PI / 180, px = cx + cosf(a) * 0.2f, py = cy - sinf(a) * 0.2f;
            mjt_band(f, ART_PART, px, py, px + cosf(a + 0.8f) * 0.09f, py - sinf(a + 0.8f) * 0.09f, 0.035f);
            mjt_band(f, ART_PART, px, py, px + cosf(a - 0.8f) * 0.09f, py - sinf(a - 0.8f) * 0.09f, 0.035f);
        }
        mjt_paint(o, w, h, f, SNOW, NULL, 0.15f);
        break;
    }
    mjt_glyph(o, w, h, '1' + k, 0.2f, 0.2f, 0.3f, MJT_U_BLUE);
}

/* a face's picture (no tile under it), w x h, clear around */
static inline void mjt_face_pic(int face, int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    if (face < MJ_BAMBOO) mjt_face_dots(o, w, h, face - MJ_DOTS + 1);
    else if (face < MJ_CHARS) mjt_face_bamboo(o, w, h, face - MJ_BAMBOO + 1);
    else if (face < MJ_WINDS) {
        mjt_glyph(o, w, h, MJT_NUMERAL[face - MJ_CHARS], 0.5f, 0.36f, 0.50f, MJT_U_INK);
        mjt_glyph(o, w, h, MJT_WAN, 0.5f, 0.92f, 0.56f, MJT_U_RED);
    } else if (face < MJ_DRAGONS) mjt_glyph(o, w, h, MJT_WIND[face - MJ_WINDS], 0.5f, 0.65f, 0.74f, MJT_U_INK);
    else if (face == MJ_DRAGONS) mjt_glyph(o, w, h, MJT_ZHONG, 0.5f, 0.65f, 0.80f, MJT_U_RED);
    else if (face == MJ_DRAGONS + 1) mjt_glyph(o, w, h, MJT_FA, 0.5f, 0.65f, 0.76f, MJT_U_GREEN);
    else if (face == MJ_DRAGONS + 2) {          /* the white dragon: a blue frame */
        static const float in[4][4] = {{0.15f, 0.18f, 0.85f, 1.12f}, {0.21f, 0.24f, 0.79f, 1.06f},
                                       {0.26f, 0.29f, 0.74f, 1.01f}, {0.29f, 0.32f, 0.71f, 0.98f}};
        for (int i = 0; i < 4; i++) {
            art_fig *f = art_fig_new();
            art_rect(f, ART_PART, in[i][0], in[i][1], in[i][2], in[i][3]);
            mjt_paint(o, w, h, f, i & 1 ? MJT_IVORY : MJT_BLUE, NULL, 0);
        }
    } else if (face < MJ_SEASONS) mjt_face_flower(o, w, h, face - MJ_FLOWERS);
    else mjt_face_season(o, w, h, face - MJ_SEASONS);
}

/* ---- the tile: an ivory face, a thin ivory layer, the green back; a soft shadow ---------------------------------- */
/* coverage of a rounded rectangle (x0, y0, w, h, radius r) at pixel centre (px, py) */
static inline float mjt_rrect(float px, float py, float x0, float y0, float w, float h, float r)
{
    float dx = fmaxf(fabsf(px - (x0 + w / 2)) - (w / 2 - r), 0), dy = fmaxf(fabsf(py - (y0 + h / 2)) - (h / 2 - r), 0);
    return clampf(r - hypotf(dx, dy) + 0.5f, 0, 1);
}

static inline void mjt_over(uint8_t *p, const float c[3], float a)
{
    float da = p[3] / 255.0f, oa = a + da * (1 - a);
    if (oa <= 0) return;
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf((c[i] * a + p[i] * da * (1 - a)) / oa + 0.5f, 0, 255);
    p[3] = (uint8_t)(oa * 255 + 0.5f);
}

/* the tile's body (fw x fh face, d thick: (fw + d) x (fh + d)) */
static inline void mjt_tile_pic(int fw, int fh, int d, uint8_t *o)
{
    static const float BACK[3] = {52, 124, 80}, LAYER[3] = {222, 212, 186}, EDGE[3] = {150, 138, 112};
    int w = fw + d, h = fh + d;
    float r = 0.12f * fw;
    memset(o, 0, (size_t)w * h * 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            uint8_t *p = o + 4 * (y * w + x);
            float a = mjt_rrect(px, py, (float)d, (float)d, (float)fw, (float)fh, r);   /* the green back */
            if (a > 0) {
                float k = 0.85f + 0.15f * (1 - (px + py) / (w + h));
                const float c[3] = {BACK[0] * k, BACK[1] * k, BACK[2] * k};
                mjt_over(p, c, a);
            }
            a = mjt_rrect(px, py, 0.5f * d, 0.5f * d, (float)fw, (float)fh, r);         /* the ivory between */
            if (a > 0) mjt_over(p, LAYER, a);
            a = mjt_rrect(px, py, 0, 0, (float)fw, (float)fh, r);                       /* the face */
            if (a > 0) {
                float in = mjt_rrect(px, py, 1.2f, 1.2f, fw - 2.4f, fh - 2.4f, r - 1.2f);   /* its rim */
                float lit = 1 + 0.035f * (1 - 2 * (px / fw + py / fh) / 2);
                float c[3];
                for (int i = 0; i < 3; i++) c[i] = (MJT_IVORY[i] * lit) * in + EDGE[i] * (1 - in);
                mjt_over(p, c, a);
            }
        }
}

/* the soft shadow a tile casts (its face's size, blurred over b pixels) */
static inline void mjt_shadow_pic(int w, int h, float b, uint8_t *o)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = fminf(x + 0.5f, w - x - 0.5f), dy = fminf(y + 0.5f, h - y - 0.5f);
            float k = clampf(dx / b, 0, 1) * clampf(dy / b, 0, 1);
            uint8_t *p = o + 4 * (y * w + x);
            p[0] = p[1] = p[2] = 0;
            p[3] = (uint8_t)(255 * 0.42f * k * k * (3 - 2 * k));
        }
}

/* the tile lying face down, seen from above as mjt_tile_pic ((fw + d) x (fh + d)): the green back up, lit from the
 * upper left, a thin ivory layer, the ivory face's edge under it at the lower right */
static inline void mjt_back_pic(int fw, int fh, int d, uint8_t *o)
{
    static const float BACK[3] = {52, 124, 80}, RIM[3] = {30, 82, 52}, LAYER[3] = {222, 212, 186},
                       EDGE[3] = {196, 186, 160};
    int w = fw + d, h = fh + d;
    float r = 0.12f * fw;
    memset(o, 0, (size_t)w * h * 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            uint8_t *p = o + 4 * (y * w + x);
            float a = mjt_rrect(px, py, (float)d, (float)d, (float)fw, (float)fh, r);   /* the ivory face, under */
            if (a > 0) mjt_over(p, EDGE, a);
            a = mjt_rrect(px, py, 0.5f * d, 0.5f * d, (float)fw, (float)fh, r);         /* the ivory between */
            if (a > 0) mjt_over(p, LAYER, a);
            a = mjt_rrect(px, py, 0, 0, (float)fw, (float)fh, r);                       /* the green back */
            if (a > 0) {
                float in = mjt_rrect(px, py, 1.2f, 1.2f, fw - 2.4f, fh - 2.4f, r - 1.2f);
                float lit = 1.08f - 0.16f * (px / fw + py / fh) / 2, c[3];
                for (int i = 0; i < 3; i++) c[i] = clampf(BACK[i] * lit, 0, 255) * in + RIM[i] * (1 - in);
                mjt_over(p, c, a);
            }
        }
}

#endif
