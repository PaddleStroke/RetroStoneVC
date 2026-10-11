/*
 * The family deck's pictures (families_deck.h): faces painted by code with the art kit's figures (art.h), a texture
 * cache, cards drawn, flipped and slid like the card kit's (the house back from the kit, the face laid over it).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/families_deck.h"
#include "shell.h"

#define PI_F 3.14159265f

/* each family: its colour (the card's panel), its animal's fur, the lighter muzzle, an accent (nose, inner ears,
 * beak, cheeks); its head's measures (head-local unit square): the head's centre y and half height, the eyes' half
 * gap and height, the nose's height */
static const struct {
    uint8_t panel[3], fur[3], light[3], accent[3];
    float hy, ry, ex, ey, ny;
} FAM[FAM_FAMILIES] = {
    {{66, 128, 200}, {140, 92, 58}, {222, 186, 140}, {58, 36, 28}, 0.56f, 0.31f, 0.13f, 0.5f, 0.62f},     /* bears */
    {{82, 158, 70}, {238, 234, 228}, {252, 250, 246}, {236, 146, 160}, 0.6f, 0.27f, 0.1f, 0.55f, 0.66f},  /* rabbits */
    {{130, 84, 172}, {236, 146, 56}, {252, 230, 196}, {236, 140, 150}, 0.58f, 0.29f, 0.12f, 0.54f, 0.66f}, /* cats */
    {{232, 116, 44}, {150, 152, 164}, {236, 206, 206}, {232, 140, 150}, 0.6f, 0.26f, 0.1f, 0.56f, 0.69f}, /* mice */
    {{232, 192, 56}, {128, 92, 66}, {240, 226, 198}, {236, 150, 36}, 0.56f, 0.34f, 0.13f, 0.5f, 0.62f},   /* owls */
    {{196, 56, 50}, {232, 180, 76}, {252, 232, 184}, {176, 96, 40}, 0.57f, 0.27f, 0.1f, 0.53f, 0.64f},    /* lions */
    {{228, 124, 168}, {98, 178, 72}, {206, 232, 140}, {240, 150, 170}, 0.62f, 0.25f, 0.18f, 0.4f, 0.58f}, /* frogs */
};
static const uint8_t IVORY[3] = {250, 246, 236}, RIM[3] = {176, 164, 140}, INK[3] = {44, 32, 28},
                     WHITE[3] = {250, 250, 246}, GREY[3] = {204, 204, 210}, FAINT[3] = {214, 204, 188};

/* ---- painting ---------------------------------------------------------------------------------------------------- */
static art_style style(const uint8_t fill[3], const uint8_t line[3], float width, float light)
{
    art_style st;
    for (int i = 0; i < 3; i++) {
        st.fill[i] = fill[i];
        st.outer[i] = st.inner[i] = line ? line[i] : fill[i];
        st.mark[i] = INK[i];
    }
    st.light = light;
    st.line = line ? width : 0;
    return st;
}

/* a figure's unit square drawn s pixels wide with its top left at (x, y) of a w x h picture */
static void put(uint8_t *rgba, int w, int h, art_fig *f, const art_style *st, float x, float y, float s)
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

static void tri(art_fig *f, int role, float x0, float y0, float x1, float y1, float x2, float y2)
{
    const float p[] = {x0, y0, x1, y1, x2, y2};
    art_poly(f, role, p, 3);
}

static void circle(art_fig *f, int role, float cx, float cy, float r) { art_ellipse(f, role, cx, cy, r, r); }

/* an animal's head (head-local unit square, s pixels at (x, y)): the fur, the light parts and the face */
static void head(uint8_t *rgba, int w, int h, int fam, float x, float y, float s)
{
    const uint8_t *fur = FAM[fam].fur, *li = FAM[fam].light, *ac = FAM[fam].accent;
    uint8_t dark[3] = {(uint8_t)(fur[0] * 0.45f), (uint8_t)(fur[1] * 0.4f), (uint8_t)(fur[2] * 0.38f)};
    float lw = fmaxf(0.022f, 1.2f / s), hy = FAM[fam].hy, ry = FAM[fam].ry, ex = FAM[fam].ex, ey = FAM[fam].ey;
    art_style sf = style(fur, dark, lw, 0.28f), sl = style(li, NULL, 0, 0.1f), sa = style(ac, NULL, 0, 0.15f);
    art_fig *f;
    if (fam == FAM_LIONS) {                     /* the mane behind */
        float p[64];
        for (int i = 0; i < 32; i++) {
            float a = i * PI_F / 16, r = i & 1 ? 0.37f : 0.45f;
            p[2 * i] = 0.5f + r * cosf(a);
            p[2 * i + 1] = 0.56f + r * sinf(a) * 0.98f;
        }
        f = art_fig_new();
        art_poly(f, ART_PART, p, 32);
        art_style sm = style(ac, dark, lw, 0.3f);
        put(rgba, w, h, f, &sm, x, y, s);
    }
    f = art_fig_new();                          /* ears, the head, the eyes */
    switch (fam) {
    case FAM_BEARS: circle(f, ART_PART, 0.25f, 0.27f, 0.12f), circle(f, ART_PART, 0.75f, 0.27f, 0.12f); break;
    case FAM_RABBITS:
        art_ellipse(f, ART_PART, 0.38f, 0.22f, 0.08f, 0.2f);
        art_ellipse(f, ART_PART, 0.62f, 0.22f, 0.08f, 0.2f);
        break;
    case FAM_CATS:
        tri(f, ART_PART, 0.2f, 0.46f, 0.23f, 0.1f, 0.46f, 0.32f);
        tri(f, ART_PART, 0.8f, 0.46f, 0.77f, 0.1f, 0.54f, 0.32f);
        break;
    case FAM_MICE: circle(f, ART_PART, 0.21f, 0.34f, 0.17f), circle(f, ART_PART, 0.79f, 0.34f, 0.17f); break;
    case FAM_OWLS:
        tri(f, ART_PART, 0.18f, 0.36f, 0.2f, 0.08f, 0.4f, 0.26f);
        tri(f, ART_PART, 0.82f, 0.36f, 0.8f, 0.08f, 0.6f, 0.26f);
        break;
    case FAM_LIONS: circle(f, ART_PART, 0.3f, 0.33f, 0.07f), circle(f, ART_PART, 0.7f, 0.33f, 0.07f); break;
    case FAM_FROGS: circle(f, ART_PART, 0.5f - ex, ey, 0.12f), circle(f, ART_PART, 0.5f + ex, ey, 0.12f); break;
    }
    art_ellipse(f, ART_PART, 0.5f, hy, fam == FAM_FROGS ? 0.38f : fam == FAM_MICE ? 0.27f : 0.32f, ry);
    if (fam == FAM_FROGS) circle(f, ART_PART, 0.5f - ex, ey, 0.12f), circle(f, ART_PART, 0.5f + ex, ey, 0.12f);
    put(rgba, w, h, f, &sf, x, y, s);
    f = art_fig_new();                          /* the light parts: muzzle, inner ears, face discs, eye whites */
    switch (fam) {
    case FAM_BEARS:
        circle(f, ART_PART, 0.25f, 0.27f, 0.06f), circle(f, ART_PART, 0.75f, 0.27f, 0.06f);
        art_ellipse(f, ART_PART, 0.5f, 0.68f, 0.15f, 0.11f);
        break;
    case FAM_RABBITS:
        art_ellipse(f, ART_PART, 0.45f, 0.71f, 0.07f, 0.06f);
        art_ellipse(f, ART_PART, 0.55f, 0.71f, 0.07f, 0.06f);
        art_rect(f, ART_PART, 0.47f, 0.75f, 0.53f, 0.81f);
        break;
    case FAM_CATS: art_ellipse(f, ART_PART, 0.5f, 0.71f, 0.15f, 0.1f); break;
    case FAM_MICE: art_ellipse(f, ART_PART, 0.5f, 0.73f, 0.12f, 0.08f); break;
    case FAM_OWLS: circle(f, ART_PART, 0.5f - ex, ey, 0.15f), circle(f, ART_PART, 0.5f + ex, ey, 0.15f); break;
    case FAM_LIONS: art_ellipse(f, ART_PART, 0.5f, 0.69f, 0.15f, 0.1f); break;
    case FAM_FROGS:
        circle(f, ART_PART, 0.5f - ex, ey, 0.075f), circle(f, ART_PART, 0.5f + ex, ey, 0.075f);
        art_arc(f, ART_MARK, 0.5f, 0.56f, 0.2f, 0.225f, 205, 335);   /* the smile */
        break;
    }
    /* the eyes (owls: big), the nose, the mouth */
    float er = fam == FAM_OWLS ? 0.07f : fam == FAM_FROGS ? 0.04f : 0.035f;
    if (fam == FAM_CATS) {
        art_ellipse(f, ART_MARK, 0.5f - ex, ey, 0.035f, 0.055f);
        art_ellipse(f, ART_MARK, 0.5f + ex, ey, 0.035f, 0.055f);
    } else {
        circle(f, ART_MARK, 0.5f - ex, ey, er);
        circle(f, ART_MARK, 0.5f + ex, ey, er);
    }
    float ny = FAM[fam].ny;
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
    put(rgba, w, h, f, &sl, x, y, s);
    f = art_fig_new();                          /* the accent: nose, inner ears, beak, cheeks */
    switch (fam) {
    case FAM_RABBITS:
        art_ellipse(f, ART_PART, 0.38f, 0.24f, 0.04f, 0.14f);
        art_ellipse(f, ART_PART, 0.62f, 0.24f, 0.04f, 0.14f);
        tri(f, ART_PART, 0.46f, ny - 0.02f, 0.54f, ny - 0.02f, 0.5f, ny + 0.03f);
        break;
    case FAM_CATS:
        tri(f, ART_PART, 0.25f, 0.38f, 0.26f, 0.18f, 0.39f, 0.31f);
        tri(f, ART_PART, 0.75f, 0.38f, 0.74f, 0.18f, 0.61f, 0.31f);
        tri(f, ART_PART, 0.45f, ny - 0.03f, 0.55f, ny - 0.03f, 0.5f, ny + 0.02f);
        break;
    case FAM_MICE:
        circle(f, ART_PART, 0.21f, 0.34f, 0.1f), circle(f, ART_PART, 0.79f, 0.34f, 0.1f);
        circle(f, ART_PART, 0.5f, ny, 0.04f);
        break;
    case FAM_OWLS: tri(f, ART_PART, 0.45f, ny - 0.02f, 0.55f, ny - 0.02f, 0.5f, ny + 0.09f); break;
    case FAM_FROGS: art_ellipse(f, ART_PART, 0.27f, 0.7f, 0.06f, 0.035f), art_ellipse(f, ART_PART, 0.73f, 0.7f, 0.06f,
                                                                                         0.035f); break;
    default: break;
    }
    put(rgba, w, h, f, &sa, x, y, s);
}

/* what the member wears (head-local unit square, over the head) */
static void wear(uint8_t *rgba, int w, int h, int fam, int member, float x, float y, float s)
{
    float lw = fmaxf(0.02f, 1.2f / s), hy = FAM[fam].hy, ry = FAM[fam].ry, ex = FAM[fam].ex, ey = FAM[fam].ey;
    float top = fam == FAM_FROGS ? ey - 0.13f : hy - ry, chin = hy + ry, ny = FAM[fam].ny;
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
        st = style(frame, NULL, 0, 0);
        put(rgba, w, h, f, &st, x, y, s);
        f = art_fig_new();
        if (member == FAM_GRANDFATHER) {        /* a white moustache */
            art_ellipse(f, ART_PART, 0.43f, ny + 0.055f, 0.085f, 0.04f);
            art_ellipse(f, ART_PART, 0.57f, ny + 0.055f, 0.085f, 0.04f);
            st = style(WHITE, edge, lw, 0.1f);
        } else {                                /* a grey bun */
            circle(f, ART_PART, 0.5f, top - 0.025f, 0.075f);
            art_ellipse(f, ART_PART, 0.5f, top + 0.045f, 0.15f, 0.045f);
            st = style(GREY, edge, lw, 0.3f);
        }
        break;
    case FAM_FATHER:                            /* a bow tie */
        tri(f, ART_PART, 0.5f, chin + 0.035f, 0.34f, chin - 0.025f, 0.34f, chin + 0.095f);
        tri(f, ART_PART, 0.5f, chin + 0.035f, 0.66f, chin - 0.025f, 0.66f, chin + 0.095f);
        circle(f, ART_PART, 0.5f, chin + 0.035f, 0.035f);
        st = style(navy, NULL, 0, 0.2f);
        break;
    case FAM_MOTHER:                            /* a pearl necklace */
        for (int i = 0; i < 9; i++) {
            float a = (200 + 140.0f * i / 8) * PI_F / 180;
            circle(f, ART_PART, 0.5f + 0.24f * cosf(a), chin - 0.13f - 0.17f * sinf(a), 0.028f);
        }
        st = style(WHITE, edge, lw * 0.8f, 0.3f);
        break;
    case FAM_SON: {                             /* a cap: its crown and its peak */
        float p[2 * 14];
        for (int i = 0; i <= 12; i++) {
            float a = PI_F * i / 12;
            p[2 * i] = 0.5f + 0.22f * cosf(a);
            p[2 * i + 1] = top + 0.06f - 0.15f * sinf(a);
        }
        p[26] = 0.28f, p[27] = top + 0.08f;
        art_poly(f, ART_PART, p, 14);
        art_ellipse(f, ART_PART, 0.68f, top + 0.065f, 0.17f, 0.04f);
        st = style(red, frame, lw, 0.25f);
        break;
    }
    case FAM_DAUGHTER:                          /* a bow in the hair */
        tri(f, ART_PART, 0.66f, top + 0.02f, 0.5f, top - 0.08f, 0.52f, top + 0.12f);
        tri(f, ART_PART, 0.66f, top + 0.02f, 0.82f, top - 0.08f, 0.8f, top + 0.12f);
        circle(f, ART_PART, 0.66f, top + 0.02f, 0.04f);
        st = style(pink, frame, lw, 0.2f);
        break;
    }
    put(rgba, w, h, f, &st, x, y, s);
}

/* a member's figure (a person standing, unit square): grandparents with a cane, mothers and daughters in a dress,
 * the children small */
static void figure(art_fig *f, int member)
{
    int old = member <= FAM_GRANDMOTHER, kid = member >= FAM_SON, girl = member & 1;
    float cx = old ? 0.42f : 0.5f, hr = kid ? 0.12f : 0.13f, top = kid ? 0.3f : old ? 0.08f : 0.02f;
    float hcy = top + hr, body = hcy + hr + 0.01f, foot = 0.98f, hem = kid ? 0.84f : 0.76f;
    float half = kid ? 0.17f : 0.2f, leg = kid ? 0.07f : 0.08f;
    circle(f, ART_PART, cx, hcy, hr);
    if (member == FAM_GRANDMOTHER) circle(f, ART_PART, cx, hcy - hr * 1.1f, hr * 0.55f);
    if (member == FAM_DAUGHTER) {               /* pigtails */
        circle(f, ART_PART, cx - hr * 1.15f, hcy + 0.02f, hr * 0.45f);
        circle(f, ART_PART, cx + hr * 1.15f, hcy + 0.02f, hr * 0.45f);
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

static void put_figure(uint8_t *rgba, int w, int h, int member, const uint8_t rgb[3], float x, float y, float s)
{
    art_fig *f = art_fig_new();
    figure(f, member);
    art_style st = style(rgb, NULL, 0, 0);
    put(rgba, w, h, f, &st, x, y, s);
}

static void roundrect_at(uint8_t *rgba, int w, int h, float x, float y, float rw, float rh, float rad,
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

void fam_paint(int card, int w, int h, uint8_t *rgba)
{
    int fam = FAM_FAMILY(card) % FAM_FAMILIES, member = FAM_MEMBER(card) % FAM_MEMBERS, small = w < 52;
    float W = (float)w, H = (float)h, b = fmaxf(1, 0.014f * W);
    memset(rgba, 0, (size_t)w * h * 4);
    const uint8_t rim[4] = {RIM[0], RIM[1], RIM[2], 255};
    art_roundrect(w, h, 0.075f * W, rim, rgba);
    roundrect_at(rgba, w, h, b, b, W - 2 * b, H - 2 * b, 0.075f * W - b, IVORY);
    float m = 0.06f * W, ph = small ? H - 2 * m : H - m - 0.3f * W;
    roundrect_at(rgba, w, h, m, m, W - 2 * m, ph, 0.05f * W, FAM[fam].panel);
    int kid = member >= FAM_SON;
    float s = fminf(W - 2 * m, ph) * (kid ? 0.8f : 0.98f), hx = W / 2 - s / 2;
    float hy = m + ph / 2 - s / 2 + (kid ? 0.09f * s : 0) + (small ? -0.08f * s : 0);
    head(rgba, w, h, fam, hx, hy, s);
    wear(rgba, w, h, fam, member, hx, hy, s);
    uint8_t deep[3] = {(uint8_t)(FAM[fam].panel[0] * 0.62f), (uint8_t)(FAM[fam].panel[1] * 0.62f),
                       (uint8_t)(FAM[fam].panel[2] * 0.62f)};
    if (small) {                                /* the member at the foot, on a light disc */
        float d = 0.42f * W;
        uint8_t *disc = malloc((size_t)((int)d + 1) * ((int)d + 1) * 4);
        if (disc) {
            art_disc((int)d, IVORY, 1, 0, disc);
            art_blend(rgba, w, h, disc, (int)d, (int)d, (int)(W / 2 - d / 2), (int)(H - m - d));
            free(disc);
        }
        put_figure(rgba, w, h, member, deep, W / 2 - 0.36f * d, H - m - 0.92f * d, 0.72f * d * 1.0f);
        return;
    }
    float cell = (W - 2 * m) / FAM_MEMBERS, fs = fminf(1.3f * cell, 0.24f * W), fy = H - m - fs;
    for (int k = 0; k < FAM_MEMBERS; k++)       /* the six members, this one in the family's colour */
        put_figure(rgba, w, h, k, k == member ? deep : FAINT, m + (k + 0.5f) * cell - fs / 2, fy, fs);
    float d = 0.28f * W;                        /* the corner: this member on a light disc */
    uint8_t *disc = malloc((size_t)((int)d + 1) * ((int)d + 1) * 4);
    if (disc) {
        art_disc((int)d, IVORY, 1, 0, disc);
        art_blend(rgba, w, h, disc, (int)d, (int)d, (int)(0.045f * W), (int)(0.045f * W));
        free(disc);
    }
    put_figure(rgba, w, h, member, deep, 0.045f * W + 0.14f * d, 0.045f * W + 0.12f * d, 0.74f * d);
}

void fam_emblem(int family, int size, uint8_t *rgba)
{
    int fam = family % FAM_FAMILIES;
    memset(rgba, 0, (size_t)size * size * 4);
    art_disc(size, RIM, 1, 0, rgba);
    float b = fmaxf(1, 0.05f * size);
    uint8_t *d = malloc((size_t)size * size * 4);
    if (!d) return;
    art_disc((int)(size - 2 * b), FAM[fam].panel, 1, 0, d);
    art_blend(rgba, size, size, d, (int)(size - 2 * b), (int)(size - 2 * b), (int)b, (int)b);
    free(d);
    head(rgba, size, size, fam, 0.1f * size, 0.06f * size, 0.8f * size);
}

/* ---- the textures and the drawing -------------------------------------------------------------------------------- */
#define CACHE 96
static struct { int card, w; unsigned used; SDL_Texture *t; } cache[CACHE];
static unsigned tick;

static SDL_Texture *face(int card, int w)
{
    if (w < 4) return NULL;
    int slot = 0;
    for (int i = 0; i < CACHE; i++) {
        if (cache[i].t && cache[i].card == card && cache[i].w == w) { cache[i].used = ++tick; return cache[i].t; }
        if (!cache[i].t) slot = i;
    }
    if (cache[slot].t)                          /* full: the least used lately */
        for (int i = 0; i < CACHE; i++)
            if (cache[i].used < cache[slot].used) slot = i;
    ui_free(&cache[slot].t);
    int h = (int)(w * 1.4f + 0.5f);
    uint8_t *b = malloc((size_t)w * h * 4);
    if (!b) return NULL;
    fam_paint(card, w, h, b);
    cache[slot].card = card;
    cache[slot].w = w;
    cache[slot].used = ++tick;
    cache[slot].t = ui_tex(w, h, b);
    free(b);
    return cache[slot].t;
}

/* the face over the kit's card (its shadow, its light, its back), sx: narrowed (a flip) */
static void lay(int card, float cx, float cy, float w, float sx, float deg, int how)
{
    if (how & CD_RAISED) {                      /* lifted as the kit lifts its cards */
        float a = deg * PI_F / 180;
        cx += sinf(a) * 0.14f * w;
        cy -= cosf(a) * 0.14f * w;
    }
    SDL_Texture *t = face(card, (int)(w + 0.5f));
    if (!t) return;
    if (how & CD_DIM) SDL_SetTextureColorMod(t, 150, 150, 150);
    SDL_FRect d = {cx - w * sx / 2, cy - w * 0.7f, w * sx, w * 1.4f};
    SDL_RenderCopyExF(ui.r, t, NULL, &d, deg, NULL, SDL_FLIP_NONE);
    if (how & CD_DIM) SDL_SetTextureColorMod(t, 255, 255, 255);
}

void fam_draw(int card, float cx, float cy, float w, float deg, int how)
{
    cards_draw(DECK_FRENCH, CARD_NONE, cx, cy, w, deg, how | CD_DOWN);
    if (!(how & CD_DOWN) && card != CARD_NONE) lay(card, cx, cy, w, 1, deg, how);
}

void fam_draw_flip(int card, float cx, float cy, float w, float deg, float t, int how)
{
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    cards_draw_flip(DECK_FRENCH, CARD_NONE, cx, cy, w, deg, t, how);
    if (t < 0.5f || card == CARD_NONE) return;
    float sx = fabsf(cosf(t * PI_F)), lift = 1 + 0.08f * sinf(t * PI_F);
    lay(card, cx, cy, w * lift, sx < 0.02f ? 0.02f : sx, deg, how);
}

int fam_slide_draw(cards_slide *s)
{
    if (!s->on) return 0;
    uint32_t el = ui_ms() - s->t0;
    if (el >= (uint32_t)(s->ms + s->delay)) { s->on = 0; return 0; }
    ui_animate();
    float t = el < (uint32_t)s->delay ? 0 : (float)(el - s->delay) / s->ms, e = t * t * (3 - 2 * t);
    float x = s->x0 + (s->x1 - s->x0) * e, y = s->y0 + (s->y1 - s->y0) * e, w = s->w0 + (s->w1 - s->w0) * e;
    float a = s->a0 + (s->a1 - s->a0) * e;
    y -= sinf(e * PI_F) * fminf(0.25f * w, 0.12f * hypotf(s->x1 - s->x0, s->y1 - s->y0));
    if (s->up0 != s->up1) fam_draw_flip(s->card, x, y, w, a, s->up1 ? e : 1 - e, 0);
    else fam_draw(s->card, x, y, w, a, s->up0 ? 0 : CD_DOWN);
    return 1;
}
