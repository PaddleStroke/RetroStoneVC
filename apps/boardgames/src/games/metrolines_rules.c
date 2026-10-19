/*
 * Metro Lines' rules (metrolines.h): the map's water, the lines' ways, the trains, the passengers and their routes,
 * the weeks' gifts, the edits, the notation, the two planners (a naive one and one that looks ahead), the levels.
 * No SDL, no floating point in the simulation: the same run on every device.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/metrolines.h"
#include "games/puzrand.h"

#define VMAX 80                                 /* a train's top speed (FP a tick: 2 units a second) */
#define ACC 5                                   /* its speeding up and slowing down (FP a tick, a tick) */
#define VMIN 6
#define ACT 5                                   /* a passenger off or on: ticks */
#define ACT_INTER 2
#define DWELL 6                                 /* at a stop at least */
#define ARRIVE 3
#define SAMPLE (MTL_FP / 8)                     /* water looked for this often along a way */

static int iabs(int v) { return v < 0 ? -v : v; }
static int isgn(int v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }

static void event(mtl_state *s, int kind, int a, int b)
{
    mtl_event *e = &s->ev[s->nev++ % MTL_EVENTS];
    e->kind = (uint8_t)kind;
    e->a = (uint8_t)a;
    e->b = (uint8_t)b;
    e->tick = s->tick;
}

/* ---- the map ------------------------------------------------------------------------------------------------------ */
mtl_pt mtl_station_pt(const mtl_state *s, int st)
{
    mtl_pt p = {s->L.st[st].x * MTL_FP, s->L.st[st].y * MTL_FP};
    return p;
}

static int64_t seg_d2_lt(int32_t px, int32_t py, mtl_pt a, mtl_pt b, int64_t r)   /* distance to a..b below r */
{
    int64_t dx = b.x - a.x, dy = b.y - a.y, wx = px - a.x, wy = py - a.y, t = wx * dx + wy * dy, den = dx * dx + dy * dy;
    if (t <= 0 || den == 0) return wx * wx + wy * wy < r * r;
    if (t >= den) {
        int64_t ex = px - b.x, ey = py - b.y;
        return ex * ex + ey * ey < r * r;
    }
    int64_t c = wx * dy - wy * dx;
    /* c^2 / den < r^2, without overflow: c up to 1e8 */
    if (c < 0) c = -c;
    if (c >= (int64_t)3000000000LL) return 0;
    return c * c < r * r * den;
}

int mtl_wet(const mtl_state *s, int32_t x, int32_t y)
{
    for (int r = 0; r < s->L.nriver; r++) {
        int64_t hw = (int64_t)s->L.rhw[r] * (MTL_FP / 4);
        for (int i = 0; i + 1 < s->nrp[r]; i++) {
            mtl_pt a = s->rp[r][i], b = s->rp[r][i + 1];
            if ((x < a.x - hw && x < b.x - hw) || (x > a.x + hw && x > b.x + hw) || (y < a.y - hw && y < b.y - hw) ||
                (y > a.y + hw && y > b.y + hw))
                continue;
            if (seg_d2_lt(x, y, a, b, hw)) return 1;
        }
    }
    for (int k = 0; k < s->L.nlake; k++) {
        int64_t cx = (int64_t)s->L.lx[k] * (MTL_FP / 4), cy = (int64_t)s->L.ly[k] * (MTL_FP / 4);
        int64_t rx = (int64_t)s->L.lrx[k] * (MTL_FP / 4), ry = (int64_t)s->L.lry[k] * (MTL_FP / 4);
        int64_t dx = x - cx, dy = y - cy;
        if (rx <= 0 || ry <= 0 || iabs((int)dx) > rx || iabs((int)dy) > ry) continue;
        if ((dx * ry) * (dx * ry) + (dy * rx) * (dy * rx) < (rx * ry) * (rx * ry)) return 1;
    }
    return 0;
}

int mtl_crossings(const mtl_state *s, const mtl_pt *p, int n)
{
    int wet = 0, count = 0;
    for (int i = 0; i + 1 < n; i++) {
        int32_t dx = p[i + 1].x - p[i].x, dy = p[i + 1].y - p[i].y, len = iabs(dx) > iabs(dy) ? iabs(dx) : iabs(dy);
        int m = len / SAMPLE;
        if (m < 1) m = 1;
        for (int j = 1; j <= m; j++) {
            int w = mtl_wet(s, p[i].x + (int32_t)((int64_t)dx * j / m), p[i].y + (int32_t)((int64_t)dy * j / m));
            if (w && !wet) count++;
            wet = w;
        }
    }
    return count;
}

int32_t mtl_path_len(const mtl_pt *p, int n)
{
    int32_t l = 0;
    for (int i = 0; i + 1 < n; i++) {
        int32_t dx = iabs(p[i + 1].x - p[i].x), dy = iabs(p[i + 1].y - p[i].y);
        l += dx == 0 || dy == 0 ? dx + dy : (int32_t)((int64_t)dx * MTL_DIAG / MTL_FP);
    }
    return l;
}

int mtl_route(const mtl_state *s, int a, int b, int bend, mtl_pt pts[3])
{
    int lo = a < b ? a : b, hi = a < b ? b : a;
    mtl_pt A = mtl_station_pt(s, lo), B = mtl_station_pt(s, hi);
    int dx = (B.x - A.x) / MTL_FP, dy = (B.y - A.y) / MTL_FP, n;
    if (dx == 0 || dy == 0 || iabs(dx) == iabs(dy)) {
        pts[0] = A;
        pts[1] = B;
        n = 2;
    } else {
        int m = iabs(dx) < iabs(dy) ? iabs(dx) : iabs(dy);
        mtl_pt P;
        if (!bend) { P.x = A.x + m * isgn(dx) * MTL_FP; P.y = A.y + m * isgn(dy) * MTL_FP; }
        else { P.x = B.x - m * isgn(dx) * MTL_FP; P.y = B.y - m * isgn(dy) * MTL_FP; }
        pts[0] = A;
        pts[1] = P;
        pts[2] = B;
        n = 3;
    }
    if (a > b) {                                /* from a to b */
        mtl_pt t = pts[0];
        pts[0] = pts[n - 1];
        pts[n - 1] = t;
    }
    return n;
}

static int pair_cross(const mtl_state *s, int a, int b, int bend)
{
    int lo = a < b ? a : b, hi = a < b ? b : a;
    uint8_t v = s->xc[lo][hi];
    return bend ? v >> 4 : v & 15;
}

int mtl_best_bend(const mtl_state *s, int a, int b, int *crossings)
{
    mtl_pt p[3];
    int n = mtl_route(s, a, b, 0, p), c0 = pair_cross(s, a, b, 0), bend = 0, c = c0;
    if (n == 3) {
        int c1 = pair_cross(s, a, b, 1);
        if (c1 < c0) { bend = 1; c = c1; }
    }
    if (crossings) *crossings = c;
    return bend;
}

static int32_t link_len(const mtl_state *s, int a, int b, int bend)
{
    mtl_pt p[3];
    int n = mtl_route(s, a, b, bend, p);
    return mtl_path_len(p, n);
}

/* ---- the lines ---------------------------------------------------------------------------------------------------- */
static int links(const mtl_line *l) { return l->n < 2 ? 0 : l->loop ? l->n : l->n - 1; }

static int line_tunnels(const mtl_line *l)
{
    int t = 0;
    for (int k = 0; k < links(l); k++) t += l->cross[k];
    return t;
}

int mtl_tunnels_used(const mtl_state *s)
{
    int t = 0;
    for (int i = 0; i < MTL_MAXL; i++) t += line_tunnels(&s->l[i]);
    return t;
}

int mtl_lines_used(const mtl_state *s)
{
    int n = 0;
    for (int i = 0; i < MTL_MAXL; i++) n += s->l[i].n > 0;
    return n;
}

int mtl_free_line(const mtl_state *s)
{
    if (mtl_lines_used(s) >= s->lines) return -1;
    for (int i = 0; i < MTL_MAXL; i++)
        if (!s->l[i].n) return i;
    return -1;
}

int mtl_on_line(const mtl_state *s, int l, int st)
{
    for (int k = 0; k < s->l[l].n; k++)
        if (s->l[l].stop[k] == st) return k;
    return -1;
}

int mtl_room(const mtl_state *s, int st) { return s->s[st].inter ? MTL_ROOM_INTER : MTL_ROOM; }
int mtl_capacity(const mtl_state *s, int l) { return s->t[l].cars * MTL_CAR; }

static void line_fill(const mtl_state *s, mtl_line *l)   /* each link's way, crossings, length */
{
    for (int k = 0; k < links(l); k++) {
        int a = l->stop[k], b = l->stop[(k + 1) % l->n], c;
        l->bend[k] = (uint8_t)mtl_best_bend(s, a, b, &c);
        l->cross[k] = (uint8_t)c;
        l->len[k] = link_len(s, a, b, l->bend[k]);
    }
}

static void routes(mtl_state *s)                /* the stops to each shape, over the lines */
{
    static const int MAXE = MTL_MAXL * MTL_STOPS * 2;
    uint8_t ea[MTL_MAXL * MTL_STOPS * 2], eb[MTL_MAXL * MTL_STOPS * 2];
    int ne = 0;
    for (int i = 0; i < MTL_MAXL; i++) {
        const mtl_line *l = &s->l[i];
        for (int k = 0; k < links(l) && ne + 2 <= MAXE; k++) {
            int a = l->stop[k], b = l->stop[(k + 1) % l->n];
            ea[ne] = (uint8_t)a, eb[ne++] = (uint8_t)b;
            if (!l->loop) ea[ne] = (uint8_t)b, eb[ne++] = (uint8_t)a;
        }
    }
    for (int sh = 0; sh < MTL_SHAPES; sh++) {
        uint8_t *d = s->dist[sh];
        int q[MTL_MAXS], h = 0, t = 0;
        for (int i = 0; i < MTL_MAXS; i++) {
            d[i] = MTL_INF;
            if (i < s->nst && s->L.st[i].shape == sh) { d[i] = 0; q[t++] = i; }
        }
        while (h < t) {
            int u = q[h++];
            for (int e = 0; e < ne; e++)
                if (eb[e] == u && d[ea[e]] == MTL_INF) {   /* ea -> u: ea one stop further */
                    d[ea[e]] = (uint8_t)(d[u] + 1);
                    q[t++] = ea[e];
                }
        }
    }
}

static void drop_riders(mtl_state *s, mtl_train *t, int st)   /* a train taken away: its passengers left at st */
{
    for (int i = 0; i < t->nride; i++) {
        mtl_station *S = &s->s[st];
        if (S->nwait < MTL_WAIT) S->wait[S->nwait++] = t->ride[i];
    }
    t->nride = 0;
}

/* the train of a line just changed from `was` to its new stops: on the same link if it is still there, else at the
 * stop it was going to, else the one it left, else the first */
static void remap(mtl_state *s, int li, const mtl_line *was)
{
    mtl_line *l = &s->l[li];
    mtl_train *t = &s->t[li];
    if (!l->n) return;
    if (!was->n) {
        int cars = t->cars > 0 ? t->cars : 1;
        memset(t, 0, sizeof *t);
        t->cars = cars;
        t->stopped = 1;
        t->dir = 1;
        t->timer = ARRIVE;
        return;
    }
    if (t->stopped) {
        int k = mtl_on_line(s, li, was->stop[t->at]);
        t->at = k >= 0 ? k : 0;
        if (k < 0) t->timer = ARRIVE;
        if (!l->loop && t->at == l->n - 1) t->dir = -1;
        if (!l->loop && t->at == 0) t->dir = 1;
        return;
    }
    int from = t->dir > 0 ? was->stop[t->k] : was->stop[(t->k + 1) % was->n];
    int to = t->dir > 0 ? was->stop[(t->k + 1) % was->n] : was->stop[t->k];
    int32_t old_len = was->len[t->k];
    int i1 = mtl_on_line(s, li, from), i2 = mtl_on_line(s, li, to);
    if (i1 >= 0 && i2 >= 0) {
        int d;
        if (l->loop) d = 1;
        else d = i2 > i1 ? 1 : -1;
        if (l->loop && (i1 + 1) % l->n != i2) {   /* a loop one way: the other way round, back where it left */
            t->stopped = 1;
            t->at = i1;
            t->timer = ARRIVE;
            t->v = 0;
            return;
        }
        int nx = i1 + d;
        if (!l->loop && (nx < 0 || nx >= l->n)) { t->stopped = 1; t->at = i1; t->timer = ARRIVE; t->v = 0; return; }
        t->dir = d;
        t->k = d > 0 ? i1 : nx;
        int32_t nl = l->len[t->k];
        t->pos = old_len > 0 ? (int32_t)((int64_t)t->pos * nl / old_len) : 0;
        if (t->pos >= nl) t->pos = nl - 1;
        if (t->pos < 0) t->pos = 0;
        return;
    }
    t->stopped = 1;
    t->timer = ARRIVE;
    t->v = 0;
    t->at = i2 >= 0 ? i2 : i1 >= 0 ? i1 : 0;
    if (!l->loop && t->at == l->n - 1) t->dir = -1;
    if (!l->loop && t->at == 0) t->dir = 1;
}

/* a line laid out as `nl` would be (all checked: stations shown, each once, tunnels enough): 1 done */
static int commit(mtl_state *s, int li, mtl_line *nl)
{
    if (nl->n == 1) nl->n = 0;
    if (nl->n) {
        if (nl->n > MTL_STOPS || (nl->loop && nl->n < 3)) return 0;
        for (int k = 0; k < nl->n; k++) {
            if (nl->stop[k] >= s->nst) return 0;
            for (int j = 0; j < k; j++)
                if (nl->stop[j] == nl->stop[k]) return 0;
        }
        line_fill(s, nl);
    }
    int others = mtl_tunnels_used(s) - line_tunnels(&s->l[li]);
    if (nl->n && others + line_tunnels(nl) > s->tunnels) return 0;
    mtl_line was = s->l[li];
    if (!nl->n && was.n) {                      /* taken away: the passengers off where the train is */
        mtl_train *t = &s->t[li];
        int st = t->stopped ? was.stop[t->at] : t->dir > 0 ? was.stop[t->k] : was.stop[(t->k + 1) % was.n];
        drop_riders(s, t, st);
    }
    s->l[li] = *nl;
    remap(s, li, &was);
    routes(s);
    s->edits++;
    return 1;
}

int mtl_new_line(mtl_state *s, const int *stops, int n)
{
    int li = mtl_free_line(s);
    if (li < 0 || n < 2 || s->over || s->won) return 0;
    mtl_line nl;
    memset(&nl, 0, sizeof nl);
    if (n >= 4 && stops[n - 1] == stops[0]) { nl.loop = 1; n--; }
    if (n > MTL_STOPS) return 0;
    for (int k = 0; k < n; k++) {
        if (stops[k] < 0 || stops[k] >= s->nst) return 0;
        nl.stop[k] = (uint8_t)stops[k];
    }
    nl.n = n;
    return commit(s, li, &nl);
}

int mtl_extend(mtl_state *s, int li, int tail, int st)
{
    if (li < 0 || li >= MTL_MAXL || st < 0 || st >= s->nst || s->over || s->won) return 0;
    mtl_line nl = s->l[li];
    if (nl.n < 2 || nl.loop) return 0;
    int other = tail ? nl.stop[0] : nl.stop[nl.n - 1];
    if (st == other && nl.n >= 3) { nl.loop = 1; return commit(s, li, &nl); }
    if (mtl_on_line(s, li, st) >= 0 || nl.n >= MTL_STOPS) return 0;
    if (tail) nl.stop[nl.n++] = (uint8_t)st;
    else {
        memmove(nl.stop + 1, nl.stop, (size_t)nl.n);
        nl.stop[0] = (uint8_t)st;
        nl.n++;
    }
    return commit(s, li, &nl);
}

int mtl_insert(mtl_state *s, int li, int k, int st)
{
    if (li < 0 || li >= MTL_MAXL || st < 0 || st >= s->nst || s->over || s->won) return 0;
    mtl_line nl = s->l[li];
    if (k < 0 || k >= links(&nl) || mtl_on_line(s, li, st) >= 0 || nl.n >= MTL_STOPS) return 0;
    memmove(nl.stop + k + 2, nl.stop + k + 1, (size_t)(nl.n - k - 1));
    nl.stop[k + 1] = (uint8_t)st;
    nl.n++;
    return commit(s, li, &nl);
}

int mtl_cut(mtl_state *s, int li, int tail)
{
    if (li < 0 || li >= MTL_MAXL || s->over || s->won) return 0;
    mtl_line nl = s->l[li];
    if (!nl.n) return 0;
    if (nl.loop) nl.loop = 0;
    else if (nl.n <= 2) nl.n = 0;
    else if (tail) nl.n--;
    else { memmove(nl.stop, nl.stop + 1, (size_t)(nl.n - 1)); nl.n--; }
    return commit(s, li, &nl);
}

int mtl_remove(mtl_state *s, int li)
{
    if (li < 0 || li >= MTL_MAXL || !s->l[li].n || s->over || s->won) return 0;
    mtl_line nl;
    memset(&nl, 0, sizeof nl);
    return commit(s, li, &nl);
}

int mtl_set_lines(mtl_state *s, const mtl_line *lines)
{
    int n = 0, tun = 0;
    mtl_line nl[MTL_MAXL];
    for (int i = 0; i < MTL_MAXL; i++) {
        nl[i] = lines[i];
        if (!nl[i].n) continue;
        n++;
        for (int k = 0; k < nl[i].n; k++)
            if (nl[i].stop[k] >= s->nst) return 0;
        line_fill(s, &nl[i]);
        tun += line_tunnels(&nl[i]);
    }
    if (n > s->lines || tun > s->tunnels || s->over || s->won) return 0;
    for (int i = 0; i < MTL_MAXL; i++) {        /* the lines going first (their passengers off), then the others */
        if (nl[i].n || !s->l[i].n) continue;
        mtl_line e;
        memset(&e, 0, sizeof e);
        commit(s, i, &e);
    }
    for (int i = 0; i < MTL_MAXL; i++) {
        if (!nl[i].n) continue;
        mtl_line was = s->l[i];
        s->l[i] = nl[i];
        remap(s, i, &was);
    }
    routes(s);
    s->edits++;
    return 1;
}

/* ---- the gifts ---------------------------------------------------------------------------------------------------- */
static int gift_ok(const mtl_state *s, int k)
{
    if (k == MTL_UP_LINE) return s->lines < s->L.lines_max && s->lines < MTL_MAXL;
    if (k == MTL_UP_TUNNEL) return s->L.tunnel_gifts;
    if (k == MTL_UP_CAR) {
        for (int i = 0; i < MTL_MAXL; i++)
            if (s->l[i].n && s->t[i].cars < MTL_MAXCARS) return 1;
        return 0;
    }
    return 1;                                   /* an interchange: some station has none (there is always one) */
}

static void week(mtl_state *s)
{
    uint32_t r = s->L.seed ^ 0xa5a5f00du;       /* the gifts' own chance: the same draws whatever was chosen */
    for (int w = 0; w <= s->weekno; w++) pr_next(&r);
    int order[MTL_UPS] = {0, 1, 2, 3}, n = 0;
    pr_shuffle(&r, order, MTL_UPS);
    s->weekno++;
    for (int i = 0; i < MTL_UPS && n < 2; i++)
        if (gift_ok(s, order[i])) s->opt[n++] = (uint8_t)order[i];
    if (n < 2) s->opt[1] = s->opt[0];
    s->choosing = n > 0;
    event(s, MTL_EV_WEEK, s->weekno, 0);
}

int mtl_choose(mtl_state *s, int k)
{
    if (!s->choosing || k < 0 || k > 1) return 0;
    int g = s->opt[k];
    s->choosing = 0;
    if (g == MTL_UP_LINE) s->lines++;
    else if (g == MTL_UP_TUNNEL) s->tunnels += 2;
    else s->placing = g;
    return 1;
}

int mtl_place(mtl_state *s, int target)
{
    if (s->placing == MTL_UP_CAR) {
        if (target < 0 || target >= MTL_MAXL || !s->l[target].n || s->t[target].cars >= MTL_MAXCARS) return 0;
        s->t[target].cars++;
    } else if (s->placing == MTL_UP_INTER) {
        if (target < 0 || target >= s->nst || s->s[target].inter) return 0;
        s->s[target].inter = 1;
    } else
        return 0;
    s->placing = 0;
    return 1;
}

/* ---- the clock ---------------------------------------------------------------------------------------------------- */
static void chaikin(mtl_pt *p, int *n)
{
    for (int it = 0; it < 3; it++) {
        mtl_pt q[MTL_RPOLY];
        int m = 0;
        if (2 * *n > MTL_RPOLY) return;
        q[m++] = p[0];
        for (int i = 0; i + 1 < *n; i++) {
            mtl_pt a = p[i], b = p[i + 1];
            if (i > 0) q[m++] = (mtl_pt){(3 * a.x + b.x) / 4, (3 * a.y + b.y) / 4};
            if (i + 2 < *n) q[m++] = (mtl_pt){(a.x + 3 * b.x) / 4, (a.y + 3 * b.y) / 4};
        }
        q[m++] = p[*n - 1];
        memcpy(p, q, (size_t)m * sizeof *p);
        *n = m;
    }
}

void mtl_begin(mtl_state *s, const mtl_level *L)
{
    memset(s, 0, sizeof *s);
    s->L = *L;
    for (int r = 0; r < L->nriver; r++) {
        s->nrp[r] = L->rn[r];
        for (int i = 0; i < L->rn[r]; i++) s->rp[r][i] = (mtl_pt){L->rx[r][i] * (MTL_FP / 4), L->ry[r][i] * (MTL_FP / 4)};
        chaikin(s->rp[r], &s->nrp[r]);
    }
    for (int a = 0; a < L->ns; a++)             /* each pair's crossings, both ways */
        for (int b = a + 1; b < L->ns; b++) {
            mtl_pt p[3];
            int n = mtl_route(s, a, b, 0, p), c0 = mtl_crossings(s, p, n), c1 = c0;
            if (n == 3) {
                n = mtl_route(s, a, b, 1, p);
                c1 = mtl_crossings(s, p, n);
            }
            s->xc[a][b] = (uint8_t)((c0 > 15 ? 15 : c0) | (c1 > 15 ? 15 : c1) << 4);
        }
    s->rng = pr_seed(L->seed);
    s->lines = L->lines0;
    s->tunnels = L->tunnels0;
    for (int i = 0; i < MTL_MAXL; i++) { s->t[i].cars = 1; s->t[i].dir = 1; }
    while (s->nst < L->ns && L->st[s->nst].tick <= 0) {
        event(s, MTL_EV_APPEAR, s->nst, 0);
        s->nst++;
    }
    s->next_spawn = L->spawn0 / 2;
    s->next_week = L->week > 0 ? L->week : 0x7fffffff;
    s->over_st = -1;
    routes(s);
}

int mtl_running(const mtl_state *s) { return !s->over && !s->won && !s->choosing && !s->placing; }
int mtl_ended(const mtl_state *s) { return s->over || s->won; }

static int interval(const mtl_state *s)
{
    const mtl_level *L = &s->L;
    int32_t t = s->tick < L->ramp ? s->tick : L->ramp;
    int i = L->ramp > 0 ? L->spawn0 - (int)((int64_t)(L->spawn0 - L->spawn_min) * t / L->ramp) : L->spawn_min;
    if (L->ramp > 0 && s->tick > L->ramp) i = (int)((int64_t)L->spawn_min * L->ramp / s->tick);   /* and on, faster */
    if (L->rush_every > 0 && s->tick % L->rush_every >= L->rush_every - L->rush_len) i /= 2;
    return i < 3 ? 3 : i;
}

static void spawn(mtl_state *s)
{
    static const int W[MTL_SHAPES] = {6, 4, 4, 3, 3, 3, 3};
    int st = pr_below(&s->rng, s->nst), here = s->L.st[st].shape, roll = (int)(pr_next(&s->rng) % 1000);
    int present[MTL_SHAPES] = {0}, sum = 0;
    for (int i = 0; i < s->nst; i++) present[s->L.st[i].shape] = 1;
    for (int k = 0; k < MTL_SHAPES; k++)
        if (present[k] && k != here) sum += W[k];
    if (!sum) return;
    int pick = roll * sum / 1000, want = -1;
    for (int k = 0; k < MTL_SHAPES && want < 0; k++)
        if (present[k] && k != here) {
            if (pick < W[k]) want = k;
            else pick -= W[k];
        }
    mtl_station *S = &s->s[st];
    if (want < 0 || S->nwait >= MTL_WAIT) return;
    S->wait[S->nwait++] = (uint8_t)want;
    event(s, MTL_EV_SPAWN, st, want);
}

static int next_stop(const mtl_line *l, const mtl_train *t, int *dir)   /* where a stopped train goes next */
{
    if (l->loop) { *dir = 1; return (t->at + 1) % l->n; }
    int d = t->dir;
    if (t->at + d < 0 || t->at + d >= l->n) d = -d;
    *dir = d;
    return t->at + d;
}

static void train_step(mtl_state *s, int li)
{
    mtl_line *l = &s->l[li];
    mtl_train *t = &s->t[li];
    if (l->n < 2) return;
    if (!t->stopped) {                          /* on its way: faster, then slower near the stop */
        int32_t len = l->len[t->k], left = len - t->pos, brake = t->v * t->v / (2 * ACC);
        if (left <= brake + t->v) t->v = t->v - ACC < VMIN ? VMIN : t->v - ACC;
        else t->v = t->v + ACC > VMAX ? VMAX : t->v + ACC;
        t->pos += t->v;
        if (t->pos >= len) {
            t->stopped = 1;
            t->at = t->dir > 0 ? (t->k + 1) % l->n : t->k;
            t->pos = 0;
            t->v = 0;
            t->timer = ARRIVE;
            t->dwell = 0;
        }
        return;
    }
    t->dwell++;
    if (t->timer > 0) { t->timer--; return; }
    int st = l->stop[t->at], shape = s->L.st[st].shape, dir, nx = l->stop[next_stop(l, t, &dir)];
    int act = s->s[st].inter ? ACT_INTER : ACT;
    mtl_station *S = &s->s[st];
    for (int i = 0; i < t->nride; i++)          /* delivered */
        if (t->ride[i] == shape) {
            memmove(t->ride + i, t->ride + i + 1, (size_t)(t->nride - i - 1));
            t->nride--;
            s->delivered++;
            event(s, MTL_EV_DELIVER, st, shape);
            t->timer = act;
            if (s->L.goal[2] > 0 && s->delivered >= s->L.goal[2]) {
                s->won = 1;
                event(s, MTL_EV_WON, st, 0);
            }
            return;
        }
    for (int i = 0; i < t->nride; i++) {        /* off to change lines */
        int w = t->ride[i];
        if (s->dist[w][nx] >= s->dist[w][st] && S->nwait < MTL_WAIT) {
            memmove(t->ride + i, t->ride + i + 1, (size_t)(t->nride - i - 1));
            t->nride--;
            S->wait[S->nwait++] = (uint8_t)w;
            t->timer = act;
            return;
        }
    }
    if (t->nride < t->cars * MTL_CAR)           /* on board */
        for (int i = 0; i < S->nwait; i++) {
            int w = S->wait[i];
            if (s->dist[w][nx] < s->dist[w][st]) {
                memmove(S->wait + i, S->wait + i + 1, (size_t)(S->nwait - i - 1));
                S->nwait--;
                t->ride[t->nride++] = (uint8_t)w;
                t->timer = act;
                event(s, MTL_EV_BOARD, st, li);
                return;
            }
        }
    if (t->dwell < DWELL) return;
    t->dir = dir;                               /* off it goes */
    t->k = dir > 0 ? t->at : (t->at - 1 + l->n) % l->n;
    if (l->loop) t->k = t->at;
    t->stopped = 0;
    t->pos = 0;
    t->v = 0;
}

void mtl_step(mtl_state *s)
{
    if (!mtl_running(s)) return;
    s->tick++;
    int more = 0;
    while (s->nst < s->L.ns && s->L.st[s->nst].tick <= s->tick) {
        event(s, MTL_EV_APPEAR, s->nst, 0);
        s->nst++;
        more = 1;
    }
    if (more) routes(s);
    if (s->tick >= s->next_spawn) {
        spawn(s);
        s->next_spawn = s->tick + interval(s);
    }
    for (int i = 0; i < MTL_MAXL; i++) train_step(s, i);
    for (int i = 0; i < s->nst && !s->won; i++) {
        mtl_station *S = &s->s[i];
        if (S->nwait > mtl_room(s, i)) S->crowd++;
        else if (S->crowd > 0) S->crowd -= S->crowd >= 2 ? 2 : 1;
        if (S->crowd >= MTL_CROWD && !s->over) {
            s->over = 1;
            s->over_st = i;
            event(s, MTL_EV_OVER, i, 0);
        }
    }
    if (!s->over && !s->won && s->tick >= s->next_week) {
        s->next_week += s->L.week;
        week(s);
    }
}

int mtl_stars(const mtl_state *s)
{
    int d = s->delivered, n = 0;
    for (int k = 0; k < 3; k++)
        if (s->L.goal[k] > 0 && d >= s->L.goal[k]) n = k + 1;
    return n;
}

int mtl_endless_stars(int variant, int delivered)
{
    static const int T[MTL_VARIANTS][2] = {{400, 800}, {350, 700}, {300, 600}};   /* (a bigger city: harder) */
    int v = variant < 0 || variant >= MTL_VARIANTS ? 0 : variant;
    return delivered >= T[v][1] ? 3 : delivered >= T[v][0] ? 2 : 1;
}

/* ---- the notation ------------------------------------------------------------------------------------------------- */
static int num(const char **p)
{
    int v = 0, any = 0;
    while (**p >= '0' && **p <= '9') { v = v * 10 + (**p - '0'); (*p)++; any = 1; }
    return any ? v : -1;
}

static int run_ticks(mtl_state *s, int32_t n)
{
    for (int32_t i = 0; i < n && mtl_running(s); i++) mtl_step(s);
    return 1;
}

int mtl_do(mtl_state *s, const char *a)
{
    const char *p = a + 1;
    switch (a[0]) {
    case 'w': {
        int sec = num(&p), tenth = 0;
        if (sec < 0) return 0;
        if (*p == '.') { p++; tenth = num(&p); if (tenth < 0) tenth = 0; }
        return run_ticks(s, sec * MTL_HZ + tenth * MTL_HZ / 10);
    }
    case 't': {
        int n = num(&p);
        return n >= 0 && run_ticks(s, n);
    }
    case '@': {
        int n = num(&p);
        if (n < 0) return 0;
        while (s->tick < n && mtl_running(s)) mtl_step(s);
        return 1;
    }
    case 'n': {
        int st[MTL_STOPS + 1], n = 0;
        while (n <= MTL_STOPS) {
            int v = num(&p);
            if (v < 1) return 0;
            st[n++] = v - 1;
            if (*p != '-') break;
            p++;
        }
        return mtl_new_line(s, st, n);
    }
    case 'e': {
        int l = num(&p) - 1, tail = *p == 't';
        if (*p != 'h' && *p != 't') return 0;
        p++;
        int ok = 0;
        while (*p == '-') {
            p++;
            int v = num(&p);
            if (v < 1 || !mtl_extend(s, l, tail, v - 1)) return 0;
            ok = 1;
        }
        return ok;
    }
    case 'i': {
        int l = num(&p) - 1;
        if (*p++ != '.') return 0;
        int k = num(&p) - 1;
        if (*p++ != '-') return 0;
        int v = num(&p);
        return v >= 1 && mtl_insert(s, l, k, v - 1);
    }
    case 'c': {
        int l = num(&p) - 1;
        if (*p != 'h' && *p != 't') return 0;
        return mtl_cut(s, l, *p == 't');
    }
    case 'x': return mtl_remove(s, num(&p) - 1);
    case 'g': return mtl_choose(s, num(&p) - 1);
    case 'p': return mtl_place(s, num(&p) - 1);
    }
    return 0;
}

int mtl_script(mtl_state *s, const char *moves)
{
    char a[MTL_ACT];
    int ok = 1;
    while (*moves) {
        while (*moves == ' ') moves++;
        int n = 0;
        while (moves[n] && moves[n] != ' ') n++;
        if (!n) break;
        if (n >= MTL_ACT) return 0;
        memcpy(a, moves, (size_t)n);
        a[n] = 0;
        moves += n;
        if (!mtl_do(s, a)) ok = 0;
    }
    return ok;
}

/* ---- the planners ------------------------------------------------------------------------------------------------- */
static int on_any(const mtl_state *s, int st)
{
    for (int i = 0; i < MTL_MAXL; i++)
        if (s->l[i].n && mtl_on_line(s, i, st) >= 0) return 1;
    return 0;
}

static int32_t pair_cost(const mtl_state *s, int a, int b, int *cross)
{
    int c, bend = mtl_best_bend(s, a, b, &c);
    *cross = c;
    return link_len(s, a, b, bend) + 2500 * c;
}

static int tunnels_left(const mtl_state *s) { return s->tunnels - mtl_tunnels_used(s); }

/* the naive player: the nearest way to join each new station, gifts in a fixed order */
static int greedy(const mtl_state *s, char *act)
{
    act[0] = 0;
    if (s->choosing) {
        static const int PREF[MTL_UPS] = {MTL_UP_LINE, MTL_UP_CAR, MTL_UP_INTER, MTL_UP_TUNNEL};
        for (int p = 0; p < MTL_UPS; p++)
            for (int k = 0; k < 2; k++)
                if (s->opt[k] == PREF[p]) { sprintf(act, "g%d", k + 1); return 1; }
        return 0;
    }
    if (s->placing == MTL_UP_CAR) {
        int best = -1;
        for (int i = 0; i < MTL_MAXL; i++)
            if (s->l[i].n && s->t[i].cars < MTL_MAXCARS && (best < 0 || s->l[i].n > s->l[best].n)) best = i;
        sprintf(act, "p%d", best + 1);
        return best >= 0;
    }
    if (s->placing == MTL_UP_INTER) {
        int best = -1;
        for (int i = 0; i < s->nst; i++)
            if (!s->s[i].inter && (best < 0 || s->s[i].nwait > s->s[best].nwait)) best = i;
        sprintf(act, "p%d", best + 1);
        return best >= 0;
    }
    int left = tunnels_left(s);
    for (int st = 0; st < s->nst; st++) {
        if (on_any(s, st)) continue;
        int32_t ext = 0x7fffffff, nw = 0x7fffffff;
        int el = -1, et = 0, nt = -1, c;
        for (int i = 0; i < MTL_MAXL; i++) {
            const mtl_line *l = &s->l[i];
            if (l->n < 2 || l->loop || l->n >= MTL_STOPS) continue;
            for (int e = 0; e < 2; e++) {
                int32_t v = pair_cost(s, e ? l->stop[l->n - 1] : l->stop[0], st, &c);
                if (c <= left && v < ext) { ext = v; el = i; et = e; }
            }
        }
        if (mtl_free_line(s) >= 0)
            for (int o = 0; o < s->nst; o++) {
                if (o == st) continue;
                int32_t v = pair_cost(s, st, o, &c);
                if (c <= left && v < nw) { nw = v; nt = o; }
            }
        if (el >= 0 && (nt < 0 || ext <= nw)) { sprintf(act, "e%d%c-%d", el + 1, et ? 't' : 'h', st + 1); return 1; }
        if (nt >= 0) { sprintf(act, "n%d-%d", st + 1, nt + 1); return 1; }
    }
    return 0;
}

static int64_t value(const mtl_state *s, int32_t t0)
{
    int64_t v = (int64_t)s->delivered * 400;
    for (int i = 0; i < s->nst; i++) {
        v -= (int64_t)s->s[i].crowd * s->s[i].crowd / 40;
        v -= s->s[i].nwait * 6;
    }
    if (s->over) v -= 2000000 - (int64_t)(s->tick - t0) * 300;
    return v;
}

/* the state after `act`, then `horizon` ticks with the naive player deciding: how good */
static int64_t rollout(const mtl_state *s0, const char *act, int horizon)
{
    static mtl_state s;
    s = *s0;
    s.L.goal[2] = 0;                            /* (looking ahead the same, whatever the goals) */
    if (act[0] && !mtl_script(&s, act)) return INT64_MIN;
    int32_t end = s.tick + horizon, t0 = s0->tick;
    int wait = 0;
    char a[MTL_ACT * 2];
    while (s.tick < end && !mtl_ended(&s)) {
        if (!mtl_running(&s) || wait <= 0) {
            if (greedy(&s, a) && mtl_script(&s, a)) continue;
            if (!mtl_running(&s)) break;
            wait = 25;
        }
        wait--;
        mtl_step(&s);
    }
    return value(&s, t0);
}

#define CANDS 48
typedef struct { char a[MTL_ACT * 2]; } cand;

static void add(cand *c, int *n, const char *fmt, int x, int y, int z)
{
    if (*n >= CANDS) return;
    snprintf(c[*n].a, sizeof c[*n].a, fmt, x, y, z);
    for (int i = 0; i < *n; i++)
        if (!strcmp(c[i].a, c[*n].a)) return;
    (*n)++;
}

static void nearest(const mtl_state *s, int st, int *out, int k, int want_shape)   /* the k nearest stations */
{
    int32_t d[MTL_MAXS];
    int n = 0;
    for (int o = 0; o < s->nst; o++) {
        if (o == st || (want_shape >= 0 && s->L.st[o].shape != want_shape)) continue;
        int c;
        int32_t v = pair_cost(s, st, o, &c);
        int at = n;
        while (at > 0 && d[at - 1] > v) at--;
        if (at >= k) continue;
        for (int j = (n < k ? n : k - 1); j > at; j--) { d[j] = d[j - 1]; out[j] = out[j - 1]; }
        d[at] = v;
        out[at] = o;
        if (n < k) n++;
    }
    for (int j = n; j < k; j++) out[j] = -1;
}

static void place_one(cand *c, int *n, const char *pre, int target)
{
    if (*n >= CANDS) return;
    snprintf(c[*n].a, sizeof c[0].a, "%s%sp%d", pre, pre[0] ? " " : "", target + 1);
    (*n)++;
}

static void place_cands(const mtl_state *s, cand *c, int *n, const char *pre, int gift)
{
    if (gift == MTL_UP_CAR) {
        for (int i = 0; i < MTL_MAXL; i++)
            if (s->l[i].n && s->t[i].cars < MTL_MAXCARS) place_one(c, n, pre, i);
    } else {
        int top[4] = {-1, -1, -1, -1};
        for (int i = 0; i < s->nst; i++) {
            if (s->s[i].inter) continue;
            int v = s->s[i].nwait * 40 + s->s[i].crowd;
            for (int j = 0; j < 4; j++) {
                int u = top[j] < 0 ? -1 : s->s[top[j]].nwait * 40 + s->s[top[j]].crowd;
                if (top[j] < 0 || v > u) {
                    memmove(top + j + 1, top + j, (size_t)(3 - j) * sizeof top[0]);
                    top[j] = i;
                    break;
                }
            }
        }
        for (int j = 0; j < 4; j++)
            if (top[j] >= 0) place_one(c, n, pre, top[j]);
    }
}

static int candidates(const mtl_state *s, cand *c)
{
    int n = 0;
    if (s->choosing) {
        for (int k = 0; k < 2; k++) {
            int g = s->opt[k];
            char pre[8];
            snprintf(pre, sizeof pre, "g%d", k + 1);
            if (g == MTL_UP_CAR || g == MTL_UP_INTER) place_cands(s, c, &n, pre, g);
            else add(c, &n, "g%d", k + 1, 0, 0);
        }
        return n;
    }
    if (s->placing) {
        place_cands(s, c, &n, "", s->placing);
        return n;
    }
    int free = mtl_free_line(s) >= 0, joined = 0;
    for (int st = 0; st < s->nst && joined < 2; st++) {
        if (on_any(s, st)) continue;
        joined++;
        int nb[4];
        if (free) {
            nearest(s, st, nb, 4, -1);
            for (int j = 0; j < 4; j++)
                if (nb[j] >= 0) add(c, &n, "n%d-%d", st + 1, nb[j] + 1, 0);
        }
        for (int i = 0; i < MTL_MAXL; i++) {
            const mtl_line *l = &s->l[i];
            if (l->n < 2) continue;
            if (!l->loop) {
                add(c, &n, "e%dh-%d", i + 1, st + 1, 0);
                add(c, &n, "e%dt-%d", i + 1, st + 1, 0);
            }
            int bestk = -1;
            int32_t bd = 0x7fffffff;            /* the cheapest detour on this line */
            for (int k = 0; k < links(l); k++) {
                int a = l->stop[k], b = l->stop[(k + 1) % l->n], ca, cb;
                int32_t d = pair_cost(s, a, st, &ca) + pair_cost(s, st, b, &cb) - l->len[k];
                if (d < bd) { bd = d; bestk = k; }
            }
            if (bestk >= 0 && bd < 4 * MTL_FP) add(c, &n, "i%d.%d-%d", i + 1, bestk + 1, st + 1);
        }
    }
    if (joined) return n;
    /* all joined: something for the busiest station, or nothing */
    add(c, &n, "", 0, 0, 0);
    int busy = -1;
    for (int i = 0; i < s->nst; i++)
        if (busy < 0 || s->s[i].nwait + s->s[i].crowd / 30 > s->s[busy].nwait + s->s[busy].crowd / 30) busy = i;
    if (busy < 0 || s->s[busy].nwait < 3) return n;
    int want[MTL_SHAPES] = {0}, top = -1;
    for (int i = 0; i < s->s[busy].nwait; i++) want[s->s[busy].wait[i]]++;
    for (int k = 0; k < MTL_SHAPES; k++)
        if (want[k] && (top < 0 || want[k] > want[top])) top = k;
    if (top >= 0) {
        int nb[3];
        nearest(s, busy, nb, 3, top);
        for (int j = 0; j < 3; j++) {
            if (nb[j] < 0) continue;
            if (free) add(c, &n, "n%d-%d", busy + 1, nb[j] + 1, 0);
            for (int i = 0; i < MTL_MAXL; i++) {   /* a line through the busy station reaching out to that shape */
                const mtl_line *l = &s->l[i];
                if (l->n < 2 || l->loop || mtl_on_line(s, i, nb[j]) >= 0) continue;
                if (l->stop[0] == busy || l->stop[l->n - 1] == busy || mtl_on_line(s, i, busy) >= 0) {
                    add(c, &n, "e%dh-%d", i + 1, nb[j] + 1, 0);
                    add(c, &n, "e%dt-%d", i + 1, nb[j] + 1, 0);
                }
            }
        }
    }
    for (int i = 0; i < MTL_MAXL; i++) {       /* a line made a loop */
        const mtl_line *l = &s->l[i];
        if (l->n >= 4 && !l->loop) add(c, &n, "e%dt-%d", i + 1, l->stop[0] + 1, 0);
    }
    for (int i = 0; i < MTL_MAXL; i++) {       /* a line reaching a shape it does not have */
        const mtl_line *l = &s->l[i];
        if (l->n < 2 || l->loop) continue;
        int has[MTL_SHAPES] = {0};
        for (int q = 0; q < l->n; q++) has[s->L.st[l->stop[q]].shape] = 1;
        for (int sh = 0; sh < MTL_SHAPES; sh++) {
            if (has[sh]) continue;
            for (int e = 0; e < 2; e++) {
                int nb[1];
                nearest(s, e ? l->stop[l->n - 1] : l->stop[0], nb, 1, sh);
                if (nb[0] >= 0 && mtl_on_line(s, i, nb[0]) < 0) add(c, &n, e ? "e%dt-%d" : "e%dh-%d", i + 1, nb[0] + 1, 0);
            }
        }
    }
    return n;
}

int mtl_plan(const mtl_state *s, int naive, int horizon, char *act)
{
    act[0] = 0;
    if (mtl_ended(s)) return 0;
    if (naive) return greedy(s, act);
    static cand c[CANDS];
    int n = candidates(s, c), best = -1;
    int64_t bv = INT64_MIN;
    for (int i = 0; i < n; i++) {
        int64_t v = rollout(s, c[i].a, horizon);
        if (v > bv) { bv = v; best = i; }
    }
    if (best < 0) return greedy(s, act);
    snprintf(act, MTL_ACT * 2, "%s", c[best].a);
    return act[0] != 0;
}

int mtl_bot_horizon = 30 * MTL_HZ;

int mtl_bot_run(const mtl_level *L, int naive, int max_ticks, char *plan, int cap, int *ticks)
{
    static mtl_state s;
    mtl_begin(&s, L);
    int used = 0, wait = 0, last_edit = -1000, horizon = mtl_bot_horizon;
    int32_t last_tick = -1;
    if (plan && cap > 0) plan[0] = 0;
    char a[MTL_ACT * 2];
    while (!mtl_ended(&s) && s.tick < max_ticks) {
        int need = !mtl_running(&s), loose = 0;
        for (int i = 0; i < s.nst && !need; i++)
            if (!on_any(&s, i)) loose = 1;
        if (loose && wait <= 0) need = 1;
        if (!naive && !need && wait <= 0 && s.tick - last_edit >= 4 * MTL_HZ) need = 1;
        if (need) {
            int ok = mtl_plan(&s, naive, horizon, a) && a[0];
            if (ok && mtl_script(&s, a)) {
                if (plan && used + 32 < cap) {
                    if (s.tick != last_tick) used += snprintf(plan + used, (size_t)(cap - used), "%s@%d", used ? " " : "",
                                                              (int)s.tick);
                    used += snprintf(plan + used, (size_t)(cap - used), " %s", a);
                    last_tick = s.tick;
                }
                if (a[0] != 'g' && a[0] != 'p') last_edit = s.tick;
                continue;
            }
            if (!mtl_running(&s)) break;        /* (a gift no player takes: never) */
            wait = loose ? 25 : 4 * MTL_HZ;
            last_edit = s.tick;
        }
        wait--;
        mtl_step(&s);
    }
    if (ticks) *ticks = s.tick;
    return s.delivered;
}

/* ---- the levels --------------------------------------------------------------------------------------------------- */
/* a level as text: "w,h|lines0,lines_max,tunnels0,gifts|week,spawn0,spawn_min,ramp,rush_every,rush_len|g1,g2,g3|
 * seed|rivers: hw:x,y;x,y... / ...|lakes: x,y,rx,ry/...|stations: x,y,shape,tick;..." */
int mtl_text(const mtl_level *L, char *o, int cap)
{
    int n = snprintf(o, (size_t)cap, "%d,%d|%d,%d,%d,%d|%d,%d,%d,%d,%d,%d|%d,%d,%d|%u|", L->w, L->h, L->lines0,
                     L->lines_max, L->tunnels0, L->tunnel_gifts, L->week, L->spawn0, L->spawn_min, L->ramp,
                     L->rush_every, L->rush_len, L->goal[0], L->goal[1], L->goal[2], (unsigned)L->seed);
    for (int r = 0; r < L->nriver && n < cap; r++) {
        n += snprintf(o + n, (size_t)(cap - n), "%s%d:", r ? "/" : "", L->rhw[r]);
        for (int i = 0; i < L->rn[r] && n < cap; i++)
            n += snprintf(o + n, (size_t)(cap - n), "%s%d,%d", i ? ";" : "", L->rx[r][i], L->ry[r][i]);
    }
    if (n < cap) n += snprintf(o + n, (size_t)(cap - n), "|");
    for (int k = 0; k < L->nlake && n < cap; k++)
        n += snprintf(o + n, (size_t)(cap - n), "%s%d,%d,%d,%d", k ? "/" : "", L->lx[k], L->ly[k], L->lrx[k], L->lry[k]);
    if (n < cap) n += snprintf(o + n, (size_t)(cap - n), "|");
    for (int i = 0; i < L->ns && n < cap; i++)
        n += snprintf(o + n, (size_t)(cap - n), "%s%d,%d,%d,%d", i ? ";" : "", L->st[i].x, L->st[i].y, L->st[i].shape,
                      (int)L->st[i].tick);
    return n < cap;
}

static int snum(const char **p, int *v)        /* a number, maybe negative */
{
    int neg = 0;
    if (**p == '-') { neg = 1; (*p)++; }
    int x = num(p);
    if (x < 0) return 0;
    *v = neg ? -x : x;
    return 1;
}

static int nums(const char **p, int *v, int n, char sep)
{
    for (int i = 0; i < n; i++) {
        if (i && *(*p)++ != sep) return 0;
        if (!snum(p, &v[i])) return 0;
    }
    return 1;
}

int mtl_parse(const char *t, mtl_level *L)
{
    memset(L, 0, sizeof *L);
    const char *p = t;
    int v[6];
    if (!nums(&p, v, 2, ',') || *p++ != '|') return 0;
    L->w = v[0], L->h = v[1];
    if (!nums(&p, v, 4, ',') || *p++ != '|') return 0;
    L->lines0 = v[0], L->lines_max = v[1], L->tunnels0 = v[2], L->tunnel_gifts = v[3];
    if (!nums(&p, v, 6, ',') || *p++ != '|') return 0;
    L->week = v[0], L->spawn0 = v[1], L->spawn_min = v[2], L->ramp = v[3], L->rush_every = v[4], L->rush_len = v[5];
    if (!nums(&p, v, 3, ',') || *p++ != '|') return 0;
    L->goal[0] = v[0], L->goal[1] = v[1], L->goal[2] = v[2];
    unsigned sd = 0;
    while (*p >= '0' && *p <= '9') sd = sd * 10 + (unsigned)(*p++ - '0');
    L->seed = sd;
    if (*p++ != '|') return 0;
    while (*p && *p != '|' && L->nriver < MTL_RIVERS) {
        int r = L->nriver++;
        if (!snum(&p, &L->rhw[r]) || *p++ != ':') return 0;
        while (L->rn[r] < MTL_RPTS) {
            if (!nums(&p, v, 2, ',')) return 0;
            L->rx[r][L->rn[r]] = (int8_t)v[0];
            L->ry[r][L->rn[r]++] = (int8_t)v[1];
            if (*p != ';') break;
            p++;
        }
        if (*p == '/') p++;
    }
    if (*p++ != '|') return 0;
    while (*p && *p != '|' && L->nlake < MTL_LAKES) {
        if (!nums(&p, v, 4, ',')) return 0;
        int k = L->nlake++;
        L->lx[k] = (int8_t)v[0], L->ly[k] = (int8_t)v[1], L->lrx[k] = (int8_t)v[2], L->lry[k] = (int8_t)v[3];
        if (*p == '/') p++;
    }
    if (*p++ != '|') return 0;
    while (*p && L->ns < MTL_MAXS) {
        if (!nums(&p, v, 4, ',')) return 0;
        mtl_sched *s = &L->st[L->ns++];
        s->x = (int8_t)v[0], s->y = (int8_t)v[1], s->shape = (int8_t)v[2], s->tick = v[3];
        if (*p == ';') p++;
    }
    return L->w > 1 && L->h > 1 && L->ns >= 2;
}

#ifdef MTL_GEN                                   /* (the level maker: no levels yet) */
const char *const mtl_levels[MTL_LEVELS], *const mtl_plans[MTL_LEVELS], *const mtl_icons[MTL_CHAPTERS];
#else
#include "games/metrolines_levels.h"
#endif

int mtl_level_get(int level, mtl_level *L)
{
    if (level < 0 || level >= MTL_LEVELS || !mtl_levels[level]) return 0;
    return mtl_parse(mtl_levels[level], L);
}

int mtl_chapter_of(int level) { return level < 0 ? 0 : level / (MTL_LEVELS / MTL_CHAPTERS); }

/* ---- the endless maps: a city made from a seed ------------------------------------------------------------------- */
static int land_ok(const mtl_state *w, int x, int y, int margin)   /* a station's place clear of water by margin/4 */
{
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (mtl_wet(w, x * MTL_FP + dx * margin * (MTL_FP / 4), y * MTL_FP + dy * margin * (MTL_FP / 4))) return 0;
    return 1;
}

void mtl_make_river(mtl_level *L, uint32_t *r, int across)   /* a river from one side of the map to the other */
{
    int k = L->nriver++, n = 7;
    L->rhw[k] = 2 + pr_below(r, 2);             /* half a unit or more */
    L->rn[k] = n;
    int W = L->w * 4, H = L->h * 4;
    for (int i = 0; i < n; i++) {
        int along = -8 + i * ((across ? W : H) + 16) / (n - 1);
        int base = (across ? H : W) * (30 + pr_below(r, 40)) / 100;
        if (i > 0) {
            int prev = across ? L->ry[k][i - 1] : L->rx[k][i - 1];
            base = prev + (pr_below(r, 17) - 8);
            int lim = across ? H : W;
            if (base < lim / 5) base = lim / 5;
            if (base > lim * 4 / 5) base = lim * 4 / 5;
        }
        if (across) { L->rx[k][i] = (int8_t)along; L->ry[k][i] = (int8_t)base; }
        else { L->rx[k][i] = (int8_t)base; L->ry[k][i] = (int8_t)along; }
    }
}

/* stations placed on land, apart from each other, in the order they come (shapes: the first three, then mostly
 * circles, triangles and squares, a rare one now and then) */
void mtl_make_stations(mtl_level *L, uint32_t *r, int n, int32_t every, int rare)
{
    static mtl_state w;
    mtl_begin(&w, L);                           /* (its water) */
    L->ns = 0;
    int shapes[MTL_MAXS], used_rare = 0;
    for (int i = 0; i < n; i++) {
        if (i < 3) shapes[i] = i;
        else {
            int roll = pr_below(r, 100);
            if (rare > used_rare && i >= 6 && roll < 14) shapes[i] = 3 + used_rare++;
            else shapes[i] = roll < 50 ? MTL_CIRCLE : roll < 76 ? MTL_TRIANGLE : MTL_SQUARE;
        }
    }
    for (int i = 0; i < n; i++) {
        int bx = -1, by = -1, best = -1000000;
        int maxr = (L->w > L->h ? L->w : L->h) * 10 / 2, aim = 15 + (maxr - 15) * i / (n > 1 ? n - 1 : 1);
        for (int y = 1; y < L->h - 1; y++)
            for (int x = 1; x < L->w - 1; x++) {
                int dmin = 99;
                for (int j = 0; j < L->ns; j++) {
                    int d = iabs(L->st[j].x - x) > iabs(L->st[j].y - y) ? iabs(L->st[j].x - x) : iabs(L->st[j].y - y);
                    if (d < dmin) dmin = d;
                }
                if (dmin < (i < 3 ? 3 : 2) || !land_ok(&w, x, y, 2)) continue;
                /* near the middle first, spreading out later; not far from the others */
                int cx = iabs(2 * x - L->w + 1) * 5, cy = iabs(2 * y - L->h + 1) * 5, rr = cx > cy ? cx : cy;
                int score = -iabs(rr - aim) * 3 + pr_below(r, 40);
                if (L->ns && dmin > 3) score -= (dmin - 3) * 25;
                if (score > best) { best = score; bx = x; by = y; }
            }
        if (bx < 0) break;
        mtl_sched *s = &L->st[L->ns++];
        s->x = (int8_t)bx, s->y = (int8_t)by, s->shape = (int8_t)shapes[i];
        s->tick = i < 3 ? 0 : (int32_t)((i - 2) * every);
    }
}

void mtl_variant_level(int variant, uint32_t seed, mtl_level *L)
{
    static const int W[MTL_VARIANTS] = {9, 10, 12}, H[MTL_VARIANTS] = {12, 14, 17}, N[MTL_VARIANTS] = {16, 24, 32};
    int v = variant < 0 || variant >= MTL_VARIANTS ? 0 : variant;
    uint32_t r = pr_seed(seed);
    memset(L, 0, sizeof *L);
    L->w = W[v];
    L->h = H[v];
    L->lines0 = 3;
    L->lines_max = MTL_MAXL;
    L->tunnels0 = 3;
    L->tunnel_gifts = 1;
    L->week = 45 * MTL_HZ;
    L->spawn0 = 55;
    L->spawn_min = 10 - v;
    L->ramp = 8 * 60 * MTL_HZ;
    L->seed = pr_next(&r);
    int rivers = 1 + (v > 0 && pr_below(&r, 2));
    for (int i = 0; i < rivers; i++) mtl_make_river(L, &r, (int)pr_below(&r, 3) > 0 || i);
    mtl_make_stations(L, &r, N[v], 16 * MTL_HZ, 2 + v);
}

/* ---- the shapes, for drawing ------------------------------------------------------------------------------------- */
int mtl_shape_poly(int shape, float *xy)
{
    static const float TRI[] = {0, -1.12f, 0.98f, 0.62f, -0.98f, 0.62f};
    static const float SQ[] = {-0.84f, -0.84f, 0.84f, -0.84f, 0.84f, 0.84f, -0.84f, 0.84f};
    static const float DIA[] = {0, -1.12f, 1.0f, 0, 0, 1.12f, -1.0f, 0};
    static const float CROSS[] = {-0.34f, -1, 0.34f, -1, 0.34f, -0.34f, 1, -0.34f, 1, 0.34f, 0.34f, 0.34f,
                                  0.34f, 1, -0.34f, 1, -0.34f, 0.34f, -1, 0.34f, -1, -0.34f, -0.34f, -0.34f};
    int n = 0;
    switch (shape) {
    case MTL_TRIANGLE: memcpy(xy, TRI, sizeof TRI); return 3;
    case MTL_SQUARE: memcpy(xy, SQ, sizeof SQ); return 4;
    case MTL_DIAMOND: memcpy(xy, DIA, sizeof DIA); return 4;
    case MTL_CROSS: memcpy(xy, CROSS, sizeof CROSS); return 12;
    case MTL_STAR: {                            /* (the points every 36 degrees from straight up) */
        static const float C[10] = {0, 0.5877853f, 0.9510565f, 0.9510565f, 0.5877853f, 0, -0.5877853f,
                                    -0.9510565f, -0.9510565f, -0.5877853f};
        static const float S[10] = {-1, -0.8090170f, -0.3090170f, 0.3090170f, 0.8090170f, 1, 0.8090170f,
                                    0.3090170f, -0.3090170f, -0.8090170f};
        for (int i = 0; i < 10; i++) {
            float r = i & 1 ? 0.52f : 1.18f;
            xy[n++] = r * C[i];
            xy[n++] = r * S[i];
        }
        return 10;
    }
    case MTL_PENTAGON: {
        static const float C[5] = {0, 0.9510565f, 0.5877853f, -0.5877853f, -0.9510565f};
        static const float S[5] = {-1, -0.3090170f, 0.8090170f, 0.8090170f, -0.3090170f};
        for (int i = 0; i < 5; i++) { xy[n++] = 1.05f * C[i]; xy[n++] = 1.05f * S[i]; }
        return 5;
    }
    }
    return 0;
}
