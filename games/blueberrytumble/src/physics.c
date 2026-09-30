/*
 * Blueberry Tumble: the berry. One frame of motion and collisions on the flat grid (the slope is only drawn).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 *
 * Step order (tests/test_physics.c checks it against tuning.h):
 *   1. input: A held; a press = held now and not on the previous frame; the "release first" lock
 *   2. normal / snow: on the ground and A held (and not on ice): jump; in the air, a press on a dew drop: orb jump
 *      glide: A held rises, released sinks
 *   3. airborne: vy -= GRAVITY (capped at MAX_FALL); move: x = x_new, h += vy
 *   4. solids: land on the highest top crossed (or up to SNAP_UP above); lose the support at an edge;
 *      glide: slide along floors and ceilings; the core inside a solid = a splat
 *   5. hazards (thorns, pebbles, cones: smashed by a snowberry), pads, coins, the ground (snow, water, ice), gates
 *   6. fell into a crevasse
 * Integer fixed point only: the same inputs always give the same run (the solver relies on it).
 */
#include "bt.h"
#include <limits.h>
#include <string.h>

#define PX(v) ((int32_t)(v) * Q16_ONE)      /* heights (32-bit)  */
#define PXL(v) ((int64_t)(v) * Q16_ONE)     /* world x (64-bit) */
#define NO_TOP INT32_MIN

static const int8_t thorn_box[4] = THORN_BOX, hang_box[4] = HANG_BOX, pebble_box[4] = PEBBLE_BOX,
                    pad_box[4] = PAD_BOX;

int berry_size(const berry *b) { return b->mode == M_SNOW ? SNOW_SIZE : BERRY_SIZE; }
static int haz_r(const berry *b) { return b->mode == M_SNOW ? SNOW_HAZ : BERRY_HAZ; }
static int solid_r(const berry *b) { return b->mode == M_SNOW ? SNOW_SOLID : BERRY_SOLID; }
static int core_r(const berry *b) { return b->mode == M_SNOW ? SNOW_CORE : BERRY_CORE; }

void berry_reset(berry *b, int64_t x)
{
    memset(b, 0, sizeof *b);
    b->x = x;
    b->grounded = 1;
    b->orb_used = b->pad_used = b->coin_last = -1;
    b->land_t = 99;
    b->air_t = 99;
    b->grow_t = 99;
}

int berry_hash(const berry *b)
{
    uint32_t h = 2166136261u;
    const int32_t v[6] = {b->h, b->vy, b->mode | b->grounded << 2 | b->held << 3 | b->need_release << 4 |
                          b->on_ice << 5 | b->coin_got << 6, b->orb_used, b->pad_used, b->dead};
    for (int i = 0; i < 6; i++) h = (h ^ (uint32_t)v[i]) * 16777619u;
    return (int)(h & 0x7fffffff);
}

static int is_solid(int k) { return k == K_BLOCK || k == K_LOG; }
static int32_t floor_div16(int64_t xq) { return (int32_t)((xq >> 16) >> 4); }    /* Q16 px -> column (arithmetic shift) */

/* the highest top (Q16) in [lo, hi] under the columns [x0, x1) (Q16), or NO_TOP */
static int32_t best_top(const bt_course *c, int64_t x0, int64_t x1, int32_t lo, int32_t hi)
{
    int32_t best = NO_TOP;
    for (int32_t col = floor_div16(x0); col <= floor_div16(x1 - 1); col++) {
        const bt_col *k = course_col(c, col);
        if (k->ground != G_GAP && 0 >= lo && 0 <= hi && 0 > best) best = 0;
        for (int r = 0; r < ROWS; r++) {
            if (!is_solid(k->cell[r])) continue;
            int32_t t = PX((r + 1) * CELL);
            if (t >= lo && t <= hi && t > best) best = t;
        }
    }
    return best;
}

/* the lowest bottom (Q16) of a solid cell in [lo, hi] over the columns [x0, x1), or INT32_MAX */
static int32_t best_bottom(const bt_course *c, int64_t x0, int64_t x1, int32_t lo, int32_t hi)
{
    int32_t best = INT32_MAX;
    for (int32_t col = floor_div16(x0); col <= floor_div16(x1 - 1); col++) {
        const bt_col *k = course_col(c, col);
        for (int r = 0; r < ROWS; r++) {
            if (!is_solid(k->cell[r])) continue;
            int32_t bt = PX(r * CELL);
            if (bt >= lo && bt <= hi && bt < best) best = bt;
        }
    }
    return best;
}

/* does the box [x0, x1) x [y0, y1) (Q16) overlap a solid (the ground fills everything below 0) */
static int solid_overlap(const bt_course *c, int64_t x0, int64_t x1, int32_t y0, int32_t y1)
{
    for (int32_t col = floor_div16(x0); col <= floor_div16(x1 - 1); col++) {
        const bt_col *k = course_col(c, col);
        if (k->ground != G_GAP && y0 < 0) return 1;
        for (int r = 0; r < ROWS; r++)
            if (is_solid(k->cell[r]) && y0 < PX((r + 1) * CELL) && y1 > PX(r * CELL)) return 1;
    }
    return 0;
}

static int box_hit(int64_t ax0, int64_t ax1, int32_t ay0, int32_t ay1, int64_t bx0, int64_t bx1, int32_t by0, int32_t by1)
{
    return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

static void add_smash(step_info *info, int32_t id)
{
    if (info && info->nsmash < 6) info->smash_id[info->nsmash++] = id;
}

int berry_step(berry *b, const bt_course *c, int64_t x_new, int64_t x_ref, int held, step_info *info)
{
    int ev = 0;
    if (info) { info->nsmash = 0; info->coin_id = -1; }
    if (b->dead) return 0;
    int pressed = held && !b->held;
    if (b->need_release && !held) b->need_release = 0;
    int32_t dx = (int32_t)(x_new - b->x);
    int32_t h_old = b->h;

    /* 2. the action */
    if (b->mode == M_GLIDE) {
        b->vy += held ? GLIDE_LIFT : -GLIDE_SINK;
        if (b->vy > GLIDE_VMAX) b->vy = GLIDE_VMAX;
        if (b->vy < -GLIDE_VMAX) b->vy = -GLIDE_VMAX;
    } else {
        if (held && b->grounded && !b->need_release && !b->on_ice) {
            b->vy = b->mode == M_SNOW ? SNOW_JUMP_V : JUMP_V;
            b->grounded = 0;
            b->air_t = 0;
            ev |= EV_JUMP;
        } else if (pressed && !b->grounded) {
            /* a dew drop under the berry's solid box: a jump in mid-air, once per drop */
            int sr = solid_r(b), size = berry_size(b);
            for (int32_t col = floor_div16(b->x - PXL(sr + ORB_R)); col <= floor_div16(b->x + PXL(sr + ORB_R)); col++) {
                const bt_col *k = course_col(c, col);
                for (int r = 0; r < ROWS; r++) {
                    if (k->cell[r] != K_ORB || OBJ_ID(col, r) == b->orb_used) continue;
                    int64_t ox = PXL(col * CELL + CELL / 2);
                    int32_t oy = PX(r * CELL + CELL / 2);
                    if (box_hit(b->x - PX(sr), b->x + PX(sr), b->h, b->h + PX(size),
                                ox - PX(ORB_R), ox + PX(ORB_R), oy - PX(ORB_R), oy + PX(ORB_R))) {
                        b->vy = ORB_V;
                        b->orb_used = OBJ_ID(col, r);
                        b->air_t = 0;
                        ev |= EV_ORB;
                        goto orb_done;
                    }
                }
            }
        orb_done:;
        }
        /* 3. gravity */
        if (!b->grounded) {
            b->vy -= GRAVITY;
            if (b->vy < -MAX_FALL) b->vy = -MAX_FALL;
        }
    }
    b->x = x_new;
    b->h += b->vy;
    b->held = (uint8_t)(held != 0);

    /* 4. solids */
    int size = berry_size(b), sr = solid_r(b), cr = core_r(b);
    int64_t sx0 = b->x - PX(sr), sx1 = b->x + PX(sr);
    int was_grounded = b->grounded;
    if (b->vy <= 0) {
        int32_t hi = (h_old > b->h ? h_old : b->h) + PX(SNAP_UP);
        int32_t t = best_top(c, sx0, sx1, b->h, hi);
        if (t != NO_TOP) {
            if (!was_grounded) { ev |= EV_LAND; b->land_t = 0; }
            b->h = t;
            b->vy = 0;
            b->grounded = 1;
        } else {
            b->grounded = 0;
        }
    } else {
        b->grounded = 0;
        if (b->mode == M_GLIDE) {
            int32_t top_old = h_old + PX(size), top = b->h + PX(size);
            int32_t bt = best_bottom(c, sx0, sx1, top_old - PX(SNAP_UP), top);
            if (bt != INT32_MAX) { b->h = bt - PX(size); b->vy = 0; }
        }
    }
    if (b->mode == M_GLIDE && b->h + PX(size) > PX(GLIDE_CEIL)) {
        b->h = PX(GLIDE_CEIL - size);
        if (b->vy > 0) b->vy = 0;
    }
    int32_t cy = b->h + PX(size) / 2;
    if (solid_overlap(c, b->x - PX(cr), b->x + PX(cr), cy - PX(cr), cy + PX(cr))) {
        b->dead = 1;
        b->cause = D_WALL;
    }

    /* 5. hazards, pads, coins */
    int hr = haz_r(b);
    int64_t hx0 = b->x - PX(hr), hx1 = b->x + PX(hr);
    int32_t hy0 = cy - PX(hr), hy1 = cy + PX(hr);
    for (int32_t col = floor_div16(sx0) - 1; col <= floor_div16(sx1) + 1 && !b->dead; col++) {
        const bt_col *k = course_col(c, col);
        int64_t cx = PXL(col * CELL);
        for (int r = 0; r < ROWS; r++) {
            int kind = k->cell[r];
            const int8_t *bx = kind == K_THORN ? thorn_box : kind == K_HANG ? hang_box : kind == K_PEBBLE ? pebble_box : NULL;
            int32_t cyb = PX(r * CELL);
            if (bx) {
                if (box_hit(hx0, hx1, hy0, hy1, cx + PX(bx[0]), cx + PX(bx[1]), cyb + PX(bx[2]), cyb + PX(bx[3]))) {
                    if (b->mode == M_SNOW && kind != K_HANG) {
                        add_smash(info, OBJ_ID(col, r));
                        ev |= EV_SMASH;
                    } else {
                        b->dead = 1;
                        b->cause = D_HAZARD;
                        break;
                    }
                }
            } else if (kind == K_PAD && b->mode != M_GLIDE) {
                if (OBJ_ID(col, r) != b->pad_used &&
                    box_hit(sx0, sx1, b->h, b->h + PX(size), cx + PX(pad_box[0]), cx + PX(pad_box[1]),
                            cyb + PX(pad_box[2]), cyb + PX(pad_box[3]))) {
                    b->vy = PAD_V;
                    b->grounded = 0;
                    b->pad_used = OBJ_ID(col, r);
                    b->air_t = 0;
                    ev |= EV_PAD;
                }
            } else if (kind == K_COIN) {
                int64_t ox = cx + PX(CELL / 2);
                int32_t oy = cyb + PX(CELL / 2);
                if (OBJ_ID(col, r) != b->coin_last &&
                    box_hit(hx0, hx1, hy0, hy1, ox - PX(COIN_R), ox + PX(COIN_R), oy - PX(COIN_R), oy + PX(COIN_R))) {
                    b->coin_last = OBJ_ID(col, r);
                    b->coin_got = 1;
                    if (info) info->coin_id = OBJ_ID(col, r);
                    ev |= EV_COIN;
                }
            }
        }
    }
    /* the cones rolling toward player 1 */
    if (!b->dead)
        for (int32_t i = c->ncone - 1; i >= 0 && i >= c->ncone - CONE_RING; i--) {
            const bt_cone *k = &c->cone[i % CONE_RING];
            int64_t kx = cone_x(k, x_ref);
            if (kx < b->x - PX(64) || kx > b->x + PX(64)) continue;
            if (box_hit(hx0, hx1, hy0, hy1, kx - PX(CONE_R), kx + PX(CONE_R), 0, PX(2 * CONE_R))) {
                if (b->mode == M_SNOW) {
                    add_smash(info, -1 - i);
                    ev |= EV_SMASH;
                } else {
                    b->dead = 1;
                    b->cause = D_HAZARD;
                }
            }
        }

    /* the ground under the berry's centre, the gates */
    const bt_col *under = course_col(c, floor_div16(b->x));
    int on_ground_floor = b->grounded && b->h == 0;
    int ice = on_ground_floor && under->ground == G_ICE;
    if (ice && !b->on_ice) ev |= EV_ICE;
    b->on_ice = (uint8_t)ice;
    if (on_ground_floor && b->mode == M_NORMAL && under->ground == G_SNOW) {
        b->mode = M_SNOW;
        b->grow_t = 0;
        ev |= EV_GROW;
    } else if (on_ground_floor && b->mode == M_SNOW && under->ground == G_WATER) {
        b->mode = M_NORMAL;
        b->grow_t = 0;
        ev |= EV_SHRINK;
    }
    if ((under->flags & F_LEAF) && b->mode != M_GLIDE) {
        b->mode = M_GLIDE;
        b->grounded = 0;
        b->on_ice = 0;
        /* the leaf catches the berry on whole pixels (the glide moves in whole pixels) */
        b->h = (b->h + Q16_ONE / 2) & ~(Q16_ONE - 1);
        b->vy = (b->vy + Q16_ONE / 2) & ~(Q16_ONE - 1);
        if (b->vy > GLIDE_VMAX) b->vy = GLIDE_VMAX;
        if (b->vy < -GLIDE_VMAX) b->vy = -GLIDE_VMAX;
        ev |= EV_GLIDE;
    } else if ((under->flags & F_LEAF_END) && b->mode == M_GLIDE) {
        b->mode = M_NORMAL;
        b->need_release = 1;
        b->grounded = 0;
        ev |= EV_GLIDE_END;
    }

    /* 6. fell */
    if (b->h < -PX(FALL_DEATH) && !b->dead) {
        b->dead = 1;
        b->cause = D_FALL;
    }
    if (b->dead) {
        ev |= EV_DIE;
        b->dead_x = b->x;
        b->dead_h = b->h;
    }

    /* cosmetic: the roll (arc length / radius; it keeps spinning in the air, slides on ice) */
    if (!b->on_ice && b->mode != M_GLIDE)
        b->angle = (uint16_t)(b->angle + (int32_t)(((int64_t)dx * 65536 / (314 * size / 100)) >> 16));
    if (b->land_t < 99) b->land_t++;
    if (b->air_t < 99) b->air_t++;
    if (b->grow_t < 99) b->grow_t++;
    return ev;
}
