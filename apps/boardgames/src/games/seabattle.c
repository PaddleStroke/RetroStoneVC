/*
 * Sea Battle (Bataille navale) in RetroStone Board: two fleets hidden at sea, the device passed after every turn (the
 * rules: seabattle_rules.c, seabattle.h). VIEW_HIDDEN: each view is one player's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The sea: a wooden board in its walnut frame, its 10 x 10 squares inlaid in sea blue. The ships: wooden pieces (a
 * hull, its deck, a cabin and funnels). The pegs: small round pins, white for a miss, red for a hit.
 * Placing: your sea big, your fleet on it (a random one to start from); drag a ship, or tap it (it lifts) and tap
 * where its top or left end should go; tap it again to turn it round the square tapped. The die: another random
 * fleet; the tick: the fleet ready (the device passes).
 * Firing: the other's sea big, your shots on it (their sunk ships shown), your own sea small beside it with their
 * shots (the last ones marked). Tap squares to aim (one shot, or a salvo: as many as your ships afloat; the number
 * left to aim shows by the fire button); tap the fire button, or the aimed square again (one shot). The pegs drop,
 * a sunk ship rises into view; a moment to see it, then the device passes.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/guessart.h"
#include "games/seabattle.h"
#include "shell.h"

#define DROP_MS 280                             /* a peg dropping */
#define GAP_MS 170                              /* the next one of a salvo */
#define RISE_MS 360                             /* a sunk ship coming into view */
#define REST_MS 1300                            /* the result shown before the device passes */

static sbgame G;
static const sbpos *P = &G.pos;
static int variant;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static const float HULL[3] = {160, 104, 62}, DECK[3] = {228, 192, 142}, CABIN[3] = {118, 78, 48},
                   FUNNEL[3] = {84, 56, 36};

/* the sea: the house's plain wood in its walnut frame, the squares inlaid in blue, set a little into the wood */
static void paint_sea(int sq, int fr, uint8_t *o)
{
    static const float blue[3] = {50, 108, 156};
    int w = SB_N * sq + 2 * fr;
    art_board_n(SB_N, SB_N, sq, fr, 0, o);
    int m = (int)fmaxf(1, roundf(0.07f * sq)), e = (int)fmaxf(1, roundf(0.06f * sq));
    for (int c = 0; c < SB_CELLS; c++) {
        int x0 = fr + (c % SB_N) * sq + m, y0 = fr + (c / SB_N) * sq + m, n = sq - 2 * m;
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                float fx = (float)x / n, fy = (float)y / n;
                float k = 1 + 0.07f * sinf(fx * 9 + 3 * sinf(fy * 7 + c)) + 0.05f * sinf(fy * 21 + fx * 4);
                k *= 1.08f - 0.16f * fy;
                if (x < e || y < e) k *= 0.66f;                     /* inlaid: the wood's shade on its top left */
                else if (x >= n - e || y >= n - e) k *= 1.14f;
                uint8_t *p = o + 4 * ((size_t)(y0 + y) * w + x0 + x);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(blue[i] * k, 0, 255);
                p[3] = 255;
            }
    }
}

/* a ship of len squares lying across, sq pixels a square: (len * sq) x sq */
static void paint_ship(int len, int sq, uint8_t *o)
{
    int w = len * sq;
    memset(o, 0, (size_t)w * sq * 4);
    ga_pen p;
    ga_begin(&p, o, w, sq, (float)sq, 0, 0);
    float L = (float)len, lw = fmaxf(1, 0.035f * sq);
    const float hull[] = {0.15f, 0.2f, L - 0.62f, 0.19f, L - 0.3f, 0.27f, L - 0.06f, 0.5f, L - 0.3f, 0.73f,
                          L - 0.62f, 0.81f, 0.15f, 0.8f, 0.08f, 0.66f, 0.06f, 0.5f, 0.08f, 0.34f};
    ga_poly(&p, ART_PART, hull, 10);
    ga_paint(&p, HULL, NULL, 0.35f, lw);
    const float deck[] = {0.2f, 0.3f, L - 0.64f, 0.29f, L - 0.36f, 0.35f, L - 0.18f, 0.5f, L - 0.36f, 0.65f,
                          L - 0.64f, 0.71f, 0.2f, 0.7f, 0.15f, 0.5f};
    ga_poly(&p, ART_PART, deck, 8);
    for (int i = 1; i < len; i++) ga_rect(&p, ART_MARK, i - 0.01f, 0.31f, i + 0.01f, 0.69f);   /* the squares' seams */
    float seam[3] = {DECK[0] * 0.8f, DECK[1] * 0.8f, DECK[2] * 0.8f};
    ga_paint(&p, DECK, seam, 0.2f, lw * 0.8f);
    if (len == 1) ga_ell(&p, ART_PART, 0.44f, 0.5f, 0.15f, 0.12f);
    else ga_rect(&p, ART_PART, 0.28f * L, 0.37f, 0.28f * L + fminf(1.1f, 0.3f * L), 0.63f);
    ga_paint(&p, CABIN, NULL, 0.3f, lw);
    if (len >= 3) {
        ga_ell(&p, ART_PART, 0.64f * L, 0.5f, 0.12f, 0.12f);
        if (len >= 5) ga_ell(&p, ART_PART, 0.64f * L + 0.4f, 0.5f, 0.1f, 0.1f);
        ga_paint(&p, FUNNEL, NULL, 0.45f, lw);
    }
    ga_end(&p);
}

/* turned a quarter clockwise: (w x h) to (h x w) */
static void quarter(const uint8_t *s, int w, int h, uint8_t *d)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) memcpy(d + 4 * ((size_t)x * h + (h - 1 - y)), s + 4 * ((size_t)y * w + x), 4);
}

/* aiming: a ring and its four ticks (white: tinted) */
static void paint_aim(int size, uint8_t *o)
{
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    art_arc(f, ART_PART, 0.5f, 0.5f, 0.24f, 0.31f, 0, 360);
    art_rect(f, ART_PART, 0.47f, 0.08f, 0.53f, 0.32f);
    art_rect(f, ART_PART, 0.47f, 0.68f, 0.53f, 0.92f);
    art_rect(f, ART_PART, 0.08f, 0.47f, 0.32f, 0.53f);
    art_rect(f, ART_PART, 0.68f, 0.47f, 0.92f, 0.53f);
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.05f, 0.05f);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

static void paint_die(int size, uint8_t *o) { art_die(5, 0, size, o); }

/* ---- the table's geometry ---------------------------------------------------------------------------------------- */
static struct {
    box b;
    int wide;
    float bx, by;                               /* the big sea */
    int sq, fr, side;
    float sx, sy;                               /* the small one */
    int ssq, sfr, sside;
    box panel;                                  /* the room beside or under the big sea */
    float bd, ax, ay, kx, ky;                   /* firing: the shots left to aim, the fire button */
    float px, qx, py;                           /* placing: the die, the tick */
} T;

static void layout(box b)
{
    float u = ui.unit;
    T.b = b;
    T.wide = b.w > b.h * 1.05f;
    if (!T.wide) {
        float small = fmaxf(3.0f * u, b.h * 0.25f);
        T.sq = (int)fminf(b.w / 10.9f, (b.h - small) / 10.9f);
    } else {
        T.sq = (int)fminf(b.h / 10.9f, (b.w - 3.0f * u) / 10.9f);
    }
    T.fr = (int)roundf(0.45f * T.sq);
    T.side = SB_N * T.sq + 2 * T.fr;
    if (!T.wide) {
        T.bx = floorf(b.x + (b.w - T.side) / 2);
        T.by = floorf(b.y);
        T.panel = (box){b.x, T.by + T.side + 0.15f * u, b.w, b.y + b.h - (T.by + T.side + 0.15f * u)};
        T.ssq = (int)fminf(T.panel.h / 10.9f, b.w * 0.45f / 10.9f);
    } else {
        T.bx = floorf(b.x);
        T.by = floorf(b.y + (b.h - T.side) / 2);
        T.panel = (box){T.bx + T.side + 0.2f * u, b.y, b.x + b.w - (T.bx + T.side + 0.2f * u), b.h};
        T.ssq = (int)(T.panel.w / 10.9f);
    }
    T.sfr = (int)fmaxf(2, roundf(0.45f * T.ssq));
    T.sside = SB_N * T.ssq + 2 * T.sfr;
    T.bd = floorf(fminf(1.1f * u, (T.wide ? T.panel.w : T.panel.h) * 0.42f));
    box q = T.panel;
    if (!T.wide) {                              /* the small sea on the left, the buttons on the right */
        T.sx = floorf(q.x);
        T.sy = floorf(q.y + (q.h - T.sside) / 2);
        float rx = q.x + T.sside, rw = q.x + q.w - rx;
        T.kx = rx + rw * 0.62f;
        T.ax = T.kx - 1.25f * T.bd;
        T.ay = T.ky = q.y + q.h / 2;
    } else {                                    /* the small sea on top, the buttons under it */
        T.sx = floorf(q.x + (q.w - T.sside) / 2);
        T.sy = floorf(q.y);
        float ry = q.y + T.sside, rh = q.y + q.h - ry;
        T.kx = q.x + q.w * 0.62f;
        T.ax = T.kx - 1.25f * T.bd;
        T.ay = T.ky = ry + rh * 0.5f;
    }
    T.py = T.panel.y + T.panel.h / 2;
    T.px = T.panel.x + T.panel.w / 2 - 0.85f * T.bd;
    T.qx = T.panel.x + T.panel.w / 2 + 0.85f * T.bd;
}

/* ---- the textures ------------------------------------------------------------------------------------------------ */
static SDL_Texture *t_sea[2], *t_ship[2][6], *t_peg[2][2], *t_aim, *t_fire, *t_die, *t_tick, *t_boat;
static int made_sq = -1, made_ssq = -1, made_bd = -1, made_boat = -1;
static ga_buttons btn;

static void make_set(int k, int sq, int fr)
{
    int side = SB_N * sq + 2 * fr;
    uint8_t *o = ui_buf(side, side);
    paint_sea(sq, fr, o);
    ui_free(&t_sea[k]);
    t_sea[k] = ui_tex(side, side, o);
    for (int len = 1; len <= 5; len++) {
        o = ui_buf(len * sq, sq);
        paint_ship(len, sq, o);
        ui_free(&t_ship[k][len]);
        t_ship[k][len] = ui_tex(len * sq, sq, o);
    }
    int d = (int)fmaxf(4, 0.46f * sq);
    for (int c = 0; c < 2; c++) {
        o = ui_buf(d, d);
        art_counter(COUNTER_STONE, c ? 2 : 0, d, o);
        ui_free(&t_peg[k][c]);
        t_peg[k][c] = ui_tex(d, d, o);
    }
}

static void textures(void)
{
    if (made_sq != T.sq) {
        made_sq = T.sq;
        make_set(0, T.sq, T.fr);
        ui_free(&t_aim);
        t_aim = ga_icon(paint_aim, (int)(0.9f * T.sq), 0);
        if (t_aim) SDL_SetTextureColorMod(t_aim, 250, 214, 90);
    }
    if (made_ssq != T.ssq) {
        made_ssq = T.ssq;
        make_set(1, T.ssq, T.sfr);
    }
    if (made_bd != (int)T.bd) {
        made_bd = (int)T.bd;
        ga_buttons_make(&btn, made_bd);
        int is = (int)(0.62f * T.bd);
        ui_free(&t_fire);
        t_fire = ga_icon(paint_aim, is, 1);
        ui_free(&t_tick);
        t_tick = ga_icon(ga_tick, is, 1);
        ui_free(&t_die);
        t_die = ga_icon(paint_die, is, 0);
    }
}

/* ---- the state of the hands ------------------------------------------------------------------------------------- */
static int selected = -1;                       /* placing: the ship lifted */
static struct { int on, ship, k, moved; float x0, y0, x, y, dx, dy; } drag;
static int aim[SB_MAXSHIPS], naim;              /* firing: the squares aimed at */
static struct { int on, n, cells[SB_MAXSHIPS], hit[SB_MAXSHIPS], sunk[SB_MAXSHIPS], target; Uint32 t0; } fx;

static void calm(void)
{
    selected = -1;
    drag.on = 0;
    naim = 0;
    fx.on = 0;
}

static int me(void) { return ui.viewer >= 0 ? ui.viewer : P->turn; }

static int fx_length(void) { return (fx.n - 1) * GAP_MS + DROP_MS + RISE_MS + REST_MS; }

static int busy(void) { return fx.on && ui_ms() - fx.t0 < (Uint32)fx_length(); }

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static void start(int v)
{
    variant = v >= 0 && v < SB_VARIANTS ? v : SB_CLASSIC;
    sb_new(&G, variant, (uint32_t)ui_random(1 << 30));
    calm();
}

static int turn(void) { return P->turn; }
static int status(void) { return P->phase == SP_OVER ? gs_win(P->winner) : GS_PLAY; }

static int undo(void)
{
    if (P->phase == SP_SHOOT && naim > 0) { naim--; return 1; }
    if (!sb_undo(&G)) return 0;
    selected = -1;
    drag.on = 0;
    return 1;
}

static int script(const char *s)
{
    int ok = sb_script(&G, s);
    calm();
    ui_handoff(P->turn);
    return ok;
}

static void fire(void)
{
    int them = !P->turn, was[SB_MAXSHIPS];
    for (int i = 0; i < P->nships; i++) was[i] = sb_sunk(P, them, i);
    if (!sb_fire(&G, aim, naim)) { ui_sound(SND_DENY); return; }
    fx.on = 1;
    fx.t0 = ui_ms();
    fx.n = naim;
    fx.target = them;
    int sunk = 0;
    for (int i = 0; i < naim; i++) {
        fx.cells[i] = aim[i];
        fx.hit[i] = P->shot[them][aim[i]] == SHOT_HIT;
        ui_sound_later(fx.hit[i] ? SND_CAPTURE : SND_PLACE, i * GAP_MS + DROP_MS - 40);
    }
    for (int i = 0; i < P->nships; i++) sunk |= (fx.sunk[i] = !was[i] && sb_sunk(P, them, i));
    if (sunk) ui_sound_later(SND_CHECK, (naim - 1) * GAP_MS + DROP_MS + 120);
    naim = 0;
    if (P->phase != SP_OVER) ui_handoff(P->turn);   /* after the pause showing the result */
    ui_animate();
}

/* ---- the places on the screen ------------------------------------------------------------------------------------ */
static int cell_at(float x, float y)            /* the big sea's square under (x, y) (-1: none) */
{
    float fx = (x - T.bx - T.fr) / T.sq, fy = (y - T.by - T.fr) / T.sq;
    if (fx < 0 || fy < 0 || fx >= SB_N || fy >= SB_N) return -1;
    return (int)fy * SB_N + (int)fx;
}

/* a ship moved to (x, y), across or down, kept inside the sea; 1: the fleet stays valid (set) */
static int move_ship(int i, int x, int y, int vert)
{
    sbship f[SB_MAXSHIPS];
    memcpy(f, P->ship[P->turn], sizeof f);
    int len = f[i].len;
    int mx = vert ? SB_N - 1 : SB_N - len, my = vert ? SB_N - len : SB_N - 1;
    f[i].x = (int8_t)(x < 0 ? 0 : x > mx ? mx : x);
    f[i].y = (int8_t)(y < 0 ? 0 : y > my ? my : y);
    f[i].vert = (uint8_t)vert;
    return sb_set(&G, f);
}

/* where a dragged ship would land: its first square */
static void drag_target(int *x, int *y)
{
    const sbship *s = &P->ship[P->turn][drag.ship];
    float fx = (drag.x - drag.dx - T.bx - T.fr) / T.sq, fy = (drag.y - drag.dy - T.by - T.fr) / T.sq;
    int len = s->len;
    *x = (int)floorf(fx + 0.5f);
    *y = (int)floorf(fy + 0.5f);
    int mx = s->vert ? SB_N - 1 : SB_N - len, my = s->vert ? SB_N - len : SB_N - 1;
    *x = *x < 0 ? 0 : *x > mx ? mx : *x;
    *y = *y < 0 ? 0 : *y > my ? my : *y;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* a ship with its shadow; lifted (chosen, held): off the sea a little, its shadow further */
static void put_ship(int k, int sq, float x0, float y0, int len, int vert, float alpha, int lifted)
{
    SDL_Texture *t = t_ship[k][len];
    float w = len * sq, cx = x0 + (vert ? sq : w) / 2.0f, cy = y0 + (vert ? w : sq) / 2.0f, up = lifted ? 0.1f * sq : 0;
    ui_shadow_flat(cx + up * 0.6f, cy + up * 0.6f, vert ? sq * 0.8f : w * 0.95f, vert ? w * 0.95f : sq * 0.8f,
                   0.8f * alpha);
    cx -= up;
    cy -= up;
    x0 -= up;
    y0 -= up;
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(alpha, 0, 1)));
    if (vert) ui_put_rot(t, cx, cy, w, sq, 90);
    else ui_put(t, x0, y0, w, sq);
    SDL_SetTextureAlphaMod(t, 255);
}

/* a sea: its board, the ships shown (all: mine; theirs: sunk), the pegs; k 0 the big one, 1 the small */
static void draw_sea(int k, int pl, int mine, float ox, float oy, int sq, int fr)
{
    int side = SB_N * sq + 2 * fr;
    ui_put(t_sea[k], ox, oy, side, side);
    float x0 = ox + fr, y0 = oy + fr;
    Uint32 now = ui_ms();
    int anim = fx.on && fx.target == pl && busy();
    float t_rise = (fx.n - 1) * GAP_MS + DROP_MS;
    if (k == 1 || !anim)                        /* the last shots received */
        for (int i = 0; i < P->nlast[pl]; i++) {
            int c = P->last[pl][i];
            ui_mark_last(x0 + (c % SB_N) * sq, y0 + (c / SB_N) * sq, sq, sq);
        }
    for (int i = 0; i < P->nships; i++) {
        const sbship *s = &P->ship[pl][i];
        int sunk = sb_sunk(P, pl, i);
        if (!mine && !sunk) continue;
        if (drag.on && drag.moved && mine && k == 0 && i == drag.ship) continue;
        float a = 1;
        if (!mine && anim && fx.sunk[i]) {      /* sinking now: it rises into view */
            float t = (now - fx.t0 - t_rise) / (float)RISE_MS;
            if (t <= 0) continue;
            a = fminf(1, t);
        }
        if (mine && sunk) a = 0.55f;
        if (mine && k == 0 && i == selected && P->phase == SP_PLACE) {
            int c[SB_N], n = sb_cells(s, c);
            for (int j = 0; j < n; j++) ui_mark_select(x0 + (c[j] % SB_N) * sq, y0 + (c[j] / SB_N) * sq, sq, sq);
        }
        put_ship(k, sq, x0 + s->x * sq, y0 + s->y * sq, s->len, s->vert, a,
                 mine && k == 0 && i == selected && P->phase == SP_PLACE);
    }
    float d = 0.46f * sq;
    for (int c = 0; c < SB_CELLS; c++) {
        int st = P->shot[pl][c];
        if (!st) continue;
        float cx = x0 + (c % SB_N + 0.5f) * sq, cy = y0 + (c / SB_N + 0.5f) * sq, a = 1, grow = 1;
        if (anim) {                             /* this salvo's pegs drop one by one */
            for (int i = 0; i < fx.n; i++) {
                if (fx.cells[i] != c) continue;
                float t = (now - fx.t0 - i * GAP_MS) / (float)DROP_MS;
                a = clampf(t * 1.6f, 0, 1);
                grow = 1 + 1.4f * (1 - clampf(t, 0, 1)) * (1 - clampf(t, 0, 1));
            }
        }
        if (a <= 0) continue;
        ui_flat_at(t_peg[k][st == SHOT_HIT], cx, cy, d * grow, a, 0);
    }
}

static void draw(void)
{
    textures();
    int m = me(), mine = P->turn == m && !ui_handing() && P->phase != SP_OVER;
    if (fx.on && !busy()) fx.on = 0;
    if (fx.on) ui_animate();
    if (P->phase == SP_PLACE) {
        draw_sea(0, m, 1, T.bx, T.by, T.sq, T.fr);
        if (drag.on && drag.moved) {            /* the ship held: where it would land, and the ship */
            const sbship *s = &P->ship[m][drag.ship];
            int x, y;
            drag_target(&x, &y);
            sbship f[SB_MAXSHIPS];
            memcpy(f, P->ship[m], sizeof f);
            f[drag.ship].x = (int8_t)x;
            f[drag.ship].y = (int8_t)y;
            int ok = sb_valid(P, f), c[SB_N], n = sb_cells(&f[drag.ship], c);
            for (int j = 0; j < n; j++) {
                float cx = T.bx + T.fr + (c[j] % SB_N) * T.sq, cy = T.by + T.fr + (c[j] / SB_N) * T.sq;
                if (ok) ui_mark_select(cx, cy, T.sq, T.sq);
                else ui_fill(cx, cy, T.sq, T.sq, 210, 40, 30, 120);
            }
            put_ship(0, T.sq, drag.x - drag.dx, drag.y - drag.dy, s->len, s->vert, 0.92f, 1);
        }
        ga_button(&btn, t_die, T.px, T.py, T.bd, 0.62f, mine ? 0 : GA_DIM);
        ga_button(&btn, t_tick, T.qx, T.py, T.bd, 0.62f, mine ? GA_ON : GA_DIM);
        return;
    }
    /* firing: theirs big (my shots), mine small (theirs) */
    draw_sea(0, !m, 0, T.bx, T.by, T.sq, T.fr);
    for (int i = 0; i < naim && mine; i++) {
        float cx = T.bx + T.fr + (aim[i] % SB_N + 0.5f) * T.sq, cy = T.by + T.fr + (aim[i] / SB_N + 0.5f) * T.sq;
        ui_put_c(t_aim, cx, cy, 0.9f * T.sq, 0.9f * T.sq);
    }
    draw_sea(1, m, 1, T.sx, T.sy, T.ssq, T.sfr);
    if (P->phase == SP_SHOOT) {
        int n = sb_shots(P), left = n - naim, ready = mine && naim == n && !busy();
        ga_button(&btn, t_fire, T.kx, T.ky, T.bd, 0.62f, ready ? GA_ON : GA_DIM);
        if (mine && n > 1) ui_number(T.ax, T.ay, 0.6f * ui.unit, left, 1);   /* the shots still to aim */
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (P->phase == SP_OVER || busy()) return;
    if (P->turn != me()) { ui_sound(SND_DENY); return; }
    if (P->phase == SP_PLACE) {
        if (ga_in(T.px, T.py, T.bd, x, y)) {
            sb_reroll(&G);
            selected = -1;
            ui_sound(SND_DICE);
            return;
        }
        if (ga_in(T.qx, T.py, T.bd, x, y)) {
            if (!sb_ready(&G)) { ui_sound(SND_DENY); return; }
            calm();
            ui_sound(SND_PLACE);
            ui_handoff(P->turn);
            return;
        }
        int c = cell_at(x, y);
        if (c < 0) return;
        int i = sb_ship_at(P, P->ship[P->turn], c);
        if (i >= 0) {                           /* a ship taken: dragged, or tapped */
            const sbship *s = &P->ship[P->turn][i];
            drag.on = 1;
            drag.ship = i;
            drag.k = s->vert ? c / SB_N - s->y : c % SB_N - s->x;
            drag.moved = 0;
            drag.x0 = drag.x = x;
            drag.y0 = drag.y = y;
            drag.dx = x - (T.bx + T.fr + s->x * T.sq);
            drag.dy = y - (T.by + T.fr + s->y * T.sq);
            return;
        }
        if (selected < 0) { ui_sound(SND_DENY); return; }
        const sbship *s = &P->ship[P->turn][selected];
        if (move_ship(selected, c % SB_N, c / SB_N, s->vert)) ui_sound(SND_PLACE);
        else ui_sound(SND_DENY);
        return;
    }
    int n = sb_shots(P);
    if (ga_in(T.kx, T.ky, T.bd, x, y)) {
        if (naim == n) fire();
        else ui_sound(SND_DENY);
        return;
    }
    int c = cell_at(x, y);
    if (c < 0) return;
    if (P->shot[!P->turn][c]) { ui_sound(SND_DENY); return; }
    for (int i = 0; i < naim; i++)
        if (aim[i] == c) {                      /* aimed already: one shot fires, a salvo's aim is taken back */
            if (n == 1) fire();
            else {
                aim[i] = aim[--naim];
                ui_sound(SND_TAP);
            }
            return;
        }
    if (naim < n) aim[naim++] = c;
    else if (n == 1) aim[0] = c;
    else { ui_sound(SND_DENY); return; }
    ui_sound(SND_TAP);
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x;
    drag.y = y;
    if (hypotf(x - drag.x0, y - drag.y0) > 0.3f * T.sq) drag.moved = 1;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    motion(x, y);
    drag.on = 0;
    int i = drag.ship;
    const sbship *s = &P->ship[P->turn][i];
    if (drag.moved) {                           /* dropped: where it lands, if it may */
        int tx, ty;
        drag_target(&tx, &ty);
        selected = i;
        if (tx == s->x && ty == s->y) return;
        ui_sound(move_ship(i, tx, ty, s->vert) ? SND_PLACE : SND_DENY);
        return;
    }
    if (selected != i) { selected = i; ui_sound(SND_TAP); return; }
    int ax = s->x + (s->vert ? 0 : drag.k), ay = s->y + (s->vert ? drag.k : 0), v = !s->vert;   /* turned round the
                                                                                               * square tapped */
    ui_sound(move_ship(i, v ? ax : ax - drag.k, v ? ay - drag.k : ay, v) ? SND_MOVE : SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the ships still afloat */
{
    int s = (int)(0.6f * ui.unit);
    if (made_boat != s) {
        made_boat = s;
        uint8_t *o = ui_buf(s, s);
        paint_ship(1, s, o);
        ui_free(&t_boat);
        t_boat = ui_tex(s, s, o);
    }
    ui_bar_count(b, t_boat, sb_afloat(P, player));
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's and the picker's cards: a sea with its fleet (the modern one, or the paper fleet's ten), pegs (the
 * salvo: a row of them) */
static void thumb(int v, int size, uint8_t *rgba)
{
    int paper = v == SB_PAPER || v == SB_PAPER_SALVO, salvo = v == SB_SALVO || v == SB_PAPER_SALVO;
    int sq = (int)(size / 10.9f), fr = (int)fmaxf(2, roundf(0.45f * sq)), side = SB_N * sq + 2 * fr;
    memset(rgba, 0, (size_t)size * size * 4);
    uint8_t *b = malloc((size_t)side * side * 4), *s = malloc((size_t)5 * sq * sq * 4),
            *r = malloc((size_t)5 * sq * sq * 4);
    if (b && s && r) {
        paint_sea(sq, fr, b);
        static const sbship modern[5] = {{1, 1, 5, 0}, {8, 2, 4, 1}, {1, 4, 3, 1}, {4, 6, 3, 0}, {6, 8, 2, 0}};
        static const sbship ten[10] = {{1, 1, 4, 0}, {7, 1, 3, 1}, {1, 4, 3, 1}, {4, 4, 2, 0}, {4, 7, 2, 1},
                                       {7, 6, 2, 0}, {8, 9, 1, 0}, {1, 9, 1, 0}, {9, 4, 1, 0}, {6, 9, 1, 0}};
        const sbship *f = paper ? ten : modern;
        for (int i = 0; i < (paper ? 10 : 5); i++) {
            paint_ship(f[i].len, sq, s);
            int w = f[i].len * sq, h = sq;
            if (f[i].vert) quarter(s, w, h, r);
            art_blend(b, side, side, f[i].vert ? r : s, f[i].vert ? h : w, f[i].vert ? w : h, fr + f[i].x * sq,
                      fr + f[i].y * sq);
        }
        int d = (int)fmaxf(3, 0.5f * sq);
        uint8_t *pg = malloc((size_t)d * d * 4);
        static const int pegs[2][7][3] = {   /* x, y, a hit */
            {{2, 1, 1}, {3, 1, 1}, {8, 3, 1}, {5, 4, 0}, {2, 8, 0}, {6, 3, 0}, {9, 8, 0}},
            {{2, 1, 1}, {3, 1, 1}, {7, 2, 1}, {5, 5, 0}, {2, 7, 0}, {3, 3, 0}, {9, 8, 0}}};
        for (int i = 0; pg && i < 7; i++) {
            const int *q = pegs[paper][i];
            art_counter(COUNTER_STONE, q[2] ? 2 : 0, d, pg);
            art_blend(b, side, side, pg, d, d, fr + q[0] * sq + (sq - d) / 2, fr + q[1] * sq + (sq - d) / 2);
        }
        free(pg);
        int a = (int)fmaxf(3, 0.9f * sq);
        uint8_t *am = salvo ? malloc((size_t)a * a * 4) : NULL;
        if (am) {                               /* a salvo: four squares aimed at once */
            paint_aim(a, am);
            for (int i = 0; i < a * a; i++) am[4 * i] = 250, am[4 * i + 1] = 214, am[4 * i + 2] = 90;
            for (int i = 0; i < 4; i++)
                art_blend(b, side, side, am, a, a, fr + (2 + i) * sq + (sq - a) / 2, fr + 9 * sq + (sq - a) / 2);
        }
        free(am);
        art_blend(rgba, size, size, b, side, side, (size - side) / 2, (size - side) / 2);
    }
    free(b);
    free(s);
    free(r);
}

static const game seabattle_game = {
    .id = "seabattle", .order = 370, .variants = SB_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .view = VIEW_HIDDEN,
};
REGISTER_GAME(seabattle_game)
