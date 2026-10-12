/*
 * The guessing games' shared drawing (Mystery Face, Sea Battle): a pen for pictures of many colours (art.h's figures
 * drawn one colour at a time, each layer rendered only in its bounds and blended over the last). Their round buttons
 * are the shell's (ui_button).
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

/* ---- kept for the games written with them (new code: ui_button, ui_button_paint, ICON_TICK) ---------------------- */
static inline void ga_tick(int size, uint8_t *rgba) { art_icon(ICON_TICK, size, rgba); }

enum { GA_ON = 1, GA_DIM = 2, GA_IVORY = 4 };
typedef struct { int d; } ga_buttons;
static inline void ga_buttons_make(ga_buttons *b, int d) { b->d = d; }

/* a button centred on (cx, cy), d across, its icon (a texture) k times as wide: the shell's disc */
static inline void ga_button(const ga_buttons *b, SDL_Texture *icon, float cx, float cy, float d, float k, int how)
{
    (void)b;
    Uint8 a = how & GA_DIM ? 100 : 255;
    ui_button_paint(cx, cy, d, NULL, 0, 1, (how & GA_ON ? UI_ON : 0) | (how & GA_DIM ? UI_DIM : 0) |
                                           (how & GA_IVORY ? UI_IVORY : 0));
    if (!icon) return;
    SDL_SetTextureAlphaMod(icon, a);
    ui_put_c(icon, cx, cy, d * k, d * k);
    SDL_SetTextureAlphaMod(icon, 255);
}

static inline int ga_in(float cx, float cy, float d, float x, float y) { return ui_button_hit(cx, cy, d, x, y); }

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
