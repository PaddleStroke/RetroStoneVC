/*
 * Fruit Drop for one player: the rules (no SDL; the tests and the level maker link this file alone). See dropmerge.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/dropmerge.h"
#include "games/puzrand.h"

const int dmj_radius[DMJ_SIZES + 1] = {0, 14, 19, 25, 31, 38, 47, 56, 66, 78, 91, 106};
const int dmj_widths[DMJ_VARIANTS] = {DMJ_WIDE, 336, 280};

#define GRAV 46                                 /* steps a tick, each tick: about 2600 units a second squared */
#define VMAX (10 * DMJ_FP)                      /* a tick */
#define BOUNCE_MIN (3 * DMJ_FP / 2)             /* an impact faster than this bounces back a little */
#define BOUNCE_K 3                              /* sixteenths of it */
#define REST_V 10                               /* at rest: nothing faster than this (steps a tick)... */
#define REST_TICKS 30                           /* ...for this long */
#define POP (DMJ_FP * 3 / 4)                    /* a merged fruit's hop */
#define TOUCH (DMJ_FP / 2)                      /* two fruits closer than this touch (they merge) */
#define OUT_TICKS (6 * DMJ_HZ)                  /* the fruits used up: the end at rest, or this long after */
#define MAXC 640                                /* contacts kept a tick */

static const uint8_t WEIGHT[DMJ_DROPS] = {24, 24, 20, 18, 14};   /* endless: the dropper's fruits by chance */

static uint64_t isq(uint64_t v)
{
    uint64_t r = 0, bit = (uint64_t)1 << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else
            r >>= 1;
        bit >>= 2;
    }
    return r;
}

static int32_t R(int size) { return dmj_radius[size < 1 ? 1 : size > DMJ_SIZES ? DMJ_SIZES : size] * DMJ_FP; }
static int64_t mass(const dmj_ball *b) { int64_t r = b->r >> 4; return r * r + 1; }

static void event(dmj_world *w, int type, int size, int32_t x, int32_t y, int pts, int force, int id)
{
    dmj_event *e = &w->ev[w->nev++ % DMJ_EVENTS];
    e->type = (uint8_t)type;
    e->size = (uint8_t)size;
    e->chain = (uint8_t)(w->chain > 255 ? 255 : w->chain);
    e->x = (int16_t)(x / (DMJ_FP / 16));
    e->y = (int16_t)(y / (DMJ_FP / 16));
    e->tick = w->tick;
    e->pts = pts;
    e->force = force;
    e->id = (uint16_t)id;
}

/* ---- the dropper's fruits ----------------------------------------------------------------------------------------- */
static int fruit_at(dmj_world *w, int i)       /* the i-th fruit given (0..): a level's sequence, else by chance */
{
    if (w->J.nseq) return i < w->J.nseq ? w->J.seq[i] : 0;
    int sum = 0, k;
    for (k = 0; k < DMJ_DROPS; k++) sum += WEIGHT[k];
    int v = pr_below(&w->rng, sum);
    for (k = 0; k < DMJ_DROPS - 1 && v >= WEIGHT[k]; k++) v -= WEIGHT[k];
    return k + 1;
}

/* ---- the physics ---------------------------------------------------------------------------------------------- */
typedef struct { uint8_t a, b; } contact;      /* two fruits touching */

static int add_ball(dmj_world *w, int size, int32_t x, int32_t y)
{
    if (w->nb >= DMJ_MAXB) return -1;
    dmj_ball *b = &w->b[w->nb];
    memset(b, 0, sizeof *b);
    b->x = b->px = x;
    b->y = b->py = y;
    b->r = R(size);
    b->size = (uint8_t)size;
    b->born = -1;
    b->id = ++w->serial;
    return w->nb++;
}

/* two fruits pushed apart (by their masses); 1 they touch */
static int pair(dmj_ball *a, dmj_ball *b)
{
    int32_t dx = b->x - a->x, dy = b->y - a->y, rs = a->r + b->r;
    if (dx >= rs || -dx >= rs || dy >= rs || -dy >= rs) return 0;
    int64_t d2 = (int64_t)dx * dx + (int64_t)dy * dy;
    if (d2 >= (int64_t)rs * rs) return d2 <= (int64_t)(rs + TOUCH) * (rs + TOUCH);
    int32_t d = (int32_t)isq((uint64_t)d2);
    if (d == 0) dx = 1, dy = 0, d = 1;
    int64_t pen = rs - d, ma = mass(a), mb = mass(b);
    int64_t pa = pen * mb / (ma + mb), pb = pen - pa;
    a->x -= (int32_t)(pa * dx / d);
    a->y -= (int32_t)(pa * dy / d);
    b->x += (int32_t)(pb * dx / d);
    b->y += (int32_t)(pb * dy / d);
    return 1;
}

/* a fruit pushed out of the walls, the floor and the pegs; bits: 1 the left wall, 8 the right one, 2 the floor, 4 a peg */
static int bounds(const dmj_world *w, dmj_ball *b)
{
    int hit = 0;
    int32_t W = w->J.width * DMJ_FP, F = DMJ_JH * DMJ_FP;
    if (b->x - b->r < 0) b->x = b->r;
    if (b->x + b->r > W) b->x = W - b->r;
    if (b->y + b->r > F) b->y = F - b->r;
    if (b->x - b->r < TOUCH) hit |= 1;          /* touching (pushed out, or resting there) */
    if (b->x + b->r > W - TOUCH) hit |= 8;
    if (b->y + b->r > F - TOUCH) hit |= 2;
    for (int i = 0; i < w->J.npeg; i++) {
        const dmj_peg *p = &w->J.peg[i];
        int32_t dx = b->x - p->x * DMJ_FP, dy = b->y - p->y * DMJ_FP, rs = b->r + p->r * DMJ_FP;
        int32_t rt = rs + TOUCH;
        if (dx >= rt || -dx >= rt || dy >= rt || -dy >= rt) continue;
        int64_t d2 = (int64_t)dx * dx + (int64_t)dy * dy;
        if (d2 >= (int64_t)(rs + TOUCH) * (rs + TOUCH)) continue;
        hit |= 4;
        if (d2 >= (int64_t)rs * rs) continue;
        int32_t d = (int32_t)isq((uint64_t)d2);
        if (d == 0) dx = 0, dy = -1, d = 1;
        b->x += (int32_t)((int64_t)(rs - d) * dx / d);
        b->y += (int32_t)((int64_t)(rs - d) * dy / d);
    }
    return hit;
}

/* the speed along n (4096: one) of a fruit, or between two */
static int32_t along(int32_t vx, int32_t vy, int32_t nx, int32_t ny) { return (int32_t)(((int64_t)vx * nx + (int64_t)vy * ny) / 4096); }

static void merge_all(dmj_world *w)
{
    uint8_t gone[DMJ_MAXB] = {0};
    int any = 0;
    for (int i = 0; i < w->nb; i++) {
        if (gone[i]) continue;
        for (int j = i + 1; j < w->nb; j++) {
            dmj_ball *a = &w->b[i], *c = &w->b[j];
            if (gone[j] || a->size != c->size) continue;
            int32_t dx = c->x - a->x, dy = c->y - a->y, rs = a->r + c->r + TOUCH;
            if (dx >= rs || -dx >= rs || dy >= rs || -dy >= rs) continue;
            if ((int64_t)dx * dx + (int64_t)dy * dy > (int64_t)rs * rs) continue;
            int s = a->size, pts = s * (s + 1) / 2;
            int32_t mx = a->x + dx / 2, my = a->y + dy / 2;
            w->score += pts;
            w->chain++;
            if (w->chain > w->best_chain) w->best_chain = w->chain;
            any = 1;
            if (s >= DMJ_SIZES) {               /* two of the biggest: gone */
                gone[i] = gone[j] = 1;
                event(w, DMJ_E_GONE, s, mx, my, pts, 0, 0);
                break;
            }
            a->x = a->px = mx;
            a->y = a->py = my;
            a->vx = (a->vx + c->vx) / 2;
            a->vy = (a->vy + c->vy) / 2 - POP;
            if (a->r < c->r) a->r = c->r;
            a->size = (uint8_t)(s + 1);
            a->born = w->tick;
            a->id = ++w->serial;
            a->fresh = 0;
            a->over = 0;
            a->ang = (a->ang + c->ang) / 2;
            gone[j] = 1;
            w->made[s + 1]++;
            if (s + 1 > w->biggest) w->biggest = s + 1;
            event(w, DMJ_E_MERGE, s + 1, mx, my, pts, 0, a->id);
        }
    }
    if (!any) return;
    int n = 0;
    for (int i = 0; i < w->nb; i++)
        if (!gone[i]) w->b[n++] = w->b[i];
    w->nb = n;
}

static int goal_reached(const dmj_world *w) { return w->J.goal != DMJ_ENDLESS && dmj_goal_left(w) == 0; }

/* the fruits above the rim; the ends */
static void judge(dmj_world *w)
{
    int most = 0;
    for (int i = 0; i < w->nb; i++) {
        dmj_ball *b = &w->b[i];
        if (!b->fresh && b->y - b->r < 0) {
            if (b->over < 32000) b->over++;
        } else
            b->over = 0;
        if (b->over > most) most = b->over;
    }
    w->overmax = most;
    if (w->end != DMJ_PLAYING) return;
    if (goal_reached(w)) {
        w->end = DMJ_WON;
        w->end_tick = w->tick;
    } else if (most >= DMJ_OVER) {
        w->end = DMJ_OVERFLOW;
        w->end_tick = w->tick;
    } else if (w->J.nseq && !w->cur && (w->rest || w->tick - w->last_drop >= OUT_TICKS)) {
        w->end = DMJ_OUT;
        w->end_tick = w->tick;
    }
}

static void physics(dmj_world *w, int merging)
{
    contact C[MAXC];
    int nb = w->nb, nc = 0;
    int32_t vx0[DMJ_MAXB], vy0[DMJ_MAXB];
    uint8_t hit[DMJ_MAXB];
    for (int i = 0; i < nb; i++) {
        dmj_ball *b = &w->b[i];
        b->px = b->x;
        b->py = b->y;
        vx0[i] = b->vx;
        vy0[i] = b->vy;
        b->vy += GRAV;
        if (b->vx > VMAX) b->vx = VMAX;
        if (b->vx < -VMAX) b->vx = -VMAX;
        if (b->vy > VMAX) b->vy = VMAX;
        if (b->vy < -VMAX) b->vy = -VMAX;
        b->x += b->vx;
        b->y += b->vy;
        int32_t full = R(b->size);
        if (b->r < full) {
            int32_t step = (full - R(b->size - 1)) / DMJ_GROW + 1;
            b->r = b->r + step > full ? full : b->r + step;
        }
        hit[i] = 0;
    }
    for (int it = 0; it < DMJ_ITER; it++) {
        int last = it == DMJ_ITER - 1;
        for (int i = 0; i < nb; i++)
            for (int j = i + 1; j < nb; j++)
                if (pair(&w->b[i], &w->b[j]) && last) {
                    hit[i] = hit[j] = 1;
                    if (nc < MAXC) C[nc].a = (uint8_t)i, C[nc].b = (uint8_t)j, nc++;
                }
        for (int i = 0; i < nb; i++) {
            int h = bounds(w, &w->b[i]);
            if (last && h) hit[i] |= (uint8_t)(h << 1);
        }
    }
    /* the speeds: what each moved; a bounce, the rubbing */
    for (int i = 0; i < nb; i++) {
        dmj_ball *b = &w->b[i];
        b->vx = b->x - b->px;
        b->vy = b->y - b->py;
    }
    for (int k = 0; k < nc; k++) {
        dmj_ball *a = &w->b[C[k].a], *b = &w->b[C[k].b];
        int32_t dx = b->x - a->x, dy = b->y - a->y;
        int32_t d = (int32_t)isq((uint64_t)((int64_t)dx * dx + (int64_t)dy * dy));
        if (d == 0) continue;
        int32_t nx = (int32_t)((int64_t)dx * 4096 / d), ny = (int32_t)((int64_t)dy * 4096 / d);
        int64_t ma = mass(a), mb = mass(b);
        int32_t vn0 = along(vx0[C[k].b] - vx0[C[k].a], vy0[C[k].b] - vy0[C[k].a], nx, ny);
        int32_t rvx = b->vx - a->vx, rvy = b->vy - a->vy, vn = along(rvx, rvy, nx, ny);
        int32_t want = vn0 < -BOUNCE_MIN ? -vn0 * BOUNCE_K / 16 : vn;
        int32_t j = want > vn ? want - vn : 0;
        int32_t tx = rvx - (int32_t)((int64_t)vn * nx / 4096), ty = rvy - (int32_t)((int64_t)vn * ny / 4096);
        tx /= 6, ty /= 6;                       /* the rubbing: a sixth of the sliding taken off */
        int32_t jx = (int32_t)((int64_t)j * nx / 4096), jy = (int32_t)((int64_t)j * ny / 4096);
        a->vx += (int32_t)((-(int64_t)jx + tx) * mb / (ma + mb));
        a->vy += (int32_t)((-(int64_t)jy + ty) * mb / (ma + mb));
        b->vx += (int32_t)(((int64_t)jx - tx) * ma / (ma + mb));
        b->vy += (int32_t)(((int64_t)jy - ty) * ma / (ma + mb));
    }
    int32_t fastest = 0;
    for (int i = 0; i < nb; i++) {
        dmj_ball *b = &w->b[i];
        int h = hit[i] >> 1;
        if (h & 2) {                            /* the floor */
            if (vy0[i] > BOUNCE_MIN && b->vy > -vy0[i] * BOUNCE_K / 16) b->vy = -vy0[i] * BOUNCE_K / 16;
            b->vx -= b->vx / 10;
        }
        if (h & 9) {                            /* the glass: a bounce back when it hit it fast */
            if (((h & 1) && vx0[i] < -BOUNCE_MIN) || ((h & 8) && vx0[i] > BOUNCE_MIN)) b->vx = -vx0[i] * BOUNCE_K / 16;
        }
        if (h & 4) {
            b->vx -= b->vx / 8;
            b->vy -= b->vy / 8;
        }
        b->vx -= b->vx / 256;
        b->vy -= b->vy / 256;
        if (hit[i] && b->fresh) {
            b->fresh = 0;
            event(w, DMJ_E_LAND, b->size, b->x, b->y, 0, vy0[i], b->id);
        }
        if (hit[i]) b->ang += (int32_t)((int64_t)b->vx * 10430 / (b->r ? b->r : 1));
        b->hit = hit[i];
        int32_t v = b->vx < 0 ? -b->vx : b->vx, u = b->vy < 0 ? -b->vy : b->vy;
        if (v > fastest) fastest = v;
        if (u > fastest) fastest = u;
        if (b->r < R(b->size) || b->fresh) fastest = VMAX;
    }
    if (merging) merge_all(w);
    if (fastest < REST_V) {
        if (++w->calm >= REST_TICKS) {
            w->rest = 1;
            for (int i = 0; i < w->nb; i++) {
                dmj_ball *b = &w->b[i];
                b->vx = b->vy = 0;
                b->px = b->x;
                b->py = b->y;
            }
        }
    } else
        w->calm = 0;
}

void dmj_step(dmj_world *w)
{
    if (w->end == DMJ_OVERFLOW) return;         /* the jar stands still at the end */
    w->tick++;
    if (!w->rest) physics(w, !w->still);
    else
        for (int i = 0; i < w->nb; i++) w->b[i].px = w->b[i].x, w->b[i].py = w->b[i].y;
    judge(w);
}

void dmj_run(dmj_world *w, int ticks)
{
    while (ticks > 0) {
        if (w->rest && !w->overmax && w->end != DMJ_PLAYING) { w->tick += ticks; return; }
        if (w->rest && !w->overmax && w->end == DMJ_PLAYING && !(w->J.nseq && !w->cur)) {
            w->tick += ticks;                   /* at rest, nothing above the rim: the clock alone */
            return;
        }
        if (w->end == DMJ_OVERFLOW) return;
        dmj_step(w);
        ticks--;
    }
}

void dmj_settle(dmj_world *w, int most)
{
    for (int t = 0; t < most && !w->rest && w->end != DMJ_OVERFLOW; t++) dmj_step(w);
}

int dmj_danger(const dmj_world *w) { return w->overmax; }

int dmj_goal_left(const dmj_world *w)
{
    int v;
    switch (w->J.goal) {
    case DMJ_SIZE: v = w->J.target - w->biggest; break;
    case DMJ_SCORE: v = w->J.target - w->score; break;
    case DMJ_COUNT: v = w->J.target - w->made[w->J.tsize < 1 ? 1 : w->J.tsize > DMJ_SIZES ? DMJ_SIZES : w->J.tsize]; break;
    case DMJ_CHAIN: v = w->J.target - w->best_chain; break;
    default: return 0;
    }
    return v < 0 ? 0 : v;
}

/* ---- the jar ------------------------------------------------------------------------------------------------- */
void dmj_build(dmj_world *w, const dmj_jar *j)
{
    memset(w, 0, sizeof *w);
    w->J = *j;
    if (w->J.width < 2 * dmj_radius[DMJ_SIZES] + 20) w->J.width = 2 * dmj_radius[DMJ_SIZES] + 20;
    w->rng = pr_seed(j->seed);
    for (int i = 0; i < j->npre; i++)
        add_ball(w, j->pre[i].size, j->pre[i].x * DMJ_FP, j->pre[i].y * DMJ_FP);
    for (int t = 0; t < 900 && !w->rest; t++) {   /* the fruits already there come to rest (no merging) */
        w->tick++;
        physics(w, 0);
    }
    for (int i = 0; i < w->nb; i++) {
        dmj_ball *b = &w->b[i];
        b->vx = b->vy = 0;
        b->px = b->x;
        b->py = b->y;
        b->fresh = 0;
        b->ang = (int32_t)((b->id * 40503u) & 0xffff) - 32768;
    }
    w->tick = 0;
    w->rest = 1;
    w->calm = 0;
    w->nev = 0;
    w->cur = fruit_at(w, 0);
    w->next = fruit_at(w, 1);
    w->last_drop = 0;
}

int dmj_can_drop(const dmj_world *w) { return w->end == DMJ_PLAYING && w->cur && w->tick >= w->ready; }

int dmj_clamp(const dmj_world *w, int size, int x)
{
    int32_t r = R(size), W = w->J.width * DMJ_FP;
    return x < r ? r : x > W - r ? W - r : x;
}

int dmj_drop(dmj_world *w, int x)
{
    if (!dmj_can_drop(w)) return 0;
    int i = add_ball(w, w->cur, dmj_clamp(w, w->cur, x), DMJ_SPAWN * DMJ_FP);
    if (i < 0) return 0;
    w->b[i].fresh = 1;
    w->b[i].ang = (int32_t)((w->b[i].id * 40503u) & 0x3fff) - 8192;
    event(w, DMJ_E_DROP, w->cur, w->b[i].x, w->b[i].y, 0, 0, w->b[i].id);
    w->drops++;
    w->chain = 0;
    w->ready = w->tick + DMJ_GAP;
    w->last_drop = w->tick;
    w->cur = w->next;
    w->next = fruit_at(w, w->drops + 1);
    w->rest = 0;
    w->calm = 0;
    return 1;
}

int32_t dmj_land(const dmj_world *w, int size, int x, int *what)
{
    int32_t r = R(size), best = DMJ_JH * DMJ_FP - r;
    *what = -1;
    for (int i = 0; i < w->nb; i++) {
        const dmj_ball *b = &w->b[i];
        int32_t rs = r + b->r, dx = x - b->x;
        if (dx >= rs || -dx >= rs) continue;
        int32_t y = b->y - (int32_t)isq((uint64_t)((int64_t)rs * rs - (int64_t)dx * dx));
        if (y < best) best = y, *what = i;
    }
    for (int i = 0; i < w->J.npeg; i++) {
        const dmj_peg *p = &w->J.peg[i];
        int32_t rs = r + p->r * DMJ_FP, dx = x - p->x * DMJ_FP;
        if (dx >= rs || -dx >= rs) continue;
        int32_t y = p->y * DMJ_FP - (int32_t)isq((uint64_t)((int64_t)rs * rs - (int64_t)dx * dx));
        if (y < best) best = y, *what = -2;
    }
    return best;
}

/* ---- a player ---------------------------------------------------------------------------------------------- */
static int rate(const dmj_world *w, int size, int x)
{
    int what;
    int32_t y = dmj_land(w, size, x, &what), r = R(size);
    int v = y / DMJ_FP * 2;
    if (what >= 0) {
        int s2 = w->b[what].size;
        if (s2 == size) v += 700 + 40 * size;
        else if (s2 < size) v -= 140 * (size - s2);
        else v -= 10 * (s2 - size);
    }
    int near = 0;
    for (int i = 0; i < w->nb; i++) {
        const dmj_ball *b = &w->b[i];
        if (i == what || (b->size != size && b->size != size + 1)) continue;
        int64_t dx = b->x - x, dy = b->y - y, rs = r + b->r + 24 * DMJ_FP;
        if (dx * dx + dy * dy > rs * rs) continue;
        near += b->size == size ? 260 : 40;
    }
    v += near > 400 ? 400 : near;
    if (y - r < 50 * DMJ_FP) v -= 900;         /* close under the rim */
    return v;
}

int dmj_aim(const dmj_world *w, int noise, uint32_t *rng)
{
    int size = w->cur ? w->cur : 1, best = -(1 << 30), at = w->J.width * DMJ_FP / 2;
    int r = dmj_radius[size], W = w->J.width;
    for (int u = r; u <= W - r + 3; u += 4) {
        int x = (u > W - r ? W - r : u) * DMJ_FP;
        int v = rate(w, size, x) + (noise ? pr_below(rng, noise) : 0);
        if (v > best) best = v, at = x;
    }
    for (int i = 0; i < w->nb; i++) {           /* right over a fruit of its kind */
        if (w->b[i].size != size) continue;
        int x = dmj_clamp(w, size, w->b[i].x / DMJ_FP * DMJ_FP);
        x = x / DMJ_FP * DMJ_FP;
        if (x < r * DMJ_FP) x += DMJ_FP;
        int v = rate(w, size, x) + (noise ? pr_below(rng, noise) : 0);
        if (v > best) best = v, at = x;
    }
    return at;
}

int dmj_play(dmj_world *w, const int *xs, int nxs, int noise, uint32_t *rng, int most, int *out)
{
    int k = 0;
    while (w->end == DMJ_PLAYING) {
        while (w->end == DMJ_PLAYING && !(dmj_can_drop(w) && (w->rest || w->tick - w->last_drop >= DMJ_SETTLE))) {
            if (w->rest && !w->overmax && w->cur && w->tick < w->ready) { w->tick = w->ready; continue; }
            dmj_step(w);
        }
        if (w->end != DMJ_PLAYING || k >= most || (xs && k >= nxs)) break;
        int x = xs ? xs[k] * DMJ_FP : dmj_aim(w, noise, rng);
        if (out) out[k] = x / DMJ_FP;
        if (!dmj_drop(w, x)) break;
        k++;
    }
    return w->end;
}

/* ---- a game ---------------------------------------------------------------------------------------------- */
static const char *num(const char *s, int *v)
{
    while (*s == ' ' || *s == ',') s++;
    if (*s < '0' || *s > '9') return NULL;
    int n = 0;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    *v = n;
    return s;
}

int dmj_parse(dmj_jar *j, int goal, int target, int tsize, int width, const char *seq, const char *pegs,
              const char *pre)
{
    memset(j, 0, sizeof *j);
    if (goal <= DMJ_ENDLESS || goal >= DMJ_GOALS || target < 1 || width < 2 * dmj_radius[DMJ_SIZES] + 20 ||
        width > DMJ_WIDE)
        return 0;
    j->goal = goal;
    j->target = target;
    j->tsize = tsize;
    j->width = width;
    for (; seq && *seq; seq++) {
        if (*seq < '1' || *seq > '0' + DMJ_DROPS || j->nseq >= DMJ_SEQ) return 0;
        j->seq[j->nseq++] = (uint8_t)(*seq - '0');
    }
    if (!j->nseq) return 0;
    const char *s = pegs;
    for (int x, y, r; s && *s;) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (j->npeg >= DMJ_MAXPEG || !(s = num(s, &x)) || !(s = num(s, &y)) || !(s = num(s, &r))) return 0;
        j->peg[j->npeg++] = (dmj_peg){(int16_t)x, (int16_t)y, (int16_t)r};
    }
    s = pre;
    for (int z, x, y; s && *s;) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (j->npre >= DMJ_MAXPRE || !(s = num(s, &z)) || !(s = num(s, &x)) || !(s = num(s, &y))) return 0;
        if (z < 1 || z > DMJ_SIZES) return 0;
        j->pre[j->npre++] = (dmj_pre){(uint8_t)z, (int16_t)x, (int16_t)y};
    }
    return 1;
}

int dmj_load(dmj_jar *j, const dmj_level_def *d)
{
    if (!dmj_parse(j, d->goal, d->target, d->tsize, d->width, d->seq, d->pegs, d->pre)) return 0;
    j->par = d->par;
    return 1;
}

static void begin(dmj_game *g, const dmj_jar *j)
{
    dmj_build(&g->w, j);
    g->w0 = g->w;
    g->hints = g->restarts = 0;
}

void dmj_endless(dmj_game *g, int variant, uint32_t seed)
{
    dmj_jar j;
    memset(&j, 0, sizeof j);
    j.goal = DMJ_ENDLESS;
    j.width = dmj_widths[variant < 0 || variant >= DMJ_VARIANTS ? 0 : variant];
    j.seed = seed;
    g->level = -1;
    begin(g, &j);
}

#ifndef DMJ_GEN
#include "games/dropmerge_levels.h"
#endif

int dmj_level(dmj_game *g, int level)
{
#ifdef DMJ_GEN
    (void)g, (void)level;
    return 0;
#else
    dmj_jar j;
    if (level < 0 || level >= DMJ_LEVELS || !dmj_load(&j, &dmj_levels[level])) return 0;
    g->level = level;
    begin(g, &j);
    return 1;
#endif
}

int dmj_chapter_of(int level)
{
    int c = level / (DMJ_LEVELS / DMJ_CHAPTERS);
    return c < 0 ? 0 : c >= DMJ_CHAPTERS ? DMJ_CHAPTERS - 1 : c;
}

void dmj_restart(dmj_game *g)
{
    if (g->w.drops) g->restarts++;
    g->w = g->w0;
}

int dmj_stars(const dmj_game *g)
{
    const dmj_world *w = &g->w;
    if (w->J.goal == DMJ_ENDLESS) return w->score >= DMJ_STAR3 ? 3 : w->score >= DMJ_STAR2 ? 2 : 1;
    if (w->end != DMJ_WON) return 0;
    int par = w->J.par;
    return par <= 0 || w->drops <= par ? 3 : 2 * w->drops <= 3 * par ? 2 : 1;
}

int dmj_script(dmj_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int v = -1;
        if (*s >= '0' && *s <= '9') s = num(s, &v);
        if (c == 'd' && v >= 0) {
            if (g->w.end != DMJ_PLAYING) return 0;
            int x = v * DMJ_FP;
            dmj_play(&g->w, &v, 1, 0, NULL, 1, NULL);
            (void)x;
            dmj_settle(&g->w, DMJ_SETTLE);
        } else if (c == '=') {
            uint32_t rs = 1;
            dmj_play(&g->w, NULL, 0, 0, &rs, v < 0 ? 1 << 20 : v, NULL);
            dmj_settle(&g->w, DMJ_SETTLE);
        } else if (c == 'w' && v >= 0)
            dmj_run(&g->w, v);
        else if (c == 'r')
            dmj_restart(g);
        else
            return 0;
    }
    return 1;
}
