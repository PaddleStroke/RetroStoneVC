/*
 * The board games' pictures, drawn by code (art.h). A figure is a list of shapes in a unit square (y down), painted
 * back to front: each shape's fill (lit from the upper left) and its outline, which becomes an inner line where it
 * crosses a shape painted before it (the collars and crowns of the pieces); MARK shapes (eyes, slits) are painted in
 * the mark colour. Every pixel averages SS x SS samples (smooth edges at any size).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "art.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/chess.h"

#define SS 4
#define MAXPT 48
#define MAXSHAPES 32
#define PI_F 3.14159265f

typedef struct { float x, y; } v2;
enum { SH_POLY, SH_ELL };
typedef struct {
    int kind, role, n;
    v2 pt[MAXPT];
    float cx, cy, rx, ry;
    float x0, y0, x1, y1;                           /* the bounding box */
} shape;
struct art_fig { shape s[MAXSHAPES]; int n; };

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

art_fig *art_fig_new(void) { return calloc(1, sizeof(art_fig)); }
void art_fig_free(art_fig *f) { free(f); }

static shape *add(art_fig *f, int kind, int role)
{
    static shape spare;
    shape *s = f->n < MAXSHAPES ? &f->s[f->n++] : &spare;
    memset(s, 0, sizeof *s);
    s->kind = kind;
    s->role = role;
    s->x0 = s->y0 = 1e9f;
    s->x1 = s->y1 = -1e9f;
    return s;
}

static void grow(shape *s, float x, float y)
{
    s->x0 = fminf(s->x0, x);
    s->y0 = fminf(s->y0, y);
    s->x1 = fmaxf(s->x1, x);
    s->y1 = fmaxf(s->y1, y);
}

void art_poly(art_fig *f, int role, const float *xy, int n)
{
    shape *s = add(f, SH_POLY, role);
    s->n = n < MAXPT ? n : MAXPT;
    for (int i = 0; i < s->n; i++) {
        s->pt[i].x = xy[2 * i];
        s->pt[i].y = xy[2 * i + 1];
        grow(s, s->pt[i].x, s->pt[i].y);
    }
}

void art_ellipse(art_fig *f, int role, float cx, float cy, float rx, float ry)
{
    shape *s = add(f, SH_ELL, role);
    s->cx = cx;
    s->cy = cy;
    s->rx = rx;
    s->ry = ry;
    grow(s, cx - rx, cy - ry);
    grow(s, cx + rx, cy + ry);
}

void art_rect(art_fig *f, int role, float x0, float y0, float x1, float y1)
{
    const float p[8] = {x0, y0, x1, y0, x1, y1, x0, y1};
    art_poly(f, role, p, 4);
}

void art_arc(art_fig *f, int role, float cx, float cy, float r0, float r1, float a0, float a1)
{
    float p[2 * MAXPT];
    int n = 0, k = 21;
    for (int i = 0; i <= k; i++) {
        float a = (a0 + (a1 - a0) * i / k) * PI_F / 180;
        p[n++] = cx + r1 * cosf(a);
        p[n++] = cy - r1 * sinf(a);
    }
    for (int i = k; i >= 0; i--) {
        float a = (a0 + (a1 - a0) * i / k) * PI_F / 180;
        p[n++] = cx + r0 * cosf(a);
        p[n++] = cy - r0 * sinf(a);
    }
    art_poly(f, role, p, n / 2);
}

/* the signed distance to a shape's edge: negative inside (ellipses: a first-order estimate, close enough) */
static float sdf(const shape *s, float x, float y)
{
    if (s->kind == SH_ELL) {
        float u = (x - s->cx) / s->rx, v = (y - s->cy) / s->ry, l = sqrtf(u * u + v * v);
        if (l < 1e-4f) return -fminf(s->rx, s->ry);
        float g = sqrtf(u * u / (s->rx * s->rx) + v * v / (s->ry * s->ry)) / l;
        return (l - 1) / g;
    }
    int in = 0;
    float d2 = 1e9f;
    for (int i = 0, j = s->n - 1; i < s->n; j = i++) {
        v2 a = s->pt[j], b = s->pt[i];
        if ((b.y > y) != (a.y > y) && x < (a.x - b.x) * (y - b.y) / (a.y - b.y) + b.x) in = !in;
        float ex = a.x - b.x, ey = a.y - b.y, wx = x - b.x, wy = y - b.y;
        float t = clampf((wx * ex + wy * ey) / (ex * ex + ey * ey + 1e-12f), 0, 1);
        float dx = wx - ex * t, dy = wy - ey * t;
        d2 = fminf(d2, dx * dx + dy * dy);
    }
    return in ? -sqrtf(d2) : sqrtf(d2);
}

static void shade(const art_style *st, const shape *s, float x, float y, float *c)
{
    float cx = (s->x0 + s->x1) * 0.5f, hw = (s->x1 - s->x0) * 0.5f + 1e-3f;
    float t = (x - cx) / hw + 0.35f;                /* round shapes, lit from the upper left */
    float hh = fmaxf(1, s->y1 - s->y0);          /* a figure drawn in pixel units: the shading still spans it once */
    float l = 1.0f + st->light * (0.55f - t * t) - st->light * 0.45f * (y - s->y0) / hh;
    for (int i = 0; i < 3; i++) c[i] = clampf(st->fill[i] * l, 0, 255);
}

void art_render(const art_fig *f, const art_style *st, int w, int h, float scale, float ox, float oy, uint8_t *out)
{
    float wl = st->line;
    for (int py = 0; py < h; py++)
        for (int px = 0; px < w; px++) {
            float acc[4] = {0, 0, 0, 0};
            for (int sy = 0; sy < SS; sy++)
                for (int sx = 0; sx < SS; sx++) {
                    float x = (px - ox + (sx + 0.5f) / SS) / scale, y = (py - oy + (sy + 0.5f) / SS) / scale;
                    float c[3] = {0, 0, 0};
                    int has = 0, filled = 0;
                    for (int k = 0; k < f->n; k++) {
                        const shape *s = &f->s[k];
                        if (x < s->x0 - wl || x > s->x1 + wl || y < s->y0 - wl || y > s->y1 + wl) continue;
                        float d = sdf(s, x, y);
                        if (s->role == ART_MARK) {
                            if (d < 0) { memcpy(c, st->mark, sizeof c); has = 1; }
                        } else if (d < 0) {
                            shade(st, s, x, y, c);
                            has = filled = 1;
                        } else if (d < wl) {
                            memcpy(c, filled ? st->inner : st->outer, sizeof c);
                            has = 1;
                        }
                    }
                    if (has) {
                        acc[0] += c[0];
                        acc[1] += c[1];
                        acc[2] += c[2];
                        acc[3] += 1;
                    }
                }
            uint8_t *o = out + 4 * (py * w + px);
            if (acc[3] > 0) {
                for (int i = 0; i < 3; i++) o[i] = (uint8_t)(acc[i] / acc[3] + 0.5f);
                o[3] = (uint8_t)(255 * acc[3] / (SS * SS) + 0.5f);
            } else {
                memset(o, 0, 4);
            }
        }
}

void art_fig_blend(uint8_t *o, int pw, int ph, const art_fig *f, const art_style *st, int x, int y, int w, float a)
{
    if (w < 2) return;
    uint8_t *b = malloc((size_t)w * w * 4);
    if (!b) return;
    art_render(f, st, w, w, (float)w, 0, 0, b);
    if (a < 1)
        for (int i = 0; i < w * w; i++) b[4 * i + 3] = (uint8_t)(b[4 * i + 3] * a);
    art_blend(o, pw, ph, b, w, w, x, y);
    free(b);
}

void art_band(art_fig *f, int role, float x0, float y0, float x1, float y1, float wd)
{
    float dx = x1 - x0, dy = y1 - y0, l = hypotf(dx, dy) + 1e-6f, nx = -dy / l * wd / 2, ny = dx / l * wd / 2;
    const float q[] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    art_poly(f, role, q, 4);
}

void art_head(art_fig *f, int role, float x, float y, float deg, float l)
{
    float a = deg * PI_F / 180, c = cosf(a), s = sinf(a), bx = x - c * l * 0.45f, by = y - s * l * 0.45f;
    const float q[] = {x + c * l * 0.55f, y + s * l * 0.55f, bx - s * l * 0.5f, by + c * l * 0.5f, bx + s * l * 0.5f,
                       by - c * l * 0.5f};
    art_poly(f, role, q, 3);
}

void art_star(art_fig *f, int role, float cx, float cy, float r)
{
    float q[20];
    for (int i = 0; i < 10; i++) {                  /* the points and the hollows between them, from the top */
        float a = (-90 + 36 * i) * PI_F / 180, d = i & 1 ? 0.41f * r : r;
        q[2 * i] = cx + d * cosf(a);
        q[2 * i + 1] = cy + d * sinf(a);
    }
    art_poly(f, role, q, 10);
}

art_style art_flat(const uint8_t fill[3], const uint8_t ink[3], float line, float light)
{
    art_style st = {{0}, {0}, {0}, {0}, light, line};
    for (int k = 0; k < 3; k++) {
        st.fill[k] = fill[k];
        st.outer[k] = st.inner[k] = st.mark[k] = ink[k];
    }
    return st;
}

void art_fig_paint(uint8_t *o, int size, art_fig *f, const uint8_t fill[3], const uint8_t ink[3], float line,
                   float light)
{
    art_style st = art_flat(fill, ink ? ink : fill, line, light);
    art_fig_blend(o, size, size, f, &st, 0, 0, size, 1);
    art_fig_free(f);
}

/* ---- the chess pieces -------------------------------------------------------------------------------------------- */
static void base(art_fig *f)
{
    art_rect(f, ART_PART, 0.19f, 0.835f, 0.81f, 0.905f);
    const float p[] = {0.255f, 0.835f, 0.745f, 0.835f, 0.70f, 0.775f, 0.30f, 0.775f};
    art_poly(f, ART_PART, p, 4);
}

/* a bishop's mitre: half an ellipse below, an ogive up to a point */
static void mitre(art_fig *f, float cx, float cy, float rx, float ry, float tip)
{
    float p[2 * 40];
    int n = 0;
    for (int i = 0; i <= 14; i++) {
        float a = PI_F * i / 14;
        p[n++] = cx + rx * cosf(a);
        p[n++] = cy + ry * sinf(a);
    }
    for (int i = 1; i <= 9; i++) {
        float s = i / 9.0f;
        p[n++] = cx - rx * (1 - s * s);
        p[n++] = cy - (cy - tip) * s;
    }
    for (int i = 8; i >= 1; i--) {
        float s = i / 9.0f;
        p[n++] = cx + rx * (1 - s * s);
        p[n++] = cy - (cy - tip) * s;
    }
    art_poly(f, ART_PART, p, n / 2);
}

static void piece(art_fig *f, int type)
{
    f->n = 0;
    base(f);
    switch (type) {
    case PAWN: {
        const float body[] = {0.345f, 0.775f, 0.655f, 0.775f, 0.565f, 0.56f, 0.435f, 0.56f};
        art_poly(f, ART_PART, body, 4);
        art_ellipse(f, ART_PART, 0.5f, 0.555f, 0.155f, 0.04f);
        art_ellipse(f, ART_PART, 0.5f, 0.405f, 0.125f, 0.125f);
        break;
    }
    case ROOK: {
        const float body[] = {0.315f, 0.775f, 0.685f, 0.775f, 0.645f, 0.40f, 0.355f, 0.40f};
        const float top[] = {0.285f, 0.40f, 0.715f, 0.40f, 0.715f, 0.19f, 0.625f, 0.19f, 0.625f, 0.255f,
                             0.545f, 0.255f, 0.545f, 0.19f, 0.455f, 0.19f, 0.455f, 0.255f, 0.375f, 0.255f,
                             0.375f, 0.19f, 0.285f, 0.19f};
        art_poly(f, ART_PART, body, 4);
        art_poly(f, ART_PART, top, 12);
        art_rect(f, ART_PART, 0.33f, 0.40f, 0.67f, 0.445f);
        break;
    }
    case KNIGHT: {
        const float head[] = {0.30f, 0.775f, 0.72f, 0.775f, 0.72f, 0.62f, 0.745f, 0.46f, 0.705f, 0.32f,
                              0.625f, 0.21f, 0.535f, 0.165f, 0.50f, 0.07f, 0.45f, 0.15f, 0.38f, 0.18f,
                              0.29f, 0.28f, 0.21f, 0.40f, 0.18f, 0.48f, 0.21f, 0.545f, 0.29f, 0.555f,
                              0.37f, 0.51f, 0.45f, 0.50f, 0.40f, 0.60f, 0.33f, 0.68f};
        const float mane[] = {0.72f, 0.62f, 0.745f, 0.46f, 0.705f, 0.32f, 0.625f, 0.21f, 0.535f, 0.165f,
                              0.565f, 0.215f, 0.64f, 0.275f, 0.695f, 0.37f, 0.71f, 0.47f, 0.69f, 0.60f};
        art_poly(f, ART_PART, head, 19);
        art_poly(f, ART_PART, mane, 10);
        art_ellipse(f, ART_MARK, 0.405f, 0.295f, 0.026f, 0.026f);
        art_ellipse(f, ART_MARK, 0.235f, 0.465f, 0.016f, 0.02f);
        break;
    }
    case BISHOP: {
        const float body[] = {0.37f, 0.775f, 0.63f, 0.775f, 0.555f, 0.60f, 0.445f, 0.60f};
        const float slit[] = {0.535f, 0.32f, 0.565f, 0.345f, 0.495f, 0.44f, 0.465f, 0.415f};
        art_poly(f, ART_PART, body, 4);
        mitre(f, 0.5f, 0.45f, 0.15f, 0.135f, 0.20f);
        art_ellipse(f, ART_PART, 0.5f, 0.595f, 0.16f, 0.038f);
        art_ellipse(f, ART_PART, 0.5f, 0.185f, 0.042f, 0.042f);
        art_poly(f, ART_MARK, slit, 4);
        break;
    }
    case QUEEN: {
        const float body[] = {0.335f, 0.775f, 0.665f, 0.775f, 0.58f, 0.52f, 0.42f, 0.52f};
        const float crown[] = {0.335f, 0.49f, 0.205f, 0.245f, 0.335f, 0.355f, 0.365f, 0.175f, 0.44f, 0.335f,
                               0.50f, 0.15f, 0.56f, 0.335f, 0.635f, 0.175f, 0.665f, 0.355f, 0.795f, 0.245f,
                               0.665f, 0.49f};
        const float tips[5][2] = {{0.205f, 0.24f}, {0.365f, 0.17f}, {0.5f, 0.145f}, {0.635f, 0.17f},
                                  {0.795f, 0.24f}};
        art_poly(f, ART_PART, body, 4);
        art_poly(f, ART_PART, crown, 11);
        art_ellipse(f, ART_PART, 0.5f, 0.515f, 0.165f, 0.04f);
        for (int i = 0; i < 5; i++) art_ellipse(f, ART_PART, tips[i][0], tips[i][1], 0.04f, 0.04f);
        break;
    }
    case KING: {
        const float body[] = {0.335f, 0.775f, 0.665f, 0.775f, 0.59f, 0.52f, 0.41f, 0.52f};
        const float cross[] = {0.475f, 0.285f, 0.525f, 0.285f, 0.525f, 0.20f, 0.575f, 0.20f, 0.575f, 0.15f,
                               0.525f, 0.15f, 0.525f, 0.075f, 0.475f, 0.075f, 0.475f, 0.15f, 0.425f, 0.15f,
                               0.425f, 0.20f, 0.475f, 0.20f};
        const float crown[] = {0.33f, 0.49f, 0.67f, 0.49f, 0.73f, 0.335f, 0.67f, 0.275f, 0.585f, 0.315f,
                               0.50f, 0.265f, 0.415f, 0.315f, 0.33f, 0.275f, 0.27f, 0.335f};
        art_poly(f, ART_PART, body, 4);
        art_poly(f, ART_PART, cross, 12);
        art_poly(f, ART_PART, crown, 9);
        art_ellipse(f, ART_PART, 0.5f, 0.515f, 0.17f, 0.04f);
        break;
    }
    }
}

const art_style art_player[ART_PLAYERS] = {
    {{242, 234, 216}, {36, 30, 28}, {64, 54, 48}, {36, 30, 28}, 0.22f, 0},          /* ivory */
    {{70, 64, 62}, {10, 9, 9}, {150, 140, 132}, {205, 195, 185}, 0.35f, 0},         /* ebony */
    {{168, 52, 40}, {48, 14, 10}, {226, 140, 120}, {246, 222, 204}, 0.32f, 0},      /* red lacquer */
    {{62, 128, 98}, {14, 40, 28}, {150, 206, 176}, {226, 242, 228}, 0.32f, 0},      /* jade */
    {{222, 168, 66}, {66, 42, 10}, {120, 84, 30}, {62, 40, 12}, 0.26f, 0},          /* amber */
    {{52, 84, 150}, {10, 20, 46}, {150, 176, 226}, {222, 230, 248}, 0.32f, 0},      /* lapis */
};

static int player_index(int p) { return p < 0 ? 0 : p >= ART_PLAYERS ? ART_PLAYERS - 1 : p; }
int art_player_dark(int p) { return player_index(p) != 0 && player_index(p) != 4; }

void art_piece(int type, int color, int size, uint8_t *rgba)
{
    art_style st = art_player[player_index(color)];
    st.line = fmaxf(0.024f, 1.3f / size);
    art_fig *f = art_fig_new();
    piece(f, type);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

void art_piece_style(int type, const art_style *style, int size, uint8_t *rgba)
{
    art_style st = *style;
    st.line = fmaxf(st.line, 1.3f / size);
    art_fig *f = art_fig_new();
    piece(f, type);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

void art_counter(int kind, int player, int size, uint8_t *rgba)
{
    art_style st = art_player[player_index(player)];
    st.line = fmaxf(0.022f, 1.3f / size);
    art_fig *f = art_fig_new();
    if (kind == COUNTER_STONE) {                    /* glossy, a highlight towards the light */
        st.light = art_player_dark(player) ? 0.55f : 0.28f;
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.45f, 0.45f);
        static const float hi[2][3] = {{255, 255, 250}, {150, 144, 140}};
        if (player == 0 || player == 1) memcpy(st.mark, hi[player], sizeof st.mark);
        else for (int i = 0; i < 3; i++) st.mark[i] = fminf(255, st.fill[i] * 0.5f + 140);
        art_ellipse(f, ART_MARK, 0.36f, 0.33f, 0.09f, 0.06f);
    } else {
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.44f, 0.44f);
        float top = kind == COUNTER_DISC ? 0.32f : 0.34f;
        art_ellipse(f, ART_PART, 0.5f, 0.5f, top, top);
        if (kind != COUNTER_DISC) art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.21f, 0.21f);
        if (kind == COUNTER_KING) {                 /* a crown on the man */
            const float crown[] = {0.36f, 0.60f, 0.64f, 0.60f, 0.68f, 0.40f, 0.585f, 0.49f, 0.50f, 0.36f,
                                   0.415f, 0.49f, 0.32f, 0.40f};
            art_poly(f, ART_MARK, crown, 7);
        }
    }
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

void art_flat_shadow(int size, uint8_t *o)
{
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float d = hypotf(x + 0.5f - size / 2.0f, y + 0.5f - size / 2.0f) / size, a = (0.5f - d) / 0.12f;
            uint8_t *p = o + 4 * (y * size + x);
            p[0] = p[1] = p[2] = 0;
            p[3] = (uint8_t)(110 * clampf(a, 0, 1));
        }
}

void art_flat_put(uint8_t *o, int w, int h, const uint8_t *piece, int d, float cx, float cy)
{
    uint8_t *sh = d > 0 ? malloc((size_t)d * d * 4) : NULL;
    if (sh) {
        art_flat_shadow(d, sh);
        art_blend(o, w, h, sh, d, d, (int)(cx + 0.06f * d - d / 2.0f), (int)(cy + 0.1f * d - d / 2.0f));
        free(sh);
    }
    art_blend(o, w, h, piece, d, d, (int)(cx - d / 2.0f + 0.5f), (int)(cy - d / 2.0f + 0.5f));
}

void art_die(int value, int player, int size, uint8_t *rgba)
{
    static const float pip[7][6][2] = {         /* the pips of each face, in the die's unit square */
        {{0}}, {{0.5f, 0.5f}}, {{0.30f, 0.30f}, {0.70f, 0.70f}}, {{0.28f, 0.28f}, {0.5f, 0.5f}, {0.72f, 0.72f}},
        {{0.29f, 0.29f}, {0.71f, 0.29f}, {0.29f, 0.71f}, {0.71f, 0.71f}},
        {{0.28f, 0.28f}, {0.72f, 0.28f}, {0.5f, 0.5f}, {0.28f, 0.72f}, {0.72f, 0.72f}},
        {{0.29f, 0.26f}, {0.71f, 0.26f}, {0.29f, 0.5f}, {0.71f, 0.5f}, {0.29f, 0.74f}, {0.71f, 0.74f}}};
    art_style st = art_player[player_index(player)];
    st.line = fmaxf(0.025f, 1.3f / size);
    st.light = art_player_dark(player) ? 0.30f : 0.16f;
    art_fig *f = art_fig_new();
    float p[2 * 40], r = 0.17f, lo = 0.08f, hi = 0.92f;
    int n = 0;
    for (int c = 0; c < 4; c++) {               /* a rounded square: four quarter circles joined */
        float cx = c == 0 || c == 3 ? hi - r : lo + r, cy = c < 2 ? hi - r : lo + r;
        for (int i = 0; i <= 9; i++) {
            float a = (c * 90 + i * 10) * PI_F / 180;
            p[n++] = cx + r * cosf(a);
            p[n++] = cy + r * sinf(a);
        }
    }
    art_poly(f, ART_PART, p, n / 2);
    int v = value < 1 ? 1 : value > 6 ? 6 : value;
    for (int i = 0; i < v; i++) art_ellipse(f, ART_MARK, pip[v][i][0], pip[v][i][1], 0.075f, 0.075f);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

void art_die_blend(uint8_t *o, int w, int h, float cx, float cy, int size, int player, int value, float deg)
{
    if (size < 3) return;
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b) return;
    art_flat_shadow(size, b);
    art_blend_rot(o, w, h, b, size, size, cx + size / 14.0f, cy + size / 10.0f, 0);
    art_die(value, player, size, b);
    art_blend_rot(o, w, h, b, size, size, cx, cy, deg);
    free(b);
}

/* ---- the board --------------------------------------------------------------------------------------------------- */
static float hash(int a, int b)
{
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return (h & 0xffff) / 65535.0f;
}

void art_board_n(int nx, int ny, int sq, int fr, int checkered, uint8_t *o)
{
    static const float light[3] = {236, 212, 172}, dark[3] = {176, 124, 82}, plain[3] = {222, 184, 128},
                       frame[3] = {92, 60, 40};
    int w = nx * sq + 2 * fr, h = ny * sq + 2 * fr;
    float rad = fr * 0.7f;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            float c[3], a = 1;
            int bx = x - fr, by = y - fr;
            if (bx >= 0 && by >= 0 && bx < nx * sq && by < ny * sq) {
                float u = (float)bx / sq, v = (float)by / sq;
                if (checkered) {
                    int f = bx / sq, r = by / sq, lt = !((f + r) & 1);
                    float s1 = hash(f, r) * 6.28f, s2 = hash(r, f) * 6.28f;
                    /* the grain runs one way on light squares, the other on dark */
                    float g1 = lt ? v : u, g2 = lt ? u : v;
                    float g = sinf(g1 * 38 + 2.6f * sinf(g2 * 2.1f + s1) + s2) * 0.5f + 0.5f;
                    float k = 1 + 0.045f * (g - 0.5f) + 0.02f * (hash(f * 9 + r, 3) - 0.5f);
                    for (int i = 0; i < 3; i++) c[i] = (lt ? light[i] : dark[i]) * k;
                } else {                            /* one plank of wood, the grain across it */
                    float g = sinf(v * 23 + 2.4f * sinf(u * 0.9f + 1.3f) + 1.7f * sinf(u * 0.21f)) * 0.5f + 0.5f;
                    float k = 1 + 0.06f * (g - 0.5f);
                    for (int i = 0; i < 3; i++) c[i] = plain[i] * k;
                }
            } else {
                /* the frame: walnut, a groove next to the squares, a bevel lit from the top left, rounded corners */
                float dx = fminf(x, w - 1 - x), dy = fminf(y, h - 1 - y), edge = fminf(dx, dy);
                float g = sinf((x + y * 0.15f) * 0.09f + 1.7f * sinf(y * 0.013f)) * 0.5f + 0.5f;
                float k = 1 + 0.06f * (g - 0.5f);
                if (fr - edge < fr * 0.14f) k *= 0.62f;                   /* the groove */
                else if (edge < fr * 0.18f) k *= (x * h + y * w < w * h) ? 1.18f : 0.82f;
                for (int i = 0; i < 3; i++) c[i] = frame[i] * k;
                float cx = fmaxf(rad - x, x - (w - 1 - rad)), cy = fmaxf(rad - y, y - (h - 1 - rad));
                if (cx > 0 && cy > 0) a = clampf(rad - sqrtf(cx * cx + cy * cy) + 0.5f, 0, 1);
            }
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(c[i], 0, 255);
            p[3] = (uint8_t)(a * 255);
        }
}

void art_board(int sq, int fr, uint8_t *o) { art_board_n(8, 8, sq, fr, 1, o); }

float art_noise(int x, int y)
{
    uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (h & 255) / 255.0f - 0.5f;
}

float art_recess(float dx, float dy, float r, float b, float deep, float wall_lit, float rim_lit)
{
    float d = hypotf(dx, dy);
    if (d >= r + b) return 1;
    float lit = (dx + dy) / (1.4142f * d + 1e-3f);  /* 1 towards the lower right: the far wall, in the light */
    float wall = deep + wall_lit * lit * fminf(d / r, 1), rim = 1 + rim_lit * lit * clampf(1 - (d - r) / b, 0, 1);
    float in = clampf(r - d + 0.5f, 0, 1);
    return wall * in + rim * (1 - in);
}

void art_felt(uint8_t *o, int w, int h, int x0, int y0, int x1, int y1)
{
    static const float green[3] = {40, 116, 72};
    float soft = 0.018f * (w > h ? w : h);
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < h; y++)
        for (int x = x0 < 0 ? 0 : x0; x < x1 && x < w; x++) {
            float e = fminf(fminf((float)(x - x0), (float)(x1 - 1 - x)), fminf((float)(y - y0), (float)(y1 - 1 - y)));
            float k = (1 - 0.5f * expf(-e / soft)) * (1 + 0.07f * art_noise(x, y) + 0.03f * art_noise(x / 3, y / 2));
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)fminf(255, green[c] * k);
            p[3] = 255;
        }
}

/* ---- the buttons' icons ------------------------------------------------------------------------------------------ */
void art_icon(int which, int size, uint8_t *rgba)
{
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    switch (which) {
    case ICON_UNDO: {                               /* an arrow curling back to the left */
        art_arc(f, ART_PART, 0.53f, 0.54f, 0.17f, 0.25f, -40, 180);
        const float head[] = {0.19f, 0.50f, 0.41f, 0.50f, 0.30f, 0.68f};
        art_poly(f, ART_PART, head, 3);
        break;
    }
    case ICON_FLIP: {                               /* the board turns round: up and down */
        const float up[] = {0.22f, 0.40f, 0.50f, 0.40f, 0.36f, 0.20f},
                    down[] = {0.50f, 0.60f, 0.78f, 0.60f, 0.64f, 0.80f};
        art_rect(f, ART_PART, 0.32f, 0.36f, 0.40f, 0.76f);
        art_rect(f, ART_PART, 0.60f, 0.24f, 0.68f, 0.64f);
        art_poly(f, ART_PART, up, 3);
        art_poly(f, ART_PART, down, 3);
        break;
    }
    case ICON_NEW: {                                /* a new board: four squares, two of them filled */
        const float frame[] = {0.20f, 0.20f, 0.80f, 0.20f, 0.80f, 0.80f, 0.20f, 0.80f, 0.20f, 0.20f,
                               0.25f, 0.25f, 0.25f, 0.75f, 0.75f, 0.75f, 0.75f, 0.25f, 0.25f, 0.25f};
        art_rect(f, ART_PART, 0.24f, 0.24f, 0.50f, 0.50f);
        art_rect(f, ART_PART, 0.50f, 0.50f, 0.76f, 0.76f);
        art_poly(f, ART_PART, frame, 10);
        break;
    }
    case ICON_HOME: {                               /* a house, its door */
        const float house[] = {0.20f, 0.52f, 0.50f, 0.22f, 0.80f, 0.52f, 0.71f, 0.52f, 0.71f, 0.79f, 0.57f, 0.79f,
                               0.57f, 0.60f, 0.43f, 0.60f, 0.43f, 0.79f, 0.29f, 0.79f, 0.29f, 0.52f};
        art_poly(f, ART_PART, house, 11);
        break;
    }
    case ICON_F2F:                                  /* two players face to face across the table */
        art_ellipse(f, ART_PART, 0.5f, 0.70f, 0.085f, 0.085f);
        art_arc(f, ART_PART, 0.5f, 0.98f, 0.0f, 0.19f, 0, 180);
        art_ellipse(f, ART_PART, 0.5f, 0.30f, 0.085f, 0.085f);
        art_arc(f, ART_PART, 0.5f, 0.02f, 0.0f, 0.19f, 180, 360);
        art_rect(f, ART_PART, 0.16f, 0.485f, 0.84f, 0.515f);
        break;
    case ICON_PASS: {                               /* an arrow jumping over a stone */
        art_arc(f, ART_PART, 0.47f, 0.60f, 0.20f, 0.27f, 0, 180);
        const float head[] = {0.62f, 0.56f, 0.86f, 0.56f, 0.74f, 0.74f};
        art_poly(f, ART_PART, head, 3);
        art_ellipse(f, ART_PART, 0.47f, 0.66f, 0.09f, 0.09f);
        break;
    }
    case ICON_HELP: {                               /* a question mark */
        art_arc(f, ART_PART, 0.5f, 0.37f, 0.11f, 0.195f, -75, 185);
        const float stem[] = {0.535f, 0.47f, 0.585f, 0.535f, 0.545f, 0.56f,
                              0.545f, 0.65f, 0.455f, 0.65f, 0.455f, 0.53f};
        art_poly(f, ART_PART, stem, 6);
        art_ellipse(f, ART_PART, 0.5f, 0.775f, 0.062f, 0.062f);
        break;
    }
    case ICON_CLOSE: {                              /* a cross */
        const float a[] = {0.25f, 0.31f, 0.31f, 0.25f, 0.75f, 0.69f, 0.69f, 0.75f};
        const float b[] = {0.69f, 0.25f, 0.75f, 0.31f, 0.31f, 0.75f, 0.25f, 0.69f};
        art_poly(f, ART_PART, a, 4);
        art_poly(f, ART_PART, b, 4);
        break;
    }
    case ICON_DEVICE: {                             /* pass the device: a device, two arrows going round it */
        const float dev[] = {0.38f, 0.30f, 0.62f, 0.30f, 0.62f, 0.70f, 0.38f, 0.70f, 0.38f, 0.30f,
                             0.42f, 0.35f, 0.42f, 0.62f, 0.58f, 0.62f, 0.58f, 0.35f, 0.42f, 0.35f};
        art_poly(f, ART_PART, dev, 10);
        art_ellipse(f, ART_PART, 0.5f, 0.66f, 0.022f, 0.022f);
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.33f, 0.40f, 35, 150);
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.33f, 0.40f, 215, 330);
        for (int k = 0; k < 2; k++) {               /* each head at its arc's end, along the circle */
            float a0 = (k ? 330 : 150) * PI_F / 180, a1 = a0 + 22 * PI_F / 180, rm = 0.365f, hw = 0.09f;
            const float hd[] = {0.5f + (rm + hw) * cosf(a0), 0.5f - (rm + hw) * sinf(a0),
                                0.5f + rm * cosf(a1), 0.5f - rm * sinf(a1),
                                0.5f + (rm - hw) * cosf(a0), 0.5f - (rm - hw) * sinf(a0)};
            art_poly(f, ART_PART, hd, 3);
        }
        break;
    }
    case ICON_PLAY: {                               /* start: a triangle pointing right */
        const float tri[] = {0.36f, 0.22f, 0.80f, 0.50f, 0.36f, 0.78f};
        art_poly(f, ART_PART, tri, 3);
        break;
    }
    case ICON_HINT: {                               /* a lamp, lit: its bulb, its neck, its screw, its rays */
        art_ellipse(f, ART_PART, 0.5f, 0.42f, 0.19f, 0.19f);
        const float neck[] = {0.38f, 0.54f, 0.62f, 0.54f, 0.585f, 0.68f, 0.415f, 0.68f};
        art_poly(f, ART_PART, neck, 4);
        art_rect(f, ART_PART, 0.415f, 0.70f, 0.585f, 0.74f);
        art_rect(f, ART_PART, 0.44f, 0.76f, 0.56f, 0.80f);
        for (int i = 0; i < 5; i++) {
            float a = (90 + (i - 2) * 40) * PI_F / 180, c = cosf(a), s = -sinf(a);
            art_band(f, ART_PART, 0.5f + c * 0.26f, 0.42f + s * 0.26f, 0.5f + c * 0.34f, 0.42f + s * 0.34f, 0.045f);
        }
        break;
    }
    case ICON_TICK: {                               /* a tick */
        const float tick[] = {0.17f, 0.53f, 0.29f, 0.41f, 0.42f, 0.55f, 0.72f, 0.22f, 0.84f, 0.34f, 0.42f, 0.78f};
        art_poly(f, ART_PART, tick, 6);
        break;
    }
    case ICON_PUZZLE:                               /* a jigsaw piece: a square, a knob on its top and its right */
        art_rect(f, ART_PART, 0.18f, 0.34f, 0.66f, 0.82f);
        art_ellipse(f, ART_PART, 0.42f, 0.26f, 0.11f, 0.1f);
        art_ellipse(f, ART_PART, 0.74f, 0.58f, 0.1f, 0.11f);
        break;
    case ICON_GRID:                                 /* the levels: nine tiles, three by three */
        for (int k = 0; k < 9; k++) {
            float x = 0.19f + (k % 3) * 0.225f, y = 0.19f + (k / 3) * 0.225f;
            art_rect(f, ART_PART, x, y, x + 0.17f, y + 0.17f);
        }
        break;
    case ICON_STAR:                                 /* a star */
        art_star(f, ART_PART, 0.5f, 0.53f, 0.36f);
        break;
    case ICON_ARROW: {                              /* an arrow pointing right */
        const float head[] = {0.50f, 0.22f, 0.86f, 0.50f, 0.50f, 0.78f};
        art_rect(f, ART_PART, 0.14f, 0.42f, 0.56f, 0.58f);
        art_poly(f, ART_PART, head, 3);
        break;
    }
    case ICON_SOUND:
    case ICON_MUTE: {                               /* a loudspeaker, its waves (or a cross) */
        const float spk[] = {0.18f, 0.40f, 0.32f, 0.40f, 0.50f, 0.23f, 0.50f, 0.77f, 0.32f, 0.60f, 0.18f, 0.60f};
        art_poly(f, ART_PART, spk, 6);
        if (which == ICON_SOUND) {
            art_arc(f, ART_PART, 0.50f, 0.50f, 0.11f, 0.17f, -50, 50);
            art_arc(f, ART_PART, 0.50f, 0.50f, 0.24f, 0.30f, -50, 50);
        } else {
            const float a[] = {0.59f, 0.36f, 0.63f, 0.32f, 0.85f, 0.64f, 0.81f, 0.68f};
            const float b[] = {0.81f, 0.32f, 0.85f, 0.36f, 0.63f, 0.68f, 0.59f, 0.64f};
            art_poly(f, ART_PART, a, 4);
            art_poly(f, ART_PART, b, 4);
        }
        break;
    }
    }
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

/* ---- plain shapes ------------------------------------------------------------------------------------------------ */
void art_roundrect(int w, int h, float rad, const uint8_t fill[4], uint8_t *o)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float qx = fabsf(x + 0.5f - w * 0.5f) - (w * 0.5f - rad);
            float qy = fabsf(y + 0.5f - h * 0.5f) - (h * 0.5f - rad);
            float ox = fmaxf(qx, 0), oy = fmaxf(qy, 0);
            float d = sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0) - rad;
            uint8_t *p = o + 4 * (y * w + x);
            memcpy(p, fill, 3);
            p[3] = (uint8_t)(fill[3] * clampf(0.5f - d, 0, 1));
        }
}

void art_disc(int size, const uint8_t rgb[3], float a0, float power, uint8_t *o)
{
    float r = size * 0.5f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = x + 0.5f - r, dy = y + 0.5f - r, d = sqrtf(dx * dx + dy * dy);
            float a = power > 0 ? a0 * powf(clampf(1 - d / r, 0, 1), power) : a0 * clampf(r - d + 0.5f, 0, 1);
            uint8_t *p = o + 4 * (y * size + x);
            memcpy(p, rgb, 3);
            p[3] = (uint8_t)(255 * a);
        }
}

void art_ring(int size, float r_in, const uint8_t rgb[3], float a, uint8_t *o)
{
    float r = size * 0.5f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = x + 0.5f - r, dy = y + 0.5f - r, d = sqrtf(dx * dx + dy * dy);
            float cov = clampf(r - d + 0.5f, 0, 1) * clampf(d - r * r_in + 0.5f, 0, 1);
            uint8_t *p = o + 4 * (y * size + x);
            memcpy(p, rgb, 3);
            p[3] = (uint8_t)(255 * a * cov);
        }
}

void art_blend(uint8_t *dst, int dw, int dh, const uint8_t *src, int sw, int sh, int x, int y)
{
    for (int j = 0; j < sh; j++) {
        if (y + j < 0 || y + j >= dh) continue;
        for (int i = 0; i < sw; i++) {
            if (x + i < 0 || x + i >= dw) continue;
            const uint8_t *s = src + 4 * (j * sw + i);
            uint8_t *d = dst + 4 * ((y + j) * dw + x + i);
            float sa = s[3] / 255.0f, da = d[3] / 255.0f, oa = sa + da * (1 - sa);
            if (oa <= 0) continue;
            for (int k = 0; k < 3; k++) d[k] = (uint8_t)((s[k] * sa + d[k] * da * (1 - sa)) / oa + 0.5f);
            d[3] = (uint8_t)(oa * 255 + 0.5f);
        }
    }
}

void art_blend_rot(uint8_t *o, int w, int h, const uint8_t *src, int sw, int sh, float cx, float cy, float deg)
{
    float a = deg * PI_F / 180, c = cosf(a), s = sinf(a), r = 0.5f * hypotf((float)sw, (float)sh) + 1;
    for (int y = (int)(cy - r); y <= (int)(cy + r); y++)
        for (int x = (int)(cx - r); x <= (int)(cx + r); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float u = c * dx + s * dy + sw / 2.0f - 0.5f, v = -s * dx + c * dy + sh / 2.0f - 0.5f;
            if (u < -1 || v < -1 || u > sw || v > sh) continue;
            int u0 = (int)floorf(u), v0 = (int)floorf(v);
            float fu = u - u0, fv = v - v0, px[4] = {0, 0, 0, 0};
            for (int k = 0; k < 4; k++) {
                int su = u0 + (k & 1), sv = v0 + (k >> 1);
                if (su < 0 || sv < 0 || su >= sw || sv >= sh) continue;
                float wk = (k & 1 ? fu : 1 - fu) * (k >> 1 ? fv : 1 - fv);
                const uint8_t *q = src + 4 * ((size_t)sv * sw + su);
                float al = q[3] / 255.0f * wk;
                for (int ch = 0; ch < 3; ch++) px[ch] += q[ch] * al;
                px[3] += al;
            }
            if (px[3] <= 0.001f) continue;
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            float al = clampf(px[3], 0, 1), da = p[3] / 255.0f, oa = al + da * (1 - al);
            for (int ch = 0; ch < 3; ch++) {
                float sc = px[ch] / px[3];
                p[ch] = (uint8_t)clampf((sc * al + p[ch] * da * (1 - al)) / (oa > 0 ? oa : 1), 0, 255);
            }
            p[3] = (uint8_t)clampf(oa * 255, 0, 255);
        }
}
