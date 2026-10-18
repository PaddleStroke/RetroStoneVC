/*
 * Defuse the Bomb in RetroStone Board, for one player (the rules: defuse_rules.c, defuse.h; the circuit's text, its
 * gates and its drawing are Logic Gates'): a red bomb casing, a seven-segment timer at its top beside the power lamp,
 * and under it a green circuit board with gold traces: chunky toggle switches, red push buttons and the battery at
 * the bottom, Logic Gates' gates (their brass shapes and names) on black chips, memory chips, coloured jumper cables
 * to cut, and at the top the disarm LEDs (green, lettered A to D), the red DETONATE LED and the power socket.
 * After each move the change runs along the wires, a bright pulse at its front, wire after wire (STEP_MS each); a
 * press runs twice (the button held, then let go). DETONATE lit: the flash, the board shaking, a fireball and smoke,
 * the board scorched; the restart tool lit (a tap on the board starts again). The timer at 0: the same. Disarmed:
 * the timer stops, turns green and the LEDs flash green; then the shell's stars card. The timer only runs while the
 * game is on the screen (frames drawn: a gap, or the clock leaping far, is not counted: Mental Math's way).
 * The board art stays itself in both themes; only the label on the table follows the theme.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/defuse.h"
#include "games/puzzles.h"
#include "pages.h"
#include "shell.h"
#include "text.h"

#define STEP_MS 190                             /* the change along one wire */
#define FLIP_MS 150
#define HOLD_MS 160                             /* a button held down, its change landed, before it is let go */
#define CUT_MS 720
#define MEND_MS 360
#define HINT_MS 2400
#define WIN_MS 1500
#define BOOM_MS 1900
#define AWAY_MS 250                             /* a gap between two frames: the game away, not counted */
#define LEAP_MS 300000                          /* the clock leaping that far (a kept game played again) */
#define SPARKS 12

static df_game G;

/* ---- the layout ----------------------------------------------------------------------------------------------- */
static box B, CS, BAND, PB, panel;              /* the room; the casing; its timer band; the circuit board */
static float lamp_x, lamp_y, lamp_d;            /* the power lamp on the casing */
static int wide;
static float ox, oy, cw, ch, tw, gs = 1, shx, shy;   /* grid (x, y) at (ox + x cw, oy - y ch); the shake */
enum { T_RESET, T_HINT, TOOLS };

static float stretched(float y)
{
    float extra = 0;
    for (int k = 0; gs > 1 && k + 1 < G.D.L.layers; k++) {
        float g0 = G.D.L.ly[k] + LG_NH, g1 = G.D.L.ly[k + 1];
        if (y <= g0) break;
        extra += (fminf(y, g1) - g0) * (gs - 1);
    }
    return y + extra;
}
static float PX(float x) { return ox + shx + x * cw; }
static float PY(float y) { return oy + shy - stretched(y) * ch; }

/* ---- the colours ------------------------------------------------------------------------------------------------ */
static const uint8_t GOLD[3] = {255, 206, 84}, BRASS[3] = {128, 102, 56}, INK[3] = {28, 20, 14},
                     TRACE[3] = {150, 124, 60}, LIT[3] = {255, 214, 86}, SHINE[3] = {255, 240, 180},
                     PCB[3] = {22, 104, 52}, RED[3] = {196, 34, 30}, CHIP[3] = {38, 38, 42};

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

/* ---- a quick painter for the parts' figures (Logic Gates' own: one sample a pixel, its coverage from its distance
 * to the edge) ------------------------------------------------------------------------------------------------------ */
#define FIG_SHAPES 32
#define FIG_PTS 720
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

static void fig_render(const fig *f, const art_style *st, int w, int h, uint8_t *out)
{
    float *acc = calloc((size_t)w * h, 4 * sizeof(float));
    if (!acc || !f) { free(acc); return; }
    float wl = st->line, reach = wl + 1;
    int near[FIG_PTS];
    float cross[FIG_PTS];
    for (int k = 0; k < f->ns; k++) {
        const fig_shape *s = &f->s[k];
        int mark = s->role == ART_MARK;
        int xa = (int)fmaxf(0, floorf(s->x0 - reach)), xb = (int)fminf(w - 1, ceilf(s->x1 + reach));
        int ya = (int)fmaxf(0, floorf(s->y0 - reach)), yb = (int)fminf(h - 1, ceilf(s->y1 + reach));
        float cx = (s->x0 + s->x1) * 0.5f, hw = (s->x1 - s->x0) * 0.5f + 1e-3f, hh = fmaxf(1, s->y1 - s->y0);
        for (int py = ya; py <= yb; py++) {
            float y = py + 0.5f;
            int nn = 0, nc = 0;
            if (!s->ell)
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

/* a figure painted over what o holds */
static void fig_over(fig *f, const art_style *st, int w, int h, uint8_t *o)
{
    uint8_t *b = calloc((size_t)w * h, 4);
    if (b) {
        fig_render(f, st, w, h, b);
        art_blend(o, w, h, b, w, h, 0, 0);
        free(b);
    }
    fig_free(f);
}

static int curve(float *p, int k, float ax, float ay, float bx, float by, float cx, float cy, int n)
{
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n, u = 1 - t;
        p[k++] = u * u * ax + 2 * u * t * bx + t * t * cx;
        p[k++] = u * u * ay + 2 * u * t * by + t * t * cy;
    }
    return k;
}

/* a rounded plate (w x h) of a colour, a faint grain, bevelled */
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

/* ---- the gates: Logic Gates' brass shapes and their names (in English, as on any circuit drawing) --------------- */
static const char *const GNAME[] = {"", "NOT", "AND", "OR", "XOR", "NAND", "NOR"};

/* a word in capitals in an em-high font, engraved (a dark cut, a light edge under it), centred on (cx, cy) */
static void engrave(uint8_t *o, int w, int h, const char *s, float cx, float cy, float em, float maxw,
                    const uint8_t cut[3], const uint8_t edge[3])
{
    int n = (int)strlen(s), c = 0, x0[8], x1[8];
    uint8_t *g[8] = {0};
    static const uint8_t white[3] = {255, 255, 255};
    float total = 0;
    if (n > 8) return;
    for (int pass = 0; pass < 2; pass++) {
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
    for (int layer = edge ? 0 : 1; layer < 2; layer++) {
        float x = cx - total / 2;
        int dy = layer ? 0 : (int)fmaxf(1, 0.06f * em);
        for (int i = 0; i < n; i++) {
            int ox2 = (int)lroundf(x) - x0[i], oy2 = (int)lroundf(cy - c / 2.0f) + dy;
            for (int y = 0; y < c; y++)
                for (int xx = 0; xx < c; xx++) {
                    int px = ox2 + xx, py = oy2 + y;
                    if (px < 0 || py < 0 || px >= w || py >= h) continue;
                    uint8_t *p = o + 4 * (py * w + px);
                    float a = g[i][4 * (y * c + xx) + 3] / 255.0f * (layer ? 0.95f : 0.5f);
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

/* a gate's brass shape, pointing up, in a w x h picture; its output's lead up to the top edge (lw wide); its name.
 * Lit: gold. */
static void paint_gate(int kind, int lit, int w, int h, float lw, int named, uint8_t *o)
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
    } else {
        float back = 0.17f * (yb - yt), gap = kind == LG_XOR ? 0.13f * (yb - yt) : 0;
        float b = yb - gap;
        n = curve(p, n, xl, b, xc, b - 2 * back, xr, b, 16);
        n = curve(p, n, xr, b, xr, yt + 0.42f * (b - yt), xc, yt, 16);
        n = curve(p, n, xc, yt, xl, yt + 0.42f * (b - yt), xl, b, 16);
        fig_poly(f, ART_PART, p, n / 2);
        if (kind == LG_XOR) {
            float t = fmaxf(1.5f, 0.06f * (yb - yt));
            int m = curve(p, 0, xl, yb, xc, yb - 2 * back, xr, yb, 16);
            float q[2 * 40];
            int k = curve(q, 0, xr, yb - t, xc, yb - t - 2 * back, xl, yb - t, 16);
            memcpy(p + m, q, sizeof(float) * (size_t)k);
            fig_poly(f, ART_PART, p, (m + k) / 2);
        }
    }
    if (bubble) fig_ellipse(f, ART_PART, xc, yt - rb, rb, rb);
    fig_render(f, &st, w, h, o);
    fig_free(f);
    if (!named || h < 26 || kind < LG_NOT || kind > LG_NOR) return;
    /* the name across the body where the shape is wide (Logic Gates' places) */
    float body = yb - yt, ff, wf;
    switch (kind) {
    case LG_NOT: ff = 0.8f; wf = 0.58f; break;
    case LG_AND: case LG_NAND: ff = 0.62f; wf = 0.86f; break;
    case LG_XOR: ff = 0.44f; wf = 0.76f; break;
    default: ff = 0.52f; wf = 0.78f;
    }
    const uint8_t edge[3] = {255, 238, 176}, cut[3] = {lit ? 92 : 46, lit ? 52 : 30, lit ? 8 : 12};
    float em = fminf(0.42f * body, 0.3f * w);
    engrave(o, w, h, GNAME[kind], 0.5f * w, yt + ff * body, em, wf * (xr - xl), cut, edge);
}

/* ---- the other parts ---------------------------------------------------------------------------------------------- */
/* a chip: black epoxy, a faint grain, bevelled; a small notch at its top */
static void paint_chip(int w, int h, uint8_t *o)
{
    paint_plate(o, w, h, CHIP, 1.0f, 0.05f);
    float r = 0.09f * fminf(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = x + 0.5f - 0.5f * w, dy = y + 0.5f - 0.1f * h;
            if (dx * dx + dy * dy < r * r) {
                uint8_t *p = o + 4 * (y * w + x);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * 0.5f);
            }
        }
}

/* a memory chip (w x h): a chip, a little window lit amber once it holds a 1, and a hold bar over it */
static void paint_latch(int on, int w, int h, uint8_t *o)
{
    paint_chip(w, h, o);
    float cx = 0.5f * w, cy = 0.56f * h, r = 0.2f * fminf(w, 1.6f * h);
    const uint8_t amber[3] = {255, 178, 40}, dark[3] = {74, 46, 18}, steel[3] = {170, 176, 186};
    art_style st = style(on ? amber : dark, on ? 0.25f : 0.5f, fmaxf(1, 0.03f * h));
    fig *f = fig_new();
    fig_ellipse(f, ART_PART, cx, cy, r, r);
    fig_over(f, &st, w, h, o);
    art_style ss = style(steel, 0.5f, fmaxf(1, 0.025f * h));
    f = fig_new();                              /* a clamp's two jaws round it: it holds */
    float t = 0.12f * r;
    fig_rect(f, ART_PART, cx - 1.55f * r, cy - 0.25f * r, cx - 1.15f * r, cy + 0.25f * r);
    fig_rect(f, ART_PART, cx + 1.15f * r, cy - 0.25f * r, cx + 1.55f * r, cy + 0.25f * r);
    fig_rect(f, ART_PART, cx - 1.55f * r, cy - t, cx - 1.05f * r, cy + t);
    fig_over(f, &ss, w, h, o);
}

/* a toggle switch's plate (w x h): dark steel, a hex nut in its middle, a little lamp at its top (lit: on) */
static void paint_plate_sw(int on, int w, int h, uint8_t *o)
{
    const uint8_t steel[3] = {70, 74, 82};
    paint_plate(o, w, h, steel, 1.0f, 0.03f);
    float cx = 0.5f * w, cy = 0.56f * h, r = 0.3f * w;
    const uint8_t nut[3] = {176, 182, 192}, on_c[3] = {120, 255, 120}, off_c[3] = {40, 60, 40};
    art_style sn = style(nut, 0.6f, fmaxf(1, 0.03f * w));
    fig *f = fig_new();
    float p[12];
    for (int i = 0; i < 6; i++) {
        p[2 * i] = cx + r * cosf(i * 3.14159265f / 3);
        p[2 * i + 1] = cy + r * sinf(i * 3.14159265f / 3);
    }
    fig_poly(f, ART_PART, p, 6);
    fig_over(f, &sn, w, h, o);
    art_style sl = style(on ? on_c : off_c, 0.3f, fmaxf(1, 0.02f * w));
    f = fig_new();
    fig_ellipse(f, ART_PART, cx, 0.12f * h, 0.11f * w, 0.11f * w);
    fig_over(f, &sl, w, h, o);
}

/* a toggle's lever (w x h, its pivot at the middle of the picture, pointing up): a chrome bat, a ball at its end */
static void paint_lever(int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    const uint8_t chrome[3] = {214, 220, 230};
    art_style st = style(chrome, 0.7f, fmaxf(1, 0.04f * w));
    fig *f = fig_new();
    float cx = 0.5f * w, r = 0.3f * w, top = 0.03f * h + r;
    const float bat[] = {cx - 0.16f * w, 0.5f * h, cx + 0.16f * w, 0.5f * h, cx + 0.24f * w, top, cx - 0.24f * w, top};
    fig_poly(f, ART_PART, bat, 4);
    fig_ellipse(f, ART_PART, cx, top, r, r);
    fig_render(f, &st, w, h, o);
    fig_free(f);
}

/* a push button (d x d): a square black base, four screws, a red cap (down: darker, smaller) */
static void paint_button(int down, int d, uint8_t *o)
{
    const uint8_t base[3] = {44, 46, 52}, screw[3] = {150, 156, 166}, cap_up[3] = {226, 44, 36},
                  cap_dn[3] = {150, 26, 22};
    paint_plate(o, d, d, base, 1.0f, 0.03f);
    art_style ss = style(screw, 0.6f, 1);
    fig *f = fig_new();
    for (int k = 0; k < 4; k++) fig_ellipse(f, ART_PART, (k & 1 ? 0.84f : 0.16f) * d, (k & 2 ? 0.84f : 0.16f) * d, 0.06f * d, 0.06f * d);
    fig_over(f, &ss, d, d, o);
    float r = (down ? 0.3f : 0.34f) * d, cy = (down ? 0.52f : 0.48f) * d;
    art_style sc = style(down ? cap_dn : cap_up, down ? 0.3f : 0.7f, fmaxf(1, 0.03f * d));
    f = fig_new();
    fig_ellipse(f, ART_PART, 0.5f * d, cy, r, r);
    fig_over(f, &sc, d, d, o);
}

/* the battery (w x h): an upright cell, its plus end at the top (its output), a gold band, a plus sign */
static void paint_battery(int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    const uint8_t body[3] = {34, 40, 52}, band[3] = {232, 176, 54}, cap[3] = {196, 200, 210}, white[3] = {245, 245, 240};
    float cx = 0.5f * w, bw = 0.4f * w, top = 0.12f * h, bot = 0.96f * h;
    art_style sb = style(body, 0.55f, fmaxf(1, 0.03f * w));
    fig *f = fig_new();
    fig_rect(f, ART_PART, cx - bw, top + 0.04f * h, cx + bw, bot);
    fig_render(f, &sb, w, h, o);
    fig_free(f);
    art_style sg = style(band, 0.5f, 0);
    f = fig_new();
    fig_rect(f, ART_PART, cx - bw + 1, top + 0.04f * h, cx + bw - 1, top + 0.3f * h);
    fig_over(f, &sg, w, h, o);
    art_style sc = style(cap, 0.6f, fmaxf(1, 0.03f * w));
    f = fig_new();
    fig_rect(f, ART_PART, cx - 0.42f * bw, 0, cx + 0.42f * bw, top + 0.05f * h);
    fig_over(f, &sc, w, h, o);
    art_style sw = style(white, 0, 0);
    f = fig_new();
    float py = 0.6f * h, s = 0.42f * bw, t = 0.1f * bw;
    fig_rect(f, ART_MARK, cx - s, py - t, cx + s, py + t);
    fig_rect(f, ART_MARK, cx - t, py - s, cx + t, py + s);
    sw.mark[0] = white[0];
    sw.mark[1] = white[1];
    sw.mark[2] = white[2];
    fig_over(f, &sw, w, h, o);
}

/* a LED (w x h): a dark base with two legs, the dome over it (DF_LED green, DF_DET red); on the dome its letter, or
 * DETONATE's spark */
static void paint_led(int role, int letter, int lit, int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    static const uint8_t base[3] = {40, 42, 48}, leg[3] = {190, 196, 206};
    const uint8_t g_on[3] = {120, 255, 140}, g_off[3] = {24, 74, 40}, r_on[3] = {255, 84, 60}, r_off[3] = {96, 22, 18};
    const uint8_t *dome = role == DF_DET ? (lit ? r_on : r_off) : lit ? g_on : g_off;
    float xc = w / 2.0f, sb = 0.22f * h, r = fminf(0.46f * w, 0.36f * h);
    art_style sl = style(leg, 0.5f, 1);
    fig *f = fig_new();
    fig_rect(f, ART_PART, xc - 0.3f * r - 1, h - sb, xc - 0.3f * r + 1, (float)h);
    fig_rect(f, ART_PART, xc + 0.3f * r - 1, h - sb, xc + 0.3f * r + 1, (float)h);
    fig_render(f, &sl, w, h, o);
    fig_free(f);
    art_style sk = style(base, 0.5f, fmaxf(1, 0.025f * w));
    f = fig_new();
    fig_rect(f, ART_PART, xc - 1.08f * r, h - sb - 0.16f * r, xc + 1.08f * r, h - sb + 0.12f * r);
    fig_over(f, &sk, w, h, o);
    art_style sg = style(dome, lit ? 0.25f : 0.6f, fmaxf(1, 0.025f * w));
    f = fig_new();
    float p[2 * 40];
    int n = 0;
    float y0 = h - sb - 0.16f * r;
    p[n++] = xc - r; p[n++] = y0;
    for (int i = 0; i <= 24; i++) {
        float a = 3.14159265f * i / 24;
        p[n++] = xc - r * cosf(a);
        p[n++] = y0 - 0.9f * r - r * sinf(a);
    }
    p[n++] = xc + r; p[n++] = y0;
    fig_poly(f, ART_PART, p, n / 2);
    fig_over(f, &sg, w, h, o);
    float cy = y0 - 0.95f * r;
    if (role == DF_DET) {                       /* a spark: an eight-pointed star */
        const uint8_t ink_on[3] = {120, 10, 0}, ink_off[3] = {220, 120, 100};
        art_style sm = style(lit ? ink_on : ink_off, 0, 0);
        for (int i = 0; i < 3; i++) sm.mark[i] = (lit ? ink_on : ink_off)[i];
        f = fig_new();
        float q[32];
        for (int i = 0; i < 16; i++) {
            float a = i * 3.14159265f / 8, rr = (i & 1 ? 0.22f : 0.6f) * r;
            q[2 * i] = xc + rr * cosf(a);
            q[2 * i + 1] = cy + rr * sinf(a);
        }
        fig_poly(f, ART_MARK, q, 16);
        fig_over(f, &sm, w, h, o);
    } else {
        const uint8_t on_ink[3] = {14, 70, 30}, off_ink[3] = {226, 240, 228};
        char s[2] = {(char)('A' + letter), 0};
        engrave(o, w, h, s, xc, cy, 1.15f * r, 1.5f * r, lit ? on_ink : off_ink, NULL);
    }
    /* the dome's shine */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = (x + 0.5f - (xc - 0.42f * r)) / (0.26f * r), dy = (y + 0.5f - (cy - 0.45f * r)) / (0.4f * r);
            float a = 1 - (dx * dx + dy * dy);
            uint8_t *q = o + 4 * (y * w + x);
            if (a <= 0 || q[3] < 200) continue;
            a = fminf(1, a) * 0.5f;
            for (int i = 0; i < 3; i++) q[i] = (uint8_t)(q[i] + (255 - q[i]) * a);
        }
}

/* the power symbol (a ring open at the top, a bar through the gap) in rgb, centred, r its radius */
static void power_sign(uint8_t *o, int w, int h, float cx, float cy, float r, const uint8_t rgb[3])
{
    float t = 0.24f * r;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx * dx + dy * dy), a = 0;
            float ang = atan2f(dx, -dy);                    /* 0 at the top */
            if (fabsf(d - r) < t && fabsf(ang) > 0.6f) a = clampf(t - fabsf(d - r), 0, 1);
            if (fabsf(dx) < t * 0.8f && dy < 0.15f * r && dy > -1.25f * r) a = fmaxf(a, clampf(t * 0.8f - fabsf(dx), 0, 1));
            if (a <= 0) continue;
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * (1 - a) + rgb[i] * a);
            p[3] = (uint8_t)fmaxf(p[3], 255 * a);
        }
}

/* the power socket (w x h): a black block, two brass contacts, the power sign over it (lit: amber, the bomb fed) */
static void paint_plug(int lit, int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    const uint8_t block[3] = {36, 36, 40}, brass[3] = {214, 170, 70}, amber[3] = {255, 176, 40},
                  dim[3] = {120, 110, 96};
    float xc = 0.5f * w, bw = 0.42f * w, top = 0.48f * h, bot = 0.96f * h;
    art_style sb = style(block, 0.5f, fmaxf(1, 0.03f * w));
    fig *f = fig_new();
    fig_rect(f, ART_PART, xc - bw, top, xc + bw, bot);
    fig_render(f, &sb, w, h, o);
    fig_free(f);
    art_style sc = style(brass, 0.6f, 1);
    f = fig_new();
    for (int k = 0; k < 2; k++) fig_rect(f, ART_PART, xc + (k ? 0.12f : -0.28f) * bw * 2, top + 0.22f * (bot - top),
                                         xc + (k ? 0.28f : -0.12f) * bw * 2, top + 0.55f * (bot - top));
    fig_over(f, &sc, w, h, o);
    power_sign(o, w, h, xc, 0.26f * h, fminf(0.2f * h, 0.3f * w), lit ? amber : dim);
}

/* the casing's power lamp (d x d): a black bezel, an amber dome (lit: the bomb fed), the power sign on it */
static void paint_power_lamp(int lit, int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    const uint8_t bezel[3] = {30, 26, 26}, on[3] = {255, 186, 50}, off[3] = {92, 62, 26}, ink_on[3] = {120, 60, 0},
                  ink_off[3] = {200, 160, 110};
    float c = d / 2.0f;
    art_style sb = style(bezel, 0.5f, fmaxf(1, 0.03f * d));
    fig *f = fig_new();
    fig_ellipse(f, ART_PART, c, c, 0.48f * d, 0.48f * d);
    fig_render(f, &sb, d, d, o);
    fig_free(f);
    art_style sd = style(lit ? on : off, lit ? 0.3f : 0.6f, fmaxf(1, 0.02f * d));
    f = fig_new();
    fig_ellipse(f, ART_PART, c, c, 0.36f * d, 0.36f * d);
    fig_over(f, &sd, d, d, o);
    power_sign(o, d, d, c, c + 0.02f * d, 0.17f * d, lit ? ink_on : ink_off);
}
static void p_power(int k, int w, int h, uint8_t *o) { (void)h; paint_power_lamp(k, w, o); }

/* a cable's terminal (d x d): a brass post seen from above, slotted (lit: gold) */
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
        fig_over(f, &st, d, d, o);
    }
}

static const uint8_t CABLE[4][3] = {{218, 50, 40}, {44, 104, 214}, {240, 192, 36}, {236, 236, 228}};

/* a cable picture (w x h): a red jumper arcing between two brass posts; cut: two halves apart */
static void paint_cable_pic(int cut, int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    float t = 0.16f * h, pd = 0.3f * h, ax = 0.18f * w, bx = 0.82f * w, y0 = 0.68f * h, top = 0.12f * h;
    for (int half = 0; half < (cut ? 2 : 1); half++) {
        float u0 = cut ? (half ? 0.58f : 0) : 0, u1 = cut ? (half ? 1 : 0.42f) : 1;
        float p[2 * 70];
        int n = 0;
        for (int side = 0; side < 2; side++)
            for (int i = 0; i <= 32; i++) {
                float u = u0 + (u1 - u0) * (side ? 1 - i / 32.0f : i / 32.0f), x = ax + (bx - ax) * u;
                float y = y0 - 4 * (y0 - top) * u * (1 - u) + (side ? t / 2 : -t / 2);
                p[n++] = x;
                p[n++] = y + (cut ? 0.1f * h * fabsf(u - 0.5f) * 0 : 0);
            }
        art_style st = style(CABLE[0], 0.45f, fmaxf(1, 0.04f * h));
        fig *f = fig_new();
        fig_poly(f, ART_PART, p, n / 2);
        fig_over(f, &st, w, h, o);
    }
    uint8_t *b = calloc((size_t)w * h, 4);
    if (!b) return;
    for (int k = 0; k < 2; k++) {
        int d = (int)pd;
        memset(b, 0, (size_t)w * h * 4);
        paint_post(1, d, b);
        art_blend(o, w, h, b, d, d, (int)((k ? bx : ax) - d / 2.0f), (int)(y0 - d / 2.0f));
    }
    free(b);
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

/* ---- the casing, the board, the timer's digits ------------------------------------------------------------------ */
/* the bomb's casing (w x h): red enamel, a lighter rim, a dark edge, a screw in each corner; the timer's window (a
 * box in it: dark glass) */
static void paint_casing(int w, int h, box win, uint8_t *o)
{
    float rad = 0.07f * fminf(w, h), e = fmaxf(2, 0.012f * fminf(w, h));
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            float dx = fmaxf(0, fmaxf(rad - x, x + 1 - (w - rad))), dy = fmaxf(0, fmaxf(rad - y, y + 1 - (h - rad)));
            float d = rad - sqrtf(dx * dx + dy * dy);      /* inside the rounded rect: > 0 */
            float edge = fminf(fminf(x, w - 1 - x), fminf(y, h - 1 - y));
            if (dx > 0 && dy > 0) edge = d;
            if (edge < 0) { memset(p, 0, 4); continue; }
            float k = 0.86f + 0.22f * (1 - (float)y / h) - 0.08f * fabsf((float)x / w - 0.5f);
            float n = sinf(x * 0.37f + y * 0.11f) * sinf(y * 0.23f - x * 0.05f) * 0.02f;
            k += n;
            if (edge < e) k *= 0.45f;                       /* the dark edge */
            else if (edge < 3 * e) k *= 1.18f;              /* the rim, catching the light */
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(RED[i] * k, 0, 255);
            p[3] = (uint8_t)(255 * clampf(edge + 0.5f, 0, 1));
        }
    /* the screws */
    int sd = (int)fmaxf(8, 0.045f * fminf(w, h));
    uint8_t *b = calloc((size_t)sd * sd, 4);
    if (b) {
        const uint8_t steel[3] = {190, 194, 202};
        for (int k = 0; k < 4; k++) {
            memset(b, 0, (size_t)sd * sd * 4);
            art_style st = style(steel, 0.6f, 1);
            fig *f = fig_new();
            fig_ellipse(f, ART_PART, sd / 2.0f, sd / 2.0f, 0.46f * sd, 0.46f * sd);
            float a = 0.6f + k, c = sd / 2.0f, l = 0.34f * sd, t = 0.07f * sd;
            float slot[] = {c - l * cosf(a) - t * sinf(a), c - l * sinf(a) + t * cosf(a), c + l * cosf(a) - t * sinf(a),
                            c + l * sinf(a) + t * cosf(a), c + l * cosf(a) + t * sinf(a), c + l * sinf(a) - t * cosf(a),
                            c - l * cosf(a) + t * sinf(a), c - l * sinf(a) - t * cosf(a)};
            fig_poly(f, ART_MARK, slot, 4);
            fig_render(f, &st, sd, sd, b);
            fig_free(f);
            int m = (int)(rad * 0.55f);
            art_blend(o, w, h, b, sd, sd, k & 1 ? w - m - sd : m, k & 2 ? h - m - sd : m);
        }
        free(b);
    }
    /* the timer's window: dark glass in a black bezel */
    for (int y = (int)win.y; y < (int)(win.y + win.h); y++)
        for (int x = (int)win.x; x < (int)(win.x + win.w); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float ex = fminf(x - win.x, win.x + win.w - 1 - x), ey = fminf(y - win.y, win.y + win.h - 1 - y);
            float r = 0.12f * win.h, cx = fmaxf(0, r - ex), cy = fmaxf(0, r - ey);
            if (cx * cx + cy * cy > r * r) continue;
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            float edge = fminf(ex, ey), bez = fmaxf(2, 0.07f * win.h);
            int g = edge < bez ? 12 : 22 + (int)(8 * (1 - (y - win.y) / win.h));
            p[0] = (uint8_t)g;
            p[1] = (uint8_t)(g * 0.6f);
            p[2] = (uint8_t)(g * 0.6f);
            if (edge >= bez && y < win.y + 0.35f * win.h) {   /* the glass's sheen */
                float a = 0.06f * (1 - (y - win.y) / (0.35f * win.h));
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] + (255 - p[i]) * a);
            }
            p[3] = 255;
        }
}

/* the circuit board (w x h): green solder mask, its light uneven, a thin silkscreen frame, a gold-ringed hole in each
 * corner; a dark shadow round its edge where it sits in the casing */
static void paint_pcb(int w, int h, uint8_t *o)
{
    float sh = 0.02f * fminf(w, h), inset = 0.035f * fminf(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            float e = fminf(fminf(x, w - 1 - x), fminf(y, h - 1 - y));
            float k = 0.9f + 0.14f * (1 - fabsf((float)x / w - 0.5f)) * (1 - fabsf((float)y / h - 0.5f));
            k += 0.03f * sinf(x * 0.9f) * sinf(y * 0.83f);
            if (e < sh) k *= 0.55f + 0.45f * e / sh;
            float fx = fminf(fabsf(x - inset), fabsf(w - 1 - inset - x)), fy = fminf(fabsf(y - inset), fabsf(h - 1 - inset - y));
            int silk = (fx < 0.8f && y > inset && y < h - 1 - inset) || (fy < 0.8f && x > inset && x < w - 1 - inset);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(PCB[i] * k, 0, 255);
            if (silk) { p[0] = (uint8_t)(p[0] + 90); p[1] = (uint8_t)fminf(255, p[1] + 90); p[2] = (uint8_t)(p[2] + 90); }
            p[3] = 255;
        }
    float r = fmaxf(4, 0.022f * fminf(w, h));
    for (int k = 0; k < 4; k++) {
        float cx = k & 1 ? w - 2.4f * inset : 2.4f * inset, cy = k & 2 ? h - 2.4f * inset : 2.4f * inset;
        for (int y = (int)(cy - 2 * r); y <= (int)(cy + 2 * r); y++)
            for (int x = (int)(cx - 2 * r); x <= (int)(cx + 2 * r); x++) {
                if (x < 0 || y < 0 || x >= w || y >= h) continue;
                float d = hypotf(x + 0.5f - cx, y + 0.5f - cy);
                uint8_t *p = o + 4 * ((size_t)y * w + x);
                if (d < 0.55f * r) { p[0] = 10; p[1] = 18; p[2] = 12; }
                else if (d < r) { p[0] = 214; p[1] = 172; p[2] = 74; }
            }
    }
}

/* a seven-segment digit (w x h, white: tinted when drawn): the segments of mask (bit 0 a, the top, .. bit 6 g) */
static const uint8_t SEG[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
static void paint_digit(int mask, int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    const uint8_t white[3] = {255, 255, 255};
    art_style st = style(white, 0, 0);
    for (int i = 0; i < 3; i++) st.mark[i] = 255;
    fig *f = fig_new();
    float t = 0.15f * w, g = 0.03f * h, x0 = 0.14f * w, x1 = 0.86f * w, y0 = 0.06f * h, y1 = 0.94f * h,
          ym = 0.5f * h, sl = 0.1f;                 /* the slant: x moves left going down */
    #define SX(x, y) ((x) + sl * (ym - (y)))
    float hs[3] = {y0, ym, y1};                     /* the three across: a, g, d */
    int hb[3] = {0, 6, 3};
    for (int k = 0; k < 3; k++) {
        if (!(mask >> hb[k] & 1)) continue;
        float y = hs[k];
        float p[] = {SX(x0 + g + t / 2, y), y, SX(x0 + g + t, y - t / 2), y - t / 2, SX(x1 - g - t, y - t / 2), y - t / 2,
                     SX(x1 - g - t / 2, y), y, SX(x1 - g - t, y + t / 2), y + t / 2, SX(x0 + g + t, y + t / 2), y + t / 2};
        if (k == 0) for (int i = 1; i < 12; i += 2) p[i] = fmaxf(p[i], y - 0.01f * h) + t / 2;
        if (k == 2) for (int i = 1; i < 12; i += 2) p[i] = fminf(p[i], y + 0.01f * h) - t / 2;
        fig_poly(f, ART_MARK, p, 6);
    }
    struct { int bit; float x, ya, yb; } V[4] = {{5, x0, y0, ym}, {1, x1, y0, ym}, {4, x0, ym, y1}, {2, x1, ym, y1}};
    for (int k = 0; k < 4; k++) {
        if (!(mask >> V[k].bit & 1)) continue;
        float x = V[k].x + (V[k].x < 0.5f * w ? t / 2 : -t / 2), ya = V[k].ya + g + t / 2, yb = V[k].yb - g - t / 2;
        float p[] = {SX(x, ya - t / 2), ya - t / 2, SX(x + t / 2, ya), ya, SX(x + t / 2, yb), yb,
                     SX(x, yb + t / 2), yb + t / 2, SX(x - t / 2, yb), yb, SX(x - t / 2, ya), ya};
        fig_poly(f, ART_MARK, p, 6);
    }
    #undef SX
    fig_render(f, &st, w, h, o);
    fig_free(f);
}

/* ---- the chapters' pictures, the token, the thumbnails ----------------------------------------------------------- */
static void paint_toggle_pic(int on, int w, int h, uint8_t *o)   /* a whole toggle switch, its lever up when on */
{
    memset(o, 0, (size_t)w * h * 4);
    uint8_t *p = calloc((size_t)w * h, 4), *l = calloc((size_t)w * h, 4);
    if (!p || !l) { free(p); free(l); return; }
    int pw2 = (int)(0.62f * w), ph2 = (int)(0.8f * h);
    paint_plate_sw(on, pw2, ph2, p);
    art_blend(o, w, h, p, pw2, ph2, (w - pw2) / 2, (h - ph2) / 2);
    int lw = (int)(0.36f * w), lh = (int)(0.72f * h);
    paint_lever(lw, lh, l);
    if (!on) {                                  /* pointing down */
        for (int y = 0; y < lh / 2; y++)
            for (int x = 0; x < lw; x++) {
                uint8_t t[4];
                memcpy(t, l + 4 * (y * lw + x), 4);
                memcpy(l + 4 * (y * lw + x), l + 4 * ((lh - 1 - y) * lw + x), 4);
                memcpy(l + 4 * ((lh - 1 - y) * lw + x), t, 4);
            }
    }
    art_blend(o, w, h, l, lw, lh, (w - lw) / 2, (int)((h - ph2) / 2 + 0.56f * ph2 - lh / 2.0f));
    free(p);
    free(l);
}

static void paint_emblem(int c, int size, uint8_t *o)   /* switches; cables; the battery; XOR; buttons; DETONATE */
{
    memset(o, 0, (size_t)size * size * 4);
    int w = (int)(0.8f * size), h = (int)(0.8f * size);
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    switch (c) {
    case 0: paint_toggle_pic(1, w, h, b); break;
    case 1: paint_cable_pic(1, w, h, b); break;
    case 2: w = (int)(0.5f * size); paint_battery(w, h, b); break;
    case 3: paint_gate(LG_XOR, 1, w, h, fmaxf(2, 0.07f * size), 0, b); break;
    case 4: paint_button(0, w, b); h = w; break;
    default: paint_led(DF_DET, 0, 1, w, h, b);
    }
    art_blend(o, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    free(b);
}

static void token(int player, int size, uint8_t *rgba)   /* a disarm LED, lit */
{
    (void)player;
    memset(rgba, 0, (size_t)size * size * 4);
    int w = (int)(0.7f * size), h = (int)(0.96f * size);
    uint8_t *b = calloc((size_t)w * h, 4);
    if (!b) return;
    paint_led(DF_LED, 0, 1, w, h, b);
    art_blend(rgba, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    free(b);
}

/* the menu's card and the die tab's (the difficulties: 2, 3 or 4 disarm LEDs): the casing, its timer, the board, a
 * cable, the LEDs */
static void thumb(int variant, int size, uint8_t *rgba)
{
    float S = (float)size;
    box win = {0.2f * S, 0.08f * S, 0.6f * S, 0.22f * S};
    memset(rgba, 0, (size_t)size * size * 4);
    paint_casing(size, size, win, rgba);
    int pw2 = (int)(0.84f * S), ph2 = (int)(0.56f * S);
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    paint_pcb(pw2, ph2, b);
    art_blend(rgba, size, size, b, pw2, ph2, (int)(0.08f * S), (int)(0.36f * S));
    /* the digits: 0:42 (the menu), 1:00 (the die tab) */
    int dw = (int)(0.11f * S), dh = (int)(0.16f * S);
    int dig[3] = {variant < 0 ? 0 : 1, variant < 0 ? 4 : 0, variant < 0 ? 2 : 0};
    const float xs[3] = {0.27f, 0.47f, 0.62f};
    for (int k = 0; k < 3; k++) {
        memset(b, 0, (size_t)size * size * 4);
        paint_digit(SEG[dig[k]], dw, dh, b);
        for (int i = 0; i < dw * dh; i++) { b[4 * i + 1] = (uint8_t)(b[4 * i + 1] * 0.2f); b[4 * i + 2] = (uint8_t)(b[4 * i + 2] * 0.16f); }
        art_blend(rgba, size, size, b, dw, dh, (int)(xs[k] * S), (int)(0.11f * S));
    }
    for (int k = 0; k < 2; k++) {               /* the colon */
        int y = (int)((0.15f + 0.07f * k) * S), x = (int)(0.41f * S), d = (int)fmaxf(2, 0.025f * S);
        for (int yy = y; yy < y + d; yy++)
            for (int xx = x; xx < x + d; xx++) {
                uint8_t *p = rgba + 4 * (yy * size + xx);
                p[0] = 255; p[1] = 52; p[2] = 40;
            }
    }
    /* the traces: up from the bottom into each LED */
    int nl = variant < 0 ? 2 : 2 + variant, n = nl + 1;
    float t = fmaxf(2, 0.03f * S);
    for (int k = 0; k < n; k++) {
        float x = (0.16f + 0.68f * (k + 0.5f) / n) * S;
        puz_ink_rect(rgba, size, size, x - t / 2, 0.55f * S, x + t / 2, 0.86f * S, k == 0 ? LIT : TRACE, 1);
    }
    /* a red cable over the last trace */
    if (variant < 0) {
        int cw2 = (int)(0.3f * S), chh = (int)(0.2f * S);
        memset(b, 0, (size_t)size * size * 4);
        paint_cable_pic(0, cw2, chh, b);
        art_blend(rgba, size, size, b, cw2, chh, (int)(0.56f * S), (int)(0.62f * S));
    }
    int lw = (int)fminf(0.2f * S, 0.62f * S / n), lh = (int)(1.3f * lw);
    for (int k = 0; k < n; k++) {
        float x = (0.16f + 0.68f * (k + 0.5f) / n) * S;
        int det = k == n - 1;
        memset(b, 0, (size_t)size * size * 4);
        paint_led(det ? DF_DET : DF_LED, k, !det && k == 0, lw, lh, b);
        art_blend(rgba, size, size, b, lw, lh, (int)(x - lw / 2.0f), (int)(0.58f * S - lh));
    }
    free(b);
}

/* ---- the textures ------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_casing, *t_pcb, *t_chip, *t_gate[LG_KINDS][2][2], *t_plate[2], *t_lever, *t_button[2],
    *t_battery, *t_latch[2], *t_led[2][4][2], *t_plug[2], *t_post[2], *t_dot, *t_glow, *t_puff, *t_power[2], *t_digit[11],
    *t_emblem[DF_CHAPTERS], *t_bar_sw, *t_bar_star;
static long long made_room = -1;
static int made_parts = -1, made_shared = -1, made_digits = -1, made_bar = -1;
static float pw, ph, sw_w, sw_h, tg_w, tg_h, lamp_w, lamp_h, post_d, cable_t, gate_w[2], gate_h, dig_w, dig_h;
static puz_font font;

static SDL_Texture *tex_buf(int w, int h, uint8_t *b)
{
    SDL_Texture *t = ui_tex(w, h, b);
    free(b);
    return t;
}

static SDL_Texture *made(SDL_Texture **t, int w, int h, void (*paint)(int, int, int, uint8_t *), int k)
{
    uint8_t *b;
    if (!*t && w > 0 && h > 0 && (b = calloc((size_t)w * h, 4))) {
        paint(k, w, h, b);
        *t = tex_buf(w, h, b);
    }
    return *t;
}

static void p_chip(int k, int w, int h, uint8_t *o) { (void)k; paint_chip(w, h, o); }
static void p_plate(int k, int w, int h, uint8_t *o) { paint_plate_sw(k, w, h, o); }
static void p_lever(int k, int w, int h, uint8_t *o) { (void)k; paint_lever(w, h, o); }
static void p_button(int k, int w, int h, uint8_t *o) { (void)h; paint_button(k, w, o); }
static void p_battery(int k, int w, int h, uint8_t *o) { (void)k; paint_battery(w, h, o); }
static void p_latch(int k, int w, int h, uint8_t *o) { paint_latch(k, w, h, o); }
static void p_plug(int k, int w, int h, uint8_t *o) { paint_plug(k, w, h, o); }
static void p_post(int k, int w, int h, uint8_t *o) { (void)h; paint_post(k, w, o); }
static void p_digit(int k, int w, int h, uint8_t *o) { paint_digit(k < 10 ? SEG[k] : 0x7f, w, h, o); }

static SDL_Texture *gate_tex(int k, int lit, int w3)
{
    SDL_Texture **t = &t_gate[k][lit][w3];
    int gw = (int)gate_w[w3], gh = (int)gate_h;
    uint8_t *b;
    if (!*t && (b = calloc((size_t)gw * gh, 4))) {
        paint_gate(k, lit, gw, gh, tw, 1, b);
        *t = tex_buf(gw, gh, b);
    }
    return *t;
}

static SDL_Texture *led_tex(int role, int letter, int lit)
{
    SDL_Texture **t = &t_led[role == DF_DET][role == DF_DET ? 0 : letter & 3][lit];
    int w = (int)lamp_w, h = (int)lamp_h;
    uint8_t *b;
    if (!*t && (b = calloc((size_t)w * h, 4))) {
        paint_led(role, letter, lit, w, h, b);
        *t = tex_buf(w, h, b);
    }
    return *t;
}

static void free_parts(void)
{
    ui_free(&t_chip);
    ui_free(&t_lever);
    ui_free(&t_battery);
    for (int k = 0; k < LG_KINDS; k++)
        for (int i = 0; i < 4; i++) ui_free(&t_gate[k][i / 2][i % 2]);
    for (int i = 0; i < 2; i++) {
        ui_free(&t_plate[i]);
        ui_free(&t_button[i]);
        ui_free(&t_latch[i]);
        ui_free(&t_plug[i]);
        ui_free(&t_post[i]);
    }
    for (int i = 0; i < 16; i++) ui_free(&t_led[i / 8][i / 2 % 4][i % 2]);
}

static void part_sizes(void)
{
    int key = (int)(cw * 8) * 8192 + (int)(ch * 8);
    if (made_parts == key) return;
    made_parts = key;
    float m = fminf(cw, ch);
    tw = fmaxf(2.5f, roundf(0.05f * m));
    pw = roundf(2 * LG_HALF * cw);
    ph = roundf(LG_NH * ch);
    gate_h = floorf(0.92f * ph);
    for (int w3 = 0; w3 < 2; w3++)
        gate_w[w3] = floorf(fminf(0.94f * pw, fmaxf(0.6f * cw, w3 ? 1.6f * gate_h : 1.2f * gate_h)));
    sw_w = roundf(fminf(2 * LG_SW_HALF * cw * 0.94f, 0.75f * ph));
    sw_h = ph;
    tg_w = floorf(fminf(0.72f * cw, 0.9f * ph));   /* a toggle: chunkier than the slot Logic Gates leaves, and
                                                 * reaching under the bottom row */
    tg_h = floorf(1.3f * ph);
    lamp_h = roundf((LG_NH + LG_PAD_T - 0.04f) * ch);
    lamp_w = roundf(fminf(0.8f * cw, 0.8f * lamp_h));
    cable_t = roundf(fmaxf(7, 2.1f * tw));
    post_d = roundf(1.6f * cable_t) + 1;
    free_parts();
}

static void shared_textures(void)
{
    int key = (int)ui.unit;
    if (made_shared == key) return;
    made_shared = key;
    const uint8_t white[3] = {255, 255, 255};
    int d = (int)(0.5f * ui.unit) | 1;
    ui_free(&t_dot);
    uint8_t *b = calloc((size_t)d * d, 4);
    if (b) { art_disc(d, white, 1, 0, b); t_dot = tex_buf(d, d, b); }
    int g = (int)(2 * ui.unit);
    ui_free(&t_glow);
    b = calloc((size_t)g * g, 4);
    if (b) { art_disc(g, white, 0.85f, 1.6f, b); t_glow = tex_buf(g, g, b); }
    ui_free(&t_puff);
    b = calloc((size_t)g * g, 4);
    if (b) { art_disc(g, white, 1, 0.8f, b); t_puff = tex_buf(g, g, b); }
    int e = (int)(0.7f * ui.unit);
    for (int c = 0; c < DF_CHAPTERS; c++) {
        ui_free(&t_emblem[c]);
        b = calloc((size_t)e * e, 4);
        if (b) { paint_emblem(c, e, b); t_emblem[c] = tex_buf(e, e, b); }
    }
    puz_font_make(&font, (int)(0.6f * ui.unit), 1);
}

/* ---- the layout ------------------------------------------------------------------------------------------------- */
static void room_textures(void)
{
    long long key = ((long long)CS.w * 4096 + (int)CS.h) * 4096 + (int)PB.w;
    if (made_room == key) return;
    made_room = key;
    int W = (int)CS.w, H = (int)CS.h;
    uint8_t *b = calloc((size_t)W * H, 4);
    if (b) {
        box win = {BAND.x - CS.x, BAND.y - CS.y, BAND.w, BAND.h};
        paint_casing(W, H, win, b);
        ui_free(&t_casing);
        t_casing = tex_buf(W, H, b);
    }
    W = (int)PB.w;
    H = (int)PB.h;
    b = calloc((size_t)W * H, 4);
    if (b) {
        paint_pcb(W, H, b);
        ui_free(&t_pcb);
        t_pcb = tex_buf(W, H, b);
    }
    dig_h = floorf(0.72f * BAND.h);
    dig_w = floorf(0.56f * dig_h);
    if (made_digits != (int)dig_h) {
        made_digits = (int)dig_h;
        for (int i = 0; i < 11; i++) ui_free(&t_digit[i]);
        ui_free(&t_power[0]);
        ui_free(&t_power[1]);
    }
}

/* the casing in the room; its timer band at the top; the board under it; the circuit as big as the board allows */
static void fit(void)
{
    float u = ui.unit, m = fmaxf(4, 0.025f * fminf(B.w, B.h));
    CS = (box){floorf(B.x + m), floorf(B.y + m), floorf(B.w - 2 * m), floorf(B.h - 2 * m)};
    float in = fmaxf(6, 0.04f * fminf(CS.w, CS.h));
    if (wide) {                                 /* lying: the timer in a column on the left, the lamp under it */
        float col = floorf(clampf(0.27f * CS.w, 3.2f * u, 0.4f * CS.w)), bw = col - 1.6f * in;
        BAND = (box){floorf(CS.x + 1.5f * in), floorf(CS.y + 2.6f * in), floorf(bw - 0.6f * in), 0};
        BAND.h = floorf(fminf(BAND.w / 2.6f, 2.1f * u));
        lamp_d = floorf(0.75f * BAND.h);
        lamp_x = BAND.x + BAND.w / 2;
        lamp_y = BAND.y + BAND.h + 0.5f * in + lamp_d;
        PB = (box){CS.x + col + 0.4f * in, CS.y + in, 0, CS.h - 2 * in};
        PB.w = floorf(CS.x + CS.w - in - PB.x);
    } else {
        float bh = clampf(0.15f * CS.h, 1.25f * u, 2.1f * u), bw = fminf(CS.w - 2 * in - 2.4f * bh, 4.2f * bh);
        BAND = (box){floorf(CS.x + (CS.w - bw) / 2 + 0.5f * bh), floorf(CS.y + in), floorf(bw), floorf(bh)};
        lamp_d = floorf(0.66f * bh);
        lamp_x = BAND.x - 0.7f * bh;
        lamp_y = BAND.y + bh / 2;
        PB = (box){CS.x + in, floorf(BAND.y + bh + 0.6f * in), CS.w - 2 * in, 0};
        PB.h = floorf(CS.y + CS.h - in - PB.y);
    }
    panel = PB;
    room_textures();
    if (!G.D.L.nn) return;
    float mm = 0.06f * fminf(PB.w, PB.h), iw = PB.w - 2 * mm, ih = PB.h - 2 * mm;
    lg_part_size(&G.D.L, iw, ih, &cw, &ch);
    cw = floorf(cw);
    ch = floorf(ch);
    float cols = G.D.L.width, span = G.D.L.height + LG_PAD_B + LG_PAD_T, gaps = 0;
    for (int k = 0; k + 1 < G.D.L.layers; k++) gaps += G.D.L.ly[k + 1] - G.D.L.ly[k] - LG_NH;
    gs = 1;
    if (gaps > 0 && ih > span * ch) {
        float most = 2.2f * LG_NH * (G.D.L.layers - 1) / gaps;
        gs = fmaxf(1, fminf(1 + (ih - span * ch) / (gaps * ch), most));
    }
    span += gaps * (gs - 1);
    ox = floorf(PB.x + PB.w / 2 - cols * cw / 2);
    oy = floorf(PB.y + PB.h / 2 + (span / 2 - LG_PAD_B) * ch);
    part_sizes();
}

static void layout(box b)
{
    B = b;
    wide = b.w > 1.08f * b.h;
    ui_tools_shown(TOOLS);
    shared_textures();
    fit();
}

/* ---- the clock: runs while frames are drawn, a gap not counted ------------------------------------------------- */
static struct { Uint32 last, real, ms; int stop; } clk;
static int last_sec;

static void clock_frame(void)
{
    if (clk.stop) return;
    Uint32 now = ui_ms(), real = ui_real_ms();
    if (clk.last && real - clk.real <= AWAY_MS && now - clk.last <= LEAP_MS) clk.ms += now - clk.last;
    clk.last = now;
    clk.real = real;
}
static int left_ms(void) { int l = G.D.secs * 1000 - (int)clk.ms; return l < 0 ? 0 : l; }

/* ---- the change running along the wires ------------------------------------------------------------------------- */
typedef struct {
    Uint32 t0;                                  /* 0: none */
    df_values a, b;                             /* the values before, after */
    float wst[LG_WIRES];                        /* when the change sets off along each wire (ms after t0) */
    float arr[LG_NODES];                        /* when each part shows its new value */
    float end;
    int origin, held;                           /* the part moved; a button held down in this one */
} wave;
static wave WV[2];
static int nwv;
static Uint32 now_ms;
static struct { int wire; Uint32 t; int mend; } snip = {-1, 0, 0};
static struct { int node; Uint32 t; } flip = {-1, 0};
static struct { int on; df_move m; Uint32 t; } hint;
static Uint32 deny_reset, boom_t, win_t;
static int boom_at;                             /* the part it blew at (DETONATE), -1 the timer */
static int boomed;                              /* its sound played */

static int changed(const wave *v, int w) { return v->a.wsrc[w] != v->b.wsrc[w] || v->a.wend[w] != v->b.wend[w]; }

static void wave_make(wave *v, const df_values *a, const df_values *b, int origin, int cutw, Uint32 t0)
{
    const lg_level *L = &G.D.L;
    v->a = *a;
    v->b = *b;
    v->t0 = t0 ? t0 : 1;
    v->origin = origin;
    v->held = 0;
    v->end = 0;
    for (int w = 0; w < L->nw; w++) v->wst[w] = 0;
    for (int i = 0; i < L->nn; i++) {
        float m = -1;
        if (i == origin) m = 0.6f * FLIP_MS;
        for (int k = 0; k < L->n[i].nin; k++) {
            int w = L->n[i].in[k];
            if (changed(v, w)) m = fmaxf(m, v->wst[w] + STEP_MS);
        }
        v->arr[i] = m < 0 ? 0 : m;
        if (v->arr[i] > v->end) v->end = v->arr[i];
        for (int w = 0; w < L->nw; w++)
            if (L->w[w].from == i) {
                v->wst[w] = v->arr[i];
                if (w == cutw) v->wst[w] = -STEP_MS * L->w[w].fs / fmaxf(0.01f, L->w[w].len);
                if (changed(v, w) && v->wst[w] + STEP_MS > v->end) v->end = v->wst[w] + STEP_MS;
            }
    }
}

static const wave *cur_wave(void)                /* the wave showing now (the last begun), or NULL */
{
    for (int k = nwv - 1; k >= 0; k--)
        if (now_ms >= WV[k].t0 || k == 0) return &WV[k];
    return NULL;
}
static float wave_t(const wave *v) { return v ? (float)(int)(now_ms - v->t0) : 1e9f; }

static int out_of(int i)                         /* a part's output as shown */
{
    const wave *v = cur_wave();
    if (!v) return G.v.out[i];
    return wave_t(v) >= v->arr[i] ? v->b.out[i] : v->a.out[i];
}
static int blown(void) { return G.over == DF_LOST && boom_t && now_ms >= boom_t; }
static int powered_shown(void) { return !blown() && (G.D.plug < 0 || out_of(G.D.plug)); }

static float front(int w)
{
    const lg_wire *W = &G.D.L.w[w];
    const wave *v = cur_wave();
    if (!v) return W->len + 1;
    float f = (wave_t(v) - v->wst[w]) / STEP_MS;
    return clampf(f, 0, 1) * (W->len + 0.01f);
}

static int lit_at(int w, float s, float f)
{
    const lg_wire *W = &G.D.L.w[w];
    const wave *v = cur_wave();
    const df_values *a = v ? &v->a : &G.v, *b = v ? &v->b : &G.v;
    int fresh = s <= f;
    float fs = W->cuttable ? W->fs : W->len + 1;
    if (s < fs) return fresh ? b->wsrc[w] : a->wsrc[w];
    return fresh ? b->wend[w] : a->wend[w];
}

static int animating(void)
{
    for (int k = 0; k < nwv; k++)
        if ((float)(int)(now_ms - WV[k].t0) < WV[k].end + 40) return 1;
    return 0;
}

static void calm(void)
{
    nwv = 0;
    snip.wire = -1;
    flip.node = -1;
    hint.on = 0;
    deny_reset = 0;
    boom_t = win_t = 0;
    boomed = 0;
}

/* ---- the level --------------------------------------------------------------------------------------------------- */
static void fresh_clock(void)
{
    memset(&clk, 0, sizeof clk);
    last_sec = -1;
}

static void opened(void)
{
    calm();
    fresh_clock();
    made_parts = -1;
    if (B.w > 0) fit();
}

static void start(int level)
{
    if (!df_open(&G, level)) df_open(&G, 0);
    opened();
}

static void start_random(int variant)
{
    uint32_t seed = (uint32_t)ui_random(0x7fffffff) + 1u;
    if (!df_random(&G, variant, seed)) df_open(&G, 0);
    opened();
}

static int chapter_of(int level) { return df_chapter(level); }
static int status(void) { return G.over == DF_WON ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return df_stars(&G); }
static int score(void) { return G.nh; }
static int par(void) { return G.par; }

static int busy(void)
{
    now_ms = ui_ms();
    if (animating()) return 1;
    if (flip.node >= 0 && now_ms - flip.t < FLIP_MS) return 1;
    if (snip.wire >= 0 && now_ms - snip.t < (Uint32)(snip.mend ? MEND_MS : CUT_MS)) return 1;
    if (G.over == DF_WON && win_t && (int)(now_ms - win_t) < WIN_MS) return 1;
    return G.over == DF_LOST && boom_t && (int)(now_ms - boom_t) < BOOM_MS;
}

/* the move's waves from what showed before; its outcome: the explosion's time, or the win's */
static void moved(const df_values *before, const df_values *held, int origin, int cutw, int r)
{
    Uint32 t0 = ui_ms();
    nwv = 0;
    if (held) {                                 /* pressed: the change of the button held, then of it let go */
        wave_make(&WV[0], before, held, origin, -1, t0);
        WV[0].held = 1;
        nwv = 1;
        if (!(held->det || held->win)) {
            wave_make(&WV[1], held, &G.v, origin, -1, t0 + (Uint32)(WV[0].end + HOLD_MS));
            nwv = 2;
        }
    } else {
        wave_make(&WV[0], before, &G.v, origin, cutw, t0);
        nwv = 1;
    }
    const wave *last = &WV[nwv - 1];
    if (r == DF_LOST) {
        boom_at = G.D.det;
        boom_t = last->t0 + (Uint32)(last->arr[G.D.det] + 420);   /* (DETONATE lit a moment first) */
    }
    if (r == DF_WON) win_t = last->t0 + (Uint32)last->end;
    if (r != DF_SAFE) clk.stop = 1;
    ui_animate();
}

static void boom_sound(void)
{
    ui_sound(SND_CAPTURE);
    ui_sound_later(SND_DROP, 60);
    ui_sound_later(SND_DICE, 140);
}

static int undo(void)
{
    if (G.over == DF_LOST) return 0;
    df_values before = G.v;
    int was = G.nh;
    df_move m = was > 0 && was <= DF_HIST ? G.hist[was - 1] : (df_move){0, 0};
    if (!df_undo(&G)) return 0;
    calm();
    int cutw = m.kind == DF_MCUT ? G.D.L.cut[m.idx] : -1;
    moved(&before, NULL, m.kind == DF_MCUT ? -1 : G.D.sw[m.idx], -1, DF_SAFE);
    if (cutw >= 0) {
        snip.wire = cutw;
        snip.t = WV[0].t0;
        snip.mend = 1;
    }
    clk.stop = 0;
    return 1;
}

static void restart(void)
{
    df_restart(&G);
    calm();
    fresh_clock();
}

static void do_move(df_move m, int node, int wire)
{
    if (G.over) return;
    df_values before = G.v, held;
    uint32_t ns;
    int press = m.kind == DF_MPRESS;
    if (press && df_step(&G.D, G.state, m, &ns, &held) < 0) { ui_sound(SND_DENY); return; }
    int r = df_play(&G, m);
    if (r < 0) { ui_sound(SND_DENY); return; }
    hint.on = 0;
    deny_reset = 0;
    snip.wire = -1;
    flip.node = -1;
    moved(&before, press ? &held : NULL, node, wire, r);
    if (m.kind == DF_MCUT) {
        snip.wire = wire;
        snip.t = WV[0].t0;
        snip.mend = 0;
        ui_sound(SND_PLACE);
        ui_sound_later(SND_TAP, 45);
    } else {
        flip.node = node;
        flip.t = WV[0].t0;
        ui_sound(press ? SND_TAP : SND_MOVE);
    }
}

/* the notation (defuse.h): the moves as taps would make them (the last one's change runs: --at catches it); "w30":
 * the timer 30 seconds on */
static int script(const char *moves)
{
    now_ms = ui_ms();
    for (const char *s = moves; *s;) {
        if (*s == ' ') { s++; continue; }
        char c = *s++;
        int n = 0;
        while (*s >= '0' && *s <= '9') n = n * 10 + *s++ - '0';
        if (c == 'w') {
            clk.ms += (Uint32)n * 1000u;
            continue;
        }
        if (c == 'u') { if (!undo()) return 0; continue; }
        if (c == 'x') { restart(); continue; }
        if (c == 'h') {
            if (!df_hint(&G, &hint.m)) return 0;
            hint.on = 1;
            hint.t = ui_ms();
            G.hinted = 1;
            continue;
        }
        if (c == '=') {
            char eq[2] = "=";
            if (!df_script(&G, eq)) return 0;
            calm();
            win_t = ui_ms() - WIN_MS;
            clk.stop = 1;
            continue;
        }
        if ((c != 's' && c != 'b' && c != 'c') || n < 1 || G.over) return 0;
        df_move m = {(uint8_t)(c == 's' ? DF_MFLIP : c == 'b' ? DF_MPRESS : DF_MCUT), (uint8_t)(n - 1)};
        if (c == 'c' && m.idx >= G.D.ncut) return 0;
        if (c != 'c' && m.idx >= G.D.nsw) return 0;
        int was = G.nh;
        do_move(m, c == 'c' ? -1 : G.D.sw[m.idx], c == 'c' ? G.D.L.cut[m.idx] : -1);
        if (G.nh == was) return 0;
    }
    return 1;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void trace(float x0, float y0, float x1, float y1, int lit, int layer)
{
    float t = tw, xa = fminf(x0, x1) - t / 2, ya = fminf(y0, y1) - t / 2, w = fabsf(x1 - x0) + t, h = fabsf(y1 - y0) + t;
    if (layer == 0) {
        float g = 1.1f * t;
        SDL_FRect q = {xa - g, ya - g, w + 2 * g, h + 2 * g};
        SDL_RenderFillRectF(ui.r, &q);
    } else if (layer == 1) {
        ui_fill(xa - 1, ya - 1, w + 2, h + 2, 8, 40, 18, 255);
    } else {
        const uint8_t *c = lit ? LIT : TRACE;
        ui_fill(xa, ya, w, h, c[0], c[1], c[2], 255);
        if (lit) {
            int vert = fabsf(x1 - x0) < fabsf(y1 - y0);
            float e = layer == 3 ? 0.5f * t : 0.15f * t;
            if (vert) ui_fill(xa + 0.35f * t, fminf(y0, y1) - e, 0.3f * t, h - t + 2 * e, SHINE[0], SHINE[1], SHINE[2], 255);
            else ui_fill(fminf(x0, x1) - e, ya + 0.35f * t, w - t + 2 * e, 0.3f * t, SHINE[0], SHINE[1], SHINE[2], 255);
        }
    }
}

static float hop_r(void) { return fmaxf(4, 1.8f * tw); }

static void stretch(float x0, float y0, float x1, float y1, int net, int lit, int layer)
{
    if (fabsf(y1 - y0) > 1e-4f) {
        trace(PX(x0), PY(y0), PX(x1), PY(y1), lit, layer);
        return;
    }
    float a = fminf(x0, x1), b = fmaxf(x0, x1), r = hop_r() / cw;
    while (a < b) {
        float next = 1e9f;
        for (int h = 0; h < G.D.L.nhop; h++) {
            const lg_mark *m = &G.D.L.hop[h];
            if (G.D.L.w[m->wire].net == net && fabsf(m->y - y0) < 1e-3f && m->x + r > a && m->x - r < b && m->x < next)
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

static void point_at(const lg_wire *W, float s, float *x, float *y)   /* the place arc s along a wire (grid) */
{
    float acc = 0;
    for (int i = 0; i + 1 < W->npt; i++) {
        float x0 = W->px[i], y0 = W->py[i], x1 = W->px[i + 1], y1 = W->py[i + 1];
        float len = fabsf(x1 - x0) + fabsf(y1 - y0);
        if (len < 1e-4f) continue;
        if (s <= acc + len || i + 2 == W->npt) {
            float f = clampf((s - acc) / len, 0, 1);
            *x = x0 + (x1 - x0) * f;
            *y = y0 + (y1 - y0) * f;
            return;
        }
        acc += len;
    }
    *x = W->px[0];
    *y = W->py[0];
}

static void marks(void)
{
    static const uint8_t EDGE[3] = {8, 40, 18};
    float r = hop_r(), d = 2.1f * tw;
    for (int pass = 0; pass < 2; pass++) {
        for (int h = 0; h < G.D.L.nhop; h++) {
            const lg_mark *m = &G.D.L.hop[h];
            const uint8_t *c = pass ? (lit_at(m->wire, m->s, front(m->wire)) ? LIT : TRACE) : EDGE;
            float cx = PX(m->x), cy = PY(m->y), s = pass ? tw : tw + 2;
            SDL_SetTextureColorMod(t_dot, c[0], c[1], c[2]);
            for (int i = 0; i <= 18; i++) {
                float a = 3.14159265f * i / 18;
                ui_put_c(t_dot, cx - r * cosf(a), cy - r * sinf(a), s, s);
            }
        }
        for (int j = 0; j < G.D.L.njoint; j++) {
            const lg_mark *m = &G.D.L.joint[j];
            const uint8_t *c = pass ? (lit_at(m->wire, m->s, front(m->wire)) ? LIT : TRACE) : EDGE;
            float s = pass ? d : d + 2;
            SDL_SetTextureColorMod(t_dot, c[0], c[1], c[2]);
            ui_put_c(t_dot, PX(m->x), PY(m->y), s, s);
        }
    }
    SDL_SetTextureColorMod(t_dot, 255, 255, 255);
}

static int pieces(int w, float *a, float *b, int *lit)
{
    const lg_wire *W = &G.D.L.w[w];
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
        lit[k++] = lit_at(w, m, f) && !blown();
    }
    return k;
}

static SDL_Texture *t_glowbuf;
static int glow_w, glow_h;

static void glows(void)
{
    static const uint8_t C[2][4] = {{255, 196, 60, 30}, {255, 214, 90, 60}};
    int W = (int)ceilf(B.w), H = (int)ceilf(B.h);
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
    float sox = ox, soy = oy;
    for (int pass = 1; pass <= 2; pass++) {
        SDL_Texture *was = SDL_GetRenderTarget(ui.r);
        SDL_SetRenderTarget(ui.r, t_glowbuf);
        SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ui.r, 0, 0, 0, 0);
        SDL_RenderClear(ui.r);
        const uint8_t *c = C[pass - 1];
        SDL_SetRenderDrawColor(ui.r, c[0], c[1], c[2], 255);
        ox = sox - B.x;                         /* (the buffer: the room's own places) */
        oy = soy - B.y;
        float stw = tw;
        if (pass == 2) tw *= 0.6f;
        for (int w = 0; w < G.D.L.nw; w++) {
            int k = pieces(w, a, b, lit);
            for (int i = 0; i < k; i++)
                if (lit[i])
                    for (int vert = 0; vert < 2; vert++) path(&G.D.L.w[w], a[i], b[i], vert, 1, 0);
        }
        tw = stw;
        ox = sox;
        oy = soy;
        SDL_SetRenderDrawBlendMode(ui.r, mode);
        SDL_SetRenderTarget(ui.r, was);
        SDL_SetTextureAlphaMod(t_glowbuf, c[3]);
        ui_put(t_glowbuf, B.x, B.y, (float)W, (float)H);
    }
}

static void draw_wires(void)
{
    float a[6], b[6];
    int lit[6];
    glows();
    for (int layer = 1; layer < 3; layer++)
        for (int on = 0; on < 2; on++)
            for (int vert = 0; vert < 2; vert++)
                for (int w = 0; w < G.D.L.nw; w++) {
                    int k = pieces(w, a, b, lit);
                    for (int i = 0; i < k; i++)
                        if (lit[i] == on) path(&G.D.L.w[w], a[i], b[i], vert, on, layer);
                }
    marks();
}

static void glow(float x, float y, float d, float a, const uint8_t rgb[3])
{
    SDL_SetTextureColorMod(t_glow, rgb[0], rgb[1], rgb[2]);
    SDL_SetTextureAlphaMod(t_glow, (Uint8)clampf(255 * a, 0, 255));
    ui_put_c(t_glow, x, y, d, d);
    SDL_SetTextureAlphaMod(t_glow, 255);
    SDL_SetTextureColorMod(t_glow, 255, 255, 255);
}

/* the pulses: a bright head at the front of every wire the change runs along */
static void pulses(void)
{
    const wave *v = cur_wave();
    if (!v || blown()) return;
    float t = wave_t(v);
    static const uint8_t hot[3] = {255, 236, 160}, cool[3] = {200, 230, 255};
    for (int w = 0; w < G.D.L.nw; w++) {
        if (!changed(v, w)) continue;
        float q = (t - v->wst[w]) / STEP_MS;
        if (q <= 0 || q >= 1) continue;
        const lg_wire *W = &G.D.L.w[w];
        float gx, gy, s = q * W->len;
        point_at(W, s, &gx, &gy);
        int up = lit_at(w, s - 1e-3f, s);
        float x = PX(gx), y = PY(gy), d = (up ? 6.0f : 4.4f) * tw;
        glow(x, y, 3.2f * d, up ? 0.9f : 0.6f, up ? hot : cool);
        glow(x, y, 1.4f * d, 1, up ? hot : cool);
        SDL_SetTextureColorMod(t_dot, 255, 255, 255);
        ui_put_c(t_dot, x, y, 0.55f * d, 0.55f * d);
    }
    ui_animate();
}

/* ---- the cables -------------------------------------------------------------------------------------------------- */
static int cable_no(int w)
{
    for (int i = 0; i < G.D.L.ncut; i++)
        if (G.D.L.cut[i] == w) return i;
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

static void cable_point(const cable_geo *c, float t, float *x, float *y)
{
    float u = 1 - t, k = 4.0f / 3, sag = 0.07f * c->len;
    float p1x = c->ax + (c->bx - c->ax) / 3 + c->nx * c->bow * k, p1y = c->ay + (c->by - c->ay) / 3 + c->ny * c->bow * k;
    float p2x = c->ax + 2 * (c->bx - c->ax) / 3 + c->nx * c->bow * k,
          p2y = c->ay + 2 * (c->by - c->ay) / 3 + c->ny * c->bow * k;
    p1y += sag;
    p2y += sag;
    *x = u * u * u * c->ax + 3 * u * u * t * p1x + 3 * u * t * t * p2x + t * t * t * c->bx;
    *y = u * u * u * c->ay + 3 * u * u * t * p1y + 3 * u * t * t * p2y + t * t * t * c->by;
}

static void half_point(const cable_geo *c, int side, float f, float t, float *x, float *y)
{
    float mx, my, sx = side ? c->bx : c->ax, sy = side ? c->by : c->ay;
    cable_point(c, 0.5f, &mx, &my);
    float vx = mx - sx, vy = my - sy, l = hypotf(vx, vy) + 1e-3f, s = c->bow < 0 ? -1.0f : 1.0f;
    float ox2 = c->nx * s, oy2 = c->ny * s;
    float gx = fabsf(vx) > fabsf(vy) ? 0 : ox2, gy = fabsf(vx) > fabsf(vy) ? 1 : oy2;
    float ex = sx + 0.55f * vx + 0.5f * l * gx, ey = sy + 0.55f * vy + 0.5f * l * gy;
    ex = mx + (ex - mx) * f;
    ey = my + (ey - my) * f;
    float jx, jy;
    cable_point(c, side ? 0.75f : 0.25f, &jx, &jy);
    float kx = sx + 0.5f * vx + 0.12f * l * ox2, ky = sy + 0.5f * vy + 0.12f * l * oy2;
    float cx = jx + f * (kx - jx), cy = jy + f * (ky - jy);
    float u = 1 - t;
    *x = u * u * sx + 2 * u * t * cx + t * t * ex;
    *y = u * u * sy + 2 * u * t * cy + t * t * ey;
}

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
            float hx = 0, hy = 0;
            if (pass == 4) {
                hx = sy / l;
                hy = -sx / l;
                if (hx + hy > 0) { hx = -hx; hy = -hy; }
                hx *= 0.2f * t;
                hy *= 0.2f * t;
            }
            for (float s = carry; s < l; s += step) ui_put_c(t_dot, px[i] + sx * s / l + hx + dx, py[i] + sy * s / l + hy + dy, d, d);
            carry = fmodf(carry - l, step);
            if (carry < 0) carry += step;
        }
    }
    if (copper) {
        float ex = px[n - 1], ey = py[n - 1], ux = ex - px[n - 3], uy = ey - py[n - 3], l = hypotf(ux, uy) + 1e-3f;
        ux /= l;
        uy /= l;
        static const float ang[3] = {-0.4f, 0, 0.4f};
        for (int k = 0; k < 3; k++) {
            float c = cosf(ang[k]), s = sinf(ang[k]), vx = ux * c - uy * s, vy = ux * s + uy * c;
            SDL_SetTextureColorMod(t_dot, 120, 62, 26);
            SDL_SetTextureAlphaMod(t_dot, 255);
            for (int i = 0; i < 6; i++) ui_put_c(t_dot, ex + vx * i * 0.16f * t, ey + vy * i * 0.16f * t, 0.36f * t, 0.36f * t);
            SDL_SetTextureColorMod(t_dot, 236, 150, 84);
            for (int i = 0; i < 6; i++) ui_put_c(t_dot, ex + vx * i * 0.16f * t, ey + vy * i * 0.16f * t, 0.22f * t, 0.22f * t);
        }
    }
    SDL_SetTextureColorMod(t_dot, 255, 255, 255);
    SDL_SetTextureAlphaMod(t_dot, 255);
}

static float spring(float q) { return q <= 0 ? 0 : q >= 1 ? 1 : 1 - expf(-5.5f * q) * cosf(9.5f * q); }

static void cable_spark(float cx, float cy, float q, int seed)
{
    float m = fminf(cw, ch);
    if (q < 0.18f) {
        ui_mark_gold(cx, cy, m * (0.35f + 2.2f * q));
        SDL_SetTextureColorMod(t_dot, 255, 250, 230);
        SDL_SetTextureAlphaMod(t_dot, (Uint8)(255 * (1 - q / 0.18f)));
        ui_put_c(t_dot, cx, cy, cable_t * (1.6f + 4 * q), cable_t * (1.6f + 4 * q));
    }
    for (int k = 0; k < SPARKS; k++) {
        float a = k * 6.2831853f / SPARKS + 0.5f * sinf(k * 7.1f + seed);
        float sp = 0.3f + 0.3f * (k * 37 % 7) / 7.0f;
        float r = m * sp * puz_ease(fminf(1, q * 1.6f)), x = cx + r * cosf(a), y = cy + r * sinf(a) + m * 0.3f * q * q;
        float s = fmaxf(3, 1.3f * tw * (1 - 0.8f * q));
        SDL_SetTextureColorMod(t_dot, 255, (Uint8)(250 - 90 * q), (Uint8)(200 - 160 * q));
        SDL_SetTextureAlphaMod(t_dot, (Uint8)(255 * (1 - q)));
        ui_put_c(t_dot, x, y, s, s);
    }
    SDL_SetTextureColorMod(t_dot, 255, 255, 255);
    SDL_SetTextureAlphaMod(t_dot, 255);
}

static void draw_cables(void)
{
    for (int w = 0; w < G.D.L.nw; w++) {
        const lg_wire *W = &G.D.L.w[w];
        if (!W->cuttable) continue;
        cable_geo c = cable_at(W);
        float f = front(w);
        int la = lit_at(w, W->ta - 1e-3f, f) && !blown(), lb = lit_at(w, W->tb + 1e-3f, f) && !blown();
        int k = cable_no(w), cut = (G.state & df_bit_cut(&G.D, k)) != 0;
        const uint8_t *col = CABLE[k % 4];
        float open = cut ? 1 : 0, q = -1;
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
        if (!cut && open <= 0) {
            for (int i = 0; i < 40; i++) cable_point(&c, i / 39.0f, &px[0][i], &py[0][i]);
            tube(px[0], py[0], 40, col, la, 0);
        } else {
            for (int s = 0; s < 2; s++) {
                for (int i = 0; i < 20; i++) half_point(&c, s, open, i / 19.0f, &px[s][i], &py[s][i]);
                tube(px[s], py[s], 20, col, s ? lb : la, open > 0.05f);
            }
        }
        for (int s = 0; s < 2; s++)
            ui_put_c(made(&t_post[s ? lb : la], (int)post_d, (int)post_d, p_post, s ? lb : la), s ? c.bx : c.ax,
                     s ? c.by : c.ay, post_d, post_d);
        float mx, my;
        cable_point(&c, 0.5f, &mx, &my);
        if (q >= 0) cable_spark(mx, my, q, w);
    }
}

static int cable_hit(float x, float y, float *dist)
{
    int best = -1;
    float bd = 1e9f;
    for (int n = 0; n < G.D.L.ncut; n++) {
        cable_geo c = cable_at(&G.D.L.w[G.D.L.cut[n]]);
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

/* ---- the parts ----------------------------------------------------------------------------------------------------- */
static void node_box(int i, float *x0, float *y0, float *x1, float *y1)
{
    const lg_node *N = &G.D.L.n[i];
    float cx = PX(N->x), hw = N->kind == LG_SWITCH ? fmaxf(tg_w / 2, 0.3f * cw) : pw / 2;
    *x0 = cx - hw;
    *x1 = cx + hw;
    *y0 = PY(N->y + LG_NH);
    *y1 = N->kind == LG_SWITCH ? *y0 + tg_h : PY(N->y);
}

static void leads(int i, float yb, float top)   /* a part's inputs' leads, up into it */
{
    const lg_node *N = &G.D.L.n[i];
    for (int k = 0; k < N->nin; k++) {
        int w = N->in[k];
        const lg_wire *W = &G.D.L.w[w];
        int lit = lit_at(w, W->len, front(w)) && !blown();
        float x = PX(W->px[W->npt - 1]);
        trace(x, yb - 0.5f * tw - 1, x, top, lit, 1);
        trace(x, yb, x, top, lit, 3);
    }
}

static void draw_node(int i)
{
    const lg_node *N = &G.D.L.n[i];
    int role = G.D.role[i];
    float cx = PX(N->x), yt = PY(N->y + LG_NH), yb = PY(N->y);
    int out = out_of(i) && !blown();
    switch (role) {
    case DF_SW: {
        int k = 0;
        while (k < G.D.nsw && G.D.sw[k] != i) k++;
        int on = (int)(G.state >> k & 1);
        float p = (float)on;
        if (flip.node == i && now_ms - flip.t < FLIP_MS) {
            float q = puz_ease((now_ms - flip.t) / (float)FLIP_MS);
            p = on ? q : 1 - q;
            ui_animate();
        }
        ui_shadow_flat(cx + 0.05f * tg_w, yt + tg_h / 2 + 0.06f * tg_w, tg_w, tg_h, 0.5f);
        ui_put(made(&t_plate[on], (int)tg_w, (int)tg_h, p_plate, on), cx - tg_w / 2, yt, tg_w, tg_h);
        float lw = floorf(0.5f * tg_w), lh = floorf(0.84f * tg_h), ext = cosf(3.14159265f * (1 - p));
        float py0 = yt + 0.56f * tg_h, h = fmaxf(2, fabsf(ext) * lh);
        SDL_Texture *lv = made(&t_lever, (int)lw, (int)lh, p_lever, 0);
        ui_shadow_flat(cx + 0.1f * lw, py0 - (ext > 0 ? 0.2f : -0.2f) * h + 0.1f * lw, 0.5f * lw, 0.6f * h, 0.45f);
        ui_put_rot(lv, cx, py0, lw, h, ext >= 0 ? 0 : 180);
        break;
    }
    case DF_BUTTON: {
        int down = 0;
        const wave *v = cur_wave();
        if (v && v->held && v->origin == i && nwv && (nwv == 1 ? 1 : now_ms < WV[1].t0)) down = 1;
        if (v && v->held && v->origin == i && nwv == 1 && wave_t(v) > v->end + HOLD_MS) down = 0;
        float d = floorf(fminf(0.76f * cw, 1.1f * ph));
        ui_shadow_flat(cx + 0.05f * d, yt + 0.55f * d + 0.06f * d, d, d, 0.5f);
        ui_put(made(&t_button[down], (int)d, (int)d, p_button, down), cx - d / 2, yt, d, d);
        if (down) ui_animate();
        break;
    }
    case DF_BATTERY: {
        float w = floorf(fminf(0.6f * cw, 0.8f * ph)), h = tg_h;
        ui_shadow_flat(cx + 0.05f * w, yt + h / 2 + 0.06f * w, 0.8f * w, h, 0.5f);
        ui_put(made(&t_battery, (int)w, (int)h, p_battery, 0), cx - w / 2, yt, w, h);
        break;
    }
    case DF_LED: case DF_DET: {
        float gy = yb - 0.62f * lamp_h;
        static const uint8_t green[3] = {90, 255, 120}, red[3] = {255, 60, 40};
        int lit = out && powered_shown();
        float gd = 2.1f * lamp_w;
        if (lit && role == DF_DET && G.over == DF_LOST) {   /* about to blow: throbbing */
            gd *= 1.3f + 0.45f * sinf(now_ms * 0.045f);
            ui_animate();
        }
        if (lit) glow(cx, gy, gd, role == DF_DET ? 0.95f : 0.8f, role == DF_DET ? red : green);
        ui_put(led_tex(role, G.D.letter[i], lit), cx - lamp_w / 2, yb - lamp_h, lamp_w, lamp_h);
        break;
    }
    case DF_PLUG: {
        static const uint8_t amber[3] = {255, 170, 40};
        if (out) glow(cx, yb - 0.75f * lamp_h, 1.6f * lamp_w, 0.5f, amber);
        ui_put(made(&t_plug[out], (int)lamp_w, (int)lamp_h, p_plug, out), cx - lamp_w / 2, yb - lamp_h, lamp_w, lamp_h);
        break;
    }
    case DF_LATCH: {
        ui_put(made(&t_chip, (int)pw, (int)ph, p_chip, 0), cx - pw / 2, yt, pw, ph);
        leads(i, yb, yt + 0.8f * ph);
        float lw2 = floorf(0.9f * pw), lh2 = floorf(0.9f * ph);
        ui_put(made(&t_latch[out], (int)lw2, (int)lh2, p_latch, out), cx - lw2 / 2, yt + 0.05f * ph, lw2, lh2);
        break;
    }
    default: {
        ui_put(made(&t_chip, (int)pw, (int)ph, p_chip, 0), cx - pw / 2, yt, pw, ph);
        leads(i, yb, yt + 0.84f * gate_h);
        int w3 = N->nin == 3;
        ui_put(gate_tex(N->kind, out, w3), cx - gate_w[w3] / 2, yt, gate_w[w3], gate_h);
    }
    }
}

/* ---- the timer, the power lamp on the casing ----------------------------------------------------------------- */
static void draw_timer(void)
{
    int left = left_ms(), secs = (left + 999) / 1000;
    if (G.over == DF_WON) secs = left / 1000;
    int mm = secs / 60, ss = secs % 60;
    float x0 = BAND.x + shx, y0 = BAND.y + shy, h = BAND.h;
    float dw = dig_w, dh = dig_h, gap = 0.12f * dw, colon = 0.45f * dw;
    float total = 3 * dw + 2 * gap + colon, x = x0 + (BAND.w - total) / 2, cy = y0 + h / 2;
    static const uint8_t red[3] = {255, 46, 36}, green[3] = {70, 255, 110};
    int won = G.over == DF_WON, dead = blown();
    const uint8_t *c = won ? green : red;
    float a = 1;
    if (dead) a = (now_ms / 90) % 3 ? 0.25f : 0.9f;   /* blown: flickering */
    if (!won && !dead && G.over != DF_LOST && secs <= 10) a = 0.75f + 0.25f * sinf(now_ms * 0.012f);
    int digs[3] = {mm % 10, ss / 10, ss % 10};
    float xs[3] = {x, x + dw + gap + colon, x + 2 * dw + 2 * gap + colon};
    glow(x0 + BAND.w / 2, cy, 1.2f * BAND.w, 0.12f * a, c);
    for (int k = 0; k < 3; k++) {
        SDL_Texture *g = made(&t_digit[10], (int)dw, (int)dh, p_digit, 10), *d = made(&t_digit[digs[k]], (int)dw, (int)dh, p_digit, digs[k]);
        SDL_SetTextureColorMod(g, c[0], c[1], c[2]);
        SDL_SetTextureAlphaMod(g, 22);
        ui_put(g, xs[k], cy - dh / 2, dw, dh);
        SDL_SetTextureAlphaMod(g, 255);
        SDL_SetTextureColorMod(d, c[0], c[1], c[2]);
        SDL_SetTextureAlphaMod(d, (Uint8)(255 * a));
        ui_put(d, xs[k], cy - dh / 2, dw, dh);
        SDL_SetTextureAlphaMod(d, 255);
        SDL_SetTextureColorMod(d, 255, 255, 255);
    }
    int blink = !won && !dead && !clk.stop && (left % 1000) < 500;
    float dd = 0.16f * dw, cxp = x + dw + gap + colon / 2 - 0.05f * dw;
    for (int k = 0; k < 2; k++) {
        float yy = cy + (k ? 0.2f : -0.2f) * dh;
        ui_fill(cxp - dd / 2, yy - dd / 2, dd, dd, c[0], c[1], c[2], (Uint8)(blink ? 60 : 255 * a));
    }
    /* the power lamp, left of the window: amber while the bomb is fed */
    static const uint8_t amber[3] = {255, 170, 40};
    float px = lamp_x + shx, py = lamp_y + shy, pd = lamp_d;
    int fed = powered_shown() && !(won && G.D.plug >= 0 && !G.v.powered);
    if (fed) glow(px, py, 2.2f * pd, 0.6f, amber);
    ui_put_c(made(&t_power[fed], (int)pd, (int)pd, p_power, fed), px, py, pd, pd);
}

/* ---- the end: disarmed (green flashes), blown (flash, shake, fireball, smoke, scorch) ----------------------------- */
static void win_flash(void)
{
    if (G.over != DF_WON || !win_t || now_ms < win_t) return;
    float q = clampf((now_ms - win_t) / (float)WIN_MS, 0, 1);
    if (q < 1) ui_animate();
    float pulse = 0.5f + 0.5f * cosf(q * 3 * 6.2831853f);
    static const uint8_t green[3] = {90, 255, 130};
    for (int i = 0; i < G.D.L.nn; i++) {
        if (G.D.role[i] != DF_LED) continue;
        const lg_node *N = &G.D.L.n[i];
        float cx = PX(N->x), cy = PY(N->y) - 0.62f * lamp_h;
        glow(cx, cy, lamp_w * (2.4f + 1.4f * pulse), q < 1 ? 0.5f + 0.5f * pulse : 0.8f, green);
        if (q >= 1) continue;
        ui_put(led_tex(DF_LED, G.D.letter[i], 1), cx - lamp_w / 2, PY(N->y) - lamp_h, lamp_w, lamp_h);
    }
    if (q < 0.5f) {
        SDL_SetTextureColorMod(t_glow, 120, 255, 150);
        SDL_SetTextureAlphaMod(t_glow, (Uint8)(90 * (1 - 2 * q)));
        ui_put_c(t_glow, PB.x + PB.w / 2, PB.y + PB.h / 2, 1.6f * fmaxf(PB.w, PB.h) * (0.4f + q), 1.6f * fmaxf(PB.w, PB.h) * (0.4f + q));
        SDL_SetTextureAlphaMod(t_glow, 255);
        SDL_SetTextureColorMod(t_glow, 255, 255, 255);
    }
}

static void boom_place(float *x, float *y)
{
    if (boom_at >= 0) {
        const lg_node *N = &G.D.L.n[boom_at];
        *x = PX(N->x);
        *y = PY(N->y) - 0.62f * lamp_h;
    } else {
        *x = CS.x + shx + CS.w / 2;          /* the timer: the whole bomb */
        *y = CS.y + shy + 0.45f * CS.h;
    }
}

/* the shake's offset now (drawn everything by it) */
static void shake(void)
{
    shx = shy = 0;
    if (!blown()) return;
    float q = (now_ms - boom_t) / (float)BOOM_MS;
    if (q >= 0.45f) return;
    float a = ui.unit * 0.35f * (1 - q / 0.45f) * (1 - q / 0.45f);
    shx = a * sinf(now_ms * 0.09f) * cosf(now_ms * 0.031f);
    shy = a * cosf(now_ms * 0.083f);
}

static void puff(float x, float y, float d, float a, const uint8_t rgb[3])   /* a soft round cloud */
{
    SDL_SetTextureColorMod(t_puff, rgb[0], rgb[1], rgb[2]);
    SDL_SetTextureAlphaMod(t_puff, (Uint8)clampf(255 * a, 0, 255));
    ui_put_c(t_puff, x, y, d, d);
    SDL_SetTextureAlphaMod(t_puff, 255);
    SDL_SetTextureColorMod(t_puff, 255, 255, 255);
}

static void explosion(void)
{
    if (!blown()) return;
    float q = (now_ms - boom_t) / (float)BOOM_MS, x, y;
    boom_place(&x, &y);
    float R = fmaxf(PB.w, PB.h);
    static const uint8_t soot[3] = {0, 0, 0}, smoke[3] = {58, 54, 50}, smoke2[3] = {96, 90, 84};
    /* the board scorched, darkened: kept */
    ui_fill(PB.x + shx, PB.y + shy, PB.w, PB.h, 0, 0, 0, (Uint8)(130 * clampf(q * 4, 0, 1)));
    puff(x, y, 0.75f * R * clampf(0.3f + q * 3, 0, 1), 0.8f * clampf(q * 5, 0, 1), soot);
    if (q < 1) {
        ui_animate();
        if (q < 0.5f) {                         /* the fireball: white, yellow, orange, then red going brown */
            float f = q / 0.5f, e = puz_ease(f);
            uint8_t c[3] = {(uint8_t)(255 - 60 * f), (uint8_t)(220 - 170 * f), (uint8_t)(140 - 130 * f)};
            SDL_SetTextureBlendMode(t_puff, SDL_BLENDMODE_ADD);   /* (fire adds its light) */
            for (int k = 0; k < 7; k++) {
                float a = k * 2.39996f, r = k ? R * 0.16f * e : 0;
                puff(x + r * cosf(a), y + r * sinf(a) - R * 0.1f * e, R * (0.22f + 0.5f * e) * (k ? 0.7f : 1),
                     powf(1 - f, 0.7f), c);
            }
            uint8_t w[3] = {255, 250, 230};
            puff(x, y, R * (0.12f + 0.3f * e), (1 - f) * (1 - f), w);
            SDL_SetTextureBlendMode(t_puff, SDL_BLENDMODE_BLEND);
        }
        for (int k = 0; k < 12; k++) {          /* the smoke rising and spreading */
            float a = k * 2.39996f, t = clampf((q - 0.08f - 0.04f * (k % 4)) / 0.9f, 0, 1);
            float px = x + cosf(a) * R * 0.22f * (0.3f + t) + R * 0.05f * sinf(t * 5 + k);
            float py = y + sinf(a) * R * 0.12f * (0.3f + t) - R * 0.4f * t * t;
            puff(px, py, R * (0.16f + 0.42f * t), 0.85f * sinf(3.14159f * t), k & 1 ? smoke : smoke2);
        }
        for (int k = 0; k < 30; k++) {          /* the sparks and bits flying */
            float a = k * 6.2831853f / 30 + 0.3f * sinf(k * 3.7f), sp = 0.35f + 0.55f * (k * 29 % 11) / 11.0f;
            float r = R * sp * puz_ease(fminf(1, q * 2.2f)), px = x + r * cosf(a), py = y + r * sinf(a) + R * 0.5f * q * q;
            float s = fmaxf(3, ui.unit * 0.18f * (1 - q));
            SDL_SetTextureColorMod(t_dot, 255, (Uint8)(220 - 120 * q), (Uint8)(120 - 100 * q));
            SDL_SetTextureAlphaMod(t_dot, (Uint8)(255 * (1 - q)));
            ui_put_c(t_dot, px, py, s, s);
        }
        SDL_SetTextureColorMod(t_dot, 255, 255, 255);
        SDL_SetTextureAlphaMod(t_dot, 255);
        if (q < 0.12f) ui_fill(B.x, B.y, B.w, B.h, 255, 250, 235, (Uint8)(255 * (1 - q / 0.12f)));   /* the flash */
    } else {                                    /* a last wisp of smoke */
        float t = fmodf((now_ms - boom_t) / 2600.0f, 1);
        puff(x + R * 0.03f * sinf(t * 6), y - R * 0.3f * t, R * (0.12f + 0.2f * t), 0.4f * sinf(3.14159f * t), smoke2);
        ui_animate();
    }
}

static void hint_mark(void)
{
    if (!hint.on || G.over) return;
    Uint32 dt = now_ms - hint.t;
    if (dt < HINT_MS) ui_animate();
    if (dt < HINT_MS && dt / 300 % 2) return;
    float x, y, d = 1.3f * fminf(cw, ch);
    if (hint.m.kind == DF_MCUT) {
        cable_geo c = cable_at(&G.D.L.w[G.D.L.cut[hint.m.idx]]);
        cable_point(&c, 0.5f, &x, &y);
        d *= 0.8f;
    } else {
        float x0, y0, x1, y1;
        node_box(G.D.sw[hint.m.idx], &x0, &y0, &x1, &y1);
        x = (x0 + x1) / 2;
        y = (y0 + y1) / 2;
    }
    ui_mark_gold(x, y, d);
}

static float draw_label(float x, float y, int centred);

static void draw_tools(void)
{
    int blink = deny_reset && now_ms - deny_reset < HINT_MS;
    if (blink) ui_animate();
    int lost = G.over == DF_LOST && blown() && now_ms - boom_t > BOOM_MS / 2;
    if (lost) ui_animate();
    int lit = (blink && (now_ms - deny_reset) / 300 % 2 == 0) || (lost && now_ms / 450 % 2 == 0);
    ui_tools_shown(TOOLS);
    ui_tool(T_RESET, ICON_REPLAY, lit ? UI_LIT : lost ? UI_ON : 0);
    ui_tool(T_HINT, ICON_HINT, G.over ? UI_DIM : 0);
    if (wide && G.level >= 0) {               /* the level in the side column, under the header */
        box c = ui_bar_box(0, NULL);
        draw_label(c.x + c.w / 2, B.y + 1.6f * ui.unit, 1);
    }
}

static void draw(void)
{
    now_ms = ui_ms();
    if (made_shared < 0) layout(B);
    if (!G.D.L.nn) return;
    if (!G.over) {
        clock_frame();
        ui_animate();                           /* (the timer runs) */
        int sec = (left_ms() + 999) / 1000;
        if (sec != last_sec) {
            if (last_sec > 0 && sec <= 10 && sec > 0) ui_sound_soft(SND_TAP, 0.45f);
            last_sec = sec;
        }
        if (left_ms() <= 0) {                   /* time's up */
            df_blow(&G);
            clk.stop = 1;
            boom_at = -1;
            boom_t = now_ms;
            nwv = 0;
        }
    }
    if (G.over == DF_LOST && boom_t && now_ms >= boom_t && !boomed) {   /* (its sound as it blows, once) */
        boomed = 1;
        boom_sound();
    }
    shake();
    ui_put(t_casing, CS.x + shx, CS.y + shy, CS.w, CS.h);
    draw_timer();
    ui_put(t_pcb, PB.x + shx, PB.y + shy, PB.w, PB.h);
    draw_wires();
    for (int i = 0; i < G.D.L.nn; i++) draw_node(i);
    draw_cables();
    pulses();
    hint_mark();
    win_flash();
    explosion();
    draw_tools();
    if (animating()) ui_animate();
    shx = shy = 0;
}

/* ---- the pointer --------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    now_ms = ui_ms();
    int t = ui_tool_hit(x, y);
    if (t == T_RESET) {
        if (!G.nh && !G.over) { ui_sound(SND_DENY); return; }
        ui_sound(SND_TAP);
        restart();
        return;
    }
    if (t == T_HINT) {
        if (G.over) { ui_sound(SND_DENY); return; }
        if (df_hint(&G, &hint.m)) {
            hint.on = 1;
            hint.t = now_ms;
            G.hinted = 1;
            deny_reset = 0;
            ui_sound(SND_TAP);
        } else {
            deny_reset = now_ms | 1;
            ui_sound(SND_DENY);
        }
        ui_animate();
        return;
    }
    if (G.over == DF_LOST) {                    /* blown: a tap on the board, once it settled, starts again */
        if (blown() && now_ms - boom_t > BOOM_MS) {
            ui_sound(SND_TAP);
            restart();
        }
        return;
    }
    if (G.over) return;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < G.D.L.nn; i++) {
            int r = G.D.role[i];
            if (r != DF_SW && r != DF_BUTTON) continue;
            float x0, y0, x1, y1, g = (pass ? 0.14f : 0.02f) * cw;
            node_box(i, &x0, &y0, &x1, &y1);
            if (x < x0 - g || x > x1 + g || y < y0 - g || y > y1 + g) continue;
            int k = 0;
            while (G.D.sw[k] != i) k++;
            do_move((df_move){(uint8_t)(r == DF_BUTTON ? DF_MPRESS : DF_MFLIP), (uint8_t)k}, i, -1);
            return;
        }
        if (pass) break;
        float d;
        int best = cable_hit(x, y, &d);
        if (best >= 0 && d < 0.4f * fminf(cw, ch) + cable_t) {
            if (G.state & df_bit_cut(&G.D, best)) { ui_sound(SND_DENY); return; }
            do_move((df_move){DF_MCUT, (uint8_t)best}, -1, G.D.L.cut[best]);
            return;
        }
    }
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the bar: the level, the moves, the par ---------------------------------------------------------------------- */
/* the level: its chapter's picture, its number, from x (or centred on it); returns the x after it */
static float draw_label(float x, float y, int centred)
{
    float u = ui.unit;
    int n = G.level + 1, c = df_chapter(G.level);
    float dh = 0.5f * u, nw = puz_number_w(n, dh), ew = 0.62f * u, w = ew + 0.12f * u + nw;
    float x0 = centred ? x - w / 2 : x;
    if (t_emblem[c]) ui_put_c(t_emblem[c], x0 + ew / 2, y, ew, ew);
    puz_number(&font, x0 + ew + 0.12f * u + nw / 2, y, dh, n, TH.dark ? PUZ_CREAM : PUZ_INK, 255);
    return x0 + w;
}

static void bar(int player, box b)
{
    (void)player;
    float u = ui.unit, cy = b.y + b.h / 2;
    int sz = (int)(0.6f * u);
    if (made_bar != sz) {
        made_bar = sz;
        uint8_t *bf = calloc((size_t)sz * sz, 4);
        if (bf) {
            paint_toggle_pic(1, sz, sz, bf);
            ui_free(&t_bar_sw);
            t_bar_sw = ui_tex(sz, sz, bf);
            static const uint8_t gold[3] = {255, 200, 80};
            paint_star(sz, gold, bf);
            ui_free(&t_bar_star);
            t_bar_star = ui_tex(sz, sz, bf);
            free(bf);
        }
    }
    float x = b.x;
    if (G.level >= 0 && !wide) x = draw_label(x, cy, 0) + 0.5f * u;   /* the level (lying: in the side column) */
    ui_count c[2] = {{t_bar_sw, G.nh}, {t_bar_star, G.par}};
    float w = 0;
    for (int i = 0; i < 2; i++) w += 0.72f * u + ui_number_w(c[i].value, 0.46f * u) + (i ? 0.35f * u : 0);
    float room = b.x + b.w - x, k = w > room - 0.1f * u ? fmaxf(0.55f, (room - 0.1f * u) / w) : 1, s = 0.6f * u * k;
    for (int i = 0; i < 2; i++) {
        if (i) x += 0.35f * u * k;
        ui_put(c[i].pic, x, cy - s / 2, s, s);
        x += s + 0.12f * u * k;
        float nw = ui_number_w(c[i].value, 0.46f * u * k);
        ui_number(x + nw / 2, cy, 0.46f * u * k, c[i].value, 1);
        x += nw;
    }
}

static const game defuse_game = {
    .id = "defuse", .order = 371, .variants = DF_RANDOM,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = DF_LEVELS, .chapters = DF_CHAPTERS, .chapter_of = chapter_of, .chapter_icon = paint_emblem,
    .level_stars = level_stars, .start_random = start_random, .score = score, .par = par,
    .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(defuse_game)
