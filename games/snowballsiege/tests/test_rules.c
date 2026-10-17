/*
 * Snowball Siege: the rules alone (play.c), without the runtime: ballistics and the wind, the throwables, carving the
 * terrain, damage and knock-back, falls and the pond, the turns and their timer, charging and walking, switching, the
 * computer's accuracy by level, winning and losing (1 player: the next match, the game over), the frost, the touch
 * slingshot, the camera and determinism.
 *   build/host/snowballsiege_rules            (make snowballsiege-check)
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the SDK's RNG (rs_core.c), here so that play.c links alone */
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

static world W;
static const tin NONE;

static void flat_world(world *w, int ground_y)
{
    world_init(w, 1, 7);
    memset(w->ter, 0, sizeof w->ter);
    w->nfort = 0;
    w->ntrees = 0;
    w->pond_x[0] = w->pond_x[1] = 0;
    for (int y = ground_y; y < WORLD_H; y++)
        for (int x = 0; x < WORLD_W; x++) terrain_set(w, x, y, 1);
    for (int i = 0; i < w->ncrit; i++) w->c[i].y = ground_y << 16;
}

static int steps_until(world *w, const tin *in, int phase, int max)
{
    for (int i = 0; i < max; i++) {
        if (w->phase == phase) return i;
        world_step(w, in);
    }
    return -1;
}

static void test_maths(void)
{
    CHECK(isin(0) == 0 && isin(90) == 65536 && isin(-90) == -65536 && isin(180) == 0, "isin");
    CHECK(icos(0) == 65536 && icos(60) == 32768 && icos(450) == 0, "icos");
    CHECK(isqrt64(0) == 0 && isqrt64(144) == 12 && isqrt64(145) == 12 && isqrt64(1LL << 40) == 1 << 20, "isqrt");
}

/* a throw over a field with nothing in the way: how far it goes */
static int throw_range(int angle, int power, int wind, int kind)
{
    flat_world(&W, 230);
    W.ncrit = 1;
    W.c[0].x = 40 << 16;
    W.c[0].y = 230 << 16;
    impact im;
    proj_sim(&W, &W.c[0], kind, 1, angle, power, wind_accel(wind, kind), &im);
    return im.end == PE_TERRAIN ? im.x - 44 : -1;
}

static void test_ballistics(void)
{
    int r45 = throw_range(45, POWER_MAX, 0, PK_SNOW), r30 = throw_range(30, POWER_MAX, 0, PK_SNOW);
    int r60 = throw_range(60, POWER_MAX, 0, PK_SNOW), half = throw_range(45, POWER_MAX / 2, 0, PK_SNOW);
    /* v^2 / g = 8.2^2 / 0.1 = 672 px on the flat (the throw starts 11 px up: a little further) */
    CHECK(r45 > 640 && r45 < 720, "full power at 45 degrees: %d px (about 672)", r45);
    CHECK(r30 < r45 && r60 < r45 && abs(r30 - r60) < 60, "30 and 60 degrees go less far, about as far: %d %d %d",
          r30, r45, r60);
    CHECK(half > r45 / 5 && half < r45 / 3, "half power: about a quarter of the range: %d", half);
    int tail = throw_range(45, POWER_MAX * 6 / 10, WIND_MAX, PK_SNOW), head = throw_range(45, POWER_MAX * 6 / 10,
                                                                                         -WIND_MAX, PK_SNOW);
    int calm = throw_range(45, POWER_MAX * 6 / 10, 0, PK_SNOW);
    CHECK(tail > calm + 30 && head < calm - 30, "the wind pushes the throw: %d (tail) %d (calm) %d (head)", tail, calm,
          head);
    int btail = throw_range(45, POWER_MAX * 6 / 10, WIND_MAX, PK_BIG);
    CHECK(btail - calm < tail - calm, "a big snowball feels the wind less: +%d against +%d", btail - calm, tail - calm);
    CHECK(wind_accel(-4, PK_SNOW) == -4 * WIND_ACC && wind_accel(10, PK_BIG) == 10 * WIND_ACC * 60 / 100, "wind_accel");
    /* the ice cube bounces before it splats */
    flat_world(&W, 200);
    W.ncrit = 1;
    W.c[0].x = 40 << 16;
    W.c[0].y = 200 << 16;
    proj p;
    proj_launch(&W, &W.c[0], PK_ICE, 1, 40, 700, &p);
    int e = PE_NONE, crit, b, bounces = 0;
    while (e == PE_NONE) { e = proj_frame(&W, &p, 0, &crit, &b); bounces += b; }
    CHECK(bounces == 3 && e == PE_TERRAIN, "an ice cube bounces 3 times then splats: %d bounces, end %d", bounces, e);
    impact snow;
    proj_sim(&W, &W.c[0], PK_SNOW, 1, 40, 700, 0, &snow);
    CHECK(p.x >> 16 > snow.x + 40, "... further than a snowball: %d against %d", p.x >> 16, snow.x);
}

static void test_carving(void)
{
    flat_world(&W, 150);
    uint32_t seq = W.dirty_seq;
    terrain_carve(&W, 300, 160, 11);
    CHECK(!terrain_solid(&W, 300, 160) && !terrain_solid(&W, 310, 160) && !terrain_solid(&W, 300, 170),
          "a splat carves the snow in its radius");
    CHECK(terrain_solid(&W, 313, 160) && terrain_solid(&W, 300, 173) && terrain_solid(&W, 291, 151),
          "... and nothing outside it");
    CHECK(W.dirty_seq == seq + 1 && W.dirty[seq % DIRTY_N].x0 == 289 && W.dirty[seq % DIRTY_N].y1 == 171,
          "... and tells draw.c where");
    terrain_carve(&W, 400, BEDROCK_Y + 2, 18);
    CHECK(!terrain_solid(&W, 400, BEDROCK_Y - 4) && terrain_solid(&W, 400, BEDROCK_Y) &&
          terrain_solid(&W, 400, BEDROCK_Y + 8), "the frozen ground under BEDROCK_Y is never carved");
    CHECK(surface_y(&W, 300) == 172 && surface_y(&W, 500) == 150, "surface_y: %d %d", surface_y(&W, 300),
          surface_y(&W, 500));
    /* a generated field: forts with walls, a pond, the critters standing on the ground */
    world_init(&W, 1, 1234);
    CHECK(W.nfort == 4 && W.ncrit == 4 && W.pond_x[0] == WORLD_W / 2, "a 2-side field: 4 walls, 4 critters, a pond");
    for (int i = 0; i < W.ncrit; i++)
        CHECK(terrain_solid(&W, W.c[i].x >> 16, W.c[i].y >> 16) && !terrain_solid(&W, W.c[i].x >> 16,
              (W.c[i].y >> 16) - 1), "critter %d stands on the ground", i);
    const rect16 *f = &W.fort[1];
    CHECK(fort_at(&W, f->x0, f->y1) && terrain_solid(&W, f->x0, f->y1 - 6), "the fort's wall is snow");
    world_init(&W, 3, 99);
    CHECK(W.nsides == 3 && W.ncrit == 6 && W.pond_x[0] && W.pond_x[1], "3 sides: 6 critters, 2 ponds");
    world_init(&W, 4, 99);
    CHECK(W.nsides == 4 && W.ncrit == 8 && W.nfort == 8, "4 sides: 8 critters, 8 walls");
}

static void test_damage(void)
{
    CHECK(splat_damage(PK_SNOW, 0) == 3 && splat_damage(PK_SNOW, CR_R) == 3 && splat_damage(PK_SNOW, 22) == 0,
          "a snowball: 3 half-hearts at its centre, 0 beyond its reach");
    CHECK(splat_damage(PK_BIG, 0) == 5 && splat_damage(PK_BIG, 20) > 0 && splat_damage(PK_SNOW, 20) == 0,
          "a big snowball: 5 and a wider splat");
    CHECK(splat_damage(PK_SNOW, 12) >= 1 && splat_damage(PK_SNOW, 12) < 3, "damage falls off with the distance");
    flat_world(&W, 180);
    critter *c = &W.c[2];
    c->x = 400 << 16;
    W.p.side = 0;
    W.p.x0 = 120 << 16;
    int score = W.s[0].score;
    splat(&W, PK_SNOW, 392, 174);
    CHECK(c->hp == HP_MAX - 3 + (splat_damage(PK_SNOW, 8) == 3 ? 0 : 1) && c->state == CS_AIR && c->vx > 0 &&
          c->vy < 0, "a splat beside a critter chills it and knocks it away (hp %d, v %d,%d)", c->hp, c->vx, c->vy);
    CHECK(W.s[0].score == score + PTS_DAMAGE * (HP_MAX - c->hp) && W.s[0].damage == HP_MAX - c->hp && c->last_hit == 0,
          "the thrower's side scores the damage");
    CHECK(!terrain_solid(&W, 392, 180), "... and the snow under the splat is gone");
    for (int i = 0; i < 200 && c->state == CS_AIR; i++) world_step(&W, &NONE);
    CHECK(c->state == CS_STAND && (c->x >> 16) > 402, "the critter lands further away (x %d)", c->x >> 16);
}

static void test_falls_and_pond(void)
{
    flat_world(&W, 120);
    critter *c = &W.c[0];
    c->x = 200 << 16;
    for (int y = 120; y < 200; y++)
        for (int x = 190; x < 212; x++) terrain_set(&W, x, y, 0);
    int hp = c->hp;
    for (int i = 0; i < 200; i++) world_step(&W, &NONE);
    CHECK(c->state == CS_STAND && (c->y >> 16) == 200, "with the ground carved away, a critter falls (y %d)",
          c->y >> 16);
    CHECK(c->hp == hp - (1 + (80 - FALL_SAFE) / FALL_PER), "an 80-px fall costs %d half-hearts (hp %d)",
          1 + (80 - FALL_SAFE) / FALL_PER, c->hp);
    c = &W.c[1];
    c->x = 300 << 16;
    for (int y = 120; y < 140; y++)
        for (int x = 290; x < 312; x++) terrain_set(&W, x, y, 0);
    hp = c->hp;
    for (int i = 0; i < 200; i++) world_step(&W, &NONE);
    CHECK(c->hp == hp, "a 20-px fall does nothing");
    /* the pond: a critter that falls in is out; a snowball that lands in it just plops */
    world_init(&W, 1, 42);
    int pc = W.pond_x[0];
    c = &W.c[3];
    c->x = pc << 16;
    c->y = (POND_Y - 30) << 16;
    c->state = CS_AIR;
    for (int i = 0; i < 120 && c->state == CS_AIR; i++) world_step(&W, &NONE);
    CHECK(c->state == CS_SPLASHED && side_alive(&W, 1) == 1, "a critter that falls into the pond is out");
    proj p;
    memset(&p, 0, sizeof p);
    p.on = 1;
    p.crit = -1;
    p.x = pc << 16;
    p.y = (POND_Y - 40) << 16;
    p.vy = 2 << 16;
    int e = PE_NONE, crit, b;
    while (e == PE_NONE) e = proj_frame(&W, &p, 0, &crit, &b);
    CHECK(e == PE_WATER, "a snowball that falls into the pond plops (end %d)", e);
    /* knocked into the pond: the thrower's side scores the splash */
    world_init(&W, 1, 42);
    c = &W.c[2];
    c->x = (pc + POND_HALF + 6) << 16;
    c->y = (POND_Y - 1) << 16;
    W.p.side = 0;
    int score = W.s[0].score;
    splat(&W, PK_BIG, pc + POND_HALF + 18, POND_Y - 6);
    for (int i = 0; i < 200 && c->state == CS_AIR; i++) world_step(&W, &NONE);
    CHECK(c->state == CS_SPLASHED && W.s[0].score >= score + PTS_SPLASH, "knocked into the pond: SPLASH! (+%d)",
          W.s[0].score - score);
}

static void test_turns(void)
{
    world_init(&W, 1, 5);
    world_start(&W);
    CHECK(W.phase == PH_AIM && W.cur_side == 0 && W.cur == 0 && W.timer == TURN_FRAMES && world_human_turn(&W) == 0,
          "match 1: the player's first critter starts, the timer full");
    CHECK(W.wind >= -5 && W.wind <= 5, "the first wind is gentle (%d)", W.wind);
    /* let the timer run out: TIME UP, then the computer's turn */
    int n = steps_until(&W, &NONE, PH_TIMEUP, TURN_FRAMES + 5);
    CHECK(n == TURN_FRAMES, "20 s without a throw: TIME UP (%d frames)", n);
    steps_until(&W, &NONE, PH_AIM, 400);
    CHECK(W.cur_side == 1 && W.s[1].cpu && world_human_turn(&W) == -1 && W.turns == 2,
          "then it is the computer's turn");
    int w0 = W.wind;
    (void)w0;
    steps_until(&W, &NONE, PH_FLIGHT, TURN_FRAMES);
    CHECK(W.phase == PH_FLIGHT, "the computer throws within its turn");
    steps_until(&W, &NONE, PH_AIM, 2000);
    CHECK(W.cur_side == 0 && W.cur == 1 && W.turns == 3, "the player's other critter plays next (cur %d)", W.cur);
    CHECK(W.wind >= -WIND_MAX && W.wind <= WIND_MAX, "the wind stays in -10..10");
    /* charging: A held -> the gauge rises; let go: the throw */
    tin a = NONE;
    a.a_down = 1;
    world_step(&W, &a);                         /* A must be let go first (it may be held from before) */
    CHECK(!W.charging, "A held from before the turn does not charge");
    world_step(&W, &NONE);
    for (int i = 0; i < 10; i++) world_step(&W, &a);
    CHECK(W.charging && W.power == POWER_START + 9 * CHARGE_STEP, "A held 10 frames: power %d", W.power);
    world_step(&W, &NONE);
    CHECK(W.phase == PH_FLIGHT && W.p.on && W.thrown, "let go: the throw flies");
    /* the timer runs out while charging: it throws */
    steps_until(&W, &NONE, PH_AIM, 3000);
    steps_until(&W, &NONE, PH_AIM, 1);
    while (world_human_turn(&W) != 0) world_step(&W, &NONE);
    world_step(&W, &NONE);
    world_step(&W, &a);
    W.timer = 3;
    for (int i = 0; i < 4; i++) world_step(&W, &a);
    CHECK(W.phase == PH_FLIGHT, "the timer runs out while A is held: it throws (phase %d)", W.phase);
    /* 2-4 players: hot seat in the side order */
    world_init(&W, 3, 11);
    world_start(&W);
    int order[6];
    for (int k = 0; k < 6; k++) {
        order[k] = W.cur_side * 10 + W.cur;
        W.timer = 1;
        steps_until(&W, &NONE, PH_TIMEUP, 5);
        steps_until(&W, &NONE, PH_AIM, 400);
    }
    CHECK(order[0] == 0 && order[1] == 12 && order[2] == 24 && order[3] == 1 && order[4] == 13 && order[5] == 25,
          "3 players: P1, P2, P3, then each side's other critter (%d %d %d %d %d %d)", order[0], order[1], order[2],
          order[3], order[4], order[5]);
    CHECK(world_human_turn(&W) == 0, "then P1 again (the human whose turn it is)");
}

static void test_walk_and_switch(void)
{
    flat_world(&W, 160);
    world_start(&W);
    critter *c = world_active(&W);
    int x0 = c->x >> 16;
    tin l = NONE;
    l.walk = -1;
    world_step(&W, &l);
    world_step(&W, &NONE);
    CHECK(c->facing == -1 && (c->x >> 16) == x0, "a tap the other way only turns round");
    tin r = NONE;
    r.walk = 1;
    for (int i = 0; i < 400; i++) world_step(&W, &r);
    CHECK(c->facing == 1 && (c->x >> 16) == x0 + MOVE_BUDGET, "held: it walks %d px at most (moved %d)", MOVE_BUDGET,
          (c->x >> 16) - x0);
    /* a wall in the way stops it; a small step is climbed */
    flat_world(&W, 160);
    world_start(&W);
    c = world_active(&W);
    x0 = c->x >> 16;
    for (int y = 157; y < 160; y++)
        for (int x = x0 + 10; x < x0 + 60; x++) terrain_set(&W, x, y, 1);
    for (int y = 120; y < 157; y++)
        for (int x = x0 + 25; x < x0 + 30; x++) terrain_set(&W, x, y, 1);
    for (int i = 0; i < 300; i++) world_step(&W, &r);
    CHECK((c->y >> 16) == 157 && (c->x >> 16) == x0 + 22, "up a 3-px step, then stopped by a wall (x %d y %d)",
          (c->x >> 16) - x0, c->y >> 16);
    /* B: snowball -> big snowball -> ice cube -> snowball; a used-up throwable is skipped */
    tin b = NONE;
    b.b_press = 1;
    world_step(&W, &b);
    CHECK(W.weapon == PK_BIG, "B: the big snowball");
    world_step(&W, &b);
    CHECK(W.weapon == PK_ICE, "B again: the ice cube");
    world_step(&W, &b);
    CHECK(W.weapon == PK_SNOW, "and back to the snowball");
    W.s[0].stock[PK_BIG] = 0;
    world_step(&W, &b);
    CHECK(W.weapon == PK_ICE, "no big snowball left: skipped");
    tin a = NONE;
    a.a_down = 1;
    world_step(&W, &a);
    world_step(&W, &NONE);
    CHECK(W.p.on && W.p.kind == PK_ICE && W.s[0].stock[PK_ICE] == 1, "throwing one uses it up (left %d)",
          W.s[0].stock[PK_ICE]);
}

/* the computer: how close its throws land to the nearest critter of the other side, by level */
static int cpu_miss(int level, int trials)
{
    int total = 0;
    for (int k = 0; k < trials; k++) {
        world_init(&W, 1, 1000 + k * 77);
        world_start(&W);                        /* the player starts match 1 */
        W.s[1].cpu = (int8_t)level;             /* level 0: no computer side: cpu_think drives it, as the tests' bot */
        W.level = level;
        W.timer = 1;
        steps_until(&W, &NONE, PH_TIMEUP, 5);
        steps_until(&W, &NONE, PH_AIM, 400);    /* the computer's turn */
        int n;
        for (n = 0; n < TURN_FRAMES && W.phase == PH_AIM; n++) {
            tin in = NONE;
            if (!level) cpu_think(&W, 0, &in);
            world_step(&W, &in);
        }
        if (W.phase != PH_FLIGHT) { total += 999; continue; }
        proj p = W.p;
        int e = PE_NONE, crit, b;
        while (e == PE_NONE) e = proj_frame(&W, &p, wind_accel(W.wind, p.kind), &crit, &b);
        int best = 999;
        for (int i = 0; i < W.ncrit; i++) {
            if (W.c[i].side == 1) continue;
            int dx = (W.c[i].x >> 16) - (p.x >> 16), dy = (W.c[i].y >> 16) - 6 - (p.y >> 16);
            int d = isqrt64((int64_t)dx * dx + (int64_t)dy * dy);
            if (d < best) best = d;
        }
        total += e == PE_GONE ? 300 : best;
    }
    return total / trials;
}

static void test_cpu(void)
{
    int m0 = cpu_miss(0, 12), m1 = cpu_miss(1, 24), m4 = cpu_miss(4, 24), m8 = cpu_miss(8, 24);
    printf("  the computer's mean miss: level 0 %d px, 1: %d, 4: %d, 8: %d\n", m0, m1, m4, m8);
    CHECK(m0 <= 12, "with no error it hits (mean miss %d px)", m0);
    CHECK(m1 > m4 && m4 > m8, "it gets better level by level (%d > %d > %d)", m1, m4, m8);
    CHECK(m1 >= 40, "level 1 misses a lot (%d px)", m1);
    CHECK(m8 <= 30, "level 8 is close (%d px)", m8);
    /* it uses the same rules as a player: it aims, then charges, then lets go (a few frames each) */
    world_init(&W, 1, 3);
    world_start(&W);
    W.timer = 1;
    steps_until(&W, &NONE, PH_TIMEUP, 5);
    steps_until(&W, &NONE, PH_AIM, 400);
    int charged = 0, n;
    for (n = 0; n < TURN_FRAMES && W.phase == PH_AIM; n++) {
        world_step(&W, &NONE);
        charged |= W.charging;
    }
    CHECK(charged && n > 30, "the computer charges like a player (%d frames to throw)", n);
}

static void test_win_lose(void)
{
    world_init(&W, 1, 21);
    world_start(&W);
    int score = W.s[0].score;
    W.c[2].hp = 0;
    W.c[3].hp = 0;
    W.timer = 1;
    steps_until(&W, &NONE, PH_TIMEUP, 5);
    steps_until(&W, &NONE, PH_END, 400);
    CHECK(W.phase == PH_END && W.winner == 0 && W.wins == 1 && W.c[2].state == CS_SNOWED,
          "the other side snowed under: the match is won");
    CHECK(W.s[0].score == score + PTS_WIN + PTS_HP_LEFT * 2 * HP_MAX, "the win's bonus: %d",
          W.s[0].score - score);
    uint32_t gen = W.gen;
    steps_until(&W, &NONE, PH_AIM, END_WAIT + 5);
    CHECK(W.level == 2 && W.s[1].cpu == 2 && W.gen != gen && W.cur_side == 1 && !W.over && side_hearts(&W, 0) == 12,
          "1 player: the next match, a new field, a better computer, which starts this time");
    W.c[0].hp = 0;
    W.c[1].hp = 0;
    W.timer = 1;
    steps_until(&W, &NONE, PH_END, 2000);
    CHECK(W.winner == 1 && !W.over, "the player's side snowed under: lost");
    for (int i = 0; i < END_WAIT + 2; i++) world_step(&W, &NONE);
    CHECK(W.over && W.level == 2 && W.wins == 1, "... the game over (1 match won)");
    /* 2-4 players: the ranking (the last side standing first, then the score) */
    world_init(&W, 3, 8);
    world_start(&W);
    W.s[2].score = 500;
    W.c[2].hp = W.c[3].hp = 0;
    W.timer = 1;
    steps_until(&W, &NONE, PH_TIMEUP, 5);
    steps_until(&W, &NONE, PH_AIM, 400);
    CHECK(!side_alive(&W, 1) && W.s[1].out_turn_set && W.phase == PH_AIM, "P2's side is out, the match goes on");
    CHECK(side_rank_key(&W, 0) > side_rank_key(&W, 2) || W.s[0].score > 500 ? 1 : side_rank_key(&W, 2) >
          side_rank_key(&W, 1), "the ranking: alive sides above those out");
    W.c[4].hp = W.c[5].hp = 0;
    W.timer = 1;
    steps_until(&W, &NONE, PH_END, 800);
    for (int i = 0; i < END_WAIT + 2; i++) world_step(&W, &NONE);
    CHECK(W.winner == 0 && W.over && side_rank_key(&W, 0) > side_rank_key(&W, 2) &&
          side_rank_key(&W, 2) > side_rank_key(&W, 1), "P1 wins; P3 out last ranks above P2");
}

static void test_frost(void)
{
    world_init(&W, 2, 4);
    world_start(&W);
    W.turns = FROST_TURN;
    W.c[0].hp = 1;
    W.timer = 1;
    steps_until(&W, &NONE, PH_TIMEUP, 5);
    steps_until(&W, &NONE, PH_AIM, 400);
    CHECK(W.frost && W.c[0].hp == 1 && W.c[1].hp == HP_MAX - 1, "the frost: half a heart each, never the last one");
    W.turns = MAX_TURNS;
    W.c[2].hp = 1;
    W.c[3].hp = 1;
    W.timer = 1;
    steps_until(&W, &NONE, PH_END, 400);
    CHECK(W.phase == PH_END && W.winner == 0, "the 80th turn ends the match: the most hearts win (%d)", W.winner);
}

static void test_touch(void)
{
    int f, a, p;
    /* (dx, dy): from the finger to the critter, screen px (y down): the throw goes that way */
    sling_aim(-40, -40, &f, &a, &p);            /* the finger down-right of the critter: up-left */
    CHECK(f == -1 && a == 45, "pulled down-right: thrown up-left at 45 degrees (%d %d)", f, a);
    sling_aim(40, -40, &f, &a, &p);
    CHECK(f == 1 && a == 45, "pulled down-left: up-right at 45 degrees (%d)", a);
    sling_aim(SLING_MAX, 0, &f, &a, &p);
    CHECK(a == 0 && p == POWER_MAX, "a full pull: full power, flat (%d %d)", a, p);
    sling_aim(SLING_MIN, 0, &f, &a, &p);
    CHECK(p == POWER_START, "a short pull: the gauge's start (%d)", p);
    sling_aim(0, -50, &f, &a, &p);
    CHECK(a == AIM_MAX, "pulled straight down: straight up, clamped (%d)", a);
    sling_aim(30, 40, &f, &a, &p);
    CHECK(a == -30, "pulled up: thrown down, clamped (%d)", a);
    /* the slingshot in the rules: the aim follows while it is pulled, the throw on release */
    flat_world(&W, 160);
    world_start(&W);
    tin s = NONE;
    s.sling = 1;
    s.sl_facing = -1;
    s.sl_angle = 30;
    s.sl_power = 600;
    world_step(&W, &s);
    CHECK(W.sling_on && W.angle == 30 && W.power == 600 && world_active(&W)->facing == -1 && !W.thrown,
          "pulled: the aim and the gauge follow");
    s.sling = 0;
    s.fire = 1;
    world_step(&W, &s);
    CHECK(W.thrown && W.p.vx < 0 && W.p.vy < 0, "let go: thrown up-left");
    flat_world(&W, 160);
    world_start(&W);
    critter *c = world_active(&W);
    int x0 = c->x >> 16;
    tin t = NONE;
    t.shuffle = 1;
    world_step(&W, &t);
    for (int i = 0; i < 60; i++) world_step(&W, &NONE);
    CHECK((c->x >> 16) == x0 + SHUFFLE_PX, "a tap beside the critter shuffles it %d px (%d)", SHUFFLE_PX,
          (c->x >> 16) - x0);
}

static void test_camera(void)
{
    world_init(&W, 1, 5);
    world_start(&W);
    camera cam;
    camera_init(&cam, &W, 320);
    CHECK(cam.x == 160 + 0 || cam.x == (W.c[0].x >> 16), "the camera starts on the active critter (%d)", cam.x);
    for (int i = 0; i < 60; i++) camera_step(&cam, &W, 320, CAM_PAN, i);
    CHECK(cam.x > (W.c[0].x >> 16) + 100, "L/R pans along the field (%d)", cam.x);
    for (int i = 0; i < 400; i++) camera_step(&cam, &W, 560, CAM_PAN, i);
    CHECK(cam.x == WORLD_W - 280, "never past the field's edge (%d)", cam.x);
    tin a = NONE;
    a.aim = 1;
    world_step(&W, &a);
    world_step(&W, &a);
    world_step(&W, &a);
    for (int i = 0; i < 200; i++) camera_step(&cam, &W, 427, 0, i);
    CHECK(cam.x == 427 / 2 || cam.x == (W.c[0].x >> 16), "aiming brings it back to the critter (%d)", cam.x);
}

static uint32_t world_hash(const world *w)
{
    uint32_t h = 2166136261u;
    const uint8_t *p = (const uint8_t *)w->ter;
    for (size_t i = 0; i < sizeof w->ter; i++) h = (h ^ p[i]) * 16777619u;
    for (int i = 0; i < w->ncrit; i++) h = (h ^ (uint32_t)(w->c[i].x + w->c[i].hp)) * 16777619u;
    return h ^ (uint32_t)w->s[0].score;
}

static void test_determinism(void)
{
    static world a, b;
    world_init(&a, 4, 77);
    world_start(&a);
    world_init(&b, 4, 77);
    world_start(&b);
    for (int i = 0; i < 3000; i++) {
        tin ia, ib;
        cpu_think(&a, 0, &ia);
        cpu_think(&b, 0, &ib);
        world_step(&a, &ia);
        world_step(&b, &ib);
    }
    CHECK(world_hash(&a) == world_hash(&b) && a.turns > 6, "4 bot sides, 3000 frames, twice: the same (%d turns)",
          a.turns);
    CHECK(a.dirty_seq > 5, "and the field got carved (%u splats)", a.dirty_seq);
}

/* the bug hunt's regressions (snowballsiege-bugs) */
static void test_regressions(void)
{
    /* a critter snowed under by its own fall (0 hearts) on its own turn: the turn ends, it cannot throw */
    flat_world(&W, 120);
    world_start(&W);
    critter *c = world_active(&W);
    int x0 = c->x >> 16;
    for (int y = 120; y < 200; y++)
        for (int x = x0 + 5; x < x0 + 40; x++) terrain_set(&W, x, y, 0);
    c->hp = 1;
    tin r = NONE;
    r.walk = 1;
    int n;
    for (n = 0; n < 200 && W.phase == PH_AIM && !(c->state == CS_STAND && (c->y >> 16) == 200); n++) world_step(&W, &r);
    tin a = NONE;
    a.a_down = 1;
    for (int i = 0; i < 3; i++) world_step(&W, &NONE);
    for (int i = 0; i < 20; i++) world_step(&W, &a);
    world_step(&W, &NONE);                     /* let go: it would throw */
    CHECK((c->y >> 16) == 200 && c->hp == 0 && !W.thrown && W.phase != PH_AIM && W.phase != PH_FLIGHT,
          "a critter snowed under by its own fall loses its turn (y %d hp %d thrown %d phase %d)", c->y >> 16, c->hp,
          W.thrown, W.phase);
    steps_until(&W, &NONE, PH_AIM, 400);
    CHECK(c->state == CS_SNOWED && W.cur_side == 1, "... it is snowed under, the other side plays (side %d)",
          W.cur_side);
    /* 1 player, match after match: the computer stays the computer (its level is an int8_t: no wrap at 128) */
    world_init(&W, 1, 21);
    world_start(&W);
    W.level = 127;
    W.c[2].hp = W.c[3].hp = 0;
    W.timer = 1;
    steps_until(&W, &NONE, PH_END, 400);
    steps_until(&W, &NONE, PH_AIM, END_WAIT + 5);
    CHECK(W.level == 128 && W.s[1].cpu > 0 && W.cur_side == 1 && world_human_turn(&W) == -1,
          "match 128: the computer's side is still the computer (cpu %d)", W.s[1].cpu);
    W.level = 255;
    W.c[2].hp = W.c[3].hp = 0;
    W.cur_side = 0;
    W.cur = 0;
    W.timer = 1;
    steps_until(&W, &NONE, PH_END, 2000);
    steps_until(&W, &NONE, PH_AIM, END_WAIT + 5);
    CHECK(W.level == 256 && W.s[1].cpu > 0, "match 256: still the computer (cpu %d)", W.s[1].cpu);
    /* 3 players, the 80th turn: the most hearts win, and the results rank that side first (not the best score) */
    world_init(&W, 3, 8);
    world_start(&W);
    W.turns = MAX_TURNS;
    W.c[2].hp = W.c[3].hp = 1;                  /* P2: the best score, few hearts */
    W.c[4].hp = 2;
    W.s[1].score = 900;
    W.timer = 1;
    steps_until(&W, &NONE, PH_END, 400);
    CHECK(W.phase == PH_END && W.winner == 0 && side_rank_key(&W, 0) > side_rank_key(&W, 1) &&
          side_rank_key(&W, 1) > side_rank_key(&W, 2), "the 80th turn: the winner (most hearts) ranks first (%d %d %d)",
          side_rank_key(&W, 0), side_rank_key(&W, 1), side_rank_key(&W, 2));
}

int main(void)
{
    test_maths();
    test_ballistics();
    test_carving();
    test_damage();
    test_falls_and_pond();
    test_turns();
    test_walk_and_switch();
    test_cpu();
    test_win_lose();
    test_frost();
    test_touch();
    test_camera();
    test_determinism();
    test_regressions();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok   the rules", checks, fails);
    return fails ? 1 : 0;
}
