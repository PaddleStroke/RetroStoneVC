/*
 * Hex Stacks in RetroStone Board, for one player: a honeycomb board of the house wood, its cells sunk in it, seen a
 * little from the front so the stacks of thin lacquered hex tiles stand up from it; the tray's three stacks below it
 * (on its right on a wide screen) (the rules: hexstack_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag a stack from the tray onto an empty cell (it rides above the finger; the cell it would land on lights, and the
 * neighbours whose top matches glow), or tap a stack (it rises in gold light) and then a cell. The stack drops in
 * with a little bounce, then the chain plays: the matching tiles leap one by one from stack to stack, each turning
 * over in its arc and landing with a click; ten or more of a colour on top burst in gold, the tiles flying up and
 * fading, the count rising over them; a lock whose count is reached springs open. A press while the chain plays
 * plays it out at once. The lamp (the shell's tool): a hint, a stack and its cell in gold light. Undo takes back the
 * last placement. A level's bar: the tiles still to clear (a stack of the goal's colour, gold: any colour), the stacks
 * placed and the par by its flag; endless: the score and the best. The end: a gold wave over the board (solved), or
 * the stacks dimming row by row (the board full).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/hexstack.h"
#include "games/hues.h"
#include "games/puzrand.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define SQ 0.80f                                /* the board seen from the front: its depth squashed */
#define ROW (0.8660254f * SQ)                   /* a row's step, of a cell's width */
#define TILE 0.86f                              /* a tile's width, of a cell's */
#define THICK 0.085f                            /* a tile's thickness on the screen, of a cell's width */
#define TRAY_K 0.8f                             /* a tray stack's size, of the board's */
#define DROP_MS 150
#define FLY_MS 300
#define POP_MS 420
#define UNLOCK_MS 420
#define DEAL_MS 260
#define BACK_MS 170
#define LIFT_MS 110
#define WIN_MS 1300
#define END_MS 1100
#define HINT_MS 3000
#define HINT_TOOL 0

static const int HUE[HX_COLOURS] = {1, 4, 3, 2, 7, 5};   /* red lacquer, lapis, amber, jade, plum, ivory (hues.h) */

static hx_game G;
static int best[HX_VARIANTS], best_read;

static box last_box;
static int below_board;                         /* the tray under the board (else on its right) */
static float W, cx0, cy0, bx, by;               /* a cell's width; cell (0, 0)'s centre; the board picture's top left */
static int bw, bh;                              /* the board picture's size */
static box slot_box[HX_TRAY];
static int made_w = -1, made_small = -1, made_lock_serial = -1, serial;
static SDL_Texture *t_board, *t_tile[2][HX_COLOURS], *t_top[2][HX_COLOURS], *t_shadow[2], *t_lock, *t_mat;
static SDL_Texture *t_goal, *t_placed, *t_tok, *t_star, *t_par;

static int sel = -1;                            /* the tray stack chosen by a tap */
static struct { int on, moving, slot; float x, y, x0, y0; Uint32 t0; } drag;
static struct { int on, slot, cell; Uint32 t0; } hint;
static struct { int on, slot; float x, y, k; Uint32 t0; } back;
static struct { int on; Uint32 t0; } deal_fx;
static struct {                                 /* the last placement's drop and chain */
    int on, cell, nev;
    Uint32 t0, drop_end, start[HX_EVENTS], len[HX_EVENTS], end;
    hx_pos from;                                /* the position as the drop lands, the chain to come */
    hx_event ev[HX_EVENTS];
    float sx, sy, sk;                           /* where the stack dropped from (its foot), its size */
} A;
static struct { int on, won; Uint32 t0; } endfx;
static int tool_press;                          /* the press was on the tool: its release is nobody's */

/* ---- the shapes ---------------------------------------------------------------------------------------------- */
/* a pointy-topped hexagon a across its flats (squashed in depth): how far (dx, dy) is outside it (< 0 inside) */
static float hex_sdf(float dx, float dy, float a)
{
    dx = fabsf(dx);
    dy = fabsf(dy / SQ);
    return fmaxf(dx, 0.5f * dx + 0.8660254f * dy) - a / 2;
}

/* the column's top and bottom edge of such a hexagon at dx (its centre 0): -1 outside */
static int hex_span(float dx, float a, float *top, float *bot)
{
    *top = *bot = 0;
    float h = a / 2;
    if (fabsf(dx) > h) return 0;
    float y = (2 * h / 1.7320508f - fabsf(dx) / 1.7320508f) * SQ;
    *top = -y;
    *bot = y;
    return 1;
}

static void cell_xy(int c, float *x, float *y)  /* a cell's centre on the screen */
{
    int col = c % HX_COLS, row = c / HX_COLS;
    *x = cx0 + (col + 0.5f * (row & 1)) * W;
    *y = cy0 + row * ROW * W;
}

/* ---- the pictures -------------------------------------------------------------------------------------------- */
static void colour_of(int col, uint8_t rgb[3]) { hue_rgb(HUE[col < 0 || col >= HX_COLOURS ? 0 : col], rgb); }

/* one tile, tw across, its face and its edge below it th thick, into a picture tw + 2 wide, face + th + 2 high; its
 * face lit from the top left, a bevel round it, maybe the colour's mark (top: the stack's top tile) */
static void tile_size(float tw, int *pw, int *ph)
{
    float t, b;
    hex_span(0, tw, &t, &b);
    *pw = (int)ceilf(tw) + 2;
    *ph = (int)ceilf(b - t + THICK / TILE * tw) + 2;
}

static void paint_tile(int col, float tw, int mark, int gold, uint8_t *o)
{
    int pw, ph;
    tile_size(tw, &pw, &ph);
    memset(o, 0, (size_t)pw * ph * 4);
    float th = THICK / TILE * tw, top0, bot0;
    hex_span(0, tw, &top0, &bot0);
    float cx = pw / 2.0f, cy = 1 - top0;        /* the face's centre */
    uint8_t base[3];
    colour_of(col, base);
    if (gold) base[0] = 226, base[1] = 178, base[2] = 72;
    for (int y = 0; y < ph; y++)
        for (int x = 0; x < pw; x++) {
            float face = 0, side = 0, shade = 0, lit = 0;
            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++) {
                    float px = x + (i + 0.5f) / 4 - cx, py = y + (j + 0.5f) / 4 - cy, t, b;
                    if (!hex_span(px, tw, &t, &b)) continue;
                    if (py >= t && py <= b) {
                        face += 1.0f / 16;
                        float d = -hex_sdf(px, py, tw);   /* inside: how far from the edge */
                        float bev = fmaxf(1.2f, 0.07f * tw);
                        if (d < bev) {          /* the bevel: lit at the top left, shaded at the bottom right */
                            float nx = px / (tw / 2), ny = py / (bot0 + 0.001f);
                            lit += (-(nx + ny) * 0.55f) * (1 - d / bev) / 16;
                        }
                        lit += (0.10f - 0.16f * (px / tw + py / (2 * bot0))) / 16;
                        float hx = px / tw + 0.18f, hy = py / (2 * bot0) + 0.2f;   /* a soft sheen */
                        lit += 0.16f * expf(-(hx * hx + hy * hy) * 18) / 16;
                    } else if (py > b && py <= b + th) {
                        side += 1.0f / 16;
                        float f = (py - b) / th;
                        shade += (px < 0 ? 0.72f : 0.56f) * (f > 0.78f ? 0.45f : f < 0.22f ? 1.18f : 1) / 16;
                    }
                }
            float a = face + side;
            if (a <= 0) continue;
            uint8_t *p = o + 4 * ((size_t)y * pw + x);
            float kf = face > 0 ? 1 + lit / face : 1, ks = side > 0 ? shade / side : 1;
            for (int i = 0; i < 3; i++) {
                float v = (base[i] * kf * face + base[i] * ks * side) / a;
                p[i] = (uint8_t)clampf(v, 0, 255);
            }
            p[3] = (uint8_t)clampf(255 * a, 0, 255);
        }
    if (mark && !gold) {                        /* the colour's mark, pressed into the face (squashed with it) */
        int d = (int)(0.36f * tw);
        if (d >= 6) {
            int hue = HUE[col];
            uint8_t *m = malloc((size_t)d * d * 4), tone[3];
            float a;
            if (m) {
                hue_mark(hue, d, m);
                hue_mark_tone(hue, tone, &a);
                int mh = (int)(d * SQ);
                for (int y = 0; y < mh; y++)
                    for (int x = 0; x < d; x++) {
                        const uint8_t *s = m + 4 * ((size_t)(y * d / mh) * d + x);
                        int px = (int)(cx - d / 2.0f) + x, py = (int)(cy - mh / 2.0f) + y;
                        if (px < 0 || py < 0 || px >= pw || py >= ph || !s[3]) continue;
                        pzk_over(o + 4 * ((size_t)py * pw + px), tone, 0.42f * a * s[3] / 255.0f);
                    }
                free(m);
            }
        }
    }
}

/* the board: the plank cut round its cells (each cell sunk in it, a stone raised where blocked), its rim of the
 * frame's wood; into o (pw x ph), cell (0, 0)'s centre at (ox, oy), cells w wide */
static void paint_board(const hx_pos *p, float w, float ox, float oy, int pw, int ph, uint8_t *o)
{
    int cells[HX_CELLS], n = 0;
    float cxs[HX_CELLS], cys[HX_CELLS];
    for (int c = 0; c < HX_CELLS; c++)
        if (p->kind[c] != HX_VOID) {
            cells[n] = c;
            cxs[n] = ox + (c % HX_COLS + 0.5f * (c / HX_COLS & 1)) * w;
            cys[n] = oy + c / HX_COLS * ROW * w;
            n++;
        }
    art_plank(pw, ph, 0, o);
    int light = art_frame_is_light();
    static const uint8_t walnut[3] = {92, 60, 40}, maple[3] = {226, 204, 168}, slate[3] = {112, 106, 98};
    const uint8_t *rim = light ? maple : walnut;
    float grow = 0.30f * w, rw = fmaxf(2, 0.12f * w), well = 0.88f * w, wall = fmaxf(1.5f, 0.08f * w);
    for (int y = 0; y < ph; y++)
        for (int x = 0; x < pw; x++) {
            float px = x + 0.5f, py = y + 0.5f, d = 1e9f;
            int near = -1;
            for (int i = 0; i < n; i++) {
                float dd = hex_sdf(px - cxs[i], py - cys[i], w);
                if (dd < d) d = dd, near = i;
            }
            uint8_t *q = o + 4 * ((size_t)y * pw + x);
            float edge = grow - d;              /* inside the board's outline by this much */
            if (edge <= -0.5f || near < 0) { q[3] = 0; continue; }
            q[3] = (uint8_t)(255 * clampf(edge + 0.5f, 0, 1));
            if (edge < rw) {                    /* the rim, rounded by the light */
                float f = edge / rw, k = 0.86f + 0.3f * sinf(ART_PI * f) - 0.12f * (py / ph);
                float g = 1 + 0.06f * art_noise(x * 2, y);
                for (int i = 0; i < 3; i++) q[i] = (uint8_t)clampf(rim[i] * k * g, 0, 255);
                continue;
            }
            int c = cells[near];
            float dx = px - cxs[near], dy = py - cys[near], dw = hex_sdf(dx, dy, well);
            float nx = dx / (0.5f * w), ny = dy / (0.5f * w * SQ);
            if (p->kind[c] == HX_BLOCK) {       /* a stone: its face raised, its front edge below it */
                float lift = 0.13f * w, df = hex_sdf(dx, dy + lift, well * 0.96f), t, b;
                int col = hex_span(dx, well * 0.96f, &t, &b);
                if (df < 0.5f) {
                    float a = clampf(0.5f - df, 0, 1), k = 1.08f - 0.22f * (nx + ny) * 0.5f;
                    if (-df < wall) k += 0.22f * (-(nx + ny)) * (1 - -df / wall);
                    k *= 1 + 0.08f * art_noise(x * 3 + 11, y * 2);
                    for (int i = 0; i < 3; i++) q[i] = (uint8_t)clampf(q[i] + (slate[i] * k - q[i]) * a, 0, 255);
                } else if (col && dy + lift > b && dy + lift - b <= lift && dy < b + 1) {
                    float k = dx < 0 ? 0.62f : 0.5f;
                    for (int i = 0; i < 3; i++) q[i] = (uint8_t)clampf(slate[i] * k, 0, 255);
                } else if (dw < wall) {         /* its shadow round it */
                    for (int i = 0; i < 3; i++) q[i] = (uint8_t)(q[i] * 0.78f);
                }
                continue;
            }
            if (dw < 0) {                       /* the sunken cell: darker, its walls lit across from the light */
                float k = 0.66f;
                if (-dw < wall) k += 0.26f * (nx + ny) * 0.5f * (1 - -dw / wall) + 0.05f;
                else k -= 0.05f * (nx + ny) * 0.3f;
                for (int i = 0; i < 3; i++) q[i] = (uint8_t)clampf(q[i] * k, 0, 255);
            } else if (dw < 1.2f) {
                float k = 1 + 0.18f * (1 - dw / 1.2f) * (-(nx + ny) * 0.5f + 0.4f);
                for (int i = 0; i < 3; i++) q[i] = (uint8_t)clampf(q[i] * k, 0, 255);
            }
        }
}

/* a soft shadow under a stack (an oval as wide as a tile), white: tinted black when drawn */
static void paint_shadow(int s, uint8_t *o)
{
    int h = (int)(s * SQ);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < s; x++) {
            float dx = (x + 0.5f) / s * 2 - 1, dy = (y + 0.5f) / h * 2 - 1, d = sqrtf(dx * dx + dy * dy);
            uint8_t *p = o + 4 * ((size_t)y * s + x);
            p[0] = p[1] = p[2] = 0;
            p[3] = (uint8_t)(255 * clampf(1 - d, 0, 1) * clampf(1 - d, 0, 1));
        }
}

/* a lock's brass plate over a sunk cell: a hexagon of brass, a padlock struck in it (its count drawn over it) */
static void paint_lock(int s, uint8_t *o)
{
    int h = (int)(s * SQ * 1.1547f) + 2;
    memset(o, 0, (size_t)s * h * 4);
    static const uint8_t brass[3] = {214, 168, 80};
    float cx = s / 2.0f, cy = h / 2.0f, a = 0.86f * s;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < s; x++) {
            float cov = 0, k = 0;
            for (int j = 0; j < 3; j++)
                for (int i = 0; i < 3; i++) {
                    float dx = x + (i + 0.5f) / 3 - cx, dy = y + (j + 0.5f) / 3 - cy, d = hex_sdf(dx, dy, a);
                    if (d > 0) continue;
                    cov += 1.0f / 9;
                    float nx = dx / (a / 2), ny = dy / (a / 2 * SQ), kk = 1.05f - 0.2f * (nx + ny) * 0.5f;
                    if (-d < 0.08f * s) kk += 0.3f * (-(nx + ny) * 0.5f) * (1 - -d / (0.08f * s));
                    k += kk / 9;
                }
            if (cov <= 0) continue;
            uint8_t *p = o + 4 * ((size_t)y * s + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(brass[i] * k / cov * (1 + 0.04f * art_noise(x, y * 7)), 0, 255);
            p[3] = (uint8_t)(255 * cov);
        }
}

/* a stack's picture painted into a picture (a thumbnail): its tiles from (cx, cy) up (cells w wide) */
static void put_stack(uint8_t *o, int pw, int ph, const uint8_t *t, int n, float cx, float cy, float w, int gold)
{
    float tw = TILE * w;
    int sw, sh;
    tile_size(tw, &sw, &sh);
    uint8_t *s = malloc((size_t)sw * sh * 4);
    if (!s) return;
    int painted = -1;
    float top, bot;
    hex_span(0, tw, &top, &bot);
    for (int i = 0; i < n; i++) {
        int key = t[i] * 2 + (i == n - 1);
        if (key != painted) paint_tile(t[i], tw, i == n - 1, gold, s), painted = key;
        art_blend(o, pw, ph, s, sw, sh, (int)lroundf(cx - sw / 2.0f), (int)lroundf(cy + top - 1 - i * THICK * w));
    }
    free(s);
}

/* a position painted (its board and its stacks) into a square picture */
static void paint_pos(const hx_pos *p, int size, uint8_t *rgba, int extra_tiles)
{
    int x2lo = 99, x2hi = -1, rlo = 99, rhi = -1;
    for (int c = 0; c < HX_CELLS; c++)
        if (p->kind[c] != HX_VOID) {
            int r = c / HX_COLS, x2 = 2 * (c % HX_COLS) + (r & 1);
            if (x2 < x2lo) x2lo = x2;
            if (x2 > x2hi) x2hi = x2;
            if (r < rlo) rlo = r;
            if (r > rhi) rhi = r;
        }
    if (x2hi < 0) return;
    float cw = (x2hi - x2lo) / 2.0f + 1 + 0.6f, ch = (rhi - rlo) * ROW + 1.1547f * SQ + 0.6f + 0.25f * extra_tiles * THICK;
    float w = size / fmaxf(cw, ch);
    float ox = (size - ((x2hi - x2lo) / 2.0f) * w) / 2 - x2lo / 2.0f * w;
    float oy = (size - (rhi - rlo) * ROW * w) / 2 + 0.25f * extra_tiles * THICK * w * 0.5f - rlo * ROW * w;
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b) return;
    paint_board(p, w, ox, oy, size, size, b);
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
    for (int c = 0; c < HX_CELLS; c++)
        if (p->n[c]) {
            float x = ox + (c % HX_COLS + 0.5f * (c / HX_COLS & 1)) * w, y = oy + c / HX_COLS * ROW * w;
            put_stack(rgba, size, size, p->t[c], p->n[c], x, y, w, 0);
        }
}

static void paint_token(int size, uint8_t *o)   /* a stack of three bands */
{
    memset(o, 0, (size_t)size * size * 4);
    static const uint8_t t[8] = {3, 3, 1, 1, 1, 0, 0, 0};
    float w = 0.84f * size;
    put_stack(o, size, size, t, 8, size / 2.0f, 0.62f * size, w, 0);
}

static void paint_goal(int col, int size, uint8_t *o)   /* the goal's stack: its colour, or gold (any colour) */
{
    memset(o, 0, (size_t)size * size * 4);
    uint8_t t[7];
    for (int i = 0; i < 7; i++) t[i] = (uint8_t)(col < 0 ? 0 : col);
    put_stack(o, size, size, t, 7, size / 2.0f, 0.64f * size, 0.82f * size, col < 0);
}

static void paint_placed(int size, uint8_t *o)  /* a stack set down on a sunk cell */
{
    memset(o, 0, (size_t)size * size * 4);
    static const uint8_t t[4] = {1, 1, 2, 2};
    float w = 0.78f * size;
    put_stack(o, size, size, t, 4, size / 2.0f, 0.6f * size, w, 0);
}

static void paint_star(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    static const uint8_t gold[3] = {240, 196, 80}, ink[3] = {120, 80, 20};
    art_fig *f = art_fig_new();
    art_star(f, ART_PART, 0.5f, 0.54f, 0.46f);
    art_fig_paint(o, size, f, gold, ink, 0.05f, 0.5f);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    paint_token(size, rgba);
}

static SDL_Texture *tex_paint(int w, int h, void (*paint)(int, uint8_t *), int size)
{
    uint8_t *b = ui_buf(w, h);
    paint(size, b);
    return ui_tex(w, h, b);
}

/* ---- the layout ---------------------------------------------------------------------------------------------- */
static void extent(int *x2lo, int *x2hi, int *rlo, int *rhi)
{
    *x2lo = 99, *x2hi = -1, *rlo = 99, *rhi = -1;
    for (int c = 0; c < HX_CELLS; c++)
        if (G.p.kind[c] != HX_VOID || A.from.kind[c] != HX_VOID) {
            int r = c / HX_COLS, x2 = 2 * (c % HX_COLS) + (r & 1);
            if (x2 < *x2lo) *x2lo = x2;
            if (x2 > *x2hi) *x2hi = x2;
            if (r < *rlo) *rlo = r;
            if (r > *rhi) *rhi = r;
        }
}

static void layout(box b)
{
    last_box = b;
    int x2lo, x2hi, rlo, rhi;
    extent(&x2lo, &x2hi, &rlo, &rhi);
    if (x2hi < 0) return;
    float cw = (x2hi - x2lo) / 2.0f + 1, ch = (rhi - rlo) * ROW + 1.1547f * SQ, pad = 0.75f, head = 1.0f;
    float tray_v = 2.1f, tray_h = 2.5f;
    float wv = fminf(b.w / (cw + pad), b.h / (ch + pad + head + tray_v));
    float wh = fminf(b.w / (cw + pad + tray_h), b.h / (ch + pad + head));
    below_board = wv >= wh;
    W = floorf(fminf(below_board ? wv : wh, 0.22f * fminf((float)ui.w, (float)ui.h)));
    if (W < 8) W = 8;
    float boardw = (cw + pad) * W, boardh = (ch + pad) * W;
    float totw = below_board ? boardw : boardw + tray_h * W, toth = below_board ? boardh + head * W + tray_v * W
                                                                             : boardh + head * W;
    float x0 = b.x + (b.w - totw) / 2, y0 = b.y + (b.h - toth) / 2 + head * W;
    bx = floorf(x0);
    by = floorf(y0);
    bw = (int)boardw;
    bh = (int)boardh;
    float ox = pad / 2 * W + 0.5f * W - x2lo / 2.0f * W, oy = pad / 2 * W + 0.5774f * SQ * W - rlo * ROW * W;
    cx0 = bx + ox;
    cy0 = by + oy;
    for (int i = 0; i < HX_TRAY; i++) {
        if (below_board) slot_box[i] = (box){bx + i * boardw / 3, by + boardh, boardw / 3, tray_v * W};
        else slot_box[i] = (box){bx + boardw, by + i * boardh / 3 - 0.3f * W, tray_h * W, boardh / 3 + 0.3f * W};
    }
    if (!below_board) {
        float th = fminf(boardh + head * W, 3 * 2.1f * W), t0 = by - head * W + (boardh + head * W - th) / 2 + 0.6f * W;
        if (t0 + th > b.y + b.h) t0 = b.y + b.h - th;   /* the last stack kept on the screen (a board as tall as it) */
        for (int i = 0; i < HX_TRAY; i++) slot_box[i] = (box){bx + boardw, t0 + i * th / 3, tray_h * W, th / 3};
    }
    int key = (int)W;
    if (made_w != key || made_lock_serial != serial) {
        made_w = key;
        made_lock_serial = serial;
        ui_free(&t_board);
        uint8_t *o = malloc((size_t)bw * bh * 4);
        if (o) {
            hx_pos shape = G.p;                 /* (the board as at the start: locks drawn sunk, plates over them) */
            for (int c = 0; c < HX_CELLS; c++)
                if (shape.kind[c] == HX_LOCK) shape.kind[c] = HX_OPEN;
            paint_board(&shape, W, ox, oy, bw, bh, o);
            t_board = ui_tex(bw, bh, o);
            free(o);
        }
        for (int k = 0; k < 2; k++) {
            float tw = TILE * W * (k ? TRAY_K : 1);
            int pw, ph;
            tile_size(tw, &pw, &ph);
            for (int c = 0; c < HX_COLOURS; c++) {
                ui_free(&t_tile[k][c]);
                ui_free(&t_top[k][c]);
                paint_tile(c, tw, 0, 0, ui_buf(pw, ph));
                t_tile[k][c] = ui_tex(pw, ph, ui_buf(pw, ph));
                paint_tile(c, tw, 1, 0, ui_buf(pw, ph));
                t_top[k][c] = ui_tex(pw, ph, ui_buf(pw, ph));
            }
            int s = (int)(1.15f * tw);
            ui_free(&t_shadow[k]);
            paint_shadow(s, ui_buf(s, (int)(s * SQ)));
            t_shadow[k] = ui_tex(s, (int)(s * SQ), ui_buf(s, (int)(s * SQ)));
        }
        int s = (int)(0.9f * W), lh = (int)(s * SQ * 1.1547f) + 2;
        ui_free(&t_lock);
        paint_lock(s, ui_buf(s, lh));
        t_lock = ui_tex(s, lh, ui_buf(s, lh));
        ui_free(&t_mat);                        /* a tray place: a faint hexagon on the table */
        int m = (int)(TRAY_K * W * 1.05f), mh = (int)(m * SQ * 1.1547f) + 2;
        uint8_t *mb = ui_buf(m, mh);
        memset(mb, 0, (size_t)m * mh * 4);
        for (int y = 0; y < mh; y++)
            for (int x = 0; x < m; x++) {
                float d = hex_sdf(x + 0.5f - m / 2.0f, y + 0.5f - mh / 2.0f, m - 2.0f);
                float a = clampf(0.5f - fabsf(d + 1.2f) + 1.0f, 0, 1) * 0.9f + (d < 0 ? 0.25f : 0);
                uint8_t *p = mb + 4 * ((size_t)y * m + x);
                p[0] = p[1] = p[2] = 255;
                p[3] = (uint8_t)(255 * clampf(a, 0, 1));
            }
        t_mat = ui_tex(m, mh, mb);
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small && small > 3) {
        made_small = small;
        ui_free(&t_tok);
        t_tok = tex_paint(small, small, paint_token, small);
        ui_free(&t_placed);
        t_placed = tex_paint(small, small, paint_placed, small);
        ui_free(&t_star);
        t_star = tex_paint(small, small, paint_star, small);
        ui_free(&t_par);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
        ui_free(&t_goal);
    }
    if (!t_goal && small > 3) {
        paint_goal(G.goal == HX_COLOUR ? G.colour : -1, small, ui_buf(small, small));
        t_goal = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position -------------------------------------------------------------------------------------------- */
static void calm(void)
{
    sel = -1;
    drag.on = hint.on = back.on = deal_fx.on = A.on = endfx.on = 0;
}

static void read_best(void)
{
    if (best_read) return;
    best_read = 1;
    for (int v = 0; v < HX_VARIANTS; v++) {
        char key[32];
        uint8_t b[4];
        snprintf(key, sizeof key, "hexstack/best%d", v);
        if (ui_save_get(key, b, 4) == 4) best[v] = b[0] | b[1] << 8 | b[2] << 16 | (b[3] & 0x7f) << 24;
    }
}

static void keep_best(void)
{
    if (G.goal != HX_ENDLESS) return;
    int v = G.variant, s = hx_score(&G);
    if (v < 0 || v >= HX_VARIANTS || s <= best[v]) return;
    best[v] = s;
    char key[32];
    uint8_t b[4] = {(uint8_t)s, (uint8_t)(s >> 8), (uint8_t)(s >> 16), (uint8_t)(s >> 24)};
    snprintf(key, sizeof key, "hexstack/best%d", v);
    ui_save_set(key, b, 4);
}

static void begin(void)
{
    calm();
    serial++;
    ui_free(&t_goal);
    A.from = G.p;
    deal_fx.on = 1;
    deal_fx.t0 = ui_ms();
    if (last_box.w > 0) layout(last_box);
    ui_animate();
}

static void start(int level)
{
    if (!hx_level(&G, level)) hx_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    read_best();
    hx_endless(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void)
{
    if (G.goal == HX_ENDLESS) return hx_over(&G) ? GS_WIN0 : GS_PLAY;
    return hx_solved(&G) ? GS_WIN0 : hx_over(&G) ? GS_WIN1 : GS_PLAY;
}
static int turn(void) { return 0; }
static int score(void) { return G.goal == HX_ENDLESS ? hx_score(&G) : G.p.placed; }
static int par(void) { return G.goal == HX_ENDLESS ? 0 : G.par; }
static int level_stars(void) { return G.goal == HX_ENDLESS ? hx_stars(&G) : ui_par_stars(G.p.placed, G.par); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (A.on && (int)(now - A.end) < 0) || (endfx.on && (int)(now - endfx.t0) < END_MS) || drag.on;
}

/* the chain's timing: each step after the last; a band's tiles one after another */
static Uint32 stagger(int n) { int s = 78 - 3 * n; return (Uint32)(s < 38 ? 38 : s); }

static Uint32 event_len(const hx_event *e)
{
    if (e->kind == HX_EV_MOVE) return (e->count - 1) * stagger(e->count) + FLY_MS + 30;
    if (e->kind == HX_EV_CLEAR) return (Uint32)(e->count * 28 + POP_MS);
    return UNLOCK_MS;
}

static void ended(void)
{
    if (!hx_over(&G)) return;
    endfx.on = 1;
    endfx.won = hx_solved(&G);
    endfx.t0 = A.on ? A.end : ui_ms();
}

/* a placement made (the rules already played it): its drop from (sx, sy) at size sk, then its chain */
static void show_placement(int slot, float sx, float sy, float sk)
{
    Uint32 now = ui_ms();
    A.on = 1;
    A.cell = G.last_cell;
    A.from = G.prev;
    A.from.n[A.cell] = G.prev.tray[slot].n;
    memcpy(A.from.t[A.cell], G.prev.tray[slot].t, G.prev.tray[slot].n);
    A.from.tray[slot].n = 0;
    A.nev = G.nev;
    memcpy(A.ev, G.ev, sizeof(hx_event) * (size_t)G.nev);
    A.t0 = now;
    A.drop_end = now + DROP_MS;
    A.sx = sx, A.sy = sy, A.sk = sk;
    Uint32 t = A.drop_end + 60;
    int clicks = 0;
    for (int k = 0; k < A.nev; k++) {
        A.start[k] = t;
        A.len[k] = event_len(&A.ev[k]);
        const hx_event *e = &A.ev[k];
        if (e->kind == HX_EV_MOVE)
            for (int i = 0; i < e->count && clicks < 40; i++, clicks++)
                snd_play_at(SND_PLACE, 0.32f, (int)(t - now + i * stagger(e->count) + FLY_MS));
        else if (e->kind == HX_EV_CLEAR) snd_play_at(e->count >= 14 ? SND_END : SND_CAPTURE, 0.9f, (int)(t - now + 60));
        else snd_play_at(SND_TAP, 0.8f, (int)(t - now));
        t += A.len[k] + (e->kind == HX_EV_CLEAR ? 60 : 0);
    }
    A.end = t;
    ui_sound_later(SND_DROP, DROP_MS - 30);
    if (G.prev.tray[(slot + 1) % 3].n == 0 && G.prev.tray[(slot + 2) % 3].n == 0) {
        deal_fx.on = 1;                         /* the tray dealt again */
        deal_fx.t0 = now + 80;
    }
    sel = -1;
    hint.on = 0;
    keep_best();
    ended();
    ui_animate();
}

static void lay(int slot, int cell, float sx, float sy, float sk)
{
    if (!hx_place(&G, slot, cell)) { ui_sound(SND_DENY); return; }
    show_placement(slot, sx, sy, sk);
}

static int undo(void)
{
    calm();
    int ok = hx_undo(&G);
    A.from = G.p;
    return ok;
}

static void slot_mid(int i, float *x, float *y)   /* a tray stack's foot */
{
    *x = slot_box[i].x + slot_box[i].w / 2;
    *y = slot_box[i].y + slot_box[i].h * 0.68f;
}

/* the moves at once, the last one laid as a finger would (its chain to see: the pictures' --at) */
static int script(const char *moves)
{
    calm();
    int ok = hx_script(&G, moves);
    if (ok && G.has_prev && G.last_cell >= 0) {
        float sx, sy;
        slot_mid(G.last_slot, &sx, &sy);
        show_placement(G.last_slot, sx, sy, TRAY_K);
        return 1;
    }
    A.from = G.p;
    keep_best();
    ended();
    return ok;
}

/* ---- the chain as it plays ----------------------------------------------------------------------------------- */
static void apply(hx_pos *p, const hx_event *e)
{
    if (e->kind == HX_EV_MOVE) {
        for (int i = 0; i < e->count; i++) p->t[e->b][p->n[e->b]++] = e->colour;
        p->n[e->a] = (uint8_t)(p->n[e->a] - e->count);
    } else if (e->kind == HX_EV_CLEAR) {
        p->n[e->a] = (uint8_t)(p->n[e->a] - e->count);
        p->cleared = (uint16_t)(p->cleared + e->count);
        p->by_colour[e->colour] = (uint16_t)(p->by_colour[e->colour] + e->count);
    } else p->kind[e->a] = HX_OPEN;
}

/* the position shown at `now`: the events done applied; the one going on (-1 none) */
static int shown(Uint32 now, hx_pos *p)
{
    if (!A.on) { *p = G.p; return -1; }
    *p = A.from;
    for (int k = 0; k < A.nev; k++) {
        if ((int)(now - A.start[k]) < 0) return -1;
        if ((int)(now - (A.start[k] + A.len[k])) < 0) return k;
        apply(p, &A.ev[k]);
    }
    return -1;
}

/* ---- drawing ------------------------------------------------------------------------------------------------- */
static void tile_at(int size, int col, int top, float cx, float foot, int i, float k, Uint8 light, Uint8 alpha)
{
    SDL_Texture *t = (top ? t_top : t_tile)[size][col];
    if (!t) return;
    float made = TILE * W * (size ? TRAY_K : 1), sc = TILE * W * k / made, t0, b0;
    int pw, ph;
    tile_size(made, &pw, &ph);
    hex_span(0, made, &t0, &b0);
    SDL_SetTextureColorMod(t, light, light, light);
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put(t, cx - pw * sc / 2, foot - i * THICK * W * k - (1 - t0) * sc, pw * sc, ph * sc);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

/* a stack standing with its foot's centre at (cx, foot): k its size (1 the board's), lifted (its shadow apart) */
static void stack_at(const uint8_t *t, int n, float cx, float foot, float k, float lift, Uint8 light, Uint8 alpha,
                     int tray)
{
    if (n <= 0) return;
    int sz = tray && fabsf(k - TRAY_K) < 0.01f ? 1 : 0;
    SDL_Texture *sh = t_shadow[sz];
    if (sh) {
        float s = 1.15f * TILE * W * k;
        SDL_SetTextureAlphaMod(sh, (Uint8)(alpha * (lift > 0 ? 0.32f : 0.42f)));
        ui_put_c(sh, cx + 0.06f * W * k + 0.25f * lift, foot + 0.07f * W * k + 0.35f * lift, s * (1 + 0.15f * lift / W),
                 s * SQ * (1 + 0.15f * lift / W));
        SDL_SetTextureAlphaMod(sh, 255);
    }
    for (int i = 0; i < n; i++) tile_at(sz, t[i], i == n - 1, cx, foot - lift, i, k, light, alpha);
}

/* a tile in flight from (ax, ay) to (bx, by) (its face's centre), f of the way, turning over: its face (the tile's own
 * picture) mapped on a hexagon turned in space, lit by how it faces the light, its edge a tile's thickness under it */
static void flying_tile(int col, float ax, float ay, float bx_, float by_, float f, float k)
{
    float e = puz_ease(f), x = ax + (bx_ - ax) * e, y = ay + (by_ - ay) * e;
    float dist = hypotf(bx_ - ax, by_ - ay);
    y -= (0.45f * W + 0.35f * dist) * sinf(PUZ_PI * f);
    float dx = bx_ - ax, dy = (by_ - ay) / SQ, dl = hypotf(dx, dy);
    if (dl < 1e-3f) dx = 1, dy = 0, dl = 1;
    float ux = -dy / dl, uy = dx / dl;           /* the axis it turns round: across its way, in the board's plane */
    float th = PUZ_PI * e, c = cosf(th), s = sinf(th), tw = TILE * W, r = tw * k / 1.7320508f;
    SDL_Texture *tex = t_top[0][col];
    int pw, ph;
    float t0, b0;
    tile_size(tw, &pw, &ph);
    hex_span(0, tw, &t0, &b0);
    float fcx = pw / 2.0f, fcy = 1 - t0, rr = tw / 1.7320508f;   /* the face in the picture */
    uint8_t base[3];
    colour_of(col, base);
    float lit = 0.62f + 0.38f * fabsf(c) + 0.12f * s * (ux + uy);   /* edge on: darker */
    SDL_Vertex v[2][7];
    for (int face = 0; face < 2; face++) {
        float off = face ? 0 : THICK * W * k * (c >= 0 ? 1 : -1);   /* the edge first, under the face */
        for (int i = 0; i < 6; i++) {
            float a = (90 + 60 * i) * PUZ_PI / 180, px = r * cosf(a), py = r * sinf(a);
            float dot = px * ux + py * uy;       /* turned round the axis by th (Rodrigues; it lies in the plane) */
            float qx = px * c + ux * dot * (1 - c), qy = py * c + uy * dot * (1 - c), qz = (ux * py - uy * px) * s;
            v[face][i + 1].position = (SDL_FPoint){x + qx, y + qy * SQ - qz * 0.6f + off};
            float l = face ? lit : 0.42f;
            Uint8 g = (Uint8)clampf(255 * l, 0, 255);
            if (face) v[face][i + 1].color = (SDL_Color){g, g, g, 255};
            else v[face][i + 1].color = (SDL_Color){(Uint8)(base[0] * l), (Uint8)(base[1] * l), (Uint8)(base[2] * l), 255};
            v[face][i + 1].tex_coord = (SDL_FPoint){(fcx + rr * cosf(a)) / pw, (fcy + rr * sinf(a) * SQ) / ph};
        }
        float mx = 0, my = 0;
        for (int i = 1; i <= 6; i++) mx += v[face][i].position.x / 6, my += v[face][i].position.y / 6;
        v[face][0] = v[face][1];
        v[face][0].position = (SDL_FPoint){mx, my};
        v[face][0].tex_coord = (SDL_FPoint){fcx / pw, fcy / ph};
        int idx[18];
        for (int i = 0; i < 6; i++) idx[3 * i] = 0, idx[3 * i + 1] = 1 + i, idx[3 * i + 2] = 1 + (i + 1) % 6;
        SDL_RenderGeometry(ui.r, face ? tex : NULL, v[face], 7, idx, 18);
    }
}

static void draw_board_marks(const hx_pos *p, Uint32 now)
{
    for (int c = 0; c < HX_CELLS; c++) {        /* the locks' plates and their counts */
        if (p->kind[c] != HX_LOCK && !(A.on && A.from.kind[c] == HX_LOCK)) continue;
        float x, y;
        cell_xy(c, &x, &y);
        float s = 0.9f * W, h = s * SQ * 1.1547f + 2, k = 1, a = 1;
        if (p->kind[c] != HX_LOCK) {            /* opening: it lifts away and fades */
            float f = 1;
            for (int e = 0; e < A.nev; e++)
                if (A.ev[e].kind == HX_EV_UNLOCK && A.ev[e].a == c) f = clampf((now - A.start[e]) / (float)UNLOCK_MS, 0, 1);
            if (f >= 1) continue;
            k = 1 + 0.35f * f;
            a = 1 - f;
            y -= 0.3f * W * f;
            ui_mark_gold_a(x, y, 1.6f * W, 0.8f * (1 - f));
        }
        SDL_SetTextureAlphaMod(t_lock, (Uint8)(255 * a));
        ui_put_c(t_lock, x, y, s * k, h * k);
        SDL_SetTextureAlphaMod(t_lock, 255);
        int left = p->need[c] * 10 - p->cleared;
        if (left < 1) left = 1;
        ui_number_ex(x, y + 0.02f * W, 0.36f * W * k, left, PUZ_INK, (Uint8)(235 * a), 0);
    }
}

static void draw(void)
{
    if (made_w < 0) layout(last_box);
    Uint32 now = ui_ms();
    hx_pos p;
    int cur = shown(now, &p);
    if (A.on) {
        if ((int)(now - A.end) >= 0) A.on = 0, p = G.p, cur = -1;
        else ui_animate();
    }
    ui_put(t_board, bx, by, (float)bw, (float)bh);
    ui_tool(HINT_TOOL, ICON_HINT, hx_over(&G) ? UI_DIM : hint.on && (now - hint.t0) / 320 % 2 == 0 ? UI_ON : 0);
    draw_board_marks(&p, now);
    if (hint.on && (int)(now - hint.t0) > HINT_MS) hint.on = 0;   /* (the stack it chose stays chosen) */
    float et = endfx.on && (int)(now - endfx.t0) >= 0 ? (float)(now - endfx.t0) : -1;
    if (endfx.on && et < END_MS) ui_animate();

    /* where a stack held or chosen would land, and the stacks it would gather */
    int target = -1, tslot = -1;
    float fx = 0, fy = 0, lift_k = 1;
    if (drag.on && drag.moving) {
        float f = clampf((now - drag.t0) / (float)LIFT_MS, 0, 1);
        lift_k = TRAY_K + (1 - TRAY_K) * puz_ease(f);
        fx = drag.x;
        fy = drag.y - 1.05f * W;
        float bd = 0.8f * W;
        for (int c = 0; c < HX_CELLS; c++) {
            if (G.p.kind[c] != HX_OPEN || G.p.n[c]) continue;
            float x, y;
            cell_xy(c, &x, &y);
            float d = hypotf(x - fx, (y - fy) / SQ);
            if (d < bd) bd = d, target = c;
        }
        tslot = drag.slot;
        if (f < 1) ui_animate();
    } else if (hint.on && sel >= 0) target = hint.cell, tslot = sel;
    if (target >= 0 && tslot >= 0 && G.p.tray[tslot].n) {
        float x, y;
        cell_xy(target, &x, &y);
        float pulse = 0.5f + 0.5f * sinf(now / 150.0f);
        ui_mark_gold_a(x, y, 1.5f * W, hint.on ? 0.45f + 0.4f * pulse : 0.75f);
        int col = G.p.tray[tslot].t[G.p.tray[tslot].n - 1], nb[6], k = hx_neighbours(&G.p, target, nb);
        for (int i = 0; i < k; i++)
            if (G.p.n[nb[i]] && hx_top(&G.p, nb[i], NULL) == col) {
                float nx, ny;
                cell_xy(nb[i], &nx, &ny);
                ui_mark_gold_a(nx, ny, 1.3f * W, 0.3f + 0.25f * pulse);
            }
        ui_animate();
    }

    /* the stacks, row by row from the back; the cell just filled waits for its drop */
    const hx_event *ce = cur >= 0 ? &A.ev[cur] : NULL;
    float ft = ce ? (float)(int)(now - A.start[cur]) : 0;
    int launched = 0, landed = 0, popped = 0;
    if (ce && ce->kind == HX_EV_MOVE) {
        Uint32 s = stagger(ce->count);
        for (int i = 0; i < ce->count; i++) {
            if (ft >= i * (float)s) launched++;
            if (ft >= i * (float)s + FLY_MS) landed++;
        }
    } else if (ce && ce->kind == HX_EV_CLEAR)
        for (int j = 0; j < ce->count; j++) popped += ft >= j * 28.0f;
    for (int row = 0; row < HX_ROWS; row++)
        for (int col = 0; col < HX_COLS; col++) {
            int c = col + row * HX_COLS, n = p.n[c];
            if (A.on && c == A.cell && (int)(now - A.drop_end) < 0) continue;
            if (ce && ce->kind == HX_EV_MOVE && c == ce->a) n -= launched;
            if (ce && ce->kind == HX_EV_MOVE && c == ce->b) n += landed;
            if (ce && ce->kind == HX_EV_CLEAR && c == ce->a) n -= popped;
            if (n <= 0) continue;
            uint8_t t[HX_TALL + HX_STACK];
            memcpy(t, p.t[c], p.n[c] < n ? p.n[c] : n);
            for (int i = p.n[c]; i < n; i++) t[i] = ce->colour;
            float x, y;
            cell_xy(c, &x, &y);
            Uint8 light = 255;
            if (et >= 0 && !endfx.won) light = (Uint8)(255 - 115 * clampf((et - row * 70) / 320.0f, 0, 1));
            float squash = 0;
            if (A.on && c == A.cell) {          /* the drop's bounce */
                float bt = (float)(int)(now - A.drop_end);
                if (bt < 160) squash = 0.06f * sinf(PUZ_PI * bt / 160) * W;
            }
            stack_at(t, n, x, y + squash, 1, 0, light, 255, 0);
        }
    /* the end: a gold wave over the board, from the middle out */
    if (et >= 0 && endfx.won)
        for (int c = 0; c < HX_CELLS; c++) {
            if (G.p.kind[c] == HX_VOID) continue;
            float x, y;
            cell_xy(c, &x, &y);
            float d = hypotf(x - (bx + bw / 2.0f), y - (by + bh / 2.0f)) / W, a = puz_pulse(et, 70 * d, 520);
            if (a > 0) ui_mark_gold_a(x, y, 2.0f * W, a);
        }

    /* the chain's step going on: tiles in flight, a band bursting */
    if (ce && ce->kind == HX_EV_MOVE) {
        Uint32 s = stagger(ce->count);
        float ax, ay, bx_, by_;
        cell_xy(ce->a, &ax, &ay);
        cell_xy(ce->b, &bx_, &by_);
        int na = p.n[ce->a], nb_ = p.n[ce->b];
        for (int i = landed; i < launched; i++) {
            float f = (ft - i * (float)s) / FLY_MS;
            float y0 = ay - (na - 1 - i) * THICK * W, y1 = by_ - (nb_ + i) * THICK * W;
            flying_tile(ce->colour, ax, y0, bx_, y1, clampf(f, 0, 1), 1);
        }
    } else if (ce && ce->kind == HX_EV_CLEAR) {
        float x, y;
        cell_xy(ce->a, &x, &y);
        int n0 = p.n[ce->a];
        for (int j = 0; j < ce->count && j < popped; j++) {   /* each tile from the top: up, swelling, fading */
            float f = clampf((ft - j * 28.0f) / POP_MS, 0, 1);
            if (f >= 1) continue;
            float h = (n0 - 1 - j) * THICK * W, rise = (0.25f + 1.1f * puz_ease(f)) * W * (0.4f + 0.06f * j);
            Uint8 a = (Uint8)(255 * (1 - f) * (1 - f));
            tile_at(0, ce->colour, 1, x + (art_hash(ce->a, j) - 0.5f) * 0.5f * W * f, y - h - rise, 0,
                    1 + 0.3f * f, 255, a);
        }
        float burst = clampf(ft / (POP_MS + ce->count * 28.0f), 0, 1);
        float top = y - n0 * THICK * W * (1 - burst);
        ui_mark_gold_a(x, top, (1.2f + 2.6f * burst) * W, 0.9f * (1 - burst));
        for (int k = 0; k < 10; k++) {          /* sparks */
            float a = art_hash(k, ce->a + 7) * 2 * PUZ_PI, r = (0.3f + 1.4f * puz_ease(burst)) * W;
            pzk_dot(x + cosf(a) * r, top + sinf(a) * r * SQ - 0.4f * W * burst, 0.16f * W * (1 - burst), 0.08f * W,
                    (pzk_rgba){255, 226, 140, (Uint8)(230 * (1 - burst))}, 1);
        }
        /* the count rising */
        float h = 0.62f * W, cy = top - 0.6f * W - 1.0f * W * puz_ease(burst);
        Uint8 a = (Uint8)(255 * clampf(1.5f - 1.5f * burst, 0, 1));
        static const uint8_t gold[3] = {255, 216, 110};
        float nw = ui_number_w(ce->count, h), px = x - (nw + 0.55f * h) / 2;
        ui_fill(px, cy - 0.05f * h, 0.42f * h, 0.1f * h, gold[0], gold[1], gold[2], a);
        ui_fill(px + 0.16f * h, cy - 0.21f * h, 0.1f * h, 0.42f * h, gold[0], gold[1], gold[2], a);
        ui_number_ex(px + 0.55f * h + nw / 2 + 0.03f * h, cy + 0.04f * h, h, ce->count, PUZ_INK, (Uint8)(a * 0.6f), 0);
        ui_number_ex(px + 0.55f * h + nw / 2, cy, h, ce->count, gold, a, 0);
    }

    /* the tray */
    for (int i = 0; i < HX_TRAY; i++) {
        float x, y;
        slot_mid(i, &x, &y);
        if (t_mat) {
            const uint8_t *ink = ui_table_ink();
            SDL_SetTextureColorMod(t_mat, ink[0], ink[1], ink[2]);
            SDL_SetTextureAlphaMod(t_mat, 40);
            ui_put_c(t_mat, x, y, TRAY_K * W * 1.05f, TRAY_K * W * 1.05f * SQ * 1.1547f);
            SDL_SetTextureAlphaMod(t_mat, 255);
            SDL_SetTextureColorMod(t_mat, 255, 255, 255);
        }
        const hx_tray_stack *s = &G.p.tray[i];
        if (!s->n || (drag.on && drag.moving && drag.slot == i)) continue;
        float ox = 0, k = TRAY_K;
        if (back.on && back.slot == i) {
            float f = clampf((now - back.t0) / (float)BACK_MS, 0, 1), e = puz_ease(f);
            if (f >= 1) back.on = 0;
            else ui_animate();
            x = back.x + (x - back.x) * e;
            y = back.y + (y - back.y) * e;
            k = back.k + (TRAY_K - back.k) * e;
        } else if (deal_fx.on) {
            float f = clampf(((int)(now - deal_fx.t0) - i * 70) / (float)DEAL_MS, 0, 1), e = puz_ease(f);
            if (f < 1) ui_animate();
            else if (i == HX_TRAY - 1) deal_fx.on = 0;
            if (below_board) ox = (1 - e) * (last_box.x + last_box.w + W - x);
            else y += (1 - e) * (last_box.y + last_box.h + W - y);
        }
        float lift = 0;
        if (sel == i) {
            ui_mark_gold_a(x + ox, y - 0.5f * W, 2.2f * W, 0.55f);
            lift = 0.18f * W;
        }
        int dim = et >= 0 && !endfx.won;
        stack_at(s->t, s->n, x + ox, y, k, lift, dim ? 140 : 255, 255, fabsf(k - TRAY_K) < 0.01f);
    }

    /* a stack dropping onto its cell */
    if (A.on && (int)(now - A.drop_end) < 0) {
        float f = clampf((now - A.t0) / (float)DROP_MS, 0, 1), e = f * f, x, y;
        cell_xy(A.cell, &x, &y);
        float k = A.sk + (1 - A.sk) * e;
        stack_at(A.from.t[A.cell], A.from.n[A.cell], A.sx + (x - A.sx) * e, A.sy + (y - A.sy) * e, k,
                 0.3f * W * (1 - e), 255, 255, 0);
    }
    /* the stack under the finger */
    if (drag.on && drag.moving && G.p.tray[drag.slot].n) {
        const hx_tray_stack *s = &G.p.tray[drag.slot];
        float sx, sy;
        slot_mid(drag.slot, &sx, &sy);
        float f = clampf((now - drag.t0) / (float)LIFT_MS, 0, 1), e = puz_ease(f);
        stack_at(s->t, s->n, sx + (fx - sx) * e, sy + (fy - sy) * e, lift_k, 0.25f * W, 255, 255, 0);
    }
}

/* ---- the pointer --------------------------------------------------------------------------------------------- */
static int slot_at(float x, float y)
{
    for (int i = 0; i < HX_TRAY; i++) {
        box b = slot_box[i];
        if (x >= b.x && x < b.x + b.w && y >= b.y - 0.4f * W && y < b.y + b.h && G.p.tray[i].n) return i;
    }
    return -1;
}

static int cell_at(float x, float y)            /* the cell under (x, y), stacks counted as their cell */
{
    int best = -1;
    float bd = 0.62f * W;
    for (int c = 0; c < HX_CELLS; c++) {
        if (G.p.kind[c] == HX_VOID) continue;
        float cx, cy;
        cell_xy(c, &cx, &cy);
        float d = hypotf(x - cx, (y - cy) / SQ);
        if (G.p.n[c] && y < cy && x > cx - 0.45f * W && x < cx + 0.45f * W && cy - y < (G.p.n[c] * THICK + 0.5f) * W)
            d = fminf(d, 0.4f * W);
        if (d < bd) bd = d, best = c;
    }
    return best;
}

static void use_hint(void)
{
    int s, c;
    if (!hx_hint(&G, &s, &c)) { ui_sound(SND_DENY); return; }
    sel = s;
    hint.on = 1;
    hint.slot = s;
    hint.cell = c;
    hint.t0 = ui_ms();
    ui_sound(SND_TAP);
    ui_animate();
}

static void press(float x, float y)
{
    tool_press = ui_tool_hit(x, y) == HINT_TOOL;
    if (tool_press) {
        if (!hx_over(&G)) use_hint();
        return;
    }
    if (A.on && (int)(ui_ms() - A.end) < 0) {   /* the chain played out at once */
        A.on = 0;
        if (endfx.on) endfx.t0 = ui_ms();
    }
    back.on = 0;
    drag.on = 0;
    drag.x0 = x;
    drag.y0 = y;
    drag.slot = -1;
    if (hx_over(&G)) return;
    int s = slot_at(x, y);
    if (s >= 0) {
        drag.on = 1;
        drag.moving = 0;
        drag.slot = s;
        drag.x = x;
        drag.y = y;
        drag.t0 = ui_ms();
        ui_animate();
    }
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x;
    drag.y = y;
    if (!drag.moving && hypotf(x - drag.x0, y - drag.y0) > 0.25f * W) {
        drag.moving = 1;
        drag.t0 = ui_ms();
        sel = -1;
        hint.on = 0;
        ui_sound(SND_TAP);
    }
    ui_animate();
}

static void release(float x, float y)
{
    if (tool_press) { tool_press = 0; return; }
    if (!drag.on) {
        if (hypotf(x - drag.x0, y - drag.y0) > 0.5f * W || hx_over(&G)) return;
        int c = cell_at(x, y);
        if (c < 0) {
            if (sel >= 0) sel = -1, hint.on = 0, ui_animate();
            return;
        }
        if (sel < 0 || G.p.kind[c] != HX_OPEN || G.p.n[c]) { ui_sound(SND_DENY); return; }
        float sx, sy;
        slot_mid(sel, &sx, &sy);
        lay(sel, c, sx, sy, TRAY_K);
        return;
    }
    int s = drag.slot;
    drag.x = x;
    drag.y = y;
    if (!drag.moving) {                         /* a tap on a tray stack: chosen, or no longer */
        drag.on = 0;
        hint.on = 0;
        sel = sel == s ? -1 : s;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    drag.on = 0;
    float fx = x, fy = y - 1.05f * W, bd = 0.8f * W;
    int target = -1;
    for (int c = 0; c < HX_CELLS; c++) {
        if (G.p.kind[c] != HX_OPEN || G.p.n[c]) continue;
        float cx, cy;
        cell_xy(c, &cx, &cy);
        float d = hypotf(cx - fx, (cy - fy) / SQ);
        if (d < bd) bd = d, target = c;
    }
    if (target >= 0) {
        lay(s, target, fx, fy, 1);
        return;
    }
    back.on = 1;                                /* nowhere to land: back to the tray */
    back.slot = s;
    back.x = fx;
    back.y = fy;
    back.k = 1;
    back.t0 = ui_ms();
    if (cell_at(fx, fy) >= 0) ui_sound(SND_DENY);
    ui_animate();
}

/* ---- the bar ------------------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    if (G.goal == HX_ENDLESS) {
        int v = G.variant < 0 ? 0 : G.variant, s = hx_score(&G), top = best[v] > s ? best[v] : s;
        ui_bar_fit(b, (ui_count[]){{t_tok, s}, {t_star, top}}, 2, 0);
        return;
    }
    hx_pos p;
    shown(ui_ms(), &p);                         /* the goal's count as the chain plays */
    int done = G.goal == HX_COLOUR ? p.by_colour[G.colour] : p.cleared;
    int left = G.target - done < 0 ? 0 : G.target - done;
    pzt_cream_ink(t_par);
    ui_bar_fit(b, (ui_count[]){{t_goal, left}, {t_placed, G.p.placed}, {t_par, G.par}}, 3, 0);
    pzt_cream_end(t_par);
}

/* ---- the shell's pictures ------------------------------------------------------------------------------------ */
static void thumb(int variant, int size, uint8_t *rgba)
{
    hx_game *t = malloc(sizeof *t);
    if (!t) return;
    if (variant >= 0) {                         /* a board of that size, a few stacks on it */
        hx_endless(t, variant, 3u + (uint32_t)variant);
        uint32_t r = pr_seed(11u + (uint32_t)variant);
        for (int k = 0; k < 4 + 3 * variant; k++) {
            int c = pr_below(&r, HX_CELLS);
            if (t->p.kind[c] != HX_OPEN || t->p.n[c]) continue;
            hx_tray_stack s;
            hx_chance_stack(&r, 4, 1, NULL, 0, &s);
            t->p.n[c] = s.n;
            memcpy(t->p.t[c], s.t, s.n);
        }
        paint_pos(&t->p, size, rgba, 6);
        free(t);
        return;
    }
    /* the menu's card: a small honeycomb, a tall red stack about to clear, tiles on their way */
    if (hx_load(t, "..ooo..|.SSSo..|.SoSSS.|.oSSS..|..ooo..", "FbEjJfCccIbBfFjIcEfJb", "BBB", HX_TILES, 0, 10, 4))
    paint_pos(&t->p, size, rgba, 8);
    free(t);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    hx_game *t = malloc(sizeof *t);
    if (t && hx_level(t, level)) paint_pos(&t->p, size, rgba, 6);
    free(t);
}

/* a chapter's tab: a small board of its kind */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const B[HX_CHAPTERS] = {
        ".......|..SS...|..SoS..|..SS...", ".......|..SS...|..SoS..|..SS...", ".......|..oo...|..S#S..|..oo...",
        ".......|..SS...|..o1o..|..SS...", ".......|..SS...|..SSS..|..SS...", "..SSS..|.SSSS..|.SSoSS.|.SSSS..|..SSS..",
    };
    static const char *const P[HX_CHAPTERS] = {
        "BFJFJB", "BFNJRB", "JJ", "JRJR", "CfkCfkCfkCfkCfkCfkCfk", "BFJNRVBFJNRVBFJNRV",
    };
    hx_game *t = malloc(sizeof *t);
    if (!t) return;
    int c = chapter < 0 ? 0 : chapter % HX_CHAPTERS;
    if (hx_load(t, B[c], P[c], "BBB", HX_TILES, 0, 10, 6)) paint_pos(&t->p, size, rgba, c == 4 ? 10 : 4);
    free(t);
}

static const game hexstack_game = {
    .id = "hexstack", .order = 461, .variants = HX_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = 1,
    .score = score, .par = par, .level_stars = level_stars,
    .levels = HX_LEVELS, .chapters = HX_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb,
};
REGISTER_GAME(hexstack_game)
