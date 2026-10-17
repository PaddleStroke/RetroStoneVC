/*
 * Cosmo Corgi: the rules alone (src/play.c, no video, no sound, no screen width: this program links play.c with the
 * SDK's RNG only, so the rules cannot ask the width): the start, auto-fire, the d-pad and the touch drag, hits and the
 * shield, the power-ups and their levels, the shots (spread, the piercing beam, homing bones), the flight patterns,
 * the gold groups and their capsule, the walls, the sections and the boss, the widescreen spawn equivalence (things
 * come from beyond the widest screen, the shots' reach is the stage's), determinism and the bot.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE.
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
static int iabs_test(int v) { return v < 0 ? -v : v; }

static pin in[MAX_PLAYERS];
static void idle(void) { memset(in, 0, sizeof in); }
static world W, W2;

/* a run started, no robots or shots around */
static void playing(world *w, int players)
{
    world_init(w, players, 1234);
    idle();
    in[0].any = 1;
    world_step(w, in);
    idle();
}

static void clear_robots(world *w)
{
    memset(w->en, 0, sizeof w->en);
    memset(w->eb, 0, sizeof w->eb);
    memset(w->caps, 0, sizeof w->caps);
    w->waves_done = 0xffffffffu;          /* no more waves this section */
}

static int shots_of(const world *w, int p, int kind)
{
    int n = 0;
    for (int i = 0; i < SHOTS_PER_PLAYER; i++) {
        const shot *s = &w->shots[p * SHOTS_PER_PLAYER + i];
        n += s->on && (kind < 0 || s->kind == kind);
    }
    return n;
}

static ebullet *bullet_at(world *w, int x, int y)
{
    ebullet *b = &w->eb[0];
    memset(b, 0, sizeof *b);
    b->on = 1;
    b->x = (int32_t)x << 16;
    b->y = (int32_t)y << 16;
    return b;
}

static void capsule_at(world *w, int kind, int x, int y)
{
    capsule *c = &w->caps[0];
    memset(c, 0, sizeof *c);
    c->on = 1;
    c->kind = (uint8_t)kind;
    c->x = (int32_t)x << 16;
    c->y = (int32_t)y << 16;
}

static void test_start_and_fire(void)
{
    world_init(&W, 1, 1);
    idle();
    for (int i = 0; i < 30; i++) world_step(&W, in);
    CHECK(!W.started && W.phase == PH_READY && W.h[0].state == SH_READY && W.h[0].shots == 0,
          "the run waits for a press (started %d phase %d shots %d)", W.started, W.phase, W.h[0].shots);
    in[0].any = 1;
    world_step(&W, in);
    CHECK(W.started && W.phase == PH_INTRO && W.h[0].state == SH_PLAY && (W.events[0] & EV_START),
          "a press starts it: the section's intro, the corgi flies");
    idle();
    int x0 = PX(W.h[0].x);
    for (int i = 0; i < FIRE_GAP * 10; i++) world_step(&W, in);
    CHECK(W.h[0].shots == 11, "auto-fire, no button: a volley every %d frames (11 in %d frames: %d)", FIRE_GAP,
          FIRE_GAP * 10 + 1, W.h[0].shots);
    CHECK(PX(W.h[0].x) == x0, "no input: the rocket stays");
    int far = 0;
    for (int i = 0; i < 200; i++) {
        world_step(&W, in);
        for (int k = 0; k < SHOTS_PER_PLAYER; k++)
            if (W.shots[k].on && PX(W.shots[k].x) > SHOT_RANGE) far = 1;
    }
    CHECK(!far, "a shot never goes past x %d (the stage's reach at every width)", SHOT_RANGE);
    CHECK(shots_of(&W, 0, SK_BARK) <= SHOTS_PER_PLAYER, "at most %d shots each", SHOTS_PER_PLAYER);
}

static void test_moves(void)
{
    playing(&W, 1);
    int x0 = PX(W.h[0].x), y0 = PX(W.h[0].y);
    in[0].dx = 1;
    for (int i = 0; i < 40; i++) world_step(&W, in);
    CHECK(PX(W.h[0].x) == x0 + 60, "the d-pad: 1.5 px a frame (x %d -> %d)", x0, PX(W.h[0].x));
    idle();
    in[0].dx = -1;
    in[0].dy = 1;
    for (int i = 0; i < 40; i++) world_step(&W, in);
    CHECK(PX(W.h[0].y) == y0 + 42 && PX(W.h[0].x) == x0 + 17, "8 directions: a diagonal goes 0.71 as fast (%d,%d)",
          PX(W.h[0].x), PX(W.h[0].y));
    idle();
    in[0].dy = -1;
    for (int i = 0; i < 400; i++) world_step(&W, in);
    CHECK(PX(W.h[0].y) == ROCKET_MIN_Y, "it stays on the stage (y %d)", PX(W.h[0].y));
    /* touch: the finger's movement, 1:1, at most TOUCH_MAX a frame, clamped to the stage */
    playing(&W, 1);
    x0 = PX(W.h[0].x);
    y0 = PX(W.h[0].y);
    in[0].touch = 1;
    in[0].tdx = 5;
    in[0].tdy = -3;
    world_step(&W, in);
    CHECK(PX(W.h[0].x) == x0 + 5 && PX(W.h[0].y) == y0 - 3, "a drag moves the rocket by the finger's movement");
    in[0].tdx = 100;
    in[0].tdy = 0;
    world_step(&W, in);
    CHECK(PX(W.h[0].x) == x0 + 5 + TOUCH_MAX, "at most %d px a frame (x %d)", TOUCH_MAX, PX(W.h[0].x));
    in[0].tdx = 0;
    world_step(&W, in);
    CHECK(PX(W.h[0].x) == x0 + 5 + TOUCH_MAX, "a finger held still: the rocket stays");
    for (int i = 0; i < 60; i++) {
        in[0].tdx = 8;
        world_step(&W, in);
    }
    CHECK(PX(W.h[0].x) == ROCKET_MAX_X, "the drag is clamped to the stage (x %d)", PX(W.h[0].x));
    CHECK(W.h[0].shots > 0, "it fires by itself while dragged");
}

static void test_hits_and_shield(void)
{
    playing(&W, 2);
    clear_robots(&W);
    bullet_at(&W, PX(W.h[0].x) + 1, PX(W.h[0].y));
    idle();
    world_step(&W, in);
    CHECK(W.h[0].state == SH_HIT && (W.events[0] & EV_HIT) && W.freeze == HIT_FREEZE,
          "a robot's shot: one hit ends the corgi's run (state %d), a hit-stop", W.h[0].state);
    for (int i = 0; i < HIT_FREEZE + HIT_FRAMES + 2; i++) world_step(&W, in);
    CHECK(W.h[0].state == SH_OUT && W.h[1].state == SH_PLAY && world_running(&W) && !world_all_out(&W),
          "P1 is out, P2 flies on");
    bullet_at(&W, PX(W.h[1].x), PX(W.h[1].y));
    for (int i = 0; i < HIT_FREEZE + HIT_FRAMES + 2; i++) world_step(&W, in);
    CHECK(world_all_out(&W) && !world_running(&W), "the run ends when all are out");
    /* the shield takes one hit */
    playing(&W, 1);
    clear_robots(&W);
    W.h[0].shield = 1;
    bullet_at(&W, PX(W.h[0].x), PX(W.h[0].y));
    idle();
    world_step(&W, in);
    CHECK(W.h[0].state == SH_PLAY && !W.h[0].shield && W.h[0].invul == SHIELD_INVUL && (W.events[0] & EV_SHIELD),
          "the shield takes the hit and pops (state %d shield %d invul %d)", W.h[0].state, W.h[0].shield,
          W.h[0].invul);
    bullet_at(&W, PX(W.h[0].x), PX(W.h[0].y));
    world_step(&W, in);
    CHECK(W.h[0].state == SH_PLAY, "then the rocket blinks, untouchable");
    for (int i = 0; i < SHIELD_INVUL; i++) world_step(&W, in);
    bullet_at(&W, PX(W.h[0].x), PX(W.h[0].y));
    world_step(&W, in);
    CHECK(W.h[0].state == SH_HIT, "and the next hit ends the run");
    /* touching a robot, or a wall, is a hit too */
    playing(&W, 1);
    clear_robots(&W);
    enemy *e = &W.en[0];
    memset(e, 0, sizeof *e);
    e->on = 1;
    e->kind = EK_TANK;
    e->pat = PAT_TANK;
    e->hp = 10;
    e->y0 = (int16_t)PX(W.h[0].y);
    e->x = W.h[0].x + (8 << 16);
    e->y = W.h[0].y;
    e->v = 0;
    world_step(&W, in);
    CHECK(W.h[0].state == SH_HIT, "touching a robot is a hit");
    playing(&W, 1);
    world_start_section(&W, 1, 0);
    clear_robots(&W);
    W.ter_col0 -= 60;                     /* the walls full grown here */
    in[0].dy = 1;
    int hit = 0;
    for (int i = 0; i < 2000 && !hit; i++) {
        world_step(&W, in);
        hit = W.h[0].state == SH_HIT;
    }
    CHECK(hit, "low over the asteroid belt's floor: its rocks are a hit (y %d)", PX(W.h[0].y));
}

static void test_powerups(void)
{
    playing(&W, 1);
    clear_robots(&W);
    idle();
    hero *h = &W.h[0];
    int x = PX(h->x), y = PX(h->y);
    for (int k = 1; k <= 3; k++) {
        capsule_at(&W, CAP_SPREAD, x, y);
        world_step(&W, in);
        CHECK(h->weapon == W_SPREAD && h->wlevel == k && W.msg == MSG_SPREAD, "spread shot level %d (%d)", k, h->wlevel);
    }
    int s0 = h->score;
    capsule_at(&W, CAP_SPREAD, x, y);
    world_step(&W, in);
    CHECK(h->wlevel == WEAPON_MAX && h->score == s0 + MAXED_POINTS && W.msg == MSG_MAX,
          "past the top level: %d points instead", MAXED_POINTS);
    capsule_at(&W, CAP_BEAM, x, y);
    world_step(&W, in);
    CHECK(h->weapon == W_BEAM && h->wlevel == WEAPON_MAX, "another weapon keeps the level");
    for (int k = 0; k < 3; k++) {
        capsule_at(&W, CAP_SPEED, x, y);
        world_step(&W, in);
        capsule_at(&W, CAP_POD, x, y);
        world_step(&W, in);
    }
    CHECK(h->speed == SPEED_MAX && h->pods == PODS_MAX, "speed up to %d, pods up to %d", SPEED_MAX, PODS_MAX);
    capsule_at(&W, CAP_SHIELD, x, y);
    world_step(&W, in);
    CHECK(h->shield == 1, "the shield");
    /* the spread shot: 3 shots a volley at level 1, 5 from level 2 */
    for (int L = 1; L <= 2; L++) {
        playing(&W, 1);
        clear_robots(&W);
        W.h[0].weapon = W_SPREAD;
        W.h[0].wlevel = L;
        memset(W.shots, 0, sizeof W.shots);
        W.h[0].fire_t = 1;
        world_step(&W, in);
        CHECK(shots_of(&W, 0, -1) == (L == 1 ? 3 : 5), "spread level %d: %d shots a volley", L, shots_of(&W, 0, -1));
    }
    /* the pods follow the rocket's path and bark with each volley */
    playing(&W, 1);
    clear_robots(&W);
    W.h[0].pods = 2;
    in[0].dy = 1;
    for (int i = 0; i < 40; i++) world_step(&W, in);
    int px0, py0, px1, py1;
    pod_pos(&W, 0, 0, &px0, &py0);
    pod_pos(&W, 0, 1, &px1, &py1);
    CHECK(px0 == PX(W.h[0].x) && py0 < PX(W.h[0].y) && py1 < py0, "the pods trail along its path (%d %d %d)",
          PX(W.h[0].y), py0, py1);
    idle();
    memset(W.shots, 0, sizeof W.shots);
    W.h[0].fire_t = 1;
    world_step(&W, in);
    CHECK(shots_of(&W, 0, SK_BARK) == 3, "each pod barks too (%d barks)", shots_of(&W, 0, SK_BARK));
    /* the beam pierces: two robots in a row, both hit by one beam */
    playing(&W, 1);
    clear_robots(&W);
    W.h[0].weapon = W_BEAM;
    W.h[0].wlevel = 1;
    for (int k = 0; k < 2; k++) {
        enemy *e = &W.en[k];
        e->on = 1;
        e->kind = EK_TANK;
        e->pat = PAT_TANK;
        e->hp = 10;
        e->y0 = (int16_t)PX(W.h[0].y);
        e->x = W.h[0].x + ((40 + 24 * k) << 16);
        e->y = W.h[0].y;
        e->t = 10;
    }
    memset(W.shots, 0, sizeof W.shots);
    W.h[0].beam_t = 1;
    world_step(&W, in);
    for (int i = 0; i < 9; i++) world_step(&W, in);
    CHECK(W.en[0].hp == 9 && W.en[1].hp == 9, "the star beam goes through robots (hp %d %d)", W.en[0].hp, W.en[1].hp);
    /* homing bones turn toward a robot off their line */
    playing(&W, 1);
    clear_robots(&W);
    W.h[0].weapon = W_HOMING;
    W.h[0].wlevel = 1;
    enemy *e = &W.en[0];
    e->on = 1;
    e->kind = EK_TANK;
    e->pat = PAT_TANK;
    e->hp = 10;
    e->x = W.h[0].x + (150 << 16);
    e->y = W.h[0].y + (60 << 16);
    e->y0 = (int16_t)PX(e->y);
    e->t = 10;
    W.h[0].bone_t = 1;
    int hp0 = e->hp, bones = 0;
    for (int i = 0; i < 90; i++) {
        world_step(&W, in);
        bones += shots_of(&W, 0, SK_BONE) > 0;
    }
    CHECK(bones && e->hp <= hp0 - BONE_DAMAGE, "a homing bone finds a robot 60 px off the line (hp %d -> %d)", hp0,
          e->hp);
}

static void test_patterns(void)
{
    /* the gentle sine waves of the first section: within their amplitude of the lane */
    playing(&W, 1);
    W.h[0].state = SH_OUT;
    int seen = 0, ok = 1, entered_far = 1;
    for (int i = 0; i < 700; i++) {
        world_step(&W, in);
        for (int k = 0; k < MAX_ENEMIES; k++) {
            const enemy *e = &W.en[k];
            if (!e->on || e->pat != PAT_SINE) continue;
            seen = 1;
            if (e->t >= 0 && (PX(e->y) < e->y0 - e->p - 1 || PX(e->y) > e->y0 + e->p + 1)) ok = 0;
            if (e->t < 0 && PX(e->x) < ENTRY_R) entered_far = 0;
        }
    }
    CHECK(seen && ok, "sine waves stay within their amplitude");
    CHECK(entered_far, "before its pattern starts a robot glides in beyond the stage");
    /* the loop: in, round (it goes back right for a while), out on its lane */
    playing(&W, 1);
    W.h[0].state = SH_OUT;
    int min_x = 1 << 30, back = 0, last_x = 1 << 30, lane = 0, looped = 0;
    for (int i = 0; i < 1200; i++) {
        world_step(&W, in);
        const enemy *e = NULL;
        for (int k = 0; k < MAX_ENEMIES; k++)
            if (W.en[k].on && W.en[k].pat == PAT_LOOP) { e = &W.en[k]; break; }
        if (!e) { if (looped) break; continue; }
        looped = 1;
        int x = PX(e->x);
        if (e->t > 0 && x > last_x) back = 1;
        if (x < min_x) min_x = x;
        last_x = x;
        if (e->aux == 2 && iabs_test(PX(e->y) - e->y0) <= 1) lane = 1;
    }
    CHECK(looped && back && lane, "a loop: the drones turn back for a moment and leave on their lane (%d %d %d)", looped,
          back, lane);
}

static void test_groups(void)
{
    /* the first gold group (5 drones looping): pop them all -> a capsule and the bonus */
    playing(&W, 1);
    W.h[0].state = SH_OUT;
    int g = 0;
    for (int i = 0; i < 2000 && !g; i++) {
        world_step(&W, in);
        for (int k = 0; k < MAX_ENEMIES; k++)
            if (W.en[k].on && W.en[k].group && W.en[k].t > 0) g = W.en[k].group;
    }
    CHECK(g, "a gold group comes in the first section");
    int caps0 = 0;
    for (int i = 0; i < MAX_CAPS; i++) caps0 += W.caps[i].on;
    W.h[0].state = SH_PLAY;
    int s0 = W.h[0].score;
    for (int k = 0; k < MAX_ENEMIES; k++)
        if (W.en[k].on && W.en[k].group == g) W.en[k].hp = 1;
    /* shoot each with a shot put on it */
    for (int k = 0; k < MAX_ENEMIES; k++) {
        enemy *e = &W.en[k];
        if (!e->on || e->group != g) continue;
        e->t = 1;                         /* (put on the stage, flying straight: the test aims at it) */
        e->pat = PAT_LINE;
        e->x = (200 + 8 * k) << 16;
        e->y = (40 + 30 * (k % 6)) << 16;
        shot *s = &W.shots[k % SHOTS_PER_PLAYER];
        memset(s, 0, sizeof *s);
        s->on = 1;
        s->kind = SK_BARK;
        s->damage = 1;
        s->x = e->x;
        s->y = e->y;
        world_step(&W, in);
    }
    int caps = 0;
    for (int i = 0; i < MAX_CAPS; i++) caps += W.caps[i].on;
    CHECK(caps == caps0 + 1 && W.h[0].score >= s0 + 5 * 10 + GROUP_BONUS,
          "the gold group all popped: a capsule and %d points (caps %d score %d)", GROUP_BONUS, caps, W.h[0].score - s0);
}

static void test_walls(void)
{
    playing(&W, 1);
    world_start_section(&W, 1, 0);
    W.h[0].state = SH_OUT;
    int gap_ok = 1, walls = 0, turrets = 0, turret_ok = 1;
    for (int i = 0; i < 2400; i++) {
        world_step(&W, in);
        int c0 = (int)(W.scroll >> 19);
        for (int c = c0; c < c0 + 40; c++) {
            int ce = ter_ceil_at(&W, c), fl = ter_floor_at(&W, c);
            walls += ce + fl > 0;
            if (30 - ce - fl < TER_MIN_GAP) gap_ok = 0;
        }
        for (int k = 0; k < MAX_ENEMIES; k++) {
            const enemy *e = &W.en[k];
            if (!e->on || e->pat != PAT_TURRET) continue;
            turrets++;
            int c = (int)((W.scroll + e->x) >> 19), h = e->side ? ter_ceil_at(&W, c) : ter_floor_at(&W, c);
            int y = e->side ? h * 8 + 7 : STAGE_H - h * 8 - 7;
            if (h == 0 || PX(e->y) != y) turret_ok = 0;
        }
    }
    CHECK(walls > 0 && gap_ok, "the asteroid belt has walls, always %d tiles open", TER_MIN_GAP);
    CHECK(turrets > 0 && turret_ok, "its turrets sit on the walls");
}

static void test_boss(void)
{
    playing(&W, 1);
    clear_robots(&W);
    world_to_boss(&W);
    boss *b = &W.b;
    CHECK(b->on && b->phase == BP_ENTER && W.phase == PH_BOSS && b->kind == BK_LADYBIRD, "the nebula's boss comes");
    int hp0 = b->hp;
    W.h[0].y = b->y;                      /* in its line: the shots reach it */
    W.h[0].x = (60 << 16);
    for (int i = 0; i < BOSS_ENTER - 10; i++) world_step(&W, in);
    CHECK(b->hp == hp0, "untouchable while it flies in");
    for (int i = 0; i < 80; i++) {
        W.h[0].y = b->y;
        world_step(&W, in);
    }
    CHECK(b->phase == BP_FIGHT && b->hp < hp0 && W.h[0].score > 0, "then the shots take its strength (%d of %d)",
          b->hp, hp0);
    int s0 = W.h[0].score;
    b->hp = 1;
    for (int i = 0; i < 60 && b->phase == BP_FIGHT; i++) {
        W.h[0].y = b->y;
        world_step(&W, in);
    }
    CHECK(b->phase == BP_DOWN && W.h[0].score >= s0 + 1000, "popped: %d points", W.h[0].score - s0);
    for (int i = 0; i < BOSS_DOWN_FRAMES + 2; i++) world_step(&W, in);
    CHECK(!b->on && W.phase == PH_CLEAR, "then the section is clear");
    for (int i = 0; i < CLEAR_FRAMES + 2; i++) world_step(&W, in);
    CHECK(W.sec == 1 && W.phase == PH_INTRO, "and the asteroid belt begins (section %d phase %d)", W.sec, W.phase);
    /* after a loop of four sections, the next is faster */
    playing(&W, 1);
    world_start_section(&W, SECTIONS - 1, 0);
    clear_robots(&W);
    world_to_boss(&W);
    W.b.hp = 0;
    W.b.phase = BP_DOWN;
    for (int i = 0; i < BOSS_DOWN_FRAMES + CLEAR_FRAMES + 4; i++) world_step(&W, in);
    CHECK(W.sec == 0 && W.loop == 1 && W.scroll_v == SCROLL_SPEED + SCROLL_LOOP, "loop 2: faster (%d %d)", W.sec,
          W.loop);
    /* the boss's strength grows with the players and the loops; it flies away after 90 s */
    playing(&W, 3);
    clear_robots(&W);
    world_to_boss(&W);
    { static const int hp[4] = BOSS_HP; int want = hp[0] * (100 + BOSS_HP_PLAYER * 2) / 100;
      CHECK(W.b.hp0 == want, "3 players: +%d%% each (hp %d, %d)", BOSS_HP_PLAYER, W.b.hp0, want); }
    W.h[0].state = W.h[1].state = W.h[2].state = SH_OUT;
    W.h[0].state = SH_PLAY;
    W.h[0].y = (20 << 16);
    W.h[0].invul = 1 << 20;               /* untouchable: it only waits */
    int pts = W.h[0].score;
    for (int i = 0; i < BOSS_ENTER + BOSS_TIME + 400 && W.b.on; i++) {
        W.h[0].y = (20 << 16);
        W.h[0].x = (300 << 16);
        world_step(&W, in);
    }
    CHECK(!W.b.on && W.boss_away && W.h[0].score - pts < 1000, "after %d s it flies away, no points", BOSS_TIME / 60);
}

/* widescreen: the gameplay is that of the 320 stage. Every robot appears beyond the widest screen (or above / below
 * it), no corgi shot goes past the stage's reach, no robot fires from outside the stage: a wider screen shows them
 * sooner, never earlier, nearer or more of them. */
static void test_widescreen_spawns(void)
{
    world_init(&W, 2, 77);
    idle();
    in[0].any = 1;
    int was_on[MAX_ENEMIES] = {0}, eb_was[MAX_EBULLETS] = {0};
    int spawn_ok = 1, shots_ok = 1, fire_ok = 1, spawned = 0, bad_x = 0, bad_eb = 0;
    for (int f = 0; f < 15000; f++) {
        for (int p = 0; p < 2; p++) {
            pin b = bot_decide(&W, p, 0);
            in[p].dx = b.dx;
            in[p].dy = b.dy;
            in[p].any = 1;
        }
        world_step(&W, in);
        for (int k = 0; k < MAX_ENEMIES; k++) {
            const enemy *e = &W.en[k];
            if (e->on && !was_on[k]) {
                spawned++;
                int x = PX(e->x), y = PX(e->y), from_r = x >= SPAWN_R - 8, from_l = x <= SPAWN_L + 8;
                int above = y <= -12 || y >= STAGE_H + 12;
                if (!(from_r || from_l || above) && !(W.b.on && W.b.kind == BK_CRAB && e->kind == EK_DRONE)) {
                    spawn_ok = 0;
                    bad_x = x;
                }
            }
            was_on[k] = e->on;
        }
        for (int i = 0; i < MAX_PLAYERS * SHOTS_PER_PLAYER; i++)
            if (W.shots[i].on && PX(W.shots[i].x) > SHOT_RANGE) shots_ok = 0;
        for (int i = 0; i < MAX_EBULLETS; i++) {
            const ebullet *b = &W.eb[i];
            if (b->on && !eb_was[i]) {
                int x = PX(b->x - b->vx);
                if (x < 0 || x > STAGE_W) { fire_ok = 0; bad_eb = x; }
            }
            eb_was[i] = b->on;
        }
        if (!world_running(&W)) break;
    }
    CHECK(spawned > 100 && spawn_ok, "%d robots, each appears beyond the widest screen (x %d)", spawned, bad_x);
    CHECK(shots_ok, "the corgis' shots never pass x %d", SHOT_RANGE);
    CHECK(fire_ok, "the robots fire from the stage only (x %d)", bad_eb);
}

/* the ramp: robots per minute of waves, section by section over two loops (an untouchable corgi waits, the bosses
 * fly off at once): gentle at first, rising within each section, from section to section and strongly by loop */
static void test_ramp(void)
{
    static const char *names[4] = {"nebula", "asteroids", "station", "comets"};
    int spawns[8] = {0}, frames[8] = {0}, half[2] = {0}, first20 = 0, was_on[MAX_ENEMIES] = {0};
    world_init(&W, 1, 4321);
    idle();
    in[0].any = 1;
    while (W.loop < 2) {
        W.h[0].invul = 1 << 20;
        if (W.b.on && W.b.phase == BP_FIGHT) W.b.t = BOSS_TIME;
        world_step(&W, in);
        int s = W.loop * 4 + W.sec, waves = W.phase == PH_INTRO || W.phase == PH_WAVES;
        if (W.loop < 2 && waves) frames[s]++;
        for (int k = 0; k < MAX_ENEMIES; k++) {
            if (W.en[k].on && !was_on[k] && W.loop < 2 && waves) {
                spawns[s]++;
                if (s == 0 && W.sec_dist < (20 * 60) << 16) first20++;
                if (s == 0) half[W.sec_dist >= (SECTION_LEN / 2) << 16]++;
            }
            was_on[k] = W.en[k].on;
        }
        if (W.t > 200000) break;
    }
    int per_min[8], rising = 1;
    printf("  robots per minute of waves:");
    for (int s = 0; s < 8; s++) {
        per_min[s] = frames[s] ? spawns[s] * 3600 / frames[s] : 0;
        printf(" %s%s %d", s == 4 ? "| loop 2 " : "", names[s & 3], per_min[s]);
        if (s && s < 4 && per_min[s] <= per_min[s - 1]) rising = 0;         /* loop 1: each section busier */
        if (s >= 4 && per_min[s] * 4 <= per_min[s - 4] * 5) rising = 0;     /* loop 2: each 25% busier than in loop 1 */
    }
    printf(" (the nebula's first 20 s: %d)\n", first20);
    CHECK(W.loop == 2, "two loops played (loop %d)", W.loop + 1);
    CHECK(first20 * 3 <= 80 && first20 * 3 * 2 < per_min[3], "a gentle start: %d robots in the first 20 s", first20);
    CHECK(half[1] > half[0] * 4 / 3, "more robots in the nebula's second half (%d against %d)", half[1], half[0]);
    CHECK(rising && per_min[7] > per_min[4], "more robots a minute from section to section, and each section busier in loop 2");
    CHECK(per_min[4] > per_min[0] * 2 && per_min[7] > per_min[3] * 5 / 4,
          "loop 2 is much busier (the nebula %d against %d, the comets %d against %d)", per_min[4], per_min[0],
          per_min[7], per_min[3]);
}

/* regressions found by the bug hunt */
static int pop_group(world *w, int g)     /* shoot each robot of gold group g with a shot put on it: robots popped */
{
    int n = 0;
    for (int k = 0; k < MAX_ENEMIES; k++) {
        enemy *e = &w->en[k];
        if (!e->on || e->group != g) continue;
        e->t = 1;
        e->pat = PAT_LINE;
        e->hp = 1;
        e->x = (200 + 2 * k) << 16;
        e->y = (40 + 30 * (k % 6)) << 16;
        shot *s = &w->shots[k % SHOTS_PER_PLAYER];
        memset(s, 0, sizeof *s);
        s->on = 1;
        s->kind = SK_BARK;
        s->damage = 1;
        s->x = e->x;
        s->y = e->y;
        world_step(w, in);
        n += !e->on;
    }
    return n;
}

static void test_regressions(void)
{
    /* the scroll (Q16 in an int32) overflowed after 32768 px, in the third loop: the walls were gone for good */
    world_init(&W, 1, 2024);
    idle();
    in[0].any = 1;
    int max_scroll = 0, walled[12] = {0}, turret_ok = 1, turrets = 0;
    while (W.loop < 3 && W.t < 60000) {
        W.h[0].invul = 1 << 20;
        if (W.b.on && W.b.phase == BP_FIGHT) W.b.t = BOSS_TIME;
        world_step(&W, in);
        int s = PX(W.scroll);
        if (s > max_scroll) max_scroll = s;
        if (W.phase == PH_WAVES && W.loop < 3) {
            int c0 = s >> 3;
            for (int c = c0; c < c0 + 40; c++) walled[W.loop * 4 + W.sec] |= ter_ceil_at(&W, c) + ter_floor_at(&W, c) > 0;
        }
        for (int k = 0; k < MAX_ENEMIES; k++) {
            const enemy *e = &W.en[k];
            if (!e->on || e->pat != PAT_TURRET || PX(e->x) < 0 || PX(e->x) >= STAGE_W) continue;
            turrets++;
            int c = (int)((W.scroll + e->x) >> 19), h = e->side ? ter_ceil_at(&W, c) : ter_floor_at(&W, c);
            if (h == 0 || PX(e->y) != (e->side ? h * 8 + 7 : STAGE_H - h * 8 - 7)) turret_ok = 0;
        }
    }
    CHECK(W.loop == 3 && max_scroll < 32767 - 2 * SPAWN_R, "three loops: the scroll stays far from the int32 limit "
          "(at most %d px)", max_scroll);
    CHECK(walled[9] && walled[10] && walled[11], "the walls are still there in loop 3 (%d %d %d)", walled[9], walled[10],
          walled[11]);
    CHECK(turrets && turret_ok, "and the turrets sit on them (%d)", turrets);
    /* the walrus's snowball: its period was 80 or less from loop 3, and t % period never equalled 80 */
    for (int loop = 0; loop < 4; loop++) {
        playing(&W, 1);
        world_start_section(&W, 3, loop);
        clear_robots(&W);
        world_to_boss(&W);
        int big = 0;
        for (int i = 0; i < BOSS_ENTER + 330; i++) {
            W.h[0].invul = 1 << 20;
            world_step(&W, in);
            for (int k = 0; k < MAX_EBULLETS; k++) big += W.eb[k].on && W.eb[k].kind == EB_BIG;
        }
        CHECK(big, "loop %d: the walrus throws its big snowball", loop + 1);
    }
    /* the boss's strength is for the corgis still flying: one survivor of four does not face four corgis' boss */
    {
        static const int hp[4] = BOSS_HP;
        playing(&W, 4);
        clear_robots(&W);
        world_to_boss(&W);
        CHECK(W.b.hp0 == hp[0] * (100 + BOSS_HP_PLAYER * 3) / 100, "4 corgis flying: +%d%% each (%d)", BOSS_HP_PLAYER,
              W.b.hp0);
        playing(&W, 4);
        clear_robots(&W);
        W.h[1].state = W.h[2].state = W.h[3].state = SH_OUT;
        world_to_boss(&W);
        CHECK(W.b.hp0 == hp[0], "1 of 4 still flying: the 1-corgi strength (%d)", W.b.hp0);
    }
    /* a gold group sent when the robots' pool is nearly full: the robots that came, all popped, give the bonus */
    playing(&W, 1);
    clear_robots(&W);
    W.waves_done = 0xffffffffu & ~(1u << 2);   /* the nebula's first gold group (5 drones looping) */
    W.sec_dist = (600 << 16) - 1;
    for (int k = 0; k < MAX_ENEMIES - 2; k++) {
        enemy *e = &W.en[k];
        e->on = 1;
        e->kind = EK_BUG;
        e->pat = PAT_LINE;
        e->t = -30000;                        /* (parked far away, gliding: out of play) */
        e->x = 600 << 16;
    }
    world_step(&W, in);
    int g = W.en[MAX_ENEMIES - 1].on ? W.en[MAX_ENEMIES - 1].group : 0;
    CHECK(g && W.group_left[g - 1] == 2, "a full pool: the group counts the 2 robots that came (%d)",
          g ? W.group_left[g - 1] : -9);
    int s0 = W.h[0].score, caps0 = 0, caps = 0;
    for (int i = 0; i < MAX_CAPS; i++) caps0 += W.caps[i].on;
    int popped = g ? pop_group(&W, g) : 0;
    for (int i = 0; i < MAX_CAPS; i++) caps += W.caps[i].on;
    CHECK(popped == 2 && caps == caps0 + 1 && W.h[0].score >= s0 + 2 * 10 + GROUP_BONUS,
          "both popped: the gold bonus and its capsule (popped %d caps %d score +%d)", popped, caps - caps0,
          W.h[0].score - s0);
}

static void bot_run(world *w, int players, uint32_t seed, int frames, int skill)
{
    world_init(w, players, seed);
    for (int f = 0; f < frames; f++) {
        pin pi[MAX_PLAYERS];
        memset(pi, 0, sizeof pi);
        for (int p = 0; p < players; p++) pi[p] = bot_decide(w, p, skill);
        world_step(w, pi);
    }
}

static void test_determinism_and_bot(void)
{
    bot_run(&W, 2, 99, 3000, 0);
    bot_run(&W2, 2, 99, 3000, 0);
    CHECK(!memcmp(&W, &W2, sizeof W), "the same inputs, the same world, byte for byte");
    bot_run(&W, 1, 5, 7000, 0);
    CHECK(W.h[0].state == SH_PLAY && (W.sec >= 1 || W.phase == PH_CLEAR) && W.h[0].score > 2000,
          "the bot clears the first section and pops its boss (section %d score %d)", W.sec, W.h[0].score);
}


int main(void)
{
    printf("cosmocorgi rules\n");
    test_start_and_fire();
    test_moves();
    test_hits_and_shield();
    test_powerups();
    test_patterns();
    test_groups();
    test_walls();
    test_boss();
    test_widescreen_spawns();
    test_ramp();
    test_regressions();
    test_determinism_and_bot();
    if (fails) printf("  %d of %d checks FAILED\n", fails, checks);
    else printf("  ok   %d checks: auto-fire, moves and the touch drag, hits and the shield, power-up levels, the "
                "beam and the bones, flight patterns, gold groups, walls and turrets, the boss and the loops, "
                "widescreen spawns, determinism and the bot\n", checks);
    return fails ? 1 : 0;
}
