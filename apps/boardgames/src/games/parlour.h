/*
 * The French parlour card games' shared helpers (Barbu, Piquet, Écarté): static inline, each game takes what it uses.
 * - Painting a game's own pictures: a figure rendered and blended over a picture; a picture blended faded.
 * - A hand where several cards are chosen at once (the cards thrown in an exchange): a mask over the hand's sorted
 *   order, the chosen cards raised; a tap toggles one.
 * - The deals of a game in a row of dots, a dot each in a player's colour, those played bright, the last one in gold.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef PARLOUR_H
#define PARLOUR_H

#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "cards.h"
#include "cards_table.h"
#include "shell.h"

/* a figure rendered over a picture of s x s (the figure freed) */
static inline void parlour_fig(uint8_t *rgba, int s, art_fig *f, const art_style *st)
{
    uint8_t *b = calloc((size_t)s * s, 4);
    if (b) {
        art_render(f, st, s, s, (float)s, 0, 0, b);
        art_blend(rgba, s, s, b, s, s, 0, 0);
        free(b);
    }
    art_fig_free(f);
}

/* src (w x h) blended over a picture of s x s at (x, y), faded (alpha 0..1) */
static inline void parlour_blend(uint8_t *rgba, int s, const uint8_t *src, int w, int h, int x, int y, float alpha)
{
    if (alpha >= 1) { art_blend(rgba, s, s, src, w, h, x, y); return; }
    uint8_t *c = malloc((size_t)w * h * 4);
    if (!c) return;
    memcpy(c, src, (size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) c[i * 4 + 3] = (uint8_t)(c[i * 4 + 3] * alpha);
    art_blend(rgba, s, s, c, w, h, x, y);
    free(c);
}

/* a card painted into a picture of s x s, w wide, its top left at (x, y), faded */
static inline void parlour_card(uint8_t *rgba, int s, int card, int w, int x, int y, float alpha)
{
    int h = (int)(w * cards_aspect(DECK_FRENCH));
    uint8_t *b = malloc((size_t)w * h * 4);
    if (!b) return;
    cards_paint(DECK_FRENCH, card, w, h, b);
    parlour_blend(rgba, s, b, w, h, x, y, alpha);
    free(b);
}

/* ---- several cards chosen: bit i the hand's i-th card ------------------------------------------------------------ */
static inline int parlour_count(unsigned mask)
{
    int k = 0;
    for (; mask; mask &= mask - 1) k++;
    return k;
}

/* the hand's cards drawn: the chosen raised, the others glowing while more may be chosen (live) */
static inline void parlour_hows(const uint8_t *h, int n, unsigned mask, int live, int more, int *how)
{
    (void)h;
    for (int i = 0; i < n; i++) how[i] = mask >> i & 1 ? CD_RAISED : live && more ? CD_GLOW : 0;
}

/* a tap on the hand: the card's index toggled in the mask (more: one more may be chosen); 1: taken */
static inline int parlour_toggle(const ct_table *T, const uint8_t *h, int n, const int *how, unsigned *mask, int more,
                                 float x, float y)
{
    int i = ct_hand_hit(T, n, how, NULL, x, y);
    (void)h;
    if (i < 0) return 0;
    if (!(*mask >> i & 1) && !more) { ui_sound(SND_DENY); return 1; }
    *mask ^= 1u << i;
    ui_sound(SND_TAP);
    return 1;
}

/* ---- the deals in a row of dots ------------------------------------------------------------------------------------
 * n dots centred on (cx, y), d wide, in groups of `per` (a gap between), dot i in player who[i]'s colour; `done` played
 * (bright), the last of them in gold light */
static inline void parlour_dots(float cx, float y, float d, int n, int per, const uint8_t *who, int done)
{
    int groups = (n + per - 1) / per;
    float span = n * 1.25f * d + (groups - 1) * d, x = cx - span / 2 + 0.62f * d;
    for (int i = 0; i < n; i++) {
        float dx = x + i * 1.25f * d + (i / per) * d;
        if (i == done - 1) ui_mark_gold(dx, y, 2.2f * d);
        SDL_Texture *t = ct_token(who[i], (int)d);
        SDL_SetTextureAlphaMod(t, i < done ? 255 : 70);
        ui_put_c(t, dx, y, d, d);
        SDL_SetTextureAlphaMod(t, 255);
    }
}

#endif
