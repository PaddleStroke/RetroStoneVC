/*
 * Llama Drama: the rules (DESIGN.md "Rules"). No drawing, no sound, no screen width: deterministic and testable alone
 * (tests/test_rules.c links this file with the SDK's RNG only). Hornets come from the right, gliding in from beyond
 * the widest screen (SPAWN_R) to ENTRY_R, where their pattern starts; their sine waves and zig-zags follow the x (a
 * fixed path in the sky, whatever their speed); near a llama they steer to its head. The llamas spit along their aim
 * (a slight arc), at most GOBS_MAX gobs in the air each, RELOAD frames apart; a hornet that touches a llama's head
 * or neck stings it: that llama is out (one hit). The ramp: the herd's score heats the sky up: faster hornets, more
 * of them, new patterns and kinds (director()).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
 */
#include "game.h"
#include <string.h>

static const int slot_x[MAX_PLAYERS] = SLOT_X, slot_y[MAX_PLAYERS] = SLOT_Y, slot_neck[MAX_PLAYERS] = SLOT_NECK;
static const int kind_hp[HK_KINDS] = KIND_HP, kind_points[HK_KINDS] = KIND_POINTS, kind_r[HK_KINDS] = KIND_R,
                 kind_speed[HK_KINDS] = KIND_SPEED;

/* a quarter of a sine, Q14, 256 steps to the quarter turn */
static const int16_t SINQ[257] = {
    0, 101, 201, 302, 402, 503, 603, 704, 804, 904, 1005, 1105, 1205, 1306, 1406, 1506,
    1606, 1706, 1806, 1906, 2006, 2105, 2205, 2305, 2404, 2503, 2603, 2702, 2801, 2900, 2999, 3098,
    3196, 3295, 3393, 3492, 3590, 3688, 3786, 3883, 3981, 4078, 4176, 4273, 4370, 4467, 4563, 4660,
    4756, 4852, 4948, 5044, 5139, 5235, 5330, 5425, 5520, 5614, 5708, 5803, 5897, 5990, 6084, 6177,
    6270, 6363, 6455, 6547, 6639, 6731, 6823, 6914, 7005, 7096, 7186, 7276, 7366, 7456, 7545, 7635,
    7723, 7812, 7900, 7988, 8076, 8163, 8250, 8337, 8423, 8509, 8595, 8680, 8765, 8850, 8935, 9019,
    9102, 9186, 9269, 9352, 9434, 9516, 9598, 9679, 9760, 9841, 9921, 10001, 10080, 10159, 10238, 10316,
    10394, 10471, 10549, 10625, 10702, 10778, 10853, 10928, 11003, 11077, 11151, 11224, 11297, 11370, 11442, 11514,
    11585, 11656, 11727, 11797, 11866, 11935, 12004, 12072, 12140, 12207, 12274, 12340, 12406, 12472, 12537, 12601,
    12665, 12729, 12792, 12854, 12916, 12978, 13039, 13100, 13160, 13219, 13279, 13337, 13395, 13453, 13510, 13567,
    13623, 13678, 13733, 13788, 13842, 13896, 13949, 14001, 14053, 14104, 14155, 14206, 14256, 14305, 14354, 14402,
    14449, 14497, 14543, 14589, 14635, 14680, 14724, 14768, 14811, 14854, 14896, 14937, 14978, 15019, 15059, 15098,
    15137, 15175, 15213, 15250, 15286, 15322, 15357, 15392, 15426, 15460, 15493, 15525, 15557, 15588, 15619, 15649,
    15679, 15707, 15736, 15763, 15791, 15817, 15843, 15868, 15893, 15917, 15941, 15964, 15986, 16008, 16029, 16049,
    16069, 16088, 16107, 16125, 16143, 16160, 16176, 16192, 16207, 16221, 16235, 16248, 16261, 16273, 16284, 16295,
    16305, 16315, 16324, 16332, 16340, 16347, 16353, 16359, 16364, 16369, 16373, 16376, 16379, 16381, 16383, 16384,
    16384};

int isin1024(int a)
{
    a &= ANG_TURN - 1;
    if (a < 256) return SINQ[a];
    if (a < 512) return SINQ[512 - a];
    if (a < 768) return -SINQ[a - 512];
    return -SINQ[1024 - a];
}

static int32_t q16(int v) { return (int32_t)v * Q16_ONE; }
static int px(int32_t q) { return (int)(q >> 16); }
static int iabs(int v) { return v < 0 ? -v : v; }
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int32_t mulq14(int32_t v, int s) { return (int32_t)(((int64_t)v * s) >> 14); }

int hornet_radius(int kind) { return kind_r[kind]; }

int aim_frame(int aim)
{
    int span = AIM_MAX - AIM_MIN;
    return clampi(((aim - AIM_MIN) * (AIM_FRAMES - 1) * 2 + span) / (2 * span), 0, AIM_FRAMES - 1);
}

/* ---- the llama's geometry ---------------------------------------------------------------------------------------- */
void llama_neck(int p, int aim, int32_t *sx, int32_t *sy, int32_t *bx, int32_t *by)
{
    int n = NECK_ANG0 + aim * NECK_K / 256;
    *sx = q16(slot_x[p]);
    *sy = q16(slot_y[p]);
    *bx = *sx + slot_neck[p] * icos1024(n) * 4;
    *by = *sy - slot_neck[p] * isin1024(n) * 4;
}

void llama_head(int p, int aim, int32_t *x, int32_t *y)
{
    int32_t sx, sy, bx, by;
    llama_neck(p, aim, &sx, &sy, &bx, &by);
    int c = icos1024(aim), s = isin1024(aim);
    /* the head's centre: the neck's top minus the joint, turned by the aim */
    *x = bx - (HEAD_JOINT_U * c - HEAD_JOINT_V * s) * 4;
    *y = by + (HEAD_JOINT_U * s + HEAD_JOINT_V * c) * 4;
}

void llama_mouth(int p, int aim, int32_t *x, int32_t *y)
{
    int32_t hx, hy;
    llama_head(p, aim, &hx, &hy);
    int c = icos1024(aim), s = isin1024(aim);
    *x = hx + (HEAD_MOUTH_U * c - HEAD_MOUTH_V * s) * 4;
    *y = hy - (HEAD_MOUTH_U * s + HEAD_MOUTH_V * c) * 4;
}

static void gob_velocity(int aim, int32_t *vx, int32_t *vy)
{
    *vx = mulq14(GOB_SPEED, icos1024(aim));
    *vy = -mulq14(GOB_SPEED, isin1024(aim));
}

/* where a gob launched from (mx, my) at velocity (vx, vy) is after f frames (Q16; the same steps as gob_step) */
static void gob_at(int32_t mx, int32_t my, int32_t vx, int32_t vy, int f, int32_t *x, int32_t *y)
{
    *x = mx + vx * f;
    *y = my + vy * f + GOB_GRAV * (f * (f - 1) / 2);
}

int llama_aim_at(int p, int x, int y)
{
    int best = AIM_START, best_err = 1 << 30;
    for (int a = AIM_MIN; a <= AIM_MAX; a += 2) {
        int32_t mx, my, vx, vy;
        llama_mouth(p, a, &mx, &my);
        gob_velocity(a, &vx, &vy);
        if (vx <= 0 || q16(x) <= mx) continue;
        int f = (int)((q16(x) - mx) / vx);
        int32_t gx, gy;
        gob_at(mx, my, vx, vy, f, &gx, &gy);
        int err = iabs(px(gy) - y);
        if (err < best_err) {
            best_err = err;
            best = a;
        }
    }
    return best;
}

/* ---- set-up --------------------------------------------------------------------------------------------------------- */
void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->players = clampi(players, 1, MAX_PLAYERS);
    w->seed = seed;
    rs_rng_seed(&w->rng, seed ? seed : 1);
    for (int p = 0; p < MAX_PLAYERS; p++) {
        llama *l = &w->l[p];
        l->state = p < w->players ? LS_READY : LS_OFF;
        l->aim = AIM_START;
        l->bot_target = -1;
    }
    w->wave_t = WAVE_FIRST;
}

int world_running(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->l[p].state == LS_PLAY || w->l[p].state == LS_READY) return 1;
    return 0;
}

int world_all_out(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->l[p].state != LS_OUT) return 0;
    return 1;
}

int world_team_score(const world *w)
{
    int s = 0;
    for (int p = 0; p < w->players; p++) s += w->l[p].score;
    return s;
}

static int rnd(world *w, int n) { return rs_rng_range(&w->rng, n); }

static effect *fx_add(world *w, int kind, int32_t x, int32_t y)
{
    effect *e = &w->fx[w->fx_next];
    w->fx_next = (w->fx_next + 1) % FX_MAX;
    memset(e, 0, sizeof *e);
    e->on = 1;
    e->kind = (uint8_t)kind;
    e->x = x;
    e->y = y;
    return e;
}

/* ---- the hornets ---------------------------------------------------------------------------------------------------- */
static int playing(const world *w, int p) { return p >= 0 && p < w->players && w->l[p].state == LS_PLAY; }

/* the llama a hornet in lane y goes for: the playing one whose head is nearest (-1: none) */
static int nearest_llama(const world *w, int32_t y)
{
    int best = -1, bd = 1 << 30;
    for (int p = 0; p < w->players; p++) {
        if (!playing(w, p) && !(w->l[p].state == LS_READY && !w->started)) continue;
        int32_t hx, hy;
        llama_head(p, w->l[p].aim, &hx, &hy);
        int d = iabs(px(hy - y));
        if (d < bd) { bd = d; best = p; }
    }
    return best;
}

static int32_t hornet_speed(const world *w, int kind)
{
    int pct = 100 + imin(w->heat * HORNET_V_K, HORNET_V_MAX);
    return (int32_t)((int64_t)HORNET_V0 * pct / 100 * kind_speed[kind] / 100);
}

/* a new hornet beyond the widest screen (extra: px further right, for groups) */
hornet *hornet_spawn(world *w, int kind, int pat, int lane, int extra)
{
    for (int i = 0; i < MAX_HORNETS; i++) {
        hornet *h = &w->hn[i];
        if (h->on) continue;
        memset(h, 0, sizeof *h);
        h->on = 1;
        h->kind = (uint8_t)kind;
        h->pat = (uint8_t)pat;
        h->hp = (uint8_t)kind_hp[kind];
        h->v = hornet_speed(w, kind);
        h->x = q16(SPAWN_R + extra);
        h->y0 = h->y = q16(lane);
        h->t = -1;
        int t = nearest_llama(w, h->y);
        h->target = (uint8_t)(t < 0 ? 0 : t);
        int sa[3] = SINE_AMP;
        h->amp = (int16_t)(pat == PAT_ZIGZAG ? ZIG_AMP : sa[w->heat < 12 ? 0 : w->heat < 30 ? 1 : 2]);
        if (pat == PAT_DIVE) h->aux = (int16_t)(150 + rnd(w, 90));     /* where it dives */
        if (pat == PAT_LOOP) h->aux = (int16_t)(180 + rnd(w, 70));     /* where it loops */
        h->ph = (int16_t)(rnd(w, 2) * 512);                            /* up or down first */
        return h;
    }
    return NULL;
}

static int isqrt(int64_t v)
{
    int r = 0;
    for (int b = 1 << 15; b; b >>= 1)
        if ((int64_t)(r + b) * (r + b) <= v) r += b;
    return r;
}

/* the velocity (Q16) of speed v from (x, y) toward (tx, ty) */
static void aim_vel(int32_t x, int32_t y, int32_t tx, int32_t ty, int32_t v, int32_t *vx, int32_t *vy)
{
    int dx = px(tx - x), dy = px(ty - y);
    int d = imax(1, isqrt((int64_t)dx * dx + (int64_t)dy * dy));
    *vx = (int32_t)((int64_t)v * dx / d);
    *vy = (int32_t)((int64_t)v * dy / d);
}

static int tri1024(int a)       /* a triangle wave, -16384..16384 like isin1024 */
{
    a &= ANG_TURN - 1;
    if (a < 256) return a * 64;
    if (a < 768) return (512 - a) * 64;
    return (a - 1024) * 64;
}

/* one hornet's move (no RNG: the bot forecasts with it) */
static void hornet_move(const world *w, hornet *h)
{
    int32_t ox = h->x, oy = h->y;
    if (h->pat == 255) {                                /* leaving after a sting: up and away */
        h->x += q16(2);
        h->y -= q16(2);
        h->vx = h->x - ox, h->vy = h->y - oy;
        return;
    }
    if (h->t < 0) {                                     /* gliding in along its lane */
        h->x -= h->v;
        if (h->x <= q16(ENTRY_R)) h->t = 0;
        h->vx = h->x - ox, h->vy = h->y - oy;
        return;
    }
    h->t++;
    int tg = h->target;
    int32_t hx = 0, hy = h->y;
    if (tg < w->players) llama_head(tg, w->l[tg].aim, &hx, &hy);
    int heat = w->heat;
    int free_y = 0;                                     /* the pattern sets y itself (no homing) */
    switch (h->pat) {
    case PAT_DIVE:
        if (h->aux_t == 0 && h->x <= q16(h->aux)) h->aux_t = 1;          /* it dives at the head, from up high */
        if (h->aux_t && tg < w->players) {
            aim_vel(h->x, h->y, hx, hy, (int32_t)((int64_t)h->v * DIVE_PCT / 100), &h->dx, &h->dy);
            h->x += h->dx;
            h->y += h->dy;
            free_y = 1;
        }
        break;
    case PAT_LOOP:
        if (h->aux_t == 0 && h->x <= q16(h->aux)) {     /* the loop: up, back, down and on */
            h->aux_t = 1;
            h->dx = h->x;                               /* the loop's centre */
            h->dy = h->y - q16(LOOP_R);
            h->ph = 0;
        }
        if (h->aux_t == 1) {
            /* the angle goes round at the hornet's speed: v / R radians a frame (in 1/1024 turns, x16) */
            int step = (int)((int64_t)h->v * 16384 / (411775LL * LOOP_R));
            h->ph = (int16_t)(h->ph + imax(step, 8));
            int a = h->ph / 16;
            if (a >= ANG_TURN) {
                h->aux_t = 2;
                h->x = h->dx;
                h->y = h->dy + q16(LOOP_R);
                h->y0 = h->y;
            } else {
                h->x = h->dx - LOOP_R * isin1024(a) * 4;
                h->y = h->dy + LOOP_R * icos1024(a) * 4;
            }
            h->vx = h->x - ox, h->vy = h->y - oy;
            return;
        }
        break;
    case PAT_DART:
        if (h->aux_t == 0 && h->x <= q16(DART_PERCH)) h->aux_t = 1, h->aux = h->t;   /* perched, buzzing */
        if (h->aux_t == 1) {
            if (h->t - h->aux >= DART_WIND) {           /* dash at the head */
                aim_vel(h->x, h->y, hx, hy, (int32_t)((int64_t)DART_SPEED * (100 + imin(heat, 100)) / 100), &h->dx,
                        &h->dy);
                h->aux_t = 2;
            }
            h->vx = 0, h->vy = 0;
            return;
        }
        if (h->aux_t == 2) {
            h->x += h->dx;
            h->y += h->dy;
            h->vx = h->x - ox, h->vy = h->y - oy;
            return;
        }
        break;
    }
    if (!free_y) {
        h->x -= h->v;
        int waits = (h->pat == PAT_DIVE || h->pat == PAT_LOOP) && h->aux_t == 0;   /* its dive or loop first */
        if (h->x < hx + q16(HOME_DX) && tg < w->players && !waits) h->homing = 1;
        if (h->homing) {
            int32_t hv = (int32_t)((int64_t)HOME_VY * (100 + imin(heat, 100)) / 100);
            int32_t d = hy - h->y;
            h->y += d > hv ? hv : d < -hv ? -hv : d;
        } else {
            int s = (ENTRY_R - px(h->x)) * SINE_K + h->ph;
            if (h->pat == PAT_SINE || h->pat == PAT_DIVE || h->pat == PAT_LOOP)
                h->y = h->y0 + (h->pat == PAT_SINE ? h->amp * isin1024(s) * 4 : 0);
            else if (h->pat == PAT_ZIGZAG)
                h->y = h->y0 + h->amp * tri1024(s) * 4;
        }
    }
    h->vx = h->x - ox, h->vy = h->y - oy;
}

/* ---- the waves (the ramp) ---------------------------------------------------------------------------------------- */
static int lane(world *w)
{
    if (w->heat < 6) return LANE_EASY + rnd(w, LANE_EASY_MAX - LANE_EASY + 1);
    return LANE_MIN + rnd(w, LANE_MAX - LANE_MIN + 1);
}

static void seen(world *w, int msg)
{
    if (w->seen & (1 << msg)) return;
    w->seen |= 1 << msg;
    w->msg = msg;
    w->msg_t = MSG_FRAMES;
    w->wevents |= WEV_MSG;
}

/* sends one wave; returns its cost (the gap after it) */
int world_wave(world *w, int kind)
{
    int heat = w->heat, y = lane(w);
    switch (kind) {
    case W_PAIR: {                                      /* two at once: in step, or crossing waves */
        int yy = clampi(y, LANE_MIN + 24, LANE_MAX - 24);
        if (rnd(w, 2)) {
            hornet_spawn(w, HK_SMALL, PAT_STRAIGHT, yy - 24, 0);
            hornet_spawn(w, HK_SMALL, PAT_STRAIGHT, yy + 24, 24);
        } else {
            hornet *a = hornet_spawn(w, HK_SMALL, PAT_SINE, yy, 0), *b = hornet_spawn(w, HK_SMALL, PAT_SINE, yy, 0);
            if (a && b) b->ph = (int16_t)(a->ph + 512);   /* the second one mirrors the first: they cross */
        }
        return 3;
    }
    case W_DIVE:
        hornet_spawn(w, HK_SMALL, PAT_DIVE, LANE_MIN - 6 + rnd(w, 18), 0);
        return 2;
    case W_LOOP:
        hornet_spawn(w, HK_SMALL, PAT_LOOP, clampi(y, LANE_MIN + 2 * LOOP_R, LANE_MAX), 0);
        return 2;
    case W_DART:
        hornet_spawn(w, HK_DART, PAT_DART, y, 0);
        seen(w, MSG_DART);
        return 3;
    case W_SWARM: {                                     /* a little swarm in a diamond, along a sine wave */
        static const int sn[3] = SWARM_N;
        int n = sn[heat < 50 ? 0 : heat < 70 ? 1 : 2];
        static const int ox[6] = {0, 14, 14, 28, 28, 42}, oy[6] = {0, -12, 12, 0, -20, 10};
        int yy = clampi(y, LANE_MIN + 20, LANE_MAX - 20);
        for (int i = 0; i < n; i++) hornet_spawn(w, HK_SMALL, PAT_SINE, yy + oy[i], ox[i]);
        seen(w, MSG_SWARM);
        return 5;
    }
    case W_BIG:
        hornet_spawn(w, HK_BIG, PAT_SINE, clampi(y, LANE_MIN + 16, LANE_MAX - 16), 0);
        if (heat >= ESCORT_FROM) {
            hornet_spawn(w, HK_SMALL, PAT_STRAIGHT, clampi(y - 30, LANE_MIN, LANE_MAX), 30);
            hornet_spawn(w, HK_SMALL, PAT_STRAIGHT, clampi(y + 30, LANE_MIN, LANE_MAX), 30);
        }
        seen(w, MSG_BIG);
        return heat >= ESCORT_FROM ? 6 : 5;
    }
    int pat = heat < 3 ? PAT_STRAIGHT : heat >= UNLOCK_ZIGZAG && rnd(w, 3) == 0 ? PAT_ZIGZAG
                                                                                : rnd(w, 2) ? PAT_SINE : PAT_STRAIGHT;
    hornet_spawn(w, HK_SMALL, pat, y, 0);
    return 2;
}

static int pick_wave(world *w)
{
    static const int unlock[W_KINDS] = {0, UNLOCK_PAIR, UNLOCK_DIVE, UNLOCK_LOOP, UNLOCK_DART, UNLOCK_SWARM, UNLOCK_BIG};
    int weight[W_KINDS] = {imax(3, 10 - w->heat / 10), 6, 5, 4, 4, 3, 3};
    int total = 0;
    for (int k = 0; k < W_KINDS; k++) {
        if (w->heat < unlock[k]) weight[k] = 0;
        total += weight[k];
    }
    int r = rnd(w, total);
    for (int k = 0; k < W_KINDS; k++) {
        if (r < weight[k]) return k;
        r -= weight[k];
    }
    return W_SINGLE;
}

static void director(world *w)
{
    if (--w->wave_t > 0) return;
    static const int pf[MAX_PLAYERS] = HEAT_PF;
    int cost = world_wave(w, pick_wave(w));
    if (w->heat >= DOUBLE_FROM && rnd(w, 100) < imin(60, (w->heat - DOUBLE_FROM) * 2)) cost += world_wave(w, pick_wave(w)) / 2;
    int gap = imax(GAP_MIN, GAP0 - w->heat * GAP_K);
    w->wave_t = gap * cost / 2 * 4 / pf[w->players - 1];
    w->waves++;
    w->wevents |= WEV_WAVE;
}

/* ---- the llamas ------------------------------------------------------------------------------------------------------ */
static int gobs_of(const world *w, int p)
{
    int n = 0;
    for (int i = 0; i < GOBS_MAX; i++) n += w->gobs[p * GOBS_MAX + i].on;
    return n;
}

static void spit(world *w, int p)
{
    llama *l = &w->l[p];
    for (int i = 0; i < GOBS_MAX; i++) {
        gob *g = &w->gobs[p * GOBS_MAX + i];
        if (g->on) continue;
        memset(g, 0, sizeof *g);
        g->on = 1;
        g->owner = (uint8_t)p;
        llama_mouth(p, l->aim, &g->x, &g->y);
        gob_velocity(l->aim, &g->vx, &g->vy);
        l->reload = RELOAD;
        l->puff = PUFF_FRAMES;
        l->buffer = 0;
        l->spits++;
        w->events[p] |= EV_SPIT;
        return;
    }
}

static void llama_step(world *w, int p, const pin *in)
{
    llama *l = &w->l[p];
    l->t++;
    if (l->state == LS_HIT) {
        if (l->t >= HIT_FRAMES) {
            l->state = LS_OUT;
            l->t = 0;
            w->events[p] |= EV_OUT;
        }
        return;
    }
    if (l->state != LS_PLAY) return;
    if (l->reload > 0) l->reload--;
    if (l->puff > 0) l->puff--;
    if (l->buffer > 0) l->buffer--;
    int32_t mx, my;
    llama_mouth(p, l->aim, &mx, &my);
    int touched = in->touch && in->tx >= px(mx) + TOUCH_MIN_DX;
    if (touched) {                                      /* touch: the head turns to the finger */
        l->aim = llama_aim_at(p, in->tx, in->ty);
        l->hold = 0;
    } else if (in->dy) {
        if (in->dy != l->dir) l->hold = 0;
        l->hold++;
        l->dir = in->dy;
        int v = l->hold <= AIM_ACCEL ? AIM_SLOW : AIM_FAST;
        l->aim = clampi(l->aim - in->dy * v, AIM_MIN, AIM_MAX);
    } else {
        l->hold = 0;
        l->dir = 0;
    }
    if (in->spit || (touched && in->touch == 1)) l->buffer = SPIT_BUFFER;
    if (l->buffer > 0 && l->reload <= 0 && gobs_of(w, p) < GOBS_MAX) spit(w, p);
}

/* the distance from (x, y) to the llama's neck, px */
static int neck_dist2(int p, int aim, int x, int y)
{
    int32_t sx, sy, bx, by;
    llama_neck(p, aim, &sx, &sy, &bx, &by);
    int ax = px(sx), ay = px(sy), ux = px(bx) - ax, uy = px(by) - ay;
    int len2 = ux * ux + uy * uy, dot = (x - ax) * ux + (y - ay) * uy;
    int t256 = len2 ? clampi(dot * 256 / len2, 0, 256) : 0;
    int cx = ax + ux * t256 / 256, cy = ay + uy * t256 / 256;
    return (x - cx) * (x - cx) + (y - cy) * (y - cy);
}

static void sting(world *w, int p, hornet *h)
{
    llama *l = &w->l[p];
    l->state = LS_HIT;
    l->t = 0;
    l->puff = 0;
    l->scared = 0;
    w->events[p] |= EV_HIT;
    w->freeze = HIT_FREEZE;
    w->sting_kind = h->kind;                            /* (the log) */
    w->sting_pat = h->pat;
    h->pat = 255;                                       /* it buzzes away, pleased with itself */
    h->homing = 0;
}

static void pop(world *w, hornet *h, int owner)
{
    llama *l = &w->l[owner];
    l->score += kind_points[h->kind];
    l->pops++;
    w->events[owner] |= EV_POP;
    effect *e = fx_add(w, FX_FALL, h->x, h->y);         /* bonk: it tumbles down with stars, lands, bounces */
    e->sub = h->kind;
    e->vx = 20000;
    e->vy = -q16(2);
    e->value = (int16_t)(204 + rnd(w, 24));             /* where it lands on the meadow */
    e = fx_add(w, FX_POINTS, h->x, h->y - q16(8));
    e->value = (int16_t)kind_points[h->kind];
    h->on = 0;
}

/* ---- a frame ---------------------------------------------------------------------------------------------------------- */
void world_step(world *w, const pin in[MAX_PLAYERS])
{
    memset(w->events, 0, sizeof w->events);
    w->wevents = 0;
    if (!w->started) {
        if (!(in[0].any || in[0].spit || in[0].touch == 1)) return;
        w->started = 1;
        for (int p = 0; p < w->players; p++) {
            w->l[p].state = LS_PLAY;
            w->l[p].t = 0;
            w->events[p] |= EV_START;
        }
    }
    if (w->freeze > 0) {                                /* the hit-stop */
        w->freeze--;
        return;
    }
    w->t++;
    static const int pf[MAX_PLAYERS] = HEAT_PF;
    w->heat = world_team_score(w) * 4 / (HEAT_PTS * pf[w->players - 1]);

    for (int p = 0; p < w->players; p++) llama_step(w, p, &in[p]);

    /* the spit */
    for (int i = 0; i < MAX_PLAYERS * GOBS_MAX; i++) {
        gob *g = &w->gobs[i];
        if (!g->on) continue;
        g->x += g->vx;
        g->y += g->vy;
        g->vy += GOB_GRAV;
        g->t++;
        if (g->x >= q16(GOB_RANGE)) {
            g->on = 0;
            fx_add(w, FX_FIZZLE, q16(GOB_RANGE), g->y);
        } else if (g->y >= q16(GROUND_HIT)) {
            g->on = 0;
            fx_add(w, FX_SPLAT, g->x, q16(GROUND_HIT));
        } else if (g->y < q16(-8)) {                    /* out of the top of the sky: the llama can spit again */
            g->on = 0;
        }
    }

    /* the hornets */
    for (int i = 0; i < MAX_HORNETS; i++) {
        hornet *h = &w->hn[i];
        if (!h->on) continue;
        if (h->pat != 255 && !playing(w, h->target)) {
            int t = nearest_llama(w, h->y);
            if (t >= 0) h->target = (uint8_t)t;
        }
        int was_out = h->t < 0;
        hornet_move(w, h);
        if (was_out && h->t >= 0) w->wevents |= WEV_ENTER;
        if (h->pat == PAT_DART && h->aux_t == 1 && h->t == h->aux + 1) w->wevents |= WEV_DART;
        if (h->flash) h->flash--;
        if (h->x < q16(FIELD_L) || h->y < q16(-48) || h->y > q16(STAGE_H + 32) || h->x > q16(SPAWN_R + 64)) h->on = 0;
    }
    /* a gob hits a hornet */
    for (int i = 0; i < MAX_PLAYERS * GOBS_MAX; i++) {
        gob *g = &w->gobs[i];
        if (!g->on) continue;
        for (int k = 0; k < MAX_HORNETS; k++) {
            hornet *h = &w->hn[k];
            if (!h->on || h->t < 0 || h->pat == 255) continue;
            int dx = px(g->x - h->x), dy = px(g->y - h->y), r = kind_r[h->kind] + GOB_R;
            if (dx * dx + dy * dy >= r * r) continue;
            g->on = 0;
            fx_add(w, FX_SPLAT, g->x, g->y);
            if (--h->hp == 0) {
                pop(w, h, g->owner);
            } else {
                h->flash = FLASH_FRAMES;
                h->x += q16(BIG_PUSH);
                w->events[g->owner] |= EV_BIG_HIT;
            }
            break;
        }
    }
    /* a hornet stings a llama (its head or its neck) */
    for (int k = 0; k < MAX_HORNETS; k++) {
        hornet *h = &w->hn[k];
        if (!h->on || h->t < 0 || h->pat == 255) continue;
        int hx = px(h->x), hy = px(h->y), r = kind_r[h->kind];
        for (int p = 0; p < w->players; p++) {
            if (!playing(w, p)) continue;
            int32_t cx, cy;
            llama_head(p, w->l[p].aim, &cx, &cy);
            int dx = hx - px(cx), dy = hy - px(cy);
            if (dx * dx + dy * dy < (HEAD_R + r) * (HEAD_R + r) ||
                neck_dist2(p, w->l[p].aim, hx, hy) < (NECK_R + r) * (NECK_R + r)) {
                sting(w, p, h);
                break;
            }
        }
    }
    /* the scared faces */
    for (int p = 0; p < w->players; p++) {
        llama *l = &w->l[p];
        l->scared = 0;
        if (l->state != LS_PLAY) continue;
        int32_t cx, cy;
        llama_head(p, l->aim, &cx, &cy);
        for (int k = 0; k < MAX_HORNETS && !l->scared; k++) {
            const hornet *h = &w->hn[k];
            if (!h->on || h->t < 0 || h->pat == 255) continue;
            int dx = px(h->x - cx), dy = px(h->y - cy);
            l->scared = dx * dx + dy * dy < SCARED_DIST * SCARED_DIST;
        }
    }

    /* the effects */
    for (int i = 0; i < FX_MAX; i++) {
        effect *e = &w->fx[i];
        if (!e->on) continue;
        e->t++;
        switch (e->kind) {
        case FX_FALL:
            if (e->vy != 0 || e->y < q16(e->value)) {
                e->x += e->vx;
                e->y += e->vy;
                e->vy += 11796;                          /* 0.18 px/frame/frame */
                if (e->y >= q16(e->value)) {             /* it lands: one little bounce, then lies there */
                    e->y = q16(e->value);
                    if (e->vy > q16(1)) {
                        e->vy = -e->vy / 4;
                        w->wevents |= WEV_LAND;
                    } else {
                        e->vy = 0;
                        e->vx = 0;
                    }
                }
            }
            if (e->t > 150) e->on = 0;
            break;
        case FX_POINTS:
            if (e->t >= 40) e->on = 0;
            break;
        case FX_SPLAT:
        case FX_FIZZLE:
        case FX_PUFF:
            if (e->t >= 12) e->on = 0;
            break;
        }
    }

    director(w);
    /* the sky: the day turns to a sunset, the dusk, the night (one step a frame toward the heat's) */
    int target = w->heat < SKY_SUNSET ? 0
                 : w->heat < SKY_DUSK ? (w->heat - SKY_SUNSET) * 256 / (SKY_DUSK - SKY_SUNSET)
                 : w->heat < SKY_NIGHT ? 256 + (w->heat - SKY_DUSK) * 256 / (SKY_NIGHT - SKY_DUSK)
                 : imin(768, 512 + (w->heat - SKY_NIGHT) * 256 / 20);
    if (w->sky < target) w->sky++;
    if (w->msg_t > 0 && --w->msg_t == 0) w->msg = MSG_NONE;
}

/* ---- the bot -------------------------------------------------------------------------------------------------------------
 * It plays from the world: it forecasts every hornet PRED frames ahead (hornet_move on a copy), works out which ones the
 * gobs already in the air will pop, takes the most pressing of the others (its own first: the one that would reach a
 * head soonest) and the aim that pops it soonest (the head's turn, the reload and the gob's flight included), turns to
 * it and spits. A human-like bot (skill 1..10) decides only every skill/2 + 1 frames, sees the hornets later, aims
 * a little off and spits before its aim is right. */
#define PRED 72
static int16_t fx_[MAX_HORNETS][PRED], fy_[MAX_HORNETS][PRED];   /* the forecast (scratch: state_audit.txt) */

static uint32_t bot_hash(const world *w, int p, int k)
{
    uint32_t x = (uint32_t)(w->t * 2654435761u) ^ (uint32_t)(p * 40503 + k * 9973) ^ w->seed;
    x ^= x >> 15;
    x *= 0x2c1b3c6du;
    x ^= x >> 12;
    return x;
}

pin bot_decide(world *w, int p, int skill)
{
    pin out = {0, 0, 0, 0, 0, 1};
    llama *l = &w->l[p];
    if (l->state != LS_PLAY) return out;
    if (skill && w->t - l->bot_t < skill / 2 + 1) {     /* a human-like bot keeps its last decision a while */
        out.dy = l->bot_dy;
        return out;
    }
    l->bot_t = w->t;

    int hits[MAX_HORNETS] = {0}, live[MAX_HORNETS] = {0};
    int sees = skill ? 320 - 6 * skill : ENTRY_R + 8;
    for (int k = 0; k < MAX_HORNETS; k++) {
        const hornet *h = &w->hn[k];
        if (!h->on || h->pat == 255 || px(h->x) > sees) continue;
        hornet c = *h;
        live[k] = 1;
        int32_t tx = 0, ty = 0, r = HEAD_R + kind_r[h->kind];
        if (h->target < w->players) llama_head(h->target, w->l[h->target].aim, &tx, &ty);
        for (int f = 0; f < PRED; f++) {
            hornet_move(w, &c);
            int dx = px(c.x - tx), dy = px(c.y - ty), gone = dx * dx + dy * dy < r * r;   /* it stings there */
            fx_[k][f] = (int16_t)px(c.x);
            fy_[k][f] = (int16_t)(c.t < 0 ? -999 : px(c.y));
            if (gone)
                for (; f < PRED; f++) fx_[k][f] = 0, fy_[k][f] = -999;
        }
    }
    /* the gobs in the air: which hornets will they pop? */
    if (skill < 6)
        for (int i = 0; i < MAX_PLAYERS * GOBS_MAX; i++) {
            const gob *g = &w->gobs[i];
            if (!g->on) continue;
            int32_t gx = g->x, gy = g->y, vy = g->vy;
            for (int f = 0; f < PRED && gx < q16(GOB_RANGE) && gy < q16(GROUND_HIT); f++) {
                gx += g->vx, gy += vy, vy += GOB_GRAV;
                int done = 0;
                for (int k = 0; k < MAX_HORNETS && !done; k++) {
                    if (!live[k] || fy_[k][f] == -999) continue;
                    int r = kind_r[w->hn[k].kind] + GOB_R, dx = px(gx) - fx_[k][f], dy = px(gy) - fy_[k][f];
                    if (dx * dx + dy * dy < r * r) hits[k]++, done = 1;
                }
                if (done) break;
            }
        }
    /* the most pressing hornet: the soonest at a head (its own llama's first) */
    int32_t hx, hy;
    llama_head(p, l->aim, &hx, &hy);
    int best = -1, best_t = 1 << 30;
    for (int k = 0; k < MAX_HORNETS; k++) {
        if (!live[k] || hits[k] >= w->hn[k].hp) continue;
        int when = PRED + px(w->hn[k].x) / 4;
        for (int f = 0; f < PRED; f++) {
            if (fy_[k][f] == -999) continue;
            int dx = fx_[k][f] - px(hx), dy = fy_[k][f] - px(hy);
            if (dx * dx + dy * dy < 40 * 40 || fx_[k][f] < px(hx)) { when = f; break; }
        }
        if (w->hn[k].target != p) when += 50;
        if (when < best_t) { best_t = when; best = k; }
    }
    int want = AIM_START;
    int found = 0;
    if (best >= 0) {
        /* the aim that pops it soonest */
        int best_total = 1 << 30;
        for (int a = AIM_MIN; a <= AIM_MAX; a += 3) {
            int turn = iabs(a - l->aim), d = turn <= AIM_SLOW * AIM_ACCEL ? (turn + 1) / AIM_SLOW
                                                                         : AIM_ACCEL + (turn - AIM_SLOW * AIM_ACCEL + 4) / AIM_FAST;
            d = imax(imax(d, l->reload), 1);
            if (d >= PRED) continue;
            int32_t mx, my, vx, vy;
            llama_mouth(p, a, &mx, &my);
            gob_velocity(a, &vx, &vy);
            int r = kind_r[w->hn[best].kind] + GOB_R - 1;
            for (int f = 1; d + f - 1 < PRED; f++) {
                int32_t gx, gy;
                gob_at(mx, my, vx, vy, f, &gx, &gy);
                if (gx >= q16(GOB_RANGE) || gy >= q16(GROUND_HIT)) break;
                int k = d + f - 1;
                if (fy_[best][k] == -999) continue;
                int dx = px(gx) - fx_[best][k], dy = px(gy) - fy_[best][k];
                if (dx * dx + dy * dy < r * r) {
                    if (d + f < best_total || (d + f == best_total && iabs(a - l->aim) < iabs(want - l->aim))) {
                        best_total = d + f;
                        want = a;
                        found = 1;
                    }
                    break;
                }
            }
        }
        if (skill && found) {                               /* a human aims a little off (a new error each spit) */
            if (l->bot_target != best || l->bot_spit != l->spits) {
                l->bot_err = (int)(bot_hash(w, p, l->spits) % (uint32_t)(skill * 4 + 1)) - skill * 2;
                l->bot_spit = l->spits;
            }
            want = clampi(want + l->bot_err, AIM_MIN, AIM_MAX);
        }
        l->bot_target = best;
    }
    int diff = want - l->aim;
    out.dy = diff >= 3 ? -1 : diff <= -3 ? 1 : 0;
    if (out.dy && iabs(diff) < AIM_FAST + 2 && l->hold > AIM_ACCEL) out.dy = 0;   /* no overshoot: let go, then nudge */
    if (found && iabs(diff) <= 3 + skill && l->reload <= 0 && gobs_of(w, p) < GOBS_MAX) out.spit = 1;
    l->bot_dy = out.dy;
    return out;
}
