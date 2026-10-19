/*
 * Train Switches in RetroStone Board, for one player: a toy railway on a green mat in the house's frame. The track is
 * beech wood with two grooves; a switch shows its way with a brass lever (the other branch darker, a red stop block
 * across it), a toggle switch has a brass disc with an arrow going round. Stations are little houses roofed in their
 * colour, the portals brick arches at the board's edge, the tunnels green hills with a stone mouth and their pair's
 * number. The engines are painted wooden toys seen from above (a coloured boiler, a dark cab roof, a black chimney),
 * their order on the cab when they run one by one (the rules: trains_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a switch to flip it (its lever swings); the arrow tool sends the trains: they run square by square, puffing,
 * toggles flipping behind them, each one stopping at its station (a gold light) or going wrong (a red glow: the wrong
 * station, a crash, a dead end), and then they are back at their portals for another go. A tap while they run speeds
 * them up. The lamp: the hint (a switch to flip lit in gold; or the arrow on the gold disc: the switches are right).
 * The bar: the flips, and at its right end the par by a flag. Solved: the stations flare gold; perfect (the par, no
 * hint, first time): the light runs along the track from the portals.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/trains.h"
#include "shell.h"

#define FRAME 0.035f
#define TICK_MS 300.0f                          /* a square, running */
#define FAST_MS 90.0f                           /* sped up by a tap */
#define FLIP_MS 170
#define HOLD_MS 1200                            /* a run gone wrong: shown, then the trains are back */
#define FLARE_MS 900
#define WAVE_STEP 45
#define WAVE_LEN 520
#define ENGINE 0.74f                            /* an engine's length, of a square */
#define PUFF_EVERY 0.32f                        /* ticks between two puffs */
#define PUFFS 6

enum { T_GO, T_HINT, TOOLS };
static const int COL[TRS_COLOURS] = {2, 5, 4, 3, 0};   /* red lacquer, lapis, amber, jade, ivory */
static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
static const uint8_t BEECH[3] = {228, 188, 130}, GROOVE[3] = {150, 104, 62}, BRASS[3] = {214, 170, 84},
                     IRON[3] = {46, 40, 36}, STOP[3] = {176, 52, 38}, SHADOW[3] = {0, 0, 0};

static trs_game G;
static trs_run R;
static struct {
    int on;                                     /* a run shown (on after it ended well: the trains at their stations) */
    Uint32 t0;
    float tau0, ms;                             /* tick tau0 at t0, ms a tick from there */
    uint16_t set;                               /* the switches it started from */
    float heard;                                /* the ticks whose sounds were played */
} run;
static Uint32 flip_t0[TRS_MAXS];
static uint16_t flip_from;                      /* each switch's setting before its last flip (the lever swinging) */
static int hint_sw = -1, hint_go, down = -1, serial;
static int park[TRS_MAXT];                      /* trains stopped at a station before this one there: it waits so
                                                 * many squares back, one behind the other */
static float wave[TRS_CELLS];                   /* a square's distance along the track from the portals */

static box last_box;
static float ox, oy;
static int fr, cell, bw, bh, made_cell = -1, made_serial = -1, made_small = -1;
static SDL_Texture *t_board, *t_sw[TRS_MAXS][2], *t_engine[TRS_COLOURS], *t_toggle, *t_par;

static void colour_of(int k, uint8_t rgb[3])
{
    const float *f = art_player[COL[k % TRS_COLOURS]].fill;
    for (int i = 0; i < 3; i++) rgb[i] = (uint8_t)f[i];
}

static int low_side(unsigned v)
{
    for (int d = 0; d < 4; d++)
        if (v >> d & 1) return d;
    return 0;
}

/* ---- the track's geometry: a point of square (x0, y0, sq) on the way from side a's middle to side b's, u 0..1 ---- */
static void piece_at(float x0, float y0, float sq, int a, int b, float u, float *x, float *y)
{
    float cx = x0 + sq / 2, cy = y0 + sq / 2;
    if (b == ((a + 2) & 3)) {
        *x = cx + 0.5f * sq * (DX[a] + (DX[b] - DX[a]) * u);
        *y = cy + 0.5f * sq * (DY[a] + (DY[b] - DY[a]) * u);
        return;
    }
    float kx = cx + 0.5f * sq * (DX[a] + DX[b]), ky = cy + 0.5f * sq * (DY[a] + DY[b]);
    float aa = atan2f(cy + 0.5f * sq * DY[a] - ky, cx + 0.5f * sq * DX[a] - kx);
    float ab = atan2f(cy + 0.5f * sq * DY[b] - ky, cx + 0.5f * sq * DX[b] - kx);
    float d = ab - aa;
    while (d > PUZ_PI) d -= 2 * PUZ_PI;
    while (d < -PUZ_PI) d += 2 * PUZ_PI;
    *x = kx + 0.5f * sq * cosf(aa + d * u);
    *y = ky + 0.5f * sq * sinf(aa + d * u);
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a beech rail piece in square (x0, y0, sq) of a picture: side a to side b (straight pieces up to t1 of the way),
 * shade 1 lit (less: darker), its grooves; shadow: its soft shadow instead */
static void rail(uint8_t *o, int W, int H, float x0, float y0, float sq, int a, int b, float t1, float shade,
                 int shadow)
{
    float hw = 0.21f * sq, g = 0.085f * sq, gw = 0.021f * sq, sh = shadow ? 0.035f * sq : 0;
    int straight = b == ((a + 2) & 3);
    float cx = x0 + sq / 2, cy = y0 + sq / 2;
    float kx = cx + 0.5f * sq * (DX[a] + DX[b]), ky = cy + 0.5f * sq * (DY[a] + DY[b]);
    float ax = cx + 0.5f * sq * DX[a], ay = cy + 0.5f * sq * DY[a];
    for (int y = (int)floorf(y0); y < (int)ceilf(y0 + sq); y++)
        for (int x = (int)floorf(x0); x < (int)ceilf(x0 + sq); x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            float px = x + 0.5f - sh, py = y + 0.5f - 1.2f * sh, off, along = 0;
            if (straight) {
                along = ((px - ax) * DX[b] + (py - ay) * DY[b]) / sq;
                off = (px - cx) * -DY[b] + (py - cy) * DX[b];
                if (along > t1 || along < -0.02f) continue;
            } else off = hypotf(px - kx, py - ky) - 0.5f * sq;
            float d = fabsf(off);
            if (d > hw + 1) continue;
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            float cov = clampf(hw + 0.5f - d, 0, 1);
            if (straight && t1 < 1) cov *= clampf((t1 - along) * sq + 0.5f, 0, 1);
            if (shadow) { pzk_over(p, SHADOW, 0.28f * cov); continue; }
            float grain = 1 + 0.045f * sinf(off / sq * 70 + 3 * art_hash(x / 7, y / 7)) + 0.02f * art_hash(x, y);
            float edge = clampf((d - (hw - 0.045f * sq)) / (0.045f * sq), 0, 1);
            float lit = off * (straight ? 1 : -1) < 0 ? 1.06f : 0.92f;   /* one edge in the light */
            float k = shade * grain * (1 - 0.3f * edge * edge) * (edge > 0 ? lit : 1);
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(BEECH[i] * k, 0, 255);
            float gc = clampf(gw + 0.5f - fabsf(d - g), 0, 1);
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)(c[i] + (GROOVE[i] * shade - c[i]) * gc);
            pzk_over(p, c, cov);
        }
}

static void rail_full(uint8_t *o, int W, int H, float x0, float y0, float sq, int a, int b, float t1, float shade)
{
    rail(o, W, H, x0, y0, sq, a, b, t1, shade, 1);
    rail(o, W, H, x0, y0, sq, a, b, t1, shade, 0);
}

/* local coordinates of a pixel in a square: s along the way from side `a` (-0.5) to the far side (0.5), t across */
static void local_st(float px, float py, float cx, float cy, float sq, int a, float *s, float *t)
{
    int f = (a + 2) & 3;
    *s = ((px - cx) * DX[f] + (py - cy) * DY[f]) / sq;
    *t = ((px - cx) * -DY[f] + (py - cy) * DX[f]) / sq;
}

/* a station's house at the far side of its square (open by side a): a hipped roof in its colour, its eaves' shadow,
 * a platform by the track and a buffer stop at the track's end */
static void paint_station(uint8_t *o, int W, int H, float x0, float y0, float sq, int a, int k)
{
    float cx = x0 + sq / 2, cy = y0 + sq / 2;
    rail_full(o, W, H, x0, y0, sq, a, (a + 2) & 3, 0.62f, 1);
    uint8_t col[3];
    colour_of(k, col);
    static const uint8_t PLATFORM[3] = {196, 186, 168};
    for (int y = (int)y0; y < (int)(y0 + sq); y++)
        for (int x = (int)x0; x < (int)(x0 + sq); x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            float s, t;
            local_st(x + 0.5f, y + 0.5f, cx, cy, sq, a, &s, &t);
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            /* the platform: a pale strip across, under the house */
            float dp = fmaxf(fabsf(s - 0.24f) - 0.2f, fabsf(t) - 0.46f) * sq;
            if (dp < 1) pzk_over(p, PLATFORM, clampf(0.5f - dp, 0, 1));
            /* the eaves' shadow, then the roof */
            float hs = 0.15f, ht = 0.4f;
            float ds = fmaxf(fabsf(s - 0.3f - 0.03f) - hs, fabsf(t - 0.03f) - ht) * sq;
            if (ds < 2) pzk_over(p, SHADOW, 0.3f * clampf(0.5f - ds / 2, 0, 1));
            float ls = s - 0.3f, dr = fmaxf(fabsf(ls) - hs, fabsf(t) - ht) * sq;
            if (dr > 1) continue;
            float es = hs - fabsf(ls), et = ht - fabsf(t), sh;   /* a hipped roof: four slopes */
            if (es < et) sh = ls < 0 ? 1.12f : 0.72f;
            else sh = t < 0 ? 0.98f : 0.86f;
            if (fabsf(es - et) * sq < 0.9f || (es > et && fabsf(ls) * sq < 0.6f && et > hs)) sh *= 0.8f;   /* ridges */
            float rim = clampf(1 - fminf(es, et) * sq / 1.6f, 0, 1);
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(col[i] * sh * (1 - 0.35f * rim), 0, 255);
            float tile = 0.92f + 0.08f * (sinf(fminf(es, et) * sq * 1.1f) > 0);   /* rows of tiles */
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)(c[i] * tile);
            pzk_over(p, c, clampf(0.5f - dr, 0, 1));
        }
    /* a chimney on the roof, the buffer stop at the track's end */
    float chx = cx + (0.32f * DX[(a + 2) & 3] - 0.22f * DY[(a + 2) & 3]) * sq;
    float chy = cy + (0.32f * DY[(a + 2) & 3] + 0.22f * DX[(a + 2) & 3]) * sq;
    art_roundrect_put(o, W, H, (int)(chx - 0.05f * sq), (int)(chy - 0.05f * sq), (int)(0.1f * sq), (int)(0.1f * sq),
                      0.02f * sq, (const uint8_t[3]){120, 64, 50}, 1);
    float bx = cx + 0.12f * sq * DX[(a + 2) & 3], by = cy + 0.12f * sq * DY[(a + 2) & 3];
    float nx = -DY[a], ny = DX[a];
    pzk_stroke(o, W, H, bx - nx * 0.2f * sq, by - ny * 0.2f * sq, bx + nx * 0.2f * sq, by + ny * 0.2f * sq,
               0.045f * sq, STOP, 1);
}

/* an arch at the far side of a square open by side a: brick (a portal at the board's edge) or stone in a green hill
 * (a tunnel mouth), its mouth dark where the track goes in */
static void paint_arch(uint8_t *o, int W, int H, float x0, float y0, float sq, int a, int brick)
{
    float cx = x0 + sq / 2, cy = y0 + sq / 2;
    rail_full(o, W, H, x0, y0, sq, a, (a + 2) & 3, 0.95f, 1);
    static const uint8_t HILL[3] = {58, 128, 70}, STONE[3] = {156, 150, 140}, BRICK[3] = {170, 82, 58},
                         DARK[3] = {22, 18, 16};
    for (int y = (int)y0; y < (int)(y0 + sq); y++)
        for (int x = (int)x0; x < (int)(x0 + sq); x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            float s, t;
            local_st(x + 0.5f, y + 0.5f, cx, cy, sq, a, &s, &t);
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            if (!brick) {                       /* the hill: a mound behind the face */
                float hx = (s - 0.3f) / 0.19f, ht = t / 0.46f, r = hypotf(hx, ht);
                if (r < 1.05f && s > 0.05f) {
                    float k = 0.8f + 0.35f * (1 - r) - 0.15f * hx;
                    uint8_t c[3];
                    for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(HILL[i] * k, 0, 255);
                    pzk_over(p, c, clampf((1.05f - r) * sq * 0.15f, 0, 1));
                }
            }
            /* the face: a band across, its mouth in the middle */
            float df = fmaxf(fabsf(s - 0.36f) - 0.07f, fabsf(t) - 0.36f) * sq;
            if (df < 1) {
                const uint8_t *base = brick ? BRICK : STONE;
                int row = (int)floorf((s - 0.29f) * sq / (0.047f * sq));
                float jt = t * sq / (0.1f * sq) + (row & 1) * 0.5f;
                float joint = fabsf(jt - roundf(jt)) < 0.06f || fabsf(fmodf((s - 0.29f) / 0.047f + 4, 1)) < 0.08f;
                uint8_t c[3];
                float k = (joint ? 0.72f : 1) * (0.92f + 0.12f * art_hash((int)floorf(jt), row));
                for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(base[i] * k, 0, 255);
                pzk_over(p, c, clampf(0.5f - df, 0, 1));
            }
            float dm = fmaxf(fabsf(s - 0.4f) - 0.12f, fabsf(t) - 0.21f) * sq;   /* the mouth, darker inside */
            if (dm < 1 && s > 0.28f) pzk_over(p, DARK, clampf(0.5f - dm, 0, 1) * clampf(0.6f + (s - 0.29f) * 4, 0, 1));
        }
}

/* the board: the frame, the mat, the fixed track, the stations, portals and tunnels; (w sq + 2 f) x (h sq + 2 f) */
static void paint_board(const trs_map *m, int sq, int f, uint8_t *o)
{
    int W = m->w * sq + 2 * f, H = m->h * sq + 2 * f;
    art_board_n(m->w, m->h, sq, f, 0, o);
    art_felt(o, W, H, f, f, W - f, H - f);
    for (int c = 0; c < m->w * m->h; c++) {     /* tufts of grass, here and there */
        float x0 = f + c % m->w * sq, y0 = f + c / m->w * sq;
        for (int i = 0; i < 3; i++) {
            float tx = x0 + sq * (0.15f + 0.7f * art_hash(c * 3 + i, 11)), ty = y0 + sq * (0.15f + 0.7f * art_hash(c, i + 29));
            static const uint8_t TUFT[3] = {70, 150, 84};
            pzk_stroke(o, W, H, tx, ty, tx - 0.03f * sq, ty - 0.06f * sq, 0.012f * sq, TUFT, 0.35f);
            pzk_stroke(o, W, H, tx, ty, tx + 0.03f * sq, ty - 0.055f * sq, 0.012f * sq, TUFT, 0.35f);
        }
    }
    for (int pass = 0; pass < 2; pass++)        /* the shadows first, then the wood */
        for (int c = 0; c < m->w * m->h; c++) {
            float x0 = f + c % m->w * sq, y0 = f + c / m->w * sq;
            int op = m->open[c], a = low_side(op);
            switch (m->kind[c]) {
            case TRS_TRACK: {
                int b = low_side(op & ~(1u << a));
                rail(o, W, H, x0, y0, sq, a, b, 1, 1, !pass);
                break;
            }
            case TRS_CROSS:
                rail(o, W, H, x0, y0, sq, TRS_UP, TRS_DOWN, 1, 1, !pass);
                rail(o, W, H, x0, y0, sq, TRS_LEFT, TRS_RIGHT, 1, 1, !pass);
                break;
            default: break;
            }
        }
    for (int c = 0; c < m->w * m->h; c++) {
        float x0 = f + c % m->w * sq, y0 = f + c / m->w * sq;
        int a = low_side(m->open[c]);
        if (m->kind[c] == TRS_STATION) paint_station(o, W, H, x0, y0, sq, a, m->k[c]);
        else if (m->kind[c] == TRS_PORTAL) paint_arch(o, W, H, x0, y0, sq, a, 1);
        else if (m->kind[c] == TRS_TUNNEL) paint_arch(o, W, H, x0, y0, sq, a, 0);
    }
}

/* a switch's square set one way, on a clear square picture (or in a board at x0, y0): the other branch darker with a
 * red stop block across it, the way it is set on top */
static void paint_switch(uint8_t *o, int W, int H, float x0, float y0, float sq, const trs_map *m, int c, int setting)
{
    int stem = m->k[c], on = trs_branch(m, c, setting), off = trs_branch(m, c, !setting);
    rail(o, W, H, x0, y0, sq, stem, off, 1, 0.8f, 1);
    rail(o, W, H, x0, y0, sq, stem, on, 1, 1, 1);
    rail(o, W, H, x0, y0, sq, stem, off, 1, 0.74f, 0);
    float px, py, qx, qy;
    piece_at(x0, y0, sq, stem, off, 0.66f, &px, &py);
    piece_at(x0, y0, sq, stem, off, 0.70f, &qx, &qy);
    float dx = qx - px, dy = qy - py, l = hypotf(dx, dy) + 1e-6f, nx = -dy / l * 0.19f * sq, ny = dx / l * 0.19f * sq;
    pzk_stroke(o, W, H, px - nx, py - ny, px + nx, py + ny, 0.05f * sq, STOP, 1);
    rail(o, W, H, x0, y0, sq, stem, on, 1, 1, 0);
}

/* an engine seen from above, painted at (cx, cy), len long, heading deg: a dark chassis and wheels, the cab, the
 * boiler in its colour (round: lit along its top), brass bands and dome, the chimney, a red buffer beam */
static void paint_engine(uint8_t *o, int W, int H, float cx, float cy, float len, float deg, int k)
{
    uint8_t col[3];
    colour_of(k, col);
    float ca = cosf(deg * PUZ_PI / 180), sa = sinf(deg * PUZ_PI / 180), R0 = 0.56f * len;
    static const uint8_t CHASSIS[3] = {52, 44, 40}, WHEEL[3] = {26, 24, 22}, SMOKE[3] = {64, 60, 58};
    for (int y = (int)(cy - R0); y <= (int)(cy + R0); y++)
        for (int x = (int)(cx - R0); x <= (int)(cx + R0); x++) {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float lx = (dx * ca + dy * sa) / len, ly = (-dx * sa + dy * ca) / len, px = 1 / len;
            uint8_t *p = o + 4 * ((size_t)y * W + x);
#define BOX(x0_, x1_, hy_, r_) (fmaxf(fabsf(lx - ((x0_) + (x1_)) / 2) - ((x1_) - (x0_)) / 2 + (r_), fabsf(ly) - (hy_) + (r_)) - (r_))
#define COVER(d_) clampf(0.5f - (d_) / px, 0, 1)
            float d;
            static const float WX[3] = {-0.3f, -0.03f, 0.24f};
            for (int i = 0; i < 3; i++) {       /* the wheels, peeping out at the sides */
                d = fmaxf(fabsf(lx - WX[i]) - 0.08f, fabsf(fabsf(ly) - 0.25f) - 0.05f);
                if (d < px) pzk_over(p, WHEEL, COVER(d));
            }
            if ((d = BOX(-0.48f, 0.47f, 0.24f, 0.05f)) < px) pzk_over(p, CHASSIS, COVER(d));
            if ((d = BOX(-0.46f, -0.11f, 0.23f, 0.04f)) < px) {   /* the cab: its roof, lit at one edge */
                float rim = clampf(1 - (0.23f - fabsf(ly)) / 0.04f, 0, 1);
                uint8_t c[3];
                for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(col[i] * (0.62f + 0.3f * (ly < 0) * rim), 0, 255);
                pzk_over(p, c, COVER(d));
                if (fabsf(lx + 0.28f) < 0.1f && fabsf(ly) < 0.15f) {   /* its roof's middle, raised */
                    uint8_t r2[3];
                    for (int i = 0; i < 3; i++) r2[i] = (uint8_t)clampf(col[i] * 0.78f, 0, 255);
                    pzk_over(p, r2, COVER(BOX(-0.38f, -0.18f, 0.15f, 0.03f)));
                }
            }
            if ((d = BOX(-0.12f, 0.39f, 0.165f, 0.02f)) < px) {   /* the boiler */
                float q = ly / 0.165f, sh = 0.62f + 0.5f * (1 - q * q) + 0.2f * clampf(1 - fabsf(q + 0.35f) / 0.2f, 0, 1);
                uint8_t c[3];
                for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(col[i] * sh + 18 * (sh > 1.15f), 0, 255);
                if (fabsf(lx - 0.02f) < 0.016f || fabsf(lx - 0.22f) < 0.016f)
                    for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(BRASS[i] * sh, 0, 255);
                pzk_over(p, c, COVER(d));
            }
            if ((d = BOX(0.39f, 0.46f, 0.15f, 0.02f)) < px) pzk_over(p, SMOKE, COVER(d));
            if ((d = hypotf(lx - 0.09f, ly) - 0.055f) < px) {   /* the dome */
                uint8_t c[3];
                float sh = 1.05f - 2.5f * (ly + 0.02f);
                for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(BRASS[i] * sh, 0, 255);
                pzk_over(p, c, COVER(d));
            }
            if ((d = hypotf(lx - 0.31f, ly) - 0.068f) < px) {   /* the chimney, hollow */
                pzk_over(p, WHEEL, COVER(d));
                float di = hypotf(lx - 0.31f, ly) - 0.042f;
                if (di < px) pzk_over(p, (const uint8_t[3]){8, 6, 6}, COVER(di));
            }
            if ((d = BOX(0.46f, 0.495f, 0.21f, 0.008f)) < px) pzk_over(p, STOP, COVER(d));
#undef BOX
#undef COVER
        }
}

/* a toggle switch's disc: brass, an arrow going round it */
static void paint_toggle(int k, int size, uint8_t *o)
{
    (void)k;
    float c = size / 2.0f;
    pzk_ball(o, size, size, c, c, 0.48f * size, IRON, 1, 0.5f);
    pzk_ball(o, size, size, c, c, 0.42f * size, BRASS, 1, 0.9f);
    pzk_arc(o, size, size, c, c, 0.2f * size, 0.27f * size, 30, 320, IRON, 1);
    float a = 30 * PUZ_PI / 180, r = 0.235f * size, hx = c + r * cosf(a), hy = c + r * sinf(a);
    pzk_stroke(o, size, size, hx, hy, hx - 0.13f * size, hy - 0.02f * size, 0.04f * size, IRON, 1);
    pzk_stroke(o, size, size, hx, hy, hx + 0.02f * size, hy - 0.13f * size, 0.04f * size, IRON, 1);
}

static void token(int player, int size, uint8_t *rgba)   /* an engine */
{
    (void)player;
    paint_engine(rgba, size, size, size / 2.0f, size / 2.0f, 0.9f * size, -30, 0);
}

static void make_tex(SDL_Texture **t, int size, void (*paint)(int, int, uint8_t *), int k)
{
    ui_free(t);
    memset(ui_buf(size, size), 0, (size_t)size * size * 4);
    paint(k, size, ui_buf(size, size));
    *t = ui_tex(size, size, ui_buf(size, size));
}

static void engine_tex(int k, int size, uint8_t *o) { paint_engine(o, size, size, size / 2.0f, size / 2.0f, size * 0.98f, 0, k); }

static void plan_wave(void)                     /* the squares' distances along the track from the portals */
{
    int q[TRS_CELLS], n = 0, W = G.m.w * G.m.h;
    for (int c = 0; c < W; c++) wave[c] = -1;
    for (int i = 0; i < G.m.ne; i++) { wave[G.m.portal[i]] = 0; q[n++] = G.m.portal[i]; }
    for (int h = 0; h < n; h++) {
        int c = q[h];
        for (int d = 0; d < 4; d++) {
            int b = trs_neighbour(&G.m, c, d);
            if (!(G.m.open[c] >> d & 1) || b < 0 || wave[b] >= 0) continue;
            wave[b] = wave[c] + 1;
            q[n++] = b;
        }
        if (G.m.kind[c] == TRS_TUNNEL)
            for (int t = 0; t < W; t++)
                if (t != c && G.m.kind[t] == TRS_TUNNEL && G.m.k[t] == G.m.k[c] && wave[t] < 0) {
                    wave[t] = wave[c] + 1;
                    q[n++] = t;
                }
    }
}

static void layout(box b)
{
    last_box = b;
    fr = (int)(fminf(b.w, b.h) * FRAME);
    cell = (int)fminf((b.w - 2 * fr) / G.m.w, (b.h - 2 * fr) / G.m.h);
    bw = G.m.w * cell + 2 * fr;
    bh = G.m.h * cell + 2 * fr;
    ox = floorf(b.x + (b.w - bw) / 2) + fr;
    oy = floorf(b.y + (b.h - bh) / 2) + fr;
    if (made_cell != cell || made_serial != serial) {
        int fresh = made_cell != cell || !t_toggle;
        made_cell = cell;
        made_serial = serial;
        ui_free(&t_board);
        uint8_t *o = malloc((size_t)bw * bh * 4);
        if (o) {
            paint_board(&G.m, cell, fr, o);
            t_board = ui_tex(bw, bh, o);
            free(o);
        }
        for (int i = 0; i < TRS_MAXS; i++)
            for (int s = 0; s < 2; s++) {
                ui_free(&t_sw[i][s]);
                if (i >= G.m.ns) continue;
                uint8_t *q = ui_buf(cell, cell);
                memset(q, 0, (size_t)cell * cell * 4);
                paint_switch(q, cell, cell, 0, 0, (float)cell, &G.m, G.m.sw[i], s);
                t_sw[i][s] = ui_tex(cell, cell, q);
            }
        if (fresh) {
            int l = (int)(ENGINE * cell);
            for (int k = 0; k < TRS_COLOURS; k++) make_tex(&t_engine[k], l, engine_tex, k);
            make_tex(&t_toggle, (int)(0.3f * cell), paint_toggle, 0);
        }
        plan_wave();
    }
    ui_board_used(ox - fr, oy - fr, (float)bw, (float)bh);
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_par);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the run ----------------------------------------------------------------------------------------------------- */
static float tau_end(void)                      /* where the run's picture stops (ticks) */
{
    if (R.result == TRS_CRASH && R.bad_t2 >= 0) {
        int i = R.bad_t, j = R.bad_t2, ki = R.ticks - R.start[i], kj = R.ticks - R.start[j];
        if (ki > 0 && kj > 0 && R.cell[i][ki] != R.cell[j][kj]) return R.ticks - 0.5f;   /* passing each other */
    }
    if (R.result == TRS_DEAD) return R.ticks - 0.4f;
    return (float)R.ticks;
}

static float tau_now(Uint32 now)
{
    float t = run.tau0 + (float)(int)(now - run.t0) / run.ms;
    return fminf(t, tau_end());
}

static Uint32 end_ms(void) { return run.t0 + (Uint32)fmaxf(0, (tau_end() - run.tau0) * run.ms); }

static int solved(void) { return G.solved; }

static void settle(Uint32 now)                  /* a run gone wrong and shown: the trains back at their portals */
{
    if (run.on && R.result != TRS_OK && now >= end_ms() + HOLD_MS) run.on = 0;
}

static int running(Uint32 now) { return run.on && now < end_ms(); }

static int busy(void)
{
    if (!run.on) return 0;
    Uint32 now = ui_ms(), e = end_ms();
    return now < e + (R.result == TRS_OK ? FLARE_MS : HOLD_MS);
}

static void calm(void)
{
    run.on = 0;
    hint_sw = -1;
    hint_go = 0;
    down = -1;
    memset(flip_t0, 0, sizeof flip_t0);
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!trs_level(&G, level)) trs_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    trs_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return pzk_stars(ui_par_stars(G.moves, par()), G.hints, G.fails); }
static int perfect(void) { return solved() && G.moves <= par() && !G.hints && !G.fails; }

static void plan_park(void)
{
    for (int i = 0; i < G.m.nt; i++) {
        park[i] = 0;
        if (R.len[i] < 2) continue;
        int c = R.cell[i][R.len[i] - 1], t = R.start[i] + R.len[i] - 1;
        if (G.m.kind[c] != TRS_STATION) continue;
        for (int j = 0; j < G.m.nt; j++) {
            if (j == i || R.len[j] < 2 || R.cell[j][R.len[j] - 1] != c) continue;
            int tj = R.start[j] + R.len[j] - 1;
            if (tj < t || (tj == t && j < i)) park[i]++;
        }
    }
}

static void go(void)
{
    Uint32 now = ui_ms();
    settle(now);
    if (solved() || run.on) { ui_sound(SND_DENY); return; }
    run.set = G.set;
    trs_go(&G, &R);
    plan_park();
    run.on = 1;
    run.t0 = now;
    run.tau0 = -0.35f;                          /* a moment's pause, then off they go */
    run.ms = TICK_MS;
    run.heard = -1;
    hint_sw = -1;
    hint_go = 0;
    ui_sound(SND_MOVE);
    ui_animate();
}

static void flip(int sw)
{
    Uint32 now = ui_ms();
    settle(now);
    if (run.on || solved()) { ui_sound(SND_DENY); return; }
    uint16_t before = G.set;
    if (!trs_flip(&G, sw)) { ui_sound(SND_DENY); return; }
    flip_from = (uint16_t)((flip_from & ~(1u << sw)) | (before & (1u << sw)));
    flip_t0[sw] = now ? now : 1;
    if (hint_sw == sw) hint_sw = -1;
    hint_go = 0;
    ui_sound(SND_TAP);
    ui_animate();
}

static void hint(void)
{
    settle(ui_ms());
    if (solved() || run.on) { ui_sound(SND_DENY); return; }
    int sw = trs_hint(&G);
    G.hints++;
    hint_sw = sw;
    hint_go = sw < 0;
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    int r;
    if (run.on && !solved()) { calm(); return 1; }   /* a run going wrong: called off */
    r = trs_undo(&G);
    calm();
    return r;
}

static int script(const char *moves)
{
    int ok = trs_script(&G, moves);
    calm();
    if (solved()) {                             /* the trains at their stations, the light at once */
        trs_simulate(&G.m, G.set, &R);
        plan_park();
        run.on = 1;
        run.set = G.set;
        run.ms = 1;
        run.tau0 = tau_end();
        run.t0 = ui_ms();
        run.heard = 1e9f;
    }
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y)  /* a square's top left */
{
    *x = ox + c % G.m.w * cell;
    *y = oy + c / G.m.w * cell;
}

/* train i at tick tau: where, heading where (degrees), how seen (0..1: in a tunnel) */
static void at_piece(int c, int in, int out, float u, float *x, float *y, float *deg, float *alpha)
{
    float x0, y0;
    cell_xy(c, &x0, &y0);
    *alpha = 1;
    if (in >= 4 && out >= 4) { in = TRS_LEFT; out = TRS_RIGHT; }
    if (in == 5) { in = (out + 2) & 3; *alpha = clampf(u * 2.2f - 0.1f, 0, 1); }
    if (out == 5) { out = (in + 2) & 3; *alpha = clampf((1 - u) * 2.2f - 0.1f, 0, 1); }
    if (in == 4) in = (out + 2) & 3;
    if (out == 4) out = (in + 2) & 3;
    float ax, ay, bx, by, u0 = fmaxf(0, u - 0.02f), u1 = fminf(1, u + 0.02f);
    piece_at(x0, y0, (float)cell, in, out, u, x, y);
    piece_at(x0, y0, (float)cell, in, out, u0, &ax, &ay);
    piece_at(x0, y0, (float)cell, in, out, u1, &bx, &by);
    *deg = atan2f(by - ay, bx - ax) * 180 / PUZ_PI;
}

static void pose(int i, float tau, float *x, float *y, float *deg, float *alpha)
{
    int n = R.len[i];
    float l = run.on ? tau - R.start[i] : -1;
    if (!run.on || n <= 1 || l <= 0) {          /* at its portal, heading out */
        int c = G.m.portal[G.m.tfrom[i]];
        at_piece(c, 4, low_side(G.m.open[c]), 0.5f, x, y, deg, alpha);
        return;
    }
    float end = n - 1 - (float)park[i];         /* behind the trains already there */
    if (end < 0) end = 0;
    if (l > end) l = end;
    int k = (int)l;
    float f = l - k;
    if (k >= n - 1) { k = n - 2; f = 1; }
    if (f < 0.5f) { at_piece(R.cell[i][k], R.in[i][k], R.out[i][k], 0.5f + f, x, y, deg, alpha); return; }
    int c = R.cell[i][k + 1];
    float u = f - 0.5f;
    if (G.m.kind[c] == TRS_STATION) u = 0.26f * puz_ease(u * 2);   /* drawing up at the platform */
    at_piece(c, R.in[i][k + 1], R.out[i][k + 1], u, x, y, deg, alpha);
}

static uint16_t set_at(float tau, int sw, float *since)   /* a switch's setting at tick tau, flipped `since` ticks ago */
{
    uint16_t s = run.set;
    *since = 99;
    for (int f = 0; f < R.nflip; f++) {
        float t = R.flip_t[f] + 0.5f;
        if (t > tau) break;
        s ^= (uint16_t)(1u << R.flip_s[f]);
        if (R.flip_s[f] == sw) *since = tau - t;
    }
    return s;
}

static int waiting(int i, float tau)            /* train i not yet left its portal */
{
    return !run.on || R.len[i] <= 1 || tau - R.start[i] <= 0;
}

static void sounds(float tau)                   /* what happened since the last frame */
{
    if (tau <= run.heard) return;
    for (int i = 0; i < G.m.nt; i++) {
        float t = (float)(R.start[i] + R.len[i] - 1);
        int ok = R.result == TRS_OK || i != R.bad_t;
        if (R.len[i] > 1 && t > run.heard && t <= tau && ok && G.m.kind[R.cell[i][R.len[i] - 1]] == TRS_STATION)
            ui_sound(SND_PLACE);
    }
    for (int f = 0; f < R.nflip; f++) {
        float t = R.flip_t[f] + 0.5f;
        if (t > run.heard && t <= tau) ui_sound_soft(SND_TAP, 0.6f);
    }
    float e = tau_end();
    if (e > run.heard && e <= tau) ui_sound(R.result == TRS_OK ? SND_END : R.result == TRS_CRASH ? SND_CAPTURE : SND_DENY);
    run.heard = tau;
}

static void draw_engine(int i, float x, float y, float deg, float alpha, float tilt)
{
    int k = G.m.tcol[i] % TRS_COLOURS;
    float l = ENGINE * cell;
    if (alpha <= 0.01f) return;
    ui_shadow_flat(x + 0.04f * cell, y + 0.06f * cell, 0.9f * l, 0.5f * l, 0.45f * alpha);
    SDL_SetTextureAlphaMod(t_engine[k], (Uint8)(255 * alpha));
    ui_put_rot(t_engine[k], x, y, l, l, deg + tilt);
    SDL_SetTextureAlphaMod(t_engine[k], 255);
    if (!G.m.together && G.m.nt > 1 && alpha > 0.5f) {   /* its order, on the cab */
        float r = (deg + tilt) * PUZ_PI / 180, cx = x - cosf(r) * 0.29f * l, cy = y - sinf(r) * 0.29f * l;
        static const uint8_t CREAM[3] = {250, 242, 222};
        ui_number_ex(cx, cy, 0.2f * cell, i + 1, CREAM, (Uint8)(255 * alpha), UI_NUM_TIGHT);
    }
}

static void draw(void)
{
    if (made_cell < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    settle(now);
    float tau = run.on ? tau_now(now) : -1e9f;
    if (run.on) sounds(tau);
    if (busy()) ui_animate();
    ui_put(t_board, ox - fr, oy - fr, (float)bw, (float)bh);
    for (int i = 0; i < G.m.ns; i++) {          /* the switches: set, flipping */
        int c = G.m.sw[i];
        float x0, y0, q = 1;
        cell_xy(c, &x0, &y0);
        int s = G.set >> i & 1, was = s;
        if (run.on) {
            float since;
            s = set_at(tau, i, &since) >> i & 1;
            if (since < 0.45f) { q = since / 0.45f; was = !s; }
        } else if (flip_t0[i] && now - flip_t0[i] < FLIP_MS) {
            q = (float)(now - flip_t0[i]) / FLIP_MS;
            was = flip_from >> i & 1;
            ui_animate();
        }
        q = puz_ease(q);
        if (was != s && q < 1) {
            ui_put(t_sw[i][was], x0, y0, (float)cell, (float)cell);
            SDL_SetTextureAlphaMod(t_sw[i][s], (Uint8)(255 * q));
        }
        ui_put(t_sw[i][s], x0, y0, (float)cell, (float)cell);
        SDL_SetTextureAlphaMod(t_sw[i][s], 255);
        if (hint_sw == i && !run.on) {
            ui_mark_gold(x0 + cell / 2.0f, y0 + cell / 2.0f, (1.3f + 0.12f * sinf(now / 180.0f)) * cell);
            ui_animate();
        }
        /* the lever by the side no track leaves from: a brass knob, an iron arm towards the branch set */
        int closed = 0;
        while (G.m.open[c] >> closed & 1) closed++;
        float kx = x0 + cell * (0.5f + 0.34f * DX[closed]), ky = y0 + cell * (0.5f + 0.34f * DY[closed]);
        float ax, ay, bx, by;
        int on = trs_branch(&G.m, c, s), off = trs_branch(&G.m, c, was);
        piece_at(x0, y0, (float)cell, G.m.k[c], on, 1, &ax, &ay);
        piece_at(x0, y0, (float)cell, G.m.k[c], off, 1, &bx, &by);
        float a1 = atan2f(ay - ky, ax - kx), a0 = atan2f(by - ky, bx - kx), da = a1 - a0;
        while (da > PUZ_PI) da -= 2 * PUZ_PI;
        while (da < -PUZ_PI) da += 2 * PUZ_PI;
        float ang = a0 + da * q, len = 0.21f * cell;
        float ex = kx + cosf(ang) * len, ey = ky + sinf(ang) * len;
        pzk_line(kx + 0.02f * cell, ky + 0.03f * cell, ex + 0.02f * cell, ey + 0.03f * cell, 0.05f * cell, 1.5f,
                 (pzk_rgba){0, 0, 0, 70}, 0);
        pzk_line(kx, ky, ex, ey, 0.06f * cell, 0, (pzk_rgba){IRON[0], IRON[1], IRON[2], 255}, 0);
        pzk_dot(ex, ey, 0.11f * cell, 0, (pzk_rgba){STOP[0], STOP[1], STOP[2], 255}, 0);
        if (G.m.toggle[c]) ui_put_c(t_toggle, kx, ky, 0.3f * cell, 0.3f * cell);
        else {
            pzk_dot(kx, ky, 0.2f * cell, 0, (pzk_rgba){IRON[0], IRON[1], IRON[2], 255}, 0);
            pzk_dot(kx, ky, 0.15f * cell, 0, (pzk_rgba){BRASS[0], BRASS[1], BRASS[2], 255}, 0);
        }
    }
    for (int c = 0; c < G.m.w * G.m.h; c++)     /* the tunnels' pairs, on their hills */
        if (G.m.kind[c] == TRS_TUNNEL) {
            float x0, y0;
            int a = low_side(G.m.open[c]), f = (a + 2) & 3;
            cell_xy(c, &x0, &y0);
            float cx = x0 + cell * (0.5f + 0.38f * DX[f]), cy = y0 + cell * (0.5f + 0.38f * DY[f]);
            pzk_dot(cx, cy, 0.24f * cell, 0, (pzk_rgba){BRASS[0], BRASS[1], BRASS[2], 255}, 0);
            ui_number_ex(cx, cy, 0.17f * cell, G.m.k[c] + 1, IRON, 255, UI_NUM_TIGHT);
        }
    /* the trains: those waiting at each portal (the first one there, the others as tokens), those on their way */
    int crash = run.on && now >= end_ms() && R.result != TRS_OK;
    if (crash) {                                /* where it went wrong: a red light under the trains */
        float x, y, deg, al, x0, y0;
        pose(R.bad_t, tau, &x, &y, &deg, &al);
        cell_xy(R.bad, &x0, &y0);
        if (R.result == TRS_WRONG) { x = x0 + cell / 2.0f; y = y0 + cell / 2.0f; }
        Uint8 a = (Uint8)(150 + 60 * sinf(now / 110.0f));
        pzk_dot(x, y, 1.1f * cell, 0.45f * cell, (pzk_rgba){230, 52, 34, a}, 0);
    }
    for (int p = 0; p < G.m.ne; p++) {
        int first = -1, more = 0;
        for (int i = 0; i < G.m.nt; i++)
            if (G.m.tfrom[i] == p && waiting(i, tau)) {
                if (first < 0) first = i;
                else {                          /* a token by the arch: its colour, its order */
                    int c = G.m.portal[p], a = low_side(G.m.open[c]), f = (a + 2) & 3;
                    float x0, y0;
                    cell_xy(c, &x0, &y0);
                    float side = more & 1 ? -1.0f : 1.0f, along = 0.36f - 0.3f * (float)(more / 2);
                    float tx = x0 + cell * (0.5f + along * DX[f] + side * 0.36f * -DY[f]);
                    float ty = y0 + cell * (0.5f + along * DY[f] + side * 0.36f * DX[f]);
                    uint8_t col[3];
                    colour_of(G.m.tcol[i], col);
                    pzk_dot(tx + 0.02f * cell, ty + 0.03f * cell, 0.27f * cell, 2, (pzk_rgba){0, 0, 0, 90}, 0);
                    pzk_dot(tx, ty, 0.25f * cell, 0, (pzk_rgba){IRON[0], IRON[1], IRON[2], 255}, 0);
                    pzk_dot(tx, ty, 0.2f * cell, 0, (pzk_rgba){col[0], col[1], col[2], 255}, 0);
                    int dark = art_player_dark(COL[G.m.tcol[i] % TRS_COLOURS]);
                    static const uint8_t CREAM[3] = {250, 242, 222};
                    ui_number_ex(tx, ty, 0.15f * cell, i + 1, dark ? CREAM : IRON, 255, UI_NUM_TIGHT);
                    more++;
                }
            }
        if (first >= 0) {
            float x, y, deg, al;
            pose(first, tau, &x, &y, &deg, &al);
            draw_engine(first, x, y, deg, al, 0);
        }
    }
    float ptau = run.on ? run.tau0 + (float)(int)(now - run.t0) / run.ms : 0;   /* the puffs go on fading */
    for (int i = 0; i < G.m.nt; i++) {
        if (waiting(i, tau)) continue;
        float x, y, deg, al, tilt = 0;
        pose(i, tau, &x, &y, &deg, &al);
        if (crash && R.result == TRS_CRASH && (i == R.bad_t || i == R.bad_t2)) {   /* knocked askew */
            float q = clampf((float)(int)(now - end_ms()) / 220.0f, 0, 1);
            tilt = (i == R.bad_t ? 24 : -20) * puz_ease(q);
            x += (i == R.bad_t ? -1 : 1) * 0.06f * cell * puz_ease(q);
        }
        draw_engine(i, x, y, deg, al, tilt);
    }
    if (run.on && ptau > -1) {                  /* the puffs: rising from each chimney, growing, fading */
        for (int i = 0; i < G.m.nt; i++) {
            float t_end = (float)(R.start[i] + R.len[i] - 1);
            if (R.len[i] <= 1) continue;
            for (int j = 0; j < PUFFS; j++) {
                float e = (floorf(ptau / PUFF_EVERY) - j) * PUFF_EVERY;
                if (e < R.start[i] || e > t_end || e > ptau) continue;
                float age = (ptau - e) / (PUFFS * PUFF_EVERY);
                float x, y, deg, al;
                pose(i, e, &x, &y, &deg, &al);
                if (al < 0.5f) continue;
                float r = deg * PUZ_PI / 180, cx = x + cosf(r) * 0.23f * cell, cy = y + sinf(r) * 0.23f * cell;
                cx += 0.12f * cell * age * sinf(e * 7.3f);
                cy -= 0.5f * cell * age;
                float d = (0.14f + 0.3f * age) * cell;
                pzk_dot(cx, cy, d, 0.35f * d, (pzk_rgba){246, 244, 238, (Uint8)(200 * (1 - age))}, 0);
            }
        }
    }
    if (crash) {                                /* where it went wrong: a red glow (and a cloud, for a crash) */
        float x0, y0, x, y, deg, al;
        cell_xy(R.bad, &x0, &y0);
        pose(R.bad_t, tau, &x, &y, &deg, &al);
        float q = (float)(int)(now - end_ms()) / HOLD_MS;
        ui_mark_glow(x, y, (1.3f + 0.15f * sinf(now / 90.0f)) * cell);
        if (R.result == TRS_CRASH)
            for (int j = 0; j < 5; j++) {
                float a = j * 1.26f, d = (0.25f + 0.35f * q) * cell;
                pzk_dot(x + cosf(a) * d * 0.6f, y + sinf(a) * d * 0.5f - 0.3f * q * cell, d, 0.4f * d,
                        (pzk_rgba){120, 116, 112, (Uint8)(170 * (1 - q))}, 0);
            }
        if (R.result == TRS_WRONG) ui_mark_glow(x0 + cell / 2.0f, y0 + cell / 2.0f, 1.2f * cell);
    }
    if (run.on && R.result == TRS_OK && now >= end_ms()) {   /* solved: the stations flare; perfect: the track lights */
        float wt = (float)(int)(now - end_ms());
        for (int c = 0; c < G.m.w * G.m.h; c++) {
            float x0, y0;
            cell_xy(c, &x0, &y0);
            if (G.m.kind[c] == TRS_STATION) {
                float q = puz_pulse(wt, 0, FLARE_MS);
                if (q > 0) ui_mark_gold(x0 + cell / 2.0f, y0 + cell / 2.0f, (1.2f + 1.2f * q) * cell * q);
            }
            if (perfect() && wave[c] >= 0) {
                float k = puz_pulse(wt, FLARE_MS * 0.25f + wave[c] * WAVE_STEP, WAVE_LEN);
                if (k > 0) ui_mark_gold(x0 + cell / 2.0f, y0 + cell / 2.0f, 1.15f * cell * k);
            }
        }
        if (wt < FLARE_MS + (G.m.w + G.m.h) * 2 * WAVE_STEP + WAVE_LEN) ui_animate();
    }
    int live = !solved() && !run.on;
    ui_tool(T_GO, ICON_PLAY, !live ? UI_DIM : hint_go ? UI_ON : UI_LIT);
    ui_tool(T_HINT, ICON_HINT, live ? (down == T_HINT ? UI_ON : 0) : UI_DIM);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    down = ui_tool_hit(x, y);
    if (down >= 0) { ui_animate(); return; }
    Uint32 now = ui_ms();
    settle(now);
    if (running(now)) {                         /* hurry them up */
        run.tau0 = tau_now(now);
        run.t0 = now;
        run.ms = FAST_MS;
        ui_animate();
        return;
    }
    if (run.on || solved()) return;
    int cx = (int)floorf((x - ox) / cell), cy = (int)floorf((y - oy) / cell);
    if (cx < 0 || cy < 0 || cx >= G.m.w || cy >= G.m.h) return;
    int sw = trs_switch_at(&G.m, cx + cy * G.m.w);
    if (sw < 0) { ui_sound(SND_DENY); return; }
    flip(sw);
}

static void motion(float x, float y) { (void)x; (void)y; }

static void release(float x, float y)
{
    if (down < 0) return;
    int k = down;
    down = -1;
    ui_animate();
    if (ui_tool_hit(x, y) != k) return;
    if (k == T_GO) go();
    else hint();
}

static void key(SDL_Keycode k)
{
    if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_g) go();
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.moves, t_par, par(), solved() && !running(ui_ms()) && G.moves <= par());
}

/* ---- the shell's pictures: a network, its switches as given, the engines at their portals ---------------------- */
static void paint_net(const trs_map *m, int size, uint8_t *rgba)
{
    int big = m->w > m->h ? m->w : m->h, f = (int)(size * FRAME), sq = (size - 2 * f) / big;
    if (sq < 3) return;
    int W = m->w * sq + 2 * f, H = m->h * sq + 2 * f;
    uint8_t *o = malloc((size_t)W * H * 4);
    if (o) {
        paint_board(m, sq, f, o);
        for (int i = 0; i < m->ns; i++)
            paint_switch(o, W, H, (float)(f + m->sw[i] % m->w * sq), (float)(f + m->sw[i] / m->w * sq), (float)sq, m,
                         m->sw[i], m->set0 >> i & 1);
        for (int p = 0; p < m->ne; p++)
            for (int i = 0; i < m->nt; i++)
                if (m->tfrom[i] == p) {
                    int c = m->portal[p], a = low_side(m->open[c]);
                    float deg = (float)(a == TRS_RIGHT ? 0 : a == TRS_DOWN ? 90 : a == TRS_LEFT ? 180 : 270);
                    paint_engine(o, W, H, f + (c % m->w + 0.5f) * sq, f + (c / m->w + 0.5f) * sq, ENGINE * sq, deg,
                                 m->tcol[i]);
                    break;
                }
        art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    }
    free(o);
}

/* the menu's card and the random tab's: a level of that kind (levels are read, never made here: the cards are
 * painted on a thread of their own) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const int LV[TRS_VARIANTS] = {12, 45, 95};
    trs_game T;
    if (trs_level(&T, variant < 0 ? 31 : LV[variant % TRS_VARIANTS])) paint_net(&T.m, size, rgba);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    trs_game T;
    if (trs_level(&T, level)) paint_net(&T.m, size, rgba);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    trs_map m;
    if (chapter >= 0 && chapter < TRS_CHAPTERS && trs_parse(&m, trs_icons[chapter])) paint_net(&m, size, rgba);
}

static const game trains_game = {
    .id = "trains", .order = 458, .variants = TRS_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = TRS_LEVELS, .chapters = TRS_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1,
    .tools = TOOLS,
};
REGISTER_GAME(trains_game)
