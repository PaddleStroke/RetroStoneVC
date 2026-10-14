/*
 * Hamster Havoc: the rules alone (src/play.c, no video, no sound): the march (steps, the drop at each edge, the
 * speed-up as the mice thin out, each wave faster), the shelters eroded by both sides' shots and nibbled by the mice,
 * the seeds (one on screen, double, rapid), one hit ends a hamster's run (the bubble shield takes one), the landing,
 * the saucer, the mothership, the touch slide, determinism and the bot.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
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

static const pin NONE[MAX_PLAYERS] = {{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}};
static pin in[MAX_PLAYERS];

static void idle(void) { memcpy(in, NONE, sizeof in); }

/* a world in play (the intro over), the mice not firing, no saucer */
static void playing(world *w, int players, int wave)
{
    world_init(w, players, 1234);
    if (wave > 1) world_start_wave(w, wave);
    idle();
    in[0].fire = 1;
    world_step(w, in);
    idle();
    while (w->phase != PH_PLAY) world_step(w, in);
    w->bomb_t = 1 << 30;
    w->saucer_t = 1 << 30;
}

static int shelter_bits(const world *w)
{
    int n = 0;
    for (int s = 0; s < SHELTERS; s++)
        for (int y = 0; y < SHELTER_H; y++)
            for (int x = 0; x < SHELTER_W; x++) n += w->shelter[s][y] >> x & 1;
    return n;
}

/* a column with mice at the bottom and no shelter under it: its left x */
static int open_column(const world *w)
{
    for (int c = 0; c < COLS; c++) {
        int x = mouse_x(w, c) + 8, shel = 0;
        for (int y = SHELTER_Y; y < SHELTER_Y + SHELTER_H; y++) shel |= shelter_solid(w, x, y);
        if (!shel && w->alive[ROWS - 1][c] && x > SHIP_MIN_X && x < SHIP_MAX_X) return mouse_x(w, c);
    }
    return mouse_x(w, 0);
}

static void kill_all_but(world *w, int keep)
{
    int k = 0;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (w->alive[r][c] && k++ >= keep) {
                w->alive[r][c] = 0;
                w->alive_n--;
            }
}

static void test_start(void)
{
    world w;
    world_init(&w, 1, 7);
    CHECK(w.alive_n == 55 && w.count0 == 55 && w.wave == 1 && w.phase == PH_READY, "wave 1: 55 mice, ready");
    idle();
    for (int i = 0; i < 100; i++) world_step(&w, in);
    CHECK(w.t == 0 && !w.started && w.fx == FORM_X0, "nothing moves before the first press");
    in[0].fire = 1;
    world_step(&w, in);
    CHECK(w.started && w.phase == PH_INTRO && w.h[0].state == SH_PLAY, "a press starts the intro");
    CHECK(w.h[0].shots == 0, "no seed during the intro");
    for (int i = 0; i < INTRO_FRAMES; i++) world_step(&w, in);
    CHECK(w.phase == PH_PLAY, "the intro lasts %d frames", INTRO_FRAMES);
}

static void test_march(void)
{
    world w;
    playing(&w, 1, 1);
    CHECK(march_interval(&w) == MARCH_FULL, "a full formation steps every %d frames (%d)", MARCH_FULL,
          march_interval(&w));
    int fx = w.fx, ff = w.fframe, steps = 0;
    for (int i = 0; i < MARCH_FULL; i++) {
        world_step(&w, in);
        if (w.wevents & WEV_BEAT) steps++;
    }
    CHECK(steps == 1 && w.fx == fx + MARCH_DX && w.fframe != ff, "one step of %d px (and the beat, the next frame)",
          MARCH_DX);
    /* the right edge: the next step would pass BAND_R: it drops instead, and turns */
    w.fx = BAND_R - 16 - (COLS - 1) * COL_W - 1;
    w.fstep_t = march_interval(&w) - 1;
    int fy = w.fy;
    fx = w.fx;
    world_step(&w, in);
    CHECK(w.fy == fy + MARCH_DY && w.fx == fx && w.fdir == -1, "at the right edge it drops %d px and turns", MARCH_DY);
    w.fstep_t = march_interval(&w) - 1;
    world_step(&w, in);
    CHECK(w.fx == fx - MARCH_DX && w.fy == fy + MARCH_DY, "then it goes left");
    /* the left edge (with the left columns gone, the edge is the leftmost mouse's) */
    for (int r = 0; r < ROWS; r++) w.alive[r][0] = w.alive[r][1] = 0;
    w.alive_n -= 2 * ROWS;
    w.fx = BAND_L - 2 * COL_W + 1;
    w.fstep_t = march_interval(&w) - 1;
    fy = w.fy;
    world_step(&w, in);
    CHECK(w.fy == fy + MARCH_DY && w.fdir == 1, "the left edge is the leftmost mouse's: it drops and turns");
}

static void test_speedup(void)
{
    world w;
    playing(&w, 1, 1);
    int last = march_interval(&w), mono = 1;
    while (w.alive_n > 1) {
        kill_all_but(&w, w.alive_n - 1);
        int iv = march_interval(&w);
        if (iv > last) mono = 0;
        last = iv;
        if (w.alive_n == 28) CHECK(iv < MARCH_FULL * 2 / 3, "half the mice: faster (%d frames)", iv);
        if (w.alive_n == 2) CHECK(iv == MARCH_MIN, "two mice: every %d frames (%d)", MARCH_MIN, iv);
    }
    CHECK(mono, "fewer mice, never slower");
    CHECK(march_interval(&w) == 1, "the last mouse steps every frame");
    w.fstep_t = 0;
    int fx = w.fx;
    world_step(&w, in);
    CHECK(w.fx == fx + MARCH_LAST_DX || w.fy > FORM_Y0, "the last mouse runs %d px a step", MARCH_LAST_DX);
    world w5;
    playing(&w5, 1, 5);
    CHECK(march_interval(&w5) < MARCH_FULL && w5.fy > FORM_Y0, "wave 5 starts lower (%d) and faster (%d)", w5.fy,
          march_interval(&w5));
    world w2;
    playing(&w2, 1, 2);
    CHECK(w2.alive_n == 50 && w2.alive[0][1] == 0 && w2.alive[0][0] == 1, "wave 2: another layout (50 mice)");
}

static void test_seeds(void)
{
    world w;
    playing(&w, 1, 1);
    /* aim at column 3 */
    int col = (open_column(&w) - w.fx) / COL_W, x = mouse_x(&w, col) + 8;
    w.h[0].x = x << 16;
    in[0].fire = 1;
    world_step(&w, in);
    CHECK(w.h[0].shots == 1 && (w.events[0] & EV_FIRE), "A fires a seed");
    world_step(&w, in);
    CHECK(w.h[0].shots == 1, "one seed on screen at a time");
    in[0].fire = 0;
    int score = 0;
    for (int i = 0; i < 60 && !score; i++) {
        w.fstep_t = 0;                                  /* hold the formation still */
        world_step(&w, in);
        score = w.h[0].score;
    }
    CHECK(score == mouse_points(ROWS - 1) && !w.alive[ROWS - 1][col] && w.alive_n == 54,
          "the seed pops the bottom mouse of its column (+%d): %d", mouse_points(ROWS - 1), score);
    CHECK(mouse_points(0) == 30 && mouse_points(1) == 20 && mouse_points(4) == 10, "30, 20, 10 points by row");
    /* double seeds: two at once; rapid: up to three, a gap between */
    playing(&w, 1, 1);
    w.h[0].power = PW_DOUBLE;
    w.h[0].power_t = POWER_FRAMES;
    in[0].fire = 1;
    world_step(&w, in);
    int n = 0;
    for (int i = 0; i < MAX_SEEDS; i++) n += w.seeds[0][i].on;
    CHECK(n == 2, "double seeds: two at once (%d)", n);
    playing(&w, 1, 1);
    w.h[0].x = 30 << 16;                                /* under no mouse */
    w.fx = 120;
    w.h[0].power = PW_RAPID;
    w.h[0].power_t = POWER_FRAMES;
    in[0].fire = 1;
    for (int i = 0; i < 2 * RAPID_GAP + 1; i++) world_step(&w, in);
    n = 0;
    for (int i = 0; i < MAX_SEEDS; i++) n += w.seeds[0][i].on;
    CHECK(n == 3 && w.h[0].shots == 3, "rapid seeds: three on screen (%d, %d fired)", n, w.h[0].shots);
    w.h[0].power_t = 1;
    world_step(&w, in);
    CHECK(w.h[0].power == PW_NONE, "the power-up runs out");
}

static void test_shelters(void)
{
    world w;
    playing(&w, 1, 1);
    int full = shelter_bits(&w), sx = 40 + 12;
    CHECK(full > 4 * 24 * 16 / 2, "four cheese wedges (%d pixels)", full);
    CHECK(shelter_solid(&w, sx, SHELTER_Y + 10) && !shelter_solid(&w, 40, SHELTER_Y) && !shelter_solid(&w, sx, SHELTER_Y + 15),
          "a wedge: its slope, its mouse hole");
    w.h[0].x = (sx + 6) << 16;                          /* from below, beside the mouse hole */
    in[0].fire = 1;
    world_step(&w, in);
    in[0].fire = 0;
    for (int i = 0; i < 20; i++) world_step(&w, in);
    int after = shelter_bits(&w);
    CHECK(after < full && after >= full - 25 && w.h[0].score == 0, "a seed bites the cheese from below (-%d px)",
          full - after);
    int seeds = 0;
    for (int i = 0; i < MAX_SEEDS; i++) seeds += w.seeds[0][i].on;
    CHECK(seeds == 0, "and is gone");
    /* a crumb from above */
    playing(&w, 1, 1);
    full = shelter_bits(&w);
    w.bombs[0] = (bomb){(sx + 4) << 16, (SHELTER_Y - 20) << 16, 0, Q16_ONE, 1, BK_PEA};
    for (int i = 0; i < 30; i++) world_step(&w, in);
    after = shelter_bits(&w);
    CHECK(!w.bombs[0].on && after < full && after >= full - 25, "a crumb bites it from above (-%d px)", full - after);
    /* two more on the same spot dig deeper: through, eventually */
    int dug = 0;
    for (int k = 0; k < 12 && !dug; k++) {
        w.bombs[0] = (bomb){(sx + 4) << 16, (SHELTER_Y - 20) << 16, 0, Q16_ONE, 1, BK_PEA};
        for (int i = 0; i < 60 && w.bombs[0].on; i++) world_step(&w, in);
        dug = !w.bombs[0].on && w.phase == PH_PLAY && shelter_bits(&w) > 0 && !shelter_solid(&w, sx + 4, SHELTER_Y + 8);
    }
    CHECK(dug, "crumbs dig a hole through");
    /* the mice nibble what they march over */
    playing(&w, 1, 1);
    full = shelter_bits(&w);
    w.fy = SHELTER_Y - 4 * ROW_H - 6;                   /* the bottom row over the shelters */
    w.fstep_t = march_interval(&w) - 1;
    world_step(&w, in);
    CHECK(shelter_bits(&w) < full, "the mice nibble the cheese (%d -> %d)", full, shelter_bits(&w));
}

static void test_hits(void)
{
    world w;
    /* the bubble shield takes exactly one hit */
    playing(&w, 1, 1);
    w.h[0].shield_t = SHIELD_FRAMES;
    w.bombs[0] = (bomb){w.h[0].x, (SHIP_Y + 4) << 16, 0, Q16_ONE, 1, BK_CRUMB};
    w.bombs[1] = (bomb){w.h[0].x, (SHIP_Y - 30) << 16, 0, Q16_ONE, 1, BK_PEA};
    int shield = 0;
    for (int i = 0; i < 10; i++) {
        world_step(&w, in);
        shield |= w.events[0] & EV_SHIELD;
    }
    CHECK(shield && w.h[0].shield_t == 0 && w.h[0].state == SH_PLAY && w.phase == PH_PLAY, "the bubble takes a hit");
    /* ... and the next shot is the end: one hit, game over (no lives, no respawn) */
    int hit = 0;
    for (int i = 0; i < 40 && !hit; i++) {
        world_step(&w, in);
        hit = w.events[0] & EV_HIT;
    }
    CHECK(hit && (w.events[0] & EV_DOWN) && w.h[0].state == SH_POP && w.phase == PH_OVER && !world_running(&w),
          "one hit without the bubble: the run is over");
    CHECK(w.freeze == HIT_FREEZE, "the hit-stop");
    int fx = w.fx;
    for (int i = 0; i < 200; i++) world_step(&w, in);
    CHECK(w.h[0].state == SH_DOWN && world_all_down(&w) && w.fx == fx, "the ship pops, then nothing moves");
    /* 2 players: one hit puts P2 out, P1 plays on, nothing waits */
    playing(&w, 2, 1);
    w.bombs[0] = (bomb){w.h[1].x, (SHIP_Y + 4) << 16, 0, Q16_ONE, 1, BK_CRUMB};
    w.bombs[1] = (bomb){((w.h[0].x >> 16) - 60) << 16, 100 << 16, 0, Q16_ONE, 1, BK_PEA};
    world_step(&w, in);
    CHECK((w.events[1] & EV_DOWN) && w.freeze == 0 && w.bombs[1].on, "2 players: P2 is out, no pause, the shots go on");
    for (int i = 0; i < 200; i++) world_step(&w, in);
    CHECK(w.h[1].state == SH_DOWN && w.h[0].state == SH_PLAY && world_running(&w), "... and P1 plays on");
    w.bombs[0] = (bomb){w.h[0].x, (SHIP_Y + 4) << 16, 0, Q16_ONE, 1, BK_CRUMB};
    for (int i = 0; i < 5; i++) world_step(&w, in);
    CHECK(w.phase == PH_OVER && !world_running(&w), "the run ends when the last hamster is out");
}


static void test_landing(void)
{
    world w;
    playing(&w, 2, 1);
    w.fy = INVADE_Y - 14 - 4 * ROW_H - MARCH_DY;        /* the next drop reaches the ships' line */
    w.fx = BAND_R - 16 - (COLS - 1) * COL_W - 1;
    w.fstep_t = march_interval(&w) - 1;
    world_step(&w, in);
    CHECK(w.landed && w.phase == PH_OVER && w.h[0].state == SH_POP && w.h[1].state == SH_POP, "the mice landed: game over");
    for (int i = 0; i < POP_FRAMES + 2; i++) world_step(&w, in);
    CHECK(world_all_down(&w), "every ship pops");
}

static void test_saucer(void)
{
    world w;
    playing(&w, 1, 1);
    w.saucer_t = 1;
    world_step(&w, in);
    CHECK(w.saucer_on && w.saucer_dir == 1, "the saucer comes after %d frames", SAUCER_FIRST);
    w.saucer_x = 122 << 16;                             /* it flies 1 px a frame: over the ship in 30 frames */
    w.h[0].x = 151 << 16;
    w.fx = 200;                                          /* no mouse in the way */
    in[0].fire = 1;
    int pts = 0;
    for (int i = 0; i < 60 && !pts; i++) {
        world_step(&w, in);
        in[0].fire = 0;
        pts = w.h[0].score;
    }
    CHECK(!w.saucer_on && (pts == 50 || pts == 100 || pts == 150 || pts == 300), "mystery points: %d", pts);
    /* its power-up: caught by the ship below */
    w.power_on = 1;
    w.power_kind = PW_SHIELD;
    w.power_x = w.h[0].x;
    w.power_y = (SHIP_Y - 20) << 16;
    for (int i = 0; i < 40; i++) world_step(&w, in);
    CHECK(!w.power_on && w.h[0].shield_t > 0 && w.msg == MSG_SHIELD, "the capsule is caught: a bubble shield");
}

static void test_boss(void)
{
    world w;
    playing(&w, 1, 4);
    CHECK(world_is_boss(&w) && w.boss_on && w.boss_hp == BOSS_HP && w.alive_n == 0, "wave 4: the mothership, %d hits",
          BOSS_HP);
    world w2;
    playing(&w2, 2, 8);
    CHECK(w2.boss_hp == BOSS_HP + BOSS_HP_PLAYER, "2 players: %d hits", BOSS_HP + BOSS_HP_PLAYER);
    w.boss_fire_t = 1 << 30;
    in[0].fire = 1;
    int hits = 0;
    for (int i = 0; i < 4000 && w.phase == PH_PLAY; i++) {
        w.h[0].x = w.boss_x;
        world_step(&w, in);
        if (w.events[0] & EV_BOSS_HIT) hits++;
    }
    CHECK(hits == BOSS_HP && !w.boss_on && w.phase == PH_CLEAR, "popped after %d hits", hits);
    CHECK(w.h[0].score == BOSS_HP * BOSS_HIT_POINTS + BOSS_POINTS, "points: %d", w.h[0].score);
    while (w.phase != PH_INTRO) world_step(&w, in);
    CHECK(w.wave == 5 && w.alive_n == w.count0 && w.alive_n > 0, "then wave 5");
    /* it fires spreads of three */
    playing(&w, 1, 4);
    w.boss_fire_t = 1;
    world_step(&w, in);
    int n = 0, left = 0, right = 0;
    for (int i = 0; i < BOMB_MAX; i++)
        if (w.bombs[i].on) {
            n++;
            left += w.bombs[i].vx < 0;
            right += w.bombs[i].vx > 0;
        }
    CHECK(n == 3 && left == 1 && right == 1, "a spread of three peas");
}

static void test_touch(void)
{
    world w;
    playing(&w, 1, 1);
    w.h[0].x = 100 << 16;
    idle();
    in[0].target = 160;
    world_step(&w, in);
    CHECK(w.h[0].x == (100 << 16) + SHIP_SPEED, "a touch target: the ship slides at full speed");
    for (int i = 0; i < 100; i++) world_step(&w, in);
    CHECK(w.h[0].x == 160 << 16, "and stops under the finger");
    in[0].target = 400;
    for (int i = 0; i < 300; i++) world_step(&w, in);
    CHECK(w.h[0].x == SHIP_MAX_X << 16, "never past the edge");
}

static uint32_t world_hash(const world *w)
{
    const uint8_t *p = (const uint8_t *)w;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sizeof *w; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

static void test_determinism_and_bot(void)
{
    world a, b;
    world_init(&a, 2, 99);
    world_init(&b, 2, 99);
    for (int t = 0; t < 4000; t++) {
        pin ia[MAX_PLAYERS], ib[MAX_PLAYERS];
        memcpy(ia, NONE, sizeof ia);
        memcpy(ib, NONE, sizeof ib);
        for (int p = 0; p < 2; p++) {
            ia[p] = bot_decide(&a, p, p * 4);
            ib[p] = bot_decide(&b, p, p * 4);
        }
        world_step(&a, ia);
        world_step(&b, ib);
    }
    CHECK(world_hash(&a) == world_hash(&b) && a.t > 3000, "the same seed and inputs, the same run");
    /* the bot clears waves without a hit */
    world w;
    world_init(&w, 1, 5);
    for (int t = 0; t < 9000; t++) {
        pin i2[MAX_PLAYERS];
        memcpy(i2, NONE, sizeof i2);
        i2[0] = bot_decide(&w, 0, 0);
        world_step(&w, i2);
    }
    CHECK(w.wave >= 4 && w.h[0].state == SH_PLAY, "the bot reaches wave %d unhit", w.wave);
}

int main(void)
{
    test_start();
    test_march();
    test_speedup();
    test_seeds();
    test_shelters();
    test_hits();
    test_landing();
    test_saucer();
    test_boss();
    test_touch();
    test_determinism_and_bot();
    printf("hamsterhavoc rules: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
