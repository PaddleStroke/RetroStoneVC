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

/* the kit's own pictures: white figures, a cream moon, the buttons' discs */
static void paint_kit(const int *k, int s, int h, uint8_t *rgba, user_painter u)
{
    static const uint8_t off_c[3] = {54, 72, 64}, on_c[3] = {150, 118, 46}, dark_c[3] = {22, 30, 27},
                         cream[3] = {246, 226, 160}, black[3] = {0, 0, 0};
    int kind = k[0];
    (void)h, (void)u;
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
    if (tricks > 0) cards_pile(DECK_FRENCH, T->px, T->py, T->pw, tricks, CARD_NONE, 0);
}

int ct_on_pile(const ct_table *T, int tricks, float x, float y)
{
    return tricks > 0 && cards_in(T->px, T->py, T->pw * 1.5f, T->asp, x, y);
}

void ct_summary(const ct_table *T, int rows, float *x0, float *pw, float *y_row, float *rh)
{
    box b = T->b;
    float u = T->u;
    *rh = fminf(1.15f * u, b.h * 0.8f / rows);
    *pw = fminf(b.w * 0.94f, 8.0f * u);
    float ph = *rh * rows + 0.5f * u, y0 = b.y + (b.h - ph) / 2;
    *x0 = b.x + (b.w - *pw) / 2;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 80);
    ct_panel(*x0, y0, *pw, ph, 246);
    *y_row = y0 + 0.25f * u + *rh / 2;
}

void ct_bar(box b, int cards, int score)
{
    float u = ui.unit, cy = b.y + b.h / 2, h = 0.46f * u, hc = 0.3f * u;
    float cw = 0.26f * u, wc = ct_number_w(cards, hc, 0);
    float need = cw + 0.08f * u + wc + 0.3f * u + ct_number_w(score, h, 0);
    float x = b.x;
    if (b.w >= need) {
        cards_draw(DECK_FRENCH, CARD_NONE, x + cw / 2, cy, cw, 0, CD_DOWN | CD_FLAT);
        x += cw + 0.08f * u;
        ui_number(x + wc / 2, cy, hc, cards, 1);
        x += wc + 0.3f * u;
    }
    float w = ct_number_w(score, 1, 0), hs = fminf(h, (b.x + b.w - x) / w * 0.95f);
    if (hs > 0) ui_number_signed(x + w * hs / 2, cy, hs, score, 1, 0);
}

void ct_thumb(const int *cards, int n, int size, uint8_t *rgba)
{
    float asp = cards_aspect(DECK_FRENCH), w = fminf(size * 0.44f, size * 0.8f / asp);
    int cw = (int)w, ch = (int)(w * asp);
    uint8_t *b = malloc((size_t)cw * ch * 4);
    memset(rgba, 0, (size_t)size * size * 4);
    if (!b) return;
    for (int i = 0; i < n; i++) {
        cards_paint(DECK_FRENCH, cards[i], cw, ch, b);
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
        if (h[i] == skip || ct_hides(mv, CT_HAND_TAG, h[i], -1)) continue;
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

int ct_hand_at(const ct_table *T, const uint8_t *h, int n, const int *how, const float *lift, float x, float y)
{
    cards_spot s[CT_HAND];
    if (n > CT_HAND) n = CT_HAND;
    ct_fan(T, n, s);
    for (int i = 0; i < n; i++) {
        float up = how[i] & CD_RAISED ? 0.14f : lift && lift[i] > 0 ? lift[i] : 0;
        if (up > 0 && cards_hit(&s[i], 1, T->wh, T->asp, x, y + up * T->wh)) return h[i];
    }
    int i = cards_hit(s, n, T->wh, T->asp, x, y);
    return i < 0 ? CARD_NONE : h[i];
}

void ct_deal(ct_moves *mv, const ct_table *T, const uint8_t *h, int n)
{
    cards_spot s[CT_HAND];
    if (n > CT_HAND) n = CT_HAND;
    ct_fan(T, n, s);
    for (int i = 0; i < n; i++)
        ct_rest(mv, ct_slide(mv, T->deck, h[i], T->cx, T->cy, T->wt, 0, 0, s[i].x, s[i].y, T->wh, s[i].deg, 1,
                             CT_DEAL_MS, 50 * i, CT_HAND_TAG, h[i], -1, i % 3 == 0 ? SND_MOVE : -1), 0);
}

/* ---- the finger -------------------------------------------------------------------------------------------------- */
void ct_touch_reset(ct_touch *t)
{
    t->card = t->raised = CARD_NONE;
    t->on = 0;
}

void ct_touch_down(ct_touch *t, int card, float x, float y)
{
    t->card = card;
    t->on = 0;
    t->x0 = t->x = x;
    t->y0 = t->y = y;
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
    if (t->raised == card) return 1;
    t->raised = card;
    ui_sound(SND_TAP);
    return 0;
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
    cards_spot s[CT_HAND];
    if (n > CT_HAND) n = CT_HAND;
    ct_fan(T, n, s);
    int i = 0, me = T->me;
    while (i < n && h[i] != card) i++;
    float x0 = i < n ? s[i].x : T->hx, y0 = i < n ? s[i].y - 0.14f * T->wh : T->hy, d0 = i < n ? s[i].deg : 0;
    if (f->on) x0 = f->x, y0 = f->y, d0 = 0;
    ct_rest(mv, ct_slide(mv, T->deck, card, x0, y0, T->wh, d0, 1, T->tx[me], T->ty[me], T->wt, T->tdeg[me], 1,
                         CT_SLIDE_MS, 0, CT_TRICK, me, -1, SND_PLACE), 200);
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
