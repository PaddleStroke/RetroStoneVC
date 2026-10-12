/*
 * Spades in RetroStone Board: four players in two partnerships passing the device round, each hand secret (VIEW_HIDDEN;
 * the rules: spades_rules.c, spades.h). The table (the kit's cards_table.h): the viewer's hand fanned at the bottom,
 * the seats round the middle (the partner across), each with its bid in a dark disc and the tricks taken beside it, the
 * trick with each card placed towards its player, the trick pile (tap it: the last trick). Bidding: a pad of numbers 0
 * to 13 in the middle (0 struck through: nil), then the tick. Playing: tap a card (it rises), tap it again, or drag it
 * to the middle. After each round a summary: each side's tokens, the round's points, the total, the bags.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/spades.h"
#include "games/tricks.h"
#include "shell.h"


static sgame G;
static ct_table T;
static ct_moves A;
static ct_touch F;
static int pick = -1, floor_n, show_last;       /* pick: the bid chosen on the pad (-1: none yet) */
static unsigned seen;

static void layout(box b) { ct_layout(&T, b, SPADES_P, ui_bottom(), SPADES_HAND); }

static void reset_ui(void)
{
    pick = -1;
    show_last = 0;
    ct_touch_reset(&F);
}

static int hand(uint8_t *h, int *how, int live)
{
    const spos *p = &G.pos;
    int n = p->n[T.me], mine = live && p->turn == T.me && p->phase == SP_PLAY;
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][i];
        h[i] = (uint8_t)c;
        how[i] = mine ? (spades_can_play(p, c) ? CD_GLOW : CD_DIM) : 0;
        if (ct_is_raised(&F, i)) how[i] = CD_RAISED;
    }
    return n;
}

/* the bidding pad: 0..13 in two rows of seven, the tick under them; between the side seats when they leave room
 * (landscape), else above them */
static int pad_between(void) { return (T.b.w - 7.4f * T.tok) / 7.9f >= 0.8f * T.u; }

static float pad_d(void)                       /* a button's diameter */
{
    return floorf(fminf(1.0f * T.u, (T.b.w - (pad_between() ? 7.4f : 2.6f) * T.tok) / 7.9f));
}

static float pad_top(void) { return T.cy - (pad_between() ? 0.95f : 2.45f) * pad_d(); }   /* the first row's centre */

static void pad_at(int v, float *x, float *y)
{
    float d = pad_d();
    *x = T.cx + (v % 7 - 3) * 1.12f * d;
    *y = pad_top() + (v / 7) * 1.15f * d;
}

static void tick_at(float *x, float *y, float *d)
{
    *d = 1.15f * T.u;
    *x = T.cx;
    *y = pad_top() + 1.15f * pad_d() + 0.6f * pad_d() + 0.6f * *d;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    spades_new(&G, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    int w = spades_winner(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static unsigned winners(void)                   /* the winning side: both partners */
{
    int w = spades_winner(&G.pos);
    return w < 0 ? 0 : 1u << w | 1u << (w + 2);
}

static int turn(void) { return G.pos.turn; }

static void animate(const spos *b, int a, const uint8_t *h, int n)
{
    const spos *p = &G.pos;
    if (SA_KIND(a) == SA_BID) { ui_sound(SND_PLACE); return; }
    if (SA_KIND(a) != SA_PLAY) return;
    ct_play(&A, &T, &F, h, n, SA_ARG(a));
    if (p->tricks != b->tricks && p->last_winner != TRICK_NONE)
        ct_gather(&A, &T, p->last, NULL, SPADES_P, p->last_leader, p->last_winner, CT_SLIDE_MS + CT_LOOK_MS,
                  CT_SLIDE_MS);
}

static void act(int a)
{
    spos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 0);
    if (!spades_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    animate(&before, a, h, n);
    reset_ui();
    const spos *p = &G.pos;
    if (SA_KIND(a) == SA_NEXT) seen = 0;
    if (p->turn != before.turn || p->phase == SP_ROUND || p->phase == SP_OVER) floor_n = G.n;
    if (p->phase == SP_BID || p->phase == SP_PLAY) ui_handoff(p->turn);
}

static int undo(void)
{
    if (pick >= 0 || F.raised != CARD_NONE || show_last) { reset_ui(); return 1; }
    if (G.n <= floor_n || !spades_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = spades_script(&G, s);
    ct_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&A); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* a bid in a dark disc (nil: the 0 struck through) */
static void badge(float x, float y, float d, int v, int nil)
{
    ct_pic(CT_DISC_DARK, x, y, d, 0, 255, 255, 255, 255);
    ct_number(x, y, 0.5f * d, v, nil);
}

static void draw_pad(int live)
{
    float d = pad_d();
    for (int v = 0; v <= SPADES_HAND; v++) {
        float x, y;
        pad_at(v, &x, &y);
        ct_button(x, y, d, v == pick, -1, 0);
        ct_number(x, y, 0.46f * d, v, v == SPADES_NIL);
    }
    float tx, ty, td;
    tick_at(&tx, &ty, &td);
    if (pick >= 0 && live) ui_mark_gold(tx, ty, 2.0f * td);
    ct_button(tx, ty, td, pick >= 0, CT_TICK, 0);
}

/* the round's summary: a row a side, its two tokens (the higher total in gold light), the round's points, the
 * total, the bags left (in a dark disc; a red glow: ten of them cost 100 this round) */
static void draw_summary(void)
{
    const spos *p = &G.pos;
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, 2, &x0, &pw, &y, &rh);
    rh = fminf(rh, 1.4f * u);
    float d = fminf(0.62f * rh, 0.75f * u);
    const int ts = (int)d;
    int top = p->score[0] >= p->score[1] ? 0 : 1;
    for (int t = 0; t < 2; t++, y += rh) {
        for (int k = 0; k < 2; k++) {
            float tx = x0 + 0.7f * u + k * 0.85f * d;
            if (t == top && p->score[0] != p->score[1]) ui_mark_gold(tx, y, 1.8f * d);
        }
        for (int k = 0; k < 2; k++) ui_put_c(ct_token(t + 2 * k, ts), x0 + 0.7f * u + k * 0.85f * d, y, d, d);
        ui_number_signed(x0 + pw * 0.43f, y, 0.4f * rh, p->pts[t], 1, 1);
        ui_number_signed(x0 + pw * 0.69f, y, 0.5f * rh, p->score[t], 1, 0);
        float bx = x0 + pw - 0.65f * u, bd = 0.62f * u;
        if (p->penalty[t]) ui_mark_glow(bx, y, 1.6f * bd);
        badge(bx, y, bd, p->bags[t], 0);
        if (t == 0) ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    const spos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&A), ph = p->phase, hide = ui.viewer < 0;   /* hide: the hand-off cover */
    if (!(seen >> T.me & 1) && !ui_handing()) {
        seen |= 1u << T.me;
        if (ph == SP_BID) {
            uint8_t h[CT_HAND];
            int how[CT_HAND];
            ct_deal(&A, &T, h, hand(h, how, 0));
        }
    }
    ct_draw_seats(&T, ph == SP_BID || ph == SP_PLAY ? p->turn : -1);
    for (int q = 0; q < SPADES_P; q++) {       /* the bids, the tricks taken */
        if (p->bid[q] == SPADES_NOBID || ph == SP_ROUND || ph == SP_OVER) continue;
        float x, y;
        ct_seat_spot(&T, q, 0, &x, &y);
        badge(x, y, 0.95f * T.tok, p->bid[q], p->bid[q] == SPADES_NIL);
        if (ph != SP_PLAY) continue;
        ct_seat_spot(&T, q, 1, &x, &y);
        ui_number(x, y, 0.5f * T.tok, p->won[q], 1);
    }
    ct_draw_pile(&T, p->tricks);
    if (ph == SP_BID && p->turn == T.me && !hide) draw_pad(live);
    ct_draw_trick(&T, &A, p->trick, NULL, p->ntrick, p->leader);
    if (F.raised != CARD_NONE && live) ct_draw_target(&T);
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, live);
    if (hide) ct_hand_backs(&T, n);   /* face down */
    else ct_hand_draw(&T, &A, h, n, how, NULL, ct_dragged(&F));
    ct_draw(&A);
    if (!hide) ct_touch_draw(&F, &T);
    if (show_last && p->last_winner != TRICK_NONE)
        ct_draw_last(&T, p->last, NULL, SPADES_P, p->last_leader, p->last_winner);
    if ((ph == SP_ROUND || ph == SP_OVER) && !ct_busy(&A)) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (spades_can_play(&G.pos, card)) act(SA_PLAY | card);
    else { ct_lower(&F); ui_sound(SND_DENY); }
}

static void press(float x, float y)
{
    const spos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last) { show_last = 0; return; }
    if (ct_busy(&A)) return;                    /* the cards still moving */
    if (p->phase == SP_ROUND) { act(SA_NEXT); return; }
    if (p->phase == SP_OVER) return;
    if (ct_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    if (p->phase == SP_BID) {
        float d = pad_d(), tx, ty, td;
        for (int v = 0; v <= SPADES_HAND; v++) {
            float bx, by;
            pad_at(v, &bx, &by);
            if (hypotf(x - bx, y - by) > 0.56f * d) continue;
            pick = v;
            ui_sound(SND_TAP);
            return;
        }
        tick_at(&tx, &ty, &td);
        if (hypotf(x - tx, y - ty) <= 0.6f * td && pick >= 0) act(SA_BID | pick);
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
    else if (r == CT_T_TAP && ct_tap_play(&F, c, spades_can_play(&G.pos, c))) send(c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, G.pos.n[player], G.pos.score[player & 1]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: a fan of spades, the ace on top */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[4] = {CARD(SUIT_HEARTS, RANK_KING), CARD(SUIT_SPADES, 9), CARD(SUIT_SPADES, RANK_KING),
                                 CARD(SUIT_SPADES, RANK_ACE)};
    ct_thumb(cards, 4, size, rgba);
}

static const game spades_game = {
    .id = "spades", .order = 287, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(spades_game)
