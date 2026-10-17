/*
 * The party games' shared screen (The Spy, Werewolves, Draw and Guess): talk games for a group round one device. No
 * bots. Up to 12 people, more than the shell's six seats, so the people are the game's own: a numbered chip each
 * (the house's six colours, then again), and the shell sees two seats only, which take turns holding the device.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The device's holder: the shell's hand-off cover shows the token of the seat it passes to, so each pass flips the
 * seat and names who gets it (pt_pass): a person (their chip), everyone (a group of heads: the device in the middle
 * of the table) or the game's own (a werewolf at night). The seat not holding the device paints nothing. At the end
 * the game paints the result into the two tokens (the winning side, the losing one).
 */
#ifndef PARTY_H
#define PARTY_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "shell.h"
#include "text.h"

#define PT_MAXP 12
enum { PT_NONE = -100, PT_ALL = -1 };          /* holders besides a person 0..: nobody, everyone */

/* ---- who holds the device ---------------------------------------------------------------------------------------- */
typedef struct { int who[2], slot; } pt_seat;

static inline void pt_seat_reset(pt_seat *s, int who)
{
    s->slot = 0;
    s->who[0] = who;
    s->who[1] = PT_NONE;
    ui_tokens_changed();
}

/* the device passes to `who`: the other seat takes it (the cover shows its token) */
static inline void pt_pass(pt_seat *s, int who)
{
    s->slot ^= 1;
    s->who[s->slot] = who;
    s->who[!s->slot] = PT_NONE;
    ui_tokens_changed();
    ui_handoff(s->slot);
}

/* ---- painting into pictures -------------------------------------------------------------------------------------- */
/* a line of text centred on (cx, cy) in a picture (w x h), bold, px high, shrunk to maxw; returns its width */
static inline int pt_text(uint8_t *rgba, int w, int h, const char *s, float px, int bold, const uint8_t rgb[3],
                          float cx, float cy, float maxw)
{
    int tw = text_line(s, px, bold, rgb, NULL, 0, 0, 0, 0);
    if (tw > maxw && tw > 0) {
        px = fmaxf(5, px * maxw / tw);
        tw = text_line(s, px, bold, rgb, NULL, 0, 0, 0, 0);
    }
    int W = tw + 4, H = (int)ceilf(px * 1.55f);
    uint8_t *t = calloc((size_t)W * H, 4);
    if (!t) return tw;
    text_line(s, px, bold, rgb, t, W, H, 2, (int)((H - 1.25f * px) / 2));
    art_blend(rgba, w, h, t, W, H, (int)lroundf(cx - W / 2.0f), (int)lroundf(cy - H / 2.0f));
    free(t);
    return tw;
}

/* a person's chip: the house's disc in their colour, their number on it */
static inline void pt_chip(int person, int size, uint8_t *rgba)
{
    int c = person % ART_PLAYERS;
    art_counter(COUNTER_DISC, c, size, rgba);
    char s[12];
    snprintf(s, sizeof s, "%d", person + 1);
    pt_text(rgba, size, size, s, size * (person >= 9 ? 0.4f : 0.46f), 1, art_player_dark(c) ? ART_PALE : ART_INK,
            size / 2.0f, size / 2.0f, size * 0.7f);
}

/* everyone: three heads and shoulders on an ivory disc (the device in the middle of the table) */
static inline void pt_group(int size, uint8_t *rgba)
{
    static const uint8_t ivory[3] = {242, 230, 204}, rim[3] = {150, 118, 70};
    uint8_t *d = calloc((size_t)size * size, 4);
    if (!d) return;
    art_disc(size, rim, 1, 0, d);
    art_blend(rgba, size, size, d, size, size, 0, 0);
    int in = (int)(size * 0.9f);
    memset(d, 0, (size_t)size * size * 4);
    art_disc(in, ivory, 1, 0, d);
    art_blend(rgba, size, size, d, in, in, (size - in) / 2, (size - in) / 2);
    static const float X[3] = {0.31f, 0.69f, 0.5f}, Y[3] = {0.42f, 0.42f, 0.5f}, K[3] = {0.4f, 0.4f, 0.46f};
    static const uint8_t ink[3][3] = {{120, 70, 52}, {58, 92, 120}, {84, 56, 36}};
    for (int i = 0; i < 3; i++) {
        art_fig *f = art_fig_new();
        float k = K[i], x = X[i], y = Y[i];
        art_ellipse(f, ART_PART, x, y - 0.06f * k / 0.4f, 0.13f * k / 0.4f, 0.13f * k / 0.4f);
        art_arc(f, ART_PART, x, y + 0.3f * k / 0.4f, 0, 0.24f * k / 0.4f, 0, 180);
        art_style st = art_flat(ink[i], ivory, 0.02f, 0.25f);
        art_fig_blend(rgba, size, size, f, &st, 0, 0, size, 1);
        art_fig_free(f);
    }
    free(d);
}

/* a word card: ivory, rounded, a thin walnut rim */
static inline void pt_card(int w, int h, uint8_t *rgba)
{
    const uint8_t rim[4] = {120, 84, 50, 255}, ivory[4] = {248, 240, 222, 255};
    float r = fminf(w, h) * 0.08f;
    art_roundrect(w, h, r, rim, rgba);
    int m = (int)fmaxf(2, fminf(w, h) * 0.025f);
    uint8_t *t = calloc((size_t)w * h, 4);
    if (!t) return;
    art_roundrect(w - 2 * m, h - 2 * m, r - m, ivory, t);
    art_blend(rgba, w, h, t, w - 2 * m, h - 2 * m, m, m);
    free(t);
}

/* ---- on the screen ----------------------------------------------------------------------------------------------- */
/* chips kept: person p at size d (a dozen sizes) */
static inline SDL_Texture *pt_chip_tex(int person, int d)
{
    enum { SIZES = 6 };
    static struct { int d; SDL_Texture *t[PT_MAXP]; } c[SIZES];
    static int next;
    if (person < 0 || person >= PT_MAXP || d < 2) return NULL;
    int k = -1;
    for (int i = 0; i < SIZES; i++)
        if (c[i].d == d) k = i;
    if (k < 0) {
        k = next;
        next = (next + 1) % SIZES;
        for (int i = 0; i < PT_MAXP; i++) ui_free(&c[k].t[i]);
        c[k].d = d;
    }
    if (!c[k].t[person]) {
        uint8_t *b = calloc((size_t)d * d, 4);
        if (!b) return NULL;
        pt_chip(person, d, b);
        c[k].t[person] = ui_tex(d, d, b);
        free(b);
    }
    return c[k].t[person];
}

/* n chips in the box: as big as they go (up to most), in rows centred; d their size, (x, y) each centre */
static inline float pt_grid(box b, int n, float most, float *x, float *y)
{
    int best = 1;
    float bd = 0;
    for (int cols = 1; cols <= n; cols++) {
        int rows = (n + cols - 1) / cols;
        float d = fminf(b.w / (cols * 1.25f), b.h / (rows * 1.25f));
        if (d > bd + 0.5f) { bd = d; best = cols; }
    }
    float d = floorf(fminf(bd, most));
    int cols = best, rows = (n + cols - 1) / cols;
    float pitch = d * 1.25f, y0 = b.y + (b.h - rows * pitch) / 2 + pitch / 2;
    for (int i = 0; i < n; i++) {
        int r = i / cols, inrow = r < rows - 1 ? cols : n - r * cols;
        float x0 = b.x + (b.w - inrow * pitch) / 2 + pitch / 2;
        x[i] = floorf(x0 + (i % cols) * pitch);
        y[i] = floorf(y0 + r * pitch);
    }
    return d;
}

static inline int pt_grid_hit(int n, const float *x, const float *y, float d, float px, float py)
{
    for (int i = 0; i < n; i++)
        if (hypotf(px - x[i], py - y[i]) < d * 0.6f) return i;
    return -1;
}

/* a clock: minutes, two dots, the seconds in two digits; centred on (cx, cy), h high, in rgb */
static inline void pt_clock(float cx, float cy, float h, int secs, const uint8_t rgb[3])
{
    if (secs < 0) secs = 0;
    int m = secs / 60, s = secs % 60;
    float wm = ui_number_w(m, h), wd = ui_number_w(0, h), gap = 0.32f * h, w = wm + gap + 2 * wd;
    float x = cx - w / 2;
    ui_number_ex(x + wm / 2, cy, h, m, rgb, 255, UI_NUM_TIGHT);
    float dx = x + wm + gap / 2, r = fmaxf(1.5f, 0.07f * h);
    ui_fill(dx - r, cy - 0.2f * h - r, 2 * r, 2 * r, rgb[0], rgb[1], rgb[2], 255);
    ui_fill(dx - r, cy + 0.2f * h - r, 2 * r, 2 * r, rgb[0], rgb[1], rgb[2], 255);
    ui_number_ex(x + wm + gap + wd / 2, cy, h, s / 10, rgb, 255, UI_NUM_TIGHT);
    ui_number_ex(x + wm + gap + 1.5f * wd, cy, h, s % 10, rgb, 255, UI_NUM_TIGHT);
}

/* a clock that runs while its screen is drawn: each frame adds the time since the last, at most a second (the app
 * away, the rules page open: it waits; the picture mode's clock jumps: a second) */
typedef struct { int on, ms; Uint32 last; } pt_timer;

static inline void pt_timer_reset(pt_timer *t) { t->on = 0; t->ms = 0; }

static inline int pt_timer_tick(pt_timer *t)    /* the ms run so far */
{
    Uint32 now = ui_ms();
    if (!t->on) { t->on = 1; t->ms = 0; }
    else {
        Sint64 d = (Sint64)now - (Sint64)t->last;
        t->ms += d < 0 ? 0 : d > 1000 ? 1000 : (int)d;
    }
    t->last = now;
    ui_animate();
    return t->ms;
}

/* a thin bar of time left (a part 0..1), gold, red near its end */
static inline void pt_timebar(float x, float y, float w, float h, float left)
{
    left = clampf(left, 0, 1);
    ui_fill(x, y, w, h, 0, 0, 0, 70);
    if (left < 0.2f) ui_fill(x, y, w * left, h, 214, 76, 56, 255);
    else ui_fill(x, y, w * left, h, 234, 190, 80, 255);
}

/* ---- a card seen alone (a place, a role): big in the table, the pass button under it (beside it on a wide table) - */
#define PT_FLIP_MS 420
typedef struct { float cw, cx, cy, bx, by, bd; int key; Uint32 t0; } pt_look;

static inline void pt_look_layout(pt_look *l, box b, float aspect)
{
    float u = ui.unit;
    l->bd = floorf(1.15f * u);
    if (b.w > b.h * 1.15f) {
        l->cw = floorf(fminf(b.h * 0.86f / aspect, b.w * 0.4f));
        l->cx = b.x + b.w / 2 - 0.8f * u;
        l->cy = b.y + b.h / 2;
        l->bx = l->cx + l->cw / 2 + 1.4f * u;
        l->by = l->cy;
    } else {
        l->cw = floorf(fminf(b.w * 0.74f, (b.h - 2.6f * u) / aspect));
        l->cx = b.x + b.w / 2;
        l->cy = b.y + (b.h - 2.2f * u) / 2;
        l->bx = l->cx;
        l->by = l->cy + l->cw * aspect / 2 + 1.05f * u;
    }
    l->key = -1;
}

/* the card: face down under the cover (mine 0), turning over once its person sees it (a new key), then face up with
 * the pass button lit */
static inline void pt_look_draw(pt_look *l, int deck, int card, int key, int mine)
{
    if (!mine) {
        cards_draw(deck, card, l->cx, l->cy, l->cw, 0, CD_DOWN);
        return;
    }
    if (l->key != key) { l->key = key; l->t0 = ui_ms(); }
    float t = (ui_ms() - l->t0) / (float)PT_FLIP_MS;
    if (t < 1) {
        ui_animate();
        cards_draw_flip(deck, card, l->cx, l->cy, l->cw, 0, ui_ease(clampf(t, 0, 1)), 0);
    } else cards_draw(deck, card, l->cx, l->cy, l->cw, 0, 0);
    ui_button(l->bx, l->by, l->bd, ICON_DEVICE, t < 1 ? UI_DIM : UI_LIT);
}

static inline int pt_look_busy(const pt_look *l) { return l->key >= 0 && ui_ms() - l->t0 < PT_FLIP_MS; }
static inline int pt_look_hit(const pt_look *l, float x, float y) { return ui_button_hit(l->bx, l->by, l->bd, x, y); }

/* a chip on the table with its shadow, faded */
static inline void pt_chip_at(int person, float cx, float cy, float d, float alpha)
{
    SDL_Texture *t = pt_chip_tex(person, (int)d);
    if (!t) return;
    ui_shadow_flat(cx, cy, d, d, 0.8f * alpha);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha));
    ui_put_c(t, cx, cy, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

/* the table split for the public screens: a strip at the top (a picture, the clock), the middle (the chips, a list),
 * a strip at the bottom (the buttons); on a wide table the top and the bottom are a column at the left */
typedef struct { box top, mid, bot; int wide; float bd, by, bx[3]; } pt_split;

static inline void pt_split_layout(pt_split *s, box b)
{
    float u = ui.unit;
    s->wide = b.w > b.h * 1.15f;
    s->bd = floorf(1.15f * u);
    if (s->wide) {
        float cw = fminf(4.2f * u, b.w * 0.34f);
        s->top = (box){b.x, b.y, cw, b.h * 0.5f};
        s->bot = (box){b.x, b.y + b.h * 0.5f, cw, b.h * 0.5f};
        s->mid = (box){b.x + cw, b.y, b.w - cw, b.h};
        s->by = b.y + b.h * 0.72f;
        float step = fminf(1.35f * u, cw / 3.1f);
        for (int i = 0; i < 3; i++) s->bx[i] = b.x + cw / 2 + (i - 1) * step;
    } else {
        float th = fminf(2.4f * u, b.h * 0.2f), bh = fminf(1.9f * u, b.h * 0.16f);
        s->top = (box){b.x, b.y, b.w, th};
        s->bot = (box){b.x, b.y + b.h - bh, b.w, bh};
        s->mid = (box){b.x, b.y + th, b.w, b.h - th - bh};
        s->by = b.y + b.h - bh / 2;
        for (int i = 0; i < 3; i++) s->bx[i] = b.x + b.w / 2 + (i - 1) * 2.0f * u;
    }
}

/* the language now (a change: the game's word pictures again) */
static inline int pt_lang_changed(char *seen, int size)
{
    const char *l = text_lang();
    if (l && strcmp(seen, l)) {
        snprintf(seen, (size_t)size, "%s", l);
        return 1;
    }
    return 0;
}

/* the picker's card for n people: n chips in a ring, the number big on an ivory disc in the middle */
static inline void pt_count_thumb(int n, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    int cd = (int)(size * (n > 8 ? 0.17f : 0.2f));
    uint8_t *c = calloc((size_t)cd * cd, 4);
    for (int i = 0; i < n && c; i++) {
        memset(c, 0, (size_t)cd * cd * 4);
        art_counter(COUNTER_DISC, i % ART_PLAYERS, cd, c);
        float a = -ART_PI / 2 + 2 * ART_PI * i / n, R = size * 0.37f;
        art_blend(rgba, size, size, c, cd, cd, (int)lroundf(size / 2 + R * cosf(a) - cd / 2.0f),
                  (int)lroundf(size / 2 + R * sinf(a) - cd / 2.0f));
    }
    free(c);
    int d = (int)(size * 0.46f);               /* the number on an ivory chip in the middle */
    uint8_t *g = calloc((size_t)d * d, 4);
    if (!g) return;
    art_counter(COUNTER_DISC, 0, d, g);
    char s[12];
    snprintf(s, sizeof s, "%d", n);
    pt_text(g, d, d, s, d * 0.55f, 1, ART_INK, d / 2.0f, d / 2.0f, d * 0.8f);
    art_blend(rgba, size, size, g, d, d, (size - d) / 2, (size - d) / 2);
    free(g);
}

#endif
