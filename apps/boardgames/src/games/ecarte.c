/*
 * Ecarte in RetroStone Board: two players passing the device, each hand secret (VIEW_HIDDEN; the rules: ecarte_rules.c,
 * ecarte.h). The table (the kit's cards_table.h): the viewer's five cards fanned at the bottom, the other's backs at
 * the top beside their chip; on the left the stock and the turned card (its suit is trumps), on the right the cards
 * thrown, face down; the trick in the middle.
 * - The elder proposes by tapping the stock (it glows), or plays a card at once (tap it, tap it again or drag it).
 * - The dealer answers: the tick (yes, cards may be changed) or the cross (no: the play begins).
 * - Changing cards: tap the cards to throw (they rise; the elder at least one), then the tick: they go to the thrown
 *   pile and as many come from the stock, standing a little higher. Beside the other's chip: how many they changed.
 * - The king of trumps, held, shows small beside its holder's chip (its point counted).
 * - Playing: tap a card (it rises), tap it again, or drag it to the middle; the cards that may not be played are dim.
 * After each deal a summary: each player's chip, the king's point, the tricks taken, the deal's points, the total (5
 * wins).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/ecarte.h"
#include "games/tricks.h"
#include "shell.h"

#define LIFT 0.16f                              /* the cards just drawn stand this much higher (card widths) */
enum { TAG_THROWN = 10, TAG_STOCK };

static ecarte_game G;
static ct_table T;
static ct_moves A;
static ct_touch F;
static int floor_n, show_last;
static unsigned pick, seen;                     /* pick: the cards chosen to throw (bit i: the hand's i-th) */

static void layout(box b) { ct_layout(&T, b, ECARTE_P, ui_bottom(), ECARTE_HAND); }

static void reset_ui(void)
{
    pick = 0;
    show_last = 0;
    ct_touch_reset(&F);
}

/* ---- the places: the stock and the turned card on the left, the thrown cards on the right, the buttons --------- */
static float pile_w(void) { return floorf(0.8f * T.wt); }
static void stock_at(float *x, float *y) { *x = T.b.x + 0.62f * pile_w() + 0.2f * T.u, *y = T.cy; }
static void turned_at(float *x, float *y)
{
    stock_at(x, y);
    *x += 0.78f * pile_w();
    *y += 0.1f * pile_w();
}
static void thrown_at(float *x, float *y) { *x = T.b.x + T.b.w - 0.62f * pile_w() - 0.2f * T.u, *y = T.cy; }

static void buttons(float *yes_x, float *no_x, float *y, float *d)
{
    *d = 1.15f * T.u;
    *y = T.cy;
    *yes_x = T.cx - 0.75f * *d;
    *no_x = T.cx + 0.75f * *d;
}

/* the cards just drawn by the viewer: stand higher until the next exchange or the first card played */
static int fresh(int card)
{
    const ecarte_pos *p = &G.pos;
    if (p->n[T.me] < ECARTE_HAND || p->phase > EC_PLAY || (p->phase == EC_DISCARD && p->turn == T.me)) return 0;
    for (int i = 0; i < ECARTE_HAND; i++)
        if (p->got[T.me][i] == card) return 1;
    return 0;
}

static int hand(uint8_t *h, int *how, float *lift, int live)
{
    const ecarte_pos *p = &G.pos;
    int n = p->n[T.me], mine = live && p->turn == T.me;
    int play = mine && (p->phase == EC_PLAY || p->phase == EC_PROPOSE);
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][i];
        h[i] = (uint8_t)c;
        how[i] = play ? (ecarte_can_play(p, c) ? CD_GLOW : CD_DIM) : 0;
        if (ct_is_raised(&F, i)) how[i] = CD_RAISED;
        if (lift) lift[i] = fresh(c) ? LIFT : 0;
    }
    if (mine && p->phase == EC_DISCARD)
        ct_chosen_hows(n, pick, ct_count(pick) < ecarte_throw_max(p), how);
    return n;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    ecarte_new(&G, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    int w = ecarte_winner(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static int turn(void) { return G.pos.turn; }

/* the exchange: the thrown cards to their pile face down, as many from the stock to the hand (turning up) */
static void animate_exchange(const ecarte_pos *b, unsigned mask, const uint8_t *h, int n)
{
    const ecarte_pos *p = &G.pos;
    int me = b->turn;
    if (me != T.me) return;
    cards_spot s[CT_HAND];
    ct_fan(&T, n, s);
    float tx, ty, sx, sy;
    thrown_at(&tx, &ty);
    stock_at(&sx, &sy);
    for (int i = 0, k = 0; i < n; i++) {
        if (!(mask >> i & 1)) continue;
        ct_slide(&A, DECK_FRENCH, h[i], s[i].x, s[i].y - 0.14f * T.wh, T.wh, s[i].deg, 1, tx, ty, pile_w(), 0, 0,
                 CT_SLIDE_MS, 70 * k, TAG_THROWN, 0, -1, k == 0 ? SND_MOVE : -1);
        k++;
    }
    cards_spot ns[CT_HAND];
    ct_fan(&T, p->n[me], ns);
    for (int i = 0, k = 0; i < p->n[me]; i++) {
        int c = p->hand[me][i];
        if (trick_find(b->hand[me], b->n[me], c) >= 0) continue;
        ct_rest(&A, ct_slide(&A, DECK_FRENCH, c, sx, sy, pile_w(), 0, 0, ns[i].x, ns[i].y, T.wh, ns[i].deg, 1,
                             CT_SLIDE_MS + 100, CT_SLIDE_MS + 120 + 90 * k, CT_HAND_TAG, i, -1,
                             k == 0 ? SND_PLACE : -1), 250);
        k++;
    }
}

static void animate(const ecarte_pos *b, int a, const uint8_t *h, int n)
{
    const ecarte_pos *p = &G.pos;
    switch (ECA_KIND(a)) {
    case ECA_ASK:
    case ECA_YES:
    case ECA_NO: ui_sound(SND_TAP); return;
    case ECA_DISCARD: animate_exchange(b, (unsigned)ECA_ARG(a), h, n); return;
    case ECA_PLAY:
        ct_play(&A, &T, &F, h, n, ECA_ARG(a));
        if (p->tricks != b->tricks || p->deal != b->deal)
            ct_gather(&A, &T, p->last, NULL, ECARTE_P, p->last_leader, p->last_winner, CT_SLIDE_MS + CT_LOOK_MS,
                      CT_SLIDE_MS);
        return;
    }
}

static void act(int a)
{
    ecarte_pos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, NULL, 0);
    if (!ecarte_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    animate(&before, a, h, n);
    reset_ui();
    const ecarte_pos *p = &G.pos;
    if (ECA_KIND(a) == ECA_NEXT) seen = 0;
    if (p->turn != before.turn || p->phase == EC_ROUND || p->phase == EC_OVER) floor_n = G.n;
    if (p->phase <= EC_PLAY) ui_handoff(p->turn);
}

static int undo(void)
{
    if (pick || F.raised != CARD_NONE || show_last) { reset_ui(); return 1; }
    if (G.n <= floor_n || !ecarte_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = ecarte_script(&G, s);
    ct_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&A); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_piles(int live)
{
    const ecarte_pos *p = &G.pos;
    float x, y, w = pile_w();
    stock_at(&x, &y);
    int ask = live && p->phase == EC_PROPOSE && p->turn == T.me && p->nstock > 0;
    if (ask) ui_mark_gold(x, y, 1.9f * w);
    if (p->nstock) cards_pile(DECK_FRENCH, x, y, w, p->nstock, CARD_NONE, ask ? CD_GLOW : 0);
    else cards_place(DECK_FRENCH, x, y, w);
    turned_at(&x, &y);
    cards_draw(DECK_FRENCH, p->turned, x, y, w, 0, 0);
    thrown_at(&x, &y);
    int n = p->thrown - ct_hides(&A, TAG_THROWN, -1, -1);
    if (n > 0) cards_pile(DECK_FRENCH, x, y, w, n, CARD_NONE, 0);
}

/* the other's hand: backs beside their chip; how many they changed; the king of trumps held */
static void draw_seats(void)
{
    const ecarte_pos *p = &G.pos;
    ct_draw_seats(&T, p->phase <= EC_PLAY ? p->turn : -1);
    int o = 1 - T.me;
    float bw = floorf(0.45f * T.wt);
    ct_backs(DECK_FRENCH, T.sx[o] + 1.4f * T.tok + 1.4f * bw, T.sy[o], 2.6f * bw, bw, p->n[o], 0);
    for (int q = 0; q < ECARTE_P; q++) {
        float x, y;
        ct_seat_spot(&T, q, 0, &x, &y);
        if (q == o) x = T.sx[o] - (x - T.sx[o]);   /* the other side of the chip: the backs are on its right */
        if (p->phase == EC_PLAY) ui_number(x, y, 0.5f * T.tok, p->won[q], 1);
        else if (p->phase <= EC_DISCARD && p->drew[q]) ct_badge(x, y, 0.95f * T.tok, p->drew[q]);
        if (p->king[q] && CARD_RANK(p->turned) != RANK_KING && p->phase == EC_PLAY) {
            ct_seat_spot(&T, q, 1, &x, &y);
            if (q == o) x = T.sx[o] - (x - T.sx[o]);
            cards_draw(DECK_FRENCH, CARD(p->trump, RANK_KING), x, y, 0.8f * T.tok, 0, 0);
        }
    }
}

static void draw_answer(int live)
{
    float yx, nx, y, d;
    buttons(&yx, &nx, &y, &d);
    if (live) ui_mark_gold(yx, y, 1.7f * d);
    ct_button(yx, y, d, 1, CT_TICK, 0);
    ct_button(nx, y, d, 0, -1, 0);
    int s = (int)(0.6f * d);
    ui_put_c(ct_icon(ICON_CLOSE, s, 255, 255, 255), nx, y, (float)s, (float)s);
}

static void draw_throw(int live)
{
    const ecarte_pos *p = &G.pos;
    int k = ct_count(pick), ok = k >= ecarte_throw_min(p) && k <= ecarte_throw_max(p);
    float d = 1.15f * T.u;
    if (ok && live) ui_mark_gold(T.cx, T.cy, 1.8f * d);
    ct_button(T.cx, T.cy, d, ok, CT_TICK, 0);
    if (k) ct_badge(T.cx + 0.85f * d, T.cy - 0.45f * d, 0.55f * d, k);
}

/* the deal's summary: a row a player, their chip, the king's point, the tricks, the deal's points, the total */
static void draw_summary(void)
{
    const ecarte_pos *p = &G.pos;
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, ECARTE_P, &x0, &pw, &y, &rh);
    float d = fminf(0.72f * rh, 0.8f * u);
    for (int q = 0; q < ECARTE_P; q++, y += rh) {
        float tx = x0 + 0.8f * u, nh = 0.42f * rh;
        if (p->total[q] >= p->total[1 - q] && p->total[q] > 0) ui_mark_gold(tx, y, 2.0f * d);
        ui_put_c(ct_token(q, (int)d), tx, y, d, d);
        if (p->king[q]) cards_draw(DECK_FRENCH, CARD(p->trump, RANK_KING), x0 + pw * 0.3f, y, 0.55f * d, 0, 0);
        cards_draw(DECK_FRENCH, CARD_NONE, x0 + pw * 0.45f, y, 0.42f * d, 0, CD_DOWN);
        ui_number(x0 + pw * 0.45f + 0.5f * d, y, nh, p->won[q], 1);
        if (p->score[q]) ui_number_signed(x0 + pw * 0.66f, y, nh, p->score[q], 1, 1);
        ui_number(x0 + pw * 0.86f, y, 0.56f * rh, p->total[q], 1);
        if (!q) ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    const ecarte_pos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&A), ph = p->phase, hide = ui.viewer < 0;
    int mine = p->turn == T.me && !hide;
    if (!(seen >> T.me & 1) && !ui_handing()) {
        seen |= 1u << T.me;
        if (ph == EC_PROPOSE && !p->asked && !p->ntrick) {
            uint8_t h[CT_HAND];
            int how[CT_HAND];
            ct_deal(&A, &T, h, hand(h, how, NULL, 0));
        }
    }
    draw_seats();
    draw_piles(live && mine);
    ct_draw_pile(&T, p->tricks);
    if (ph == EC_ANSWER && mine) draw_answer(live);
    if (ph == EC_DISCARD && mine) draw_throw(live);
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
        ct_draw_last(&T, p->last, NULL, ECARTE_P, p->last_leader, p->last_winner);
    if ((ph == EC_ROUND || ph == EC_OVER) && !ct_busy(&A) && p->tricks) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (ecarte_can_play(&G.pos, card)) act(ECA_PLAY | card);
    else { ct_lower(&F); ui_sound(SND_DENY); }
}

static void press(float x, float y)
{
    const ecarte_pos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last) { show_last = 0; return; }
    if (ct_busy(&A)) return;
    if (p->phase == EC_ROUND) { act(ECA_NEXT); return; }
    if (p->phase == EC_OVER) return;
    if (ct_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    float lift[CT_HAND];
    int n = hand(h, how, lift, 1);
    switch (p->phase) {
    case EC_ANSWER: {
        float yx, nx, by, d;
        buttons(&yx, &nx, &by, &d);
        if (ct_on(yx, by, d, x, y)) act(ECA_YES);
        else if (ct_on(nx, by, d, x, y)) act(ECA_NO);
        else ui_sound(SND_DENY);
        return;
    }
    case EC_DISCARD: {
        if (ct_on(T.cx, T.cy, 1.15f * T.u, x, y)) {
            int k = ct_count(pick);
            if (k >= ecarte_throw_min(p) && k <= ecarte_throw_max(p)) act(ECA_DISCARD | (int)pick);
            else ui_sound(SND_DENY);
            return;
        }
        if (!ct_toggle(&T, n, how, &pick, ct_count(pick) < ecarte_throw_max(p), x, y))
            ui_sound(SND_DENY);
        return;
    }
    case EC_PROPOSE: {
        float sx, sy;
        stock_at(&sx, &sy);
        if (p->nstock && cards_in(sx, sy, pile_w() * 1.2f, T.asp, x, y)) { act(ECA_ASK); return; }
        break;
    }
    }
    if (F.raised != CARD_NONE && ct_on_target(&T, x, y)) { send(F.raised); return; }
    ct_touch_hand(&F, &T, h, n, how, lift, x, y);   /* a tap or a drag: the release says */
}

static void motion(float x, float y) { ct_touch_move(&F, x, y); }

static void release(float x, float y)
{
    int c, r = ct_touch_up(&F, &T, x, y, &c);
    if (r == CT_T_SEND) send(c);
    else if (r == CT_T_TAP && ct_tap_play(&F, c, ecarte_can_play(&G.pos, c))) send(c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, T.deck, G.pos.n[player], G.pos.total[player]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the turned king, the jack and the queen of trumps, the king on top */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[4] = {CARD(SUIT_CLUBS, 7), CARD(SUIT_DIAMONDS, RANK_JACK), CARD(SUIT_DIAMONDS, RANK_QUEEN),
                                 CARD(SUIT_DIAMONDS, RANK_KING)};
    ct_thumb(DECK_FRENCH, cards, 4, size, rgba);
}

static const game ecarte_game_def = {
    .id = "ecarte", .order = 320, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .view = VIEW_HIDDEN,
};
REGISTER_GAME(ecarte_game_def)
