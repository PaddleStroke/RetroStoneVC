/*
 * The hanafuda deck's pictures (koikoi_deck.h): each card painted by code with the art kit's figures (art.h), layer
 * by layer (one colour a layer, its ink outline), in card units: x 0..1 across, y 0..1.6 down. A month's plant is
 * drawn once for its four cards; each card adds its subject. The card kit draws the cards as a deck of its own
 * (hana_deck: cards_deck_add): its texture cache, shadows, gold light, flips and slides.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/koikoi_deck.h"
#include "shell.h"

#define PI_F 3.14159265f
#define H_ 1.6f                                 /* the card's height, in widths */

typedef const uint8_t rgb[3];
static rgb PAPER = {247, 240, 222}, INK = {40, 32, 30}, RED = {204, 48, 40}, CRIMSON = {168, 30, 44},
           PINK = {246, 196, 204}, PINKD = {228, 124, 146}, WHITE = {252, 250, 244}, GREEN_D = {30, 82, 58},
           GREEN = {62, 128, 72}, GREEN_L = {136, 182, 88}, OLIVE = {124, 138, 52}, BROWN = {116, 76, 46},
           BROWN_D = {70, 46, 34}, TAN = {206, 166, 104}, YELLOW = {238, 194, 64}, GOLD = {214, 168, 66},
           BLUE = {40, 74, 156}, BLUE_L = {118, 168, 214}, NAVY = {30, 42, 86}, PURPLE = {116, 74, 166},
           PURPLE_L = {170, 134, 210}, ORANGE = {230, 120, 42}, SKY = {212, 70, 50}, SLATE = {52, 60, 80},
           NIGHT = {30, 30, 38}, HILL = {44, 40, 46}, MOON = {250, 242, 214}, GREY = {150, 150, 158},
           CREAM = {240, 230, 204};

/* ---- the painter: a canvas, a transform, a pen (one figure of one colour), its bounding box ------------------- */
static struct { uint8_t *o; int w, h; float s; } C;   /* s: pixels per card width */
static art_fig *F;
static art_style ST;
static int nshape, detail;
static float BX0, BY0, BX1, BY1;
static float TA = 1, TB, TC, TD = 1, TE, TF;   /* local (x, y) to card: (TA x + TC y + TE, TB x + TD y + TF) */

static void ident(void) { TA = TD = 1, TB = TC = TE = TF = 0; }

/* local coordinates: their origin at (cx, cy), s card widths to a local unit, turned deg clockwise, mirrored */
static void place(float cx, float cy, float s, float deg, int mirror)
{
    float a = deg * PI_F / 180, m = mirror ? -1.0f : 1.0f;
    TA = m * s * cosf(a), TC = -s * sinf(a), TB = m * s * sinf(a), TD = s * cosf(a), TE = cx, TF = cy;
}

static void tf(float x, float y, float *X, float *Y)
{
    *X = TA * x + TC * y + TE;
    *Y = TB * x + TD * y + TF;
}

static void grow(float x, float y)
{
    BX0 = fminf(BX0, x), BY0 = fminf(BY0, y), BX1 = fmaxf(BX1, x), BY1 = fmaxf(BY1, y);
}

static void flush(void)
{
    if (!F) return;
    if (nshape) {
        float pad = ST.line + 2 / C.s;
        int x0 = (int)floorf((BX0 - pad) * C.s), y0 = (int)floorf((BY0 - pad) * C.s);
        int x1 = (int)ceilf((BX1 + pad) * C.s), y1 = (int)ceilf((BY1 + pad) * C.s);
        x0 = x0 < 0 ? 0 : x0, y0 = y0 < 0 ? 0 : y0, x1 = x1 > C.w ? C.w : x1, y1 = y1 > C.h ? C.h : y1;
        if (x1 > x0 && y1 > y0) {
            uint8_t *b = malloc((size_t)(x1 - x0) * (y1 - y0) * 4);
            if (b) {
                art_render(F, &ST, x1 - x0, y1 - y0, C.s, (float)-x0, (float)-y0, b);
                art_blend(C.o, C.w, C.h, b, x1 - x0, y1 - y0, x0, y0);
                free(b);
            }
        }
    }
    art_fig_free(F);
    F = NULL;
}

static void restart(void)
{
    flush();
    F = art_fig_new();
    nshape = 0;
    BX0 = BY0 = 1e9f, BX1 = BY1 = -1e9f;
}

/* a new layer: its fill, its outline (NULL: none; width in card widths, at least a pixel's worth), its light */
static void pen(rgb fill, const uint8_t *edge, float line, float light)
{
    restart();
    memset(&ST, 0, sizeof ST);
    for (int i = 0; i < 3; i++) {
        ST.fill[i] = fill[i];
        ST.outer[i] = ST.inner[i] = ST.mark[i] = edge ? edge[i] : fill[i];
    }
    ST.light = light;
    ST.line = edge ? fmaxf(line, 0.8f / C.s) : 0;
}
static void inked(rgb fill) { pen(fill, INK, 0.007f, 0.12f); }
static void flat(rgb fill) { pen(fill, NULL, 0, 0); }

static void room(void)                          /* a figure holds 32 shapes: carry on in a new one */
{
    if (nshape >= 30) {
        art_style keep = ST;
        restart();
        ST = keep;
    }
    nshape++;
}

static void poly(const float *p, int n)
{
    float q[96];
    if (n > 48) n = 48;
    room();
    for (int i = 0; i < n; i++) {
        tf(p[2 * i], p[2 * i + 1], &q[2 * i], &q[2 * i + 1]);
        grow(q[2 * i], q[2 * i + 1]);
    }
    art_poly(F, ART_PART, q, n);
}
#define POLY(...) poly((const float[]){__VA_ARGS__}, (int)(sizeof((const float[]){__VA_ARGS__}) / sizeof(float) / 2))

static void ell(float cx, float cy, float rx, float ry)
{
    if (fabsf(TB) < 1e-6f && fabsf(TC) < 1e-6f) {
        float X, Y, ax = fabsf(TA) * rx, ay = fabsf(TD) * ry;
        tf(cx, cy, &X, &Y);
        room();
        grow(X - ax, Y - ay);
        grow(X + ax, Y + ay);
        art_ellipse(F, ART_PART, X, Y, ax, ay);
        return;
    }
    float p[72];
    for (int i = 0; i < 36; i++) {
        float a = i * 2 * PI_F / 36;
        p[2 * i] = cx + rx * cosf(a), p[2 * i + 1] = cy + ry * sinf(a);
    }
    poly(p, 36);
}
static void circ(float cx, float cy, float r) { ell(cx, cy, r, r); }

/* an ellipse turned deg degrees (a petal, a leaf seen round) */
static void oval(float cx, float cy, float rx, float ry, float deg)
{
    float p[64], a = deg * PI_F / 180, c = cosf(a), s = sinf(a);
    for (int i = 0; i < 32; i++) {
        float t = i * 2 * PI_F / 32, x = rx * cosf(t), y = ry * sinf(t);
        p[2 * i] = cx + x * c - y * s, p[2 * i + 1] = cy + x * s + y * c;
    }
    poly(p, 32);
}

/* a leaf: pointed at both ends, from (x0, y0) to (x1, y1), wid at its widest, bent to one side (bend: 0 straight) */
static void lens(float x0, float y0, float x1, float y1, float wid, float bend)
{
    float p[64], dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy) + 1e-6f, nx = -dy / l, ny = dx / l;
    int k = 16;
    for (int i = 0; i < k; i++) {               /* one side, then the other back */
        float t = (float)i / (k - 1), w = sinf(t * PI_F) * wid / 2, b = bend * sinf(t * PI_F) * l;
        p[2 * i] = x0 + dx * t + nx * (w + b), p[2 * i + 1] = y0 + dy * t + ny * (w + b);
        p[2 * (2 * k - 1 - i)] = x0 + dx * t + nx * (b - w), p[2 * (2 * k - 1 - i) + 1] = y0 + dy * t + ny * (b - w);
    }
    poly(p, 2 * k);
}

/* a bezier's points (n of them) */
static void bez(float *o, int n, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
{
    for (int i = 0; i < n; i++) {
        float t = (float)i / (n - 1), u = 1 - t, a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
        o[2 * i] = a * x0 + b * x1 + c * x2 + d * x3;
        o[2 * i + 1] = a * y0 + b * y1 + c * y2 + d * y3;
    }
}

/* a line through n points (at most 24), w0 wide at its start to w1 at its end */
static void stroke(const float *p, int n, float w0, float w1)
{
    float q[96];
    if (n > 24) n = 24;
    for (int i = 0; i < n; i++) {
        int a = i > 0 ? i - 1 : 0, b = i < n - 1 ? i + 1 : n - 1;
        float dx = p[2 * b] - p[2 * a], dy = p[2 * b + 1] - p[2 * a + 1], l = sqrtf(dx * dx + dy * dy) + 1e-6f;
        float w = (w0 + (w1 - w0) * i / (n - 1)) / 2, nx = -dy / l * w, ny = dx / l * w;
        q[2 * i] = p[2 * i] + nx, q[2 * i + 1] = p[2 * i + 1] + ny;
        q[2 * (2 * n - 1 - i)] = p[2 * i] - nx, q[2 * (2 * n - 1 - i) + 1] = p[2 * i + 1] - ny;
    }
    poly(q, 2 * n);
}
#define STROKE(w0, w1, ...) stroke((const float[]){__VA_ARGS__}, \
                                   (int)(sizeof((const float[]){__VA_ARGS__}) / sizeof(float) / 2), w0, w1)

static void curve(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float w0, float w1)
{
    float p[40];
    bez(p, 20, x0, y0, x1, y1, x2, y2, x3, y3);
    stroke(p, 20, w0, w1);
}

/* a band of a ring between radii r0 and r1, from deg0 to deg1 (counter-clockwise, 0 east) */
static void band(float cx, float cy, float r0, float r1, float deg0, float deg1)
{
    float p[96];
    int k = 24;
    for (int i = 0; i < k; i++) {
        float a = (deg0 + (deg1 - deg0) * i / (k - 1)) * PI_F / 180;
        p[2 * i] = cx + r1 * cosf(a), p[2 * i + 1] = cy - r1 * sinf(a);
        p[2 * (2 * k - 1 - i)] = cx + r0 * cosf(a), p[2 * (2 * k - 1 - i) + 1] = cy - r0 * sinf(a);
    }
    poly(p, 2 * k);
}

static uint32_t seed;
static float rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return (float)(seed >> 8) / 16777216.0f;
}

/* ---- shared subjects --------------------------------------------------------------------------------------------- */
/* the whole face one colour (a sky), or a band of it from y0 down to y1 (wavy edge: amp) */
static void sky(rgb c, float y0, float y1, float amp, float phase)
{
    flat(c);
    float p[96];
    int k = 22;
    for (int i = 0; i < k; i++) {
        float x = -0.05f + 1.1f * i / (k - 1);
        p[2 * i] = x, p[2 * i + 1] = y1 + amp * sinf(x * 7 + phase);
    }
    p[2 * k] = 1.05f, p[2 * k + 1] = y0;
    p[2 * k + 2] = -0.05f, p[2 * k + 3] = y0;
    poly(p, k + 2);
}

/* a mist band: a long rounded cloud (the classic pictures' bands of colour) */
static void mist(float x0, float x1, float y, float h)
{
    float r = h / 2, p[80];
    int k = 10, n = 0;
    for (int i = 0; i <= k; i++) {              /* the right end, round */
        float a = -PI_F / 2 + PI_F * i / k;
        p[n++] = x1 + r * cosf(a), p[n++] = y + r * sinf(a);
    }
    for (int i = 0; i <= k; i++) {
        float a = PI_F / 2 + PI_F * i / k;
        p[n++] = x0 + r * cosf(a), p[n++] = y + r * sinf(a);
    }
    poly(p, n / 2);
}

/* a five-petal blossom */
static void blossom(float cx, float cy, float r, float turn)
{
    for (int i = 0; i < 5; i++) {
        float a = turn + i * 2 * PI_F / 5;
        circ(cx + 0.55f * r * cosf(a), cy + 0.55f * r * sinf(a), 0.48f * r);
    }
}

/* the ribbon (tanzaku): a long strip of length l turned deg, a tie at its top; the poetry ribbon's brush strokes */
static void ribbon(float cx, float cy, float l, float deg, int kind, int month)
{
    rgb *c = kind == HR_BLUE ? &BLUE : &RED;
    float w = 0.15f, t = -l / 2, b = l / 2;
    place(cx, cy, 1, deg, 0);
    inked(*c);
    ST.light = 0.18f;
    POLY(-w / 2, t + 0.05f, w / 2, t + 0.05f, w / 2, b - 0.02f, -w / 2, b + 0.03f);
    flush();
    inked(*c);                                  /* the tie: a fold over the top */
    POLY(-w / 2 - 0.01f, t, w / 2 + 0.01f, t, w / 2 + 0.01f, t + 0.07f, -w / 2 - 0.01f, t + 0.07f);
    flush();
    flat(kind == HR_BLUE ? BLUE_L : (rgb){236, 120, 96});   /* a light line inside each edge */
    POLY(-w / 2 + 0.018f, t + 0.08f, -w / 2 + 0.03f, t + 0.08f, -w / 2 + 0.03f, b - 0.03f, -w / 2 + 0.018f, b - 0.02f);
    flush();
    if (kind == HR_POETRY && detail) {          /* writing-like strokes down the middle, never letters */
        flat(INK);
        seed = 977u + 131u * (unsigned)month;
        float y = t + 0.14f;
        while (y < b - 0.1f) {
            float h = 0.06f + 0.07f * rnd(), x = -0.015f + 0.03f * rnd(), dx = (rnd() - 0.5f) * 0.05f;
            int kind2 = (int)(rnd() * 4);
            if (kind2 == 0) {                                              /* two dots */
                oval(x - 0.02f, y + 0.02f, 0.02f, 0.013f, 30);
                oval(x + 0.025f, y + 0.035f, 0.018f, 0.012f, -20);
                h = 0.04f;
            } else if (kind2 == 1) curve(x - 0.035f, y, x + 0.03f, y + h * 0.2f, x - 0.03f, y + h * 0.6f, x + dx,
                                         y + h, 0.04f, 0.008f);            /* a falling stroke */
            else {                                                         /* a cross stroke and a hook */
                curve(x - 0.045f, y + 0.012f, x - 0.01f, y - 0.008f, x + 0.02f, y + 0.02f, x + 0.045f, y + 0.004f,
                      0.032f, 0.01f);
                curve(x, y + 0.012f, x + 0.012f, y + h * 0.4f, x - 0.015f, y + h * 0.8f, x - 0.035f, y + h, 0.034f,
                      0.008f);
                if (kind2 == 3) STROKE(0.026f, 0.008f, x + 0.012f, y + h * 0.45f, x + 0.04f, y + h * 0.6f);
            }
            y += h + 0.03f;
        }
        flush();
    }
    ident();
}

/* the sun or the moon: a disc in a thin ring */
static void orb(float cx, float cy, float r, rgb c)
{
    inked(c);
    ST.light = 0.1f;
    circ(cx, cy, r);
    flush();
    flat((rgb){255, 255, 255});
    band(cx, cy, r * 0.86f, r * 0.9f, 100, 170);
    flush();
}

/* ---- January: the pine ------------------------------------------------------------------------------------------- */
static void pine_dome(float cx, float cy, float r)
{
    float p[64];
    int k = 20, n = 0;
    for (int i = 0; i < k; i++) {               /* the top: half an ellipse; the foot: a slight sag */
        float a = PI_F * i / (k - 1);
        p[n++] = cx - r * cosf(a), p[n++] = cy - 0.66f * r * sinf(a);
    }
    for (int i = 1; i < 8; i++) {
        float x = cx + r - 2 * r * i / 8.0f;
        p[n++] = x, p[n++] = cy + 0.1f * r * sinf(PI_F * i / 8.0f);
    }
    poly(p, n / 2);
}

static void pine_needles(float cx, float cy, float r)
{
    for (int i = 1; i < 8; i++) {
        float a = PI_F * i / 8.0f, x = cx - 0.93f * r * cosf(a), y = cy - 0.6f * r * sinf(a);
        STROKE(0.012f, 0.004f, cx, cy, x, y);
    }
}

static void pine(int k)
{
    static const float D[2][4][3] = {
        {{0.50f, 0.70f, 0.30f}, {0.84f, 0.98f, 0.20f}, {0.20f, 0.72f, 0.18f}, {0.30f, 1.20f, 0.16f}},
        {{0.18f, 0.56f, 0.18f}, {0.40f, 0.86f, 0.17f}, {0.15f, 1.06f, 0.15f}, {0.86f, 1.22f, 0.13f}},
    };
    int v = k == 0;                             /* the crane's card: the pine on the left, the sun and the bird */
    if (k == 2) { flat(RED); POLY(-0.05f, 1.36f, 0.3f, 1.3f, 0.7f, 1.34f, 1.05f, 1.28f, 1.05f, 1.65f, -0.05f, 1.65f); }
    if (k == 3) { flat((rgb){226, 92, 70}); mist(-0.2f, 0.45f, 0.3f, 0.1f); mist(0.55f, 1.2f, 1.42f, 0.1f); }
    if (k == 0) {
        sky((rgb){236, 176, 120}, -0.05f, 0.55f, 0.03f, 1);   /* a dawn sky behind the sun */
        orb(0.62f, 0.42f, 0.25f, RED);
    }
    inked(BROWN);                               /* the trunk and its branches */
    if (v) {
        curve(0.12f, 1.65f, 0.02f, 1.3f, 0.3f, 1.05f, 0.30f, 0.75f, 0.11f, 0.05f);
        curve(0.24f, 1.0f, 0.32f, 0.92f, 0.34f, 0.9f, 0.42f, 0.86f, 0.05f, 0.02f);
    } else {
        curve(0.22f, 1.65f, 0.08f, 1.25f, 0.46f, 1.14f, 0.50f, 0.78f, 0.13f, 0.06f);
        curve(0.38f, 1.12f, 0.55f, 1.06f, 0.70f, 1.10f, 0.86f, 1.0f, 0.055f, 0.025f);
        curve(0.46f, 0.92f, 0.38f, 0.82f, 0.30f, 0.78f, 0.22f, 0.72f, 0.045f, 0.02f);
    }
    flush();
    inked(GREEN_D);
    for (int i = 0; i < 4; i++) pine_dome(D[v][i][0], D[v][i][1], D[v][i][2]);
    flush();
    if (detail) {
        flat(GREEN_L);
        for (int i = 0; i < 4; i++) pine_needles(D[v][i][0], D[v][i][1] - 0.01f, D[v][i][2]);
        flush();
    }
}

/* the crane, standing, facing right (local: its body at the origin, a card width a unit) */
static void crane(void)
{
    inked(INK);                                 /* the legs */
    STROKE(0.016f, 0.012f, -0.02f, 0.06f, -0.01f, 0.2f, -0.03f, 0.34f);
    STROKE(0.016f, 0.012f, 0.05f, 0.06f, 0.07f, 0.2f, 0.09f, 0.33f);
    STROKE(0.012f, 0.006f, -0.03f, 0.34f, -0.1f, 0.35f);
    STROKE(0.012f, 0.006f, 0.09f, 0.33f, 0.17f, 0.34f);
    flush();
    inked(INK);                                 /* the black plumes over the tail */
    POLY(-0.08f, -0.04f, -0.22f, -0.02f, -0.34f, 0.06f, -0.27f, 0.07f, -0.33f, 0.13f, -0.2f, 0.12f, -0.07f, 0.06f);
    flush();
    inked(WHITE);
    ell(0.0f, 0.0f, 0.18f, 0.105f);             /* the body */
    curve(0.11f, -0.02f, 0.2f, -0.12f, 0.12f, -0.26f, 0.2f, -0.37f, 0.07f, 0.045f);   /* the neck */
    flush();
    inked(INK);
    curve(0.155f, -0.2f, 0.135f, -0.27f, 0.16f, -0.32f, 0.2f, -0.36f, 0.035f, 0.04f);
    flush();
    inked(WHITE);
    ell(0.215f, -0.38f, 0.042f, 0.036f);        /* the head */
    POLY(-0.13f, -0.03f, -0.02f, -0.1f, 0.12f, -0.08f, 0.04f, 0.0f, -0.1f, 0.03f);   /* the wing */
    flush();
    inked(YELLOW);                              /* the bill */
    lens(0.245f, -0.385f, 0.36f, -0.35f, 0.022f, 0);
    flush();
    inked(RED);                                 /* the red crown */
    ell(0.21f, -0.405f, 0.022f, 0.014f);
    flush();
    flat(INK);
    circ(0.225f, -0.382f, 0.007f);
    if (detail) {                               /* feathers on the wing */
        STROKE(0.006f, 0.002f, -0.08f, -0.03f, 0.04f, -0.06f);
        STROKE(0.006f, 0.002f, -0.06f, 0.0f, 0.05f, -0.03f);
    }
    flush();
}

static void january(int k)
{
    pine(k);
    if (k == 0) {
        place(0.6f, 1.08f, 1.25f, 0, 0);
        crane();
        ident();
    }
    if (k == 1) ribbon(0.56f, 1.02f, 0.82f, 12, HR_POETRY, 0);
}

/* ---- February: the plum ------------------------------------------------------------------------------------------ */
static const float PLUM_FLOWERS[][2] = {{0.62f, 1.22f}, {0.80f, 0.78f}, {0.18f, 0.42f}, {0.70f, 0.28f},
                                        {0.34f, 1.10f}, {0.56f, 0.6f}, {0.28f, 0.84f}, {0.88f, 1.06f},
                                        {0.48f, 0.38f}};

static void plum(int k)
{
    if (k == 2) { flat((rgb){230, 104, 84}); mist(-0.2f, 0.5f, 0.16f, 0.12f); mist(0.4f, 1.3f, 1.46f, 0.12f); }
    if (k == 0) sky((rgb){238, 216, 168}, 1.3f, 1.7f, 0.02f, 0);
    inked((rgb){62, 40, 38});                   /* the old branch, angular */
    STROKE(0.09f, 0.03f, 0.08f, 1.65f, 0.28f, 1.32f, 0.22f, 1.06f, 0.46f, 0.82f, 0.42f, 0.56f, 0.68f, 0.3f);
    STROKE(0.03f, 0.012f, 0.27f, 1.3f, 0.45f, 1.24f, 0.62f, 1.22f);
    STROKE(0.03f, 0.012f, 0.46f, 0.82f, 0.62f, 0.8f, 0.80f, 0.78f);
    STROKE(0.026f, 0.01f, 0.43f, 0.58f, 0.3f, 0.48f, 0.18f, 0.42f);
    STROKE(0.02f, 0.008f, 0.6f, 1.2f, 0.75f, 1.12f, 0.88f, 1.06f);
    STROKE(0.02f, 0.008f, 0.25f, 1.02f, 0.26f, 0.92f, 0.28f, 0.84f);
    flush();
    int nf = k == 0 ? 7 : 9;
    inked(CRIMSON);
    ST.light = 0.2f;
    for (int i = 0; i < nf; i++) blossom(PLUM_FLOWERS[i][0], PLUM_FLOWERS[i][1], 0.085f, 0.3f * i);
    flush();
    flat(WHITE);
    for (int i = 0; i < nf; i++) circ(PLUM_FLOWERS[i][0], PLUM_FLOWERS[i][1], 0.022f);
    flush();
    flat(YELLOW);
    for (int i = 0; i < nf && detail; i++)
        for (int j = 0; j < 5; j++) {
            float a = j * 2 * PI_F / 5 + 0.6f;
            circ(PLUM_FLOWERS[i][0] + 0.03f * cosf(a), PLUM_FLOWERS[i][1] + 0.03f * sinf(a), 0.008f);
        }
    flush();
    inked(RED);                                 /* buds */
    circ(0.6f, 0.86f, 0.022f), circ(0.12f, 0.46f, 0.02f), circ(0.74f, 0.25f, 0.02f), circ(0.92f, 1.0f, 0.02f);
    flush();
}

/* the bush warbler, perched, facing left */
static void warbler(void)
{
    inked(OLIVE);
    lens(0.06f, 0.0f, 0.26f, 0.1f, 0.06f, 0);   /* the tail */
    oval(0.0f, 0.0f, 0.13f, 0.08f, 12);         /* the body */
    circ(-0.11f, -0.05f, 0.06f);                /* the head */
    flush();
    flat((rgb){200, 200, 120});
    oval(-0.03f, 0.035f, 0.08f, 0.035f, 10);    /* the light breast */
    flush();
    inked((rgb){86, 98, 36});
    lens(-0.04f, -0.03f, 0.15f, 0.04f, 0.07f, 0.05f);   /* the wing */
    flush();
    inked(INK);
    POLY(-0.165f, -0.07f, -0.215f, -0.05f, -0.16f, -0.04f);   /* the bill */
    STROKE(0.012f, 0.01f, -0.02f, 0.07f, -0.03f, 0.12f);   /* the feet */
    STROKE(0.012f, 0.01f, 0.03f, 0.07f, 0.03f, 0.12f);
    flush();
    flat(WHITE);
    circ(-0.125f, -0.06f, 0.016f);
    flush();
    flat(INK);
    circ(-0.128f, -0.06f, 0.009f);
    flush();
}

static void february(int k)
{
    plum(k);
    if (k == 0) {
        place(0.52f, 1.12f, 1.35f, -4, 0);
        warbler();
        ident();
    }
    if (k == 1) ribbon(0.5f, 0.98f, 0.82f, -10, HR_POETRY, 1);
}

/* ---- March: the cherry ------------------------------------------------------------------------------------------- */
static const float CHERRY_FLOWERS[][2] = {
    {0.14f, 0.2f}, {0.32f, 0.16f}, {0.52f, 0.22f}, {0.74f, 0.16f}, {0.9f, 0.3f}, {0.22f, 0.38f}, {0.42f, 0.36f},
    {0.64f, 0.38f}, {0.84f, 0.5f}, {0.1f, 0.56f}, {0.32f, 0.56f}, {0.54f, 0.54f}, {0.72f, 0.66f}, {0.2f, 0.74f},
    {0.44f, 0.72f}, {0.9f, 0.76f}, {0.6f, 0.86f}, {0.12f, 0.92f}};

static void cherry(int k)
{
    int low = k == 0 ? 13 : 18;                 /* the curtain's card: blossoms over the top half only */
    if (k != 0) {
        flat((rgb){222, 88, 80});               /* bands of mist */
        mist(-0.2f, 0.42f, 1.08f, 0.11f);
        mist(0.5f, 1.2f, 1.3f, 0.11f);
        if (k >= 2) mist(-0.1f, 0.6f, 1.48f, 0.1f);
        flush();
    }
    inked(BROWN_D);
    curve(-0.05f, 0.1f, 0.3f, 0.3f, 0.6f, 0.1f, 1.05f, 0.42f, 0.06f, 0.03f);
    curve(0.3f, 0.24f, 0.36f, 0.4f, 0.24f, 0.6f, 0.3f, 0.82f, 0.04f, 0.015f);
    flush();
    pen(PINK, PINKD, 0.008f, 0.15f);
    for (int i = 0; i < low; i++) blossom(CHERRY_FLOWERS[i][0], CHERRY_FLOWERS[i][1], 0.1f, 0.5f * i);
    flush();
    flat(PINKD);
    for (int i = 0; i < low; i++) circ(CHERRY_FLOWERS[i][0], CHERRY_FLOWERS[i][1], 0.022f);
    flush();
    if (detail) {
        flat(CRIMSON);
        for (int i = 0; i < low; i++) circ(CHERRY_FLOWERS[i][0], CHERRY_FLOWERS[i][1], 0.01f);
        flush();
    }
}

/* the festival curtain across the lower half: red and white panels, crests, a scalloped hem */
static void curtain(void)
{
    float top = 0.86f, bot = 1.44f;
    inked(RED);
    ST.light = 0.15f;
    float p[96];
    int n = 0;
    p[n++] = -0.05f, p[n++] = top;
    p[n++] = 1.05f, p[n++] = top;
    for (int i = 0; i <= 20; i++) {             /* the hem: four swags */
        float x = 1.05f - 1.1f * i / 20, s = fabsf(sinf((x + 0.05f) / 1.1f * 4 * PI_F));
        p[n++] = x, p[n++] = bot - 0.06f + 0.07f * s;
    }
    poly(p, n / 2);
    flush();
    flat(WHITE);                                /* two white panels */
    for (int i = 0; i < 2; i++) {
        float x = 0.22f + 0.4f * i;
        POLY(x, top + 0.02f, x + 0.16f, top + 0.02f, x + 0.16f, bot - 0.02f + 0.04f * (i ? -1 : 1), x,
             bot - 0.02f + 0.03f * (i ? -1 : 1));
    }
    flush();
    inked(GOLD);                                /* the rope along the top */
    curve(-0.05f, top, 0.3f, top + 0.03f, 0.7f, top - 0.03f, 1.05f, top, 0.035f, 0.035f);
    flush();
    inked(WHITE);                               /* crests: a light disc with a blossom */
    circ(0.12f, 1.12f, 0.075f), circ(0.5f, 1.12f, 0.075f), circ(0.88f, 1.12f, 0.075f);
    flush();
    pen(PINKD, CRIMSON, 0.006f, 0);
    blossom(0.12f, 1.12f, 0.06f, 0.3f), blossom(0.5f, 1.12f, 0.06f, 0.3f), blossom(0.88f, 1.12f, 0.06f, 0.3f);
    flush();
    flat(CRIMSON);                              /* the folds */
    for (int i = 0; i < 5 && detail; i++) {
        float x = 0.06f + 0.22f * i + 0.02f;
        STROKE(0.008f, 0.002f, x, 1.22f, x + 0.01f, 1.38f);
    }
    flush();
}

static void march(int k)
{
    cherry(k);
    if (k == 0) curtain();
    if (k == 1) ribbon(0.5f, 1.08f, 0.8f, 8, HR_POETRY, 2);
}

/* ---- April: the wisteria ----------------------------------------------------------------------------------------- */
static void wisteria(int k)
{
    static const float CL[4][3] = {{0.16f, 0.2f, 0.62f}, {0.4f, 0.22f, 0.82f}, {0.64f, 0.18f, 0.7f},
                                   {0.86f, 0.22f, 0.56f}};
    int nc = k == 0 ? 4 : 4;
    float scale = k == 0 ? 0.72f : 1.0f;
    if (k == 3) { flat((rgb){206, 92, 80}); mist(-0.2f, 0.5f, 1.4f, 0.12f); mist(0.4f, 1.2f, 1.52f, 0.1f); }
    inked(BROWN_D);                             /* the twisting vine along the top */
    curve(-0.05f, 0.16f, 0.3f, 0.02f, 0.6f, 0.3f, 1.05f, 0.1f, 0.05f, 0.035f);
    flush();
    inked((rgb){34, 48, 40});                   /* dark leaves along the vine */
    for (int i = 0; i < 6; i++) {
        float x = 0.05f + 0.18f * i, y = 0.14f + 0.04f * sinf(i * 2.1f);
        lens(x, y, x + 0.1f, y + 0.12f, 0.05f, 0.1f);
        lens(x, y, x - 0.04f, y + 0.14f, 0.045f, -0.1f);
    }
    flush();
    inked(PURPLE);
    ST.light = 0.25f;
    for (int c = 0; c < nc; c++) {              /* the hanging clusters: florets smaller down to the tip */
        float x = CL[c][0], y = CL[c][1], l = CL[c][2] * scale;
        int m = 11;
        for (int i = 0; i < m; i++) {
            float t = (float)i / (m - 1), r = 0.05f * (1 - 0.6f * t);
            circ(x + 0.035f * sinf(i * 1.7f) + 0.03f * t, y + 0.06f + t * l, r);
        }
    }
    flush();
    if (detail) {
        flat(PURPLE_L);
        for (int c = 0; c < nc; c++) {
            float x = CL[c][0], y = CL[c][1], l = CL[c][2] * scale;
            for (int i = 0; i < 10; i += 1) {
                float t = (float)i / 10, r = 0.05f * (1 - 0.6f * t);
                circ(x + 0.035f * sinf(i * 1.7f) + 0.03f * t - 0.3f * r, y + 0.06f + t * l - 0.3f * r, 0.35f * r);
            }
        }
        flush();
    }
}

/* a wing: from the shoulder (0, 0) to its tip (tx, ty), long flight feathers fanned behind it (local coordinates) */
static void wing(float tx, float ty, float chord, int feathers)
{
    float l = sqrtf(tx * tx + ty * ty), nx = ty / l, ny = -tx / l;   /* the normal, to the trailing side */
    for (int i = feathers - 1; i >= 0; i--) {   /* the long flight feathers, fanned from the tip back */
        float f = (float)i / (feathers - 1), s0 = 0.35f + 0.1f * f, s1 = 1.0f - 0.3f * f;
        float d = chord * (0.15f + 0.95f * f);
        lens(tx * s0 + nx * chord * 0.35f * f, ty * s0 + ny * chord * 0.35f * f, tx * s1 + nx * d, ty * s1 + ny * d,
             chord * 0.42f, 0.04f);
    }
    lens(0, 0, tx * 0.6f + nx * chord * 0.3f, ty * 0.6f + ny * chord * 0.3f, chord * 1.1f, -0.06f);   /* the coverts */
}

/* the cuckoo, flying left */
static void cuckoo(void)
{
    float A = TA, B = TB, Cc = TC, D = TD, E = TE, Fv = TF;
    inked((rgb){62, 66, 82});                   /* the far wing */
    TE = A * 0.06f + Cc * -0.03f + E, TF = B * 0.06f + D * -0.03f + Fv;
    wing(0.2f, -0.28f, 0.1f, 5);
    TE = E, TF = Fv;
    flush();
    inked((rgb){84, 90, 108});
    POLY(0.12f, -0.02f, 0.38f, 0.02f, 0.4f, 0.07f, 0.12f, 0.04f);   /* the tail */
    lens(-0.2f, -0.01f, 0.18f, 0.02f, 0.12f, -0.02f);   /* the body */
    circ(-0.19f, -0.025f, 0.05f);               /* the head */
    flush();
    flat(CREAM);                                /* the barred belly */
    lens(-0.14f, 0.025f, 0.12f, 0.035f, 0.05f, 0.02f);
    flush();
    if (detail) {
        flat(INK);
        for (int i = 0; i < 5; i++) STROKE(0.008f, 0.008f, -0.1f + 0.05f * i, 0.012f, -0.09f + 0.05f * i, 0.045f);
        flush();
    }
    inked((rgb){104, 112, 132});                /* the near wing, swept up and back */
    TE = A * -0.02f + Cc * -0.03f + E, TF = B * -0.02f + D * -0.03f + Fv;
    wing(0.3f, -0.34f, 0.13f, 6);
    TE = E, TF = Fv;
    flush();
    if (detail) {
        flat((rgb){60, 64, 80});
        TE = A * -0.02f + Cc * -0.03f + E, TF = B * -0.02f + D * -0.03f + Fv;
        for (int i = 2; i < 6; i++) {
            float t = i / 6.0f;
            STROKE(0.006f, 0.002f, 0.3f * t - 0.02f, -0.34f * t + 0.01f, 0.3f * t + 0.07f, -0.34f * t + 0.06f);
        }
        TE = E, TF = Fv;
        flush();
    }
    inked(YELLOW);
    POLY(-0.235f, -0.035f, -0.3f, -0.015f, -0.235f, -0.005f);
    circ(-0.2f, -0.035f, 0.016f);
    flush();
    flat(INK);
    circ(-0.2f, -0.035f, 0.008f);
    flush();
}

/* a crescent moon: the lit edge of a disc of radius r, its hollow turned to deg (0: the left) */
static void crescent(float cx, float cy, float r, float deg, rgb c)
{
    float p[96], al = 125 * PI_F / 180, d = 0.42f * r, tx = r * cosf(al), ty = r * sinf(al);
    float r2 = sqrtf((tx + d) * (tx + d) + ty * ty), be = atan2f(ty, tx + d);
    int k = 24, n = 0;
    for (int i = 0; i < k; i++) {               /* the outer edge, through the lit side (east) */
        float a = -al + 2 * al * i / (k - 1);
        p[n++] = r * cosf(a), p[n++] = r * sinf(a);
    }
    for (int i = 1; i < k - 1; i++) {           /* the inner edge back */
        float a = be - 2 * be * i / (k - 1);
        p[n++] = -d + r2 * cosf(a), p[n++] = r2 * sinf(a);
    }
    place(cx, cy, 1, deg, 0);
    inked(c);
    ST.light = 0.15f;
    poly(p, n / 2);
    flush();
    ident();
}

static void april(int k)
{
    if (k == 0) sky((rgb){62, 58, 86}, 1.0f, 1.7f, 0.03f, 2);   /* a night over the lower half */
    wisteria(k);
    if (k == 0) {
        crescent(0.76f, 1.32f, 0.13f, -35, YELLOW);
        place(0.44f, 1.2f, 1.25f, -6, 0);
        cuckoo();
        ident();
    }
    if (k == 1) ribbon(0.5f, 1.04f, 0.8f, -8, HR_RED, 3);
}

/* ---- May: the iris ----------------------------------------------------------------------------------------------- */
static void iris_flower(float cx, float cy, float s)
{
    place(cx, cy, s, 0, 0);
    inked(PURPLE);
    ST.light = 0.2f;
    lens(0.0f, 0.0f, -0.17f, 0.12f, 0.1f, 0.12f);   /* the three falls */
    lens(0.0f, 0.0f, 0.17f, 0.12f, 0.1f, -0.12f);
    lens(0.0f, 0.0f, 0.0f, 0.2f, 0.1f, 0);
    flush();
    inked((rgb){134, 96, 196});                 /* the standards */
    lens(0.0f, 0.02f, -0.07f, -0.16f, 0.07f, -0.1f);
    lens(0.0f, 0.02f, 0.07f, -0.16f, 0.07f, 0.1f);
    flush();
    flat(YELLOW);                               /* the bright marks */
    lens(-0.03f, 0.03f, -0.11f, 0.08f, 0.025f, 0);
    lens(0.03f, 0.03f, 0.11f, 0.08f, 0.025f, 0);
    flush();
    flat(WHITE);
    lens(0.0f, 0.04f, 0.0f, 0.13f, 0.02f, 0);
    flush();
    ident();
}

static void iris(int k)
{
    if (k == 2 || k == 3) {                     /* water */
        flat((rgb){150, 190, 216});
        POLY(-0.05f, 1.3f, 0.4f, 1.27f, 0.8f, 1.32f, 1.05f, 1.28f, 1.05f, 1.65f, -0.05f, 1.65f);
        flush();
        flat(WHITE);
        for (int i = 0; i < 3; i++) STROKE(0.01f, 0.004f, 0.1f + 0.3f * i, 1.42f + 0.05f * (i % 2), 0.28f + 0.3f * i,
                                          1.4f + 0.05f * (i % 2));
        flush();
    }
    inked(GREEN);                               /* sword leaves */
    static const float L[7][4] = {{0.12f, 0.62f, 0.08f, 0.2f}, {0.26f, 0.5f, 0.2f, -0.12f}, {0.36f, 0.7f, 0.34f, 0.1f},
                                  {0.56f, 0.56f, 0.5f, 0.15f}, {0.7f, 0.66f, 0.64f, -0.1f},
                                  {0.84f, 0.5f, 0.84f, 0.12f}, {0.95f, 0.74f, 0.96f, -0.1f}};
    for (int i = 0; i < 7; i++) {
        float bx = L[i][2], tip = L[i][1] + (k == 0 ? 0.2f : 0);
        lens(bx, 1.66f, L[i][0], tip, 0.075f, L[i][3] * 0.3f);
    }
    flush();
    flat(GREEN_D);
    for (int i = 0; i < 7 && detail; i++) {
        float bx = L[i][2], tip = L[i][1] + (k == 0 ? 0.2f : 0);
        STROKE(0.006f, 0.002f, bx, 1.6f, (bx + L[i][0]) / 2 + L[i][3] * 0.06f, (1.6f + tip) / 2, L[i][0],
               tip + 0.05f);
    }
    flush();
    inked(GREEN);
    if (k == 0) STROKE(0.016f, 0.012f, 0.62f, 1.62f, 0.66f, 0.62f);
    else STROKE(0.016f, 0.012f, 0.46f, 1.62f, 0.48f, 0.5f), STROKE(0.016f, 0.012f, 0.2f, 1.62f, 0.28f, 0.88f);
    flush();
    if (k == 0) iris_flower(0.66f, 0.56f, 1.2f), iris_flower(0.24f, 0.42f, 1.0f);
    else iris_flower(0.48f, 0.42f, 1.35f), iris_flower(0.28f, 0.82f, 1.0f), iris_flower(0.8f, 0.26f, 0.9f);
}

/* the plank bridge in a zigzag over the water */
static void bridge(void)
{
    static const float S[2][4] = {{-0.1f, 0.96f, 0.6f, 1.14f}, {0.42f, 1.26f, 1.1f, 1.08f}};
    flat((rgb){150, 190, 216});
    POLY(-0.05f, 1.24f, 1.05f, 1.2f, 1.05f, 1.65f, -0.05f, 1.65f);
    flush();
    for (int s = 0; s < 2; s++) {
        float x0 = S[s][0], y0 = S[s][1], x1 = S[s][2], y1 = S[s][3], t = 0.13f;
        inked(BROWN_D);                         /* the posts */
        for (int i = 1; i < 4; i++) {
            float x = x0 + (x1 - x0) * i / 4, y = y0 + (y1 - y0) * i / 4;
            POLY(x - 0.02f, y + t * 0.6f, x + 0.02f, y + t * 0.6f, x + 0.02f, y + t + 0.14f, x - 0.02f, y + t + 0.14f);
        }
        flush();
        inked(TAN);                             /* the planks: their side, their top */
        POLY(x0, y0 + t, x1, y1 + t, x1, y1 + t + 0.035f, x0, y0 + t + 0.035f);
        POLY(x0 + 0.04f, y0, x1 + 0.04f, y1, x1, y1 + t, x0, y0 + t);
        flush();
        flat(BROWN);
        for (int i = 1; i < 6; i++) {
            float u = (float)i / 6, x = x0 + (x1 - x0) * u, y = y0 + (y1 - y0) * u;
            STROKE(0.006f, 0.006f, x + 0.04f, y, x, y + t);
        }
        flush();
    }
}

static void may(int k)
{
    iris(k);
    if (k == 0) bridge();
    if (k == 1) ribbon(0.5f, 1.08f, 0.8f, 10, HR_RED, 4);
}

/* ---- June: the peony --------------------------------------------------------------------------------------------- */
static void peony_flower(float cx, float cy, float r)
{
    inked((rgb){214, 70, 104});
    ST.light = 0.2f;
    for (int i = 0; i < 9; i++) {
        float a = i * 2 * PI_F / 9;
        oval(cx + 0.62f * r * cosf(a), cy + 0.5f * r * sinf(a), 0.42f * r, 0.34f * r, a * 180 / PI_F);
    }
    flush();
    pen((rgb){240, 130, 156}, (rgb){178, 44, 80}, 0.006f, 0.2f);
    for (int i = 0; i < 6; i++) {
        float a = i * 2 * PI_F / 6 + 0.4f;
        oval(cx + 0.32f * r * cosf(a), cy - 0.05f * r + 0.26f * r * sinf(a), 0.3f * r, 0.24f * r, a * 180 / PI_F);
    }
    flush();
    flat(YELLOW);
    for (int i = 0; i < 7; i++) {
        float a = i * 2 * PI_F / 7;
        circ(cx + 0.12f * r * cosf(a), cy - 0.06f * r + 0.09f * r * sinf(a), 0.05f * r);
    }
    flush();
}

static void peony_leaves(float cx, float cy, float s, float deg)
{
    place(cx, cy, s, deg, 0);
    lens(0, 0, -0.06f, -0.22f, 0.1f, 0.08f);
    lens(0, 0, 0.16f, -0.14f, 0.1f, -0.08f);
    lens(0, 0, -0.2f, -0.06f, 0.09f, -0.08f);
    ident();
}

static void peony(int k)
{
    if (k >= 2) { flat((rgb){150, 182, 220}); mist(-0.2f, 0.4f, 0.24f, 0.1f); mist(0.6f, 1.3f, 1.5f, 0.1f); }
    inked(GREEN_D);
    peony_leaves(0.2f, 1.42f, 1.2f, -10), peony_leaves(0.8f, 1.5f, 1.1f, 20), peony_leaves(0.5f, 1.6f, 1.3f, 0);
    peony_leaves(0.86f, 0.98f, 0.9f, 40), peony_leaves(0.12f, 0.96f, 0.9f, -30);
    flush();
    if (detail) {
        flat(GREEN_L);
        STROKE(0.008f, 0.002f, 0.2f, 1.42f, 0.16f, 1.25f), STROKE(0.008f, 0.002f, 0.5f, 1.6f, 0.46f, 1.36f);
        STROKE(0.008f, 0.002f, 0.8f, 1.5f, 0.96f, 1.36f);
        flush();
    }
    if (k == 0) peony_flower(0.5f, 1.14f, 0.28f);
    else peony_flower(0.42f, 1.12f, 0.3f), peony_flower(0.72f, 0.62f, 0.22f);
}

/* a butterfly seen from above, its wings `c` with spots `d` */
static void butterfly(float cx, float cy, float s, float deg, rgb c, rgb d)
{
    place(cx, cy, s, deg, 0);
    inked(c);
    ST.light = 0.15f;
    oval(-0.1f, -0.06f, 0.11f, 0.08f, -30), oval(0.1f, -0.06f, 0.11f, 0.08f, 30);
    oval(-0.07f, 0.07f, 0.07f, 0.055f, 30), oval(0.07f, 0.07f, 0.07f, 0.055f, -30);
    flush();
    flat(d);
    circ(-0.12f, -0.07f, 0.03f), circ(0.12f, -0.07f, 0.03f), circ(-0.07f, 0.08f, 0.02f), circ(0.07f, 0.08f, 0.02f);
    flush();
    inked(INK);
    lens(0, -0.09f, 0, 0.12f, 0.03f, 0);
    STROKE(0.006f, 0.004f, -0.005f, -0.09f, -0.04f, -0.17f), STROKE(0.006f, 0.004f, 0.005f, -0.09f, 0.04f, -0.17f);
    flush();
    ident();
}

static void june(int k)
{
    peony(k);
    if (k == 0) {
        butterfly(0.3f, 0.42f, 1.25f, -18, BLUE, YELLOW);
        butterfly(0.7f, 0.66f, 1.0f, 22, YELLOW, RED);
    }
    if (k == 1) ribbon(0.5f, 0.86f, 0.8f, -10, HR_BLUE, 5);
}

/* ---- July: the bush clover --------------------------------------------------------------------------------------- */
static void clover(int k)
{
    /* sprays rising from one clump and arching over, left and right, like a fountain */
    static const float S[6][8] = {
        {0.48f, 1.66f, 0.42f, 1.0f, 0.1f, 0.62f, 0.02f, 0.94f},
        {0.54f, 1.66f, 0.6f, 0.92f, 0.94f, 0.46f, 1.02f, 0.8f},
        {0.5f, 1.66f, 0.42f, 0.62f, 0.6f, 0.16f, 0.82f, 0.24f},
        {0.52f, 1.66f, 0.5f, 0.74f, 0.24f, 0.18f, 0.08f, 0.32f},
        {0.56f, 1.66f, 0.7f, 1.22f, 0.94f, 1.06f, 1.02f, 1.3f},
        {0.46f, 1.66f, 0.3f, 1.3f, 0.06f, 1.16f, -0.02f, 1.36f}};
    static const uint8_t SPRAYS[2][6] = {{2, 3, 0, 1, 9, 9}, {2, 3, 0, 1, 4, 5}};
    const uint8_t *use = SPRAYS[k != 0];
    if (k == 3) { flat((rgb){222, 98, 74}); mist(-0.2f, 0.36f, 1.5f, 0.12f); mist(0.66f, 1.2f, 1.36f, 0.1f); }
    inked((rgb){110, 44, 38});
    for (int s = 0; s < 6 && use[s] < 6; s++) {
        const float *q = S[use[s]];
        curve(q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7], 0.024f, 0.008f);
    }
    flush();
    for (int pass = 0; pass < 2; pass++) {      /* leaves in threes along the sprays: dark ones, red ones */
        pen(pass ? (rgb){196, 58, 52} : (rgb){46, 42, 38}, INK, 0.006f, 0.1f);
        for (int s = 0; s < 6 && use[s] < 6; s++) {
            const float *q = S[use[s]];
            float p[40];
            bez(p, 20, q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7]);
            for (int j = 6 + 2 * pass; j < 19; j += 4) {
                float x = p[2 * j], y = p[2 * j + 1];
                float dx = p[2 * j + 2] - p[2 * j - 2], dy = p[2 * j + 3] - p[2 * j - 1];
                float a = atan2f(dy, dx), side = (j / 2) % 2 ? 1.0f : -1.0f, r = 0.042f * (1 - 0.35f * j / 19.0f);
                for (int l = -1; l <= 1; l++) {
                    float b = a + side * (1.1f + 0.55f * l);
                    oval(x + 1.1f * r * cosf(b), y + 1.1f * r * sinf(b), r, 0.58f * r, b * 180 / PI_F);
                }
            }
        }
        flush();
    }
    if (detail) {
        flat((rgb){206, 70, 150});              /* tiny flowers towards the tips */
        for (int s = 0; s < 6 && use[s] < 6; s++) {
            const float *q = S[use[s]];
            float p[40];
            bez(p, 20, q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7]);
            for (int j = 11; j < 20; j += 3) {
                lens(p[2 * j], p[2 * j + 1], p[2 * j] + 0.012f, p[2 * j + 1] + 0.035f, 0.016f, 0);
                lens(p[2 * j], p[2 * j + 1], p[2 * j] - 0.02f, p[2 * j + 1] + 0.03f, 0.014f, 0);
            }
        }
        flush();
    }
}

/* the boar, running left */
static void boar(void)
{
    inked((rgb){82, 56, 42});                   /* the far legs */
    STROKE(0.05f, 0.03f, -0.12f, 0.06f, -0.22f, 0.18f);
    STROKE(0.05f, 0.03f, 0.18f, 0.06f, 0.3f, 0.16f);
    flush();
    inked((rgb){110, 76, 52});
    ST.light = 0.2f;
    POLY(-0.3f, -0.06f, -0.2f, -0.17f, 0.0f, -0.2f, 0.2f, -0.17f, 0.32f, -0.08f, 0.32f, 0.04f, 0.2f, 0.11f,
         -0.1f, 0.12f, -0.26f, 0.08f);          /* the body */
    POLY(-0.26f, -0.1f, -0.44f, 0.0f, -0.46f, 0.06f, -0.4f, 0.09f, -0.24f, 0.08f);   /* the head and snout */
    flush();
    inked((rgb){70, 46, 34});                   /* the bristly ridge */
    POLY(-0.26f, -0.1f, -0.2f, -0.2f, -0.14f, -0.17f, -0.08f, -0.25f, -0.02f, -0.2f, 0.06f, -0.26f, 0.1f, -0.19f,
         0.18f, -0.22f, 0.22f, -0.15f, 0.3f, -0.12f, 0.2f, -0.13f, 0.0f, -0.15f, -0.18f, -0.12f);
    POLY(-0.24f, -0.1f, -0.2f, -0.2f, -0.16f, -0.1f);   /* the ear */
    flush();
    inked((rgb){110, 76, 52});                  /* the near legs */
    STROKE(0.055f, 0.032f, -0.18f, 0.06f, -0.3f, 0.12f, -0.36f, 0.2f);
    STROKE(0.055f, 0.032f, 0.22f, 0.04f, 0.3f, 0.14f, 0.4f, 0.18f);
    flush();
    inked((rgb){200, 150, 140});
    ell(-0.45f, 0.06f, 0.018f, 0.03f);          /* the snout's tip */
    flush();
    inked(WHITE);
    POLY(-0.4f, 0.06f, -0.37f, 0.0f, -0.36f, 0.06f);   /* the tusk */
    flush();
    flat(INK);
    circ(-0.31f, -0.04f, 0.012f);
    curve(0.31f, -0.06f, 0.36f, -0.1f, 0.4f, -0.04f, 0.36f, -0.02f, 0.012f, 0.004f);   /* the tail */
    flush();
}

static void july(int k)
{
    clover(k);
    if (k == 0) {
        place(0.57f, 1.22f, 1.04f, 0, 0);
        boar();
        ident();
    }
    if (k == 1) ribbon(0.48f, 1.0f, 0.82f, 10, HR_RED, 6);
}

/* ---- August: the pampas grass on its hill ------------------------------------------------------------------------ */
static void pampas(int k)
{
    float top = k == 0 ? 1.06f : k == 1 ? 1.12f : 0.92f;
    if (k <= 1) sky(k == 0 ? SKY : (rgb){232, 132, 60}, -0.05f, top + 0.1f, 0, 0);
    if (k == 3) { flat((rgb){214, 74, 58}); mist(-0.2f, 0.55f, 0.36f, 0.12f); mist(0.4f, 1.2f, 0.58f, 0.1f); }
    if (k == 0) orb(0.5f, 0.52f, 0.33f, MOON);
    inked(HILL);                                /* the hill */
    float p[52];
    bez(p, 24, -0.05f, top + 0.14f, 0.25f, top - 0.14f, 0.7f, top - 0.12f, 1.05f, top + 0.16f);
    p[48] = 1.05f, p[49] = 1.65f, p[50] = -0.05f, p[51] = 1.65f;
    poly(p, 26);
    flush();
    flat(CREAM);                                /* the grass: blades over the hill, plumes above it */
    for (int i = 0; i < 9; i++) {
        float x = 0.06f + 0.11f * i, y = top + 0.02f + 0.05f * fabsf(x - 0.5f);
        curve(x, y + 0.5f, x - 0.02f, y + 0.3f, x + 0.04f, y + 0.12f, x + 0.1f, y - 0.05f, 0.016f, 0.003f);
    }
    flush();
    if (detail) {
        flat((rgb){180, 170, 150});
        for (int i = 0; i < 12; i++) {
            float x = 0.04f + 0.085f * i, y = top + 0.22f + 0.14f * (i % 3);
            curve(x, y + 0.2f, x + 0.02f, y + 0.1f, x + 0.05f, y + 0.04f, x + 0.09f, y, 0.012f, 0.002f);
        }
        flush();
    }
    if (k >= 2) {
        flat(k == 2 ? (rgb){214, 74, 58} : CREAM);
        for (int i = 0; i < 4; i++) {
            float x = 0.18f + 0.22f * i;
            curve(x, top + 0.05f, x + 0.02f, top - 0.15f, x + 0.08f, top - 0.3f, x + 0.16f, top - 0.38f, 0.014f,
                  0.004f);
        }
        flush();
    }
}

/* a goose in flight, wings up */
static void goose(float cx, float cy, float s)
{
    place(cx, cy, s, -10, 0);
    lens(-0.1f, 0.0f, 0.12f, 0.02f, 0.05f, 0);
    POLY(-0.02f, 0.0f, 0.06f, -0.14f, 0.12f, -0.16f, 0.06f, -0.04f, 0.04f, 0.0f);
    POLY(0.0f, 0.0f, -0.02f, -0.12f, 0.02f, -0.12f, 0.04f, 0.0f);
    circ(-0.11f, -0.005f, 0.022f);
    ident();
}

static void august(int k)
{
    pampas(k);
    if (k == 1) {
        inked(INK);
        goose(0.26f, 0.32f, 1.3f), goose(0.52f, 0.52f, 1.15f), goose(0.76f, 0.74f, 1.0f);
        flush();
    }
}

/* ---- September: the chrysanthemum -------------------------------------------------------------------------------- */
static void mum(float cx, float cy, float r, rgb a, rgb b)
{
    pen(a, INK, 0.006f, 0.1f);
    for (int i = 0; i < 16; i++) {
        float t = i * 2 * PI_F / 16;
        lens(cx, cy, cx + r * cosf(t), cy + r * sinf(t) * 0.92f, 0.16f * r, 0);
    }
    flush();
    pen(b, INK, 0.006f, 0.1f);
    for (int i = 0; i < 12; i++) {
        float t = i * 2 * PI_F / 12 + 0.13f;
        lens(cx, cy, cx + 0.62f * r * cosf(t), cy + 0.62f * r * sinf(t) * 0.92f, 0.2f * r, 0);
    }
    flush();
    inked(YELLOW);
    circ(cx, cy, 0.16f * r);
    flush();
}

static void mum_leaf(float cx, float cy, float s, float deg)
{
    place(cx, cy, s, deg, 0);                   /* a lobed leaf: five pointed lobes from its stalk */
    lens(0, 0, 0, -0.2f, 0.09f, 0);
    lens(0, -0.03f, -0.12f, -0.14f, 0.08f, 0.05f), lens(0, -0.03f, 0.12f, -0.14f, 0.08f, -0.05f);
    lens(0, 0, -0.11f, -0.03f, 0.06f, 0.05f), lens(0, 0, 0.11f, -0.03f, 0.06f, -0.05f);
    ident();
}

static void chrysanthemum(int k)
{
    if (k >= 2) { flat((rgb){132, 160, 206}); mist(-0.2f, 0.5f, 0.26f, 0.1f); mist(0.5f, 1.2f, 1.06f, 0.1f); }
    inked(GREEN);
    STROKE(0.02f, 0.014f, 0.5f, 1.66f, 0.42f, 1.2f, 0.44f, 0.7f);
    STROKE(0.016f, 0.012f, 0.44f, 1.3f, 0.6f, 1.0f, 0.74f, 0.48f);
    flush();
    inked(GREEN_D);
    mum_leaf(0.3f, 1.46f, 1.1f, -40), mum_leaf(0.66f, 1.4f, 1.0f, 30), mum_leaf(0.5f, 1.02f, 0.9f, 20);
    mum_leaf(0.22f, 1.0f, 0.8f, -60);
    flush();
    if (k == 0) {
        mum(0.3f, 0.36f, 0.2f, YELLOW, (rgb){240, 222, 120});
        mum(0.72f, 0.4f, 0.17f, (rgb){222, 72, 60}, (rgb){240, 140, 120});
    } else {
        mum(0.44f, 0.62f, 0.24f, YELLOW, (rgb){240, 222, 120});
        mum(0.76f, 0.34f, 0.17f, (rgb){222, 72, 60}, (rgb){240, 140, 120});
        if (k != 1) mum(0.2f, 0.28f, 0.14f, WHITE, CREAM);
    }
}

/* the sake cup: red lacquer, a gold flower crest inside */
static void sake_cup(float cx, float cy, float s)
{
    place(cx, cy, s, 0, 0);
    inked(CRIMSON);
    POLY(-0.1f, 0.12f, 0.1f, 0.12f, 0.13f, 0.2f, -0.13f, 0.2f);   /* the foot */
    flush();
    inked(RED);
    ST.light = 0.3f;
    float p[60];
    int n = 0;
    for (int i = 0; i <= 14; i++) {             /* the bowl */
        float a = PI_F * i / 14;
        p[n++] = 0.36f * cosf(a), p[n++] = 0.15f * sinf(a);
    }
    poly(p, n / 2);
    flush();
    inked((rgb){176, 30, 34});
    ell(0, 0, 0.36f, 0.1f);                     /* the inside */
    flush();
    inked(GOLD);                                /* the crest */
    for (int i = 0; i < 8; i++) {
        float a = i * 2 * PI_F / 8;
        ell(0.07f * cosf(a), 0.02f * sinf(a), 0.05f, 0.022f);
    }
    flush();
    flat(YELLOW);
    ell(0, 0, 0.035f, 0.014f);
    flush();
    flat(WHITE);
    lens(-0.26f, 0.07f, -0.06f, 0.135f, 0.018f, 0.02f);   /* a gleam on the lacquer */
    flush();
    ident();
}

static void september(int k)
{
    chrysanthemum(k);
    if (k == 0) sake_cup(0.5f, 1.12f, 1.25f);
    if (k == 1) ribbon(0.52f, 1.06f, 0.8f, -12, HR_BLUE, 8);
}

/* ---- October: the maple ------------------------------------------------------------------------------------------ */
static void maple_leaf(float cx, float cy, float r, float deg)
{
    float p[64];
    int n = 0;
    for (int i = 0; i < 7; i++) {               /* seven lobes: a tip, a notch */
        float a = (deg - 90 + (i - 3) * 38) * PI_F / 180, b = a + 19 * PI_F / 180;
        float l = r * (i == 3 ? 1.0f : i == 2 || i == 4 ? 0.95f : i == 1 || i == 5 ? 0.78f : 0.55f);
        p[n++] = cx + l * cosf(a), p[n++] = cy + l * sinf(a);
        if (i < 6) p[n++] = cx + 0.4f * r * cosf(b), p[n++] = cy + 0.4f * r * sinf(b);
    }
    float a = (deg + 90) * PI_F / 180;
    p[n++] = cx + 0.12f * r * cosf(a - 0.6f), p[n++] = cy + 0.12f * r * sinf(a - 0.6f);
    poly(p, n / 2);
}

static void maple(int k)
{
    static const float LV[9][4] = {{0.2f, 0.22f, 0.15f, -20}, {0.46f, 0.18f, 0.13f, 10}, {0.74f, 0.26f, 0.15f, 30},
                                   {0.3f, 0.48f, 0.14f, -40}, {0.6f, 0.46f, 0.15f, 15}, {0.86f, 0.56f, 0.13f, 40},
                                   {0.16f, 0.74f, 0.13f, -25}, {0.44f, 0.78f, 0.14f, 5}, {0.72f, 0.86f, 0.13f, 25}};
    int nl = k == 0 ? 6 : 9;
    if (k >= 2) {
        flat((rgb){226, 120, 70});
        mist(-0.2f, 0.6f, 1.2f + 0.2f * (k == 3), 0.12f);
        mist(0.5f, 1.3f, 1.46f, 0.1f);
    }
    inked(BROWN_D);
    curve(1.05f, 0.05f, 0.6f, 0.2f, 0.4f, 0.5f, 0.1f, 0.85f, 0.045f, 0.015f);
    curve(0.6f, 0.26f, 0.7f, 0.5f, 0.7f, 0.7f, 0.72f, 0.86f, 0.025f, 0.01f);
    flush();
    for (int c = 0; c < 3; c++) {
        inked(c == 0 ? RED : c == 1 ? ORANGE : (rgb){236, 174, 52});
        for (int i = c; i < nl; i += 3) maple_leaf(LV[i][0], LV[i][1], LV[i][2], LV[i][3]);
        flush();
    }
    if (detail) {
        flat((rgb){140, 30, 24});
        for (int i = 0; i < nl; i++)
            for (int j = 0; j < 5; j++) {
                float a = (LV[i][3] - 90 + (j - 2) * 40) * PI_F / 180;
                STROKE(0.005f, 0.002f, LV[i][0], LV[i][1], LV[i][0] + 0.6f * LV[i][2] * cosf(a),
                       LV[i][1] + 0.6f * LV[i][2] * sinf(a));
            }
        flush();
    }
}

/* the deer, standing to the right, looking back */
static void deer(void)
{
    rgb coat = {176, 112, 62};
    inked((rgb){150, 92, 50});                  /* the far legs */
    STROKE(0.03f, 0.022f, -0.12f, 0.06f, -0.14f, 0.34f), STROKE(0.03f, 0.022f, 0.16f, 0.06f, 0.2f, 0.33f);
    flush();
    inked(coat);
    ST.light = 0.2f;
    ell(0.0f, 0.0f, 0.24f, 0.11f);              /* the body */
    curve(0.17f, -0.03f, 0.22f, -0.15f, 0.2f, -0.24f, 0.12f, -0.3f, 0.09f, 0.06f);   /* the neck, turning */
    lens(0.13f, -0.31f, -0.02f, -0.27f, 0.08f, 0.04f);   /* the head, looking back */
    STROKE(0.032f, 0.022f, -0.16f, 0.04f, -0.2f, 0.34f), STROKE(0.032f, 0.022f, 0.12f, 0.06f, 0.12f, 0.34f);
    flush();
    inked(coat);
    lens(0.13f, -0.33f, 0.2f, -0.4f, 0.04f, 0);  /* the ear */
    flush();
    inked(BROWN_D);                             /* the antlers */
    STROKE(0.016f, 0.008f, 0.1f, -0.34f, 0.08f, -0.44f, 0.12f, -0.52f);
    STROKE(0.012f, 0.006f, 0.088f, -0.44f, 0.02f, -0.48f);
    STROKE(0.016f, 0.008f, 0.07f, -0.33f, 0.02f, -0.42f, 0.03f, -0.5f);
    flush();
    flat(CREAM);                                /* the spots, the tail */
    for (int i = 0; i < 7; i++) circ(-0.15f + 0.05f * i, -0.04f + 0.025f * (i % 2), 0.016f);
    ell(-0.24f, -0.03f, 0.03f, 0.04f);
    flush();
    flat(INK);
    circ(0.03f, -0.29f, 0.01f);
    circ(-0.025f, -0.27f, 0.012f);
    flush();
}

static void october(int k)
{
    maple(k);
    if (k == 0) {
        place(0.52f, 1.12f, 1.25f, 0, 0);
        deer();
        ident();
    }
    if (k == 1) ribbon(0.5f, 1.1f, 0.8f, 12, HR_BLUE, 9);
}

/* ---- November: the willow in the rain ---------------------------------------------------------------------------- */
static void willow(int k)
{
    sky(k == 3 ? NIGHT : SLATE, -0.05f, 1.7f, 0, 0);
    if (k == 3) return;
    if (k == 0 && detail) {                     /* the rain */
        flat((rgb){110, 120, 150});
        for (int i = 0; i < 12; i++) {
            float x = 0.06f + 0.08f * i, y = 0.1f + 0.31f * (float)((i * 7) % 5);
            STROKE(0.006f, 0.006f, x, y, x - 0.03f, y + 0.18f);
        }
        flush();
    }
    inked((rgb){60, 50, 40});                   /* the trunk */
    curve(1.06f, 1.0f, 0.86f, 0.7f, 0.86f, 0.3f, 0.9f, -0.05f, 0.1f, 0.06f);
    flush();
    flat(GREEN_L);                              /* the hanging strands */
    for (int i = 0; i < 7; i++) {
        float x = 0.88f - 0.13f * i, l = 0.7f + 0.4f * ((i * 3) % 4) / 3.0f - (k == 0 ? 0.25f : 0);
        curve(0.9f, 0.05f + 0.06f * i, x + 0.02f, 0.1f, x - 0.06f, 0.3f, x - 0.08f, l, 0.012f, 0.006f);
    }
    flush();
    inked(GREEN);
    for (int i = 0; i < 7; i++) {
        float x = 0.88f - 0.13f * i, l = 0.7f + 0.4f * ((i * 3) % 4) / 3.0f - (k == 0 ? 0.25f : 0), p[40];
        bez(p, 20, 0.9f, 0.05f + 0.06f * i, x + 0.02f, 0.1f, x - 0.06f, 0.3f, x - 0.08f, l);
        for (int j = 6; j < 20; j += 3) lens(p[2 * j], p[2 * j + 1], p[2 * j] - 0.03f, p[2 * j + 1] + 0.09f, 0.035f, 0);
    }
    flush();
}

/* a stream: light waves across the foot */
static void stream(float y)
{
    flat((rgb){60, 96, 150});
    POLY(-0.05f, y, 0.3f, y - 0.03f, 0.7f, y + 0.02f, 1.05f, y - 0.02f, 1.05f, 1.65f, -0.05f, 1.65f);
    flush();
    flat(BLUE_L);
    for (int i = 0; i < 4; i++)
        curve(0.05f + 0.24f * i, y + 0.1f + 0.06f * (i % 2), 0.1f + 0.24f * i, y + 0.05f, 0.16f + 0.24f * i,
              y + 0.05f, 0.21f + 0.24f * i, y + 0.1f + 0.06f * (i % 2), 0.012f, 0.012f);
    flush();
}

/* the man under the umbrella walking by the stream, and the frog leaping */
static void rain_man(void)
{
    stream(1.36f);
    inked((rgb){180, 64, 44});                  /* the robe, flaring back as he walks */
    POLY(0.41f, 0.83f, 0.51f, 0.83f, 0.56f, 0.98f, 0.6f, 1.3f, 0.5f, 1.33f, 0.36f, 1.33f, 0.24f, 1.3f, 0.32f, 1.06f);
    flush();
    inked((rgb){40, 50, 90});                   /* the hem, the sleeve reaching for the shaft */
    POLY(0.25f, 1.25f, 0.6f, 1.25f, 0.6f, 1.31f, 0.5f, 1.34f, 0.36f, 1.34f, 0.24f, 1.31f);
    lens(0.42f, 0.88f, 0.55f, 1.02f, 0.1f, 0.1f);
    flush();
    inked(INK);                                 /* the feet, the tall cap, the shaft */
    ell(0.33f, 1.35f, 0.04f, 0.018f), ell(0.53f, 1.35f, 0.04f, 0.018f);
    POLY(0.42f, 0.76f, 0.45f, 0.64f, 0.49f, 0.65f, 0.5f, 0.76f);
    STROKE(0.014f, 0.014f, 0.555f, 1.0f, 0.58f, 0.42f);
    flush();
    inked((rgb){236, 214, 186});                /* the face, the hand */
    circ(0.46f, 0.79f, 0.045f);
    circ(0.555f, 0.99f, 0.022f);
    flush();
    inked(YELLOW);                              /* the umbrella, tilted into the rain */
    ST.light = 0.2f;
    POLY(0.14f, 0.66f, 0.3f, 0.46f, 0.58f, 0.38f, 0.86f, 0.44f, 0.96f, 0.6f, 0.8f, 0.58f, 0.6f, 0.62f, 0.38f, 0.64f);
    flush();
    flat(RED);
    for (int i = 0; i < 4; i++) {
        float x = 0.28f + 0.2f * i;
        STROKE(0.012f, 0.004f, 0.58f, 0.4f, x - 0.04f, 0.64f - 0.04f * (i == 3));
    }
    flush();
    inked((rgb){96, 170, 72});                  /* the frog */
    ell(0.18f, 1.24f, 0.06f, 0.04f);
    circ(0.13f, 1.2f, 0.025f);
    STROKE(0.016f, 0.008f, 0.22f, 1.26f, 0.3f, 1.3f, 0.32f, 1.36f);
    STROKE(0.014f, 0.008f, 0.14f, 1.26f, 0.08f, 1.32f);
    flush();
    flat(INK);
    circ(0.12f, 1.19f, 0.008f);
    flush();
}

/* the swallow, swooping down to the left */
static void swallow(void)
{
    place(0.42f, 0.98f, 1.3f, -24, 0);
    inked(NAVY);
    POLY(0.08f, -0.02f, 0.3f, 0.04f, 0.42f, 0.0f, 0.3f, 0.08f, 0.4f, 0.12f, 0.24f, 0.1f, 0.08f, 0.04f);   /* tail */
    POLY(-0.02f, -0.02f, 0.18f, -0.2f, 0.36f, -0.26f, 0.12f, -0.04f);   /* the wings */
    POLY(0.0f, 0.02f, 0.2f, 0.16f, 0.34f, 0.22f, 0.1f, 0.06f);
    lens(-0.16f, 0.0f, 0.14f, 0.0f, 0.09f, 0);  /* the body */
    circ(-0.15f, -0.005f, 0.045f);
    flush();
    flat(WHITE);
    lens(-0.1f, 0.025f, 0.1f, 0.02f, 0.04f, 0);
    flush();
    flat(RED);
    circ(-0.17f, 0.02f, 0.02f);
    flush();
    flat(INK);
    POLY(-0.19f, -0.015f, -0.235f, 0.0f, -0.19f, 0.01f);
    flush();
    ident();
}

/* the lightning: a band of red cloud with drums, and the bolt */
static void lightning(void)
{
    inked(CRIMSON);
    float p[80];
    int n = 0;
    for (int i = 0; i <= 18; i++) {
        float x = -0.05f + 1.1f * i / 18;
        p[n++] = x, p[n++] = 0.3f + 0.05f * sinf(i * 1.3f);
    }
    for (int i = 18; i >= 0; i--) {
        float x = -0.05f + 1.1f * i / 18;
        p[n++] = x, p[n++] = 0.72f + 0.05f * sinf(i * 1.1f + 1);
    }
    poly(p, n / 2);
    flush();
    inked(INK);                                 /* drums along the cloud */
    for (int i = 0; i < 4; i++) circ(0.14f + 0.24f * i, 0.5f + 0.04f * (i % 2), 0.07f);
    flush();
    pen(GOLD, NULL, 0, 0);
    for (int i = 0; i < 4; i++) band(0.14f + 0.24f * i, 0.5f + 0.04f * (i % 2), 0.03f, 0.05f, 0, 300);
    flush();
    inked(YELLOW);
    ST.light = 0.2f;
    POLY(0.56f, 0.6f, 0.7f, 0.6f, 0.56f, 0.9f, 0.68f, 0.9f, 0.4f, 1.42f, 0.5f, 1.0f, 0.38f, 1.0f, 0.48f, 0.72f);
    flush();
    inked(GREY);                                /* a black-and-grey ground */
    POLY(-0.05f, 1.46f, 0.4f, 1.42f, 1.05f, 1.48f, 1.05f, 1.65f, -0.05f, 1.65f);
    flush();
}

static void november(int k)
{
    willow(k);
    if (k == 0) rain_man();
    if (k == 1) { stream(1.42f); swallow(); }
    if (k == 2) ribbon(0.44f, 1.02f, 0.82f, -8, HR_RED, 10);
    if (k == 3) lightning();
}

/* ---- December: the paulownia ------------------------------------------------------------------------------------- */
static void kiri_leaf(float cx, float cy, float r, float deg)
{
    place(cx, cy, r, deg, 0);
    lens(0, 0.85f, 0, -1.0f, 1.55f, 0);         /* a broad leaf, its point up */
    lens(0, 0.85f, -0.75f, 0.1f, 0.55f, -0.08f), lens(0, 0.85f, 0.75f, 0.1f, 0.55f, 0.08f);   /* two side lobes */
    ident();
}

static void paulownia(int k)
{
    if (k == 1 || k == 0) sky(YELLOW, 1.2f, 1.7f, 0.04f, 0.5f);
    if (k == 3) sky((rgb){214, 92, 60}, 1.3f, 1.7f, 0.03f, 2);
    float lift = k == 0 ? 0.12f : 0;
    if (k != 0) {                               /* the flower stalks */
        inked(BROWN);
        for (int i = 0; i < 3; i++)
            STROKE(0.02f, 0.012f, 0.3f + 0.2f * i, 0.96f, 0.26f + 0.22f * i, 0.32f - 0.08f * (i == 1));
        flush();
        inked(PURPLE);
        ST.light = 0.2f;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 7; j++) {
                float x = 0.26f + 0.22f * i, y = 0.3f - 0.08f * (i == 1) + 0.075f * j;
                circ(x + (j % 2 ? 0.045f : -0.045f), y, 0.04f), circ(x, y + 0.03f, 0.035f);
            }
        flush();
    }
    inked(GREEN_D);
    kiri_leaf(0.28f, 1.24f + lift, 0.26f, -25), kiri_leaf(0.74f, 1.22f + lift, 0.26f, 25);
    kiri_leaf(0.5f, 1.12f + lift, 0.22f, 0);
    flush();
    if (detail) {
        flat(GREEN_L);
        static const float V[3][3] = {{0.28f, 1.24f, -25}, {0.74f, 1.22f, 25}, {0.5f, 1.12f, 0}};
        for (int i = 0; i < 3; i++) {
            float a = (V[i][2] - 90) * PI_F / 180, r = i == 2 ? 0.22f : 0.26f;
            STROKE(0.008f, 0.002f, V[i][0], V[i][1] + lift + 0.15f, V[i][0] + r * cosf(a),
                   V[i][1] + lift + r * sinf(a));
        }
        flush();
    }
}

/* the phoenix, wings spread over the paulownia, its long tail sweeping */
static void phoenix(void)
{
    static rgb TAIL[5] = {{214, 60, 50}, {60, 130, 90}, {236, 178, 52}, {60, 92, 170}, {214, 60, 50}};
    for (int i = 0; i < 5; i++) {               /* the tail feathers, curling down to the left */
        inked(TAIL[i]);
        float a = i * 0.07f;
        curve(0.6f, 0.62f, 0.4f - a, 0.7f + a, 0.1f + a, 0.6f + 1.5f * a, 0.12f + 0.6f * a, 0.98f + a, 0.05f, 0.02f);
        flush();
    }
    flat(TAIL[2]);
    for (int i = 0; i < 5; i++) {
        float a = i * 0.07f;
        circ(0.12f + 0.6f * a, 0.98f + a, 0.03f);
    }
    flush();
    inked((rgb){44, 104, 72});                  /* the far wing */
    for (int i = 0; i < 4; i++) lens(0.62f, 0.48f, 0.8f + 0.05f * i, 0.08f + 0.07f * i, 0.07f, 0.03f);
    flush();
    inked((rgb){214, 60, 50});                  /* the body, the neck */
    lens(0.48f, 0.66f, 0.76f, 0.44f, 0.16f, 0.05f);
    curve(0.7f, 0.46f, 0.8f, 0.34f, 0.7f, 0.24f, 0.78f, 0.16f, 0.07f, 0.045f);
    flush();
    for (int r = 0; r < 3; r++) {               /* the near wing, three rows of feathers */
        static rgb W[3] = {{236, 178, 52}, {60, 92, 170}, {60, 130, 90}};
        inked(W[r]);
        for (int i = 0; i < 6; i++) {
            float a = (150 - 14 * i) * PI_F / 180, l = 0.42f - 0.1f * r;
            lens(0.58f, 0.52f, 0.58f + l * cosf(a), 0.52f - l * sinf(a), 0.07f, 0.02f);
        }
        flush();
    }
    inked((rgb){236, 178, 52});                 /* the head, the crest, the bill */
    circ(0.79f, 0.15f, 0.045f);
    lens(0.78f, 0.12f, 0.72f, 0.02f, 0.03f, 0.1f);
    lens(0.8f, 0.12f, 0.8f, 0.0f, 0.03f, -0.1f);
    flush();
    inked(RED);
    lens(0.83f, 0.15f, 0.9f, 0.19f, 0.026f, 0);
    POLY(0.78f, 0.18f, 0.82f, 0.24f, 0.76f, 0.24f);   /* the wattle */
    flush();
    flat(INK);
    circ(0.8f, 0.14f, 0.009f);
    flush();
}

static void december(int k)
{
    paulownia(k);
    if (k == 0) phoenix();
    if (k == 2 && detail) {                     /* the one plain with a mark: a small red seal */
        inked(RED);
        POLY(0.08f, 1.4f, 0.2f, 1.4f, 0.2f, 1.52f, 0.08f, 1.52f);
        flush();
        flat(PAPER);
        STROKE(0.012f, 0.012f, 0.11f, 1.43f, 0.17f, 1.43f, 0.17f, 1.49f, 0.11f, 1.49f, 0.11f, 1.45f);
        flush();
    }
}

/* ---- the card ---------------------------------------------------------------------------------------------------- */
static float rr(float x, float y, float w, float h, float rad)   /* the distance to a rounded rectangle's edge */
{
    float qx = fabsf(x - w * 0.5f) - (w * 0.5f - rad), qy = fabsf(y - h * 0.5f) - (h * 0.5f - rad);
    return sqrtf(fmaxf(qx, 0) * fmaxf(qx, 0) + fmaxf(qy, 0) * fmaxf(qy, 0)) + fminf(fmaxf(qx, qy), 0) - rad;
}

static void scene(int card)
{
    int k = HANA_INDEX(card);
    switch (HANA_MONTH(card)) {
    case M_PINE: january(k); break;
    case M_PLUM: february(k); break;
    case M_CHERRY: march(k); break;
    case M_WISTERIA: april(k); break;
    case M_IRIS: may(k); break;
    case M_PEONY: june(k); break;
    case M_CLOVER: july(k); break;
    case M_PAMPAS: august(k); break;
    case M_CHRYSANTHEMUM: september(k); break;
    case M_MAPLE: october(k); break;
    case M_WILLOW: november(k); break;
    default: december(k); break;
    }
    flush();
}

void hana_paint(int card, int w, int h, uint8_t *rgba)
{
    float W = (float)w, rad = 0.075f * W;
    memset(rgba, 0, (size_t)w * h * 4);
    if (card < 0 || card >= KK_CARDS) {         /* the back: black lacquer, a sheen, a thin gold line */
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                float d = rr(x + 0.5f, y + 0.5f, W, (float)h, rad), a = fminf(fmaxf(0.5f - d, 0), 1);
                float g = (float)(x + y) / (w + h), sheen = 10 * expf(-40 * (g - 0.35f) * (g - 0.35f));
                float in = fabsf(rr(x + 0.5f - 0.09f * W, y + 0.5f - 0.09f * W, 0.82f * W, h - 0.18f * W, 0.04f * W));
                float line = fminf(fmaxf(fmaxf(0.012f * W, 0.7f) - in + 0.5f, 0), 1) * 0.75f;
                uint8_t *p = rgba + 4 * (y * w + x);
                float r = 34 + sheen, gg = 28 + sheen, b = 30 + sheen;
                p[0] = (uint8_t)(r + (GOLD[0] - r) * line), p[1] = (uint8_t)(gg + (GOLD[1] - gg) * line);
                p[2] = (uint8_t)(b + (GOLD[2] - b) * line), p[3] = (uint8_t)(255 * a);
            }
        return;
    }
    /* the scene on paper, then laid inside the dark rim (the back folded over the face's edge) */
    uint8_t *sc = calloc((size_t)w * h, 4);
    if (!sc) return;
    C.o = sc, C.w = w, C.h = h, C.s = W;
    detail = w >= 44;
    ident();
    for (int i = 0; i < w * h; i++) sc[4 * i] = PAPER[0], sc[4 * i + 1] = PAPER[1], sc[4 * i + 2] = PAPER[2],
                                    sc[4 * i + 3] = 255;
    scene(card);
    float b = fmaxf(1.2f, 0.035f * W);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float d = rr(x + 0.5f, y + 0.5f, W, (float)h, rad), a = fminf(fmaxf(0.5f - d, 0), 1);
            float di = rr(x + 0.5f - b, y + 0.5f - b, W - 2 * b, h - 2 * b, fmaxf(rad - b, 1));
            float m = fminf(fmaxf(0.5f - di, 0), 1);
            uint8_t *p = rgba + 4 * (y * w + x), *s = sc + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(34 + (s[i] - 34) * m);
            p[3] = (uint8_t)(255 * a);
        }
    free(sc);
}

/* ---- the decision's icons ---------------------------------------------------------------------------------------- */
void hana_icon(int which, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    C.o = rgba, C.w = C.h = size, C.s = (float)size;
    detail = 1;
    ident();
    if (which == HI_STOP) {                     /* stop: a gold disc, a dark open hand raised */
        pen(GOLD, (rgb){150, 110, 40}, 0.03f, 0.25f);
        circ(0.5f, 0.5f, 0.47f);
        flush();
        inked((rgb){70, 40, 30});
        ST.line = 0.02f;
        static const float F[4][3] = {{0.33f, 0.2f, 0.42f}, {0.44f, 0.14f, 0.42f}, {0.55f, 0.15f, 0.42f},
                                      {0.66f, 0.22f, 0.44f}};
        for (int i = 0; i < 4; i++) STROKE(0.1f, 0.1f, F[i][0], F[i][2], F[i][0] + 0.005f, F[i][1]);
        for (int i = 0; i < 4; i++) circ(F[i][0] + 0.005f, F[i][1], 0.05f);
        STROKE(0.1f, 0.09f, 0.27f, 0.62f, 0.2f, 0.5f, 0.18f, 0.42f);   /* the thumb */
        circ(0.18f, 0.42f, 0.045f);
        POLY(0.28f, 0.4f, 0.72f, 0.4f, 0.71f, 0.62f, 0.62f, 0.78f, 0.38f, 0.78f, 0.28f, 0.62f);   /* the palm */
        flush();
    } else {                                    /* koi-koi: a red lacquer disc, a cream double arrow onwards */
        pen(RED, CRIMSON, 0.03f, 0.25f);
        circ(0.5f, 0.5f, 0.47f);
        flush();
        pen(CREAM, (rgb){120, 20, 30}, 0.02f, 0.1f);
        for (int i = 0; i < 2; i++) {
            float x = 0.24f + 0.24f * i;
            POLY(x, 0.26f, x + 0.12f, 0.26f, x + 0.3f, 0.5f, x + 0.12f, 0.74f, x, 0.74f, x + 0.18f, 0.5f);
        }
        flush();
    }
}

/* cards side by side, overlapping, each cw wide (a yaku's badge): their picture, w x h */
void hana_fan(const uint8_t *cards, int n, int cw, int w, int h, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)w * h * 4);
    int ch = (int)(cw * HANA_ASPECT + 0.5f);
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b || n < 1) { free(b); return; }
    float step = n > 1 ? (float)(w - cw) / (n - 1) : 0;
    for (int i = 0; i < n; i++) {
        hana_paint(cards[i], cw, ch, b);
        art_blend(rgba, w, h, b, cw, ch, (int)(i * step + 0.5f), (h - ch) / 2 + (i % 2 ? 1 : -1) * (h - ch) / 4);
    }
    free(b);
}

/* ---- the deck for the card kit (its texture cache, shadows, gold light, flips and slides) ------------------------ */
static void hana_face(int card, int w, int h, uint8_t *rgba)
{
    hana_paint(card < KK_CARDS ? card : KK_NONE, w, h, rgba);
}
static void hana_back(int w, int h, uint8_t *rgba) { hana_paint(KK_NONE, w, h, rgba); }

int hana_deck(void)
{
    static const cards_deck_def def = {HANA_ASPECT, hana_face, hana_back};
    static int deck = -1;
    if (deck < 0) deck = cards_deck_add(&def);
    return deck;
}
