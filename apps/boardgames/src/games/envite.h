/*
 * The Spanish betting games' table pieces (Mus, Truco): the pictures of their calls and answers, as badges at the
 * seats and as round buttons, and the bet's amount on a stepper. Static inline: each game takes what it uses. No
 * text: icons and digits.
 * - Quiero: a tick; no quiero: a cross; a bet: its amount; a raise: a plus and its amount; the ordago (Mus) and the
 *   falta envido (Truco): a gold bolt on red lacquer (the whole game, or what is left of it).
 * - The kit's pictures (vt_paint, white, tinted when drawn): the cross, the bolt, the mus (two arrows, one each way),
 *   the grande and chica arrows, a pair of cards, the flor's flower, a stack of backs (the mazo), a card of a suit.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef ENVITE_H
#define ENVITE_H

#include <math.h>
#include <string.h>

#include "art.h"
#include "cards_table.h"
#include "shell.h"

enum { VT_CROSS, VT_BOLT, VT_CYCLE, VT_UP, VT_DOWN, VT_PAIR, VT_FLOWER, VT_CARD, VT_KINDS };

static const uint8_t VT_RED[3] = {176, 50, 40};

/* the pictures, white on clear, size x size */
static inline void vt_paint(int kind, int size, uint8_t *rgba)
{
    art_style st = {{255, 255, 255}, {150, 150, 150}, {150, 150, 150}, {120, 120, 120}, 0, 0};
    art_fig *f = art_fig_new();
    memset(rgba, 0, (size_t)size * size * 4);
    switch (kind) {
    case VT_CROSS: {
        const float a[] = {0.2f, 0.28f, 0.28f, 0.2f, 0.8f, 0.72f, 0.72f, 0.8f};
        const float b[] = {0.72f, 0.2f, 0.8f, 0.28f, 0.28f, 0.8f, 0.2f, 0.72f};
        art_poly(f, ART_PART, a, 4);
        art_poly(f, ART_PART, b, 4);
        break;
    }
    case VT_BOLT: {
        const float b[] = {0.58f, 0.06f, 0.24f, 0.56f, 0.47f, 0.56f, 0.38f, 0.94f, 0.78f, 0.4f, 0.54f, 0.4f, 0.68f, 0.06f};
        art_poly(f, ART_PART, b, 7);
        break;
    }
    case VT_CYCLE: {                            /* two arrows, one each way: changing cards */
        const float r[] = {0.14f, 0.3f, 0.6f, 0.3f, 0.6f, 0.18f, 0.86f, 0.36f, 0.6f, 0.54f, 0.6f, 0.42f, 0.14f, 0.42f};
        const float l[] = {0.86f, 0.58f, 0.4f, 0.58f, 0.4f, 0.46f, 0.14f, 0.64f, 0.4f, 0.82f, 0.4f, 0.7f, 0.86f, 0.7f};
        art_poly(f, ART_PART, r, 7);
        art_poly(f, ART_PART, l, 7);
        break;
    }
    case VT_UP:
    case VT_DOWN: {                             /* a card and an arrow: the highest cards, the lowest */
        int up = kind == VT_UP;
        float h0 = up ? 0.16f : 0.36f;
        art_rect(f, ART_PART, 0.18f, h0, 0.5f, 0.86f);
        art_rect(f, ART_MARK, 0.23f, h0 + 0.05f, 0.45f, 0.81f);
        const float upa[] = {0.72f, 0.14f, 0.92f, 0.42f, 0.79f, 0.42f, 0.79f, 0.7f, 0.65f, 0.7f, 0.65f, 0.42f,
                             0.52f, 0.42f};
        const float dna[] = {0.72f, 0.86f, 0.92f, 0.58f, 0.79f, 0.58f, 0.79f, 0.3f, 0.65f, 0.3f, 0.65f, 0.58f,
                             0.52f, 0.58f};
        art_poly(f, ART_PART, up ? upa : dna, 7);
        st.mark[0] = st.mark[1] = st.mark[2] = 0;
        break;
    }
    case VT_PAIR:                               /* two cards side by side, alike */
        for (int k = 0; k < 2; k++) {
            float x = 0.14f + 0.38f * k;
            art_rect(f, ART_PART, x, 0.18f, x + 0.34f, 0.82f);
            art_ellipse(f, ART_MARK, x + 0.17f, 0.5f, 0.09f, 0.09f);
        }
        st.mark[0] = st.mark[1] = st.mark[2] = 0;
        break;
    case VT_FLOWER:                             /* five petals round a heart */
        for (int k = 0; k < 5; k++) {
            float a = (-90 + 72.0f * k) * 3.14159265f / 180;
            art_ellipse(f, ART_PART, 0.5f + 0.22f * cosf(a), 0.5f + 0.22f * sinf(a), 0.16f, 0.16f);
        }
        art_ellipse(f, ART_MARK, 0.5f, 0.5f, 0.11f, 0.11f);
        st.mark[0] = 0, st.mark[1] = 0, st.mark[2] = 0;
        break;
    case VT_CARD:                               /* a card's outline */
        art_rect(f, ART_PART, 0.26f, 0.14f, 0.74f, 0.86f);
        art_rect(f, ART_MARK, 0.31f, 0.19f, 0.69f, 0.81f);
        st.mark[0] = st.mark[1] = st.mark[2] = 0;
        break;
    }
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
    if (kind == VT_UP || kind == VT_DOWN || kind == VT_PAIR || kind == VT_FLOWER || kind == VT_CARD)
        for (int i = 0; i < size * size; i++)   /* the marks cut out: a clear hole */
            if (rgba[4 * i] < 128 && rgba[4 * i + 3]) {
                int k = rgba[4 * i];
                rgba[4 * i + 3] = (uint8_t)(rgba[4 * i + 3] * k / 128);
                rgba[4 * i] = rgba[4 * i + 1] = rgba[4 * i + 2] = 255;
            }
}

/* a picture d wide centred on (cx, cy), tinted */
static inline void vt_icon(int kind, float cx, float cy, float d, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_Texture *t = ct_picture(vt_paint, kind, (int)(d + 0.5f));
    SDL_SetTextureColorMod(t, r, g, b);
    SDL_SetTextureAlphaMod(t, a);
    ui_put_c(t, cx, cy, d, d);
}

/* a disc of the red lacquer (the ordago, the falta, the truco's calls) */
static inline void vt_red(float cx, float cy, float d)
{
    ui_put_c(ct_disc(VT_RED, 100, (int)(d + 0.5f)), cx, cy, d, d);
}

/* ---- badges (a player's word at his seat) ------------------------------------------------------------------------ */
static inline void vt_quiero(float cx, float cy, float d)
{
    ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    ct_pic(CT_TICK, cx, cy, 0.68f * d, 0, 140, 220, 150, 255);
}

static inline void vt_noquiero(float cx, float cy, float d)
{
    ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    vt_icon(VT_CROSS, cx, cy, 0.7f * d, 236, 110, 90, 255);
}

/* the whole game at stake: a gold bolt on red; value > 0: its worth beside it, small */
static inline void vt_bolt(float cx, float cy, float d)
{
    vt_red(cx, cy, d);
    vt_icon(VT_BOLT, cx, cy, 0.72f * d, 250, 214, 90, 255);
}

/* a raise: a plus and the amount on the dark disc */
static inline void vt_raise(float cx, float cy, float d, int n)
{
    ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    ct_plus(cx, cy, (n >= 10 ? 0.34f : 0.42f) * d, n);
}

/* an icon on the dark disc (a badge) */
static inline void vt_badge(int kind, float cx, float cy, float d)
{
    ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    vt_icon(kind, cx, cy, 0.66f * d, 246, 238, 220, 255);
}

/* ---- buttons ----------------------------------------------------------------------------------------------------- */
typedef struct { float x, y, d; int id, on; } vt_btn;   /* a round button: its id (the game's), lit */

static inline int vt_hit(const vt_btn *b, int n, float x, float y)   /* the id under (x, y); -1: none */
{
    for (int i = 0; i < n; i++)
        if (ct_on(b[i].x, b[i].y, b[i].d, x, y)) return b[i].id;
    return -1;
}

/* n buttons d wide in a row centred on (cx, y), gap: d * step between centres */
static inline void vt_row(vt_btn *b, int n, float cx, float y, float d, float step)
{
    for (int i = 0; i < n; i++) b[i].x = cx + (i - (n - 1) / 2.0f) * step * d, b[i].y = y, b[i].d = d;
}

/* a minus or a plus on a small round button (the stepper) */
static inline void vt_step(float cx, float cy, float d, int plus, int enabled)
{
    ct_button(cx, cy, d, 0, -1, 0);
    Uint8 a = enabled ? 255 : 90;
    float w = 0.46f * d, t = fmaxf(2, roundf(0.11f * d));
    SDL_Texture *bar = ct_round((int)w, (int)t, t / 2, (const uint8_t[4]){255, 255, 255, 255});
    SDL_SetTextureAlphaMod(bar, a);
    ui_put_c(bar, cx, cy, w, t);
    if (plus) ui_put_rot(bar, cx, cy, w, t, 90);
    SDL_SetTextureAlphaMod(bar, 255);
}

/* a bet's button: the amount (plus: a raise) on a gold button in gold light */
static inline void vt_bet_button(float cx, float cy, float d, int n, int plus, int live)
{
    if (live) ui_mark_gold(cx, cy, 1.8f * d);
    ct_button(cx, cy, d, 1, -1, 0);
    if (plus) ct_plus(cx, cy, (n >= 10 ? 0.32f : 0.4f) * d, n);
    else ui_number(cx, cy, (n >= 10 ? 0.38f : 0.46f) * d, n, 1);
}

#endif
