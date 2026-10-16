/*
 * Gecko Golf: the rules alone (src/play.c, no video, no sound, no screen width: this program links play.c with the
 * SDK's RNG only): the course (the same from the same seed, its heights and greens, every hole played through by the
 * bot), the ball on each surface (grass, sand, ice, mushrooms, water, a wall, the cup taken slow and skipped fast), the
 * water and out-of-bounds penalties, the stroke budget and its refunds, out of strokes, the turn order of 2 players and
 * the honour at the next tee, the power gauge, the touch slingshot, determinism.
 *   geckogolf_rules            the tests
 *   geckogolf_rules --tune S N the holes a bot of skill S reaches in N runs (the medals' tuning, DESIGN.md)
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
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

static world W, W2;
static pin IN[MAX_PLAYERS];

static void idle(void) { for (int p = 0; p < MAX_PLAYERS; p++) pin_none(&IN[p]); }
static void step(world *w) { world_step(w, IN); }

/* a flat course of `surf` at y 150 around the current hole (the cup moved far away) */
static void flat(world *w, int surf)
{
    for (int32_t x = w->hole_cam - 200; x < w->hole_cam + 600; x++) {
        w->tr.h[x & TMASK] = 150;
        w->tr.s[x & TMASK] = (uint8_t)surf;
    }
    hole_rec *h = &w->tr.holes[w->hole % HOLE_RING];
    h->cup_x = w->hole_cam + 2000;
    h->cup_y = 150;
}

static ball rolling(int x, int32_t u)
{
    ball b;
    memset(&b, 0, sizeof b);
    b.x = x << 16;
    b.y = 150 << 16;
    b.u = u;
    b.state = BS_ROLL;
    return b;
}

/* roll a ball to its end; returns the frames */
static int run_ball(const world *w, ball *b, int max)
{
    int f = 0;
    while ((b->state == BS_FLY || b->state == BS_ROLL) && f < max) {
        ball_step(w, b);
        f++;
    }
    return f;
}

/* play one shot of player p as the bot (the same aim and power as the game's bot, without its thinking pause) */
static void bot_shot(world *w, int p, int skill)
{
    bot_plan pl;
    bot_plan_shot(w, p, skill, &pl);
    idle();
    IN[p].set_aim = pl.aim;
    IN[p].hold = 1;
    step(w);
    idle();
    while (w->phase == PH_CHARGE && w->gauge < pl.gauge) {
        IN[p].hold = 1;
        step(w);
    }
    idle();
    step(w);                                            /* let go */
}

/* steps until a player may shoot again (or the run is over); returns 0 when over */
static int to_aim(world *w)
{
    idle();
    for (int i = 0; i < 4000 && w->phase != PH_AIM && w->phase != PH_OVER; i++) step(w);
    return w->phase == PH_AIM;
}

/* a whole run of one bot: the holes reached */
static int bot_run(uint32_t seed, int skill, int max_holes, int *aces)
{
    world_init(&W, 1, seed, 0);
    while (to_aim(&W) && W.g[0].holes < max_holes) bot_shot(&W, 0, skill);
    if (aces) *aces = W.g[0].aces;
    return W.g[0].holes;
}

static void tune(int skill, int runs)
{
    int sum = 0, hist[6] = {0};
    static const int medals[4] = MEDAL_SCORES;
    printf("skill %d:", skill);
    for (int r = 0; r < runs; r++) {
        int aces, h = bot_run(1000u + (uint32_t)r * 7919u, skill, 400, &aces);
        sum += h;
        int m = 0;
        while (m < 4 && h >= medals[m]) m++;
        hist[m]++;
        printf(" %d", h);
        fflush(stdout);
    }
    printf("\n  average %d.%d holes; no medal %d, bronze %d, silver %d, gold %d, pearl %d (of %d)\n", sum / runs,
           sum * 10 / runs % 10, hist[0], hist[1], hist[2], hist[3], hist[4], runs);
}

static void test_course(void)
{
    world_init(&W, 1, 42, 0);
    world_init(&W2, 1, 42, 0);
    CHECK(!memcmp(&W.tr, &W2.tr, sizeof W.tr), "the same seed, the same course");
    world_init(&W2, 1, 43, 0);
    CHECK(memcmp(W.tr.h, W2.tr.h, sizeof W.tr.h), "another seed, another course");
    /* 60 holes from 3 seeds: the heights, the greens, the hazards' places */
    int shapes[SH_COUNT] = {0}, hz = 0, biomes[BI_COUNT] = {0};
    for (uint32_t seed = 1; seed <= 3; seed++) {
        world_init(&W, 1, seed, 0);
        for (int k = 0; k < 60; k++) {
            /* generate on as the game would: the hole's camera moves along */
            W.hole_cam = world_hole(&W, k)->tee_x - TEE_X;
            world_gen_ahead(&W);
            if (W.tr.gen_hole <= k + 1) break;
            const hole_rec *h = world_hole(&W, k);
            int d = (int)(h->cup_x - h->tee_x);
            CHECK(d >= DIST_MIN && d <= DIST_MAX, "seed %u hole %d: the cup %d px from the tee", seed, k, d);
            CHECK(h->cup_y >= CUP_Y_TOP && h->cup_y <= CUP_Y_BOT, "hole %d: the cup's height %d", k, h->cup_y);
            int bad = 0;
            for (int32_t x = h->cup_x - GREEN_HALF; x < h->cup_x + TEE_FLAT; x++) {
                int y = terrain_y(&W.tr, x), s = terrain_surf(&W.tr, x);
                if (k == 0 && x >= h->cup_x - CUP_W / 2 && x < h->cup_x + CUP_W / 2) continue;   /* hole 0: dug */
                if (y != h->cup_y || (s != SF_GRASS)) bad++;
            }
            CHECK(!bad, "seed %u hole %d: the green is flat grass (%d px not)", seed, k, bad);
            for (int32_t x = h->tee_x; x < h->cup_x; x++) {
                int y = terrain_y(&W.tr, x);
                if (y < Y_TOP || y > Y_BOT + CUP_DEPTH) bad++;
            }
            CHECK(!bad, "hole %d: the ground stays between Y_TOP and Y_BOT", k);
            shapes[h->shape]++;
            biomes[h->biome]++;
            hz |= h->hazards;
            CHECK(h->biome == (k / HOLES_PER_BIOME) % BI_COUNT, "hole %d: biome %d", k, h->biome);
        }
    }
    printf("  course: shapes");
    for (int i = 0; i < SH_COUNT; i++) printf(" %d", shapes[i]);
    printf(", hazards 0x%x\n", hz);
    for (int i = 0; i < SH_COUNT; i++) CHECK(shapes[i] > 0, "every shape shows up (%d)", i);
    CHECK(hz == (HZ_POND | HZ_BUNKER | HZ_ICE | HZ_MUSH), "every hazard shows up (0x%x)", hz);
}

/* every hole can be played: the best bot holes each of the first 40 holes of 4 seeds in a few strokes */
static void test_playable(void)
{
    int worst = 0, holes = 0, strokes = 0;
    for (uint32_t seed = 11; seed <= 14; seed++) {
        world_init(&W, 1, seed, 0);
        W.g[0].strokes = STROKES_MAX;
        int last = 0, shots = 0;
        while (to_aim(&W) && W.g[0].holes < 40 && shots < 400) {
            W.g[0].strokes = STROKES_MAX;               /* never out: every hole is played */
            bot_shot(&W, 0, 0);
            shots++;
            if (W.g[0].holes != last) {
                int n = W.g[0].total - strokes;
                strokes = W.g[0].total;
                if (n > worst) worst = n;
                holes++;
                last = W.g[0].holes;
            }
        }
        if (W.g[0].holes < 40) {
            const hole_rec *h = world_hole(&W, W.hole);
            bot_plan pl;
            bot_plan_shot(&W, 0, 0, &pl);
            ball e;
            int r = sim_shot(&W, 0, pl.aim, pl.gauge, &e);
            printf("  stuck: hole %d shape %d hazards %x cup %d,%d; plan %d/%d -> %d at %d,%d\n", W.hole, h->shape,
                   h->hazards, world_cup_sx(&W), h->cup_y, pl.aim, pl.gauge, r, (int)(e.x >> 16), (int)(e.y >> 16));
            for (int x = -10; x <= 40; x += 2) printf("%d:%d%c ", x, terrain_y(&W.tr, W.hole_cam + (W.g[0].b.x >> 16) + x),
                                                     "gsimwc"[terrain_surf(&W.tr, W.hole_cam + (W.g[0].b.x >> 16) + x)]);
            printf("\n");
        }
        CHECK(W.g[0].holes >= 40, "seed %u: the bot played through 40 holes (%d; phase %d, ball %d at %d,%d, shots %d)",
              seed, W.g[0].holes, W.phase, W.g[0].b.state, (int)(W.g[0].b.x >> 16), (int)(W.g[0].b.y >> 16), shots);
        strokes = 0;
        W.g[0].total = 0;
    }
    printf("  playable: %d holes, the worst in %d strokes\n", holes, worst);
    CHECK(worst <= 5, "every hole within 5 strokes of the best bot (worst %d)", worst);
}

static void test_surfaces(void)
{
    int dist[SF_COUNT];
    int surf[3] = {SF_GRASS, SF_SAND, SF_ICE};
    for (int i = 0; i < 3; i++) {
        world_init(&W, 1, 5, 0);
        flat(&W, surf[i]);
        ball b = rolling(60, FX * 2);
        run_ball(&W, &b, 5000);
        dist[surf[i]] = (int)(b.x >> 16) - 60;
        CHECK(b.state == BS_REST || b.state == BS_OB, "surface %d: the ball stops (state %d)", surf[i], b.state);
    }
    printf("  rolling at 2 px/frame: grass %d px, sand %d, ice %d\n", dist[SF_GRASS], dist[SF_SAND], dist[SF_ICE]);
    CHECK(dist[SF_GRASS] > 40 && dist[SF_GRASS] < 140, "grass: a fair roll (%d)", dist[SF_GRASS]);
    CHECK(dist[SF_SAND] < 12, "sand stops the ball (%d)", dist[SF_SAND]);
    CHECK(dist[SF_ICE] > dist[SF_GRASS] * 2, "ice slides far (%d)", dist[SF_ICE]);
    /* a landing on sand: it stays where it lands (on grass it bounces and rolls on) */
    int land[2];
    for (int g = 0; g < 2; g++) {
        world_init(&W, 1, 5, 0);
        flat(&W, g ? SF_GRASS : SF_SAND);
        ball b;
        memset(&b, 0, sizeof b);
        b.x = 100 << 16;
        b.y = 100 << 16;
        b.vx = FX * 3;
        b.vy = FX * 2;
        b.state = BS_FLY;
        int first = -1;
        for (int f = 0; f < 2000 && (b.state == BS_FLY || b.state == BS_ROLL); f++) {
            ball_step(&W, &b);
            if (first < 0 && b.y >= (150 << 16)) first = (int)(b.x >> 16);
        }
        land[g] = (int)(b.x >> 16) - first;
        CHECK(b.state == BS_REST, "a landing ball rests (%d)", b.state);
    }
    CHECK(land[0] <= 6 && land[1] > 30, "sand: a landing ball stays (%d px on, grass %d)", land[0], land[1]);
    ball b;
    /* a mushroom: dropped on it, the ball bounces back up at least MUSH_MIN */
    world_init(&W, 1, 5, 0);
    flat(&W, SF_MUSH);
    memset(&b, 0, sizeof b);
    b.x = 160 << 16;
    b.y = 140 << 16;
    b.vy = FX / 2;
    b.state = BS_FLY;
    int up = 0;
    for (int f = 0; f < 60 && !up; f++) {
        int ev = ball_step(&W, &b);
        if ((ev & EV_MUSH) && b.vy <= -MUSH_MIN * 9 / 10) up = 1;
    }
    CHECK(up, "a mushroom bounces the ball up (vy %d)", b.vy);
    /* water: a splash */
    world_init(&W, 1, 5, 0);
    flat(&W, SF_WATER);
    memset(&b, 0, sizeof b);
    b.x = 160 << 16;
    b.y = 120 << 16;
    b.vx = FX;
    b.state = BS_FLY;
    run_ball(&W, &b, 200);
    CHECK(b.state == BS_SPLASH, "into the water: a splash (state %d)", b.state);
    /* a wall: rolling into a cliff, the ball comes back */
    world_init(&W, 1, 5, 0);
    flat(&W, SF_GRASS);
    for (int32_t x = W.hole_cam + 120; x < W.hole_cam + 200; x++) W.tr.h[x & TMASK] = 110;
    b = rolling(80, FX * 3);
    run_ball(&W, &b, 3000);
    CHECK((b.x >> 16) < 120 && b.state == BS_REST, "a cliff stops the ball (x %d, state %d)", (int)(b.x >> 16), b.state);
    /* the cup: slow, in; fast, over it */
    for (int fast = 0; fast < 2; fast++) {
        world_init(&W, 1, 5, 0);
        flat(&W, SF_GRASS);
        hole_rec *h = &W.tr.holes[W.hole % HOLE_RING];
        h->cup_x = W.hole_cam + 200;
        h->cup_y = 150;
        for (int32_t x = h->cup_x - CUP_W / 2; x < h->cup_x + CUP_W / 2; x++) {
            W.tr.h[x & TMASK] = 150 + CUP_DEPTH;
            W.tr.s[x & TMASK] = SF_CUP;
        }
        b = rolling(fast ? 150 : 180, fast ? FX * 3 : FX * 13 / 10);
        run_ball(&W, &b, 3000);
        if (fast) CHECK(b.state == BS_REST && (b.x >> 16) > 210, "a fast ball skips the cup (state %d x %d)", b.state, (int)(b.x >> 16));
        else CHECK(b.state == BS_CUP, "a slow ball drops in the cup (state %d x %d)", b.state, (int)(b.x >> 16));
    }
}

/* the flow: a shot, its stroke; the water's and out of bounds' penalty; the budget; the refunds */
static void shoot_at(world *w, int p, int aim, int gauge)
{
    idle();
    IN[p].set_aim = (int16_t)aim;
    IN[p].hold = 1;
    step(w);
    while (w->gauge < gauge) step(w);
    idle();
    step(w);
}

static void test_budget(void)
{
    /* the gauge: up 2 a frame to the top, then down; a release below GAUGE_MIN_SHOT is no shot */
    world_init(&W, 1, 5, 0);
    idle();
    IN[0].hold = 1;
    step(&W);
    CHECK(W.phase == PH_CHARGE && W.gauge == 0, "A held: the gauge starts");
    for (int i = 0; i < GAUGE_MAX / GAUGE_RATE + 5; i++) step(&W);
    CHECK(W.gauge == GAUGE_MAX - 5 * GAUGE_RATE, "the gauge goes up and comes back down (%d)", W.gauge);
    idle();
    IN[0].cancel = 1;
    step(&W);
    CHECK(W.phase == PH_AIM && W.g[0].strokes == START_STROKES, "B: no shot");
    IN[0].cancel = 0;
    IN[0].hold = 1;
    step(&W);
    step(&W);
    idle();
    step(&W);
    CHECK(W.phase == PH_AIM && W.g[0].strokes == START_STROKES, "a tap of A is no shot");

    /* the water: a shot into a pond costs the stroke and a penalty, the ball comes back */
    world_init(&W, 1, 5, 0);
    flat(&W, SF_GRASS);
    for (int32_t x = W.hole_cam + 120; x < W.hole_cam + 300; x++) W.tr.s[x & TMASK] = SF_WATER;
    int32_t x0 = W.g[0].b.x;
    shoot_at(&W, 0, 60, 80);
    idle();
    for (int i = 0; i < 600 && W.phase != PH_SPLASH; i++) step(&W);
    CHECK(W.phase == PH_SPLASH && W.msg == MSG_WATER, "the ball in the water (phase %d)", W.phase);
    to_aim(&W);
    CHECK(W.g[0].b.x == x0 && W.g[0].strokes == START_STROKES - 1 - PENALTY && W.g[0].hole_strokes == 2,
          "the water: back to the spot, a penalty stroke (strokes %d, hole %d)", W.g[0].strokes, W.g[0].hole_strokes);
    /* out of bounds: the same */
    shoot_at(&W, 0, 330, GAUGE_MAX);
    idle();
    for (int i = 0; i < 600 && W.phase != PH_SPLASH; i++) step(&W);
    CHECK(W.msg == MSG_OB, "out of bounds (msg %d)", W.msg);
    to_aim(&W);
    CHECK(W.g[0].b.x == x0 && W.g[0].strokes == START_STROKES - 2 - 2 * PENALTY, "out of bounds: back, a penalty");

    /* the refunds: a hole in one +REFUND_ACE, in two +REFUND_TWO, in three nothing (the best bot's first holes) */
    int seen[4] = {0};
    for (uint32_t seed = 1; seed < 40 && !(seen[1] && seen[2] && seen[3]); seed++) {
        world_init(&W, 1, seed, 0);
        int misses = (int)(seed % 3);
        for (int tries = 0; tries < 8 && to_aim(&W) && W.g[0].holes == 0; tries++) {
            if (W.g[0].hole_strokes < misses) {
                /* a miss on purpose: a short putt, then the bot */
                shoot_at(&W, 0, AIM_START, 12);
                continue;
            }
            int before = W.g[0].strokes, n = W.g[0].hole_strokes + 1;
            bot_shot(&W, 0, 0);
            for (int i = 0; i < 2000 && W.phase == PH_FLIGHT; i++) step(&W);
            if (W.phase == PH_HOLED) {
                int want = n == 1 ? REFUND_ACE : n == 2 ? REFUND_TWO : 0;
                CHECK(W.g[0].strokes == before - 1 + want, "in %d: %d strokes back (had %d, now %d)", n, want, before,
                      W.g[0].strokes);
                seen[n > 3 ? 3 : n] = 1;
            }
        }
    }
    CHECK(seen[1] && seen[2] && seen[3], "holes in one, two and three seen (%d %d %d)", seen[1], seen[2], seen[3]);

    /* out of strokes: the last stroke rests outside the cup: out, the run is over */
    world_init(&W, 1, 5, 0);
    W.g[0].strokes = 1;
    shoot_at(&W, 0, AIM_START, 10);
    to_aim(&W);
    CHECK(W.g[0].out && W.phase == PH_OVER && !world_running(&W), "out of strokes: over (out %d phase %d)", W.g[0].out,
          W.phase);
}

/* 2 players: the farthest from the cup plays; at the next tee the honour (fewer strokes) goes first */
static void test_turns(void)
{
    world_init(&W, 2, 77, 0);
    CHECK(W.turn == 0, "the first tee: P1");
    shoot_at(&W, 0, AIM_START, 30);                     /* P1: a short shot */
    to_aim(&W);
    {
        int cup = world_cup_sx(&W), d0 = abs((int)(W.g[0].b.x >> 16) - cup), d1 = abs((int)(W.g[1].b.x >> 16) - cup);
        CHECK(W.turn == (d1 > d0 ? 1 : 0), "the farther plays (P1 %d px, P2 %d px: turn %d)", d0, d1, W.turn);
    }
    int guard = 0;
    while (W.hole == 0 && guard++ < 40) {
        if (!to_aim(&W)) break;
        int cup = world_cup_sx(&W), p = W.turn;
        int d = abs((int)(W.g[p].b.x >> 16) - cup);
        for (int q = 0; q < 2; q++)
            if (q != p && !W.g[q].done) CHECK(abs((int)(W.g[q].b.x >> 16) - cup) <= d + 1, "the farthest plays");
        if (W.hole) break;
        bot_shot(&W, p, p == 0 ? 0 : 0);
        if (W.g[1].hole_strokes > 0 && W.g[0].done == 0 && p == 0 && W.g[0].hole_strokes < 3) continue;
    }
    to_aim(&W);
    CHECK(W.hole == 1, "both holed: the next hole (%d)", W.hole);
    int first = W.g[0].last_hole < W.g[1].last_hole ? 0 : W.g[1].last_hole < W.g[0].last_hole ? 1 : 0;
    CHECK(W.turn == first, "the honour: the fewer strokes first (P1 %d, P2 %d, turn %d)", W.g[0].last_hole,
          W.g[1].last_hole, W.turn);
    CHECK(W.view_cam == W.hole_cam && (W.g[0].b.x >> 16) == TEE_X && (W.g[1].b.x >> 16) == TEE_X,
          "the camera panned; both balls at the new tee");
    /* a player out of strokes is skipped; the other plays on */
    world_init(&W, 2, 78, 0);
    W.g[0].strokes = 1;
    shoot_at(&W, 0, AIM_START, 10);
    to_aim(&W);
    CHECK(W.g[0].out && W.turn == 1 && world_running(&W), "P1 out: P2 plays on");
}

static void test_touch(void)
{
    int aim, gauge;
    CHECK(!touch_shot(3, 4, &aim, &gauge), "a short pull: no shot");
    /* touch_shot takes where the finger went down less where it is (the pull, backwards): the shot goes that way */
    CHECK(touch_shot(50, -50, &aim, &gauge) && abs(aim - 90) <= 1, "pulled down-left: up-right at 45 degrees (%d)", aim);
    CHECK(gauge == (70 - TOUCH_DEAD) * GAUGE_MAX / (TOUCH_FULL - TOUCH_DEAD), "the power by the pull (%d)", gauge);
    CHECK(touch_shot(-40, -30, &aim, &gauge) && aim > 270, "pulled down-right: up-left (%d)", aim);
    CHECK(touch_shot(0, -60, &aim, &gauge) && abs(aim - 180) <= 1, "pulled straight down: straight up (%d)", aim);
    CHECK(touch_shot(-60, 40, &aim, &gauge) && aim == AIM_MAX, "pulled upwards: along the ground (%d)", aim);
    CHECK(touch_shot(300, 0, &aim, &gauge) && gauge == GAUGE_MAX, "a long pull: full power (%d)", gauge);
    /* the slingshot's shot is the pad's shot at the same aim and power */
    world_init(&W, 1, 9, 0);
    world_init(&W2, 1, 9, 0);
    touch_shot(50, -50, &aim, &gauge);
    idle();
    IN[0].set_aim = (int16_t)aim;
    IN[0].set_gauge = (int16_t)gauge;
    step(&W);
    CHECK(W.phase == PH_CHARGE && W.gauge == gauge, "the pull shows on the gauge");
    IN[0].shoot = 1;
    step(&W);
    CHECK(W.phase == PH_FLIGHT && W.g[0].strokes == START_STROKES - 1, "let go: the shot");
    shoot_at(&W2, 0, aim, gauge);
    for (int i = 0; i < 300; i++) { idle(); step(&W); step(&W2); }
    CHECK(W.g[0].b.x == W2.g[0].b.x && W.g[0].b.y == W2.g[0].b.y, "the same shot as the pad's");
}

static void test_determinism(void)
{
    int a = bot_run(321, 6, 60, NULL);
    world W3 = W;
    int b = bot_run(321, 6, 60, NULL);
    CHECK(a == b && !memcmp(&W3, &W, sizeof W), "a bot run twice: the same run (%d holes)", a);
}

int main(int argc, char **argv)
{
    if (argc >= 4 && !strcmp(argv[1], "--tune")) {
        tune(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }
    test_course();
    test_surfaces();
    test_budget();
    test_turns();
    test_touch();
    test_playable();
    test_determinism();
    printf("geckogolf rules: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
