/*
 * Schafkopf in RetroStone Board: four players passing the device round, each hand secret (VIEW_HIDDEN; the rules:
 * schafkopf_rules.c, schafkopf.h). French-suited cards: clubs for acorns, spades for leaves, diamonds for bells; the
 * queens are the Obers, the jacks the Unters. The table (games/trick4_ui.h on the kit's cards_table.h): the viewer's
 * hand fanned at the bottom, the seats round the middle, the trick, the trick pile (tap it: the last trick). Bidding:
 * the Rufspiel's three aces to call (the ones the hand may call), the Wenz (the Unter's figure), the four Solos (a
 * suit's sign): tap one, then the tick (or the same again), or the pass button. Each seat shows its word (a pass, a
 * tick: I would play); in play the declarer's seat shows the game (the ace called, the Wenz, the Solo's suit) and the
 * partner's too once the called ace has shown. After each deal a table: the declaring side, its game and its card
 * points (90: schneider, 120: schwarz), the club Ober and the Laufende counted, the tariff and the value; a Ramsch:
 * each player's card points, the payers in red; every player's score and total; the list of deals.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/schafkopf.h"
#include "games/trick4_ui.h"
#include "shell.h"

static sfgame G;
static tk4_ui U;
static int pick = -1;                           /* the game chosen in the bidding */

static void layout(box b) { tk4u_layout(&U, b, SFK_HAND); }

static void reset_ui(void)
{
    pick = -1;
    tk4u_reset(&U);
}

static int can_play(int card) { return sfk_can_play(&G.pos, card); }

/* a game's picture: the ace called, the Unter's figure (Wenz), the Solo's suit */
static int game_pic(int kind, int suit)
{
    return kind == SFG_RUF ? TK4_PIC_CARD | CARD(suit, RANK_ACE) : kind == SFG_WENZ ? TK4_PIC_FIG | BISHOP : suit;
}

/* ---- the bidding's buttons in the middle ------------------------------------------------------------------------- */
typedef struct { float x, y; } pt;
#define CHOICES 8
static struct { float d; pt at[CHOICES], pass, ok; } K;
static const uint8_t CHOICE[CHOICES] = {SFG(SFG_RUF, SUIT_CLUBS), SFG(SFG_RUF, SUIT_SPADES), SFG(SFG_RUF, SUIT_DIAMONDS),
                                        SFG(SFG_WENZ, 0), SFG(SFG_SOLO, SUIT_CLUBS), SFG(SFG_SOLO, SUIT_SPADES),
                                        SFG(SFG_SOLO, SUIT_HEARTS), SFG(SFG_SOLO, SUIT_DIAMONDS)};

static void controls(void)
{
    const ct_table *T = &U.T;
    float top = T->sy[(T->me + 2) % TK4_P] + 0.9f * T->tok, bot = T->sy[T->me] - 0.9f * T->tok, room = bot - top;
    K.d = floorf(fminf(fminf(T->u, T->b.w * 0.8f / 5), room / 3.9f));
    float y = top + (room - 3.6f * K.d) / 2 + 0.5f * K.d;
    for (int i = 0; i < CHOICES; i++) K.at[i] = (pt){T->cx + (i % 4 - 1.5f) * 1.15f * K.d, y + (i / 4) * 1.25f * K.d};
    K.pass = (pt){T->cx - 0.7f * K.d, y + 2.6f * K.d};
    K.ok = (pt){T->cx + 0.7f * K.d, y + 2.6f * K.d};
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    sfk_new(&G, v, (uint32_t)ui_random(1 << 30));
    tk4u_new(&U);
    reset_ui();
}

static int status(void)
{
    const sfpos *p = &G.pos;
    if (p->phase != SFP_OVER) return GS_PLAY;
    int m = sfk_winners(p);
    for (int q = 0; q < TK4_P; q++)
        if (m == 1 << q) return gs_win(q);
    return GS_DRAW;
}

static unsigned winners(void) { return (unsigned)sfk_winners(&G.pos); }

static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    sfpos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = tk4u_hand(&U, &G.pos.t, 0, 0, can_play, h, how);
    if (!sfk_play(&G, a)) { ui_sound(SND_DENY); return; }
    const sfpos *p = &G.pos;
    ct_calm(&U.A);
    U.fl.n = 0;
    if (SFA_KIND(a) == SFA_PLAY) tk4u_played(&U, &p->t, before.t.tricks, SFA_ARG(a), h, n);
    else ui_sound(SFA_KIND(a) == SFA_PASS ? SND_TAP : SND_PLACE);
    reset_ui();
    if (SFA_KIND(a) == SFA_NEXT) U.seen = 0;
    if (p->turn != before.turn || p->phase >= SFP_DEAL) U.floor_n = G.n;
    if (p->phase < SFP_DEAL) ui_handoff(p->turn);
}

static int undo(void)
{
    if (pick >= 0 || U.F.raised != CARD_NONE || U.show_last) { reset_ui(); return 1; }
    if (G.n <= U.floor_n || !sfk_undo(&G)) return 0;
    ct_calm(&U.A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = sfk_script(&G, s);
    ct_calm(&U.A);
    reset_ui();
    U.floor_n = G.n;
    U.seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&U.A); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_bidding(int live)
{
    const sfpos *p = &G.pos;
    for (int i = 0; i < CHOICES; i++) {
        int g = CHOICE[i];
        ct_choice(tk4u_tex(game_pic(SFG_KIND(g), SFG_SUIT(g)), K.d), K.at[i].x, K.at[i].y, K.d, g == pick,
                  sfk_game_ok(p, g));
    }
    ct_pass(K.pass.x, K.pass.y, K.d, 0);
    if (pick >= 0 && live) ui_mark_gold(K.ok.x, K.ok.y, 1.9f * K.d);
    ct_button(K.ok.x, K.ok.y, K.d, pick >= 0, CT_TICK, 0);
}

/* beside each seat: its word in the bidding; then the declarer's game, and the partner's once the ace has shown */
static void draw_words(void)
{
    const sfpos *p = &G.pos;
    float d = 0.95f * U.T.tok;
    if (p->phase >= SFP_DEAL) return;
    for (int q = 0; q < TK4_P; q++) {
        float x, y;
        ct_seat_spot(&U.T, q, 0, &x, &y);
        if (p->phase == SFP_BID) {
            if (p->word[q] == SFW_PASS) ct_pass_badge(x, y, d);
            else if (p->word[q] == SFW_PLAY) tk4u_tick_badge(x, y, d);
        } else if (q == p->declarer || (q == p->partner && p->revealed))
            tk4u_pic(game_pic(p->kind, p->suit), x, y, 1.1f * d, 255);
    }
}

static void times(float x, float y, float h)    /* a times sign: two strokes crossing */
{
    ct_pic(CT_SLASH, x, y, h, 0, 246, 238, 220, 255);
    ct_pic(CT_SLASH, x, y, h, -60, 246, 238, 220, 255);
}

/* the deal's table */
static void draw_summary(void)
{
    const sfpos *p = &G.pos;
    const sfdeal *r = &p->list[p->ndeals - 1];
    int normal = r->kind == SFG_RUF || r->kind == SFG_WENZ || r->kind == SFG_SOLO, head = normal ? 2 : 1;
    float x0, pw, y, rh, u = U.T.u;
    ct_summary(&U.T, head + TK4_P + 1, &x0, &pw, &y, &rh);
    float d = floorf(fminf(0.62f * rh, 0.75f * u)), L = x0 + 0.35f * u + d / 2, R = x0 + pw - 0.35f * u;
    float nh = floorf(0.38f * rh);
    if (normal) {
        unsigned side = 1u << r->declarer | (r->partner != SFK_NONE ? 1u << r->partner : 0);
        /* the side, its game, its card points (won: a gold tick; lost: a red glow), schneider, schwarz */
        float w = tk4u_chips(side, L, y, d, 255), x = L + w - d / 2 + 0.75f * d;
        tk4u_pic(game_pic(r->kind, r->suit), x, y, 0.9f * d, 255);
        float xe = x0 + 0.5f * pw;
        if (!r->won) ui_mark_glow(xe, y, 1.3f * d);
        ui_number(xe, y, nh, r->side, 1);
        if (r->won) ct_pic(CT_TICK, xe + 0.5f * ct_number_w(r->side, nh, 0) + 0.5f * d, y, 0.8f * d, 0, TK4_GOLD[0],
                           TK4_GOLD[1], TK4_GOLD[2], 255);
        if (r->schwarz) ct_badge(R - 0.45f * d, y, 0.95f * d, 120);
        if (r->schneider) ct_badge(R - (r->schwarz ? 1.5f : 0.45f) * d, y, 0.95f * d, 90);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
        y += rh;
        /* the top trump and the Laufende counted, the tariff, the value */
        int top = r->kind == SFG_WENZ ? CARD(SUIT_CLUBS, RANK_JACK) : CARD(SUIT_CLUBS, RANK_QUEEN);
        tk4u_card(top, L, y, 0.75f * rh, r->runners ? 0 : CD_DIM);
        ui_number(L + 0.85f * d, y, nh, r->runners, 1);
        float xt = x0 + 0.42f * pw;
        ui_number(xt, y, nh, r->kind == SFG_RUF ? 10 : 50, 1);
        if (r->kind != SFG_RUF) {               /* a Solo, a Wenz: three times for the declarer */
            float xm = xt + 0.5f * ct_number_w(50, nh, 0) + 0.5f * nh;
            times(xm, y, 0.7f * nh);
            ui_number(xm + 0.6f * nh, y, 0.8f * nh, 3, 1);
        }
        ct_pic(CT_ARROW, x0 + 0.66f * pw, y, 0.8f * nh, 0, 246, 238, 220, 200);
        tk4u_total(R, y, 1.2f * nh, r->value);
    } else if (r->kind == SFG_RAMSCH) {         /* each one's card points, the payers in red */
        for (int q = 0; q < TK4_P; q++) {
            float x = x0 + pw * (q + 0.5f) / TK4_P - 0.4f * d;
            if (r->losers >> q & 1) ui_mark_glow(x, y, 1.6f * d);
            if (r->march == q + 1) ui_mark_gold(x, y, 1.9f * d);
            ui_put_c(ct_token(q, (int)d), x, y, d, d);
            ui_number(x + 0.6f * d + 0.5f * ct_number_w(r->eyes[q], 0.8f * nh, 0), y, 0.8f * nh, r->eyes[q], 1);
        }
    } else                                      /* thrown in: everyone passed */
        for (int q = 0; q < TK4_P; q++) {
            float x = x0 + pw * (q + 0.5f) / TK4_P - 0.45f * d;
            ui_put_c(ct_token(q, (int)d), x, y, d, d);
            ct_pass_badge(x + 0.95f * d, y, 0.8f * d);
        }
    ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    y += rh;
    /* each player: the deal's score, the total (the best in gold light) */
    int best = p->total[0];
    for (int q = 1; q < TK4_P; q++) best = p->total[q] > best ? p->total[q] : best;
    for (int q = 0; q < TK4_P; q++, y += rh) {
        if (p->total[q] == best) ui_mark_gold(L, y, 1.8f * d);
        ui_put_c(ct_token(q, (int)d), L, y, d, d);
        if (r->score[q]) ui_number_signed(x0 + 0.5f * pw, y, nh, r->score[q], 1, 1);
        tk4u_total(R, y, 1.2f * nh, p->total[q]);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
    uint8_t who[SFK_DEALS], made[SFK_DEALS];
    for (int i = 0; i < p->ndeals; i++) {
        const sfdeal *e = &p->list[i];
        if (e->kind == SFG_RAMSCH) {
            who[i] = e->march ? (uint8_t)(1 << (e->march - 1)) : e->losers;
            made[i] = e->march != 0;
        } else {
            who[i] = e->declarer == SFK_NONE ? 0
                                             : (uint8_t)(1 << e->declarer | (e->partner != SFK_NONE ? 1 << e->partner : 0));
            made[i] = e->won;
        }
    }
    tk4u_deal_list(x0, pw, y, rh, SFK_DEALS, SFK_DEALS, p->ndeals, who, made);
}

static void draw(void)
{
    const sfpos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&U.A), ph = p->phase, hide = ui.viewer < 0;
    tk4u_look(&U, &p->t, ph == SFP_BID);
    controls();
    ct_draw_seats(&U.T, ph < SFP_DEAL ? p->turn : -1);
    draw_words();
    if (ph == SFP_BID && p->turn == U.T.me && !hide) draw_bidding(live);
    tk4u_draw_middle(&U, &p->t, live, ph == SFP_PLAY);
    int summary = ph >= SFP_DEAL && !ct_busy(&U.A);
    tk4u_draw_hand(&U, &p->t, live, ph == SFP_PLAY, can_play, hide, summary);
    if (summary) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (can_play(card)) act(SFA_PLAY | card);
    else { ct_lower(&U.F); ui_sound(SND_DENY); }
}

static void press_bidding(float x, float y)
{
    const sfpos *p = &G.pos;
    if (ct_on(K.pass.x, K.pass.y, K.d, x, y)) { act(SFA_PASS); return; }
    for (int i = 0; i < CHOICES; i++)
        if (ct_on(K.at[i].x, K.at[i].y, K.d, x, y)) {
            if (!sfk_game_ok(p, CHOICE[i])) { ui_sound(SND_DENY); return; }
            if (pick == CHOICE[i]) { act(SFA_GAME | pick); return; }   /* tapped again: said */
            pick = CHOICE[i];
            ui_sound(SND_TAP);
            return;
        }
    if (ct_on(K.ok.x, K.ok.y, K.d, x, y) && pick >= 0) { act(SFA_GAME | pick); return; }
    ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    const sfpos *p = &G.pos;
    if (tk4u_press_first(&U)) return;
    if (p->phase == SFP_DEAL) { act(SFA_NEXT); return; }
    if (p->phase == SFP_OVER) return;
    if (tk4u_press_pile(&U, &p->t, x, y)) return;
    if (p->turn != U.T.me) { ui_sound(SND_DENY); return; }
    controls();
    if (p->phase == SFP_BID) press_bidding(x, y);
    else tk4u_press_card(&U, &p->t, can_play, send, x, y);
}

static void motion(float x, float y) { tk4u_motion(&U, x, y); }

static void release(float x, float y) { tk4u_release(&U, can_play, send, x, y); }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, U.T.deck, G.pos.t.n[player], G.pos.total[player]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the four Obers, the club Ober on top; the picker's: the Ramsch's with the hearts' sign */
static void thumb(int v, int size, uint8_t *rgba)
{
    static const int cards[4] = {CARD(SUIT_DIAMONDS, RANK_QUEEN), CARD(SUIT_HEARTS, RANK_QUEEN),
                                 CARD(SUIT_SPADES, RANK_QUEEN), CARD(SUIT_CLUBS, RANK_QUEEN)};
    ct_thumb(DECK_FRENCH, cards, 4, size, rgba);
    if (v != SFK_RAMSCH) return;
    int s = (int)(0.3f * size);
    uint8_t *b = malloc((size_t)s * s * 4);
    if (!b) return;
    for (int k = 0; k < 4; k++) {               /* everyone for himself: the four chips */
        cards_token(k, s, b);
        art_blend(rgba, size, size, b, s, s, (int)(size * 0.5f + (k - 2) * 0.75f * s), size - s - (int)(0.02f * size));
    }
    free(b);
}

static const game schafkopf_game = {
    .id = "schafkopf", .order = 322, .variants = SFK_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(schafkopf_game)
