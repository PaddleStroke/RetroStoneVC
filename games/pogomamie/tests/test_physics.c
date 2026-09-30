/*
 * Pogo Mamie: unit tests of the game rules against the tuning table (physics.c, world.c, gen.c; no video or
 * sound): the bounce arcs, the air control, the roofs' shapes, the collisions, the generator (every gap
 * reachable in the worst case, over thousands of seeds; no unfair spawn; the difficulty ramp) and determinism.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 *
 *   test_physics [--seeds N] [--metres M]
 */
#include "pm.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))

static double px(int32_t q) { return q / 65536.0; }

/* ---- a flat test terrain: a floor at FLOOR everywhere, an optional wall block [wall_x, +inf) rising to wall_top -- */
#define FLOOR 400
static int wall_x = 1 << 30, wall_top = 0;
static int flat_land(const void *c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, hit *h)
{
    (void)c; (void)x0;
    int fx = (int)(x1 >> 16), s = fx + FOOT_HALF >= wall_x ? wall_top : FLOOR;
    if (y1 >= (int32_t)s * Q16_ONE && y0 <= (int32_t)(s + STEP_UP) * Q16_ONE) {
        memset(h, 0, sizeof *h);
        h->kind = SF_ROOF;
        h->y = s;
        return 1;
    }
    return 0;
}
static int flat_solid(const void *c, int x, int feet) { (void)c; return x >= wall_x && wall_top < feet - STEP_UP; }
static const terrain flat = {flat_land, flat_solid, 0, 0};

typedef struct { double apex; int apex_t, air_t; double range; } arc;

/* one bounce from the floor with a given control (dir) and start speed */
static arc bounce_arc(int kind, int dir, int32_t vx0)
{
    mamie m;
    memset(&m, 0, sizeof m);
    m.state = MS_AIR;
    m.x = 100 * Q16_ONE;
    m.y = FLOOR * Q16_ONE;
    m.vx = vx0;
    m.vy = -bounce_speed(kind);
    arc a = {0, 0, 0, 0};
    int32_t top = m.y;
    for (int t = 1; t < 1000; t++) {
        hit h;
        int e = mamie_air_step(&m, &flat, dir, &h);
        if (m.y < top) { top = m.y; a.apex_t = t; }
        if (e & EV_LAND) { a.air_t = t; break; }
    }
    a.apex = FLOOR - px(top);
    a.range = px(m.x) - 100;
    return a;
}

static void test_references(void)
{
    printf("the references and the conversion\n");
    double f1 = REF1_BOUNCE * REF1_BOUNCE / (2 * REF1_GRAVITY) / REF1_H, f2 = REF2_BOUNCE * REF2_BOUNCE / (2 * REF2_GRAVITY) / REF2_H;
    printf("  apex / screen height: ref 1 %.3f (%.1f frames), ref 2 %.3f (%.1f frames), ref 3 apex after %.1f frames\n",
           f1, REF1_BOUNCE / REF1_GRAVITY, f2, REF2_BOUNCE / REF2_GRAVITY, REF3_T_APEX);
    CHECK(NEAR(REF_APEX_FRAC, 0.314, 0.003), "mean apex share %.3f", REF_APEX_FRAC);
    CHECK(NEAR(APEX_NORMAL, REF_APEX_FRAC * SCREEN_PLAY_H, 0.6), "apex %d px = %.1f", APEX_NORMAL, REF_APEX_FRAC * 240);
    CHECK(NEAR(T_APEX, REF_T_APEX, 0.5), "time to apex %d vs %.1f frames", T_APEX, REF_T_APEX);
    CHECK(NEAR(APEX_SPRING, REF_SPRING_FRAC * SCREEN_PLAY_H, 1.0), "spring apex %d = %.1f", APEX_SPRING,
          REF_SPRING_FRAC * 240);
    double vx1 = REF1_VX * SCREEN_PLAY_H / REF1_H, vx2 = REF2_VX * SCREEN_PLAY_H / REF2_H;
    CHECK(NEAR(px(VX_MAX), (vx1 + vx2) / 2, 0.05), "top speed %.3f vs the refs' mean %.3f (%.2f, %.2f)", px(VX_MAX),
          (vx1 + vx2) / 2, vx1, vx2);
    CHECK(NEAR(px(ACCEL_X), REF1_DRAG * SCREEN_PLAY_H / REF1_H, 0.02), "inertia %.3f vs ref 1's drag %.3f", px(ACCEL_X),
          REF1_DRAG * 240 / REF1_H);
    /* the speeds are the arcs' */
    CHECK(NEAR(px(V_BIG) * px(V_BIG) / (2 * px(GRAVITY)), APEX_BIG, 0.3), "V_BIG gives %.2f px",
          px(V_BIG) * px(V_BIG) / (2 * px(GRAVITY)));
    CHECK(NEAR(px(V_SPRING) * px(V_SPRING) / (2 * px(GRAVITY)), APEX_SPRING, 0.3), "V_SPRING");
    CHECK(NEAR(px(V_SLING) * px(V_SLING) / (2 * px(GRAVITY)), APEX_SLING, 0.3), "V_SLING");
}

static void test_arcs(void)
{
    printf("the bounce arcs (the game's fixed-point steps)\n");
    arc n = bounce_arc(BN_NORMAL, 0, 0), b = bounce_arc(BN_BIG, 0, 0), s = bounce_arc(BN_SPRING, 0, 0);
    printf("  normal: apex %.2f px after %d frames, %d frames in the air (%.2f s)\n", n.apex, n.apex_t, n.air_t, n.air_t / 60.0);
    printf("  big:    apex %.2f px after %d frames, %d frames in the air\n", b.apex, b.apex_t, b.air_t);
    printf("  spring: apex %.2f px after %d frames, %d frames in the air\n", s.apex, s.apex_t, s.air_t);
    /* the discrete arc (speed first, then position) peaks half a step under the continuous one (v^2/2g - v/2) */
    CHECK(NEAR(n.apex, APEX_NORMAL - px(V_NORMAL) / 2, 1.0), "normal apex %.2f", n.apex);
    CHECK(iabs(n.apex_t - T_APEX) <= 1, "normal apex after %d frames", n.apex_t);
    CHECK(iabs(n.air_t - 2 * T_APEX) <= 2, "normal air time %d", n.air_t);
    CHECK(NEAR(b.apex, APEX_BIG - px(V_BIG) / 2, 1.0), "big apex %.2f", b.apex);
    CHECK(b.apex / n.apex > 1.5 && b.air_t > n.air_t * 5 / 4, "big: higher and longer");
    CHECK(NEAR(s.apex, APEX_SPRING - px(V_SPRING) / 2, 1.5), "spring apex %.2f", s.apex);
    /* the references' arcs, scaled to our screen: ours sits between them */
    double r1 = REF1_BOUNCE * REF1_BOUNCE / (2 * REF1_GRAVITY) / REF1_H * 240, r2 = REF2_BOUNCE * REF2_BOUNCE / (2 * REF2_GRAVITY) / REF2_H * 240;
    CHECK(n.apex > r2 && n.apex < r1 + 2, "apex %.1f between the refs %.1f and %.1f", n.apex, r2, r1);
    CHECK(n.apex_t >= 30 && n.apex_t <= 41, "apex time within the refs (30..41 frames)");
    /* a landing always sets the same bounce, whatever the fall before */
    mamie m;
    memset(&m, 0, sizeof m);
    hit h = {SF_ROOF, 0, 0, FLOOR, 0};
    m.vy = Q16(7.5);
    mamie_bounce(&m, &h, 0);
    CHECK(m.vy == -V_NORMAL, "a bounce sets the speed");
    mamie_bounce(&m, &h, 1);
    CHECK(m.vy == -V_BIG && m.bounce == BN_BIG, "holding A: the big bounce");
    h.kind = SF_AWNING;
    mamie_bounce(&m, &h, 0);
    CHECK(m.vy == -V_SPRING, "an awning springs");
}

static void test_air_control(void)
{
    printf("air control\n");
    mamie m;
    memset(&m, 0, sizeof m);
    int t = 0;
    while (m.vx < VX_MAX && t < 100) { m.vx = vx_after_input(&m, 1); t++; }
    CHECK(t == 15, "0 to top speed in %d frames (15)", t);
    for (int i = 0; i < 100; i++) m.vx = vx_after_input(&m, 1);
    CHECK(m.vx == VX_MAX, "holding Right never goes past the top speed");
    t = 0;
    while (m.vx > 0 && t < 100) { m.vx = vx_after_input(&m, -1); t++; }
    CHECK(t == 10, "braking from the top speed to a stop: %d frames (10)", t);
    for (int i = 0; i < 100; i++) m.vx = vx_after_input(&m, -1);
    CHECK(m.vx == VX_BACK, "Left: down to the backward drift");
    for (int i = 0; i < 200; i++) m.vx = vx_after_input(&m, 0);
    CHECK(m.vx == VX_CRUISE, "no input: back to the cruise speed");
    m.croissant_t = 10;
    for (int i = 0; i < 40; i++) m.vx = vx_after_input(&m, 1);
    CHECK(m.vx == VX_BOOST, "the croissant: a faster top speed");
    m.croissant_t = 0;
    for (int i = 0; i < 100; i++) m.vx = vx_after_input(&m, 1);
    CHECK(m.vx == VX_MAX, "... back to normal after it");
    /* ranges */
    arc a = bounce_arc(BN_NORMAL, 1, VX_MAX), s = bounce_arc(BN_NORMAL, 1, 0), bb = bounce_arc(BN_BIG, 1, 0);
    arc brake = bounce_arc(BN_NORMAL, -1, VX_MAX);
    printf("  one bounce, holding Right: %.1f px at top speed, %.1f px from a standstill (big: %.1f); braking: %.1f\n",
           a.range, s.range, bb.range, brake.range);
    CHECK(NEAR(a.range, px(VX_MAX) * a.air_t, 2), "range at top speed");
    CHECK(bb.range > s.range * 1.2, "the big bounce goes further");
    CHECK(brake.range < 20, "braking stops her within a bounce");
    /* the umbrella: a slow glide */
    memset(&m, 0, sizeof m);
    m.state = MS_AIR;
    m.y = 100 * Q16_ONE;
    m.umbrella_t = UMBRELLA_T;
    hit h;
    for (int i = 0; i < 120; i++) mamie_air_step(&m, &flat, 0, &h);
    CHECK(m.vy <= UMB_MAX_FALL, "umbrella: falls at %.2f px/frame at most", px(m.vy));
}

static void test_roofs(void)
{
    printf("roof shapes\n");
    bldg b;
    memset(&b, 0, sizeof b);
    for (int k = 0; k < BUMP_MAX; k++) b.bump_x[k] = -1;
    b.x0 = 800; b.x1 = 800 + 128; b.top = 300; b.roof = RF_MANSARD;
    CHECK(bldg_surface(&b, 800) == 315 && bldg_surface(&b, 815) == 300 && bldg_surface(&b, 816) == 300, "mansard left slope");
    CHECK(bldg_surface(&b, 927) == 315 && bldg_surface(&b, 912) == 300, "mansard right slope");
    CHECK(bldg_slope(&b, 805) == -2 && bldg_slope(&b, 920) == 2 && bldg_slope(&b, 860) == 0, "45-degree slopes push outwards");
    int mono = 1;
    for (int x = 800; x < 815; x++) mono &= bldg_surface(&b, x + 1) == bldg_surface(&b, x) - 1;
    CHECK(mono, "a 1-px step per px (45 degrees)");
    b.roof = RF_PITCH; b.x1 = 800 + 64;
    CHECK(bldg_surface(&b, 800) == 315 && bldg_surface(&b, 831) == 300 && bldg_surface(&b, 832) == 300 &&
          bldg_surface(&b, 863) == 315, "pitched: eaves 16 px below the ridge");
    CHECK(bldg_slope(&b, 810) == -1 && bldg_slope(&b, 850) == 1, "1:2 slopes");
    b.roof = RF_FLAT; b.x1 = 800 + 128; b.sky0 = 48; b.sky1 = 72;
    CHECK(bldg_surface(&b, 860) == 300, "an intact skylight holds");
    b.sky_broken = 1;
    CHECK(bldg_surface(&b, 860) == 300 + PIT_DEPTH && bldg_drawn_top(&b, 860) == 300, "broken: one floor down (the frame stays)");
    b.bump_x[0] = 16; b.bump_w[0] = 16; b.bump_h[0] = 24;
    CHECK(bump_top(&b, 0) == 276 && bldg_drawn_top(&b, 820) == 276, "a chimney 24 px tall");
}

/* a world holding two buildings (and optionally a prop), for collision cases */
static void two_bldgs(world *w, int gap, int top2)
{
    memset(w, 0, sizeof *w);
    w->nb = 2;
    for (int i = 0; i < 2; i++) for (int k = 0; k < BUMP_MAX; k++) w->b[i].bump_x[k] = -1;
    w->b[0].x0 = 0; w->b[0].x1 = 200; w->b[0].top = 320;
    w->b[1].x0 = 200 + gap; w->b[1].x1 = 200 + gap + 200; w->b[1].top = (int16_t)top2;
    w->players = 1;
    mamie_reset(&w->m[0], 100, 320);
    w->m[0].state = MS_AIR;
}

static void test_collisions(void)
{
    printf("collisions\n");
    world w;
    terrain T;
    hit h;
    /* a wall: a taller building next door, rushed at from low down */
    two_bldgs(&w, 0, 200);
    T = world_terrain(&w, 0);
    mamie *m = &w.m[0];
    m->x = 180 * Q16_ONE; m->y = 320 * Q16_ONE; m->vx = VX_MAX; m->vy = -Q16(1.0);
    int bonk = 0;
    for (int i = 0; i < 30 && !bonk; i++) bonk = mamie_air_step(m, &T, 1, &h) & EV_BONK;
    CHECK(bonk && m->vx == 0 && (m->x >> 16) + HALF_W < 200, "the wall stops her (x %d)", (int)(m->x >> 16));
    /* a step within STEP_UP is not a wall */
    two_bldgs(&w, 0, 316);
    T = world_terrain(&w, 0);
    m->x = 190 * Q16_ONE; m->y = 319 * Q16_ONE; m->vx = VX_MAX; m->vy = Q16(0.5);
    int e = mamie_air_step(m, &T, 1, &h);
    CHECK(!(e & EV_BONK), "a 4-px step is no wall");
    /* landing on the very edge: the foot overhangs by up to FOOT_HALF */
    two_bldgs(&w, 64, 320);
    T = world_terrain(&w, 0);
    m->x = (199 + FOOT_HALF) * Q16_ONE; m->y = 316 * Q16_ONE; m->vx = 0; m->vy = Q16(2.0);
    e = 0;
    for (int i = 0; i < 5 && !(e & EV_LAND); i++) e = mamie_air_step(m, &T, 0, &h);
    CHECK((e & EV_LAND) && h.idx == 0 && h.y == 320, "the foot catches the edge");
    m->x = (200 + FOOT_HALF) * Q16_ONE; m->y = 316 * Q16_ONE; m->vy = Q16(2.0);
    e = 0;
    for (int i = 0; i < 5; i++) e |= mamie_air_step(m, &T, 0, &h);
    CHECK(!(e & EV_LAND), "one pixel further: she falls");
    /* a prop is one-way: up through it, then land on it */
    two_bldgs(&w, 160, 320);
    w.no = 1;
    memset(&w.o[0], 0, sizeof w.o[0]);
    w.o[0].kind = OB_AWNING; w.o[0].x = 220; w.o[0].w = 80; w.o[0].y = 360;
    T = world_terrain(&w, 0);
    m->x = 205 * Q16_ONE; m->y = 380 * Q16_ONE; m->vx = VX_CRUISE; m->vy = -V_NORMAL;
    int landed = 0, up_through = 0;
    for (int i = 0; i < 200 && !landed; i++) {
        e = mamie_air_step(m, &T, 0, &h);
        if ((m->y >> 16) < 355) up_through = 1;
        if (e & EV_LAND) { landed = h.kind == SF_AWNING; if (!landed) printf("  (landed on kind %d at y %d x %d)\n", h.kind, h.y, (int)(m->x >> 16)); break; }
    }
    CHECK(up_through && landed, "up through the awning, then onto it");
    /* the whole world step: an antenna kills the speed, a pigeon knocks back or is bounced on */
    two_bldgs(&w, 64, 320);
    w.no = 1;
    memset(&w.o[0], 0, sizeof w.o[0]);
    w.o[0].kind = OB_ANTENNA; w.o[0].x = 120; w.o[0].y = 320;
    m->x = 110 * Q16_ONE; m->y = 310 * Q16_ONE; m->vx = VX_MAX; m->vy = 0;
    w.camx = 0;
    int dir[2] = {1, 0}, a[2] = {0, 0}, ap[2] = {0, 0};
    w.started = 1;
    int stumbled = 0;
    for (int i = 0; i < 20 && !stumbled; i++) { world_step(&w, dir, a, ap); stumbled = w.events[0] & EV_STUMBLE; }
    CHECK(stumbled, "a TV antenna: a stumble");
    two_bldgs(&w, 64, 320);
    w.no = 1;
    memset(&w.o[0], 0, sizeof w.o[0]);
    w.o[0].kind = OB_PIGEON; w.o[0].x = 0; w.o[0].y = 320; w.o[0].w = 200; w.o[0].a = 120; w.o[0].b = 120;
    w.o[0].pos = 120 * Q16_ONE; w.o[0].t = 100;         /* pecking: it stays put */
    m->x = 120 * Q16_ONE; m->y = 280 * Q16_ONE; m->vx = 0; m->vy = Q16(2.0);
    w.started = 1;
    int pig = 0;
    for (int i = 0; i < 40 && !pig; i++) { int d0[2] = {0, 0}; world_step(&w, d0, a, ap); pig = w.events[0] & EV_PIGEON; }
    CHECK(pig && w.m[0].stunts == PTS_PIGEON, "on a pigeon's head: a stunt (%d points)", w.m[0].stunts);
    two_bldgs(&w, 64, 320);
    w.no = 1;
    memset(&w.o[0], 0, sizeof w.o[0]);
    w.o[0].kind = OB_PIGEON; w.o[0].x = 0; w.o[0].y = 320; w.o[0].w = 200; w.o[0].a = 130; w.o[0].b = 130;
    w.o[0].pos = 130 * Q16_ONE; w.o[0].t = 100;
    m->x = 110 * Q16_ONE; m->y = 318 * Q16_ONE; m->vx = VX_MAX; m->vy = -Q16(0.5);
    w.started = 1;
    int knock = 0;
    for (int i = 0; i < 30 && !knock; i++) { world_step(&w, dir, a, ap); knock = w.events[0] & EV_KNOCK; }
    CHECK(knock && w.m[0].vx < 0, "into a pigeon from the side: knocked back");
    /* a skylight breaks: she drops into the attic */
    two_bldgs(&w, 64, 320);
    w.b[0].sky0 = 88; w.b[0].sky1 = 112;
    m->x = 100 * Q16_ONE; m->y = 300 * Q16_ONE; m->vx = 0; m->vy = Q16(2.0);
    w.started = 1;
    int glass = 0, pit = 0;
    for (int i = 0; i < 80; i++) {
        int d0[2] = {0, 0};
        world_step(&w, d0, a, ap);
        glass |= w.events[0] & EV_GLASS;
        if (glass && (w.events[0] & EV_LAND) && w.m[0].land_y == 320 + PIT_DEPTH) pit = 1;
    }
    CHECK(glass && w.b[0].sky_broken && pit, "the skylight breaks, the attic's floor catches her");
    /* doomed: in a gap, below both roofs */
    two_bldgs(&w, 96, 320);
    m->x = 240 * Q16_ONE; m->y = 330 * Q16_ONE; m->vy = Q16(1.0);
    CHECK(world_doomed(&w, m), "below both roofs in a gap: doomed");
    m->y = 318 * Q16_ONE;
    CHECK(!world_doomed(&w, m), "above them: not yet");
}

/* ---- the generator ------------------------------------------------------------------------------------------------- */
/* An independent worst-case check: from a standstill at a take-off point on building a, the real physics, holding
 * Right until past b's edge, then braking. */
static int reach_indep(const world *w, int ai, int bi, int takeoff, int big, int *land_x)
{
    const bldg *a = &w->b[ai], *b = &w->b[bi];
    mamie m;
    memset(&m, 0, sizeof m);
    m.state = MS_AIR;
    int tx = (int)a->x1 - takeoff;
    m.x = (int32_t)tx * Q16_ONE;
    m.y = (int32_t)bldg_surface(a, tx) * Q16_ONE;
    m.vy = -bounce_speed(big ? BN_BIG : BN_NORMAL);
    terrain T = world_terrain(w, 1);
    for (int t = 0; t < 600; t++) {
        int x = (int)(m.x >> 16);
        int dir = x < b->x0 + LAND_MARGIN ? 1 : m.vx > 0 ? -1 : 0;
        hit h;
        if (mamie_air_step(&m, &T, dir, &h) & EV_LAND) {
            *land_x = (int)(m.x >> 16);
            return h.idx == bi;
        }
    }
    return 0;
}

typedef struct { long gaps, gap_sum, pigeons, roofs, gusts, hazards, props, nogap; int gap_max; } band;

static int seeds = 3000, metres = 1100, long_seeds = 300, long_metres = 3300;

static void test_generator(void)
{
    printf("the generator: %d seeds x %d m (the first %d: %d m)\n", seeds, metres, long_seeds, long_metres);
    static world w;
    static band bands[8];
    memset(bands, 0, sizeof bands);
    long pairs = 0, bad_reach = 0, bad_shape = 0, bad_spawn = 0, need_big = 0, printed = 0;
    int land_x;
    for (int s = 0; s < seeds; s++) {
        memset(&w, 0, sizeof w);
        w.seed = (uint32_t)s * 7919u + 13u;
        gen_reset(&w);
        int32_t until = 0, last_checked = 0;
        int32_t checked_index = 0;
        int32_t end = (int32_t)(s < long_seeds ? long_metres : metres) * PX_PER_M;
        while (until < end) {
            until += 256;
            gen_ahead(&w, until);
            /* check each new building against the one before it */
            for (int i = 1; i < w.nb; i++) {
                const bldg *a = &w.b[i - 1], *b = &w.b[i];
                if (b->index <= checked_index) continue;
                checked_index = b->index;
                pairs++;
                int ok = 0, via_big = 1;
                for (int big = 0; big < 2 && !ok; big++)
                    for (int k = 0; k < 3 && !ok; k++)
                        if (reach_indep(&w, i - 1, i, gen_takeoffs[k], big, &land_x)) { ok = 1; via_big = big; }
                if (!ok) {
                    bad_reach++;
                    if (printed++ < 5)
                        printf("  FAIL seed %d: no way from building %d (%d..%d, top %d) to %d (%d..%d, top %d)\n", s,
                               (int)a->index, (int)a->x0, (int)a->x1, a->top, (int)b->index, (int)b->x0, (int)b->x1, b->top);
                }
                need_big += via_big;
                /* shapes */
                int wid = (int)(b->x1 - b->x0);
                if (b->x0 % 8 || b->x1 % 8 || b->top % 8 || wid < BLD_W_MIN || b->x0 < a->x1 ||
                    (b->kind != BK_BARGE && (b->top < ROOF_MIN_Y || b->top > ROOF_MAX_Y)) ||
                    (b->roof == RF_PITCH && wid % 32) || (b->roof == RF_MANSARD && wid < 64) ||
                    b->top > bldg_surface(a, (int)a->x1 - 1) + MAX_DROP) {
                    bad_shape++;
                    if (printed++ < 8)
                        printf("  FAIL seed %d: building %d kind %d roof %d at %d..%d top %d (after %d..%d)\n", s,
                               (int)b->index, b->kind, b->roof, (int)b->x0, (int)b->x1, b->top, (int)a->x0, (int)a->x1);
                }
                for (int k = 0; k < BUMP_MAX; k++) {
                    if (b->bump_x[k] < 0) continue;
                    int lo = b->roof == RF_MANSARD ? MANSARD_W : 0, hi = b->roof == RF_MANSARD ? wid - MANSARD_W : wid;
                    if (b->roof == RF_PITCH || b->bump_x[k] < lo + 8 || b->bump_x[k] + b->bump_w[k] > hi - 8 ||
                        b->bump_x[k] % 8) bad_shape++;
                    for (int j = 0; j < k; j++)
                        if (b->bump_x[j] >= 0 && b->bump_x[k] < b->bump_x[j] + b->bump_w[j] && b->bump_x[j] < b->bump_x[k] + b->bump_w[k])
                            bad_spawn++;
                }
                if (b->sky1 && (b->sky0 < EDGE_KEEP || b->sky1 > wid - EDGE_KEEP)) bad_spawn++;
                int gap = (int)(b->x0 - a->x1), d = difficulty_at(a->x1) >> 7;
                band *bd = &bands[d > 7 ? 7 : d];
                bd->roofs++;
                if (gap) { bd->gaps++; bd->gap_sum += gap; if (gap > bd->gap_max) bd->gap_max = gap; }
                else bd->nogap++;
                bd->hazards += b->sky1 != 0;
            }
            /* objects: every one placed fairly */
            for (int i = 0; i < w.no; i++) {
                obj *o = &w.o[i];
                if (o->t < 0 || o->flags == 99) continue;
                const bldg *b = 0;
                for (int j = 0; j < w.nb; j++) if (w.b[j].index == o->bld) b = &w.b[j];
                int d = difficulty_at(o->x) >> 7;
                band *bd = &bands[d > 7 ? 7 : d];
                switch (o->kind) {
                case OB_PIGEON:
                    if (b && (o->a < PIGEON_KEEP_L || o->b > (int)(b->x1 - b->x0) - PIGEON_KEEP_R || o->a > o->b)) bad_spawn++;
                    bd->pigeons++;
                    break;
                case OB_ANTENNA:
                    if (b && (o->x - b->x0 < EDGE_KEEP || b->x1 - o->x < EDGE_KEEP)) bad_spawn++;
                    bd->hazards++;
                    break;
                case OB_AWNING: case OB_POT: case OB_CRADLE: case OB_LINE: case OB_LEDGE: {
                    /* a prop stays in the gap before its building, clear of the walls */
                    int bi = -1;
                    for (int j = 1; j < w.nb; j++) if (w.b[j].index == o->bld) bi = j;
                    if (bi > 0 && (o->x < w.b[bi - 1].x1 || o->x + o->w > w.b[bi].x0)) bad_spawn++;
                    if (o->kind != OB_LINE && o->kind != OB_CRADLE && (o->y < 0 || o->y > STREET_Y - 40)) bad_spawn++;
                    bd->props++;
                    break;
                }
                case OB_ITEM:
                    if (b && (o->x < b->x0 || o->x + o->w > b->x1 || o->y > b->top - 16)) bad_spawn++;
                    break;
                case OB_GUST:
                    for (int j = 0; j < i; j++)
                        if (w.o[j].kind == OB_GUST && w.o[j].flags != 99 && o->x < w.o[j].x + w.o[j].w && w.o[j].x < o->x + o->w)
                            bad_spawn++;
                    bd->gusts++;
                    break;
                }
                o->flags = 99;                                  /* counted */
            }
            /* forget what is far behind (the game's camera does the same) */
            int k = 0;
            for (int i = 0; i < w.nb; i++) if (w.b[i].x1 >= until - 700 || i >= w.nb - 2) w.b[k++] = w.b[i];
            w.nb = k;
            k = 0;
            for (int i = 0; i < w.no; i++) if (w.o[i].x + w.o[i].w >= until - 900) w.o[k++] = w.o[i];
            w.no = k;
            (void)last_checked;
        }
    }
    printf("  %ld gaps and steps checked: %ld not reachable, %ld need the big bounce; %ld bad shapes, %ld unfair spawns\n",
           pairs, bad_reach, need_big, bad_shape, bad_spawn);
    CHECK(pairs > (long)seeds * 50, "enough buildings (%ld)", pairs);
    CHECK(bad_reach == 0, "every gap reachable in the worst case (%ld not)", bad_reach);
    CHECK(need_big > 0, "some gaps need the big bounce");
    CHECK(bad_shape == 0, "%ld bad shapes", bad_shape);
    CHECK(bad_spawn == 0, "%ld unfair spawns", bad_spawn);
    printf("  difficulty band (m):   gaps  mean gap  max gap  no-gap %%  pigeons/roof  gusts/100 roofs  props/gap\n");
    double prev_gap = 0, prev_pig = -1;
    int mono_gap = 1, mono_pig = 1;
    for (int d = 0; d < 8; d++) {
        band *b = &bands[d];
        if (!b->roofs) continue;
        double mg = b->gaps ? (double)b->gap_sum / b->gaps : 0, pg = (double)b->pigeons / b->roofs;
        printf("  %4d-%4d          %7ld  %8.1f  %7d  %8.1f  %12.2f  %15.1f  %9.2f\n", d * DIFF_FULL_M / 8,
               (d + 1) * DIFF_FULL_M / 8, b->gaps, mg, b->gap_max, 100.0 * b->nogap / b->roofs, pg, 100.0 * b->gusts / b->roofs,
               b->gaps ? (double)b->props / b->gaps : 0);
        if (mg < prev_gap - 2) mono_gap = 0;
        if (pg < prev_pig - 0.05) mono_pig = 0;       /* (the districts mix narrow and wide roofs) */
        prev_gap = mg;
        prev_pig = pg;
    }
    CHECK(mono_gap, "the gaps widen with the distance");
    CHECK(mono_pig, "more pigeons with the distance");
    CHECK(bands[0].gaps && bands[7].gaps && (double)bands[7].gap_sum / bands[7].gaps > 1.4 * bands[0].gap_sum / bands[0].gaps,
          "the far gaps are much wider than the first ones");
    CHECK(bands[7].roofs && bands[0].roofs && (double)bands[7].pigeons / bands[7].roofs > 3.0 * bands[0].pigeons / bands[0].roofs,
          "pigeons: 3x more at the end");
    CHECK(bands[0].gusts == 0 && bands[7].gusts > 0, "no wind at the start, wind later");
}

static void test_determinism(void)
{
    printf("determinism\n");
    static world w;
    uint32_t hs[3];
    for (int k = 0; k < 3; k++) {
        world_init(&w, 1, k == 2 ? 778 : 777, 0);
        w.fixed_seed = 1;
        uint32_t hash = 2166136261u;
        for (int f = 0; f < 4000; f++) {
            int dir[2] = {(f / 50) % 3 ? 1 : 0, 0}, a[2] = {(f / 90) % 2, 0}, ap[2] = {f == 20, 0};
            world_step(&w, dir, a, ap);
            hash = (hash ^ (uint32_t)w.m[0].x) * 16777619u;
            hash = (hash ^ (uint32_t)w.m[0].y ^ (uint32_t)w.camy) * 16777619u;
            for (int i = 0; i < w.nb; i++) hash = (hash ^ (uint32_t)(w.b[i].x0 + w.b[i].top)) * 16777619u;
        }
        hs[k] = hash;
    }
    CHECK(hs[0] == hs[1], "the same inputs, the same run: %08x %08x", hs[0], hs[1]);
    CHECK(hs[0] != hs[2], "another seed, another course");
    /* the medals */
    CHECK(medal_of(MEDAL_BRONZE - 1) == 0 && medal_of(MEDAL_BRONZE) == 1 && medal_of(MEDAL_GOLD) == 3 &&
          medal_of(MEDAL_CAT) == 4, "the cat-catch medals");
    CHECK(district_at(0) == 0 && district_at(DISTRICT_PX) == 1 && district_at(4 * DISTRICT_PX) == 0 &&
          night_at(4 * DISTRICT_PX) == 1, "districts, then the night loop");
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--metres") && i + 1 < argc) metres = atoi(argv[++i]);
    }
    test_references();
    test_arcs();
    test_air_control();
    test_roofs();
    test_collisions();
    test_generator();
    test_determinism();
    printf("pogomamie physics and generator: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
