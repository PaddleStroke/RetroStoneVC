/*
 * Nutty Branch: unit tests of the rules (play.c, gen.c, road.c; no video or sound).
 *   - the balance: the pull against the angle, the slip threshold, the feel targets of DESIGN.md, the curve force,
 *     the sap, the jump arcs;
 *   - the generator's FAIRNESS: for thousands of seeds and start distances, a planner with human limits must find
 *     a way through every stretch it generates (a dynamic programme over frames x angles, running and jumping);
 *   - determinism (1 and 4 players), the projection's sanity.
 *   test_physics [seeds] [metres per seed]
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
 */
#include "game.h"
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

static double deg(int32_t a) { return a / 4096.0; }

static squirrel fresh(int32_t ang, int32_t speed)
{
    squirrel q;
    memset(&q, 0, sizeof q);
    q.state = SQ_RUN;
    q.ang = ang;
    q.speed = speed;
    return q;
}

/* frames until |ang| > SLIP_ANG (or max) with a constant input */
static int frames_to(squirrel q, int steer, int32_t curve, int wind, int sap, int32_t limit, int max)
{
    for (int t = 1; t <= max; t++) {
        sq_physics(&q, steer, curve, wind, sap);
        if (limit > 0 ? (q.ang > limit || q.ang < -limit) : (q.ang < -limit)) return t;
    }
    return max + 1;
}

static void test_balance(void)
{
    printf("balance\n");
    for (int d = -90; d <= 90; d += 15) {
        double s = sin(d * M_PI / 180);
        CHECK(fabs(sin_q14(ANG(d)) / 16384.0 - s) < 0.001, "sin(%d) = %.4f", d, sin_q14(ANG(d)) / 16384.0);
    }
    /* the pull grows with the angle (proportional to sin), is zero on top, pushes away from it */
    int32_t prev = -1;
    for (int d = 0; d <= 62; d += 2) {
        squirrel q = fresh(ANG(d), SPEED0);
        int32_t a = sq_angacc(&q, 0, 0, 0, 0);
        double want = deg(PULL) * sin(d * M_PI / 180);
        CHECK(fabs(deg(a) - want) < 0.002, "pull at %d deg: %.4f, want %.4f", d, deg(a), want);
        CHECK(a >= prev, "the pull grows with the angle (%d deg)", d);
        squirrel m = fresh(-ANG(d), SPEED0);
        CHECK(sq_angacc(&m, 0, 0, 0, 0) == -a, "the pull is symmetric (%d deg)", d);
        prev = a;
    }
    {
        squirrel q = fresh(SLIP_ANG, SPEED0);
        int32_t pull = sq_angacc(&q, 0, 0, 0, 0);
        printf("  pull at the slip angle (%.0f deg): %.3f deg/f^2 = %.0f%% of the steer (%.3f)\n", deg(SLIP_ANG),
               deg(pull), 100.0 * pull / STEER_ACC, deg(STEER_ACC));
        CHECK(pull < STEER_ACC, "the steer beats the pull up to the slip angle");
    }
    /* the feel targets */
    int t = frames_to(fresh(ANG(2), SPEED0), 0, 0, 0, 0, SLIP_ANG, 600);
    printf("  a 2-degree nudge, no input: slips after %d frames (%.2f s)\n", t, t / 60.0);
    CHECK(t >= 90 && t <= 150, "a 2-degree nudge slips in 1.5-2.5 s: %d frames", t);
    t = frames_to(fresh(0, SPEED0), 1, 0, 0, 0, ANG(45), 600);
    printf("  full steer from the top to 45 degrees: %d frames\n", t);
    CHECK(t >= 18 && t <= 32, "full steer to 45 degrees in about 0.4 s: %d frames", t);
    {
        squirrel q = fresh(0, SPEED0);
        for (int i = 0; i < 12; i++) sq_physics(&q, 1, 0, 0, 0);
        int32_t v0 = q.angv;
        int half = 0;                       /* the slide alone (back on the top each frame: no pull) */
        while (q.angv > v0 / 2 && half < 100) { q.ang = 0; sq_physics(&q, 0, 0, 0, 0); half++; }
        printf("  letting go at %.2f deg/f: the slide halves in %d frames\n", deg(v0), half);
        CHECK(half >= 5 && half <= 14, "momentum lasts ~12 frames: halves in %d", half);
        squirrel r = fresh(0, SPEED0);
        for (int i = 0; i < 200; i++) sq_physics(&r, 1, 0, 0, 0);
        (void)r;
    }
    {
        squirrel q = fresh(ANG(55), SPEED0);
        int ok = 0;
        for (int i = 0; i < 120 && !ok; i++) {
            sq_physics(&q, -1, 0, 0, 0);
            if (q.ang < ANG(30)) ok = 1;
            if (q.ang > SLIP_ANG) break;
        }
        CHECK(ok, "at 55 degrees full steer brings it back (%.1f)", deg(q.ang));
        squirrel s = fresh(ANG(55), SPEED0);
        int slipped = 0;
        for (int i = 0; i < 120 && !slipped; i++) {
            sq_physics(&s, -1, 0, 0, 1);
            if (s.ang > SLIP_ANG) slipped = 1;
        }
        CHECK(slipped, "on sap at 55 degrees it slips anyway (%.1f)", deg(s.ang));
        int hold = 0;
        for (int d = 0; d < 62; d++) {
            squirrel h = fresh(ANG(d), SPEED0);
            if (sq_angacc(&h, -1, 0, 0, 1) < 0) hold = d;
        }
        printf("  on sap the steer holds up to %d degrees (no sap: the slip angle)\n", hold);
        CHECK(hold >= 15 && hold <= 30, "sap: holdable up to 15-30 degrees: %d", hold);
    }
    /* the curve: a centrifugal push to the outside, by speed squared */
    {
        squirrel q = fresh(0, SPEED_MAX);
        int32_t c = sq_angacc(&q, 0, CURVE_HARD, 0, 0);
        printf("  a hard right curve at top speed: %.3f deg/f^2 (%.0f%% of the steer)\n", deg(c), -100.0 * c / STEER_ACC);
        CHECK(c < 0, "a right curve pushes to the left");
        CHECK(-c > STEER_ACC * 30 / 100 && -c < STEER_ACC * 45 / 100, "hard curve at top speed ~37%% of the steer");
        squirrel s = fresh(0, SPEED0);
        int32_t c0 = sq_angacc(&s, 0, CURVE_HARD, 0, 0);
        CHECK(c0 * 4 > c - 8 && c0 * 4 < c + 8, "x speed^2: %d at speed 5, %d at speed 10", c0, c);
        squirrel l = fresh(0, SPEED_MAX);
        CHECK(sq_angacc(&l, 0, -CURVE_HARD, 0, 0) == -c, "left curves mirror right ones");
        /* holding at 45 degrees on the outside of a hard curve at top speed is still possible */
        squirrel o = fresh(-ANG(45), SPEED_MAX);
        CHECK(sq_angacc(&o, 1, CURVE_HARD, 0, 0) > 0, "45 degrees outside a hard curve at top speed: holdable");
    }
    /* in the air: no pull, half the steer */
    {
        squirrel q = fresh(ANG(40), SPEED0);
        q.air = 1;
        CHECK(sq_angacc(&q, 0, CURVE_HARD, 0, 0) == 0, "no pull and no curve force in the air");
        CHECK(sq_angacc(&q, 1, 0, 0, 0) == STEER_ACC * AIR_STEER_256 / 256, "half the steer in the air");
    }
}

static void arc(int held, int32_t *peak, int *air)
{
    squirrel q = fresh(0, SPEED0);
    q.air = 1;
    q.vh = JUMP_VH;
    *peak = 0;
    *air = 0;
    while (1) {
        sq_jump_step(&q, held);
        (*air)++;
        if (q.h > *peak) *peak = q.h;
        if (q.h <= 0 || *air > 200) break;
    }
}

static int32_t tap_peak, held_peak;
static int tap_air, held_air;

static void test_jump(void)
{
    printf("jump\n");
    arc(0, &tap_peak, &tap_air);
    arc(1, &held_peak, &held_air);
    printf("  tap: %.1f units high, %d frames; held: %.1f units, %d frames\n", tap_peak / 256.0, tap_air,
           held_peak / 256.0, held_air);
    CHECK(tap_peak > TWIG_H && tap_peak > CONE_H && tap_peak > OWL_H, "a tap clears twigs, cones, caterpillars, owls");
    CHECK(tap_peak < KNOT_H, "a tap does not clear a knot");
    CHECK(held_peak > KNOT_H, "a held jump clears a knot");
    CHECK(tap_air >= 26 && tap_air <= 34, "tap air time ~30 frames: %d", tap_air);
    CHECK(held_air >= 38 && held_air <= 48, "held air time ~43 frames: %d", held_air);
    CHECK(GAP_FRAMES_MAX + 8 <= held_air, "the longest gap leaves 8 frames of take-off timing to a held jump");
    CHECK(CONE_LEAD * 256 / SPEED_MAX >= REACT_FRAMES + 20, "a pinecone's shadow shows %d frames before it is reached",
          CONE_LEAD * 256 / SPEED_MAX);
}

/* a world step with scripted inputs */
static void script(world *w, int f, sq_input in[MAX_PLAYERS])
{
    memset(in, 0, sizeof(sq_input) * MAX_PLAYERS);
    for (int p = 0; p < w->players; p++) {
        const squirrel *q = &w->q[p];
        /* a simple balancer: steer back toward a wandering target */
        int32_t target = ((f / 90 + p) % 3 - 1) * ANG(15);
        int32_t e = target - q->ang - q->angv * 8;
        in[p].steer = e > ANG(1) ? 1 : e < -ANG(1) ? -1 : 0;
        in[p].jump_pressed = (f + p * 7) % 53 == 0;
        in[p].jump_held = (f + p * 7) % 53 < 10;
    }
}

static void test_world(void)
{
    printf("world\n");
    static world a, b;
    for (int players = 1; players <= 4; players += 3) {
        world_init(&a, players, 1234);
        world_init(&b, players, 1234);
        world_start(&a);
        world_start(&b);
        sq_input in[MAX_PLAYERS];
        int f;
        for (f = 0; f < 3000 && world_running(&a); f++) {
            script(&a, f, in);
            world_step(&a, in);
            script(&b, f, in);
            world_step(&b, in);
        }
        CHECK(memcmp(&a, &b, sizeof a) == 0, "determinism: %d player(s), %d frames, the same world twice", players, f);
        printf("  %d player(s): %d frames, %d m, score %d\n", players, f, a.q[0].dist_m, a.q[0].score);
    }
    /* a squirrel left alone slips off */
    world_init(&a, 1, 77);
    world_start(&a);
    a.q[0].ang = ANG(3);
    sq_input none[MAX_PLAYERS];
    memset(none, 0, sizeof none);
    int f = 0;
    while (a.q[0].state == SQ_RUN && f < 1000) { world_step(&a, none); f++; }
    CHECK(a.q[0].state == SQ_FALL && a.q[0].fall_kind == FALL_SLIP, "left alone it slips off (after %d frames)", f);
    /* the projection: a straight flat branch narrows toward the horizon, lines below the squirrel are filled */
    world_init(&a, 1, 5);
    static road_view v;
    road_project(&a, &v);
    int last_z = 0, ok = 1, filled = 0;
    for (int y = RS_SCREEN_H - 1; y >= 0; y--) {
        if (v.ln[y].row == ROW_NONE) continue;
        filled++;
        if (v.ln[y].z < last_z) ok = 0;
        last_z = v.ln[y].z;
    }
    CHECK(ok, "z grows toward the horizon");
    CHECK(filled > 120 && v.ln[RS_SCREEN_H - 1].row != ROW_NONE, "the branch fills the lower screen (%d lines)", filled);
    int sx, sy, sc, hid;
    road_point(&a, &v, a.q[0].s, 0, 0, 0, &sx, &sy, &sc, &hid);
    printf("  the squirrel's feet at (%d, %d), %.2f px per unit; the branch top at line %d\n", sx, sy, sc / 256.0, v.top);
    CHECK(sx >= 150 && sx <= 170 && sy >= 180 && sy <= 200, "the leader stands near (160, 192)");
}

/* ---- the fairness planner ----------------------------------------------------------------------------------------- */
#define BINS 61                                     /* -60..60 degrees, 2 degrees each */
#define BIN_ANG(b) ANG(((b) - 30) * 2)
#define PLAN_MAX 9000
#define NCLS 10
static const int32_t CLS_H[NCLS] = {CATER_H, SNAKE_H, TWIG_H, OWL_H, CONE_H, KNOT_H, SWING_H, WEB_H, BEE_H, Q8(1000)};   /* 1000: no floor */
typedef uint64_t mask;
static mask cls[PLAN_MAX][NCLS];                    /* bins blocked by a thing of each height (the last: no floor) */
static mask holdm[PLAN_MAX], forkb[PLAN_MAX];
static mask reach[PLAN_MAX + 64];
static uint8_t busy[PLAN_MAX];
static int32_t arc_tap[64], arc_held[64];
static int n_tap, n_held;

static int cls_of(int32_t h)
{
    for (int k = 0; k < NCLS; k++)
        if (CLS_H[k] == h) return k;
    return -1;
}

static void arcs(void)
{
    for (int held = 0; held < 2; held++) {
        squirrel q = fresh(0, SPEED0);
        q.air = 1;
        q.vh = JUMP_VH;
        int n = 0;
        int32_t *A = held ? arc_held : arc_tap;
        while (n < 63) {
            sq_jump_step(&q, held);
            A[++n] = q.h;
            if (q.h <= 0) break;
        }
        if (held) n_held = n; else n_tap = n;
    }
}

/* above height h at bin b, frame f: blocked? */
static mask blocked_above(int f, int32_t h)
{
    mask m = 0;
    for (int k = 0; k < NCLS - 1; k++)
        if (CLS_H[k] > h) m |= cls[f][k];
    return m;
}

typedef struct plan_result { int ok, frames, fail_f; int32_t fail_m; } plan_result;

/* the planner's own check: an object inserted in order (an unfair trap) */
static int inject_kind;
static void inject(world *w, int kind, int32_t s, int32_t ang, int32_t len)
{
    obj o;
    memset(&o, 0, sizeof o);
    o.kind = (uint8_t)kind;
    o.s = s;
    o.ang = ang;
    o.len = len;
    o.t0 = -1;
    int i = w->ob_count;
    while (i > 0 && ob_get(w, i - 1)->s > s) {
        w->ob[(w->ob_first + i) % OB_MAX] = *ob_get(w, i - 1);
        i--;
    }
    w->ob[(w->ob_first + i) % OB_MAX] = o;
    w->ob_count++;
}

static plan_result plan(uint32_t seed, int start_m, int metres)
{
    static world w;
    world_init_at(&w, 1, seed, start_m);
    int32_t s_in = start_m * UNITS_PER_M + 900;
    if (inject_kind == OB_GAP) inject(&w, OB_GAP, s_in, 0, course_speed(start_m) * 60 / 256);
    if (inject_kind == OB_WEB)
        for (int a = -60; a <= 60; a += 20) inject(&w, OB_WEB, s_in, ANG(a), 0);
    w.ghost = 1;
    world_start(&w);
    sq_input none[MAX_PLAYERS];
    memset(none, 0, sizeof none);
    int N = 0;
    int32_t end_s = (start_m + metres) * UNITS_PER_M;
    int32_t s_prev = w.q[0].s;
    while (N < PLAN_MAX - 64 && (w.q[0].s >> 8) < end_s) {
        squirrel *q = &w.q[0];
        int32_t s = q->s >> 8;
        memset(cls[N], 0, sizeof cls[N]);
        holdm[N] = forkb[N] = 0;
        busy[N] = 0;
        int32_t curve = seg_curve(&w, s);
        for (int b = 0; b < BINS; b++) {
            int32_t ang = BIN_ANG(b);
            mask bit = (mask)1 << b;
            if (!floor_at(&w, s, ang, 0)) cls[N][NCLS - 1] |= bit;
            int sap = grip_at(&w, s, ang);
            for (int i = 0; i < w.ob_count; i++) {
                const obj *o = ob_get(&w, i);
                if (o->s > s + 32) break;
                int32_t ds = s - o->s;

                if (o->kind == OB_WEB && labs(ds) <= SQ_HALF_LEN + OB_LEN / 2 && labs(ang - o->ang) < WEB_HALF + SQ_HALF_ANG)
                    cls[N][cls_of(WEB_H)] |= bit;
                int32_t oa, half, hh = ob_solid(&w, o, &oa, &half);
                if (hh && labs(ds) <= SQ_HALF_LEN + OB_LEN / 2 && labs(ang - oa) < half + SQ_HALF_ANG) {
                    int k = cls_of(hh);
                    if (k >= 0) cls[N][k] |= bit;
                }
            }
            /* holdable: the steer (with a margin: a human's 75%) beats the forces there */
            squirrel h = fresh(ang, w.speed);
            int32_t acc = sq_angacc(&h, 0, curve, q->wind, sap);
            int32_t grip = sap ? STEER_ACC * SAP_GRIP_256 / 256 : STEER_ACC;
            if (labs(acc) <= grip * 3 / 4) holdm[N] |= bit;
        }
        for (int k = 0; k < NCLS; k++) busy[N] |= cls[N][k] != 0;
        /* a fork's split: the dead side (and the crest, ambiguous) is not taken */
        for (int i = 0; i < w.ob_count; i++) {
            const obj *o = ob_get(&w, i);
            if (o->kind == OB_FORK && (s_prev >> 8) < o->s + 16 && s >= o->s - 16)
                for (int b = 0; b < BINS; b++) {
                    int32_t ang = BIN_ANG(b);
                    int side = ang > ANG(1) ? 1 : ang < -ANG(1) ? -1 : 0;
                    if (side == 0 || side == o->side) forkb[N] |= (mask)1 << b;
                }
        }
        s_prev = q->s;
        world_step(&w, none);
        N++;
    }
    /* the dynamic programme: running (one bin per frame at most) and jumping (tap or held, no steering in the air) */
    memset(reach, 0, sizeof(mask) * (N + 64));
    reach[0] = (mask)1 << 30;
    mask all = ((mask)1 << BINS) - 1;
    plan_result r = {1, N, -1, 0};
    int beyond = 0;
    for (int f = 0; f + 1 < N; f++) {
        mask R = reach[f];
        if (!R) continue;                            /* in the air (a jump lands later), or stuck */
        r.fail_f = f;
        mask ground = 0;
        for (int k = 0; k < NCLS; k++) ground |= cls[f + 1][k];
        mask run = (R | (R << 1) | (R >> 1)) & all & ~ground & ~forkb[f + 1] & holdm[f + 1];
        reach[f + 1] |= run;
        /* jumps: only worth checking where something is coming */
        int any = 0;
        for (int i = 1; i <= n_held && f + i < N; i++) any |= busy[f + i];
        if (!any) continue;
        for (int held = 0; held < 2; held++) {
            int n = held ? n_held : n_tap;
            const int32_t *A = held ? arc_held : arc_tap;
            mask J = R;
            for (int i = 1; i < n && f + i < N && J; i++) J &= ~blocked_above(f + i, A[i]) & ~forkb[f + i];
            if (f + n >= N) { beyond |= J != 0; continue; }     /* lands after the planned stretch */
            mask land = 0;
            for (int k = 0; k < NCLS; k++) land |= cls[f + n][k];
            J &= ~land & holdm[f + n];
            reach[f + n] |= J;
        }
    }
    r.ok = reach[N - 1] != 0 || beyond;
    r.fail_f++;
    if (!r.ok && getenv("NB_DEBUG")) {
        static const char *KN[] = {"-", "acorn", "hazel", "walnut", "twig", "knot", "web", "cater", "cone", "owl",
                                   "pecker", "sap", "gap", "wind", "fork"};
        static world d;
        world_init_at(&d, 1, seed, start_m);
        d.ghost = 1;
        world_start(&d);
        for (int f = 0; f < r.fail_f - 70 && f < N; f++) world_step(&d, none);
        int32_t s0 = d.q[0].s >> 8;
        for (int i = 0; i < d.ob_count; i++) {
            const obj *o = ob_get(&d, i);
            if (o->s < s0 - 64 || o->s > s0 + 900 || o->kind <= OB_WALNUT) continue;
            printf("    %-6s s=%d (+%d) ang=%.0f len=%d side=%d\n", KN[o->kind], o->s, o->s - s0, o->ang / 4096.0, o->len, o->side);
        }
        for (int f = r.fail_f - 70; f <= r.fail_f + 2 && f < N; f++) {
            if (f < 0) continue;
            char line[BINS + 1], g[BINS + 1];
            mask ground = 0;
            for (int k = 0; k < NCLS; k++) ground |= cls[f][k];
            for (int b = 0; b < BINS; b++) {
                line[b] = (reach[f] >> b) & 1 ? 'o' : '.';
                g[b] = (cls[f][NCLS - 1] >> b) & 1 ? '_' : (ground >> b) & 1 ? '#' : (holdm[f] >> b) & 1 ? ' ' : '~';
            }
            line[BINS] = g[BINS] = 0;
            printf("    f%5d %s |%s|\n", f, line, g);
        }
    }
    if (!r.ok) r.fail_m = start_m + (int32_t)((int64_t)r.fail_f * course_speed(start_m) / 256 / UNITS_PER_M);
    return r;
}

static void test_fairness(int seeds, int metres)
{
    printf("generator fairness: %d seeds x %d m, start distances 0..2400 m\n", seeds, metres);
    arcs();
    int bad = 0;
    long frames = 0;
    for (int i = 0; i < seeds; i++) {
        uint32_t seed = 0x9e3779b9u * (uint32_t)(i + 1);
        int start = (i * 97) % 2400;
        plan_result r = plan(seed, start, metres);
        frames += r.frames;
        if (!r.ok) {
            if (bad < 8) printf("  FAIL seed %u start %d m: no way through after frame %d (about %d m)\n", seed, start,
                                r.fail_f, r.fail_m);
            bad++;
        }
    }
    printf("  %d of %d stretches passable (%ld frames planned)\n", seeds - bad, seeds, frames);
    /* the planner itself: an unfair trap must be caught */
    inject_kind = OB_GAP;
    CHECK(!plan(7, 300, 120).ok, "the planner refuses a gap of 60 frames");
    inject_kind = OB_WEB;
    CHECK(!plan(7, 300, 120).ok, "the planner refuses a wall of webs across the branch");
    inject_kind = 0;
    CHECK(bad == 0, "every generated stretch is survivable with human limits (%d unfair)", bad);
}

int main(int argc, char **argv)
{
    int seeds = argc > 1 ? atoi(argv[1]) : 2000, metres = argc > 2 ? atoi(argv[2]) : 300;
    test_balance();
    test_jump();
    test_world();
    test_fairness(seeds, metres);
    printf("nuttybranch rules: %d checks, %s\n", checks, fails ? "FAILED" : "all passed");
    return fails ? 1 : 0;
}
