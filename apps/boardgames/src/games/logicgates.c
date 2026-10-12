/*
 * Logic Gates in RetroStone Board, for one player (the rules: logicgates_rules.c, logicgates.h): a circuit board of
 * dark green in a walnut frame, brass traces on it, the gates as brass shapes inlaid in walnut plaques (the classic
 * shapes, no letters), the switches along the bottom, the lamps at the top. A wire carrying 1 glows gold; after each
 * move the change runs up the wires, gate after gate. Solved: the lamps flare in a gold burst, then the shell's end
 * card (the stars, the next level).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The levels: the shell's level screen (a tab for each chapter with its emblem, the chapter's 25 levels as tiles; the
 * shell keeps the stars won); its new game goes back to it. In a level, the tools beside the board (above it standing,
 * at its right lying): the level (its chapter's emblem, its number), restart, the hint (one part to change blinks). The
 * bar: the moves made, the par (three stars), the cuts left.
 * Tap a switch or a route switch to flip it, a fuse to cut its wire (sparks); undo takes a move back.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/logicgates.h"
#include "games/puzzles.h"
#include "shell.h"

#define STEP_MS 90                              /* the signal from one part to the next */
#define FLIP_MS 130
#define SPARK_MS 560
#define BURST_MS 1200
#define HINT_MS 2400
#define PAD_B 0.30f                             /* the switches reach below row 0's box, the lamps above the top's */
#define PAD_T 0.46f
#define SPARKS 12

static lg_game G;

/* the animations */
static Uint32 t0;                               /* the last move: its change runs up from there */
static uint8_t old_src[LG_WIRES], old_end[LG_WIRES], old_out[LG_NODES][2], old_val[LG_NODES];
static Uint32 flip_t;                           /* a switch or route switch moving */
static int flip_node = -1;
static struct { int wire; Uint32 t; } spark = {-1, 0};
static struct { int on; lg_move m; Uint32 t; } hint;
static int deny_reset;                          /* the hint found nothing: the restart button blinks */
static Uint32 solved_t;                         /* when the lamps light up (the burst) */

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static box B, panel;                            /* the game's room; the framed board */
static int wide, fr;                            /* lying: the tools at the right */
static float ox, oy, cw, ch, tw;                /* grid (x, y) at (ox + x cw, oy - y ch); the traces' width */
static float tbx[2], tby[2], tbr, labx, laby;   /* the tools: restart, hint; the level's number */

enum { T_RESET, T_HINT };

static float PX(float x) { return ox + x * cw; }
static float PY(float y) { return oy - y * ch; }

/* ---- the colours ------------------------------------------------------------------------------------------------- */
static const uint8_t GOLD[3] = {255, 206, 84}, BRASS[3] = {128, 102, 56}, WALNUT[3] = {92, 60, 40},
                     PCB[3] = {22, 60, 42}, INK[3] = {40, 26, 14};

static art_style style(const uint8_t fill[3], float light, float line)
{
    art_style s;
    for (int i = 0; i < 3; i++) {
        s.fill[i] = fill[i];
        s.outer[i] = s.inner[i] = s.mark[i] = INK[i];
    }
    s.light = light;
    s.line = line;
    return s;
}

/* ---- the pictures (painted once for the board's size) ------------------------------------------------------------ */
/* a plaque (walnut, the house's light wood, brass), its grain, bevelled */
static void paint_plate(uint8_t *o, int w, int h, const uint8_t rgb[3], float k, float grain)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float g = sinf((x * 0.55f + y * 0.1f) * 0.32f + 1.9f * sinf(y * 0.07f + x * 0.01f)) * 0.5f + 0.5f;
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(rgb[i] * k * (1 + grain * (g - 0.5f)), 0, 255);
            p[3] = 255;
        }
    float m = fminf(w, h);
    puz_bevel(o, w, h, 0.16f * m, fmaxf(1.5f, 0.07f * m), 0.4f);
}

static void paint_walnut(uint8_t *o, int w, int h, float k) { paint_plate(o, w, h, WALNUT, k, 0.12f); }

/* the board: the house's walnut frame round a dark green circuit board */
static void paint_board(int W, int H, int f, uint8_t *o)
{
    int S = W > H ? W : H;
    uint8_t *sq = malloc((size_t)S * S * 4);
    if (!sq) return;
    art_board_n(1, 1, S - 2 * f, f, 0, sq);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int sx = x < W / 2 ? x : S - (W - x), sy = y < H / 2 ? y : S - (H - y);
            uint8_t *p = o + 4 * (y * W + x);
            memcpy(p, sq + 4 * ((size_t)sy * S + sx), 4);
            if (x < f || y < f || x >= W - f || y >= H - f) continue;
            float ix = (float)(x - f) / (W - 2 * f), iy = (float)(y - f) / (H - 2 * f);
            float e = fminf(x - f, y - f), sh = 0.022f * fminf(W, H);
            float k = 0.92f + 0.16f * (1 - fabsf(ix - 0.5f)) * (1 - fabsf(iy - 0.5f));
            k *= 1 - 0.4f * clampf(1 - e / sh, 0, 1);   /* the frame's shadow */
            k *= 1 + 0.025f * sinf(x * 0.9f) * sinf(y * 0.83f);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(PCB[i] * k, 0, 255);
            p[3] = 255;
        }
    free(sq);
}

/* a quadratic curve's points (n + 1) from a to c, pulled towards b */
static int curve(float *p, int k, float ax, float ay, float bx, float by, float cx, float cy, int n)
{
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n, u = 1 - t;
        p[k++] = u * u * ax + 2 * u * t * bx + t * t * cx;
        p[k++] = u * u * ay + 2 * u * t * by + t * t * cy;
    }
    return k;
}

/* a gate's brass shape, pointing up, in a w x h picture (pixels); its output's lead from its top to the picture's
 * top edge (lw wide). Lit: gold. */
static void paint_gate(int kind, int lit, int w, int h, float lw, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    art_style st = style(lit ? GOLD : BRASS, 0.4f, fmaxf(1.0f, 0.03f * fminf(w, h)));
    art_fig *f = art_fig_new();
    float xl = 0.1f * w, xr = 0.9f * w, xc = 0.5f * w, yb = 0.94f * h, yt = 0.07f * h;
    int bubble = kind == LG_NOT || kind == LG_NAND || kind == LG_NOR;
    float rb = 0.085f * fminf(w, 1.6f * h);
    if (bubble) yt += 2 * rb;
    art_rect(f, ART_PART, xc - lw / 2, 0, xc + lw / 2, yt + 0.1f * h);
    float p[2 * 200];
    int n = 0;
    if (kind == LG_AND || kind == LG_NAND) {
        float rx = (xr - xl) / 2, ry = fminf(rx * 0.9f, (yb - yt) * 0.62f), ym = yt + ry;
        p[n++] = xl; p[n++] = yb;
        p[n++] = xr; p[n++] = yb;
        for (int i = 0; i <= 24; i++) {
            float a = 3.14159265f * i / 24;
            p[n++] = xc + rx * cosf(a);
            p[n++] = ym - ry * sinf(a);
        }
        art_poly(f, ART_PART, p, n / 2);
    } else if (kind == LG_NOT) {
        float q[] = {xl + 0.06f * w, yb, xr - 0.06f * w, yb, xc, yt};
        art_poly(f, ART_PART, q, 3);
    } else {                                    /* OR, NOR, XOR: a curved shield, its back hollow */
        float back = 0.17f * (yb - yt), gap = kind == LG_XOR ? 0.13f * (yb - yt) : 0;
        float b = yb - gap;
        n = curve(p, n, xl, b, xc, b - 2 * back, xr, b, 16);
        n = curve(p, n, xr, b, xr, yt + 0.42f * (b - yt), xc, yt, 16);
        n = curve(p, n, xc, yt, xl, yt + 0.42f * (b - yt), xl, b, 16);
        art_poly(f, ART_PART, p, n / 2);
        if (kind == LG_XOR) {                   /* the second back line */
            float t = fmaxf(1.5f, 0.06f * (yb - yt));
            int m = 0;
            m = curve(p, m, xl, yb, xc, yb - 2 * back, xr, yb, 16);
            float q[2 * 40];
            int k = curve(q, 0, xr, yb - t, xc, yb - t - 2 * back, xl, yb - t, 16);
            memcpy(p + m, q, sizeof(float) * (size_t)k);
            art_poly(f, ART_PART, p, (m + k) / 2);
        }
    }
    if (bubble) art_ellipse(f, ART_PART, xc, yt - rb, rb, rb);
    art_render(f, &st, w, h, 1, 0, 0, o);
    art_fig_free(f);
}

/* a route switch's brass way on its plaque (w x h): from the input at the bottom middle up to the output at off
 * pixels left (way 0) or right (way 1) of the middle; the other way a dark groove */
static void paint_route(int way, int lit, int w, int h, float off, float lw, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    float xc = w / 2.0f, yj = 0.62f * h, ya = 0.3f * h;
    for (int pass = 0; pass < 2; pass++) {
        const uint8_t groove[3] = {30, 20, 12};
        art_style st = style(pass ? (lit ? GOLD : BRASS) : groove, pass ? 0.4f : 0, pass ? fmaxf(1, 0.025f * w) : 0);
        art_fig *f = art_fig_new();
        float t = pass ? lw * 0.75f : lw * 0.45f;
        for (int side = -1; side <= 1; side += 2) {
            if (pass && (side < 0) != (way == 0)) continue;
            float x1 = xc + side * off;
            const float diag[] = {xc - t, yj, xc + t, yj, x1 + t, ya, x1 - t, ya};
            art_poly(f, ART_PART, diag, 4);
            art_rect(f, ART_PART, x1 - t, 0, x1 + t, ya + 0.5f * t);
        }
        art_rect(f, ART_PART, xc - t, yj - 0.5f * t, xc + t, (float)h);
        if (pass) art_ellipse(f, ART_PART, xc, yj, 1.9f * t, 1.9f * t);
        uint8_t *b = calloc((size_t)w * h, 4);
        if (b) {
            art_render(f, &st, w, h, 1, 0, 0, b);
            art_blend(o, w, h, b, w, h, 0, 0);
            free(b);
        }
        art_fig_free(f);
    }
}

/* a switch's plate (w x h): walnut, a slot down its middle (gold inside when on) */
static void paint_switch(int on, int w, int h, uint8_t *o)
{
    paint_walnut(o, w, h, 1.0f);
    float sw = 0.34f * w, x0 = (w - sw) / 2, y0 = 0.12f * h, y1 = 0.88f * h;
    for (int y = (int)y0; y < (int)y1; y++)
        for (int x = (int)x0; x < (int)(x0 + sw); x++) {
            float dx = fminf(x - x0, x0 + sw - 1 - x), dy = fminf(y - y0, y1 - 1 - y), r = sw / 2;
            float cxd = fmaxf(0, r - dx), cyd = fmaxf(0, r - dy);
            if (cxd * cxd + cyd * cyd > r * r) continue;
            uint8_t *p = o + 4 * (y * w + x);
            float depth = clampf((y - y0) / (y1 - y0), 0, 1);
            const uint8_t *c = on ? GOLD : INK;
            float k = on ? 0.65f + 0.35f * depth : 0.7f;
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(c[i] * k);
        }
}

/* the bar screwed over a locked switch (w x h): brass, two screws */
static void paint_lock(int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    const uint8_t brass[3] = {196, 156, 76}, screw[3] = {150, 118, 62};
    art_style st = style(brass, 0.5f, fmaxf(1, 0.03f * h));
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, 0.02f * w, 0.18f * h, 0.98f * w, 0.82f * h);
    art_render(f, &st, w, h, 1, 0, 0, o);
    art_fig_free(f);
    art_style ss = style(screw, 0.6f, fmaxf(1, 0.03f * h));
    f = art_fig_new();
    for (int s = 0; s < 2; s++) {
        float cx = (s ? 0.82f : 0.18f) * w, r = 0.2f * h;
        art_ellipse(f, ART_PART, cx, 0.5f * h, r, r);
        art_rect(f, ART_MARK, cx - 0.8f * r, 0.5f * h - 0.12f * r, cx + 0.8f * r, 0.5f * h + 0.12f * r);
    }
    uint8_t *b = calloc((size_t)w * h, 4);
    if (b) {
        art_render(f, &ss, w, h, 1, 0, 0, b);
        art_blend(o, w, h, b, w, h, 0, 0);
        free(b);
    }
    art_fig_free(f);
}

/* a lamp (w x h): a brass socket at the bottom, the glass globe above it (lit: gold), its filament */
static void paint_lamp(int alarm, int lit, int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    const uint8_t brass[3] = {176, 140, 70}, glass[3] = {150, 166, 156}, warm[3] = {255, 238, 150},
                  red_off[3] = {112, 34, 28}, red_on[3] = {244, 70, 46};
    float xc = w / 2.0f, sb = 0.3f * h, r = fminf(0.48f * w, 0.36f * h);
    art_style sk = style(brass, 0.5f, fmaxf(1, 0.025f * w));
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, xc - 0.42f * r, h - sb, xc + 0.42f * r, (float)h);
    for (int i = 1; i < 3; i++)
        art_rect(f, ART_MARK, xc - 0.42f * r, h - sb + i * sb / 3 - 0.6f, xc + 0.42f * r, h - sb + i * sb / 3 + 0.6f);
    art_render(f, &sk, w, h, 1, 0, 0, o);
    art_fig_free(f);
    f = art_fig_new();
    float cy = h - sb - 0.82f * r;
    const uint8_t *fill = alarm ? (lit ? red_on : red_off) : lit ? warm : glass;
    art_style sg = style(fill, lit ? 0.3f : 0.55f, fmaxf(1, 0.025f * w));
    if (alarm) {                                /* a dome, a cage over it */
        float p[2 * 40];
        int n = 0;
        p[n++] = xc - r; p[n++] = h - sb + 0.5f;
        for (int i = 0; i <= 24; i++) {
            float a = 3.14159265f * i / 24;
            p[n++] = xc - r * cosf(a);
            p[n++] = h - sb - 1.25f * r * sinf(a);
        }
        p[n++] = xc + r; p[n++] = h - sb + 0.5f;
        art_poly(f, ART_PART, p, n / 2);
        float t = fmaxf(0.8f, 0.035f * r);
        art_rect(f, ART_MARK, xc - t, h - sb - 1.25f * r, xc + t, h - sb);
        art_rect(f, ART_MARK, xc - r, h - sb - 0.55f * r - t, xc + r, h - sb - 0.55f * r + t);
    } else {
        art_ellipse(f, ART_PART, xc, cy, r, r);
        const float neck[] = {xc - 0.55f * r, cy + 0.6f * r, xc + 0.55f * r, cy + 0.6f * r, xc + 0.42f * r,
                              h - sb + 1.0f, xc - 0.42f * r, h - sb + 1.0f};
        art_poly(f, ART_PART, neck, 4);
        float t = fmaxf(0.7f, 0.04f * r);         /* the filament: a small zigzag */
        for (int i = 0; i < 4; i++) {
            float xa = xc - 0.3f * r + i * 0.15f * r, xb = xa + 0.15f * r, ya = cy + (i & 1 ? -0.12f : 0.12f) * r,
                  yb2 = cy + (i & 1 ? 0.12f : -0.12f) * r;
            const float seg[] = {xa, ya - t, xb, yb2 - t, xb, yb2 + t, xa, ya + t};
            art_poly(f, ART_MARK, seg, 4);
        }
        art_rect(f, ART_MARK, xc - 0.3f * r - t, cy + 0.12f * r, xc - 0.3f * r + t, h - sb);
        art_rect(f, ART_MARK, xc + 0.3f * r - t, cy + 0.12f * r, xc + 0.3f * r + t, h - sb);
    }
    uint8_t *b = calloc((size_t)w * h, 4);
    if (b) {
        if (lit && !alarm) memcpy(sg.mark, (float[3]){170, 90, 20}, sizeof sg.mark);
        art_render(f, &sg, w, h, 1, 0, 0, b);
        art_blend(o, w, h, b, w, h, 0, 0);
        free(b);
    }
    art_fig_free(f);
}

/* a fuse lying left to right (w x h): a cream body between brass caps; broken: two halves, charred */
static void paint_fuse(int broken, int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    const uint8_t cream[3] = {238, 226, 200}, brass[3] = {196, 156, 76}, char_c[3] = {36, 26, 20};
    float m = fmaxf(1, 0.06f * h);
    art_style sb = style(cream, 0.45f, m), sc = style(brass, 0.5f, m), sx = style(char_c, 0, 0);
    for (int part = 0; part < 3; part++) {
        art_fig *f = art_fig_new();
        const art_style *st = part == 0 ? &sb : part == 1 ? &sc : &sx;
        if (part == 0) {
            if (broken) {
                art_rect(f, ART_PART, 0.2f * w, 0.18f * h, 0.42f * w, 0.82f * h);
                art_rect(f, ART_PART, 0.58f * w, 0.18f * h, 0.8f * w, 0.82f * h);
            } else {
                art_rect(f, ART_PART, 0.2f * w, 0.18f * h, 0.8f * w, 0.82f * h);
            }
        } else if (part == 1) {
            art_rect(f, ART_PART, 0.04f * w, 0.08f * h, 0.24f * w, 0.92f * h);
            art_rect(f, ART_PART, 0.76f * w, 0.08f * h, 0.96f * w, 0.92f * h);
        } else if (broken) {
            art_rect(f, ART_PART, 0.38f * w, 0.18f * h, 0.43f * w, 0.82f * h);
            art_rect(f, ART_PART, 0.57f * w, 0.18f * h, 0.62f * w, 0.82f * h);
        }
        uint8_t *b = calloc((size_t)w * h, 4);
        if (b) {
            art_render(f, st, w, h, 1, 0, 0, b);
            art_blend(o, w, h, b, w, h, 0, 0);
            free(b);
        }
        art_fig_free(f);
    }
}

static void paint_star(int size, const uint8_t rgb[3], uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_style st = style(rgb, 0.35f, fmaxf(1, 0.04f * size));
    art_fig *f = art_fig_new();
    float p[20];
    for (int i = 0; i < 10; i++) {
        float a = -3.14159265f / 2 + i * 3.14159265f / 5, r = (i & 1 ? 0.21f : 0.48f) * size;
        p[2 * i] = size / 2.0f + r * cosf(a);
        p[2 * i + 1] = size * 0.53f + r * sinf(a);
    }
    art_poly(f, ART_PART, p, 10);
    art_render(f, &st, size, size, 1, 0, 0, o);
    art_fig_free(f);
}

/* the restart tool's icon (white): an arrow going round */
static void paint_restart(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    const uint8_t white[3] = {255, 255, 255};
    art_style st = style(white, 0, 0);
    art_fig *f = art_fig_new();
    art_arc(f, ART_PART, 0.5f, 0.52f, 0.2f, 0.29f, -10, 250);
    float a = -10 * 3.14159265f / 180, px = 0.5f + 0.245f * cosf(a), py = 0.52f - 0.245f * sinf(a);
    const float head[] = {px - 0.13f, py, px + 0.13f, py, px, py + 0.15f};
    art_poly(f, ART_PART, head, 3);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

/* the chapters' emblems (size x size): AND, XOR, a route switch, a cut fuse, a wire branching to three, a lamp */
static void paint_emblem(int c, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    int w = (int)(0.8f * size), h = (int)(0.8f * size);
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    float lw = fmaxf(2, 0.07f * size);
    if (c == 0 || c == 1) {
        paint_gate(c == 0 ? LG_AND : LG_XOR, 1, w, h, lw, b);
        art_blend(o, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    } else if (c == 2) {
        paint_route(1, 1, w, h, 0.26f * w, lw, b);
        art_blend(o, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    } else if (c == 3 || c == 4) {              /* gold traces: a wire through a fuse, or a branching */
        art_style st = style(GOLD, 0.3f, fmaxf(1, 0.025f * size));
        art_fig *f = art_fig_new();
        float t = lw / 2, S = (float)size;
        if (c == 3) {
            art_rect(f, ART_PART, 0, S / 2 - t, S, S / 2 + t);
        } else {
            art_rect(f, ART_PART, S / 2 - t, 0.52f * S, S / 2 + t, S);
            art_rect(f, ART_PART, 0.18f * S - t, 0.52f * S - t, 0.82f * S + t, 0.52f * S + t);
            for (int k = 0; k < 3; k++) {
                float x = (0.18f + 0.32f * k) * S;
                art_rect(f, ART_PART, x - t, 0.14f * S, x + t, 0.52f * S);
                art_ellipse(f, ART_PART, x, 0.14f * S, 2.2f * t, 2.2f * t);
            }
            art_ellipse(f, ART_PART, S / 2, 0.52f * S, 2.2f * t, 2.2f * t);
        }
        art_render(f, &st, size, size, 1, 0, 0, b);
        art_fig_free(f);
        art_blend(o, size, size, b, size, size, 0, 0);
        if (c == 3) {
            int fh = (int)(0.42f * size);
            memset(b, 0, (size_t)size * size * 4);
            paint_fuse(1, size, fh, b);
            art_blend(o, size, size, b, size, fh, 0, (size - fh) / 2);
        }
    } else {
        paint_lamp(0, 1, w, h, b);
        art_blend(o, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    }
    free(b);
}

/* the token: a lamp, lit */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    memset(rgba, 0, (size_t)size * size * 4);
    int w = (int)(0.78f * size), h = (int)(0.96f * size);
    uint8_t *b = calloc((size_t)w * h, 4);
    if (!b) return;
    paint_lamp(0, 1, w, h, b);
    art_blend(rgba, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    free(b);
}

/* ---- the textures ------------------------------------------------------------------------------------------------ */
static SDL_Texture *t_board, *t_plaque, *t_gate[LG_KINDS][2][2], *t_route[2][2], *t_switch[2], *t_lock, *t_lamp[2][2],
    *t_fuse[2], *t_dot, *t_glow, *t_tool, *t_bulb, *t_disc, *t_emblem[LG_CHAPTERS];
static int made_board = -1, made_parts = -1, made_shared = -1, made_bar = -1;
static float pw, ph, sw_w, sw_h, lamp_w, lamp_h, fuse_l, fuse_t, gate_w[2], gate_h;
static puz_font font;

static SDL_Texture *tex_buf(int w, int h, uint8_t *b)
{
    SDL_Texture *t = ui_tex(w, h, b);
    free(b);
    return t;
}

static void board_textures(void)
{
    int key = (int)panel.w * 4096 + (int)panel.h;
    if (made_board == key) return;
    made_board = key;
    int W = (int)panel.w, H = (int)panel.h;
    fr = (int)(0.03f * fminf(W, H));
    uint8_t *b = calloc((size_t)W * H, 4);
    if (!b) return;
    paint_board(W, H, fr, b);
    ui_free(&t_board);
    t_board = tex_buf(W, H, b);
}

static void part_textures(void)
{
    int key = (int)(cw * 8) * 8192 + (int)(ch * 8);
    if (made_parts == key) return;
    made_parts = key;
    float m = fminf(cw, ch);
    tw = fmaxf(2.5f, roundf(0.05f * m));
    pw = roundf(2 * LG_HALF * cw);
    ph = roundf(LG_NH * ch);
    int w = (int)pw, h = (int)ph;
    ui_free(&t_plaque);
    uint8_t *b = calloc((size_t)w * h, 4);
    if (b) { paint_walnut(b, w, h, 1.0f); t_plaque = tex_buf(w, h, b); }
    gate_h = floorf(0.92f * ph);
    for (int wide3 = 0; wide3 < 2; wide3++) {   /* two inputs, or three: wider */
        gate_w[wide3] = floorf(fminf(0.9f * pw, wide3 ? 1.6f * gate_h : 1.2f * gate_h));
        int gw = (int)gate_w[wide3], gh = (int)gate_h;
        for (int k = LG_NOT; k <= LG_NOR; k++)
            for (int lit = 0; lit < 2; lit++) {
                ui_free(&t_gate[k][lit][wide3]);
                b = calloc((size_t)gw * gh, 4);
                if (!b) continue;
                paint_gate(k, lit, gw, gh, tw, b);
                t_gate[k][lit][wide3] = tex_buf(gw, gh, b);
            }
    }
    for (int way = 0; way < 2; way++)
        for (int lit = 0; lit < 2; lit++) {
            ui_free(&t_route[way][lit]);
            b = calloc((size_t)w * h, 4);
            if (!b) continue;
            paint_route(way, lit, w, h, 0.18f * cw, tw, b);
            t_route[way][lit] = tex_buf(w, h, b);
        }
    sw_w = roundf(fminf(0.5f * cw, 0.62f * ch));
    sw_h = roundf((LG_NH + PAD_B - 0.06f) * ch);
    for (int on = 0; on < 2; on++) {
        ui_free(&t_switch[on]);
        b = calloc((size_t)sw_w * sw_h, 4);
        if (!b) continue;
        paint_switch(on, (int)sw_w, (int)sw_h, b);
        t_switch[on] = tex_buf((int)sw_w, (int)sw_h, b);
    }
    ui_free(&t_lock);
    int lkw = (int)(sw_w * 1.12f), lkh = (int)(sw_w * 0.42f);
    b = calloc((size_t)lkw * lkh, 4);
    if (b) { paint_lock(lkw, lkh, b); t_lock = tex_buf(lkw, lkh, b); }
    lamp_h = roundf((LG_NH + PAD_T - 0.04f) * ch);
    lamp_w = roundf(fminf(0.8f * cw, 0.85f * lamp_h));
    for (int a = 0; a < 2; a++)
        for (int lit = 0; lit < 2; lit++) {
            ui_free(&t_lamp[a][lit]);
            b = calloc((size_t)lamp_w * lamp_h, 4);
            if (!b) continue;
            paint_lamp(a, lit, (int)lamp_w, (int)lamp_h, b);
            t_lamp[a][lit] = tex_buf((int)lamp_w, (int)lamp_h, b);
        }
    fuse_l = roundf(fmaxf(18, 0.28f * m));
    fuse_t = roundf(fmaxf(8, 2.7f * tw));
    for (int k = 0; k < 2; k++) {
        ui_free(&t_fuse[k]);
        b = calloc((size_t)fuse_l * fuse_t, 4);
        if (!b) continue;
        paint_fuse(k, (int)fuse_l, (int)fuse_t, b);
        t_fuse[k] = tex_buf((int)fuse_l, (int)fuse_t, b);
    }
}

static void shared_textures(void)
{
    int key = (int)ui.unit;
    if (made_shared == key) return;
    made_shared = key;
    const uint8_t white[3] = {255, 255, 255}, gold[3] = {255, 200, 80}, house[3] = {54, 72, 64};
    int d = (int)(0.5f * ui.unit) | 1;
    ui_free(&t_dot);
    uint8_t *b = calloc((size_t)d * d, 4);
    if (b) { art_disc(d, white, 1, 0, b); t_dot = tex_buf(d, d, b); }
    int g = (int)(2 * ui.unit);
    ui_free(&t_glow);
    b = calloc((size_t)g * g, 4);
    if (b) { art_disc(g, gold, 0.85f, 1.6f, b); t_glow = tex_buf(g, g, b); }
    int ic = (int)(0.62f * ui.unit);
    ui_free(&t_tool);
    b = calloc((size_t)ic * ic, 4);
    if (b) { paint_restart(ic, b); t_tool = tex_buf(ic, ic, b); }
    ui_free(&t_bulb);
    b = calloc((size_t)ic * ic, 4);
    if (b) { art_icon(ICON_HINT, ic, b); t_bulb = tex_buf(ic, ic, b); }
    SDL_SetTextureColorMod(t_bulb, 252, 236, 170);
    int dd = (int)(1.0f * ui.unit);
    ui_free(&t_disc);
    b = calloc((size_t)dd * dd, 4);
    if (b) { art_disc(dd, house, 1, 0, b); t_disc = tex_buf(dd, dd, b); }
    int e = (int)(0.7f * ui.unit);
    for (int c = 0; c < LG_CHAPTERS; c++) {
        ui_free(&t_emblem[c]);
        b = calloc((size_t)e * e, 4);
        if (b) { paint_emblem(c, e, b); t_emblem[c] = tex_buf(e, e, b); }
    }
    (void)white;
    puz_font_make(&font, (int)(0.6f * ui.unit), 1);
}

static SDL_Texture *t_knob;
static int made_knob = -1;

static void knob_texture(void)
{
    int d = (int)(0.66f * sw_w);
    if (made_knob == d) return;
    made_knob = d;
    ui_free(&t_knob);
    uint8_t *b = calloc((size_t)d * d, 4);
    if (b) { art_counter(COUNTER_DISC, 0, d, b); t_knob = tex_buf(d, d, b); }
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void fit(void)
{
    float m = 0.16f * ui.unit;
    float iw = panel.w - 2 * fr - 2 * m, ih = panel.h - 2 * fr - 2 * m;
    int cols = G.L.cols, rows = G.L.rows;
    float span = rows - 1 + LG_NH + PAD_B + PAD_T, cap = fminf(iw, ih) / 3.0f;
    cw = iw / cols;
    ch = ih / span;
    cw = fminf(cw, fminf(cap, 1.15f * ch));
    ch = fminf(ch, fminf(1.15f * cap, 1.4f * cw));
    cw = floorf(cw);
    ch = floorf(ch);
    ox = floorf(panel.x + panel.w / 2 - cols * cw / 2);
    oy = floorf(panel.y + panel.h / 2 + (span / 2 - PAD_B) * ch);
    part_textures();
    knob_texture();
}

static void layout(box b)
{
    B = b;
    float u = ui.unit;
    wide = b.w > 1.08f * b.h;
    if (wide) {
        float tcw = 1.35f * u, cx = b.x + b.w - tcw / 2;
        panel = (box){b.x, b.y, floorf(b.w - tcw - 0.2f * u), b.h};
        labx = cx;
        laby = b.y + 0.75f * u;
        for (int i = 0; i < 2; i++) {           /* under the level's number */
            tbx[i] = cx;
            tby[i] = b.y + (2.0f + 1.25f * i) * u;
        }
    } else {
        float th = 1.15f * u, cy = b.y + th / 2;
        panel = (box){b.x, b.y + th + 0.15f * u, b.w, b.h - th - 0.15f * u};
        tbx[T_RESET] = b.x + b.w - 1.85f * u;
        tbx[T_HINT] = b.x + b.w - 0.6f * u;
        for (int i = 0; i < 2; i++) tby[i] = cy;
        labx = b.x + b.w / 2;
        laby = cy;
    }
    tbr = 0.48f * u;
    shared_textures();
    board_textures();
    if (G.L.cols) fit();                        /* a level open */
}

/* ---- the level --------------------------------------------------------------------------------------------------- */
static int lamp_depth(void)
{
    int d = 0;
    for (int i = 0; i < G.L.nn; i++)
        if ((G.L.n[i].kind == LG_LAMP || G.L.n[i].kind == LG_ALARM) && G.L.depth[i] > d) d = G.L.depth[i];
    return d;
}

static void calm(void)
{
    t0 = 0;
    flip_node = -1;
    spark.wire = -1;
    hint.on = 0;
    deny_reset = 0;
}

static void open_level(int level)
{
    if (!lg_open(&G, level)) return;
    calm();
    if (B.w > 0) fit();
}

static void start(int level) { open_level(level); }   /* the shell's level screen: 0..LG_LEVELS-1 */

static int status(void) { return lg_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return lg_stars(&G); }

/* after a move (or undo, restart): the change runs up the wires from now */
static void moved(const lg_game *before)
{
    memcpy(old_src, before->wsrc, sizeof old_src);
    memcpy(old_end, before->wend, sizeof old_end);
    memcpy(old_out, before->out, sizeof old_out);
    memcpy(old_val, before->val, sizeof old_val);
    t0 = ui_ms();
    if (!t0) t0 = 1;
    if (lg_solved(&G)) solved_t = t0 + (Uint32)(lamp_depth() * STEP_MS);
    ui_animate();
}

static int busy(void)
{
    Uint32 now = ui_ms();
    if (t0 && now - t0 < (Uint32)((G.L.maxdepth + 1) * STEP_MS)) return 1;
    if (flip_node >= 0 && now - flip_t < FLIP_MS) return 1;
    if (spark.wire >= 0 && now - spark.t < SPARK_MS) return 1;
    return lg_solved(&G) && t0 && (int)(now - solved_t) < BURST_MS;
}

static int undo(void)
{
    static lg_game before;
    before = G;
    if (!lg_undo(&G)) return 0;
    calm();
    moved(&before);
    return 1;
}

static void restart(void)
{
    static lg_game before;
    before = G;
    lg_restart(&G);
    calm();
    moved(&before);
}

/* the notation (logicgates.h); the hint's here (shown blinking), the moves in the rules */
static int script(const char *moves)
{
    char tok[32];
    int ok = 1;
    for (const char *s = moves; *s && ok;) {
        if (*s == ' ') { s++; continue; }
        int n = 0;
        while (s[n] && s[n] != ' ' && n < 31) n++;
        memcpy(tok, s, (size_t)n);
        tok[n] = 0;
        s += n;
        if (tok[0] == 'h' && !tok[1]) {
            ok = lg_hint(&G, &hint.m);
            hint.on = ok;
            hint.t = ui_ms();
            G.hinted |= ok;
        } else {
            ok = lg_script(&G, tok);
            hint.on = 0;
        }
    }
    t0 = 0;
    flip_node = -1;
    spark.wire = -1;
    if (lg_solved(&G)) solved_t = ui_ms() - BURST_MS;
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static const uint8_t TRACE[3] = {98, 80, 44};

static Uint32 now_ms;

static int arrived(int node)                    /* the change has reached this part */
{
    return !t0 || now_ms - t0 >= (Uint32)(G.L.depth[node] * STEP_MS);
}

static float front(int w)                       /* how far along the wire the change has run */
{
    const lg_wire *W = &G.L.w[w];
    if (!t0) return W->len + 1;
    float f = ((float)(now_ms - t0) - G.L.depth[W->from] * STEP_MS) / STEP_MS;
    return clampf(f, 0, 1) * (W->len + 0.01f);
}

static int lit_at(int w, float s, float f)
{
    const lg_wire *W = &G.L.w[w];
    int fresh = s <= f;
    float fs = W->cuttable ? W->fs : W->len + 1;
    if (s < fs) return fresh ? G.wsrc[w] : old_src[w];
    return fresh ? G.wend[w] : old_end[w];
}

/* a straight stretch of trace (px): its glow (layer 0), its dark edge (1), its brass or gold (2) */
static void trace(float x0, float y0, float x1, float y1, int lit, int layer)
{
    float t = tw, xa = fminf(x0, x1) - t / 2, ya = fminf(y0, y1) - t / 2, w = fabsf(x1 - x0) + t,
          h = fabsf(y1 - y0) + t;
    if (layer == 0) {
        float g = 1.0f * t;
        ui_fill(xa - g, ya - g, w + 2 * g, h + 2 * g, 255, 186, 60, 24);
        g = 0.45f * t;
        ui_fill(xa - g, ya - g, w + 2 * g, h + 2 * g, 255, 196, 70, 46);
    } else if (layer == 1) {
        ui_fill(xa - 1, ya - 1, w + 2, h + 2, 6, 18, 12, 230);
    } else {
        const uint8_t *c = lit ? GOLD : TRACE;
        ui_fill(xa, ya, w, h, c[0], c[1], c[2], 255);
        if (lit) {                              /* a bright line down its middle */
            int vert = fabsf(x1 - x0) < fabsf(y1 - y0);
            if (vert) ui_fill(xa + 0.35f * t, ya, 0.3f * t, h, 255, 242, 196, 150);
            else ui_fill(xa, ya + 0.35f * t, w, 0.3f * t, 255, 242, 196, 150);
        }
    }
}

/* the part of a wire from arc a to arc b: its vertical (vert) or horizontal stretches */
static void path(const lg_wire *W, float a, float b, int vert, int lit, int layer)
{
    float s = 0;
    for (int i = 0; i + 1 < W->npt; i++) {
        float x0 = W->px[i], y0 = W->py[i], x1 = W->px[i + 1], y1 = W->py[i + 1];
        float len = fabsf(x1 - x0) + fabsf(y1 - y0);
        if (len < 1e-4f) continue;
        int v = fabsf(x1 - x0) < 1e-4f;
        float lo = fmaxf(a, s), hi = fminf(b, s + len);
        if (v == vert && hi > lo) {
            float f0 = (lo - s) / len, f1 = (hi - s) / len;
            trace(PX(x0 + (x1 - x0) * f0), PY(y0 + (y1 - y0) * f0), PX(x0 + (x1 - x0) * f1), PY(y0 + (y1 - y0) * f1),
                  lit, layer);
        }
        s += len;
    }
}

/* a wire's pieces by value: cut at its fuse and at the change's front */
static int pieces(int w, float *a, float *b, int *lit)
{
    const lg_wire *W = &G.L.w[w];
    float cuts[4] = {0, 0, 0, 0}, f = front(w), fs = W->cuttable ? W->fs : -1;
    int n = 1;
    if (fs > 0 && fs < W->len) cuts[n++] = fs;
    if (f > 0 && f < W->len) cuts[n++] = f;
    cuts[n++] = W->len;
    if (n == 4 && cuts[1] > cuts[2]) {
        float t = cuts[1];
        cuts[1] = cuts[2];
        cuts[2] = t;
    }
    int k = 0;
    for (int i = 0; i + 1 < n; i++) {
        if (cuts[i + 1] - cuts[i] < 1e-4f) continue;
        a[k] = cuts[i];
        b[k] = cuts[i + 1];
        lit[k++] = lit_at(w, (cuts[i] + cuts[i + 1]) / 2, f);
    }
    return k;
}

static void draw_wires(void)
{
    float a[4], b[4];
    int lit[4];
    /* the glows; then the horizontal stretches, their edges, dark then lit; then the vertical ones over them */
    for (int w = 0; w < G.L.nw; w++) {
        int k = pieces(w, a, b, lit);
        for (int i = 0; i < k; i++)
            if (lit[i])
                for (int vert = 0; vert < 2; vert++) path(&G.L.w[w], a[i], b[i], vert, 1, 0);
    }
    for (int vert = 0; vert < 2; vert++)
        for (int on = 0; on < 2; on++)
            for (int layer = 1; layer < 3; layer++)
                for (int w = 0; w < G.L.nw; w++) {
                    int k = pieces(w, a, b, lit);
                    for (int i = 0; i < k; i++)
                        if (lit[i] == on) path(&G.L.w[w], a[i], b[i], vert, on, layer);
                }
    /* the corners and the branchings: round */
    float d = 1.55f * tw;
    for (int on = 0; on < 2; on++)
        for (int w = 0; w < G.L.nw; w++) {
            const lg_wire *W = &G.L.w[w];
            if (W->npt < 4) continue;
            float f = front(w), s1 = fabsf(W->py[1] - W->py[0]), s2 = s1 + fabsf(W->px[2] - W->px[1]);
            for (int c = 1; c <= 2; c++) {
                if (lit_at(w, c == 1 ? s1 : s2, f) != on) continue;
                const uint8_t *col = on ? GOLD : TRACE;
                SDL_SetTextureColorMod(t_dot, col[0], col[1], col[2]);
                ui_put_c(t_dot, PX(W->px[c]), PY(W->py[c]), d, d);
            }
        }
    SDL_SetTextureColorMod(t_dot, 255, 255, 255);
}

static void draw_fuses(void)
{
    for (int w = 0; w < G.L.nw; w++) {
        const lg_wire *W = &G.L.w[w];
        if (!W->cuttable) continue;
        int broken = G.cut[w] && !(spark.wire == w && now_ms - spark.t < 90);
        ui_put_rot(t_fuse[broken], PX(W->fx), PY(W->fy), fuse_l, fuse_t, W->fvert ? 90 : 0);
    }
}

static void sparks(void)
{
    if (spark.wire < 0) return;
    float q = (now_ms - spark.t) / (float)SPARK_MS;
    if (q >= 1) { spark.wire = -1; return; }
    const lg_wire *W = &G.L.w[spark.wire];
    float cx = PX(W->fx), cy = PY(W->fy), m = fminf(cw, ch);
    if (q < 0.3f) ui_mark_gold(cx, cy, m * (0.5f + 1.5f * q));
    for (int k = 0; k < SPARKS; k++) {
        float a = k * 6.2831853f / SPARKS + 0.4f * sinf(k * 7.1f + spark.wire);
        float sp = 0.35f + 0.25f * (k * 37 % 7) / 7.0f;
        float r = m * sp * puz_ease(q * 1.3f), x = cx + r * cosf(a), y = cy + r * sinf(a) + m * 0.25f * q * q;
        float s = fmaxf(3, 1.4f * tw * (1 - 0.7f * q));
        SDL_SetTextureColorMod(t_dot, 255, (Uint8)(250 - 90 * q), (Uint8)(200 - 160 * q));
        SDL_SetTextureAlphaMod(t_dot, (Uint8)(255 * (1 - q)));
        ui_put_c(t_dot, x, y, s, s);
    }
    SDL_SetTextureColorMod(t_dot, 255, 255, 255);
    SDL_SetTextureAlphaMod(t_dot, 255);
    ui_animate();
}

static void glow(float x, float y, float d, float a)
{
    SDL_SetTextureAlphaMod(t_glow, (Uint8)clampf(255 * a, 0, 255));
    ui_put_c(t_glow, x, y, d, d);
    SDL_SetTextureAlphaMod(t_glow, 255);
}

/* a switch's or a route switch's box on the screen (where a tap flips it) */
static void node_box(int i, float *x0, float *y0, float *x1, float *y1)
{
    const lg_node *N = &G.L.n[i];
    float cx = PX(N->col + 0.5f);
    if (N->kind == LG_SWITCH) {
        *x0 = cx - sw_w / 2;
        *x1 = cx + sw_w / 2;
        *y0 = PY(LG_NH);
        *y1 = *y0 + sw_h;
    } else {
        *x0 = cx - pw / 2;
        *x1 = cx + pw / 2;
        *y0 = PY(N->row + LG_NH);
        *y1 = PY(N->row);
    }
}

static void draw_node(int i)
{
    const lg_node *N = &G.L.n[i];
    float cx = PX(N->col + 0.5f), yt = PY(N->row + LG_NH), yb = PY(N->row);
    int out = arrived(i) ? G.out[i][0] : old_out[i][0];
    switch (N->kind) {
    case LG_SWITCH: {
        int on = G.val[i];
        float d = 0.66f * sw_w, p = (float)on;
        if (on) glow(cx, yt + 0.35f * sw_h, 1.6f * sw_w, 0.5f);
        ui_shadow_flat(cx + 0.04f * sw_w, yt + sw_h / 2 + 0.05f * sw_w, sw_w, sw_h, 0.5f);
        ui_put(t_switch[on], cx - sw_w / 2, yt, sw_w, sw_h);
        if (flip_node == i && now_ms - flip_t < FLIP_MS) {
            float q = puz_ease((now_ms - flip_t) / (float)FLIP_MS);
            p = on ? q : 1 - q;
            ui_animate();
        }
        float ky = yt + sw_h * (0.71f - 0.42f * p);
        ui_shadow_flat(cx + 0.05f * d, ky + 0.08f * d, d, d, 0.6f);
        ui_put_c(t_knob, cx, ky, d, d);
        if (N->locked) ui_put_c(t_lock, cx, yt + 0.5f * sw_h, floorf(sw_w * 1.12f), floorf(sw_w * 0.42f));
        break;
    }
    case LG_LAMP:
    case LG_ALARM: {
        int a = N->kind == LG_ALARM;
        float gy = yb - 0.62f * lamp_h;
        if (out && !a) glow(cx, gy, 1.9f * lamp_w, 0.85f);
        if (out && a) ui_mark_glow(cx, gy, 1.7f * lamp_w);
        ui_put(t_lamp[a][out], cx - lamp_w / 2, yb - lamp_h, lamp_w, lamp_h);
        break;
    }
    case LG_ROUTE: {
        int in = arrived(i) ? G.wend[N->in[0]] : old_end[N->in[0]];
        ui_put(t_plaque, cx - pw / 2, yt, pw, ph);
        if (flip_node == i && now_ms - flip_t < FLIP_MS) {
            float q = puz_ease((now_ms - flip_t) / (float)FLIP_MS);
            SDL_Texture *o = t_route[!G.val[i]][in], *n = t_route[G.val[i]][in];
            SDL_SetTextureAlphaMod(o, (Uint8)(255 * (1 - q)));
            ui_put(o, cx - pw / 2, yt, pw, ph);
            SDL_SetTextureAlphaMod(o, 255);
            SDL_SetTextureAlphaMod(n, (Uint8)(255 * q));
            ui_put(n, cx - pw / 2, yt, pw, ph);
            SDL_SetTextureAlphaMod(n, 255);
            ui_animate();
        } else {
            ui_put(t_route[G.val[i]][in], cx - pw / 2, yt, pw, ph);
        }
        break;
    }
    default: {
        ui_put(t_plaque, cx - pw / 2, yt, pw, ph);
        float top = yt + 0.84f * gate_h;          /* just into the shape */
        for (int k = 0; k < N->nin; k++) {      /* the inputs' leads, up into the shape */
            int w = N->in[k];
            const lg_wire *W = &G.L.w[w];
            int lit = lit_at(w, W->len, front(w));
            float x = PX(W->px[W->npt - 1]);
            trace(x, yb, x, top, lit, 1);
            trace(x, yb, x, top, lit, 2);
        }
        int w3 = N->nin == 3;
        ui_put(t_gate[N->kind][out][w3], cx - gate_w[w3] / 2, yt, gate_w[w3], gate_h);
    }
    }
}

static void hint_mark(void)
{
    if (!hint.on) return;
    Uint32 dt = now_ms - hint.t;
    if (dt < HINT_MS) ui_animate();
    if (dt < HINT_MS && dt / 300 % 2) return;  /* it blinks, then stays lit until the next move */
    float x, y, d = 1.3f * fminf(cw, ch);
    if (hint.m.kind == LG_MCUT) {
        const lg_wire *W = &G.L.w[G.L.cut[hint.m.idx]];
        x = PX(W->fx);
        y = PY(W->fy);
        d *= 0.8f;
    } else {
        int i = hint.m.kind == LG_MSWITCH ? G.L.sw[hint.m.idx] : G.L.rt[hint.m.idx];
        float x0, y0, x1, y1;
        node_box(i, &x0, &y0, &x1, &y1);
        x = (x0 + x1) / 2;
        y = (y0 + y1) / 2;
    }
    ui_mark_gold(x, y, d);
}

static void burst(void)
{
    if (!lg_solved(&G)) return;
    int dt = (int)(now_ms - solved_t);
    if (t0 && dt < 0) return;
    float q = t0 ? clampf(dt / (float)BURST_MS, 0, 1) : 1;
    for (int i = 0; i < G.L.nn; i++) {
        const lg_node *N = &G.L.n[i];
        if (N->kind != LG_LAMP) continue;
        float cx = PX(N->col + 0.5f), cy = PY(N->row) - 0.62f * lamp_h;
        glow(cx, cy, lamp_w * (2.2f + 0.8f * sinf(3.14159f * fminf(1, q * 1.4f))), 1);
        if (q >= 1) continue;
        glow(cx, cy, lamp_w * (1.5f + 4.5f * puz_ease(q)), 1 - q);
        for (int k = 0; k < 16; k++) {          /* the sparks flying out */
            float a = k * 6.2831853f / 16 + 0.2f * (k & 1), r = lamp_w * (0.5f + (1.4f + 0.5f * (k % 3)) * puz_ease(q));
            float s = fmaxf(2, 0.12f * lamp_w * (1 - q));
            SDL_SetTextureColorMod(t_dot, 255, 226, 130);
            SDL_SetTextureAlphaMod(t_dot, (Uint8)(255 * (1 - q)));
            ui_put_c(t_dot, cx + r * cosf(a), cy + r * sinf(a), s, s);
        }
        SDL_SetTextureColorMod(t_dot, 255, 255, 255);
        SDL_SetTextureAlphaMod(t_dot, 255);
        ui_animate();
    }
}

static void draw_tools(void)
{
    float u = ui.unit;
    for (int i = 0; i < 2; i++) {
        if (i == T_RESET && deny_reset && now_ms - (Uint32)deny_reset < HINT_MS) {
            if ((now_ms - (Uint32)deny_reset) / 300 % 2 == 0) ui_mark_gold(tbx[i], tby[i], 3 * tbr);
            ui_animate();
        }
        ui_put_c(t_disc, tbx[i], tby[i], 2 * tbr, 2 * tbr);
        ui_put_c(i == T_HINT ? t_bulb : t_tool, tbx[i], tby[i], 1.3f * tbr, 1.3f * tbr);
    }
    /* the level: its chapter's emblem, its number in the chapter */
    int n = G.level % LG_PER_CHAPTER + 1, c = G.level / LG_PER_CHAPTER;
    float dh = 0.55f * u, nw = puz_number_w(n, dh), ew = 0.7f * u;
    if (wide) {
        ui_put_c(t_emblem[c], labx, laby - 0.3f * u, ew, ew);
        puz_number(&font, labx, laby + 0.45f * u, dh, n, PUZ_CREAM, 255);
    } else {
        float x0 = labx - (ew + 0.15f * u + nw) / 2;
        ui_put_c(t_emblem[c], x0 + ew / 2, laby, ew, ew);
        puz_number(&font, x0 + ew + 0.15f * u + nw / 2, laby, dh, n, PUZ_CREAM, 255);
    }
}

static void draw(void)
{
    now_ms = ui_ms();
    if (made_shared < 0) layout(B);
    ui_put(t_board, panel.x, panel.y, panel.w, panel.h);
    draw_wires();
    draw_fuses();
    for (int i = 0; i < G.L.nn; i++) draw_node(i);
    sparks();
    burst();
    hint_mark();
    draw_tools();
    if (t0 && now_ms - t0 < (Uint32)((G.L.maxdepth + 1) * STEP_MS)) ui_animate();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void do_move(lg_move m, int node, int wire)
{
    static lg_game before;
    before = G;
    if (!lg_play(&G, m)) { ui_sound(SND_DENY); return; }
    hint.on = 0;
    deny_reset = 0;
    moved(&before);
    if (m.kind == LG_MCUT) {
        spark.wire = wire;
        spark.t = t0;
        ui_sound(SND_CAPTURE);
    } else {
        flip_node = node;
        flip_t = t0;
        ui_sound(m.kind == LG_MSWITCH ? SND_MOVE : SND_PLACE);
    }
}

static void press(float x, float y)
{
    for (int i = 0; i < 2; i++) {
        if (hypotf(x - tbx[i], y - tby[i]) > 1.15f * tbr) continue;
        if (i == T_RESET) {
            if (!G.nh) { ui_sound(SND_DENY); return; }
            ui_sound(SND_TAP);
            restart();
            return;
        }
        if (lg_solved(&G)) { ui_sound(SND_DENY); return; }
        if (lg_hint(&G, &hint.m)) {
            hint.on = 1;
            hint.t = ui_ms();
            G.hinted = 1;
            deny_reset = 0;
            ui_sound(SND_TAP);
        } else {
            deny_reset = (int)(ui_ms() | 1);
            ui_sound(SND_DENY);
        }
        ui_animate();
        return;
    }
    if (lg_solved(&G)) return;                  /* the lamps flaring; then the shell's card (the next level) */
    /* a part's own box first, then the fuses, then a part's surroundings */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < G.L.nn; i++) {
            int k = G.L.n[i].kind;
            if (k != LG_SWITCH && k != LG_ROUTE) continue;
            float x0, y0, x1, y1, g = (pass ? 0.12f : 0.02f) * cw;
            node_box(i, &x0, &y0, &x1, &y1);
            if (x < x0 - g || x > x1 + g || y < y0 - g || y > y1 + g) continue;
            int n = 0;
            if (k == LG_SWITCH) {
                while (G.L.sw[n] != i) n++;
                if (G.L.n[i].locked) { ui_sound(SND_DENY); return; }
                do_move((lg_move){LG_MSWITCH, (uint8_t)n}, i, -1);
            } else {
                while (G.L.rt[n] != i) n++;
                do_move((lg_move){LG_MROUTE, (uint8_t)n}, i, -1);
            }
            return;
        }
        if (pass) break;
        int best = -1;
        float bd = 0.36f * fminf(cw, ch);
        for (int n = 0; n < G.L.ncut; n++) {
            const lg_wire *W = &G.L.w[G.L.cut[n]];
            float d = hypotf(x - PX(W->fx), y - PY(W->fy));
            if (d < bd) { bd = d; best = n; }
        }
        if (best >= 0) { do_move((lg_move){LG_MCUT, (uint8_t)best}, -1, G.L.cut[best]); return; }
    }
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the bar: the moves, the par (three stars), the cuts left ---------------------------------------------------- */
static SDL_Texture *t_bar_star, *t_bar_fuse;

static void bar(int player, box b)
{
    (void)player;
    float u = ui.unit, cy = b.y + b.h / 2, dh = 0.46f * u;
    if (made_bar != (int)u) {
        made_bar = (int)u;
        static const uint8_t gold[3] = {255, 200, 80};
        int s = (int)(0.5f * u), fl = (int)(0.6f * u), ft = (int)(0.26f * u);
        ui_free(&t_bar_star);
        uint8_t *bf = calloc((size_t)s * s, 4);
        if (bf) { paint_star(s, gold, bf); t_bar_star = tex_buf(s, s, bf); }
        ui_free(&t_bar_fuse);
        bf = calloc((size_t)fl * ft, 4);
        if (bf) { paint_fuse(0, fl, ft, bf); t_bar_fuse = tex_buf(fl, ft, bf); }
    }
    puz_bar_moves(b, NULL, G.nh);
    float x = b.x + b.w, nw = puz_number_w(G.par, dh);   /* from the right: the par, the cuts left */
    ui_number(x - nw / 2 - 0.05f * u, cy, dh, G.par, 1);
    x -= nw + 0.2f * u;
    for (int k = b.w < 5 * u ? 0 : 2; k >= 0; k--) {   /* three stars; one in a narrow bar */
        ui_put_c(t_bar_star, x - 0.2f * u, cy + (k == 1 ? -0.08f : 0.04f) * u, 0.4f * u, 0.4f * u);
        x -= 0.3f * u;
    }
    if (G.L.ncut) {
        x -= 0.5f * u;
        int left = G.L.cut_limit - lg_cuts(&G);
        float lw = puz_number_w(left, dh);
        ui_number(x - lw / 2, cy, dh, left, 1);
        x -= lw + 0.15f * u;
        ui_put_c(t_bar_fuse, x - 0.3f * u, cy, 0.6f * u, 0.26f * u);
    }
}

/* ---- the menu's card: two switches on, an AND, its lamp lit ------------------------------------------------------ */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    paint_board(size, size, (int)(0.045f * size), rgba);
    float S = (float)size, t = fmaxf(2, 0.035f * S), gy = 0.4f, gh = 0.2f, track = 0.66f;
    const float sx[2] = {0.27f, 0.73f};
    for (int k = 0; k < 2; k++) {               /* the wires: up from the switches, across, into the gate */
        float x = sx[k] * S, xi = (0.5f + (k ? 0.07f : -0.07f)) * S;
        puz_ink_rect(rgba, size, size, x - t / 2, track * S - t / 2, x + t / 2, 0.76f * S, GOLD, 1);
        puz_ink_rect(rgba, size, size, fminf(x, xi) - t / 2, track * S - t / 2, fmaxf(x, xi) + t / 2, track * S + t / 2,
                     GOLD, 1);
        puz_ink_rect(rgba, size, size, xi - t / 2, (gy + gh / 2) * S - 2, xi + t / 2, track * S + t / 2, GOLD, 1);
    }
    puz_ink_rect(rgba, size, size, 0.5f * S - t / 2, 0.26f * S, 0.5f * S + t / 2, (gy - gh / 2) * S + 2, GOLD, 1);
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    int lw2 = (int)(0.3f * S), lh2 = (int)(0.3f * S);
    art_disc(lw2 * 2, (const uint8_t[3]){255, 200, 80}, 0.7f, 1.6f, b);
    art_blend(rgba, size, size, b, lw2 * 2, lw2 * 2, (int)(0.5f * S - lw2), (int)(0.17f * S - lw2));
    memset(b, 0, (size_t)size * size * 4);
    paint_lamp(0, 1, lw2, lh2, b);
    art_blend(rgba, size, size, b, lw2, lh2, (int)(0.5f * S - lw2 / 2.0f), (int)(0.27f * S - lh2));
    int pw2 = (int)(0.3f * S), ph2 = (int)(gh * S);
    memset(b, 0, (size_t)size * size * 4);
    paint_walnut(b, pw2, ph2, 1);
    art_blend(rgba, size, size, b, pw2, ph2, (int)(0.5f * S - pw2 / 2.0f), (int)((gy - gh / 2) * S));
    int gw = (int)(0.9f * pw2), ghh = (int)(0.88f * ph2);
    memset(b, 0, (size_t)size * size * 4);
    paint_gate(LG_AND, 1, gw, ghh, t, b);
    art_blend(rgba, size, size, b, gw, ghh, (int)(0.5f * S - gw / 2.0f), (int)((gy - gh / 2) * S));
    int sw = (int)(0.16f * S), sh = (int)(0.2f * S), kd = (int)(0.66f * sw);
    uint8_t *k = calloc((size_t)kd * kd, 4);
    for (int i = 0; i < 2; i++) {
        memset(b, 0, (size_t)size * size * 4);
        paint_switch(1, sw, sh, b);
        art_blend(rgba, size, size, b, sw, sh, (int)(sx[i] * S - sw / 2.0f), (int)(0.75f * S));
        if (k) {
            memset(k, 0, (size_t)kd * kd * 4);
            art_counter(COUNTER_DISC, 0, kd, k);
            art_blend(rgba, size, size, k, kd, kd, (int)(sx[i] * S - kd / 2.0f),
                      (int)(0.75f * S + 0.29f * sh - kd / 2.0f));
        }
    }
    free(k);
    free(b);
}

static const game logicgates_game = {
    .id = "logicgates", .order = 370, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .levels = LG_LEVELS, .chapters = LG_CHAPTERS, .chapter_icon = paint_emblem, .level_stars = level_stars,
};
REGISTER_GAME(logicgates_game)
