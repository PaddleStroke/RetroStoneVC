/*
 * Manille in RetroStone Board: four players in two partnerships passing the device round, each hand secret
 * (VIEW_HIDDEN; the rules: manille_rules.c, manille.h). The table (the kit's cards_table.h): the viewer's hand fanned
 * at the bottom, trumps first; the seats round the middle (the partner across); the dealer's turned card face up beside
 * their token until the first trick is over; the trumps (their ace, small) and the doubling (a cross and 2 or 4) in the
 * top left corner; the trick, each card placed towards its player; the trick pile (tap it: the last trick). Doubling:
 * the round button in the middle doubles (a cross and 2; redouble: a cross and 4), the pass button in your bar passes.
 * Playing: tap a card (it rises), tap it again, or drag it to the middle; the cards you may not play are dimmed. After
 * each deal a summary: each team's tokens, its points (cards and tricks), the deal's score, the total.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/manille.h"
#include "games/tricks.h"
#include "shell.h"

static mgame G;
static ct_table T;
static ct_moves A;
static ct_touch F;
static int floor_n, show_last;                  /* floor_n: the history's length at the last hand-off */
static unsigned seen;                           /* bit q: q has seen this deal's cards arrive */

static void layout(box b) { ct_layout(&T, b, MAN_P, ui_bottom(), MAN_HAND); }

static void reset_ui(void)
{
    show_last = 0;
    ct_touch_reset(&F);
}

static int hand(uint8_t *h, int *how, int live)
{
    const mpos *p = &G.pos;
    int n = p->n[T.me], mine = live && p->turn == T.me && p->phase == MP_PLAY;
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][i];
        h[i] = (uint8_t)c;
        how[i] = mine ? (man_can_play(p, c) ? CD_GLOW : CD_DIM) : 0;
        if (c == F.raised) how[i] = CD_RAISED;
    }
    return n;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    man_new(&G, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    int w = man_winner_team(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static unsigned winners(void)                   /* the winning team: both partners */
{
    int w = man_winner_team(&G.pos);
    return w < 0 ? 0 : 1u << w | 1u << (w + 2);
}

static int turn(void) { return G.pos.turn; }

static void animate(const mpos *b, int a, const uint8_t *h, int n)
{
    const mpos *p = &G.pos;
    int k = MA_KIND(a);
    if (k == MA_DOUBLE || k == MA_REDOUBLE) { ui_sound(SND_CHECK); return; }
    if (k == MA_PASS) { ui_sound(SND_TAP); return; }
    if (k != MA_PLAY) return;
    ct_play(&A, &T, &F, h, n, MA_ARG(a));
    if (p->tricks != b->tricks || p->phase != b->phase)
        ct_gather(&A, &T, p->last, NULL, MAN_P, p->last_leader, p->last_winner, CT_SLIDE_MS + CT_LOOK_MS, CT_SLIDE_MS);
}

static void act(int a)
{
    mpos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 0);
    if (!man_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    animate(&before, a, h, n);
    reset_ui();
    const mpos *p = &G.pos;
    if (MA_KIND(a) == MA_NEXT) seen = 0;
    if (p->turn != before.turn || p->phase == MP_DEAL || p->phase == MP_OVER) floor_n = G.n;
    if (p->phase == MP_SPEAK || p->phase == MP_PLAY) ui_handoff(p->turn);
}

static int undo(void)
{
    if (F.raised != CARD_NONE || show_last) { reset_ui(); return 1; }
    if (G.n <= floor_n || !man_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = man_script(&G, s);
    ct_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&A); }

static int can_pass(void) { return G.pos.phase == MP_SPEAK && !ui_handing() && !busy(); }
static void pass(void) { act(MA_PASS); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* a doubling: a gold cross and 2 (4: redoubled), centred on (cx, cy), h high */
static void draw_mult(float cx, float cy, float h, int mult)
{
    ui_put_c(ct_icon(ICON_CLOSE, (int)(0.8f * h), 250, 214, 90), cx - 0.42f * h, cy, 0.8f * h, 0.8f * h);
    ui_number(cx + 0.42f * h, cy, h, mult, 1);
}

/* the double (redouble) button in the middle */
static void double_at(float *x, float *y, float *d)
{
    *d = floorf(1.5f * T.u);
    *x = T.cx;
    *y = T.cy;
}

/* what each player said, beside their token: a pass (the pass icon), a double or a redouble */
static void draw_said(void)
{
    const mpos *p = &G.pos;
    for (int q = 0; q < MAN_P; q++) {
        if (p->said[q] == MS_NONE) continue;
        float x, y, d = 0.95f * T.tok;
        ct_seat_spot(&T, q, 0, &x, &y);
        ct_pic(CT_DISC_DARK, x, y, d, 0, 255, 255, 255, 255);
        if (p->said[q] == MS_PASS) {
            int s = (int)(0.62f * d);
            ui_put_c(ct_icon(ICON_PASS, s, 255, 255, 255), x, y, (float)s, (float)s);
        } else draw_mult(x, y, 0.42f * d, p->said[q] == MS_REDOUBLE ? 4 : 2);
    }
}

/* the dealer's turned card, beside their token, while it is in their hand and the first trick is not over */
static void draw_turned(void)
{
    const mpos *p = &G.pos;
    if (p->tricks > 0 || trick_find(p->hand[p->dealer], p->n[p->dealer], p->turned) < 0) return;
    float x, y;
    ct_seat_spot(&T, p->dealer, p->said[p->dealer] ? 1 : 0, &x, &y);
    float w = 0.62f * T.wt, a = T.ang[p->dealer];
    if (a > 60 && a < 120) x += 0.35f * T.wt, y -= 0.25f * T.wt * T.asp;   /* the viewer: up a little, off the hand */
    cards_draw(DECK_FRENCH, p->turned, x, y, w, 0, CD_FLAT);
}

/* the top left corner: the trumps (their ace, small) and the doubling */
static void draw_trumps(void)
{
    const mpos *p = &G.pos;
    float w = floorf(fmaxf(10, 0.42f * T.wt)), x = T.b.x + 0.2f * T.u + w / 2, y = T.b.y + 0.15f * T.u + w * T.asp / 2;
    cards_draw(DECK_FRENCH, CARD(p->trump, RANK_ACE), x, y, w, 0, CD_FLAT);
    if (p->mult > 1) draw_mult(x + w / 2 + 0.6f * T.u, y, 0.42f * T.u, p->mult);
}

/* the deal's scores: a row a team, its two tokens (the higher total in gold light), its points (cards and tricks),
 * the deal's score (with the doubling), the total */
static void draw_summary(void)
{
    const mpos *p = &G.pos;
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, 2, &x0, &pw, &y, &rh);
    rh = fminf(rh, 1.4f * u);
    float d = fminf(0.62f * rh, 0.75f * u);
    const int ts = (int)d;
    int top = p->total[1] > p->total[0];
    for (int t = 0; t < 2; t++, y += rh) {
        for (int k = 0; k < 2 && p->total[0] != p->total[1] && t == top; k++)
            ui_mark_gold(x0 + 0.7f * u + k * 0.85f * d, y, 1.8f * d);
        for (int k = 0; k < 2; k++) ui_put_c(ct_token(t + 2 * k, ts), x0 + 0.7f * u + k * 0.85f * d, y, d, d);
        ui_number(x0 + pw * 0.38f, y, 0.36f * rh, p->made[t], 1);
        float sx = x0 + pw * 0.56f, sh = 0.4f * rh, sw = ct_number_w(p->score[t], sh,
                                                                     0) + (p->score[t] ? 0.56f * sh : 0);
        ui_number_signed(sx, y, sh, p->score[t], 1, 1);
        if (p->score[t] && p->mult > 1) draw_mult(sx + sw / 2 + 0.75f * sh, y, 0.75f * sh, p->mult);
        ui_number(x0 + pw * 0.86f, y, 0.5f * rh, p->total[t], 1);
        if (t == 0) ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    const mpos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&A), ph = p->phase, hide = ui.viewer < 0;
    if (!(seen >> T.me & 1) && !ui_handing()) {
        seen |= 1u << T.me;
        if (ph == MP_SPEAK) {
            uint8_t h[CT_HAND];
            int how[CT_HAND];
            ct_deal(&A, &T, h, hand(h, how, 0));
        }
    }
    ct_draw_seats(&T, ph == MP_SPEAK || ph == MP_PLAY ? p->turn : -1);
    if (ph == MP_SPEAK) draw_said();
    if (ph == MP_SPEAK || ph == MP_PLAY) {
        draw_turned();
        draw_trumps();
    }
    ct_draw_pile(&T, p->tricks);
    if (ph == MP_SPEAK && p->turn == T.me && !hide) {
        int a = man_legal(p, MA_REDOUBLE) ? MA_REDOUBLE : man_legal(p, MA_DOUBLE) ? MA_DOUBLE : 0;
        if (a) {
            float x, y, d;
            double_at(&x, &y, &d);
            if (live) ui_mark_gold(x, y, 1.8f * d);
            ct_pic(CT_DISC_DARK, x, y, d, 0, 255, 255, 255, 255);
            draw_mult(x, y, 0.36f * d, a == MA_REDOUBLE ? 4 : 2);
        }
    }
    ct_draw_trick(&T, &A, p->trick, NULL, p->ntrick, p->leader);
    if (F.raised != CARD_NONE && live) ct_draw_target(&T);
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, live);
    if (hide) ct_hand_backs(&T, n);
    else ct_hand_draw(&T, &A, h, n, how, NULL, F.on ? F.card : CARD_NONE);
    ct_draw(&A);
    if (!hide) ct_touch_draw(&F, &T);
    if (show_last && p->last_winner != MAN_NONE)
        ct_draw_last(&T, p->last, NULL, MAN_P, p->last_leader, p->last_winner);
    if ((ph == MP_DEAL || ph == MP_OVER) && !ct_busy(&A)) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (man_can_play(&G.pos, card)) act(MA_PLAY | card);
    else { F.raised = CARD_NONE; ui_sound(SND_DENY); }
}

static void press(float x, float y)
{
    const mpos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last) { show_last = 0; return; }
    if (ct_busy(&A)) return;
    if (p->phase == MP_DEAL) { act(MA_NEXT); return; }
    if (p->phase == MP_OVER) return;
    if (ct_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    if (p->phase == MP_SPEAK) {
        float bx, by, d;
        double_at(&bx, &by, &d);
        int a = man_legal(p, MA_REDOUBLE) ? MA_REDOUBLE : man_legal(p, MA_DOUBLE) ? MA_DOUBLE : 0;
        if (a && hypotf(x - bx, y - by) <= 0.6f * d) act(a);
        else ui_sound(SND_DENY);
        return;
    }
    if (F.raised != CARD_NONE && ct_on_target(&T, x, y)) { send(F.raised); return; }
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 1), c = ct_hand_at(&T, h, n, how, NULL, x, y);
    if (c == CARD_NONE) {
        if (F.raised != CARD_NONE) F.raised = CARD_NONE;
        else ui_sound(SND_DENY);
        return;
    }
    ct_touch_down(&F, c, x, y);
}

static void motion(float x, float y) { ct_touch_move(&F, x, y); }

static void release(float x, float y)
{
    int c, r = ct_touch_up(&F, &T, x, y, &c);
    if (r == CT_T_SEND) send(c);
    else if (r == CT_T_TAP && ct_tap_play(&F, c, man_can_play(&G.pos, c))) send(c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, G.pos.n[player], G.pos.total[man_team(player)]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the manille and the manillon (a ten and an ace) over a king and a queen */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[4] = {CARD(SUIT_DIAMONDS, RANK_QUEEN), CARD(SUIT_CLUBS, RANK_KING),
                                 CARD(SUIT_HEARTS, RANK_ACE), CARD(SUIT_HEARTS, 10)};
    ct_thumb(cards, 4, size, rgba);
}

static const game manille_game = {
    .id = "manille", .order = 305, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(manille_game)
