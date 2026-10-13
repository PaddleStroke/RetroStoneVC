/*
 * Crapette (Russian Bank) in RetroStone Board: the race patience for two, each with their own pack (the rules:
 * crapette_rules.c, crapette.h). Nothing is secret but the talons (VIEW_OPEN): the view turns each turn so that the
 * player to move sits at the bottom, the table turned round with them. The classic layout: the 8 foundations in two
 * columns in the middle, the 8 houses beside them (yours on the right, fanned outwards), your talon, the card you
 * turned, your waste and your crapette along the bottom (a wide screen: in a column on the right, the houses then as
 * tall as the height allows); the other's the same way round at the top (on the left). Player 0's pack has the kit's
 * green back, player 1's a red one (the kit's back tinted).
 * Tap a card (it rises, the places it may go show a dot) then tap where it goes, or drag it there; tap it again to
 * send it to a foundation. Tap your talon to turn a card, your waste to put the turned card on it (your turn ends).
 * A move that breaks a priority is allowed but marked: the card that should have gone first glows red, and the
 * other player's crapette button (a red cross in their bar) lights up; they tap it to call "Crapette!".
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/crapette.h"
#include "shell.h"

#define SLIDE_MS 260
#define FLIP_MS 300
#define DEAL_MS 380

static cgame G;
static ct_moves MV = {.rest = 300};
static int dealing;
enum { TAG_COL = 1, TAG_FOUND, TAG_CRAP, TAG_WASTE, TAG_HELD, TAG_TALON };
/* the UI's places beyond the rules' ones: the player to move's talon and waste */
enum { UP_TALON = CL_N, UP_WASTE };

/* ---- the table's geometry: the viewer (the player to move) at the bottom; the other's side turned round ---------- */
static struct {
    box b;
    int me, side;                               /* side: the piles in a column beside the houses (a wide screen) */
    float w, h, cx, cy;                         /* the cards' width and height, the table's centre */
    float ry[4], yb;                            /* the house rows (top to bottom), the viewer's pile row */
    float xr, edge;                             /* the right-hand houses' first card; how far they may fan */
    float fx[2];                                /* the foundations' two columns */
    float tx, hx, wx, kx;                       /* the viewer's talon, turned card, waste, crapette */
} T;

static struct { float x, y, r; } btn[2];         /* the crapette buttons in the bars (where bar() drew them) */

static void layout(box b)
{
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me > 1) T.me = 0;
    float asp = cards_aspect(DECK_FRENCH);
    /* upright: the piles in a row under the houses (and the other's over them); wide: in a column on the right (the
     * other's on the left), the houses as tall as the height allows: the bigger cards */
    float wu = floorf(fminf(b.h * 0.97f / (6.54f * asp), b.w / 7.0f));
    float ws = floorf(fminf(b.h * 0.97f / (4.18f * asp), b.w / 11.4f));
    T.side = b.w > 1.3f * b.h && ws > wu;
    T.w = T.side ? ws : wu;
    T.h = T.w * asp;
    T.cx = b.x + b.w / 2;
    T.cy = b.y + b.h / 2;
    float rs = 1.06f * T.h;
    for (int k = 0; k < 4; k++) T.ry[k] = T.cy + (k - 1.5f) * rs;
    T.yb = floorf(T.cy + 1.5f * rs + 1.18f * T.h);
    T.fx[0] = T.cx - 0.56f * T.w;
    T.fx[1] = T.cx + 0.56f * T.w;
    T.xr = T.cx + 1.72f * T.w;
    if (T.side) {
        T.kx = T.cx + fminf(b.w / 2 - 0.6f * T.w, 6.4f * T.w);
        T.edge = T.kx - 1.2f * T.w;
        return;
    }
    float left = T.cx - fminf(b.w, 7.4f * T.w) / 2, right = T.cx + fminf(b.w, 7.4f * T.w) / 2;
    T.edge = T.cx + fminf(b.w / 2, 4.8f * T.w) - 0.55f * T.w;   /* landscape: the houses may fan wider */
    T.tx = left + 0.6f * T.w;
    T.hx = T.tx + 1.12f * T.w;
    T.wx = T.hx + 1.12f * T.w;
    T.kx = right - 0.6f * T.w;
}

/* a point of the bottom player's side, seen by the viewer: as it is, or turned round the centre (the other's) */
static void turned(int owner, float *x, float *y)
{
    if (owner == T.me) return;
    *x = 2 * T.cx - *x;
    *y = 2 * T.cy - *y;
}

/* house k's card i (as if it held at least i + 1 cards): its centre; its owner's on their right, fanned outwards */
static void col_at(const cpos *p, int k, int i, float *x, float *y)
{
    int n = p->ncol[k] > i + 1 ? p->ncol[k] : i + 1;
    float step = n > 1 ? fminf(0.3f * T.w, (T.edge - T.xr) / (n - 1)) : 0;
    *x = T.xr + i * step;
    *y = T.ry[3 - k % 4];
    turned(k / 4, x, y);
}

static void found_at(int f, float *x, float *y)
{
    *x = T.fx[f / 4];
    *y = T.ry[f % 4];
    turned(0, x, y);                            /* the foundations as player 0 sees them */
}

enum { PILE_TALON, PILE_HELD, PILE_WASTE, PILE_CRAP };
static void pile_at(int owner, int which, float *x, float *y)
{
    if (T.side) {                               /* the column on the right: talon, turned card, waste, crapette */
        *x = T.kx;
        *y = T.ry[which];
    } else {
        *x = which == PILE_TALON ? T.tx : which == PILE_HELD ? T.hx : which == PILE_WASTE ? T.wx : T.kx;
        *y = T.yb;
    }
    turned(owner, x, y);
}

/* a place (the rules' places, seen by the player to move in p, and the UI's): where its top card lies */
static void place_at(const cpos *p, int pl, float *x, float *y)
{
    int me = p->turn, op = 1 - me;
    if (pl < CL_FOUND) col_at(p, pl, p->ncol[pl] ? p->ncol[pl] - 1 : 0, x, y);
    else if (pl < CL_CRAP) found_at(pl - CL_FOUND, x, y);
    else if (pl == CL_CRAP) pile_at(me, PILE_CRAP, x, y);
    else if (pl == CL_HELD) pile_at(me, PILE_HELD, x, y);
    else if (pl == CL_OCRAP) pile_at(op, PILE_CRAP, x, y);
    else if (pl == CL_OWASTE) pile_at(op, PILE_WASTE, x, y);
    else if (pl == UP_TALON) pile_at(me, PILE_TALON, x, y);
    else pile_at(me, PILE_WASTE, x, y);
}

/* ---- player 1's back: the kit's back, its green felt tinted a deep lacquer red ----------------------------------- */
static void tint_back(uint8_t *px, int n)
{
    for (int i = 0; i < n; i++) {
        uint8_t *c = px + 4 * i;
        int r = c[0], g = c[1], b = c[2];
        if (g > r + 8 && g >= b) {
            c[0] = (uint8_t)fminf(255, 1.25f * g + 0.3f * r);
            c[1] = (uint8_t)(0.35f * r + 0.1f * g);
            c[2] = (uint8_t)(0.45f * b);
        }
    }
}

/* each player's pack for the card kit: player 0's the kit's French pack, player 1's the same with the red back */
static void face_of(int card, int w, int h, uint8_t *rgba) { cards_paint(DECK_FRENCH, card, w, h, rgba); }

static void red_back(int w, int h, uint8_t *rgba)
{
    cards_paint(DECK_FRENCH, CARD_NONE, w, h, rgba);
    tint_back(rgba, w * h);
}

static int deck_of(int owner)
{
    static const cards_deck_def red = {1.4f, face_of, red_back, NULL};
    static int deck = -1;
    if (owner != 1) return DECK_FRENCH;
    if (deck < 0) deck = cards_deck_add(&red);
    return deck;
}

/* a back of `owner`'s pack */
static void back_draw(int owner, float cx, float cy, float w, int how)
{
    cards_draw(deck_of(owner), CARD_NONE, cx, cy, w, 0, how | CD_DOWN);
}

/* a pile of n backs, its thickness showing */
static void backs_pile(int owner, float cx, float cy, float w, int n, int how)
{
    if (n > 0) cards_pile(deck_of(owner), cx, cy, w, n, CARD_NONE, how);
}

/* ---- cards turning over on their way (the talon's, the deal's): their own pack's back, a lower hop, no rest ------ */
#define FLIP_TAG 16                             /* their tags: the table's (TAG_*) plus this */

static void flip_start(int owner, int card, int up0, float x0, float y0, int up1, float x1, float y1, int ms,
                       int delay, int tag, int a, int b, int sound)
{
    int i = ct_slide(&MV, deck_of(owner), card, x0, y0, T.w, 0, up0, x1, y1, T.w, 0, up1, ms, delay, FLIP_TAG + tag, a,
                     b, sound);
    ct_rest(&MV, i, 0);
    ct_hop(&MV, i, 0.15f, 0.1f);
}

static int flips_hide(int tag, int a) { return ct_hides(&MV, FLIP_TAG + tag, a, -1); }
static int flips_hide_card(int tag, int a, int b) { return ct_hides(&MV, FLIP_TAG + tag, a, b) > 0; }
static int busy(void) { return ct_busy(&MV); }
static void calm(void) { ct_calm(&MV); }

/* the deal: each house card flies from its owner's talon, turning over; then the crapettes' top cards turn */
static void deal_slides(void)
{
    const cpos *p = &G.pos;
    for (int k = 0; k < CR_COLS; k++) {
        float x0, y0, x1, y1;
        pile_at(k / 4, PILE_TALON, &x0, &y0);
        col_at(p, k, 0, &x1, &y1);
        flip_start(k / 4, p->col[k][0], 0, x0, y0, 1, x1, y1, DEAL_MS, 110 * (k % 4) + 50 * (k / 4), TAG_COL, k, 0,
                   k % 2 ? -1 : SND_MOVE);
    }
    for (int q = 0; q < 2; q++) {
        float x, y;
        pile_at(q, PILE_CRAP, &x, &y);
        flip_start(q, p->crap[q][p->ncrap[q] - 1], 0, x, y, 1, x, y, FLIP_MS, 560, TAG_CRAP, q, 0, SND_PLACE);
    }
}

/* ---- the moves --------------------------------------------------------------------------------------------------- */
static struct { int on, place, moved, was; float x0, y0, x, y; } drag;
static int sel = -1;                            /* the place of the card raised (-1: none) */

/* what an action moved, from the position before it (b) to the one after (G.pos) */
static void animate(const cpos *b, int a)
{
    const cpos *p = &G.pos;
    int me = b->turn, op = 1 - me;
    float x0, y0, x1, y1;
    if (!busy()) ct_calm(&MV);
    switch (CA_KIND(a)) {
    case CA_MOVE: {
        int s = CA_SRC(a), d = CA_DST(a), c = crap_card_at(b, s), tag, ta, tb = 0, snd = SND_PLACE;
        place_at(b, s, &x0, &y0);
        if (drag.on && drag.moved) x0 = drag.x, y0 = drag.y;
        place_at(p, d, &x1, &y1);
        if (d < CL_FOUND) tag = TAG_COL, ta = d, tb = p->ncol[d] - 1;
        else if (d < CL_CRAP) tag = TAG_FOUND, ta = d - CL_FOUND;
        else tag = d == CL_OCRAP ? TAG_CRAP : TAG_WASTE, ta = op, snd = SND_CAPTURE;
        ct_slide(&MV, DECK_FRENCH, c, x0, y0, T.w, 0, 1, x1, y1, T.w, 0, 1, SLIDE_MS, 0, tag, ta, tb, snd);
        if (s == CL_CRAP && p->ncrap[me]) {     /* the crapette's next card turns up */
            place_at(b, s, &x0, &y0);
            flip_start(me, p->crap[me][p->ncrap[me] - 1], 0, x0, y0, 1, x0, y0, FLIP_MS, SLIDE_MS / 2, TAG_CRAP, me,
                       0, -1);
        }
        break;
    }
    case CA_TURN: {
        int delay = 0;
        pile_at(me, PILE_TALON, &x1, &y1);
        if (!b->ntalon[me]) {                   /* the waste turned over into the talon */
            pile_at(me, PILE_WASTE, &x0, &y0);
            flip_start(me, b->waste[me][b->nwaste[me] - 1], 1, x0, y0, 0, x1, y1, SLIDE_MS + 60, 0, TAG_TALON, me, 0,
                       SND_MOVE);
            delay = SLIDE_MS + 80;
        }
        pile_at(me, PILE_HELD, &x0, &y0);
        flip_start(me, p->held, 0, x1, y1, 1, x0, y0, FLIP_MS, delay, TAG_HELD, me, 0, SND_PLACE);
        break;
    }
    case CA_DISCARD:
        pile_at(me, PILE_HELD, &x0, &y0);
        if (drag.on && drag.moved) x0 = drag.x, y0 = drag.y;
        pile_at(me, PILE_WASTE, &x1, &y1);
        ct_slide(&MV, DECK_FRENCH, b->held, x0, y0, T.w, 0, 1, x1, y1, T.w, 0, 1, SLIDE_MS, 0, TAG_WASTE, me, 0,
                 SND_PLACE);
        break;
    case CA_PASS:
        ui_sound(SND_TAP);
        break;
    case CA_CALL:                               /* "Crapette!": the table back as it was before the fault */
        calm();
        ui_sound(SND_CHECK);
        break;
    }
}

static void act(int a)
{
    cpos before = G.pos;
    if (!crap_play(&G, a)) { ui_sound(SND_DENY); return; }
    animate(&before, a);
    sel = -1;
    drag.on = 0;
    ui_handoff(G.pos.turn);                     /* the view turns once the cards have landed */
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    (void)v;
    crap_new(&G, (uint32_t)ui_random(1 << 30));
    calm();
    dealing = 1;
    sel = -1;
    drag.on = 0;
}

static int status(void)
{
    const cpos *p = &G.pos;
    if (p->phase != CP_OVER) return GS_PLAY;
    return p->winner == CR_DRAW ? GS_DRAW : gs_win(p->winner);
}

static int turn(void) { return G.pos.turn; }

/* a fault marked is not taken back by undo: it is the other player's to call */
static int undo(void)
{
    if (sel >= 0) { sel = -1; return 1; }
    if (G.pos.fault != CF_NONE || !crap_undo(&G)) return 0;
    calm();
    dealing = 0;
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = crap_script(&G, s);
    calm();
    dealing = 0;
    sel = -1;
    ui_handoff(G.pos.turn);
    return ok;
}

static int can_pass(void) { return !busy() && crap_legal(&G.pos, CA_PASS); }
static void pass(void) { act(CA_PASS); }

/* the bot (crapette_bot.c): the game read-only, and one action of the notation played as a tap plays it */
const cgame *crapette_live(void) { return &G; }

int crapette_show(const char *move)
{
    static cgame t;
    t = G;
    if (!move[0] || strchr(".#", move[0]) || !crap_script(&t, move) || t.n != G.n + 1) return 0;
    act(t.act[G.n]);
    return 1;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static int fits_sel(int place)                  /* the raised card may go there */
{
    const cpos *p = &G.pos;
    if (sel < 0) return 0;
    if (place == UP_WASTE) return sel == CL_HELD;
    return place < CL_N && crap_legal(p, CA_MOVEOF(sel, place));
}

static void dot(float x, float y) { ui_mark_dot(x, y, 0.42f * T.w); }

/* where the fault's passed-over card lies now (red glow); 0: not on the table any more */
static int fault_spot(float *x, float *y)
{
    const cpos *p = &G.pos;
    if (p->fault == CF_NONE) return 0;
    static const int places[] = {CL_CRAP, CL_HELD, 0, 1, 2, 3, 4, 5, 6, 7};
    for (unsigned i = 0; i < sizeof places / sizeof places[0]; i++)
        if (crap_card_at(p, places[i]) == p->fault_card) { place_at(p, places[i], x, y); return 1; }
    return 0;
}

static void draw_found(void)
{
    const cpos *p = &G.pos;
    for (int f = 0; f < CR_FOUND; f++) {
        float x, y;
        found_at(f, &x, &y);
        int c = p->found[f];
        if (c != CARD_NONE && ct_hides(&MV, TAG_FOUND, f, -1))   /* the card under the one arriving */
            c = CARD_RANK(c) == RANK_ACE ? CARD_NONE : CARD(CARD_SUIT(c), CARD_RANK(c) - 1);
        if (c == CARD_NONE) cards_place(DECK_FRENCH, x, y, T.w);
        else cards_draw(DECK_FRENCH, c, x, y, T.w, 0, CARD_RANK(c) == RANK_KING ? CD_DIM : 0);
        if (fits_sel(CL_FOUND + f)) dot(x, y);
    }
}

static void draw_cols(int live)
{
    const cpos *p = &G.pos;
    for (int k = 0; k < CR_COLS; k++) {
        int n = p->ncol[k];
        float x, y;
        col_at(p, k, 0, &x, &y);
        if (!n) cards_place(DECK_FRENCH, x, y, T.w);
        for (int i = 0; i < n; i++) {
            if (ct_hides(&MV, TAG_COL, k, i) || flips_hide_card(TAG_COL, k, i)) continue;
            if (i == n - 1 && drag.on && drag.moved && drag.place == k) continue;
            col_at(p, k, i, &x, &y);
            int how = i == n - 1 && sel == k && live ? CD_RAISED : 0;
            cards_draw(DECK_FRENCH, p->col[k][i], x, y, T.w, 0, how);
        }
        if (fits_sel(k)) {
            col_at(p, k, n, &x, &y);
            dot(x, y);
        }
    }
}

/* a player's talon, waste, crapette and turned card */
static void draw_piles(int q, int live)
{
    const cpos *p = &G.pos;
    int mine = q == p->turn && live;
    float x, y;
    /* the talon */
    pile_at(q, PILE_TALON, &x, &y);
    int nt = flips_hide(TAG_TALON, q) ? 0 : p->ntalon[q];
    if (!nt) cards_place(DECK_FRENCH, x, y, T.w);
    else backs_pile(q, x, y, T.w, nt, mine && sel < 0 && crap_legal(p, CA_TURN) ? CD_GLOW : 0);
    /* the waste */
    pile_at(q, PILE_WASTE, &x, &y);
    int nw = p->nwaste[q] - ct_hides(&MV, TAG_WASTE, q, -1);
    if (nw > 0) cards_pile(DECK_FRENCH, x, y, T.w, nw, p->waste[q][nw - 1], 0);
    else cards_place(DECK_FRENCH, x, y, T.w);
    if ((q == p->turn && fits_sel(UP_WASTE)) || (q != p->turn && fits_sel(CL_OWASTE))) dot(x, y);
    /* the crapette: backs under its top card (a card turning up, or one loaded on its way: the one under shows) */
    pile_at(q, PILE_CRAP, &x, &y);
    int nc = p->ncrap[q] - ct_hides(&MV, TAG_CRAP, q, -1), turning = flips_hide(TAG_CRAP, q);
    if (nc <= 0) cards_place(DECK_FRENCH, x, y, T.w);
    else {
        backs_pile(q, x, y, T.w, nc, 0);
        if (!turning && !(drag.on && drag.moved && drag.place == CL_CRAP && q == p->turn)) {
            int how = q == p->turn && sel == CL_CRAP && live ? CD_RAISED : 0;
            cards_draw(DECK_FRENCH, p->crap[q][nc - 1], x, y, T.w, 0, how | CD_FLAT);
        }
    }
    if (q != p->turn && fits_sel(CL_OCRAP)) dot(x, y);
    /* the card turned from the talon */
    if (q == p->turn && p->held != CARD_NONE && !flips_hide(TAG_HELD, q) &&
        !(drag.on && drag.moved && drag.place == CL_HELD)) {
        pile_at(q, PILE_HELD, &x, &y);
        cards_draw(DECK_FRENCH, p->held, x, y, T.w, 0, sel == CL_HELD && live ? CD_RAISED : 0);
    }
}

static void draw(void)
{
    const cpos *p = &G.pos;
    if (dealing && !ui_handing()) { dealing = 0; deal_slides(); }
    int live = p->phase == CP_PLAY && !ui_handing();
    float fx, fy;
    if (fault_spot(&fx, &fy)) ui_mark_glow(fx, fy, 1.9f * T.w);   /* the card that should have gone first */
    draw_found();
    draw_cols(live);
    for (int q = 0; q < 2; q++) draw_piles(q, live);
    ct_draw(&MV);
    if (drag.on && drag.moved) {
        int c = crap_card_at(p, drag.place);
        if (c != CARD_NONE) cards_draw(DECK_FRENCH, c, drag.x, drag.y, T.w * 1.04f, 0, CD_RAISED);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int on(float cx, float cy, float x, float y)
{
    return cards_in(cx, cy, T.w * 1.06f, cards_aspect(DECK_FRENCH), x, y);
}

/* a card of the player to move's under the finger: their crapette, their turned card, a house's top card */
static int source_at(float x, float y)
{
    const cpos *p = &G.pos;
    float cx, cy;
    if (p->held != CARD_NONE) {
        place_at(p, CL_HELD, &cx, &cy);
        if (on(cx, cy, x, y)) return CL_HELD;
    }
    if (p->ncrap[p->turn]) {
        place_at(p, CL_CRAP, &cx, &cy);
        if (on(cx, cy, x, y)) return CL_CRAP;
    }
    for (int k = 0; k < CR_COLS; k++) {
        if (!p->ncol[k]) continue;
        place_at(p, k, &cx, &cy);
        if (on(cx, cy, x, y)) return k;
    }
    return -1;
}

/* a place under the finger to put a card on (or to tap: the talon, the waste) */
static int target_at(float x, float y)
{
    const cpos *p = &G.pos;
    float cx, cy, x0, y0;
    for (int f = 0; f < CR_FOUND; f++) {
        found_at(f, &cx, &cy);
        if (on(cx, cy, x, y)) return CL_FOUND + f;
    }
    for (int k = 0; k < CR_COLS; k++) {         /* a house: its whole row, from its first card to its top one */
        col_at(p, k, 0, &x0, &y0);
        col_at(p, k, p->ncol[k] ? p->ncol[k] - 1 : 0, &cx, &cy);
        float lo = fminf(x0, cx) - 0.53f * T.w, hi = fmaxf(x0, cx) + 0.53f * T.w;
        if (x >= lo && x <= hi && fabsf(y - cy) <= 0.53f * T.h) return k;
    }
    static const int piles[] = {CL_OCRAP, CL_OWASTE, UP_TALON, UP_WASTE};
    for (int i = 0; i < 4; i++) {
        place_at(p, piles[i], &cx, &cy);
        if (on(cx, cy, x, y)) return piles[i];
    }
    return -1;
}

/* the raised card put on a place; 0: it may not go there */
static int put_on(int from, int place)
{
    const cpos *p = &G.pos;
    if (place == UP_WASTE && from == CL_HELD) { act(CA_DISCARD); return 1; }
    if (place >= 0 && place < CL_N && place != from && crap_legal(p, CA_MOVEOF(from, place))) {
        act(CA_MOVEOF(from, place));
        return 1;
    }
    return 0;
}

static void press(float x, float y)
{
    const cpos *p = &G.pos;
    drag.on = 0;
    if (p->phase != CP_PLAY) return;
    int s = source_at(x, y), d = target_at(x, y);
    if (sel >= 0 && d != sel && put_on(sel, d)) return;
    if (s >= 0) {                               /* raise it (and maybe drag it) */
        drag.on = 1, drag.place = s, drag.moved = 0, drag.was = s == sel;
        drag.x0 = drag.x = x, drag.y0 = drag.y = y;
        if (s != sel) ui_sound(SND_TAP);
        sel = s;
        return;
    }
    if (d == UP_TALON) {
        if (crap_legal(p, CA_TURN)) act(CA_TURN);
        else ui_sound(SND_DENY);
        return;
    }
    if (sel >= 0) { sel = -1; return; }
    ui_sound(SND_DENY);
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (!drag.moved && hypotf(x - drag.x0, y - drag.y0) > 0.15f * T.w) drag.moved = 1;
    drag.x = x, drag.y = y;
    if (drag.moved) ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    const cpos *p = &G.pos;
    drag.x = x, drag.y = y;
    int from = drag.place;
    if (drag.moved) {                           /* dropped: where it was let go, or back */
        if (!put_on(from, target_at(x, y))) { ui_sound(SND_DENY); sel = -1; }
    } else if (drag.was) {                      /* tapped again: to a foundation, or put down */
        int f = crap_found_for(p, crap_card_at(p, from));
        if (f >= 0) act(CA_MOVEOF(from, f));
        else sel = -1;
    }
    drag.on = 0;
}

/* ---- the bars ---------------------------------------------------------------------------------------------------- */
static float num_w(int v, float h)              /* the width of ui_number(v) at height h */
{
    int digits = 1;
    for (; v >= 10; v /= 10) digits++;
    return (0.74f * (digits - 1) + 0.62f) * h;
}

/* the waiting player's crapette button: a red cross in a dark disc, lit while a fault may be called */
static void call_button(int player, float bx, float cy, float d, int lit)
{
    static SDL_Texture *disc, *cross;
    static int made;
    if (made != (int)d) {
        made = (int)d;
        static const uint8_t c[3] = {40, 22, 20};
        ui_free(&disc);
        art_disc(made, c, 1, 0, ui_buf(made, made));
        disc = ui_tex(made, made, ui_buf(made, made));
        ui_free(&cross);
        art_icon(ICON_CLOSE, made, ui_buf(made, made));
        cross = ui_tex(made, made, ui_buf(made, made));
    }
    if (!disc || !cross) return;
    if (lit) ui_mark_glow(bx, cy, 1.8f * d);
    SDL_SetTextureAlphaMod(disc, lit ? 255 : 120);
    SDL_SetTextureColorMod(cross, 236, 70, 56);
    SDL_SetTextureAlphaMod(cross, lit ? 255 : 90);
    ui_put_c(disc, bx, cy, d, d);
    ui_put_c(cross, bx, cy, 0.62f * d, 0.62f * d);
    btn[player].x = bx, btn[player].y = cy, btn[player].r = 0.6f * d;
}

/* a player's bar: their crapette's top card and how many cards it holds; a back of their pack and their talon and
 * waste together; the waiting player's crapette button at the right */
static void bar(int player, box b)
{
    const cpos *p = &G.pos;
    float u = ui.unit, cy = b.y + b.h / 2, right = b.x + b.w;
    btn[player].r = 0;
    if (p->phase == CP_PLAY && player != p->turn) {
        float d = fminf(0.84f * u, 0.92f * b.h);
        call_button(player, right - d / 2 - 0.05f * u, cy, d, p->fault != CF_NONE);
        right -= d + 0.2f * u;
    }
    int nc = p->ncrap[player], rest = p->ntalon[player] + p->nwaste[player];
    float cw = 0.36f * u, nh = fminf(0.4f * u, 0.45f * b.h), gap = 0.1f * u, sep = 0.3f * u;
    float need = 2 * cw + 2 * gap + sep + num_w(nc, nh) + num_w(rest, 0.8f * nh), k = fminf(1, (right - b.x) / need);
    cw *= k, nh *= k, gap *= k, sep *= k;
    float x = b.x;
    int top = nc ? p->crap[player][nc - 1] : CARD_NONE;
    if (top != CARD_NONE) cards_draw(DECK_FRENCH, top, x + cw / 2, cy, cw, 0, CD_FLAT);
    else cards_place(DECK_FRENCH, x + cw / 2, cy, cw);
    x += cw + gap;
    ui_number(x + num_w(nc, nh) / 2, cy, nh, nc, 1);
    x += num_w(nc, nh) + sep;
    back_draw(player, x + cw / 2, cy, cw, CD_FLAT);
    x += cw + gap;
    ui_number(x + num_w(rest, 0.8f * nh) / 2, cy, 0.8f * nh, rest, 1);
}

/* a press in a bar: the waiting player's crapette button */
static int bar_press(int player, float x, float y)
{
    const cpos *p = &G.pos;
    if (p->phase != CP_PLAY || player == p->turn || btn[player].r <= 0) return 0;
    if (hypotf(x - btn[player].x, y - btn[player].y) > btn[player].r) return 0;
    if (crap_legal(p, CA_CALL)) act(CA_CALL);
    else ui_sound(SND_DENY);
    return 1;
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: the two packs' backs, green and red, and a crapette's run of hearts going up */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    float asp = cards_aspect(DECK_FRENCH), w = size * 0.42f;
    int cw = (int)w, ch = (int)(w * asp);
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    memset(rgba, 0, (size_t)size * size * 4);
    cards_paint(DECK_FRENCH, CARD_NONE, cw, ch, b);
    art_blend(rgba, size, size, b, cw, ch, (int)(size * 0.04f), (int)(size * 0.03f));
    cards_paint(deck_of(1), CARD_NONE, cw, ch, b);
    art_blend(rgba, size, size, b, cw, ch, size - cw - (int)(size * 0.04f), (int)(size * 0.03f));
    static const uint8_t run[3] = {CARD(SUIT_HEARTS, 6), CARD(SUIT_SPADES, 5), CARD(SUIT_HEARTS, 4)};
    for (int i = 0; i < 3; i++) {
        cards_paint(DECK_FRENCH, run[i], cw, ch, b);
        art_blend(rgba, size, size, b, cw, ch, (int)(size * (0.14f + 0.15f * i)), size - ch - (int)(size * 0.03f));
    }
    free(b);
}

static const game crapette_game = {
    .id = "crapette", .order = 304, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .bar_press = bar_press,
    .view = VIEW_OPEN,
};
REGISTER_GAME(crapette_game)
