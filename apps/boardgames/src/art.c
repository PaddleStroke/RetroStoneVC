/*
 * The board games' pictures, drawn by code (art.h). A figure is a list of shapes in a unit square (y down), painted
 * back to front: each shape's fill (lit from the upper left) and its outline, which becomes an inner line where it
 * crosses a shape painted before it (the collars and crowns of the pieces); MARK shapes (eyes, slits) are painted in
 * the mark colour. Every pixel averages SS x SS samples (smooth edges at any size).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "art.h"

#include <SDL.h>
#include <stdatomic.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/chess.h"

#define SS 4
#define ARC_PT 22                                   /* an arc's points along each edge */
#define PI_F 3.14159265f

typedef struct { float x, y; } v2;
enum { SH_POLY, SH_ELL };
typedef struct {
    int kind, role, n, p0;                          /* a polygon: its n points from the figure's pt[p0] */
    float cx, cy, rx, ry;
    float x0, y0, x1, y1;                           /* the bounding box */
} shape;
/* the shapes and the polygons' points, as many as a figure has (both grow as it is drawn) */
struct art_fig { shape *s; v2 *pt; int n, cap, npt, ptcap; };

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

art_fig *art_fig_new(void) { return calloc(1, sizeof(art_fig)); }
void art_fig_free(art_fig *f)
{
    if (!f) return;
    free(f->s);
    free(f->pt);
    free(f);
}

/* room for one more shape and n more points: 0 when memory ran out */
static int room(art_fig *f, int n)
{
    if (f->n == f->cap) {
        int cap = f->cap ? 2 * f->cap : 16;
        shape *s = realloc(f->s, (size_t)cap * sizeof *s);
        if (!s) return 0;
        f->s = s;
        f->cap = cap;
    }
    if (f->npt + n > f->ptcap) {
        int cap = f->ptcap ? 2 * f->ptcap : 256;
        while (cap < f->npt + n) cap *= 2;
        v2 *p = realloc(f->pt, (size_t)cap * sizeof *p);
        if (!p) return 0;
        f->pt = p;
        f->ptcap = cap;
    }
    return 1;
}

/* a new shape (NULL: no memory left, the shape is left out) */
static shape *add(art_fig *f, int kind, int role, int npt)
{
    if (!f || !room(f, npt)) return NULL;
    shape *s = &f->s[f->n++];
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
    if (n < 0) n = 0;
    shape *s = add(f, SH_POLY, role, n);
    if (!s) return;
    s->n = n;
    s->p0 = f->npt;
    v2 *pt = f->pt + s->p0;
    f->npt += n;
    for (int i = 0; i < n; i++) {
        pt[i].x = xy[2 * i];
        pt[i].y = xy[2 * i + 1];
        grow(s, pt[i].x, pt[i].y);
    }
}

void art_ellipse(art_fig *f, int role, float cx, float cy, float rx, float ry)
{
    shape *s = add(f, SH_ELL, role, 0);
    if (!s) return;
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
    float p[4 * ARC_PT];
    int n = 0, k = ARC_PT - 1;
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
static float sdf(const art_fig *f, const shape *s, float x, float y)
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
        v2 a = f->pt[s->p0 + j], b = f->pt[s->p0 + i];
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

/* the first painter, every sample of every pixel through every shape: the reference the fast one is checked against
 * (RSB_ART_REF=1 in the environment paints with it) */
static void render_ref(const art_fig *f, const art_style *st, int w, int h, float scale, float ox, float oy,
                       uint8_t *out)
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
                        float d = sdf(f, s, x, y);
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

/* ---- the fast painter: the same pixels, the work only where an edge is ---------------------------------------------
 * A shape is the same over a whole region of samples (a block of pixels, a pixel) when the region lies wholly inside
 * it, wholly in its outline's band, or wholly outside: its distance at the region's centre, against the region's
 * radius, says so. Only the shapes whose edge crosses a pixel are looked at sample by sample; a pixel no edge crosses
 * takes its colour from the shape on top (a fill's light still computed at each sample, as the reference does). */
enum { S_OUT, S_IN, S_LINE, S_MIX };
#define BLK 8                                       /* a block: BLK x BLK pixels */

/* the state of shape s over a region: its samples all within R (unit square) of (x, y); wl the outline's width */
static int region_state(const art_fig *f, const shape *s, float x, float y, float R, float wl)
{
    if (x + R < s->x0 - wl || x - R > s->x1 + wl || y + R < s->y0 - wl || y - R > s->y1 + wl) return S_OUT;
    int inbox = x - R >= s->x0 - wl && x + R <= s->x1 + wl && y - R >= s->y0 - wl && y + R <= s->y1 + wl;
    int mark = s->role == ART_MARK, band = !mark && wl > 0;
    if (s->kind == SH_ELL) {
        /* the reference's distance is an estimate: inside, the radius l (1 on the edge) moves at most 1 / min(rx, ry)
         * a unit; outside, the estimate never exceeds the true distance (l is convex). Its outline's band: a margin. */
        float u = (x - s->cx) / s->rx, v = (y - s->cy) / s->ry, l = sqrtf(u * u + v * v), m = fminf(s->rx, s->ry);
        if (l + R / m < 1) return S_IN;
        if (l < 1) return S_MIX;
        float d = sdf(f, s, x, y);
        if (d <= R) return S_MIX;
        if (!band || d > wl + 3 * R) return S_OUT;
        return d < wl - 3 * R && inbox ? S_LINE : S_MIX;
    }
    float d = sdf(f, s, x, y);                      /* a polygon's: the true distance */
    if (d < -R) return S_IN;
    if (d <= R) return S_MIX;
    if (!band || d > wl + R) return S_OUT;
    return d < wl - R && inbox ? S_LINE : S_MIX;
}

/* a sample's state in shape s (as sdf() would say it, d < 0 in, d < wl its outline; wl 0: no outline), sooner: the
 * polygon's inside first, its distance only outside and only as far as the outline */
static int point_state(const art_fig *f, const shape *s, float x, float y, float wl)
{
    if (s->kind == SH_ELL) {
        float d = sdf(f, s, x, y);
        return d < 0 ? S_IN : d < wl ? S_LINE : S_OUT;
    }
    int in = 0;
    const v2 *p = f->pt + s->p0;
    for (int i = 0, j = s->n - 1; i < s->n; j = i++) {
        v2 a = p[j], b = p[i];
        if ((b.y > y) != (a.y > y) && x < (a.x - b.x) * (y - b.y) / (a.y - b.y) + b.x) in = !in;
    }
    if (in) {                                       /* on an edge exactly: sdf's -0, not inside */
        for (int i = 0, j = s->n - 1; i < s->n; j = i++) {
            v2 a = p[j], b = p[i];
            if (x < fminf(a.x, b.x) || x > fmaxf(a.x, b.x) || y < fminf(a.y, b.y) || y > fmaxf(a.y, b.y)) continue;
            float ex = a.x - b.x, ey = a.y - b.y, wx = x - b.x, wy = y - b.y;
            float t = clampf((wx * ex + wy * ey) / (ex * ex + ey * ey + 1e-12f), 0, 1);
            float dx = wx - ex * t, dy = wy - ey * t;
            if (dx * dx + dy * dy == 0) return 0 < wl ? S_LINE : S_OUT;
        }
        return S_IN;
    }
    if (wl <= 0) return S_OUT;
    float w2 = wl * wl * 1.01f;                     /* a first look without the root; the root decides, as sdf's */
    for (int i = 0, j = s->n - 1; i < s->n; j = i++) {
        v2 a = p[j], b = p[i];
        float ex = a.x - b.x, ey = a.y - b.y, wx = x - b.x, wy = y - b.y;
        float t = clampf((wx * ex + wy * ey) / (ex * ex + ey * ey + 1e-12f), 0, 1);
        float dx = wx - ex * t, dy = wy - ey * t, d2 = dx * dx + dy * dy;
        if (d2 < w2 && sqrtf(d2) < wl) return S_LINE;
    }
    return S_OUT;
}

/* one sample through the shapes listed (idx), their states given (S_MIX: looked at here): the colour on top (c), 0:
 * none; *top_fill: the shape whose fill is on top (-1: an outline or a mark) */
static int sample(const art_fig *f, const art_style *st, const int *idx, const uint8_t *state, int n, float x, float y,
                  float *c, int *top_fill)
{
    float wl = st->line;
    int has = 0, filled = 0, top = -1;              /* top: the fill on top (its light computed once, at the end) */
    for (int i = 0; i < n; i++) {
        const shape *s = &f->s[idx[i]];
        int q = state[i];
        if (q == S_MIX) {
            if (x < s->x0 - wl || x > s->x1 + wl || y < s->y0 - wl || y > s->y1 + wl) continue;
            q = point_state(f, s, x, y, s->role == ART_MARK ? 0 : wl);
        }
        if (q == S_OUT) continue;
        if (s->role == ART_MARK) {
            if (q == S_IN) { memcpy(c, st->mark, 3 * sizeof *c); has = 1; top = -1; }
        } else if (q == S_IN) {
            top = idx[i];
            has = filled = 1;
        } else {
            memcpy(c, filled ? st->inner : st->outer, 3 * sizeof *c);
            has = 1;
            top = -1;
        }
    }
    if (top >= 0) shade(st, &f->s[top], x, y, c);
    if (top_fill) *top_fill = top;
    return has;
}

/* one sample's colour, every shape's state known (q[i * SS * SS]: shape idx[i]'s): as sample() */
static int sample_q(const art_fig *f, const art_style *st, const int *idx, const uint8_t *q, int n, float x, float y,
                    float *c)
{
    int has = 0, filled = 0, top = -1;
    for (int i = 0; i < n; i++) {
        int s = q[i * SS * SS];
        if (s == S_OUT) continue;
        if (f->s[idx[i]].role == ART_MARK) {
            if (s == S_IN) { memcpy(c, st->mark, 3 * sizeof *c); has = 1; top = -1; }
        } else if (s == S_IN) {
            top = idx[i];
            has = filled = 1;
        } else {
            memcpy(c, filled ? st->inner : st->outer, 3 * sizeof *c);
            has = 1;
            top = -1;
        }
    }
    if (top >= 0) shade(st, &f->s[top], x, y, c);
    return has;
}

/* a pixel's SS x SS samples (columns xs, rows ys) in shape s: q[sy * SS + sx], as point_state() says each. A polygon:
 * a row's crossings once for its SS samples, its edges near the row only for the outline. xc, ec: room for s->n. */
static void pixel_states(const art_fig *f, const shape *s, const float *xs, const float *ys, float wl, uint8_t *q,
                         float *xc, int *ec)
{
    if (s->kind == SH_ELL) {
        for (int k = 0; k < SS * SS; k++) {
            float x = xs[k % SS], y = ys[k / SS];
            if (x < s->x0 - wl || x > s->x1 + wl || y < s->y0 - wl || y > s->y1 + wl) q[k] = S_OUT;
            else q[k] = (uint8_t)point_state(f, s, x, y, s->role == ART_MARK ? 0 : wl);
        }
        return;
    }
    float bwl = s->role == ART_MARK ? 0 : wl, w2 = bwl * bwl * 1.01f;   /* the box's margin is wl, a mark's too */
    const v2 *p = f->pt + s->p0;
    for (int sy = 0; sy < SS; sy++) {
        float y = ys[sy];
        int nc = 0, ne = 0;
        for (int i = 0, j = s->n - 1; i < s->n; j = i++) {
            v2 a = p[j], b = p[i];
            if ((b.y > y) != (a.y > y)) xc[nc++] = (a.x - b.x) * (y - b.y) / (a.y - b.y) + b.x;
            if (fminf(a.y, b.y) - bwl <= y && y <= fmaxf(a.y, b.y) + bwl) ec[ne++] = i;   /* near the row */
        }
        for (int sx = 0; sx < SS; sx++) {
            float x = xs[sx];
            uint8_t *o = q + sy * SS + sx;
            if (x < s->x0 - wl || x > s->x1 + wl || y < s->y0 - wl || y > s->y1 + wl) { *o = S_OUT; continue; }
            int in = 0;
            for (int k = 0; k < nc; k++) in ^= x < xc[k];
            *o = in ? S_IN : S_OUT;
            for (int k = 0; k < ne && (in || bwl > 0); k++) {
                int i = ec[k], j = i ? i - 1 : s->n - 1;
                v2 a = p[j], b = p[i];
                if (in && (x < fminf(a.x, b.x) || x > fmaxf(a.x, b.x) || y < fminf(a.y, b.y) || y > fmaxf(a.y, b.y)))
                    continue;                       /* inside: only an edge it lies on matters (sdf's -0) */
                if (!in && (x < fminf(a.x, b.x) - bwl || x > fmaxf(a.x, b.x) + bwl)) continue;
                float ex = a.x - b.x, ey = a.y - b.y, wx = x - b.x, wy = y - b.y;
                float t = clampf((wx * ex + wy * ey) / (ex * ex + ey * ey + 1e-12f), 0, 1);
                float dx = wx - ex * t, dy = wy - ey * t, d2 = dx * dx + dy * dy;
                if (in) {
                    if (d2 == 0) {
                        *o = 0 < bwl ? S_LINE : S_OUT;
                        break;
                    }
                } else if (d2 < w2 && sqrtf(d2) < bwl) {
                    *o = S_LINE;
                    break;
                }
            }
        }
    }
}

/* the pixels [x0, x1) x [y0, y1) of the picture (w wide) */
static void render_fast(const art_fig *f, const art_style *st, int w, int x0, int y0, int x1, int y1, float scale,
                        float ox, float oy, uint8_t *out)
{
    float wl = st->line;
    int n = f->n, most = 1;
    for (int k = 0; k < n; k++) most = f->s[k].n > most ? f->s[k].n : most;
    int *bidx = malloc((size_t)(n ? n : 1) * 2 * sizeof *bidx), *pidx = bidx + (n ? n : 1);
    uint8_t *bst = malloc((size_t)(n ? n : 1) * (2 + SS * SS)), *pst = bst + (n ? n : 1), *qs = pst + (n ? n : 1);
    float *xc = malloc(sizeof *xc * (size_t)most);
    int *ec = malloc(sizeof *ec * (size_t)most);
    if (!bidx || !bst || !xc || !ec) {
        free(bidx);
        free(bst);
        free(xc);
        free(ec);
        return;
    }
    /* a sample lies at most 0.375 px from its pixel's centre each way; a block's, 3.875 + 0.375 from its centre */
    float Rp = 0.375f * 1.41422f / scale * 1.001f, Rb = (BLK / 2.0f - 0.125f) * 1.41422f / scale * 1.001f;
    for (int by = y0; by < y1; by += BLK)
        for (int bx = x0; bx < x1; bx += BLK) {
            int ex = bx + BLK < x1 ? bx + BLK : x1, ey = by + BLK < y1 ? by + BLK : y1;
            /* the block's centre: of a full block's (a cut one is inside it) */
            float cx = (bx + BLK / 2.0f - ox) / scale, cy = (by + BLK / 2.0f - oy) / scale;
            int nb = 0, mix = 0;
            for (int k = 0; k < n; k++) {
                int q = region_state(f, &f->s[k], cx, cy, Rb, wl);
                if (q == S_OUT) continue;
                bidx[nb] = k;
                bst[nb++] = (uint8_t)q;
                mix |= q == S_MIX;
            }
            for (int py = by; py < ey; py++)
                for (int px = bx; px < ex; px++) {
                    uint8_t *o = out + 4 * ((size_t)py * w + px);
                    int np = 0, pmix = 0;
                    const int *idx = bidx;
                    const uint8_t *state = bst;
                    if (mix) {                      /* the block's edges: each pixel looks again, closer */
                        float x = (px + 0.5f - ox) / scale, y = (py + 0.5f - oy) / scale;
                        for (int i = 0; i < nb; i++) {
                            int q = bst[i] == S_MIX ? region_state(f, &f->s[bidx[i]], x, y, Rp, wl) : bst[i];
                            if (q == S_OUT) continue;
                            pidx[np] = bidx[i];
                            pst[np++] = (uint8_t)q;
                            pmix |= q == S_MIX;
                        }
                        idx = pidx;
                        state = pst;
                    } else {
                        np = nb;
                    }
                    if (!np) {
                        memset(o, 0, 4);
                        continue;
                    }
                    float acc[4] = {0, 0, 0, 0};
                    float c[3] = {0, 0, 0};
                    int has = 0, top = -1;
                    float xs[SS], ys[SS];
                    for (int k = 0; k < SS; k++) {
                        xs[k] = (px - ox + (k + 0.5f) / SS) / scale;
                        ys[k] = (py - oy + (k + 0.5f) / SS) / scale;
                    }
                    if (!pmix) {                    /* no edge: the same shapes everywhere, only the light moves */
                        has = sample(f, st, idx, state, np, xs[0], ys[0], c, &top);   /* a fill on top: its light varies */
                    } else {                        /* an edge: each shape's samples, a row at a time */
                        for (int i = 0; i < np; i++) {
                            if (state[i] == S_MIX)
                                pixel_states(f, &f->s[idx[i]], xs, ys, wl, qs + i * SS * SS, xc, ec);
                            else memset(qs + i * SS * SS, state[i], SS * SS);
                        }
                    }
                    for (int sy = 0; sy < SS; sy++)
                        for (int sx = 0; sx < SS; sx++) {
                            float x = xs[sx], y = ys[sy];
                            float cs[3];
                            int hs;
                            if (pmix) hs = sample_q(f, st, idx, qs + sy * SS + sx, np, x, y, cs);
                            else if (top >= 0) {
                                shade(st, &f->s[top], x, y, cs);
                                hs = 1;
                            } else {
                                memcpy(cs, c, sizeof cs);
                                hs = has;
                            }
                            if (hs) {
                                acc[0] += cs[0];
                                acc[1] += cs[1];
                                acc[2] += cs[2];
                                acc[3] += 1;
                            }
                        }
                    if (acc[3] > 0) {
                        for (int i = 0; i < 3; i++) o[i] = (uint8_t)(acc[i] / acc[3] + 0.5f);
                        o[3] = (uint8_t)(255 * acc[3] / (SS * SS) + 0.5f);
                    } else {
                        memset(o, 0, 4);
                    }
                }
        }
    free(bidx);
    free(bst);
    free(xc);
    free(ec);
}

/* ---- the helpers: a big picture's bands of blocks painted on the other cores too ----------------------------------
 * A band is BLK rows of blocks, painted as render_fast() paints it alone (the same pixels, whoever paints it). The
 * caller paints bands too, and waits for the last. Only one picture at a time: another thread's (or a picture while
 * one is being painted) is painted alone. RSB_ART_THREADS=n: n helpers (0: none). */
#define POOL_MAX 7
#define POOL_LEAST 3000                             /* pixels: a smaller picture is painted alone */
static struct {
    SDL_mutex *m;
    SDL_cond *go, *done;
    atomic_flag busy;                               /* a picture being painted with the helpers */
    int n, ready;                                   /* n helpers; ready: 1 made, -1 none to be had */
    unsigned gen;                                   /* a new picture (the helpers wake) */
    const art_fig *f;
    const art_style *st;
    int w, x0, y0, x1, y1, bands, next, left;       /* next: the next band to take; left: the bands not finished */
    float scale, ox, oy;
    uint8_t *out;
    void (*rows)(void *ctx, int y0, int y1);        /* another painter's rows (pool_rows), else a figure's */
    void *ctx;
    int band;                                       /* rows a band */
} pool;

/* takes bands and paints them, until none is left to take */
static void pool_bands(void)
{
    for (;;) {
        SDL_LockMutex(pool.m);
        int b = pool.next < pool.bands ? pool.next++ : -1;
        SDL_UnlockMutex(pool.m);
        if (b < 0) return;
        int y0 = pool.y0 + b * pool.band, y1 = y0 + pool.band < pool.y1 ? y0 + pool.band : pool.y1;
        if (pool.rows) pool.rows(pool.ctx, y0, y1);
        else render_fast(pool.f, pool.st, pool.w, pool.x0, y0, pool.x1, y1, pool.scale, pool.ox, pool.oy, pool.out);
        SDL_LockMutex(pool.m);
        if (--pool.left == 0) SDL_CondSignal(pool.done);
        SDL_UnlockMutex(pool.m);
    }
}

static int pool_helper(void *arg)
{
    (void)arg;
    unsigned seen = 0;
    for (;;) {
        SDL_LockMutex(pool.m);
        while (pool.gen == seen) SDL_CondWait(pool.go, pool.m);   /* asleep between pictures */
        seen = pool.gen;
        SDL_UnlockMutex(pool.m);
        pool_bands();
    }
    return 0;
}

static int pool_start(void)
{
    if (pool.ready) return pool.ready > 0;
    pool.ready = -1;
    const char *e = getenv("RSB_ART_THREADS");
    int n = e ? atoi(e) : SDL_GetCPUCount() - 1;
    if (!e && n > 3) n = 3;                         /* three helpers: a phone's big cores */
    if (n > POOL_MAX) n = POOL_MAX;
    if (n < 1) return 0;
    pool.m = SDL_CreateMutex();
    pool.go = SDL_CreateCond();
    pool.done = SDL_CreateCond();
    if (!pool.m || !pool.go || !pool.done) return 0;
    for (int i = 0; i < n; i++) {
        SDL_Thread *t = SDL_CreateThread(pool_helper, "art", NULL);
        if (!t) break;
        SDL_DetachThread(t);
        pool.n++;
    }
    if (pool.n) pool.ready = 1;
    return pool.ready > 0;
}

static int pool_run(int bands);

/* the picture's bands painted with the helpers; 0: not (none, or busy: the caller paints it alone) */
static int pool_render(const art_fig *f, const art_style *st, int w, int x0, int y0, int x1, int y1, float scale,
                       float ox, float oy, uint8_t *out)
{
    int bands = (y1 - y0 + BLK - 1) / BLK;
    if (bands < 2 || atomic_flag_test_and_set_explicit(&pool.busy, memory_order_acquire)) return 0;
    if (!pool_start()) {
        atomic_flag_clear_explicit(&pool.busy, memory_order_release);
        return 0;
    }
    SDL_LockMutex(pool.m);
    pool.f = f;
    pool.st = st;
    pool.w = w;
    pool.x0 = x0;
    pool.y0 = y0;
    pool.x1 = x1;
    pool.y1 = y1;
    pool.scale = scale;
    pool.ox = ox;
    pool.oy = oy;
    pool.out = out;
    pool.rows = NULL;
    pool.band = BLK;
    return pool_run(bands);
}

/* rows [0, h) painted by fn(ctx, y0, y1) in bands of `band` rows, with the helpers (each row's pixels its own: the
 * same pixels whoever paints them); alone when they are busy */
static void pool_rows(void (*fn)(void *ctx, int y0, int y1), void *ctx, int h, int band)
{
    int bands = (h + band - 1) / band;
    if (bands < 2 || atomic_flag_test_and_set_explicit(&pool.busy, memory_order_acquire)) {
        fn(ctx, 0, h);
        return;
    }
    if (!pool_start()) {
        atomic_flag_clear_explicit(&pool.busy, memory_order_release);
        fn(ctx, 0, h);
        return;
    }
    SDL_LockMutex(pool.m);
    pool.rows = fn;
    pool.ctx = ctx;
    pool.band = band;
    pool.y0 = 0;
    pool.y1 = h;
    pool_run(bands);
}

/* (pool.m held, the job set) the helpers woken, the caller's share, the last band waited for; then free again */
static int pool_run(int bands)
{
    pool.bands = pool.left = bands;
    pool.next = 0;
    pool.gen++;
    SDL_CondBroadcast(pool.go);
    SDL_UnlockMutex(pool.m);
    pool_bands();                                   /* the caller's share */
    SDL_LockMutex(pool.m);
    while (pool.left > 0) SDL_CondWait(pool.done, pool.m);
    pool.bands = pool.next = 0;                     /* a helper waking late finds nothing to take */
    SDL_UnlockMutex(pool.m);
    atomic_flag_clear_explicit(&pool.busy, memory_order_release);
    return 1;
}

static _Atomic int art_ref = -1;                    /* RSB_ART_REF=1: the reference painter (comparisons) */

/* the pixels the figure may touch (its shapes' boxes and outline, a pixel more): [*x0, *x1) x [*y0, *y1), clipped to
 * w x h; 0: none */
static int fig_pixels(const art_fig *f, float wl, int w, int h, float scale, float ox, float oy, int *x0, int *y0,
                      int *x1, int *y1)
{
    float a = 1e9f, b = 1e9f, c = -1e9f, d = -1e9f;
    for (int k = 0; k < f->n; k++) {
        a = fminf(a, f->s[k].x0 - wl);
        b = fminf(b, f->s[k].y0 - wl);
        c = fmaxf(c, f->s[k].x1 + wl);
        d = fmaxf(d, f->s[k].y1 + wl);
    }
    if (!f->n) return 0;
    float fx0 = floorf(a * scale + ox) - 1, fy0 = floorf(b * scale + oy) - 1;
    float fx1 = ceilf(c * scale + ox) + 1, fy1 = ceilf(d * scale + oy) + 1;
    *x0 = fx0 < 0 ? 0 : fx0 > w ? w : (int)fx0;
    *y0 = fy0 < 0 ? 0 : fy0 > h ? h : (int)fy0;
    *x1 = fx1 < 0 ? 0 : fx1 > w ? w : (int)fx1;
    *y1 = fy1 < 0 ? 0 : fy1 > h ? h : (int)fy1;
    return *x0 < *x1 && *y0 < *y1;
}

/* ---- small pictures remembered: the same figure painted again (a card's pips, its corner turned round, the same
 * piece on many cards) is copied, not painted. Keyed by everything the pixels follow (the shapes, the style, the
 * size and place), a 64-bit hash; any thread (a spin lock). */
#define MEMO_N 128
#define MEMO_MAX (80 * 80)                          /* pixels: a bigger picture is painted each time */
static struct { uint64_t key; int w, h; unsigned used; uint8_t *px; } memo[MEMO_N];
static unsigned memo_clock;
static atomic_flag memo_lock = ATOMIC_FLAG_INIT;   /* (C11 atomics: thread checkers see them) */

static uint64_t hash_add(uint64_t h, const void *p, size_t n)
{
    const unsigned char *b = p;
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 0x100000001b3ull;
    return h;
}

static uint64_t memo_key(const art_fig *f, const art_style *st, int w, int h, float scale, float ox, float oy)
{
    uint64_t k = 0xcbf29ce484222325ull;
    const float geo[3] = {scale, ox, oy};
    const int size[3] = {w, h, f->n};
    k = hash_add(k, size, sizeof size);
    k = hash_add(k, geo, sizeof geo);
    k = hash_add(k, st->fill, sizeof st->fill);
    k = hash_add(k, st->outer, sizeof st->outer);
    k = hash_add(k, st->inner, sizeof st->inner);
    k = hash_add(k, st->mark, sizeof st->mark);
    k = hash_add(k, &st->light, sizeof st->light);
    k = hash_add(k, &st->line, sizeof st->line);
    for (int i = 0; i < f->n; i++) {
        const shape *s = &f->s[i];
        const int is[3] = {s->kind, s->role, s->n};
        const float fs[8] = {s->cx, s->cy, s->rx, s->ry, s->x0, s->y0, s->x1, s->y1};
        k = hash_add(k, is, sizeof is);
        k = hash_add(k, fs, sizeof fs);
        if (s->kind == SH_POLY) k = hash_add(k, f->pt + s->p0, sizeof(v2) * (size_t)s->n);
    }
    return k;
}

static int memo_get(uint64_t key, int w, int h, uint8_t *out)
{
    int hit = 0;
    while (atomic_flag_test_and_set_explicit(&memo_lock, memory_order_acquire)) {}
    for (int i = 0; i < MEMO_N && !hit; i++)
        if (memo[i].px && memo[i].key == key && memo[i].w == w && memo[i].h == h) {
            memcpy(out, memo[i].px, (size_t)w * h * 4);
            memo[i].used = ++memo_clock;
            hit = 1;
        }
    atomic_flag_clear_explicit(&memo_lock, memory_order_release);
    return hit;
}

static void memo_put(uint64_t key, int w, int h, const uint8_t *px)
{
    uint8_t *copy = malloc((size_t)w * h * 4);
    if (!copy) return;
    memcpy(copy, px, (size_t)w * h * 4);
    while (atomic_flag_test_and_set_explicit(&memo_lock, memory_order_acquire)) {}
    int k = 0;
    for (int i = 0; i < MEMO_N; i++) {
        if (!memo[i].px) { k = i; break; }
        if (memo[i].used < memo[k].used) k = i;
    }
    uint8_t *old = memo[k].px;
    memo[k].px = copy;
    memo[k].key = key;
    memo[k].w = w;
    memo[k].h = h;
    memo[k].used = ++memo_clock;
    atomic_flag_clear_explicit(&memo_lock, memory_order_release);
    free(old);
}

static void render_now(const art_fig *f, const art_style *st, int w, int h, float scale, float ox, float oy,
                       uint8_t *out);

void art_render(const art_fig *f, const art_style *st, int w, int h, float scale, float ox, float oy, uint8_t *out)
{
    if (art_ref < 0) art_ref = getenv("RSB_ART_REF") && atoi(getenv("RSB_ART_REF")) > 0;
    if (art_ref) {
        render_ref(f, st, w, h, scale, ox, oy, out);
        return;
    }
    if (w <= 0 || h <= 0) return;
    if (w * h > MEMO_MAX) {
        render_now(f, st, w, h, scale, ox, oy, out);
        return;
    }
    uint64_t key = memo_key(f, st, w, h, scale, ox, oy);
    if (memo_get(key, w, h, out)) return;
    render_now(f, st, w, h, scale, ox, oy, out);
    memo_put(key, w, h, out);
}

static void render_now(const art_fig *f, const art_style *st, int w, int h, float scale, float ox, float oy,
                       uint8_t *out)
{
    int x0, y0, x1, y1;
    if (!fig_pixels(f, st->line, w, h, scale, ox, oy, &x0, &y0, &x1, &y1)) {
        memset(out, 0, (size_t)w * h * 4);
        return;
    }
    for (int y = 0; y < h; y++) {                   /* outside the figure: nothing */
        if (y < y0 || y >= y1) {
            memset(out + 4 * (size_t)y * w, 0, (size_t)w * 4);
            continue;
        }
        memset(out + 4 * (size_t)y * w, 0, (size_t)x0 * 4);
        memset(out + 4 * ((size_t)y * w + x1), 0, (size_t)(w - x1) * 4);
    }
    if ((x1 - x0) * (y1 - y0) < POOL_LEAST || !pool_render(f, st, w, x0, y0, x1, y1, scale, ox, oy, out))
        render_fast(f, st, w, x0, y0, x1, y1, scale, ox, oy, out);
}

void art_fig_blend(uint8_t *o, int pw, int ph, const art_fig *f, const art_style *st, int x, int y, int w, float a)
{
    if (w < 2) return;
    int x0 = 0, y0 = 0, x1 = w, y1 = w;             /* only the figure's pixels (the rest would blend nothing) */
    if (art_ref < 0) art_ref = getenv("RSB_ART_REF") && atoi(getenv("RSB_ART_REF")) > 0;
    if (!art_ref && !fig_pixels(f, st->line, w, w, (float)w, 0, 0, &x0, &y0, &x1, &y1)) return;
    int bw = x1 - x0, bh = y1 - y0;
    uint8_t *b = malloc((size_t)bw * bh * 4);
    if (!b) return;
    art_render(f, st, bw, bh, (float)w, (float)-x0, (float)-y0, b);
    if (a < 1)
        for (int i = 0; i < bw * bh; i++) b[4 * i + 3] = (uint8_t)(b[4 * i + 3] * a);
    art_blend(o, pw, ph, b, bw, bh, x + x0, y + y0);
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
    f->n = f->npt = 0;
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

/* the wood inside (iw x ih pixels: squares of sq when checkered; a plank's grain at the scale of a plank gs wide) in
 * its walnut frame (fr pixels) */
typedef struct { int iw, ih, sq, fr, checkered; float gs; uint8_t *o; } board_job;

static void board_rows(void *ctx, int y0, int y1)
{
    const board_job *j = ctx;
    int iw = j->iw, ih = j->ih, sq = j->sq, fr = j->fr, checkered = j->checkered;
    float gs = j->gs;
    uint8_t *o = j->o;
    static const float light[3] = {236, 212, 172}, dark[3] = {176, 124, 82}, plain[3] = {222, 184, 128},
                       frame[3] = {92, 60, 40};
    int w = iw + 2 * fr, h = ih + 2 * fr;
    float rad = fr * 0.7f;
    for (int y = y0; y < y1; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            float c[3], a = 1;
            int bx = x - fr, by = y - fr;
            if (bx >= 0 && by >= 0 && bx < iw && by < ih) {
                float u = bx / gs, v = by / gs;
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

/* (its rows painted on the helpers too: a board is many pixels) */
static void board(int iw, int ih, int sq, float gs, int fr, int checkered, uint8_t *o)
{
    board_job j = {iw, ih, sq, fr, checkered, gs, o};
    pool_rows(board_rows, &j, ih + 2 * fr, 16);
}

void art_board_n(int nx, int ny, int sq, int fr, int checkered, uint8_t *o)
{
    board(nx * sq, ny * sq, sq, (float)sq, fr, checkered, o);
}

void art_plank(int w, int h, int fr, uint8_t *o)
{
    int s = w < h ? w : h;                      /* the grain as on a square plank of the short side */
    board(w, h, 1, (float)(s > 1 ? s : 1), fr, 0, o);
}

void art_board(int sq, int fr, uint8_t *o) { art_board_n(8, 8, sq, fr, 1, o); }

void art_inlay(uint8_t *o, int pw, int ph, float x0, float y0, float w, float h, float rad, const float tone[3],
               float seam)
{
    for (int y = (int)floorf(y0); y < (int)ceilf(y0 + h); y++)
        for (int x = (int)floorf(x0); x < (int)ceilf(x0 + w); x++) {
            if (x < 0 || y < 0 || x >= pw || y >= ph) continue;
            float px = x + 0.5f - x0, py = y + 0.5f - y0;
            float dx = fmaxf(rad - px, px - (w - rad)), dy = fmaxf(rad - py, py - (h - rad)), d;
            if (dx > 0 && dy > 0) d = rad - hypotf(dx, dy);                 /* in a rounded corner */
            else d = fminf(fminf(px, w - px), fminf(py, h - py));           /* inside: to the nearest side */
            float in = clampf(d + 0.5f, 0, 1);
            if (in <= 0) continue;
            float k = d < seam ? 0.72f + 0.28f * clampf(d / seam - 0.4f, 0, 1) : 1;   /* the seam */
            uint8_t *p = o + 4 * ((size_t)y * pw + x);
            for (int c = 0; c < 3; c++) {
                float v = p[c] * tone[c] * k;
                p[c] = (uint8_t)clampf(p[c] + (v - p[c]) * in, 0, 255);
            }
        }
}

float art_hash(int a, int b)
{
    unsigned h = (unsigned)a * 73856093u ^ ((unsigned)b * 19349663u + 0x5bd1e995u);   /* wrapping, as int's did */
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (h & 0xffff) / 65535.0f;
}

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

void art_tint_rect(uint8_t *o, int w, int h, float x0, float y0, float x1, float y1, const uint8_t rgb[3], float a)
{
    int ix0 = (int)floorf(x0), iy0 = (int)floorf(y0), ix1 = (int)ceilf(x1), iy1 = (int)ceilf(y1);
    for (int y = iy0 < 0 ? 0 : iy0; y < iy1 && y < h; y++)
        for (int x = ix0 < 0 ? 0 : ix0; x < ix1 && x < w; x++) {
            float cx = fminf(x + 1.0f, x1) - fmaxf((float)x, x0), cy = fminf(y + 1.0f, y1) - fmaxf((float)y, y0);
            float k = a * clampf(cx, 0, 1) * clampf(cy, 0, 1);
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] + (rgb[c] - p[c]) * k);
            if (p[3] < 255) p[3] = (uint8_t)(p[3] + (255 - p[3]) * k);
        }
}

/* the band of a player's colour round a cup: the quad (x, y four times, of the unit square) */
static void cup_band(uint8_t *o, int s, const float band[8], int player)
{
    art_fig *f = art_fig_new();
    art_poly(f, ART_PART, band, 4);
    art_style bs = art_player[player < 0 ? 0 : player % ART_PLAYERS];
    bs.line = 0.012f;
    uint8_t *b = calloc((size_t)s * s, 4);
    if (b) {
        art_render(f, &bs, s, s, (float)s, 0, 0, b);
        art_blend(o, s, s, b, s, s, 0, 0);
        free(b);
    }
    art_fig_free(f);
}

void art_cup(int kind, int player, int s, uint8_t *o)
{
    memset(o, 0, (size_t)s * s * 4);
    art_fig *f = art_fig_new();
    if (kind == ART_CUP_LEATHER) {
        static const uint8_t leather[3] = {122, 74, 44}, ink[3] = {58, 34, 20}, top[3] = {150, 96, 60},
                             stitch[3] = {214, 182, 140};
        static const float body[] = {0.3f, 0.12f, 0.7f, 0.12f, 0.86f, 0.88f, 0.14f, 0.88f},
                           band[] = {0.188f, 0.68f, 0.812f, 0.68f, 0.835f, 0.79f, 0.165f, 0.79f};
        art_poly(f, ART_PART, body, 4);
        art_ellipse(f, ART_PART, 0.5f, 0.88f, 0.36f, 0.06f);
        art_style st = art_flat(leather, ink, 0.025f, 0.45f);
        art_render(f, &st, s, s, (float)s, 0, 0, o);
        art_fig_free(f);
        f = art_fig_new();                      /* the top, a little lighter */
        art_ellipse(f, ART_PART, 0.5f, 0.12f, 0.2f, 0.045f);
        art_fig_paint(o, s, f, top, ink, 0.015f, 0.1f);
        cup_band(o, s, band, player);
        f = art_fig_new();                      /* stitches */
        for (int i = 0; i < 7; i++) {
            float y = 0.2f + i * 0.065f;
            art_rect(f, ART_PART, 0.5f - 0.006f, y, 0.5f + 0.006f, y + 0.03f);
        }
        art_fig_paint(o, s, f, stitch, NULL, 0, 0.1f);
        return;
    }
    static const uint8_t wood[3] = {124, 80, 50}, ink[3] = {52, 32, 20}, brass[3] = {206, 164, 84},
                         brass_ink[3] = {120, 88, 36}, top[3] = {160, 110, 72};
    static const float body[] = {0.29f, 0.14f, 0.71f, 0.14f, 0.77f, 0.8f, 0.23f, 0.8f},
                       band[] = {0.255f, 0.42f, 0.745f, 0.42f, 0.755f, 0.56f, 0.245f, 0.56f},
                       r1[] = {0.27f, 0.3f, 0.73f, 0.3f, 0.734f, 0.345f, 0.266f, 0.345f},
                       r2[] = {0.226f, 0.66f, 0.774f, 0.66f, 0.778f, 0.705f, 0.222f, 0.705f};
    art_poly(f, ART_PART, body, 4);
    art_ellipse(f, ART_PART, 0.5f, 0.14f, 0.21f, 0.05f);
    art_fig_paint(o, s, f, wood, ink, 0.022f, 0.55f);
    f = art_fig_new();                          /* the lip */
    art_rect(f, ART_PART, 0.17f, 0.8f, 0.83f, 0.88f);
    art_ellipse(f, ART_PART, 0.5f, 0.88f, 0.33f, 0.05f);
    art_fig_paint(o, s, f, wood, ink, 0.02f, 0.4f);
    f = art_fig_new();                          /* the rounded top, lighter */
    art_ellipse(f, ART_PART, 0.5f, 0.14f, 0.18f, 0.035f);
    art_fig_paint(o, s, f, top, ink, 0.012f, 0.1f);
    cup_band(o, s, band, player);
    f = art_fig_new();                          /* the brass rings */
    art_poly(f, ART_PART, r1, 4);
    art_poly(f, ART_PART, r2, 4);
    art_fig_paint(o, s, f, brass, brass_ink, 0.01f, 0.5f);
}

/* ---- the buttons' icons ------------------------------------------------------------------------------------------ */
void art_icon(int which, int size, uint8_t *rgba)
{
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new(), *cut = NULL;       /* cut: the holes (the app's pages' icons) */
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
    case ICON_REPLAY: {                             /* an arrow going nearly round clockwise (as most apps' replay),
                                                     * its head at the top right end, pointing on round */
        art_arc(f, ART_PART, 0.5f, 0.52f, 0.20f, 0.29f, 70, 335);
        float a0 = 70 * PI_F / 180, a1 = a0 - 34 * PI_F / 180, rm = 0.245f, hw = 0.125f;
        const float hd[] = {0.5f + (rm + hw) * cosf(a0), 0.52f - (rm + hw) * sinf(a0), 0.5f + rm * cosf(a1),
                            0.52f - rm * sinf(a1), 0.5f + (rm - hw) * cosf(a0), 0.52f - (rm - hw) * sinf(a0)};
        art_poly(f, ART_PART, hd, 3);
        break;
    }
    case ICON_PAR: {                                /* a flag on its pole, the hole's rim at its foot */
        const float flag[] = {0.40f, 0.17f, 0.80f, 0.32f, 0.40f, 0.47f};
        art_ellipse(f, ART_PART, 0.37f, 0.80f, 0.19f, 0.05f);
        art_rect(f, ART_PART, 0.335f, 0.15f, 0.405f, 0.80f);
        art_poly(f, ART_PART, flag, 3);
        break;
    }
    case ICON_EYE:                                  /* an eye: its two lids, the iris between them */
        art_arc(f, ART_PART, 0.5f, 0.76f, 0.43f, 0.49f, 34, 146);
        art_arc(f, ART_PART, 0.5f, 0.24f, 0.43f, 0.49f, 214, 326);
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.14f, 0.14f);
        break;
    case ICON_PENCIL: {                             /* a pencil lying from the lower left up, its point, its cap */
        const float tip[] = {0.335f, 0.745f, 0.255f, 0.665f, 0.17f, 0.83f};
        art_band(f, ART_PART, 0.32f, 0.68f, 0.69f, 0.31f, 0.17f);
        art_band(f, ART_PART, 0.725f, 0.275f, 0.81f, 0.19f, 0.17f);
        art_poly(f, ART_PART, tip, 3);
        break;
    }
    case ICON_ERASER: {                             /* an eraser leaning on the page: its rubber, its sleeve */
        const float c = 0.819f, s = 0.574f, ax = 0.262f, ay = 0.60f, l = 0.58f;
        art_band(f, ART_PART, ax, ay, ax + 0.36f * l * c, ay - 0.36f * l * s, 0.30f);
        art_band(f, ART_PART, ax + 0.42f * l * c, ay - 0.42f * l * s, ax + l * c, ay - l * s, 0.30f);
        art_rect(f, ART_PART, 0.18f, 0.80f, 0.82f, 0.845f);
        break;
    }
    case ICON_HOURGLASS: {                          /* an hourglass: its two glasses between their boards */
        const float up[] = {0.31f, 0.25f, 0.69f, 0.25f, 0.53f, 0.50f, 0.47f, 0.50f},
                    down[] = {0.47f, 0.50f, 0.53f, 0.50f, 0.69f, 0.75f, 0.31f, 0.75f};
        art_rect(f, ART_PART, 0.25f, 0.16f, 0.75f, 0.23f);
        art_rect(f, ART_PART, 0.25f, 0.77f, 0.75f, 0.84f);
        art_poly(f, ART_PART, up, 4);
        art_poly(f, ART_PART, down, 4);
        break;
    }
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
    case ICON_MENU:                                 /* three bars, rounded at their ends */
        for (int k = 0; k < 3; k++) {
            float y = 0.30f + 0.20f * k;
            art_rect(f, ART_PART, 0.27f, y - 0.04f, 0.73f, y + 0.04f);
            art_ellipse(f, ART_PART, 0.27f, y, 0.04f, 0.04f);
            art_ellipse(f, ART_PART, 0.73f, y, 0.04f, 0.04f);
        }
        break;
    case ICON_PERSON:                               /* a person: the head, the shoulders */
        art_ellipse(f, ART_PART, 0.5f, 0.335f, 0.145f, 0.145f);
        art_arc(f, ART_PART, 0.5f, 0.87f, 0.0f, 0.31f, 0, 180);
        break;
    case ICON_BOT: {                                /* a robot's head: the antenna, the ears, a rounded head */
        float x0 = 0.21f, x1 = 0.79f, y0 = 0.29f, y1 = 0.80f, r = 0.09f;
        art_ellipse(f, ART_PART, 0.5f, 0.135f, 0.06f, 0.06f);
        art_rect(f, ART_PART, 0.47f, 0.17f, 0.53f, 0.30f);
        art_rect(f, ART_PART, 0.12f, 0.45f, 0.22f, 0.64f);
        art_rect(f, ART_PART, 0.78f, 0.45f, 0.88f, 0.64f);
        art_rect(f, ART_PART, x0 + r, y0, x1 - r, y1);
        art_rect(f, ART_PART, x0, y0 + r, x1, y1 - r);
        for (int k = 0; k < 4; k++) art_ellipse(f, ART_PART, k & 1 ? x1 - r : x0 + r, k & 2 ? y1 - r : y0 + r, r, r);
        break;
    }
    case ICON_LOCK: {                               /* a padlock: its shackle, its body (rounded corners) */
        float x0 = 0.24f, x1 = 0.76f, y0 = 0.45f, y1 = 0.84f, r = 0.06f;
        art_arc(f, ART_PART, 0.5f, 0.42f, 0.115f, 0.19f, 0, 180);
        art_rect(f, ART_PART, 0.31f, 0.42f, 0.385f, 0.47f);
        art_rect(f, ART_PART, 0.615f, 0.42f, 0.69f, 0.47f);
        art_rect(f, ART_PART, x0 + r, y0, x1 - r, y1);
        art_rect(f, ART_PART, x0, y0 + r, x1, y1 - r);
        for (int k = 0; k < 4; k++) art_ellipse(f, ART_PART, k & 1 ? x1 - r : x0 + r, k & 2 ? y1 - r : y0 + r, r, r);
        break;
    }
    /* ---- the app's pages (the ribbon, the Settings and Stats pages): flat, white, cut out where they show through */
    case ICON_BACK:                                 /* back: an arrow head pointing left */
        art_band(f, ART_PART, 0.62f, 0.2f, 0.34f, 0.5f, 0.1f);
        art_band(f, ART_PART, 0.34f, 0.5f, 0.62f, 0.8f, 0.1f);
        art_ellipse(f, ART_PART, 0.34f, 0.5f, 0.05f, 0.05f);
        break;
    case ICON_INFO:                                 /* the rules: an i in a ring */
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.3f, 0.37f, 0, 360);
        art_ellipse(f, ART_PART, 0.5f, 0.33f, 0.055f, 0.055f);
        art_rect(f, ART_PART, 0.46f, 0.43f, 0.54f, 0.69f);
        break;
    case ICON_STATS:                                /* three bars, rounded, of three heights */
        for (int k = 0; k < 3; k++) {
            static const float top[3] = {0.48f, 0.22f, 0.62f};
            float x = 0.27f + 0.23f * k;
            art_rect(f, ART_PART, x - 0.075f, top[k], x + 0.075f, 0.78f);
            art_ellipse(f, ART_PART, x, top[k], 0.075f, 0.075f);
            art_ellipse(f, ART_PART, x, 0.78f, 0.075f, 0.075f);
        }
        break;
    case ICON_TILES:                                /* the games: four rounded tiles */
        for (int k = 0; k < 4; k++) {
            float x = k & 1 ? 0.53f : 0.17f, y = k & 2 ? 0.53f : 0.17f, w = 0.30f, r = 0.07f;
            art_rect(f, ART_PART, x + r, y, x + w - r, y + w);
            art_rect(f, ART_PART, x, y + r, x + w, y + w - r);
            for (int c = 0; c < 4; c++)
                art_ellipse(f, ART_PART, c & 1 ? x + w - r : x + r, c & 2 ? y + w - r : y + r, r, r);
        }
        break;
    case ICON_GEAR:                                 /* a gear: eight teeth, a hole */
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.27f, 0.27f);
        for (int k = 0; k < 8; k++) {
            float a = k * PI_F / 4, c = cosf(a), s = sinf(a);
            art_band(f, ART_PART, 0.5f + 0.2f * c, 0.5f + 0.2f * s, 0.5f + 0.37f * c, 0.5f + 0.37f * s, 0.13f);
        }
        cut = art_fig_new();
        art_ellipse(cut, ART_PART, 0.5f, 0.5f, 0.11f, 0.11f);
        break;
    case ICON_SEARCH:                               /* a magnifying glass */
        art_arc(f, ART_PART, 0.43f, 0.43f, 0.17f, 0.245f, 0, 360);
        art_band(f, ART_PART, 0.6f, 0.6f, 0.81f, 0.81f, 0.11f);
        art_ellipse(f, ART_PART, 0.81f, 0.81f, 0.055f, 0.055f);
        break;
    case ICON_FLAME: {                              /* a flame: a drop pointing up, a tongue at its side, a hole */
        const float tip[] = {0.27f, 0.6f, 0.47f, 0.10f, 0.73f, 0.6f}, side[] = {0.62f, 0.44f, 0.74f, 0.28f, 0.76f, 0.6f};
        const float in[] = {0.42f, 0.72f, 0.5f, 0.46f, 0.58f, 0.72f};
        art_ellipse(f, ART_PART, 0.5f, 0.62f, 0.235f, 0.235f);
        art_poly(f, ART_PART, tip, 3);
        art_poly(f, ART_PART, side, 3);
        cut = art_fig_new();
        art_ellipse(cut, ART_PART, 0.5f, 0.72f, 0.085f, 0.085f);
        art_poly(cut, ART_PART, in, 3);
        break;
    }
    case ICON_CUP:                                  /* a cup won: its bowl, its handles, its foot */
        art_rect(f, ART_PART, 0.27f, 0.18f, 0.73f, 0.32f);
        art_arc(f, ART_PART, 0.5f, 0.32f, 0.0f, 0.23f, 180, 360);
        art_arc(f, ART_PART, 0.27f, 0.34f, 0.07f, 0.12f, 90, 270);
        art_arc(f, ART_PART, 0.73f, 0.34f, 0.07f, 0.12f, -90, 90);
        art_rect(f, ART_PART, 0.455f, 0.5f, 0.545f, 0.7f);
        art_rect(f, ART_PART, 0.31f, 0.7f, 0.69f, 0.81f);
        break;
    case ICON_REVIEW: {                             /* a speech bubble, a star cut in it */
        const float tail[] = {0.30f, 0.62f, 0.48f, 0.62f, 0.26f, 0.84f};
        float x0 = 0.15f, x1 = 0.85f, y0 = 0.18f, y1 = 0.68f, r = 0.1f;
        art_rect(f, ART_PART, x0 + r, y0, x1 - r, y1);
        art_rect(f, ART_PART, x0, y0 + r, x1, y1 - r);
        for (int k = 0; k < 4; k++) art_ellipse(f, ART_PART, k & 1 ? x1 - r : x0 + r, k & 2 ? y1 - r : y0 + r, r, r);
        art_poly(f, ART_PART, tail, 3);
        cut = art_fig_new();
        art_star(cut, ART_PART, 0.5f, 0.445f, 0.17f);
        break;
    }
    case ICON_BUG:                                  /* a beetle: its head, its body split by its wings, six legs */
        art_ellipse(f, ART_PART, 0.5f, 0.57f, 0.2f, 0.25f);
        art_ellipse(f, ART_PART, 0.5f, 0.28f, 0.12f, 0.1f);
        for (int k = 0; k < 3; k++) {
            float y = 0.46f + 0.13f * k, dy = (k - 1) * 0.07f;
            art_band(f, ART_PART, 0.3f, y, 0.15f, y + dy, 0.055f);
            art_band(f, ART_PART, 0.7f, y, 0.85f, y + dy, 0.055f);
        }
        art_band(f, ART_PART, 0.44f, 0.22f, 0.34f, 0.1f, 0.045f);
        art_band(f, ART_PART, 0.56f, 0.22f, 0.66f, 0.1f, 0.045f);
        cut = art_fig_new();
        art_rect(cut, ART_PART, 0.485f, 0.4f, 0.515f, 0.84f);
        art_rect(cut, ART_PART, 0.3f, 0.37f, 0.7f, 0.4f);
        break;
    case ICON_SUN:                                  /* the sun: a disc, eight rays */
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.15f, 0.15f);
        for (int k = 0; k < 8; k++) {
            float a = k * PI_F / 4, c = cosf(a), s = sinf(a);
            art_band(f, ART_PART, 0.5f + 0.24f * c, 0.5f + 0.24f * s, 0.5f + 0.35f * c, 0.5f + 0.35f * s, 0.065f);
        }
        break;
    case ICON_MOON:                                 /* a crescent */
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.3f, 0.3f);
        cut = art_fig_new();
        art_ellipse(cut, ART_PART, 0.66f, 0.38f, 0.25f, 0.25f);
        break;
    case ICON_AUTO:                                 /* as the device: a ring, its left half filled */
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.24f, 0.32f, 0, 360);
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.0f, 0.245f, 90, 270);
        break;
    case ICON_GLOBE:                                /* the languages: a globe, its lines */
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.28f, 0.34f, 0, 360);
        art_rect(f, ART_PART, 0.18f, 0.475f, 0.82f, 0.525f);
        art_rect(f, ART_PART, 0.25f, 0.325f, 0.75f, 0.37f);
        art_rect(f, ART_PART, 0.25f, 0.63f, 0.75f, 0.675f);
        art_arc(f, ART_PART, 0.68f, 0.5f, 0.31f, 0.36f, 120, 240);
        art_arc(f, ART_PART, 0.32f, 0.5f, 0.31f, 0.36f, -60, 60);
        break;
    case ICON_AWAKE: {                              /* the screen kept on: a phone, a sun on its screen */
        const float dev[] = {0.30f, 0.14f, 0.70f, 0.14f, 0.70f, 0.86f, 0.30f, 0.86f, 0.30f, 0.14f,
                             0.36f, 0.22f, 0.36f, 0.74f, 0.64f, 0.74f, 0.64f, 0.22f, 0.36f, 0.22f};
        art_poly(f, ART_PART, dev, 10);
        art_ellipse(f, ART_PART, 0.5f, 0.8f, 0.03f, 0.03f);
        art_ellipse(f, ART_PART, 0.5f, 0.48f, 0.065f, 0.065f);
        for (int k = 0; k < 8; k++) {
            float a = k * PI_F / 4, c = cosf(a), s = sinf(a);
            art_band(f, ART_PART, 0.5f + 0.1f * c, 0.48f + 0.1f * s, 0.5f + 0.15f * c, 0.48f + 0.15f * s, 0.035f);
        }
        break;
    }
    case ICON_SHIELD: {                             /* a shield, a keyhole cut in it */
        const float sh[] = {0.5f, 0.12f, 0.79f, 0.23f, 0.77f, 0.5f, 0.66f, 0.72f, 0.5f, 0.87f,
                            0.34f, 0.72f, 0.23f, 0.5f, 0.21f, 0.23f};
        art_poly(f, ART_PART, sh, 8);
        cut = art_fig_new();
        art_ellipse(cut, ART_PART, 0.5f, 0.42f, 0.07f, 0.07f);
        art_rect(cut, ART_PART, 0.47f, 0.44f, 0.53f, 0.64f);
        break;
    }
    case ICON_DOC: {                                /* a page, its corner folded, its lines cut */
        const float pg[] = {0.25f, 0.13f, 0.58f, 0.13f, 0.77f, 0.32f, 0.77f, 0.87f, 0.25f, 0.87f};
        const float fold[] = {0.55f, 0.13f, 0.55f, 0.35f, 0.77f, 0.35f};
        art_poly(f, ART_PART, pg, 5);
        cut = art_fig_new();
        art_poly(cut, ART_PART, fold, 3);
        art_rect(cut, ART_PART, 0.35f, 0.5f, 0.67f, 0.555f);
        art_rect(cut, ART_PART, 0.35f, 0.65f, 0.67f, 0.705f);
        break;
    }
    case ICON_CODE:                                 /* the code: < / > */
        art_band(f, ART_PART, 0.36f, 0.29f, 0.17f, 0.5f, 0.085f);
        art_band(f, ART_PART, 0.17f, 0.5f, 0.36f, 0.71f, 0.085f);
        art_band(f, ART_PART, 0.64f, 0.29f, 0.83f, 0.5f, 0.085f);
        art_band(f, ART_PART, 0.83f, 0.5f, 0.64f, 0.71f, 0.085f);
        art_ellipse(f, ART_PART, 0.17f, 0.5f, 0.042f, 0.042f);
        art_ellipse(f, ART_PART, 0.83f, 0.5f, 0.042f, 0.042f);
        art_band(f, ART_PART, 0.56f, 0.22f, 0.44f, 0.78f, 0.07f);
        break;
    case ICON_CHEVRON:                              /* a thin arrow head pointing right: go on */
        art_band(f, ART_PART, 0.4f, 0.24f, 0.64f, 0.5f, 0.09f);
        art_band(f, ART_PART, 0.64f, 0.5f, 0.4f, 0.76f, 0.09f);
        art_ellipse(f, ART_PART, 0.64f, 0.5f, 0.045f, 0.045f);
        break;
    case ICON_HEART: {                              /* a heart: support */
        const float v[] = {0.18f, 0.44f, 0.82f, 0.44f, 0.5f, 0.84f};
        art_ellipse(f, ART_PART, 0.35f, 0.38f, 0.175f, 0.175f);
        art_ellipse(f, ART_PART, 0.65f, 0.38f, 0.175f, 0.175f);
        art_poly(f, ART_PART, v, 3);
        break;
    }
    case ICON_BOLT: {                               /* a bolt: XP */
        const float b[] = {0.58f, 0.10f, 0.25f, 0.56f, 0.47f, 0.56f, 0.40f, 0.90f, 0.76f, 0.42f, 0.54f, 0.42f};
        art_poly(f, ART_PART, b, 6);
        break;
    }
    case ICON_MEDAL: {                              /* a medal: its two ribbons, the disc, a ring and a star cut in it */
        const float l[] = {0.30f, 0.52f, 0.46f, 0.60f, 0.38f, 0.90f, 0.31f, 0.80f, 0.22f, 0.82f},
                    r[] = {0.70f, 0.52f, 0.54f, 0.60f, 0.62f, 0.90f, 0.69f, 0.80f, 0.78f, 0.82f};
        art_poly(f, ART_PART, l, 5);
        art_poly(f, ART_PART, r, 5);
        art_ellipse(f, ART_PART, 0.5f, 0.4f, 0.26f, 0.26f);
        cut = art_fig_new();
        art_arc(cut, ART_PART, 0.5f, 0.4f, 0.17f, 0.2f, 0, 360);
        art_star(cut, ART_PART, 0.5f, 0.41f, 0.11f);
        break;
    }
    }
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
    if (cut) {                                      /* the holes: the button shows through */
        uint8_t *hole = calloc((size_t)size * size, 4);
        if (hole) {
            art_render(cut, &st, size, size, (float)size, 0, 0, hole);
            for (int i = 0; i < size * size; i++) rgba[4 * i + 3] = (uint8_t)(rgba[4 * i + 3] * (255 - hole[4 * i + 3]) / 255);
        }
        free(hole);
        art_fig_free(cut);
    }
    if (which == ICON_BOT || which == ICON_LOCK) {  /* its eyes and mouth, its keyhole cut out: the button shows
                                                     * through */
        art_fig *c = art_fig_new();
        uint8_t *cut = calloc((size_t)size * size, 4);
        if (c && cut && which == ICON_LOCK) {
            art_ellipse(c, ART_PART, 0.5f, 0.6f, 0.065f, 0.065f);
            art_rect(c, ART_PART, 0.47f, 0.6f, 0.53f, 0.73f);
        } else if (c && cut) {
            art_ellipse(c, ART_PART, 0.385f, 0.49f, 0.075f, 0.075f);
            art_ellipse(c, ART_PART, 0.615f, 0.49f, 0.075f, 0.075f);
            art_rect(c, ART_PART, 0.36f, 0.645f, 0.64f, 0.70f);
        }
        if (c && cut) {
            art_render(c, &st, size, size, (float)size, 0, 0, cut);
            for (int i = 0; i < size * size; i++) rgba[4 * i + 3] = (uint8_t)(rgba[4 * i + 3] * (255 - cut[4 * i + 3]) / 255);
        }
        free(cut);
        if (c) art_fig_free(c);
    }
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

void art_roundrect_put(uint8_t *o, int w, int h, int x, int y, int rw, int rh, float rad, const uint8_t rgb[3],
                       float a)
{
    if (rw < 1 || rh < 1) return;
    const uint8_t fill[4] = {rgb[0], rgb[1], rgb[2], (uint8_t)(255 * a)};
    uint8_t *b = malloc((size_t)rw * rh * 4);
    if (!b) return;
    art_roundrect(rw, rh, rad, fill, b);
    art_blend(o, w, h, b, rw, rh, x, y);
    free(b);
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
            if (!s[3]) continue;                    /* nothing to blend (as the sum would leave it) */
            uint8_t *d = dst + 4 * ((y + j) * dw + x + i);
            if (s[3] == 255) {                      /* opaque: the source (as the sum gives it) */
                memcpy(d, s, 4);
                continue;
            }
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
