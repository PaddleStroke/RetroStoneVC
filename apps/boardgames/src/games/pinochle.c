/*
 * Pinochle in RetroStone Board: two players with a stock, or four in two partnerships with an auction, passing the
 * device round, each hand secret (VIEW_HIDDEN; the rules: pinochle_rules.c, pinochle.h). The trick table (the kit's
 * cards_table.h), the double pack of 48 (the French deck's 9 to ace).
 * Two players: the stock at the lower left with the turned trump across under it (then the trumps' sign); each
 * player's melded cards face up in a row, the viewer's above their hand, the other's beside their chip; they are
 * played from there like the hand's (tap one: it rises; again, or its place in the trick: it goes). After winning a
 * trick, a meld: tap its cards (those that make a meld glow), then the tick; or the arrow: no meld. Points rise from
 * the melder's chip.
 * Four players: the bid on a stepper (minus, the amount, plus), the tick to bid, or pass; each bid in a dark disc by
 * its chip. The bidder taps the trumps' suit. Then every player's melds on a panel (tap to play), the bidder's bid
 * and trumps by its chip, the tricks taken by each chip, the trumps' sign at the lower left.
 * After each deal a summary: each side's melds, counters, the deal's points (set: in a red glow) and the total.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/pinochle.h"
#include "games/tricks.h"
#include "pages.h"
#include "shell.h"

#define DRAW_MS 320
#define TABLE_BIT 0x80

static pino_game G;
static ct_table T;
static ct_moves A;
static ct_touch F;
static ct_floats FL;
static int floor_n, show_last, row_up = -1, pick;   /* row_up: the viewer's table card risen; pick: the bid shown */
static unsigned seen, mask, rmask;              /* a meld chosen: the hand's cards, the table row's */
static float ws, stx, sty, rw;                  /* the stock: its cards' width, centre; a meld row's cards' width */

static int P(void) { return G.players ? G.players : 2; }

static void layout(box b)
{
    ct_layout(&T, b, P(), ui_bottom(), PN_HAND);
    ws = floorf(0.72f * T.wt);
    stx = b.x + 0.35f * T.u + 0.5f * ws * T.asp;
    sty = T.hand_top - 0.5f * ws * T.asp - 0.15f * T.u;
    rw = floorf(0.62f * T.wt);
}

static void reset_ui(void)
{
    show_last = 0;
    row_up = -1;
    mask = rmask = 0;
    ct_touch_reset(&F);
}

/* a player's cards in the hand (fan) or on the table (row), with their slots in the rules' hand */
static int cards_of(int pl, int table, uint8_t *h, uint8_t *slot)
{
    const pino_pos *p = &G.pos;
    int n = 0;
    for (int i = 0; i < p->n[pl]; i++)
        if (!(p->flag[pl][i] & PK_TABLE) == !table) {
            h[n] = p->hand[pl][i];
            if (slot) slot[n] = (uint8_t)i;
            n++;
        }
    return n;
}

/* the cards of the melds the viewer may make now (bit: the card's byte & 127) */
static void meld_cards(uint64_t *bits)
{
    const pino_pos *p = &G.pos;
    bits[0] = bits[1] = 0;
    if (p->phase != PP_MELD) return;
    for (int k = 0; k < PM_KINDS; k++)
        for (int s = 0; s < (k == PM_MARRIAGE ? 4 : 1); s++) {
            if (!pinochle_legal(p, PA_MELD_OF(k, s))) continue;
            uint8_t m[8];
            int n = pinochle_meld_cards(k, s, p->trump, m);
            for (int i = 0; i < n; i++) bits[m[i] >> 6] |= 1ull << (m[i] & 63);
        }
}

static int in_bits(const uint64_t *bits, int c) { return (int)(bits[c >> 6 & 1] >> (c & 63) & 1); }

static int hand(uint8_t *h, int *how, int live)
{
    const pino_pos *p = &G.pos;
    int n = cards_of(T.me, 0, h, NULL), mine = live && p->turn == T.me;
    uint64_t bits[2];
    meld_cards(bits);
    for (int i = 0; i < n; i++) {
        how[i] = 0;
        if (mine && p->phase == PP_PLAY) how[i] = pinochle_can_play(p, h[i]) ? CD_GLOW : CD_DIM;
        if (mine && p->phase == PP_MELD) how[i] = mask >> i & 1 ? CD_RAISED : in_bits(bits, h[i]) ? CD_GLOW : CD_DIM;
        if (ct_is_raised(&F, i)) how[i] = CD_RAISED;
    }
    return n;
}

/* the meld rows: each player's to the right of its chip, clear of its card in the trick (the viewer's up to the
 * trick pile, the other's to the table's edge, inside the table) */
static void row_spot(int pl, int k, int n, float *x, float *y)
{
    float x0 = T.sx[pl] + fmaxf(1.4f * T.tok, 0.5f * T.wt + 0.1f * T.u) + 0.5f * rw;   /* past the tricks taken */
    float right = pl == T.me ? T.px - 0.5f * T.pw - 0.5f * rw - 0.15f * T.u : T.b.x + T.b.w - 0.5f * rw - 0.1f * T.u;
    float step = n > 1 ? fminf(0.7f * rw, fmaxf(0.2f * rw, (right - x0) / (n - 1))) : 0;
    *x = x0 + k * step;
    *y = pl == T.me ? T.sy[pl] : fmaxf(T.sy[pl], T.b.y + 0.5f * rw * T.asp + 0.05f * T.u);
}

static int row_hit(float x, float y)
{
    uint8_t h[PN_HAND];
    int n = cards_of(T.me, 1, h, NULL);
    for (int k = n - 1; k >= 0; k--) {
        float cx, cy;
        row_spot(T.me, k, n, &cx, &cy);
        if (k == row_up || rmask >> k & 1) cy -= 0.15f * rw;
        if (cards_in(cx, cy, rw, T.asp, x, y)) return k;
    }
    return -1;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    pinochle_new(&G, ui.players, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    FL.n = 0;
    reset_ui();
    floor_n = 0;
    seen = 0;
    pick = PN_MIN_BID;
}

static int status(void)
{
    int w = pinochle_winner(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static unsigned winners(void)
{
    int w = pinochle_winner(&G.pos);
    if (w < 0) return 0;
    return G.players == 4 ? 1u << w | 1u << (w + 2) : 1u << w;
}

static int turn(void) { return G.pos.turn; }

static void trump_at(float *x, float *y)
{
    *x = stx + 0.42f * ws;
    *y = sty;
}

static void draws(int delay)
{
    const pino_pos *p = &G.pos;
    for (int k = 0; k < 2; k++) {
        int q = (p->last_winner + k) % 2, c = p->drew[k], last = c != CARD_NONE && p->nstock == 0 && k == 1;
        if (c == CARD_NONE) continue;
        float x0 = stx, y0 = sty, a0 = 0;
        if (last) trump_at(&x0, &y0), a0 = 90;
        if (q == T.me) {
            uint8_t h[PN_HAND];
            int n = cards_of(q, 0, h, NULL);
            cards_spot s[CT_HAND];
            ct_fan(&T, n, s);
            int i = ct_index(h, n, c);
            if (i < 0) continue;
            ct_slide(&A, T.deck, c, x0, y0, ws, a0, last, s[i].x, s[i].y, T.wh, s[i].deg, 1, DRAW_MS,
                     delay + 200 * k, CT_HAND_TAG, i, -1, SND_MOVE);
        } else
            ct_slide(&A, T.deck, c, x0, y0, ws, a0, last, T.sx[q], T.sy[q], 0.45f * T.wt, T.tdeg[q], 0, DRAW_MS,
                     delay + 200 * k, CT_SEAT, q, -1, SND_MOVE);
    }
}

static void animate(const pino_pos *b, int a, const uint8_t *h, int n, const float *from)
{
    const pino_pos *p = &G.pos;
    int k = PA_KIND(a);
    if (k == PA_BID || k == PA_PASS || k == PA_TRUMP) { ui_sound(SND_PLACE); return; }
    if (k == PA_MELD || k == PA_SKIP) {
        if (k == PA_MELD) {
            ui_sound(SND_PLACE);
            ct_float(&FL, &A, T.sx[b->turn], T.sy[b->turn] - 0.8f * T.tok, pinochle_meld_value(PA_ARG(a) >> 2, 2),
                     0, ui_ms());
        }
        draws(k == PA_MELD ? 500 : 100);
        return;
    }
    if (k != PA_PLAY) return;
    int c = PA_ARG(a) & ~TABLE_BIT, pl = b->turn;
    if (from)                                   /* from the table row */
        ct_rest(&A, ct_slide(&A, T.deck, c, from[0], from[1], rw, 0, 1, T.tx[pl], T.ty[pl], T.wt, T.tdeg[pl], 1,
                             CT_SLIDE_MS, 0, CT_TRICK, pl, -1, SND_PLACE), 200);
    else if (pl == T.me) ct_play(&A, &T, &F, h, n, c);
    else
        ct_rest(&A, ct_slide(&A, T.deck, c, T.sx[pl], T.sy[pl], 0.45f * T.wt, T.tdeg[pl], 0, T.tx[pl], T.ty[pl], T.wt,
                             T.tdeg[pl], 1, CT_SLIDE_MS, 0, CT_TRICK, pl, -1, SND_PLACE), 200);
    if (p->tricks == b->tricks || p->last_winner == PN_NONE) return;
    int look = CT_SLIDE_MS + CT_LOOK_MS;
    ct_gather(&A, &T, p->last, NULL, p->players, p->last_leader, p->last_winner, look, CT_SLIDE_MS);
    if (p->drew[0] != CARD_NONE) draws(look + 520);
}

static void act_from(int a, const float *from)
{
    pino_pos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 0);
    if (!pinochle_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    animate(&before, a, h, n, from);
    reset_ui();
    const pino_pos *p = &G.pos;
    if (PA_KIND(a) == PA_NEXT) seen = 0;
    if (p->phase == PP_BID) pick = p->high < PN_MIN_BID ? PN_MIN_BID : p->high + 1;
    if (p->turn != before.turn || p->phase != before.phase) floor_n = G.n;
    if (p->phase != PP_ROUND && p->phase != PP_OVER && p->phase != PP_MELDS) ui_handoff(p->turn);
}

static void act(int a) { act_from(a, NULL); }

static int undo(void)
{
    if (mask || rmask || row_up >= 0 || F.raised != CARD_NONE || show_last) { reset_ui(); return 1; }
    if (G.n <= floor_n || !pinochle_undo(&G)) return 0;
    ct_calm(&A);
    FL.n = 0;
    reset_ui();
    pick = G.pos.high < PN_MIN_BID ? PN_MIN_BID : G.pos.high + 1;
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = pinochle_script(&G, s);
    ct_calm(&A);
    FL.n = 0;
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    pick = G.pos.high < PN_MIN_BID ? PN_MIN_BID : G.pos.high + 1;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&A); }

/* ---- the bot's side (pinochle_bot.c) -------------------------------------------------------------------------------- */
const pino_game *pinochle_live(void) { return &G; }

int pinochle_show(const char *move)
{
    static pino_game t;
    t = G;
    if (!pinochle_script(&t, move) || t.n != G.n + 1) return 0;
    int a = t.act[G.n];
    float from[2], *fp = NULL;
    if (PA_KIND(a) == PA_PLAY && G.players == 2) {   /* a card from the other's table row: from there */
        const pino_pos *p = &G.pos;
        int pl = p->turn, c = PA_ARG(a) & ~TABLE_BIT, hidden = 0;
        uint8_t h[PN_HAND];
        int n = cards_of(pl, 1, h, NULL);
        for (int i = 0; i < p->n[pl]; i++) hidden |= p->hand[pl][i] == c && !(p->flag[pl][i] & PK_TABLE);
        int k = ct_index(h, n, c);
        if (k >= 0 && (!hidden || PA_ARG(a) & TABLE_BIT)) row_spot(pl, k, n, &from[0], &from[1]), fp = from;
    }
    act_from(a, fp);
    return 1;
}

/* ---- the 4-player calls' places ------------------------------------------------------------------------------------ */
static float call_d(void) { return floorf(fminf(1.05f * T.u, T.b.w / 7.0f)); }

/* the bid: minus, the amount, plus in a row; pass and the tick under it */
static void bid_spots(float *x, float *y)
{
    float d = call_d();
    x[0] = T.cx - 1.25f * d, x[1] = T.cx, x[2] = T.cx + 1.25f * d, x[3] = T.cx - 0.7f * d, x[4] = T.cx + 0.7f * d;
    y[0] = y[1] = y[2] = T.cy - 0.6f * d;
    y[3] = y[4] = T.cy + 0.7f * d;
}

static void suit_spot(int s, float *x, float *y)
{
    float d = call_d();
    *x = T.cx + (s - 1.5f) * 1.2f * d;
    *y = T.cy;
}

/* two players' meld buttons: the tick, the arrow (no meld) */
static void meld_spots(float *x, float *y)
{
    float d = call_d();
    x[0] = T.cx - 0.75f * d, x[1] = T.cx + 0.75f * d;
    y[0] = y[1] = T.cy;
}

/* the cards chosen for a meld (hand and row): the action they make (0: none) */
static int chosen_meld(void)
{
    uint8_t h[PN_HAND], r[PN_HAND], m[8];
    int nh = cards_of(T.me, 0, h, NULL), nr = cards_of(T.me, 1, r, NULL), k = 0;
    for (int i = 0; i < nh && k < 8; i++)
        if (mask >> i & 1) m[k++] = h[i];
    for (int i = 0; i < nr && k < 8; i++)
        if (rmask >> i & 1) m[k++] = r[i];
    int a = k ? pinochle_meld_of(m, k, G.pos.trump) : 0;
    return a && pinochle_legal(&G.pos, a) ? a : 0;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void corner_draw(const pino_pos *p)
{
    if (p->trump == PN_NONE) return;
    float tx, ty;
    trump_at(&tx, &ty);
    if (p->players == 2) {
        int n = p->nstock;
        for (int i = 0; i < A.n; i++)
            if (cards_slide_busy(&A.m[i].s) && (A.m[i].tag == CT_SEAT || A.m[i].tag == CT_HAND_TAG) &&
                ui_ms() < A.m[i].s.t0 + (Uint32)A.m[i].s.delay)
                n++;
        ct_talon(T.deck, stx, sty, tx, ty, ws, n, n ? p->up : CARD_NONE, p->trump, 0);
        return;
    }
    float d = 0.75f * ws;
    ui_put_c(ct_suit(T.deck, p->trump, (int)d), stx, sty, d, d);
}

static void rows_draw(const pino_pos *p, int live)
{
    for (int pl = 0; pl < 2; pl++) {
        uint8_t h[PN_HAND];
        int n = cards_of(pl, 1, h, NULL), mine = pl == T.me && ui.viewer >= 0;
        uint64_t bits[2];
        meld_cards(bits);
        for (int k = 0; k < n; k++) {
            float x, y;
            row_spot(pl, k, n, &x, &y);
            int how = 0;
            if (mine && live && p->turn == T.me) {
                if (p->phase == PP_PLAY) how = pinochle_can_play(p, h[k] | TABLE_BIT) ? CD_GLOW : CD_DIM;
                if (p->phase == PP_MELD) how = in_bits(bits, h[k]) ? CD_GLOW : CD_DIM;
            }
            if (mine && (k == row_up || rmask >> k & 1)) how = CD_RAISED, y -= 0.15f * rw;
            cards_draw(T.deck, h[k], x, y, rw, 0, how);
        }
    }
}

static void draw_calls(const pino_pos *p, int live)
{
    if (p->turn != T.me || ui.viewer < 0 || !live) return;
    float d = call_d();
    if (p->phase == PP_BID) {
        float x[5], y[5];
        bid_spots(x, y);
        int lo = p->high < PN_MIN_BID ? PN_MIN_BID : p->high + 1;
        ct_step(x[0], y[0], 0.8f * d, 0, pick > lo);
        ct_amount(x[1], y[1], 1.1f * d, pick, 0, 1);
        ct_step(x[2], y[2], 0.8f * d, 1, pick < 250);
        if (pinochle_legal(p, PA_PASS)) ct_pass(x[3], y[3], d, 0);
        ui_mark_gold(x[4], y[4], 1.8f * d);
        ct_button(x[4], y[4], d, 1, CT_TICK, 0);
    } else if (p->phase == PP_TRUMP)
        for (int s = 0; s < 4; s++) {
            float x, y;
            suit_spot(s, &x, &y);
            ct_choice(ct_suit(T.deck, s, (int)(0.7f * d)), x, y, d, 0, 1);
        }
    else if (p->phase == PP_MELD) {
        float x[2], y[2];
        meld_spots(x, y);
        int ok = chosen_meld() != 0;
        if (ok) ui_mark_gold(x[0], y[0], 1.8f * d);
        ct_button(x[0], y[0], d, ok, CT_TICK, 0);
        ct_button(x[1], y[1], d, 0, CT_ARROW, 0);
    }
}

/* four players: each chip's bid (a dark disc) or pass; the bidder's bid and trumps; the tricks taken */
static void seats_draw(const pino_pos *p)
{
    for (int q = 0; q < p->players; q++) {
        float x, y, x2, y2;
        ct_seat_spot(&T, q, 0, &x, &y);
        ct_seat_spot(&T, q, 1, &x2, &y2);
        if (p->players == 4 && p->phase == PP_BID) {
            if (p->passed >> q & 1) ct_pass_badge(x, y, 0.95f * T.tok);
            else if (p->bid[q]) ct_badge(x, y, 0.95f * T.tok, p->bid[q]);
            continue;
        }
        if (p->players == 4 && q == p->bidder && p->phase != PP_ROUND && p->phase != PP_OVER) {
            ct_badge(x, y, 0.95f * T.tok, p->high);
            if (p->trump != PN_NONE) {
                float d = 0.75f * T.tok;
                ui_put_c(ct_suit(T.deck, p->trump, (int)d), x2, y2, d, d);
            }
            continue;
        }
        if (p->phase == PP_PLAY || p->phase == PP_MELD) {
            if (p->won[q]) ui_number(x, y, 0.5f * T.tok, p->won[q], TH.dark);   /* (the table's ink) */
        }
    }
}

/* four players: every player's melds, a row each: the chip, the cards, the points */
static void draw_melds(const pino_pos *p)
{
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, 4, &x0, &pw, &y, &rh);
    float d = fminf(0.6f * rh, 0.7f * u);
    for (int q = 0; q < 4; q++, y += rh) {
        int pl = (p->bidder + q) % 4;
        float tx = x0 + 0.6f * u;
        if (pl == p->bidder) ui_mark_gold(tx, y, 1.8f * d);
        ui_put_c(ct_token(pl, (int)d), tx, y, d, d);
        float cw = fminf(0.7f * rh / T.asp, 0.8f * u), left = tx + 0.7f * u, right = x0 + pw - 1.5f * u;
        int n = p->nm[pl];
        float step = n > 1 ? fminf(0.75f * cw, (right - left - cw) / (n - 1)) : 0;
        for (int i = 0; i < n; i++) cards_draw(T.deck, p->mcards[pl][i], left + cw / 2 + i * step, y, cw, 0, CD_FLAT);
        ui_number(x0 + pw - 0.75f * u, y, 0.45f * rh, p->meldpts[pl], 1);
        if (q < 3) ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
}

/* the deal's summary: a row a side, its chips, its melds (struck out: not counted), its counters, the deal's points
 * (set: a red glow), the total */
static void draw_summary(const pino_pos *p)
{
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, 2, &x0, &pw, &y, &rh);
    rh = fminf(rh, 1.4f * u);
    float d = fminf(0.62f * rh, 0.75f * u);
    int four = p->players == 4, top = p->score[0] >= p->score[1] ? 0 : 1;
    for (int s = 0; s < 2; s++, y += rh) {
        float tx = x0 + 0.65f * u;
        if (s == top && p->score[0] != p->score[1]) ui_mark_gold(tx + (four ? 0.42f * d : 0), y, 1.9f * d);
        for (int k = 0; k < (four ? 2 : 1); k++) ui_put_c(ct_token(s + 2 * k, (int)d), tx + k * 0.85f * d, y, d, d);
        if (four && s == (p->bidder & 1)) ct_badge(tx + 1.75f * d, y, 0.6f * u, p->high);
        float mx = x0 + pw * 0.42f;
        ct_call(CT_CALL_PAIR + CARD(SUIT_HEARTS, RANK_KING), mx - 0.09f * pw, y, 0.6f * rh, 255);
        ui_number(mx, y, 0.38f * rh, p->melds[s], 1);
        if (four && p->won[s] + p->won[s + 2] == 0 && p->melds[s]) ct_strike(mx, y, 0.8f * u);
        ct_card_flat(T.deck, CARD(SUIT_HEARTS, RANK_ACE), x0 + pw * 0.5f, y, 0.5f * rh, 0);
        ui_number(x0 + pw * 0.58f, y, 0.38f * rh, p->counters[s], 1);
        float px = x0 + pw * 0.72f;
        if (four && p->set && s == (p->bidder & 1)) ui_mark_glow(px, y, 1.4f * u);
        if (four) ui_number_signed(px, y, 0.4f * rh, p->pts[s], 1, 1);
        ui_number_signed(x0 + pw - 0.8f * u, y, 0.5f * rh, p->score[s], 1, 0);
        if (s == 0) ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    const pino_pos *p = &G.pos;
    if (T.me != ui_bottom() && ui_bottom() >= 0) layout(T.b);
    int live = !ui_handing() && !ct_busy(&A), hide = ui.viewer < 0, ph = p->phase;
    if (!(seen >> T.me & 1) && !ui_handing()) {
        seen |= 1u << T.me;
        if (p->tricks == 0 && p->ntrick == 0 && (ph == PP_BID || ph == PP_PLAY)) {
            uint8_t h[CT_HAND];
            int how[CT_HAND];
            ct_deal(&A, &T, h, hand(h, how, 0));
        }
    }
    ct_draw_seats(&T, ph == PP_ROUND || ph == PP_OVER || ph == PP_MELDS ? -1 : p->turn);
    seats_draw(p);
    ct_draw_pile(&T, p->tricks);
    corner_draw(p);
    if (p->players == 2) rows_draw(p, live);
    ct_draw_trick(&T, &A, p->trick, NULL, p->ntrick, p->leader);
    if ((F.raised != CARD_NONE || row_up >= 0) && live && ph == PP_PLAY) ct_draw_target(&T);
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, live);
    if (hide) ct_hand_backs(&T, n);
    else ct_hand_draw(&T, &A, h, n, how, NULL, ct_dragged(&F));
    if (!ct_hides(&A, CT_HAND_TAG, -1, -1)) draw_calls(p, live);
    ct_draw(&A);
    if (!hide) ct_touch_draw(&F, &T);
    const uint8_t pill[4] = {24, 30, 28, 255};
    ct_floats_draw(&FL, ct_round((int)(1.5f * T.u), (int)(0.56f * T.u), 0.28f * T.u, pill), NULL);
    if (show_last && p->last_winner != PN_NONE)
        ct_draw_last(&T, p->last, NULL, p->players, p->last_leader, p->last_winner);
    if (ph == PP_MELDS && !ct_busy(&A)) draw_melds(p);
    if ((ph == PP_ROUND || ph == PP_OVER) && !ct_busy(&A)) draw_summary(p);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (pinochle_can_play(&G.pos, card)) act(PA_PLAY | card);
    else { ct_lower(&F); ui_sound(SND_DENY); }
}

static void send_row(int k)
{
    uint8_t h[PN_HAND];
    int n = cards_of(T.me, 1, h, NULL);
    if (k < 0 || k >= n || !pinochle_can_play(&G.pos, h[k] | TABLE_BIT)) { row_up = -1; ui_sound(SND_DENY); return; }
    float from[2];
    row_spot(T.me, k, n, &from[0], &from[1]);
    from[1] -= 0.15f * rw;
    act_from(PA_PLAY | h[k] | TABLE_BIT, from);
}

static int press_calls(const pino_pos *p, float x, float y)
{
    float d = call_d();
    if (p->phase == PP_BID) {
        float bx[5], by[5];
        bid_spots(bx, by);
        int lo = p->high < PN_MIN_BID ? PN_MIN_BID : p->high + 1;
        if (pick < lo) pick = lo;
        if (ct_on(bx[0], by[0], 0.8f * d, x, y)) {
            if (pick > lo) pick--, ui_sound(SND_TAP);
            else ui_sound(SND_DENY);
        } else if (ct_on(bx[2], by[2], 0.8f * d, x, y)) {
            if (pick < 250) pick++, ui_sound(SND_TAP);
        } else if (ct_on(bx[3], by[3], d, x, y) && pinochle_legal(p, PA_PASS)) act(PA_PASS);
        else if (ct_on(bx[4], by[4], d, x, y) || ct_on(bx[1], by[1], 1.1f * d, x, y)) act(PA_BID | pick);
        else ui_sound(SND_DENY);
        return 1;
    }
    if (p->phase == PP_TRUMP) {
        for (int s = 0; s < 4; s++) {
            float sx, sy;
            suit_spot(s, &sx, &sy);
            if (ct_on(sx, sy, d, x, y)) { act(PA_TRUMP | s); return 1; }
        }
        ui_sound(SND_DENY);
        return 1;
    }
    if (p->phase == PP_MELD) {
        float mx[2], my[2];
        meld_spots(mx, my);
        if (ct_on(mx[1], my[1], d, x, y)) { act(PA_SKIP); return 1; }
        if (ct_on(mx[0], my[0], d, x, y)) {
            int a = chosen_meld();
            if (a) act(a);
            else ui_sound(SND_DENY);
            return 1;
        }
        int k = row_hit(x, y);
        if (k >= 0) { rmask ^= 1u << k; ui_sound(SND_TAP); return 1; }
        uint8_t h[CT_HAND];
        int how[CT_HAND];
        int n = hand(h, how, 1);
        if (!ct_toggle(&T, n, how, &mask, 1, x, y)) ui_sound(SND_DENY);
        return 1;
    }
    return 0;
}

static void press(float x, float y)
{
    const pino_pos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last) { show_last = 0; return; }
    if (ct_busy(&A)) return;
    if (p->phase == PP_ROUND) { act(PA_NEXT); return; }
    if (p->phase == PP_MELDS) { act(PA_GO); return; }
    if (p->phase == PP_OVER) return;
    if (ct_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    if (press_calls(p, x, y)) return;
    if ((F.raised != CARD_NONE || row_up >= 0) && ct_on_target(&T, x, y)) {
        if (row_up >= 0) send_row(row_up);
        else send(F.raised);
        return;
    }
    if (p->players == 2) {
        int k = row_hit(x, y);
        if (k >= 0) {
            if (k == row_up) send_row(k);
            else ct_lower(&F), row_up = k, ui_sound(SND_TAP);
            return;
        }
    }
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 1);
    row_up = -1;
    ct_touch_hand(&F, &T, h, n, how, NULL, x, y);
}

static void motion(float x, float y) { ct_touch_move(&F, x, y); }

static void release(float x, float y)
{
    int c, r = ct_touch_up(&F, &T, x, y, &c);
    if (r == CT_T_SEND) send(c);
    else if (r == CT_T_TAP && ct_tap_play(&F, c, pinochle_can_play(&G.pos, c))) send(c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    ct_bar(b, T.deck, G.pos.n[player], G.pos.score[pinochle_side(&G.pos, player)]);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: a pinochle (the queen of spades and the jack of diamonds) on a run's king and queen */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[4] = {CARD(SUIT_HEARTS, RANK_KING), CARD(SUIT_HEARTS, RANK_QUEEN),
                                 CARD(SUIT_DIAMONDS, RANK_JACK), CARD(SUIT_SPADES, RANK_QUEEN)};
    ct_thumb(DECK_FRENCH, cards, 4, size, rgba);
}

static const game pinochle_game_def = {
    .id = "pinochle", .order = 329, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(pinochle_game_def)
