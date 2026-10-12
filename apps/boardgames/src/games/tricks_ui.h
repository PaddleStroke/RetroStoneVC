/*
 * Trick-taking games, the table they share (Hearts, Spades): the viewer's hand fanned at the bottom, a seat for each
 * player round the middle (their token, the card they play placed towards them), the trick pile (tap it: the last
 * trick), cards sliding (played, gathered by the trick's winner), the round's summary panel, round buttons, the bars
 * (a small card back with the cards in hand, the score). Static inline; the game keeps the state (tk_table, tk_anim,
 * tk_gfx). The rules side is games/tricks.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The seats: the viewer at the bottom; the others in turn order from the left round the top (the order of their bars
 * at the top of the screen), so play goes round clockwise.
 */
#ifndef TRICKS_UI_H
#define TRICKS_UI_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "shell.h"

#define TK_MAXP 6
#define TK_HAND 24
#define TK_MOVES 48
#define TK_PI 3.14159265f

/* ---- the table's geometry ---------------------------------------------------------------------------------------- */
typedef struct {
    box b;                                      /* the table (between the bars) */
    int players, me;                            /* me: the viewer, at the bottom */
    float asp;                                  /* a card's height / width */
    float wh, hx, hy, hw, arc;                  /* the hand: card width, the fan's centre and width, its arc */
    float hand_top;                             /* above it: the trick's area */
    float wt, cx, cy, rx, ry;                   /* the trick: card width, centre, the cards' ellipse */
    float tok, sx[TK_MAXP], sy[TK_MAXP], ang[TK_MAXP];   /* the seats: token size, centres, angles (degrees) */
    float tx[TK_MAXP], ty[TK_MAXP], tdeg[TK_MAXP];       /* each player's card in the trick */
    float px, py, pw;                           /* the trick pile */
    float u;
} tk_table;

/* where a player sits (degrees, y down): the viewer below (90), the others evenly round the table in turn order,
 * clockwise (from the left, over the top, to the right) */
static inline float tk_angle(int players, int me, int pl)
{
    float a = 90 + (float)((pl - me + players) % players) * 360 / players;
    return a >= 360 ? a - 360 : a;
}

static inline void tk_layout(tk_table *T, box b, int players, int me, int max_hand)
{
    float u = ui.unit;
    T->b = b;
    T->u = u;
    T->players = players;
    T->me = me < 0 || me >= players ? 0 : me;
    T->asp = cards_aspect(DECK_FRENCH);
    int portrait = b.w <= b.h * 1.05f;
    T->wh = floorf(fminf(b.w * (portrait ? 0.2f : 0.12f), b.h * (portrait ? 0.15f : 0.165f)));
    T->hw = fminf(b.w * 0.97f, T->wh * (1 + 0.62f * (max_hand - 1)));
    T->arc = fminf(16, 1.1f * max_hand);
    float hh = T->wh * T->asp;
    T->hy = floorf(b.y + b.h - hh / 2 - 0.1f * T->wh - (T->hw / 2) * tanf(T->arc / 2 * TK_PI / 180) * 0.5f);
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
        float a = tk_angle(players, T->me, pl), r = a * TK_PI / 180, c = cosf(r), s = sinf(r);
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

/* ---- the hand ---------------------------------------------------------------------------------------------------- */
static inline void tk_fan(const tk_table *T, int n, cards_spot *s)
{
    float w = fminf(T->hw, T->wh * (1 + 0.95f * (n - 1)));
    cards_fan(T->hx, T->hy, w, T->wh, T->asp, n, n > 1 ? T->arc * (n - 1) / (n + 2.0f) : 0, s);
}

/* ---- moving cards ------------------------------------------------------------------------------------------------ */
typedef struct {
    cards_slide s;
    int tag, idx;
    Uint32 show_after;                          /* not drawn before (another card brings it there first) */
} tk_move;
typedef struct {
    tk_move m[TK_MOVES];
    int n;
    Uint32 quiet;                               /* busy until then (the last card landed, and a moment) */
} tk_anim;

enum { TK_NONE, TK_TRICK, TK_HAND_TAG, TK_SEAT };

static inline void tk_calm(tk_anim *a)
{
    a->n = 0;
    a->quiet = 0;
}

static inline void tk_slide(tk_anim *a, int card, float x0, float y0, float w0, float a0, int up0, float x1, float y1,
                            float w1, float a1, int up1, int ms, int delay, int tag, int idx, int show_after,
                            int sound, int rest)
{
    if (a->n >= TK_MOVES) return;
    tk_move *m = &a->m[a->n++];
    cards_slide_start(&m->s, DECK_FRENCH, card, x0, y0, w0, a0, up0, x1, y1, w1, a1, up1, ms, delay);
    m->tag = tag;
    m->idx = idx;
    m->show_after = ui_ms() + (Uint32)show_after;
    Uint32 end = ui_ms() + (Uint32)(delay + ms + rest);
    if (end > a->quiet) a->quiet = end;
    if (sound >= 0) ui_sound_later(sound, delay + ms - 40 > 0 ? delay + ms - 40 : 0);
}

static inline int tk_busy(const tk_anim *a)
{
    for (int i = 0; i < a->n; i++)
        if (cards_slide_busy(&a->m[i].s)) return 1;
    return a->quiet && ui_ms() < a->quiet;
}

/* a card on its way to (tag, idx): hidden there until it lands */
static inline int tk_hides(const tk_anim *a, int tag, int idx)
{
    for (int i = 0; i < a->n; i++)
        if (a->m[i].tag == tag && a->m[i].idx == idx && cards_slide_busy(&a->m[i].s)) return 1;
    return 0;
}

static inline void tk_draw_moves(tk_anim *a)
{
    for (int i = 0; i < a->n; i++) {
        if (ui_ms() < a->m[i].show_after) { ui_animate(); continue; }
        cards_slide_draw(&a->m[i].s);
    }
}

/* the trick gathered: its cards (played from `leader` on) slide to the winner's seat after a look at them; the last
 * one is still on its way (it shows once it has landed) */
static inline void tk_gather(tk_anim *a, const tk_table *T, const uint8_t *cards, int n, int leader, int winner,
                             int look_ms, int last_lands)
{
    for (int i = 0; i < n; i++) {
        int pl = (leader + i) % T->players;
        tk_slide(a, cards[i], T->tx[pl], T->ty[pl], T->wt, T->tdeg[pl], 1, T->sx[winner], T->sy[winner],
                 0.45f * T->wt, T->tdeg[winner], 1, 360, look_ms + 30 * i, TK_NONE, 0,
                 i == n - 1 ? last_lands : 0, i == 0 ? SND_MOVE : -1, 200);
    }
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* the small pictures, made at the size they are drawn and kept (a few sizes of each) */
enum { TK_ARROW, TK_TICK, TK_SLASH, TK_MOON, TK_DISC_OFF, TK_DISC_ON, TK_DISC_DARK, TK_KINDS };
#define TK_PICS 16
typedef struct {
    SDL_Texture *tok[TK_MAXP], *panel;
    int tok_size, panel_w, panel_h;
    struct { int kind, size; unsigned used; SDL_Texture *t; } pic[TK_PICS];
    unsigned clock;
} tk_gfx;

/* white figures (tinted when drawn): an arrow pointing right, a tick, a slash (nil's mark over its 0); a cream
 * crescent moon; discs in the shell's button colours (off, on) and a dark one (a bid's badge) */
static inline void tk_paint(int kind, int s, uint8_t *rgba)
{
    static const uint8_t off_c[3] = {54, 72, 64}, on_c[3] = {150, 118, 46}, dark_c[3] = {22, 30, 27},
                         cream[3] = {246, 226, 160}, black[3] = {0, 0, 0};
    memset(rgba, 0, (size_t)s * s * 4);
    if (kind >= TK_DISC_OFF) {
        art_disc(s, kind == TK_DISC_ON ? on_c : kind == TK_DISC_OFF ? off_c : dark_c, kind == TK_DISC_DARK ? 0.85f : 1,
                 0, rgba);
        return;
    }
    if (kind == TK_MOON) {                      /* a disc, a smaller one cut out of it (upper right) */
        art_disc(s, cream, 1, 0, rgba);
        int k = (int)(0.8f * s), off = (int)(0.34f * s), up = (int)(0.12f * s);
        uint8_t *cut = malloc((size_t)k * k * 4);
        if (!cut) return;
        art_disc(k, black, 1, 0, cut);
        for (int y = 0; y < k; y++)
            for (int x = 0; x < k; x++) {
                int X = x + off, Y = y - up;
                if (X < 0 || Y < 0 || X >= s || Y >= s) continue;
                uint8_t *p = rgba + ((size_t)Y * s + X) * 4;
                p[3] = (uint8_t)(p[3] * (255 - cut[((size_t)y * k + x) * 4 + 3]) / 255);
            }
        free(cut);
        return;
    }
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    if (kind == TK_ARROW) {
        const float head[] = {0.50f, 0.22f, 0.86f, 0.50f, 0.50f, 0.78f};
        art_rect(f, ART_PART, 0.14f, 0.42f, 0.56f, 0.58f);
        art_poly(f, ART_PART, head, 3);
    } else if (kind == TK_TICK) {
        const float tick[] = {0.16f, 0.52f, 0.27f, 0.41f, 0.42f, 0.56f, 0.74f, 0.22f, 0.85f, 0.33f, 0.42f, 0.78f};
        art_poly(f, ART_PART, tick, 6);
    } else {
        const float slash[] = {0.70f, 0.08f, 0.80f, 0.13f, 0.30f, 0.92f, 0.20f, 0.87f};
        art_poly(f, ART_PART, slash, 4);
    }
    art_render(f, &st, s, s, (float)s, 0, 0, rgba);
    art_fig_free(f);
}

static inline SDL_Texture *tk_pic(tk_gfx *g, int kind, int size)
{
    if (size < 2) size = 2;
    int lru = 0;
    g->clock++;
    for (int i = 0; i < TK_PICS; i++) {
        if (g->pic[i].t && g->pic[i].kind == kind && g->pic[i].size == size) {
            g->pic[i].used = g->clock;
            return g->pic[i].t;
        }
        if (!g->pic[i].t || g->pic[i].used < g->pic[lru].used) lru = i;
        if (!g->pic[i].t) break;
    }
    ui_free(&g->pic[lru].t);
    tk_paint(kind, size, ui_buf(size, size));
    g->pic[lru].t = ui_tex(size, size, ui_buf(size, size));
    g->pic[lru].kind = kind;
    g->pic[lru].size = size;
    g->pic[lru].used = g->clock;
    return g->pic[lru].t;
}

/* a picture centred, d wide, turned deg, tinted (r, g, b, a) */
static inline void tk_put(tk_gfx *g, int kind, float cx, float cy, float d, double deg, Uint8 r, Uint8 gr, Uint8 b,
                          Uint8 a)
{
    SDL_Texture *t = tk_pic(g, kind, (int)(d + 0.5f));
    if (!t) return;
    SDL_SetTextureColorMod(t, r, gr, b);
    SDL_SetTextureAlphaMod(t, a);
    ui_put_rot(t, cx, cy, d, d, deg);
}

static inline void tk_tokens(tk_gfx *g, int size)
{
    if (size < 4) size = 4;
    if (g->tok_size == size) return;
    g->tok_size = size;
    for (int q = 0; q < TK_MAXP; q++) {
        ui_free(&g->tok[q]);
        cards_token(q, size, ui_buf(size, size));
        g->tok[q] = ui_tex(size, size, ui_buf(size, size));
    }
}

/* a dark panel (the round's summary) */
static inline void tk_panel(tk_gfx *g, float x, float y, float w, float h)
{
    if (g->panel_w != (int)w || g->panel_h != (int)h) {
        g->panel_w = (int)w;
        g->panel_h = (int)h;
        static const uint8_t c[4] = {28, 34, 32, 246};
        ui_free(&g->panel);
        art_roundrect(g->panel_w, g->panel_h, 0.4f * ui.unit, c, ui_buf(g->panel_w, g->panel_h));
        g->panel = ui_tex(g->panel_w, g->panel_h, ui_buf(g->panel_w, g->panel_h));
    }
    ui_put(g->panel, x, y, w, h);
}

/* a round button (the shell's colours): `on` gold, its icon (TK_ARROW, TK_TICK; -1: none) turned `deg`, faint when
 * off */
static inline void tk_button(tk_gfx *g, float cx, float cy, float d, int on, int icon, double deg)
{
    tk_put(g, on ? TK_DISC_ON : TK_DISC_OFF, cx, cy, d, 0, 255, 255, 255, 255);
    if (icon >= 0) tk_put(g, icon, cx, cy, 0.66f * d, deg, 255, 255, 255, on ? 255 : 110);
}

/* a number in cream, h high; nil (a bid of 0): the 0 struck through in gold */
static inline void tk_number(tk_gfx *g, float cx, float cy, float h, int v, int nil)
{
    ui_number(cx, cy, h, v, 1);
    if (nil) tk_put(g, TK_SLASH, cx, cy, 1.15f * h, 0, 250, 214, 90, 255);
}

static inline void tk_moon(tk_gfx *g, float cx, float cy, float d)
{
    tk_put(g, TK_MOON, cx, cy, d, 0, 255, 255, 255, 255);
}

/* the seats' tokens; the player to move in gold light */
static inline void tk_draw_seats(tk_gfx *g, const tk_table *T, int turn)
{
    tk_tokens(g, (int)T->tok);
    for (int pl = 0; pl < T->players; pl++) {
        if (pl == turn) ui_mark_gold(T->sx[pl], T->sy[pl], 2.1f * T->tok);
        ui_put_c(g->tok[pl], T->sx[pl], T->sy[pl], T->tok, T->tok);
    }
}

/* beside a seat's token, towards the middle: where its numbers go (k: 0 the first, 1 the next) */
static inline void tk_seat_spot(const tk_table *T, int pl, int k, float *x, float *y)
{
    float a = T->ang[pl];
    float side = a > 300 || a < 100 ? -1 : 1;   /* the right-hand seats: to their left */
    if (a > 240 && a < 300) side = 1;
    *x = T->sx[pl] + side * (0.95f + 0.85f * k) * T->tok;
    *y = T->sy[pl];
}

/* the cards of a trick on the table (played from `leader` on), those still on their way left out */
static inline void tk_draw_trick(const tk_table *T, const tk_anim *a, const uint8_t *cards, int n, int leader)
{
    for (int i = 0; i < n; i++) {
        int pl = (leader + i) % T->players;
        if (tk_hides(a, TK_TRICK, pl)) continue;
        cards_draw(DECK_FRENCH, cards[i], T->tx[pl], T->ty[pl], T->wt, T->tdeg[pl], 0);
    }
}

/* the last trick, over the table: its cards in their places, the winner's seat in gold */
static inline void tk_draw_last(const tk_table *T, const uint8_t *cards, int n, int leader, int winner)
{
    ui_fill(T->b.x, T->b.y, T->b.w, T->hand_top - T->b.y, 0, 0, 0, 90);
    ui_mark_gold(T->sx[winner], T->sy[winner], 2.6f * T->tok);
    for (int i = 0; i < n; i++) {
        int pl = (leader + i) % T->players;
        cards_draw(DECK_FRENCH, cards[i], T->tx[pl], T->ty[pl], T->wt, T->tdeg[pl], pl == winner ? CD_RAISED : 0);
    }
}

/* the trick pile: the tricks played this round, face down (tap it: the last trick) */
static inline void tk_draw_pile(const tk_table *T, int tricks)
{
    if (tricks > 0) cards_pile(DECK_FRENCH, T->px, T->py, T->pw, tricks, CARD_NONE, 0);
}

static inline int tk_on_pile(const tk_table *T, int tricks, float x, float y)
{
    return tricks > 0 && cards_in(T->px, T->py, T->pw * 1.5f, T->asp, x, y);
}

/* ---- the round's summary ----------------------------------------------------------------------------------------- */
/* the panel for `rows` rows over the table (dimmed): its left, its width, the first row's centre and the row height */
static inline void tk_summary(tk_gfx *g, const tk_table *T, int rows, float *x0, float *pw, float *y_row, float *rh)
{
    box b = T->b;
    float u = T->u;
    *rh = fminf(1.15f * u, b.h * 0.8f / rows);
    *pw = fminf(b.w * 0.94f, 8.0f * u);
    float ph = *rh * rows + 0.5f * u, y0 = b.y + (b.h - ph) / 2;
    *x0 = b.x + (b.w - *pw) / 2;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 80);
    tk_panel(g, *x0, y0, *pw, ph);
    *y_row = y0 + 0.25f * u + *rh / 2;
}

static inline void tk_row_line(float x0, float pw, float y, float u)
{
    ui_fill(x0 + 0.3f * u, y, pw - 0.6f * u, fmaxf(1, 0.02f * u), 246, 238, 220, 40);
}

/* ---- the bars ---------------------------------------------------------------------------------------------------- */
static inline float tk_number_w(int v, float h)    /* the width of ui_number_signed(v) at height h */
{
    int digits = 1;
    for (int a = abs(v); a >= 10; a /= 10) digits++;
    return (0.74f * (digits - 1) + 0.62f + (v < 0 ? 0.56f : 0)) * h;
}

/* the cards in hand (a small card back and their number, smaller), then the score; a narrow bar: the score alone */
static inline void tk_bar(box b, int cards, int score)
{
    float u = ui.unit, cy = b.y + b.h / 2, h = 0.46f * u, hc = 0.3f * u;
    float cw = 0.26f * u, wc = tk_number_w(cards, hc), need = cw + 0.08f * u + wc + 0.3f * u + tk_number_w(score, h);
    float x = b.x;
    if (b.w >= need) {
        cards_draw(DECK_FRENCH, CARD_NONE, x + cw / 2, cy, cw, 0, CD_DOWN | CD_FLAT);
        x += cw + 0.08f * u;
        ui_number(x + wc / 2, cy, hc, cards, 1);
        x += wc + 0.3f * u;
    }
    float w = tk_number_w(score, 1), hs = fminf(h, (b.x + b.w - x) / w * 0.95f);
    if (hs > 0) ui_number_signed(x + w * hs / 2, cy, hs, score, 1, 0);
}

/* ---- the hand, drawn and touched --------------------------------------------------------------------------------- */
/* the viewer's cards: each drawn how[i] (CD_*: CD_RAISED the one chosen), lifted lift[i] card widths in gold light
 * (cards just received; NULL: none); `skip` (a card dragged) and the cards still on their way left out */
static inline void tk_hand_draw(const tk_table *T, const tk_anim *a, const uint8_t *h, int n, const int *how,
                                const float *lift, int skip)
{
    cards_spot s[TK_HAND];
    tk_fan(T, n, s);
    for (int i = 0; i < n; i++) {
        if (h[i] == skip || tk_hides(a, TK_HAND_TAG, h[i])) continue;
        float y = s[i].y;
        if (lift && lift[i] > 0 && !(how[i] & CD_RAISED)) {
            y -= lift[i] * T->wh;
            ui_mark_gold(s[i].x, y - 0.42f * T->wh * T->asp, 0.8f * T->wh);
        }
        cards_draw(DECK_FRENCH, h[i], s[i].x, y, T->wh, s[i].deg, how[i]);
    }
}

/* the viewer's n cards face down (the hand-off cover is up: ui.viewer -1) */
static inline void tk_hand_backs(const tk_table *T, int n)
{
    cards_spot s[TK_HAND];
    if (n > TK_HAND) n = TK_HAND;
    tk_fan(T, n, s);
    for (int i = 0; i < n; i++) cards_draw(DECK_FRENCH, CARD_NONE, s[i].x, s[i].y, T->wh, s[i].deg, CD_DOWN);
}

/* the card under (x, y), the raised or lifted ones first (they stand out of the fan); CARD_NONE: none */
static inline int tk_hand_at(const tk_table *T, const uint8_t *h, int n, const int *how, const float *lift, float x,
                             float y)
{
    cards_spot s[TK_HAND];
    tk_fan(T, n, s);
    for (int i = 0; i < n; i++) {
        float up = how[i] & CD_RAISED ? 0.14f : lift && lift[i] > 0 ? lift[i] : 0;
        if (up > 0 && cards_hit(&s[i], 1, T->wh, T->asp, x, y + up * T->wh)) return h[i];
    }
    int i = cards_hit(s, n, T->wh, T->asp, x, y);
    return i < 0 ? CARD_NONE : h[i];
}

/* a finger on a card of the hand: a tap (it rises; tapped again, it goes) or a drag (released above the hand: it
 * goes to the middle) */
typedef struct {
    int card, on, raised;                       /* the card touched, dragged (moved far enough), the one risen */
    float x0, y0, x, y;
} tk_touch;
enum { TK_T_NONE, TK_T_TAP, TK_T_SEND };

static inline void tk_touch_reset(tk_touch *t)
{
    t->card = t->raised = CARD_NONE;
    t->on = 0;
}

static inline void tk_touch_down(tk_touch *t, int card, float x, float y)
{
    t->card = card;
    t->on = 0;
    t->x0 = t->x = x;
    t->y0 = t->y = y;
}

static inline void tk_touch_move(tk_touch *t, float x, float y)
{
    if (t->card == CARD_NONE) return;
    t->x = x;
    t->y = y;
    if (!t->on && hypotf(x - t->x0, y - t->y0) > 0.25f * ui.unit) t->on = 1;
}

/* the finger up: TK_T_TAP (a tap on *card), TK_T_SEND (*card dragged to the middle), TK_T_NONE */
static inline int tk_touch_up(tk_touch *t, const tk_table *T, float x, float y, int *card)
{
    *card = t->card;
    if (t->card == CARD_NONE) return TK_T_NONE;
    tk_touch_move(t, x, y);
    int r = !t->on ? TK_T_TAP : y < T->hand_top ? TK_T_SEND : TK_T_NONE;
    t->card = CARD_NONE;
    if (r != TK_T_SEND) t->on = 0;              /* sent: kept (the card slides on from the finger) */
    return r;
}

/* the dragged card under the finger */
static inline void tk_touch_draw(const tk_touch *t, const tk_table *T)
{
    if (t->card != CARD_NONE && t->on) cards_draw(DECK_FRENCH, t->card, t->x, t->y, T->wh, 0, CD_RAISED);
}

/* a tap on a card to play: it rises (0), or, risen already, it goes (1); one that cannot be played: SND_DENY */
static inline int tk_tap_play(tk_touch *t, int card, int legal)
{
    if (!legal) { ui_sound(SND_DENY); return 0; }
    if (t->raised == card) return 1;
    t->raised = card;
    ui_sound(SND_TAP);
    return 0;
}

/* where the risen card would go: its place in the trick, in gold light (tap it to play) */
static inline void tk_draw_target(const tk_table *T)
{
    ui_mark_gold(T->tx[T->me], T->ty[T->me], 1.6f * T->wt);
    cards_place(DECK_FRENCH, T->tx[T->me], T->ty[T->me], T->wt);
}

static inline int tk_on_target(const tk_table *T, float x, float y)
{
    return cards_in(T->tx[T->me], T->ty[T->me], T->wt * 1.2f, T->asp, x, y);
}

/* ---- the menu's card --------------------------------------------------------------------------------------------- */
/* n cards in a row, overlapping, the last one on top and a little higher */
static inline void tk_thumb(const int *cards, int n, int size, uint8_t *rgba)
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

/* ---- the viewer's cards moving ----------------------------------------------------------------------------------- */
#define TK_SLIDE_MS 300
#define TK_LOOK_MS 900                          /* a full trick stays a moment before its winner gathers it */
#define TK_DEAL_MS 260

/* the deal, as the viewer first sees it: their cards fly in from the middle and turn face up */
static inline void tk_deal(tk_anim *a, const tk_table *T, const uint8_t *h, int n)
{
    cards_spot s[TK_HAND];
    tk_fan(T, n, s);
    for (int i = 0; i < n; i++)
        tk_slide(a, h[i], T->cx, T->cy, T->wt, 0, 0, s[i].x, s[i].y, T->wh, s[i].deg, 1, TK_DEAL_MS, 50 * i,
                 TK_HAND_TAG, h[i], 0, i % 3 == 0 ? SND_MOVE : -1, 0);
}

/* the viewer plays `card`: from its place in their hand as it was shown (h, n; risen), or from the finger (dragged),
 * to their place in the trick */
static inline void tk_play(tk_anim *a, const tk_table *T, const tk_touch *f, const uint8_t *h, int n, int card)
{
    cards_spot s[TK_HAND];
    tk_fan(T, n, s);
    int i = 0, me = T->me;
    while (i < n && h[i] != card) i++;
    float x0 = i < n ? s[i].x : T->hx, y0 = i < n ? s[i].y - 0.14f * T->wh : T->hy, d0 = i < n ? s[i].deg : 0;
    if (f->on) x0 = f->x, y0 = f->y, d0 = 0;
    tk_slide(a, card, x0, y0, T->wh, d0, 1, T->tx[me], T->ty[me], T->wt, T->tdeg[me], 1, TK_SLIDE_MS, 0, TK_TRICK, me,
             0, SND_PLACE, 200);
}

#endif
