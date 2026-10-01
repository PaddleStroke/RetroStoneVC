/*
 * Pogo Mamie: the endless generator. Buildings come one after the other, each with the gap before it; every
 * gap is checked by simulating the worst case (a standstill take-off near the edge, the real physics, the wind,
 * the slopes' deflection) with a normal and a big bounce, and made easier until one of them lands. Then come the
 * hazards: TV antennas, pigeons walking on the roofs or flying in the gaps; the check runs again with them (it
 * must not touch an antenna, nor a pigeon anywhere along its path: it does not know when she will come), and the
 * hazards that leave no way through are taken away. Hot-air balloons drift high enough above every roof that a
 * normal bounce never touches them (she can always wait for one to drift by). The rescue props in the gaps
 * (awnings, flower pots, cradles, clotheslines, crumbling ledges) and the baguette are extras: a gap never needs
 * them.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 */
#include "pm.h"
#include <string.h>

const int gen_takeoffs[3] = TAKEOFFS;

static int rnd(world *w, int n) { return n <= 1 ? 0 : rs_rng_range(&w->rng, n); }
static int pct(world *w, int p) { return rnd(w, 100) < p; }
static int lerp_d(int d, int a, int b) { return a + (b - a) * d / 1024; }
static int rnd8(world *w, int lo, int hi) { return hi <= lo ? lo : lo + 8 * rnd(w, (hi - lo) / 8 + 1); }  /* lo..hi step 8 */

/* Two runs' seeds often differ in their low bits only: mix every bit into all of them first. */
static uint32_t seed_mix(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x ? x : 1;
}

int obj_new(world *w)
{
    if (w->no >= OBJ_MAX) return -1;
    memset(&w->o[w->no], 0, sizeof w->o[0]);
    return w->no++;
}

static void no_bumps(bldg *b)
{
    for (int k = 0; k < BUMP_MAX; k++) b->bump_x[k] = -1;
}

/* the highest pixel one can land on (the roof or a chimney) */
static int bldg_highest(const bldg *b)
{
    int top = b->top;
    for (int k = 0; k < BUMP_MAX; k++)
        if (b->bump_x[k] >= 0 && bump_top(b, k) < top) top = bump_top(b, k);
    return top;
}

static void hist_push(world *w, const bldg *b)
{
    int i = w->hist_n++ % HIST_MAX;
    w->hist_x0[i] = b->x0;
    w->hist_x1[i] = b->x1;
    w->hist_top[i] = (int16_t)bldg_highest(b);
}

void gen_reset(world *w)
{
    w->nb = 0;
    w->no = 0;
    w->next_index = 1;
    w->hist_n = 0;
    rs_rng_seed(&w->rng, seed_mix(w->seed));
    bldg *b = &w->b[w->nb++];
    memset(b, 0, sizeof *b);
    b->x0 = w->skip_x & ~7;
    b->x1 = b->x0 + 192;
    b->top = START_TOP;
    b->district = (uint8_t)district_at(b->x0);
    b->kind = b->district == 1 ? BK_QUAY : BK_HAUSS;
    b->roof = RF_FLAT;
    no_bumps(b);
    hist_push(w, b);
    w->items_due = b->x1 + ITEM_EVERY * PX_PER_M / 2;
    w->balloon_due = b->x0 + (int32_t)BALLOON_FROM_M * PX_PER_M;
    w->gust_end = 0;
}

/* ---- the worst-case reachability check ----------------------------------------------------------------------------- */
/* the hazards between two roofs: the antennas and the walking pigeons of both, the pigeons flying in the gap
 * (attached to the second), each pigeon anywhere along its path */
int gen_hazards(const world *w, int from, int to, hbox *out, int max)
{
    int32_t ia = w->b[from].index, ib = w->b[to].index;
    int n = 0;
    for (int i = 0; i < w->no && n < max; i++) {
        const obj *o = &w->o[i];
        if (o->state || (o->bld != ia && o->bld != ib)) continue;
        if (o->kind == OB_ANTENNA)
            out[n++] = (hbox){(int)o->x - ANTENNA_HALF, o->y - ANTENNA_H, (int)o->x + ANTENNA_HALF + 1, o->y};
        else if (o->kind == OB_PIGEON)
            n += pigeon_swept(o, out + n, max - n);
    }
    return n;
}

static int touches(const mamie *m, const hbox *hz, int n)
{
    hbox b = body_box((int)(m->x >> 16), (int)(m->y >> 16));
    for (int i = 0; i < n; i++)
        if (box_hit(b.x0, b.y0, b.x1, b.y1, &hz[i])) return 1;
    return 0;
}

/* a landing at x on building `to` from which she can hop on: never just before an antenna (from a stop, she
 * hops over it from ANTENNA_HOP px before it) nor on it */
static int landing_clear(const world *w, int to, int x)
{
    int32_t ib = w->b[to].index;
    for (int i = 0; i < w->no; i++) {
        const obj *o = &w->o[i];
        if (o->kind == OB_ANTENNA && o->bld == ib && x > o->x - ANTENNA_HOP && x < o->x + ANTENNA_HALF + HALF_W + 1)
            return 0;
    }
    return 1;
}

/* one arc: hold Right while x < aim_x, then brake to a stop; returns 1 when it lands on `to` (h, the landing) */
static int arc_to(const world *w, mamie *m, int to, int aim_x, const hbox *hz, int nh, hit *h)
{
    terrain T = world_terrain(w, 1);            /* buildings and wind only: no prop helps */
    for (int t = 0; t < 400; t++) {
        int x = (int)(m->x >> 16);
        int dir = x < aim_x ? 1 : m->vx > 0 ? -1 : 0;
        int e = mamie_air_step(m, &T, dir, h);
        if (touches(m, hz, nh)) return 0;
        if (e & EV_LAND) return h->idx == to && (m->x >> 16) >= w->b[to].x0;
        if ((m->y >> 16) > STREET_Y) return 0;
    }
    return 0;
}

int gen_reachable(const world *w, int from, int to, int takeoff, int big, int aim)
{
    const bldg *a = &w->b[from], *b = &w->b[to];
    int tx = (int)a->x1 - takeoff;
    if (tx < a->x0 + 8) return 0;
    hbox hz[48];
    int nh = gen_hazards(w, from, to, hz, 48);
    mamie m;
    memset(&m, 0, sizeof m);
    m.state = MS_AIR;
    m.x = (int32_t)tx * Q16_ONE;
    m.y = (int32_t)bldg_surface(a, tx) * Q16_ONE;
    m.vx = 0;                                   /* the worst case: a standstill (a slope still deflects it) */
    slope_deflect(bldg_slope(a, tx), bounce_speed(big ? BN_BIG : BN_NORMAL), &m.vx, &m.vy);
    hit h;
    if (!arc_to(w, &m, to, (int)b->x0 + aim, hz, nh, &h)) return 0;
    if (!landing_clear(w, to, (int)(m.x >> 16))) return 0;
    /* a slope throwing her back toward the gap: the recovery bounce (normal, holding Right until past the slope)
     * must land on `to`, away from the slope */
    int flat = (int)b->x0;
    while (flat < b->x1 - 1 && bldg_slope(b, flat) < 0) flat++;
    for (int r = 0; r < 2 && h.slope < 0; r++) {
        slope_deflect(h.slope, V_NORMAL, &m.vx, &m.vy);
        if (!arc_to(w, &m, to, flat + 8, hz, nh, &h)) return 0;
        if (!landing_clear(w, to, (int)(m.x >> 16))) return 0;
    }
    return h.slope >= 0;
}

/* the aims tried: just past the edge, then past each antenna near the start of the roof */
static int gen_aims(const world *w, int to, int *aims, int max)
{
    int n = 0;
    aims[n++] = LAND_MARGIN;
    int32_t ib = w->b[to].index;
    for (int i = 0; i < w->no && n < max; i++) {
        const obj *o = &w->o[i];
        if (o->kind == OB_ANTENNA && o->bld == ib) aims[n++] = (int)(o->x - w->b[to].x0) + ANTENNA_HALF + HALF_W + 4;
    }
    return n;
}

int gen_gap_ok(const world *w, int from, int to)
{
    int aims[4], na = gen_aims(w, to, aims, 4);
    for (int k = 0; k < na; k++)
        for (int big = 0; big < 2; big++)
            for (int i = 0; i < 3; i++)
                if (gen_reachable(w, from, to, gen_takeoffs[i], big, aims[k])) return 1;
    return 0;
}

/* ---- placing things -------------------------------------------------------------------------------------------------- */
/* the flat part of a roof, relative to x0: [lo, hi) (empty for a pitched roof) */
static void flat_part(const bldg *b, int *lo, int *hi)
{
    int w = (int)(b->x1 - b->x0);
    *lo = 0;
    *hi = w;
    if (b->roof == RF_MANSARD) { *lo = MANSARD_W; *hi = w - MANSARD_W; }
    if (b->roof == RF_PITCH) *hi = 0;
}

static int bump_clash(const bldg *b, int x0, int x1, int pad)
{
    for (int k = 0; k < BUMP_MAX; k++)
        if (b->bump_x[k] >= 0 && x0 < b->bump_x[k] + b->bump_w[k] + pad && b->bump_x[k] - pad < x1) return 1;
    if (b->sky1 && x0 < b->sky1 + pad && b->sky0 - pad < x1) return 1;
    return 0;
}

static void add_bumps(world *w, bldg *b)
{
    int lo, hi;
    flat_part(b, &lo, &hi);
    no_bumps(b);
    int bw = 16, n = 0, bh = 16;
    switch (b->kind) {
    case BK_HOUSE: case BK_HAUSS:
        n = pct(w, 65) + pct(w, 35);
        break;
    case BK_QUAY:
        bw = 24;
        n = 1 + pct(w, 50);
        break;
    case BK_BARGE:
        bw = 32;
        n = 1;
        lo += 16;
        hi -= 24;
        break;
    default:
        return;
    }
    for (int k = 0; k < n; k++) {
        if (b->kind == BK_HOUSE || b->kind == BK_HAUSS) bh = pct(w, 50) ? 16 : 24;
        int x0 = rnd8(w, lo + 16, hi - 16 - bw);
        if (x0 < lo + 16 || x0 + bw > hi - 16 || bump_clash(b, x0, x0 + bw, 8)) continue;
        b->bump_x[k] = (int16_t)x0;
        b->bump_w[k] = (uint8_t)bw;
        b->bump_h[k] = (uint8_t)bh;
    }
}

static void add_antenna(world *w, bldg *b, int ax)
{
    int i = obj_new(w);
    if (i < 0) return;
    obj *o = &w->o[i];
    o->kind = OB_ANTENNA;
    o->x = b->x0 + ax;
    o->y = b->top;
    o->w = 16;
    o->bld = b->index;
}

static void add_roof_things(world *w, int bi, int d)
{
    bldg *b = &w->b[bi];
    int lo, hi, wid = (int)(b->x1 - b->x0);
    flat_part(b, &lo, &hi);
    int flat_lo = lo > EDGE_KEEP ? lo : EDGE_KEEP, flat_hi = hi < wid - EDGE_KEEP ? hi : wid - EDGE_KEEP;
    int hz = lerp_d(d, HAZARD_EASY, HAZARD_HARD);
    /* a skylight */
    if ((b->kind == BK_HAUSS || b->kind == BK_HOUSE) && pct(w, hz / 2)) {
        int sw = pct(w, 50) ? 16 : 24;
        int s0 = rnd8(w, (flat_lo + 7) & ~7, flat_hi - sw);
        if (s0 >= flat_lo && s0 + sw <= flat_hi && !bump_clash(b, s0, s0 + sw, 8)) {
            b->sky0 = (int16_t)s0;
            b->sky1 = (int16_t)(s0 + sw);
        }
    }
    /* TV antennas on the flat part, from near the landing end to EDGE_KEEP before the take-off end */
    int ants[2], nant = 0;
    int a_lo = lo > ANTENNA_KEEP_L ? lo : ANTENNA_KEEP_L, a_hi = hi < wid - EDGE_KEEP ? hi : wid - EDGE_KEEP;
    int want = b->kind == BK_BARGE ? 0 : pct(w, lerp_d(d, ANTENNA_EASY, ANTENNA_HARD)) +
                                        (a_hi - a_lo >= 96 && pct(w, lerp_d(d, ANTENNA_EASY, ANTENNA_HARD) / 2));
    for (int k = 0; k < want && a_hi - a_lo >= 8; k++) {
        int ax = a_lo + rnd(w, a_hi - a_lo);
        if (bump_clash(b, ax - 12, ax + 12, 0)) continue;
        if (nant && iabs(ax - ants[0]) < ANTENNA_SPACE) continue;
        add_antenna(w, b, ax);
        ants[nant++] = ax;
    }
    /* a pigeon strutting on the roof, in a free stretch */
    if (pct(w, lerp_d(d, PIGEON_EASY, PIGEON_HARD))) {
        int p_lo = lo > PIGEON_KEEP_L ? lo : PIGEON_KEEP_L, p_hi = hi < wid - PIGEON_KEEP_R ? hi : wid - PIGEON_KEEP_R;
        int best0 = 0, best1 = 0, s = p_lo;
        for (int x = p_lo; x <= p_hi; x++) {
            int blocked = x == p_hi || bump_clash(b, x - PIGEON_W / 2, x + PIGEON_W / 2, 4);
            for (int k = 0; k < nant; k++) blocked |= iabs(x - ants[k]) < PIGEON_W / 2 + 6;
            if (blocked) {
                if (x - s > best1 - best0) { best0 = s; best1 = x; }
                s = x + 1;
            }
        }
        if (best1 - best0 >= 4) {
            int i = obj_new(w);
            if (i >= 0) {
                obj *o = &w->o[i];
                o->kind = OB_PIGEON;
                o->var = PG_WALK;
                o->x = b->x0;
                o->y = b->top;
                o->w = (int16_t)wid;
                o->a = (int16_t)best0;
                o->b = (int16_t)(best1 - 1);
                o->pos = (int32_t)(best0 + rnd(w, best1 - best0)) * Q16_ONE;
                o->dir = pct(w, 50) ? 1 : -1;
                o->t = (int16_t)rnd(w, 150);
                o->bld = b->index;
            }
        }
    }
    /* a power-up, now and then, floating in the bounce above the roof */
    if (b->x0 >= w->items_due && hi - lo >= 32) {
        int i = obj_new(w);
        if (i >= 0) {
            obj *o = &w->o[i];
            o->kind = OB_ITEM;
            o->var = (uint8_t)rnd(w, IT_COUNT);
            o->x = b->x0 + (lo + hi) / 2 - 8;
            o->y = b->top - 36;
            o->w = 16;
            o->bld = b->index;
        }
        w->items_due = b->x1 + (int32_t)ITEM_EVERY * PX_PER_M * (80 + rnd(w, 41)) / 100;
    }
}

static int edge_top(const bldg *b, int right) { return bldg_surface(b, right ? (int)b->x1 - 1 : (int)b->x0); }

static void add_gap_prop(world *w, int ai, int bi, int d)
{
    bldg *a = &w->b[ai], *b = &w->b[bi];
    int gap = (int)(b->x0 - a->x1);
    if (gap < 24 || a->district == 1 || b->district == 1 || !pct(w, lerp_d(d, PROP_EASY, PROP_HARD))) return;
    int i = obj_new(w);
    if (i < 0) return;
    obj *o = &w->o[i];
    o->bld = b->index;
    int ta = edge_top(a, 1), tb = edge_top(b, 0);
    int kind = rnd(w, 5);
    for (int tries = 0; tries < 5; tries++, kind = (kind + 1) % 5) {
        int side = pct(w, 50);                  /* 1: on the left building's right wall */
        int wall_top = side ? ta : tb;
        if (kind == 0 && gap >= 32) {           /* a café-style awning: springy */
            o->kind = OB_AWNING;
            o->w = 24;
            o->x = side ? a->x1 : b->x0 - 24;
            o->y = (int16_t)(((wall_top + 32 + 8 * rnd(w, 7)) + 7) & ~7);
            o->var = (uint8_t)side;
        } else if (kind == 1) {                 /* a window box of geraniums: breaks */
            o->kind = OB_POT;
            o->w = 16;
            o->x = side ? a->x1 : b->x0 - 16;
            o->y = (int16_t)(((wall_top + 24 + 8 * rnd(w, 7)) + 7) & ~7);
            o->var = (uint8_t)side;
        } else if (kind == 2 && gap >= 48) {    /* a window-cleaner's cradle, going up and down */
            o->kind = OB_CRADLE;
            o->w = 32;
            o->x = b->x0 - 32;
            o->a = (int16_t)(tb + 24);
            o->b = (int16_t)(o->a + 32 + 8 * rnd(w, 7));
            o->pos = (int32_t)(o->a + 8 * rnd(w, (o->b - o->a) / 8)) * Q16_ONE;
            o->dir = pct(w, 50) ? 1 : -1;
            o->y = (int16_t)tb;                 /* the davit on the roof */
        } else if (kind == 3 && gap >= 32) {    /* a clothesline across the gap: its ends at most gap/2 apart in height */
            o->kind = OB_LINE;
            o->x = a->x1;
            o->w = (int16_t)gap;
            o->y = (int16_t)(ta + 24 + 8 * rnd(w, 5));
            o->b = (int16_t)(tb + 24 + 8 * rnd(w, 5));
            o->b = (int16_t)clampi(o->b, o->y - gap / 2, o->y + gap / 2);
            o->a = (int16_t)LINE_REST_SAG(gap);
        } else if (kind == 4 && gap >= 40 && ta == a->top && a->roof == RF_FLAT) {   /* crumbling tiles */
            o->kind = OB_LEDGE;
            o->x = a->x1;
            o->w = (int16_t)(gap >= 56 ? 24 : 16);
            o->y = (int16_t)ta;
        } else {
            continue;
        }
        if (o->kind == OB_AWNING || o->kind == OB_POT)
            if (o->y > STREET_Y - 64) o->y = (int16_t)((STREET_Y - 64) & ~7);
        if (o->kind == OB_CRADLE && o->b > STREET_Y - 48) o->b = (int16_t)(STREET_Y - 48);
        return;
    }
    w->no--;                                    /* nothing fitted */
}

/* a pigeon flying in the gap before building bi: flapping in place, gliding or swooping, somewhere above the
 * higher of the two edges; returns its object (or -1) */
static int add_flyer(world *w, int ai, int bi, int d)
{
    bldg *a = &w->b[ai], *b = &w->b[bi];
    int gap = (int)(b->x0 - a->x1);
    if (gap < GAP_MIN || a->x1 < (int32_t)FLYER_FROM_M * PX_PER_M + w->skip_x || !pct(w, lerp_d(d, FLYER_EASY, FLYER_HARD)))
        return -1;
    int i = obj_new(w);
    if (i < 0) return -1;
    obj *o = &w->o[i];
    int hi_top = edge_top(a, 1) < edge_top(b, 0) ? edge_top(a, 1) : edge_top(b, 0);
    int way = gap >= 48 ? PG_HOVER + rnd(w, 3) : PG_HOVER;
    o->kind = OB_PIGEON;
    o->var = (uint8_t)way;
    o->bld = b->index;
    o->b = (int16_t)(FLY_PERIOD_MIN + rnd(w, FLY_PERIOD_MAX - FLY_PERIOD_MIN));
    o->t = (int16_t)rnd(w, o->b);
    o->dir = 1;
    switch (way) {
    case PG_HOVER:
        o->x = a->x1 + gap / 2;
        o->w = 0;
        o->a = FLY_HOVER_BOB;
        o->y = (int16_t)(hi_top - 16 - 8 * rnd(w, 9));
        break;
    case PG_GLIDE:
        o->x = a->x1 + FLY_KEEP;
        o->w = (int16_t)(gap - 2 * FLY_KEEP);
        o->y = (int16_t)(hi_top - 24 - 8 * rnd(w, 10));
        break;
    default:
        o->x = a->x1 + FLY_KEEP;
        o->w = (int16_t)(gap - 2 * FLY_KEEP);
        o->a = (int16_t)(FLY_SWOOP_MIN + rnd(w, FLY_SWOOP_MAX - FLY_SWOOP_MIN + 1));
        o->y = (int16_t)(hi_top - 56 - o->a / 2 - 8 * rnd(w, 6));
        break;
    }
    return i;
}

/* take away objects [from, no) of building index ib that match: kind, flying (1) or not (0) or any (-1) */
static void remove_objs(world *w, int from, int32_t ib, int kind, int flying)
{
    int k = from;
    for (int i = from; i < w->no; i++) {
        const obj *o = &w->o[i];
        int match = o->bld == ib && o->kind == kind && (flying < 0 || (o->var != PG_WALK) == flying);
        if (!match) w->o[k++] = *o;
    }
    w->no = k;
}

static void maybe_gust(world *w, int32_t x, int d)
{
    if (x < w->gust_end || x < (int32_t)GUST_FROM_M * PX_PER_M || !pct(w, lerp_d(d, GUST_EASY, GUST_HARD))) return;
    int i = obj_new(w);
    if (i < 0) return;
    obj *o = &w->o[i];
    o->kind = OB_GUST;
    o->x = x;                                   /* (from the gap on: the roofs before it were checked without it) */
    o->w = (int16_t)(160 + 8 * rnd(w, 21));
    o->dir = pct(w, 70) ? -1 : 1;               /* mostly a headwind */
    o->y = 0;
    w->gust_end = o->x + o->w + 256;
}

/* a hot-air balloon at the front, drifting left: its basket BALLOON_CLEAR (and more) above the highest roof it
 * can drift over (the last BALLOON_BACK px of roofs) */
static void maybe_balloon(world *w, int32_t front, int d, int32_t bld_index)
{
    if (front < w->balloon_due) return;
    int every = lerp_d(d, BALLOON_EVERY_EASY, BALLOON_EVERY_HARD);
    w->balloon_due = front + (int32_t)every * PX_PER_M * (70 + rnd(w, 61)) / 100;
    int32_t bx = front - 40;
    int hi = WORLD_H;
    int n = w->hist_n < HIST_MAX ? w->hist_n : HIST_MAX;
    for (int k = 0; k < n; k++)
        if (w->hist_x1[k] > bx - BALLOON_BACK && w->hist_x0[k] < bx + BALLOON_W && w->hist_top[k] < hi) hi = w->hist_top[k];
    int top = hi - BALLOON_CLEAR - 8 * rnd(w, 5) - BALLOON_HAZ_Y1;
    if (top < 8) return;
    int i = obj_new(w);
    if (i < 0) return;
    obj *o = &w->o[i];
    o->kind = OB_BALLOON;
    o->x = bx;
    o->pos = bx * Q16_ONE;
    o->y = (int16_t)top;
    o->w = BALLOON_W;
    o->dir = -1;
    o->var = (uint8_t)rnd(w, 3);                /* its stripes */
    o->bld = bld_index;
}

/* ---- the next building --------------------------------------------------------------------------------------------- */
static void gen_next(world *w)
{
    if (w->nb >= BLD_MAX) return;
    bldg *prev = &w->b[w->nb - 1];
    int32_t x = prev->x1;
    int night = night_at(x), dist = district_at(x);
    int d = difficulty_at(x);
    if (night) d = 1024 + (night < 4 ? night : 4) * 96;
    int dc = d > 1024 ? 1024 : d;
    maybe_gust(w, x, d);
    bldg nb;
    memset(&nb, 0, sizeof nb);
    no_bumps(&nb);
    nb.district = (uint8_t)dist;
    int r = rnd(w, 100), wid;
    switch (dist) {
    case 0: nb.kind = r < 65 ? BK_HOUSE : BK_HAUSS; break;
    case 1: nb.kind = r < 30 ? BK_QUAY : r < 70 ? BK_BARGE : BK_BRIDGE; break;
    case 2: nb.kind = r < 85 ? BK_HAUSS : BK_HOUSE; break;
    default: nb.kind = r < 75 ? BK_HAUSS : BK_HOUSE; break;
    }
    int top = prev->top, dh = lerp_d(dc, 32, 64);
    switch (nb.kind) {
    case BK_HOUSE:
        if (pct(w, lerp_d(dc, SLOPED_EASY, SLOPED_HARD))) { nb.roof = RF_PITCH; wid = pct(w, 50) ? 64 : 96; }
        else { nb.roof = RF_FLAT; wid = rnd8(w, BLD_W_MIN, 112); }
        top = prev->top + rnd8(w, -dh, dh) + 8;
        break;
    case BK_HAUSS:
        nb.roof = pct(w, 70) ? RF_MANSARD : RF_FLAT;
        wid = rnd8(w, 96, lerp_d(dc, 224, 144));
        wid &= ~15;
        top = prev->top + rnd8(w, -dh, dh) - 8;
        break;
    case BK_QUAY: wid = rnd8(w, 96, 176); top = rnd8(w, 320, 368); break;
    case BK_BRIDGE: wid = rnd8(w, 128, 224); top = rnd8(w, 280, 336); break;
    default: wid = rnd8(w, 96, 160); top = STREET_Y - 24; break;
    }
    /* never a drop the camera cannot show: a barge only below a low enough quay, a roof at most MAX_DROP lower */
    int prev_edge = bldg_surface(prev, (int)prev->x1 - 1);
    if (nb.kind == BK_BARGE && top > prev_edge + MAX_DROP) { nb.kind = BK_QUAY; wid = rnd8(w, 96, 176); top = rnd8(w, 320, 368); }
    if (nb.kind != BK_BARGE) top = clampi(top, ROOF_MIN_Y, ROOF_MAX_Y);
    if (top > prev_edge + MAX_DROP) top = (prev_edge + MAX_DROP) & ~7;
    top &= ~7;
    nb.style = (uint8_t)rnd(w, 4);
    int seine = dist == 1, min_gap = seine ? 32 : 0;
    int gap;
    if (seine || !pct(w, lerp_d(dc, NOGAP_EASY, NOGAP_HARD)))
        gap = rnd8(w, GAP_MIN > min_gap ? GAP_MIN : min_gap, lerp_d(d, GAP_MAX_EASY, GAP_MAX_HARD));
    else
        gap = 0;
    if (x < w->skip_x + (int32_t)WARMUP_M * PX_PER_M && !seine) gap = 0;     /* the warm-up: no gap */
    /* make it reachable in the worst case: narrower gap, closer height, then a plain roof */
    int ok = 0;
    for (int tries = 0; tries < 24 && !ok; tries++) {
        nb.top = (int16_t)top;
        nb.x0 = x + gap;
        nb.x1 = nb.x0 + wid;
        nb.index = w->next_index;
        w->b[w->nb++] = nb;
        ok = gen_gap_ok(w, w->nb - 2, w->nb - 1);
        w->nb--;
        if (ok) break;
        int lo_top = nb.kind == BK_BARGE ? STREET_Y - 24 : ROOF_MAX_Y;       /* a roof never sinks below its range */
        if ((tries & 1) && gap > min_gap) gap = gap - 8 < GAP_MIN && !seine ? 0 : gap - 8;
        else if (top != prev->top && (top > prev->top || top + 8 <= lo_top)) top += top < prev->top ? 8 : -8;
        if (top > prev_edge + MAX_DROP) top = (prev_edge + MAX_DROP) & ~7;
        else if (nb.roof != RF_FLAT) nb.roof = RF_FLAT;
        else if (gap > min_gap) gap -= 8;
    }
    if (!ok) {                                  /* the plain fallback: a flat roof at the same height, a small gap */
        nb.top = (int16_t)(nb.kind == BK_BARGE ? prev->top : clampi(prev->top, ROOF_MIN_Y, ROOF_MAX_Y));
        nb.roof = RF_FLAT;
        nb.x0 = x + (seine ? 32 : GAP_MIN);
        nb.x1 = nb.x0 + wid;
    }
    w->next_index++;
    w->b[w->nb++] = nb;
    int bi = w->nb - 1, first = w->no;
    add_bumps(w, &w->b[bi]);
    add_roof_things(w, bi, dc);
    add_gap_prop(w, bi - 1, bi, dc);
    add_flyer(w, bi - 1, bi, d > 1024 ? 1024 : d);
    /* the hazards must leave a way through: else the flying pigeon goes, then the antennas, then the walker */
    int32_t ib = w->b[bi].index;
    if (!gen_gap_ok(w, bi - 1, bi)) remove_objs(w, first, ib, OB_PIGEON, 1);
    if (!gen_gap_ok(w, bi - 1, bi)) remove_objs(w, first, ib, OB_ANTENNA, -1);
    if (!gen_gap_ok(w, bi - 1, bi)) remove_objs(w, first, ib, OB_PIGEON, -1);
    hist_push(w, &w->b[bi]);
    if (w->b[bi].x0 >= (int32_t)BALLOON_FROM_M * PX_PER_M + w->skip_x) maybe_balloon(w, w->b[bi].x1, dc, ib);
}

void gen_ahead(world *w, int32_t until_x)
{
    while (w->nb > 0 && w->b[w->nb - 1].x1 < until_x && w->nb < BLD_MAX) gen_next(w);
}

/* the baguette: a plank over the next gap ahead */
void gen_add_baguette(world *w, int32_t from_x)
{
    for (int i = 0; i + 1 < w->nb; i++) {
        const bldg *a = &w->b[i], *b = &w->b[i + 1];
        if (a->x1 <= from_x + 8 || b->x0 <= a->x1) continue;
        int j = obj_new(w);
        if (j < 0) return;
        obj *o = &w->o[j];
        o->kind = OB_BAGUETTE;
        o->x = a->x1 - 8;
        o->w = (int16_t)(b->x0 - a->x1 + 16);
        int ta = edge_top(a, 1), tb = edge_top(b, 0);
        o->y = (int16_t)(ta < tb ? ta : tb);
        o->bld = b->index;
        return;
    }
}
