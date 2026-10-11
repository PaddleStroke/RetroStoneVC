/*
 * Mystery Face in RetroStone Board: the face-guessing game for two passing the device (the rules: mysteryface_rules.c,
 * mysteryface.h). VIEW_HIDDEN: each player's view is their own board of 24 faces, their secret face small in the
 * corner. The faces are our own cartoon portraits, drawn by code (painted once at the size they are shown), each in a
 * small wooden frame standing on a plank in the walnut frame; a face put down shows the back of its frame.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The table: the board on top (6 x 4 in portrait, 8 x 3 in landscape); the questions under it, a row of picture
 * buttons (hat, glasses, beard, moustache, earrings, bald, long hair, curly hair, the five hair colours as swatches,
 * the three eye colours); then the player's secret face, the answer (the question asked and a big tick or cross), the
 * guess button (a magnifying glass) and the end of the turn (pass the device; at the start, a tick: the face picked).
 * Taps only: a face to pick it, to put it down or stand it up again, or (the guess button on) to guess it.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/guessart.h"
#include "games/mysteryface.h"
#include "shell.h"

#define FLIP_MS 240
#define POP_MS 300
#define FA 1.17f                                /* a frame's height / width */
#define BORDER 0.075f                           /* its wood, of its width */

static mfgame G;
static const mfpos *P = &G.pos;
static int guessing;                            /* the guess button on: the next face tapped is the guess */
static Uint32 flip_t0[MF_FACES], pop_t0;
static int flip_on[MF_FACES], pop_on;

/* ---- the faces --------------------------------------------------------------------------------------------------- */
static const float SKIN[SKINS][3] = {{250, 224, 200}, {234, 192, 152}, {200, 146, 104}, {146, 96, 62}, {96, 62, 42}};
static const float HAIRC[HAIRS][3] = {{46, 40, 38}, {118, 74, 42}, {236, 198, 112}, {200, 88, 42}, {218, 218, 212}};
static const float IRIS[EYES][3] = {{116, 70, 36}, {62, 128, 206}, {66, 152, 74}};
static const float BACKS[6][3] = {{196, 222, 236}, {238, 214, 196}, {212, 232, 198}, {232, 212, 230},
                                  {244, 232, 190}, {206, 214, 238}};
static const float SHIRT[7][3] = {{70, 110, 172}, {176, 72, 60}, {58, 130, 100}, {222, 170, 62}, {122, 82, 152},
                                  {64, 66, 76}, {232, 124, 92}};
static const float HATC[5][3] = {{96, 64, 42}, {40, 72, 116}, {164, 52, 50}, {62, 104, 62}, {70, 70, 76}};
static const float WHITE[3] = {250, 250, 246}, INK[3] = {28, 22, 22}, GOLD[3] = {234, 186, 66},
                   GLASS[3] = {46, 42, 48}, LIP[3] = {168, 70, 66};

/* what a portrait shows (a face of the game, or the questions' plain head) */
typedef struct {
    const float *skin, *hair, *iris, *shirt, *hat, *beard;
    int style, has, body;
} look;

static look look_of(int f)
{
    const mf_face *c = &MF_FACE[f];
    look l = {SKIN[c->skin], HAIRC[c->hair], IRIS[c->eyes], SHIRT[(f * 3 + 1) % 7], HATC[f % 5], HAIRC[c->hair],
              c->style, c->has, 1};
    return l;
}

static void shade(const float *c, float k, float *o)
{
    for (int i = 0; i < 3; i++) o[i] = fminf(255, c[i] * k);
}

/* a portrait in the unit frame (1 wide, 1.2 tall), back to front */
static void paint_look(ga_pen *p, const look *L)
{
    float lw = fmaxf(0.9f, p->s * 0.011f), c[3], d[3];
    if (L->style == STYLE_LONG) {               /* long hair: behind the head, down to the shoulders */
        const float back[] = {0.205f, 0.5f, 0.795f, 0.5f, 0.84f, 0.99f, 0.16f, 0.99f};
        ga_ell(p, ART_PART, 0.5f, 0.5f, 0.3f, 0.3f);
        ga_poly(p, ART_PART, back, 4);
        shade(L->hair, 0.86f, c);
        ga_paint(p, c, NULL, 0.2f, lw);
    }
    if (L->body) {
        shade(L->skin, 0.86f, c);
        ga_rect(p, ART_PART, 0.43f, 0.7f, 0.57f, 0.96f);
        ga_paint(p, c, NULL, 0.15f, lw);
        ga_ell(p, ART_PART, 0.5f, 1.2f, 0.41f, 0.29f);
        ga_paint(p, L->shirt, NULL, 0.3f, lw);
    }
    ga_ell(p, ART_PART, 0.265f, 0.55f, 0.045f, 0.065f);   /* the ears, the head */
    ga_ell(p, ART_PART, 0.735f, 0.55f, 0.045f, 0.065f);
    ga_paint(p, L->skin, NULL, 0.2f, lw);
    ga_ell(p, ART_PART, 0.5f, 0.52f, 0.235f, 0.275f);
    if (L->style == STYLE_BALD) ga_ell(p, ART_MARK, 0.42f, 0.34f, 0.07f, 0.035f);   /* a shine */
    shade(L->skin, 1.12f, d);
    ga_paint(p, L->skin, d, 0.2f, lw);
    /* the eyes: the whites, the irises (their colour), the pupils, a glint */
    ga_ell(p, ART_PART, 0.41f, 0.535f, 0.054f, 0.044f);
    ga_ell(p, ART_PART, 0.59f, 0.535f, 0.054f, 0.044f);
    ga_paint(p, WHITE, NULL, 0, lw * 0.8f);
    ga_ell(p, ART_PART, 0.413f, 0.54f, 0.032f, 0.036f);
    ga_ell(p, ART_PART, 0.593f, 0.54f, 0.032f, 0.036f);
    ga_ell(p, ART_MARK, 0.413f, 0.542f, 0.014f, 0.016f);
    ga_ell(p, ART_MARK, 0.593f, 0.542f, 0.014f, 0.016f);
    ga_paint(p, L->iris, INK, 0.25f, 0);
    ga_ell(p, ART_PART, 0.403f, 0.528f, 0.008f, 0.008f);
    ga_ell(p, ART_PART, 0.583f, 0.528f, 0.008f, 0.008f);
    ga_paint(p, WHITE, NULL, 0, 0);
    /* the eyebrows (the hair's colour; pale hair a little darker) */
    shade(L->hair, L->hair[0] > 180 ? 0.72f : 0.9f, c);
    ga_arc(p, ART_PART, 0.41f, 0.535f, 0.068f, 0.082f, 55, 125);
    ga_arc(p, ART_PART, 0.59f, 0.535f, 0.068f, 0.082f, 55, 125);
    ga_paint(p, c, NULL, 0, 0);
    shade(L->skin, 0.84f, c);                   /* the nose */
    ga_ell(p, ART_PART, 0.5f, 0.605f, 0.03f, 0.024f);
    ga_paint(p, c, NULL, 0.3f, 0);
    if (L->has & MF_BEARD) {                    /* round the jaw, up to the cheeks */
        float b[2 * 24];
        int n = 0;
        for (int i = 0; i <= 16; i++) {
            float a = (196 + 148.0f * i / 16) * 3.14159265f / 180;
            b[n++] = 0.5f + 0.25f * cosf(a);
            b[n++] = 0.52f - 0.3f * sinf(a);
        }
        const float top[] = {0.68f, 0.635f, 0.6f, 0.66f, 0.5f, 0.668f, 0.4f, 0.66f, 0.32f, 0.635f};
        memcpy(b + n, top, sizeof top);
        n += 10;
        ga_poly(p, ART_PART, b, n / 2);
        ga_paint(p, L->beard, NULL, 0.25f, lw);
    }
    ga_arc(p, ART_PART, 0.5f, 0.62f, 0.07f, 0.09f, 207, 333);   /* a smile */
    ga_paint(p, LIP, NULL, 0, 0);
    if (L->has & MF_MOUSTACHE) {
        const float m1[] = {0.5f, 0.648f, 0.455f, 0.643f, 0.418f, 0.662f, 0.4f, 0.69f, 0.45f, 0.675f, 0.5f, 0.672f};
        const float m2[] = {0.5f, 0.648f, 0.545f, 0.643f, 0.582f, 0.662f, 0.6f, 0.69f, 0.55f, 0.675f, 0.5f, 0.672f};
        ga_poly(p, ART_PART, m1, 6);
        ga_poly(p, ART_PART, m2, 6);
        ga_paint(p, L->beard, NULL, 0.2f, lw * 0.8f);
    }
    /* the hair in front */
    float h[2 * 40];
    int n = 0;
    switch (L->style) {
    case STYLE_SHORT: {
        for (int i = 0; i <= 14; i++) {
            float a = (-8 + 196.0f * i / 14) * 3.14159265f / 180;
            h[n++] = 0.5f + 0.255f * cosf(a);
            h[n++] = 0.5f - 0.3f * sinf(a);
        }
        const float fringe[] = {0.3f, 0.42f, 0.4f, 0.37f, 0.52f, 0.395f, 0.63f, 0.36f, 0.71f, 0.42f};
        memcpy(h + n, fringe, sizeof fringe);
        n += 10;
        ga_poly(p, ART_PART, h, n / 2);
        break;
    }
    case STYLE_LONG: {
        for (int i = 0; i <= 14; i++) {
            float a = (-22 + 224.0f * i / 14) * 3.14159265f / 180;
            h[n++] = 0.5f + 0.262f * cosf(a);
            h[n++] = 0.5f - 0.31f * sinf(a);
        }
        const float in[] = {0.3f, 0.62f, 0.31f, 0.45f, 0.38f, 0.37f, 0.48f, 0.34f, 0.56f, 0.37f, 0.66f, 0.4f,
                            0.7f, 0.47f, 0.7f, 0.62f};
        memcpy(h + n, in, sizeof in);
        n += 16;
        ga_poly(p, ART_PART, h, n / 2);
        break;
    }
    case STYLE_CURLY:
        ga_ell(p, ART_PART, 0.5f, 0.38f, 0.25f, 0.15f);
        for (int i = 0; i < 13; i++) {
            float a = (-28 + 236.0f * i / 12) * 3.14159265f / 180;
            ga_ell(p, ART_PART, 0.5f + 0.25f * cosf(a), 0.47f - 0.25f * sinf(a), 0.058f, 0.058f);
        }
        break;
    case STYLE_BALD:                            /* a little hair over the ears */
        ga_ell(p, ART_PART, 0.272f, 0.47f, 0.036f, 0.075f);
        ga_ell(p, ART_PART, 0.728f, 0.47f, 0.036f, 0.075f);
        break;
    }
    ga_paint(p, L->hair, NULL, 0.25f, lw);
    if (L->has & MF_GLASSES) {
        ga_arc(p, ART_PART, 0.41f, 0.535f, 0.062f, 0.076f, 0, 360);
        ga_arc(p, ART_PART, 0.59f, 0.535f, 0.062f, 0.076f, 0, 360);
        ga_rect(p, ART_PART, 0.47f, 0.524f, 0.53f, 0.538f);
        ga_paint(p, GLASS, NULL, 0, 0);
    }
    if (L->has & MF_EARRINGS) {
        ga_ell(p, ART_PART, 0.262f, 0.63f, 0.02f, 0.02f);
        ga_ell(p, ART_PART, 0.738f, 0.63f, 0.02f, 0.02f);
        ga_ell(p, ART_PART, 0.262f, 0.678f, 0.024f, 0.03f);
        ga_ell(p, ART_PART, 0.738f, 0.678f, 0.024f, 0.03f);
        ga_paint(p, GOLD, NULL, 0.6f, lw * 0.7f);
    }
    if (L->has & MF_HAT) {                      /* a crown, its band, the brim */
        const float crown[] = {0.31f, 0.3f, 0.33f, 0.11f, 0.4f, 0.075f, 0.6f, 0.075f, 0.67f, 0.11f, 0.69f, 0.3f};
        ga_poly(p, ART_PART, crown, 6);
        ga_paint(p, L->hat, NULL, 0.3f, lw);
        shade(L->hat, 0.6f, c);
        ga_rect(p, ART_PART, 0.315f, 0.235f, 0.685f, 0.285f);
        ga_paint(p, c, NULL, 0.2f, 0);
        ga_ell(p, ART_PART, 0.5f, 0.3f, 0.34f, 0.055f);
        ga_paint(p, L->hat, NULL, 0.3f, lw);
    }
}

static float grain(int x, int y, float along)
{
    return sinf(along * 0.21f + 2.2f * sinf(along * 0.013f + x * 0.05f + y * 0.03f)) * 0.5f + 0.5f;
}

/* a frame, w x h: its wood all round, and inside the portrait (look) or its back (NULL) */
static void paint_frame(int w, int h, const look *L, const float *bg, uint8_t *o)
{
    static const float wood[3] = {204, 150, 96}, back[3] = {164, 114, 72};
    int t = (int)fmaxf(2, roundf(BORDER * w)), iw = w - 2 * t, ih = h - 2 * t;
    float rad = fmaxf(1.5f, 0.06f * w);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *q = o + 4 * ((size_t)y * w + x);
            int in = x >= t && y >= t && x < w - t && y < h - t;
            float c[3], k;
            if (!in) {                          /* the frame: lit from the top left, a dark line next to the inside */
                int side = x < t || x >= w - t;
                k = 1 + 0.08f * (grain(x, y, side ? (float)y : (float)x) - 0.5f);
                float e = fminf(fminf((float)x, (float)(w - 1 - x)), fminf((float)y, (float)(h - 1 - y)));
                if (e < 0.35f * t) k *= (x + y < (w + h) / 2) ? 1.14f : 0.84f;
                else if (e > t - 1.2f) k *= 0.62f;
                for (int i = 0; i < 3; i++) c[i] = wood[i] * k;
            } else if (!L) {                    /* the back: a darker panel, its grain upright */
                k = 1 + 0.1f * (grain(x, y, (float)x * 3) - 0.5f);
                float e = fminf(fminf((float)(x - t), (float)(w - t - 1 - x)),
                                fminf((float)(y - t), (float)(h - t - 1 - y)));
                if (e < 1.5f) k *= 0.8f;
                for (int i = 0; i < 3; i++) c[i] = back[i] * k;
            } else {                            /* the portrait's ground, lighter in the middle */
                float fx = (x - t + 0.5f) / iw - 0.5f, fy = (y - t + 0.5f) / ih - 0.42f;
                k = 1.06f - 0.22f * (fx * fx + fy * fy);
                for (int i = 0; i < 3; i++) c[i] = bg[i] * k;
            }
            float a = 1;                        /* rounded corners */
            float cx = fmaxf(rad - x, x - (w - 1 - rad)), cy = fmaxf(rad - y, y - (h - 1 - rad));
            if (cx > 0 && cy > 0) a = clampf(rad - sqrtf(cx * cx + cy * cy) + 0.5f, 0, 1);
            for (int i = 0; i < 3; i++) q[i] = (uint8_t)clampf(c[i], 0, 255);
            q[3] = (uint8_t)(255 * a);
        }
    if (!L || iw < 4 || ih < 4) return;
    uint8_t *pic = calloc((size_t)iw * ih, 4);  /* the portrait in its own picture: clipped to the inside */
    if (!pic) return;
    ga_pen p;
    ga_begin(&p, pic, iw, ih, (float)iw, 0, (ih - 1.2f * iw) / 2);
    paint_look(&p, L);
    ga_end(&p);
    art_blend(o, w, h, pic, iw, ih, t, t);
    free(pic);
}

static void paint_face(int f, int w, int h, uint8_t *o)
{
    look l = look_of(f);
    paint_frame(w, h, &l, BACKS[f % 6], o);
}

/* ---- the questions' pictures ------------------------------------------------------------------------------------- */
static const float PLAIN_SKIN[3] = {222, 206, 190}, PLAIN_HAIR[3] = {150, 146, 152}, STYLE_HAIR[3] = {92, 88, 96},
                   PLAIN_EYE[3] = {70, 70, 76}, FEATURE[3] = {96, 66, 44}, ICON_HAT[3] = {164, 52, 50};

/* round: the picture inside a circle (the button's ivory medallion) */
static void round_off(int size, uint8_t *o)
{
    float r = size / 2.0f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float d = hypotf(x + 0.5f - r, y + 0.5f - r), a = clampf(r - d, 0, 1);
            o[4 * ((size_t)y * size + x) + 3] = (uint8_t)(o[4 * ((size_t)y * size + x) + 3] * a);
        }
}

static void paint_question(int q, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    ga_pen p;
    if (q >= MQ_BLACK && q <= MQ_WHITE) {       /* a swatch of the hair, its strands */
        const float *c = HAIRC[q - MQ_BLACK];
        float m[3];
        shade(c, c[0] > 180 ? 0.72f : 1.45f, m);
        ga_begin(&p, o, size, size, (float)size, 0, 0);
        ga_ell(&p, ART_PART, 0.5f, 0.5f, 0.4f, 0.4f);
        for (int i = 0; i < 4; i++) {
            float r = 0.12f + 0.07f * i;
            ga_arc(&p, ART_MARK, 0.5f + 0.06f * (i & 1), 0.72f, r, r + 0.018f, 70 + 12 * i, 160 - 6 * i);
        }
        ga_paint(&p, c, m, 0.35f, fmaxf(1, size * 0.02f));
        ga_end(&p);
    } else if (q >= MQ_EYE_BROWN) {            /* an eye, its iris the colour */
        ga_begin(&p, o, size, size, (float)size, 0, 0);
        float a[2 * 34];
        int n = 0;
        for (int i = 0; i <= 16; i++) {         /* an almond: two arcs */
            float t = 3.14159265f * i / 16;
            a[n++] = 0.5f - 0.36f * cosf(t);
            a[n++] = 0.5f - 0.2f * sinf(t);
        }
        for (int i = 1; i < 16; i++) {
            float t = 3.14159265f * i / 16;
            a[n++] = 0.5f + 0.36f * cosf(t);
            a[n++] = 0.5f + 0.2f * sinf(t);
        }
        ga_poly(&p, ART_PART, a, n / 2);
        ga_paint(&p, WHITE, NULL, 0, fmaxf(1, size * 0.03f));
        ga_ell(&p, ART_PART, 0.5f, 0.5f, 0.17f, 0.17f);
        ga_ell(&p, ART_MARK, 0.5f, 0.5f, 0.075f, 0.075f);
        ga_paint(&p, IRIS[q - MQ_EYE_BROWN], INK, 0.3f, fmaxf(1, size * 0.02f));
        ga_ell(&p, ART_PART, 0.45f, 0.45f, 0.035f, 0.035f);
        ga_paint(&p, WHITE, NULL, 0, 0);
        ga_end(&p);
    } else {                                    /* a plain head showing the feature */
        look l = {PLAIN_SKIN, PLAIN_HAIR, PLAIN_EYE, NULL, ICON_HAT, FEATURE, STYLE_SHORT, 0, 0};
        switch (q) {
        case MQ_HAT: l.has = MF_HAT; break;
        case MQ_GLASSES: l.has = MF_GLASSES; break;
        case MQ_BEARD: l.has = MF_BEARD; break;
        case MQ_MOUSTACHE: l.has = MF_MOUSTACHE; break;
        case MQ_EARRINGS: l.has = MF_EARRINGS; break;
        case MQ_BALD: l.style = STYLE_BALD; l.hair = STYLE_HAIR; break;
        case MQ_LONG: l.style = STYLE_LONG; l.hair = STYLE_HAIR; break;
        case MQ_CURLY: l.style = STYLE_CURLY; l.hair = STYLE_HAIR; break;
        }
        float s = size / (q == MQ_HAT ? 0.84f : 0.74f);
        ga_begin(&p, o, size, size, s, size / 2.0f - 0.5f * s, size / 2.0f - (q == MQ_HAT ? 0.43f : 0.47f) * s);
        paint_look(&p, &l);
        ga_end(&p);
    }
    round_off(size, o);
}

/* the guess: a magnifying glass (white: tinted cream) */
static void paint_glass(int size, uint8_t *o)
{
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    art_arc(f, ART_PART, 0.43f, 0.42f, 0.17f, 0.25f, 0, 360);
    const float handle[] = {0.58f, 0.62f, 0.64f, 0.56f, 0.86f, 0.78f, 0.80f, 0.84f};
    art_poly(f, ART_PART, handle, 4);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

static void paint_device(int size, uint8_t *o) { art_icon(ICON_DEVICE, size, o); }

/* ---- the table's geometry ---------------------------------------------------------------------------------------- */
static struct {
    box b;
    int cols, rows, wide;
    float bx, by;                               /* the plank */
    int bw, bh, sq, fr;
    float fw, fh, gap, fx0, fy0;                /* the frames */
    float qd, qx[MF_QUESTIONS], qy[MF_QUESTIONS];
    float sx, sy, sw, sh;                       /* the secret face, small */
    float ax, ay, ad, kx;                       /* the answer, the question asked beside it */
    float gx, gy, ex, ey, bd;                   /* the guess and the end of the turn */
} T;

static void layout(box b)
{
    float u = ui.unit;
    T.b = b;
    T.wide = b.w > b.h * 1.05f;
    T.cols = T.wide ? 8 : 6;
    T.rows = T.wide ? 3 : 4;
    float wk = T.cols + (T.cols - 1) * 0.08f + 0.68f, hk = T.rows * FA + (T.rows - 1) * 0.08f + 0.68f;
    T.qd = floorf(fminf(0.92f * u, (T.wide ? b.w * 0.66f : b.w) / 8 / 1.12f));
    float pitch = T.qd * 1.12f, qh = 2 * pitch, ah = T.wide ? 0 : 1.75f * u, room = qh + ah + 0.3f * u;
    if (T.wide) room = fmaxf(qh, 2.3f * u) + 0.3f * u;
    float fw = floorf(fminf(b.w * 0.99f / wk, (b.h - room) / hk));
    T.fw = fw;
    T.fh = floorf(fw * FA);
    T.gap = floorf(0.08f * fw);
    T.fr = (int)fmaxf(3, 0.22f * fw);
    int iw = (int)(T.cols * fw + (T.cols - 1) * T.gap + 0.24f * fw);
    int ih = (int)(T.rows * T.fh + (T.rows - 1) * T.gap + 0.24f * fw);
    T.sq = (int)fmaxf(4, fw / 2);               /* the plank's wood: plain, its grain on this scale */
    int nx = (iw + T.sq - 1) / T.sq, ny = (ih + T.sq - 1) / T.sq;
    T.bw = nx * T.sq + 2 * T.fr;
    T.bh = ny * T.sq + 2 * T.fr;
    float total = T.bh + room, spare = fmaxf(0, b.h - total);
    T.bx = floorf(b.x + (b.w - T.bw) / 2);
    T.by = floorf(b.y + spare * 0.3f);
    T.fx0 = floorf(T.bx + (T.bw - (T.cols * fw + (T.cols - 1) * T.gap)) / 2);
    T.fy0 = floorf(T.by + (T.bh - (T.rows * T.fh + (T.rows - 1) * T.gap)) / 2);
    float top = T.by + T.bh + 0.25f * u + spare * 0.25f;
    float qw = 8 * pitch, qx0 = T.wide ? b.x + 0.1f * u : b.x + (b.w - qw) / 2;
    for (int q = 0; q < MF_QUESTIONS; q++) {
        T.qx[q] = qx0 + (q % 8 + 0.5f) * pitch;
        T.qy[q] = top + (q / 8 + 0.5f) * pitch;
    }
    T.bd = floorf(fminf(1.0f * u, T.qd * 1.15f));
    if (!T.wide) {                              /* a row: the secret, the answer, guess, end */
        float cy = top + qh + 0.2f * u + spare * 0.2f + ah / 2;
        T.sh = floorf(1.55f * u);
        T.sw = floorf(T.sh / FA);
        T.sx = floorf(b.x + 0.2f * u);
        T.sy = floorf(cy - T.sh / 2);
        T.ad = floorf(1.3f * u);
        T.ax = b.x + b.w * 0.44f;
        T.kx = b.x + b.w * 0.29f;
        T.ay = cy;
        T.gx = b.x + b.w * 0.68f;
        T.ex = b.x + b.w - 0.2f * u - T.bd / 2;
        T.gy = T.ey = cy;
    } else {                                    /* a block of two rows beside the questions */
        float x0 = qx0 + qw + 0.2f * u, x1 = b.x + b.w, cy1 = top + pitch * 0.5f, cy2 = top + pitch * 1.5f;
        T.sh = floorf(fminf(1.2f * u, pitch * 1.05f));
        T.sw = floorf(T.sh / FA);
        T.sx = floorf(x0 + (x1 - x0) * 0.17f - T.sw / 2);
        T.sy = floorf(cy1 - T.sh / 2);
        T.ad = floorf(fminf(1.1f * u, pitch));
        T.ax = x0 + (x1 - x0) * 0.82f;
        T.kx = x0 + (x1 - x0) * 0.5f;
        T.ay = cy1;
        T.gx = x0 + (x1 - x0) * 0.25f;
        T.ex = x0 + (x1 - x0) * 0.68f;
        T.gy = T.ey = cy2;
        T.bd = floorf(fminf(T.bd, pitch * 0.92f));
    }
}

static void face_rect(int f, float *x, float *y)
{
    *x = T.fx0 + (f % T.cols) * (T.fw + T.gap);
    *y = T.fy0 + (f / T.cols) * (T.fh + T.gap);
}

static int face_at(float x, float y)
{
    for (int f = 0; f < MF_FACES; f++) {
        float fx, fy;
        face_rect(f, &fx, &fy);
        if (x >= fx - T.gap / 2 && x < fx + T.fw + T.gap / 2 && y >= fy - T.gap / 2 && y < fy + T.fh + T.gap / 2)
            return f;
    }
    return -1;
}

/* ---- the textures ------------------------------------------------------------------------------------------------ */
static SDL_Texture *t_face[MF_FACES], *t_back, *t_plank, *t_q[MF_QUESTIONS], *t_glass, *t_device, *t_tick,
    *t_yes, *t_no, *t_mini;
static int made_fw = -1, made_plank = -1, made_q = -1, made_ad = -1, made_mini = -1;
static ga_buttons btn;

static void textures(void)
{
    int fw = (int)T.fw, fh = (int)T.fh;
    if (made_fw != fw) {
        made_fw = fw;
        uint8_t *o = malloc((size_t)fw * fh * 4);
        for (int f = 0; f < MF_FACES && o; f++) {
            paint_face(f, fw, fh, o);
            ui_free(&t_face[f]);
            t_face[f] = ui_tex(fw, fh, o);
        }
        if (o) {
            paint_frame(fw, fh, NULL, NULL, o);
            ui_free(&t_back);
            t_back = ui_tex(fw, fh, o);
        }
        free(o);
    }
    int key = T.bw * 4096 + T.bh;
    if (made_plank != key) {
        made_plank = key;
        int nx = (T.bw - 2 * T.fr) / T.sq, ny = (T.bh - 2 * T.fr) / T.sq;
        uint8_t *o = ui_buf(T.bw, T.bh);
        art_board_n(nx, ny, T.sq, T.fr, 0, o);
        ui_free(&t_plank);
        t_plank = ui_tex(T.bw, T.bh, o);
    }
    int qs = (int)(T.qd * 0.8f);
    if (made_q != qs * 4096 + (int)T.bd) {
        made_q = qs * 4096 + (int)T.bd;
        for (int q = 0; q < MF_QUESTIONS; q++) {
            ui_free(&t_q[q]);
            uint8_t *o = ui_buf(qs, qs);
            paint_question(q, qs, o);
            t_q[q] = ui_tex(qs, qs, o);
        }
        int is = (int)(T.bd * 0.62f);
        ui_free(&t_glass);
        t_glass = ga_icon(paint_glass, is, 1);
        ui_free(&t_device);
        t_device = ga_icon(paint_device, is, 1);
        ui_free(&t_tick);
        t_tick = ga_icon(ga_tick, is, 1);
        ga_buttons_make(&btn, (int)T.bd);
    }
    int ad = (int)T.ad;
    if (made_ad != ad) {                        /* the answer: a jade disc and a tick, a red one and a cross */
        made_ad = ad;
        static const uint8_t jade[3] = {62, 138, 98}, red[3] = {178, 52, 40};
        int ic = (int)(0.62f * ad);
        uint8_t *mark = malloc((size_t)ic * ic * 4);
        for (int k = 0; k < 2 && mark; k++) {
            uint8_t *o = ui_buf(ad, ad);
            art_disc(ad, k ? red : jade, 1, 0, o);
            if (k) art_icon(ICON_CLOSE, ic, mark);
            else ga_tick(ic, mark);
            art_blend(o, ad, ad, mark, ic, ic, (ad - ic) / 2, (ad - ic) / 2);
            SDL_Texture **t = k ? &t_no : &t_yes;
            ui_free(t);
            *t = ui_tex(ad, ad, o);
        }
        free(mark);
    }
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    memset(flip_on, 0, sizeof flip_on);
    pop_on = 0;
}

static int me(void) { return ui.viewer >= 0 ? ui.viewer : P->turn; }

static void act(int a)
{
    if (!mf_play(&G, a)) { ui_sound(SND_DENY); return; }
    int k = MA_KIND(a), x = MA_ARG(a);
    if (k == MA_FLIP) {
        flip_on[x] = 1;
        flip_t0[x] = ui_ms();
        ui_sound(P->down[P->turn] >> x & 1 ? SND_PLACE : SND_MOVE);
    } else if (k == MA_ASK) {
        pop_on = 1;
        pop_t0 = ui_ms();
        ui_sound(SND_MOVE);
        guessing = 0;
    } else if (k == MA_PICK) {
        ui_sound(SND_PLACE);
    } else if (k == MA_END) {
        guessing = 0;
        ui_handoff(P->turn);
    } else if (k == MA_GUESS) {
        guessing = 0;
    }
    ui_animate();
}

static void start(int v)
{
    (void)v;
    mf_new(&G);
    calm();
    guessing = 0;
}

static int undo(void)
{
    if (guessing) { guessing = 0; return 1; }
    if (!mf_undo(&G)) return 0;
    calm();
    return 1;
}

static int script(const char *s)
{
    int ok = mf_script(&G, s);
    calm();
    guessing = 0;
    ui_handoff(P->turn);
    return ok;
}

static int turn(void) { return P->turn; }

static int status(void) { return P->phase == MF_OVER ? gs_win(P->winner) : GS_PLAY; }

static int busy(void)
{
    Uint32 t = ui_ms();
    for (int f = 0; f < MF_FACES; f++)
        if (flip_on[f] && t - flip_t0[f] < FLIP_MS) return 1;
    return pop_on && t - pop_t0 < POP_MS;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void outline(float x, float y, float w, float h, float t, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    ui_fill(x - t, y - t, w + 2 * t, t, r, g, b, a);
    ui_fill(x - t, y + h, w + 2 * t, t, r, g, b, a);
    ui_fill(x - t, y, t, h, r, g, b, a);
    ui_fill(x + w, y, t, h, r, g, b, a);
}

/* a frame standing (or put down), folding over when it turns: t 0..1 of the turn */
static void draw_frame(int f, float x, float y, int down)
{
    float w = T.fw, h = T.fh, k = 1;
    int show_down = down;
    if (flip_on[f]) {
        float t = (ui_ms() - flip_t0[f]) / (float)FLIP_MS;
        if (t >= 1) flip_on[f] = 0;
        else {
            ui_animate();
            k = fabsf(1 - 2 * t);               /* it folds to its foot, and rises again turned */
            show_down = t < 0.5f ? !down : down;
        }
    }
    float hh = h * fmaxf(0.04f, k), yy = y + h - hh;
    float sh = fmaxf(1, 0.04f * w);
    ui_fill(x + sh, yy + sh, w, hh, 0, 0, 0, 60);
    ui_put(show_down ? t_back : t_face[f], x, yy, w, hh);
}

static void draw_secret(void)
{
    int s = P->secret[me()];
    float x = T.sx, y = T.sy;
    ui_fill(x + 2, y + 2, T.sw, T.sh, 0, 0, 0, 60);
    if (s >= 0) ui_put(t_face[s], x, y, T.sw, T.sh);
    else {
        SDL_SetTextureAlphaMod(t_back, 110);
        ui_put(t_back, x, y, T.sw, T.sh);
        SDL_SetTextureAlphaMod(t_back, 255);
    }
}

static void draw(void)
{
    textures();
    int m = me(), ph = P->phase, mine = P->turn == m && !ui_handing() && ph != MF_OVER;
    ui_put(t_plank, T.bx, T.by, T.bw, T.bh);
    float edge = fmaxf(2, 0.045f * T.fw);
    for (int f = 0; f < MF_FACES; f++) {
        float x, y;
        face_rect(f, &x, &y);
        int down = ph != MF_PICK && (P->down[m] >> f & 1);
        if (ph == MF_OVER && f == P->guess && P->turn == m) {   /* the guess: right (gold) or wrong (red) */
            if (P->winner == m) ui_mark_gold(x + T.fw / 2, y + T.fh / 2, T.fh * 1.5f);
            else ui_mark_glow(x + T.fw / 2, y + T.fh / 2, T.fh * 1.4f);
        }
        draw_frame(f, x, y, down);
        if (ph == MF_OVER && f == P->guess && P->turn == m) {
            if (P->winner == m) outline(x, y, T.fw, T.fh, edge * 1.4f, 250, 214, 90, 255);
            else outline(x, y, T.fw, T.fh, edge * 1.4f, 220, 50, 36, 255);
        }
        if (ph == MF_PICK && f == P->secret[m]) outline(x, y, T.fw, T.fh, edge * 1.4f, 250, 214, 90, 255);
        else if (mine && guessing && !down) outline(x, y, T.fw, T.fh, edge, 250, 214, 90, 150);
    }
    /* the questions: one a turn (the one asked gold, the others faded once it is asked) */
    int can_ask = mine && ph == MF_PLAY && P->asked < 0 && !guessing;
    for (int q = 0; q < MF_QUESTIONS; q++) {
        int how = GA_IVORY;
        if (ph == MF_PLAY && P->asked == q && P->turn == m) how |= GA_ON;
        else if (!can_ask) how |= GA_DIM;
        ga_button(&btn, t_q[q], T.qx[q], T.qy[q], T.qd, 0.8f, how);
    }
    draw_secret();
    if (ph == MF_PLAY && P->asked >= 0 && P->turn == m) {   /* the answer pops up */
        float t = pop_on ? (ui_ms() - pop_t0) / (float)POP_MS : 1;
        if (t >= 1) pop_on = 0;
        else ui_animate();
        float k = t >= 1 ? 1 : 0.4f + 0.6f * sinf(fminf(1, t) * 1.9f) / sinf(1.9f);
        ga_button(&btn, t_q[P->asked], T.kx, T.ay, T.qd * 0.86f, 0.8f, GA_IVORY);
        ui_put_c(P->answer ? t_yes : t_no, T.ax, T.ay, T.ad * k, T.ad * k);
    } else if (ph == MF_OVER) {                 /* the other's face, shown at last */
        int s = P->secret[!m];
        float w = T.sw, h = T.sh, x = T.ax - w / 2, y = T.ay - h / 2;
        ui_mark_gold(T.ax, T.ay, h * 1.6f);
        ui_fill(x + 2, y + 2, w, h, 0, 0, 0, 60);
        if (s >= 0) ui_put(t_face[s], x, y, w, h);
    }
    if (ph == MF_PICK) {
        ga_button(&btn, t_tick, T.ex, T.ey, T.bd, 0.62f, mine && P->secret[m] >= 0 ? GA_ON : GA_DIM);
    } else {
        int gh = mine && P->asked < 0 ? (guessing ? GA_ON : 0) : GA_DIM;
        ga_button(&btn, t_glass, T.gx, T.gy, T.bd, 0.62f, gh);
        ga_button(&btn, t_device, T.ex, T.ey, T.bd, 0.62f, mine && P->asked >= 0 ? GA_ON : GA_DIM);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (P->phase == MF_OVER) return;
    if (P->turn != me()) { ui_sound(SND_DENY); return; }
    if (ga_in(T.ex, T.ey, T.bd, x, y)) { act(MA_END); return; }
    if (P->phase == MF_PLAY && ga_in(T.gx, T.gy, T.bd, x, y)) {
        if (P->asked >= 0) { ui_sound(SND_DENY); return; }
        guessing = !guessing;
        ui_sound(SND_TAP);
        return;
    }
    for (int q = 0; q < MF_QUESTIONS; q++)
        if (ga_in(T.qx[q], T.qy[q], T.qd, x, y)) {
            if (guessing || P->phase != MF_PLAY) ui_sound(SND_DENY);
            else act(MA_ASK | q);
            return;
        }
    int f = face_at(x, y);
    if (f < 0) return;
    if (P->phase == MF_PICK) act(MA_PICK | f);
    else act((guessing ? MA_GUESS : MA_FLIP) | f);
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the faces still standing on the player's board */
{
    int s = (int)(0.6f * ui.unit);
    if (made_mini != s) {
        made_mini = s;
        uint8_t *o = ui_buf(s, s);
        paint_question(MQ_HAT, s, o);           /* a plain head */
        ui_free(&t_mini);
        t_mini = ui_tex(s, s, o);
    }
    ui_bar_count(b, t_mini, mf_up(P, player));
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: a plank and six faces, one put down */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    int fw = (int)(size * 0.26f), fh = (int)(fw * FA), gap = (int)(size * 0.025f);
    int iw = 3 * fw + 2 * gap, ih = 2 * fh + gap, fr = (int)fmaxf(2, size * 0.045f), sq = (int)fmaxf(2, size / 14.0f);
    int nx = (iw + gap * 2 + sq - 1) / sq, ny = (ih + gap * 2 + sq - 1) / sq;
    int bw = nx * sq + 2 * fr, bh = ny * sq + 2 * fr;
    uint8_t *b = malloc((size_t)bw * bh * 4), *f = malloc((size_t)fw * fh * 4);
    if (b && f) {
        art_board_n(nx, ny, sq, fr, 0, b);
        art_blend(rgba, size, size, b, bw, bh, (size - bw) / 2, (size - bh) / 2);
        static const int pick[6] = {8, 3, 18, 14, 2, 22};
        int x0 = (size - iw) / 2, y0 = (size - ih) / 2;
        for (int i = 0; i < 6; i++) {
            if (i == 4) paint_frame(fw, fh, NULL, NULL, f);
            else paint_face(pick[i], fw, fh, f);
            art_blend(rgba, size, size, f, fw, fh, x0 + (i % 3) * (fw + gap), y0 + (i / 3) * (fh + gap));
        }
    }
    free(b);
    free(f);
}

static const game mysteryface_game = {
    .id = "mysteryface", .order = 360, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .view = VIEW_HIDDEN,
};
REGISTER_GAME(mysteryface_game)
