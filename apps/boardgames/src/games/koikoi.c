/*
 * Koi-Koi in RetroStone Board: the hanafuda game for 2 players passing the device (the rules: koikoi_rules.c,
 * koikoi.h; the cards: koikoi_deck.c). Each hand is secret (VIEW_HIDDEN): the cover comes up whenever the device
 * passes. The table, from the top: the other's hand face down; their captured cards grouped by type (brights,
 * animals, ribbons, plains; a count on each group) and the yaku they have formed (badges: the yaku's cards and its
 * points); the stock and the field; the viewer's yaku and captured cards; the viewer's hand. Landscape: the captures
 * on the right. What the other did in their last turn is marked (the cards they left on the field, the ones they
 * took).
 * Taps: a card of the hand chooses it (the field cards of its month glow), then a glowing field card (or the card
 * again, or the field when nothing matches) plays it; drag it onto the field too. Two of its month: tap the one to
 * take. Then the stock, to turn its top card. A yaku formed or raised: two big icons, stop (an open hand) or koi-koi
 * (a double arrow). After a round, its points (the yaku as badges, a cross and a 2 for each doubling); tap for the
 * next.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/koikoi.h"
#include "games/koikoi_deck.h"
#include "shell.h"

#define HD hana_deck()                          /* the hanafuda deck, drawn by the card kit */

#define SLIDE_MS 300
#define FLIP_MS 300
#define DEAL_MS 240
#define PAUSE_MS 260
#define REST_MS 320                             /* after the last card lands: a moment before the device passes */
#define ASP HANA_ASPECT

static kgame G;
static int dealing, floor_n;                    /* floor_n: undo stops there (the last hand-off, a card turned) */
static int sel = KK_NONE;                       /* the hand's card chosen */
static kpos turn_snap, mark_snap;               /* the table as the turn began; as the other's last turn began */
static int marks_on, mark_pl;
static struct { int on, card, moved; float x0, y0, x, y; } drag;

/* ---- the table's geometry (it follows the viewer) ---------------------------------------------------------------- */
static struct {
    box b;
    int me, land;
    float hw, hx, hy, hwide;                    /* the viewer's hand: card width, centre, room */
    float ow, ox, oy, owide;                    /* the other's hand (backs) */
    box field;                                  /* the stock and the field */
    box cap[2], bad[2];                         /* by seat (0: the viewer's, at the bottom): captures, badges */
    int rows[2];                                /* the capture groups in one row or two */
    float cw[2];                                /* the captured cards' width */
    float sx, sy, kx, ky, br;                   /* the decision's buttons: stop, koi-koi; their radius */
} T;

typedef struct { int cols; float w, x0, y0, colw, rowh, dx, dy; } fgeo;

static int seat(int pl) { return pl == T.me ? 0 : 1; }

static void layout(box b)
{
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me > 1) T.me = 0;
    T.land = b.w > b.h * 1.05f;
    if (!T.land) {                              /* portrait: one column, the other's side at the top */
        float H = b.h, y = b.y, ho = 0.08f * H, hc = 0.135f * H, hb = 0.06f * H, hf = 0.3f * H;
        float hh = H - ho - 2 * hc - 2 * hb - hf;
        T.owide = b.w * 0.62f, T.ox = b.x + b.w / 2, T.oy = y + ho / 2;
        T.ow = floorf(fminf(ho * 0.86f / ASP, T.owide / 5));
        y += ho;
        T.cap[1] = (box){b.x, y, b.w, hc}, y += hc;
        T.bad[1] = (box){b.x, y, b.w, hb}, y += hb;
        T.field = (box){b.x, y, b.w, hf}, y += hf;
        T.bad[0] = (box){b.x, y, b.w, hb}, y += hb;
        T.cap[0] = (box){b.x, y, b.w, hc}, y += hc;
        T.hwide = b.w * 0.98f, T.hx = b.x + b.w / 2;
        T.hw = floorf(fminf(hh * 0.8f / ASP, T.hwide / 8.6f));
        T.hy = y + hh / 2 + 0.03f * hh;
    } else {                                    /* landscape: the hands and the field; the captures on the right */
        float lw = floorf(b.w * 0.6f), H = b.h, ho = 0.13f * H, hf = 0.43f * H, hh = H - ho - hf;
        T.owide = lw * 0.7f, T.ox = b.x + lw / 2, T.oy = b.y + ho / 2;
        T.ow = floorf(fminf(ho * 0.86f / ASP, T.owide / 5));
        T.field = (box){b.x, b.y + ho, lw, hf};
        T.hwide = lw * 0.98f, T.hx = b.x + lw / 2;
        T.hw = floorf(fminf(hh * 0.8f / ASP, T.hwide / 8.6f));
        T.hy = b.y + ho + hf + hh / 2 + 0.03f * hh;
        float rx = b.x + lw + 0.03f * b.w, rw = b.x + b.w - rx, half = H / 2, hb = 0.12f * H, gap = 0.015f * H;
        T.cap[1] = (box){rx, b.y, rw, half - hb - gap};
        T.bad[1] = (box){rx, b.y + half - hb - gap, rw, hb};
        T.bad[0] = (box){rx, b.y + half + gap, rw, hb};
        T.cap[0] = (box){rx, b.y + half + gap + hb, rw, half - hb - gap};
    }
    for (int s = 0; s < 2; s++) {
        box c = T.cap[s];
        T.rows[s] = c.w / c.h > 3.2f ? 1 : 2;
        float gh = c.h / T.rows[s], gw = c.w * (T.rows[s] == 1 ? 0.19f : 0.42f);
        T.cw[s] = floorf(fminf(gh * 0.9f / ASP, gw * 0.62f));
    }
    float u = ui.unit;
    T.br = floorf(fminf(0.95f * u, fminf(b.w * 0.17f, b.h * 0.12f)));
    T.sx = b.x + b.w / 2 - 1.5f * T.br, T.kx = b.x + b.w / 2 + 1.5f * T.br;
    T.sy = T.ky = b.y + b.h / 2 + 0.45f * T.br;
}

/* the field: its places in columns of two (five columns at least, more when it grows), the stock on its left */
static fgeo field_geo(const kpos *p)
{
    fgeo g;
    int hi = 0, hole = -1;                      /* the last card; the first empty place (where the next one goes) */
    for (int s = 0; s < KK_FIELD; s++)
        if (p->field[s] != KK_NONE) hi = s + 1;
        else if (hole < 0) hole = s;
    if (hole + 1 > hi) hi = hole + 1;
    g.cols = hi > 8 ? (hi + 1) / 2 : 4;
    box f = T.field;
    g.rowh = f.h / 2;
    g.w = floorf(fminf(f.w / (g.cols + 1.4f) / 1.14f, g.rowh * 0.92f / ASP));
    g.colw = fminf(f.w / (g.cols + 1.4f), 1.3f * g.w);
    float x = f.x + (f.w - g.colw * (g.cols + 1.4f)) / 2;
    g.x0 = x + 1.4f * g.colw;
    g.y0 = f.y;
    g.dx = x + 0.6f * g.colw;
    g.dy = f.y + f.h / 2;
    return g;
}

static void slot_at(const fgeo *g, int s, float *x, float *y)
{
    *x = g->x0 + (s / 2 + 0.5f) * g->colw;
    *y = g->y0 + (s % 2 + 0.5f) * g->rowh;
}

static void held_at(const fgeo *g, float *x, float *y)
{
    *x = g->dx + 0.14f * g->w;
    *y = g->dy - 0.1f * g->w;
}

static void hand_at(const kpos *p, int pl, int i, float *x, float *y, float *w)
{
    int n = p->nh[pl], mine = pl == T.me;
    float cw = mine ? T.hw : T.ow, wide = mine ? T.hwide : T.owide;
    float step = n > 1 ? fminf((mine ? 1.06f : 0.62f) * cw, (wide - cw) / (n - 1)) : 0;
    *x = (mine ? T.hx : T.ox) + (i - (n - 1) / 2.0f) * step;
    *y = mine ? T.hy : T.oy;
    *w = cw;
}

/* the captures: four groups (brights, animals, ribbons, plains), in a row or two by two */
static void group_box(int s, int g, box *o)
{
    static const float ONE[5] = {0, 0.19f, 0.44f, 0.69f, 1};
    box c = T.cap[s];
    if (T.rows[s] == 1) {
        *o = (box){c.x + ONE[g] * c.w, c.y, (ONE[g + 1] - ONE[g]) * c.w - 0.02f * c.w, c.h};
        return;
    }
    float x0 = g % 2 ? 0.45f : 0, x1 = g % 2 ? 1 : 0.43f, h = c.h / 2;
    *o = (box){c.x + x0 * c.w, c.y + (g / 2) * h, (x1 - x0) * c.w, h};
}

static int group_of(int card) { return kk_type(card); }

static void group_spot(int s, int g, int i, int n, float *x, float *y)
{
    box o;
    group_box(s, g, &o);
    float cw = T.cw[s], step = n > 1 ? fminf(0.56f * cw, (o.w - cw) / (n - 1)) : 0;
    *x = o.x + cw / 2 + i * step;
    *y = o.y + o.h / 2;
}

static void cap_at(const kpos *p, int pl, int card, float *x, float *y, float *w)
{
    int g = group_of(card), i = 0, n = 0;
    for (int k = 0; k < p->ncap[pl]; k++) {
        if (group_of(p->cap[pl][k]) != g) continue;
        if (p->cap[pl][k] == card) i = n;
        n++;
    }
    group_spot(seat(pl), g, i, n, x, y);
    *w = T.cw[seat(pl)];
}

/* where a card is: in a hand, on the field, captured, in the stock, held (its match to choose) */
enum { L_NONE, L_HAND, L_FIELD, L_CAP, L_DECK, L_HELD };
static int where(const kpos *p, int card, int *pl, int *i)
{
    for (int q = 0; q < 2; q++) {
        for (int k = 0; k < p->nh[q]; k++)
            if (p->hand[q][k] == card) { *pl = q, *i = k; return L_HAND; }
        for (int k = 0; k < p->ncap[q]; k++)
            if (p->cap[q][k] == card) { *pl = q, *i = k; return L_CAP; }
    }
    for (int s = 0; s < KK_FIELD; s++)
        if (p->field[s] == card) { *i = s; return L_FIELD; }
    if (p->phase == KP_CHOOSE && p->held == card) return L_HELD;
    for (int k = 0; k < p->ndeck; k++)
        if (p->deck[k] == card) return L_DECK;
    return L_NONE;
}

/* a card's place as drawn: its centre, width, face up */
static void spot(const kpos *p, int card, float *x, float *y, float *w, int *up)
{
    int pl = 0, i = 0, l = where(p, card, &pl, &i);
    fgeo g = field_geo(p);
    *up = 1;
    switch (l) {
    case L_HAND:
        hand_at(p, pl, i, x, y, w);
        *up = pl == T.me && ui.viewer == T.me;
        break;
    case L_FIELD: slot_at(&g, i, x, y), *w = g.w; break;
    case L_CAP: cap_at(p, pl, card, x, y, w); break;
    case L_HELD: held_at(&g, x, y), *w = g.w; break;
    default: *x = g.dx, *y = g.dy, *w = g.w, *up = 0; break;
    }
}

/* ---- the moving cards: slides drawn from `show` ms on (a second leg waits for the first) ------------------------- */
#define MOVES 64
static struct { cards_slide s; Uint32 show; int card; } mv[MOVES];
static int nmv;
static Uint32 quiet;

static void calm(void)
{
    nmv = 0;
    quiet = 0;
}

static void slide(int card, int up0, float x0, float y0, float w0, int up1, float x1, float y1, float w1, int ms,
                  int delay, int show, int sound)
{
    if (nmv >= MOVES) return;
    cards_slide_start(&mv[nmv].s, HD, card, x0, y0, w0, 0, up0, x1, y1, w1, 0, up1, ms, delay);
    mv[nmv].show = (Uint32)show;
    mv[nmv].card = card;
    nmv++;
    Uint32 end = ui_ms() + (Uint32)(delay + ms + REST_MS);
    if (end > quiet) quiet = end;
    if (sound >= 0) ui_sound_later(sound, delay + ms > 40 ? delay + ms - 40 : 0);
}

static int moving(int card)
{
    for (int i = 0; i < nmv; i++)
        if (mv[i].card == card && cards_slide_busy(&mv[i].s)) return 1;
    return 0;
}

static int busy(void)
{
    for (int i = 0; i < nmv; i++)
        if (cards_slide_busy(&mv[i].s)) return 1;
    return quiet && ui_ms() < quiet;
}

static void moves_draw(void)
{
    Uint32 now = ui_ms();
    for (int i = 0; i < nmv; i++) {
        if (!mv[i].s.on) continue;
        if (now - mv[i].s.t0 < mv[i].show) { ui_animate(); continue; }
        cards_slide_draw(&mv[i].s);
    }
}

/* the deal: from the stock to the hands and the field, two at a time */
static void deal_slides(void)
{
    const kpos *p = &G.pos;
    fgeo g = field_geo(p);
    int k = 0;
    for (int r = 0; r < 4; r++)
        for (int part = 0; part < 3; part++)
            for (int j = 0; j < 2; j++, k++) {
                int c;
                if (part < 2) {
                    int q = part ? 1 - p->dealer : p->dealer;
                    c = p->hand[q][(2 * r + j) % KK_HAND];
                } else {
                    c = p->field[2 * r + j];
                }
                if (c == KK_NONE) continue;
                float x, y, w;
                int up;
                spot(p, c, &x, &y, &w, &up);
                slide(c, 0, g.dx, g.dy, g.w, up, x, y, w, DEAL_MS, k * 45, 0, k % 4 == 0 ? SND_MOVE : -1);
            }
}

/* what an action did: the card played (or turned) meets the field; captured cards fly to their group */
static void animate(const kpos *b)
{
    const kpos *p = &G.pos;
    int mover = KK_NONE, from = L_NONE, pl, i;
    for (int c = 0; c < KK_CARDS && mover == KK_NONE; c++) {
        int l0 = where(b, c, &pl, &i), l1 = where(p, c, &pl, &i);
        if ((l0 == L_HAND || l0 == L_DECK || l0 == L_HELD) && l1 != l0) mover = c, from = l0;
    }
    if (mover == KK_NONE) return;
    fgeo g0 = field_geo(b), g1 = field_geo(p);
    float x, y, w, x1, y1, w1;
    int up, t = 0;
    spot(b, mover, &x, &y, &w, &up);
    if (from == L_DECK) {                       /* turned over, by the stock */
        float hx, hy;
        held_at(&g0, &hx, &hy);
        slide(mover, 0, x, y, w, 1, hx, hy, g0.w, FLIP_MS, 0, 0, SND_PLACE);
        x = hx, y = hy, w = g0.w, up = 1, t = FLIP_MS + 180;
    }
    int l1 = where(p, mover, &pl, &i);
    if (l1 == L_FIELD || l1 == L_HELD) {
        spot(p, mover, &x1, &y1, &w1, &up);
        slide(mover, from == L_DECK ? 1 : up == 0 ? 0 : (b->turn == T.me && ui.viewer == T.me), x, y, w, 1, x1, y1,
              w1, SLIDE_MS, t, t, SND_PLACE);
        return;
    }
    /* captured: first onto a field card of its month, then all of them to the captures */
    int mate = KK_NONE;
    for (int s = 0; s < KK_FIELD; s++) {
        int c = b->field[s];
        if (c != KK_NONE && HANA_MONTH(c) == HANA_MONTH(mover) && where(p, c, &pl, &i) == L_CAP) { mate = s; break; }
    }
    float mx = x, my = y;
    if (mate >= 0 && mate != KK_NONE) {
        slot_at(&g0, mate, &mx, &my);
        mx += 0.16f * g0.w, my += 0.12f * g0.w;
        int up0 = from == L_HAND ? (b->turn == T.me && ui.viewer == T.me) : 1;
        slide(mover, up0, x, y, w, 1, mx, my, g0.w, SLIDE_MS, t, t, SND_PLACE);
        t += SLIDE_MS;
    }
    (void)g1;
    int k = 0;
    for (int c = 0; c < KK_CARDS; c++) {
        int l0 = where(b, c, &pl, &i);
        if (l0 == L_CAP || where(p, c, &pl, &i) != L_CAP) continue;
        float sx, sy, sw;
        int u0;
        if (c == mover) sx = mx, sy = my, sw = g0.w;
        else spot(b, c, &sx, &sy, &sw, &u0);
        cap_at(p, pl, c, &x1, &y1, &w1);
        slide(c, 1, sx, sy, sw, 1, x1, y1, w1, SLIDE_MS + 80, t + PAUSE_MS + 50 * k, c == mover ? t : 0,
              k == 0 ? SND_CAPTURE : -1);
        k++;
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    kk_new(&G, v == KK_HALF ? KK_HALF : KK_FULL, (uint32_t)ui_random(1 << 30));
    calm();
    sel = KK_NONE;
    floor_n = 0;
    marks_on = 0;
    turn_snap = G.pos;
    dealing = 1;
}

static int status(void)
{
    if (G.pos.phase != KP_OVER) return GS_PLAY;
    int w = kk_winner(&G.pos);
    return w < 0 ? GS_DRAW : gs_win(w);
}

static unsigned winners(void)
{
    const kpos *p = &G.pos;
    if (p->phase != KP_OVER) return 0;
    return p->score[0] == p->score[1] ? 3u : 1u << (p->score[1] > p->score[0]);
}

static int turn(void) { return G.pos.turn; }

static void handed(int next)                    /* the device passes: the marks of this turn for the next player */
{
    mark_snap = turn_snap;
    mark_pl = 1 - next;
    marks_on = 1;
    turn_snap = G.pos;
    floor_n = G.n;
    ui_handoff(next);
}

static void after(const kpos *b, int last)
{
    const kpos *p = &G.pos;
    calm();
    sel = KK_NONE;
    drag.on = 0;
    int k = KA_KIND(last);
    if (k == KA_NEXT) {
        marks_on = 0;
        turn_snap = *p;
        floor_n = G.n;
        dealing = 1;
        if (p->turn != ui.viewer) ui_handoff(p->turn);
        return;
    }
    animate(b);
    if (k == KA_DRAW) floor_n = G.n;            /* a card turned: no going back on it */
    if (p->phase == KP_PLAY && p->turn != b->turn) handed(p->turn);
}

static void act(int a)
{
    kpos before = G.pos;
    if (!kk_play(&G, a)) { ui_sound(SND_DENY); return; }
    after(&before, a);
}

/* a card of the hand played, onto field card f (KK_NONE: wherever it goes) */
static void play(int c, int f)
{
    kpos before = G.pos;
    if (!kk_play(&G, KA_PLAY | c)) { ui_sound(SND_DENY); return; }
    if (G.pos.phase == KP_CHOOSE && f != KK_NONE && !kk_play(&G, KA_TAKE | f)) kk_undo(&G);
    after(&before, KA_PLAY);
}

static int undo(void)
{
    if (G.n <= floor_n || !kk_undo(&G)) return 0;
    calm();
    sel = KK_NONE;
    dealing = 0;
    return 1;
}

static int script(const char *s)
{
    int ok = kk_script(&G, s);
    calm();
    sel = KK_NONE;
    dealing = 0;
    marks_on = 0;
    turn_snap = G.pos;
    floor_n = G.n;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* the yaku's badges: a few of its cards, side by side */
static const uint8_t BADGE[YAKU_N][5] = {
    {KK_CRANE, KK_CURTAIN, KK_MOON, KK_RAINMAN, KK_PHOENIX}, {KK_CRANE, KK_CURTAIN, KK_MOON, KK_PHOENIX},
    {KK_CRANE, KK_MOON, KK_RAINMAN, KK_PHOENIX}, {KK_CRANE, KK_CURTAIN, KK_MOON}, {KK_BOAR, KK_DEER, KK_BUTTERFLIES},
    {HANA(0, 1), HANA(1, 1), HANA(2, 1)}, {HANA(5, 1), HANA(8, 1), HANA(9, 1)}, {KK_CURTAIN, KK_SAKE},
    {KK_MOON, KK_SAKE}, {KK_WARBLER, KK_CUCKOO, KK_GEESE}, {HANA(3, 1), HANA(4, 1), HANA(6, 1)},
    {HANA(0, 2), HANA(5, 2), HANA(9, 3)}, {HANA(0, 0), HANA(0, 1), HANA(0, 2), HANA(0, 3)},
    {HANA(2, 2), HANA(2, 3), HANA(9, 2), HANA(9, 3)}};
static int badge_n(int y)
{
    return y <= Y_GOKO ? 5 : y <= Y_AMESHIKO || y >= Y_TESHI ? 4 : y <= Y_AOTAN || y >= Y_TANE ? 3 : 2;
}

static SDL_Texture *t_badge[YAKU_N], *t_pill, *t_icon[2], *t_koi, *t_cross, *t_tok[2], *t_panel, *t_disc, *t_dot;
static int badge_h, pill_h, icon_s, koi_s, cross_s, tok_s, panel_w, panel_h, disc_s;

static float mini_w(float bh) { return floorf(0.8f * bh / ASP); }
static float fan_w(int y, float bh) { return mini_w(bh) * (1 + 0.55f * (badge_n(y) - 1)); }

static void textures(void)
{
    float u = ui.unit;
    int bh = (int)fminf(0.66f * u, fminf(T.bad[0].h * 0.92f, 0.11f * T.b.w));
    if (badge_h != bh) {
        badge_h = bh;
        for (int y = 0; y < YAKU_N; y++) {
            int h = bh * 2, w = (int)fan_w(y, (float)h);   /* made larger: drawn up to twice as big */
            ui_free(&t_badge[y]);
            hana_fan(BADGE[y], badge_n(y), (int)mini_w((float)h), w, h, ui_buf(w, h));
            t_badge[y] = ui_tex(w, h, ui_buf(w, h));
        }
    }
    int ph = bh;
    if (pill_h != ph) {
        pill_h = ph;
        const uint8_t c[4] = {24, 30, 28, 215};
        ui_free(&t_pill);
        art_roundrect(ph, ph, ph * 0.3f, c, ui_buf(ph, ph));
        t_pill = ui_tex(ph, ph, ui_buf(ph, ph));
    }
    int is = (int)(2 * T.br);
    if (icon_s != is) {
        icon_s = is;
        for (int i = 0; i < 2; i++) {
            ui_free(&t_icon[i]);
            hana_icon(i, is, ui_buf(is, is));
            t_icon[i] = ui_tex(is, is, ui_buf(is, is));
        }
    }
    int ks = (int)(0.6f * bh);
    if (koi_s != ks) {
        koi_s = ks;
        ui_free(&t_koi);
        hana_icon(HI_KOI, ks, ui_buf(ks, ks));
        t_koi = ui_tex(ks, ks, ui_buf(ks, ks));
    }
    int cs = (int)(0.3f * u);
    if (cross_s != cs) {
        cross_s = cs;
        ui_free(&t_cross);
        art_icon(ICON_CLOSE, cs, ui_buf(cs, cs));
        t_cross = ui_tex(cs, cs, ui_buf(cs, cs));
        SDL_SetTextureColorMod(t_cross, 250, 214, 90);
    }
    int ts = (int)(0.6f * u);
    if (tok_s != ts) {
        tok_s = ts;
        for (int q = 0; q < 2; q++) {
            ui_free(&t_tok[q]);
            cards_token(q, ts, ui_buf(ts, ts));
            t_tok[q] = ui_tex(ts, ts, ui_buf(ts, ts));
        }
    }
    int ds = (int)(0.4f * u);
    if (disc_s != ds) {
        disc_s = ds;
        const uint8_t c[3] = {24, 30, 28};
        ui_free(&t_disc);
        art_disc(ds, c, 0.85f, 0, ui_buf(ds, ds));
        t_disc = ui_tex(ds, ds, ui_buf(ds, ds));
    }
    if (!t_dot) {
        const uint8_t c[3] = {255, 255, 255};
        art_disc(32, c, 1, 0, ui_buf(32, 32));
        t_dot = ui_tex(32, 32, ui_buf(32, 32));
    }
}

/* a dark pill w x h at (x, y): its round ends from the square texture, its middle stretched */
static void pill(float x, float y, float w, float h)
{
    if (!t_pill) return;
    int s = pill_h;
    float r = h / 2;
    SDL_Rect a = {0, 0, s / 2, s}, m = {s / 2, 0, 1, s}, z = {s - s / 2, 0, s / 2, s};
    SDL_FRect da = {x, y, r, h}, dm = {x + r, y, fmaxf(0, w - 2 * r), h}, dz = {x + w - r, y, r, h};
    SDL_RenderCopyF(ui.r, t_pill, &a, &da);
    SDL_RenderCopyF(ui.r, t_pill, &m, &dm);
    SDL_RenderCopyF(ui.r, t_pill, &z, &dz);
}

static int digits(int v) { return v >= 100 ? 3 : v >= 10 ? 2 : 1; }

/* a yaku's badge at (x, cy): its cards and its points; its width */
static float badge_width(int y, int pts, float bh)
{
    return 0.18f * bh + fan_w(y, bh) + 0.14f * bh + 0.32f * bh * digits(pts) + 0.2f * bh;
}

static float badge(int y, int pts, float x, float cy, float bh, int draw_it)
{
    float w = badge_width(y, pts, bh);
    if (!draw_it) return w;
    pill(x, cy - bh / 2, w, bh);
    float fw = fan_w(y, bh);
    ui_put(t_badge[y], x + 0.18f * bh, cy - bh / 2, fw, bh);
    float dw = 0.32f * bh * digits(pts);
    ui_number(x + 0.18f * bh + fw + 0.14f * bh + dw / 2, cy, 0.46f * bh, pts, 1);
    return w;
}

/* a koi-koi call shown: the red disc, and how many when more than one */
static float koi_chip(int n, float x, float cy, float bh, int draw_it)
{
    float d = 0.62f * bh, w = d + (n > 1 ? 0.5f * bh : 0) + 0.3f * bh;
    if (!draw_it) return w;
    pill(x, cy - bh / 2, w, bh);
    ui_put_c(t_koi, x + 0.15f * bh + d / 2, cy, d, d);
    if (n > 1) ui_number(x + 0.15f * bh + d + 0.25f * bh, cy, 0.42f * bh, n, 1);
    return w;
}

/* a row of badges (the yaku of pts, and koi-koi calls) centred in box b; scaled down to fit */
static void badge_row(const int *pts, int koi, box b, int left, float scale)
{
    float bh = badge_h * scale, w = 0, gap = 0.15f * bh;
    int n = 0;
    for (int y = 0; y < YAKU_N; y++)
        if (pts[y]) w += badge(y, pts[y], 0, 0, bh, 0) + gap, n++;
    if (koi) w += koi_chip(koi, 0, 0, bh, 0) + gap, n++;
    if (!n) return;
    w -= gap;
    float k = w > b.w ? b.w / w : 1, x = left ? b.x : b.x + (b.w - w * k) / 2, cy = b.y + b.h / 2;
    bh *= k, gap *= k;
    if (koi) x += koi_chip(koi, x, cy, bh, 1) + gap;
    for (int y = 0; y < YAKU_N; y++)
        if (pts[y]) x += badge(y, pts[y], x, cy, bh, 1) + gap;
}

/* the stock: a few backs, its thickness */
static void stock(float cx, float cy, float w, int n, int how)
{
    if (n <= 0) { cards_place(HD, cx, cy, w); return; }
    int layers = n < 4 ? n - 1 : n < 16 ? 3 : 4;
    float step = fmaxf(1, 0.025f * w);
    for (int k = layers; k >= 1; k--)
        cards_draw(HD, KK_NONE, cx + k * step * 0.4f, cy + k * step, w, 0, CD_DOWN | CD_FLAT);
    cards_draw(HD, KK_NONE, cx, cy, w, 0, CD_DOWN | (layers ? CD_FLAT : 0) | how);
}

static int marked_field(const kpos *p, int card)   /* left on the field by the other's last turn */
{
    if (!marks_on || p->phase != KP_PLAY || ui.viewer == mark_pl) return 0;
    for (int s = 0; s < KK_FIELD; s++)
        if (mark_snap.field[s] == card) return 0;
    return 1;
}

static int marked_cap(const kpos *p, int pl, int k)
{
    return marks_on && p->phase == KP_PLAY && pl == mark_pl && ui.viewer != mark_pl && k >= mark_snap.ncap[pl];
}

static void draw_caps(const kpos *p, int pl)
{
    int s = seat(pl), n[HTYPES] = {0}, at[HTYPES] = {0};
    for (int k = 0; k < p->ncap[pl]; k++) n[group_of(p->cap[pl][k])]++;
    float cw = T.cw[s];
    for (int g = 0; g < HTYPES; g++)            /* the empty groups: a faint place */
        if (!n[g]) {
            float x, y;
            group_spot(s, g, 0, 1, &x, &y);
            cards_place(HD, x, y, cw);
        }
    for (int k = 0; k < p->ncap[pl]; k++) {
        int c = p->cap[pl][k], g = group_of(c);
        float x, y;
        group_spot(s, g, at[g]++, n[g], &x, &y);
        if (moving(c)) continue;
        cards_draw(HD, c, x, y, cw, 0, CD_FLAT);
        if (marked_cap(p, pl, k)) ui_mark_last(x - 0.5f * cw, y - 0.5f * cw * ASP, cw, cw * ASP);
    }
    for (int g = 0; g < HTYPES; g++) {          /* each group's count, on its last card's corner */
        if (!n[g]) continue;
        float x, y;
        group_spot(s, g, n[g] - 1, n[g], &x, &y);
        float d = fmaxf(0.38f * ui.unit, 0.42f * cw), cx = x + 0.5f * cw - 0.4f * d;
        float cy = y - 0.5f * cw * ASP + 0.62f * d;
        ui_put_c(t_disc, cx, cy, d, d);
        ui_number(cx, cy, 0.55f * d, n[g], 1);
    }
}

static void draw_hand(const kpos *p, int pl)
{
    int ph = p->phase, show = (pl == T.me || ph == KP_ROUND || ph == KP_OVER) && ui.viewer >= 0 && ui.viewer == T.me;
    for (int i = 0; i < p->nh[pl]; i++) {
        int c = p->hand[pl][i];
        if (moving(c) || (drag.on && drag.moved && drag.card == c)) continue;
        float x, y, w;
        hand_at(p, pl, i, &x, &y, &w);
        int how = show ? (c == sel ? CD_RAISED : 0) : CD_DOWN;
        cards_draw(HD, c, x, y, w, 0, how);
    }
}

/* the round dots: the rounds of the game, the ones played filled */
static void draw_rounds(const kpos *p, const fgeo *g)
{
    float d = fminf(0.16f * ui.unit, 0.95f * g->w / 6), y = g->dy + 0.5f * g->w * ASP + 0.45f * d + 0.12f * g->w;
    int n = p->rounds, per = n > 6 ? 6 : n;
    for (int i = 0; i < n; i++) {
        float x = g->dx + (i % per - (per - 1) / 2.0f) * 1.25f * d, yy = y + (i / per) * 1.25f * d;
        int done = i < p->round || (i == p->round && (p->phase == KP_ROUND || p->phase == KP_OVER));
        if (i == p->round && !done) ui_mark_gold(x, yy, 2.2f * d);
        SDL_SetTextureColorMod(t_dot, done ? 250 : 246, done ? 214 : 238, done ? 90 : 220);
        SDL_SetTextureAlphaMod(t_dot, done ? 255 : 80);
        ui_put_c(t_dot, x, yy, d, d);
    }
}

static void dim(void) { ui_fill(T.b.x, T.b.y, T.b.w, T.b.h, 0, 0, 0, 110); }

static void panel(float x, float y, float w, float h)
{
    if (panel_w != (int)w || panel_h != (int)h) {
        panel_w = (int)w, panel_h = (int)h;
        const uint8_t c[4] = {28, 34, 32, 248};
        ui_free(&t_panel);
        art_roundrect(panel_w, panel_h, 0.4f * ui.unit, c, ui_buf(panel_w, panel_h));
        t_panel = ui_tex(panel_w, panel_h, ui_buf(panel_w, panel_h));
    }
    ui_put(t_panel, x, y, w, h);
}

/* the points of a stop now: the yaku added up, doubled at 7, doubled for the other's koi-koi */
static int stop_points(const kpos *p, int pl)
{
    int v = kk_points(p, pl);
    return v << (v >= 7) << (p->koi[1 - pl] > 0);
}

/* stop or koi-koi: the yaku, the stop button (the points it scores), the koi-koi button */
static void draw_decide(const kpos *p)
{
    float u = ui.unit, bh = (float)badge_h;
    int pts[YAKU_N];
    kk_yaku(p->cap[p->turn], p->ncap[p->turn], pts);
    dim();
    float pw = fminf(T.b.w * 0.94f, 8.2f * u), ph = 2 * T.br + 2.8f * bh + 0.9f * u;
    float px = T.b.x + (T.b.w - pw) / 2, py = T.sy - T.br - 2.0f * bh - 0.35f * u;
    panel(px, py, pw, ph);
    badge_row(pts, 0, (box){px + 0.3f * u, py + 0.3f * u, pw - 0.6f * u, 1.6f * bh}, 0, 1.7f);
    ui_mark_gold(T.sx, T.sy, 2.9f * T.br);
    ui_put_c(t_icon[HI_STOP], T.sx, T.sy, 2 * T.br, 2 * T.br);
    ui_put_c(t_icon[HI_KOI], T.kx, T.ky, 2 * T.br, 2 * T.br);
    ui_number(T.sx, T.sy + T.br + 0.42f * u, 0.5f * u, stop_points(p, p->turn), 1);
    if (p->koi[p->turn]) ui_number(T.kx, T.ky + T.br + 0.42f * u, 0.5f * u, p->koi[p->turn] + 1, 1);
}

/* the round's end: the winner's yaku and doublings, then each player's points of the round and total */
static void draw_round(const kpos *p)
{
    const kk_result *r = &p->res;
    float u = ui.unit, bh = (float)badge_h, rh = 0.95f * u;
    int win = r->kind != KR_DRAW;
    float pw = fminf(T.b.w * 0.94f, 8.2f * u), ph = 2 * rh + 0.5f * u;
    if (win) ph += 2.0f * bh + 0.2f * u + (r->seven || r->koi ? 0.7f * u : 0);
    float px = T.b.x + (T.b.w - pw) / 2, py = T.b.y + (T.b.h - ph) / 2, y = py + 0.3f * u;
    dim();
    panel(px, py, pw, ph);
    if (win) {
        int pts[YAKU_N];
        for (int i = 0; i < YAKU_N; i++) pts[i] = r->pts[i];
        if (r->kind == KR_INSTANT) {            /* teshi, kuttsuki: the hand dealt, and its points */
            int w = r->winner;
            float cw = floorf(fminf((pw - 2 * u) / 6.2f, 1.6f * bh / ASP)), step = 0.62f * cw;
            float x0 = px + pw / 2 - (7 * step + cw + 0.9f * u) / 2 + cw / 2;
            for (int i = 0; i < p->nh[w]; i++)
                cards_draw(HD, p->hand[w][i], x0 + i * step, y + 0.85f * bh, cw, 0, CD_FLAT);
            ui_number(x0 + 7 * step + cw / 2 + 0.5f * u, y + 0.85f * bh, 0.5f * u, r->points, 1);
        } else {
            badge_row(pts, 0, (box){px + 0.3f * u, y, pw - 0.6f * u, 1.7f * bh}, 0, 1.8f);
        }
        y += 1.9f * bh;
        float cy = y + 0.35f * u, x = px + pw / 2 - (r->seven + r->koi) * 0.7f * u + 0.0f;
        for (int k = 0; k < 2; k++) {           /* the doublings: a cross and a 2 (the other's koi-koi: its disc) */
            if (!(k ? r->koi : r->seven)) continue;
            if (k) ui_put_c(t_koi, x - 0.05f * u, cy, 0.4f * u, 0.4f * u), x += 0.3f * u;
            ui_put_c(t_cross, x + 0.15f * u, cy, 0.34f * u, 0.34f * u);
            ui_number(x + 0.55f * u, cy, 0.4f * u, 2, 1);
            x += 1.1f * u;
        }
        if (r->seven || r->koi) y += 0.7f * u;
    }
    for (int k = 0; k < 2; k++) {               /* the rows: the viewer's first */
        int q = k ? 1 - T.me : T.me;
        float cy = y + (k + 0.5f) * rh, tx = px + 0.75f * u, d = 0.62f * u;
        if (r->winner == q) ui_mark_gold(tx, cy, 2.0f * d);
        ui_put_c(t_tok[q], tx, cy, d, d);
        ui_number_signed(px + pw * 0.45f, cy, 0.42f * rh, r->winner == q ? r->points : 0, 1, 1);
        ui_number(px + pw * 0.8f, cy, 0.56f * rh, p->score[q], 1);
        if (k == 0) ui_fill(px + 0.3f * u, cy + rh / 2, pw - 0.6f * u, fmaxf(1, 0.02f * u), 246, 238, 220, 40);
    }
}

static void draw(void)
{
    const kpos *p = &G.pos;
    if (dealing && ui.viewer >= 0) { dealing = 0; calm(); deal_slides(); }
    textures();
    int b = busy(), ph = p->phase, mine = p->turn == T.me && ui.viewer == T.me && !ui_handing() && !b;
    fgeo g = field_geo(p);
    /* the stock, the rounds */
    stock(g.dx, g.dy, g.w, p->ndeck, mine && ph == KP_DRAW ? CD_GLOW : 0);
    draw_rounds(p, &g);
    /* the field: the cards of the chosen card's month glow (or the held card's, its match to choose) */
    int month = -1;
    if (mine && ph == KP_PLAY && sel != KK_NONE) month = HANA_MONTH(sel);
    if (mine && ph == KP_CHOOSE) month = HANA_MONTH(p->held);
    if (drag.on && drag.moved && drag.card != KK_NONE) month = HANA_MONTH(drag.card);
    for (int s = 0; s < KK_FIELD; s++) {
        int c = p->field[s];
        if (c == KK_NONE || moving(c)) continue;
        float x, y;
        slot_at(&g, s, &x, &y);
        cards_draw(HD, c, x, y, g.w, 0, HANA_MONTH(c) == month ? CD_GLOW : 0);
        if (marked_field(p, c)) ui_mark_last(x - 0.5f * g.w, y - 0.5f * g.w * ASP, g.w, g.w * ASP);
    }
    if (month >= 0 && ph == KP_PLAY && kk_matches(p, sel != KK_NONE ? sel : drag.card, NULL) == 0) {
        for (int s = 0; s < KK_FIELD; s++)      /* no match: where it will lie */
            if (p->field[s] == KK_NONE) {
                float x, y;
                slot_at(&g, s, &x, &y);
                cards_place(HD, x, y, g.w);
                ui_mark_dot(x, y, 0.5f * ui.unit);
                break;
            }
    }
    if (ph == KP_CHOOSE && !moving(p->held)) {
        float x, y;
        held_at(&g, &x, &y);
        cards_draw(HD, p->held, x, y, g.w, 0, CD_RAISED);
    }
    /* the captures, the badges, the hands */
    for (int q = 0; q < 2; q++) {
        draw_caps(p, q);
        int pts[YAKU_N];
        kk_yaku(p->cap[q], p->ncap[q], pts);
        badge_row(pts, p->koi[q], T.bad[seat(q)], 0, 1);
        draw_hand(p, q);
    }
    moves_draw();
    if (drag.on && drag.moved && drag.card != KK_NONE) cards_draw(HD, drag.card, drag.x, drag.y, T.hw, 0, CD_RAISED);
    if (ph == KP_DECIDE && mine) draw_decide(p);
    if ((ph == KP_ROUND || ph == KP_OVER) && !b) draw_round(p);
    if (b) ui_animate();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int hand_hit(float x, float y)
{
    const kpos *p = &G.pos;
    for (int i = p->nh[T.me] - 1; i >= 0; i--) {
        float cx, cy, w;
        hand_at(p, T.me, i, &cx, &cy, &w);
        if (p->hand[T.me][i] == sel) cy -= 0.14f * w;
        if (cards_in(cx, cy, w, ASP, x, y)) return p->hand[T.me][i];
    }
    return KK_NONE;
}

static int field_hit(float x, float y)
{
    const kpos *p = &G.pos;
    fgeo g = field_geo(p);
    for (int s = 0; s < KK_FIELD; s++) {
        if (p->field[s] == KK_NONE) continue;
        float cx, cy;
        slot_at(&g, s, &cx, &cy);
        if (cards_in(cx, cy, g.w * 1.08f, ASP, x, y)) return p->field[s];
    }
    return KK_NONE;
}

static int in_field(float x, float y)
{
    box f = T.field;
    return x >= f.x && x <= f.x + f.w && y >= f.y && y <= f.y + f.h;
}

static int mine_now(void) { return G.pos.turn == T.me && ui.viewer == T.me && !busy(); }

/* a card of the hand let go onto the field at (x, y): played if it can go there */
static int drop(int c, float x, float y)
{
    const kpos *p = &G.pos;
    int f = field_hit(x, y), n = kk_matches(p, c, NULL);
    if (f != KK_NONE && HANA_MONTH(f) == HANA_MONTH(c)) { play(c, f); return 1; }
    if (n == 0 && in_field(x, y)) { play(c, KK_NONE); return 1; }
    if (n > 0 && f == KK_NONE && in_field(x, y) && n != 2) { play(c, KK_NONE); return 1; }
    return 0;
}

static void tap(float x, float y)
{
    const kpos *p = &G.pos;
    int ph = p->phase;
    if (ph == KP_ROUND) { if (!busy()) act(KA_NEXT); return; }
    if (ph == KP_OVER || busy()) return;
    if (!mine_now()) { ui_sound(SND_DENY); return; }
    fgeo g = field_geo(p);
    switch (ph) {
    case KP_PLAY: {
        int h = hand_hit(x, y);
        if (h != KK_NONE) {
            if (h == sel) play(h, KK_NONE);
            else sel = h, marks_on = 0, ui_sound(SND_TAP);
            return;
        }
        if (sel != KK_NONE && drop(sel, x, y)) return;
        ui_sound(SND_DENY);
        return;
    }
    case KP_CHOOSE: {
        int f = field_hit(x, y);
        if (f != KK_NONE && kk_legal(p, KA_TAKE | f)) act(KA_TAKE | f);
        else ui_sound(SND_DENY);
        return;
    }
    case KP_DRAW:
        if (cards_in(g.dx, g.dy, g.w * 1.2f, ASP, x, y)) act(KA_DRAW);
        else ui_sound(SND_DENY);
        return;
    case KP_DECIDE:
        if (hypotf(x - T.sx, y - T.sy) <= T.br * 1.1f) act(KA_STOP);
        else if (hypotf(x - T.kx, y - T.ky) <= T.br * 1.1f) act(KA_KOI);
        return;
    }
}

static void press(float x, float y)
{
    drag.on = 1;
    drag.moved = 0;
    drag.x0 = drag.x = x, drag.y0 = drag.y = y;
    drag.card = G.pos.phase == KP_PLAY && mine_now() ? hand_hit(x, y) : KK_NONE;
}

static void motion(float x, float y)
{
    if (!drag.on || drag.card == KK_NONE) return;
    drag.x = x, drag.y = y;
    if (hypotf(x - drag.x0, y - drag.y0) > 0.2f * ui.unit) drag.moved = 1;
    if (drag.moved) ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    int c = drag.card, moved = drag.moved;
    drag.on = 0;
    if (moved && c != KK_NONE) {
        if (!drop(c, x, y)) ui_sound(SND_DENY);
        return;
    }
    tap(x, y);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the points */
{
    int v = G.pos.score[player];
    float h = fminf(0.46f * ui.unit, b.h * 0.6f), w = (0.74f * (digits(v) - 1) + 0.62f) * h;
    ui_number(b.x + w / 2, b.y + b.h / 2, h, v, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the crane, the moon and the sake cup; the picker's: the months of the game, 12 or 6 */
static void thumb(int v, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    if (v < 0) {
        static const uint8_t c[3] = {KK_CRANE, KK_MOON, KK_SAKE};
        int cw = size * 36 / 100, h = (int)(cw * ASP) + size / 10, w = size * 94 / 100;
        uint8_t *b = malloc((size_t)w * h * 4);
        if (!b) return;
        hana_fan(c, 3, cw, w, h, b);
        art_blend(rgba, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
        free(b);
        return;
    }
    int n = v == KK_HALF ? 6 : 12, cols = n == 6 ? 3 : 4, rows = n / cols;
    float cw = fminf(size * 0.94f / (cols * 1.1f - 0.1f), size * 0.94f / (rows * 1.1f * ASP - 0.1f * ASP));
    int iw = (int)cw, ih = (int)(cw * ASP);
    uint8_t *b = malloc((size_t)iw * ih * 4);
    if (!b) return;
    float x0 = (size - (cols * 1.1f - 0.1f) * cw) / 2, y0 = (size - (rows * 1.1f - 0.1f) * cw * ASP) / 2;
    for (int i = 0; i < n; i++) {
        hana_paint(HANA(i, 0), iw, ih, b);
        art_blend(rgba, size, size, b, iw, ih, (int)(x0 + (i % cols) * 1.1f * cw),
                  (int)(y0 + (i / cols) * 1.1f * cw * ASP));
    }
    free(b);
}

static const game koikoi_game = {
    .id = "koikoi", .order = 308, .variants = KK_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .view = VIEW_HIDDEN,
};
REGISTER_GAME(koikoi_game)
