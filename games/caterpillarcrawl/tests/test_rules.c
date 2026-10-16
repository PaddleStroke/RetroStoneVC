/*
 * Caterpillar Crawl: the rules alone (src/play.c): moving on the clock, the turn buffer (no instant reverse, a U-turn
 * in two steps), growing from a leaf, the walls, the obstacles and its own body, crawling into the cell its tail leaves,
 * the treats, the butterfly and the next garden, the arena (growing as it crawls, any body is out, heads meeting, the
 * last one crawling wins the round, the match), determinism, and the bot.
 *   build/host/caterpillarcrawl_rules
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE.
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

static const int NONE[MAX_PLAYERS] = {0};
#define CELLOF(x, y) ((y) * GRID_W + (x))

static int hx(const world *w, int p) { return cat_cell(&w->c[p], 0) % GRID_W; }
static int hy(const world *w, int p) { return cat_cell(&w->c[p], 0) / GRID_W; }

/* run until the next step (and through it); returns the frames it took */
static int to_step(world *w, const int *press)
{
    int n = 0;
    do {
        world_step(w, n == 0 && press ? press : NONE);
        n++;
    } while (!w->stepped && n < 1000);
    return n;
}

static void steps(world *w, int k)
{
    for (int i = 0; i < k; i++) to_step(w, NULL);
}

static void press1(world *w, int dir)
{
    int pr[MAX_PLAYERS] = {1 + dir};
    world_step(w, pr);
}

/* 1 player, started, the leaf moved out of the way (to a far corner) */
static void solo(world *w, uint32_t seed)
{
    int go[MAX_PLAYERS] = {PRESS_GO};
    world_init(w, 1, seed);
    w->leaf = CELLOF(GRID_W - 1, 0);
    world_step(w, go);
}

int main(void)
{
    world w, a, b;
    printf("Caterpillar Crawl rules: %d x %d cells, speed %d.%02d cells/s to start\n", GRID_W, GRID_H,
           SPEED_START / 100, SPEED_START % 100);

    /* waiting, the start */
    world_init(&w, 1, 3);
    for (int i = 0; i < 600; i++) world_step(&w, NONE);
    CHECK(!w.started && w.c[0].state == CS_READY && hx(&w, 0) == START_X, "it waits for a press (600 frames)");
    solo(&w, 3);
    CHECK(w.started && w.c[0].state == CS_CRAWL && w.stepped && hx(&w, 0) == START_X + 1 && hy(&w, 0) == START_Y,
          "A starts it: the first step at once, to the right");

    /* moving on the clock: 6 cells a second (10 frames a step) at the start */
    solo(&w, 3);
    int f = to_step(&w, NULL);
    CHECK(f == 10 && hx(&w, 0) == START_X + 2, "a step every %d frames (6 cells a second)", f);
    CHECK(w.c[0].len == START_LEN, "the body follows: still %d segments", w.c[0].len);

    /* turns: buffered, never straight back */
    solo(&w, 3);
    press1(&w, DIR_LEFT);
    CHECK(w.c[0].nq == 0, "a press straight back into itself is ignored");
    press1(&w, DIR_RIGHT);
    CHECK(w.c[0].nq == 0, "a press the way it goes already is ignored");
    press1(&w, DIR_UP);
    press1(&w, DIR_LEFT);
    CHECK(w.c[0].nq == 2, "a quick double press (up, left) is buffered");
    int x0 = hx(&w, 0), y0 = hy(&w, 0);
    to_step(&w, NULL);
    CHECK(hx(&w, 0) == x0 && hy(&w, 0) == y0 - 1, "step 1 of the U-turn: up");
    to_step(&w, NULL);
    CHECK(hx(&w, 0) == x0 - 1 && hy(&w, 0) == y0 - 1 && w.c[0].state == CS_CRAWL, "step 2: left, back along itself");
    press1(&w, DIR_DOWN);
    press1(&w, DIR_RIGHT);
    press1(&w, DIR_UP);
    CHECK(w.c[0].nq == 2 && w.c[0].queue[0] == DIR_DOWN && w.c[0].queue[1] == DIR_RIGHT,
          "the buffer holds %d turns (a third press waits)", TURN_BUFFER);
    solo(&w, 3);
    press1(&w, DIR_UP);
    press1(&w, DIR_DOWN);
    CHECK(w.c[0].nq == 1, "up then down: down would be straight back after up, ignored");

    /* a leaf: +1, the body grows by GROW_PER_LEAF, one segment a step */
    solo(&w, 3);
    w.leaf = cat_cell(&w.c[0], 0) + 1;
    to_step(&w, NULL);
    CHECK(w.c[0].score == 1 && w.c[0].leaves == 1 && w.c[0].grow == GROW_PER_LEAF && (w.events[0] & EV_LEAF),
          "a leaf eaten: 1 point, %d segments to grow", GROW_PER_LEAF);
    CHECK(w.leaf >= 0 && !w.occ[w.leaf], "a new leaf grows on a free cell");
    CHECK(w.fat[cat_cell(&w.c[0], 0)], "the swallowed leaf makes a fat segment");
    w.leaf = CELLOF(0, GRID_H - 1);                 /* out of the way */
    steps(&w, 2);
    CHECK(w.c[0].len == START_LEN + GROW_PER_LEAF && w.c[0].grow == 0, "two steps later it is %d long", w.c[0].len);
    int fat_cell = cat_cell(&w.c[0], 2);
    CHECK(w.fat[fat_cell], "the bulge stays where it was swallowed");
    steps(&w, w.c[0].len);
    CHECK(!w.fat[fat_cell], "... until the tail passes");

    /* walls */
    solo(&w, 3);
    w.leaf = -1;
    steps(&w, GRID_W - START_X - 2);
    CHECK(hx(&w, 0) == GRID_W - 1 && w.c[0].state == CS_CRAWL, "it reaches the right edge");
    to_step(&w, NULL);
    CHECK(w.c[0].state == CS_HIT && w.c[0].why == HIT_WALL && hx(&w, 0) == GRID_W - 1 && !world_running(&w),
          "one more step: the wall, the run is over");
    CHECK(w.c[0].len == START_LEN && w.occ[cat_cell(&w.c[0], START_LEN - 1)] == 1, "the body stays whole");
    for (int i = 0; i < 200; i++) world_step(&w, NONE);
    CHECK(w.c[0].state == CS_DOWN && world_over_t(&w) >= PANEL_DELAY, "the bonk, then down");

    /* an obstacle (garden 2: stones) */
    world_init(&w, 1, 3);
    world_start_garden(&w, 1);
    CHECK(world_obstacle(&w, CELLOF(3, 2)) == OB_ROCK && w.occ[CELLOF(3, 2)] == CELL_ROCK, "garden 2 has stones");
    CHECK(w.phase == PH_PLAY || w.phase == PH_GARDEN || w.phase == PH_READY, "garden 2 starts");
    {
        int go[MAX_PLAYERS] = {PRESS_GO};
        world_step(&w, go);
    }
    while (w.phase != PH_PLAY) world_step(&w, NONE);
    w.leaf = -1;
    press1(&w, DIR_UP);
    steps(&w, 3);                                   /* up from (6,6) to (6,3), then left into the stones at x 3-4 */
    press1(&w, DIR_LEFT);
    for (int i = 0; i < 6 && w.c[0].state == CS_CRAWL; i++) to_step(&w, NULL);
    CHECK(w.c[0].state == CS_HIT && w.c[0].why == HIT_ROCK, "into a stone: out (%d)", w.c[0].why);

    /* its own body: a long caterpillar turning into itself */
    solo(&w, 3);
    w.c[0].grow = 6;
    steps(&w, 6);
    press1(&w, DIR_DOWN);
    to_step(&w, NULL);
    press1(&w, DIR_LEFT);
    to_step(&w, NULL);
    press1(&w, DIR_UP);
    to_step(&w, NULL);
    CHECK(w.c[0].state == CS_HIT && w.c[0].why == HIT_SELF, "turning into its own body: out");

    /* ... but into the cell its tail leaves this very step: fine (a 4-long caterpillar chasing its tail) */
    solo(&w, 3);
    press1(&w, DIR_DOWN);
    to_step(&w, NULL);
    press1(&w, DIR_LEFT);
    to_step(&w, NULL);
    press1(&w, DIR_UP);
    to_step(&w, NULL);
    CHECK(w.c[0].state == CS_CRAWL, "a square of 4: it crawls into the cell its tail just left");
    for (int i = 0; i < 12; i++) {
        press1(&w, (DIR_RIGHT + i) & 3);
        to_step(&w, NULL);
    }
    CHECK(w.c[0].state == CS_CRAWL && w.c[0].len == START_LEN, "and round and round (12 more steps)");

    /* the treats */
    solo(&w, 3);
    w.treat = cat_cell(&w.c[0], 0) + 1;
    w.treat_kind = TREAT_FLOWER;
    to_step(&w, NULL);
    CHECK(w.c[0].score == FLOWER_POINTS && w.c[0].grow == 0 && w.treat < 0, "a flower: %d points, no growth",
          FLOWER_POINTS);
    solo(&w, 3);
    int sp0 = world_speed(&w);
    w.treat = cat_cell(&w.c[0], 0) + 1;
    w.treat_kind = TREAT_BERRY;
    to_step(&w, NULL);
    CHECK(w.slow_t > 0 && world_speed(&w) == sp0 * SLOW_PCT / 100, "a berry: time runs at %d%% (%d -> %d)", SLOW_PCT,
          sp0, world_speed(&w));
    int s0 = w.slow_t;
    for (int i = 0; i < 100; i++) world_step(&w, NONE);
    CHECK(w.c[0].state == CS_CRAWL && w.slow_t == s0 - 100, "for %d frames (%d left after 100)", SLOW_FRAMES,
          w.slow_t);
    w.slow_t = 1;
    world_step(&w, NONE);
    CHECK(w.slow_t == 0 && world_speed(&w) == sp0, "then the speed is back");
    solo(&w, 3);
    w.treat = CELLOF(0, 11);
    w.treat_kind = TREAT_FLOWER;
    w.treat_t = TREAT_LIFE - 2;
    for (int i = 0; i < 2; i++) world_step(&w, NONE);
    CHECK(w.treat < 0 && (w.wevents & WEV_TREAT_GONE), "a treat wilts after %d frames", TREAT_LIFE);
    world_init(&w, 1, 5);
    {
        int go[MAX_PLAYERS] = {PRESS_GO};
        world_step(&w, go);
    }
    w.c[0].leaves = FLOWER_EVERY - 1;
    w.leaf = cat_cell(&w.c[0], 0) + 1;
    to_step(&w, NULL);
    CHECK(w.treat >= 0 && w.treat_kind == TREAT_FLOWER, "a flower blooms after the %dth leaf", FLOWER_EVERY);

    /* speed rises with the length (and the garden) */
    solo(&w, 3);
    int s4 = world_speed(&w);
    w.c[0].len = START_LEN + 20;
    CHECK(world_speed(&w) == s4 + 20 * SPEED_LEN, "speed: %d at %d segments, %d at %d", s4, START_LEN,
          world_speed(&w), START_LEN + 20);
    w.c[0].len = 200;
    CHECK(world_speed(&w) == SPEED_MAX, "at most %d", SPEED_MAX);

    /* the butterfly: the 12th leaf, the cocoon, the bonus, the next garden */
    solo(&w, 3);
    w.garden_leaves = LEAVES_PER_GARDEN - 1;
    int sc0 = w.c[0].score;
    w.leaf = cat_cell(&w.c[0], 0) + 1;
    to_step(&w, NULL);
    CHECK(w.phase == PH_META && w.c[0].state == CS_COCOON && (w.wevents & WEV_COCOON), "the 12th leaf: the cocoon");
    for (int i = 0; i < META_FRAMES && w.phase == PH_META; i++) world_step(&w, NONE);
    CHECK(w.c[0].butterflies == 1 && w.c[0].score == sc0 + 1 + BUTTERFLY_POINTS, "the butterfly: +%d (score %d)",
          BUTTERFLY_POINTS, w.c[0].score);
    CHECK(w.phase == PH_GARDEN && w.garden == 1 && w.layout == 1 && w.c[0].len == START_LEN &&
          w.c[0].state == CS_WAIT && w.garden_leaves == 0, "garden 2: a little caterpillar again, new obstacles");
    press1(&w, DIR_UP);
    int g = 0;
    while (w.phase == PH_GARDEN && g < 500) world_step(&w, NONE), g++;
    CHECK(w.phase == PH_PLAY && w.stepped && hy(&w, 0) == START_Y - 1, "after %d frames it crawls (the turn pressed "
          "during the pause taken)", g + 1);
    CHECK(world_speed(&w) == SPEED_START + SPEED_GARDEN, "a little faster: %d", world_speed(&w));

    /* the arena */
    world_init(&w, 4, 9);
    CHECK(w.mode == MODE_ARENA && w.c[3].len == ARENA_LEN && w.phase == PH_READY, "4 players: the arena");
    {
        int go[MAX_PLAYERS] = {PRESS_GO};
        world_step(&w, go);
    }
    CHECK(w.phase == PH_ROUND && w.round == 1, "a press: ROUND 1");
    int r = 0;
    while (w.phase == PH_ROUND && r < 500) world_step(&w, NONE), r++;
    CHECK(w.phase == PH_PLAY && r + 1 == ROUND_PAUSE, "the crawl starts after %d frames", r + 1);
    steps(&w, 4);
    CHECK(w.c[0].len == ARENA_LEN + 2 && w.c[1].len == ARENA_LEN + 2, "they grow as they crawl (+1 every %d cells)",
          ARENA_GROW_EVERY);
    /* P3 (heading down along the right edge) runs into the bottom wall; the others turn away in time */
    while (w.c[2].state == CS_CRAWL && r < 3000) {
        int pr[MAX_PLAYERS] = {0};
        for (int p = 0; p < 4; p++)
            if (p != 2) pr[p] = bot_decide(&w, p, 0, 0);
        world_step(&w, pr);
        r++;
    }
    CHECK(w.c[2].state == CS_HIT && w.c[2].why == HIT_WALL, "P3 crawls on into the wall: out");
    for (int i = 0; i < HIT_FRAMES + 10; i++) {
        int pr[MAX_PLAYERS] = {bot_decide(&w, 0, 0, 0), bot_decide(&w, 1, 0, 0), 0, bot_decide(&w, 3, 0, 0)};
        world_step(&w, pr);
    }
    CHECK(w.c[2].state == CS_MELT || w.c[2].state == CS_DOWN, "its body melts away");
    while (w.phase == PH_PLAY && r < 20000) {
        int pr[MAX_PLAYERS] = {bot_decide(&w, 0, 0, 0), bot_decide(&w, 1, 0, 0), 0, bot_decide(&w, 3, 0, 0)};
        world_step(&w, pr);
        r++;
    }
    int alive = 0;
    for (int p = 0; p < 4; p++) alive += w.c[p].state == CS_CRAWL || w.c[p].state == CS_WAIT;
    CHECK(w.phase == PH_ROUND_END && w.round_winner >= 0 && w.c[w.round_winner].wins == 1 && alive == 1,
          "the last one crawling (P%d) wins the round", w.round_winner + 1);
    for (int i = 0; i < ROUND_END; i++) world_step(&w, NONE);
    CHECK(w.phase == PH_ROUND && w.round == 2 && w.c[2].len == ARENA_LEN && w.layout == 1,
          "round 2: everyone back, a new layout");

    /* any body is out: P1's head into P2's body */
    world_init(&w, 2, 9);
    {
        int go[MAX_PLAYERS] = {PRESS_GO};
        world_step(&w, go);
    }
    while (w.phase != PH_PLAY) world_step(&w, NONE);
    w.occ[CELLOF(hx(&w, 0) + 2, hy(&w, 0))] = 2;    /* a piece of P2 right ahead of P1 */
    steps(&w, 2);
    CHECK(w.c[0].state == CS_HIT && w.c[0].why == HIT_OTHER, "P1 into P2's body: out");
    CHECK(w.phase == PH_ROUND_END && w.round_winner == 1, "2 players: P2 wins the round at once");

    /* heads meeting in one cell: both out, a draw */
    world_init(&w, 2, 9);
    {
        int go[MAX_PLAYERS] = {PRESS_GO};
        world_step(&w, go);
    }
    while (w.phase != PH_PLAY) world_step(&w, NONE);
    memset(w.occ, 0, sizeof w.occ);
    for (int p = 0; p < 2; p++) {                   /* face to face on row 5, two cells apart */
        cat *c = &w.c[p];
        c->len = 1;
        c->head = 0;
        c->body[0] = (uint8_t)CELLOF(p ? 10 : 8, 5);
        c->dir = p ? DIR_LEFT : DIR_RIGHT;
        c->grow = 0;
        w.occ[c->body[0]] = (uint8_t)(p + 1);
    }
    to_step(&w, NULL);
    CHECK(w.c[0].state == CS_HIT && w.c[1].state == CS_HIT && w.phase == PH_ROUND_END && w.round_winner < 0,
          "two heads into the same cell: both out, nobody wins the round");

    /* the match: first to ARENA_WINS */
    world_init(&w, 2, 9);
    w.c[1].wins = ARENA_WINS - 1;
    {
        int go[MAX_PLAYERS] = {PRESS_GO};
        world_step(&w, go);
    }
    while (w.phase != PH_PLAY) world_step(&w, NONE);
    while (w.phase == PH_PLAY) {
        int pr[MAX_PLAYERS] = {0, bot_decide(&w, 1, 0, 0)};
        world_step(&w, pr);
    }
    CHECK(w.phase == PH_OVER && w.c[1].wins == ARENA_WINS && !world_running(&w), "P2's %dth round: the match is over",
          ARENA_WINS);

    /* determinism */
    world_init(&a, 4, 99);
    world_init(&b, 4, 99);
    for (int i = 0; i < 6000; i++) {
        int pr[MAX_PLAYERS] = {bot_decide(&a, 0, 0, 0), bot_decide(&a, 1, 3, 0), bot_decide(&a, 2, 0, 0),
                               i % 37 == 0 ? 1 + (i / 37) % 4 : 0};
        world_step(&a, pr);
        world_step(&b, pr);
    }
    CHECK(!memcmp(&a, &b, sizeof a), "the same presses, the same world (6000 frames, 4 players)");

    /* the bot: it eats and lives on (3 seeds); botstop crashes it */
    for (int seed = 1; seed <= 3; seed++) {
        world_init(&w, 1, (uint32_t)seed);
        int fr = 0;
        for (; fr < 12000 && world_running(&w); fr++) {
            int pr[MAX_PLAYERS] = {bot_decide(&w, 0, 0, 0)};
            world_step(&w, pr);
        }
        CHECK(w.c[0].leaves >= 30, "the bot (seed %d): %d leaves, garden %d in %d frames%s", seed, w.c[0].leaves,
              w.garden + 1, fr, world_running(&w) ? "" : " (then out)");
    }
    world_init(&w, 1, 4);
    for (int fr = 0; fr < 3000 && world_running(&w); fr++) {
        int pr[MAX_PLAYERS] = {bot_decide(&w, 0, 0, w.c[0].score >= 3)};
        world_step(&w, pr);
    }
    CHECK(!world_running(&w) && w.c[0].score >= 3 && w.c[0].score <= 4, "the bot's botstop: out at score %d",
          w.c[0].score);

    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
