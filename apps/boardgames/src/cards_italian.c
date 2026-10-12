/*
 * The card kit's Italian-suited pack of 40 (DECK_ITALIAN: Scopa, Briscola), drawn by code in the house style: an
 * original rendition, no image files, no letters. The suits are the traditional symbols: coins (denari) gold discs
 * with a red rosette, cups (coppe) gold goblets with the wine showing, swords (spade) steel blades with a gold guard,
 * clubs (bastoni) knotty batons putting out leaves. The number cards: the coins and cups in two columns, the swords
 * and clubs in a lattice crossing in the middle; the settebello (the 7 of coins) framed in gold; the aces one big
 * symbol, the swords' and clubs' in a laurel wreath, the coins' with rays. The courts single-headed, as in the
 * regional patterns (Napoletane, Piacentine): a figure under an arch on a tiled floor, the house's chess figure in
 * the suit's colour (fante a pawn, the foot soldier; cavallo a knight; re a king), a coin or a cup held up over it, a
 * sword or a club on its shoulder. The corner index: the digit 1..10 (the courts' 8, 9, 10, as the modern Italian
 * packs print them) over the suit. Under about 52 px a plainer face: the digit big over the symbol. The back is the
 * house's; the cards are 1.4 times as tall as wide, like the French ones.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "cards.h"
#include "games/chess.h"
#include "text.h"

#define IT_PI 3.14159265f

/* ---- the colours ------------------------------------------------------------------------------------------------- */
enum { ITC_IVORY, ITC_RIM, ITC_GOLD, ITC_GOLD_D, ITC_RED, ITC_RED_D, ITC_WINE, ITC_STEEL, ITC_STEEL_D, ITC_WOOD,
       ITC_WOOD_D, ITC_CUT, ITC_LEAF, ITC_LEAF_D, ITC_PANEL, ITC_FLOOR, ITC_BLUE, ITC_N };

static const uint8_t *it_rgb(int k)
{
    static const uint8_t c[ITC_N][3] = {
        {250, 246, 236}, {176, 164, 140}, {238, 188, 66}, {120, 72, 20}, {200, 46, 40}, {92, 20, 16},
        {128, 24, 34}, {196, 212, 228}, {40, 54, 88}, {184, 120, 62}, {78, 44, 20}, {236, 204, 150},
        {78, 150, 80}, {26, 72, 38}, {246, 236, 210}, {224, 208, 176}, {58, 92, 168},
    };
    return c[k];
}

/* each suit's ink (the index) and its figure's style (the courts): coins amber, cups red, swords lapis, clubs jade:
 * the house's own colours */
static const uint8_t *it_ink(int suit)
{
    static const uint8_t c[4][3] = {{176, 108, 16}, {184, 36, 36}, {38, 68, 146}, {32, 108, 66}};
    return c[suit & 3];
}

static const art_style *it_figure_style(int suit)
{
    static const art_style st[4] = {
        {{226, 166, 54}, {82, 48, 10}, {150, 96, 28}, {252, 238, 204}, 0.34f, 0},
        {{192, 46, 40}, {62, 12, 10}, {236, 150, 130}, {248, 226, 210}, 0.32f, 0},
        {{60, 96, 172}, {14, 24, 56}, {156, 182, 230}, {228, 236, 250}, 0.34f, 0},
        {{58, 132, 94}, {12, 44, 28}, {150, 208, 172}, {222, 242, 230}, 0.34f, 0},
    };
    return &st[suit & 3];
}

/* ---- painting: shapes in pixels, turned and scaled, rendered over their own bounds ----------------------------- */
typedef struct { uint8_t *o; int w, h; } it_canvas;
typedef struct { art_fig *f; float x0, y0, x1, y1; } it_pen;
typedef struct { float x, y, s, c, n; } it_xf;  /* a local point (u, v) goes to (x, y) + s * (u, v) turned */

static it_xf it_at(float x, float y, float s, float deg)
{
    float a = deg * IT_PI / 180;
    return (it_xf){x, y, s, cosf(a), sinf(a)};
}

static void it_pen_new(it_pen *p)
{
    p->f = art_fig_new();
    p->x0 = p->y0 = 1e9f;
    p->x1 = p->y1 = -1e9f;
}

static void it_grow(it_pen *p, float x, float y)
{
    p->x0 = fminf(p->x0, x);
    p->y0 = fminf(p->y0, y);
    p->x1 = fmaxf(p->x1, x);
    p->y1 = fmaxf(p->y1, y);
}

static void it_poly(it_pen *p, int role, it_xf t, const float *uv, int n)
{
    float q[2 * 48];
    if (n > 48) n = 48;
    for (int i = 0; i < n; i++) {
        float u = uv[2 * i], v = uv[2 * i + 1];
        q[2 * i] = t.x + t.s * (u * t.c - v * t.n);
        q[2 * i + 1] = t.y + t.s * (u * t.n + v * t.c);
        it_grow(p, q[2 * i], q[2 * i + 1]);
    }
    if (p->f) art_poly(p->f, role, q, n);
}

/* an ellipse in local units (turned with t): a polygon */
static void it_oval(it_pen *p, int role, it_xf t, float cx, float cy, float rx, float ry)
{
    float uv[2 * 32];
    for (int i = 0; i < 32; i++) {
        float a = 2 * IT_PI * i / 32;
        uv[2 * i] = cx + rx * cosf(a);
        uv[2 * i + 1] = cy + ry * sinf(a);
    }
    it_poly(p, role, t, uv, 32);
}

static void it_box(it_pen *p, int role, it_xf t, float u0, float v0, float u1, float v1)
{
    const float uv[8] = {u0, v0, u1, v0, u1, v1, u0, v1};
    it_poly(p, role, t, uv, 4);
}

/* a band round a circle (pixels, not turned) */
static void it_ring(it_pen *p, int role, float cx, float cy, float r0, float r1)
{
    if (p->f) art_arc(p->f, role, cx, cy, r0, r1, 0, 360);
    it_grow(p, cx - r1, cy - r1);
    it_grow(p, cx + r1, cy + r1);
}

static art_style it_style(int fill, int line, float lw, float light)
{
    art_style st;
    const uint8_t *f = it_rgb(fill), *l = it_rgb(line);
    for (int i = 0; i < 3; i++) st.fill[i] = f[i], st.outer[i] = st.inner[i] = st.mark[i] = l[i];
    st.light = light;
    st.line = lw;
    return st;
}

/* the pen's shapes painted on the canvas (only over their bounds), the pen emptied */
static void it_ink_pen(it_canvas *c, it_pen *p, const art_style *st)
{
    if (!p->f) return;
    float m = st->line + 2;
    int x0 = (int)floorf(fmaxf(0, p->x0 - m)), y0 = (int)floorf(fmaxf(0, p->y0 - m));
    int x1 = (int)ceilf(fminf((float)c->w, p->x1 + m)), y1 = (int)ceilf(fminf((float)c->h, p->y1 + m));
    if (x1 > x0 && y1 > y0) {
        int bw = x1 - x0, bh = y1 - y0;
        uint8_t *b = malloc((size_t)bw * bh * 4);
        if (b) {
            art_render(p->f, st, bw, bh, 1, (float)-x0, (float)-y0, b);
            art_blend(c->o, c->w, c->h, b, bw, bh, x0, y0);
            free(b);
        }
    }
    art_fig_free(p->f);
    p->f = NULL;
}

/* a rounded rectangle of one colour (alpha 0..1) */
static void it_fill(it_canvas *c, float x, float y, float w, float h, float rad, const uint8_t rgb[3], float a)
{
    int iw = (int)(w + 0.5f), ih = (int)(h + 0.5f);
    if (iw < 1 || ih < 1) return;
    const uint8_t col[4] = {rgb[0], rgb[1], rgb[2], (uint8_t)(255 * (a < 0 ? 0 : a > 1 ? 1 : a))};
    uint8_t *b = malloc((size_t)iw * ih * 4);
    if (!b) return;
    art_roundrect(iw, ih, rad, col, b);
    art_blend(c->o, c->w, c->h, b, iw, ih, (int)(x + 0.5f), (int)(y + 0.5f));
    free(b);
}

/* a frame: a rounded rectangle less a smaller one inside it, t thick */
static void it_frame(it_canvas *c, float x, float y, float w, float h, float rad, float t, const uint8_t rgb[3],
                            float a)
{
    for (int j = (int)y; j < (int)ceilf(y + h) && j < c->h; j++)
        for (int i = (int)x; i < (int)ceilf(x + w) && i < c->w; i++) {
            if (i < 0 || j < 0) continue;
            float qx = fabsf(i + 0.5f - x - w / 2) - (w / 2 - rad), qy = fabsf(j + 0.5f - y - h / 2) - (h / 2 - rad);
            float d = sqrtf(fmaxf(qx, 0) * fmaxf(qx, 0) + fmaxf(qy, 0) * fmaxf(qy, 0)) + fminf(fmaxf(qx, qy), 0) - rad;
            float k = fminf(1, fmaxf(0, 0.5f - d)) * fminf(1, fmaxf(0, d + t + 0.5f)) * a;
            if (k <= 0) continue;
            uint8_t px[4] = {rgb[0], rgb[1], rgb[2], (uint8_t)(255 * k)};
            art_blend(c->o, c->w, c->h, px, 1, 1, i, j);
        }
}

/* digits from the font, bold, centred on (cx, cy), hgt pixels tall; a 6 or a 9 underlined (it may show turned) */
static void it_digits(it_canvas *c, int value, float cx, float cy, float hgt, const uint8_t rgb[3])
{
    char s[4];
    int n = value >= 10 ? 2 : 1;
    s[0] = (char)('0' + (n == 2 ? value / 10 : value));
    s[1] = (char)('0' + value % 10);
    float step = 0.56f * hgt, total = step * (n - 1);
    int cw = (int)ceilf(0.9f * hgt) + 2, ch = (int)ceilf(1.3f * hgt) + 2;
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    for (int i = 0; i < n; i++) {
        memset(b, 0, (size_t)cw * ch * 4);
        if (!text_glyph(s[i], 1.2f * hgt, 1, rgb, b, cw, ch)) continue;
        float dx = cx - total / 2 + i * step - cw / 2.0f, dy = cy - ch / 2.0f;
        art_blend(c->o, c->w, c->h, b, cw, ch, (int)floorf(dx + 0.5f), (int)floorf(dy + 0.5f));
    }
    free(b);
    if (value == 6 || value == 9) {
        float bw = 0.46f * hgt, bt = fmaxf(1.5f, 0.1f * hgt);
        it_fill(c, cx - bw / 2, cy + 0.64f * hgt, bw, bt, 0, rgb, 1);
    }
}

/* ---- the suits' symbols: centred on (x, y), `s` pixels tall, turned deg ---------------------------------------- */
static float it_lw(float s) { return fmaxf(0.9f, 0.045f * s); }   /* an outline for a symbol s tall */

/* a coin: a gold disc, a red ring in it, a rosette in the middle (rich: rays round it, the aces) */
static void it_coin(it_canvas *c, float x, float y, float s, int rich)
{
    float r = s / 2, lw = fminf(it_lw(s) * 0.8f, 2.4f);
    it_pen p;
    if (rich) {                                 /* sixteen rays, gold, round the coin */
        it_pen_new(&p);
        for (int i = 0; i < 16; i++) {
            float len = i & 1 ? 0.2f : 0.3f;
            const float ray[6] = {-0.07f, -0.98f, 0, -0.98f - len, 0.07f, -0.98f};
            it_poly(&p, ART_PART, it_at(x, y, r, 22.5f * i), ray, 3);
        }
        art_style st = it_style(ITC_GOLD, ITC_GOLD_D, lw, 0.1f);
        it_ink_pen(c, &p, &st);
        r *= 0.8f;
    }
    it_pen_new(&p);
    it_oval(&p, ART_PART, it_at(x, y, 1, 0), 0, 0, r, r);
    art_style gold = it_style(ITC_GOLD, ITC_GOLD_D, lw, 0.35f);
    it_ink_pen(c, &p, &gold);
    it_pen_new(&p);
    it_ring(&p, ART_PART, x, y, 0.62f * r, 0.76f * r);
    art_style red = it_style(ITC_RED, ITC_RED_D, s < 26 ? 0 : 0.6f * lw, 0.15f);
    it_ink_pen(c, &p, &red);
    it_pen_new(&p);
    if (s >= 26) {                              /* a rosette of eight petals */
        for (int i = 0; i < 8; i++) {
            it_xf t = it_at(x, y, r, 45.0f * i + 22.5f);
            it_oval(&p, ART_PART, t, 0, -0.3f, 0.1f, 0.2f);
        }
        art_style ro = it_style(ITC_RED, ITC_RED_D, 0.5f * lw, 0.2f);
        it_ink_pen(c, &p, &ro);
        it_pen_new(&p);
        it_oval(&p, ART_PART, it_at(x, y, r, 0), 0, 0, 0.14f, 0.14f);
        it_ink_pen(c, &p, &gold);
    } else {
        it_oval(&p, ART_PART, it_at(x, y, r, 0), 0, 0, 0.28f, 0.28f);
        it_ink_pen(c, &p, &red);
    }
}

/* a cup: a goblet, its bowl showing the wine, a red knot on the stem, a broad foot; local units: 1 tall */
static void it_cup(it_canvas *c, float x, float y, float s, float deg, int rich)
{
    it_xf t = it_at(x, y, s, deg);
    float lw = fminf(it_lw(s) * 0.8f, 2.4f);
    art_style gold = it_style(ITC_GOLD, ITC_GOLD_D, lw, 0.4f), red = it_style(ITC_RED, ITC_RED_D, lw, 0.25f);
    art_style wine = it_style(ITC_WINE, ITC_GOLD_D, 0.7f * lw, 0);
    it_pen p;
    it_pen_new(&p);                             /* the foot and the stem */
    const float foot[] = {-0.07f, 0.3f, 0.07f, 0.3f, 0.26f, 0.44f, 0.26f, 0.49f, -0.26f, 0.49f, -0.26f, 0.44f};
    it_poly(&p, ART_PART, t, foot, 6);
    it_box(&p, ART_PART, t, -0.055f, -0.08f, 0.055f, 0.32f);
    it_ink_pen(c, &p, &gold);
    if (rich) {                                 /* the aces: two handles, behind the bowl */
        it_pen_new(&p);
        for (int sd = -1; sd <= 1; sd += 2) {
            float h[2 * 24];
            for (int i = 0; i < 12; i++) {
                float a = (-110 + 220.0f * i / 11) * IT_PI / 180;
                h[2 * i] = sd * (0.3f + 0.13f * cosf(a)), h[2 * i + 1] = -0.24f + 0.12f * sinf(a);
                h[2 * (23 - i)] = sd * (0.3f + 0.08f * cosf(a)), h[2 * (23 - i) + 1] = -0.24f + 0.07f * sinf(a);
            }
            it_poly(&p, ART_PART, t, h, 24);
        }
        it_ink_pen(c, &p, &gold);
    }
    it_pen_new(&p);                             /* the bowl: a half-ellipse under the rim */
    float bowl[2 * 26];
    for (int i = 0; i <= 24; i++) {
        float a = IT_PI * i / 24;
        bowl[2 * i] = 0.34f * cosf(a);
        bowl[2 * i + 1] = -0.36f + 0.34f * sinf(a) * (rich ? 0.95f : 1);
    }
    it_poly(&p, ART_PART, t, bowl, 25);
    it_ink_pen(c, &p, &gold);
    it_pen_new(&p);                             /* the knot, a band round the bowl */
    it_oval(&p, ART_PART, t, 0, 0.1f, 0.1f, 0.065f);
    if (s >= 30) {
        float band[2 * 20];
        for (int i = 0; i < 10; i++) {
            float a = IT_PI * i / 9, ca = cosf(a);
            band[2 * i] = 0.33f * ca;
            band[2 * i + 1] = -0.27f + 0.06f * sinf(a);
            band[2 * (19 - i)] = 0.315f * ca;
            band[2 * (19 - i) + 1] = -0.2f + 0.09f * sinf(a);
        }
        it_poly(&p, ART_PART, t, band, 20);
    }
    it_ink_pen(c, &p, &red);
    it_pen_new(&p);                             /* the rim, the wine inside it */
    it_oval(&p, ART_PART, t, 0, -0.36f, 0.345f, 0.075f);
    it_ink_pen(c, &p, &gold);
    it_pen_new(&p);
    it_oval(&p, ART_PART, t, 0, -0.355f, 0.27f, 0.045f);
    it_ink_pen(c, &p, &wine);
    if (rich) {                                 /* the aces: three blue gems on the band */
        it_pen_new(&p);
        for (int i = -1; i <= 1; i++) it_oval(&p, ART_PART, t, 0.15f * i, -0.215f + 0.012f * (i == 0), 0.034f, 0.03f);
        art_style gem = it_style(ITC_BLUE, ITC_STEEL_D, 0.6f * lw, 0.5f);
        it_ink_pen(c, &p, &gem);
    }
}

/* a sword: a steel blade (a dark fuller along it), a gold guard and pommel, a red grip; local units: 1 long, the
 * point up */
static void it_sword(it_canvas *c, float x, float y, float s, float deg, float wide, float hilt)
{
    it_xf t = it_at(x, y, s, deg);
    float lw = fminf(it_lw(0.32f * s), 2.2f), bw = 0.042f * wide, gw = wide * hilt;
    art_style steel = it_style(ITC_STEEL, ITC_STEEL_D, lw, 0.35f), gold = it_style(ITC_GOLD, ITC_GOLD_D, lw, 0.3f);
    art_style red = it_style(ITC_RED, ITC_RED_D, lw, 0.2f);
    steel.mark[0] = 120, steel.mark[1] = 140, steel.mark[2] = 170;
    it_pen p;
    it_pen_new(&p);
    const float blade[] = {-bw, 0.22f, -0.85f * bw, -0.36f, 0, -0.5f, 0.85f * bw, -0.36f, bw, 0.22f};
    it_poly(&p, ART_PART, t, blade, 5);
    if (s >= 40) {
        const float fuller[] = {-0.18f * bw, 0.18f, -0.15f * bw, -0.33f, 0.15f * bw, -0.33f, 0.18f * bw, 0.18f};
        it_poly(&p, ART_MARK, t, fuller, 4);
    }
    it_ink_pen(c, &p, &steel);
    it_pen_new(&p);
    it_box(&p, ART_PART, t, -0.034f * wide, 0.25f, 0.034f * wide, 0.42f);
    it_ink_pen(c, &p, &red);
    it_pen_new(&p);                             /* the guard: a bar with its ends curling, the pommel */
    const float guard[] = {-0.17f * gw, 0.205f, 0.17f * gw, 0.205f, 0.19f * gw, 0.235f, 0.17f * gw, 0.26f,
                           -0.17f * gw, 0.26f, -0.19f * gw, 0.235f};
    it_poly(&p, ART_PART, t, guard, 6);
    it_oval(&p, ART_PART, t, -0.19f * gw, 0.2f, 0.028f * gw, 0.035f);
    it_oval(&p, ART_PART, t, 0.19f * gw, 0.2f, 0.028f * gw, 0.035f);
    it_oval(&p, ART_PART, t, 0, 0.455f, 0.055f * gw, 0.05f);
    it_ink_pen(c, &p, &gold);
}

/* a club: a knotty baton, thick at its head, cut branches with leaves sprouting; local units: 1 long, the head up */
static void it_club(it_canvas *c, float x, float y, float s, float deg, float wide, int twigs)
{
    it_xf t = it_at(x, y, s, deg);
    float lw = fminf(it_lw(0.32f * s), 2.2f);
    art_style wood = it_style(ITC_WOOD, ITC_WOOD_D, lw, 0.4f), cut = it_style(ITC_CUT, ITC_WOOD_D, 0.8f * lw, 0);
    art_style leaf = it_style(ITC_LEAF, ITC_LEAF_D, 0.8f * lw, 0.2f), red = it_style(ITC_RED, ITC_RED_D, lw, 0.2f);
    it_pen p;
    it_pen_new(&p);                             /* the stubs of cut branches, behind the baton */
    static const float stub[3][3] = {{1, -0.02f, 0.8f}, {-1, -0.24f, 0.75f}, {-1, 0.2f, 0.7f}};
    for (int k = 0; k < twigs; k++) {
        float sd = stub[k][0], v = stub[k][1], w0 = 0.032f * wide * stub[k][2];
        float bx = sd * 0.045f * wide, ex = sd * 0.13f * wide, ey = v - 0.07f;
        const float st[] = {bx, v - w0, ex - sd * 0.01f, ey - w0 * 0.8f, ex + sd * 0.01f, ey + w0 * 0.8f, bx, v + w0};
        it_poly(&p, ART_PART, t, st, 4);
    }
    it_ink_pen(c, &p, &wood);
    if (s >= 36) {
        it_pen_new(&p);
        for (int k = 0; k < twigs; k++) {
            float sd = stub[k][0], ex = sd * 0.13f * wide, ey = stub[k][1] - 0.07f;
            it_oval(&p, ART_PART, t, ex, ey, 0.022f * wide, 0.026f);
        }
        it_ink_pen(c, &p, &cut);
    }
    it_pen_new(&p);                             /* the baton: tapering, the head rounded */
    float body[2 * 40];
    int n = 0;
    for (int i = 0; i <= 9; i++) {              /* the head: half a circle */
        float a = IT_PI + IT_PI * i / 9;
        body[n++] = 0.075f * wide * cosf(a);
        body[n++] = -0.42f + 0.075f * sinf(a);
    }
    static const float side[5][2] = {{-0.3f, 0.07f}, {-0.1f, 0.062f}, {0.1f, 0.056f}, {0.3f, 0.05f}, {0.45f, 0.046f}};
    for (int i = 0; i < 5; i++) body[n++] = side[i][1] * wide, body[n++] = side[i][0];
    for (int i = 1; i < 8; i++) {               /* the butt: rounded */
        float a = IT_PI * i / 8;
        body[n++] = 0.046f * wide * cosf(a);
        body[n++] = 0.45f + 0.03f * sinf(a);
    }
    for (int i = 4; i >= 0; i--) body[n++] = -side[i][1] * wide, body[n++] = side[i][0] + 0.015f * (i & 1);
    it_poly(&p, ART_PART, t, body, n / 2);
    it_ink_pen(c, &p, &wood);
    if (s >= 36) {                              /* leaves at the stubs, a red band at the grip */
        it_pen_new(&p);
        for (int k = 0; k < twigs; k++) {
            float sd = stub[k][0], ex = sd * 0.15f * wide, ey = stub[k][1] - 0.1f;
            it_xf lt = it_at(t.x + t.s * (ex * t.c - ey * t.n), t.y + t.s * (ex * t.n + ey * t.c), s,
                             deg + sd * 35);
            it_oval(&p, ART_PART, lt, 0, -0.035f, 0.022f, 0.045f);
        }
        it_ink_pen(c, &p, &leaf);
        it_pen_new(&p);
        it_box(&p, ART_PART, t, -0.052f * wide, 0.3f, 0.052f * wide, 0.35f);
        it_ink_pen(c, &p, &red);
    }
}

/* a laurel wreath (the aces of swords and clubs): two branches of leaves round (x, y), radius r */
static void it_wreath(it_canvas *c, float x, float y, float r)
{
    it_pen p;
    it_pen_new(&p);
    for (int sd = -1; sd <= 1; sd += 2)
        for (int i = 0; i < 9; i++) {
            float a = (90 + sd * (28 + 17 * i)) * IT_PI / 180, lx = x + r * cosf(a), ly = y + r * sinf(a);
            float tang = a * 180 / IT_PI + 90 + (sd < 0 ? 180 : 0) + sd * 25;
            it_oval(&p, ART_PART, it_at(lx, ly, r, tang + 90), 0, 0, 0.07f, 0.15f);
            if (i == 8) break;
        }
    art_style leaf = it_style(ITC_LEAF, ITC_LEAF_D, fmaxf(0.9f, 0.012f * r * 2), 0.25f);
    it_ink_pen(c, &p, &leaf);
    it_pen_new(&p);                             /* the ribbon tying them, at the bottom */
    const float bow[] = {-0.16f, 0.95f, 0.16f, 0.95f, 0.22f, 1.12f, 0.08f, 1.04f, 0, 1.0f, -0.08f, 1.04f,
                         -0.22f, 1.12f};
    it_poly(&p, ART_PART, it_at(x, y, r, 0), bow, 7);
    art_style red = it_style(ITC_RED, ITC_RED_D, fmaxf(0.9f, 0.012f * r * 2), 0.2f);
    it_ink_pen(c, &p, &red);
}

/* a suit's symbol, s tall (the coins: s wide), centred on (x, y), turned deg (swords and clubs) */
static void it_sym(it_canvas *c, int suit, float x, float y, float s, float deg)
{
    switch (suit & 3) {
    case IT_COINS: it_coin(c, x, y, s, 0); break;
    case IT_CUPS: it_cup(c, x, y, s, deg, 0); break;
    case IT_SWORDS: it_sword(c, x, y, s, deg, 1, 1); break;
    default: it_club(c, x, y, s, deg, 1, 3); break;
    }
}

/* ---- the faces --------------------------------------------------------------------------------------------------- */
/* the number cards' pips: coins and cups in two columns (the 7 with one in the middle above, the settebello's
 * classic look); swords and clubs in a sheaf crossing in the middle, the ends on an ellipse */
static void it_pips(it_canvas *c, int suit, int n)
{
    float W = (float)c->w, H = (float)c->h, cx = W / 2, cy = H / 2;
    if (suit == IT_COINS || suit == IT_CUPS) {
        float at[7][2], s = (suit == IT_COINS ? 0.25f : 0.31f) * W;
        int k = 0;
        float xl = 0.33f * W, xr = 0.67f * W, top = 0.23f * H, bot = 0.77f * H;
        switch (n) {
        case 2: at[0][0] = cx, at[0][1] = 0.3f * H, at[1][0] = cx, at[1][1] = 0.7f * H, k = 2; break;
        case 3:
            for (int i = 0; i < 3; i++) at[i][0] = 0.36f * W + 0.14f * W * i, at[i][1] = top + (bot - top) * i / 2;
            k = 3;
            break;
        case 4: case 5:
            at[0][0] = xl, at[0][1] = top + 0.02f * H, at[1][0] = xr, at[1][1] = top + 0.02f * H;
            at[2][0] = xl, at[2][1] = bot - 0.02f * H, at[3][0] = xr, at[3][1] = bot - 0.02f * H;
            k = 4;
            if (n == 5) at[4][0] = cx, at[4][1] = cy, k = 5;
            if (n == 5) s *= suit == IT_COINS ? 0.96f : 0.86f;
            break;
        default:                                /* 6 in two columns; the 7 one more in the middle, above */
            for (int i = 0; i < 3; i++) {
                at[k][0] = xl, at[k][1] = top + (bot - top) * i / 2, k++;
                at[k][0] = xr, at[k][1] = top + (bot - top) * i / 2, k++;
            }
            if (n == 7) at[k][0] = cx, at[k][1] = top + (bot - top) * 0.25f, k++;
            s *= suit == IT_COINS ? (n == 7 ? 0.84f : 0.92f) : (n == 7 ? 0.74f : 0.82f);
        }
        for (int i = 0; i < k; i++) it_sym(c, suit, at[i][0], at[i][1], s, 0);
        return;
    }
    /* a lattice: the ones leaning right side by side, the ones leaning left across them, each family's middles
     * spread along the other's direction (so their hilts, or the clubs' butts, stagger); an odd one upright in the
     * middle. Each as long as the card allows, its ends kept off the corner indices. */
    int p = n / 2;
    int sw = suit == IT_SWORDS;
    float lean = n <= 3 ? 30 : sw ? 27 : 24, L = (n <= 3 ? 0.8f : 0.78f) * H;
    float wide = n >= 6 ? 0.5f : n >= 4 ? 0.6f : 0.9f;   /* the swords' hilts: smaller when many */
    float S = n <= 3 ? 0 : (p == 2 ? (sw ? 1.0f : 0.86f) : (sw ? 1.08f : 0.92f)) * W;
    float sl = sinf(lean * IT_PI / 180), cl = cosf(lean * IT_PI / 180);
    for (int k = 0; k < n; k++) {
        int fam = k < p ? 0 : k < 2 * p ? 1 : 2, i = fam < 2 ? k % p : 0;   /* 2: the upright one, drawn last */
        float f = fam == 2 ? 0 : (i + 1.0f) / (p + 1) - 0.5f, dir = fam == 0 ? 1 : -1;
        float deg = fam == 2 ? 0 : dir * lean, ux = fam == 2 ? 0 : -dir * sl, uy = -cl;   /* the other family's up */
        float mx = cx + f * S * ux, my = cy + f * S * uy, ax = sinf(deg * IT_PI / 180), ay = -cosf(deg * IT_PI / 180);
        float half = L / 2;
        for (int it = 0; it < 40; it++) {       /* shortened until both ends are clear */
            int ok = 1;
            for (int e = -1; e <= 1; e += 2) {
                float x = mx + e * half * ax, y = my + e * half * ay;
                if (x < 0.1f * W || x > 0.9f * W || y < 0.07f * H || y > 0.93f * H) ok = 0;
                if ((x < 0.25f * W && y < 0.32f * H) || (x > 0.75f * W && y > 0.68f * H)) ok = 0;
            }
            if (ok) break;
            half *= 0.97f;
        }
        int flip = 0;
        if (suit == IT_SWORDS) it_sword(c, mx, my, 2 * half, deg + 180 * flip, fam == 2 ? 0.9f : 0.8f, wide);
        else it_club(c, mx, my, 2 * half, deg, n >= 6 ? 0.72f : n >= 4 ? 0.8f : 0.95f, n >= 4 ? 1 : 3);
    }
}

/* the ace: one big symbol (coins with rays; cups ornate; swords and clubs in a laurel wreath) */
static void it_ace(it_canvas *c, int suit)
{
    float W = (float)c->w, H = (float)c->h, cx = W / 2, cy = H / 2;
    switch (suit) {
    case IT_COINS: it_coin(c, cx, cy, 0.66f * W, 1); break;
    case IT_CUPS: it_cup(c, cx, cy, 0.72f * W, 0, 1); break;
    default:
        it_wreath(c, cx, cy + 0.08f * H, 0.3f * W);
        if (suit == IT_SWORDS) it_sword(c, cx, cy, 0.76f * H, 0, 1.25f, 1);
        else it_club(c, cx, cy, 0.74f * H, 0, 1.3f, 3);
    }
}

/* a court: a pale niche with a round arch (the spandrels in the suit's colour), a tiled floor, the chess figure
 * standing on it with the suit's symbol: a coin or a cup held up over it, a sword or a club shouldered behind it */
static void it_court(it_canvas *c, int suit, int rank)
{
    float W = (float)c->w, H = (float)c->h, x0 = 0.215f * W, x1 = 0.785f * W, y0 = 0.055f * H, y1 = 0.945f * H;
    float pw = x1 - x0, ph = y1 - y0, lt = fmaxf(1, 0.012f * W), rad = 0.04f * W;
    const uint8_t *ink = it_ink(suit), *panel = it_rgb(ITC_PANEL);
    uint8_t tint[3];
    for (int i = 0; i < 3; i++) tint[i] = (uint8_t)(panel[i] + (ink[i] - panel[i]) * 0.38f);
    float ar = pw / 2, ay = y0 + ar;            /* the arch: a half circle as wide as the niche */
    it_fill(c, x0, y0, pw, ar + 1, rad, tint, 1);
    it_fill(c, x0, ay, pw, y1 - ay, rad, panel, 1);
    it_pen p;
    it_pen_new(&p);
    if (p.f) art_arc(p.f, ART_PART, W / 2, ay, 0, ar - 0.6f * lt, 0, 180);
    it_grow(&p, W / 2 - ar, ay - ar);
    it_grow(&p, W / 2 + ar, ay + 1);
    art_style sky = it_style(ITC_PANEL, ITC_PANEL, 0, 0);
    it_ink_pen(c, &p, &sky);
    it_pen_new(&p);
    if (p.f) art_arc(p.f, ART_PART, W / 2, ay, ar - 1.6f * lt, ar - 0.3f * lt, 0, 180);
    it_grow(&p, W / 2 - ar, ay - ar);
    it_grow(&p, W / 2 + ar, ay + 1);
    art_style line = sky;
    for (int i = 0; i < 3; i++) line.fill[i] = ink[i];
    it_ink_pen(c, &p, &line);
    float fy = y1 - 0.12f * ph;                 /* the floor: tiles in perspective */
    it_fill(c, x0, fy, pw, y1 - fy, rad, it_rgb(ITC_FLOOR), 1);
    for (int i = 0; i < 3; i++) it_fill(c, x0, fy + (y1 - fy) * i / 3.0f, pw, fmaxf(1, 0.006f * W), 0, ink, 0.3f);
    for (int i = -3; i <= 3; i++) {
        float xa = W / 2 + i * pw / 6, xb = W / 2 + i * pw / 4.2f;
        for (int k = 0; k < 6; k++)
            it_fill(c, xa + (xb - xa) * (k + 0.5f) / 6 - 0.4f, fy + (y1 - fy) * k / 6, fmaxf(0.8f, 0.006f * W),
                    (y1 - fy) / 6, 0, ink, 0.26f);
    }
    it_frame(c, x0, y0, pw, ph, rad, lt, ink, 0.9f);
    int type = rank == IT_FANTE ? PAWN : rank == IT_CAVALLO ? KNIGHT : KING;
    int held = suit == IT_SWORDS || suit == IT_CLUBS;
    float fs = (rank == IT_FANTE ? 0.9f : 0.96f) * W, fx = W / 2 - fs / 2 - (held ? 0.03f * W : 0);
    float ft = fy + 0.055f * ph - 0.905f * fs;
    if (held) {                                 /* shouldered: behind the figure, its top over the right shoulder */
        float len = 0.62f * H, deg = 16, mx = W / 2 + 0.07f * W, my = fy - 0.36f * H;
        if (suit == IT_SWORDS) it_sword(c, mx, my, len, deg, 1.1f, 1);
        else it_club(c, mx, my, len, deg, 1.2f, 3);
    }
    int si = (int)ceilf(fs);
    uint8_t *b = malloc((size_t)si * si * 4);
    if (b) {
        art_piece_style(type, it_figure_style(suit), si, b);
        art_blend(c->o, c->w, c->h, b, si, si, (int)(fx + 0.5f), (int)(ft + 0.5f));
        free(b);
    }
    if (!held) {                                /* held up: over the figure's head, in the arch */
        float head = ft + (type == PAWN ? 0.2f : type == KNIGHT ? 0.12f : 0.06f) * fs, room = head - (y0 + 0.03f * W);
        float s = fminf(suit == IT_COINS ? 0.32f * W : 0.38f * W, 0.95f * room);
        it_sym(c, suit, W / 2, y0 + 0.03f * W + room / 2, s, 0);
    }
}

/* the corner index: the digit over the suit, top left, then again turned round bottom right */
static void it_corner(it_canvas *c, int card)
{
    float W = (float)c->w;
    int suit = CARD_SUIT(card), rank = CARD_RANK(card), cw = (int)ceilf(0.22f * W), ch = (int)ceilf(0.42f * W);
    it_canvas k = {calloc((size_t)cw * ch, 4), cw, ch};
    if (!k.o) return;
    float x = 0.11f * W;
    it_digits(&k, rank, x, 0.13f * W, 0.15f * W, it_ink(suit));
    float s = 0.13f * W, y = 0.325f * W;
    if (suit == IT_SWORDS || suit == IT_CLUBS) it_sym(&k, suit, x, y, 0.2f * W, 0);
    else if (suit == IT_CUPS) it_cup(&k, x, y, 0.15f * W, 0, 0);
    else it_coin(&k, x, y, s, 0);
    art_blend(c->o, c->w, c->h, k.o, cw, ch, 0, 0);
    for (int i = 0, j = cw * ch - 1; i < j; i++, j--) {   /* turned round */
        uint8_t t[4];
        memcpy(t, k.o + 4 * i, 4);
        memcpy(k.o + 4 * i, k.o + 4 * j, 4);
        memcpy(k.o + 4 * j, t, 4);
    }
    art_blend(c->o, c->w, c->h, k.o, cw, ch, c->w - cw, c->h - ch);
    free(k.o);
}

/* a face, w x h (CARD_NONE: nothing; the back is the kit's) */
void cards_italian_face(int card, int w, int h, uint8_t *rgba)
{
    it_canvas c = {rgba, w, h};
    memset(rgba, 0, (size_t)w * h * 4);
    if (card == CARD_NONE) return;
    float W = (float)w, H = (float)h, r = 0.075f * W, b = fmaxf(1, 0.014f * W);
    it_fill(&c, 0, 0, W, H, r, it_rgb(ITC_RIM), 1);
    it_fill(&c, b, b, W - 2 * b, H - 2 * b, r - b, it_rgb(ITC_IVORY), 1);
    int suit = CARD_SUIT(card) & 3, rank = CARD_RANK(card);
    if (w < 52) {                               /* small: the digit big, the symbol under it */
        it_digits(&c, rank, W / 2, 0.3f * H, 0.4f * W, it_ink(suit));
        if (suit == IT_COINS) it_coin(&c, W / 2, 0.7f * H, 0.5f * W, 0);
        else if (suit == IT_CUPS) it_cup(&c, W / 2, 0.7f * H, 0.56f * W, 0, 0);
        else it_sym(&c, suit, W / 2, 0.69f * H, 0.62f * W, 30);
        return;
    }
    /* the settebello, the card every Scopa player is after: framed in gold */
    if (card == CARD(IT_COINS, 7)) {
        float m = 0.034f * W;
        it_frame(&c, m, m, W - 2 * m, H - 2 * m, r - m, fmaxf(1.2f, 0.016f * W), it_rgb(ITC_GOLD), 1);
        it_frame(&c, m, m, W - 2 * m, H - 2 * m, r - m, fmaxf(0.6f, 0.006f * W), it_rgb(ITC_GOLD_D), 0.5f);
    }
    if (rank == IT_ACE) it_ace(&c, suit);
    else if (rank >= IT_FANTE) it_court(&c, suit, rank);
    else it_pips(&c, suit, rank);
    it_corner(&c, card);
}

/* a suit's symbol alone, size x size (icons: the scores) */
void cards_italian_symbol(int suit, int size, uint8_t *rgba)
{
    it_canvas c = {rgba, size, size};
    memset(rgba, 0, (size_t)size * size * 4);
    float s = (float)size;
    if (suit == IT_COINS) it_coin(&c, s / 2, s / 2, 0.92f * s, 0);
    else if (suit == IT_CUPS) it_cup(&c, s / 2, s / 2, 0.96f * s, 0, 0);
    else it_sym(&c, suit, s / 2, s / 2, 1.2f * s, 35);
}

