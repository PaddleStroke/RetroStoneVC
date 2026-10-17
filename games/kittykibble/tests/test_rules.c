/*
 * Kitty Kibble: the rules alone (src/play.c and src/mazes.c, no video, no sound): the five mazes, the start, moving
 * and the turn buffer (taken at the junction, before or just after its centre; forgotten after TURN_BUFFER frames),
 * turning round, walls, the tunnels, kibble and the room clear, the fish treat, each vacuum's target (Dash, Sneak,
 * Patrol, Wobble; scatter), the dock, the yarn (scared vacuums turn round, are caught for 20, 40... points and zip
 * home), one catch ending the run (and with 2 players only that kitten's), the eyes home from everywhere (a busy dock
 * too), meeting head-on (never through each other; the end of yarn time), the room clean as a vacuum arrives, 4 kittens
 * on one kibble, the touch swipe, determinism and the bot.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
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

#define Q(px) ((int32_t)(px) * Q16_ONE)
#define CX(c) Q((c) * CELL + CELL / 2)
static pin in[MAX_PLAYERS];
static world W;

static void idle(void) { memset(in, 0, sizeof in); }
static void step(world *w, int n) { for (int i = 0; i < n; i++) world_step(w, in); }
static int kcx(const kitten *k) { return (int)(k->x / Q(CELL)); }
static int kcy(const kitten *k) { return (int)(k->y / Q(CELL)); }

/* a world in play (the intro over) */
static void playing(world *w, int players, uint32_t seed)
{
    world_init(w, players, seed);
    idle();
    in[0].start = 1;
    world_step(w, in);
    idle();
    while (w->phase != PH_PLAY) world_step(w, in);
}

/* ... with no vacuum out (all charging) and no kibble (no stop to eat; the room does not end) */
static void quiet(world *w, int players)
{
    playing(w, players, 1234);
    for (int i = 0; i < VACUUMS; i++) {
        w->v[i].state = VS_DOCK;
        w->v[i].wait = 1 << 30;
        w->v[i].x = CX(13);
        w->v[i].y = CX(DOCK_IN_Y);
    }
    for (int y = 0; y < MAZE_H; y++)
        for (int x = 0; x < MAZE_W; x++)
            if (w->cell[y][x] == C_KIBBLE || w->cell[y][x] == C_YARN) w->cell[y][x] = C_EMPTY;
    w->kibble_left = 999;
    w->fish_n = 2;
}

static void put(kitten *k, int cx, int cy, int dir)
{
    k->x = CX(cx);
    k->y = CX(cy);
    k->dir = k->face = dir;
    k->moving = 1;
    k->want = DIR_NONE;
    k->want_t = 0;
}

static void test_mazes(void)
{
    for (int m = 0; m < ROOMS; m++) {
        int yarn = 0, starts = 0, fish = 0, dead = 0, asym = 0, kibble = 0;
        for (int y = 0; y < MAZE_H; y++) {
            CHECK((int)strlen(maze_rows[m][y]) == MAZE_W, "maze %d row %d: %d cells", m, y, (int)strlen(maze_rows[m][y]));
            for (int x = 0; x < MAZE_W; x++) {
                char c = maze_rows[m][y][x], o = maze_rows[m][y][MAZE_W - 1 - x];
                if ((c == '#') != (o == '#')) asym++;
                yarn += c == 'o';
                starts += c >= '1' && c <= '4';
                fish += c == 'f';
                kibble += c == '.';
            }
        }
        CHECK(!asym && yarn == 4 && starts == 4 && fish == 1, "maze %d: asym %d yarn %d starts %d fish %d", m, asym,
              yarn, starts, fish);
        CHECK(maze_rows[m][8][DOCK_X] == '-' && maze_rows[m][DOCK_Y][DOCK_X] != '#', "maze %d: the dock's gate", m);
        CHECK(kibble > 200 && kibble < 260, "maze %d: %d kibble", m, kibble);
        /* no dead end, all connected (from start 1) */
        world_init(&W, 1, 1);
        world_start_room(&W, m + 1);
        static int seen[MAZE_H][MAZE_W];
        static int q[MAZE_H * MAZE_W];
        memset(seen, 0, sizeof seen);
        int head = 0, tail = 0, cells = 0, reached = 0;
        q[tail++] = W.start_y[0] * MAZE_W + W.start_x[0];
        seen[W.start_y[0]][W.start_x[0]] = 1;
        while (head < tail) {
            int c = q[head++], x = c % MAZE_W, y = c / MAZE_W;
            reached++;
            for (int d = 0; d < 4; d++)
                if (kitten_open(&W, x, y, d)) {
                    int nx = (x + (d == DIR_RIGHT) - (d == DIR_LEFT) + MAZE_W) % MAZE_W;
                    int ny = y + (d == DIR_DOWN) - (d == DIR_UP);
                    if (!seen[ny][nx]) { seen[ny][nx] = 1; q[tail++] = ny * MAZE_W + nx; }
                }
        }
        for (int y = 0; y < MAZE_H; y++)
            for (int x = 0; x < MAZE_W; x++) {
                int c = cell_at(&W, x, y);
                if (c == C_WALL || c == C_GATE || c == C_DOCK) continue;
                cells++;
                int n = 0;
                for (int d = 0; d < 4; d++) n += kitten_open(&W, x, y, d);
                if (n < 2) dead++;
            }
        CHECK(!dead && reached == cells, "maze %d: %d dead ends, %d of %d cells reached", m, dead, reached, cells);
    }
}

static void test_start(void)
{
    world_init(&W, 1, 7);
    idle();
    step(&W, 30);
    CHECK(W.phase == PH_READY && !W.started && W.t == 0, "the run waits for its first press");
    in[0].start = 1;
    world_step(&W, in);
    idle();
    CHECK(W.phase == PH_INTRO, "the press starts the room's intro");
    int32_t x0 = W.k[0].x;
    step(&W, INTRO_FRAMES - 2);
    CHECK(W.phase == PH_INTRO && W.k[0].x == x0, "nothing moves during the intro");
    step(&W, 2);
    CHECK(W.phase == PH_PLAY, "then it plays");
    CHECK(W.room == 1 && W.maze == 0 && W.kibble_left == W.kibble_total, "room 1, the kitchen, full of kibble");
    int calm = 1;
    for (int f = 0; f < 200; f++) {
        world_step(&W, in);
        for (int i = 0; i < VACUUMS; i++) calm &= W.v[i].scared_t == 0;
    }
    CHECK(calm && world_yarn_time(&W) == 0, "no yarn eaten: the vacuums are never scared (their own colours)");
}

static void test_moving(void)
{
    quiet(&W, 1);
    kitten *k = &W.k[0];
    put(k, 22, 16, DIR_LEFT);
    idle();
    step(&W, 40);
    int32_t want = CX(22) - (int32_t)((int64_t)kitten_speed(&W, k) * 40);
    CHECK(k->x == want && k->y == CX(16), "40 frames left: %d px (1.15 a frame), x %d expected %d",
          (int)((CX(22) - k->x) >> 16), (int)(k->x >> 16), (int)(want >> 16));
    /* a wall: the kitten stops at the centre of the last cell (row 16 ends at column 1) */
    step(&W, 300);
    CHECK(k->x == CX(1) && !k->moving, "it stops at the wall (x %d, moving %d)", (int)(k->x >> 16), k->moving);
    /* turning round at once, anywhere */
    put(k, 20, 16, DIR_LEFT);
    step(&W, 3);
    int32_t x1 = k->x;
    in[0].pressed = in[0].held = DIR_BIT(DIR_RIGHT);
    world_step(&W, in);
    idle();
    CHECK(k->dir == DIR_RIGHT && k->x > x1, "Right turns it round at once");
    /* a corridor's side is no way: Up pressed in the middle of row 16 (no opening) does nothing */
    put(k, 20, 16, DIR_LEFT);
    in[0].held = DIR_BIT(DIR_UP);
    step(&W, 4);
    idle();
    CHECK(k->dir == DIR_LEFT && k->y == CX(16), "no turn into a wall");
}

static void test_turn_buffer(void)
{
    /* row 16 of the kitchen: column 10 has a way up. The kitten runs left 10 px before it; Up is tapped once. */
    quiet(&W, 1);
    kitten *k = &W.k[0];
    put(k, 10, 16, DIR_LEFT);
    k->x += Q(10);
    idle();
    in[0].pressed = in[0].held = DIR_BIT(DIR_UP);
    world_step(&W, in);
    idle();
    step(&W, 20);
    CHECK(k->dir == DIR_UP && k->x == CX(10) && k->y < CX(16), "a turn tapped before a junction is taken there");
    /* tapped too early: forgotten after TURN_BUFFER frames */
    put(k, 10, 16, DIR_LEFT);
    k->x += Q(40);
    in[0].pressed = in[0].held = DIR_BIT(DIR_UP);
    world_step(&W, in);
    idle();
    step(&W, 50);
    CHECK(k->dir == DIR_LEFT && k->y == CX(16), "a turn tapped %d px before is forgotten", 40);
    /* held, it waits for the junction however far */
    put(k, 10, 16, DIR_LEFT);
    k->x += Q(40);
    in[0].held = DIR_BIT(DIR_UP);
    step(&W, 50);
    idle();
    CHECK(k->dir == DIR_UP && k->x == CX(10), "held, the turn waits for the junction");
    /* pressed 2 px after the junction's centre: still taken (snapped back to it) */
    put(k, 10, 16, DIR_LEFT);
    k->x -= Q(2);
    in[0].pressed = in[0].held = DIR_BIT(DIR_UP);
    world_step(&W, in);
    idle();
    CHECK(k->dir == DIR_UP && k->x == CX(10), "a turn just after the junction is still taken (x %d)", (int)(k->x >> 16));
    /* a diagonal held: the turn across the way wins at the next junction */
    put(k, 10, 16, DIR_LEFT);
    k->x += Q(6);
    in[0].held = DIR_BIT(DIR_UP) | DIR_BIT(DIR_LEFT);
    step(&W, 12);
    idle();
    CHECK(k->dir == DIR_UP, "Up-Left held: up at the junction");
}

static void test_tunnel(void)
{
    quiet(&W, 1);
    kitten *k = &W.k[0];
    put(k, 2, 10, DIR_LEFT);                        /* row 10 of the kitchen: the pet doors */
    idle();
    step(&W, 30);
    CHECK(k->dir == DIR_LEFT && kcx(k) >= MAZE_W - 3 && kcy(k) == 10, "the tunnel wraps (x %d)", (int)(k->x >> 16));
    vacuum *v = &W.v[0];
    v->state = VS_ROAM;
    v->x = CX(0);
    v->y = CX(10);
    v->dir = DIR_LEFT;
    v->scared_t = 0;
    int slow = vacuum_speed(&W, v);
    v->x = CX(6);
    CHECK(abs(slow * 100 / vacuum_speed(&W, v) - VAC_TUNNEL) <= 1, "vacuums slow down in the tunnels");
}

static void test_kibble(void)
{
    playing(&W, 1, 99);
    for (int i = 0; i < VACUUMS; i++) { W.v[i].state = VS_DOCK; W.v[i].wait = 1 << 30; }
    kitten *k = &W.k[0];
    int left = W.kibble_left;
    idle();
    step(&W, 60);
    CHECK(k->score > 0 && W.kibble_left == left - k->score && k->eaten == k->score, "kibble: 1 point each (%d)",
          k->score);
    /* the fish treat at 30% */
    CHECK(!W.fish_on, "no fish yet");
    int target = W.kibble_total * 30 / 100;
    for (int y = 0; y < MAZE_H && W.kibble_total - W.kibble_left < target - 1; y++)
        for (int x = 0; x < MAZE_W && W.kibble_total - W.kibble_left < target - 1; x++)
            if (W.cell[y][x] == C_KIBBLE && y != kcy(k)) { W.cell[y][x] = C_EMPTY; W.kibble_left--; }
    step(&W, 40);
    CHECK(W.fish_on && W.fish_t > 0, "the fish treat comes at 30%%");
    int s = k->score;
    put(k, W.fish_x, W.fish_y, DIR_LEFT);
    k->x += Q(3);
    step(&W, 1);
    CHECK(!W.fish_on && k->score >= s + fish_points(1), "eaten: %d points", fish_points(1));
    /* the last kibble: the room is clean, then the next room */
    for (int y = 0; y < MAZE_H; y++)
        for (int x = 0; x < MAZE_W; x++)
            if (W.cell[y][x] == C_KIBBLE || W.cell[y][x] == C_YARN) W.cell[y][x] = C_EMPTY;
    W.cell[16][5] = C_KIBBLE;
    W.kibble_left = 1;
    put(k, 6, 16, DIR_LEFT);
    step(&W, 8);
    CHECK(W.phase == PH_CLEAR && W.kibble_left == 0, "the last kibble eaten: the room is clean");
    step(&W, CLEAR_FRAMES);
    CHECK(W.room == 2 && W.maze == 1 && W.phase == PH_INTRO && W.kibble_left == W.kibble_total && k->state == KS_PLAY,
          "then room 2 (the living room), full again");
    CHECK(kitten_speed(&W, k) > KIT_SPEED && vacuum_speed(&W, &W.v[0]) > VAC_SPEED, "each room a little faster");
}

static void test_targets(void)
{
    quiet(&W, 1);
    kitten *k = &W.k[0];
    put(k, 4, 4, DIR_RIGHT);
    for (int i = 0; i < VACUUMS; i++) {
        W.v[i].state = VS_ROAM;
        W.v[i].x = CX(25);
        W.v[i].y = CX(22);
        W.v[i].dir = DIR_UP;
    }
    int tx, ty;
    W.mode = MODE_SCATTER;
    static const int corners[VACUUMS][2] = HOME_CORNERS;
    for (int i = 0; i < VACUUMS; i++) {
        vacuum_target(&W, i, &tx, &ty);
        CHECK(tx == corners[i][0] && ty == corners[i][1], "scatter: vacuum %d heads for its corner", i);
    }
    W.mode = MODE_CHASE;
    vacuum_target(&W, V_DASH, &tx, &ty);
    CHECK(tx == 4 && ty == 4, "Dash heads straight for the kitten");
    vacuum_target(&W, V_SNEAK, &tx, &ty);
    CHECK(tx == 4 + SNEAK_AHEAD && ty == 4, "Sneak cuts it off, %d cells ahead", SNEAK_AHEAD);
    k->face = DIR_UP;
    vacuum_target(&W, V_SNEAK, &tx, &ty);
    CHECK(tx == 4 && ty == 4 - SNEAK_AHEAD, "... ahead the way it faces");
    vacuum_target(&W, V_PATROL, &tx, &ty);
    CHECK(tx == PATROL_POST_X && ty == PATROL_POST_Y, "Patrol keeps to its post while the kitten is far");
    put(k, 4, 19, DIR_RIGHT);
    vacuum_target(&W, V_PATROL, &tx, &ty);
    CHECK(tx == 4 && ty == 19, "... and chases it near its post");
    vacuum_target(&W, V_WOBBLE, &tx, &ty);
    CHECK(tx == -1000, "Wobble wanders at random when the kitten is far");
    W.v[V_WOBBLE].x = CX(7);
    W.v[V_WOBBLE].y = CX(19);
    vacuum_target(&W, V_WOBBLE, &tx, &ty);
    CHECK(tx == corners[V_WOBBLE][0] && ty == corners[V_WOBBLE][1], "... and backs off to its corner when close");
    /* a vacuum takes the open way (never back) closest to its target: Dash at the junction (13,16) going left,
     * the kitten below-left */
    quiet(&W, 1);
    put(&W.k[0], 7, 22, DIR_LEFT);
    W.mode = MODE_CHASE;
    vacuum *v = &W.v[V_DASH];
    v->state = VS_ROAM;
    v->x = CX(14);
    v->y = CX(16);
    v->dir = DIR_LEFT;
    v->x -= Q(1);
    idle();
    step(&W, 8);
    CHECK(v->dir == DIR_DOWN || v->dir == DIR_LEFT, "Dash takes the way toward the kitten (dir %d)", v->dir);
    int cells = 0, back = 0, d0 = v->dir;
    for (int f = 0; f < 600; f++) {
        world_step(&W, in);
        if (v->dir == ((d0 + 2) & 3) && f < 2) back++;
        d0 = v->dir;
        cells++;
    }
    CHECK(!back, "it never turns round by itself");
}

static void test_dock(void)
{
    playing(&W, 1, 5);
    CHECK(W.v[V_DASH].state == VS_ROAM && W.v[1].state == VS_DOCK && W.v[3].state == VS_DOCK, "Dash starts out, the "
          "others in the dock");
    idle();
    int out[VACUUMS] = {0};
    for (int f = 0; f < 900; f++) {
        for (int i = 0; i < VACUUMS; i++) W.v[i].scared_t = 500;     /* (harmless: the kitten stays in play) */
        world_step(&W, in);
        for (int i = 1; i < VACUUMS; i++)
            if (!out[i] && W.v[i].state == VS_ROAM) out[i] = f + 1;
        if (!world_running(&W)) break;
    }
    CHECK(out[1] && out[2] && out[3] && out[1] < out[2] && out[2] < out[3], "they come out in turn (%d %d %d)", out[1],
          out[2], out[3]);
    CHECK(dock_release(4, 3) < dock_release(1, 3), "sooner in later rooms");
}

static void test_yarn(void)
{
    quiet(&W, 1);
    kitten *k = &W.k[0];
    for (int i = 0; i < VACUUMS; i++) {
        vacuum *v = &W.v[i];
        v->state = VS_ROAM;
        v->x = CX(1 + i * 6);
        v->y = CX(22);
        v->dir = DIR_RIGHT;
    }
    W.v[3].state = VS_DOCK;
    W.cell[16][20] = C_YARN;
    put(k, 21, 16, DIR_LEFT);
    idle();
    int s = k->score;
    step(&W, 8);
    CHECK(k->score == s + YARN_POINTS && W.cell[16][20] == C_EMPTY, "a ball of yarn: %d points", YARN_POINTS);
    CHECK(W.v[0].scared_t > 0 && W.v[3].scared_t > 0 && world_yarn_time(&W) > yarn_frames(1) - 20,
          "the vacuums are scared (in the dock too)");
    CHECK(W.v[0].reverse || W.v[0].dir == DIR_LEFT, "they turn round");
    CHECK(vacuum_speed(&W, &W.v[0]) * 100 / ((int64_t)VAC_SPEED * 100 / 100) <= VAC_SCARED + 1, "and slow down");
    {
        static world c;
        c = W;
        int over = 0;
        for (int f = 0; f < yarn_frames(1) + 200 && !over; f++) {     /* (+ the catches' hit-stops) */
            world_step(&c, in);
            if (c.wevents & WEV_CALM) over = f + 1;
        }
        CHECK(over && world_yarn_time(&c) == 0, "yarn time ends (frame %d): the vacuums calm again", over);
    }
    CHECK(yarn_frames(5) < yarn_frames(1) && yarn_frames(30) == YARN_MIN, "yarn time gets shorter, down to %d",
          YARN_MIN);
    /* catching them: 20, 40, 80, 160 */
    int pts[4] = {0}, prev = k->score;
    for (int i = 0; i < 3; i++) {
        vacuum *v = &W.v[i];
        v->x = k->x + Q(2);
        v->y = k->y;
        v->scared_t = 100;
        world_step(&W, in);
        pts[i] = k->score - prev;
        prev = k->score;
        CHECK(v->state == VS_HOME, "a caught vacuum zips home");
        step(&W, CATCH_FREEZE);
    }
    CHECK(pts[0] == CATCH_POINTS && pts[1] == 2 * CATCH_POINTS && pts[2] == 4 * CATCH_POINTS, "points double: %d %d %d",
          pts[0], pts[1], pts[2]);
    CHECK(k->state == KS_PLAY && k->catches == 3, "the kitten is fine");
    /* home: into the dock, recharged, out again (not scared) */
    int back = 0;
    for (int f = 0; f < 1500 && !back; f++) {
        world_step(&W, in);
        if (W.v[0].state == VS_ROAM) back = f + 1;
    }
    CHECK(back && !W.v[0].scared_t, "it comes out of the dock again, calm (after %d frames)", back);
}

static void test_caught(void)
{
    quiet(&W, 1);
    kitten *k = &W.k[0];
    put(k, 20, 16, DIR_LEFT);
    vacuum *v = &W.v[V_DASH];
    v->state = VS_ROAM;
    v->x = CX(17);
    v->y = CX(16);
    v->dir = DIR_RIGHT;
    v->scared_t = 0;
    idle();
    int f = 0;
    while (k->state == KS_PLAY && f < 60) { world_step(&W, in); f++; }
    CHECK(k->state == KS_CAUGHT && !world_running(&W), "one catch ends the run (frame %d)", f);
    CHECK(W.freeze == HIT_FREEZE, "the hit-stop");
    int32_t vx = v->x;
    step(&W, HIT_FREEZE + 20);
    CHECK(v->x == vx, "the vacuums stop while the kitten spins");
    step(&W, CAUGHT_FRAMES);
    CHECK(k->state == KS_OUT && world_all_out(&W), "then it is out");
    /* 2 players: only that kitten is out, the other plays on */
    quiet(&W, 2);
    put(&W.k[0], 20, 16, DIR_LEFT);
    put(&W.k[1], 4, 22, DIR_RIGHT);
    v = &W.v[V_DASH];
    v->state = VS_ROAM;
    v->x = CX(18);
    v->y = CX(16);
    v->dir = DIR_RIGHT;
    for (f = 0; f < 60 && W.k[0].state == KS_PLAY; f++) world_step(&W, in);
    CHECK(W.k[0].state == KS_CAUGHT && W.k[1].state == KS_PLAY && world_running(&W) && W.msg == MSG_CAUGHT,
          "2 players: P1 caught, P2 plays on");
    int32_t x2 = W.k[1].x;
    step(&W, 30);
    CHECK(W.k[1].x != x2, "nothing stops for long");
}

/* a caught vacuum's eyes reach the dock from every corridor cell, going every way, in every room (they used to circle
 * the living room's top corners for ever: the greedy way to the dock's exit cell runs round a loop) */
static void test_eyes_home(void)
{
    int stuck = 0, worst = 0, fx = -1, fy = -1, fm = -1;
    for (int m = 0; m < ROOMS; m++) {
        quiet(&W, 1);
        world_start_room(&W, m + 1);
        W.phase = PH_PLAY;
        for (int i = 0; i < VACUUMS; i++) { W.v[i].state = VS_DOCK; W.v[i].wait = 1 << 30; }
        W.kibble_left = 999;
        put(&W.k[0], 1, 1, DIR_LEFT);                   /* the kitten out of the way, stopped in a corner */
        W.k[0].moving = 0;
        static world base;
        base = W;
        idle();
        for (int y = 0; y < MAZE_H; y++)
            for (int x = 0; x < MAZE_W; x++)
                for (int d = 0; d < 4; d++) {
                    int c = cell_at(&base, x, y);
                    if (c == C_WALL || c == C_GATE || c == C_DOCK || (x == 1 && y == 1)) continue;
                    W = base;
                    vacuum *v = &W.v[V_DASH];
                    v->state = VS_HOME;
                    v->x = CX(x);
                    v->y = CX(y);
                    v->dir = d;
                    int f = 0;
                    while (v->state != VS_DOCK && f < 1000) { world_step(&W, in); f++; }
                    if (v->state != VS_DOCK) { if (!stuck++) { fx = x; fy = y; fm = m; } }
                    else if (f > worst) worst = f;
                }
    }
    CHECK(!stuck, "the eyes get home from everywhere: %d stuck (the first: room %d, cell %d,%d)", stuck, fm + 1, fx, fy);
    CHECK(worst < 200, "... quickly (at most %d frames)", worst);
    /* home while the dock is busy: Wobble still charges in the slot Dash's eyes go to */
    quiet(&W, 1);
    put(&W.k[0], 1, 1, DIR_LEFT);
    vacuum *v = &W.v[V_DASH];
    v->state = VS_HOME;
    v->x = CX(1);
    v->y = CX(22);
    v->dir = DIR_DOWN;
    int f = 0, docked = 0, wobble = 0;
    while (f < 600 && v->state != VS_ROAM) {
        world_step(&W, in);
        f++;
        if (v->state == VS_DOCK && !docked) { docked = f; W.v[V_WOBBLE].wait = 0; }   /* Wobble leaves meanwhile */
        if (W.v[V_WOBBLE].state == VS_LEAVE) wobble = 1;
    }
    CHECK(docked && v->state == VS_ROAM && !v->scared_t && wobble, "home into a busy dock (frame %d), out again", docked);
}

/* meeting head-on: at the fastest speeds (room 11, the kitten in yarn time), at every sub-pixel phase, in a corridor
 * and through a pet door's wrap, a kitten and a vacuum never pass through each other */
static void test_meeting(void)
{
    int missed = 0, tries = 0;
    for (int room = 1; room <= 11; room += 10)
        for (int scared = 0; scared < 2; scared++)
            for (int lane = 0; lane < 2; lane++)
                for (int ph = 0; ph < 64; ph++) {
                    quiet(&W, 1);
                    world_start_room(&W, room);    /* the kitchen (rooms 1, 11): row 22, or the pet doors' row 10 */
                    W.phase = PH_PLAY;
                    for (int i = 0; i < VACUUMS; i++) { W.v[i].state = VS_DOCK; W.v[i].wait = 1 << 30; }
                    for (int y = 0; y < MAZE_H; y++)
                        for (int x = 0; x < MAZE_W; x++)
                            if (W.cell[y][x] == C_KIBBLE || W.cell[y][x] == C_YARN) W.cell[y][x] = C_EMPTY;
                    W.kibble_left = 999;
                    W.fish_n = 2;
                    int row = lane ? 10 : 22;
                    kitten *k = &W.k[0];
                    put(k, lane ? 1 : 8, row, lane ? DIR_LEFT : DIR_RIGHT);    /* lane 1: out by the left pet door */
                    k->x += ph * (Q16_ONE / 8);
                    vacuum *v = &W.v[V_DASH];
                    v->state = VS_ROAM;
                    v->x = CX(lane ? 22 : 14);                  /* ... the vacuum to the right one */
                    v->y = CX(row);
                    v->dir = lane ? DIR_RIGHT : DIR_LEFT;
                    v->scared_t = scared ? 1000 : 0;
                    W.mode = MODE_CHASE;
                    W.mode_t = -1;
                    W.v[V_SNEAK].scared_t = 1000;               /* yarn time: the kitten at its fastest */
                    idle();
                    int f = 0, met = 0;
                    while (f < 200 && !met) {
                        world_step(&W, in);
                        f++;
                        met = k->state != KS_PLAY || v->state == VS_HOME;
                    }
                    tries++;
                    if (!met) missed++;
                }
    CHECK(!missed, "a kitten and a vacuum meeting head-on always meet (%d of %d passed through)", missed, tries);
    /* yarn time's last frame: still caught by the kitten; the frame it ends, the vacuum catches the kitten */
    for (int last = 0; last < 2; last++) {
        quiet(&W, 1);
        kitten *k = &W.k[0];
        put(k, 20, 16, DIR_LEFT);
        k->pause = 5;                                        /* (it stays put) */
        vacuum *v = &W.v[V_DASH];
        v->state = VS_ROAM;
        v->y = k->y;
        v->dir = DIR_RIGHT;
        v->scared_t = 1000;
        v->x = k->x - Q(CATCH_PX) + 1 - vacuum_speed(&W, v);  /* it comes within reach on the next step ... */
        v->scared_t = last ? 1 : 2;                          /* ... as its fright ends (1) or a frame before (2) */
        idle();
        world_step(&W, in);
        if (last) CHECK(k->state == KS_CAUGHT && v->state == VS_ROAM, "yarn time over: the vacuum catches the kitten");
        else CHECK(k->state == KS_PLAY && v->state == VS_HOME, "yarn time's last frame: the kitten catches it");
    }
}

/* the room clean with a vacuum reaching the kitten in the same frame: the room ends, the kitten plays on; 4 kittens
 * on one kibble: it is eaten once */
static void test_clear_and_shared(void)
{
    quiet(&W, 1);
    kitten *k = &W.k[0];
    W.cell[16][19] = C_KIBBLE;
    W.kibble_left = 1;
    put(k, 20, 16, DIR_LEFT);
    k->x -= Q(CELL / 2 - 1);                                 /* about to enter the cell of the last kibble */
    vacuum *v = &W.v[V_DASH];
    v->state = VS_ROAM;
    v->x = k->x - Q(CATCH_PX + 1);
    v->y = k->y;
    v->dir = DIR_RIGHT;
    idle();
    int f = 0;
    while (W.phase == PH_PLAY && k->state == KS_PLAY && f < 30) { world_step(&W, in); f++; }
    CHECK(W.phase == PH_CLEAR && k->state == KS_PLAY, "the last kibble eaten as a vacuum arrives: the room is clean");
    step(&W, CLEAR_FRAMES + INTRO_FRAMES + 2);
    CHECK(W.room == 2 && W.phase == PH_PLAY && k->state == KS_PLAY, "... and the kitten plays the next room");
    quiet(&W, 4);
    W.cell[16][19] = C_KIBBLE;
    W.kibble_left = 2;                                       /* (one more somewhere: the room goes on) */
    for (int p = 0; p < 4; p++) put(&W.k[p], 20, 16, DIR_LEFT);
    for (f = 0; f < 10; f++) world_step(&W, in);
    int pts = W.k[0].score + W.k[1].score + W.k[2].score + W.k[3].score;
    CHECK(pts == 1 && W.kibble_left == 1 && W.cell[16][19] == C_EMPTY, "4 kittens on one kibble: eaten once (%d)", pts);
}

static void test_touch(void)
{
    world_init(&W, 1, 3);
    CHECK(touch_swipe_dir(&W, 1, 1, 100, 100) == DIR_NONE, "a finger down: nothing yet");
    CHECK(touch_swipe_dir(&W, 1, 0, 105, 101) == DIR_NONE, "a small move: nothing");
    CHECK(touch_swipe_dir(&W, 1, 0, 114, 102) == DIR_RIGHT, "a swipe right: Right");
    CHECK(touch_swipe_dir(&W, 1, 0, 116, 108) == DIR_NONE, "measured again from there");
    CHECK(touch_swipe_dir(&W, 1, 0, 116, 118) == DIR_DOWN, "then down, without lifting: Down");
    CHECK(touch_swipe_dir(&W, 1, 0, 104, 106) == DIR_NONE, "a diagonal: nothing");
    CHECK(touch_swipe_dir(&W, 1, 0, 103, 90) == DIR_UP, "up");
    CHECK(touch_swipe_dir(&W, 0, 0, 0, 0) == DIR_NONE && !W.touch_on, "lifted: forgotten");
    CHECK(touch_swipe_dir(&W, 1, 1, 200, 50) == DIR_NONE && touch_swipe_dir(&W, 1, 0, 186, 52) == DIR_LEFT,
          "a new finger, a swipe left");
    /* a swipe is the d-pad's press: buffered for the next junction */
    quiet(&W, 1);
    kitten *k = &W.k[0];
    put(k, 10, 16, DIR_LEFT);
    k->x += Q(10);
    idle();
    int d = touch_swipe_dir(&W, 1, 1, 160, 120);
    d = touch_swipe_dir(&W, 1, 0, 161, 104);
    in[0].pressed = (uint8_t)DIR_BIT(d);
    world_step(&W, in);
    idle();
    step(&W, 20);
    CHECK(k->dir == DIR_UP && k->y < CX(16), "a swipe up before a junction turns the kitten there");
}

static void test_determinism_and_bot(void)
{
    static world a, b;
    playing(&a, 1, 42);
    playing(&b, 1, 42);
    for (int f = 0; f < 3000; f++) {
        in[0].held = (uint8_t)bot_decide(&a, 0, 4);
        world_step(&a, in);
        in[0].held = (uint8_t)bot_decide(&b, 0, 4);
        world_step(&b, in);
    }
    idle();
    CHECK(!memcmp(&a, &b, sizeof a), "the same seed and inputs: the same run");
    /* the bot clears the kitchen */
    playing(&a, 1, 3);
    int f = 0;
    while (f < 9000 && a.room == 1 && world_running(&a)) {
        in[0].held = (uint8_t)bot_decide(&a, 0, 0);
        world_step(&a, in);
        f++;
    }
    idle();
    CHECK(a.room == 2 && world_running(&a), "the bot clears room 1 (frame %d, score %d)", f, a.k[0].score);
}

int main(void)
{
    test_mazes();
    test_start();
    test_moving();
    test_turn_buffer();
    test_tunnel();
    test_kibble();
    test_targets();
    test_dock();
    test_yarn();
    test_caught();
    test_eyes_home();
    test_meeting();
    test_clear_and_shared();
    test_touch();
    test_determinism_and_bot();
    printf("kittykibble rules: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
