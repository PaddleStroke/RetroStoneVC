/*
 * Duck Parade: the lanes (what moves where, as a closed-form function of the frame number), the
 * generator (units: a hazard group and a grass run) and the fairness judge (an exact time-expanded
 * reachability search over the duck's own moves).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 */
#include "dp.h"
#include <string.h>

/* ---- geometry ---------------------------------------------------------------------------------------- */
int lane_is_ground(const lane *l) { return l->kind != LK_RIVER; }
int lane_is_safe_kind(const lane *l) { return l->kind == LK_GRASS || l->kind == LK_NEST || l->kind == LK_HEDGE; }

int lane_offset(const lane *l, uint32_t t) { return (int)(((int64_t)l->v * t) >> 8) % LOOP_PX; }

int mover_pos(const lane *l, int i, uint32_t t)
{
    int off = lane_offset(l, t);
    return modi(l->m[i].p0 + (l->down ? off : -off), LOOP_PX);
}

int field_y_of_loop(int p) { return modi(p, LOOP_PX) - LOOP_TOP; }

int train_len(const lane *l) { return l->cars * TRAIN_CAR_PX; }

int train_at(const lane *l, uint32_t t, int *y0, int *y1)
{
    *y0 = *y1 = 0;
    if (l->kind != LK_RAIL || !l->period) return 0;
    int cyc = (int)((t + l->phase) % l->period);
    if (cyc < TRAIN_WARN) return 1;
    int front = (int)(((int64_t)(cyc - TRAIN_WARN) * TRAIN_V) >> 8);   /* px of the loco inside the field */
    int tl = train_len(l);
    if (front - tl >= FIELD_H) return 0;
    int a = front - tl, b = front;
    if (l->down) { *y0 = a; *y1 = b; }
    else { *y0 = FIELD_H - b; *y1 = FIELD_H - a; }
    return 2;
}

/* does [a, a + alen) overlap [b, b + blen) on the loop? */
static int circ_overlap(int a, int alen, int b, int blen)
{
    int d = modi(a - b, LOOP_PX);
    return d < blen || d > LOOP_PX - alen;
}

int lane_hits(const lane *l, uint32_t t, int y0, int y1)
{
    if (l->kind == LK_RAIL) {
        int a, b;
        if (train_at(l, t, &a, &b) == 2) return y0 < b && a < y1;
        return 0;
    }
    if (l->kind != LK_ROAD && l->kind != LK_PARK) return 0;
    for (int i = 0; i < l->n; i++)
        if (circ_overlap(y0 + LOOP_TOP, y1 - y0, mover_pos(l, i, t), l->m[i].len)) return 1;
    return 0;
}

int lane_platform(const lane *l, uint32_t t, int centre)
{
    if (l->kind != LK_RIVER) return -1;
    int best = -1, bestd = 1 << 20;
    for (int i = 0; i < l->n; i++) {
        int d = modi(centre + LOOP_TOP - mover_pos(l, i, t), LOOP_PX);   /* 0..len-1: on it */
        int len = l->m[i].len;
        int dist = d < len ? 0 : d < len + RIDE_TOLERANCE ? d - len + 1 : d >= LOOP_PX - RIDE_TOLERANCE ? LOOP_PX - d : 1 << 20;
        if (dist < bestd) { bestd = dist; best = i; }
    }
    return bestd < (1 << 20) ? best : -1;
}

int lane_can_stand(const lane *l, uint32_t t, int y, int hit_h)
{
    if (y < 0 || y > FIELD_H - CELL) return 0;
    int row = y / CELL;
    switch (l->kind) {
    case LK_GRASS: case LK_HEDGE: return !(l->block >> row & 1);
    case LK_NEST: return 1;
    case LK_POND: return (l->block >> row & 1) != 0;
    case LK_RIVER: return lane_platform(l, t, y + CELL / 2) >= 0;
    default: return !lane_hits(l, t, y + (CELL - hit_h) / 2, y + (CELL + hit_h) / 2);
    }
}

uint16_t lane_free_rows(const lane *l)
{
    if (l->kind == LK_NEST) return (1 << ROWS) - 1;
    if (l->kind == LK_GRASS || l->kind == LK_HEDGE) return (uint16_t)(~l->block & ((1 << ROWS) - 1));
    if (l->kind == LK_POND) return l->block;
    return (1 << ROWS) - 1;
}

/* ---- the judge: bitsets of the 224 field px of a column (bit y = the duck's top at y) --------------- */
#define PW 7
typedef struct { uint32_t w[PW]; } pxs;
#define YMAX (FIELD_H - CELL)       /* 208: the lowest top a duck can have */

static inline void ps_clear(pxs *a) { memset(a, 0, sizeof *a); }
static inline int ps_any(const pxs *a) { uint32_t o = 0; for (int i = 0; i < PW; i++) o |= a->w[i]; return o != 0; }
static inline void ps_and(pxs *a, const pxs *b) { for (int i = 0; i < PW; i++) a->w[i] &= b->w[i]; }
static inline void ps_or(pxs *a, const pxs *b) { for (int i = 0; i < PW; i++) a->w[i] |= b->w[i]; }
static inline void ps_set(pxs *a, int y) { if (y >= 0 && y <= YMAX) a->w[y >> 5] |= 1u << (y & 31); }
static inline int ps_get(const pxs *a, int y) { return y >= 0 && y <= YMAX && (a->w[y >> 5] >> (y & 31) & 1); }

static void ps_range(pxs *a, int y0, int y1)       /* bits [y0, y1) */
{
    if (y0 < 0) y0 = 0;
    if (y1 > YMAX + 1) y1 = YMAX + 1;
    for (int y = y0; y < y1;) {
        if ((y & 31) == 0 && y + 32 <= y1) { a->w[y >> 5] = 0xffffffffu; y += 32; }
        else { a->w[y >> 5] |= 1u << (y & 31); y++; }
    }
}

static void ps_all(pxs *a) { ps_clear(a); ps_range(a, 0, YMAX + 1); }

/* bits move by d px (d > 0: down); bits leaving [0, YMAX] are lost */
static void ps_shift(pxs *out, const pxs *a, int d)
{
    ps_clear(out);
    if (d >= 0) {
        int ws = d >> 5, bs = d & 31;
        for (int i = PW - 1; i >= ws; i--) {
            uint32_t v = a->w[i - ws] << bs;
            if (bs && i - ws - 1 >= 0) v |= a->w[i - ws - 1] >> (32 - bs);
            out->w[i] = v;
        }
    } else {
        d = -d;
        int ws = d >> 5, bs = d & 31;
        for (int i = 0; i + ws < PW; i++) {
            uint32_t v = a->w[i + ws] >> bs;
            if (bs && i + ws + 1 < PW) v |= a->w[i + ws + 1] << (32 - bs);
            out->w[i] = v;
        }
    }
    /* keep [0, YMAX] only */
    pxs m;
    ps_all(&m);
    ps_and(out, &m);
}

static void ps_rows(pxs *a, uint16_t rows)
{
    ps_clear(a);
    for (int r = 0; r < ROWS; r++) if (rows >> r & 1) ps_set(a, r * CELL);
}

/* a platform position -> the ground row it snaps to (nearest row) */
static void ps_snap(pxs *out, const pxs *a)
{
    ps_clear(out);
    for (int r = 0; r < ROWS; r++) {
        int y0 = r * CELL - CELL / 2, y1 = r * CELL + CELL / 2;
        for (int y = y0 < 0 ? 0 : y0; y < y1 && y <= YMAX; y++)
            if (ps_get(a, y)) { ps_set(out, r * CELL); break; }
    }
}

/* where a duck may stand in column l at frame t */
static void safe_mask(pxs *out, const lane *l, uint32_t t)
{
    ps_clear(out);
    if (l->kind == LK_RIVER) {
        for (int i = 0; i < l->n; i++) {
            int a = field_y_of_loop(mover_pos(l, i, t));
            /* centre (y + 8) in [a - tol, a + len + tol) */
            ps_range(out, a - RIDE_TOLERANCE - CELL / 2, a + l->m[i].len + RIDE_TOLERANCE - CELL / 2);
        }
        return;
    }
    if (l->kind == LK_GRASS || l->kind == LK_HEDGE || l->kind == LK_NEST || l->kind == LK_POND) {
        ps_rows(out, lane_free_rows(l));
        return;
    }
    uint16_t rows = 0;
    for (int r = 0; r < ROWS; r++)
        if (!lane_hits(l, t, r * CELL + (CELL - DUCK_HIT_H) / 2, r * CELL + (CELL + DUCK_HIT_H) / 2)) rows |= 1 << r;
    ps_rows(out, rows);
}

/* free of anything that moves (the hop's halves); a river is "free" (the duck is in the air) */
static void free_mask(pxs *out, const lane *l, uint32_t t)
{
    if (l->kind == LK_ROAD || l->kind == LK_PARK || l->kind == LK_RAIL) safe_mask(out, l, t);
    else ps_all(out);
}

static int disp(const lane *l, uint32_t t0, uint32_t t1)
{
    if (l->kind != LK_RIVER) return 0;
    int d = (int)((((int64_t)l->v * t1) >> 8) - (((int64_t)l->v * t0) >> 8));
    return l->down ? d : -d;
}

#define JMAX 16
#define VFR 16                  /* ring of free masks, frames t .. t + HOP_FRAMES */
int judge_cross_from(const lane *cols, int n, int start_j, uint16_t start_rows, uint32_t t0, int window)
{
    static pxs S[JMAX], W[JMAX], arr[HOP_FRAMES + 1][JMAX], vf[VFR][JMAX];
    if (n > JMAX) n = JMAX;
    memset(arr, 0, sizeof arr);
    for (int j = 0; j < n; j++) {
        ps_clear(&S[j]);
        for (int k = 0; k < HOP_FRAMES; k++) free_mask(&vf[(t0 + k) % VFR][j], &cols[j], t0 + k);
    }
    pxs tmp, tmp2, src, pos;
    ps_rows(&S[start_j], start_rows);
    safe_mask(&tmp, &cols[start_j], t0);
    ps_and(&S[start_j], &tmp);
    for (int step = 0; step < window; step++) {
        uint32_t t = t0 + step;
        for (int j = 0; j < n; j++) free_mask(&vf[(t + HOP_FRAMES) % VFR][j], &cols[j], t + HOP_FRAMES);
        int slot = (int)((t + HOP_FRAMES) % (HOP_FRAMES + 1));
        for (int j = 0; j < n; j++) {
            if (!ps_any(&S[j])) { ps_clear(&W[j]); continue; }
            const lane *l = &cols[j];
            int river = l->kind == LK_RIVER;
            pxs vsrc, vdst;
            ps_all(&vsrc);
            for (int k = 0; k < HOP_MID; k++) ps_and(&vsrc, &vf[(t + k) % VFR][j]);
            /* forward and back */
            for (int dj = -1; dj <= 1; dj += 2) {
                int j2 = j + dj;
                if (j2 < 0 || j2 >= n) continue;
                src = S[j];
                if (!river) ps_and(&src, &vsrc);
                if (river && cols[j2].kind != LK_RIVER) ps_snap(&pos, &src);
                else pos = src;
                ps_all(&vdst);
                for (int k = HOP_MID; k < HOP_FRAMES; k++) ps_and(&vdst, &vf[(t + k) % VFR][j2]);
                ps_and(&pos, &vdst);
                ps_or(&arr[slot][j2], &pos);
            }
            /* up and down the lane */
            if (river) {
                int d = disp(l, t, t + HOP_FRAMES);
                ps_shift(&tmp, &S[j], -CELL + d);
                ps_shift(&tmp2, &S[j], CELL + d);
                ps_or(&tmp, &tmp2);
                ps_or(&arr[slot][j], &tmp);
            } else {
                src = S[j];
                ps_and(&src, &vsrc);
                ps_all(&vdst);
                for (int k = HOP_MID; k < HOP_FRAMES; k++) ps_and(&vdst, &vf[(t + k) % VFR][j]);
                ps_shift(&tmp, &src, -CELL);
                ps_shift(&tmp2, &src, CELL);
                ps_or(&tmp, &tmp2);
                ps_and(&tmp, &vdst);
                ps_or(&arr[slot][j], &tmp);
            }
            /* wait (carried on a platform) */
            if (river) ps_shift(&W[j], &S[j], disp(l, t, t + 1));
            else W[j] = S[j];
        }
        uint32_t t1 = t + 1;
        int s1 = (int)(t1 % (HOP_FRAMES + 1));
        for (int j = 0; j < n; j++) {
            S[j] = W[j];
            ps_or(&S[j], &arr[s1][j]);
            ps_clear(&arr[s1][j]);
            if (ps_any(&S[j])) {
                safe_mask(&tmp, &cols[j], t1);
                ps_and(&S[j], &tmp);
            }
        }
        if (ps_any(&S[n - 1])) return step + 1;
    }
    return -1;
}

/* ---- the generator --------------------------------------------------------------------------------------- */
int lanes_diff256(int32_t col)
{
    int lane = col - MEADOW_COLS;
    if (lane <= 0) return 0;
    return lane >= DIFF_LANES ? 256 : lane * 256 / DIFF_LANES;
}

/* speeds past DIFF_LANES: up to +DIFF_EXTRA_PCT % */
static int extra_pct(int32_t col)
{
    int lane = col - MEADOW_COLS - DIFF_LANES;
    if (lane <= 0) return 0;
    int span = DIFF_EXTRA_LANES - DIFF_LANES;
    return lane >= span ? DIFF_EXTRA_PCT : lane * DIFF_EXTRA_PCT / span;
}

static int rr(rs_rng *r, int lo, int hi) { return hi <= lo ? lo : lo + rs_rng_range(r, hi - lo + 1); }
static int lerp(int a, int b, int d256) { return a + (b - a) * d256 / 256; }

/* place n movers of the given lengths around the loop: top-to-top spacing in [smin, smax] px */
static void place_movers(lane *l, rs_rng *r, int smin, int smax)
{
    int total = 0;
    for (int i = 0; i < l->n; i++) total += smin;
    if (total > LOOP_PX) { l->n = (uint8_t)(LOOP_PX / smin); }
    int p = rs_rng_range(r, LOOP_PX), left = LOOP_PX;
    for (int i = 0; i < l->n; i++) {
        l->m[i].p0 = (int16_t)modi(p, LOOP_PX);
        int rest = l->n - 1 - i;
        int hi = left - rest * smin - smin;      /* keep room for the others and the wrap gap */
        int s = rest ? rr(r, smin, hi < smax ? hi : smax) : left;
        if (s < smin) s = smin;
        p += s;
        left -= s;
    }
}

static int speed(rs_rng *r, int vmin, int vmax_start, int vmax_end, int32_t col, int ease)
{
    int d = lanes_diff256(col);
    int vmax = lerp(vmax_start, vmax_end, d);
    vmax = vmax * (100 + extra_pct(col)) / 100;
    vmax -= (vmax - vmin) * ease / 4;
    return rr(r, vmin, vmax);
}

static void make_road(lane *l, rs_rng *r, int32_t col, int ease)
{
    int d = lanes_diff256(col);
    l->kind = LK_ROAD;
    l->down = (uint8_t)rs_rng_range(r, 2);
    l->v = (uint16_t)speed(r, CAR_V_MIN, CAR_V_START_MAX, CAR_V_MAX, col, ease);
    int nmax = lerp(CARS_START_MAX, CARS_MAX, d) - ease / 2, nmin = 1 + (d >= 128);
    if (nmax < 1) nmax = 1;
    if (nmin > nmax) nmin = nmax;
    l->n = (uint8_t)rr(r, nmin, nmax);
    int kind = rs_rng_range(r, 100);
    if (kind < BIKE_CHANCE) {                       /* a bike lane: quick and short */
        l->v = (uint16_t)(l->v * BIKE_V_MUL_16 / 16);
        l->n = (uint8_t)(l->n + 1);
        for (int i = 0; i < l->n; i++) { l->m[i].kind = MV_BIKE; l->m[i].len = 12; }
        place_movers(l, r, 4 * CELL, 8 * CELL);
        l->deco = 1;
        return;
    }
    for (int i = 0; i < l->n; i++) { l->m[i].kind = (uint8_t)(MV_CAR0 + rs_rng_range(r, 6)); l->m[i].len = 22; }
    if (kind < BIKE_CHANCE + BUS_CHANCE) {          /* a bus among the cars: slower lane */
        l->v = (uint16_t)(l->v * BUS_V_MUL_16 / 16);
        if (l->v < CAR_V_MIN) l->v = CAR_V_MIN;
        l->m[0].kind = MV_BUS;
        l->m[0].len = 40;
        if (l->n > 2) l->n = 2;
    }
    place_movers(l, r, CAR_GAP_MIN * CELL + (l->m[0].kind == MV_BUS ? CELL : 0), 8 * CELL);
}

static void make_park(lane *l, rs_rng *r, int32_t col, int ease)
{
    int d = lanes_diff256(col);
    l->kind = LK_PARK;
    l->down = (uint8_t)rs_rng_range(r, 2);
    if (rs_rng_range(r, 100) < MOWER_CHANCE) {
        l->v = MOWER_V;
        l->n = (uint8_t)(1 + (d >= 160 && ease == 0));
        for (int i = 0; i < l->n; i++) { l->m[i].kind = MV_MOWER; l->m[i].len = 20; }
        place_movers(l, r, 9 * CELL, 14 * CELL);
        l->deco = 1;
        return;
    }
    l->v = (uint16_t)speed(r, JOG_V_MIN, (JOG_V_MIN + JOG_V_MAX) / 2, JOG_V_MAX, col, ease);
    /* groups of joggers: a group is 1..3 runners 14 px apart */
    int groups = rr(r, 1, 2 + (d >= 128) - (ease >= 2)), n = 0;
    if (groups < 1) groups = 1;
    int p = rs_rng_range(r, LOOP_PX), span = LOOP_PX / groups;
    for (int g = 0; g < groups && n < MOVER_MAX; g++) {
        int k = rr(r, 1, 2 + (d >= 96) - (ease >= 1));
        if (k < 1) k = 1;
        int q = p + g * span + rs_rng_range(r, span / 3);
        for (int i = 0; i < k && n < MOVER_MAX; i++, n++) {
            l->m[n].kind = MV_JOGGER;
            l->m[n].len = 12;
            l->m[n].p0 = (int16_t)modi(q + i * 14, LOOP_PX);
        }
    }
    l->n = (uint8_t)n;
}

static void make_river(lane *l, rs_rng *r, int32_t col, int ease, int down)
{
    int d = lanes_diff256(col);
    l->kind = LK_RIVER;
    l->down = (uint8_t)down;
    l->v = (uint16_t)speed(r, LOG_V_MIN, LOG_V_START_MAX, LOG_V_MAX, col, ease);
    int what = rs_rng_range(r, 100);
    /* the water between two platforms: a duck waits at most ~110 frames for the next one (or 2 cells) */
    int gap_max = (int)((int64_t)l->v * 110 >> 8);
    if (gap_max < 2 * CELL) gap_max = 2 * CELL;
    if (gap_max > 5 * CELL) gap_max = 5 * CELL;
    int gap_min = CELL + CELL / 2;
    int pads = what < PADS_CHANCE, boat = !pads && what < PADS_CHANCE + BOAT_CHANCE;
    int lmin = 3 - (d >= 128), lmax = 4 - (d >= 192);
    if (ease) { lmin = 3; lmax = 4; }
    if (pads) { lmin = lmax = 1; gap_min = CELL / 2; gap_max = gap_max * 2 / 3 < CELL ? CELL : gap_max * 2 / 3; }
    /* platforms until the water left fits in the gaps (each gap <= gap_max) */
    int lens[MOVER_MAX], n = 0, sum = 0;
    while (n < MOVER_MAX) {
        int len = boat && n == 0 ? 2 * CELL : rr(r, lmin, lmax) * CELL;
        if (sum + len + (n + 1) * gap_min > LOOP_PX) break;
        lens[n++] = len;
        sum += len;
        if (LOOP_PX - sum <= n * gap_max) break;
    }
    /* share the water between the gaps, each in [gap_min, gap_max] (as far as it goes) */
    int water = LOOP_PX - sum, extra = water - n * gap_min, span = gap_max - gap_min;
    int p = rs_rng_range(r, LOOP_PX);
    for (int i = 0; i < n; i++) {
        int rest = n - 1 - i;
        int lo = extra - rest * span, hi = extra;
        if (lo < 0) lo = 0;
        if (hi > span) hi = span;
        int e = rest ? rr(r, lo, hi) : extra;
        extra -= e;
        l->m[i].len = (uint8_t)lens[i];
        l->m[i].kind = (uint8_t)(pads ? MV_PAD : boat && i == 0 ? MV_BOAT :
                                 lens[i] == 2 * CELL ? MV_LOG2 : lens[i] == 3 * CELL ? MV_LOG3 : MV_LOG4);
        l->m[i].p0 = (int16_t)modi(p, LOOP_PX);
        p += lens[i] + gap_min + e;
    }
    l->n = (uint8_t)n;
    l->deco = (uint8_t)(pads ? 2 : boat ? 1 : 0);
}

static void make_rail(lane *l, rs_rng *r, int32_t col, int ease)
{
    int d = lanes_diff256(col);
    l->kind = LK_RAIL;
    l->down = (uint8_t)rs_rng_range(r, 2);
    int pmin = lerp(TRAIN_PERIOD_START_MIN, TRAIN_PERIOD_MIN, d), pmax = lerp(TRAIN_PERIOD_START_MAX, TRAIN_PERIOD_MAX, d);
    pmin += ease * 60;
    pmax += ease * 60;
    l->period = (uint16_t)rr(r, pmin, pmax);
    l->phase = (uint16_t)rs_rng_range(r, l->period);
    l->cars = (uint8_t)rr(r, TRAIN_CARS_MIN, TRAIN_CARS_MAX);
    l->n = 0;
}

static void make_pond(lane *l, rs_rng *r, uint16_t prev_free, int ease)
{
    l->kind = LK_POND;
    int n = rr(r, POND_PADS_MIN + ease, POND_PADS_MAX + ease);
    uint16_t pads = 0;
    for (int k = 0; k < n; k++) pads |= (uint16_t)(1 << rs_rng_range(r, ROWS));
    /* the clone's rule: keep pads in reach of the cells before (two, when it can) */
    int inreach = 0;
    for (int row = 0; row < ROWS; row++) if ((pads & prev_free) >> row & 1) inreach++;
    for (int k = 0; inreach < 2 && k < 32; k++) {
        int row = rs_rng_range(r, ROWS);
        if ((prev_free >> row & 1) && !(pads >> row & 1)) { pads |= (uint16_t)(1 << row); inreach++; }
    }
    l->block = pads;
    l->n = 0;
    l->deco = (uint8_t)rs_rng_range(r, 4);
}

static void make_grass(lane *l, rs_rng *r, int trees_max)
{
    memset(l, 0, sizeof *l);
    l->kind = LK_GRASS;
    int n = rr(r, 0, trees_max);
    for (int k = 0; k < n; k++) l->block |= (uint16_t)(1 << rs_rng_range(r, ROWS));
    l->deco = (uint8_t)rs_rng_range(r, 8);
}

static int free_count(uint16_t rows) { int c = 0; for (int i = 0; i < ROWS; i++) c += rows >> i & 1; return c; }

/* the free cells of a grass run must be connected: a clear corridor, then unreachable pockets become bushes */
static void connect_run(lane *run, int n, rs_rng *r, uint16_t entry_rows)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        int corridor = rs_rng_range(r, ROWS);
        if (entry_rows && !(entry_rows >> corridor & 1)) {   /* prefer a row the entry can use */
            for (int k = 0; k < ROWS; k++) if (entry_rows >> ((corridor + k) % ROWS) & 1) { corridor = (corridor + k) % ROWS; break; }
        }
        for (int j = 0; j < n; j++) if (run[j].kind == LK_GRASS) run[j].block &= (uint16_t)~(1 << corridor);
        /* flood fill from the corridor */
        uint16_t seen[16] = {0};
        for (int j = 0; j < n; j++) seen[j] = (uint16_t)(1 << corridor) & lane_free_rows(&run[j]);
        for (int changed = 1; changed;) {
            changed = 0;
            for (int j = 0; j < n; j++) {
                uint16_t fr = lane_free_rows(&run[j]);
                uint16_t s = seen[j];
                s |= (uint16_t)((s << 1 | s >> 1) & fr);
                if (j > 0) s |= seen[j - 1] & fr;
                if (j + 1 < n) s |= seen[j + 1] & fr;
                if (s != seen[j]) { seen[j] = s; changed = 1; }
            }
        }
        int ok = 1;
        for (int j = 0; j < n; j++) {
            if (run[j].kind == LK_GRASS) run[j].block = (uint16_t)(~seen[j] & ((1 << ROWS) - 1));
            if (free_count(lane_free_rows(&run[j])) < GRASS_FREE_MIN) ok = 0;
        }
        if (ok) return;
        for (int j = 0; j < n; j++) if (run[j].kind == LK_GRASS) run[j].block = (uint16_t)(run[j].block & rs_rng_next(r));
    }
    for (int j = 0; j < n; j++) if (run[j].kind == LK_GRASS) run[j].block = 0;
}

enum { FAM_ROAD, FAM_RAIL, FAM_PARK, FAM_RIVER, FAM_POND, FAM_COUNT };
static int pick_family(rs_rng *r)
{
    static const int w[FAM_COUNT] = {W_ROAD, W_RAIL, W_PARK, W_RIVER, W_POND};
    int sum = 0;
    for (int i = 0; i < FAM_COUNT; i++) sum += w[i];
    int x = rs_rng_range(r, sum);
    for (int i = 0; i < FAM_COUNT; i++) { if (x < w[i]) return i; x -= w[i]; }
    return FAM_ROAD;
}

static int group_size(rs_rng *r, int fam, int d, int ease)
{
    int lo = 1, hi;
    switch (fam) {
    case FAM_ROAD: hi = lerp(ROAD_GROUP_START, ROAD_GROUP_MAX, d); break;
    case FAM_RIVER: hi = lerp(RIVER_GROUP_START, RIVER_GROUP_MAX, d); break;
    case FAM_RAIL: hi = lerp(1, RAIL_GROUP_MAX, d); break;
    case FAM_PARK: hi = lerp(1, PARK_GROUP_MAX, d); break;
    default: hi = lerp(1, POND_GROUP_MAX, d); break;
    }
    hi -= ease / 2;
    if (hi < lo) hi = lo;
    lo = (hi + 1) / 2;
    return rr(r, lo, hi);
}

static void make_lane(lane *l, gen_state *g, int fam, int32_t col, int ease, uint16_t prev_free)
{
    memset(l, 0, sizeof *l);
    switch (fam) {
    case FAM_ROAD: make_road(l, &g->rng, col, ease); break;
    case FAM_RAIL: make_rail(l, &g->rng, col, ease); break;
    case FAM_PARK: make_park(l, &g->rng, col, ease); break;
    case FAM_RIVER: g->river_down = !g->river_down; make_river(l, &g->rng, col, ease, g->river_down); break;
    default: make_pond(l, &g->rng, prev_free, ease); break;
    }
}

/* builds the unit's hazard group into g->pend[0..ngroup) (the grass run follows, kept across re-rolls) */
static void build_group(lanes *L, int ease)
{
    gen_state *g = &L->g;
    int32_t col = g->next_col;
    int d = lanes_diff256(col);
    int fam = pick_family(&g->rng);
    int n = group_size(&g->rng, fam, d, ease);
    const lane *start = lanes_get(L, g->start_col);
    uint16_t prev_free = start ? lane_free_rows(start) : (1 << ROWS) - 1;
    int mixed = col - MEADOW_COLS >= MIXED_FROM;
    for (int i = 0; i < n; i++) {
        int f = fam;
        if (mixed && i > 0 && rs_rng_range(&g->rng, 100) < 35) f = pick_family(&g->rng);
        make_lane(&g->pend[i], g, f, col + i, ease, prev_free);
        prev_free = lane_is_ground(&g->pend[i]) && g->pend[i].kind == LK_POND ? g->pend[i].block : (1 << ROWS) - 1;
    }
    g->pend[0].unit_first = 1;
    g->ngroup = n;
}

static void build_run(lanes *L)
{
    gen_state *g = &L->g;
    int32_t col = g->next_col + g->ngroup;
    int n = GRASS_RUN_MIN + (rs_rng_range(&g->rng, 100) < GRASS_RUN_TWO) + (rs_rng_range(&g->rng, 100) < GRASS_RUN_LONG);
    int nest = col - MEADOW_COLS >= g->next_nest_at - MEADOW_COLS && col >= g->next_nest_at;
    if (nest && n < 3) n = 3;
    int d = lanes_diff256(col);
    int trees = TREE_PCT_MAX * ROWS / 100 * (128 + d / 2) / 256;
    lane *run = &g->pend[g->ngroup];
    for (int j = 0; j < n; j++) make_grass(&run[j], &g->rng, trees);
    if (nest) {
        memset(&run[1], 0, sizeof run[1]);
        run[1].kind = LK_NEST;
        run[1].deco = (uint8_t)rs_rng_range(&g->rng, 4);
        g->last_nest = col + 1;
        g->next_nest_at = col + 1 + NEST_EVERY_MIN + rs_rng_range(&g->rng, NEST_EVERY_RAND);
    }
    const lane *start = lanes_get(L, g->start_col);
    connect_run(run, n, &g->rng, start ? lane_free_rows(start) : 0);   /* a straight row from the cells before */
    g->npend = g->ngroup + n;
}

void lanes_init(lanes *L, uint32_t seed)
{
    memset(L, 0, sizeof *L);
    for (int i = 0; i < RING; i++) L->ring[i].col = -1;
    gen_state *g = &L->g;
    rs_rng_seed(&g->rng, seed ? seed : 0x0dcc1e5u);
    for (int c = 0; c < MEADOW_COLS; c++) {
        lane *l = &L->ring[c % RING];
        memset(l, 0, sizeof *l);
        l->col = c;
        if (c == 0) { l->kind = LK_HEDGE; l->block = (1 << ROWS) - 1; continue; }
        l->kind = LK_GRASS;
        l->deco = (uint8_t)rs_rng_range(&g->rng, 8);
        /* the meadow: bushes on the top and bottom rows, now and then one inside */
        if (rs_rng_range(&g->rng, 100) < 60) l->block |= 1;
        if (rs_rng_range(&g->rng, 100) < 60) l->block |= 1 << (ROWS - 1);
        if (c != START_COL && c > 1 && rs_rng_range(&g->rng, 100) < 30) l->block |= (uint16_t)(1 << rr(&g->rng, 2, ROWS - 3));
        if (c == START_COL) l->block &= (uint16_t)~(1 << START_ROW);
    }
    connect_run(&L->ring[1], MEADOW_COLS - 1, &g->rng, (uint16_t)(1 << START_ROW));   /* the meadow: connected through the start */
    g->next_col = MEADOW_COLS;
    g->start_col = MEADOW_COLS - 1;
    g->river_down = rs_rng_range(&g->rng, 2);
    g->next_nest_at = MEADOW_COLS + FIRST_NEST;
    g->last_nest = -1000;
}

const lane *lanes_get(const lanes *L, int32_t col)
{
    if (col < 0) return NULL;
    const lane *l = &L->ring[col % RING];
    return l->col == col ? l : NULL;
}

static void commit(lanes *L)
{
    gen_state *g = &L->g;
    for (int i = 0; i < g->npend; i++) {
        lane *l = &L->ring[(g->next_col + i) % RING];
        *l = g->pend[i];
        l->col = g->next_col + i;
        if (l->kind == LK_GRASS && rs_rng_range(&g->rng, 100) < DUCKLING_CHANCE) {
            int k = 1 + (rs_rng_range(&g->rng, 100) < DUCKLING_PAIR);
            uint16_t fr = lane_free_rows(l);
            for (int e = 0; e < k; e++) {
                int row = rs_rng_range(&g->rng, ROWS);
                for (int s = 0; s < ROWS && !(fr >> row & 1); s++) row = (row + 1) % ROWS;
                if (!(fr >> row & 1)) break;
                fr &= (uint16_t)~(1 << row);
                if (L->nsp < SPAWN_MAX) {
                    L->sp[L->nsp].col = l->col;
                    L->sp[L->nsp].row = (int8_t)row;
                    L->nsp++;
                }
            }
        }
    }
    g->next_col += g->npend;
    g->start_col = g->next_col - 1;
    g->unit++;
    g->pending = 0;
}

/* the judge's start times are spread over the group's longest period (every phase of its traffic is tried) */
static int group_spread(const gen_state *g)
{
    int pmax = JUDGE_SPACING * JUDGE_STARTS;
    for (int i = 0; i < g->ngroup; i++) {
        const lane *l = &g->pend[i];
        int p = l->kind == LK_RAIL ? l->period : l->v ? LOOP_PX * 256 / l->v : 0;
        if (p > pmax) pmax = p;
    }
    return pmax / JUDGE_STARTS + 7;
}

void lanes_generate(lanes *L, int32_t upto, int budget)
{
    gen_state *g = &L->g;
    while (g->next_col <= upto) {
        if (!g->pending) {
            build_group(L, 0);
            build_run(L);
            g->pending = 1;
            g->tries = 0;
            g->start_k = 0;
        }
        while (g->start_k < JUDGE_STARTS) {
            if (budget == 0) return;
            if (budget > 0) budget--;
            lane cols[JMAX];
            const lane *start = lanes_get(L, g->start_col);
            int n = 0;
            cols[n++] = *start;
            for (int i = 0; i < g->ngroup && n < JMAX - 1; i++) cols[n++] = g->pend[i];
            cols[n++] = g->pend[g->ngroup];
            uint32_t t0 = (uint32_t)(g->unit * 131 + g->tries * 53) + (uint32_t)g->start_k * (uint32_t)group_spread(g);
            g->judged++;
            if (judge_cross(cols, n, lane_free_rows(start), t0, JUDGE_WINDOW) >= 0) {
                g->start_k++;
                continue;
            }
            /* unfair: re-roll the group, easier each time, then give up and make it grass */
            g->rerolls++;
            g->tries++;
            g->start_k = 0;
            lane run[8];
            int nrun = g->npend - g->ngroup;
            memcpy(run, &g->pend[g->ngroup], sizeof(lane) * (size_t)nrun);
            if (g->tries < JUDGE_TRIES) {
                build_group(L, g->tries);
            } else {
                g->fallbacks++;
                make_grass(&g->pend[0], &g->rng, 0);
                g->pend[0].unit_first = 1;
                g->ngroup = 1;
                g->start_k = JUDGE_STARTS;       /* grass next to grass: nothing to judge */
            }
            memcpy(&g->pend[g->ngroup], run, sizeof(lane) * (size_t)nrun);
            g->npend = g->ngroup + nrun;
        }
        commit(L);
    }
}

int judge_cross(const lane *cols, int n, uint16_t start_rows, uint32_t t0, int window)
{
    return judge_cross_from(cols, n, 0, start_rows, t0, window);
}
