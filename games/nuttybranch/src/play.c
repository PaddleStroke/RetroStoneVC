/*
 * Nutty Branch: the rules. The squirrels' balance on the cylinder, the jumps, the collisions with the course, the
 * nuts, the pack and the camera. No drawing, no sound, integer maths only: every run is deterministic.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
 */
#include "game.h"
#include <string.h>

/* sin(d) for d = 0..90 degrees, Q14 */
static const int16_t SIN_DEG[91] = {
    0, 286, 572, 857, 1143, 1428, 1713, 1997, 2280, 2563, 2845, 3126, 3406, 3686, 3964, 4240, 4516, 4790, 5063,
    5334, 5604, 5872, 6138, 6402, 6664, 6924, 7182, 7438, 7692, 7943, 8192, 8438, 8682, 8923, 9162, 9397, 9630, 9860,
    10087, 10311, 10531, 10749, 10963, 11174, 11381, 11585, 11786, 11982, 12176, 12365, 12551, 12733, 12911, 13085,
    13255, 13421, 13583, 13741, 13894, 14044, 14189, 14330, 14466, 14598, 14726, 14849, 14968, 15082, 15191, 15296,
    15396, 15491, 15582, 15668, 15749, 15826, 15897, 15964, 16026, 16083, 16135, 16182, 16225, 16262, 16294, 16322,
    16344, 16362, 16374, 16382, 16384};

int32_t sin_q14(int32_t ang)
{
    int32_t full = ANG(360), a = ang % full;
    if (a < 0) a += full;
    int neg = 0;
    if (a >= ANG(180)) { a -= ANG(180); neg = 1; }
    if (a > ANG(90)) a = ANG(180) - a;
    int d = a >> 12, f = a & 4095;
    int32_t v = d >= 90 ? SIN_DEG[90] : SIN_DEG[d] + (((SIN_DEG[d + 1] - SIN_DEG[d]) * f) >> 12);
    return neg ? -v : v;
}

int32_t cos_q14(int32_t ang) { return sin_q14(ang + ANG(90)); }

static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

int32_t course_speed(int32_t metres)
{
    int64_t v = SPEED0 + (int64_t)(metres < 0 ? 0 : metres) * 256 / SPEED_STEP_M;
    return v > SPEED_MAX ? SPEED_MAX : (int32_t)v;
}

/* ---- the course ---------------------------------------------------------------------------------------------- */
static int32_t seg_index(int32_t s) { return (s >= 0 ? s / SEG_LEN : -((-s + SEG_LEN - 1) / SEG_LEN)); }

int32_t seg_curve(const world *w, int32_t s) { return w->curve[seg_index(s) & (SEG_RING - 1)]; }

int32_t seg_height(const world *w, int32_t s_q8)
{
    int32_t n = seg_index(s_q8 >> 8), f = s_q8 - n * SEG_LEN * 256;
    int32_t a = w->hgt[n & (SEG_RING - 1)], b = w->hgt[(n + 1) & (SEG_RING - 1)];
    return a + (int32_t)((int64_t)(b - a) * f / (SEG_LEN * 256));
}

int32_t seg_head(const world *w, int32_t s_q8)
{
    int32_t n = seg_index(s_q8 >> 8), f = s_q8 - n * SEG_LEN * 256;
    int32_t a = w->head[n & (SEG_RING - 1)], b = w->head[(n + 1) & (SEG_RING - 1)];
    return a + (int32_t)((int64_t)(b - a) * f / (SEG_LEN * 256));
}

const obj *ob_get(const world *w, int i) { return &w->ob[(w->ob_first + i) % OB_MAX]; }

int hole_open(const world *w, const obj *o)
{
    if (o->t0 < 0) return 0;
    int k = (w->t - o->t0) * 256 / HOLE_OPEN;
    return k > 256 ? 256 : k < 0 ? 0 : k;
}

int floor_at(const world *w, int32_t s, int32_t ang, int branch)
{
    for (int i = 0; i < w->ob_count; i++) {
        const obj *o = ob_get(w, i);
        if (o->s > s + 64) break;
        switch (o->kind) {
        case OB_GAP:
            if (s >= o->s && s < o->s + o->len) return 0;
            break;
        case OB_FORK:
            if (branch && s >= o->s + o->len && s < o->s + o->len + FORK_TAIL) return 0;
            break;
        case OB_KNOTHOLE:
            if (s >= o->s && s < o->s + KNOTHOLE_LEN && iabs(ang - o->ang) < KNOTHOLE_HALF) return 0;
            break;
        case OB_PECKER: {
            int open = hole_open(w, o);
            if (open >= 128 && s >= o->s && s < o->s + HOLE_LEN && iabs(ang - o->ang) < HOLE_HALF * open / 256)
                return 0;
            break;
        }
        default:
            break;
        }
    }
    return 1;
}

int owl_ang(const world *w, const obj *o, int32_t *ang)
{
    if (o->kind != OB_OWL || o->t0 < 0) return 0;
    int k = w->t - o->t0 - 60;                       /* a hoot, then the swoop */
    if (k < 0 || k > OWL_SWEEP) return 0;
    *ang = o->side * (ANG(-80) + (int32_t)((int64_t)ANG(160) * k / OWL_SWEEP));
    return 1;
}

int cater_ang(const world *w, const obj *o, int32_t *ang)
{
    if (o->kind != OB_CATER) return 0;
    int k = o->t0 < 0 ? 0 : w->t - o->t0;
    int32_t a = o->ang + o->side * CATERPILLAR_V * k;
    if (iabs(a) > ANG(70)) return 0;                 /* off the top: gone round the side */
    *ang = a;
    return 1;
}

int cone_h(const world *w, const obj *o, int32_t *h)
{
    if (o->kind != OB_CONE || o->t0 < 0) return 0;
    int k = w->t - o->t0;
    if (k >= CONE_FALL) { *h = 0; return 1; }
    int r = CONE_FALL - k;                           /* falls from 160 units, accelerating */
    *h = (int32_t)((int64_t)Q8(160) * r * r / (CONE_FALL * CONE_FALL));
    return 1;
}

int snake_ang(const world *w, const obj *o, int32_t *ang)
{
    if (o->kind != OB_SNAKE) return 0;
    int k = o->t0 < 0 ? 0 : w->t - o->t0;
    int32_t a = o->ang + o->side * SNAKE_V * k;
    if (iabs(a) > ANG(75)) return 0;                 /* gone down the side */
    *ang = a;
    return 1;
}

int bees_ang(const world *w, const obj *o, int32_t *ang)
{
    if (o->kind != OB_BEES) return 0;
    int k = o->t0 < 0 ? 0 : w->t - o->t0;
    int32_t a = o->ang + o->side * BEE_V * k;
    if (iabs(a) > ANG(80)) return 0;
    *ang = a;
    return 1;
}

/* the swinging branch: a pendulum, SWING_AMP degrees each way, SWING_PERIOD frames, from its wake-up */
int32_t swing_deg(const world *w, const obj *o)
{
    int k = (o->t0 < 0 ? 0 : w->t - o->t0) + o->len;    /* len: its phase */
    int32_t ph = (int32_t)((int64_t)ANG(360) * (k % SWING_PERIOD) / SWING_PERIOD);
    return (int32_t)((int64_t)ANG(SWING_AMP) * sin_q14(ph) / 16384);
}

/* where its leaves are round the branch: lateral = SWING_LEN sin(deg), on a radius of BRANCH_R */
int32_t swing_ang(const world *w, const obj *o)
{
    int32_t r = (int32_t)((int64_t)SWING_LEN * sin_q14(swing_deg(w, o)) / BRANCH_R), neg = r < 0;   /* Q14 */
    if (neg) r = -r;
    if (r >= 16384) return neg ? -ANG(90) : ANG(90);
    int32_t lo = 0, hi = ANG(90);
    while (hi - lo > 32) {
        int32_t mid = (lo + hi) / 2;
        if (sin_q14(mid) < r) lo = mid; else hi = mid;
    }
    return neg ? -lo : lo;
}

int crow_at(const world *w, const obj *o, int32_t *s, int32_t *h)
{
    if (o->kind != OB_CROW) return 0;
    int32_t far = o->s + o->len;
    int k = o->t0 < 0 ? 0 : w->t - o->t0;
    int32_t walk = o->t1 >= 0 ? o->t1 - o->t0 : k;
    *s = far - walk * 3 / 2;                         /* it walks down its line of nuts at 1.5 units a frame */
    *h = 0;
    if (o->t1 >= 0) {                                /* flown off: up and away */
        int f = w->t - o->t1;
        if (f > 60) return 0;
        *h = Q8(2) * f + Q8(1) * f * f / 8;
    }
    return 1;
}

int grip_at(const world *w, int32_t s, int32_t ang)
{
    int g = 0;
    for (int i = 0; i < w->ob_count; i++) {
        const obj *o = ob_get(w, i);
        if (o->s > s + 8) break;
        if (o->kind == OB_SAP && s >= o->s && s < o->s + o->len && iabs(ang - o->ang) < SAP_HALF) return 1;
        if (o->kind == OB_RAIN && s >= o->s && s < o->s + o->len) g = 2;
    }
    return g;
}

/* a crossing branch at one angle (Q12 degrees) round the branch: 1 if it is there; its lowest and highest point */
static int limb_point(const obj *o, int32_t a, int32_t *lo, int32_t *hi)
{
    int arch = o->len != 0, span = arch ? LIMB_ARCH_SPAN : LIMB_LOW_SPAN;
    int64_t t = ((int64_t)ANG(LIMB_ROOT) - (int64_t)o->side * a) * 65536 / ((int64_t)span * 4096);   /* 0..65536 */
    if (t < 0 || t > 65536) return 0;
    int32_t hc, r;
    if (arch) {
        hc = Q8(4) + (int32_t)((int64_t)Q8(30) * sin_q14((int32_t)((int64_t)ANG(180) * t / 65536)) / 16384);
        r = Q8(8) - (int32_t)((int64_t)Q8(3.5) * t / 65536);
    } else {                                         /* lying on the bark near its root, rising a little */
        int64_t t2 = t * 2 > 65536 ? 65536 : t * 2;
        r = Q8(7.5) - (int32_t)((int64_t)Q8(3.7) * t / 65536);
        hc = r + Q8(0.5) + (int32_t)((int64_t)Q8(2) * sin_q14((int32_t)((int64_t)ANG(90) * t2 / 65536)) / 16384);
    }
    *lo = arch ? hc - r : 0;                         /* a low one blocks from the bark up */
    *hi = hc + r;
    return 1;
}

int limb_at(const obj *o, int32_t ang, int32_t *lo, int32_t *hi)
{
    int found = 0;
    *lo = 1 << 30;
    *hi = 0;
    for (int k = -1; k <= 1; k++) {                  /* the squirrel's width */
        int32_t l, h;
        if (!limb_point(o, ang + k * SQ_HALF_ANG, &l, &h)) continue;
        found = 1;
        if (l < *lo) *lo = l;
        if (h > *hi) *hi = h;
    }
    return found;
}

int32_t ob_solid(const world *w, const obj *o, int32_t *ang, int32_t *half)
{
    *ang = o->ang;
    switch (o->kind) {
    case OB_SNAKE: *half = SNAKE_HALF; return snake_ang(w, o, ang) ? SNAKE_H : 0;
    case OB_BEES: *half = BEE_HALF; return bees_ang(w, o, ang) ? BEE_H : 0;
    case OB_SNOW: *half = SNOW_HALF; return SNOW_H;
    case OB_SWING: *half = SWING_HALF; *ang = swing_ang(w, o); return SWING_H;
    case OB_KNOT: *half = KNOT_HALF; return KNOT_H;
    case OB_CATER: *half = CATER_HALF; return cater_ang(w, o, ang) ? CATER_H : 0;
    case OB_CONE: {
        int32_t h;
        *half = CONE_HALF;
        return cone_h(w, o, &h) && h == 0 ? CONE_H : 0;
    }
    case OB_OWL: *half = OWL_HALF; return owl_ang(w, o, ang) ? OWL_H : 0;
    default: return 0;
    }
}

/* ---- one squirrel's balance ----------------------------------------------------------------------------------- */
int32_t sq_angacc(const squirrel *q, int steer, int32_t curve, int wind, int sap)
{
    int32_t acc = 0;
    if (!q->air) {
        int32_t pull = (int32_t)((int64_t)PULL * sin_q14(q->ang) / 16384);
        if (sap) pull = pull * (sap == 2 ? WET_PULL_256 : SAP_PULL_256) / 256;
        acc += pull;
        acc -= (int32_t)((int64_t)curve * q->speed * q->speed / CENT_DIV);   /* a curve pushes to its outside */
    }
    int32_t st = STEER_ACC * steer;
    if (q->air) st = st * AIR_STEER_256 / 256;
    else if (sap) st = st * (sap == 2 ? WET_GRIP_256 : SAP_GRIP_256) / 256;
    if (q->sticky_t > 0) st /= 2;
    acc += st + WIND_ACC * wind;
    return acc;
}

void sq_physics(squirrel *q, int steer, int32_t curve, int wind, int sap)
{
    q->angv += sq_angacc(q, steer, curve, wind, sap);
    int damp = q->air ? DAMP_256 / 2 : sap == 2 ? WET_DAMP_256 : sap ? SAP_DAMP_256 : DAMP_256;
    q->angv -= q->angv * damp / 256;
    q->ang += q->angv;
}

void sq_jump_step(squirrel *q, int held)
{
    int hold = held && q->vh > 0 && q->hold_t < JUMP_HOLD_MAX;
    if (hold) q->hold_t++;
    q->vh -= hold ? JUMP_G_HOLD : JUMP_G;
    q->h += q->vh;
}

/* ---- the world ------------------------------------------------------------------------------------------------- */
static void sq_reset(squirrel *q, int32_t s)
{
    memset(q, 0, sizeof *q);
    q->state = SQ_READY;
    q->s = s;
    q->land_t = 99;
    q->air_t = 99;
}

void world_init_at(world *w, int players, uint32_t seed, int32_t start_m)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    rs_rng_seed(&w->rng, seed ? seed : 1);
    w->start_m = start_m;
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    gen_init(w);
    int32_t s0 = (start_m * UNITS_PER_M + 48) * 256;
    gen_fill(w, (s0 >> 8) + GEN_AHEAD);
    for (int p = 0; p < MAX_PLAYERS; p++) {
        sq_reset(&w->q[p], s0);
        /* the pack starts side by side round the top */
        static const int32_t start_ang[4][4] = {{0}, {ANG(-12), ANG(12)}, {ANG(-18), 0, ANG(18)},
                                                {ANG(-24), ANG(-8), ANG(8), ANG(24)}};
        w->q[p].ang = start_ang[w->players - 1][p < w->players ? p : 0];
        w->q[p].dist_m = start_m;
        if (p >= w->players) w->q[p].state = SQ_OFF;
    }
    w->speed = course_speed(start_m);
    w->lead_s = s0;
    w->cam_s = s0 - CAM_BACK * 256;
    w->cam_head = seg_head(w, s0);
    w->cam_h = seg_height(w, s0) + CAM_H * 256;
}

void world_init(world *w, int players, uint32_t seed) { world_init_at(w, players, seed, 0); }

int world_running(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->q[p].state <= SQ_HIT) return 1;
    return 0;
}

int world_all_down(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->q[p].state != SQ_DOWN) return 0;
    return 1;
}

/* wake the moving things as the leader comes near */
static void wake(world *w)
{
    int32_t lead = w->lead_s >> 8, v = w->speed;
    w->ev_world = 0;
    for (int i = 0; i < w->ob_count; i++) {
        obj *o = &w->ob[(w->ob_first + i) % OB_MAX];
        int32_t d = o->s - lead;
        if (d > 2600) break;
        switch (o->kind) {
        case OB_CATER:
            if (o->t0 < 0 && d <= 640) o->t0 = w->t;
            break;
        case OB_OWL:
            if (o->t0 < 0 && (int64_t)d * 256 <= (int64_t)v * (60 + OWL_SWEEP / 2)) {
                o->t0 = w->t;
                w->ev_world |= EVW_HOOT;
            }
            break;
        case OB_CONE:
            if (o->t0 < 0 && d <= CONE_LEAD) {
                o->t0 = w->t;
                w->ev_world |= EVW_CONE;
            }
            break;
        case OB_PECKER:
            if (o->t0 < 0 && d <= 1600) o->t0 = w->t;
            if (o->t0 >= 0 && d > -32 && (w->t - o->t0) % 24 == 0 && w->t - o->t0 < HOLE_OPEN + 72)
                w->ev_world |= EVW_PECK;
            break;
        case OB_SNAKE:
            if (o->t0 < 0 && d <= 700) { o->t0 = w->t; w->ev_world |= EVW_HISS; }
            break;
        case OB_BEES:
            if (o->t0 < 0 && d <= 900) o->t0 = w->t;
            if (o->t0 >= 0 && d > -64 && (w->t - o->t0) % 40 == 0) w->ev_world |= EVW_BUZZ;
            break;
        case OB_SWING:
            if (o->t0 < 0 && d <= 2200) o->t0 = w->t;
            if (o->t0 >= 0 && d > 0 && d < 700 && (w->t - o->t0 + o->len) % (SWING_PERIOD / 2) == SWING_PERIOD / 4)
                w->ev_world |= EVW_SWISH;
            break;
        case OB_CROW:
            if (o->t0 < 0 && d + o->len <= 1100) { o->t0 = w->t; w->ev_world |= EVW_CAW; }
            break;
        case OB_RAIN:
            if (o->t0 < 0 && (int64_t)d * 256 <= (int64_t)v * RAIN_WARN) { o->t0 = w->t; w->ev_world |= EVW_RAIN; }
            break;
        case OB_WIND:
            if (o->t0 < 0 && (int64_t)d * 256 <= (int64_t)v * WIND_WARN) {
                o->t0 = w->t;                        /* the leaves start streaming: the gust comes in 1 s */
                w->ev_world |= EVW_WIND;
            }
            break;
        default:
            break;
        }
    }
}

/* the crows eat their way down their nut lines until a squirrel comes near; the showers drop acorns */
static void beasts(world *w)
{
    int32_t lead = w->lead_s >> 8;
    for (int i = 0; i < w->ob_count; i++) {
        obj *o = &w->ob[(w->ob_first + i) % OB_MAX];
        if (o->s > lead + 2600) break;
        if (o->kind == OB_CROW && o->t0 >= 0 && o->t1 < 0) {
            int32_t cs, ch;
            crow_at(w, o, &cs, &ch);
            for (int p = 0; p < w->players; p++)
                if (w->q[p].state <= SQ_HIT && iabs((w->q[p].s >> 8) - cs) < CROW_FLEE) {
                    o->t1 = w->t;
                    w->ev_world |= EVW_CAW;
                }
            for (int j = i + 1; j < w->ob_count && o->t1 < 0; j++) {     /* the nuts of its line it has reached */
                obj *n = &w->ob[(w->ob_first + j) % OB_MAX];
                if (n->s > o->s + o->len) break;
                if (n->kind >= OB_ACORN && n->kind <= OB_WALNUT && !n->taken && n->s >= cs &&
                    iabs(n->ang - o->ang) < ANG(12))
                    n->taken = 2;
            }
        }
        if (o->kind == OB_SHOWER && lead >= o->s - 300 && lead < o->s + o->len && w->t % DROP_EVERY == 0) {
            drop *d = &w->drops[w->drop_next++ % DROPS];
            d->s = lead + 60 + rs_rng_range(&w->rng, 200);
            d->ang = ANG(rs_rng_range(&w->rng, 81) - 40);
            d->t = 0;
            d->live = 1;
        }
    }
    for (int k = 0; k < DROPS; k++) {
        drop *d = &w->drops[k];
        if (!d->live) continue;
        if (d->s < lead - 200) { d->live = 0; continue; }
        if (d->t < DROP_FALL && ++d->t == DROP_FALL)       /* it lands: a bonk on whoever is under it */
            for (int p = 0; p < w->players; p++) {
                squirrel *q = &w->q[p];
                if (q->state == SQ_RUN && iabs((q->s >> 8) - d->s) <= 10 && iabs(q->ang - d->ang) <= DROP_REACH &&
                    q->h < Q8(12) && !w->ghost) {
                    q->angv += q->ang >= d->ang ? BONK_KICK : -BONK_KICK;
                    q->slow_t = WEB_RECOVER / 2;
                    w->events[p] |= EV_BONK;
                }
            }
    }
}

/* the stretches (sap, wind) under a squirrel */
static void stretches(const world *w, squirrel *q)
{
    int32_t s = q->s >> 8;
    q->on_sap = 0;
    q->wind = 0;
    for (int i = 0; i < w->ob_count; i++) {
        const obj *o = ob_get(w, i);
        if (o->s > s + 8) break;
        if (o->kind == OB_SAP && !q->air && s >= o->s && s < o->s + o->len && iabs(q->ang - o->ang) < SAP_HALF)
            q->on_sap = 1;
        if (o->kind == OB_RAIN && !q->air && !q->on_sap && s >= o->s && s < o->s + o->len) q->on_sap = 2;
        if (o->kind == OB_WIND && s >= o->s && s < o->s + o->len) q->wind = o->side;
    }
}

static void fall(squirrel *q, int kind)
{
    q->state = kind == FALL_CRASH ? SQ_HIT : SQ_FALL;
    q->t = 0;
    q->fall_kind = kind;
    q->fall_dir = q->ang > 0 ? 1 : q->ang < 0 ? -1 : (q->angv >= 0 ? 1 : -1);
}

/* contacts with the things on the bark: nuts, solids, webs */
static void contacts(world *w, squirrel *q, int p)
{
    int32_t s = q->s >> 8;
    for (int k = 0; k < DROPS; k++) {                /* a shower's acorns lying on the bark */
        drop *d = &w->drops[k];
        if (d->live && d->t >= DROP_FALL && iabs(s - d->s) <= SQ_HALF_LEN + 4 && iabs(q->ang - d->ang) <= NUT_REACH &&
            q->h <= NUT_REACH_H) {
            d->live = 0;
            q->score += PTS_ACORN;
            q->nuts++;
            q->combo = q->combo_t > 0 && q->combo < COMBO_MAX ? q->combo + 1 : q->combo_t > 0 ? COMBO_MAX : 0;
            q->combo_t = COMBO_FRAMES;
            w->events[p] |= EV_NUT;
        }
    }
    for (int i = 0; i < w->ob_count; i++) {
        obj *o = &w->ob[(w->ob_first + i) % OB_MAX];
        if (o->s > s + 24) break;
        int32_t ds = s - o->s;
        if (o->kind <= OB_WALNUT && o->kind != OB_NONE) {
            if (!o->taken && iabs(ds) <= SQ_HALF_LEN + 4 && iabs(q->ang - o->ang) <= NUT_REACH && q->h <= NUT_REACH_H) {
                o->taken = 1;
                int pts = o->kind == OB_ACORN ? PTS_ACORN : o->kind == OB_HAZEL ? PTS_HAZEL : PTS_WALNUT;
                q->score += pts;
                q->nuts++;
                q->combo = q->combo_t > 0 && q->combo < COMBO_MAX ? q->combo + 1 : q->combo_t > 0 ? COMBO_MAX : 0;
                q->combo_t = COMBO_FRAMES;
                w->events[p] |= EV_NUT | (o->kind == OB_WALNUT ? EV_WALNUT : 0);
            }
            continue;
        }
        if (o->kind == OB_WEB) {
            if (iabs(ds) <= SQ_HALF_LEN + OB_LEN / 2 && iabs(q->ang - o->ang) < WEB_HALF + SQ_HALF_ANG && q->h < WEB_H) {
                if (q->slow_t < WEB_RECOVER - 8) w->events[p] |= EV_WEB;
                q->slow_t = WEB_RECOVER;
                q->sticky_t = WEB_STICKY;
            }
            continue;
        }
        if (o->kind == OB_TWIG) {                    /* a branch across the path: over it, under it, or into it */
            int32_t lo, hi;
            if (iabs(ds) <= SQ_HALF_LEN + 4 && limb_at(o, q->ang, &lo, &hi) && q->h < hi && q->h + SQ_BODY_H > lo) {
                fall(q, FALL_CRASH);
                q->hit_kind = o->kind;
                w->events[p] |= EV_HIT;
                return;
            }
            continue;
        }
        int32_t ang, half, hh = ob_solid(w, o, &ang, &half);
        if (hh && iabs(ds) <= SQ_HALF_LEN + OB_LEN / 2 && iabs(q->ang - ang) < half + SQ_HALF_ANG && q->h < hh) {
            fall(q, FALL_CRASH);
            q->hit_kind = o->kind;
            w->events[p] |= EV_HIT;
            return;
        }
    }
}

static void run_step(world *w, squirrel *q, int p, const sq_input *in)
{
    q->steer = in->steer;
    stretches(w, q);
    int32_t s = q->s >> 8;
    int support = floor_at(w, s, q->ang, q->branch);
    /* the jump: buffered presses, coyote time at a gap's edge */
    if (in->jump_pressed) q->buffer = JUMP_BUFFER;
    if (q->buffer > 0 && !q->air && (support || q->coyote <= COYOTE)) {
        q->air = 1;
        q->vh = JUMP_VH;
        q->hold_t = 0;
        q->air_t = 0;
        q->buffer = 0;
        q->coyote = 0;
        w->events[p] |= EV_JUMP;
    }
    if (q->buffer > 0) q->buffer--;
    /* the balance */
    sq_physics(q, in->steer, seg_curve(w, s), q->wind, q->on_sap);
    if (q->air) {
        q->air_t++;
        sq_jump_step(q, in->jump_held);
        if (q->h <= 0) {
            q->h = 0;
            q->vh = 0;
            q->air = 0;
            q->land_t = 0;
            w->events[p] |= EV_LAND;
            support = floor_at(w, s, q->ang, q->branch);
            q->coyote = support ? 0 : COYOTE + 1;
        }
    } else {
        q->land_t++;
        q->coyote = support ? 0 : q->coyote + 1;
    }
    if (w->ghost) {                                  /* tests: the planner's sampling squirrel never falls */
        q->ang = 0;
        q->angv = 0;
        q->coyote = 0;
    }
    if (!q->air && q->coyote > COYOTE) {
        fall(q, q->branch ? FALL_STUMP : FALL_GAP);
        w->events[p] |= EV_FALL;
        return;
    }
    if (!q->air && iabs(q->ang) > SLIP_ANG) {
        fall(q, FALL_SLIP);
        w->events[p] |= EV_FALL;
        return;
    }
    /* beyond the warning angle: the creak (at once, then every 30 frames) */
    int warn = !q->air && iabs(q->ang) > WARN_ANG;
    if (warn && (!q->warn || q->t % 30 == 0)) w->events[p] |= EV_CREAK;
    q->warn = warn;
    /* the speed: the course's, a web's slow-down, the catch-up behind the leader */
    int32_t v = w->speed;
    if (q->slow_t > 0) {
        v = (int32_t)((int64_t)v * (WEB_SPEED_256 + (256 - WEB_SPEED_256) * (WEB_RECOVER - q->slow_t) / WEB_RECOVER) / 256);
        q->slow_t--;
    } else if (q->s < w->lead_s) {
        v += v * CATCH_UP_256 / 256;
        if (q->s + v > w->lead_s) v = w->lead_s - q->s;
    }
    if (q->sticky_t > 0) q->sticky_t--;
    q->speed = v;
    q->s += v;
    if (q->s < w->lead_s - PACK_LAG) q->s = w->lead_s - PACK_LAG;
    q->dist_m = (q->s >> 8) / UNITS_PER_M;
    if (q->combo_t > 0 && --q->combo_t == 0) q->combo = 0;
    if (!w->ghost) contacts(w, q, p);
}

/* a squirrel taking a fork: crossing the split decides the branch (once) */
static void take_forks(world *w, squirrel *q, int32_t s_prev)
{
    for (int i = 0; i < w->ob_count; i++) {
        const obj *o = ob_get(w, i);
        if (o->kind != OB_FORK) continue;
        if ((s_prev >> 8) < o->s && (q->s >> 8) >= o->s) {
            int side = q->ang > ANG(1) ? 1 : q->ang < ANG(-1) ? -1 : q->angv >= 0 ? 1 : -1;
            q->branch = side == o->side;
        }
    }
}

void world_start(world *w)
{
    w->started = 1;
    for (int p = 0; p < w->players; p++) {
        w->q[p].state = SQ_RUN;
        w->q[p].t = 0;
        w->events[p] |= EV_START;
    }
}

void world_step(world *w, const sq_input in[MAX_PLAYERS])
{
    w->t++;
    for (int p = 0; p < MAX_PLAYERS; p++) w->events[p] = 0;
    if (!w->started) {                                /* ready=1 (tests): the first press starts the run */
        int go = 0;
        for (int p = 0; p < w->players; p++) go |= in[p].jump_pressed || in[p].steer;
        if (go) world_start(w);
        return;
    }
    w->speed = course_speed((w->lead_s >> 8) / UNITS_PER_M);
    wake(w);
    beasts(w);
    for (int p = 0; p < w->players; p++) {
        squirrel *q = &w->q[p];
        q->t++;
        switch (q->state) {
        case SQ_RUN: {
            int32_t s_prev = q->s;
            run_step(w, q, p, &in[p]);
            if (q->state == SQ_RUN || q->state == SQ_HIT) {
                take_forks(w, q, s_prev);
                /* leaving the fork zone: back to the one branch */
                int infork = 0;
                for (int i = 0; i < w->ob_count; i++) {
                    const obj *o = ob_get(w, i);
                    if (o->kind == OB_FORK && (q->s >> 8) >= o->s && (q->s >> 8) < o->s + o->len + FORK_TAIL) infork = 1;
                }
                if (!infork) q->branch = 0;
            }
            break;
        }
        case SQ_HIT:
            if (q->t >= HIT_FREEZE) { q->state = SQ_FALL; q->t = 0; }
            break;
        case SQ_FALL:
            /* tumbling off: round the side and down */
            q->angv += q->fall_dir * ANG(0.6);
            q->ang += q->angv;
            q->vh -= JUMP_G;
            q->h += q->vh;
            if (q->t >= FALL_FRAMES) { q->state = SQ_DOWN; q->t = 0; }
            break;
        default:
            break;
        }
    }
    /* the pack jostles: two squirrels side by side on the bark push each other apart round the branch */
    for (int i = 0; i < w->players; i++)
        for (int j = i + 1; j < w->players; j++) {
            squirrel *a = &w->q[i], *b = &w->q[j];
            if (a->state != SQ_RUN || b->state != SQ_RUN || a->air != b->air) continue;
            if (iabs(a->s - b->s) >= BUMP_LEN * 256 || iabs(a->ang - b->ang) >= 2 * SQ_HALF_ANG) continue;
            int sign = a->ang > b->ang ? 1 : a->ang < b->ang ? -1 : (i & 1 ? 1 : -1);
            a->angv += sign * BUMP_ACC;
            b->angv -= sign * BUMP_ACC;
        }
    /* the leader and the camera */
    int lead = -1;
    for (int p = 0; p < w->players; p++)
        if (w->q[p].state <= SQ_HIT && (lead < 0 || w->q[p].s > w->q[lead].s)) lead = p;
    if (lead >= 0) {
        w->leader = lead;
        int32_t ls = w->q[lead].s;
        w->lead_s = ls > w->lead_s ? ls : w->lead_s + (ls - w->lead_s) / 16;
    }
    w->cam_s = w->lead_s - CAM_BACK * 256;
    int32_t head = seg_head(w, w->lead_s);
    const squirrel *lq = &w->q[w->leader];
    if (lq->branch || fork_offset(w, w->lead_s, 0) != fork_offset(w, w->lead_s - 256, 0)) {
        int32_t d = fork_offset(w, w->lead_s, lq->branch) - fork_offset(w, w->lead_s - SEG_LEN * 256, lq->branch);
        head += d;      /* a fork's branch heads off at an angle: the camera turns with it */
    }
    w->cam_head += (head - w->cam_head) / (1 << CAM_TURN_SH);
    w->cam_h = seg_height(w, w->lead_s) + CAM_H * 256;
    if (lead >= 0) w->drift += (int32_t)((int64_t)seg_curve(w, w->lead_s >> 8) * w->speed >> 9);
    gen_fill(w, (w->lead_s >> 8) + GEN_AHEAD);
    int32_t back = w->lead_s;
    for (int p = 0; p < w->players; p++)
        if (w->q[p].s < back) back = w->q[p].s;
    gen_trim(w, (back >> 8) - CAM_BACK - 64);
}
