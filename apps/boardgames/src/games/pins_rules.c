/*
 * Pull the Pins: the rules (pins.h), without SDL: the levels' text, the world in whole numbers and its physics, the
 * search over the orders of pulling the pins, the hand-drawn templates and the maker that fills them.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/pins.h"
#include "games/puzrand.h"

/* the things: radius, weight, damping (speed lost a tick: 1 / DAMP), rubbing on a wall (1 / FRIC of the speed along
 * it a tick) */
static const int32_t RAD[PTP_KINDS] = {640, 640, 384, 384, 384};
static const int MASS[PTP_KINDS] = {4, 4, 1, 2, 2};
static const int DAMP[PTP_KINDS] = {150, 150, 260, 40, 80};
static const int FRIC[PTP_KINDS] = {9, 9, 40, 4, 6};
#define GRAV 9                                  /* steps a tick, each tick */
#define VMAX 230                                /* the fastest a tick (under a ball's radius: nothing goes through) */
#define REST_V 10                               /* at rest: nothing moving faster than this a tick ... */
#define REST_TICKS 30                           /* ... for this long; or */
#define WINDOW 16                               /* in two windows of this many ticks running ... */
#define WINDOW_D 48                             /* ... nothing out of the cups gone further than this */
#define QUIET_MAX 900                           /* at rest anyway this long after the last pull */
#define COOL_TICKS 8                            /* lava touched by water or stone: stone this much later */
#define SLACK 40                                /* touching: this close */
#define HALF 128                                /* a half unit in steps */
#define U(v) ((int32_t)(v) * PTP_FP)

/* ---- the text ------------------------------------------------------------------------------------------------------ */
static int hex2(const char *s)
{
    int v = 0;
    for (int i = 0; i < 2; i++) {
        char c = s[i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (d < 0) return -1;
        v = v * 16 + d;
    }
    return v;
}

static int hexn(const char *s, int n, int *v)
{
    for (int i = 0; i < n; i++)
        if ((v[i] = hex2(s + 2 * i)) < 0) return 0;
    return 1;
}

static const char KIND_CH[] = "gdwl";

int ptp_parse(ptp_map *m, const char *t)
{
    memset(m, 0, sizeof *m);
    m->cups = 1;
    m->cupcol[0] = m->cupcol[1] = -1;
    m->tpl = -1;
    int kinds[PTP_MAXFILL], cols[PTP_MAXFILL], counts[PTP_MAXFILL], nb = 0;
    while (*t) {
        while (*t == ' ') t++;
        if (!*t) break;
        const char *e = t;
        while (*e && *e != ' ') e++;
        int len = (int)(e - t), v[8];
        if (t[0] == 'C' && len >= 2) {
            m->cups = t[1] == '2' ? 2 : 1;
            for (int k = 0; k < m->cups && 2 + k < len; k++) m->cupcol[k] = (int8_t)(t[2 + k] == '-' ? -1 : t[2 + k] - '0');
        } else if ((t[0] == 'w' || t[0] == 'p') && len == 9 && hexn(t + 1, 4, v)) {
            ptp_line *l;
            if (t[0] == 'w') {
                if (m->nw >= PTP_MAXWALL) return 0;
                l = &m->w[m->nw++];
            } else {
                if (m->np >= PTP_MAXPIN) return 0;
                l = &m->p[m->np++];
            }
            *l = (ptp_line){(int16_t)v[0], (int16_t)v[1], (int16_t)v[2], (int16_t)v[3]};
        } else if (t[0] == 'f' && len == 13 && hexn(t + 3, 1, v) && hexn(t + 5, 4, v + 1)) {
            const char *k = strchr(KIND_CH, t[1]);
            if (!k || m->nf >= PTP_MAXFILL) return 0;
            m->f[m->nf++] = (ptp_fill){(uint8_t)(k - KIND_CH), (uint8_t)(t[2] - '0'), (uint8_t)v[0], (int16_t)v[1],
                                       (int16_t)v[2], (int16_t)v[3], (int16_t)v[4]};
        } else if (t[0] == 't' && len == 4 && hexn(t + 1, 1, v)) {
            m->tpl = v[0];
            m->mirror = t[3] == '1';
        } else if (t[0] == 'b' && len == 5 && nb < PTP_MAXFILL && hexn(t + 3, 1, v)) {
            const char *k = strchr(KIND_CH, t[1]);
            kinds[nb] = t[1] == '-' || !k ? -1 : (int)(k - KIND_CH);
            cols[nb] = t[2] - '0';
            counts[nb++] = v[0];
        } else if (t[0] == 's') {
            const char *p = t + 1;
            while (p < e && m->nsol < PTP_MAXSOL) {
                int n = 0;
                while (p < e && *p >= '0' && *p <= '9' && n < PTP_MAXPIN) m->sol[m->nsol][n++] = (uint8_t)(*p++ - '0');
                if (n) m->sollen[m->nsol++] = (uint8_t)n;
                if (p < e && *p == ',') p++;
                else break;
            }
        } else
            return 0;
        t = e;
    }
    if (m->tpl >= 0) {                          /* the template's walls, pins and boxes; the cups and the orders kept */
        ptp_map t2;
        int8_t cc[2] = {m->cupcol[0], m->cupcol[1]};
        int cupcol[2] = {cc[0], cc[1]};
        if (m->tpl >= PTP_TPLS || nb != ptp_tpl_boxes(m->tpl) ||
            !ptp_from_tpl(&t2, m->tpl, m->mirror, kinds, cols, counts, cupcol))
            return 0;
        memcpy(t2.sol, m->sol, sizeof m->sol);
        memcpy(t2.sollen, m->sollen, sizeof m->sollen);
        t2.nsol = m->nsol;
        *m = t2;
    }
    return m->np > 0 && m->nf > 0;
}

void ptp_text(const ptp_map *m, char *o)
{
    int n = 0;
    n += sprintf(o + n, "C%d", m->cups);
    for (int k = 0; k < m->cups; k++) o[n++] = m->cupcol[k] < 0 ? '-' : (char)('0' + m->cupcol[k]);
    if (m->tpl >= 0) {
        n += sprintf(o + n, " t%02x%d", m->tpl, m->mirror);
        for (int b = 0; b < m->nbox; b++) {
            int k = m->bkind[b];
            n += sprintf(o + n, " b%c%c%02x", k < PTP_STONE ? KIND_CH[k] : '-', k < PTP_STONE ? '0' + m->bcol[b] : '0',
                         k < PTP_STONE ? m->bn[b] : 0);
        }
    } else {
        for (int i = 0; i < m->nw; i++)
            n += sprintf(o + n, " w%02x%02x%02x%02x", m->w[i].ax, m->w[i].ay, m->w[i].bx, m->w[i].by);
        for (int i = 0; i < m->np; i++)
            n += sprintf(o + n, " p%02x%02x%02x%02x", m->p[i].ax, m->p[i].ay, m->p[i].bx, m->p[i].by);
        for (int i = 0; i < m->nf; i++)
            n += sprintf(o + n, " f%c%c%02x%02x%02x%02x%02x", KIND_CH[m->f[i].kind], '0' + m->f[i].col, m->f[i].n,
                         m->f[i].x0, m->f[i].y0, m->f[i].x1, m->f[i].y1);
    }
    if (m->nsol) {
        o[n++] = ' ';
        o[n++] = 's';
        for (int k = 0; k < m->nsol; k++) {
            if (k) o[n++] = ',';
            for (int i = 0; i < m->sollen[k]; i++) o[n++] = (char)('0' + m->sol[k][i]);
        }
    }
    o[n] = 0;
}

/* ---- whole-number geometry ----------------------------------------------------------------------------------------- */
static int32_t isqrt64(int64_t v)
{
    if (v <= 0) return 0;
    int64_t r = (int64_t)sqrt((double)v);
    while (r * r > v) r--;
    while ((r + 1) * (r + 1) <= v) r++;
    return (int32_t)r;
}

static int32_t mini(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t maxi(int32_t a, int32_t b) { return a > b ? a : b; }

static void add_seg(ptp_world *w, int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t r)
{
    if (w->nseg >= PTP_MAXSEG) return;
    w->seg[w->nseg++] = (ptp_seg){ax, ay, bx, by, r, mini(ax, bx) - r, mini(ay, by) - r, maxi(ax, bx) + r,
                                  maxi(ay, by) + r};
}

static void add_u(ptp_world *w, int ax, int ay, int bx, int by) { add_seg(w, U(ax), U(ay), U(bx), U(by), PTP_WALL_R); }

/* the frame and the cups, the same for every level of a kind */
static void frame_and_cups(ptp_world *w, int cups)
{
    add_u(w, 0, 0, PTP_WU, 0);
    add_u(w, 0, 0, 0, PTP_HU);
    add_u(w, PTP_WU, 0, PTP_WU, PTP_HU);
    add_u(w, 0, PTP_HU, PTP_WU, PTP_HU);
    w->rim = U(80);
    if (cups == 1) {
        add_u(w, 0, 70, 9, 80);
        add_u(w, 64, 70, 55, 80);
        add_u(w, 9, 80, 14, 101);
        add_u(w, 14, 101, 50, 101);
        add_u(w, 50, 101, 55, 80);
        w->cx0[0] = U(9);
        w->cx1[0] = U(55);
    } else {
        add_u(w, 0, 72, 4, 80);
        add_u(w, 64, 72, 60, 80);
        add_u(w, 32, 72, 29, 80);
        add_u(w, 32, 72, 35, 80);
        add_u(w, 4, 80, 7, 101);
        add_u(w, 7, 101, 26, 101);
        add_u(w, 26, 101, 29, 80);
        add_u(w, 35, 80, 38, 101);
        add_u(w, 38, 101, 57, 101);
        add_u(w, 57, 101, 60, 80);
        w->cx0[0] = U(4);
        w->cx1[0] = U(29);
        w->cx0[1] = U(35);
        w->cx1[1] = U(60);
    }
}

/* the segments near each cell of the grid: within a ball's reach of any point of it */
static void cell_lists(ptp_world *w)
{
    int32_t reach = RAD[PTP_GEM] + 2 * SLACK + 64;
    for (int gy = 0; gy < PTP_GY; gy++)
        for (int gx = 0; gx < PTP_GX; gx++) {
            int32_t x0 = gx * PTP_CS - reach, y0 = gy * PTP_CS - reach, x1 = (gx + 1) * PTP_CS + reach,
                    y1 = (gy + 1) * PTP_CS + reach;
            int n = 0;
            for (int i = 0; i < w->nseg; i++) {
                const ptp_seg *s = &w->seg[i];
                if (s->x1 < x0 || s->x0 > x1 || s->y1 < y0 || s->y0 > y1) continue;
                /* the segment's distance to the cell's middle against the cell's half diagonal and the reach */
                int64_t mx = (x0 + x1) / 2, my = (y0 + y1) / 2, dx = s->bx - s->ax, dy = s->by - s->ay;
                int64_t l2 = dx * dx + dy * dy, t = (mx - s->ax) * dx + (my - s->ay) * dy, cx = s->ax, cy = s->ay;
                if (l2 > 0 && t > 0) {
                    if (t >= l2) { cx = s->bx; cy = s->by; }
                    else { cx = s->ax + dx * t / l2; cy = s->ay + dy * t / l2; }
                }
                int64_t ex = mx - cx, ey = my - cy, lim = (x1 - x0) * 3 / 4 + s->r;
                if (ex * ex + ey * ey > lim * lim) continue;
                if (n == PTP_CELLSEG) { n = 255; break; }
                w->cs[gy][gx][n++] = (uint8_t)i;
            }
            w->ncs[gy][gx] = (uint8_t)n;
        }
}

static int dist_ok(const ptp_world *w, const ptp_sim *s, int32_t x, int32_t y, int32_t r)
{
    for (int i = 0; i < w->nseg; i++) {
        const ptp_seg *g = &w->seg[i];
        int64_t dx = g->bx - g->ax, dy = g->by - g->ay, l2 = dx * dx + dy * dy, t = (x - g->ax) * dx + (y - g->ay) * dy;
        int64_t cx = g->ax, cy = g->ay;
        if (l2 > 0 && t > 0) {
            if (t >= l2) { cx = g->bx; cy = g->by; }
            else { cx = g->ax + dx * t / l2; cy = g->ay + dy * t / l2; }
        }
        int64_t ex = x - cx, ey = y - cy, R = r + g->r + 24;
        if (ex * ex + ey * ey < R * R) return 0;
    }
    for (int i = 0; i < w->np; i++) {
        int64_t dx = w->pbx[i] - w->pax[i], dy = w->pby[i] - w->pay[i], l2 = dx * dx + dy * dy;
        int64_t t = (x - w->pax[i]) * dx + (y - w->pay[i]) * dy, cx = w->pax[i], cy = w->pay[i];
        if (l2 > 0 && t > 0) {
            if (t >= l2) { cx = w->pbx[i]; cy = w->pby[i]; }
            else { cx = w->pax[i] + dx * t / l2; cy = w->pay[i] + dy * t / l2; }
        }
        int64_t ex = x - cx, ey = y - cy, R = r + PTP_PIN_R + 24;
        if (ex * ex + ey * ey < R * R) return 0;
    }
    for (int i = 0; i < s->nb; i++) {
        int64_t ex = x - s->b[i].x, ey = y - s->b[i].y, R = r + RAD[s->b[i].kind] + 8;
        if (ex * ex + ey * ey < R * R) return 0;
    }
    return 1;
}

/* a box filled from its bottom up, row by row (every other row set half a ball over), where nothing is in the way */
static void pack(const ptp_world *w, ptp_sim *s, const ptp_fill *f)
{
    int32_t r = RAD[f->kind], sp = 2 * r + 40, rowh = sp * 222 / 256;
    int32_t x0 = f->x0 * HALF, y0 = f->y0 * HALF, x1 = f->x1 * HALF, y1 = f->y1 * HALF;
    int placed = 0;
    for (int row = 0; placed < f->n; row++) {
        int32_t y = y1 - r - 48 - row * rowh;
        if (y < y0 + r) break;
        for (int32_t x = x0 + r + 48 + (row & 1 ? sp / 2 : 0); x <= x1 - r - 48 && placed < f->n; x += sp) {
            if (s->nb >= PTP_MAXB) return;
            if (!dist_ok(w, s, x, y, r)) continue;
            s->b[s->nb++] = (ptp_ball){x, y, x, y, x, y, f->kind, f->col, 1, 0, 0};
            placed++;
        }
    }
}

/* ---- the physics --------------------------------------------------------------------------------------------------- */
static void event(ptp_sim *s, int type, int col, int32_t x, int32_t y)
{
    ptp_event *e = &s->ev[s->nev++ % PTP_EVENTS];
    *e = (ptp_event){(uint8_t)type, (uint8_t)col, (int16_t)(x / 16), (int16_t)(y / 16), s->tick};
}

static void lose(ptp_sim *s, int why, int32_t x, int32_t y)
{
    if (s->end != PTP_PLAYING) return;
    s->end = why;
    s->fx = x;
    s->fy = y;
}

int ptp_pin_now(const ptp_world *w, const ptp_sim *s, int i, int t, int32_t *ax, int32_t *ay, int32_t *bx,
                int32_t *by, int *out)
{
    int64_t u = 0;
    if (s->pulled[i] >= 0) {
        u = (int64_t)(t - s->pulled[i]) * 1024 / PTP_PULL;
        if (u < 0) u = 0;
        if (u >= 1024) {
            *out = 1024;
            return 0;
        }
    }
    int64_t e = u * u * (3072 - 2 * u) / (1024 * 1024);    /* eased: slow, fast, slow */
    int64_t len = w->plen[i] > 0 ? w->plen[i] : 1, move = (len + U(4)) * e / 1024;
    int32_t dx = (int32_t)((w->pax[i] - w->pbx[i]) * move / len), dy = (int32_t)((w->pay[i] - w->pby[i]) * move / len);
    *ax = w->pax[i] + dx;
    *ay = w->pay[i] + dy;
    *bx = w->pbx[i] + dx;
    *by = w->pby[i] + dy;
    *out = (int)u;
    return 1;
}

/* a ball out of a round-ended bar (ax, ay)..(bx, by) of half thickness sr; fric: also rubbed along it. 1: touched */
static int seg_push(ptp_ball *b, int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t sr, int fric)
{
    int32_t R = RAD[b->kind] + sr;
    if (b->x < mini(ax, bx) - R || b->x > maxi(ax, bx) + R || b->y < mini(ay, by) - R || b->y > maxi(ay, by) + R)
        return 0;
    int64_t dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, t = (int64_t)(b->x - ax) * dx + (int64_t)(b->y - ay) * dy;
    int64_t cx = ax, cy = ay;
    if (l2 > 0 && t > 0) {
        if (t >= l2) { cx = bx; cy = by; }
        else { cx = ax + dx * t / l2; cy = ay + dy * t / l2; }
    }
    int64_t ex = b->x - cx, ey = b->y - cy, d2 = ex * ex + ey * ey;
    if (d2 >= (int64_t)R * R) return 0;
    int32_t d = isqrt64(d2);
    if (d == 0) { ex = 0; ey = -1; d = 1; }     /* on the line itself: up */
    int32_t push = R - d;
    b->x += (int32_t)(ex * push / d);
    b->y += (int32_t)(ey * push / d);
    if (fric) {
        int64_t vx = b->x - b->px, vy = b->y - b->py, vn = (vx * ex + vy * ey) / d;
        int64_t tx = vx - vn * ex / d, ty = vy - vn * ey / d;
        b->px += (int32_t)(tx / fric);
        b->py += (int32_t)(ty / fric);
    }
    return 1;
}

static int hot(const ptp_ball *b) { return b->kind == PTP_LAVA; }
static int ball(const ptp_ball *b) { return b->kind == PTP_GEM || b->kind == PTP_DULL; }

/* two things touching: a ball burnt by lava, a grey ball coloured by a gem, lava cooled by water or stone */
static void touch(ptp_sim *s, ptp_ball *a, ptp_ball *c)
{
    ptp_ball *p[2] = {a, c};
    for (int k = 0; k < 2; k++) {
        ptp_ball *x = p[k], *y = p[1 - k];
        if (hot(x) && ball(y) && y->live) {
            y->live = 0;
            event(s, PTP_E_BURN, y->col, y->x, y->y);
            lose(s, PTP_BURNT, y->x, y->y);
        } else if (x->kind == PTP_GEM && y->kind == PTP_DULL) {
            y->kind = PTP_GEM;
            y->col = x->col;
            event(s, PTP_E_COLOUR, y->col, y->x, y->y);
        } else if (hot(x) && !x->cool && (y->kind == PTP_WATER || y->kind == PTP_STONE))
            x->cool = COOL_TICKS;
    }
}

static void enter_cup(const ptp_world *w, ptp_sim *s, ptp_ball *b, int k)
{
    b->cup = (uint8_t)(k + 1);
    if (b->kind == PTP_LAVA) lose(s, PTP_LAVA_IN, b->x, b->y);
    else if (b->kind == PTP_DULL) lose(s, PTP_GREY_IN, b->x, b->y);
    else if (b->kind == PTP_GEM) {
        if (w->cupcol[k] >= 0 && b->col != w->cupcol[k]) lose(s, PTP_WRONG_CUP, b->x, b->y);
        else s->incup++;
        event(s, PTP_E_CUP, b->col, b->x, b->y);
    }
}

void ptp_step(const ptp_world *w, ptp_sim *s)
{
    s->tick++;
    int32_t maxv = 0;
    for (int i = 0; i < s->nb; i++) {           /* on: the speed kept, damped, gravity */
        ptp_ball *b = &s->b[i];
        if (!b->live) continue;
        int32_t vx = b->x - b->px, vy = b->y - b->py;
        vx -= vx / DAMP[b->kind];
        vy -= vy / DAMP[b->kind];
        vx = vx > VMAX ? VMAX : vx < -VMAX ? -VMAX : vx;
        vy = vy > VMAX ? VMAX : vy < -VMAX ? -VMAX : vy;
        int32_t m = maxi(vx < 0 ? -vx : vx, vy < 0 ? -vy : vy);
        if (m > maxv && !b->cup) maxv = m;
        b->px = b->x;
        b->py = b->y;
        b->x += vx;
        b->y += vy + GRAV;
    }
    int32_t pin[PTP_MAXPIN][4];
    int on[PTP_MAXPIN], moving = 0;
    for (int i = 0; i < w->np; i++) {
        int out;
        on[i] = ptp_pin_now(w, s, i, s->tick, &pin[i][0], &pin[i][1], &pin[i][2], &pin[i][3], &out);
        if (s->pulled[i] >= 0 && out < 1024) moving = 1;
    }
    /* the grid: each ball in the cell of its middle */
    int16_t head[PTP_GY * PTP_GX], next[PTP_MAXB];
    uint8_t cx[PTP_MAXB], cy[PTP_MAXB];
    for (int c = 0; c < PTP_GY * PTP_GX; c++) head[c] = -1;
    for (int i = 0; i < s->nb; i++) {
        if (!s->b[i].live) continue;
        int gx = s->b[i].x / PTP_CS, gy = s->b[i].y / PTP_CS;
        gx = gx < 0 ? 0 : gx >= PTP_GX ? PTP_GX - 1 : gx;
        gy = gy < 0 ? 0 : gy >= PTP_GY ? PTP_GY - 1 : gy;
        cx[i] = (uint8_t)gx;
        cy[i] = (uint8_t)gy;
        next[i] = head[gy * PTP_GX + gx];
        head[gy * PTP_GX + gx] = (int16_t)i;
    }
    for (int it = 0; it < 3; it++) {
        for (int i = 0; i < s->nb; i++) {       /* the balls pushed apart */
            ptp_ball *a = &s->b[i];
            if (!a->live) continue;
            for (int oy = -1; oy <= 1; oy++)
                for (int ox = -1; ox <= 1; ox++) {
                    int gx = cx[i] + ox, gy = cy[i] + oy;
                    if (gx < 0 || gy < 0 || gx >= PTP_GX || gy >= PTP_GY) continue;
                    for (int j = head[gy * PTP_GX + gx]; j >= 0; j = next[j]) {
                        if (j <= i) continue;
                        ptp_ball *c = &s->b[j];
                        if (!c->live || !a->live) continue;
                        int32_t R = RAD[a->kind] + RAD[c->kind], RS = R + (it == 0 ? SLACK : 0);
                        int32_t dx = c->x - a->x, dy = c->y - a->y;
                        if (dx >= RS || dx <= -RS || dy >= RS || dy <= -RS) continue;
                        int64_t d2 = (int64_t)dx * dx + (int64_t)dy * dy;
                        if (d2 >= (int64_t)RS * RS) continue;
                        if (it == 0) {
                            touch(s, a, c);
                            if (!a->live || !c->live) continue;
                        }
                        if (d2 >= (int64_t)R * R) continue;
                        int32_t d = isqrt64(d2);
                        if (d == 0) { dx = 0; dy = 1; d = 1; }
                        int64_t push = R - d, ma = MASS[a->kind], mc = MASS[c->kind];
                        int64_t mx = dx * push / d, my = dy * push / d;
                        a->x -= (int32_t)(mx * mc / (ma + mc));
                        a->y -= (int32_t)(my * mc / (ma + mc));
                        c->x += (int32_t)(mx * ma / (ma + mc));
                        c->y += (int32_t)(my * ma / (ma + mc));
                    }
                }
        }
        int last = it == 2;
        for (int i = 0; i < s->nb; i++) {       /* out of the walls and the pins */
            ptp_ball *b = &s->b[i];
            if (!b->live) continue;
            int gx = b->x / PTP_CS, gy = b->y / PTP_CS;
            gx = gx < 0 ? 0 : gx >= PTP_GX ? PTP_GX - 1 : gx;
            gy = gy < 0 ? 0 : gy >= PTP_GY ? PTP_GY - 1 : gy;
            int n = w->ncs[gy][gx], f = last ? FRIC[b->kind] : 0;
            if (n == 255)
                for (int k = 0; k < w->nseg; k++) {
                    const ptp_seg *g = &w->seg[k];
                    seg_push(b, g->ax, g->ay, g->bx, g->by, g->r, f);
                }
            else
                for (int k = 0; k < n; k++) {
                    const ptp_seg *g = &w->seg[w->cs[gy][gx][k]];
                    seg_push(b, g->ax, g->ay, g->bx, g->by, g->r, f);
                }
            for (int k = 0; k < w->np; k++)
                if (on[k]) seg_push(b, pin[k][0], pin[k][1], pin[k][2], pin[k][3], PTP_PIN_R, f);
        }
    }
    int cooling = 0;
    for (int i = 0; i < s->nb; i++) {           /* lava cooling, the cups */
        ptp_ball *b = &s->b[i];
        if (!b->live) continue;
        if (b->cool) {
            cooling = 1;
            if (--b->cool == 0) {
                b->kind = PTP_STONE;
                event(s, PTP_E_STONE, 0, b->x, b->y);
            }
        }
        if (!b->cup && b->y > w->rim)
            for (int k = 0; k < w->cups; k++)
                if (b->x > w->cx0[k] && b->x < w->cx1[k]) enter_cup(w, s, b, k);
    }
    if (maxv <= REST_V && !moving && !cooling) s->calm++;
    else s->calm = 0;
    if (s->tick % WINDOW == 0) {               /* the windows: what jiggles in place without going anywhere */
        int32_t far = 0;
        for (int i = 0; i < s->nb; i++) {
            ptp_ball *b = &s->b[i];
            int32_t d = maxi(abs(b->x - b->wx), abs(b->y - b->wy));
            if (b->live && !b->cup && d > far) far = d;
            b->wx = b->x;
            b->wy = b->y;
        }
        s->still = far <= WINDOW_D && !moving && !cooling ? s->still + 1 : 0;
    }
    if (s->calm >= REST_TICKS || s->still >= 3 || s->tick - s->last >= QUIET_MAX) {
        s->rest = 1;
        if (s->end == PTP_PLAYING) {
            if (s->incup >= s->target) s->end = PTP_WON;
            else if (s->npulled == w->np) {
                for (int i = 0; i < s->nb; i++)    /* where: a ball left out */
                    if (s->b[i].live && ball(&s->b[i]) && !s->b[i].cup) {
                        s->fx = s->b[i].x;
                        s->fy = s->b[i].y;
                        break;
                    }
                s->end = PTP_STUCK;
            }
        }
    }
}

void ptp_settle(const ptp_world *w, ptp_sim *s)
{
    while (!s->rest && s->end == PTP_PLAYING) ptp_step(w, s);
}

int ptp_pull(const ptp_world *w, ptp_sim *s, int pin)
{
    if (pin < 0 || pin >= w->np || s->pulled[pin] >= 0 || s->end != PTP_PLAYING) return 0;
    s->pulled[pin] = s->tick;
    s->order[s->npulled++] = (uint8_t)pin;
    s->last = s->tick;
    s->rest = 0;
    s->calm = s->still = 0;
    return 1;
}

void ptp_build(const ptp_map *m, ptp_world *w, ptp_sim *s)
{
    memset(w, 0, sizeof *w);
    memset(s, 0, sizeof *s);
    w->cups = m->cups;
    w->cupcol[0] = m->cupcol[0];
    w->cupcol[1] = m->cupcol[1];
    frame_and_cups(w, m->cups);
    for (int i = 0; i < m->nw; i++)
        add_seg(w, m->w[i].ax * HALF, m->w[i].ay * HALF, m->w[i].bx * HALF, m->w[i].by * HALF, PTP_WALL_R);
    cell_lists(w);
    w->np = m->np;
    for (int i = 0; i < m->np; i++) {
        w->pax[i] = m->p[i].ax * HALF;
        w->pay[i] = m->p[i].ay * HALF;
        w->pbx[i] = m->p[i].bx * HALF;
        w->pby[i] = m->p[i].by * HALF;
        int64_t dx = w->pbx[i] - w->pax[i], dy = w->pby[i] - w->pay[i];
        w->plen[i] = isqrt64(dx * dx + dy * dy);
        s->pulled[i] = -1;
    }
    for (int i = 0; i < m->nf; i++) pack(w, s, &m->f[i]);
    for (int i = 0; i < s->nb; i++)
        if (ball(&s->b[i])) s->target++;
    ptp_settle(w, s);                           /* the things settle on their pins before the first pull */
    s->nev = 0;
    s->last = s->tick;
}

/* ---- orders ----------------------------------------------------------------------------------------------------- */
int ptp_naive(const ptp_map *m, uint8_t *order)
{
    int n = m->np;
    for (int i = 0; i < n; i++) order[i] = (uint8_t)i;
    for (int i = 1; i < n; i++)                 /* by the pin's middle: from the top, then from the left */
        for (int j = i; j > 0; j--) {
            const ptp_line *a = &m->p[order[j - 1]], *b = &m->p[order[j]];
            int ay = a->ay + a->by, by = b->ay + b->by, ax = a->ax + a->bx, bx = b->ax + b->bx;
            if (ay > by || (ay == by && ax > bx)) {
                uint8_t t = order[j];
                order[j] = order[j - 1];
                order[j - 1] = t;
            } else
                break;
        }
    return n;
}

int ptp_try(const ptp_map *m, const uint8_t *order, int n, ptp_sim *out)
{
    static ptp_world w;
    ptp_sim s;
    ptp_build(m, &w, &s);
    for (int i = 0; i < n && s.end == PTP_PLAYING; i++) {
        ptp_pull(&w, &s, order[i]);
        ptp_settle(&w, &s);
    }
    if (out) *out = s;
    return s.end;
}

static void keep(ptp_report *r, const uint8_t *ord, int len)
{
    int at = r->nkept;
    if (at == PTP_MAXSOL) {                     /* full: in place of the longest, if longer */
        at = 0;
        for (int k = 1; k < PTP_MAXSOL; k++)
            if (r->len[k] > r->len[at]) at = k;
        if (r->len[at] <= len) return;
    } else
        r->nkept++;
    memcpy(r->ord[at], ord, (size_t)len);
    r->len[at] = (uint8_t)len;
}

static void dfs(const ptp_world *w, const ptp_sim *s, int depth, uint8_t *ord, ptp_report *r, double share)
{
    int left = w->np - depth;
    for (int i = 0; i < w->np; i++) {
        if (s->pulled[i] >= 0) continue;
        ptp_sim t = *s;
        ptp_pull(w, &t, i);
        ptp_settle(w, &t);
        r->settles++;
        ord[depth] = (uint8_t)i;
        if (t.end == PTP_WON) {
            r->wins++;
            r->share += share / left;
            keep(r, ord, depth + 1);
        } else if (t.end == PTP_PLAYING)
            dfs(w, &t, depth + 1, ord, r, share / left);
    }
}

void ptp_search(const ptp_map *m, ptp_report *r)
{
    static ptp_world w;
    ptp_sim s;
    memset(r, 0, sizeof *r);
    ptp_build(m, &w, &s);
    uint8_t ord[PTP_MAXPIN], nv[PTP_MAXPIN];
    if (s.end == PTP_PLAYING) dfs(&w, &s, 0, ord, r, 1.0);
    for (int a = 0; a < r->nkept; a++)          /* the shortest first */
        for (int b = a + 1; b < r->nkept; b++)
            if (r->len[b] < r->len[a] || (r->len[b] == r->len[a] && memcmp(r->ord[b], r->ord[a], r->len[a]) < 0)) {
                uint8_t t[PTP_MAXPIN], tl = r->len[a];
                memcpy(t, r->ord[a], PTP_MAXPIN);
                memcpy(r->ord[a], r->ord[b], PTP_MAXPIN);
                memcpy(r->ord[b], t, PTP_MAXPIN);
                r->len[a] = r->len[b];
                r->len[b] = tl;
            }
    int n = ptp_naive(m, nv);
    r->naive = s.end != PTP_PLAYING ? s.end : ptp_try(m, nv, n, NULL);
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void ptp_begin(ptp_game *g)
{
    ptp_build(&g->m, &g->w, &g->s0);
    g->s = g->s0;
    g->hints = g->restarts = 0;
}

int ptp_hint(const ptp_game *g)
{
    const ptp_sim *s = &g->s;
    if (s->end != PTP_PLAYING) return -2;
    for (int k = 0; k < g->m.nsol; k++) {
        if (g->m.sollen[k] <= s->npulled || memcmp(g->m.sol[k], s->order, (size_t)s->npulled)) continue;
        return g->m.sol[k][s->npulled];
    }
    return s->npulled ? -1 : -2;
}

int ptp_script(ptp_game *g, const char *mv)
{
    while (*mv) {
        while (*mv == ' ') mv++;
        if (!*mv) break;
        if (mv[0] == 'p' && mv[1] >= '0' && mv[1] <= '9') {
            if (!ptp_pull(&g->w, &g->s, mv[1] - '0')) return 0;
            ptp_settle(&g->w, &g->s);
            mv += 2;
        } else if (mv[0] == 'r') {
            g->s = g->s0;
            g->restarts++;
            mv++;
        } else if (mv[0] == 'h') {
            int p = ptp_hint(g);
            if (p == -1) {
                g->s = g->s0;
                g->restarts++;
                p = ptp_hint(g);
            }
            if (p < 0 || !ptp_pull(&g->w, &g->s, p)) return 0;
            g->hints++;
            ptp_settle(&g->w, &g->s);
            mv++;
        } else
            return 0;
    }
    return 1;
}

/* ---- the templates: hand-drawn chambers in units ("w" a wall, "p" a pin from its ring, "s" a box to fill) -------- */
const ptp_tpl ptp_tpls[PTP_TPLS] = {
    /* 0 a box */
    {"w 20 34 20 58|w 44 34 44 58|p 14 59 45 59|s 21 35 43 58", 1, 0},
    /* 1 two boxes stacked */
    {"w 18 8 18 35|w 18 37 18 62|w 46 8 46 62|p 12 36 47 36|p 52 63 17 63|s 19 9 45 35|s 19 37 45 62", 1, 0},
    /* 2 three boxes stacked */
    {"w 18 4 18 25|w 18 27 18 65|w 46 4 46 45|w 46 47 46 65|p 12 26 47 26|p 52 46 17 46|p 12 66 47 66|"
     "s 19 5 45 25|s 19 27 45 45|s 19 47 45 65", 1, 0},
    /* 3 two towers side by side */
    {"w 8 12 8 61|w 32 12 32 61|w 56 12 56 61|p 4 62 32 62|p 60 62 32 62|s 9 13 31 61|s 33 13 55 61", 1, 0},
    /* 4 two towers of two boxes */
    {"w 8 8 8 35|w 8 37 8 61|w 56 8 56 35|w 56 37 56 61|w 32 8 32 61|p 4 36 32 36|p 60 36 32 36|p 4 62 32 62|"
     "p 60 62 32 62|s 9 9 31 35|s 33 9 55 35|s 9 37 31 61|s 33 37 55 61", 1, 1},
    /* 5 two towers over a hopper and its gate */
    {"w 8 6 8 39|w 32 6 32 40|w 56 6 56 39|p 4 40 32 40|p 60 40 32 40|w 8 41 26 58|w 56 41 38 58|p 17 60 39 60|"
     "s 9 7 31 39|s 33 7 55 39|s 27 52 37 59", 1, 1},
    /* 6 a zigzag: a box over a slide into a lower box, a box over that one */
    {"w 8 4 8 24|w 36 4 36 24|p 4 25 37 25|s 9 5 35 24|w 9 31 37 41|w 56 28 56 57|w 30 45 30 57|p 60 58 29 58|"
     "s 31 46 55 57|w 40 4 40 21|w 56 4 56 21|p 60 22 39 22|s 41 5 55 21", 1, 0},
    /* 7 a pocket sliding into a tower of two boxes, a box over the tower */
    {"w 56 30 56 49|w 56 51 56 66|w 26 41 26 66|p 60 50 25 50|p 60 67 25 67|w 8 6 8 24|w 24 6 24 24|p 4 25 25 25|"
     "s 9 7 23 24|w 8 30 30 38|w 30 4 30 25|w 52 4 52 25|p 58 26 29 26|s 31 5 51 25|s 27 41 55 49|s 27 51 55 66", 1, 1},
    /* 8 a V of two slanting pins over a box */
    {"w 10 6 10 29|w 54 6 54 29|w 32 6 32 44|p 5 30 32 45|p 59 30 32 45|w 10 31 17 50|w 54 31 47 50|w 17 51 17 65|"
     "w 47 51 47 65|p 11 66 48 66|s 11 7 31 29|s 33 7 53 29|s 18 52 46 65", 1, 0},
    /* 9 two side boxes sliding into a middle tower of two boxes */
    {"w 8 6 8 22|w 22 6 22 22|p 4 23 23 23|s 9 7 21 22|w 8 28 27 35|w 56 6 56 22|w 42 6 42 22|p 60 23 41 23|"
     "s 43 7 55 22|w 56 28 37 35|w 24 38 24 66|w 40 38 40 47|w 40 49 40 66|p 46 48 23 48|p 18 67 41 67|"
     "s 25 39 39 47|s 25 49 39 66", 1, 1},
    /* 10 two cups: two boxes, a slanting pin sends the right one's fall to the left cup */
    {"w 8 4 8 21|w 28 4 28 21|p 4 22 29 22|s 9 5 27 21|w 36 4 36 21|w 56 4 56 21|p 60 22 35 22|s 37 5 55 21|"
     "p 60 32 24 50", 2, 3},
    /* 11 two cups: a box over a slanting pin to the right cup, a slide under it to the left one, a small box */
    {"w 20 4 20 22|w 44 4 44 22|p 14 23 45 23|s 21 5 43 22|p 8 29 56 49|w 50 52 22 63|w 48 4 48 18|w 58 4 58 18|"
     "p 60 19 47 19|s 49 5 57 18", 2, 3},
    /* 12 two cups: two boxes over two slanting pins that swap their falls */
    {"w 8 4 8 20|w 26 4 26 20|p 4 21 27 21|s 9 5 25 20|w 38 4 38 20|w 56 4 56 20|p 60 21 37 21|s 39 5 55 20|"
     "p 4 27 40 45|p 60 50 24 66", 2, 3},
    /* 13 two cups: two towers of two boxes */
    {"w 8 8 8 33|w 8 35 8 58|w 56 8 56 33|w 56 35 56 58|w 32 8 32 58|p 4 34 32 34|p 60 34 32 34|p 4 59 32 59|"
     "p 60 59 32 59|s 9 9 31 33|s 33 9 55 33|s 9 35 31 58|s 33 35 55 58", 2, 3},
    /* 14 two cups: a tower of two boxes over a slanting pin to the left cup, a slide to the right one under it */
    {"w 22 6 22 25|w 22 27 22 45|w 42 6 42 45|p 16 26 43 26|p 48 46 21 46|s 23 7 41 25|s 23 27 41 45|p 50 50 12 64|"
     "w 20 66 46 72", 2, 3},
    /* 15 three boxes in a row of steps, each over a slide into the next */
    {"w 8 4 8 20|w 24 4 24 20|p 4 21 25 21|s 9 5 23 20|w 8 26 30 33|w 22 34 22 42|w 42 24 42 42|p 46 43 21 43|"
     "s 23 34 41 42|w 22 48 46 55|w 40 57 40 64|w 58 40 58 64|p 61 65 39 65|s 41 56 57 64", 1, 1},
    /* 16 a wide box over two slanting pins (a roof's two sides) above two towers */
    {"w 8 6 8 27|w 56 6 56 27|p 4 28 32 38|p 60 28 32 38|w 8 29 8 61|w 56 29 56 61|w 32 40 32 61|p 4 62 32 62|"
     "p 60 62 32 62|s 9 7 55 27|s 9 44 31 61|s 33 44 55 61", 1, 1},
    /* 17 two cups: a tower of three boxes over a slanting pin to the left cup, a slide to the right one under it */
    {"w 22 4 22 19|w 22 21 22 49|w 42 4 42 35|w 42 37 42 49|p 16 20 43 20|p 48 36 21 36|p 16 50 43 50|s 23 5 41 19|"
     "s 23 21 41 35|s 23 37 41 49|p 50 54 12 66|w 20 68 46 73", 2, 3},
};

static int tpl_count(int t, char what)
{
    int n = 0;
    for (const char *p = ptp_tpls[t].geo; *p; p++)
        if (*p == what && (p == ptp_tpls[t].geo || p[-1] == '|')) n++;
    return n;
}

int ptp_tpl_boxes(int t) { return tpl_count(t, 's'); }
int ptp_tpl_pins(int t) { return tpl_count(t, 'p'); }

/* a box's room: how many of a kind it holds packed */
static int room(int kind, int x0, int y0, int x1, int y1)
{
    int32_t r = RAD[kind], sp = 2 * r + 40, rowh = sp * 222 / 256, w = (x1 - x0) * HALF - 2 * r - 96;
    int32_t h = (y1 - y0) * HALF - 2 * r - 48;
    if (w < 0 || h < 0) return 0;
    int cols = w / sp + 1, rows = h / rowh + 1;
    return cols * rows - rows / 2;
}

/* the template's parts one by one: what ('w' 'p' 's') and where (half units, mirrored); the next place, NULL at the end */
static const char *tpl_next(const char *p, int mirror, char *what, int *v)
{
    while (*p == ' ' || *p == '|') p++;
    if (!*p) return NULL;
    *what = *p++;
    for (int i = 0; i < 4; i++) {
        char *e;
        v[i] = (int)strtol(p, &e, 10) * 2;
        p = e;
    }
    if (mirror) {
        v[0] = PTP_WU * 2 - v[0];
        v[2] = PTP_WU * 2 - v[2];
        if (*what == 's') { int x = v[0]; v[0] = v[2]; v[2] = x; }
    }
    return p;
}

int ptp_tpl_room(int t, int box, int kind)
{
    char what;
    int v[4], k = 0;
    for (const char *p = ptp_tpls[t].geo; (p = tpl_next(p, 0, &what, v));)
        if (what == 's' && k++ == box) return room(kind, v[0], v[1], v[2], v[3]);
    return 0;
}

int ptp_from_tpl(ptp_map *m, int t, int mirror, const int *kinds, const int *cols, const int *counts,
                 const int *cupcol)
{
    memset(m, 0, sizeof *m);
    if (t < 0 || t >= PTP_TPLS) return 0;
    m->tpl = t;
    m->mirror = mirror != 0;
    m->cups = ptp_tpls[t].cups;
    m->cupcol[0] = m->cupcol[1] = -1;
    for (int k = 0; k < m->cups; k++) m->cupcol[k] = (int8_t)(cupcol ? cupcol[k] : -1);
    char what;
    int v[4];
    for (const char *p = ptp_tpls[t].geo; (p = tpl_next(p, mirror, &what, v));) {
        ptp_line l = {(int16_t)v[0], (int16_t)v[1], (int16_t)v[2], (int16_t)v[3]};
        if (what == 'w' && m->nw < PTP_MAXWALL) m->w[m->nw++] = l;
        else if (what == 'p' && m->np < PTP_MAXPIN) m->p[m->np++] = l;
        else if (what == 's' && m->nbox < PTP_MAXFILL) {
            int b = m->nbox++, k = kinds[b], n = counts[b];
            m->bkind[b] = (uint8_t)(k >= 0 && k < PTP_STONE && n > 0 ? k : 255);
            m->bcol[b] = (uint8_t)(cols ? cols[b] : 0);
            m->bn[b] = (uint8_t)(n > 0 ? n : 0);
            if (m->bkind[b] != 255)
                m->f[m->nf++] = (ptp_fill){(uint8_t)k, m->bcol[b], (uint8_t)n, l.ax, l.ay, l.bx, l.by};
        }
    }
    return m->nf > 0;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
/* a chapter's things: 0 gems; 1 gems, water and lava; 2 grey balls (water and lava too); 3 two cups; 4 everything, one cup; 5
 * everything */
int ptp_make(ptp_map *m, int ch, uint32_t *rs)
{
    int tries = 0;
again:
    if (++tries > 200) return 0;
    int t, two = ch == 3 ? 1 : ch == 5 ? pr_below(rs, 3) == 0 : 0;
    do t = pr_below(rs, PTP_TPLS);
    while (ptp_tpls[t].chapter_min > (ch > 3 ? 3 : ch) || (ptp_tpls[t].cups == 2) != two ||
           (ch == 0 && ptp_tpl_pins(t) > 3));
    int nb = ptp_tpl_boxes(t), kinds[PTP_MAXFILL], cols[PTP_MAXFILL], counts[PTP_MAXFILL], cupcol[2];
    int gemcol = pr_below(rs, 4), other = (gemcol + 1 + pr_below(rs, 3)) % 4;
    cupcol[0] = gemcol;
    cupcol[1] = other;
    if (!two) cupcol[0] = cupcol[1] = -1;
    int gems = 0, dull = 0, water = 0, lava = 0, colours = 0;
    for (int b = 0; b < nb; b++) {
        int r = pr_below(rs, 100), k;
        if (ch == 0) k = r < 75 ? PTP_GEM : -1;
        else if (ch == 1) k = r < 34 ? PTP_GEM : r < 62 ? PTP_LAVA : r < 92 ? PTP_WATER : -1;
        else if (ch == 2) k = r < 32 ? PTP_GEM : r < 64 ? PTP_DULL : r < 78 ? PTP_LAVA : r < 94 ? PTP_WATER : -1;
        else if (ch == 3) k = r < 50 ? PTP_GEM : r < 65 ? PTP_DULL : r < 77 ? PTP_LAVA : r < 90 ? PTP_WATER : -1;
        else k = r < 30 ? PTP_GEM : r < 45 ? PTP_DULL : r < 70 ? PTP_LAVA : r < 92 ? PTP_WATER : -1;
        int full = k == PTP_WATER || k == PTP_LAVA ? 25 + pr_below(rs, 40) : 30 + pr_below(rs, 50);
        if (ch == 0 && nb == 1) full = 45 + pr_below(rs, 40);
        kinds[b] = k;
        cols[b] = two && pr_below(rs, 2) ? other : gemcol;
        counts[b] = k < 0 ? 0 : ptp_tpl_room(t, b, k) * full / 100;
        if (k >= 0 && counts[b] < 2) counts[b] = 2;
        if (counts[b] > 48) counts[b] = 48;
        gems += k == PTP_GEM;
        dull += k == PTP_DULL;
        water += k == PTP_WATER;
        lava += k == PTP_LAVA;
        if (k == PTP_GEM) colours |= 1 << cols[b];
    }
    int vol = 0;                                /* what the cups take (square units, packed) */
    for (int b = 0; b < nb; b++)
        if (kinds[b] >= 0) vol += counts[b] * (kinds[b] <= PTP_DULL ? 25 : 9);
    if (vol > (two ? 480 : 560)) goto again;
    if (!gems) goto again;
    if (ch == 2 && !dull) goto again;
    if ((ch == 1 || ch == 4) && (!lava || !water)) goto again;
    if ((ch == 2 || ch == 3) && lava && !water) goto again;
    if (ch == 4 && !dull && pr_below(rs, 2)) goto again;
    if (two && colours != ((1 << gemcol) | (1 << other))) goto again;
    if (ch == 5 && dull + water + lava == 0) goto again;
    return ptp_from_tpl(m, t, pr_below(rs, 2), kinds, cols, counts, cupcol);
}

int ptp_judge(ptp_map *m, int ch, ptp_report *r)
{
    static ptp_world w;
    ptp_sim s;
    ptp_build(m, &w, &s);
    if (s.end != PTP_PLAYING || s.target < 1) return 0;
    for (int i = 0; i < s.nb; i++)
        if (s.b[i].cup) return 0;               /* something in a cup before the first pull */
    ptp_search(m, r);
    if (!r->wins) return 0;
    m->nsol = r->nkept;
    for (int k = 0; k < r->nkept; k++) {
        memcpy(m->sol[k], r->ord[k], PTP_MAXPIN);
        m->sollen[k] = r->len[k];
    }
    return ch == 0 || r->naive != PTP_WON;
}

/* ---- levels and puzzles at random -------------------------------------------------------------------------------- */
static const int CH_LEN[PTP_CHAPTERS] = {3, 30, 25, 18, 32, 25};   /* the first: three, every order wins */

int ptp_chapter_of(int level)
{
    for (int c = 0, at = 0; c < PTP_CHAPTERS; c++) {
        at += CH_LEN[c];
        if (level < at) return c;
    }
    return PTP_CHAPTERS - 1;
}

#ifndef PTP_GEN
int ptp_level(ptp_game *g, int level)
{
    if (level < 0 || level >= PTP_LEVELS || !ptp_parse(&g->m, ptp_levels[level].text)) return 0;
    ptp_begin(g);
    return 1;
}
#endif

void ptp_random(ptp_game *g, int variant, uint32_t seed)
{
    static const int CHS[PTP_VARIANTS][2] = {{0, 1}, {2, 3}, {4, 5}};
    uint32_t rs = pr_seed(seed);
    variant = variant < 0 ? 0 : variant >= PTP_VARIANTS ? PTP_VARIANTS - 1 : variant;
    for (int tries = 0; tries < 60; tries++) {
        int ch = CHS[variant][pr_below(&rs, 2)];
        ptp_report r;
        if (!ptp_make(&g->m, ch, &rs)) continue;
        if (variant > 0 && g->m.np < 3) continue;
        if (ptp_judge(&g->m, ch, &r) && (variant == 0 || r.share < (variant == 1 ? 0.5 : 0.3))) {
            ptp_begin(g);
            return;
        }
    }
#ifndef PTP_GEN
    ptp_level(g, variant * 50 + 10);
#endif
}

#ifndef PTP_GEN
#include "games/pins_levels.h"
#endif
