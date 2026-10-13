/*
 * Kept for the games written with it: a game's round buttons, in knobs.h's words, drawn as the shell's (its disc and
 * the game's picture on it: ui_button_paint). New code: ui_button, ui_button_paint, ui_button_hit (shell.h) and the
 * pictures' strokes art_band, art_head, art_fig_paint (art.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef KNOBS_H
#define KNOBS_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"

static const uint8_t KNOB_WALNUT[3] = {104, 70, 46}, KNOB_DARK[3] = {52, 34, 22}, KNOB_CREAM[3] = {246, 238, 220},
                     KNOB_RED[3] = {196, 64, 48}, KNOB_GOLD[3] = {232, 190, 92};

/* a turned walnut disc (size x size, cleared first): a picture, no longer a button's */
static inline void knob_disc(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_style st = art_flat(KNOB_WALNUT, KNOB_DARK, 0.035f, 0.35f);
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.47f, 0.47f);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

static inline void knob_paint(uint8_t *o, int size, art_fig *f, const uint8_t rgb[3], const uint8_t ink[3], float line)
{
    art_fig_paint(o, size, f, rgb, ink, line, 0.1f);
}

static inline void knob_band(art_fig *f, float x0, float y0, float x1, float y1, float wd)
{
    art_band(f, ART_PART, x0, y0, x1, y1, wd);
}

static inline void knob_head(art_fig *f, float x, float y, float deg, float l) { art_head(f, ART_PART, x, y, deg, l); }

static inline void knob_check(art_fig *f)
{
    static const float tick[] = {0.25f, 0.52f, 0.33f, 0.44f, 0.45f, 0.56f, 0.70f, 0.29f, 0.78f, 0.37f, 0.45f, 0.72f};
    art_poly(f, ART_PART, tick, 6);
}

static inline void knob_flag(uint8_t *o, int size)
{
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, 0.33f, 0.2f, 0.38f, 0.8f);
    art_ellipse(f, ART_PART, 0.355f, 0.2f, 0.04f, 0.04f);
    knob_paint(o, size, f, KNOB_CREAM, KNOB_DARK, 0.01f);
    f = art_fig_new();
    static const float flag[] = {0.38f, 0.23f, 0.74f, 0.33f, 0.38f, 0.5f};
    art_poly(f, ART_PART, flag, 3);
    knob_paint(o, size, f, KNOB_RED, KNOB_DARK, 0.015f);
}

/* a game's button pictures at one size: paint(kind, size, rgba) draws kind's picture (on a clear picture) */
typedef struct {
    int size, n;
    SDL_Texture *t[16];
} knob_set;

static inline SDL_Texture *knob_tex(knob_set *k, int kind, int size, void (*paint)(int kind, int size, uint8_t *o))
{
    if (size < 4 || kind < 0 || kind >= 16) return NULL;
    if (k->size != size) {
        for (int i = 0; i < 16; i++) ui_free(&k->t[i]);
        k->size = size;
    }
    if (!k->t[kind]) {
        uint8_t *o = calloc((size_t)size * size, 4);
        if (!o) return NULL;
        paint(kind, size, o);
        k->t[kind] = ui_tex(size, size, o);
        free(o);
    }
    return k->t[kind];
}

/* a button: the shell's disc, the picture on it; lit (a gold light: it can be pressed), dimmed, on (the gold disc) */
enum { KNOB_DIM, KNOB_READY, KNOB_LIT, KNOB_ON };
static inline void knob_at(SDL_Texture *t, float cx, float cy, float d, int state)
{
    int how = state == KNOB_DIM ? UI_DIM : state == KNOB_LIT ? UI_LIT : state == KNOB_ON ? UI_ON : 0;
    ui_button_paint(cx, cy, d, NULL, 0, 1, how);
    if (!t) return;
    SDL_SetTextureAlphaMod(t, state == KNOB_DIM ? 100 : 255);
    ui_put_c(t, cx, cy, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

static inline int knob_hit(float cx, float cy, float d, float x, float y) { return ui_button_hit(cx, cy, d, x, y); }

#endif
