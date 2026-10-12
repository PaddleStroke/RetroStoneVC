/*
 * Président in RetroStone Board: the climbing card game for 3 to 6, the device passed round (the rules:
 * president_rules.c, president.h). VIEW_HIDDEN: each hand is secret, the screen covered between turns. Along the top
 * of the table the others in turn order (as their bars): their chip, their cards face down and how many, a pass mark
 * when they passed, their new rank's sign once they are out. In the middle the trick: the last play big, with its
 * player's chip, the plays before it smaller behind; the cards of the tricks over go face down on a pile at the side.
 * The viewer's hand along the bottom, from the 3s up to the 2s. Tap cards to choose them (following, one tap chooses
 * as many of that rank as the play needs), then tap the middle, or drag them there, to play; the pass button in your
 * bar passes. The exchange: the Président (then the Vice-président) chooses the cards to give back and taps the
 * places by the receiver's chip; the cards received glow. The bars: each one's rank (a crown, a small crown, nothing,
 * a down arrow, two down arrows) and total. After a deal, the places, the new ranks and the points; tap for the next.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/president.h"
#include "shell.h"

#define PLAY_MS 300
#define DEAL_MS 260
#define SWEEP_MS 380
#define SHOW_MS 800                             /* a trick's last play stays this long before the trick is swept */

enum { TAG_HAND = 1, TAG_PILE, TAG_SWEPT, TAG_GIVE };

static pgame G;
static ct_moves M = {.rest = 320};              /* the cards on the move: a moment after the last lands */
static int dealing, mark;                       /* dealing: the deal flies when the view is set; mark: the history's
                                                 * length at the last hand-off (undo stops there) */
static unsigned chosen;                         /* the cards chosen: bit i, the i-th card of the sorted hand */
static struct { int on, moved; float x0, y0, x, y; } drag;

/* ---- the table's geometry (the viewer at the bottom; the others along the top, in turn order) -------------------- */
static struct {
    box b;
    int me, others;
    float cell_w, strip_h, strip_y, sbw, tok;   /* the others' cells */
    float pw, cx, cy;                           /* the trick's cards, its centre */
    float dx, dy, dw;                           /* the pile of the tricks over */
    float hw, hand_max;                         /* the hand's card width, the room it may take */
} T;

static void deal_slides(void);

static int seat(int q) { return (q - T.me - 1 + 2 * G.pos.players) % G.pos.players; }   /* the others: 0 .. k-1 */
static float cell_x(int q) { return T.b.x + (seat(q) + 0.5f) * T.cell_w; }

static void layout(box b)
{
    const ppos *p = &G.pos;
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= p->players) T.me = 0;
    T.others = p->players - 1;
    float u = ui.unit, asp = cards_aspect(DECK_FRENCH);
    T.cell_w = b.w / T.others;
    T.strip_h = floorf(fminf(1.7f * u, b.h * 0.2f));
    T.strip_y = b.y + T.strip_h / 2;
    T.sbw = floorf(fminf(0.6f * T.strip_h / asp * 1.0f, T.cell_w * 0.3f));
    T.tok = floorf(fminf(0.5f * u, T.cell_w * 0.24f));
    T.hand_max = b.h * 0.36f;
    T.hw = floorf(fminf(b.w / 5.0f, T.hand_max / (asp * 1.2f)));
    ct_rows h;
    ct_rows_layout(&h, b, 13, 13, T.hw, T.hand_max);
    float top = b.y + T.strip_h, bottom = h.top;
    T.pw = floorf(fminf(T.hw * 0.85f, (bottom - top) / (asp * 1.9f)));
    T.cx = b.x + b.w / 2 + 0.25f * T.pw;
    T.cy = top + (bottom - top) * 0.55f;
    T.dw = floorf(T.pw * 0.55f);
    T.dx = b.x + 0.75f * T.dw + 0.1f * u;
    T.dy = T.cy;
    if (dealing && ui.viewer >= 0) { dealing = 0; deal_slides(); }   /* the cover lifted: the deal flies */
}

/* ---- the viewer's hand: from the 3s up to the 2s -------------------------------------------------------------- */
static int sorted_hand(const ppos *p, int pl, uint8_t *out)
{
    int n = p->n[pl];
    memcpy(out, p->hand[pl], (size_t)n);
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--) {
            int a = out[j], b = out[j - 1];
            int ka = pres_strength(CARD_RANK(a)) * 4 + CARD_SUIT(a), kb = pres_strength(CARD_RANK(b)) * 4 + CARD_SUIT(b);
            if (ka >= kb) break;
            out[j] = (uint8_t)b, out[j - 1] = (uint8_t)a;
        }
    return n;
}

static void hand_of(const ppos *p, int pl, uint8_t *cards, int *n, ct_rows *h)
{
    *n = sorted_hand(p, pl, cards);
    ct_rows_layout(h, T.b, *n, T.b.w > 1.2f * T.b.h ? 18 : 12, T.hw, T.hand_max);   /* landscape: one row */
}

/* the chosen cards: one rank and a mask of suits (0: none, or not one rank) */
static int chosen_play(const uint8_t *cards, int n, int *rank)
{
    int m = 0, r = -1;
    for (int i = 0; i < n; i++) {
        if (!(chosen >> i & 1)) continue;
        if (r >= 0 && CARD_RANK(cards[i]) != r) return 0;
        r = CARD_RANK(cards[i]);
        m |= 1 << CARD_SUIT(cards[i]);
    }
    *rank = r;
    return m;
}

static int count_bits(unsigned m)
{
    int c = 0;
    for (; m; m &= m - 1) c++;
    return c;
}

/* ---- the trick's places ------------------------------------------------------------------------------------------ */
/* the k-th card of a play of n, the play `back` places behind the last one (0: the last) */
static void play_spot(int k, int n, int back, float *x, float *y, float *w)
{
    float s = back ? 0.62f : 1, pw = T.pw * s, step = 0.5f * pw;
    float cx = T.cx - back * 0.62f * T.pw, cy = T.cy - back * 0.5f * T.pw;
    *x = cx + (k - (n - 1) / 2.0f) * step;
    *y = cy;
    *w = pw;
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
static void deal_slides(void)
{
    const ppos *p = &G.pos;
    uint8_t cards[PRES_HAND];
    int n;
    ct_rows h;
    hand_of(p, T.me, cards, &n, &h);
    for (int i = 0; i < n; i++)
        ct_slide(&M, DECK_FRENCH, cards[i], T.cx, T.cy, T.pw, 0, 0, h.spot[i].x, h.spot[i].y, h.w, h.spot[i].deg, 1,
                 DEAL_MS, i * 55, TAG_HAND, cards[i], 0, i % 3 == 0 ? SND_MOVE : -1);
}

/* the trick's cards swept onto the pile of the tricks over, after `go` ms */
static void sweep(const ppos *b, int go, int last_show)
{
    int k = 0, start = 0;
    for (int pl = 0; pl < b->nplays; pl++) {
        int cnt = b->plays[pl], back = b->nplays - 1 - pl;
        for (int j = 0; j < cnt; j++, k++) {
            float x, y, w;
            play_spot(j, cnt, back > 2 ? 2 : back, &x, &y, &w);
            int m = ct_slide(&M, DECK_FRENCH, b->pile[start + j], x, y, w, 0, 1, T.dx, T.dy, T.dw, 0, 0, SWEEP_MS,
                             go + k * 25, TAG_SWEPT, 0, 0, k == 0 ? SND_MOVE : -1);
            ct_show(&M, m, back > 2 ? 100000 : back ? 0 : last_show);   /* the plays behind: not seen going */
        }
        start += cnt;
    }
}

static void animate(const ppos *b, int a)
{
    const ppos *p = &G.pos;
    int pl = b->turn;
    switch (PA_KIND(a)) {
    case PA_PLAY: {
        uint8_t cards[PRES_HAND];
        int n, k = 0, cnt = count_bits((unsigned)PA_MASK(a));
        ct_rows h;
        hand_of(b, pl, cards, &n, &h);
        for (int i = 0; i < n; i++) {
            int c = cards[i];
            if (CARD_RANK(c) != PA_RANK(a) || !(PA_MASK(a) >> CARD_SUIT(c) & 1)) continue;
            float x, y, w;
            play_spot(k, cnt, 0, &x, &y, &w);
            float x0 = h.spot[i].x, y0 = h.spot[i].y;
            if (drag.on && drag.moved) x0 = drag.x + (k - (cnt - 1) / 2.0f) * 0.4f * h.w, y0 = drag.y;
            ct_slide(&M, DECK_FRENCH, c, x0, y0, h.w, h.spot[i].deg, 1, x, y, w, 0, 1, PLAY_MS, k * 50, TAG_PILE, k, 0,
                     k == 0 ? SND_PLACE : -1);
            k++;
        }
        if (p->npile == 0) {                    /* the trick ended at once (a 2, or the deal): the play shown, swept */
            ppos t = *b;
            for (int s = 0; s < 4; s++)
                if (PA_MASK(a) >> s & 1) t.pile[t.npile++] = (uint8_t)CARD(s, PA_RANK(a));
            t.plays[t.nplays++] = (uint8_t)cnt;
            sweep(&t, PLAY_MS + SHOW_MS, PLAY_MS + 50 * cnt);
        }
        break;
    }
    case PA_PASS:
        if (p->npile == 0 && b->npile > 0) sweep(b, 120, 0);   /* the last pass: the trick goes */
        break;
    case PA_GIVE: {
        uint8_t cards[PRES_HAND];
        int n;
        ct_rows h;
        hand_of(b, pl, cards, &n, &h);
        for (int i = 0; i < n; i++)
            if (cards[i] == (a & 0xff))
                ct_slide(&M, DECK_FRENCH, cards[i], h.spot[i].x, h.spot[i].y, h.w, h.spot[i].deg, 1,
                         cell_x(b->receiver), T.strip_y, T.sbw, 0, 0, PLAY_MS + 80, 0, TAG_GIVE, 0, 0, SND_MOVE);
        break;
    }
    }
}

static void calm(void)
{
    ct_calm(&M);
    drag.on = 0;
    chosen = 0;
}

static int busy(void) { return ct_busy(&M); }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    pres_new(&G, ui.players, (uint32_t)ui_random(1 << 30));
    calm();
    dealing = 1;
    mark = 0;
}

static unsigned winners(void) { return pres_winners(&G.pos); }

static int status(void)
{
    unsigned w = pres_winners(&G.pos);
    if (!w) return GS_PLAY;
    if (w & (w - 1)) return GS_DRAW;
    int q = 0;
    while (!(w >> q & 1)) q++;
    return gs_win(q);
}

static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    ppos before = G.pos;
    if (!pres_play(&G, a)) { ui_sound(SND_DENY); return; }
    animate(&before, a);
    drag.on = 0;
    chosen = 0;
    const ppos *p = &G.pos;
    int fresh = PA_KIND(a) == PA_NEXT;
    if (fresh) dealing = 1;
    if (p->turn != before.turn || fresh) mark = G.n;
    if (p->phase == PP_GIVE || p->phase == PP_PLAY) ui_handoff(p->turn);
}

/* several actions as one move (the cards given back): the history mark after the last */
static int undo(void)
{
    if (G.n <= mark || !pres_undo(&G)) return 0;
    calm();
    dealing = 0;
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = pres_script(&G, s);
    calm();
    dealing = 0;
    mark = G.n;
    ui_handoff(G.pos.turn);
    return ok;
}

static int can_pass(void) { return !ui_handing() && !busy() && pres_legal(&G.pos, PA_PASS); }
static void pass(void) { act(PA_PASS); }

/* ---- the ranks' signs: a crown, a small crown, a down arrow, two down arrows ------------------------------------- */
static SDL_Texture *t_role[PR_TROU + 1];
static int role_size;

static void paint_role(int role, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    if (role == PR_NONE || role == PR_NEUTRAL) return;
    art_fig *f = art_fig_new();
    int crown = role == PR_PRES || role == PR_VICE;
    art_style gold = {{232, 184, 70}, {70, 44, 14}, {150, 104, 30}, {196, 52, 44}, 0.35f, 0.045f};
    art_style red = {{196, 70, 56}, {60, 16, 12}, {120, 36, 28}, {0, 0, 0}, 0.3f, 0.045f};
    if (crown) {
        float k = role == PR_PRES ? 1.0f : 0.72f, ox = 0.5f - 0.5f * k, oy = role == PR_PRES ? 0 : 0.16f;
        const float c[] = {0.12f, 0.78f, 0.08f, 0.30f, 0.30f, 0.50f, 0.50f, 0.20f, 0.70f, 0.50f, 0.92f, 0.30f,
                           0.88f, 0.78f};
        float pts[14];
        for (int i = 0; i < 7; i++) pts[2 * i] = ox + c[2 * i] * k, pts[2 * i + 1] = oy + c[2 * i + 1] * k;
        art_poly(f, ART_PART, pts, 7);
        art_rect(f, ART_PART, ox + 0.12f * k, oy + 0.70f * k, ox + 0.88f * k, oy + 0.84f * k);
        art_ellipse(f, ART_PART, ox + 0.08f * k, oy + 0.27f * k, 0.07f * k, 0.07f * k);
        art_ellipse(f, ART_PART, ox + 0.50f * k, oy + 0.16f * k, 0.07f * k, 0.07f * k);
        art_ellipse(f, ART_PART, ox + 0.92f * k, oy + 0.27f * k, 0.07f * k, 0.07f * k);
        art_ellipse(f, ART_MARK, ox + 0.50f * k, oy + 0.77f * k, 0.05f * k, 0.05f * k);
    } else if (role == PR_VICETROU) {
        const float head[] = {0.18f, 0.46f, 0.82f, 0.46f, 0.50f, 0.86f};
        art_rect(f, ART_PART, 0.39f, 0.12f, 0.61f, 0.50f);
        art_poly(f, ART_PART, head, 3);
    } else {
        const float h1[] = {0.18f, 0.12f, 0.36f, 0.12f, 0.50f, 0.30f, 0.64f, 0.12f, 0.82f, 0.12f, 0.50f, 0.50f},
                    h2[] = {0.18f, 0.46f, 0.36f, 0.46f, 0.50f, 0.64f, 0.64f, 0.46f, 0.82f, 0.46f, 0.50f, 0.84f};
        art_poly(f, ART_PART, h1, 6);
        art_poly(f, ART_PART, h2, 6);
    }
    art_render(f, crown ? &gold : &red, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

static void draw_role(int role, float cx, float cy, float d)
{
    int size = (int)fmaxf(8, d);
    if (role_size != size) {
        role_size = size;
        for (int r = 0; r <= PR_TROU; r++) {
            ui_free(&t_role[r]);
            paint_role(r, size, ui_buf(size, size));
            t_role[r] = ui_tex(size, size, ui_buf(size, size));
        }
    }
    if (role > PR_NONE && role <= PR_TROU && role != PR_NEUTRAL) ui_put_c(t_role[role], cx, cy, d, d);
}

/* the rank a place gives (president_rules.c's deal_end) */
static int role_of_place(int np, int i)
{
    return i == 0 ? PR_PRES : i == np - 1 ? PR_TROU : np >= 4 && i == 1 ? PR_VICE : np >= 4 && i == np - 2 ? PR_VICETROU
                                                                                                     : PR_NEUTRAL;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* the others along the top: chip, cards face down and how many, a pass mark, or their new rank once out */
static void draw_others(void)
{
    const ppos *p = &G.pos;
    float u = ui.unit, asp = cards_aspect(DECK_FRENCH);
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) continue;
        float x = cell_x(q), y = T.strip_y, tx = x - 0.32f * T.cell_w;
        tx = fmaxf(tx, x - T.sbw * 1.2f - T.tok);
        if (q == p->turn && (p->phase == PP_PLAY || p->phase == PP_GIVE)) ui_mark_gold(tx, y, 2.1f * T.tok);
        ui_put_c(ct_token(q, (int)T.tok), tx, y, T.tok, T.tok);
        int n = p->n[q] - (p->phase == PP_GIVE && q == p->receiver ? ct_hides(&M, TAG_GIVE, -1, -1) : 0);
        float px = x;
        int place = pres_place(p, q);
        if (place >= 0 && p->phase == PP_PLAY) {
            draw_role(role_of_place(p->players, place), px, y, 0.8f * T.sbw * asp);
            if (role_of_place(p->players, place) == PR_NEUTRAL) ui_number(px, y, 0.4f * T.sbw * asp, place + 1, 1);
        } else if (n > 0) {             /* their cards, how many; a pass: dimmed, the pass sign on them */
            int passed = p->phase == PP_PLAY && p->passed >> q & 1;
            cards_pile(DECK_FRENCH, px, y, T.sbw, n > 3 ? 3 : n, CARD_NONE, passed ? CD_DIM : 0);
            if (passed) {
                float d = fminf(0.9f * T.sbw, 0.5f * u);
                ui_put_c(ct_icon(ICON_PASS, (int)d, 246, 238, 220), px, y, d, d);
            }
            float nh = fminf(0.42f * u, 0.45f * T.sbw * asp), nw = ct_number_w(n, floorf(nh + 0.5f), 0);
            ui_number(px + 0.62f * T.sbw + 0.08f * u + nw / 2, y, nh, n, 1);
        }
    }
}

/* the trick: the plays before the last smaller behind it, the last big with its player's chip */
static void draw_trick(int glow)
{
    const ppos *p = &G.pos;
    int moving = ct_hides(&M, TAG_PILE, -1, -1);
    if (glow) ui_mark_gold(T.cx, T.cy, 2.4f * T.pw);   /* under the cards: tap here to play */
    for (int pl = 0, at = 0; pl < p->nplays; pl++) {
        int cnt = p->plays[pl], back = p->nplays - 1 - pl;
        if (back <= 2)
            for (int j = 0; j < cnt; j++) {
                if (!back && moving && ct_hides(&M, TAG_PILE, j, -1)) continue;
                float x, y, w;
                play_spot(j, cnt, back, &x, &y, &w);
                cards_draw(DECK_FRENCH, p->pile[at + j], x, y, w, 0, back ? CD_DIM : 0);
            }
        at += cnt;
    }
    if (p->top_count && p->top_player != PRES_NONE) {
        float x, y, w;
        play_spot(0, p->top_count, 0, &x, &y, &w);
        float tx = T.cx + (p->top_count - 1) * 0.25f * w + 0.5f * w + 0.75f * T.tok;
        ui_put_c(ct_token(p->top_player, (int)T.tok), tx, y, T.tok, T.tok);
    }
    if (!p->top_count && !moving) cards_place(DECK_FRENCH, T.cx, T.cy, T.pw);
}

/* the tricks over: a pile face down at the side */
static void draw_discards(void)
{
    const ppos *p = &G.pos;
    int held = 0;
    for (int q = 0; q < p->players; q++) held += p->n[q];
    int n = 52 - held - p->npile - ct_hides(&M, TAG_SWEPT, -1, -1);
    if (n > 0) cards_pile(DECK_FRENCH, T.dx, T.dy, T.dw, n, CARD_NONE, 0);
}

/* the exchange: the receiver's chip and the places for the cards to give back */
static void draw_exchange(int ready)
{
    const ppos *p = &G.pos;
    float w = T.pw * 0.8f, tx = T.cx - (p->give_left + 1) * 0.6f * w;
    ui_put_c(ct_token(p->receiver, (int)(1.3f * T.tok)), tx, T.cy, 1.3f * T.tok, 1.3f * T.tok);
    draw_role(p->role[p->receiver], tx, T.cy - 1.1f * T.tok, 1.2f * T.tok);
    if (ready) ui_mark_gold(T.cx, T.cy, 2.2f * T.pw);
    for (int i = 0; i < p->give_left; i++) cards_place(DECK_FRENCH, T.cx + (i - (p->give_left - 1) / 2.0f) * 1.1f * w,
                                                       T.cy, w);
}

/* the deal's end: a row a place (chip, the new rank, the points of the deal, the total); the best total in gold */
static void draw_summary(void)
{
    const ppos *p = &G.pos;
    box b = T.b;
    float u = ui.unit, rh = fminf(1.0f * u, b.h * 0.8f / p->players), pw = fminf(b.w * 0.92f, 7.0f * u);
    float ph = rh * p->players + 0.5f * u, x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 70);
    ct_panel(x0, y0, pw, ph, 255);
    int best = -1;
    for (int q = 0; q < p->players; q++) best = p->score[q] > best ? p->score[q] : best;
    for (int i = 0; i < p->nout; i++) {
        int q = p->out[i];
        float cy = y0 + 0.25f * u + (i + 0.5f) * rh, d = fminf(0.72f * rh, 0.8f * u), tx = x0 + 0.75f * u;
        if (p->score[q] == best) ui_mark_gold(tx, cy, 2.0f * d);
        ui_put_c(ct_token(q, (int)d), tx, cy, d, d);
        draw_role(p->role[q], x0 + pw * 0.33f, cy, 0.8f * rh);
        float nh = 0.42f * rh;
        ui_number_signed(x0 + pw * 0.58f, cy, nh, p->last[q], 1, 1);
        ui_number(x0 + pw * 0.83f, cy, 0.56f * rh, p->score[q], 1);
        if (i < p->nout - 1) ct_rule(x0 + 0.3f * u, cy + rh / 2, pw - 0.6f * u);
    }
}

static int legal_choice(const ppos *p, const uint8_t *cards, int n)
{
    int r, m = chosen_play(cards, n, &r);
    if (p->phase == PP_GIVE) return count_bits(chosen) == p->give_left;
    return m && pres_legal(p, PA_PLAYOF(r, m));
}

static void draw(void)
{
    const ppos *p = &G.pos;
    if (dealing && ui.viewer >= 0) { dealing = 0; deal_slides(); }
    int mine = p->turn == T.me && !ui_handing() && !busy();
    uint8_t cards[PRES_HAND];
    int n;
    ct_rows h;
    hand_of(p, T.me, cards, &n, &h);
    int ready = mine && chosen && legal_choice(p, cards, n);
    draw_others();
    draw_discards();
    if (p->phase == PP_GIVE && p->turn == T.me && !ui_handing()) draw_exchange(ready);
    else draw_trick(ready && p->phase == PP_PLAY);
    /* the hand: the cards received in the exchange glow (until the first trick is over); under the hand-off cover:
     * face down */
    int held = 0, hide = ui.viewer < 0;
    for (int q = 0; q < p->players; q++) held += p->n[q];
    int first_trick = p->phase == PP_GIVE || (p->phase == PP_PLAY && 52 - held == p->npile);
    for (int i = 0; i < n; i++) {
        if (ct_hides(&M, TAG_HAND, cards[i], -1)) continue;
        if (hide) {
            cards_draw(DECK_FRENCH, CARD_NONE, h.spot[i].x, h.spot[i].y, h.w, h.spot[i].deg, CD_DOWN);
            continue;
        }
        if (drag.on && drag.moved && chosen >> i & 1) continue;
        int how = chosen >> i & 1 ? CD_RAISED : 0;
        if (!how && first_trick)
            for (int k = 0; k < p->ngot[T.me]; k++)
                if (p->got[T.me][k] == cards[i]) how = CD_GLOW;
        cards_draw(DECK_FRENCH, cards[i], h.spot[i].x, h.spot[i].y, h.w, h.spot[i].deg, how);
    }
    ct_draw(&M);
    if (drag.on && drag.moved && !hide)
        for (int i = 0, k = 0, c = count_bits(chosen); i < n; i++)
            if (chosen >> i & 1)
                cards_draw(DECK_FRENCH, cards[i], drag.x + (k++ - (c - 1) / 2.0f) * 0.4f * h.w, drag.y, h.w, 0,
                           CD_RAISED);
    if ((p->phase == PP_DEAL || p->phase == PP_OVER) && !busy()) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void submit(const uint8_t *cards, int n)
{
    const ppos *p = &G.pos;
    if (!legal_choice(p, cards, n)) { ui_sound(SND_DENY); return; }
    if (p->phase == PP_GIVE) {                  /* each card given is an action; one move for undo */
        unsigned c = chosen;
        for (int i = 0; i < n; i++)
            if (c >> i & 1) act(PA_GIVE | cards[i]);
        return;
    }
    int r, m = chosen_play(cards, n, &r);
    act(PA_PLAYOF(r, m));
}

/* a tap on card i: choose it (following: as many of its rank as the play needs), or leave it */
static void choose(const ppos *p, const uint8_t *cards, int n, int i)
{
    if (chosen >> i & 1) { chosen &= ~(1u << i); ui_sound(SND_TAP); return; }
    if (p->phase == PP_GIVE) {
        if (count_bits(chosen) >= p->give_left) chosen = 0;
        chosen |= 1u << i;
        ui_sound(SND_TAP);
        return;
    }
    int r = CARD_RANK(cards[i]);
    for (int j = 0; j < n; j++)                 /* one rank at a time */
        if (chosen >> j & 1 && CARD_RANK(cards[j]) != r) chosen = 0;
    if (p->top_count) {
        int have = 0;
        for (int j = 0; j < n; j++) have += CARD_RANK(cards[j]) == r;
        if (have < p->top_count || pres_strength(r) <= pres_strength(p->top_rank)) { ui_sound(SND_DENY); return; }
        if (!chosen) {                          /* as many as the play needs, the tapped one first */
            chosen = 1u << i;
            for (int j = 0; j < n && count_bits(chosen) < p->top_count; j++)
                if (CARD_RANK(cards[j]) == r) chosen |= 1u << j;
            ui_sound(SND_TAP);
            return;
        }
        if (count_bits(chosen) >= p->top_count) {   /* swap one of them for this one */
            for (int j = 0; j < n; j++)
                if (chosen >> j & 1) { chosen &= ~(1u << j); break; }
        }
    }
    chosen |= 1u << i;
    ui_sound(SND_TAP);
}

static void press(float x, float y)
{
    const ppos *p = &G.pos;
    drag.on = 0;
    if (busy()) return;
    if (p->phase == PP_DEAL) { act(PA_NEXT); return; }
    if (p->phase == PP_OVER) return;
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    uint8_t cards[PRES_HAND];
    int n;
    ct_rows h;
    hand_of(p, T.me, cards, &n, &h);
    int i = ct_rows_hit(&h, x, y);
    if (i < 0) {                                /* the middle: play (or give) the chosen cards */
        if (chosen && y < h.top) submit(cards, n);
        else if (y < h.top) ui_sound(SND_DENY);
        return;
    }
    int was = chosen >> i & 1;
    if (!was) choose(p, cards, n, i);
    if (chosen >> i & 1) {
        drag.on = 1, drag.moved = 0;
        drag.x0 = drag.x = x, drag.y0 = drag.y = y;
        if (was) drag.moved = -1;               /* a chosen card tapped: left when released without a drag */
    } else if (was) chosen &= ~(1u << i);
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (drag.moved <= 0 && hypotf(x - drag.x0, y - drag.y0) > 0.12f * T.hw) drag.moved = 1;
    drag.x = x, drag.y = y;
    if (drag.moved > 0) ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    const ppos *p = &G.pos;
    uint8_t cards[PRES_HAND];
    int n;
    ct_rows h;
    hand_of(p, T.me, cards, &n, &h);
    drag.x = x, drag.y = y;
    if (drag.moved > 0 && y < h.top) { submit(cards, n); drag.on = 0; return; }
    if (drag.moved < 0) {                       /* a chosen card tapped again: leave it */
        int i = ct_rows_hit(&h, x, y);
        if (i >= 0 && chosen >> i & 1) chosen &= ~(1u << i), ui_sound(SND_TAP);
    }
    drag.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* a player's bar: their rank's sign and their total */
static void bar(int player, box b)
{
    const ppos *p = &G.pos;
    float u = ui.unit, h = fminf(0.46f * u, b.h * 0.5f), cy = b.y + b.h / 2, d = fminf(0.62f * u, b.h * 0.7f);
    int role = p->role[player], v = p->score[player];
    float x = b.x;
    if (role != PR_NONE && role != PR_NEUTRAL) {
        draw_role(role, x + d / 2, cy, d);
        x += d + 0.15f * u;
    }
    float w = ct_number_w(v, floorf(h + 0.5f), 0);
    if (x + w > b.x + b.w) h *= (b.x + b.w - x) / w, w = ct_number_w(v, floorf(h + 0.5f), 0);
    ui_number(x + w / 2, cy, h, v, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's and the picker's card: a 3 and a pair of 2s, the crown above */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    float asp = cards_aspect(DECK_FRENCH), w = size * 0.46f;
    int cw = (int)w, ch = (int)(w * asp), cs = (int)(size * 0.4f);
    uint8_t *b = malloc((size_t)(cw * ch > cs * cs ? cw * ch : cs * cs) * 4);
    if (!b) return;
    memset(rgba, 0, (size_t)size * size * 4);
    static const uint8_t c[3] = {CARD(SUIT_CLUBS, 3), CARD(SUIT_SPADES, 2), CARD(SUIT_HEARTS, 2)};
    float y = size - ch - size * 0.01f;
    for (int i = 0; i < 3; i++) {
        cards_paint(DECK_FRENCH, c[i], cw, ch, b);
        art_blend(rgba, size, size, b, cw, ch, (int)(size * 0.16f + i * 0.17f * size), (int)(y - i * 0.05f * size));
    }
    paint_role(PR_PRES, cs, b);
    art_blend(rgba, size, size, b, cs, cs, (int)(size * 0.02f), 0);
    free(b);
}

static const game president_game = {
    .id = "president", .order = 290, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6, .view = VIEW_HIDDEN,
};
REGISTER_GAME(president_game)
