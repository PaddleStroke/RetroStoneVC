/*
 * RetroStone Board's card and tile kit (cards.h): the faces and backs painted by code at the size they are shown,
 * a texture cache, cards drawn turned, raised, flipping, hands fanned, piles, slides.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A face is painted in pixels on a w x h canvas (straight alpha, like art.c): the card's body, then figures (suit
 * symbols, chess figures, icons) rendered by art_render in their unit square and blended on, and digits from the
 * font (text_glyph, help/glyphs.txt keeps them). The corner index is painted once and blended again turned round
 * (bottom right). The tarot's trumps, excuse and courts are pictures: the 1898 "Tarot nouveau" (art/tarot, embedded
 * by tools/gen_art.py), decoded (stb_image) and scaled to the card when its face is painted (the texture cache keeps
 * the result).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/chess.h"
#include "shell.h"
#include "text.h"

#define STB_IMAGE_IMPLEMENTATION                /* the tarot's pictures: JPEG only, from memory */
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"     /* the library's parts this kit does not use */
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "stb_image.h"
#pragma GCC diagnostic pop

#define PI_F 3.14159265f

typedef struct { uint8_t *o; int w, h; } canvas;

static const uint8_t IVORY[3] = {250, 246, 236}, RIM[3] = {176, 164, 140}, CREAM[3] = {246, 238, 220},
                     INK[3] = {40, 30, 28}, GOLD[3] = {214, 168, 66}, WALNUT[3] = {92, 60, 40},
                     FELT[3] = {46, 86, 68};
static const uint8_t SUIT_RGB[2][3] = {{32, 30, 32}, {192, 36, 40}};          /* black, red */
static const uint8_t TILE_RGB[4][3] = {{184, 40, 36}, {36, 74, 150}, {210, 132, 22}, {34, 32, 32}};
static const uint8_t ACTION_RGB[4][3] = {{168, 52, 40}, {58, 124, 94}, {214, 160, 58}, {52, 84, 150}};
static const uint8_t BAND_RGB[5][3] = {{70, 62, 138}, {82, 148, 196}, {62, 128, 98}, {222, 168, 66}, {176, 54, 42}};

static int red(int suit) { return suit == SUIT_HEARTS || suit == SUIT_DIAMONDS; }
static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

/* ---- painting on a canvas ---------------------------------------------------------------------------------------- */
static art_style flat_style(const uint8_t rgb[3], float line, const uint8_t edge[3])
{
    art_style st;
    memset(&st, 0, sizeof st);
    for (int i = 0; i < 3; i++) {
        st.fill[i] = rgb[i];
        st.outer[i] = st.inner[i] = edge ? edge[i] : rgb[i];
        st.mark[i] = edge ? edge[i] : rgb[i];
    }
    st.line = line;
    return st;
}

/* a figure's unit square drawn `size` pixels wide at (x, y) (its top left); flip: turned round (180 degrees) */
static void put_fig(canvas *c, const art_fig *f, const art_style *st, float x, float y, float size, int flip)
{
    int s = (int)ceilf(size);
    if (s < 2) return;
    uint8_t *b = malloc((size_t)s * s * 4);
    if (!b) return;
    art_render(f, st, s, s, size, 0, 0, b);
    if (flip)                                   /* turned round: the pixels in reverse order */
        for (int i = 0, j = s * s - 1; i < j; i++, j--) {
            uint8_t t[4];
            memcpy(t, b + 4 * i, 4);
            memcpy(b + 4 * i, b + 4 * j, 4);
            memcpy(b + 4 * j, t, 4);
        }
    art_blend(c->o, c->w, c->h, b, s, s, (int)floorf(x + 0.5f), (int)floorf(y + 0.5f));
    free(b);
}

/* a rounded rectangle of one colour (alpha a) blended at (x, y) */
static void put_rect(canvas *c, float x, float y, float w, float h, float rad, const uint8_t rgb[3], float a)
{
    int iw = (int)(w + 0.5f), ih = (int)(h + 0.5f);
    if (iw < 1 || ih < 1) return;
    const uint8_t fill[4] = {rgb[0], rgb[1], rgb[2], (uint8_t)(255 * clamp01(a))};
    uint8_t *b = malloc((size_t)iw * ih * 4);
    if (!b) return;
    art_roundrect(iw, ih, rad, fill, b);
    art_blend(c->o, c->w, c->h, b, iw, ih, (int)(x + 0.5f), (int)(y + 0.5f));
    free(b);
}

/* a rounded rectangle's outline (thickness t) */
static float rr_dist(float x, float y, float w, float h, float rad)
{
    float qx = fabsf(x - w * 0.5f) - (w * 0.5f - rad), qy = fabsf(y - h * 0.5f) - (h * 0.5f - rad);
    return sqrtf(fmaxf(qx, 0) * fmaxf(qx, 0) + fmaxf(qy, 0) * fmaxf(qy, 0)) + fminf(fmaxf(qx, qy), 0) - rad;
}

static void put_frame(canvas *c, float x, float y, float w, float h, float rad, float t, const uint8_t rgb[3], float a)
{
    for (int j = (int)y; j < (int)ceilf(y + h) && j < c->h; j++)
        for (int i = (int)x; i < (int)ceilf(x + w) && i < c->w; i++) {
            if (i < 0 || j < 0) continue;
            float d = rr_dist(i + 0.5f - x, j + 0.5f - y, w, h, rad);
            float cov = clamp01(0.5f - d) * clamp01(d + t + 0.5f) * a;
            if (cov <= 0) continue;
            uint8_t px[4] = {rgb[0], rgb[1], rgb[2], (uint8_t)(255 * cov)};
            art_blend(c->o, c->w, c->h, px, 1, 1, i, j);
        }
}

/* digits from the font, bold, centred on (cx, cy), hgt pixels tall; squeeze: closer (10, 12); a minus before a value
 * below zero; outline: a dark rim round light digits */
static void put_digits(canvas *c, int value, float cx, float cy, float hgt, const uint8_t rgb[3], float squeeze,
                       const uint8_t *outline)
{
    char s[8];
    int n = 0, v = abs(value);
    do { s[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 7);
    float step = 0.68f * hgt * squeeze, dw = 0.6f * hgt, mw = value < 0 ? 0.5f * hgt : 0;
    float total = step * (n - 1) + dw + mw, x = cx - total / 2 + mw;
    int cw = (int)ceilf(0.9f * hgt) + 2, ch = (int)ceilf(1.3f * hgt) + 2;
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    float o = fmaxf(1.0f, 0.07f * hgt);
    for (int pass = outline ? 0 : 1; pass < 2; pass++) {
        const uint8_t *col = pass ? rgb : outline;
        if (value < 0) {                        /* the minus: a bar */
            float bw = 0.4f * hgt, bt = fmaxf(1.5f, 0.13f * hgt), bx = x - mw;
            float grow = pass ? 0 : o;
            put_rect(c, bx - grow, cy - bt / 2 - grow, bw + 2 * grow, bt + 2 * grow, 0, col, 1);
        }
        for (int i = 0; i < n; i++) {
            memset(b, 0, (size_t)cw * ch * 4);
            if (!text_glyph(s[n - 1 - i], hgt * 1.2f, 1, col, b, cw, ch)) continue;
            float dx = x + i * step + dw / 2 - cw / 2.0f, dy = cy - ch / 2.0f;
            if (pass) art_blend(c->o, c->w, c->h, b, cw, ch, (int)(dx + 0.5f), (int)(dy + 0.5f));
            else
                for (int k = 0; k < 8; k++)     /* the rim: the digit shifted all round */
                    art_blend(c->o, c->w, c->h, b, cw, ch, (int)(dx + o * cosf(k * PI_F / 4) + 0.5f),
                              (int)(dy + o * sinf(k * PI_F / 4) + 0.5f));
        }
    }
    free(b);
}

/* a plus (drawn: no font character) centred on (cx, cy), size s */
static void put_plus(canvas *c, float cx, float cy, float s, const uint8_t rgb[3], const uint8_t *outline)
{
    float t = fmaxf(1.5f, 0.24f * s), o = outline ? fmaxf(1.0f, 0.07f * s / 0.6f) : 0;
    if (outline) {
        put_rect(c, cx - s / 2 - o, cy - t / 2 - o, s + 2 * o, t + 2 * o, 0, outline, 1);
        put_rect(c, cx - t / 2 - o, cy - s / 2 - o, t + 2 * o, s + 2 * o, 0, outline, 1);
    }
    put_rect(c, cx - s / 2, cy - t / 2, s, t, 0, rgb, 1);
    put_rect(c, cx - t / 2, cy - s / 2, t, s, 0, rgb, 1);
}

/* ---- figures in their unit square ----------------------------------------------------------------------------- */
/* points fitted into a box (x0, y0)-(x1, y1), keeping their shape, centred */
static void fit(float *p, int n, float x0, float y0, float x1, float y1)
{
    float a = 1e9f, b = 1e9f, c = -1e9f, d = -1e9f;
    for (int i = 0; i < n; i++) {
        a = fminf(a, p[2 * i]);
        c = fmaxf(c, p[2 * i]);
        b = fminf(b, p[2 * i + 1]);
        d = fmaxf(d, p[2 * i + 1]);
    }
    float k = fminf((x1 - x0) / (c - a), (y1 - y0) / (d - b)), ox = (x0 + x1 - (a + c) * k) / 2,
          oy = (y0 + y1 - (b + d) * k) / 2;
    for (int i = 0; i < n; i++) {
        p[2 * i] = ox + p[2 * i] * k;
        p[2 * i + 1] = oy + p[2 * i + 1] * k;
    }
}

static void heart_pts(float *p, int n, int up)
{
    for (int i = 0; i < n; i++) {
        float t = 2 * PI_F * i / n, s = sinf(t);
        p[2 * i] = 16 * s * s * s;
        p[2 * i + 1] = -(13 * cosf(t) - 5 * cosf(2 * t) - 2 * cosf(3 * t) - cosf(4 * t)) * (up ? -1 : 1);
    }
}

static const float STEM[] = {0.5f, 0.52f, 0.455f, 0.78f, 0.31f, 0.96f, 0.69f, 0.96f, 0.545f, 0.78f};

static void suit_fig(art_fig *f, int suit)
{
    float p[2 * 44];
    switch (suit) {
    case SUIT_HEARTS:
        heart_pts(p, 44, 0);
        fit(p, 44, 0.04f, 0.06f, 0.96f, 0.94f);
        art_poly(f, ART_PART, p, 44);
        break;
    case SUIT_SPADES:
        heart_pts(p, 44, 1);
        fit(p, 44, 0.06f, 0.02f, 0.94f, 0.80f);
        art_poly(f, ART_PART, p, 44);
        art_poly(f, ART_PART, STEM, 5);
        break;
    case SUIT_DIAMONDS: {
        static const float cx[4] = {0.5f, 0.85f, 0.5f, 0.15f}, cy[4] = {0.02f, 0.5f, 0.98f, 0.5f};
        int n = 0;
        for (int e = 0; e < 4; e++)
            for (int k = 0; k < 8; k++) {   /* each side curved in a little */
                float s = k / 8.0f, x = cx[e] + (cx[(e + 1) % 4] - cx[e]) * s,
                      y = cy[e] + (cy[(e + 1) % 4] - cy[e]) * s;
                float bow = 0.035f * sinf(PI_F * s);
                p[n++] = x + (0.5f - x) * bow * 2.2f;
                p[n++] = y + (0.5f - y) * bow * 2.2f;
            }
        art_poly(f, ART_PART, p, n / 2);
        break;
    }
    default:                                    /* clubs */
        art_ellipse(f, ART_PART, 0.5f, 0.25f, 0.205f, 0.205f);
        art_ellipse(f, ART_PART, 0.265f, 0.555f, 0.205f, 0.205f);
        art_ellipse(f, ART_PART, 0.735f, 0.555f, 0.205f, 0.205f);
        art_ellipse(f, ART_PART, 0.5f, 0.47f, 0.13f, 0.13f);
        art_poly(f, ART_PART, STEM, 5);
        break;
    }
}

static void star_fig(art_fig *f, float cx, float cy, float ro, float ri, int points)
{
    float p[2 * 40];
    int n = points * 2 > 40 ? 20 : points;
    for (int i = 0; i < 2 * n; i++) {
        float a = (-90 + 180.0f * i / n) * PI_F / 180, r = i & 1 ? ri : ro;
        p[2 * i] = cx + r * cosf(a);
        p[2 * i + 1] = cy + r * sinf(a);
    }
    art_poly(f, ART_PART, p, 2 * n);
}

static void jester_fig(art_fig *f)              /* a jester's cap: two horns drooping to the sides, one up, bells */
{
    const float left[] = {0.24f, 0.72f, 0.20f, 0.58f, 0.13f, 0.50f, 0.06f, 0.52f, 0.05f, 0.42f, 0.10f, 0.32f,
                          0.20f, 0.25f, 0.32f, 0.26f, 0.40f, 0.36f, 0.46f, 0.70f};
    const float right[] = {0.76f, 0.72f, 0.80f, 0.58f, 0.87f, 0.50f, 0.94f, 0.52f, 0.95f, 0.42f, 0.90f, 0.32f,
                           0.80f, 0.25f, 0.68f, 0.26f, 0.60f, 0.36f, 0.54f, 0.70f};
    const float mid[] = {0.38f, 0.70f, 0.40f, 0.42f, 0.46f, 0.22f, 0.54f, 0.13f, 0.60f, 0.24f, 0.62f, 0.44f,
                         0.62f, 0.70f};
    const float band[] = {0.22f, 0.70f, 0.5f, 0.66f, 0.78f, 0.70f, 0.78f, 0.84f, 0.5f, 0.80f, 0.22f, 0.84f};
    art_poly(f, ART_PART, mid, 7);
    art_poly(f, ART_PART, left, 10);
    art_poly(f, ART_PART, right, 10);
    art_poly(f, ART_PART, band, 6);
    art_ellipse(f, ART_PART, 0.065f, 0.57f, 0.055f, 0.055f);
    art_ellipse(f, ART_PART, 0.545f, 0.10f, 0.055f, 0.055f);
    art_ellipse(f, ART_PART, 0.935f, 0.57f, 0.055f, 0.055f);
}

/* an arrow's head at angle a (degrees, y up) on a circle of radius r round (cx, cy), pointing on round it (dir +1:
 * anticlockwise, -1: clockwise) */
static void head_fig(art_fig *f, float cx, float cy, float r, float a, int dir, float size)
{
    float rad = a * PI_F / 180, px = cx + r * cosf(rad), py = cy - r * sinf(rad);
    float dx = -sinf(rad) * dir, dy = -cosf(rad) * dir, nx = cosf(rad), ny = -sinf(rad);
    const float t[] = {px + dx * size * 1.2f, py + dy * size * 1.2f, px + nx * size, py + ny * size,
                       px - nx * size, py - ny * size};
    art_poly(f, ART_PART, t, 3);
}

/* ---- the faces --------------------------------------------------------------------------------------------------- */
/* a suit symbol, size pixels, centred on (cx, cy); flip: turned round */
static void put_suit(canvas *c, int suit, float cx, float cy, float size, int flip)
{
    art_fig *f = art_fig_new();
    suit_fig(f, suit);
    art_style st = flat_style(SUIT_RGB[red(suit)], 0, NULL);
    put_fig(c, f, &st, cx - size / 2, cy - size / 2, size, flip);
    art_fig_free(f);
}

static const art_style *court_style(int suit)
{
    static const art_style st[2] = {
        {{58, 54, 54}, {10, 9, 9}, {150, 140, 132}, {205, 195, 185}, 0.35f, 0},
        {{184, 42, 38}, {62, 12, 10}, {236, 150, 130}, {248, 226, 210}, 0.32f, 0},
    };
    return &st[red(suit)];
}

/* a chess figure (the court) size pixels, its unit square's top left at (x, y) */
static void put_figure(canvas *c, int type, int suit, float x, float y, float size, int flip)
{
    int s = (int)ceilf(size);
    if (s < 4) return;
    uint8_t *b = malloc((size_t)s * s * 4);
    if (!b) return;
    art_piece_style(type, court_style(suit), s, b);
    if (flip)
        for (int i = 0, j = s * s - 1; i < j; i++, j--) {
            uint8_t t[4];
            memcpy(t, b + 4 * i, 4);
            memcpy(b + 4 * i, b + 4 * j, 4);
            memcpy(b + 4 * j, t, 4);
        }
    art_blend(c->o, c->w, c->h, b, s, s, (int)(x + 0.5f), (int)(y + 0.5f));
    free(b);
}

/* the court figure of a rank (french or tarot): jack a bishop, knight a knight, queen a queen, king a king */
static int court_of(int deck, int rank)
{
    if (deck == DECK_TAROT)
        return rank == TAROT_JACK ? BISHOP : rank == TAROT_KNIGHT ? KNIGHT : rank == TAROT_QUEEN ? QUEEN
             : rank == TAROT_KING ? KING : -1;
    return rank == RANK_JACK ? BISHOP : rank == RANK_QUEEN ? QUEEN : rank == RANK_KING ? KING : -1;
}

static void put_star(canvas *c, float cx, float cy, float size, const uint8_t rgb[3], const uint8_t *edge)
{
    art_fig *f = art_fig_new();
    star_fig(f, 0.5f, 0.53f, 0.48f, 0.2f, 5);
    art_style st = flat_style(rgb, edge ? 0.035f : 0, edge);
    put_fig(c, f, &st, cx - size / 2, cy - size / 2, size, 0);
    art_fig_free(f);
}

static void put_lozenge(canvas *c, float cx, float cy, float size, const uint8_t rgb[3])
{
    art_fig *f = art_fig_new();
    const float p[] = {0.5f, 0.0f, 0.82f, 0.5f, 0.5f, 1.0f, 0.18f, 0.5f};
    art_poly(f, ART_PART, p, 4);
    art_style st = flat_style(rgb, 0, NULL);
    put_fig(c, f, &st, cx - size / 2, cy - size / 2, size, 0);
    art_fig_free(f);
}

/* the gold sun of a trump: rays, a cream disc, a gold rim */
static void put_sun(canvas *c, float cx, float cy, float r)
{
    art_fig *f = art_fig_new();
    star_fig(f, 0.5f, 0.5f, 0.5f, 0.36f, 16);
    art_style st = flat_style(GOLD, 0, NULL);
    st.light = 0.2f;
    put_fig(c, f, &st, cx - r, cy - r, 2 * r, 0);
    art_fig_free(f);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.5f, 0.5f);
    art_style disc = flat_style(IVORY, 0.05f, GOLD);
    put_fig(c, f, &disc, cx - 0.72f * r, cy - 0.72f * r, 1.44f * r, 0);
    art_fig_free(f);
}

/* a 6 or a 9 alone gets a bar under it (cards turn round: 6 and 9 must not be mistaken) */
static void mark69(canvas *c, int value, float cx, float cy, float hgt, const uint8_t rgb[3], const uint8_t *outline)
{
    if (value != 6 && value != 9) return;
    float bw = 0.5f * hgt, bt = fmaxf(1.5f, 0.1f * hgt), y = cy + 0.66f * hgt,
          o = outline ? fmaxf(1.0f, 0.07f * hgt) : 0;
    if (outline) put_rect(c, cx - bw / 2 - o, y - o, bw + 2 * o, bt + 2 * o, 0, outline, 1);
    put_rect(c, cx - bw / 2, y, bw, bt, 0, rgb, 1);
}

/* the corner index: what the card is, over its suit; painted top left, then again turned round bottom right */
static void corner(canvas *c, int deck, int card)
{
    float w = (float)c->w;
    int cw = (int)ceilf(0.27f * w), ch = (int)ceilf(0.42f * w);
    canvas k = {calloc((size_t)cw * ch, 4), cw, ch};
    if (!k.o) return;
    int suit = CARD_SUIT(card), rank = CARD_RANK(card);
    float x = 0.125f * w, y = 0.13f * w;
    if (deck == DECK_FRENCH || deck == DECK_TAROT) {
        int court = suit < 4 ? court_of(deck, rank) : -1;
        if (suit == SUIT_JOKER || (suit == SUIT_TRUMP && rank == 0)) {
            put_star(&k, x, 0.16f * w, 0.18f * w, suit == SUIT_JOKER ? SUIT_RGB[rank == 0] : INK, NULL);
        } else if (suit == SUIT_TRUMP) {
            put_digits(&k, rank, x, y, 0.14f * w, INK, rank >= 10 ? 0.78f : 1, NULL);
            put_sun(&k, x, 0.285f * w, 0.07f * w);
        } else if (rank == 1) {
            put_suit(&k, suit, x, 0.2f * w, 0.19f * w, 0);
        } else {
            if (court >= 0) put_figure(&k, court, suit, x - 0.12f * w, y - 0.14f * w, 0.24f * w, 0);
            else put_digits(&k, rank, x, y, 0.15f * w, SUIT_RGB[red(suit)], rank >= 10 ? 0.78f : 1, NULL);
            put_suit(&k, suit, x, 0.305f * w, 0.13f * w, 0);
        }
    } else if (deck == DECK_NUMBER) {
        int v = NUMBER_VALUE(card);
        put_digits(&k, v, (v < 0 ? 0.17f : 0.14f) * w, 0.13f * w, 0.15f * w, CREAM, 0.8f, INK);
        mark69(&k, NUMBER_VALUE(card), 0.14f * w, 0.13f * w, 0.15f * w, CREAM, INK);
    }
    art_blend(c->o, c->w, c->h, k.o, cw, ch, 0, 0);
    for (int i = 0, j = cw * ch - 1; i < j; i++, j--) {   /* turned round: bottom right */
        uint8_t t[4];
        memcpy(t, k.o + 4 * i, 4);
        memcpy(k.o + 4 * i, k.o + 4 * j, 4);
        memcpy(k.o + 4 * j, t, 4);
    }
    art_blend(c->o, c->w, c->h, k.o, cw, ch, c->w - cw, c->h - ch);
    free(k.o);
}

/* the card's body: ivory in a thin rim, rounded */
static void body(canvas *c, const uint8_t face[3], const uint8_t rim[3])
{
    float w = (float)c->w, h = (float)c->h, r = 0.075f * w, b = fmaxf(1, 0.014f * w);
    const uint8_t rf[4] = {rim[0], rim[1], rim[2], 255};
    art_roundrect(c->w, c->h, r, rf, c->o);
    put_rect(c, b, b, w - 2 * b, h - 2 * b, r - b, face, 1);
}

/* pips: the classic layouts of 2..10 (columns left, middle, right; rows from the top 0..1), the lower ones turned */
static void pips(canvas *c, int suit, int n, int tarot)
{
    static const float L9[] = {0, 1 / 3.0f, 2 / 3.0f, 1};
    float w = (float)c->w, h = (float)c->h, col[3] = {0.325f * w, 0.5f * w, 0.675f * w};
    float top = (tarot ? 0.18f : 0.215f) * h, bot = (tarot ? 0.82f : 0.785f) * h, size = (n >= 9 ? 0.175f : 0.195f) * w;
    float at[10][2];
    int k = 0;
#define PIP(cc, rr) (at[k][0] = (float)(cc), at[k][1] = (rr), k++)
    switch (n) {
    case 2: PIP(1, 0); PIP(1, 1); break;
    case 3: PIP(1, 0); PIP(1, 0.5f); PIP(1, 1); break;
    default:
        if (n <= 8) {
            for (int r = 0; r < (n >= 6 ? 3 : 2); r++) { float y = n >= 6 ? r * 0.5f : r; PIP(0, y); PIP(2, y); }
            if (n == 5) PIP(1, 0.5f);
            if (n >= 7) PIP(1, 0.25f);
            if (n == 8) PIP(1, 0.75f);
        } else {
            for (int r = 0; r < 4; r++) { PIP(0, L9[r]); PIP(2, L9[r]); }
            if (n == 9) PIP(1, 0.5f);
            else { PIP(1, 1 / 6.0f); PIP(1, 5 / 6.0f); }
        }
    }
#undef PIP
    for (int i = 0; i < k; i++)
        put_suit(c, suit, col[(int)at[i][0]], top + (bot - top) * at[i][1], size, at[i][1] > 0.5f);
}

/* a court card: a pale panel in a line of the suit's colour, the figure twice, foot to foot */
static void court(canvas *c, int type, int suit)
{
    float w = (float)c->w, h = (float)c->h, x0 = 0.245f * w, y0 = 0.075f * h, pw = 0.51f * w, ph = 0.85f * h;
    static const uint8_t pale[3] = {244, 232, 204};
    put_rect(c, x0, y0, pw, ph, 0.03f * w, pale, 1);
    put_frame(c, x0, y0, pw, ph, 0.03f * w, fmaxf(1, 0.012f * w), SUIT_RGB[red(suit)], 0.85f);
    float s = fminf(0.48f * h, 0.95f * w) * 0.98f;
    put_figure(c, type, suit, w / 2 - s / 2, h / 2 - 0.905f * s, s, 0);
    put_figure(c, type, suit, w / 2 - s / 2, h / 2 - 0.095f * s, s, 1);
}

/* ---- the tarot's pictures ---------------------------------------------------------------------------------------- */
/* a tarot card's picture (cards_tarot_art): the excuse, a trump, a court; -1: none (a number card) */
static int tarot_art_of(int card)
{
    int suit = CARD_SUIT(card), rank = CARD_RANK(card);
    if (suit == SUIT_TRUMP) return rank <= 21 ? TAROT_ART_EXCUSE + rank : -1;
    if (suit < 4 && rank >= TAROT_JACK && rank <= TAROT_KING) return TAROT_ART_COURTS + 4 * suit + rank - TAROT_JACK;
    return -1;
}

/* one axis of a resampling, n samples to m: each output's source samples and weights (area averaging when it
 * shrinks, linear when it grows) */
typedef struct { int first, count; float w[64]; } tap;

static tap *taps(int n, int m)
{
    tap *t = malloc(sizeof *t * (size_t)m);
    if (!t) return NULL;
    float scale = (float)n / m;
    for (int i = 0; i < m; i++) {
        if (scale > 1) {                        /* the source span [a, b) this pixel covers */
            float a = i * scale, b = a + scale;
            int j0 = (int)a, j1 = (int)ceilf(b);
            if (j1 > n) j1 = n;
            if (j1 - j0 > 64) j1 = j0 + 64;
            t[i].first = j0;
            t[i].count = j1 - j0;
            for (int j = j0; j < j1; j++) t[i].w[j - j0] = (fminf(b, j + 1.0f) - fmaxf(a, (float)j)) / scale;
        } else {
            float c = (i + 0.5f) * scale - 0.5f;
            int j0 = (int)floorf(c);
            float f = c - j0;
            if (j0 < 0) j0 = 0, f = 0;
            if (j0 >= n - 1) j0 = n - 1, f = 0;
            t[i].first = j0;
            t[i].count = f > 0 ? 2 : 1;
            t[i].w[0] = 1 - f;
            t[i].w[1] = f;
        }
    }
    return t;
}

/* picture idx scaled to w x h, its corners rounded (rad), blended at (x, y) */
static void put_scan(canvas *c, int idx, float x, float y, float w, float h, float rad)
{
    int sw, sh, n, tw = (int)(w + 0.5f), th = (int)(h + 0.5f);
    if (tw < 2 || th < 2) return;
    uint8_t *s = stbi_load_from_memory(cards_tarot_art[idx].jpg, cards_tarot_art[idx].size, &sw, &sh, &n, 3);
    if (!s) return;
    tap *tx = taps(sw, tw), *ty = taps(sh, th);
    float *mid = malloc(sizeof *mid * (size_t)tw * sh * 3);
    uint8_t *o = malloc((size_t)tw * th * 4);
    if (tx && ty && mid && o) {
        for (int j = 0; j < sh; j++)            /* across, each source row */
            for (int i = 0; i < tw; i++) {
                float acc[3] = {0, 0, 0};
                for (int k = 0; k < tx[i].count; k++) {
                    const uint8_t *p = s + 3 * ((size_t)j * sw + tx[i].first + k);
                    for (int ch = 0; ch < 3; ch++) acc[ch] += tx[i].w[k] * p[ch];
                }
                memcpy(mid + 3 * ((size_t)j * tw + i), acc, sizeof acc);
            }
        for (int j = 0; j < th; j++)            /* down */
            for (int i = 0; i < tw; i++) {
                float acc[3] = {0, 0, 0};
                for (int k = 0; k < ty[j].count; k++) {
                    const float *p = mid + 3 * ((size_t)(ty[j].first + k) * tw + i);
                    for (int ch = 0; ch < 3; ch++) acc[ch] += ty[j].w[k] * p[ch];
                }
                uint8_t *q = o + 4 * ((size_t)j * tw + i);
                for (int ch = 0; ch < 3; ch++) q[ch] = (uint8_t)fminf(255, fmaxf(0, acc[ch] + 0.5f));
                q[3] = (uint8_t)(255 * clamp01(0.5f - rr_dist(i + 0.5f, j + 0.5f, (float)tw, (float)th, rad)));
            }
        art_blend(c->o, c->w, c->h, o, tw, th, (int)(x + 0.5f), (int)(y + 0.5f));
    }
    free(tx);
    free(ty);
    free(mid);
    free(o);
    stbi_image_free(s);
}

/* a pictured trump's corner index, top left and turned round bottom right: its number (the excuse: a star), dark
 * in an ivory rim, on the picture's corner ornament. The 1898 trumps carry their number at the top in the middle,
 * hidden in a fanned hand (only a card's left part shows); the courts carry their suit in the corner already. */
static void scan_corner(canvas *c, int card)
{
    float w = (float)c->w;
    int cw = (int)ceilf(0.28f * w), ch = (int)ceilf(0.26f * w);
    canvas k = {calloc((size_t)cw * ch, 4), cw, ch};
    if (!k.o) return;
    int rank = CARD_RANK(card);
    float x = 0.13f * w, y = 0.125f * w;
    if (rank == 0) put_star(&k, x, y, 0.19f * w, INK, IVORY);
    else put_digits(&k, rank, x, y, 0.15f * w, INK, rank >= 10 ? 0.78f : 1, IVORY);
    art_blend(c->o, c->w, c->h, k.o, cw, ch, 0, 0);
    for (int i = 0, j = cw * ch - 1; i < j; i++, j--) {
        uint8_t t[4];
        memcpy(t, k.o + 4 * i, 4);
        memcpy(k.o + 4 * i, k.o + 4 * j, 4);
        memcpy(k.o + 4 * j, t, 4);
    }
    art_blend(c->o, c->w, c->h, k.o, cw, ch, c->w - cw, c->h - ch);
    free(k.o);
}

/* a pictured tarot card: the kit's rim, the picture inside it, a trump's corner index */
static void face_scan(canvas *c, int card, int idx)
{
    float w = (float)c->w, h = (float)c->h, r = 0.075f * w, b = fmaxf(1, 0.014f * w);
    body(c, IVORY, RIM);
    put_scan(c, idx, b, b, w - 2 * b, h - 2 * b, r - b);
    if (CARD_SUIT(card) == SUIT_TRUMP) scan_corner(c, card);
}

static void face_french(canvas *c, int deck, int card)
{
    float w = (float)c->w, h = (float)c->h;
    int suit = CARD_SUIT(card), rank = CARD_RANK(card), small = c->w < 52;
    int pic = deck == DECK_TAROT && !small ? tarot_art_of(card) : -1;
    if (pic >= 0) {                             /* small: the drawn face below (a picture is unreadable there) */
        face_scan(c, card, pic);
        return;
    }
    body(c, IVORY, RIM);
    if (suit == SUIT_JOKER) {                   /* a jester's cap */
        art_fig *f = art_fig_new();
        jester_fig(f);
        art_style st = *court_style(rank == 0 ? SUIT_HEARTS : SUIT_SPADES);
        st.line = 0.025f;
        float s = (small ? 0.9f : 0.72f) * w;
        put_fig(c, f, &st, w / 2 - s / 2, h / 2 - s / 2, s, 0);
        art_fig_free(f);
        if (!small) corner(c, deck, card);
        return;
    }
    if (suit == SUIT_TRUMP) {                   /* a trump: its number on a gold sun; the excuse: a star */
        static const uint8_t sky[3] = {222, 232, 238}, line[3] = {96, 120, 140};
        float m = small ? 0.08f * w : 0.24f * w;
        put_rect(c, m, 0.06f * h, w - 2 * m, 0.88f * h, 0.03f * w, sky, 1);
        put_frame(c, m, 0.06f * h, w - 2 * m, 0.88f * h, 0.03f * w, fmaxf(1, 0.012f * w), line, 0.8f);
        if (rank == 0) {
            put_sun(c, w / 2, h / 2, (small ? 0.46f : 0.34f) * w);
            put_star(c, w / 2, h / 2, (small ? 0.62f : 0.46f) * w, INK, NULL);
        } else {
            put_sun(c, w / 2, h / 2, (small ? 0.48f : 0.36f) * w);
            put_digits(c, rank, w / 2, h / 2, (small ? 0.36f : 0.25f) * w, INK, 0.84f, NULL);
        }
        if (!small) corner(c, deck, card);
        return;
    }
    int type = court_of(deck, rank);
    if (small) {                                /* small: the index big, the suit under it */
        if (rank == 1) put_suit(c, suit, w / 2, h / 2, 0.7f * w, 0);
        else {
            if (type >= 0) put_figure(c, type, suit, w / 2 - 0.36f * w, 0.3f * h - 0.42f * w, 0.72f * w, 0);
            else put_digits(c, rank, w / 2, 0.31f * h, 0.42f * w, SUIT_RGB[red(suit)], rank >= 10 ? 0.8f : 1, NULL);
            put_suit(c, suit, w / 2, 0.71f * h, 0.46f * w, 0);
        }
        return;
    }
    if (rank == 1) put_suit(c, suit, w / 2, h / 2, 0.44f * w, 0);
    else if (type >= 0) court(c, type, suit);
    else pips(c, suit, rank, deck == DECK_TAROT);
    corner(c, deck, card);
}

/* a number card: lacquer by band, a cream rim, the value big in the middle (cream, a dark rim), small in the
 * corners */
static void face_number(canvas *c, int card)
{
    float w = (float)c->w, h = (float)c->h;
    int v = NUMBER_VALUE(card), band = v < 0 ? 0 : v == 0 ? 1 : v <= 4 ? 2 : v <= 8 ? 3 : 4, small = c->w < 52;
    body(c, BAND_RGB[band], CREAM);
    float b = 0.07f * w;
    put_frame(c, b, b, w - 2 * b, h - 2 * b, 0.05f * w, fmaxf(1, 0.018f * w), CREAM, 0.55f);
    put_digits(c, v, w / 2, h / 2, (small ? 0.55f : 0.42f) * w, CREAM, v >= 10 || v < 0 ? 0.82f : 1, INK);
    mark69(c, v, w / 2, h / 2, (small ? 0.55f : 0.42f) * w, CREAM, INK);
    if (!small) corner(c, DECK_NUMBER, card);
}

/* a rummy tile: ivory on a wooden side (its thickness at the bottom right), the number in its colour, a ring under it;
 * a joker: the cards' jester's cap (the house's own mark, not a face) */
static void face_tile(canvas *c, int card)
{
    float w = (float)c->w, h = (float)c->h, r = 0.12f * w;
    static const uint8_t side[3] = {176, 140, 96}, face[3] = {248, 242, 226};
    const uint8_t sf[4] = {side[0], side[1], side[2], 255};
    art_roundrect(c->w, c->h, r, sf, c->o);
    float fw = w - 0.07f * w, fh = h - 0.07f * w;
    put_rect(c, 0, 0, fw, fh, r, face, 1);
    put_frame(c, 0, 0, fw, fh, r, fmaxf(1, 0.02f * w), side, 0.35f);
    int suit = CARD_SUIT(card), rank = CARD_RANK(card);
    if (suit == SUIT_JOKER) {
        const uint8_t *col = TILE_RGB[rank ? 3 : 0];
        art_fig *f = art_fig_new();
        jester_fig(f);
        art_style st = flat_style(col, 0, NULL);
        float s = 0.74f * w;
        put_fig(c, f, &st, fw / 2 - s / 2, 0.44f * fh - s / 2, s, 0);
        art_fig_free(f);
        return;
    }
    const uint8_t *col = TILE_RGB[suit & 3];
    put_digits(c, rank, fw / 2, 0.42f * fh, 0.44f * w, col, rank >= 10 ? 0.8f : 1, NULL);
    art_fig *f = art_fig_new();
    art_arc(f, ART_PART, 0.5f, 0.5f, 0.3f, 0.5f, 0, 360);
    art_style st = flat_style(col, 0, NULL);
    float s = 0.2f * w;
    put_fig(c, f, &st, fw / 2 - s / 2, 0.8f * fh - s / 2, s, 0);
    art_fig_free(f);
}

/* the action deck's icons, cream with a dark rim, size pixels centred on (cx, cy) */
static void put_action_icon(canvas *c, int card, float cx, float cy, float size, int field)
{
    int rank = CARD_RANK(card), wild = CARD_SUIT(card) == SUIT_JOKER;
    art_style st = flat_style(CREAM, 0.045f, INK);
    float x = cx - size / 2, y = cy - size / 2;
    if (wild) {                                 /* four colour quarters in a cream ring */
        art_fig *ring = art_fig_new();
        art_ellipse(ring, ART_PART, 0.5f, 0.5f, 0.46f, 0.46f);
        put_fig(c, ring, &st, x, y, size, 0);
        art_fig_free(ring);
        for (int q = 0; q < 4; q++) {
            art_fig *f = art_fig_new();
            art_arc(f, ART_PART, 0.5f, 0.5f, 0, 0.38f, 90.0f * q + 0.5f, 90.0f * q + 89.5f);
            art_style qs = flat_style(ACTION_RGB[(q + 1) % 4], 0, NULL);
            put_fig(c, f, &qs, x, y, size, 0);
            art_fig_free(f);
        }
        if (rank == 1) {                        /* draw four: +4 over the quarters */
            put_plus(c, cx - 0.17f * size, cy, 0.22f * size, CREAM, INK);
            put_digits(c, 4, cx + 0.12f * size, cy, 0.36f * size, CREAM, 1, INK);
        }
        return;
    }
    art_fig *f = art_fig_new();
    if (rank == ACTION_SKIP) {                  /* a circle with a bar across */
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.27f, 0.4f, 0, 360);
        const float bar[] = {0.27f, 0.66f, 0.66f, 0.27f, 0.73f, 0.34f, 0.34f, 0.73f};
        art_poly(f, ART_PART, bar, 4);
    } else if (rank == ACTION_REVERSE) {        /* two curved arrows going round */
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.24f, 0.34f, 30, 160);
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.24f, 0.34f, 210, 340);
        head_fig(f, 0.5f, 0.5f, 0.29f, 160, 1, 0.13f);
        head_fig(f, 0.5f, 0.5f, 0.29f, 340, 1, 0.13f);
    } else {                                    /* draw two: two cards stacked */
        art_rect(f, ART_PART, 0.2f, 0.12f, 0.58f, 0.66f);
        art_rect(f, ART_PART, 0.4f, 0.32f, 0.8f, 0.88f);
    }
    put_fig(c, f, &st, x, y, size, 0);
    art_fig_free(f);
    if (rank == ACTION_DRAW2) {                 /* +2 on the front card, in the card's colour */
        put_plus(c, x + 0.5f * size, y + 0.6f * size, 0.13f * size, ACTION_RGB[field], NULL);
        put_digits(c, 2, x + 0.67f * size, y + 0.6f * size, 0.24f * size, ACTION_RGB[field], 1, NULL);
    }
}

static void face_action(canvas *c, int card)
{
    float w = (float)c->w, h = (float)c->h;
    int suit = CARD_SUIT(card), rank = CARD_RANK(card), wild = suit == SUIT_JOKER, small = c->w < 52;
    static const uint8_t ebony[3] = {44, 40, 40};
    const uint8_t *field = wild ? ebony : ACTION_RGB[suit & 3];
    body(c, field, CREAM);
    float b = 0.05f * w;
    put_frame(c, b, b, w - 2 * b, h - 2 * b, 0.05f * w, fmaxf(1, 0.012f * w), GOLD, 0.7f);
    if (!wild) {                                /* a darker lozenge behind the middle */
        uint8_t dk[3] = {(uint8_t)(field[0] * 0.8f), (uint8_t)(field[1] * 0.8f), (uint8_t)(field[2] * 0.8f)};
        art_fig *f = art_fig_new();
        const float p[] = {0.5f, 0.02f, 0.98f, 0.5f, 0.5f, 0.98f, 0.02f, 0.5f};
        art_poly(f, ART_PART, p, 4);
        art_style st = flat_style(dk, 0, NULL);
        float s = (small ? 0.95f : 0.8f) * w;
        put_fig(c, f, &st, w / 2 - s / 2, h / 2 - s / 2, s, 0);
        art_fig_free(f);
    }
    float big = (small ? 0.85f : 0.62f) * w;
    if (!wild && rank <= 9) {
        put_digits(c, rank, w / 2, h / 2, (small ? 0.55f : 0.44f) * w, CREAM, 1, INK);
        mark69(c, rank, w / 2, h / 2, (small ? 0.55f : 0.44f) * w, CREAM, INK);
    } else put_action_icon(c, card, w / 2, h / 2, big, suit & 3);
    if (small) return;
    /* the corners: the number or a small icon, top left and turned round bottom right */
    int cw = (int)ceilf(0.3f * w), ch = (int)ceilf(0.3f * w);
    canvas k = {calloc((size_t)cw * ch, 4), cw, ch};
    if (!k.o) return;
    if (!wild) mark69(&k, rank, 0.15f * w, 0.15f * w, 0.15f * w, CREAM, INK);
    if (!wild && rank <= 9) put_digits(&k, rank, 0.15f * w, 0.15f * w, 0.15f * w, CREAM, 1, INK);
    else if (!wild && rank == ACTION_DRAW2) {
        put_plus(&k, 0.09f * w, 0.15f * w, 0.08f * w, CREAM, INK);
        put_digits(&k, 2, 0.19f * w, 0.15f * w, 0.14f * w, CREAM, 1, INK);
    } else if (wild && rank == 1) {
        put_plus(&k, 0.09f * w, 0.15f * w, 0.08f * w, CREAM, INK);
        put_digits(&k, 4, 0.19f * w, 0.15f * w, 0.14f * w, CREAM, 1, INK);
    } else put_action_icon(&k, card, 0.15f * w, 0.15f * w, 0.22f * w, suit & 3);
    art_blend(c->o, c->w, c->h, k.o, cw, ch, 0, 0);
    for (int i = 0, j = cw * ch - 1; i < j; i++, j--) {
        uint8_t t[4];
        memcpy(t, k.o + 4 * i, 4);
        memcpy(k.o + 4 * i, k.o + 4 * j, 4);
        memcpy(k.o + 4 * j, t, 4);
    }
    art_blend(c->o, c->w, c->h, k.o, cw, ch, c->w - cw, c->h - ch);
    free(k.o);
}

/* the backs: the house's (the green table in a walnut frame, a lattice, a gold medallion), the tiles' (wood), the
 * action deck's (ebony, the four colours in a cross) */
static void back(canvas *c, int deck)
{
    float w = (float)c->w, h = (float)c->h;
    if (deck == DECK_TILES) {
        const uint8_t sf[4] = {150, 112, 72, 255};
        art_roundrect(c->w, c->h, 0.12f * w, sf, c->o);
        float fw = w - 0.07f * w, fh = h - 0.07f * w;
        for (int y = 0; y < (int)fh; y++)       /* the wood's grain */
            for (int x = 0; x < (int)fw; x++) {
                float g = sinf(y * 23.0f / h + 2.4f * sinf(x * 2.0f / w + 1.3f)) * 0.5f + 0.5f,
                      k = 1 + 0.07f * (g - 0.5f);
                float d = rr_dist(x + 0.5f, y + 0.5f, fw, fh, 0.12f * w);
                if (d > 0.5f) continue;
                uint8_t px[4] = {(uint8_t)fminf(255, 222 * k), (uint8_t)fminf(255, 184 * k),
                                 (uint8_t)fminf(255, 128 * k),
                                 (uint8_t)(255 * clamp01(0.5f - d))};
                art_blend(c->o, c->w, c->h, px, 1, 1, x, y);
            }
        return;
    }
    if (deck == DECK_ACTION) {
        static const uint8_t ebony[3] = {44, 40, 40};
        body(c, ebony, CREAM);
        float b = 0.05f * w;
        put_frame(c, b, b, w - 2 * b, h - 2 * b, 0.05f * w, fmaxf(1, 0.012f * w), GOLD, 0.7f);
        float s = 0.2f * w;
        static const float at[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        for (int q = 0; q < 4; q++) put_lozenge(c, w / 2 + at[q][0] * 0.17f * w, h / 2 + at[q][1] * 0.17f * w, s,
                                               ACTION_RGB[q]);
        put_frame(c, w / 2 - 0.36f * w, h / 2 - 0.36f * w, 0.72f * w, 0.72f * w, 0.36f * w, fmaxf(1, 0.02f * w), CREAM,
                  0.8f);
        return;
    }
    body(c, WALNUT, WALNUT);
    float b = 0.07f * w;
    put_rect(c, b, b, w - 2 * b, h - 2 * b, 0.04f * w, FELT, 1);
    float step = 0.14f * w, lw = fmaxf(1, 0.012f * w);
    for (int y = (int)b; y < (int)(h - b); y++)  /* a lattice of lighter lines, both ways */
        for (int x = (int)b; x < (int)(w - b); x++) {
            float u = (x + y) / step, v = (x - y + 1000 * step) / step;
            float du = fabsf(u - floorf(u + 0.5f)) * step, dv = fabsf(v - floorf(v + 0.5f)) * step;
            float a = clamp01(lw - fminf(du, dv) + 0.5f) * 0.22f;
            if (rr_dist(x + 0.5f - b, y + 0.5f - b, w - 2 * b, h - 2 * b, 0.04f * w) > -0.5f) continue;
            uint8_t px[4] = {150, 196, 168, (uint8_t)(255 * a)};
            if (a > 0) art_blend(c->o, c->w, c->h, px, 1, 1, x, y);
        }
    put_frame(c, b + 0.04f * w, b + 0.04f * w, w - 2 * b - 0.08f * w, h - 2 * b - 0.08f * w, 0.03f * w,
              fmaxf(1, 0.012f * w), GOLD, 0.55f);
    float r = 0.2f * w;
    put_rect(c, w / 2 - r, h / 2 - r, 2 * r, 2 * r, r, FELT, 1);
    put_frame(c, w / 2 - r, h / 2 - r, 2 * r, 2 * r, r, fmaxf(1, 0.03f * w), GOLD, 0.9f);
    put_lozenge(c, w / 2, h / 2, 0.26f * w, GOLD);
}

/* ---- the games' own decks ---------------------------------------------------------------------------------------- */
static const cards_deck_def *own[DECKS_MAX - DECKS];
static int nown;

int cards_deck_add(const cards_deck_def *def)
{
    if (nown >= DECKS_MAX - DECKS) return -1;
    own[nown] = def;
    return DECKS + nown++;
}

static const cards_deck_def *own_def(int deck)
{
    return deck >= DECKS && deck < DECKS + nown ? own[deck - DECKS] : NULL;
}

float cards_aspect(int deck)
{
    const cards_deck_def *d = own_def(deck);
    if (d) return d->aspect;
    switch (deck) {
    case DECK_TAROT: return 1.6f;
    case DECK_SPANISH: return 1.5f;
    case DECK_TILES: return 1.3f;
    default: return 1.4f;
    }
}

void cards_paint(int deck, int card, int w, int h, uint8_t *rgba)
{
    canvas c = {rgba, w, h};
    const cards_deck_def *d = own_def(deck);
    memset(rgba, 0, (size_t)w * h * 4);
    if (d) {
        if (card != CARD_NONE) d->face(card, w, h, rgba);
        else if (d->back) d->back(w, h, rgba);
        else back(&c, DECK_FRENCH);
    } else if (card == CARD_NONE) back(&c, deck);
    else if (deck == DECK_NUMBER) face_number(&c, card);
    else if (deck == DECK_TILES) face_tile(&c, card);
    else if (deck == DECK_ACTION) face_action(&c, card);
    else if (deck == DECK_ITALIAN) cards_italian_face(card, w, h, rgba);
    else if (deck == DECK_SPANISH) cards_spanish_face(card, w, h, rgba);
    else face_french(&c, deck, card);
}

void cards_token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* a suit's sign: the Italian and Spanish packs' symbols, a game's own deck's (its `symbol`); else the French pip in the
 * middle of an ace's face (as big, at the same place: the small face's below 52 px), on an ivory disc, its edge
 * smoothed, a thin walnut rim */
void cards_suit_icon(int deck, int suit, int size, uint8_t *rgba)
{
    const cards_deck_def *d = own_def(deck);
    if (deck == DECK_ITALIAN) { cards_italian_symbol(suit, size, rgba); return; }
    if (deck == DECK_SPANISH) { cards_spanish_symbol(suit, size, rgba); return; }
    if (d && d->symbol) { d->symbol(suit, size, rgba); return; }
    int s = size, w = s < 52 ? s : (int)ceilf(s / 0.6f), h = (int)(w * cards_aspect(DECK_FRENCH));
    canvas c = {rgba, s, s};
    int ix = (w - s) / 2, iy = (h - s) / 2;
    for (int i = 0; i < s * s; i++) memcpy(rgba + 4 * i, (const uint8_t[4]){IVORY[0], IVORY[1], IVORY[2], 255}, 4);
    if (s < 52) {                               /* small: as wide as the card, its rim at the sides */
        canvas f = {calloc((size_t)w * h, 4), w, h};
        if (f.o) {
            body(&f, IVORY, RIM);
            for (int y = 0; y < s; y++)
                memcpy(rgba + (size_t)y * s * 4, f.o + ((size_t)(y + iy) * w + ix) * 4, (size_t)s * 4);
            for (int i = 0; i < s * s; i++) rgba[4 * i + 3] = 255;
            free(f.o);
        }
    }
    float x0 = (float)ix, y0 = (float)iy;
    put_suit(&c, suit & 3, w / 2.0f - x0, h / 2.0f - y0, (s < 52 ? 0.7f : 0.44f) * w, 0);
    cards_disc_cut(rgba, s);
    cards_disc_rim(rgba, s);
}

void cards_disc_cut(uint8_t *rgba, int s)
{
    float r = s / 2.0f;
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            float d = hypotf(x + 0.5f - r, y + 0.5f - r), a = clampf(r - d, 0, 1);
            uint8_t *p = rgba + ((size_t)y * s + x) * 4;
            p[3] = (uint8_t)(p[3] * a);
        }
}

void cards_disc_rim(uint8_t *rgba, int s)
{
    static const uint8_t walnut[3] = {70, 48, 30};
    uint8_t *ring = malloc((size_t)s * s * 4);
    if (!ring) return;
    art_ring(s, 0.9f, walnut, 0.75f, ring);
    art_blend(rgba, s, s, ring, s, s, 0, 0);
    free(ring);
}

void cards_paint_at(uint8_t *rgba, int size, int deck, int card, float cx, float cy, int w)
{
    int h = (int)(w * cards_aspect(deck) + 0.5f);
    uint8_t *c = malloc((size_t)w * h * 4);
    if (!c) return;
    cards_paint(deck, card, w, h, c);
    art_blend(rgba, size, size, c, w, h, (int)(cx - w / 2.0f), (int)(cy - h / 2.0f));
    free(c);
}

void cards_paint_turned(uint8_t *rgba, int size, int deck, int card, float cx, float cy, float w, float deg)
{
    int cw = (int)w, ch = (int)(w * cards_aspect(deck));
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    cards_paint(deck, card, cw, ch, b);
    float a = deg * PI_F / 180, ca = cosf(a), sa = sinf(a), r = 0.5f * hypotf((float)cw, (float)ch) + 1;
    for (int y = (int)fmaxf(0, cy - r); y < (int)fminf((float)size, cy + r + 1); y++)
        for (int x = (int)fmaxf(0, cx - r); x < (int)fminf((float)size, cx + r + 1); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float u = dx * ca + dy * sa + cw / 2.0f - 0.5f, v = -dx * sa + dy * ca + ch / 2.0f - 0.5f;
            int u0 = (int)floorf(u), v0 = (int)floorf(v);
            float fu = u - u0, fv = v - v0, acc[4] = {0, 0, 0, 0};
            for (int k = 0; k < 4; k++) {       /* the four pixels round it, weighted (outside: clear) */
                int uu = u0 + (k & 1), vv = v0 + (k >> 1);
                if (uu < 0 || vv < 0 || uu >= cw || vv >= ch) continue;
                const uint8_t *p = b + 4 * ((size_t)vv * cw + uu);
                float wt = (k & 1 ? fu : 1 - fu) * (k >> 1 ? fv : 1 - fv) * p[3] / 255.0f;
                for (int i = 0; i < 3; i++) acc[i] += wt * p[i];
                acc[3] += wt;
            }
            if (acc[3] <= 0.003f) continue;
            uint8_t px[4] = {(uint8_t)(acc[0] / acc[3]), (uint8_t)(acc[1] / acc[3]), (uint8_t)(acc[2] / acc[3]),
                             (uint8_t)(255 * fminf(1, acc[3]))};
            art_blend(rgba, size, size, px, 1, 1, x, y);
        }
    free(b);
}

/* ---- the textures ------------------------------------------------------------------------------------------------ */
#define CACHE 256
static struct { int deck, card, w; unsigned used; SDL_Texture *t; } cache[CACHE];
static unsigned tick;

/* ---- the faces painted ahead --------------------------------------------------------------------------------------
 * A face is slow to paint on a phone (a tarot picture, an Italian court: tens of ms), and a deal shows many at once.
 * Once a width has been asked for two cards of a deck of the kit, a thread of its own paints the deck's other faces
 * at that width (the kit's painting is the kit's alone: no game code, the font's and the art's scratch per thread),
 * and card_tex() finds them painted. A face not ready yet is painted at once, as before; one being painted is waited
 * for. The games' own decks (cards_deck_add) are painted as they are asked for. RSB_NO_PREFETCH=1: none. */
#define PF_WIDTHS 6                             /* the widths asked for lately, per deck of the kit */
#define PF_JOBS 512
#define PF_DONE 400
#define PF_BYTES (24 << 20)                     /* the faces painted and not yet asked for: at most this */
static struct {
    SDL_mutex *m;
    SDL_cond *wake, *done_cv;
    int on;                                     /* 1 started, -1 none */
    struct { int w, cards; unsigned used; uint8_t seen[32]; } width[DECKS][PF_WIDTHS];
    struct { uint8_t deck, card; int w; } job[PF_JOBS];
    int njob;
    struct { uint8_t deck, card; int w, h; uint8_t *px; } done[PF_DONE];
    int ndone;
    size_t bytes;
    int doing_deck, doing_card, doing_w;        /* the face being painted (doing_w 0: none) */
    unsigned clock;
} PF;

static int pf_thread(void *arg)
{
    (void)arg;
    SDL_LockMutex(PF.m);
    for (;;) {
        while (!PF.njob || PF.ndone >= PF_DONE || PF.bytes >= PF_BYTES) SDL_CondWait(PF.wake, PF.m);
        int deck = PF.job[0].deck, card = PF.job[0].card, w = PF.job[0].w;
        memmove(PF.job, PF.job + 1, sizeof PF.job[0] * (size_t)--PF.njob);
        int h = (int)(w * cards_aspect(deck) + 0.5f), dup = 0;
        for (int i = 0; i < PF.ndone && !dup; i++)
            dup = PF.done[i].deck == deck && PF.done[i].card == card && PF.done[i].w == w;
        if (dup) continue;
        PF.doing_deck = deck;
        PF.doing_card = card;
        PF.doing_w = w;
        SDL_UnlockMutex(PF.m);
        uint8_t *b = malloc((size_t)w * h * 4);
        if (b) cards_paint(deck, card, w, h, b);
        SDL_LockMutex(PF.m);
        PF.doing_w = 0;
        if (b) {
            PF.done[PF.ndone].deck = (uint8_t)deck;
            PF.done[PF.ndone].card = (uint8_t)card;
            PF.done[PF.ndone].w = w;
            PF.done[PF.ndone].h = h;
            PF.done[PF.ndone++].px = b;
            PF.bytes += (size_t)w * h * 4;
        }
        SDL_CondBroadcast(PF.done_cv);
    }
    return 0;
}

static int pf_start(void)
{
    if (PF.on) return PF.on > 0;
    PF.on = -1;
    if (getenv("RSB_NO_PREFETCH")) return 0;
    PF.m = SDL_CreateMutex();
    PF.wake = SDL_CreateCond();
    PF.done_cv = SDL_CreateCond();
    if (!PF.m || !PF.wake || !PF.done_cv) return 0;
    SDL_Thread *t = SDL_CreateThread(pf_thread, "cards", NULL);
    if (!t) return 0;
    SDL_DetachThread(t);
    PF.on = 1;
    return 1;
}

/* the deck's faces, each once (its biggest pack) and its back */
static int pf_pack(int deck, uint8_t *out)
{
    static const int PACK_OF[DECKS] = {PACK_54, PACK_TAROT, PACK_NUMBER, PACK_TILES, PACK_ACTION, PACK_40,
                                       PACK_ES48};
    uint8_t all[CARDS_MAX], seen[256] = {0};
    int n = cards_pack(PACK_OF[deck], all), k = 0;
    for (int i = 0; i < n; i++)
        if (!seen[all[i]]) seen[all[i]] = 1, out[k++] = all[i];
    out[k++] = CARD_NONE;
    return k;
}

static int cached(int deck, int card, int w)
{
    for (int i = 0; i < CACHE; i++)
        if (cache[i].t && cache[i].deck == deck && cache[i].card == card && cache[i].w == w) return 1;
    return 0;
}

/* the face (deck, card) at width w asked for and painted on the spot: its width counted; at the second card, the deck
 * painted ahead at that width. Called with PF.m held. */
static void pf_note(int deck, int card, int w)
{
    int k = -1, old = 0;
    for (int i = 0; i < PF_WIDTHS; i++) {
        if (PF.width[deck][i].w == w) k = i;
        if (PF.width[deck][i].used < PF.width[deck][old].used) old = i;
    }
    if (k < 0) {                                /* a width not seen lately: in the place of the oldest */
        k = old;
        memset(&PF.width[deck][k], 0, sizeof PF.width[deck][k]);
        PF.width[deck][k].w = w;
    }
    PF.width[deck][k].used = ++PF.clock;
    uint8_t *seen = PF.width[deck][k].seen;
    if (seen[card >> 3] & (1 << (card & 7))) return;
    seen[card >> 3] |= (uint8_t)(1 << (card & 7));
    if (++PF.width[deck][k].cards != 2) return;
    for (int i = 0; i < PF.ndone; i++) {        /* the faces painted for a width no longer in use: let go */
        int keep = PF.done[i].deck != deck;
        for (int j = 0; j < PF_WIDTHS && !keep; j++) keep = PF.width[deck][j].w == PF.done[i].w;
        if (keep) continue;
        PF.bytes -= (size_t)PF.done[i].w * PF.done[i].h * 4;
        free(PF.done[i].px);
        PF.done[i--] = PF.done[--PF.ndone];
    }
    uint8_t pack[CARDS_MAX + 1];
    int n = pf_pack(deck, pack);
    for (int i = 0; i < n && PF.njob < PF_JOBS; i++) {
        if (seen[pack[i] >> 3] & (1 << (pack[i] & 7)) || cached(deck, pack[i], w)) continue;
        PF.job[PF.njob].deck = (uint8_t)deck;
        PF.job[PF.njob].card = pack[i];
        PF.job[PF.njob++].w = w;
    }
    SDL_CondSignal(PF.wake);
}

/* the face painted ahead (or being painted: waited for), taken: its pixels (the caller frees them), else NULL */
static uint8_t *pf_take(int deck, int card, int w)
{
    if (deck >= DECKS || !pf_start()) return NULL;
    SDL_LockMutex(PF.m);
    while (PF.doing_w == w && PF.doing_deck == deck && PF.doing_card == card) SDL_CondWait(PF.done_cv, PF.m);
    uint8_t *px = NULL;
    for (int i = 0; i < PF.ndone; i++)
        if (PF.done[i].deck == deck && PF.done[i].card == card && PF.done[i].w == w) {
            px = PF.done[i].px;
            PF.bytes -= (size_t)PF.done[i].w * PF.done[i].h * 4;
            PF.done[i] = PF.done[--PF.ndone];
            SDL_CondSignal(PF.wake);            /* room again */
            break;
        }
    if (!px) {
        for (int i = 0; i < PF.njob; i++)       /* waiting its turn: the caller paints it now */
            if (PF.job[i].deck == deck && PF.job[i].card == card && PF.job[i].w == w) {
                memmove(PF.job + i, PF.job + i + 1, sizeof PF.job[0] * (size_t)(--PF.njob - i));
                break;
            }
        pf_note(deck, card, w);
    }
    SDL_UnlockMutex(PF.m);
    return px;
}

/* a card's picture w pixels wide; near: any size already made will do (a card growing as it slides) */
static SDL_Texture *card_tex(int deck, int card, int w, int near)
{
    if (w < 4) return NULL;
    int best = -1, free_slot = 0;
    for (int i = 0; i < CACHE; i++) {
        if (!cache[i].t) { free_slot = i; continue; }
        if (cache[i].deck != deck || cache[i].card != card) continue;
        if (cache[i].w == w) { cache[i].used = ++tick; return cache[i].t; }
        if (near && (best < 0 || abs(cache[i].w - w) < abs(cache[best].w - w))) best = i;
    }
    if (best >= 0) { cache[best].used = ++tick; return cache[best].t; }
    int slot = free_slot;
    if (cache[slot].t)                          /* full: the least used lately */
        for (int i = 0; i < CACHE; i++)
            if (cache[i].used < cache[slot].used) slot = i;
    for (int i = 0; i < CACHE && cache[slot].t; i++)
        if (!cache[i].t) slot = i;
    ui_free(&cache[slot].t);
    int h = (int)(w * cards_aspect(deck) + 0.5f);
    uint8_t *b = near ? NULL : pf_take(deck, card, w);   /* painted ahead */
    if (!b) {
        b = malloc((size_t)w * h * 4);
        if (!b) return NULL;
        cards_paint(deck, card, w, h, b);
    }
    cache[slot].deck = deck;
    cache[slot].card = card;
    cache[slot].w = w;
    cache[slot].used = ++tick;
    cache[slot].t = ui_tex(w, h, b);
    free(b);
    return cache[slot].t;
}

/* soft shapes the size of a card, scaled: the shadow, the gold light, the empty place's outline */
static SDL_Texture *soft(int kind, int deck)
{
    static SDL_Texture *t[3][DECKS_MAX];
    if (t[kind][deck]) return t[kind][deck];
    int w = 96, h = (int)(w * cards_aspect(deck) + 0.5f), pad = kind == 2 ? 0 : 16, W = w + 2 * pad, H = h + 2 * pad;
    uint8_t *b = calloc((size_t)W * H, 4);
    if (!b) return NULL;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float d = rr_dist(x + 0.5f - pad, y + 0.5f - pad, (float)w, (float)h, 0.075f * w);
            uint8_t *p = b + 4 * (y * W + x);
            if (kind == 0) p[3] = (uint8_t)(120 * clamp01(0.5f - d / 12));          /* shadow: soft edge */
            else if (kind == 1) {                                                  /* gold light round it */
                p[0] = 250, p[1] = 214, p[2] = 90;
                p[3] = (uint8_t)(230 * powf(clamp01(1 - fabsf(d) / 15), 1.6f));
            } else {                                                               /* a faint outline */
                p[0] = 246, p[1] = 238, p[2] = 220;
                p[3] = (uint8_t)(90 * clamp01(0.5f - d) * clamp01(d + 2.5f)) + (uint8_t)(18 * clamp01(-d));
            }
        }
    t[kind][deck] = ui_tex(W, H, b);
    free(b);
    return t[kind][deck];
}

/* a texture drawn at d turned deg, faded (alpha below 255: set for this copy only) */
static void put_tex(SDL_Texture *t, const SDL_FRect *d, float deg, Uint8 alpha)
{
    if (!t) return;
    if (alpha < 255) SDL_SetTextureAlphaMod(t, alpha);
    SDL_RenderCopyExF(ui.r, t, NULL, d, deg, NULL, SDL_FLIP_NONE);
    if (alpha < 255) SDL_SetTextureAlphaMod(t, 255);
}

/* the card's picture (or its back) at (cx, cy), w x h, narrowed to sx of its width (a flip), turned deg, faded */
static void put_card(int deck, int card, float cx, float cy, float w, float sx, float deg, int how, int near,
                     Uint8 alpha)
{
    float h = w * cards_aspect(deck), pad = 16 / 96.0f * w;
    if (how & CD_RAISED) {                      /* lifted towards the card's top */
        float k = 0.14f * w, a = deg * PI_F / 180;
        cx += sinf(a) * k;
        cy -= cosf(a) * k;
    }
    if (!(how & CD_FLAT)) {
        float off = (how & CD_RAISED ? 0.09f : 0.035f) * w;
        SDL_FRect d = {cx + off * 0.6f - (w * sx) / 2 - pad * sx, cy + off - h / 2 - pad, (w + 2 * pad) * sx,
                       h + 2 * pad};
        put_tex(soft(0, deck), &d, deg, alpha);
    }
    if (how & (CD_GLOW | CD_RAISED)) {
        SDL_FRect d = {cx - (w * sx) / 2 - pad * sx, cy - h / 2 - pad, (w + 2 * pad) * sx, h + 2 * pad};
        put_tex(soft(1, deck), &d, deg, alpha);
    }
    SDL_Texture *t = card_tex(deck, how & CD_DOWN ? CARD_NONE : card, (int)(w + 0.5f), near);
    if (!t) return;
    if (how & CD_DIM) SDL_SetTextureColorMod(t, 150, 150, 150);
    SDL_FRect d = {cx - w * sx / 2, cy - h / 2, w * sx, h};
    put_tex(t, &d, deg, alpha);
    if (how & CD_DIM) SDL_SetTextureColorMod(t, 255, 255, 255);
}

void cards_draw(int deck, int card, float cx, float cy, float w, float deg, int how)
{
    put_card(deck, card, cx, cy, w, 1, deg, how, 0, 255);
}

void cards_draw_alpha(int deck, int card, float cx, float cy, float w, float deg, int how, float alpha)
{
    put_card(deck, card, cx, cy, w, 1, deg, how, 0, (Uint8)(255 * clamp01(alpha) + 0.5f));
}

void cards_draw_flip(int deck, int card, float cx, float cy, float w, float deg, float t, int how)
{
    t = clamp01(t);
    float sx = fabsf(cosf(t * PI_F)), lift = 1 + 0.08f * sinf(t * PI_F);
    if (sx < 0.02f) sx = 0.02f;
    put_card(deck, card, cx, cy, w * lift, sx, deg, (how & ~CD_DOWN) | (t < 0.5f ? CD_DOWN : 0), 1, 255);
}

void cards_place(int deck, float cx, float cy, float w)
{
    float h = w * cards_aspect(deck);
    ui_put_c(soft(2, deck), cx, cy, w, h);
}

void cards_pile(int deck, float cx, float cy, float w, int n, int top, int how)
{
    if (n <= 0) {
        if (how & CD_GLOW) ui_mark_gold(cx, cy, 1.3f * w);   /* an empty place to tap */
        cards_place(deck, cx, cy, w);
        return;
    }
    int layers = n > 1 ? (n < 4 ? n - 1 : n < 20 ? 3 : n < 50 ? 4 : 5) : 0;
    float step = fmaxf(1, 0.02f * w);
    for (int k = layers; k >= 1; k--)           /* the cards under: their edges, lower right */
        put_card(deck, top, cx + k * step * 0.4f, cy + k * step, w, 1, 0,
                 (top == CARD_NONE ? CD_DOWN : 0) | (k == layers ? 0 : CD_FLAT), 0, 255);
    int look = (top == CARD_NONE ? CD_DOWN : 0) | (layers ? CD_FLAT : 0) | (how & ~CD_DOWN);
    put_card(deck, top, cx, cy, w, 1, 0, look, 0, 255);
}

/* ---- hands ------------------------------------------------------------------------------------------------------- */
void cards_fan(float cx, float cy, float width, float w, float aspect, int n, float arc, cards_spot *s)
{
    (void)aspect;
    if (n <= 0) return;
    float step = n > 1 ? fminf(1.06f * w, (width - w) / (n - 1)) : 0;
    for (int i = 0; i < n; i++) {
        float t = n > 1 ? (float)i / (n - 1) - 0.5f : 0, dx = (i - (n - 1) / 2.0f) * step;
        s[i].deg = arc * t;
        s[i].x = cx + dx;
        s[i].y = cy + fabsf(dx) * tanf(fabsf(s[i].deg) * PI_F / 180) * 0.5f;
    }
}

int cards_in(float cx, float cy, float w, float aspect, float x, float y)
{
    return fabsf(x - cx) <= w / 2 && fabsf(y - cy) <= w * aspect / 2;
}

int cards_hit(const cards_spot *s, int n, float w, float aspect, float x, float y)
{
    for (int i = n - 1; i >= 0; i--) {          /* the top one first */
        float a = -s[i].deg * PI_F / 180, dx = x - s[i].x, dy = y - s[i].y;
        float lx = dx * cosf(a) - dy * sinf(a), ly = dx * sinf(a) + dy * cosf(a);
        if (fabsf(lx) <= w / 2 && fabsf(ly) <= w * aspect / 2) return i;
    }
    return -1;
}

/* ---- slides ------------------------------------------------------------------------------------------------------ */
void cards_slide_start(cards_slide *s, int deck, int card, float x0, float y0, float w0, float a0, int up0, float x1,
                       float y1, float w1, float a1, int up1, int ms, int delay)
{
    *s = (cards_slide){1, deck, card, up0, up1, ms > 0 ? ms : 1, delay, x0, y0, w0, a0, x1, y1, w1, a1, ui_ms(), 0, 0};
}

int cards_slide_busy(const cards_slide *s) { return s->on && ui_ms() - s->t0 < (uint32_t)(s->ms + s->delay); }

int cards_slide_draw(cards_slide *s)
{
    if (!s->on) return 0;
    uint32_t el = ui_ms() - s->t0;
    if (el >= (uint32_t)(s->ms + s->delay)) { s->on = 0; return 0; }
    ui_animate();
    float t = el < (uint32_t)s->delay ? 0 : (float)(el - s->delay) / s->ms, e = t * t * (3 - 2 * t);
    float x = s->x0 + (s->x1 - s->x0) * e, y = s->y0 + (s->y1 - s->y0) * e, w = s->w0 + (s->w1 - s->w0) * e;
    float a = s->a0 + (s->a1 - s->a0) * e, moved = hypotf(s->x1 - s->x0, s->y1 - s->y0);
    float hop = s->hop > 0 ? s->hop : 0.25f, reach = s->reach > 0 ? s->reach : 0.12f;
    y -= sinf(e * PI_F) * fminf(hop * w, reach * moved);       /* a little lift on the way */
    if (s->up0 != s->up1) cards_draw_flip(s->deck, s->card, x, y, w, a, s->up1 ? e : 1 - e, 0);
    else put_card(s->deck, s->card, x, y, w, 1, a, s->up0 ? 0 : CD_DOWN, 1, 255);
    return 1;
}
