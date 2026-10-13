/*
 * Le Nain jaune in RetroStone Board: 3 to 6 players passing the device round (the rules: nainjaune_rules.c,
 * nainjaune.h). Each hand is secret (VIEW_HIDDEN): the player to move sees their hand at the bottom (sorted by rank),
 * the others' hands as small fans of backs along the top, and between them the board, a plank of the house wood with
 * five round wells: the ten of diamonds, the jack of clubs, the queen of spades and the king of hearts in the corners,
 * the yellow dwarf (the seven of diamonds) in the middle in a brass ring, each with its card lying in it and the bone
 * chips staked on it. Beside the board: the sequence being played, card after card, and the talon face down.
 * Tap a card that glows to play it; the device passes when the sequence goes to another player. Chips fly: the stakes
 * into the wells at each deal, a well's chips to whoever plays its card, the payments when a player goes out; then a
 * summary of the deal (no words: tokens, cards, chips and numbers); a tap deals again.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards_table.h"
#include "games/dicecup.h"
#include "games/nainjaune.h"

#define SLIDE_MS 380
#define DEAL_MS 230
#define FLY_MS 460
#define FLY_GAP 70
#define RUN_SHOWN 7                             /* the sequence's last cards shown */

static njgame G;
static int dealing, variant;
static ct_moves MV = {.rest = 300};
enum { H_HAND = 1, H_SEAT, H_RUN };

/* the wells: their centres and radii, fractions of the board */
static const float WELL[NJ_BOXES][3] = {
    {0.23f, 0.23f, 0.17f}, {0.77f, 0.23f, 0.17f}, {0.23f, 0.77f, 0.17f}, {0.77f, 0.77f, 0.17f}, {0.5f, 0.5f, 0.19f},
};

static struct {
    box b, hb;                                  /* the table; the viewer's hand */
    int me, k;
    float sy, sw, sfan;                         /* the others' seats */
    float bx, by, B;                            /* the board */
    float rx, ry, rw;                           /* the sequence: its first card's centre, the cards' width */
    float tx, ty, tw;                           /* the talon */
} T;

static float seat_x(int q) { return ct_seat_x(T.b.x, T.b.w, T.k, ct_seat(q, T.me, G.pos.players)); }

static void layout(box b)
{
    const njpos *p = &G.pos;
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= p->players) T.me = 0;
    T.k = p->players - 1;
    int wide = b.w > b.h * 1.05f;
    float hs = b.h * (wide ? 0.19f : 0.16f), hm = b.h * (wide ? 0.5f : 0.44f), u = ui.unit;
    T.sw = floorf(fminf(fminf(hs * 0.62f / 1.4f, b.w / T.k * 0.3f), 0.85f * u));
    T.sfan = fminf(b.w / T.k * 0.86f, T.sw * 3.2f);
    T.sy = b.y + hs * 0.42f;
    T.B = floorf(fminf(hm * 0.97f, b.w * (wide ? 0.44f : 0.62f)));
    float mid = b.y + hs + hm / 2, left = b.x + (wide ? 0.08f : 0.01f) * b.w;
    T.bx = floorf(left);
    T.by = floorf(mid - T.B / 2);
    float rx0 = T.bx + T.B + 0.25f * u, rx1 = b.x + b.w - 0.1f * u, room = rx1 - rx0;
    T.rw = floorf(fminf(room / (1 + 0.3f * (RUN_SHOWN - 1)), hm * 0.55f / 1.4f));
    T.rx = rx0 + T.rw / 2;
    T.ry = mid - 0.18f * hm;
    T.tw = floorf(0.62f * T.rw);
    T.tx = rx1 - T.tw * 0.7f;
    T.ty = mid + hm / 2 - T.tw * 0.85f;
    T.hb = (box){b.x, b.y + hs + hm, b.w, b.h - hs - hm};
}

/* the viewer's hand shown sorted (rank, then suit): order[i], the index in the hand of the i-th card shown */
static int shown(const njpos *p, int pl, uint8_t *order)
{
    int n = p->n[pl];
    for (int i = 0; i < n; i++) order[i] = (uint8_t)i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--) {
            int a = p->hand[pl][order[j - 1]], c = p->hand[pl][order[j]];
            if (CARD_RANK(a) * 4 + CARD_SUIT(a) <= CARD_RANK(c) * 4 + CARD_SUIT(c)) break;
            uint8_t t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    return n;
}

static float hand_spots(int n, cards_spot *s)
{
    float wmax = T.b.w > T.b.h * 1.05f ? 1.5f * ui.unit : fminf(T.b.w * 0.2f, 1.9f * ui.unit);
    return ct_hand(T.hb, n, 1.4f, wmax, s);
}

static int place_of(const njpos *p, int pl, int card, float *x, float *y, float *w, float *deg)
{
    uint8_t order[NJ_HAND];
    cards_spot s[NJ_HAND];
    int n = shown(p, pl, order);
    *w = hand_spots(n, s);
    for (int i = 0; i < n; i++)
        if (p->hand[pl][order[i]] == card) {
            *x = s[i].x, *y = s[i].y, *deg = s[i].deg;
            return 1;
        }
    return 0;
}

/* the i-th card of the sequence on show (of n shown) */
static void run_at(int i, float *x, float *y) { *x = T.rx + i * 0.3f * T.rw; *y = T.ry; }

static int run_first(const njpos *p)           /* the first card of the sequence on show */
{
    int f = p->seq, n = p->nplayed - f;
    return n > RUN_SHOWN ? p->nplayed - RUN_SHOWN : f;
}

/* where a player sits on the screen: the viewer under their hand's middle, the others at their seats */
static void seat_of(int q, float *x, float *y)
{
    if (q == T.me) *x = T.b.x + T.b.w / 2, *y = T.hb.y + T.hb.h * 0.55f;
    else *x = seat_x(q), *y = T.sy;
}

static void well_at(int k, float *x, float *y, float *r)
{
    *x = T.bx + WELL[k][0] * T.B;
    *y = T.by + WELL[k][1] * T.B;
    *r = WELL[k][2] * T.B;
}

/* ---- the flying chips -------------------------------------------------------------------------------------------- */
#define FLIGHTS 48
typedef struct { float x0, y0, x1, y1; int n; Uint32 t0; } flight;
static flight fly[FLIGHTS];
static int nfly;

static void fly_chips(float x0, float y0, float x1, float y1, int n, int delay)
{
    if (n <= 0 || nfly >= FLIGHTS) return;
    n = n > 8 ? 8 : n;                          /* a handful shows many */
    fly[nfly++] = (flight){x0, y0, x1, y1, n, ui_ms() + (Uint32)delay};
    ct_until(&MV, ui_ms() + (Uint32)(delay + FLY_MS + FLY_GAP * (n - 1) + MV.rest));
    ui_sound_later(SND_PLACE, delay + FLY_MS - 30);
}

static int flying(void)
{
    for (int i = 0; i < nfly; i++)
        if ((int)(ui_ms() - fly[i].t0) < FLY_MS + FLY_GAP * (fly[i].n - 1)) return 1;
    return 0;
}

static void calm(void)
{
    ct_calm(&MV);
    nfly = 0;
}

static int busy(void) { return ct_busy(&MV) || flying(); }

/* ---- the moves on the screen ------------------------------------------------------------------------------------- */
static void deal_slides(void)
{
    const njpos *p = &G.pos;
    float dx, dy;
    seat_of(p->dealer, &dx, &dy);
    int k = 0, delay = 0;
    for (int q = 0; q < p->players; q++) {      /* the stakes: a chip from each player into each well */
        float x, y, wx, wy, wr;
        seat_of(q, &x, &y);
        for (int w = 0; w < NJ_BOXES; w++) {
            well_at(w, &wx, &wy, &wr);
            fly_chips(x, y, wx, wy + 0.3f * wr, 1, 60 * q + 40 * w);
        }
    }
    delay = 60 * p->players + 300;
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) {
            uint8_t order[NJ_HAND];
            cards_spot s[NJ_HAND];
            int n = shown(p, q, order);
            float hw = hand_spots(n, s);
            for (int i = 0; i < n; i++, k++)
                ct_slide(&MV, DECK_FRENCH, p->hand[q][order[i]], dx, dy, T.sw, 0, 0, s[i].x, s[i].y, hw, s[i].deg, 1,
                         DEAL_MS + 80, delay + k * 28, H_HAND, q, p->hand[q][order[i]], k % 4 ? -1 : SND_MOVE);
        } else {
            for (int i = 0; i < p->n[q] && i < 14; i++, k++) {
                float x, y, a;
                ct_backs_end(DECK_FRENCH, seat_x(q), T.sy, T.sfan, T.sw, i + 1, &x, &y, &a);
                ct_slide(&MV, DECK_FRENCH, CARD_NONE, dx, dy, T.sw, 0, 0, x, y, T.sw, a, 0, DEAL_MS, delay + k * 28,
                         H_SEAT, q, -1, k % 4 ? -1 : SND_MOVE);
            }
        }
    }
}

/* a card played: from the hand (or the seat) to the sequence; a well's chips to the player; the payments at the end */
static void animate(const njpos *b, int act)
{
    const njpos *p = &G.pos;
    if (act == NJ_NEXT) return;
    int pl = b->turn, i = p->nplayed - 1 - run_first(p);
    float x0, y0, w0 = T.sw, a0 = 0, x1, y1;
    int up0 = 0;
    if (pl == T.me && place_of(b, pl, act, &x0, &y0, &w0, &a0)) up0 = 1;
    else seat_of(pl, &x0, &y0);
    run_at(i, &x1, &y1);
    ct_slide(&MV, DECK_FRENCH, act, x0, y0, w0, a0, up0, x1, y1, T.rw, 0, 1, SLIDE_MS, 0, H_RUN, 0, act, SND_PLACE);
    int k = nj_box_of(act), t = SLIDE_MS;
    float px, py, wx, wy, wr;
    seat_of(pl, &px, &py);
    if (k >= 0 && b->box[k] > 0) {
        well_at(k, &wx, &wy, &wr);
        fly_chips(wx, wy + 0.3f * wr, px, py, b->box[k], t);
        t += FLY_MS + 100;
    }
    if (p->phase != NP_DEALT) return;
    for (int o = 0; o < p->players; o++) {      /* the payments: to whoever went out; the stakes again to the wells */
        if (o == p->out) continue;
        float ox, oy;
        seat_of(o, &ox, &oy);
        fly_chips(ox, oy, px, py, -p->got_cards[o], t + 120 * o);
        for (int c = 0; c < b->n[o]; c++) {
            int w = nj_box_of(b->hand[o][c]);
            if (w < 0) continue;
            well_at(w, &wx, &wy, &wr);
            fly_chips(ox, oy, wx, wy + 0.3f * wr, w + 1, t + 400 + 120 * o);
        }
    }
    if (p->opera)                               /* grand opera: every well to the winner */
        for (int w = 0; w < NJ_BOXES; w++) {
            well_at(w, &wx, &wy, &wr);
            fly_chips(wx, wy + 0.3f * wr, px, py, 4, t + 900 + 80 * w);
        }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    variant = v == NJ_NO_OPERA ? NJ_NO_OPERA : NJ_CLASSIC;
    nj_new(&G, variant, ui.players, (uint32_t)ui_random(1 << 30));
    calm();
    dealing = 1;
}

static int status(void)
{
    if (G.pos.phase != NP_OVER) return GS_PLAY;
    unsigned w = nj_winners(&G.pos);
    int n = 0, first = -1;
    for (int q = 0; q < G.pos.players; q++)
        if (w >> q & 1) n++, first = first < 0 ? q : first;
    return n == 1 ? gs_win(first) : GS_DRAW;
}

static unsigned winners(void) { return nj_winners(&G.pos); }
static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    njpos before = G.pos;
    if (!nj_play(&G, a)) { ui_sound(SND_DENY); return; }
    if (a == NJ_NEXT) {
        calm();
        if (G.pos.phase == NP_PLAY) {
            dealing = 1;
            ui_handoff(G.pos.turn);
        }
        return;
    }
    calm();
    animate(&before, a);
    if (G.pos.phase == NP_PLAY) ui_handoff(G.pos.turn);   /* after the card lands */
}

static int undo(void)
{
    if (!nj_undo(&G)) return 0;
    calm();
    dealing = 0;
    ui_handoff(G.pos.turn);
    return 1;
}

static int script(const char *s)
{
    int ok = nj_script(&G, s);
    calm();
    dealing = 0;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* the board: the plank in its frame, five round wells sunk in it (the middle one in a brass ring) */
static void board_pic(int n, uint8_t *o)
{
    int fr = (int)(0.035f * n);
    art_board_n(1, 1, n - 2 * fr, fr, 0, o);
    for (int k = 0; k < NJ_BOXES; k++) {
        float cx = WELL[k][0] * n, cy = WELL[k][1] * n, R = WELL[k][2] * n, rim = fmaxf(2, 0.12f * R);
        int brass = k == NJ_SEVEN;
        for (int y = (int)(cy - R - 2); y <= (int)(cy + R + 2); y++)
            for (int x = (int)(cx - R - 2); x <= (int)(cx + R + 2); x++) {
                if (x < 0 || y < 0 || x >= n || y >= n) continue;
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = hypotf(dx, dy), in = clampf(R - d + 0.5f, 0, 1);
                if (in <= 0) continue;
                uint8_t *p = o + 4 * ((size_t)y * n + x);
                float c[3];
                float lit = -(dx + dy) / (1.414f * R);  /* -1..1: the upper left lit */
                if (d > R - rim) {              /* the rim: lit on the lower right (the light from the upper left
                                                 * falls into the well) */
                    float k2 = 1 - 0.25f * lit;
                    if (brass) c[0] = 214 * k2, c[1] = 168 * k2, c[2] = 74 * k2;
                    else c[0] = p[0] * 0.78f * k2, c[1] = p[1] * 0.74f * k2, c[2] = p[2] * 0.70f * k2;
                } else {                        /* the hollow: darker, its upper left in shadow */
                    float e = (R - rim - d) / (R - rim), sh = 0.55f + 0.25f * e + 0.15f * (-lit);
                    sh = clampf(sh, 0.4f, 0.95f);
                    c[0] = p[0] * sh * 0.86f, c[1] = p[1] * sh * 0.78f, c[2] = p[2] * sh * 0.72f;
                }
                for (int ch = 0; ch < 3; ch++) p[ch] = (uint8_t)clampf(p[ch] + (c[ch] - p[ch]) * in, 0, 255);
            }
    }
}

static SDL_Texture *t_board, *t_chip, *t_chip_bar;
static int board_made, chip_made, chip_bar_made, tok_made;

static SDL_Texture *chip_tex(SDL_Texture **t, int *made, int size)
{
    if (size < 4) return NULL;
    if (*made != size || !*t) {
        *made = size;
        ui_free(t);
        cup_chip_pic(size, ui_buf(size, size));
        *t = ui_tex(size, size, ui_buf(size, size));
    }
    return *t;
}

static void textures(void)
{
    int n = (int)T.B;
    if (board_made != n && n > 8) {
        board_made = n;
        ui_free(&t_board);
        board_pic(n, ui_buf(n, n));
        t_board = ui_tex(n, n, ui_buf(n, n));
    }
    chip_tex(&t_chip, &chip_made, (int)(0.075f * T.B));
    tok_made = (int)fmaxf(8, 0.5f * T.sw);
}

static void chip_at(float x, float y, float d)
{
    if (!t_chip) return;
    ui_shadow_flat(x, y, d, d, 0.55f);
    ui_put_c(t_chip, x, y, d, d);
}

/* a well: its card lying in it, the chips staked on it and how many */
static void draw_well(int k, int lit)
{
    float cx, cy, r, d = 0.075f * T.B;
    well_at(k, &cx, &cy, &r);
    if (lit) ui_mark_gold(cx, cy, 2.4f * r);
    cards_draw(DECK_FRENCH, nj_box_card[k], cx, cy - 0.12f * r, 0.62f * r, k == NJ_SEVEN ? 0 : (k & 1 ? 8 : -8),
               CD_FLAT);
    int n = G.pos.box[k], shown_n = n > 14 ? 14 : n;
    for (int i = 0; i < shown_n; i++) {         /* a heap on the card's lower half */
        float a = art_hash(i, k + 7) * 2 * ART_PI, rr = 0.36f * r * sqrtf((i + 0.5f) / 14);
        chip_at(cx + rr * cosf(a), cy + 0.36f * r + 0.6f * rr * sinf(a), d);
    }
    if (n) ui_number(cx + 0.62f * r, cy + 0.62f * r, 0.3f * r, n, 1);
}

/* the deal's summary: a row a player, their token, the cards paid (the one who went out: received, in gold), the chips
 * from the wells (taken, less the stakes paid again), their chips now; over the columns: a card back, a well's card,
 * a chip */
static void draw_summary(void)
{
    const njpos *p = &G.pos;
    box b = T.b;
    float u = ui.unit, rh = fminf(1.0f * u, b.h * 0.7f / (p->players + 1)), pw = fminf(b.w * 0.94f, 7.8f * u);
    float ph = rh * (p->players + 1) + 0.4f * u, x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 70);
    ct_panel(x0, y0, pw, ph, 255);
    float c1 = x0 + pw * 0.36f, c2 = x0 + pw * 0.6f, c3 = x0 + pw * 0.84f, hy = y0 + 0.2f * u + rh / 2;
    float cw = 0.42f * rh;                      /* the column heads */
    cards_draw(DECK_FRENCH, CARD_NONE, c1, hy, cw, 0, CD_DOWN);
    cards_draw(DECK_FRENCH, nj_box_card[NJ_SEVEN], c2, hy, cw, 0, 0);
    chip_tex(&t_chip_bar, &chip_bar_made, (int)(0.6f * rh));
    if (t_chip_bar) ui_put_c(t_chip_bar, c3, hy, 0.6f * rh, 0.6f * rh);
    tok_made = (int)(0.6f * u);
    for (int q = 0; q < p->players; q++) {
        float cy = hy + (q + 1) * rh, tx = x0 + 0.7f * u, d = fminf(0.7f * rh, 0.8f * u), nh = 0.42f * rh;
        if (q == p->out) ui_mark_gold(tx, cy, 2.0f * d);
        ui_put_c(ct_token(q, tok_made), tx, cy, d, d);
        ui_number_signed(c1, cy, nh, p->got_cards[q], 1, 1);
        int wells = p->got_boxes[q] + p->paid_again[q];
        if (q == p->out && p->opera) ui_mark_gold(c2, cy, 1.8f * nh);
        ui_number_signed(c2, cy, nh, wells, 1, 1);
        ui_number(c3, cy, 0.52f * rh, p->chips[q], 1);
        ct_rule(x0 + 0.3f * u, cy - rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    const njpos *p = &G.pos;
    textures();
    if (dealing && ui.viewer >= 0) { dealing = 0; calm(); deal_slides(); }   /* the cover lifted: the deal flies */
    int mine = p->turn == T.me && !ui_handing() && p->phase == NP_PLAY, moving = busy();
    /* the board and its wells: the well of the card just played lit */
    ui_put(t_board, T.bx, T.by, T.B, T.B);
    int last = p->nplayed ? nj_box_of(p->played[p->nplayed - 1]) : -1;
    for (int k = 0; k < NJ_BOXES; k++) draw_well(k, k == last && p->phase == NP_PLAY);
    /* the sequence being played, the talon */
    int f = run_first(p), n = p->nplayed - f;
    for (int i = 0; i < n; i++) {
        int c = p->played[f + i];
        float x, y;
        if (ct_hides(&MV, H_RUN, 0, c)) continue;
        run_at(i, &x, &y);
        cards_draw(DECK_FRENCH, c, x, y, T.rw, 0, p->phase == NP_PLAY && !p->need ? CD_DIM : 0);   /* over: dimmed */
    }
    cards_pile(DECK_FRENCH, T.tx, T.ty, T.tw, p->ntalon, CARD_NONE, 0);
    /* the others' hands at their seats */
    for (int q = 0; q < p->players; q++) {
        if (q == T.me) continue;
        float x = seat_x(q), ch = T.sw * 1.4f;
        int k = p->n[q] - ct_hides(&MV, H_SEAT, q, -1);
        if (q == p->turn && p->phase == NP_PLAY) ui_mark_gold(x, T.sy, 2.2f * T.sw);
        ct_backs(DECK_FRENCH, x, T.sy, T.sfan, T.sw, k, 0);
        if (!k) cards_place(DECK_FRENCH, x, T.sy, T.sw);
        ct_label(ct_token(q, tok_made), x, T.sy + 0.5f * ch + 0.32f * T.sw + 0.12f * ch, 0.48f * T.sw, p->n[q]);
    }
    /* the hand, sorted: the cards that can be played glow (under the hand-off cover: face down) */
    uint8_t order[NJ_HAND];
    cards_spot s[NJ_HAND];
    int hn = shown(p, T.me, order);
    float w = hand_spots(hn, s);
    for (int i = 0; i < hn; i++) {
        int c = p->hand[T.me][order[i]];
        if (ct_hides(&MV, H_HAND, T.me, c)) continue;
        if (ui.viewer < 0) { cards_draw(DECK_FRENCH, CARD_NONE, s[i].x, s[i].y, w, s[i].deg, CD_DOWN); continue; }
        int how = mine && !moving && nj_legal(p, c) ? CD_GLOW : 0;
        cards_draw(DECK_FRENCH, c, s[i].x, s[i].y, w, s[i].deg, how);
    }
    ct_draw(&MV);
    /* the chips in the air */
    Uint32 now = ui_ms();
    int any = 0;
    for (int i = 0; i < nfly; i++)
        for (int k = 0; k < fly[i].n; k++) {
            float t = ((float)(int)(now - fly[i].t0) - FLY_GAP * k) / FLY_MS;
            if (t < 0) { any = 1; continue; }
            if (t >= 1) continue;
            any = 1;
            float x = cup_mix(fly[i].x0, fly[i].x1, t), y = cup_mix(fly[i].y0, fly[i].y1, t);
            chip_at(x, y - 0.08f * T.B * sinf(ART_PI * t), 0.075f * T.B * (1 + 0.25f * sinf(ART_PI * t)));
        }
    if (any || moving) ui_animate();
    if (p->phase != NP_PLAY && !moving) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    const njpos *p = &G.pos;
    if (busy()) return;
    if (p->phase == NP_DEALT) { act(NJ_NEXT); return; }
    if (p->phase != NP_PLAY) return;
    if (p->turn != T.me) { ui_sound(SND_DENY); return; }
    uint8_t order[NJ_HAND];
    cards_spot s[NJ_HAND];
    int n = shown(p, T.me, order);
    float w = hand_spots(n, s);
    int i = cards_hit(s, n, w, 1.4f, x, y);
    if (i >= 0 && nj_legal(p, p->hand[T.me][order[i]])) act(p->hand[T.me][order[i]]);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static SDL_Texture *t_bar_chip;
static int bar_chip_made;

static void bar(int player, box b)              /* the player's chips (a narrow bar: the number alone) */
{
    int v = G.pos.chips[player], digits = v >= 100 ? 3 : v >= 10 ? 2 : 1;
    if (b.w >= 0.75f * ui.unit + 0.35f * ui.unit * digits + 0.3f * ui.unit) {
        ui_bar_count(b, chip_tex(&t_bar_chip, &bar_chip_made, (int)(0.6f * ui.unit)), v);
        return;
    }
    float w = 0.74f * (digits - 1) + 0.62f, h = fminf(0.46f * ui.unit, b.w / w * 0.92f);
    ui_number(b.x + w * h / 2, b.y + b.h / 2, h, v, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card and the picker's: the board, its five cards in their wells, chips on them (no grand opera: the
 * middle well empty of chips) */
static void thumb(int v, int size, uint8_t *rgba)
{
    board_pic(size, rgba);
    int cw = (int)(0.11f * size), chh = (int)(cw * 1.4f), cd = (int)(0.07f * size);
    uint8_t *b = malloc((size_t)cw * chh * 4), *c = malloc((size_t)cd * cd * 4);
    if (!b || !c) { free(b), free(c); return; }
    cup_chip_pic(cd, c);
    for (int k = 0; k < NJ_BOXES; k++) {
        float cx = WELL[k][0] * size, cy = WELL[k][1] * size, r = WELL[k][2] * size;
        cards_paint(DECK_FRENCH, nj_box_card[k], cw, chh, b);
        art_blend(rgba, size, size, b, cw, chh, (int)(cx - cw / 2.0f), (int)(cy - 0.12f * r - chh / 2.0f));
        int chips = v == NJ_NO_OPERA && k == NJ_SEVEN ? 0 : k + 2;
        for (int i = 0; i < chips; i++) {
            float a = art_hash(i, k + 7) * 2 * ART_PI, rr = 0.36f * r * sqrtf((i + 0.5f) / 7);
            art_blend(rgba, size, size, c, cd, cd, (int)(cx + rr * cosf(a) - cd / 2.0f),
                      (int)(cy + 0.36f * r + 0.6f * rr * sinf(a) - cd / 2.0f));
        }
    }
    free(b);
    free(c);
}

static const game nainjaune_game = {
    .id = "nainjaune", .order = 303, .variants = NJ_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6, .view = VIEW_HIDDEN, .players_usual = 4,
};
REGISTER_GAME(nainjaune_game)
