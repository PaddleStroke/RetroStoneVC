/*
 * The felt table of belote and président (the trick game and the climbing game with hidden hands): what both draw on
 * top of the card kit (cards.h) and the shell. The cards on the move (slides that may wait unseen, or held where they
 * start, before they go; a tag says which card of the table each one is, so the game hides it in its place until it
 * lands), the players' chips, rounded panels and chips of one colour, tinted icons, and the viewer's hand along the
 * bottom of the table in one or two rows, fanned a little.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="belote" builds
 * belote.c and belote_*.c), so a shared .c file would be left out of one game's build or the other's. The state lives
 * in function-local statics: one copy in each game.
 */
#ifndef FELTABLE_H
#define FELTABLE_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "shell.h"

/* ---- the cards on the move --------------------------------------------------------------------------------------- */
#define FT_MOVES 96
#define FT_REST 320                             /* after the last card lands: a moment before the device passes */

typedef struct { cards_slide s; int show, tag, a, b; } ft_move;
typedef struct { ft_move mv[FT_MOVES]; int n; Uint32 quiet; } ft_moves;
static inline ft_moves *ft_m(void) { static ft_moves m; return &m; }

static inline void ft_calm(void)
{
    ft_m()->n = 0;
    ft_m()->quiet = 0;
}

/* a card from A to B (turning over on the way if up0 != up1), moving `go` ms from now for `ms`; drawn from `show` ms
 * on (held at A until it moves); tag, a, b: which card of the table it is (ft_hides); sound as it lands (-1: none) */
static inline void ft_slide(int deck, int card, int up0, float x0, float y0, float w0, float a0, int up1, float x1,
                            float y1, float w1, float a1, int ms, int go, int show, int tag, int a, int b, int sound)
{
    ft_moves *m = ft_m();
    if (m->n >= FT_MOVES) return;
    ft_move *v = &m->mv[m->n++];
    cards_slide_start(&v->s, deck, card, x0, y0, w0, a0, up0, x1, y1, w1, a1, up1, ms, go);
    v->show = show;
    v->tag = tag;
    v->a = a;
    v->b = b;
    Uint32 end = ui_ms() + (Uint32)(go + ms + FT_REST);
    if (end > m->quiet) m->quiet = end;
    if (sound >= 0) ui_sound_later(sound, go + ms - 40 > 0 ? go + ms - 40 : 0);
}

/* how many cards on the move carry this tag (a, b: -1 any) */
static inline int ft_hides(int tag, int a, int b)
{
    ft_moves *m = ft_m();
    int n = 0;
    for (int i = 0; i < m->n; i++)
        if (m->mv[i].tag == tag && cards_slide_busy(&m->mv[i].s) && (a < 0 || m->mv[i].a == a) &&
            (b < 0 || m->mv[i].b == b))
            n++;
    return n;
}

static inline int ft_busy(void)
{
    ft_moves *m = ft_m();
    for (int i = 0; i < m->n; i++)
        if (cards_slide_busy(&m->mv[i].s)) return 1;
    return m->quiet && ui_ms() < m->quiet;
}

static inline void ft_draw_moves(void)
{
    ft_moves *m = ft_m();
    for (int i = 0; i < m->n; i++) {
        ft_move *v = &m->mv[i];
        if (v->s.on && ui_ms() - v->s.t0 < (Uint32)v->show) { ui_animate(); continue; }
        cards_slide_draw(&v->s);
    }
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* a player's chip, size x size */
static inline SDL_Texture *ft_token(int player, int size)
{
    static SDL_Texture *t[MAX_PLAYERS];
    static int made[MAX_PLAYERS];
    if (player < 0 || player >= MAX_PLAYERS) return NULL;
    if (size < 4) size = 4;
    if (made[player] != size || !t[player]) {
        made[player] = size;
        ui_free(&t[player]);
        uint8_t *b = ui_buf(size, size);
        memset(b, 0, (size_t)size * size * 4);
        cards_token(player, size, b);
        t[player] = ui_tex(size, size, b);
    }
    return t[player];
}

/* a rounded rectangle of one colour, w x h (kept by slot, 0..7) */
static inline SDL_Texture *ft_round(int slot, int w, int h, float radius, const uint8_t rgba[4])
{
    static SDL_Texture *t[8];
    static int tw[8], th[8];
    static uint8_t col[8][4];
    if (slot < 0 || slot >= 8 || w < 2 || h < 2) return NULL;
    if (!t[slot] || tw[slot] != w || th[slot] != h || memcmp(col[slot], rgba, 4)) {
        tw[slot] = w, th[slot] = h;
        memcpy(col[slot], rgba, 4);
        ui_free(&t[slot]);
        uint8_t *b = ui_buf(w, h);
        art_roundrect(w, h, radius, rgba, b);
        t[slot] = ui_tex(w, h, b);
    }
    return t[slot];
}

/* the panel of a deal's scores: dark, nearly opaque, over the table dimmed */
static inline void ft_panel(box table, float x, float y, float w, float h)
{
    static const uint8_t c[4] = {28, 34, 32, 250};
    ui_fill(table.x, table.y, table.w, table.h, 0, 0, 0, 70);
    ui_put(ft_round(0, (int)w, (int)h, 0.4f * ui.unit, c), x, y, w, h);
}

/* the width of ui_number_signed(v, plus) at height h */
static inline float ft_number_w(int v, float h, int plus)
{
    int digits = 1;
    for (int a = abs(v); a >= 10; a /= 10) digits++;
    return (0.74f * (digits - 1) + 0.62f + (v < 0 || (plus && v > 0) ? 0.56f : 0)) * floorf(h + 0.5f);
}

/* a faint cream line across a panel */
static inline void ft_rule(float x, float y, float w)
{
    ui_fill(x, y, w, fmaxf(1, 0.02f * ui.unit), 246, 238, 220, 40);
}

/* the shell's icon (art.h ICON_*), tinted, size x size (kept by slot, 0..7) */
static inline SDL_Texture *ft_icon(int slot, int which, int size, Uint8 r, Uint8 g, Uint8 b)
{
    static SDL_Texture *t[8];
    static int made[8], kind[8];
    if (slot < 0 || slot >= 8) return NULL;
    if (size < 2) size = 2;
    if (!t[slot] || made[slot] != size || kind[slot] != which) {
        made[slot] = size, kind[slot] = which;
        ui_free(&t[slot]);
        art_icon(which, size, ui_buf(size, size));
        t[slot] = ui_tex(size, size, ui_buf(size, size));
    }
    SDL_SetTextureColorMod(t[slot], r, g, b);
    return t[slot];
}

/* ---- the viewer's hand along the bottom of the table --------------------------------------------------------------
 * n cards (in the game's order) in one row, or in two when there are more than per_row (the first half behind, the
 * second lower, in front), as big as w_max and the height h_max allow; their spots, the cards' width, the hand's top
 * (above a raised card). */
#define FT_HAND 24
typedef struct { float w, top; int n, rows, per; cards_spot spot[FT_HAND]; } ft_hand;

static inline void ft_hand_layout(ft_hand *h, box b, int n, int per_row, float w_max, float h_max)
{
    if (n > FT_HAND) n = FT_HAND;
    h->n = n;
    h->rows = n > per_row ? 2 : 1;
    h->per = h->rows == 2 ? (n + 1) / 2 : n > 0 ? n : 1;
    float asp = cards_aspect(DECK_FRENCH);
    float w = fminf(w_max, b.w * 0.97f / (1 + (h->per - 1) * 0.36f));
    float hh = w * asp * (1.2f + 0.48f * (h->rows - 1));
    if (hh > h_max) w *= h_max / hh;
    h->w = floorf(w);
    float hgt = h->w * asp, row_dy = 0.48f * hgt;
    float y_last = b.y + b.h - 0.56f * hgt, y_first = y_last - (h->rows - 1) * row_dy;
    h->top = y_first - hgt / 2 - 0.16f * h->w;
    for (int r = 0, i = 0; r < h->rows; r++) {
        int k = h->rows == 2 ? (r == 0 ? h->per : n - h->per) : n;
        cards_fan(b.x + b.w / 2, r == 0 ? y_first : y_last, b.w * 0.97f, h->w, asp, k, fminf(10.0f, 1.6f * k),
                  &h->spot[i]);
        i += k;
    }
}

/* the card of the hand under (x, y) (the front one first); -1: none */
static inline int ft_hand_hit(const ft_hand *h, float x, float y)
{
    float lift = 0.14f * h->w;                  /* a raised card can be hit where it shows */
    int i = cards_hit(h->spot, h->n, h->w, cards_aspect(DECK_FRENCH), x, y);
    if (i < 0) i = cards_hit(h->spot, h->n, h->w, cards_aspect(DECK_FRENCH), x, y + lift);
    return i;
}

#endif
