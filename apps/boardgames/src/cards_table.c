/*
 * RetroStone Board's card table (cards_table.h): the card games' table, written once on top of the card kit.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards_table.h"

#define PI_F 3.14159265f

/* ---- moving cards ------------------------------------------------------------------------------------------------ */
void ct_calm(ct_moves *mv)
{
    mv->n = 0;
    mv->quiet = 0;
}

int ct_slide(ct_moves *mv, int deck, int card, float x0, float y0, float w0, float a0, int up0, float x1, float y1,
             float w1, float a1, int up1, int ms, int delay, int tag, int a, int b, int sound)
{
    if (mv->n >= CT_MOVES) return -1;
    ct_move *m = &mv->m[mv->n++];
    cards_slide_start(&m->s, deck, card, x0, y0, w0, a0, up0, x1, y1, w1, a1, up1, ms, delay);
    m->tag = tag;
    m->a = a;
    m->b = b;
    m->after = -1;
    m->show = 0;
    m->end = m->s.t0 + (Uint32)(delay + ms + mv->rest);
    if (sound >= 0) ui_sound_later(sound, delay + ms - 40 > 0 ? delay + ms - 40 : 0);
    return mv->n - 1;
}

void ct_after(ct_moves *mv, int i, int first)
{
    if (i >= 0 && first >= 0) mv->m[i].after = first;
}

void ct_show(ct_moves *mv, int i, int ms)
{
    if (i >= 0) mv->m[i].show = ms > 0 ? mv->m[i].s.t0 + (Uint32)ms : 0;
}

void ct_rest(ct_moves *mv, int i, int ms)
{
    if (i >= 0) mv->m[i].end = mv->m[i].s.t0 + (Uint32)(mv->m[i].s.delay + mv->m[i].s.ms + ms);
}

void ct_hop(ct_moves *mv, int i, float hop, float reach)
{
    if (i >= 0) mv->m[i].s.hop = hop, mv->m[i].s.reach = reach;
}

void ct_until(ct_moves *mv, Uint32 t)
{
    if (t > mv->quiet) mv->quiet = t;
}

int ct_moving(const ct_moves *mv)
{
    for (int i = 0; i < mv->n; i++)
        if (cards_slide_busy(&mv->m[i].s)) return 1;
    return 0;
}

int ct_busy(const ct_moves *mv)
{
    Uint32 now = ui_ms();
    for (int i = 0; i < mv->n; i++)
        if (now < mv->m[i].end || cards_slide_busy(&mv->m[i].s)) return 1;
    return mv->quiet && now < mv->quiet;
}

Uint32 ct_quiet(const ct_moves *mv)
{
    Uint32 t = mv->quiet;
    for (int i = 0; i < mv->n; i++)
        if (mv->m[i].end > t) t = mv->m[i].end;
    return t;
}

int ct_hides(const ct_moves *mv, int tag, int a, int b)
{
    int n = 0;
    for (int i = 0; i < mv->n; i++) {
        const ct_move *m = &mv->m[i];
        if (m->tag == tag && (a < 0 || m->a == a) && (b < 0 || m->b == b) && cards_slide_busy(&m->s)) n++;
    }
    return n;
}

void ct_draw(ct_moves *mv)
{
    Uint32 now = ui_ms();
    for (int i = 0; i < mv->n; i++) {
        ct_move *m = &mv->m[i];
        if ((m->show && m->s.on && now < m->show) || (m->after >= 0 && cards_slide_busy(&mv->m[m->after].s))) {
            ui_animate();
            continue;
        }
        cards_slide_draw(&m->s);
    }
}

/* ---- small pictures, kept ---------------------------------------------------------------------------------------- */
typedef void (*user_painter)(int kind, int size, uint8_t *rgba);
typedef void (*painter)(const int *k, int w, int h, uint8_t *rgba, user_painter user);

#define PICS 192
static struct { painter paint; user_painter user; int k[4], w, h; unsigned used; SDL_Texture *t; } pics[PICS];
static unsigned clock_;

/* the picture painted by `paint` with the key k0..k3, w x h: made once, kept (the least used lately goes) */
static SDL_Texture *pic(painter paint, user_painter user, int k0, int k1, int k2, int k3, int w, int h)
{
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    int k[4] = {k0, k1, k2, k3}, slot = -1;
    clock_++;
    for (int i = 0; i < PICS; i++) {
        if (!pics[i].t) {
            if (slot < 0 || pics[slot].t) slot = i;
            continue;
        }
        if (pics[i].paint == paint && pics[i].user == user && pics[i].w == w && pics[i].h == h &&
            !memcmp(pics[i].k, k, sizeof k)) {
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
    paint(k, w, h, b, user);
    pics[slot].t = ui_tex(w, h, b);
    pics[slot].paint = paint;
    pics[slot].user = user;
    memcpy(pics[slot].k, k, sizeof k);
    pics[slot].w = w;
    pics[slot].h = h;
    pics[slot].used = clock_;
    return pics[slot].t;
}

static void paint_token(const int *k, int w, int h, uint8_t *rgba, user_painter u)
{
    (void)h, (void)u;
    cards_token(k[0], w, rgba);
}

static void paint_suit(const int *k, int w, int h, uint8_t *rgba, user_painter u)
{
    (void)h, (void)u;
    cards_suit_icon(k[0], k[1], w, rgba);
}

static void paint_disc(const int *k, int w, int h, uint8_t *rgba, user_painter u)
{
    const uint8_t rgb[3] = {(uint8_t)k[0], (uint8_t)k[1], (uint8_t)k[2]};
    (void)h, (void)u;
    art_disc(w, rgb, k[3] / 100.0f, 0, rgba);
}

static void paint_round(const int *k, int w, int h, uint8_t *rgba, user_painter u)
{
    float radius;
    (void)u;
    memcpy(&radius, &k[1], sizeof radius);
    const uint8_t c[4] = {(uint8_t)(k[0] >> 24), (uint8_t)(k[0] >> 16), (uint8_t)(k[0] >> 8), (uint8_t)k[0]};
    art_roundrect(w, h, radius, c, rgba);
}

static void paint_icon(const int *k, int w, int h, uint8_t *rgba, user_painter u)
{
    (void)h, (void)u;
    art_icon(k[0], w, rgba);
}

static void paint_user(const int *k, int w, int h, uint8_t *rgba, user_painter u)
{
    (void)h;
    u(k[0], w, rgba);
}

SDL_Texture *ct_token(int player, int size)
{
    return pic(paint_token, NULL, player, 0, 0, 0, size < 4 ? 4 : size, size < 4 ? 4 : size);
}

SDL_Texture *ct_suit(int deck, int suit, int size) { return pic(paint_suit, NULL, deck, suit & 3, 0, 0, size, size); }

SDL_Texture *ct_disc(const uint8_t rgb[3], int a0, int size)
{
    return pic(paint_disc, NULL, rgb[0], rgb[1], rgb[2], a0, size, size);
}

SDL_Texture *ct_round(int w, int h, float radius, const uint8_t rgba[4])
{
    int r;
    memcpy(&r, &radius, sizeof r);
    uint32_t c = (uint32_t)rgba[0] << 24 | (uint32_t)rgba[1] << 16 | (uint32_t)rgba[2] << 8 | rgba[3];
    return pic(paint_round, NULL, (int)c, r, 0, 0, w, h);
}

SDL_Texture *ct_icon(int which, int size, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_Texture *t = pic(paint_icon, NULL, which, 0, 0, 0, size < 2 ? 2 : size, size < 2 ? 2 : size);
    SDL_SetTextureColorMod(t, r, g, b);
    return t;
}

SDL_Texture *ct_picture(void (*paint)(int kind, int size, uint8_t *rgba), int kind, int size)
{
    return pic(paint_user, paint, kind, 0, 0, 0, size < 4 ? 4 : size, size < 4 ? 4 : size);
}

/* ---- painting a game's own pictures ------------------------------------------------------------------------------ */
void ct_paint_fig(uint8_t *rgba, int s, art_fig *f, const art_style *st)
{
    uint8_t *b = calloc((size_t)s * s, 4);
    if (b) {
        art_render(f, st, s, s, (float)s, 0, 0, b);
        art_blend(rgba, s, s, b, s, s, 0, 0);
        free(b);
    }
    art_fig_free(f);
}

void ct_paint_blend(uint8_t *rgba, int s, const uint8_t *src, int w, int h, int x, int y, float alpha)
{
    if (alpha >= 1) { art_blend(rgba, s, s, src, w, h, x, y); return; }
    uint8_t *c = malloc((size_t)w * h * 4);
    if (!c) return;
    memcpy(c, src, (size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) c[i * 4 + 3] = (uint8_t)(c[i * 4 + 3] * alpha);
    art_blend(rgba, s, s, c, w, h, x, y);
    free(c);
}

void ct_paint_card(uint8_t *rgba, int s, int deck, int card, int w, int x, int y, float alpha)
{
    int h = (int)(w * cards_aspect(deck));
    uint8_t *b = malloc((size_t)w * h * 4);
    if (!b) return;
    cards_paint(deck, card, w, h, b);
    ct_paint_blend(rgba, s, b, w, h, x, y, alpha);
    free(b);
}

void ct_paint_piece(uint8_t *rgba, int s, int type, const art_style *st, float k, float dy)
{
    int f = (int)(k * s);
    uint8_t *b = calloc((size_t)f * f, 4);
    if (!b) return;
    art_piece_style(type, st, f, b);
    art_blend(rgba, s, s, b, f, f, (s - f) / 2, (s - f) / 2 + (int)(dy * s));
    free(b);
}

void ct_paint_ivory(uint8_t *rgba, int s)
{
    static const uint8_t ivory[3] = {244, 236, 216};
    art_disc(s, ivory, 1, 0, rgba);
}

/* the calls' signs, white on clear, size x size (the marks cut out of the cards: clear holes) */
static void paint_call(int kind, int size, uint8_t *rgba)
{
    art_style st = {{255, 255, 255}, {150, 150, 150}, {150, 150, 150}, {120, 120, 120}, 0, 0};
    art_fig *f = art_fig_new();
    memset(rgba, 0, (size_t)size * size * 4);
    switch (kind) {
    case CT_CROSS: {
        const float a[] = {0.2f, 0.28f, 0.28f, 0.2f, 0.8f, 0.72f, 0.72f, 0.8f};
        const float b[] = {0.72f, 0.2f, 0.8f, 0.28f, 0.28f, 0.8f, 0.2f, 0.72f};
        art_poly(f, ART_PART, a, 4);
        art_poly(f, ART_PART, b, 4);
        break;
    }
    case CT_BOLT: {
        const float b[] = {0.58f, 0.06f, 0.24f, 0.56f, 0.47f, 0.56f, 0.38f, 0.94f, 0.78f, 0.4f, 0.54f, 0.4f, 0.68f, 0.06f};
        art_poly(f, ART_PART, b, 7);
        break;
    }
    case CT_CYCLE: {                            /* two arrows, one each way */
        const float r[] = {0.14f, 0.3f, 0.6f, 0.3f, 0.6f, 0.18f, 0.86f, 0.36f, 0.6f, 0.54f, 0.6f, 0.42f, 0.14f, 0.42f};
        const float l[] = {0.86f, 0.58f, 0.4f, 0.58f, 0.4f, 0.46f, 0.14f, 0.64f, 0.4f, 0.82f, 0.4f, 0.7f, 0.86f, 0.7f};
        art_poly(f, ART_PART, r, 7);
        art_poly(f, ART_PART, l, 7);
        break;
    }
    case CT_HIGH:
    case CT_LOW: {                              /* a card and an arrow */
        int up = kind == CT_HIGH;
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
    case CT_PAIR:                               /* two cards side by side, alike */
        for (int k = 0; k < 2; k++) {
            float x = 0.14f + 0.38f * k;
            art_rect(f, ART_PART, x, 0.18f, x + 0.34f, 0.82f);
            art_ellipse(f, ART_MARK, x + 0.17f, 0.5f, 0.09f, 0.09f);
        }
        st.mark[0] = st.mark[1] = st.mark[2] = 0;
        break;
    case CT_FLOWER:                             /* five petals round a heart */
        for (int k = 0; k < 5; k++) {
            float a = (-90 + 72.0f * k) * PI_F / 180;
            art_ellipse(f, ART_PART, 0.5f + 0.22f * cosf(a), 0.5f + 0.22f * sinf(a), 0.16f, 0.16f);
        }
        art_ellipse(f, ART_MARK, 0.5f, 0.5f, 0.11f, 0.11f);
        st.mark[0] = 0, st.mark[1] = 0, st.mark[2] = 0;
        break;
    case CT_CARD:                               /* a card's outline */
        art_rect(f, ART_PART, 0.26f, 0.14f, 0.74f, 0.86f);
        art_rect(f, ART_MARK, 0.31f, 0.19f, 0.69f, 0.81f);
        st.mark[0] = st.mark[1] = st.mark[2] = 0;
        break;
    }
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
    if (kind >= CT_HIGH)
        for (int i = 0; i < size * size; i++)   /* the marks cut out: a clear hole */
            if (rgba[4 * i] < 128 && rgba[4 * i + 3]) {
                int k = rgba[4 * i];
                rgba[4 * i + 3] = (uint8_t)(rgba[4 * i + 3] * k / 128);
                rgba[4 * i] = rgba[4 * i + 1] = rgba[4 * i + 2] = 255;
            }
}

/* the kit's own pictures: white figures, a cream moon, the buttons' discs, the calls' signs */
static void paint_kit(const int *k, int s, int h, uint8_t *rgba, user_painter u)
{
    static const uint8_t off_c[3] = {54, 72, 64}, on_c[3] = {150, 118, 46}, dark_c[3] = {22, 30, 27},
                         cream[3] = {246, 226, 160}, black[3] = {0, 0, 0};
    int kind = k[0];
    (void)h, (void)u;
    if (kind >= CT_CROSS) {
        paint_call(kind, s, rgba);
        return;
    }
    if (kind >= CT_DISC_OFF) {
        art_disc(s, kind == CT_DISC_ON ? on_c : kind == CT_DISC_OFF ? off_c : dark_c, kind == CT_DISC_DARK ? 0.85f : 1,
                 0, rgba);
        return;
    }
    if (kind == CT_MOON) {                      /* a disc, a smaller one cut out of it (upper right) */
        art_disc(s, cream, 1, 0, rgba);
        int c = (int)(0.8f * s), off = (int)(0.34f * s), up = (int)(0.12f * s);
        uint8_t *cut = malloc((size_t)c * c * 4);
        if (!cut) return;
        art_disc(c, black, 1, 0, cut);
        for (int y = 0; y < c; y++)
            for (int x = 0; x < c; x++) {
                int X = x + off, Y = y - up;
                if (X < 0 || Y < 0 || X >= s || Y >= s) continue;
                uint8_t *p = rgba + ((size_t)Y * s + X) * 4;
                p[3] = (uint8_t)(p[3] * (255 - cut[((size_t)y * c + x) * 4 + 3]) / 255);
            }
        free(cut);
        return;
    }
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    if (kind == CT_ARROW) {
        const float head[] = {0.50f, 0.22f, 0.86f, 0.50f, 0.50f, 0.78f};
        art_rect(f, ART_PART, 0.14f, 0.42f, 0.56f, 0.58f);
        art_poly(f, ART_PART, head, 3);
    } else if (kind == CT_TICK) {
        const float tick[] = {0.16f, 0.52f, 0.27f, 0.41f, 0.42f, 0.56f, 0.74f, 0.22f, 0.85f, 0.33f, 0.42f, 0.78f};
        art_poly(f, ART_PART, tick, 6);
    } else {
        const float slash[] = {0.70f, 0.08f, 0.80f, 0.13f, 0.30f, 0.92f, 0.20f, 0.87f};
        art_poly(f, ART_PART, slash, 4);
    }
    art_render(f, &st, s, s, (float)s, 0, 0, rgba);
    art_fig_free(f);
}

void ct_pic(int kind, float cx, float cy, float d, double deg, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    int s = (int)(d + 0.5f);
    if (s < 2) s = 2;
    SDL_Texture *t = pic(paint_kit, NULL, kind, 0, 0, 0, s, s);
    if (!t) return;
    SDL_SetTextureColorMod(t, r, g, b);
    SDL_SetTextureAlphaMod(t, a);
    ui_put_rot(t, cx, cy, d, d, deg);
}

void ct_button(float cx, float cy, float d, int on, int p, double deg)
{
    ct_pic(on ? CT_DISC_ON : CT_DISC_OFF, cx, cy, d, 0, 255, 255, 255, 255);
    if (p >= 0) ct_pic(p, cx, cy, 0.66f * d, deg, 255, 255, 255, on ? 255 : 110);
}

int ct_on(float cx, float cy, float d, float x, float y) { return hypotf(x - cx, y - cy) <= 0.56f * d; }

void ct_number(float cx, float cy, float h, int v, int nil)
{
    ui_number(cx, cy, h, v, 1);
    if (nil) ct_pic(CT_SLASH, cx, cy, 1.15f * h, 0, 250, 214, 90, 255);
}

void ct_badge(float cx, float cy, float d, int v)
{
    ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    ui_number(cx, cy, v >= 100 ? 0.36f * d : 0.46f * d, v, 1);
}

void ct_pass(float cx, float cy, float d, int glow)
{
    if (glow) ui_mark_gold(cx, cy, 1.8f * d);
    ct_pic(CT_DISC_OFF, cx, cy, d, 0, 255, 255, 255, 255);
    int s = (int)(0.62f * d);
    ui_put_c(ct_icon(ICON_PASS, s, 255, 255, 255), cx, cy, (float)s, (float)s);
}

void ct_pass_badge(float cx, float cy, float d)
{
    ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    int s = (int)(0.6f * d);
    ui_put_c(ct_icon(ICON_PASS, s, 246, 238, 220), cx, cy, (float)s, (float)s);
}

void ct_pic_paint(int kind, int size, uint8_t *rgba)
{
    const int k[4] = {kind, 0, 0, 0};
    memset(rgba, 0, (size_t)size * size * 4);
    paint_kit(k, size, size, rgba, NULL);
}

void ct_btn_row(ct_btn *b, int n, float cx, float y, float d, float step)
{
    for (int i = 0; i < n; i++) b[i].x = cx + (i - (n - 1) / 2.0f) * step * d, b[i].y = y, b[i].d = d;
}

int ct_btn_hit(const ct_btn *b, int n, float x, float y)
{
    for (int i = 0; i < n; i++)
        if (ct_on(b[i].x, b[i].y, b[i].d, x, y)) return b[i].id;
    return -1;
}

void ct_step(float cx, float cy, float d, int plus, int enabled)
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

void ct_amount(float cx, float cy, float d, int n, int plus, int live)
{
    if (live) ui_mark_gold(cx, cy, 1.8f * d);
    ct_button(cx, cy, d, 1, -1, 0);
    if (plus) ct_plus(cx, cy, (n >= 10 ? 0.32f : 0.4f) * d, n);
    else ui_number(cx, cy, (n >= 10 ? 0.38f : 0.46f) * d, n, 1);
}

void ct_badge_plus(float cx, float cy, float d, int v)
{
    ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    ct_plus(cx, cy, (v >= 10 ? 0.34f : 0.42f) * d, v);
}

void ct_badge_pic(int kind, float cx, float cy, float d, float k, Uint8 r, Uint8 g, Uint8 b)
{
    ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    ct_pic(kind, cx, cy, k * d, 0, r, g, b, 255);
}

void ct_badge_icon(int icon, float cx, float cy, float d, Uint8 r, Uint8 g, Uint8 b)
{
    ct_pic(CT_DISC_DARK, cx, cy, d, 0, 255, 255, 255, 255);
    int s = (int)(0.62f * d);
    ui_put_c(ct_icon(icon, s < 2 ? 2 : s, r, g, b), cx, cy, (float)s, (float)s);
}

void ct_badge_yes(float cx, float cy, float d) { ct_badge_pic(CT_TICK, cx, cy, d, 0.68f, 140, 220, 150); }
void ct_badge_no(float cx, float cy, float d) { ct_badge_pic(CT_CROSS, cx, cy, d, 0.7f, 236, 110, 90); }

void ct_red_disc(float cx, float cy, float d) { ui_put_c(ct_disc(CT_RED, 100, (int)(d + 0.5f)), cx, cy, d, d); }

void ct_bolt(float cx, float cy, float d)
{
    ct_red_disc(cx, cy, d);
    ct_pic(CT_BOLT, cx, cy, 0.72f * d, 0, CT_GOLD[0], CT_GOLD[1], CT_GOLD[2], 255);
}

void ct_choice(SDL_Texture *p, float cx, float cy, float d, int chosen, int enabled)
{
    if (chosen) ui_mark_gold(cx, cy, 1.9f * d);
    if (!p) return;
    SDL_SetTextureAlphaMod(p, enabled ? 255 : 90);
    ui_put_c(p, cx, cy - (chosen ? 0.08f * d : 0), d, d);
    SDL_SetTextureAlphaMod(p, 255);
}

float ct_number_w(int v, float h, int plus)
{
    int digits = 1;
    for (int a = abs(v); a >= 10; a /= 10) digits++;
    return (0.74f * (digits - 1) + 0.62f + (v < 0 || (plus && v > 0) ? 0.56f : 0)) * h;
}

void ct_rule(float x, float y, float w) { ui_fill(x, y, w, fmaxf(1, 0.02f * ui.unit), 246, 238, 220, 40); }

void ct_panel(float x, float y, float w, float h, int alpha)
{
    const uint8_t c[4] = {28, 34, 32, (uint8_t)alpha};
    ui_put(ct_round((int)w, (int)h, 0.4f * ui.unit, c), x, y, w, h);
}

/* the plus snapped to whole pixels (ui_number_signed's plus can break into three pieces at some heights, 30 px: its
 * bars are rounded apart) */
void ct_plus(float cx, float cy, float h, int value)
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

/* ---- points rising ----------------------------------------------------------------------------------------------- */
void ct_float(ct_floats *fl, ct_moves *mv, float x, float y, int value, int badge, Uint32 t0)
{
    if (fl->n >= CT_FLOATS) return;
    fl->f[fl->n].x = x, fl->f[fl->n].y = y, fl->f[fl->n].value = value, fl->f[fl->n].badge = badge;
    fl->f[fl->n++].t0 = t0;
    ct_until(mv, t0 + CT_FLOAT_MS);
}

void ct_floats_draw(const ct_floats *fl, SDL_Texture *pill, SDL_Texture *icon)
{
    Uint32 now = ui_ms();
    float u = ui.unit;
    for (int i = 0; i < fl->n; i++) {
        float fx = fl->f[i].x, fy = fl->f[i].y;
        int value = fl->f[i].value, badge = fl->f[i].badge;
        Uint32 t0 = fl->f[i].t0;
        if (now < t0 || now >= t0 + CT_FLOAT_MS) continue;
        ui_animate();
        float k = (float)(now - t0) / CT_FLOAT_MS, rise = 0.9f * u * (1 - (1 - k) * (1 - k));
        float x = floorf(fx), y = floorf(fy - rise), h = floorf(0.4f * u), extra = badge ? 0.75f * u : 0;
        float a = k < 0.75f ? 1 : (1 - k) / 0.25f;
        int digits = value >= 10 ? 2 : 1;
        SDL_SetTextureAlphaMod(pill, (Uint8)(210 * a));
        ui_put_c(pill, x + extra / 2, y, (0.9f + 0.3f * digits) * u * 0.62f + extra, 0.58f * u);
        SDL_SetTextureAlphaMod(pill, 255);
        if (badge > 0) ui_number(x - 0.12f * u, y, floorf(0.32f * u), badge, 1);
        else if (badge < 0 && icon) ui_put_c(icon, x - 0.12f * u, y, 0.4f * u, 0.4f * u);
        ct_plus(x + extra, y, h, value);
    }
}

/* ---- the trick table --------------------------------------------------------------------------------------------- */
float ct_angle(int players, int me, int pl)
{
    float a = 90 + (float)((pl - me + players) % players) * 360 / players;
    return a >= 360 ? a - 360 : a;
}

void ct_layout(ct_table *T, box b, int players, int me, int max_hand)
{
    float u = ui.unit;
    T->b = b;
    T->u = u;
    T->players = players;
    T->me = me < 0 || me >= players ? 0 : me;
    T->asp = cards_aspect(T->deck);
    int portrait = b.w <= b.h * 1.05f;
    T->wh = floorf(fminf(b.w * (portrait ? 0.2f : 0.12f), b.h * (portrait ? 0.15f : 0.165f)));
    T->hw = fminf(b.w * 0.97f, T->wh * (1 + 0.62f * (max_hand - 1)));
    T->arc = fminf(16, 1.1f * max_hand);
    float hh = T->wh * T->asp;
    T->hy = floorf(b.y + b.h - hh / 2 - 0.1f * T->wh - (T->hw / 2) * tanf(T->arc / 2 * PI_F / 180) * 0.5f);
    T->hx = b.x + b.w / 2;
    T->hand_top = T->hy - hh / 2 - 0.22f * T->wh;
    /* the trick's area: the seats on an ellipse in it, the cards on a smaller one */
    float ay = b.y, ah = T->hand_top - ay, aw = b.w;
    T->tok = floorf(fminf(0.62f * u, ah * 0.11f));
    float m = 0.75f * T->tok;
    T->cx = b.x + aw / 2;
    T->cy = ay + ah / 2;
    float Rx = aw / 2 - m, Ry = ah / 2 - m;
    T->wt = floorf(fminf(0.92f * T->wh, fminf((ah - 2.4f * T->tok) / (2 * 0.62f * T->asp + T->asp + 0.1f),
                                             (aw - 2.4f * T->tok) / 3.5f)));
    if (players >= 5) T->wt = floorf(0.88f * T->wt);   /* five or six cards round the middle */
    T->ry = fminf((players >= 5 ? 1.0f : 0.62f) * T->wt * T->asp, Ry - T->tok - 0.5f * T->wt * T->asp);
    float wide = fmaxf(1, 0.85f * aw / ah);      /* a wide area: the cards further apart */
    T->rx = fminf((players >= 5 ? 1.4f : 1.2f) * wide * T->wt, Rx - T->tok - 0.6f * T->wt);
    for (int pl = 0; pl < players; pl++) {
        float a = ct_angle(players, T->me, pl), r = a * PI_F / 180, c = cosf(r), s = sinf(r);
        T->ang[pl] = a;
        T->sx[pl] = T->cx + c * Rx;
        T->sy[pl] = T->cy + s * Ry;
        T->tx[pl] = T->cx + c * T->rx;
        T->ty[pl] = T->cy + s * T->ry;
        float d = a - 270;                      /* turned a little towards its player */
        if (d < -180) d += 360;
        T->tdeg[pl] = pl == T->me ? 0 : d * 0.07f;
    }
    /* the trick pile: the lower right corner of the area */
    T->pw = floorf(0.5f * T->wt);
    T->px = b.x + b.w - 0.5f * T->pw - 0.35f * u;
    T->py = T->hand_top - 0.5f * T->pw * T->asp - 0.1f * u;
}

void ct_fan(const ct_table *T, int n, cards_spot *s)
{
    float w = fminf(T->hw, T->wh * (1 + 0.95f * (n - 1)));
    cards_fan(T->hx, T->hy, w, T->wh, T->asp, n, n > 1 ? T->arc * (n - 1) / (n + 2.0f) : 0, s);
}

void ct_seat_spot(const ct_table *T, int pl, int k, float *x, float *y)
{
    float a = T->ang[pl];
    float side = a > 300 || a < 100 ? -1 : 1;   /* the right-hand seats: to their left */
    if (a > 240 && a < 300) side = 1;
    *x = T->sx[pl] + side * (0.95f + 0.85f * k) * T->tok;
    *y = T->sy[pl];
}

void ct_draw_seats(const ct_table *T, int turn)
{
    int size = (int)T->tok < 4 ? 4 : (int)T->tok;
    for (int pl = 0; pl < T->players; pl++) {
        if (pl == turn) ui_mark_gold(T->sx[pl], T->sy[pl], 2.1f * T->tok);
        ui_put_c(ct_token(pl, size), T->sx[pl], T->sy[pl], T->tok, T->tok);
    }
}

static int player_of(const ct_table *T, const uint8_t *who, int leader, int i)
{
    return who ? who[i] : (leader + i) % T->players;
}

void ct_draw_trick(const ct_table *T, const ct_moves *mv, const uint8_t *cards, const uint8_t *who, int n, int leader)
{
    for (int i = 0; i < n; i++) {
        int pl = player_of(T, who, leader, i);
        if (ct_hides(mv, CT_TRICK, pl, -1)) continue;
        cards_draw(T->deck, cards[i], T->tx[pl], T->ty[pl], T->wt, T->tdeg[pl], 0);
    }
}

void ct_draw_last(const ct_table *T, const uint8_t *cards, const uint8_t *who, int n, int leader, int winner)
{
    ui_fill(T->b.x, T->b.y, T->b.w, T->hand_top - T->b.y, 0, 0, 0, 90);
    ui_mark_gold(T->sx[winner], T->sy[winner], 2.6f * T->tok);
    for (int i = 0; i < n; i++) {
        int pl = player_of(T, who, leader, i);
        cards_draw(T->deck, cards[i], T->tx[pl], T->ty[pl], T->wt, T->tdeg[pl], pl == winner ? CD_RAISED : 0);
    }
}

void ct_gather(ct_moves *mv, const ct_table *T, const uint8_t *cards, const uint8_t *who, int n, int leader, int winner,
               int look_ms, int last_lands)
{
    for (int i = 0; i < n; i++) {
        int pl = player_of(T, who, leader, i);
        int k = ct_slide(mv, T->deck, cards[i], T->tx[pl], T->ty[pl], T->wt, T->tdeg[pl], 1, T->sx[winner],
                         T->sy[winner], 0.45f * T->wt, T->tdeg[winner], 1, 360, look_ms + 30 * i, CT_NONE, 0, -1,
                         i == 0 ? SND_MOVE : -1);
        ct_show(mv, k, i == n - 1 ? last_lands : 0);
        ct_rest(mv, k, 200);
    }
}

void ct_draw_pile(const ct_table *T, int tricks)
{
    if (tricks > 0) cards_pile(T->deck, T->px, T->py, T->pw, tricks, CARD_NONE, 0);
}

int ct_on_pile(const ct_table *T, int tricks, float x, float y)
{
    return tricks > 0 && cards_in(T->px, T->py, T->pw * 1.5f, T->asp, x, y);
}

void ct_talon(int deck, float x, float y, float tx, float ty, float w, int n, int trump, int suit, int count)
{
    if (n > 0 && trump != CARD_NONE) cards_draw(deck, trump, tx, ty, w, 90, 0);
    if (n > 1) cards_pile(deck, x, y, w, n - 1 > 6 ? 6 : n - 1, CARD_NONE, 0);
    if (n > 1 && count) ui_number(x, y, 0.34f * w, n, 1);
    if (n > 0) return;
    float d = 0.75f * w;
    ui_put_c(ct_suit(deck, suit, (int)d), tx, ty, d, d);
}

void ct_summary(const ct_table *T, int rows, float *x0, float *pw, float *y_row, float *rh)
{
    ct_summary_in(T->b, rows, x0, pw, y_row, rh);
}

void ct_summary_in(box b, int rows, float *x0, float *pw, float *y_row, float *rh)
{
    float u = ui.unit;
    *rh = fminf(1.15f * u, b.h * 0.8f / rows);
    *pw = fminf(b.w * 0.94f, 8.0f * u);
    float ph = *rh * rows + 0.5f * u, y0 = b.y + (b.h - ph) / 2;
    if (ui_over()) {                            /* the game is over: the summary inside the shell's end card */
        box r = ui_end_room(*pw, ph);
        *rh = fminf(*rh, (r.h - 0.5f * u) / rows);
        *pw = r.w;
        *x0 = r.x;
        *y_row = r.y + (r.h - *rh * rows) / 2 + *rh / 2;
        return;
    }
    *x0 = b.x + (b.w - *pw) / 2;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 80);
    ct_panel(*x0, y0, *pw, ph, 255);
    *y_row = y0 + 0.25f * u + *rh / 2;
}

void ct_dots(float cx, float y, float d, int n, int per, const uint8_t *who, int done)
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

void ct_bar(box b, int deck, int cards, int score)
{
    float u = ui.unit, cy = b.y + b.h / 2, h = 0.46f * u, hc = 0.3f * u, cw = 0.26f * u, gap = 0.08f * u, sep = 0.3f * u;
    float need = cw + gap + ct_number_w(cards, hc, 0) + sep + ct_number_w(score, h, 0);
    float k = need > b.w ? b.w / need : 1, x = b.x;   /* a narrow bar (a wide score): all of it smaller */
    if (k >= 0.55f) {                           /* too narrow even so: the score alone */
        h *= k, hc *= k, cw *= k, gap *= k, sep *= k;
        float wc = ct_number_w(cards, hc, 0);
        cards_draw(deck, CARD_NONE, x + cw / 2, cy, cw, 0, CD_DOWN | CD_FLAT);
        x += cw + gap;
        ui_number(x + wc / 2, cy, hc, cards, 1);
        x += wc + sep;
    }
    float w = ct_number_w(score, 1, 0), hs = fminf(h, (b.x + b.w - x) / w * 0.95f);
    if (hs > 0) ui_number_signed(x + w * hs / 2, cy, hs, score, 1, 0);
}

void ct_thumb(int deck, const int *cards, int n, int size, uint8_t *rgba)
{
    float asp = cards_aspect(deck), w = fminf(size * 0.44f, size * 0.8f / asp);
    int cw = (int)w, ch = (int)(w * asp);
    uint8_t *b = malloc((size_t)cw * ch * 4);
    memset(rgba, 0, (size_t)size * size * 4);
    if (!b) return;
    for (int i = 0; i < n; i++) {
        cards_paint(deck, cards[i], cw, ch, b);
        int x = (int)(size * 0.06f + i * (size * 0.88f - cw) / (n > 1 ? n - 1 : 1));
        int y = (int)((size - ch) / 2 + size * 0.05f * (i == n - 1 ? -1 : 1 - i * 0.5f));
        art_blend(rgba, size, size, b, cw, ch, x, y);
    }
    free(b);
}

/* ---- the viewer's hand ------------------------------------------------------------------------------------------- */
void ct_hand_draw(const ct_table *T, const ct_moves *mv, const uint8_t *h, int n, const int *how, const float *lift,
                  int skip)
{
    cards_spot s[CT_HAND];
    if (n > CT_HAND) n = CT_HAND;
    ct_fan(T, n, s);
    for (int i = 0; i < n; i++) {
        if (i == skip || ct_hides(mv, CT_HAND_TAG, i, -1)) continue;
        float y = s[i].y;
        if (lift && lift[i] > 0 && !(how[i] & CD_RAISED)) {
            y -= lift[i] * T->wh;
            ui_mark_gold(s[i].x, y - 0.42f * T->wh * T->asp, 0.8f * T->wh);
        }
        cards_draw(T->deck, h[i], s[i].x, y, T->wh, s[i].deg, how[i]);
    }
}

void ct_hand_backs(const ct_table *T, int n)
{
    cards_spot s[CT_HAND];
    if (n > CT_HAND) n = CT_HAND;
    ct_fan(T, n, s);
    for (int i = 0; i < n; i++) cards_draw(T->deck, CARD_NONE, s[i].x, s[i].y, T->wh, s[i].deg, CD_DOWN);
}

int ct_hand_hit(const ct_table *T, int n, const int *how, const float *lift, float x, float y)
{
    cards_spot s[CT_HAND];
    if (n > CT_HAND) n = CT_HAND;
    ct_fan(T, n, s);
    for (int i = 0; i < n; i++) {
        float up = how[i] & CD_RAISED ? 0.14f : lift && lift[i] > 0 ? lift[i] : 0;
        if (up > 0 && cards_hit(&s[i], 1, T->wh, T->asp, x, y + up * T->wh) >= 0) return i;
    }
    return cards_hit(s, n, T->wh, T->asp, x, y);
}

int ct_hand_at(const ct_table *T, const uint8_t *h, int n, const int *how, const float *lift, float x, float y)
{
    int i = ct_hand_hit(T, n, how, lift, x, y);
    return i < 0 ? CARD_NONE : h[i];
}

int ct_index(const uint8_t *h, int n, int card)
{
    for (int i = 0; i < n; i++)
        if (h[i] == card) return i;
    return -1;
}

void ct_deal(ct_moves *mv, const ct_table *T, const uint8_t *h, int n)
{
    cards_spot s[CT_HAND];
    if (n > CT_HAND) n = CT_HAND;
    ct_fan(T, n, s);
    for (int i = 0; i < n; i++)
        ct_rest(mv, ct_slide(mv, T->deck, h[i], T->cx, T->cy, T->wt, 0, 0, s[i].x, s[i].y, T->wh, s[i].deg, 1,
                             CT_DEAL_MS, 50 * i, CT_HAND_TAG, i, -1, i % 3 == 0 ? SND_MOVE : -1), 0);
}

/* ---- the finger -------------------------------------------------------------------------------------------------- */
void ct_touch_reset(ct_touch *t)
{
    t->card = t->raised = CARD_NONE;
    t->at = t->raised_at = -1;
    t->on = 0;
}

void ct_touch_down(ct_touch *t, int at, int card, float x, float y)
{
    t->card = card;
    t->at = at;
    t->on = 0;
    t->x0 = t->x = x;
    t->y0 = t->y = y;
}

int ct_touch_hand(ct_touch *t, const ct_table *T, const uint8_t *h, int n, const int *how, const float *lift, float x,
                  float y)
{
    int i = ct_hand_hit(T, n, how, lift, x, y);
    if (i >= 0) {
        ct_touch_down(t, i, h[i], x, y);
        return 1;
    }
    if (t->raised != CARD_NONE) ct_lower(t);
    else ui_sound(SND_DENY);
    return 0;
}

void ct_touch_move(ct_touch *t, float x, float y)
{
    if (t->card == CARD_NONE) return;
    t->x = x;
    t->y = y;
    if (!t->on && hypotf(x - t->x0, y - t->y0) > 0.25f * ui.unit) t->on = 1;
}

int ct_touch_up(ct_touch *t, const ct_table *T, float x, float y, int *card)
{
    *card = t->card;
    if (t->card == CARD_NONE) return CT_T_NONE;
    ct_touch_move(t, x, y);
    int r = !t->on ? CT_T_TAP : y < T->hand_top ? CT_T_SEND : CT_T_NONE;
    t->card = CARD_NONE;
    if (r != CT_T_SEND) t->on = 0;              /* sent: kept (the card slides on from the finger) */
    return r;
}

void ct_touch_draw(const ct_touch *t, const ct_table *T)
{
    if (t->card != CARD_NONE && t->on) cards_draw(T->deck, t->card, t->x, t->y, T->wh, 0, CD_RAISED);
}

int ct_tap_play(ct_touch *t, int card, int legal)
{
    if (!legal) { ui_sound(SND_DENY); return 0; }
    if (t->raised == card && t->raised_at == t->at) return 1;
    t->raised = card;
    t->raised_at = t->at;
    ui_sound(SND_TAP);
    return 0;
}

void ct_lower(ct_touch *t)
{
    t->raised = CARD_NONE;
    t->raised_at = -1;
}

int ct_dragged(const ct_touch *t) { return t->on && t->card != CARD_NONE ? t->at : -1; }

/* ---- several cards chosen ---------------------------------------------------------------------------------------- */
int ct_count(uint64_t mask)
{
    int k = 0;
    for (; mask; mask &= mask - 1) k++;
    return k;
}

void ct_chosen_hows(int n, unsigned mask, int glow, int *how)
{
    for (int i = 0; i < n; i++) how[i] = mask >> i & 1 ? CD_RAISED : glow ? CD_GLOW : 0;
}

int ct_toggle(const ct_table *T, int n, const int *how, unsigned *mask, int more, float x, float y)
{
    int i = ct_hand_hit(T, n, how, NULL, x, y);
    if (i < 0) return 0;
    if (!(*mask >> i & 1) && !more) { ui_sound(SND_DENY); return 1; }
    *mask ^= 1u << i;
    ui_sound(SND_TAP);
    return 1;
}

void ct_draw_target(const ct_table *T)
{
    ui_mark_gold(T->tx[T->me], T->ty[T->me], 1.6f * T->wt);
    cards_place(T->deck, T->tx[T->me], T->ty[T->me], T->wt);
}

int ct_on_target(const ct_table *T, float x, float y)
{
    return cards_in(T->tx[T->me], T->ty[T->me], T->wt * 1.2f, T->asp, x, y);
}

void ct_play(ct_moves *mv, const ct_table *T, const ct_touch *f, const uint8_t *h, int n, int card)
{
    int pl = ui_bot_moving();                   /* a bot's card (not the viewer's): from its seat to its place */
    if (pl >= 0 && pl != T->me && pl < T->players) {
        ct_rest(mv, ct_slide(mv, T->deck, card, T->sx[pl], T->sy[pl], 0.45f * T->wt, T->tdeg[pl], 0, T->tx[pl], T->ty[pl],
                             T->wt, T->tdeg[pl], 1, CT_SLIDE_MS, 0, CT_TRICK, pl, -1, SND_PLACE), 200);
        return;
    }
    cards_spot s[CT_HAND];
    if (n > CT_HAND) n = CT_HAND;
    ct_fan(T, n, s);
    int me = T->me, i = f->on ? f->at : f->raised_at;
    if (i < 0 || i >= n || h[i] != card) i = ct_index(h, n, card);
    if (i < 0) i = n;
    float x0 = i < n ? s[i].x : T->hx, y0 = i < n ? s[i].y - 0.14f * T->wh : T->hy, d0 = i < n ? s[i].deg : 0;
    if (f->on) x0 = f->x, y0 = f->y, d0 = 0;
    ct_rest(mv, ct_slide(mv, T->deck, card, x0, y0, T->wh, d0, 1, T->tx[me], T->ty[me], T->wt, T->tdeg[me], 1,
                         CT_SLIDE_MS, 0, CT_TRICK, me, -1, SND_PLACE), 200);
}

/* ---- a trick game's table, ready made ---------------------------------------------------------------------------- */
void ct_game_reset(ct_game *U)
{
    U->show_last = 0;
    ct_touch_reset(&U->F);
}

void ct_game_new(ct_game *U)
{
    ct_calm(&U->A);
    U->fl.n = 0;
    ct_game_reset(U);
    U->floor_n = 0;
    U->seen = 0;
}

void ct_game_hows(const ct_game *U, const uint8_t *h, int n, int mine, int (*ok)(int card), int *how)
{
    for (int i = 0; i < n; i++) {
        how[i] = mine ? (ok(h[i]) ? CD_GLOW : CD_DIM) : 0;
        if (ct_is_raised(&U->F, i)) how[i] = CD_RAISED;
    }
}

void ct_game_look(ct_game *U, const uint8_t *h, int n, int start)
{
    if ((U->seen >> U->T.me & 1) || ui_handing()) return;
    U->seen |= 1u << U->T.me;
    if (start) ct_deal(&U->A, &U->T, h, n);
}

void ct_game_draw_middle(const ct_game *U, int tricks, const uint8_t *trick, const uint8_t *who, int ntrick,
                         int leader, int target)
{
    ct_draw_pile(&U->T, tricks);
    ct_draw_trick(&U->T, &U->A, trick, who, ntrick, leader);
    if (U->F.raised != CARD_NONE && target) ct_draw_target(&U->T);
}

void ct_game_draw_hand(ct_game *U, const uint8_t *h, int n, int mine, int (*ok)(int card), int hide)
{
    int how[CT_HAND];
    if (n > CT_HAND) n = CT_HAND;
    if (hide) ct_hand_backs(&U->T, n);
    else {
        ct_game_hows(U, h, n, mine, ok, how);
        ct_hand_draw(&U->T, &U->A, h, n, how, NULL, ct_dragged(&U->F));
    }
    ct_draw(&U->A);
    if (!hide) ct_touch_draw(&U->F, &U->T);
    const uint8_t pill[4] = {24, 30, 28, 255};
    ct_floats_draw(&U->fl, ct_round((int)(1.5f * ui.unit), (int)(0.56f * ui.unit), 0.28f * ui.unit, pill), NULL);
}

void ct_game_draw_last(const ct_game *U, const uint8_t *last, const uint8_t *who, int n, int leader, int winner)
{
    if (U->show_last && winner >= 0 && winner < U->T.players) ct_draw_last(&U->T, last, who, n, leader, winner);
}

int ct_game_press_first(ct_game *U)
{
    U->F.card = CARD_NONE;
    U->F.on = 0;
    if (U->show_last) { U->show_last = 0; return 1; }
    return ct_busy(&U->A);
}

int ct_game_press_pile(ct_game *U, int tricks, float x, float y)
{
    if (!ct_on_pile(&U->T, tricks, x, y)) return 0;
    U->show_last = 1;
    ui_sound(SND_TAP);
    return 1;
}

void ct_game_press_card(ct_game *U, const uint8_t *h, int n, int (*ok)(int card), void (*send)(int card), float x,
                        float y)
{
    if (U->F.raised != CARD_NONE && ct_on_target(&U->T, x, y)) { send(U->F.raised); return; }
    int how[CT_HAND];
    if (n > CT_HAND) n = CT_HAND;
    ct_game_hows(U, h, n, 1, ok, how);
    ct_touch_hand(&U->F, &U->T, h, n, how, NULL, x, y);
}

void ct_game_release(ct_game *U, int (*ok)(int card), void (*send)(int card), float x, float y)
{
    int c, r = ct_touch_up(&U->F, &U->T, x, y, &c);
    if (r == CT_T_SEND) send(c);
    else if (r == CT_T_TAP && ct_tap_play(&U->F, c, ok(c))) send(c);
    U->F.on = 0;
}

void ct_game_played(ct_game *U, int card, const uint8_t *h, int n, const uint8_t *last, int leader, int winner)
{
    ct_play(&U->A, &U->T, &U->F, h, n, card);
    if (last)
        ct_gather(&U->A, &U->T, last, NULL, U->T.players, leader, winner, CT_SLIDE_MS + CT_LOOK_MS, CT_SLIDE_MS);
}

void ct_game_float(ct_game *U, int pl, int value, int badge)
{
    ct_float(&U->fl, &U->A, U->T.sx[pl], U->T.sy[pl] - 0.8f * U->T.tok, value, badge,
             ui_ms() + CT_SLIDE_MS + CT_LOOK_MS + 300);
}

/* ---- the calls' pictures ----------------------------------------------------------------------------------------- */
void ct_call_paint(int kind, int s, uint8_t *rgba)
{
    static const uint8_t ink[3] = {44, 40, 36};
    float asp = cards_aspect(DECK_FRENCH);
    memset(rgba, 0, (size_t)s * s * 4);
    if (kind >= CT_CALL_CARD && kind < CT_CALL_FIG) {
        int card = kind & 0xff, pair = kind >= CT_CALL_PAIR;
        float w = 0.9f * s / asp * (pair ? 0.86f : 1);
        if (pair) {
            cards_paint_turned(rgba, s, DECK_FRENCH, card, 0.5f * s - 0.17f * w, 0.5f * s + 0.02f * s, w, -12);
            cards_paint_turned(rgba, s, DECK_FRENCH, card, 0.5f * s + 0.17f * w, 0.5f * s - 0.02f * s, w, 12);
        } else
            cards_paint_turned(rgba, s, DECK_FRENCH, card, 0.5f * s, 0.5f * s, w, 0);
        return;
    }
    if (kind == CT_CALL_ACES) {                 /* the four aces fanned */
        static const int suit[4] = {SUIT_CLUBS, SUIT_HEARTS, SUIT_SPADES, SUIT_DIAMONDS};
        float w = 0.62f * s / asp;
        for (int i = 0; i < 4; i++)
            cards_paint_turned(rgba, s, DECK_FRENCH, CARD(suit[i], RANK_ACE), 0.5f * s + (i - 1.5f) * 0.2f * s,
                               0.54f * s + fabsf(i - 1.5f) * 0.03f * s, w, (i - 1.5f) * 14);
        return;
    }
    ct_paint_ivory(rgba, s);
    if (kind >= CT_CALL_FIG && kind < CT_CALL_UP)   /* a chess figure in amber */
        ct_paint_piece(rgba, s, kind & 0xff, &art_player[4], 0.8f, 0);
    else {                                      /* an arrow up or down */
        art_fig *a = art_fig_new();
        float d = kind == CT_CALL_UP ? -1 : 1;
        art_band(a, ART_PART, 0.5f, 0.5f - 0.29f * d, 0.5f, 0.5f + 0.02f * d, 0.15f);
        art_head(a, ART_PART, 0.5f, 0.5f + 0.17f * d, d < 0 ? 270 : 90, 0.38f);
        art_fig_paint(rgba, s, a, ink, NULL, 0, 0);
    }
    cards_disc_cut(rgba, s);
    cards_disc_rim(rgba, s);
}

SDL_Texture *ct_call_tex(int kind, float d)
{
    int s = (int)(d + 0.5f);
    if (s < 4) s = 4;
    return kind < 4 ? ct_suit(DECK_FRENCH, kind, s) : ct_picture(ct_call_paint, kind, s);
}

void ct_call(int kind, float x, float y, float d, Uint8 alpha)
{
    SDL_Texture *t = ct_call_tex(kind, d);
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put_c(t, x, y, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

void ct_strike(float x, float y, float d) { ct_pic(CT_SLASH, x, y, d, 0, 230, 70, 60, 255); }

void ct_card_flat(int deck, int card, float x, float y, float h, int how)
{
    cards_draw(deck, card, x, y, h / cards_aspect(deck), 0, CD_FLAT | how);
}

/* ---- the summaries' pieces --------------------------------------------------------------------------------------- */
float ct_chips(unsigned who, float x, float y, float d, Uint8 alpha)
{
    float cx = x;
    int k = 0;
    for (int q = 0; q < CT_MAXP; q++) {
        if (!(who >> q & 1)) continue;
        SDL_Texture *t = ct_token(q, (int)d);
        SDL_SetTextureAlphaMod(t, alpha);
        ui_put_c(t, cx, y, d, d);
        SDL_SetTextureAlphaMod(t, 255);
        cx += 0.78f * d;
        k++;
    }
    return k ? d + (k - 1) * 0.78f * d : 0;
}

void ct_deal_list(float x0, float pw, float y, float rh, int N, int per, int done, const uint8_t *who,
                  const uint8_t *made)
{
    float u = ui.unit, cs = floorf(fminf(0.56f * rh, (pw - 0.6f * u) / (per * 1.45f)));
    for (int i = 0; i < N; i++) {
        float cx = x0 + pw / 2 + (i % per - (per - 1) / 2.0f) * 1.45f * cs, cy = y + (i / per) * rh * 0.85f;
        if (i >= done) { ct_pic(CT_DISC_DARK, cx, cy, 0.4f * cs, 0, 255, 255, 255, 160); continue; }
        if (!who[i]) {
            ct_pic(CT_DISC_OFF, cx, cy, cs, 0, 255, 255, 255, 255);
            ct_pic(CT_DISC_DARK, cx, cy, 0.7f * cs, 0, 255, 255, 255, 255);
            continue;
        }
        int k = 0;
        for (int q = 0; q < CT_MAXP; q++) k += who[i] >> q & 1;
        float d = k > 1 ? 0.8f * cs : cs, w = d + (k - 1) * 0.5f * d;
        if (made[i]) ui_mark_gold(cx, cy, 1.7f * cs);
        float x = cx - w / 2 + d / 2;
        for (int q = 0; q < CT_MAXP; q++) {
            if (!(who[i] >> q & 1)) continue;
            SDL_Texture *t = ct_token(q, (int)d);
            SDL_SetTextureAlphaMod(t, made[i] ? 255 : 110);
            ui_put_c(t, x, cy - (k > 1 ? 0.12f * d : 0) + (x > cx ? 0.24f * d : 0), d, d);
            SDL_SetTextureAlphaMod(t, 255);
            x += 0.5f * d;
        }
    }
}

void ct_total(float r, float y, float h, int v)
{
    float w = ct_number_w(v, h, 0);
    ui_number_signed(r - w / 2, y, h, v, 1, 0);
}

/* ---- the others along the top, the viewer's hand in a box -------------------------------------------------------- */
void ct_backs(int deck, float cx, float cy, float width, float w, int n, int how)
{
    cards_spot s[14];
    int k = n < 14 ? n : 14;
    if (k <= 0) return;
    cards_fan(cx, cy, width, w, cards_aspect(deck), k, k > 1 ? 18 : 0, s);
    for (int i = 0; i < k; i++) cards_draw(deck, CARD_NONE, s[i].x, s[i].y, w, s[i].deg, CD_DOWN | how);
}

void ct_backs_end(int deck, float cx, float cy, float width, float w, int n, float *x, float *y, float *deg)
{
    cards_spot s[14];
    int k = n < 1 ? 1 : n < 14 ? n : 14;
    cards_fan(cx, cy, width, w, cards_aspect(deck), k, k > 1 ? 18 : 0, s);
    *x = s[k - 1].x;
    *y = s[k - 1].y;
    *deg = s[k - 1].deg;
}

void ct_label(SDL_Texture *token, float cx, float cy, float h, int n)
{
    float digits = n >= 10 ? 2 : 1, w = 1.15f * h + 0.62f * h * digits;
    ui_put_c(token, cx - w / 2 + h / 2, cy, h, h);
    ui_number(cx + w / 2 - 0.31f * h * digits, cy, 0.85f * h, n, 1);
}

void ct_others_layout(ct_others *O, box b, int deck, int players, int me, float hs)
{
    float u = ui.unit;
    O->b = b;
    O->deck = deck;
    O->players = players;
    O->me = me < 0 || me >= players ? 0 : me;
    O->k = players > 1 ? players - 1 : 1;
    O->asp = cards_aspect(deck);
    O->sw = floorf(fminf(fminf(hs * 0.6f / O->asp, b.w / O->k * 0.3f), 0.85f * u));
    O->sfan = fminf(b.w / O->k * 0.8f, O->sw * 3.0f);
    O->sy = b.y + hs * 0.4f;
    float ch = O->sw * O->asp;
    O->lh = 0.48f * O->sw;
    O->ly = O->sy + 0.5f * ch + 0.3f * O->sw + 0.12f * ch;
}

float ct_others_x(const ct_others *O, int q) { return ct_seat_x(O->b.x, O->b.w, O->k, ct_seat(q, O->me, O->players)); }

void ct_others_end(const ct_others *O, int q, int count, float *x, float *y, float *deg)
{
    ct_backs_end(O->deck, ct_others_x(O, q), O->sy, O->sfan, O->sw, count, x, y, deg);
}

void ct_others_draw(const ct_others *O, int q, int n, int glow)
{
    float x = ct_others_x(O, q);
    if (glow) ui_mark_gold(x, O->sy, 2.4f * O->sw);
    if (n > 0) ct_backs(O->deck, x, O->sy, O->sfan, O->sw, n, 0);
    else cards_place(O->deck, x, O->sy, O->sw);
    ct_label(ct_token(q, (int)fmaxf(8, O->lh)), x, O->ly, O->lh, n);
}

void ct_others_badge(const ct_others *O, int q, int n, int side, float *x, float *y, float *d)
{
    float lw = 1.15f * O->lh + 0.62f * O->lh * (n >= 10 ? 2 : 1);
    *d = 1.25f * O->lh;
    *x = ct_others_x(O, q) + side * (lw / 2 + 0.7f * *d);
    *y = O->ly;
}

float ct_bar_cards(box b, float x, int deck, int n)
{
    float u = ui.unit, cy = b.y + b.h / 2, hc = 0.36f * u, cw = 0.28f * u, wn = ct_number_w(n, hc, 0);
    if (x + cw + 0.1f * u + wn > b.x + b.w) return x;
    cards_draw(deck, CARD_NONE, x + cw / 2, cy, cw, 0, CD_DOWN | CD_FLAT);
    x += cw + 0.1f * u;
    ui_number(x + wn / 2, cy, hc, n, 1);
    return x + wn;
}

void ct_drag_start(ct_drag *d, int moved, int card, float x, float y)
{
    d->on = 1, d->moved = moved, d->card = card;
    d->x0 = d->x = x, d->y0 = d->y = y;
}

void ct_drag_move(ct_drag *d, float x, float y, float far)
{
    if (!d->on) return;
    if (d->moved <= 0 && hypotf(x - d->x0, y - d->y0) > far) d->moved = 1;
    d->x = x, d->y = y;
    if (d->moved > 0) ui_animate();
}

float ct_columns_fit(box C, float wmax, int longest, float asp, float *step)
{
    float w = floorf(fminf(wmax, C.w / 4.35f)), hh = w * asp;
    if (hh * (1 + 0.26f * (longest - 1)) > C.h) {
        w = floorf(C.h / (asp * (1 + 0.26f * (longest - 1))));
        hh = w * asp;
    }
    *step = longest > 1 ? fminf(0.36f * hh, (C.h - hh) / (longest - 1)) : 0;
    return w;
}

int ct_columns(box C, float w, float step, float asp, const uint8_t *h, int n, int (*col_of)(int card),
               uint8_t *cards, float *x, float *y)
{
    int k = 0;
    for (int col = 0; col < 4; col++)
        for (int j = n - 1, i = 0; j >= 0; j--) {
            if (col_of(h[j]) != col) continue;
            cards[k] = h[j];
            x[k] = C.x + C.w / 2 + (col - 1.5f) * 1.08f * w;
            y[k] = C.y + w * asp / 2 + i * step;
            k++, i++;
        }
    return k;
}

int ct_columns_hit(const uint8_t *cards, const float *x, const float *y, int n, float w, float asp, int raised,
                   float px, float py)
{
    for (int i = n - 1; i >= 0; i--)
        if (cards_in(x[i], y[i] - (cards[i] == raised ? 0.12f * w : 0), w, asp, px, py)) return i;
    return -1;
}

float ct_hand(box b, int n, float aspect, float wmax, cards_spot *spots)
{
    if (n <= 0) return wmax;
    float w = fminf(wmax, b.h / (aspect * 1.1f));
    float step = n > 1 ? (b.w - w) / (n - 1) : w;
    if (step >= 0.4f * w || n < 8) {
        cards_fan(b.x + b.w / 2, b.y + b.h / 2 - 0.03f * w * aspect, b.w, w, aspect, n, n > 1 ? 8 : 0, spots);
        return w;
    }
    int top = n / 2, bot = n - top;             /* two rows: the second overlaps the lower part of the first */
    w = fminf(wmax, b.h / (aspect * 1.62f));
    float h = w * aspect, y0 = b.y + (b.h - 1.55f * h) / 2 + h / 2;
    cards_fan(b.x + b.w / 2, y0, b.w, w, aspect, top, 0, spots);
    cards_fan(b.x + b.w / 2, y0 + 0.55f * h, b.w, w, aspect, bot, 0, spots + top);
    return w;
}

void ct_rows_layout(ct_rows *h, box b, int n, int per_row, float w_max, float h_max)
{
    if (n > CT_HAND) n = CT_HAND;
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

int ct_rows_hit(const ct_rows *h, float x, float y)
{
    float lift = 0.14f * h->w;                  /* a raised card can be hit where it shows */
    int i = cards_hit(h->spot, h->n, h->w, cards_aspect(DECK_FRENCH), x, y);
    if (i < 0) i = cards_hit(h->spot, h->n, h->w, cards_aspect(DECK_FRENCH), x, y + lift);
    return i;
}

/* ---- patience ---------------------------------------------------------------------------------------------------- */
void ct_column_steps(int n, int down, float room, float w, float down_step, float up_step, float *dd, float *du)
{
    float d = down_step * w, s = up_step * w;
    int nd = down < n - 1 ? down : n - 1, nu = n - 1 - nd;
    if (nd < 0) nd = 0;
    if (nu < 0) nu = 0;
    if (nd * d + nu * s > room && nu) s = fmaxf(0.21f * w, (room - nd * d) / nu);
    if (nd * d + nu * s > room && nd) d = fmaxf(0.045f * w, (room - nu * s) / nd);
    if (nd * d + nu * s > room && nu) s = fmaxf(0.02f * w, (room - nd * d) / nu);
    *dd = d;
    *du = s;
}

void ct_run(int deck, const uint8_t *cards, int n, float x, float y, float w, float step)
{
    for (int k = 0; k < n; k++) cards_draw(deck, cards[k], x, y + k * step, w, 0, k == 0 ? CD_RAISED : 0);
}

void ct_leap(box b, float w, int j, float x0, float y0, float t, float *xs, float *ys)
{
    uint32_t h = (uint32_t)j * 2654435761u + 0x9e37u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    float r1 = (h & 1023) / 1023.0f, r2 = ((h >> 10) & 1023) / 1023.0f;
    float dir = x0 > b.x + b.w / 2 ? -1 : 1;
    if ((h >> 20) % 3 == 0) dir = -dir;         /* a third of them the other way */
    float vx = dir * (0.55f + 0.35f * r1) * b.w, vy = -(0.15f + 0.65f * r2) * b.h, g = 2.4f * b.h;
    float floor_y = b.y + b.h - 0.7f * w, px = x0, py = y0, dt = 1 / 200.0f;
    int at[CT_TRAIL + 1], k = CT_TRAIL;
    for (int i = 0; i <= CT_TRAIL; i++) {
        float ti = t - i * CT_TRAIL_MS;
        at[i] = ti < 0 ? -1 : (int)(ti / 1000 / dt);
        xs[i] = ys[i] = NAN;
    }
    while (k >= 0 && at[k] < 0) k--;
    for (int s = 0; k >= 0; s++) {
        while (k >= 0 && at[k] == s) xs[k] = px, ys[k] = py, k--;
        vy += g * dt;
        px += vx * dt;
        py += vy * dt;
        if (py > floor_y && vy > 0) { py = floor_y; vy = -vy * 0.74f; }
    }
}
