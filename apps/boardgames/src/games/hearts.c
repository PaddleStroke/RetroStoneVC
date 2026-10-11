/*
 * Hearts in RetroStone Board: 3 to 6 players passing the device round, each hand secret (VIEW_HIDDEN; the rules:
 * hearts_rules.c, hearts.h). The table (games/tricks_ui.h): the viewer's hand fanned at the bottom, the players'
 * seats round the middle (their tokens; the points each has taken this round beside them), the trick with each card
 * placed towards its player, the trick pile (tap it: the last trick). Passing: tap three cards (they go to the three
 * places in the middle; tap one there to take it back), then the arrow pointing to the player who gets them. Playing:
 * tap a card (it rises), tap it again, or drag it to the middle. After each round a summary: each player's token, the
 * round's points, the total (a moon: the player who took them all).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/hearts.h"
#include "games/tricks.h"
#include "games/tricks_ui.h"
#include "shell.h"

#define LIFT 0.16f                              /* the cards just received stand this much higher (card widths) */

static hgame G;
static tk_table T;
static tk_anim A;
static tk_gfx X;
static tk_touch F;
static uint8_t sel[3];                          /* the cards chosen to pass, in the middle */
static int nsel, floor_n, show_last;            /* floor_n: undo stops there (the last hand-off) */
static unsigned seen;                           /* the players who have seen this round's deal */

static void layout(box b) { tk_layout(&T, b, G.pos.players, ui_bottom(), G.pos.size); }

static void reset_ui(void)
{
    nsel = 0;
    show_last = 0;
    tk_touch_reset(&F);
}

/* ---- the viewer's hand as shown: the cards chosen to pass are in the middle; how each is drawn ------------------- */
static int chosen(int card)
{
    for (int i = 0; i < nsel; i++)
        if (sel[i] == card) return 1;
    return 0;
}

static int received(int card)                   /* passed to the viewer: shown until they play their first card */
{
    const hpos *p = &G.pos;
    if (p->phase != HP_PLAY || p->dir == PASS_HOLD || p->n[T.me] != p->size) return 0;
    for (int k = 0; k < 3; k++)
        if (p->got[T.me][k] == card) return 1;
    return 0;
}

static int hand(uint8_t *h, int *how, float *lift, int live)
{
    const hpos *p = &G.pos;
    int n = 0, mine = live && p->turn == T.me && p->phase == HP_PLAY;
    for (int i = 0; i < p->n[T.me]; i++) {
        int c = p->hand[T.me][i];
        if (chosen(c)) continue;
        h[n] = (uint8_t)c;
        how[n] = mine ? (hearts_can_play(p, c) ? CD_GLOW : CD_DIM) : 0;
        if (c == F.raised) how[n] = CD_RAISED;
        lift[n] = received(c) ? LIFT : 0;
        n++;
    }
    return n;
}

static void slot_at(int i, float *x, float *y)
{
    *x = T.cx + (i - 1) * 1.12f * T.wt;
    *y = T.cy - 0.22f * T.wt * T.asp;
}

static void pass_button(float *x, float *y, float *d)
{
    *d = 1.15f * T.u;
    *x = T.cx;
    *y = T.cy + 0.36f * T.wt * T.asp + 0.75f * T.u;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    hearts_new(&G, ui.players, (uint32_t)ui_random(1 << 30));
    tk_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    if (G.pos.phase != HP_OVER) return GS_PLAY;
    int w = hearts_winner(&G.pos);
    return w < 0 ? GS_DRAW : gs_win(w);
}

/* the lowest total: one winner, or the players tied for it */
static unsigned winners(void)
{
    const hpos *p = &G.pos;
    if (p->phase != HP_OVER) return 0;
    int low = 1 << 20;
    unsigned m = 0;
    for (int q = 0; q < p->players; q++) low = p->total[q] < low ? p->total[q] : low;
    for (int q = 0; q < p->players; q++) m |= (unsigned)(p->total[q] == low) << q;
    return m;
}

static int turn(void) { return G.pos.turn; }

/* what an action moved, from the position before it (b) and the viewer's hand as it was shown (h, n) */
static void animate(const hpos *b, uint32_t a, const uint8_t *h, int n)
{
    const hpos *p = &G.pos;
    int me = b->turn;
    if (HA_KIND(a) == HA_PASS) {
        int to = hearts_target(b, me);
        for (int i = 0; i < 3; i++) {           /* the three cards to their taker's seat, face down */
            float x, y;
            slot_at(i, &x, &y);
            tk_slide(&A, HA_CARD(a, i), x, y, T.wt, 0, 1, T.sx[to], T.sy[to], 0.45f * T.wt, T.tdeg[to], 0, TK_SLIDE_MS,
                     60 * i, TK_NONE, 0, 0, i == 0 ? SND_MOVE : -1, 150);
        }
        if (p->phase != HP_PLAY) return;
        /* the last to choose: the cards change hands, the viewer's come in from the giver's seat */
        int from = 0, how[TK_HAND];
        for (int q = 0; q < p->players; q++)
            if (hearts_target(p, q) == me) from = q;
        uint8_t nh[TK_HAND];
        float lift[TK_HAND];
        cards_spot ns[TK_HAND];
        int k = hand(nh, how, lift, 0), j = 0;
        tk_fan(&T, k, ns);
        for (int i = 0; i < k; i++)
            if (lift[i] > 0)
                tk_slide(&A, nh[i], T.sx[from], T.sy[from], 0.45f * T.wt, T.tdeg[from], 0, ns[i].x,
                         ns[i].y - LIFT * T.wh, T.wh, ns[i].deg, 1, TK_SLIDE_MS + 120, TK_SLIDE_MS + 200 + 90 * j,
                         TK_HAND_TAG, nh[i], 0, j == 0 ? SND_PLACE : -1, 300), j++;
        return;
    }
    if (HA_KIND(a) != HA_PLAY) return;
    tk_play(&A, &T, &F, h, n, HA_CARD(a, 2));
    if (p->tricks != b->tricks)                 /* the trick is full: its winner gathers it */
        tk_gather(&A, &T, p->last, p->players, p->last_leader, p->last_winner, TK_SLIDE_MS + TK_LOOK_MS, TK_SLIDE_MS);
}

static void act(uint32_t a)
{
    hpos before = G.pos;
    uint8_t h[TK_HAND];
    int how[TK_HAND];
    float lift[TK_HAND];
    int n = hand(h, how, lift, 0);
    if (!hearts_play(&G, a)) { ui_sound(SND_DENY); return; }
    tk_calm(&A);
    animate(&before, a, h, n);
    reset_ui();
    const hpos *p = &G.pos;
    if (HA_KIND(a) == HA_NEXT) seen = 0;
    if (p->turn != before.turn || p->phase == HP_ROUND || p->phase == HP_OVER) floor_n = G.n;
    if (p->phase == HP_PASS || p->phase == HP_PLAY) ui_handoff(p->turn);   /* the device passes (after the cards) */
}

static int undo(void)
{
    if (nsel || F.raised != CARD_NONE || show_last) { reset_ui(); return 1; }   /* a choice not made yet */
    if (G.n <= floor_n || !hearts_undo(&G)) return 0;
    tk_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = hearts_script(&G, s);
    tk_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return tk_busy(&A); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_pass(int live)
{
    const hpos *p = &G.pos;
    for (int i = 0; i < 3; i++) {
        float x, y;
        slot_at(i, &x, &y);
        if (i < nsel) cards_draw(DECK_FRENCH, sel[i], x, y, T.wt, 0, 0);
        else cards_place(DECK_FRENCH, x, y, T.wt);
    }
    int to = hearts_target(p, T.me);
    float bx, by, d;
    pass_button(&bx, &by, &d);
    double deg = atan2(T.sy[to] - by, T.sx[to] - bx) * 180 / TK_PI;
    if (nsel == 3 && live) ui_mark_gold(bx, by, 2.0f * d);
    tk_button(&X, bx, by, d, nsel == 3, TK_ARROW, deg);
}

/* the round's summary: a row a player, their token (the lowest total in gold light), the round's points (a moon:
 * shot), the total */
static void draw_summary(void)
{
    const hpos *p = &G.pos;
    float x0, pw, y, rh, u = T.u;
    tk_summary(&X, &T, p->players, &x0, &pw, &y, &rh);
    int low = 1 << 20;
    for (int q = 0; q < p->players; q++) low = p->total[q] < low ? p->total[q] : low;
    float d = fminf(0.72f * rh, 0.8f * u);
    tk_tokens(&X, (int)d);
    for (int q = 0; q < p->players; q++, y += rh) {
        float tx = x0 + 0.8f * u, nh = 0.42f * rh, rx = x0 + pw * 0.45f;
        if (p->total[q] == low) ui_mark_gold(tx, y, 2.0f * d);
        ui_put_c(X.tok[q], tx, y, d, d);
        ui_number_signed(rx, y, nh, p->score[q], 1, 1);
        if (q == p->moon) tk_moon(&X, rx + tk_number_w(p->score[q], nh) / 2 + 0.8f * nh, y, 1.1f * nh);
        ui_number_signed(x0 + pw * 0.8f, y, 0.56f * rh, p->total[q], 1, 0);
        if (q < p->players - 1) tk_row_line(x0, pw, y + rh / 2, u);
    }
}

static void draw(void)
{
    const hpos *p = &G.pos;
    if (ui.viewer < 0) return;
    int live = !ui_handing() && !tk_busy(&A), ph = p->phase;
    if (!(seen >> T.me & 1) && !ui_handing()) {   /* the viewer's first look at this round's cards */
        seen |= 1u << T.me;
        if (p->tricks == 0 && p->ntrick == 0 && (ph == HP_PASS || p->dir == PASS_HOLD)) {
            uint8_t h[TK_HAND];
            int how[TK_HAND];
            float lift[TK_HAND];
            tk_deal(&A, &T, h, hand(h, how, lift, 0));
        }
    }
    int pass_to = ph == HP_PASS && p->turn == T.me ? hearts_target(p, T.me) : -1;
    tk_draw_seats(&X, &T, ph == HP_PLAY ? p->turn : pass_to);
    for (int q = 0; q < p->players; q++) {     /* the points taken this round */
        if (!p->taken[q] || ph == HP_ROUND || ph == HP_OVER) continue;
        float x, y;
        tk_seat_spot(&T, q, 0, &x, &y);
        ui_number(x, y, 0.5f * T.tok, p->taken[q], 1);
    }
    tk_draw_pile(&T, p->tricks);
    if (ph == HP_PASS && p->turn == T.me) draw_pass(live);
    tk_draw_trick(&T, &A, p->trick, p->ntrick, p->leader);
    if (F.raised != CARD_NONE && live) tk_draw_target(&T);
    uint8_t h[TK_HAND];
    int how[TK_HAND];
    float lift[TK_HAND];
    int n = hand(h, how, lift, live);
    tk_hand_draw(&T, &A, h, n, how, lift, F.on ? F.card : CARD_NONE);
    tk_draw_moves(&A);
    tk_touch_draw(&F, &T);
    if (show_last && p->last_winner != TRICK_NONE)
        tk_draw_last(&T, p->last, p->players, p->last_leader, p->last_winner);
    if ((ph == HP_ROUND || ph == HP_OVER) && !tk_busy(&A)) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* a card sent to the middle (tapped while passing, tapped twice or dragged while playing): chosen, or played */
static void send(int card)
{
    const hpos *p = &G.pos;
    if (p->phase == HP_PASS) {
        if (nsel >= 3) { ui_sound(SND_DENY); return; }
        sel[nsel++] = (uint8_t)card;
        ui_sound(SND_TAP);
        return;
    }
    if (hearts_can_play(p, card)) act(HA_PLAY | (uint32_t)card);
    else { F.raised = CARD_NONE; ui_sound(SND_DENY); }
}

static void press(float x, float y)
{
    const hpos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last) { show_last = 0; return; }
    if (tk_busy(&A)) return;                    /* the cards still moving */
    if (p->phase == HP_ROUND) { act(HA_NEXT); return; }
    if (p->phase == HP_OVER) return;
    if (tk_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    if (p->phase == HP_PASS) {
        for (int i = 0; i < nsel; i++) {        /* a chosen card back to the hand */
            float sx, sy;
            slot_at(i, &sx, &sy);
            if (!cards_in(sx, sy, T.wt, T.asp, x, y)) continue;
            for (int k = i; k < nsel - 1; k++) sel[k] = sel[k + 1];
            nsel--;
            ui_sound(SND_TAP);
            return;
        }
        float bx, by, d;
        pass_button(&bx, &by, &d);
        if (hypotf(x - bx, y - by) <= d * 0.6f) {
            if (nsel == 3) act(HA_PASS3(sel[0], sel[1], sel[2]));
            else ui_sound(SND_DENY);
            return;
        }
    }
    if (F.raised != CARD_NONE && tk_on_target(&T, x, y)) { send(F.raised); return; }
    uint8_t h[TK_HAND];
    int how[TK_HAND];
    float lift[TK_HAND];
    int n = hand(h, how, lift, 1), c = tk_hand_at(&T, h, n, how, lift, x, y);
    if (c == CARD_NONE) {
        if (F.raised != CARD_NONE) F.raised = CARD_NONE;
        else ui_sound(SND_DENY);
        return;
    }
    tk_touch_down(&F, c, x, y);                 /* a tap or a drag: the release says */
}

static void motion(float x, float y) { tk_touch_move(&F, x, y); }

static void release(float x, float y)
{
    int c, r = tk_touch_up(&F, &T, x, y, &c);
    if (r == TK_T_SEND) send(c);
    else if (r == TK_T_TAP) {
        if (G.pos.phase == HP_PASS) send(c);
        else if (tk_tap_play(&F, c, hearts_can_play(&G.pos, c))) send(c);
    }
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { tk_bar(b, G.pos.n[player], G.pos.total[player]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the 2 of clubs, two hearts, and the queen of spades on top */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[4] = {CARD(SUIT_CLUBS, 2), CARD(SUIT_HEARTS, RANK_ACE), CARD(SUIT_HEARTS, RANK_KING),
                                 CARD(SUIT_SPADES, RANK_QUEEN)};
    tk_thumb(cards, 4, size, rgba);
}

static const game hearts_game = {
    .id = "hearts", .order = 285, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6, .view = VIEW_HIDDEN,
};
REGISTER_GAME(hearts_game)
