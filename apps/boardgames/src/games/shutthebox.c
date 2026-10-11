/*
 * Shut the Box (Ferme la boîte) in RetroStone Board: the wooden box, its green felt, each player's row of numbered
 * tiles on its own side, the shell's dice tumbling on the felt; tap the felt to throw, tap tiles to shut them (the
 * rules and the notation: shutthebox_rules.c, shutthebox.h). One to four players.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The box fills the board's square: the walnut frame, the plank's ledges holding the rows, the felt in the middle.
 * One or two players: a row at the bottom (and one at the top, turned round to face its player); three or four: a
 * row on every side, in a pinwheel (each row runs from a corner to the next side's ledge), the turn going clockwise.
 * Everything is drawn in the bottom player's frame, then turned q quarters clockwise round the centre to its side
 * (the sides: 0 bottom, 1 left, 2 top, 3 right). Coordinates: fractions of the box's side, from its centre.
 * A tile stands (open: its number) or lies flat in its slot (shut: its plain back, the dark well behind it); shut, it
 * tips over with a wooden clack. Tiles that cannot be part of a set making the throw are dimmed; the chosen ones lift,
 * in a gold light; the sum chosen so far shows over the throw's total, beside the dice. The set shuts as soon as it
 * makes the throw (no more could be added). A throw no set can make: a red glow on the dice, the turn's score on the
 * felt a moment, then the next player.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/shutthebox.h"
#include "shell.h"
#include "text.h"

#define FR 0.045f                               /* the walnut frame */
#define RIM 0.04f                               /* a side without a row (one or two players) */
#define LEDGE1 0.27f                            /* a row's ledge: one player (taller tiles) */
#define LEDGE2 0.2f                             /* two players */
#define LEDGE4 0.165f                           /* three or four: every side */
#define MARGIN 0.012f                           /* between a row and the ledge's ends */
#define TILE_W 0.86f                            /* a tile's width, of its slot */
#define TILE_H 0.74f                            /* standing: its height, of the ledge */
#define FLAT 0.3f                               /* lying: its height, of the standing one */
#define HINGE 0.09f                             /* the tiles' foot, from the frame, of the ledge */
#define DIE 0.085f                              /* a die */
#define TIP_MS 260                              /* a tile tipping over */
#define TIP_GAP 80                              /* the next one in the set after it */
#define PAUSE_MS 1500                           /* a throw that ends the turn: its score shown, after the dice lie */
#define PI_F 3.14159265f

/* a box's geometry: the ledge's depth, the sides' and the top's widths, the row's start and a slot's width */
typedef struct { float d, side, top, x0, sw; int tiles; } geom;

static sgame G;
static const spos *P = &G.pos;
static int S, made = -1, made_np = -1, made_tiles = -1, made_bottom = -1;
static float bx, by;                            /* the box's top left */
static SDL_Texture *t_tray, *t_face[STB_MAX_TILES + 1], *t_back;
static unsigned sel;                            /* the tiles chosen (bit t) */
static uint8_t rolls[STB_MAX_EVENTS][2];        /* the game's throws so far, kept through undo: undo never rethrows */
static int nrolls, fresh;                       /* fresh: the shell's dice show our throw */
static struct { int on, player; unsigned mask; Uint32 t0; } tip;
static struct { int on, player, landed, nd, v[2]; Uint32 t0; } dead;
static const uint8_t INK[3] = {84, 56, 36};
static const float MAPLE[3] = {236, 210, 166}, BACK[3] = {190, 146, 98}, INKF[3] = {84, 56, 36};

static geom geom_for(int players, int tiles)
{
    geom g;
    float len;
    if (players > 2) {                          /* the pinwheel: from the frame to the next side's ledge */
        g.d = g.side = g.top = LEDGE4;
        g.x0 = -0.5f + FR + MARGIN;
        len = 1 - 2 * FR - g.d - 2 * MARGIN;
    } else {
        g.d = players == 2 ? LEDGE2 : LEDGE1;
        g.side = RIM;
        g.top = players == 2 ? LEDGE2 : RIM;
        g.x0 = -0.5f + FR + RIM + MARGIN;
        len = 1 - 2 * FR - 2 * RIM - 2 * MARGIN;
    }
    g.tiles = tiles;
    g.sw = len / tiles;
    return g;
}

static geom now_geom(void) { return geom_for(P->players, P->tiles); }

/* a point turned q quarters clockwise round the centre, and back */
static void rot(int q, float *x, float *y)
{
    for (int i = 0; i < (q & 3); i++) {
        float t = *x;
        *x = -*y;
        *y = t;
    }
}
static void unrot(int q, float *x, float *y) { rot(4 - (q & 3), x, y); }

/* where a player sits on the screen: 0 the bottom, then clockwise */
static int screen_seat(int player) { return (stb_seat(player) - stb_seat(ui_bottom())) & 3; }

static float row_y(geom g) { return 0.5f - FR - HINGE * g.d; }     /* the tiles' foot */

/* the felt (the screen's frame), and seen from side q (its bounding box in that side's frame) */
static void felt(geom g, int q, float *l, float *t, float *r, float *b)
{
    float x0 = -0.5f + FR + g.side, y0 = -0.5f + FR + g.top, x1 = -x0, y1 = 0.5f - FR - g.d;
    unrot(q, &x0, &y0);
    unrot(q, &x1, &y1);
    *l = fminf(x0, x1), *r = fmaxf(x0, x1), *t = fminf(y0, y1), *b = fmaxf(y0, y1);
}

/* where side q's dice lie, in the screen's frame: on the felt towards its row. The shell lays dice side by side
 * across the screen, so the dice, the one die offered and the sum go across too, in the way `dir` (the player's
 * right at the bottom and the top, towards the middle on the sides). */
static void spot(geom g, int q, float *x, float *y, float *dir)
{
    float l, t, r, b;
    felt(g, q, &l, &t, &r, &b);
    *x = (l + r) / 2;
    *y = (t + b) / 2 + (q & 1 ? 0.1f : 0.26f) * (b - t);
    rot(q, x, y);
    *dir = q == 0 || q == 1 ? 1 : -1;
}
#define PAIR_AT(choice) ((choice) ? 0.06f : -0.04f)    /* the pair from the spot, along dir */
#define SINGLE_AT (-0.17f)                      /* the one die offered */

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int variant)
{
    stb_new(&G, variant, ui.players);
    sel = 0;
    nrolls = 0;
    fresh = 0;
    tip.on = 0;
    dead.on = 0;
}

static int status(void)
{
    if (P->phase != STB_OVER) return GS_PLAY;
    if (P->players == 1) return P->boxed ? GS_WIN0 : GS_WIN1;
    return P->winner >= 0 ? gs_win(P->winner) : GS_DRAW;
}

static int turn(void) { return P->turn; }
static int busy(void) { return tip.on || dead.on || (fresh && ui_dice_busy()); }
static unsigned winners(void) { return stb_winners(P); }     /* the tied lowest players, or the winner */

/* where a player sits, for the shell's bars (0..1, not flipped): the middle of its side; the left side's bar on top,
 * the right side's at the bottom */
static void home(int player, float *fx, float *fy)
{
    static const float at[4][2] = {{0.5f, 0.92f}, {0.08f, 0.45f}, {0.5f, 0.08f}, {0.92f, 0.55f}};
    *fx = at[stb_seat(player)][0];
    *fy = at[stb_seat(player)][1];
}

static void show_dice(int nd, const int *v, int player, int tumble)
{
    (tumble ? ui_dice_throw : ui_dice_show)(nd, v, player);
    fresh = 1;
}

static void throw_dice(int nd)
{
    int k = P->throws, me = P->turn;
    if (k >= STB_MAX_EVENTS) return;
    if (k >= nrolls) {                          /* a new throw; else thrown before an undo: the same again */
        rolls[k][0] = (uint8_t)(1 + ui_random(6));
        rolls[k][1] = (uint8_t)(1 + ui_random(6));
        nrolls = k + 1;
    } else if (!rolls[k][1]) rolls[k][1] = (uint8_t)(1 + ui_random(6));
    int v[2] = {rolls[k][0], rolls[k][1]}, r = stb_throw(&G, nd, v[0], v[1]);
    if (!r) { ui_sound(SND_DENY); return; }
    show_dice(nd, v, me, 1);
    sel = 0;
    if (r == ST_DEAD) {
        dead.on = 1;
        dead.player = me;
        dead.landed = 0;
        dead.nd = nd;
        dead.v[0] = v[0];
        dead.v[1] = v[1];
    }
}

static void shut_chosen(void)
{
    int me = P->turn, k = 0;
    unsigned mask = sel;
    if (!stb_shut(&G, mask)) { ui_sound(SND_DENY); return; }
    sel = 0;
    tip.on = 1;
    tip.player = me;
    tip.mask = mask;
    tip.t0 = ui_ms();
    for (int t = 1; t <= STB_MAX_TILES; t++)    /* a clack as each one lands */
        if (mask >> t & 1) ui_sound_later(SND_PLACE, (int)(0.8f * TIP_MS) + TIP_GAP * k++);
}

static void tap_tile(int t)
{
    unsigned b = 1u << t;
    if (!(P->open[P->turn] & b)) { ui_sound(SND_DENY); return; }
    if (sel & b) {                              /* chosen: tap again to put it back */
        sel &= ~b;
        ui_sound_soft(SND_TAP, 0.5f);
        return;
    }
    if (!(stb_usable(P, sel) & b)) { ui_sound(SND_DENY); return; }   /* too big, or no set with it */
    sel |= b;
    if (stb_sum(sel) == stb_total(P)) shut_chosen();
    else ui_sound_soft(SND_TAP, 0.5f);
}

static int undo(void)
{
    if (sel) { sel = 0; return 1; }             /* the tiles chosen first */
    if (!stb_undo(&G)) return 0;
    tip.on = 0;
    dead.on = 0;
    int v[2] = {P->die[0], P->die[1]};          /* the throw to shut again, at rest */
    show_dice(P->ndice, v, P->turn, 0);
    return 1;
}

/* "d53 8 d62 1+7 d44": throws and the tiles shut (shutthebox.h) */
static int script(const char *s)
{
    int ok = stb_script(&G, s);
    for (int i = 0; i < G.n; i++)               /* its throws are the game's from now on */
        if (G.ev[i] & STB_THROW_EV) {
            rolls[G.past[i].throws][0] = (uint8_t)(G.ev[i] >> 4 & 15);
            rolls[G.past[i].throws][1] = (uint8_t)(G.ev[i] & 15);
        }
    nrolls = P->throws;
    sel = 0;
    tip.on = 0;
    dead.on = 0;
    fresh = 0;
    if (P->phase == STB_SHUT) {
        int v[2] = {P->die[0], P->die[1]};
        show_dice(P->ndice, v, P->turn, 0);
    } else if (G.n && (G.ev[G.n - 1] & STB_THROW_EV)) {    /* the last throw ended a turn: its pause to come */
        const spos *b = &G.past[G.n - 1];
        uint16_t e = G.ev[G.n - 1];
        dead.on = 1;
        dead.player = b->turn;
        dead.landed = 0;
        dead.nd = e >> 8 & 3;
        dead.v[0] = e >> 4 & 15;
        dead.v[1] = e & 15;
        show_dice(dead.nd, dead.v, dead.player, 0);
    }
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static int px(float v, int n) { return (int)floorf((v + 0.5f) * n + 0.5f); }

/* a rectangle of the bottom side's frame turned to side q, in pixels */
static void rect_px(int q, float x0, float y0, float x1, float y1, int n, int *x, int *y, int *w, int *h)
{
    rot(q, &x0, &y0);
    rot(q, &x1, &y1);
    *x = px(fminf(x0, x1), n);
    *y = px(fminf(y0, y1), n);
    *w = px(fmaxf(x0, x1), n) - *x;
    *h = px(fmaxf(y0, y1), n) - *y;
}

static void paint_round(uint8_t *o, int n, int q, float x0, float y0, float x1, float y1, float rad,
                        const uint8_t rgb[3], float a)
{
    int x, y, w, h;
    rect_px(q, x0, y0, x1, y1, n, &x, &y, &w, &h);
    if (w < 1 || h < 1) return;
    const uint8_t fill[4] = {rgb[0], rgb[1], rgb[2], (uint8_t)(255 * a)};
    uint8_t *b = malloc((size_t)w * h * 4);
    if (!b) return;
    art_roundrect(w, h, rad * n, fill, b);
    art_blend(o, n, n, b, w, h, x, y);
    free(b);
}

static float noise(int x, int y)
{
    uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (h & 255) / 255.0f - 0.5f;
}

/* the green felt: fine fibres, darker under the ledges' walls */
static void paint_felt(uint8_t *o, int n, float l, float t, float r, float b)
{
    static const float green[3] = {40, 116, 72};
    int x0 = px(l, n), y0 = px(t, n), x1 = px(r, n), y1 = px(b, n);
    float soft = 0.018f * n;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float e = fminf(fminf((float)(x - x0), (float)(x1 - 1 - x)), fminf((float)(y - y0), (float)(y1 - 1 - y)));
            float k = (1 - 0.5f * expf(-e / soft)) * (1 + 0.07f * noise(x, y) + 0.03f * noise(x / 3, y / 2));
            uint8_t *p = o + 4 * ((size_t)y * n + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)fminf(255, green[c] * k);
            p[3] = 255;
        }
}

/* a tile, w x h: standing (num: its number, painted in ink on maple) or lying (num 0: its plain back); the token: a
 * tile of a player's colour (face, ink) */
static void tile_pic(int w, int h, int num, const float face[3], const float ink[3], uint8_t *o)
{
    static const uint8_t white[4] = {255, 255, 255, 255};
    art_roundrect(w, h, 0.16f * (w < h ? w : h), white, o);
    float lw = fmaxf(1.0f, w / 20.0f), band = num ? 0.09f * h : 0.22f * h;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            if (!p[3]) continue;
            float fx = (x + 0.5f) / w, fy = (y + 0.5f) / h;
            float k = 0.96f + 0.04f * sinf(fx * 29 + 2.5f * sinf(fy * 6 + fx * 4)) + 0.02f * noise(x, y / 4);
            if (y < band) k *= 1.13f;           /* the top edge, catching the light */
            else if (y < band + lw) k *= 0.9f;
            if (x >= w - 2 * lw) k *= 0.84f;    /* the right side, in the shade */
            else if (x < 2 * lw) k *= 1.06f;
            float d = fminf(fminf((float)x, (float)(w - 1 - x)), fminf((float)y, (float)(h - 1 - y)));
            float e = clampf(lw - d, 0, 1);     /* the outline */
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)clampf(face[c] * k * (1 - e) + ink[c] * e, 0, 255);
        }
    if (!num) return;
    int nd = num >= 10 ? 2 : 1;                 /* the number: the font's bold digits, as ui_number's */
    float dh = fminf(0.5f * h, 0.8f * w / (nd == 2 ? 1.36f : 0.62f)), dw = 0.62f * dh, step = 0.74f * dh;
    float x = (w - (step * (nd - 1) + dw)) / 2, cy = band + (h - band) * 0.48f;
    int iw = (int)(dw + 1), ih = (int)(dh + 0.5f);
    uint8_t rgb[3] = {(uint8_t)ink[0], (uint8_t)ink[1], (uint8_t)ink[2]};
    uint8_t *g = calloc((size_t)iw * ih, 4);
    if (!g) return;
    char s[3] = {(char)('0' + num / 10), (char)('0' + num % 10), 0};
    for (int i = 0; i < nd; i++, x += step) {
        memset(g, 0, (size_t)iw * ih * 4);
        text_glyph(s[nd == 2 ? i : 1], ih * 1.2f, 1, rgb, g, iw, ih);
        art_blend(o, w, h, g, iw, ih, (int)(x + 0.5f), (int)(cy - ih / 2.0f + 0.5f));
    }
    free(g);
}

/* one tile painted into a picture (n pixels square) at its slot (the bottom side's frame) */
static void blend_tile(uint8_t *o, int n, geom g, int t, int standing)
{
    float cx = g.x0 + (t - 0.5f) * g.sw, yb = row_y(g), H = TILE_H * g.d, tw = TILE_W * g.sw;
    float h = standing ? H : FLAT * H;
    int x = px(cx - tw / 2, n), y = px(yb - h, n), w = px(cx + tw / 2, n) - x, hh = px(yb, n) - y;
    uint8_t *b = malloc((size_t)w * hh * 4);
    if (!b) return;
    tile_pic(w, hh, standing ? t : 0, standing ? MAPLE : BACK, INKF, b);
    art_blend(o, n, n, b, w, hh, x, y);
    free(b);
}

/* the box: the walnut frame, the plank, the felt; each row's ledge with its wells and its rail in its player's
 * colour (players: the player on each side, -1 none) */
static void paint_box(uint8_t *o, int n, geom g, const int players[4])
{
    int fr = (int)(FR * n);
    art_board_n(1, 1, n - 2 * fr, fr, 0, o);
    float l, t, r, b;
    felt(g, 0, &l, &t, &r, &b);
    paint_felt(o, n, l, t, r, b);
    paint_round(o, n, 0, l - 0.004f, t - 0.004f, r + 0.004f, t + 0.002f, 0, INK, 0.45f);   /* the walls' edges */
    paint_round(o, n, 0, l - 0.004f, b - 0.002f, r + 0.004f, b + 0.004f, 0, INK, 0.45f);
    paint_round(o, n, 0, l - 0.004f, t, l + 0.002f, b, 0, INK, 0.45f);
    paint_round(o, n, 0, r - 0.002f, t, r + 0.004f, b, 0, INK, 0.45f);
    float len = g.sw * g.tiles, yb = row_y(g), H = TILE_H * g.d, tw = TILE_W * g.sw, rail = 0.1f * g.d;
    float y0 = 0.5f - FR - g.d;
    for (int q = 0; q < 4; q++) {
        int p = players[q];
        if (p < 0) continue;
        uint8_t col[3], out[3];
        for (int c = 0; c < 3; c++) {
            col[c] = (uint8_t)art_player[p].fill[c];
            out[c] = (uint8_t)art_player[p].outer[c];
        }
        paint_round(o, n, q, g.x0 - 0.008f, y0 + 0.004f, g.x0 + len + 0.008f, y0 + rail + 0.004f, 0.3f * rail, out,
                    1);
        paint_round(o, n, q, g.x0 - 0.006f, y0 + 0.006f, g.x0 + len + 0.006f, y0 + rail + 0.002f, 0.25f * rail, col,
                    1);
        static const uint8_t shine[3] = {255, 250, 236};
        paint_round(o, n, q, g.x0 - 0.004f, y0 + 0.008f, g.x0 + len + 0.004f, y0 + 0.008f + 0.3f * rail, 0, shine,
                    0.22f);
        paint_round(o, n, q, g.x0, yb, g.x0 + len, yb + 0.004f, 0, INK, 0.3f);    /* the hinge's groove */
        for (int k = 0; k < g.tiles; k++) {    /* the wells: dark where a tile stands, seen when it lies */
            float cx = g.x0 + (k + 0.5f) * g.sw;
            paint_round(o, n, q, cx - tw / 2, yb - H, cx + tw / 2, yb, 0.12f * tw, INK, 0.5f);
            paint_round(o, n, q, cx - tw / 2 + 0.003f, yb - H + 0.003f, cx + tw / 2 - 0.003f, yb - H + 0.012f, 0,
                        INK, 0.35f);
        }
    }
}

static void layout(box b)
{
    S = (int)floorf(b.w);
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    if (made != S || made_np != P->players || made_tiles != P->tiles || made_bottom != ui_bottom()) {
        made = S;
        made_np = P->players;
        made_tiles = P->tiles;
        made_bottom = ui_bottom();
        geom g = now_geom();
        int seats[4] = {-1, -1, -1, -1};
        for (int p = 0; p < P->players; p++) seats[screen_seat(p)] = p;
        uint8_t *o = malloc((size_t)S * S * 4);
        if (o) {
            paint_box(o, S, g, seats);
            ui_free(&t_tray);
            t_tray = ui_tex(S, S, o);
            free(o);
        }
        float yb = row_y(g), H = TILE_H * g.d, tw = TILE_W * g.sw;
        int w = px(tw / 2, S) - px(-tw / 2, S), h = px(yb, S) - px(yb - H, S), f = px(yb, S) - px(yb - FLAT * H, S);
        for (int t = 1; t <= P->tiles; t++) {
            ui_free(&t_face[t]);
            tile_pic(w, h, t, MAPLE, INKF, ui_buf(w, h));
            t_face[t] = ui_tex(w, h, ui_buf(w, h));
        }
        ui_free(&t_back);
        tile_pic(w, f, 0, BACK, INKF, ui_buf(w, f));
        t_back = ui_tex(w, f, ui_buf(w, f));
    }
    if (!ui_dice_busy()) {                      /* the shell's dice ours again (after another game) */
        int v[2] = {P->die[0], P->die[1]};
        if (dead.on) ui_dice_show(dead.nd, dead.v, dead.player);
        else if (P->phase == STB_SHUT) ui_dice_show(P->ndice, v, P->turn);
    }
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static float sx_of(float x) { return bx + (x + 0.5f) * S; }
static float sy_of(float y) { return by + (y + 0.5f) * S; }

/* a picture centred on (x, y) of side q's frame, w x h, turned with the side */
static void put_q(SDL_Texture *t, int q, float x, float y, float w, float h)
{
    rot(q, &x, &y);
    ui_put_rot(t, sx_of(x), sy_of(y), w * S, h * S, 90.0 * (q & 3));
}

/* a rectangle of side q's frame on the screen */
static box box_q(int q, float x0, float y0, float x1, float y1)
{
    rot(q, &x0, &y0);
    rot(q, &x1, &y1);
    return (box){sx_of(fminf(x0, x1)), sy_of(fminf(y0, y1)), fabsf(x1 - x0) * S, fabsf(y1 - y0) * S};
}

static void number_q(int q, float x, float y, float h, int v)
{
    rot(q, &x, &y);
    ui_number_rot(sx_of(x), sy_of(y), h * S, v, 1, 90.0 * (q & 3));
}

/* a tile's state: 1 standing, 0 lying, between: tipping over (1 → 0) */
static float tile_up(int p, int t, Uint32 now)
{
    if (P->open[p] >> t & 1) return 1;
    if (!tip.on || tip.player != p || !(tip.mask >> t & 1)) return 0;
    int k = 0;
    for (int i = 1; i < t; i++) k += tip.mask >> i & 1;
    float f = ((float)(now - tip.t0) - k * TIP_GAP) / TIP_MS;
    return 1 - clampf(f, 0, 1);
}

static void draw_row(int p, geom g, Uint32 now)
{
    int q = screen_seat(p);
    int active = P->phase == STB_SHUT && P->turn == p && !(fresh && ui_dice_busy());
    unsigned usable = active ? stb_usable(P, sel) : 0;
    float yb = row_y(g), H = TILE_H * g.d, tw = TILE_W * g.sw;
    int gold = P->boxed && P->winner == p && !tip.on;
    for (int t = 1; t <= g.tiles; t++) {
        float cx = g.x0 + (t - 0.5f) * g.sw, up = tile_up(p, t, now);
        int chosen = active && (sel >> t & 1);
        if (gold) {
            float x = cx, y = yb - 0.5f * H;
            rot(q, &x, &y);
            ui_mark_gold(sx_of(x), sy_of(y), 1.9f * g.sw * S);
        }
        if (up <= 0) {                          /* lying in its slot, its back up */
            box s = box_q(q, cx - tw / 2, yb - FLAT * H, cx + tw / 2, yb);
            ui_shadow_flat(s.x + s.w / 2, s.y + s.h / 2, s.w * 1.1f, s.h * 1.1f, 0.35f);
            put_q(t_back, q, cx, yb - FLAT * H / 2, tw, FLAT * H);
            continue;
        }
        float lift = chosen ? 0.12f * H : 0;
        if (up >= 1) {
            if (chosen) {
                box m = box_q(q, cx - g.sw / 2, yb - H - lift - 0.06f * H, cx + g.sw / 2, yb + 0.02f * H);
                ui_mark_select(m.x, m.y, m.w, m.h);
            }
            box s = box_q(q, cx - tw / 2, yb - H - lift, cx + tw / 2, yb - lift);
            ui_shadow_flat(s.x + s.w / 2, s.y + s.h / 2, s.w * 1.2f, s.h * 1.06f, chosen ? 0.8f : 0.6f);
            int dim = active && !chosen && !(usable >> t & 1);
            if (dim) SDL_SetTextureColorMod(t_face[t], 128, 118, 104);
            put_q(t_face[t], q, cx, yb - lift - H / 2, tw, H);
            SDL_SetTextureColorMod(t_face[t], 255, 255, 255);
            continue;
        }
        float e = 1 - up, a = e * e * PI_F / 2; /* tipping over: falling faster and faster about its foot */
        float h = FLAT * H + (1 - FLAT) * H * cosf(a);
        SDL_Texture *tex = a < 1.2f ? t_face[t] : t_back;
        Uint8 k = (Uint8)(255 * (1 - 0.4f * sinf(a)));
        box s = box_q(q, cx - tw / 2, yb - h, cx + tw / 2, yb);
        ui_shadow_flat(s.x + s.w / 2, s.y + s.h / 2, s.w * 1.15f, s.h * 1.06f, 0.5f);
        SDL_SetTextureColorMod(tex, k, k, k);
        put_q(tex, q, cx, yb - h / 2, tw, h);
        SDL_SetTextureColorMod(tex, 255, 255, 255);
    }
}

static void draw_dice(geom g, Uint32 now)
{
    float ds = DIE * S, sx, sy, dir;
    if (dead.on && dead.landed && now - dead.t0 >= PAUSE_MS) dead.on = 0;   /* the pause over: the next player */
    if (dead.on) {                              /* a throw no set can make: a red glow, the turn's score, a pause */
        int q = screen_seat(dead.player);
        spot(g, q, &sx, &sy, &dir);
        if (!ui_dice_busy() && !dead.landed) {
            dead.landed = 1;
            dead.t0 = now;
            ui_sound(SND_DENY);
        }
        float x = sx + dir * PAIR_AT(0);
        if (dead.landed) ui_mark_glow(sx_of(x), sy_of(sy), 2.9f * ds);
        ui_dice_draw(sx_of(x), sy_of(sy), ds, 0);
        if (dead.landed) {                      /* the score, big, in the middle of the felt, facing the player */
            float l, t, r, b;
            felt(g, q, &l, &t, &r, &b);
            number_q(q, (l + r) / 2, (t + b) / 2 - 0.12f * (b - t), 0.09f, stb_score(P, dead.player));
        }
        ui_animate();
        return;
    }
    if (P->phase == STB_OVER) return;
    int q = screen_seat(P->turn);
    spot(g, q, &sx, &sy, &dir);
    if (P->phase == STB_THROW) {                /* the dice to throw, lit in gold; the one die offered beside */
        int choice = stb_one_die(P);
        float x = sx + dir * PAIR_AT(choice);
        ui_mark_gold(sx_of(x), sy_of(sy), 3.4f * ds);
        ui_die_at(sx_of(x) - 0.65f * ds, sy_of(sy), ds, P->turn, 5, 0.9f);
        ui_die_at(sx_of(x) + 0.65f * ds, sy_of(sy), ds, P->turn, 3, 0.9f);
        if (choice) {
            x = sx + dir * SINGLE_AT;
            ui_mark_gold(sx_of(x), sy_of(sy), 2.3f * ds);
            ui_die_at(sx_of(x), sy_of(sy), ds, P->turn, 4, 0.9f);
        }
        return;
    }
    ui_dice_draw(sx_of(sx + dir * PAIR_AT(0)), sy_of(sy), ds, 0);  /* the throw, and beside it the sum chosen over its
                                                                     * total, facing the player */
    if (fresh && ui_dice_busy()) return;
    float fx = sx + dir * (PAIR_AT(0) + (P->ndice > 1 ? 1.15f : 0.5f) * DIE + 0.07f), fy = sy;
    unrot(q, &fx, &fy);
    if (!sel) {
        number_q(q, fx, fy, 0.07f, stb_total(P));
        return;
    }
    float x = fx, y = fy - 0.036f;
    rot(q, &x, &y);
    ui_mark_gold(sx_of(x), sy_of(y), 0.12f * S);
    number_q(q, fx, fy - 0.036f, 0.052f, stb_sum(sel));
    box line = box_q(q, fx - 0.032f, fy - 0.003f, fx + 0.032f, fy + 0.003f);
    ui_fill(line.x, line.y, line.w, line.h, 246, 238, 220, 255);
    number_q(q, fx, fy + 0.036f, 0.052f, stb_total(P));
}

static void draw(void)
{
    geom g = now_geom();
    Uint32 now = ui_ms();
    ui_put(t_tray, bx, by, (float)S, (float)S);
    if (tip.on) {                               /* the tiles tipping over: done when the last one lies */
        int last = 0;
        for (int t = 1; t <= STB_MAX_TILES; t++) last += tip.mask >> t & 1;
        if ((float)(now - tip.t0) >= TIP_MS + (last - 1) * TIP_GAP) tip.on = 0;
        else ui_animate();
    }
    for (int p = 0; p < P->players; p++) draw_row(p, g, now);
    draw_dice(g, now);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (P->phase == STB_OVER || (fresh && ui_dice_busy())) return;
    if (dead.on) {                              /* a tap ends the pause */
        if (dead.landed) dead.on = 0;
        return;
    }
    geom g = now_geom();
    float u = (x - bx) / S - 0.5f, v = (y - by) / S - 0.5f, lu = u, lv = v, sx, sy, dir;
    int q = screen_seat(P->turn);
    if (P->phase == STB_THROW) {                /* the felt (or the dice on it) throws */
        spot(g, q, &sx, &sy, &dir);
        if (stb_one_die(P) && hypotf(u - (sx + dir * SINGLE_AT), v - sy) < 1.1f * DIE) { throw_dice(1); return; }
        float l, t, r, b;
        felt(g, 0, &l, &t, &r, &b);
        if (u >= l && u <= r && v >= t && v <= b) throw_dice(2);
        else ui_sound(SND_DENY);
        return;
    }
    unrot(q, &lu, &lv);                         /* a tile of the player's row, in its own frame */
    int t = (int)floorf((lu - g.x0) / g.sw) + 1;
    if (lv >= 0.5f - FR - g.d - 0.02f && lv <= 0.5f - FR + 0.01f && lu >= g.x0 && t >= 1 && t <= g.tiles) tap_tile(t);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the player's score: the sum of their open tiles */
{
    float h = 0.5f * ui.unit;
    int s = stb_score(P, player), n = s >= 10 ? 2 : 1;
    ui_number(b.x + (0.74f * (n - 1) + 0.62f) * h / 2, b.y + b.h / 2, h, s, 1);
}

/* a tile of the player's colour, standing */
static void token(int player, int size, uint8_t *rgba)
{
    int w = (int)(0.5f * size), h = (int)(0.86f * size);
    float face[3], ink[3];
    for (int c = 0; c < 3; c++) {
        face[c] = art_player[player].fill[c];
        ink[c] = art_player[player].outer[c];
    }
    uint8_t *b = malloc((size_t)w * h * 4);
    if (!b) return;
    tile_pic(w, h, 0, face, ink, b);
    art_blend(rgba, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    free(b);
}

/* the menu's card and the picker's: one row of the variant's tiles, a few shut, two dice on the felt */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int tiles = stb_tiles(variant < 0 ? STB_NINE : variant);
    geom g = {0.4f, 0.02f, 0.02f, 0, 0, tiles};
    g.x0 = -0.5f + FR + g.side + 0.008f;
    g.sw = (1 - 2 * FR - 2 * g.side - 0.016f) / tiles;
    static const int none[4] = {-1, -1, -1, -1};
    int players[4];
    memcpy(players, none, sizeof players);
    players[0] = 0;
    paint_box(rgba, size, g, players);
    unsigned shut = tiles == 9 ? 1u << 2 | 1u << 7 : tiles == 10 ? 1u << 3 | 1u << 8 : 1u << 4 | 1u << 9;
    for (int t = 1; t <= tiles; t++) blend_tile(rgba, size, g, t, !(shut >> t & 1));
    int d = (int)(0.2f * size);
    uint8_t *b = malloc((size_t)d * d * 4), *sh = malloc((size_t)d * d * 4);
    if (b && sh) {
        static const int face[2] = {5, 4};
        for (int i = 0; i < 2; i++) {
            int x = px(-0.13f + 0.27f * i, size) - d / 2, y = px(-0.14f + 0.05f * i, size) - d / 2;
            art_flat_shadow(d, sh);
            art_blend(rgba, size, size, sh, d, d, x + d / 14, y + d / 10);
            art_die(face[i], 0, d, b);
            art_blend(rgba, size, size, b, d, d, x, y);
        }
    }
    free(b);
    free(sh);
}

static const game shutthebox_game = {
    .id = "shutthebox", .order = 152, .variants = STB_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1u << 1 | 1u << 2 | 1u << 3 | 1u << 4, .home = home,
};
REGISTER_GAME(shutthebox_game)
