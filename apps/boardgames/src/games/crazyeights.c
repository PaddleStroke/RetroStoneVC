/*
 * Crazy Eights in RetroStone Board: the shedding game for 2 to 6 players passing the device round, with the 52 cards
 * (Crazy Eights) or the action deck (Last Card). The rules: crazyeights_rules.c, crazyeights.h. Each hand is secret
 * (VIEW_HIDDEN): the player to move sees their hand big at the bottom, the others' hands as small fans of backs along
 * the top (a count under each), the stock and the discard in the middle, inside a ring of arrows showing the direction
 * of play (Last Card, three players or more). Taps only: a card that glows to play it, the stock to draw, a suit (an
 * ace) or a colour (a disc) after an eight or a wild, the "last card" button (a card with a burst) before playing
 * the second last card, a player's fan or bar to catch them; the pass button in the bar. A round's summary: the
 * points left in each hand, the winner's gain, the totals.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/cardtable.h"
#include "games/crazyeights.h"

#define SLIDE_MS 300
#define DEAL_MS 220

static cgame G;
static int dealing;                             /* the deal flies at the next frame (the view set) */
static int caught = -1;                         /* the player just caught (a red glow on their hand a moment) */
static Uint32 caught_t;
static ct_moves MV;
enum { H_HAND = 1, H_SEAT, H_PILE, H_STOCK };

/* ---- the table's geometry (it follows the viewer) ---------------------------------------------------------------- */
static struct {
    box b;
    int me, k, wide;
    float sy, sw, sfan;                         /* the others' seats: the fans' centre, a card's width, a fan's width */
    float wp, sx, cy, dx;                       /* the stock and the discard: width, the stock's x, y, the discard's */
    float ring, rx;                             /* the direction ring's diameter, its centre x */
    float bx, by, br;                           /* the "last card" button */
    float ox[4], oy, ow;                        /* the choice of a suit (colour) */
    box hb;                                     /* the hand's box */
} T;

static float seat_x(int q) { return ct_seat_x(T.b.x, T.b.w, T.k, ct_seat(q, T.me, G.pos.players)); }

static void layout(box b)
{
    const cpos *p = &G.pos;
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= p->players) T.me = 0;
    T.k = p->players - 1;
    T.wide = b.w > b.h * 1.05f;
    float hs = b.h * (T.wide ? 0.2f : 0.18f), hm = b.h * (T.wide ? 0.36f : 0.32f), u = ui.unit;
    T.sw = floorf(fminf(fminf(hs * 0.62f / 1.4f, b.w / T.k * 0.3f), 0.85f * u));
    T.sfan = fminf(b.w / T.k * 0.86f, T.sw * 3.2f);
    T.sy = b.y + hs * 0.42f;
    T.cy = b.y + hs + hm / 2;
    T.wp = floorf(fminf(hm * 0.62f / 1.4f, b.w * 0.2f));
    T.rx = b.x + b.w / 2;
    T.sx = T.rx - 0.68f * T.wp;
    T.dx = T.rx + 0.68f * T.wp;
    T.ring = fminf(hm * 0.98f, fminf(b.w * 0.62f, 3.6f * T.wp));
    T.br = 0.62f * u;
    T.bx = b.x + b.w - T.br - 0.25f * u;
    T.by = T.cy + hm * 0.5f - T.br - 0.1f * u;
    if (T.wide) T.bx = T.rx + T.ring / 2 + 1.2f * T.br, T.by = T.cy;
    T.ow = floorf(fminf(b.w / 5.2f, hm * 0.7f / 1.4f));
    for (int s = 0; s < 4; s++) T.ox[s] = T.rx + (s - 1.5f) * T.ow * 1.18f;
    T.oy = T.cy;
    T.hb = (box){b.x, b.y + hs + hm, b.w, b.h - hs - hm};
}

/* the viewer's hand: its cards' width and spots, for a hand of n */
static float hand_spots(int n, cards_spot *s)
{
    return ct_hand(T.hb, n, 1.4f, fminf(T.b.w * 0.27f, 2.4f * ui.unit), s);
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
static int busy(void) { return ct_busy(&MV); }

static void seat_end(int q, int count, float *x, float *y, float *deg)
{
    ct_backs_end(G.pos.deck, seat_x(q), T.sy, T.sfan, T.sw, count, x, y, deg);
}

/* the cards a player gained (from the stock), flying to their hand or their seat; from: the hand before */
static int gains(const cpos *b, int q, int delay)
{
    const cpos *p = &G.pos;
    int deck = p->deck, k = 0;
    if (q == T.me) {
        cards_spot s[CE_HAND];
        float w = hand_spots(p->n[q], s);
        uint8_t used[CE_HAND] = {0};
        for (int i = 0; i < b->n[q]; i++)       /* the cards it had: matched, the others are new */
            for (int j = 0; j < p->n[q]; j++)
                if (!used[j] && p->hand[q][j] == b->hand[q][i]) { used[j] = 1; break; }
        for (int j = 0; j < p->n[q]; j++) {
            if (used[j]) continue;
            ct_slide(&MV, deck, p->hand[q][j], T.sx, T.cy, T.wp, 0, 0, s[j].x, s[j].y, w, s[j].deg, 1, SLIDE_MS + 60,
                     delay + 140 * k, H_HAND, q, j, SND_MOVE);
            k++;
        }
        return k;
    }
    int add = p->n[q] - b->n[q];
    for (int i = 0; i < add; i++) {
        float x, y, a;
        seat_end(q, b->n[q] + i + 1, &x, &y, &a);
        ct_slide(&MV, deck, CARD_NONE, T.sx, T.cy, T.wp, 0, 0, x, y, T.sw, a, 0, SLIDE_MS, delay + 140 * i, H_SEAT, q,
                 -1, SND_MOVE);
    }
    return add > 0 ? add : 0;
}

/* the deal: the cards fly to every hand one at a time, then the starter turns over */
static void deal_slides(void)
{
    const cpos *p = &G.pos;
    int deck = p->deck, each = p->n[0], step = p->players * each > 30 ? 45 : 70, k = 0;
    for (int q = 0; q < p->players; q++)
        if (p->n[q] > each) each = p->n[q];
    cards_spot s[CE_HAND];
    float w = hand_spots(p->n[T.me], s);
    for (int c = 0; c < each; c++)
        for (int i = 1; i <= p->players; i++, k++) {
            int q = (p->dealer + i) % p->players;
            if (c >= p->n[q]) continue;
            if (q == T.me) {
                ct_slide(&MV, deck, p->hand[q][c], T.sx, T.cy, T.wp, 0, 0, s[c].x, s[c].y, w, s[c].deg, 1, DEAL_MS + 80,
                         k * step, H_HAND, q, c, k % 3 ? -1 : SND_MOVE);
            } else {
                float x, y, a;
                seat_end(q, c + 1, &x, &y, &a);
                ct_slide(&MV, deck, CARD_NONE, T.sx, T.cy, T.wp, 0, 0, x, y, T.sw, a, 0, DEAL_MS, k * step, H_SEAT, q,
                         -1, k % 3 ? -1 : SND_MOVE);
            }
        }
    ct_slide(&MV, deck, p->pile[0], T.sx, T.cy, T.wp, 0, 0, T.dx, T.cy, T.wp, 0, 1, SLIDE_MS, k * step + 120, H_PILE,
             -1, -1, SND_PLACE);
}

/* what an action moved, from the position before it */
static void animate(const cpos *b, int act)
{
    const cpos *p = &G.pos;
    int k = CA_KIND(act), pl = b->turn, deck = p->deck, delay = 0;
    if (k == CA_NEXT) { deal_slides(); return; }
    if (p->reshuffles != b->reshuffles) {       /* the discard shuffled into a new stock */
        for (int i = 0; i < 5; i++)
            ct_slide(&MV, deck, CARD_NONE, T.dx, T.cy, T.wp, 0, 0, T.sx, T.cy, T.wp, 0, 0, SLIDE_MS, 60 * i, H_STOCK,
                     -1, -1, i ? -1 : SND_MOVE);
        delay = 520;
    }
    if (k == CA_PLAY) {
        int card = CA_ARG(act), i = 0;
        while (i < b->n[pl] && b->hand[pl][i] != card) i++;
        if (pl == T.me) {
            cards_spot s[CE_HAND];
            float w = hand_spots(b->n[pl], s);
            ct_slide(&MV, deck, card, s[i].x, s[i].y, w, s[i].deg, 1, T.dx, T.cy, T.wp, 0, 1, SLIDE_MS, 0, H_PILE, -1,
                     -1, SND_PLACE);
        } else {
            float x, y, a;
            seat_end(pl, b->n[pl], &x, &y, &a);
            ct_slide(&MV, deck, card, x, y, T.sw, a, 0, T.dx, T.cy, T.wp, 0, 1, SLIDE_MS, 0, H_PILE, -1, -1, SND_PLACE);
        }
        delay += SLIDE_MS;
    }
    for (int q = 0; q < p->players; q++)
        if (p->n[q] > b->n[q]) delay += 140 * gains(b, q, delay);
    if (k == CA_CATCH) { ui_sound(SND_CAPTURE); caught = b->exposed; caught_t = ui_ms(); }
    if (k == CA_CALL || k == CA_CHOOSE) ui_sound(SND_TAP);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    ce_new(&G, v == CE_LAST ? CE_LAST : CE_EIGHTS, ui.players, (uint32_t)ui_random(1 << 30));
    ct_calm(&MV);
    dealing = 1;
}

static int status(void)
{
    int w = ce_winner(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    cpos before = G.pos;
    if (!ce_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&MV);
    animate(&before, a);
    if (G.pos.phase == CP_PLAY || G.pos.phase == CP_CHOOSE) ui_handoff(G.pos.turn);   /* after the cards land */
}

static int undo(void)
{
    if (!ce_undo(&G)) return 0;
    ct_calm(&MV);
    dealing = 0;
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = ce_script(&G, s);
    ct_calm(&MV);
    dealing = 0;
    ui_handoff(G.pos.turn);
    return ok;
}

static int can_pass(void) { return G.pos.turn == T.me && !busy() && ce_legal(&G.pos, CA_PASS); }
static void pass(void) { act(CA_PASS); }

/* ---- pictures of our own --------------------------------------------------------------------------------------- */
/* the "last card" icon: a single card standing, a burst over its corner (the digit 1 goes on at drawing) */
static void last_icon(int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    art_style st = {{246, 238, 220}, {40, 30, 28}, {40, 30, 28}, {40, 30, 28}, 0.1f, 0.05f};
    art_fig *f = art_fig_new();
    const float card[] = {0.24f, 0.3f, 0.6f, 0.24f, 0.7f, 0.84f, 0.34f, 0.9f};
    art_poly(f, ART_PART, card, 4);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
    art_style gold = {{250, 206, 84}, {120, 84, 20}, {120, 84, 20}, {120, 84, 20}, 0.2f, 0.03f};
    f = art_fig_new();
    for (int i = 0; i < 3; i++) {               /* three rays from the card's top corner */
        float a = (20 + 35 * i) * 3.14159265f / 180, cx = 0.66f, cy = 0.22f;
        float x0 = cx + 0.1f * cosf(a), y0 = cy - 0.1f * sinf(a), x1 = cx + 0.24f * cosf(a), y1 = cy - 0.24f * sinf(a);
        float nx = -sinf(a) * 0.035f, ny = -cosf(a) * 0.035f;
        const float ray[] = {x0 + nx * 0.4f, y0 + ny * 0.4f, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx * 0.4f,
                             y0 - ny * 0.4f};
        art_poly(f, ART_PART, ray, 4);
    }
    uint8_t *b = calloc((size_t)size * size, 4);
    if (b) {
        art_render(f, &gold, size, size, (float)size, 0, 0, b);
        art_blend(rgba, size, size, b, size, size, 0, 0);
        free(b);
    }
    art_fig_free(f);
}

/* the direction ring: two arcs of arrows going clockwise (the play goes from the bottom to the top left, the top
 * right, and back: the order of the bars) */
static void ring_pic(int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    art_style st = {{236, 226, 196}, {236, 226, 196}, {236, 226, 196}, {236, 226, 196}, 0, 0};
    art_fig *f = art_fig_new();
    const float r0 = 0.43f, r1 = 0.47f, pi = 3.14159265f;
    for (int h = 0; h < 2; h++) {
        float a0 = 180.0f * h + 160, a1 = 180.0f * h + 38;   /* clockwise: from a0 down to a1 */
        art_arc(f, ART_PART, 0.5f, 0.5f, r0, r1, a1, a0);
        float a = a1 * pi / 180, rm = (r0 + r1) / 2, px = 0.5f + rm * cosf(a), py = 0.5f - rm * sinf(a);
        float tx = sinf(a), ty = cosf(a);       /* the clockwise tangent (y down) */
        float nx = cosf(a), ny = -sinf(a), s = 0.07f;
        const float head[] = {px + tx * s * 1.3f, py + ty * s * 1.3f, px + nx * s, py + ny * s, px - nx * s,
                              py - ny * s};
        art_poly(f, ART_PART, head, 3);
    }
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_tok[MAX_PLAYERS], *t_icon, *t_ring, *t_disc, *t_disc_on, *t_panel;
static int tok_made, icon_made, ring_made, disc_made, panel_w, panel_h;

static void textures(void)
{
    int s = (int)(1.3f * T.br);
    if (icon_made != s && s > 4) {
        icon_made = s;
        ui_free(&t_icon);
        last_icon(s, ui_buf(s, s));
        t_icon = ui_tex(s, s, ui_buf(s, s));
    }
    int r = (int)T.ring;
    if (ring_made != r && r > 8) {
        ring_made = r;
        ui_free(&t_ring);
        ring_pic(r, ui_buf(r, r));
        t_ring = ui_tex(r, r, ui_buf(r, r));
        SDL_SetTextureAlphaMod(t_ring, 120);
    }
    int d = (int)(2 * T.br);
    if (disc_made != d && d > 4) {
        disc_made = d;
        static const uint8_t off[3] = {54, 72, 64}, on[3] = {150, 118, 46};
        ui_free(&t_disc);
        art_disc(d, off, 1, 0, ui_buf(d, d));
        t_disc = ui_tex(d, d, ui_buf(d, d));
        ui_free(&t_disc_on);
        art_disc(d, on, 1, 0, ui_buf(d, d));
        t_disc_on = ui_tex(d, d, ui_buf(d, d));
    }
}

/* the "last card" icon with its 1 (turned with the card), size s */
static void put_icon(float cx, float cy, float s)
{
    ui_put_c(t_icon, cx, cy, s, s);
    ui_number_rot(cx + 0.02f * s, cy + 0.07f * s, 0.34f * s, 1, 0, -6);
}

/* the round's scores: a row a player, their token, the points left in their hand (the winner: what they gained, in
 * gold light), their total */
static void draw_summary(void)
{
    const cpos *p = &G.pos;
    box b = T.b;
    float u = ui.unit, rh = fminf(1.05f * u, b.h * 0.8f / p->players), pw = fminf(b.w * 0.92f, 7.6f * u);
    float ph = rh * p->players + 0.5f * u, x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    if (panel_w != (int)pw || panel_h != (int)ph) {
        panel_w = (int)pw;
        panel_h = (int)ph;
        const uint8_t c[4] = {28, 34, 32, 246};
        ui_free(&t_panel);
        art_roundrect(panel_w, panel_h, 0.4f * u, c, ui_buf(panel_w, panel_h));
        t_panel = ui_tex(panel_w, panel_h, ui_buf(panel_w, panel_h));
    }
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 70);
    ui_put(t_panel, x0, y0, pw, ph);
    for (int q = 0; q < p->players; q++) {
        float cy = y0 + 0.25f * u + (q + 0.5f) * rh, tx = x0 + 0.75f * u, d = fminf(0.72f * rh, 0.8f * u);
        if (q == p->winner) ui_mark_gold(tx, cy, 2.0f * d);
        ui_put_c(t_tok[q], tx, cy, d, d);
        float nh = 0.42f * rh, rx = x0 + pw * 0.45f;
        if (q == p->winner) {
            ui_mark_gold(rx, cy, 1.6f * nh);
            ui_number_signed(rx, cy, nh, p->gain, 1, 1);
        } else {                                /* the points left: a small card and its count */
            cards_draw(p->deck, CARD_NONE, rx - 1.1f * nh, cy, 0.62f * nh, 0, CD_DOWN | CD_FLAT);
            ui_number(rx + 0.25f * nh, cy, 0.8f * nh, p->left[q], 1);
        }
        ui_number(x0 + pw * 0.8f, cy, 0.56f * rh, p->score[q], 1);
        if (q < p->players - 1)
            ui_fill(x0 + 0.3f * u, cy + rh / 2, pw - 0.6f * u, fmaxf(1, 0.02f * u), 246, 238, 220, 40);
    }
}

/* a card of the hand can be played (else the stock glows) */
static int can_play(void)
{
    const cpos *p = &G.pos;
    for (int i = 0; i < p->n[p->turn]; i++)
        if (ce_playable(p, p->hand[p->turn][i])) return 1;
    return 0;
}

static int top_special(const cpos *p)
{
    int t = p->pile[p->npile - 1];
    return p->variant == CE_LAST ? CARD_SUIT(t) == SUIT_JOKER : CARD_RANK(t) == 8;
}

/* a suit's (a colour's) picture: an ace of that suit; the colour's disc (red lacquer, jade, amber, lapis are the
 * players 2..5's colours) */
static void put_suit(int s, float cx, float cy, float w, int how)
{
    if (G.pos.variant == CE_LAST) {
        if (how & CD_GLOW) ui_mark_gold(cx, cy, 1.5f * w);
        ui_put_c(t_tok[2 + s], cx, cy, w, w);
    } else {
        cards_draw(DECK_FRENCH, CARD(s, RANK_ACE), cx, cy, w, 0, how);
    }
}

static void draw(void)
{
    const cpos *p = &G.pos;
    if (dealing && ui.viewer >= 0) { dealing = 0; ct_calm(&MV); deal_slides(); }   /* the cover lifted: it flies */
    textures();
    int deck = p->deck, mine = p->turn == T.me && !ui_handing(), moving = ct_moving(&MV), b = busy();
    int ph = p->phase, ready = mine && !moving && (ph == CP_PLAY || ph == CP_CHOOSE);
    ct_tokens(t_tok, &tok_made, (int)fmaxf(8, fmaxf(0.5f * T.sw, T.ow * 0.8f)));
    /* the ring (last card, three or more): which way the play goes */
    if (p->variant == CE_LAST && p->players > 2 && t_ring) {
        SDL_FRect d = {T.rx - T.ring / 2, T.cy - T.ring / 2, T.ring, T.ring};
        SDL_RenderCopyExF(ui.r, t_ring, NULL, &d, 0, NULL, p->dir ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }
    /* the stock and the discard (the cards on their way not yet there) */
    int ns = p->nstock + ct_hides(&MV, H_STOCK, -1, -1);
    cards_pile(deck, T.sx, T.cy, T.wp, ns, CARD_NONE, ready && ce_legal(p, CA_DRAW) && !can_play() ? CD_GLOW : 0);
    int np = p->npile - ct_hides(&MV, H_PILE, -1, -1);
    cards_pile(deck, T.dx, T.cy, T.wp, np, np > 0 ? p->pile[np - 1] : CARD_NONE, 0);
    if (np == p->npile && top_special(p) && ph == CP_PLAY)    /* the suit named */
        put_suit(p->suit, T.dx + 0.5f * T.wp, T.cy + 0.45f * T.wp * 1.4f, 0.48f * T.wp, CD_GLOW);
    /* the others: a fan of backs each, the count under it, the "last card" icon when they called it */
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) continue;
        int n = p->n[q] - ct_hides(&MV, H_SEAT, q, -1);
        float x = seat_x(q), ch = T.sw * 1.4f;
        if (q == caught && ui_ms() - caught_t < 1200) { ui_mark_glow(x, T.sy, 2.4f * T.sw); ui_animate(); }
        ct_backs(deck, x, T.sy, T.sfan, T.sw, n, 0);
        if (n == 0) cards_place(deck, x, T.sy, T.sw);
        ct_label(t_tok[q], x, T.sy + 0.5f * ch + 0.32f * T.sw + 0.12f * ch, 0.48f * T.sw, n);
        if (p->variant == CE_LAST && p->called[q] && p->n[q] == 1)
            put_icon(x + 0.85f * T.sw, T.sy - 0.25f * ch, 0.9f * T.sw);
    }
    /* the hand */
    int n = p->n[T.me];
    cards_spot s[CE_HAND];
    float w = hand_spots(n, s);
    for (int i = 0; i < n; i++) {
        if (ct_hides(&MV, H_HAND, T.me, i)) continue;
        int c = p->hand[T.me][i], how = ready && ph == CP_PLAY && ce_playable(p, c) ? CD_GLOW : 0;
        if (ui.viewer < 0) c = CARD_NONE, how = CD_DOWN;   /* the hand-off cover: face down */
        cards_draw(deck, c, s[i].x, s[i].y, w, s[i].deg, how);
    }
    /* "last card": the button, while it matters (two cards, or one called) */
    if (p->variant == CE_LAST && mine && ph == CP_PLAY && (n == 2 || (n == 1 && p->called[T.me]))) {
        int can = !moving && ce_legal(p, CA_CALL), on = p->called[T.me];
        if (can) ui_mark_gold(T.bx, T.by, 3.0f * T.br);
        ui_put_c(on ? t_disc_on : t_disc, T.bx, T.by, 2 * T.br, 2 * T.br);
        put_icon(T.bx, T.by, 1.3f * T.br);
    }
    ct_draw(&MV, NULL);
    /* an eight or a wild played: the suits (colours) to name */
    if (ph == CP_CHOOSE && mine && !moving) {
        float h = T.ow * 1.4f * 1.25f;
        ui_fill(T.b.x, T.oy - h / 2, T.b.w, h, 0, 0, 0, 110);
        for (int k = 0; k < 4; k++) put_suit(k, T.ox[k], T.oy, T.ow, CD_GLOW);
    }
    if ((ph == CP_ROUND || ph == CP_OVER) && !b) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int seat_at(float x, float y)
{
    const cpos *p = &G.pos;
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) continue;
        float sx = seat_x(q);
        if (fabsf(x - sx) < T.b.w / T.k / 2 && fabsf(y - T.sy) < T.sw * 1.4f * 0.8f + 0.3f * T.sw) return q;
    }
    return -1;
}

static void press(float x, float y)
{
    const cpos *p = &G.pos;
    if (p->phase == CP_ROUND) { if (!busy()) act(CA_NEXT); return; }
    if (p->phase == CP_OVER || ct_moving(&MV)) return;
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    if (p->phase == CP_CHOOSE) {
        for (int k = 0; k < 4; k++)
            if (fabsf(x - T.ox[k]) < T.ow * 0.55f && fabsf(y - T.oy) < T.ow * 0.75f) { act(CA_CHOOSE | k); return; }
        ui_sound(SND_DENY);
        return;
    }
    int n = p->n[T.me];
    if (p->variant == CE_LAST && (n == 2 || n == 1) && hypotf(x - T.bx, y - T.by) < T.br * 1.1f) {
        act(CA_CALL);
        return;
    }
    int q = seat_at(x, y);
    if (q >= 0) {                               /* a player's hand: catch them */
        act(q == p->exposed ? CA_CATCH : 0);
        return;
    }
    if (cards_in(T.sx, T.cy, T.wp * 1.1f, 1.4f, x, y)) { act(CA_DRAW); return; }
    cards_spot s[CE_HAND];
    float w = hand_spots(n, s);
    int i = cards_hit(s, n, w, 1.4f, x, y);
    if (i >= 0) { act(CA_PLAY | p->hand[T.me][i]); return; }
    ui_sound(SND_DENY);
}

/* a player's bar: catch them (they have one card, not called) */
static int bar_press(int player, float x, float y)
{
    (void)x, (void)y;
    const cpos *p = &G.pos;
    if (player == T.me || p->phase != CP_PLAY || ct_moving(&MV) || p->turn != T.me) return 0;
    act(player == p->exposed ? CA_CATCH : 0);
    return 1;
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the player's total */
{
    int v = G.pos.score[player], digits = 1;
    for (int a = v; a >= 10; a /= 10) digits++;
    float h = fminf(0.46f * ui.unit, b.w / (0.74f * digits) * 0.95f);
    ui_number(b.x + 0.37f * digits * h, b.y + b.h / 2, h, v, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's and the picker's cards: a hand of cards in a cascade, an eight (a wild) in front */
static void thumb(int v, int size, uint8_t *rgba)
{
    static const int eights[4] = {CARD(SUIT_CLUBS, 4), CARD(SUIT_HEARTS, 12), CARD(SUIT_DIAMONDS, 9),
                                  CARD(SUIT_SPADES, 8)};
    static const int action[4] = {CARD(1, 7), CARD(3, ACTION_REVERSE), CARD(2, ACTION_DRAW2), ACTION_WILD};
    int deck = v == CE_LAST ? DECK_ACTION : DECK_FRENCH;
    const int *cards = v == CE_LAST ? action : eights;
    float w = size * 0.56f;
    int cw = (int)w, ch = (int)(w * 1.4f);
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    memset(rgba, 0, (size_t)size * size * 4);
    for (int i = 0; i < 4; i++) {
        cards_paint(deck, cards[i], cw, ch, b);
        art_blend(rgba, size, size, b, cw, ch, (int)(size * (0.03f + 0.12f * i)), (int)(size * (0.19f - 0.05f * i)));
    }
    free(b);
}

static const game crazyeights_game = {
    .id = "crazyeights", .order = 280, .variants = CE_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .bar_press = bar_press,
    .players = 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6, .view = VIEW_HIDDEN,
};
REGISTER_GAME(crazyeights_game)
