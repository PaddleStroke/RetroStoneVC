/*
 * Pogo Mamie: unit tests of the game rules against the tuning table (physics.c, world.c, gen.c; no video or
 * sound): the bounce arcs, the air control, the roofs' shapes and the slopes' deflection, the collisions and the
 * hazards (antennas, pigeons walking and flying, balloons: a hit knocks her off, a stomp bounces), the clothesline
 * frame by frame, the generator (every gap reachable in the worst case with its hazards, over thousands of seeds;
 * no unfair spawn; the difficulty ramp) and determinism (1 and 4 players).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 *
 *   test_physics [--seeds N] [--metres M]
 */
#include "pm.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    CHECK(NEAR(px(V_RECOVER) * px(V_RECOVER) / (2 * px(GRAVITY)), 40, 0.3), "V_RECOVER: a 40-px hop");
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
    mamie_bounce(&m, &h, 2);
    CHECK(m.vy == -V_RECOVER && m.bounce == BN_RECOVER, "after a tumble: the weak hop");
    h.kind = SF_AWNING;
    mamie_bounce(&m, &h, 0);
    CHECK(m.vy == -V_SPRING, "an awning springs");
    h.kind = SF_BALLOON;
    mamie_bounce(&m, &h, 0);
    CHECK(m.vy == -V_SPRING, "a balloon's top springs");
}

/* ---- the slopes deflect the bounce ------------------------------------------------------------------------------- */
static void test_slopes(void)
{
    printf("slopes deflect the bounce (the half angle between up and the roof's normal)\n");
    static const struct { int slope; double roof_deg; } cases[] = {{2, 45.0}, {1, 26.565}, {-1, 26.565}, {-2, 45.0}};
    for (unsigned c = 0; c < 4; c++) {
        for (int kind = BN_NORMAL; kind <= BN_BIG; kind++) {
            int32_t v = bounce_speed(kind), vx = 0, vy = 0;
            slope_deflect(cases[c].slope, v, &vx, &vy);
            /* the roof's outward normal, (sin a, -cos a) for a slope going down to the right; the bounce: half way */
            double a = cases[c].roof_deg * M_PI / 180, sg = cases[c].slope > 0 ? 1 : -1;
            double nx = sg * sin(a), ny = -cos(a), mx = nx / 2, my = (ny - 1) / 2, ml = sqrt(mx * mx + my * my);
            double ex = px(v) * mx / ml, ey = px(v) * my / ml;
            double ang = atan2(px(vx), -px(vy)) * 180 / M_PI;
            CHECK(NEAR(px(vx), ex, 0.002) && NEAR(px(vy), ey, 0.002), "slope %d, bounce %d: (%.4f, %.4f) vs (%.4f, %.4f)",
                  cases[c].slope, kind, px(vx), px(vy), ex, ey);
            CHECK(NEAR(sqrt(px(vx) * px(vx) + px(vy) * px(vy)), px(v), 0.002), "the speed of the bounce is kept");
            CHECK(NEAR(fabs(ang), cases[c].roof_deg / 2, 0.05), "deflected by %.2f degrees (half of %.2f)", ang,
                  cases[c].roof_deg);
            if (kind == BN_NORMAL)
                printf("  slope %+d (%.1f deg roof): the bounce leaves %.2f deg off the vertical, vx %+.3f, vy %.3f px/frame\n",
                       cases[c].slope, cases[c].roof_deg, ang, px(vx), px(vy));
        }
    }
    /* the deflection adds to her speed, never past VX_SLOPE_MAX; a flat roof does not deflect */
    int32_t vx = VX_BOOST, vy = 0;
    slope_deflect(2, V_BIG, &vx, &vy);
    CHECK(vx == VX_SLOPE_MAX, "capped at %.2f px/frame", px(VX_SLOPE_MAX));
    vx = VX_MAX;
    slope_deflect(0, V_NORMAL, &vx, &vy);
    CHECK(vx == VX_MAX && vy == -V_NORMAL, "a flat roof: straight up");
    /* on the roofs: a mansard's left slope throws her back toward the gap she came from, the right one forward */
    mamie m;
    memset(&m, 0, sizeof m);
    hit h = {SF_ROOF, 0, -1, 300, -2};
    m.vx = VX_CRUISE;
    mamie_bounce(&m, &h, 0);
    CHECK(m.vx < 0 && m.deflect == -1, "the near slope of a mansard: thrown back (vx %.2f)", px(m.vx));
    h.slope = 2;
    m.vx = VX_CRUISE;
    mamie_bounce(&m, &h, 0);
    CHECK(m.vx > VX_CRUISE + Q16(1.5) && m.deflect == 1, "the far slope: thrown forward (vx %.2f)", px(m.vx));
    /* controllable: holding Right after a back kick, she is going forward again within 20 frames */
    m.vx = 0;
    h.slope = -2;
    mamie_bounce(&m, &h, 0);
    int t = 0;
    while (m.vx <= 0 && t < 100) { m.vx = vx_after_input(&m, 1); t++; }
    CHECK(t <= 20, "a back kick is recovered in %d frames holding Right", t);
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
    w->b[0].x0 = 0; w->b[0].x1 = 200; w->b[0].top = 320; w->b[0].index = 1;
    w->b[1].x0 = 200 + gap; w->b[1].x1 = 200 + gap + 200; w->b[1].top = (int16_t)top2; w->b[1].index = 2;
    w->next_index = 3;
    w->players = 1;
    for (int p = 1; p < MAX_PLAYERS; p++) w->m[p].state = MS_OFF;
    mamie_reset(&w->m[0], 100, 320);
    w->m[0].state = MS_AIR;
    w->started = 1;
}

static obj *one_obj(world *w, int kind)
{
    w->no = 1;
    memset(&w->o[0], 0, sizeof w->o[0]);
    w->o[0].kind = (uint8_t)kind;
    return &w->o[0];
}

/* run the world until an event (or n frames): returns the events seen */
static int run_until(world *w, int ev, int n, int dir0, int a0)
{
    int seen = 0;
    for (int i = 0; i < n; i++) {
        int dir[MAX_PLAYERS] = {dir0}, a[MAX_PLAYERS] = {a0}, ap[MAX_PLAYERS] = {0};
        world_step(w, dir, a, ap);
        seen |= w->events[0];
        if (w->events[0] & ev) break;
    }
    return seen;
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
    obj *o = one_obj(&w, OB_AWNING);
    o->x = 220; o->w = 80; o->y = 360;
    T = world_terrain(&w, 0);
    m->x = 205 * Q16_ONE; m->y = 380 * Q16_ONE; m->vx = VX_CRUISE; m->vy = -V_NORMAL;
    int landed = 0, up_through = 0;
    for (int i = 0; i < 200 && !landed; i++) {
        e = mamie_air_step(m, &T, 0, &h);
        if ((m->y >> 16) < 355) up_through = 1;
        if (e & EV_LAND) { landed = h.kind == SF_AWNING; break; }
    }
    CHECK(up_through && landed, "up through the awning, then onto it");
    /* a skylight breaks: she drops into the attic */
    two_bldgs(&w, 64, 320);
    w.b[0].sky0 = 88; w.b[0].sky1 = 112;
    m->x = 100 * Q16_ONE; m->y = 300 * Q16_ONE; m->vx = 0; m->vy = Q16(2.0);
    int glass = 0, pit = 0;
    for (int i = 0; i < 80; i++) {
        int d0[MAX_PLAYERS] = {0}, a[MAX_PLAYERS] = {0}, ap[MAX_PLAYERS] = {0};
        world_step(&w, d0, a, ap);
        glass |= w.events[0] & EV_GLASS;
        if (glass && (w.events[0] & EV_LAND) && w.m[0].land_y == 320 + PIT_DEPTH) pit = 1;
    }
    CHECK(glass && w.b[0].sky_broken && pit, "the skylight breaks, the attic's floor catches her");
    /* the props between the roofs: a window box and crumbling tiles break after one bounce, a cradle catches her */
    static const struct { int kind, sf, ev; } props[] = {{OB_POT, SF_POT, EV_POT}, {OB_LEDGE, SF_LEDGE, EV_BREAK},
                                                          {OB_CRADLE, SF_CRADLE, EV_LAND}};
    for (unsigned k = 0; k < sizeof props / sizeof props[0]; k++) {
        two_bldgs(&w, 96, 320);
        o = one_obj(&w, props[k].kind);
        o->x = 216; o->w = 64; o->y = 350; o->b = 350; o->a = 8;
        if (o->kind == OB_CRADLE) { o->a = 340; o->b = 380; o->pos = 350 * Q16_ONE; o->dir = 1; o->y = 320; }
        m->x = 244 * Q16_ONE; m->y = 300 * Q16_ONE; m->vx = 0; m->vy = Q16(1.0);
        int seen = 0, on = 0;
        for (int i = 0; i < 80; i++) {
            int d0[MAX_PLAYERS] = {-1}, a[MAX_PLAYERS] = {0}, ap[MAX_PLAYERS] = {0};
            world_step(&w, d0, a, ap);
            if ((w.events[0] & EV_LAND) && w.m[0].land_kind == props[k].sf) on = 1;
            seen |= w.events[0] & props[k].ev;
            if (w.m[0].vy < 0 && on && w.m[0].state == MS_AIR) break;
        }
        CHECK(on && seen, "a prop (kind %d) catches her", props[k].kind);
        if (props[k].kind == OB_POT || props[k].kind == OB_LEDGE) CHECK(w.o[0].state == 1, "... and falls (kind %d)", props[k].kind);
    }
    two_bldgs(&w, 96, 320);
    gen_add_baguette(&w, 100);
    CHECK(w.no == 1 && w.o[0].kind == OB_BAGUETTE && w.o[0].x < 200 && w.o[0].x + w.o[0].w > 296 && w.o[0].y == 320,
          "the baguette bridges the next gap");
    two_bldgs(&w, 96, 320);
    m->x = 240 * Q16_ONE; m->y = 330 * Q16_ONE; m->vy = Q16(1.0);
    CHECK(world_doomed(&w, m), "below both roofs in a gap: doomed");
    m->y = 318 * Q16_ONE;
    CHECK(!world_doomed(&w, m), "above them: not yet");
}

/* ---- the hazards: a hit knocks her off (she tumbles and drops), a stomp bounces --------------------------------------- */
static void test_hazards(void)
{
    printf("hazards: antennas, pigeons walking and flying, balloons\n");
    static world w;
    mamie *m = &w.m[0];
    /* an antenna in the middle of a roof: knocked off, she drops onto the roof and recovers with a weak hop */
    two_bldgs(&w, 64, 320);
    obj *o = one_obj(&w, OB_ANTENNA);
    o->x = 120; o->y = 320; o->bld = 1;
    m->x = 108 * Q16_ONE; m->y = 316 * Q16_ONE; m->vx = VX_MAX; m->vy = -Q16(2.0);
    int ev = run_until(&w, EV_STUMBLE, 30, 1, 0);
    CHECK((ev & EV_STUMBLE) && m->state == MS_TUMBLE && m->vy >= 0 && m->vx < 0 && m->chain == 0,
          "an antenna: the bounce is broken, she tumbles back and drops (state %d)", m->state);
    int x_hit = (int)(m->x >> 16), steered = 0;
    ev = run_until(&w, EV_RECOVER, 120, 1, 1);      /* Right and A held: no effect while tumbling */
    steered = (int)(m->x >> 16) > x_hit + 2;
    CHECK((ev & EV_RECOVER) && m->state == MS_AIR && m->vy == -V_RECOVER && !steered && !(ev & EV_DOOMED),
          "... onto the roof: she recovers with a weak hop (no control meanwhile)");
    /* an antenna near the start of the next roof, hit coming in low over the gap: knocked back into the gap */
    two_bldgs(&w, 64, 320);
    o = one_obj(&w, OB_ANTENNA);
    o->x = 264 + ANTENNA_KEEP_L; o->y = 320; o->bld = 2;
    m->x = 270 * Q16_ONE; m->y = 293 * Q16_ONE; m->vx = VX_MAX; m->vy = Q16(0.2);
    ev = run_until(&w, EV_DOOMED, 200, 1, 0);
    CHECK((ev & EV_STUMBLE) && (ev & EV_DOOMED) && m->state == MS_FALL, "an antenna by the landing edge: a fatal tumble");
    /* a walking pigeon: on its head, a stunt and a bounce; from the side, knocked off */
    two_bldgs(&w, 64, 320);
    o = one_obj(&w, OB_PIGEON);
    o->var = PG_WALK; o->x = 0; o->y = 320; o->w = 200; o->a = 120; o->b = 120; o->pos = 120 * Q16_ONE; o->t = 100;
    m->x = 120 * Q16_ONE; m->y = 280 * Q16_ONE; m->vx = 0; m->vy = Q16(2.0);
    ev = run_until(&w, EV_PIGEON, 40, 0, 0);
    CHECK((ev & EV_PIGEON) && m->stunts == PTS_PIGEON && m->state == MS_AIR && m->vy < 0,
          "on a pigeon's head: a stunt (%d points) and a bounce", m->stunts);
    two_bldgs(&w, 64, 320);
    o = one_obj(&w, OB_PIGEON);
    o->var = PG_WALK; o->x = 0; o->y = 320; o->w = 200; o->a = 130; o->b = 130; o->pos = 130 * Q16_ONE; o->t = 100;
    m->x = 110 * Q16_ONE; m->y = 318 * Q16_ONE; m->vx = VX_MAX; m->vy = -Q16(0.5);
    ev = run_until(&w, EV_KNOCK, 30, 1, 0);
    CHECK((ev & EV_KNOCK) && m->state == MS_TUMBLE && m->vx < 0 && o->state == 1,
          "into a pigeon from the side: knocked off, the pigeon flies away");
    /* a flying pigeon: hovering in the middle of a gap */
    static const int ways[3] = {PG_HOVER, PG_GLIDE, PG_SWOOP};
    for (int k = 0; k < 3; k++) {
        two_bldgs(&w, 96, 320);
        o = one_obj(&w, OB_PIGEON);
        o->var = (uint8_t)ways[k]; o->x = ways[k] == PG_HOVER ? 248 : 204; o->w = ways[k] == PG_HOVER ? 0 : 88;
        o->y = 280; o->a = ways[k] == PG_SWOOP ? 32 : FLY_HOVER_BOB; o->b = 200; o->t = 0; o->bld = 2;
        /* its path: every box it is in lies inside its swept boxes */
        hbox sw[12];
        int n = pigeon_swept(o, sw, 12), inside = 1, moved = 0, cx0 = 0, fy0 = 0;
        for (int t = 0; t < 200; t++) {
            int cx, fy;
            pigeon_at_t(o, t, &cx, &fy);
            if (t == 0) { cx0 = cx; fy0 = fy; }
            moved |= cx != cx0 || fy != fy0;
            int in = 0;
            for (int j = 0; j < n; j++)
                in |= cx - PIGEON_W / 2 >= sw[j].x0 && cx + PIGEON_W / 2 <= sw[j].x1 && fy - (PIGEON_H - 2) >= sw[j].y0 &&
                      fy <= sw[j].y1;
            inside &= in;
        }
        CHECK(moved && inside, "a flying pigeon (way %d) moves along its path, inside its swept boxes", ways[k]);
        /* flying into it from the side: knocked off, in the gap: the fall */
        o->x = 250; o->b = 30000; o->t = 0;  /* (a very slow loop: it stays put while she comes) */
        int cx, fy;
        pigeon_at(o, &cx, &fy);
        m->x = (cx - 24) * Q16_ONE; m->y = (fy + 10) * Q16_ONE; m->vx = VX_MAX; m->vy = -Q16(0.4);
        ev = run_until(&w, EV_DOOMED, 200, 1, 0);
        CHECK((ev & EV_KNOCK) && (ev & EV_DOOMED), "flying into a pigeon (way %d) over a gap: knocked off, the fall", ways[k]);
        /* from above: a stomp, a bounce and 150 points */
        two_bldgs(&w, 96, 320);
        o = one_obj(&w, OB_PIGEON);
        o->var = (uint8_t)ways[k]; o->x = 248; o->w = 0; o->y = 290; o->a = 0; o->b = 30000; o->t = 0; o->bld = 2;
        if (ways[k] != PG_HOVER) o->var = PG_HOVER;
        m->x = 248 * Q16_ONE; m->y = 250 * Q16_ONE; m->vx = 0; m->vy = Q16(2.0);
        ev = run_until(&w, EV_PIGEON, 60, 0, 0);
        CHECK((ev & EV_PIGEON) && m->stunts == PTS_PIGEON_FLY && m->vy < 0 && !(ev & EV_KNOCK),
              "stomping a flying pigeon: a bounce and %d points", m->stunts);
    }
    /* a balloon: its top is a spring and a stunt; its basket and ropes knock her off */
    two_bldgs(&w, 96, 320);
    o = one_obj(&w, OB_BALLOON);
    o->x = 100; o->pos = 100 * Q16_ONE; o->y = 150; o->w = BALLOON_W; o->dir = -1;
    m->x = 116 * Q16_ONE; m->y = 120 * Q16_ONE; m->vx = 0; m->vy = Q16(2.0);
    ev = run_until(&w, EV_BALLOON, 40, 0, 0);
    CHECK((ev & EV_BALLOON) && (ev & EV_STUNT) && m->vy == -V_SPRING && m->stunts == PTS_BALLOON,
          "on a balloon's top: a spring and %d points", m->stunts);
    two_bldgs(&w, 96, 320);
    o = one_obj(&w, OB_BALLOON);
    o->x = 100; o->pos = 100 * Q16_ONE; o->y = 200; o->w = BALLOON_W; o->dir = -1;
    m->x = 110 * Q16_ONE; m->y = 280 * Q16_ONE; m->vx = 0; m->vy = -V_BIG;
    ev = run_until(&w, EV_BASKET, 60, 0, 0);
    CHECK((ev & EV_BASKET) && m->state == MS_TUMBLE, "a big bounce into a balloon's basket: knocked off");
    /* it drifts left */
    int bx = balloon_x(o);
    run_until(&w, 0, 64, 0, 0);
    CHECK(balloon_x(o) == bx - 16, "a balloon drifts left at %.2f px/frame", px(BALLOON_V));
}

/* ---- the clothesline, frame by frame -------------------------------------------------------------------------------- */
static void line_case(int gap, int land_k, int wait)
{
    static world w;
    two_bldgs(&w, gap, 320);
    obj *o = one_obj(&w, OB_LINE);
    o->x = 200; o->w = (int16_t)gap; o->y = 350; o->b = (int16_t)(350 + gap / 4); o->a = (int16_t)LINE_REST_SAG(gap);
    mamie *m = &w.m[0];
    /* the old bug: the line's timer made it sink frame after frame; it must keep its shape whatever the time */
    int rest_mid = obj_line_y(o, 200 + gap / 2);
    m->state = MS_OFF;                                  /* (out of the way while the time passes) */
    run_until(&w, 0, wait, 0, 0);
    m->state = MS_AIR;
    CHECK(obj_line_y(o, 200 + gap / 2) == rest_mid, "gap %d: after %d frames the line still hangs at %d (%d)", gap, wait,
          rest_mid, obj_line_y(o, 200 + gap / 2));
    int lx = 200 + land_k;
    m->x = lx * Q16_ONE; m->y = (obj_line_y(o, lx) - 30) * Q16_ONE; m->vx = 0; m->vy = Q16(3.0);
    int riding = 0, on_rope = 1, deeper = 1, released = 0, release_bottom = 0, max_sag = 0, last_sag = -1, frames = 0;
    int land_y = 0, ride_frames = 0, release_y = 0;
    for (int i = 0; i < 160 && !released; i++) {
        int dir[MAX_PLAYERS] = {0}, a[MAX_PLAYERS] = {0}, ap[MAX_PLAYERS] = {0};
        world_step(&w, dir, a, ap);
        frames++;
        if (m->state == MS_SLING) ride_frames++;
        if (m->state == MS_SLING || (w.events[0] & EV_SLING)) {
            if (!riding) land_y = (int)(m->y >> 16);
            riding = 1;
            int x = (int)(m->x >> 16);
            /* her feet on the rope as drawn (obj_line_y is what draw.c draws, the rope's points aligned on her) */
            on_rope &= (int)(m->y >> 16) == obj_line_y(o, x) && o->c == x - (int)o->x;
            if ((int)o->sag < last_sag) deeper = 0;
            last_sag = (int)o->sag;
            if ((int)o->sag > max_sag) max_sag = (int)o->sag;
        }
        if (w.events[0] & EV_SLING) {
            released = 1;
            release_y = (int)(m->y >> 16);
            release_bottom = o->sag == line_dip_target(o, o->c) && (int)o->sag == max_sag && m->vy == -V_SLING &&
                             release_y == obj_line_y(o, (int)(m->x >> 16));
        }
    }
    int depth_px = (max_sag + 128) >> 8;
    CHECK(riding && ride_frames == SLING_FRAMES, "gap %d, landing at %d: rides the line for %d frames (%d)", gap, land_k,
          ride_frames, SLING_FRAMES);
    CHECK(on_rope, "gap %d, landing at %d: her feet stay on the drawn rope every frame of the dip", gap, land_k);
    CHECK(deeper && depth_px >= 3, "gap %d: the line dips steadily under her, %d px", gap, depth_px);
    CHECK(released && release_bottom, "gap %d: slung up from the bottom of the dip (y %d, landed at %d)", gap, release_y, land_y);
    /* then it recoils past its rest and back (the clothes swing), and comes to rest */
    int up = 0, settled = 0, y20 = 0;
    for (int i = 0; i < 90; i++) {
        int dir[MAX_PLAYERS] = {0}, a[MAX_PLAYERS] = {0}, ap[MAX_PLAYERS] = {0};
        world_step(&w, dir, a, ap);
        if (i == 19) y20 = (int)(m->y >> 16);
        if (o->sag < 0) up = 1;
        if (o->sag == 0 && o->sagv == 0 && i >= 19) { settled = 1; break; }
    }
    CHECK(up && settled, "gap %d: the rope recoils above its rest, then settles", gap);
    CHECK(y20 < release_y - 60, "gap %d: 20 frames later she is well on her way up (%d px)", gap, release_y - y20);
}

static void test_clothesline(void)
{
    printf("the clothesline, frame by frame\n");
    line_case(96, 48, 0);
    line_case(96, 20, 1000);
    line_case(40, 30, 300);
    line_case(136, 100, 5000);
    /* its shape: the poles, the rest sag of a shallow catenary, the dip under a load: two straight lines */
    obj o;
    memset(&o, 0, sizeof o);
    o.kind = OB_LINE; o.x = 0; o.w = 96; o.y = 100; o.b = 112; o.a = (int16_t)LINE_REST_SAG(96);
    CHECK(obj_line_y(&o, 0) == 100 && obj_line_y(&o, 96) == 112 && obj_line_y(&o, 48) == 106 + o.a, "the rest shape");
    o.c = 32; o.sag = line_dip_target(&o, 32);
    int base = 100 + 12 * 32 / 96 + o.a * 4 * 32 * 64 / (96 * 96);
    CHECK(obj_line_y(&o, 32) == base + ((o.sag + 128) >> 8), "the dip is under the load");
    CHECK(line_dip_target(&o, 48) == SLING_DEPTH * 256 && line_dip_target(&o, 4) < line_dip_target(&o, 48) / 4,
          "the full dip at the middle (%d px), little near a pole", SLING_DEPTH);
    o.w = 32;
    CHECK(line_dip_target(&o, 16) == 8 * 256, "a short line dips less (w / 4)");
}

/* ---- the generator ------------------------------------------------------------------------------------------------- */
/* An independent worst-case check: from a standstill at a take-off point on building a (a slope there deflects the
 * bounce), the real physics, holding Right until past the aim, then braking; it must not touch an antenna nor a
 * pigeon at any time of its loop; a landing on a slope throwing her back must be recovered; she must not land just
 * before an antenna. */
static const world *iw;
static int iw_from, iw_to;

static int hazard_now(int x, int y)
{
    hbox me = body_box(x, y);
    for (int i = 0; i < iw->no; i++) {
        const obj *o = &iw->o[i];
        if (o->state || (o->bld != iw->b[iw_from].index && o->bld != iw->b[iw_to].index)) continue;
        if (o->kind == OB_ANTENNA) {
            hbox a = {(int)o->x - ANTENNA_HALF, o->y - ANTENNA_H, (int)o->x + ANTENNA_HALF + 1, o->y};
            if (box_hit(me.x0, me.y0, me.x1, me.y1, &a)) return 1;
        } else if (o->kind == OB_PIGEON) {
            if (o->var == PG_WALK) {
                hbox a = {(int)o->x + o->a - PIGEON_W / 2, o->y - PIGEON_H + 2, (int)o->x + o->b + PIGEON_W / 2 + 1, o->y};
                if (box_hit(me.x0, me.y0, me.x1, me.y1, &a)) return 1;
                continue;
            }
            /* every frame of its loop */
            if (me.x1 < o->x - 8 || me.x0 > o->x + o->w + 8) continue;
            for (int t = 0; t < pigeon_period(o); t++) {
                int cx, fy;
                pigeon_at_t(o, t, &cx, &fy);
                hbox a = {cx - PIGEON_W / 2, fy - PIGEON_H + 2, cx + PIGEON_W / 2, fy};
                if (box_hit(me.x0, me.y0, me.x1, me.y1, &a)) return 1;
            }
        }
    }
    return 0;
}

static int antenna_near(int bi, int x)
{
    for (int i = 0; i < iw->no; i++) {
        const obj *o = &iw->o[i];
        if (o->kind == OB_ANTENNA && o->bld == iw->b[bi].index && x > o->x - ANTENNA_HOP && x <= o->x + ANTENNA_HALF + HALF_W)
            return 1;
    }
    return 0;
}

static int indep_arc(mamie *m, int bi, int aim, hit *h)
{
    terrain T = world_terrain(iw, 1);
    for (int t = 0; t < 600; t++) {
        int x = (int)(m->x >> 16);
        int dir = x < aim ? 1 : m->vx > 0 ? -1 : 0;
        int e = mamie_air_step(m, &T, dir, h);
        if (hazard_now((int)(m->x >> 16), (int)(m->y >> 16))) return 0;
        if (e & EV_LAND) return h->idx == bi && !antenna_near(bi, (int)(m->x >> 16));
    }
    return 0;
}

static int reach_indep(const world *w, int ai, int bi, int takeoff, int big, int aim)
{
    const bldg *a = &w->b[ai], *b = &w->b[bi];
    iw = w; iw_from = ai; iw_to = bi;
    mamie m;
    memset(&m, 0, sizeof m);
    m.state = MS_AIR;
    int tx = (int)a->x1 - takeoff;
    m.x = (int32_t)tx * Q16_ONE;
    m.y = (int32_t)bldg_surface(a, tx) * Q16_ONE;
    hit h0 = {SF_ROOF, ai, -1, bldg_surface(a, tx), bldg_slope(a, tx)};
    mamie_bounce(&m, &h0, big);
    hit h;
    if (!indep_arc(&m, bi, (int)b->x0 + aim, &h)) return 0;
    for (int r = 0; r < 2 && h.slope < 0; r++) {        /* thrown back: recover past the slope */
        int past = (int)b->x0;
        while (bldg_slope(b, past) < 0) past++;
        mamie_bounce(&m, &h, 0);
        if (!indep_arc(&m, bi, past + 8, &h)) return 0;
    }
    return h.slope >= 0;
}

typedef struct { long gaps, gap_sum, pigeons, roofs, gusts, hazards, props, nogap, antennas, flyers, balloons, sloped;
                 int gap_max; } band;

static int seeds = 3000, metres = 1100, long_seeds = 300, long_metres = 3300;

typedef struct { int32_t x0, x1; int top; } roof_rec;
static roof_rec roofs_seen[4096];
static int nroofs_seen;

static void test_generator(void)
{
    printf("the generator: %d seeds x %d m (the first %d: %d m)\n", seeds, metres, long_seeds, long_metres);
    static world w;
    static band bands[8];
    memset(bands, 0, sizeof bands);
    long pairs = 0, bad_reach = 0, bad_shape = 0, bad_spawn = 0, need_big = 0, printed = 0, with_hazards = 0;
    long balloons = 0, bad_balloon = 0;
    for (int s = 0; s < seeds; s++) {
        memset(&w, 0, sizeof w);
        w.seed = (uint32_t)s * 7919u + 13u;
        gen_reset(&w);
        nroofs_seen = 0;
        int32_t until = 0;
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
                if (nroofs_seen < 4096) {
                    int top = b->top;
                    for (int k = 0; k < BUMP_MAX; k++) if (b->bump_x[k] >= 0 && bump_top(b, k) < top) top = bump_top(b, k);
                    roofs_seen[nroofs_seen++] = (roof_rec){b->x0, b->x1, top};
                }
                /* the aims: past the edge, past each antenna of b */
                int aims[4] = {LAND_MARGIN}, na = 1, haz = 0;
                for (int k = 0; k < w.no; k++) {
                    const obj *o = &w.o[k];
                    if ((o->bld == a->index || o->bld == b->index) && (o->kind == OB_ANTENNA || o->kind == OB_PIGEON)) haz = 1;
                    if (o->kind == OB_ANTENNA && o->bld == b->index && na < 4)
                        aims[na++] = (int)(o->x - b->x0) + ANTENNA_HALF + HALF_W + 4;
                }
                with_hazards += haz;
                int ok = 0, via_big = 1;
                for (int q = 0; q < na && !ok; q++)
                    for (int big = 0; big < 2 && !ok; big++)
                        for (int k = 0; k < 3 && !ok; k++)
                            if (reach_indep(&w, i - 1, i, gen_takeoffs[k], big, aims[q])) { ok = 1; via_big = big; }
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
                int gap = (int)(b->x0 - a->x1), d = difficulty_at(a->x1) * 8 / 1024;
                band *bd = &bands[d > 7 ? 7 : d];
                bd->roofs++;
                bd->sloped += b->roof != RF_FLAT;
                if (gap) { bd->gaps++; bd->gap_sum += gap; if (gap > bd->gap_max) bd->gap_max = gap; }
                else bd->nogap++;
                bd->hazards += b->sky1 != 0;
            }
            /* objects: every one placed fairly */
            for (int i = 0; i < w.no; i++) {
                obj *o = &w.o[i];
                if (o->flags == 99) continue;
                const bldg *b = 0;
                int bi = -1;
                for (int j = 0; j < w.nb; j++) if (w.b[j].index == o->bld) { b = &w.b[j]; bi = j; }
                int d = difficulty_at(o->x) * 8 / 1024;
                band *bd = &bands[d > 7 ? 7 : d];
                switch (o->kind) {
                case OB_PIGEON:
                    if (o->var == PG_WALK) {
                        if (b && (o->a < PIGEON_KEEP_L || o->b > (int)(b->x1 - b->x0) - PIGEON_KEEP_R || o->a > o->b)) bad_spawn++;
                        bd->pigeons++;
                    } else {
                        /* in its gap, inside it by FLY_KEEP, above the higher of the two edges */
                        if (bi > 0) {
                            const bldg *a = &w.b[bi - 1];
                            int hi = bldg_surface(a, (int)a->x1 - 1) < bldg_surface(b, (int)b->x0) ? bldg_surface(a, (int)a->x1 - 1)
                                                                                                    : bldg_surface(b, (int)b->x0);
                            int cx, fy, low = 0;
                            for (int t = 0; t < pigeon_period(o); t++) { pigeon_at_t(o, t, &cx, &fy); if (fy > low) low = fy; }
                            if (o->x < a->x1 + FLY_KEEP || o->x + o->w > b->x0 - FLY_KEEP || low > hi - 8) bad_spawn++;
                        }
                        bd->flyers++;
                    }
                    break;
                case OB_ANTENNA:
                    if (b) {
                        int k = (int)(o->x - b->x0), lo = b->roof == RF_MANSARD ? MANSARD_W : 0;
                        if (k < ANTENNA_KEEP_L || k < lo || b->x1 - o->x < EDGE_KEEP || b->roof == RF_PITCH ||
                            bldg_surface(b, (int)o->x) != o->y) bad_spawn++;
                    }
                    bd->antennas++;
                    break;
                case OB_BALLOON: {
                    /* its basket BALLOON_CLEAR above every roof it can drift over */
                    int bottom = o->y + BALLOON_HAZ_Y1;
                    for (int k = 0; k < nroofs_seen; k++)
                        if (roofs_seen[k].x1 > o->x - BALLOON_BACK && roofs_seen[k].x0 < o->x + BALLOON_W &&
                            bottom > roofs_seen[k].top - BALLOON_CLEAR) { bad_balloon++; break; }
                    balloons++;
                    bd->balloons++;
                    break;
                }
                case OB_AWNING: case OB_POT: case OB_CRADLE: case OB_LINE: case OB_LEDGE: {
                    /* a prop stays in the gap before its building, clear of the walls */
                    if (bi > 0 && (o->x < w.b[bi - 1].x1 || o->x + o->w > w.b[bi].x0)) bad_spawn++;
                    if (o->kind != OB_LINE && o->kind != OB_CRADLE && (o->y < 0 || o->y > STREET_Y - 40)) bad_spawn++;
                    if (o->kind == OB_LINE && (iabs(o->b - o->y) > o->w / 2 || o->a != LINE_REST_SAG(o->w))) bad_spawn++;
                    bd->props++;
                    break;
                }
                case OB_ITEM:
                    if (b && (o->x < b->x0 || o->x + o->w > b->x1 || o->y > b->top - 16)) bad_spawn++;
                    break;
                case OB_GUST:
                    for (int j = 0; j < i; j++)
                        if (w.o[j].kind == OB_GUST && w.o[j].flags == 99 && o->x < w.o[j].x + w.o[j].w && w.o[j].x < o->x + o->w)
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
        }
    }
    printf("  %ld gaps and steps checked (%ld with hazards): %ld impossible, %ld need the big bounce; %ld bad shapes, "
           "%ld unfair spawns; %ld balloons, %ld too low\n", pairs, with_hazards, bad_reach, need_big, bad_shape, bad_spawn,
           balloons, bad_balloon);
    CHECK(pairs > (long)seeds * 50, "enough buildings (%ld)", pairs);
    CHECK(bad_reach == 0, "impossible = 0: every gap reachable in the worst case, with its hazards (%ld not)", bad_reach);
    CHECK(need_big > 0, "some gaps need the big bounce");
    CHECK(with_hazards > pairs / 3, "hazards around many gaps (%ld)", with_hazards);
    CHECK(bad_shape == 0, "%ld bad shapes", bad_shape);
    CHECK(bad_spawn == 0, "%ld unfair spawns", bad_spawn);
    CHECK(balloons > 0 && bad_balloon == 0, "balloons high enough above the roofs (%ld of %ld too low)", bad_balloon, balloons);
    printf("  difficulty band (m):  gaps  mean gap  max  no-gap %%  slopes %%  antennas/roof  pigeons/roof  flyers/gap  "
           "balloons/km  gusts/100 roofs  props/gap\n");
    double prev_gap = 0, prev_haz = -1;
    int mono_gap = 1, mono_haz = 1;
    for (int d = 0; d < 8; d++) {
        band *b = &bands[d];
        if (!b->roofs) continue;
        double mg = b->gaps ? (double)b->gap_sum / b->gaps : 0;
        double haz = (double)(b->antennas + b->pigeons) / b->roofs + (b->gaps ? (double)b->flyers / b->gaps : 0);
        double km = (double)(b->roofs) * 0.1;      /* ~100 m per roof and gap: per km, roughly */
        printf("  %4d-%4d         %6ld  %8.1f  %3d  %8.1f  %8.1f  %13.2f  %12.2f  %10.2f  %11.2f  %15.1f  %9.2f\n",
               d * DIFF_FULL_M / 8, (d + 1) * DIFF_FULL_M / 8, b->gaps, mg, b->gap_max, 100.0 * b->nogap / b->roofs,
               100.0 * b->sloped / b->roofs, (double)b->antennas / b->roofs, (double)b->pigeons / b->roofs,
               b->gaps ? (double)b->flyers / b->gaps : 0, km > 0 ? b->balloons / km : 0, 100.0 * b->gusts / b->roofs,
               b->gaps ? (double)b->props / b->gaps : 0);
        if (mg < prev_gap - 2) mono_gap = 0;
        if (haz < prev_haz - 0.08) mono_haz = 0;     /* (the districts mix narrow and wide roofs) */
        prev_gap = mg;
        prev_haz = haz;
    }
    CHECK(mono_gap, "the gaps widen with the distance");
    CHECK(mono_haz, "more hazards with the distance");
    CHECK(bands[0].gaps && bands[7].gaps && (double)bands[7].gap_sum / bands[7].gaps > 1.4 * bands[0].gap_sum / bands[0].gaps,
          "the far gaps are much wider than the first ones");
    CHECK(bands[0].roofs && bands[0].antennas + bands[0].flyers > bands[0].roofs / 8, "hazards from the start");
    CHECK(bands[7].roofs && (double)(bands[7].antennas + bands[7].pigeons + bands[7].flyers) / bands[7].roofs >
          2.0 * (bands[0].antennas + bands[0].pigeons + bands[0].flyers) / bands[0].roofs, "hazards: twice as dense at the end");
    CHECK(bands[0].gusts == 0 && bands[7].gusts > 0, "no wind at the start, wind later");
}

static void test_determinism(void)
{
    printf("determinism (1 and 4 players)\n");
    static world w;
    uint32_t hs[5];
    for (int k = 0; k < 5; k++) {
        world_init(&w, k == 2 || k == 3 ? 4 : 1, k == 4 ? 778 : 777, 0);
        w.fixed_seed = 1;
        uint32_t hash = 2166136261u;
        for (int f = 0; f < 4000; f++) {
            int dir[MAX_PLAYERS], a[MAX_PLAYERS], ap[MAX_PLAYERS];
            for (int p = 0; p < MAX_PLAYERS; p++) {
                dir[p] = ((f + 13 * p) / 50) % 3 ? 1 : 0;
                a[p] = ((f + 7 * p) / 90) % 2;
                ap[p] = f == 20;
            }
            world_step(&w, dir, a, ap);
            for (int p = 0; p < w.players; p++) {
                hash = (hash ^ (uint32_t)w.m[p].x) * 16777619u;
                hash = (hash ^ (uint32_t)w.m[p].y ^ (uint32_t)w.m[p].state) * 16777619u;
            }
            hash = (hash ^ (uint32_t)w.camy) * 16777619u;
            for (int i = 0; i < w.nb; i++) hash = (hash ^ (uint32_t)(w.b[i].x0 + w.b[i].top)) * 16777619u;
            for (int i = 0; i < w.no; i++) hash = (hash ^ (uint32_t)(w.o[i].x * 31 + w.o[i].t)) * 16777619u;
        }
        hs[k] = hash;
        if (k == 3) {
            int alive = 0, out = 0;
            for (int p = 0; p < 4; p++) { alive += MS_ALIVE(w.m[p].state); out += w.m[p].state == MS_OFF; }
            printf("  4 players after 4000 frames: %d bouncing, %d out, distances %d %d %d %d m\n", alive, out, w.m[0].dist_m,
                   w.m[1].dist_m, w.m[2].dist_m, w.m[3].dist_m);
            CHECK(w.m[0].dist_m > 0 && w.m[3].dist_m > 0, "4 players all bounced along");
        }
    }
    CHECK(hs[0] == hs[1], "the same inputs, the same run: %08x %08x", hs[0], hs[1]);
    CHECK(hs[2] == hs[3], "4 players: the same inputs, the same race: %08x %08x", hs[2], hs[3]);
    CHECK(hs[0] != hs[4] && hs[0] != hs[2], "another seed, another course; 4 players, another race");
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
    if (long_seeds > seeds) long_seeds = seeds;
    test_references();
    test_arcs();
    test_slopes();
    test_air_control();
    test_roofs();
    test_collisions();
    test_hazards();
    test_clothesline();
    test_generator();
    test_determinism();
    printf("pogomamie physics and generator: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
