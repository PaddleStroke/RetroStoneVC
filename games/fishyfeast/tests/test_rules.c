/*
 * Fishy Feast: the rules alone (src/play.c, no video, no sound, no screen width: this program links play.c with the
 * SDK's RNG only): eating thresholds, speed and size, split (1, 2, 4, 8, 16 pieces, the minimum mass, the front half
 * shooting ahead, a piece eaten by a mid-size fish, merging after the timer, out only when the last piece is gone),
 * eject, the urchins' burst, the decay and the bots' cap, the bots' sanity (they flee the bigger, chase the smaller,
 * never eat a fish not smaller enough, leave new players alone, never gang up), the split-screen layout and the touch
 * stick and buttons, determinism.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/fishyfeast/LICENSE.
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

static world W, W2;
static pin in[MAX_PLAYERS];
static void idle(void) { memset(in, 0, sizeof in); }

/* a pond with nobody in it (no bots, no plankton): the tests put the fish they need */
static void lone(world *w, int players)
{
    world_init(w, players, 1234);
    world_start(w);
    memset(w->c, 0, sizeof w->c);
    memset(w->pl, 0, sizeof w->pl);
    for (int i = 0; i < PLANKTON_N; i++) { w->sp[i].on = 0; w->sp[i].regrow = 60000; }
    for (int o = 0; o < OWNERS; o++) {
        w->own[o].alive = 0;
        w->own[o].ncells = 0;
        w->own[o].mass = 0;
        w->own[o].respawn_t = 30000;
    }
    w->started = 1;
    w->t = 1;
}

/* a piece of mass m (units) for owner o at (x, y) px */
static int put(world *w, int o, int x, int y, int m)
{
    for (int i = 0; i < MAX_CELLS; i++) {
        cell *c = &w->c[i];
        if (c->on) continue;
        memset(c, 0, sizeof *c);
        c->on = 1;
        c->owner = (int16_t)o;
        c->x = x * FX;
        c->y = y * FX;
        c->mass = m * MQ;
        c->r = fish_radius(c->mass);
        owner *ow = &w->own[o];
        ow->alive = 1;
        ow->out = 0;
        ow->ncells++;
        ow->mass += c->mass;
        ow->cx = c->x;
        ow->cy = c->y;
        return i;
    }
    return -1;
}

static int pieces(const world *w, int o)
{
    int n = 0;
    for (int i = 0; i < MAX_CELLS; i++) n += w->c[i].on && w->c[i].owner == o;
    return n;
}

static int total(const world *w, int o)
{
    int64_t m = 0;
    for (int i = 0; i < MAX_CELLS; i++)
        if (w->c[i].on && w->c[i].owner == o) m += w->c[i].mass;
    return (int)(m / MQ);
}

static void steps(world *w, int n)
{
    for (int k = 0; k < n; k++) world_step(w, in);
}

static void test_sizes(void)
{
    CHECK(can_eat(125 * MQ, 100 * MQ), "125 eats 100 (25%% heavier)");
    CHECK(!can_eat(124 * MQ, 100 * MQ), "124 does not eat 100");
    int r10 = PX(fish_radius(10 * MQ)), r100 = PX(fish_radius(100 * MQ));
    CHECK(r10 == 6 || r10 == 7, "a new fish's radius: %d px", r10);
    CHECK(r100 == 21 || r100 == 22, "mass 100: %d px", r100);
    int v10 = fish_speed(fish_radius(10 * MQ)), v100 = fish_speed(fish_radius(100 * MQ)),
        v500 = fish_speed(fish_radius(500 * MQ)), v5000 = fish_speed(fish_radius(5000 * MQ));
    CHECK(v10 > v100 && v100 > v500 && v500 >= v5000, "the bigger, the slower: %d %d %d %d", v10, v100, v500, v5000);
    CHECK(v10 > 400 && v10 < 480, "a new fish swims about 1.7 px a frame: %d/256", v10);
    CHECK(v5000 == SPEED_MIN, "the slowest: %d", v5000);
    /* swimming: full speed for its size, in 8 directions */
    lone(&W, 1);
    put(&W, 0, 900, 700, 10);
    idle();
    in[0].dx = 256;
    steps(&W, 60);
    int dx = PX(W.own[0].cx) - 900;
    CHECK(dx > 85 && dx < 105, "60 frames to the right at full speed: %d px", dx);
    in[0].dx = 181;
    in[0].dy = 181;
    int x0 = PX(W.own[0].cx), y0 = PX(W.own[0].cy);
    steps(&W, 60);
    CHECK(PX(W.own[0].cx) - x0 > 55 && PX(W.own[0].cy) - y0 > 55, "a diagonal");
    idle();
    steps(&W, 60);
    x0 = PX(W.own[0].cx);
    steps(&W, 10);
    CHECK(PX(W.own[0].cx) == x0, "no input: it stops");
}

static void test_eating(void)
{
    /* plankton */
    lone(&W, 1);
    put(&W, 0, 900, 700, 10);
    W.sp[0].on = 1;
    W.sp[0].x = 902;
    W.sp[0].y = 700;
    idle();
    steps(&W, 1);
    CHECK(total(&W, 0) == 11 && !W.sp[0].on, "a plankton speck: +1 (%d)", total(&W, 0));
    CHECK(W.sp[0].regrow > 0, "it grows again later");
    /* a fish 25% heavier eats one it covers */
    lone(&W, 1);
    int a = put(&W, 0, 900, 700, 125);
    put(&W, 5, 905, 700, 100);
    steps(&W, 1);
    CHECK(W.c[a].on && pieces(&W, 5) == 0 && total(&W, 0) == 225, "125 swims over 100: eats it (%d)", total(&W, 0));
    CHECK(W.own[5].alive == 0 && W.own[5].respawn_t > 0, "the bot comes back later");
    /* 20% heavier: no */
    lone(&W, 1);
    put(&W, 0, 900, 700, 120);
    put(&W, 5, 900, 700, 100);
    W.own[5].think_t = 1000;                    /* the bot stays still */
    steps(&W, 30);
    CHECK(pieces(&W, 5) == 1 && pieces(&W, 0) == 1, "120 and 100: nobody eats");
    /* heavy enough but not over it: no */
    lone(&W, 1);
    put(&W, 0, 900, 700, 400);
    put(&W, 5, 940, 700, 40);
    W.own[5].think_t = 1000;
    steps(&W, 1);
    CHECK(pieces(&W, 5) == 1, "400 touching 40 at its edge does not eat it yet");
}

static void test_split(void)
{
    lone(&W, 1);
    put(&W, 0, 900, 700, 35);
    idle();
    CHECK(owner_split(&W, 0, 256, 0) == 0, "35: too light to split (SPLIT_MIN %d)", SPLIT_MIN);
    lone(&W, 1);
    put(&W, 0, 900, 700, 36);
    CHECK(owner_split(&W, 0, 256, 0) == 1 && pieces(&W, 0) == 2, "36 splits in 2");
    /* 1, 2, 4, 8, 16: every piece big enough splits at each press; never more than 16 */
    lone(&W, 1);
    put(&W, 0, 900, 700, 1200);
    int want[5] = {2, 4, 8, 16, 16};
    for (int k = 0; k < 5; k++) {
        in[0].split = 1;
        in[0].dx = 256;
        world_step(&W, in);
        idle();
        CHECK(pieces(&W, 0) == want[k], "press %d: %d pieces (%d)", k + 1, want[k], pieces(&W, 0));
        CHECK(W.own[0].ncells == want[k], "the count follows");
    }
    CHECK(total(&W, 0) >= 1195, "splitting keeps the mass (%d)", total(&W, 0));
    /* the front half shoots ahead */
    lone(&W, 1);
    put(&W, 0, 600, 700, 200);
    in[0].split = 1;
    in[0].dx = 256;
    world_step(&W, in);
    idle();
    in[0].dx = 256;
    steps(&W, 20);
    int xs[2], n = 0;
    for (int i = 0; i < MAX_CELLS && n < 2; i++)
        if (W.c[i].on) xs[n++] = PX(W.c[i].x);
    int gap = xs[0] > xs[1] ? xs[0] - xs[1] : xs[1] - xs[0];
    CHECK(n == 2 && gap > 50, "the shot half is ahead: %d px apart", gap);
    /* a piece is vulnerable: a mid-size fish (heavier than the piece, lighter than the whole) eats it */
    lone(&W, 1);
    put(&W, 0, 600, 700, 200);
    in[0].split = 1;
    in[0].dx = 256;
    world_step(&W, in);
    idle();
    int front = -1;
    for (int i = 0; i < MAX_CELLS; i++)
        if (W.c[i].on && (front < 0 || W.c[i].x > W.c[front].x)) front = i;
    put(&W, 6, PX(W.c[front].x), PX(W.c[front].y), 130);     /* 130 < 200 but >= 1.25 x 100 */
    W.own[6].think_t = 1000;
    world_step(&W, in);
    CHECK(pieces(&W, 0) == 1 && !W.own[0].out, "the bot ate one piece (%d left), the player swims on", pieces(&W, 0));
    CHECK(W.events[0] & EV_LOST, "EV_LOST");
    int last = -1;
    for (int i = 0; i < MAX_CELLS; i++)
        if (W.c[i].on && W.c[i].owner == 0) last = i;
    put(&W, 7, PX(W.c[last].x), PX(W.c[last].y), 300);
    W.own[7].think_t = 1000;
    world_step(&W, in);
    CHECK(W.own[0].out && (W.events[0] & EV_OUT), "the last piece eaten: out");
    CHECK(W.own[0].eaten_by == 7, "eaten by the bot 7 (%d)", W.own[0].eaten_by);
    CHECK(!world_running(&W), "no player swims: the run is over");
    /* merging: not before the timer, then they come together */
    lone(&W, 1);
    put(&W, 0, 900, 700, 140);
    in[0].split = 1;
    in[0].dx = 256;
    world_step(&W, in);
    idle();
    steps(&W, MERGE_BASE - 20);
    CHECK(pieces(&W, 0) == 2, "not merged before the timer");
    steps(&W, MERGE_BASE / 2 + MERGE_EXTRA_MAX);
    CHECK(pieces(&W, 0) == 1 && total(&W, 0) == 140, "merged after it (%d pieces, mass %d)", pieces(&W, 0), total(&W, 0));
    CHECK(W.merges == 1, "one merge");
}

static void test_eject(void)
{
    lone(&W, 1);
    put(&W, 0, 900, 700, 100);
    in[0].eject = 1;
    in[0].dx = 256;
    world_step(&W, in);
    idle();
    int p = -1;
    for (int i = 0; i < MAX_PELLETS; i++)
        if (W.pl[i].on) p = i;
    CHECK(p >= 0 && total(&W, 0) == 100 - EJECT_COST, "B: a pellet, the fish loses %d (%d)", EJECT_COST, total(&W, 0));
    steps(&W, 30);
    CHECK(p >= 0 && PX(W.pl[p].x) > 950, "it flies ahead (%d)", p >= 0 ? PX(W.pl[p].x) : -1);
    lone(&W, 1);
    put(&W, 0, 900, 700, 30);
    in[0].eject = 1;
    world_step(&W, in);
    idle();
    CHECK(total(&W, 0) == 30, "a fish lighter than %d cannot eject", EJECT_MIN);
    /* someone else eats it */
    lone(&W, 1);
    put(&W, 0, 900, 700, 100);
    in[0].eject = 1;
    in[0].dx = 256;
    world_step(&W, in);
    idle();
    steps(&W, 30);
    p = -1;
    for (int i = 0; i < MAX_PELLETS; i++)
        if (W.pl[i].on) p = i;
    if (p >= 0) {
        put(&W, 8, PX(W.pl[p].x), PX(W.pl[p].y), 20);
        W.own[8].think_t = 1000;
        steps(&W, 1);
        CHECK(total(&W, 8) == 20 + EJECT_MASS && !W.pl[p].on, "a bot eats the pellet: +%d", EJECT_MASS);
    }
}

static void test_urchin(void)
{
    lone(&W, 1);
    int ux = W.u[0].x, uy = W.u[0].y;
    put(&W, 0, ux, uy, 100);
    steps(&W, 1);
    CHECK(pieces(&W, 0) == 1 && W.bursts == 0, "lighter than an urchin: it hides there safely");
    lone(&W, 1);
    put(&W, 0, ux + 30, uy, 400);
    steps(&W, 1);
    CHECK(pieces(&W, 0) == BURST_PIECES && W.bursts == 1, "400 touches an urchin: bursts into %d (%d)", BURST_PIECES, pieces(&W, 0));
    CHECK(total(&W, 0) >= 398, "the burst keeps the mass (%d)", total(&W, 0));
    CHECK(W.events[0] & EV_BURST, "EV_BURST");
    steps(&W, 20);
    int far = 0;
    for (int i = 0; i < MAX_CELLS; i++)
        if (W.c[i].on) {
            int dx = PX(W.c[i].x) - ux, dy = PX(W.c[i].y) - uy;
            if (dx * dx + dy * dy > far) far = dx * dx + dy * dy;
        }
    CHECK(far > 60 * 60, "the pieces scatter (the farthest %d px away)", isqrt64(far));
}

static void test_decay(void)
{
    lone(&W, 1);
    put(&W, 0, 900, 700, 1000);
    steps(&W, 600);
    int m = total(&W, 0);
    CHECK(m >= 970 && m <= 980, "1000 loses about 0.25%% a second: %d after 10 s", m);
    lone(&W, 1);
    put(&W, 0, 900, 700, 100);
    steps(&W, 600);
    CHECK(total(&W, 0) == 100, "a small fish does not decay");
    lone(&W, 1);
    put(&W, 5, 900, 700, 1000);
    W.own[5].think_t = 30000;
    steps(&W, 600);
    m = total(&W, 5);
    CHECK(m < 850, "a bot above the cap loses more: %d", m);
}

static void test_bots(void)
{
    /* flee: a bot near a much bigger fish swims away */
    lone(&W, 1);
    put(&W, 0, 960, 700, 300);
    put(&W, 5, 900, 700, 40);
    idle();
    steps(&W, 90);
    int d = PX(W.own[0].cx) - PX(W.own[5].cx);
    CHECK(d > 120, "the bot flees the bigger fish: %d px away", d);
    /* chase: a hunter goes for a smaller player (after the grace) and eats it */
    lone(&W, 1);
    W.t = PLAYER_GRACE + 1;
    put(&W, 0, 1000, 700, 40);
    W.own[6].pers = BK_HUNTER;
    put(&W, 6, 900, 700, 200);
    int ate = 0;
    for (int k = 0; k < 400 && !ate; k++) {
        world_step(&W, in);
        ate = W.own[0].out;
    }
    CHECK(ate, "the hunter catches a still player");
    /* grace: in the first seconds no bot hunts a player */
    lone(&W, 1);
    W.t = 1;
    put(&W, 0, 1000, 700, 40);
    put(&W, 6, 900, 700, 200);
    W.own[6].pers = BK_HUNTER;
    int chased = 0;
    for (int k = 0; k < 200; k++) {
        world_step(&W, in);
        chased |= W.own[6].mode == BM_CHASE && W.own[6].target == 0;
    }
    CHECK(!chased, "no bot hunts a player in its first %d frames", PLAYER_GRACE);
    /* never two bots on a small player at once */
    lone(&W, 1);
    W.t = PLAYER_GRACE + 1;
    put(&W, 0, 1000, 700, 60);
    for (int b = 0; b < 3; b++) {
        W.own[5 + b].pers = BK_HUNTER;
        put(&W, 5 + b, 1000 + (b - 1) * 160, 560, 220);
    }
    int most = 0;
    for (int k = 0; k < 300 && !W.own[0].out; k++) {
        world_step(&W, in);
        int n = 0;
        for (int b = 0; b < 3; b++) n += W.own[5 + b].mode == BM_CHASE && W.own[5 + b].target == 0;
        if (n > most) most = n;
    }
    CHECK(most <= CHASERS_SMALL, "one bot at most hunts a small player (%d)", most);
    /* no bot eats a fish not smaller enough: a hungry hunter right next to a player 90%% of its mass */
    lone(&W, 1);
    W.t = PLAYER_GRACE + 1;
    put(&W, 0, 1000, 700, 100);
    W.own[6].pers = BK_HUNTER;
    put(&W, 6, 1010, 700, 112);
    steps(&W, 200);
    CHECK(!W.own[0].out, "112 never eats 100");
    /* a full pond: bots live, eat, die and come back; nobody passes the cap by much; determinism */
    world_init(&W, 1, 99);
    world_init(&W2, 1, 99);
    int top = 0, deaths = 0;
    for (int k = 0; k < 12000; k++) {
        world_step(&W, in);
        world_step(&W2, in);
        for (int o = MAX_PLAYERS; o < OWNERS; o++) {
            int m = owner_mass(&W, o);
            if (m > top) top = m;
        }
        deaths = W.eats;
    }
    CHECK(top < BOT_CAP * 3 / 2, "the bots stay under about the cap: %d", top);
    CHECK(deaths >= 5, "the bots eat each other: %d fish eaten in 200 s", deaths);
    CHECK(!memcmp(&W, &W2, sizeof W), "the same seed, the same pond");
    int alive = 0;
    for (int o = MAX_PLAYERS; o < OWNERS; o++) alive += W.own[o].alive;
    CHECK(alive >= BOTS - 4, "the eaten bots come back: %d alive", alive);
    /* the player bot plays well: it grows */
    world_init(&W, 1, 7);
    world_start(&W);
    int best = 0;
    for (int k = 0; k < 6000 && !W.own[0].out; k++) {
        pin b[MAX_PLAYERS];
        memset(b, 0, sizeof b);
        bot_think(&W, 0, 0, &b[0]);
        b[0].any = 1;
        world_step(&W, b);
        if (W.own[0].best > best) best = W.own[0].best;
    }
    CHECK(best >= 60, "the player bot grows: best %d", best);
}

static void test_views_touch(void)
{
    vrect v[4];
    CHECK(view_layout(1, 0, 320, v) == 1 && v[0].w == 320 && v[0].h == 240, "1 player: the full screen");
    CHECK(view_layout(2, 0, 320, v) == 2 && v[0].w == 159 && v[1].x == 161 && v[1].h == 240, "2: side by side");
    CHECK(view_layout(3, 0, 320, v) == 4 && v[3].x == 161 && v[3].y == 121, "3: quarters, the 4th the map");
    CHECK(view_layout(4, 0, 320, v) == 4 && v[2].y == 121 && v[3].w == 159 && v[3].h == 119, "4: quarters");
    CHECK(view_layout(2, -53, 427, v) == 2 && v[0].x == -53 && v[0].w == 212 && v[1].x + v[1].w == 374,
          "427 columns: wider halves (%d %d %d)", v[0].x, v[0].w, v[1].x);
    CHECK(view_kind(1) == VK_FULL && view_kind(2) == VK_HALF && view_kind(4) == VK_QUAD, "the kinds");
    int16_t dx, dy;
    touch_stick(100, 100, 100 + STICK_R, 100, &dx, &dy);
    CHECK(dx == 256 && dy == 0, "the stick at its full length: full speed right");
    touch_stick(100, 100, 100 + STICK_R * 3, 100, &dx, &dy);
    CHECK(dx == 256, "further: still full speed");
    touch_stick(100, 100, 101, 101, &dx, &dy);
    CHECK(dx == 0 && dy == 0, "a tiny drag: nothing");
    touch_stick(100, 100, 100, 100 + STICK_R / 2, &dx, &dy);
    CHECK(dx == 0 && dy > 80 && dy < 180, "half the drag: about half speed down (%d)", dy);
    touch_stick(100, 100, 80, 80, &dx, &dy);
    CHECK(dx < -150 && dy < -150, "a diagonal (%d, %d)", dx, dy);
    view_layout(1, 0, 320, v);
    tbutton s, e;
    touch_buttons(&v[0], &s, &e);
    CHECK(s.x > e.x && s.y + s.r < 240 && s.x + s.r < 320, "SPLIT at the bottom right, EJECT left of it");
    CHECK(touch_on_button(&s, s.x, s.y) && !touch_on_button(&s, e.x, e.y) && touch_on_button(&e, e.x + e.r, e.y),
          "the buttons' areas");
    CHECK(touch_stick_zone(&v[0], 20, 120) && touch_stick_zone(&v[0], 180, 200) && !touch_stick_zone(&v[0], 250, 60),
          "the stick: the left part of the screen");
    view_layout(2, -53, 427, v);
    touch_buttons(&v[0], &s, &e);
    CHECK(s.x < v[0].x + v[0].w && s.x > v[0].x + v[0].w / 2, "2 players: the buttons in player 1's view");
}

int main(void)
{
    test_sizes();
    test_eating();
    test_split();
    test_eject();
    test_urchin();
    test_decay();
    test_bots();
    test_views_touch();
    printf("fishyfeast rules: %d checks, %d failed\n", checks, fails);
    if (!fails) printf("  ok   the rules\n");
    return fails ? 1 : 0;
}
