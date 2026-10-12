/*
 * The climbing (shedding) games on the screen, shared by Big Two and Tiến lên (their rules: games/shed.h and each
 * game's own). Static: a game includes it once, gives it its rules and how the deal is scored, and registers the
 * functions below as its own. VIEW_HIDDEN: each hand is secret, the device passed round.
 * Along the top the others in turn order (as their bars): a small fan of backs each, their chip and count under it, a
 * pass sign once they passed (Tiến lên: out of the trick until the next lead), their place once out. In the middle
 * the trick: the last play big with its player's chip, the two plays before it smaller behind; the tricks over face
 * down at the side. The viewer's hand along the bottom in the game's order, low to high. Tap cards to choose them (the
 * card the first play must hold glows), then tap the middle or drag them there: the middle lights once they make a
 * play that may go; the pass button in the bar passes. At the end, a panel: Big Two the cards each one has left and
 * their penalty, Tiến lên the places. What every card game shares (the others along the top, the summary's panel, the
 * finger dragging the chosen cards, the bars' cards) is the kit's; this keeps the climbing games' own table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef SHED_UI_H
#define SHED_UI_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/shed.h"
#include "shell.h"

#define SHUI_PLAY_MS 300
#define SHUI_DEAL_MS 240
#define SHUI_SWEEP_MS 380
#define SHUI_SHOW_MS 800                        /* a trick's last play stays this long before the trick is swept */

enum { SHUI_PENALTY, SHUI_PLACES };            /* the end's panel: penalty points for cards left, or places */
enum { SHUI_HAND = 1, SHUI_PILE, SHUI_SWEPT, SHUI_SEAT };

static shed_game SG;
static const shed_rules *SR;
static int shui_scoring;
static int (*shui_penalty)(int left);
static ct_moves SM = {.rest = 320};
static int shui_dealing, shui_mark;             /* the deal flies once the view is set; undo's floor */
static uint64_t shui_chosen;                    /* the cards chosen (shed_bit) */
static ct_drag shui_drag;

/* ---- the table's geometry: the viewer at the bottom, the others along the top in turn order -------------------- */
static struct {
    box b;
    int me;
    ct_others O;                                /* the others along the top */
    float pw, cx, cy;                           /* the trick's cards, its centre */
    float dx, dy, dw;                           /* the tricks over */
    float hw, hand_max;                         /* the hand's card width, the room it may take */
    float asp;
} ST;

static void shui_hand(const shed_pos *p, int pl, ct_rows *h)
{
    ct_rows_layout(h, ST.b, p->n[pl], ST.b.w > 1.2f * ST.b.h ? 18 : 13, ST.hw, ST.hand_max);
}

static void shui_deal_slides(void);

static void shui_layout(box b)
{
    const shed_pos *p = &SG.pos;
    ST.b = b;
    ST.me = ui_bottom();
    if (ST.me < 0 || ST.me >= p->players) ST.me = 0;
    ST.asp = cards_aspect(DECK_FRENCH);
    float u = ui.unit, hs = b.h * (b.w > b.h ? 0.22f : 0.17f);
    ct_others_layout(&ST.O, b, DECK_FRENCH, p->players, ST.me, hs);
    ST.hand_max = b.h * 0.3f;
    ST.hw = floorf(fminf(b.w / 4.5f, ST.hand_max / (ST.asp * 1.2f)));
    ct_rows h;
    ct_rows_layout(&h, b, 13, 13, ST.hw, ST.hand_max);
    float top = b.y + hs, bottom = h.top;
    ST.pw = floorf(fminf(fminf(ST.hw * 0.9f, (bottom - top) / (ST.asp * 1.75f)), b.w / 4.6f));
    ST.cx = b.x + b.w / 2 + 0.2f * ST.pw;
    ST.cy = top + (bottom - top) * 0.56f;
    ST.dw = floorf(ST.pw * 0.55f);
    ST.dx = b.x + 0.6f * ST.dw + 0.15f * u;
    ST.dy = top + 0.5f * ST.dw * ST.asp + 0.1f * u;
    if (shui_dealing && ui.viewer >= 0) { shui_dealing = 0; shui_deal_slides(); }
}

/* the k-th card of a play of n, the play `back` places behind the last one (0: the last) */
static void shui_play_spot(int k, int n, int back, float *x, float *y, float *w)
{
    float s = back ? 0.62f : 1, pw = ST.pw * s, step = (n > 3 ? 0.42f : 0.5f) * pw;
    float cx = ST.cx - back * 0.55f * ST.pw, cy = ST.cy - back * 0.5f * ST.pw;
    *x = cx + (k - (n - 1) / 2.0f) * step;
    *y = cy;
    *w = pw;
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
static int shui_busy(void) { return ct_busy(&SM); }

/* the deal: one card at a time round the table, from the middle */
static void shui_deal_slides(void)
{
    const shed_pos *p = &SG.pos;
    ct_rows h;
    shui_hand(p, ST.me, &h);
    int most = 0, k = 0, step = 40;
    for (int q = 0; q < p->players; q++) most = p->dealt[q] > most ? p->dealt[q] : most;
    uint8_t mine[SHED_HAND];                    /* the viewer's cards in the order dealt: as sorted, mixed */
    memcpy(mine, p->hand[ST.me], p->n[ST.me]);
    for (int c = 0; c < most; c++)
        for (int q = 0; q < p->players; q++) {
            if (c >= p->dealt[q]) continue;
            if (q == ST.me) {
                int i = (int)((unsigned)(c * 7 + 3) % (unsigned)p->n[q]);   /* a card of the hand, each once */
                while (ct_hides(&SM, SHUI_HAND, mine[i], -1)) i = (i + 1) % p->n[q];
                ct_slide(&SM, DECK_FRENCH, mine[i], ST.cx, ST.cy, ST.dw, 0, 0, h.spot[i].x, h.spot[i].y, h.w,
                         h.spot[i].deg, 1, SHUI_DEAL_MS + 80, k * step, SHUI_HAND, mine[i], 0, k % 3 ? -1 : SND_MOVE);
            } else {
                float x, y, a;
                ct_others_end(&ST.O, q, c + 1, &x, &y, &a);
                ct_slide(&SM, DECK_FRENCH, CARD_NONE, ST.cx, ST.cy, ST.dw, 0, 0, x, y, ST.O.sw, a, 0, SHUI_DEAL_MS,
                         k * step, SHUI_SEAT, q, 0, k % 3 ? -1 : SND_MOVE);
            }
            k++;
        }
}

/* the trick's cards swept onto the pile of the tricks over, after `go` ms; the last play shows from last_show */
static void shui_sweep(const shed_pos *b, int go, int last_show)
{
    int k = 0, start = 0;
    for (int pl = 0; pl < b->nplays; pl++) {
        int cnt = b->plays[pl], back = b->nplays - 1 - pl;
        for (int j = 0; j < cnt; j++, k++) {
            float x, y, w;
            shui_play_spot(j, cnt, back > 2 ? 2 : back, &x, &y, &w);
            int m = ct_slide(&SM, DECK_FRENCH, b->pile[start + j], x, y, w, 0, 1, ST.dx, ST.dy, ST.dw, 0, 0,
                             SHUI_SWEEP_MS, go + k * 25, SHUI_SWEPT, 0, 0, k == 0 ? SND_MOVE : -1);
            ct_show(&SM, m, back > 2 ? 100000 : back ? 0 : last_show);   /* the plays far behind: not seen going */
        }
        start += cnt;
    }
}

static void shui_animate(const shed_pos *b, uint64_t a)
{
    const shed_pos *p = &SG.pos;
    int pl = b->turn;
    if (a == SHED_PASS) {
        if (p->tricks != b->tricks) shui_sweep(b, 150, 0);   /* the last pass: the trick goes */
        return;
    }
    uint8_t cards[13];
    int cnt = shed_cards(SR, a, cards);
    ct_rows h;
    shui_hand(b, pl, &h);
    for (int k = 0; k < cnt; k++) {
        int i = shed_find(b, pl, cards[k]);
        float x, y, w, x0 = h.spot[i].x, y0 = h.spot[i].y;
        shui_play_spot(k, cnt, 0, &x, &y, &w);
        if (ct_dragging(&shui_drag)) x0 = shui_drag.x + (k - (cnt - 1) / 2.0f) * 0.35f * h.w, y0 = shui_drag.y;
        ct_slide(&SM, DECK_FRENCH, cards[k], x0, y0, h.w, h.spot[i].deg, 1, x, y, w, 0, 1, SHUI_PLAY_MS, k * 45,
                 SHUI_PILE, k, 0, k == 0 ? SND_PLACE : -1);
    }
    if (p->tricks != b->tricks) {               /* nobody left to beat it: shown, then swept */
        shed_pos t = *b;
        for (int k = 0; k < cnt; k++) t.pile[t.npile++] = cards[k];
        t.plays[t.nplays++] = (uint8_t)cnt;
        shui_sweep(&t, SHUI_PLAY_MS + SHUI_SHOW_MS, SHUI_PLAY_MS + 45 * cnt);
    }
}

static void shui_calm(void)
{
    ct_calm(&SM);
    shui_drag.on = 0;
    shui_chosen = 0;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void shui_start(const shed_rules *R, int scoring, int (*penalty)(int))
{
    SR = R;
    shui_scoring = scoring;
    shui_penalty = penalty;
    shed_new(&SG, R, ui.players, (uint32_t)ui_random(1 << 30));
    shui_calm();
    shui_dealing = 1;
    shui_mark = 0;
}

static unsigned shui_winners(void) { return SG.pos.phase == SP_OVER ? 1u << SG.pos.out[0] : 0; }
static int shui_status(void) { return SG.pos.phase == SP_OVER ? gs_win(SG.pos.out[0]) : GS_PLAY; }
static int shui_turn(void) { return SG.pos.turn; }

static void shui_act(uint64_t a)
{
    shed_pos before = SG.pos;
    if (!shed_play(&SG, SR, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&SM);
    shui_animate(&before, a);
    shui_drag.on = 0;
    shui_chosen = 0;
    if (a == SHED_PASS) ui_sound(SND_TAP);
    if (SG.pos.turn != before.turn) shui_mark = SG.n;
    if (SG.pos.phase == SP_PLAY) ui_handoff(SG.pos.turn);
}

static int shui_undo(void)
{
    if (shui_chosen) { shui_chosen = 0; return 1; }   /* a choice not made yet */
    if (SG.n <= shui_mark || !shed_undo(&SG, SR)) return 0;
    shui_calm();
    shui_dealing = 0;
    ui_handoff(SG.pos.turn);
    return 1;
}

static int shui_script(const char *s)
{
    int ok = shed_script(&SG, SR, s);
    shui_calm();
    shui_dealing = 0;
    shui_mark = SG.n;
    ui_handoff(SG.pos.turn);
    return ok;
}

static int shui_can_pass(void)
{
    return !ui_handing() && !shui_busy() && SG.pos.turn == ST.me && shed_legal(SR, &SG.pos, SHED_PASS);
}
static void shui_pass(void) { shui_act(SHED_PASS); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void shui_draw_others(void)
{
    const shed_pos *p = &SG.pos;
    for (int q = 0; q < p->players; q++) {
        if (q == ST.me) continue;
        int n = p->n[q] - ct_hides(&SM, SHUI_SEAT, q, -1), place = shed_place(p, q);
        float x, y, d;
        ct_others_draw(&ST.O, q, n, q == p->turn && p->phase == SP_PLAY);
        if (place >= 0 && (p->phase == SP_PLAY || shui_scoring == SHUI_PLACES))   /* out: the place */
            ct_badge(ct_others_x(&ST.O, q), ST.O.sy, 1.1f * ST.O.sw, place + 1);
        ct_others_badge(&ST.O, q, n, 1, &x, &y, &d);
        if (p->phase == SP_PLAY && p->passed >> q & 1 && place < 0) ct_pass_badge(x, y, d);
    }
}

/* the trick: the plays before the last smaller behind it, the last big with its player's chip */
static void shui_draw_trick(int glow)
{
    const shed_pos *p = &SG.pos;
    int moving = ct_hides(&SM, SHUI_PILE, -1, -1);
    if (glow) ui_mark_gold(ST.cx, ST.cy, 2.6f * ST.pw);   /* under the cards: tap here to play */
    for (int pl = 0, at = 0; pl < p->nplays; pl++) {
        int cnt = p->plays[pl], back = p->nplays - 1 - pl;
        if (back <= 2)
            for (int j = 0; j < cnt; j++) {
                if (!back && moving && ct_hides(&SM, SHUI_PILE, j, -1)) continue;
                float x, y, w;
                shui_play_spot(j, cnt, back, &x, &y, &w);
                cards_draw(DECK_FRENCH, p->pile[at + j], x, y, w, 0, back ? CD_DIM : 0);
            }
        at += cnt;
    }
    if (p->nplays && !moving) {
        int cnt = p->plays[p->nplays - 1];
        float x, y, w, tok = fminf(0.62f * ui.unit, 0.42f * ST.pw);
        shui_play_spot(cnt - 1, cnt, 0, &x, &y, &w);
        ui_put_c(ct_token(p->who[p->nplays - 1], (int)tok), x + 0.5f * w + 0.75f * tok, y, tok, tok);
    }
    if (!p->nplays && !moving) cards_place(DECK_FRENCH, ST.cx, ST.cy, ST.pw);
}

/* the end: a row a player, in the order they went out: their chip (the winner in gold), then the cards left and the
 * penalty (Big Two) or the place (Tiến lên) */
static void shui_draw_summary(void)
{
    const shed_pos *p = &SG.pos;
    float u = ui.unit, x0, pw, y0, rh;
    ct_summary_in(ST.b, p->players, &x0, &pw, &y0, &rh);
    for (int i = 0; i < p->nout; i++) {
        int q = p->out[i];
        float cy = y0 + i * rh, d = fminf(0.72f * rh, 0.8f * u), tx = x0 + 0.75f * u;
        float nh = 0.42f * rh;
        if (i == 0) ui_mark_gold(tx, cy, 2.0f * d);
        ui_put_c(ct_token(q, (int)d), tx, cy, d, d);
        if (shui_scoring == SHUI_PLACES) {
            ct_badge(x0 + pw * 0.62f, cy, 0.8f * rh, i + 1);
        } else {
            int left = p->n[q];
            cards_draw(DECK_FRENCH, CARD_NONE, x0 + pw * 0.4f, cy, 0.62f * nh, 0, CD_DOWN | CD_FLAT);
            ui_number(x0 + pw * 0.4f + 0.6f * nh + ct_number_w(left, 0.8f * nh, 0) / 2, cy, 0.8f * nh, left, 1);
            ui_number_signed(x0 + pw * 0.78f, cy, 0.56f * rh, -shui_penalty(left), 1, 0);
        }
        if (i < p->nout - 1) ct_rule(x0 + 0.3f * u, cy + rh / 2, pw - 0.6f * u);
    }
}

static int shui_ready(void)
{
    return shui_chosen && shed_legal(SR, &SG.pos, shui_chosen);
}

static void shui_draw(void)
{
    const shed_pos *p = &SG.pos;
    if (shui_dealing && ui.viewer >= 0) { shui_dealing = 0; shui_deal_slides(); }
    int mine = p->turn == ST.me && !ui_handing() && !shui_busy() && p->phase == SP_PLAY, hide = ui.viewer < 0;
    ct_rows h;
    shui_hand(p, ST.me, &h);
    shui_draw_others();
    int gone = p->played - ct_hides(&SM, SHUI_SWEPT, -1, -1);
    if (gone > 0) cards_pile(DECK_FRENCH, ST.dx, ST.dy, ST.dw, gone > 12 ? 4 : (gone + 2) / 3, CARD_NONE, 0);
    shui_draw_trick(mine && shui_ready());
    for (int i = 0; i < p->n[ST.me]; i++) {
        int c = p->hand[ST.me][i];
        if (ct_hides(&SM, SHUI_HAND, c, -1)) continue;
        if (hide) {
            cards_draw(DECK_FRENCH, CARD_NONE, h.spot[i].x, h.spot[i].y, h.w, h.spot[i].deg, CD_DOWN);
            continue;
        }
        int on = (shui_chosen & shed_bit(c)) != 0;
        if (ct_dragging(&shui_drag) && on) continue;
        int how = on ? CD_RAISED : mine && c == p->must ? CD_GLOW : 0;
        cards_draw(DECK_FRENCH, c, h.spot[i].x, h.spot[i].y, h.w, h.spot[i].deg, how);
    }
    ct_draw(&SM);
    if (ct_dragging(&shui_drag) && !hide) {
        int k = 0, n = 0;
        for (uint64_t m = shui_chosen; m; m &= m - 1) n++;
        for (int i = 0; i < p->n[ST.me]; i++)
            if (shui_chosen & shed_bit(p->hand[ST.me][i]))
                cards_draw(DECK_FRENCH, p->hand[ST.me][i], shui_drag.x + (k++ - (n - 1) / 2.0f) * 0.35f * h.w,
                           shui_drag.y, h.w, 0, CD_RAISED);
    }
    if (p->phase == SP_OVER && !shui_busy()) shui_draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void shui_press(float x, float y)
{
    const shed_pos *p = &SG.pos;
    shui_drag.on = 0;
    if (shui_busy() || p->phase != SP_PLAY) return;
    if (p->turn != ST.me) { ui_sound(SND_DENY); return; }
    ct_rows h;
    shui_hand(p, ST.me, &h);
    int i = ct_rows_hit(&h, x, y);
    if (i < 0) {                                /* the middle: play the chosen cards */
        if (y < h.top && shui_ready()) shui_act(shui_chosen);
        else if (y < h.top) ui_sound(SND_DENY);
        return;
    }
    uint64_t bit = shed_bit(p->hand[ST.me][i]);
    int was = (shui_chosen & bit) != 0;
    if (!was) {
        shui_chosen |= bit;
        ui_sound(SND_TAP);
    }
    ct_drag_start(&shui_drag, was ? -1 : 0, CARD_NONE, x, y);   /* a chosen card tapped: left when released, not dragged */
}

static void shui_motion(float x, float y) { ct_drag_move(&shui_drag, x, y, 0.12f * ST.hw); }

static void shui_release(float x, float y)
{
    if (!shui_drag.on) return;
    const shed_pos *p = &SG.pos;
    ct_rows h;
    shui_hand(p, ST.me, &h);
    shui_drag.x = x, shui_drag.y = y;
    if (shui_drag.moved > 0 && y < h.top) {
        if (shui_ready()) shui_act(shui_chosen);
        else ui_sound(SND_DENY);
    } else if (shui_drag.moved < 0) {           /* a chosen card tapped again: left */
        int i = ct_rows_hit(&h, x, y);
        if (i >= 0 && shui_chosen & shed_bit(p->hand[ST.me][i])) shui_chosen &= ~shed_bit(p->hand[ST.me][i]),
                                                                  ui_sound(SND_TAP);
    }
    shui_drag.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* a player's bar: a small back and the cards in hand; once out, their place */
static void shui_bar(int player, box b)
{
    const shed_pos *p = &SG.pos;
    float u = ui.unit, cy = b.y + b.h / 2;
    int place = shed_place(p, player);
    if (place >= 0 && (p->phase == SP_PLAY || shui_scoring == SHUI_PLACES)) {
        float d = fminf(0.62f * u, b.h * 0.8f);
        ct_badge(b.x + d / 2, cy, d, place + 1);
        return;
    }
    ct_bar_cards(b, b.x, DECK_FRENCH, p->n[player]);
}

static void shui_token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

#endif
