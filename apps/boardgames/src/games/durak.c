/*
 * Durak in RetroStone Board: the Russian game of attack and defence for 2 to 6 players passing the device round (the
 * rules: durak_rules.c, durak.h), the throw-in game (podkidnoy) or the transfer game (perevodnoy), each with secret
 * hands (VIEW_HIDDEN: the cover at each hand-off) or open hands (variants DK_OPEN on: every hand face up, the others'
 * over their backs along the top, no cover, undo free; game.hidden() tells the shell). Along the top the others in
 * turn order: a small fan of backs each, their chip and count, a sword by
 * the attacker, a shield by the defender, a pass sign once they have passed in this round of throwers. In the middle
 * the talon with the trump card across under it (the trump's sign once the talon is gone), the bout (each attacking
 * card with its beater across it, three to a row upright, six in a row on a wide screen) and the cards beaten off.
 * The viewer's hand along the bottom, by suit, the trumps last.
 * Attacking or throwing in: tap cards (they rise; those that may be thrown in glow), then tap the table or drag them
 * there; the pass button in the bar passes. Defending: tap a card (it rises, the attacking cards it beats glow), then
 * the card to beat (or the card again when it beats only one; or drag it there); the round button by the bout takes
 * the cards (it stays lit for all while the throwers add theirs). Perevodnoy: a card of the attack's rank lights an
 * empty place beside the bout: tap it to pass the attack on.
 * The hot seat: the device goes to the defender after each attack or throw-in; once all is beaten, to each thrower in
 * turn (the attacker first, then the others round the table, the defender left out), who throws in or passes; when
 * all have passed, the bout is beaten off. After a "take", each thrower has one turn to add cards, then the defender
 * picks them all up.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/durak.h"
#include "shell.h"

#define SLIDE_MS 300
#define DEAL_MS 240
#define LOOK_MS 700                             /* a bout stays a moment before it is swept off or taken */

enum { H_HAND = 1, H_SEAT, H_BOUT, H_BITO, H_TRUMP };
enum { PIC_SWORD, PIC_SHIELD };

static dgame G;
static ct_moves MV = {.rest = 300};
static Uint32 trump_go;                        /* when the trump card leaves the talon's bottom (drawn last) */
static int dealing, mark;                       /* dealing: the deal flies once the view is set; mark: undo floor */
static uint64_t chosen;                         /* the cards chosen (bit dur_index) */
static int raised = CARD_NONE;                  /* the defender's card risen */
static ct_drag drag;
static int open_hands;                          /* the hands face up for all (the variants from DK_OPEN) */

#define DK_OPEN DUR_VARIANTS                    /* variants: the rules' two, then the same two with open hands */

/* ---- the table's geometry (it follows the viewer) ---------------------------------------------------------------- */
static struct {
    box b, hb;                                  /* the table; the hand's box */
    int me, wide, cols;
    float asp, u;
    ct_others O;                                /* the others along the top */
    float wp, wt;                               /* the bout's cards, the talon's and the cards beaten off */
    float tx, ty, trx;                          /* the talon, the trump card across under it */
    float dx, dy;                               /* the cards beaten off */
    float bx[DUR_FULL], by[DUR_FULL];           /* the bout's places */
    float kx, ky, kd;                           /* the take button */
    float mid_top, mid_bot;
} T;

static void layout(box b)
{
    const dpos *p = &G.pos;
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= p->players) T.me = 0;
    T.u = ui.unit;
    T.asp = cards_aspect(DECK_FRENCH);
    T.wide = b.w > b.h * 1.05f;
    T.cols = 3;
    float u = T.u, hs = b.h * (T.wide ? 0.2f : 0.16f), hh = b.h * (T.wide ? 0.28f : 0.3f), hm = b.h - hs - hh;
    ct_others_layout(&T.O, b, DECK_FRENCH, p->players, T.me, hs);
    T.mid_top = b.y + hs;
    T.mid_bot = b.y + hs + hm;
    const int rows = 2;
    const float rowh = 1.28f;                   /* a place's height, in card heights (the beater across it) */
    float x0, x1, top;
    if (T.wide) {
        /* the talon at the left (the trump across under it), the cards beaten off at the right, the take button
         * under them; the bout between */
        T.wp = floorf(fminf(b.w / (T.cols * 1.42f + 3.6f), hm / (rows * rowh * T.asp + 0.2f)));
        T.wt = floorf(0.85f * T.wp);
        float cy = T.mid_top + hm / 2;
        T.tx = b.x + 0.62f * T.wt + 0.1f * u;
        T.ty = cy - 0.3f * T.wt * T.asp;
        T.dx = b.x + b.w - 0.62f * T.wt - 0.2f * u;
        T.kx = T.dx;
        x0 = b.x + 2.0f * T.wt + 0.2f * u, x1 = b.x + b.w - 1.4f * T.wt - 0.3f * u;
        top = cy - rows * rowh * T.wp * T.asp / 2;
    } else {
        /* a row along the top of the middle: the talon, the take button, the cards beaten off; the bout under it */
        T.wp = floorf(fminf(b.w * 0.94f / (T.cols * 1.42f), (hm - 0.3f * u) / ((0.72f + rows * rowh) * T.asp + 0.1f)));
        T.wt = floorf(0.72f * T.wp);
        T.tx = b.x + 0.62f * T.wt + 0.15f * u;
        T.ty = T.mid_top + 0.5f * T.wt * T.asp + 0.1f * u;
        T.dx = b.x + b.w - 0.62f * T.wt - 0.2f * u;
        T.kx = b.x + b.w / 2;
        x0 = b.x + 0.03f * b.w, x1 = b.x + 0.97f * b.w;
        top = T.ty + 0.5f * T.wt * T.asp + 0.15f * u;
        top += fmaxf(0, (T.mid_bot - top - rows * rowh * T.wp * T.asp) / 2);
    }
    T.trx = T.tx + 0.62f * T.wt;
    T.dy = T.ty;
    T.kd = fminf(1.25f * u, 0.95f * T.wt);
    T.ky = T.wide ? T.ty + T.wt * T.asp + 0.2f * u : T.ty;
    float pitch = (x1 - x0) / T.cols;
    for (int i = 0; i < DUR_FULL; i++) {
        int c = i % T.cols, r = i / T.cols;
        T.bx[i] = x0 + (c + 0.5f) * pitch - 0.08f * T.wp;
        T.by[i] = top + (r + 0.45f) * rowh * T.wp * T.asp;
    }
    T.hb = (box){b.x, T.mid_bot, b.w, b.y + b.h - T.mid_bot};
}

/* where the beater of place i lies: across, lower right */
static void cover_at(int i, float *x, float *y, float *deg)
{
    *x = T.bx[i] + 0.2f * T.wp;
    *y = T.by[i] + 0.2f * T.wp * T.asp;
    *deg = 14;
}

/* ---- the viewer's hand ------------------------------------------------------------------------------------------- */
static int hand_of(const dpos *p, int pl, uint8_t *out)
{
    memcpy(out, p->hand[pl], p->n[pl]);
    dur_sort(p, out, p->n[pl]);
    return p->n[pl];
}

static float hand_spots(int n, cards_spot *s)
{
    return ct_hand(T.hb, n, T.asp, fminf(T.b.w * 0.2f, 2.3f * T.u), s);
}

static int hand_hit(const uint8_t *h, int n, float x, float y)
{
    cards_spot s[DUR_HAND];
    float w = hand_spots(n, s);
    int i = cards_hit(s, n, w, T.asp, x, y);
    if (i < 0) i = cards_hit(s, n, w, T.asp, x, y + 0.14f * w);   /* a raised card: where it shows */
    return i < 0 ? CARD_NONE : h[i];
}

static uint64_t bit(int card) { return 1ull << dur_index(card); }

/* the open places the card beats (bit i) */
static unsigned targets(const dpos *p, int card)
{
    unsigned m = 0;
    for (int i = 0; i < p->natt; i++)
        if (p->def[i] == DUR_NONE && dur_beats(p, card, p->att[i])) m |= 1u << i;
    return m;
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
static int busy(void) { return ct_busy(&MV); }

/* the cards player q gained, from the position before (b) to the one after (p): first the nb cards of the bout
 * taken (bout, their places fx, fy, fd; `just`: those still flying there, shown from `land` on), then the ones drawn
 * off the talon (talon_left: its cards before, counted down; its last card is the trump under it); from `delay` on.
 * When the last one leaves */
static int gains(const dpos *b, const dpos *p, int q, const uint8_t *bout, const float *fx, const float *fy,
                 const float *fd, int nb, unsigned just, int land, int delay, int *talon_left)
{
    int t = delay, nd = p->n[q] - b->n[q] - nb;
    uint8_t h[DUR_HAND];
    int n = hand_of(p, q, h);
    cards_spot s[DUR_HAND];
    float w = hand_spots(n, s);
    /* the cards drawn, in the order they came off the talon */
    uint8_t drawn[DUR_FULL];
    for (int i = 0; i < nd; i++) drawn[i] = b->talon[*talon_left - 1 - i];
    for (int j = 0; j < nb + nd; j++) {
        int card = j < nb ? bout[j] : drawn[j - nb], from_trump = j >= nb && *talon_left - (j - nb) == 1;
        float x0 = j < nb ? fx[j] : from_trump ? T.trx : T.tx, y0 = j < nb ? fy[j] : T.ty, w0 = j < nb ? T.wp : T.wt;
        float d0 = j < nb ? fd[j] : from_trump ? 90 : 0;
        int up0 = j < nb || from_trump, src = j < nb ? 0 : from_trump ? 2 : 1, m;
        if (q == T.me && ui.viewer >= 0) {
            int i = 0;
            while (i < n && h[i] != card) i++;
            m = ct_slide(&MV, DECK_FRENCH, card, x0, y0, w0, d0, up0, s[i].x, s[i].y, w, s[i].deg, 1, SLIDE_MS + 60,
                         t, H_HAND, card, src, j % 2 ? -1 : SND_MOVE);
        } else {
            float x, y, a;
            ct_others_end(&T.O, q, b->n[q] + j + 1, &x, &y, &a);
            m = ct_slide(&MV, DECK_FRENCH, up0 || open_hands ? card : CARD_NONE, x0, y0, w0, d0, up0, x, y, T.O.sw, a,
                         open_hands, SLIDE_MS,
                         t, H_SEAT, q, src, j % 2 ? -1 : SND_MOVE);
        }
        if (from_trump) ct_show(&MV, m, t), trump_go = ui_ms() + (Uint32)t;   /* the trump stays under the talon */
        if (j < nb && just >> j & 1) ct_show(&MV, m, land);   /* the card just played shows once it has landed */
        t += j < nb ? 60 : 130;
    }
    *talon_left -= nd;
    return t;
}

/* the bout is over: its cards beaten off or taken, then everyone draws (the attacker first, the defender last) */
static void bout_over(const dpos *b, const dpos *p, uint64_t played, int land)
{
    uint8_t bout[2 * DUR_FULL];
    float fx[2 * DUR_FULL], fy[2 * DUR_FULL], fd[2 * DUR_FULL];
    int nb = 0;
    unsigned just = 0;                          /* the cards just played (still flying to the bout) */
    for (int i = 0; i < b->natt; i++) {
        just |= (unsigned)((played & bit(b->att[i])) != 0) << nb;
        bout[nb] = b->att[i], fx[nb] = T.bx[i], fy[nb] = T.by[i], fd[nb++] = 0;
        if (b->def[i] != DUR_NONE) {
            float x, y, a;
            cover_at(i, &x, &y, &a);
            just |= (unsigned)((played & bit(b->def[i])) != 0) << nb;
            bout[nb] = b->def[i], fx[nb] = x, fy[nb] = y, fd[nb++] = a;
        }
    }
    int t = land + LOOK_MS, d = b->defender, left = b->ntalon;
    if (p->last_end == 1) {                     /* beaten off */
        for (int j = 0; j < nb; j++) {
            int m = ct_slide(&MV, DECK_FRENCH, bout[j], fx[j], fy[j], T.wp, fd[j], 1, T.dx, T.dy, T.wt, 0, 0,
                             SLIDE_MS + 80, t + 40 * j, H_BITO, 0, 0, j ? -1 : SND_MOVE);
            if (just >> j & 1) ct_show(&MV, m, land);
        }
        t += SLIDE_MS + 80 + 40 * nb;
    }
    for (int i = 0; i < b->players; i++) {      /* the draw, in its order; the defender takes the bout first */
        int q = (b->attacker + i) % b->players;
        if (q == d) continue;
        if (p->n[q] > b->n[q]) t = gains(b, p, q, NULL, NULL, NULL, NULL, 0, 0, 0, t, &left) + 60;
    }
    if (p->last_end == 2) t = gains(b, p, d, bout, fx, fy, fd, nb, just, land, t, &left);
    else if (p->n[d] > b->n[d]) gains(b, p, d, NULL, NULL, NULL, NULL, 0, 0, 0, t, &left);
}

/* the deal: six cards to each, one at a time, then the trump card turns up under the talon */
static void deal_slides(void)
{
    const dpos *p = &G.pos;
    int k = 0, step = p->players > 3 ? 45 : 70;
    uint8_t h[DUR_HAND];
    int n = hand_of(p, T.me, h);
    cards_spot s[DUR_HAND];
    float w = hand_spots(n, s);
    for (int c = 0; c < DUR_FULL; c++)
        for (int q = 0; q < p->players; q++, k++) {
            if (q == T.me) {
                int card = p->hand[q][c], i = 0;
                while (i < n && h[i] != card) i++;
                ct_slide(&MV, DECK_FRENCH, card, T.tx, T.ty, T.wt, 0, 0, s[i].x, s[i].y, w, s[i].deg, 1, DEAL_MS + 80,
                         k * step, H_HAND, card, 1, k % 3 ? -1 : SND_MOVE);
            } else {
                float x, y, a;
                ct_others_end(&T.O, q, c + 1, &x, &y, &a);
                ct_slide(&MV, DECK_FRENCH, open_hands ? p->hand[q][c] : CARD_NONE, T.tx, T.ty, T.wt, 0, 0, x, y, T.O.sw,
                         a, open_hands, DEAL_MS, k * step, H_SEAT, q, 1, k % 3 ? -1 : SND_MOVE);
            }
        }
    if (p->ntalon)
        ct_slide(&MV, DECK_FRENCH, p->trump_card, T.tx, T.ty, T.wt, 0, 0, T.trx, T.ty, T.wt, 90, 1, SLIDE_MS + 100,
                 k * step + 150, H_TRUMP, 0, 0, SND_PLACE);
}

/* what an action moved, from the position before it (b); the viewer acted (their hand as it was shown) */
static void animate(const dpos *b, uint64_t a)
{
    const dpos *p = &G.pos;
    uint8_t h[DUR_HAND];
    int n = hand_of(b, b->turn, h), land = 0;
    cards_spot s[DUR_HAND];
    float w = hand_spots(n, s);
    int me = b->turn == T.me;
    for (int i = 0, k = 0; i < n; i++) {        /* the cards that left the hand, to their places in the bout */
        int c = h[i], slot = -1, cover = 0;
        if (DA_KIND(a) == DA_PLAY && DA_MASK(a) & bit(c))   /* the places in the order of the cards (the rules') */
            slot = b->natt + ct_count(DA_MASK(a) & (bit(c) - 1));
        else if (DA_KIND(a) == DA_SHIFT && DA_CARD(a) == c) slot = b->natt;
        else if (DA_KIND(a) == DA_BEAT && DA_CARD(a) == c) slot = DA_SLOT(a), cover = 1;
        if (slot < 0) continue;
        float x0 = s[i].x, y0 = s[i].y, d0 = s[i].deg, x1 = T.bx[slot], y1 = T.by[slot], d1 = 0;
        if (!me) ct_others_end(&T.O, b->turn, b->n[b->turn], &x0, &y0, &d0);
        if (ct_dragging(&drag)) x0 = drag.x + k * 0.3f * w, y0 = drag.y, d0 = 0;
        if (cover) cover_at(slot, &x1, &y1, &d1);
        ct_slide(&MV, DECK_FRENCH, c, x0, y0, me ? w : T.O.sw, d0, 1, x1, y1, T.wp, d1, 1, SLIDE_MS, 60 * k, H_BOUT, slot,
                 cover, k ? -1 : SND_PLACE);
        land = SLIDE_MS + 60 * k;
        k++;
    }
    if (p->bouts != b->bouts) {                 /* the bout is over */
        dpos bb = *b;                           /* the bout as it ended: the last action's cards in it */
        if (DA_KIND(a) == DA_PLAY)
            for (int i = 0; i < DUR_CARDS; i++)
                if (DA_MASK(a) >> i & 1) bb.att[bb.natt] = (uint8_t)dur_card(i), bb.def[bb.natt++] = DUR_NONE;
        if (DA_KIND(a) == DA_BEAT) bb.def[DA_SLOT(a)] = (uint8_t)DA_CARD(a);
        bb.n[b->turn] = (uint8_t)(b->n[b->turn] - (DA_KIND(a) == DA_PLAY ? ct_count(DA_MASK(a))
                                                   : DA_KIND(a) == DA_BEAT));
        uint64_t played = DA_KIND(a) == DA_PLAY ? DA_MASK(a) : DA_KIND(a) == DA_BEAT ? bit(DA_CARD(a)) : 0;
        bout_over(&bb, p, played, land);
    }
}

static void calm(void)
{
    ct_calm(&MV);
    chosen = 0;
    raised = CARD_NONE;
    drag.on = 0;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    open_hands = v >= DK_OPEN;
    dur_new(&G, v % DK_OPEN, ui.players, (uint32_t)ui_random(1 << 30));
    calm();
    dealing = 1;
    mark = 0;
}

static unsigned winners(void) { return dur_winners(&G.pos); }

static int status(void)
{
    const dpos *p = &G.pos;
    if (p->phase != DP_OVER) return GS_PLAY;
    if (p->fool == DUR_NONE) return GS_DRAW;
    return gs_win(p->fool == 0 ? 1 : 0);        /* any winner: they all share it (winners()) */
}

static int turn(void) { return G.pos.turn; }

static void act(uint64_t a)
{
    dpos before = G.pos;
    if (!dur_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&MV);
    animate(&before, a);
    chosen = 0;
    raised = CARD_NONE;
    drag.on = 0;
    if (DA_KIND(a) == DA_TAKE) ui_sound(SND_TAP);
    const dpos *p = &G.pos;
    if (p->turn != before.turn && !open_hands) mark = G.n;   /* secret hands: undo stops at the hand-off */
    if (p->phase != DP_OVER) ui_handoff(p->turn);
}

const dgame *durak_live(void) { return &G; }
int durak_open_hands(void) { return open_hands; }

int durak_show(const char *move)
{
    static dgame t;
    t = G;
    if (!dur_script(&t, move) || t.n != G.n + 1) return 0;
    act(t.act[G.n]);
    return 1;
}

static int undo(void)
{
    if (chosen || raised != CARD_NONE) { chosen = 0, raised = CARD_NONE; return 1; }   /* a choice not made yet */
    if (G.n <= mark || !dur_undo(&G)) return 0;
    calm();
    dealing = 0;
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = dur_script(&G, s);
    calm();
    dealing = 0;
    mark = open_hands ? 0 : G.n;
    ui_handoff(G.pos.turn);
    return ok;
}

static int hidden(void) { return !open_hands; }   /* the shell's cover: secret hands only */

static int can_pass(void) { return !ui_handing() && !busy() && G.pos.turn == T.me && dur_legal(&G.pos, DA_PASS); }
static void pass(void) { act(DA_PASS); }

/* ---- pictures of our own: a sword (the attacker), a shield (the defender) ---------------------------------------- */
static void paint_pic(int kind, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_style steel = {{214, 218, 224}, {40, 44, 52}, {110, 116, 128}, {150, 104, 30}, 0.35f, 0.05f};
    art_style shield = {{58, 92, 156}, {22, 26, 40}, {120, 150, 200}, {232, 184, 70}, 0.35f, 0.05f};
    if (kind == PIC_SWORD) {                    /* upright: the shell turns it */
        const float blade[] = {0.44f, 0.16f, 0.5f, 0.04f, 0.56f, 0.16f, 0.56f, 0.64f, 0.44f, 0.64f};
        art_poly(f, ART_PART, blade, 5);
        art_rect(f, ART_PART, 0.26f, 0.63f, 0.74f, 0.71f);
        art_rect(f, ART_PART, 0.455f, 0.71f, 0.545f, 0.88f);
        art_ellipse(f, ART_PART, 0.5f, 0.91f, 0.065f, 0.065f);
        art_render(f, &steel, size, size, (float)size, 0, 0, rgba);
    } else {
        const float s[] = {0.16f, 0.12f, 0.5f, 0.05f, 0.84f, 0.12f, 0.82f, 0.5f, 0.5f, 0.94f, 0.18f, 0.5f};
        art_poly(f, ART_PART, s, 6);
        art_rect(f, ART_MARK, 0.46f, 0.18f, 0.54f, 0.78f);
        art_rect(f, ART_MARK, 0.25f, 0.38f, 0.75f, 0.46f);
        art_render(f, &shield, size, size, (float)size, 0, 0, rgba);
    }
    art_fig_free(f);
}

static void put_role(int kind, float cx, float cy, float d)
{
    SDL_Texture *t = ct_picture(paint_pic, kind, (int)fmaxf(8, d));
    if (kind == PIC_SWORD) ui_put_rot(t, cx, cy, d, d, 35);
    else ui_put_c(t, cx, cy, d, d);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_others(void)
{
    const dpos *p = &G.pos;
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) continue;
        int n = p->n[q] - ct_hides(&MV, H_SEAT, q, -1);
        float x, y, d;
        ct_others_draw(&T.O, q, n, 0);
        if (open_hands && n > 0) {              /* open hands: their cards face up, fanned as the backs are */
            uint8_t h[DUR_HAND];
            cards_spot s[14];
            int k = hand_of(p, q, h);
            k = k < n ? k : n;
            k = k < 14 ? k : 14;
            cards_fan(ct_others_x(&T.O, q), T.O.sy, T.O.sfan, T.O.sw, T.asp, k, k > 1 ? 18 : 0, s);
            for (int j = 0; j < k; j++) cards_draw(DECK_FRENCH, h[j], s[j].x, s[j].y, T.O.sw, s[j].deg, 0);
        }
        if (p->phase == DP_OVER) continue;
        ct_others_badge(&T.O, q, n, 1, &x, &y, &d);   /* beside the chip and count */
        if (q == p->defender) put_role(PIC_SHIELD, x, y, d);
        else if (q == p->attacker) put_role(PIC_SWORD, x, y, d);
        ct_others_badge(&T.O, q, n, -1, &x, &y, &d);
        if (p->phase == DP_THROW && p->passed >> q & 1) ct_pass_badge(x, y, d);
    }
}

/* the talon and the trump card under it; the trump's sign once the talon is gone */
static void draw_talon(void)
{
    const dpos *p = &G.pos;
    int last = ct_hides(&MV, H_HAND, -1, 2) + ct_hides(&MV, H_SEAT, -1, 2);   /* the trump card drawn, flying */
    int pending = ct_hides(&MV, H_HAND, -1, 1) + ct_hides(&MV, H_SEAT, -1, 1) + last;   /* drawn, on their way */
    int n = p->ntalon + pending, trump_moving = ct_hides(&MV, H_TRUMP, -1, -1) || (last && ui_ms() >= trump_go);
    ct_talon(DECK_FRENCH, T.tx, T.ty, T.trx, T.ty, T.wt, n, trump_moving ? CARD_NONE : p->trump_card, p->trump, 1);
}

static void draw_bout(int mine)
{
    const dpos *p = &G.pos;
    int def = p->phase == DP_DEFEND && mine && !p->taking;
    unsigned tg = def && raised != CARD_NONE ? targets(p, raised) : 0;
    for (int i = 0; i < p->natt; i++) {
        if (!ct_hides(&MV, H_BOUT, i, 0)) {
            if (tg >> i & 1) ui_mark_gold(T.bx[i], T.by[i], 1.5f * T.wp * T.asp);
            cards_draw(DECK_FRENCH, p->att[i], T.bx[i], T.by[i], T.wp, 0, tg >> i & 1 ? CD_GLOW : 0);
        }
        if (p->def[i] != DUR_NONE && !ct_hides(&MV, H_BOUT, i, 1)) {
            float x, y, a;
            cover_at(i, &x, &y, &a);
            cards_draw(DECK_FRENCH, p->def[i], x, y, T.wp, a, 0);
        }
    }
    /* perevodnoy: the place to pass the attack on */
    if (def && raised != CARD_NONE && dur_can_shift(p, raised) && p->natt < DUR_FULL)
        cards_pile(DECK_FRENCH, T.bx[p->natt], T.by[p->natt], T.wp, 0, CARD_NONE, CD_GLOW);
    /* attacking or throwing in: the bout's area lights when the cards chosen can go */
    if (mine && chosen && (p->phase == DP_ATTACK || p->phase == DP_THROW) && !drag.on) {
        int i = p->natt < DUR_FULL ? p->natt : DUR_FULL - 1;
        cards_pile(DECK_FRENCH, T.bx[i], T.by[i], T.wp, 0, CARD_NONE, CD_GLOW);
    }
}

static void draw(void)
{
    const dpos *p = &G.pos;
    if (dealing && ui.viewer >= 0) { dealing = 0; ct_calm(&MV); deal_slides(); }   /* the cover lifted: it flies */
    int mine = p->turn == T.me && !ui_handing() && !ct_moving(&MV) && p->phase != DP_OVER, hide = ui.viewer < 0;
    draw_others();
    draw_talon();
    int nd = p->discards - ct_hides(&MV, H_BITO, -1, -1);
    if (nd > 0) cards_pile(DECK_FRENCH, T.dx, T.dy, T.wt, nd > 8 ? 4 : (nd + 1) / 2, CARD_NONE, 0);
    draw_bout(mine);
    /* the take button: the defender's, lit for all once they take */
    if (p->phase == DP_DEFEND || (p->phase == DP_THROW && p->taking)) {
        int can = mine && dur_legal(p, DA_TAKE);
        if (can || p->taking) {
            if (can) ui_mark_gold(T.kx, T.ky, 1.7f * T.kd);
            ct_button(T.kx, T.ky, T.kd, p->taking, CT_ARROW, 90);
        }
    }
    /* the hand */
    uint8_t h[DUR_HAND];
    int n = hand_of(p, T.me, h);
    cards_spot s[DUR_HAND];
    float w = hand_spots(n, s);
    for (int i = 0; i < n; i++) {
        int c = h[i];
        if (ct_hides(&MV, H_HAND, c, -1)) continue;
        if (hide) { cards_draw(DECK_FRENCH, CARD_NONE, s[i].x, s[i].y, w, s[i].deg, CD_DOWN); continue; }
        if (ct_dragging(&drag) && ((chosen & bit(c)) || c == drag.card)) continue;
        int how = 0;
        if ((chosen & bit(c)) || c == raised) how = CD_RAISED;
        else if (mine && p->phase == DP_THROW && dur_room(p) && dur_can_throw(p, c)) how = CD_GLOW;
        else if (mine && p->phase == DP_DEFEND && !p->taking && raised == CARD_NONE &&
                 (targets(p, c) || dur_can_shift(p, c)))
            how = CD_GLOW;
        cards_draw(DECK_FRENCH, c, s[i].x, s[i].y, w, s[i].deg, how);
    }
    ct_draw(&MV);
    if (ct_dragging(&drag) && !hide) {   /* the cards under the finger */
        int k = 0;
        for (int i = 0; i < n; i++)
            if ((chosen & bit(h[i])) || h[i] == drag.card)
                cards_draw(DECK_FRENCH, h[i], drag.x + k++ * 0.3f * w, drag.y, w, 0, CD_RAISED);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int bout_at(float x, float y)            /* the bout's place under (x, y) (its cover too); -1: none */
{
    for (int i = DUR_FULL - 1; i >= 0; i--) {
        float cx, cy, a;
        cover_at(i, &cx, &cy, &a);
        if (cards_in(T.bx[i], T.by[i], T.wp * 1.1f, T.asp, x, y) || cards_in(cx, cy, T.wp, T.asp, x, y)) return i;
    }
    return -1;
}

/* the defender sends the risen card to place i (-1: wherever it can only go) */
static void defend(int card, int i)
{
    const dpos *p = &G.pos;
    unsigned tg = targets(p, card);
    int shift = dur_can_shift(p, card);
    if (i >= 0 && i < p->natt && tg >> i & 1) { act(DA_BEATS(card, i)); return; }
    if (i == p->natt && shift) { act(DA_SHIFT | (uint64_t)card); return; }
    if (i < 0 && tg && !(tg & (tg - 1)) && !shift) {
        int k = 0;
        while (!(tg >> k & 1)) k++;
        act(DA_BEATS(card, k));
        return;
    }
    if (i < 0 && !tg && shift) { act(DA_SHIFT | (uint64_t)card); return; }
    ui_sound(SND_DENY);
}

/* a tap on a card of the attacker's or a thrower's hand: chosen, or left */
static void choose(int c)
{
    const dpos *p = &G.pos;
    if (chosen & bit(c)) { chosen &= ~bit(c); ui_sound(SND_TAP); return; }
    if (p->phase == DP_ATTACK) {
        for (int i = 0; i < DUR_CARDS; i++)     /* one rank only */
            if (chosen >> i & 1 && CARD_RANK(dur_card(i)) != CARD_RANK(c)) chosen = 0;
    } else if (!dur_can_throw(p, c)) { ui_sound(SND_DENY); return; }
    if (ct_count(chosen) >= dur_room(p)) { ui_sound(SND_DENY); return; }
    chosen |= bit(c);
    ui_sound(SND_TAP);
}

static void press(float x, float y)
{
    const dpos *p = &G.pos;
    drag.on = 0;
    if (ct_moving(&MV) || p->phase == DP_OVER) return;
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    uint8_t h[DUR_HAND];
    int n = hand_of(p, T.me, h), c = hand_hit(h, n, x, y);
    if (p->phase == DP_DEFEND) {
        if (ct_on(T.kx, T.ky, T.kd, x, y)) { act(DA_TAKE); return; }
        if (c == CARD_NONE) {
            if (raised == CARD_NONE) { ui_sound(SND_DENY); return; }
            int i = bout_at(x, y);
            if (i < 0 && y < T.mid_bot) { ui_sound(SND_DENY); return; }
            if (i < 0) { raised = CARD_NONE; return; }
            defend(raised, i);
            return;
        }
        if (c == raised) {                      /* tapped again: it goes where it can only go */
            ct_drag_start(&drag, -1, c, x, y);
            return;
        }
        if (!targets(p, c) && !dur_can_shift(p, c)) { ui_sound(SND_DENY); return; }
        raised = c;
        ui_sound(SND_TAP);
        ct_drag_start(&drag, 0, c, x, y);
        return;
    }
    /* attacking, throwing in */
    if (c == CARD_NONE) {
        if (y < T.mid_bot && chosen) act(DA_PLAY | chosen);
        else ui_sound(SND_DENY);
        return;
    }
    int was = (chosen & bit(c)) != 0;
    if (!was) choose(c);
    if (chosen & bit(c)) ct_drag_start(&drag, was ? -1 : 0, c, x, y);
}

static void motion(float x, float y)
{
    ct_drag_move(&drag, x, y, 0.25f * T.u);
}

static void release(float x, float y)
{
    if (!drag.on) return;
    const dpos *p = &G.pos;
    drag.x = x, drag.y = y;
    int moved = drag.moved, c = drag.card;
    if (p->phase == DP_DEFEND) {
        if (moved > 0) {
            int i = bout_at(x, y);
            if (i >= 0 || y < T.mid_bot) defend(c, i);
        } else if (moved < 0) defend(c, -1);
        drag.on = 0;
        return;
    }
    if (moved > 0 && y < T.mid_bot) act(DA_PLAY | chosen);
    else if (moved < 0) { chosen &= ~bit(c); ui_sound(SND_TAP); }   /* a chosen card tapped again: left */
    drag.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* a player's bar: a sword (the attacker) or a shield (the defender), a small back and the cards in hand */
static void bar(int player, box b)
{
    const dpos *p = &G.pos;
    float u = ui.unit, cy = b.y + b.h / 2, d = fminf(0.62f * u, b.h * 0.8f), x = b.x;
    if (p->phase != DP_OVER && (player == p->attacker || player == p->defender)) {
        put_role(player == p->defender ? PIC_SHIELD : PIC_SWORD, x + d / 2, cy, d);
        x += d + 0.12f * u;
    }
    ct_bar_cards(b, x, DECK_FRENCH, p->n[player]);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

static void paint_eye(int kind, int size, uint8_t *rgba) { (void)kind, art_icon(ICON_EYE, size, rgba); }

/* the menu's and the picker's cards: a 9 beaten by a jack, a trump ace (throw-in); three 8s (transfer); open hands:
 * the same with an eye on the shell's disc in the corner */
static void thumb(int v, int size, uint8_t *rgba)
{
    if (v >= DK_OPEN) {
        thumb(v - DK_OPEN, size, rgba);
        int d = (int)(0.34f * size);
        ui_button_pic(rgba, size, size, size - 0.58f * d, size - 0.58f * d, d, paint_eye, 0, 0.62f, UI_TINT);
        return;
    }
    static const int classic[4] = {CARD(SUIT_SPADES, 6), CARD(SUIT_HEARTS, 9), CARD(SUIT_HEARTS, RANK_JACK),
                                   CARD(SUIT_CLUBS, RANK_ACE)};
    static const int transfer[3] = {CARD(SUIT_CLUBS, 8), CARD(SUIT_DIAMONDS, 8), CARD(SUIT_SPADES, 8)};
    if (v == DUR_TRANSFER) ct_thumb(DECK_FRENCH, transfer, 3, size, rgba);
    else ct_thumb(DECK_FRENCH, classic, 4, size, rgba);
}

static const game durak_game = {
    .id = "durak", .order = 314, .variants = 2 * DUR_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6, .players_usual = 3, .view = VIEW_HIDDEN, .hidden = hidden,
};
REGISTER_GAME(durak_game)
