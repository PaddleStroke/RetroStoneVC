/*
 * Leady Squid: unit tests of the game rules against the tuning table
 * (physics.c, world.c; no video or sound).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
 *
 * The expected values are computed here from the REFERENCE numbers of tuning.h
 * (60 Hz, reference pixels), with the reference's own step order, and scaled:
 * the fixed-point game must follow them.
 */
#include "ls.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))

static double px(int32_t q) { return q / 65536.0; }

/* the reference's arc at 60 Hz: the flap frame moves by the flap speed, then gravity, capped */
static void ref_arc(double *apex, int *apex_t, int *back_t)
{
    double v = REF_FLAP_VY, y = 0, top = 0;
    int t = 0, tt = 0;
    y += v;
    t = 1;
    while (1) {
        v = fmin(v + REF_GRAVITY, REF_MAX_FALL);
        y += v;
        t++;
        if (y < top) { top = y; tt = t; }
        if (y >= 0) break;
    }
    *apex = -top;
    *apex_t = tt;
    *back_t = t;
}

static int ref_fall_frames(double dist)
{
    double v = 0, y = 0;
    int t = 0;
    while (y < dist) {
        v = fmin(v + REF_GRAVITY, REF_MAX_FALL);
        y += v;
        t++;
    }
    return t;
}

static void test_constants(void)
{
    printf("constants\n");
    CHECK(GRAVITY == Q16(0.25 * 192 / 404.0), "gravity %d", GRAVITY);
    CHECK(NEAR(px(GRAVITY), 0.1188, 0.0005), "gravity %.4f px/frame^2", px(GRAVITY));
    CHECK(NEAR(px(FLAP_VY), -2.139, 0.001), "flap %.4f px/frame", px(FLAP_VY));
    CHECK(NEAR(px(MAX_FALL), 2.376, 0.001), "max fall %.4f px/frame", px(MAX_FALL));
    CHECK(SCROLL == Q16_ONE, "scroll %d (1 px/frame)", SCROLL);
    CHECK(SPACING == 72 && SPACING % 8 == 0, "spacing %d", SPACING);
    CHECK(SPACING * Q16_ONE / SCROLL == 72, "72 frames (1.2 s) between obstacles");
    CHECK(REF_SPACING / REF_SCROLL == 72.0, "the reference: 72 frames too");
    CHECK(NEAR(GAP, REF_GAP * K_V, 0.6), "gap %d vs %.2f", GAP, REF_GAP * K_V);
    CHECK(GAP_TOP_MIN == 38 && GAP_TOP_RANGE == 67, "gap top range [%d, %d)", GAP_TOP_MIN, GAP_TOP_MIN + GAP_TOP_RANGE);
    CHECK(FIRST_OBST_X % 8 == 0 && OBST_W % 8 == 0, "obstacles on the 8-px grid");
}

static void test_arc(void)
{
    printf("the arc of a flap\n");
    double ref_apex;
    int ref_apex_t, ref_back_t;
    ref_arc(&ref_apex, &ref_apex_t, &ref_back_t);
    squid s;
    squid_reset(&s, SQUID_X);
    s.state = SQ_SWIM;
    s.y = 120 * Q16_ONE;
    int32_t y0 = s.y, top = s.y;
    int t = 0, apex_t = 0;
    squid_swim_step(&s, 1);
    t = 1;
    CHECK(s.vy == FLAP_VY, "the flap frame sets the speed");
    while (s.y < y0 && t < 200) {
        if (s.y < top) { top = s.y; apex_t = t; }
        squid_swim_step(&s, 0);
        t++;
    }
    double apex = px(y0 - top);
    printf("  apex %.2f px after %d frames, back after %d frames (reference x K_V: %.2f px, %d, %d)\n",
           apex, apex_t, t, ref_apex * K_V, ref_apex_t, ref_back_t);
    CHECK(NEAR(apex, ref_apex * K_V, 0.5), "apex %.2f vs %.2f", apex, ref_apex * K_V);
    CHECK(abs(apex_t - ref_apex_t) <= 1, "apex time %d vs %d", apex_t, ref_apex_t);
    CHECK(abs(t - ref_back_t) <= 1, "arc duration %d vs %d", t, ref_back_t);
    /* the 30-fps FlapPyBird arc (45 px, top after 9 frames = 0.30 s) within 10% */
    CHECK(NEAR(apex, 45 * K_V, 45 * K_V * 0.1), "apex %.2f vs FlapPyBird %.2f", apex, 45 * K_V);
    CHECK(NEAR(apex_t / 60.0, 0.30, 0.03), "apex after %.3f s (reference 0.30 s)", apex_t / 60.0);
}

static void test_flap_sets_speed(void)
{
    printf("a flap sets the speed, it does not add\n");
    squid s;
    squid_reset(&s, SQUID_X);
    s.y = 100 * Q16_ONE;
    for (int i = 0; i < 120; i++) squid_swim_step(&s, 0);
    CHECK(s.vy == MAX_FALL, "speed capped at MAX_FALL after 2 s (%d)", s.vy);
    squid_swim_step(&s, 1);
    CHECK(s.vy == FLAP_VY, "flap while falling at full speed -> %d", s.vy);
    squid_swim_step(&s, 1);
    CHECK(s.vy == FLAP_VY, "flap while rising -> %d (no double boost)", s.vy);
}

static void test_fall_time(void)
{
    printf("fall from the surface to the seabed\n");
    squid s;
    squid_reset(&s, SQUID_X);
    s.state = SQ_SWIM;
    s.tilt = 0;
    s.y = 6 * Q16_ONE;                     /* hitbox top at the surface */
    int t = 0;
    while (!box_hits_seabed(squid_box(&s)) && t < 1000) {
        squid_swim_step(&s, 0);
        t++;
    }
    double dist = px(s.y) - 6.0;           /* how far the centre went until the (turned) box touched */
    int ref = ref_fall_frames(dist / K_V);
    printf("  %d frames (%.2f s) over %.1f px; the reference over the same share of its sky: %d frames\n",
           t, t / 60.0, dist, ref);
    CHECK(abs(t - ref) <= 1, "fall %d frames vs %d", t, ref);
    CHECK(NEAR(t / 60.0, 1.4, 0.15), "about 1.4 s (%.2f)", t / 60.0);
}

static void test_tilt_and_box(void)
{
    printf("tilt and hitbox\n");
    static const int w[6] = HIT_W_TABLE, h[6] = HIT_H_TABLE;
    squid s;
    squid_reset(&s, SQUID_X);
    s.y = 100 * Q16_ONE;
    squid_swim_step(&s, 1);
    CHECK(squid_tilt_frame(s.tilt) == 0 && squid_tilt_shown(s.tilt) == 20, "nose up after a flap");
    int t = 0;
    while (squid_tilt_shown(s.tilt) >= 20 && t < 100) { squid_swim_step(&s, 0); t++; }
    CHECK(NEAR(t, (45 - 20) / 1.5, 1.5), "held nose-up for %d frames (FlapPyBird: 16.7)", t);
    while (squid_tilt_frame(s.tilt) != 5 && t < 400) { s.y = 100 * Q16_ONE; squid_swim_step(&s, 0); t++; }
    CHECK(NEAR(t, (45 + 90 - 11) / 1.5, 3), "nose straight down after %d frames", t);
    for (int f = 0; f < 6; f++) {
        static const int ang[6] = TILT_ANGLES;
        s.tilt = ang[f] * TILT_ONE;
        box bx = squid_box(&s);
        CHECK(bx.x1 - bx.x0 == w[f] && bx.y1 - bx.y0 == h[f], "box of frame %d is %dx%d", f, bx.x1 - bx.x0, bx.y1 - bx.y0);
        CHECK(abs((bx.x0 + bx.x1) - 2 * SQUID_X) <= 1, "box centred on x");
    }
    CHECK(NEAR(16, 34 * K_H, 1.0) && NEAR(12, 24 * K_V, 0.7), "level box = the reference bird (34x24) scaled");
}

static void test_obstacles(void)
{
    printf("obstacles\n");
    rs_rng r;
    rs_rng_seed(&r, 12345);
    int lo = 1000, hi = -1, hist[4] = {0};
    for (int i = 0; i < 10000; i++) {
        obstacle o = obstacle_make(&r, i);
        if (o.gap_top < lo) lo = o.gap_top;
        if (o.gap_top > hi) hi = o.gap_top;
        hist[(o.gap_top - GAP_TOP_MIN) * 4 / GAP_TOP_RANGE]++;
        if (o.x != FIRST_OBST_X + i * SPACING || o.x % 8) { CHECK(0, "obstacle %d at x %d", i, o.x); break; }
        if (o.theme != (i / 10) % 4) { CHECK(0, "obstacle %d theme %d", i, o.theme); break; }
    }
    CHECK(lo == GAP_TOP_MIN && hi == GAP_TOP_MIN + GAP_TOP_RANGE - 1, "gap tops in [%d, %d]", lo, hi);
    for (int k = 0; k < 4; k++) CHECK(hist[k] > 2200 && hist[k] < 2800, "uniform: quarter %d has %d", k, hist[k]);
    CHECK(GAP_TOP_MIN + GAP_TOP_RANGE - 1 + GAP < SEABED_Y - 8, "the bottom part is never empty");
}

static void test_collision(void)
{
    printf("collision\n");
    obstacle o = {100, 60, 0, 0};
    box b = {90, 60, 106, 72};                /* inside the gap, overlapping the column */
    CHECK(!box_hits_obstacle(b, &o, 0), "in the gap");
    b.y0 = 59; b.y1 = 71;
    CHECK(box_hits_obstacle(b, &o, 0), "1 px into the top part");
    b.y0 = 96; b.y1 = 108;                    /* gap bottom = 108 */
    CHECK(!box_hits_obstacle(b, &o, 0), "touching the bottom edge");
    b.y0 = 97; b.y1 = 109;
    CHECK(box_hits_obstacle(b, &o, 0), "1 px into the bottom part");
    b.y0 = 20; b.y1 = 32; b.x0 = 84; b.x1 = 100;
    CHECK(!box_hits_obstacle(b, &o, 0), "just left of the column");
    b.x0 = 85; b.x1 = 101;
    CHECK(box_hits_obstacle(b, &o, 0), "1 px into the column");
    b.x0 = 124; b.x1 = 140;
    CHECK(!box_hits_obstacle(b, &o, 0), "just right of the column");
    CHECK(box_hits_obstacle(b, &o, -1), "the scroll offset moves the column");
    b.y0 = 180; b.y1 = SEABED_Y;
    CHECK(box_hits_seabed(b), "touching the seabed");
    b.y1 = SEABED_Y - 1;
    CHECK(!box_hits_seabed(b), "just above the seabed");
    /* the surface: flapping all the time never leaves the water nor kills */
    squid s;
    squid_reset(&s, SQUID_X);
    s.state = SQ_SWIM;
    for (int i = 0; i < 300; i++) {
        squid_swim_step(&s, 1);
        box bx = squid_box(&s);
        if (bx.y0 < SURFACE_Y) { CHECK(0, "above the surface at frame %d", i); break; }
    }
    CHECK(squid_box(&s).y0 == SURFACE_Y, "held at the surface");
}

/* a perfect pilot: the squid is put in the middle of the next gap every frame */
static void test_scoring(void)
{
    printf("scoring\n");
    world w;
    world_init(&w, 1, 99, 0);
    w.fixed_seed = 1;
    int flap[2] = {1, 0}, none[2] = {0, 0};
    world_step(&w, flap);
    CHECK(w.started && w.sq[0].state == SQ_SWIM, "the first flap starts the run");
    int prev = 0, bad = 0;
    for (int f = 1; f < 72 * 60; f++) {
        const obstacle *o = world_next_obstacle(&w, 0);
        w.sq[0].y = (o->gap_top + GAP / 2) * Q16_ONE;
        w.sq[0].vy = 0;
        w.sq[0].tilt = 0;
        world_step(&w, none);
        if (w.sq[0].state != SQ_SWIM) { CHECK(0, "the pilot died at frame %d", f); break; }
        int sx = world_scroll_px(&w);
        int expect = sx + SQUID_X >= FIRST_OBST_X + OBST_W / 2 ? (sx + SQUID_X - FIRST_OBST_X - OBST_W / 2) / SPACING + 1 : 0;
        if (w.sq[0].score != expect) bad++;
        if (w.sq[0].score > prev + 1) bad++;
        if (w.sq[0].score != prev && !(w.events[0] & EV_SCORE)) bad++;
        prev = w.sq[0].score;
    }
    CHECK(!bad, "%d frames with a wrong score", bad);
    printf("  %d points in %d frames (one per obstacle, when the centre passes the centre)\n", prev, 72 * 60 - 1);
    CHECK(prev == 57, "57 obstacles passed in 72 s (got %d)", prev);
}

static void test_death_and_determinism(void)
{
    printf("death and determinism\n");
    world w;
    world_init(&w, 1, 5, 0);
    int flap[2] = {1, 0}, none[2] = {0, 0};
    world_step(&w, flap);
    int t = 0, landed = 0;
    while (t < 600 && w.sq[0].state != SQ_REST) {
        world_step(&w, none);
        t++;
        if (w.events[0] & EV_LAND) landed = 1;
    }
    CHECK(w.sq[0].state == SQ_REST && landed && w.sq[0].score == 0, "no input: sinks to the seabed with 0 (%d frames)", t);
    CHECK(!world_running(&w), "the scroll stops");
    /* the same inputs, the same run */
    uint32_t h[2];
    for (int k = 0; k < 2; k++) {
        world_init(&w, 1, 777, 0);
        uint32_t hash = 2166136261u;
        for (int f = 0; f < 3000; f++) {
            int in[2] = {(f % 23) == 0 || (f % 37) == 5, 0};
            world_step(&w, in);
            hash = (hash ^ (uint32_t)w.sq[0].y ^ (uint32_t)w.scroll ^ (uint32_t)w.sq[0].score) * 16777619u;
            for (int i = 0; i < w.nob; i++) hash = (hash ^ (uint32_t)w.ob[i].gap_top) * 16777619u;
        }
        h[k] = hash;
    }
    CHECK(h[0] == h[1], "two identical runs: %08x %08x", h[0], h[1]);
}

int main(void)
{
    test_constants();
    test_arc();
    test_flap_sets_speed();
    test_fall_time();
    test_tilt_and_box();
    test_obstacles();
    test_collision();
    test_scoring();
    test_death_and_determinism();
    printf("leadysquid physics: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
