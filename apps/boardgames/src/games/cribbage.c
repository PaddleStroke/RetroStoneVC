/*
 * Cribbage in RetroStone Board: 2 (or 3) players passing the device round (the rules: cribbage_rules.c, cribbage.h).
 * Each hand is secret (VIEW_HIDDEN): the cover comes up whenever the device must pass. The table, from the top: the
 * others' hands (face down) and the pack with the starter on it; the peg board, the star: the house wood with a lane
 * of two rows of holes a player (out on one, back on the other, twice round, then the game hole), each player's two
 * pegs leapfrogging as they score; the crib, the cards of the count in a row and the count; the viewer's hand.
 * Taps: choose the cards for the crib, then tap the crib; tap the pack to cut; choose a card, tap it again (or drag
 * it to the table) to lay it. The show: each hand and the crib with the starter on a stage, every combination lit in
 * turn with its points rising from it, then the peg leaps; tap to go on. No words: cards, pegs, tokens, digits.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/cardmoves.h"
#include "games/cribbage.h"
#include "shell.h"

#define SLIDE_MS 320
#define FLIP_MS 260
#define DEAL_MS 240
#define LEAP_MS 460
#define EV_MS 380                               /* between the points of one card (15, a pair, a run) */
#define ITEM_MS 720                             /* the show: one combination lit */

static cgame G;
static int dealing, floor_n;                    /* floor_n: undo stops there (the last hand-off) */
static struct { int on, slot, moved; float x0, y0, x, y; } drag;   /* a card of the hand under the finger */
static unsigned sel;                            /* the cards chosen: the hand's slots (the crib), a kept slot (the
                                                 * play) */
static const int deck = DECK_FRENCH;

/* ---- the table's geometry (it follows the viewer) ---------------------------------------------------------------- */
static struct {
    box b;
    int me, land;
    float ow, oy, ox[CRIB_MAXP];                /* the others' hands: card width, centres */
    float sx, sy;                               /* the pack (and the starter on it) */
    float hw, hx, hy, hwide;                    /* the viewer's hand: card width, centre, the room for it */
    float pw, kx, ky, rx0, rx1;                 /* the play: card width, the crib, the row's room */
    float gw, gx, gy;                           /* the show's stage: card width, centre */
    float pz0, pz1;                             /* the play zone */
    int pitch, fr, bwpx, bhpx;                  /* the peg board: hole pitch, frame, size */
    float bx, by;
} T;

static float aspect(void) { return cards_aspect(deck); }

static void layout(box b)
{
    const cpos *p = &G.pos;
    int n = p->players;
    float u = ui.unit;
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= n) T.me = 0;
    T.land = b.w > b.h * 1.05f;
    float ho = b.h * (T.land ? 0.15f : 0.14f), hh = b.h * (T.land ? 0.34f : 0.28f);
    /* the others and the pack, at the top */
    T.ow = floorf(fminf(b.w / n * 0.8f / 3.5f, ho * 0.84f / aspect()));
    T.oy = b.y + ho / 2;
    T.sx = b.x + b.w - 0.75f * T.ow;
    T.sy = T.oy;
    float room = b.w - 1.6f * T.ow;
    for (int i = 0; i < n - 1; i++) T.ox[(T.me + 1 + i) % n] = b.x + room * (i + 0.5f) / (n - 1);
    /* the peg board */
    int ny = n == 3 ? 11 : 7;                   /* the board's height in holes: a lane a player */
    T.pitch = (int)floorf(fminf(b.w * 0.99f / 36.9f, b.h * (T.land ? 0.27f : 0.2f) / (ny + 0.9f)));
    if (T.pitch < 4) T.pitch = 4;
    T.fr = (int)(0.45f * T.pitch);
    T.bwpx = 36 * T.pitch + 2 * T.fr;
    T.bhpx = ny * T.pitch + 2 * T.fr;
    T.bx = floorf(b.x + (b.w - T.bwpx) / 2);
    T.by = floorf(b.y + ho + 0.04f * u);
    /* the hand */
    T.hw = floorf(fminf(b.w * 0.97f / 6.3f, hh * 0.8f / aspect()));
    T.hx = b.x + b.w / 2;
    T.hy = b.y + b.h - hh / 2 + 0.05f * hh;
    T.hwide = b.w * 0.97f;
    /* the play: the crib on the left, the row of the count */
    T.pz0 = T.by + T.bhpx + 0.1f * u;
    T.pz1 = b.y + b.h - hh;
    float ph = T.pz1 - T.pz0;
    T.pw = floorf(fminf(b.w / 6.4f, ph * 0.66f / aspect()));
    T.kx = b.x + 0.62f * T.pw + 0.1f * u;
    T.ky = T.pz0 + ph / 2;
    T.rx0 = T.kx + 0.62f * T.pw + 0.25f * u;
    T.rx1 = b.x + b.w - 1.0f * u;
    /* the stage */
    T.gw = floorf(fminf(b.w * 0.92f / 5.4f, ph * 0.64f / aspect()));
    T.gx = b.x + b.w / 2;
    T.gy = T.pz0 + ph * 0.6f;
}

/* ---- the peg board ----------------------------------------------------------------------------------------------- */
/* the lane of a player: the viewer's at the bottom, the others above in turn order */
static int lane_of(int pl) { return (pl - T.me + G.pos.players) % G.pos.players; }

/* a hole's centre in the board's picture: lane (0 at the bottom), row (0 the lower: out; 1 the upper: back), col */
static void hole_local(int lane, int row, int col, float *x, float *y)
{
    float P = (float)T.pitch;
    int i = G.pos.players - 1 - lane;
    *x = T.fr + 3.0f * P + (col + 0.6f * (col / 5)) * P;
    float lh = (float)(T.bhpx - 2 * T.fr) / P / G.pos.players;   /* a lane's height, in holes */
    *y = T.fr + (i + (row ? 0.29f : 0.71f)) * lh * P;
}

/* where a peg stands for a score: 0 the start holes (one a peg), 1..120 out and back twice, 121 the game hole */
static void peg_xy(int pl, int pos, int k, float *x, float *y)
{
    float P = (float)T.pitch;
    int lane = lane_of(pl);
    if (pos <= 0) {
        hole_local(lane, k, 0, x, y);
        *x = T.fr + 1.8f * P;
    } else if (pos >= CRIB_GOAL) {
        *x = T.fr + 0.8f * P;
        *y = T.bhpx / 2.0f;
    } else {
        int t = (pos - 1) % 60;
        hole_local(lane, t < 30 ? 0 : 1, t < 30 ? t : 59 - t, x, y);
    }
    *x += T.bx;
    *y += T.by;
}

/* a hole bored in the wood: dark inside (its far wall lit), its rim bevelled */
static void bore(uint8_t *o, int w, int h, float cx, float cy, float R)
{
    float rim = fmaxf(1.2f, 0.35f * R);
    for (int y = (int)(cy - R - rim - 1); y <= (int)(cy + R + rim + 1); y++)
        for (int x = (int)(cx - R - rim - 1); x <= (int)(cx + R + rim + 1); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx * dx + dy * dy);
            if (d > R + rim) continue;
            uint8_t *q = o + 4 * (y * w + x);
            float side = (dx + dy) / (d * 1.4142f + 1e-3f);
            float in = clampf(R - d + 0.5f, 0, 1);
            float k_rim = 1 - 0.22f * side * (1 - (d - R) / rim), k_in = 0.16f + 0.16f * (side * 0.5f + 0.5f);
            float k = in * k_in + (1 - in) * k_rim;
            for (int c = 0; c < 3; c++) q[c] = (uint8_t)clampf(q[c] * k, 0, 255);
        }
}

static SDL_Texture *t_board;
static int board_pitch, board_n;

static void make_board(void)
{
    int n = G.pos.players, P = T.pitch, w = T.bwpx, h = T.bhpx;
    if (t_board && board_pitch == P && board_n == n) return;
    board_pitch = P;
    board_n = n;
    uint8_t *o = ui_buf(w, h);
    int ny = (h - 2 * T.fr) / P;
    art_board_n(36, ny, P, T.fr, 0, o);
    for (int l = 1; l < n; l++) {               /* an inlay between the lanes: a line of dark wood */
        int ym = T.fr + l * ny * P / n, y0 = ym - (int)fmaxf(1, 0.05f * P), y1 = ym + (int)fmaxf(1, 0.05f * P);
        for (int y = y0; y <= y1; y++)
            for (int x = T.fr + (int)(2.6f * P); x < w - T.fr - (int)(0.4f * P); x++) {
                uint8_t *q = o + 4 * (y * w + x);
                for (int c = 0; c < 3; c++) q[c] = (uint8_t)(q[c] * 0.55f);
            }
    }
    float R = fmaxf(1.6f, 0.22f * P);
    for (int l = 0; l < n; l++) {
        for (int r = 0; r < 2; r++) {
            for (int c = 0; c < 30; c++) {
                float x, y;
                hole_local(l, r, c, &x, &y);
                bore(o, w, h, x, y, R);
            }
            float x, y;
            hole_local(l, r, 0, &x, &y);
            bore(o, w, h, T.fr + 1.8f * P, y, R);   /* the start holes */
        }
    }
    float gx = T.fr + 0.8f * P, gy = h / 2.0f;  /* the game hole: bigger, in a ring of brass */
    for (int y = (int)(gy - 0.7f * P); y <= (int)(gy + 0.7f * P); y++)
        for (int x = (int)(gx - 0.7f * P); x <= (int)(gx + 0.7f * P); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float d = hypotf(x + 0.5f - gx, y + 0.5f - gy), a = clampf(0.62f * P - d + 0.5f, 0, 1) *
                                                                clampf(d - 0.42f * P + 0.5f, 0, 1);
            uint8_t *q = o + 4 * (y * w + x);
            static const float brass[3] = {214, 168, 74};
            for (int c = 0; c < 3; c++) q[c] = (uint8_t)(q[c] * (1 - a) + brass[c] * a);
        }
    bore(o, w, h, gx, gy, R * 1.3f);
    ui_free(&t_board);
    t_board = ui_tex(w, h, o);
}

/* the pegs: two a player, each at a score; a peg leaping draws on its way */
static int pegs[CRIB_MAXP][2];
#define LEAPS 8
typedef struct { int pl, k, from, to; Uint32 t0; } leaping;
static leaping lp[LEAPS];
static int nlp;

static void pegs_sync(void)
{
    const cpos *p = &G.pos;
    for (int q = 0; q < p->players; q++) {
        int a = pegs[q][0], b = pegs[q][1], f = p->score[q], k = p->back[q];
        if (!((a == f && b == k) || (a == k && b == f))) pegs[q][0] = k, pegs[q][1] = f;
    }
}

static SDL_Texture *t_peg[CRIB_MAXP];
static int peg_size;

static void draw_board(void)
{
    const cpos *p = &G.pos;
    make_board();
    ui_put(t_board, T.bx, T.by, (float)T.bwpx, (float)T.bhpx);
    int ps = (int)(0.96f * T.pitch);
    if (ps != peg_size) {
        peg_size = ps;
        for (int q = 0; q < CRIB_MAXP; q++) {
            ui_free(&t_peg[q]);
            art_counter(COUNTER_STONE, q, ps, ui_buf(ps, ps));
            t_peg[q] = ui_tex(ps, ps, ui_buf(ps, ps));
        }
    }
    Uint32 now = ui_ms();
    for (int pass = 0; pass < 2; pass++)        /* the pegs at rest, then the ones in the air */
        for (int q = 0; q < p->players; q++)
            for (int k = 0; k < 2; k++) {
                int pos = pegs[q][k], flying = 0;
                float t = 0;
                int from = pos, to = pos;
                for (int i = nlp - 1; i >= 0; i--) {   /* this peg's leaps: before the first, at its start */
                    if (lp[i].pl != q || lp[i].k != k) continue;
                    if (now < lp[i].t0) { pos = lp[i].from; continue; }
                    if (now < lp[i].t0 + LEAP_MS) {
                        flying = 1;
                        from = lp[i].from, to = lp[i].to;
                        t = (float)(now - lp[i].t0) / LEAP_MS;
                    }
                    break;
                }
                if (flying != pass) continue;
                float x, y;
                if (!flying) {
                    peg_xy(q, pos, k, &x, &y);
                    ui_shadow_flat(x, y, (float)ps, (float)ps, 0.55f);
                    ui_put_c(t_peg[q], x, y, (float)ps, (float)ps);
                    continue;
                }
                float x0, y0, x1, y1, e = t * t * (3 - 2 * t), lift = sinf(t * 3.14159f);
                peg_xy(q, from, k, &x0, &y0);
                peg_xy(q, to, k, &x1, &y1);
                x = x0 + (x1 - x0) * e;
                y = y0 + (y1 - y0) * e;
                float s = ps * (1 + 0.55f * lift), off = lift * 0.5f * T.pitch;
                ui_shadow_flat(x + off, y + off, ps * 1.1f, ps * 1.1f, 0.45f * (1 - 0.4f * lift));
                ui_put_c(t_peg[q], x, y - 0.3f * off, s, s);
                ui_animate();
            }
}

/* ---- the moving things: cards, floating points, leaps; what the last action showed ---------------------------- */
enum { H_NONE, H_ROW, H_HAND, H_CRIB, H_STARTER, H_STAGE };
static cm_table M;
static struct {
    int on;
    cpos b;                                     /* the position before the action */
    Uint32 reset, show, items;                  /* the count's cards leave; the stage comes; its first item lit */
    int hl_n[4];                                /* the row's last cards lit (a card's points), from t to t + EV_MS */
    Uint32 hl_t[4];
    int nhl;
    Uint32 heels;                               /* the starter lit (his heels) */
} A;

static void calm(void)
{
    cm_calm(&M);
    nlp = 0;
    memset(&A, 0, sizeof A);
}

static int busy(void) { return cm_busy(&M); }

static void slide(int card, int up0, float x0, float y0, float w0, int up1, float x1, float y1, float w1, int ms,
                  int delay, int hide, int pl, int s, int sound)
{
    cm_slide(&M, deck, card, up0, x0, y0, w0, up1, x1, y1, w1, ms, delay, hide, pl, s, sound);
}

static int hides(int kind, int pl, int s) { return cm_hides(&M, kind, pl, s); }

static void float_at(float x, float y, int value, int badge, Uint32 t0) { cm_float_at(&M, x, y, value, badge, t0); }

/* a leap of the peg behind the player's front one */
static void leap(int pl, int from, int to, Uint32 t0)
{
    int k = pegs[pl][0] == from ? 1 : 0;
    if (pegs[pl][0] == from && pegs[pl][1] == from) k = 1;
    if (nlp < LEAPS) lp[nlp++] = (leaping){pl, k, pegs[pl][k], to, t0};
    pegs[pl][k] = to;
    cm_until(&M, t0 + LEAP_MS + CM_REST);
    Uint32 now = ui_ms();
    ui_sound_later(SND_PLACE, (int)(t0 + LEAP_MS - 60 > now ? t0 + LEAP_MS - 60 - now : 0));
}

/* ---- where the cards are ----------------------------------------------------------------------------------------- */
static int my_cards(const cpos *p, int pl, int *slots)   /* the cards in hand: slots of hand[] (the crib) or kept[] */
{
    int n = 0;
    if (p->phase == CP_DISCARD && p->nhand[pl]) {
        for (int s = 0; s < p->nhand[pl]; s++) slots[n++] = s;
    } else
        for (int s = 0; s < 4; s++)
            if (p->kept[pl][s] != CARD_NONE && !(p->used[pl] >> s & 1)) slots[n++] = s;
    return n;
}

static int in_crib_phase(const cpos *p, int pl) { return p->phase == CP_DISCARD && p->nhand[pl]; }

static int card_of(const cpos *p, int pl, int s) { return in_crib_phase(p, pl) ? p->hand[pl][s] : p->kept[pl][s]; }

static void hand_spots(int n, cards_spot *sp) { cards_fan(T.hx, T.hy, T.hwide, T.hw, aspect(), n, 6, sp); }

/* where a card of a hand is: the viewer's (big), another's (small) */
static void hand_card_at(const cpos *p, int pl, int s, float *x, float *y, float *w)
{
    int slots[6], n = my_cards(p, pl, slots), i = 0;
    while (i < n && slots[i] != s) i++;
    if (i == n) i = n > 0 ? n - 1 : 0;
    if (pl == T.me) {
        cards_spot sp[6];
        hand_spots(n, sp);
        *x = sp[i].x;
        *y = sp[i].y;
        *w = T.hw;
    } else {
        float step = 0.5f * T.ow;
        *x = T.ox[pl] + (i - (n - 1) / 2.0f) * step;
        *y = T.oy;
        *w = T.ow;
    }
}

static float row_step(void)
{
    int most = G.pos.players == 3 ? 12 : 8;
    return fminf(0.62f * T.pw, (T.rx1 - T.rx0 - T.pw) / (most - 1));
}

static void row_at(int i, float *x, float *y)
{
    *x = T.rx0 + T.pw / 2 + i * row_step();
    *y = T.ky;
}

/* the stage: the four cards and the starter (index 4), a gap before it */
static void stage_at(int i, float *x, float *y)
{
    float step = 1.06f * T.gw, w = 4 * step + 0.3f * T.gw + T.gw;
    *x = T.gx - w / 2 + T.gw / 2 + i * step + (i == 4 ? 0.3f * T.gw : 0);
    *y = T.gy;
}

/* ---- the action's pictures --------------------------------------------------------------------------------------- */
static void deal_slides(void)
{
    const cpos *p = &G.pos;
    int k = 0;
    for (int c = 0; c < p->ndeal; c++)
        for (int i = 1; i <= p->players; i++, k++) {
            int q = (p->dealer + i) % p->players;
            float x, y, w;
            hand_card_at(p, q, c, &x, &y, &w);
            slide(p->hand[q][c], 0, T.sx, T.sy, T.ow, 0, x, y, w, DEAL_MS, k * 70, H_HAND, q, c,
                  k % 3 == 0 ? SND_MOVE : -1);
        }
}

/* the points of a card (or a go), and their leap */
static Uint32 play_events(const cpos *p, Uint32 t, int leap_i, int row_n)
{
    int any = 0;
    for (int i = 0; i < p->nev; i++) {
        const crib_ev *e = &p->ev[i];
        if (e->leap != leap_i || e->src == CS_SHOW) continue;
        float x, y;
        if (e->kind == CE_GO || e->kind == CE_LAST) {
            row_at(row_n > 0 ? row_n - 1 : 0, &x, &y);
            float_at(x + 0.9f * T.pw, y - 0.3f * T.pw, e->pts, e->kind == CE_GO ? -1 : 0, t);
        } else if (e->kind == CE_HEELS) {
            float_at(T.sx, T.sy + 0.6f * T.ow, e->pts, 0, t);
            A.heels = t;
        } else {
            int k = e->mask < 1 ? 1 : e->mask;
            float x0, x1, y0;
            row_at(row_n - k, &x0, &y0);
            row_at(row_n - 1, &x1, &y0);
            float_at((x0 + x1) / 2, y0 - 0.55f * T.pw * aspect(), e->pts,
                     e->kind == CE_FIFTEEN ? 15 : e->kind == CE_THIRTYONE ? 31 : 0, t);
            if (A.nhl < 4) A.hl_n[A.nhl] = k, A.hl_t[A.nhl++] = t;
        }
        ui_sound_later(SND_TAP, (int)(t - ui_ms()));
        t += EV_MS;
        any = 1;
    }
    if (any && leap_i < p->nleap) {
        leap(p->leap[leap_i].pl, p->leap[leap_i].from, p->leap[leap_i].to, t);
        t += LEAP_MS + 120;
    }
    return t;
}

static int stage_count(void);
static int stage_from(const cpos *p, int k, int i, float *x, float *y, float *w);

/* the show's count k: its cards come to the stage, its items lit one by one, then its leap */
static Uint32 show_events(const cpos *p, Uint32 t, int k)
{
    const uint8_t *cards;
    crib_show_of(p, k, &cards);
    A.show = t;
    for (int i = 0; i < 5; i++) {
        float x0, y0, w0, x1, y1;
        int up = stage_from(p, k, i, &x0, &y0, &w0);
        stage_at(i, &x1, &y1);
        slide(i < 4 ? cards[i] : p->starter, up, x0, y0, w0, 1, x1, y1, T.gw, SLIDE_MS, (int)(t - ui_ms()) + 50 * i,
              H_STAGE, 0, i, i == 0 ? SND_MOVE : -1);
    }
    t += SLIDE_MS + 4 * 50 + 200;
    A.items = t;
    int n = 0, j = -1;
    for (int i = 0; i < p->nev; i++) {
        const crib_ev *e = &p->ev[i];
        if (e->src != CS_SHOW || e->count != k) continue;
        j = e->leap;
        float sx = 0, sy = 0;
        int m = 0;
        for (int c = 0; c < 5; c++)
            if (e->mask >> c & 1) {
                float x, y;
                stage_at(c, &x, &y);
                sx += x, sy = y, m++;
            }
        float_at(sx / (m ? m : 1), sy - 0.62f * T.gw * aspect(), e->pts, e->kind == CE_FIFTEEN ? 15 : 0,
                 t + (Uint32)(n * ITEM_MS) + 120);
        ui_sound_later(SND_TAP, (int)(t + n * ITEM_MS + 120 - ui_ms()));
        n++;
    }
    t += (Uint32)(n * ITEM_MS);
    if (j >= 0 && j < p->nleap) {               /* the count's total pegged at once */
        leap(p->leap[j].pl, p->leap[j].from, p->leap[j].to, t);
        t += LEAP_MS;
    }
    return t + 200;
}

static void animate(const cpos *b)
{
    const cpos *p = &G.pos;
    Uint32 now = ui_ms(), t = now;
    int pl = b->turn;
    A.on = 1;
    A.b = *b;
    switch (b->phase) {
    case CP_DISCARD: {
        int k = 0;
        for (int s = 0; s < b->nhand[pl]; s++) {
            int kept = 0;
            for (int i = 0; i < 4; i++) kept |= p->kept[pl][i] == b->hand[pl][s];
            if (kept) continue;                 /* the cards laid away fly to the crib, face down */
            float x, y, w;
            hand_card_at(b, pl, s, &x, &y, &w);
            slide(b->hand[pl][s], pl == T.me, x, y, w, 0, T.kx, T.ky, T.pw, SLIDE_MS, 90 * k, H_CRIB, 0, 0,
                  k == 0 ? SND_MOVE : -1);
            k++;
        }
        break;
    }
    case CP_CUT:
        slide(p->starter, 0, T.sx, T.sy, T.ow, 1, T.sx, T.sy, T.ow, FLIP_MS, 0, H_STARTER, 0, 0, SND_PLACE);
        t += FLIP_MS + 120;
        t = play_events(p, t, 0, 0);
        break;
    case CP_PLAY: {
        int s = -1;
        for (int i = 0; i < 4; i++)
            if ((p->used[pl] >> i & 1) && !(b->used[pl] >> i & 1)) s = i;
        if (s < 0) break;
        float x, y, w, x1, y1;
        hand_card_at(b, pl, s, &x, &y, &w);
        int n = b->nseq + 1;
        row_at(n - 1, &x1, &y1);
        slide(b->kept[pl][s], pl == T.me, x, y, w, 1, x1, y1, T.pw, SLIDE_MS, 0, H_ROW, 0, n - 1, SND_PLACE);
        t += SLIDE_MS + 60;
        for (int j = 0; j < p->nleap; j++) {    /* the card's points, then the go (not the show's) */
            int play = 0;
            for (int i = 0; i < p->nev; i++) play |= p->ev[i].leap == j && p->ev[i].src == CS_PLAY;
            if (play) t = play_events(p, t, j, n);
        }
        if (p->phase != CP_OVER && p->nseq == 0) {   /* the count ends: its cards go */
            A.reset = t + 150;
            float ox, oy;
            row_at(0, &ox, &oy);
            for (int i = 0; i < n; i++) {
                float rx, ry;
                row_at(i, &rx, &ry);
                slide(i < n - 1 ? b->seq[i] : b->kept[pl][s], 1, rx, ry, T.pw, 0, ox, oy, T.pw, SLIDE_MS,
                      (int)(A.reset - now) + 40 * (n - 1 - i), H_NONE, 0, 0, i == 0 ? SND_MOVE : -1);
            }
            t = A.reset + SLIDE_MS + 40 * n + 100;
        }
        if (p->phase == CP_SHOW || (p->phase == CP_OVER && p->nev && p->ev[p->nev - 1].src == CS_SHOW)) {
            t = show_events(p, t, stage_count());
        }
        break;
    }
    case CP_SHOW:
        if (p->phase == CP_DISCARD) {
            calm();
            deal_slides();
            return;
        }
        t = show_events(p, t, stage_count());
        break;
    }
    cm_until(&M, t + CM_REST);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
/* the show's count on the stage (-1: none) */
static int stage_count(void)
{
    const cpos *p = &G.pos;
    for (int i = p->nev - 1; i >= 0; i--)
        if (p->ev[i].src == CS_SHOW) return p->ev[i].count;
    if (p->phase == CP_SHOW) return p->shown - 1;
    return -1;
}

/* where a stage card comes from: the hand, the crib, the starter on the pack; 1: face up there */
static int stage_from(const cpos *p, int k, int i, float *x, float *y, float *w)
{
    if (i == 4) { *x = T.sx, *y = T.sy, *w = T.ow; return 1; }
    if (k >= p->players) { *x = T.kx, *y = T.ky, *w = T.pw; return 0; }
    const uint8_t *cards;
    int pl = crib_show_of(p, k, &cards);
    if (pl == T.me) {
        cards_spot sp[4];
        hand_spots(4, sp);
        *x = sp[i].x, *y = sp[i].y, *w = T.hw;
    } else {
        *x = T.ox[pl] + (i - 1.5f) * 0.5f * T.ow, *y = T.oy, *w = T.ow;
    }
    return 1;
}

static void start(int v)
{
    (void)v;
    crib_new(&G, ui.players == 3 ? 3 : 2, (uint32_t)ui_random(1 << 30));
    calm();
    memset(pegs, 0, sizeof pegs);
    sel = 0;
    floor_n = 0;
    dealing = 1;
}

static int status(void)
{
    int w = crib_winner(&G.pos);
    return w < 0 ? GS_PLAY : gs_win(w);
}

static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    cpos before = G.pos;
    if (!crib_play(&G, a)) { ui_sound(SND_DENY); return; }
    calm();
    sel = 0;
    animate(&before);
    pegs_sync();
    const cpos *p = &G.pos;
    if (p->phase != CP_SHOW && p->phase != CP_OVER && p->turn != ui.viewer) {
        floor_n = G.n;
        ui_handoff(p->turn);                    /* the device passes (after the cards and the pegs) */
    }
}

static int undo(void)
{
    if (G.n <= floor_n || !crib_undo(&G)) return 0;
    calm();
    sel = 0;
    dealing = 0;
    pegs_sync();
    return 1;
}

static int script(const char *s)
{
    int ok = crib_script(&G, s);
    calm();
    sel = 0;
    dealing = 0;
    pegs_sync();
    floor_n = G.n;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_tok[CRIB_MAXP], *t_disc, *t_go, *t_play, *t_pill;
static int tok_size, disc_size, go_size, play_size, pill_w, pill_h;

static void textures(void)
{
    int ts = (int)(0.42f * ui.unit);
    if (ts != tok_size) {
        tok_size = ts;
        for (int q = 0; q < CRIB_MAXP; q++) {
            ui_free(&t_tok[q]);
            cards_token(q, ts, ui_buf(ts, ts));
            t_tok[q] = ui_tex(ts, ts, ui_buf(ts, ts));
        }
    }
    int ds = (int)(0.86f * ui.unit);
    if (ds != disc_size) {
        disc_size = ds;
        const uint8_t dark[3] = {24, 30, 28};
        ui_free(&t_disc);
        art_disc(ds, dark, 0.82f, 0, ui_buf(ds, ds));
        t_disc = ui_tex(ds, ds, ui_buf(ds, ds));
    }
    int gs = (int)(0.4f * ui.unit);
    if (gs != go_size) {
        go_size = gs;
        ui_free(&t_go);
        art_icon(ICON_PASS, gs, ui_buf(gs, gs));
        t_go = ui_tex(gs, gs, ui_buf(gs, gs));
    }
    int psz = (int)(0.5f * ui.unit);
    if (psz != play_size) {
        play_size = psz;
        ui_free(&t_play);
        art_icon(ICON_PLAY, psz, ui_buf(psz, psz));
        t_play = ui_tex(psz, psz, ui_buf(psz, psz));
    }
    int pw = (int)(1.5f * ui.unit), ph = (int)(0.56f * ui.unit);
    if (pw != pill_w || ph != pill_h) {
        pill_w = pw, pill_h = ph;
        const uint8_t c[4] = {24, 30, 28, 200};
        ui_free(&t_pill);
        art_roundrect(pw, ph, ph / 2.0f, c, ui_buf(pw, ph));
        t_pill = ui_tex(pw, ph, ui_buf(pw, ph));
    }
}

/* the count: its digits in a dark disc */
static void draw_count(float x, float y, int count)
{
    ui_put_c(t_disc, x, y, (float)disc_size, (float)disc_size);
    ui_number(x, y, 0.44f * ui.unit, count, 1);
}

/* the row of the count, as it stands (before the cards of an ended count have gone, as they were) */
static void draw_row(const cpos *p)
{
    Uint32 now = ui_ms();
    const uint8_t *seq = p->seq;
    int n = p->nseq, old = p->old, count = p->count;
    uint8_t tmp[16];
    if (A.on && A.b.phase == CP_PLAY && A.reset && now < A.reset) {   /* the ended count, still there */
        n = A.b.nseq;
        memcpy(tmp, A.b.seq, (size_t)n);
        for (int s = 0; s < 4; s++)
            if ((p->used[A.b.turn] >> s & 1) && !(A.b.used[A.b.turn] >> s & 1)) tmp[n++] = p->kept[A.b.turn][s];
        seq = tmp;
        old = A.b.old;
        count = 0;
        for (int i = 0; i < n; i++) count += crib_value(seq[i]);
    } else if (A.on && A.reset && now < A.reset + SLIDE_MS + 400) old = A.b.old;
    if (old) {
        float x, y;
        row_at(0, &x, &y);
        cards_pile(deck, x, y, T.pw, old, CARD_NONE, 0);
    }
    for (int i = 0; i < n; i++) {
        if (hides(H_ROW, 0, i)) continue;
        float x, y;
        row_at(i, &x, &y);
        int how = 0;
        for (int h = 0; h < A.nhl; h++)
            if (now >= A.hl_t[h] && now < A.hl_t[h] + EV_MS && i >= n - A.hl_n[h]) how = CD_GLOW, y -= 0.12f * T.pw;
        cards_draw(deck, seq[i], x, y, T.pw, 0, how);
    }
    if (n && !hides(H_ROW, 0, n - 1)) {
        float x, y;
        row_at(n - 1, &x, &y);
        draw_count(fminf(x + T.pw / 2 + 0.55f * ui.unit, T.b.x + T.b.w - 0.45f * ui.unit), y, count);
    }
}

/* the show's stage: the count's owner and its points so far over the four cards and the starter; the combination
 * being counted lit and lifted */
static void draw_stage(const cpos *p, int k)
{
    const uint8_t *cards;
    int pl = crib_show_of(p, k, &cards), n;
    crib_ev it[CRIB_MAX_EV];
    int total = crib_show(cards, p->starter, k >= p->players, it, &n);
    Uint32 now = ui_ms();
    int active = -1, sum = total;
    if (A.on && A.items && stage_count() == k && now < A.items + (Uint32)(n * ITEM_MS)) {
        sum = 0;
        if (now >= A.items) {
            active = (int)((now - A.items) / ITEM_MS);
            for (int i = 0; i <= active && i < n; i++) sum += it[i].pts;
        }
        ui_animate();
    }
    unsigned lit = active >= 0 ? it[active].mask : 0;
    for (int i = 0; i < 5; i++) {
        if (hides(H_STAGE, 0, i)) continue;
        float x, y;
        stage_at(i, &x, &y);
        int on = lit >> i & 1;
        cards_draw(deck, i < 4 ? cards[i] : p->starter, x, y - (on ? 0.16f * T.gw : 0), T.gw, 0, on ? CD_GLOW : 0);
    }
    /* the owner (the crib: a card's back beside the token) and the points */
    float x0, y0;
    stage_at(0, &x0, &y0);
    float hy = y0 - 0.5f * T.gw * aspect() - 0.62f * ui.unit, tx = x0 - 0.1f * T.gw;
    if (A.on && now < A.show + SLIDE_MS) return;
    ui_mark_gold(tx, hy, 1.1f * ui.unit);
    ui_put_c(t_tok[pl], tx, hy, 0.62f * ui.unit, 0.62f * ui.unit);
    float nx = tx + 0.62f * ui.unit;
    if (k >= p->players) {
        cards_draw(deck, CARD_NONE, nx, hy, 0.36f * ui.unit, 0, CD_DOWN);
        nx += 0.5f * ui.unit;
    }
    if (sum > 0) cm_plus(nx + 0.55f * ui.unit, hy, 0.5f * ui.unit, sum);
    else ui_number(nx + 0.3f * ui.unit, hy, 0.5f * ui.unit, 0, 1);
    if (!busy() && p->phase == CP_SHOW) {       /* tap to go on: a play sign */
        float px = T.b.x + T.b.w - 0.5f * ui.unit;
        ui_put_c(t_disc, px, hy, 0.8f * ui.unit, 0.8f * ui.unit);
        ui_put_c(t_play, px + 0.04f * ui.unit, hy, (float)play_size * 0.8f, (float)play_size * 0.8f);
    }
}

static void draw(void)
{
    const cpos *p = &G.pos;
    if (dealing) { dealing = 0; deal_slides(); }
    textures();
    Uint32 now = ui_ms();
    int b = busy(), me = T.me, ph = p->phase, mine = p->turn == me && !ui_handing() && !b;
    int sk = stage_count(), showing = (ph == CP_SHOW || ph == CP_OVER) && sk >= 0;
    if (showing && A.on && now < A.show) showing = 0;   /* the play ends first */
    /* the pack, the starter on it */
    int starter_up = p->starter != CARD_NONE && !hides(H_STARTER, 0, 0) && !showing;
    int how = mine && ph == CP_CUT ? CD_GLOW : 0;
    if (A.on && now >= A.heels && now < A.heels + EV_MS * 2 && A.heels) how |= CD_GLOW;
    cards_pile(deck, T.sx, T.sy, T.ow, p->nstock + (starter_up ? 1 : 0), starter_up ? p->starter : CARD_NONE, how);
    /* the others' hands: face down (the show: face up, the one on the stage gone to it) */
    for (int q = 0; q < p->players; q++) {
        if (q == me) continue;
        int slots[6], n = my_cards(p, q, slots), away = 0;
        if (showing) {
            const uint8_t *c;
            n = 4;
            away = sk < p->players && crib_show_of(p, sk, &c) == q;
        }
        for (int i = 0; i < n; i++) {
            if (hides(H_HAND, q, i)) continue;
            float x = T.ox[q] + (i - (n - 1) / 2.0f) * 0.5f * T.ow;
            if (away) cards_place(deck, x, T.oy, T.ow);
            else if (showing) cards_draw(deck, p->kept[q][i], x, T.oy, T.ow, 0, 0);
            else cards_draw(deck, card_of(p, q, slots[i]), x, T.oy, T.ow, 0, CD_DOWN);
        }
        if (p->players > 2) {
            float x = T.ox[q] - (n > 0 ? n - 1 : 0) / 2.0f * 0.5f * T.ow - 0.5f * T.ow - 0.3f * ui.unit;
            ui_put_c(t_tok[q], x, T.oy, (float)tok_size, (float)tok_size);
        }
    }
    draw_board();
    /* the crib, by the dealer's token */
    int ncrib = p->ncrib;
    ncrib -= cm_hiding(&M, H_CRIB);
    if (showing && sk >= p->players) ncrib = 0;
    int full = ph == CP_DISCARD && p->turn == me && cm_bits(sel) == crib_away(p) && mine;
    if (full) ui_mark_gold(T.kx, T.ky, T.pw * 1.9f);
    if (!showing) {
        cards_pile(deck, T.kx, T.ky, T.pw, ncrib < 0 ? 0 : ncrib, CARD_NONE, full ? CD_GLOW : 0);
        ui_put_c(t_tok[p->dealer], T.kx - 0.42f * T.pw, T.ky + 0.62f * T.pw * aspect(), (float)tok_size,
                 (float)tok_size);
        draw_row(p);
    } else draw_stage(p, sk);
    /* the viewer's hand */
    int slots[6], n = my_cards(p, me, slots);
    if (showing) n = 4;
    cards_spot sp[6];
    hand_spots(n, sp);
    for (int i = 0; i < n; i++) {
        int s = showing ? i : slots[i];
        if (hides(H_HAND, me, s) || (!showing && drag.on && drag.moved && drag.slot == s)) continue;
        if (showing) {
            const uint8_t *c;
            if (sk < p->players && crib_show_of(p, sk, &c) == me) { cards_place(deck, sp[i].x, sp[i].y, T.hw); continue; }
            cards_draw(deck, p->kept[me][i], sp[i].x, sp[i].y, T.hw, sp[i].deg, 0);
            continue;
        }
        int h = 0;
        float y = sp[i].y;
        if (sel >> s & 1) h |= CD_RAISED, y -= 0.18f * T.hw;
        if (mine && ph == CP_PLAY) h |= crib_fits(p, me, s) ? (sel >> s & 1 ? 0 : CD_GLOW) : CD_DIM;
        cards_draw(deck, card_of(p, me, s), sp[i].x, y, T.hw, sp[i].deg, h);
    }
    cm_draw(&M);
    if (drag.on && drag.moved && !showing)  /* the card dragged, under the finger */
        cards_draw(deck, card_of(p, me, drag.slot), drag.x, drag.y, T.hw, 0, CD_RAISED);
    cm_floats_draw(&M, t_pill, t_go);
    if (busy()) ui_animate();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */

static int hand_hit(float x, float y)
{
    const cpos *p = &G.pos;
    int slots[6], n = my_cards(p, T.me, slots);
    cards_spot sp[6];
    hand_spots(n, sp);
    for (int i = 0; i < n; i++)                 /* a raised card is higher */
        if (sel >> slots[i] & 1) sp[i].y -= 0.18f * T.hw;
    int i = cards_hit(sp, n, T.hw, aspect(), x, y);
    return i < 0 ? -1 : slots[i];
}

static void press(float x, float y)
{
    drag.on = 0;
    const cpos *p = &G.pos;
    if (ui_handing() || p->phase == CP_OVER) return;
    if (p->phase == CP_SHOW) {
        if (busy()) {                           /* a tap hurries the show on */
            calm();
            pegs_sync();
        } else act(CA_NEXT);
        return;
    }
    if (p->turn != T.me || busy()) { ui_sound(SND_DENY); return; }
    int s = hand_hit(x, y);
    if (s >= 0) {
        drag.on = 1, drag.slot = s, drag.moved = 0;
        drag.x0 = drag.x = x, drag.y0 = drag.y = y;
        return;
    }
    if (p->phase == CP_CUT && cards_in(T.sx, T.sy, T.ow * 1.3f, aspect(), x, y)) { act(CA_CUT); return; }
    if (p->phase == CP_DISCARD && cards_in(T.kx, T.ky, T.pw * 1.2f, aspect(), x, y)) {
        if (cm_bits(sel) == crib_away(p)) act((int)sel);
        else ui_sound(SND_DENY);
        return;
    }
    if (p->phase == CP_PLAY && sel && y > T.pz0 && y < T.pz1) {   /* the table: lay the chosen card */
        for (int i = 0; i < 4; i++)
            if (sel == 1u << i) act(CA_PLAY + i);
        return;
    }
    ui_sound(SND_DENY);
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x, drag.y = y;
    if (hypotf(x - drag.x0, y - drag.y0) > 0.25f * ui.unit) drag.moved = 1;
}

/* a card tapped: chosen (again: laid); dragged to the table: laid (the crib: chosen, and away once they are all) */
static void release(float x, float y)
{
    (void)x;
    const cpos *p = &G.pos;
    if (!drag.on) return;
    drag.on = 0;
    if (ui_handing() || p->turn != T.me) return;
    int s = drag.slot, n = crib_away(p);
    if (drag.moved) {
        if (y > T.pz1 - 0.2f * T.hw) return;    /* back in the hand */
        if (p->phase == CP_PLAY) {
            if (crib_fits(p, T.me, s)) act(CA_PLAY + s);
            else ui_sound(SND_DENY);
        } else if (p->phase == CP_DISCARD) {
            if (!(sel >> s & 1) && cm_bits(sel) < n) sel |= 1u << s;
            if (cm_bits(sel) == n) act((int)sel);
        }
        return;
    }
    if (p->phase == CP_DISCARD) {
        if (sel >> s & 1) sel &= ~(1u << s);
        else if (cm_bits(sel) < n) sel |= 1u << s, ui_sound(SND_TAP);
        else ui_sound(SND_DENY);
    } else if (p->phase == CP_PLAY) {
        if (!crib_fits(p, T.me, s)) ui_sound(SND_DENY);
        else if (sel == 1u << s) act(CA_PLAY + s);
        else sel = 1u << s, ui_sound(SND_TAP);
    }
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the score; the go's sign when the player cannot lay a card */
{
    const cpos *p = &G.pos;
    float h = fminf(0.46f * ui.unit, b.h * 0.6f);
    int v = p->score[player], digits = v >= 100 ? 3 : v >= 10 ? 2 : 1;
    float w = (0.74f * (digits - 1) + 0.62f) * h;
    ui_number(b.x + w / 2, b.y + b.h / 2, h, v, 1);
    int left = 0;
    for (int s = 0; s < 4; s++) left += p->kept[player][s] != CARD_NONE && !(p->used[player] >> s & 1);
    if (p->phase == CP_PLAY && p->nseq && left && !crib_can_play(p, player) && t_go)
        ui_put_c(t_go, b.x + w + 0.45f * ui.unit, b.y + b.h / 2, 0.42f * ui.unit, 0.42f * ui.unit);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: a corner of the peg board with its pegs, the 29 hand fanned over it */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    int P = size / 13, fr = P / 2, nx = 12, ny = 4, bw = nx * P + 2 * fr, bh = ny * P + 2 * fr;
    uint8_t *b = malloc((size_t)bw * bh * 4), *peg = malloc((size_t)P * P * 4);
    if (!b || !peg) { free(b); free(peg); return; }
    art_board_n(nx, ny, P, fr, 0, b);
    static const int at[2][2] = {{7, 10}, {4, 6}};   /* the pegs: lane 0 (bottom) row 0, lane 1 */
    for (int l = 0; l < 2; l++)
        for (int r = 0; r < 2; r++)
            for (int c = 0; c < 10; c++) {
                float x = fr + (1.2f + c + 0.5f * (c / 5)) * P, y = fr + (1 - l) * 2 * P + (r ? 0.55f : 1.45f) * P;
                bore(b, bw, bh, x, y, 0.2f * P);
            }
    int by = size - bh - size / 14;
    art_blend(rgba, size, size, b, bw, bh, (size - bw) / 2, by);
    for (int l = 0; l < 2; l++)
        for (int k = 0; k < 2; k++) {
            int c = at[l][k] - 1;
            art_counter(COUNTER_STONE, l, P, peg);
            float x = (size - bw) / 2 + fr + (1.2f + c + 0.5f * (c / 5)) * P, y = by + fr + (1 - l) * 2 * P + 1.45f * P;
            art_blend(rgba, size, size, peg, P, P, (int)(x - P / 2), (int)(y - P / 2));
        }
    static const int hand[5] = {CARD(SUIT_SPADES, 5), CARD(SUIT_HEARTS, 5), CARD(SUIT_DIAMONDS, 5),
                                CARD(SUIT_CLUBS, RANK_JACK), CARD(SUIT_CLUBS, 5)};
    int cw = size * 30 / 100, ch = (int)(cw * 1.4f);
    uint8_t *c = malloc((size_t)cw * ch * 4);
    if (c) {
        for (int i = 0; i < 5; i++) {
            cards_paint(DECK_FRENCH, hand[i], cw, ch, c);
            art_blend(rgba, size, size, c, cw, ch, size / 20 + i * (size - size / 10 - cw) / 4, size / 16);
        }
        free(c);
    }
    free(b);
    free(peg);
}

static const game cribbage_game = {
    .id = "cribbage", .order = 292, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
    .players = 1 << 2 | 1 << 3, .view = VIEW_HIDDEN,
};
REGISTER_GAME(cribbage_game)
