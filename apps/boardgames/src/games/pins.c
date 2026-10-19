/*
 * Pull the Pins in RetroStone Board, for one player: a tall box of dark glass in the house frame, its chambers of
 * walnut bars, a glass cup (or two) at the bottom (the rules: pins_rules.c). Golden pins with a ring at one end hold
 * glossy gems, grey balls, water and glowing lava; tap a pin (or swipe across it) and it slides out towards its ring,
 * and everything falls. Water and lava are drops drawn as soft merged blobs; lava glows and turns to dark stone where
 * water touches it, with a puff of steam; a grey ball lights up in the colour of the gem that touches it; a gem that
 * lands in the cup sparkles. Lost (a ball burnt, the wrong thing in a cup, a ball left out): a red glow where it went
 * wrong, the restart lit; a tap restarts. Solved: the cup glows gold.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The physics runs on the shell's clock (ui_ms): the world steps PTP_HZ times a second from the try's start, so a game
 * played again from its record (and the pictures) comes out the same. The tools: restart, the lamp (a hint: the next
 * pin of a winning order glows; after a wrong pull, the restart glows). No undo (physics): restart instead. The bar:
 * the balls still to bring to the cups. Stars: no hint and no restart 3, up to two of them 2, else 1.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/pins.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define F 2.0f                                  /* the frame beyond the world's edge, units */
#define WIN_MS 1300
#define HINT_MS 2600
#define FADE_MS 260                             /* a pin pulled out fades away */
#define NFX 64

enum { T_RESTART, T_HINT, T_COUNT };

static const uint8_t GEMS[4][3] = {{206, 38, 52}, {34, 158, 84}, {44, 98, 214}, {236, 166, 28}};
static const uint8_t GREY[3] = {158, 158, 164}, STONE[3] = {92, 84, 80}, WALNUT[3] = {120, 80, 52},
                     GOLD[3] = {222, 170, 70}, GLASS[3] = {205, 236, 246};

static ptp_game G;
static int serial, tool_down = -1, pulled_this, seen, hint_pin = -1, hint_restart, lost_seen;
static Uint32 t0, hint_t0, win_t0, lose_t0, last_cup_snd, last_stone_snd;
static int base_tick;
static struct { int type, col; float x, y; Uint32 t; } fx[NFX];
static int nfx;

static box last_box;
static float ox, oy, k;                         /* the world's (0, 0) on the screen less the frame; pixels a unit */
static int made_k = -1, made_serial = -1, made_small = -1;
static SDL_Texture *t_board, *t_front, *t_ball[PTP_KINDS + 4], *t_pin[PTP_MAXPIN], *t_gem_small;
static int pin_tw[PTP_MAXPIN], pin_th[PTP_MAXPIN];

/* ---- the clock ------------------------------------------------------------------------------------------------- */
static double tpos(Uint32 now) { return base_tick + (double)(now - t0) * PTP_HZ / 1000.0; }

/* the world stepped to now (at rest: its clock only moves on) */
static void advance(Uint32 now)
{
    int target = (int)tpos(now);
    while (G.s.tick < target) {
        if (G.s.rest) {
            G.s.tick = target;
            break;
        }
        ptp_step(&G.w, &G.s);
    }
}

static Uint32 tick_ms(int tick) { return t0 + (Uint32)((double)(tick - base_tick) * 1000.0 / PTP_HZ); }

static void rebase(void)
{
    t0 = ui_ms();
    base_tick = G.s.tick;
    seen = G.s.nev;
    nfx = 0;
    hint_pin = -1;
    hint_restart = 0;
    win_t0 = lose_t0 = 0;
    lost_seen = 0;
    tool_down = -1;
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* a lit ball d across with its soft shadow: gloss 1 a gem, lower duller (grey, stone) */
static void paint_ball(const uint8_t rgb[3], float gloss, int d, uint8_t *o)
{
    float R = 0.44f * d, mx = 0.47f * d, my = 0.46f * d, L[3] = {-0.45f, -0.62f, 0.64f}, HV[3] = {-0.45f, -0.62f, 1.64f};
    float ln = sqrtf(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]), hn = sqrtf(HV[0] * HV[0] + HV[1] * HV[1] + HV[2] * HV[2]);
    for (int i = 0; i < 3; i++) L[i] /= ln, HV[i] /= hn;
    for (int py = 0; py < d; py++)
        for (int px = 0; px < d; px++) {
            float col[3] = {0, 0, 0}, cov = 0;
            for (int sy = 0; sy < 3; sy++)
                for (int sx = 0; sx < 3; sx++) {
                    float x = (px + (sx + 0.5f) / 3 - mx) / R, y = (py + (sy + 0.5f) / 3 - my) / R, r2 = x * x + y * y;
                    if (r2 >= 1) continue;
                    float z = sqrtf(1 - r2), nl = x * L[0] + y * L[1] + z * L[2], nh = x * HV[0] + y * HV[1] + z * HV[2];
                    float glow = gloss * 0.45f * clampf(x * 0.5f + y * 0.7f, 0, 1);   /* light through it, low right */
                    float kk = 0.38f + 0.7f * fmaxf(0, nl) + glow;
                    float sp = gloss * (0.95f * powf(fmaxf(0, nh), 60) + 0.18f * powf(fmaxf(0, nh), 5));
                    float rim = clampf((sqrtf(r2) - 1) * R / 1.4f + 1, 0, 1) * 0.45f;
                    float n = gloss < 0.5f ? 1 + 0.12f * art_noise(px * 3, py * 3) : 1;
                    for (int j = 0; j < 3; j++) {
                        float v = clampf(rgb[j] * kk * n, 0, 255);
                        v += (255 - v) * sp;
                        col[j] += v * (1 - rim) + rgb[j] * 0.35f * rim;
                    }
                    cov += 1;
                }
            uint8_t *p = o + 4 * ((size_t)py * d + px);
            float sx = (px + 0.5f - mx - 0.07f * d) / R, sy = (py + 0.5f - my - 0.09f * d) / R;
            float sa = 0.4f * clampf((1.06f - sqrtf(sx * sx + sy * sy)) / 0.25f, 0, 1), a = cov / 9;
            float out = a + sa * (1 - a);
            for (int j = 0; j < 3; j++) p[j] = (uint8_t)(out > 0 ? clampf(col[j] / 9 / out, 0, 255) : 0);
            p[3] = (uint8_t)(255 * out + 0.5f);
        }
}

static void ball_pic(int which, int d, uint8_t *o)   /* 0..3 gems, 4 grey, 5 stone */
{
    memset(o, 0, (size_t)d * d * 4);
    if (which < 4) paint_ball(GEMS[which], 1, d, o);
    else paint_ball(which == 4 ? GREY : STONE, which == 4 ? 0.45f : 0.15f, d, o);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    ball_pic(0, size, rgba);
}

/* a round-ended bar painted lit as a cylinder: style 0 walnut, 1 glass, 2 gold; 3 its soft shadow */
static void paint_bar(uint8_t *o, int W, int H, float ax, float ay, float bx, float by, float r, int style)
{
    float soft = style == 3 ? fmaxf(1.5f, 0.6f * r) : 0, R = r + soft;
    int x0 = (int)fmaxf(0, floorf(fminf(ax, bx) - R - 1)), x1 = (int)fminf((float)W - 1, ceilf(fmaxf(ax, bx) + R + 1));
    int y0 = (int)fmaxf(0, floorf(fminf(ay, by) - R - 1)), y1 = (int)fminf((float)H - 1, ceilf(fmaxf(ay, by) + R + 1));
    float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, len = sqrtf(l2);
    const float L[3] = {-0.42f, -0.6f, 0.68f};
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float px = x + 0.5f, py = y + 0.5f, t = l2 > 0 ? clampf(((px - ax) * dx + (py - ay) * dy) / l2, 0, 1) : 0;
            float ex = px - ax - t * dx, ey = py - ay - t * dy, d = hypotf(ex, ey);
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            if (style == 3) {
                float a = 0.42f * clampf((R - d) / (2 * soft), 0, 1);
                if (a > 0) pzk_over(p, (const uint8_t[]){0, 0, 0}, a);
                continue;
            }
            float cov = clampf(r - d + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float q = fminf(d / r, 0.999f), z = sqrtf(1 - q * q), nx = d > 0 ? ex / d * q : 0, ny = d > 0 ? ey / d * q : 0;
            float nl = fmaxf(0, nx * L[0] + ny * L[1] + z * L[2]);
            float hv = fmaxf(0, nx * L[0] * 0.6f + ny * L[1] * 0.6f + z), sp = powf(fmaxf(0, nl), 24);
            (void)hv;
            uint8_t c[3];
            if (style == 0) {
                float along = t * len, g = art_noise((int)(along * 0.35f), (int)((d * (ex * dy - ey * dx > 0 ? 1 : -1) + r) * 1.6f));
                float kk = (0.52f + 0.62f * nl) * (1 + 0.16f * g);
                for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(WALNUT[i] * kk + 120 * sp * 0.6f, 0, 255);
                pzk_over(p, c, cov);
            } else if (style == 1) {
                for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(GLASS[i] * (0.7f + 0.4f * nl) + 255 * sp, 0, 255);
                pzk_over(p, c, cov * clampf(0.22f + 0.6f * q * q * q + 0.7f * sp, 0, 0.95f));
            } else {
                float kk = 0.5f + 0.62f * nl;
                for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(GOLD[i] * kk + 255 * sp * 0.85f, 0, 255);
                pzk_over(p, c, cov);
            }
        }
}

/* a pin: its ring (centred R + 1 from the left, the middle) and its bar to the right, len pixels from the ring's centre */
static void paint_pin(float len, float kk, int *tw, int *th, uint8_t **out)
{
    float R = 1.75f * kk, Rin = 1.0f * kk, br = (float)PTP_PIN_R / PTP_FP * kk;
    int W = (int)ceilf(len + R + 2 + br), H = (int)ceilf(2 * R + 3);
    uint8_t *o = calloc((size_t)W * H, 4);
    *tw = W;
    *th = H;
    *out = o;
    if (!o) return;
    float cx = R + 1.5f, cy = H / 2.0f;
    paint_bar(o, W, H, cx + Rin * 0.8f, cy, cx + len, cy, br, 2);
    const float L[3] = {-0.42f, -0.6f, 0.68f};
    float rc = (R + Rin) / 2, tr = (R - Rin) / 2;
    for (int y = 0; y < H; y++)                 /* the ring: a gold torus */
        for (int x = 0; x < W && x < (int)(cx + R + 2); x++) {
            float px = x + 0.5f - cx, py = y + 0.5f - cy, dist = hypotf(px, py), d = fabsf(dist - rc);
            float cov = clampf(tr - d + 0.5f, 0, 1);
            if (cov <= 0 || dist <= 0) continue;
            float q = fminf(d / tr, 0.999f), z = sqrtf(1 - q * q), sgn = dist > rc ? 1.0f : -1.0f;
            float nx = px / dist * q * sgn, ny = py / dist * q * sgn;
            float nl = fmaxf(0, nx * L[0] + ny * L[1] + z * L[2]), sp = powf(nl, 24);
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(GOLD[i] * (0.5f + 0.62f * nl) + 255 * sp * 0.85f, 0, 255);
            pzk_over(o + 4 * ((size_t)y * W + x), c, cov);
        }
}

/* the cups' shapes in units: polygons of their insides */
static int cup_poly(const ptp_world *w, int i, float *xy)
{
    for (int k = 0; k < 8; k++) xy[k] = (float)w->cpoly[i][k] / PTP_FP;
    float x0 = fminf(xy[0], xy[6]), x1 = fmaxf(xy[0], xy[6]), b0 = fminf(xy[2], xy[4]), b1 = fmaxf(xy[2], xy[4]);
    xy[0] = x0, xy[2] = b0, xy[4] = b1, xy[6] = x1;   /* from the left rim round, as drawn */
    return 4;
}

/* the board: the frame, the dark glass, the walnut bars and their shadows, the cups' glass at the back */
static void paint_board(const ptp_world *w, float kk, int W, int H, uint8_t *o)
{
    int f = (int)((F + 1) * kk);
    art_plank(W - 2 * f, H - 2 * f, f, o);
    float bx = F * kk, by = F * kk;             /* the world's (0, 0) in the picture */
    for (int y = f; y < H - f; y++)
        for (int x = f; x < W - f; x++) {
            float t = (float)(y - f) / (H - 2 * f), u = (float)(x - f) / (W - 2 * f);
            float c[3] = {38 + (18 - 38) * t, 66 + (32 - 66) * t, 74 + (38 - 74) * t};
            float band = (x + 0.55f * y) / (float)W;   /* two soft reflections across the glass */
            float sheen = 0.05f * expf(-powf((band - 0.35f) * 9, 2)) + 0.035f * expf(-powf((band - 0.62f) * 14, 2));
            float edge = fminf(fminf(x - f, W - f - x), fminf(y - f, H - f - y)) / kk;
            float shade = 1 - 0.45f * expf(-edge / 0.9f);
            (void)u;
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf((c[i] * (1 + 0.03f * art_noise(x, y)) + 255 * sheen) * shade, 0, 255);
            p[3] = 255;
        }
    for (int c = 0; c < w->cups; c++) {         /* the cups' glass, lighter */
        float xy[8];
        cup_poly(w, c, xy);
        for (int i = 0; i < 4; i++) { xy[2 * i] = bx + xy[2 * i] * kk; xy[2 * i + 1] = by + xy[2 * i + 1] * kk; }
        int y0 = (int)(by + 79 * kk), y1 = (int)(by + 102 * kk);
        for (int y = y0; y < y1 && y < H; y++)
            for (int x = 0; x < W; x++) {
                float cov = pzt_cover(xy, 4, (float)x, (float)y);
                if (cov > 0) pzk_over(o + 4 * ((size_t)y * W + x), (const uint8_t[]){70, 110, 122}, 0.35f * cov);
            }
    }
    if (w->drain) {                             /* the drain: a dark well under its opening, an iron grate over it */
        float x0 = bx + w->dx0 * kk / PTP_FP, x1 = bx + w->dx1 * kk / PTP_FP, y0 = by + 80 * kk;
        for (int y = (int)y0; y < H - f; y++)
            for (int x = (int)x0; x < (int)x1 && x < W - f; x++) {
                if (x < f) continue;
                float d = (y - y0) / (8 * kk), e = fminf(x - x0, x1 - x) / (2 * kk);
                float a = clampf(0.55f + 0.45f * d, 0, 0.92f) * clampf(e, 0.3f, 1);
                pzk_over(o + 4 * ((size_t)y * W + x), (const uint8_t[]){6, 8, 10}, a);
            }
        for (int i = 0; i < 6; i++) {
            float gx = x0 + (i + 0.5f) * (x1 - x0) / 6;
            paint_bar(o, W, H, gx, y0 + 1.2f * kk, gx, y0 + 4.2f * kk, 0.32f * kk, 1);
        }
        paint_bar(o, W, H, x0 + 0.6f * kk, y0 + 1.2f * kk, x1 - 0.6f * kk, y0 + 1.2f * kk, 0.38f * kk, 1);
    }
    int lips = w->wood_end, cupend = w->glass_end;
    float r = (float)PTP_WALL_R / PTP_FP * kk;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 4; i < w->nseg; i++) {
            const ptp_seg *s = &w->seg[i];
            float ax = bx + s->ax * kk / PTP_FP, ay = by + s->ay * kk / PTP_FP, bxx = bx + s->bx * kk / PTP_FP,
                  byy = by + s->by * kk / PTP_FP;
            int glass = i >= lips && i < cupend;
            if (pass == 0 && !glass) paint_bar(o, W, H, ax + 0.3f * kk, ay + 0.45f * kk, bxx + 0.3f * kk, byy + 0.45f * kk, r, 3);
            if (pass == 1) paint_bar(o, W, H, ax, ay, bxx, byy, glass ? r * 0.8f : r, glass ? 1 : 0);
        }
}

/* the cups' front: a sheen on the glass, the rim a band of its colour (gold: any colour) */
static void paint_front(const ptp_world *w, float kk, int W, int H, uint8_t *o)
{
    memset(o, 0, (size_t)W * H * 4);
    float bx = F * kk, by = F * kk;
    for (int c = 0; c < w->cups; c++) {
        float xy[8];
        cup_poly(w, c, xy);
        float l = xy[0], rr = xy[6];
        for (int i = 0; i < 4; i++) { xy[2 * i] = bx + xy[2 * i] * kk; xy[2 * i + 1] = by + xy[2 * i + 1] * kk; }
        int y0 = (int)(by + 79 * kk), y1 = (int)(by + 102 * kk);
        for (int y = y0; y < y1 && y < H; y++)
            for (int x = 0; x < W; x++) {
                float cov = pzt_cover(xy, 4, (float)x, (float)y);
                if (cov <= 0) continue;
                float u = ((x - bx) / kk - l) / (rr - l), v = ((y - by) / kk - 80) / 21;
                float a = 0.16f * expf(-powf((u - 0.14f) * 16, 2)) + 0.08f * expf(-powf((u - 0.84f) * 22, 2)) +
                          0.05f * (1 - v);
                pzk_over(o + 4 * ((size_t)y * W + x), (const uint8_t[]){255, 255, 255}, a * cov);
            }
        const uint8_t *band = w->cupcol[c] >= 0 ? GEMS[w->cupcol[c] & 3] : (const uint8_t[]){214, 222, 228};   /* any: silver */
        paint_bar(o, W, H, bx + (l + 0.6f) * kk, by + 80.4f * kk, bx + (rr - 0.6f) * kk, by + 80.4f * kk, 0.55f * kk, 3);
        uint8_t *tmp = o;
        float x0 = bx + l * kk, x1 = bx + rr * kk, yy = by + 80.2f * kk, hr = 0.62f * kk;
        for (int y = (int)(yy - hr - 1); y <= (int)(yy + hr + 1); y++)   /* the rim's band, lit as a tube */
            for (int x = (int)x0; x <= (int)x1; x++) {
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                float d = fabsf(y + 0.5f - yy), cov = clampf(hr - d + 0.5f, 0, 1);
                if (cov <= 0) continue;
                float q = fminf(d / hr, 1), kk2 = 0.6f + 0.55f * (1 - q) * (y + 0.5f < yy ? 1.2f : 0.8f);
                uint8_t cc[3];
                for (int i = 0; i < 3; i++) cc[i] = (uint8_t)clampf(band[i] * kk2 + (q < 0.3f && y + 0.5f < yy ? 90 : 0), 0, 255);
                pzk_over(tmp + 4 * ((size_t)y * W + x), cc, cov);
            }
    }
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    last_box = b;
    k = fminf(b.w / (PTP_WU + 2 * F), b.h / (PTP_HU + 2 * F));
    float bw = (PTP_WU + 2 * F) * k, bh = (PTP_HU + 2 * F) * k;
    ox = floorf(b.x + (b.w - bw) / 2);
    oy = floorf(b.y + (b.h - bh) / 2);
    int kq = (int)(k * 64);
    if (made_k != kq || made_serial != serial) {
        int W = (int)ceilf(bw), H = (int)ceilf(bh);
        uint8_t *o = malloc((size_t)W * H * 4);
        if (made_k != kq) {
            for (int i = 0; i < 6; i++) {
                int d = (int)ceilf(2 * (float)640 / PTP_FP * k * 1.12f) + 2;
                if (i == 5) d = (int)ceilf(2 * 384.0f / PTP_FP * k * 1.12f) + 2;
                ui_free(&t_ball[i]);
                ball_pic(i, d, ui_buf(d, d));
                t_ball[i] = ui_tex(d, d, ui_buf(d, d));
            }
        }
        made_k = kq;
        made_serial = serial;
        if (o) {
            paint_board(&G.w, k, W, H, o);
            ui_free(&t_board);
            t_board = ui_tex(W, H, o);
            paint_front(&G.w, k, W, H, o);
            ui_free(&t_front);
            t_front = ui_tex(W, H, o);
        }
        free(o);
        for (int i = 0; i < PTP_MAXPIN; i++) {
            ui_free(&t_pin[i]);
            if (i >= G.w.np) continue;
            uint8_t *po;
            paint_pin((float)G.w.plen[i] / PTP_FP * k, k, &pin_tw[i], &pin_th[i], &po);
            if (po) t_pin[i] = ui_tex(pin_tw[i], pin_th[i], po);
            free(po);
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_gem_small);
        ball_pic(0, small, ui_buf(small, small));
        t_gem_small = ui_tex(small, small, ui_buf(small, small));
    }
}

static float sx_of(float units) { return ox + (units + F) * k; }
static float sy_of(float units) { return oy + (units + F) * k; }

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void begin(void)
{
    serial++;
    rebase();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!ptp_level(&G, level)) ptp_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    ptp_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return G.s.end == PTP_WON ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return pzk_stars(3, G.hints, G.restarts); }
static int lost(void) { return G.s.end != PTP_PLAYING && G.s.end != PTP_WON; }

static int busy(void)
{
    Uint32 now = ui_ms();
    advance(now);
    if (!G.s.rest && (G.s.end == PTP_PLAYING || G.s.end == PTP_WON)) return 1;
    if (G.s.end == PTP_WON && (!win_t0 || now - win_t0 < WIN_MS)) return 1;
    for (int i = 0; i < G.w.np; i++)
        if (G.s.pulled[i] >= 0 && tpos(now) - G.s.pulled[i] < PTP_PULL + FADE_MS * PTP_HZ / 1000.0) return 1;
    return 0;
}

static void restart(void)
{
    if (G.s.npulled) G.restarts++;
    G.s = G.s0;
    rebase();
    ui_sound(SND_TAP);
    ui_animate();
}

static void pull(int i)
{
    advance(ui_ms());
    if (!ptp_pull(&G.w, &G.s, i)) return;
    if (hint_pin == i) hint_pin = -1;
    ui_sound(SND_MOVE);
    ui_animate();
}

static void hint(void)
{
    if (G.s.end != PTP_PLAYING) { ui_sound(SND_DENY); return; }
    int p = ptp_hint(&G);
    Uint32 now = ui_ms();
    if (p == -2) { ui_sound(SND_DENY); return; }
    int again = (p >= 0 && hint_pin == p) || (p == -1 && hint_restart);
    if (!again || now - hint_t0 > HINT_MS) G.hints += !again;
    hint_pin = p;
    hint_restart = p == -1;
    hint_t0 = now;
    ui_sound(SND_TAP);
    ui_animate();
}

static int script(const char *moves)
{
    int ok = ptp_script(&G, moves);
    rebase();
    if (G.s.end == PTP_WON) win_t0 = ui_ms() - WIN_MS;
    return ok;
}

/* a pin's place on the screen now (its ring's centre a, its tip b) and how far out (0..1 slid, past 1 fading) */
static int pin_screen(int i, double tp, float *ax, float *ay, float *bx, float *by, float *out)
{
    float u = 0;
    if (G.s.pulled[i] >= 0) u = (float)((tp - G.s.pulled[i]) / PTP_PULL);
    if (u < 0) u = 0;
    float e = u < 1 ? u * u * (3 - 2 * u) : 1 + (u - 1) * 2.2f;
    float len = (float)G.w.plen[i] / PTP_FP, mv = (len + 4) * e;
    float pax = (float)G.w.pax[i] / PTP_FP, pay = (float)G.w.pay[i] / PTP_FP, pbx = (float)G.w.pbx[i] / PTP_FP,
          pby = (float)G.w.pby[i] / PTP_FP, ux = len > 0 ? (pax - pbx) / len : 0, uy = len > 0 ? (pay - pby) / len : 0;
    *ax = sx_of(pax + ux * mv);
    *ay = sy_of(pay + uy * mv);
    *bx = sx_of(pbx + ux * mv);
    *by = sy_of(pby + uy * mv);
    *out = u;
    return u < 1 + (float)FADE_MS * PTP_HZ / 1000 / PTP_PULL;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void add_fx(int type, int col, float x, float y, Uint32 t)
{
    int i = nfx < NFX ? nfx++ : (int)(t % NFX);
    fx[i].type = type;
    fx[i].col = col;
    fx[i].x = x;
    fx[i].y = y;
    fx[i].t = t;
}

static void take_events(Uint32 now)
{
    if (G.s.nev - seen > PTP_EVENTS) seen = G.s.nev - PTP_EVENTS;
    for (; seen < G.s.nev; seen++) {
        const ptp_event *e = &G.s.ev[seen % PTP_EVENTS];
        Uint32 t = tick_ms(e->tick);
        add_fx(e->type, e->col, e->x / 16.0f, e->y / 16.0f, t);
        if (e->type == PTP_E_CUP && now - last_cup_snd > 70) {
            ui_sound_soft(SND_PLACE, 0.4f);
            last_cup_snd = now;
        } else if (e->type == PTP_E_COLOUR) ui_sound_soft(SND_TAP, 0.5f);
        else if (e->type == PTP_E_DRAIN && now - last_cup_snd > 90) {
            ui_sound_soft(SND_DROP, 0.3f);
            last_cup_snd = now;
        } else if (e->type == PTP_E_STONE && now - last_stone_snd > 140) {
            ui_sound_soft(SND_CAPTURE, 0.3f);
            last_stone_snd = now;
        }
    }
}

static void liquid(int kind, float frac, Uint32 now)
{
    float r = 384.0f / PTP_FP * k;
    for (int pass = 0; pass < 4; pass++) {
        for (int i = 0; i < G.s.nb; i++) {
            const ptp_ball *b = &G.s.b[i];
            if (!b->live || b->kind != kind) continue;
            float x = sx_of((b->px + (b->x - b->px) * frac) / (float)PTP_FP),
                  y = sy_of((b->py + (b->y - b->py) * frac) / (float)PTP_FP);
            float cool = kind == PTP_LAVA && b->cool ? 1 - b->cool / 8.0f : 0;
            if (kind == PTP_STONE) {           /* stone: one dark rough mass */
                if (pass == 0) pzk_dot(x + 0.15f * r, y + 0.25f * r, 2.9f * r, 0.9f * r, (pzk_rgba){0, 0, 0, 70}, 0);
                else if (pass == 1) pzk_dot(x, y, 3.05f * r, 1.2f, (pzk_rgba){44, 38, 36, 255}, 0);
                else if (pass == 2) {
                    Uint8 g = (Uint8)(92 + (i * 37) % 22);
                    pzk_dot(x, y, 2.62f * r, 1.2f, (pzk_rgba){g, (Uint8)(g - 8), (Uint8)(g - 14), 255}, 0);
                } else pzk_dot(x - 0.35f * r, y - 0.4f * r, 0.7f * r, 0.5f * r, (pzk_rgba){200, 190, 180, 70}, 0);
            } else if (kind == PTP_WATER) {
                if (pass == 0) pzk_dot(x + 0.15f * r, y + 0.25f * r, 2.9f * r, 0.9f * r, (pzk_rgba){0, 10, 30, 70}, 0);
                else if (pass == 1) pzk_dot(x, y, 3.05f * r, 1.2f, (pzk_rgba){26, 78, 150, 255}, 0);
                else if (pass == 2) pzk_dot(x, y, 2.65f * r, 1.2f, (pzk_rgba){70, 150, 228, 255}, 0);
                else pzk_dot(x - 0.35f * r, y - 0.4f * r, 0.6f * r, 0.5f * r, (pzk_rgba){230, 246, 255, 95}, 0);
            } else {
                float fl = 0.75f + 0.25f * sinf((float)(now % 100000) / 180.0f + i * 1.7f);
                uint8_t br = (uint8_t)(244 - 150 * cool), bg = (uint8_t)(112 - 30 * cool), bb = (uint8_t)(30 + 50 * cool);
                if (pass == 0) pzk_dot(x, y, 6.0f * r, 2.5f * r, (pzk_rgba){255, 90, 20, (Uint8)(70 * fl * (1 - cool))}, 1);
                else if (pass == 1) pzk_dot(x, y, 3.05f * r, 1.2f, (pzk_rgba){(Uint8)(150 - 70 * cool), 34, 12, 255}, 0);
                else if (pass == 2) pzk_dot(x, y, 2.62f * r, 1.2f, (pzk_rgba){br, bg, bb, 255}, 0);
                else pzk_dot(x - 0.2f * r, y - 0.25f * r, 1.1f * r, 0.8f * r,
                             (pzk_rgba){255, 226, 120, (Uint8)(120 * fl * (1 - cool))}, 0);
            }
        }
    }
}

static void draw(void)
{
    if (made_k < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    advance(now);
    take_events(now);
    double tp = tpos(now);
    float frac = G.s.rest ? 1 : clampf((float)(tp - G.s.tick + 1), 0, 1);
    if (G.s.end == PTP_WON && !win_t0) win_t0 = tick_ms(G.s.tick);
    if (lost() && !lose_t0) {
        lose_t0 = now;
        ui_sound(SND_DENY);
    }
    float bw = (PTP_WU + 2 * F) * k, bh = (PTP_HU + 2 * F) * k;
    ui_put(t_board, ox, oy, ceilf(bw), ceilf(bh));
    /* drops, then balls */
    liquid(PTP_WATER, frac, now);
    liquid(PTP_LAVA, frac, now);
    liquid(PTP_STONE, frac, now);
    for (int i = 0; i < G.s.nb; i++) {
        const ptp_ball *b = &G.s.b[i];
        if (!b->live || b->kind >= PTP_WATER) continue;
        float x = sx_of((b->px + (b->x - b->px) * frac) / (float)PTP_FP),
              y = sy_of((b->py + (b->y - b->py) * frac) / (float)PTP_FP);
        int big = b->kind != PTP_STONE;
        SDL_Texture *t = t_ball[b->kind == PTP_GEM ? b->col & 3 : b->kind == PTP_DULL ? 4 : 5];
        float d = 2 * (big ? 640.0f : 384.0f) / PTP_FP * k * 1.12f;
        if (!big) t = t_ball[5];
        ui_put_c(t, x + 0.03f * d, y + 0.04f * d, d, d);
    }
    /* the pins: a shadow, the gold, a hint's glow */
    for (int i = 0; i < G.w.np; i++) {
        float ax, ay, bx, by, out;
        if (!t_pin[i] || !pin_screen(i, tp, &ax, &ay, &bx, &by, &out)) continue;
        float ang = atan2f(by - ay, bx - ax), R = 1.75f * k + 1.5f, half = pin_tw[i] / 2.0f - R;
        float cx = ax + cosf(ang) * half, cy = ay + sinf(ang) * half, alpha = 1;
        if (out > 1) alpha = clampf(1 - (out - 1) / ((float)FADE_MS * PTP_HZ / 1000 / PTP_PULL), 0, 1);
        SDL_SetTextureColorMod(t_pin[i], 0, 0, 0);
        SDL_SetTextureAlphaMod(t_pin[i], (Uint8)(90 * alpha));
        ui_put_rot(t_pin[i], cx + 0.35f * k, cy + 0.5f * k, (float)pin_tw[i], (float)pin_th[i], ang * 180 / PUZ_PI);
        SDL_SetTextureColorMod(t_pin[i], 255, 255, 255);
        SDL_SetTextureAlphaMod(t_pin[i], (Uint8)(255 * alpha));
        ui_put_rot(t_pin[i], cx, cy, (float)pin_tw[i], (float)pin_th[i], ang * 180 / PUZ_PI);
        SDL_SetTextureAlphaMod(t_pin[i], 255);
        if (i == hint_pin && G.s.pulled[i] < 0 && now - hint_t0 < HINT_MS) {
            float g = 0.55f + 0.45f * sinf((now - hint_t0) / 140.0f);
            pzk_line(ax, ay, bx, by, 4.2f * k, 2.0f * k, (pzk_rgba){255, 210, 100, (Uint8)(230 * g)}, 1);
            pzk_line(ax, ay, bx, by, 1.6f * k, 0.6f * k, (pzk_rgba){255, 240, 190, (Uint8)(200 * g)}, 1);
            ui_mark_gold_a(ax, ay, 9 * k, g);
            ui_animate();
        }
    }
    ui_put(t_front, ox, oy, ceilf(bw), ceilf(bh));
    /* what happened: sparkles in the cup, a grey ball lighting up, steam, a burn */
    for (int i = 0; i < nfx; i++) {
        float g = (float)(int)(now - fx[i].t), x = sx_of(fx[i].x), y = sy_of(fx[i].y);
        if (g < 0) continue;
        const uint8_t *c = GEMS[fx[i].col & 3];
        if (fx[i].type == PTP_E_CUP && g < 450) {
            float f = g / 450;
            ui_mark_gold_a(x, y, (2.5f + 4 * f) * k, 0.8f * (1 - f));
        } else if (fx[i].type == PTP_E_COLOUR && g < 500) {
            float f = g / 500;
            pzk_dot(x, y, (5 + 6 * f) * k, 2 * k, (pzk_rgba){c[0], c[1], c[2], (Uint8)(200 * (1 - f))}, 1);
        } else if (fx[i].type == PTP_E_STONE && g < 700) {
            float f = g / 700;
            for (int j = 0; j < 2; j++)
                pzk_dot(x + (j ? 0.8f : -0.6f) * k, y - (1 + 4 * f + j) * k, (1.5f + 2.5f * f) * k, 1.2f * k,
                        (pzk_rgba){235, 240, 245, (Uint8)(110 * (1 - f))}, 0);
        } else if (fx[i].type == PTP_E_DRAIN && g < 500) {   /* gone down the drain: a dark ripple */
            float f = g / 500;
            pzk_dot(x, y, (3 + 5 * f) * k, 1.5f * k, (pzk_rgba){0, 0, 0, (Uint8)(150 * (1 - f))}, 0);
        } else if (fx[i].type == PTP_E_BURN && g < 900) {
            float f = g / 900;
            pzk_dot(x, y, (5 + 8 * f) * k, 2.5f * k, (pzk_rgba){255, 120, 30, (Uint8)(220 * (1 - f))}, 1);
            pzk_dot(x, y - 5 * f * k, (3 + 5 * f) * k, 2 * k, (pzk_rgba){40, 36, 34, (Uint8)(140 * (1 - f))}, 0);
        } else
            continue;
        ui_animate();
    }
    if (lost()) {                               /* where it went wrong */
        float g = 0.6f + 0.4f * sinf((now - lose_t0) / 200.0f);
        float lx = sx_of(G.s.fx / (float)PTP_FP), ly = sy_of(G.s.fy / (float)PTP_FP);
        ui_mark_glow(lx, ly, 22 * k * g);
        ui_mark_glow(lx, ly, 11 * k);
        ui_animate();
    }
    if (G.s.end == PTP_WON && win_t0) {         /* the cups glow gold, a light runs over the gems in them */
        float g = (float)(int)(now - win_t0);
        for (int c = 0; c < G.w.cups; c++) {
            float xy[8];
            cup_poly(&G.w, c, xy);
            float a = puz_pulse(g, 0, 1100);
            if (a > 0) ui_mark_gold_a(sx_of((xy[0] + xy[6]) / 2), sy_of(90), 26 * k, a * 0.8f);
        }
        if (g < WIN_MS + 600) {
            for (int i = 0; i < G.s.nb; i++) {
                const ptp_ball *b = &G.s.b[i];
                if (!b->live || !b->cup || b->kind != PTP_GEM) continue;
                float a = puz_pulse(g, 300 + (i * 37) % 500, 420);
                if (a > 0) ui_mark_gold_a(sx_of(b->x / (float)PTP_FP), sy_of(b->y / (float)PTP_FP), 6 * k, a);
            }
            ui_animate();
        }
    }
    if (!G.s.rest || busy()) ui_animate();
    /* the tools */
    int glow_restart = lost() || (hint_restart && now - hint_t0 < HINT_MS);
    ui_tool(T_RESTART, ICON_REPLAY,
            (glow_restart ? UI_LIT : G.s.npulled ? 0 : UI_DIM) | (tool_down == T_RESTART ? UI_ON : 0));
    ui_tool(T_HINT, ICON_HINT, (G.s.end != PTP_PLAYING ? UI_DIM : 0) | (tool_down == T_HINT ? UI_ON : 0));
    if (glow_restart && !lost()) ui_animate();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int pin_at(float x, float y)
{
    double tp = tpos(ui_ms());
    float best = fmaxf(1.9f * k, 0.035f * fminf((float)ui.w, (float)ui.h));
    int at = -1;
    for (int i = 0; i < G.w.np; i++) {
        float ax, ay, bx, by, out;
        if (G.s.pulled[i] >= 0 || !pin_screen(i, tp, &ax, &ay, &bx, &by, &out)) continue;
        float d = pzk_seg_dist(x, y, ax, ay, bx, by);
        if (d < best) { best = d; at = i; }
    }
    return at;
}

static void press(float x, float y)
{
    tool_down = ui_tool_hit(x, y);
    pulled_this = 0;
    if (tool_down >= 0) { ui_animate(); return; }
    if (lost()) { restart(); pulled_this = 1; return; }
    if (G.s.end != PTP_PLAYING) return;
    int p = pin_at(x, y);
    if (p >= 0) {
        pull(p);
        pulled_this = 1;
    }
}

static void motion(float x, float y)
{
    if (tool_down >= 0 || pulled_this || G.s.end != PTP_PLAYING) return;
    int p = pin_at(x, y);                       /* a swipe across a pin */
    if (p >= 0) {
        pull(p);
        pulled_this = 1;
    }
}

static void release(float x, float y)
{
    if (tool_down >= 0) {
        int t = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) != t) return;
        if (t == T_RESTART) restart();
        else hint();
    }
}

static void key(SDL_Keycode kc)
{
    if (kc == SDLK_r) restart();
    else if (kc >= SDLK_1 && kc <= SDLK_6 && G.s.end == PTP_PLAYING) pull((int)(kc - SDLK_1));
}

static void bar(int player, box b)
{
    (void)player;
    int left = G.s.target - G.s.incup;
    ui_bar_fit(b, (ui_count[]){{NULL, left < 0 ? 0 : left}}, 1, 0);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* a level drawn small (pixels only: the menu's thread too): the frame, the glass, the bars, the pins, the things */
static void paint_scene(const ptp_map *m, int size, uint8_t *rgba)
{
    ptp_world *w = malloc(sizeof *w);
    ptp_sim *s = malloc(sizeof *s);
    if (!w || !s) { free(w); free(s); return; }
    ptp_build(m, w, s);
    float kk = size * 0.96f / (PTP_HU + 2 * F);
    int W = (int)((PTP_WU + 2 * F) * kk), H = (int)((PTP_HU + 2 * F) * kk);
    uint8_t *o = W > 8 && H > 8 ? malloc((size_t)W * H * 4) : NULL;
    if (o) {
        paint_board(w, kk, W, H, o);
        float bx = F * kk, by = F * kk;
        for (int i = 0; i < s->nb; i++) {
            const ptp_ball *b = &s->b[i];
            const uint8_t *c = b->kind == PTP_GEM ? GEMS[b->col & 3] : b->kind == PTP_DULL ? GREY
                             : b->kind == PTP_WATER ? (const uint8_t[]){70, 150, 228} : b->kind == PTP_LAVA
                             ? (const uint8_t[]){244, 112, 30} : STONE;
            float r = (b->kind <= PTP_DULL ? 640.0f : 384.0f * 1.2f) / PTP_FP * kk;
            pzk_ball(o, W, H, bx + b->x * kk / PTP_FP, by + b->y * kk / PTP_FP, r, c, 1, b->kind <= PTP_DULL ? 0.8f : 0.3f);
        }
        for (int i = 0; i < w->np; i++) {
            float ax = bx + w->pax[i] * kk / PTP_FP, ay = by + w->pay[i] * kk / PTP_FP, pbx = bx + w->pbx[i] * kk / PTP_FP,
                  pby = by + w->pby[i] * kk / PTP_FP;
            paint_bar(o, W, H, ax, ay, pbx, pby, fmaxf(0.8f, 0.9f * kk), 2);
            pzk_arc(o, W, H, ax, ay, 1.0f * kk, 1.8f * kk, 0, 360, GOLD, 1);
        }
        uint8_t *fr = malloc((size_t)W * H * 4);
        if (fr) {
            paint_front(w, kk, W, H, fr);
            art_blend(o, W, H, fr, W, H, 0, 0);
            free(fr);
        }
        art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    }
    free(o);
    free(w);
    free(s);
}

static void paint_level(int level, int size, uint8_t *rgba)
{
    ptp_map *m = malloc(sizeof *m);
    if (m && level >= 0 && level < PTP_LEVELS && ptp_parse(m, ptp_levels[level].text)) paint_scene(m, size, rgba);
    free(m);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    if (variant < 0) { paint_level(PTP_LEVELS * 2 / 3, size, rgba); return; }
    ptp_map *m = malloc(sizeof *m);             /* the random tab's cards: a puzzle of each pool */
    if (m && ptp_parse(m, ptp_pool[variant % PTP_VARIANTS][0].text)) paint_scene(m, size, rgba);
    free(m);
}

static void level_thumb(int level, int size, uint8_t *rgba) { paint_level(level, size, rgba); }

/* a chapter's picture: a grey ball by a gem; water and lava; lava by a grey ball; two cups; lava, a grey ball, a gem;
 * a pin */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    float s = (float)size;
    static const uint8_t W3[3] = {70, 150, 228}, L3[3] = {244, 112, 30};
    switch (chapter) {
    case 0:
        pzk_ball(rgba, size, size, 0.34f * s, 0.5f * s, 0.24f * s, GREY, 1, 0.7f);
        pzk_ball(rgba, size, size, 0.68f * s, 0.5f * s, 0.24f * s, GEMS[0], 1, 1);
        break;
    case 1:
        pzk_ball(rgba, size, size, 0.33f * s, 0.55f * s, 0.22f * s, W3, 1, 0.6f);
        pzk_ball(rgba, size, size, 0.67f * s, 0.45f * s, 0.22f * s, L3, 1, 0.6f);
        break;
    case 2:
        pzk_ball(rgba, size, size, 0.34f * s, 0.5f * s, 0.24f * s, L3, 1, 0.6f);
        pzk_ball(rgba, size, size, 0.68f * s, 0.5f * s, 0.24f * s, GREY, 1, 0.7f);
        break;
    case 3:
        for (int c = 0; c < 2; c++) {
            float x = (0.28f + 0.44f * c) * s;
            const float cup[] = {x - 0.17f * s, 0.32f * s, x - 0.12f * s, 0.8f * s, x + 0.12f * s, 0.8f * s, x + 0.17f * s,
                                 0.32f * s};
            for (int y = 0; y < size; y++)
                for (int xx = 0; xx < size; xx++)
                    pzk_over(rgba + 4 * ((size_t)y * size + xx), GLASS, 0.45f * pzt_cover(cup, 4, (float)xx, (float)y));
            pzk_ball(rgba, size, size, x, 0.64f * s, 0.11f * s, GEMS[c ? 2 : 0], 1, 1);
            pzk_stroke(rgba, size, size, x - 0.17f * s, 0.32f * s, x + 0.17f * s, 0.32f * s, 0.035f * s, GEMS[c ? 2 : 0], 1);
        }
        break;
    case 4:
        pzk_ball(rgba, size, size, 0.28f * s, 0.6f * s, 0.18f * s, L3, 1, 0.6f);
        pzk_ball(rgba, size, size, 0.5f * s, 0.38f * s, 0.18f * s, GREY, 1, 0.7f);
        pzk_ball(rgba, size, size, 0.72f * s, 0.6f * s, 0.18f * s, GEMS[3], 1, 1);
        break;
    default:
        paint_bar(rgba, size, size, 0.34f * s, 0.62f * s, 0.9f * s, 0.24f * s, 0.06f * s, 2);
        pzk_arc(rgba, size, size, 0.26f * s, 0.68f * s, 0.07f * s, 0.15f * s, 0, 360, GOLD, 1);
        break;
    }
}

static const game pins_game = {
    .id = "pins", .order = 461, .variants = PTP_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = NULL, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = PTP_LEVELS, .chapters = PTP_CHAPTERS, .chapter_of = ptp_chapter_of, .start_random = start_random,
    .chapter_icon = chapter_icon, .level_thumb = level_thumb, .level_stars = level_stars, .fill = 1,
    .tools = T_COUNT,
};
REGISTER_GAME(pins_game)
