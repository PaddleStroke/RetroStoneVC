/*
 * What four pencil puzzles share on the screen (Calcudoku, Kakuro, Bridges, Loop): the pictures of their buttons (the
 * pencil of the notes, the eraser, the eye of the clashes; the lamp is the shell's), the row of wooden digit keys
 * under a number grid with the buttons beside it (a key tapped or carried to a cell), the bar (the moves, a lamp at
 * its end), a hint's gold light, the win's wave of gold light, and the level's ebony stones under a picker's card.
 * The rules' side is pzbench.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only, so a shared .c file would
 * be left out of one game's build or another's.
 */
#ifndef PZBENCH_UI_H
#define PZBENCH_UI_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "shell.h"

#define PZB_WAVE_STEP 55                        /* the win's wave: ms a cell further on */
#define PZB_WAVE_LEN 560
#define PZB_HINT_MS 1500                        /* a hint's gold light */
#define PZB_POP_MS 200                          /* a digit set down settles from a little larger */

static const uint8_t PZB_GOLD[3] = {232, 190, 92}, PZB_RED[3] = {190, 58, 42}, PZB_BLUE[3] = {30, 64, 140},
                     PZB_GIVEN[3] = {40, 34, 30}, PZB_NOTE[3] = {92, 70, 50}, PZB_CLASH[3] = {184, 44, 34};

/* ---- the buttons' pictures (on the shell's disc, in their own colours) ----------------------------------------- */
enum { PZB_PENCIL, PZB_ERASER, PZB_EYE };

static inline void pzb_paint_tool(int kind, int size, uint8_t *o)
{
    art_fig *f = art_fig_new();
    if (kind == PZB_PENCIL) {                   /* its point down to the left */
        art_band(f, ART_PART, 0.40f, 0.60f, 0.73f, 0.27f, 0.16f);
        art_fig_paint(o, size, f, PUZ_CREAM, PUZ_INK, 0.02f, 0.1f);
        f = art_fig_new();
        const float tip[] = {0.343f, 0.543f, 0.457f, 0.657f, 0.26f, 0.74f};
        art_poly(f, ART_PART, tip, 3);
        art_fig_paint(o, size, f, PZB_GOLD, PUZ_INK, 0.02f, 0.1f);
        f = art_fig_new();
        const float lead[] = {0.30f, 0.66f, 0.34f, 0.70f, 0.26f, 0.74f};
        art_poly(f, ART_PART, lead, 3);
        art_fig_paint(o, size, f, PUZ_INK, NULL, 0, 0);
    } else if (kind == PZB_ERASER) {            /* half red, half cream, lying aslant */
        art_band(f, ART_PART, 0.28f, 0.64f, 0.50f, 0.46f, 0.26f);
        art_fig_paint(o, size, f, PZB_RED, PUZ_INK, 0.02f, 0.1f);
        f = art_fig_new();
        art_band(f, ART_PART, 0.50f, 0.46f, 0.72f, 0.28f, 0.26f);
        art_fig_paint(o, size, f, PUZ_CREAM, PUZ_INK, 0.02f, 0.1f);
    } else {                                    /* an eye, its iris red */
        float lens[2 * 26];
        for (int i = 0; i <= 12; i++) {
            float t = i / 12.0f, s = sinf(PUZ_PI * t);
            lens[2 * i] = 0.2f + 0.6f * t, lens[2 * i + 1] = 0.5f - 0.2f * s;
            lens[2 * (13 + i)] = 0.8f - 0.6f * t, lens[2 * (13 + i) + 1] = 0.5f + 0.2f * s;
        }
        art_poly(f, ART_PART, lens, 26);
        art_fig_paint(o, size, f, PUZ_CREAM, PUZ_INK, 0.02f, 0.1f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.125f, 0.125f);
        art_fig_paint(o, size, f, PZB_RED, PUZ_INK, 0.015f, 0.1f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.05f, 0.05f);
        art_fig_paint(o, size, f, PUZ_INK, NULL, 0, 0);
    }
}

/* ---- a hint's light and the win's wave ------------------------------------------------------------------------- */
/* a hint's place: a gold light swelling and fading (t0: when); 0 once it has faded */
static inline int pzb_hint_glow(Uint32 t0, float cx, float cy, float d)
{
    float p = puz_pulse((float)(ui_ms() - t0), 0, PZB_HINT_MS);
    if (p <= 0) return 0;
    ui_mark_gold(cx, cy, d * p);
    ui_animate();
    return 1;
}

typedef struct { int on; float fx, fy, span; Uint32 t0; } pzb_wave;

/* from the point (fx, fy) of a grid span cells across */
static inline void pzb_wave_start(pzb_wave *w, float fx, float fy, float span)
{
    w->on = 1;
    w->fx = fx;
    w->fy = fy;
    w->span = span;
    w->t0 = ui_ms();
}

static inline Uint32 pzb_wave_ms(const pzb_wave *w) { return (Uint32)(1.5f * w->span * PZB_WAVE_STEP + PZB_WAVE_LEN); }
static inline int pzb_wave_busy(const pzb_wave *w) { return w->on && ui_ms() - w->t0 < pzb_wave_ms(w); }

/* its light on the point (gx, gy) of the grid, drawn at (cx, cy), d across at its height */
static inline void pzb_wave_at(const pzb_wave *w, float gx, float gy, float cx, float cy, float d)
{
    if (!w->on) return;
    float p = puz_pulse((float)(ui_ms() - w->t0), hypotf(gx - w->fx, gy - w->fy) * PZB_WAVE_STEP, PZB_WAVE_LEN);
    if (p > 0) ui_mark_gold(cx, cy, d * p);
}

/* after the drawing: on while it runs */
static inline void pzb_wave_tick(pzb_wave *w)
{
    if (!w->on) return;
    if (ui_ms() - w->t0 < pzb_wave_ms(w)) ui_animate();
    else w->on = 0;
}

/* ---- the digit keys and the buttons beside a number grid ------------------------------------------------------- *
 * The board's square: the grid's square at its top left, the buttons in a column at its right, the keys in a row
 * along its bottom (as many as the digits). A key tapped gives its digit; a key carried onto a cell, the same there. */
#define PZB_TOOLS 4

typedef struct {
    int nkeys, ntools;
    float gx, gy, gw;                           /* the grid's square (its frame in it) */
    float kx0, kp, ky, kw, kh;                  /* the first key's centre, the pitch, the row's centre, a key */
    float tx, ty[PZB_TOOLS], td;                /* the buttons */
    int made_w, made_h;
    SDL_Texture *t_key;
    puz_font font;
    struct { int on, key, moved; float x, y, x0, y0; } drag;
} pzb_pad;

static inline void pzb_pad_layout(pzb_pad *k, box b, int nkeys, int ntools)
{
    float S = fminf(b.w, b.h), u = ui.unit;
    k->nkeys = nkeys;
    k->ntools = ntools;
    k->kh = floorf(0.98f * u);
    k->gw = floorf(S - k->kh - 0.24f * u);
    k->gx = floorf(b.x + (b.w - S) / 2);
    k->gy = floorf(b.y + (b.h - S) / 2);
    k->kp = S / nkeys;
    k->kw = floorf(fminf(1.3f * u, 0.88f * k->kp));
    k->kx0 = k->gx + k->kp / 2;
    k->ky = k->gy + S - k->kh / 2;
    k->td = floorf(fminf(0.92f * u, 0.9f * (S - k->gw)));
    k->tx = k->gx + k->gw + (S - k->gw) / 2;
    for (int i = 0; i < ntools; i++) k->ty[i] = k->gy + k->gw * (i + 0.5f) / ntools;
    int w = (int)k->kw, h = (int)k->kh;
    if (w < 2 || h < 2) return;
    if (k->made_w != w || k->made_h != h || !k->t_key) {
        k->made_w = w;
        k->made_h = h;
        ui_free(&k->t_key);
        uint8_t *o = ui_buf(w, h);
        int q = w > h ? w : h;
        uint8_t *plank = malloc((size_t)q * q * 4);
        if (plank) {
            art_board_n(1, 1, q, 0, 0, plank);
            for (int y = 0; y < h; y++) memcpy(o + 4 * (size_t)y * w, plank + 4 * (size_t)y * q, (size_t)w * 4);
            free(plank);
        }
        puz_bevel(o, w, h, 0.14f * fminf(w, h), fmaxf(2, 0.09f * fminf(w, h)), 0.32f);
        k->t_key = ui_tex(w, h, o);
        puz_font_make(&k->font, h, 1);
    }
}

static inline float pzb_key_x(const pzb_pad *k, int d) { return k->kx0 + (d - 1) * k->kp; }

static inline int pzb_key_at(const pzb_pad *k, float x, float y)
{
    if (fabsf(y - k->ky) > 0.62f * k->kh || x < k->gx || x >= k->gx + k->nkeys * k->kp) return 0;
    return (int)((x - k->gx) / k->kp) + 1;
}

static inline int pzb_tool_at(const pzb_pad *k, float x, float y)
{
    for (int i = 0; i < k->ntools; i++)
        if (ui_button_hit(k->tx, k->ty[i], k->td, x, y)) return i;
    return -1;
}

/* a key: the wooden tile, its digit (small in its corner of a cell when notes are on), scaled s, faded a */
static inline void pzb_key(const pzb_pad *k, int d, float x, float y, float s, Uint8 a, int notes)
{
    SDL_SetTextureAlphaMod(k->t_key, a);
    ui_shadow_flat(x, y, k->kw * s, k->kh * s, a / 255.0f * 0.8f);
    ui_put_c(k->t_key, x, y, k->kw * s, k->kh * s);
    SDL_SetTextureAlphaMod(k->t_key, 255);
    if (notes) {
        float nx = x + ((d - 1) % 3 - 1) * 0.26f * k->kw * s, ny = y + ((d - 1) / 3 - 1) * 0.25f * k->kh * s;
        puz_number(&k->font, nx, ny, 0.36f * k->kh * s, d, PUZ_INK, a);
    } else {
        puz_number(&k->font, x, y, 0.58f * k->kh * s, d, PUZ_INK, a);
    }
}

/* the keys (lit: a digit in gold light; faded: bit d, all its digits placed) */
static inline void pzb_keys(const pzb_pad *k, int lit, unsigned faded, int notes)
{
    for (int d = 1; d <= k->nkeys; d++) {
        float x = pzb_key_x(k, d);
        if (d == lit) ui_mark_gold(x, k->ky, 1.7f * k->kw);
        int held = k->drag.on && k->drag.key == d && k->drag.moved;
        pzb_key(k, d, x, k->ky, 1, (Uint8)(held ? 90 : faded >> d & 1 ? 105 : 255), notes);
    }
}

/* the key carried, above the finger (0: none) */
static inline int pzb_carried(const pzb_pad *k, float *x, float *y)
{
    if (!k->drag.on || !k->drag.key || !k->drag.moved) return 0;
    *x = k->drag.x;
    *y = k->drag.y - 0.6f * k->kh;
    return k->drag.key;
}

/* the buttons, kinds[i] a picture (PZB_*) or -1 the shell's lamp; how[i] UI_* */
static inline void pzb_tools(const pzb_pad *k, const int *kinds, const int *how)
{
    for (int i = 0; i < k->ntools; i++) {
        if (kinds[i] < 0) ui_button(k->tx, k->ty[i], k->td, ICON_HINT, how[i]);
        else ui_button_paint(k->tx, k->ty[i], k->td, pzb_paint_tool, kinds[i], 0.84f, how[i]);
    }
}

/* the finger on a key: 1 when it took it (a tap or a carry follows) */
static inline int pzb_pad_press(pzb_pad *k, float x, float y)
{
    memset(&k->drag, 0, sizeof k->drag);
    int d = pzb_key_at(k, x, y);
    if (!d) return 0;
    k->drag.on = 1;
    k->drag.key = d;
    k->drag.x = k->drag.x0 = x;
    k->drag.y = k->drag.y0 = y;
    return 1;
}

static inline void pzb_pad_motion(pzb_pad *k, float x, float y)
{
    if (!k->drag.on) return;
    k->drag.x = x;
    k->drag.y = y;
    if (hypotf(x - k->drag.x0, y - k->drag.y0) > 0.3f * k->kw) k->drag.moved = 1;
    ui_animate();
}

/* the finger lifted: the key tapped (*tapped 1) or carried to (*dx, *dy: under the key); 0: no key held */
static inline int pzb_pad_release(pzb_pad *k, float x, float y, int *tapped, float *dx, float *dy)
{
    if (!k->drag.on) return 0;
    k->drag.on = 0;
    ui_animate();
    *tapped = !k->drag.moved;
    if (*tapped && pzb_key_at(k, x, y) != k->drag.key) return 0;
    *dx = x;
    *dy = y - 0.6f * k->kh;
    return k->drag.key;
}

/* ---- the bar: the moves, and a lamp at its right end (a hint) -------------------------------------------------- */
typedef struct { box b; float lx, ly, ld; } pzb_bar;

static inline void pzb_bar_draw(pzb_bar *k, box b, SDL_Texture *icon, int moves, int lamp_how)
{
    k->b = b;
    k->ld = 0.84f * ui.unit;
    k->lx = b.x + b.w - k->ld / 2 - 0.05f * ui.unit;
    k->ly = b.y + b.h / 2;
    ui_bar_number(b, icon, moves);
    ui_button(k->lx, k->ly, k->ld, ICON_HINT, lamp_how);
}

static inline int pzb_bar_lamp(const pzb_bar *k, float x, float y)
{
    return k->b.w > 0 && ui_button_hit(k->lx, k->ly, k->ld, x, y);
}

/* ---- pictures for the shell ------------------------------------------------------------------------------------ */
/* the level's ebony stones (level + 1 of them) in a row, their tops at y, in a picker's card */
static inline void pzb_stones(uint8_t *rgba, int size, int y, int level)
{
    int d = (int)(0.1f * size);
    uint8_t *st = calloc((size_t)d * d, 4);
    if (!st) return;
    art_counter(COUNTER_STONE, 1, d, st);
    for (int i = 0; i <= level; i++)
        art_blend(rgba, size, size, st, d, d, (int)(size / 2.0f + (i - level / 2.0f) * 1.25f * d - d / 2.0f), y);
    free(st);
}

/* the bar's small picture: the token at 0.6 unit, made again when the unit changes */
static inline void pzb_small(SDL_Texture **t, int *made, void (*token)(int player, int size, uint8_t *rgba))
{
    int small = (int)(0.6f * ui.unit);
    if (*made == small && *t) return;
    *made = small;
    ui_free(t);
    uint8_t *b = calloc((size_t)small * small, 4);
    if (!b) return;
    token(0, small, b);
    *t = ui_tex(small, small, b);
    free(b);
}

#endif
