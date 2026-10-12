/*
 * The card kit's Spanish-suited pack (baraja española: Mus, Truco), drawn by code in the house style: an original
 * rendition, no image files, no letters. The suits: oros, gold coins with a beaded ring and an eight-pointed star;
 * copas, chalices with a red band and a blue knot; espadas, straight steel swords with a gold cross-guard; bastos,
 * knotty cudgels of raw wood with a green grip. The frame line round each face is broken by the pinta, the gaps Spanish
 * players read the suit by at a glance: oros none, copas one, espadas two, bastos three, at the top and at the bottom.
 * The number cards: the coins and cups in columns, the swords and clubs crossed in sheaves (an X of two, a star of
 * three), stacked. The aces one big symbol: the coin with a ring of rays, the chalice with its lid, the sword and the
 * cudgel wound with a red ribbon. The courts single-headed on a tiled floor before a hanging, the house's chess figure in
 * the suit's colour (sota a pawn, caballo a knight, rey a king), the coin or the cup held up beside it, the sword or the
 * cudgel at its shoulder. The corner index: the number (10, 11, 12 on the courts, as the Spanish packs print them) in
 * the suit's ink. Under about 52 px a plainer face: the number big over the symbol, in the broken frame. The back is the
 * house's; the cards are 1.5 times as tall as wide, a little slimmer than the French ones.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "cards.h"
#include "games/chess.h"
#include "text.h"

#define ES_PI 3.14159265f

/* ---- the colours ------------------------------------------------------------------------------------------------- */
enum { EC_FACE, EC_RIM, EC_GOLD, EC_GOLD_D, EC_STAR, EC_RED, EC_RED_D, EC_WINE, EC_BLUE, EC_BLUE_D, EC_STEEL,
       EC_STEEL_D, EC_WOOD, EC_WOOD_D, EC_KNOT, EC_GREEN, EC_GREEN_D, EC_HANG, EC_TILE, EC_N };

static const uint8_t *es_rgb(int k)
{
    static const uint8_t c[EC_N][3] = {
        {250, 245, 233}, {172, 160, 138}, {240, 192, 70}, {126, 78, 22}, {204, 104, 36}, {196, 44, 38},
        {90, 18, 14}, {120, 22, 30}, {52, 96, 178}, {18, 34, 80}, {206, 220, 236}, {44, 60, 96},
        {196, 146, 84}, {92, 56, 26}, {120, 76, 36}, {58, 136, 84}, {16, 56, 34}, {244, 234, 212}, {226, 212, 184},
    };
    return c[k];
}

/* each suit's ink (the index, the frame, the courts' figures): oros ochre, copas red, espadas blue, bastos green */
static const uint8_t *es_ink(int suit)
{
    static const uint8_t c[4][3] = {{172, 104, 14}, {182, 34, 34}, {36, 70, 150}, {30, 110, 62}};
    return c[suit & 3];
}

static const art_style *es_figure(int suit)
{
    static const art_style st[4] = {
        {{230, 170, 56}, {84, 50, 10}, {152, 98, 30}, {252, 238, 204}, 0.34f, 0},
        {{196, 50, 42}, {64, 14, 10}, {238, 152, 132}, {248, 226, 210}, 0.32f, 0},
        {{62, 100, 178}, {14, 24, 58}, {158, 184, 232}, {228, 236, 250}, 0.34f, 0},
        {{56, 134, 92}, {12, 46, 28}, {150, 210, 172}, {222, 242, 230}, 0.34f, 0},
    };
    return &st[suit & 3];
}

/* ---- painting: shapes gathered in pixels, then rendered over their bounds ---------------------------------------- */
typedef struct { uint8_t *px; int w, h; } es_cv;
typedef struct {
    art_fig *f;
    float x0, y0, x1, y1;
    float ox, oy, k, c, s;                      /* where local (u, v) goes: (ox, oy) + k * (u, v) turned */
} es_sh;

static void es_begin(es_sh *g, float ox, float oy, float k, float deg)
{
    g->f = art_fig_new();
    g->x0 = g->y0 = 1e9f;
    g->x1 = g->y1 = -1e9f;
    g->ox = ox, g->oy = oy, g->k = k;
    g->c = cosf(deg * ES_PI / 180), g->s = sinf(deg * ES_PI / 180);
}

static void es_to(const es_sh *g, float u, float v, float *x, float *y)
{
    *x = g->ox + g->k * (u * g->c - v * g->s);
    *y = g->oy + g->k * (u * g->s + v * g->c);
}

static void es_bound(es_sh *g, float x, float y)
{
    g->x0 = fminf(g->x0, x), g->y0 = fminf(g->y0, y);
    g->x1 = fmaxf(g->x1, x), g->y1 = fmaxf(g->y1, y);
}

static void es_poly(es_sh *g, int role, const float *uv, int n)
{
    float q[96];
    if (n > 48) n = 48;
    for (int i = 0; i < n; i++) {
        es_to(g, uv[2 * i], uv[2 * i + 1], &q[2 * i], &q[2 * i + 1]);
        es_bound(g, q[2 * i], q[2 * i + 1]);
    }
    if (g->f) art_poly(g->f, role, q, n);
}

static void es_oval(es_sh *g, int role, float u, float v, float ru, float rv)
{
    float p[2 * 28];
    for (int i = 0; i < 28; i++) {
        float a = 2 * ES_PI * i / 28;
        p[2 * i] = u + ru * cosf(a), p[2 * i + 1] = v + rv * sinf(a);
    }
    es_poly(g, role, p, 28);
}

static void es_rect(es_sh *g, int role, float u0, float v0, float u1, float v1)
{
    const float p[8] = {u0, v0, u1, v0, u1, v1, u0, v1};
    es_poly(g, role, p, 4);
}

static art_style es_style(int fill, int line, float width, float light)
{
    art_style st;
    const uint8_t *f = es_rgb(fill), *l = es_rgb(line);
    for (int i = 0; i < 3; i++) st.fill[i] = f[i], st.outer[i] = st.inner[i] = st.mark[i] = l[i];
    st.light = light;
    st.line = width;
    return st;
}

/* the shapes rendered onto the canvas in a style (over their bounds only); the set emptied for the next ones */
static void es_paint(es_cv *cv, es_sh *g, const art_style *st)
{
    if (!g->f) return;
    float m = st->line + 2;
    int x0 = (int)floorf(fmaxf(0, g->x0 - m)), y0 = (int)floorf(fmaxf(0, g->y0 - m));
    int x1 = (int)ceilf(fminf((float)cv->w, g->x1 + m)), y1 = (int)ceilf(fminf((float)cv->h, g->y1 + m));
    if (x1 > x0 && y1 > y0) {
        uint8_t *b = malloc((size_t)(x1 - x0) * (y1 - y0) * 4);
        if (b) {
            art_render(g->f, st, x1 - x0, y1 - y0, 1, (float)-x0, (float)-y0, b);
            art_blend(cv->px, cv->w, cv->h, b, x1 - x0, y1 - y0, x0, y0);
            free(b);
        }
    }
    art_fig_free(g->f);
    es_begin(g, g->ox, g->oy, g->k, atan2f(g->s, g->c) * 180 / ES_PI);
}

static void es_end(es_sh *g)
{
    art_fig_free(g->f);
    g->f = NULL;
}

/* a plain rounded rectangle (alpha 0..1) */
static void es_block(es_cv *cv, float x, float y, float w, float h, float r, const uint8_t rgb[3], float a)
{
    int iw = (int)(w + 0.5f), ih = (int)(h + 0.5f);
    if (iw < 1 || ih < 1) return;
    const uint8_t col[4] = {rgb[0], rgb[1], rgb[2], (uint8_t)(255 * fminf(1, fmaxf(0, a)))};
    uint8_t *b = malloc((size_t)iw * ih * 4);
    if (!b) return;
    art_roundrect(iw, ih, r, col, b);
    art_blend(cv->px, cv->w, cv->h, b, iw, ih, (int)(x + 0.5f), (int)(y + 0.5f));
    free(b);
}

/* a pixel-exact line segment along x or y (the frame), a soft edge */
static void es_hline(es_cv *cv, float x0, float x1, float y, float t, const uint8_t rgb[3])
{
    for (int j = (int)floorf(y - t / 2); j <= (int)ceilf(y + t / 2); j++)
        for (int i = (int)floorf(x0); i <= (int)ceilf(x1); i++) {
            if (i < 0 || j < 0 || i >= cv->w || j >= cv->h) continue;
            float cx = fminf(1, fmaxf(0, fminf(i + 1 - x0, x1 - i)));
            float cy = fminf(1, fmaxf(0, fminf(j + 1 - (y - t / 2), y + t / 2 - j)));
            uint8_t p[4] = {rgb[0], rgb[1], rgb[2], (uint8_t)(255 * cx * cy)};
            if (p[3]) art_blend(cv->px, cv->w, cv->h, p, 1, 1, i, j);
        }
}

static void es_vline(es_cv *cv, float x, float y0, float y1, float t, const uint8_t rgb[3])
{
    for (int i = (int)floorf(x - t / 2); i <= (int)ceilf(x + t / 2); i++)
        for (int j = (int)floorf(y0); j <= (int)ceilf(y1); j++) {
            if (i < 0 || j < 0 || i >= cv->w || j >= cv->h) continue;
            float cy = fminf(1, fmaxf(0, fminf(j + 1 - y0, y1 - j)));
            float cx = fminf(1, fmaxf(0, fminf(i + 1 - (x - t / 2), x + t / 2 - i)));
            uint8_t p[4] = {rgb[0], rgb[1], rgb[2], (uint8_t)(255 * cx * cy)};
            if (p[3]) art_blend(cv->px, cv->w, cv->h, p, 1, 1, i, j);
        }
}

/* the number from the font, bold, centred on (cx, cy), hgt tall; a lone 6 or 9 underlined */
static void es_number(es_cv *cv, int v, float cx, float cy, float hgt, const uint8_t rgb[3])
{
    char d[2] = {(char)('0' + (v >= 10 ? v / 10 : v)), (char)('0' + v % 10)};
    int n = v >= 10 ? 2 : 1, bw = (int)ceilf(0.9f * hgt) + 2, bh = (int)ceilf(1.3f * hgt) + 2;
    float step = 0.55f * hgt;
    uint8_t *b = malloc((size_t)bw * bh * 4);
    if (!b) return;
    for (int i = 0; i < n; i++) {
        memset(b, 0, (size_t)bw * bh * 4);
        if (!text_glyph(d[i], 1.2f * hgt, 1, rgb, b, bw, bh)) continue;
        float x = cx + (i - (n - 1) / 2.0f) * step - bw / 2.0f, y = cy - bh / 2.0f;
        art_blend(cv->px, cv->w, cv->h, b, bw, bh, (int)floorf(x + 0.5f), (int)floorf(y + 0.5f));
    }
    free(b);
    if (v == 6 || v == 9) es_block(cv, cx - 0.22f * hgt, cy + 0.63f * hgt, 0.44f * hgt, fmaxf(1.5f, 0.1f * hgt), 0, rgb, 1);
}

/* ---- the suits' symbols ------------------------------------------------------------------------------------------ */
static float es_lw(float s) { return fminf(2.2f, fmaxf(0.8f, 0.04f * s)); }

/* an oro: a gold coin s wide, a dark ring inside its edge, beads round it (big enough), an eight-pointed star in the
 * middle; rich (the as): a ring of rays round it */
static void es_coin(es_cv *cv, float x, float y, float s, int rich)
{
    float r = s / 2, lw = es_lw(s);
    es_sh g;
    if (rich) {
        es_begin(&g, x, y, r, 0);
        for (int i = 0; i < 24; i++) {
            float a = 2 * ES_PI * i / 24, len = i & 1 ? 1.12f : 1.24f, wd = 0.05f;
            float ca = cosf(a), sa = sinf(a);
            const float ray[6] = {0.9f * ca - wd * sa, 0.9f * sa + wd * ca, len * ca, len * sa, 0.9f * ca + wd * sa,
                                  0.9f * sa - wd * ca};
            es_poly(&g, ART_PART, ray, 3);
        }
        art_style st = es_style(EC_GOLD, EC_GOLD_D, 0.7f * lw, 0.1f);
        es_paint(cv, &g, &st);
        es_end(&g);
        r *= 0.86f;
    }
    es_begin(&g, x, y, r, 0);
    es_oval(&g, ART_PART, 0, 0, 1, 1);
    art_style gold = es_style(EC_GOLD, EC_GOLD_D, lw, 0.4f);
    es_paint(cv, &g, &gold);
    if (s >= 22) {                              /* the inner ring */
        if (g.f) art_arc(g.f, ART_PART, x, y, 0.74f * r, 0.79f * r, 0, 360);
        es_bound(&g, x - r, y - r), es_bound(&g, x + r, y + r);
        art_style ring = es_style(EC_GOLD_D, EC_GOLD_D, 0, 0);
        es_paint(cv, &g, &ring);
    }
    if (s >= 40) {                              /* beads between the ring and the edge */
        for (int i = 0; i < 16; i++) {
            float a = 2 * ES_PI * (i + 0.5f) / 16;
            es_oval(&g, ART_PART, 0.885f * cosf(a), 0.885f * sinf(a), 0.05f, 0.05f);
        }
        art_style bead = es_style(EC_GOLD_D, EC_GOLD_D, 0, 0);
        es_paint(cv, &g, &bead);
    }
    float star[2 * 16];                         /* the star, its points between the beads */
    for (int i = 0; i < 16; i++) {
        float a = 2 * ES_PI * i / 16 - ES_PI / 2, rr = i & 1 ? 0.27f : 0.6f;
        star[2 * i] = rr * cosf(a), star[2 * i + 1] = rr * sinf(a);
    }
    es_poly(&g, ART_PART, star, 16);
    art_style st = es_style(EC_STAR, EC_RED_D, s >= 22 ? 0.6f * lw : 0, 0.25f);
    es_paint(cv, &g, &st);
    es_oval(&g, ART_PART, 0, 0, 0.16f, 0.16f);
    es_paint(cv, &g, &gold);
    es_end(&g);
}

/* a copa: a chalice s tall, its bowl wide at the rim, a red band round it, a blue knot on the stem, a stepped foot;
 * rich (the as): a domed lid with a knob, gems on the band */
static void es_cup(es_cv *cv, float x, float y, float s, float deg, int rich)
{
    float lw = es_lw(s);
    art_style gold = es_style(EC_GOLD, EC_GOLD_D, lw, 0.42f), red = es_style(EC_RED, EC_RED_D, 0.8f * lw, 0.25f);
    art_style blue = es_style(EC_BLUE, EC_BLUE_D, 0.8f * lw, 0.45f), wine = es_style(EC_WINE, EC_GOLD_D, 0.6f * lw, 0);
    float top = rich ? -0.26f : -0.42f, half = rich ? 0.3f : 0.33f;
    es_sh g;
    es_begin(&g, x, y, s, deg);
    const float foot[] = {-0.24f, 0.5f, 0.24f, 0.5f, 0.24f, 0.45f, 0.15f, 0.42f, 0.09f, 0.36f, -0.09f, 0.36f,
                          -0.15f, 0.42f, -0.24f, 0.45f};
    es_poly(&g, ART_PART, foot, 8);
    es_rect(&g, ART_PART, -0.045f, 0.0f, 0.045f, 0.37f);
    es_paint(cv, &g, &gold);
    es_oval(&g, ART_PART, 0, 0.2f, 0.085f, 0.06f);   /* the knot */
    es_paint(cv, &g, &blue);
    float bowl[2 * 22];                         /* the bowl: wide at the rim, narrowing in a curve to the stem */
    int n = 0;
    for (int i = 0; i <= 10; i++) {
        float t = i / 10.0f, w = 0.06f + (half - 0.06f) * (1 - t * t * t) * (0.65f + 0.35f * (1 - t));
        bowl[n++] = -w, bowl[n++] = top + (0.04f - top) * t;
    }
    for (int i = 10; i >= 0; i--) {
        float t = i / 10.0f, w = 0.06f + (half - 0.06f) * (1 - t * t * t) * (0.65f + 0.35f * (1 - t));
        bowl[n++] = w, bowl[n++] = top + (0.04f - top) * t;
    }
    es_poly(&g, ART_PART, bowl, n / 2);
    es_paint(cv, &g, &gold);
    if (s >= 24) {                              /* the red band, a third of the way down */
        float b0 = top + 0.3f * (0.04f - top), b1 = top + 0.45f * (0.04f - top), wa = 0, wb = 0;
        for (int k = 0; k < 2; k++) {
            float t = (k ? b1 - top : b0 - top) / (0.04f - top);
            float w = 0.06f + (half - 0.06f) * (1 - t * t * t) * (0.65f + 0.35f * (1 - t)) - 0.01f;
            if (k) wb = w;
            else wa = w;
        }
        const float band[] = {-wa, b0, wa, b0, wb, b1, -wb, b1};
        es_poly(&g, ART_PART, band, 4);
        es_paint(cv, &g, &red);
        if (rich) {
            for (int i = -1; i <= 1; i++) es_oval(&g, ART_PART, 0.13f * i, (b0 + b1) / 2, 0.03f, 0.028f);
            es_paint(cv, &g, &blue);
        }
    }
    if (rich) {                                 /* the lid: a dome, a knob, over the rim */
        float dome[2 * 14];
        for (int i = 0; i <= 12; i++) {
            float a = ES_PI * i / 12;
            dome[2 * i] = -0.29f * cosf(a), dome[2 * i + 1] = top - 0.02f - 0.17f * sinf(a);
        }
        es_poly(&g, ART_PART, dome, 13);
        es_rect(&g, ART_PART, -0.32f, top - 0.04f, 0.32f, top + 0.01f);
        es_paint(cv, &g, &gold);
        es_oval(&g, ART_PART, 0, top - 0.25f, 0.06f, 0.06f);
        es_paint(cv, &g, &red);
        es_end(&g);
        return;
    }
    es_oval(&g, ART_PART, 0, top, half, 0.055f);   /* the rim, the wine in it */
    es_paint(cv, &g, &gold);
    es_oval(&g, ART_PART, 0, top + 0.004f, half - 0.06f, 0.032f);
    es_paint(cv, &g, &wine);
    es_end(&g);
}

/* an espada: a straight steel sword s long, point up, turned deg: a broad blade with a fuller, a gold cross-guard
 * with round ends, a red grip, a gold pommel; wide: the blade's breadth */
static void es_sword(es_cv *cv, float x, float y, float s, float deg, float wide)
{
    float lw = fminf(2.0f, fmaxf(0.8f, 0.016f * s)), bw = 0.036f * wide, gw = 0.15f * fminf(1.3f, 0.6f + 0.4f * wide);
    art_style steel = es_style(EC_STEEL, EC_STEEL_D, lw, 0.4f), gold = es_style(EC_GOLD, EC_GOLD_D, lw, 0.35f);
    art_style red = es_style(EC_RED, EC_RED_D, lw, 0.2f);
    steel.mark[0] = 110, steel.mark[1] = 132, steel.mark[2] = 168;
    es_sh g;
    es_begin(&g, x, y, s, deg);
    const float blade[] = {-bw, 0.2f, -bw, -0.4f, 0, -0.5f, bw, -0.4f, bw, 0.2f};
    es_poly(&g, ART_PART, blade, 5);
    if (s >= 44) {
        const float fuller[] = {-0.22f * bw, 0.16f, -0.22f * bw, -0.37f, 0.22f * bw, -0.37f, 0.22f * bw, 0.16f};
        es_poly(&g, ART_MARK, fuller, 4);
    }
    es_paint(cv, &g, &steel);
    es_rect(&g, ART_PART, -0.026f * wide, 0.22f, 0.026f * wide, 0.41f);   /* the grip */
    es_paint(cv, &g, &red);
    const float guard[] = {-gw, 0.2f, gw, 0.2f, gw, 0.24f, -gw, 0.24f};
    es_poly(&g, ART_PART, guard, 4);
    es_oval(&g, ART_PART, -gw, 0.22f, 0.032f, 0.032f);
    es_oval(&g, ART_PART, gw, 0.22f, 0.032f, 0.032f);
    es_oval(&g, ART_PART, 0, 0.44f, 0.042f * fminf(1.4f, wide), 0.042f);
    es_paint(cv, &g, &gold);
    es_end(&g);
}

/* a basto: a knotty cudgel s long, head up, turned deg: thick and rounded at the head, tapering to the grip, bumps
 * where branches were cut (their stubs showing the wood's light end), a green band at the grip */
static void es_club(es_cv *cv, float x, float y, float s, float deg, float wide)
{
    float lw = fminf(2.0f, fmaxf(0.8f, 0.016f * s));
    art_style wood = es_style(EC_WOOD, EC_WOOD_D, lw, 0.45f), knot = es_style(EC_KNOT, EC_WOOD_D, 0.8f * lw, 0.2f);
    art_style green = es_style(EC_GREEN, EC_GREEN_D, lw, 0.3f), cut = es_style(EC_GOLD, EC_WOOD_D, 0.6f * lw, 0);
    es_sh g;
    es_begin(&g, x, y, s, deg);
    static const float bump[4][3] = {{1, -0.2f, 1}, {-1, -0.05f, 0.9f}, {1, 0.12f, 0.8f}, {-1, -0.33f, 0.85f}};
    int nb = s >= 30 ? 4 : 2;
    for (int k = 0; k < nb; k++) {              /* the cut branches, behind the cudgel */
        float sd = bump[k][0], v = bump[k][1], r = 0.03f * wide * bump[k][2];
        float bx = sd * (0.07f - 0.04f * (v + 0.5f)) * wide;
        const float st[] = {bx, v - r, bx + sd * 0.07f * wide, v - 0.05f - r, bx + sd * 0.085f * wide, v - 0.02f,
                            bx, v + r};
        es_poly(&g, ART_PART, st, 4);
    }
    es_paint(cv, &g, &wood);
    float body[2 * 44];                         /* the cudgel: a wavy outline, thick at the head */
    int n = 0;
    for (int i = 0; i <= 10; i++) {             /* the head */
        float a = ES_PI + ES_PI * i / 10;
        body[n++] = 0.085f * wide * cosf(a), body[n++] = -0.41f + 0.07f * sinf(a);
    }
    for (int i = 0; i <= 8; i++) {              /* the right side down, swelling at the knots */
        float v = -0.41f + 0.86f * i / 8, w = (0.085f - 0.045f * (v + 0.41f) / 0.86f) * wide;
        body[n++] = w * (1 + 0.12f * (i & 1)), body[n++] = v;
    }
    for (int i = 1; i < 6; i++) {               /* the butt */
        float a = ES_PI * i / 6;
        body[n++] = 0.04f * wide * cosf(a), body[n++] = 0.45f + 0.025f * sinf(a);
    }
    for (int i = 8; i >= 0; i--) {
        float v = -0.41f + 0.86f * i / 8, w = (0.085f - 0.045f * (v + 0.41f) / 0.86f) * wide;
        body[n++] = -w * (1 + 0.12f * !(i & 1)), body[n++] = v;
    }
    es_poly(&g, ART_PART, body, n / 2);
    es_paint(cv, &g, &wood);
    if (s >= 30) {
        for (int k = 0; k < nb; k++) {          /* knots on the wood, the stubs' cut ends */
            float sd = bump[k][0], v = bump[k][1];
            float bx = sd * (0.07f - 0.04f * (v + 0.5f)) * wide;
            es_oval(&g, ART_PART, -sd * 0.025f * wide, v + 0.09f, 0.012f * wide, 0.018f);
            (void)bx;
        }
        es_paint(cv, &g, &knot);
        for (int k = 0; k < nb; k++) {
            float sd = bump[k][0], v = bump[k][1];
            float bx = sd * (0.07f - 0.04f * (v + 0.5f)) * wide;
            es_oval(&g, ART_PART, bx + sd * 0.078f * wide, v - 0.035f, 0.016f * wide, 0.024f);
        }
        es_paint(cv, &g, &cut);
    }
    es_rect(&g, ART_PART, -0.05f * wide, 0.3f, 0.05f * wide, 0.36f);   /* the grip's band */
    es_paint(cv, &g, &green);
    es_end(&g);
}

/* a glory of rays behind the aces of espadas and bastos (the strongest cards of Truco): r its radius */
static void es_glory(es_cv *cv, float x, float y, float r)
{
    es_sh g;
    es_begin(&g, x, y, r, 0);
    for (int i = 0; i < 20; i++) {
        float a = 2 * ES_PI * (i + 0.5f) / 20, len = i & 1 ? 0.78f : 1.0f, wd = 0.07f;
        float ca = cosf(a), sa = sinf(a);
        const float ray[6] = {0.3f * ca - wd * sa, 0.3f * sa + wd * ca, len * ca, len * sa, 0.3f * ca + wd * sa,
                              0.3f * sa - wd * ca};
        es_poly(&g, ART_PART, ray, 3);
    }
    art_style st = es_style(EC_TILE, EC_GOLD_D, fmaxf(0.8f, 0.01f * r), 0.15f);
    st.fill[0] = 244, st.fill[1] = 206, st.fill[2] = 110;
    es_paint(cv, &g, &st);
    es_oval(&g, ART_PART, 0, 0, 0.36f, 0.36f);
    st.fill[0] = 250, st.fill[1] = 236, st.fill[2] = 196;
    es_paint(cv, &g, &st);
    es_end(&g);
}

/* a red bow tied round a grip (the aces): its knot at (x, y), s across */
static void es_bow(es_cv *cv, float x, float y, float s)
{
    art_style red = es_style(EC_RED, EC_RED_D, fmaxf(0.8f, 0.03f * s), 0.3f);
    es_sh g;
    es_begin(&g, x, y, s, 0);
    const float loop_l[] = {-0.06f, -0.05f, -0.42f, -0.26f, -0.5f, -0.08f, -0.42f, 0.1f, -0.06f, 0.05f};
    const float loop_r[] = {0.06f, -0.05f, 0.42f, -0.26f, 0.5f, -0.08f, 0.42f, 0.1f, 0.06f, 0.05f};
    const float tail_l[] = {-0.04f, 0.04f, -0.3f, 0.5f, -0.16f, 0.44f, -0.08f, 0.56f, 0.04f, 0.06f};
    const float tail_r[] = {0.04f, 0.04f, 0.26f, 0.52f, 0.14f, 0.46f, 0.1f, 0.6f, -0.02f, 0.06f};
    es_poly(&g, ART_PART, tail_l, 5);
    es_poly(&g, ART_PART, tail_r, 5);
    es_poly(&g, ART_PART, loop_l, 5);
    es_poly(&g, ART_PART, loop_r, 5);
    es_paint(cv, &g, &red);
    es_oval(&g, ART_PART, 0, 0, 0.09f, 0.08f);
    es_paint(cv, &g, &red);
    es_end(&g);
}

/* a suit's symbol centred on (x, y), s tall (an oro: s wide), turned deg (the long ones) */
static void es_sym(es_cv *cv, int suit, float x, float y, float s, float deg)
{
    switch (suit & 3) {
    case ES_OROS: es_coin(cv, x, y, s, 0); break;
    case ES_COPAS: es_cup(cv, x, y, s, deg, 0); break;
    case ES_ESPADAS: es_sword(cv, x, y, s, deg, 1); break;
    default: es_club(cv, x, y, s, deg, 1); break;
    }
}

/* ---- the faces --------------------------------------------------------------------------------------------------- */
typedef struct { float x0, y0, x1, y1; } es_area;   /* the room inside the frame, clear of the corner numbers */

/* the frame line: inset from the edge, in the suit's ink, broken by the pinta (oros 0, copas 1, espadas 2, bastos 3
 * gaps, at the top and at the bottom) */
static void es_frame(es_cv *cv, int suit, float m, float t)
{
    float W = (float)cv->w, H = (float)cv->h, x0 = m, x1 = W - m, y0 = m, y1 = H - m;
    const uint8_t *ink = es_ink(suit);
    int gaps = suit & 3;
    float gw = fmaxf(2.0f, 0.075f * W), at[4];
    for (int i = 0; i < gaps; i++) at[i] = W / 2 + (i - (gaps - 1) / 2.0f) * 0.15f * W;
    for (int e = 0; e < 2; e++) {
        float y = e ? y1 : y0, a = x0;
        for (int i = 0; i <= gaps; i++) {
            float b = i < gaps ? at[i] - gw / 2 : x1;
            es_hline(cv, a, b, y, t, ink);
            if (i < gaps) a = at[i] + gw / 2;
        }
    }
    es_vline(cv, x0, y0 - t / 2, y1 + t / 2, t, ink);
    es_vline(cv, x1, y0 - t / 2, y1 + t / 2, t, ink);
}

/* the number cards: oros and copas in columns, espadas and bastos in crossed sheaves */
static void es_pips(es_cv *cv, int suit, int n, es_area a)
{
    float W = (float)cv->w, cx = (a.x0 + a.x1) / 2, aw = a.x1 - a.x0, ah = a.y1 - a.y0;
    if (suit == ES_OROS || suit == ES_COPAS) {
        /* where each goes: x across (-1..1 of the half width), y down (0..1), and the size (of the height) */
        static const float lay[10][10][2] = {
            {{0}}, {{0, 0.5f}}, {{0, 0.22f}, {0, 0.78f}}, {{0, 0.16f}, {0, 0.5f}, {0, 0.84f}},
            {{-0.5f, 0.22f}, {0.5f, 0.22f}, {-0.5f, 0.78f}, {0.5f, 0.78f}},
            {{-0.5f, 0.18f}, {0.5f, 0.18f}, {0, 0.5f}, {-0.5f, 0.82f}, {0.5f, 0.82f}},
            {{-0.5f, 0.16f}, {0.5f, 0.16f}, {-0.5f, 0.5f}, {0.5f, 0.5f}, {-0.5f, 0.84f}, {0.5f, 0.84f}},
            {{-0.52f, 0.13f}, {0.52f, 0.13f}, {0, 0.3f}, {-0.52f, 0.5f}, {0.52f, 0.5f}, {-0.52f, 0.87f},
             {0.52f, 0.87f}},
            {{-0.52f, 0.13f}, {0.52f, 0.13f}, {0, 0.3f}, {-0.52f, 0.5f}, {0.52f, 0.5f}, {0, 0.7f}, {-0.52f, 0.87f},
             {0.52f, 0.87f}},
            {{-0.62f, 0.16f}, {0, 0.16f}, {0.62f, 0.16f}, {-0.62f, 0.5f}, {0, 0.5f}, {0.62f, 0.5f}, {-0.62f, 0.84f},
             {0, 0.84f}, {0.62f, 0.84f}},
        };
        static const float size[10] = {0, 0.4f, 0.36f, 0.27f, 0.3f, 0.26f, 0.25f, 0.21f, 0.21f, 0.2f};
        float s = size[n] * ah * (suit == ES_OROS ? 0.8f : 0.96f);
        s = fminf(s, (n <= 3 ? 0.8f : n == 9 ? 0.3f : 0.46f) * aw);
        for (int i = 0; i < n; i++) es_sym(cv, suit, cx + lay[n][i][0] * aw / 2, a.y0 + lay[n][i][1] * ah, s, 0);
        (void)W;
        return;
    }
    /* sheaves stacked from the top: 2 an X, 3 a star (an X and one upright through it) */
    static const int8_t sheaves[10][4] = {{0}, {1}, {2}, {3}, {2, 2}, {3, 2}, {3, 3}, {2, 3, 2}, {3, 2, 3},
                                          {3, 3, 3}};
    int rows = 0;
    while (rows < 3 && sheaves[n][rows]) rows++;
    float rh = ah / rows;
    for (int r = 0; r < rows; r++) {
        int m = sheaves[n][r];
        float y = a.y0 + rh * (r + 0.5f), lean = m == 1 ? 0 : rows == 1 ? 26 : rows == 2 ? 32 : 40;
        /* as long as the row and the width allow */
        float L = fminf(rh / cosf(lean * ES_PI / 180) * (rows == 1 ? 0.98f : 0.93f),
                        lean > 0 ? aw / sinf(lean * ES_PI / 180) * 0.92f : rh);
        if (m == 1) L = rh * 0.98f;
        float wide = rows == 1 ? 1.15f : rows == 2 ? 1.0f : 0.9f;
        for (int i = 0; i < m; i++) {
            float deg = m == 1 ? 0 : i == 0 ? -lean : i == 1 ? lean : 0;
            float len = i == 2 ? fminf(L, rh * 0.98f) : L;
            if (suit == ES_ESPADAS) es_sword(cv, cx, y, len, deg, wide);
            else es_club(cv, cx, y, len, deg, (rows == 1 ? 1.1f : 0.9f) * wide);
        }
    }
}

/* the as: one big symbol */
static void es_ace(es_cv *cv, int suit, es_area a)
{
    float cx = (a.x0 + a.x1) / 2, cy = (a.y0 + a.y1) / 2, aw = a.x1 - a.x0, ah = a.y1 - a.y0;
    switch (suit) {
    case ES_OROS: es_coin(cv, cx, cy, 0.84f * aw, 1); break;
    case ES_COPAS: es_cup(cv, cx, cy + 0.04f * ah, fminf(0.86f * ah, 1.5f * aw), 0, 1); break;
    case ES_ESPADAS:
        es_glory(cv, cx, cy - 0.1f * ah, 0.66f * aw);
        es_sword(cv, cx, cy, 0.98f * ah, 0, 1.9f);
        es_bow(cv, cx, cy + 0.3f * ah, 0.42f * aw);
        break;
    default:
        es_glory(cv, cx, cy - 0.12f * ah, 0.66f * aw);
        es_club(cv, cx, cy, 0.9f * ah, 0, 1.45f);
        es_bow(cv, cx, cy + 0.27f * ah, 0.46f * aw);
    }
}

/* a court: a hanging behind (the suit's colour, faint, in folds), a floor of tiles, the chess figure standing on it,
 * the coin or the cup held up beside its head, the sword or the cudgel at its shoulder */
static void es_court(es_cv *cv, int suit, int rank, es_area a)
{
    float W = (float)cv->w, aw = a.x1 - a.x0, ah = a.y1 - a.y0;
    const uint8_t *ink = es_ink(suit), *hang = es_rgb(EC_HANG);
    uint8_t tint[3];
    for (int i = 0; i < 3; i++) tint[i] = (uint8_t)(hang[i] + (ink[i] - hang[i]) * 0.22f);
    float px0 = a.x0, px1 = a.x1, py0 = a.y0, py1 = a.y1, fy = py1 - 0.14f * ah;
    es_block(cv, px0, py0, aw, fy - py0, 0, tint, 1);
    for (int sd = 0; sd < 2; sd++) {            /* the drapes, gathered at each side */
        es_sh g;
        es_begin(&g, sd ? px1 : px0, py0, aw, 0);
        float k = sd ? -1.0f : 1.0f, dr[2 * 14];
        int n = 0;
        dr[n++] = 0, dr[n++] = 0;
        for (int i = 0; i <= 10; i++) {         /* its inner edge: in, then back out to the floor */
            float t = i / 10.0f, v = t * (fy - py0) / aw;
            dr[n++] = k * (0.2f - 0.13f * sinf(t * ES_PI * 0.8f)), dr[n++] = v;
        }
        dr[n++] = 0, dr[n++] = (fy - py0) / aw;
        es_poly(&g, ART_PART, dr, n / 2);
        art_style st = es_style(EC_HANG, EC_HANG, fmaxf(0.8f, 0.006f * W), 0.5f);
        for (int c = 0; c < 3; c++)
            st.fill[c] = (uint8_t)(hang[c] + (ink[c] - hang[c]) * 0.6f), st.outer[c] = (uint8_t)(ink[c] * 0.7f);
        es_paint(cv, &g, &st);
        es_end(&g);
    }
    for (int i = 0; i < 7; i++) {               /* the hanging's scalloped edge along the top */
        float x = px0 + aw * (i + 0.5f) / 7;
        es_sh g;
        es_begin(&g, x, py0, aw / 7, 0);
        float sc[2 * 13];
        for (int k = 0; k <= 12; k++) {
            float t = ES_PI * k / 12;
            sc[2 * k] = 0.5f * cosf(t), sc[2 * k + 1] = 0.32f * sinf(t);
        }
        es_poly(&g, ART_PART, sc, 13);
        art_style st = es_style(EC_HANG, EC_HANG, 0, 0);
        for (int c = 0; c < 3; c++) st.fill[c] = ink[c], st.outer[c] = ink[c];
        es_paint(cv, &g, &st);
        es_end(&g);
    }
    /* the floor: tiles in two rows, every other one in the suit's ink */
    es_block(cv, px0, fy, aw, py1 - fy, 0, es_rgb(EC_TILE), 1);
    int cols = 6;
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < cols; c++)
            if ((r + c) & 1) es_block(cv, px0 + aw * c / cols, fy + (py1 - fy) * r / 2, aw / cols, (py1 - fy) / 2,
                                      0, ink, 0.35f);
    es_hline(cv, px0, px1, fy, fmaxf(1, 0.008f * W), ink);
    float bt = fmaxf(1, 0.008f * W);            /* the panel's thin edge */
    es_hline(cv, px0, px1, py0, bt, ink), es_hline(cv, px0, px1, py1, bt, ink);
    es_vline(cv, px0, py0, py1, bt, ink), es_vline(cv, px1, py0, py1, bt, ink);
    int type = rank == ES_SOTA ? PAWN : rank == ES_CABALLO ? KNIGHT : KING;
    int held = suit == ES_ESPADAS || suit == ES_BASTOS;
    float fs = fminf(1.0f * aw, 0.8f * ah) * (rank == ES_SOTA ? 0.92f : 1.0f);
    float fx = (px0 + px1) / 2 - fs / 2 - (held ? 0.05f * aw : 0.08f * aw), ft = fy + 0.06f * fs - 0.905f * fs;
    if (held) {                                 /* at the shoulder: behind the figure, its top over the right */
        float len = 0.66f * ah, mx = (px0 + px1) / 2 + 0.2f * aw, my = fy - 0.36f * ah;
        if (suit == ES_ESPADAS) es_sword(cv, mx, my, len, 12, 1.1f);
        else es_club(cv, mx, my, len, 12, 1.2f);
    }
    int si = (int)ceilf(fs);
    uint8_t *b = malloc((size_t)si * si * 4);
    if (b) {
        art_piece_style(type, es_figure(suit), si, b);
        art_blend(cv->px, cv->w, cv->h, b, si, si, (int)(fx + 0.5f), (int)(ft + 0.5f));
        free(b);
    }
    if (!held) {                                /* held up beside the head, to the right */
        float s = suit == ES_OROS ? 0.34f * aw : 0.42f * aw;
        float sx = px1 - 0.06f * aw - s / 2, sy = ft + (type == PAWN ? 0.24f : 0.16f) * fs;
        es_sym(cv, suit, sx, fmaxf(sy, py0 + 0.6f * s), s, 0);
    }
}

void cards_spanish_face(int card, int w, int h, uint8_t *rgba)
{
    es_cv cv = {rgba, w, h};
    memset(rgba, 0, (size_t)w * h * 4);
    if (card == CARD_NONE) return;
    float W = (float)w, H = (float)h, r = 0.075f * W, b = fmaxf(1, 0.014f * W);
    es_block(&cv, 0, 0, W, H, r, es_rgb(EC_RIM), 1);
    es_block(&cv, b, b, W - 2 * b, H - 2 * b, r - b, es_rgb(EC_FACE), 1);
    int suit = CARD_SUIT(card) & 3, rank = CARD_RANK(card);
    const uint8_t *ink = es_ink(suit);
    if (w < 52) {                               /* small: the number big over the symbol, in the broken frame */
        es_frame(&cv, suit, 0.09f * W, fmaxf(1, 0.03f * W));
        es_number(&cv, rank, W / 2, 0.31f * H, 0.38f * W, ink);
        if (suit == ES_OROS) es_coin(&cv, W / 2, 0.69f * H, 0.5f * W, 0);
        else if (suit == ES_COPAS) es_cup(&cv, W / 2, 0.69f * H, 0.5f * W, 0, 0);
        else es_sym(&cv, suit, W / 2, 0.69f * H, 0.6f * W, 32);
        return;
    }
    float m = 0.07f * W;
    es_frame(&cv, suit, m, fmaxf(1, 0.014f * W));
    /* the numbers in the frame's corners, top left and (turned) bottom right */
    float nh = 0.15f * W, nx = m + 0.04f * W + 0.3f * nh * (rank >= 10 ? 2 : 1), ny = m + 0.04f * W + 0.62f * nh;
    es_number(&cv, rank, nx, ny, nh, ink);
    {
        int cw = (int)ceilf(2 * nx), ch = (int)ceilf(2 * ny);
        es_cv k = {calloc((size_t)cw * ch, 4), cw, ch};
        if (k.px) {
            es_number(&k, rank, nx, ny, nh, ink);
            for (int i = 0, j = cw * ch - 1; i < j; i++, j--)
                for (int c = 0; c < 4; c++) {
                    uint8_t t = k.px[4 * i + c];
                    k.px[4 * i + c] = k.px[4 * j + c];
                    k.px[4 * j + c] = t;
                }
            art_blend(rgba, w, h, k.px, cw, ch, w - cw, h - ch);
            free(k.px);
        }
    }
    float side = m + 0.05f * W, corner = ny + 0.72f * nh;
    es_area a = {side, m + 0.06f * W, W - side, H - m - 0.06f * W};
    if (rank == ES_AS) {
        float tall = suit == ES_ESPADAS || suit == ES_BASTOS ? m + 0.07f * W : corner;   /* the long ones: past the numbers */
        es_area big = {m + 0.1f * W, tall, W - m - 0.1f * W, H - tall};
        es_ace(&cv, suit, big);
    } else if (rank >= ES_SOTA) {
        es_area in = {m + 0.035f * W, corner, W - m - 0.035f * W, H - corner};
        es_court(&cv, suit, rank, in);
    } else {
        float sx = suit == ES_OROS || suit == ES_COPAS ? 0.11f : 0.19f;
        a.x0 = m + sx * W, a.x1 = W - m - sx * W, a.y0 = m + 0.08f * W, a.y1 = H - m - 0.08f * W;
        es_pips(&cv, suit, rank, a);
    }
}

void cards_spanish_symbol(int suit, int size, uint8_t *rgba)
{
    es_cv cv = {rgba, size, size};
    memset(rgba, 0, (size_t)size * size * 4);
    float s = (float)size;
    if (suit == ES_OROS) es_coin(&cv, s / 2, s / 2, 0.92f * s, 0);
    else if (suit == ES_COPAS) es_cup(&cv, s / 2, s / 2, 0.96f * s, 0, 0);
    else es_sym(&cv, suit, s / 2, s / 2, 1.22f * s, 38);
}
