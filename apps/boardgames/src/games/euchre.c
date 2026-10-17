/*
 * Euchre in RetroStone Board: four players in two partnerships passing the device round, each hand secret (VIEW_HIDDEN;
 * the rules: euchre_rules.c, euchre.h). The table (the kit's cards_table.h): the viewer's hand fanned at the bottom,
 * the seats round the middle (the partner across), the trick with each card placed towards its player, the trick pile
 * (tap it: the last trick). Making trumps (the kit's calls): the kitty in the middle, the turned card on it; round one:
 * the tick orders it up, or the pass button; round two: tap a suit (not the turned one), then the tick, or pass (not
 * the dealer: stick the dealer). The alone button (your chip, your partner's struck through) goes alone. The dealer who
 * took the card taps one to lay away (tapped again, or the kitty). The maker's seat shows the trump suit; a player
 * sitting out is dimmed and struck through; each seat shows its tricks. After each hand a summary: each side's chips,
 * its tricks, the hand's points, the total.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/euchre.h"
#include "games/tricks.h"
#include "pages.h"
#include "shell.h"

static egame G;
static ct_table T;
static ct_moves A;
static ct_touch F;
static int floor_n, show_last, pick = -1, alone;   /* pick: the suit chosen in round two */
static unsigned seen;

static void layout(box b) { ct_layout(&T, b, EU_P, ui_bottom(), EU_HAND); }

static void reset_ui(void)
{
    pick = -1;
    alone = 0;
    show_last = 0;
    ct_touch_reset(&F);
}

static int hand(uint8_t *h, int *how, int live)
{
    const epos *p = &G.pos;
    int n = p->n[T.me], mine = live && p->turn == T.me;
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][i];
        h[i] = (uint8_t)c;
        how[i] = 0;
        if (mine && p->phase == EP_PLAY) how[i] = euchre_can_play(p, c) ? CD_GLOW : CD_DIM;
        if (mine && p->phase == EP_DISCARD) how[i] = CD_GLOW;
        if (ct_is_raised(&F, i)) how[i] = CD_RAISED;
    }
    return n;
}

/* ---- the middle: the kitty and the controls ---------------------------------------------------------------------- */
typedef struct { float x, y; } pt;
static struct {
    float d, kw;                                /* a button's diameter; the kitty's card width */
    pt kitty, suit[4], alone, pass, ok;
} K;

static void controls(void)
{
    const epos *p = &G.pos;
    float u = T.u, top = T.sy[(T.me + 2) % EU_P] + 0.8f * T.tok, bot = T.sy[T.me] - 0.8f * T.tok;
    int r2 = p->phase == EP_ROUND2, mine = p->turn == T.me && (p->phase == EP_ROUND1 || r2);
    float room = bot - top;
    K.kw = floorf(fminf(T.wt * (r2 ? 0.6f : 0.95f), room * (mine ? (r2 ? 0.22f : 0.42f) : 0.6f) / T.asp));
    float kh = K.kw * T.asp, rows = mine ? (r2 ? 2.4f : 1.2f) : 0;
    K.d = floorf(fminf(fminf(u, T.b.w * 0.8f / 5), (room - kh - 0.3f * u) / fmaxf(rows, 1)));
    float h = kh + (mine ? 0.3f * u + rows * K.d : 0), y = top + (room - h) / 2;
    K.kitty = (pt){T.cx, y + kh / 2};
    y += kh + 0.3f * u + 0.5f * K.d;
    for (int s = 0; s < 4; s++) K.suit[s] = (pt){T.cx + (s - 1.5f) * 1.15f * K.d, y};
    if (r2) y += 1.2f * K.d;
    K.alone = (pt){T.cx - 1.25f * K.d, y};
    K.pass = (pt){T.cx, y};
    K.ok = (pt){T.cx + 1.25f * K.d, y};
}

/* the suits in round two, left to right (spades, hearts, clubs, diamonds: the colours alternate) */
static const int SUIT_ORDER[4] = {SUIT_SPADES, SUIT_HEARTS, SUIT_CLUBS, SUIT_DIAMONDS};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    euchre_new(&G, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    int w = euchre_winner(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static unsigned winners(void)                   /* the winning side: both partners */
{
    int w = euchre_winner(&G.pos);
    return w < 0 ? 0 : 1u << w | 1u << (w + 2);
}

static int turn(void) { return G.pos.turn; }

static void animate(const epos *b, int a, const uint8_t *h, int n)
{
    const epos *p = &G.pos;
    int k = EA_ARG(a);
    controls();
    switch (EA_KIND(a)) {
    case EA_ORDER:
        if (p->phase == EP_DISCARD && T.me == p->dealer) {   /* the dealer's own order: the card into his hand */
            cards_spot s[CT_HAND];
            ct_fan(&T, p->n[T.me], s);
            for (int i = 0; i < p->n[T.me]; i++)
                if (p->hand[T.me][i] == b->up)
                    ct_rest(&A, ct_slide(&A, DECK_FRENCH, b->up, K.kitty.x, K.kitty.y, K.kw, 0, 1, s[i].x, s[i].y, T.wh,
                                         s[i].deg, 1, CT_SLIDE_MS, 0, CT_HAND_TAG, i, -1, SND_PLACE), 0);
        } else
            ui_sound(SND_PLACE);
        break;
    case EA_NAME: ui_sound(SND_PLACE); break;
    case EA_PASS: ui_sound(SND_TAP); break;
    case EA_DISCARD: {                          /* the card laid away, face down onto the kitty */
        cards_spot s[CT_HAND];
        ct_fan(&T, n, s);
        for (int i = 0; i < n; i++)
            if (h[i] == k)
                ct_rest(&A, ct_slide(&A, DECK_FRENCH, k, s[i].x, s[i].y, T.wh, s[i].deg, 1, T.cx, T.cy, 0.6f * T.wt, 0,
                                     0,
                                     CT_SLIDE_MS, 0, CT_NONE, 0, -1, SND_PLACE), 150);
        break;
    }
    case EA_PLAY:
        ct_play(&A, &T, &F, h, n, k);
        if (p->tricks != b->tricks)          /* its winner gathers it, each card from its player (one may sit out) */
            ct_gather(&A, &T, p->last, p->last_who, p->nlast, 0, p->last_winner, CT_SLIDE_MS + CT_LOOK_MS, CT_SLIDE_MS);
        break;
    }
}

static void act(int a)
{
    epos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 0);
    if (!euchre_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    animate(&before, a, h, n);
    reset_ui();
    const epos *p = &G.pos;
    if (EA_KIND(a) == EA_NEXT) seen = 0;
    if (p->turn != before.turn || p->phase >= EP_HAND) floor_n = G.n;
    if (p->phase < EP_HAND) ui_handoff(p->turn);
}

static int undo(void)
{
    if (pick >= 0 || alone || F.raised != CARD_NONE || show_last) { reset_ui(); return 1; }
    if (G.n <= floor_n || !euchre_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = euchre_script(&G, s);
    ct_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&A); }

/* ---- the bot's side (euchre_bot.c): the game read-only, and one action of the notation played as a tap ---------- */
const egame *euchre_live(void) { return &G; }

int euchre_show(const char *move)
{
    static egame t;
    t = G;
    if (!euchre_script(&t, move) || t.n != G.n + 1) return 0;
    act(t.act[G.n]);
    return 1;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* the alone button: the viewer's chip, the partner's struck through */
static void draw_alone(float x, float y, float d)
{
    if (alone) ui_mark_gold(x, y, 1.8f * d);
    ct_button(x, y, d, alone, -1, 0);
    const int ts = (int)T.tok;
    float s = 0.42f * d;
    int partner = (T.me + 2) % EU_P;
    ui_put_c(ct_token(T.me, ts), x - 0.17f * d, y - 0.12f * d, s, s);
    SDL_Texture *pt = ct_token(partner, ts);
    SDL_SetTextureAlphaMod(pt, 150);
    ui_put_c(pt, x + 0.17f * d, y + 0.12f * d, s, s);
    ct_pic(CT_SLASH, x + 0.17f * d, y + 0.12f * d, 0.5f * d, 0, 230, 70, 60, 255);
}

static void draw_kitty(int live, int hide)
{
    const epos *p = &G.pos;
    if (p->phase == EP_ROUND1) {
        cards_pile(DECK_FRENCH, K.kitty.x, K.kitty.y, K.kw, 4, p->up, 0);
        return;
    }
    if (p->phase == EP_ROUND2) { cards_pile(DECK_FRENCH, K.kitty.x, K.kitty.y, K.kw, 4, CARD_NONE, 0); return; }
    if (p->phase == EP_DISCARD && p->turn == T.me && !hide)   /* the kitty: where the card laid away goes */
        cards_pile(DECK_FRENCH, T.cx, T.cy, 0.6f * T.wt, 3, CARD_NONE, F.raised != CARD_NONE && live ? CD_GLOW : 0);
}

static void draw_making(int live)
{
    const epos *p = &G.pos;
    int r2 = p->phase == EP_ROUND2, ready = !r2 || pick >= 0;
    if (r2)
        for (int i = 0; i < 4; i++) {
            int s = SUIT_ORDER[i];
            ct_choice(ct_suit(DECK_FRENCH, s, (int)(K.d + 0.5f)), K.suit[i].x, K.suit[i].y, K.d, s == pick,
                      s != CARD_SUIT(p->up));
        }
    draw_alone(K.alone.x, K.alone.y, K.d);
    if (!(r2 && p->turn == p->dealer)) ct_pass(K.pass.x, K.pass.y, K.d, 0);
    if (ready && live) ui_mark_gold(K.ok.x, K.ok.y, 1.9f * K.d);
    ct_button(K.ok.x, K.ok.y, K.d, ready, CT_TICK, 0);
}

/* beside each seat: a pass in the making, the trump suit at the maker's, the tricks taken; a player sitting out dimmed
 * and struck through */
static void draw_seats_info(void)
{
    const epos *p = &G.pos;
    float d = 0.95f * T.tok;
    for (int q = 0; q < EU_P; q++) {
        float x, y, x1, y1;
        ct_seat_spot(&T, q, 0, &x, &y);
        ct_seat_spot(&T, q, 1, &x1, &y1);
        if (p->phase <= EP_ROUND2) {
            if (p->word[q] == EW_PASS) ct_pass_badge(x, y, d);
            continue;
        }
        if (p->phase >= EP_HAND) continue;
        if (q == p->out) {
            ct_pic(CT_DISC_DARK, T.sx[q], T.sy[q], 1.05f * T.tok, 0, 255, 255, 255, 200);
            ct_pic(CT_SLASH, T.sx[q], T.sy[q], 1.1f * T.tok, 0, 230, 70, 60, 255);
            continue;
        }
        if (q == p->maker) {
            ui_put_c(ct_suit(DECK_FRENCH, p->trump, (int)(d + 0.5f)), x, y, d, d);
            x = x1, y = y1;
        }
        if (p->took[q]) ui_number(x, y, 0.5f * T.tok, p->took[q], TH.dark);   /* ink on the light table */
    }
}

/* the hand's summary: a row a side, its two chips (the higher total in gold light), the trumps and the alone mark by
 * the makers, the tricks, the hand's points, the total */
static void draw_summary(void)
{
    const epos *p = &G.pos;
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, 2, &x0, &pw, &y, &rh);
    rh = fminf(rh, 1.5f * u);
    float d = fminf(0.6f * rh, 0.75f * u);
    const int ts = (int)d;
    int top = p->score[0] >= p->score[1] ? 0 : 1;
    for (int t = 0; t < 2; t++, y += rh) {
        for (int k = 0; k < 2; k++) {
            float tx = x0 + 0.65f * u + k * 0.85f * d;
            if (t == top && p->score[0] != p->score[1]) ui_mark_gold(tx, y, 1.7f * d);
        }
        for (int k = 0; k < 2; k++) {
            int q = t + 2 * k;
            float tx = x0 + 0.65f * u + k * 0.85f * d;
            ui_put_c(ct_token(q, ts), tx, y, d, d);
            if (q == p->out) ct_pic(CT_SLASH, tx, y, d, 0, 230, 70, 60, 255);
        }
        float mx = x0 + 0.65f * u + 1.85f * d;
        if ((p->maker & 1) == t) ui_put_c(ct_suit(DECK_FRENCH, p->trump, (int)(0.8f * d + 0.5f)), mx, y, 0.8f * d,
                                          0.8f * d);
        ui_number(x0 + pw * 0.45f, y, 0.4f * rh, p->won[t], 1);
        if (p->pts[t]) ui_number_signed(x0 + pw * 0.64f, y, 0.4f * rh, p->pts[t], 1, 1);
        ui_number(x0 + pw - 0.6f * u - ct_number_w(p->score[t], 0.5f * rh, 0) / 2, y, 0.5f * rh, p->score[t], 1);
        if (t == 0) ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    const epos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&A), ph = p->phase, hide = ui.viewer < 0;   /* hide: the hand-off cover */
    if (!(seen >> T.me & 1) && !ui_handing()) {
        seen |= 1u << T.me;
        if (ph == EP_ROUND1) {
            uint8_t h[CT_HAND];
            int how[CT_HAND];
            ct_deal(&A, &T, h, hand(h, how, 0));
        }
    }
    int dealing = ct_hides(&A, CT_HAND_TAG, -1, -1);   /* the deal still flying in: no call under the deck */
    controls();
    ct_draw_seats(&T, ph < EP_HAND ? p->turn : -1);
    draw_seats_info();
    ct_draw_pile(&T, p->tricks);
    draw_kitty(live, hide);
    if (p->turn == T.me && !hide && !dealing && (ph == EP_ROUND1 || ph == EP_ROUND2)) draw_making(live);
    ct_draw_trick(&T, &A, p->trick, p->who, p->ntrick, p->leader);
    if (F.raised != CARD_NONE && live && ph == EP_PLAY) ct_draw_target(&T);
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, live);
    if (hide) ct_hand_backs(&T, n);
    else ct_hand_draw(&T, &A, h, n, how, NULL, ct_dragged(&F));
    ct_draw(&A);
    if (!hide) ct_touch_draw(&F, &T);
    if (show_last && p->last_winner != EU_NONE)
        ct_draw_last(&T, p->last, p->last_who, p->nlast, 0, p->last_winner);
    if (ph >= EP_HAND && !ct_busy(&A)) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    const epos *p = &G.pos;
    if (p->phase == EP_DISCARD) { act(EA_DISCARD | card); return; }
    if (euchre_can_play(p, card)) act(EA_PLAY | card);
    else { ct_lower(&F); ui_sound(SND_DENY); }
}

static void press_making(float x, float y)
{
    const epos *p = &G.pos;
    int r2 = p->phase == EP_ROUND2, al = alone ? EA_ALONE : 0;
    if (ct_on(K.alone.x, K.alone.y, K.d, x, y)) { alone = !alone; ui_sound(SND_TAP); return; }
    if (ct_on(K.pass.x, K.pass.y, K.d, x, y)) { act(EA_PASS); return; }
    if (r2)
        for (int i = 0; i < 4; i++)
            if (ct_on(K.suit[i].x, K.suit[i].y, K.d, x, y)) {
                if (SUIT_ORDER[i] == CARD_SUIT(p->up)) { ui_sound(SND_DENY); return; }
                pick = SUIT_ORDER[i];
                ui_sound(SND_TAP);
                return;
            }
    if (ct_on(K.ok.x, K.ok.y, K.d, x, y) || (!r2 && cards_in(K.kitty.x, K.kitty.y, K.kw, T.asp, x, y))) {
        if (!r2) act(EA_ORDER | al);
        else if (pick >= 0) act(EA_NAME | pick | al);
        else ui_sound(SND_DENY);
        return;
    }
    ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    const epos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last) { show_last = 0; return; }
    if (ct_busy(&A)) return;
    if (p->phase == EP_HAND) { act(EA_NEXT); return; }
    if (p->phase == EP_OVER) return;
    if (ct_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    controls();
    if (p->phase == EP_ROUND1 || p->phase == EP_ROUND2) { press_making(x, y); return; }
    if (F.raised != CARD_NONE) {
        int on = p->phase == EP_DISCARD ? cards_in(T.cx, T.cy, 0.9f * T.wt, T.asp, x, y) : ct_on_target(&T, x, y);
        if (on) { send(F.raised); return; }
    }
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 1);
    ct_touch_hand(&F, &T, h, n, how, NULL, x, y);   /* a tap or a drag: the release says */
}

static void motion(float x, float y) { ct_touch_move(&F, x, y); }

static void release(float x, float y)
{
    const epos *p = &G.pos;
    int c, r = ct_touch_up(&F, &T, x, y, &c);
    if (r == CT_T_SEND) send(c);
    else if (r == CT_T_TAP && ct_tap_play(&F, c, p->phase == EP_DISCARD || euchre_can_play(p, c))) send(c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, T.deck, G.pos.n[player], G.pos.score[player & 1]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the 9 and the ace of hearts, the left bower, the right bower on top */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[4] = {CARD(SUIT_HEARTS, 9), CARD(SUIT_HEARTS, RANK_ACE), CARD(SUIT_DIAMONDS, RANK_JACK),
                                 CARD(SUIT_HEARTS, RANK_JACK)};
    ct_thumb(DECK_FRENCH, cards, 4, size, rgba);
}

static const game euchre_game = {
    .id = "euchre", .order = 312, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(euchre_game)
