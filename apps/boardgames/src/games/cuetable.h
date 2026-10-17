/*
 * The physics of the table games (carrom, pool), no SDL: discs and balls rolling on a flat table, slowed by friction,
 * bouncing off each other (elastic, with a little loss) and off the cushions, dropping into pockets. A fixed step
 * (CUE_HZ a second), so the same shot always ends the same way: the rules run it to the end at once, the screen runs
 * the same steps as it draws them, and the bots and the tests try shots by running them.
 * Prefix cue_. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The table: 0..w across, 0..h down (y down, as the screen). Its cushions are its four edges, but for the pockets'
 * mouths (pool: stretches of an edge with no cushion, their two ends the jaws a ball can strike; a ball whose centre
 * passes the edge's line by `drop` falls in the nearest pocket) and the holes (carrom: a disc whose centre comes over
 * a hole falls in it). Angles: centi-degrees, 0 along +x, 9000 along +y (down the table).
 */
#ifndef CUETABLE_H
#define CUETABLE_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#define CUE_MAX 20                              /* balls or discs on a table, at most */
#define CUE_HZ 480                              /* steps a second */
#define CUE_DT (1.0f / CUE_HZ)
#define CUE_STOP 0.6f                           /* slower than this (units a second): at rest */
#define CUE_MAX_STEPS (CUE_HZ * 40)             /* a shot never runs longer */
#define CUE_PI 3.14159265f

enum { CUE_TOP, CUE_BOTTOM, CUE_LEFT, CUE_RIGHT };   /* the edges: y = 0, y = h, x = 0, x = w */

typedef struct {
    float w, h;
    float mu, drag;                             /* friction: a slowing of mu units/s^2 and drag (1/s) of the speed */
    float e_ball, e_wall;                       /* restitution: ball on ball, ball on a cushion */
    float slip;                                 /* the speed along a cushion kept by a bounce */
    int nholes;                                 /* carrom's holes: centre, radius */
    float hx[6], hy[6], hr[6];
    int nmouths;                                /* pool's mouths: an edge, from a to b along it (x or y) */
    struct { int edge; float a, b; } mouth[12];
    float drop;
    int npockets;                               /* pool: the pockets' centres (which one a ball fell in) */
    float px[6], py[6];
} cue_table;

typedef struct {
    float x, y, vx, vy, r, m;
    int down;                                   /* -1 on the table, else the pocket (or hole) it fell in */
} cue_ball;

typedef struct {
    int n;
    cue_ball b[CUE_MAX];
} cue_world;

/* what happened in a shot (ball 0 is the one struck: the cue ball, the striker) */
typedef struct {
    int first;                                  /* the first ball ball 0 touched (-1: none) */
    uint32_t rail;                              /* bit i: ball i touched a cushion */
    uint32_t rail_after;                        /* ... after ball 0's first contact */
    uint32_t moved;                             /* bit i: ball i moved */
    int npot;                                   /* the balls that fell, in order */
    int pot[CUE_MAX];
    int steps;
} cue_events;

static inline void cue_events_clear(cue_events *e)
{
    memset(e, 0, sizeof *e);
    e->first = -1;
}

static inline int cue_moving(const cue_world *w)
{
    for (int i = 0; i < w->n; i++)
        if (w->b[i].down < 0 && (w->b[i].vx != 0 || w->b[i].vy != 0)) return 1;
    return 0;
}

static inline int cue_in_mouth(const cue_table *t, int edge, float along)
{
    for (int k = 0; k < t->nmouths; k++)
        if (t->mouth[k].edge == edge && along > t->mouth[k].a && along < t->mouth[k].b) return 1;
    return 0;
}

static inline int cue_nearest_pocket(const cue_table *t, float x, float y)
{
    int best = 0;
    float bd = 1e30f;
    for (int k = 0; k < t->npockets; k++) {
        float d = (x - t->px[k]) * (x - t->px[k]) + (y - t->py[k]) * (y - t->py[k]);
        if (d < bd) { bd = d; best = k; }
    }
    return best;
}

static inline void cue_fall(cue_ball *b, int pocket, int i, cue_events *e)
{
    b->down = pocket;
    b->vx = b->vy = 0;
    if (e && e->npot < CUE_MAX) e->pot[e->npot++] = i;
}

/* a ball against a point (a jaw): pushed out and bounced off it */
static inline void cue_point(const cue_table *t, cue_ball *b, float px, float py, int i, cue_events *e)
{
    float dx = b->x - px, dy = b->y - py, d2 = dx * dx + dy * dy;
    if (d2 >= b->r * b->r || d2 <= 1e-12f) return;
    float d = sqrtf(d2), nx = dx / d, ny = dy / d, vn = b->vx * nx + b->vy * ny;
    b->x = px + nx * b->r;
    b->y = py + ny * b->r;
    if (vn < 0) {
        b->vx -= (1 + t->e_wall) * vn * nx;
        b->vy -= (1 + t->e_wall) * vn * ny;
    }
    if (e) e->rail |= 1u << i;
}

/* one step of CUE_DT; returns the hardest knock between two balls in it (for the clicks' loudness), 0 none */
static inline float cue_step(const cue_table *t, cue_world *w, cue_events *e)
{
    float knock = 0;
    int n = w->n;
    for (int i = 0; i < n; i++) {
        cue_ball *b = &w->b[i];
        if (b->down >= 0) continue;
        b->x += b->vx * CUE_DT;
        b->y += b->vy * CUE_DT;
    }
    for (int i = 0; i < n; i++) {               /* ball on ball */
        cue_ball *a = &w->b[i];
        if (a->down >= 0) continue;
        int am = a->vx != 0 || a->vy != 0;
        for (int j = i + 1; j < n; j++) {
            cue_ball *c = &w->b[j];
            if (c->down >= 0 || (!am && c->vx == 0 && c->vy == 0)) continue;
            float dx = c->x - a->x, dy = c->y - a->y, R = a->r + c->r, d2 = dx * dx + dy * dy;
            if (d2 >= R * R || d2 <= 1e-12f) continue;
            float d = sqrtf(d2), nx = dx / d, ny = dy / d;
            float vn = (a->vx - c->vx) * nx + (a->vy - c->vy) * ny;
            float ia = 1 / a->m, ic = 1 / c->m;
            if (vn > 0) {
                float J = (1 + t->e_ball) * vn / (ia + ic);
                a->vx -= J * ia * nx;
                a->vy -= J * ia * ny;
                c->vx += J * ic * nx;
                c->vy += J * ic * ny;
                if (vn > knock) knock = vn;
                if (e) {
                    if (e->first < 0 && (i == 0 || j == 0)) e->first = i == 0 ? j : i;
                    e->moved |= 1u << i | 1u << j;
                }
            }
            float over = R - d, sa = over * ia / (ia + ic), sc = over * ic / (ia + ic);
            a->x -= nx * sa;
            a->y -= ny * sa;
            c->x += nx * sc;
            c->y += ny * sc;
        }
    }
    for (int i = 0; i < n; i++) {               /* the cushions, the mouths, the pockets */
        cue_ball *b = &w->b[i];
        if (b->down >= 0) continue;
        float r = b->r;
        int hit = 0;
        if (b->x < r && !cue_in_mouth(t, CUE_LEFT, b->y)) {
            b->x = 2 * r - b->x;
            if (b->vx < 0) { b->vx = -b->vx * t->e_wall; b->vy *= t->slip; }
            hit = 1;
        } else if (b->x > t->w - r && !cue_in_mouth(t, CUE_RIGHT, b->y)) {
            b->x = 2 * (t->w - r) - b->x;
            if (b->vx > 0) { b->vx = -b->vx * t->e_wall; b->vy *= t->slip; }
            hit = 1;
        }
        if (b->y < r && !cue_in_mouth(t, CUE_TOP, b->x)) {
            b->y = 2 * r - b->y;
            if (b->vy < 0) { b->vy = -b->vy * t->e_wall; b->vx *= t->slip; }
            hit = 1;
        } else if (b->y > t->h - r && !cue_in_mouth(t, CUE_BOTTOM, b->x)) {
            b->y = 2 * (t->h - r) - b->y;
            if (b->vy > 0) { b->vy = -b->vy * t->e_wall; b->vx *= t->slip; }
            hit = 1;
        }
        if (hit && e) {
            e->rail |= 1u << i;
            if (e->first >= 0) e->rail_after |= 1u << i;
        }
        for (int k = 0; k < t->nmouths; k++) {  /* the jaws: the cushions' ends at a mouth */
            int ed = t->mouth[k].edge;
            float lx = ed == CUE_LEFT ? 0 : ed == CUE_RIGHT ? t->w : 0, ly = ed == CUE_TOP ? 0 : t->h;
            for (int s = 0; s < 2; s++) {
                float al = s ? t->mouth[k].b : t->mouth[k].a;
                float px = ed == CUE_LEFT || ed == CUE_RIGHT ? lx : al, py = ed == CUE_LEFT || ed == CUE_RIGHT ? al : ly;
                if (al <= 0 || (ed <= CUE_BOTTOM ? al >= t->w : al >= t->h)) continue;   /* a corner: no jaw */
                uint32_t had = e ? e->rail : 0;
                cue_point(t, b, px, py, i, e);
                if (e && e->rail != had && e->first >= 0) e->rail_after |= 1u << i;
            }
        }
        if (t->nmouths && (b->x < -t->drop || b->x > t->w + t->drop || b->y < -t->drop || b->y > t->h + t->drop)) {
            cue_fall(b, cue_nearest_pocket(t, b->x, b->y), i, e);
            continue;
        }
        for (int k = 0; k < t->nholes; k++) {
            float dx = b->x - t->hx[k], dy = b->y - t->hy[k];
            if (dx * dx + dy * dy < t->hr[k] * t->hr[k]) { cue_fall(b, k, i, e); break; }
        }
    }
    for (int i = 0; i < n; i++) {               /* friction */
        cue_ball *b = &w->b[i];
        if (b->down >= 0 || (b->vx == 0 && b->vy == 0)) continue;
        if (e) e->moved |= 1u << i;
        float s = sqrtf(b->vx * b->vx + b->vy * b->vy), ns = s - (t->mu + t->drag * s) * CUE_DT;
        if (ns < CUE_STOP) b->vx = b->vy = 0;
        else { b->vx *= ns / s; b->vy *= ns / s; }
    }
    if (e) e->steps++;
    return knock;
}

/* the shot to its end (everything at rest) */
static inline void cue_run(const cue_table *t, cue_world *w, cue_events *e)
{
    for (int k = 0; k < CUE_MAX_STEPS && cue_moving(w); k++) cue_step(t, w, e);
    for (int i = 0; i < w->n; i++) w->b[i].vx = w->b[i].vy = 0;
}

/* an angle (centi-degrees) as a direction */
static inline void cue_dir(int ang, float *dx, float *dy)
{
    float a = (float)ang * (CUE_PI / 18000.0f);
    *dx = cosf(a);
    *dy = sinf(a);
}

static inline int cue_angle(float dx, float dy)  /* a direction as centi-degrees 0..35999 */
{
    int a = (int)lroundf(atan2f(dy, dx) * (18000.0f / CUE_PI));
    return (a % 36000 + 36000) % 36000;
}

/* ball i struck along ang (centi-degrees) at speed */
static inline void cue_strike(cue_world *w, int i, int ang, float speed)
{
    float dx, dy;
    cue_dir(ang, &dx, &dy);
    w->b[i].vx = dx * speed;
    w->b[i].vy = dy * speed;
}

/* 1: a disc of radius r at (x, y) would lie on another (skip: a ball not counted, -1 none) */
static inline int cue_overlaps(const cue_world *w, float x, float y, float r, int skip)
{
    for (int i = 0; i < w->n; i++) {
        const cue_ball *b = &w->b[i];
        if (i == skip || b->down >= 0) continue;
        float R = b->r + r;
        if ((b->x - x) * (b->x - x) + (b->y - y) * (b->y - y) < R * R) return 1;
    }
    return 0;
}

/* the first ball a disc of radius r rolling from (x, y) along (dx, dy) (a unit) would touch: its index, the distance
 * rolled in *dist; -1 none (then *dist the distance to the first cushion line, *edge which one) */
static inline int cue_ray(const cue_table *t, const cue_world *w, float x, float y, float r, float dx, float dy,
                          int skip, float *dist, int *edge)
{
    float best = 1e30f;
    int hit = -1;
    for (int i = 0; i < w->n; i++) {
        const cue_ball *b = &w->b[i];
        if (i == skip || b->down >= 0) continue;
        float R = b->r + r, ox = b->x - x, oy = b->y - y, along = ox * dx + oy * dy;
        if (along <= 0) continue;
        float d2 = ox * ox + oy * oy - along * along;
        if (d2 >= R * R) continue;
        float s = along - sqrtf(R * R - d2);
        if (s < 0) s = 0;
        if (s < best) { best = s; hit = i; }
    }
    float wall = 1e30f;
    int ed = -1;
    if (dx < 0 && (x - r) / -dx < wall) { wall = (x - r) / -dx; ed = CUE_LEFT; }
    if (dx > 0 && (t->w - r - x) / dx < wall) { wall = (t->w - r - x) / dx; ed = CUE_RIGHT; }
    if (dy < 0 && (y - r) / -dy < wall) { wall = (y - r) / -dy; ed = CUE_TOP; }
    if (dy > 0 && (t->h - r - y) / dy < wall) { wall = (t->h - r - y) / dy; ed = CUE_BOTTOM; }
    if (wall < 0) wall = 0;
    if (hit >= 0 && best <= wall) { *dist = best; *edge = -1; return hit; }
    *dist = wall;
    *edge = ed;
    return -1;
}

/* 1: nothing on the table (but a and b) lies within `clear` of the segment from (x0, y0) to (x1, y1) */
static inline int cue_clear(const cue_world *w, float x0, float y0, float x1, float y1, float clear, int a, int b)
{
    float dx = x1 - x0, dy = y1 - y0, L2 = dx * dx + dy * dy;
    for (int i = 0; i < w->n; i++) {
        const cue_ball *c = &w->b[i];
        if (i == a || i == b || c->down >= 0) continue;
        float t = L2 > 0 ? ((c->x - x0) * dx + (c->y - y0) * dy) / L2 : 0;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        float ex = x0 + t * dx - c->x, ey = y0 + t * dy - c->y, R = clear + c->r;
        if (ex * ex + ey * ey < R * R) return 0;
    }
    return 1;
}

/* the nearest free place for a disc of radius r to (x, y), inside [lo, hi] (x and y), searched in rings */
static inline void cue_free_spot(const cue_world *w, float r, float x, float y, float lo_x, float lo_y, float hi_x,
                                 float hi_y, int skip, float *ox, float *oy)
{
    for (int ring = 0; ring < 60; ring++) {
        int k = ring ? 6 * ring : 1;
        for (int i = 0; i < k; i++) {
            float a = 2 * CUE_PI * i / k, d = ring * r * 0.7f;
            float cx = x + d * cosf(a), cy = y + d * sinf(a);
            if (cx < lo_x || cx > hi_x || cy < lo_y || cy > hi_y) continue;
            if (!cue_overlaps(w, cx, cy, r + 0.02f, skip)) { *ox = cx; *oy = cy; return; }
        }
    }
    *ox = x;
    *oy = y;
}

/* a small steady random generator for the rules' deals (racks) */
static inline uint32_t cue_rand(uint64_t *s)
{
    uint64_t z = (*s += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return (uint32_t)((z ^ (z >> 31)) >> 32);
}

#endif
