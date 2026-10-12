/*
 * Barbu in RetroStone Board: four players passing the device round, each hand secret (VIEW_HIDDEN; the rules:
 * barbu_rules.c, barbu.h). The table (the kit's cards_table.h): the viewer's hand fanned at the bottom, the seats round
 * the middle, the trick with each card placed towards its player, the trick pile (tap it: the last trick).
 * - The contract: the declarer taps one of the seven pictures (each crossed out in red but the two positive ones): a
 *   crown (no tricks), a heart (no hearts), a queen (no queens), the red king and his heart (the barbu), the last two
 *   cards of a row (no last two tricks), the four suits (trumps: then one suit), a row of dominoes (dominoes: then the
 *   starting rank, a card of each rank), then the tick. The ones already chosen are faint.
 * - Doubling: the players who may be doubled have their chip ringed in gold; tap them (a gold cross and 2 beside them;
 *   redoubling: 4), then the tick. The doubles so far are listed in the top left corner: the one who doubled, the
 *   cross and 2 (4: redoubled), the one doubled. The deal's contract and its declarer are in the top right corner (tap
 *   it: the table of scores).
 * - Playing: tap a card (it rises), tap it again, or drag it to the middle. Dominoes: the four rows of the suits in the
 *   middle, the starting rank's places outlined; a card laid goes to its place; nothing to lay: the pass button in the
 *   bar. Beside each chip: the deal's points so far (dominoes: the order out).
 * - After each deal the table of scores: a column a contract (its picture), a row a player, the cells each player's
 *   points in that contract (gold: the contracts they chose), the deal's points, the totals; under it the deals, a dot
 *   each in its declarer's colour.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/barbu.h"
#include "games/chess.h"
#include "games/parlour.h"
#include "games/tricks.h"
#include "shell.h"

enum { TAG_LAY = 10 };
enum { PIC_TRUMPS_ALL = 5 };                    /* the trumps' picture: the four suits */

static barbu_game G;
static ct_table T;
static ct_moves A;
static ct_touch F;
static int floor_n, show_last, show_table, pick_c = -1, pick_x = -1;
static unsigned pick_m, seen;                   /* pick_m: the players chosen to (re)double */

static const uint8_t IVORY[3] = {244, 236, 216};

/* ---- the contracts' pictures ------------------------------------------------------------------------------------- */
static void red_cross(uint8_t *rgba, int s)    /* crossed out: two red bars corner to corner */
{
    art_fig *f = art_fig_new();
    const float t = 0.038f, a = 0.2f, b = 0.8f;
    const float one[] = {a - t, a + t, a + t, a - t, b + t, b - t, b - t, b + t};
    const float two[] = {b - t, a - t, b + t, a + t, a + t, b + t, a - t, b - t};
    art_poly(f, ART_PART, one, 4);
    art_poly(f, ART_PART, two, 4);
    art_style st = {{204, 44, 36}, {84, 14, 10}, {204, 44, 36}, {0, 0, 0}, 0.2f, 0.02f};
    parlour_fig(rgba, s, f, &st);
}

static void figure(uint8_t *rgba, int s, int type, const art_style *st, float k, float dy)
{
    int f = (int)(k * s);
    uint8_t *b = calloc((size_t)f * f, 4);
    if (!b) return;
    art_piece_style(type, st, f, b);
    art_blend(rgba, s, s, b, f, f, (s - f) / 2, (s - f) / 2 + (int)(dy * s));
    free(b);
}

static void heart(art_fig *f, float cx, float cy, float k)   /* a heart k wide, centred */
{
    const float tip[] = {cx - 0.47f * k, cy - 0.02f * k, cx + 0.47f * k, cy - 0.02f * k, cx, cy + 0.5f * k};
    art_ellipse(f, ART_PART, cx - 0.24f * k, cy - 0.12f * k, 0.26f * k, 0.25f * k);
    art_ellipse(f, ART_PART, cx + 0.24f * k, cy - 0.12f * k, 0.26f * k, 0.25f * k);
    art_poly(f, ART_PART, tip, 3);
}

static void paint_contract(int kind, int s, uint8_t *rgba)
{
    float asp = cards_aspect(DECK_FRENCH);
    memset(rgba, 0, (size_t)s * s * 4);
    art_disc(s, IVORY, 1, 0, rgba);
    cards_disc_rim(rgba, s);
    switch (kind) {
    case BB_NOTRICKS: {                         /* a crown */
        art_fig *f = art_fig_new();
        const float crown[] = {0.27f, 0.66f, 0.23f, 0.34f, 0.38f, 0.5f, 0.5f, 0.27f, 0.62f, 0.5f, 0.77f, 0.34f, 0.73f,
                               0.66f};
        art_poly(f, ART_PART, crown, 7);
        art_rect(f, ART_PART, 0.27f, 0.64f, 0.73f, 0.73f);
        art_ellipse(f, ART_PART, 0.23f, 0.32f, 0.04f, 0.04f);
        art_ellipse(f, ART_PART, 0.5f, 0.25f, 0.045f, 0.045f);
        art_ellipse(f, ART_PART, 0.77f, 0.32f, 0.04f, 0.04f);
        art_style st = {{232, 186, 72}, {104, 72, 18}, {150, 110, 36}, {0, 0, 0}, 0.5f, 0.022f};
        parlour_fig(rgba, s, f, &st);
        break;
    }
    case BB_NOHEARTS: {
        art_fig *f = art_fig_new();
        heart(f, 0.5f, 0.5f, 0.56f);
        art_style st = {{206, 38, 44}, {96, 12, 16}, {206, 38, 44}, {0, 0, 0}, 0.45f, 0.022f};
        parlour_fig(rgba, s, f, &st);
        break;
    }
    case BB_NOQUEENS: figure(rgba, s, QUEEN, &art_player[1], 0.74f, 0); break;
    case BB_KING: {                             /* the red king, a small heart at his foot */
        figure(rgba, s, KING, &art_player[2], 0.7f, -0.03f);
        art_fig *f = art_fig_new();
        heart(f, 0.71f, 0.7f, 0.2f);
        art_style st = {{206, 38, 44}, {96, 12, 16}, {206, 38, 44}, {0, 0, 0}, 0.3f, 0.016f};
        parlour_fig(rgba, s, f, &st);
        break;
    }
    case BB_LASTTWO: {                          /* a row of five backs: the last two bright */
        int cw = (int)(0.17f * s), ch = (int)(cw * asp);
        uint8_t *b = malloc((size_t)cw * ch * 4);
        if (!b) break;
        cards_paint(DECK_FRENCH, CARD_NONE, cw, ch, b);
        for (int i = 0; i < 5; i++)
            parlour_blend(rgba, s, b, cw, ch, (int)((0.13f + i * 0.152f) * s), (s - ch) / 2, i < 3 ? 0.35f : 1);
        free(b);
        break;
    }
    case PIC_TRUMPS_ALL: {                      /* the four suits */
        int k = (int)(0.36f * s);
        uint8_t *b = malloc((size_t)k * k * 4);
        if (!b) break;
        for (int su = 0; su < 4; su++) {
            cards_suit_icon(DECK_FRENCH, su, k, b);
            art_blend(rgba, s, s, b, k, k, (int)((su & 1 ? 0.52f : 0.12f) * s), (int)((su & 2 ? 0.52f : 0.12f) * s));
        }
        free(b);
        break;
    }
    case BB_DOMINO: {                           /* a row of four tiles, one to four pips */
        art_fig *f = art_fig_new(), *m = art_fig_new();
        for (int i = 0; i < 4; i++) {
            float x0 = 0.14f + i * 0.18f;
            art_rect(f, ART_PART, x0, 0.33f, x0 + 0.17f, 0.67f);
            for (int k = 0; k <= i; k++) {
                float py = i == 0 ? 0.5f : 0.39f + 0.22f * k / i;
                float px = x0 + 0.085f + (i >= 2 ? (k & 1 ? 0.035f : -0.035f) : 0);
                art_ellipse(m, ART_PART, px, py, 0.024f, 0.024f);
            }
        }
        art_style st = {{250, 246, 236}, {60, 44, 30}, {60, 44, 30}, {0, 0, 0}, 0.25f, 0.018f};
        art_style dots = {{30, 30, 30}, {30, 30, 30}, {30, 30, 30}, {0, 0, 0}, 0, 0};
        parlour_fig(rgba, s, f, &st);
        parlour_fig(rgba, s, m, &dots);
        break;
    }
    }
    if (kind < BB_TRUMPS) red_cross(rgba, s);
}

static SDL_Texture *contract_pic(int kind, float d) { return ct_picture(paint_contract, kind, (int)(d + 0.5f)); }

/* a double: a gold cross and 2 (redoubled: 4), centred on (cx, cy), h high */
static float mult_w(float h) { return 1.65f * h; }
static void draw_mult(float cx, float cy, float h, int level)
{
    float x = cx - mult_w(h) / 2;
    int s = (int)(0.8f * h);
    ui_put_c(ct_icon(ICON_CLOSE, s, 250, 214, 90), x + 0.42f * h, cy, (float)s, (float)s);
    ui_number(x + 1.25f * h, cy, h, level == 2 ? 4 : 2, 1);
}

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b) { ct_layout(&T, b, BARBU_P, ui_bottom(), BARBU_HAND); }

static void reset_ui(void)
{
    show_last = show_table = 0;
    pick_c = pick_x = -1;
    pick_m = 0;
    ct_touch_reset(&F);
}

static int hand(uint8_t *h, int *how, int live)
{
    const barbu_pos *p = &G.pos;
    int n = p->n[T.me], mine = live && p->turn == T.me && p->phase == BBP_PLAY;
    for (int i = 0; i < n; i++) {
        int c = p->hand[T.me][i];
        h[i] = (uint8_t)c;
        how[i] = mine ? (barbu_can_play(p, c) ? CD_GLOW : CD_DIM) : 0;
        if (ct_is_raised(&F, i)) how[i] = CD_RAISED;
    }
    return n;
}

/* the area free for the middle's pictures: inside the seats */
static box middle(void)
{
    box a;
    a.x = T.b.x + 1.7f * T.tok;
    a.w = T.b.w - 3.4f * T.tok;
    a.y = T.b.y + 1.6f * T.tok;
    a.h = T.hand_top - 0.1f * T.u - a.y;
    return a;
}

/* the contract's choice: the seven pictures (one row or two), the suits or the ranks under them, the tick */
static struct { float d, x[7], y[7], sd, sx[4], sy, cw, rx[13], ry[13], tx, ty, td; } K;

static void choice_layout(void)
{
    box a = middle();
    float u = T.u, asp = T.asp;
    int cols = a.w >= 7 * 1.12f * 0.95f * u ? 7 : 4, rows = cols == 7 ? 1 : 2;
    float d = floorf(fminf(1.2f * u, a.w / (cols * 1.12f)));
    float cw = fminf(0.72f * d, a.w / (7 * 1.1f));
    float total = rows * 1.12f * d + 0.25f * d + 2.1f * cw * asp + 0.3f * d + 1.15f * u;   /* the tallest: ranks */
    float k = total > a.h ? a.h / total : 1;
    d = floorf(d * k), cw = floorf(cw * k);
    float td = 1.1f * u * k, y = a.y + 0.56f * d;   /* the contracts stay put; what follows them grows down */
    K.d = d, K.cw = cw, K.td = td;
    for (int c = 0; c < 7; c++) {
        int r = c / cols, i = c % cols, in_row = r == rows - 1 ? 7 - r * cols : cols;
        K.x[c] = T.cx + (i - (in_row - 1) / 2.0f) * 1.12f * d;
        K.y[c] = y + r * 1.12f * d;
    }
    float sub = y + (rows - 1) * 1.12f * d + 0.56f * d + 0.25f * d;
    K.sd = floorf(fminf(0.9f * d, 1.05f * cw * asp));
    for (int s = 0; s < 4; s++) K.sx[s] = T.cx + (s - 1.5f) * 1.15f * K.sd;
    K.sy = sub + 0.5f * K.sd + 0.1f * d;
    for (int r = 0; r < 13; r++) {               /* the ranks 2..ace, rows of seven and six */
        int row = r / 7, i = r % 7, in_row = row ? 6 : 7;
        K.rx[r] = T.cx + (i - (in_row - 1) / 2.0f) * 1.1f * cw;
        K.ry[r] = sub + (0.5f + row * 1.05f) * cw * asp;
    }
    K.tx = T.cx;
    K.ty = sub + (pick_c == BB_DOMINO ? 2.1f * cw * asp : pick_c == BB_TRUMPS ? K.sd + 0.2f * d : 0) +
           0.3f * d + td / 2;
}

/* the deal's contract, top right: the declarer's chip, the contract, the trump suit or the starting rank */
static void corner(float *x, float *y, float *d)
{
    *d = floorf(fmaxf(1.15f * T.tok, 0.62f * T.u));
    *x = T.b.x + T.b.w - 0.2f * T.u - *d / 2;
    *y = T.b.y + 0.2f * T.u + *d / 2;
}

/* ---- dominoes: four rows (the suits as a hand is sorted), thirteen places (2..ace) ------------------------------- */
static struct { float cw, x0, step, y[4]; } L;

static void lay_layout(void)
{
    box a = middle();
    float asp = T.asp;
    L.cw = floorf(fminf(T.wt, fminf(a.w / (1 + 12 * 0.62f), a.h / (4 * 1.08f * asp))));
    L.step = 0.62f * L.cw;
    L.x0 = T.cx - 6 * L.step;
    static const int row_of[4] = {2, 3, 1, 0}; /* spades, hearts, diamonds, clubs: as trick_suit_order */
    float h = L.cw * asp * 1.08f, top = a.y + (a.h - 4 * h) / 2 + h / 2;
    for (int s = 0; s < 4; s++) L.y[s] = top + row_of[s] * h;
}

static void lay_spot(int card, float *x, float *y)
{
    *x = L.x0 + (trick_rank(card) - 2) * L.step;
    *y = L.y[CARD_SUIT(card)];
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    barbu_new(&G, v, (uint32_t)ui_random(1 << 30));
    ct_calm(&A);
    reset_ui();
    floor_n = 0;
    seen = 0;
}

static int status(void)
{
    if (G.pos.phase != BBP_OVER) return GS_PLAY;
    unsigned w = barbu_winners(&G.pos);
    if (w & (w - 1)) return GS_DRAW;
    for (int q = 0; q < BARBU_P; q++)
        if (w >> q & 1) return gs_win(q);
    return GS_DRAW;
}

static unsigned winners(void) { return barbu_winners(&G.pos); }

static int turn(void) { return G.pos.turn; }

/* a card laid (dominoes): from its place in the hand as shown, or from the finger, to its place in the rows */
static void lay(const uint8_t *h, int n, int card)
{
    cards_spot s[CT_HAND];
    ct_fan(&T, n, s);
    int i = 0;
    while (i < n && h[i] != card) i++;
    float x0 = i < n ? s[i].x : T.hx, y0 = i < n ? s[i].y - 0.14f * T.wh : T.hy, d0 = i < n ? s[i].deg : 0, x, y;
    if (F.on) x0 = F.x, y0 = F.y, d0 = 0;
    lay_layout();
    lay_spot(card, &x, &y);
    ct_rest(&A, ct_slide(&A, DECK_FRENCH, card, x0, y0, T.wh, d0, 1, x, y, L.cw, 0, 1, CT_SLIDE_MS, 0, TAG_LAY, card,
                         -1, SND_PLACE), 200);
}

static void animate(const barbu_pos *b, int a, const uint8_t *h, int n)
{
    const barbu_pos *p = &G.pos;
    int k = BBA_KIND(a);
    if (k == BBA_CHOOSE || k == BBA_DOUBLE || k == BBA_REDOUBLE) { ui_sound(SND_PLACE); return; }
    if (k != BBA_PLAY) return;
    if (b->contract == BB_DOMINO) { lay(h, n, BBA_ARG(a)); return; }
    ct_play(&A, &T, &F, h, n, BBA_ARG(a));
    if (p->tricks != b->tricks || p->deal != b->deal)
        ct_gather(&A, &T, p->last, NULL, BARBU_P, p->last_leader, p->last_winner, CT_SLIDE_MS + CT_LOOK_MS,
                  CT_SLIDE_MS);
}

static void act(int a)
{
    barbu_pos before = G.pos;
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 0);
    if (!barbu_play(&G, a)) { ui_sound(SND_DENY); return; }
    ct_calm(&A);
    animate(&before, a, h, n);
    reset_ui();
    const barbu_pos *p = &G.pos;
    if (BBA_KIND(a) == BBA_NEXT) seen = 0;
    if (p->turn != before.turn || p->phase == BBP_ROUND || p->phase == BBP_OVER) floor_n = G.n;
    if (p->phase < BBP_ROUND) ui_handoff(p->turn);
}

static int undo(void)
{
    if (pick_c >= 0 || pick_m || F.raised != CARD_NONE || show_last || show_table) { reset_ui(); return 1; }
    if (G.n <= floor_n || !barbu_undo(&G)) return 0;
    ct_calm(&A);
    reset_ui();
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = barbu_script(&G, s);
    ct_calm(&A);
    reset_ui();
    floor_n = G.n;
    seen = ~0u;
    ui_handoff(G.pos.turn);
    return ok;
}

static int busy(void) { return ct_busy(&A); }

static int can_pass(void) { return !ui_handing() && !busy() && G.pos.turn == T.me && barbu_must_pass(&G.pos); }

static void pass(void) { act(BBA_PASS); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* the contract's picture, its suit or rank beside it (x: the picture's centre; to the left of it) */
static float draw_contract(float x, float y, float d, int with_x)
{
    const barbu_pos *p = &G.pos;
    if (p->contract >= BARBU_CONTRACTS) return x;
    ui_put_c(contract_pic(p->contract, d), x, y, d, d);
    if (!with_x) return x - d / 2;
    if (p->contract == BB_TRUMPS) {
        ui_put_c(ct_suit(DECK_FRENCH, p->trump, (int)(0.8f * d)), x - 1.0f * d, y, 0.8f * d, 0.8f * d);
        return x - 1.4f * d;
    }
    if (p->contract == BB_DOMINO) {
        float w = floorf(0.62f * d / T.asp * 1.3f);
        cards_draw(DECK_FRENCH, CARD(SUIT_SPADES, p->rank == 14 ? RANK_ACE : p->rank), x - 0.62f * d - 0.5f * w, y, w,
                   0, 0);
        return x - 0.7f * d - w;
    }
    return x - d / 2;
}

static void draw_corner(void)
{
    const barbu_pos *p = &G.pos;
    float x, y, d;
    corner(&x, &y, &d);
    float left = p->contract < BARBU_CONTRACTS ? draw_contract(x, y, d, 1) : x + d / 2;
    float td = floorf(0.75f * d);
    ui_put_c(ct_token(p->declarer, (int)td), left - 0.2f * d - td / 2, y, td, td);
}

/* the doubles so far, top left: the one who doubled, the cross and 2 (4), the one doubled; three a column */
static void draw_ledger(void)
{
    const barbu_pos *p = &G.pos;
    float h = floorf(fmaxf(0.75f * T.tok, 0.36f * T.u)), x0 = T.b.x + 0.2f * T.u, y0 = T.b.y + 0.2f * T.u + h / 2;
    float rw = 2 * h + mult_w(0.8f * h) + 0.5f * h;
    int k = 0;
    for (int a = 0; a < BARBU_P; a++)
        for (int b = 0; b < BARBU_P; b++) {
            if (!p->dbl[a][b]) continue;
            float x = x0 + (k / 3) * (rw + 0.4f * h), y = y0 + (k % 3) * 1.2f * h;
            ct_panel(x - 0.12f * h, y - 0.56f * h, rw + 0.24f * h, 1.12f * h, 200);
            ui_put_c(ct_token(a, (int)h), x + h / 2, y, h, h);
            draw_mult(x + h + 0.25f * h + mult_w(0.8f * h) / 2, y, 0.8f * h, p->dbl[a][b]);
            ui_put_c(ct_token(b, (int)h), x + rw - h / 2, y, h, h);
            k++;
        }
}

/* the choice of the contract */
static void draw_choice(int live)
{
    const barbu_pos *p = &G.pos;
    choice_layout();
    for (int c = 0; c < BARBU_CONTRACTS; c++) {
        int kind = c == BB_TRUMPS ? PIC_TRUMPS_ALL : c;
        ct_choice(contract_pic(kind, K.d), K.x[c], K.y[c], K.d, c == pick_c, barbu_can_choose(p, c));
    }
    if (pick_c == BB_TRUMPS)
        for (int s = 0; s < 4; s++)
            ct_choice(ct_suit(DECK_FRENCH, s, (int)K.sd), K.sx[s], K.sy, K.sd, s == pick_x, 1);
    if (pick_c == BB_DOMINO)
        for (int r = 0; r < 13; r++) {
            int rank = r + 2;
            if (rank == pick_x) ui_mark_gold(K.rx[r], K.ry[r], 1.5f * K.cw);
            cards_draw(DECK_FRENCH, CARD(SUIT_SPADES, rank == 14 ? RANK_ACE : rank), K.rx[r],
                       K.ry[r] - (rank == pick_x ? 0.1f * K.cw : 0), K.cw, 0, rank == pick_x ? CD_RAISED : 0);
        }
    int ok = pick_c >= 0 && (pick_c < BB_TRUMPS || pick_x >= 0);
    if (ok && live) ui_mark_gold(K.tx, K.ty, 2.0f * K.td);
    ct_button(K.tx, K.ty, K.td, ok, CT_TICK, 0);
}

/* doubling: the contract big in the middle, the tick under it; the chips that may be doubled ringed, the chosen ones
 * with the cross and 2 */
static float seat_r(void) { return fmaxf(0.9f * T.tok, 0.45f * T.u); }

static void tick_at(float *x, float *y, float *d)
{
    *d = 1.1f * T.u;
    *x = T.cx;
    *y = T.cy + 0.95f * T.u;
}

static void draw_doubling(int live)
{
    const barbu_pos *p = &G.pos;
    unsigned m = barbu_double_mask(p);
    float d = floorf(1.5f * T.u), x, y, td;
    draw_contract(T.cx + (p->contract >= BB_TRUMPS ? 0.5f * d : 0), T.cy - 0.55f * T.u, d, 1);
    for (int q = 0; q < BARBU_P; q++) {
        if (!(m >> q & 1)) continue;
        if (pick_m >> q & 1) {
            ui_mark_gold(T.sx[q], T.sy[q], 2.4f * T.tok);
            float sx, sy;
            ct_seat_spot(&T, q, 0, &sx, &sy);
            ct_pic(CT_DISC_DARK, sx + 0.3f * T.tok * (sx > T.sx[q] ? 1 : -1), sy, 1.3f * T.tok, 0, 255, 255, 255, 255);
            draw_mult(sx + 0.3f * T.tok * (sx > T.sx[q] ? 1 : -1), sy, 0.5f * T.tok,
                      p->phase == BBP_REDOUBLE ? 2 : 1);
        } else ui_mark_select_round(T.sx[q], T.sy[q], 1.6f * T.tok);
        ui_put_c(ct_token(q, (int)T.tok), T.sx[q], T.sy[q], T.tok, T.tok);   /* the chip over its light */
    }
    tick_at(&x, &y, &td);
    if (live) ui_mark_gold(x, y, 1.8f * td);
    ct_button(x, y, td, 1, CT_TICK, 0);
}

static void draw_layout(int live)
{
    const barbu_pos *p = &G.pos;
    lay_layout();
    for (int s = 0; s < 4; s++) {
        float x, y;
        lay_spot(CARD(s, p->rank == 14 ? RANK_ACE : p->rank), &x, &y);
        if (!p->lo[s]) { cards_place(DECK_FRENCH, x, y, L.cw); continue; }
        for (int r = p->lo[s]; r <= p->hi[s]; r++) {
            int c = CARD(s, r == 14 ? RANK_ACE : r);
            if (ct_hides(&A, TAG_LAY, c, -1)) continue;
            lay_spot(c, &x, &y);
            cards_draw(DECK_FRENCH, c, x, y, L.cw, 0, 0);
        }
    }
    if (F.raised != CARD_NONE && live) {        /* the raised card's place */
        float x, y;
        lay_spot(F.raised, &x, &y);
        ui_mark_gold(x, y, 1.6f * L.cw);
        cards_place(DECK_FRENCH, x, y, L.cw);
    }
}

/* beside each chip: the deal's points so far; dominoes: the order out */
static void draw_points(void)
{
    const barbu_pos *p = &G.pos;
    for (int q = 0; q < BARBU_P; q++) {
        float x, y;
        ct_seat_spot(&T, q, 0, &x, &y);
        if (p->contract == BB_DOMINO) {
            for (int i = 0; i < p->nout; i++)
                if (p->out[i] == q) ct_badge(x, y, 0.95f * T.tok, i + 1);
            continue;
        }
        if (p->pts[q]) ui_number_signed(x + (x > T.sx[q] ? 0.3f : -0.3f) * T.tok, y, 0.5f * T.tok, p->pts[q], 1, 0);
    }
}

/* the table of scores: a column a contract, the deal's points, the totals; a row a player; the deals under it */
static void draw_table(void)
{
    const barbu_pos *p = &G.pos;
    box b = T.b;
    float u = T.u, pw = fminf(b.w * 0.97f, 11.0f * u), cw = pw / 10.7f;
    int n = p->deals, groups = BARBU_IS_SHORT(p->variant) ? 1 : 4, per = n / groups;
    float rh = fminf(0.95f * u, b.h * 0.13f), hh = fminf(0.85f * cw, 0.8f * u);
    float dot = floorf(fminf(0.32f * u, (pw - 0.6f * u) / (n * 1.25f + (groups - 1) * 1.0f)));
    float ph = 0.25f * u + hh + 0.15f * u + 4 * rh + 0.25f * u + dot + 0.35f * u;
    float x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 80);
    ct_panel(x0, y0, pw, ph, 246);
    float y = y0 + 0.25f * u + hh / 2, cx[10];
    cx[0] = x0 + 0.75f * cw;
    for (int c = 0; c < 7; c++) cx[c + 1] = x0 + (1.6f + c) * cw + 0.5f * cw;
    cx[8] = x0 + 9.0f * cw, cx[9] = x0 + 10.0f * cw;
    int cur = p->deal > 0 && p->contract < BARBU_CONTRACTS ? p->contract : -1;
    for (int c = 0; c < 7; c++) {
        float d = 0.9f * hh;
        if (c == cur) ui_mark_gold(cx[c + 1], y, 1.7f * d);
        ui_put_c(contract_pic(c == BB_TRUMPS ? PIC_TRUMPS_ALL : c, d), cx[c + 1], y, d, d);
    }
    y += hh / 2 + 0.15f * u + rh / 2;
    int top = -32000;
    for (int q = 0; q < BARBU_P; q++) top = p->total[q] > top ? p->total[q] : top;
    float nh = floorf(fminf(0.36f * rh, 0.31f * cw)), td = fminf(0.7f * rh, 0.75f * cw);
    for (int q = 0; q < BARBU_P; q++, y += rh) {
        if (p->total[q] == top) ui_mark_gold(cx[0], y, 1.9f * td);
        ui_put_c(ct_token(q, (int)td), cx[0], y, td, td);
        for (int c = 0; c < 7; c++) {           /* a gold cell: the contract they chose; a number: one played */
            if (p->chosen[q] >> c & 1)
                ui_fill(cx[c + 1] - 0.46f * cw, y - 0.42f * rh, 0.92f * cw, 0.84f * rh, 214, 168, 66, 70);
            if (p->used >> c & 1) ui_number_signed(cx[c + 1], y, nh, p->table[q][c], 1, 0);
        }
        if (p->deal > 0) ui_number_signed(cx[8], y, nh, p->score[q], 1, 1);
        ui_number_signed(cx[9], y, floorf(1.25f * nh), p->total[q], 1, 0);
        ct_rule(x0 + 0.3f * u, y + rh / 2, pw - 0.6f * u);
    }
    /* the deals: a dot each in its declarer's colour, those played bright, the last one in gold */
    y += -rh / 2 + 0.25f * u + 0.5f * dot;
    uint8_t who[BARBU_P * BARBU_CONTRACTS];
    for (int i = 0; i < n; i++) who[i] = (uint8_t)barbu_declarer_of(p->variant, i);
    parlour_dots(x0 + pw / 2, y, dot, n, per, who, p->deal);
}

static void draw(void)
{
    const barbu_pos *p = &G.pos;
    int live = !ui_handing() && !ct_busy(&A), ph = p->phase, hide = ui.viewer < 0, mine = p->turn == T.me && !hide;
    if (!(seen >> T.me & 1) && !ui_handing()) {   /* the viewer's first look at this deal's cards */
        seen |= 1u << T.me;
        if (ph < BBP_ROUND && p->n[T.me] == BARBU_HAND && p->ntrick == 0) {
            uint8_t h[CT_HAND];
            int how[CT_HAND];
            ct_deal(&A, &T, h, hand(h, how, 0));
        }
    }
    ct_draw_seats(&T, ph < BBP_ROUND ? p->turn : -1);
    draw_corner();
    draw_ledger();
    if (ph == BBP_PLAY || ph == BBP_ROUND) draw_points();
    if (p->contract != BB_DOMINO) ct_draw_pile(&T, p->tricks);
    if (ph == BBP_CHOOSE && mine) draw_choice(live);
    if ((ph == BBP_DOUBLE || ph == BBP_REDOUBLE) && mine) draw_doubling(live);
    if (ph == BBP_PLAY || ph == BBP_ROUND || ph == BBP_OVER) {
        if (p->contract == BB_DOMINO) draw_layout(live && mine);
        else {
            ct_draw_trick(&T, &A, p->trick, NULL, p->ntrick, p->leader);
            if (F.raised != CARD_NONE && live) ct_draw_target(&T);
        }
    }
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, live);
    if (hide) ct_hand_backs(&T, n);
    else ct_hand_draw(&T, &A, h, n, how, NULL, ct_dragged(&F));
    ct_draw(&A);
    if (!hide) ct_touch_draw(&F, &T);
    if (show_last && p->last_winner != TRICK_NONE)
        ct_draw_last(&T, p->last, NULL, BARBU_P, p->last_leader, p->last_winner);
    if (((ph == BBP_ROUND || ph == BBP_OVER) && !ct_busy(&A)) || show_table) draw_table();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void send(int card)
{
    if (barbu_can_play(&G.pos, card)) act(BBA_PLAY | card);
    else { ct_lower(&F); ui_sound(SND_DENY); }
}

static int press_choice(float x, float y)
{
    const barbu_pos *p = &G.pos;
    choice_layout();
    for (int c = 0; c < BARBU_CONTRACTS; c++) {
        if (!ct_on(K.x[c], K.y[c], K.d, x, y)) continue;
        if (!barbu_can_choose(p, c)) { ui_sound(SND_DENY); return 1; }
        pick_c = c, pick_x = -1;
        ui_sound(SND_TAP);
        return 1;
    }
    if (pick_c == BB_TRUMPS)
        for (int s = 0; s < 4; s++)
            if (ct_on(K.sx[s], K.sy, K.sd, x, y)) { pick_x = s; ui_sound(SND_TAP); return 1; }
    if (pick_c == BB_DOMINO)
        for (int r = 0; r < 13; r++)
            if (cards_in(K.rx[r], K.ry[r], K.cw, T.asp, x, y)) { pick_x = r + 2; ui_sound(SND_TAP); return 1; }
    if (ct_on(K.tx, K.ty, K.td, x, y)) {
        if (pick_c >= 0 && (pick_c < BB_TRUMPS || pick_x >= 0))
            act(BBA_CONTRACT(pick_c, pick_c < BB_TRUMPS ? 0 : pick_x));
        else ui_sound(SND_DENY);
        return 1;
    }
    return 0;
}

static int press_doubling(float x, float y)
{
    const barbu_pos *p = &G.pos;
    unsigned m = barbu_double_mask(p);
    for (int q = 0; q < BARBU_P; q++) {
        if (q == T.me || hypotf(x - T.sx[q], y - T.sy[q]) > seat_r()) continue;
        if (!(m >> q & 1)) { ui_sound(SND_DENY); return 1; }
        pick_m ^= 1u << q;
        ui_sound(SND_TAP);
        return 1;
    }
    float tx, ty, td;
    tick_at(&tx, &ty, &td);
    if (ct_on(tx, ty, td, x, y)) {
        act((p->phase == BBP_DOUBLE ? BBA_DOUBLE : BBA_REDOUBLE) | (int)pick_m);
        return 1;
    }
    return 0;
}

static void press(float x, float y)
{
    const barbu_pos *p = &G.pos;
    F.card = CARD_NONE;
    F.on = 0;
    if (show_last || show_table) { show_last = show_table = 0; return; }
    if (ct_busy(&A)) return;
    if (p->phase == BBP_ROUND) { act(BBA_NEXT); return; }
    if (p->phase == BBP_OVER) return;
    float cx, cy, cd;
    corner(&cx, &cy, &cd);
    if (x > cx - 3 * cd && y < cy + 0.7f * cd) { show_table = 1; ui_sound(SND_TAP); return; }
    if (p->contract != BB_DOMINO && ct_on_pile(&T, p->tricks, x, y)) { show_last = 1; ui_sound(SND_TAP); return; }
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    if (p->phase == BBP_CHOOSE) {
        if (!press_choice(x, y)) ui_sound(SND_DENY);
        return;
    }
    if (p->phase == BBP_DOUBLE || p->phase == BBP_REDOUBLE) {
        if (!press_doubling(x, y)) ui_sound(SND_DENY);
        return;
    }
    if (F.raised != CARD_NONE) {                /* the raised card's place: the trick's, or its row's */
        float lx, ly;
        if (p->contract == BB_DOMINO) {
            lay_layout();
            lay_spot(F.raised, &lx, &ly);
            if (cards_in(lx, ly, L.cw * 1.2f, T.asp, x, y)) { send(F.raised); return; }
        } else if (ct_on_target(&T, x, y)) { send(F.raised); return; }
    }
    uint8_t h[CT_HAND];
    int how[CT_HAND];
    int n = hand(h, how, 1);
    ct_touch_hand(&F, &T, h, n, how, NULL, x, y);   /* a tap or a drag: the release says */
}

static void motion(float x, float y) { ct_touch_move(&F, x, y); }

static void release(float x, float y)
{
    int c, r = ct_touch_up(&F, &T, x, y, &c);
    if (r == CT_T_SEND) send(c);
    else if (r == CT_T_TAP && ct_tap_play(&F, c, barbu_can_play(&G.pos, c))) send(c);
    F.on = 0;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ct_bar(b, G.pos.n[player], G.pos.total[player]); }

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's and the picker's cards: the whole game, four cards (the barbu on top); the short one, two; doubling: the
 * gold cross on a dark disc in the corner */
static void thumb(int v, int size, uint8_t *rgba)
{
    static const int full[4] = {CARD(SUIT_SPADES, RANK_QUEEN), CARD(SUIT_DIAMONDS, RANK_QUEEN),
                                CARD(SUIT_HEARTS, RANK_ACE), CARD(SUIT_HEARTS, RANK_KING)},
                     shrt[2] = {CARD(SUIT_SPADES, RANK_QUEEN), CARD(SUIT_HEARTS, RANK_KING)};
    if (v < 0) v = BARBU_FULL;
    if (BARBU_IS_SHORT(v)) ct_thumb(shrt, 2, size, rgba);
    else ct_thumb(full, 4, size, rgba);
    if (!BARBU_DOUBLING(v)) return;
    int d = (int)(0.34f * size), k = (int)(0.24f * size);
    static const uint8_t dark[3] = {22, 30, 27};
    uint8_t *b = malloc((size_t)d * d * 4), *c = malloc((size_t)k * k * 4);
    if (b && c) {
        art_disc(d, dark, 0.92f, 0, b);
        art_blend(rgba, size, size, b, d, d, size - d - size / 40, size / 40);
        art_icon(ICON_CLOSE, k, c);
        for (int i = 0; i < k * k; i++) c[i * 4] = 250, c[i * 4 + 1] = 214, c[i * 4 + 2] = 90;
        art_blend(rgba, size, size, c, k, k, size - d - size / 40 + (d - k) / 2, size / 40 + (d - k) / 2);
    }
    free(b);
    free(c);
}

static const game barbu_game_def = {
    .id = "barbu", .order = 317, .variants = BARBU_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(barbu_game_def)
