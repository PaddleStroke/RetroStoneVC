/*
 * Ladybug Laps: the rules alone (src/play.c and src/tracks.c, no video, no sound, no screen width: this program links
 * them with the SDK's RNG only): the tracks (closed, on the band, the start grid), the handling (gas, top speed,
 * brake, reverse, steering toward the direction held), the surfaces (rough, jam, puddle, a hop over a bump, a fall off
 * the table), the bumps (two ladybugs, an object, a wall), the laps and the checkpoints (no shortcut), the left-behind
 * rule (the fixed zone, the point to the leader, put back behind it), the bots completing every track both ways, the
 * points and the championship, the touch geometry at every width, determinism.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
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
#define PX(q) ((int)((q) / FX))
static const char *const NAMES[TRACKS] = {"tea", "desk", "picnic", "veg", "patio"};

static world W, W2;
static pin in[RACERS];

/* a race already going (no countdown), the 3 bots parked far away (out of the way: they do not move) */
static void solo(world *w, int track, int humans)
{
    world_init(w, track, 0, 5, humans, 77);
    w->phase = PH_RACE;
    w->phase_t = DROP_GRACE + 1000;             /* the left-behind rule on */
    for (int i = humans; i < RACERS; i++) {
        w->c[i].state = CS_OUT;
        w->c[i].timer = 30000;
    }
    memset(in, 0, sizeof in);
}

/* put car i at (x, y) px, heading a, still */
static void put(world *w, int i, int x, int y, int a)
{
    car *c = &w->c[i];
    c->x = x * FX;
    c->y = y * FX;
    c->vx = c->vy = c->z = c->vz = 0;
    c->ang = (int16_t)a;
    c->state = CS_RACE;
    c->ghost = 0;
    c->surf = (uint8_t)surface_at(w, x, y);
}

static int iabs_v(int v) { return v < 0 ? -v : v; }
static void steps(world *w, int n) { for (int k = 0; k < n; k++) world_step(w, in); }

static void test_tracks(void)
{
    for (int t = 0; t < TRACKS; t++)
        for (int rev = 0; rev < 2; rev++) {
            int ok = track_build(&W, t, rev);
            CHECK(ok, "%s (reverse %d) builds", NAMES[t], rev);
            if (!ok) continue;
            const waypoints *p = &W.wp;
            CHECK(p->n > 100 && p->n <= MAX_WP && p->len > 2000 && p->len < 5000, "%s: %d waypoints, %d px", NAMES[t],
                  p->n, p->len);
            int off = 0;
            for (int i = 0; i < p->n; i++) {
                int s = surface_at(&W, p->x[i], p->y[i]);
                if (s == SF_ROUGH || s == SF_FALL || s == SF_WALL) off++;
                int j = (i + 1) % p->n, dx = p->x[j] - p->x[i], dy = p->y[j] - p->y[i];
                if (dx * dx + dy * dy > 20 * 20 || dx * dx + dy * dy < 8 * 8) off += 100;
            }
            CHECK(off == 0, "%s (reverse %d): the centre line stays on the band, waypoints ~16 px apart (%d)", NAMES[t], rev, off);
            world_init(&W, t, rev, 0, 1, 5);
            for (int i = 0; i < RACERS; i++) {
                int s = surface_at(&W, PX(W.c[i].x), PX(W.c[i].y));
                CHECK(s == SF_TRACK, "%s (reverse %d): car %d starts on the band (surface %d)", NAMES[t], rev, i, s);
                CHECK(W.c[i].lap == -1 && W.c[i].prog < 0 && W.c[i].prog > -80, "%s: car %d behind the line (prog %d)",
                      NAMES[t], i, W.c[i].prog);
            }
            for (int i = 0; i < RACERS; i++)    /* the grid: no two ladybugs on top of each other */
                for (int j = i + 1; j < RACERS; j++) {
                    int dx = PX(W.c[i].x - W.c[j].x), dy = PX(W.c[i].y - W.c[j].y);
                    CHECK(dx * dx + dy * dy >= 4 * CAR_R * CAR_R, "%s: cars %d and %d apart on the grid", NAMES[t], i, j);
                }
        }
}

static void test_handling(void)
{
    /* tea's top straight runs east at y = 96 from x = 192 to 1088 */
    solo(&W, 3, 1);                            /* veg's first straight: east at y 96, x 256..896, clear */
    put(&W, 0, 300, 96, 0);
    in[0].gas = 1;
    steps(&W, 120);
    int f = car_speed(&W.c[0]);
    CHECK(f >= 680 && f <= 704, "A held 2 s: top speed on the band (%d / 704)", f);
    CHECK(PX(W.c[0].x) > 300 + 200, "it went east (x %d)", PX(W.c[0].x));
    in[0].gas = 0;
    in[0].brake = 1;
    steps(&W, 40);
    CHECK(car_speed(&W.c[0]) <= 0, "B brakes to a stop in under 40 frames (%d)", car_speed(&W.c[0]));
    steps(&W, 80);
    CHECK(car_speed(&W.c[0]) <= -REV_MAX + REV_ACCEL && car_speed(&W.c[0]) >= -REV_MAX - REV_ACCEL, "B held still: reverse (%d)",
          car_speed(&W.c[0]));
    /* steering: toward the direction held (south from east: clockwise), at the turn rate */
    solo(&W, 3, 1);
    put(&W, 0, 300, 96, 0);
    in[0].gas = 1;
    steps(&W, 60);
    in[0].dx = 0;
    in[0].dy = 256;
    int a0 = W.c[0].ang;
    steps(&W, 4);
    int d = ang_diff(a0, W.c[0].ang);
    CHECK(d == 4 * TURN_RATE, "down held: it turns clockwise toward south at %d a frame (%d in 4)", TURN_RATE, d);
    steps(&W, 40);
    CHECK(W.c[0].ang == 256, "... and stops turning there (%d)", W.c[0].ang);
    in[0].dx = -256;                            /* from south to west: clockwise again (the shorter way) */
    in[0].dy = 0;
    a0 = W.c[0].ang;
    steps(&W, 2);
    CHECK(ang_diff(a0, W.c[0].ang) > 0, "west from south: clockwise");
    in[0].dx = in[0].dy = 0;
    a0 = W.c[0].ang;
    steps(&W, 10);
    CHECK(W.c[0].ang == a0, "the d-pad let go: the heading is kept");
    /* the drift: a hard turn at speed leaves a sideways slide */
    solo(&W, 3, 1);
    put(&W, 0, 300, 96, 0);
    in[0].gas = 1;
    steps(&W, 100);
    in[0].dy = 256;
    steps(&W, 8);
    int32_t lat = (int32_t)(((int64_t)-W.c[0].vx * isin(W.c[0].ang) + (int64_t)W.c[0].vy * icos(W.c[0].ang)) >> 14);
    CHECK(lat < -120, "a hard turn at full speed slides sideways (the drift: %d)", lat);
}


/* a car flat out on a surface for 3 s: its speed */
static int top_on(int track, int x, int y, int a)
{
    solo(&W, track, 1);
    put(&W, 0, x, y, a);
    in[0].gas = 1;
    int best = 0;
    for (int k = 0; k < 90; k++) {
        world_step(&W, in);
        W.c[0].x = x * FX;                      /* held in place: only the speed matters */
        W.c[0].y = y * FX;
        if (car_speed(&W.c[0]) > best) best = car_speed(&W.c[0]);
    }
    return best;
}

static void test_surfaces(void)
{
    static const int16_t tab[SF_COUNT][4] = SURF_TABLE;
    CHECK(surface_at(&W, 0, 0) >= 0, "surface_at");
    world_init(&W, 0, 0, 5, 1, 1);
    CHECK(surface_at(&W, 600, 96) == SF_TRACK, "tea: the top straight is the band");
    CHECK(surface_at(&W, 600, 300) == SF_ROUGH, "tea: the table between is rough");
    CHECK(surface_at(&W, 10, 300) == SF_FALL, "tea: off the table is a fall");
    CHECK(surface_at(&W, 1008, 736) == SF_JAM, "tea: the jam");
    CHECK(surface_at(&W, 1184, 576) == SF_BUMP, "tea: the spoon is a bump");
    world_init(&W, 4, 0, 5, 1, 1);
    CHECK(surface_at(&W, 576, 736) == SF_WATER, "patio: the puddle");
    CHECK(surface_at(&W, 4, 300) == SF_WALL, "patio: the garden's frame");
    int ft = top_on(0, 600, 96, 0), fr = top_on(0, 600, 300, 0), fj = top_on(0, 1008, 736, 512);
    CHECK(ft >= tab[SF_TRACK][0] - 24, "band: top speed %d", ft);
    CHECK(fr <= tab[SF_ROUGH][0] + 4 && fr < ft * 2 / 3, "off the band: slower (%d)", fr);
    CHECK(fj <= tab[SF_JAM][0] + 4 && fj < fr, "jam: sticky, slower still (%d)", fj);
    /* the puddle: slippery, a turn there keeps sliding */
    world_init(&W, 4, 0, 5, 1, 1);
    int slide[2];
    for (int k = 0; k < 2; k++) {
        solo(&W, 4, 1);
        put(&W, 0, k ? 520 : 300, 736, 0);
        W.c[0].surf = (uint8_t)surface_at(&W, k ? 520 : 300, 736);
        W.c[0].vx = 600;
        in[0].gas = 1;
        in[0].dy = -256;
        steps(&W, 10);
        slide[k] = (int)(((int64_t)-W.c[0].vx * isin(W.c[0].ang) + (int64_t)W.c[0].vy * icos(W.c[0].ang)) >> 14);
    }
    CHECK(slide[1] > slide[0] * 2 + 40, "the puddle: the turn slides much further (%d against %d on the path)", slide[1],
          slide[0]);
    /* a hop over the spoon (tea, the right side, going south) */
    solo(&W, 0, 1);
    put(&W, 0, 1184, 480, 256);
    W.c[0].vy = 650;
    in[0].gas = 1;
    int hopped = 0, landed = 0, maxz = 0;
    for (int k = 0; k < 80; k++) {
        world_step(&W, in);
        hopped |= W.events[0] & EV_HOP;
        landed |= W.events[0] & EV_LAND;
        if (W.c[0].z > maxz) maxz = W.c[0].z;
    }
    CHECK(hopped && landed && maxz > 8 * FX, "the spoon taken at speed: a hop (%d px up), then it lands", maxz / FX);
    CHECK(PX(W.c[0].y) > 576 + 40, "... beyond it (y %d)", PX(W.c[0].y));
    solo(&W, 0, 1);
    put(&W, 0, 1184, 560, 256);
    W.c[0].vy = 100;
    in[0].gas = 0;
    hopped = 0;
    for (int k = 0; k < 40; k++) {
        world_step(&W, in);
        hopped |= W.events[0] & EV_HOP;
    }
    CHECK(!hopped, "slowly: it rolls over the spoon");
    /* off the table: a fall, then put back on the band after FALL_FRAMES */
    solo(&W, 0, 1);
    W.c[0].wp = 20;
    put(&W, 0, 25, 300, 512);
    W.c[0].wp = 20;
    W.c[0].lap = 0;
    W.c[0].vx = -400;
    world_step(&W, in);
    world_step(&W, in);
    CHECK(W.c[0].state == CS_FALL, "over the table's edge: it falls (state %d)", W.c[0].state);
    steps(&W, FALL_FRAMES + 1);
    CHECK(W.c[0].state == CS_RACE && W.c[0].ghost > 0 && surface_at(&W, PX(W.c[0].x), PX(W.c[0].y)) == SF_TRACK,
          "after %d frames it is back on the band, blinking", FALL_FRAMES);
}

static void test_bumps(void)
{
    /* two ladybugs head on, on veg's first straight */
    solo(&W, 3, 2);
    put(&W, 0, 400, 96, 0);
    put(&W, 1, 440, 96, 512);
    W.c[0].vx = 500;
    W.c[1].vx = -500;
    int bumped = 0;
    for (int k = 0; k < 30; k++) {
        world_step(&W, in);
        bumped |= W.events[0] & EV_BUMP;
    }
    CHECK(bumped && W.c[0].vx < 0 && W.c[1].vx > 0, "head on: they bounce off each other (%d, %d)", W.c[0].vx, W.c[1].vx);
    int dx = PX(W.c[1].x - W.c[0].x);
    CHECK(dx >= 2 * CAR_R - 1, "... and do not overlap (%d px)", dx);
    /* into the teapot (tea, centre 512, 320, radius 38) */
    solo(&W, 0, 1);
    put(&W, 0, 400, 320, 0);
    in[0].gas = 1;
    int wall = 0, inside = 0;
    for (int k = 0; k < 120; k++) {
        world_step(&W, in);
        wall |= W.events[0] & EV_WALL;
        int ex = PX(W.c[0].x) - 512, ey = PX(W.c[0].y) - 320;
        if (ex * ex + ey * ey < (38 + CAR_R - 1) * (38 + CAR_R - 1)) inside = 1;
    }
    CHECK(wall && !inside, "into the teapot: a thock, never inside it");
    /* a box: a sugar cube (664, 84) */
    solo(&W, 0, 1);
    put(&W, 0, 620, 84, 0);
    in[0].gas = 1;
    inside = 0;
    for (int k = 0; k < 60; k++) {
        world_step(&W, in);
        if (iabs_v(PX(W.c[0].x) - 664) < 8 + CAR_R - 1 && iabs_v(PX(W.c[0].y) - 84) < 8 + CAR_R - 1) inside = 1;
    }
    CHECK(!inside, "a sugar cube stops it");
    /* the garden's frame (patio) */
    solo(&W, 4, 1);
    put(&W, 0, 60, 300, 512);
    W.c[0].vx = -600;
    steps(&W, 30);
    CHECK(PX(W.c[0].x) >= WALL_IN + CAR_R - 1, "the patio's wall holds (x %d)", PX(W.c[0].x));
}


/* drive car 0 along the centre line by hand, k waypoints a step */
static void walk(world *w, int from, int count, int k)
{
    for (int s = 0; s < count; s++) {
        int i = ((from + s * k) % w->wp.n + w->wp.n) % w->wp.n;
        put(w, 0, w->wp.x[i], w->wp.y[i], w->wp.dir[i]);
        world_step(w, in);
    }
}

static void test_laps(void)
{
    solo(&W, 0, 1);
    W.c[0].wp = (int16_t)(W.wp.n - 3);
    W.c[0].lap = -1;
    walk(&W, W.wp.n - 3, 6, 1);
    CHECK(W.c[0].lap == 0 && W.c[0].prog >= 0, "over the line: lap 0 begins (lap %d, prog %d)", W.c[0].lap, W.c[0].prog);
    int laps = 0;
    for (int s = 0; s < W.wp.n * 2; s++) {
        int i = (3 + s) % W.wp.n;
        put(&W, 0, W.wp.x[i], W.wp.y[i], W.wp.dir[i]);
        world_step(&W, in);
        laps |= W.events[0] & EV_LAP;
    }
    CHECK(W.c[0].lap == 2 && laps, "two laps round: lap %d", W.c[0].lap);
    int32_t p0 = W.c[0].prog;
    int wp0 = W.c[0].wp;
    /* a shortcut: jumping 30 waypoints ahead counts nothing */
    int j = (wp0 + 30) % W.wp.n;
    put(&W, 0, W.wp.x[j], W.wp.y[j], W.wp.dir[j]);
    world_step(&W, in);
    CHECK(W.c[0].wp == wp0 && W.c[0].prog <= p0 + 2 * WP_STEP, "a shortcut 30 waypoints ahead gains nothing (prog %d -> %d)",
          p0, W.c[0].prog);
    /* backwards over the line: the lap goes back */
    solo(&W, 0, 1);
    W.c[0].wp = 2;
    W.c[0].lap = 1;
    for (int s = 0; s < 8; s++) {
        int i = ((2 - s) % W.wp.n + W.wp.n) % W.wp.n;
        put(&W, 0, W.wp.x[i], W.wp.y[i], W.wp.dir[i]);
        world_step(&W, in);
    }
    CHECK(W.c[0].lap == 0, "backwards over the line: the lap is taken back (%d)", W.c[0].lap);
    /* the race ends when the leader completes the last lap */
    solo(&W, 0, 1);
    W.c[0].wp = (int16_t)(W.wp.n - 2);
    W.c[0].lap = LAPS - 1;
    walk(&W, W.wp.n - 2, 5, 1);
    CHECK(W.phase == PH_FINISH, "the leader's last lap: the finish (phase %d)", W.phase);
    CHECK(W.final_place[0] == 1 && W.c[0].points >= 3, "the only one racing wins (place %d, %d points)", W.final_place[0],
          W.c[0].points);
    steps(&W, FINISH_FRAMES);
    CHECK(W.phase == PH_DONE, "then the race is done");
}

static void test_left_behind(void)
{
    /* the zone: fixed, 320 x 240 round the camera (ZONE_OUT px of grace), whatever the screen's width */
    world_init(&W, 0, 0, 5, 1, 3);
    W.camx = 600 * FX;
    W.camy = 400 * FX;
    CHECK(!racer_out_of_zone(&W, (600 + ZONE_HW + ZONE_OUT) * FX, 400 * FX), "the zone's edge + %d px: still in", ZONE_OUT);
    CHECK(racer_out_of_zone(&W, (600 + ZONE_HW + ZONE_OUT + 1) * FX, 400 * FX), "one more pixel: out");
    CHECK(racer_out_of_zone(&W, (600 - ZONE_HW - ZONE_OUT - 1) * FX, 400 * FX), "out on the left too");
    CHECK(racer_out_of_zone(&W, 600 * FX, (400 + ZONE_HH + ZONE_OUT + 1) * FX), "out below");
    CHECK(!racer_out_of_zone(&W, 600 * FX, (400 - ZONE_HH) * FX), "in at the top edge");
    /* a race: the leader (car 0) far ahead of car 1 */
    solo(&W, 0, 2);
    put(&W, 0, W.wp.x[43], W.wp.y[43], W.wp.dir[43]);   /* car 0 at waypoint 43: the leader, the camera on it */
    W.c[0].wp = 43;
    W.c[0].lap = 0;
    W.camx = W.c[0].x;
    W.camy = W.c[0].y;
    put(&W, 1, W.wp.x[2], W.wp.y[2], 0);        /* car 1 600 px behind */
    W.c[1].wp = 2;
    W.c[1].lap = 0;
    W.c[1].points = 2;
    int pts = W.c[0].points;
    for (int k = 0; k < 5; k++) {
        int i = 43;
        put(&W, 0, W.wp.x[i], W.wp.y[i], W.wp.dir[i]);
        world_step(&W, in);
        if (W.c[1].state == CS_OUT) break;
    }
    CHECK(W.leader == 0, "the leader is the one furthest along");
    CHECK(W.c[1].state == CS_OUT && W.c[1].drops == 1, "left behind: out of the zone, gone (state %d)", W.c[1].state);
    CHECK(W.c[0].points == pts + DROP_POINTS, "the leader takes the point (%d -> %d)", pts, W.c[0].points);
    CHECK(W.c[1].points == 2 - DROP_LOSS, "... the one left behind loses one (2 -> %d)", W.c[1].points);
    for (int k = 0; k < DROP_DELAY + 1; k++) {
        put(&W, 0, W.wp.x[43], W.wp.y[43], W.wp.dir[43]);
        world_step(&W, in);
    }
    CHECK(W.c[1].state == CS_RACE && W.c[1].ghost > 0, "after %d frames it is back, blinking", DROP_DELAY);
    CHECK(W.c[1].wp < W.c[0].wp && W.c[0].wp - W.c[1].wp <= DROP_BACK_WP + 2 && !racer_out_of_zone(&W, W.c[1].x, W.c[1].y),
          "... just behind the leader, in the zone (waypoint %d, the leader's %d)", W.c[1].wp, W.c[0].wp);
    CHECK(W.c[1].prog < W.c[0].prog, "... and still behind it in the race");
    CHECK(W.c[1].gift > 500, "... the ground it was carried does not count for its place (%d px)", W.c[1].gift);
    /* no one is left behind in the first seconds */
    world_init(&W, 0, 0, 5, 2, 9);
    W.phase = PH_RACE;
    W.phase_t = 0;
    put(&W, 1, 1100, 700, 0);
    world_step(&W, in);
    CHECK(W.c[1].state == CS_RACE, "no left-behind in the first %d frames", DROP_GRACE);
}

/* a whole race of bots on every track, both ways: they finish, within reason */
static void test_bots(void)
{
    for (int t = 0; t < TRACKS; t++)
        for (int rev = 0; rev < 2; rev++)
            for (int lv = 0; lv < CHAMP_RACES; lv += 9) {
                world_init(&W, t, rev, lv, 0, 11 + t + rev * 5 + lv);
                int frames = 0, stuck = 0;
                while (W.phase != PH_DONE && frames < 60 * 240) {
                    world_step(&W, NULL);
                    frames++;
                }
                for (int i = 0; i < RACERS; i++)
                    if (W.c[i].falls > 3) stuck++;
                CHECK(W.phase == PH_DONE && frames < 60 * 150, "%s (reverse %d, level %d): the bots race %d laps in %d s",
                      NAMES[t], rev, lv, LAPS, frames / 60);
                CHECK(stuck == 0, "%s (reverse %d): no bot keeps falling off", NAMES[t], rev);
                int sum = 0, firsts = 0;
                for (int i = 0; i < RACERS; i++) {
                    sum += W.c[i].points;
                    firsts += W.final_place[i] == 1;
                }
                CHECK(sum >= 6 && firsts >= 1, "%s: points given (%d), a winner", NAMES[t], sum);
            }
    /* the player bot (bot=1) beats the easy bots; a human-like one (botskill=10) can still finish in the top 3 */
    int top3 = 0, wins = 0;
    for (int t = 0; t < TRACKS; t++) {
        world_init(&W, t, 0, 0, 1, 40 + t);
        W.skill = 0;
        int frames = 0;
        while (W.phase != PH_DONE && frames < 60 * 240) {
            bot_think(&W, 0, &in[0]);
            world_step(&W, in);
            frames++;
        }
        wins += W.final_place[0] == 1;
        world_init(&W2, t, 0, 0, 1, 40 + t);
        W2.skill = 10;
        frames = 0;
        while (W2.phase != PH_DONE && frames < 60 * 240) {
            pin p[RACERS];
            memset(p, 0, sizeof p);
            bot_think(&W2, 0, &p[0]);
            world_step(&W2, p);
            frames++;
        }
        top3 += W2.final_place[0] <= 3;
    }
    CHECK(wins >= 4, "the player bot wins the first race on most tracks (%d of 5)", wins);
    CHECK(top3 >= 3, "a beginner-like bot reaches the top 3 at the easiest level (%d of 5)", top3);
    /* determinism: the same race twice */
    world_init(&W, 2, 1, 4, 0, 99);
    world_init(&W2, 2, 1, 4, 0, 99);
    for (int k = 0; k < 3000; k++) {
        world_step(&W, NULL);
        world_step(&W2, NULL);
    }
    CHECK(!memcmp(&W, &W2, sizeof W), "the same race twice: the same world");
}

static void test_points(void)
{
    static const int fp[RACERS] = FINISH_POINTS;
    CHECK(fp[0] > fp[1] && fp[1] > fp[2] && fp[2] >= fp[3], "the finish points go down with the place");
    world_init(&W, 0, 0, 0, 1, 1);
    for (int i = 0; i < RACERS; i++) W.c[i].place = (int16_t)(i + 1);
    W.c[0].points = 2;
    W.c[1].points = 5;
    W.c[2].points = 2;
    W.c[3].points = 0;
    CHECK(race_points_place(&W, 1) == 1 && race_points_place(&W, 0) == 2 && race_points_place(&W, 2) == 3 &&
          race_points_place(&W, 3) == 4, "most points first; the same points: the better place");
    CHECK(champ_qualifies(1) && champ_qualifies(3) && !champ_qualifies(4), "1 player: the top 3 go on, 4th is out");
    int seen[TRACKS][2];
    memset(seen, 0, sizeof seen);
    for (int r = 0; r < CHAMP_RACES; r++) seen[champ_track(r)][champ_reverse(r)]++;
    int all = 1;
    for (int t = 0; t < TRACKS; t++) all &= seen[t][0] == 1 && seen[t][1] == 1;
    CHECK(all && !champ_reverse(0) && champ_reverse(CHAMP_RACES - 1), "the championship: every track, then every track "
          "the other way");
    static const int medals[4] = MEDAL_SCORES;
    CHECK(medals[0] < medals[1] && medals[1] < medals[2] && medals[2] < medals[3], "the medals go up");
}

static void test_touch(void)
{
    static const int widths[4] = {320, 427, 520, 560};
    for (int k = 0; k < 4; k++) {
        int w = widths[k], left = -(w - 320) / 2;
        tbutton b;
        touch_brake_button(left, w, &b);
        CHECK(b.x == left + w - BTN_BRAKE_DX && b.y == RS_SCREEN_H - BTN_BRAKE_DY, "width %d: BRAKE at the bottom right", w);
        CHECK(touch_on_button(&b, b.x + b.r, b.y) && !touch_on_button(&b, b.x - b.r - BTN_SLOP - 1, b.y),
              "width %d: the button's round edge", w);
        CHECK(touch_stick_zone(left, w, left + 10, 120) && !touch_stick_zone(left, w, left + w - 10, 120),
              "width %d: the stick on the left part only", w);
    }
    int16_t dx, dy;
    touch_stick(100, 100, 100 + STICK_DEAD - 2, 100, &dx, &dy);
    CHECK(!dx && !dy, "a tiny drag: no direction (the heading is kept)");
    touch_stick(100, 100, 140, 100, &dx, &dy);
    CHECK(dx == 256 && dy == 0, "dragged right: east (%d, %d)", dx, dy);
    touch_stick(100, 100, 80, 80, &dx, &dy);
    CHECK(dx < -170 && dy < -170 && iatan2(dy, dx) > 600 && iatan2(dy, dx) < 680, "dragged up-left: north-west (%d)",
          iatan2(dy, dx));
}

int main(void)
{
    test_tracks();
    test_handling();
    test_surfaces();
    test_bumps();
    test_laps();
    test_left_behind();
    test_points();
    test_touch();
    test_bots();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails ? 1 : 0;
}
