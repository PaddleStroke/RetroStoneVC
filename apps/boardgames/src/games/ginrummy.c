/*
 * Gin Rummy in RetroStone Board: 2 players passing the device round (the rules: ginrummy_rules.c, ginrummy.h). Each
 * hand is secret (VIEW_HIDDEN): the cover comes up whenever the device must pass. The table, from the top: the other's
 * hand face down; the stock, the discard pile and, when the hand allows it, the knock button; the viewer's hand, its
 * melds put together by themselves (a gold line under each, the deadwood's points over the rest); a card dragged
 * along the hand stays where it is put (tap the deadwood's points to have them arranged again).
 * Taps: the stock or the discard pile to draw (the pass button in the bar refuses the first upcard); a card to choose
 * it, again (or the discard pile) to discard it, or the knock button. The end of a hand: both hands laid out in melds
 * and deadwood, the defender's cards laid off flying to the knocker's melds, the points; tap for the next hand.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/cardmoves.h"
#include "games/ginrummy.h"
#include "shell.h"

#define SLIDE_MS 320
#define FLIP_MS 260
#define DEAL_MS 220
#define LAY_MS 420

static gngame G;
static int dealing, floor_n;                    /* floor_n: undo stops there (the last hand-off) */
static int sel = CARD_NONE;                     /* the card chosen in the hand */
static const int deck = DECK_FRENCH;
static struct { int on, card, moved; float x0, y0, x, y; } drag;

/* ---- the table's geometry (it follows the viewer) ---------------------------------------------------------------- */
static struct {
    box b;
    int me, land;
    float ow, oy;                               /* the other's hand: card width, centre y */
    float pw, sx, sy, dx, dy;                   /* the stock and the discard pile */
    float kx, ky, kr;                           /* the knock button */
    float hw, hy, wide;                         /* the viewer's hand: card width, centre y; the rows' room */
    float my;                                   /* the middle (the result) */
} T;

static float aspect(void) { return cards_aspect(deck); }

static void layout(box b)
{
    float u = ui.unit;
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me > 1) T.me = 0;
    T.land = b.w > b.h * 1.05f;
    float ho = b.h * (T.land ? 0.23f : 0.2f), hh = b.h * (T.land ? 0.38f : 0.3f), mh = b.h - ho - hh;
    T.wide = b.w * 0.97f;
    T.ow = floorf(fminf(ho * 0.76f / aspect(), T.wide / 7.6f));
    T.oy = b.y + ho / 2;
    T.hw = floorf(fminf(hh * 0.78f / aspect(), T.wide / 6.3f));
    T.hy = b.y + b.h - hh / 2 + 0.06f * hh;
    T.pw = floorf(fminf(b.w / 4.8f, mh * (T.land ? 0.66f : 0.44f) / aspect()));
    T.my = b.y + ho + mh / 2;
    float cx = b.x + b.w / 2 - 0.4f * u;
    T.sx = cx - 0.66f * T.pw - 0.1f * u;
    T.dx = cx + 0.66f * T.pw + 0.1f * u;
    T.sy = T.dy = T.my;
    T.kr = 0.62f * u;
    T.kx = fminf(T.dx + 0.5f * T.pw + 0.3f * u + T.kr, b.x + b.w - T.kr - 0.1f * u);
    T.ky = T.my;
}

/* ---- a row of cards in groups: melds apart, a gold line under each ---------------------------------------------- */
#define ROWMAX 22
typedef struct {
    uint8_t card[ROWMAX];
    int8_t grp[ROWMAX];                         /* a meld (0..), the deadwood -1 */
    uint8_t laid[ROWMAX];                       /* a card laid off (the end of a hand) */
    int n, gaps;                                /* gaps: between groups (else one row, evenly) */
    float x[ROWMAX];
} row;

static void row_place(row *r, float cx, float w)
{
    if (!r->n) return;
    int ng = 0;
    for (int i = 1; i < r->n; i++) ng += r->gaps && r->grp[i] != r->grp[i - 1];
    float gap = 0.4f * w, step = fminf(0.64f * w, (T.wide - w - ng * gap) / (r->n > 1 ? r->n - 1 : 1));
    if (step < 0.3f * w) gap = 0.1f * w, step = (T.wide - w - ng * gap) / (r->n > 1 ? r->n - 1 : 1);
    float total = w + (r->n - 1) * step + ng * gap, x = cx - total / 2 + w / 2;
    for (int i = 0; i < r->n; i++) {
        if (i && r->gaps && r->grp[i] != r->grp[i - 1]) x += gap;
        r->x[i] = x;
        x += step;
    }
}

/* the gold line under each meld's cards side by side; the deadwood's points over the deadwood */
static SDL_Texture *t_disc;
static void row_marks(const row *r, float y, float w, int dead_points, int below)
{
    float h = w * aspect(), t = fmaxf(2, 0.07f * ui.unit);
    for (int i = 0; i < r->n;) {                /* the line ends where the next card begins */
        int j = i;
        while (j + 1 < r->n && r->grp[j + 1] == r->grp[i]) j++;
        float x0 = r->x[i] - w / 2 + 0.06f * w, x1 = j + 1 < r->n ? r->x[j + 1] - w / 2 - 0.06f * w : r->x[j] + 0.44f * w;
        if (r->grp[i] >= 0 && j - i >= 2) ui_fill(x0, y + h / 2 + 0.1f * ui.unit, x1 - x0, t, 250, 214, 90, 230);
        i = j + 1;
    }
    if (dead_points < 0) return;
    float sx = 0;
    int m = 0;
    for (int i = 0; i < r->n; i++)
        if (r->grp[i] < 0 && !r->laid[i]) sx += r->x[i], m++;
    float x = m ? sx / m : r->x[r->n - 1] + 0.8f * w, d = 0.8f * ui.unit;
    if (!m && r->n) x = fminf(x, T.b.x + T.b.w - d / 2);
    float dy = below ? y + h / 2 + 0.62f * ui.unit : y - h / 2 - 0.62f * ui.unit;
    ui_put_c(t_disc, x, dy, d, d);
    ui_number(x, dy, 0.4f * ui.unit, dead_points, 1);
}

/* ---- the viewer's order of their hand ---------------------------------------------------------------------------- */
static uint8_t order[2][11];
static int norder[2], manual[2];

/* a hand in its order: the best melds first (by themselves), or as the player put it (new cards at the end); the
 * groups: each card's meld */
static int hand_row(const gnpos *p, int pl, row *r)
{
    gin_arr a;
    int dead = gin_arrange(p->hand[pl], p->n[pl], &a);
    int8_t meld_of[128];
    memset(meld_of, -1, sizeof meld_of);
    for (int m = 0, k = 0; m < a.nm; m++)
        for (int i = 0; i < a.mlen[m]; i++) meld_of[a.card[k++] & 127] = (int8_t)m;
    memset(r, 0, sizeof *r);
    if (pl != T.me) {                          /* the other's, face down: as dealt and drawn (nothing told) */
        r->n = p->n[pl];
        memcpy(r->card, p->hand[pl], p->n[pl]);
        for (int i = 0; i < r->n; i++) r->grp[i] = -1;
        return dead;
    }
    if (!manual[pl]) {
        memcpy(order[pl], a.card, a.n);
        norder[pl] = a.n;
    } else {                                    /* the player's order: the cards still held, then the new ones */
        int k = 0;
        uint8_t o[11];
        for (int i = 0; i < norder[pl]; i++)
            if (memchr(p->hand[pl], order[pl][i], p->n[pl])) o[k++] = order[pl][i];
        for (int i = 0; i < p->n[pl]; i++)
            if (!memchr(o, p->hand[pl][i], (size_t)k)) o[k++] = p->hand[pl][i];
        memcpy(order[pl], o, (size_t)k);
        norder[pl] = k;
    }
    r->n = norder[pl];
    r->gaps = !manual[pl];
    for (int i = 0; i < r->n; i++) {
        r->card[i] = order[pl][i];
        r->grp[i] = meld_of[r->card[i] & 127];
    }
    return dead;
}

/* the end of a hand: a player's melds (the knocker's with the cards laid off on them, lit), then their deadwood (the
 * defender's: less the cards laid off, unless `before`) */
static int end_row(const gnpos *p, int pl, int before, row *r)
{
    const gin_result *s = &p->res;
    const gin_arr *a = &s->arr[pl];
    memset(r, 0, sizeof *r);
    r->gaps = 1;
    int k = 0;
    for (int m = 0; m < a->nm; m++) {
        int g0 = r->n;
        for (int i = 0; i < a->mlen[m]; i++) r->card[r->n] = a->card[k++], r->grp[r->n++] = (int8_t)m;
        if (pl == s->knocker)
            for (int i = 0; i < s->nlay; i++)
                if (s->layto[i] == m) r->card[r->n] = s->lay[i], r->grp[r->n] = (int8_t)m, r->laid[r->n++] = 1;
        for (int i = g0 + 1; i < r->n; i++)        /* a run grown at its ends: in order */
            for (int j = i; j > g0 && CARD_RANK(r->card[j]) < CARD_RANK(r->card[j - 1]); j--) {
                uint8_t c = r->card[j], l = r->laid[j];
                r->card[j] = r->card[j - 1], r->laid[j] = r->laid[j - 1];
                r->card[j - 1] = c, r->laid[j - 1] = l;
            }
    }
    for (; k < a->n; k++) {
        int laid = 0;
        for (int i = 0; i < s->nlay && pl != s->knocker; i++) laid |= s->lay[i] == a->card[k];
        if (laid && !before) continue;
        r->card[r->n] = a->card[k], r->grp[r->n++] = -1;
    }
    return s->dead[pl];
}

static int in_end(const gnpos *p) { return (p->phase == GN_END || p->phase == GN_OVER) && p->res.kind != GR_DEAD; }

/* where a player's row lies: the viewer's at the bottom, big; the other's at the top */
static float row_y(int pl) { return pl == T.me ? T.hy : T.oy; }
static float row_w(int pl) { return pl == T.me ? T.hw : T.ow; }

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
enum { H_NONE, H_HAND, H_PILE, H_LAY };
static cm_table M;
static Uint32 t_flip, t_lay, t_panel;           /* the end of a hand: the cards turn up, the layoffs fly, the points */

static void calm(void)
{
    cm_calm(&M);
    t_flip = t_lay = t_panel = 0;
}

static int busy(void) { return cm_busy(&M); }

static void slide(int card, int up0, float x0, float y0, float w0, int up1, float x1, float y1, float w1, int ms,
                  int delay, int hide, int pl, int s, int sound)
{
    cm_slide(&M, deck, card, up0, x0, y0, w0, up1, x1, y1, w1, ms, delay, hide, pl, s, sound);
}

/* a card's place in a player's hand as it is drawn now */
static int hand_spot(const gnpos *p, int pl, int card, float *x, float *y, float *w)
{
    row r;
    hand_row(p, pl, &r);
    row_place(&r, T.b.x + T.b.w / 2, row_w(pl));
    for (int i = 0; i < r.n; i++)
        if (r.card[i] == card) {
            *x = r.x[i], *y = row_y(pl), *w = row_w(pl);
            return 1;
        }
    *x = T.b.x + T.b.w / 2, *y = row_y(pl), *w = row_w(pl);
    return 0;
}

static void deal_slides(void)
{
    const gnpos *p = &G.pos;
    int k = 0, first = 1 - p->dealer;
    for (int i = 0; i < 10; i++)
        for (int j = 0; j < 2; j++, k++) {
            int q = j ? p->dealer : first, c = p->hand[q][i];
            float x, y, w;
            hand_spot(p, q, c, &x, &y, &w);
            slide(c, 0, T.sx, T.sy, T.pw, 0, x, y, w, DEAL_MS, k * 55, H_HAND, q, c, k % 3 == 0 ? SND_MOVE : -1);
        }
    int top = p->pile[p->npile - 1];
    slide(top, 0, T.sx, T.sy, T.pw, 1, T.dx, T.dy, T.pw, FLIP_MS, k * 55 + 100, H_PILE, 0, 0, SND_PLACE);
}

/* the end of a hand: the other's cards turn up where they are; both hands spread in melds; the layoffs fly */
static void end_slides(const gnpos *b)
{
    const gnpos *p = &G.pos;
    Uint32 now = ui_ms();
    t_flip = now + SLIDE_MS + 100;
    int o = 1 - T.me, k = 0;
    row r;
    end_row(p, o, 1, &r);
    row_place(&r, T.b.x + T.b.w / 2, T.ow);
    for (int i = 0; i < r.n; i++) {             /* the other's cards turn up, from their hand to their row */
        if (r.laid[i]) continue;
        float x, y, w;
        hand_spot(b, o, r.card[i], &x, &y, &w);
        slide(r.card[i], 0, x, y, w, 1, r.x[i], T.oy, T.ow, FLIP_MS, (int)(t_flip - now) + 40 * k, H_HAND, o,
              r.card[i], k == 0 ? SND_PLACE : -1);
        k++;
    }
    t_lay = t_flip + FLIP_MS + 40 * k + 300;
    const gin_result *s = &p->res;
    int df = 1 - s->knocker;
    row from, to;
    end_row(p, df, 1, &from);
    end_row(p, s->knocker, 0, &to);
    row_place(&from, T.b.x + T.b.w / 2, row_w(df));
    row_place(&to, T.b.x + T.b.w / 2, row_w(s->knocker));
    for (int i = 0, n = 0; i < to.n; i++) {     /* the layoffs: from the defender's deadwood to the knocker's melds */
        if (!to.laid[i]) continue;
        for (int j = 0; j < from.n; j++)
            if (from.card[j] == to.card[i])
                slide(to.card[i], 1, from.x[j], row_y(df), row_w(df), 1, to.x[i], row_y(s->knocker),
                      row_w(s->knocker), LAY_MS, (int)(t_lay - now) + 260 * n, H_LAY, 0, to.card[i],
                      SND_PLACE);
        n++;
        t_panel = t_lay + 260 * n + LAY_MS;
    }
    if (!t_panel) t_panel = t_lay;
    cm_until(&M, t_panel + 600);
}

static void animate(const gnpos *b, int a)
{
    const gnpos *p = &G.pos;
    int k = GNA_KIND(a), c = GNA_CARD(a), pl = b->turn;
    float x, y, w;
    switch (k) {
    case GNA_STOCK:
    case GNA_TAKE: {
        int card = p->hand[pl][p->n[pl] - 1];
        hand_spot(p, pl, card, &x, &y, &w);
        if (k == GNA_STOCK) slide(card, 0, T.sx, T.sy, T.pw, pl == T.me, x, y, w, SLIDE_MS, 0, H_HAND, pl, card, SND_MOVE);
        else slide(card, 1, T.dx, T.dy, T.pw, pl == T.me, x, y, w, SLIDE_MS, 0, H_HAND, pl, card, SND_MOVE);
        break;
    }
    case GNA_DISCARD:
    case GNA_KNOCK:
        hand_spot(b, pl, c, &x, &y, &w);
        if (drag.on && drag.moved) x = drag.x, y = drag.y;
        slide(c, pl == T.me, x, y, w, 1, T.dx, T.dy, T.pw, SLIDE_MS, 0, H_PILE, 0, 0, SND_PLACE);
        if (k == GNA_KNOCK) end_slides(b);
        break;
    case GNA_NEXT:
        deal_slides();
        break;
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    gin_new(&G, (uint32_t)ui_random(1 << 30));
    calm();
    manual[0] = manual[1] = norder[0] = norder[1] = 0;
    sel = CARD_NONE;
    floor_n = 0;
    dealing = 1;
}

static int status(void)
{
    int w = gin_winner(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    gnpos before = G.pos;
    if (!gin_play(&G, a)) { ui_sound(SND_DENY); return; }
    calm();
    sel = CARD_NONE;
    const gnpos *p = &G.pos;
    if (GNA_KIND(a) == GNA_NEXT) manual[0] = manual[1] = 0;   /* a new hand: arranged by itself again */
    animate(&before, a);
    drag.on = 0;
    if (p->phase != GN_END && p->phase != GN_OVER && p->turn != ui.viewer) {
        floor_n = G.n;
        ui_handoff(p->turn);                    /* the device passes (after the cards) */
    }
}

static int undo(void)
{
    if (G.n <= floor_n || !gin_undo(&G)) return 0;
    calm();
    sel = CARD_NONE;
    dealing = 0;
    return 1;
}

static int script(const char *s)
{
    int ok = gin_script(&G, s);
    calm();
    sel = CARD_NONE;
    dealing = 0;
    floor_n = G.n;
    ui_handoff(G.pos.turn);
    return ok;
}

static int can_pass(void) { return G.pos.phase == GN_OFFER && !busy(); }
static void pass(void) { act(GNA_PASS); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_tok[2], *t_pill, *t_cross, *t_ring;
static int tok_size, disc_size, pill_w, cross_size, ring_size;

static void textures(void)
{
    float u = ui.unit;
    int ts = (int)(0.62f * u);
    if (ts != tok_size) {
        tok_size = ts;
        for (int q = 0; q < 2; q++) {
            ui_free(&t_tok[q]);
            cards_token(q, ts, ui_buf(ts, ts));
            t_tok[q] = ui_tex(ts, ts, ui_buf(ts, ts));
        }
    }
    int ds = (int)(0.86f * u);
    if (ds != disc_size) {
        disc_size = ds;
        const uint8_t dark[3] = {24, 30, 28};
        ui_free(&t_disc);
        art_disc(ds, dark, 0.84f, 0, ui_buf(ds, ds));
        t_disc = ui_tex(ds, ds, ui_buf(ds, ds));
    }
    int pw = (int)(1.5f * u), ph = (int)(0.58f * u);
    if (pw != pill_w) {
        pill_w = pw;
        const uint8_t c[4] = {24, 30, 28, 220};
        ui_free(&t_pill);
        art_roundrect(pw, ph, ph / 2.0f, c, ui_buf(pw, ph));
        t_pill = ui_tex(pw, ph, ui_buf(pw, ph));
    }
    int cs = (int)(0.34f * u);
    if (cs != cross_size) {
        cross_size = cs;
        ui_free(&t_cross);
        art_icon(ICON_CLOSE, cs, ui_buf(cs, cs));
        t_cross = ui_tex(cs, cs, ui_buf(cs, cs));
        SDL_SetTextureColorMod(t_cross, 250, 214, 90);
    }
    int rs = (int)(2 * T.kr);
    if (rs != ring_size) {
        ring_size = rs;
        const uint8_t gold[3] = {250, 214, 90};
        ui_free(&t_ring);
        art_ring(rs, 0.86f, gold, 1, ui_buf(rs, rs));
        t_ring = ui_tex(rs, rs, ui_buf(rs, rs));
    }
}

/* the knock button: a dark disc in a gold ring, a card face down on it and the deadwood the knock leaves */
static int knock_card(const gnpos *p)          /* the card the knock throws: the chosen one, if it can */
{
    if (sel != CARD_NONE && gin_legal(p, GNA_KNOCK | sel)) return sel;
    return CARD_NONE;
}

static void draw_knock(const gnpos *p)
{
    int c = knock_card(p), pl = p->turn;
    if (c != CARD_NONE) ui_mark_gold(T.kx, T.ky, 3.2f * T.kr);
    ui_put_c(t_disc, T.kx, T.ky, 2 * T.kr, 2 * T.kr);
    ui_put_c(t_ring, T.kx, T.ky, 2 * T.kr, 2 * T.kr);
    cards_draw(deck, CARD_NONE, T.kx, T.ky - 0.3f * T.kr, 0.62f * T.kr, -12, CD_DOWN | CD_FLAT);
    int best = 99;
    for (int i = 0; i < p->n[pl]; i++) {
        int d = gin_legal(p, GNA_KNOCK | p->hand[pl][i]) ? gin_without(p->hand[pl], p->n[pl], p->hand[pl][i]) : 99;
        best = d < best ? d : best;
    }
    int d = c != CARD_NONE ? gin_without(p->hand[pl], p->n[pl], c) : best;
    ui_number(T.kx, T.ky + 0.42f * T.kr, 0.5f * T.kr, d, 1);
}

/* a row of cards (a hand, or the end's melds): face up or down, the chosen card raised */
static void draw_row(const gnpos *p, int pl, const row *r, float y, float w, int up, int dead_points)
{
    Uint32 now = ui_ms();
    int mine = pl == T.me && p->phase == GN_DISCARD && p->turn == T.me && !busy();
    if (up) row_marks(r, y, w, dead_points, pl != T.me);
    for (int i = 0; i < r->n; i++) {
        int c = r->card[i];
        if (cm_hides(&M, H_HAND, pl, c) || (r->laid[i] && cm_hides(&M, H_LAY, 0, c))) continue;
        if (drag.on && drag.moved && drag.card == c && pl == T.me) continue;
        if (r->laid[i] && t_lay && now < t_lay) continue;
        int how = up ? 0 : CD_DOWN;
        float cy = y;
        if (pl == T.me && c == sel) how |= CD_RAISED, cy -= 0.18f * w;
        if (mine && c == p->taken) how |= CD_DIM;
        if (r->laid[i]) how |= CD_GLOW;
        cards_draw(deck, c, r->x[i], cy, w, 0, how);
    }
}

/* the end of a hand: the points to the winner (the bonus apart: gin, an undercut); a dead hand: the stock crossed */
static void draw_result(const gnpos *p)
{
    const gin_result *s = &p->res;
    float u = ui.unit, cx = T.b.x + T.b.w / 2, cy = T.my;
    if (s->kind == GR_DEAD) {
        cards_pile(deck, cx, cy, T.pw * 0.8f, p->nstock, CARD_NONE, 0);
        ui_put_c(t_cross, cx, cy, 1.4f * u, 1.4f * u);
        return;
    }
    if (p->phase == GN_OVER) {                  /* the game: points, boxes, the game bonus, a shutout; the totals */
        float rh = 1.3f * u, y0 = cy - rh / 2;
        for (int k = 0; k < 2; k++) {
            int q = k ? T.me : 1 - T.me, w = gin_winner(p) == q;
            float y = y0 + k * rh, x = T.b.x + 0.5f * u;
            if (w) ui_mark_gold(x, y, 1.3f * u);
            ui_put_c(t_tok[q], x, y, (float)tok_size, (float)tok_size);
            x += 1.0f * u;
            ui_number(x, y, 0.44f * u, p->score[q], 1);
            x += 1.1f * u;
            for (int b = 0; b < p->boxes[q] && b < 8; b++)    /* the hands won: a small gold box each */
                ui_fill(x + b * 0.22f * u - 0.08f * u, y - 0.08f * u, 0.16f * u, 0.16f * u, 250, 214, 90, 255);
            x += fmaxf(1, (float)(p->boxes[q] < 8 ? p->boxes[q] : 8)) * 0.22f * u + 0.65f * u;
            if (p->boxes[q]) cm_plus(x, y, 0.4f * u, GIN_BOX * p->boxes[q]);
            x += 1.3f * u;
            if (w) cm_plus(x, y, 0.4f * u, GIN_GAME_BONUS);
            x += 1.2f * u;
            if (w && p->shutout) {
                ui_put_c(t_cross, x - 0.15f * u, y, (float)cross_size, (float)cross_size);
                ui_number(x + 0.25f * u, y, 0.4f * u, 2, 1);
            }
            ui_put_c(t_disc, T.b.x + T.b.w - 0.75f * u, y, 1.12f * u, 1.12f * u);
            ui_number(T.b.x + T.b.w - 0.75f * u, y, 0.42f * u, p->final[q], 1);
        }
        return;
    }
    int w = s->winner, bonus = s->bonus;
    float x = cx - (bonus ? 1.6f : 1.0f) * u;
    ui_mark_gold(x, cy, 1.6f * u);
    ui_put_c(t_tok[w], x, cy, 0.9f * u, 0.9f * u);
    cm_plus(x + 1.35f * u, cy, 0.56f * u, s->points - bonus);
    if (bonus) {                                /* gin, an undercut: the 25 apart, in a gold ring */
        float bx = x + 3.0f * u;
        ui_put_c(t_disc, bx, cy, 1.25f * u, 1.25f * u);
        ui_put_c(t_ring, bx, cy, 1.25f * u, 1.25f * u);
        cm_plus(bx, cy, 0.4f * u, bonus);
    }
}

static void draw(void)
{
    const gnpos *p = &G.pos;
    if (dealing) { dealing = 0; deal_slides(); }
    textures();
    Uint32 now = ui_ms();
    int b = busy(), me = T.me, o = 1 - me, ph = p->phase, mine = p->turn == me && !ui_handing() && !b;
    int end = in_end(p);
    /* the stock and the discard pile (not at the end of a hand, once the points show) */
    int piles = !end || now < t_panel;
    if (piles && !(ph == GN_END && p->res.kind == GR_DEAD && !b)) {
        int glow = mine && (ph == GN_DRAW || ph == GN_OFFER);
        cards_pile(deck, T.sx, T.sy, T.pw, p->nstock, CARD_NONE, glow && ph == GN_DRAW ? CD_GLOW : 0);
        int np = p->npile - cm_hiding(&M, H_PILE);
        int take = glow && (ph == GN_OFFER || p->passes < 2), drop = mine && ph == GN_DISCARD && sel != CARD_NONE &&
                                                                  sel != p->taken;
        cards_pile(deck, T.dx, T.dy, T.pw, np, np > 0 ? p->pile[np - 1] : CARD_NONE, take || drop ? CD_GLOW : 0);
        if (mine && ph == GN_DISCARD && gin_can_knock(p)) draw_knock(p);
    }
    /* the hands, or the end's rows */
    row r;
    if (end && now >= t_flip) {
        for (int k = 0; k < 2; k++) {
            int q = k ? me : o, d = end_row(p, q, 0, &r);
            row_place(&r, T.b.x + T.b.w / 2, row_w(q));
            draw_row(p, q, &r, row_y(q), row_w(q), 1, now >= t_panel ? d : -1);
        }
    } else {
        hand_row(p, o, &r);
        row_place(&r, T.b.x + T.b.w / 2, T.ow);
        draw_row(p, o, &r, T.oy, T.ow, 0, -1);
        int d = hand_row(p, me, &r);
        row_place(&r, T.b.x + T.b.w / 2, T.hw);
        draw_row(p, me, &r, T.hy, T.hw, 1, ph == GN_END || ph == GN_OVER ? -1 : d);
    }
    cm_draw(&M);
    if (drag.on && drag.moved) cards_draw(deck, drag.card, drag.x, drag.y, T.hw, 0, CD_RAISED);
    if ((ph == GN_END || ph == GN_OVER) && now >= t_panel && !(b && p->res.kind == GR_DEAD)) draw_result(p);
    cm_floats_draw(&M, t_pill, NULL);
    if (b) ui_animate();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int hand_hit(float x, float y)           /* the viewer's card under the finger (CARD_NONE) */
{
    const gnpos *p = &G.pos;
    row r;
    hand_row(p, T.me, &r);
    row_place(&r, T.b.x + T.b.w / 2, T.hw);
    cards_spot sp[ROWMAX];
    for (int i = 0; i < r.n; i++) sp[i] = (cards_spot){r.x[i], T.hy - (r.card[i] == sel ? 0.18f * T.hw : 0), 0};
    int i = cards_hit(sp, r.n, T.hw, aspect(), x, y);
    return i < 0 ? CARD_NONE : r.card[i];
}

static int on_knock(float x, float y) { return hypotf(x - T.kx, y - T.ky) <= T.kr * 1.1f; }

static void press(float x, float y)
{
    const gnpos *p = &G.pos;
    drag.on = 0;
    if (ui_handing() || p->phase == GN_OVER) return;
    if (p->phase == GN_END) {
        if (busy()) calm();                     /* a tap hurries the end on */
        else act(GNA_NEXT);
        return;
    }
    int c = hand_hit(x, y);
    if (c != CARD_NONE) {                       /* a card: chosen, dragged (the turn or not: to reorder) */
        drag.on = 1, drag.card = c, drag.moved = 0;
        drag.x0 = drag.x = x, drag.y0 = drag.y = y;
        return;
    }
    if (p->turn != T.me || busy()) { ui_sound(SND_DENY); return; }
    int on_stock = cards_in(T.sx, T.sy, T.pw * 1.1f, aspect(), x, y);
    int on_pile = cards_in(T.dx, T.dy, T.pw * 1.1f, aspect(), x, y);
    if (p->phase == GN_DISCARD && on_knock(x, y) && gin_can_knock(p)) {
        int k = knock_card(p);
        if (k != CARD_NONE) { act(GNA_KNOCK | k); return; }
        int best = 99;                          /* no card chosen that can: the best one is chosen, tap again */
        for (int i = 0; i < p->n[p->turn]; i++) {
            int h = p->hand[p->turn][i], d = gin_legal(p, GNA_KNOCK | h) ? gin_without(p->hand[p->turn],
                                                                                     p->n[p->turn], h) : 99;
            if (d < best || (d == best && gin_value(h) > gin_value(sel))) best = d, sel = h;
        }
        ui_sound(SND_TAP);
        return;
    }
    int a = 0;
    if (p->phase == GN_OFFER || p->phase == GN_DRAW) a = on_stock ? GNA_STOCK : on_pile ? GNA_TAKE : 0;
    else if (p->phase == GN_DISCARD && on_pile && sel != CARD_NONE) a = GNA_DISCARD | sel;
    if (a && gin_legal(p, a)) act(a);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x, drag.y = y;
    if (hypotf(x - drag.x0, y - drag.y0) > 0.25f * ui.unit) drag.moved = 1;
}

/* a card let go: moved along the hand (its new place: arranged by hand from now on), dropped on the discard pile
 * (discarded), or tapped (chosen; tapped again: discarded) */
static void release(float x, float y)
{
    const gnpos *p = &G.pos;
    if (!drag.on || ui_handing()) { drag.on = 0; return; }
    int c = drag.card, myturn = p->turn == T.me && p->phase == GN_DISCARD && !busy();
    if (drag.moved) {
        if (cards_in(T.dx, T.dy, T.pw * 1.3f, aspect(), x, y) && myturn) {
            if (gin_legal(p, GNA_DISCARD | c)) act(GNA_DISCARD | c);
            else ui_sound(SND_DENY);
        } else if (y > T.hy - T.hw * aspect()) {     /* along the hand: its new place */
            row r;
            hand_row(p, T.me, &r);
            row_place(&r, T.b.x + T.b.w / 2, T.hw);
            uint8_t o[11];
            int k = 0, put = 0;
            for (int i = 0; i < r.n; i++) {
                if (r.card[i] == c) continue;
                if (!put && x < r.x[i]) o[k++] = (uint8_t)c, put = 1;
                o[k++] = r.card[i];
            }
            if (!put) o[k++] = (uint8_t)c;
            memcpy(order[T.me], o, (size_t)k);
            norder[T.me] = k;
            manual[T.me] = 1;
            ui_sound(SND_TAP);
        }
        drag.on = 0;
        return;
    }
    drag.on = 0;
    if (!myturn) return;
    if (c == sel) {
        if (gin_legal(p, GNA_DISCARD | c)) act(GNA_DISCARD | c);
        else ui_sound(SND_DENY);
    } else {
        sel = c;
        ui_sound(SND_TAP);
    }
}

/* the deadwood's points: a tap puts the hand back in order (arranged by itself) */
static int dead_tap(float x, float y)
{
    (void)x;
    return manual[T.me] && y > T.hy - T.hw * aspect() / 2 - 1.0f * ui.unit && y < T.hy - T.hw * aspect() / 2 - 0.1f *
                                                                                ui.unit;
}

static void press_all(float x, float y)
{
    if (!ui_handing() && G.pos.phase != GN_END && G.pos.phase != GN_OVER && dead_tap(x, y) &&
        hand_hit(x, y) == CARD_NONE) {
        manual[T.me] = 0;
        ui_sound(SND_TAP);
        return;
    }
    press(x, y);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the points; the hands won, a small gold box each */
{
    const gnpos *p = &G.pos;
    float h = fminf(0.46f * ui.unit, b.h * 0.6f), u = ui.unit;
    int v = p->score[player], digits = v >= 100 ? 3 : v >= 10 ? 2 : 1;
    float w = (0.74f * (digits - 1) + 0.62f) * h;
    ui_number(b.x + w / 2, b.y + b.h / 2, h, v, 1);
    for (int k = 0; k < p->boxes[player] && k < 8; k++)
        ui_fill(b.x + w + 0.25f * u + k * 0.22f * u, b.y + b.h / 2 - 0.08f * u, 0.15f * u, 0.15f * u, 250, 214, 90,
                255);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: a hand in melds (a run, a set) and a card of deadwood, a gold line under each meld */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int hand[7] = {CARD(SUIT_HEARTS, 7), CARD(SUIT_HEARTS, 8), CARD(SUIT_HEARTS, 9),
                                CARD(SUIT_SPADES, RANK_QUEEN), CARD(SUIT_DIAMONDS, RANK_QUEEN),
                                CARD(SUIT_CLUBS, RANK_QUEEN), CARD(SUIT_DIAMONDS, 1)};
    memset(rgba, 0, (size_t)size * size * 4);
    int cw = size * 30 / 100, ch = (int)(cw * 1.4f);
    float gap = 0.2f * cw, step = (size * 0.94f - cw - 2 * gap) / 6, xs[8], x = size * 0.03f;
    for (int i = 0; i < 7; i++) {
        if (i == 3 || i == 6) x += gap;
        xs[i] = x;
        x += step;
    }
    xs[7] = x + gap;
    uint8_t *c = malloc((size_t)cw * ch * 4);
    if (!c) return;
    int y = (size - ch) / 2 - size / 20;
    for (int i = 0; i < 7; i++) {
        cards_paint(DECK_FRENCH, hand[i], cw, ch, c);
        art_blend(rgba, size, size, c, cw, ch, (int)xs[i], y);
    }
    free(c);
    int t = size / 40 > 2 ? size / 40 : 2, ly = y + ch + size / 30;
    for (int m = 0; m < 2; m++) {               /* the gold lines under the melds, to where the next group begins */
        int x0 = (int)xs[m * 3] + cw / 12, x1 = (int)xs[m * 3 + 3] - cw / 12;
        for (int yy = ly; yy < ly + t && yy < size; yy++)
            for (int xx = x0; xx < x1 && xx < size; xx++) {
                uint8_t *q = rgba + 4 * (yy * size + xx);
                q[0] = 250, q[1] = 214, q[2] = 90, q[3] = 255;
            }
    }
}

static const game ginrummy_game = {
    .id = "ginrummy", .order = 294, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press_all, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy,
    .view = VIEW_HIDDEN,
};
REGISTER_GAME(ginrummy_game)
