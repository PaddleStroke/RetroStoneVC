/*
 * The moving cards of a card game (static inline, shared by cribbage.c and ginrummy.c): the slides of the card kit
 * (cards.h) in flight, each hiding its card at its destination while it moves (a kind, a player, a slot: the game's
 * own), and the moment the table is quiet again (busy() until then). Also a floating "+n": points rising from where
 * they were scored. A candidate for the card kit (cards.h), as more card games need the same.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef CARDMOVES_H
#define CARDMOVES_H

#include <math.h>

#include "cards.h"
#include "shell.h"

#define CM_MOVES 64
#define CM_FLOATS 48
#define CM_REST 320                             /* after the last card lands: a moment before the device passes */
#define CM_FLOAT_MS 1000

typedef struct { cards_slide s; int hide, pl, slot; } cm_move;
typedef struct { float x, y; int value, badge; Uint32 t0; } cm_float;   /* badge: a number before it, -1 an icon */
typedef struct {
    cm_move m[CM_MOVES];
    int n;
    cm_float f[CM_FLOATS];
    int nf;
    Uint32 quiet;                               /* busy until then */
} cm_table;

static inline int cm_bits(unsigned m)
{
    int n = 0;
    for (; m; m &= m - 1) n++;
    return n;
}

static inline void cm_calm(cm_table *t)
{
    t->n = t->nf = 0;
    t->quiet = 0;
}

static inline void cm_until(cm_table *t, Uint32 end)
{
    if (end > t->quiet) t->quiet = end;
}

/* a card sliding (and turning over if up0 != up1), hiding (kind, pl, slot) at its destination while it moves; a sound
 * as it lands (-1: none) */
static inline void cm_slide(cm_table *t, int deck, int card, int up0, float x0, float y0, float w0, int up1, float x1,
                            float y1, float w1, int ms, int delay, int hide, int pl, int slot, int sound)
{
    if (t->n >= CM_MOVES) return;
    cm_move *m = &t->m[t->n++];
    cards_slide_start(&m->s, deck, card, x0, y0, w0, 0, up0, x1, y1, w1, 0, up1, ms, delay);
    m->hide = hide;
    m->pl = pl;
    m->slot = slot;
    cm_until(t, ui_ms() + (Uint32)(delay + ms + CM_REST));
    if (sound >= 0) ui_sound_later(sound, delay + ms - 40 > 0 ? delay + ms - 40 : 0);
}

/* a card still on its way to (kind, pl, slot) */
static inline int cm_hides(const cm_table *t, int kind, int pl, int slot)
{
    for (int i = 0; i < t->n; i++)
        if (t->m[i].hide == kind && cards_slide_busy(&t->m[i].s) && t->m[i].pl == pl && t->m[i].slot == slot)
            return 1;
    return 0;
}

static inline int cm_hiding(const cm_table *t, int kind)   /* how many cards are on their way to kind */
{
    int n = 0;
    for (int i = 0; i < t->n; i++) n += t->m[i].hide == kind && cards_slide_busy(&t->m[i].s);
    return n;
}

static inline int cm_busy(const cm_table *t)
{
    for (int i = 0; i < t->n; i++)
        if (cards_slide_busy(&t->m[i].s)) return 1;
    return t->quiet && ui_ms() < t->quiet;
}

static inline void cm_draw(cm_table *t)
{
    for (int i = 0; i < t->n; i++) cards_slide_draw(&t->m[i].s);
}

/* points scored: a plus and the digits, cream, centred on (cx, cy); the plus snapped to whole pixels (ui_number_signed's
 * plus can break into three pieces at some heights, 30 px: its bars are rounded apart) */
static inline void cm_plus(float cx, float cy, float h, int value)
{
    int ih = (int)(h + 0.5f), n = value >= 100 ? 3 : value >= 10 ? 2 : 1, t = (int)fmaxf(2, roundf(0.13f * ih));
    int sw = (int)roundf(0.46f * ih);
    if ((sw - t) & 1) sw++;
    float digits = 0.74f * ih * (n - 1) + 0.62f * ih, gap = roundf(0.1f * ih);
    int x = (int)floorf(cx - (sw + gap + digits) / 2), y = (int)floorf(cy);
    ui_fill((float)x, (float)(y - t / 2), (float)sw, (float)t, 246, 238, 220, 255);
    ui_fill((float)(x + (sw - t) / 2), (float)(y - sw / 2), (float)t, (float)sw, 246, 238, 220, 255);
    ui_number(x + sw + gap + digits / 2, cy, (float)ih, value, 1);
}

/* points rising from (x, y) at t0 */
static inline void cm_float_at(cm_table *t, float x, float y, int value, int badge, Uint32 t0)
{
    if (t->nf >= CM_FLOATS) return;
    t->f[t->nf++] = (cm_float){x, y, value, badge, t0};
    cm_until(t, t0 + CM_FLOAT_MS);
}

/* the points in the air: a dark pill (pill: its texture), the badge's number (or the icon) and a plus with the digits;
 * they rise and fade */
static inline void cm_floats_draw(const cm_table *t, SDL_Texture *pill, SDL_Texture *icon)
{
    Uint32 now = ui_ms();
    float u = ui.unit;
    for (int i = 0; i < t->nf; i++) {
        const cm_float *f = &t->f[i];
        if (now < f->t0 || now >= f->t0 + CM_FLOAT_MS) continue;
        ui_animate();
        float k = (float)(now - f->t0) / CM_FLOAT_MS, rise = 0.9f * u * (1 - (1 - k) * (1 - k));
        float x = floorf(f->x), y = floorf(f->y - rise), h = floorf(0.4f * u), extra = f->badge ? 0.75f * u : 0;
        float a = k < 0.75f ? 1 : (1 - k) / 0.25f;
        int digits = f->value >= 10 ? 2 : 1;
        SDL_SetTextureAlphaMod(pill, (Uint8)(210 * a));
        ui_put_c(pill, x + extra / 2, y, (0.9f + 0.3f * digits) * u * 0.62f + extra, 0.58f * u);
        SDL_SetTextureAlphaMod(pill, 255);
        if (f->badge > 0) ui_number(x - 0.12f * u, y, floorf(0.32f * u), f->badge, 1);
        else if (f->badge < 0 && icon) ui_put_c(icon, x - 0.12f * u, y, 0.4f * u, 0.4f * u);
        cm_plus(x + extra, y, h, f->value);
    }
}

#endif
