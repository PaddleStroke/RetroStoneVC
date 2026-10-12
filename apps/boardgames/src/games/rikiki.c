/*
 * Rikiki (Oh Hell) in RetroStone Board: 3 to 6 players passing the device round, each hand secret (VIEW_HIDDEN; the
 * rules: rikiki_rules.c, rikiki.h). The table (the kit's cards_table.h): the viewer's hand fanned at the bottom, the
 * seats round the middle, each with its bid in a dark disc and the tricks taken beside it (the dealer: a small back
 * beside the chip), the trick with each card placed towards its player, the trick pile (tap it: the last trick), the
 * turned card in the lower left corner (its suit is trumps; struck out: no trumps). Bidding: a pad of numbers from 0
 * to the cards in hand (the dealer's forbidden number struck out), then the tick. Playing: tap a card (it rises), tap
 * it again, or drag it to the middle. After each deal a summary: each player's token, bid, tricks, points and total.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/rikiki.h"
#include "games/tricks.h"
#include "shell.h"

static rikiki_game G;
static ct_table T;
static ct_moves A;
static ct_touch F;
static int pick = -1, floor_n, show_last;       /* pick: the bid chosen on the pad (-1: none yet) */
static unsigned seen;                           /* the players who have seen this deal's cards */

static void layout(box b) { ct_layout(&T, b, G.pos.players, ui_bottom(), G.pos.max); }

static void reset_ui(void)
{
    pick = -1;
    show_last = 0;
    ct_touch_reset(&F);
}

static int hand(uint8_t *h, int *how, int live)
{
    const rikiki_pos *p = &G.pos;
    int n = p->n[T.me], mine = live && p->turn == T.me && p->phase == RK_PLAY;
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][i];
        h[i] = (uint8_t)c;
        how[i] = mine ? (rikiki_can_play(p, c) ? CD_GLOW : CD_DIM) : 0;
        if (ct_is_raised(&F, i)) how[i] = CD_RAISED;
    }
    return n;
}

/* the bidding pad: 0..size, rows of seven at most, the tick under them */
static int pad_cols(void) { return G.pos.size + 1 < 7 ? G.pos.size + 1 : 7; }
static int pad_rows(void) { return (G.pos.size + 7) / 7; }

static float pad_d(void)
{
    float room = T.b.w - 2.8f * T.tok;
    return floorf(fminf(1.0f * T.u, room / (7 * 1.12f)));
}

static float pad_top(void)
{
    float d = pad_d(), h = (pad_rows() - 1) * 1.15f * d + 0.6f * d + 1.3f * T.u;
    return T.cy - h / 2 + 0.1f * d;
}

static void pad_at(int v, float *x, float *y)
{
    float d = pad_d();
    int cols = pad_cols(), row = v / 7, in_row = row < pad_rows() - 1 ? cols : G.pos.size + 1 - row * 7;
    *x = T.cx + (v % 7 - (in_row - 1) / 2.0f) * 1.12f * d;
    *y = pad_top() + row * 1.15f * d;
}

static void tick_at(float *x, float *y, float *d)
{
    *d = 1.15f * T.u;
    *x = T.cx;
    *y = pad_top() + (pad_rows() - 1) * 1.15f * pad_d() + 0.6f * pad_d() + 0.65f * *d;
}

/* the turned card: the lower left corner of the trick's area */
static void turned_at(float *x, float *y, float *w)
{
    *w = floorf(0.62f * T.wt);
    *x = T.b.x + 0.5f * *w + 0.35f * T.u;
    *y = T.hand_top - 0.5f * *w * T.asp - 0.1f * T.u;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    rikiki_new(&G, ui.players, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    if (G.pos.phase != RK_OVER) return GS_PLAY;
    unsigned w = rikiki_winners(&G.pos);
    if (w & (w - 1)) return GS_DRAW;
    for (int q = 0; q < G.pos.players; q++)
        if (w >> q & 1) return gs_win(q);
    return GS_DRAW;
}

static unsigned winners(void) { return rikiki_winners(&G.pos); }

static int turn(void) { return G.pos.turn; }

static void animate(const rikiki_pos *b, int a, const uint8_t *h, int n)
{
    const rikiki_pos *p = &G.pos;
    if (RKA_KIND(a) == RKA_BID) { ui_sound(SND_PLACE); return; }
    if (RKA_KIND(a) != RKA_PLAY) return;
    ct_play(&A, &T, &F, h, n, RKA_ARG(a));
    if (p->tricks != b->tricks)
        ct_gather(&A, &T, p->last, NULL, p->players, p->last_leader, p->last_winner, CT_SLIDE_MS + CT_LOOK_MS,
                  CT_SLIDE_MS);
}

static void act(int a)
{
    rikiki_pos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 0);
    if (!rikiki_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    animate(&before, a, h, n);
    reset_ui();
    const rikiki_pos *p = &G.pos;
    if (RKA_KIND(a) == RKA_NEXT) seen = 0;
    if (p->turn != before.turn || p->phase == RK_ROUND || p->phase == RK_OVER) floor_n = G.n;
    if (p->phase == RK_BID || p->phase == RK_PLAY) ui_handoff(p->turn);
}

static int undo(void)
{
    if (pick >= 0 || F.raised != CARD_NONE || show_last) { reset_ui(); return 1; }
    if (G.n <= floor_n || !rikiki_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = rikiki_script(&G, s);
    ct_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&A); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_pad(int live)
{
    const rikiki_pos *p = &G.pos;
    float d = pad_d();
    int no = rikiki_forbidden(p);
    for (int v = 0; v <= p->size; v++) {
        float x, y;
        pad_at(v, &x, &y);
        ct_button(x, y, d, v == pick, -1, 0);
        ct_number(x, y, 0.46f * d, v, v == no);
    }
    float tx, ty, td;
    tick_at(&tx, &ty, &td);
    if (pick >= 0 && live) ui_mark_gold(tx, ty, 2.0f * td);
    ct_button(tx, ty, td, pick >= 0, CT_TICK, 0);
}

static void draw_turned(void)
{
    const rikiki_pos *p = &G.pos;
    float x, y, w;
    turned_at(&x, &y, &w);
    if (p->turned != CARD_NONE) cards_draw(DECK_FRENCH, p->turned, x, y, w, 0, 0);
    else {                                      /* no trumps: the empty place struck out */
        cards_place(DECK_FRENCH, x, y, w);
        ct_pic(CT_SLASH, x, y, 0.9f * w, 0, 250, 214, 90, 255);
    }
}

/* the deal's summary: a row a player, their token (the highest total in gold light), the bid, the tricks taken (a
 * gold light: made exactly), the deal's points, the total */
static void draw_summary(void)
{
    const rikiki_pos *p = &G.pos;
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, p->players, &x0, &pw, &y, &rh);
    int top = -30000;
    for (int q = 0; q < p->players; q++) top = p->total[q] > top ? p->total[q] : top;
    float d = fminf(0.72f * rh, 0.8f * u);
    const int ts = (int)d;
    for (int q = 0; q < p->players; q++, y += rh) {
        float tx = x0 + 0.8f * u, nh = 0.42f * rh;
        if (p->total[q] == top) ui_mark_gold(tx, y, 2.0f * d);
        ui_put_c(ct_token(q, ts), tx, y, d, d);
        ct_badge(x0 + pw * 0.3f, y, 0.85f * d, p->bid[q]);
        if (p->bid[q] == p->won[q]) ui_mark_gold(x0 + pw * 0.43f, y, 1.5f * d);
        ui_number(x0 + pw * 0.43f, y, nh, p->won[q], 1);
        ui_number_signed(x0 + pw * 0.62f, y, nh, p->score[q], 1, 1);
        ui_number_signed(x0 + pw * 0.85f, y, 0.56f * rh, p->total[q], 1, 0);
        if (q < p->players - 1) ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    const rikiki_pos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&A), ph = p->phase, hide = ui.viewer < 0;   /* hide: the hand-off cover */
    if (!(seen >> T.me & 1) && !ui_handing()) {
        seen |= 1u << T.me;
        if (ph == RK_BID && p->tricks == 0) {
            uint8_t h[CT_HAND];
            int how[CT_HAND];
            ct_deal(&A, &T, h, hand(h, how, 0));
        }
    }
    ct_draw_seats(&T, ph == RK_BID || ph == RK_PLAY ? p->turn : -1);
    for (int q = 0; q < p->players; q++) {     /* the bids, the tricks taken; the dealer's back */
        float x, y;
        if (ph == RK_BID || ph == RK_PLAY) {
            ct_seat_spot(&T, q, 2, &x, &y);
            if (q == p->dealer) cards_draw(DECK_FRENCH, CARD_NONE, x, y, 0.62f * T.tok, 0, CD_DOWN);
        }
        if (p->bid[q] == RIKIKI_NOBID || ph == RK_ROUND || ph == RK_OVER) continue;
        ct_seat_spot(&T, q, 0, &x, &y);
        ct_badge(x, y, 0.95f * T.tok, p->bid[q]);
        if (ph != RK_PLAY) continue;
        ct_seat_spot(&T, q, 1, &x, &y);
        ui_number(x, y, 0.5f * T.tok, p->won[q], 1);
    }
    ct_draw_pile(&T, p->tricks);
    if (ph == RK_BID || ph == RK_PLAY) draw_turned();
    if (ph == RK_BID && p->turn == T.me && !hide) draw_pad(live);
    ct_draw_trick(&T, &A, p->trick, NULL, p->ntrick, p->leader);
    if (F.raised != CARD_NONE && live) ct_draw_target(&T);
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, live);
    if (hide) ct_hand_backs(&T, n);
    else ct_hand_draw(&T, &A, h, n, how, NULL, ct_dragged(&F));
    ct_draw(&A);
    if (!hide) ct_touch_draw(&F, &T);
    if (show_last && p->last_winner != TRICK_NONE)
        ct_draw_last(&T, p->last, NULL, p->players, p->last_leader, p->last_winner);
    if ((ph == RK_ROUND || ph == RK_OVER) && !ct_busy(&A)) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (rikiki_can_play(&G.pos, card)) act(RKA_PLAY | card);
    else { ct_lower(&F); ui_sound(SND_DENY); }
}

static void press(float x, float y)
{
    const rikiki_pos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last) { show_last = 0; return; }
    if (ct_busy(&A)) return;
    if (p->phase == RK_ROUND) { act(RKA_NEXT); return; }
    if (p->phase == RK_OVER) return;
    if (ct_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    if (p->phase == RK_BID) {
        float d = pad_d(), tx, ty, td;
        for (int v = 0; v <= p->size; v++) {
            float bx, by;
            pad_at(v, &bx, &by);
            if (hypotf(x - bx, y - by) > 0.56f * d) continue;
            if (rikiki_can_bid(p, v)) pick = v, ui_sound(SND_TAP);
            else ui_sound(SND_DENY);
            return;
        }
        tick_at(&tx, &ty, &td);
        if (hypotf(x - tx, y - ty) <= 0.6f * td && pick >= 0) act(RKA_BID | pick);
        else ui_sound(SND_DENY);
        return;
    }
    if (F.raised != CARD_NONE && ct_on_target(&T, x, y)) { send(F.raised); return; }
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 1);
    ct_touch_hand(&F, &T, h, n, how, NULL, x, y);   /* a tap or a drag: the release says */
}

static void motion(float x, float y) { ct_touch_move(&F, x, y); }

static void release(float x, float y)
{
    int c, r = ct_touch_up(&F, &T, x, y, &c);
    if (r == CT_T_SEND) send(c);
    else if (r == CT_T_TAP && ct_tap_play(&F, c, rikiki_can_play(&G.pos, c))) send(c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, G.pos.n[player], G.pos.total[player]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: one card, then two, then three: the 2 of clubs, the 7 of diamonds, the jack of hearts, the ace */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[4] = {CARD(SUIT_CLUBS, 2), CARD(SUIT_DIAMONDS, 7), CARD(SUIT_HEARTS, RANK_JACK),
                                 CARD(SUIT_DIAMONDS, RANK_ACE)};
    ct_thumb(cards, 4, size, rgba);
}

static const game rikiki_game_def = {
    .id = "rikiki", .order = 318, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6, .players_usual = 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(rikiki_game_def)
