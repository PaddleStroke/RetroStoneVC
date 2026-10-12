/*
 * Unicorn Hop: the rules alone (src/play.c): the jump, the landing and its margin, the miss (never a lower cloud),
 * riding, the pink bounce, the rain cloud, the thunder zap, the stars, the rainbow, the camera and the players left
 * below, determinism, and the bot climbing.
 *   build/host/unicornhop_rules
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/unicornhop/LICENSE.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the SDK's generator (sdk/src/rs_core.c), so play.c links alone */
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

static int fails;
#define CHECK(c, ...) do { if (c) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } \
                           else { printf("  FAIL "); printf(__VA_ARGS__); printf("  (%s:%d)\n", __FILE__, __LINE__); fails++; } } while (0)

static const int NONE[MAX_PLAYERS] = {0}, P1[MAX_PLAYERS] = {1};

static cloud *cl(world *w, int level) { return (cloud *)world_cloud(w, level); }

/* a still world: no cloud moves, nothing in the gaps (the tests place what they need) */
static void still(world *w, int players)
{
    world_init(w, players, 7);
    for (int i = 0; i < RING; i++) {
        cloud *c = &w->c[i];
        c->vx = 0;
        c->thunder = c->star = 0;
        if (c->type != CL_START) { c->type = CL_NORMAL; c->x = CENTER_X << 16; c->w = 96; }
    }
}

static void run(world *w, int frames, const int *press)
{
    for (int i = 0; i < frames; i++) world_step(w, i == 0 ? press : NONE);
}

static void put_on(world *w, int p, int level)
{
    hero *h = &w->h[p];
    h->level = h->height = level;
    h->state = HS_STAND;
    h->dy = h->vy = 0;
    h->ox = h->x - cl(w, level)->x;
    w->started = 1;
}

int main(void)
{
    world w;
    int nj = jump_frames(0), nb = jump_frames(1);
    printf("Unicorn Hop rules: a jump lands %d frames after the take-off, a pink bounce %d\n", nj, nb);
    CHECK(nj >= 38 && nj <= 44 && nb > nj && nb <= 56, "the jump arcs: %d and %d frames", nj, nb);

    /* nothing happens until A: the unicorn waits on the start cloud */
    world_init(&w, 1, 3);
    run(&w, 600, NONE);
    CHECK(w.h[0].state == HS_READY && hero_alt(&w.h[0]) == 0 && !w.started, "it waits on the start cloud (600 frames)");

    /* the start press: the first jump lands on level 1 (wide, right above) */
    world_init(&w, 4, 3);
    run(&w, nj + 2, P1);
    CHECK(w.h[0].state == HS_STAND && w.h[0].level == 1 && w.h[0].score == 1, "the first jump lands on level 1 (P1 of 4)");
    CHECK(w.h[1].state == HS_STAND && w.h[1].level == 0, "the others stay on the start cloud until their press");

    /* straight up: no steering, x fixed in the air */
    still(&w, 1);
    world_step(&w, P1);
    int32_t x0 = w.h[0].x;
    for (int i = 0; i < 20; i++) world_step(&w, P1);
    CHECK(w.h[0].state == HS_AIR && w.h[0].x == x0, "straight up: no steering, presses in the air do nothing");
    run(&w, nj, NONE);
    CHECK(w.h[0].state == HS_STAND && w.h[0].level == 1, "lands on the cloud above");

    /* the landing margin: the hooves' centre up to LAND_MARGIN past the end lands, one more pixel misses */
    for (int k = 0; k < 2; k++) {
        still(&w, 1);
        cloud *c = cl(&w, 1);
        c->x = w.h[0].x + ((int32_t)(c->w / 2 + LAND_MARGIN + k) << 16);
        run(&w, nj + 1, P1);
        CHECK(k ? w.h[0].state == HS_FALL && w.h[0].why == FALL_MISS : w.h[0].state == HS_STAND,
              k ? "%d px past the end: a miss" : "%d px past the end: it lands (the fair margin)", LAND_MARGIN + k);
    }
    /* after landing on the end it steps inside */
    run(&w, 1, NONE);
    still(&w, 1);
    cl(&w, 1)->x = w.h[0].x + ((int32_t)(cl(&w, 1)->w / 2 + LAND_MARGIN) << 16);
    run(&w, nj + 40, P1);
    CHECK(abs((w.h[0].x - cl(&w, 1)->x) >> 16) <= cl(&w, 1)->w / 2 - SETTLE_IN, "it steps inside after landing on the end");

    /* a miss falls past every lower cloud: never lands back */
    still(&w, 1);
    put_on(&w, 0, 2);
    cl(&w, 3)->x = 40 << 16;
    w.h[0].x = cl(&w, 2)->x = 250 << 16;
    w.h[0].ox = 0;
    int landed = 0;
    world_step(&w, P1);
    for (int i = 0; i < 300 && w.h[0].state != HS_DOWN; i++) {
        world_step(&w, NONE);
        if (w.events[0] & EV_LAND) landed = 1;
    }
    CHECK(!landed && w.h[0].state == HS_DOWN && w.h[0].why == FALL_MISS && !world_running(&w),
          "a miss: it falls out of the sky, never onto a lower cloud");

    /* riding: it moves with its cloud; the clouds turn back inside the band */
    world_init(&w, 1, 5);
    run(&w, nj + 2, P1);
    int ok = 1;
    for (int i = 0; i < 3000; i++) {
        world_step(&w, NONE);
        const cloud *c = world_cloud(&w, 1);
        if (w.h[0].x != c->x + w.h[0].ox || (c->x >> 16) - c->w / 2 < BAND_L || (c->x >> 16) + c->w / 2 > BAND_R) ok = 0;
    }
    CHECK(ok && w.h[0].state == HS_STAND, "it rides its cloud, which turns back at the band's ends (3000 frames)");

    /* the pink cloud: a bounce of two levels; the level between does not catch it */
    still(&w, 1);
    cl(&w, 1)->type = CL_PINK;
    put_on(&w, 0, 1);
    world_step(&w, P1);
    CHECK(w.h[0].big && w.h[0].target == 3, "a jump from the pink cloud aims two levels up");
    run(&w, nb + 1, NONE);
    CHECK(w.h[0].state == HS_STAND && w.h[0].level == 3 && w.h[0].score == 3, "... and lands there (height 3)");
    still(&w, 1);
    cl(&w, 1)->type = CL_PINK;
    put_on(&w, 0, 1);
    cl(&w, 3)->x = 40 << 16;
    w.h[0].x = cl(&w, 1)->x = 250 << 16;
    w.h[0].ox = 0;
    run(&w, nb + 2, P1);
    CHECK(w.h[0].state == HS_FALL, "a pink bounce that misses two levels up falls (not onto the level between)");

    /* the rain cloud: it drips once landed on, melts RAIN_MELT later; its twin above waits, still */
    still(&w, 1);
    cl(&w, 1)->type = CL_RAIN;
    cl(&w, 1)->vx = cl(&w, 2)->vx = Q16_ONE / 3;
    run(&w, nj + 1, P1);
    CHECK(cl(&w, 1)->rain_t >= 0 && cl(&w, 1)->vx == 0 && cl(&w, 2)->vx == 0, "landing on a rain cloud: it drips and stops, "
          "and so does the cloud right above");
    run(&w, RAIN_MELT, NONE);
    CHECK(w.h[0].state == HS_FALL && w.h[0].why == FALL_MELT, "still on it after %d frames: it melted, the unicorn falls",
          RAIN_MELT);
    run(&w, RAIN_REFORM, NONE);
    CHECK(cloud_there(cl(&w, 1)) && cl(&w, 1)->rain_t < 0, "the rain cloud puffs back later (for the players behind)");
    world_init(&w, 1, 1);
    int twins = 0, rains = 0;
    for (int i = 0; i < 400; i++) {
        w.cam_level = i;
        world_step(&w, NONE);
        const cloud *a = world_cloud(&w, i), *b = world_cloud(&w, i + 1);
        if (a && b && a->type == CL_RAIN) { rains++; twins += a->x == b->x && a->w == b->w && a->vx == b->vx; }
    }
    CHECK(rains > 10 && twins == rains, "every rain cloud has its twin right above (%d of %d)", twins, rains);

    /* the thunder cloud: a touch while jumping zaps (hit-stop, then the fall); standing under it is safe */
    still(&w, 1);
    cl(&w, 0)->thunder = 1;
    cl(&w, 0)->tx = w.h[0].x;
    cl(&w, 0)->tvx = 0;
    run(&w, 200, NONE);
    CHECK(w.h[0].state == HS_READY, "standing right under a thunder cloud is safe");
    world_step(&w, P1);
    int zap = 0;
    for (int i = 0; i < 30; i++) { world_step(&w, NONE); if (w.events[0] & EV_ZAP) zap = 1; }
    CHECK(zap && (w.h[0].state == HS_FALL || w.h[0].state == HS_ZAP) && w.h[0].why == FALL_ZAP, "jumping into it: zapped");

    /* a star in the gap: caught while flying through it */
    still(&w, 1);
    cl(&w, 0)->star = 1;
    cl(&w, 0)->sx = (int16_t)(w.h[0].x >> 16);
    run(&w, nj + 1, P1);
    CHECK(w.h[0].stars == 1 && cl(&w, 0)->star_taken && w.h[0].score == 1 + STAR_POINTS, "a star: +%d (score %d)",
          STAR_POINTS, w.h[0].score);

    /* the rainbow every 25 levels */
    still(&w, 1);
    w.top = RAINBOW_EVERY - 1;
    for (int i = 0; i < RING; i++) w.c[i].index = -1;
    for (int L = RAINBOW_EVERY - 3; L < RAINBOW_EVERY + 4; L++) {
        cloud *c = &w.c[L & (RING - 1)];
        memset(c, 0, sizeof *c);
        c->index = L; c->x = CENTER_X << 16; c->w = 96; c->type = CL_NORMAL; c->rain_t = c->gone_t = -1;
    }
    w.top = RAINBOW_EVERY + 4;
    w.cam_level = RAINBOW_EVERY - 1;
    w.cam = (int32_t)(w.cam_level * LEVEL_H) << 8;
    put_on(&w, 0, RAINBOW_EVERY - 1);
    run(&w, nj + 1, P1);
    CHECK(w.h[0].rainbows == 1 && w.rainbow_t >= 0 && w.h[0].score == RAINBOW_EVERY + RAINBOW_BONUS,
          "level %d: a rainbow, +%d (score %d)", RAINBOW_EVERY, RAINBOW_BONUS, w.h[0].score);

    /* 2 players: the camera follows the leader; one left 3 levels behind is out */
    still(&w, 2);
    int up[MAX_PLAYERS] = {1, 0};
    for (int k = 0; k < 3; k++) {
        world_step(&w, up);
        run(&w, nj + 40, NONE);
    }
    CHECK(w.h[0].level == 3 && w.h[1].state == HS_DOWN && w.h[1].why == FALL_OUT, "P1 3 levels up: P2, left below "
          "the screen, is out");
    still(&w, 2);
    for (int k = 0; k < 2; k++) {
        world_step(&w, up);
        run(&w, nj + 40, NONE);
    }
    CHECK(w.h[1].state != HS_DOWN, "2 levels behind: still in");

    /* determinism: the same presses, the same world */
    world a, b;
    world_init(&a, 2, 99);
    world_init(&b, 2, 99);
    for (int i = 0; i < 4000; i++) {
        int pr[MAX_PLAYERS] = {i % 37 == 0, i % 53 == 0};
        world_step(&a, pr);
        world_step(&b, pr);
    }
    CHECK(!memcmp(&a, &b, sizeof a), "the same presses, the same world (4000 frames, 2 players)");

    /* the bot climbs: it times its jumps (never a miss, a zap or a melt) */
    for (int seed = 1; seed <= 3; seed++) {
        world_init(&w, 1, (uint32_t)seed);
        int f = 0;
        for (; f < 12000 && world_running(&w); f++) {
            int pr[MAX_PLAYERS] = {bot_decide(&w, 0, 0, 0)};
            world_step(&w, pr);
        }
        CHECK(world_running(&w) && w.h[0].height >= 60, "the bot (seed %d) climbs %d levels in %d frames", seed,
              w.h[0].height, f);
    }
    /* ... and its botstop mode misses on purpose */
    world_init(&w, 1, 4);
    for (int f = 0; f < 3000 && world_running(&w); f++) {
        int pr[MAX_PLAYERS] = {bot_decide(&w, 0, w.h[0].height >= 3, 0)};
        world_step(&w, pr);
    }
    CHECK(!world_running(&w) && w.h[0].height == 3 && w.h[0].why == FALL_MISS, "the bot's botstop: a miss at height 3");

    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
