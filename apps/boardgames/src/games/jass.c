/*
 * Jass (the Schieber) in RetroStone Board: four players in two partnerships passing the device round, each hand secret
 * (VIEW_HIDDEN; the rules: jass_rules.c, jass.h). The table (games/trick4_ui.h on the kit's cards_table.h): the
 * viewer's hand fanned at the bottom, the seats round the middle (the partner across), the trick, the trick pile (tap
 * it: the last trick). The choice: tap a game (the four suits; an arrow up: obenabe; an arrow down: undeufe), then the
 * tick, or the pass button to schieben (the partner chooses). The chooser's seat shows the game, the one who pushed
 * shows the pass sign. In the first trick the weis button (its points) declares the weis before the card is played;
 * each seat shows the weis declared, then only the winning team's, in gold light; stöck shows as the trump king and
 * queen at the seat that scored it. After each deal a table: each team's chips, the game (and its multiplier), card
 * points (match: in gold light), weis, stöck, the deal's score and the total; under it, both teams' chips along the
 * way to the target.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/jass.h"
#include "games/trick4_ui.h"
#include "shell.h"

static jsgame G;
static tk4_ui U;
static int pick = -1;                           /* the game chosen */

static void layout(box b) { tk4u_layout(&U, b, JAS_HAND); }

static void reset_ui(void)
{
    pick = -1;
    tk4u_reset(&U);
}

static int can_play(int card) { return jas_can_play(&G.pos, card); }

/* a game's picture: a suit's sign, an arrow up (obenabe) or down (undeufe) */
static int mode_pic(int mode) { return mode < 4 ? mode : mode == JM_OBEN ? TK4_PIC_UP : TK4_PIC_DOWN; }

/* ---- the buttons in the middle ----------------------------------------------------------------------------------- */
typedef struct { float x, y; } pt;
static struct { float d; pt mode[JM_MODES], push, ok, weis; } K;
static const int MODE_ORDER[JM_MODES] = {SUIT_SPADES, SUIT_HEARTS, SUIT_CLUBS, SUIT_DIAMONDS, JM_OBEN, JM_UNDE};

static void controls(void)
{
    const ct_table *T = &U.T;
    float top = T->sy[(T->me + 2) % TK4_P] + 0.9f * T->tok, bot = T->sy[T->me] - 0.9f * T->tok, room = bot - top;
    K.d = floorf(fminf(fminf(T->u, T->b.w * 0.8f / 5), room / 2.6f));
    float y = top + (room - 2.3f * K.d) / 2 + 0.5f * K.d;
    for (int i = 0; i < 4; i++) K.mode[MODE_ORDER[i]] = (pt){T->cx + (i - 1.5f) * 1.15f * K.d, y};
    for (int i = 4; i < 6; i++) K.mode[MODE_ORDER[i]] = (pt){T->cx + (i - 5.5f) * 1.15f * K.d, y + 1.3f * K.d};
    K.push = (pt){T->cx + 0.5f * 1.15f * K.d, y + 1.3f * K.d};
    K.ok = (pt){T->cx + 1.5f * 1.15f * K.d, y + 1.3f * K.d};
    K.weis = (pt){T->b.x + T->b.w - T->px + T->b.x, T->py - 0.1f * T->u};   /* across from the trick pile */
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    jas_new(&G, v, (uint32_t)ui_random(1 << 30));
    tk4u_new(&U);
    reset_ui();
}

static int status(void)
{
    const jspos *p = &G.pos;
    return p->phase != JP_OVER || p->first == JAS_NONE ? GS_PLAY : gs_win(p->first);
}

static unsigned winners(void) { return (unsigned)jas_winners(&G.pos); }

static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    jspos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = tk4u_hand(&U, &G.pos.t, 0, 0, can_play, h, how);
    if (!jas_play(&G, a)) { ui_sound(SND_DENY); return; }
    const jspos *p = &G.pos;
    ct_calm(&U.A);
    U.fl.n = 0;
    if (JA_KIND(a) == JA_PLAY) {
        tk4u_played(&U, &p->t, before.t.tricks, JA_ARG(a), h, n);
        int k = tk4_team(before.turn);
        if (p->stock[k] != before.stock[k]) ui_sound_later(SND_CAPTURE, CT_SLIDE_MS);
        if (p->t.tricks == 1 && before.t.tricks == 0 && p->weis_team != JAS_NONE)
            for (int q = 0; q < TK4_P; q++)
                if (tk4_team(q) == p->weis_team && p->declared[q] && p->wpts[q]) tk4u_float(&U, q, p->wpts[q], 0);
    } else
        ui_sound(JA_KIND(a) == JA_PUSH ? SND_TAP : SND_PLACE);
    reset_ui();
    if (JA_KIND(a) == JA_NEXT) U.seen = 0;
    if (p->turn != before.turn || p->phase >= JP_DEAL) U.floor_n = G.n;
    if (p->phase < JP_DEAL) ui_handoff(p->turn);
}

static int undo(void)
{
    if (pick >= 0 || U.F.raised != CARD_NONE || U.show_last) { reset_ui(); return 1; }
    if (G.n <= U.floor_n || !jas_undo(&G)) return 0;
    ct_calm(&U.A);
    U.fl.n = 0;
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = jas_script(&G, s);
    ct_calm(&U.A);
    reset_ui();
    U.floor_n = G.n;
    U.seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&U.A); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* a times sign and the multiplier, h high, from x */
static void times(float x, float y, float h, int m)
{
    ct_pic(CT_SLASH, x + 0.3f * h, y, 0.8f * h, 0, 246, 238, 220, 255);
    ct_pic(CT_SLASH, x + 0.3f * h, y, 0.8f * h, -60, 246, 238, 220, 255);
    ui_number(x + 0.9f * h, y, h, m, 1);
}

/* stöck: the trump king and queen, side by side, h high */
static void stock_pic(int mode, float x, float y, float h)
{
    float w = h / cards_aspect(DECK_FRENCH);
    cards_draw(DECK_FRENCH, CARD(mode, RANK_KING), x - 0.28f * w, y, w, -8, CD_FLAT);
    cards_draw(DECK_FRENCH, CARD(mode, RANK_QUEEN), x + 0.28f * w, y, w, 8, CD_FLAT);
}

static void draw_choice(int live)
{
    const jspos *p = &G.pos;
    for (int m = 0; m < JM_MODES; m++) ct_choice(tk4u_tex(mode_pic(m), K.d), K.mode[m].x, K.mode[m].y, K.d, m == pick, 1);
    if (!p->pushed) ct_pass(K.push.x, K.push.y, K.d, 0);
    if (pick >= 0 && live) ui_mark_gold(K.ok.x, K.ok.y, 1.9f * K.d);
    ct_button(K.ok.x, K.ok.y, K.d, pick >= 0, CT_TICK, 0);
}

/* beside each seat: schoben, the game chosen, the weis (in the first trick all those declared, then the counted
 * ones), stöck */
static void draw_words(void)
{
    const jspos *p = &G.pos;
    float d = 0.95f * U.T.tok;
    if (p->phase >= JP_DEAL) return;
    for (int q = 0; q < TK4_P; q++) {
        float x[3], y[3];
        for (int k = 0; k < 3; k++) ct_seat_spot(&U.T, q, k, &x[k], &y[k]);
        int k = 0;
        if (p->pushed && q == tk4_partner(p->phase == JP_CHOOSE ? p->turn : p->chooser)) ct_pass_badge(x[k++], y[0], d);
        if (p->phase == JP_PLAY && q == p->chooser) tk4u_pic(mode_pic(p->mode), x[k++], y[0], d, 255);
        if (p->declared[q] && p->wpts[q] && (p->t.tricks == 0 || p->weis_team == tk4_team(q))) {
            if (p->t.tricks) ui_mark_gold(x[k], y[0], 1.6f * d);
            ct_badge(x[k++], y[0], d, p->wpts[q]);
        }
        if (p->stock_done && q == p->stock_pl) stock_pic(p->mode, x[k], y[0], 1.1f * d);
    }
}

/* the weis button, across from the trick pile: its points (the viewer's first card still to come) */
static void draw_weis_button(int live)
{
    const jspos *p = &G.pos;
    if (!jas_can_weis(p) || p->turn != U.T.me) return;
    float d = 0.9f * K.d;
    int pts = jas_melds(p->t.hand[U.T.me], p->t.n[U.T.me], p->mode, NULL, NULL, NULL);
    if (live) ui_mark_gold(K.weis.x, K.weis.y, 1.8f * d);
    ct_button(K.weis.x, K.weis.y, d, 1, -1, 0);
    ui_number(K.weis.x, K.weis.y, (pts >= 100 ? 0.34f : 0.42f) * d, pts, 1);
}

/* the deal's table: a row a team; the way to the target */
static void draw_summary(void)
{
    const jspos *p = &G.pos;
    const jsdeal *r = &p->list[p->ndeals - 1];
    float x0, pw, y, rh, u = U.T.u;
    ct_summary(&U.T, 3, &x0, &pw, &y, &rh);
    float d = floorf(fminf(0.62f * rh, 0.75f * u)), L = x0 + 0.35f * u + d / 2, R = x0 + pw - 0.35f * u;
    float nh = floorf(0.34f * rh);
    int C = tk4_team(r->chooser);
    for (int k = 0; k < 2; k++, y += rh) {
        if (p->first == k && p->phase == JP_OVER) ui_mark_gold(L + 0.4f * d, y, 2.4f * d);
        tk4u_chips(1u << k | 1u << (k + 2), L, y, d, 255);
        if (k == C) {                           /* the game, and its multiplier */
            float tx = L + 1.95f * d;
            tk4u_pic(mode_pic(r->mode), tx, y, 0.8f * d, 255);
            if (r->mult > 1) times(tx + 0.5f * d, y, 0.7f * nh, r->mult);
        }
        float xe = x0 + 0.385f * pw;
        int eyes = r->eyes[k] + (r->match == k + 1 ? 100 : 0);
        if (r->match == k + 1) ui_mark_gold(xe, y, 1.4f * d);
        ui_number(xe, y, nh, eyes, 1);
        if (r->weis[k]) ui_number_signed(x0 + 0.5f * pw, y, 0.8f * nh, r->weis[k], 1, 1);
        if (r->stock[k]) stock_pic(r->mode, x0 + 0.585f * pw, y, 1.6f * nh);
        ct_pic(CT_ARROW, x0 + 0.637f * pw, y, 0.7f * nh, 0, 246, 238, 220, 200);
        ui_number(x0 + 0.72f * pw, y, 0.9f * nh, r->score[k], 1);
        tk4u_total(R, y, 1.1f * nh, p->total[k]);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
    /* the way to the target: a track, each team's chips where its total is, the target at its end */
    int target = jas_target(p->variant);
    float tw = ct_number_w(target, 0.8f * nh, 0), a = L - d / 2, b = R - tw - 0.4f * u, th = 0.12f * rh;
    const uint8_t track[4] = {246, 238, 220, 60};
    ui_put(ct_round((int)(b - a), (int)th, th / 2, track), a, y - th / 2, b - a, th);
    ui_number(R - tw / 2, y, 0.8f * nh, target, 1);
    for (int k = 0; k < 2; k++) {
        float f = clampf((float)p->total[k] / target, 0, 1), cx = a + f * (b - a), cd = 0.6f * d;
        tk4u_chips(1u << k | 1u << (k + 2), cx - 0.39f * cd, y + (k ? 0.22f : -0.22f) * rh, cd, 255);
    }
}

static void draw(void)
{
    const jspos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&U.A), ph = p->phase, hide = ui.viewer < 0;
    tk4u_look(&U, &p->t, ph == JP_CHOOSE && !p->pushed);
    controls();
    ct_draw_seats(&U.T, ph < JP_DEAL ? p->turn : -1);
    draw_words();
    if (ph == JP_CHOOSE && p->turn == U.T.me && !hide) draw_choice(live);
    if (ph == JP_PLAY && !hide) draw_weis_button(live);
    tk4u_draw_middle(&U, &p->t, live, ph == JP_PLAY);
    int summary = ph >= JP_DEAL && !ct_busy(&U.A);
    tk4u_draw_hand(&U, &p->t, live, ph == JP_PLAY, can_play, hide, summary);
    if (summary) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (can_play(card)) act(JA_PLAY | card);
    else { U.F.raised = CARD_NONE; ui_sound(SND_DENY); }
}

static void press_choice(float x, float y)
{
    if (!G.pos.pushed && ct_on(K.push.x, K.push.y, K.d, x, y)) { act(JA_PUSH); return; }
    for (int m = 0; m < JM_MODES; m++)
        if (ct_on(K.mode[m].x, K.mode[m].y, K.d, x, y)) {
            if (pick == m) { act(JA_MODE | m); return; }   /* tapped again: chosen */
            pick = m;
            ui_sound(SND_TAP);
            return;
        }
    if (ct_on(K.ok.x, K.ok.y, K.d, x, y) && pick >= 0) { act(JA_MODE | pick); return; }
    ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    const jspos *p = &G.pos;
    if (tk4u_press_first(&U)) return;
    if (p->phase == JP_DEAL) { act(JA_NEXT); return; }
    if (p->phase == JP_OVER) return;
    if (tk4u_press_pile(&U, &p->t, x, y)) return;
    if (p->turn != U.T.me) { ui_sound(SND_DENY); return; }
    controls();
    if (p->phase == JP_CHOOSE) { press_choice(x, y); return; }
    if (jas_can_weis(p) && ct_on(K.weis.x, K.weis.y, 0.9f * K.d, x, y)) { act(JA_WEIS); return; }
    tk4u_press_card(&U, &p->t, can_play, send, x, y);
}

static void motion(float x, float y) { tk4u_motion(&U, x, y); }

static void release(float x, float y) { tk4u_release(&U, can_play, send, x, y); }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, G.pos.t.n[player], G.pos.total[tk4_team(player)]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the trumps' best: the ace, the Nell (9), the Puur (jack) on top; the picker's: with the game's
 * multipliers (an arrow up and down: x3) for the long game */
static void thumb(int v, int size, uint8_t *rgba)
{
    static const int cards[4] = {CARD(SUIT_DIAMONDS, RANK_KING), CARD(SUIT_DIAMONDS, RANK_ACE),
                                 CARD(SUIT_DIAMONDS, 9), CARD(SUIT_DIAMONDS, RANK_JACK)};
    ct_thumb(cards, 4, size, rgba);
    if (v != JASS_2500) return;
    int s = (int)(0.3f * size);
    uint8_t *b = malloc((size_t)s * s * 4);
    if (!b) return;
    for (int k = 0; k < 2; k++) {
        tk4_paint(k ? TK4_PIC_DOWN : TK4_PIC_UP, s, b);
        art_blend(rgba, size, size, b, s, s, (int)(size * 0.5f + (k - 1) * 1.05f * s), size - s - (int)(0.02f * size));
    }
    free(b);
}

static const game jass_game = {
    .id = "jass", .order = 323, .variants = JASS_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(jass_game)
