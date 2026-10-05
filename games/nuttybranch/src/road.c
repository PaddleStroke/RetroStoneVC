/*
 * Nutty Branch: the pseudo-3D projection of the branch (DESIGN.md "Rendering"). Integer maths only.
 *
 * The course ahead of the camera is a list of 16-unit segments (a curve and a height each). Their boundary points
 * are projected (y = HORIZON + (camh - h) F / z, x = 160 + lateral F / z); the screen lines are then filled from the
 * bottom up, each line taking the z of its place between two points (1/z interpolated: perspective-correct), a
 * segment lower than the highest line already drawn being hidden behind a crest (Lou's Pseudo 3d Page). Each line
 * gets one texture row, a scale (texels per pixel = z / F) and the screen x of the branch centre: draw.c sends them
 * to the affine layer line by line.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
 */
#include "game.h"
#include <string.h>

#define MAX_SPANS 8

/* a fork's two branches are apart (more than a branch width between their centres) */
static int sep_far_enough(int32_t s_q8, int32_t f_s)
{
    return (int64_t)(s_q8 - f_s * 256) * FORK_SPREAD_256 >> 8 > 2 * BRANCH_R * 256;
}

typedef struct span { int32_t s0, s1; int side; } span;

static int32_t seg_floor(int32_t s) { return s >= 0 ? s / SEG_LEN : -((-s + SEG_LEN - 1) / SEG_LEN); }

/* one fork's lateral offset of a branch at s (Q8 units): the live branch drifts to its side, the dead one to the other */
static int32_t one_fork(int32_t f_s, int32_t f_len, int side, int32_t s_q8, int branch)
{
    int32_t s = s_q8 >> 8;
    if (s < f_s) return 0;
    int64_t sep = s < f_s + f_len ? (int64_t)(s_q8 - f_s * 256) * FORK_SPREAD_256 >> 8 : (int64_t)f_len * FORK_SPREAD_256;
    int live = -side;
    if (branch && s < f_s + f_len + FORK_TAIL) return (int32_t)(-live * sep / 2);
    return (int32_t)(live * sep / 2);
}

int32_t fork_offset(const world *w, int32_t s_q8, int branch)
{
    int32_t off = 0;
    for (int i = 0; i < w->ob_count; i++) {
        const obj *o = ob_get(w, i);
        if (o->kind == OB_FORK) off += one_fork(o->s, o->len, o->side, s_q8, branch);
    }
    return off;
}

static int32_t foff(const road_view *v, int32_t s_q8, int branch)
{
    int32_t off = 0;
    for (int i = 0; i < v->nfk; i++) off += one_fork(v->fk_s[i], v->fk_len[i], v->fk_side[i], s_q8, branch);
    return off;
}

/* the camera frame: the lateral position (Q8 units) of the main branch's centre at s, relative to the camera's
 * heading line, from the segment headings; X(n0) = 0 */
static int32_t lateral(const world *w, int32_t n0, int32_t s_q8)
{
    int32_t n = seg_floor(s_q8 >> 8);
    int64_t x = 0;
    for (int32_t i = n0; i < n; i++) x += w->head[i & (SEG_RING - 1)] - w->cam_head;
    int32_t f = s_q8 - n * SEG_LEN * 256;
    x += (int64_t)(w->head[n & (SEG_RING - 1)] - w->cam_head) * f / (SEG_LEN * 256);
    return (int32_t)x;
}

static void proj(const world *w, const road_view *v, int32_t s_q8, int32_t lat, int32_t h, int32_t *px, int32_t *py)
{
    int32_t z = s_q8 - w->cam_s;
    *px = 160 * 256 + (int32_t)((int64_t)(lat - v->xref) * FOCAL * 256 / z);
    *py = HORIZON_Y * 256 + (int32_t)((int64_t)(w->cam_h - h) * FOCAL * 256 / z);
}

void road_project(const world *w, road_view *v)
{
    memset(v->ln, 0, sizeof v->ln);
    for (int y = 0; y < RS_SCREEN_H; y++) v->ln[y].row = ROW_NONE;
    int32_t n0 = seg_floor(w->cam_s >> 8);
    v->nfk = 0;
    for (int i = 0; i < w->ob_count && v->nfk < 4; i++) {
        const obj *o = ob_get(w, i);
        if (o->kind != OB_FORK) continue;
        v->fk_s[v->nfk] = o->s;
        v->fk_len[v->nfk] = o->len;
        v->fk_side[v->nfk++] = o->side;
    }
    /* the camera's lateral reference: the leader's branch at the leader */
    const squirrel *lq = &w->q[w->leader];
    v->xref = lateral(w, n0, w->lead_s) + foff(v, w->lead_s, lq->branch);
    /* the points: the near plane, then every segment boundary (lateral X integrated along the way) */
    int np = 0;
    int32_t last_seg = w->g.seg - 1;
    int32_t s_near = w->cam_s + CAM_NEAR * 256;
    v->ps[0] = s_near;
    v->pz[0] = s_near - w->cam_s;
    proj(w, v, s_near, lateral(w, n0, s_near) + foff(v, s_near, 0), seg_height(w, s_near), &v->px[0], &v->py[0]);
    np = 1;
    int64_t xb = lateral(w, n0, (n0 + 1) * SEG_LEN * 256);
    for (int32_t n = n0 + 1; np < ROAD_PTS && n <= last_seg; n++) {
        int32_t s_q8 = n * SEG_LEN * 256, lat = (int32_t)xb;
        xb += w->head[n & (SEG_RING - 1)] - w->cam_head;
        if (s_q8 <= s_near) continue;
        v->ps[np] = s_q8;
        v->pz[np] = s_q8 - w->cam_s;
        proj(w, v, s_q8, lat + foff(v, s_q8, 0), seg_height(w, s_q8), &v->px[np], &v->py[np]);
        np++;
    }
    v->npts = np;
    /* the gaps and forks in view */
    span gaps[MAX_SPANS], forks[MAX_SPANS], rains[MAX_SPANS];
    int ng = 0, nf = 0, nr = 0;
    static const int16_t var_cx[TEX_VARIANTS] = TEX_VAR_CX;
    int32_t cs = w->cam_s >> 8;
    for (int i = 0; i < w->ob_count; i++) {
        const obj *o = ob_get(w, i);
        if (o->s > cs + DRAW_SEGS * SEG_LEN + 64) break;
        if (o->kind == OB_GAP && o->s + o->len + JAG_BACK >= cs && ng < MAX_SPANS)
            gaps[ng++] = (span){o->s, o->s + o->len, 0};
        if (o->kind == OB_FORK && o->s + o->len >= cs && nf < MAX_SPANS) forks[nf++] = (span){o->s, o->s + o->len, o->side};
        if (o->kind == OB_RAIN && o->s + o->len >= cs && nr < MAX_SPANS) rains[nr++] = (span){o->s, o->s + o->len, 0};
    }
    /* fill the lines, near to far */
    int32_t maxy = RS_SCREEN_H * 256;
    v->top = RS_SCREEN_H;
    for (int k = 0; k + 1 < np; k++) {
        v->pclip[k] = maxy;
        int32_t yn = v->py[k], yf = v->py[k + 1];
        if (yf >= maxy || yf >= yn) continue;        /* behind a crest, or facing away */
        int32_t lo = yn < maxy ? yn : maxy;
        int y0 = (yf - 128 + 255) >> 8, y1 = ((lo - 128 + 255) >> 8) - 1;
        if (y0 < 0) y0 = 0;
        if (y1 > RS_SCREEN_H - 1) y1 = RS_SCREEN_H - 1;
        int64_t izn = ((int64_t)1 << 40) / v->pz[k], izf = ((int64_t)1 << 40) / v->pz[k + 1];
        for (int y = y1; y >= y0; y--) {
            int32_t yc = y * 256 + 128;
            int64_t t = (int64_t)(yn - yc) * 65536 / (yn - yf);
            int64_t iz = izn + (izf - izn) * t / 65536;
            int32_t z = (int32_t)(((int64_t)1 << 40) / iz);
            int32_t xc = v->px[k] + (int32_t)((int64_t)(v->px[k + 1] - v->px[k]) * t / 65536);
            road_line *L = &v->ln[y];
            int32_t s_q8 = w->cam_s + z, s = s_q8 >> 8;
            L->z = z;
            L->s = s;
            L->a = (int16_t)(z / FOCAL);
            L->xc = (int16_t)((xc + 128) >> 8);
            /* the bark: one of the patches per metre (a hash of the metre), the row of the metre */
            uint32_t mh = (uint32_t)(s >> 6) * 2654435761u;
            int pv = (int)((mh >> 16) % 8);               /* the plain patch 6 metres in 8, a burl, a scar */
            L->tcx = var_cx[pv < 6 ? 0 : pv - 5];
            L->row = (int16_t)(ROW_BARK + (s & 63));
            for (int i = 0; i < nr; i++)
                if (s >= rains[i].s0 && s < rains[i].s1) L->wet = 1;
            L->ring = (uint8_t)(((s >= 0 ? s : -s) / RING_LEN) & 3);
            L->haze = (uint8_t)(z < 640 * 256 ? 0 : z < 1150 * 256 ? 1 : z < 1750 * 256 ? 2 : 3);
            for (int i = 0; i < ng; i++) {             /* a gap: the two torn ends, nothing between */
                int32_t n0s = gaps[i].s0 - JAG_BACK, f0s = gaps[i].s1 - JAG_OUT;
                if (s >= n0s && s < gaps[i].s0 + JAG_OUT) {
                    L->row = (int16_t)(ROW_JAGGED + (s - n0s) * 8 / (JAG_BACK + JAG_OUT));
                    L->tcx = TEX_CX;
                } else if (s >= f0s && s < gaps[i].s1 + JAG_BACK) {
                    L->row = (int16_t)(ROW_GRAIN + (s - f0s) * 8 / (JAG_BACK + JAG_OUT));
                    L->tcx = TEX_CX;
                } else if (s >= gaps[i].s0 && s < gaps[i].s1) {
                    L->row = ROW_NONE;
                }
            }
            for (int i = 0; i < nf && L->row != ROW_NONE; i++) {
                if (s < forks[i].s0 || s >= forks[i].s1) continue;
                int32_t sep = (int32_t)((int64_t)(s_q8 - forks[i].s0 * 256) * FORK_SPREAD_256 >> 8);   /* Q8 units */
                if (sep < 8 * 256) break;
                int live = -forks[i].side, j = (sep / 256 + FORK_STEP / 2) / FORK_STEP - 1;
                if (j < 0) j = 0;
                if (j > FORK_STEPS - 1) j = FORK_STEPS - 1;
                int32_t sq = FORK_STEP * (j + 1), a = z / FOCAL;
                /* the live branch is at xc, the dead one a separation away; the branch the camera follows lands
                 * exactly, the other within 8 texels (the separations come in 16-texel steps) */
                int32_t dead = xc - (int32_t)((int64_t)live * sep * FOCAL / z);
                int32_t fx = lq->branch ? dead : xc, ox = lq->branch ? xc : dead;
                if (z < (CAM_BACK - 8) * 256 && sep_far_enough(s_q8, forks[i].s0)) {
                    /* near the camera (behind the leader) the other branch's steps would show as slabs: it has swung
                     * away by then, only the followed one is drawn */
                    L->xc = (int16_t)((fx + 128) >> 8);
                    break;
                }
                int32_t half = (int32_t)((int64_t)sq / 2 * 256 * 256 / (a > 0 ? a : 1));     /* Q8 px */
                int32_t mid = fx < ox ? fx + half : fx - half;
                L->xc = (int16_t)((mid + 128) >> 8);
                L->row = (int16_t)(ROW_FORK + j * 8 + (s & 7));
                L->tcx = TEX_CX;
                break;
            }
            if (L->row != ROW_NONE && y < v->top) v->top = y;
        }
        if (yf < maxy) maxy = yf;
    }
    if (np > 0) v->pclip[np - 1] = maxy;
    /* a gap's far side: its broken face, a dark disc of torn wood hanging under the far teeth, on the empty lines (the
     * near side's bark, in front, keeps its lines). The face has the far edge's scale and centre on every line, its
     * texture row the line's place down the face. */
    for (int i = 0; i < ng && np >= 2; i++) {
        int32_t s_q8 = (gaps[i].s1 - JAG_OUT) * 256, z = s_q8 - w->cam_s;
        if (z < CAM_NEAR * 256 || s_q8 >= v->ps[np - 1]) continue;
        int k = s_q8 < v->ps[1] ? 0 : 1 + (int)(((s_q8 - v->ps[1]) >> 8) / SEG_LEN);
        if (k > np - 2) k = np - 2;
        int32_t lat = lateral(w, n0, s_q8) + foff(v, s_q8, 0), hg = seg_height(w, s_q8), px, pt, pb;
        proj(w, v, s_q8, lat, hg, &px, &pt);
        if (pt > v->pclip[k] + 2 * 256) continue;   /* behind a crest */
        proj(w, v, s_q8, lat, hg - FACE_DEPTH * 256, &px, &pb);
        if (pb <= pt) continue;
        int y0 = (pt + 128) >> 8, y1 = (pb - 128) >> 8;
        if (y0 < 0) y0 = 0;
        if (y1 > RS_SCREEN_H - 1) y1 = RS_SCREEN_H - 1;
        int32_t s1 = gaps[i].s1;
        for (int y = y0; y <= y1; y++) {
            road_line *L = &v->ln[y];
            if (L->row != ROW_NONE) continue;
            int r = (int)((int64_t)(y * 256 + 128 - pt) * FACE_ROWS / (pb - pt));
            L->row = (int16_t)(ROW_FACE + (r < 0 ? 0 : r > FACE_ROWS - 1 ? FACE_ROWS - 1 : r));
            L->tcx = FACE_CX;
            L->a = (int16_t)(z / FOCAL);
            L->xc = (int16_t)((px + 128) >> 8);
            L->z = z;
            L->s = s1;
            L->ring = (uint8_t)(((s1 >= 0 ? s1 : -s1) / RING_LEN) & 3);
            L->haze = (uint8_t)(z < 640 * 256 ? 0 : z < 1150 * 256 ? 1 : z < 1750 * 256 ? 2 : 3);
            L->wet = 0;
        }
    }
}

int road_free(const world *w, const road_view *v, int32_t s, int32_t lat, int32_t h, int *sx, int *sy, int *scale)
{
    int32_t z = s - w->cam_s;
    if (v->npts < 2 || z < CAM_NEAR * 256 || s >= v->ps[v->npts - 1]) return 0;
    int32_t n0 = seg_floor(w->cam_s >> 8), px, py;
    proj(w, v, s, lateral(w, n0, s) + foff(v, s, 0) + lat, seg_height(w, s) + h, &px, &py);
    *sx = (px + 128) >> 8;
    *sy = (py + 128) >> 8;
    *scale = (int)((int64_t)FOCAL * 256 * 256 / z);
    return 1;
}

int road_point(const world *w, const road_view *v, int32_t s, int32_t ang, int32_t h, int branch, int *sx, int *sy,
               int *scale, int *hidden)
{
    int32_t z = s - w->cam_s;
    *hidden = 0;
    if (v->npts < 2 || z < CAM_NEAR * 256 || s >= v->ps[v->npts - 1]) return 0;
    int k = 0;
    if (s >= v->ps[1]) k = 1 + (int)(((s - v->ps[1]) >> 8) / SEG_LEN);
    if (k > v->npts - 2) k = v->npts - 2;
    int32_t n0 = seg_floor(w->cam_s >> 8);
    int32_t lat = lateral(w, n0, s) + foff(v, s, branch);
    int32_t r = BRANCH_R * 256;
    lat += (int32_t)((int64_t)r * sin_q14(ang) >> 14);
    int32_t drop = (int32_t)(((int64_t)r * (16384 - cos_q14(ang)) >> 14) * DROP_K_256 / 256);
    int32_t px, py;
    proj(w, v, s, lat, seg_height(w, s) + h - drop, &px, &py);
    int32_t pyb;
    proj(w, v, s, lat, seg_height(w, s), &px, &pyb);              /* the bark line at s: is it behind a crest? */
    *sx = (px + 128) >> 8;
    *sy = (py + 128) >> 8;
    *scale = (int)((int64_t)FOCAL * 256 * 256 / z);
    if (pyb > v->pclip[k] + 2 * 256) *hidden = 1;
    return 1;
}
