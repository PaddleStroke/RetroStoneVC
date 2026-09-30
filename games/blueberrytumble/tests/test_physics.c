/*
 * Blueberry Tumble: unit tests of the rules against the tuning table and the feel sources (course.c, physics.c,
 * patterns.c, director.c; no video or sound).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 */
#include "bt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))

static double px(int64_t q) { return q / 65536.0; }
static bt_course C;

/* a flat test course at one tier; objects placed by the tests */
static void flat(int tier)
{
    course_reset(&C, tier);
    course_place_flat(&C, 200, PAT_START);
}
static void put(int col, int row, int k) { course_col_w(&C, col)->cell[row] = (uint8_t)k; }
static void setground(int col, int g) { course_col_w(&C, col)->ground = (uint8_t)g; }

/* run a berry from x = 0 at frame 0, A pressed on the frames in `press` (held for `hold` frames) */
typedef struct run_t { int die_frame, cause, jumps, lands, orbs, pads, grows, shrinks, glides, smash; int32_t die_x; } run_t;
static run_t run(berry *b, int frames, int press_at, int hold)
{
    run_t r;
    memset(&r, 0, sizeof r);
    r.die_frame = -1;
    for (int f = 0; f < frames; f++) {
        int held = press_at >= 0 && f >= press_at && f < press_at + hold;
        int64_t x = course_x(&C, f + 1);
        step_info in;
        int ev = berry_step(b, &C, x, x, held, &in);
        if (ev & EV_JUMP) r.jumps++;
        if (ev & EV_LAND) r.lands++;
        if (ev & EV_ORB) r.orbs++;
        if (ev & EV_PAD) r.pads++;
        if (ev & EV_GROW) r.grows++;
        if (ev & EV_SHRINK) r.shrinks++;
        if (ev & EV_GLIDE) r.glides++;
        if (ev & EV_SMASH) r.smash++;
        if (ev & EV_DIE) { r.die_frame = f + 1; r.cause = b->cause; r.die_x = b->x; break; }
    }
    return r;
}

static void test_constants(void)
{
    printf("the tuning against the sources\n");
    static const int bpm100[NTIERS] = TIER_BPM_X100, T[NTIERS] = TIER_MOD_TEMPO, S[NTIERS] = TIER_MOD_SPEED;
    for (int t = 0; t < NTIERS; t++) {
        int num = tier_fpb_num(t), den = tier_fpb_den(t);
        /* 64 px per beat, exactly */
        CHECK(llabs((long long)tier_speed(t) * num - (long long)BEAT_PX * Q16_ONE * den) < num,
              "tier %d: speed x frames per beat = 64 px", t);
        CHECK(NEAR(3600.0 * den / num, bpm100[t] / 100.0, 0.01), "tier %d: %d/%d frames per beat = %.2f BPM", t, num, den,
              bpm100[t] / 100.0);
        /* the MOD: BPM = 6 T / S, a whole tick of samples, a whole number of frames per biome */
        CHECK(6LL * T[t] * num == 3600LL * den * S[t], "tier %d: MOD tempo %d speed %d gives the tier's BPM", t, T[t], S[t]);
        CHECK(80000 % T[t] == 0, "tier %d: a whole tick (80000 / %d samples)", t, T[t]);
        CHECK((BIOME_CELLS / BEAT_CELLS * num) % den == 0, "tier %d: a biome lasts a whole number of frames", t);
    }
    double slow = px(tier_speed(0)) * 60 / CELL, normal = px(tier_speed(2)) * 60 / CELL;
    CHECK(NEAR(slow, REF_SPEED_SLOW, 0.3), "tier 0: %.2f blocks/s vs the reference slow %.2f", slow, REF_SPEED_SLOW);
    CHECK(NEAR(normal, REF_SPEED_NORMAL, 0.45), "tier 2: %.2f blocks/s vs the reference normal %.2f", normal, REF_SPEED_NORMAL);
    CHECK(NEAR(REF_AIR_FRAMES, JUMP_FRAMES, 0.5), "the reference jump lasts %.1f frames, ours %d", REF_AIR_FRAMES, JUMP_FRAMES);
    CHECK(BIOME_CELLS % BAR_CELLS == 0 && BEAT_PX == 64, "biomes on bars, 64 px per beat");
}

static void test_clock(void)
{
    printf("the frame clock\n");
    course_reset(&C, -1);
    int32_t f = 0;
    int64_t x = 0;
    for (int b = 0; b < 7; b++) {
        int t = b < NTIERS - 1 ? b : NTIERS - 1;
        f += biome_frames(t);
        x += BIOME_CELLS * CELL;
        CHECK(course_x(&C, f) == x * Q16_ONE, "biome %d ends at frame %d exactly at %d px (got %.3f)", b, f, (int)x, px(course_x(&C, f)));
    }
    int mono = 1;
    for (int32_t i = 0; i < 20000; i++)
        if (course_x(&C, i + 1) <= course_x(&C, i)) mono = 0;
    CHECK(mono, "x always increases");
    CHECK(course_tier_of_col(&C, 0) == 0 && course_tier_of_col(&C, 384) == 1 && course_tier_of_col(&C, 5000) == 4,
          "tiers by biome");
    CHECK(course_biome_of_col(1536) == 0 && course_loop_of_col(1536) == 1, "the night loop starts at 1536 m");
}

static void test_jump(void)
{
    printf("the jump\n");
    for (int t = 0; t < NTIERS; t++) {
        flat(t);
        berry b;
        berry_reset(&b, 0);
        int32_t top = 0;
        int apex_f = 0, land_f = -1;
        for (int f = 0; f < 40; f++) {
            int64_t x = course_x(&C, f + 1);
            int ev = berry_step(&b, &C, x, x, f < 3, NULL);
            if (b.h > top) { top = b.h; apex_f = f + 1; }
            if ((ev & EV_LAND) && land_f < 0) land_f = f + 1;
        }
        if (t == 0) {
            CHECK(NEAR(px(top), JUMP_HEIGHT, 0.5), "apex %.2f px (%.2f blocks; the reference: a little over 2)", px(top), px(top) / CELL);
            CHECK(apex_f == 10 || apex_f == 11, "apex on frame %d (h(10) = h(11) = 35.0)", apex_f);
            CHECK(land_f == JUMP_FRAMES, "back on the ground on frame %d", land_f);
        }
        double len = px(course_x(&C, JUMP_FRAMES)) / CELL;
        printf("  tier %d: a jump covers %.2f blocks\n", t, len);
        if (t == 2) CHECK(NEAR(len, REF_JUMP_BLOCKS, 0.2), "tier 2 jump %.2f blocks vs the reference %.1f", len, REF_JUMP_BLOCKS);
    }
    /* pad and snow heights */
    flat(2);
    put(4, 0, K_PAD);
    berry b;
    berry_reset(&b, 0);
    int32_t top = 0;
    run_t r = run(&b, 1, -1, 0);
    for (int f = 1; f < 80; f++) {
        int64_t x = course_x(&C, f + 1);
        berry_step(&b, &C, x, x, 0, NULL);
        if (b.h > top) top = b.h;
    }
    (void)r;
    CHECK(NEAR(px(top), 72, 2.5), "a mushroom launches %.1f px (4.5 blocks)", px(top));
    flat(2);
    berry_reset(&b, 0);
    b.mode = M_SNOW;
    top = 0;
    for (int f = 0; f < 40; f++) {
        int64_t x = course_x(&C, f + 1);
        berry_step(&b, &C, x, x, f < 2, NULL);
        if (b.h > top) top = b.h;
    }
    CHECK(NEAR(px(top), 22, 1.0), "the snowberry jumps %.1f px (heavier: lower)", px(top));
    /* holding A on the ground jumps again on each landing */
    flat(2);
    berry_reset(&b, 0);
    r = run(&b, 200, 0, 200);
    CHECK(r.jumps >= 8 && r.jumps <= 10, "held for 200 frames: %d jumps (one every %d frames)", r.jumps, JUMP_FRAMES + 1);
    /* the start press does not jump until released */
    flat(2);
    berry_reset(&b, 0);
    b.need_release = 1;
    b.held = 1;
    r = run(&b, 30, 0, 30);
    CHECK(r.jumps == 0, "held from the start: no jump before a release (%d)", r.jumps);
}

static void test_collisions(void)
{
    printf("collisions\n");
    berry b;
    run_t r;
    /* a thorn: the hazard boxes meet when x > 16 c + 5 - 5 */
    flat(2);
    put(10, 0, K_THORN);
    berry_reset(&b, 0);
    r = run(&b, 200, -1, 0);
    CHECK(r.cause == D_HAZARD && px(r.die_x) > 160 && px(r.die_x) <= 160 + 2.7, "thorn: splat at x %.2f (edge 160)", px(r.die_x));
    /* the same thorn jumped over */
    flat(2);
    put(10, 0, K_THORN);
    berry_reset(&b, 0);
    int press = 0;
    while (px(course_x(&C, press)) < 160 - 26) press++;
    r = run(&b, 200, press, 3);
    CHECK(r.die_frame < 0, "thorn jumped over (press at x %.1f)", px(course_x(&C, press)));
    /* a wall: the core (+-3) hits it */
    flat(2);
    put(10, 0, K_BLOCK);
    berry_reset(&b, 0);
    r = run(&b, 200, -1, 0);
    CHECK(r.cause == D_WALL && px(r.die_x) > 157 && px(r.die_x) <= 157 + 2.7, "block: splat at x %.2f (edge 157)", px(r.die_x));
    /* landing on the block */
    flat(2);
    put(10, 0, K_BLOCK);
    put(11, 0, K_BLOCK);
    put(12, 0, K_BLOCK);
    berry_reset(&b, 0);
    press = 0;
    while (px(course_x(&C, press)) < 160 - 40) press++;
    int landed16 = 0;
    for (int f = 0; f < 90; f++) {
        int64_t x = course_x(&C, f + 1);
        berry_step(&b, &C, x, x, f >= press && f < press + 3, NULL);
        if (b.grounded && b.h == 16 * Q16_ONE) landed16 = 1;
    }
    CHECK(landed16 && !b.dead, "a jump lands on a 1-block rock and rolls off it");
    /* snap: a bottom 2 px below a top steps up onto it */
    flat(2);
    put(10, 0, K_BLOCK);
    berry_reset(&b, 155 * Q16_ONE);
    b.h = 14 * Q16_ONE;
    b.vy = 0;
    b.grounded = 0;
    berry_step(&b, &C, 156 * Q16_ONE, 156 * Q16_ONE, 0, NULL);
    CHECK(!b.dead && b.grounded && b.h == 16 * Q16_ONE, "snap up onto a top 2 px above (h %.1f)", px(b.h));
    /* a 1-cell crevasse is rolled over, a 3-cell one swallows */
    flat(2);
    setground(10, G_GAP);
    berry_reset(&b, 0);
    r = run(&b, 200, -1, 0);
    CHECK(r.die_frame < 0, "a 1-cell gap is rolled over");
    flat(2);
    for (int i = 10; i < 13; i++) setground(i, G_GAP);
    berry_reset(&b, 0);
    r = run(&b, 200, -1, 0);
    CHECK(r.die_frame > 0, "a 3-cell crevasse: splat (cause %d)", r.cause);
    /* ice: no jump on it */
    flat(2);
    for (int i = 4; i < 12; i++) setground(i, G_ICE);
    berry_reset(&b, 0);
    press = 0;
    while (px(course_x(&C, press)) < 100) press++;
    r = run(&b, press + 10, press, 5);
    CHECK(r.jumps == 0 && b.on_ice, "on ice: A does nothing (%d jumps)", r.jumps);
    /* a dew drop: a press in it jumps again, once */
    flat(2);
    put(12, 2, K_ORB);
    berry_reset(&b, 0);
    int orbs = 0, jumps = 0;
    press = 0;
    while (px(course_x(&C, press)) < 160) press++;
    for (int f = 0; f < 120; f++) {
        int64_t x = course_x(&C, f + 1);
        int held = (f >= press && f < press + 2) || (f >= press + 10 && f < press + 12) || (f >= press + 14 && f < press + 16);
        int ev = berry_step(&b, &C, x, x, held, NULL);
        if (ev & EV_ORB) orbs++;
        if (ev & EV_JUMP) jumps++;
    }
    CHECK(orbs == 1 && jumps == 1, "dew drop: one orb jump (%d) after one jump (%d)", orbs, jumps);
    /* a cone rolls in: splat; a snowberry smashes it */
    flat(2);
    C.cone[0].meet = 20 * CELL;
    C.cone[0].k256 = CONE_FAST;
    C.ncone = 1;
    berry_reset(&b, 0);
    r = run(&b, 300, -1, 0);
    CHECK(r.cause == D_HAZARD && NEAR(px(r.die_x), 320 - 10 / 1.5, 4), "a cone: splat near its meet point (x %.1f)", px(r.die_x));
    berry_reset(&b, 0);
    b.mode = M_SNOW;
    r = run(&b, 300, -1, 0);
    CHECK(r.die_frame < 0 && r.smash > 0, "a snowberry smashes the cone");
    /* snow and water */
    flat(2);
    for (int i = 5; i < 11; i++) setground(i, G_SNOW);
    put(14, 0, K_THORN);
    put(16, 0, K_PEBBLE);
    for (int i = 20; i < 23; i++) setground(i, G_WATER);
    put(26, 0, K_THORN);
    berry_reset(&b, 0);
    r = run(&b, 200, -1, 0);
    CHECK(r.grows == 1 && r.smash > 0 && r.shrinks == 1 && r.cause == D_HAZARD && px(r.die_x) > 26 * 16 - 1,
          "snow grows (%d), smashes (%d), water shrinks (%d), the small berry then dies on a thorn", r.grows, r.smash,
          r.shrinks);
    /* the leaf glider */
    flat(2);
    course_col_w(&C, 4)->flags |= F_LEAF;
    course_col_w(&C, 40)->flags |= F_LEAF_END;
    berry_reset(&b, 0);
    int32_t top = 0, vmax = 0;
    int glides = 0, ends = 0;
    for (int f = 0; f < 300; f++) {
        int64_t x = course_x(&C, f + 1);
        int ev = berry_step(&b, &C, x, x, f < 60, NULL);
        if (ev & EV_GLIDE) glides++;
        if (ev & EV_GLIDE_END) ends++;
        if (b.h > top) top = b.h;
        if (b.mode == M_GLIDE && b.vy > vmax) vmax = b.vy;
    }
    CHECK(glides == 1 && ends == 1 && vmax == GLIDE_VMAX && px(top) > 60 && !b.dead && b.mode == M_NORMAL && b.h == 0,
          "glide: held rises (top %.0f px, vmax %.2f), released sinks, the leaf drop ends it", px(top), px(vmax));
    flat(2);
    course_col_w(&C, 4)->flags |= F_LEAF;
    berry_reset(&b, 0);
    for (int f = 0; f < 200; f++) {
        int64_t x = course_x(&C, f + 1);
        berry_step(&b, &C, x, x, 1, NULL);
    }
    CHECK(!b.dead && b.h + BERRY_SIZE * Q16_ONE == GLIDE_CEIL * Q16_ONE, "glide: held, the berry stops at the ceiling");
}

static void test_patterns(void)
{
    printf("patterns\n");
    static pbuild pb;
    int total = 0, bad = 0;
    for (int p = 0; p < bt_npatterns; p++)
        for (int t = 0; t < NTIERS; t++)
            for (int k = 0; k < pat_ncombo(p, t); k++)
                for (int coin = 0; coin <= 1; coin++) {
                    pat_build(&pb, p, t, k, coin);
                    total++;
                    int ok = pb.len % BEAT_CELLS == 0 && pb.len > 0 && pb.len <= PAT_MAX_LEN;
                    for (int i = 0; i < pb.ncone; i++) ok &= pb.cone[i].meet % CELL == 0 && pb.cone[i].meet < pb.len * CELL;
                    /* the last cell is plain ground: the pattern ends on the ground */
                    ok &= pb.col[pb.len - 1].ground == G_GROUND;
                    if (!ok) { bad++; if (bad < 5) printf("  bad: %s tier %d combo %d\n", pat_name(p), t, k); }
                }
    CHECK(bad == 0, "%d of %d builds end on the ground on a whole beat", total - bad, total);
    CHECK(bt_npatterns >= 20, "%d patterns", bt_npatterns);
}

static void test_director(void)
{
    printf("the director\n");
    static bt_course a, b;
    uint32_t h1 = 0, h2 = 0, h3 = 0;
    for (int run = 0; run < 3; run++) {
        bt_course *c = run == 1 ? &b : &a;
        course_reset(c, -1);
        director_start(c, run == 2 ? 99 : 42);
        uint32_t h = 2166136261u;
        for (int32_t col = 0; col < 4000; col += 64) {
            director_fill(c, col + 128);
            for (int32_t i = col; i < col + 64; i++) {
                const bt_col *k = course_col(c, i);
                h = (h ^ k->ground ^ (uint32_t)k->cell[0] << 8 ^ (uint32_t)k->cell[1] << 16) * 16777619u;
            }
        }
        if (run == 0) h1 = h; else if (run == 1) h2 = h; else h3 = h;
    }
    CHECK(h1 == h2, "the same seed makes the same course");
    CHECK(h1 != h3, "another seed makes another course");
    /* every segment starts on a beat, biomes on bars with a gate */
    course_reset(&a, -1);
    director_start(&a, 7);
    int onbeat = 1, gates = 0;
    for (int32_t col = 0; col < 3000; col += 32) {
        director_fill(&a, col + 64);
    }
    for (int32_t i = 0; i < a.nseg && i < SEG_RING; i++) {
        const bt_seg *s = &a.seg[(a.nseg - 1 - i) % SEG_RING];
        if (s->col0 % BEAT_CELLS) onbeat = 0;
        if (s->pat == PAT_GATE) { gates++; if (s->col0 % BIOME_CELLS) onbeat = 0; }
    }
    CHECK(onbeat, "every pattern starts on a beat, every gate on a biome boundary");
}

int main(void)
{
    test_constants();
    test_clock();
    test_jump();
    test_collisions();
    test_patterns();
    test_director();
    printf("%s: %d/%d checks passed\n", fails ? "FAILED" : "physics tests passed", checks - fails, checks);
    return fails ? 1 : 0;
}
