/*
 * The four-handed trick games (Doppelkopf, Schafkopf, Jass, Klaverjas): the screen's side they share, on top of the
 * kit's card table (cards_table.h). Static inline, as their rules' trick4.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * - The table: the viewer's hand fanned at the bottom, the four seats round the middle (the partner across), the trick
 *   with each card towards its player, the trick pile (tap it: the last trick), points rising from a seat.
 * - The viewer's turn to play: the cards that may be played glow, the others are dimmed; a tap raises a card, a second
 *   tap (or its place in the trick, or a drag there) plays it. A double pack holds two equal cards: only one of them
 *   rises or leaves the hand.
 * - The pictures of their games and calls, kept at the size drawn: a card (a called ace, the queen of clubs), a pair
 *   of cards (a marriage), the house's chess figures on an ivory disc (the queens' and the jacks' games), an arrow up
 *   or down (top-down, bottom-up), the four aces (no trumps); badges in a dark disc (a call made, a word said).
 * - The summaries: the players' chips side by side, the list of deals.
 */
#ifndef TRICK4_UI_H
#define TRICK4_UI_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/chess.h"
#include "games/trick4.h"
#include "shell.h"

typedef struct {
    ct_table T;
    ct_moves A;
    ct_touch F;
    ct_floats fl;                               /* points rising */
    int floor_n;                                /* undo stops at this length of the history (the last hand-off) */
    int show_last;                              /* the last trick shown over the table */
    unsigned seen;                              /* the players who have seen their cards fly in this deal (bit n) */
} tk4_ui;

static const uint8_t TK4_GOLD[3] = {250, 214, 90};

static inline void tk4u_reset(tk4_ui *U)
{
    U->show_last = 0;
    ct_touch_reset(&U->F);
}

static inline void tk4u_new(tk4_ui *U)
{
    ct_calm(&U->A);
    U->fl.n = 0;
    tk4u_reset(U);
    U->floor_n = 0;
    U->seen = 0;
}

static inline void tk4u_layout(tk4_ui *U, box b, int max_hand) { ct_layout(&U->T, b, TK4_P, ui_bottom(), max_hand); }

/* the viewer's hand as shown (h) and how each card is drawn: while the viewer is to play (live), glowing if it may be
 * played (ok), else dimmed; the raised one (by its index: a double pack holds two equal cards) */
static inline int tk4u_hand(const tk4_ui *U, const tk4_table *t, int live, int playing, int (*ok)(int card), uint8_t *h,
                            int *how)
{
    int me = U->T.me, n = t->n[me], mine = live && playing && tk4_turn(t) == me;
    for (int i = 0; i < n; i++) {
        int c = t->hand[me][i];
        h[i] = (uint8_t)c;
        how[i] = mine ? (ok(c) ? CD_GLOW : CD_DIM) : 0;
        if (ct_is_raised(&U->F, i)) how[i] = CD_RAISED;
    }
    return n;
}

/* the viewer's first look at a deal (the cover lifted): the cards fly in, when it is still the deal's start */
static inline void tk4u_look(tk4_ui *U, const tk4_table *t, int start)
{
    if ((U->seen >> U->T.me & 1) || ui_handing()) return;
    U->seen |= 1u << U->T.me;
    if (!start) return;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    ct_deal(&U->A, &U->T, h, tk4u_hand(U, t, 0, 0, NULL, h, how));
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* the middle: the trick pile, the trick, the raised card's place */
static inline void tk4u_draw_middle(const tk4_ui *U, const tk4_table *t, int live, int playing)
{
    ct_draw_pile(&U->T, t->tricks);
    ct_draw_trick(&U->T, &U->A, t->trick, NULL, t->ntrick, t->leader);
    if (U->F.raised != CARD_NONE && live && playing) ct_draw_target(&U->T);
}

/* the viewer's hand (backs under the hand-off cover; none: cards 0), the cards on their way, the finger, the points
 * rising, the last trick */
static inline void tk4u_draw_hand(tk4_ui *U, const tk4_table *t, int live, int playing, int (*ok)(int card), int hide,
                                  int none)
{
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = none ? 0 : tk4u_hand(U, t, live, playing, ok, h, how);
    if (hide) ct_hand_backs(&U->T, n);
    else ct_hand_draw(&U->T, &U->A, h, n, how, NULL, ct_dragged(&U->F));
    ct_draw(&U->A);
    if (!hide) ct_touch_draw(&U->F, &U->T);
    const uint8_t pill[4] = {24, 30, 28, 255};
    ct_floats_draw(&U->fl, ct_round((int)(1.5f * ui.unit), (int)(0.56f * ui.unit), 0.28f * ui.unit, pill), NULL);
    if (U->show_last && t->last_winner != TK4_NONE)
        ct_draw_last(&U->T, t->last, NULL, TK4_P, t->last_leader, t->last_winner);
}

/* ---- the finger -------------------------------------------------------------------------------------------------- */
/* the start of a press: the last trick put away, or the cards still moving (1: the press is spent) */
static inline int tk4u_press_first(tk4_ui *U)
{
    U->F.card = CARD_NONE;
    U->F.on = 0;
    if (U->show_last) { U->show_last = 0; return 1; }
    return ct_busy(&U->A);
}

/* the trick pile tapped: the last trick shown (1) */
static inline int tk4u_press_pile(tk4_ui *U, const tk4_table *t, float x, float y)
{
    if (!ct_on_pile(&U->T, t->tricks, x, y)) return 0;
    U->show_last = 1;
    ui_sound(SND_TAP);
    return 1;
}

/* a press while the viewer plays: the raised card's place (its card goes: send), or a card of the hand touched */
static inline void tk4u_press_card(tk4_ui *U, const tk4_table *t, int (*ok)(int card), void (*send)(int card), float x,
                                   float y)
{
    if (U->F.raised != CARD_NONE && ct_on_target(&U->T, x, y)) { send(U->F.raised); return; }
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    ct_touch_hand(&U->F, &U->T, h, tk4u_hand(U, t, 1, 1, ok, h, how), how, NULL, x, y);
}

static inline void tk4u_motion(tk4_ui *U, float x, float y) { ct_touch_move(&U->F, x, y); }

/* the finger lifted: a card dragged up to the table, or tapped once risen, goes (send) */
static inline void tk4u_release(tk4_ui *U, int (*ok)(int card), void (*send)(int card), float x, float y)
{
    int c, r = ct_touch_up(&U->F, &U->T, x, y, &c);
    if (r == CT_T_SEND) send(c);
    else if (r == CT_T_TAP && ct_tap_play(&U->F, c, ok(c))) send(c);
    U->F.on = 0;
}

/* the viewer's card played (from the hand as it was: h, n): it slides to the trick; a full trick then goes to its
 * winner */
static inline void tk4u_played(tk4_ui *U, const tk4_table *after, int tricks_before, int card, const uint8_t *h, int n)
{
    ct_play(&U->A, &U->T, &U->F, h, n, card);
    if (after->tricks != tricks_before)
        ct_gather(&U->A, &U->T, after->last, NULL, TK4_P, after->last_leader, after->last_winner,
                  CT_SLIDE_MS + CT_LOOK_MS, CT_SLIDE_MS);
}

/* points rising from a player's seat once the trick has gone to them (roem, a fox caught...) */
static inline void tk4u_float(tk4_ui *U, int pl, int value, int badge)
{
    ct_float(&U->fl, &U->A, U->T.sx[pl], U->T.sy[pl] - 0.8f * U->T.tok, value, badge,
             ui_ms() + CT_SLIDE_MS + CT_LOOK_MS + 300);
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* kinds: 0..3 a suit's sign (the kit's); TK4_PIC_CARD + card: one card; TK4_PIC_PAIR + card: two of it; TK4_PIC_FIG +
 * a chess piece: the figure on an ivory disc; an arrow up, down; the four aces */
enum { TK4_PIC_CARD = 0x100, TK4_PIC_PAIR = 0x200, TK4_PIC_FIG = 0x300, TK4_PIC_UP = 0x400, TK4_PIC_DOWN,
       TK4_PIC_ACES };

static inline void tk4_blend_card(uint8_t *rgba, int s, int card, float cx, float cy, float w, float deg)
{
    cards_paint_turned(rgba, s, DECK_FRENCH, card, cx, cy, w, deg);
}

static inline void tk4_paint(int kind, int s, uint8_t *rgba)
{
    static const uint8_t ivory[3] = {244, 236, 216}, ink[3] = {44, 40, 36};
    float asp = cards_aspect(DECK_FRENCH);
    memset(rgba, 0, (size_t)s * s * 4);
    if (kind >= TK4_PIC_CARD && kind < TK4_PIC_FIG) {
        int card = kind & 0xff, pair = kind >= TK4_PIC_PAIR;
        float w = 0.9f * s / asp * (pair ? 0.86f : 1);
        if (pair) {
            tk4_blend_card(rgba, s, card, 0.5f * s - 0.17f * w, 0.5f * s + 0.02f * s, w, -12);
            tk4_blend_card(rgba, s, card, 0.5f * s + 0.17f * w, 0.5f * s - 0.02f * s, w, 12);
        } else
            tk4_blend_card(rgba, s, card, 0.5f * s, 0.5f * s, w, 0);
        return;
    }
    if (kind == TK4_PIC_ACES) {                 /* the four aces fanned */
        static const int suit[4] = {SUIT_CLUBS, SUIT_HEARTS, SUIT_SPADES, SUIT_DIAMONDS};
        float w = 0.62f * s / asp;
        for (int i = 0; i < 4; i++)
            tk4_blend_card(rgba, s, CARD(suit[i], RANK_ACE), 0.5f * s + (i - 1.5f) * 0.2f * s,
                           0.54f * s + fabsf(i - 1.5f) * 0.03f * s, w, (i - 1.5f) * 14);
        return;
    }
    art_disc(s, ivory, 1, 0, rgba);
    if (kind >= TK4_PIC_FIG && kind < TK4_PIC_UP) {   /* a chess figure in amber */
        int f = (int)(0.8f * s);
        uint8_t *b = calloc((size_t)f * f, 4);
        if (b) {
            art_piece_style(kind & 0xff, &art_player[4], f, b);
            art_blend(rgba, s, s, b, f, f, (s - f) / 2, (s - f) / 2);
            free(b);
        }
    } else {                                    /* an arrow up or down */
        art_fig *a = art_fig_new();
        float d = kind == TK4_PIC_UP ? -1 : 1;
        art_band(a, ART_PART, 0.5f, 0.5f - 0.29f * d, 0.5f, 0.5f + 0.02f * d, 0.15f);
        art_head(a, ART_PART, 0.5f, 0.5f + 0.17f * d, d < 0 ? 270 : 90, 0.38f);
        art_fig_paint(rgba, s, a, ink, NULL, 0, 0);
    }
    cards_disc_cut(rgba, s);
    cards_disc_rim(rgba, s);
}

/* a picture (a suit's sign below 4) d wide */
static inline SDL_Texture *tk4u_tex(int kind, float d)
{
    int s = (int)(d + 0.5f);
    if (s < 4) s = 4;
    return kind < 4 ? ct_suit(DECK_FRENCH, kind, s) : ct_picture(tk4_paint, kind, s);
}

static inline void tk4u_pic(int kind, float x, float y, float d, Uint8 alpha)
{
    SDL_Texture *t = tk4u_tex(kind, d);
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put_c(t, x, y, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

/* a red stroke across (no, against, lost) */
static inline void tk4u_strike(float x, float y, float d) { ct_pic(CT_SLASH, x, y, d, 0, 230, 70, 60, 255); }

/* badges in a dark disc: a shell icon in gold (ICON_HINT: a reservation), a gold tick (yes, I play) */
static inline void tk4u_icon_badge(int icon, float x, float y, float d)
{
    ct_pic(CT_DISC_DARK, x, y, d, 0, 255, 255, 255, 255);
    int s = (int)(0.62f * d);
    ui_put_c(ct_icon(icon, s < 2 ? 2 : s, TK4_GOLD[0], TK4_GOLD[1], TK4_GOLD[2]), x, y, (float)s, (float)s);
}

static inline void tk4u_tick_badge(float x, float y, float d)
{
    ct_pic(CT_DISC_DARK, x, y, d, 0, 255, 255, 255, 255);
    ct_pic(CT_TICK, x, y, 0.7f * d, 0, TK4_GOLD[0], TK4_GOLD[1], TK4_GOLD[2], 255);
}

/* a small card lying flat (a call, a mark) */
static inline void tk4u_card(int card, float x, float y, float h, int how)
{
    cards_draw(DECK_FRENCH, card, x, y, h / cards_aspect(DECK_FRENCH), 0, CD_FLAT | how);
}

/* ---- the summaries ----------------------------------------------------------------------------------------------- */
/* the chips of the players in `who` (bit n: player n) side by side from (x, y), d wide, overlapping a little; dimmed
 * (alpha); the width they take */
static inline float tk4u_chips(unsigned who, float x, float y, float d, Uint8 alpha)
{
    float cx = x;
    int k = 0;
    for (int q = 0; q < TK4_P; q++) {
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

/* the list of deals in rows of `per` from y (centres): for each deal played the chips of its takers (who[i]; 0: thrown
 * in, an empty ring) in gold light when they made it (made[i]), dimmed when not; a dot for each deal to come */
static inline void tk4u_deal_list(float x0, float pw, float y, float rh, int N, int per, int done, const uint8_t *who,
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
        for (int q = 0; q < TK4_P; q++) k += who[i] >> q & 1;
        float d = k > 1 ? 0.8f * cs : cs, w = d + (k - 1) * 0.5f * d;
        if (made[i]) ui_mark_gold(cx, cy, 1.7f * cs);
        float x = cx - w / 2 + d / 2;
        for (int q = 0; q < TK4_P; q++) {
            if (!(who[i] >> q & 1)) continue;
            SDL_Texture *t = ct_token(q, (int)d);
            SDL_SetTextureAlphaMod(t, made[i] ? 255 : 110);
            ui_put_c(t, x, cy - (k > 1 ? 0.12f * d : 0) + (x > cx ? 0.24f * d : 0), d, d);
            SDL_SetTextureAlphaMod(t, 255);
            x += 0.5f * d;
        }
    }
}

/* a total right-aligned at r (below zero: a minus) */
static inline void tk4u_total(float r, float y, float h, int v)
{
    float w = ct_number_w(v, h, 0);
    ui_number_signed(r - w / 2, y, h, v, 1, 0);
}

#endif
