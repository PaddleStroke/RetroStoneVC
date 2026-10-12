/*
 * Piquet in RetroStone Board: two players passing the device, each hand secret (VIEW_HIDDEN; the rules: piquet_rules.c,
 * piquet.h). The table (the kit's cards_table.h): the viewer's twelve cards fanned at the bottom, the other's backs at
 * the top beside their chip; the talon in the middle while the cards are changed, the cards thrown on the right.
 * - The exchange: tap the cards to throw (they rise: the elder 1 to 5, the younger up to what is left), then the
 *   tick; as many come from the talon, standing a little higher. Beside each chip: how many they changed.
 * - The declarations, a board in the middle: a column each (the point: three cards of a suit; sequences: three cards
 *   in a stair; sets: three aces; carte blanche, when held: a blank card), a row each player (the other at the top).
 *   The point: its cards and value; a sequence: its length and its top card; a set: three or four and its rank. The
 *   elder sees their own row and taps the tick (declared); the younger then sees both rows, a gold tick beside the
 *   better one (good, or not good) or an equals sign (equal), and the points it scores; their tick shows the better
 *   combinations' cards to both. The board stays until the elder leads.
 * - Playing: tap a card (it rises), tap it again, or drag it to the middle. Beside each chip: the deal's points so far.
 * After each deal a summary: each player's chip, the declarations' points, the play's (capot: a crown), the pique or
 * repique (a spade's sign: 30 or 60), the deal's points, the total; the six deals as dots. The last one adds the
 * Rubicon: the winner's score for the partie (under 100, the loser's total is struck out: rubiconed).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/piquet.h"
#include "games/tricks.h"
#include "shell.h"

#define LIFT 0.14f
enum { TAG_THROWN = 10 };
enum { PIC_POINT, PIC_SEQUENCE, PIC_SETS, PIC_BLANCHE, PIC_CROWN };

static piquet_game G;
static ct_table T;
static ct_moves A;
static ct_touch F;
static int floor_n, show_last;
static unsigned pick, seen;
static uint8_t got[PIQUET_P][PIQUET_HAND];      /* the cards each drew from the talon (for the lift) */

/* ---- the pictures: the declarations' columns ------------------------------------------------------------------- */
static void paint(int kind, int s, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)s * s * 4);
    ct_paint_ivory(rgba, s);
    cards_disc_rim(rgba, s);
    int w = (int)(0.3f * s), h = (int)(w * cards_aspect(DECK_FRENCH)), y = (s - h) / 2;
    switch (kind) {
    case PIC_POINT:                             /* three hearts fanned */
        ct_paint_card(rgba, s, DECK_FRENCH, CARD(SUIT_HEARTS, 7), w, (int)(0.14f * s), y + s / 30, 1);
        ct_paint_card(rgba, s, DECK_FRENCH, CARD(SUIT_HEARTS, 9), w, (int)(0.35f * s), y - s / 60, 1);
        ct_paint_card(rgba, s, DECK_FRENCH, CARD(SUIT_HEARTS, RANK_KING), w, (int)(0.56f * s), y + s / 30, 1);
        break;
    case PIC_SEQUENCE:                          /* 8, 9, 10 of spades in a stair */
        for (int i = 0; i < 3; i++)
            ct_paint_card(rgba, s, DECK_FRENCH, CARD(SUIT_SPADES, 8 + i), w, (int)((0.14f + 0.21f * i) * s),
                          y + (int)((0.12f - 0.12f * i) * s), 1);
        break;
    case PIC_SETS:                              /* three aces */
        ct_paint_card(rgba, s, DECK_FRENCH, CARD(SUIT_CLUBS, RANK_ACE), w, (int)(0.14f * s), y, 1);
        ct_paint_card(rgba, s, DECK_FRENCH, CARD(SUIT_HEARTS, RANK_ACE), w, (int)(0.35f * s), y, 1);
        ct_paint_card(rgba, s, DECK_FRENCH, CARD(SUIT_SPADES, RANK_ACE), w, (int)(0.56f * s), y, 1);
        break;
    case PIC_BLANCHE: {                         /* a blank card */
        art_fig *f = art_fig_new();
        art_rect(f, ART_PART, 0.33f, 0.22f, 0.67f, 0.78f);
        art_style st = {{252, 250, 244}, {90, 64, 40}, {90, 64, 40}, {0, 0, 0}, 0.2f, 0.025f};
        ct_paint_fig(rgba, s, f, &st);
        break;
    }
    case PIC_CROWN: {                           /* capot: a gold crown, no disc */
        memset(rgba, 0, (size_t)s * s * 4);
        art_fig *f = art_fig_new();
        const float crown[] = {0.2f, 0.74f, 0.14f, 0.3f, 0.34f, 0.52f, 0.5f, 0.22f, 0.66f, 0.52f, 0.86f, 0.3f, 0.8f,
                               0.74f};
        art_poly(f, ART_PART, crown, 7);
        art_style st = {{232, 186, 72}, {104, 72, 18}, {150, 110, 36}, {0, 0, 0}, 0.5f, 0.03f};
        ct_paint_fig(rgba, s, f, &st);
        break;
    }
    }
}

static SDL_Texture *pic(int kind, float d) { return ct_picture(paint, kind, (int)(d + 0.5f)); }

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b) { ct_layout(&T, b, PIQUET_P, ui_bottom(), PIQUET_HAND); }

static void reset_ui(void)
{
    pick = 0;
    show_last = 0;
    ct_touch_reset(&F);
}

static float pile_w(void) { return floorf(0.75f * T.wt); }
static void talon_at(float *x, float *y) { *x = T.cx - 0.7f * pile_w(), *y = T.cy - 0.3f * T.u; }
static void thrown_at(float *x, float *y) { *x = T.b.x + T.b.w - 0.62f * pile_w() - 0.2f * T.u, *y = T.cy; }
static void tick_at(float *x, float *y, float *d)
{
    *d = 1.1f * T.u;
    *x = T.cx + 0.8f * pile_w();
    *y = T.cy - 0.3f * T.u;
}

/* the board shows before the first card: while declaring, then until the elder leads */
static int board_on(void)
{
    const piquet_pos *p = &G.pos;
    return p->phase == PQ_DECLARE || (p->phase == PQ_PLAY && p->tricks == 0 && p->ntrick == 0);
}

static int fresh(int card)
{
    const piquet_pos *p = &G.pos;
    if (p->phase > PQ_PLAY || p->tricks || p->ntrick || (p->phase == PQ_EXCHANGE && p->turn == T.me)) return 0;
    for (int i = 0; i < PIQUET_HAND; i++)
        if (got[T.me][i] == card) return 1;
    return 0;
}

static int hand(uint8_t *h, int *how, float *lift, int live)
{
    const piquet_pos *p = &G.pos;
    int n = p->n[T.me], mine = live && p->turn == T.me;
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][i];
        h[i] = (uint8_t)c;
        how[i] = mine && p->phase == PQ_PLAY ? (piquet_can_play(p, c) ? CD_GLOW : CD_DIM) : 0;
        if (ct_is_raised(&F, i)) how[i] = CD_RAISED;
        if (lift) lift[i] = fresh(c) ? LIFT : 0;
    }
    if (mine && p->phase == PQ_EXCHANGE) ct_chosen_hows(n, pick, ct_count(pick) < piquet_throw_max(p), how);
    return n;
}

/* the cards drawn, found again from the position (undo, the picture mode): the hand against the deal's */
static void find_got(void)
{
    const piquet_pos *p = &G.pos;
    memset(got, CARD_NONE, sizeof got);
    if (p->phase > PQ_PLAY) return;
    for (int pl = 0; pl < PIQUET_P; pl++)
        for (int i = 0, k = 0; i < p->n[pl]; i++) {
            int c = p->hand[pl][i], from_talon = 0;
            for (int t = 0; t < p->top; t++) from_talon |= p->talon[t] == c;
            if (from_talon) got[pl][k++] = (uint8_t)c;
        }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    piquet_new(&G, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    reset_ui();
    find_got();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    if (G.pos.phase != PQ_OVER) return GS_PLAY;
    int w = piquet_winner(&G.pos);
    return w < 0 ? GS_DRAW : gs_win(w);
}

static int turn(void) { return G.pos.turn; }

static void animate_exchange(const piquet_pos *b, unsigned mask, const uint8_t *h, int n)
{
    const piquet_pos *p = &G.pos;
    int me = b->turn;
    if (me != T.me) return;
    cards_spot s[CT_HAND], ns[CT_HAND];
    ct_fan(&T, n, s);
    float tx, ty, sx, sy;
    thrown_at(&tx, &ty);
    talon_at(&sx, &sy);
    for (int i = 0, k = 0; i < n; i++)
        if (mask >> i & 1)
            ct_slide(&A, DECK_FRENCH, h[i], s[i].x, s[i].y - 0.14f * T.wh, T.wh, s[i].deg, 1, tx, ty, pile_w(), 0, 0,
                     CT_SLIDE_MS, 60 * k, TAG_THROWN, 0, -1, k == 0 ? SND_MOVE : -1), k++;
    ct_fan(&T, p->n[me], ns);
    for (int i = 0, k = 0; i < p->n[me]; i++) {
        int c = p->hand[me][i];
        if (trick_find(b->hand[me], b->n[me], c) >= 0) continue;
        ct_rest(&A, ct_slide(&A, DECK_FRENCH, c, sx, sy, pile_w(), 0, 0, ns[i].x, ns[i].y - LIFT * T.wh, T.wh,
                             ns[i].deg, 1, CT_SLIDE_MS + 100, CT_SLIDE_MS + 100 + 80 * k, CT_HAND_TAG, i, -1,
                             k == 0 ? SND_PLACE : -1), 250);
        k++;
    }
}

static void animate(const piquet_pos *b, int a, const uint8_t *h, int n)
{
    const piquet_pos *p = &G.pos;
    switch (PQA_KIND(a)) {
    case PQA_EXCHANGE: animate_exchange(b, (unsigned)PQA_ARG(a), h, n); return;
    case PQA_DECLARE: ui_sound(SND_PLACE); return;
    case PQA_PLAY:
        ct_play(&A, &T, &F, h, n, PQA_ARG(a));
        if (p->tricks != b->tricks || p->deal != b->deal)
            ct_gather(&A, &T, p->last, NULL, PIQUET_P, p->last_leader, p->last_winner, CT_SLIDE_MS + CT_LOOK_MS,
                      CT_SLIDE_MS);
        return;
    }
}

static void act(int a)
{
    piquet_pos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, NULL, 0);
    if (!piquet_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    animate(&before, a, h, n);
    reset_ui();
    find_got();
    const piquet_pos *p = &G.pos;
    if (PQA_KIND(a) == PQA_NEXT) seen = 0;
    if (p->turn != before.turn || p->phase == PQ_ROUND || p->phase == PQ_OVER) floor_n = G.n;
    if (p->phase <= PQ_PLAY) ui_handoff(p->turn);
}

static int undo(void)
{
    if (pick || F.raised != CARD_NONE || show_last) { reset_ui(); return 1; }
    if (G.n <= floor_n || !piquet_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    find_got();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = piquet_script(&G, s);
    ct_calm(&A);
    reset_ui();
    find_got();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&A); }

/* ---- drawing: the board of the declarations --------------------------------------------------------------------- */
static struct { float x, y, w, h, lw, cw, ic, line, fan; int kinds; } D;

static void board_layout(void)
{
    const piquet_pos *p = &G.pos;
    float u = T.u;
    D.kinds = PQ_KINDS + (p->blanche[0] || p->blanche[1]);
    D.x = T.b.x + 0.15f * u;
    D.w = T.b.w - 0.3f * u;
    D.y = T.sy[1 - T.me] + 0.75f * T.tok;
    float room = T.sy[T.me] - 0.75f * T.tok - D.y;   /* above the viewer's chip */
    D.lw = 0.95f * u;
    D.cw = (D.w - D.lw) / D.kinds;
    D.ic = floorf(fminf(0.95f * u, fminf(0.42f * D.cw, 0.2f * room)));
    D.line = floorf(fminf(0.62f * u, 0.14f * room));
    D.fan = floorf(fminf(0.42f * D.cw, (room - D.ic - 2 * D.line - 0.5f * u) / (2 * T.asp + 0.2f)));
    D.h = 0.2f * u + D.ic + 0.1f * u + 2 * (D.line + D.fan * T.asp + 0.1f * u) + 0.1f * u;
    if (D.h < room) D.y += (room - D.h) / 2;
}

/* a mini card of a rank (the sets, a sequence's top) from the player's hand, w wide */
static int card_of_rank(const piquet_pos *p, int pl, int index, int suit)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (piquet_index(p->hand[pl][i]) == index && (suit < 0 || CARD_SUIT(p->hand[pl][i]) == suit))
            return p->hand[pl][i];
    return CARD_NONE;
}

/* one player's cell in a column: the numbers, the gold tick (better) or the equals sign, the points; the cards shown */
static void draw_cell(int pl, int kind, float cx, float y, int step)
{
    const piquet_pos *p = &G.pos;
    const piquet_decl *d = &p->decl[pl];
    float h = D.line, nh = floorf(0.62f * h), x = cx - 0.42f * D.cw;
    int big = 0, small = -1, card = CARD_NONE, has = 1;
    if (kind == PQ_KINDS) {                     /* carte blanche */
        has = p->blanche[pl];
        big = 10;
    } else if (kind == PQ_POINT) big = d->pn, small = d->pv;
    else if (kind == PQ_SEQUENCE) {
        has = d->sn > 0, big = d->sn;
        card = card_of_rank(p, pl, d->stop, d->ssuit);
    } else {
        has = d->setn > 0, big = d->setn;
        card = card_of_rank(p, pl, d->setrank, -1);
    }
    if (!has) {                                 /* nothing to declare: a faint dash */
        ui_fill(cx - 0.2f * h, y - 0.04f * h, 0.4f * h, fmaxf(2, 0.08f * h), 246, 238, 220, 70);
        return;
    }
    int w = kind == PQ_KINDS ? (p->blanche[pl] ? pl : -1) : step ? piquet_kind_winner(p, kind) : -1;
    if (w == pl) ui_mark_gold(x + 0.3f * h, y, 1.5f * h);
    ui_number(x + 0.3f * h, y, nh, big, 1);
    x += 0.75f * h;
    if (small >= 0) ui_number(x + 0.4f * h, y + 0.1f * h, floorf(0.42f * h), small, 1), x += 0.85f * h;
    if (card != CARD_NONE) cards_draw(DECK_FRENCH, card, x + 0.3f * h, y, floorf(0.55f * h), 0, CD_FLAT), x += 0.7f * h;
    if (!step) return;
    if (w == pl) {
        ct_pic(CT_TICK, x + 0.35f * h, y, 0.65f * h, 0, 250, 214, 90, 255);
        int pts = kind == PQ_KINDS ? 10 : piquet_kind_points(d, kind);
        ct_plus(x + 0.75f * h + ct_number_w(pts, 0.42f * h, 1) / 2, y, floorf(0.42f * h), pts);
    }
    if (step < 1 || w != pl || kind == PQ_KINDS) return;
    uint8_t c[PIQUET_HAND];                     /* the better combination's cards, shown */
    int n = piquet_shown(p, pl, kind, c);
    cards_spot s[PIQUET_HAND];
    float fy = y + 0.5f * h + 0.55f * D.fan * T.asp;
    cards_fan(cx, fy, 0.94f * D.cw, D.fan, T.asp, n, 0, s);
    for (int i = 0; i < n; i++) cards_draw(DECK_FRENCH, c[i], s[i].x, s[i].y, D.fan, 0, CD_FLAT);
}

/* step 0: the elder declares (their row only); 1: the younger answers (both rows, the verdicts, the cards shown) */
static void draw_board(int step)
{
    const piquet_pos *p = &G.pos;
    board_layout();
    ct_panel(D.x, D.y, D.w, D.h, 190);
    float y = D.y + 0.2f * T.u + D.ic / 2, u = T.u;
    for (int k = 0; k < D.kinds; k++) {
        float cx = D.x + D.lw + (k + 0.5f) * D.cw;
        ui_put_c(pic(k == PQ_KINDS ? PIC_BLANCHE : k, D.ic), cx, y, D.ic, D.ic);
    }
    float row = D.line + D.fan * T.asp + 0.1f * u, y0 = y + D.ic / 2 + 0.1f * u + D.line / 2;
    for (int r = 0; r < 2; r++) {
        int pl = r == 0 ? 1 - T.me : T.me;
        float ry = y0 + r * row, td = floorf(0.6f * D.line);
        ui_put_c(ct_token(pl, (int)td), D.x + 0.5f * D.lw, ry, td, td);
        if (!step && pl != T.me) continue;      /* the younger's answers come later */
        for (int k = 0; k < D.kinds; k++) draw_cell(pl, k, D.x + D.lw + (k + 0.5f) * D.cw, ry, step);
        if (r == 0) ct_rule(D.x + 0.3f * u, ry + row / 2 - 0.05f * u, D.w - 0.6f * u);
    }
    if (!step) return;
    for (int k = 0; k < PQ_KINDS; k++) {        /* equal: an equals sign between the rows */
        if (p->verdict[k] != PQ_EQUAL) continue;
        float cx = D.x + D.lw + (k + 0.5f) * D.cw + 0.36f * D.cw, my = y0 + row / 2 - 0.05f * u, bw = 0.4f * D.line;
        ui_fill(cx - bw / 2, my - 0.14f * D.line, bw, fmaxf(2, 0.08f * D.line), 250, 214, 90, 255);
        ui_fill(cx - bw / 2, my + 0.06f * D.line, bw, fmaxf(2, 0.08f * D.line), 250, 214, 90, 255);
    }
}

static void draw_exchange(int live)
{
    const piquet_pos *p = &G.pos;
    int k = ct_count(pick), ok = k >= piquet_throw_min(p) && k <= piquet_throw_max(p);
    float x, y, d;
    tick_at(&x, &y, &d);
    if (ok && live) ui_mark_gold(x, y, 1.8f * d);
    ct_button(x, y, d, ok, CT_TICK, 0);
    if (k) ct_badge(x + 0.7f * d, y - 0.45f * d, 0.55f * d, k);
}

/* the declaration's tick: right of the viewer's chip */
static void declare_at(float *x, float *y, float *d)
{
    *d = 1.05f * T.u;
    *x = T.sx[T.me] + 1.9f * T.u;
    *y = T.sy[T.me] - 0.1f * T.u;
}

static void draw_tick(int live)
{
    float x, y, d;
    declare_at(&x, &y, &d);
    if (live) ui_mark_gold(x, y, 1.8f * d);
    ct_button(x, y, d, 1, CT_TICK, 0);
}

static void draw_seats(void)
{
    const piquet_pos *p = &G.pos;
    ct_draw_seats(&T, p->phase <= PQ_PLAY ? p->turn : -1);
    int o = 1 - T.me;
    float bw = floorf(0.36f * T.wt);
    ct_backs(DECK_FRENCH, T.sx[o] + 1.4f * T.tok + 1.8f * bw, T.sy[o], 3.4f * bw, bw, p->n[o], 0);
    for (int q = 0; q < PIQUET_P; q++) {
        float x, y;
        ct_seat_spot(&T, q, 0, &x, &y);
        if (q == o) x = T.sx[o] - (x - T.sx[o]);
        if (p->phase <= PQ_DECLARE && p->ex[q]) ct_badge(x, y, 0.95f * T.tok, p->ex[q]);
        int pts = p->dpts[q] + p->ppts[q] + p->bonus[q];
        if (p->phase == PQ_PLAY && pts) ui_number(x, y, 0.5f * T.tok, pts, 1);
    }
}

/* the deal's summary: a row a player: the chip, the declarations, the play (capot: a crown), the pique or repique,
 * the deal, the total; the six deals; at the end the Rubicon line */
static void draw_summary(void)
{
    const piquet_pos *p = &G.pos;
    box b = T.b;
    float u = T.u, pw = fminf(b.w * 0.96f, 8.6f * u), rh = fminf(1.1f * u, b.h * 0.18f), hh = 0.7f * u;
    int over = p->phase == PQ_OVER && piquet_winner(p) >= 0;
    float ph = 0.25f * u + hh + 2 * rh + 0.5f * u + (over ? 1.3f * u : 0) + 0.3f * u;
    float x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2, col = pw / 8.6f;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 80);
    ct_panel(x0, y0, pw, ph, 255);
    float y = y0 + 0.25f * u + hh / 2, cx[6] = {x0 + 0.75f * col, x0 + 2.2f * col, x0 + 3.55f * col, x0 + 4.9f * col,
                                               x0 + 6.25f * col, x0 + 7.75f * col};
    float id = 0.62f * u;
    ui_put_c(pic(PIC_SEQUENCE, id), cx[1], y, id, id);
    cards_draw(DECK_FRENCH, CARD_NONE, cx[2], y, 0.38f * u, 0, CD_DOWN);
    ui_put_c(ct_suit(DECK_FRENCH, SUIT_SPADES, (int)id), cx[3], y, id, id);
    y += hh / 2 + rh / 2;
    float nh = floorf(fminf(0.36f * rh, 0.4f * col)), d = fminf(0.62f * rh, 0.7f * u);
    for (int q = 0; q < PIQUET_P; q++, y += rh) {
        if (p->total[q] > p->total[1 - q]) ui_mark_gold(cx[0], y, 1.9f * d);
        ui_put_c(ct_token(q, (int)d), cx[0], y, d, d);
        ui_number(cx[1], y, nh, p->dpts[q], 1);
        ui_number(cx[2], y, nh, p->ppts[q], 1);
        if (p->capot == q) ui_put_c(pic(PIC_CROWN, 0.5f * u), cx[2] + 0.75f * col, y, 0.5f * u, 0.5f * u);
        if (p->bonus[q]) ui_number(cx[3], y, nh, p->bonus[q], 1);
        ui_number_signed(cx[4], y, nh, p->score[q], 1, 1);
        ui_number(cx[5], y, floorf(1.3f * nh), p->total[q], 1);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
    uint8_t who[PIQUET_DEALS];
    for (int i = 0; i < PIQUET_DEALS; i++) who[i] = (uint8_t)(i % PIQUET_P);   /* each deal's elder */
    y += -rh / 2 + 0.25f * u;
    ct_dots(x0 + pw / 2, y, floorf(0.3f * u), PIQUET_DEALS, PIQUET_DEALS, who, p->deal);
    if (!over) return;
    int w = piquet_winner(p), l = 1 - w;        /* the Rubicon: the winner's score; rubiconed: the loser's 100 struck */
    y += 0.25f * u + 0.65f * u;
    float td = 0.8f * u;
    ui_mark_gold(x0 + pw * 0.3f, y, 2.0f * td);
    ui_put_c(ct_token(w, (int)td), x0 + pw * 0.3f, y, td, td);
    ui_number(x0 + pw * 0.3f + 1.4f * td, y, floorf(0.6f * u), p->final, 1);
    if (!p->rubicon) return;
    ui_put_c(ct_token(l, (int)(0.6f * td)), x0 + pw * 0.68f, y, 0.6f * td, 0.6f * td);
    ui_number(x0 + pw * 0.68f + 1.1f * td, y, floorf(0.4f * u), 100, 1);
    ct_pic(CT_SLASH, x0 + pw * 0.68f + 1.1f * td, y, 0.6f * u, 0, 214, 70, 60, 255);
}

static void draw(void)
{
    const piquet_pos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&A), ph = p->phase, hide = ui.viewer < 0, mine = p->turn == T.me && !hide;
    if (!(seen >> T.me & 1) && !ui_handing()) {
        seen |= 1u << T.me;
        if (ph == PQ_EXCHANGE && !p->ex[T.me]) {
            uint8_t h[CT_HAND];
            int how[CT_HAND];
            ct_deal(&A, &T, h, hand(h, how, NULL, 0));
        }
    }
    draw_seats();
    float x, y;
    if (ph == PQ_EXCHANGE) {
        talon_at(&x, &y);
        cards_pile(DECK_FRENCH, x, y, pile_w(), PIQUET_TALON - p->top, CARD_NONE, 0);
        if (mine) draw_exchange(live);
    }
    int thrown = p->ex[0] + p->ex[1] - ct_hides(&A, TAG_THROWN, -1, -1);
    thrown_at(&x, &y);
    if (thrown > 0 && ph <= PQ_PLAY && !board_on()) cards_pile(DECK_FRENCH, x, y, pile_w(), thrown, CARD_NONE, 0);
    ct_draw_pile(&T, p->tricks);
    if (board_on()) {
        int step = ph == PQ_DECLARE ? p->turn != p->elder : 1;
        if ((step && !(hide && ph == PQ_DECLARE)) || mine) draw_board(step);   /* the answers: not under the cover */
        if (ph == PQ_DECLARE && mine) draw_tick(live);
    }
    ct_draw_trick(&T, &A, p->trick, NULL, p->ntrick, p->leader);
    if (F.raised != CARD_NONE && live) ct_draw_target(&T);
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    float lift[CT_HAND];
    int n = hand(h, how, lift, live);
    if (hide) ct_hand_backs(&T, n);
    else ct_hand_draw(&T, &A, h, n, how, lift, ct_dragged(&F));
    ct_draw(&A);
    if (!hide) ct_touch_draw(&F, &T);
    if (show_last && p->last_winner != TRICK_NONE)
        ct_draw_last(&T, p->last, NULL, PIQUET_P, p->last_leader, p->last_winner);
    if ((ph == PQ_ROUND || ph == PQ_OVER) && !ct_busy(&A)) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (piquet_can_play(&G.pos, card)) act(PQA_PLAY | card);
    else { ct_lower(&F); ui_sound(SND_DENY); }
}

static void press(float x, float y)
{
    const piquet_pos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last) { show_last = 0; return; }
    if (ct_busy(&A)) return;
    if (p->phase == PQ_ROUND) { act(PQA_NEXT); return; }
    if (p->phase == PQ_OVER) return;
    if (ct_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    float lift[CT_HAND];
    int n = hand(h, how, lift, 1);
    if (p->phase == PQ_EXCHANGE) {
        float tx, ty, td;
        tick_at(&tx, &ty, &td);
        if (ct_on(tx, ty, td, x, y)) {
            int k = ct_count(pick);
            if (k >= piquet_throw_min(p) && k <= piquet_throw_max(p)) act(PQA_EXCHANGE | (int)pick);
            else ui_sound(SND_DENY);
            return;
        }
        if (!ct_toggle(&T, n, how, &pick, ct_count(pick) < piquet_throw_max(p), x, y)) ui_sound(SND_DENY);
        return;
    }
    if (p->phase == PQ_DECLARE) {
        board_layout();
        float tx, ty, d;
        declare_at(&tx, &ty, &d);
        if (ct_on(tx, ty, d, x, y)) act(PQA_DECLARE);
        else ui_sound(SND_DENY);
        return;
    }
    if (F.raised != CARD_NONE && ct_on_target(&T, x, y)) { send(F.raised); return; }
    ct_touch_hand(&F, &T, h, n, how, lift, x, y);   /* a tap or a drag: the release says */
}

static void motion(float x, float y) { ct_touch_move(&F, x, y); }

static void release(float x, float y)
{
    int c, r = ct_touch_up(&F, &T, x, y, &c);
    if (r == CT_T_SEND) send(c);
    else if (r == CT_T_TAP && ct_tap_play(&F, c, piquet_can_play(&G.pos, c))) send(c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, T.deck, G.pos.n[player], G.pos.total[player]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: a quint to the ace in spades */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[4] = {CARD(SUIT_SPADES, RANK_JACK), CARD(SUIT_SPADES, RANK_QUEEN),
                                 CARD(SUIT_SPADES, RANK_KING), CARD(SUIT_SPADES, RANK_ACE)};
    ct_thumb(DECK_FRENCH, cards, 4, size, rgba);
}

static const game piquet_game_def = {
    .id = "piquet", .order = 319, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .view = VIEW_HIDDEN,
};
REGISTER_GAME(piquet_game_def)
