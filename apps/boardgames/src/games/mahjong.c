/*
 * Mahjong Solitaire in RetroStone Board, for one player: the tiles stacked on the green table, ivory faces on a
 * bamboo-green back, each layer drawn up and to the left of the one under it, its shadow on the tiles below (the
 * rules: mahjong_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The faces are drawn by code: the dots, the bamboo (the one a bird), the characters (the numeral over 萬), the
 * winds 東南西北, the dragons 中, 發 and the white one (a blue frame), the flowers and the seasons (a picture, and
 * their number in a corner). The characters are characters, not words: help/glyphs.txt keeps them in the fonts.
 *
 * The input: tap a free tile (the select mark), then its match: both lift away. A tap on the tile picked lets it
 * go; on another free tile, picks that one. Only free tiles can be played, so a tap goes to the free tile under the
 * finger, or the free tile nearest to it if the finger is just beside one (the Turtle's tiles are small on a phone).
 * The tiles that are not free are dimmed. The bar: the tiles left, a hint button (two matching free tiles blink),
 * and, when no pair is left, the shuffle button (big in the middle of the board, and in the bar when it has room):
 * the tiles left are dealt again so that they can be cleared. Undo takes back a pair, or a shuffle.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/mahjong.h"
#include "shell.h"
#include "text.h"

#define ASPECT 1.3f                             /* a tile's face: its height to its width */
#define THICK 0.13f                             /* a tile's thickness, a part of its width */
#define TAKE_MS 420                             /* a pair lifting away */
#define HINT_MS 2400
#define PI_F 3.14159265f

/* ---- the colours ------------------------------------------------------------------------------------------------- */
static const float RED[3] = {178, 40, 34}, GREEN[3] = {26, 116, 70}, BLUE[3] = {32, 72, 150};
static const float IVORY[3] = {248, 243, 228};
static const uint8_t U_RED[3] = {178, 40, 34}, U_GREEN[3] = {26, 116, 70}, U_BLUE[3] = {32, 72, 150},
                     U_INK[3] = {34, 30, 30};

/* ---- drawing helpers: figures in the face's unit (its width; y down to ASPECT) ----------------------------------- */
/* renders f in `fill` (its marks in `mark`) and paints it over o (w x h); frees f */
static void paint(uint8_t *o, int w, int h, art_fig *f, const float fill[3], const float mark[3], float light)
{
    art_style st;
    for (int i = 0; i < 3; i++) {
        st.fill[i] = fill[i];
        st.outer[i] = fill[i] * 0.45f;
        st.inner[i] = fill[i] * 0.6f;
        st.mark[i] = mark ? mark[i] : IVORY[i];
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
static void glyph(uint8_t *o, int w, int h, int cp, float cx, float cy, float em, const uint8_t rgb[3])
{
    int g = (int)(em * w * 1.35f) + 2;
    uint8_t *t = calloc((size_t)g * g, 4);
    if (!t) return;
    text_glyph(cp, em * w, 1, rgb, t, g, g);
    art_blend(o, w, h, t, g, g, (int)lroundf(cx * w - g / 2.0f), (int)lroundf(cy * w - g / 2.0f));
    free(t);
}

/* a band from (x0, y0) to (x1, y1), wide `wd` */
static void band(art_fig *f, int role, float x0, float y0, float x1, float y1, float wd)
{
    float dx = x1 - x0, dy = y1 - y0, l = hypotf(dx, dy) + 1e-6f, nx = -dy / l * wd / 2, ny = dx / l * wd / 2;
    const float q[] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    art_poly(f, role, q, 4);
}

/* a pointed petal or leaf from (cx, cy) out at angle a (degrees, y up), long l, wide wd */
static void petal(art_fig *f, int role, float cx, float cy, float a, float l, float wd)
{
    float c = cosf(a * PI_F / 180), s = -sinf(a * PI_F / 180), nx = -s, ny = c;
    const float p[] = {cx, cy, cx + c * l * 0.5f + nx * wd / 2, cy + s * l * 0.5f + ny * wd / 2, cx + c * l, cy + s * l,
                       cx + c * l * 0.5f - nx * wd / 2, cy + s * l * 0.5f - ny * wd / 2};
    art_poly(f, role, p, 4);
}

/* a leaf: a pointed oval from (cx, cy) out at angle a (degrees, y up), long l, wide wd */
static void leaf(art_fig *f, int role, float cx, float cy, float a, float l, float wd)
{
    enum { K = 10 };
    float c = cosf(a * PI_F / 180), s = -sinf(a * PI_F / 180), p[4 * K + 4];
    for (int i = 0; i <= K; i++) {
        float t = (float)i / K, hw = wd / 2 * sinf(PI_F * t), x = cx + c * l * t, y = cy + s * l * t;
        p[2 * i] = x - s * hw;
        p[2 * i + 1] = y + c * hw;
        p[4 * K + 2 - 2 * i] = x + s * hw;
        p[4 * K + 3 - 2 * i] = y - c * hw;
    }
    art_poly(f, role, p, 2 * K + 2);
}

/* ---- the faces --------------------------------------------------------------------------------------------------- */
/* the dots: (x, y, radius, colour 0 blue 1 green 2 red) */
typedef struct { float x, y, r; int c; } spot;
static const spot DOTS[9][9] = {
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
static const float *const SUIT[3] = {BLUE, GREEN, RED};

static void face_dots(uint8_t *o, int w, int h, int n)
{
    if (n == 1) {                               /* the one: a large round flower of rings */
        art_fig *f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.65f, 0.37f, 0.37f);
        art_arc(f, ART_MARK, 0.5f, 0.65f, 0.27f, 0.32f, 0, 360);
        for (int k = 0; k < 12; k++) {
            float a = k * 30 * PI_F / 180;
            art_ellipse(f, ART_MARK, 0.5f + 0.345f * cosf(a), 0.65f + 0.345f * sinf(a), 0.018f, 0.018f);
        }
        paint(o, w, h, f, GREEN, NULL, 0.25f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.65f, 0.21f, 0.21f);
        art_arc(f, ART_MARK, 0.5f, 0.65f, 0.12f, 0.155f, 0, 360);
        paint(o, w, h, f, BLUE, NULL, 0.25f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.65f, 0.075f, 0.075f);
        paint(o, w, h, f, RED, NULL, 0.2f);
        return;
    }
    for (int c = 0; c < 3; c++) {
        art_fig *f = art_fig_new();
        int any = 0;
        for (int i = 0; i < n; i++) {
            const spot *d = &DOTS[n - 1][i];
            if (d->c != c) continue;
            art_ellipse(f, ART_PART, d->x, d->y, d->r, d->r);
            art_arc(f, ART_MARK, d->x, d->y, d->r * 0.42f, d->r * 0.66f, 0, 360);
            any = 1;
        }
        if (any) paint(o, w, h, f, SUIT[c], NULL, 0.25f);
        else art_fig_free(f);
    }
}

/* a bamboo stick: its three joints */
static void stick(art_fig *f, float x, float y0, float y1, float wd)
{
    art_rect(f, ART_PART, x - wd / 2, y0, x + wd / 2, y1);
    float m = (y0 + y1) / 2, j = 0.018f;
    art_rect(f, ART_MARK, x - wd / 2, m - j, x + wd / 2, m + j);
    art_rect(f, ART_MARK, x - wd / 2, y0 + 0.02f, x + wd / 2, y0 + 0.02f + j);
    art_rect(f, ART_MARK, x - wd / 2, y1 - 0.02f - j, x + wd / 2, y1 - 0.02f);
}

/* the sticks: (x, top, bottom, colour 0 blue 1 green 2 red) */
typedef struct { float x, y0, y1; int c; } stalk;
static const stalk STICKS[9][9] = {
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

static void face_bird(uint8_t *o, int w, int h)  /* the one of bamboo: a bird on a perch */
{
    static const float AMBER[3] = {214, 150, 46};
    art_fig *f = art_fig_new();                 /* its tail, fanned out behind it */
    leaf(f, ART_PART, 0.46f, 0.72f, 160, 0.40f, 0.14f);
    leaf(f, ART_PART, 0.46f, 0.72f, 185, 0.40f, 0.14f);
    leaf(f, ART_PART, 0.46f, 0.72f, 210, 0.36f, 0.14f);
    paint(o, w, h, f, BLUE, NULL, 0.2f);
    f = art_fig_new();                          /* its legs, its perch */
    band(f, ART_PART, 0.47f, 0.86f, 0.43f, 1.04f, 0.035f);
    band(f, ART_PART, 0.57f, 0.86f, 0.60f, 1.04f, 0.035f);
    art_rect(f, ART_PART, 0.22f, 1.03f, 0.84f, 1.08f);
    paint(o, w, h, f, AMBER, NULL, 0.1f);
    f = art_fig_new();                          /* its body and wing */
    art_ellipse(f, ART_PART, 0.54f, 0.72f, 0.20f, 0.16f);
    petal(f, ART_MARK, 0.62f, 0.66f, 200, 0.26f, 0.11f);
    static const float WING[3] = {16, 76, 44};
    paint(o, w, h, f, GREEN, WING, 0.25f);
    f = art_fig_new();                          /* its head, its eye */
    art_ellipse(f, ART_PART, 0.70f, 0.48f, 0.105f, 0.105f);
    art_ellipse(f, ART_MARK, 0.725f, 0.46f, 0.022f, 0.022f);
    const float crest[] = {0.62f, 0.40f, 0.60f, 0.26f, 0.70f, 0.38f};
    art_poly(f, ART_PART, crest, 3);
    paint(o, w, h, f, RED, NULL, 0.2f);
    f = art_fig_new();                          /* its beak */
    const float beak[] = {0.79f, 0.45f, 0.90f, 0.50f, 0.79f, 0.53f};
    art_poly(f, ART_PART, beak, 3);
    paint(o, w, h, f, AMBER, NULL, 0.1f);
}

static void face_bamboo(uint8_t *o, int w, int h, int n)
{
    if (n == 1) { face_bird(o, w, h); return; }
    float wd = n == 8 ? 0.10f : 0.12f;
    for (int c = 0; c < 3; c++) {
        art_fig *f = art_fig_new();
        int any = 0;
        for (int i = 0; i < n; i++) {
            const stalk *s = &STICKS[n - 1][i];
            if (s->c != c) continue;
            stick(f, s->x, s->y0, s->y1, wd);
            any = 1;
        }
        float joint[3];
        for (int k = 0; k < 3; k++) joint[k] = SUIT[c][k] * 0.5f;
        if (any) paint(o, w, h, f, SUIT[c], joint, 0.35f);
        else art_fig_free(f);
    }
}

static const int NUMERAL[9] = {0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D};  /* 一..九 */
static const int WIND[4] = {0x6771, 0x5357, 0x897F, 0x5317};   /* 東南西北 */
#define WAN 0x842C                              /* 萬 */
#define ZHONG 0x4E2D                            /* 中 */
#define FA 0x767C                               /* 發 */

static void face_flower(uint8_t *o, int w, int h, int k)
{
    static const float PLUM[3] = {214, 96, 128}, ORCHID[3] = {140, 82, 172}, MUM[3] = {226, 162, 40},
                       LEAF[3] = {58, 132, 64}, BROWN[3] = {120, 76, 44}, HEART[3] = {250, 214, 90};
    float cx = 0.54f, cy = 0.70f;
    art_fig *f = art_fig_new();                 /* a stem and leaves */
    if (k == 3) {                               /* bamboo: two stalks, their leaves */
        stick(f, 0.44f, 0.34f, 1.14f, 0.075f);
        stick(f, 0.64f, 0.50f, 1.14f, 0.065f);
        leaf(f, ART_PART, 0.46f, 0.52f, 30, 0.30f, 0.09f);
        leaf(f, ART_PART, 0.43f, 0.70f, 150, 0.28f, 0.09f);
        leaf(f, ART_PART, 0.66f, 0.66f, 20, 0.24f, 0.08f);
        float joint[3] = {LEAF[0] * 0.55f, LEAF[1] * 0.55f, LEAF[2] * 0.55f};
        paint(o, w, h, f, LEAF, joint, 0.3f);
    } else {
        band(f, ART_PART, cx, cy, cx - 0.05f, 1.14f, 0.04f);
        leaf(f, ART_PART, cx - 0.03f, 0.98f, k == 1 ? 150 : 200, k == 1 ? 0.34f : 0.24f, 0.10f);
        leaf(f, ART_PART, cx - 0.04f, 1.04f, k == 1 ? 30 : -20, k == 1 ? 0.34f : 0.22f, 0.09f);
        paint(o, w, h, f, k == 0 ? BROWN : LEAF, NULL, 0.2f);
        f = art_fig_new();
        if (k == 0)                             /* plum: five round petals */
            for (int i = 0; i < 5; i++) {
                float a = (90 + 72 * i) * PI_F / 180;
                art_ellipse(f, ART_PART, cx + 0.135f * cosf(a), cy - 0.135f * sinf(a), 0.105f, 0.105f);
            }
        else if (k == 1)                        /* orchid: three long petals */
            for (int i = 0; i < 3; i++) leaf(f, ART_PART, cx, cy, 90 + 120 * i, 0.30f, 0.15f);
        else                                    /* chrysanthemum: many narrow petals */
            for (int i = 0; i < 14; i++) petal(f, ART_PART, cx, cy, i * 360 / 14.0f, 0.27f, 0.07f);
        art_ellipse(f, ART_MARK, cx, cy, 0.05f, 0.05f);
        paint(o, w, h, f, k == 0 ? PLUM : k == 1 ? ORCHID : MUM, k == 2 ? BROWN : HEART, 0.25f);
    }
    glyph(o, w, h, '1' + k, 0.2f, 0.2f, 0.3f, U_RED);
}

static void face_season(uint8_t *o, int w, int h, int k)
{
    static const float SPRING[3] = {70, 150, 66}, SUN[3] = {232, 140, 40}, AUTUMN[3] = {188, 88, 36},
                       SNOW[3] = {70, 120, 190}, BUD[3] = {226, 110, 140}, VEIN[3] = {120, 52, 20};
    float cx = 0.55f, cy = 0.72f;
    art_fig *f = art_fig_new();
    switch (k) {
    case 0:                                     /* spring: a sprout, a bud */
        band(f, ART_PART, cx, 1.14f, cx, 0.56f, 0.045f);
        leaf(f, ART_PART, cx, 0.86f, 35, 0.30f, 0.14f);
        leaf(f, ART_PART, cx, 0.74f, 150, 0.30f, 0.14f);
        paint(o, w, h, f, SPRING, NULL, 0.25f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, cx, 0.50f, 0.075f, 0.10f);
        paint(o, w, h, f, BUD, NULL, 0.25f);
        break;
    case 1:                                     /* summer: the sun */
        for (int i = 0; i < 12; i++) petal(f, ART_PART, cx, cy, i * 30.0f, 0.36f, 0.09f);
        art_ellipse(f, ART_PART, cx, cy, 0.17f, 0.17f);
        paint(o, w, h, f, SUN, NULL, 0.25f);
        break;
    case 2: {                                   /* autumn: a falling leaf, its veins */
        float x0 = cx - 0.22f, y0 = cy + 0.30f, c = cosf(55 * PI_F / 180), s = -sinf(55 * PI_F / 180);
        band(f, ART_PART, x0 - c * 0.12f, y0 - s * 0.12f, x0, y0, 0.035f);   /* its stalk */
        leaf(f, ART_PART, x0, y0, 55, 0.66f, 0.40f);
        band(f, ART_MARK, x0, y0, x0 + c * 0.58f, y0 + s * 0.58f, 0.022f);
        for (int i = 1; i <= 3; i++) {
            float t = 0.15f * i, px = x0 + c * t, py = y0 + s * t, cc = cosf(100 * PI_F / 180), ss = -sinf(100 * PI_F / 180),
                  c2 = cosf(10 * PI_F / 180), s2 = -sinf(10 * PI_F / 180), k = 0.13f - 0.02f * i;
            band(f, ART_MARK, px, py, px + cc * k, py + ss * k, 0.016f);
            band(f, ART_MARK, px, py, px + c2 * k, py + s2 * k, 0.016f);
        }
        paint(o, w, h, f, AUTUMN, VEIN, 0.25f);
        break;
    }
    default:                                    /* winter: a snowflake */
        for (int i = 0; i < 3; i++) {
            float a = (90 + 60 * i) * PI_F / 180, c = cosf(a) * 0.30f, s = sinf(a) * 0.30f;
            band(f, ART_PART, cx - c, cy + s, cx + c, cy - s, 0.05f);
        }
        for (int i = 0; i < 6; i++) {
            float a = (90 + 60 * i) * PI_F / 180, px = cx + cosf(a) * 0.2f, py = cy - sinf(a) * 0.2f;
            band(f, ART_PART, px, py, px + cosf(a + 0.8f) * 0.09f, py - sinf(a + 0.8f) * 0.09f, 0.035f);
            band(f, ART_PART, px, py, px + cosf(a - 0.8f) * 0.09f, py - sinf(a - 0.8f) * 0.09f, 0.035f);
        }
        paint(o, w, h, f, SNOW, NULL, 0.15f);
        break;
    }
    glyph(o, w, h, '1' + k, 0.2f, 0.2f, 0.3f, U_BLUE);
}

/* a face's picture (no tile under it), w x h, clear around */
static void face_pic(int face, int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    if (face < MJ_BAMBOO) face_dots(o, w, h, face - MJ_DOTS + 1);
    else if (face < MJ_CHARS) face_bamboo(o, w, h, face - MJ_BAMBOO + 1);
    else if (face < MJ_WINDS) {
        glyph(o, w, h, NUMERAL[face - MJ_CHARS], 0.5f, 0.36f, 0.50f, U_INK);
        glyph(o, w, h, WAN, 0.5f, 0.92f, 0.56f, U_RED);
    } else if (face < MJ_DRAGONS) glyph(o, w, h, WIND[face - MJ_WINDS], 0.5f, 0.65f, 0.74f, U_INK);
    else if (face == MJ_DRAGONS) glyph(o, w, h, ZHONG, 0.5f, 0.65f, 0.80f, U_RED);
    else if (face == MJ_DRAGONS + 1) glyph(o, w, h, FA, 0.5f, 0.65f, 0.76f, U_GREEN);
    else if (face == MJ_DRAGONS + 2) {          /* the white dragon: a blue frame */
        static const float in[4][4] = {{0.15f, 0.18f, 0.85f, 1.12f}, {0.21f, 0.24f, 0.79f, 1.06f},
                                       {0.26f, 0.29f, 0.74f, 1.01f}, {0.29f, 0.32f, 0.71f, 0.98f}};
        for (int i = 0; i < 4; i++) {
            art_fig *f = art_fig_new();
            art_rect(f, ART_PART, in[i][0], in[i][1], in[i][2], in[i][3]);
            paint(o, w, h, f, i & 1 ? IVORY : BLUE, NULL, 0);
        }
    } else if (face < MJ_SEASONS) face_flower(o, w, h, face - MJ_FLOWERS);
    else face_season(o, w, h, face - MJ_SEASONS);
}

/* ---- the tile: an ivory face, a thin ivory layer, the green back; a soft shadow ---------------------------------- */
/* coverage of a rounded rectangle (x0, y0, w, h, radius r) at pixel centre (px, py) */
static float rrect(float px, float py, float x0, float y0, float w, float h, float r)
{
    float dx = fmaxf(fabsf(px - (x0 + w / 2)) - (w / 2 - r), 0), dy = fmaxf(fabsf(py - (y0 + h / 2)) - (h / 2 - r), 0);
    return clampf(r - hypotf(dx, dy) + 0.5f, 0, 1);
}

static void over(uint8_t *p, const float c[3], float a)
{
    float da = p[3] / 255.0f, oa = a + da * (1 - a);
    if (oa <= 0) return;
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf((c[i] * a + p[i] * da * (1 - a)) / oa + 0.5f, 0, 255);
    p[3] = (uint8_t)(oa * 255 + 0.5f);
}

/* the tile's body (fw x fh face, d thick: (fw + d) x (fh + d)) */
static void tile_pic(int fw, int fh, int d, uint8_t *o)
{
    static const float BACK[3] = {52, 124, 80}, LAYER[3] = {222, 212, 186}, EDGE[3] = {150, 138, 112};
    int w = fw + d, h = fh + d;
    float r = 0.12f * fw;
    memset(o, 0, (size_t)w * h * 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            uint8_t *p = o + 4 * (y * w + x);
            float a = rrect(px, py, (float)d, (float)d, (float)fw, (float)fh, r);   /* the green back */
            if (a > 0) {
                float k = 0.85f + 0.15f * (1 - (px + py) / (w + h));
                const float c[3] = {BACK[0] * k, BACK[1] * k, BACK[2] * k};
                over(p, c, a);
            }
            a = rrect(px, py, 0.5f * d, 0.5f * d, (float)fw, (float)fh, r);         /* the ivory between */
            if (a > 0) over(p, LAYER, a);
            a = rrect(px, py, 0, 0, (float)fw, (float)fh, r);                       /* the face */
            if (a > 0) {
                float in = rrect(px, py, 1.2f, 1.2f, fw - 2.4f, fh - 2.4f, r - 1.2f);   /* its rim */
                float lit = 1 + 0.035f * (1 - 2 * (px / fw + py / fh) / 2);
                float c[3];
                for (int i = 0; i < 3; i++) c[i] = (IVORY[i] * lit) * in + EDGE[i] * (1 - in);
                over(p, c, a);
            }
        }
}

/* the soft shadow a tile casts (its face's size, blurred over b pixels) */
static void shadow_pic(int w, int h, float b, uint8_t *o)
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

/* ---- the state --------------------------------------------------------------------------------------------------- */
static mjgame G;
static int sel = -1;
static int order[MJ_MAX];                       /* the drawing order: layer by layer, back to front */
static uint8_t freeish[MJ_MAX];                 /* the free tiles (worked out once a frame) */
static struct { int on, a, b; uint8_t fa, fb; Uint32 t0; } anim;
static struct { int on, a, b, shuffle; Uint32 t0; } hint;
static box last_box, barbox;
static int fw, fh, th, made_key = -1, made_bar = -1;
static float ox, oy;                            /* where the layout's (0, 0) on layer 0 is drawn */
static SDL_Texture *t_tile, *t_shadow, *t_face[MJ_FACES], *t_small, *t_disc, *t_disc_on, *t_icon[2];

static void layout(box b);

static void sort_places(const mjpos *pos, int n, int *ord)
{
    for (int i = 0; i < n; i++) ord[i] = i;
    for (int i = 1; i < n; i++) {               /* insertion sort: layer, then towards the lower right */
        int k = ord[i], j = i - 1;
        mjpos p = pos[k];
        while (j >= 0) {
            mjpos q = pos[ord[j]];
            int before = q.z < p.z || (q.z == p.z && (q.x + q.y < p.x + p.y || (q.x + q.y == p.x + p.y && q.x <= p.x)));
            if (before) break;
            ord[j + 1] = ord[j];
            j--;
        }
        ord[j + 1] = k;
    }
}

static void calm(void)
{
    sel = -1;
    anim.on = 0;
    hint.on = 0;
}

static void start(int variant)
{
    int v = variant < 0 || variant >= MJ_VARIANTS ? 0 : variant;
    for (int t = 0; t < 8 && !mj_deal(&G, v, ui_random); t++) {}
    sort_places(G.pos, G.n, order);
    calm();
    if (made_key >= 0) layout(last_box);        /* another layout: another size of tiles */
}

static int status(void)
{
    int st = mj_status(&G);
    return st == MJ_WON ? GS_WIN0 : st == MJ_LOST ? GS_WIN1 : GS_PLAY;
}

static int turn(void) { return 0; }
static int busy(void) { return anim.on; }

static int undo(void)
{
    calm();
    return mj_undo(&G);
}

static int script(const char *moves)
{
    calm();
    return mj_script(&G, moves, ui_random);
}

static void take(int a, int b)
{
    anim.a = a;
    anim.b = b;
    anim.fa = G.face[a];
    anim.fb = G.face[b];
    if (!mj_take(&G, a, b)) { ui_sound(SND_DENY); return; }
    anim.on = 1;
    anim.t0 = ui_ms();
    sel = -1;
    hint.on = 0;
    ui_sound(SND_CAPTURE);
    ui_animate();
}

static void shuffle(void)
{
    if (!mj_shuffle(&G, ui_random)) { ui_sound(SND_DENY); return; }
    calm();
    ui_sound(SND_DICE);
    ui_animate();
}

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static void place_xy(int i, float *x, float *y)  /* a tile's face, its top left corner */
{
    mjpos p = G.pos[i];
    *x = ox + p.x * (fw / 2.0f) - p.z * th;
    *y = oy + p.y * (fh / 2.0f) - p.z * th;
}

/* the box the layout takes for tiles fw x fh, d thick: from (x0, y0) (relative to the layout's origin) */
static void extent(const mjpos *pos, int n, int w, int h, int d, float *x0, float *y0, float *x1, float *y1)
{
    *x0 = *y0 = 1e9f;
    *x1 = *y1 = -1e9f;
    for (int i = 0; i < n; i++) {
        float x = pos[i].x * (w / 2.0f) - pos[i].z * d, y = pos[i].y * (h / 2.0f) - pos[i].z * d;
        *x0 = fminf(*x0, x);
        *y0 = fminf(*y0, y);
        *x1 = fmaxf(*x1, x + w + d);
        *y1 = fmaxf(*y1, y + h + d);
    }
}

/* the largest tiles (even sizes) for which the layout fits in W x H */
static void fit(const mjpos *pos, int n, float W, float H, int *w, int *h, int *d)
{
    for (*w = 400; *w > 6; *w -= 2) {
        *h = 2 * (int)lroundf(ASPECT * *w / 2);
        *d = (int)fmaxf(2, lroundf(THICK * *w));
        float x0, y0, x1, y1;
        extent(pos, n, *w, *h, *d, &x0, &y0, &x1, &y1);
        if (x1 - x0 <= W && y1 - y0 <= H) return;
    }
}

static void layout(box b)
{
    last_box = b;
    fit(G.pos, G.n, 0.99f * b.w, 0.99f * b.h, &fw, &fh, &th);
    float x0, y0, x1, y1;
    extent(G.pos, G.n, fw, fh, th, &x0, &y0, &x1, &y1);
    ox = floorf(b.x + (b.w - (x1 - x0)) / 2 - x0);
    oy = floorf(b.y + (b.h - (y1 - y0)) / 2 - y0);
    int key = fw * 1000 + fh;
    if (made_key == key) return;
    made_key = key;
    int w = fw + th, h = fh + th;
    ui_free(&t_tile);
    tile_pic(fw, fh, th, ui_buf(w, h));
    t_tile = ui_tex(w, h, ui_buf(w, h));
    int sw = fw + 2 * th, sh = fh + 2 * th;
    ui_free(&t_shadow);
    shadow_pic(sw, sh, 1.1f * th, ui_buf(sw, sh));
    t_shadow = ui_tex(sw, sh, ui_buf(sw, sh));
    for (int f = 0; f < MJ_FACES; f++) {
        ui_free(&t_face[f]);
        face_pic(f, fw, fh, ui_buf(fw, fh));
        t_face[f] = ui_tex(fw, fh, ui_buf(fw, fh));
    }
}

/* ---- the buttons' pictures: a hint (a lamp) and a shuffle (two arrows crossing) ---------------------------------- */
static void icon_pic(int which, int size, uint8_t *o)
{
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    if (!which) {                               /* the hint: a lamp, lit */
        art_ellipse(f, ART_PART, 0.5f, 0.42f, 0.19f, 0.19f);
        const float neck[] = {0.38f, 0.54f, 0.62f, 0.54f, 0.585f, 0.68f, 0.415f, 0.68f};
        art_poly(f, ART_PART, neck, 4);
        art_rect(f, ART_PART, 0.415f, 0.70f, 0.585f, 0.74f);
        art_rect(f, ART_PART, 0.44f, 0.76f, 0.56f, 0.80f);
        for (int i = 0; i < 5; i++) {           /* its rays */
            float a = (90 + (i - 2) * 40) * PI_F / 180, c = cosf(a), s = -sinf(a);
            band(f, ART_PART, 0.5f + c * 0.26f, 0.42f + s * 0.26f, 0.5f + c * 0.34f, 0.42f + s * 0.34f, 0.045f);
        }
    } else {                                    /* the shuffle: two arrows crossing */
        band(f, ART_PART, 0.18f, 0.34f, 0.40f, 0.34f, 0.075f);
        band(f, ART_PART, 0.38f, 0.34f, 0.62f, 0.66f, 0.075f);
        band(f, ART_PART, 0.60f, 0.66f, 0.70f, 0.66f, 0.075f);
        band(f, ART_PART, 0.18f, 0.66f, 0.40f, 0.66f, 0.075f);
        band(f, ART_PART, 0.38f, 0.66f, 0.62f, 0.34f, 0.075f);
        band(f, ART_PART, 0.60f, 0.34f, 0.70f, 0.34f, 0.075f);
        const float a1[] = {0.68f, 0.22f, 0.84f, 0.34f, 0.68f, 0.46f}, a2[] = {0.68f, 0.54f, 0.84f, 0.66f, 0.68f, 0.78f};
        art_poly(f, ART_PART, a1, 3);
        art_poly(f, ART_PART, a2, 3);
    }
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

/* a tile standing, seen from the front, size x size: the token (the red dragon), the bar's count */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = 2 * (int)(size * 0.30f), h = 2 * (int)lroundf(ASPECT * w / 2), d = (int)fmaxf(1, lroundf(THICK * w));
    int bw = w + d, bh = h + d;
    uint8_t *body = malloc((size_t)bw * bh * 4), *face = malloc((size_t)w * h * 4);
    if (body && face) {
        tile_pic(w, h, d, body);
        face_pic(MJ_DRAGONS, w, h, face);
        art_blend(body, bw, bh, face, w, h, 0, 0);
        memset(rgba, 0, (size_t)size * size * 4);
        art_blend(rgba, size, size, body, bw, bh, (size - bw) / 2, (size - bh) / 2);
    }
    free(body);
    free(face);
}

static void bar_textures(void)
{
    int key = (int)ui.unit;
    if (made_bar == key) return;
    made_bar = key;
    static const uint8_t house[3] = {54, 72, 64}, gold[3] = {150, 118, 46};
    int d = (int)(1.7f * ui.unit), ic = (int)(1.05f * ui.unit), s = (int)(0.6f * ui.unit);
    ui_free(&t_disc);
    art_disc(d, house, 1, 0, ui_buf(d, d));
    t_disc = ui_tex(d, d, ui_buf(d, d));
    ui_free(&t_disc_on);
    art_disc(d, gold, 1, 0, ui_buf(d, d));
    t_disc_on = ui_tex(d, d, ui_buf(d, d));
    for (int i = 0; i < 2; i++) {
        ui_free(&t_icon[i]);
        icon_pic(i, ic, ui_buf(ic, ic));
        t_icon[i] = ui_tex(ic, ic, ui_buf(ic, ic));
        SDL_SetTextureColorMod(t_icon[i], 252, 244, 226);
    }
    ui_free(&t_small);
    token(0, s, ui_buf(s, s));
    t_small = ui_tex(s, s, ui_buf(s, s));
}

static void button(float x, float y, float r, int icon, int on)
{
    bar_textures();
    ui_put_c(on ? t_disc_on : t_disc, x, y, 2 * r, 2 * r);
    ui_put_c(t_icon[icon], x, y, 1.3f * r, 1.3f * r);
}

/* the big shuffle button in the middle of the board, when no pair is left */
static int big_shuffle(float *x, float *y, float *r)
{
    *x = last_box.x + last_box.w / 2;
    *y = last_box.y + last_box.h / 2;
    *r = 0.85f * ui.unit;
    return mj_status(&G) == MJ_STUCK && !anim.on;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static float ease(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

static void tile_at(float x, float y, int face, Uint8 k, Uint8 alpha)
{
    SDL_SetTextureColorMod(t_tile, k, k, (Uint8)(k * 0.97f));
    SDL_SetTextureColorMod(t_face[face], k, k, k);
    SDL_SetTextureAlphaMod(t_tile, alpha);
    SDL_SetTextureAlphaMod(t_face[face], alpha);
    ui_put(t_tile, x, y, (float)(fw + th), (float)(fh + th));
    ui_put(t_face[face], x, y, (float)fw, (float)fh);
    SDL_SetTextureAlphaMod(t_tile, 255);
    SDL_SetTextureAlphaMod(t_face[face], 255);
}

static void draw(void)
{
    float x, y;
    uint8_t on[MJ_MAX];
    for (int i = 0; i < G.n; i++) on[i] = G.face[i] != MJ_NONE;
    for (int i = 0; i < G.n; i++) freeish[i] = (uint8_t)mj_free(&G, i);
    Uint32 now = ui_ms();
    if (anim.on && now - anim.t0 >= TAKE_MS) anim.on = 0;
    if (hint.on && now - hint.t0 >= HINT_MS) hint.on = 0;
    if (anim.on || hint.on) ui_animate();
    int blink = hint.on && (now - hint.t0) / 300 % 2 == 0;
    for (int k = 0; k < G.n;) {                 /* a layer at a time: its shadows on the layer under it, then it */
        int z = G.pos[order[k]].z, e = k;
        while (e < G.n && G.pos[order[e]].z == z) e++;
        for (int j = k; j < e; j++) {
            int i = order[j];
            if (!on[i]) continue;
            place_xy(i, &x, &y);
            ui_put(t_shadow, x + 0.2f * th, y + 0.5f * th, (float)(fw + 2 * th), (float)(fh + 2 * th));
        }
        for (int j = k; j < e; j++) {
            int i = order[j];
            if (!on[i]) continue;
            place_xy(i, &x, &y);
            tile_at(x, y, G.face[i], freeish[i] ? 255 : 196, 255);
            if (i == sel) ui_mark_select(x, y, (float)fw, (float)fh);
            if (blink && !hint.shuffle && (i == hint.a || i == hint.b)) ui_mark_select(x, y, (float)fw, (float)fh);
        }
        k = e;
    }
    if (anim.on) {                              /* the pair taken: rising, fading away */
        float t = (now - anim.t0) / (float)TAKE_MS, e = ease(t);
        for (int k = 0; k < 2; k++) {
            place_xy(k ? anim.b : anim.a, &x, &y);
            tile_at(x - e * 0.3f * fw, y - e * 0.6f * fh, k ? anim.fb : anim.fa, 255, (Uint8)(255 * (1 - e)));
        }
    }
    float r;
    if (big_shuffle(&x, &y, &r)) button(x, y, r, 1, !hint.on || blink);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* the tile a press at (x, y) means: the top tile there if it is free, else the free tile nearest the finger if it is
 * just beside one; *hit: a tile is under the finger */
static int tile_under(float x, float y, int *hit)
{
    *hit = 0;
    for (int j = G.n - 1; j >= 0; j--) {        /* the top tile there */
        int i = order[j];
        if (G.face[i] == MJ_NONE) continue;
        float tx, ty;
        place_xy(i, &tx, &ty);
        if (x < tx || y < ty || x >= tx + fw + th || y >= ty + fh + th) continue;
        *hit = 1;
        if (mj_free(&G, i)) return i;
        break;
    }
    int best = -1;
    float bd = 1e9f, mx = 0.3f * fw, my = 0.22f * fh;
    for (int i = 0; i < G.n; i++) {             /* beside a free tile */
        if (!mj_free(&G, i)) continue;
        float tx, ty;
        place_xy(i, &tx, &ty);
        if (x < tx - mx || y < ty - my || x >= tx + fw + th + mx || y >= ty + fh + th + my) continue;
        float d = hypotf(x - (tx + fw / 2.0f), y - (ty + fh / 2.0f));
        if (d < bd) bd = d, best = i;
    }
    return best;
}

static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    float bx, by, br;
    if (big_shuffle(&bx, &by, &br) && hypotf(x - bx, y - by) < 1.1f * br) { shuffle(); return; }
    anim.on = 0;
    int hit, t = tile_under(x, y, &hit);
    if (t < 0) {
        if (hit) ui_sound(SND_DENY);            /* a tile that is not free */
        sel = -1;
        ui_animate();
        return;
    }
    hint.on = 0;
    if (t == sel) sel = -1;                     /* the tile picked: let it go */
    else if (sel >= 0 && mj_match(G.face[sel], G.face[t])) take(sel, t);
    else sel = t;                               /* the first of a pair, or another one instead */
    ui_animate();
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the bar: the tiles left, the hint, and when stuck the shuffle ---------------------------------------------- */
static void bar_button(int which, float *x, float *y, float *r)   /* 0 the hint (at the right end), 1 the shuffle */
{
    *r = 0.42f * ui.unit;
    *x = barbox.x + barbox.w - *r - 0.05f * ui.unit - which * (2 * *r + 0.25f * ui.unit);
    *y = barbox.y + barbox.h / 2;
}

/* the shuffle in the bar: when no pair is left, and the bar has room for it beside the count (a narrow bar, in
 * landscape: only the big one in the middle of the board) */
static int bar_shuffle(void)
{
    float x, y, r;
    bar_button(1, &x, &y, &r);
    return mj_status(&G) == MJ_STUCK && x - r >= barbox.x + 1.75f * ui.unit;
}

static void bar(int player, box b)
{
    (void)player;
    bar_textures();
    barbox = b;
    ui_bar_count(b, t_small, G.tiles);
    if (status() != GS_PLAY) return;
    float x, y, r;
    int blink = hint.on && (ui_ms() - hint.t0) / 300 % 2 == 0;
    bar_button(0, &x, &y, &r);
    button(x, y, r, 0, 0);
    if (bar_shuffle()) {
        bar_button(1, &x, &y, &r);
        button(x, y, r, 1, !hint.on || blink);
    }
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (!barbox.w || status() != GS_PLAY) return 0;
    float bx, by, r;
    bar_button(0, &bx, &by, &r);
    if (hypotf(x - bx, y - by) < 1.1f * r) {    /* the hint: a pair, or (none left) the shuffle */
        anim.on = 0;
        hint.on = 1;
        hint.t0 = ui_ms();
        hint.shuffle = !mj_hint(&G, &hint.a, &hint.b);
        sel = -1;
        ui_sound(hint.shuffle ? SND_DENY : SND_TAP);
        ui_animate();
        return 1;
    }
    bar_button(1, &bx, &by, &r);
    if (bar_shuffle() && hypotf(x - bx, y - by) < 1.1f * r) {
        shuffle();
        return 1;
    }
    return 0;
}

/* ---- the menu's and the picker's cards --------------------------------------------------------------------------- */
static uint32_t thumb_rng;
static int thumb_rnd(int n)
{
    thumb_rng = thumb_rng * 1664525u + 1013904223u;
    return n > 0 ? (int)((thumb_rng >> 8) % (uint32_t)n) : 0;
}

/* the layout dealt (the same deal each time); the menu's, the Pyramid under way */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static mjgame T;
    int v = variant < 0 ? 1 : variant;        /* the menu's: the Pyramid (larger tiles on a small card) */
    thumb_rng = 2026u + (uint32_t)v;
    if (!mj_deal(&T, v, thumb_rnd)) return;
    if (variant < 0) mj_script(&T, "=7", thumb_rnd);
    int w, h, d;
    fit(T.pos, T.n, 0.96f * size, 0.96f * size, &w, &h, &d);
    float x0, y0, x1, y1;
    extent(T.pos, T.n, w, h, d, &x0, &y0, &x1, &y1);
    float px0 = (size - (x1 - x0)) / 2 - x0, py0 = (size - (y1 - y0)) / 2 - y0;
    int bw = w + d, bh = h + d, sw = w + 2 * d, sh = h + 2 * d;
    uint8_t *body = malloc((size_t)bw * bh * 4), *sh_pic = malloc((size_t)sw * sh * 4);
    uint8_t *faces = malloc((size_t)MJ_FACES * w * h * 4);
    if (!body || !sh_pic || !faces) { free(body); free(sh_pic); free(faces); return; }
    tile_pic(w, h, d, body);
    shadow_pic(sw, sh, 1.1f * d, sh_pic);
    for (int f = 0; f < MJ_FACES; f++) face_pic(f, w, h, faces + (size_t)f * w * h * 4);
    int ord[MJ_MAX], n = T.n;
    sort_places(T.pos, n, ord);
    for (int j = 0; j < n; j++) {
        int i = ord[j];
        if (T.face[i] == MJ_NONE) continue;
        int x = (int)lroundf(px0 + T.pos[i].x * (w / 2.0f) - T.pos[i].z * d);
        int y = (int)lroundf(py0 + T.pos[i].y * (h / 2.0f) - T.pos[i].z * d);
        art_blend(rgba, size, size, sh_pic, sw, sh, x + d / 5, y + d / 2);
        art_blend(rgba, size, size, body, bw, bh, x, y);
        art_blend(rgba, size, size, faces + (size_t)T.face[i] * w * h * 4, w, h, x, y);
    }
    free(body);
    free(sh_pic);
    free(faces);
}

static const game mahjong_game = {
    .id = "mahjong", .order = 310, .variants = MJ_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(mahjong_game)
