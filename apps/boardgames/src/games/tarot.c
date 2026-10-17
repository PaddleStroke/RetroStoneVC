/*
 * French Tarot in RetroStone Board: 3, 4 or 5 players passing the device round (the rules: tarot_rules.c, tarot.h).
 * VIEW_HIDDEN: the screen is the player to move's, a cover at every hand-off. Their hand fanned at the bottom of the
 * table (in two rows when it is long), the trick in the middle (each player's card on their side, their token at its
 * corner; the trick just won stays there, its winner's card in gold light, until the next card is led), along the
 * top the tricks played (tap: the last trick again), the dog, the called king and the poignées shown, the cards put
 * aside. The bars: each player's total, their cards in hand, their bid.
 * The bidding: buttons carrying the bids' words, the first words a game of this app shows outside its rules page:
 * help/<lang>/tarot_bids.txt (its first line a title, then the pass and the four bids in full, then the same five
 * short for the bars), read with text_get() and drawn with text_line(). The rest: cards, tokens and digits.
 * The dog turned up (Petite, Garde) shows face up to everyone with the taker's hand face down; the taker taps it to
 * take it, then discards in private (a tap a card; the discard's cards go back with a tap; the round button ends it).
 * A deal's count: a table of the contract, the taker's side's oudlers and points against the points needed, the
 * bonuses, each player's points and total.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/tarot.h"
#include "games/endtable.h"
#include "pages.h"
#include "shell.h"
#include "text.h"

#define AS 1.6f                                 /* cards_aspect(DECK_TAROT) */
#define DK DECK_TAROT
#define SLIDE_MS 300
#define FLIP_MS 260
#define DEAL_MS 260
#define DEAL_GAP 55                             /* between two packets of the deal */
#define REST_MS 300                             /* after the last card lands: a moment before the device passes */
#define TRICK_REST 700                          /* a trick complete: a moment to see who wins it */
#define PI_F 3.14159265f

static tgame G;
static int mark;                                /* the history's length at the last hand-off: undo stops there */
static int pend_deal, pend_dog;                 /* the deal flies, the dog turns up, once the right view shows */
static int showlast;                            /* the last trick shown again (the pile tapped) */

static const uint8_t CREAM[3] = {246, 238, 220}, GOLD[3] = {250, 214, 90};

/* ---- the words: help/<lang>/tarot_bids.txt ----------------------------------------------------------------------- */
static void word(int bid, int shrt, char *buf, int size)
{
    const char *t = text_get("tarot_bids");
    buf[0] = 0;
    for (int k = 0; t && k < 1 + bid + (shrt ? 5 : 0); k++) {
        t = strchr(t, '\n');
        if (t) t++;
    }
    if (!t) return;
    int n = (int)strcspn(t, "\n");
    if (n >= size) n = size - 1;
    memcpy(buf, t, (size_t)n);
    buf[n] = 0;
}

#define LABELS 24
static struct { SDL_Texture *t; int key, w, h; unsigned used; } lab[LABELS];   /* the words made, by size */
static unsigned lab_tick;

/* a bid's word as a picture, px pixels, no wider than maxw (shrunk to fit): its texture, its size. gold: 1 gold, else
 * cream; plus W_TABLE: on the table (a bar), where the light theme's ink is dark (its gold deeper) */
enum { W_TABLE = 2 };
static SDL_Texture *word_tex(int bid, int shrt, float px, float maxw, int gold, int *w, int *h)
{
    static const uint8_t INK[3] = {46, 40, 30}, INK_GOLD[3] = {150, 118, 46};
    int light = (gold & W_TABLE) && !TH.dark;
    const uint8_t *rgb = gold & 1 ? (light ? INK_GOLD : GOLD) : light ? INK : CREAM;
    gold &= 1;
    int key = (((((int)(px * 4) * 8192 + (int)maxw) * 5 + bid) * 2 + shrt) * 2 + gold) * 2 + light, k = 0;
    for (int i = 0; i < LABELS; i++)
        if (lab[i].t && lab[i].key == key) {
            lab[i].used = ++lab_tick;
            *w = lab[i].w;
            *h = lab[i].h;
            return lab[i].t;
        }
    for (int i = 1; i < LABELS && lab[k].t; i++)   /* an empty place, else the least used lately */
        if (!lab[i].t || lab[i].used < lab[k].used) k = i;
    char s[80];
    word(bid, shrt, s, sizeof s);
    int tw = text_line(s, px, 1, CREAM, NULL, 0, 0, 0, 0);
    if (tw > maxw && tw > 0) {
        px = fmaxf(6, px * maxw / tw);
        tw = text_line(s, px, 1, CREAM, NULL, 0, 0, 0, 0);
    }
    int W = tw + 4, H = (int)ceilf(px * 1.55f);
    uint8_t *b = calloc((size_t)W * H, 4);
    if (!b) return NULL;
    text_line(s, px, 1, rgb, b, W, H, 2, (int)((H - 1.25f * px) / 2));
    ui_free(&lab[k].t);
    lab[k].t = ui_tex(W, H, b);
    lab[k].key = key;
    lab[k].w = *w = W;
    lab[k].h = *h = H;
    lab[k].used = ++lab_tick;
    free(b);
    return lab[k].t;
}

/* ---- the table's geometry (it follows the viewer) ---------------------------------------------------------------- */
static struct {
    box b;
    int me;
    float u;
    float hx, hy, hw, hh;                       /* the hand's zone */
    float by, bh;                               /* the band along the top */
    float lx, ly, ex, ey, ws;                   /* the tricks played, the cards put aside: centres, width */
    float zy0, zy1;                             /* the middle zone */
    float tx, ty, wt;                           /* the trick: its middle, the cards' width */
    float pw, ph, pg;                           /* the bidding buttons */
} T;

static void launch(void);

static void layout(box b)
{
    const tpos *p = &G.pos;
    float u = ui.unit;
    T.b = b;
    T.u = u;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= p->players) T.me = 0;
    int land = b.w > b.h * 1.05f;
    T.bh = floorf(fminf(1.5f * u, 0.16f * b.h));
    T.by = b.y;
    T.ws = floorf(T.bh * 0.8f / AS);
    T.lx = b.x + 0.15f * u + 0.6f * T.ws;
    T.ex = b.x + b.w - 0.15f * u - 0.9f * T.ws;
    T.ly = T.ey = T.by + T.bh / 2;
    T.hh = floorf(b.h * (land ? 0.39f : 0.4f));
    T.hw = b.w * 0.98f;
    T.hx = b.x + (b.w - T.hw) / 2;
    T.hy = b.y + b.h - T.hh;
    T.zy0 = T.by + T.bh;
    T.zy1 = T.hy;
    float zh = T.zy1 - T.zy0;
    int n = p->players;
    static const float ext_y[6] = {0, 0, 0, 2.05f, 2.12f, 2.2f}, ext_x[6] = {0, 0, 0, 2.6f, 3.4f, 5.2f};
    T.wt = floorf(fminf(fminf(zh * 0.97f / (AS * ext_y[n]), b.w * 0.96f / ext_x[n]), 1.6f * u));
    T.tx = b.x + b.w / 2;
    T.ty = T.zy0 + zh / 2 + (n == 3 ? 0.025f * AS * T.wt : 0);
    T.pg = 0.25f * u;
    T.pw = floorf(fminf(3.9f * u, (b.w - 3 * T.pg) / 2));
    T.ph = floorf(fminf(0.95f * u, (zh - 4 * T.pg) / 3));
    launch();                                   /* the cover lifted: the deal, the dog, for the viewer's eyes */
}

/* a player's seat round the trick: the viewer at the bottom, the others in turn order from the left (in card widths
 * and heights from the middle; no two cards overlap) */
static void seat(int pl, float *x, float *y)
{
    static const float at[6][5][2] = {
        [3] = {{0, 0.55f}, {-0.8f, -0.5f}, {0.8f, -0.5f}},
        [4] = {{0, 0.56f}, {-1.2f, 0}, {0, -0.56f}, {1.2f, 0}},
        [5] = {{0, 0.6f}, {-2.1f, 0}, {-0.62f, -0.6f}, {0.62f, -0.6f}, {2.1f, 0}},
    };
    int n = G.pos.players, k = (pl - T.me + n) % n;
    *x = T.tx + at[n][k][0] * T.wt;
    *y = T.ty + at[n][k][1] * T.wt * AS;
}

/* the viewer's hand: one row, or two (the second over the first's lower half), fanned by up to ARC degrees: the
 * outer cards hang lower by up to drop() */
#define ARC 8.0f
static float drop(void) { return 0.25f * T.hw * tanf(ARC / 2 * PI_F / 180) + 0.03f * T.hh; }

static void hand_geom(int n, int *rows, int *per, float *w)
{
    int ref = n > G.pos.handsize ? n : G.pos.handsize, br = 1;
    float best = 0;
    for (int r = 1; r <= 2; r++) {
        int pr = (ref + r - 1) / r;
        float wr = fminf((T.hh * 0.97f - drop()) / (AS * (1 + 0.52f * (r - 1)) + 0.1f), T.hw / (1 + 0.46f * (pr - 1)));
        if (wr > best * 1.06f) best = wr, br = r;
    }
    *w = floorf(fminf(best, 1.9f * T.u));
    *rows = br == 2 && n > 5 ? 2 : 1;
    *per = (n + *rows - 1) / *rows;
}

static void hand_spots(int n, cards_spot *s, float *w)
{
    int rows, per;
    hand_geom(n, &rows, &per, w);
    float h = *w * AS, total = h * (1 + 0.52f * (rows - 1));
    float top = T.hy + (T.hh - total - drop()) / 2 + 0.05f * h;
    for (int r = 0, i = 0; r < rows; r++) {
        int cnt = rows == 1 ? n : r == 0 ? per : n - per;
        cards_fan(T.hx + T.hw / 2, top + h / 2 + r * 0.52f * h, T.hw, *w, AS, cnt, cnt > 1 ? fminf(ARC, cnt * 0.8f) : 0,
                  s + i);
        i += cnt;
    }
}

static int hand_hit(int n, float x, float y)
{
    cards_spot s[TAROT_HAND];
    float w;
    int rows, per;
    hand_spots(n, s, &w);
    hand_geom(n, &rows, &per, &w);
    if (rows == 2) {
        int k = cards_hit(s + per, n - per, w, AS, x, y);
        if (k >= 0) return per + k;
        return cards_hit(s, per, w, AS, x, y);
    }
    return cards_hit(s, n, w, AS, x, y);
}

/* a row of k cards in the middle (the dog, the discard, the cards to call): centre of card i, their width */
static float row_w(int k) { return floorf(fminf(T.wt, T.b.w * 0.9f / (k * 1.1f))); }
static float row_x(int k, int i, float w, float shift) { return T.tx + shift + (i - (k - 1) / 2.0f) * 1.1f * w; }

/* the discard's row leaves room for the round button on its right */
static float ecart_shift(void) { return -0.75f * T.u; }
static void ok_button(float *x, float *y, float *r)
{
    int d = G.pos.dogsize;
    float w = fminf(row_w(d), (T.b.w * 0.92f - 1.6f * T.u) / (d * 1.1f));
    *r = 0.52f * T.u;
    *x = row_x(d, d - 1, w, ecart_shift()) + 0.55f * w + 0.35f * T.u + *r;
    *y = T.ty;
}
static float ecart_w(void)
{
    int d = G.pos.dogsize;
    return floorf(fminf(row_w(d), (T.b.w * 0.92f - 1.6f * T.u) / (d * 1.1f)));
}

/* the bidding buttons: Petite | Garde, Garde sans | Garde contre, then the pass across */
static box pill(int bid)
{
    float y0 = T.ty - 1.5f * T.ph - T.pg;
    if (bid == TB_PASS) return (box){T.tx - T.pw / 2, y0 + 2 * (T.ph + T.pg), T.pw, T.ph};
    int c = (bid - 1) % 2, r = (bid - 1) / 2;
    return (box){T.tx + (c ? T.pg / 2 : -T.pg / 2 - T.pw), y0 + r * (T.ph + T.pg), T.pw, T.ph};
}

static int in_box(box b, float x, float y) { return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h; }

/* ---- the band along the top: the dog (bidding), the called card, the poignée buttons and the poignées shown ---- */
enum { BI_DOG, BI_CALLED, BI_PILL, BI_SHOWN };
typedef struct { int kind, arg; float x, w; } bitem;

static int poignee_open(int level)              /* the viewer may show this poignée now */
{
    const tpos *p = &G.pos;
    return p->turn == T.me && tarot_legal(p, TACT(TA_POIGNEE, level));
}

static float shown_w(int pl)                    /* a poignée shown: a token and the trumps fanned */
{
    float w = T.ws * 0.85f;
    return 0.5f * T.bh + w + (G.pos.nshown[pl] - 1) * 0.52f * w;
}

static int band_items(bitem *it)
{
    const tpos *p = &G.pos;
    int n = 0;
    float g = 0.2f * T.u;
    if (p->phase == TP_BID || p->phase == TP_CALL || p->phase == TP_VOID) {
        it[n++] = (bitem){BI_DOG, 0, 0, T.ws * (1 + 0.35f * (p->dogsize - 1))};
    } else if (p->phase == TP_PLAY) {
        if (p->called != TAROT_NONE) it[n++] = (bitem){BI_CALLED, 0, 0, T.ws};
        for (int l = 1; l <= 3; l++)
            if (poignee_open(l)) it[n++] = (bitem){BI_PILL, l, 0, 1.25f * T.bh};
        if (p->tricks < 2)
            for (int k = 0; k < p->players; k++) {
                int q = (p->dealer + 1 + k) % p->players;
                if (p->poignee[q]) it[n++] = (bitem){BI_SHOWN, q, 0, shown_w(q)};
            }
    }
    float x0 = T.lx + T.ws, x1 = T.ex - T.ws, tot = 0;
    for (int i = 0; i < n; i++) tot += it[i].w + (i ? g : 0);
    float k = tot > x1 - x0 ? (x1 - x0) / tot : 1, x = (x0 + x1) / 2 - tot * k / 2;
    for (int i = 0; i < n; i++) {
        it[i].w *= k;
        it[i].x = x;
        x += it[i].w + g * k;
    }
    return n;
}

static float dog_band_x(int i)
{
    bitem it[8];
    int n = band_items(it);
    for (int k = 0; k < n; k++)
        if (it[k].kind == BI_DOG)
            return it[k].x + T.ws / 2 + i * 0.35f * T.ws * it[k].w / (T.ws * (1 + 0.35f * (G.pos.dogsize - 1)));
    return T.tx;
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
enum { H_NONE, H_HAND, H_TRICK, H_LAST, H_ASIDE, H_DOG, H_ECART };
static ct_moves M = {.rest = REST_MS};          /* the cards on the move: a moment after the last lands */

static void calm(void) { ct_calm(&M); }

static void slide(int card, int up0, float x0, float y0, float w0, float a0, int up1, float x1, float y1, float w1,
                  float a1, int ms, int delay, int hide, int sound)
{
    ct_slide(&M, DK, card, x0, y0, w0, a0, up0, x1, y1, w1, a1, up1, ms, delay, hide, card, -1, sound);
}

static void rest(int ms) { ct_until(&M, ui_ms() + (Uint32)ms); }
static int hides(int kind, int card) { return ct_hides(&M, kind, card, -1); }   /* card -1: how many of that kind */
static int busy(void) { return ct_busy(&M); }

static int index_of(const uint8_t *c, int n, int card)
{
    for (int i = 0; i < n; i++)
        if (c[i] == card) return i;
    return -1;
}

/* the viewer's hand moving from one layout to the next (cards come or go): every card that stays slides along */
static void relayout(const tpos *b, int up0, int skip)
{
    const tpos *p = &G.pos;
    int me = T.me;
    cards_spot s0[TAROT_HAND], s1[TAROT_HAND];
    float w0, w1;
    hand_spots(b->nhand[me], s0, &w0);
    hand_spots(p->nhand[me], s1, &w1);
    for (int i = 0; i < p->nhand[me]; i++) {
        int c = p->hand[me][i], j = index_of(b->hand[me], b->nhand[me], c);
        if (j < 0 || c == skip) continue;
        if (fabsf(s0[j].x - s1[i].x) < 0.5f && fabsf(s0[j].y - s1[i].y) < 0.5f && w0 == w1 && up0) continue;
        slide(c, up0, s0[j].x, s0[j].y, w0, s0[j].deg, 1, s1[i].x, s1[i].y, w1, s1[i].deg, up0 ? 220 : FLIP_MS + 60,
              up0 ? 0 : 20 * i, H_HAND, -1);
    }
}

/* the deal: packets of three flying from the dealer to each player (the viewer's turning up in their hand), the
 * dog's cards one by one to the band */
static void deal_slides(void)
{
    const tpos *p = &G.pos;
    float dx, dy, w;
    cards_spot s[TAROT_HAND];
    hand_spots(p->nhand[T.me], s, &w);
    seat(p->dealer, &dx, &dy);
    int packet = 0, nd = 0, inpk = 0;
    for (int q = 0; q < 78; q++) {
        int to = p->deal_to[q], c = p->deal_card[q], delay = packet * DEAL_GAP + inpk * 12;
        if (to == TAROT_DOG) {
            slide(c, 0, dx, dy, T.wt * 0.8f, 0, 0, dog_band_x(nd), T.ly, T.ws, 0, DEAL_MS, delay, H_DOG, -1);
            nd++;
            continue;
        }
        if (to == T.me) {
            int i = index_of(p->hand[T.me], p->nhand[T.me], c);
            slide(c, 0, dx, dy, T.wt * 0.8f, 0, 1, s[i].x, s[i].y, w, s[i].deg, DEAL_MS + 60, delay, H_HAND,
                  q % 9 == 0 ? SND_MOVE : -1);
        } else {
            float x, y;
            seat(to, &x, &y);
            slide(c, 0, dx, dy, T.wt * 0.8f, 0, 0, x, T.b.y - 0.3f * T.u, T.wt * 0.4f, 0, DEAL_MS, delay, H_NONE,
                  q % 9 == 0 ? SND_MOVE : -1);
        }
        if (++inpk == 3) inpk = 0, packet++;    /* a packet: three cards to one player */
    }
}

/* the dog turning up for all to see: from the band to the middle, face up */
static void dog_slides(void)
{
    const tpos *p = &G.pos;
    float w = row_w(p->dogsize);
    for (int i = 0; i < p->dogsize; i++)
        slide(p->dog[i], 0, dog_band_x(i), T.ly, T.ws, 0, 1, row_x(p->dogsize, i, w, 0), T.ty, w, 0, FLIP_MS + 120,
              140 * i, H_DOG, SND_PLACE);
}

/* the trick won, swept to the tricks played */
static void sweep(const tpos *b, int delay)
{
    for (int i = 0; i < b->players; i++) {
        float x, y;
        seat((b->lastlead + i) % b->players, &x, &y);
        slide(b->last[i], 1, x, y, T.wt, 0, 0, T.lx, T.ly, T.ws, 0, SLIDE_MS, delay + 30 * i, H_LAST,
              i == 0 ? SND_MOVE : -1);
    }
}

/* what an action moved, from the position before it: the viewer's own action from and to their hand; another
 * seat's (a bot's, the view staying with the person) from and to that seat's place, as the deal sends its cards,
 * face down but for the card played */
static void animate(const tpos *b, int act)
{
    const tpos *p = &G.pos;
    int k = TA_KIND(act), a = TA_ARG(act), me = T.me, own = (k == TA_PLAY ? b->turn : b->taker) == me;
    cards_spot s0[TAROT_HAND], s1[TAROT_HAND];
    float w0, w1, ax, ay, aw = T.wt * 0.4f;
    hand_spots(b->nhand[me], s0, &w0);
    hand_spots(p->nhand[me], s1, &w1);
    seat(k == TA_PLAY ? b->turn : b->taker, &ax, &ay);
    ay = T.b.y - 0.3f * T.u;
    switch (k) {
    case TA_BID:
    case TA_CALL:
        ui_sound(SND_TAP);
        if (p->phase == TP_PLAY && p->contract >= TB_SANS)   /* the dog put aside unseen */
            for (int i = 0; i < p->dogsize; i++)
                slide(p->dog[i], 0, dog_band_x(i), T.ly, T.ws, 0, 0, T.ex, T.ey, T.ws, 0, SLIDE_MS, 300 + 60 * i,
                      H_ASIDE, i == 0 ? SND_MOVE : -1);
        if (p->phase == TP_DOG) pend_dog = 1;      /* it turns up for the taker */
        break;
    case TA_POIGNEE: ui_sound(SND_TAP); break;
    case TA_TAKE: {
        float w = row_w(b->dogsize);
        if (own) relayout(b, 0, -1);
        for (int i = 0; i < b->dogsize; i++) {
            int j = index_of(p->hand[me], p->nhand[me], b->dog[i]);
            if (own) slide(b->dog[i], 1, row_x(b->dogsize, i, w, 0), T.ty, w, 0, 1, s1[j].x, s1[j].y, w1, s1[j].deg,
                           SLIDE_MS, 60 * i, H_HAND, i == 0 ? SND_MOVE : -1);
            else slide(b->dog[i], 1, row_x(b->dogsize, i, w, 0), T.ty, w, 0, 0, ax, ay, aw, 0, SLIDE_MS, 60 * i, H_NONE,
                       i == 0 ? SND_MOVE : -1);
        }
        break;
    }
    case TA_DISCARD: {
        int j = index_of(b->hand[me], b->nhand[me], a);
        float w = ecart_w();
        if (own) relayout(b, 1, a);
        slide(a, own, own ? s0[j].x : ax, own ? s0[j].y : ay, own ? w0 : aw, own ? s0[j].deg : 0, own,
              row_x(p->dogsize, p->necart - 1, w, ecart_shift()), T.ty, w, 0, 240, 0, H_ECART, SND_PLACE);
        break;
    }
    case TA_RETURN: {
        int j = index_of(b->ecart, b->necart, a), i = index_of(p->hand[me], p->nhand[me], a);
        float w = ecart_w();
        if (own) relayout(b, 1, a);
        slide(a, own, row_x(p->dogsize, j, w, ecart_shift()), T.ty, w, 0, own, own ? s1[i].x : ax, own ? s1[i].y : ay,
              own ? w1 : aw, own ? s1[i].deg : 0, 240, 0, own ? H_HAND : H_NONE, SND_PLACE);
        for (int e = j; e < p->necart; e++)
            slide(p->ecart[e], own, row_x(p->dogsize, e + 1, w, ecart_shift()), T.ty, w, 0, own,
                  row_x(p->dogsize, e, w, ecart_shift()), T.ty, w, 0, 200, 0, H_ECART, -1);
        break;
    }
    case TA_DONE: {
        float w = ecart_w();
        for (int e = 0; e < b->necart; e++) {
            int c = b->ecart[e], up = CARD_SUIT(c) == SUIT_TRUMP;
            slide(c, own, row_x(b->dogsize, e, w, ecart_shift()), T.ty, w, 0, up, T.ex, T.ey, T.ws, 0, SLIDE_MS, 50 * e,
                  H_ASIDE, e == 0 ? SND_MOVE : -1);
        }
        break;
    }
    case TA_PLAY: {
        int j = index_of(b->hand[me], b->nhand[me], a);
        float x, y;
        seat(b->turn, &x, &y);
        int delay = 0;
        if (b->ntrick == 0 && b->tricks > 0) { sweep(b, 0); delay = 120; }
        if (own) relayout(b, 1, a);
        slide(a, 1, own ? s0[j].x : ax, own ? s0[j].y : ay, own ? w0 : aw, own ? s0[j].deg : 0, 1, x, y, T.wt, 0,
              SLIDE_MS, delay, H_TRICK, SND_PLACE);
        if (p->ntrick == 0) {                   /* the trick complete: a moment to see who takes it */
            rest(delay + SLIDE_MS + TRICK_REST);
            ui_sound_later(SND_CAPTURE, delay + SLIDE_MS + 200);
        }
        if (p->phase == TP_SCORE || p->phase == TP_OVER) rest(delay + SLIDE_MS + 2 * TRICK_REST);
        break;
    }
    case TA_NEXT:
        pend_deal = 1;
        pend_dog = 0;
        break;
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
/* no shuffle between games either: a new game goes on from the cards of the last one, gathered as they lie (kept in
 * memory for the session); the session's first game, from an unseen warm-up deal */
static void start(int v)
{
    (void)v;
    uint8_t pack[78];
    if (G.players) tarot_gather(&G.pos, pack);
    else tarot_warmup(pack, ui.players, (uint32_t)ui_random(1 << 30));
    tarot_start(&G, ui.players, (uint32_t)ui_random(1 << 30), pack);
    calm();
    mark = 0;
    pend_deal = 1;
    pend_dog = 0;
    showlast = 0;
}

static int status(void)
{
    if (G.pos.phase != TP_OVER) return GS_PLAY;
    int w = tarot_winner(&G.pos);
    return w < 0 ? GS_DRAW : gs_win(w);
}

static unsigned winners(void)                   /* the highest total: one winner, or the players tied for it */
{
    const tpos *p = &G.pos;
    if (p->phase != TP_OVER) return 0;
    int top = -(1 << 20);
    unsigned m = 0;
    for (int q = 0; q < p->players; q++) top = p->total[q] > top ? p->total[q] : top;
    for (int q = 0; q < p->players; q++) m |= (unsigned)(p->total[q] == top) << q;
    return m;
}

static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    tpos before = G.pos;
    if (!tarot_play(&G, a)) { ui_sound(SND_DENY); return; }
    showlast = 0;
    calm();
    animate(&before, a);
    const tpos *p = &G.pos;
    if (p->turn != before.turn || TA_KIND(a) == TA_NEXT || p->phase == TP_SCORE || p->phase == TP_OVER) mark = G.n;
    if (p->phase != TP_OVER) ui_handoff(p->turn);   /* the device passes (after the cards) */
}

static int undo(void)
{
    if (G.n <= mark || !tarot_undo(&G)) return 0;
    calm();
    pend_deal = pend_dog = showlast = 0;
    ui_handoff(G.pos.turn);
    return 1;
}

/* the bot's side (bots.h): the rules' state to copy; one action in the notation, played as if tapped */
const tgame *tarot_live(void) { return &G; }

int tarot_show(const char *move)
{
    static tgame t;
    t = G;
    if (!tarot_script(&t, move) || t.n != G.n + 1) return 0;
    act(t.act[G.n]);
    return 1;
}

static int script(const char *s)
{
    int ok = tarot_script(&G, s);
    calm();
    mark = G.n;
    pend_deal = pend_dog = showlast = 0;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_disc, *t_play, *t_pill[2], *t_dot;
static int disc_d, pill_w, pill_h, dot_d;

static void put_tok(int q, float cx, float cy, float d)
{
    ui_put_c(ct_token(q, (int)fmaxf(4, 0.6f * T.u)), cx, cy, d, d);
}

/* the round gold button with the start icon: the discard done, the next deal */
static void round_button(float cx, float cy, float r, int glow)
{
    int d = (int)(2 * r);
    if (disc_d != d) {
        static const uint8_t gold[3] = {150, 118, 46};
        disc_d = d;
        ui_free(&t_disc);
        art_disc(d, gold, 1, 0, ui_buf(d, d));
        t_disc = ui_tex(d, d, ui_buf(d, d));
        int pd = (int)(1.2f * r);
        ui_free(&t_play);
        art_icon(ICON_PLAY, pd, ui_buf(pd, pd));
        t_play = ui_tex(pd, pd, ui_buf(pd, pd));
        SDL_SetTextureColorMod(t_play, 252, 244, 226);
    }
    if (glow) ui_mark_gold(cx, cy, 3.0f * r);
    SDL_SetTextureAlphaMod(t_disc, glow ? 255 : 110);
    SDL_SetTextureAlphaMod(t_play, glow ? 255 : 110);
    ui_put_c(t_disc, cx, cy, 2 * r, 2 * r);
    ui_put_c(t_play, cx + 0.06f * r, cy, 1.2f * r, 1.2f * r);
}

static void pill_bg(box b, int on, int alpha)
{
    if (pill_w != (int)b.w || pill_h != (int)b.h) {
        static const uint8_t off[4] = {40, 54, 48, 240}, gold[4] = {150, 118, 46, 255};
        pill_w = (int)b.w;
        pill_h = (int)b.h;
        for (int i = 0; i < 2; i++) {
            ui_free(&t_pill[i]);
            art_roundrect(pill_w, pill_h, pill_h / 2.0f, i ? gold : off, ui_buf(pill_w, pill_h));
            t_pill[i] = ui_tex(pill_w, pill_h, ui_buf(pill_w, pill_h));
        }
    }
    SDL_SetTextureAlphaMod(t_pill[on], (Uint8)alpha);
    ui_put(t_pill[on], b.x, b.y, b.w, b.h);
}

static void put_word(int bid, int shrt, float cx, float cy, float px, float maxw, int gold, int alpha)
{
    int w, h;
    SDL_Texture *t = word_tex(bid, shrt, px, maxw, gold, &w, &h);
    if (!t) return;
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);
    ui_put_c(t, cx, cy, (float)w, (float)h);
}

static float word_width(int bid, int shrt, float px, float maxw)
{
    int w, h;
    word_tex(bid, shrt, px, maxw, 0, &w, &h);
    return (float)w;
}

/* digits: the width ui_number draws them at height h (about) */
static float num_w(int v, float h)
{
    int d = 1;
    for (int a = abs(v); a >= 10; a /= 10) d++;
    return h * (0.74f * (d - 1) + 0.62f + (v < 0 ? 0.56f : 0));
}

/* a number of half points: the whole points, and a small 5 after a dot when there is a half */
static void half_number(float cx, float cy, float h, int half)
{
    int whole = half / 2, odd = half & 1;
    float w = num_w(whole, h) + (odd ? 0.62f * h : 0);
    float x = cx - w / 2 + num_w(whole, h) / 2;
    ui_number(x, cy, h, whole, 1);
    if (!odd) return;
    int d = (int)fmaxf(2, 0.16f * h);
    if (dot_d != d) {
        dot_d = d;
        ui_free(&t_dot);
        art_disc(d, CREAM, 1, 0, ui_buf(d, d));
        t_dot = ui_tex(d, d, ui_buf(d, d));
    }
    float xe = x + num_w(whole, h) / 2;
    ui_put_c(t_dot, xe + 0.12f * h, cy + 0.38f * h, (float)d, (float)d);
    ui_number(xe + 0.42f * h, cy - 0.12f * h, 0.62f * h, 5, 1);
}

/* the trick in the middle: the cards at their players' seats, a token at each seat's corner */
static void draw_trick(const uint8_t *cards, int n, int leader, int winner, int glow)
{
    const tpos *p = &G.pos;
    float d = 0.36f * T.u;
    for (int q = 0; q < p->players; q++) {
        float x, y;
        seat(q, &x, &y);
        int i = (q - leader + p->players) % p->players;
        if (i >= n) cards_place(DK, x, y, T.wt);
    }
    for (int i = 0; i < n; i++) {
        int q = (leader + i) % p->players, c = cards[i];
        float x, y;
        seat(q, &x, &y);
        if (hides(H_TRICK, c)) continue;
        cards_draw(DK, c, x, y, T.wt, 0, glow && q == winner ? CD_GLOW : 0);
    }
    for (int q = 0; q < p->players; q++) {
        float x, y;
        seat(q, &x, &y);
        put_tok(q, x + 0.5f * T.wt - 0.1f * d, y + 0.5f * AS * T.wt - 0.1f * d, d);
    }
}

static void draw_hand(void)
{
    const tpos *p = &G.pos;
    int me = T.me, n = p->nhand[me], ph = p->phase, mine = p->turn == me && !ui_handing(), b = busy();
    if (pend_deal) return;
    cards_spot s[TAROT_HAND];
    float w;
    hand_spots(n, s, &w);
    for (int i = 0; i < n; i++) {
        int c = p->hand[me][i], how = ph == TP_DOG || ui.viewer < 0 ? CD_DOWN : 0;   /* the cover: face down */
        if (hides(H_HAND, c)) continue;
        if (mine && !b && ph == TP_PLAY && !tarot_legal(p, TACT(TA_PLAY, c))) how |= CD_DIM;
        if (mine && !b && ph == TP_DISCARD && !tarot_legal(p, TACT(TA_DISCARD, c))) how |= CD_DIM;
        cards_draw(DK, c, s[i].x, s[i].y, w, s[i].deg, how);
    }
}

static void draw_band(void)
{
    const tpos *p = &G.pos;
    int ph = p->phase;
    /* the tricks played (tap: the last again); the cards put aside, the trumps among them face up */
    int nl = p->tricks - (hides(H_LAST, -1) > 0) - (p->ntrick == 0 && p->tricks > 0 && ph == TP_PLAY ? 1 : 0);
    if (ph == TP_SCORE || ph == TP_OVER) nl = p->tricks - 1;
    if (ph < TP_PLAY || ph == TP_VOID) nl = 0;
    cards_pile(DK, T.lx, T.ly, T.ws, nl > 0 ? nl * 2 : 0, CARD_NONE, 0);
    if (nl > 0) ui_number(T.lx + 0.5f * T.ws + 0.3f * T.u, T.ly, 0.34f * T.u, nl, TH.dark);
    int na = (ph == TP_PLAY || ph == TP_SCORE || ph == TP_OVER) ? p->necart - hides(H_ASIDE, -1) : 0;
    cards_pile(DK, T.ex, T.ey, T.ws, na, CARD_NONE, 0);
    if (na > 0) {
        int k = 0;
        for (int i = 0; i < p->necart; i++)
            if (CARD_SUIT(p->ecart[i]) == SUIT_TRUMP && p->aside_attack && p->contract <= TB_GARDE &&
                !hides(H_ASIDE, p->ecart[i]))
                cards_draw(DK, p->ecart[i], T.ex - 0.5f * T.ws - (k++) * 0.4f * T.ws, T.ey + 0.15f * T.ws, T.ws, 0, 0);
    }
    bitem it[8];
    int n = band_items(it);
    for (int i = 0; i < n; i++) {
        float x = it[i].x, w = it[i].w, cy = T.by + T.bh / 2;
        switch (it[i].kind) {
        case BI_DOG:
            if (pend_deal) break;
            for (int k = 0; k < p->dogsize; k++)
                if (!hides(H_DOG, p->dog[k])) cards_draw(DK, CARD_NONE, dog_band_x(k), T.ly, T.ws, 0, CD_DOWN);
            break;
        case BI_CALLED: {
            float cw = fminf(T.ws, w);
            cards_draw(DK, p->called, x + cw / 2, cy, cw, 0, p->revealed ? 0 : CD_GLOW);
            break;
        }
        case BI_PILL: {                         /* a poignée to show: a small trump and the trumps it takes */
            box b = {x, cy - 0.32f * T.bh, w, 0.64f * T.bh};
            pill_bg(b, 1, 255);
            float cw = 0.3f * T.bh;
            cards_draw(DK, CARD(SUIT_TRUMP, 21), b.x + 0.42f * b.h, cy, cw, 0, CD_FLAT);
            ui_number(b.x + b.w * 0.62f, cy, 0.5f * b.h, tarot_poignee_size(p->players, it[i].arg), 1);
            break;
        }
        case BI_SHOWN: {
            int q = it[i].arg, m = p->nshown[q];
            float k = w / shown_w(q), d = 0.42f * T.bh * k, cw = T.ws * 0.85f * k;
            put_tok(q, x + d / 2, cy, d);
            for (int j = 0; j < m; j++)
                cards_draw(DK, p->shown[q][j], x + 0.5f * T.bh * k + cw / 2 + j * 0.52f * cw, cy, cw, 0, CD_FLAT);
            break;
        }
        }
    }
}

static void draw_middle(void)
{
    const tpos *p = &G.pos;
    int ph = p->phase, mine = p->turn == T.me && !ui_handing(), b = busy();
    if (pend_deal) return;
    switch (ph) {
    case TP_BID:
        if (!mine || b) break;                  /* the buttons once the deal has landed */
        for (int bid = TB_PASS; bid <= TB_CONTRE; bid++) {
            box r = pill(bid);
            int ok = tarot_legal(p, TACT(TA_BID, bid));
            pill_bg(r, bid != TB_PASS && ok, ok ? 255 : 90);
            put_word(bid, 0, r.x + r.w / 2, r.y + r.h / 2, 0.42f * r.h, r.w * 0.86f, 0, ok ? 255 : 110);
        }
        break;
    case TP_CALL: {
        float w = row_w(4);
        int r = tarot_call_rank(p);
        for (int s = 0; s < 4; s++) cards_draw(DK, CARD(s, r), row_x(4, s, w, 0), T.ty, w, 0, mine ? CD_GLOW : 0);
        break;
    }
    case TP_DOG: {
        float w = row_w(p->dogsize);
        for (int i = 0; i < p->dogsize; i++)
            if (!hides(H_DOG, p->dog[i]) && !pend_dog)
                cards_draw(DK, p->dog[i], row_x(p->dogsize, i, w, 0), T.ty, w, 0, mine && !b ? CD_GLOW : 0);
        break;
    }
    case TP_DISCARD: {
        float w = ecart_w(), x, y, r;
        for (int i = 0; i < p->dogsize; i++) {
            float cx = row_x(p->dogsize, i, w, ecart_shift());
            if (i < p->necart && !hides(H_ECART, p->ecart[i])) {   /* face down but to the taker (a bot taking) */
                int up = ui.viewer >= 0 && T.me == p->taker;
                cards_draw(DK, up ? p->ecart[i] : CARD_NONE, cx, T.ty, w, 0, up ? 0 : CD_DOWN);
            }
            else cards_place(DK, cx, T.ty, w);
        }
        ok_button(&x, &y, &r);
        round_button(x, y, r, p->necart == p->dogsize && !b);
        break;
    }
    case TP_PLAY:
    case TP_SCORE:
    case TP_OVER:
        if (p->ntrick > 0) draw_trick(p->trick, p->ntrick, p->leader, -1, 0);
        else if (p->tricks > 0) draw_trick(p->last, p->players, p->lastlead, p->lastwin, !b || !hides(H_TRICK, -1));
        else draw_trick(p->trick, 0, p->leader, -1, 0);
        break;
    }
}

/* a veil over the table's band: black on the dark table, a warm brown, fainter, on the light one (no grey band) */
static void veil(int a)
{
    if (TH.dark) ui_fill(T.b.x, T.zy0, T.b.w, T.zy1 - T.zy0, 0, 0, 0, (Uint8)a);
    else ui_fill(T.b.x, T.zy0, T.b.w, T.zy1 - T.zy0, 110, 84, 50, (Uint8)(a / 2));
}

/* the last trick again: over a veil, each card at its player's seat, the winner's in gold light */
static void draw_last(void)
{
    const tpos *p = &G.pos;
    veil(120);
    draw_trick(p->last, p->players, p->lastlead, p->lastwin, 1);
}

static SDL_Texture *cross(float size)          /* a gold cross: "times" */
{
    static SDL_Texture *t;
    static int made;
    int s = (int)fmaxf(4, size);
    if (made != s) {
        made = s;
        ui_free(&t);
        art_icon(ICON_CLOSE, s, ui_buf(s, s));
        t = ui_tex(s, s, ui_buf(s, s));
        SDL_SetTextureColorMod(t, GOLD[0], GOLD[1], GOLD[2]);
    }
    return t;
}

/* the deal's count: the contract, the taker's side's oudlers and points against the points needed, the bonuses,
 * then each player's points and total (the highest in gold light), the button for the next deal */
static void draw_summary(void)
{
    const tpos *p = &G.pos;
    const tresult *r = &p->res;
    box b = T.b;
    float u = T.u;
    int bonus = r->pab || r->poignee || r->chelem, more = p->phase == TP_SCORE;
    int rows = 2 + bonus + p->players + more;
    float rh = fminf(1.0f * u, b.h * 0.92f / (rows + 0.4f)), pw = fminf(b.w * 0.94f, 8.2f * u);
    float ph = rh * (rows + 0.4f), x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    endt_begin(b, pw, ph, 70, &x0, &y0);   /* over the table, or in the end card */
    float y = y0 + 0.2f * rh + rh / 2, d = fminf(0.7f * rh, 0.75f * u), lx = x0 + 0.7f * u;
    /* the contract */
    if (r->made) ui_mark_gold(lx, y, 2.0f * d);
    put_tok(p->taker, lx, y, d);
    float tx = lx + 0.7f * u, ww = word_width(p->contract, 0, 0.42f * rh, pw * 0.5f);
    put_word(p->contract, 0, tx + ww / 2, y, 0.42f * rh, pw * 0.5f, 1, 255);
    if (p->called != TAROT_NONE) {
        float cw = 0.42f * rh, cx = tx + ww + 0.3f * u + cw / 2;
        cards_draw(DK, p->called, cx, y, cw, 0, CD_FLAT);
        if (p->partner != p->taker) put_tok(p->partner, cx + cw / 2 + 0.25f * u + d * 0.35f, y, d * 0.7f);
    }
    float nh = 0.42f * rh, mx = x0 + pw - 0.6f * u;   /* the bid's multiplier: a gold cross and its number */
    ui_put_c(cross(0.8f * nh), mx - 0.75f * nh, y, 0.8f * nh, 0.8f * nh);
    ui_number(mx, y, nh, r->mult, 1);
    y += rh;
    /* the three oudlers (those the taker's side has bright), the points, the points needed, the difference */
    float cw = 0.38f * rh, ox = x0 + 0.5f * u;
    static const int OUD[3] = {TAROT_PETIT, TAROT_21, TAROT_EXCUSE};
    for (int i = 0; i < 3; i++) {
        float cx = ox + cw / 2 + i * 1.08f * cw;
        cards_draw(DK, OUD[i], cx, y, cw, 0, CD_FLAT);
        if (!(r->oud >> i & 1)) ui_fill(cx - cw / 2, y - AS * cw / 2, cw, AS * cw, 28, 34, 32, 175);   /* not won */
    }
    half_number(x0 + pw * 0.44f, y, 0.5f * rh, r->half);
    ui_number(x0 + pw * 0.64f, y, 0.34f * rh, r->need, 1);
    ui_number_signed(x0 + pw * 0.84f, y, 0.46f * rh, r->made ? r->diff : -r->diff, 1, 1);
    y += rh;
    if (bonus) {                                /* petit au bout, the poignées, the chelem */
        float x = x0 + 0.5f * u;
        if (r->pab) {
            cards_draw(DK, TAROT_PETIT, x + cw / 2, y, cw, 0, CD_FLAT);
            ui_number_signed(x + cw + 0.65f * u, y, 0.4f * rh, r->pab * r->mult, 1, 1);
            x += pw * 0.32f;
        }
        if (r->poignee) {
            for (int j = 0; j < 3; j++)
                cards_draw(DK, CARD(SUIT_TRUMP, 18 + j), x + cw / 2 + j * 0.3f * cw, y, cw, 0, CD_FLAT);
            ui_number_signed(x + 1.6f * cw + 0.65f * u, y, 0.4f * rh, r->poignee, 1, 1);
            x += pw * 0.32f;
        }
        if (r->chelem) {
            for (int j = 0; j < 3; j++)
                cards_draw(DK, CARD_NONE, x + cw / 2 + j * 0.3f * cw, y, cw, 0, CD_DOWN | CD_FLAT);
            ui_number_signed(x + 1.6f * cw + 0.75f * u, y, 0.4f * rh, r->chelem, 1, 1);
        }
        y += rh;
    }
    ui_fill(x0 + 0.3f * u, y - rh / 2 + 0.04f * rh, pw - 0.6f * u, fmaxf(1, 0.03f * u), 246, 238, 220, 60);
    /* the players */
    int top = -(1 << 20);
    for (int q = 0; q < p->players; q++) top = p->total[q] > top ? p->total[q] : top;
    for (int k = 0; k < p->players; k++, y += rh) {
        int q = (p->dealer + 1 + k) % p->players;
        if (p->total[q] == top) ui_mark_gold(lx, y, 2.0f * d);
        put_tok(q, lx, y, d);
        ui_number_signed(x0 + pw * 0.45f, y, 0.42f * rh, p->score[q], 1, 1);
        ui_number_signed(x0 + pw * 0.78f, y, 0.52f * rh, p->total[q], 1, 0);
    }
    if (more) round_button(x0 + pw / 2, y + 0.05f * rh, 0.4f * rh, 1);
    endt_done();
}

/* a cancelled deal: the Petit sec shown (its holder's hand), or everyone passed; the button for the next deal */
static void draw_void(void)
{
    const tpos *p = &G.pos;
    float u = T.u;
    if (p->sec != TAROT_NONE) {
        int q = p->sec, n = p->nhand[q];
        cards_spot s[TAROT_HAND];
        float w = fminf(T.wt * 0.85f, T.b.w * 0.95f / (1 + 0.42f * (n - 1)));
        cards_fan(T.tx, T.ty - 0.2f * u, T.b.w * 0.95f, w, AS, n, 6, s);
        veil(60);
        put_tok(q, T.tx, T.ty - 0.2f * u - 0.5f * AS * w - 0.45f * u, 0.7f * u);
        for (int i = 0; i < n; i++)
            cards_draw(DK, p->hand[q][i], s[i].x, s[i].y, w, s[i].deg, p->hand[q][i] == TAROT_PETIT ? CD_GLOW : 0);
        round_button(T.tx, T.ty + 0.5f * AS * w + 0.6f * u, 0.5f * u, 1);
    } else {
        float d = 0.7f * u, gap = fminf(1.6f * u, T.b.w * 0.9f / p->players);
        for (int k = 0; k < p->players; k++) {
            int q = (p->dealer + 1 + k) % p->players;
            float x = T.tx + (k - (p->players - 1) / 2.0f) * gap;
            put_tok(q, x, T.ty - 0.5f * u, d);
            put_word(TB_PASS, 1, x, T.ty + 0.15f * u, 0.32f * u, gap * 0.95f, W_TABLE, 220);
        }
        round_button(T.tx, T.ty + 1.1f * u, 0.5f * u, 1);
    }
}

/* the animations waiting for the right view: the deal (the first bidder's), the dog turning up (the taker's) */
static void launch(void)
{
    const tpos *p = &G.pos;
    if (ui_handing() || T.b.w <= 0) return;
    if (pend_deal && T.me == p->turn) { pend_deal = 0; calm(); deal_slides(); }
    if (pend_dog && T.me == p->taker && p->phase == TP_DOG) { pend_dog = 0; dog_slides(); }
}

static void draw(void)
{
    const tpos *p = &G.pos;
    launch();
    int b = busy(), summary = (p->phase == TP_SCORE || p->phase == TP_OVER) && !b;
    draw_band();
    if (!summary) draw_middle();
    draw_hand();
    ct_draw(&M);
    if (showlast && p->tricks > 0) draw_last();
    if (summary) draw_summary();
    if (p->phase == TP_VOID && !b && !pend_deal) draw_void();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    const tpos *p = &G.pos;
    int ph = p->phase, me = T.me;
    if (showlast) { showlast = 0; return; }
    if (ph == TP_SCORE || ph == TP_VOID) { if (!busy()) act(TACT(TA_NEXT, 0)); return; }
    if (ph == TP_OVER) return;
    if (ph == TP_PLAY && p->tricks > 0 && cards_in(T.lx, T.ly, T.ws * 1.6f, AS, x, y)) {   /* the last trick */
        showlast = 1;
        ui_sound(SND_TAP);
        return;
    }
    if (p->turn != me) { ui_sound(SND_DENY); return; }
    int a = 0;
    switch (ph) {
    case TP_BID:
        if (busy()) break;                      /* the deal still landing */
        for (int bid = TB_PASS; bid <= TB_CONTRE; bid++)
            if (in_box(pill(bid), x, y)) a = TACT(TA_BID, bid);
        break;
    case TP_CALL: {
        float w = row_w(4);
        for (int s = 0; s < 4; s++)
            if (cards_in(row_x(4, s, w, 0), T.ty, w, AS, x, y)) a = TACT(TA_CALL, CARD(s, tarot_call_rank(p)));
        break;
    }
    case TP_DOG: {
        float w = row_w(p->dogsize);
        for (int i = 0; i < p->dogsize; i++)
            if (cards_in(row_x(p->dogsize, i, w, 0), T.ty, w * 1.1f, AS, x, y)) a = TACT(TA_TAKE, 0);
        break;
    }
    case TP_DISCARD: {
        float w = ecart_w(), bx, by, r;
        ok_button(&bx, &by, &r);
        if (hypotf(x - bx, y - by) <= r * 1.3f) { a = TACT(TA_DONE, 0); break; }
        for (int i = 0; i < p->necart; i++)
            if (cards_in(row_x(p->dogsize, i, w, ecart_shift()), T.ty, w, AS, x, y)) a = TACT(TA_RETURN, p->ecart[i]);
        if (a) break;
        int k = hand_hit(p->nhand[me], x, y);
        if (k >= 0) a = TACT(TA_DISCARD, p->hand[me][k]);
        break;
    }
    case TP_PLAY: {
        bitem it[8];
        int n = band_items(it);
        for (int i = 0; i < n; i++)
            if (it[i].kind == BI_PILL && x >= it[i].x && x <= it[i].x + it[i].w && fabsf(y - T.ly) < 0.4f * T.bh)
                a = TACT(TA_POIGNEE, it[i].arg);
        if (a) break;
        int k = hand_hit(p->nhand[me], x, y);
        if (k >= 0) a = TACT(TA_PLAY, p->hand[me][k]);
        break;
    }
    }
    if (a && tarot_legal(p, a)) act(a);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the bars: the total, the cards in hand, the bid (the called card: the taker's, the partner's once shown) ---- */
static void bar(int pl, box b)
{
    const tpos *p = &G.pos;
    float u = ui.unit, cy = b.y + b.h / 2;
    int narrow = b.w < 3.0f * u, ph = p->phase;
    int bid = -1, gold = 0;
    if (ph == TP_BID || (ph == TP_VOID && p->sec == TAROT_NONE)) bid = p->bid[pl] == TB_NONE ? -1 : p->bid[pl];
    else if (ph != TP_VOID && pl == p->taker) bid = p->contract, gold = 1;
    int called = p->called != TAROT_NONE && ph != TP_VOID && ph != TP_BID &&
                 (pl == p->taker || (pl == p->partner && (p->revealed || ph == TP_SCORE || ph == TP_OVER)));
    int count = ph == TP_SCORE || ph == TP_OVER || ph == TP_VOID ? -1 : p->nhand[pl];
    float sh = fminf(0.42f * u, narrow ? 0.36f * u : 0.42f * u);
    if (!narrow) {                              /* one row: the total, the cards, the bid */
        float x = b.x;
        float w = num_w(p->total[pl], sh);
        ui_number_signed(x + w / 2, cy, sh, p->total[pl], 1, 0);
        x += w + 0.35f * u;
        if (count >= 0) {
            cards_draw(DK, CARD_NONE, x + 0.15f * u, cy, 0.3f * u, 0, CD_DOWN | CD_FLAT);
            float cw = num_w(count, 0.34f * u);
            ui_number(x + 0.4f * u + cw / 2, cy, 0.34f * u, count, 1);
            x += 0.5f * u + cw + 0.3f * u;
        }
        if (called) {
            cards_draw(DK, p->called, x + 0.18f * u, cy, 0.36f * u, 0, CD_FLAT);
            x += 0.5f * u;
        }
        if (bid >= 0 && b.x + b.w - x > 0.5f * u) {
            float mw = b.x + b.w - x, ww = word_width(bid, 1, 0.36f * u, mw);
            put_word(bid, 1, x + ww / 2, cy, 0.36f * u, mw, gold | W_TABLE, bid == TB_PASS ? 170 : 255);
        }
        return;
    }
    /* narrow (five players side by side): the total and the cards on top, the bid under them */
    float y1 = b.y + 0.3f * b.h, y2 = b.y + 0.74f * b.h, w = num_w(p->total[pl], sh);
    if (w > b.w * 0.6f) sh *= b.w * 0.6f / w, w = num_w(p->total[pl], sh);
    ui_number_signed(b.x + w / 2, y1, sh, p->total[pl], 1, 0);
    if (count >= 0) {
        float ch = 0.3f * u, cw = num_w(count, ch), x = b.x + b.w - cw / 2 - 0.02f * u;
        ui_number(x, y1, ch, count, 1);
        cards_draw(DK, CARD_NONE, x - cw / 2 - 0.15f * u, y1, 0.22f * u, 0, CD_DOWN | CD_FLAT);
    }
    float x = b.x;
    if (called) {
        cards_draw(DK, p->called, x + 0.12f * u, y2, 0.22f * u, 0, CD_FLAT);
        x += 0.3f * u;
    }
    if (bid >= 0) {
        float mw = b.x + b.w - x, ww = word_width(bid, 1, 0.26f * u, mw);
        put_word(bid, 1, x + ww / 2, y2, 0.26f * u, mw, gold | W_TABLE, bid == TB_PASS ? 170 : 255);
    }
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the three oudlers, the Excuse, the 21 and the Petit, a little apart */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    static const int c[3] = {TAROT_EXCUSE, CARD(SUIT_TRUMP, 1), CARD(SUIT_TRUMP, 21)};
    int cw = (int)(size * 0.42f), ch = (int)(cw * AS);
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    memset(rgba, 0, (size_t)size * size * 4);
    for (int i = 0; i < 3; i++) {
        cards_paint(DK, c[i], cw, ch, b);
        float lift = i == 2 ? -0.07f : i == 0 ? 0.07f : 0;
        int x = (int)(size * 0.04f + i * size * 0.27f), y = (int)((size - ch) / 2 + lift * size);
        art_blend(rgba, size, size, b, cw, ch, x, y);
    }
    free(b);
}

static const game tarot_game = {
    .id = "tarot", .order = 270, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 3 | 1 << 4 | 1 << 5, .players_usual = 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(tarot_game)
