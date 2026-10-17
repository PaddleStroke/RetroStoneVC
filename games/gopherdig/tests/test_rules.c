/*
 * Gopher Dig: the rules alone (src/play.c, no video, no sound, no screen width: this program links play.c with the
 * SDK's RNG only): drilling (a whole colour group pops), colour groups and their support (another colour holds a
 * block up, a stuck neighbour holds an overhang, the gopher holds nothing), the shake then the fall, chain pops (4 after
 * a fall, not 3), crushing (and stepping aside in time), the air (drain, bubbles, hard rocks' cost, out of air), hard
 * rocks (5 drills), climbing a step (not two), the strata and the checkpoint, the touch inputs (walk to a column, drill
 * a side), the same well for every player, the bot.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
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
static pin in[MAX_PLAYERS];
static int evs;

static void idle(void)
{
    memset(in, 0, sizeof in);
    for (int p = 0; p < MAX_PLAYERS; p++) in[p].walk_to = -1;
}

static void step(int n)
{
    for (int i = 0; i < n; i++) {
        world_step(&W, in);
        evs |= W.events[0];
        idle();
    }
}

/* an empty well (the rules' tests: new rows are rock), rock from row 4 down, the gopher at (c, r) */
static well *box(int c, int r)
{
    world_init(&W, 1, 1234);
    well *w = &W.w[0];
    well_clear(w);
    for (int r = 4; r < w->top + ROWS; r++)      /* rock down to the undug rows */
        for (int x = 0; x < COLS; x++) well_set(w, x, r, K_HARD);
    gopher *g = &w->go;
    g->col = g->from_col = c;
    g->row = g->from_row = r;
    g->x = c * CELL;
    g->y = r * CELL;
    idle();
    evs = 0;
    return w;
}

static int kind(const well *w, int c, int r) { return well_kind(w, c, r); }
static int st(const well *w, int c, int r) { const gcell *x = well_cell(w, c, r); return x ? x->st : -1; }

static void test_drill(void)
{
    well *w = box(2, 2);
    for (int c = 0; c < COLS; c++) well_set(w, c, 3, c < 3 ? K_RED : K_BLUE);
    well_set(w, 5, 2, K_RED);                    /* a red not touching the group */
    step(2);
    in[0].down = 1;
    in[0].drill = 1;
    step(1);
    CHECK(kind(w, 0, 3) == K_EMPTY && kind(w, 1, 3) == K_EMPTY && kind(w, 2, 3) == K_EMPTY,
          "down + A pops the whole red group under the gopher");
    CHECK(kind(w, 3, 3) == K_BLUE && kind(w, 5, 2) == K_RED, "...and nothing else");
    CHECK(w->go.pops == 3 && (evs & EV_POP), "3 blocks popped (%d)", w->go.pops);
    step(DRILL_FRAMES + CELL / GOPHER_FALL + 2);
    CHECK(w->go.row == 3, "the gopher falls into the hole (row %d)", w->go.row);
    /* sideways: Left + A drills the left block */
    well_set(w, 1, 3, K_GREEN);
    step(2);
    in[0].dir = -1;
    in[0].drill = 1;
    step(1);
    CHECK(kind(w, 1, 3) == K_EMPTY, "Left + A drills the block on the left");
    step(DRILL_FRAMES + 2);
    /* A alone: where it faces (left, since) */
    well_set(w, 1, 3, K_YELLOW);
    in[0].drill = 1;
    step(1);
    CHECK(kind(w, 1, 3) == K_EMPTY, "A alone drills where the gopher faces");
}

static void test_support(void)
{
    well *w = box(8, 3);
    for (int c = 0; c < 8; c++) well_set(w, c, 3, K_BLUE);
    well_set(w, 6, 2, K_GREEN);                  /* on a blue: another colour holds it up */
    well_set(w, 0, 2, K_YELLOW);
    well_set(w, 0, 1, K_YELLOW);
    well_set(w, 1, 1, K_YELLOW);                 /* an overhang: stuck to the yellow beside it */
    well_set(w, 2, 1, K_YELLOW);
    step(3);
    CHECK(st(w, 6, 2) == C_STILL, "a block on another colour stands");
    CHECK(st(w, 2, 1) == C_STILL && st(w, 1, 1) == C_STILL, "an overhang stuck to a supported block stands");
    /* nothing under it but the gopher: it shakes, then falls on it */
    well *v = box(4, 3);
    well_set(v, 4, 1, K_RED);
    well_set(v, 4, 0, K_HARD);                   /* nothing holds them: the gopher holds nothing up */
    step(2);
    CHECK(st(v, 4, 1) == C_SHAKE && st(v, 4, 0) == C_SHAKE && (evs & EV_SHAKE), "a loose group shakes");
    int shake = stratum_param(0, P_SHAKE), fall = stratum_param(0, P_FALL);
    step(shake - 6);
    CHECK(st(v, 4, 1) == C_SHAKE && kind(v, 4, 1) == K_RED, "...for S_SHAKE frames (%d) before it falls", shake);
    step(6 + fall + 1);
    CHECK(kind(v, 4, 1) != K_RED && ((kind(v, 4, 2) == K_RED && st(v, 4, 2) == C_FALL) || !v->go.alive),
          "then it falls, a row per S_FALL frames");
    step(fall + 1);
    CHECK(!v->go.alive && v->go.out == OUT_CRUSH && (evs & EV_CRUSH), "a block landing on the gopher crushes it");
    /* the same, but the gopher steps aside in time */
    v = box(4, 3);
    well_set(v, 4, 1, K_RED);
    step(10);
    in[0].dir = 1;
    step(1);
    step(shake + 4 * fall);
    CHECK(v->go.alive && v->go.col == 5, "stepping aside in time: alive");
    CHECK(kind(v, 4, 3) == K_RED && st(v, 4, 3) == C_STILL, "the block lands on the floor and stands");
}

static void test_chain(void)
{
    well *w = box(8, 3);
    for (int c = 0; c < 3; c++) well_set(w, c, 3, K_BLUE);
    well_set(w, 3, 1, K_BLUE);                   /* falls next to the three blues: 4 */
    well_set(w, 6, 1, K_GREEN);
    well_set(w, 5, 3, K_GREEN);                  /* a green that falls next to one green: 2, no pop */
    step(stratum_param(0, P_SHAKE) + 4 * stratum_param(0, P_FALL));
    CHECK(kind(w, 0, 3) == K_EMPTY && kind(w, 3, 3) == K_EMPTY, "4 blues touching after a fall pop by themselves");
    CHECK((evs & EV_CHAIN) && w->chain == 1, "a chain (%d)", w->chain);
    CHECK(kind(w, 6, 3) == K_GREEN && kind(w, 5, 3) == K_GREEN, "2 greens stay");
    w = box(8, 3);
    for (int c = 0; c < 2; c++) well_set(w, c, 3, K_RED);
    well_set(w, 2, 1, K_RED);
    step(stratum_param(0, P_SHAKE) + 4 * stratum_param(0, P_FALL));
    CHECK(kind(w, 2, 3) == K_RED && kind(w, 0, 3) == K_RED, "3 after a fall stay");
    /* the gopher's drill on a group that was already 4+ pops it (and only a fall makes a chain) */
    w = box(4, 2);
    for (int c = 0; c < COLS; c++) well_set(w, c, 3, K_GREEN);
    step(3);
    CHECK(st(w, 0, 3) == C_STILL && kind(w, 0, 3) == K_GREEN, "a big group that never fell stays");
}

static void test_air(void)
{
    well *w = box(4, 3);
    int32_t a0 = w->go.air;
    step(100);
    CHECK(w->go.air == a0 - 100 * stratum_param(0, P_DRAIN), "the air drains S_DRAIN a frame (%d)", w->go.air);
    well_set(w, 5, 3, K_AIR);
    w->go.air = 50000;
    in[0].dir = 1;
    step(1);
    CHECK(w->go.airs == 1 && (evs & EV_AIR) && w->go.air > 50000 + AIR_ITEM - 1000, "walking into a bubble: +%d",
          AIR_ITEM);
    step(20);
    w->go.air = AIR_FULL - 1000;
    well_set(w, 4, 3, K_AIR);
    in[0].dir = -1;
    step(1);
    CHECK(w->go.air <= AIR_FULL, "never more than a full tank");
    /* a hard rock: 5 drills, and it costs air */
    w = box(4, 3);
    well_set(w, 5, 3, K_HARD);
    int32_t before = 0;
    for (int k = 0; k < HARD_HITS; k++) {
        step(DRILL_FRAMES + 1);
        if (k == HARD_HITS - 1) before = w->go.air;
        in[0].dir = 1;
        in[0].drill = 1;
        step(1);
        if (k < HARD_HITS - 1) CHECK(kind(w, 5, 3) == K_HARD, "a hard rock stands %d drills", k + 1);
    }
    CHECK(kind(w, 5, 3) == K_EMPTY && w->go.hards == 1, "...and breaks at the %dth", HARD_HITS);
    CHECK(w->go.air <= before - HARD_AIR, "breaking it costs %d air", HARD_AIR);
    /* out of air */
    w = box(4, 3);
    w->go.air = 50;
    step(3);
    CHECK(!w->go.alive && w->go.out == OUT_AIR && (evs & EV_NOAIR), "air 0: out");
}

static void test_climb(void)
{
    well *w = box(4, 3);
    well_set(w, 5, 3, K_GREEN);
    int held = 0;
    for (; held < 40 && !(evs & EV_CLIMB); held++) {
        in[0].dir = 1;
        step(1);
    }
    step(CLIMB_FRAMES + 2);
    CHECK(w->go.col == 5 && w->go.row == 2 && held == CLIMB_HOLD, "pushed against a one-block step %d frames, the gopher "
          "climbs (%d, %d)", held, w->go.col, w->go.row);
    w = box(4, 3);
    well_set(w, 5, 3, K_GREEN);
    well_set(w, 5, 2, K_RED);
    for (int i = 0; i < CLIMB_HOLD + CLIMB_FRAMES + 4; i++) {
        in[0].dir = 1;
        step(1);
    }
    CHECK(w->go.col == 4 && w->go.row == 3, "not two blocks");
    w = box(4, 3);
    well_set(w, 5, 3, K_GREEN);
    in[0].dir = 1;
    step(3);
    CHECK(w->go.col == 4 && w->go.face == 1, "a tap toward a block only turns (no climb)");
    /* a block falling down the gopher's own column while it climbs out of it: the climb goes up that column first, so
     * the block comes down on it (it used to pass through the gopher: only the cells it left and went to counted) */
    w = box(4, 3);
    well_set(w, 5, 3, K_GREEN);
    well_set(w, 4, 1, K_RED);
    step(1);
    gcell *x = (gcell *)well_cell(w, 4, 1);
    x->st = C_FALL;                              /* falling, the next tick in 2 frames: into (4, 2) */
    x->timer = 0;
    w->dirty = 0;
    w->fall_t = stratum_param(0, P_FALL) - 2;
    w->go.hold_t = CLIMB_HOLD - 1;
    in[0].dir = 1;
    step(1);
    CHECK(w->go.act == ACT_CLIMB && w->go.col == 5 && w->go.row == 2, "the climb starts (%d %d)", w->go.col, w->go.row);
    step(CLIMB_FRAMES + 2);
    CHECK(!w->go.alive && w->go.out == OUT_CRUSH, "a block falling into the column it climbs up crushes the gopher");
    /* the same block a moment later, the gopher over the edge already (past half the climb): it lives */
    w = box(4, 3);
    well_set(w, 5, 3, K_GREEN);
    well_set(w, 4, 1, K_RED);
    step(1);
    x = (gcell *)well_cell(w, 4, 1);
    x->st = C_FALL;
    x->timer = 0;
    w->dirty = 0;
    w->fall_t = stratum_param(0, P_FALL) - 1 - CLIMB_FRAMES / 2;   /* the tick at half the climb */
    w->go.hold_t = CLIMB_HOLD - 1;
    in[0].dir = 1;
    step(1);
    step(CLIMB_FRAMES + 2);
    CHECK(w->go.alive && w->go.col == 5 && w->go.row == 2, "...but not once it is past half the climb");
}

static void test_strata(void)
{
    CHECK(stratum_of_row(99) == 0 && stratum_of_row(100) == 1, "a stratum every %d rows", STRATUM_M);
    CHECK(stratum_param(1, P_SHAKE) < stratum_param(0, P_SHAKE) && stratum_param(5, P_DRAIN) > stratum_param(0, P_DRAIN),
          "deeper: shorter shakes, more air drain");
    CHECK(stratum_param(9, P_DRAIN) > stratum_param(5, P_DRAIN), "past the tables the drain still grows");
    well *w = box(4, 3);
    w->go.depth = STRATUM_M - 1;
    w->go.air = 40000;
    w->go.row = STRATUM_M - 1;                  /* depth = row + 1 */
    step(1);
    CHECK(w->go.stratum == 1 && (evs & EV_CHECKPOINT) && w->freeze > 0, "%d m: a checkpoint (stratum %d)", STRATUM_M,
          w->go.stratum);
    CHECK(w->go.air >= 40000 + CHECK_AIR - 100, "...the air topped up");
    int32_t a = w->go.air;
    step(CHECK_FREEZE - 5);
    CHECK(w->go.air == a, "the celebration: the well stands still, no air drained");
}

static void test_touch(void)
{
    well *w = box(1, 3);
    in[0].walk_to = 6;
    step(1);
    step(6 * (CELL / WALK_SPEED) + 6);
    CHECK(w->go.col == 6, "touch: walks to the column tapped (%d)", w->go.col);
    well_set(w, 7, 3, K_RED);
    in[0].tap_drill = 2;
    step(1);
    CHECK(kind(w, 7, 3) == K_EMPTY, "touch: a tap on the block on the right drills it");
    step(DRILL_FRAMES + 2);
    well_set(w, 7, 3, K_RED);
    well_set(w, 7, 2, K_EMPTY);
    in[0].walk_to = 8;
    step(1);
    step(CLIMB_FRAMES + 20);
    CHECK(w->go.col == 8 && (evs & EV_CLIMB), "touch: walking to a column climbs a step on the way (%d)", w->go.col);
}

/* very deep (3 km: past 32767 well pixels): the effects keep their place (an fx's y was an int16 and wrapped) */
static void test_deep(void)
{
    world_init(&W, 1, 99);
    well *w = &W.w[0];
    well_clear(w);
    int row = 3019;
    w->top = row - ROWS_ABOVE;
    for (int r = row + 1; r < w->top + ROWS; r++)
        for (int c = 0; c < COLS; c++) well_set(w, c, r, K_HARD);
    gopher *g = &w->go;
    g->col = g->from_col = 4;
    g->row = g->from_row = row;
    g->x = 4 * CELL;
    g->y = row * CELL;
    g->depth = row + 1;
    g->stratum = g->depth / STRATUM_M;
    well_set(w, 5, row, K_RED);
    idle();
    step(2);
    in[0].dir = 1;
    in[0].drill = 1;
    world_step(&W, in);
    CHECK(kind(w, 5, row) == K_EMPTY && W.nfx >= 1 && W.fx[0].kind == FX_POP && W.fx[0].y == row * CELL &&
          W.fx[0].x == 5 * CELL, "3 km down: the pop's effect at its block (y %d, not %d)", (int)W.fx[0].y, row * CELL);
    idle();
}

static void test_wells(void)
{
    world_init(&W, 4, 777);
    world_init(&W2, 2, 777);
    int same = 1, colours = 0;
    for (int r = 0; r < 20; r++)
        for (int c = 0; c < COLS; c++) {
            same &= well_kind(&W.w[0], c, r) == well_kind(&W.w[3], c, r) &&
                    well_kind(&W.w[0], c, r) == well_kind(&W2.w[1], c, r);
            colours |= 1 << well_kind(&W.w[0], c, r);
        }
    CHECK(same, "every player digs the same well");
    CHECK((colours & 0x1e) == 0x1e, "four colours");
    world_init(&W2, 1, 778);
    int diff = 0;
    for (int r = 0; r < 20; r++)
        for (int c = 0; c < COLS; c++) diff += well_kind(&W.w[0], c, r) != well_kind(&W2.w[0], c, r);
    CHECK(diff > 50, "another seed, another well");
}

static void test_bot(void)
{
    world_init(&W, 1, 4242);
    for (int f = 0; f < 3600 && W.w[0].go.alive; f++) {
        idle();
        bot_think(&W, 0, 0, &in[0]);
        world_step(&W, in);
    }
    CHECK(W.w[0].go.depth >= 80, "the bot digs 80 m in a minute (%d)", W.w[0].go.depth);
}

int main(void)
{
    test_drill();
    test_support();
    test_chain();
    test_air();
    test_climb();
    test_strata();
    test_touch();
    test_deep();
    test_wells();
    test_bot();
    printf("gopherdig rules: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
