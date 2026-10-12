/*
 * What the push puzzles share on the screen (Ice Slide, Merge): a row of the shell's round buttons under the board (the
 * four pushes: the shell's arrow turned; then the hint: its lamp), the swipe on the board, and the arrow keys.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only, so a shared .c file would
 * be left out of one game's build or the other's.
 *
 * The arrow keys: the shell gives the game its keys (game.key); swp_key_way() turns an arrow into its push.
 */
#ifndef SWIPEPAD_H
#define SWIPEPAD_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"

enum { SWP_UP, SWP_RIGHT, SWP_DOWN, SWP_LEFT, SWP_HINT, SWP_KNOBS };

static inline art_style swp_style(const uint8_t rgb[3], const uint8_t ink[3], float line, float light)
{
    art_style st = {{0}, {0}, {0}, {0}, light, line};
    for (int k = 0; k < 3; k++) {
        st.fill[k] = rgb[k];
        st.outer[k] = st.inner[k] = st.mark[k] = ink[k];
    }
    return st;
}

/* an arrow button's picture (ui_button_paint's painter): the shell's arrow (ICON_ARROW, pointing right) turned to
 * the push `way` (a quarter clockwise each step from right) */
static inline void swp_arrow(int way, int size, uint8_t *o)
{
    uint8_t *a = calloc((size_t)size * size, 4), *b = calloc((size_t)size * size, 4);
    if (a && b) {
        art_icon(ICON_ARROW, size, a);
        for (int q = 0; q < (way + 3) % 4; q++) {   /* right 0, down 1, left 2, up 3 quarters */
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++)
                    memcpy(b + 4 * ((size_t)x * size + size - 1 - y), a + 4 * ((size_t)y * size + x), 4);
            memcpy(a, b, (size_t)size * size * 4);
        }
        memcpy(o, a, (size_t)size * size * 4);
    }
    free(a);
    free(b);
}

/* ---- the row of buttons: the shell's round buttons ------------------------------------------------------------- */
typedef struct {
    float cx[SWP_KNOBS], cy[SWP_KNOBS], d;
    int down;                                   /* the button held (-1: none) */
} swp_pad;

/* the buttons in a row across `strip`, left to right: left, up, down, right, then the lamp a little apart */
static inline void swp_place(swp_pad *p, box strip)
{
    static const int ORDER[SWP_KNOBS] = {SWP_LEFT, SWP_UP, SWP_DOWN, SWP_RIGHT, SWP_HINT};
    float d = fminf(0.84f * strip.h, strip.w / 6.6f), pitch = 1.24f * d, apart = 0.5f * d;
    float x = strip.x + (strip.w - 4 * pitch - apart) / 2, cy = strip.y + strip.h / 2;
    for (int i = 0; i < SWP_KNOBS; i++) {
        int k = ORDER[i];
        p->cx[k] = x + i * pitch + (i == SWP_KNOBS - 1 ? apart : 0);
        p->cy[k] = cy;
    }
    p->d = d;
    p->down = -1;
}

static inline int swp_hit(const swp_pad *p, float x, float y)
{
    for (int k = 0; k < SWP_KNOBS; k++)
        if (ui_button_hit(p->cx[k], p->cy[k], p->d, x, y)) return k;
    return -1;
}

/* the whole row: can (bit k: push k moves something; the others dimmed), lit: the button in gold light (the hint's
 * push; -1: none), hint: the lamp usable. The button held down shows the gold disc. */
static inline void swp_draw(const swp_pad *p, unsigned can, int lit, int hint)
{
    for (int k = 0; k < SWP_HINT; k++) {
        int how = UI_TINT | (k == lit ? UI_LIT : 0) | (can >> k & 1 ? 0 : UI_DIM) | (p->down == k ? UI_ON : 0);
        ui_button_paint(p->cx[k], p->cy[k], p->d, swp_arrow, k, 0.62f, how);
    }
    ui_button(p->cx[SWP_HINT], p->cy[SWP_HINT], p->d, ICON_HINT,
              (hint ? 0 : UI_DIM) | (p->down == SWP_HINT ? UI_ON : 0));
}

/* ---- the swipe --------------------------------------------------------------------------------------------------- */
typedef struct { int on, done; float x0, y0; } swp_swipe;

static inline int swp_way(float dx, float dy)
{
    return fabsf(dx) > fabsf(dy) ? (dx > 0 ? SWP_RIGHT : SWP_LEFT) : (dy > 0 ? SWP_DOWN : SWP_UP);
}

static inline void swp_begin(swp_swipe *s, float x, float y)
{
    s->on = 1;
    s->done = 0;
    s->x0 = x;
    s->y0 = y;
}

/* the finger moving: past `far` (pixels) the push happens at once (its way), once a swipe; else -1 */
static inline int swp_follow(swp_swipe *s, float x, float y, float far)
{
    if (!s->on || s->done || hypotf(x - s->x0, y - s->y0) < far) return -1;
    s->done = 1;
    return swp_way(x - s->x0, y - s->y0);
}

/* the finger lifted: a push if it went past `near` and none happened yet; *tap: it hardly moved (a tap) */
static inline int swp_end(swp_swipe *s, float x, float y, float near, int *tap)
{
    float l = hypotf(x - s->x0, y - s->y0);
    int on = s->on, done = s->done;
    s->on = 0;
    *tap = on && !done && l < 0.5f * near;
    if (!on || done || l < near) return -1;
    return swp_way(x - s->x0, y - s->y0);
}

/* ---- the arrow keys ---------------------------------------------------------------------------------------------- */
/* a key the shell gave the game (game.key): the push of an arrow, else -1 */
static inline int swp_key_way(SDL_Keycode k)
{
    switch (k) {
    case SDLK_UP: return SWP_UP;
    case SDLK_RIGHT: return SWP_RIGHT;
    case SDLK_DOWN: return SWP_DOWN;
    case SDLK_LEFT: return SWP_LEFT;
    default: return -1;
    }
}

#endif
