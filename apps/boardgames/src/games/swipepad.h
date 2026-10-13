/*
 * What the push puzzles share on the screen (Ice Slide, Merge): the shell's tools for the four pushes (the shell's arrow
 * turned) and the hint (its lamp), the swipe on the board, and the arrow keys.
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

/* ---- the buttons: the shell's tools (game.tools = SWP_KNOBS), left, up, down, right, then the lamp ------------- */
static const int SWP_TOOL_WAY[SWP_KNOBS] = {SWP_LEFT, SWP_UP, SWP_DOWN, SWP_RIGHT, SWP_HINT};

/* the push (or SWP_HINT) of the tool under the finger; -1: none */
static inline int swp_tool_hit(float x, float y)
{
    int i = ui_tool_hit(x, y);
    return i < 0 || i >= SWP_KNOBS ? -1 : SWP_TOOL_WAY[i];
}

/* all of them: can (bit k: push k moves something; the others dimmed), lit: the push in gold light (the hint's; -1:
 * none), hint: the lamp usable, down: the button held (its gold disc; -1 none) */
static inline void swp_tools_draw(unsigned can, int lit, int hint, int down)
{
    for (int i = 0; i < SWP_KNOBS; i++) {
        int k = SWP_TOOL_WAY[i];
        if (k == SWP_HINT) {
            ui_tool(i, ICON_HINT, (hint ? 0 : UI_DIM) | (down == k ? UI_ON : 0));
            continue;
        }
        int how = UI_TINT | (k == lit ? UI_LIT : 0) | (can >> k & 1 ? 0 : UI_DIM) | (down == k ? UI_ON : 0);
        ui_tool_paint(i, swp_arrow, k, 0.62f, how);
    }
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
