/*
 * Contract bridge in RetroStone Board: four players in two partnerships passing the device round, each hand secret
 * (VIEW_HIDDEN; the rules: bridge_rules.c, bridge.h). Chicago (four deals) by default, or a rubber.
 * - The auction: the calls so far in a compact table (a column a player, their chips at its head in seat order: left,
 *   across, right, the viewer; a red ring round a vulnerable player's chip), and, for the player to call, the bidding
 *   box: levels 1 to 7 by clubs, diamonds, hearts, spades and no trumps (an ivory disc, the four pips, struck through),
 *   then pass (green), double (red, a cross), redouble (blue, two crosses) and the tick. Tap a call (lit gold), then
 *   the tick, or the same call again.
 * - The play: the viewer's hand along the bottom; the dummy, once the opening lead is made, face up in four suit
 *   columns at its seat (across from the declarer, or on a defender's left or right); the trick in a cross, each card
 *   towards its player; the tricks in front of the player who keeps them (the declarer, and the defender who won the
 *   first one), as a small pile and a number. The device goes to the declarer for the dummy's turns too: the dummy's
 *   owner never holds it during the play. The contract in a corner (the declarer's chip, the level, the strain, the
 *   doubles); tap it for the auction and the last trick.
 * - After each deal the score sheet: we (the viewer's side) and they, in columns of digits; Chicago a row a deal (its
 *   contract and result, the points), the rubber above and below the line, a rule after each game; the totals.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/bridge.h"
#include "games/endtable.h"
#include "games/tricks.h"
#include "shell.h"

#define PLAY_MS 300
#define FLY_MS 420
#define SHOW_MS 950                             /* a full trick stays this long before it goes to its keeper */

enum { TAG_HAND = 1, TAG_TRICK, TAG_DUMMY };
enum { PIC_NT, PIC_VUL };

static brg_game G;
static ct_moves M = {.rest = 300};
static int variant, mark, pick = -1, info; /* mark: the history's length at the last hand-off; pick: the call chosen;
                                            * info: the auction and the last trick shown over the table */
static unsigned seen;                           /* the players whose cards have flown in this deal */
static int sel = CARD_NONE;                     /* the card raised (the viewer's hand or the dummy) */
static struct { int on, card, moved, dummy; float x0, y0, x, y; } drag;
static struct { Uint32 from, to; float x, y; } win_glow;
static box tb;                                  /* the table */

static const uint8_t BTN[4] = {54, 80, 68, 240}, BTN_ON[4] = {214, 168, 66, 255}, BTN_PASS[4] = {44, 118, 70, 255},
                     BTN_X[4] = {172, 48, 40, 255}, BTN_XX[4] = {46, 86, 164, 255};

/* ---- the geometry (it follows the viewer: seat 0 the viewer, 1 left, 2 across, 3 right) ---------------------------- */
static struct {
    int me, portrait;
    float u, asp, tok;
    float hw, hand_top;                         /* the hand's card width, its top (thirteen cards) */
    box A;                                      /* the table above the hand */
    float cx[4], cy[4];                         /* the chips by seat */
    float sx[4], sy[4], tw;                     /* the trick's places by seat, its cards' width */
    int dseat;                                  /* the dummy's seat (-1: none) */
    box C;                                      /* the dummy's columns */
    float dw, dstep;                            /* their cards' width, the step down a column */
    float kx, ky, kw, kh;                       /* the contract's panel */
    box H;                                      /* the auction's table */
    float gx, gy, gw, gh, gpx, gpy;             /* the bidding box: left, top, a call's size, the pitch */
} L;

static int seat(int q) { return (q - L.me + BRG_P) % BRG_P; }

static void geo(void)
{
    const brg_pos *p = &G.pos;
    box b = tb;
    float u = ui.unit;
    L.u = u;
    L.me = ui_bottom();
    if (L.me < 0 || L.me >= BRG_P) L.me = 0;
    L.asp = cards_aspect(DECK_FRENCH);
    L.portrait = b.w <= b.h * 1.05f;
    float wmax = floorf(fminf(b.w * (L.portrait ? 0.19f : 0.11f), b.h * (L.portrait ? 0.15f : 0.17f)));
    ct_rows h;
    ct_rows_layout(&h, b, BRG_HAND, BRG_HAND, wmax, b.h * 0.26f);
    L.hw = h.w;
    L.hand_top = h.top;
    box A = {b.x, b.y, b.w, L.hand_top - b.y};
    L.A = A;
    L.tok = floorf(fminf(0.6f * u, A.h * 0.085f));
    float m = 1.4f * L.tok, gap = 0.12f * u;

    /* the contract's panel: a top corner (the one away from the dummy) */
    int dummy = brg_dummy(p);
    L.dseat = p->phase == BRG_PLAY && dummy != BRG_NONE ? seat(dummy) : -1;
    if (L.dseat == 0) L.dseat = -1;            /* the viewer is the dummy (never during the play) */
    L.kh = floorf(1.25f * L.tok);
    L.kw = floorf(3.3f * L.tok);
    L.kx = L.dseat == 3 ? A.x + gap : A.x + A.w - gap - L.kw;
    L.ky = A.y + gap;

    /* the dummy's place; the trick's room (under the contract's row, or under the dummy across) */
    box R = {A.x + m, L.ky + L.kh + gap, A.w - 2 * m, 0};
    box D = {0, 0, 0, 0};
    if (L.dseat == 2) {
        D = (box){A.x + m, A.y + gap, A.w - m - L.kw - 2 * gap, A.h * (L.portrait ? 0.36f : 0.46f)};
        R.y = D.y + D.h + gap;
    } else if (L.dseat == 1 || L.dseat == 3) {
        float w = A.w * (L.portrait ? 0.42f : 0.3f);
        D = (box){L.dseat == 1 ? A.x + gap : A.x + A.w - gap - w, A.y + gap, w, A.h - 2 * gap};
        if (L.dseat == 1) R.x = D.x + D.w + gap, R.w = A.x + A.w - m - R.x;
        else R.w = D.x - gap - R.x;
    }
    R.h = A.y + A.h - gap - 1.3f * L.tok - R.y;   /* the viewer's chip under it */
    float tcx = R.x + R.w / 2, tcy = R.y + R.h / 2;
    L.tw = floorf(fminf(0.85f * L.hw, fminf(R.h / (2 * L.asp * 1.03f + 0.1f), R.w / 3.25f)));
    float th = L.tw * L.asp;
    L.sx[0] = tcx, L.sy[0] = tcy + 0.53f * th;
    L.sx[2] = tcx, L.sy[2] = tcy - 0.53f * th;
    L.sx[1] = tcx - 1.04f * L.tw, L.sy[1] = tcy;
    L.sx[3] = tcx + 1.04f * L.tw, L.sy[3] = tcy;
    float half = 0.7f * L.tok;
    /* the viewer's chip under the trick, the one across in the contract's row, the sides at the edges */
    L.cx[0] = tcx, L.cy[0] = R.y + R.h + 0.65f * L.tok;
    L.cx[2] = tcx, L.cy[2] = L.ky + L.kh / 2;
    L.cx[1] = A.x + half + 0.1f * u, L.cy[1] = tcy;
    L.cx[3] = A.x + A.w - half - 0.1f * u, L.cy[3] = tcy;
    if (L.dseat == 2) L.cx[2] = A.x + half + 0.1f * u, L.cy[2] = D.y + half;
    if (L.dseat == 1 || L.dseat == 3) L.cx[L.dseat] = D.x + D.w / 2, L.cy[L.dseat] = D.y + half;

    /* the dummy's columns: a suit each, its cards stepping down (the longest suit as dealt fits) */
    if (L.dseat >= 0) {
        box C = D;
        if (L.dseat == 2) C.x += 0.1f * u, C.w -= 0.1f * u;
        else C.y += 1.45f * L.tok, C.h -= 1.45f * L.tok;
        int len[4] = {0}, longest = 1;
        for (int i = 0; i < BRG_HAND; i++) len[trick_suit_order(CARD_SUIT(p->orig[dummy][i]))]++;
        for (int k = 0; k < 4; k++) longest = len[k] > longest ? len[k] : longest;
        L.dw = ct_columns_fit(C, 0.8f * L.hw, longest, L.asp, &L.dstep);
        L.C = C;
    }

    /* the auction: the table of calls, the bidding box (under it, or beside it in landscape) */
    int wide = A.w > A.h * 1.25f;
    float gw = floorf(fminf(1.3f * u, (wide ? A.w * 0.56f : A.w * 0.94f) / 5.35f));
    float gh = floorf(fminf(0.62f * gw, (A.h * (wide ? 0.97f : 0.64f)) / (8 * 1.13f)));
    L.gw = gw, L.gh = gh, L.gpx = floorf(1.07f * gw), L.gpy = floorf(1.13f * gh);
    float bw = 4 * L.gpx + gw, bh = 7 * L.gpy + gh;
    if (wide) {
        L.gx = A.x + A.w - gap - bw;
        L.gy = A.y + (A.h - bh) / 2;
        L.H = (box){A.x + gap, A.y + gap, L.gx - 2 * gap - A.x, A.h - 2 * gap};
    } else {
        L.gx = A.x + (A.w - bw) / 2;
        L.gy = A.y + A.h - bh - 0.15f * u;
        L.H = (box){A.x + gap, A.y + gap, A.w - 2 * gap, L.gy - 2 * gap - A.y};
    }
}

static void layout(box b)
{
    tb = b;
    geo();
}

/* ---- the cards' places ---------------------------------------------------------------------------------------------- */
static void hand_rows(const brg_pos *p, ct_rows *h) { ct_rows_layout(h, tb, p->n[L.me], BRG_HAND, L.hw, tb.h * 0.26f); }

/* the dummy's cards in the order drawn (column by column, the highest at the top) and their centres */
static int suit_col(int card) { return trick_suit_order(CARD_SUIT(card)); }

static int dummy_spots(const brg_pos *p, uint8_t *cards, float *x, float *y)
{
    int d = brg_dummy(p);
    if (d == BRG_NONE || L.dseat < 0) return 0;
    return ct_columns(L.C, L.dw, L.dstep, L.asp, p->hand[d], p->n[d], suit_col, cards, x, y);
}

static int dummy_at(const brg_pos *p, float px, float py)
{
    uint8_t c[BRG_HAND];
    float x[BRG_HAND], y[BRG_HAND];
    int n = dummy_spots(p, c, x, y);
    int i = ct_columns_hit(c, x, y, n, L.dw, L.asp, sel, px, py);
    return i < 0 ? CARD_NONE : c[i];
}

/* ---- the moving cards ------------------------------------------------------------------------------------------------ */
static void deal_in(void)                       /* the viewer's cards fly in from the middle, turning over */
{
    const brg_pos *p = &G.pos;
    ct_rows h;
    hand_rows(p, &h);
    float cx = L.A.x + L.A.w / 2, cy = L.A.y + L.A.h / 2;
    for (int i = 0; i < h.n; i++)
        ct_rest(&M, ct_slide(&M, DECK_FRENCH, p->hand[L.me][i], cx, cy, 0.7f * L.hw, 0, 0, h.spot[i].x, h.spot[i].y,
                             h.w, h.spot[i].deg, 1, 260, 45 * i, TAG_HAND, p->hand[L.me][i], 0,
                             i % 3 == 0 ? SND_MOVE : -1), 0);
}

static void animate(const brg_pos *b, int a)
{
    const brg_pos *p = &G.pos;
    if (BRG_KIND(a) == BRG_ACALL) { ui_sound(SND_PLACE); return; }
    if (BRG_KIND(a) != BRG_APLAY) return;
    int card = BRG_ARG(a), pl = b->turn, s = seat(pl);
    float x0 = L.cx[s], y0 = L.cy[s], w0 = 0.5f * L.tw, a0 = 0;
    if (s == 0) {                               /* from the viewer's hand */
        ct_rows h;
        hand_rows(b, &h);
        int i = trick_find(b->hand[L.me], b->n[L.me], card);
        if (i >= 0) x0 = h.spot[i].x, y0 = h.spot[i].y - 0.14f * h.w, w0 = h.w, a0 = h.spot[i].deg;
    } else if (pl == brg_dummy(b) && brg_dummy_shown(b)) {
        uint8_t c[BRG_HAND];
        float x[BRG_HAND], y[BRG_HAND];
        int n = dummy_spots(b, c, x, y);
        for (int i = 0; i < n; i++)
            if (c[i] == card) x0 = x[i], y0 = y[i] - 0.12f * L.dw, w0 = L.dw;
    }
    if (drag.on && drag.moved && drag.card == card) x0 = drag.x, y0 = drag.y, a0 = 0;
    ct_rest(&M, ct_slide(&M, DECK_FRENCH, card, x0, y0, w0, a0, 1, L.sx[s], L.sy[s], L.tw, 0, 1, PLAY_MS, 0, TAG_TRICK,
                         pl, 0, SND_PLACE), 200);
    if (b->tricks == 0 && b->ntrick == 0) {    /* the opening lead: the dummy turns its cards up, column by column */
        geo();
        uint8_t c[BRG_HAND];
        float x[BRG_HAND], y[BRG_HAND];
        int n = dummy_spots(p, c, x, y), ds = L.dseat;
        for (int i = 0; ds >= 0 && i < n; i++)
            ct_slide(&M, DECK_FRENCH, c[i], L.cx[ds], L.cy[ds], 0.6f * L.dw, 0, 0, x[i], y[i], L.dw, 0, 1, 320,
                     PLAY_MS + 120 + 40 * i, TAG_DUMMY, c[i], 0, i % 4 == 0 ? SND_MOVE : -1);
    }
    if (b->ntrick != BRG_P - 1) return;
    /* the full trick: shown a moment, its best card in gold light, then to the side's keeper */
    int k = p->keeper[brg_side(p->last_winner)], ks = seat(k), go = PLAY_MS + SHOW_MS;
    for (int i = 0; i < BRG_P; i++) {
        int q = (b->leader + i) % BRG_P, si = seat(q);
        int mv = ct_slide(&M, DECK_FRENCH, p->last[i], L.sx[si], L.sy[si], L.tw, 0, 1, L.cx[ks], L.cy[ks],
                          0.5f * L.tok, 0, 0, FLY_MS, go + 40 * i, 0, 0, 0, i == 0 ? SND_MOVE : -1);
        ct_show(&M, mv, q == pl ? PLAY_MS : 0);   /* the card just played shows once it has landed */
    }
    int ws = seat(p->last_winner);
    win_glow.from = ui_ms() + PLAY_MS;
    win_glow.to = ui_ms() + go;
    win_glow.x = L.sx[ws], win_glow.y = L.sy[ws];
}

static void calm(void)
{
    ct_calm(&M);
    win_glow.from = win_glow.to = 0;
    drag.on = 0;
    sel = CARD_NONE;
    pick = -1;
    info = 0;
}

static int busy(void) { return ct_busy(&M); }

/* ---- the position ---------------------------------------------------------------------------------------------------- */
static void start(int v)
{
    variant = v == BRG_RUBBER ? BRG_RUBBER : BRG_CHICAGO;
    brg_new(&G, variant, (uint32_t)ui_random(1 << 30));
    calm();
    mark = 0;
    seen = 0;
}

static int status(void)
{
    int w = brg_winner(&G.pos);
    return w < 0 ? GS_PLAY : w == 2 ? GS_DRAW : gs_win(w);
}

static unsigned winners(void)                   /* the winning side: both partners (a draw: all four) */
{
    int w = brg_winner(&G.pos);
    return w < 0 ? 0 : w == 2 ? 0xfu : 1u << w | 1u << (w + 2);
}

static int turn(void) { return brg_controller(&G.pos); }

static void act(int a)
{
    brg_pos before = G.pos;
    geo();
    if (!brg_play(&G, a)) { ui_sound(SND_DENY); return; }
    animate(&before, a);
    drag.on = 0;
    sel = CARD_NONE;
    pick = -1;
    info = 0;
    const brg_pos *p = &G.pos;
    if (BRG_KIND(a) == BRG_ANEXT) seen = 0;
    if (brg_controller(p) != brg_controller(&before) || p->phase != before.phase) mark = G.n;
    if (p->phase == BRG_AUCTION || p->phase == BRG_PLAY) ui_handoff(brg_controller(p));
}

static int undo(void)
{
    if (pick >= 0 || sel != CARD_NONE || info) { pick = -1, sel = CARD_NONE, info = 0; return 1; }
    if (G.n <= mark || !brg_undo(&G)) return 0;
    calm();
    ui_handoff(brg_controller(&G.pos));
    return 1;
}

static int script(const char *s)
{
    int ok = brg_script(&G, s);
    calm();
    mark = G.n;
    seen = ~0u;
    ui_handoff(brg_controller(&G.pos));
    return ok;
}

/* ---- small pictures ---------------------------------------------------------------------------------------------- */
static void paint(int kind, int size, uint8_t *rgba)
{
    static const uint8_t red[3] = {206, 54, 44}, ivory[3] = {246, 238, 222};
    memset(rgba, 0, (size_t)size * size * 4);
    if (kind == PIC_VUL) {                      /* vulnerable: a red ring */
        art_ring(size, 0.74f, red, 1, rgba);
        return;
    }
    /* no trumps: an ivory disc in its rim, the four suits' pips in a cluster, struck through in red */
    art_disc(size, ivory, 1, 0, rgba);
    cards_disc_rim(rgba, size);
    int m = (int)(0.6f * size);
    uint8_t *pip = malloc((size_t)m * m * 4), *bar = malloc((size_t)size * size * 4);
    if (!pip || !bar) { free(pip); free(bar); return; }
    static const int suit[4] = {SUIT_SPADES, SUIT_HEARTS, SUIT_DIAMONDS, SUIT_CLUBS};
    for (int k = 0; k < 4; k++) {
        cards_suit_icon(DECK_FRENCH, suit[k], m, pip);
        float r = 0.4f * m, c = (m - 1) / 2.0f;
        for (int y = 0; y < m; y++)            /* the pip alone: its disc's rim left out */
            for (int x = 0; x < m; x++)
                if (hypotf(x - c, y - c) > r) pip[((size_t)y * m + x) * 4 + 3] = 0;
        int ox = (int)(size * (k & 1 ? 0.69f : 0.31f) - m / 2.0f), oy = (int)(size * (k & 2 ? 0.69f : 0.31f) - m / 2.0f);
        art_blend(rgba, size, size, pip, m, m, ox, oy);
    }
    art_style st = {{206, 54, 44}, {206, 54, 44}, {206, 54, 44}, {206, 54, 44}, 0, 0};
    art_fig *f = art_fig_new();
    const float slash[] = {0.76f, 0.15f, 0.85f, 0.24f, 0.24f, 0.85f, 0.15f, 0.76f};
    art_poly(f, ART_PART, slash, 4);
    art_render(f, &st, size, size, (float)size, 0, 0, bar);
    art_fig_free(f);
    art_blend(rgba, size, size, bar, size, size, 0, 0);
    free(pip);
    free(bar);
}

static void strain_icon(int strain, float cx, float cy, float d)
{
    int s = (int)(d + 0.5f);
    SDL_Texture *t = strain == BRG_NT ? ct_picture(paint, PIC_NT, s) : ct_suit(DECK_FRENCH, brg_suit(strain), s);
    ui_put_c(t, cx, cy, (float)s, (float)s);
}

static void chip(int player, float cx, float cy, float d, int glow)
{
    if (glow) ui_mark_gold(cx, cy, 2.0f * d);
    if (brg_vulnerable(&G.pos, brg_side(player)) && G.pos.phase <= BRG_PLAY) {
        int s = (int)(1.32f * d);
        ui_put_c(ct_picture(paint, PIC_VUL, s), cx, cy, (float)s, (float)s);
    }
    int s = (int)d;
    ui_put_c(ct_token(player, s), cx, cy, (float)s, (float)s);
}

/* the doubles: a cross (two for a redouble), h high, from x; their width */
static float doubles_w(int dbl, float h) { return dbl ? (dbl == 2 ? 1.6f : 0.9f) * h : 0; }
static void doubles(int dbl, float x, float cy, float h)
{
    int s = (int)(0.9f * h);
    for (int k = 0; k < dbl; k++)
        ui_put_c(dbl == 2 ? ct_icon(ICON_CLOSE, s, 120, 160, 240) : ct_icon(ICON_CLOSE, s, 236, 84, 70),
                 x + (0.45f + 0.7f * k) * h, cy, (float)s, (float)s);
}

/* a call, centred on (cx, cy), h high (pass: the pass sign, faint) */
static void draw_call(int call, float cx, float cy, float h)
{
    if (call == BRG_PASS) {
        int s = (int)(0.78f * h);
        SDL_Texture *t = ct_icon(ICON_PASS, s, 246, 238, 220);
        SDL_SetTextureAlphaMod(t, 150);
        ui_put_c(t, cx, cy, (float)s, (float)s);
        SDL_SetTextureAlphaMod(t, 255);
    } else if (call == BRG_DOUBLE || call == BRG_REDOUBLE) {
        int d = call == BRG_DOUBLE ? 1 : 2;
        doubles(d, cx - doubles_w(d, h) / 2, cy, h);
    } else {
        ui_number(cx - 0.36f * h, cy, 0.72f * h, BRG_LEVEL(call), 1);
        strain_icon(BRG_STRAIN(call), cx + 0.36f * h, cy, 0.86f * h);
    }
}

/* a contract: the declarer's chip, the level, the strain, the doubles, from x; its width (draw 0: measured only) */
static float contract_at(int decl, int level, int strain, int dbl, float x, float cy, float h, int draw)
{
    float w = h + 0.15f * h + 0.55f * h + 0.95f * h + (dbl ? 0.1f * h + doubles_w(dbl, 0.7f * h) : 0);
    if (!draw) return w;
    int s = (int)h;
    ui_put_c(ct_token(decl, s), x + h / 2, cy, (float)s, (float)s);
    x += 1.15f * h;
    ui_number(x + 0.27f * h, cy, 0.72f * h, level, 1);
    strain_icon(strain, x + 0.55f * h + 0.47f * h, cy, 0.86f * h);
    x += 1.5f * h;
    if (dbl) doubles(dbl, x + 0.1f * h, cy, 0.7f * h);
    return w;
}

/* ---- the auction ----------------------------------------------------------------------------------------------------- */
/* the table of calls in box H: the chips at the head of the columns (left, across, right, the viewer); the rows that
 * fit, the last ones; the next call's cell lit while the auction goes on */
static void draw_history(box H, float rh_max)
{
    const brg_pos *p = &G.pos;
    float u = L.u, cw = floorf(fminf(1.7f * u, H.w / 4)), x0 = H.x + (H.w - 4 * cw) / 2;
    float tok = fminf(L.tok, 0.62f * cw), head = 1.5f * tok;
    int live = p->phase == BRG_AUCTION, first = (seat(p->dealer) + 3) % 4;
    int cells = first + p->ncalls + live, rows = (cells + 3) / 4;
    float rh = floorf(fminf(rh_max, 0.62f * u));
    int fit = (int)((H.h - head) / rh);
    if (fit < 1) fit = 1;
    int skip = rows > fit ? rows - fit : 0;
    for (int col = 0; col < 4; col++) {
        int q = (L.me + 1 + col) % BRG_P;
        chip(q, x0 + (col + 0.5f) * cw, H.y + head / 2, tok, live && q == p->turn);
    }
    ct_rule(x0 + 0.1f * cw, H.y + head - 0.05f * u, 4 * cw - 0.2f * cw);
    for (int i = 0; i < p->ncalls + live; i++) {
        int pos = first + i, r = pos / 4 - skip, col = pos % 4;
        if (r < 0) continue;
        float x = x0 + (col + 0.5f) * cw, y = H.y + head + (r + 0.5f) * rh;
        if (i == p->ncalls) {                   /* the call to come */
            ui_put_c(ct_round((int)(0.9f * cw), (int)(0.86f * rh), 0.3f * rh, BTN), x, y, 0.9f * cw, 0.86f * rh);
            continue;
        }
        draw_call(p->call[i], x, y, 0.7f * rh);
    }
}

/* the bidding box: a call's cell */
static void cell_at(int call, float *x, float *y, float *w)
{
    *w = L.gw;
    if (call >= 1 && call <= 35) {
        *x = L.gx + BRG_STRAIN(call) * L.gpx + L.gw / 2;
        *y = L.gy + (BRG_LEVEL(call) - 1) * L.gpy + L.gh / 2;
        return;
    }
    *y = L.gy + 7 * L.gpy + L.gh / 2;
    if (call == BRG_PASS) *x = L.gx + (L.gpx + L.gw) / 2, *w = L.gpx + L.gw;
    else *x = L.gx + (call == BRG_DOUBLE ? 2 : 3) * L.gpx + L.gw / 2;
}

static void tick_at(float *x, float *y, float *d)
{
    *d = fminf(1.12f * L.gh, L.gw * 0.8f);
    *x = L.gx + 4 * L.gpx + L.gw / 2;
    *y = L.gy + 7 * L.gpy + L.gh / 2;
}

static void draw_box(void)
{
    const brg_pos *p = &G.pos;
    for (int call = 0; call <= BRG_REDOUBLE; call++) {
        float x, y, w;
        cell_at(call, &x, &y, &w);
        int ok = brg_can_call(p, call), on = call == pick;
        const uint8_t *col = on ? BTN_ON : call == BRG_PASS ? BTN_PASS : call == BRG_DOUBLE ? BTN_X
                             : call == BRG_REDOUBLE ? BTN_XX : BTN;
        SDL_Texture *t = ct_round((int)w, (int)L.gh, 0.22f * L.gh, col);
        SDL_SetTextureAlphaMod(t, ok ? 255 : 70);
        ui_put_c(t, x, y, w, L.gh);
        SDL_SetTextureAlphaMod(t, 255);
        if (!ok && call >= 1 && call <= 35) {  /* a bid below the last: faint */
            ui_number(x - 0.29f * L.gh, y, 0.58f * L.gh, BRG_LEVEL(call), 0);
            int s = (int)(0.62f * L.gh);
            SDL_Texture *ic = BRG_STRAIN(call) == BRG_NT ? ct_picture(paint, PIC_NT, s)
                                                        : ct_suit(DECK_FRENCH, brg_suit(BRG_STRAIN(call)), s);
            SDL_SetTextureAlphaMod(ic, 70);
            ui_put_c(ic, x + 0.33f * L.gh, y, (float)s, (float)s);
            SDL_SetTextureAlphaMod(ic, 255);
            continue;
        }
        if (call == BRG_PASS) {
            int s = (int)(0.7f * L.gh);
            ui_put_c(ct_icon(ICON_PASS, s, 255, 255, 255), x, y, (float)s, (float)s);
        } else if (call == BRG_DOUBLE || call == BRG_REDOUBLE) {
            int d = call == BRG_DOUBLE ? 1 : 2, s = (int)(0.62f * L.gh);
            for (int k = 0; k < d; k++) {
                SDL_Texture *ic = ct_icon(ICON_CLOSE, s, 255, 255, 255);
                SDL_SetTextureAlphaMod(ic, ok ? 255 : 60);
                ui_put_c(ic, x + (d == 2 ? (k - 0.5f) * 0.62f * L.gh : 0), y, (float)s, (float)s);
                SDL_SetTextureAlphaMod(ic, 255);
            }
        } else {
            ui_number(x - 0.29f * L.gh, y, 0.58f * L.gh, BRG_LEVEL(call), !on);
            strain_icon(BRG_STRAIN(call), x + 0.33f * L.gh, y, 0.66f * L.gh);
        }
    }
    float tx, ty, td;
    tick_at(&tx, &ty, &td);
    if (pick >= 0) ui_mark_gold(tx, ty, 1.9f * td);
    ct_button(tx, ty, td, pick >= 0, CT_TICK, 0);
}

static int box_hit(float px, float py)          /* the call under (px, py); -1: none */
{
    for (int call = 0; call <= BRG_REDOUBLE; call++) {
        float x, y, w;
        cell_at(call, &x, &y, &w);
        if (fabsf(px - x) <= w / 2 + 0.03f * L.gw && fabsf(py - y) <= L.gh / 2 + 0.06f * L.gh) return call;
    }
    return -1;
}

/* ---- the play -------------------------------------------------------------------------------------------------------- */
static void draw_contract(void)
{
    const brg_pos *p = &G.pos;
    float h = 0.62f * L.kh, cy = L.ky + L.kh / 2;
    float w = contract_at(p->declarer, p->level, p->strain, p->contract_dbl, 0, 0, h, 0);
    const uint8_t panel[4] = {22, 30, 27, 200};
    ui_put(ct_round((int)L.kw, (int)L.kh, 0.3f * L.kh, panel), L.kx, L.ky, L.kw, L.kh);
    contract_at(p->declarer, p->level, p->strain, p->contract_dbl, L.kx + (L.kw - w) / 2, cy, h, 1);
}

static int on_contract(float x, float y)
{
    return G.pos.level && x >= L.kx && x <= L.kx + L.kw && y >= L.ky && y <= L.ky + L.kh;
}

/* each side's tricks in front of its keeper: a small pile of backs and the count */
static void draw_kept(void)
{
    const brg_pos *p = &G.pos;
    for (int side = 0; side < 2; side++) {
        int k = p->keeper[side];
        if (k == BRG_NONE) continue;
        int n = p->won[side] + p->won[side + 2], s = seat(k), left = s == 3 || s == 0;
        float d = L.tok, x = L.cx[s] + (left ? -1.25f : 1.25f) * d, y = L.cy[s];
        cards_pile(DECK_FRENCH, x, y, 0.55f * d, n, CARD_NONE, 0);
        ct_badge(x + (left ? -0.5f : 0.5f) * d, y + 0.3f * d, 0.7f * d, n);
    }
}

static void draw_dummy(int live)
{
    const brg_pos *p = &G.pos;
    if (L.dseat < 0) return;
    int d = brg_dummy(p), mine = live && p->turn == d && brg_controller(p) == L.me;
    if (!brg_dummy_shown(p)) {                  /* before the opening lead: its cards face down */
        cards_pile(DECK_FRENCH, L.C.x + L.C.w / 2, L.C.y + L.C.h / 2, L.dw, p->n[d], CARD_NONE, 0);
        return;
    }
    uint8_t c[BRG_HAND];
    float x[BRG_HAND], y[BRG_HAND];
    int n = dummy_spots(p, c, x, y);
    for (int i = 0; i < n; i++) {
        if (ct_hides(&M, TAG_DUMMY, c[i], -1)) continue;
        if (drag.on && drag.moved && drag.card == c[i]) continue;
        int how = 0;
        if (mine) how = !brg_can_play(p, c[i]) ? CD_DIM : c[i] == sel ? CD_RAISED : 0;
        cards_draw(DECK_FRENCH, c[i], x[i], y[i], L.dw, 0, how);
    }
}

static void draw_trick(void)
{
    const brg_pos *p = &G.pos;
    Uint32 now = ui_ms();
    if (win_glow.to && now >= win_glow.from && now < win_glow.to) ui_mark_gold(win_glow.x, win_glow.y, 1.9f * L.tw);
    for (int i = 0; i < p->ntrick; i++) {
        int q = (p->leader + i) % BRG_P, s = seat(q);
        if (ct_hides(&M, TAG_TRICK, q, -1)) continue;
        cards_draw(DECK_FRENCH, p->trick[i], L.sx[s], L.sy[s], L.tw, 0, 0);
    }
}

static void draw_play(int live)
{
    const brg_pos *p = &G.pos;
    int ctl = brg_controller(p), playing = p->phase == BRG_PLAY;
    for (int q = 0; q < BRG_P; q++) {
        int s = seat(q);
        chip(q, L.cx[s], L.cy[s], L.tok, playing && q == p->turn);
    }
    if (!playing) return;
    draw_contract();
    draw_dummy(live);
    draw_kept();
    draw_trick();
    if (sel != CARD_NONE && live && ctl == L.me) {
        int s = seat(p->turn);
        ui_mark_gold(L.sx[s], L.sy[s], 1.6f * L.tw);
        cards_place(DECK_FRENCH, L.sx[s], L.sy[s], L.tw);
    }
}

/* the auction and the last trick over the dimmed table (tap the contract) */
static void draw_info(void)
{
    const brg_pos *p = &G.pos;
    box A = L.A;
    float u = L.u, pw = fminf(A.w * 0.94f, 8.5f * u), ph = A.h * 0.94f;
    float x0 = A.x + (A.w - pw) / 2, y0 = A.y + (A.h - ph) / 2;
    ui_fill(tb.x, tb.y, tb.w, tb.h, 0, 0, 0, 120);
    ct_panel(x0, y0, pw, ph, 255);
    float tw = floorf(fminf(0.7f * L.tw, ph * 0.4f / (2.1f * L.asp))), th = tw * L.asp;
    float trick_h = p->last_winner != BRG_NONE ? 2.2f * th : 0;
    box H = {x0 + 0.2f * u, y0 + 0.2f * u, pw - 0.4f * u, ph - 0.4f * u - trick_h};
    draw_history(H, 0.62f * u);
    if (p->last_winner == BRG_NONE) return;
    float cx = x0 + pw / 2, cy = y0 + ph - 0.2f * u - trick_h / 2;
    for (int i = 0; i < BRG_P; i++) {
        int q = (p->last_leader + i) % BRG_P, s = seat(q);
        float x = cx + (s == 1 ? -1.04f : s == 3 ? 1.04f : 0) * tw, y = cy + (s == 0 ? 0.5f : s == 2 ? -0.5f : 0) * th;
        if (q == p->last_winner) ui_mark_gold(x, y, 1.8f * tw);
        cards_draw(DECK_FRENCH, p->last[i], x, y, tw, 0, q == p->last_winner ? CD_RAISED : 0);
    }
}

/* ---- the score sheet ------------------------------------------------------------------------------------------------- */
enum { ROW_HEAD, ROW_DEAL, ROW_ABOVE, ROW_BELOW, ROW_LINE, ROW_GAME, ROW_TOTAL };
typedef struct { int kind, deal; } srow;

static int sheet_rows(srow *r)
{
    const brg_pos *p = &G.pos;
    int n = 0;
    r[n++] = (srow){ROW_HEAD, 0};
    if (p->variant == BRG_CHICAGO) {
        for (int i = 0; i < p->nrec; i++) r[n++] = (srow){ROW_DEAL, i};
    } else {
        for (int i = p->nrec - 1; i >= 0; i--)
            if (p->rec[i].above[0] || p->rec[i].above[1]) r[n++] = (srow){ROW_ABOVE, i};
        r[n++] = (srow){ROW_LINE, 0};
        for (int i = 0; i < p->nrec; i++) {
            if (p->rec[i].below[0] || p->rec[i].below[1]) r[n++] = (srow){ROW_BELOW, i};
            if (p->rec[i].game && !p->rec[i].rubber) r[n++] = (srow){ROW_GAME, i};
        }
    }
    r[n++] = (srow){ROW_TOTAL, 0};
    return n;
}

static void two_chips(int side, float cx, float cy, float d)
{
    int s = (int)d;
    ui_put_c(ct_token(side, s), cx - 0.42f * d, cy - 0.1f * d, (float)s, (float)s);
    ui_put_c(ct_token(side + 2, s), cx + 0.42f * d, cy + 0.1f * d, (float)s, (float)s);
}

static void draw_sheet(void)
{
    const brg_pos *p = &G.pos;
    box b = tb;
    float u = L.u;
    if (p->phase == BRG_SCORE && p->passed_out) {   /* passed out: each one's pass; tap: the cards dealt again */
        float pw = fminf(b.w * 0.9f, 6.5f * u), ph = 1.7f * u, x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
        ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 80);
        ct_panel(x0, y0, pw, ph, 255);
        for (int q = 0; q < BRG_P; q++) {
            float x = x0 + pw * (q + 0.5f) / 4, y = y0 + ph / 2, d = 0.7f * u;
            ui_put_c(ct_token(q, (int)d), x - 0.3f * d, y, d, d);
            ct_pass_badge(x + 0.45f * d, y + 0.2f * d, 0.6f * d);
        }
        return;
    }
    srow r[2 * BRG_DEALS + 8];
    int n = sheet_rows(r);
    float pw = fminf(b.w * 0.96f, 9.0f * u), avail = b.h * 0.92f - 0.5f * u;
    float rh = floorf(fminf(1.0f * u, avail / n));
    if (rh < 0.42f * u) {                       /* too many: the oldest rows above the line go */
        rh = floorf(0.42f * u);
        int keep = (int)(avail / rh), drop = n - keep, k = 0;
        for (int i = 0; i < n; i++) {
            if (drop > 0 && r[i].kind == ROW_ABOVE) {
                int last_above = 1;
                for (int j = i + 1; j < n; j++) last_above &= r[j].kind != ROW_ABOVE;
                if (!last_above) { drop--; continue; }
            }
            r[k++] = r[i];
        }
        n = k;
    }
    float ph = n * rh + 0.5f * u, x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    endt_begin(b, pw, ph, 80, &x0, &y0);   /* over the table, or in the end card */
    int we = brg_side(L.me), they = we ^ 1;
    float c_no = x0 + 0.06f * pw, c_k = x0 + 0.12f * pw, c_we = x0 + 0.68f * pw, c_they = x0 + 0.88f * pw;
    float y = y0 + 0.25f * u, nh = fminf(0.42f * rh, 0.4f * u);
    ui_fill(x0 + 0.78f * pw, y0 + 0.25f * u + 0.15f * rh, fmaxf(1, 0.02f * u), ph - 0.5f * u - 0.3f * rh, 246, 238,
            220, 40);
    for (int i = 0; i < n; i++, y += rh) {
        float cy = y + rh / 2;
        const brg_deal *d = &p->rec[r[i].deal];
        switch (r[i].kind) {
        case ROW_HEAD: {
            float cd = fminf(0.62f * rh, 0.6f * u);
            two_chips(we, c_we, cy, cd);
            two_chips(they, c_they, cy, cd);
            ct_rule(x0 + 0.3f * u, y + rh, pw - 0.6f * u);
            break;
        }
        case ROW_LINE:                          /* the line: below it the trick points */
            ui_fill(x0 + 0.2f * u, cy - fmaxf(1, 0.03f * u), pw - 0.4f * u, fmaxf(2, 0.06f * u), 246, 238, 220, 170);
            break;
        case ROW_GAME:
            ct_rule(c_k, cy, pw - (c_k - x0) - 0.2f * u);
            break;
        case ROW_TOTAL: {
            ct_rule(x0 + 0.3f * u, y + 0.08f * rh, pw - 0.6f * u);
            int lead = p->total[they] > p->total[we] ? they : we;
            if (p->total[0] != p->total[1]) ui_mark_gold(lead == we ? c_we : c_they, cy, 2.4f * nh);
            ui_number_signed(c_we, cy, 1.15f * nh, p->total[we], 1, 0);
            ui_number_signed(c_they, cy, 1.15f * nh, p->total[they], 1, 0);
            break;
        }
        default: {                              /* a deal: its number (Chicago), its contract and result, the points */
            float h = fminf(0.6f * rh, 0.55f * u), x = c_k;
            if (r[i].kind == ROW_DEAL) ui_number(c_no, cy, 0.8f * nh, r[i].deal + 1, 1);
            else x = x0 + 0.25f * u;
            x += contract_at(d->declarer, d->level, d->strain, d->dbl, x, cy, h, 1) + 0.25f * h;
            int res = d->taken - 6 - d->level;
            if (res == 0) ct_pic(CT_TICK, x + 0.4f * h, cy, 0.8f * h, 0, 246, 238, 220, 255);
            else ui_number_signed(x + ct_number_w(res, 0.7f * h, 1) / 2, cy, 0.7f * h, res, 1, 1);
            for (int k = 0; k < 2; k++) {
                int side = k ? they : we;
                float cx = k ? c_they : c_we;
                int v = r[i].kind == ROW_ABOVE ? d->above[side] : r[i].kind == ROW_BELOW ? d->below[side]
                                                                                          : d->below[side] + d->above[side];
                if (r[i].kind == ROW_DEAL && d->vul >> side & 1) {   /* vulnerable on that deal: a red ring */
                    int s = (int)(0.5f * rh);
                    ui_put_c(ct_picture(paint, PIC_VUL, s), cx - 0.5f * (c_they - c_we) + 0.36f * rh, cy, (float)s,
                             (float)s);
                }
                if (r[i].kind == ROW_ABOVE && d->rubber == side + 1) ui_mark_gold(cx, cy, 2.2f * nh);
                if (v) ui_number(cx, cy, nh, v, 1);
            }
            break;
        }
        }
    }
    endt_done();
}

/* ---- drawing --------------------------------------------------------------------------------------------------------- */
static void draw(void)
{
    geo();
    const brg_pos *p = &G.pos;
    int hide = ui.viewer < 0, live = !ui_handing() && !busy(), ph = p->phase;
    if (ph == BRG_AUCTION && !hide && !ui_handing() && !(seen >> L.me & 1)) {
        seen |= 1u << L.me;
        deal_in();
    }
    if (ph == BRG_AUCTION) {
        draw_history(L.H, 0.62f * L.u);
        if (!hide && p->turn == L.me && !ui_handing() && !ct_hides(&M, TAG_HAND, -1, -1)) draw_box();   /* once dealt */
    } else draw_play(live);
    /* the viewer's hand (under the hand-off cover: face down) */
    ct_rows h;
    hand_rows(p, &h);
    int mine = ph == BRG_PLAY && live && brg_controller(p) == L.me, own = p->turn == L.me;
    for (int i = 0; i < h.n; i++) {
        int c = p->hand[L.me][i];
        if (ct_hides(&M, TAG_HAND, c, -1)) continue;
        if (hide) {
            cards_draw(DECK_FRENCH, CARD_NONE, h.spot[i].x, h.spot[i].y, h.w, h.spot[i].deg, CD_DOWN);
            continue;
        }
        if (drag.on && drag.moved && drag.card == c) continue;
        int how = 0;
        if (mine) how = !own || !brg_can_play(p, c) ? CD_DIM : c == sel ? CD_RAISED : 0;
        cards_draw(DECK_FRENCH, c, h.spot[i].x, h.spot[i].y, h.w, h.spot[i].deg, how);
    }
    ct_draw(&M);
    if (drag.on && drag.moved && !hide)
        cards_draw(DECK_FRENCH, drag.card, drag.x, drag.y, drag.dummy ? L.dw * 1.1f : h.w * 1.04f, 0, CD_RAISED);
    if (info && !hide) draw_info();
    if ((ph == BRG_SCORE || ph == BRG_OVER) && !busy()) draw_sheet();
}

/* ---- the pointer ----------------------------------------------------------------------------------------------------- */
static int on_target(float x, float y)
{
    int s = seat(G.pos.turn);
    return cards_in(L.sx[s], L.sy[s], L.tw * 1.2f, L.asp, x, y);
}

static void press(float x, float y)
{
    geo();
    const brg_pos *p = &G.pos;
    drag.on = 0;
    if (info) { info = 0; return; }
    if (busy()) return;
    if (p->phase == BRG_SCORE) { act(BRG_ANEXT); return; }
    if (p->phase == BRG_OVER) return;
    if (p->phase == BRG_PLAY && on_contract(x, y)) { info = 1; ui_sound(SND_TAP); return; }
    if (brg_controller(p) != L.me) { ui_sound(SND_DENY); return; }
    if (p->phase == BRG_AUCTION) {
        float tx, ty, td;
        tick_at(&tx, &ty, &td);
        if (ct_on(tx, ty, td, x, y)) {
            if (pick >= 0) act(BRG_ACALL | pick);
            else ui_sound(SND_DENY);
            return;
        }
        int call = box_hit(x, y);
        if (call < 0) { pick = -1; return; }
        if (!brg_can_call(p, call)) { ui_sound(SND_DENY); return; }
        if (call == pick) { act(BRG_ACALL | call); return; }
        pick = call;
        ui_sound(SND_TAP);
        return;
    }
    int from_dummy = p->turn == brg_dummy(p), c = CARD_NONE;
    if (from_dummy) c = dummy_at(p, x, y);
    else {
        ct_rows h;
        hand_rows(p, &h);
        int i = ct_rows_hit(&h, x, y);
        if (i >= 0) c = p->hand[L.me][i];
    }
    if (c == CARD_NONE) {
        if (sel != CARD_NONE && on_target(x, y)) act(BRG_APLAY | sel);
        else if (sel != CARD_NONE) sel = CARD_NONE;
        else ui_sound(SND_DENY);
        return;
    }
    if (!brg_can_play(p, c)) { ui_sound(SND_DENY); return; }
    if (c == sel) { act(BRG_APLAY | c); return; }
    sel = c;
    ui_sound(SND_TAP);
    drag.on = 1, drag.card = c, drag.moved = 0, drag.dummy = from_dummy;
    drag.x0 = drag.x = x, drag.y0 = drag.y = y;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (!drag.moved && hypotf(x - drag.x0, y - drag.y0) > 0.25f * L.u) drag.moved = 1;
    drag.x = x, drag.y = y;
    if (drag.moved) ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    geo();
    int c = drag.card;
    drag.x = x, drag.y = y;
    if (drag.moved && G.pos.phase == BRG_PLAY) {
        box D = L.C;
        int away = drag.dummy ? !(x >= D.x && x <= D.x + D.w && y >= D.y && y <= D.y + D.h) : y < L.hand_top;
        if (away) { act(BRG_APLAY | c); return; }
    }
    drag.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* a player's bar: a red ring when their side is vulnerable, the rubber's part-score (below the line, this game) in a
 * dark disc, the side's total */
static void bar(int player, box b)
{
    const brg_pos *p = &G.pos;
    float u = ui.unit, cy = b.y + b.h / 2, x = b.x, h = fminf(0.46f * u, 0.5f * b.h);
    int side = brg_side(player);
    if (brg_vulnerable(p, side) && p->phase != BRG_OVER) {
        int s = (int)(0.5f * u);
        ui_put_c(ct_picture(paint, PIC_VUL, s), x + 0.25f * u, cy, (float)s, (float)s);
        x += 0.62f * u;
    }
    if (p->variant == BRG_RUBBER && p->part[side] > 0 && p->phase != BRG_OVER) {
        ct_badge(x + 0.3f * u, cy, 0.6f * u, p->part[side]);
        x += 0.75f * u;
    }
    float w = ct_number_w(p->total[side], 1, 0), hs = fminf(h, (b.x + b.w - x) / w * 0.95f);
    if (hs > 0) ui_number_signed(x + w * hs / 2, cy, hs, p->total[side], 1, 0);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card and Chicago's: the four aces (four deals); the rubber's: the five honours of spades */
static void thumb(int v, int size, uint8_t *rgba)
{
    static const int aces[4] = {CARD(SUIT_CLUBS, RANK_ACE), CARD(SUIT_DIAMONDS, RANK_ACE), CARD(SUIT_HEARTS, RANK_ACE),
                                CARD(SUIT_SPADES, RANK_ACE)},
                     honours[5] = {CARD(SUIT_SPADES, 10), CARD(SUIT_SPADES, RANK_JACK), CARD(SUIT_SPADES, RANK_QUEEN),
                                   CARD(SUIT_SPADES, RANK_KING), CARD(SUIT_SPADES, RANK_ACE)};
    if (v == BRG_RUBBER) ct_thumb(DECK_FRENCH, honours, 5, size, rgba);
    else ct_thumb(DECK_FRENCH, aces, 4, size, rgba);
}

static const game bridge_game = {
    .id = "bridge", .order = 327, .variants = BRG_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(bridge_game)
