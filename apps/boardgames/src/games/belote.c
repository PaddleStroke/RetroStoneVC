/*
 * Belote in RetroStone Board: the French trick game for four in two partnerships, the device passed round (the rules:
 * belote_rules.c, belote.h). VIEW_HIDDEN: each hand is secret, the screen covered between turns. The viewer's hand
 * along the bottom of the table; the others round it as backs, in turn order from the left (left, across: the
 * partner, right), each with their chip; the trick in a cross in the middle, each card towards its player; the last
 * trick small in the corner. Bidding: tap the turned card to take it, one of the three aces to name that suit (the
 * second round), or the pass button in your bar; coinche: tap a contract, then the ace of its suit, or the coinche
 * chip (a cross and 2; surcoinche: a cross and 4). Playing: tap a card to raise it, tap it again (or the table, or drag
 * it there) to play it; the cards you may not play are dimmed. The bars: the team's total, and what each one said (a
 * pass, a contract) or the trumps (the taker: the ace of trumps). After a deal, the scores (no words: chips and
 * numbers); tap for the next deal.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/belote.h"
#include "games/feltable.h"
#include "shell.h"

#define PLAY_MS 300
#define DEAL_MS 280
#define FLY_MS 420
#define SHOW_MS 950                             /* a full trick stays this long before it goes to its winner */

enum { TAG_HAND = 1, TAG_SEAT, TAG_TRICK, TAG_TURNED };

static bgame G;
static int variant, dealing, mark;              /* dealing: the deal flies at the next frame; mark: the history's
                                                 * length at the last hand-off (undo stops there) */
static int sel = CARD_NONE, level = -1;         /* the card raised; coinche: the contract chosen */
static struct { int on, card, moved; float x0, y0, x, y; } drag;
static struct { Uint32 from, to; float x, y; } win_glow;   /* the gold light under a full trick's winning card */

/* ---- the table's geometry (it follows the viewer: seat 0 the viewer, 1 left, 2 across, 3 right) ---------------- */
static struct {
    box b;
    int me;
    float asp, hw;                              /* the hand's card width (eight cards) */
    float hand_top;
    float tw, th, cx, cy;                       /* the trick's cards, its centre */
    float spot_x[4], spot_y[4];                 /* the trick's places by seat */
    float seat_x[4], seat_y[4], bw, tok;        /* the others' hands (backs) and chips by seat */
    float lw, lx, ly;                           /* the last trick, small */
    float chip_w, chip_h, chip_x[BEL_LEVELS], chip_y[BEL_LEVELS];   /* coinche: the contracts */
    float ace_w, ace_x[4], ace_y, co_x, co_d;   /* the suits' aces, the coinche chip */
} T;

static void deal_slides(const bpos *p, const bpos *before);
static int seat(int q) { return (q - T.me + BEL_P) % BEL_P; }

static float ace_x2(int i, int n) { return T.cx + (i - (n - 1) / 2.0f) * 1.2f * T.tw; }

static void layout(box b)
{
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= BEL_P) T.me = 0;
    T.asp = cards_aspect(DECK_FRENCH);
    float u = ui.unit;
    T.hw = floorf(fminf(fminf(b.w / 4.4f, b.w * 0.97f / (1 + 7 * 0.36f)), b.h * 0.3f / (T.asp * 1.2f)));
    ft_hand h;
    ft_hand_layout(&h, b, BEL_HAND, BEL_HAND, T.hw, b.h * 0.3f);
    T.hw = h.w;
    T.hand_top = h.top;
    T.cx = b.x + b.w / 2;
    T.bw = floorf(T.hw * 0.4f);
    T.tok = floorf(0.5f * u);
    float bh = T.bw * T.asp, mid = (b.y + T.hand_top) / 2;
    T.seat_x[2] = T.cx, T.seat_y[2] = b.y + 0.55f * bh;
    T.seat_x[1] = b.x + 0.58f * bh, T.seat_y[1] = mid;
    T.seat_x[3] = b.x + b.w - 0.58f * bh, T.seat_y[3] = mid;
    T.seat_x[0] = T.cx, T.seat_y[0] = b.y + b.h - 0.5f * T.hw * T.asp;
    /* the trick: a cross between the backs */
    float y0 = T.seat_y[2] + 0.6f * bh, y1 = T.hand_top, x0 = b.x + 1.25f * bh, x1 = b.x + b.w - 1.25f * bh;
    T.tw = floorf(fminf(T.hw * 0.88f, fminf((y1 - y0) / (2.1f * T.asp + 0.12f), (x1 - x0) / 3.2f)));
    T.th = T.tw * T.asp;
    T.cy = (y0 + y1) / 2;
    T.spot_x[0] = T.cx, T.spot_y[0] = T.cy + 0.53f * T.th;
    T.spot_x[2] = T.cx, T.spot_y[2] = T.cy - 0.53f * T.th;
    T.spot_x[1] = T.cx - 1.04f * T.tw, T.spot_y[1] = T.cy;
    T.spot_x[3] = T.cx + 1.04f * T.tw, T.spot_y[3] = T.cy;
    /* the last trick: small, in the top left corner */
    T.lw = floorf(fmaxf(8, T.tw * 0.32f));
    T.lx = b.x + 1.55f * T.lw + 0.1f * u;
    T.ly = b.y + 1.05f * T.lw * T.asp + 0.1f * u;
    /* coinche: two rows of five contracts, then the four aces and the coinche chip */
    float aw = fminf(x1 - x0 + 0.4f * bh, b.w * 0.92f);
    T.chip_w = floorf(fminf(1.3f * u, aw / 5.5f));
    T.chip_h = floorf(0.62f * T.chip_w);
    T.ace_w = floorf(fminf(T.chip_w * 0.8f, (y1 - y0 - 2.6f * T.chip_h) / (T.asp * 1.15f)));
    float total = 2 * T.chip_h + 0.25f * T.chip_h + 0.35f * T.chip_h + T.ace_w * T.asp;
    float top = T.cy - total / 2;
    for (int l = 0; l < BEL_LEVELS; l++) {
        T.chip_x[l] = T.cx + (l % 5 - 2) * 1.08f * T.chip_w;
        T.chip_y[l] = top + T.chip_h / 2 + (l / 5) * 1.25f * T.chip_h;
    }
    T.ace_y = top + 2.6f * T.chip_h + T.ace_w * T.asp / 2;
    for (int s = 0; s < 4; s++) T.ace_x[s] = T.cx + (s - 2) * 1.12f * T.chip_w + 0.08f * T.chip_w;
    T.co_d = fminf(T.chip_w * 0.95f, T.ace_w * T.asp);
    T.co_x = T.cx + 2 * 1.12f * T.chip_w + 0.1f * T.chip_w;
    if (dealing && ui.viewer >= 0) { dealing = 0; deal_slides(&G.pos, NULL); }   /* the cover lifted: the deal flies */
}

/* ---- the viewer's hand: sorted (suits black, red, black, red; the strongest first), fanned ------------------------ */
static int order_key(const bpos *p, int c)
{
    static const int SUIT_ORDER[4] = {0, 1, 3, 2};   /* spades, hearts, clubs, diamonds */
    int trump = p->trump == BEL_NONE ? -1 : p->trump;
    return SUIT_ORDER[CARD_SUIT(c)] * 16 + (15 - bel_rank(c, trump));
}

static int sorted_hand(const bpos *p, int pl, uint8_t *out)
{
    int n = p->n[pl];
    memcpy(out, p->hand[pl], (size_t)n);
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && order_key(p, out[j]) < order_key(p, out[j - 1]); j--) {
            uint8_t t = out[j];
            out[j] = out[j - 1];
            out[j - 1] = t;
        }
    return n;
}

static void hand_of(const bpos *p, uint8_t *cards, int *n, ft_hand *h)
{
    *n = sorted_hand(p, T.me, cards);
    ft_hand_layout(h, T.b, *n, BEL_HAND, T.hw, T.b.h * 0.3f);
}

static int spot_of(const uint8_t *cards, int n, int card)
{
    for (int i = 0; i < n; i++)
        if (cards[i] == card) return i;
    return -1;
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
static void deal_slides(const bpos *p, const bpos *before)
{
    uint8_t cards[BEL_HAND];
    int n;
    ft_hand h;
    hand_of(p, cards, &n, &h);
    int k = 0;
    for (int i = 0; i < n; i++) {               /* the viewer's new cards, from the middle, turning over */
        if (before && spot_of(before->hand[T.me], before->n[T.me], cards[i]) >= 0) continue;
        int from_turned = before && cards[i] == before->turned;
        ft_slide(DECK_FRENCH, cards[i], from_turned, T.cx, T.cy, from_turned ? T.tw * 1.1f : T.tw, 0, 1, h.spot[i].x,
                 h.spot[i].y, h.w, h.spot[i].deg, DEAL_MS, k * 90, 0, TAG_HAND, cards[i], 0, k % 3 == 0 ? SND_MOVE : -1);
        k++;
    }
    for (int q = 0; q < BEL_P; q++) {           /* the others' backs, to their seats */
        if (q == T.me) continue;
        int s = seat(q), was = before ? before->n[q] : 0;
        for (int i = was; i < p->n[q]; i++, k++)
            ft_slide(DECK_FRENCH, CARD_NONE, 0, T.cx, T.cy, T.tw, 0, 0, T.seat_x[s], T.seat_y[s], T.bw,
                     s == 2 ? 0 : 90, DEAL_MS, k * 45, 0, TAG_SEAT, s, 0, -1);
    }
    if (!before && p->turned != CARD_NONE)      /* classic: the turned card */
        ft_slide(DECK_FRENCH, p->turned, 0, T.cx, T.cy, T.tw * 1.1f, 0, 1, T.cx, T.cy, T.tw * 1.1f, 0, 260,
                 k * 45 + 120, 0, TAG_TURNED, 0, 0, SND_PLACE);
}

/* what an action moved, from the position before it */
static void animate(const bpos *b, int a)
{
    const bpos *p = &G.pos;
    switch (BA_KIND(a)) {
    case BA_TAKE:
    case BA_NAME:
        if (p->variant == BEL_CLASSIC) deal_slides(p, b);   /* the rest of the pack */
        break;
    case BA_PLAY: {
        int card = BA_ARG(a), pl = b->turn, s = seat(pl);
        float x0 = T.spot_x[s], y0 = T.spot_y[s], w0 = T.tw;
        if (pl == T.me) {                       /* from the viewer's hand */
            uint8_t cards[BEL_HAND];
            int n;
            ft_hand h;
            hand_of(b, cards, &n, &h);
            int i = spot_of(cards, n, card);
            if (i >= 0) x0 = h.spot[i].x, y0 = h.spot[i].y, w0 = h.w;
            if (drag.on && drag.moved) x0 = drag.x, y0 = drag.y;
        }
        int full = b->ntrick == BEL_P - 1;
        ft_slide(DECK_FRENCH, card, 1, x0, y0, w0, 0, 1, T.spot_x[s], T.spot_y[s], T.tw, 0, PLAY_MS, 0, 0, TAG_TRICK,
                 b->ntrick, 0, SND_PLACE);
        if (!full) break;
        /* the full trick: shown a moment, its best card in gold light, then to its winner */
        int w = seat(p->last_winner), go = PLAY_MS + SHOW_MS;
        for (int i = 0; i < BEL_P; i++) {
            int si = seat((b->leader + i) % BEL_P), c = i < BEL_P - 1 ? b->trick[i] : card;
            ft_slide(DECK_FRENCH, c, 1, T.spot_x[si], T.spot_y[si], T.tw, 0, 0, T.seat_x[w], T.seat_y[w],
                     w == 0 ? T.hw * 0.7f : T.bw, w == 1 || w == 3 ? 90 : 0, FLY_MS, go + i * 40,
                     i < BEL_P - 1 ? 0 : PLAY_MS, TAG_TRICK, 10 + i, 0, i == 0 ? SND_MOVE : -1);
        }
        int sb = seat(p->last_winner);
        win_glow.from = ui_ms() + PLAY_MS;
        win_glow.to = ui_ms() + go;
        win_glow.x = T.spot_x[sb], win_glow.y = T.spot_y[sb];
        break;
    }
    }
}

static void calm(void)
{
    ft_calm();
    win_glow.from = win_glow.to = 0;
    drag.on = 0;
    sel = CARD_NONE;
    level = -1;
}

static int busy(void) { return ft_busy(); }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    variant = v == BEL_COINCHE ? BEL_COINCHE : BEL_CLASSIC;
    bel_new(&G, variant, (uint32_t)ui_random(1 << 30));
    calm();
    dealing = 1;
    mark = 0;
}

static int status(void)
{
    int w = bel_winner_team(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static unsigned winners(void)
{
    int w = bel_winner_team(&G.pos);
    return w < 0 ? 0 : w == 0 ? 1u << 0 | 1u << 2 : 1u << 1 | 1u << 3;
}

static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    bpos before = G.pos;
    if (!bel_play(&G, a)) { ui_sound(SND_DENY); return; }
    animate(&before, a);
    drag.on = 0;
    sel = CARD_NONE;
    level = -1;
    const bpos *p = &G.pos;
    int fresh = p->shuffles != before.shuffles;     /* a new deal */
    if (fresh) dealing = 1;
    if (p->turn != before.turn || fresh) mark = G.n;
    if (p->phase != BP_DEAL && p->phase != BP_OVER) ui_handoff(p->turn);
}

static int undo(void)
{
    if (G.n <= mark || !bel_undo(&G)) return 0;
    calm();
    dealing = 0;
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = bel_script(&G, s);
    calm();
    dealing = 0;
    mark = G.n;
    ui_handoff(G.pos.turn);
    return ok;
}

static int can_pass(void)
{
    const bpos *p = &G.pos;
    return p->phase <= BP_AUCTION && !ui_handing() && !busy() && bel_legal(p, BA_PASS);
}

static void pass(void) { act(BA_PASS); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* a coinche: a gold cross and 2 (4: surcoinche), centred on (cx, cy), h high */
static float mult_w(float h) { return 1.7f * h; }
static void draw_mult(float cx, float cy, float h, int coinche)
{
    float x = cx - mult_w(h) / 2;
    ui_put_c(ft_icon(1, ICON_CLOSE, (int)(0.9f * h), 250, 214, 90), x + 0.45f * h, cy, 0.9f * h, 0.9f * h);
    ui_number(x + 1.3f * h, cy, h, coinche == 2 ? 4 : 2, 1);
}

/* the others' hands, as backs: across along the top, left and right on their side; each one's chip */
static void draw_seats(void)
{
    const bpos *p = &G.pos;
    for (int q = 0; q < BEL_P; q++) {
        int s = seat(q);
        if (!s) continue;
        int n = p->n[q] - ft_hides(TAG_SEAT, s, -1);
        float step = 0.26f * T.bw, x = T.seat_x[s], y = T.seat_y[s], span = (n - 1) * step;
        for (int i = 0; i < n; i++) {
            if (s == 2) cards_draw(DECK_FRENCH, CARD_NONE, x - span / 2 + i * step, y, T.bw, 0, CD_DOWN);
            else cards_draw(DECK_FRENCH, CARD_NONE, x, y - span / 2 + i * step, T.bw, 90, CD_DOWN);
        }
        float tx, ty;
        if (s == 2) tx = x - fmaxf(span / 2 + 0.5f * T.bw, 0.5f * T.bw) - 0.75f * T.tok, ty = y;
        else tx = x, ty = y - fmaxf(span / 2 + 0.5f * T.bw, 0.5f * T.bw) - 0.75f * T.tok;
        if (q == p->turn && p->phase <= BP_PLAY) ui_mark_gold(tx, ty, 2.0f * T.tok);
        ui_put_c(ft_token(q, (int)T.tok), tx, ty, T.tok, T.tok);
    }
}

/* the trick so far: each card towards its player; a belote card in gold light */
static void draw_trick(void)
{
    const bpos *p = &G.pos;
    Uint32 now = ui_ms();
    if (win_glow.to && now >= win_glow.from && now < win_glow.to) ui_mark_gold(win_glow.x, win_glow.y, 1.9f * T.tw);
    for (int i = 0; i < p->ntrick; i++) {
        if (ft_hides(TAG_TRICK, i, -1)) continue;
        int q = (p->leader + i) % BEL_P, s = seat(q), c = p->trick[i];
        if (q == p->belote && CARD_SUIT(c) == p->trump && (CARD_RANK(c) == RANK_KING || CARD_RANK(c) == RANK_QUEEN))
            ui_mark_gold(T.spot_x[s], T.spot_y[s], 1.7f * T.tw);
        cards_draw(DECK_FRENCH, c, T.spot_x[s], T.spot_y[s], T.tw, 0, 0);
    }
}

/* the last trick, small in the corner, the winner's card in gold light */
static void draw_last(void)
{
    const bpos *p = &G.pos;
    if (p->phase != BP_PLAY || p->last_winner == BEL_NONE || ft_hides(TAG_TRICK, -1, -1)) return;
    float lh = T.lw * T.asp;
    for (int i = 0; i < BEL_P; i++) {
        int q = (p->last_leader + i) % BEL_P, s = seat(q);
        float x = T.lx + (s == 1 ? -0.95f : s == 3 ? 0.95f : 0) * T.lw;
        float y = T.ly + (s == 0 ? 0.48f : s == 2 ? -0.48f : 0) * lh;
        if (q == p->last_winner) ui_mark_gold(x, y, 2.6f * T.lw);
        cards_draw(DECK_FRENCH, p->last[i], x, y, T.lw, 0, CD_FLAT);
    }
}

/* the bidding in the middle: the turned card to take (round one), the three other aces to name (round two); coinche:
 * the contracts, the aces, the coinche chip */
static int level_ok(const bpos *p, int l) { return p->bidder == BEL_NONE || l > p->level; }

static void draw_bidding(int mine)
{
    const bpos *p = &G.pos;
    if (p->phase == BP_BID1) {
        if (!ft_hides(TAG_TURNED, -1, -1))
            cards_draw(DECK_FRENCH, p->turned, T.cx, T.cy, T.tw * 1.1f, 0, mine ? CD_GLOW : 0);
        return;
    }
    if (p->phase == BP_BID2) {
        cards_draw(DECK_FRENCH, p->turned, T.cx, T.spot_y[2] - 0.1f * T.th, T.tw * 0.8f, 0, CD_DIM);
        for (int s = 0, i = 0; s < 4; s++) {
            if (s == CARD_SUIT(p->turned)) continue;
            cards_draw(DECK_FRENCH, CARD(s, RANK_ACE), ace_x2(i, 3), T.cy + 0.25f * T.th, T.tw, 0, mine ? CD_GLOW : 0);
            i++;
        }
        return;
    }
    if (p->phase != BP_AUCTION || !mine) return;
    static const uint8_t CHIP[4] = {54, 80, 68, 240}, CHIP_ON[4] = {214, 168, 66, 255};
    if (!p->coinche) {
        for (int l = 0; l < BEL_LEVELS; l++) {
            int ok = level_ok(p, l), on = l == level;
            SDL_Texture *t = ft_round(on ? 2 : 1, (int)T.chip_w, (int)T.chip_h, 0.25f * T.chip_h, on ? CHIP_ON : CHIP);
            if (!ok) SDL_SetTextureAlphaMod(t, 90);
            ui_put_c(t, T.chip_x[l], T.chip_y[l], T.chip_w, T.chip_h);
            if (!ok) SDL_SetTextureAlphaMod(t, 255);
            float nh = 0.5f * T.chip_h;
            if (ok) ui_number(T.chip_x[l], T.chip_y[l], nh, bel_value(l), !on);
            else ui_number(T.chip_x[l], T.chip_y[l], nh, bel_value(l), 0);
        }
        for (int s = 0; s < 4; s++)
            cards_draw(DECK_FRENCH, CARD(s, RANK_ACE), T.ace_x[s], T.ace_y, T.ace_w, 0, level >= 0 ? CD_GLOW : CD_DIM);
    }
    int co = bel_legal(p, BA_COINCHE), sur = bel_legal(p, BA_SURCOINCHE);
    if (co || sur) {
        float x = p->coinche ? T.cx : T.co_x, y = p->coinche ? T.cy : T.ace_y, d = T.co_d * (p->coinche ? 1.3f : 1);
        static const uint8_t DARK[4] = {24, 30, 28, 240};
        ui_mark_gold(x, y, 1.6f * d);
        ui_put_c(ft_round(3, (int)d, (int)d, 0.5f * d, DARK), x, y, d, d);
        draw_mult(x, y, 0.36f * d, sur ? 2 : 1);
    }
}

/* the deal's scores: a row a team (its two chips; the taker's: the ace of trumps, the contract, a cross when they
 * failed; the points made, the deal's score, the total); the leading total in gold light */
static void draw_summary(void)
{
    const bpos *p = &G.pos;
    box b = T.b;
    float u = ui.unit, rh = fminf(1.5f * u, b.h * 0.3f), pw = fminf(b.w * 0.96f, 8.6f * u), ph = 2 * rh + 0.5f * u;
    float x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    ft_panel(b, x0, y0, pw, ph);
    int lead = p->total[1] > p->total[0];
    float col = pw / 8.6f;
    for (int t = 0; t < 2; t++) {
        float cy = y0 + 0.25f * u + (t + 0.5f) * rh, d = fminf(0.6f * rh, 0.62f * u), tx = x0 + 0.6f * col;
        if (t == lead && p->total[0] != p->total[1]) ui_mark_gold(tx + 0.45f * d, cy, 2.6f * d);
        ui_put_c(ft_token(t, (int)d), tx, cy - 0.12f * d, d, d);
        ui_put_c(ft_token(t + 2, (int)d), tx + 0.9f * d, cy + 0.12f * d, d, d);
        float nh = fminf(0.36f * rh, 0.4f * u);
        if (bel_team(p->taker) == t) {          /* the contract */
            float ax = x0 + 2.25f * col;
            cards_draw(DECK_FRENCH, CARD(p->trump, RANK_ACE), ax, cy, 0.62f * col, 0, CD_FLAT);
            if (p->variant == BEL_COINCHE) {
                ui_number(ax + 1.0f * col, cy - (p->coinche ? 0.3f * nh : 0), 0.8f * nh, bel_value(p->level), 1);
                if (p->coinche) draw_mult(ax + 1.0f * col, cy + 0.55f * nh, 0.55f * nh, p->coinche);
            }
            if (!p->made)
                ui_put_c(ft_icon(2, ICON_CLOSE, (int)(0.7f * rh), 214, 70, 60), ax, cy, 0.7f * rh, 0.7f * rh);
        }
        ui_number(x0 + 4.4f * col, cy, nh, p->made_pts[t], 1);
        ui_number_signed(x0 + 5.85f * col, cy, nh, p->score[t], 1, 1);
        ui_number(x0 + 7.55f * col, cy, 1.3f * nh, p->total[t], 1);
        if (!t) ft_rule(x0 + 0.3f * u, cy + rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    const bpos *p = &G.pos;
    if (dealing && ui.viewer >= 0) { dealing = 0; deal_slides(p, NULL); }
    int mine = p->turn == T.me && !ui_handing() && !busy();
    draw_seats();
    draw_last();
    if (p->phase <= BP_AUCTION) draw_bidding(mine);
    else draw_trick();
    /* the viewer's hand */
    uint8_t cards[BEL_HAND];
    int n;
    ft_hand h;
    hand_of(p, cards, &n, &h);
    for (int i = 0; i < n; i++) {
        if (ft_hides(TAG_HAND, cards[i], -1)) continue;
        if (drag.on && drag.moved && cards[i] == drag.card) continue;
        int how = 0;
        if (p->phase == BP_PLAY && p->turn == T.me && !ui_handing()) {
            if (!bel_legal(p, BA_PLAY | cards[i])) how = CD_DIM;
            else if (cards[i] == sel) how = CD_RAISED;
        }
        cards_draw(DECK_FRENCH, cards[i], h.spot[i].x, h.spot[i].y, h.w, h.spot[i].deg, how);
    }
    ft_draw_moves();
    if (drag.on && drag.moved) cards_draw(DECK_FRENCH, drag.card, drag.x, drag.y, h.w * 1.04f, 0, CD_RAISED);
    if ((p->phase == BP_DEAL || p->phase == BP_OVER) && !busy()) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int on_card(float cx, float cy, float w, float x, float y) { return cards_in(cx, cy, w * 1.06f, T.asp, x, y); }

static void play_card(int c) { act(BA_PLAY | c); }

static void press(float x, float y)
{
    const bpos *p = &G.pos;
    drag.on = 0;
    if (busy()) return;
    if (p->phase == BP_DEAL) { act(BA_NEXT); return; }
    if (p->phase == BP_OVER) return;
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    switch (p->phase) {
    case BP_BID1:
        if (on_card(T.cx, T.cy, T.tw * 1.1f, x, y)) act(BA_TAKE);
        else ui_sound(SND_DENY);
        return;
    case BP_BID2:
        for (int s = 0, i = 0; s < 4; s++) {
            if (s == CARD_SUIT(p->turned)) continue;
            if (on_card(ace_x2(i, 3), T.cy + 0.25f * T.th, T.tw, x, y)) { act(BA_NAME | s); return; }
            i++;
        }
        ui_sound(SND_DENY);
        return;
    case BP_AUCTION: {
        int co = bel_legal(p, BA_COINCHE), sur = bel_legal(p, BA_SURCOINCHE);
        float bx = p->coinche ? T.cx : T.co_x, by = p->coinche ? T.cy : T.ace_y, d = T.co_d * (p->coinche ? 1.3f : 1);
        if ((co || sur) && hypotf(x - bx, y - by) < 0.6f * d) { act(sur ? BA_SURCOINCHE : BA_COINCHE); return; }
        if (!p->coinche) {
            for (int l = 0; l < BEL_LEVELS; l++)
                if (fabsf(x - T.chip_x[l]) < 0.54f * T.chip_w && fabsf(y - T.chip_y[l]) < 0.62f * T.chip_h) {
                    if (level_ok(p, l)) level = l, ui_sound(SND_TAP);
                    else ui_sound(SND_DENY);
                    return;
                }
            for (int s = 0; s < 4; s++)
                if (on_card(T.ace_x[s], T.ace_y, T.ace_w, x, y)) {
                    if (level >= 0) act(BA_BIDOF(level, s));
                    else ui_sound(SND_DENY);
                    return;
                }
        }
        ui_sound(SND_DENY);
        return;
    }
    case BP_PLAY: {
        uint8_t cards[BEL_HAND];
        int n;
        ft_hand h;
        hand_of(p, cards, &n, &h);
        int i = ft_hand_hit(&h, x, y);
        if (i < 0) {                            /* the table: play the raised card */
            if (sel != CARD_NONE && y < T.hand_top) play_card(sel);
            else if (sel != CARD_NONE) sel = CARD_NONE;
            return;
        }
        int c = cards[i];
        if (!bel_legal(p, BA_PLAY | c)) { ui_sound(SND_DENY); return; }
        if (c == sel) { play_card(c); return; }
        sel = c;
        ui_sound(SND_TAP);
        drag.on = 1, drag.card = c, drag.moved = 0;
        drag.x0 = drag.x = x, drag.y0 = drag.y = y;
        return;
    }
    }
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (!drag.moved && hypotf(x - drag.x0, y - drag.y0) > 0.12f * T.hw) drag.moved = 1;
    drag.x = x, drag.y = y;
    if (drag.moved) ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    int moved = drag.moved, c = drag.card;
    drag.x = x, drag.y = y;
    if (moved && y < T.hand_top && G.pos.phase == BP_PLAY && G.pos.turn == T.me) { play_card(c); return; }
    drag.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* a player's bar: the team's total; then what they said (a pass, a contract, a coinche) or, the taker, the trumps */
static void bar(int player, box b)
{
    const bpos *p = &G.pos;
    float u = ui.unit, h = fminf(0.42f * u, b.h * 0.5f), cy = b.y + b.h / 2;
    int total = p->total[bel_team(player)];
    /* what follows the total: its parts' widths at h */
    int word = 0, value = -1, mult = 0;
    float ace = 0.5f * u;
    if (p->phase <= BP_AUCTION) {
        int k = BA_KIND(p->said[player]);
        if (k == BA_PASS) word = 1;
        else if (k == BA_BID) word = 2, value = bel_value(BA_ARG(p->said[player]) >> 2);
        else if (k == BA_COINCHE) word = 3, mult = 1;
        else if (k == BA_SURCOINCHE) word = 3, mult = 2;
        if (p->phase == BP_AUCTION && player == p->bidder && p->coinche) mult = p->coinche;
    } else if (player == p->taker && p->phase != BP_OVER) {
        word = 2;
        if (p->variant == BEL_COINCHE) value = bel_value(p->level), mult = p->coinche;
    }
    float wt = ft_number_w(total, h, 0), gap = 0.22f * u;
    float ww = word == 1 ? 0.55f * u : word == 2 ? ace + (value >= 0 ? 0.12f * u + ft_number_w(value, 0.8f * h, 0) : 0)
                                                 : 0;
    if (mult) ww += (ww > 0 ? gap : 0) + mult_w(0.7f * h);
    float need = wt + (ww > 0 ? gap + ww : 0), k = need > b.w ? b.w / need : 1;
    h *= k, gap *= k, ace *= k, wt *= k;
    float x = b.x;
    ui_number_signed(x + wt / 2, cy, h, total, 1, 0);
    x += wt + gap;
    if (word == 1) {
        float d = 0.55f * u * k;
        ui_put_c(ft_icon(0, ICON_PASS, (int)d, 246, 238, 220), x + d / 2, cy, d, d);
        x += d;
    } else if (word == 2) {
        int suit = p->phase <= BP_AUCTION ? BA_ARG(p->said[player]) & 3 : p->trump;
        cards_draw(DECK_FRENCH, CARD(suit, RANK_ACE), x + ace / 2, cy, ace, 0, CD_FLAT);
        x += ace;
        if (value >= 0) {
            float vw = ft_number_w(value, 0.8f * h, 0);
            ui_number(x + 0.12f * u * k + vw / 2, cy, 0.8f * h, value, 1);
            x += 0.12f * u * k + vw;
        }
    }
    if (mult) {
        if (word) x += gap;
        draw_mult(x + mult_w(0.7f * h) / 2, cy, 0.7f * h, mult);
    }
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's and the picker's cards: classic, the trumps' best four (ten, ace, nine, the jack in front); coinche, the
 * four aces (any suit may be named in the auction) */
static void thumb(int v, int size, uint8_t *rgba)
{
    static const uint8_t classic[4] = {CARD(SUIT_HEARTS, 10), CARD(SUIT_HEARTS, 1), CARD(SUIT_HEARTS, 9),
                                       CARD(SUIT_HEARTS, 11)},
                         coinche[4] = {CARD(SUIT_SPADES, 1), CARD(SUIT_HEARTS, 1), CARD(SUIT_CLUBS, 1),
                                       CARD(SUIT_DIAMONDS, 1)};
    const uint8_t *c = v == BEL_COINCHE ? coinche : classic;
    float asp = cards_aspect(DECK_FRENCH), w = fminf(size * 0.94f / (1 + 3 * 0.32f), size * 0.94f / (asp + 3 * 0.1f));
    int cw = (int)w, ch = (int)(w * asp);
    float x0 = (size - (w + 3 * 0.32f * w)) / 2, y0 = (size - (ch + 3 * 0.1f * w)) / 2;
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    memset(rgba, 0, (size_t)size * size * 4);
    for (int i = 0; i < 4; i++) {
        cards_paint(DECK_FRENCH, c[i], cw, ch, b);
        art_blend(rgba, size, size, b, cw, ch, (int)(x0 + i * 0.32f * w), (int)(y0 + (3 - i) * 0.1f * w));
    }
    free(b);
}

static const game belote_game = {
    .id = "belote", .order = 275, .variants = BEL_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(belote_game)
