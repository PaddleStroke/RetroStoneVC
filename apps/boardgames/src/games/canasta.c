/*
 * Canasta in RetroStone Board: four players in two partnerships (or two players) passing the device round, each hand
 * secret (VIEW_HIDDEN; the rules: canasta_rules.c, canasta.h). The table, from the top: the other hands, small fans
 * of backs with their token and count; the other side's melds; the stock and the discard pile (a wild card in it
 * lies crosswise under it: frozen); the viewer's side's melds; the viewer's hand, sorted, in one or two rows. Each
 * side's melds go in a row of columns, one a rank, the cards cascading down; a canasta closes into a pile (a natural
 * one in gold light), its count under it; the side's tokens and red 3s at the row's left; while the side has not
 * melded, a dark disc at its right with the count its initial meld needs (and, beside it, what is laid this turn).
 * Taps: cards of the hand to choose them (they rise), then a meld of your side (onto it), your side's row (a new
 * meld), or the pile (to take it with them; or, after drawing, one card to discard it); the stock to draw. Dragging a
 * card does the same. What the chosen cards can do glows. At the end of a hand, a summary: each side's melds, canasta
 * bonuses, red 3s, going out, the cards left in the hands, the hand's points and the totals.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/canasta.h"
#include "games/cardtable.h"
#include "games/tricks_ui.h"
#include "shell.h"

#define SLIDE_MS 340
#define FLIP_MS 260
#define DEAL_MS 260
#define CASCADE 0.22f                           /* a meld's cards: each this much of a card's height below the last */

static cagame G;
static const int deck = DECK_FRENCH;
static int dealing, floor_n;                    /* floor_n: undo stops there (the last hand-off) */
static uint8_t selc[CA_ACT];                    /* the chosen cards (values: copies alike) */
static int nsel;
static struct { int on, moved, card; float x0, y0, x, y; } drag;
static ct_moves M;
static tk_gfx X;
static SDL_Texture *t_tok[MAX_PLAYERS];
static int tok_made;

enum { TAG_HAND = 1, TAG_PILE, TAG_MELD, TAG_RED3, TAG_STOCK };

static float aspect(void) { return cards_aspect(deck); }

/* ---- the table's geometry (it follows the viewer) ---------------------------------------------------------------- */
static struct {
    box b, others, zone[2], hand;               /* zone[0]: the other side's melds (top), zone[1]: the viewer's */
    int me, land;
    float u, pw, sx, sy, dx, dy;                /* the stock and the pile */
    float mw, colw[2];                          /* a meld card's width; each zone's column width */
} T;

static int my_side(void) { return ca_side(&G.pos, T.me); }
static int zone_of(int side) { return side == my_side() ? 1 : 0; }

static void layout(box b)
{
    const capos *p = &G.pos;
    T.b = b;
    T.u = ui.unit;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= p->players) T.me = 0;
    T.land = b.w > b.h * 1.05f;
    float pad = 0.15f * T.u;
    if (!T.land) {                              /* portrait: from the top down */
        float ho = b.h * (p->players == 2 ? 0.075f : 0.085f), hz = b.h * 0.225f, hm = b.h * 0.115f;
        float hh = b.h - ho - 2 * hz - hm;
        T.others = (box){b.x, b.y, b.w, ho};
        T.zone[0] = (box){b.x + pad, b.y + ho, b.w - 2 * pad, hz};
        T.zone[1] = (box){b.x + pad, b.y + ho + hz + hm, b.w - 2 * pad, hz};
        T.hand = (box){b.x + pad, b.y + b.h - hh, b.w - 2 * pad, hh};
        T.pw = floorf(fminf(hm * 0.86f / aspect(), b.w / 6.5f));
        float cy = b.y + ho + hz + hm / 2, cx = b.x + b.w / 2;
        T.sx = cx - 0.7f * T.pw - 0.15f * T.u, T.sy = cy;
        T.dx = cx + 0.7f * T.pw + 0.15f * T.u, T.dy = cy;
    } else {                                    /* landscape: the hand along the bottom; the melds on the left, the
                                                 * other hands and the piles on the right */
        float hh = b.h * 0.34f, top = b.h - hh, lw = b.w * 0.72f, rw = b.w - lw;
        T.zone[0] = (box){b.x + pad, b.y, lw - 2 * pad, top / 2};
        T.zone[1] = (box){b.x + pad, b.y + top / 2, lw - 2 * pad, top / 2};
        T.hand = (box){b.x + pad, b.y + top, b.w - 2 * pad, hh};
        T.others = (box){b.x + lw, b.y, rw, top * 0.36f};
        float mh = top * 0.64f, cy = b.y + top * 0.36f + mh / 2, cx = b.x + lw + rw / 2;
        T.pw = floorf(fminf(mh * 0.8f / aspect(), rw / 3.2f));
        T.sx = cx - 0.7f * T.pw - 0.1f * T.u, T.sy = cy;
        T.dx = cx + 0.7f * T.pw + 0.1f * T.u, T.dy = cy;
    }
}

/* ---- the melds' places ------------------------------------------------------------------------------------------- */
static const int RANK_ORDER[12] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 1, 3};

/* a side's melds in their order (ranks); their count */
static int melds_of(const capos *p, int s, int *ranks)
{
    int n = 0;
    for (int k = 0; k < 12; k++)
        if (p->mn[s][RANK_ORDER[k]]) ranks[n++] = RANK_ORDER[k];
    return n;
}

/* the columns: the side's tokens and red 3s first, then its melds, then a free place (a new meld) */
static void zone_measure(const capos *p)
{
    int r[12], most = 0;
    for (int s = 0; s < 2; s++) {
        int n = melds_of(p, s, r) + 2;
        most = n > most ? n : most;
    }
    float h = fminf(T.zone[0].h, T.zone[1].h);
    for (int z = 0; z < 2; z++) T.colw[z] = T.zone[z].w / (most > 9 ? most : 9);
    T.mw = floorf(fminf(fminf(T.colw[0], T.colw[1]) * 0.9f, h * 0.94f / (aspect() * (1 + 5 * CASCADE))));
}

static float col_x(int side, int col) { box z = T.zone[zone_of(side)]; return z.x + (col + 0.5f) * T.colw[zone_of(side)]; }
static float zone_top(int side) { return T.zone[zone_of(side)].y + 0.03f * T.zone[zone_of(side)].h + T.mw * aspect() / 2; }

static int order_of(int r)
{
    for (int k = 0; k < 12; k++)
        if (RANK_ORDER[k] == r) return k;
    return 12;
}

/* the column of a side's meld of rank r (one not there yet: where it will go, among the others in order) */
static int meld_col(const capos *p, int s, int r)
{
    int ranks[12], n = melds_of(p, s, ranks), k = 0;
    for (int i = 0; i < n; i++) {
        if (ranks[i] == r) return 1 + i;
        k += order_of(ranks[i]) < order_of(r);
    }
    return 1 + k;
}

/* the i-th card of a meld (a canasta: the pile) */
static void meld_spot(const capos *p, int s, int r, int i, float *x, float *y)
{
    float h = T.mw * aspect();
    *x = col_x(s, meld_col(p, s, r));
    int n = p->mn[s][r] > i ? p->mn[s][r] : i + 1;
    *y = zone_top(s) + (n >= 7 ? 0 : i * CASCADE * h);
}

static void red3_spot(int s, int i, float *x, float *y)
{
    float h = T.mw * aspect();
    *x = col_x(s, 0);
    *y = zone_top(s) + 0.42f * h + i * 0.24f * h;
}

/* ---- the viewer's hand, sorted ----------------------------------------------------------------------------------- */
static int sort_key(int c)
{
    if (CARD_SUIT(c) == SUIT_JOKER) return 100000 + CARD_RANK(c);
    int r = CARD_RANK(c), k = r == 2 ? 900 : r == 3 ? 800 : r == 1 ? 14 : r;   /* 4..K, A, black 3s, 2s, jokers */
    return k * 8 + CARD_SUIT(c);
}

static int sorted(const capos *p, int pl, uint8_t *h)
{
    int n = p->n[pl];
    memcpy(h, p->hand[pl], (size_t)n);
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && sort_key(h[j]) < sort_key(h[j - 1]); j--) {
            uint8_t t = h[j];
            h[j] = h[j - 1];
            h[j - 1] = t;
        }
    return n;
}

static float hand_wmax(void) { return floorf(fminf(T.hand.w / 6.6f, 1.45f * T.u)); }

/* the spots of n cards in the hand's box; the cards' width */
static float hand_spots(int n, cards_spot *sp) { return ct_hand(T.hand, n, aspect(), hand_wmax(), sp); }

/* the chosen copies in a sorted hand: the first copies of each chosen value */
static void chosen(const uint8_t *h, int n, uint8_t *flag)
{
    int left[256] = {0};
    for (int i = 0; i < nsel; i++) left[selc[i]]++;
    for (int i = 0; i < n; i++) flag[i] = left[h[i]] > 0 ? (left[h[i]]--, 1) : 0;
}

/* where a card of a player's hand is (the viewer's: its last copy, or the first when `first`; the others': the end of
 * their fan) */
static void seat_xy(int q, float *x, float *y, float *w, float *deg);
static void hand_spot(const capos *p, int pl, int card, int first, float *x, float *y, float *w, float *deg)
{
    if (pl != T.me) { seat_xy(pl, x, y, w, deg); return; }
    uint8_t h[CA_CARDS], fl[CA_CARDS];
    cards_spot sp[CA_CARDS];
    int n = sorted(p, pl, h);
    *w = hand_spots(n, sp);
    chosen(h, n, fl);
    int k = -1;
    for (int i = 0; i < n; i++)
        if (h[i] == card) {
            k = i;
            if (first) break;
        }
    if (k < 0) { *x = T.hand.x + T.hand.w / 2, *y = T.hand.y + T.hand.h / 2, *deg = 0; return; }
    *x = sp[k].x, *y = sp[k].y - (fl[k] ? 0.18f * *w : 0), *deg = sp[k].deg;
}

/* ---- the others' seats ------------------------------------------------------------------------------------------- */
static float seat_w(void) { return floorf(fminf(T.others.h * 0.62f / aspect(), 0.5f * T.u)); }

static void seat_xy(int q, float *x, float *y, float *w, float *deg)
{
    int k = G.pos.players - 1, i = ct_seat(q, T.me, G.pos.players);
    float cx = ct_seat_x(T.others.x, T.others.w, k, i), cy = T.others.y + T.others.h * 0.4f;
    *w = seat_w();
    ct_backs_end(deck, cx, cy, T.others.w / k * 0.7f, *w, G.pos.n[q], x, y, deg);
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
static void calm(void) { ct_calm(&M); }
static int busy(void) { return ct_busy(&M); }

static void slide(int card, int up0, float x0, float y0, float w0, float a0, int up1, float x1, float y1, float w1,
                  int ms, int delay, int tag, int a, int b, int sound)
{
    ct_slide(&M, deck, card, x0, y0, w0, a0, up0, x1, y1, w1, 0, up1, ms, delay, tag, a, b, sound);
}

/* the viewer's hand flying in from the stock (a new hand, once the cover is lifted) */
static void deal_slides(void)
{
    const capos *p = &G.pos;
    uint8_t h[CA_CARDS];
    cards_spot sp[CA_CARDS];
    int n = sorted(p, T.me, h);
    float w = hand_spots(n, sp);
    for (int i = 0; i < n; i++)
        slide(h[i], 0, T.sx, T.sy, T.pw, 0, 1, sp[i].x, sp[i].y, w, DEAL_MS, 40 * i, TAG_HAND, T.me, h[i],
              i % 3 == 0 ? SND_MOVE : -1);
}

/* the cards an action moved, from the position before it (b) to the one after */
static void animate(const capos *b, const ca_act *a)
{
    const capos *p = &G.pos;
    int pl = b->turn, s = ca_side(b, pl), up = pl == T.me, d = 0;
    float x, y, w, deg, mw = T.mw;
    zone_measure(p);
    switch (a->kind) {
    case CA_STOCK:
        for (int i = b->nred3[s]; i < p->nred3[s]; i++) {   /* red 3s: beside the melds */
            red3_spot(s, i, &x, &y);
            slide(p->red3[s][i], 0, T.sx, T.sy, T.pw, 0, 1, x, y, mw, SLIDE_MS, d, TAG_RED3, s, i, SND_PLACE);
            d += 220;
        }
        for (int i = 0; i < p->ndrew; i++) {
            hand_spot(p, pl, p->drew[i], 0, &x, &y, &w, &deg);
            slide(p->drew[i], 0, T.sx, T.sy, T.pw, 0, up, x, y, w, SLIDE_MS, d, TAG_HAND, pl, p->drew[i], SND_MOVE);
            d += 160;
        }
        break;
    case CA_TAKE:
    case CA_MELD_IT: {
        int r = a->kind == CA_TAKE ? CARD_RANK(b->pile[b->npile - 1]) : ca_meld_rank(a->card, a->n, a->rank);
        int at = b->mn[s][r];
        if (a->kind == CA_TAKE) {
            meld_spot(p, s, r, at, &x, &y);
            slide(b->pile[b->npile - 1], 1, T.dx, T.dy, T.pw, 0, 1, x, y, mw, SLIDE_MS, 0, TAG_MELD, s, r, SND_PLACE);
            at++;
            d = 80;
        }
        for (int i = 0; i < a->n; i++, at++) {
            float x0, y0, w0, deg0;
            hand_spot(b, pl, a->card[i], 1, &x0, &y0, &w0, &deg0);
            if (drag.on && drag.moved && a->card[i] == drag.card) x0 = drag.x, y0 = drag.y;
            meld_spot(p, s, r, at, &x, &y);
            slide(a->card[i], up, x0, y0, w0, deg0, 1, x, y, mw, SLIDE_MS, d, TAG_MELD, s, r, i ? -1 : SND_PLACE);
            d += 70;
        }
        if (a->kind == CA_TAKE) {               /* the rest of the pile into the hand (red 3s beside the melds) */
            int k = 0;
            for (int i = b->nred3[s]; i < p->nred3[s]; i++) {
                red3_spot(s, i, &x, &y);
                slide(p->red3[s][i], 1, T.dx, T.dy, T.pw, 0, 1, x, y, mw, SLIDE_MS, d + 200, TAG_RED3, s, i, SND_PLACE);
            }
            for (int i = b->npile - 2; i >= 0; i--) {
                int c = b->pile[i];
                if (ca_red3(c)) continue;
                hand_spot(p, pl, c, 0, &x, &y, &w, &deg);
                slide(c, 1, T.dx, T.dy, T.pw, 0, up, x, y, w, SLIDE_MS, d + 200 + 45 * k, TAG_HAND, pl, c,
                      k % 4 == 0 ? SND_MOVE : -1);
                k++;
            }
        }
        break;
    }
    case CA_DISCARD: {
        float x0, y0, w0, deg0;
        hand_spot(b, pl, a->card[0], 1, &x0, &y0, &w0, &deg0);
        if (drag.on && drag.moved) x0 = drag.x, y0 = drag.y;
        ct_slide(&M, deck, p->pile[p->npile - 1], x0, y0, w0, deg0, up, T.dx, T.dy, T.pw, ca_wild(a->card[0]) ? 90 : 0, 1,
                 SLIDE_MS, 0, TAG_PILE, 0, 0, SND_PLACE);   /* a wild card lands crosswise */
        break;
    }
    case CA_NEXT:
        dealing = 1;
        break;
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void unselect(void) { nsel = 0; }

static void start(int v)
{
    (void)v;
    ca_new(&G, ui.players, (uint32_t)ui_random(1 << 30));
    calm();
    unselect();
    drag.on = 0;
    floor_n = 0;
    dealing = 1;
}

static int status(void)
{
    if (G.pos.phase != CP_OVER) return GS_PLAY;
    int w = ca_winner(&G.pos);
    return w < 0 ? GS_DRAW : gs_win(w);         /* side 0 is player 0's, side 1 player 1's */
}

static unsigned winners(void) { return ca_winners(&G.pos); }
static int turn(void) { return G.pos.turn; }

static void act(const ca_act *a)
{
    capos before = G.pos;
    if (!ca_play(&G, a)) { ui_sound(SND_DENY); return; }
    calm();
    animate(&before, a);
    unselect();
    drag.on = 0;
    const capos *p = &G.pos;
    if ((p->phase == CP_DRAW || p->phase == CP_MELD) && p->turn != ui.viewer) {
        floor_n = G.n;
        ui_handoff(p->turn);                    /* the device passes (after the cards) */
    }
}

static int undo(void)
{
    if (G.n <= floor_n || !ca_undo(&G)) return 0;
    calm();
    unselect();
    dealing = 0;
    return 1;
}

static int script(const char *s)
{
    int ok = ca_script(&G, s);
    calm();
    unselect();
    dealing = 0;
    floor_n = G.n;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- what the chosen cards can do -------------------------------------------------------------------------------- */
static ca_act make(int kind, int rank)
{
    ca_act a;
    memset(&a, 0, sizeof a);
    a.kind = (uint8_t)kind;
    a.rank = (uint8_t)rank;
    a.n = (uint8_t)nsel;
    memcpy(a.card, selc, (size_t)nsel);
    return a;
}

static int my_turn(void)
{
    const capos *p = &G.pos;
    return (p->phase == CP_DRAW || p->phase == CP_MELD) && p->turn == T.me && !ui_handing() && ui.viewer >= 0;
}

static int can(int kind, int rank)
{
    ca_act a = make(kind, rank);
    return ca_legal(&G.pos, &a);
}

/* the pile: taken with the chosen cards (before drawing), or the chosen card discarded */
static int pile_act(ca_act *a)
{
    const capos *p = &G.pos;
    if (p->phase == CP_DRAW) *a = make(CA_TAKE, 0);
    else if (nsel == 1) *a = make(CA_DISCARD, 0);
    else return 0;
    return ca_legal(p, a);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void textures(void)
{
    ct_tokens(t_tok, &tok_made, (int)(0.5f * T.u));
    tk_tokens(&X, (int)(0.5f * T.u));
}

/* a side's row: its tokens and red 3s, its melds (the chosen cards' targets glowing), the free place, the count its
 * initial meld needs */
static void draw_zone(const capos *p, int s)
{
    float h = T.mw * aspect(), u = T.u, top = zone_top(s);
    int mine = s == my_side() && my_turn() && !busy(), z = zone_of(s);
    /* the tokens over the red 3s */
    float tk = fminf(0.42f * u, T.colw[z] * 0.46f), tx = col_x(s, 0), ty = top - h / 2 + tk / 2;
    for (int pl = 0, k = 0; pl < p->players; pl++) {
        if (ca_side(p, pl) != s) continue;
        int two = p->players == 4;
        float x = tx + (two ? (k ? 0.27f : -0.27f) * T.colw[z] : 0);
        if (p->turn == pl && (p->phase == CP_DRAW || p->phase == CP_MELD)) ui_mark_gold(x, ty, 1.7f * tk);
        ui_put_c(t_tok[pl], x, ty, tk, tk);
        k++;
    }
    for (int i = 0; i < p->nred3[s]; i++) {
        float x, y;
        if (ct_hides(&M, TAG_RED3, s, i)) continue;
        red3_spot(s, i, &x, &y);
        cards_draw(deck, p->red3[s][i], x, y, T.mw, 0, 0);
    }
    /* the melds */
    int ranks[12], n = melds_of(p, s, ranks), wild_only = nsel > 0;
    for (int i = 0; i < nsel; i++) wild_only &= ca_wild(selc[i]);
    for (int k = 0; k < n; k++) {
        int r = ranks[k], m = p->mn[s][r] - ct_hides(&M, TAG_MELD, s, r);
        float x = col_x(s, 1 + k), y;
        int glow = mine && nsel && can(CA_MELD_IT, wild_only ? r : 0) && (wild_only || ca_meld_rank(selc, nsel, 0) == r);
        if (m <= 0) continue;
        if (p->mn[s][r] >= 7 && m >= 7) {       /* a canasta: a pile, a natural card on top (red if it can: natural;
                                                 * black: mixed), a wild card crosswise under a mixed one */
            int wild = CARD_NONE, nat = CARD_NONE, natural = 1;
            for (int i = 0; i < p->mn[s][r]; i++) {
                int c = p->meld[s][r][i];
                if (ca_wild(c)) { natural = 0; if (wild == CARD_NONE) wild = c; continue; }
                int red = CARD_SUIT(c) == SUIT_HEARTS || CARD_SUIT(c) == SUIT_DIAMONDS;
                if (nat == CARD_NONE || red == natural) nat = c;
            }
            y = top;
            if (natural) ui_mark_gold(x, y, 1.9f * T.mw);
            else cards_draw(deck, wild, x, y + 0.1f * h, T.mw, 90, 0);
            cards_pile(deck, x, y, T.mw, p->mn[s][r], nat, glow ? CD_GLOW : 0);
            float d = 0.62f * T.mw;
            tk_put(&X, TK_DISC_DARK, x, y + h / 2 + 0.36f * d, d, 0, 255, 255, 255, 255);
            ui_number(x, y + h / 2 + 0.36f * d, 0.5f * d, p->mn[s][r], 1);
            continue;
        }
        for (int i = 0; i < m; i++) {
            meld_spot(p, s, r, i, &x, &y);
            cards_draw(deck, p->meld[s][r][i], x, y, T.mw, 0, glow && i == m - 1 ? CD_GLOW : 0);
        }
    }
    /* the free place: a new meld of the chosen cards */
    if (mine && nsel && !wild_only) {
        int r = ca_meld_rank(selc, nsel, 0);
        if (r > 0 && !p->mn[s][r] && can(CA_MELD_IT, 0)) {
            float x = col_x(s, 1 + n);
            cards_place(deck, x, top, T.mw);
            cards_draw(deck, CARD_NONE, x, top, T.mw * 0.98f, 0, CD_GLOW | CD_DOWN | CD_DIM);
        }
    }
    /* the initial meld: the count it needs, and what is laid this turn */
    int need = ca_need(p, s);
    if (need) {
        box zb = T.zone[z];
        float d = fminf(0.8f * u, zb.h * 0.36f), x = zb.x + zb.w - d / 2 - 0.05f * u, y = zb.y + zb.h / 2;
        int v = ca_meld_points(p, s);
        tk_put(&X, TK_DISC_DARK, x, y, d, 0, 255, 255, 255, 255);
        ui_number(x, y, 0.38f * d, need, 1);
        if (v) {
            float x2 = x - 1.05f * d;
            if (v >= need) ui_mark_gold(x2, y, 1.6f * d);
            tk_put(&X, TK_DISC_DARK, x2, y, d, 0, 255, 255, 255, 255);
            ui_number(x2, y, 0.38f * d, v, 1);
        }
    }
}

/* the other players' hands: small fans of backs along the top, each with its token and count */
static void draw_others(const capos *p)
{
    int k = p->players - 1;
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) continue;
        int i = ct_seat(q, T.me, p->players);
        float cx = ct_seat_x(T.others.x, T.others.w, k, i), cy = T.others.y + T.others.h * 0.4f, w = seat_w();
        int n = p->n[q] - ct_hides(&M, TAG_HAND, q, -1);
        ct_backs(deck, cx, cy, T.others.w / k * 0.7f, w, n, 0);
        float lh = fminf(0.4f * T.u, T.others.h * 0.3f);
        if (p->turn == q && (p->phase == CP_DRAW || p->phase == CP_MELD))
            ui_mark_gold(cx - (1.15f * lh + 0.62f * lh * (n >= 10 ? 2 : 1)) / 2 + lh / 2, T.others.y + T.others.h - lh * 0.6f,
                         1.8f * lh);
        ct_label(t_tok[q], cx, T.others.y + T.others.h - lh * 0.6f, lh, n);
    }
}

static void draw_piles(const capos *p)
{
    int mine = my_turn() && !busy(), ph = p->phase;
    ca_act a;
    int stock_ok = mine && ph == CP_DRAW && can(CA_STOCK, 0) && !nsel;
    cards_pile(deck, T.sx, T.sy, T.pw, p->nstock, CARD_NONE, stock_ok ? CD_GLOW : 0);
    int np = p->npile - ct_hides(&M, TAG_PILE, -1, -1);
    int top = np > 0 ? p->pile[np - 1] : CARD_NONE;
    int glow = mine && pile_act(&a);
    if (np > 0 && ca_wild(top)) {               /* a wild card on top: crosswise on the pile under it */
        cards_pile(deck, T.dx, T.dy, T.pw, np - 1, np > 1 ? p->pile[np - 2] : CARD_NONE, 0);
        cards_draw(deck, top, T.dx, T.dy, T.pw, 90, glow ? CD_GLOW : 0);
    } else {                                    /* frozen: the wild card that froze it crosswise under the pile */
        if (p->frozen && p->freezer != CA_NONE && np > 0) cards_draw(deck, p->freezer, T.dx, T.dy, T.pw, 90, 0);
        cards_pile(deck, T.dx, T.dy, T.pw, np, top, glow ? CD_GLOW : 0);
    }
    if (p->npile > 0) {                         /* how many cards the pile holds */
        float d = fminf(0.5f * T.pw, 0.62f * T.u), x = T.dx + T.pw * 0.5f, y = T.dy + T.pw * aspect() / 2;   /* its corner */
        tk_put(&X, TK_DISC_DARK, x, y, d, 0, 255, 255, 255, 255);
        ui_number(x, y, 0.5f * d, p->npile, 1);
    }
}

static void draw_hand(const capos *p)
{
    uint8_t h[CA_CARDS], fl[CA_CARDS];
    cards_spot sp[CA_CARDS];
    int n = sorted(p, T.me, h);
    float w = hand_spots(n, sp);
    int covered = ui.viewer < 0, dragging = drag.on && drag.moved && !covered;
    chosen(h, n, fl);
    int coming[256] = {0}, after[256] = {0};    /* the copies still on their way: the last ones left out */
    for (int i = 0; i < M.n; i++)
        if (M.m[i].tag == TAG_HAND && M.m[i].a == T.me && cards_slide_busy(&M.m[i].s)) coming[M.m[i].b & 255]++;
    for (int i = 0; i < n; i++) after[h[i]]++;
    for (int i = 0; i < n; i++) {               /* left to right (the first row behind the second) */
        int c = h[i];
        after[c]--;
        if (after[c] < coming[c]) continue;
        if (dragging && fl[i]) continue;        /* the chosen cards go with the finger */
        int how = covered ? CD_DOWN : fl[i] ? CD_RAISED : 0;
        cards_draw(deck, covered ? CARD_NONE : c, sp[i].x, sp[i].y - (how & CD_RAISED ? 0.18f * w : 0), w, sp[i].deg,
                   how);
    }
}

/* the end of a hand: a row a score item (a picture), a column a side (its tokens at the head); the hand's points,
 * then the totals, the leader's in gold light */
static void draw_item(int row, float cx, float cy, float d)
{
    float w = 0.42f * d;
    switch (row) {
    case 0:                                     /* the melds: three cards fanned */
        for (int i = -1; i <= 1; i++)
            cards_draw(deck, CARD(i == 0 ? SUIT_HEARTS : SUIT_SPADES, RANK_KING), cx + i * 0.26f * d, cy + fabsf((float)i) *
                       0.04f * d, w, i * 12.0f, CD_FLAT);
        break;
    case 1:                                     /* the canastas: a pile */
        ui_mark_gold(cx, cy, 1.1f * d);
        cards_pile(deck, cx, cy, w, 7, CARD(SUIT_HEARTS, 7), 0);
        break;
    case 2: cards_draw(deck, CARD(SUIT_HEARTS, 3), cx, cy, w, 0, CD_FLAT); break;   /* the red 3s */
    case 3: tk_put(&X, TK_ARROW, cx, cy, 0.7f * d, 0, 250, 214, 90, 255); break;   /* going out */
    case 4:                                     /* the cards left in the hands: backs */
        for (int i = -1; i <= 1; i++)
            cards_draw(deck, CARD_NONE, cx + i * 0.2f * d, cy, w, i * 10.0f, CD_DOWN | CD_FLAT);
        break;
    }
}

static void draw_summary(const capos *p)
{
    box b = T.b;
    float u = T.u, rh = fminf(0.86f * u, b.h * 0.88f / 8.6f), pw = fminf(b.w * 0.94f, 7.2f * u);
    float ph = rh * 8.4f, x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 80);
    tk_panel(&X, x0, y0, pw, ph);
    float cx[2] = {x0 + pw * 0.53f, x0 + pw * 0.82f}, ix = x0 + pw * 0.17f, y = y0 + 0.2f * rh + rh / 2;
    float tk = fminf(0.5f * u, 0.62f * rh), nh = fminf(0.38f * u, 0.46f * rh);
    int first = my_side(), lead = p->score[0] > p->score[1] ? 0 : p->score[1] > p->score[0] ? 1 : -1;
    for (int c = 0; c < 2; c++) {               /* the heads: each side's tokens */
        int s = c ? 1 - first : first, k = 0;
        for (int pl = 0; pl < p->players; pl++) {
            if (ca_side(p, pl) != s) continue;
            float x = cx[c] + (p->players == 4 ? (k ? 0.36f : -0.36f) * tk * 1.5f : 0);
            if (p->out == pl) ui_mark_gold(x, y, 1.8f * tk);
            ui_put_c(t_tok[pl], x, y, tk, tk);
            k++;
        }
    }
    y += rh;
    for (int row = 0; row < 5; row++, y += rh) {
        draw_item(row, ix, y, rh);
        for (int c = 0; c < 2; c++) {
            const ca_score *r = &p->res[c ? 1 - first : first];
            int v = row == 0 ? r->melds : row == 1 ? r->bonus : row == 2 ? r->red3 : row == 3 ? r->out : -r->minus;
            if (v || row == 0) ui_number_signed(cx[c], y, nh, v, 1, 1);
            else ui_fill(cx[c] - 0.12f * u, y, 0.24f * u, fmaxf(1, 0.03f * u), 246, 238, 220, 90);
        }
    }
    tk_row_line(x0, pw, y - rh / 2, u);         /* the hand's points */
    for (int c = 0; c < 2; c++) ui_number_signed(cx[c], y, nh * 1.1f, p->res[c ? 1 - first : first].total, 1, 1);
    y += rh * 1.2f;
    for (int c = 0; c < 2; c++) {               /* the totals */
        int s = c ? 1 - first : first;
        float d = 1.15f * rh;
        if (lead == s) ui_mark_gold(cx[c], y, 1.7f * d);
        tk_put(&X, TK_DISC_DARK, cx[c], y, d, 0, 255, 255, 255, 255);
        ui_number_signed(cx[c], y, 0.34f * d, p->score[s], 1, 0);
    }
    float d = 0.9f * rh;                        /* the goal beside the totals */
    ui_number(ix, y, 0.32f * d, CA_GOAL, 1);
}

static void draw(void)
{
    const capos *p = &G.pos;
    if (dealing && ui.viewer >= 0) { dealing = 0; deal_slides(); }   /* the cover lifted: the hand flies in */
    textures();
    zone_measure(p);
    draw_others(p);
    for (int s = 0; s < 2; s++) draw_zone(p, s);
    draw_piles(p);
    draw_hand(p);
    ct_draw(&M, NULL);
    if (drag.on && drag.moved && ui.viewer >= 0) {   /* the dragged card(s) under the finger */
        uint8_t h[CA_CARDS];
        cards_spot sp[CA_CARDS];
        float w = hand_spots(sorted(p, T.me, h), sp);
        int k = 0, own = 0;
        for (int i = 0; i < nsel; i++) {        /* the others chosen fanned behind it, then the card held */
            if (selc[i] == drag.card && !own) { own = 1; continue; }
            k++;
        }
        for (int i = 0, j = 0, skip = 1; i < nsel; i++) {
            if (selc[i] == drag.card && skip) { skip = 0; continue; }
            cards_draw(deck, selc[i], drag.x - (k - j) * 0.2f * w, drag.y, w, 0, CD_RAISED);
            j++;
        }
        cards_draw(deck, drag.card, drag.x, drag.y, w, 0, CD_RAISED);
    }
    if ((p->phase == CP_HAND || p->phase == CP_OVER) && !busy()) draw_summary(p);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int hand_hit(float x, float y)           /* the sorted hand's card under the finger (-1) */
{
    const capos *p = &G.pos;
    uint8_t h[CA_CARDS], fl[CA_CARDS];
    cards_spot sp[CA_CARDS];
    int n = sorted(p, T.me, h);
    float w = hand_spots(n, sp);
    chosen(h, n, fl);
    for (int i = 0; i < n; i++)
        if (fl[i]) sp[i].y -= 0.18f * w;
    int i = cards_hit(sp, n, w, aspect(), x, y);
    return i < 0 ? -1 : h[i];
}

static int sel_has(int c)
{
    for (int i = 0; i < nsel; i++)
        if (selc[i] == c) return i;
    return -1;
}

/* a copy of c chosen or unchosen: the finger was on a chosen copy (raised) or not */
static void toggle(int c, int was_chosen)
{
    int i = sel_has(c);
    if (was_chosen && i >= 0) {
        selc[i] = selc[--nsel];
        ui_sound(SND_TAP);
    } else if (nsel < CA_ACT) {
        selc[nsel++] = (uint8_t)c;
        ui_sound(SND_TAP);
    } else ui_sound(SND_DENY);
}

/* was the card under (x, y) a chosen copy */
static int hit_chosen(float x, float y)
{
    const capos *p = &G.pos;
    uint8_t h[CA_CARDS], fl[CA_CARDS];
    cards_spot sp[CA_CARDS];
    int n = sorted(p, T.me, h);
    float w = hand_spots(n, sp);
    chosen(h, n, fl);
    for (int i = 0; i < n; i++)
        if (fl[i]) sp[i].y -= 0.18f * w;
    int i = cards_hit(sp, n, w, aspect(), x, y);
    return i >= 0 && fl[i];
}

static int in_box(box b, float x, float y) { return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h; }

/* a tap (or a drop) on the table with the chosen cards: the stock, the pile, a meld or the row of the viewer's side */
static void use(float x, float y)
{
    const capos *p = &G.pos;
    ca_act a;
    if (cards_in(T.sx, T.sy, T.pw * 1.15f, aspect(), x, y)) {
        a = make(CA_STOCK, 0);
        a.n = 0;
        if (ca_legal(p, &a)) act(&a);
        else ui_sound(SND_DENY);
        return;
    }
    if (cards_in(T.dx, T.dy, T.pw * 1.25f, aspect(), x, y)) {
        if (pile_act(&a)) act(&a);
        else ui_sound(SND_DENY);
        return;
    }
    int s = my_side();
    box z = T.zone[1];
    if (in_box(z, x, y) && nsel) {
        int ranks[12], n = melds_of(p, s, ranks), r = 0;
        int col = (int)((x - z.x) / T.colw[1]) - 1;
        if (col >= 0 && col < n) r = ranks[col];
        a = make(CA_MELD_IT, 0);
        if (r && ca_legal(p, (a.rank = (uint8_t)r, &a))) { act(&a); return; }
        a.rank = 0;
        if (ca_legal(p, &a)) { act(&a); return; }
    }
    ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    const capos *p = &G.pos;
    drag.on = 0;
    if (ui_handing() || p->phase == CP_OVER) return;
    if (p->phase == CP_HAND) {
        if (busy()) calm();                     /* a tap hurries the end on */
        else {
            ca_act a = {CA_NEXT, 0, 0, {0}};
            act(&a);
        }
        return;
    }
    if (p->turn != T.me || busy()) { ui_sound(SND_DENY); return; }
    int c = hand_hit(x, y);
    if (c >= 0) {
        drag.on = 1, drag.card = c, drag.moved = 0;
        drag.x0 = drag.x = x, drag.y0 = drag.y = y;
        return;
    }
    use(x, y);
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x, drag.y = y;
    if (!drag.moved && hypotf(x - drag.x0, y - drag.y0) > 0.3f * ui.unit) {
        drag.moved = 1;
        if (sel_has(drag.card) < 0 && nsel < CA_ACT) selc[nsel++] = (uint8_t)drag.card;   /* dragged: chosen too */
    }
}

static void release(float x, float y)
{
    if (!drag.on || ui_handing()) { drag.on = 0; return; }
    if (!drag.moved) {
        toggle(drag.card, hit_chosen(drag.x0, drag.y0));
        drag.on = 0;
        return;
    }
    if (y < T.hand.y) use(x, y);               /* dropped on the table */
    drag.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { tk_bar(b, G.pos.n[player], G.pos.score[ca_side(&G.pos, player)]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: a canasta closed (a pile of sevens), a meld of kings with a joker cascading down, a red 3 */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    int cw = size * 28 / 100, ch = (int)(cw * 1.4f);
    uint8_t *c = malloc((size_t)cw * ch * 4);
    if (!c) return;
    static const int meld[4] = {CARD(SUIT_HEARTS, RANK_KING), CARD(SUIT_SPADES, RANK_KING),
                                CARD(SUIT_DIAMONDS, RANK_KING), CARD(SUIT_JOKER, 1)};
    float step = ch * 0.26f, y0 = (size - ch - 3 * step) / 2;
    for (int k = 4; k >= 0; k--) {              /* the canasta */
        cards_paint(DECK_FRENCH, CARD(SUIT_HEARTS, 7), cw, ch, c);
        art_blend(rgba, size, size, c, cw, ch, (int)(size * 0.04f + k * size * 0.006f), (int)(y0 + k * size * 0.012f));
    }
    for (int i = 0; i < 4; i++) {               /* the meld */
        cards_paint(DECK_FRENCH, meld[i], cw, ch, c);
        art_blend(rgba, size, size, c, cw, ch, (int)(size * 0.36f), (int)(y0 + i * step));
    }
    cards_paint(DECK_FRENCH, CARD(SUIT_DIAMONDS, 3), cw, ch, c);   /* a red 3 */
    art_blend(rgba, size, size, c, cw, ch, (int)(size * 0.68f), (int)(y0 + 2 * step));
    free(c);
}

static const game canasta_game = {
    .id = "canasta", .order = 313, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 4, .players_usual = 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(canasta_game)
