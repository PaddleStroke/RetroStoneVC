/*
 * Map Quiz's maps and quiz, without SDL (mapquiz.h): the generated outlines decoded, each region's fill cut into
 * triangles, the finger's region found, a map painted into a picture, the prompts and the stars.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/mapquiz.h"
#include "games/mapquiz_data.h"

const mq_map_def *mq_def(int map) { return map >= 0 && map < MQ_MAPS ? &mq_maps[map] : NULL; }
const mq_level_def *mq_level(int level) { return level >= 0 && level < MQ_LEVELS ? &mq_levels[level] : NULL; }
int mq_chapter_of(int level) { return level >= 0 && level < MQ_LEVELS ? mq_levels[level].chapter : 0; }
int mq_chapter_map(int chapter) { return chapter >= 0 && chapter < MQ_CHAPTERS ? mq_chapter_maps[chapter] : 0; }
const char *mq_map_set(int map) { return map >= 0 && map < MQ_MAPS ? mq_sets[map] : "*"; }

/* ---- ear clipping ------------------------------------------------------------------------------------------------
 * A ring (n points) cut into n - 2 triangles: an ear (a corner whose triangle holds no other corner) cut off at a time,
 * walking round. A ring that crosses itself (simplified outlines can, by a hair) still ends: when a whole turn finds no
 * ear, the corner is cut anyway. */
static float cross3(const float *p, int a, int b, int c)
{
    return (p[2 * b] - p[2 * a]) * (p[2 * c + 1] - p[2 * a + 1]) - (p[2 * b + 1] - p[2 * a + 1]) * (p[2 * c] - p[2 * a]);
}

static int in_tri(const float *p, int a, int b, int c, int q)
{
    float x = p[2 * q], y = p[2 * q + 1];
    float d1 = (p[2 * b] - p[2 * a]) * (y - p[2 * a + 1]) - (p[2 * b + 1] - p[2 * a + 1]) * (x - p[2 * a]);
    float d2 = (p[2 * c] - p[2 * b]) * (y - p[2 * b + 1]) - (p[2 * c + 1] - p[2 * b + 1]) * (x - p[2 * b]);
    float d3 = (p[2 * a] - p[2 * c]) * (y - p[2 * c + 1]) - (p[2 * a + 1] - p[2 * c + 1]) * (x - p[2 * c]);
    return d1 >= 0 && d2 >= 0 && d3 >= 0;
}

/* the ring's points p[first .. first + n - 1] (pairs), its triangles appended to out (indices into p); returns how
 * many corners were written (3 a triangle) */
int mq_earclip(const float *p, int first, int n, int *out)
{
    if (n < 3) return 0;
    int *nx = malloc(sizeof(int) * (size_t)n), *pv = malloc(sizeof(int) * (size_t)n);
    uint8_t *reflex = malloc((size_t)n);
    if (!nx || !pv || !reflex) {
        free(nx), free(pv), free(reflex);
        return 0;
    }
    double a2 = 0;                              /* turning the right way: counter-clockwise on the grid (y down: the
                                                 * signed area below 0) */
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        a2 += (double)p[2 * (first + i)] * p[2 * (first + j) + 1] - (double)p[2 * (first + j)] * p[2 * (first + i) + 1];
    }
    float sgn = a2 > 0 ? 1.0f : -1.0f;
    for (int i = 0; i < n; i++) {
        nx[i] = (i + 1) % n;
        pv[i] = (i + n - 1) % n;
    }
#define CR(a, b, c) (sgn * cross3(p, first + (a), first + (b), first + (c)))
    for (int i = 0; i < n; i++) reflex[i] = CR(pv[i], i, nx[i]) <= 0;
    int k = 0, left = n, i = 0, stuck = 0;
    while (left > 3) {
        int a = pv[i], c = nx[i], ear = 0;
        if (!reflex[i]) {
            ear = 1;
            for (int q = nx[c]; q != a && ear; q = nx[q])
                if (reflex[q] && q != i) {
                    /* the triangle a, i, c held the right way round */
                    int A = first + a, B = first + i, C = first + c, Q = first + q;
                    ear = sgn > 0 ? !in_tri(p, A, B, C, Q) : !in_tri(p, A, C, B, Q);
                }
        }
        if (ear || stuck > left) {
            out[k++] = first + a;
            out[k++] = first + i;
            out[k++] = first + c;
            nx[a] = c;
            pv[c] = a;
            left--;
            reflex[a] = CR(pv[a], a, nx[a]) <= 0;
            reflex[c] = CR(pv[c], c, nx[c]) <= 0;
            i = c;
            stuck = 0;
        } else {
            i = nx[i];
            stuck++;
        }
    }
#undef CR
    out[k++] = first + pv[i];
    out[k++] = first + i;
    out[k++] = first + nx[i];
    free(nx), free(pv), free(reflex);
    return k;
}

/* ---- a map loaded ---------------------------------------------------------------------------------------------- */
void mq_map_free(mq_map *m)
{
    free(m->ring), free(m->rp), free(m->fi), free(m->ap), free(m->as), free(m->ak), free(m->ab);
    memset(m, 0, sizeof *m);
}

int mq_map_load_ex(mq_map *m, int map, int fills)
{
    memset(m, 0, sizeof *m);
    const mq_map_def *d = mq_def(map);
    if (!d || d->nregions > MQ_MAX_REG) return 0;
    m->map = map;
    m->w = d->w;
    m->h = d->h;
    /* the arcs' points: steps from the point before */
    m->narc = d->narcs;
    m->nap = d->npts;
    m->ap = malloc(sizeof(float) * 2 * (size_t)(d->npts > 0 ? d->npts : 1));
    m->as = malloc(sizeof(int) * (size_t)(d->narcs + 1));
    m->ak = malloc((size_t)(d->narcs > 0 ? d->narcs : 1));
    m->ab = malloc(sizeof(float) * 4 * (size_t)(d->narcs > 0 ? d->narcs : 1));
    if (!m->ap || !m->as || !m->ak || !m->ab) {
        mq_map_free(m);
        return 0;
    }
    int x = 0, y = 0, b = 0, k = 0;
    for (int a = 0; a < d->narcs; a++) {
        m->as[a] = k;
        m->ak[a] = d->arck[a];
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (int j = 0; j < d->arcs[a] && k < d->npts; j++, k++) {
            if (d->pts[b] == -128) {
                const uint8_t *u = (const uint8_t *)d->pts + b + 1;
                x += (int16_t)(u[0] | u[1] << 8);
                y += (int16_t)(u[2] | u[3] << 8);
                b += 5;
            } else {
                x += d->pts[b];
                y += d->pts[b + 1];
                b += 2;
            }
            m->ap[2 * k] = (float)x;
            m->ap[2 * k + 1] = (float)y;
            x0 = fminf(x0, x), y0 = fminf(y0, y), x1 = fmaxf(x1, x), y1 = fmaxf(y1, y);
        }
        m->ab[4 * a] = x0, m->ab[4 * a + 1] = y0, m->ab[4 * a + 2] = x1, m->ab[4 * a + 3] = y1;
    }
    m->as[d->narcs] = k;
    /* the rings: their arcs' points in turn (an arc's last point is the next one's first) */
    int nr = 0, np = 0;
    for (int i = 0; i < d->nring;) {
        int n = d->rings[i + 2];
        nr++;
        for (int j = 0; j < n; j++) {
            int r = d->rings[i + 3 + j], a = r < 0 ? -1 - r : r;
            if (a < d->narcs) np += m->as[a + 1] - m->as[a];
        }
        i += 3 + n;
    }
    m->ring = calloc((size_t)(nr > 0 ? nr : 1), sizeof(mq_ring));
    m->rp = malloc(sizeof(float) * 2 * (size_t)(np > 0 ? np : 1));
    m->fi = malloc(sizeof(int) * 3 * (size_t)(fills && np > 0 ? np : 1));
    if (!m->ring || !m->rp || !m->fi) {
        mq_map_free(m);
        return 0;
    }
    m->nreg = d->nregions;
    for (int r = 0; r < m->nreg; r++) {
        const mq_region_def *R = &d->regions[r];
        mq_reg *g = &m->reg[r];
        g->code = R->code;
        g->name = R->name;
        g->kind = R->kind;
        g->tone = R->tone;
        g->flags = R->flags;
        g->lx = R->lx;
        g->ly = R->ly;
        g->area = R->area;
        g->ring0 = -1;
        g->x0 = g->y0 = 1e9f;
        g->x1 = g->y1 = -1e9f;
    }
    for (int i = 0; i < d->nring;) {
        int reg = d->rings[i], outer = d->rings[i + 1], n = d->rings[i + 2];
        mq_ring *R = &m->ring[m->nring];
        R->first = m->nrp;
        R->region = reg;
        R->outer = outer;
        for (int j = 0; j < n; j++) {
            int r = d->rings[i + 3 + j], a = r < 0 ? -1 - r : r;
            if (a >= d->narcs) continue;
            int s = m->as[a], e = m->as[a + 1], cnt = e - s;
            /* the arc whole but its last point (the next arc's first); a ring of one closed arc: all its points */
            int take = n == 1 && cnt >= 3 && !(m->ap[2 * s] == m->ap[2 * (e - 1)] && m->ap[2 * s + 1] == m->ap[2 * (e - 1) + 1])
                           ? cnt : cnt - 1;
            for (int t = 0; t < take; t++) {
                int q = r < 0 ? e - 1 - t : s + t;
                m->rp[2 * m->nrp] = m->ap[2 * q];
                m->rp[2 * m->nrp + 1] = m->ap[2 * q + 1];
                m->nrp++;
            }
        }
        R->count = m->nrp - R->first;
        if (reg >= 0 && reg < m->nreg) {
            mq_reg *g = &m->reg[reg];
            if (g->ring0 < 0) g->ring0 = m->nring;
            g->nring = m->nring + 1 - g->ring0;
            for (int t = R->first; t < R->first + R->count; t++) {
                g->x0 = fminf(g->x0, m->rp[2 * t]), g->x1 = fmaxf(g->x1, m->rp[2 * t]);
                g->y0 = fminf(g->y0, m->rp[2 * t + 1]), g->y1 = fmaxf(g->y1, m->rp[2 * t + 1]);
            }
        }
        m->nring++;
        i += 3 + n;
    }
    /* the fills: each region's outer rings cut into triangles (holes are filled by the regions inside them, drawn
     * after: the biggest region first) */
    for (int r = 0; r < m->nreg; r++) {
        mq_reg *g = &m->reg[r];
        g->i0 = m->nfi;
        for (int j = 0; g->ring0 >= 0 && j < g->nring; j++) {
            mq_ring *R = &m->ring[g->ring0 + j];
            if (fills && R->outer && R->count >= 3) m->nfi += mq_earclip(m->rp, R->first, R->count, m->fi + m->nfi);
        }
        g->ni = m->nfi - g->i0;
        if (g->ring0 < 0) {                     /* a speck: no outline, its marker only */
            g->ring0 = g->nring = 0;
            g->x0 = g->x1 = g->lx;
            g->y0 = g->y1 = g->ly;
        }
        m->order[r] = r;
    }
    for (int i = 1; i < m->nreg; i++)           /* the biggest first */
        for (int j = i; j > 0 && m->reg[m->order[j]].area > m->reg[m->order[j - 1]].area; j--) {
            int t = m->order[j];
            m->order[j] = m->order[j - 1];
            m->order[j - 1] = t;
        }
    return 1;
}

int mq_find(const mq_map *m, const char *code)
{
    for (int r = 0; r < m->nreg; r++)
        if (!strcmp(m->reg[r].code, code)) return r;
    return -1;
}

int mq_inside(const mq_map *m, int region, float x, float y)
{
    const mq_reg *g = &m->reg[region];
    if (x < g->x0 || x > g->x1 || y < g->y0 || y > g->y1) return 0;
    int c = 0;
    for (int j = 0; j < g->nring; j++) {
        const mq_ring *R = &m->ring[g->ring0 + j];
        const float *p = m->rp + 2 * R->first;
        for (int i = 0, k = R->count - 1; i < R->count; k = i++) {
            float yi = p[2 * i + 1], yk = p[2 * k + 1];
            if ((yi > y) != (yk > y) && x < p[2 * k] + (p[2 * i] - p[2 * k]) * (y - yk) / (yi - yk)) c = !c;
        }
    }
    return c;
}

static float seg_d2(float px, float py, float ax, float ay, float bx, float by)
{
    float dx = bx - ax, dy = by - ay, L = dx * dx + dy * dy, t = L > 0 ? ((px - ax) * dx + (py - ay) * dy) / L : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    float qx = ax + t * dx - px, qy = ay + t * dy - py;
    return qx * qx + qy * qy;
}

int mq_hit(const mq_map *m, float x, float y, float slack)
{
    const mq_map_def *d = mq_def(m->map);
    int framed = 0;                             /* the point is in an inset's frame: only its regions */
    for (int f = 0; d && f < d->nframes; f++)
        if (x >= d->frames[4 * f] && x <= d->frames[4 * f + 2] && y >= d->frames[4 * f + 1] && y <= d->frames[4 * f + 3])
            framed = 1;
    int best = -1;
    for (int r = 0; r < m->nreg; r++) {
        if (framed != !!(m->reg[r].flags & MQ_IN_INSET)) continue;
        if (mq_inside(m, r, x, y) && (best < 0 || m->reg[r].area < m->reg[best].area)) best = r;
    }
    if (best >= 0 || slack <= 0) return best;
    float bd = slack * slack;
    for (int r = 0; r < m->nreg; r++) {
        const mq_reg *g = &m->reg[r];
        if (framed != !!(g->flags & MQ_IN_INSET)) continue;
        if (x < g->x0 - slack || x > g->x1 + slack || y < g->y0 - slack || y > g->y1 + slack) continue;
        for (int j = 0; j < g->nring; j++) {
            const mq_ring *R = &m->ring[g->ring0 + j];
            const float *p = m->rp + 2 * R->first;
            for (int i = 0, k = R->count - 1; i < R->count; k = i++) {
                float e = seg_d2(x, y, p[2 * k], p[2 * k + 1], p[2 * i], p[2 * i + 1]);
                if (e < bd) bd = e, best = r;
            }
        }
    }
    return best;
}

/* ---- a map painted into a picture -------------------------------------------------------------------------------
 * Each region's rings filled even-odd, four rows of samples a pixel with exact spans across, blended in its colour. */
#define SUB 4
static int cmpf(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}

void mq_paint(const mq_map *m, float s, float ox, float oy, int w, int h, uint8_t *rgba, mq_colour colour, void *ctx)
{
    float *cov = malloc(sizeof(float) * (size_t)w), *xs = NULL;
    int cap = 0;
    if (!cov) return;
    for (int o = 0; o < m->nreg; o++) {
        int r = m->order[o];
        const mq_reg *g = &m->reg[r];
        uint8_t c[4];
        colour(m, r, ctx, c);
        if (!c[3] || !g->nring) continue;
        int Y0 = (int)floorf(oy + g->y0 * s), Y1 = (int)ceilf(oy + g->y1 * s);
        int X0 = (int)floorf(ox + g->x0 * s), X1 = (int)ceilf(ox + g->x1 * s);
        if (Y1 < 0 || Y0 >= h || X1 < 0 || X0 >= w) continue;
        Y0 = Y0 < 0 ? 0 : Y0, Y1 = Y1 > h ? h : Y1, X0 = X0 < 0 ? 0 : X0, X1 = X1 > w ? w : X1;
        int ne = 0;
        for (int j = 0; j < g->nring; j++) ne += m->ring[g->ring0 + j].count;
        if (ne > cap) {
            float *t = realloc(xs, sizeof(float) * (size_t)ne);
            if (!t) break;
            xs = t;
            cap = ne;
        }
        for (int py = Y0; py < Y1; py++) {
            for (int px = X0; px < X1; px++) cov[px] = 0;
            for (int sub = 0; sub < SUB; sub++) {
                float yy = py + (sub + 0.5f) / SUB;
                int nx = 0;
                for (int j = 0; j < g->nring; j++) {
                    const mq_ring *R = &m->ring[g->ring0 + j];
                    const float *p = m->rp + 2 * R->first;
                    for (int i = 0, k = R->count - 1; i < R->count; k = i++) {
                        float ya = oy + p[2 * k + 1] * s, yb = oy + p[2 * i + 1] * s;
                        if ((ya > yy) != (yb > yy)) {
                            float xa = ox + p[2 * k] * s, xb = ox + p[2 * i] * s;
                            xs[nx++] = xa + (xb - xa) * (yy - ya) / (yb - ya);
                        }
                    }
                }
                qsort(xs, (size_t)nx, sizeof(float), cmpf);
                for (int q = 0; q + 1 < nx; q += 2) {
                    float a = fmaxf(xs[q], (float)X0), b = fminf(xs[q + 1], (float)X1);
                    if (b <= a) continue;
                    int ia = (int)a, ib = (int)b;
                    if (ia == ib) {
                        cov[ia] += (b - a) / SUB;
                        continue;
                    }
                    cov[ia] += (ia + 1 - a) / SUB;
                    for (int t = ia + 1; t < ib; t++) cov[t] += 1.0f / SUB;
                    if (ib < X1) cov[ib] += (b - ib) / SUB;
                }
            }
            uint8_t *row = rgba + 4 * (size_t)py * w;
            for (int px = X0; px < X1; px++) {
                float a = fminf(cov[px], 1) * c[3] / 255.0f;
                if (a <= 0) continue;
                uint8_t *d = row + 4 * px;
                float da = d[3] / 255.0f, oa = a + da * (1 - a);
                for (int ch = 0; ch < 3; ch++)
                    d[ch] = (uint8_t)((c[ch] * a + d[ch] * da * (1 - a)) / (oa > 0 ? oa : 1) + 0.5f);
                d[3] = (uint8_t)(oa * 255 + 0.5f);
            }
        }
    }
    free(cov);
    free(xs);
}

/* ---- the quiz ---------------------------------------------------------------------------------------------------- */
int mq_set(const mq_map *m, const char *codes, int16_t *out)
{
    int n = 0;
    if (!codes || !strcmp(codes, "*")) codes = mq_map_set(m->map);
    if (!strcmp(codes, "*")) {                  /* a country's map: every state asked */
        for (int r = 0; r < m->nreg; r++)
            if (m->reg[r].kind == MQ_ASK) out[n++] = (int16_t)r;
        return n;
    }
    char code[32];
    for (const char *s = codes; *s;) {
        while (*s == ' ') s++;
        int k = 0;
        while (*s && *s != ' ' && k < 31) code[k++] = *s++;
        code[k] = 0;
        if (!k) break;
        int r = mq_find(m, code);
        if (r < 0 || m->reg[r].kind != MQ_ASK || n >= MQ_MAX_REG) return 0;
        out[n++] = (int16_t)r;
    }
    return n;
}

static uint32_t rnd(uint32_t *s)
{
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

void mq_quiz_start(mq_quiz *q, const mq_map *m, const int16_t *regions, int n, uint32_t seed)
{
    memset(q, 0, sizeof *q);
    q->map = m->map;
    q->n = n > MQ_MAX_REG ? MQ_MAX_REG : n;
    memcpy(q->ask, regions, sizeof(int16_t) * (size_t)q->n);
    uint32_t s = seed ? seed : 0x9e3779b9u;
    for (int i = q->n - 1; i > 0; i--) {        /* the order asked: shuffled */
        int j = (int)(rnd(&s) % (uint32_t)(i + 1));
        int16_t t = q->ask[i];
        q->ask[i] = q->ask[j];
        q->ask[j] = t;
    }
    for (int i = 0; i < q->n; i++) q->in[q->ask[i]] = 1;
}

int mq_target(const mq_quiz *q) { return q->cur < q->n ? q->ask[q->cur] : -1; }
int mq_done(const mq_quiz *q) { return q->cur >= q->n; }

int mq_tap(mq_quiz *q, const mq_map *m, int region)
{
    int t = mq_target(q);
    if (t < 0 || region < 0 || region >= m->nreg) return MQ_NOTHING;
    if (m->reg[region].kind == MQ_DISPUTED) return MQ_NOTHING;
    if (region == t) {
        q->state[t] = MQ_FOUND;
        q->cur++;
        q->tries = 0;
        return MQ_RIGHT;
    }
    if (!q->in[region]) return MQ_OUT;
    q->misses++;
    if (++q->tries < MQ_TRIES) return MQ_WRONG;
    q->state[t] = MQ_MISSED;
    q->given++;
    q->cur++;
    q->tries = 0;
    return MQ_GIVEN;
}

uint32_t mq_time_limit(int n) { return 15000u + 8000u * (uint32_t)n; }

int mq_stars(const mq_quiz *q, uint32_t ms)
{
    if (q->misses <= q->n / 10 && ms <= mq_time_limit(q->n)) return 3;
    if (q->misses <= q->n / 4 + 1) return 2;
    return 1;
}

int mq_map_load(mq_map *m, int map) { return mq_map_load_ex(m, map, 1); }
