/*
 * Duck Parade: the rules' unit tests (links lanes.c and world.c alone).
 *   - the tuning table against its sources (DESIGN.md "Balance and feel sources");
 *   - the lanes (loops, trains, platforms), the hop, riding, snapping, blocking, the buffer;
 *   - the parade: joining, following the exact path, the knock-off and the gap, banking;
 *   - the camera and the fox;
 *   - the judge against the game (every simple strategy that crosses in the game is seen by the judge);
 *   - the generator's fairness over thousands of seeds (an independent re-judge, a strict judge on a subset,
 *     the train telegraph and the constructive rules).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 *
 *   test_rules [--seeds N] [--strict N] [--lanes N]
 */
#include "dp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int fails, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void section(const char *s) { printf("-- %s\n", s); }

/* ---- helpers: a world with hand-made lanes ------------------------------------------------------------------ */
static world W;

static lane *put(world *w, int32_t col, int kind)
{
    lane *l = &w->L.ring[col % RING];
    memset(l, 0, sizeof *l);
    l->col = col;
    l->kind = (uint8_t)kind;
    return l;
}

static void step(world *w, int press)
{
    int pr[MAX_PLAYERS] = {press, 0};
    world_step(w, pr);
}

static void steps(world *w, int n) { for (int i = 0; i < n; i++) step(w, 0); }

/* a quiet world: meadow grass everywhere up to col 40 (no trees), no ducklings */
static void quiet(world *w, int players)
{
    world_init(w, players, 1234);
    for (int32_t c = 1; c < 40; c++) put(w, c, LK_GRASS);
    memset(w->ls, 0, sizeof w->ls);
    w->L.g.next_col = 40;              /* the generator goes on from here (out of our way) */
    w->L.g.start_col = 39;
}

static int add_loose(world *w, int32_t col, int row)
{
    for (int i = 0; i < LOOSE_MAX; i++)
        if (w->ls[i].state == LOOSE_FREE) {
            memset(&w->ls[i], 0, sizeof w->ls[i]);
            w->ls[i].state = LOOSE_WAIT;
            w->ls[i].col = col;
            w->ls[i].row = (int8_t)row;
            w->ls[i].x = col * CELL << 8;
            w->ls[i].y = row * CELL << 8;
            w->ls[i].id = (int16_t)(100 + i);
            return i;
        }
    return -1;
}

/* ---- tuning ------------------------------------------------------------------------------------------------ */
static void test_tuning(void)
{
    section("tuning against the sources");
    CHECK(HOP_FRAMES == (int)(REF_HOP_S * 60 + 0.5), "hop = the clone's two 0.1-s tweens = 12 frames (%d)", HOP_FRAMES);
    CHECK(HOP_MID == 6, "the lane changes at mid-hop (%d)", HOP_MID);
    CHECK(CELL == 16, "a hop is Frogger's 16-px step");
    CHECK(CAR_V_MIN == 82 && CAR_V_MAX == 328, "cars 0.02..0.08 tiles/frame = 0.32..1.28 px/frame (Q8 %d..%d)", CAR_V_MIN, CAR_V_MAX);
    CHECK(LOG_V_MIN == 82 && LOG_V_MAX == 287, "logs 0.02..0.07 tiles/frame (Q8 %d..%d)", LOG_V_MIN, LOG_V_MAX);
    CHECK(TRAIN_V == 3277, "train 0.8 tiles/frame = 12.8 px/frame (Q8 %d)", TRAIN_V);
    CHECK(LOOP_PX == 22 * 16 && LOOP_PX - FIELD_H == 2 * LOOP_TOP, "the 22-tile loop, 4 hidden cells above and below");
    CHECK(TRAIN_WARN >= TRAIN_WARN_MIN && TRAIN_WARN_MIN >= 60, "trains telegraphed >= 1 s (%d frames)", TRAIN_WARN);
    CHECK(FORGIVE_PX == 1 || FORGIVE_PX == 2, "hit box forgiveness about 0.1 tile");
    CHECK(CAM_EASE_256 * 1000 / 256 >= 29 && CAM_EASE_256 * 1000 / 256 <= 33, "camera easing ~0.03 per frame");
    /* the fox: idling from the anchor column to the edge */
    int from = CAM_ANCHOR_COL * CELL + CELL / 2;
    int t_start = from * 256 / CREEP_START, t_max = from * 256 / CREEP_MAX;
    printf("  fox after idling at the anchor: %.1f s at the start, %.1f s from lane %d (Crossy Road: ~5 s)\n",
           t_start / 60.0, t_max / 60.0, DIFF_LANES);
    CHECK(t_start >= 7 * 60 && t_start <= 9 * 60, "fox at the start after 7..9 s (%d frames)", t_start);
    CHECK(t_max >= 290 && t_max <= 330, "fox later after ~5 s (%d frames; the eagle: 4.9..5.19 s)", t_max);
    CHECK(JUDGE_WINDOW < t_max, "the judge's window (%d) is shorter than the fox's grace (%d)", JUDGE_WINDOW, t_max);
    CHECK(bank_points(10) == 100 && bank_points(1) == 1, "banking n x n");
    CHECK(medal_of(MEDAL_BRONZE) == 1 && medal_of(MEDAL_PEARL) == 4 && medal_of(MEDAL_BRONZE - 1) == 0, "egg medals");
}

/* ---- lanes -------------------------------------------------------------------------------------------------------- */
static void test_lanes(void)
{
    section("lanes: loops, trains, platforms");
    lane l;
    memset(&l, 0, sizeof l);
    l.kind = LK_ROAD;
    l.v = CAR_V_MAX;
    l.down = 1;
    l.n = 1;
    l.m[0].p0 = 0;
    l.m[0].len = 22;
    /* the loop: the car is back where it was after LOOP_PX px */
    uint32_t period = (uint32_t)(LOOP_PX * 256 / CAR_V_MAX);
    int back = 0;
    for (uint32_t t = period - 2; t <= period + 2; t++) if (mover_pos(&l, 0, t) == 0) back = 1;
    CHECK(back, "a car comes back after one loop (%u frames)", period);
    CHECK(mover_pos(&l, 0, 100) == (int)((CAR_V_MAX * 100) >> 8) % LOOP_PX, "down: the position grows");
    l.down = 0;
    CHECK(mover_pos(&l, 0, 100) == modi(-((CAR_V_MAX * 100) >> 8), LOOP_PX), "up: it shrinks");
    /* a hit: the car's box against a duck's */
    l.down = 1;
    l.m[0].p0 = LOOP_TOP + 5 * CELL;      /* row 5 at t = 0 */
    CHECK(lane_hits(&l, 0, 5 * CELL + 1, 6 * CELL - 1), "the car hits a duck in its row");
    CHECK(!lane_hits(&l, 0, 7 * CELL + 1, 8 * CELL - 1), "not two rows further");
    /* trains: warning >= TRAIN_WARN frames before the first hit, the light on while it passes */
    lane r;
    memset(&r, 0, sizeof r);
    r.kind = LK_RAIL;
    r.period = 400;
    r.phase = 0;
    r.cars = 3;
    for (int down = 0; down < 2; down++) {
        r.down = (uint8_t)down;
        int first_warn = -1, first_hit = -1, last_hit = -1, quiet_after = -1, y0, y1;
        for (int t = 0; t < 400; t++) {
            int ph = train_at(&r, (uint32_t)t, &y0, &y1);
            int hit = 0;
            for (int row = 0; row < ROWS; row++) hit |= lane_hits(&r, (uint32_t)t, row * CELL + 1, row * CELL + 15);
            if (ph == 1 && first_warn < 0) first_warn = t;
            if (hit && first_hit < 0) first_hit = t;
            if (hit) last_hit = t;
            if (hit && ph != 2) CHECK(0, "a train hits only while passing (t=%d)", t);
            if (!hit && ph == 0 && last_hit >= 0 && quiet_after < 0) quiet_after = t;
        }
        CHECK(first_warn == 0 && first_hit - first_warn >= TRAIN_WARN_MIN, "train (%s): telegraphed %d frames ahead",
              down ? "down" : "up", first_hit - first_warn);
        CHECK(last_hit > first_hit && quiet_after > last_hit, "... it passes (%d..%d) and goes", first_hit, last_hit);
    }
    /* platforms: the centre on a log, with the tolerance */
    lane v;
    memset(&v, 0, sizeof v);
    v.kind = LK_RIVER;
    v.n = 1;
    v.v = 0;
    v.m[0].p0 = LOOP_TOP + 3 * CELL;
    v.m[0].len = 3 * CELL;
    CHECK(lane_platform(&v, 0, 3 * CELL) == 0 && lane_platform(&v, 0, 6 * CELL - 1) == 0, "on the log");
    CHECK(lane_platform(&v, 0, 6 * CELL + RIDE_TOLERANCE - 1) == 0, "just past its end (tolerance)");
    CHECK(lane_platform(&v, 0, 6 * CELL + RIDE_TOLERANCE + 1) < 0, "in the water");
    CHECK(lane_can_stand(&v, 0, 3 * CELL, DUCK_HIT_H) && !lane_can_stand(&v, 0, 8 * CELL, DUCK_HIT_H), "stand on the log only");
}

/* ---- the hop -------------------------------------------------------------------------------------------------------- */
static void test_hop(void)
{
    section("the hop");
    quiet(&W, 1);
    duck *d = &W.d[0];
    CHECK(d->state == DK_READY && !W.started, "ready before the first hop");
    step(&W, HOP_FWD);
    CHECK(W.started && d->state == DK_ALIVE && d->h.dir == HOP_FWD, "the first hop starts the run");
    int landed_at = -1;
    for (int i = 1; i <= 20 && landed_at < 0; i++) {
        step(&W, 0);
        if (!d->h.dir) landed_at = i;
    }
    CHECK(landed_at == HOP_FRAMES, "a hop lasts %d frames (landed after %d)", HOP_FRAMES, landed_at);
    CHECK(d->at.col == START_COL + 1 && d->at.y == START_ROW * CELL, "one cell forward");
    CHECK(d->max_col == START_COL + 1 && world_score(&W, 0) == 1, "score = lanes crossed (%d)", world_score(&W, 0));
    /* the buffer: a press in the last HOP_BUFFER frames starts the next hop on landing */
    step(&W, HOP_UP);
    steps(&W, HOP_FRAMES - HOP_BUFFER - 1);
    step(&W, HOP_FWD);                  /* h.t = HOP_FRAMES - HOP_BUFFER: buffered */
    steps(&W, HOP_BUFFER - 1);
    CHECK(d->h.dir == HOP_UP, "still in the first hop");
    step(&W, 0);
    CHECK(d->h.dir == HOP_FWD && d->at.y == (START_ROW - 1) * CELL, "the buffered hop starts on landing");
    steps(&W, HOP_FRAMES);
    CHECK(d->at.col == START_COL + 2, "... and lands");
    /* an early press is not buffered */
    step(&W, HOP_FWD);
    steps(&W, 2);
    step(&W, HOP_FWD);
    steps(&W, HOP_FRAMES + 2);
    CHECK(d->at.col == START_COL + 3 && !d->h.dir, "a press early in a hop is ignored (one hop per press)");
    /* a tree blocks */
    lane *g = &W.L.ring[(START_COL + 4) % RING];
    g->block = (uint16_t)(1 << (d->at.y / CELL));
    step(&W, HOP_FWD);
    CHECK(!d->h.dir && d->h.bump == BUMP_FRAMES && (W.events[0] & EV_BUMP), "a tree: a bump, no hop");
    steps(&W, BUMP_FRAMES + 1);
    CHECK(d->at.col == START_COL + 3, "the duck stays");
    /* the top edge blocks */
    while (d->at.y > 0) { step(&W, HOP_UP); steps(&W, HOP_FRAMES); }
    step(&W, HOP_UP);
    CHECK(!d->h.dir && (W.events[0] & EV_BUMP), "the field's top edge blocks");
    /* visuals: arc and squash */
    steps(&W, BUMP_FRAMES + 2);
    step(&W, HOP_DOWN);
    steps(&W, HOP_MID - 1);
    int x, y, lift, sq;
    duck_visual(&W, d, &x, &y, &lift, &sq);
    CHECK(lift >= HOP_HEIGHT - 1 && sq > 16, "mid-hop: up in the air (%d px) and stretched (%d/16)", lift, sq);
    CHECK(y > 0 && y < CELL * 3 / 4 + 1, "75%% of the move at mid-hop (y %d)", y);
}

/* ---- collisions ------------------------------------------------------------------------------------------------------- */
static void test_collisions(void)
{
    section("roads, rivers, ponds");
    /* a car in the lane ahead: hopping into it is a hit */
    quiet(&W, 1);
    duck *d = &W.d[0];
    lane *road = put(&W, START_COL + 1, LK_ROAD);
    road->v = 0;
    road->n = 1;
    road->m[0].len = 22;
    road->m[0].p0 = (int16_t)(LOOP_TOP + START_ROW * CELL - 3);
    step(&W, HOP_FWD);
    int hit_at = -1;
    for (int i = 1; i < 20 && hit_at < 0; i++) { step(&W, 0); if (d->state == DK_HIT) hit_at = i; }
    CHECK(hit_at == HOP_MID, "a parked car: hit when the duck enters its lane at mid-hop (frame %d)", hit_at);
    /* the same car one row away: safe */
    quiet(&W, 1);
    road = put(&W, START_COL + 1, LK_ROAD);
    road->n = 1;
    road->m[0].len = 22;
    road->m[0].p0 = (int16_t)(LOOP_TOP + (START_ROW + 2) * CELL);
    step(&W, HOP_FWD);
    steps(&W, 30);
    CHECK(W.d[0].state == DK_ALIVE && W.d[0].at.col == START_COL + 1, "a car two rows away: safe");
    /* a river: a log carries the duck; landing in the water is a splash */
    quiet(&W, 1);
    d = &W.d[0];
    lane *riv = put(&W, START_COL + 1, LK_RIVER);
    riv->v = Q8(0.5);
    riv->down = 1;
    riv->n = 1;
    riv->m[0].len = 3 * CELL;
    riv->m[0].kind = MV_LOG3;
    /* the log's top will be one row above the duck when it lands (frame HOP_FRAMES) */
    int off12 = (int)(((int64_t)riv->v * HOP_FRAMES) >> 8);
    riv->m[0].p0 = (int16_t)(LOOP_TOP + (START_ROW - 1) * CELL - off12);
    step(&W, HOP_FWD);
    steps(&W, HOP_FRAMES);
    CHECK(d->state == DK_ALIVE && d->at.plat == 0, "lands on the log");
    int y0 = place_field_y(&W, &d->at);
    steps(&W, 60);
    int y1 = place_field_y(&W, &d->at);
    CHECK(y1 - y0 >= 29 && y1 - y0 <= 31, "carried 0.5 px/frame (%d px in 60 frames)", y1 - y0);
    /* hop along the log, then forward to grass: snapped to a row */
    step(&W, HOP_UP);
    steps(&W, HOP_FRAMES);
    CHECK(d->state == DK_ALIVE && d->at.plat == 0, "a hop along the log keeps it");
    step(&W, HOP_FWD);
    steps(&W, HOP_FRAMES);
    CHECK(d->state == DK_ALIVE && d->at.plat < 0 && d->at.y % CELL == 0, "off the log: snapped to row %d", d->at.y / CELL);
    /* carried off the field: swept */
    quiet(&W, 1);
    d = &W.d[0];
    riv = put(&W, START_COL + 1, LK_RIVER);
    riv->v = Q8(1.0);
    riv->down = 1;
    riv->n = 1;
    riv->m[0].len = 4 * CELL;
    riv->m[0].p0 = (int16_t)(LOOP_TOP + (START_ROW - 1) * CELL - (int)(((int64_t)riv->v * HOP_FRAMES) >> 8));
    step(&W, HOP_FWD);
    steps(&W, HOP_FRAMES);
    CHECK(d->state == DK_ALIVE, "on the fast log");
    for (int i = 0; i < 400 && d->state == DK_ALIVE; i++) step(&W, 0);
    CHECK(d->state == DK_SWEPT, "carried past the bottom row: swept away");
    /* into the water */
    quiet(&W, 1);
    riv = put(&W, START_COL + 1, LK_RIVER);
    riv->n = 0;
    step(&W, HOP_FWD);
    steps(&W, HOP_FRAMES);
    CHECK(W.d[0].state == DK_SWEPT, "hopping into the rapids: swept away");
    /* a lily pond: pads only */
    quiet(&W, 1);
    lane *pond = put(&W, START_COL + 1, LK_POND);
    pond->block = (uint16_t)(1 << START_ROW);
    step(&W, HOP_FWD);
    steps(&W, HOP_FRAMES + 2);
    CHECK(W.d[0].state == DK_ALIVE, "on a lily pad");
    step(&W, HOP_UP);
    steps(&W, HOP_FRAMES + 2);
    CHECK(W.d[0].state == DK_SWEPT, "off the pad: in the water");
}

/* ---- the parade --------------------------------------------------------------------------------------------------------- */
static void hop_n(world *w, int dir, int n) { for (int i = 0; i < n; i++) { step(w, dir); steps(w, HOP_FRAMES); } }

static void test_parade(void)
{
    section("the parade");
    quiet(&W, 1);
    duck *d = &W.d[0];
    for (int k = 1; k <= 4; k++) add_loose(&W, START_COL + k, START_ROW);
    hop_n(&W, HOP_FWD, 4);
    steps(&W, 10);
    CHECK(d->nline == 4, "four ducklings picked up on the way (%d)", d->nline);
    /* snake: after more hops each duckling k stands where the duck was k + 1 hops ago */
    hop_n(&W, HOP_UP, 2);
    hop_n(&W, HOP_FWD, 3);
    steps(&W, 10);
    int ok = 1;
    for (int k = 0; k < d->nline; k++) {
        const place *want = &d->hist[d->line[k].lag];
        if (d->line[k].at.col != want->col || d->line[k].at.y != want->y || d->line[k].lag != k + 1) ok = 0;
    }
    CHECK(ok, "each duckling stands where the duck was lag hops ago, lags 1..n");
    /* they are on the exact path: the second duckling is 2 hops behind: one up and one forward from it */
    CHECK(d->line[1].at.col == d->at.col - 2 && d->line[1].at.y == d->at.y, "duckling 2 two cells behind on the path");
    CHECK(d->line[3].at.col == d->at.col - 3 && d->line[3].at.y == d->at.y + CELL, "duckling 4 round the corner");
    /* the knock-off: a car parked on duckling 2's cell (a road there) */
    int32_t kc = d->line[1].at.col;
    int ky = d->line[1].at.y;
    lane *road = put(&W, kc, LK_ROAD);
    road->n = 1;
    road->m[0].len = 22;
    road->m[0].p0 = (int16_t)(LOOP_TOP + ky - 3);
    step(&W, 0);
    CHECK(d->nline == 3 && (W.events[0] & EV_KNOCK), "a car knocks duckling 2 off the line (%d left)", d->nline);
    CHECK(d->line[1].lag == 3 && d->line[2].lag == 4, "the ones behind stay in the line (lags %d, %d)", d->line[1].lag, d->line[2].lag);
    /* the car goes away: the gap closes */
    road->m[0].p0 = (int16_t)(LOOP_TOP + FIELD_H + 20);
    steps(&W, CATCHUP_DELAY + HOP_FRAMES * 4 + 30);
    CHECK(d->line[1].lag == 2 && d->line[2].lag == 3, "the gap closes along the path (lags %d, %d)", d->line[1].lag, d->line[2].lag);
    /* the knocked duckling flew back to a grass cell and waits */
    int waiting = 0;
    for (int i = 0; i < LOOSE_MAX; i++) if (W.ls[i].state == LOOSE_WAIT) waiting++;
    CHECK(waiting == 1, "the knocked duckling waits on the grass (%d)", waiting);
    /* banking: a nest pond ahead */
    put(&W, d->at.col + 1, LK_NEST);
    int before = world_score(&W, 0), n = d->nline;
    hop_n(&W, HOP_FWD, 1);
    CHECK((W.events[0] & EV_BANK) || W.bank_t[0] >= 0, "the nest pond banks");
    CHECK(d->nline == 0 && d->banked == bank_points(n), "banked %d ducklings for %d points", n, d->banked);
    CHECK(world_score(&W, 0) == before + 1 + bank_points(n), "score = lanes + banked (%d)", world_score(&W, 0));
    /* the duck's death scatters the line */
    quiet(&W, 1);
    d = &W.d[0];
    for (int k = 1; k <= 3; k++) add_loose(&W, START_COL + k, START_ROW);
    hop_n(&W, HOP_FWD, 3);
    CHECK(d->nline == 3, "three in the line");
    put(&W, START_COL + 4, LK_RIVER);
    hop_n(&W, HOP_FWD, 1);
    CHECK(d->state == DK_SWEPT && d->nline == 0, "the duck swept away: the line scatters");
}

/* ---- the camera and the fox -------------------------------------------------------------------------------------------- */
static void test_fox(void)
{
    section("the camera and the fox");
    quiet(&W, 1);
    duck *d = &W.d[0];
    step(&W, HOP_FWD);
    int warn_at = -1, caught_at = -1;
    for (int i = 1; i < 2000 && caught_at < 0; i++) {
        step(&W, 0);
        if (warn_at < 0 && d->fox_warn) warn_at = i;
        if (d->state == DK_CAUGHT) caught_at = i;
    }
    printf("  idle from column %d: the fox shows after %.1f s, catches after %.1f s\n", START_COL + 1, warn_at / 60.0, caught_at / 60.0);
    CHECK(warn_at > 0 && caught_at - warn_at >= 120, "warned at least 2 s before the pounce (%d frames)", caught_at - warn_at);
    CHECK(caught_at >= 6 * 60, "not before 6 s of idling at the start (%d)", caught_at);
    /* hopping forward keeps the duck away from the fox */
    quiet(&W, 1);
    d = &W.d[0];
    for (int i = 0; i < 30; i++) { step(&W, HOP_FWD); steps(&W, 30); }
    CHECK(d->state == DK_ALIVE && !d->fox_warn, "a hop a second: no fox");
    int x = d->at.col * CELL - world_cam_px(&W);
    CHECK(x >= (CAM_ANCHOR_COL - 2) * CELL && x <= (CAM_ANCHOR_COL + 2) * CELL, "the camera keeps the duck near column %d (x %d)", CAM_ANCHOR_COL, x);
}

/* ---- the generator's fairness ------------------------------------------------------------------------------------------------ */
static lane ALL[4096];

static int generate_all(uint32_t seed, int lanes_n)
{
    static lanes L;
    lanes_init(&L, seed);
    int32_t last = MEADOW_COLS + lanes_n, copied = 0;
    for (int32_t upto = 40; ; upto += 16) {
        if (upto > last) upto = last;
        lanes_generate(&L, upto, -1);
        for (; copied < L.g.next_col && copied < 4096; copied++) ALL[copied] = *lanes_get(&L, copied);
        if (upto >= last) break;
    }
    return copied;
}

/* ---- the judge against the game ------------------------------------------------------------------------------------------- */
/* In the game, from the start column at frame t0: wait k frames, hop forward (the moment the duck lands, hop
 * again) through the group: does it make it to the goal column? */
static int game_cross(const lane *cols, int n, int row, uint32_t t0, int wait)
{
    static world G;
    world_init(&G, 1, 99);
    memset(G.ls, 0, sizeof G.ls);
    for (int j = 0; j < n + 4; j++) {
        lane *l = &G.L.ring[(START_COL + j) % RING];
        if (j < n) *l = cols[j];
        else { memset(l, 0, sizeof *l); l->kind = LK_GRASS; }
        l->col = START_COL + j;
    }
    G.L.g.next_col = START_COL + n + 4;
    G.L.g.start_col = G.L.g.next_col - 1;
    G.d[0].at.y = (int16_t)(row * CELL);
    G.d[0].hist[0] = G.d[0].at;
    G.started = 1;
    G.d[0].state = DK_ALIVE;
    G.cam = -(1000 << 8);                /* no fox here */
    G.t = t0;
    for (int f = 0; f < wait + (n + 1) * HOP_FRAMES + 2; f++) {
        int pr[2] = {f >= wait && !G.d[0].h.dir ? HOP_FWD : 0, 0};
        world_step(&G, pr);
        G.cam = -(1000 << 8);
        if (G.d[0].state != DK_ALIVE) return 0;
        if (G.d[0].at.col == START_COL + n - 1 && !G.d[0].h.dir) return 1;
    }
    return 0;
}

static void test_judge_vs_game(void)
{
    section("the judge sees every crossing the game allows");
    int game_only = 0, judge_yes = 0, cases = 0;
    for (uint32_t seed = 1; seed <= 60; seed++) {
        int total = generate_all(seed, 60);
        for (int32_t c = MEADOW_COLS; c < total; c++) {
            if (!ALL[c].unit_first || lane_is_safe_kind(&ALL[c])) continue;
            lane cols[16];
            int n = 0;
            cols[n++] = ALL[c - 1];
            int32_t k = c;
            while (k < total && !lane_is_safe_kind(&ALL[k]) && n < 12) cols[n++] = ALL[k++];
            if (k >= total) break;
            cols[n++] = ALL[k];
            uint16_t fr = lane_free_rows(&cols[0]);
            for (int trial = 0; trial < 6; trial++) {
                int row = (int)((seed * 7 + (uint32_t)trial * 5 + (uint32_t)c) % ROWS);
                if (!(fr >> row & 1)) continue;
                uint32_t t0 = seed * 1000 + (uint32_t)trial * 173;
                /* the judge from that single cell, and the game's straight crossings (every wait) */
                int j = judge_cross(cols, n, (uint16_t)(1 << row), t0, 240);
                int g = 0;
                for (int wait = 0; wait < 240 - n * HOP_FRAMES && !g; wait += 3) g = game_cross(cols, n, row, t0, wait);
                cases++;
                if (j >= 0) judge_yes++;
                if (g && j < 0) {
                    game_only++;
                    if (game_only < 4) printf("  seed %u col %d row %d: the game crossed, the judge did not\n", seed, c, row);
                }
            }
        }
    }
    printf("  %d cases: the judge finds a way in %d; straight crossings the judge missed: %d\n", cases, judge_yes, game_only);
    CHECK(cases > 100 && game_only == 0, "every straight crossing of the game is seen by the judge");
}


static void describe(const lane *cols, int k)
{
    static const char *kn[] = {"grass", "road", "river", "rail", "park", "pond", "nest", "hedge"};
    for (int i = 0; i < k; i++) {
        const lane *l = &cols[i];
        printf(" [%s", kn[l->kind]);
        if (l->kind == LK_GRASS || l->kind == LK_POND) printf(" %04x", l->block);
        else if (l->kind == LK_RAIL) printf(" P%d ph%d c%d", l->period, l->phase, l->cars);
        else if (l->kind != LK_NEST) { printf(" v%d %s n%d:", l->v, l->down ? "dn" : "up", l->n); for (int m = 0; m < l->n; m++) printf(" %d/%d", l->m[m].p0, l->m[m].len); }
        printf("]");
    }
    printf("\n");
}

static int connected_run(int32_t a, int32_t b)
{
    uint16_t seen[16] = {0};
    int n = b - a + 1;
    if (n > 16) return 1;
    for (int j = 0; j < n; j++) {
        uint16_t fr = lane_free_rows(&ALL[a + j]);
        if (fr) { for (int r = 0; r < ROWS; r++) if (fr >> r & 1) { seen[j] = (uint16_t)(1 << r); break; } break; }
    }
    for (int changed = 1; changed;) {
        changed = 0;
        for (int j = 0; j < n; j++) {
            uint16_t fr = lane_free_rows(&ALL[a + j]), s = seen[j];
            s |= (uint16_t)((s << 1 | s >> 1) & fr);
            if (j > 0) s |= seen[j - 1] & fr;
            if (j + 1 < n) s |= seen[j + 1] & fr;
            if (s != seen[j]) { seen[j] = s; changed = 1; }
        }
    }
    for (int j = 0; j < n; j++) if (seen[j] != lane_free_rows(&ALL[a + j])) return 0;
    return 1;
}

static void test_fairness(int seeds, int strict_seeds, int lanes_n)
{
    section("the generator's fairness");
    clock_t c0 = clock();
    int groups = 0, rejudge_fail = 0, strict_fail = 0, strict_groups = 0, rule_fail = 0, nests = 0;
    int fam[LK_KINDS] = {0}, total_cols = 0, worst_wait = 0;
    long long sum_cross = 0;
    rs_rng tr;
    rs_rng_seed(&tr, 777);
    for (int s = 1; s <= seeds; s++) {
        int n = generate_all((uint32_t)s, lanes_n);
        int32_t last_nest = -1;
        for (int32_t c = MEADOW_COLS; c < n; c++) {
            const lane *l = &ALL[c];
            fam[l->kind]++;
            total_cols++;
            /* constructive rules */
            if (l->kind == LK_RAIL) {
                int pass = (FIELD_H + train_len(l)) * 256 / TRAIN_V + 2;
                if (l->period < TRAIN_WARN + pass + 60) { rule_fail++; printf("  seed %d col %d: train period %d too short\n", s, c, l->period); }
            }
            if (l->kind == LK_RIVER && c > 0 && ALL[c - 1].kind == LK_RIVER && ALL[c - 1].down == l->down) {
                rule_fail++;
                printf("  seed %d col %d: two river lanes flow the same way\n", s, c);
            }
            if (l->kind == LK_GRASS) {
                int fc = 0;
                for (int r = 0; r < ROWS; r++) fc += lane_free_rows(l) >> r & 1;
                if (fc < GRASS_FREE_MIN) { rule_fail++; printf("  seed %d col %d: only %d free cells\n", s, c, fc); }
            }
            if (l->kind == LK_NEST) {
                if (last_nest >= 0 && c - last_nest < NEST_EVERY_MIN - 6) { rule_fail++; printf("  seed %d: nests %d and %d too close\n", s, last_nest, c); }
                last_nest = c;
                nests++;
            }
            /* grass runs: connected */
            if (lane_is_safe_kind(l) && !lane_is_safe_kind(&ALL[c - 1])) {
                int32_t e = c;
                while (e + 1 < n && lane_is_safe_kind(&ALL[e + 1])) e++;
                if (e + 1 < n && !connected_run(c, e)) { rule_fail++; printf("  seed %d cols %d..%d: grass run not connected\n", s, c, e); }
            }
            /* groups: re-judge with other start times (and a longer window) */
            if (!lane_is_safe_kind(l) && lane_is_safe_kind(&ALL[c - 1])) {
                lane cols[16];
                int k = 0;
                cols[k++] = ALL[c - 1];
                int32_t e = c;
                while (e < n && !lane_is_safe_kind(&ALL[e]) && k < 15) cols[k++] = ALL[e++];
                if (e >= n) break;
                cols[k++] = ALL[e];
                groups++;
                uint16_t fr = lane_free_rows(&cols[0]);
                for (int trial = 0; trial < 4; trial++) {
                    uint32_t t0 = rs_rng_next(&tr) % 100000;
                    int f = judge_cross(cols, k, fr, t0, STRICT_WINDOW);
                    if (f < 0) {
                        rejudge_fail++;
                        if (rejudge_fail < 12) { printf("  seed %d col %d (%d lanes): no crossing from t0=%u:", s, c, k - 2, t0); describe(cols, k); }
                        break;
                    }
                    sum_cross += f;
                    if (f > worst_wait) worst_wait = f;
                }
                /* the strict judge: every STRICT_STEP frames of start time, from single start cells */
                if (s <= strict_seeds) {
                    strict_groups++;
                    int bad = 0;
                    uint32_t base = rs_rng_next(&tr) % 50000;
                    for (int t = 0; t < 240 && !bad; t += STRICT_STEP) {
                        if (judge_cross(cols, k, fr, base + (uint32_t)t, STRICT_WINDOW) < 0) bad = 1;
                    }
                    /* from each single free cell of the last grass column, one start time: the whole grass run
                     * before the group is part of the search (its cells connect through it) */
                    lane big[16];
                    int nb = 0, run0 = c - 1;
                    while (run0 - 1 >= 1 && lane_is_safe_kind(&ALL[run0 - 1]) && c - run0 < 9) run0--;
                    for (int32_t q = run0; q < c && nb < 16; q++) big[nb++] = ALL[q];
                    int sj = nb - 1;
                    for (int q = 1; q < k && nb < 16; q++) big[nb++] = cols[q];
                    for (int r = 0; r < ROWS && !bad; r++)
                        if ((fr >> r & 1) && judge_cross_from(big, nb, sj, (uint16_t)(1 << r), base + (uint32_t)r * 29,
                                                              STRICT_WINDOW + 120) < 0) bad = 2;
                    if (bad) {
                        strict_fail++;
                        if (strict_fail < 6) printf("  seed %d col %d (%d lanes): the strict judge fails (%s)\n", s, c, k - 2, bad == 1 ? "a start time" : "a start cell");
                    }
                }
            }
        }
    }
    double secs = (double)(clock() - c0) / CLOCKS_PER_SEC;
    printf("  %d seeds x %d lanes (%d columns, %d groups, %d nest ponds) in %.1f s\n", seeds, lanes_n, total_cols, groups, nests, secs);
    printf("  lane mix: grass %.0f%% road %.0f%% river %.0f%% rail %.0f%% park %.0f%% pond %.0f%% nest %.1f%%\n",
           100.0 * fam[LK_GRASS] / total_cols, 100.0 * fam[LK_ROAD] / total_cols, 100.0 * fam[LK_RIVER] / total_cols,
           100.0 * fam[LK_RAIL] / total_cols, 100.0 * fam[LK_PARK] / total_cols, 100.0 * fam[LK_POND] / total_cols,
           100.0 * fam[LK_NEST] / total_cols);
    printf("  re-judged from random start times: %d failures; crossing time %.1f frames on average, %d at worst\n",
           rejudge_fail, groups ? (double)sum_cross / (groups * 4) : 0.0, worst_wait);
    printf("  strict judge (%d seeds, %d groups: a start every %d frames, every start cell): %d failures\n",
           strict_seeds, strict_groups, STRICT_STEP, strict_fail);
    CHECK(rule_fail == 0, "constructive rules (%d broken)", rule_fail);
    CHECK(rejudge_fail == 0, "every group can be crossed from any start time tried (%d failed)", rejudge_fail);
    CHECK(strict_fail == 0, "the strict judge (%d failed)", strict_fail);
    CHECK(worst_wait <= STRICT_WINDOW, "worst crossing %d frames", worst_wait);
    CHECK(nests > seeds * lanes_n / (NEST_EVERY_MIN + NEST_EVERY_RAND) / 2, "nest ponds come regularly (%d)", nests);
}

static void test_generator_stats(void)
{
    section("the generator at run time");
    lanes L;
    lanes_init(&L, 42);
    /* staged: one judge start per call, as in the game; the result must equal an unstaged run */
    static lanes L2;
    lanes_init(&L2, 42);
    for (int i = 0; i < 5000 && L.g.next_col < 300; i++) lanes_generate(&L, L.g.next_col + 20, 1);
    lanes_generate(&L2, L.g.next_col - 1, -1);
    int same = 1;
    for (int32_t c = L.g.next_col - 60; c < L.g.next_col; c++) {
        const lane *a = lanes_get(&L, c), *b = lanes_get(&L2, c);
        if (!a || !b || memcmp(a, b, sizeof *a)) same = 0;
    }
    CHECK(same, "staged generation = unstaged generation (determinism)");
    printf("  seed 42 to column %d: %d units, %d judge runs, %d re-rolls, %d groups replaced by grass\n",
           L.g.next_col, L.g.unit, L.g.judged, L.g.rerolls, L.g.fallbacks);
    /* the judge's cost */
    clock_t c0 = clock();
    int runs = 0;
    for (uint32_t s = 1; s <= 20; s++) {
        lanes_init(&L, s);
        lanes_generate(&L, 200, -1);
        runs += L.g.judged;
    }
    double us = (double)(clock() - c0) / CLOCKS_PER_SEC * 1e6 / (runs ? runs : 1);
    printf("  the judge: %.0f us per start on this host (x15-20 on the A20: %.1f-%.1f ms, one start per frame)\n", us, us * 15 / 1000, us * 20 / 1000);
}

int main(int argc, char **argv)
{
    int seeds = 2000, strict = 50, lanes_n = 300;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--seeds")) seeds = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--strict")) strict = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--lanes")) lanes_n = atoi(argv[i + 1]);
    }
    test_tuning();
    test_lanes();
    test_hop();
    test_collisions();
    test_parade();
    test_fox();
    test_judge_vs_game();
    test_generator_stats();
    test_fairness(seeds, strict, lanes_n);
    printf("%s: %d checks, %d failed\n", fails ? "FAILED" : "all passed", checks, fails);
    return fails != 0;
}
