/*
 * Llama Drama: the rules alone (src/play.c, no video, no sound, no screen width: this program links play.c with the
 * SDK's RNG only, so the rules cannot ask the width): the start, the aim (its range, its speed, the head's frames),
 * the spit (along the aim, the arc, the reload, 3 gobs at most, its reach), a gob popping a hornet (and the big one's
 * three hits), a hornet stinging a llama's head or neck (the run ends), the speed rising with the score, the ramp
 * (waves, kinds, a gentle start), the flight patterns (sine, zig-zag, the dive, the loop, the dart, pairs, swarms),
 * the widescreen equivalence (hornets appear beyond the widest screen, the spit's reach is fixed), the touch's
 * tap-to-aim, 4 llamas, determinism and the bot.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the SDK's RNG (play.c's only runtime calls), as sdk/src/rs_core.c: xorshift32 */
void rs_rng_seed(rs_rng *r, uint32_t s) { r->s = s ? s : 0x2545F491u; }
uint32_t rs_rng_next(rs_rng *r)
{
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return r->s = x;
}
int rs_rng_range(rs_rng *r, int n) { return n > 0 ? (int)(((uint64_t)rs_rng_next(r) * (uint32_t)n) >> 32) : 0; }

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
                           printf(__VA_ARGS__); printf("\n"); } } while (0)
#define PX(q) ((int)((q) >> 16))
#define Q(v) ((int32_t)(v) * 65536)

static pin in[MAX_PLAYERS];
static void idle(void) { memset(in, 0, sizeof in); }
static world W, W2;

/* a run started, no hornets yet (the first wave held back) */
static void playing(world *w, int players)
{
    world_init(w, players, 1234);
    idle();
    in[0].any = 1;
    world_step(w, in);
    idle();
    w->wave_t = 1 << 20;
}

static void steps(world *w, int n)
{
    for (int i = 0; i < n; i++) world_step(w, in);
}

static int gobs_up(const world *w, int p)
{
    int n = 0;
    for (int i = 0; i < GOBS_MAX; i++) n += w->gobs[p * GOBS_MAX + i].on;
    return n;
}

static int hornets(const world *w)
{
    int n = 0;
    for (int i = 0; i < MAX_HORNETS; i++) n += w->hn[i].on;
    return n;
}

/* a hornet already on the stage at (x, y), going for llama p */
static hornet *hornet_at(world *w, int kind, int pat, int x, int y)
{
    hornet *h = hornet_spawn(w, kind, pat, y, 0);
    h->x = Q(x);
    h->t = 0;
    return h;
}

static void test_start_and_aim(void)
{
    world_init(&W, 1, 1);
    idle();
    steps(&W, 30);
    CHECK(W.l[0].state == LS_READY && W.t == 0 && hornets(&W) == 0, "nothing moves before the first press");
    in[0].any = 1;
    world_step(&W, in);
    CHECK(W.l[0].state == LS_PLAY && (W.events[0] & EV_START), "a press starts the run");
    W.wave_t = 1 << 20;
    idle();
    CHECK(W.l[0].aim == AIM_START, "the aim starts at %d (%d)", AIM_START, W.l[0].aim);
    in[0].dy = -1;                                   /* UP */
    steps(&W, AIM_ACCEL);
    CHECK(W.l[0].aim == AIM_START + AIM_SLOW * AIM_ACCEL, "UP: %d a frame at first (%d)", AIM_SLOW, W.l[0].aim);
    steps(&W, 2);
    CHECK(W.l[0].aim == AIM_START + AIM_SLOW * AIM_ACCEL + 2 * AIM_FAST, "...then %d (%d)", AIM_FAST, W.l[0].aim);
    steps(&W, 200);
    CHECK(W.l[0].aim == AIM_MAX, "the aim stops at 55 degrees (%d)", W.l[0].aim);
    in[0].dy = 1;                                    /* DOWN */
    steps(&W, 1);
    CHECK(W.l[0].aim == AIM_MAX - AIM_SLOW, "turning back starts slowly again (%d)", W.l[0].aim);
    steps(&W, 200);
    CHECK(W.l[0].aim == AIM_MIN, "...down to -35 degrees, the grass (%d)", W.l[0].aim);
    CHECK(AIM_MIN * 360 / ANG_TURN == -35 && AIM_MAX * 360 / ANG_TURN == 54, "the range: -35..55 degrees");
    /* the head: its frames across the range, the mouth ahead of it and higher with a higher aim */
    CHECK(aim_frame(AIM_MIN) == 0 && aim_frame(AIM_MAX) == AIM_FRAMES - 1 && aim_frame((AIM_MIN + AIM_MAX) / 2) == 3,
          "the head's frames cover the range");
    int32_t hx, hy, mx0, my0, mx1, my1;
    llama_head(0, 0, &hx, &hy);
    llama_mouth(0, 0, &mx0, &my0);
    llama_mouth(0, AIM_MAX, &mx1, &my1);
    CHECK(mx0 > hx + Q(10) && my1 < my0 - Q(8), "the mouth is ahead of the head, higher when aiming up");
    int32_t sx, sy, bx, by, bx2, by2;
    llama_neck(0, AIM_MIN, &sx, &sy, &bx, &by);
    llama_neck(0, AIM_MAX, &sx, &sy, &bx2, &by2);
    CHECK(bx > bx2 && by > by2, "aiming low leans the neck toward the hornets, aiming high raises it");
}

static void test_spit(void)
{
    playing(&W, 1);
    in[0].spit = 1;
    world_step(&W, in);
    idle();
    const gob *g = &W.gobs[0];
    int32_t mx, my;
    llama_mouth(0, W.l[0].aim, &mx, &my);
    CHECK(g->on && (W.events[0] & EV_SPIT) && W.l[0].spits == 1, "A spits");
    CHECK(PX(g->x - mx) >= 3 && PX(g->x - mx) <= 6 && g->vy < 0, "the gob flies from the mouth, along the aim (up)");
    int32_t vy0 = g->vy;
    world_step(&W, in);
    CHECK(g->vy == vy0 + GOB_GRAV, "a slight arc: gravity %d a frame", GOB_GRAV);
    CHECK(W.l[0].puff > 0, "the cheeks puff as it spits");
    /* the reload, then 3 gobs at most */
    in[0].spit = 1;
    world_step(&W, in);
    idle();
    CHECK(W.l[0].spits == 1, "no second spit during the reload");
    int f = 0;
    while (W.l[0].spits == 1 && f < 40) world_step(&W, in), f++;
    CHECK(W.l[0].spits == 2 && f + 2 == RELOAD, "the press waits for the reload: spat %d frames later", f + 2);
    for (int i = 0; i < 6 * RELOAD; i++) {
        in[0].spit = 1;
        world_step(&W, in);
        CHECK(gobs_up(&W, 0) <= GOBS_MAX, "3 gobs in the air at most");
    }
    idle();
    /* its reach: the gob ends at GOB_RANGE, whatever the screen; low, it splats on the meadow */
    playing(&W, 1);
    int max_x = 0;
    in[0].spit = 1;
    world_step(&W, in);
    idle();
    for (int i = 0; i < 120; i++) {
        world_step(&W, in);
        if (W.gobs[0].on && PX(W.gobs[0].x) > max_x) max_x = PX(W.gobs[0].x);
    }
    CHECK(max_x < GOB_RANGE && max_x > GOB_RANGE - 8 && !W.gobs[0].on, "the spit's reach is x %d (%d)", GOB_RANGE, max_x);
    playing(&W, 1);
    W.l[0].aim = AIM_MIN;
    in[0].spit = 1;
    world_step(&W, in);
    idle();
    int splat = 0;
    for (int f = 0; f < 60; f++) {
        world_step(&W, in);
        for (int i = 0; i < FX_MAX; i++) splat |= W.fx[i].on && W.fx[i].kind == FX_SPLAT && PX(W.fx[i].y) == GROUND_HIT;
    }
    CHECK(!W.gobs[0].on && splat, "aimed at the grass: it splats on the meadow");
}

static void test_pop_and_big(void)
{
    playing(&W, 1);
    W.l[0].aim = 0;
    int32_t mx, my;
    llama_mouth(0, 0, &mx, &my);
    hornet *h = hornet_at(&W, HK_SMALL, PAT_STRAIGHT, 200, PX(my) + 1);
    in[0].spit = 1;
    world_step(&W, in);
    idle();
    int f = 0;
    while (h->on && f < 60) world_step(&W, in), f++;
    CHECK(!h->on && W.l[0].score == 10 && W.l[0].pops == 1, "a gob pops a hornet: 10 points (%d)", W.l[0].score);
    int fall = 0, pts = 0;
    for (int i = 0; i < FX_MAX; i++) {
        fall |= W.fx[i].on && W.fx[i].kind == FX_FALL;
        pts |= W.fx[i].on && W.fx[i].kind == FX_POINTS;
    }
    CHECK(fall && pts, "it tumbles down with stars (bonk), its points float up");
    steps(&W, 150);
    for (int i = 0; i < FX_MAX; i++) fall |= W.fx[i].on && W.fx[i].kind == FX_FALL;
    /* the big one: three hits, knocked back each time, 50 points */
    playing(&W, 1);
    W.l[0].aim = 0;
    h = hornet_at(&W, HK_BIG, PAT_STRAIGHT, 230, PX(my));
    int hits = 0, pushed = 1;
    for (int i = 0; i < 300 && h->on; i++) {
        int32_t x0 = h->x;
        in[0].spit = 1;
        world_step(&W, in);
        if (W.events[0] & EV_BIG_HIT) {
            hits++;
            pushed &= h->x > x0;
        }
    }
    idle();
    CHECK(hits == 2 && !h->on && W.l[0].score == 50, "a big hornet takes 3 gobs (%d hits, then popped), 50 points", hits);
    CHECK(pushed, "...each hit knocks it back");
    /* the dart: 25 */
    playing(&W, 1);
    W.l[0].aim = 0;
    h = hornet_at(&W, HK_DART, PAT_STRAIGHT, 200, PX(my));
    in[0].spit = 1;
    for (int i = 0; i < 60 && h->on; i++) world_step(&W, in);
    idle();
    CHECK(!h->on && W.l[0].score == 25, "a dart: 25 points (%d)", W.l[0].score);
}

static void test_sting(void)
{
    playing(&W, 1);
    int32_t hx, hy;
    llama_head(0, W.l[0].aim, &hx, &hy);
    hornet *h = hornet_at(&W, HK_SMALL, PAT_STRAIGHT, PX(hx) + 40, PX(hy));
    int f = 0;
    while (W.l[0].state == LS_PLAY && f < 200) world_step(&W, in), f++;
    CHECK(W.l[0].state == LS_HIT && (W.events[0] & EV_HIT), "a hornet reaching the head stings it");
    CHECK(W.freeze == HIT_FREEZE && h->pat == 255, "the hit-stop; the hornet buzzes away");
    steps(&W, HIT_FREEZE + HIT_FRAMES + 2);
    CHECK(W.l[0].state == LS_OUT && !world_running(&W) && world_all_out(&W), "one sting: the llama is out, the run ends");
    /* the neck counts too */
    playing(&W, 1);
    int32_t sx, sy, bx, by;
    llama_neck(0, W.l[0].aim, &sx, &sy, &bx, &by);
    h = hornet_at(&W, HK_SMALL, PAT_STRAIGHT, PX((sx + bx) / 2) + 2, PX((sy + by) / 2));
    world_step(&W, in);
    CHECK(W.l[0].state == LS_HIT, "a hornet on the neck stings too");
    /* the scared face */
    playing(&W, 1);
    llama_head(0, W.l[0].aim, &hx, &hy);
    hornet_at(&W, HK_SMALL, PAT_STRAIGHT, PX(hx) + SCARED_DIST - 8, PX(hy));
    world_step(&W, in);
    CHECK(W.l[0].scared, "a hornet close: the scared face");
    /* homing: near the head a hornet steers to it */
    playing(&W, 1);
    llama_head(0, W.l[0].aim, &hx, &hy);
    h = hornet_at(&W, HK_SMALL, PAT_STRAIGHT, PX(hx) + HOME_DX + 20, PX(hy) - 50);
    int y0 = PX(h->y);
    steps(&W, 60);
    CHECK(h->homing && PX(h->y) > y0 + 20, "near a llama a hornet steers to its head (%d -> %d)", y0, PX(h->y));
}

static void test_speed_and_ramp(void)
{
    playing(&W, 1);
    hornet *a = hornet_spawn(&W, HK_SMALL, PAT_STRAIGHT, 100, 0);
    W.l[0].score = 500;
    world_step(&W, in);
    hornet *b = hornet_spawn(&W, HK_SMALL, PAT_STRAIGHT, 100, 0);
    W.l[0].score = 100000;
    world_step(&W, in);
    hornet *c = hornet_spawn(&W, HK_SMALL, PAT_STRAIGHT, 100, 0);
    CHECK(a->v == HORNET_V0 && b->v > a->v * 3 / 2 && c->v == HORNET_V0 * (100 + HORNET_V_MAX) / 100,
          "faster with the score: %.2f, %.2f, at most %.2f px a frame", a->v / 65536.0, b->v / 65536.0, c->v / 65536.0);
    hornet *big = hornet_spawn(&W, HK_BIG, PAT_SINE, 100, 0);
    CHECK(big->v < c->v, "the big one is slower");
    /* more of them: hornets sent in a minute (none ever reaches the llama: they are cleared), by heat */
    int counts[3], kinds[3];
    static const int score[3] = {0, 300, 800};
    for (int k = 0; k < 3; k++) {
        playing(&W, 1);
        W.wave_t = WAVE_FIRST;
        W.l[0].score = score[k];
        counts[k] = kinds[k] = 0;
        for (int f = 0; f < 3600; f++) {
            world_step(&W, in);
            for (int i = 0; i < MAX_HORNETS; i++)
                if (W.hn[i].on) {
                    counts[k]++;
                    kinds[k] |= 1 << (W.hn[i].kind * 8 + W.hn[i].pat);
                    W.hn[i].on = 0;
                }
            W.l[0].state = LS_PLAY;
        }
    }
    CHECK(counts[0] < counts[1] && counts[1] < counts[2], "more hornets as the score rises: %d, %d, %d a minute",
          counts[0], counts[1], counts[2]);
    CHECK(!(kinds[0] & ~((1 << PAT_STRAIGHT) | (1 << PAT_SINE))) && (kinds[2] & (1 << (HK_BIG * 8 + PAT_SINE))) &&
          (kinds[2] & (1 << (HK_DART * 8 + PAT_DART))) && (kinds[2] & (1 << PAT_LOOP)) && (kinds[2] & (1 << PAT_DIVE)) &&
          (kinds[2] & (1 << PAT_ZIGZAG)), "new kinds and patterns later (heat 0: %x, heat 80: %x)", kinds[0], kinds[2]);
    /* a gentle start: the first 20 s */
    playing(&W, 1);
    W.wave_t = WAVE_FIRST;
    int n = 0;
    for (int f = 0; f < 1200; f++) {
        world_step(&W, in);
        for (int i = 0; i < MAX_HORNETS; i++)
            if (W.hn[i].on && W.hn[i].t == -1 && W.hn[i].x == Q(SPAWN_R) - W.hn[i].v) n++;
    }
    CHECK(n >= 6 && n <= 12, "a gentle start: %d hornets in the first 20 s", n);
}

static void test_patterns(void)
{
    /* sine waves and zig-zags around their lane, before homing */
    for (int pat = PAT_SINE; pat <= PAT_ZIGZAG; pat++) {
        playing(&W, 1);
        hornet *h = hornet_spawn(&W, HK_SMALL, pat, 100, 0);
        int lo = 999, hi = -999;
        for (int f = 0; f < 600 && h->on && !h->homing; f++) {
            world_step(&W, in);
            if (h->t >= 0) {
                if (PX(h->y) < lo) lo = PX(h->y);
                if (PX(h->y) > hi) hi = PX(h->y);
            }
        }
        CHECK(lo < 100 - h->amp + 4 && hi > 100 + h->amp - 4 && hi - lo <= 2 * h->amp + 2,
              "%s: %d..%d around the lane 100", pat == PAT_SINE ? "the sine wave" : "the zig-zag", lo, hi);
    }
    /* the dive: high, then down at the head, faster */
    playing(&W, 1);
    hornet *h = hornet_spawn(&W, HK_SMALL, PAT_DIVE, 40, 0);
    while (h->on && !h->aux_t) world_step(&W, in);
    int32_t x0 = h->x, y0 = h->y;
    world_step(&W, in);
    CHECK(PX(y0) < 60 && h->y > y0 && h->x < x0 && (h->x - x0) * (int64_t)(h->x - x0) + (int64_t)(h->y - y0) * (h->y - y0) >
          (int64_t)h->v * h->v * 2, "the dive: from high up, down at the head, faster");
    /* the loop: it goes back (right) for a while, then on */
    playing(&W, 1);
    h = hornet_spawn(&W, HK_SMALL, PAT_LOOP, 120, 0);
    int back = 0, top = 999;
    for (int f = 0; f < 900 && h->on && h->aux_t < 2; f++) {
        world_step(&W, in);
        if (h->vx > 0) back++;
        if (PX(h->y) < top) top = PX(h->y);
    }
    CHECK(h->aux_t == 2 && back > 20 && top < 120 - LOOP_R * 2 + 3 && PX(h->y) == 120, "the loop: back for %d frames, up to y %d, "
          "then on along its lane", back, top);
    /* the dart: it perches, buzzes, then dashes at the head */
    playing(&W, 1);
    h = hornet_spawn(&W, HK_DART, PAT_DART, 140, 0);
    int perch = 0, warned = 0;
    for (int f = 0; f < 900 && h->on && h->aux_t < 2; f++) {
        world_step(&W, in);
        if (h->aux_t == 1) perch++;
        warned |= W.wevents & WEV_DART;
    }
    world_step(&W, in);
    int32_t hx, hy;
    llama_head(0, W.l[0].aim, &hx, &hy);
    CHECK(perch >= DART_WIND && warned && h->aux_t == 2 && PX(h->x) <= DART_PERCH && h->dx < -Q(2) &&
          (h->dy > 0) == (hy > h->y), "the dart: perched %d frames (a warning), then a dash at the head", perch);
    /* pairs and swarms */
    playing(&W, 1);
    world_wave(&W, W_PAIR);
    CHECK(hornets(&W) == 2, "a pair: two at once");
    playing(&W, 1);
    world_wave(&W, W_SWARM);
    CHECK(hornets(&W) == 4, "a swarm: four (more later)");
    playing(&W, 1);
    W.heat = 75;
    world_wave(&W, W_SWARM);
    CHECK(hornets(&W) == 6, "...six when the sky is hot");
    playing(&W, 1);
    world_wave(&W, W_BIG);
    CHECK(hornets(&W) == 1 && W.msg == MSG_BIG, "a big one, named the first time");
}

/* widescreen: every hornet appears beyond the widest screen's right edge (480 at 560 columns) and glides in along
 * its lane; the spit never goes past GOB_RANGE; the rules never ask the width (this program has none) */
static void test_widescreen(void)
{
    world_init(&W, 2, 77);
    idle();
    in[0].any = 1;
    int seen[MAX_HORNETS] = {0}, bad = 0, born = 0, gob_far = 0;
    for (int f = 0; f < 7200; f++) {
        for (int p = 0; p < 2; p++) {
            pin b = bot_decide(&W, p, 0);
            in[p] = b;
            if (W.l[p].state != LS_PLAY) W.l[p].state = LS_PLAY;    /* untouchable for the test */
        }
        world_step(&W, in);
        for (int i = 0; i < MAX_HORNETS; i++) {
            const hornet *h = &W.hn[i];
            if (h->on && !seen[i]) {
                born++;
                if (PX(h->x) < 480 + 8) bad++;
            }
            seen[i] = h->on;
        }
        for (int i = 0; i < MAX_PLAYERS * GOBS_MAX; i++) gob_far |= W.gobs[i].on && PX(W.gobs[i].x) >= GOB_RANGE;
    }
    CHECK(born > 100 && !bad, "%d hornets came, all from beyond the widest screen", born);
    CHECK(!gob_far, "no spit beyond x %d", GOB_RANGE);
}

static void test_touch(void)
{
    /* tap-to-aim: the aim whose spit passes through the point */
    static const int pts[][2] = {{150, 60}, {220, 40}, {300, 120}, {200, 180}, {260, 90}, {130, 110}};
    for (int i = 0; i < (int)(sizeof pts / sizeof pts[0]); i++) {
        int a = llama_aim_at(0, pts[i][0], pts[i][1]);
        int32_t mx, my;
        llama_mouth(0, a, &mx, &my);
        int32_t vx = (int32_t)(((int64_t)GOB_SPEED * icos1024(a)) >> 14), vy = -(int32_t)(((int64_t)GOB_SPEED * isin1024(a)) >> 14);
        int best = 999;
        int32_t x = mx, y = my;
        for (int f = 0; f < 80; f++) {
            x += vx, y += vy, vy += GOB_GRAV;
            int d = abs(PX(x) - pts[i][0]) + abs(PX(y) - pts[i][1]);
            if (d < best) best = d;
        }
        CHECK(best <= 6, "a tap at %d,%d: the spit passes within %d px", pts[i][0], pts[i][1], best);
    }
    CHECK(llama_aim_at(0, 200, -200) == AIM_MAX && llama_aim_at(0, 150, 236) == AIM_MIN,
          "out of reach: the nearest aim of the range");
    playing(&W, 1);
    in[0].touch = 1, in[0].tx = 220, in[0].ty = 40;
    world_step(&W, in);
    idle();
    CHECK(W.l[0].aim == llama_aim_at(0, 220, 40) && W.l[0].spits == 1, "a finger down: the head turns there, it spits");
    in[0].touch = 2, in[0].tx = 220, in[0].ty = 150;
    world_step(&W, in);
    CHECK(W.l[0].aim == llama_aim_at(0, 220, 150) && W.l[0].spits == 1, "held: the head follows, no new spit");
    in[0].touch = 1;
    world_step(&W, in);
    idle();
    CHECK(W.l[0].spits == 1, "a new tap during the reload waits ...");
    steps(&W, RELOAD);
    CHECK(W.l[0].spits == 2, "... and spits when it ends");
    int aim = W.l[0].aim;
    in[0].touch = 1, in[0].tx = 40, in[0].ty = 60;
    steps(&W, 20);
    CHECK(W.l[0].aim == aim && W.l[0].spits == 2, "a tap left of the mouth does nothing");
}

static void test_herd(void)
{
    playing(&W, 4);
    int32_t hx[4], hy[4];
    for (int p = 0; p < 4; p++) llama_head(p, AIM_START, &hx[p], &hy[p]);
    CHECK(hy[3] < hy[1] && hy[1] < hy[0] && hy[0] < hy[2] && hy[2] - hy[3] > Q(90),
          "the herd: four heads one above another (P4 %d, P2 %d, P1 %d, P3 %d)", PX(hy[3]), PX(hy[1]), PX(hy[0]), PX(hy[2]));
    hornet *h = hornet_spawn(&W, HK_SMALL, PAT_STRAIGHT, PX(hy[1]) + 4, 0);
    CHECK(h->target == 1, "a hornet goes for the llama whose head is nearest its lane");
    hornet_at(&W, HK_SMALL, PAT_STRAIGHT, PX(hx[0]) + 8, PX(hy[0]));
    world_step(&W, in);
    CHECK(W.l[0].state == LS_HIT && W.l[1].state == LS_PLAY && world_running(&W), "P1 stung: the others play on");
    W.l[1].state = W.l[2].state = W.l[3].state = LS_OUT;
    steps(&W, HIT_FREEZE + HIT_FRAMES + 2);
    CHECK(!world_running(&W) && world_all_out(&W), "the run ends when all are out");
    playing(&W, 4);
    W.l[1].state = LS_OUT;
    h = hornet_spawn(&W, HK_SMALL, PAT_STRAIGHT, PX(hy[1]), 0);
    world_step(&W, in);
    CHECK(h->target != 1, "an out llama is left alone");
}

static void test_determinism_and_bot(void)
{
    for (int k = 0; k < 2; k++) {
        world *w = k ? &W2 : &W;
        world_init(w, 1, 99);
        for (int f = 0; f < 3000; f++) {
            in[0] = bot_decide(w, 0, 0);
            world_step(w, in);
        }
    }
    CHECK(!memcmp(&W, &W2, sizeof W), "the same seed and inputs: the same run");
    CHECK(W.l[0].state == LS_PLAY && W.l[0].pops >= 25, "the bot plays 50 s without a sting (%d pops, t %d, state %d)",
          W.l[0].pops, W.t, W.l[0].state);
    world_init(&W, 1, 99);
    for (int f = 0; f < 3000 && world_running(&W); f++) {
        in[0] = bot_decide(&W, 0, 10);
        world_step(&W, in);
    }
    CHECK(W.l[0].pops >= 3, "a beginner-like bot pops some (%d)", W.l[0].pops);
}

int main(void)
{
    test_start_and_aim();
    test_spit();
    test_pop_and_big();
    test_sting();
    test_speed_and_ramp();
    test_patterns();
    test_widescreen();
    test_touch();
    test_herd();
    test_determinism_and_bot();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "rules: all passed", checks, fails);
    return fails ? 1 : 0;
}
