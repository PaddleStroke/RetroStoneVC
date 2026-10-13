/*
 * Scopa in RetroStoneBoard: 2 players, or 4 in two partnerships, passing the device round, each hand secret
 * (VIEW_HIDDEN; the rules: scopa_rules.c, scopa.h). The Italian pack (the card kit's DECK_ITALIAN). The table: the
 * cards face up in the middle, the viewer's hand at the bottom with the stock on its left and their captures on its
 * right, the others at the top (their cards face down, their captures; partners: the middle seat). The captures are a
 * pile face down, each scopa's card across it face up. Playing: tap a card (it rises; the cards it can take glow), then
 * tap it again, or the table, or drag it there; when it could take more than one set, tap the cards to take until they
 * add up. The played card flies onto what it takes, they glow a moment and go to the player's pile. After each round a
 * table of icons and digits: cards, coins, the settebello, the primiera, the scopas, the round's points, the total.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards_table.h"
#include "games/scopa.h"
#include "games/endtable.h"
#include "shell.h"

#define SLIDE_MS 300
#define LOOK_MS 560                             /* the cards taken glow a moment before they go */
#define TAKE_MS 400
#define DEAL_MS 240
#define ASP cards_aspect(DECK_ITALIAN)

enum { H_NONE, H_HAND, H_TABLE, H_PILE, H_SEAT };

static scgame G;
static ct_moves MV = {.rest = 300};
static int floor_n, dealing, preview;   /* preview: the table made room for a card that takes nothing */
static struct {                                 /* the viewer's finger and choice */
    int card, on, raised;                       /* the card touched, dragged far enough; the card risen (-1) */
    uint32_t picks;                             /* the table cards picked so far (several captures possible) */
    float x0, y0, x, y;
} F = {-1, 0, -1, 0, 0, 0, 0, 0};
static struct { Uint32 t0, t1; int n; float x[SCOPA_TABLE], y[SCOPA_TABLE], w; } glow;   /* the cards being taken */
static struct { Uint32 t0, t1; int pl; } flash;    /* a scopa: its pile in gold light a moment as its card lands */

/* ---- the table's geometry ---------------------------------------------------------------------------------------- */
static struct {
    box b;
    int me, wide;
    float wh, hx, hy, hand_top;                 /* the viewer's hand: card width, centre */
    float wp, stx, sty;                         /* the piles' card width; the stock */
    float px[SCOPA_MAXP], py[SCOPA_MAXP];       /* each player's captures */
    float sw, sx[SCOPA_MAXP], sy, tok, ty;      /* the others' seats: their backs' width, centres; tokens */
    box ta;                                     /* the table's area */
    float wmax;                                 /* the table's cards at most this wide */
} T;

static void layout(box b)
{
    const scpos *p = &G.pos;
    float u = ui.unit;
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= p->players) T.me = 0;
    T.wide = b.w > b.h * 1.05f;
    int k = p->players - 1;
    /* the hand, the stock on its left, the captures on its right */
    T.wh = floorf(fminf(b.w * (T.wide ? 0.13f : 0.19f), b.h * (T.wide ? 0.2f : 0.15f)));
    T.wp = floorf(0.6f * T.wh);
    T.hy = floorf(b.y + b.h - T.wh * ASP / 2 - 0.12f * T.wh);
    T.hx = b.x + b.w / 2;
    T.hand_top = T.hy - T.wh * ASP / 2 - 0.2f * T.wh;
    float side = T.wide ? 3.4f * T.wh : (b.w / 2 - 1.62f * T.wh) / 2 + 1.62f * T.wh;
    T.stx = T.hx - side;
    T.sty = T.hy;
    T.px[T.me] = T.hx + side;
    T.py[T.me] = T.hy - 0.1f * T.wp;
    /* the others along the top, in turn order from the left */
    float hs = b.h * (T.wide ? 0.25f : 0.2f), cell = b.w / k;
    T.sw = floorf(fminf(fminf(cell * 0.2f, hs * 0.56f / ASP), 0.9f * T.wh));
    T.sy = b.y + hs * 0.42f;
    T.tok = floorf(fminf(0.5f * u, 0.5f * T.sw));
    T.ty = T.sy + T.sw * ASP / 2 + 0.75f * T.tok;
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) continue;
        float cx = b.x + cell * (ct_seat(q, T.me, p->players) + 0.5f);
        T.sx[q] = cx - fminf(0.24f * cell, 1.4f * T.sw);
        T.px[q] = cx + fminf(0.25f * cell, 1.5f * T.sw);
        T.py[q] = T.sy;
    }
    T.ta = (box){b.x + 0.02f * b.w, b.y + hs, b.w * 0.96f, T.hand_top - (b.y + hs)};
    T.wmax = floorf(fminf(1.0f * T.wh, T.ta.h / (ASP * 1.12f)));
}

/* the table's n cards: their centres and their width (one, two or three rows, the largest that fit) */
static float table_spots(int n, float *x, float *y)
{
    box a = T.ta;
    float best = 0;
    int rows = 1;
    for (int r = 1; r <= 3; r++) {
        int cols = (n + r - 1) / r;
        if (cols < 1) cols = 1;
        float w = fminf(fminf(a.w / (cols * 1.1f), a.h / (r * ASP * 1.08f)), T.wmax);
        if (w > best * 1.02f) best = w, rows = r;
    }
    best = floorf(best);
    int cols = n > 0 ? (n + rows - 1) / rows : 1;
    for (int i = 0; i < n; i++) {
        int r = i / cols, c = i % cols, in_row = r < rows - 1 ? cols : n - r * cols;
        x[i] = floorf(a.x + a.w / 2 + (c - (in_row - 1) / 2.0f) * best * 1.1f);
        y[i] = floorf(a.y + a.h / 2 + (r - (rows - 1) / 2.0f) * best * ASP * 1.08f);
    }
    return best;
}

/* the viewer's hand of n: centres, angles */
static void hand_spots(int n, cards_spot *s)
{
    cards_fan(T.hx, T.hy, T.wh * (1 + 1.08f * (n > 1 ? n - 1 : 0)), T.wh, ASP, n, n > 1 ? 5 : 0, s);
}

/* where a player's card in hand starts from (the viewer: its spot; the others: their seat) */
static void hand_from(const scpos *b, int pl, int card, float *x, float *y, float *w, float *deg)
{
    if (pl == T.me) {
        cards_spot s[SCOPA_HAND];
        hand_spots(b->n[pl], s);
        int i = 0;
        while (i < b->n[pl] && b->hand[pl][i] != card) i++;
        if (i == b->n[pl]) i = 0;
        *x = s[i].x, *y = s[i].y - (F.raised == card ? 0.14f * T.wh : 0), *w = T.wh, *deg = s[i].deg;
        if (F.on && F.card == card) *x = F.x, *y = F.y, *deg = 0;
    } else ct_backs_end(DECK_FRENCH, T.sx[pl], T.sy, 2.4f * T.sw, T.sw, b->n[pl], x, y, deg), *w = T.sw;
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
static int busy(void) { return ct_busy(&MV); }

static void calm(void)
{
    ct_calm(&MV);
    glow.n = 0;
    flash.t1 = 0;
}

static int slide(int card, float x0, float y0, float w0, float a0, int up0, float x1, float y1, float w1, float a1,
                 int up1, int ms, int delay, int tag, int a, int b, int sound)
{
    return ct_slide(&MV, DECK_ITALIAN, card, x0, y0, w0, a0, up0, x1, y1, w1, a1, up1, ms, delay, tag, a, b, sound);
}

/* the hands dealt: from the stock to each hand, three each from the player after the dealer, after `delay` */
static int deal_hands(int delay)
{
    const scpos *p = &G.pos;
    int k = 0;
    for (int j = 0; j < p->players; j++) {
        int q = (p->dealer + 1 + j) % p->players;
        cards_spot s[SCOPA_HAND];
        hand_spots(SCOPA_HAND, s);
        for (int i = 0; i < SCOPA_HAND; i++, k++) {
            float x, y, deg, w = T.sw;
            if (q == T.me) x = s[i].x, y = s[i].y, deg = s[i].deg, w = T.wh;
            else ct_backs_end(DECK_FRENCH, T.sx[q], T.sy, 2.4f * T.sw, T.sw, i + 1, &x, &y, &deg);
            slide(p->hand[q][i], T.stx, T.sty, T.wp, 0, 0, x, y, w, deg, 0, DEAL_MS, delay + 70 * k,
                  q == T.me ? H_HAND : H_SEAT, q, q == T.me ? i : -1, k % 3 == 0 ? SND_MOVE : -1);
        }
    }
    return delay + 70 * k + DEAL_MS;
}

static void deal_round(void)
{
    const scpos *p = &G.pos;
    int t = deal_hands(0);
    float x[SCOPA_TABLE], y[SCOPA_TABLE], w = table_spots(p->ntable, x, y);
    for (int i = 0; i < p->ntable; i++)
        slide(p->table[i], T.stx, T.sty, T.wp, 0, 0, x[i], y[i], w, 0, 1, DEAL_MS + 80, t + 90 * i, H_TABLE, i, 0,
              SND_PLACE);
}

/* the pile's card width and where a card lands on it (a scopa: across it, below the ones before) */
static void pile_at(int pl, int scopa, int nth, float *x, float *y, float *w)
{
    *w = pl == T.me ? T.wp : T.sw;
    *x = T.px[pl];
    *y = T.py[pl] + (scopa ? (0.12f * nth - 0.1f) * *w : 0);
}

/* what an action moved, from the position before it */
static void animate(const scpos *b, uint32_t a)
{
    const scpos *p = &G.pos;
    if (a == SCA_NEXT) { deal_round(); return; }
    int pl = b->turn, card = SCA_CARD(a);
    uint32_t take = SCA_TAKE(a);
    float x0, y0, w0, d0, ox[SCOPA_TABLE], oy[SCOPA_TABLE], nx[SCOPA_TABLE], ny[SCOPA_TABLE];
    hand_from(b, pl, card, &x0, &y0, &w0, &d0);
    float wo = table_spots(b->ntable, ox, oy), wn;
    int up0 = pl == T.me, t, sweep_after[SCOPA_TABLE];
    for (int i = 0; i < SCOPA_TABLE; i++) sweep_after[i] = -1;
    if (!take) {                                /* laid on the table; the others make room */
        wn = table_spots(b->ntable + 1, nx, ny);
        sweep_after[b->ntable] = slide(card, x0, y0, w0, d0, up0, nx[b->ntable], ny[b->ntable], wn, 0, 1, SLIDE_MS, 0,
                                       H_TABLE, b->ntable, 0, SND_PLACE);
        for (int i = 0; i < b->ntable && !preview; i++)   /* (shown making room already: they stay) */
            if (ox[i] != nx[i] || oy[i] != ny[i] || wo != wn)
                sweep_after[i] = slide(b->table[i], ox[i], oy[i], wo, 0, 1, nx[i], ny[i], wn, 0, 1, SLIDE_MS, 0,
                                       H_TABLE, i, 0, -1);
        t = SLIDE_MS;
        memcpy(ox, nx, sizeof ox);
        memcpy(oy, ny, sizeof oy);
        wo = wn;
    } else {                                    /* onto the first card it takes; they glow; all to the pile */
        int k0 = 0;
        while (!(take >> k0 & 1)) k0++;
        float mx = ox[k0] + 0.16f * wo, my = oy[k0] - 0.2f * wo * ASP;
        int s1 = slide(card, x0, y0, w0, d0, up0, mx, my, wo, 0, 1, SLIDE_MS, 0, H_NONE, 0, 0, SND_PLACE);
        glow.t0 = ui_ms() + SLIDE_MS;
        glow.t1 = glow.t0 + LOOK_MS;
        glow.n = 0;
        glow.w = wo;
        for (int k = 0; k < b->ntable; k++)
            if (take >> k & 1) glow.x[glow.n] = ox[k], glow.y[glow.n] = oy[k], glow.n++;
        int delay = SLIDE_MS + LOOK_MS, scopa = p->lscopa, nth = 0;
        float px, py, pw;
        pile_at(pl, 0, 0, &px, &py, &pw);
        for (int k = 0; k < b->ntable; k++)
            if (take >> k & 1) {
                slide(b->table[k], ox[k], oy[k], wo, 0, 1, px, py, pw, 0, 0, TAKE_MS, delay + 50 * nth, H_PILE, pl, 0,
                      nth == 0 ? SND_CAPTURE : -1);
                nth++;
            }
        if (scopa) pile_at(pl, 1, p->scope[pl] - 1, &px, &py, &pw);
        int s2 = slide(card, mx, my, wo, 0, 1, px, py, pw, scopa ? 90 : 0, scopa, TAKE_MS, delay + 50 * nth, H_PILE,
                       pl, scopa, scopa ? SND_CHECK : -1);
        ct_after(&MV, s2, s1);
        if (scopa) {
            flash.pl = pl;
            flash.t0 = ui_ms() + (Uint32)(delay + 50 * nth + TAKE_MS - 60);
            flash.t1 = flash.t0 + 900;
            if (MV.quiet < flash.t1) MV.quiet = flash.t1;
        }
        t = delay + 50 * nth + TAKE_MS;
        int left = 0;                           /* the rest close up (unless the round ends: they are swept) */
        for (int k = 0; k < b->ntable; k++)
            if (!(take >> k & 1)) ox[left] = ox[k], oy[left] = oy[k], left++;
        if (!p->nswept && left) {
            wn = table_spots(left, nx, ny);
            for (int i = 0; i < left; i++)
                if (ox[i] != nx[i] || oy[i] != ny[i] || wo != wn)
                    slide(p->table[i], ox[i], oy[i], wo, 0, 1, nx[i], ny[i], wn, 0, 1, SLIDE_MS, t - 120, H_TABLE, i,
                          0, -1);
            t += SLIDE_MS - 120;
        }
    }
    if (p->deals > b->deals && p->phase == SC_PLAY) deal_hands(t + 150);
    if (p->nswept) {                            /* the round's end: the table's last cards to the last taker */
        float px, py, pw;
        pile_at(p->sweeper, 0, 0, &px, &py, &pw);
        for (int i = 0; i < p->nswept; i++) {
            int s = slide(p->swept[i], ox[i], oy[i], wo, 0, 1, px, py, pw, 0, 0, TAKE_MS, t + 350 + 70 * i, H_PILE,
                          p->sweeper, 0, i == 0 ? SND_CAPTURE : -1);
            ct_after(&MV, s, sweep_after[i]);
        }
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void reset_ui(void)
{
    F.card = F.raised = -1;
    F.on = 0;
    F.picks = 0;
}

static void start(int v)
{
    (void)v;
    scopa_new(&G, ui.players, (uint32_t)ui_random(1 << 30));
    calm();
    reset_ui();
    floor_n = 0;
    dealing = 1;
}

static int status(void)
{
    int w = scopa_winner(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static unsigned winners(void)                   /* the winning side: both partners */
{
    int w = scopa_winner(&G.pos);
    if (w < 0) return 0;
    return G.pos.players == 4 ? 1u << w | 1u << (w + 2) : 1u << w;
}

static int turn(void) { return G.pos.turn; }

static void act(uint32_t a)
{
    scpos before = G.pos;
    if (!scopa_play(&G, a)) { ui_sound(SND_DENY); return; }
    calm();
    if (T.me != ui_bottom()) layout(T.b);
    animate(&before, a);
    reset_ui();
    floor_n = G.n;
    if (G.pos.phase == SC_PLAY) ui_handoff(G.pos.turn);
}

const scgame *scopa_live(void) { return &G; }

int scopa_show(const char *move)
{
    static scgame t;
    t = G;
    if (!scopa_script(&t, move) || t.n != G.n + 1) return 0;
    act(t.act[G.n]);
    return 1;
}

static int undo(void)
{
    if (F.raised >= 0) { reset_ui(); return 1; }
    if (G.n <= floor_n || !scopa_undo(&G)) return 0;
    calm();
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = scopa_script(&G, s);
    calm();
    reset_ui();
    dealing = 0;
    floor_n = G.n;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- choosing what a card takes ---------------------------------------------------------------------------------- */
/* the captures of the risen card that contain the picks so far; their number; all their cards (bits) */
static int options(int card, uint32_t picks, uint32_t *all, uint32_t *only)
{
    uint32_t m[64];
    int n = scopa_captures(&G.pos, card, m, 64), k = 0;
    *all = 0;
    for (int i = 0; i < n; i++)
        if ((m[i] & picks) == picks) {
            *all |= m[i];
            *only = m[i];
            k++;
        }
    return k;
}

static int captures(int card)
{
    uint32_t m[64];
    return scopa_captures(&G.pos, card, m, 64);
}

/* the card goes: laid, or taking its only possible set (or the set picked); 0: a choice is still to make */
static int go(int card, uint32_t picks)
{
    uint32_t all, only;
    if (!captures(card)) { act(SCA(card, 0)); return 1; }
    if (options(card, picks, &all, &only) == 1) { act(SCA(card, only)); return 1; }
    return 0;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */

static void draw_pile(int pl)
{
    const scpos *p = &G.pos;
    float x, y, w;
    pile_at(pl, 0, 0, &x, &y, &w);
    if (flash.pl == pl && ui_ms() >= flash.t0 && ui_ms() < flash.t1) {
        ui_mark_gold(x, y, 2.4f * w);
        ui_animate();
    }
    int sc = p->scope[pl] - ct_hides(&MV, H_PILE, pl, 1), n = p->ntook[pl] - ct_hides(&MV, H_PILE, pl, -1);
    for (int k = 0; k < sc; k++) {              /* the scopas: across, under the pile */
        float sx, sy, sw;
        pile_at(pl, 1, k, &sx, &sy, &sw);
        cards_draw(DECK_ITALIAN, p->scopa_card[pl][k], sx, sy, sw, 90, k ? CD_FLAT : 0);
    }
    if (n - sc > 0) cards_pile(DECK_FRENCH, x, y, w, n - sc, CARD_NONE, 0);
    else if (!sc) cards_place(DECK_FRENCH, x, y, w);
}

/* the round's table: a row a side (its tokens), a column a count (icons over them: cards, coins, the settebello, the
 * primiera, the scopas), the counts that won a point in gold light; then the round's points and the total */
static void draw_summary(void)
{
    const scpos *p = &G.pos;
    box b = T.b;
    float u = ui.unit, pw = fminf(b.w * 0.96f, 9.4f * u), rh = fminf(1.25f * u, b.h * 0.2f), hh = 1.1f * rh;
    float ph = hh + 2 * rh + 0.4f * u, x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    endt_begin(b, pw, ph, 80, &x0, &y0);   /* over the table, or in the end card */
    /* the columns: tokens, 5 counts, the round, the total */
    float tw = (p->players == 4 ? 2.1f : 1.0f) * 0.8f * rh, cw = (pw - tw - 0.4f * u) / 7.2f;
    float cx[8], hy = y0 + 0.2f * u + hh / 2, ic = fminf(0.62f * hh, 0.92f * cw);
    cx[0] = x0 + 0.2f * u + tw / 2;
    for (int k = 1; k < 8; k++) cx[k] = x0 + 0.2f * u + tw + cw * (k - 0.5f) + (k >= 6 ? 0.1f * cw * (k - 5) : 0);
    /* the icons: a small pile, a coin, the settebello, the four suits, a card across a pile */
    cards_pile(DECK_FRENCH, cx[1], hy, 0.5f * ic, 6, CARD_NONE, CD_FLAT);
    ui_put_c(ct_suit(DECK_ITALIAN, IT_COINS, (int)ic), cx[2], hy, 0.8f * ic, 0.8f * ic);
    cards_draw(DECK_ITALIAN, CARD(IT_COINS, 7), cx[3], hy, 0.62f * ic, 0, CD_FLAT);
    for (int s = 0; s < 4; s++)
        ui_put_c(ct_suit(DECK_ITALIAN, s, (int)(0.46f * ic)), cx[4] + (s & 1 ? 0.24f : -0.24f) * ic,
                 hy + (s & 2 ? 0.24f : -0.24f) * ic, 0.46f * ic, 0.46f * ic);
    /* a scopa: across, under the pile */
    cards_draw(DECK_ITALIAN, CARD(IT_COINS, 1), cx[5], hy + 0.08f * ic, 0.46f * ic, 90, CD_FLAT);
    cards_pile(DECK_FRENCH, cx[5], hy, 0.34f * ic, 4, CARD_NONE, CD_FLAT);
    ui_fill(x0 + 0.3f * u, y0 + 0.2f * u + hh, pw - 0.6f * u, fmaxf(1, 0.02f * u), 246, 238, 220, 60);
    int lead = p->score[0] == p->score[1] ? -1 : p->score[1] > p->score[0];
    int big = (int)fminf(0.66f * rh, 0.8f * u);
    for (int s = 0; s < 2; s++) {
        float y = y0 + 0.2f * u + hh + (s + 0.5f) * rh, d = (float)big, nh = 0.42f * rh;
        if (s == lead) ui_mark_gold(cx[0], y, 1.6f * tw);
        for (int q = s, k = 0; q < p->players; q += 2, k++)
            ui_put_c(ct_token(q, big), cx[0] + (p->players == 4 ? (k ? 0.24f : -0.24f) * tw : 0), y, d, d);
        for (int k = 0; k < SCORE_KINDS; k++) {
            float x = cx[1 + k];
            if (p->won[s][k]) ui_mark_gold(x, y, 1.5f * fminf(cw, rh));
            if (k == SCORE_SETTEBELLO) {
                if (p->count[s][k]) cards_draw(DECK_ITALIAN, CARD(IT_COINS, 7), x, y, 0.5f * ic, 0, CD_FLAT);
                continue;
            }
            ui_number(x, y, nh, p->count[s][k], 1);
        }
        if (p->rscope[s]) ui_mark_gold(cx[5], y, 1.5f * fminf(cw, rh));
        ui_number(cx[5], y, nh, p->rscope[s], 1);
        ui_number_signed(cx[6], y, nh, p->pts[s], 1, 1);
        ui_number(cx[7], y, 0.56f * rh, p->score[s], 1);
    }
    endt_done();
}

static void draw(void)
{
    const scpos *p = &G.pos;
    if (T.me != ui_bottom() && ui_bottom() >= 0) layout(T.b);
    if (dealing) { dealing = 0; deal_round(); }
    int hide = ui.viewer < 0, live = !hide && !ui_handing() && !busy() && p->phase == SC_PLAY && p->turn == T.me;
    float u = ui.unit;
    int ts = (int)fmaxf(4, T.tok);
    /* the stock, the piles, the others' hands */
    if (p->nstock) cards_pile(DECK_FRENCH, T.stx, T.sty, T.wp, p->nstock, CARD_NONE, 0);
    for (int q = 0; q < p->players; q++) {
        draw_pile(q);
        if (q == T.me) continue;
        int n = p->n[q] - ct_hides(&MV, H_SEAT, q, -1);
        if (n > 0) ct_backs(DECK_FRENCH, T.sx[q], T.sy, 2.4f * T.sw, T.sw, n, 0);
        if (p->turn == q && p->phase == SC_PLAY) ui_mark_gold(T.sx[q], T.ty, 2.2f * T.tok);
        ui_put_c(ct_token(q, ts), T.sx[q], T.ty, T.tok, T.tok);
    }
    /* the table; a card that takes nothing, risen or dragged over the table: the others make room for it */
    int lone = F.on && F.card >= 0 ? (F.y < T.hand_top ? F.card : -1) : F.raised;
    preview = live && lone >= 0 && !captures(lone);
    float x[SCOPA_TABLE], y[SCOPA_TABLE], w = table_spots(p->ntable + preview, x, y);
    uint32_t all = 0, only = 0;
    int held = F.on && F.card >= 0 ? F.card : F.raised;   /* the cards it can take glow */
    int nopt = held >= 0 && live ? options(held, held == F.raised ? F.picks : 0, &all, &only) : 0;
    Uint32 now = ui_ms();
    if (glow.n && now >= glow.t0 && now < glow.t1) {
        for (int i = 0; i < glow.n; i++) ui_mark_gold(glow.x[i], glow.y[i], 1.5f * glow.w * ASP);
        ui_animate();
    }
    for (int i = 0; i < p->ntable; i++) {
        if (ct_hides(&MV, H_TABLE, i, -1)) continue;
        int how = F.picks >> i & 1 ? CD_RAISED : nopt && (all >> i & 1) ? CD_GLOW : 0;
        cards_draw(DECK_ITALIAN, p->table[i], x[i], y[i], w, 0, how);
    }
    if (preview) {                              /* where it would lie */
        ui_mark_gold(x[p->ntable], y[p->ntable], 1.3f * w * ASP);
        cards_place(DECK_FRENCH, x[p->ntable], y[p->ntable], w);
    }
    /* the viewer's hand */
    cards_spot s[SCOPA_HAND];
    int n = p->n[T.me];
    hand_spots(n, s);
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][i];
        if (ct_hides(&MV, H_HAND, T.me, i) || (F.on && F.card == c)) continue;
        int how = hide ? CD_DOWN : c == F.raised ? CD_RAISED : live && F.raised < 0 ? CD_GLOW : 0;
        cards_draw(DECK_ITALIAN, c, s[i].x, s[i].y, T.wh, s[i].deg, how);
    }
    ct_draw(&MV);
    if (F.on && F.card >= 0 && !hide) cards_draw(DECK_ITALIAN, F.card, F.x, F.y, T.wh, 0, CD_RAISED);
    (void)u;
    if ((p->phase == SC_ROUND || p->phase == SC_OVER) && !busy()) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int table_at(float px, float py)
{
    float x[SCOPA_TABLE], y[SCOPA_TABLE], w = table_spots(G.pos.ntable, x, y);
    for (int i = G.pos.ntable - 1; i >= 0; i--)
        if (cards_in(x[i], y[i], w * 1.04f, ASP, px, py)) return i;
    return -1;
}

static int hand_at(float px, float py)
{
    const scpos *p = &G.pos;
    cards_spot s[SCOPA_HAND];
    hand_spots(p->n[T.me], s);
    for (int i = 0; i < p->n[T.me]; i++)        /* the risen card first: it stands out */
        if (p->hand[T.me][i] == F.raised && cards_hit(&s[i], 1, T.wh, ASP, px, py + 0.14f * T.wh) >= 0) return F.raised;
    int i = cards_hit(s, p->n[T.me], T.wh, ASP, px, py);
    return i < 0 ? -1 : p->hand[T.me][i];
}

/* a table card tapped while a card is risen: picked or let go; the capture goes once the picks make one */
static void pick(int i)
{
    uint32_t all, only, m = F.picks ^ 1u << i;
    if (F.picks >> i & 1) { F.picks = m; ui_sound(SND_TAP); return; }
    if (!options(F.raised, m, &all, &only)) { ui_sound(SND_DENY); return; }
    F.picks = m;
    if (options(F.raised, m, &all, &only) == 1 && only == m) act(SCA(F.raised, m));
    else ui_sound(SND_TAP);
}

static void press(float x, float y)
{
    const scpos *p = &G.pos;
    F.card = -1;
    F.on = 0;
    if (busy()) return;
    if (p->phase == SC_ROUND) { act(SCA_NEXT); return; }
    if (p->phase != SC_PLAY) return;
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    int c = hand_at(x, y);
    if (c >= 0) {
        F.card = c;
        F.x0 = F.x = x;
        F.y0 = F.y = y;
        return;
    }
    if (F.raised < 0) { ui_sound(SND_DENY); return; }
    int t = table_at(x, y);
    if (t >= 0 && captures(F.raised)) { pick(t); return; }
    int in_table = y >= T.ta.y && y < T.hand_top;
    if ((t >= 0 || in_table) && go(F.raised, F.picks)) return;
    if (t >= 0 || in_table) { ui_sound(SND_DENY); return; }
    reset_ui();
}

static void motion(float x, float y)
{
    if (F.card < 0) return;
    F.x = x;
    F.y = y;
    if (!F.on && hypotf(x - F.x0, y - F.y0) > 0.25f * ui.unit) F.on = 1;
}

static void release(float x, float y)
{
    int c = F.card;
    if (c < 0) return;
    motion(x, y);
    F.card = -1;
    if (F.on) {                                 /* dropped: on the table, on a card or not */
        if (y >= T.hand_top) { F.on = 0; return; }
        int t = table_at(x, y);
        F.raised = c;
        F.picks = 0;
        if (t >= 0 && captures(c)) {
            uint32_t all, only;
            if (options(c, 1u << t, &all, &only)) {
                F.card = c;                     /* it slides on from the finger */
                F.on = 1;
                F.x = x, F.y = y;
                pick(t);
                F.on = 0;
                F.card = -1;
                return;
            }
        }
        F.card = c;
        F.on = 1;
        F.x = x, F.y = y;
        int gone = go(c, 0);
        F.on = 0;
        F.card = -1;
        if (!gone) ui_sound(SND_TAP);           /* a choice to make: it stays risen */
        return;
    }
    if (F.raised != c) {                        /* a tap: it rises */
        F.raised = c;
        F.picks = 0;
        ui_sound(SND_TAP);
        return;
    }
    if (!go(c, F.picks)) ui_sound(SND_DENY);    /* tapped again: it goes, if nothing is left to choose */
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the side's total */
{
    int v = G.pos.score[scopa_side(&G.pos, player)];
    float h = fminf(0.46f * ui.unit, b.h * 0.7f), w = (v >= 10 ? 1.36f : 0.62f) * h;
    ui_number(b.x + w / 2, b.y + b.h / 2, h, v, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the table, the settebello coming down on a 3 and a 4 */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int cards[4] = {CARD(IT_CUPS, 3), CARD(IT_CLUBS, 4), CARD(IT_SWORDS, IT_RE), CARD(IT_COINS, 7)};
    static const float at[4][3] = {{0.27f, 0.62f, -6}, {0.5f, 0.64f, 3}, {0.73f, 0.6f, 8}, {0.42f, 0.38f, -14}};
    memset(rgba, 0, (size_t)size * size * 4);
    for (int i = 0; i < 4; i++)
        cards_paint_turned(rgba, size, DECK_ITALIAN, cards[i], at[i][0] * size, at[i][1] * size,
                           0.36f * size, at[i][2]);
}

static const game scopa_game = {
    .id = "scopa", .order = 309, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(scopa_game)
