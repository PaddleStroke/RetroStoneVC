/*
 * Briscola in RetroStoneBoard: 2 players, or 4 in two partnerships, passing the device round, each hand secret
 * (VIEW_HIDDEN; the rules: briscola_rules.c, briscola.h). The Italian pack (the card kit's DECK_ITALIAN) on the
 * trick-taking table (the kit's cards_table.h: the seats, the trick, the hand, the touch, the summary's panel). The
 * stock at the lower left, the briscola across under it, face up; once it is drawn, its suit's symbol stays in its
 * place. Playing: tap a card (it rises), then tap it again or its place in the middle; or drag it there. A full trick
 * stays a moment, then goes to its winner; then each player draws, the winner first. The trick pile at the lower right:
 * tap it to see the last trick. At the end, each side's counting cards (aces, threes, re, cavalli, fanti) in a row and
 * its points.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards_table.h"
#include "cards_table.h"
#include "games/briscola.h"
#include "shell.h"

#define DRAW_MS 320

static brgame G;
static ct_table T = {.deck = DECK_ITALIAN};
static ct_moves A = {.rest = 200};             /* the draws' rest */
static ct_touch F;
static int floor_n, show_last;
static unsigned seen;
static float ws, stx, sty;                      /* the stock: its cards' width, centre */

static void layout(box b)
{
    ct_layout(&T, b, G.pos.players, ui_bottom(), 5);   /* as for a hand of 5: the three spread out */
    ws = floorf(0.78f * T.wt);
    stx = b.x + 0.35f * T.u + 0.5f * ws * cards_aspect(DECK_ITALIAN);
    sty = T.hand_top - 0.5f * ws * cards_aspect(DECK_ITALIAN) - 0.1f * T.u;
}

static void reset_ui(void)
{
    show_last = 0;
    ct_touch_reset(&F);
}

/* the briscola lying across under the stock: its centre */
static void trump_at(float *x, float *y)
{
    *x = stx + 0.42f * ws;
    *y = sty;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    briscola_new(&G, ui.players, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    int w = briscola_winner(&G.pos);
    return w < 0 ? GS_PLAY : w == 2 ? GS_DRAW : gs_win(w);
}

static unsigned winners(void)                   /* the winning side: both partners; a draw: everyone */
{
    int w = briscola_winner(&G.pos), four = G.pos.players == 4;
    if (w < 0) return 0;
    if (w == 2) return four ? 15u : 3u;
    return four ? 1u << w | 1u << (w + 2) : 1u << w;
}

static int turn(void) { return G.pos.turn; }

static int hand(uint8_t *h, int *how, int live)
{
    const brpos *p = &G.pos;
    int n = p->n[T.me];
    for (int i = 0; i < n; i++) {
        h[i] = p->hand[T.me][i];
        how[i] = h[i] == F.raised ? CD_RAISED : live && p->turn == T.me ? CD_GLOW : 0;
    }
    return n;
}

/* the cards drawn after a trick: from the stock (the last one the briscola, from across under it) to each hand, the
 * winner first, after `delay` ms */
static void draws(const brpos *b, int delay)
{
    const brpos *p = &G.pos;
    for (int k = 0; k < p->ndrew; k++) {
        int q = (p->last_winner + k) % p->players, c = p->drew[k], briscola = c == p->briscola && b->nstock - k == 1;
        float x0 = stx, y0 = sty, a0 = 0;
        if (briscola) trump_at(&x0, &y0), a0 = 90;
        if (q == T.me) {                        /* the viewer's own: it turns up as it lands */
            cards_spot s[CT_HAND];
            ct_fan(&T, p->n[q], s);
            int i = 0;
            while (i < p->n[q] - 1 && p->hand[q][i] != c) i++;
            ct_slide(&A, DECK_ITALIAN, c, x0, y0, ws, a0, briscola, s[i].x, s[i].y, T.wh, s[i].deg, 1, DRAW_MS,
                     delay + 160 * k, CT_HAND_TAG, c, -1, SND_MOVE);
        } else
            ct_slide(&A, DECK_ITALIAN, c, x0, y0, ws, a0, briscola, T.sx[q], T.sy[q], 0.45f * T.wt, T.tdeg[q], 0,
                     DRAW_MS,
                     delay + 160 * k, CT_SEAT, q, -1, SND_MOVE);
    }
}

static void animate(const brpos *b, int card, const uint8_t *h, int n)
{
    const brpos *p = &G.pos;
    ct_play(&A, &T, &F, h, n, card);
    if (p->tricks == b->tricks) return;
    int look = CT_SLIDE_MS + CT_LOOK_MS;
    ct_gather(&A, &T, p->last, NULL, p->players, p->last_leader, p->last_winner, look, CT_SLIDE_MS);
    if (p->ndrew) draws(b, look + 360 + 30 * p->players + 100);
}

static void act(int card)
{
    brpos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 0);
    if (!briscola_play(&G, card)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    animate(&before, card, h, n);
    reset_ui();
    floor_n = G.n;
    if (G.pos.phase == BR_PLAY) ui_handoff(G.pos.turn);
}

static int undo(void)
{
    if (F.raised != CARD_NONE || show_last) { reset_ui(); return 1; }
    if (G.n <= floor_n || !briscola_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = briscola_script(&G, s);
    ct_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&A); }

/* ---- drawing (the trick table with the Italian pack: T.deck) -----------------------------------------------------*/
/* the stock on the briscola lying across; once all is drawn, the trumps' symbol in its place */
static void stock_draw(const brpos *p)
{
    int n = p->nstock, gone = 0;
    for (int i = 0; i < A.n; i++)               /* the cards on their way out still show in the stock */
        if (cards_slide_busy(&A.m[i].s) && (A.m[i].tag == CT_SEAT || A.m[i].tag == CT_HAND_TAG) &&
            ui_ms() < A.m[i].s.t0 + (Uint32)A.m[i].s.delay)
            gone++;
    n += gone;
    float tx, ty;
    trump_at(&tx, &ty);
    if (n >= 1) cards_draw(DECK_ITALIAN, p->briscola, tx, ty, ws, 90, 0);
    else {
        SDL_Texture *t = ct_suit(DECK_ITALIAN, p->trump, (int)(0.7f * ws));
        SDL_SetTextureAlphaMod(t, 120);
        ui_put_c(t, tx, ty, 0.7f * ws, 0.7f * ws);
        SDL_SetTextureAlphaMod(t, 255);
    }
    if (n >= 2) cards_pile(DECK_FRENCH, stx, sty, ws, n - 1, CARD_NONE, 0);
}

/* the end: a row a side, its tokens, its counting cards in a row (aces, threes, re, cavalli, fanti), its points */
static void draw_summary(void)
{
    const brpos *p = &G.pos;
    float x0, pw, y, rh, u = T.u;
    ct_summary(&T, 2, &x0, &pw, &y, &rh);
    rh = fminf(rh, 1.5f * u);
    float d = fminf(0.6f * rh, 0.75f * u);
    const int ts = (int)d;
    int w = briscola_winner(p), four = p->players == 4;
    static const int order[5] = {1, 3, IT_RE, IT_CAVALLO, IT_FANTE};
    for (int s = 0; s < 2; s++, y += rh) {
        float tx = x0 + 0.65f * u;
        if (s == w) ui_mark_gold(tx + (four ? 0.42f * d : 0), y, 2.0f * d);
        for (int k = 0; k < (four ? 2 : 1); k++) ui_put_c(ct_token(s + 2 * k, ts), tx + k * 0.85f * d, y, d, d);
        uint8_t c[20];
        int n = 0;
        for (int r = 0; r < 5; r++)
            for (int i = 0; i < p->ntook[s]; i++)
                if (CARD_RANK(p->took[s][i]) == order[r] && n < 20) c[n++] = p->took[s][i];
        float left = tx + (four ? 1.4f : 0.8f) * d, right = x0 + pw - 1.6f * u;
        float cw = fminf(0.62f * rh / cards_aspect(DECK_ITALIAN), 0.9f * u);
        float step = n > 1 ? fminf(1.05f * cw, (right - left - cw) / (n - 1)) : 0;
        for (int i = 0; i < n; i++) cards_draw(DECK_ITALIAN, c[i], left + cw / 2 + i * step, y, cw, 0, CD_FLAT);
        ui_number(x0 + pw - 0.85f * u, y, 0.5f * rh, p->points[s], 1);
        if (s == 0) ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    const brpos *p = &G.pos;
    int hide = ui.viewer < 0, live = !ui_handing() && !ct_busy(&A) && p->phase == BR_PLAY;
    if (T.me != ui_bottom() && ui_bottom() >= 0) layout(T.b);
    if (!(seen >> T.me & 1) && !ui_handing()) {   /* the deal, as each player first sees it */
        seen |= 1u << T.me;
        uint8_t h[CT_HAND];
        int how[CT_HAND];
        ct_deal(&A, &T, h, hand(h, how, 0));
    }
    ct_draw_seats(&T, p->phase == BR_PLAY ? p->turn : -1);
    ct_draw_pile(&T, p->tricks);
    stock_draw(p);
    ct_draw_trick(&T, &A, p->trick, NULL, p->ntrick, p->leader);
    if (F.raised != CARD_NONE && live) ct_draw_target(&T);
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, live);
    if (hide) ct_hand_backs(&T, n);
    else ct_hand_draw(&T, &A, h, n, how, NULL, F.on ? F.card : CARD_NONE);
    ct_draw(&A);
    if (!hide) ct_touch_draw(&F, &T);
    if (show_last && p->last_winner != BRISCOLA_NONE)
        ct_draw_last(&T, p->last, NULL, p->players, p->last_leader, p->last_winner);
    if (p->phase == BR_OVER && !ct_busy(&A)) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    const brpos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last) { show_last = 0; return; }
    if (ct_busy(&A) || p->phase != BR_PLAY) return;
    if (ct_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    if (F.raised != CARD_NONE && ct_on_target(&T, x, y)) { act(F.raised); return; }
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
    if (r == CT_T_SEND) act(c);
    else if (r == CT_T_TAP && ct_tap_play(&F, c, 1)) act(c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the cards the side has taken: a small pile and their number */
{
    int n = G.pos.ntook[briscola_side(&G.pos, player)];
    float u = ui.unit, cy = b.y + b.h / 2, cw = 0.26f * u, h = fminf(0.42f * u, 0.7f * b.h);
    cards_pile(DECK_FRENCH, b.x + cw / 2, cy, cw, n, CARD_NONE, CD_FLAT);
    ui_number(b.x + cw + 0.12f * u + (n >= 10 ? 0.68f : 0.31f) * h, cy, h, n, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the briscola across under the stock, an ace and a three played on it */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    float w = 0.34f * size;
    memset(rgba, 0, (size_t)size * size * 4);
    cards_paint_turned(rgba, size, DECK_ITALIAN, CARD(IT_CLUBS, 7), 0.4f * size, 0.71f * size, 0.8f * w, 90);
    cards_paint_turned(rgba, size, DECK_ITALIAN, CARD_NONE, 0.27f * size, 0.71f * size, 0.8f * w, 0);
    cards_paint_turned(rgba, size, DECK_ITALIAN, CARD(IT_CUPS, 3), 0.42f * size, 0.38f * size, w, -10);
    cards_paint_turned(rgba, size, DECK_ITALIAN, CARD(IT_COINS, 1), 0.67f * size, 0.42f * size, w, 8);
}

static const game briscola_game = {
    .id = "briscola", .order = 310, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(briscola_game)
