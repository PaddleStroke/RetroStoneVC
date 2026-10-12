/*
 * The calls of trick games (Skat, Euchre): what their bidding draws on top of games/tricks_ui.h. A suit as an icon (an
 * ivory disc with the suit's pip, the kit's own pip: the middle of the ace's face), round buttons carrying a shell icon
 * (pass) or one of the game's pictures, a dark badge with a number, and a small cache of the pictures a game paints
 * (by kind and size; the game gives the painter). Static inline, like tricks_ui.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef CALLS_UI_H
#define CALLS_UI_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/feltable.h"
#include "games/tricks_ui.h"
#include "shell.h"

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* kinds 0..3: the suits (cards.h's numbers); a game's own kinds from CL_GAME on */
enum { CL_SUIT = 0, CL_GAME = 8 };
typedef void (*cl_painter)(int kind, int size, uint8_t *rgba);

/* a disc's edge smoothed: everything outside the circle of the square made clear */
static inline void cl_round(uint8_t *rgba, int s)
{
    float r = s / 2.0f;
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            float d = hypotf(x + 0.5f - r, y + 0.5f - r), a = clampf(r - d, 0, 1);
            uint8_t *p = rgba + ((size_t)y * s + x) * 4;
            p[3] = (uint8_t)(p[3] * a);
        }
}

/* a thin dark rim round a disc of side s */
static inline void cl_rim(uint8_t *rgba, int s)
{
    static const uint8_t walnut[3] = {70, 48, 30};
    uint8_t *ring = malloc((size_t)s * s * 4);
    if (!ring) return;
    art_ring(s, 0.9f, walnut, 0.75f, ring);
    art_blend(rgba, s, s, ring, s, s, 0, 0);
    free(ring);
}

/* an ivory disc carrying a picture: the middle of a card's face (a small face below 52 px wide: its big pip) */
static inline void cl_paint_face(int card, int s, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)s * s * 4);
    int w = s < 52 ? s : (int)ceilf(s / 0.6f), h = (int)(w * cards_aspect(DECK_FRENCH));
    uint8_t *face = malloc((size_t)w * h * 4);
    if (!face) return;
    cards_paint(DECK_FRENCH, card, w, h, face);
    int x0 = (w - s) / 2, y0 = (h - s) / 2;
    for (int y = 0; y < s; y++) memcpy(rgba + (size_t)y * s * 4, face + ((size_t)(y + y0) * w + x0) * 4, (size_t)s * 4);
    for (int i = 0; i < s * s; i++) rgba[i * 4 + 3] = 255;   /* the face's ivory, no card edge */
    free(face);
    cl_round(rgba, s);
    cl_rim(rgba, s);
}

static inline void cl_paint_suit(int suit, int s, uint8_t *rgba) { cl_paint_face(CARD(suit, RANK_ACE), s, rgba); }

#define CL_PICS 24
static inline SDL_Texture *cl_pic(int kind, int size, cl_painter paint)
{
    static struct { int kind, size; unsigned used; SDL_Texture *t; } pic[CL_PICS];
    static unsigned clock;
    if (size < 4) size = 4;
    int lru = 0;
    clock++;
    for (int i = 0; i < CL_PICS; i++) {
        if (pic[i].t && pic[i].kind == kind && pic[i].size == size) {
            pic[i].used = clock;
            return pic[i].t;
        }
        if (!pic[i].t) { lru = i; break; }
        if (pic[i].used < pic[lru].used) lru = i;
    }
    ui_free(&pic[lru].t);
    uint8_t *b = ui_buf(size, size);
    memset(b, 0, (size_t)size * size * 4);
    if (kind < CL_GAME) cl_paint_suit(kind & 3, size, b);
    else if (paint) paint(kind, size, b);
    pic[lru].t = ui_tex(size, size, b);
    pic[lru].kind = kind;
    pic[lru].size = size;
    pic[lru].used = clock;
    return pic[lru].t;
}

/* a picture centred, d wide; faded (alpha) */
static inline void cl_put(int kind, float cx, float cy, float d, cl_painter paint, Uint8 alpha)
{
    SDL_Texture *t = cl_pic(kind, (int)(d + 0.5f), paint);
    if (!t) return;
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put_c(t, cx, cy, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

/* ---- buttons ----------------------------------------------------------------------------------------------------- */
/* a choice (a suit, a game): the picture on the button, raised in gold light when chosen, faint when it cannot be
 * chosen */
static inline void cl_choice(int kind, float cx, float cy, float d, int chosen, int enabled, cl_painter paint)
{
    if (chosen) ui_mark_gold(cx, cy, 1.9f * d);
    cl_put(kind, cx, cy - (chosen ? 0.08f * d : 0), d, paint, enabled ? 255 : 90);
}

/* the pass button: the shell's pass icon on a round button */
static inline void cl_pass(tk_gfx *g, float cx, float cy, float d, int glow)
{
    if (glow) ui_mark_gold(cx, cy, 1.8f * d);
    tk_put(g, TK_DISC_OFF, cx, cy, d, 0, 255, 255, 255, 255);
    int s = (int)(0.62f * d);
    ui_put_c(ft_icon(7, ICON_PASS, s, 255, 255, 255), cx, cy, (float)s, (float)s);
}

/* a number in a dark disc (a bid) */
static inline void cl_badge(tk_gfx *g, float cx, float cy, float d, int v)
{
    tk_put(g, TK_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    float h = v >= 100 ? 0.36f * d : 0.46f * d;
    ui_number(cx, cy, h, v, 1);
}

/* the pass icon in a dark disc (a player who passed) */
static inline void cl_pass_badge(tk_gfx *g, float cx, float cy, float d)
{
    tk_put(g, TK_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    int s = (int)(0.6f * d);
    ui_put_c(ft_icon(6, ICON_PASS, s, 246, 238, 220), cx, cy, (float)s, (float)s);
}

static inline int cl_on(float cx, float cy, float d, float x, float y) { return hypotf(x - cx, y - cy) <= 0.56f * d; }

#endif
