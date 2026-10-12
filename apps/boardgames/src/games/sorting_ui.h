/*
 * Water Sort's and Ball Sort's screen (one screen, the rules of sorting.h): the house plank in its walnut frame, one or
 * two walnut shelves with the glass tubes standing on them, the liquids (Water Sort) or the marbles (Ball Sort) in the
 * twelve colours of hues.h, each layer or ball with its mark, a cork in each tube once it holds one colour, the extra
 * tube's place at the end of the last shelf, the hint's lamp in the bar.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a tube: it lifts (Water Sort), or its top ball rises over it (Ball Sort); tap another: the tube flies over to it,
 * tilts and pours, the liquid flowing in a stream, and goes back (the ball glides over and drops in). A move not allowed
 * shakes and stays up; a tap on the same tube puts it back. Or drag a tube (a ball) to another. Undo takes back one
 * move, all the way to the start (the extra tube too). The extra tube's place: a ghost tube with a brass plus; a tap
 * sets the tube there, once a puzzle (it keeps a brass band at its foot). The lamp: the hint lifts the tube to pour
 * from and lights the one to pour into (the extra tube's place when only the extra tube can help); red when nothing
 * can be done from here but undo. Solved: the corked tubes light up one by one, then together.
 *
 * Written once as static inline functions (the build takes a game's own files only), used by watersort.c and
 * ballsort.c, each with its own `sui` state.
 */
#ifndef SORTING_UI_H
#define SORTING_UI_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/hues.h"
#include "games/puzzles.h"
#include "games/sorting.h"
#include "shell.h"

#define SUI_FRAME 0.035f
#define SUI_LIFT_MS 130
#define SUI_GO_MS 300                           /* Water Sort: the tube flies to the pour */
#define SUI_FLOW_MS 150                         /* a layer flowing */
#define SUI_FLOW0_MS 140
#define SUI_BACK_MS 260
#define SUI_BALL_MS 380
#define SUI_SHAKE_MS 280
#define SUI_CORK_MS 240
#define SUI_EXTRA_MS 320
#define SUI_WAVE_STEP 80
#define SUI_WAVE_LEN 560
#define SUI_TILT_MAX 112.0f                     /* degrees */
#define SUI_ARC 13                              /* the interior's points: 4 corners, 9 round its bottom */

typedef int (*sui_new_fn)(srt_game *g, int variant, uint32_t seed, srt_move *way);
typedef int (*sui_level_fn)(srt_game *g, int level);

typedef struct {
    int balls;
    sui_new_fn make;
    srt_game G;
    int lifted;                                 /* the tube up (Water Sort) or whose ball is up; -1 */
    Uint32 lift_t0, shake_t0;
    struct { int on, from, to, n, colour, dir, h0, h1; float x0, y0; Uint32 t0, len; } mv;   /* the last move */
    struct { int on; Uint32 t0; } win;
    struct { int on, tube, moved; float x, y, x0, y0, gx, gy; } drag;
    struct { int on, to, extra, none; Uint32 t0; } hint;
    Uint32 cork_t0[SRT_TUBES];                  /* when each tube's cork came (0: none) */
    Uint32 extra_t0;
    hue_lamp lamp;
    /* the layout */
    box last_box;
    int made_key, made_small, fr, rows, per[2], show;   /* show: the places drawn (the menu's card: a few); 0 all */
    float bx, by, bs, pitch, tw, th, lh, gw, iw, base_y[2];
    float area[SRT_CAP + 1];                    /* the interior's area below each layer's top, upright */
    float poly[2 * SUI_ARC];                    /* the interior, the tube's centre at 0, 0 */
    SDL_Texture *t_board, *t_back, *t_front, *t_cork, *t_band, *t_plus, *t_small, *t_ball[HUE_N + 1],
        *t_mark[HUE_N + 1];
} sui;

/* ---- the geometry --------------------------------------------------------------------------------------------------- */
static inline int sui_slots(const sui *s) { return s->G.base + 1; }   /* the tubes and the extra tube's place */

static inline void sui_slot_rc(const sui *s, int k, int *row, int *col)
{
    *row = k < s->per[0] ? 0 : 1;
    *col = *row ? k - s->per[0] : k;
}

static inline float sui_slot_x(const sui *s, int k)
{
    int r, c;
    sui_slot_rc(s, k, &r, &c);
    return s->bx + s->bs / 2 + (c - (s->per[r] - 1) / 2.0f) * s->pitch;
}

static inline int sui_row(const sui *s, int k) { return k < s->per[0] ? 0 : 1; }
static inline float sui_rest_y(const sui *s, int k) { return s->base_y[sui_row(s, k)] - s->th / 2; }   /* centre */
static inline float sui_mouth_y(const sui *s, int k) { return s->base_y[sui_row(s, k)] - s->th; }
static inline float sui_floor_y(const sui *s, int k) { return s->base_y[sui_row(s, k)] - s->gw; }
static inline float sui_layer_y(const sui *s, int k, int i) { return sui_floor_y(s, k) - (i + 0.5f) * s->lh; }
static inline float sui_hover_y(const sui *s, int k) { return sui_mouth_y(s, k) - 0.72f * s->lh; }   /* a ball up */
static inline float sui_lift(const sui *s) { return 0.42f * s->lh; }

/* the sizes for a board of side S: the frame, the rows, the tubes */
static inline void sui_fit(sui *s, float S)
{
    int n = s->show ? s->show : sui_slots(s);
    s->rows = n > 8 ? 2 : 1;
    s->per[0] = s->rows == 2 ? (n + 1) / 2 : n;
    s->per[1] = n - s->per[0];
    s->fr = (int)(S * SUI_FRAME);
    float inner = S - 2 * s->fr, rh = inner / s->rows;
    s->pitch = inner / (s->show ? n : s->per[0] < 6 ? 6 : s->per[0]);
    /* the layer's height and the lift, in tube widths: a ball's height; the liquid's as tall as the row allows, from
     * 0.95 to 1.45 */
    float lift = s->balls ? 1.25f : 0.5f, tw0 = (s->balls ? 0.7f : 0.6f) * s->pitch;
    float k = s->balls ? 0.84f : clampf((0.93f * rh / tw0 - 0.96f) / (4 + lift), 0.95f, 1.45f);
    float need = 4 * k + 0.62f + 0.34f + lift * k;   /* the layers, the head room, the shelf, the lift */
    s->tw = floorf(fminf(tw0, 0.93f * rh / need));
    s->lh = k * s->tw;
    s->gw = fmaxf(2, roundf(0.075f * s->tw));
    s->iw = s->tw - 2 * s->gw;
    s->th = floorf(4 * s->lh + 0.62f * s->tw);
    for (int r = 0; r < s->rows; r++) {         /* the tubes' feet: low in each row, the lift's room above */
        float top = s->by + s->fr + r * rh, used = need * s->tw;
        s->base_y[r] = floorf(top + (rh - used) / 2 + lift * s->lh + s->th);
    }
}

/* the interior's outline round the tube's centre (y down): its mouth, its sides, its round bottom */
static inline void sui_interior(const sui *s, float *xy)
{
    float r = s->iw / 2, top = -s->th / 2, yc = s->th / 2 - s->gw - r;
    int k = 0;
    xy[k++] = -r, xy[k++] = top;
    xy[k++] = r, xy[k++] = top;
    for (int i = 0; i <= SUI_ARC - 4; i++) {
        float a = PUZ_PI * i / (SUI_ARC - 4);
        xy[k++] = r * cosf(a);
        xy[k++] = yc + r * sinf(a);
    }
    xy[k++] = -r, xy[k++] = yc;
}

/* a convex polygon clipped to y >= lo (below the line, the screen's y down) and y <= hi: its points in out */
static inline int sui_clip(const float *in, int n, float lo, float hi, float *out)
{
    float a[2 * 40], b[2 * 40];
    int m = n;
    memcpy(a, in, sizeof(float) * 2 * n);
    for (int pass = 0; pass < 2; pass++) {
        int k = 0;
        for (int i = 0; i < m; i++) {
            const float *p = a + 2 * i, *q = a + 2 * ((i + 1) % m);
            float dp = pass ? hi - p[1] : p[1] - lo, dq = pass ? hi - q[1] : q[1] - lo;
            if (dp >= 0) { b[2 * k] = p[0]; b[2 * k + 1] = p[1]; k++; }
            if ((dp >= 0) != (dq >= 0)) {
                float t = dp / (dp - dq);
                b[2 * k] = p[0] + (q[0] - p[0]) * t;
                b[2 * k + 1] = p[1] + (q[1] - p[1]) * t;
                k++;
            }
        }
        m = k;
        memcpy(a, b, sizeof(float) * 2 * m);
    }
    memcpy(out, a, sizeof(float) * 2 * m);
    return m;
}

static inline float sui_area(const float *p, int n)
{
    float a = 0;
    for (int i = 0; i < n; i++) {
        const float *u = p + 2 * i, *v = p + 2 * ((i + 1) % n);
        a += u[0] * v[1] - v[0] * u[1];
    }
    return fabsf(a) / 2;
}

/* the interior turned by deg (clockwise) round the centre, put at (cx, cy) */
static inline void sui_turned(const sui *s, float cx, float cy, float deg, float *xy)
{
    float a = deg * PUZ_PI / 180, c = cosf(a), sn = sinf(a);
    for (int i = 0; i < SUI_ARC; i++) {
        float x = s->poly[2 * i], y = s->poly[2 * i + 1];
        xy[2 * i] = cx + x * c - y * sn;
        xy[2 * i + 1] = cy + x * sn + y * c;
    }
}

/* the level (y) where the liquid's surface stands when it fills `area` of the polygon */
static inline float sui_level(const float *xy, int n, float area)
{
    float lo = 1e9f, hi = -1e9f, tmp[80];
    for (int i = 0; i < n; i++) {
        lo = fminf(lo, xy[2 * i + 1]);
        hi = fmaxf(hi, xy[2 * i + 1]);
    }
    if (area <= 0) return hi;
    float a = lo, b = hi;                       /* the area below y falls as y grows */
    for (int it = 0; it < 26; it++) {
        float m = (a + b) / 2;
        int k = sui_clip(xy, n, m, 1e9f, tmp);
        if (sui_area(tmp, k) > area) a = m;
        else b = m;
    }
    return (a + b) / 2;
}

/* the interior's area under v layers (v may be a fraction) */
static inline float sui_volume(const sui *s, float v)
{
    v = clampf(v, 0, SRT_CAP);
    int i = (int)v;
    if (i >= SRT_CAP) return s->area[SRT_CAP];
    return s->area[i] + (v - i) * (s->area[i + 1] - s->area[i]);
}

/* the lip a tilted tube pours from (dir 1: tilted to the right, its right lip), round the centre */
static inline void sui_lip(const sui *s, float deg, int dir, float *x, float *y)
{
    float a = deg * PUZ_PI / 180, lx = dir * s->tw / 2, ly = -s->th / 2;
    *x = lx * cosf(a) - ly * sinf(a);
    *y = lx * sinf(a) + ly * cosf(a);
}

/* the tilt (degrees, signed by dir) at which v layers reach the lip */
static inline float sui_tilt(const sui *s, float v, int dir)
{
    float area = sui_volume(s, v), a = 0, b = SUI_TILT_MAX, xy[2 * SUI_ARC];
    if (area <= 0) return dir * SUI_TILT_MAX;
    for (int it = 0; it < 22; it++) {
        float m = (a + b) / 2, lx, ly;
        sui_turned(s, 0, 0, dir * m, xy);
        sui_lip(s, dir * m, dir, &lx, &ly);
        if (sui_level(xy, SUI_ARC, area) < ly) b = m;   /* the surface above the lip: it spills */
        else a = m;
    }
    return dir * (a + b) / 2;
}

/* ---- the pictures --------------------------------------------------------------------------------------------------- */
static inline void sui_tint(uint8_t *p, const uint8_t rgb[3], float k)
{
    static const float plank[3] = {222, 184, 128};
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * rgb[i] / plank[i] * k, 0, 255);
}

/* how far inside the tube's outline (w x h, round at the bottom) a pixel is (below 0: outside) */
static inline float sui_inside(float px, float py, int w, int h)
{
    float r = w / 2.0f;
    if (py < h - r) return fminf(fminf(px, w - px), py + 2);   /* the open mouth: never an edge at the top */
    return r - hypotf(px - r, py - (h - r));
}

/* the glass: back (the tube's far side, under the liquid) or front (its walls, its rim, the light on it) */
static inline void sui_paint_glass(int w, int h, float gw, int front, uint8_t *o)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            float d = sui_inside(x + 0.5f, y + 0.5f, w, h), cov = clampf(d + 0.5f, 0, 1), u = (x + 0.5f) / w, a = 0;
            int lum = 255;
            p[0] = p[1] = p[2] = 0;
            p[3] = 0;
            if (cov <= 0) continue;
            if (d < gw) {                       /* the wall */
                a = front ? 0.62f - 0.3f * u : 0.18f;
                lum = front ? 236 : 200;
            } else if (y < 1.4f * gw) {         /* the rim at the mouth */
                a = front ? 0.5f : 0;
            } else if (front) {                 /* the light down its left, the shade down its right */
                float hl = clampf(1 - fabsf(u - 0.25f) / 0.07f, 0, 1), sh = clampf((u - 0.66f) / 0.26f, 0, 1);
                if (hl > 0) a = 0.42f * hl;
                else if (sh > 0) { a = 0.3f * sh; lum = 0; }
            } else {
                a = 0.12f;
                lum = 235;
            }
            if (y < 1.4f * gw && d < gw + 2) a = fmaxf(a, front ? 0.75f : 0.2f), lum = 248;
            p[0] = p[1] = p[2] = (uint8_t)lum;
            p[3] = (uint8_t)(255 * a * cov);
        }
}

/* a cork: the house wood, round at its edges, w x h */
static inline void sui_paint_cork(int w, int h, uint8_t *o)
{
    int q = w > h ? w : h;
    uint8_t *plank = malloc((size_t)q * q * 4);
    if (!plank) return;
    art_board_n(1, 1, q, 0, 0, plank);
    static const uint8_t cork[3] = {196, 150, 100};
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            memcpy(p, plank + 4 * (y * q + x), 4);
            float t = (y + 0.5f) / h;
            sui_tint(p, cork, 1.15f - 0.3f * t);
        }
    free(plank);
    puz_bevel(o, w, h, 0.28f * h, fmaxf(1.5f, 0.18f * h), 0.32f);
}

/* the extra tube's brass band, w x h */
static inline void sui_paint_band(int w, int h, uint8_t *o)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            float t = (y + 0.5f) / h, k = 1.15f - 0.45f * t + 0.25f * expf(-(t - 0.3f) * (t - 0.3f) * 40);
            p[0] = (uint8_t)clampf(214 * k, 0, 255), p[1] = (uint8_t)clampf(164 * k, 0, 255);
            p[2] = (uint8_t)clampf(70 * k, 0, 255), p[3] = 255;
        }
    puz_bevel(o, w, h, 0.4f * h, fmaxf(1, 0.25f * h), 0.25f);
}

/* the brass plus on the extra tube's place, d x d */
static inline void sui_paint_plus(int d, uint8_t *o)
{
    static const uint8_t brass[3] = {204, 160, 72};
    memset(o, 0, (size_t)d * d * 4);
    art_disc(d, brass, 1, 0, o);
    for (int y = 0; y < d; y++)
        for (int x = 0; x < d; x++) {
            uint8_t *p = o + 4 * (y * d + x);
            float u = (x + 0.5f) / d - 0.5f, v = (y + 0.5f) / d - 0.5f, r = hypotf(u, v);
            float k = 1.1f - 0.5f * (u + v) * 0.8f - (r > 0.43f ? 0.25f : 0);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k, 0, 255);
        }
    puz_ink_rect(o, d, d, 0.44f * d, 0.24f * d, 0.56f * d, 0.76f * d, PUZ_CREAM, 1);
    puz_ink_rect(o, d, d, 0.24f * d, 0.44f * d, 0.76f * d, 0.56f * d, PUZ_CREAM, 1);
}

/* the backdrop: the plank in its walnut frame (w x w), a walnut shelf along each row's foot */
static inline void sui_paint_board(const sui *s, int w, uint8_t *o)
{
    art_board_n(1, 1, w - 2 * s->fr, s->fr, 0, o);
    static const uint8_t walnut[3] = {118, 78, 50};
    float sh = 0.34f * s->tw;
    for (int r = 0; r < s->rows; r++) {
        float y0 = s->base_y[r] - s->by - 0.06f * s->tw, x0 = s->fr + 0.1f * s->pitch, x1 = w - s->fr - 0.1f * s->pitch;
        float ry = y0 - 0.8f * s->th, rh = 0.2f * s->tw;   /* the rack's back rail, behind the tubes' tops */
        for (int y = (int)ry; y < (int)(ry + rh); y++)
            for (int x = (int)(x0 + 0.2f * s->pitch); x < (int)(x1 - 0.2f * s->pitch); x++) {
                uint8_t *p = o + 4 * (y * w + x);
                float t = (y - ry) / rh;
                sui_tint(p, walnut, (t < 0.3f ? 1.3f : t > 0.75f ? 0.78f : 1.05f) * 1.2f);
            }
        for (int y = (int)(ry + rh); y < (int)(ry + 1.7f * rh); y++)
            for (int x = (int)(x0 + 0.2f * s->pitch); x < (int)(x1 - 0.2f * s->pitch); x++) {
                uint8_t *p = o + 4 * (y * w + x);
                float k = 0.78f + 0.22f * (y - ry - rh) / (0.7f * rh);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
            }
        for (int y = (int)y0; y < (int)(y0 + sh) && y < w - s->fr; y++)
            for (int x = (int)x0; x < (int)x1; x++) {
                uint8_t *p = o + 4 * (y * w + x);
                float t = (y - y0) / sh, k = t < 0.3f ? 1.32f - 0.2f * t : t > 0.85f ? 0.72f : 1.02f;
                sui_tint(p, walnut, k * 1.2f);
            }
        for (int y = (int)(y0 + sh); y < (int)(y0 + 1.5f * sh) && y < w - s->fr; y++)   /* its shadow */
            for (int x = (int)(x0 + 0.1f * sh); x < (int)(x1 + 0.1f * sh) && x < w - s->fr; x++) {
                uint8_t *p = o + 4 * (y * w + x);
                float k = 0.7f + 0.3f * (y - y0 - sh) / (0.5f * sh);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
            }
    }
}

/* liquid in an upright tube painted into a picture (the thumbnails): units colours from the bottom, the tube w x h at
 * (ox, oy), layers lh high */
static inline void sui_paint_liquid(uint8_t *o, int W, int H, int ox, int oy, int w, int h, float gw, float lh,
                                    const uint8_t *units, int n)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float d = sui_inside(x + 0.5f, y + 0.5f, w, h) - gw, up = (h - gw) - (y + 0.5f);
            int i = (int)(up / lh);
            if (d <= -0.5f || up < 0 || i >= n || ox + x < 0 || oy + y < 0 || ox + x >= W || oy + y >= H) continue;
            uint8_t q[3], *p = o + 4 * ((oy + y) * W + ox + x);
            hue_rgb(units[i], q);
            float cov = clampf(d + 0.5f, 0, 1), top = i == n - 1 && up > (i + 1) * lh - 0.08f * lh ? 1.25f : 1;
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] + (clampf(q[c] * top, 0, 255) - p[c]) * cov);
            p[3] = (uint8_t)fmaxf(p[3], 255 * cov);
        }
}

/* ---- the layout ----------------------------------------------------------------------------------------------------- */
static inline void sui_layout(sui *s, box b)
{
    s->last_box = b;
    float S = floorf(fminf(b.w, b.h));
    s->bs = S;
    s->bx = floorf(b.x + (b.w - S) / 2);
    s->by = floorf(b.y + (b.h - S) / 2);
    sui_fit(s, S);
    sui_interior(s, s->poly);
    float xy[2 * SUI_ARC], tmp[80];
    sui_turned(s, 0, 0, 0, xy);
    for (int i = 0; i <= SRT_CAP; i++) {
        int k = sui_clip(xy, SUI_ARC, s->th / 2 - s->gw - i * s->lh, 1e9f, tmp);
        s->area[i] = i ? sui_area(tmp, k) : 0;
    }
    int key = ((int)S * 16 + s->G.base) * 2 + s->balls;
    if (s->made_key != key) {
        s->made_key = key;
        int w = (int)S, tw = (int)s->tw, th = (int)s->th;
        ui_free(&s->t_board);
        sui_paint_board(s, w, ui_buf(w, w));
        s->t_board = ui_tex(w, w, ui_buf(w, w));
        ui_free(&s->t_back);
        sui_paint_glass(tw, th, s->gw, 0, ui_buf(tw, th));
        s->t_back = ui_tex(tw, th, ui_buf(tw, th));
        ui_free(&s->t_front);
        sui_paint_glass(tw, th, s->gw, 1, ui_buf(tw, th));
        s->t_front = ui_tex(tw, th, ui_buf(tw, th));
        int cw = (int)(s->iw + 0.12f * s->tw), ch = (int)(0.42f * s->tw);
        ui_free(&s->t_cork);
        sui_paint_cork(cw, ch, ui_buf(cw, ch));
        s->t_cork = ui_tex(cw, ch, ui_buf(cw, ch));
        int bw = tw + 2, bh = (int)fmaxf(4, 0.2f * s->tw);
        ui_free(&s->t_band);
        sui_paint_band(bw, bh, ui_buf(bw, bh));
        s->t_band = ui_tex(bw, bh, ui_buf(bw, bh));
        int pd = (int)(0.62f * s->tw);
        ui_free(&s->t_plus);
        sui_paint_plus(pd, ui_buf(pd, pd));
        s->t_plus = ui_tex(pd, pd, ui_buf(pd, pd));
        int d = (int)(s->balls ? s->lh * 0.98f : fminf(0.62f * s->lh, 0.58f * s->iw));
        for (int c = 1; c <= HUE_N; c++) {
            ui_free(&s->t_ball[c]);
            ui_free(&s->t_mark[c]);
            if (s->balls) {
                hue_marble(c, d, ui_buf(d, d));
                s->t_ball[c] = ui_tex(d, d, ui_buf(d, d));
            } else {
                hue_mark(c, d, ui_buf(d, d));
                s->t_mark[c] = ui_tex(d, d, ui_buf(d, d));
                uint8_t tone[3];
                float a;
                hue_mark_tone(c, tone, &a);
                SDL_SetTextureColorMod(s->t_mark[c], tone[0], tone[1], tone[2]);
                SDL_SetTextureAlphaMod(s->t_mark[c], (Uint8)(255 * a));
            }
        }
    }
}

/* ---- the state ------------------------------------------------------------------------------------------------------ */
static inline void sui_corks_now(sui *s)        /* the corks as they are, none coming */
{
    Uint32 now = ui_ms();
    for (int t = 0; t < SRT_TUBES; t++)
        s->cork_t0[t] = t < s->G.tubes && srt_complete(&s->G.p, t) ? (now > SUI_CORK_MS ? now - SUI_CORK_MS : 1) : 0;
}

static inline void sui_calm(sui *s)
{
    s->lifted = -1;
    s->mv.on = s->win.on = 0;
    s->hint.on = s->hint.none = 0;
    memset(&s->drag, 0, sizeof s->drag);
    s->shake_t0 = 0;
    s->extra_t0 = 0;
    sui_corks_now(s);
}

static inline void sui_begin(sui *s)
{
    sui_calm(s);
    s->made_key = -1;
    if (s->last_box.w > 0) sui_layout(s, s->last_box);
}

static inline void sui_start(sui *s, int variant)   /* a puzzle made at random */
{
    s->make(&s->G, variant, (uint32_t)ui_random(0x7fffffff) + 1u, NULL);
    sui_begin(s);
}

static inline void sui_start_level(sui *s, sui_level_fn level_of, int level)   /* a designed level */
{
    if (!level_of(&s->G, level)) level_of(&s->G, 0);
    sui_begin(s);
}

/* the stars: the pours against the par; the extra tube used, two at most */
static inline int sui_stars(const sui *s)
{
    int k = ui_par_stars(s->G.moves, s->G.par);
    return s->G.tubes > s->G.base && k > 2 ? 2 : k;
}

static inline Uint32 sui_win_len(const sui *s) { return (Uint32)(s->G.tubes * SUI_WAVE_STEP + SUI_WAVE_LEN + 600); }

static inline void sui_won(sui *s, Uint32 at)
{
    if (!srt_solved(&s->G)) return;
    s->win.on = 1;
    s->win.t0 = at;
}

static inline int sui_busy(const sui *s)
{
    Uint32 now = ui_ms();
    return (s->mv.on && now - s->mv.t0 < s->mv.len) || (s->win.on && now - s->win.t0 < sui_win_len(s));
}

static inline int sui_undo(sui *s)            /* a pour, or the extra tube (its place shows again) */
{
    int r = srt_undo(&s->G);
    sui_calm(s);
    return r != 0;
}

static inline int sui_script(sui *s, const char *moves)
{
    int ok = srt_script(&s->G, moves);
    sui_calm(s);
    sui_won(s, ui_ms());
    return ok;
}

/* the move from tube `from` (up, or carried: its centre (x0, y0) for Water Sort, its ball's for Ball Sort) to `to` */
static inline void sui_go(sui *s, int from, int to, float x0, float y0)
{
    int h0 = s->G.p.h[from], h1 = s->G.p.h[to], colour = srt_top(&s->G.p, from);
    int n = srt_move_do(&s->G, from, to);
    if (!n) return;
    Uint32 now = ui_ms();
    s->mv.on = 1;
    s->mv.from = from;
    s->mv.to = to;
    s->mv.n = n;
    s->mv.colour = colour;
    s->mv.h0 = h0;
    s->mv.h1 = h1;
    s->mv.x0 = x0;
    s->mv.y0 = y0;
    s->mv.t0 = now;
    float fx = sui_slot_x(s, from), tx = sui_slot_x(s, to);
    s->mv.dir = fx < tx - 1 ? 1 : fx > tx + 1 ? -1 : tx < s->bx + s->bs / 2 ? -1 : 1;
    Uint32 land;
    if (s->balls) {
        s->mv.len = SUI_BALL_MS;
        land = SUI_BALL_MS - 40;
        ui_sound(SND_MOVE);
        ui_sound_later(SND_PLACE, (int)land);
    } else {
        Uint32 flow = SUI_FLOW0_MS + SUI_FLOW_MS * (Uint32)n;
        s->mv.len = SUI_GO_MS + flow + SUI_BACK_MS;
        land = SUI_GO_MS + flow;
        ui_sound(SND_MOVE);
        for (int i = 0; i < n; i++) ui_sound_later(SND_DROP, (int)(SUI_GO_MS + 60 + i * SUI_FLOW_MS));
    }
    s->lifted = -1;
    s->hint.on = s->hint.none = 0;
    if (srt_complete(&s->G.p, to)) {            /* its cork comes once the move has landed */
        s->cork_t0[to] = now + land;
        ui_sound_later(SND_PLACE, (int)(land + SUI_CORK_MS - 30));
    }
    sui_won(s, now + s->mv.len);
    ui_animate();
}

/* ---- the board on the screen ---------------------------------------------------------------------------------------- */
/* a polygon filled with one colour (alpha a) */
static inline void sui_fill_poly(const float *xy, int n, const uint8_t rgb[3], float a)
{
    if (n < 3) return;
    SDL_Vertex v[40];
    int idx[3 * 40], k = 0;
    SDL_Color c = {rgb[0], rgb[1], rgb[2], (Uint8)(255 * clampf(a, 0, 1))};
    for (int i = 0; i < n; i++) v[i] = (SDL_Vertex){{xy[2 * i], xy[2 * i + 1]}, c, {0, 0}};
    for (int i = 1; i + 1 < n; i++) idx[k++] = 0, idx[k++] = i, idx[k++] = i + 1;
    SDL_RenderGeometry(ui.r, NULL, v, n, idx, k);
}

/* the liquid of a tube at (cx, cy) tilted deg: its units from the bottom (colours), `vol` layers of them (a fraction
 * while it flows), their marks when upright */
static inline void sui_liquid(const sui *s, float cx, float cy, float deg, const uint8_t *units, int n, float vol)
{
    float xy[2 * SUI_ARC], band[80];
    sui_turned(s, cx, cy, deg, xy);
    float below = 1e9f;                         /* the bottom of the band drawn next */
    int upright = fabsf(deg) < 0.5f;
    for (int i = 0; i < n && vol > i; i++) {
        int j = i;                              /* a run of one colour: one band */
        while (j + 1 < n && vol > j + 1 && units[j + 1] == units[i]) j++;
        float top = sui_level(xy, SUI_ARC, sui_volume(s, fminf(vol, j + 1.0f)));
        uint8_t q[3];
        hue_rgb(units[i], q);
        int k = sui_clip(xy, SUI_ARC, top, below, band);
        sui_fill_poly(band, k, q, 1);
        if (j + 1 >= n || vol <= j + 1) {       /* the surface: a lighter line */
            uint8_t l[3] = {(uint8_t)fminf(255, q[0] * 1.2f + 30), (uint8_t)fminf(255, q[1] * 1.2f + 30),
                            (uint8_t)fminf(255, q[2] * 1.2f + 30)};
            k = sui_clip(xy, SUI_ARC, top, top + fmaxf(2, 0.07f * s->lh), band);
            sui_fill_poly(band, k, l, 0.9f);
        }
        if (upright)
            for (int u = i; u <= j; u++) {
                float y0 = cy + s->th / 2 - s->gw - (u + 1) * s->lh, y1 = y0 + s->lh;
                float lo = fmaxf(y0, top), shown = y1 - lo;
                if (shown < 0.55f * s->lh) continue;
                float d = (float)(int)fminf(0.62f * s->lh, 0.58f * s->iw);
                ui_put_c(s->t_mark[units[u]], cx, (lo + y1) / 2 + 0.03f * s->lh, d, d);
            }
        below = top;
        i = j;
    }
}

static inline void sui_glass(const sui *s, SDL_Texture *t, float cx, float cy, float deg, float alpha)
{
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha));
    ui_put_rot(t, cx, cy, s->tw, s->th, deg);
    SDL_SetTextureAlphaMod(t, 255);
}

static inline void sui_ball(const sui *s, int c, float x, float y)
{
    float d = (float)(int)(s->lh * 0.98f);
    ui_put_c(s->t_ball[c], x, y, d, d);
}

/* a tube with what it holds: at (cx, cy) tilted deg; vol: how many of its units show (a fraction while it flows) */
static inline void sui_tube(const sui *s, int t, float cx, float cy, float deg, const uint8_t *units, int n, float vol,
                            int hide_top)
{
    sui_glass(s, s->t_back, cx, cy, deg, 1);
    if (s->balls) {
        for (int i = 0; i < n - hide_top; i++) sui_ball(s, units[i], cx, cy + s->th / 2 - s->gw - (i + 0.5f) * s->lh);
    } else sui_liquid(s, cx, cy, deg, units, n, vol);
    sui_glass(s, s->t_front, cx, cy, deg, 1);
    if (t == s->G.base) {                       /* the extra tube: its brass band */
        float bh = fmaxf(4, 0.2f * s->tw);
        ui_put_c(s->t_band, cx, cy + s->th / 2 - 0.42f * s->tw, s->tw + 2, bh);
    }
}

static inline void sui_cork(const sui *s, int t, float cx, float mouth, Uint32 now)
{
    if (!s->cork_t0[t] || now < s->cork_t0[t]) return;
    float q = clampf((now - s->cork_t0[t]) / (float)SUI_CORK_MS, 0, 1), e = q * q;
    float cw = s->iw + 0.12f * s->tw, ch = 0.42f * s->tw, y = mouth + 0.06f * s->tw - (1 - e) * 1.2f * s->tw;
    SDL_SetTextureAlphaMod(s->t_cork, (Uint8)(255 * clampf(q * 3, 0, 1)));
    ui_put_c(s->t_cork, cx, y, cw, ch);
    SDL_SetTextureAlphaMod(s->t_cork, 255);
    if (q < 1) ui_animate();
}

/* where a gliding ball is: up from (x0, y0) to the hover line, across, down into its place */
static inline void sui_glide(const sui *s, float t, float *x, float *y)
{
    float x1 = sui_slot_x(s, s->mv.to), y1 = sui_layer_y(s, s->mv.to, s->mv.h1);
    float hover = fminf(sui_hover_y(s, s->mv.from), sui_hover_y(s, s->mv.to));
    float top = fminf(hover, s->mv.y0);
    float up = s->mv.y0 - top, across = fabsf(x1 - s->mv.x0), down = y1 - top, all = up + across + down;
    float d = puz_ease(t) * all;
    if (d < up) { *x = s->mv.x0; *y = s->mv.y0 - d; return; }
    d -= up;
    if (d < across) { *x = s->mv.x0 + (x1 > s->mv.x0 ? d : -d); *y = top; return; }
    *x = x1;
    *y = top + (d - across);
}

/* Water Sort's pour: the source tube's centre and tilt at time t (ms) of the move; f: how much has flowed (0..1) */
static inline void sui_pour_at(const sui *s, Uint32 t, float *cx, float *cy, float *deg, float *f)
{
    int dir = s->mv.dir, n = s->mv.n;
    float flow = (float)(SUI_FLOW0_MS + SUI_FLOW_MS * n);
    float tx = sui_slot_x(s, s->mv.to), lipx = tx - dir * 0.1f * s->tw, lipy = sui_mouth_y(s, s->mv.to) - 0.28f * s->tw;
    float a0 = sui_tilt(s, (float)s->mv.h0, dir), a1 = sui_tilt(s, (float)(s->mv.h0 - n), dir);
    float lx, ly;
    if (t < SUI_GO_MS) {                        /* flying over, tilting till the liquid reaches the lip */
        float u = puz_ease(t / (float)SUI_GO_MS);
        *deg = a0 * u;
        sui_lip(s, 0, dir, &lx, &ly);
        float sx = s->mv.x0 + lx, sy = s->mv.y0 + ly;   /* the lip where it started */
        sui_lip(s, *deg, dir, &lx, &ly);
        float px = sx + (lipx - sx) * u, py = sy + (lipy - sy) * u - sinf(PUZ_PI * u) * 0.3f * s->th;
        *cx = px - lx;
        *cy = py - ly;
        *f = 0;
        return;
    }
    t -= SUI_GO_MS;
    if (t < flow) {                             /* pouring: turning on its lip as it empties */
        *f = clampf((t - SUI_FLOW0_MS * 0.5f) / (flow - SUI_FLOW0_MS * 0.5f), 0, 1);
        *deg = sui_tilt(s, s->mv.h0 - n * *f, dir);
        sui_lip(s, *deg, dir, &lx, &ly);
        *cx = lipx - lx;
        *cy = lipy - ly;
        return;
    }
    t -= (Uint32)flow;                          /* back to its place, upright */
    *f = 1;
    float u = puz_ease(t / (float)SUI_BACK_MS);
    sui_lip(s, a1, dir, &lx, &ly);
    float px = lipx - lx, py = lipy - ly, hx = sui_slot_x(s, s->mv.from), hy = sui_rest_y(s, s->mv.from);
    *deg = a1 * (1 - u);
    *cx = px + (hx - px) * u;
    *cy = py + (hy - py) * u - sinf(PUZ_PI * u) * 0.15f * s->th;
}

static inline void sui_units(const sui *s, int t, uint8_t *u)
{
    for (int i = 0; i < SRT_CAP; i++) u[i] = s->G.p.c[t][i];
}

static inline void sui_draw(sui *s)
{
    if (s->made_key < 0) sui_layout(s, s->last_box);
    Uint32 now = ui_ms();
    ui_put(s->t_board, s->bx, s->by, s->bs, s->bs);
    Uint32 mt = s->mv.on ? now - s->mv.t0 : 0;
    if (s->mv.on && mt >= s->mv.len) s->mv.on = 0;
    float wt = s->win.on && now >= s->win.t0 ? (float)(now - s->win.t0) : -1;
    if (s->win.on && now < s->win.t0) ui_animate();
    /* the pour's state: the source in flight, how much has flowed */
    float pcx = 0, pcy = 0, pdeg = 0, pf = 1;
    if (s->mv.on && !s->balls) sui_pour_at(s, mt, &pcx, &pcy, &pdeg, &pf);
    /* the hint's light under the tube to pour into (or the extra tube's place) */
    if (s->hint.on) {
        float q = 0.65f + 0.35f * sinf((now - s->hint.t0) / 180.0f);
        int k = s->hint.extra ? s->G.base : s->hint.to;
        ui_mark_gold(sui_slot_x(s, k), sui_rest_y(s, k), 1.25f * s->th * q);
        ui_animate();
    }
    for (int k = 0; k < sui_slots(s); k++) {
        float x = sui_slot_x(s, k), y = sui_rest_y(s, k);
        if (k >= s->G.tubes) {                  /* the extra tube's place: a ghost tube, a brass plus */
            float pd = (float)(int)(0.62f * s->tw);
            sui_glass(s, s->t_back, x, y, 0, 0.55f);
            sui_glass(s, s->t_front, x, y, 0, 0.3f);
            ui_put_c(s->t_plus, x, y + 0.1f * s->th, pd, pd);
            continue;
        }
        if (wt >= 0) {                          /* solved: each corked tube lights up in turn, from its foot up */
            for (int i = 0; i < 3 && s->G.p.h[k]; i++) {
                float q = puz_pulse(wt, k * SUI_WAVE_STEP + i * 70, SUI_WAVE_LEN);
                if (q > 0) ui_mark_gold(x, y + (1 - i) * 0.32f * s->th, 2.6f * s->tw * q);
            }
        }
        uint8_t u[SRT_CAP];
        sui_units(s, k, u);
        int n = s->G.p.h[k];
        float vol = (float)n;
        if (s->mv.on && !s->balls && k == s->mv.from) continue;   /* in flight: drawn last */
        if (s->mv.on && !s->balls && k == s->mv.to) vol = s->mv.h1 + s->mv.n * pf;
        int hide = 0;
        if (s->balls && ((s->mv.on && k == s->mv.to) || ((k == s->lifted || (s->drag.on && s->drag.moved &&
                                                                                k == s->drag.tube)) && n)))
            hide = 1;                           /* its top ball is up, carried or gliding in */
        if (!s->balls && (k == s->lifted || (s->drag.on && s->drag.moved && k == s->drag.tube))) {
            if (s->drag.on && s->drag.moved && k == s->drag.tube) continue;   /* in the finger: drawn last */
            float t = clampf((now - s->lift_t0) / (float)SUI_LIFT_MS, 0, 1), st = (float)(now - s->shake_t0);
            y -= sui_lift(s) * puz_ease(t);
            if (s->shake_t0 && st < SUI_SHAKE_MS)
                x += sinf(st / SUI_SHAKE_MS * 6 * PUZ_PI) * 0.12f * s->tw * (1 - st / SUI_SHAKE_MS);
            if (t < 1 || (s->shake_t0 && st < SUI_SHAKE_MS)) ui_animate();
        }
        if (s->extra_t0 && k == s->G.base) {    /* the extra tube coming */
            float q = clampf((now - s->extra_t0) / (float)SUI_EXTRA_MS, 0, 1);
            y -= (1 - puz_ease(q)) * 0.5f * s->th;
            if (q < 1) ui_animate();
        }
        sui_tube(s, k, x, y, 0, u, n, vol, hide);
        sui_cork(s, k, x, y - s->th / 2, now);
        if (wt >= 0) {
            float q = puz_pulse(wt, k * SUI_WAVE_STEP, SUI_WAVE_LEN);
            if (q > 0 && s->G.p.h[k]) ui_mark_gold(x, y - s->th / 2, 0.8f * s->tw * q);
        }
    }
    if (wt >= 0) {                              /* then all together */
        float q = puz_pulse(wt, s->G.tubes * SUI_WAVE_STEP, SUI_WAVE_LEN + 500);
        if (q > 0)
            for (int k = 0; k < s->G.tubes; k++)
                for (int i = 0; i < 3 && s->G.p.h[k]; i++)
                    ui_mark_gold(sui_slot_x(s, k), sui_rest_y(s, k) + (1 - i) * 0.32f * s->th, 2.2f * s->tw * q);
        if (wt < sui_win_len(s)) ui_animate();
        else s->win.on = 0;
    }
    if (s->balls) {
        if (s->lifted >= 0 && s->G.p.h[s->lifted]) {   /* the ball up over its tube; refused: it shakes */
            int k = s->lifted;
            float t = clampf((now - s->lift_t0) / (float)SUI_LIFT_MS, 0, 1), y0 = sui_layer_y(s, k, s->G.p.h[k] - 1);
            float x = sui_slot_x(s, k), y = y0 + (sui_hover_y(s, k) - y0) * puz_ease(t), st = (float)(now - s->shake_t0);
            if (s->shake_t0 && st < SUI_SHAKE_MS)
                x += sinf(st / SUI_SHAKE_MS * 6 * PUZ_PI) * 0.12f * s->tw * (1 - st / SUI_SHAKE_MS);
            if (t < 1 || (s->shake_t0 && st < SUI_SHAKE_MS)) ui_animate();
            sui_ball(s, srt_top(&s->G.p, k), x, y);
        }
        if (s->mv.on) {
            float x, y;
            sui_glide(s, mt / (float)SUI_BALL_MS, &x, &y);
            sui_ball(s, s->mv.colour, x, y);
            ui_animate();
        }
        if (s->drag.on && s->drag.moved && s->G.p.h[s->drag.tube])
            sui_ball(s, srt_top(&s->G.p, s->drag.tube), s->drag.x - s->drag.gx, s->drag.y - s->drag.gy);
        return;
    }
    if (s->mv.on) {                             /* the tube pouring: the stream, then the tube over it */
        uint8_t u[SRT_CAP];
        sui_units(s, s->mv.from, u);
        for (int i = 0; i < s->mv.n; i++) u[s->mv.h0 - s->mv.n + i] = (uint8_t)s->mv.colour;
        Uint32 flow = SUI_FLOW0_MS + SUI_FLOW_MS * (Uint32)s->mv.n;
        if (mt >= SUI_GO_MS && mt < SUI_GO_MS + flow) {
            float lx, ly, q = (mt - SUI_GO_MS) / (float)flow;
            sui_lip(s, pdeg, s->mv.dir, &lx, &ly);
            float sx = pcx + lx, sy = pcy + ly, xy[2 * SUI_ARC];
            sui_turned(s, sui_slot_x(s, s->mv.to), sui_rest_y(s, s->mv.to), 0, xy);
            float surf = sui_level(xy, SUI_ARC, sui_volume(s, s->mv.h1 + s->mv.n * pf));
            float y0 = sy, y1 = surf, sw = fmaxf(3, 0.16f * s->tw);
            if (q < 0.15f) y1 = sy + (surf - sy) * q / 0.15f;   /* the stream reaching down, then leaving */
            if (q > 0.85f) y0 = sy + (surf - sy) * (q - 0.85f) / 0.15f;
            uint8_t c[3];
            hue_rgb(s->mv.colour, c);
            ui_fill(sx - sw / 2, y0, sw, fmaxf(0, y1 - y0), c[0], c[1], c[2], 235);
            ui_fill(sx - sw / 2 + 0.2f * sw, y0, 0.22f * sw, fmaxf(0, y1 - y0), 255, 255, 255, 60);
        }
        sui_tube(s, s->mv.from, pcx, pcy, pdeg, u, s->mv.h0, s->mv.h0 - s->mv.n * pf, 0);
        ui_animate();
    }
    if (s->drag.on && s->drag.moved) {          /* a tube in the finger */
        uint8_t u[SRT_CAP];
        sui_units(s, s->drag.tube, u);
        sui_tube(s, s->drag.tube, s->drag.x - s->drag.gx, s->drag.y - s->drag.gy, 0, u, s->G.p.h[s->drag.tube],
                 s->G.p.h[s->drag.tube], 0);
    }
}

/* ---- the pointer ---------------------------------------------------------------------------------------------------- */
static inline int sui_slot_at(const sui *s, float x, float y)
{
    if (x < s->bx || x >= s->bx + s->bs || y < s->by || y >= s->by + s->bs) return -1;
    float rh = (s->bs - 2 * s->fr) / s->rows;
    int r = (int)clampf((y - s->by - s->fr) / rh, 0, s->rows - 1);
    int c = (int)floorf((x - (s->bx + s->bs / 2)) / s->pitch + s->per[r] / 2.0f);
    if (c < 0 || c >= s->per[r]) return -1;
    return r ? s->per[0] + c : c;
}

static inline void sui_set_extra(sui *s)
{
    if (!srt_extra(&s->G)) { ui_sound(SND_DENY); return; }
    s->extra_t0 = ui_ms();
    s->hint.on = s->hint.none = 0;
    ui_sound(SND_PLACE);
    ui_animate();
}

static inline void sui_tap(sui *s, int k)
{
    if (k >= s->G.tubes) {                      /* the extra tube's place */
        s->lifted = -1;
        sui_set_extra(s);
        return;
    }
    if (s->lifted < 0) {
        if (!s->G.p.h[k] || srt_complete(&s->G.p, k)) { ui_sound(SND_DENY); return; }
        s->lifted = k;
        s->lift_t0 = ui_ms();
        s->shake_t0 = 0;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (k == s->lifted) {                       /* back down */
        s->lifted = -1;
        s->hint.on = 0;
        ui_sound(SND_PLACE);
        ui_animate();
        return;
    }
    if (!srt_can(&s->G, s->lifted, k)) {
        s->shake_t0 = ui_ms();
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    int from = s->lifted;
    float x = sui_slot_x(s, from);
    if (s->balls) sui_go(s, from, k, x, sui_hover_y(s, from));
    else sui_go(s, from, k, x, sui_rest_y(s, from) - sui_lift(s));
}

static inline void sui_press(sui *s, float x, float y)
{
    memset(&s->drag, 0, sizeof s->drag);
    if (srt_solved(&s->G)) return;
    if (s->mv.on) {                             /* the last move lands at once */
        s->mv.on = 0;
        for (int t = 0; t < SRT_TUBES; t++)
            if (s->cork_t0[t] > ui_ms()) s->cork_t0[t] = ui_ms();
    }
    int k = sui_slot_at(s, x, y);
    if (k < 0) return;
    s->drag.on = 1;
    s->drag.tube = k;
    s->drag.x = s->drag.x0 = x;
    s->drag.y = s->drag.y0 = y;
}

static inline void sui_motion(sui *s, float x, float y)
{
    if (!s->drag.on) return;
    s->drag.x = x;
    s->drag.y = y;
    int k = s->drag.tube;
    if (!s->drag.moved && k < s->G.tubes && hypotf(x - s->drag.x0, y - s->drag.y0) > 0.45f * s->tw && s->G.p.h[k] &&
        !srt_complete(&s->G.p, k) && (s->lifted < 0 || s->lifted == k)) {   /* taken in hand */
        s->drag.moved = 1;
        float dx = sui_slot_x(s, k), dy;
        if (s->balls) dy = s->lifted == k ? sui_hover_y(s, k) : sui_layer_y(s, k, s->G.p.h[k] - 1);
        else dy = sui_rest_y(s, k) - (s->lifted == k ? sui_lift(s) : 0);
        s->drag.gx = s->drag.x0 - dx;
        s->drag.gy = s->drag.y0 - dy;
        if (s->balls && (fabsf(s->drag.gx) > s->pitch / 2 || fabsf(s->drag.gy) > 1.5f * s->lh)) s->drag.gx = s->drag.gy = 0;
        s->lifted = -1;
        ui_sound(SND_TAP);
    }
    ui_animate();
}

static inline void sui_release(sui *s, float x, float y)
{
    if (!s->drag.on) return;
    s->drag.on = 0;
    ui_animate();
    int k = sui_slot_at(s, x, y), from = s->drag.tube;
    if (!s->drag.moved) {
        if (k == from) sui_tap(s, k);
        return;
    }
    if (k == from || k < 0) {                   /* back in its place */
        ui_sound(SND_PLACE);
        return;
    }
    if (k < s->G.tubes && srt_can(&s->G, from, k)) {
        sui_go(s, from, k, x - s->drag.gx, y - s->drag.gy);
        return;
    }
    s->lifted = from;                           /* up over its own place, shaking */
    s->lift_t0 = ui_ms() - SUI_LIFT_MS;
    s->shake_t0 = ui_ms();
    ui_sound(SND_DENY);
}

/* ---- the bar: the moves, the hint's lamp ---------------------------------------------------------------------------- */
static inline void sui_bar(sui *s, box b)
{
    box left = b;
    left.w = b.w - hue_lamp_room();
    puz_bar_moves(left, left.w > 2.2f * ui.unit ? s->t_small : NULL, s->G.moves);
    if (srt_solved(&s->G)) { s->lamp.bar.w = 0; return; }
    Uint32 now = ui_ms();
    int none = s->hint.none && now - s->hint.t0 < 1400;
    if (none) ui_animate();
    hue_lamp_draw(&s->lamp, b, s->hint.on && (now - s->hint.t0) / 320 % 2 == 0, none);
}

static inline int sui_bar_press(sui *s, float x, float y)
{
    if (srt_solved(&s->G) || !hue_lamp_hit(&s->lamp, x, y)) return 0;
    if (s->mv.on) s->mv.on = 0;
    int from, to, extra;
    s->hint.t0 = ui_ms();
    s->hint.on = s->hint.none = 0;
    if (srt_hint(&s->G, &from, &to, &extra) >= 0) {
        s->hint.on = 1;
        s->hint.to = to;
        s->hint.extra = 0;
        s->lifted = from;                       /* the tube to pour from, lifted as by a tap */
        s->lift_t0 = ui_ms();
        s->shake_t0 = 0;
        ui_sound(SND_TAP);
    } else if (extra) {
        s->hint.on = 1;
        s->hint.extra = 1;
        s->lifted = -1;
        ui_sound(SND_TAP);
    } else {
        s->hint.none = 1;
        ui_sound(SND_DENY);
    }
    ui_animate();
    return 1;
}

/* ---- the shell's pictures ------------------------------------------------------------------------------------------- */
/* a tube with what it holds painted into a picture (the token, the thumbnails): tw x th at (ox, oy) */
static inline void sui_paint_tube(uint8_t *o, int W, int H, int ox, int oy, int tw, int th, int balls,
                                  const uint8_t *units, int n, int cork)
{
    float gw = fmaxf(1, roundf(0.075f * tw)), lh = balls ? 0.84f * tw : (th - 0.62f * tw) / 4;
    uint8_t *g = malloc((size_t)tw * th * 4);
    if (!g) return;
    sui_paint_glass(tw, th, gw, 0, g);
    art_blend(o, W, H, g, tw, th, ox, oy);
    if (balls) {
        int d = (int)(0.98f * lh);
        uint8_t *b = malloc((size_t)d * d * 4);
        for (int i = 0; i < n && b; i++) {
            hue_marble(units[i], d, b);
            art_blend(o, W, H, b, d, d, ox + (tw - d) / 2, (int)(oy + th - gw - (i + 0.5f) * lh - d / 2.0f));
        }
        free(b);
    } else {
        sui_paint_liquid(o, W, H, ox, oy, tw, th, gw, lh, units, n);
        int d = (int)fminf(0.62f * lh, 0.58f * (tw - 2 * gw));
        for (int i = 0; i < n; i++)
            hue_mark_on(o, W, H, units[i], ox + tw / 2.0f, oy + th - gw - (i + 0.5f) * lh, d);
    }
    sui_paint_glass(tw, th, gw, 1, g);
    art_blend(o, W, H, g, tw, th, ox, oy);
    free(g);
    if (cork) {
        int cw = (int)(tw - 2 * gw + 0.12f * tw), ch = (int)(0.42f * tw);
        uint8_t *c = calloc((size_t)cw * ch, 4);
        if (c) {
            sui_paint_cork(cw, ch, c);
            art_blend(o, W, H, c, cw, ch, ox + (tw - cw) / 2, (int)(oy + 0.06f * tw - ch / 2.0f));
        }
        free(c);
    }
}

static inline void sui_token(int balls, int size, uint8_t *rgba)   /* a tube of three colours */
{
    static const uint8_t u[4] = {1, 4, 3, 2};
    int th = (int)(0.92f * size), tw = (int)(balls ? th / 4.2f : th / 3.8f);
    sui_paint_tube(rgba, size, size, (size - tw) / 2, (int)(0.06f * size), tw, th, balls, u, 4, 0);
}

static inline void sui_make_small(sui *s)
{
    int small = (int)(0.6f * ui.unit);
    if (s->made_small == small) return;
    s->made_small = small;
    ui_free(&s->t_small);
    memset(ui_buf(small, small), 0, (size_t)small * small * 4);
    sui_token(s->balls, small, ui_buf(small, small));
    s->t_small = ui_tex(small, small, ui_buf(small, small));
}

/* a rack painted into a picture: G's tubes (show: only the first few, close up; 0 all) */
static inline void sui_rack_pic(int balls, const srt_game *G, int show, int size, uint8_t *rgba)
{
    static sui T;
    memset(&T, 0, sizeof T);
    T.balls = balls;
    T.G = *G;
    T.show = show;
    T.bs = (float)size;
    sui_fit(&T, (float)size);
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b) return;
    sui_paint_board(&T, size, b);
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
    int tw = (int)T.tw, th = (int)T.th;
    for (int k = 0; k < (T.show ? T.show : T.G.tubes); k++) {
        uint8_t u[SRT_CAP];
        for (int i = 0; i < SRT_CAP; i++) u[i] = T.G.p.c[k][i];
        sui_paint_tube(rgba, size, size, (int)(sui_slot_x(&T, k) - tw / 2.0f), (int)(T.base_y[sui_row(&T, k)] - th), tw,
                       th, balls, u, T.G.p.h[k], srt_complete(&T.G.p, k));
    }
}

/* the menu's card (variant -1: a small rack half sorted) and the picker's (each size's puzzle, all its tubes) */
static inline void sui_thumb(int balls, sui_new_fn make, int variant, int size, uint8_t *rgba)
{
    static srt_game G;
    make(&G, variant < 0 ? 0 : variant, 11, NULL);
    if (variant < 0) srt_script(&G, "=6");      /* the menu's: four tubes close up, half sorted */
    sui_rack_pic(balls, &G, variant < 0 ? 4 : 0, size, rgba);
}

/* the level screen: a level's rack; a chapter's tab: the rack of its first level, sorted and corked */
static inline void sui_level_thumb(int balls, sui_level_fn level_of, int level, int size, uint8_t *rgba)
{
    static srt_game G;
    if (level_of(&G, level)) sui_rack_pic(balls, &G, 0, size, rgba);
}

static inline void sui_chapter_icon(int balls, sui_level_fn level_of, int first, int size, uint8_t *rgba)
{
    static srt_game G;
    if (!level_of(&G, first)) return;
    srt_pos p;
    memset(&p, 0, sizeof p);
    for (int t = 0; t < G.colours; t++) {
        p.h[t] = SRT_CAP;
        for (int i = 0; i < SRT_CAP; i++) p.c[t][i] = (uint8_t)(t + 1);
    }
    srt_load(&G, G.colours, G.base, balls, &p);
    sui_rack_pic(balls, &G, 0, size, rgba);
}

/* ---- the game for the shell: one state, the shell's calls passed to it ----------------------------------------------- */
#define SUI_GAME(NAME, ID, ORDER, BALLS, MAKE, LEVEL, LEVELS, CHAPTERS)                                             \
    static sui S = {.balls = BALLS, .make = MAKE, .lifted = -1, .made_key = -1, .made_small = -1};                  \
    static void token(int player, int size, uint8_t *rgba) { (void)player; sui_token(BALLS, size, rgba); }        \
    static void thumb(int variant, int size, uint8_t *rgba) { sui_thumb(BALLS, MAKE, variant, size, rgba); }      \
    static void level_thumb(int level, int size, uint8_t *rgba)                                                     \
    {                                                                                                               \
        sui_level_thumb(BALLS, LEVEL, level, size, rgba);                                                           \
    }                                                                                                               \
    static void chapter_icon(int chapter, int size, uint8_t *rgba)                                                  \
    {                                                                                                               \
        sui_chapter_icon(BALLS, LEVEL, chapter * ((LEVELS) / (CHAPTERS)), size, rgba);                              \
    }                                                                                                               \
    static void start(int level) { sui_start_level(&S, LEVEL, level); }                                             \
    static void start_random(int variant) { sui_start(&S, variant); }                                               \
    static int level_stars(void) { return sui_stars(&S); }                                                          \
    static int score(void) { return S.G.moves; }                                                                    \
    static int par(void) { return S.G.par; }                                                                        \
    static void layout(box b) { sui_layout(&S, b); sui_make_small(&S); }                                           \
    static void draw(void) { sui_draw(&S); }                                                                        \
    static void press(float x, float y) { sui_press(&S, x, y); }                                                    \
    static void motion(float x, float y) { sui_motion(&S, x, y); }                                                  \
    static void release(float x, float y) { sui_release(&S, x, y); }                                                \
    static int undo(void) { return sui_undo(&S); }                                                                  \
    static int turn(void) { return 0; }                                                                             \
    static int status(void) { return srt_solved(&S.G) ? GS_WIN0 : GS_PLAY; }                                        \
    static int busy(void) { return sui_busy(&S); }                                                                  \
    static int script(const char *moves) { return sui_script(&S, moves); }                                          \
    static void bar(int player, box b) { (void)player; sui_bar(&S, b); }                                           \
    static int bar_press(int player, float x, float y) { (void)player; return sui_bar_press(&S, x, y); }           \
    static const game NAME = {                                                                                      \
        .id = ID, .order = ORDER, .variants = SRT_VARIANTS,                                                         \
        .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,                             \
        .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,         \
        .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,                     \
        .levels = LEVELS, .chapters = CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,         \
        .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par,                         \
    };                                                                                                              \
    REGISTER_GAME(NAME)

#endif
