/*
 * Pogo Mamie: a run (the players, the roofs and what is on them, the camera, the cat). Pure game rules: no
 * drawing or sound, events are reported instead.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 */
#include "pm.h"
#include <limits.h>
#include <string.h>

int world_camx(const world *w) { return (int)(w->camx >> 8); }
int world_camy(const world *w) { return (int)(w->camy >> 8); }

const bldg *world_bldg_at(const world *w, int x)
{
    int i = world_bldg_index_at(w, x);
    return i < 0 ? 0 : &w->b[i];
}

int world_bldg_index_at(const world *w, int x)
{
    for (int i = 0; i < w->nb; i++)
        if (x >= w->b[i].x0 && x < w->b[i].x1) return i;
    return -1;
}

/* ---- surfaces of the things between and on the roofs ------------------------------------------------------------- */
int obj_line_y(const obj *o, int x)
{
    int k = clampi(x - (int)o->x, 0, o->w);
    int sag = o->a + o->t;
    return o->y + (o->b - o->y) * k / o->w + sag * 4 * k * (o->w - k) / (o->w * o->w);
}

/* the landing surface of a prop at x (its y), or INT_MIN when x is not on it */
static int prop_surface(const obj *o, int x, int *slope)
{
    *slope = 0;
    if (o->state) return INT_MIN;
    switch (o->kind) {
    case OB_PIGEON: {
        int cx = (int)o->x + (o->pos >> 16);
        return iabs(x - cx) <= PIGEON_W / 2 ? o->y - PIGEON_H : INT_MIN;
    }
    case OB_AWNING: case OB_POT: case OB_LEDGE: case OB_BAGUETTE:
        return x >= o->x && x < o->x + o->w ? o->y : INT_MIN;
    case OB_CRADLE:
        return x >= o->x && x < o->x + o->w ? (int)(o->pos >> 16) : INT_MIN;
    case OB_LINE:
        return x > o->x && x < o->x + o->w ? obj_line_y(o, x) : INT_MIN;
    default:
        return INT_MIN;
    }
}

static const int prop_sf[OB_KINDS] = {
    [OB_PIGEON] = SF_PIGEON, [OB_AWNING] = SF_AWNING, [OB_POT] = SF_POT, [OB_CRADLE] = SF_CRADLE,
    [OB_LINE] = SF_LINE, [OB_LEDGE] = SF_LEDGE, [OB_BAGUETTE] = SF_BAGUETTE};

static int land_impl(const world *w, int32_t y0, int32_t x1, int32_t y1, hit *h, int bonly)
{
    int fx = (int)(x1 >> 16), best = INT_MAX;
    for (int i = 0; i < w->nb; i++) {
        const bldg *b = &w->b[i];
        if (b->x1 <= fx - FOOT_HALF || b->x0 > fx + FOOT_HALF) continue;
        int xs = clampi(fx, (int)b->x0, (int)b->x1 - 1);
        int sy = bldg_surface(b, xs);
        /* a roof is walkable within STEP_UP: a foot a little under a slope still lands on it (no sinking) */
        if (y1 >= (int32_t)sy * Q16_ONE && y0 <= (int32_t)(sy + STEP_UP) * Q16_ONE && sy < best) {
            best = sy;
            int k = xs - (int)b->x0;
            h->kind = b->sky1 && k >= b->sky0 && k < b->sky1 ? (b->sky_broken ? SF_PIT : SF_SKY) : SF_ROOF;
            h->idx = i;
            h->sub = -1;
            h->y = sy;
            h->slope = bldg_slope(b, xs);
        }
        for (int k = 0; k < BUMP_MAX; k++) {
            if (b->bump_x[k] < 0) continue;
            int bx0 = (int)b->x0 + b->bump_x[k], bx1 = bx0 + b->bump_w[k];
            if (bx1 <= fx - FOOT_HALF || bx0 > fx + FOOT_HALF) continue;
            int by = bump_top(b, k);
            if (y1 >= (int32_t)by * Q16_ONE && y0 <= (int32_t)by * Q16_ONE && by < best) {
                best = by;
                h->kind = SF_BUMP;
                h->idx = i;
                h->sub = k;
                h->y = by;
                h->slope = 0;
            }
        }
    }
    if (!bonly)
        for (int i = 0; i < w->no; i++) {
            const obj *o = &w->o[i];
            if (!prop_sf[o->kind] || o->state) continue;
            int slope, sy = INT_MIN;
            for (int dx = -FOOT_HALF; dx <= FOOT_HALF && sy == INT_MIN; dx += FOOT_HALF)
                sy = prop_surface(o, fx + (dx == 0 ? 0 : dx), &slope);
            if (sy == INT_MIN) sy = prop_surface(o, fx, &slope);
            if (sy == INT_MIN) continue;
            int tol = o->kind == OB_CRADLE ? 1 : 0;     /* it moved this frame */
            if (y1 >= (int32_t)sy * Q16_ONE && y0 <= (int32_t)(sy + tol) * Q16_ONE && sy < best) {
                best = sy;
                h->kind = prop_sf[o->kind];
                h->idx = i;
                h->sub = -1;
                h->y = sy;
                h->slope = 0;
            }
        }
    return best != INT_MAX;
}

static int land_full(const void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, hit *h)
{
    (void)x0;
    return land_impl((const world *)ctx, y0, x1, y1, h, 0);
}

static int land_bld(const void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, hit *h)
{
    (void)x0;
    return land_impl((const world *)ctx, y0, x1, y1, h, 1);
}

static int solid_world(const void *ctx, int x, int feet)
{
    const bldg *b = world_bldg_at((const world *)ctx, x);
    return b && bldg_surface(b, x) < feet - STEP_UP;
}

static int32_t wind_world(const void *ctx, int x, int y)
{
    const world *w = (const world *)ctx;
    (void)y;
    for (int i = 0; i < w->no; i++) {
        const obj *o = &w->o[i];
        if (o->kind == OB_GUST && x >= o->x && x < o->x + o->w) return o->dir * GUST_V;
    }
    return 0;
}

terrain world_terrain(const world *w, int buildings_only)
{
    terrain t = {buildings_only ? land_bld : land_full, solid_world, wind_world, w};
    return t;
}

/* ---- set-up ---------------------------------------------------------------------------------------------------------- */
void world_init(world *w, int players, uint32_t seed, int skip_m)
{
    memset(w, 0, sizeof *w);
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    w->seed = seed;
    w->skip_x = (int32_t)skip_m * PX_PER_M;
    gen_reset(w);
    for (int p = 0; p < MAX_PLAYERS; p++) {
        mamie_reset(&w->m[p], w->skip_x + START_X - p * 24, START_TOP);
        w->m[p].start_x = START_X;              /* the distance counts from the first roof of Paris (skip too) */
        if (p >= w->players) w->m[p].state = MS_OFF;
    }
    w->m[1].face = 1;
    w->camx = (int32_t)(w->skip_x + START_X - CAM_X_SLOW) * 256;
    if (w->camx < 0) w->camx = 0;
    w->cam_focus = START_TOP * 256;
    gen_ahead(w, world_camx(w) + 2 * RS_SCREEN_W);
    world_camera(w, 1);
    /* the cat waits on the first perch ahead */
    w->c.x = w->skip_x + START_X + 160;
    w->c.y = START_TOP;
    w->c.state = 2;             /* "place me" */
}

/* ---- the players ------------------------------------------------------------------------------------------------------- */
static int leader_of(const world *w)
{
    int best = -1;
    for (int pass = 0; pass < 2 && best < 0; pass++)
        for (int p = 0; p < w->players; p++) {
            const mamie *m = &w->m[p];
            int alive = m->state == MS_READY || m->state == MS_AIR || m->state == MS_SLING || m->state == MS_REEL;
            if (m->state == MS_OFF || (pass == 0 && !alive)) continue;
            if (best < 0 || m->x > w->m[best].x) best = p;
        }
    return best < 0 ? 0 : best;
}

int world_running(const world *w)
{
    for (int p = 0; p < w->players; p++) {
        int s = w->m[p].state;
        if (s == MS_READY || s == MS_AIR || s == MS_SLING || s == MS_REEL) return 1;
    }
    return 0;
}

int world_all_down(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->m[p].state != MS_DOWN && w->m[p].state != MS_OFF) return 0;
    return 1;
}

int world_dist(const world *w)
{
    int d = 0;
    for (int p = 0; p < w->players; p++) d = w->m[p].dist_m > d ? w->m[p].dist_m : d;
    return d;
}

/* doomed: over a gap, below both roofs around it, and nothing to land on below */
int world_doomed(const world *w, const mamie *m)
{
    if (m->vy <= 0) return 0;
    int fx = (int)(m->x >> 16), feet = (int)(m->y >> 16);
    int top_l = INT_MAX, top_r = INT_MAX;
    for (int i = 0; i < w->nb; i++) {
        const bldg *b = &w->b[i];
        if (b->x1 > fx - FOOT_HALF && b->x0 <= fx + FOOT_HALF) return 0;        /* a roof under her */
        if (b->x1 <= fx) top_l = bldg_surface(b, (int)b->x1 - 1);               /* the last one on the left */
        else if (top_r == INT_MAX) top_r = bldg_surface(b, (int)b->x0);
    }
    int lowest_top = top_l == INT_MAX ? top_r : top_r == INT_MAX ? top_l : top_l > top_r ? top_l : top_r;
    if (lowest_top != INT_MAX && feet <= lowest_top) return 0;
    for (int i = 0; i < w->no; i++) {
        const obj *o = &w->o[i];
        int slope;
        if (!prop_sf[o->kind] || o->state) continue;
        for (int dx = -24; dx <= 24; dx += 4) {
            int sy = prop_surface(o, fx + dx, &slope);
            if (sy != INT_MIN && sy >= feet) return 0;
        }
    }
    return 1;
}

static int box_overlap(int ax0, int ay0, int ax1, int ay1, int bx0, int by0, int bx1, int by1)
{
    return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

static void take_item(world *w, int p, obj *o)
{
    mamie *m = &w->m[p];
    o->state = 1;
    m->item_got = o->var;
    w->events[p] |= EV_ITEM;
    switch (o->var) {
    case IT_UMBRELLA: m->umbrella_t = UMBRELLA_T; break;
    case IT_CROISSANT: m->croissant_t = CROISSANT_T; m->vx = VX_BOOST; break;
    case IT_YARN: m->yarn = 1; break;
    case IT_BAGUETTE: gen_add_baguette(w, (int)(m->x >> 16)); break;
    }
}

/* pigeons (from the side), antennas, power-ups: the body box against theirs */
static void touch(world *w, int p, int landed_on_pigeon)
{
    mamie *m = &w->m[p];
    int x = (int)(m->x >> 16), y = (int)(m->y >> 16);
    int bx0 = x - HALF_W, bx1 = x + HALF_W + 1, by0 = y - BODY_H, by1 = y;
    for (int i = 0; i < w->no; i++) {
        obj *o = &w->o[i];
        if (o->state) continue;
        if (o->kind == OB_PIGEON && i != landed_on_pigeon) {
            int cx = (int)o->x + (o->pos >> 16);
            if (m->knock_t == 0 &&
                box_overlap(bx0, by0, bx1, by1, cx - PIGEON_W / 2, o->y - PIGEON_H + 2, cx + PIGEON_W / 2, o->y)) {
                int from_left = x < cx;
                m->vx = from_left ? KNOCK_VX : -KNOCK_VX;
                if (m->vy > KNOCK_VY) m->vy = KNOCK_VY;
                m->knock_t = 24;
                m->chain = 0;
                o->state = 1;       /* it flies off */
                o->t = 0;
                o->dir = from_left ? 1 : -1;
                w->events[p] |= EV_KNOCK;
            }
        } else if (o->kind == OB_ANTENNA) {
            if (m->stumble_t == 0 &&
                box_overlap(bx0, by0, bx1, by1, (int)o->x - ANTENNA_HALF, o->y - ANTENNA_H, (int)o->x + ANTENNA_HALF + 1,
                            o->y)) {
                m->vx = 0;
                m->stumble_t = 30;
                m->chain = 0;
                o->t = 40;          /* it wobbles */
                w->events[p] |= EV_STUMBLE;
            }
        } else if (o->kind == OB_ITEM) {
            if (box_overlap(bx0, by0, bx1, by1, (int)o->x, o->y - 16, (int)o->x + 16, o->y)) take_item(w, p, o);
        }
    }
}

static void add_stunt(world *w, int p, int base)
{
    mamie *m = &w->m[p];
    m->chain = m->chain < CHAIN_MAX ? m->chain + 1 : CHAIN_MAX;
    if (m->chain > m->best_chain) m->best_chain = m->chain;
    m->stunt_pts = base * m->chain;
    m->stunts += m->stunt_pts;
    w->events[p] |= EV_STUNT;
}

/* a landing: what she landed on decides the bounce */
static void landed(world *w, int p, const hit *h, int big)
{
    mamie *m = &w->m[p];
    int *ev = &w->events[p];
    m->landings++;
    switch (h->kind) {
    case SF_SKY: {                      /* the glass breaks: she drops one floor, slowed down */
        bldg *b = &w->b[h->idx];
        b->sky_broken = 1;
        m->vx /= 2;
        m->vy = Q16(1.0);
        m->chain = 0;
        *ev |= EV_GLASS;
        m->land_kind = SF_SKY;
        return;
    }
    case SF_LINE: {
        obj *o = &w->o[h->idx];
        m->state = MS_SLING;
        m->t = 0;
        m->line = h->idx;
        m->line_x = (int)(m->x >> 16);
        m->vy = 0;
        m->vx = m->vx * 3 / 4;
        o->t = 0;
        m->land_t = 0;
        m->land_kind = SF_LINE;
        m->land_y = h->y;
        *ev |= EV_LAND;
        return;
    }
    default:
        break;
    }
    mamie_bounce(m, h, big);
    *ev |= EV_LAND;
    if (m->bounce == BN_BIG) { *ev |= EV_BIG; m->big_bounces++; }
    switch (h->kind) {
    case SF_BUMP: {                     /* a chimney top: a stunt, once per chimney */
        int32_t id = w->b[h->idx].index * 4 + h->sub;
        if (id != m->last_bump) add_stunt(w, p, PTS_CHIMNEY);
        m->last_bump = id;
        break;
    }
    case SF_PIGEON: {
        obj *o = &w->o[h->idx];
        o->state = 1;
        o->t = 0;
        o->dir = m->face;
        m->pigeons++;
        add_stunt(w, p, PTS_PIGEON);
        *ev |= EV_PIGEON;
        break;
    }
    case SF_AWNING:
        w->o[h->idx].t = 12;            /* the canvas gives */
        m->chain = 0;
        *ev |= EV_SPRING;
        break;
    case SF_POT: case SF_LEDGE:
        w->o[h->idx].state = 1;         /* one bounce, then it falls */
        w->o[h->idx].t = 0;
        m->chain = 0;
        *ev |= h->kind == SF_POT ? EV_POT : EV_BREAK;
        break;
    default:
        m->chain = 0;
        break;
    }
}

static void clamp_left(world *w, mamie *m)
{
    if (w->players > 1) return;
    int32_t minx = (int32_t)(world_camx(w) + HALF_W + 4) * Q16_ONE;
    if (m->x < minx) {
        m->x = minx;
        if (m->vx < 0) m->vx = 0;
    }
}

static void start_fall(world *w, int p)
{
    mamie *m = &w->m[p];
    if (m->yarn) {                       /* the knitting yarn hooks the nearest ledge */
        int fx = (int)(m->x >> 16), bestd = INT_MAX;
        for (int i = 0; i < w->nb; i++) {
            const bldg *b = &w->b[i];
            int ex[2] = {(int)b->x1 - 10, (int)b->x0 + 10};
            for (int k = 0; k < 2; k++) {
                int d = iabs(ex[k] - fx);
                if (b->x1 - b->x0 >= 24 && d < bestd) {
                    bestd = d;
                    m->reel_x = ex[k];
                    m->reel_y = bldg_surface(b, ex[k]);
                }
            }
        }
        if (bestd < 200) {
            m->yarn = 0;
            m->state = MS_REEL;
            m->t = 0;
            m->vx = m->vy = 0;
            m->falls_saved++;
            w->events[p] |= EV_SAVED;
            return;
        }
    }
    m->state = MS_FALL;
    m->t = 0;
    m->fall_phase = 0;
    m->reel_x = (int)(m->x >> 16);      /* the cafÃ© awning waits below */
    m->down_kind = district_at(m->reel_x) == 1 && night_at(m->reel_x) >= 0 ? 1 : 0;
    m->umbrella_t = 0;
    w->events[p] |= EV_DOOMED;
}

static void step_player(world *w, int p, int dir, int a)
{
    mamie *m = &w->m[p];
    int *ev = &w->events[p];
    terrain T = world_terrain(w, 0);
    hit h;
    m->t++;
    if (m->land_t < 1000) m->land_t++;
    switch (m->state) {
    case MS_READY: {                    /* bouncing in place: the title and "get ready" */
        m->vx = 0;
        int e = mamie_air_step(m, &T, 0, &h);
        m->vx = 0;
        if (e & EV_LAND) { mamie_bounce(m, &h, 0); m->vx = 0; *ev |= EV_LAND; }
        break;
    }
    case MS_AIR: {
        if (m->stumble_t) m->stumble_t--;
        if (m->knock_t) m->knock_t--;
        if (m->umbrella_t) m->umbrella_t--;
        if (m->croissant_t) m->croissant_t--;
        int e = mamie_air_step(m, &T, m->knock_t > 12 ? 0 : dir, &h);
        *ev |= e & EV_BONK;
        clamp_left(w, m);
        int pigeon = -1;
        if (e & EV_LAND) {
            if (h.kind == SF_PIGEON) pigeon = h.idx;
            landed(w, p, &h, a);
        }
        if (m->state == MS_AIR) touch(w, p, pigeon);
        if (m->state == MS_AIR && world_doomed(w, m)) start_fall(w, p);
        break;
    }
    case MS_SLING: {                    /* riding the clothesline down, then the slingshot */
        obj *o = &w->o[m->line];
        m->x += m->vx / 4;
        o->t = SLING_DEPTH * (m->t < SLING_FRAMES ? m->t : SLING_FRAMES) / SLING_FRAMES;
        m->y = (int32_t)obj_line_y(o, (int)(m->x >> 16)) * Q16_ONE;
        if (m->t >= SLING_FRAMES) {
            m->state = MS_AIR;
            m->vy = -V_SLING;
            m->bounce = BN_SLING;
            m->land_t = 0;
            o->t = 0;
            *ev |= EV_SLING;
        }
        break;
    }
    case MS_REEL: {                     /* the yarn pulls her up to the ledge */
        int32_t tx = (int32_t)m->reel_x * Q16_ONE, ty = (int32_t)m->reel_y * Q16_ONE;
        int left = YARN_REEL_T - m->t;
        if (left <= 0) {
            m->x = tx;
            m->y = ty;
            m->state = MS_AIR;
            m->vx = 0;
            m->vy = -V_NORMAL;
            m->bounce = BN_NORMAL;
            m->land_t = 0;
            m->land_y = m->reel_y;
            m->land_kind = SF_ROOF;
            *ev |= EV_LAND;
        } else {
            m->x += (tx - m->x) / left;
            m->y += (ty - m->y) / left;
        }
        break;
    }
    case MS_FALL: {                     /* the comic fall into the street (or the river) */
        m->vy = min32(m->vy + GRAVITY, MAX_FALL);
        m->vx = m->vx * 15 / 16;
        m->x += m->vx;
        m->y += m->vy;
        int feet = (int)(m->y >> 16);
        if (m->down_kind == 0 && m->fall_phase == 0 && m->vy > 0 && feet >= STREET_Y - 24 &&
            iabs((int)(m->x >> 16) - m->reel_x) < 24) {
            m->fall_phase = 1;          /* boing off the cafÃ© awning */
            m->y = (int32_t)(STREET_Y - 24) * Q16_ONE;
            m->vy = -Q16(3.0);
            m->vx = m->face * Q16(0.75);
            *ev |= EV_SPRING;
        } else if (feet >= STREET_Y + (m->down_kind ? 6 : 0)) {
            m->y = (int32_t)(STREET_Y + (m->down_kind ? 6 : 0)) * Q16_ONE;
            m->state = MS_DOWN;
            m->t = 0;
            m->vx = m->vy = 0;
            *ev |= EV_DOWN;
        }
        break;
    }
    default:
        break;
    }
    if (m->state == MS_AIR || m->state == MS_SLING || m->state == MS_REEL) {
        int d = ((int)(m->x >> 16) - (int)m->start_x) / PX_PER_M;
        d = d < 0 ? 0 : d;
        if (d > m->dist_m) m->dist_m = d;
    }
    m->score = m->dist_m + m->stunts;
    /* 2 players: too far behind the leader's camera, out */
    if (w->players > 1 && m->state != MS_OFF && m->state != MS_DOWN && (m->x >> 16) < world_camx(w) - DROP_OUT_X) {
        m->state = MS_OFF;
        m->t = 0;
        *ev |= EV_OUT;
    }
}

/* ---- props, pigeons, the camera, the cat --------------------------------------------------------------------------- */
static void step_objects(world *w)
{
    for (int i = 0; i < w->no; i++) {
        obj *o = &w->o[i];
        o->t++;
        switch (o->kind) {
        case OB_PIGEON:
            if (o->state == 0) {
                if ((o->t / 50) % 3 == 2) break;        /* stop and peck */
                o->pos += o->dir * PIGEON_SPEED;
                if ((o->pos >> 16) <= o->a) o->dir = 1;
                if ((o->pos >> 16) >= o->b) o->dir = -1;
            } else if (o->state == 1) {                 /* flying off */
                o->pos += o->dir * Q16(1.5);
                o->y -= o->t < 30 ? 2 : 1;
                if (o->t > 120) o->state = 2;
            }
            break;
        case OB_CRADLE: {
            o->pos += o->dir * Q16(0.5);
            if ((o->pos >> 16) <= o->a) { o->pos = (int32_t)o->a * Q16_ONE; o->dir = 1; }
            if ((o->pos >> 16) >= o->b) { o->pos = (int32_t)o->b * Q16_ONE; o->dir = -1; }
            break;
        }
        case OB_AWNING: case OB_ANTENNA:
            o->t -= 2;                                  /* (t counts down: the canvas, the wobble) */
            if (o->t < 0) o->t = 0;
            break;
        case OB_POT: case OB_LEDGE:
            if (o->state == 1) {
                o->y += 1 + o->t / 6;
                if (o->y > WORLD_H) o->state = 2;
            }
            break;
        default:
            break;
        }
    }
}

static void cleanup(world *w)
{
    int cx = world_camx(w) - 96, k = 0;
    for (int i = 0; i < w->nb; i++)
        if (w->b[i].x1 >= cx || i == w->nb - 1) w->b[k++] = w->b[i];
    w->nb = k;
    /* objects: a player may be riding a line (its index must not move while she does) */
    int riding = 0;
    for (int p = 0; p < w->players; p++) riding |= w->m[p].state == MS_SLING;
    if (riding) return;
    k = 0;
    for (int i = 0; i < w->no; i++) {
        obj *o = &w->o[i];
        int gone = o->state == 2 || (o->x + o->w + 32 < cx && o->kind != OB_GUST) ||
                   (o->kind == OB_GUST && o->x + o->w < cx) || (o->kind == OB_ITEM && o->state);
        if (!gone) w->o[k++] = *o;
    }
    w->no = k;
}

void world_camera(world *w, int snap)
{
    const mamie *m = &w->m[w->leader];
    int x = (int)(m->x >> 16), feet = (int)(m->y >> 16);
    int vxp = (int)(m->vx >> 8);                           /* Q8 px/frame */
    int32_t tx = (int32_t)(x - CAM_X_SLOW) * 256 + (int32_t)CAM_X_LEAD * vxp;
    if (m->state == MS_READY) tx = w->camx;
    /* 2 players: the camera follows the leader but waits for the other one while the leader stays on screen */
    for (int p = 0; p < w->players; p++) {
        const mamie *o = &w->m[p];
        if (p == w->leader || (o->state != MS_AIR && o->state != MS_SLING && o->state != MS_REEL)) continue;
        int32_t wait = (int32_t)((o->x >> 16) - CAM_WAIT_X) * 256, lead_max = (int32_t)(x - CAM_LEADER_MAX_X) * 256;
        if (wait < lead_max) wait = lead_max;
        if (tx > wait) tx = wait;
    }
    if (snap) w->camx = tx > 0 ? tx : 0;
    else if (tx > w->camx) w->camx += (tx - w->camx + CAM_SMOOTH_X - 1) / CAM_SMOOTH_X;
    if (w->camx < 0) w->camx = 0;
    /* vertical: centred on the roof she bounces on, then kept so that the roofs ahead (the highest and the
     * lowest, a quay above the river or a barge below it) stay in view: tall buildings are never cut off */
    if (m->land_t == 0 && (m->state == MS_AIR || m->state == MS_READY)) w->cam_focus = m->land_y * 256;
    int focus = (int)(w->cam_focus >> 8), hi = focus, lo = focus;
    for (int i = 0; i < w->nb; i++) {
        const bldg *b = &w->b[i];
        if (b->x1 < x - 16 || b->x0 > x + CAM_LOOK_AHEAD) continue;
        int top = b->top, edge = bldg_surface(b, (int)b->x0);
        hi = top < hi ? top : hi;
        lo = edge > lo ? edge : lo;
    }
    int32_t ty = (int32_t)(focus - CAM_FEET_Y) * 256;
    if (ty > (int32_t)(hi - CAM_HI_KEEP) * 256) ty = (int32_t)(hi - CAM_HI_KEEP) * 256;
    if (ty < (int32_t)(lo - CAM_LO_KEEP) * 256) ty = (int32_t)(lo - CAM_LO_KEEP) * 256;
    if (m->state == MS_READY) ty = (int32_t)(focus - CAM_FEET_Y_READY) * 256;   /* the title: the sky for the text */
    if (m->state == MS_FALL || m->state == MS_DOWN) ty = (int32_t)(feet - CAM_BOT_KEEP + 24) * 256;
    if ((feet - 32) * 256 - ty < CAM_TOP_KEEP * 256) ty = (int32_t)(feet - 32 - CAM_TOP_KEEP) * 256;
    if (feet * 256 - ty > CAM_BOT_KEEP * 256) ty = (int32_t)(feet - CAM_BOT_KEEP) * 256;
    int32_t maxy = (int32_t)(WORLD_H - RS_SCREEN_H) * 256;
    ty = ty < 0 ? 0 : ty > maxy ? maxy : ty;
    if (snap) w->camy = ty;
    else w->camy += (ty - w->camy) / CAM_SMOOTH_Y;
}

/* a perch for the cat between xmin and xmax: a chimney top first, else a roof */
static int cat_perch(const world *w, int32_t xmin, int32_t xmax, int32_t *px, int32_t *py)
{
    for (int i = 0; i < w->nb; i++) {
        const bldg *b = &w->b[i];
        for (int k = 0; k < BUMP_MAX; k++) {
            if (b->bump_x[k] < 0) continue;
            int32_t cx = b->x0 + b->bump_x[k] + b->bump_w[k] / 2;
            if (cx >= xmin && cx <= xmax) { *px = cx; *py = bump_top(b, k); return 1; }
        }
    }
    for (int i = 0; i < w->nb; i++) {
        const bldg *b = &w->b[i];
        int32_t cx = b->x0 + 16 > xmin ? b->x0 + 16 : xmin;
        if (cx <= xmax && cx < b->x1 - 16) { *px = cx; *py = bldg_surface(b, (int)cx); return 1; }
    }
    return 0;
}

static void step_cat(world *w)
{
    cat *c = &w->c;
    const mamie *m = &w->m[w->leader];
    int32_t lx = m->x >> 16;
    c->t++;
    if (c->state == 2) {                                   /* place it on the first perch ahead */
        if (cat_perch(w, c->x, c->x + 160, &c->x, &c->y)) c->state = 0;
        c->face = -1;
        return;
    }
    if (c->state == 0) {
        c->face = -1;                                      /* it looks back at her */
        int near = lx > c->x - 96 || c->x < world_camx(w) + 40;
        if (near && m->state != MS_READY && m->state != MS_DOWN && m->state != MS_FALL) {
            int32_t nx, ny;
            if (cat_perch(w, c->x + 120, c->x + 260, &nx, &ny) || cat_perch(w, lx + 140, lx + 400, &nx, &ny)) {
                c->fx = c->x;
                c->fy = c->y;
                c->tx = nx;
                c->ty = ny;
                c->state = 1;
                c->t = 0;
                c->face = 1;
                if ((w->t / 7) % 3 == 0) w->events[0] |= EV_MEOW;
            }
        }
    } else if (c->state == 1) {
        int dur = 24 + (int)((c->tx - c->fx) / 6);
        if (c->t >= dur) {
            c->state = 0;
            c->x = c->tx;
            c->y = c->ty;
            c->t = 0;
        } else {
            c->x = c->fx + (c->tx - c->fx) * c->t / dur;
            int arc = 4 * 36 * c->t * (dur - c->t) / (dur * dur);
            c->y = c->fy + (c->ty - c->fy) * c->t / dur - arc;
        }
    }
}

void world_step(world *w, const int dir[MAX_PLAYERS], const int a[MAX_PLAYERS], const int a_pressed[MAX_PLAYERS])
{
    w->t++;
    for (int p = 0; p < MAX_PLAYERS; p++) w->events[p] = 0;
    if (!w->started) {
        int go = 0;
        for (int p = 0; p < w->players; p++) go |= a_pressed[p];
        if (go) {
            w->started = 1;
            for (int p = 0; p < w->players; p++) {
                mamie *m = &w->m[p];
                if (m->state != MS_READY) continue;
                m->state = MS_AIR;
                m->vx = VX_CRUISE;
                m->t = 0;
                w->events[p] |= EV_START;
            }
        }
    }
    step_objects(w);
    for (int p = 0; p < w->players; p++) step_player(w, p, dir[p], a[p]);
    w->leader = leader_of(w);
    world_camera(w, 0);
    step_cat(w);
    gen_ahead(w, world_camx(w) + 2 * RS_SCREEN_W);
    cleanup(w);
}
