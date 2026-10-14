/*
 * Cosmo Corgi: the rules. No drawing, no sound, no screen width (widescreen shows more of the same world: the rules
 * are the stage's at every width). Integer maths only, the world's own RNGs: every run is deterministic.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE.
 *
 * The world scrolls right to left (w->scroll); the corgis' rockets fly freely on the 320 x 240 stage and fire by
 * themselves. Each section (a theme) sends its scripted waves of robots (WAVES), then its boss; four sections make a
 * loop, and the next loop is faster and shoots more. Robots from the right glide in from beyond the widest screen
 * (SPAWN_R) to their pattern's start (ENTRY_R), from the left from SPAWN_L: the same at every width.
 */
#include "game.h"
#include <string.h>

/* ---- small maths ------------------------------------------------------------------------------------------------ */
static const int16_t SIN_Q[65] = {
    0, 402, 804, 1205, 1606, 2006, 2404, 2801, 3196, 3590, 3981, 4370, 4756, 5139, 5520, 5897, 6270, 6639, 7005,
    7366, 7723, 8076, 8423, 8765, 9102, 9434, 9760, 10080, 10394, 10702, 11003, 11297, 11585, 11866, 12140, 12406,
    12665, 12916, 13160, 13395, 13623, 13842, 14053, 14256, 14449, 14635, 14811, 14978, 15137, 15286, 15426, 15557,
    15679, 15791, 15893, 15986, 16069, 16143, 16207, 16261, 16305, 16340, 16364, 16379, 16384};

int isin256(int a)
{
    a &= 255;
    if (a < 64) return SIN_Q[a];
    if (a < 128) return SIN_Q[128 - a];
    if (a < 192) return -SIN_Q[a - 128];
    return -SIN_Q[256 - a];
}

static int iabs(int v) { return v < 0 ? -v : v; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int fdiv8(int v) { return v >= 0 ? v >> 3 : -((7 - v) >> 3); }        /* floor(v / 8) */
static int32_t q16(int px) { return (int32_t)px * Q16_ONE; }
static int px(int32_t q) { return q >> 16; }
static int32_t sinq(int amp_px, int a) { return (int32_t)amp_px * isin256(a) * 4; }   /* amp * sin, Q16 */

static uint32_t isqrt32(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}

/* rotate a Q16 vector by a (1/256 turns) */
static void rotate(int32_t *vx, int32_t *vy, int a)
{
    int64_t c = icos256(a), s = isin256(a);
    int64_t x = *vx, y = *vy;
    *vx = (int32_t)((x * c - y * s) >> 14);
    *vy = (int32_t)((x * s + y * c) >> 14);
}

/* a Q16 velocity of the given speed from (x0, y0) toward (x1, y1) (Q16 points) */
static void aim(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t speed, int32_t *vx, int32_t *vy)
{
    int dx = px(x1 - x0), dy = px(y1 - y0);
    int len = (int)isqrt32((uint32_t)(dx * dx + dy * dy));
    if (len < 1) { *vx = -speed; *vy = 0; return; }
    *vx = (int32_t)((int64_t)speed * dx / len);
    *vy = (int32_t)((int64_t)speed * dy / len);
}

/* ---- the robots' tables ------------------------------------------------------------------------------------------ */
static const int K_HP[EK_KINDS] = KIND_HP;
static const int K_POINTS[EK_KINDS] = KIND_POINTS;
static const int32_t K_SPEED[EK_KINDS] = KIND_SPEED;
static const int K_HW[EK_KINDS] = {6, 6, 7, 6, 13, 6, 6, 6, 6};       /* hit boxes (half), px */
static const int K_HH[EK_KINDS] = {6, 6, 5, 5, 9, 6, 6, 6, 6};
int enemy_half_w(int kind) { return K_HW[kind]; }
int enemy_half_h(int kind) { return K_HH[kind]; }
static const int B_HW[4] = {26, 26, 22, 26}, B_HH[4] = {18, 18, 20, 18};
int boss_half_w(int kind) { return B_HW[kind & 3]; }
int boss_half_h(int kind) { return B_HH[kind & 3]; }

/* ---- the waves of each section (DESIGN.md "Sections"): sent when the section has scrolled `at` px ------------------
 * pattern parameters: LINE y = lane; SINE y = centre, p = amplitude; LOOP y = lane, p = the loop's x; SWOOP y = the lane
 * it ends on, p = the x it comes down (up) at; BEHIND (from the left) y = lane, p = amplitude; DASH y = lane, p = where
 * it winds up; HOVER y = lane, p = where it hovers; TANK y = lane; TURRET on the wall (WF_CEIL / WF_FLOOR) at the
 * column then at the screen's far right; DRIFT y = where it crosses the stage's edge, p = its fall in 1/16 px/frame. */
typedef struct wave_def { int16_t at; uint8_t kind, pat, n, gap; int16_t y, p; uint8_t flags; } wave_def;
enum { WF_GROUP = 1, WF_CEIL = 2, WF_FLOOR = 4 };
static const wave_def WAVES0[] = {               /* the pastel nebula: open space, gentle */
    {100, EK_BUG, PAT_SINE, 5, 20, 80, 24, 0},
    {340, EK_BUG, PAT_SINE, 5, 20, 160, 24, 0},
    {600, EK_DRONE, PAT_LOOP, 5, 14, 72, 200, WF_GROUP},
    {840, EK_CARRIER, PAT_LINE, 1, 0, 120, 0, 0},
    {1000, EK_BUG, PAT_LINE, 4, 22, 60, 0, 0},
    {1000, EK_BUG, PAT_LINE, 4, 22, 180, 0, 0},
    {1200, EK_UFO, PAT_HOVER, 1, 0, 80, 260, 0},
    {1240, EK_UFO, PAT_HOVER, 1, 0, 160, 240, 0},
    {1420, EK_DRONE, PAT_SWOOP_TOP, 5, 14, 90, 280, WF_GROUP},
    {1640, EK_DRONE, PAT_SWOOP_BOT, 5, 14, 150, 280, 0},
    {1840, EK_BUG, PAT_SINE, 6, 18, 120, 48, 0},
    {1960, EK_TANK, PAT_TANK, 1, 0, 120, 0, 0},
    {2220, EK_UFO, PAT_HOVER, 1, 0, 60, 270, 0},
    {2260, EK_UFO, PAT_HOVER, 1, 0, 180, 250, 0},
    {2400, EK_DRONE, PAT_LOOP, 5, 14, 168, 210, WF_GROUP},
};
static const wave_def WAVES1[] = {               /* the asteroid belt: rocky walls, turrets, drifting rocks, dashers */
    {120, EK_ROCK, PAT_DRIFT, 3, 50, 70, 6, 0},
    {300, EK_BUG, PAT_SINE, 5, 18, 120, 30, 0},
    {420, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_FLOOR},
    {500, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_CEIL},
    {640, EK_DASHER, PAT_DASH, 3, 44, 100, 280, 0},
    {840, EK_DRONE, PAT_LOOP, 5, 14, 120, 220, WF_GROUP},
    {1020, EK_ROCK, PAT_DRIFT, 4, 34, 64, 8, 0},
    {1120, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_FLOOR},
    {1180, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_CEIL},
    {1320, EK_UFO, PAT_HOVER, 1, 0, 90, 250, 0},
    {1360, EK_UFO, PAT_HOVER, 1, 0, 150, 270, 0},
    {1480, EK_CARRIER, PAT_LINE, 1, 0, 110, 0, 0},
    {1620, EK_DASHER, PAT_DASH, 4, 40, 140, 270, 0},
    {1760, EK_TANK, PAT_TANK, 1, 0, 120, 0, 0},
    {1960, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_FLOOR},
    {2020, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_CEIL},
    {2160, EK_DRONE, PAT_SWOOP_TOP, 5, 14, 100, 270, WF_GROUP},
    {2360, EK_BUG, PAT_SINE, 6, 16, 120, 40, 0},
};
static const wave_def WAVES2[] = {               /* the robot station: metal walls with gates, turrets, from behind */
    {120, EK_DRONE, PAT_LINE, 4, 16, 100, 0, 0},
    {200, EK_DRONE, PAT_LINE, 4, 16, 140, 0, 0},
    {320, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_FLOOR},
    {380, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_CEIL},
    {500, EK_UFO, PAT_HOVER, 1, 0, 100, 260, 0},
    {660, EK_DRONE, PAT_BEHIND, 5, 16, 90, 12, WF_GROUP},
    {860, EK_TANK, PAT_TANK, 1, 0, 130, 0, 0},
    {1000, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_FLOOR},
    {1060, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_CEIL},
    {1160, EK_DASHER, PAT_DASH, 3, 44, 120, 280, 0},
    {1360, EK_DRONE, PAT_LOOP, 5, 14, 110, 210, WF_GROUP},
    {1560, EK_CARRIER, PAT_LINE, 1, 0, 120, 0, 0},
    {1660, EK_BUG, PAT_SINE, 6, 16, 120, 36, 0},
    {1820, EK_DRONE, PAT_BEHIND, 5, 16, 150, 12, 0},
    {2000, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_FLOOR},
    {2060, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_CEIL},
    {2160, EK_UFO, PAT_HOVER, 1, 0, 90, 250, 0},
    {2200, EK_UFO, PAT_HOVER, 1, 0, 150, 270, 0},
    {2340, EK_TANK, PAT_TANK, 1, 0, 110, 0, 0},
};
static const wave_def WAVES3[] = {               /* the icy comets: icy walls, comets, dashers, from behind */
    {120, EK_COMET, PAT_DRIFT, 3, 40, 60, 10, 0},
    {300, EK_DRONE, PAT_SWOOP_BOT, 5, 14, 140, 280, WF_GROUP},
    {480, EK_DASHER, PAT_DASH, 3, 40, 110, 280, 0},
    {620, EK_COMET, PAT_DRIFT, 4, 30, 70, 12, 0},
    {800, EK_BUG, PAT_SINE, 6, 16, 120, 40, 0},
    {980, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_FLOOR},
    {1040, EK_TURRET, PAT_TURRET, 1, 0, 0, 0, WF_CEIL},
    {1120, EK_DRONE, PAT_BEHIND, 5, 16, 130, 16, WF_GROUP},
    {1320, EK_UFO, PAT_HOVER, 1, 0, 80, 260, 0},
    {1360, EK_UFO, PAT_HOVER, 1, 0, 160, 250, 0},
    {1480, EK_COMET, PAT_DRIFT, 4, 30, 60, 12, 0},
    {1620, EK_TANK, PAT_TANK, 1, 0, 120, 0, 0},
    {1700, EK_BUG, PAT_SINE, 5, 18, 90, 24, 0},
    {1820, EK_DASHER, PAT_DASH, 4, 36, 130, 270, 0},
    {2000, EK_CARRIER, PAT_LINE, 1, 0, 100, 0, 0},
    {2140, EK_DRONE, PAT_LOOP, 5, 14, 140, 210, WF_GROUP},
    {2340, EK_COMET, PAT_DRIFT, 5, 26, 60, 14, 0},
};
static const wave_def *const WAVES[SECTIONS] = {WAVES0, WAVES1, WAVES2, WAVES3};
static const int WAVES_N[SECTIONS] = {sizeof WAVES0 / sizeof WAVES0[0], sizeof WAVES1 / sizeof WAVES1[0],
                                       sizeof WAVES2 / sizeof WAVES2[0], sizeof WAVES3 / sizeof WAVES3[0]};
_Static_assert(sizeof WAVES2 / sizeof WAVES2[0] <= 32, "a section's waves fit waves_done's 32 bits");

int world_theme(int sec) { return sec & 3; }
int world_stage(const world *w) { return w->loop * SECTIONS + w->sec + 1; }

/* the loop's speed-ups */
static int32_t robot_speed(const world *w, int kind)
{
    int l = w->loop < 4 ? w->loop : 4;
    return (int32_t)((int64_t)K_SPEED[kind] * (100 + LOOP_SPEED_PCT * l) / 100);
}

static int32_t eb_speed(const world *w)
{
    int32_t v = EB_SPEED + EB_SPEED_LOOP * w->loop;
    return v > EB_SPEED_MAX ? EB_SPEED_MAX : v;
}

static int reload(const world *w, int frames)
{
    int pct = 100 - FIRE_LOOP_PCT * w->loop;
    return frames * (pct < 50 ? 50 : pct) / 100;
}

/* ---- effects ------------------------------------------------------------------------------------------------------- */
static void fx(world *w, int kind, int x, int y, int value)
{
    effect *e = &w->fxs[w->fxs_next];
    w->fxs_next = (w->fxs_next + 1) % FX_MAX;
    e->x = (int16_t)x;
    e->y = (int16_t)y;
    e->value = (int16_t)value;
    e->kind = (uint8_t)kind;
    e->t = 0;
    e->on = 1;
}

static int fx_life(int kind)
{
    static const int life[] = {18, 48, 40, 8, 18, 24};
    return life[kind];
}

/* ---- the walls ------------------------------------------------------------------------------------------------------
 * A ring of the columns around the stage (world column c at c mod TER_RING), made ahead of the far right edge as the
 * world scrolls: heights in tiles from the top (ceil) and the bottom (floor), each section's theme. A random walk
 * between the theme's limits with plateaus (turrets sit on them), always TER_MIN_GAP tiles open; it grows in at the
 * section's start, and fades out before its boss (an open arena). */
static int ring(int c) { return ((c % TER_RING) + TER_RING) % TER_RING; }
static int ter_valid(const world *w, int c) { return c < w->ter_col && c >= w->ter_col - TER_RING; }
int ter_ceil_at(const world *w, int c) { return ter_valid(w, c) ? w->ter_ceil[ring(c)] : 0; }
int ter_floor_at(const world *w, int c) { return ter_valid(w, c) ? w->ter_floor[ring(c)] : 0; }
int ter_theme_at(const world *w, int c) { return ter_valid(w, c) ? w->ter_theme[ring(c)] : 0; }

static void ter_make(world *w)
{
    int c = w->ter_col;
    if (w->ter_next_col0 >= 0 && c >= w->ter_next_col0) {     /* the next section's walls begin */
        w->ter_sec = (w->ter_sec + 1) % SECTIONS;
        w->ter_col0 = w->ter_next_col0;
        w->ter_next_col0 = -1;
        w->ter_run = 0;
    }
    int theme = world_theme(w->ter_sec), j = c - w->ter_col0, len = SECTION_LEN / 8;
    int env = j < 0 ? 0 : j < 16 ? j / 2 + 1 : j < len - 8 ? 9 : j < len + 8 ? (len + 8 - j) / 2 : 0;
    int ce = 0, fl = 0;
    if (theme != THEME_NEBULA) {
        static const int cmin[4] = {0, 1, 2, 1}, cmax[4] = {0, 5, 4, 4}, fmin[4] = {0, 1, 2, 0}, fmax[4] = {0, 5, 4, 3};
        if (--w->ter_run <= 0) {
            w->ter_c = cmin[theme] + rs_rng_range(&w->ter_rng, cmax[theme] - cmin[theme] + 1);
            w->ter_f = fmin[theme] + rs_rng_range(&w->ter_rng, fmax[theme] - fmin[theme] + 1);
            w->ter_run = 4 + rs_rng_range(&w->ter_rng, 6);
        }
        int tc = w->ter_c, tf = w->ter_f;
        if (theme == THEME_STATION && j % 40 >= 30 && j % 40 < 34) tc = tf = 6;     /* a gate */
        int pc = ter_ceil_at(w, c - 1), pf = ter_floor_at(w, c - 1);
        ce = clampi(tc, pc - 1, pc + 1);                           /* the walls change one tile per column */
        fl = clampi(tf, pf - 1, pf + 1);
        if (ce > env) ce = env;
        if (fl > env) fl = env;
        while (30 - ce - fl < TER_MIN_GAP) {
            if (ce >= fl) ce--;
            else fl--;
        }
    }
    w->ter_ceil[ring(c)] = (uint8_t)ce;
    w->ter_floor[ring(c)] = (uint8_t)fl;
    w->ter_theme[ring(c)] = (uint8_t)theme;
    w->ter_col = c + 1;
}

static void ter_ahead(world *w)
{
    int need = fdiv8(px(w->scroll) + SPAWN_R + 32) + 1;
    while (w->ter_col < need) ter_make(w);
}

/* the walls at a stage box, the world scrolled by `ahead` more px (the bot's forecast) */
static int wall_hit_at(const world *w, int x0, int y0, int x1, int y1, int ahead)
{
    int s = px(w->scroll) + ahead;
    for (int c = fdiv8(s + x0); c <= fdiv8(s + x1); c++) {
        int ce = ter_ceil_at(w, c), fl = ter_floor_at(w, c);
        if ((ce && y0 < ce * 8) || (fl && y1 >= STAGE_H - fl * 8)) return 1;
    }
    return 0;
}

int wall_hit(const world *w, int x0, int y0, int x1, int y1) { return wall_hit_at(w, x0, y0, x1, y1, 0); }

/* ---- spawning ------------------------------------------------------------------------------------------------------ */
static enemy *enemy_new(world *w)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (!w->en[i].on) {
            memset(&w->en[i], 0, sizeof w->en[i]);
            return &w->en[i];
        }
    return NULL;
}

/* one robot of a wave: its pattern starts in `delay` frames (t < 0 until then: it glides in from beyond the widest
 * screen along its entry line, the same at every width) */
static enemy *spawn_robot(world *w, int kind, int pat, int y0, int p, int delay, int group)
{
    enemy *e = enemy_new(w);
    if (!e) return NULL;
    e->on = 1;
    e->kind = (uint8_t)kind;
    e->pat = (uint8_t)pat;
    e->hp = (uint8_t)K_HP[kind];
    e->group = (uint8_t)group;
    e->y0 = (int16_t)y0;
    e->p = (int16_t)p;
    e->v = robot_speed(w, kind);
    int32_t ex, ey, gx = 0, gy = 0;                   /* the entry point and the glide in */
    int pre = 0;
    switch (pat) {
    case PAT_SWOOP_TOP:
        ex = q16(p), ey = q16(-12), gy = e->v;
        break;
    case PAT_SWOOP_BOT:
        ex = q16(p), ey = q16(STAGE_H + 12), gy = -e->v;
        break;
    case PAT_BEHIND:
        ex = q16(ENTRY_L), ey = q16(y0), gx = e->v;
        pre = (int)((q16(ENTRY_L - SPAWN_L) + e->v - 1) / e->v);
        break;
    default:
        ex = q16(ENTRY_R), ey = q16(y0), gx = -e->v;
        if (pat == PAT_DRIFT) gy = p * 4096;
        pre = (int)((q16(SPAWN_R - ENTRY_R) + e->v - 1) / e->v);
        break;
    }
    int t0 = pre + delay;
    e->t = (int16_t)-t0;
    e->x = ex - gx * t0;
    e->y = ey - gy * t0;
    e->vx = gx;
    e->vy = gy;
    e->side = (uint8_t)(y0 < STAGE_H / 2 ? 1 : 0);      /* loops and exits turn away from the nearest edge */
    e->fire_t = (int16_t)(kind == EK_TANK || kind == EK_TURRET ? 50 + rs_rng_range(&w->rng, 60)
                                                               : 40 + rs_rng_range(&w->rng, 140));
    return e;
}

/* a turret on the wall at the far right: the first plateau there (two columns of the same height) */
static void spawn_turret(world *w, int ceil, int group)
{
    int c0 = fdiv8(px(w->scroll) + SPAWN_R);
    for (int c = c0; c < c0 + 8; c++) {
        int h0 = ceil ? ter_ceil_at(w, c) : ter_floor_at(w, c), h1 = ceil ? ter_ceil_at(w, c + 1) : ter_floor_at(w, c + 1);
        if (h0 <= 0 || h0 != h1 || !ter_valid(w, c + 1)) continue;
        enemy *e = enemy_new(w);
        if (!e) return;
        e->on = 1;
        e->kind = EK_TURRET;
        e->pat = PAT_TURRET;
        e->hp = (uint8_t)K_HP[EK_TURRET];
        e->group = (uint8_t)group;
        e->side = (uint8_t)ceil;
        e->wx = q16(c * 8 + 8);
        e->x = e->wx - w->scroll;
        e->y = q16(ceil ? h0 * 8 + 7 : STAGE_H - h0 * 8 - 7);
        e->fire_t = (int16_t)(60 + rs_rng_range(&w->rng, 60));
        return;
    }
}

static void send_wave(world *w, const wave_def *d)
{
    int group = 0;
    if (d->flags & WF_GROUP) {
        int g = w->group_n++ % 8;
        w->group_left[g] = d->n;
        group = g + 1;
    }
    for (int i = 0; i < d->n; i++) {
        if (d->pat == PAT_TURRET) spawn_turret(w, (d->flags & WF_CEIL) != 0, group);
        else spawn_robot(w, d->kind, d->pat, d->y, d->p, i * d->gap, group);
    }
}

static void capsule_drop(world *w, int32_t x, int32_t y)
{
    static const int order[] = CAP_ORDER;
    for (int i = 0; i < MAX_CAPS; i++) {
        capsule *c = &w->caps[i];
        if (c->on) continue;
        c->on = 1;
        c->kind = (uint8_t)order[w->cap_n++ % (int)(sizeof order / sizeof order[0])];
        c->x = x;
        c->y = q16(clampi(px(y), 24, STAGE_H - 24));
        c->t = 0;
        return;
    }
}

/* ---- enemy shots ------------------------------------------------------------------------------------------------- */
static ebullet *eb_new(world *w)
{
    for (int i = 0; i < MAX_EBULLETS; i++)
        if (!w->eb[i].on) return &w->eb[i];
    return NULL;
}

static void eb_fire(world *w, int kind, int32_t x, int32_t y, int32_t vx, int32_t vy)
{
    ebullet *b = eb_new(w);
    if (!b) return;
    b->on = 1;
    b->kind = (uint8_t)kind;
    b->x = x;
    b->y = y;
    b->vx = vx;
    b->vy = vy;
    w->wevents |= WEV_ESHOT;
}

/* the nearest corgi still flying (-1: none) and its distance in px */
static int nearest_hero(const world *w, int32_t x, int32_t y, int *dist)
{
    int best = -1, bd = 1 << 30;
    for (int p = 0; p < w->players; p++) {
        const hero *h = &w->h[p];
        if (h->state != SH_PLAY) continue;
        int dx = px(h->x - x), dy = px(h->y - y), d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = p; }
    }
    if (dist) *dist = best < 0 ? 1 << 15 : (int)isqrt32((uint32_t)bd);
    return best;
}

/* a fan of n shots aimed at the nearest corgi, `spread` apart (1/256 turns) */
static void eb_fan(world *w, int kind, int32_t x, int32_t y, int n, int spread, int32_t speed)
{
    int d, p = nearest_hero(w, x, y, &d);
    int32_t vx, vy;
    if (p < 0) aim(x, y, x - Q16_ONE, y, speed, &vx, &vy);
    else aim(x, y, w->h[p].x, w->h[p].y, speed, &vx, &vy);
    for (int i = 0; i < n; i++) {
        int32_t a = vx, b = vy;
        rotate(&a, &b, (i - (n - 1) / 2) * spread - ((n & 1) ? 0 : spread / 2));
        eb_fire(w, kind, x, y, a, b);
    }
}

/* a robot may fire: on the stage, and not at a corgi too close (fair) */
static int may_fire(const world *w, const enemy *e)
{
    int x = px(e->x), y = px(e->y), d;
    if (x < 16 || x > STAGE_W - 16 || y < 8 || y > STAGE_H - 8) return 0;
    if (nearest_hero(w, e->x, e->y, &d) < 0) return 0;
    return d >= EB_FAIR_DIST;
}

/* ---- the robots' flight --------------------------------------------------------------------------------------------- */
static void enemy_move(world *w, enemy *e)
{
    int32_t ox = e->x, oy = e->y;
    int t = e->t;
    if (e->pat == PAT_TURRET) {
        e->x = e->wx - w->scroll;
    } else if (t < 0) {                               /* gliding in along the entry line */
        e->x += e->vx;
        e->y += e->vy;
    } else {
        switch (e->pat) {
        case PAT_LINE:
            e->x -= e->v;
            break;
        case PAT_SINE:
            e->x -= e->v;
            e->y = q16(e->y0) + sinq(e->p, t * SINE_W);
            break;
        case PAT_LOOP: {
            int dir = e->side ? 1 : -1;               /* the upper ones loop down, the lower ones up */
            if (e->aux == 0) {
                e->x -= e->v;
                if (e->x <= q16(e->p)) { e->aux = 1; e->aux_t = (int16_t)t; }
            } else if (e->aux == 1) {
                int a = (t - e->aux_t) * LOOP_W;
                if (a >= 256) { e->aux = 2; a = 256; }
                e->x = q16(e->p) - sinq(LOOP_R, a);
                e->y = q16(e->y0) + dir * (q16(LOOP_R) - sinq(LOOP_R, a + 64));
            } else {
                e->x -= e->v;
            }
            break;
        }
        case PAT_SWOOP_TOP:
        case PAT_SWOOP_BOT: {
            int top = e->pat == PAT_SWOOP_TOP;
            int r = top ? e->y0 + 12 : STAGE_H + 12 - e->y0;
            if (e->aux == 0) {
                /* a quarter turn at the robot's speed: v / (2 pi r) turns a frame */
                int a = (int)((int64_t)e->v * t * 256000 / (6283LL * r) >> 16);
                if (a >= 64) { e->aux = 1; a = 64; }
                e->x = q16(e->p - r) + sinq(r, a + 64);
                e->y = top ? q16(-12) + sinq(r, a) : q16(STAGE_H + 12) - sinq(r, a);
            } else {
                e->x -= e->v;
                e->y = q16(e->y0);
            }
            break;
        }
        case PAT_BEHIND:
            e->x += e->v;
            e->y = q16(e->y0) + sinq(e->p, t * SINE_W);
            break;
        case PAT_DASH:
            if (e->aux == 0) {
                e->x -= e->v;
                if (e->x <= q16(e->p)) { e->aux = 1; e->aux_t = (int16_t)t; }
            } else if (e->aux == 1) {                 /* winding up: it slides to a corgi's line */
                int p = nearest_hero(w, e->x, e->y, NULL);
                if (p >= 0) {
                    int dy = px(w->h[p].y - e->y);
                    e->y += dy > 0 ? Q16_ONE : dy < 0 ? -Q16_ONE : 0;
                }
                if (t - e->aux_t >= DASH_HOLD) { e->aux = 2; w->wevents |= WEV_DASH; }
            } else {
                e->x -= DASH_SPEED;
            }
            break;
        case PAT_HOVER:
            if (e->aux == 0) {
                int32_t d = e->x - q16(e->p), step = d > q16(32) ? e->v : d / 16;
                if (step < Q16_ONE / 4) step = Q16_ONE / 4;
                e->x -= step;
                if (e->x - q16(e->p) < Q16_ONE / 2) { e->aux = 1; e->aux_t = (int16_t)t; }
            } else if (e->aux == 1) {
                int k = t - e->aux_t;
                e->y = q16(e->y0) + sinq(3, k * 4);
                if (k == HOVER_FRAMES / 2 && may_fire(w, e)) {
                    if (w->loop >= 1) eb_fan(w, EB_PELLET, e->x, e->y, 3, 14, eb_speed(w));
                    else eb_fan(w, EB_PELLET, e->x, e->y, 1, 0, eb_speed(w));
                }
                if (k >= HOVER_FRAMES) { e->aux = 2; e->aux_t = (int16_t)t; }
            } else {
                int k = t - e->aux_t, vy = k * 3000;
                if (vy > q16(3)) vy = q16(3);
                e->y += e->side ? -vy : vy;           /* away up (the upper ones) or down */
                e->x -= e->v / 4;
            }
            break;
        case PAT_TANK:
            e->x -= e->v;
            e->y = q16(e->y0) + sinq(2, t * 4);
            break;
        case PAT_DRIFT:
            e->x -= e->v;
            e->y += e->p * 4096;
            break;
        }
    }
    e->vx = e->x - ox;
    e->vy = e->y - oy;
    if (e->t < 30000) e->t++;
}

static void enemy_fire(world *w, enemy *e)
{
    if (e->t <= 0 && e->pat != PAT_TURRET) return;
    switch (e->kind) {
    case EK_TANK:
        if (--e->fire_t <= 0) {
            e->fire_t = (int16_t)reload(w, TANK_FIRE);
            if (may_fire(w, e)) eb_fan(w, EB_PELLET, e->x - q16(10), e->y, 3, 16, eb_speed(w));
        }
        break;
    case EK_TURRET:
        if (--e->fire_t <= 0) {
            e->fire_t = (int16_t)reload(w, TURRET_FIRE);
            if (may_fire(w, e)) eb_fan(w, EB_PELLET, e->x, e->y, 1, 0, eb_speed(w));
        }
        break;
    case EK_BUG:
    case EK_DRONE:
    case EK_DASHER:
        if (w->loop + 1 >= SMALL_FIRE_LOOP && !e->fired && e->t >= e->fire_t && may_fire(w, e)) {
            e->fired = 1;
            eb_fan(w, EB_PELLET, e->x, e->y, 1, 0, eb_speed(w));
        }
        break;
    }
}

/* ---- the corgis ------------------------------------------------------------------------------------------------------ */
static void hero_reset(world *w, int p)
{
    static const int y0[MAX_PLAYERS] = {120, 80, 160, 40};
    hero *h = &w->h[p];
    memset(h, 0, sizeof *h);
    h->x = q16(ROCKET_X0 - (p & 1) * 16);
    h->y = q16(y0[p]);
    h->state = p < w->players ? SH_READY : SH_OFF;
    h->fire_t = 1;
    for (int i = 0; i < TRAIL_N; i++) {
        h->trail_x[i] = (int16_t)px(h->x);
        h->trail_y[i] = (int16_t)px(h->y);
    }
}

int pod_pos(const world *w, int p, int k, int *x, int *y)
{
    const hero *h = &w->h[p];
    if (k >= h->pods || (h->state != SH_PLAY && h->state != SH_READY)) return 0;
    int i = ((h->trail_i - POD_DELAY * (k + 1)) % TRAIL_N + TRAIL_N) % TRAIL_N;
    *x = h->trail_x[i];
    *y = h->trail_y[i];
    return 1;
}

static shot *shot_new(world *w, int p)
{
    for (int i = 0; i < SHOTS_PER_PLAYER; i++) {
        shot *s = &w->shots[p * SHOTS_PER_PLAYER + i];
        if (!s->on) {
            memset(s, 0, sizeof *s);
            return s;
        }
    }
    return NULL;
}

static shot *shoot(world *w, int p, int kind, int32_t x, int32_t y, int32_t vx, int32_t vy, int damage)
{
    shot *s = shot_new(w, p);
    if (!s) return NULL;
    s->on = 1;
    s->kind = (uint8_t)kind;
    s->owner = (uint8_t)p;
    s->damage = (uint8_t)damage;
    s->x = x;
    s->y = y;
    s->vx = vx;
    s->vy = vy;
    s->life = kind == SK_BONE ? BONE_LIFE : 400;
    s->angle = (int16_t)(vy < 0 ? -16 : vy > 0 ? 16 : 0);
    return s;
}

/* auto-fire: a volley every FIRE_GAP frames (the bark, or the spread shot), the beams and the bones on their own
 * timers, and each puppy pod barks with the volley */
static void hero_fire(world *w, int p)
{
    static const int32_t spread_vy[4] = SPREAD_VY;
    static const int beam_gap[4] = BEAM_GAP, beam_dmg[4] = BEAM_DAMAGE, bone_gap[4] = BONE_GAP, bone_n[4] = BONE_N;
    hero *h = &w->h[p];
    int32_t x = h->x + q16(12), y = h->y;
    int L = h->wlevel, wpn = L ? h->weapon : W_BARK;
    if (--h->fire_t <= 0) {
        h->fire_t = wpn == W_SPREAD && L >= 3 ? FIRE_GAP - 2 : FIRE_GAP;
        h->shots++;
        w->events[p] |= EV_FIRE;
        if (wpn == W_SPREAD) {
            int32_t vy = spread_vy[L];
            shoot(w, p, SK_BARK, x, y, BARK_SPEED, 0, 1);
            shoot(w, p, SK_SPREAD, x, y - q16(2), BARK_SPEED - Q16_ONE / 2, -vy, 1);
            shoot(w, p, SK_SPREAD, x, y + q16(2), BARK_SPEED - Q16_ONE / 2, vy, 1);
            if (L >= 2) {
                shoot(w, p, SK_SPREAD, x, y - q16(1), BARK_SPEED - Q16_ONE / 4, -vy / 2, 1);
                shoot(w, p, SK_SPREAD, x, y + q16(1), BARK_SPEED - Q16_ONE / 4, vy / 2, 1);
            }
        } else if (wpn == W_HOMING && L >= 3) {
            shoot(w, p, SK_BARK, x, y - q16(4), BARK_SPEED, 0, 1);
            shoot(w, p, SK_BARK, x, y + q16(4), BARK_SPEED, 0, 1);
        } else if (wpn != W_BEAM) {
            shoot(w, p, SK_BARK, x, y, BARK_SPEED, 0, 1);
        }
        for (int k = 0; k < h->pods; k++) {
            int ox, oy;
            if (pod_pos(w, p, k, &ox, &oy)) shoot(w, p, SK_BARK, q16(ox + 6), q16(oy), BARK_SPEED, 0, 1);
        }
    }
    if (wpn == W_BEAM && --h->beam_t <= 0) {
        h->beam_t = beam_gap[L];
        w->events[p] |= EV_FIRE;
        if (L >= 3) {
            shoot(w, p, SK_BEAM, x + q16(8), y - q16(5), BEAM_SPEED, 0, beam_dmg[L]);
            shoot(w, p, SK_BEAM, x + q16(8), y + q16(5), BEAM_SPEED, 0, beam_dmg[L]);
        } else {
            shoot(w, p, SK_BEAM, x + q16(8), y, BEAM_SPEED, 0, beam_dmg[L]);
        }
    }
    if (wpn == W_HOMING && --h->bone_t <= 0) {
        h->bone_t = bone_gap[L];
        w->events[p] |= EV_BONE;
        for (int i = 0; i < bone_n[L]; i++) {
            int32_t vx = BONE_SPEED, vy = 0;
            int a = bone_n[L] == 1 ? -24 : i ? 24 : -24;
            rotate(&vx, &vy, a);
            shot *s = shoot(w, p, SK_BONE, h->x, h->y + q16(i ? 4 : -4), vx, vy, BONE_DAMAGE);
            if (s) s->angle = (int16_t)a;
        }
    }
}

static void apply_capsule(world *w, int p, int kind)
{
    hero *h = &w->h[p];
    int maxed = 0;
    switch (kind) {
    case CAP_SPREAD:
    case CAP_BEAM:
    case CAP_HOMING: {
        int wpn = kind == CAP_SPREAD ? W_SPREAD : kind == CAP_BEAM ? W_BEAM : W_HOMING;
        if (h->weapon == wpn && h->wlevel >= WEAPON_MAX) maxed = 1;
        else {
            h->weapon = wpn;
            if (h->wlevel < WEAPON_MAX) h->wlevel++;
            h->beam_t = h->bone_t = 1;
        }
        break;
    }
    case CAP_SPEED:
        if (h->speed >= SPEED_MAX) maxed = 1;
        else h->speed++;
        break;
    case CAP_SHIELD:
        if (h->shield) maxed = 1;
        else h->shield = 1;
        break;
    case CAP_POD:
        if (h->pods >= PODS_MAX) maxed = 1;
        else h->pods++;
        break;
    }
    h->caps++;
    w->events[p] |= EV_CAPSULE;
    w->msg = maxed ? MSG_MAX : MSG_SPREAD + kind;
    w->msg_t = MSG_FRAMES;
    w->msg_player = p;
    if (maxed) {
        h->score += MAXED_POINTS;
        w->events[p] |= EV_MAXED;
        fx(w, FX_POINTS, px(h->x), px(h->y) - 14, MAXED_POINTS);
    }
}

/* a corgi is hit: the shield takes it (and pops), else the corgi is out (one hit ends its run) */
static void hurt(world *w, int p)
{
    hero *h = &w->h[p];
    if (h->state != SH_PLAY || h->invul) return;
    if (h->shield) {
        h->shield = 0;
        h->invul = SHIELD_INVUL;
        w->events[p] |= EV_SHIELD;
        fx(w, FX_SHIELD, px(h->x), px(h->y), 0);
        /* pushed into the open: the middle of the gap between the walls there */
        int c = fdiv8(px(w->scroll) + px(h->x));
        int top = ter_ceil_at(w, c) * 8, bot = STAGE_H - ter_floor_at(w, c) * 8;
        if (px(h->y) - WALL_HH < top || px(h->y) + WALL_HH >= bot) h->y = q16((top + bot) / 2);
        return;
    }
    h->state = SH_HIT;
    h->t = 0;
    w->freeze = HIT_FREEZE;
    w->events[p] |= EV_HIT;
}

static void hero_step(world *w, int p, const pin *in)
{
    hero *h = &w->h[p];
    h->moving = 0;
    if (h->state == SH_HIT) {                         /* the bonk: it tumbles away, then the corgi is out */
        h->x -= Q16_ONE;
        h->y += h->t < 16 ? -Q16_ONE / 2 : Q16_ONE;
        if (++h->t >= HIT_FRAMES) {
            h->state = SH_OUT;
            h->t = 0;
            w->events[p] |= EV_OUT;
        }
        return;
    }
    if (h->state != SH_PLAY) {
        h->t++;
        return;
    }
    h->t++;
    int32_t ox = h->x, oy = h->y;
    if (in->touch) {                                  /* touch: the finger's movement, 1:1, at most TOUCH_MAX a frame */
        h->x += q16(clampi(in->tdx, -TOUCH_MAX, TOUCH_MAX));
        h->y += q16(clampi(in->tdy, -TOUCH_MAX, TOUCH_MAX));
    }
    if (in->dx || in->dy) {
        int32_t sp = ROCKET_SPEED + SPEED_STEP * h->speed;
        if (in->dx && in->dy) sp = sp * DIAG_K / 256;
        h->x += in->dx * sp;
        h->y += in->dy * sp;
    }
    h->x = clampi(h->x, q16(ROCKET_MIN_X), q16(ROCKET_MAX_X));
    h->y = clampi(h->y, q16(ROCKET_MIN_Y), q16(ROCKET_MAX_Y));
    int dy = h->y - oy;
    h->tilt = dy < -Q16_ONE / 4 ? -1 : dy > Q16_ONE / 4 ? 1 : 0;
    if (h->x != ox || h->y != oy) {
        h->moving = 1;
        h->trail_i = (h->trail_i + 1) % TRAIL_N;
        h->trail_x[h->trail_i] = (int16_t)px(h->x);
        h->trail_y[h->trail_i] = (int16_t)px(h->y);
    }
    if (h->invul) h->invul--;
    hero_fire(w, p);
}

/* ---- the corgis' shots ------------------------------------------------------------------------------------------- */
static const int SHOT_HW[4] = {4, 3, 12, 4}, SHOT_HH[4] = {3, 3, 2, 4};

static void pop_enemy(world *w, enemy *e, int p)
{
    hero *h = &w->h[p];
    int pts = K_POINTS[e->kind];
    h->score += pts;
    h->pops++;
    w->events[p] |= EV_POP;
    fx(w, FX_POP, px(e->x), px(e->y), e->kind);
    fx(w, FX_POINTS, px(e->x), px(e->y) - 10, pts);
    if (e->kind == EK_CARRIER) capsule_drop(w, e->x, e->y);
    if (e->group) {
        int g = e->group - 1;
        if (w->group_left[g] > 0 && --w->group_left[g] == 0) {
            h->score += GROUP_BONUS;
            w->events[p] |= EV_GROUP;
            capsule_drop(w, e->x, e->y);
            fx(w, FX_POINTS, px(e->x), px(e->y) - 20, GROUP_BONUS);
        }
    }
    e->on = 0;
}

/* the nearest robot (or the boss) ahead of a bone, for its homing: its point (0: none) */
static int bone_target(const world *w, const shot *s, int32_t *tx, int32_t *ty)
{
    int best = 1 << 30, found = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const enemy *e = &w->en[i];
        if (!e->on || (e->t <= 0 && e->pat != PAT_TURRET)) continue;
        int ex = px(e->x);
        if (ex < 0 || ex > SHOT_RANGE - 8 || e->x < s->x - q16(16)) continue;
        int dx = px(e->x - s->x), dy = px(e->y - s->y), d = dx * dx + dy * dy;
        if (d < best) { best = d; *tx = e->x; *ty = e->y; found = 1; }
    }
    if (w->b.on && w->b.phase == BP_FIGHT) {
        int dx = px(w->b.x - s->x), dy = px(w->b.y - s->y), d = dx * dx + dy * dy;
        if (d < best) { *tx = w->b.x; *ty = w->b.y; found = 1; }
    }
    return found;
}

static void boss_damage(world *w, int p, int dmg);

static void shots_step(world *w)
{
    for (int i = 0; i < MAX_PLAYERS * SHOTS_PER_PLAYER; i++) {
        shot *s = &w->shots[i];
        if (!s->on) continue;
        if (s->kind == SK_BONE) {                     /* turn toward the nearest robot ahead */
            int32_t tx, ty;
            if (bone_target(w, s, &tx, &ty)) {
                int64_t cross = (int64_t)s->vx * px(ty - s->y) - (int64_t)s->vy * px(tx - s->x);
                int a = cross > 0 ? BONE_TURN : cross < 0 ? -BONE_TURN : 0;
                rotate(&s->vx, &s->vy, a);
                s->angle = (int16_t)(s->angle + a);
            }
            if (--s->life <= 0) { s->on = 0; continue; }
        }
        s->x += s->vx;
        s->y += s->vy;
        int x = px(s->x), y = px(s->y);
        if (x > SHOT_RANGE || x < -16 || y < -8 || y > STAGE_H + 8) {
            if (x > SHOT_RANGE) fx(w, FX_FIZZLE, SHOT_RANGE, y, 0);
            s->on = 0;
            continue;
        }
        if (wall_hit(w, x, y, x, y)) {
            fx(w, FX_FIZZLE, x, y, 0);
            s->on = 0;
            continue;
        }
        int hw = SHOT_HW[s->kind], hh = SHOT_HH[s->kind];
        for (int k = 0; k < MAX_ENEMIES && s->on; k++) {
            enemy *e = &w->en[k];
            if (!e->on || (e->t < 0 && e->pat != PAT_TURRET)) continue;
            if (iabs(px(e->x) - x) >= K_HW[e->kind] + hw || iabs(px(e->y) - y) >= K_HH[e->kind] + hh) continue;
            if (s->kind == SK_BEAM) {                 /* the beam pierces: each robot once */
                if (s->hit >> k & 1) continue;
                s->hit |= 1u << k;
            } else {
                s->on = 0;
            }
            int dmg = s->damage;
            e->flash = 6;
            if (e->hp > dmg) e->hp = (uint8_t)(e->hp - dmg);
            else pop_enemy(w, e, s->owner);
        }
        boss *b = &w->b;
        if (s->on && b->on && b->phase == BP_FIGHT && iabs(px(b->x) - x) < B_HW[b->kind] + hw &&
            iabs(px(b->y) - y) < B_HH[b->kind] + hh) {
            if (s->kind != SK_BEAM || !(s->hit >> 31 & 1)) {
                if (s->kind == SK_BEAM) s->hit |= 1u << 31;
                else s->on = 0;
                boss_damage(w, s->owner, s->damage);
            }
        }
    }
}

/* ---- the bosses ---------------------------------------------------------------------------------------------------- */
static void boss_spawn(world *w)
{
    static const int hp[4] = BOSS_HP;
    boss *b = &w->b;
    memset(b, 0, sizeof *b);
    b->on = 1;
    b->kind = world_theme(w->sec);
    b->hp0 = hp[b->kind] * (100 + BOSS_HP_PLAYER * (w->players - 1)) / 100 * (100 + BOSS_HP_LOOP * w->loop) / 100;
    b->hp = b->hp0;
    b->phase = BP_ENTER;
    b->x = q16(SPAWN_R - 40);
    b->y = q16(STAGE_H / 2);
    b->fire_t = 60;
    b->dir = 1;
    w->wevents |= WEV_BOSS_IN;
}

static void boss_damage(world *w, int p, int dmg)
{
    static const int pts[4] = BOSS_POINTS;
    boss *b = &w->b;
    hero *h = &w->h[p];
    b->flash = 6;
    h->score += BOSS_HIT_POINTS;
    w->events[p] |= EV_BOSS_HIT;
    b->hp -= dmg;
    if (b->hp <= 0) {
        b->hp = 0;
        b->phase = BP_DOWN;
        b->t = 0;
        int bonus = pts[b->kind] + 1000 * w->loop;
        h->score += bonus;
        w->events[p] |= EV_BOSS_DOWN;
        fx(w, FX_POINTS, px(b->x), px(b->y) - 30, bonus);
        for (int i = 0; i < MAX_EBULLETS; i++) w->eb[i].on = 0;      /* its shots pop with it */
    }
}

static void boss_step(world *w)
{
    boss *b = &w->b;
    if (!b->on) return;
    b->t++;
    if (b->flash) b->flash--;
    int angry = b->hp * 2 < b->hp0;
    int32_t spd = eb_speed(w);
    switch (b->phase) {
    case BP_ENTER: {
        int k = b->t >= BOSS_ENTER ? BOSS_ENTER : b->t, d = BOSS_ENTER - k;
        b->x = q16(BOSS_X) + (int32_t)((int64_t)q16(SPAWN_R - 40 - BOSS_X) * d * d / (BOSS_ENTER * BOSS_ENTER));
        if (b->t >= BOSS_ENTER) { b->phase = BP_FIGHT; b->t = 0; }
        return;
    }
    case BP_DOWN:
        b->y += Q16_ONE / 4;
        if (b->t % 10 == 0) fx(w, FX_SPARK, px(b->x) - 24 + (b->t * 37) % 48, px(b->y) - 16 + (b->t * 23) % 32, 0);
        if (b->t >= BOSS_DOWN_FRAMES) {
            fx(w, FX_BIGPOP, px(b->x), px(b->y), 1);
            b->on = 0;
        }
        return;
    case BP_LEAVE:
        b->x += q16(3);
        b->y -= Q16_ONE / 2;
        if (px(b->x) > FIELD_R) b->on = 0;
        return;
    }
    /* BP_FIGHT */
    if (b->t >= BOSS_TIME) {                          /* it gives up and flies away (no points) */
        b->phase = BP_LEAVE;
        b->t = 0;
        w->boss_away = 1;
        w->wevents |= WEV_BOSS_AWAY;
        return;
    }
    int t = b->t;
    switch (b->kind) {
    case BK_LADYBIRD:                                 /* sways up and down; fans of 3 (5 when angry) */
        b->y = q16(STAGE_H / 2) + sinq(56, t);
        b->x = q16(BOSS_X) + sinq(8, t * 2);
        if (--b->fire_t <= 0) {
            b->fire_t = reload(w, angry ? 76 : 100);
            eb_fan(w, EB_PELLET, b->x - q16(20), b->y, angry ? 5 : 3, 14, spd);
            w->wevents |= WEV_BOSS_FIRE;
        }
        break;
    case BK_CRAB: {                                   /* follows a corgi's line; claw shots; drones from its mouth */
        int p = nearest_hero(w, b->x, b->y, NULL);
        if (p >= 0) {
            int32_t ty = clampi(w->h[p].y, q16(56), q16(184)), step = angry ? Q16_ONE * 3 / 4 : Q16_ONE / 2;
            b->y += b->y < ty - step ? step : b->y > ty + step ? -step : 0;
        }
        if (--b->fire_t <= 0) {
            b->fire_t = reload(w, angry ? 66 : 90);
            b->volley++;
            if (b->volley % 4 == 0) {
                for (int k = 0; k < 2; k++) {
                    enemy *e = spawn_robot(w, EK_DRONE, PAT_LINE, px(b->y) + (k ? 14 : -14), 0, 0, 0);
                    if (e) {
                        e->t = 0;
                        e->x = b->x - q16(24);
                        e->y = b->y + q16(k ? 14 : -14);
                    }
                }
            } else {
                eb_fan(w, EB_PELLET, b->x - q16(22), b->y - q16(16), 1, 0, spd);
                eb_fan(w, EB_PELLET, b->x - q16(22), b->y + q16(16), 1, 0, spd);
            }
            w->wevents |= WEV_BOSS_FIRE;
        }
        break;
    }
    case BK_OWL:                                      /* a figure of eight; rings of pellets, turning */
        b->x = q16(BOSS_X) + sinq(20, t);
        b->y = q16(STAGE_H / 2) + sinq(36, t * 2);
        if (--b->fire_t <= 0) {
            int n = angry ? 12 : 8;
            b->fire_t = reload(w, angry ? 96 : 120);
            b->angle += 11;
            for (int i = 0; i < n; i++) {
                int32_t vx = spd * 4 / 5, vy = 0;
                rotate(&vx, &vy, b->angle + i * 256 / n);
                eb_fire(w, EB_PELLET, b->x, b->y, vx, vy);
            }
            w->wevents |= WEV_BOSS_FIRE;
        }
        if (angry && t % 48 == 24) eb_fan(w, EB_PELLET, b->x - q16(12), b->y, 1, 0, spd);
        break;
    case BK_WALRUS: {                                 /* slides up and down; icicles in threes; a big snowball */
        int32_t step = angry ? Q16_ONE * 3 / 2 : Q16_ONE;
        b->y += b->dir * step;
        if (b->y < q16(56)) b->dir = 1;
        if (b->y > q16(184)) b->dir = -1;
        if (--b->fire_t <= 0) {
            b->fire_t = reload(w, angry ? 54 : 72);
            b->volley = 3;
        }
        if (b->volley && b->fire_t % 8 == 0) {
            b->volley--;
            eb_fire(w, EB_ICICLE, b->x - q16(26), b->y + q16(6), -(spd + Q16_ONE * 3 / 4), 0);
            w->wevents |= WEV_BOSS_FIRE;
        }
        if (t % reload(w, 160) == 80) eb_fan(w, EB_BIG, b->x - q16(20), b->y - q16(8), 1, 0, Q16_ONE);
        break;
    }
    }
}

/* ---- the sections -------------------------------------------------------------------------------------------------- */
static int robots_left(const world *w)
{
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) n += w->en[i].on;
    return n;
}

static void section_begin(world *w, int sec, int loop)
{
    w->sec = sec % SECTIONS;
    w->loop = loop;
    w->scroll_v = SCROLL_SPEED + SCROLL_LOOP * loop;
    if (w->scroll_v > SCROLL_MAX) w->scroll_v = SCROLL_MAX;
    w->sec_dist = 0;
    w->waves_done = 0;
    w->phase = w->started ? PH_INTRO : PH_READY;
    w->phase_t = 0;
    w->boss_away = 0;
}

/* tests and screenshots: the walls made again from the left of the stage, for section sec: from the far right as
 * the section begins (j0 = the column there), or none (an arena: j0 far behind) */
static void ter_restart(world *w, int sec, int j0)
{
    int c = fdiv8(px(w->scroll) + FIELD_L) - 2;
    w->ter_sec = sec;
    w->ter_col0 = j0;
    w->ter_next_col0 = -1;
    w->ter_col = c;
    w->ter_run = 0;
    memset(w->ter_ceil, 0, sizeof w->ter_ceil);
    memset(w->ter_floor, 0, sizeof w->ter_floor);
    ter_ahead(w);
}

void world_start_section(world *w, int sec, int loop)
{
    section_begin(w, sec, loop);
    ter_restart(w, w->sec, fdiv8(px(w->scroll) + SPAWN_R));
}

void world_to_boss(world *w)
{
    w->waves_done = 0xffffffffu;
    w->sec_dist = q16(SECTION_LEN);
    ter_restart(w, w->sec, fdiv8(px(w->scroll)) - 1000);
    boss_spawn(w);
    w->phase = PH_BOSS;
    w->phase_t = 0;
}

static void section_step(world *w)
{
    w->phase_t++;
    if (w->phase == PH_INTRO || w->phase == PH_WAVES) {
        w->sec_dist += w->scroll_v;
        const wave_def *d = WAVES[w->sec];
        for (int i = 0; i < WAVES_N[w->sec]; i++)
            if (!(w->waves_done >> i & 1) && w->sec_dist >= q16(d[i].at)) {
                w->waves_done |= 1u << i;
                send_wave(w, &d[i]);
            }
        if (w->phase == PH_INTRO && w->phase_t >= INTRO_FRAMES) { w->phase = PH_WAVES; w->phase_t = 0; }
        if (w->sec_dist >= q16(SECTION_LEN) &&
            (!robots_left(w) || w->sec_dist >= q16(SECTION_LEN + SECTION_MAX_WAIT))) {
            w->phase = PH_WARN;
            w->phase_t = 0;
            w->wevents |= WEV_WARN;
        }
    } else if (w->phase == PH_WARN) {
        if (w->phase_t >= WARN_FRAMES) {
            boss_spawn(w);
            w->phase = PH_BOSS;
            w->phase_t = 0;
        }
    } else if (w->phase == PH_BOSS) {
        if (!w->b.on) {
            w->phase = PH_CLEAR;
            w->phase_t = 0;
            w->wevents |= WEV_CLEAR;
            /* the next section's walls start at the far right as it begins */
            int c = fdiv8(px(w->scroll) + SPAWN_R + (int)((int64_t)CLEAR_FRAMES * w->scroll_v >> 16));
            w->ter_next_col0 = c > w->ter_col ? c : w->ter_col;
        }
    } else if (w->phase == PH_CLEAR) {
        if (w->phase_t >= CLEAR_FRAMES) {
            int sec = w->sec + 1, loop = w->loop;
            if (sec >= SECTIONS) {
                sec = 0;
                loop++;
                w->wevents |= WEV_LOOP;
            }
            section_begin(w, sec, loop);
            w->wevents |= WEV_SECTION;
        }
    }
}

/* ---- the world ------------------------------------------------------------------------------------------------------ */
void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    rs_rng_seed(&w->rng, seed ? seed : 1);
    rs_rng_seed(&w->ter_rng, (seed ^ 0x7e11a5u) | 1);
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    for (int p = 0; p < MAX_PLAYERS; p++) hero_reset(w, p);
    section_begin(w, 0, 0);
    ter_restart(w, 0, 0);
}

int world_running(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->h[p].state == SH_PLAY || w->h[p].state == SH_READY) return 1;
    return 0;
}

int world_all_out(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->h[p].state != SH_OUT) return 0;
    return 1;
}

static void start_run(world *w)
{
    w->started = 1;
    for (int p = 0; p < w->players; p++)
        if (w->h[p].state == SH_READY) {
            w->h[p].state = SH_PLAY;
            w->h[p].t = 0;
            w->events[p] |= EV_START;
        }
    if (w->phase == PH_READY) {
        w->phase = PH_INTRO;
        w->phase_t = 0;
        w->wevents |= WEV_SECTION;
    }
}

static void collide_heroes(world *w)
{
    for (int p = 0; p < w->players; p++) {
        hero *h = &w->h[p];
        if (h->state != SH_PLAY) continue;
        int x = px(h->x), y = px(h->y);
        for (int i = 0; i < MAX_CAPS; i++) {          /* capsules: anyone who touches one */
            capsule *c = &w->caps[i];
            if (c->on && iabs(px(c->x) - x) < 14 && iabs(px(c->y) - y) < 13) {
                c->on = 0;
                apply_capsule(w, p, c->kind);
            }
        }
        if (h->invul) continue;
        for (int i = 0; i < MAX_EBULLETS; i++) {
            ebullet *b = &w->eb[i];
            if (!b->on) continue;
            int bw = b->kind == EB_ICICLE ? 6 : b->kind == EB_BIG ? 4 : 2, bh = b->kind == EB_BIG ? 4 : 2;
            if (iabs(px(b->x) - x) < HIT_HW + bw && iabs(px(b->y) - y) < HIT_HH + bh) {
                b->on = 0;
                hurt(w, p);
                break;
            }
        }
        if (h->state != SH_PLAY || h->invul) continue;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            enemy *e = &w->en[i];
            if (!e->on || (e->t < 0 && e->pat != PAT_TURRET)) continue;
            if (iabs(px(e->x) - x) < HIT_HW + K_HW[e->kind] - 1 && iabs(px(e->y) - y) < HIT_HH + K_HH[e->kind] - 1) {
                hurt(w, p);
                break;
            }
        }
        if (h->state != SH_PLAY || h->invul) continue;
        boss *b = &w->b;
        if (b->on && (b->phase == BP_FIGHT || b->phase == BP_ENTER) &&
            iabs(px(b->x) - x) < HIT_HW + B_HW[b->kind] - 4 && iabs(px(b->y) - y) < HIT_HH + B_HH[b->kind] - 4)
            hurt(w, p);
        if (h->state != SH_PLAY || h->invul) continue;
        if (wall_hit(w, x - WALL_HW, y - WALL_HH, x + WALL_HW, y + WALL_HH)) hurt(w, p);
    }
}

void world_step(world *w, const pin in[MAX_PLAYERS])
{
    for (int p = 0; p < MAX_PLAYERS; p++) w->events[p] = 0;
    w->wevents = 0;
    if (w->freeze > 0) {                              /* the hit-stop */
        w->freeze--;
        return;
    }
    if (!w->started) {
        for (int p = 0; p < w->players; p++)
            if (in[p].dx || in[p].dy || in[p].any || in[p].touch) start_run(w);
        if (!w->started) {
            w->t++;
            return;
        }
    }
    w->scroll += w->scroll_v;
    ter_ahead(w);
    section_step(w);
    for (int p = 0; p < w->players; p++) hero_step(w, p, &in[p]);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemy *e = &w->en[i];
        if (!e->on) continue;
        if (e->flash) e->flash--;
        enemy_move(w, e);
        int x = px(e->x), y = px(e->y);
        if ((e->t > 0 || e->pat == PAT_TURRET) && (x < FIELD_L || x > FIELD_R || y < -48 || y > STAGE_H + 48)) {
            if (e->group && w->group_left[e->group - 1] > 0) w->group_left[e->group - 1] = -1;   /* one got away */
            e->on = 0;
            continue;
        }
        enemy_fire(w, e);
    }
    boss_step(w);
    shots_step(w);
    for (int i = 0; i < MAX_EBULLETS; i++) {
        ebullet *b = &w->eb[i];
        if (!b->on) continue;
        b->x += b->vx;
        b->y += b->vy;
        int x = px(b->x), y = px(b->y);
        if (x < FIELD_L || x > FIELD_R || y < -16 || y > STAGE_H + 16) b->on = 0;
        else if (x > -8 && x < STAGE_W + 8 && wall_hit(w, x, y, x, y)) {
            fx(w, FX_FIZZLE, x, y, 1);
            b->on = 0;
        }
    }
    collide_heroes(w);
    for (int i = 0; i < MAX_CAPS; i++) {
        capsule *c = &w->caps[i];
        if (!c->on) continue;
        c->x -= CAPSULE_SPEED;
        c->t++;
        if (px(c->x) < FIELD_L) c->on = 0;
    }
    for (int i = 0; i < FX_MAX; i++) {
        effect *e = &w->fxs[i];
        if (e->on && ++e->t >= fx_life(e->kind)) e->on = 0;
    }
    if (w->msg_t && --w->msg_t == 0) w->msg = MSG_NONE;
    w->t++;
}

/* ---- the bot ---------------------------------------------------------------------------------------------------------
 * It looks ahead: for each of the 9 directions held for the next frames it forecasts every shot and robot along its
 * last move (a dasher winding up: its dash), the boss and the walls, and adds up the danger (the sooner, the worse);
 * of the safest it picks the one nearest its aim: a capsule, else the line of the robot ahead (or the boss) to shoot
 * it, near its home column. A human-like bot (skill 1..10) decides only every few frames, looks less far, misses some
 * shots and misjudges its aim. 2-4 bots keep their own lanes. */
static uint32_t hash3(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9e3779b1u ^ b * 0x85ebca77u ^ c * 0xc2b2ae3du;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return h;
}

pin bot_decide(world *w, int p, int skill)
{
    pin out = {0, 0, 0, 0, 0, 1};
    hero *h = &w->h[p];
    if (h->state != SH_PLAY && h->state != SH_READY) return out;
    if (skill > 0) {
        if (h->bot_t > 0) {
            h->bot_t--;
            out.dx = h->bot_dx;
            out.dy = h->bot_dy;
            return out;
        }
        h->bot_t = skill / 2;
    }
    int H = skill ? 28 - skill * 2 : 32, margin = skill ? 0 : 3;
    int32_t sp = ROCKET_SPEED + SPEED_STEP * h->speed;
    /* the aim: 2-4 bots keep their own lanes (a home column and a line offset each) */
    int lane = w->players > 1 ? p * 2 - (w->players - 1) : 0;          /* -3, -1, 1, 3 (4 players) */
    int home = 52 + p * 18, tx = home, ty = 120 + lane * 14;
    int best_d = 1 << 30;
    for (int i = 0; i < MAX_CAPS; i++) {
        const capsule *c = &w->caps[i];
        if (!c->on || px(c->x) < 20 || px(c->x) > 300) continue;
        int d = iabs(px(c->x) - px(h->x)) + iabs(px(c->y) - px(h->y)), nearest = 1;
        for (int q = 0; q < w->players; q++)               /* the nearest corgi goes for it, the others carry on */
            if (q != p && w->h[q].state == SH_PLAY &&
                iabs(px(c->x) - px(w->h[q].x)) + iabs(px(c->y) - px(w->h[q].y)) < d) nearest = 0;
        if (nearest && d < best_d) { best_d = d; tx = clampi(px(c->x) - 4, 20, 260); ty = px(c->y); }
    }
    if (best_d == 1 << 30) {
        if (w->b.on && w->b.phase == BP_FIGHT) {
            ty = px(w->b.y) + lane * 6;
        } else {
            int bx = 1 << 30;
            for (int i = 0; i < MAX_ENEMIES; i++) {
                const enemy *e = &w->en[i];
                if (!e->on || e->t < -20 || e->pat == PAT_TURRET) continue;    /* (not the turrets: by the walls) */
                int ex = px(e->x) + px(e->vx) * 20;
                if (ex < px(h->x) + 24 || ex > 330) continue;
                int d = ex - px(h->x) + iabs(px(e->y) - px(h->y)) / 2 + ((i + p) % w->players) * 40 * (w->players > 1);
                if (d < bx) { bx = d; ty = px(e->y) + px(e->vy) * 16; }
            }
        }
        if (skill) {
            int err = skill * 3;
            ty += (int)(hash3((uint32_t)w->t / 64, (uint32_t)p, 7) % (uint32_t)(2 * err + 1)) - err;
        }
    }
    ty = clampi(ty, ROCKET_MIN_Y + 6, ROCKET_MAX_Y - 6);
    int best = 1 << 30, bdx = 0, bdy = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int32_t s = dx && dy ? sp * DIAG_K / 256 : sp;
            int danger = 0, fx0 = 0, fy0 = 0;
            for (int k = 2; k <= H; k += 2) {
                int x = px(clampi(h->x + dx * s * k, q16(ROCKET_MIN_X), q16(ROCKET_MAX_X)));
                int y = px(clampi(h->y + dy * s * k, q16(ROCKET_MIN_Y), q16(ROCKET_MAX_Y)));
                fx0 = x;
                fy0 = y;
                int wgt = H + 8 - k;
                for (int i = 0; i < MAX_EBULLETS; i++) {
                    const ebullet *b = &w->eb[i];
                    if (!b->on) continue;
                    if (skill && hash3((uint32_t)i, (uint32_t)w->t / 24, (uint32_t)p) % 100 < (uint32_t)skill * 7) continue;
                    int bx = px(b->x + b->vx * k), by = px(b->y + b->vy * k);
                    int bw = b->kind == EB_ICICLE ? 6 : b->kind == EB_BIG ? 4 : 2, bh = b->kind == EB_BIG ? 4 : 2;
                    if (iabs(bx - x) < HIT_HW + bw + margin + 1 && iabs(by - y) < HIT_HH + bh + margin + 1) danger += wgt * 16;
                }
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    const enemy *e = &w->en[i];
                    if (!e->on) continue;
                    int32_t ex = e->x + e->vx * k, ey = e->y + e->vy * k;
                    if (e->pat == PAT_DASH && e->aux == 1) {
                        int left = DASH_HOLD - (e->t - e->aux_t), go = k - left;
                        ex = e->x - (go > 0 ? DASH_SPEED * go : 0);
                        ey = e->y;
                    }
                    if (iabs(px(ex) - x) < HIT_HW + K_HW[e->kind] + margin + 2 &&
                        iabs(px(ey) - y) < HIT_HH + K_HH[e->kind] + margin + 2) danger += wgt * 16;
                }
                const boss *bo = &w->b;
                if (bo->on && bo->phase != BP_DOWN && iabs(px(bo->x) - x) < B_HW[bo->kind] + HIT_HW + 8 &&
                    iabs(px(bo->y) - y) < B_HH[bo->kind] + HIT_HH + 8) danger += wgt * 16;
                if (wall_hit_at(w, x - WALL_HW - 2, y - WALL_HH - 2, x + WALL_HW + 2, y + WALL_HH + 2,
                                (int)((int64_t)w->scroll_v * k >> 16))) danger += wgt * 24;
            }
            int cost = danger * 64 + iabs(fy0 - ty) * 3 + iabs(fx0 - tx);
            for (int q = 0; q < w->players; q++)                       /* not on top of another corgi */
                if (q != p && w->h[q].state == SH_PLAY && iabs(px(w->h[q].x) - fx0) < 20 && iabs(px(w->h[q].y) - fy0) < 14)
                    cost += 60;
            if (fx0 <= ROCKET_MIN_X + 2 || fx0 >= ROCKET_MAX_X - 40) cost += 40;
            if (fy0 <= ROCKET_MIN_Y + 4 || fy0 >= ROCKET_MAX_Y - 4) cost += 30;
            if (dx == h->bot_dx && dy == h->bot_dy) cost -= 2;
            if (cost < best) { best = cost; bdx = dx; bdy = dy; }
        }
    h->bot_dx = bdx;
    h->bot_dy = bdy;
    out.dx = bdx;
    out.dy = bdy;
    return out;
}
