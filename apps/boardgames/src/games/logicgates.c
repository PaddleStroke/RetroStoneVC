/*
 * Logic Gates in RetroStone Board, for one player (the rules: logicgates_rules.c, logicgates.h): a circuit board of
 * dark green in a walnut frame, brass traces on it, the gates as brass shapes inlaid in walnut plaques (the classic
 * shapes, each with its name engraved: NOT, AND, OR, XOR, NAND, NOR, in English as on any circuit drawing; the owner
 * asked for them), each switch right under the gate it feeds, the lamps at the top; the parts as big as the room
 * allows. A wire carrying 1 glows gold; after each move the change runs up the wires, gate after gate. A cuttable wire
 * is a jumper cable over the board, coloured, bowing between two brass terminals. Solved: the lamps flare in a gold
 * burst, then the shell's end card (the stars, the moves beside the par, the next level).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The levels: the shell's level screen (a tab for each chapter with its emblem, the chapter's levels as tiles: 40 in
 * the first, 25 in the others; the shell keeps the stars won); its new game goes back to it. The die tab after the
 * chapters: random play, opening on its setup screen (the size, a toggle for each part the circuits may use, play);
 * then circuits made at random of those parts (lg_random). The board takes all the room between the bars;
 * a deep circuit in a wide room lies on its side (the switches on the left) when its parts come out bigger so. Its
 * tools are the shell's (game.tools: in the bar's row standing, in the side column lying): restart, the hint (one part
 * to change blinks). The bar: the level (its chapter's emblem, its number; lying: at the top of the side column), the
 * moves made (a switch), the cuts left (a cable), the par (a star). A level opens at once: its drawing is stored with
 * it, and the parts' pictures are painted quickly, only the kinds shown.
 * Tap a switch or a route switch to flip it, a cable to snip it (a spark, its halves spring apart); undo takes a move
 * back (a cut cable joins up again).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/logicgates.h"
#include "games/puzzles.h"
#include "shell.h"
#include "text.h"

#define STEP_MS 90                              /* the signal from one part to the next */
#define FLIP_MS 130
#define CUT_MS 720                              /* a cable cut: the snip, the halves springing apart */
#define MEND_MS 360                             /* mended (undo): the halves back together */
#define BURST_MS 1200
#define HINT_MS 2400

#define SPARKS 12

static lg_game G;

/* random play's setup screen (below: the die tab's choice) */
static int setup;                               /* it shows (instead of a circuit) */
static void setup_layout(void);
static void setup_draw(void);
static void setup_press(float x, float y);
static void setup_open(void);
static void paint_setup_icon(int kind, int size, uint8_t *rgba);

/* the animations */
static Uint32 t0;                               /* the last move: its change runs up from there */
static uint8_t old_src[LG_WIRES], old_end[LG_WIRES], old_out[LG_NODES][2], old_val[LG_NODES];
static Uint32 flip_t;                           /* a switch or route switch moving */
static int flip_node = -1;
static struct { int wire; Uint32 t; int mend; } snip = {-1, 0, 0};   /* a cable being cut, or mended (undo) */
static struct { int on; lg_move m; Uint32 t; } hint;
static int deny_reset;                          /* the hint found nothing: the restart button blinks */
static Uint32 solved_t;                         /* when the lamps light up (the burst) */

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static box B, panel;                            /* the game's room; the framed board */
static int wide, fr;                            /* lying (landscape): the level's number in the shell's column */
static float ox, oy, cw, ch, tw;                /* grid (x, y) at (ox + x cw, oy - y ch); the traces' width */

enum { T_RESET, T_HINT, T_SETUP, TOOLS };       /* the shell's tools (game.tools): restart, hint; random play's
                                                 * setup screen */
static int tools_now(void) { return setup ? 0 : G.level >= 0 ? T_SETUP : TOOLS; }

static float PX(float x) { return ox + x * cw; }
/* the gaps between rows stretched (gs times) when the room is taller than the parts need; the parts keep their size */
static float gs = 1;
static float stretched(float y)
{
    float extra = 0;
    for (int k = 0; gs > 1 && k + 1 < G.L.layers; k++) {
        float g0 = G.L.ly[k] + LG_NH, g1 = G.L.ly[k + 1];
        if (y <= g0) break;
        extra += (fminf(y, g1) - g0) * (gs - 1);
    }
    return y + extra;
}
static float PY(float y) { return oy - stretched(y) * ch; }

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

/* ---- a quick painter for the parts' figures ----------------------------------------------------------------------
 * The shell's art_render takes 16 samples a pixel against every edge of every shape: a level's parts took most of a
 * second to paint (seconds on a phone), and every level painted them anew. The same figures (polygons, ellipses; a
 * part with its dark edge `line` wide, marks) and the same shading, painted here with one sample a pixel: its
 * coverage from its distance to the edge, only the edges near the pixel looked at. */
#define FIG_SHAPES 24
#define FIG_PTS 640

typedef struct { int ell, role, p0, n; float cx, cy, rx, ry, x0, y0, x1, y1; } fig_shape;
typedef struct { int ns, np; fig_shape s[FIG_SHAPES]; float x[FIG_PTS], y[FIG_PTS]; } fig;

static fig *fig_new(void) { return calloc(1, sizeof(fig)); }
static void fig_free(fig *f) { free(f); }

static fig_shape *fig_add(fig *f, int role)
{
    if (!f || f->ns >= FIG_SHAPES) return NULL;
    fig_shape *s = &f->s[f->ns++];
    memset(s, 0, sizeof *s);
    s->role = role;
    s->p0 = f->np;
    s->x0 = s->y0 = 1e9f;
    s->x1 = s->y1 = -1e9f;
    return s;
}

static void fig_poly(fig *f, int role, const float *p, int n)
{
    fig_shape *s = fig_add(f, role);
    if (!s) return;
    for (int i = 0; i < n && f->np < FIG_PTS; i++) {
        f->x[f->np] = p[2 * i];
        f->y[f->np++] = p[2 * i + 1];
        s->x0 = fminf(s->x0, p[2 * i]);
        s->x1 = fmaxf(s->x1, p[2 * i]);
        s->y0 = fminf(s->y0, p[2 * i + 1]);
        s->y1 = fmaxf(s->y1, p[2 * i + 1]);
        s->n++;
    }
    if (s->n < 3) f->ns--;
}

static void fig_rect(fig *f, int role, float x0, float y0, float x1, float y1)
{
    const float p[8] = {x0, y0, x1, y0, x1, y1, x0, y1};
    fig_poly(f, role, p, 4);
}

static void fig_ellipse(fig *f, int role, float cx, float cy, float rx, float ry)
{
    fig_shape *s = fig_add(f, role);
    if (!s) return;
    s->ell = 1;
    s->cx = cx;
    s->cy = cy;
    s->rx = fmaxf(rx, 0.01f);
    s->ry = fmaxf(ry, 0.01f);
    s->x0 = cx - rx;
    s->x1 = cx + rx;
    s->y0 = cy - ry;
    s->y1 = cy + ry;
}

/* the figure over a clear w x h picture: each shape over the ones before (a part filled and shaded, lit from the upper
 * left, its edge in the outer colour; a mark in its colour) */
static void fig_render(const fig *f, const art_style *st, int w, int h, uint8_t *out)
{
    float *acc = calloc((size_t)w * h, 4 * sizeof(float));
    if (!acc || !f) { free(acc); return; }
    float wl = st->line, reach = wl + 1;
    static int near[FIG_PTS];
    static float cross[FIG_PTS];
    for (int k = 0; k < f->ns; k++) {
        const fig_shape *s = &f->s[k];
        int mark = s->role == ART_MARK;
        int xa = (int)fmaxf(0, floorf(s->x0 - reach)), xb = (int)fminf(w - 1, ceilf(s->x1 + reach));
        int ya = (int)fmaxf(0, floorf(s->y0 - reach)), yb = (int)fminf(h - 1, ceilf(s->y1 + reach));
        float cx = (s->x0 + s->x1) * 0.5f, hw = (s->x1 - s->x0) * 0.5f + 1e-3f, hh = fmaxf(1, s->y1 - s->y0);
        for (int py = ya; py <= yb; py++) {
            float y = py + 0.5f;
            int nn = 0, nc = 0;
            if (!s->ell)                        /* the edges near this row; where they cross it */
                for (int i = 0, j = s->n - 1; i < s->n; j = i++) {
                    float ax = f->x[s->p0 + j], ay = f->y[s->p0 + j], bx = f->x[s->p0 + i], by = f->y[s->p0 + i];
                    if (fminf(ay, by) - reach <= y && fmaxf(ay, by) + reach >= y) near[nn++] = j;
                    if ((by > y) != (ay > y)) cross[nc++] = (ax - bx) * (y - by) / (ay - by) + bx;
                }
            for (int a = 1; a < nc; a++)
                for (int b = a; b > 0 && cross[b - 1] > cross[b]; b--) {
                    float t = cross[b];
                    cross[b] = cross[b - 1];
                    cross[b - 1] = t;
                }
            int ci = 0;
            for (int px = xa; px <= xb; px++) {
                float x = px + 0.5f, d;
                if (s->ell) {
                    float u = (x - s->cx) / s->rx, v = (y - s->cy) / s->ry, l = sqrtf(u * u + v * v);
                    if (l < 1e-4f) d = -fminf(s->rx, s->ry);
                    else d = (l - 1) / (sqrtf(u * u / (s->rx * s->rx) + v * v / (s->ry * s->ry)) / l);
                } else {
                    while (ci < nc && cross[ci] < x) ci++;
                    int in = ci & 1;
                    float d2 = 1e9f;
                    for (int q = 0; q < nn; q++) {
                        int j = near[q], i = j + 1 == s->n ? 0 : j + 1;
                        float ax = f->x[s->p0 + j], ay = f->y[s->p0 + j], bx = f->x[s->p0 + i], by = f->y[s->p0 + i];
                        if (x < fminf(ax, bx) - reach || x > fmaxf(ax, bx) + reach) continue;
                        float ex = ax - bx, ey = ay - by, wx = x - bx, wy = y - by;
                        float t = clampf((wx * ex + wy * ey) / (ex * ex + ey * ey + 1e-12f), 0, 1);
                        float dx = wx - ex * t, dy = wy - ey * t;
                        d2 = fminf(d2, dx * dx + dy * dy);
                    }
                    d = d2 > 1e8f ? (in ? -1e4f : 1e4f) : in ? -sqrtf(d2) : sqrtf(d2);
                }
                float cf = clampf(0.5f - d, 0, 1), ct = mark ? cf : clampf(0.5f - (d - wl), 0, 1);
                if (ct <= 0) continue;
                float c[3];
                if (mark) {
                    for (int i = 0; i < 3; i++) c[i] = st->mark[i] * cf;
                } else {
                    float t = (x - cx) / hw + 0.35f;
                    float l = 1.0f + st->light * (0.55f - t * t) - st->light * 0.45f * (y - s->y0) / hh;
                    for (int i = 0; i < 3; i++) c[i] = clampf(st->fill[i] * l, 0, 255) * cf + st->outer[i] * (ct - cf);
                }
                float *o = acc + 4 * ((size_t)py * w + px);
                for (int i = 0; i < 3; i++) o[i] = c[i] + o[i] * (1 - ct);
                o[3] = ct + o[3] * (1 - ct);
            }
        }
    }
    for (int i = 0; i < w * h; i++) {
        float *a = acc + 4 * (size_t)i;
        uint8_t *o = out + 4 * (size_t)i;
        if (a[3] <= 1e-4f) { memset(o, 0, 4); continue; }
        for (int k = 0; k < 3; k++) o[k] = (uint8_t)clampf(a[k] / a[3] + 0.5f, 0, 255);
        o[3] = (uint8_t)clampf(255 * a[3] + 0.5f, 0, 255);
    }
    free(acc);
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

/* a place along a side of n pixels on a small square board's side of s: the ends as they are, the middle folded back
 * and forth over a stretch of the small board's side (whole folds: it ends where the end begins) */
static int fold(int x, int n, int s)
{
    int h = s / 2, mid = n - 2 * h;
    if (x < h || mid <= 0) return x < h ? x : s - (n - x);
    if (x >= n - h) return s - (n - x);
    float d = (float)(x - h), q = s / 4.0f;
    int k = (int)ceilf(mid / (2 * q));
    float hp = mid / (2.0f * k), ph = fmodf(d, 2 * hp), tri = ph < hp ? ph : 2 * hp - ph;
    int p = h - (int)tri;
    return p < 0 ? 0 : p >= s ? s - 1 : p;
}

/* the board: the house's walnut frame round a dark green circuit board. The frame comes from a small square board,
 * its sides folded along the long ones; the green is worked out row by row (its light and grain split in x and y). */
static void paint_board(int W, int H, int f, uint8_t *o)
{
    int L = W > H ? W : H, S = 4 * f + 96 < L ? 4 * f + 96 : L;
    uint8_t *sq = malloc((size_t)S * S * 4);
    float *ax = malloc(sizeof(float) * 2 * (size_t)W), *ay = malloc(sizeof(float) * 2 * (size_t)H);
    if (!sq || !ax || !ay) { free(sq); free(ax); free(ay); return; }
    art_board_n(1, 1, S - 2 * f, f, 0, sq);
    for (int x = 0; x < W; x++) {
        float ix = (float)(x - f) / (W - 2 * f);
        ax[2 * x] = 1 - fabsf(ix - 0.5f);
        ax[2 * x + 1] = sinf(x * 0.9f);
    }
    for (int y = 0; y < H; y++) {
        float iy = (float)(y - f) / (H - 2 * f);
        ay[2 * y] = 1 - fabsf(iy - 0.5f);
        ay[2 * y + 1] = sinf(y * 0.83f);
    }
    float sh = 0.022f * fminf(W, H);
    for (int y = 0; y < H; y++) {
        int sy = fold(y, H, S), edge_y = y < f || y >= H - f;
        for (int x = 0; x < W; x++) {
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            if (edge_y || x < f || x >= W - f) {
                memcpy(p, sq + 4 * ((size_t)sy * S + fold(x, W, S)), 4);
                continue;
            }
            float e = fminf(x - f, y - f);
            float k = 0.92f + 0.16f * ax[2 * x] * ay[2 * y];
            if (e < sh) k *= 1 - 0.4f * (1 - e / sh);   /* the frame's shadow */
            k *= 1 + 0.025f * ax[2 * x + 1] * ay[2 * y + 1];
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(PCB[i] * k, 0, 255);
            p[3] = 255;
        }
    }
    free(sq);
    free(ax);
    free(ay);
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

/* a gate's name, in English as on any circuit drawing: capitals spaced out, engraved in its brass (a dark cut, a light
 * edge under it), em pixels at most and no wider than maxw, centred on (cx, cy). The letters come from the boards' own
 * font (help/glyphs.txt keeps A to Z in every language's), so each language's build draws them alike. */
static void gate_word(uint8_t *o, int w, int h, int kind, float cx, float cy, float em, float maxw, int lit)
{
    static const char *const NAME[] = {"", "NOT", "AND", "OR", "XOR", "NAND", "NOR"};
    if (kind < LG_NOT || kind > LG_NOR) return;
    const char *s = NAME[kind];
    int n = (int)strlen(s), c = 0, x0[8], x1[8];
    uint8_t *g[8] = {0};
    static const uint8_t white[3] = {255, 255, 255};
    float total = 0;
    for (int pass = 0; pass < 2; pass++) {      /* measured; then smaller when too wide */
        c = (int)ceilf(1.6f * em) + 2;
        total = 0;
        for (int i = 0; i < n; i++) {
            free(g[i]);
            g[i] = calloc((size_t)c * c, 4);
            if (!g[i] || !text_glyph(s[i], em, 1, white, g[i], c, c)) goto done;
            x0[i] = c;
            x1[i] = -1;
            for (int y = 0; y < c; y++)
                for (int x = 0; x < c; x++)
                    if (g[i][4 * (y * c + x) + 3] > 12) {
                        if (x < x0[i]) x0[i] = x;
                        if (x > x1[i]) x1[i] = x;
                    }
            if (x1[i] < x0[i]) x0[i] = x1[i] = c / 2;
            total += x1[i] - x0[i] + 1;
        }
        total += (n - 1) * 0.14f * em;
        if (total <= maxw || pass) break;
        em *= maxw / total;
    }
    const uint8_t edge[3] = {255, 238, 176}, cut[3] = {lit ? 92 : 46, lit ? 52 : 30, lit ? 8 : 12};
    for (int layer = 0; layer < 2; layer++) {   /* the light edge a pixel low, then the cut */
        float x = cx - total / 2;
        int dy = layer ? 0 : (int)fmaxf(1, 0.06f * em);
        for (int i = 0; i < n; i++) {
            int ox = (int)lroundf(x) - x0[i], oy = (int)lroundf(cy - c / 2.0f) + dy;
            for (int y = 0; y < c; y++)
                for (int xx = 0; xx < c; xx++) {
                    int px = ox + xx, py = oy + y;
                    if (px < 0 || py < 0 || px >= w || py >= h) continue;
                    uint8_t *p = o + 4 * (py * w + px);
                    float a = g[i][4 * (y * c + xx) + 3] / 255.0f * (layer ? 0.92f : (lit ? 0.35f : 0.5f));
                    if (a <= 0 || p[3] < 128) continue;
                    const uint8_t *col = layer ? cut : edge;
                    for (int k = 0; k < 3; k++) p[k] = (uint8_t)(p[k] * (1 - a) + col[k] * a);
                }
            x += x1[i] - x0[i] + 1 + 0.14f * em;
        }
    }
done:
    for (int i = 0; i < n; i++) free(g[i]);
}

static int plain;                              /* the gates painted without their names (the emblems) */
static float name_size;                        /* the names' letters, the same on every gate (0: each its own) */

/* a name's width for letters of 1 pixel (its ink, the spaces between) */
static float word_ratio(int kind)
{
    static const char *const NAME[] = {"", "NOT", "AND", "OR", "XOR", "NAND", "NOR"};
    static float r[LG_KINDS];
    if (kind < LG_NOT || kind > LG_NOR) return 1;
    if (r[kind] > 0) return r[kind];
    const float em = 64;
    static const uint8_t white[3] = {255, 255, 255};
    int c = (int)(1.6f * em) + 2, n = (int)strlen(NAME[kind]);
    float total = (n - 1) * 0.14f * em;
    uint8_t *g = malloc((size_t)c * c * 4);
    if (!g) return 3;
    for (int i = 0; i < n; i++) {
        memset(g, 0, (size_t)c * c * 4);
        int x0 = c, x1 = -1;
        if (text_glyph(NAME[kind][i], em, 1, white, g, c, c))
            for (int y = 0; y < c; y++)
                for (int x = 0; x < c; x++)
                    if (g[4 * (y * c + x) + 3] > 12) {
                        if (x < x0) x0 = x;
                        if (x > x1) x1 = x;
                    }
        total += x1 >= x0 ? x1 - x0 + 1 : 0.6f * em;
    }
    free(g);
    return r[kind] = total / em;
}

/* where a gate's name goes on its picture and how big it may be there: its centre, its letters at most, its width at
 * most. w x h: the upright picture's size; turned, the centre is on the picture turned a quarter (h x w). Upright the
 * name lies across the body where the shape is wide: low in the NOT's triangle and in the AND's dome, the XOR's above
 * its second back line; turned, along the body. */
static void name_room(int kind, int w, int h, int turn, float *cx, float *cy, float *em, float *maxw)
{
    float xl = 0.1f * w, xr = 0.9f * w, yb = 0.94f * h, yt = 0.07f * h;
    if (kind == LG_NOT || kind == LG_NAND || kind == LG_NOR) yt += 2 * 0.085f * fminf(w, 1.6f * h);
    float body = yb - yt, f, wf;                /* how far down the body; the share of the width free there */
    switch (kind) {
    case LG_NOT: f = 0.8f; wf = 0.58f; break;
    case LG_AND: case LG_NAND: f = 0.62f; wf = 0.86f; break;
    case LG_XOR: f = 0.44f; wf = 0.76f; break;
    default: f = 0.52f; wf = 0.78f;
    }
    float y = yt + f * body;
    if (!turn) {
        *cx = 0.5f * w;
        *cy = y;
        *em = 0.42f * body;
        *maxw = wf * (xr - xl);
    } else {                                    /* along the body, in its middle; the NOT's nearer its base */
        y = yt + (kind == LG_NOT ? 0.62f : kind == LG_XOR ? 0.46f : 0.5f) * body;
        *cx = h - 1 - y;
        *cy = 0.5f * w;
        *em = fminf(0.42f * body, (kind == LG_NOT ? 0.26f : 0.34f) * (xr - xl));
        *maxw = (kind == LG_NOT ? 0.46f : kind == LG_AND || kind == LG_NAND ? 0.8f : 0.7f) * body;
    }
}

static float name_fit(int kind, int w, int h, int turn)   /* the biggest letters a gate's name can have */
{
    float cx, cy, em, maxw;
    name_room(kind, w, h, turn, &cx, &cy, &em, &maxw);
    return fminf(em, maxw / word_ratio(kind));
}

/* one size for every gate's name: the most the two-input gates allow (the NOT's triangle may need smaller) */
static float name_common(int w, int h, int turn)
{
    float em = 1e9f;
    for (int k = LG_AND; k <= LG_NOR; k++) em = fminf(em, name_fit(k, w, h, turn));
    return floorf(em);
}

/* a gate's name on its picture: o is w x h upright, or h x w turned */
static void gate_name(uint8_t *o, int w, int h, int turn, int kind, int lit)
{
    float cx, cy, em, maxw;
    name_room(kind, w, h, turn, &cx, &cy, &em, &maxw);
    float fit = fminf(em, maxw / word_ratio(kind));
    em = name_size > 0 ? fminf(name_size, fit) : fit;
    gate_word(o, turn ? h : w, turn ? w : h, kind, cx, cy, em, 1e9f, lit);
}

/* a gate's brass shape, pointing up, in a w x h picture (pixels); its output's lead from its top to the picture's
 * top edge (lw wide); its name engraved on it (big enough; not plain). Lit: gold. */
static void paint_gate(int kind, int lit, int w, int h, float lw, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    art_style st = style(lit ? GOLD : BRASS, 0.4f, fmaxf(1.0f, 0.03f * fminf(w, h)));
    fig *f = fig_new();
    float xl = 0.1f * w, xr = 0.9f * w, xc = 0.5f * w, yb = 0.94f * h, yt = 0.07f * h;
    int bubble = kind == LG_NOT || kind == LG_NAND || kind == LG_NOR;
    float rb = 0.085f * fminf(w, 1.6f * h);
    if (bubble) yt += 2 * rb;
    fig_rect(f, ART_PART, xc - lw / 2, 0, xc + lw / 2, yt + 0.1f * h);
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
        fig_poly(f, ART_PART, p, n / 2);
    } else if (kind == LG_NOT) {
        float q[] = {xl + 0.03f * w, yb, xr - 0.03f * w, yb, xc, yt};
        fig_poly(f, ART_PART, q, 3);
    } else {                                    /* OR, NOR, XOR: a curved shield, its back hollow */
        float back = 0.17f * (yb - yt), gap = kind == LG_XOR ? 0.13f * (yb - yt) : 0;
        float b = yb - gap;
        n = curve(p, n, xl, b, xc, b - 2 * back, xr, b, 16);
        n = curve(p, n, xr, b, xr, yt + 0.42f * (b - yt), xc, yt, 16);
        n = curve(p, n, xc, yt, xl, yt + 0.42f * (b - yt), xl, b, 16);
        fig_poly(f, ART_PART, p, n / 2);
        if (kind == LG_XOR) {                   /* the second back line */
            float t = fmaxf(1.5f, 0.06f * (yb - yt));
            int m = 0;
            m = curve(p, m, xl, yb, xc, yb - 2 * back, xr, yb, 16);
            float q[2 * 40];
            int k = curve(q, 0, xr, yb - t, xc, yb - t - 2 * back, xl, yb - t, 16);
            memcpy(p + m, q, sizeof(float) * (size_t)k);
            fig_poly(f, ART_PART, p, (m + k) / 2);
        }
    }
    if (bubble) fig_ellipse(f, ART_PART, xc, yt - rb, rb, rb);
    fig_render(f, &st, w, h, o);
    fig_free(f);
    if (h >= 26 && !plain) gate_name(o, w, h, 0, kind, lit);   /* a small picture, an emblem: the shape alone */
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
        fig *f = fig_new();
        float t = pass ? lw * 0.75f : lw * 0.45f;
        for (int side = -1; side <= 1; side += 2) {
            if (pass && (side < 0) != (way == 0)) continue;
            float x1 = xc + side * off;
            const float diag[] = {xc - t, yj, xc + t, yj, x1 + t, ya, x1 - t, ya};
            fig_poly(f, ART_PART, diag, 4);
            fig_rect(f, ART_PART, x1 - t, 0, x1 + t, ya + 0.5f * t);
        }
        fig_rect(f, ART_PART, xc - t, yj - 0.5f * t, xc + t, (float)h);
        if (pass) fig_ellipse(f, ART_PART, xc, yj, 1.9f * t, 1.9f * t);
        uint8_t *b = calloc((size_t)w * h, 4);
        if (b) {
            fig_render(f, &st, w, h, b);
            art_blend(o, w, h, b, w, h, 0, 0);
            free(b);
        }
        fig_free(f);
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
    fig *f = fig_new();
    fig_rect(f, ART_PART, 0.02f * w, 0.18f * h, 0.98f * w, 0.82f * h);
    fig_render(f, &st, w, h, o);
    fig_free(f);
    art_style ss = style(screw, 0.6f, fmaxf(1, 0.03f * h));
    f = fig_new();
    for (int s = 0; s < 2; s++) {
        float cx = (s ? 0.82f : 0.18f) * w, r = 0.2f * h;
        fig_ellipse(f, ART_PART, cx, 0.5f * h, r, r);
        fig_rect(f, ART_MARK, cx - 0.8f * r, 0.5f * h - 0.12f * r, cx + 0.8f * r, 0.5f * h + 0.12f * r);
    }
    uint8_t *b = calloc((size_t)w * h, 4);
    if (b) {
        fig_render(f, &ss, w, h, b);
        art_blend(o, w, h, b, w, h, 0, 0);
        free(b);
    }
    fig_free(f);
}

/* a lamp (w x h): a brass socket at the bottom, the glass globe above it (lit: gold), its filament */
static void paint_lamp(int alarm, int lit, int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    const uint8_t brass[3] = {176, 140, 70}, glass[3] = {150, 166, 156}, warm[3] = {255, 238, 150},
                  red_off[3] = {112, 34, 28}, red_on[3] = {244, 70, 46};
    float xc = w / 2.0f, sb = 0.3f * h, r = fminf(0.48f * w, 0.36f * h);
    art_style sk = style(brass, 0.5f, fmaxf(1, 0.025f * w));
    fig *f = fig_new();
    fig_rect(f, ART_PART, xc - 0.42f * r, h - sb, xc + 0.42f * r, (float)h);
    for (int i = 1; i < 3; i++)
        fig_rect(f, ART_MARK, xc - 0.42f * r, h - sb + i * sb / 3 - 0.6f, xc + 0.42f * r, h - sb + i * sb / 3 + 0.6f);
    fig_render(f, &sk, w, h, o);
    fig_free(f);
    f = fig_new();
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
        fig_poly(f, ART_PART, p, n / 2);
        float t = fmaxf(0.8f, 0.035f * r);
        fig_rect(f, ART_MARK, xc - t, h - sb - 1.25f * r, xc + t, h - sb);
        fig_rect(f, ART_MARK, xc - r, h - sb - 0.55f * r - t, xc + r, h - sb - 0.55f * r + t);
    } else {
        fig_ellipse(f, ART_PART, xc, cy, r, r);
        const float neck[] = {xc - 0.55f * r, cy + 0.6f * r, xc + 0.55f * r, cy + 0.6f * r, xc + 0.42f * r,
                              h - sb + 1.0f, xc - 0.42f * r, h - sb + 1.0f};
        fig_poly(f, ART_PART, neck, 4);
        float t = fmaxf(0.7f, 0.04f * r);         /* the filament: a small zigzag */
        for (int i = 0; i < 4; i++) {
            float xa = xc - 0.3f * r + i * 0.15f * r, xb = xa + 0.15f * r, ya = cy + (i & 1 ? -0.12f : 0.12f) * r,
                  yb2 = cy + (i & 1 ? 0.12f : -0.12f) * r;
            const float seg[] = {xa, ya - t, xb, yb2 - t, xb, yb2 + t, xa, ya + t};
            fig_poly(f, ART_MARK, seg, 4);
        }
        fig_rect(f, ART_MARK, xc - 0.3f * r - t, cy + 0.12f * r, xc - 0.3f * r + t, h - sb);
        fig_rect(f, ART_MARK, xc + 0.3f * r - t, cy + 0.12f * r, xc + 0.3f * r + t, h - sb);
    }
    uint8_t *b = calloc((size_t)w * h, 4);
    if (b) {
        if (lit && !alarm) memcpy(sg.mark, (float[3]){170, 90, 20}, sizeof sg.mark);
        fig_render(f, &sg, w, h, b);
        art_blend(o, w, h, b, w, h, 0, 0);
        free(b);
    }
    fig_free(f);
}

/* a cable's terminal (d x d): a brass post seen from above, its screw head slotted (lit: gold) */
static void paint_post(int lit, int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    const uint8_t brass[3] = {196, 156, 76}, head[3] = {170, 132, 62}, gold[3] = {255, 214, 110},
                  gold2[3] = {236, 184, 70};
    float c = d / 2.0f, m = fmaxf(1, 0.05f * d);
    for (int part = 0; part < 2; part++) {
        art_style st = style(part ? (lit ? gold2 : head) : lit ? gold : brass, 0.5f, m);
        fig *f = fig_new();
        if (!part) {
            fig_ellipse(f, ART_PART, c, c, 0.47f * d, 0.47f * d);
        } else {
            fig_ellipse(f, ART_PART, c, c, 0.3f * d, 0.3f * d);
            const float slot[] = {c - 0.24f * d, c + 0.1f * d, c + 0.1f * d, c - 0.24f * d, c + 0.24f * d,
                                  c - 0.1f * d, c - 0.1f * d, c + 0.24f * d};
            fig_poly(f, ART_MARK, slot, 4);
        }
        uint8_t *b = calloc((size_t)d * d, 4);
        if (b) {
            fig_render(f, &st, d, d, b);
            art_blend(o, d, d, b, d, d, 0, 0);
            free(b);
        }
        fig_free(f);
    }
}

/* a cable's colours: red, blue, yellow, green (by its number among the level's cuttable wires) */
static const uint8_t CABLE[4][3] = {{218, 50, 40}, {44, 104, 214}, {240, 192, 36}, {46, 164, 74}};

/* a cable picture (w x h, a bar's count, an emblem): a red jumper arcing between two brass posts */
static void paint_cable_pic(int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    float t = 0.16f * h, pd = 0.3f * h, ax = 0.18f * w, bx = 0.82f * w, y0 = 0.68f * h, top = 0.12f * h;
    float p[2 * 70];
    int n = 0;
    for (int side = 0; side < 2; side++)        /* the band: the arc's outer edge, then its inner one back */
        for (int i = 0; i <= 32; i++) {
            float u = side ? 1 - i / 32.0f : i / 32.0f, x = ax + (bx - ax) * u;
            float y = y0 - 4 * (y0 - top) * u * (1 - u) + (side ? t / 2 : -t / 2);
            p[n++] = x;
            p[n++] = y;
        }
    art_style st = style(CABLE[0], 0.45f, fmaxf(1, 0.04f * h));
    fig *f = fig_new();
    fig_poly(f, ART_PART, p, n / 2);
    uint8_t *b = calloc((size_t)w * h, 4);
    if (b) {
        fig_render(f, &st, w, h, b);
        art_blend(o, w, h, b, w, h, 0, 0);
        for (int k = 0; k < 2; k++) {
            int d = (int)pd;
            memset(b, 0, (size_t)w * h * 4);
            paint_post(1, d, b);
            art_blend(o, w, h, b, d, d, (int)((k ? bx : ax) - d / 2.0f), (int)(y0 - d / 2.0f));
        }
        free(b);
    }
    fig_free(f);
}

static void paint_star(int size, const uint8_t rgb[3], uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_style st = style(rgb, 0.35f, fmaxf(1, 0.04f * size));
    fig *f = fig_new();
    float p[20];
    for (int i = 0; i < 10; i++) {
        float a = -3.14159265f / 2 + i * 3.14159265f / 5, r = (i & 1 ? 0.21f : 0.48f) * size;
        p[2 * i] = size / 2.0f + r * cosf(a);
        p[2 * i + 1] = size * 0.53f + r * sinf(a);
    }
    fig_poly(f, ART_PART, p, 10);
    fig_render(f, &st, size, size, o);
    fig_free(f);
}

/* the restart tool's icon (white): an arrow going round */
/* the chapters' emblems (size x size), the gate each one brings: AND (and OR, NOT), XOR, NAND, NOR, three inputs (an
 * AND with three leads); then a lamp: everything */
static void paint_emblem(int c, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    int w = (int)(0.8f * size), h = (int)(0.8f * size);
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    float lw = fmaxf(2, 0.07f * size);
    if (c <= 4) {
        static const int KIND[5] = {LG_AND, LG_XOR, LG_NAND, LG_NOR, LG_AND};
        int gh = c == 4 ? (int)(0.62f * size) : h, y = c == 4 ? (int)(0.06f * size) : (size - h) / 2;
        if (c == 4) {                           /* the three leads under it */
            art_style st = style(GOLD, 0.3f, fmaxf(1, 0.025f * size));
            fig *f = fig_new();
            for (int k = 0; k < 3; k++) {
                float x = (0.3f + 0.2f * k) * size;
                fig_rect(f, ART_PART, x - lw / 2, y + 0.85f * gh, x + lw / 2, (float)size);
            }
            fig_render(f, &st, size, size, b);
            fig_free(f);
            art_blend(o, size, size, b, size, size, 0, 0);
            memset(b, 0, (size_t)size * size * 4);
        }
        plain = 1;
        paint_gate(KIND[c], 1, w, gh, lw, b);
        plain = 0;
        art_blend(o, size, size, b, w, gh, (size - w) / 2, y);
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
/* The parts' pictures are painted when first drawn (a level shows a few kinds only) and kept while the parts' size
 * stays; the board's when the room changes (a narrower board is drawn from it in two halves). */
static SDL_Texture *t_board, *t_plaque, *t_gate[LG_KINDS][2][2], *t_route[2][2], *t_switch[2], *t_lock, *t_lamp[2][2],
    *t_post[2], *t_dot, *t_glow, *t_emblem[LG_CHAPTERS];
static int made_board = -1, made_parts = -1, made_shared = -1, made_bar = -1;
static float pw, ph, sw_w, sw_h, lamp_w, lamp_h, post_d, cable_t, gate_w[2], gate_h, name_em;
static puz_font font;

static SDL_Texture *tex_buf(int w, int h, uint8_t *b)
{
    SDL_Texture *t = ui_tex(w, h, b);
    free(b);
    return t;
}

/* ---- the board turned: a deep circuit in a wide room lies on its side, the switches on the left, the lamps on the
 * right, when its parts come out bigger so. Everything is laid out and drawn upright (x right, y down, as the
 * circuit stands), then put on the screen a quarter turn clockwise: the pictures of the parts are painted turned,
 * the gates' names stay level. */
static int turned;

static void to_screen(float ux, float uy, float *sx, float *sy)
{
    if (!turned) { *sx = ux; *sy = uy; return; }
    *sx = panel.x + panel.w - uy;
    *sy = panel.y + ux;
}

static void from_screen(float sx, float sy, float *ux, float *uy)
{
    if (!turned) { *ux = sx; *uy = sy; return; }
    *ux = sy - panel.y;
    *uy = panel.x + panel.w - sx;
}

/* a picture turned a quarter clockwise (w x h to h x w); the old one freed */
static uint8_t *quarter(uint8_t *b, int w, int h)
{
    uint8_t *o = malloc((size_t)w * h * 4);
    if (!o) return b;
    for (int y = 0; y < w; y++)
        for (int x = 0; x < h; x++) memcpy(o + 4 * (y * h + x), b + 4 * ((h - 1 - x) * w + y), 4);
    free(b);
    return o;
}

/* a part's picture, painted upright (w x h): turned with the board */
static SDL_Texture *tex_up(int w, int h, uint8_t *b)
{
    if (!turned) return tex_buf(w, h, b);
    b = quarter(b, w, h);
    return tex_buf(h, w, b);
}

/* the drawing on the board, in upright places and sizes */
static void b_put(SDL_Texture *t, float x, float y, float w, float h)
{
    if (!turned) { ui_put(t, x, y, w, h); return; }
    float sx, sy;
    to_screen(x, y + h, &sx, &sy);
    ui_put(t, sx, sy, h, w);
}
static void b_put_c(SDL_Texture *t, float cx, float cy, float w, float h)
{
    float sx, sy;
    to_screen(cx, cy, &sx, &sy);
    if (turned) ui_put_c(t, sx, sy, h, w);
    else ui_put_c(t, sx, sy, w, h);
}

static void b_fill(float x, float y, float w, float h, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!turned) { ui_fill(x, y, w, h, r, g, b, a); return; }
    float sx, sy;
    to_screen(x, y + h, &sx, &sy);
    ui_fill(sx, sy, h, w, r, g, b, a);
}
static void b_shadow(float cx, float cy, float w, float h, float a)
{
    float sx, sy;
    to_screen(cx, cy, &sx, &sy);
    if (turned) ui_shadow_flat(sx, sy, h, w, a);
    else ui_shadow_flat(sx, sy, w, h, a);
}
static void b_mark_gold(float cx, float cy, float d)
{
    float sx, sy;
    to_screen(cx, cy, &sx, &sy);
    ui_mark_gold(sx, sy, d);
}
static void b_mark_glow(float cx, float cy, float d)
{
    float sx, sy;
    to_screen(cx, cy, &sx, &sy);
    ui_mark_glow(sx, sy, d);
}

static void board_textures(void)
{
    int key = (int)B.w * 4096 + (int)B.h;
    fr = (int)(0.03f * fminf(B.w, B.h));
    if (made_board == key) return;
    made_board = key;
    int W = (int)B.w, H = (int)B.h;
    uint8_t *b = calloc((size_t)W * H, 4);
    if (!b) return;
    paint_board(W, H, (int)(0.03f * fminf(W, H)), b);
    ui_free(&t_board);
    t_board = tex_buf(W, H, b);
}

/* the board: the whole room's picture; a narrower panel drawn from its left half and its right half */
static void draw_board(void)
{
    if (panel.w >= B.w - 0.5f) { ui_put(t_board, B.x, B.y, B.w, B.h); return; }
    SDL_Rect was;
    SDL_bool clipped = SDL_RenderIsClipEnabled(ui.r);
    SDL_RenderGetClipRect(ui.r, &was);
    int half = (int)(panel.w / 2);
    SDL_Rect c = {(int)panel.x, (int)panel.y, half, (int)panel.h};
    SDL_RenderSetClipRect(ui.r, &c);
    ui_put(t_board, panel.x, B.y, B.w, B.h);
    c.x += half;
    c.w = (int)panel.w - half;
    SDL_RenderSetClipRect(ui.r, &c);
    ui_put(t_board, panel.x + panel.w - B.w, B.y, B.w, B.h);
    SDL_RenderSetClipRect(ui.r, clipped ? &was : NULL);
}

/* the parts' sizes for the columns and rows; the pictures made at that size dropped (painted again when drawn) */
static void part_textures(void)
{
    int key = ((int)(cw * 8) * 8192 + (int)(ch * 8)) * 2 + turned;
    if (made_parts == key) return;
    made_parts = key;
    float m = fminf(cw, ch);
    tw = fmaxf(2.5f, roundf(0.05f * m));
    pw = roundf(2 * LG_HALF * cw);
    ph = roundf(LG_NH * ch);
    gate_h = floorf(0.92f * ph);
    for (int wide3 = 0; wide3 < 2; wide3++)    /* two inputs, or three: wider */
        /* wide enough for its inputs (up to 0.22 columns from its middle: the rules' PIN_MAX) */
        gate_w[wide3] = floorf(fminf(0.94f * pw, fmaxf(0.6f * cw, wide3 ? 1.6f * gate_h : 1.2f * gate_h)));
    name_em = name_common((int)gate_w[0], (int)gate_h, turned);   /* the names all alike */
    sw_w = roundf(fminf(2 * LG_SW_HALF * cw * 0.94f, 0.75f * ph));
    sw_h = ph;
    lamp_h = roundf((LG_NH + LG_PAD_T - 0.04f) * ch);
    lamp_w = roundf(fminf(0.8f * cw, 0.85f * lamp_h));
    cable_t = roundf(fmaxf(7, 2.1f * tw));
    post_d = roundf(1.6f * cable_t) + 1;
    ui_free(&t_plaque);
    ui_free(&t_lock);
    for (int k = 0; k < LG_KINDS; k++)
        for (int i = 0; i < 4; i++) ui_free(&t_gate[k][i / 2][i % 2]);
    for (int i = 0; i < 4; i++) {
        ui_free(&t_route[i / 2][i % 2]);
        ui_free(&t_lamp[i / 2][i % 2]);
    }
    for (int i = 0; i < 2; i++) {
        ui_free(&t_switch[i]);
        ui_free(&t_post[i]);
    }
}

static SDL_Texture *plaque_tex(void)
{
    int w = (int)pw, h = (int)ph;
    uint8_t *b;
    if (!t_plaque && (b = calloc((size_t)w * h, 4))) { paint_walnut(b, w, h, 1.0f); t_plaque = tex_up(w, h, b); }
    return t_plaque;
}

static SDL_Texture *gate_tex(int k, int lit, int wide3)
{
    SDL_Texture **t = &t_gate[k][lit][wide3];
    int gw = (int)gate_w[wide3], gh = (int)gate_h;
    uint8_t *b;
    if (*t || !(b = calloc((size_t)gw * gh, 4))) return *t;
    name_size = name_em;
    plain = turned;                             /* turned: the name painted level, after the turn */
    paint_gate(k, lit, gw, gh, tw, b);
    plain = 0;
    if (turned) {
        b = quarter(b, gw, gh);
        gate_name(b, gw, gh, 1, k, lit);
        *t = tex_buf(gh, gw, b);
    } else {
        *t = tex_buf(gw, gh, b);
    }
    name_size = 0;
    return *t;
}

static SDL_Texture *route_tex(int way, int lit)
{
    SDL_Texture **t = &t_route[way][lit];
    int w = (int)pw, h = (int)ph;
    uint8_t *b;
    if (!*t && (b = calloc((size_t)w * h, 4))) {
        paint_route(way, lit, w, h, 0.18f * cw, tw, b);
        *t = tex_up(w, h, b);
    }
    return *t;
}

static SDL_Texture *switch_tex(int on)
{
    uint8_t *b;
    if (!t_switch[on] && (b = calloc((size_t)sw_w * sw_h, 4))) {
        paint_switch(on, (int)sw_w, (int)sw_h, b);
        t_switch[on] = tex_up((int)sw_w, (int)sw_h, b);
    }
    return t_switch[on];
}

static SDL_Texture *lock_tex(void)
{
    int w = (int)(sw_w * 1.12f), h = (int)(sw_w * 0.42f);
    uint8_t *b;
    if (!t_lock && (b = calloc((size_t)w * h, 4))) { paint_lock(w, h, b); t_lock = tex_up(w, h, b); }
    return t_lock;
}

static SDL_Texture *lamp_tex(int alarm, int lit)
{
    SDL_Texture **t = &t_lamp[alarm][lit];
    uint8_t *b;
    if (!*t && (b = calloc((size_t)lamp_w * lamp_h, 4))) {
        paint_lamp(alarm, lit, (int)lamp_w, (int)lamp_h, b);
        *t = tex_up((int)lamp_w, (int)lamp_h, b);
    }
    return *t;
}

static SDL_Texture *post_tex(int lit)
{
    int d = (int)post_d;
    uint8_t *b;
    if (!t_post[lit] && (b = calloc((size_t)d * d, 4))) { paint_post(lit, d, b); t_post[lit] = tex_buf(d, d, b); }
    return t_post[lit];
}

static void shared_textures(void)
{
    int key = (int)ui.unit;
    if (made_shared == key) return;
    made_shared = key;
    const uint8_t white[3] = {255, 255, 255}, gold[3] = {255, 200, 80};
    int d = (int)(0.5f * ui.unit) | 1;
    ui_free(&t_dot);
    uint8_t *b = calloc((size_t)d * d, 4);
    if (b) { art_disc(d, white, 1, 0, b); t_dot = tex_buf(d, d, b); }
    int g = (int)(2 * ui.unit);
    ui_free(&t_glow);
    b = calloc((size_t)g * g, 4);
    if (b) { art_disc(g, gold, 0.85f, 1.6f, b); t_glow = tex_buf(g, g, b); }
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
/* the columns' and rows' sizes for a room (upright: uw x uh); how big a part comes out */
static float cells(float uw, float uh, float *c, float *r)
{
    float m = 0.16f * ui.unit, iw = uw - 2 * fr - 2 * m, ih = uh - 2 * fr - 2 * m;
    float s = lg_part_size(&G.L, iw, ih, c, r);  /* as big as the room allows; a column a 3.2th of it at most */
    *c = floorf(*c);
    *r = floorf(*r);
    return s;
}

static void fit(void)
{
    float m = 0.16f * ui.unit, cols = G.L.width, span = G.L.height + LG_PAD_B + LG_PAD_T, c2, r2;
    panel = B;
    float up = cells(B.w, B.h, &cw, &ch), side = cells(B.h, B.w, &c2, &r2);
    turned = wide && side > 1.1f * up;          /* lying: on its side when its parts come out bigger */
    if (turned) { cw = c2; ch = r2; }
    /* a room taller than the circuit needs: the gaps between its rows stretched to fill it, 2.5 times a part's
     * height at most on average */
    float gaps = 0, room = (turned ? B.w : B.h) - 2 * fr - 2 * m;
    for (int k = 0; k + 1 < G.L.layers; k++) gaps += G.L.ly[k + 1] - G.L.ly[k] - LG_NH;
    gs = 1;
    if (gaps > 0 && room > span * ch) {
        float most = 2.5f * LG_NH * (G.L.layers - 1) / gaps;
        gs = fmaxf(1, fminf(1 + (room - span * ch) / (gaps * ch), most));
    }
    span += gaps * (gs - 1);
    float ext = turned ? span * ch : cols * cw, cell = turned ? ch : cw;
    if (wide && ext < B.w - 2 * fr - 2 * m - 1.2f * cell) {   /* the board no wider than the circuit, centred */
        float w = ext + 1.2f * cell + 2 * fr + 2 * m;
        panel.x = floorf(B.x + (B.w - w) / 2);
        panel.w = floorf(w);
    }
    board_textures();
    float ux = turned ? 0 : panel.x, uy = turned ? 0 : panel.y, uw = turned ? panel.h : panel.w,
          uh = turned ? panel.w : panel.h;
    ox = floorf(ux + uw / 2 - cols * cw / 2);
    oy = floorf(uy + uh / 2 + (span / 2 - LG_PAD_B) * ch);
    part_textures();
    knob_texture();
}

/* the board in all the room between the bars; the tools are the shell's (in the bar's row, or the side column) */
static void layout(box b)
{
    B = panel = b;
    wide = b.w > 1.08f * b.h;
    ui_tools_shown(tools_now());
    shared_textures();
    board_textures();
    if (setup) setup_layout();
    else if (G.L.cols) fit();                   /* a level open */
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
    snip.wire = -1;
    hint.on = 0;
    deny_reset = 0;
}

static void open_level(int level)
{
    if (!lg_open(&G, level)) return;
    calm();
    if (B.w > 0) fit();
}

/* The levels were made anew (up to 8 switches, no two alike): the stars won on the old ones would land on other
 * levels, so the old progress goes, once. The shell keeps it (shell/levels/logicgates); our own key says which set
 * of levels it belongs to. Done when the menu paints the game's card, before the level screen reads the progress. */
#define LEVEL_SET 3
static void fresh_progress(void)
{
    static int done;
    if (done) return;
    done = 1;
    uint8_t v = 0;
    if (ui_save_get("logicgates/levels", &v, 1) == 1 && v == LEVEL_SET) return;
    ui_save_set("shell/levels/logicgates", NULL, 0);
    v = LEVEL_SET;
    ui_save_set("logicgates/levels", &v, 1);
}

static void start(int level)                    /* the shell's level screen: 0..LG_LEVELS-1 */
{
    setup = 0;
    ui_tools_shown(tools_now());
    open_level(level);
}

/* ---- the die tab: random play ------------------------------------------------------------------------------------
 * It opens on its setup screen (setup_draw): the size and the parts the circuits may use, kept on the device
 * (logicgates/random: the size, then the parts' bits, little-endian), every part at first. Its play button makes a
 * circuit (lg_random: as good a puzzle as the levels, of those parts only). The end card's next is a new circuit of
 * the same choice at once, its replay the same circuit; the tool after the hint opens the setup screen again. */
static int rnd_size = 1, rnd_loaded;
static unsigned rnd_comp = LG_C_ALL;
static uint32_t rnd_seed;                       /* the seed start_random drew last (a replay draws it again) */
static int rnd_unused;                          /* that seed not played yet (the first play after the tab) */

static void choice_load(void)
{
    if (rnd_loaded) return;
    rnd_loaded = 1;
    uint8_t b[3];
    if (ui_save_get("logicgates/random", b, 3) == 3 && b[0] < LG_RANDOM) {
        rnd_size = b[0];
        rnd_comp = (b[1] | (unsigned)b[2] << 8) & LG_C_ALL;
    }
}

static void choice_save(void)
{
    uint8_t b[3] = {(uint8_t)rnd_size, (uint8_t)(rnd_comp & 255), (uint8_t)(rnd_comp >> 8)};
    ui_save_set("logicgates/random", b, 3);
}

static void random_circuit(uint32_t seed)       /* a circuit of the choice */
{
    if (!lg_random(&G, rnd_size, rnd_comp, seed)) lg_open(&G, 0);
    setup = 0;
    calm();
    ui_tools_shown(tools_now());
    if (B.w > 0) fit();
}

/* The shell calls it for the die tab (the setup screen) and for the end card's next and replay (a circuit at once):
 * those come while the card shows (ui_over). A replay gives the same seed again: the same circuit, started over. */
static void start_random(int variant)
{
    (void)variant;                              /* one variant: the sizes are the setup screen's */
    choice_load();
    uint32_t seed = (uint32_t)ui_random(0x7fffffff) + 1u;
    int card = ui_over() && !setup && G.level < 0 && G.L.cols;
    if (card && seed == rnd_seed) {             /* replay */
        lg_restart(&G);
        calm();
        return;
    }
    rnd_seed = seed;
    rnd_unused = !card;
    if (card) random_circuit(seed);             /* next */
    else setup_open();
}

static int chapter_of(int level) { return lg_chapter(level); }   /* chapter 1 is longer */
static int status(void) { return !setup && lg_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return lg_stars(&G); }
static int score(void) { return G.nh; }
static int par(void) { return G.par; }

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
    if (setup) return 0;
    Uint32 now = ui_ms();
    if (t0 && now - t0 < (Uint32)((G.L.maxdepth + 1) * STEP_MS)) return 1;
    if (flip_node >= 0 && now - flip_t < FLIP_MS) return 1;
    if (snip.wire >= 0 && now - snip.t < (Uint32)(snip.mend ? MEND_MS + 200 : CUT_MS)) return 1;
    return lg_solved(&G) && t0 && (int)(now - solved_t) < BURST_MS;
}

static int undo(void)
{
    static lg_game before;
    if (setup) return 0;
    before = G;
    if (!lg_undo(&G)) return 0;
    calm();
    moved(&before);
    lg_move m = before.hist[(before.nh - 1) % LG_HIST];
    if (m.kind == LG_MCUT) {                    /* a cut undone: the halves spring back together */
        snip.wire = G.L.cut[m.idx];
        snip.t = t0;
        snip.mend = 1;
    }
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

/* the notation (logicgates.h); the hint's here (shown blinking), the moves in the rules; random play's setup screen
 * here too: "z0".."z2" chooses the size, "k" and the parts' bits in hex ("k7ff": every part) the parts, "p" plays
 * (a circuit of that choice), "o" opens the setup screen again */
static int setup_play(void);
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
        if (tok[0] == 'z' && tok[1] >= '0' && tok[1] < '0' + LG_RANDOM && !tok[2] && setup) {
            rnd_size = tok[1] - '0';
        } else if (tok[0] == 'k' && tok[1] && setup) {
            char *end;
            unsigned long c = strtoul(tok + 1, &end, 16);
            ok = !*end && c <= LG_C_ALL;
            if (ok) rnd_comp = (unsigned)c;
        } else if (tok[0] == 'p' && !tok[1] && setup) {
            ok = setup_play();
        } else if (tok[0] == 'o' && !tok[1] && !setup && G.level < 0) {
            setup_open();
        } else if (setup) {
            ok = 0;
        } else if (tok[0] == 'h' && !tok[1]) {
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
    snip.wire = -1;
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

/* The traces: each wire one continuous trace. Every stretch is a rectangle reaching half the trace's width past both
 * its ends (so a bend's corner is filled, square); all the dark edges are drawn first, then all the brass and gold over
 * them (draw_wires), everything opaque, so no stretch's edge cuts across the next one and an overlap is never darker
 * or brighter. The lit wires' glow, translucent, goes into a buffer of its own first (opaque there: overlaps, bends,
 * branchings are one), laid over the board at once (glows). */
static int glow_pass;                           /* drawing the glow into its buffer: 1 the wide glow, 2 the narrow */

static void glow_fill(float x, float y, float w, float h)   /* upright places, as b_fill, in the buffer */
{
    float sx = x, sy = y, sw = w, sh = h;
    if (turned) {
        to_screen(x, y + h, &sx, &sy);
        sw = h;
        sh = w;
    }
    SDL_FRect q = {sx - panel.x, sy - panel.y, sw, sh};
    SDL_RenderFillRectF(ui.r, &q);
}

/* a straight stretch of trace (px): its glow (layer 0), its dark edge (1), its brass or gold (2; 3 a gate's lead) */
static void trace(float x0, float y0, float x1, float y1, int lit, int layer)
{
    float t = tw, xa = fminf(x0, x1) - t / 2, ya = fminf(y0, y1) - t / 2, w = fabsf(x1 - x0) + t,
          h = fabsf(y1 - y0) + t;
    if (layer == 0) {
        float g = (glow_pass == 2 ? 0.45f : 1.0f) * t;
        glow_fill(xa - g, ya - g, w + 2 * g, h + 2 * g);
    } else if (layer == 1) {
        b_fill(xa - 1, ya - 1, w + 2, h + 2, 7, 21, 14, 255);
    } else {
        const uint8_t *c = lit ? GOLD : TRACE;
        b_fill(xa, ya, w, h, c[0], c[1], c[2], 255);
        if (lit) {                              /* a bright line down its middle, to the middle of a bend */
            int vert = fabsf(x1 - x0) < fabsf(y1 - y0);
            float e = layer == 3 ? 0.5f * t : 0.15f * t;   /* (3: a lead, straight: as long as its fill) */
            if (vert) b_fill(xa + 0.35f * t, fminf(y0, y1) - e, 0.3f * t, h - t + 2 * e, 255, 227, 150, 255);
            else b_fill(fminf(x0, x1) - e, ya + 0.35f * t, w - t + 2 * e, 0.3f * t, 255, 227, 150, 255);
        }
    }
}

static float hop_r(void) { return fmaxf(4, 1.8f * tw); }   /* a hop's radius (px) */

/* a straight stretch (grid units) of a wire of signal net: a sideways one broken where it hops over another wire */
static void stretch(float x0, float y0, float x1, float y1, int net, int lit, int layer)
{
    if (fabsf(y1 - y0) > 1e-4f) {
        trace(PX(x0), PY(y0), PX(x1), PY(y1), lit, layer);
        return;
    }
    float a = fminf(x0, x1), b = fmaxf(x0, x1), r = hop_r() / cw;
    while (a < b) {
        float next = 1e9f;                      /* the next hop along */
        for (int h = 0; h < G.L.nhop; h++) {
            const lg_mark *m = &G.L.hop[h];
            if (G.L.w[m->wire].net == net && fabsf(m->y - y0) < 1e-3f && m->x + r > a && m->x - r < b && m->x < next)
                next = m->x;
        }
        if (next > 1e8f) {
            trace(PX(a), PY(y0), PX(b), PY(y0), lit, layer);
            return;
        }
        if (next - r > a) trace(PX(a), PY(y0), PX(next - r), PY(y0), lit, layer);
        a = next + r;
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
            stretch(x0 + (x1 - x0) * f0, y0 + (y1 - y0) * f0, x0 + (x1 - x0) * f1, y0 + (y1 - y0) * f1, W->net, lit,
                    layer);
        }
        s += len;
    }
}

/* the hops (a bridge over the wire crossed) and the branchings (a dot), dark-edged */
static void marks(void)
{
    static const uint8_t EDGE[3] = {6, 18, 12};
    float r = hop_r(), d = 2.1f * tw;
    for (int pass = 0; pass < 2; pass++) {
        for (int h = 0; h < G.L.nhop; h++) {
            const lg_mark *m = &G.L.hop[h];
            const uint8_t *c = pass ? (lit_at(m->wire, m->s, front(m->wire)) ? GOLD : TRACE) : EDGE;
            float cx = PX(m->x), cy = PY(m->y), s = pass ? tw : tw + 2;
            SDL_SetTextureColorMod(t_dot, c[0], c[1], c[2]);
            for (int i = 0; i <= 18; i++) {
                float a = 3.14159265f * i / 18;
                b_put_c(t_dot, cx - r * cosf(a), cy - r * sinf(a), s, s);
            }
        }
        for (int j = 0; j < G.L.njoint; j++) {
            const lg_mark *m = &G.L.joint[j];
            const uint8_t *c = pass ? (lit_at(m->wire, m->s, front(m->wire)) ? GOLD : TRACE) : EDGE;
            float s = pass ? d : d + 2;
            SDL_SetTextureColorMod(t_dot, c[0], c[1], c[2]);
            b_put_c(t_dot, PX(m->x), PY(m->y), s, s);
        }
    }
    SDL_SetTextureColorMod(t_dot, 255, 255, 255);
}

/* a wire's pieces by value: cut at the change's front; a cuttable wire's printed trace stops at its terminals */
static int pieces(int w, float *a, float *b, int *lit)
{
    const lg_wire *W = &G.L.w[w];
    float cuts[6], f = front(w);
    int n = 0;
    cuts[n++] = 0;
    if (W->cuttable) {
        cuts[n++] = W->ta;
        cuts[n++] = W->tb;
    }
    if (f > 0 && f < W->len) cuts[n++] = f;
    cuts[n++] = W->len;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && cuts[j - 1] > cuts[j]; j--) {
            float t = cuts[j];
            cuts[j] = cuts[j - 1];
            cuts[j - 1] = t;
        }
    int k = 0;
    for (int i = 0; i + 1 < n; i++) {
        float m = (cuts[i] + cuts[i + 1]) / 2;
        if (cuts[i + 1] - cuts[i] < 1e-4f || (W->cuttable && m > W->ta && m < W->tb)) continue;
        a[k] = cuts[i];
        b[k] = cuts[i + 1];
        lit[k++] = lit_at(w, m, f);
    }
    return k;
}

static SDL_Texture *t_glowbuf;                  /* the glows' buffer: the board's size */
static int glow_w, glow_h;

/* the lit wires' glow: each of its two widths drawn whole into the buffer, then laid over the board faded */
static void glows(void)
{
    static const uint8_t C[2][4] = {{255, 186, 60, 24}, {255, 196, 70, 46}};
    int W = (int)ceilf(panel.w), H = (int)ceilf(panel.h);
    if (!t_glowbuf || glow_w != W || glow_h != H) {
        ui_free(&t_glowbuf);
        t_glowbuf = SDL_CreateTexture(ui.r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, W, H);
        if (!t_glowbuf) return;
        SDL_SetTextureBlendMode(t_glowbuf, SDL_BLENDMODE_BLEND);
        glow_w = W;
        glow_h = H;
    }
    float a[6], b[6];
    int lit[6];
    SDL_BlendMode mode;
    SDL_GetRenderDrawBlendMode(ui.r, &mode);
    for (glow_pass = 1; glow_pass <= 2; glow_pass++) {
        SDL_Texture *was = SDL_GetRenderTarget(ui.r);
        SDL_SetRenderTarget(ui.r, t_glowbuf);
        SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ui.r, 0, 0, 0, 0);
        SDL_RenderClear(ui.r);
        const uint8_t *c = C[glow_pass - 1];
        SDL_SetRenderDrawColor(ui.r, c[0], c[1], c[2], 255);
        for (int w = 0; w < G.L.nw; w++) {
            int k = pieces(w, a, b, lit);
            for (int i = 0; i < k; i++)
                if (lit[i])
                    for (int vert = 0; vert < 2; vert++) path(&G.L.w[w], a[i], b[i], vert, 1, 0);
        }
        SDL_SetRenderDrawBlendMode(ui.r, mode);
        SDL_SetRenderTarget(ui.r, was);
        SDL_SetTextureAlphaMod(t_glowbuf, c[3]);
        ui_put(t_glowbuf, panel.x, panel.y, (float)W, (float)H);
    }
    glow_pass = 0;
}

static void draw_wires(void)
{
    float a[6], b[6];
    int lit[6];
    /* the glows; then every dark edge; then the brass, then the gold over it */
    glows();
    for (int layer = 1; layer < 3; layer++)
        for (int on = 0; on < 2; on++)
            for (int vert = 0; vert < 2; vert++)
                for (int w = 0; w < G.L.nw; w++) {
                    int k = pieces(w, a, b, lit);
                    for (int i = 0; i < k; i++)
                        if (lit[i] == on) path(&G.L.w[w], a[i], b[i], vert, on, layer);
                }
    marks();
}

/* ---- the cables: jumper leads over the board, between two terminals ---------------------------------------------
 * Upright pixels: its terminals A and B, its bow sideways (the side it bows to: n), a slight sag downwards; a cubic
 * curve through them. Cut: two halves, each from its terminal to a free end, sprung apart, drooping and curling out,
 * the copper showing at the ends. */
static int cable_no(int w)                      /* its number among the level's cuttable wires */
{
    for (int i = 0; i < G.L.ncut; i++)
        if (G.L.cut[i] == w) return i;
    return 0;
}

typedef struct { float ax, ay, bx, by, nx, ny, bow, len; } cable_geo;

static cable_geo cable_at(const lg_wire *W)
{
    cable_geo c;
    c.ax = PX(W->tax);
    c.ay = PY(W->tay);
    c.bx = PX(W->tbx);
    c.by = PY(W->tby);
    c.nx = W->fvert ? 1.0f : 0;
    c.ny = W->fvert ? 0 : -1.0f;
    c.bow = W->bow * (W->fvert ? cw : ch);
    c.len = hypotf(c.bx - c.ax, c.by - c.ay);
    if (fabsf(c.bow) < 0.08f * c.len) c.bow = c.bow < 0 ? -0.08f * c.len : 0.08f * c.len;
    return c;
}

static void cable_point(const cable_geo *c, float t, float *x, float *y)   /* the whole cable, t 0..1 */
{
    float u = 1 - t, k = 4.0f / 3, sag = 0.07f * c->len;
    float p1x = c->ax + (c->bx - c->ax) / 3 + c->nx * c->bow * k,
          p1y = c->ay + (c->by - c->ay) / 3 + c->ny * c->bow * k;
    float p2x = c->ax + 2 * (c->bx - c->ax) / 3 + c->nx * c->bow * k,
          p2y = c->ay + 2 * (c->by - c->ay) / 3 + c->ny * c->bow * k;
    p1y += sag;
    p2y += sag;
    *x = u * u * u * c->ax + 3 * u * u * t * p1x + 3 * u * t * t * p2x + t * t * t * c->bx;
    *y = u * u * u * c->ay + 3 * u * u * t * p1y + 3 * u * t * t * p2y + t * t * t * c->by;
}

/* one cable half: from its terminal (side 0: A, 1: B) to its free end, sprung out by f (0: still joined at the
 * middle, 1: at rest) */
static void half_point(const cable_geo *c, int side, float f, float t, float *x, float *y)
{
    float mx, my, sx = side ? c->bx : c->ax, sy = side ? c->by : c->ay;
    cable_point(c, 0.5f, &mx, &my);
    float vx = mx - sx, vy = my - sy, l = hypotf(vx, vy) + 1e-3f, s = c->bow < 0 ? -1.0f : 1.0f;
    float ox = c->nx * s, oy = c->ny * s;       /* the side it bows to */
    /* at rest, the free end hangs: down (a cable lying across), or out to its side (one standing) */
    float gx = fabsf(vx) > fabsf(vy) ? 0 : ox, gy = fabsf(vx) > fabsf(vy) ? 1 : oy;
    float ex = sx + 0.55f * vx + 0.5f * l * gx, ey = sy + 0.55f * vy + 0.5f * l * gy;
    ex = mx + (ex - mx) * f;
    ey = my + (ey - my) * f;
    float jx, jy;                               /* its middle's pull: along the whole cable at first, then sagging */
    cable_point(c, side ? 0.75f : 0.25f, &jx, &jy);
    float kx = sx + 0.5f * vx + 0.12f * l * ox, ky = sy + 0.5f * vy + 0.12f * l * oy;
    float cx = jx + f * (kx - jx), cy = jy + f * (ky - jy);
    float u = 1 - t;
    *x = u * u * sx + 2 * u * t * cx + t * t * ex;
    *y = u * u * sy + 2 * u * t * cy + t * t * ey;
}
/* a dot on the screen from an upright place, shifted (dx, dy) on the screen itself (a shadow) */
static void dot_at(float x, float y, float d, float dx, float dy)
{
    float sx, sy;
    to_screen(x, y, &sx, &sy);
    ui_put_c(t_dot, sx + dx, sy + dy, d, d);
}

/* a tube along n points: its shadow, a glow when lit, its dark edge, its colour, a highlight; copper ends (cut) */
static void tube(const float *px, const float *py, int n, const uint8_t col[3], int lit, int copper)
{
    float t = cable_t;
    for (int pass = 0; pass < 5; pass++) {
        float d = pass == 0 || pass == 2 ? t : pass == 1 ? 1.8f * t : pass == 3 ? 0.74f * t : 0.26f * t;
        float step = pass == 1 ? 0.5f * t : 0.2f * t, dx = pass == 0 ? 0.25f * t : 0, dy = pass == 0 ? 0.35f * t : 0;
        if (pass == 1 && !lit) continue;
        uint8_t r, g, b, a = 255;
        if (pass == 0) { r = g = b = 0; a = 80; }
        else if (pass == 1) { r = 255; g = 200; b = 80; a = 20; }
        else if (pass == 2) { r = (Uint8)(col[0] * 0.42f); g = (Uint8)(col[1] * 0.42f); b = (Uint8)(col[2] * 0.42f); }
        else if (pass == 3) { r = col[0]; g = col[1]; b = col[2]; }
        else { r = (Uint8)(col[0] + (255 - col[0]) * 0.6f); g = (Uint8)(col[1] + (255 - col[1]) * 0.6f);
               b = (Uint8)(col[2] + (255 - col[2]) * 0.6f); a = 200; }
        SDL_SetTextureColorMod(t_dot, r, g, b);
        SDL_SetTextureAlphaMod(t_dot, a);
        float carry = 0;
        for (int i = 0; i + 1 < n; i++) {
            float sx = px[i + 1] - px[i], sy = py[i + 1] - py[i], l = hypotf(sx, sy);
            if (l < 1e-3f) continue;
            float hx = 0, hy = 0;               /* the highlight: towards the upper left of the tube */
            if (pass == 4) {
                hx = sy / l;
                hy = -sx / l;
                if (hx + hy > 0) { hx = -hx; hy = -hy; }
                hx *= 0.2f * t;
                hy *= 0.2f * t;
            }
            for (float s = carry; s < l; s += step) dot_at(px[i] + sx * s / l + hx, py[i] + sy * s / l + hy, d, dx, dy);
            carry = fmodf(carry - l, step);
            if (carry < 0) carry += step;
        }
    }
    if (copper) {                               /* the bare copper at the free end, a few strands splayed */
        float ex = px[n - 1], ey = py[n - 1], ux = ex - px[n - 3], uy = ey - py[n - 3], l = hypotf(ux, uy) + 1e-3f;
        ux /= l;
        uy /= l;
        static const float ang[3] = {-0.4f, 0, 0.4f};
        for (int k = 0; k < 3; k++) {
            float c = cosf(ang[k]), s = sinf(ang[k]), vx = ux * c - uy * s, vy = ux * s + uy * c;
            SDL_SetTextureColorMod(t_dot, 120, 62, 26);
            SDL_SetTextureAlphaMod(t_dot, 255);
            for (int i = 0; i < 6; i++) dot_at(ex + vx * i * 0.16f * t, ey + vy * i * 0.16f * t, 0.36f * t, 0, 0);
            SDL_SetTextureColorMod(t_dot, 236, 150, 84);
            for (int i = 0; i < 6; i++) dot_at(ex + vx * i * 0.16f * t, ey + vy * i * 0.16f * t, 0.22f * t, 0, 0);
        }
    }
    SDL_SetTextureColorMod(t_dot, 255, 255, 255);
    SDL_SetTextureAlphaMod(t_dot, 255);
}

static float spring(float q) { return q <= 0 ? 0 : q >= 1 ? 1 : 1 - expf(-5.5f * q) * cosf(9.5f * q); }

static void cable_spark(float x, float y, float q, int seed);

static void draw_cables(void)
{
    for (int w = 0; w < G.L.nw; w++) {
        const lg_wire *W = &G.L.w[w];
        if (!W->cuttable) continue;
        cable_geo c = cable_at(W);
        float f = front(w);
        int la = lit_at(w, W->ta - 1e-3f, f), lb = lit_at(w, W->tb + 1e-3f, f), cut = G.cut[w];
        const uint8_t *col = CABLE[cable_no(w) % 4];
        float open = cut ? 1 : 0, q = -1;       /* how far the halves are apart */
        if (snip.wire == w) {
            Uint32 dt = now_ms - snip.t;
            if (snip.mend && dt < MEND_MS) { open = 1 - puz_ease(dt / (float)MEND_MS); ui_animate(); }
            else if (!snip.mend && dt < CUT_MS) {
                q = dt / (float)CUT_MS;
                open = spring((q - 0.1f) / 0.9f);
                ui_animate();
            }
        }
        float px[2][40], py[2][40];
        if (!cut && open <= 0) {                /* whole */
            for (int i = 0; i < 40; i++) cable_point(&c, i / 39.0f, &px[0][i], &py[0][i]);
            tube(px[0], py[0], 40, col, la, 0);
        } else {
            for (int s = 0; s < 2; s++) {
                for (int i = 0; i < 20; i++) half_point(&c, s, open, i / 19.0f, &px[s][i], &py[s][i]);
                tube(px[s], py[s], 20, col, s ? lb : la, open > 0.05f);
            }
        }
        for (int s = 0; s < 2; s++)            /* the terminals over the cable's ends */
            b_put_c(post_tex(s ? lb : la), s ? c.bx : c.ax, s ? c.by : c.ay, post_d, post_d);
        float mx, my;
        cable_point(&c, 0.5f, &mx, &my);
        if (q >= 0) cable_spark(mx, my, q, w);
        if (snip.wire == w && snip.mend && now_ms - snip.t >= MEND_MS && now_ms - snip.t < MEND_MS + 200) {
            b_mark_gold(mx, my, 1.2f * post_d * (1 - (now_ms - snip.t - MEND_MS) / 200.0f));
            ui_animate();
        }
    }
}

/* the snip: a white-gold flash at the cut, sparks flying, fading */
static void cable_spark(float cx, float cy, float q, int seed)
{
    float m = fminf(cw, ch);
    if (q < 0.18f) {
        b_mark_gold(cx, cy, m * (0.35f + 2.2f * q));
        SDL_SetTextureColorMod(t_dot, 255, 250, 230);
        SDL_SetTextureAlphaMod(t_dot, (Uint8)(255 * (1 - q / 0.18f)));
        b_put_c(t_dot, cx, cy, cable_t * (1.6f + 4 * q), cable_t * (1.6f + 4 * q));
    }
    for (int k = 0; k < SPARKS; k++) {
        float a = k * 6.2831853f / SPARKS + 0.5f * sinf(k * 7.1f + seed);
        float sp = 0.3f + 0.3f * (k * 37 % 7) / 7.0f;
        float r = m * sp * puz_ease(fminf(1, q * 1.6f)), x = cx + r * cosf(a), y = cy + r * sinf(a) + m * 0.3f * q * q;
        float s = fmaxf(3, 1.3f * tw * (1 - 0.8f * q));
        SDL_SetTextureColorMod(t_dot, 255, (Uint8)(250 - 90 * q), (Uint8)(200 - 160 * q));
        SDL_SetTextureAlphaMod(t_dot, (Uint8)(255 * (1 - q)));
        b_put_c(t_dot, x, y, s, s);
    }
    SDL_SetTextureColorMod(t_dot, 255, 255, 255);
    SDL_SetTextureAlphaMod(t_dot, 255);
}

/* the nearest cable to a point (upright pixels) and how near; -1: none */
static int cable_hit(float x, float y, float *dist)
{
    int best = -1;
    float bd = 1e9f;
    for (int n = 0; n < G.L.ncut; n++) {
        cable_geo c = cable_at(&G.L.w[G.L.cut[n]]);
        for (int i = 0; i <= 24; i++) {
            float px, py;
            cable_point(&c, i / 24.0f, &px, &py);
            float d = hypotf(x - px, y - py);
            if (d < bd) { bd = d; best = n; }
        }
    }
    *dist = bd;
    return best;
}

static void glow(float x, float y, float d, float a)
{
    SDL_SetTextureAlphaMod(t_glow, (Uint8)clampf(255 * a, 0, 255));
    b_put_c(t_glow, x, y, d, d);
    SDL_SetTextureAlphaMod(t_glow, 255);
}

/* a switch's or a route switch's box on the screen (where a tap flips it) */
static void node_box(int i, float *x0, float *y0, float *x1, float *y1)
{
    const lg_node *N = &G.L.n[i];
    float cx = PX(N->x), hw = N->kind == LG_SWITCH ? sw_w / 2 : pw / 2;
    *x0 = cx - hw;
    *x1 = cx + hw;
    *y0 = PY(N->y + LG_NH);
    *y1 = PY(N->y);
}

static void draw_node(int i)
{
    const lg_node *N = &G.L.n[i];
    float cx = PX(N->x), yt = PY(N->y + LG_NH), yb = PY(N->y);
    int out = arrived(i) ? G.out[i][0] : old_out[i][0];
    switch (N->kind) {
    case LG_SWITCH: {
        int on = G.val[i];
        float d = 0.66f * sw_w, p = (float)on;
        if (on) glow(cx, yt + 0.35f * sw_h, 1.6f * sw_w, 0.5f);
        b_shadow(cx + 0.04f * sw_w, yt + sw_h / 2 + 0.05f * sw_w, sw_w, sw_h, 0.5f);
        b_put(switch_tex(on), cx - sw_w / 2, yt, sw_w, sw_h);
        if (flip_node == i && now_ms - flip_t < FLIP_MS) {
            float q = puz_ease((now_ms - flip_t) / (float)FLIP_MS);
            p = on ? q : 1 - q;
            ui_animate();
        }
        float ky = yt + sw_h * (0.71f - 0.42f * p);
        b_shadow(cx + 0.05f * d, ky + 0.08f * d, d, d, 0.6f);
        b_put_c(t_knob, cx, ky, d, d);
        if (N->locked) b_put_c(lock_tex(), cx, yt + 0.5f * sw_h, floorf(sw_w * 1.12f), floorf(sw_w * 0.42f));
        break;
    }
    case LG_LAMP:
    case LG_ALARM: {
        int a = N->kind == LG_ALARM;
        float gy = yb - 0.62f * lamp_h;
        if (out && !a) glow(cx, gy, 1.9f * lamp_w, 0.85f);
        if (out && a) b_mark_glow(cx, gy, 1.7f * lamp_w);
        b_put(lamp_tex(a, out), cx - lamp_w / 2, yb - lamp_h, lamp_w, lamp_h);
        break;
    }
    case LG_ROUTE: {
        int in = arrived(i) ? G.wend[N->in[0]] : old_end[N->in[0]];
        b_put(plaque_tex(), cx - pw / 2, yt, pw, ph);
        if (flip_node == i && now_ms - flip_t < FLIP_MS) {
            float q = puz_ease((now_ms - flip_t) / (float)FLIP_MS);
            SDL_Texture *o = route_tex(!G.val[i], in), *n = route_tex(G.val[i], in);
            SDL_SetTextureAlphaMod(o, (Uint8)(255 * (1 - q)));
            b_put(o, cx - pw / 2, yt, pw, ph);
            SDL_SetTextureAlphaMod(o, 255);
            SDL_SetTextureAlphaMod(n, (Uint8)(255 * q));
            b_put(n, cx - pw / 2, yt, pw, ph);
            SDL_SetTextureAlphaMod(n, 255);
            ui_animate();
        } else {
            b_put(route_tex(G.val[i], in), cx - pw / 2, yt, pw, ph);
        }
        break;
    }
    default: {
        b_put(plaque_tex(), cx - pw / 2, yt, pw, ph);
        float top = yt + 0.84f * gate_h;          /* just into the shape */
        for (int k = 0; k < N->nin; k++) {      /* the inputs' leads, up into the shape */
            int w = N->in[k];
            const lg_wire *W = &G.L.w[w];
            int lit = lit_at(w, W->len, front(w));
            float x = PX(W->px[W->npt - 1]);
            trace(x, yb - 0.5f * tw - 1, x, top, lit, 1);   /* its edge on the plaque only: none across the wire */
            trace(x, yb, x, top, lit, 3);
        }
        int w3 = N->nin == 3;
        b_put(gate_tex(N->kind, out, w3), cx - gate_w[w3] / 2, yt, gate_w[w3], gate_h);
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
        cable_geo c = cable_at(&G.L.w[G.L.cut[hint.m.idx]]);
        cable_point(&c, 0.5f, &x, &y);
        d *= 0.8f;
    } else {
        int i = hint.m.kind == LG_MSWITCH ? G.L.sw[hint.m.idx] : G.L.rt[hint.m.idx];
        float x0, y0, x1, y1;
        node_box(i, &x0, &y0, &x1, &y1);
        x = (x0 + x1) / 2;
        y = (y0 + y1) / 2;
    }
    b_mark_gold(x, y, d);
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
        float cx = PX(N->x), cy = PY(N->y) - 0.62f * lamp_h;
        glow(cx, cy, lamp_w * (2.2f + 0.8f * sinf(3.14159f * fminf(1, q * 1.4f))), 1);
        if (q >= 1) continue;
        glow(cx, cy, lamp_w * (1.5f + 4.5f * puz_ease(q)), 1 - q);
        for (int k = 0; k < 16; k++) {          /* the sparks flying out */
            float a = k * 6.2831853f / 16 + 0.2f * (k & 1), r = lamp_w * (0.5f + (1.4f + 0.5f * (k % 3)) * puz_ease(q));
            float s = fmaxf(2, 0.12f * lamp_w * (1 - q));
            SDL_SetTextureColorMod(t_dot, 255, 226, 130);
            SDL_SetTextureAlphaMod(t_dot, (Uint8)(255 * (1 - q)));
            b_put_c(t_dot, cx + r * cosf(a), cy + r * sinf(a), s, s);
        }
        SDL_SetTextureColorMod(t_dot, 255, 255, 255);
        SDL_SetTextureAlphaMod(t_dot, 255);
        ui_animate();
    }
}

/* the level: its chapter's emblem, its number (as on the level screen), centred on (x, y): before the bar's counts,
 * or at the top of the side column in landscape */
static float draw_label(float x, float y, int centred)
{
    float u = ui.unit;
    int n = G.level + 1, c = lg_chapter(G.level);
    float dh = 0.5f * u, nw = puz_number_w(n, dh), ew = 0.62f * u, w = ew + 0.12f * u + nw;
    float x0 = centred ? x - w / 2 : x;
    ui_put_c(t_emblem[c], x0 + ew / 2, y, ew, ew);
    puz_number(&font, x0 + ew + 0.12f * u + nw / 2, y, dh, n, PUZ_CREAM, 255);
    return x0 + w;
}

static void draw_tools(void)
{
    int blink = deny_reset && now_ms - (Uint32)deny_reset < HINT_MS;   /* the hint found nothing: restart, lit */
    if (blink) ui_animate();
    ui_tools_shown(tools_now());
    ui_tool(T_RESET, ICON_REPLAY, blink && (now_ms - (Uint32)deny_reset) / 300 % 2 == 0 ? UI_LIT : 0);
    ui_tool(T_HINT, ICON_HINT, 0);
    if (G.level < 0) ui_tool_paint(T_SETUP, paint_setup_icon, 0, 0.7f, UI_TINT);   /* random play: its choice */
    if (wide && G.level >= 0) {                 /* the level in the side column, above the tools */
        box c = ui_bar_box(0, NULL);
        draw_label(c.x + c.w / 2, B.y + 1.6f * ui.unit, 1);   /* (under the header) */
    }
}

static void draw(void)
{
    now_ms = ui_ms();
    if (made_shared < 0) layout(B);
    if (setup) {
        ui_tools_shown(0);
        setup_draw();
        return;
    }
    draw_board();
    draw_wires();
    for (int i = 0; i < G.L.nn; i++) draw_node(i);
    draw_cables();
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
    if (m.kind == LG_MCUT) {                    /* the snip: a sharp click, a tick after it */
        snip.wire = wire;
        snip.t = t0;
        snip.mend = 0;
        ui_sound(SND_PLACE);
        ui_sound_later(SND_TAP, 45);
    } else {
        flip_node = node;
        flip_t = t0;
        ui_sound(m.kind == LG_MSWITCH ? SND_MOVE : SND_PLACE);
    }
}

static void press(float x, float y)
{
    if (setup) {
        setup_press(x, y);
        return;
    }
    int i = ui_tool_hit(x, y);
    if (i >= 0) {
        if (i == T_SETUP) {                     /* random play: back to its setup screen */
            ui_sound(SND_TAP);
            setup_open();
            return;
        }
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
    from_screen(x, y, &x, &y);                  /* the board turned: back upright */
    /* a part's own box first, then the cables, then a part's surroundings */
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
        float d;
        int best = cable_hit(x, y, &d);
        if (best >= 0 && d < 0.4f * fminf(cw, ch) + cable_t) {
            do_move((lg_move){LG_MCUT, (uint8_t)best}, -1, G.L.cut[best]);
            return;
        }
    }
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the bar: the moves (a switch), the cuts left (a cable), the par (a star) ------------------------------------ */
static SDL_Texture *t_bar_star, *t_bar_fuse, *t_bar_switch;

static void bar(int player, box b)
{
    (void)player;
    if (setup) return;                          /* nothing to count yet */
    float u = ui.unit, cy = b.y + b.h / 2;
    int sz = (int)(0.6f * u);
    if (made_bar != sz) {                       /* the pictures, square */
        made_bar = sz;
        static const uint8_t gold[3] = {255, 200, 80};
        int w = (int)(0.5f * sz), kd = (int)(0.66f * w);
        uint8_t *bf = calloc((size_t)sz * sz, 4), *k = calloc((size_t)kd * kd, 4), *p = calloc((size_t)sz * sz, 4);
        if (bf && k && p) {
            paint_switch(1, w, sz, p);          /* a switch, on */
            art_blend(bf, sz, sz, p, w, sz, (sz - w) / 2, 0);
            art_counter(COUNTER_DISC, 0, kd, k);
            art_blend(bf, sz, sz, k, kd, kd, (sz - kd) / 2, (int)(0.29f * sz - kd / 2.0f));
            ui_free(&t_bar_switch);
            t_bar_switch = ui_tex(sz, sz, bf);
            paint_cable_pic(sz, sz, bf);        /* a cable */
            ui_free(&t_bar_fuse);
            t_bar_fuse = ui_tex(sz, sz, bf);
            paint_star(sz, gold, bf);           /* a star */
            ui_free(&t_bar_star);
            t_bar_star = ui_tex(sz, sz, bf);
        }
        free(bf);
        free(k);
        free(p);
    }
    if (!wide && G.level >= 0) {                /* the level first (landscape: in the side column) */
        float x = draw_label(b.x, cy, 0) + 0.5f * u;
        b.w -= x - b.x;
        b.x = x;
    }
    /* the counts, each beside its picture, made smaller to fit (ui_bar_fit shrinks them only to make room for its
     * buttons; ours are the shell's tools, already outside the box) */
    ui_count c[3];
    int n = 0;
    c[n++] = (ui_count){t_bar_switch, G.nh};
    if (G.L.ncut) c[n++] = (ui_count){t_bar_fuse, G.L.cut_limit - lg_cuts(&G)};
    c[n++] = (ui_count){t_bar_star, G.par};
    float w = 0;
    for (int i = 0; i < n; i++) w += 0.72f * u + ui_number_w(c[i].value, 0.46f * u) + (i ? 0.35f * u : 0);
    float k = w > b.w - 0.1f * u ? fmaxf(0.55f, (b.w - 0.1f * u) / w) : 1, x = b.x, s = 0.6f * u * k;
    for (int i = 0; i < n; i++) {
        if (i) x += 0.35f * u * k;
        ui_put(c[i].pic, x, cy - s / 2, s, s);
        x += s + 0.12f * u * k;
        float nw = ui_number_w(c[i].value, 0.46f * u * k);
        ui_number(x + nw / 2, cy, 0.46f * u * k, c[i].value, 1);
        x += nw;
    }
}

/* ---- the setup screen's size cards: a circuit of 4, 6 or 8 switches, gates halving them row by row up to the lamp
 * (some gates of each kind) */
static void circuit_thumb(int variant, int size, uint8_t *rgba)
{
    int nsw = 4 + 2 * variant;
    paint_board(size, size, (int)(0.045f * size), rgba);
    float S = (float)size, in = 0.1f * S, t = fmaxf(2, 0.022f * S);
    float x[4][8], ys = 0.8f * S, yl = 0.2f * S;
    int cnt[4] = {nsw}, rows = 1;
    for (int i = 0; i < nsw; i++) x[0][i] = in + (S - 2 * in) * (i + 0.5f) / nsw;
    while (cnt[rows - 1] > 1 && rows < 4) {     /* pairs (three when three are left) */
        int n = cnt[rows - 1], m = 0;
        for (int i = 0, k = n == 3 ? 3 : 2; i < n; i += k) x[rows][m++] = (x[rows - 1][i] + x[rows - 1][i + k - 1]) / 2;
        cnt[rows++] = m;
    }
    float gap = (ys - yl) / rows, pw = fminf(0.8f * (S - 2 * in) / nsw * 2, 0.2f * S), ph = 0.45f * gap;
    float sw = fminf(0.6f * (S - 2 * in) / nsw, 0.13f * S), sh = 1.5f * sw;
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    /* the traces: each part's output up to the gap, across, up into its gate */
    for (int r = 0; r < rows; r++) {
        float ybot = r ? ys - r * gap : ys, ytop = ys - (r + 1) * gap + ph, ym = (ybot + ytop) / 2;
        int n = cnt[r], per = n == 3 ? 3 : 2;
        for (int i = 0; i < n; i++) {
            float px = r + 1 < rows ? x[r + 1][i / per] : S / 2;
            float k = per == 3 ? (i % 3 - 1) * 0.12f : i % 2 ? 0.12f : -0.12f;
            float xi = px + (r + 1 < rows || n > 1 ? k * pw : 0);
            puz_ink_rect(rgba, size, size, x[r][i] - t / 2, ym - t / 2, x[r][i] + t / 2, ybot, GOLD, 1);
            puz_ink_rect(rgba, size, size, fminf(x[r][i], xi) - t / 2, ym - t / 2, fmaxf(x[r][i], xi) + t / 2,
                         ym + t / 2, GOLD, 1);
            puz_ink_rect(rgba, size, size, xi - t / 2, ytop - 1, xi + t / 2, ym + t / 2, GOLD, 1);
        }
    }
    puz_ink_rect(rgba, size, size, S / 2 - t / 2, yl, S / 2 + t / 2, ys - rows * gap + ph, GOLD, 1);
    /* the gates, the lamp, the switches */
    static const int KINDS[3][7] = {{LG_AND, LG_OR, LG_OR, LG_AND, LG_AND, LG_OR, LG_AND},   /* the gates by size */
                                    {LG_NAND, LG_OR, LG_XOR, LG_AND, LG_XOR, LG_AND, LG_NAND},
                                    {LG_AND, LG_OR, LG_XOR, LG_NAND, LG_NOR, LG_XOR, LG_OR}};
    for (int r = 1; r < rows; r++)
        for (int i = 0; i < cnt[r]; i++) {
            int w = (int)pw, h = (int)ph, gw = (int)(0.9f * pw), gh = (int)(0.9f * ph);
            int y = (int)(ys - r * gap + ph - h);
            memset(b, 0, (size_t)size * size * 4);
            paint_walnut(b, w, h, 1);
            art_blend(rgba, size, size, b, w, h, (int)(x[r][i] - w / 2.0f), y);
            memset(b, 0, (size_t)size * size * 4);
            paint_gate(KINDS[variant][(r * 3 + i) % 7], 1, gw, gh, t, b);
            art_blend(rgba, size, size, b, gw, gh, (int)(x[r][i] - gw / 2.0f), y);
        }
    int lw = (int)(0.22f * S);
    memset(b, 0, (size_t)size * size * 4);
    art_disc(2 * lw, (const uint8_t[3]){255, 200, 80}, 0.6f, 1.6f, b);
    art_blend(rgba, size, size, b, 2 * lw, 2 * lw, (int)(S / 2 - lw), (int)(yl - 0.08f * S - lw));
    memset(b, 0, (size_t)size * size * 4);
    paint_lamp(0, 1, lw, lw, b);
    art_blend(rgba, size, size, b, lw, lw, (int)(S / 2 - lw / 2.0f), (int)(yl - lw + 2));
    int kd = (int)(0.66f * sw);
    uint8_t *k = calloc((size_t)kd * kd + 1, 4);
    for (int i = 0; i < nsw; i++) {
        int on = (i * 5 + nsw) % 3 != 0;
        memset(b, 0, (size_t)size * size * 4);
        paint_switch(on, (int)sw, (int)sh, b);
        art_blend(rgba, size, size, b, (int)sw, (int)sh, (int)(x[0][i] - sw / 2), (int)ys);
        if (k) {
            memset(k, 0, (size_t)kd * kd * 4);
            art_counter(COUNTER_DISC, 0, kd, k);
            int ky = (int)(ys + sh * (on ? 0.29f : 0.71f) - kd / 2.0f);
            art_blend(rgba, size, size, k, kd, kd, (int)(x[0][i] - kd / 2.0f), ky);
        }
    }
    free(k);
    free(b);
}

/* ---- random play's setup screen -----------------------------------------------------------------------------------
 * On the green board: the three sizes as cards (a circuit of 4, 6 or 8 switches: the one chosen in a gold light, the
 * others dimmed), then a toggle for each part the circuits may use, drawn as that part on its walnut plaque as on the
 * board: the six gates with their names, an alarm lamp, two lamps, a route switch, a locked switch, a jumper cable.
 * Chosen: lit, in a gold light; left out: dark and dimmed. No words. The play button under them, dim while the parts
 * chosen cannot make a puzzle (lg_comp_ok: no AND, nor a NOT with an OR, NAND or NOR), the gates that would make
 * them good glowing softly meanwhile. Upright, the toggles go three across; on a wide screen six. */
#define TILE_H 0.8f                             /* a toggle's plaque: as tall, of its width */
static struct {
    float c;                                    /* a toggle's width */
    box size[LG_RANDOM], part[LG_COMPONENTS];
    float px, py, pd;                           /* the play button */
} SU;
static SDL_Texture *t_size[LG_RANDOM], *t_part[LG_COMPONENTS][2];
static int made_setup = -1;

/* a toggle's picture (w x h): the part on its walnut plaque, lit (on) or not */
static void paint_part_tile(int k, int on, int w, int h, uint8_t *o)
{
    paint_walnut(o, w, h, 1.0f);
    uint8_t *b = calloc((size_t)w * h, 4), *k2 = calloc((size_t)w * h, 4);
    if (!b || !k2) { free(b); free(k2); return; }
    float lw = fmaxf(2, roundf(0.045f * w));
    const uint8_t *lead = on ? GOLD : TRACE;
    if (k < LG_C_GATES) {                       /* a gate, its inputs' leads rising into it */
        static const int KIND[LG_C_GATES] = {LG_AND, LG_OR, LG_NOT, LG_XOR, LG_NAND, LG_NOR};
        int gw = (int)(0.8f * w), gh = (int)(0.78f * h), gx = (w - gw) / 2, gy = (int)(0.07f * h);
        int nin = KIND[k] == LG_NOT ? 1 : 2;
        for (int i = 0; i < nin; i++) {
            float x = w / 2.0f + (nin == 1 ? 0 : (i ? 0.13f : -0.13f) * w);
            puz_ink_rect(o, w, h, x - lw / 2, gy + 0.8f * gh, x + lw / 2, (float)h, lead, 1);
        }
        name_size = name_common(gw, gh, 0);
        paint_gate(KIND[k], on, gw, gh, lw, b);
        name_size = 0;
        art_blend(o, w, h, b, gw, gh, gx, gy);
    } else if (k == LG_C_ALARM || k == LG_C_LAMPS) {   /* a red alarm lamp; two lamps side by side */
        int two = k == LG_C_LAMPS, lh = (int)(0.9f * h), lwd = (int)fminf((two ? 0.4f : 0.62f) * w, 0.85f * lh);
        for (int i = 0; i <= two; i++) {
            memset(b, 0, (size_t)w * h * 4);
            paint_lamp(!two, on, lwd, lh, b);
            int x = two ? (int)((i ? 0.71f : 0.29f) * w - lwd / 2.0f) : (w - lwd) / 2;
            art_blend(o, w, h, b, lwd, lh, x, (h - lh) / 2);
        }
    } else if (k == LG_C_ROUTE) {               /* a route switch set to the right */
        paint_route(1, on, w, h, 0.2f * w, 1.3f * lw, b);
        art_blend(o, w, h, b, w, h, 0, 0);
    } else if (k == LG_C_LOCK) {                /* a switch, the brass bar screwed over it */
        int sw = (int)(0.36f * w), sh = (int)(0.86f * h), kd = (int)(0.66f * sw), bw = (int)(1.12f * sw),
            bh = (int)(0.42f * sw), y0 = (h - sh) / 2;
        paint_switch(on, sw, sh, b);
        art_blend(o, w, h, b, sw, sh, (w - sw) / 2, y0);
        art_counter(COUNTER_DISC, 0, kd, k2);
        art_blend(o, w, h, k2, kd, kd, (w - kd) / 2, (int)(y0 + sh * (on ? 0.29f : 0.71f) - kd / 2.0f));
        memset(b, 0, (size_t)w * h * 4);
        paint_lock(bw, bh, b);
        art_blend(o, w, h, b, bw, bh, (w - bw) / 2, (int)(y0 + 0.5f * sh - bh / 2.0f));
    } else {                                    /* a jumper cable between its two posts */
        paint_cable_pic(w, h, b);
        art_blend(o, w, h, b, w, h, 0, 0);
    }
    free(b);
    free(k2);
}

/* the tool that opens the setup screen (white, the shell tints it): a gate, its leads */
static void paint_setup_icon(int kind, int size, uint8_t *o)
{
    (void)kind;
    memset(o, 0, (size_t)size * size * 4);
    art_style st;
    for (int i = 0; i < 3; i++) st.fill[i] = st.outer[i] = st.inner[i] = st.mark[i] = 255;
    st.light = 0;
    st.line = 0.5f;
    fig *f = fig_new();
    float S = (float)size, lw = fmaxf(1.5f, 0.09f * S), xc = 0.5f * S, xl = 0.2f * S, xr = 0.8f * S, yt = 0.2f * S,
          yb = 0.72f * S, p[2 * 30];
    int n = 0;
    p[n++] = xl; p[n++] = yb;
    p[n++] = xr; p[n++] = yb;
    for (int i = 0; i <= 24; i++) {
        float a = 3.14159265f * i / 24, rx = (xr - xl) / 2, ry = 0.3f * S;
        p[n++] = xc + rx * cosf(a);
        p[n++] = yt + ry - ry * sinf(a);
    }
    fig_poly(f, ART_PART, p, n / 2);
    fig_rect(f, ART_PART, xc - lw / 2, 0.03f * S, xc + lw / 2, yt + 0.05f * S);
    for (int i = 0; i < 2; i++) {
        float x = xc + (i ? 0.15f : -0.15f) * S;
        fig_rect(f, ART_PART, x - lw / 2, yb - 0.02f * S, x + lw / 2, 0.97f * S);
    }
    fig_render(f, &st, size, size, o);
    fig_free(f);
}

static void setup_textures(void)
{
    int key = (int)SU.c * 4 + (int)(SU.size[0].w) * 4096;
    if (made_setup == key) return;
    made_setup = key;
    int w = (int)SU.c, h = (int)(TILE_H * SU.c), s = (int)SU.size[0].w;
    for (int k = 0; k < LG_COMPONENTS; k++)
        for (int on = 0; on < 2; on++) {
            ui_free(&t_part[k][on]);
            uint8_t *b = calloc((size_t)w * h, 4);
            if (b) { paint_part_tile(k, on, w, h, b); t_part[k][on] = tex_buf(w, h, b); }
        }
    for (int v = 0; v < LG_RANDOM; v++) {
        ui_free(&t_size[v]);
        uint8_t *b = calloc((size_t)s * s, 4);
        if (b) { circuit_thumb(v, s, b); t_size[v] = tex_buf(s, s, b); }
    }
}

/* the places: the sizes' row, the gates' rows, the other parts' rows, the play button; three toggles across or six,
 * whichever makes them bigger */
static void setup_layout(void)
{
    panel = B;
    float m = fr + 0.3f * ui.unit, W = B.w - 2 * m, H = B.h - 2 * m;
    const float gap = 0.16f, sec = 0.42f, play = 1.0f;
    int C = 3;
    float best = 0;
    for (int n = 3; n <= 6; n += 3) {
        int rg = (LG_C_GATES + n - 1) / n, rp = (LG_COMPONENTS - LG_C_GATES + n - 1) / n;
        float ks = n == 6 ? 1.15f : 1.0f;
        float hu = ks + sec + rg * TILE_H + (rg - 1) * gap + sec + rp * TILE_H + (rp - 1) * gap + sec + play;
        float c = fminf(W / (n + (n - 1) * gap), H / hu);
        if (c > best) { best = c; C = n; }
    }
    float c = floorf(fminf(best, 2.6f * ui.unit)), g = gap * c, ks = C == 6 ? 1.15f : 1.0f, s = floorf(ks * c);
    int rg = (LG_C_GATES + C - 1) / C, rp = (LG_COMPONENTS - LG_C_GATES + C - 1) / C;
    float th = floorf(TILE_H * c);
    float total = s + sec * c + rg * th + (rg - 1) * g + sec * c + rp * th + (rp - 1) * g + sec * c + play * c;
    float y = floorf(B.y + (B.h - total) / 2), cx = B.x + B.w / 2;
    SU.c = c;
    float sg = C == 6 ? 0.6f * c : gap * c;    /* the sizes' cards */
    for (int v = 0; v < LG_RANDOM; v++)
        SU.size[v] = (box){floorf(cx - (3 * s + 2 * sg) / 2 + v * (s + sg)), y, s, s};
    y += s + sec * c;
    for (int part = 0; part < 2; part++) {      /* the gates; the other parts */
        int first = part ? LG_C_GATES : 0, n = part ? LG_COMPONENTS - LG_C_GATES : LG_C_GATES;
        for (int i = 0; i < n; i++) {
            int row = i / C, in_row = n - row * C < C ? n - row * C : C, col = i % C;
            float x0 = cx - (in_row * c + (in_row - 1) * g) / 2;
            SU.part[first + i] = (box){floorf(x0 + col * (c + g)), floorf(y + row * (th + g)), c, th};
        }
        int rows = (n + C - 1) / C;
        y += rows * th + (rows - 1) * g + sec * c;
    }
    SU.pd = floorf(0.9f * c);
    SU.px = floorf(cx);
    SU.py = floorf(y + play * c / 2);
    setup_textures();
}

static void setup_open(void)
{
    choice_load();
    setup = 1;
    calm();
    ui_tools_shown(0);
    if (B.w > 0) setup_layout();
    ui_animate();
}

/* a circuit of the choice: the seed the tab drew at first (a replay draws it again), then fresh ones */
static int setup_play(void)
{
    if (!lg_comp_ok(rnd_comp)) return 0;
    uint32_t seed = rnd_unused ? rnd_seed : (uint32_t)ui_random(0x7fffffff) + 1u;
    rnd_unused = 0;
    random_circuit(seed);
    return 1;
}

static void setup_draw(void)
{
    if (SU.c <= 0 || panel.w != B.w) setup_layout();
    draw_board();
    Uint32 now = ui_ms();
    for (int v = 0; v < LG_RANDOM; v++) {       /* the sizes */
        box b = SU.size[v];
        int on = v == rnd_size;
        float cx = b.x + b.w / 2, cy = b.y + b.h / 2;
        if (on) ui_mark_gold_a(cx, cy, 1.6f * b.w, 1);
        ui_shadow_flat(cx + 0.03f * b.w, cy + 0.04f * b.w, b.w, b.h, 0.55f);
        if (on) {                               /* a gold rim */
            float r = fmaxf(2, roundf(0.035f * b.w));
            ui_fill(b.x - r, b.y - r, b.w + 2 * r, b.h + 2 * r, GOLD[0], GOLD[1], GOLD[2], 255);
        }
        Uint8 k = on ? 255 : 150;
        SDL_SetTextureColorMod(t_size[v], k, k, k);
        ui_put(t_size[v], b.x, b.y, b.w, b.h);
        SDL_SetTextureColorMod(t_size[v], 255, 255, 255);
    }
    int ok = lg_comp_ok(rnd_comp);
    float pulse = 0.5f + 0.5f * sinf(now * 0.006f);
    for (int k = 0; k < LG_COMPONENTS; k++) {   /* the parts */
        box b = SU.part[k];
        int on = rnd_comp >> k & 1, want = !ok && !on && k < LG_C_GATES && lg_comp_ok(rnd_comp | 1u << k);
        float cx = b.x + b.w / 2, cy = b.y + b.h / 2;
        if (on) ui_mark_gold_a(cx, cy, 1.4f * b.w, 0.85f);
        else if (want) {                        /* would make the choice good */
            ui_mark_gold_a(cx, cy, 1.4f * b.w, 0.2f + 0.45f * pulse);
            ui_animate();
        }
        ui_shadow_flat(cx + 0.03f * b.w, cy + 0.04f * b.w, b.w, b.h, on ? 0.6f : 0.35f);
        SDL_Texture *t = t_part[k][on];
        if (!t) continue;
        Uint8 m = on ? 255 : 120;
        SDL_SetTextureColorMod(t, m, m, m);
        SDL_SetTextureAlphaMod(t, on ? 255 : 215);
        ui_put(t, b.x, b.y, b.w, b.h);
        SDL_SetTextureColorMod(t, 255, 255, 255);
        SDL_SetTextureAlphaMod(t, 255);
    }
    ui_button(SU.px, SU.py, SU.pd, ICON_PLAY, ok ? UI_LIT : UI_DIM);
}

static int in_box(box b, float x, float y, float slack)
{
    return x >= b.x - slack && x < b.x + b.w + slack && y >= b.y - slack && y < b.y + b.h + slack;
}

static void setup_press(float x, float y)
{
    float sl = 0.06f * SU.c;
    for (int v = 0; v < LG_RANDOM; v++)
        if (in_box(SU.size[v], x, y, sl)) {
            ui_sound(SND_TAP);
            rnd_size = v;
            choice_save();
            ui_animate();
            return;
        }
    for (int k = 0; k < LG_COMPONENTS; k++)
        if (in_box(SU.part[k], x, y, sl)) {
            ui_sound(SND_TAP);
            rnd_comp ^= 1u << k;
            choice_save();
            ui_animate();
            return;
        }
    if (ui_button_hit(SU.px, SU.py, SU.pd, x, y)) ui_sound(setup_play() ? SND_TAP : SND_DENY);
}

/* ---- the menu's card: two switches on, an AND, its lamp lit ------------------------------------------------------ */
static void thumb(int variant, int size, uint8_t *rgba)
{
    fresh_progress();                           /* the menu paints it before the level screen opens */
    if (variant >= 0) {
        circuit_thumb(1, size, rgba);           /* (one variant: the setup screen chooses the size) */
        return;
    }
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
    .levels = LG_LEVELS, .chapters = LG_CHAPTERS, .chapter_of = chapter_of, .chapter_icon = paint_emblem,
    .level_stars = level_stars,
    .start_random = start_random, .score = score, .par = par, .tools = TOOLS,
};
REGISTER_GAME(logicgates_game)
