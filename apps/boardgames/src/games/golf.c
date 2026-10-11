/*
 * Golf in RetroStone Board: the card game for 2 to 6 players passing the device round (the rules: golf_rules.c,
 * golf.h). The view is the player to move's (VIEW_OPEN: nothing is secret, the device just passes): their grid big at
 * the bottom of the table, the stock, the drawn card and the discard in the middle, the others' grids smaller at the
 * top (in turn order, each under its token). Taps only: the stock or the discard to draw, a card of your grid to
 * swap, the discard to throw the drawn card away, then a face-down card to turn up. Cards slide and flip (the card
 * kit, cards.h); after a round, a summary of the scores (no words: tokens and numbers).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/golf.h"
#include "shell.h"

#define SLIDE_MS 320
#define FLIP_MS 260
#define DEAL_MS 240
#define REST_MS 320                             /* after the last card lands: a moment before the device passes */

static ggame G;
static int variant, dealing;                     /* dealing: the deal flies at the next frame (the view set) */

/* ---- the table's geometry (it follows the viewer) ---------------------------------------------------------------- */
static struct {
    box b;
    int me;
    float wg, gx, gy;                           /* the viewer's grid: card width, top left */
    float wo, ox[GOLF_MAXP], oy[GOLF_MAXP];     /* the others' grids: card width, each one's top left */
    float tok;                                  /* the others' tokens over their grids */
    float wp, sx, sy, dx, dy, hx, hy;           /* the stock, the discard, the drawn card: width, centres */
} T;

static float grid_w(float w, int cols) { return w * (1.1f * cols - 0.1f); }
static float grid_h(float w, int rows) { return w * (1.5f * rows - 0.1f); }

static void place_others(float x, float y, float w, float h)
{
    const gpos *p = &G.pos;
    int k = p->players - 1, rows = 1;
    float best = 0;
    for (int r = 1; r <= 2 && r <= k; r++) {    /* one row or two: the bigger cards */
        int per = (k + r - 1) / r;
        float cell_h = h / r, tok = fminf(0.5f * ui.unit, 0.2f * cell_h);
        float wo = fminf(w / per * 0.9f / (1.1f * p->cols - 0.1f),
                         (cell_h - tok * 1.25f) * 0.94f / (1.5f * p->rows - 0.1f));
        if (wo > best * 1.08f) best = wo, rows = r;
    }
    T.wo = fminf(best, 0.72f * T.wg);
    int per = (k + rows - 1) / rows;
    float cell_h = h / rows;
    T.tok = fminf(0.5f * ui.unit, 0.2f * cell_h);
    for (int i = 0; i < k; i++) {
        int q = (T.me + 1 + i) % p->players, r = i / per, c = i % per, in_row = r < rows - 1 ? per : k - r * per;
        float cw = w / per, gw = grid_w(T.wo, p->cols), gh = grid_h(T.wo, p->rows);
        float cx = x + w / 2 + (c - (in_row - 1) / 2.0f) * cw;
        float top = y + r * cell_h + (cell_h - gh - T.tok * 1.25f) / 2 + T.tok * 1.25f;
        T.ox[q] = floorf(cx - gw / 2);
        T.oy[q] = floorf(top);
    }
}

static void layout(box b)
{
    const gpos *p = &G.pos;
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= p->players) T.me = 0;
    int C = p->cols, R = p->rows;
    if (b.w <= b.h * 1.05f) {                   /* portrait: the others, the piles, the grid */
        float ho = b.h * (p->players == 2 ? 0.3f : p->players == 3 ? 0.35f : 0.4f),
              hm = b.h * 0.19f, hg = b.h - ho - hm;
        T.wg = floorf(fminf(b.w * 0.96f / (1.1f * C - 0.1f), hg * 0.95f / (1.5f * R - 0.1f)));
        T.gx = floorf(b.x + (b.w - grid_w(T.wg, C)) / 2);
        T.gy = floorf(b.y + b.h - grid_h(T.wg, R) - (hg - grid_h(T.wg, R)) * 0.35f);
        T.wp = floorf(fminf(fminf(T.wg * 0.95f, hm * 0.84f / 1.4f), b.w / 4.4f));
        float cy = b.y + ho + hm / 2, cx = b.x + b.w / 2;
        T.sx = cx - 1.3f * T.wp, T.sy = cy;
        T.hx = cx, T.hy = cy;
        T.dx = cx + 1.3f * T.wp, T.dy = cy;
        place_others(b.x, b.y, b.w, ho);
    } else {                                    /* landscape: the others above; the grid, and the piles on its right */
        float ho = b.h * 0.36f, hb = b.h - ho, gwmax = b.w * 0.6f;
        T.wg = floorf(fminf(gwmax * 0.95f / (1.1f * C - 0.1f), hb * 0.94f / (1.5f * R - 0.1f)));
        T.gx = floorf(b.x + (gwmax - grid_w(T.wg, C)) / 2);
        T.gy = floorf(b.y + ho + (hb - grid_h(T.wg, R)) / 2);
        float px = b.x + gwmax, pw = b.w - gwmax, pcx = px + pw / 2, pcy = b.y + ho + hb / 2;
        T.wp = floorf(fminf(fminf(T.wg, pw / 2.8f), hb / (2 * 1.4f + 0.5f)));
        float hp = 1.4f * T.wp;
        T.sx = pcx - 0.62f * T.wp, T.sy = pcy + 0.62f * hp;
        T.dx = pcx + 0.62f * T.wp, T.dy = T.sy;
        T.hx = pcx, T.hy = pcy - 0.55f * hp;
        place_others(b.x, b.y, b.w, ho);
    }
}

/* a slot's centre and its card's width (the viewer's grid, or another player's) */
static void slot_at(int pl, int s, float *x, float *y, float *w)
{
    const gpos *p = &G.pos;
    int r = s / p->cols, c = s % p->cols;
    float cw = pl == T.me ? T.wg : T.wo, gx = pl == T.me ? T.gx : T.ox[pl], gy = pl == T.me ? T.gy : T.oy[pl];
    *w = cw;
    *x = gx + c * 1.1f * cw + cw / 2;
    *y = gy + r * 1.5f * cw + 0.7f * cw;
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
enum { H_NONE, H_SLOT, H_PILE, H_HELD };
#define MOVES 96
static struct { cards_slide s; int hide, pl, slot; } mv[MOVES];
static int nmv;
static Uint32 quiet;                            /* busy until then (the last card landed, and a moment) */

static void calm(void)
{
    nmv = 0;
    quiet = 0;
}

static void slide(int card, int up0, float x0, float y0, float w0, int up1, float x1, float y1, float w1, int ms,
                  int delay, int hide, int pl, int s, int sound)
{
    if (nmv >= MOVES) return;
    cards_slide_start(&mv[nmv].s, G.pos.deck, card, x0, y0, w0, 0, up0, x1, y1, w1, 0, up1, ms, delay);
    mv[nmv].hide = hide;
    mv[nmv].pl = pl;
    mv[nmv].slot = s;
    nmv++;
    Uint32 end = ui_ms() + (Uint32)(delay + ms + REST_MS);
    if (end > quiet) quiet = end;
    if (sound >= 0) ui_sound_later(sound, delay + ms - 40 > 0 ? delay + ms - 40 : 0);
}

static int hides(int kind, int pl, int s)
{
    int n = 0;
    for (int i = 0; i < nmv; i++)
        if (mv[i].hide == kind && cards_slide_busy(&mv[i].s) && (kind != H_SLOT || (mv[i].pl == pl && mv[i].slot == s)))
            n++;
    return n;
}

static int busy(void)
{
    for (int i = 0; i < nmv; i++)
        if (cards_slide_busy(&mv[i].s)) return 1;
    return quiet && ui_ms() < quiet;
}

/* the cards of a new round flying from the stock to every grid */
static void deal_slides(void)
{
    const gpos *p = &G.pos;
    int k = 0, per = p->players * p->slots > 40 ? 10 : 18;
    for (int s = 0; s < p->slots; s++)
        for (int i = 0; i < p->players; i++, k++) {
            int q = (p->first + i) % p->players;
            float x, y, w;
            slot_at(q, s, &x, &y, &w);
            slide(p->card[q][s], 0, T.sx, T.sy, T.wp, 0, x, y, w, DEAL_MS, k * per, H_SLOT, q, s,
                  k % 4 == 0 ? SND_MOVE : -1);
        }
}

/* what an action moved: the slides and flips, from the position before it */
static void animate(const gpos *b, int act)
{
    const gpos *p = &G.pos;
    int k = GA_KIND(act), s = GA_SLOT(act), pl = b->turn, deck = p->deck;
    float x, y, w;
    int base = 0;
    switch (k) {
    case GA_REVEAL:
    case GA_FLIP:
        slot_at(pl, s, &x, &y, &w);
        slide(p->card[pl][s], 0, x, y, w, 1, x, y, w, FLIP_MS, 0, H_SLOT, pl, s, SND_PLACE);
        base = FLIP_MS;
        break;
    case GA_STOCK:
        slide(p->held, 0, T.sx, T.sy, T.wp, 1, T.hx, T.hy, T.wp * 1.06f, SLIDE_MS, 0, H_HELD, 0, 0, SND_MOVE);
        break;
    case GA_TAKE:
        slide(p->held, 1, T.dx, T.dy, T.wp, 1, T.hx, T.hy, T.wp * 1.06f, SLIDE_MS, 0, H_HELD, 0, 0, SND_MOVE);
        break;
    case GA_SWAP:
        slot_at(pl, s, &x, &y, &w);
        slide(b->held, 1, T.hx, T.hy, T.wp * 1.06f, 1, x, y, w, SLIDE_MS, 0, H_SLOT, pl, s, SND_PLACE);
        slide(b->card[pl][s], b->up[pl] >> s & 1, x, y, w, 1, T.dx, T.dy, T.wp, SLIDE_MS, 60, H_PILE, 0, 0, -1);
        base = SLIDE_MS + 60;
        break;
    case GA_DISCARD:
        slide(b->held, 1, T.hx, T.hy, T.wp * 1.06f, 1, T.dx, T.dy, T.wp, SLIDE_MS, 0, H_PILE, 0, 0, SND_PLACE);
        break;
    case GA_NEXT:
        deal_slides();
        return;
    }
    (void)deck;
    int ended = p->phase == GP_ROUND || p->phase == GP_OVER;
    if (ended && b->phase != GP_ROUND) {       /* the round's end: every card still face down turns up */
        int i = 0;
        for (int q = 0; q < p->players; q++)
            for (int t = 0; t < p->slots; t++) {
                if (b->gone[q] >> t & 1 || b->up[q] >> t & 1 || (q == pl && t == s && k == GA_SWAP)) continue;
                if (p->gone[q] >> t & 1) continue;      /* turned up and taken away: below */
                slot_at(q, t, &x, &y, &w);
                slide(p->card[q][t], 0, x, y, w, 1, x, y, w, FLIP_MS, base + 120 + 45 * i, H_SLOT, q, t,
                      i % 3 == 0 ? SND_PLACE : -1);
                i++;
            }
        base += 120 + 45 * i;
    }
    /* number golf: the columns of three taken away, to the discard (the top one last) */
    int n = 0;
    for (int q = 0; q < p->players; q++)
        for (int c = 0; c < p->cols; c++)
            for (int r = p->rows - 1; r >= 0; r--) {
                int t = r * p->cols + c;
                if (!(p->gone[q] >> t & 1) || b->gone[q] >> t & 1) continue;
                slot_at(q, t, &x, &y, &w);
                int was_up = (b->up[q] >> t & 1) || (q == pl && t == s);
                slide(p->card[q][t], was_up, x, y, w, 1, T.dx, T.dy, T.wp, SLIDE_MS, base + 160 + 110 * n, H_PILE, 0,
                      0, n == 0 ? SND_CAPTURE : -1);
                n++;
            }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    variant = v == GOLF_NUMBER ? GOLF_NUMBER : GOLF_CLASSIC;
    golf_new(&G, variant, ui.players, (uint32_t)ui_random(1 << 30));
    calm();
    dealing = 1;
}

static int status(void)
{
    if (G.pos.phase != GP_OVER) return GS_PLAY;
    int w = golf_winner(&G.pos);
    return w < 0 ? GS_DRAW : gs_win(w);
}

/* the lowest total: one winner, or the players tied for it */
static unsigned winners(void)
{
    const gpos *p = &G.pos;
    unsigned m = 0;
    int low = 1 << 20;
    for (int q = 0; q < p->players; q++) low = p->total[q] < low ? p->total[q] : low;
    for (int q = 0; q < p->players; q++) m |= (unsigned)(p->total[q] == low) << q;
    return p->phase == GP_OVER ? m : 0;
}

static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    gpos before = G.pos;
    if (!golf_play(&G, a)) { ui_sound(SND_DENY); return; }
    calm();
    animate(&before, a);
    const gpos *p = &G.pos;
    if (p->phase != GP_ROUND && p->phase != GP_OVER) ui_handoff(p->turn);   /* the device passes (after the cards) */
}

static int undo(void)
{
    if (!golf_undo(&G)) return 0;
    calm();
    dealing = 0;
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = golf_script(&G, s);
    calm();
    dealing = 0;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_tok[GOLF_MAXP], *t_panel, *t_cross;
static int tok_size, panel_w, panel_h, cross_size;

static void tokens(int size)
{
    if (tok_size == size) return;
    tok_size = size;
    for (int q = 0; q < GOLF_MAXP; q++) {
        ui_free(&t_tok[q]);
        cards_token(q, size, ui_buf(size, size));
        t_tok[q] = ui_tex(size, size, ui_buf(size, size));
    }
}

/* the round's scores: a row a player, their token, the points of the round (a cross and a 2 when doubled), the total;
 * the lowest total in gold light */
static void draw_summary(void)
{
    const gpos *p = &G.pos;
    box b = T.b;
    float u = ui.unit, rh = fminf(1.05f * u, b.h * 0.8f / p->players), pw = fminf(b.w * 0.92f, 7.6f * u);
    float ph = rh * p->players + 0.5f * u, x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    if (panel_w != (int)pw || panel_h != (int)ph) {
        panel_w = (int)pw;
        panel_h = (int)ph;
        const uint8_t c[4] = {28, 34, 32, 250};
        ui_free(&t_panel);
        art_roundrect(panel_w, panel_h, 0.4f * u, c, ui_buf(panel_w, panel_h));
        t_panel = ui_tex(panel_w, panel_h, ui_buf(panel_w, panel_h));
    }
    int cs = (int)(0.42f * rh);
    if (cross_size != cs) {
        cross_size = cs;
        ui_free(&t_cross);
        art_icon(ICON_CLOSE, cs, ui_buf(cs, cs));
        t_cross = ui_tex(cs, cs, ui_buf(cs, cs));
        SDL_SetTextureColorMod(t_cross, 250, 214, 90);
    }
    tokens((int)(0.6f * u));
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 70);
    ui_put(t_panel, x0, y0, pw, ph);
    int low = 0;
    for (int q = 1; q < p->players; q++)
        if (p->total[q] < p->total[low]) low = q;
    for (int q = 0; q < p->players; q++) {
        float cy = y0 + 0.25f * u + (q + 0.5f) * rh, tx = x0 + 0.75f * u, d = fminf(0.72f * rh, 0.8f * u);
        if (p->total[q] == p->total[low]) ui_mark_gold(tx, cy, 2.0f * d);
        ui_put_c(t_tok[q], tx, cy, d, d);
        float nh = 0.42f * rh, rx = x0 + pw * 0.45f;
        ui_number_signed(rx, cy, nh, p->last[q], 1, 1);
        if (p->doubled && q == p->ender) {      /* doubled: a gold cross and a 2 */
            ui_put_c(t_cross, rx + 1.35f * nh + 0.2f * nh, cy, 0.8f * nh, 0.8f * nh);
            ui_number(rx + 1.35f * nh + 0.95f * nh, cy, 0.8f * nh, 2, 1);
        }
        ui_number_signed(x0 + pw * 0.8f, cy, 0.56f * rh, p->total[q], 1, 0);
        if (q < p->players - 1)
            ui_fill(x0 + 0.3f * u, cy + rh / 2, pw - 0.6f * u, fmaxf(1, 0.02f * u), 246, 238, 220, 40);
    }
}

static void draw(void)
{
    if (dealing) { dealing = 0; deal_slides(); }
    const gpos *p = &G.pos;
    int deck = p->deck, mine = p->turn == T.me && !ui_handing();
    int ph = p->phase, b = busy();
    /* the stock and the discard (the cards still on their way to it not yet in it) */
    cards_pile(deck, T.sx, T.sy, T.wp, p->nstock, CARD_NONE, mine && ph == GP_DRAW && !b ? CD_GLOW : 0);
    int np = p->npile - hides(H_PILE, 0, 0);
    int dglow = mine && !b && (ph == GP_DRAW || (ph == GP_HOLD && !p->taken));
    cards_pile(deck, T.dx, T.dy, T.wp, np, np > 0 ? p->pile[np - 1] : CARD_NONE, dglow ? CD_GLOW : 0);
    if (p->held != CARD_NONE && !hides(H_HELD, 0, 0)) cards_draw(deck, p->held, T.hx, T.hy, T.wp * 1.06f, 0, CD_RAISED);
    else if (ph == GP_DRAW || ph == GP_HOLD) cards_place(deck, T.hx, T.hy, T.wp);
    /* the grids */
    tokens((int)fmaxf(4, T.tok));
    for (int q = 0; q < p->players; q++) {
        float x, y, w;
        if (q != T.me) {                        /* the others: their token over their grid */
            float gx = T.ox[q] + grid_w(T.wo, p->cols) / 2, ty = T.oy[q] - T.tok * 0.7f;
            if (q == p->ender && ph != GP_ROUND && ph != GP_OVER) ui_mark_gold(gx, ty, 2.2f * T.tok);
            ui_put_c(t_tok[q], gx, ty, T.tok, T.tok);
        }
        for (int s = 0; s < p->slots; s++) {
            if (p->gone[q] >> s & 1 || hides(H_SLOT, q, s)) continue;
            slot_at(q, s, &x, &y, &w);
            int up = p->up[q] >> s & 1, how = up ? 0 : CD_DOWN;
            if (q == T.me && mine && !b && !up && (ph == GP_REVEAL || ph == GP_FLIP)) how |= CD_GLOW;
            cards_draw(deck, p->card[q][s], x, y, w, 0, how);
        }
    }
    for (int i = 0; i < nmv; i++) cards_slide_draw(&mv[i].s);
    if ((ph == GP_ROUND || ph == GP_OVER) && !b) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int my_slot(float x, float y)
{
    const gpos *p = &G.pos;
    for (int s = 0; s < p->slots; s++) {
        float cx, cy, w;
        if (p->gone[T.me] >> s & 1) continue;
        slot_at(T.me, s, &cx, &cy, &w);
        if (cards_in(cx, cy, w * 1.06f, 1.4f, x, y)) return s;
    }
    return -1;
}

static void press(float x, float y)
{
    const gpos *p = &G.pos;
    if (p->phase == GP_ROUND) { if (!busy()) act(GA_NEXT); return; }
    if (p->phase == GP_OVER) return;
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    int s = my_slot(x, y);
    int on_stock = cards_in(T.sx, T.sy, T.wp * 1.1f, 1.4f, x, y);
    int on_pile = cards_in(T.dx, T.dy, T.wp * 1.1f, 1.4f, x, y);
    int a = 0;
    switch (p->phase) {
    case GP_REVEAL: if (s >= 0) a = GA_REVEAL | s; break;
    case GP_DRAW: a = on_stock ? GA_STOCK : on_pile ? GA_TAKE : 0; break;
    case GP_HOLD: a = s >= 0 ? GA_SWAP | s : on_pile ? GA_DISCARD : 0; break;
    case GP_FLIP: if (s >= 0) a = GA_FLIP | s; break;
    }
    if (a && golf_legal(p, a)) act(a);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the player's total */
{
    int v = G.pos.total[player], digits = 1;
    for (int a = abs(v); a >= 10; a /= 10) digits++;
    float w = (0.74f * (digits - 1) + 0.62f + (v < 0 ? 0.56f : 0)) , h = fminf(0.46f * ui.unit, b.w / w * 0.95f);
    ui_number_signed(b.x + w * h / 2, b.y + b.h / 2, h, v, 1, 0);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's and the picker's cards: a grid of cards, some face up (classic: a pair of kings; number golf: a column of
 * three about to go) */
static void thumb(int v, int size, uint8_t *rgba)
{
    int number = v == GOLF_NUMBER, cols = number ? 4 : 3, rows = number ? 3 : 2;
    int deck = number ? DECK_NUMBER : DECK_FRENCH;
    static const int classic[6] = {CARD(SUIT_HEARTS, 13), CARD_NONE, CARD(SUIT_CLUBS, 7), CARD(SUIT_SPADES, 13),
                                   CARD_NONE, CARD(SUIT_DIAMONDS, 1)};
    static const int num[12] = {NUMBER_CARD(5), CARD_NONE, NUMBER_CARD(-1), NUMBER_CARD(0),
                                NUMBER_CARD(5), NUMBER_CARD(9), CARD_NONE, CARD_NONE,
                                NUMBER_CARD(5), CARD_NONE, NUMBER_CARD(2), NUMBER_CARD(12)};
    float w = fminf(size * 0.94f / (1.1f * cols - 0.1f), size * 0.94f / (1.5f * rows - 0.1f));
    int cw = (int)w, ch = (int)(w * 1.4f);
    float x0 = (size - (1.1f * cols - 0.1f) * w) / 2, y0 = (size - (1.5f * rows - 0.1f) * w) / 2;
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    memset(rgba, 0, (size_t)size * size * 4);
    for (int s = 0; s < rows * cols; s++) {
        cards_paint(deck, number ? num[s] : classic[s], cw, ch, b);
        art_blend(rgba, size, size, b, cw, ch, (int)(x0 + (s % cols) * 1.1f * w), (int)(y0 + (s / cols) * 1.5f * w));
    }
    free(b);
}

static const game golf_game = {
    .id = "golf", .order = 260, .variants = GOLF_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6, .view = VIEW_OPEN,
};
REGISTER_GAME(golf_game)
