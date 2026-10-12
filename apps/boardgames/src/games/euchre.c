/*
 * Euchre in RetroStone Board: four players in two partnerships passing the device round, each hand secret
 * (VIEW_HIDDEN; the rules: euchre_rules.c, euchre.h). The table (games/tricks_ui.h): the viewer's hand fanned at the
 * bottom, the seats round the middle (the partner across), the trick with each card placed towards its player, the
 * trick pile (tap it: the last trick). Making trumps (games/calls_ui.h): the kitty in the middle, the turned card on
 * it; round one: the tick orders it up, or the pass button; round two: tap a suit (not the turned one), then the
 * tick, or pass (not the dealer: stick the dealer). The alone button (your chip, your partner's struck through) goes
 * alone. The dealer who took the card taps one to lay away (tapped again, or the kitty). The maker's seat shows the
 * trump suit; a player sitting out is dimmed and struck through; each seat shows its tricks. After each hand a
 * summary: each side's chips, its tricks, the hand's points, the total.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/calls_ui.h"
#include "games/euchre.h"
#include "games/tricks.h"
#include "games/tricks_ui.h"
#include "shell.h"

static egame G;
static tk_table T;
static tk_anim A;
static tk_gfx X;
static tk_touch F;
static int floor_n, show_last, pick = -1, alone;   /* pick: the suit chosen in round two */
static unsigned seen;

static void layout(box b) { tk_layout(&T, b, EU_P, ui_bottom(), EU_HAND); }

static void reset_ui(void)
{
    pick = -1;
    alone = 0;
    show_last = 0;
    tk_touch_reset(&F);
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
        if (c == F.raised) how[i] = CD_RAISED;
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
    tk_calm(&A);
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

/* a trick's cards slide to its winner (each from its player's place: one may sit out) */
static void gather(const epos *p)
{
    int w = p->last_winner;
    for (int i = 0; i < p->nlast; i++) {
        int pl = p->last_who[i];
        tk_slide(&A, p->last[i], T.tx[pl], T.ty[pl], T.wt, T.tdeg[pl], 1, T.sx[w], T.sy[w], 0.45f * T.wt, T.tdeg[w], 1,
                 360, TK_SLIDE_MS + TK_LOOK_MS + 30 * i, TK_NONE, 0, i == p->nlast - 1 ? TK_SLIDE_MS : 0,
                 i == 0 ? SND_MOVE : -1, 200);
    }
}

static void animate(const epos *b, int a, const uint8_t *h, int n)
{
    const epos *p = &G.pos;
    int k = EA_ARG(a);
    controls();
    switch (EA_KIND(a)) {
    case EA_ORDER:
        if (p->phase == EP_DISCARD && T.me == p->dealer) {   /* the dealer's own order: the card into his hand */
            cards_spot s[TK_HAND];
            tk_fan(&T, p->n[T.me], s);
            for (int i = 0; i < p->n[T.me]; i++)
                if (p->hand[T.me][i] == b->up)
                    tk_slide(&A, b->up, K.kitty.x, K.kitty.y, K.kw, 0, 1, s[i].x, s[i].y, T.wh, s[i].deg, 1, TK_SLIDE_MS,
                             0, TK_HAND_TAG, b->up, 0, SND_PLACE, 0);
        } else
            ui_sound(SND_PLACE);
        break;
    case EA_NAME: ui_sound(SND_PLACE); break;
    case EA_PASS: ui_sound(SND_TAP); break;
    case EA_DISCARD: {                          /* the card laid away, face down onto the kitty */
        cards_spot s[TK_HAND];
        tk_fan(&T, n, s);
        for (int i = 0; i < n; i++)
            if (h[i] == k)
                tk_slide(&A, k, s[i].x, s[i].y, T.wh, s[i].deg, 1, T.cx, T.cy, 0.6f * T.wt, 0, 0, TK_SLIDE_MS, 0, TK_NONE,
                         0, 0, SND_PLACE, 150);
        break;
    }
    case EA_PLAY:
        tk_play(&A, &T, &F, h, n, k);
        if (p->tricks != b->tricks) gather(p);
        break;
    }
}

static void act(int a)
{
    epos before = G.pos;
    uint8_t h[TK_HAND];
    int how[TK_HAND];
    int n = hand(h, how, 0);
    if (!euchre_play(&G, a)) { ui_sound(SND_DENY); return; }
    tk_calm(&A);
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
    tk_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = euchre_script(&G, s);
    tk_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return tk_busy(&A); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* the alone button: the viewer's chip, the partner's struck through */
static void draw_alone(float x, float y, float d)
{
    if (alone) ui_mark_gold(x, y, 1.8f * d);
    tk_button(&X, x, y, d, alone, -1, 0);
    tk_tokens(&X, (int)T.tok);
    float s = 0.42f * d;
    int partner = (T.me + 2) % EU_P;
    ui_put_c(X.tok[T.me], x - 0.17f * d, y - 0.12f * d, s, s);
    SDL_SetTextureAlphaMod(X.tok[partner], 150);
    ui_put_c(X.tok[partner], x + 0.17f * d, y + 0.12f * d, s, s);
    SDL_SetTextureAlphaMod(X.tok[partner], 255);
    tk_put(&X, TK_SLASH, x + 0.17f * d, y + 0.12f * d, 0.5f * d, 0, 230, 70, 60, 255);
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
            cl_choice(CL_SUIT + s, K.suit[i].x, K.suit[i].y, K.d, s == pick, s != CARD_SUIT(p->up), NULL);
        }
    draw_alone(K.alone.x, K.alone.y, K.d);
    if (!(r2 && p->turn == p->dealer)) cl_pass(&X, K.pass.x, K.pass.y, K.d, 0);
    if (ready && live) ui_mark_gold(K.ok.x, K.ok.y, 1.9f * K.d);
    tk_button(&X, K.ok.x, K.ok.y, K.d, ready, TK_TICK, 0);
}

/* beside each seat: a pass in the making, the trump suit at the maker's, the tricks taken; a player sitting out dimmed
 * and struck through */
static void draw_seats_info(void)
{
    const epos *p = &G.pos;
    float d = 0.95f * T.tok;
    for (int q = 0; q < EU_P; q++) {
        float x, y, x1, y1;
        tk_seat_spot(&T, q, 0, &x, &y);
        tk_seat_spot(&T, q, 1, &x1, &y1);
        if (p->phase <= EP_ROUND2) {
            if (p->word[q] == EW_PASS) cl_pass_badge(&X, x, y, d);
            continue;
        }
        if (p->phase >= EP_HAND) continue;
        if (q == p->out) {
            tk_put(&X, TK_DISC_DARK, T.sx[q], T.sy[q], 1.05f * T.tok, 0, 255, 255, 255, 200);
            tk_put(&X, TK_SLASH, T.sx[q], T.sy[q], 1.1f * T.tok, 0, 230, 70, 60, 255);
            continue;
        }
        if (q == p->maker) {
            cl_put(CL_SUIT + p->trump, x, y, d, NULL, 255);
            x = x1, y = y1;
        }
        if (p->took[q]) ui_number(x, y, 0.5f * T.tok, p->took[q], 1);
    }
}

static void draw_trick(void)
{
    const epos *p = &G.pos;
    for (int i = 0; i < p->ntrick; i++) {
        int pl = p->who[i];
        if (tk_hides(&A, TK_TRICK, pl)) continue;
        cards_draw(DECK_FRENCH, p->trick[i], T.tx[pl], T.ty[pl], T.wt, T.tdeg[pl], 0);
    }
}

static void draw_last(void)
{
    const epos *p = &G.pos;
    int w = p->last_winner;
    ui_fill(T.b.x, T.b.y, T.b.w, T.hand_top - T.b.y, 0, 0, 0, 90);
    ui_mark_gold(T.sx[w], T.sy[w], 2.6f * T.tok);
    for (int i = 0; i < p->nlast; i++) {
        int pl = p->last_who[i];
        cards_draw(DECK_FRENCH, p->last[i], T.tx[pl], T.ty[pl], T.wt, T.tdeg[pl], pl == w ? CD_RAISED : 0);
    }
}

/* the hand's summary: a row a side, its two chips (the higher total in gold light), the trumps and the alone mark by
 * the makers, the tricks, the hand's points, the total */
static void draw_summary(void)
{
    const epos *p = &G.pos;
    float x0, pw, y, rh, u = T.u;
    tk_summary(&X, &T, 2, &x0, &pw, &y, &rh);
    rh = fminf(rh, 1.5f * u);
    float d = fminf(0.6f * rh, 0.75f * u);
    tk_tokens(&X, (int)d);
    int top = p->score[0] >= p->score[1] ? 0 : 1;
    for (int t = 0; t < 2; t++, y += rh) {
        for (int k = 0; k < 2; k++) {
            float tx = x0 + 0.65f * u + k * 0.85f * d;
            if (t == top && p->score[0] != p->score[1]) ui_mark_gold(tx, y, 1.7f * d);
        }
        for (int k = 0; k < 2; k++) {
            int q = t + 2 * k;
            float tx = x0 + 0.65f * u + k * 0.85f * d;
            ui_put_c(X.tok[q], tx, y, d, d);
            if (q == p->out) tk_put(&X, TK_SLASH, tx, y, d, 0, 230, 70, 60, 255);
        }
        float mx = x0 + 0.65f * u + 1.85f * d;
        if ((p->maker & 1) == t) cl_put(CL_SUIT + p->trump, mx, y, 0.8f * d, NULL, 255);
        ui_number(x0 + pw * 0.45f, y, 0.4f * rh, p->won[t], 1);
        if (p->pts[t]) ui_number_signed(x0 + pw * 0.64f, y, 0.4f * rh, p->pts[t], 1, 1);
        ui_number(x0 + pw - 0.6f * u - tk_number_w(p->score[t], 0.5f * rh) / 2, y, 0.5f * rh, p->score[t], 1);
        if (t == 0) tk_row_line(x0, pw, y + rh / 2, u);
    }
}

static void draw(void)
{
    const epos *p = &G.pos;
    int live = !ui_handing() && !tk_busy(&A), ph = p->phase, hide = ui.viewer < 0;   /* hide: the hand-off cover */
    if (!(seen >> T.me & 1) && !ui_handing()) {
        seen |= 1u << T.me;
        if (ph == EP_ROUND1) {
            uint8_t h[TK_HAND];
            int how[TK_HAND];
            tk_deal(&A, &T, h, hand(h, how, 0));
        }
    }
    controls();
    tk_draw_seats(&X, &T, ph < EP_HAND ? p->turn : -1);
    draw_seats_info();
    tk_draw_pile(&T, p->tricks);
    draw_kitty(live, hide);
    if (p->turn == T.me && !hide && (ph == EP_ROUND1 || ph == EP_ROUND2)) draw_making(live);
    draw_trick();
    if (F.raised != CARD_NONE && live && ph == EP_PLAY) tk_draw_target(&T);
    uint8_t h[TK_HAND];
    int how[TK_HAND];
    int n = hand(h, how, live);
    if (hide) tk_hand_backs(&T, n);
    else tk_hand_draw(&T, &A, h, n, how, NULL, F.on ? F.card : CARD_NONE);
    tk_draw_moves(&A);
    if (!hide) tk_touch_draw(&F, &T);
    if (show_last && p->last_winner != EU_NONE) draw_last();
    if (ph >= EP_HAND && !tk_busy(&A)) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    const epos *p = &G.pos;
    if (p->phase == EP_DISCARD) { act(EA_DISCARD | card); return; }
    if (euchre_can_play(p, card)) act(EA_PLAY | card);
    else { F.raised = CARD_NONE; ui_sound(SND_DENY); }
}

static void press_making(float x, float y)
{
    const epos *p = &G.pos;
    int r2 = p->phase == EP_ROUND2, al = alone ? EA_ALONE : 0;
    if (cl_on(K.alone.x, K.alone.y, K.d, x, y)) { alone = !alone; ui_sound(SND_TAP); return; }
    if (cl_on(K.pass.x, K.pass.y, K.d, x, y)) { act(EA_PASS); return; }
    if (r2)
        for (int i = 0; i < 4; i++)
            if (cl_on(K.suit[i].x, K.suit[i].y, K.d, x, y)) {
                if (SUIT_ORDER[i] == CARD_SUIT(p->up)) { ui_sound(SND_DENY); return; }
                pick = SUIT_ORDER[i];
                ui_sound(SND_TAP);
                return;
            }
    if (cl_on(K.ok.x, K.ok.y, K.d, x, y) || (!r2 && cards_in(K.kitty.x, K.kitty.y, K.kw, T.asp, x, y))) {
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
    if (tk_busy(&A)) return;
    if (p->phase == EP_HAND) { act(EA_NEXT); return; }
    if (p->phase == EP_OVER) return;
    if (tk_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    controls();
    if (p->phase == EP_ROUND1 || p->phase == EP_ROUND2) { press_making(x, y); return; }
    if (F.raised != CARD_NONE) {
        int on = p->phase == EP_DISCARD ? cards_in(T.cx, T.cy, 0.9f * T.wt, T.asp, x, y) : tk_on_target(&T, x, y);
        if (on) { send(F.raised); return; }
    }
    uint8_t h[TK_HAND];
    int how[TK_HAND];
    int n = hand(h, how, 1), c = tk_hand_at(&T, h, n, how, NULL, x, y);
    if (c == CARD_NONE) {
        if (F.raised != CARD_NONE) F.raised = CARD_NONE;
        else ui_sound(SND_DENY);
        return;
    }
    tk_touch_down(&F, c, x, y);
}

static void motion(float x, float y) { tk_touch_move(&F, x, y); }

static void release(float x, float y)
{
    const epos *p = &G.pos;
    int c, r = tk_touch_up(&F, &T, x, y, &c);
    if (r == TK_T_SEND) send(c);
    else if (r == TK_T_TAP && tk_tap_play(&F, c, p->phase == EP_DISCARD || euchre_can_play(p, c))) send(c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { tk_bar(b, G.pos.n[player], G.pos.score[player & 1]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the 9 and the ace of hearts, the left bower, the right bower on top */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[4] = {CARD(SUIT_HEARTS, 9), CARD(SUIT_HEARTS, RANK_ACE), CARD(SUIT_DIAMONDS, RANK_JACK),
                                 CARD(SUIT_HEARTS, RANK_JACK)};
    tk_thumb(cards, 4, size, rgba);
}

static const game euchre_game = {
    .id = "euchre", .order = 312, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(euchre_game)
