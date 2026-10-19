/*
 * Lemur Leap: the rules. The lemur bounces by itself on every leaf it lands on and climbs a column that wraps (out on
 * one side, in on the other); the camera only rises; falling out of the stage's bottom ends the run. The leaves are
 * made row by row above the camera by a generator that keeps a PATH: each row holds one leaf that can be reached from
 * the row's leaf below (a gap within the bounce's reach, and any column is within the lemur's reach across the wrap);
 * the extras (more leaves low down, dry leaves that break, coins, power-ups, pests) come on top of it.
 * No SDK call (its own xorshift RNG): deterministic, linked alone by tests/test_world.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/lemurleap/LICENSE.
 */
#include "game.h"
#include <string.h>
#ifdef BOT_DEBUG
#include <stdio.h>
#endif

const int16_t LOOK_COST[LOOKS] = {0, 30, 60, 100, 150, 250, 400, 600,      /* the hats: none, leaf cap ... helmet */
                                  0, 200, 300, 500};                     /* the furs: grey, golden, snow, midnight */

/* ---- small helpers --------------------------------------------------------------------------------------------- */
static uint32_t rnd(world *w)
{
    uint32_t x = w->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return w->rng = x;
}
static int rint_(world *w, int n) { return n > 0 ? (int)(rnd(w) % (uint32_t)n) : 0; }
static int iabs(int v) { return v < 0 ? -v : v; }
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

int wrap_dx(int dx)
{
    dx %= COL_W;
    if (dx < -COL_W / 2) dx += COL_W;
    if (dx >= COL_W / 2) dx -= COL_W;
    return dx;
}

int zone_of(int m) { return m >= ZONE_SPACE ? Z_SPACE : m >= ZONE_SKY ? Z_SKY : m >= ZONE_CLOUDS ? Z_CLOUDS : Z_GARDEN; }

int world_height_m(const world *w) { return -w->top_y / PX_PER_M; }

/* the highest step the path may take after a leaf of this type (a dry leaf is never on the path) */
int plat_reach(int type) { return type == P_CLOUD ? CLOUD_REACH : REACH; }

int bounce_v(int type) { return type == P_SHROOM ? SHROOM_V : type == P_CLOUD ? CLOUD_V : JUMP_V; }

/* the frames from a bounce at speed v until the feet come down through a line `gap` px higher (-1: never) */
int frames_down(int v, int gap)
{
    int32_t y = 0, vy = -v;                         /* as hero_step: the speed, then the position */
    for (int t = 1; t < 1000; t++) {
        vy = imin(vy + GRAVITY, MAX_FALL);
        y += vy;
        if (vy > 0 && y >= -(int32_t)gap * FX) return t;
    }
    return -1;
}
/* px the lemur moves sideways in t frames from a standstill (MAX_VX / ACCEL frames to full speed) */
int side_reach(int t)
{
    int a = MAX_VX / ACCEL;
    return t <= a ? 0 : (MAX_VX * (t - a) + MAX_VX * a / 2) / FX;
}
/* the path's sideways budget for a step of `gap` px after a leaf of this type: the reach, a margin kept */
static int path_budget(int type, int gap)
{
    int t = frames_down(bounce_v(type), gap);
    return t < 0 ? 0 : side_reach(t) - PATH_MARGIN;
}
/* the columns between the spans [a, b] and [c, d] (px, inclusive) across the wrap: 0 when they overlap */
int span_gap(int a, int b, int c, int d)
{
    int best = COL_W;
    for (int k = -1; k <= 1; k++) {
        int a2 = a + k * COL_W, b2 = b + k * COL_W, g = a2 > d ? a2 - d : c > b2 ? c - b2 : 0;
        if (g < best) best = g;
    }
    return best;
}

/* ---- the generator ------------------------------------------------------------------------------------------------ */
static plat *plat_new(world *w, int steal)
{
    for (int i = 0; i < MAX_PLAT; i++)
        if (!w->p[i].used) {
            memset(&w->p[i], 0, sizeof w->p[i]);
            w->p[i].used = 1;
            return &w->p[i];
        }
    if (!steal) return NULL;
    int lo = 0;                                     /* full (never seen in play): the lowest leaf makes room */
    for (int i = 1; i < MAX_PLAT; i++)
        if (w->p[i].y > w->p[lo].y) lo = i;
    memset(&w->p[lo], 0, sizeof w->p[lo]);
    w->p[lo].used = 1;
    return &w->p[lo];
}

/* a leaf at (x, y) px overlaps (or nearly) another one */
static int crowded(const world *w, int x, int y, int wd)
{
    for (int i = 0; i < MAX_PLAT; i++) {
        const plat *p = &w->p[i];
        if (!p->used) continue;
        int py = p->y / FX, px = p->x / FX;
        if (iabs(py - y) < 12 && x < px + p->w + 6 && x + wd + 6 > px) return 1;
        if (p->type == P_MOVE && iabs(py - y) < 12) return 1;        /* (a moving leaf sweeps its row) */
    }
    return 0;
}

static void coin_add(world *w, int x, int y)
{
    for (int i = 0; i < MAX_COIN; i++)
        if (!w->c[i].used) {
            memset(&w->c[i], 0, sizeof w->c[i]);
            w->c[i].used = 1;
            w->c[i].x = (int32_t)x * FX;
            w->c[i].y = (int32_t)y * FX;
            return;
        }
}

/* the gap between two rows of the path at m metres: 22 px at the ground, 58 at 2000 m (then the same) */
static int gap_base(int m) { return 22 + imin(imax(m, 0), 2000) * 36 / 2000; }

static int pick_type(world *w, int m)
{
    int z = zone_of(m);
    int wt[P_TYPES] = {0};
    wt[P_LEAF] = z == Z_SPACE ? 70 : 100;
    wt[P_MOVE] = m < 50 ? 0 : imin(45, (m - 50) / 6) + (z == Z_SPACE ? 15 : 0);
    wt[P_PUFF] = m < 150 ? 0 : imin(35, (m - 150) / 10);
    wt[P_CLOUD] = z == Z_CLOUDS ? 45 : z == Z_SKY ? 15 : 0;
    wt[P_SHROOM] = m < 30 ? 0 : 5;
    int sum = 0;
    for (int i = 0; i < P_TYPES; i++) sum += wt[i];
    int r = rint_(w, sum);
    for (int i = 0; i < P_TYPES; i++) {
        if (r < wt[i]) return i;
        r -= wt[i];
    }
    return P_LEAF;
}

/* does a pest hovering over [a, b] (px) keep clear of the leaf p's column? */
static int clear_of(int a, int b, const plat *p)
{
    if (!p) return 1;
    int l = p->x0 - 10, r = p->x1 + p->w + 10;
    for (int k = -1; k <= 1; k++)
        if (b + k * COL_W >= l && a + k * COL_W <= r) return 0;
    return 1;
}

static plat *last_path;                             /* (gen_row's scratch: the row below, during one call) */

static void gen_row(world *w)
{
    int y0 = w->gen_y, m = -y0 / PX_PER_M;
    int gap = gap_base(m) + rint_(w, 13) - 6;
    gap = imin(gap, plat_reach(w->last_path_type));
    gap = imax(gap, 16);
    int y = y0 - gap, my = -y / PX_PER_M;
    int type = pick_type(w, my);
    plat *p = plat_new(w, 1);
    p->type = (uint8_t)type;
    p->w = 32;
    p->y = (int32_t)y * FX;
    p->path = 1;
    /* where: anywhere, as long as its run (a moving leaf's, a cloud's drift) comes within the lemur's reach of the
     * leaf below's run, in the time this bounce gives (a moving leaf can be waited for: bounce until it comes) */
    int budget = path_budget(w->last_path_type, gap), x = 0, sp = 0, range = 0;
    if (type == P_MOVE) {
        sp = imin(128 + my * 256 / 800, 410);
        range = imin(COL_W - p->w, 80 + my / 6);
    } else if (type == P_CLOUD) {
        range = 40;
    }
    for (int k = 0; k < 16; k++) {
        x = rint_(w, COL_W - p->w + 1);
        int l = imax(0, x - range / 2), r = imin(COL_W - p->w, l + range);
        if (r - l < range) l = imax(0, r - range);
        if (k == 15) {                              /* (never seen: then right over the leaf below) */
            x = imin(imax(w->last_path_l, 0), COL_W - p->w);
            l = r = x;
        }
        if (span_gap(l, r + p->w - 1, w->last_path_l, w->last_path_r) > budget) continue;
        p->x0 = (int16_t)l;
        p->x1 = (int16_t)r;
        break;
    }
    if (x < p->x0) x = p->x0;
    if (x > p->x1) x = p->x1;
    p->x = (int32_t)x * FX;
    if (type == P_MOVE) p->vx = (int16_t)(rint_(w, 2) ? sp : -sp);
    else if (type == P_CLOUD) p->vx = (int16_t)(rint_(w, 2) ? 48 : -48);
    else p->x0 = p->x1 = (int16_t)x;
    if ((type == P_LEAF || type == P_MOVE) && my > 15 && rint_(w, 100) < 5 + imin(my / 200, 3))
        p->spring = (uint8_t)(1 + 2 + rint_(w, p->w - 16 + 1));
    else if (type == P_LEAF && my > 60 && w->last_item_y - y > 500 && rint_(w, 1000) < 45) {
        int k = rint_(w, 100);
        p->item = (uint8_t)(k < 36 ? IT_PROP : k < 60 ? IT_MAGNET : (k < 82 || my < 400) ? IT_SHIELD : IT_ROCKET);
        if (my < 120 && p->item == IT_SHIELD) p->item = IT_MAGNET;
        w->last_item_y = y;
    }
    plat *below = last_path;

    /* more leaves low down: a gentle start */
    if (my < 250 && rint_(w, 100) < 60 - my * 60 / 250) {
        for (int k = 0; k < 6; k++) {
            int x2 = rint_(w, COL_W - 32 + 1), y2 = y + rint_(w, 9) - 4;
            if (iabs(wrap_dx(x2 - x)) < 56 || crowded(w, x2, y2, 32)) continue;
            plat *q = plat_new(w, 0);
            if (!q) break;
            q->type = P_LEAF;
            q->w = 32;
            q->x = (int32_t)x2 * FX;
            q->y = (int32_t)y2 * FX;
            break;
        }
    }
    /* a dry leaf between the rows: it breaks under the feet (no bounce) */
    if (my >= 30 && gap >= 26 && rint_(w, 100) < imin(45, 8 + my / 25)) {
        for (int k = 0; k < 6; k++) {
            int x2 = rint_(w, COL_W - 32 + 1), y2 = y0 - gap / 2 + rint_(w, gap / 3 + 1) - gap / 6;
            if (crowded(w, x2, y2, 32)) continue;
            plat *q = plat_new(w, 0);
            if (!q) break;
            q->type = P_DRY;
            q->w = 32;
            q->x = (int32_t)x2 * FX;
            q->y = (int32_t)y2 * FX;
            break;
        }
    }
    /* coins: a column over the leaf, or an arc across the gap */
    if (rint_(w, 100) < 22) {
        int n = 3 + rint_(w, 3), cx = x + 16;
        if (rint_(w, 2) || type == P_MOVE) {
            if (type != P_MOVE)
                for (int k = 0; k < n; k++) coin_add(w, cx, y - 22 - 13 * k);
        } else {
            int ax = rint_(w, COL_W - 60) + 30, ay = y0 - gap / 2;
            for (int k = 0; k < n; k++) {
                int dx = (k * 2 - (n - 1)) * 7;
                coin_add(w, (ax + dx + COL_W) % COL_W, ay + dx * dx / 40);
            }
        }
    }
    /* a pest hovering in the gap, clear of the two leaves' columns (a straight bounce never meets it) */
    int pgap = imax(220, 900 - my / 2);
    if (my >= 120 && w->last_pest_y - y >= pgap && gap >= 30 && rint_(w, 100) < 35) {
        int slot = -1;
        for (int i = 0; i < MAX_PEST; i++)
            if (!w->e[i].used) { slot = i; break; }
        for (int k = 0; k < 10 && slot >= 0; k++) {
            int amp = 6 + rint_(w, 26), x0 = 12 + amp + rint_(w, COL_W - 24 - 2 * amp + 1);
            if (!clear_of(x0 - amp - 10, x0 + amp + 10, p) || !clear_of(x0 - amp - 10, x0 + amp + 10, below)) continue;
            pest *e = &w->e[slot];
            memset(e, 0, sizeof *e);
            e->used = 1;
            int z = zone_of(my);
            e->type = (uint8_t)(z == Z_GARDEN ? E_BEE : z == Z_CLOUDS ? E_STORM : z == Z_SKY ? E_BAT : E_UFO);
            e->x0 = (int16_t)x0;
            e->y0 = (int16_t)(y0 - gap / 2);
            e->amp = (int16_t)amp;
            e->period = (uint16_t)(150 + rint_(w, 90));
            e->phase = (uint16_t)rint_(w, e->period);
            e->x = (int32_t)x0 * FX;
            e->y = (int32_t)e->y0 * FX;
            w->last_pest_y = y;
            break;
        }
    }
    w->gen_y = y;
    w->last_path_y = y;
    w->last_path_l = p->x0;
    w->last_path_r = (int16_t)(p->x1 + p->w - 1);
    w->last_path_type = (uint8_t)type;
    last_path = p;
}

void world_generate(world *w)
{
    last_path = NULL;
    for (int i = 0; i < MAX_PLAT; i++)               /* the path's last leaf: the highest made */
        if (w->p[i].used && w->p[i].path && w->p[i].y / FX == w->last_path_y) last_path = &w->p[i];
    while (w->gen_y > w->cam_y - GEN_AHEAD) gen_row(w);
    last_path = NULL;
}

void world_init(world *w, uint32_t seed, int best_m)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    w->rng = seed * 2654435761u ^ 0x9e3779b9u;
    if (!w->rng) w->rng = 1;
    w->h.x = COL_W / 2 * FX;
    w->h.y = 0;
    w->h.vy = -JUMP_V;
    w->h.face = 1;
    w->cam_y = -(STAGE_H - 36);                     /* the ground 36 px above the stage's bottom */
    w->top_y = 0;
    w->gen_y = 0;
    w->last_path_y = 0;
    w->last_path_l = 0;                             /* the ground: the whole column */
    w->last_path_r = COL_W - 1;
    w->last_path_type = P_LEAF;
    w->last_pest_y = 0;
    w->last_item_y = 0;
    w->best_m = best_m;
    world_generate(w);
}

/* (tests, screenshots) the climb from m metres: the lemur bouncing off a lone leaf there (fall: not even that one),
 * the rows made from it */
void world_warp(world *w, int m, int fall)
{
    memset(w->p, 0, sizeof w->p);
    memset(w->c, 0, sizeof w->c);
    memset(w->e, 0, sizeof w->e);
    memset(w->s, 0, sizeof w->s);
    int y = -m * PX_PER_M, x = COL_W / 2 - 16;
    plat *p = plat_new(w, 1);
    p->type = P_LEAF;
    p->w = 32;
    p->x = (int32_t)x * FX;
    p->y = (int32_t)y * FX;
    p->x0 = p->x1 = (int16_t)x;
    p->path = 1;
    if (fall) p->used = 0;
    w->h.x = COL_W / 2 * FX;
    w->h.y = (int32_t)y * FX;
    w->h.vy = -JUMP_V;
    w->h.land_y = y;
    w->h.land_i = 0;
    w->cam_y = y - (STAGE_H - 60);
    w->top_y = y;
    w->gen_y = w->last_path_y = y;
    w->last_path_l = (int16_t)x;
    w->last_path_r = (int16_t)(x + 31);
    w->last_path_type = P_LEAF;
    w->last_pest_y = y - 200;
    w->last_item_y = y;
    w->zone = (uint8_t)zone_of(m);
    world_generate(w);
    if (fall) memset(w->p, 0, sizeof w->p);
}

/* ---- the step ---------------------------------------------------------------------------------------------------- */
static void fx_add(world *w, int kind, int x, int y, int arg)
{
    if (w->n_fx >= MAX_FX) return;
    w->fx[w->n_fx++] = (fxev){(int16_t)x, (int16_t)y, (uint8_t)kind, (uint8_t)arg};
}

/* does the span [a, b] (px) meet [c, d] across the wrap? */
static int span_hit(int a, int b, int c, int d)
{
    for (int k = -1; k <= 1; k++)
        if (b + k * COL_W >= c && a + k * COL_W <= d) return 1;
    return 0;
}

static void pest_pop(world *w, pest *e, int how)
{
    e->dead_t = 1;
    w->kills++;
    w->coins += PEST_COINS;
    fx_add(w, how, e->x / FX, e->y / FX, e->type);
}

static void land(world *w, plat *p)
{
    hero *h = &w->h;
    int px = p->x / FX, hx = h->x / FX;
    h->y = p->y;
    h->land_y = p->y / FX;
    h->land_i = (uint8_t)(p - w->p + 1);
    h->land_t = 8;
    switch (p->type) {
    case P_DRY:                                     /* it breaks: no bounce, the fall goes on */
        p->state = PS_BROKEN;
        p->fall_v = 0;
        h->land_t = 0;
        w->events |= EV_BREAK;
        fx_add(w, FX_BREAK, px + 16, p->y / FX, 0);
        return;
    case P_PUFF:                                    /* one bounce, then it bursts into seeds */
        h->vy = -JUMP_V;
        p->state = PS_GONE;
        w->events |= EV_PUFF;
        fx_add(w, FX_PUFF, px + 16, p->y / FX, 0);
        break;
    case P_SHROOM:
        h->vy = -SHROOM_V;
        p->t = 12;
        w->events |= EV_SHROOM;
        fx_add(w, FX_DUST, hx, p->y / FX, 1);
        break;
    case P_CLOUD:
        h->vy = -CLOUD_V;
        p->t = 16;
        w->events |= EV_CLOUD;
        fx_add(w, FX_CLOUD, hx, p->y / FX, 0);
        break;
    default:
        if (p->spring) {
            int sx = px + p->spring - 1;
            if (span_hit(hx - 2, hx + 2, sx - 2, sx + 14)) {
                h->vy = -SPRING_V;
                h->spin_t = 44;
                p->t = 14;
                w->events |= EV_SPRING;
                fx_add(w, FX_SPRING, sx + 6, p->y / FX, 0);
                break;
            }
        }
        h->vy = -bounce_v(p->type);
        p->t = 8;
        w->events |= EV_BOUNCE;
        fx_add(w, FX_DUST, hx, p->y / FX, 0);
        break;
    }
    w->bounces++;
}

static void hero_step(world *w, int in)
{
    hero *h = &w->h;
    int dir = h->knocked || w->over ? 0 : ((in & IN_RIGHT) ? 1 : 0) - ((in & IN_LEFT) ? 1 : 0);
    if (dir) {
        h->vx += dir * (h->vx * dir < 0 ? TURN : ACCEL);
        if (h->vx > MAX_VX) h->vx = MAX_VX;
        if (h->vx < -MAX_VX) h->vx = -MAX_VX;
        h->face = (int8_t)dir;
    } else if (h->vx > 0) {
        h->vx = imax(0, h->vx - DECEL);
    } else if (h->vx < 0) {
        h->vx = imin(0, h->vx + DECEL);
    }
    h->x += h->vx;
    while (h->x < 0) h->x += COL_W * FX;
    while (h->x >= COL_W * FX) h->x -= COL_W * FX;

    if (h->fly) {
        int target = -(h->fly == IT_ROCKET ? ROCKET_V : PROP_V);
        if (h->vy > target) h->vy = imax(target, h->vy - FLY_ACCEL);
        else h->vy = target;
        if ((w->t & 3) == 0) fx_add(w, FX_FLY, h->x / FX, h->y / FX, h->fly);
        if (--h->fly_t == 0) {
            h->fly = 0;
            w->events |= EV_FLY_END;
        }
    } else {
        h->vy = imin(h->vy + GRAVITY, MAX_FALL);
    }
    int32_t prev = h->y;
    h->y += h->vy;
    if (h->fly || h->knocked || w->over || h->vy <= 0) return;
    /* the landing: the highest leaf the feet crossed this frame */
    plat *hit = NULL;
    int hx = h->x / FX;
    for (int i = 0; i < MAX_PLAT; i++) {
        plat *p = &w->p[i];
        if (!p->used || p->state != PS_OK) continue;
        if (prev > p->y || h->y < p->y) continue;
        int px = p->x / FX;
        if (!span_hit(hx - FEET_HW, hx + FEET_HW, px, px + p->w - 1)) continue;
        if (!hit || p->y < hit->y) hit = p;
    }
    if (hit) land(w, hit);
    else if (prev <= 0 && h->y >= 0) {              /* the garden's ground (only while it is on the stage) */
        h->y = 0;
        h->land_y = 0;
        h->land_i = 0;
        h->vy = -JUMP_V;
        h->land_t = 8;
        w->events |= EV_BOUNCE;
        fx_add(w, FX_DUST, hx, 0, 0);
        w->bounces++;
    }
}

static void shots_step(world *w, int in)
{
    hero *h = &w->h;
    if (h->cool) h->cool--;
    if ((in & IN_SHOOT) && !h->knocked && !w->over &&
        (h->cool == 0 || (!(h->held & IN_SHOOT) && h->cool <= SHOT_REPEAT - SHOT_COOL))) {
        for (int i = 0; i < MAX_SHOTS; i++)
            if (!w->s[i].used) {
                w->s[i] = (shot){h->x, h->y - (HERO_H + 2) * FX, 1, 0, {0, 0}};
                h->cool = SHOT_REPEAT;
                h->shoot_t = 12;
                w->events |= EV_SHOT;
                break;
            }
    }
    for (int i = 0; i < MAX_SHOTS; i++) {
        shot *s = &w->s[i];
        if (!s->used) continue;
        s->y -= SHOT_V;
        if (++s->t > SHOT_LIFE) { s->used = 0; continue; }
        for (int k = 0; k < MAX_PEST; k++) {
            pest *e = &w->e[k];
            if (!e->used || e->dead_t) continue;
            if (iabs(wrap_dx(s->x / FX - e->x / FX)) < 11 && iabs(s->y / FX - e->y / FX) < 12) {
                pest_pop(w, e, FX_POP);
                w->events |= EV_POP;
                s->used = 0;
                break;
            }
        }
    }
}

static void pests_step(world *w, int32_t prev_feet)
{
    hero *h = &w->h;
    int hx = h->x / FX, feet = h->y / FX;
    for (int i = 0; i < MAX_PEST; i++) {
        pest *e = &w->e[i];
        if (!e->used) continue;
        if (e->dead_t) {
            if (++e->dead_t > 40) e->used = 0;
            e->y += 3 * FX;
            continue;
        }
        int ph = (int)((w->t + e->phase) % e->period), half = e->period / 2;
        int tri = ph < half ? -256 + ph * 512 / half : 256 - (ph - half) * 512 / (e->period - half);
        int bob = (int)((w->t + e->phase) % 64);
        bob = bob < 32 ? bob / 8 - 2 : 2 - (bob - 32) / 8;
        e->x = (int32_t)(e->x0 * FX + e->amp * tri);
        e->y = (int32_t)(e->y0 + bob) * FX;
        if (h->knocked || w->over) continue;
        int ex = e->x / FX, ey = e->y / FX;
        if (!span_hit(hx - HERO_HW, hx + HERO_HW, ex - 9, ex + 9) || feet < ey - 8 || feet - HERO_H > ey + 8) continue;
        if (h->fly) {
            pest_pop(w, e, FX_POP);
            w->events |= EV_POP;
        } else if (h->vy > 0 && prev_feet / FX <= ey - 2) {
            pest_pop(w, e, FX_STOMP);
            h->vy = -STOMP_V;
            h->land_t = 8;
            w->events |= EV_STOMP;
        } else if (h->shield_t) {
            h->shield_t = 0;
            pest_pop(w, e, FX_SHIELD);
            w->events |= EV_SHIELD_POP;
        } else if (!w->god) {
            h->knocked = 1;
            h->vx = 0;
            h->vy = -3 * FX;
            h->spin_t = 0;
            w->events |= EV_HURT;
        }
    }
}

static void coins_items_step(world *w)
{
    hero *h = &w->h;
    if (h->knocked || w->over) return;
    int hx = h->x / FX, hy = h->y / FX - HERO_H / 2;
    for (int i = 0; i < MAX_COIN; i++) {
        coin *c = &w->c[i];
        if (!c->used) continue;
        int dx = wrap_dx(hx - c->x / FX), dy = hy - c->y / FX;
        if (h->magnet_t && iabs(dx) + iabs(dy) < MAGNET_R) c->fly = 1;
        if (c->fly) {
            int sx = dx * FX / 5, sy = dy * FX / 5;
            if (iabs(sx) < 2 * FX) sx = dx > 0 ? 2 * FX : dx < 0 ? -2 * FX : 0;
            if (iabs(sy) < 2 * FX) sy = dy > 0 ? 2 * FX : dy < 0 ? -2 * FX : 0;
            c->x += sx;
            c->y += sy;
            if (c->x < 0) c->x += COL_W * FX;
            if (c->x >= COL_W * FX) c->x -= COL_W * FX;
            dx = wrap_dx(hx - c->x / FX);
            dy = hy - c->y / FX;
        }
        if (iabs(dx) < COIN_R && iabs(dy) < COIN_R + 2) {
            c->used = 0;
            w->coins++;
            w->events |= EV_COIN;
            fx_add(w, FX_COIN, c->x / FX, c->y / FX, 0);
        }
    }
    for (int i = 0; i < MAX_PLAT; i++) {
        plat *p = &w->p[i];
        if (!p->used || !p->item || p->state != PS_OK) continue;
        int ix = p->x / FX + p->w / 2, iy = p->y / FX - 9;
        if (!span_hit(hx - HERO_HW, hx + HERO_HW, ix - 7, ix + 7) || h->y / FX < iy - 7 || h->y / FX - HERO_H > iy + 7)
            continue;
        int k = p->item;
        p->item = IT_NONE;
        w->events |= EV_ITEM;
        fx_add(w, FX_ITEM, ix, iy, k);
        if (k == IT_PROP || k == IT_ROCKET) {
            h->fly = (uint8_t)k;
            h->fly_t = (uint16_t)(k == IT_ROCKET ? ROCKET_FRAMES : PROP_FRAMES);
            h->spin_t = 0;
        } else if (k == IT_SHIELD) {
            h->shield_t = SHIELD_FRAMES;
        } else {
            h->magnet_t = MAGNET_FRAMES;
            w->events |= EV_MAGNET;
        }
    }
}

static void plats_step(world *w)
{
    int bottom = w->cam_y + STAGE_H + KEEP_BELOW;
    for (int i = 0; i < MAX_PLAT; i++) {
        plat *p = &w->p[i];
        if (!p->used) continue;
        if (p->state == PS_GONE) { p->used = 0; continue; }
        if (p->t) p->t--;
        if (p->state == PS_BROKEN) {
            p->fall_v = (int16_t)imin(p->fall_v + GRAVITY, MAX_FALL);
            p->y += p->fall_v;
        } else if (p->vx) {
            p->x += p->vx;
            if (p->x < p->x0 * FX) { p->x = p->x0 * FX; p->vx = (int16_t)-p->vx; }
            if (p->x > p->x1 * FX) { p->x = p->x1 * FX; p->vx = (int16_t)-p->vx; }
        }
        if (p->y / FX > bottom) p->used = 0;
    }
    for (int i = 0; i < MAX_COIN; i++)
        if (w->c[i].used && w->c[i].y / FX > bottom) w->c[i].used = 0;
    for (int i = 0; i < MAX_PEST; i++)
        if (w->e[i].used && w->e[i].y0 > bottom) w->e[i].used = 0;
}

void world_step(world *w, int in)
{
    hero *h = &w->h;
    w->events = 0;
    w->n_fx = 0;
    w->t++;
    plats_step(w);
    int32_t prev_feet = h->y;
    hero_step(w, in);
    shots_step(w, in);
    pests_step(w, prev_feet);
    coins_items_step(w);
    h->held = (uint8_t)in;
    if (h->land_t) h->land_t--;
    if (h->shoot_t) h->shoot_t--;
    if (h->spin_t) h->spin_t--;
    if (h->shield_t && !--h->shield_t) w->events |= EV_SHIELD_POP;
    if (h->magnet_t) h->magnet_t--;
    int feet = h->y / FX;
    if (!w->over) {
        if (!h->knocked && feet - HERO_H - w->cam_y < CAM_LINE) w->cam_y = feet - HERO_H - CAM_LINE;
        if (feet < w->top_y) w->top_y = feet;
        if (feet - w->cam_y > FALL_LINE) {
            w->over = 1;
            w->fall_t = 0;
            h->fly = 0;
            w->events |= EV_FALL;
        }
        int m = world_height_m(w), z = zone_of(m);
        if (z > w->zone) {
            w->zone = (uint8_t)z;
            w->events |= EV_ZONE;
        }
        if (w->best_m > 0 && !w->best_passed && m > w->best_m) {
            w->best_passed = 1;
            w->events |= EV_BEST;
        }
        world_generate(w);
    } else {
        w->fall_t++;
        int target = feet - 150, d = target - w->cam_y;
        w->cam_y += imax(0, imin(d, 14));
    }
}

uint32_t world_hash(const world *w)
{
    uint32_t v = 2166136261u;
#define MIX(x) (v = (v ^ (uint32_t)(x)) * 16777619u)
    MIX(w->rng); MIX(w->t); MIX(w->h.x); MIX(w->h.y); MIX(w->h.vx); MIX(w->h.vy); MIX(w->h.fly); MIX(w->h.fly_t);
    MIX(w->h.shield_t); MIX(w->h.knocked); MIX(w->cam_y); MIX(w->top_y); MIX(w->coins); MIX(w->kills); MIX(w->over);
    for (int i = 0; i < MAX_PLAT; i++)
        if (w->p[i].used) { MIX(i); MIX(w->p[i].x); MIX(w->p[i].y); MIX(w->p[i].state); MIX(w->p[i].item); }
    for (int i = 0; i < MAX_PEST; i++)
        if (w->e[i].used) { MIX(w->e[i].x); MIX(w->e[i].y); MIX(w->e[i].dead_t); }
    for (int i = 0; i < MAX_COIN; i++)
        if (w->c[i].used) { MIX(w->c[i].x); MIX(w->c[i].y); }
#undef MIX
    return v;
}

/* ---- the bot: it aims for the highest leaf it can still land on, and shoots the pests above it ----------------- */
static int64_t isqrt64(int64_t v)
{
    if (v <= 0) return 0;
    int64_t r = 0, b = (int64_t)1 << 40;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}

/* frames until the feet come down through world line `top` (px), -1 if the bounce does not reach it: the discrete
 * steps of hero_step (y_n = y + n vy + g n (n + 1) / 2) */
static int frames_to(const hero *h, int top)
{
    int64_t b2 = 2 * (int64_t)h->vy + GRAVITY, d = ((int64_t)top * FX - h->y);     /* d > 0: below the feet */
    int64_t a = b2 * b2 + 8 * (int64_t)GRAVITY * d;
    if (a < 0) return -1;
    int64_t n = (-b2 + isqrt64(a) + 2 * GRAVITY - 1) / (2 * GRAVITY);
    if (n < 0) return -1;
    if ((int64_t)h->vy + GRAVITY * n <= 0) return -1;                             /* (still rising: the apex short) */
    return (int)n;
}

/* px the lemur can cover in t frames toward a side, moving at v0 (Q8, < 0: away) now */
static int reach_from(int v0, int t)
{
    int64_t d = 0;
    if (v0 < 0) {                                   /* first the turn */
        int n = (-v0 + TURN - 1) / TURN;
        if (n > t) n = t;
        d += (int64_t)v0 * n / 2;
        t -= n;
        v0 = 0;
    }
    int a = (MAX_VX - v0) / ACCEL;
    if (t <= a) d += (int64_t)v0 * t + (int64_t)ACCEL * t * t / 2;
    else d += (int64_t)v0 * a + (int64_t)ACCEL * a * a / 2 + (int64_t)MAX_VX * (t - a);
    return (int)(d / FX);
}

int bot_input(world *w)
{
    hero *h = &w->h;
    if (h->knocked || w->over) return 0;
    int in = 0, hx = h->x / FX, feet = h->y / FX;
    for (int i = 0; i < MAX_PEST; i++) {                  /* a pest above: a seed */
        const pest *e = &w->e[i];
        if (!e->used || e->dead_t) continue;
        int dx = wrap_dx(e->x / FX - hx), dy = feet - e->y / FX;
        if (dy > 0 && dy < 220 && iabs(dx) < 12 && (w->t & 1)) in |= IN_SHOOT;
    }
    if (h->fly) return in;
    int best = -1, best_score = -(1 << 30), tx = hx, above = 0;
    int bottom = w->cam_y + FALL_LINE - 4;
    for (int i = 0; i < MAX_PLAT; i++) {
        const plat *p = &w->p[i];
        if (!p->used || p->state != PS_OK || p->type == P_DRY) continue;
        int top = p->y / FX;
        if (top > bottom) continue;
        int t = frames_to(h, top);
        if (t < 0 || (h->vy < 0 && t < 2)) continue;
        if (top < feet && h->vy > 0) continue;            /* (above the feet while falling: passed) */
        int px = p->x / FX;
        if (p->vx && p->x1 > p->x0) {                     /* where it will be (it turns back at its ends) */
            int64_t L = (int64_t)(p->x1 - p->x0) * FX, u = p->x - (int64_t)p->x0 * FX;
            if (p->vx < 0) u = 2 * L - u;
            u = (u + (int64_t)(p->vx < 0 ? -p->vx : p->vx) * t) % (2 * L);
            px = p->x0 + (int)((u <= L ? u : 2 * L - u) / FX);
        }
        int cx = p->spring ? px + p->spring - 1 + 6 : px + p->w / 2;
        int dx = iabs(wrap_dx(cx - hx)) - (p->spring ? 3 : p->w / 2 - 3);
        int reach = reach_from(h->vx * (wrap_dx(cx - hx) < 0 ? -1 : 1), t) * 85 / 100 - 3;
        if (dx > 0 && dx > reach) continue;           /* (dx <= 0: over it already) */
        int risk = 0;
        for (int k = 0; k < MAX_PEST && !w->god; k++) {     /* a pest hovering just over it: rather another one */
            const pest *e = &w->e[k];
            if (e->used && !e->dead_t && iabs(wrap_dx(e->x / FX - cx)) < 30 && top - e->y / FX > 0 && top - e->y / FX < 70)
                risk = 300;
        }
        int up = top < h->land_y - 2;
        int score = -top * 4 + (p->spring ? 400 : 0) + (p->type == P_SHROOM ? 200 : 0) - (p->type == P_CLOUD ? 40 : 0) -
                    (p->type == P_PUFF ? 8 : 0) - (dx > 0 ? dx : 0) - (up ? 0 : 4000) - risk;   /* (not back down) */
#ifdef BOT_DEBUG
        if (w->t > BOT_DEBUG && w->t < BOT_DEBUG + 140) printf("  t%d cand %d top %d t %d dx %d reach %d score %d\n", w->t, i, top, t, dx, reach, score);
#endif
        if (i == w->bot_aim - 1) score += 160;           /* (it keeps to its leaf: no dithering between two) */
        if (score > best_score) {
            best_score = score;
            best = i;
            tx = cx;
            above = up;
        }
    }
    w->bot_aim = (uint8_t)(best >= 0 && above ? best + 1 : 0);
    if (!above) {
        /* nothing higher within reach this bounce: wait on the leaf, at its end nearest the next leaf up (a moving one
         * comes by, a cloud drifts) */
        const plat *cur = h->land_i ? &w->p[h->land_i - 1] : NULL, *nxt = NULL;
        if (cur && (!cur->used || cur->state != PS_OK || cur->y / FX != h->land_y)) cur = NULL;
        for (int i = 0; i < MAX_PLAT; i++) {
            const plat *p = &w->p[i];
            if (!p->used || p->state != PS_OK || p->type == P_DRY || p->y / FX >= h->land_y - 2) continue;
            if (!nxt || p->y > nxt->y) nxt = p;
        }
        if (nxt && (cur || !h->land_i)) {
            int nc = nxt->x / FX + nxt->w / 2;
            if (cur) {
                int l = cur->x / FX + 5, r = cur->x / FX + cur->w - 6, d = wrap_dx(nc - (l + r) / 2);
                tx = d > 0 ? (wrap_dx(nc - r) > 0 ? r : nc) : (wrap_dx(nc - l) < 0 ? l : nc);
            } else {
                tx = nc;                                  /* (the ground: anywhere) */
            }
            best = 0;
        }
    }
    if (best < 0) return in;
    int dx = wrap_dx(tx - hx);
    int stop = (int)((int64_t)h->vx * h->vx / (2 * DECEL) / FX);
    if (dx > 1) {
        if (!(h->vx > 0 && stop >= dx)) in |= IN_RIGHT;
    } else if (dx < -1) {
        if (!(h->vx < 0 && stop >= -dx)) in |= IN_LEFT;
    }
    return in;
}
