/*
 * The family deck of Happy Families (jeu des 7 familles), drawn by code in the house style: 42 cards, 7 families of
 * animals (bears, rabbits, cats, mice, owls, lions, frogs), each with its own colour, 6 members each (grandfather,
 * grandmother, father, mother, son, daughter). No text: the family is the colour and the animal, the member is told
 * by what the animal wears (glasses and a white moustache, glasses and a grey bun, a bow tie, a pearl necklace, a
 * cap, a hair bow; the children smaller) and by a figure in the corner and a row of the six figures at the foot of
 * the card (this one's in the family's colour). The backs are the house's.
 * A card is a byte: CARD(family, member), cards.h's layout (the family in the suit's bits). The numbers serve the
 * rules (no SDL); the faces are painted with the art kit (art.h) and drawn by the card kit as a deck of its own:
 * fam_deck() is its number for cards_draw, cards_pile, slides (cards_deck_add). Static inline: each game that uses it
 * (Happy Families; Pairs, its emblems) takes its own copy.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef FAMILIES_DECK_H
#define FAMILIES_DECK_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "cards.h"

#define FAM_FAMILIES 7
#define FAM_MEMBERS 6
#define FAM_CARD(f, m) CARD(f, m)
#define FAM_FAMILY(c) CARD_SUIT(c)
#define FAM_MEMBER(c) CARD_RANK(c)
#define FAM_PI 3.14159265f

enum { FAM_BEARS, FAM_RABBITS, FAM_CATS, FAM_MICE, FAM_OWLS, FAM_LIONS, FAM_FROGS };
enum { FAM_GRANDFATHER, FAM_GRANDMOTHER, FAM_FATHER, FAM_MOTHER, FAM_SON, FAM_DAUGHTER };

/* ---- the looks --------------------------------------------------------------------------------------------------- */
/* each family: its colour (the card's panel), its animal's fur, the lighter muzzle, an accent (nose, inner ears,
 * beak, cheeks); its head's measures (head-local unit square): the head's centre y and half height, the eyes' half
 * gap and height, the nose's height */
typedef struct {
    uint8_t panel[3], fur[3], light[3], accent[3];
    float hy, ry, ex, ey, ny;
} fam_looks;

static inline const fam_looks *fam_look(int fam)
{
    /* bears, rabbits, cats, mice, owls, lions, frogs */
    static const fam_looks L[FAM_FAMILIES] = {
        {{66, 128, 200}, {140, 92, 58}, {222, 186, 140}, {58, 36, 28}, 0.56f, 0.31f, 0.13f, 0.5f, 0.62f},
        {{82, 158, 70}, {238, 234, 228}, {252, 250, 246}, {236, 146, 160}, 0.6f, 0.27f, 0.1f, 0.55f, 0.66f},
        {{130, 84, 172}, {236, 146, 56}, {252, 230, 196}, {236, 140, 150}, 0.58f, 0.29f, 0.12f, 0.54f, 0.66f},
        {{232, 116, 44}, {150, 152, 164}, {236, 206, 206}, {232, 140, 150}, 0.6f, 0.26f, 0.1f, 0.56f, 0.69f},
        {{232, 192, 56}, {128, 92, 66}, {240, 226, 198}, {236, 150, 36}, 0.56f, 0.34f, 0.13f, 0.5f, 0.62f},
        {{196, 56, 50}, {232, 180, 76}, {252, 232, 184}, {176, 96, 40}, 0.57f, 0.27f, 0.1f, 0.53f, 0.64f},
        {{228, 124, 168}, {98, 178, 72}, {206, 232, 140}, {240, 150, 170}, 0.62f, 0.25f, 0.18f, 0.4f, 0.58f},
    };
    return &L[fam];
}

enum { FAMC_IVORY, FAMC_RIM, FAMC_INK, FAMC_WHITE, FAMC_GREY, FAMC_FAINT };
static inline const uint8_t *fam_col(int k)
{
    static const uint8_t c[6][3] = {{250, 246, 236}, {176, 164, 140}, {44, 32, 28}, {250, 250, 246}, {204, 204, 210},
                                    {214, 204, 188}};
    return c[k];
}

/* ---- painting ---------------------------------------------------------------------------------------------------- */
/* ---- painting ---------------------------------------------------------------------------------------------------- */
static inline art_style fam_style(const uint8_t fill[3], const uint8_t line[3], float width, float light)
{
    art_style st;
    for (int i = 0; i < 3; i++) {
        st.fill[i] = fill[i];
        st.outer[i] = st.inner[i] = line ? line[i] : fill[i];
        st.mark[i] = fam_col(FAMC_INK)[i];
    }
    st.light = light;
    st.line = line ? width : 0;
    return st;
}

/* a figure's unit square drawn s pixels wide with its top left at (x, y) of a w x h picture */
static inline void fam_put(uint8_t *rgba, int w, int h, art_fig *f, const art_style *st, float x, float y, float s)
{
    int n = (int)ceilf(s);
    if (n >= 2) {
        uint8_t *b = malloc((size_t)n * n * 4);
        if (b) {
            art_render(f, st, n, n, s, 0, 0, b);
            art_blend(rgba, w, h, b, n, n, (int)floorf(x + 0.5f), (int)floorf(y + 0.5f));
            free(b);
        }
    }
    art_fig_free(f);
}

static inline void fam_tri(art_fig *f, int role, float x0, float y0, float x1, float y1, float x2, float y2)
{
    const float p[] = {x0, y0, x1, y1, x2, y2};
    art_poly(f, role, p, 3);
}

static inline void fam_circle(art_fig *f, int role, float cx, float cy, float r) { art_ellipse(f, role, cx, cy, r, r); }

/* an animal's head (head-local unit square, s pixels at (x, y)): the fur, the light parts and the face */
static inline void fam_head(uint8_t *rgba, int w, int h, int fam, float x, float y, float s)
{
    const fam_looks *L = fam_look(fam);
    const uint8_t *fur = L->fur, *li = L->light, *ac = L->accent;
    uint8_t dark[3] = {(uint8_t)(fur[0] * 0.45f), (uint8_t)(fur[1] * 0.4f), (uint8_t)(fur[2] * 0.38f)};
    float lw = fmaxf(0.022f, 1.2f / s), hy = L->hy, ry = L->ry, ex = L->ex, ey = L->ey;
    art_style sf = fam_style(fur, dark, lw, 0.28f), sl = fam_style(li, NULL, 0, 0.1f);
    art_style sa = fam_style(ac, NULL, 0, 0.15f);
    art_fig *f;
    if (fam == FAM_LIONS) {                     /* the mane behind */
        float p[64];
        for (int i = 0; i < 32; i++) {
            float a = i * FAM_PI / 16, r = i & 1 ? 0.37f : 0.45f;
            p[2 * i] = 0.5f + r * cosf(a);
            p[2 * i + 1] = 0.56f + r * sinf(a) * 0.98f;
        }
        f = art_fig_new();
        art_poly(f, ART_PART, p, 32);
        art_style sm = fam_style(ac, dark, lw, 0.3f);
        fam_put(rgba, w, h, f, &sm, x, y, s);
    }
    f = art_fig_new();                          /* ears, the head, the eyes */
    switch (fam) {
    case FAM_BEARS: fam_circle(f, ART_PART, 0.25f, 0.27f, 0.12f), fam_circle(f, ART_PART, 0.75f, 0.27f, 0.12f); break;
    case FAM_RABBITS:
        art_ellipse(f, ART_PART, 0.38f, 0.22f, 0.08f, 0.2f);
        art_ellipse(f, ART_PART, 0.62f, 0.22f, 0.08f, 0.2f);
        break;
    case FAM_CATS:
        fam_tri(f, ART_PART, 0.2f, 0.46f, 0.23f, 0.1f, 0.46f, 0.32f);
        fam_tri(f, ART_PART, 0.8f, 0.46f, 0.77f, 0.1f, 0.54f, 0.32f);
        break;
    case FAM_MICE: fam_circle(f, ART_PART, 0.21f, 0.34f, 0.17f), fam_circle(f, ART_PART, 0.79f, 0.34f, 0.17f); break;
    case FAM_OWLS:
        fam_tri(f, ART_PART, 0.18f, 0.36f, 0.2f, 0.08f, 0.4f, 0.26f);
        fam_tri(f, ART_PART, 0.82f, 0.36f, 0.8f, 0.08f, 0.6f, 0.26f);
        break;
    case FAM_LIONS: fam_circle(f, ART_PART, 0.3f, 0.33f, 0.07f), fam_circle(f, ART_PART, 0.7f, 0.33f, 0.07f); break;
    case FAM_FROGS: fam_circle(f, ART_PART, 0.5f - ex, ey, 0.12f), fam_circle(f, ART_PART, 0.5f + ex, ey, 0.12f); break;
    }
    art_ellipse(f, ART_PART, 0.5f, hy, fam == FAM_FROGS ? 0.38f : fam == FAM_MICE ? 0.27f : 0.32f, ry);
    if (fam == FAM_FROGS) fam_circle(f, ART_PART, 0.5f - ex, ey, 0.12f), fam_circle(f, ART_PART, 0.5f + ex, ey, 0.12f);
    fam_put(rgba, w, h, f, &sf, x, y, s);
    f = art_fig_new();                          /* the light parts: muzzle, inner ears, face discs, eye whites */
    switch (fam) {
    case FAM_BEARS:
        fam_circle(f, ART_PART, 0.25f, 0.27f, 0.06f), fam_circle(f, ART_PART, 0.75f, 0.27f, 0.06f);
        art_ellipse(f, ART_PART, 0.5f, 0.68f, 0.15f, 0.11f);
        break;
    case FAM_RABBITS:
        art_ellipse(f, ART_PART, 0.45f, 0.71f, 0.07f, 0.06f);
        art_ellipse(f, ART_PART, 0.55f, 0.71f, 0.07f, 0.06f);
        art_rect(f, ART_PART, 0.47f, 0.75f, 0.53f, 0.81f);
        break;
    case FAM_CATS: art_ellipse(f, ART_PART, 0.5f, 0.71f, 0.15f, 0.1f); break;
    case FAM_MICE: art_ellipse(f, ART_PART, 0.5f, 0.73f, 0.12f, 0.08f); break;
    case FAM_OWLS: fam_circle(f, ART_PART, 0.5f - ex, ey, 0.15f), fam_circle(f, ART_PART, 0.5f + ex, ey, 0.15f); break;
    case FAM_LIONS: art_ellipse(f, ART_PART, 0.5f, 0.69f, 0.15f, 0.1f); break;
    case FAM_FROGS:
        fam_circle(f, ART_PART, 0.5f - ex, ey, 0.075f), fam_circle(f, ART_PART, 0.5f + ex, ey, 0.075f);
        art_arc(f, ART_MARK, 0.5f, 0.56f, 0.2f, 0.225f, 205, 335);   /* the smile */
        break;
    }
    /* the eyes (owls: big), the nose, the mouth */
    float er = fam == FAM_OWLS ? 0.07f : fam == FAM_FROGS ? 0.04f : 0.035f;
    if (fam == FAM_CATS) {
        art_ellipse(f, ART_MARK, 0.5f - ex, ey, 0.035f, 0.055f);
        art_ellipse(f, ART_MARK, 0.5f + ex, ey, 0.035f, 0.055f);
    } else {
        fam_circle(f, ART_MARK, 0.5f - ex, ey, er);
        fam_circle(f, ART_MARK, 0.5f + ex, ey, er);
    }
    float ny = L->ny;
    if (fam == FAM_BEARS || fam == FAM_LIONS) art_ellipse(f, ART_MARK, 0.5f, ny, 0.06f, 0.04f);
    if (fam == FAM_CATS || fam == FAM_MICE || fam == FAM_LIONS) {   /* whiskers */
        for (int k = -1; k <= 1; k += 2)
            for (int j = 0; j < 2; j++) {
                float y0 = ny + 0.05f + 0.035f * j, x0 = 0.5f + k * 0.12f, x1 = 0.5f + k * 0.3f;
                float y1 = y0 + (j - 0.5f) * 0.05f;
                const float p[] = {x0, y0 - 0.007f, x1, y1 - 0.005f, x1, y1 + 0.005f, x0, y0 + 0.007f};
                art_poly(f, ART_MARK, p, 4);
            }
    }
    fam_put(rgba, w, h, f, &sl, x, y, s);
    f = art_fig_new();                          /* the accent: nose, inner ears, beak, cheeks */
    switch (fam) {
    case FAM_RABBITS:
        art_ellipse(f, ART_PART, 0.38f, 0.24f, 0.04f, 0.14f);
        art_ellipse(f, ART_PART, 0.62f, 0.24f, 0.04f, 0.14f);
        fam_tri(f, ART_PART, 0.46f, ny - 0.02f, 0.54f, ny - 0.02f, 0.5f, ny + 0.03f);
        break;
    case FAM_CATS:
        fam_tri(f, ART_PART, 0.25f, 0.38f, 0.26f, 0.18f, 0.39f, 0.31f);
        fam_tri(f, ART_PART, 0.75f, 0.38f, 0.74f, 0.18f, 0.61f, 0.31f);
        fam_tri(f, ART_PART, 0.45f, ny - 0.03f, 0.55f, ny - 0.03f, 0.5f, ny + 0.02f);
        break;
    case FAM_MICE:
        fam_circle(f, ART_PART, 0.21f, 0.34f, 0.1f), fam_circle(f, ART_PART, 0.79f, 0.34f, 0.1f);
        fam_circle(f, ART_PART, 0.5f, ny, 0.04f);
        break;
    case FAM_OWLS: fam_tri(f, ART_PART, 0.45f, ny - 0.02f, 0.55f, ny - 0.02f, 0.5f, ny + 0.09f); break;
    case FAM_FROGS: art_ellipse(f, ART_PART, 0.27f, 0.7f, 0.06f, 0.035f), art_ellipse(f, ART_PART, 0.73f, 0.7f, 0.06f,
                                                                                         0.035f); break;
    default: break;
    }
    fam_put(rgba, w, h, f, &sa, x, y, s);
}

/* what the member wears (head-local unit square, over the head) */
static inline void fam_wear(uint8_t *rgba, int w, int h, int fam, int member, float x, float y, float s)
{
    const fam_looks *L = fam_look(fam);
    float lw = fmaxf(0.02f, 1.2f / s), hy = L->hy, ry = L->ry, ex = L->ex, ey = L->ey;
    float top = fam == FAM_FROGS ? ey - 0.13f : hy - ry, chin = hy + ry, ny = L->ny;
    static const uint8_t frame[3] = {54, 40, 34}, navy[3] = {40, 52, 104}, red[3] = {206, 52, 48},
                         pink[3] = {236, 76, 128}, edge[3] = {120, 112, 110};
    art_fig *f = art_fig_new();
    art_style st;
    switch (member) {
    case FAM_GRANDFATHER:
    case FAM_GRANDMOTHER:                       /* round glasses */
        art_arc(f, ART_PART, 0.5f - ex, ey, 0.078f, 0.098f, 0, 360);
        art_arc(f, ART_PART, 0.5f + ex, ey, 0.078f, 0.098f, 0, 360);
        art_rect(f, ART_PART, 0.5f - ex + 0.09f, ey - 0.012f, 0.5f + ex - 0.09f, ey + 0.008f);
        st = fam_style(frame, NULL, 0, 0);
        fam_put(rgba, w, h, f, &st, x, y, s);
        f = art_fig_new();
        if (member == FAM_GRANDFATHER) {        /* a white moustache */
            art_ellipse(f, ART_PART, 0.43f, ny + 0.055f, 0.085f, 0.04f);
            art_ellipse(f, ART_PART, 0.57f, ny + 0.055f, 0.085f, 0.04f);
            st = fam_style(fam_col(FAMC_WHITE), edge, lw, 0.1f);
        } else {                                /* a grey bun */
            fam_circle(f, ART_PART, 0.5f, top - 0.025f, 0.075f);
            art_ellipse(f, ART_PART, 0.5f, top + 0.045f, 0.15f, 0.045f);
            st = fam_style(fam_col(FAMC_GREY), edge, lw, 0.3f);
        }
        break;
    case FAM_FATHER:                            /* a bow tie */
        fam_tri(f, ART_PART, 0.5f, chin + 0.035f, 0.34f, chin - 0.025f, 0.34f, chin + 0.095f);
        fam_tri(f, ART_PART, 0.5f, chin + 0.035f, 0.66f, chin - 0.025f, 0.66f, chin + 0.095f);
        fam_circle(f, ART_PART, 0.5f, chin + 0.035f, 0.035f);
        st = fam_style(navy, NULL, 0, 0.2f);
        break;
    case FAM_MOTHER:                            /* a pearl necklace */
        for (int i = 0; i < 9; i++) {
            float a = (200 + 140.0f * i / 8) * FAM_PI / 180;
            fam_circle(f, ART_PART, 0.5f + 0.24f * cosf(a), chin - 0.13f - 0.17f * sinf(a), 0.028f);
        }
        st = fam_style(fam_col(FAMC_WHITE), edge, lw * 0.8f, 0.3f);
        break;
    case FAM_SON: {                             /* a cap: its crown and its peak */
        float p[2 * 14];
        for (int i = 0; i <= 12; i++) {
            float a = FAM_PI * i / 12;
            p[2 * i] = 0.5f + 0.22f * cosf(a);
            p[2 * i + 1] = top + 0.06f - 0.15f * sinf(a);
        }
        p[26] = 0.28f, p[27] = top + 0.08f;
        art_poly(f, ART_PART, p, 14);
        art_ellipse(f, ART_PART, 0.68f, top + 0.065f, 0.17f, 0.04f);
        st = fam_style(red, frame, lw, 0.25f);
        break;
    }
    case FAM_DAUGHTER:                          /* a bow in the hair */
        fam_tri(f, ART_PART, 0.66f, top + 0.02f, 0.5f, top - 0.08f, 0.52f, top + 0.12f);
        fam_tri(f, ART_PART, 0.66f, top + 0.02f, 0.82f, top - 0.08f, 0.8f, top + 0.12f);
        fam_circle(f, ART_PART, 0.66f, top + 0.02f, 0.04f);
        st = fam_style(pink, frame, lw, 0.2f);
        break;
    }
    fam_put(rgba, w, h, f, &st, x, y, s);
}

/* a member's figure (a person standing, unit square): grandparents with a cane, mothers and daughters in a dress,
 * the children small */
static inline void fam_figure(art_fig *f, int member)
{
    int old = member <= FAM_GRANDMOTHER, kid = member >= FAM_SON, girl = member & 1;
    float cx = old ? 0.42f : 0.5f, hr = kid ? 0.12f : 0.13f, top = kid ? 0.3f : old ? 0.08f : 0.02f;
    float hcy = top + hr, body = hcy + hr + 0.01f, foot = 0.98f, hem = kid ? 0.84f : 0.76f;
    float half = kid ? 0.17f : 0.2f, leg = kid ? 0.07f : 0.08f;
    fam_circle(f, ART_PART, cx, hcy, hr);
    if (member == FAM_GRANDMOTHER) fam_circle(f, ART_PART, cx, hcy - hr * 1.1f, hr * 0.55f);
    if (member == FAM_DAUGHTER) {               /* pigtails */
        fam_circle(f, ART_PART, cx - hr * 1.15f, hcy + 0.02f, hr * 0.45f);
        fam_circle(f, ART_PART, cx + hr * 1.15f, hcy + 0.02f, hr * 0.45f);
    }
    if (girl) {                                 /* a dress, the legs under it */
        const float d[] = {cx - 0.45f * half, body, cx + 0.45f * half, body, cx + 1.4f * half, hem, cx - 1.4f * half,
                           hem};
        art_poly(f, ART_PART, d, 4);
        art_rect(f, ART_PART, cx - 0.1f - leg / 2, hem - 0.02f, cx - 0.1f + leg / 2, foot);
        art_rect(f, ART_PART, cx + 0.1f - leg / 2, hem - 0.02f, cx + 0.1f + leg / 2, foot);
    } else {                                    /* a jacket, the legs */
        float hip = kid ? 0.68f : 0.6f;
        art_rect(f, ART_PART, cx - half, body, cx + half, hip);
        art_rect(f, ART_PART, cx - half + 0.01f, hip - 0.02f, cx - 0.02f, foot);
        art_rect(f, ART_PART, cx + 0.02f, hip - 0.02f, cx + half - 0.01f, foot);
    }
    if (old) {                                  /* the cane */
        art_rect(f, ART_PART, 0.74f, 0.46f, 0.8f, foot);
        art_arc(f, ART_PART, 0.69f, 0.46f, 0.045f, 0.105f, 0, 180);
    }
}

static inline void fam_put_figure(uint8_t *rgba, int w, int h, int member, const uint8_t rgb[3], float x, float y,
                                  float s)
{
    art_fig *f = art_fig_new();
    fam_figure(f, member);
    art_style st = fam_style(rgb, NULL, 0, 0);
    fam_put(rgba, w, h, f, &st, x, y, s);
}

static inline void fam_rect(uint8_t *rgba, int w, int h, float x, float y, float rw, float rh, float rad,
                         const uint8_t rgb[3])
{
    int iw = (int)(rw + 0.5f), ih = (int)(rh + 0.5f);
    if (iw < 1 || ih < 1) return;
    const uint8_t c[4] = {rgb[0], rgb[1], rgb[2], 255};
    uint8_t *b = malloc((size_t)iw * ih * 4);
    if (!b) return;
    art_roundrect(iw, ih, rad, c, b);
    art_blend(rgba, w, h, b, iw, ih, (int)(x + 0.5f), (int)(y + 0.5f));
    free(b);
}

static inline void fam_paint(int card, int w, int h, uint8_t *rgba)
{
    int fam = FAM_FAMILY(card) % FAM_FAMILIES, member = FAM_MEMBER(card) % FAM_MEMBERS, small = w < 52;
    const fam_looks *L = fam_look(fam);
    float W = (float)w, H = (float)h, b = fmaxf(1, 0.014f * W);
    memset(rgba, 0, (size_t)w * h * 4);
    const uint8_t *r = fam_col(FAMC_RIM), rim[4] = {r[0], r[1], r[2], 255};
    art_roundrect(w, h, 0.075f * W, rim, rgba);
    fam_rect(rgba, w, h, b, b, W - 2 * b, H - 2 * b, 0.075f * W - b, fam_col(FAMC_IVORY));
    float m = 0.06f * W, ph = small ? H - 2 * m : H - m - 0.3f * W;
    fam_rect(rgba, w, h, m, m, W - 2 * m, ph, 0.05f * W, L->panel);
    int kid = member >= FAM_SON;
    float s = fminf(W - 2 * m, ph) * (kid ? 0.8f : 0.98f), hx = W / 2 - s / 2;
    float hy = m + ph / 2 - s / 2 + (kid ? 0.09f * s : 0) + (small ? -0.08f * s : 0);
    fam_head(rgba, w, h, fam, hx, hy, s);
    fam_wear(rgba, w, h, fam, member, hx, hy, s);
    uint8_t deep[3] = {(uint8_t)(L->panel[0] * 0.62f), (uint8_t)(L->panel[1] * 0.62f),
                       (uint8_t)(L->panel[2] * 0.62f)};
    if (small) {                                /* the member at the foot, on a light disc */
        float d = 0.42f * W;
        uint8_t *disc = malloc((size_t)((int)d + 1) * ((int)d + 1) * 4);
        if (disc) {
            art_disc((int)d, fam_col(FAMC_IVORY), 1, 0, disc);
            art_blend(rgba, w, h, disc, (int)d, (int)d, (int)(W / 2 - d / 2), (int)(H - m - d));
            free(disc);
        }
        fam_put_figure(rgba, w, h, member, deep, W / 2 - 0.36f * d, H - m - 0.92f * d, 0.72f * d * 1.0f);
        return;
    }
    float cell = (W - 2 * m) / FAM_MEMBERS, fs = fminf(1.3f * cell, 0.24f * W), fy = H - m - fs;
    for (int k = 0; k < FAM_MEMBERS; k++)       /* the six members, this one in the family's colour */
        fam_put_figure(rgba, w, h, k, k == member ? deep : fam_col(FAMC_FAINT), m + (k + 0.5f) * cell - fs / 2, fy, fs);
    float d = 0.28f * W;                        /* the corner: this member on a light disc */
    uint8_t *disc = malloc((size_t)((int)d + 1) * ((int)d + 1) * 4);
    if (disc) {
        art_disc((int)d, fam_col(FAMC_IVORY), 1, 0, disc);
        art_blend(rgba, w, h, disc, (int)d, (int)d, (int)(0.045f * W), (int)(0.045f * W));
        free(disc);
    }
    fam_put_figure(rgba, w, h, member, deep, 0.045f * W + 0.14f * d, 0.045f * W + 0.12f * d, 0.74f * d);
}

static inline void fam_emblem(int family, int size, uint8_t *rgba)
{
    int fam = family % FAM_FAMILIES;
    const fam_looks *L = fam_look(fam);
    memset(rgba, 0, (size_t)size * size * 4);
    art_disc(size, fam_col(FAMC_RIM), 1, 0, rgba);
    float b = fmaxf(1, 0.05f * size);
    uint8_t *d = malloc((size_t)size * size * 4);
    if (!d) return;
    art_disc((int)(size - 2 * b), L->panel, 1, 0, d);
    art_blend(rgba, size, size, d, (int)(size - 2 * b), (int)(size - 2 * b), (int)b, (int)b);
    free(d);
    fam_head(rgba, size, size, fam, 0.1f * size, 0.06f * size, 0.8f * size);
}

/* the deck for the card kit: its number (registered once) */
static inline int fam_deck(void)
{
    static const cards_deck_def def = {1.4f, fam_paint, NULL};
    static int deck = -1;
    if (deck < 0) deck = cards_deck_add(&def);
    return deck;
}

#endif
