/*
 * Whist in RetroStone Board: four players in two partnerships, or German whist for two, passing the device round,
 * each hand secret (VIEW_HIDDEN; the rules: whist_rules.c, whist.h). The trick table (the kit's cards_table.h): the
 * viewer's hand fanned at the bottom, the seats round the middle with the tricks each has taken, the trick, the trick
 * pile at the lower right (tap it: the last trick). At the lower left: four players, the turned card with the
 * dealer's chip on it while the dealer holds it, then the trumps' sign; two players, the stock with its face-up card,
 * then the trumps' sign. Playing: tap a card (it rises), tap it again, or drag it to the middle. After each deal a
 * summary: each side's tokens, its tricks, its honours, the deal's points and the total.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/tricks.h"
#include "games/whist.h"
#include "pages.h"
#include "shell.h"

#define DRAW_MS 320

static whist_game G;
static ct_table T;
static ct_moves A;
static ct_touch F;
static int floor_n, show_last;
static unsigned seen;
static float ws, stx, sty;                      /* the lower left: its cards' width, centre */

static void layout(box b)
{
    ct_layout(&T, b, G.players ? G.players : 4, ui_bottom(), WHIST_HAND);
    ws = floorf(0.72f * T.wt);
    stx = b.x + 0.35f * T.u + 0.5f * ws;
    sty = T.hand_top - 0.5f * ws * T.asp - 0.15f * T.u;
}

static void reset_ui(void)
{
    show_last = 0;
    ct_touch_reset(&F);
}

static int hand(uint8_t *h, int *how, int live)
{
    const whist_pos *p = &G.pos;
    int n = p->n[T.me], mine = live && p->turn == T.me && p->phase == WP_PLAY;
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][i];
        h[i] = (uint8_t)c;
        how[i] = mine ? (whist_can_play(p, c) ? CD_GLOW : CD_DIM) : 0;
        if (ct_is_raised(&F, i)) how[i] = CD_RAISED;
    }
    return n;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    whist_new(&G, ui.players, v, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    int w = whist_winner(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static unsigned winners(void)                   /* the winning side: both partners */
{
    int w = whist_winner(&G.pos);
    if (w < 0) return 0;
    return G.players == 4 ? 1u << w | 1u << (w + 2) : 1u << w;
}

static int turn(void) { return G.pos.turn; }

/* the cards drawn after a trick (two players): the face-up one to the winner, the next to the loser */
static void draws(int delay)
{
    const whist_pos *p = &G.pos;
    for (int k = 0; k < 2; k++) {
        int q = (p->last_winner + k) % 2, c = p->drew[k];
        if (c == CARD_NONE) continue;
        if (q == T.me) {
            cards_spot s[CT_HAND];
            ct_fan(&T, p->n[q], s);
            int i = ct_index(p->hand[q], p->n[q], c);
            if (i < 0) continue;
            ct_slide(&A, T.deck, c, stx, sty, ws, 0, k == 0, s[i].x, s[i].y, T.wh, s[i].deg, 1, DRAW_MS,
                     delay + 200 * k, CT_HAND_TAG, i, -1, SND_MOVE);
        } else
            ct_slide(&A, T.deck, c, stx, sty, ws, 0, k == 0, T.sx[q], T.sy[q], 0.45f * T.wt, T.tdeg[q], 0, DRAW_MS,
                     delay + 200 * k, CT_SEAT, q, -1, SND_MOVE);
    }
}

static void animate(const whist_pos *b, int a, const uint8_t *h, int n)
{
    const whist_pos *p = &G.pos;
    if (WA_KIND(a) != WA_PLAY) return;
    int c = WA_ARG(a), pl = b->turn;
    if (pl == T.me) ct_play(&A, &T, &F, h, n, c);
    else
        ct_rest(&A, ct_slide(&A, T.deck, c, T.sx[pl], T.sy[pl], 0.45f * T.wt, T.tdeg[pl], 0, T.tx[pl], T.ty[pl], T.wt,
                             T.tdeg[pl], 1, CT_SLIDE_MS, 0, CT_TRICK, pl, -1, SND_PLACE), 200);
    if (p->tricks == b->tricks || p->last_winner == WHIST_NONE) return;
    int look = CT_SLIDE_MS + CT_LOOK_MS;
    ct_gather(&A, &T, p->last, NULL, p->players, p->last_leader, p->last_winner, look, CT_SLIDE_MS);
    if (p->drew[0] != CARD_NONE) draws(look + 520);
}

static void act(int a)
{
    whist_pos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 0);
    if (!whist_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    animate(&before, a, h, n);
    reset_ui();
    const whist_pos *p = &G.pos;
    if (WA_KIND(a) == WA_NEXT) seen = 0;
    if (p->turn != before.turn || p->phase != WP_PLAY) floor_n = G.n;
    if (p->phase == WP_PLAY) ui_handoff(p->turn);
}

static int undo(void)
{
    if (F.raised != CARD_NONE || show_last) { reset_ui(); return 1; }
    if (G.n <= floor_n || !whist_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = whist_script(&G, s);
    ct_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&A); }

/* ---- the bot's side (whist_bot.c): the game read-only, and one action of the notation played as a tap ------------ */
const whist_game *whist_live(void) { return &G; }

int whist_show(const char *move)
{
    static whist_game t;
    t = G;
    if (!whist_script(&t, move) || t.n != G.n + 1) return 0;
    act(t.act[G.n]);
    return 1;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static int still_held(const whist_pos *p)       /* four players: the dealer still holds the turned card */
{
    return p->phase == WP_PLAY && ct_index(p->hand[p->dealer], p->n[p->dealer], p->up) >= 0;
}

/* the lower left: the turned card and the dealer's chip, the stock and its face-up card, or the trumps' sign */
static void corner_draw(const whist_pos *p)
{
    float d = 0.75f * ws;
    if (p->players == 2) {
        int n = p->nstock, moving = 0;
        for (int i = 0; i < A.n; i++)           /* the cards on their way out still show in the stock */
            if (cards_slide_busy(&A.m[i].s) && (A.m[i].tag == CT_SEAT || A.m[i].tag == CT_HAND_TAG) &&
                ui_ms() < A.m[i].s.t0 + (Uint32)A.m[i].s.delay)
                moving++;
        n += moving;
        if (n > 0) {                            /* the next card turns up once both have drawn */
            int top = moving >= 2 ? p->drew[0] : moving == 1 ? CARD_NONE : p->up;
            cards_pile(T.deck, stx, sty, ws, n > 6 ? 6 : n, top, 0);
            float sd = 0.5f * ws;               /* the trumps' sign beside it: the face-up card's suit changes */
            ui_put_c(ct_suit(T.deck, p->trump, (int)sd), stx + 0.5f * ws + 0.45f * sd,
                     sty + 0.5f * ws * T.asp - 0.5f * sd, sd, sd);
            return;
        }
    } else if (still_held(p)) {
        cards_draw(T.deck, p->up, stx, sty, ws, 0, 0);
        float td = 0.5f * ws;
        ui_put_c(ct_token(p->dealer, (int)td), stx + 0.42f * ws, sty + 0.5f * ws * T.asp - 0.1f * ws, td, td);
        return;
    }
    ui_put_c(ct_suit(T.deck, p->trump, (int)d), stx, sty, d, d);
}

/* the deal's summary: a row a side, its tokens (the higher total in gold light), its tricks in a dark disc, its
 * honours (a plus), the deal's points, the total */
static void draw_summary(void)
{
    const whist_pos *p = &G.pos;
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, 2, &x0, &pw, &y, &rh);
    rh = fminf(rh, 1.4f * u);
    float d = fminf(0.62f * rh, 0.75f * u);
    const int ts = (int)d;
    int four = p->players == 4, top = p->score[0] >= p->score[1] ? 0 : 1;
    for (int s = 0; s < 2; s++, y += rh) {
        float tx = x0 + 0.7f * u;
        if (s == top && p->score[0] != p->score[1]) ui_mark_gold(tx + (four ? 0.42f * d : 0), y, 1.9f * d);
        for (int k = 0; k < (four ? 2 : 1); k++) ui_put_c(ct_token(s + 2 * k, ts), tx + k * 0.85f * d, y, d, d);
        float bx = x0 + pw * 0.36f, bd = 0.66f * u;
        ct_badge(bx, y, bd, whist_side_tricks(p, s));
        if (p->hon[s]) ct_plus(x0 + pw * 0.53f, y, 0.36f * rh, p->hon[s]);
        if (p->pts[s]) ct_plus(x0 + pw * 0.69f, y, 0.4f * rh, p->pts[s]);
        ui_number(x0 + pw - 0.8f * u, y, 0.5f * rh, p->score[s], 1);
        if (s == 0) ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    const whist_pos *p = &G.pos;
    if (T.me != ui_bottom() && ui_bottom() >= 0) layout(T.b);
    int live = !ui_handing() && !ct_busy(&A), hide = ui.viewer < 0;   /* hide: the hand-off cover */
    if (!(seen >> T.me & 1) && !ui_handing()) {
        seen |= 1u << T.me;
        if (p->phase == WP_PLAY && p->tricks == 0 && p->ntrick < p->players) {
            uint8_t h[CT_HAND];
            int how[CT_HAND];
            ct_deal(&A, &T, h, hand(h, how, 0));
        }
    }
    ct_draw_seats(&T, p->phase == WP_PLAY ? p->turn : -1);
    if (p->phase == WP_PLAY)
        for (int q = 0; q < p->players; q++) {   /* the tricks taken */
            float x, y;
            ct_seat_spot(&T, q, 0, &x, &y);
            int k = p->players == 2 && !p->nstock ? p->late[q] : p->won[q];
            if (k) ui_number(x, y, 0.5f * T.tok, k, TH.dark);   /* on the table: the light one's ink */
        }
    ct_draw_pile(&T, p->tricks);
    corner_draw(p);
    ct_draw_trick(&T, &A, p->trick, NULL, p->ntrick, p->leader);
    if (F.raised != CARD_NONE && live) ct_draw_target(&T);
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, live);
    if (hide) ct_hand_backs(&T, n);
    else ct_hand_draw(&T, &A, h, n, how, NULL, ct_dragged(&F));
    ct_draw(&A);
    if (!hide) ct_touch_draw(&F, &T);
    if (show_last && p->last_winner != WHIST_NONE)
        ct_draw_last(&T, p->last, NULL, p->players, p->last_leader, p->last_winner);
    if ((p->phase == WP_ROUND || p->phase == WP_OVER) && !ct_busy(&A)) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (whist_can_play(&G.pos, card)) act(WA_PLAY | card);
    else { ct_lower(&F); ui_sound(SND_DENY); }
}

static void press(float x, float y)
{
    const whist_pos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last) { show_last = 0; return; }
    if (ct_busy(&A)) return;
    if (p->phase == WP_ROUND) { act(WA_NEXT); return; }
    if (p->phase == WP_OVER) return;
    if (ct_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    if (F.raised != CARD_NONE && ct_on_target(&T, x, y)) { send(F.raised); return; }
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 1);
    ct_touch_hand(&F, &T, h, n, how, NULL, x, y);
}

static void motion(float x, float y) { ct_touch_move(&F, x, y); }

static void release(float x, float y)
{
    int c, r = ct_touch_up(&F, &T, x, y, &c);
    if (r == CT_T_SEND) send(c);
    else if (r == CT_T_TAP && ct_tap_play(&F, c, whist_can_play(&G.pos, c))) send(c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, T.deck, G.pos.n[player], G.pos.score[whist_side(&G.pos, player)]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: a plain fan, the ace of trumps on top; the honours' variants: the four honours of clubs */
static void thumb(int v, int size, uint8_t *rgba)
{
    static const int plain[4] = {CARD(SUIT_HEARTS, 7), CARD(SUIT_DIAMONDS, 10), CARD(SUIT_SPADES, RANK_KING),
                                 CARD(SUIT_CLUBS, RANK_ACE)};
    static const int honours[4] = {CARD(SUIT_CLUBS, RANK_JACK), CARD(SUIT_CLUBS, RANK_QUEEN),
                                   CARD(SUIT_CLUBS, RANK_KING), CARD(SUIT_CLUBS, RANK_ACE)};
    ct_thumb(DECK_FRENCH, v > WV_SHORT ? honours : plain, 4, size, rgba);
}

static const game whist_game_def = {
    .id = "whist", .order = 328, .variants = WHIST_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 4, .players_usual = 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(whist_game_def)
