/*
 * Klaverjas in RetroStone Board: four players in two partnerships passing the device round, each hand secret
 * (VIEW_HIDDEN; the rules: klaverjas_rules.c, klaverjas.h, Rotterdams). The table (the kit's ct_game on
 * cards_table.h): the viewer's hand fanned at the bottom, the seats round the middle (the partner across), the trick,
 * the trick pile (tap it: the last trick). Bidding: tap a suit, then the tick, or the pass button (gone when all four
 * have passed: voorhand must name one). Each seat shows its pass; then the player who named trumps shows their suit.
 * Roem rises in points from the seat of the player who takes it. After each deal a table: each team's chips (the
 * playing team's trumps, struck through when nat), card points, roem, the deal's score and the total; the list of the
 * 16 deals (the playing team's chips: gold, made; dimmed, nat).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/klaverjas.h"
#include "games/trick4.h"
#include "shell.h"

static kvgame G;
static ct_game U;
static int pick = -1;                           /* the suit chosen in the bidding */

static void layout(box b) { ct_layout(&U.T, b, TK4_P, ui_bottom(), KVJ_HAND); }

static void reset_ui(void)
{
    pick = -1;
    ct_game_reset(&U);
}

static int can_play(int card) { return kvj_can_play(&G.pos, card); }

/* ---- the bidding's buttons in the middle ------------------------------------------------------------------------- */
typedef struct { float x, y; } pt;
static struct { float d; pt suit[4], pass, ok; } K;
static const int SUIT_ORDER[4] = {SUIT_SPADES, SUIT_HEARTS, SUIT_CLUBS, SUIT_DIAMONDS};

static void controls(void)
{
    const ct_table *T = &U.T;
    float top = T->sy[(T->me + 2) % TK4_P] + 0.9f * T->tok, bot = T->sy[T->me] - 0.9f * T->tok, room = bot - top;
    K.d = floorf(fminf(fminf(T->u, T->b.w * 0.8f / 5), room / 2.6f));
    float y = top + (room - 2.3f * K.d) / 2 + 0.5f * K.d;
    for (int i = 0; i < 4; i++) K.suit[i] = (pt){T->cx + (i - 1.5f) * 1.15f * K.d, y};
    K.pass = (pt){T->cx - 0.7f * K.d, y + 1.3f * K.d};
    K.ok = (pt){T->cx + 0.7f * K.d, y + 1.3f * K.d};
    if (kvj_must_name(&G.pos)) K.ok.x = T->cx;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    kvj_new(&G, (uint32_t)ui_random(1 << 30));
    ct_game_new(&U);
    reset_ui();
}

static int status(void)
{
    const kvpos *p = &G.pos;
    if (p->phase != KP_OVER) return GS_PLAY;
    int m = kvj_winners(p);
    return m == 15 ? GS_DRAW : gs_win(m & 1 ? 0 : 1);
}

static unsigned winners(void) { return (unsigned)kvj_winners(&G.pos); }

static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    kvpos before = G.pos;
    if (!kvj_play(&G, a)) { ui_sound(SND_DENY); return; }
    const kvpos *p = &G.pos;
    ct_calm(&U.A);
    U.fl.n = 0;
    if (KA_KIND(a) == KA_PLAY) {
        ct_game_played(&U, KA_ARG(a), before.t.hand[U.T.me], before.t.n[U.T.me],
                       p->t.tricks != before.t.tricks ? p->t.last : NULL, p->t.last_leader, p->t.last_winner);
        if (p->t.tricks != before.t.tricks && p->trick_roem) ct_game_float(&U, p->t.last_winner, p->trick_roem, 0);
    } else
        ui_sound(KA_KIND(a) == KA_PASS ? SND_TAP : SND_PLACE);
    reset_ui();
    if (KA_KIND(a) == KA_NEXT) U.seen = 0;
    if (p->turn != before.turn || p->phase >= KP_DEAL) U.floor_n = G.n;
    if (p->phase < KP_DEAL) ui_handoff(p->turn);
}

static int undo(void)
{
    if (pick >= 0 || U.F.raised != CARD_NONE || U.show_last) { reset_ui(); return 1; }
    if (G.n <= U.floor_n || !kvj_undo(&G)) return 0;
    ct_calm(&U.A);
    U.fl.n = 0;
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = kvj_script(&G, s);
    ct_calm(&U.A);
    reset_ui();
    U.floor_n = G.n;
    U.seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

const kvgame *klaverjas_live(void) { return &G; }

int klaverjas_show(const char *move)
{
    static kvgame t;
    t = G;
    if (!kvj_script(&t, move) || t.n != G.n + 1) return 0;
    act(t.act[G.n]);
    return 1;
}

static int busy(void) { return ct_busy(&U.A); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_bidding(int live)
{
    for (int i = 0; i < 4; i++)
        ct_choice(ct_call_tex(SUIT_ORDER[i], K.d), K.suit[i].x, K.suit[i].y, K.d, SUIT_ORDER[i] == pick, 1);
    if (!kvj_must_name(&G.pos)) ct_pass(K.pass.x, K.pass.y, K.d, 0);
    if (pick >= 0 && live) ui_mark_gold(K.ok.x, K.ok.y, 1.9f * K.d);
    ct_button(K.ok.x, K.ok.y, K.d, pick >= 0, CT_TICK, 0);
}

/* beside each seat: a pass in the bidding; the trumps by the player who named them */
static void draw_words(void)
{
    const kvpos *p = &G.pos;
    float d = 0.95f * U.T.tok;
    for (int q = 0; q < TK4_P; q++) {
        float x, y;
        ct_seat_spot(&U.T, q, 0, &x, &y);
        if (p->phase == KP_BID) {
            if (p->word[q] == KW_PASS) ct_pass_badge(x, y, d);
        } else if (p->phase == KP_PLAY && q == p->taker)
            ct_call(p->trump, x, y, d, 255);
    }
}

/* the deal's table: a row a team, then the list of deals */
static void draw_summary(void)
{
    const kvpos *p = &G.pos;
    const kvdeal *r = &p->list[p->ndeals - 1];
    float x0, pw, y, rh, u = U.T.u;
    ct_summary(&U.T, 4, &x0, &pw, &y, &rh);
    float d = floorf(fminf(0.62f * rh, 0.75f * u)), L = x0 + 0.35f * u + d / 2, R = x0 + pw - 0.35f * u;
    float nh = floorf(0.36f * rh);
    int P = tk4_team(r->taker), lead = p->total[0] == p->total[1] ? -1 : p->total[1] > p->total[0];
    for (int k = 0; k < 2; k++, y += rh) {
        if (k == lead) ui_mark_gold(L + 0.4f * d, y, 2.4f * d);
        ct_chips(1u << k | 1u << (k + 2), L, y, d, 255);
        float tx = L + 2.0f * d;
        if (k == P) {                           /* the playing team: their trumps (nat: struck through) */
            if (!r->made) ui_mark_glow(tx, y, 1.4f * d);
            ct_call(r->trump, tx, y, 0.85f * d, 255);
            if (!r->made) ct_strike(tx, y, d);
        }
        float xe = x0 + 0.385f * pw;
        if (r->pit == k + 1) ui_mark_gold(xe, y, 1.4f * d);
        ui_number(xe, y, nh, r->eyes[k], 1);
        if (r->roem[k]) ui_number_signed(x0 + 0.515f * pw, y, 0.8f * nh, r->roem[k], 1, 1);
        float xs = x0 + 0.7f * pw;
        ct_pic(CT_ARROW, x0 + 0.607f * pw, y, 0.7f * nh, 0, 246, 238, 220, 200);
        ui_number(xs, y, 0.95f * nh, r->score[k], 1);
        ct_total(R, y, 1.1f * nh, p->total[k]);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
    uint8_t who[KVJ_DEALS], made[KVJ_DEALS];
    for (int i = 0; i < p->ndeals; i++) {
        int t = tk4_team(p->list[i].taker);
        who[i] = (uint8_t)(1u << t | 1u << (t + 2));
        made[i] = p->list[i].made;
    }
    ct_deal_list(x0, pw, y + 0.1f * rh, rh, KVJ_DEALS, 8, p->ndeals, who, made);
}

static void draw(void)
{
    const kvpos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&U.A), ph = p->phase, hide = ui.viewer < 0;
    ct_game_look(&U, p->t.hand[U.T.me], p->t.n[U.T.me], ph == KP_BID);
    int dealing = ct_hides(&U.A, CT_HAND_TAG, -1, -1);   /* the deal still flying in: no bid under the deck */
    controls();
    ct_draw_seats(&U.T, ph < KP_DEAL ? p->turn : -1);
    draw_words();
    if (ph == KP_BID && p->turn == U.T.me && !hide && !dealing) draw_bidding(live);
    ct_game_draw_middle(&U, p->t.tricks, p->t.trick, NULL, p->t.ntrick, p->t.leader, live && ph == KP_PLAY);
    int summary = ph >= KP_DEAL && !ct_busy(&U.A);
    int mine = live && ph == KP_PLAY && tk4_turn(&p->t) == U.T.me;
    ct_game_draw_hand(&U, p->t.hand[U.T.me], summary ? 0 : p->t.n[U.T.me], mine, can_play, hide);
    ct_game_draw_last(&U, p->t.last, NULL, TK4_P, p->t.last_leader, p->t.last_winner);
    if (summary) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (can_play(card)) act(KA_PLAY | card);
    else { ct_lower(&U.F); ui_sound(SND_DENY); }
}

static void press_bidding(float x, float y)
{
    if (!kvj_must_name(&G.pos) && ct_on(K.pass.x, K.pass.y, K.d, x, y)) { act(KA_PASS); return; }
    for (int i = 0; i < 4; i++)
        if (ct_on(K.suit[i].x, K.suit[i].y, K.d, x, y)) {
            if (pick == SUIT_ORDER[i]) { act(KA_TRUMP | pick); return; }   /* tapped again: named */
            pick = SUIT_ORDER[i];
            ui_sound(SND_TAP);
            return;
        }
    if (ct_on(K.ok.x, K.ok.y, K.d, x, y) && pick >= 0) { act(KA_TRUMP | pick); return; }
    ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    const kvpos *p = &G.pos;
    if (ct_game_press_first(&U)) return;
    if (p->phase == KP_DEAL) { act(KA_NEXT); return; }
    if (p->phase == KP_OVER) return;
    if (ct_game_press_pile(&U, p->t.tricks, x, y)) return;
    if (p->turn != U.T.me) { ui_sound(SND_DENY); return; }
    controls();
    if (p->phase == KP_BID) press_bidding(x, y);
    else ct_game_press_card(&U, p->t.hand[U.T.me], p->t.n[U.T.me], can_play, send, x, y);
}

static void motion(float x, float y) { ct_touch_move(&U.F, x, y); }

static void release(float x, float y) { ct_game_release(&U, can_play, send, x, y); }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, U.T.deck, G.pos.t.n[player], G.pos.total[tk4_team(player)]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the trumps' king and queen (stuk), the nine, the jack of clubs (klaver) on top */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[4] = {CARD(SUIT_CLUBS, RANK_QUEEN), CARD(SUIT_CLUBS, RANK_KING), CARD(SUIT_CLUBS, 9),
                                 CARD(SUIT_CLUBS, RANK_JACK)};
    ct_thumb(DECK_FRENCH, cards, 4, size, rgba);
}

static const game klaverjas_game = {
    .id = "klaverjas", .order = 324, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(klaverjas_game)
