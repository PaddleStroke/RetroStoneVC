/*
 * Pogo Mamie: the physics of one bouncer and the shape of the roofs (pure functions, no drawing or sound).
 * Every number comes from tuning.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 */
#include "pm.h"

void mamie_reset(mamie *m, int32_t x, int y)
{
    mamie z = {0};
    *m = z;
    m->state = MS_READY;
    m->x = x * Q16_ONE;
    m->y = y * Q16_ONE;
    m->start_x = x;
    m->face = 1;
    m->land_y = y;
    m->land_kind = SF_ROOF;
    m->land_t = 100;
}

/* ---- roofs ------------------------------------------------------------------------------------------------------- */
/* The surface of a roof at world x (x0 <= x < x1): the first solid pixel row. The tiles are drawn to match
 * (tests/test_align.c checks the picture against it at every scroll phase). */
int bldg_surface(const bldg *b, int x)
{
    int k = x - b->x0, w = b->x1 - b->x0;
    if (b->sky1 && b->sky_broken && k >= b->sky0 && k < b->sky1) return b->top + PIT_DEPTH;
    switch (b->roof) {
    case RF_MANSARD:
        if (k < MANSARD_W) return b->top + (MANSARD_W - 1 - k);
        if (k >= w - MANSARD_W) return b->top + (k - (w - MANSARD_W));
        return b->top;
    case RF_PITCH: {
        int h = w / 4;                      /* ridge above the eaves: a 1:2 slope over half the width */
        int d = k < w / 2 ? k : w - 1 - k;
        return b->top + h - 1 - d / 2;
    }
    default:
        return b->top;
    }
}

int bump_top(const bldg *b, int k) { return b->top - b->bump_h[k]; }

int bldg_drawn_top(const bldg *b, int x)
{
    int s = bldg_surface(b, x);
    if (b->sky1 && x - b->x0 >= b->sky0 && x - b->x0 < b->sky1) s = b->top;   /* the skylight's frame */
    for (int k = 0; k < BUMP_MAX; k++)
        if (b->bump_x[k] >= 0 && x >= b->x0 + b->bump_x[k] && x < b->x0 + b->bump_x[k] + b->bump_w[k])
            s = s < bump_top(b, k) ? s : bump_top(b, k);
    return s;
}

/* downhill direction x steepness: -2 = a 45-degree slope going down to the left */
int bldg_slope(const bldg *b, int x)
{
    int k = x - b->x0, w = b->x1 - b->x0;
    if (b->sky1 && b->sky_broken && k >= b->sky0 && k < b->sky1) return 0;
    if (b->roof == RF_MANSARD) return k < MANSARD_W ? -2 : k >= w - MANSARD_W ? 2 : 0;
    if (b->roof == RF_PITCH) return k < w / 2 ? -1 : 1;
    return 0;
}

/* ---- motion ---------------------------------------------------------------------------------------------------------- */
int32_t vx_after_input(const mamie *m, int dir)
{
    int32_t vx = m->vx, vmax = m->croissant_t > 0 ? VX_BOOST : VX_MAX;
    int32_t cruise = m->croissant_t > 0 ? VX_BOOST : VX_CRUISE;
    if (dir > 0) {
        if (vx < vmax) vx = min32(vx + ACCEL_X, vmax);
        else vx = max32(vx - RELAX_X, vmax);
    } else if (dir < 0) {
        if (vx > VX_BACK) vx = max32(vx - BRAKE_X, VX_BACK);
        else vx = min32(vx + RELAX_X, VX_BACK);
    } else {
        if (vx < cruise) vx = min32(vx + RELAX_X, cruise);
        else if (vx > cruise) vx = max32(vx - RELAX_X, cruise);
    }
    return vx;
}

/* One frame in the air: air control, gravity, the horizontal move (walls stop her), the vertical move (a surface
 * crossed on the way down stops her: EV_LAND, h filled). The bounce itself is mamie_bounce(). */
int mamie_air_step(mamie *m, const terrain *T, int dir, hit *h)
{
    int ev = 0;
    m->vx = vx_after_input(m, dir);
    int umb = m->umbrella_t > 0 && m->vy > 0;
    m->vy += umb ? UMB_GRAVITY : GRAVITY;
    int32_t cap = umb ? UMB_MAX_FALL : MAX_FALL;
    if (m->vy > cap) m->vy = umb ? max32(cap, m->vy - 2 * GRAVITY) : cap;
    /* horizontal, with the wind */
    int feet = (int)(m->y >> 16);
    int32_t nx = m->x + m->vx + (T->wind ? T->wind(T->ctx, (int)(m->x >> 16), feet) : 0);
    int dxs = nx > m->x ? 1 : nx < m->x ? -1 : 0;
    if (dxs) {
        int lead = (int)(nx >> 16) + (dxs > 0 ? HALF_W : -HALF_W);
        if (T->solid(T->ctx, lead, feet)) {
            /* back off to the last free pixel (the body is [x - HALF_W, x + HALF_W]) */
            int k = 1;
            while (k < 24 && T->solid(T->ctx, lead - dxs * k, feet)) k++;
            int32_t cand = (int32_t)(lead - dxs * k - dxs * HALF_W) * Q16_ONE;
            if ((dxs > 0 && cand < m->x) || (dxs < 0 && cand > m->x)) cand = m->x;
            nx = cand;
            m->vx = 0;
            ev |= EV_BONK;
        }
    }
    /* vertical */
    int32_t ny = m->y + m->vy;
    if (m->vy > 0 && T->land(T->ctx, m->x, m->y, nx, ny, h)) {
        ny = h->y * Q16_ONE;
        ev |= EV_LAND;
    }
    m->x = nx;
    m->y = ny;
    if (m->vx > Q16(0.25)) m->face = 1;
    else if (m->vx < -Q16(0.25)) m->face = -1;
    return ev;
}

int32_t bounce_speed(int kind)
{
    switch (kind) {
    case BN_BIG: return V_BIG;
    case BN_SPRING: return V_SPRING;
    case BN_SLING: return V_SLING;
    default: return V_NORMAL;
    }
}

/* the bounce off surface h: a SET speed, like the reference (the same arc whatever the fall before) */
void mamie_bounce(mamie *m, const hit *h, int big)
{
    int kind = h->kind == SF_AWNING ? BN_SPRING : big ? BN_BIG : BN_NORMAL;
    m->vy = -bounce_speed(kind);
    m->bounce = kind;
    if (h->slope) m->vx += (h->slope > 0 ? 1 : -1) * (iabs(h->slope) == 2 ? NUDGE_45 : NUDGE_27);
    m->land_t = 0;
    m->land_kind = h->kind;
    m->land_y = (int)h->y;
}

/* ---- the course ------------------------------------------------------------------------------------------------------ */
int difficulty_at(int32_t x)
{
    int32_t m = x / PX_PER_M;
    if (m <= 0) return 0;
    if (m >= DIFF_FULL_M) return 1024;
    return (int)(m * 1024 / DIFF_FULL_M);
}

int district_at(int32_t x) { return x < 0 ? 0 : (int)((x / DISTRICT_PX) % DISTRICTS); }
int night_at(int32_t x) { return x < 0 ? 0 : (int)(x / DISTRICT_PX / DISTRICTS); }

int medal_of(int d)
{
    return d >= MEDAL_CAT ? 4 : d >= MEDAL_GOLD ? 3 : d >= MEDAL_SILVER ? 2 : d >= MEDAL_BRONZE ? 1 : 0;
}
