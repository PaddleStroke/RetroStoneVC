/*
 * Doppelkopf in RetroStone Board: four players passing the device round, each hand secret and the partners hidden
 * (VIEW_HIDDEN; the rules: doppelkopf_rules.c, doppelkopf.h, after the DDV's tournament rules). The table
 * (games/trick4_ui.h on the kit's cards_table.h): the viewer's hand of twelve fanned at the bottom, the seats round the
 * middle, the trick, the trick pile (tap it: the last trick). Reservations: the four suit solos (a suit's sign), the
 * queens' solo (the queen's figure), the jacks' (the jack's: a bishop), fleshless (the four aces), the marriage (the two
 * queens of clubs, only when holding both): tap one, then the tick (or the same again); healthy: the pass button. Each
 * seat shows its word (the pass sign: healthy; a lamp: a reservation). In play the parties show as the table learns
 * them: the queen of clubs for Re, struck through for Kontra (the viewer's own always), in gold light once announced;
 * the soloist's seat shows the solo, the bride's the marriage till the partner is found; no 90, 60, 30 and schwarz in
 * a dark disc (schwarz: 0). The announce button, across from the trick pile, shows the next announcement the viewer
 * may make before playing a card. After each deal a table: each party's chips, the game, card points, announcements,
 * extra points (the fox: the ace of diamonds; a Doppelkopf: 40; Karlchen: the jack of clubs; against the Alten),
 * the winners' tick; the game value and each Re player's points; every player's score and total; the list of deals.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/doppelkopf.h"
#include "games/trick4_ui.h"
#include "shell.h"

#define QC CARD(SUIT_CLUBS, RANK_QUEEN)

static dkgame G;
static tk4_ui U;
static int pick = -1;                           /* the reservation chosen */

static void layout(box b) { tk4u_layout(&U, b, DKO_HAND); }

static void reset_ui(void)
{
    pick = -1;
    tk4u_reset(&U);
}

static int can_play(int card) { return dko_can_play(&G.pos, card); }

/* a game's picture: a suit's sign, the queen's or the jack's figure, the four aces, two queens of clubs */
static int game_pic(int game)
{
    if (game >= DKG_SUIT) return game - DKG_SUIT;
    return game == DKG_QUEENS ? TK4_PIC_FIG | QUEEN : game == DKG_JACKS ? TK4_PIC_FIG | BISHOP
           : game == DKG_FLESHLESS ? TK4_PIC_ACES : TK4_PIC_PAIR | QC;
}

static const int LEVEL_NUM[6] = {0, 0, 90, 60, 30, 0};   /* the announcements beyond Re and Kontra, as shown */

/* a party's sign: the queen of clubs (Re), struck through (Kontra); in gold light once announced */
static void party(int k, float x, float y, float d, int said)
{
    if (said) ui_mark_gold(x, y, 1.6f * d);
    tk4u_card(QC, x, y, 0.95f * d, 0);
    if (k) tk4u_strike(x, y, 0.95f * d);
}

/* ---- the buttons in the middle ----------------------------------------------------------------------------------- */
typedef struct { float x, y; } pt;
#define CHOICES 8
static struct { float d; pt at[CHOICES], pass, ok, ann; } K;
static const uint8_t CHOICE[CHOICES] = {DKG_SUIT + SUIT_CLUBS, DKG_SUIT + SUIT_SPADES, DKG_SUIT + SUIT_HEARTS,
                                        DKG_SUIT + SUIT_DIAMONDS, DKG_QUEENS, DKG_JACKS, DKG_FLESHLESS, DKG_WEDDING};

static void controls(void)
{
    const ct_table *T = &U.T;
    float top = T->sy[(T->me + 2) % TK4_P] + 0.9f * T->tok, bot = T->sy[T->me] - 0.9f * T->tok, room = bot - top;
    K.d = floorf(fminf(fminf(T->u, T->b.w * 0.8f / 5), room / 3.9f));
    float y = top + (room - 3.6f * K.d) / 2 + 0.5f * K.d;
    for (int i = 0; i < CHOICES; i++) K.at[i] = (pt){T->cx + (i % 4 - 1.5f) * 1.15f * K.d, y + (i / 4) * 1.25f * K.d};
    K.pass = (pt){T->cx - 0.7f * K.d, y + 2.6f * K.d};
    K.ok = (pt){T->cx + 0.7f * K.d, y + 2.6f * K.d};
    K.ann = (pt){2 * T->b.x + T->b.w - T->px, T->py - 0.1f * T->u};   /* across from the trick pile */
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    dko_new(&G, v, (uint32_t)ui_random(1 << 30));
    tk4u_new(&U);
    reset_ui();
}

static int status(void)
{
    const dkpos *p = &G.pos;
    if (p->phase != DKP_OVER) return GS_PLAY;
    int m = dko_winners(p);
    for (int q = 0; q < TK4_P; q++)
        if (m == 1 << q) return gs_win(q);
    return GS_DRAW;
}

static unsigned winners(void) { return (unsigned)dko_winners(&G.pos); }

static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    dkpos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = tk4u_hand(&U, &G.pos.t, 0, 0, can_play, h, how);
    if (!dko_play(&G, a)) { ui_sound(SND_DENY); return; }
    const dkpos *p = &G.pos;
    ct_calm(&U.A);
    if (DKA_KIND(a) == DKA_PLAY) tk4u_played(&U, &p->t, before.t.tricks, DKA_ARG(a), h, n);
    else ui_sound(DKA_KIND(a) == DKA_SAY && !DKA_ARG(a) ? SND_TAP : SND_PLACE);
    reset_ui();
    if (DKA_KIND(a) == DKA_NEXT) U.seen = 0;
    if (p->turn != before.turn || p->phase >= DKP_DEAL) U.floor_n = G.n;
    if (p->phase < DKP_DEAL) ui_handoff(p->turn);
}

static int undo(void)
{
    if (pick >= 0 || U.F.raised != CARD_NONE || U.show_last) { reset_ui(); return 1; }
    if (G.n <= U.floor_n || !dko_undo(&G)) return 0;
    ct_calm(&U.A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = dko_script(&G, s);
    ct_calm(&U.A);
    reset_ui();
    U.floor_n = G.n;
    U.seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&U.A); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_reserve(int live)
{
    const dkpos *p = &G.pos;
    for (int i = 0; i < CHOICES; i++)
        ct_choice(tk4u_tex(game_pic(CHOICE[i]), K.d), K.at[i].x, K.at[i].y, K.d, CHOICE[i] == pick,
                  dko_say_ok(p, CHOICE[i]));
    ct_pass(K.pass.x, K.pass.y, K.d, 0);
    if (pick >= 0 && live) ui_mark_gold(K.ok.x, K.ok.y, 1.9f * K.d);
    ct_button(K.ok.x, K.ok.y, K.d, pick >= 0, CT_TICK, 0);
}

/* the announce button: the next announcement the viewer may make */
static void draw_announce(int live)
{
    const dkpos *p = &G.pos;
    if (p->turn != U.T.me || !dko_announce_ok(p)) return;
    int k = dko_party(p, U.T.me), L = p->ann[k] + 1;
    float d = 0.9f * K.d;
    if (live) ui_mark_gold(K.ann.x, K.ann.y, 1.8f * d);
    ct_button(K.ann.x, K.ann.y, d, 1, -1, 0);
    if (L == DKL_PARTY) party(k, K.ann.x, K.ann.y, 0.7f * d, 0);
    else ui_number(K.ann.x, K.ann.y, 0.42f * d, LEVEL_NUM[L], 1);
}

/* beside each seat: its word; then what the table knows of its party (the viewer's own), the solo, the marriage, the
 * announcements beyond Re and Kontra */
static void draw_words(int hide)
{
    const dkpos *p = &G.pos;
    float d = 0.95f * U.T.tok;
    if (p->phase >= DKP_DEAL) return;
    for (int q = 0; q < TK4_P; q++) {
        float x, y, x1, y1;
        ct_seat_spot(&U.T, q, 0, &x, &y);
        ct_seat_spot(&U.T, q, 1, &x1, &y1);
        if (p->phase == DKP_RESERVE) {
            if (p->said[q] == DKW_HEALTHY) ct_pass_badge(x, y, d);
            else if (p->said[q] == DKW_RESERVED) tk4u_icon_badge(ICON_HINT, x, y, d);
            continue;
        }
        int solo = dko_is_solo(p->game), wed = p->game == DKG_WEDDING && !p->open;
        if ((solo || wed) && q == p->soloist) {
            if (p->level[q]) ui_mark_gold(x, y, 1.7f * d);
            tk4u_pic(game_pic(p->game), x, y, 1.1f * d, 255);
        } else {
            int k = dko_known(p, q);
            if (q == U.T.me && !hide) k = dko_party(p, q);
            if (solo && !p->level[q]) k = -1;   /* a solo's opponents: plain, unless they announce */
            if (k >= 0) party(k, x, y, d, p->level[q] != 0);
        }
        if (p->level[q] >= DKL_90) ct_badge(x1, y1, d, LEVEL_NUM[p->level[q]]);
    }
}

/* the extra points of a party from x: the fox, Doppelkopf, Karlchen, against the Alten; how many of each */
static void draw_extras(const dkdeal *r, int k, float x, float y, float d, float nh)
{
    struct { int n, kind; } e[4] = {{r->fox[k], 0}, {r->doko[k], 1}, {r->karl[k], 2}, {k ? r->alten : 0, 3}};
    for (int i = 0; i < 4; i++) {
        if (!e[i].n) continue;
        if (e[i].kind == 0) tk4u_card(CARD(SUIT_DIAMONDS, RANK_ACE), x, y, 1.05f * d, 0);
        else if (e[i].kind == 1) ct_badge(x, y, 0.8f * d, 40);
        else if (e[i].kind == 2) tk4u_card(CARD(SUIT_CLUBS, RANK_JACK), x, y, 1.05f * d, 0);
        else party(1, x, y, 0.9f * d, 0);
        if (e[i].n > 1) {
            ui_number(x + 0.55f * d, y + 0.25f * d, 0.6f * nh, e[i].n, 1);
            x += 0.35f * d;
        }
        x += 0.85f * d;
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
    const dkpos *p = &G.pos;
    const dkdeal *r = &p->list[p->ndeals - 1];
    float x0, pw, y, rh, u = U.T.u;
    ct_summary(&U.T, 3 + TK4_P + 1, &x0, &pw, &y, &rh);
    float d = floorf(fminf(0.62f * rh, 0.75f * u)), L = x0 + 0.35f * u + d / 2, R = x0 + pw - 0.35f * u;
    float nh = floorf(0.38f * rh);
    for (int k = 0; k < 2; k++, y += rh) {      /* a row a party */
        unsigned who = k ? 15u & ~r->re : r->re;
        float w = tk4u_chips(who, L, y, d, 255), x = L + w - d / 2 + 0.7f * d;
        if (!k && r->game != DKG_NORMAL) tk4u_pic(game_pic(r->game), x, y, 0.9f * d, 255);
        else if (k) party(1, x, y, 0.9f * d, 0);
        float xe = x0 + 0.42f * pw;
        ui_number(xe, y, nh, r->eyes[k], 1);
        float xa = x0 + 0.53f * pw;
        if (r->ann[k] >= DKL_90) ct_badge(xa, y, 0.85f * d, LEVEL_NUM[r->ann[k]]);
        else if (r->ann[k]) party(k, xa, y, 0.8f * d, 1);
        draw_extras(r, k, x0 + 0.63f * pw, y, 0.8f * d, nh);
        if (r->winner == k) ct_pic(CT_TICK, R - 0.4f * d, y, 0.8f * d, 0, TK4_GOLD[0], TK4_GOLD[1], TK4_GOLD[2], 255);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
    /* the value: the winners' sign and the game value (nobody: the pass sign); the extra points' balance; each Re
     * player's points (a solo: the soloist three times) */
    if (r->winner >= 0) party(r->winner, L, y, 0.9f * d, 0);
    else ct_pass_badge(L, y, 0.9f * d);
    ui_number(L + 0.9f * d, y, nh, r->value, 1);
    int net = r->re_points - (r->winner == 0 ? r->value : r->winner == 1 ? -r->value : 0);
    if (net) ui_number_signed(x0 + 0.45f * pw, y, 0.85f * nh, net, 1, 1);
    ct_pic(CT_ARROW, x0 + 0.62f * pw, y, 0.8f * nh, 0, 246, 238, 220, 200);
    party(0, x0 + 0.7f * pw, y, 0.8f * d, 0);
    if (r->solo) {
        times(x0 + 0.76f * pw, y, 0.6f * nh);
        ui_number(x0 + 0.76f * pw + 0.55f * nh, y, 0.75f * nh, 3, 1);
    }
    tk4u_total(R, y, 1.1f * nh, r->re_points);
    ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    y += rh;
    int best = p->total[0];                     /* each player: the deal's score, the total (the best in gold) */
    for (int q = 1; q < TK4_P; q++) best = p->total[q] > best ? p->total[q] : best;
    for (int q = 0; q < TK4_P; q++, y += rh) {
        if (p->total[q] == best) ui_mark_gold(L, y, 1.8f * d);
        ui_put_c(ct_token(q, (int)d), L, y, d, d);
        if (r->score[q]) ui_number_signed(x0 + 0.5f * pw, y, nh, r->score[q], 1, 1);
        tk4u_total(R, y, 1.2f * nh, p->total[q]);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
    uint8_t who[DKO_DEALS_MAX], made[DKO_DEALS_MAX];
    for (int i = 0; i < p->ndeals; i++) {
        const dkdeal *e = &p->list[i];
        who[i] = e->winner < 0 ? 0 : e->winner == 0 ? e->re : (uint8_t)(15u & ~e->re);
        made[i] = e->winner >= 0;
    }
    int N = dko_deals(p->variant);
    tk4u_deal_list(x0, pw, y, rh, N, 12, p->ndeals, who, made);
}

static void draw(void)
{
    const dkpos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&U.A), ph = p->phase, hide = ui.viewer < 0;
    tk4u_look(&U, &p->t, ph == DKP_RESERVE);
    controls();
    ct_draw_seats(&U.T, ph < DKP_DEAL ? p->turn : -1);
    draw_words(hide);
    if (ph == DKP_RESERVE && p->turn == U.T.me && !hide) draw_reserve(live);
    if (ph == DKP_PLAY && !hide) draw_announce(live);
    tk4u_draw_middle(&U, &p->t, live, ph == DKP_PLAY);
    int summary = ph >= DKP_DEAL && !ct_busy(&U.A);
    tk4u_draw_hand(&U, &p->t, live, ph == DKP_PLAY, can_play, hide, summary);
    if (summary) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (can_play(card)) act(DKA_PLAY | card);
    else { U.F.raised = CARD_NONE; ui_sound(SND_DENY); }
}

static void press_reserve(float x, float y)
{
    const dkpos *p = &G.pos;
    if (ct_on(K.pass.x, K.pass.y, K.d, x, y)) { act(DKA_SAY); return; }
    for (int i = 0; i < CHOICES; i++)
        if (ct_on(K.at[i].x, K.at[i].y, K.d, x, y)) {
            if (!dko_say_ok(p, CHOICE[i])) { ui_sound(SND_DENY); return; }
            if (pick == CHOICE[i]) { act(DKA_SAY | pick); return; }   /* tapped again: said */
            pick = CHOICE[i];
            ui_sound(SND_TAP);
            return;
        }
    if (ct_on(K.ok.x, K.ok.y, K.d, x, y) && pick >= 0) { act(DKA_SAY | pick); return; }
    ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    const dkpos *p = &G.pos;
    if (tk4u_press_first(&U)) return;
    if (p->phase == DKP_DEAL) { act(DKA_NEXT); return; }
    if (p->phase == DKP_OVER) return;
    if (tk4u_press_pile(&U, &p->t, x, y)) return;
    if (p->turn != U.T.me) { ui_sound(SND_DENY); return; }
    controls();
    if (p->phase == DKP_RESERVE) { press_reserve(x, y); return; }
    if (dko_announce_ok(p) && ct_on(K.ann.x, K.ann.y, 0.9f * K.d, x, y)) {
        act(DKA_ANNOUNCE | (p->ann[dko_party(p, p->turn)] + 1));
        return;
    }
    tk4u_press_card(&U, &p->t, can_play, send, x, y);
}

static void motion(float x, float y) { tk4u_motion(&U, x, y); }

static void release(float x, float y) { tk4u_release(&U, can_play, send, x, y); }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, G.pos.t.n[player], G.pos.total[player]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the fox, the Dulle and the two Alten on top; the picker's: with one pile of deals (12) or two
 * (24) */
static void thumb(int v, int size, uint8_t *rgba)
{
    static const int cards[4] = {CARD(SUIT_DIAMONDS, RANK_ACE), CARD(SUIT_HEARTS, 10), QC, QC};
    ct_thumb(cards, 4, size, rgba);
    if (v < 0) return;
    int cw = (int)(size * 0.2f), ch = (int)(cw * cards_aspect(DECK_FRENCH));
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    cards_paint(DECK_FRENCH, CARD_NONE, cw, ch, b);
    int piles = v == DKO_LONG ? 2 : 1;
    for (int k = 0; k < piles; k++)
        for (int j = 0; j < 4; j++)
            art_blend(rgba, size, size, b, cw, ch, (int)(size * 0.5f + (k - piles / 2.0f) * 1.15f * cw) - j,
                      size - ch - (int)(size * 0.02f) - 2 * j);
    free(b);
}

static const game doppelkopf_game = {
    .id = "doppelkopf", .order = 321, .variants = DKO_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(doppelkopf_game)
