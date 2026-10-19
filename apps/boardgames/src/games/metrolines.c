/*
 * Metro Lines in RetroStone Board, for one player: a city's map on cream paper in the house's frame, rivers in soft
 * blue, stations as small white shapes outlined in ink (the rules: metrolines_rules.c). Drag from a station through
 * others to draw a line; drag a line's end (its T) to lengthen it, or back over its last stop to shorten it; drag a
 * piece of a line onto a station to take the line through it. The lines are drawn in calm colours, upright or at 45
 * degrees with rounded bends, side by side where they share a way, on a pale bridge where they cross water. Little
 * trains run on them, their passengers as tiny white shapes; waiting passengers stand beside their station as tiny
 * ink shapes. A crowded station's timer grows as a ring round it, amber then red. Each week the map greys and two
 * gifts are offered on round cards; a carriage or an interchange is then put where the player taps.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The clock: the simulation runs in ticks of MTL_TICK_MS of the game's own time, which moves with the shell's clock
 * (ui_ms) only while the game is drawn and not paused (twice as fast at the second speed): draw() moves it on by the
 * time since the last frame drawn, unless that frame was long ago in real time (ui_real_ms: the game was away, the
 * app in the background) or the clock leapt (a kept game played again). Input changes the network only between two
 * frames, on the state the last frame left, so a game played again from its record comes out the same. Sounds: a
 * piano's soft note for each passenger delivered (the piano kit's mixer), a low one when a station starts to crowd.
 * Tools: pause, the speed, the hint (the planner's next move, drawn in gold). The palette under the map: the lines
 * (a tap on a line's dot, then another, takes the line away) and the tunnels left. Undo: the last change of the lines.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/metrolines.h"
#include "games/pianokit.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define MARGIN 0.95f                            /* paper round the stations' grid (units) */
#define LW 0.21f                                /* a line's width */
#define RS 0.27f                                /* a station's radius */
#define HIT 0.62f                               /* a finger near a station */
#define BEND_R 0.6f
#define AWAY_MS 1000                            /* frames further apart in real time: the game was away */
#define LEAP_MS 300000
#define END_MS 1600
#define UNDO 32
#define FX 40
#define PAL_MAX 10

enum { T_PAUSE, T_FAST, T_HINT, TOOLS };
enum { D_NONE, D_NEW, D_EXTEND, D_INSERT };

static const uint8_t LINE_RGB[MTL_MAXL][3] = {{216, 72, 58},  {56, 116, 196}, {236, 168, 40}, {70, 158, 92},
                                              {148, 92, 176}, {38, 164, 168}, {222, 118, 158}};
static const uint8_t INK[3] = {52, 48, 46}, PAPER[3] = {245, 239, 226}, WATER[3] = {176, 207, 228},
                     BANK[3] = {146, 182, 212}, WHITE[3] = {255, 255, 255}, WARN[3] = {236, 128, 52},
                     RED[3] = {210, 58, 44};
static const int NOTE[MTL_SHAPES] = {76, 79, 81, 84, 86, 88, 91};

static mtl_state S;
static int variant, hints, serial, best[MTL_VARIANTS], best_read;
static struct { Uint32 last, real; int on; int64_t gms; } clk;   /* gms: the game's own time (ms) */
static int paused, fast;
static mtl_train prev_t[MTL_MAXL];
static int32_t prev_tick = -1;
static mtl_line undo_st[UNDO][MTL_MAXL];
static int nundo;
static struct {
    int kind, line, tail, link;
    int chain[MTL_STOPS + 2], n, cuts, closes;
    float fx, fy;                               /* the finger (world units) */
} drag;
static int tool_down = -1, armed = -1;
static Uint32 armed_t, hint_t, end_ui, week_ui, deny_t;
static char hint_act[MTL_ACT * 2];
static int seen_ev, warned[MTL_MAXS];
static struct { int kind, st, b; int64_t t; } fx[FX];
static int nfx;
static Uint32 chime_t;

/* ---- the geometry --------------------------------------------------------------------------------------------- */
static box room;
static int turned, pw, ph, fr, made_serial = -1, made_pw = -1, made_ph = -1, made_small = -1;
static float k, px0, py0, EW, EH, bx0, by0, strip_y, strip_h;
static SDL_Texture *t_paper, *t_st[MTL_SHAPES], *t_pass[MTL_SHAPES], *t_ride[MTL_SHAPES], *t_ring, *t_train,
                   *t_person, *t_flag, *t_star, *t_bridge;
static int st_px, pass_px, ring_px, tr_w, tr_h;

static void w2s(float ux, float uy, float *sx, float *sy)
{
    if (!turned) { *sx = px0 + (ux + MARGIN) * k; *sy = py0 + (uy + MARGIN) * k; }
    else { *sx = px0 + (EH - (uy + MARGIN)) * k; *sy = py0 + (ux + MARGIN) * k; }
}

static void s2w(float sx, float sy, float *ux, float *uy)
{
    if (!turned) { *ux = (sx - px0) / k - MARGIN; *uy = (sy - py0) / k - MARGIN; }
    else { *uy = EH - (sx - px0) / k - MARGIN; *ux = (sy - py0) / k - MARGIN; }
}

static void st_xy(int st, float *x, float *y) { w2s(S.L.st[st].x, S.L.st[st].y, x, y); }

/* the lines' ways as drawn: each link's points (units), side by side where links are shared, bends rounded */
#define VP 16
static struct { int n; float x[VP], y[VP], len; } vp[MTL_MAXL][MTL_STOPS];
static struct { int n; float a[4], b[4]; } wetv[MTL_MAXL][MTL_STOPS];   /* the stretches over water (0..1 along) */
static int paths_edits = -1, paths_nst = -1;

static void vp_point(int l, int k2, float d, float *x, float *y, float *ang)   /* d: units from its start */
{
    int n = vp[l][k2].n;
    float *X = vp[l][k2].x, *Y = vp[l][k2].y;
    if (n < 2) { *x = X[0]; *y = Y[0]; *ang = 0; return; }
    for (int i = 0; i + 1 < n; i++) {
        float sl = hypotf(X[i + 1] - X[i], Y[i + 1] - Y[i]);
        if (d <= sl || i + 2 == n) {
            float f = sl > 0 ? clampf(d / sl, 0, 1) : 0;
            *x = X[i] + (X[i + 1] - X[i]) * f;
            *y = Y[i] + (Y[i + 1] - Y[i]) * f;
            *ang = atan2f(Y[i + 1] - Y[i], X[i + 1] - X[i]);
            return;
        }
        d -= sl;
    }
}

static void build_paths(void)
{
    if (paths_edits == S.edits && paths_nst == S.nst) return;
    paths_edits = S.edits;
    paths_nst = S.nst;
    for (int l = 0; l < MTL_MAXL; l++) {
        const mtl_line *L = &S.l[l];
        int nk = L->n < 2 ? 0 : L->loop ? L->n : L->n - 1;
        for (int q = 0; q < nk; q++) {
            int a = L->stop[q], b = L->stop[(q + 1) % L->n], lo = a < b ? a : b, hi = a < b ? b : a;
            int cnt = 0, idx = 0;                /* the lines sharing this way */
            for (int m = 0; m < MTL_MAXL; m++) {
                const mtl_line *M = &S.l[m];
                int mk = M->n < 2 ? 0 : M->loop ? M->n : M->n - 1, has = 0;
                for (int j = 0; j < mk && !has; j++) {
                    int c = M->stop[j], d = M->stop[(j + 1) % M->n];
                    if ((c == lo && d == hi) || (c == hi && d == lo)) has = 1;
                }
                if (has) { if (m < l) idx++; cnt++; }
            }
            float off = (idx - (cnt - 1) / 2.0f) * LW * 1.18f;
            mtl_pt p[3];
            int n = mtl_route(&S, lo, hi, L->bend[q], p);
            float cx[3], cy[3];
            for (int i = 0; i < n; i++) { cx[i] = p[i].x / (float)MTL_FP; cy[i] = p[i].y / (float)MTL_FP; }
            float ox[3], oy[3], nx[2], ny[2];
            for (int i = 0; i + 1 < n; i++) {
                float dx = cx[i + 1] - cx[i], dy = cy[i + 1] - cy[i], dl = hypotf(dx, dy);
                nx[i] = dl > 0 ? -dy / dl : 0;
                ny[i] = dl > 0 ? dx / dl : 0;
            }
            ox[0] = cx[0] + off * nx[0], oy[0] = cy[0] + off * ny[0];
            ox[n - 1] = cx[n - 1] + off * nx[n - 2], oy[n - 1] = cy[n - 1] + off * ny[n - 2];
            if (n == 3) {
                float dd = 1 + nx[0] * nx[1] + ny[0] * ny[1];
                ox[1] = cx[1] + off * (nx[0] + nx[1]) / dd;
                oy[1] = cy[1] + off * (ny[0] + ny[1]) / dd;
            }
            /* the bend rounded: a curve from a point before it to one after it */
            float X[VP], Y[VP];
            int m = 0;
            X[m] = ox[0], Y[m++] = oy[0];
            if (n == 3) {
                float l1 = hypotf(ox[1] - ox[0], oy[1] - oy[0]), l2 = hypotf(ox[2] - ox[1], oy[2] - oy[1]);
                float t = fminf(BEND_R * 0.414f, 0.45f * fminf(l1, l2));
                float ax = ox[1] - (ox[1] - ox[0]) / l1 * t, ay = oy[1] - (oy[1] - oy[0]) / l1 * t;
                float bx = ox[1] + (ox[2] - ox[1]) / l2 * t, by = oy[1] + (oy[2] - oy[1]) / l2 * t;
                for (int j = 0; j <= 8; j++) {
                    float f = j / 8.0f, g = 1 - f;
                    X[m] = g * g * ax + 2 * g * f * ox[1] + f * f * bx;
                    Y[m++] = g * g * ay + 2 * g * f * oy[1] + f * f * by;
                }
            }
            X[m] = ox[n - 1], Y[m++] = oy[n - 1];
            int rev = a > b;                     /* from stop q to stop q + 1 */
            float len = 0;
            for (int i = 0; i < m; i++) {
                int j = rev ? m - 1 - i : i;
                vp[l][q].x[i] = X[j];
                vp[l][q].y[i] = Y[j];
                if (i) len += hypotf(vp[l][q].x[i] - vp[l][q].x[i - 1], vp[l][q].y[i] - vp[l][q].y[i - 1]);
            }
            vp[l][q].n = m;
            vp[l][q].len = len;
            /* where it is over water: a pale bridge under it */
            wetv[l][q].n = 0;
            int was = 0, steps = (int)(len * 12) + 2;
            float start = 0;
            for (int j = 0; j <= steps; j++) {
                float x, y, ang, d = len * j / steps;
                vp_point(l, q, d, &x, &y, &ang);
                int w = mtl_wet(&S, (int32_t)(x * MTL_FP), (int32_t)(y * MTL_FP));
                if (w && !was) start = d / len;
                if (!w && was && wetv[l][q].n < 4) {
                    wetv[l][q].a[wetv[l][q].n] = start;
                    wetv[l][q].b[wetv[l][q].n++] = d / len;
                }
                was = w;
            }
        }
    }
}

/* ---- the pictures ----------------------------------------------------------------------------------------------- */
static void fig_shape(art_fig *f, int shape, float cx, float cy, float r)
{
    float xy[24];
    int n = mtl_shape_poly(shape, xy);
    if (!n) { art_ellipse(f, ART_PART, cx, cy, r, r); return; }
    for (int i = 0; i < n; i++) { xy[2 * i] = cx + xy[2 * i] * r; xy[2 * i + 1] = cy + xy[2 * i + 1] * r; }
    art_poly(f, ART_PART, xy, n);
}

/* a shape into a picture (size x size): fill, an outline `line` wide (of the picture), r of the picture */
static void paint_shape(uint8_t *o, int size, int shape, const uint8_t fill[3], const uint8_t ink[3], float r,
                        float line)
{
    art_fig *f = art_fig_new();
    fig_shape(f, shape, 0.5f, 0.5f, r);
    art_fig_paint(o, size, f, fill, ink, line, 0);
}

static SDL_Texture *shape_tex(int size, int shape, const uint8_t fill[3], const uint8_t ink[3], float r, float line)
{
    uint8_t *o = ui_buf(size, size);
    memset(o, 0, (size_t)size * size * 4);
    paint_shape(o, size, shape, fill, ink, r, line);
    return ui_tex(size, size, o);
}

/* the paper: cream with fine fibres, darker by the frame; the water: soft blue, its banks a shade darker */
static void paint_water(uint8_t *o, int W, int H, float ox, float oy, float kk, int tn, const mtl_state *s, int x0,
                        int y0, int x1, int y1)
{
    float *m = calloc((size_t)W * H, sizeof *m), *mb = calloc((size_t)W * H, sizeof *mb);
    if (!m || !mb) { free(m); free(mb); return; }
    float ext_h = (s->L.h - 1) + 2 * MARGIN;
#define TX(ux, uy) (tn ? ox + (ext_h - ((uy) + MARGIN)) * kk : ox + ((ux) + MARGIN) * kk)
#define TY(ux, uy) (tn ? oy + ((ux) + MARGIN) * kk : oy + ((uy) + MARGIN) * kk)
    for (int r = 0; r < s->L.nriver; r++) {
        float hw = s->L.rhw[r] / 4.0f * kk, bank = fmaxf(1.2f, 0.07f * kk);
        for (int i = 0; i + 1 < s->nrp[r]; i++) {
            float ax = s->rp[r][i].x / (float)MTL_FP, ay = s->rp[r][i].y / (float)MTL_FP;
            float bx = s->rp[r][i + 1].x / (float)MTL_FP, by = s->rp[r][i + 1].y / (float)MTL_FP;
            pzk_stroke_mask(mb, W, H, TX(ax, ay), TY(ax, ay), TX(bx, by), TY(bx, by), hw + bank);
            pzk_stroke_mask(m, W, H, TX(ax, ay), TY(ax, ay), TX(bx, by), TY(bx, by), hw);
        }
    }
    for (int q = 0; q < s->L.nlake; q++) {
        float cx = s->L.lx[q] / 4.0f, cy = s->L.ly[q] / 4.0f, rx = s->L.lrx[q] / 4.0f * kk, ry = s->L.lry[q] / 4.0f * kk;
        float sx = TX(cx, cy), sy = TY(cx, cy);
        if (tn) { float t = rx; rx = ry; ry = t; }
        float bank = fmaxf(1.2f, 0.07f * kk);
        for (int y = (int)fmaxf(0, sy - ry - bank - 2); y < H && y <= sy + ry + bank + 2; y++)
            for (int x = (int)fmaxf(0, sx - rx - bank - 2); x < W && x <= sx + rx + bank + 2; x++) {
                float dx = (x + 0.5f - sx) / rx, dy = (y + 0.5f - sy) / ry, d = sqrtf(dx * dx + dy * dy);
                float edge = (1 - d) * fminf(rx, ry);   /* pixels inside, roughly */
                float c = clampf(edge + 0.5f, 0, 1), cb = clampf(edge + bank + 0.5f, 0, 1);
                if (c > m[y * W + x]) m[y * W + x] = c;
                if (cb > mb[y * W + x]) mb[y * W + x] = cb;
            }
    }
#undef TX
#undef TY
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int i = y * W + x;
            if (mb[i] > 0) pzk_over(o + 4 * i, BANK, 0.9f * mb[i]);
            if (m[i] > 0) {
                uint8_t c[3];
                float sh = 1 + 0.025f * art_noise(x / 2, y / 2);
                for (int j = 0; j < 3; j++) c[j] = (uint8_t)clampf(WATER[j] * sh, 0, 255);
                pzk_over(o + 4 * i, c, m[i]);
            }
        }
    free(m);
    free(mb);
}

static void paint_paper(uint8_t *o, int W, int x0, int y0, int x1, int y1)
{
    float edge = fmaxf(3, 0.012f * (x1 - x0));
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float d = fminf(fminf(x - x0, x1 - 1 - x), fminf(y - y0, y1 - 1 - y));
            float sh = 1 + 0.018f * art_noise(x, y) + 0.012f * art_noise(x / 5 + 31, y / 5 + 17) -
                       0.07f * clampf(1 - d / edge, 0, 1);
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            for (int j = 0; j < 3; j++) p[j] = (uint8_t)clampf(PAPER[j] * sh, 0, 255);
            p[3] = 255;
        }
}

static void make_tex(SDL_Texture **t, int size, void (*paint)(int, int, uint8_t *), int kind)
{
    ui_free(t);
    if (size < 2) return;
    uint8_t *o = ui_buf(size, size);
    memset(o, 0, (size_t)size * size * 4);
    paint(kind, size, o);
    *t = ui_tex(size, size, o);
}

static void paint_icon(int kind, int size, uint8_t *o) { art_icon(kind, size, o); }

static void layout(box b)
{
    room = b;
    float u = ui.unit;
    strip_h = floorf(0.95f * u);
    float gap = floorf(0.2f * u), rw = b.w, rh = b.h - strip_h - gap;
    EW = (S.L.w - 1) + 2 * MARGIN;
    EH = (S.L.h - 1) + 2 * MARGIN;
    fr = (int)fmaxf(3, floorf(0.028f * fminf(b.w, b.h)));
    float s0 = fminf((rw - 2 * fr) / EW, (rh - 2 * fr) / EH), s1 = fminf((rw - 2 * fr) / EH, (rh - 2 * fr) / EW);
    turned = s1 > s0 * 1.04f;
    k = turned ? s1 : s0;
    pw = (int)floorf(k * (turned ? EH : EW));
    ph = (int)floorf(k * (turned ? EW : EH));
    k = turned ? pw / EH : pw / EW;
    float total = ph + 2 * fr + gap + strip_h;
    bx0 = floorf(b.x + (b.w - (pw + 2 * fr)) / 2);
    by0 = floorf(b.y + (b.h - total) / 2);
    px0 = bx0 + fr;
    py0 = by0 + fr;
    strip_y = by0 + ph + 2 * fr + gap;
    ui_board_used(bx0, by0, (float)(pw + 2 * fr), total);
    if (made_serial != serial || made_pw != pw || made_ph != ph) {
        made_serial = serial;
        made_pw = pw;
        made_ph = ph;
        int W = pw + 2 * fr, H = ph + 2 * fr;
        uint8_t *o = malloc((size_t)W * H * 4);
        ui_free(&t_paper);
        if (o) {
            art_plank(pw, ph, fr, o);
            paint_paper(o, W, fr, fr, fr + pw, fr + ph);
            paint_water(o, W, H, (float)fr, (float)fr, k, turned, &S, fr, fr, fr + pw, fr + ph);
            t_paper = ui_tex(W, H, o);
            free(o);
        }
        st_px = (int)ceilf(2.0f * RS * 1.5f * k);
        pass_px = (int)ceilf(0.27f * k);
        ring_px = (int)ceilf(1.15f * k);
        for (int sh = 0; sh < MTL_SHAPES; sh++) {
            ui_free(&t_st[sh]);
            ui_free(&t_pass[sh]);
            ui_free(&t_ride[sh]);
            t_st[sh] = shape_tex(st_px, sh, WHITE, INK, RS * k / st_px, 0.085f * k / st_px);
            t_pass[sh] = shape_tex(pass_px, sh, INK, INK, 0.36f, 0);
            t_ride[sh] = shape_tex(pass_px, sh, WHITE, WHITE, 0.36f, 0);
        }
        ui_free(&t_ring);                       /* an interchange: a thick ring round the station */
        uint8_t *q = ui_buf(ring_px, ring_px);
        memset(q, 0, (size_t)ring_px * ring_px * 4);
        art_fig *f = art_fig_new();
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.36f, 0.47f, 0, 360);
        art_fig_paint(q, ring_px, f, INK, INK, 0, 0);
        t_ring = ui_tex(ring_px, ring_px, q);
        ui_free(&t_train);                      /* a train's car: white (tinted by its line), rounded */
        tr_w = (int)ceilf(0.62f * k) + 2;
        tr_h = (int)ceilf(0.36f * k) + 2;
        q = ui_buf(tr_w, tr_h);
        memset(q, 0, (size_t)tr_w * tr_h * 4);
        float rr = 0.09f * k;
        for (int y = 0; y < tr_h; y++)
            for (int x = 0; x < tr_w; x++) {
                float dx = fmaxf(0, fabsf(x + 0.5f - tr_w / 2.0f) - (tr_w / 2.0f - 1 - rr));
                float dy = fmaxf(0, fabsf(y + 0.5f - tr_h / 2.0f) - (tr_h / 2.0f - 1 - rr));
                float c = clampf(rr - hypotf(dx, dy) + 0.5f, 0, 1);
                float lit = 1 - 0.12f * (y + 0.5f) / tr_h;
                uint8_t *p = q + 4 * (y * tr_w + x);
                p[0] = p[1] = p[2] = (uint8_t)(255 * lit);
                p[3] = (uint8_t)(255 * c);
            }
        t_train = ui_tex(tr_w, tr_h, q);
        paths_edits = -1;
    }
    int small = (int)(0.6f * u);
    if (made_small != small) {
        made_small = small;
        make_tex(&t_person, small, paint_icon, ICON_PERSON);
        make_tex(&t_flag, small, paint_icon, ICON_PAR);
        make_tex(&t_star, small, paint_icon, ICON_STAR);
    }
    (void)t_bridge;
}

/* ---- the clock --------------------------------------------------------------------------------------------------- */
static int64_t tick_ms(int32_t t) { return (int64_t)t * MTL_TICK_MS; }

static void sim_to(int64_t gms)                 /* the simulation on to the game's time gms */
{
    int32_t target = (int32_t)(gms / MTL_TICK_MS);
    for (int guard = 0; S.tick < target && mtl_running(&S) && guard < 25 * 60 * 30; guard++) {
        if (S.tick == target - 1) { memcpy(prev_t, S.t, sizeof prev_t); prev_tick = S.tick; }
        mtl_step(&S);
    }
    if (!mtl_running(&S)) clk.gms = tick_ms(S.tick);   /* a gift waits, or the end: the clock stops there */
}

static void clock_frame(Uint32 now)
{
    Uint32 real = ui_real_ms();
    if (clk.on) {
        Uint32 dt = now - clk.last;
        int away = real - clk.real > AWAY_MS || dt > LEAP_MS;
        if (!away && !paused && mtl_running(&S)) clk.gms += (int64_t)dt * (fast ? 2 : 1);
    }
    clk.on = 1;
    clk.last = now;
    clk.real = real;
    sim_to(clk.gms);
}

static void rebase(void)                        /* the game's clock where the simulation is */
{
    clk.gms = tick_ms(S.tick);
    clk.on = 0;
    memcpy(prev_t, S.t, sizeof prev_t);
    prev_tick = S.tick;
    seen_ev = S.nev;
    nfx = 0;
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    memset(&drag, 0, sizeof drag);
    tool_down = armed = -1;
    hint_act[0] = 0;
    end_ui = week_ui = 0;
    memset(warned, 0, sizeof warned);
}

static void read_best(void)
{
    if (best_read) return;
    best_read = 1;
    uint8_t b[4 * MTL_VARIANTS];
    if (ui_save_get("metrolines/best", b, sizeof b) == (int)sizeof b)
        for (int v = 0; v < MTL_VARIANTS; v++)
            best[v] = b[4 * v] | b[4 * v + 1] << 8 | b[4 * v + 2] << 16 | b[4 * v + 3] << 24;
}

static void keep_best(void)
{
    if (ui.level >= 0 || S.delivered <= best[variant]) return;
    best[variant] = S.delivered;
    uint8_t b[4 * MTL_VARIANTS];
    for (int v = 0; v < MTL_VARIANTS; v++)
        for (int i = 0; i < 4; i++) b[4 * v + i] = (uint8_t)(best[v] >> (8 * i));
    ui_save_set("metrolines/best", b, sizeof b);
}

static void begin(const mtl_level *L)
{
    mtl_begin(&S, L);
    serial++;
    hints = 0;
    paused = fast = 0;
    nundo = 0;
    calm();
    rebase();
    read_best();
    paths_edits = -1;
    if (room.w > 0) layout(room);
}

static void start(int level)
{
    mtl_level L;
    if (!mtl_level_get(level, &L)) mtl_level_get(0, &L);
    begin(&L);
}

static void start_random(int v)
{
    mtl_level L;
    variant = v < 0 || v >= MTL_VARIANTS ? 0 : v;
    mtl_variant_level(variant, (uint32_t)ui_random(0x7fffffff) + 1u, &L);
    begin(&L);
}

static int ended(void) { return mtl_ended(&S); }
static int endless(void) { return S.L.goal[2] <= 0; }

static int status(void)
{
    if (!ended()) return GS_PLAY;
    if (endless()) return GS_WIN0;
    return mtl_stars(&S) >= 1 ? GS_WIN0 : GS_WIN1;
}

static int turn(void) { return 0; }
static int score(void) { return S.delivered; }
static int par(void) { return endless() ? 0 : S.L.goal[2]; }
static int level_stars(void)
{
    int base = endless() ? mtl_endless_stars(variant, S.delivered) : mtl_stars(&S);
    return pzk_stars(base, hints, 0);
}

static int busy(void) { return ended() && (!end_ui || ui_ms() - end_ui < END_MS); }

/* ---- the edits --------------------------------------------------------------------------------------------------- */
static void keep_undo(void)
{
    if (nundo == UNDO) { memmove(undo_st, undo_st + 1, sizeof undo_st[0] * (UNDO - 1)); nundo--; }
    memcpy(undo_st[nundo++], S.l, sizeof S.l);
}

static int undo(void)
{
    calm();
    while (nundo > 0) {
        nundo--;
        if (ended()) return 0;
        if (mtl_set_lines(&S, undo_st[nundo])) { ui_sound(SND_MOVE); return 1; }
    }
    return 0;
}

static int station_at(float ux, float uy, float r)   /* the nearest station shown within r units, -1 */
{
    int best_i = -1;
    float bd = r * r;
    for (int i = 0; i < S.nst; i++) {
        float dx = S.L.st[i].x - ux, dy = S.L.st[i].y - uy, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best_i = i; }
    }
    return best_i;
}

/* the line's end: its T beyond the end stop, away from the stop before it (units) */
static void end_handle(int l, int tail, float *x, float *y, float *ang)
{
    const mtl_line *L = &S.l[l];
    int e = tail ? L->stop[L->n - 1] : L->stop[0], q = tail ? L->n - 2 : 0;
    float ex = S.L.st[e].x, ey = S.L.st[e].y, dx, dy;
    /* the last piece of the way into the end stop: its direction, straight on */
    int n = vp[l][q].n;
    if (tail) { dx = vp[l][q].x[n - 1] - vp[l][q].x[n - 2]; dy = vp[l][q].y[n - 1] - vp[l][q].y[n - 2]; }
    else { dx = vp[l][q].x[0] - vp[l][q].x[1]; dy = vp[l][q].y[0] - vp[l][q].y[1]; }
    float dl = hypotf(dx, dy);
    if (dl <= 0) { dx = 1; dy = 0; dl = 1; }
    *ang = atan2f(dy, dx);
    float ox = tail ? vp[l][q].x[n - 1] - ex : vp[l][q].x[0] - ex, oy = tail ? vp[l][q].y[n - 1] - ey : vp[l][q].y[0] - ey;
    *x = ex + ox + dx / dl * 0.62f;
    *y = ey + oy + dy / dl * 0.62f;
}

static float seg_dist(float x, float y, float ax, float ay, float bx, float by)
{
    return pzk_seg_dist(x, y, ax, ay, bx, by);
}

static int link_at(float ux, float uy, int *line, int *link)   /* a line's piece under the finger */
{
    float bd = 0.36f;
    int found = 0;
    for (int l = 0; l < MTL_MAXL; l++) {
        const mtl_line *L = &S.l[l];
        int nk = L->n < 2 ? 0 : L->loop ? L->n : L->n - 1;
        for (int q = 0; q < nk; q++)
            for (int i = 0; i + 1 < vp[l][q].n; i++) {
                float d = seg_dist(ux, uy, vp[l][q].x[i], vp[l][q].y[i], vp[l][q].x[i + 1], vp[l][q].y[i + 1]);
                if (d < bd) { bd = d; *line = l; *link = q; found = 1; }
            }
    }
    return found;
}

static int chain_has(int st)
{
    for (int i = 0; i < drag.n; i++)
        if (drag.chain[i] == st) return 1;
    return 0;
}

/* the tunnels a drag's new pieces would take beyond those its line has now */
static int chain_tunnels(void)
{
    int t = 0;
    for (int i = 0; i + 1 < drag.n; i++) {
        int c;
        mtl_best_bend(&S, drag.chain[i], drag.chain[i + 1], &c);
        t += c;
    }
    return t;
}

static int tunnels_left(void) { return S.tunnels - mtl_tunnels_used(&S); }

static void deny(void)
{
    ui_sound(SND_DENY);
    deny_t = ui_ms() ? ui_ms() : 1;
    ui_animate();
}

static void drag_move(float ux, float uy)
{
    drag.fx = ux;
    drag.fy = uy;
    if (drag.kind != D_NEW && drag.kind != D_EXTEND) return;
    int st = station_at(ux, uy, 0.5f);
    if (st < 0) return;
    const mtl_line *L = &S.l[drag.line];
    if (drag.n >= 2 && st == drag.chain[drag.n - 2]) { drag.n--; drag.closes = 0; ui_sound_soft(SND_TAP, 0.4f); return; }
    if (drag.kind == D_EXTEND && drag.n == 1 && L->n - drag.cuts >= 2) {   /* back along the line: shorter */
        int k2 = drag.tail ? L->n - 2 - drag.cuts : 1 + drag.cuts;
        if (k2 >= 0 && k2 < L->n && L->stop[k2] == st) {
            drag.cuts++;
            drag.chain[0] = st;
            ui_sound_soft(SND_TAP, 0.4f);
            return;
        }
    }
    if (drag.n < 1 || drag.closes || st == drag.chain[drag.n - 1]) return;
    if (drag.kind == D_EXTEND && drag.cuts == 0 && L->n >= 3 && st == (drag.tail ? L->stop[0] : L->stop[L->n - 1]) &&
        drag.n == 1) {                          /* round to the other end: a loop */
        drag.closes = 1;
        ui_sound_soft(SND_PLACE, 0.6f);
        return;
    }
    if (drag.kind == D_NEW && drag.n >= 3 && st == drag.chain[0]) { drag.closes = 1; ui_sound_soft(SND_PLACE, 0.6f); return; }
    if (chain_has(st) || (drag.kind == D_EXTEND && mtl_on_line(&S, drag.line, st) >= 0) || drag.n >= MTL_STOPS) return;
    drag.chain[drag.n++] = st;
    int need = chain_tunnels();
    if (drag.kind == D_EXTEND) need += 0;
    if (need > tunnels_left()) { drag.n--; deny(); return; }
    ui_sound_soft(SND_TAP, 0.55f);
}

static void drag_commit(void)
{
    int kind = drag.kind;
    drag.kind = D_NONE;
    if (ended()) return;
    if (kind == D_NEW) {
        if (drag.n < 2) return;
        int st[MTL_STOPS + 1], n = drag.n;
        memcpy(st, drag.chain, sizeof(int) * (size_t)n);
        if (drag.closes) st[n++] = st[0];
        keep_undo();
        if (mtl_new_line(&S, st, n)) { ui_sound(SND_PLACE); hint_act[0] = 0; }
        else { nundo--; deny(); }
        return;
    }
    if (kind == D_EXTEND) {
        if (!drag.cuts && drag.n < 2 && !drag.closes) return;
        keep_undo();
        int ok = 1;
        for (int c = 0; c < drag.cuts && ok; c++) ok = mtl_cut(&S, drag.line, drag.tail);
        for (int i = 1; i < drag.n && ok && S.l[drag.line].n; i++) ok = mtl_extend(&S, drag.line, drag.tail, drag.chain[i]);
        if (ok && drag.closes && S.l[drag.line].n >= 3)
            ok = mtl_extend(&S, drag.line, drag.tail, drag.tail ? S.l[drag.line].stop[0] : S.l[drag.line].stop[S.l[drag.line].n - 1]);
        if (ok) { ui_sound(drag.cuts && drag.n < 2 ? SND_MOVE : SND_PLACE); hint_act[0] = 0; }
        else deny();
        return;
    }
    if (kind == D_INSERT) {
        int st = station_at(drag.fx, drag.fy, 0.5f);
        if (st < 0) return;
        keep_undo();
        if (mtl_insert(&S, drag.line, drag.link, st)) { ui_sound(SND_PLACE); hint_act[0] = 0; }
        else { nundo--; deny(); }
    }
}

/* ---- the palette under the map: the lines' dots, the tunnels left ---------------------------------------------- */
static int pal_n(void) { return S.L.lines_max < MTL_MAXL ? S.L.lines_max : MTL_MAXL; }

static void pal_dot(int i, float *cx, float *cy, float *d)
{
    int n = pal_n();
    float sp = fminf(strip_h * 1.15f, (pw * 0.62f) / (n + 2.5f));
    *d = fminf(0.7f * strip_h, 0.82f * sp);
    float total = (n - 1) * sp;
    *cx = bx0 + fr + pw * 0.4f - total / 2 + i * sp;
    *cy = strip_y + strip_h / 2;
}

static void pal_tunnels(float *cx, float *cy, float *d)
{
    *d = 0.7f * strip_h;
    *cx = bx0 + fr + pw * 0.83f;
    *cy = strip_y + strip_h / 2;
}

/* ---- the hint ---------------------------------------------------------------------------------------------------- */
static void hint(void)
{
    if (ended()) { deny(); return; }
    char a[MTL_ACT * 2];
    mtl_plan(&S, 0, 20 * MTL_HZ, a);
    hints++;
    snprintf(hint_act, sizeof hint_act, "%s", a[0] ? a : "-");
    hint_t = ui_ms() ? ui_ms() : 1;
    ui_sound(SND_TAP);
    ui_animate();
}

/* the hint's first action: its kind and what it joins (stations a -> b, or a line's piece), for drawing */
static int hint_parse(int *kind, int *a, int *b, int *line, int *lk)
{
    const char *p = hint_act;
    *kind = p[0];
    *a = *b = *line = *lk = -1;
    if (!p[0] || p[0] == '-') return 0;
    if (p[0] == 'n') {
        *a = atoi(p + 1) - 1;
        const char *q = strchr(p, '-');
        *b = q ? atoi(q + 1) - 1 : -1;
    } else if (p[0] == 'e') {
        *line = atoi(p + 1) - 1;
        const char *q = strchr(p, '-');
        int tail = strchr(p, 't') && strchr(p, 't') < q;
        if (*line < 0 || *line >= MTL_MAXL || !S.l[*line].n) return 0;
        *a = tail ? S.l[*line].stop[S.l[*line].n - 1] : S.l[*line].stop[0];
        *b = q ? atoi(q + 1) - 1 : -1;
    } else if (p[0] == 'i') {
        *line = atoi(p + 1) - 1;
        const char *d = strchr(p, '.'), *q = strchr(p, '-');
        *lk = d ? atoi(d + 1) - 1 : -1;
        *b = q ? atoi(q + 1) - 1 : -1;
    } else if (p[0] == 'g') {
        *a = atoi(p + 1) - 1;
        const char *q = strchr(p, 'p');
        if (q) *b = atoi(q + 1) - 1;
    } else if (p[0] == 'p') *b = atoi(p + 1) - 1;
    return 1;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static pzk_rgba rgba3(const uint8_t c[3], int a) { return (pzk_rgba){c[0], c[1], c[2], (Uint8)a}; }

static void poly_line(const float *x, const float *y, int n, float wd, pzk_rgba c)   /* units, round joins */
{
    float sx0, sy0, sx1, sy1;
    for (int i = 0; i + 1 < n; i++) {
        w2s(x[i], y[i], &sx0, &sy0);
        w2s(x[i + 1], y[i + 1], &sx1, &sy1);
        pzk_line(sx0, sy0, sx1, sy1, wd, 0, c, 0);
        if (i) pzk_dot(sx0, sy0, wd, 0, c, 0);
    }
}

/* an octilinear way from (ax, ay) to (bx, by) (units): diagonal first, then straight */
static int octi(float ax, float ay, float bx, float by, float *x, float *y)
{
    float dx = bx - ax, dy = by - ay, m = fminf(fabsf(dx), fabsf(dy));
    x[0] = ax, y[0] = ay;
    x[1] = ax + m * (dx < 0 ? -1 : 1), y[1] = ay + m * (dy < 0 ? -1 : 1);
    x[2] = bx, y[2] = by;
    return 3;
}

static void draw_route(int a, int b, float wd, pzk_rgba c)   /* a link as the rules would lay it */
{
    mtl_pt p[3];
    int cc, n = mtl_route(&S, a, b, mtl_best_bend(&S, a, b, &cc), p);
    float x[3], y[3];
    for (int i = 0; i < n; i++) { x[i] = p[i].x / (float)MTL_FP; y[i] = p[i].y / (float)MTL_FP; }
    poly_line(x, y, n, wd, c);
}

static void draw_lines(void)
{
    float wd = LW * k;
    for (int l = 0; l < MTL_MAXL; l++) {       /* bridges first: a pale deck with ink rails, under the line */
        const mtl_line *L = &S.l[l];
        int nk = L->n < 2 ? 0 : L->loop ? L->n : L->n - 1;
        for (int q = 0; q < nk; q++)
            for (int w = 0; w < wetv[l][q].n; w++) {
                float a = fmaxf(0, wetv[l][q].a[w] * vp[l][q].len - 0.18f);
                float b = fminf(vp[l][q].len, wetv[l][q].b[w] * vp[l][q].len + 0.18f);
                float sx[VP + 2], sy[VP + 2], ang, x, y, d = 0;   /* the way from a to b, its own points between */
                int m = 0;
                vp_point(l, q, a, &x, &y, &ang);
                w2s(x, y, &sx[m], &sy[m]);
                m++;
                for (int i = 1; i < vp[l][q].n; i++) {
                    d += hypotf(vp[l][q].x[i] - vp[l][q].x[i - 1], vp[l][q].y[i] - vp[l][q].y[i - 1]);
                    if (d > a + 0.01f && d < b - 0.01f) { w2s(vp[l][q].x[i], vp[l][q].y[i], &sx[m], &sy[m]); m++; }
                }
                vp_point(l, q, b, &x, &y, &ang);
                w2s(x, y, &sx[m], &sy[m]);
                m++;
                for (int side = -1; side <= 1; side++)  /* its deck: three strips side by side (narrow strips turn */
                    for (int i = 0; i + 1 < m; i++) {    /* true on any renderer), then a rail each side */
                        float dx = sx[i + 1] - sx[i], dy = sy[i + 1] - sy[i], dl = hypotf(dx, dy);
                        if (dl < 0.5f) continue;
                        float nx = -dy / dl * 0.62f * wd * side, ny = dx / dl * 0.62f * wd * side;
                        pzk_line(sx[i] + nx, sy[i] + ny, sx[i + 1] + nx, sy[i + 1] + ny, 0.9f * wd, 0, rgba3(PAPER, 255), 0);
                    }
                for (int side = -1; side <= 1; side += 2)
                    for (int i = 0; i + 1 < m; i++) {
                        float dx = sx[i + 1] - sx[i], dy = sy[i + 1] - sy[i], dl = hypotf(dx, dy);
                        if (dl < 0.5f) continue;
                        float nx = -dy / dl * 1.05f * wd * side, ny = dx / dl * 1.05f * wd * side;
                        pzk_line(sx[i] + nx, sy[i] + ny, sx[i + 1] + nx, sy[i + 1] + ny, fmaxf(1.5f, 0.13f * wd), 0,
                                 (pzk_rgba){112, 118, 128, 255}, 0);
                    }
            }
    }
    for (int l = 0; l < MTL_MAXL; l++) {
        const mtl_line *L = &S.l[l];
        int nk = L->n < 2 ? 0 : L->loop ? L->n : L->n - 1;
        int fade = (drag.kind == D_INSERT && drag.line == l) || armed == l;
        pzk_rgba c = rgba3(LINE_RGB[l], fade ? 110 : 255);
        for (int q = 0; q < nk; q++) poly_line(vp[l][q].x, vp[l][q].y, vp[l][q].n, wd, c);
        if (!nk || L->loop) continue;
        for (int e = 0; e < 2; e++) {           /* the ends: a short stub and a bar across (the T) */
            if (drag.kind == D_EXTEND && drag.line == l && drag.tail == e) continue;
            float hx, hy, ang, sx0, sy0, sx1, sy1;
            end_handle(l, e, &hx, &hy, &ang);
            int q = e ? L->n - 2 : 0, n = vp[l][q].n;
            float ex = e ? vp[l][q].x[n - 1] : vp[l][q].x[0], ey = e ? vp[l][q].y[n - 1] : vp[l][q].y[0];
            w2s(ex, ey, &sx0, &sy0);
            w2s(hx, hy, &sx1, &sy1);
            pzk_line(sx0, sy0, sx1, sy1, wd, 0, c, 0);
            float dx = sx1 - sx0, dy = sy1 - sy0, dl = hypotf(dx, dy), bar = 0.24f * k;
            if (dl > 0) pzk_line(sx1 - dy / dl * bar, sy1 + dx / dl * bar, sx1 + dy / dl * bar, sy1 - dx / dl * bar, wd, 0, c, 0);
        }
    }
}

static void draw_drag(Uint32 now)
{
    if (drag.kind == D_NONE) return;
    float wd = LW * k * 1.15f;
    pzk_rgba c = rgba3(LINE_RGB[drag.line], 200);
    if (drag.kind == D_INSERT) {
        const mtl_line *L = &S.l[drag.line];
        int a = L->stop[drag.link], b = L->stop[(drag.link + 1) % L->n], st = station_at(drag.fx, drag.fy, 0.5f);
        if (st >= 0 && mtl_on_line(&S, drag.line, st) < 0) {
            draw_route(a, st, wd, c);
            draw_route(st, b, wd, c);
        } else {
            float x[3], y[3];
            octi(S.L.st[a].x, S.L.st[a].y, drag.fx, drag.fy, x, y);
            poly_line(x, y, 3, wd, c);
            octi(S.L.st[b].x, S.L.st[b].y, drag.fx, drag.fy, x, y);
            poly_line(x, y, 3, wd, c);
        }
        return;
    }
    const mtl_line *L = &S.l[drag.line];
    if (drag.kind == D_EXTEND && drag.cuts) {   /* the stops taken off: their pieces fading */
        for (int c2 = 0; c2 < drag.cuts; c2++) {
            int q = drag.tail ? L->n - 2 - c2 : c2;
            if (q >= 0 && q < L->n - 1) {
                float pulse = 0.5f + 0.5f * sinf(now / 120.0f);
                poly_line(vp[drag.line][q].x, vp[drag.line][q].y, vp[drag.line][q].n, LW * k * 1.3f,
                          rgba3(PAPER, (int)(170 + 60 * pulse)));
            }
        }
    }
    for (int i = 0; i + 1 < drag.n; i++) draw_route(drag.chain[i], drag.chain[i + 1], wd, c);
    int last = drag.chain[drag.n - 1];
    if (drag.closes) {
        int first = drag.kind == D_NEW ? drag.chain[0] : drag.tail ? L->stop[0] : L->stop[L->n - 1];
        draw_route(last, first, wd, c);
        return;
    }
    float x[3], y[3];
    octi(S.L.st[last].x, S.L.st[last].y, drag.fx, drag.fy, x, y);
    poly_line(x, y, 3, wd, c);
    float sx, sy;
    w2s(drag.fx, drag.fy, &sx, &sy);
    pzk_dot(sx, sy, 0.42f * k, 0, c, 0);
}

static int train_xy(int l, const mtl_train *t, float *x, float *y, float *ang)   /* where its head is (units) */
{
    const mtl_line *L = &S.l[l];
    int nk = L->n < 2 ? 0 : L->loop ? L->n : L->n - 1;
    if (!nk) return 0;
    int q = t->k < nk ? t->k : 0;
    float len = vp[l][q].len, d;
    if (t->stopped) {
        int at_start = t->at == q;              /* the stop at the link's start (else its end) */
        d = at_start ? 0 : len;
        vp_point(l, q, d, x, y, ang);
        if (!at_start && t->dir < 0) *ang += PUZ_PI;
        if (at_start && t->dir < 0) *ang += PUZ_PI;
        return 1;
    }
    float u = L->len[q] > 0 ? (float)t->pos / L->len[q] : 0;
    d = t->dir > 0 ? u * len : (1 - u) * len;
    vp_point(l, q, d, x, y, ang);
    if (t->dir < 0) *ang += PUZ_PI;
    return 1;
}

static void draw_trains(int64_t gms)
{
    float f = clampf((float)(gms - tick_ms(S.tick)) / MTL_TICK_MS, 0, 1);
    for (int l = 0; l < MTL_MAXL; l++) {
        if (S.l[l].n < 2) continue;
        mtl_train t = S.t[l];
        const mtl_train *p = &prev_t[l];
        if (prev_tick == S.tick - 1 && !p->stopped && !t.stopped && p->k == t.k && p->dir == t.dir && p->pos <= t.pos)
            t.pos = p->pos + (int32_t)((t.pos - p->pos) * f);   /* (a frame between two ticks) */
        float x, y, ang;
        if (!train_xy(l, &t, &x, &y, &ang)) continue;
        float sx, sy, sx2, sy2;
        w2s(x, y, &sx, &sy);
        w2s(x + cosf(ang), y + sinf(ang), &sx2, &sy2);
        float sa = atan2f(sy2 - sy, sx2 - sx), deg = sa * 180 / PUZ_PI;
        float cl = 0.68f * k;
        for (int c = 0; c < t.cars; c++) {      /* the cars, one behind the other along its heading */
            float cx = sx - cosf(sa) * cl * c, cy = sy - sinf(sa) * cl * c;
            ui_shadow_flat(cx + 0.03f * k, cy + 0.05f * k, 0.66f * k, 0.4f * k, 0.35f);
            SDL_SetTextureColorMod(t_train, LINE_RGB[l][0], LINE_RGB[l][1], LINE_RGB[l][2]);
            ui_put_rot(t_train, cx, cy, (float)tr_w, (float)tr_h, deg);
            SDL_SetTextureColorMod(t_train, 255, 255, 255);
            for (int i = 0; i < MTL_CAR; i++) { /* its passengers: two rows of three */
                int r = c * MTL_CAR + i;
                if (r >= t.nride) break;
                float ax = (i % 3 - 1) * 0.17f * k, ay = (i / 3 - 0.5f) * 0.15f * k;
                float rx = cx + cosf(sa) * ax - sinf(sa) * ay, ry = cy + sinf(sa) * ax + cosf(sa) * ay;
                ui_put_c(t_ride[t.ride[r]], rx, ry, 0.17f * k, 0.17f * k);
            }
        }
    }
}

static float ease_back(float t)                 /* a pop: past 1, back */
{
    t = clampf(t, 0, 1);
    float c = 1.9f;
    return 1 + (c + 1) * powf(t - 1, 3) + c * powf(t - 1, 2);
}

static void draw_stations(int64_t gms, Uint32 now)
{
    for (int i = 0; i < S.nst; i++) {
        float sx, sy;
        st_xy(i, &sx, &sy);
        const mtl_station *st = &S.s[i];
        float age = (float)(gms - tick_ms(S.L.st[i].tick)), pop = S.L.st[i].tick > 0 ? ease_back(age / 450.0f) : 1;
        if (pop <= 0.01f) continue;
        if (age < 900 && S.L.st[i].tick > 0) {  /* a new station: a soft ring spreading */
            float q = age / 900.0f;
            pzk_dot(sx, sy, (0.6f + 1.6f * q) * k, 0.4f * k, rgba3(INK, (int)(60 * (1 - q))), 0);
        }
        if (st->crowd > 0) {                    /* its timer: a ring growing round it */
            float f = (float)st->crowd / MTL_CROWD, r = 0.5f * k, th = 0.11f * k;
            float pulse = 0.5f + 0.5f * sinf(now / (f > 0.6f ? 140.0f : 260.0f));
            uint8_t c[3];
            for (int j = 0; j < 3; j++) c[j] = (uint8_t)(WARN[j] + (RED[j] - WARN[j]) * clampf(f * 1.4f - 0.2f, 0, 1));
            pzk_dot(sx, sy, 2 * r + th + 0.25f * k * pulse, 0.3f * k, rgba3(c, (int)(40 + 50 * f)), 0);
            int segs = (int)ceilf(48 * f);
            float px = sx, py = sy - r;
            for (int s = 1; s <= segs; s++) {
                float a = -PUZ_PI / 2 + 2 * PUZ_PI * fminf(f, s / 48.0f), nx = sx + cosf(a) * r, ny = sy + sinf(a) * r;
                pzk_line(px, py, nx, ny, th, 0, rgba3(c, 235), 0);
                pzk_dot(nx, ny, th, 0, rgba3(c, 235), 0);
                px = nx, py = ny;
            }
            ui_animate();
        }
        if (st->inter) ui_put_c(t_ring, sx, sy, ring_px * pop, ring_px * pop);
        ui_put_c(t_st[S.L.st[i].shape], sx, sy, st_px * pop, st_px * pop);
        /* its passengers: tiny ink shapes in rows beside it (on its left near the right edge) */
        int right = sx < px0 + pw * 0.72f;
        for (int w = 0; w < st->nwait; w++) {
            float ox = (0.5f + (w % 6) * 0.22f) * k, oy = (-0.13f + (w / 6) * 0.25f) * k;
            float qx = right ? sx + ox : sx - ox, qy = sy + oy;
            ui_put_c(t_pass[st->wait[w]], qx, qy, (float)pass_px * pop, (float)pass_px * pop);
        }
    }
}

static void draw_fx(int64_t gms)
{
    int keep = 0;
    for (int i = 0; i < nfx; i++) {
        float age = (float)(gms - fx[i].t);
        if (age > 700 || age < 0) continue;
        fx[keep++] = fx[i];
        float sx, sy, q = age / 700.0f;
        st_xy(fx[i].st, &sx, &sy);
        ui_mark_gold_a(sx, sy, (0.7f + 1.0f * q) * k, 0.8f * (1 - q));
    }
    nfx = keep;
    if (nfx) ui_animate();
}

static void chime(int shape)
{
    Uint32 now = ui_ms();
    if (ui_replaying() || (chime_t && now - chime_t < 70)) return;
    chime_t = now ? now : 1;
    const int16_t *smp;
    if (pk_sound_on(NOTE[shape % MTL_SHAPES], 0, 0.34f, 300, &smp) < 0) ui_sound_soft(SND_TAP, 0.35f);
}

static void take_events(void)
{
    int from = seen_ev > S.nev - MTL_EVENTS ? seen_ev : S.nev - MTL_EVENTS;
    for (int i = from; i < S.nev; i++) {
        const mtl_event *e = &S.ev[i % MTL_EVENTS];
        if (e->kind == MTL_EV_DELIVER) {
            if (nfx < FX) { fx[nfx].kind = 0; fx[nfx].st = e->a; fx[nfx].t = tick_ms(e->tick); nfx++; }
            chime(e->b);
        } else if (e->kind == MTL_EV_APPEAR && e->tick > 0) {
            if (!ui_replaying()) ui_sound_soft(SND_PLACE, 0.45f);
        } else if (e->kind == MTL_EV_WEEK) {
            week_ui = ui_ms() ? ui_ms() : 1;
            const int16_t *smp;
            if (!ui_replaying() && pk_sound_on(72, 0, 0.3f, 600, &smp) >= 0) pk_sound_on(79, 0, 0.26f, 600, &smp);
        }
    }
    seen_ev = S.nev;
    for (int i = 0; i < S.nst; i++) {           /* a station starting to crowd: one low note */
        if (S.s[i].crowd > 0 && !warned[i]) {
            warned[i] = 1;
            const int16_t *smp;
            if (!ui_replaying() && pk_sound_on(52, 1, 0.5f, -1, &smp) < 0) ui_sound_soft(SND_DENY, 0.4f);
        }
        if (!S.s[i].crowd) warned[i] = 0;
    }
}

/* the week's clock, at the paper's top right: a pie of the week gone, its hand */
static void draw_week(void)
{
    float r = 0.42f * k, cx = px0 + pw - 0.75f * k, cy = py0 + 0.75f * k;
    float f = S.L.week > 0 ? clampf((float)(S.tick % S.L.week) / S.L.week, 0, 1) : 0;
    pzk_dot(cx, cy, 2 * r + 0.1f * k, 0, rgba3(INK, 255), 0);
    pzk_dot(cx, cy, 2 * r - 0.04f * k, 0, rgba3(PAPER, 255), 0);
    int segs = (int)(f * 36);
    for (int s = 0; s < segs; s++) {
        float a0 = -PUZ_PI / 2 + 2 * PUZ_PI * s / 36.0f, a1 = -PUZ_PI / 2 + 2 * PUZ_PI * (s + 1) / 36.0f;
        float mx = cx + cosf((a0 + a1) / 2) * r * 0.45f, my = cy + sinf((a0 + a1) / 2) * r * 0.45f;
        pzk_line(cx, cy, cx + cosf(a1) * r * 0.86f, cy + sinf(a1) * r * 0.86f, 0.05f * k, 0, rgba3(INK, 40), 0);
        (void)mx, (void)my;
    }
    float a = -PUZ_PI / 2 + 2 * PUZ_PI * f;
    pzk_line(cx, cy, cx + cosf(a) * r * 0.8f, cy + sinf(a) * r * 0.8f, 0.07f * k, 0, rgba3(INK, 255), 0);
    pzk_dot(cx, cy, 0.12f * k, 0, rgba3(INK, 255), 0);
}

/* the gifts' pictures (kind: gift * 8 + the new line's colour) */
static void paint_gift(int kind, int size, uint8_t *o)
{
    int g = kind / 8, col = kind % 8;
    float s = (float)size;
    if (g == MTL_UP_LINE) {
        const uint8_t *c = LINE_RGB[col % MTL_MAXL];
        pzk_stroke(o, size, size, 0.22f * s, 0.68f * s, 0.42f * s, 0.48f * s, 0.07f * s, c, 1);
        pzk_stroke(o, size, size, 0.42f * s, 0.48f * s, 0.78f * s, 0.48f * s, 0.07f * s, c, 1);
        pzk_ball(o, size, size, 0.22f * s, 0.68f * s, 0.11f * s, INK, 1, 0);
        pzk_ball(o, size, size, 0.22f * s, 0.68f * s, 0.075f * s, WHITE, 1, 0);
        pzk_ball(o, size, size, 0.78f * s, 0.48f * s, 0.11f * s, INK, 1, 0);
        pzk_ball(o, size, size, 0.78f * s, 0.48f * s, 0.075f * s, WHITE, 1, 0);
    } else if (g == MTL_UP_CAR) {
        for (int c = 0; c < 2; c++) {
            float x0 = (0.14f + c * 0.37f) * s, y0 = 0.38f * s, w = 0.33f * s, h = 0.24f * s, rr = 0.05f * s;
            const uint8_t *colr = c ? LINE_RGB[1] : LINE_RGB[1];
            for (int y = (int)y0 - 1; y < (int)(y0 + h) + 2; y++)
                for (int x = (int)x0 - 1; x < (int)(x0 + w) + 2; x++) {
                    float dx = fmaxf(0, fabsf(x + 0.5f - (x0 + w / 2)) - (w / 2 - rr));
                    float dy = fmaxf(0, fabsf(y + 0.5f - (y0 + h / 2)) - (h / 2 - rr));
                    float cv = clampf(rr - hypotf(dx, dy) + 0.5f, 0, 1);
                    if (x >= 0 && y >= 0 && x < size && y < size) pzk_over(o + 4 * (y * size + x), colr, cv * (c ? 1 : 0.55f));
                }
        }
        pzk_stroke(o, size, size, 0.47f * s, 0.5f * s, 0.51f * s, 0.5f * s, 0.02f * s, INK, 1);
        for (int i = 0; i < 3; i++) pzk_ball(o, size, size, (0.58f + i * 0.09f) * s, 0.5f * s, 0.028f * s, WHITE, 1, 0);
    } else if (g == MTL_UP_TUNNEL) {
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                float dy = fabsf(y + 0.5f - 0.6f * s - 0.06f * s * sinf(x * 9.0f / s));
                pzk_over(o + 4 * (y * size + x), WATER, clampf(0.13f * s - dy + 0.5f, 0, 1) *
                         clampf(0.4f * s - fabsf(x + 0.5f - 0.5f * s) + 0.5f, 0, 1));
            }
        pzk_stroke(o, size, size, 0.16f * s, 0.6f * s, 0.84f * s, 0.6f * s, 0.075f * s, LINE_RGB[0], 1);
        pzk_stroke(o, size, size, 0.28f * s, 0.49f * s, 0.72f * s, 0.49f * s, 0.016f * s, INK, 1);
        pzk_stroke(o, size, size, 0.28f * s, 0.71f * s, 0.72f * s, 0.71f * s, 0.016f * s, INK, 1);
    } else {
        pzk_ball(o, size, size, 0.5f * s, 0.5f * s, 0.27f * s, INK, 1, 0);
        pzk_ball(o, size, size, 0.5f * s, 0.5f * s, 0.21f * s, WHITE, 1, 0);
        pzk_ball(o, size, size, 0.5f * s, 0.5f * s, 0.15f * s, INK, 1, 0);
        pzk_ball(o, size, size, 0.5f * s, 0.5f * s, 0.105f * s, WHITE, 1, 0);
    }
}

static void gift_spot(int i, float *cx, float *cy, float *d)
{
    float mw = (float)pw, mh = (float)ph;
    *d = fminf(0.27f * mw, fminf(0.22f * mh, 2.5f * ui.unit));
    *cx = px0 + mw / 2 + (i ? 1 : -1) * 0.58f * *d * 1.05f;
    *cy = py0 + mh / 2;
}

static void draw_gifts(Uint32 now)
{
    ui_fill(px0, py0, (float)pw, (float)ph, 40, 36, 34, 110);
    float q = week_ui ? clampf((float)(now - week_ui) / 380.0f, 0, 1) : 1;
    if (q < 1) ui_animate();
    for (int i = 0; i < 2; i++) {
        float cx, cy, d;
        gift_spot(i, &cx, &cy, &d);
        float pop = ease_back(clampf(q * 1.25f - 0.25f * i, 0, 1));
        int hint_me = hint_act[0] == 'g' && atoi(hint_act + 1) - 1 == i;
        if (hint_me) ui_mark_gold_a(cx, cy, d * 1.4f, 0.6f + 0.25f * sinf(now / 160.0f));
        if (pop > 0.02f) ui_button_paint(cx, cy, d * pop, paint_gift, S.opt[i] * 8 + mtl_lines_used(&S) % 8, 0.8f,
                                         UI_IVORY | (hint_me ? UI_LIT : 0));
    }
    if (hint_act[0] == 'g') ui_animate();
}

static void draw_placing(Uint32 now)
{
    float a = 0.45f + 0.3f * sinf(now / 170.0f);
    ui_animate();
    if (S.placing == MTL_UP_INTER) {
        for (int i = 0; i < S.nst; i++) {
            if (S.s[i].inter) continue;
            float sx, sy;
            st_xy(i, &sx, &sy);
            ui_mark_gold_a(sx, sy, 1.2f * k, a);
        }
    } else
        for (int l = 0; l < MTL_MAXL; l++) {
            if (S.l[l].n < 2 || S.t[l].cars >= MTL_MAXCARS) continue;
            float x, y, ang, sx, sy;
            if (!train_xy(l, &S.t[l], &x, &y, &ang)) continue;
            w2s(x, y, &sx, &sy);
            ui_mark_gold_a(sx, sy, 1.4f * k, a);
        }
    float d = 1.25f * ui.unit;                  /* what is to be put somewhere, at the top of the map */
    ui_button_paint(px0 + pw / 2.0f, py0 + 0.9f * d, d, paint_gift, S.placing * 8, 0.8f, UI_IVORY | UI_LIT);
}

static void paint_pause(int kind, int size, uint8_t *o)
{
    float s = (float)size;
    if (kind == 0) {                            /* pause: two bars */
        pzk_stroke(o, size, size, 0.37f * s, 0.3f * s, 0.37f * s, 0.7f * s, 0.07f * s, WHITE, 1);
        pzk_stroke(o, size, size, 0.63f * s, 0.3f * s, 0.63f * s, 0.7f * s, 0.07f * s, WHITE, 1);
    } else {                                    /* faster: two chevrons */
        for (int c = 0; c < 2; c++) {
            float x = (0.3f + 0.22f * c) * s;
            pzk_stroke(o, size, size, x, 0.3f * s, x + 0.17f * s, 0.5f * s, 0.055f * s, WHITE, 1);
            pzk_stroke(o, size, size, x + 0.17f * s, 0.5f * s, x, 0.7f * s, 0.055f * s, WHITE, 1);
        }
    }
}

static void draw_palette(Uint32 now)
{
    int n = pal_n();
    for (int i = 0; i < n; i++) {
        float cx, cy, d;
        pal_dot(i, &cx, &cy, &d);
        int used = S.l[i].n > 0, owned = i < S.lines || used;
        const uint8_t *c = LINE_RGB[i];
        if (!owned) {
            pzk_dot(cx, cy, d * 0.62f, 0, rgba3(ui_table_ink(), 70), 0);
            continue;
        }
        if (armed == i) {
            float a = 0.5f + 0.3f * sinf(now / 120.0f);
            ui_mark_glow(cx, cy, d * 1.5f);
            (void)a;
        }
        ui_shadow_flat(cx + 0.04f * d, cy + 0.07f * d, d, d, 0.4f);
        pzk_dot(cx, cy, d, 0, rgba3(INK, 255), 0);
        pzk_dot(cx, cy, d * 0.84f, 0, rgba3(c, 255), 0);
        if (!used) pzk_dot(cx, cy, d * 0.44f, 0, rgba3(WHITE, 235), 0);   /* free: a white heart, ready */
        else if (armed == i) {                  /* armed: a cross, a second tap takes the line away */
            float r = 0.2f * d;
            pzk_line(cx - r, cy - r, cx + r, cy + r, 0.1f * d, 0, rgba3(WHITE, 255), 0);
            pzk_line(cx - r, cy + r, cx + r, cy - r, 0.1f * d, 0, rgba3(WHITE, 255), 0);
        }
    }
    if (S.L.tunnel_gifts || S.tunnels) {        /* the tunnels left: a small bridge and the count */
        float cx, cy, d;
        pal_tunnels(&cx, &cy, &d);
        static SDL_Texture *t_tun;
        static int tun_px;
        int px = (int)d;
        if (tun_px != px) {
            tun_px = px;
            ui_free(&t_tun);
            uint8_t *o = ui_buf(px, px);
            memset(o, 0, (size_t)px * px * 4);
            paint_gift(MTL_UP_TUNNEL * 8, px, o);
            t_tun = ui_tex(px, px, o);
        }
        pzk_dot(cx - 0.55f * d, cy, d * 1.05f, 0, rgba3(PAPER, 255), 0);
        ui_put_c(t_tun, cx - 0.55f * d, cy, d, d);
        int left = tunnels_left();
        int glow = deny_t && ui_ms() - deny_t < 700;
        if (glow) { ui_mark_glow(cx - 0.55f * d, cy, 1.4f * d); ui_animate(); }
        ui_number_table(cx + 0.45f * d, cy, 0.62f * d, left);
    }
}

static void draw_hint(Uint32 now)
{
    if (!hint_act[0] || now - hint_t > 6000) { hint_act[0] = 0; return; }
    ui_animate();
    float a = 0.55f + 0.35f * sinf(now / 150.0f);
    int kind, sa, sb, line, lk;
    if (!hint_parse(&kind, &sa, &sb, &line, &lk)) return;
    pzk_rgba gold = {236, 194, 96, (Uint8)(255 * a)};
    if ((kind == 'n' || kind == 'e') && sa >= 0 && sb >= 0 && sa < S.nst && sb < S.nst) {
        float sx, sy;
        draw_route(sa, sb, LW * k * 1.6f, gold);
        st_xy(sa, &sx, &sy);
        ui_mark_gold_a(sx, sy, 1.3f * k, a);
        st_xy(sb, &sx, &sy);
        ui_mark_gold_a(sx, sy, 1.3f * k, a);
    } else if (kind == 'i' && line >= 0 && line < MTL_MAXL && lk >= 0 && S.l[line].n > lk && sb >= 0 && sb < S.nst) {
        int x = S.l[line].stop[lk], y = S.l[line].stop[(lk + 1) % S.l[line].n];
        draw_route(x, sb, LW * k * 1.6f, gold);
        draw_route(sb, y, LW * k * 1.6f, gold);
        float sx, sy;
        st_xy(sb, &sx, &sy);
        ui_mark_gold_a(sx, sy, 1.3f * k, a);
    } else if (kind == 'p' && S.placing && sb >= 0) {
        float sx, sy, x, y, ang;
        if (S.placing == MTL_UP_INTER && sb < S.nst) st_xy(sb, &sx, &sy);
        else if (S.placing == MTL_UP_CAR && sb < MTL_MAXL && train_xy(sb, &S.t[sb], &x, &y, &ang)) w2s(x, y, &sx, &sy);
        else return;
        ui_mark_gold_a(sx, sy, 2.0f * k, a);
    }
}

static void draw(void)
{
    if (made_serial != serial || !t_paper) layout(room);
    Uint32 now = ui_ms();
    clock_frame(now);
    build_paths();
    take_events();
    if (!ended()) ui_animate();
    else if (!end_ui) {
        end_ui = now ? now : 1;
        keep_best();
    }
    int64_t gms = clk.gms;
    ui_put(t_paper, bx0, by0, (float)(pw + 2 * fr), (float)(ph + 2 * fr));
    draw_lines();
    draw_hint(now);
    draw_drag(now);
    draw_trains(gms);
    draw_stations(gms, now);
    draw_fx(gms);
    draw_week();
    if (S.over && S.over_st >= 0) {             /* the end: the crowded station glowing red */
        float sx, sy;
        st_xy(S.over_st, &sx, &sy);
        ui_mark_glow(sx, sy, (1.6f + 0.2f * sinf(now / 110.0f)) * k);
        if (busy()) ui_animate();
    }
    if (S.won && busy()) {                      /* the last goal: every station in gold, one after another */
        float t = (float)(now - end_ui);
        for (int i = 0; i < S.nst; i++) {
            float q = puz_pulse(t, i * 35.0f, 600);
            float sx, sy;
            st_xy(i, &sx, &sy);
            if (q > 0) ui_mark_gold_a(sx, sy, (1.0f + 1.2f * q) * k, q);
        }
        ui_animate();
    }
    if (paused && !ended()) {                   /* paused: the paper a little greyed */
        ui_fill(px0, py0, (float)pw, (float)ph, 120, 116, 110, 40);
    }
    if (S.choosing) draw_gifts(now);
    else if (S.placing) draw_placing(now);
    draw_palette(now);
    if (armed >= 0 && now - armed_t > 2500) armed = -1;
    if (armed >= 0) ui_animate();
    int live = !ended();
    ui_tool_paint(T_PAUSE, paused ? NULL : paint_pause, 0, 1, live ? (paused ? UI_ON : UI_TINT) : UI_DIM | UI_TINT);
    if (paused) {
        float cx, cy, d;
        if (ui_tool_at(T_PAUSE, &cx, &cy, &d)) ui_button(cx, cy, d, ICON_PLAY, UI_ON);
    }
    ui_tool_paint(T_FAST, paint_pause, 1, 1, live ? (fast ? UI_ON | UI_TINT : UI_TINT) : UI_DIM | UI_TINT);
    if (hint_act[0] == '-' && now - hint_t < 1500) { ui_tool(T_HINT, ICON_TICK, UI_ON); ui_animate(); }   /* nothing to do */
    else ui_tool(T_HINT, ICON_HINT, live ? (tool_down == T_HINT ? UI_ON : 0) : UI_DIM);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    tool_down = ui_tool_hit(x, y);
    if (tool_down >= 0) { ui_animate(); return; }
    if (ended()) return;
    ui_animate();
    if (S.choosing) {
        for (int i = 0; i < 2; i++) {
            float cx, cy, d;
            gift_spot(i, &cx, &cy, &d);
            if (hypotf(x - cx, y - cy) < d * 0.55f) {
                int g = S.opt[i];
                mtl_choose(&S, i);
                if (hint_act[0] == 'g') hint_act[0] = 0;
                ui_sound(g == MTL_UP_CAR || g == MTL_UP_INTER ? SND_TAP : SND_PLACE);
                return;
            }
        }
        return;
    }
    float ux, uy;
    s2w(x, y, &ux, &uy);
    if (S.placing) {
        int target = -1;
        if (S.placing == MTL_UP_INTER) target = station_at(ux, uy, HIT);
        else {
            int l, q;
            float bd = 1.0f;
            for (int i = 0; i < MTL_MAXL; i++) {   /* a train near the finger, or a line under it */
                float tx, ty, ang;
                if (S.l[i].n < 2 || S.t[i].cars >= MTL_MAXCARS || !train_xy(i, &S.t[i], &tx, &ty, &ang)) continue;
                float d = hypotf(tx - ux, ty - uy);
                if (d < bd) { bd = d; target = i; }
            }
            if (target < 0 && link_at(ux, uy, &l, &q) && S.t[l].cars < MTL_MAXCARS) target = l;
        }
        if (target >= 0 && mtl_place(&S, target)) { ui_sound(SND_PLACE); hint_act[0] = 0; }
        else deny();
        return;
    }
    for (int i = 0; i < pal_n(); i++) {         /* the palette: a line's dot */
        float cx, cy, d;
        pal_dot(i, &cx, &cy, &d);
        if (hypotf(x - cx, y - cy) > fmaxf(d * 0.7f, 0.45f * ui.unit)) continue;
        if (!S.l[i].n) { deny(); return; }
        if (armed == i && ui_ms() - armed_t < 2500) {
            keep_undo();
            if (mtl_remove(&S, i)) ui_sound(SND_CAPTURE);
            else nundo--;
            armed = -1;
            return;
        }
        armed = i;
        armed_t = ui_ms();
        ui_sound(SND_TAP);
        return;
    }
    armed = -1;
    if (ux < -MARGIN || uy < -MARGIN || ux > S.L.w - 1 + MARGIN || uy > S.L.h - 1 + MARGIN) return;
    memset(&drag, 0, sizeof drag);
    drag.fx = ux;
    drag.fy = uy;
    /* a line's end (its T) */
    float bd = 0.6f;
    for (int l = 0; l < MTL_MAXL; l++) {
        if (S.l[l].n < 2 || S.l[l].loop) continue;
        for (int e = 0; e < 2; e++) {
            float hx, hy, ang;
            end_handle(l, e, &hx, &hy, &ang);
            float d = hypotf(hx - ux, hy - uy);
            if (d < bd) { bd = d; drag.kind = D_EXTEND; drag.line = l; drag.tail = e; }
        }
    }
    int st = station_at(ux, uy, HIT);
    if (drag.kind == D_EXTEND && st >= 0) {     /* nearer a station than the T: the station */
        float sd = hypotf(S.L.st[st].x - ux, S.L.st[st].y - uy);
        if (sd < bd * 0.8f) drag.kind = D_NONE;
    }
    if (drag.kind == D_EXTEND) {
        const mtl_line *L = &S.l[drag.line];
        drag.chain[0] = drag.tail ? L->stop[L->n - 1] : L->stop[0];
        drag.n = 1;
        ui_sound_soft(SND_TAP, 0.5f);
        return;
    }
    if (st >= 0) {
        int li = mtl_free_line(&S);
        if (li < 0) {                           /* no line left: from a line's end stop, that line lengthened */
            for (int l = 0; l < MTL_MAXL && li < 0; l++) {
                const mtl_line *L = &S.l[l];
                if (L->n < 2 || L->loop) continue;
                if (L->stop[L->n - 1] == st) { li = l; drag.tail = 1; }
                else if (L->stop[0] == st) { li = l; drag.tail = 0; }
            }
            if (li < 0) { deny(); return; }
            drag.kind = D_EXTEND;
        } else
            drag.kind = D_NEW;
        drag.line = li;
        drag.chain[0] = st;
        drag.n = 1;
        ui_sound_soft(SND_TAP, 0.5f);
        return;
    }
    int l, q;
    if (link_at(ux, uy, &l, &q)) {
        drag.kind = D_INSERT;
        drag.line = l;
        drag.link = q;
        ui_sound_soft(SND_TAP, 0.5f);
    }
}

static void motion(float x, float y)
{
    if (drag.kind == D_NONE) return;
    float ux, uy;
    s2w(x, y, &ux, &uy);
    drag_move(ux, uy);
    ui_animate();
}

static void release(float x, float y)
{
    if (tool_down >= 0) {
        int t = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) != t || ended()) return;
        if (t == T_PAUSE) { paused = !paused; ui_sound(SND_TAP); }
        else if (t == T_FAST) { fast = !fast; ui_sound(SND_TAP); }
        else hint();
        return;
    }
    if (drag.kind == D_NONE) return;
    float ux, uy;
    s2w(x, y, &ux, &uy);
    drag_move(ux, uy);
    drag_commit();
    ui_animate();
}

static void key(SDL_Keycode kc)
{
    if (kc == SDLK_SPACE || kc == SDLK_p) { paused = !paused; ui_sound(SND_TAP); }
    else if (kc == SDLK_TAB || kc == SDLK_2) { fast = !fast; ui_sound(SND_TAP); }
    ui_animate();
}

static void bar(int player, box b)
{
    (void)player;
    int goal = 0;
    SDL_Texture *pic = t_flag;
    if (endless()) {
        goal = best[variant] > S.delivered ? best[variant] : S.delivered;
        pic = t_star;
    } else {
        goal = S.L.goal[2];
        for (int g = 0; g < 3; g++)
            if (S.delivered < S.L.goal[g]) { goal = S.L.goal[g]; break; }
    }
    pzt_cream_ink(t_person);
    pzt_cream_ink(pic);
    ui_bar_fit(b, (ui_count[]){{t_person, S.delivered}, {pic, goal}}, 2, 0);
    pzt_cream_end(t_person);
    pzt_cream_end(pic);
}

/* the notation (rules' mtl_script), and "z" pause, "f" faster */
static int script(const char *moves)
{
    int ok = 1;
    char t[MTL_ACT];
    for (const char *q = moves; *q;) {
        while (*q == ' ') q++;
        int m = 0;
        while (q[m] && q[m] != ' ') m++;
        if (!m) break;
        if (m >= MTL_ACT) return 0;
        memcpy(t, q, (size_t)m);
        t[m] = 0;
        q += m;
        if (!strcmp(t, "z")) paused = !paused;
        else if (!strcmp(t, "f")) fast = !fast;
        else if (!strcmp(t, "hint")) hint();
        else if (t[0] == 'W') {                 /* seconds on, the gifts taken as the naive player would */
            int32_t until = S.tick + atoi(t + 1) * MTL_HZ;
            char a[MTL_ACT * 2];
            while (S.tick < until && !ended()) {
                if (!mtl_running(&S)) {
                    if (!mtl_plan(&S, 1, 0, a) || (a[0] != 'g' && a[0] != 'p') || !mtl_script(&S, a)) break;
                    continue;
                }
                mtl_step(&S);
            }
        } else if (!strncmp(t, "plan", 4) && ui.level >= 0 && ui.level < MTL_LEVELS) {   /* the level's plan to tick n */
            int until = atoi(t + 4);
            char a[MTL_ACT];
            for (const char *p = mtl_plans[ui.level]; *p;) {
                while (*p == ' ') p++;
                int k2 = 0;
                while (p[k2] && p[k2] != ' ') k2++;
                if (!k2 || k2 >= MTL_ACT) break;
                memcpy(a, p, (size_t)k2);
                a[k2] = 0;
                p += k2;
                if (a[0] == '@' && atoi(a + 1) > until) break;
                mtl_do(&S, a);
            }
            while (S.tick < until && mtl_running(&S)) mtl_step(&S);
        } else if (!mtl_do(&S, t)) ok = 0;
    }
    clk.gms = tick_ms(S.tick);
    clk.on = 0;
    memcpy(prev_t, S.t, sizeof prev_t);
    prev_tick = S.tick;
    seen_ev = S.nev;
    return ok;
}

/* ---- the shell's pictures: a map with its rivers, stations and lines ---------------------------------------- */
static void paint_map(const mtl_state *s, int size, uint8_t *rgba, int lines)
{
    float ew = (s->L.w - 1) + 2 * MARGIN, eh = (s->L.h - 1) + 2 * MARGIN;
    int f = (int)fmaxf(2, size * 0.03f);
    float kk = fminf((size - 2 * f) / ew, (size - 2 * f) / eh);
    int pw2 = (int)(kk * ew), ph2 = (int)(kk * eh), W = pw2 + 2 * f, H = ph2 + 2 * f;
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    art_plank(pw2, ph2, f, o);
    paint_paper(o, W, f, f, f + pw2, f + ph2);
    paint_water(o, W, H, (float)f, (float)f, kk, 0, s, f, f, f + pw2, f + ph2);
#define PX(ux) (f + ((ux) + MARGIN) * kk)
#define PY(uy) (f + ((uy) + MARGIN) * kk)
    if (lines)
        for (int l = 0; l < MTL_MAXL; l++) {
            const mtl_line *L = &s->l[l];
            int nk = L->n < 2 ? 0 : L->loop ? L->n : L->n - 1;
            for (int q = 0; q < nk; q++) {
                mtl_pt p[3];
                int n = mtl_route(s, L->stop[q], L->stop[(q + 1) % L->n], L->bend[q], p);
                for (int i = 0; i + 1 < n; i++)
                    pzk_stroke(o, W, H, PX(p[i].x / (float)MTL_FP), PY(p[i].y / (float)MTL_FP),
                               PX(p[i + 1].x / (float)MTL_FP), PY(p[i + 1].y / (float)MTL_FP), LW * kk * 0.55f,
                               LINE_RGB[l], 1);
            }
        }
    int ss = (int)ceilf(2 * RS * 1.5f * kk);
    if (ss >= 4) {
        uint8_t *q = malloc((size_t)ss * ss * 4);
        for (int i = 0; i < s->nst && q; i++) {
            memset(q, 0, (size_t)ss * ss * 4);
            paint_shape(q, ss, s->L.st[i].shape, WHITE, INK, RS * kk / ss, fmaxf(0.085f * kk, 1.1f) / ss);
            art_blend(o, W, H, q, ss, ss, (int)(PX(s->L.st[i].x) - ss / 2.0f), (int)(PY(s->L.st[i].y) - ss / 2.0f));
        }
        free(q);
    }
#undef PX
#undef PY
    art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    free(o);
}

/* a level's map as its plan leaves it some minutes in (every station shown, the lines drawn) */
static void paint_level(int level, int size, uint8_t *rgba, int at_ticks)
{
    mtl_level L;
    if (!mtl_level_get(level, &L)) return;
    mtl_state *s = malloc(sizeof *s);
    if (!s) return;
    mtl_begin(s, &L);
    if (mtl_plans[level]) {                     /* the plan's actions up to that time */
        char a[MTL_ACT];
        const char *p = mtl_plans[level];
        while (*p) {
            while (*p == ' ') p++;
            int n = 0;
            while (p[n] && p[n] != ' ') n++;
            if (!n || n >= MTL_ACT) break;
            memcpy(a, p, (size_t)n);
            a[n] = 0;
            p += n;
            if (a[0] == '@' && atoi(a + 1) > at_ticks) break;
            mtl_do(s, a);
        }
    }
    s->nst = L.ns;
    paint_map(s, size, rgba, 1);
    free(s);
}

static void thumb(int v, int size, uint8_t *rgba)
{
    static const int LV[MTL_VARIANTS] = {2, 10, 26};
    paint_level(v < 0 ? 14 : LV[v % MTL_VARIANTS], size, rgba, v < 0 ? 6000 : 4000);
}

static void level_thumb(int level, int size, uint8_t *rgba) { paint_level(level, size, rgba, 3000); }

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    mtl_level L;
    if (chapter < 0 || chapter >= MTL_CHAPTERS || !mtl_icons[chapter] || !mtl_parse(mtl_icons[chapter], &L)) return;
    mtl_state *s = malloc(sizeof *s);
    if (!s) return;
    mtl_begin(s, &L);
    s->nst = L.ns;
    /* the icon's lines: its stations joined in order, the line count by the chapter */
    int nl = chapter == 3 ? 1 : 2, per = (L.ns + nl - 1) / nl;
    s->lines = nl;
    s->tunnels = 9;
    for (int l = 0; l < nl; l++) {
        int st[MTL_STOPS], n = 0;
        for (int i = l * per; i < L.ns && i < (l + 1) * per; i++) st[n++] = i;
        if (l && n) st[0] = 0;
        if (n >= 2) mtl_new_line(s, st, n);
    }
    paint_map(s, size, rgba, 1);
    free(s);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    memset(rgba, 0, (size_t)size * size * 4);
    float s = (float)size;
    pzk_stroke(rgba, size, size, 0.12f * s, 0.62f * s, 0.4f * s, 0.34f * s, 0.085f * s, LINE_RGB[0], 1);
    pzk_stroke(rgba, size, size, 0.4f * s, 0.34f * s, 0.88f * s, 0.34f * s, 0.085f * s, LINE_RGB[0], 1);
    pzk_stroke(rgba, size, size, 0.12f * s, 0.34f * s, 0.6f * s, 0.82f * s, 0.085f * s, LINE_RGB[1], 1);
    uint8_t *q = malloc((size_t)size * size * 4);
    if (!q) return;
    memset(q, 0, (size_t)size * size * 4);
    paint_shape(q, size, MTL_CIRCLE, WHITE, INK, 0.13f, 0.05f);
    art_blend(rgba, size, size, q, size, size, (int)(-0.1f * s), (int)(-0.16f * s));
    memset(q, 0, (size_t)size * size * 4);
    paint_shape(q, size, MTL_TRIANGLE, WHITE, INK, 0.12f, 0.05f);
    art_blend(rgba, size, size, q, size, size, (int)(0.38f * s), (int)(-0.16f * s));
    memset(q, 0, (size_t)size * size * 4);
    paint_shape(q, size, MTL_SQUARE, WHITE, INK, 0.11f, 0.05f);
    art_blend(rgba, size, size, q, size, size, (int)(0.1f * s), (int)(0.32f * s));
    free(q);
}

static const game metrolines_game = {
    .id = "metrolines", .order = 486, .variants = MTL_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .score = score, .par = par, .level_stars = level_stars,
    .levels = MTL_LEVELS, .chapters = MTL_CHAPTERS, .chapter_of = mtl_chapter_of, .start_random = start_random,
    .chapter_icon = chapter_icon, .level_thumb = level_thumb, .fill = 1, .tools = TOOLS, .variant_first = 1,
};
REGISTER_GAME(metrolines_game)
