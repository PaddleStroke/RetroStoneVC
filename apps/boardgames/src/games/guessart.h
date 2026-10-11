/*
 * The guessing games' shared drawing (Mystery Face, Sea Battle): a pen for pictures of many colours (art.h's figures
 * drawn one colour at a time, each layer rendered only in its bounds and blended over the last), round buttons on the
 * table in the shell's own look (its dark disc, its cream icons; gold when on; an ivory medallion for a button that
 * shows a picture in its colours), and a tick.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef GUESSART_H
#define GUESSART_H

#include <math.h>
#include <stdlib.h>

#include "shell.h"

/* ---- the pen ----------------------------------------------------------------------------------------------------- */
typedef struct {
    uint8_t *o;
    int w, h;                                   /* the picture, w x h RGBA */
    float s, ox, oy;                            /* pixels per unit; the unit square's (0, 0) at pixel (ox, oy) */
    art_fig *f;
    float x0, y0, x1, y1;                       /* the layer's bounds (units) */
} ga_pen;

static inline void ga_reset(ga_pen *p)
{
    p->x0 = p->y0 = 1e9f;
    p->x1 = p->y1 = -1e9f;
}

static inline void ga_begin(ga_pen *p, uint8_t *o, int w, int h, float s, float ox, float oy)
{
    p->o = o;
    p->w = w;
    p->h = h;
    p->s = s;
    p->ox = ox;
    p->oy = oy;
    p->f = art_fig_new();
    ga_reset(p);
}

static inline void ga_grow(ga_pen *p, float x0, float y0, float x1, float y1)
{
    p->x0 = fminf(p->x0, x0);
    p->y0 = fminf(p->y0, y0);
    p->x1 = fmaxf(p->x1, x1);
    p->y1 = fmaxf(p->y1, y1);
}

static inline void ga_ell(ga_pen *p, int role, float cx, float cy, float rx, float ry)
{
    art_ellipse(p->f, role, cx, cy, rx, ry);
    ga_grow(p, cx - rx, cy - ry, cx + rx, cy + ry);
}

static inline void ga_poly(ga_pen *p, int role, const float *xy, int n)
{
    art_poly(p->f, role, xy, n);
    for (int i = 0; i < n; i++) ga_grow(p, xy[2 * i], xy[2 * i + 1], xy[2 * i], xy[2 * i + 1]);
}

static inline void ga_rect(ga_pen *p, int role, float x0, float y0, float x1, float y1)
{
    art_rect(p->f, role, x0, y0, x1, y1);
    ga_grow(p, x0, y0, x1, y1);
}

/* a band between radii r0 and r1, from deg0 to deg1 (counter-clockwise from the right, y down on the screen) */
static inline void ga_arc(ga_pen *p, int role, float cx, float cy, float r0, float r1, float deg0, float deg1)
{
    art_arc(p->f, role, cx, cy, r0, r1, deg0, deg1);
    ga_grow(p, cx - r1, cy - r1, cx + r1, cy + r1);
}

/* the layer painted in one colour: its outline a darker shade (line: pixels, 0 none), its marks in `mark` (NULL: a
 * darker shade), lit (light, as art_style's); then a new layer */
static inline void ga_paint(ga_pen *p, const float rgb[3], const float *mark, float light, float line)
{
    if (p->x1 >= p->x0) {
        art_style st;
        for (int i = 0; i < 3; i++) {
            st.fill[i] = rgb[i];
            st.outer[i] = rgb[i] * 0.42f;
            st.inner[i] = rgb[i] * 0.62f;
            st.mark[i] = mark ? mark[i] : rgb[i] * 0.5f;
        }
        st.light = light;
        st.line = line / p->s;
        float pad = line + 2;
        int px0 = (int)floorf(p->ox + p->x0 * p->s - pad), py0 = (int)floorf(p->oy + p->y0 * p->s - pad);
        int px1 = (int)ceilf(p->ox + p->x1 * p->s + pad), py1 = (int)ceilf(p->oy + p->y1 * p->s + pad);
        px0 = px0 < 0 ? 0 : px0;
        py0 = py0 < 0 ? 0 : py0;
        px1 = px1 > p->w ? p->w : px1;
        py1 = py1 > p->h ? p->h : py1;
        int w = px1 - px0, h = py1 - py0;
        uint8_t *t = w > 0 && h > 0 ? malloc((size_t)w * h * 4) : NULL;
        if (t) {
            art_render(p->f, &st, w, h, p->s, p->ox - px0, p->oy - py0, t);
            art_blend(p->o, p->w, p->h, t, w, h, px0, py0);
            free(t);
        }
    }
    art_fig_free(p->f);
    p->f = art_fig_new();
    ga_reset(p);
}

static inline void ga_end(ga_pen *p)
{
    art_fig_free(p->f);
    p->f = NULL;
}

/* a tick, white (tint it), size x size */
static inline void ga_tick(int size, uint8_t *rgba)
{
    static const float tick[] = {0.17f, 0.53f, 0.29f, 0.41f, 0.42f, 0.55f, 0.72f, 0.22f, 0.84f, 0.34f, 0.42f, 0.78f};
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    art_poly(f, ART_PART, tick, 6);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

/* ---- round buttons ----------------------------------------------------------------------------------------------- */
enum { GA_ON = 1, GA_DIM = 2, GA_IVORY = 4 };
typedef struct { SDL_Texture *disc, *on, *ivory, *ivory_on; int d; } ga_buttons;

/* the discs, d pixels across: the shell's dark green (on: its gold); ivory: a medallion inside the rim */
static inline void ga_buttons_make(ga_buttons *b, int d)
{
    if (b->d == d) return;
    b->d = d;
    static const uint8_t dark[3] = {54, 72, 64}, gold[3] = {150, 118, 46};
    SDL_Texture **t[4] = {&b->disc, &b->on, &b->ivory, &b->ivory_on};
    for (int k = 0; k < 4; k++) {
        uint8_t *o = ui_buf(d, d);
        art_disc(d, k & 1 ? gold : dark, 1, 0, o);
        if (k >= 2) {                           /* the ivory medallion */
            int m = (int)(0.8f * d), off = (d - m) / 2;
            uint8_t *in = malloc((size_t)m * m * 4);
            if (in) {
                static const uint8_t ivory[3] = {242, 234, 216};
                art_disc(m, ivory, 1, 0, in);
                art_blend(o, d, d, in, m, m, off, off);
                free(in);
            }
        }
        ui_free(t[k]);
        *t[k] = ui_tex(d, d, o);
    }
}

/* a button centred on (cx, cy), d across, its icon k times as wide; how: GA_ON (gold), GA_DIM (faded), GA_IVORY */
static inline void ga_button(const ga_buttons *b, SDL_Texture *icon, float cx, float cy, float d, float k, int how)
{
    SDL_Texture *disc = how & GA_IVORY ? (how & GA_ON ? b->ivory_on : b->ivory) : how & GA_ON ? b->on : b->disc;
    Uint8 a = how & GA_DIM ? 100 : 255;
    if (disc) {
        SDL_SetTextureAlphaMod(disc, a);
        ui_put_c(disc, cx, cy, d, d);
        SDL_SetTextureAlphaMod(disc, 255);
    }
    if (icon) {
        SDL_SetTextureAlphaMod(icon, a);
        ui_put_c(icon, cx, cy, d * k, d * k);
        SDL_SetTextureAlphaMod(icon, 255);
    }
}

/* (x, y) on a button d across (a finger's width of slack) */
static inline int ga_in(float cx, float cy, float d, float x, float y)
{
    return hypotf(x - cx, y - cy) <= d * 0.56f;
}

/* an icon texture from a painter (cream when tinted: the shell's icons) */
static inline SDL_Texture *ga_icon(void (*paint)(int, uint8_t *), int size, int cream)
{
    uint8_t *o = ui_buf(size, size);
    paint(size, o);
    SDL_Texture *t = ui_tex(size, size, o);
    if (t && cream) SDL_SetTextureColorMod(t, 252, 244, 226);
    return t;
}

#endif
