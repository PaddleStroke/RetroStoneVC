/*
 * RetroStone Board's card table (cards_table.h): the card games' table, written once on top of the card kit.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards_table.h"

/* ---- small pictures, kept ---------------------------------------------------------------------------------------- */
typedef void (*painter)(const int *k, int w, int h, uint8_t *rgba);

#define PICS 192
static struct { painter paint; int k[4], w, h; unsigned used; SDL_Texture *t; } pics[PICS];
static unsigned clock_;

/* the picture painted by `paint` with the key k0..k3, w x h: made once, kept (the least used lately goes) */
static SDL_Texture *pic(painter paint, int k0, int k1, int k2, int k3, int w, int h)
{
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    int k[4] = {k0, k1, k2, k3}, slot = -1;
    clock_++;
    for (int i = 0; i < PICS; i++) {
        if (!pics[i].t) { if (slot < 0 || pics[slot].t) slot = i; continue; }
        if (pics[i].paint == paint && pics[i].w == w && pics[i].h == h && !memcmp(pics[i].k, k, sizeof k)) {
            pics[i].used = clock_;
            SDL_SetTextureColorMod(pics[i].t, 255, 255, 255);
            SDL_SetTextureAlphaMod(pics[i].t, 255);
            return pics[i].t;
        }
        if (slot < 0 || (pics[slot].t && pics[i].used < pics[slot].used)) slot = i;
    }
    ui_free(&pics[slot].t);
    uint8_t *b = ui_buf(w, h);
    memset(b, 0, (size_t)w * h * 4);
    paint(k, w, h, b);
    pics[slot].t = ui_tex(w, h, b);
    pics[slot].paint = paint;
    memcpy(pics[slot].k, k, sizeof k);
    pics[slot].w = w;
    pics[slot].h = h;
    pics[slot].used = clock_;
    return pics[slot].t;
}

static void paint_token(const int *k, int w, int h, uint8_t *rgba) { (void)h, cards_token(k[0], w, rgba); }
static void paint_suit(const int *k, int w, int h, uint8_t *rgba) { (void)h, cards_suit_icon(k[0], k[1], w, rgba); }

static void paint_disc(const int *k, int w, int h, uint8_t *rgba)
{
    const uint8_t rgb[3] = {(uint8_t)k[0], (uint8_t)k[1], (uint8_t)k[2]};
    (void)h;
    art_disc(w, rgb, k[3] / 100.0f, 0, rgba);
}

static void paint_round(const int *k, int w, int h, uint8_t *rgba)
{
    float radius;
    memcpy(&radius, &k[1], sizeof radius);
    const uint8_t c[4] = {(uint8_t)(k[0] >> 24), (uint8_t)(k[0] >> 16), (uint8_t)(k[0] >> 8), (uint8_t)k[0]};
    art_roundrect(w, h, radius, c, rgba);
}

static void paint_icon(const int *k, int w, int h, uint8_t *rgba) { (void)h, art_icon(k[0], w, rgba); }

SDL_Texture *ct_token(int player, int size) { return pic(paint_token, player, 0, 0, 0, size, size); }
SDL_Texture *ct_suit(int deck, int suit, int size) { return pic(paint_suit, deck, suit & 3, 0, 0, size, size); }
SDL_Texture *ct_disc(const uint8_t rgb[3], int a0, int size)
{
    return pic(paint_disc, rgb[0], rgb[1], rgb[2], a0, size, size);
}

SDL_Texture *ct_round(int w, int h, float radius, const uint8_t rgba[4])
{
    int r;
    memcpy(&r, &radius, sizeof r);
    return pic(paint_round, (int)((uint32_t)rgba[0] << 24 | (uint32_t)rgba[1] << 16 | rgba[2] << 8 | rgba[3]), r, 0, 0,
               w, h);
}

SDL_Texture *ct_icon(int which, int size, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_Texture *t = pic(paint_icon, which, 0, 0, 0, size, size);
    SDL_SetTextureColorMod(t, r, g, b);
    return t;
}
