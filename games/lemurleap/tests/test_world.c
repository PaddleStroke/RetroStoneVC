/*
 * Lemur Leap: the rules' tests (src/world.c linked alone, no SDK):
 *   - the generator's path over many seeds: every step between two rows of the path within the bounce's reach (the
 *     height after the leaf below, with a margin) and within the lemur's reach sideways in the time the bounce gives
 *     (across the wrap; a moving leaf anywhere on its run), never a dry leaf on it;
 *   - the bot climbs: with the pests harmless (god) it climbs 1500 m on every seed (the path is really reachable, with
 *     the moving leaves, the clouds, the puffs); with the pests, most seeds go far;
 *   - determinism: the same seed and inputs, the same run (hash frame by frame); another seed, another run;
 *   - the rules: the bounce's height, the wrap, a dry leaf breaks without a bounce, a spring, a stomp, a pest's touch,
 *     a seed pops a pest, the propeller, the shield, the fall out.
 *   test_world [seeds]
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/lemurleap/LICENSE.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)
#define OK(...) do { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } while (0)


/* ---- the generator's path ----------------------------------------------------------------------------------------- */
typedef struct row { int x, y, type, w, moving, x0, x1; } row;

static int path_rows(uint32_t seed, int metres, row *out, int max)
{
    static world w;
    world_init(&w, seed, 0);
    int n = 0;
    out[n++] = (row){0, 0, P_LEAF, COL_W, 0, 0, 0};          /* (x0 0, x1 0, w COL_W: every column) */            /* the ground: the whole column */
    while (n < max) {
        for (int i = 0; i < MAX_PLAT; i++) {
            plat *p = &w.p[i];
            if (p->used && p->path == 1) {
                p->path = 2;
                out[n++] = (row){p->x / FX, p->y / FX, p->type, p->w, p->type == P_MOVE || p->type == P_CLOUD, p->x0, p->x1};
                if (n >= max) break;
            }
        }
        if (-w.cam_y / PX_PER_M > metres) break;
        w.cam_y -= 150;
        for (int i = 0; i < MAX_PLAT; i++)                      /* (the step's culling) */
            if (w.p[i].used && w.p[i].y / FX > w.cam_y + STAGE_H + KEEP_BELOW) w.p[i].used = 0;
        world_generate(&w);
    }
    /* rows come out of the array unordered within a batch: sort them by height */
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && out[j].y > out[j - 1].y; j--) { row t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
    return n;
}

static int apex_of(int v)
{
    long y = 0, vy = -v, top = 0;
    while (vy < 0) { vy += GRAVITY; y += vy; if (y < top) top = y; }
    return (int)(-top / FX);
}

static void test_path(int seeds)
{
    static row rows[20000];
    int worst_margin = 1 << 30, steps = 0, maxgap = 0, kinds[P_TYPES] = {0};
    for (int s = 1; s <= seeds; s++) {
        int n = path_rows((uint32_t)s, 2500, rows, 20000), bad = 0;
        for (int i = 1; i < n; i++) {
            const row *a = &rows[i - 1], *b = &rows[i];
            int gap = a->y - b->y, v = bounce_v(a->type), apex = apex_of(v);
            kinds[b->type]++;
            steps++;
            if (gap > maxgap) maxgap = gap;
            if (b->type == P_DRY) { CHECK(0, "seed %d: a dry leaf on the path at %d m", s, -b->y / PX_PER_M); bad = 1; }
            if (gap > plat_reach(a->type) || gap > apex - 12 || gap < 12) {
                CHECK(0, "seed %d: a step of %d px after a %d at %d m (reach %d, apex %d)", s, gap, a->type,
                      -a->y / PX_PER_M, plat_reach(a->type), apex);
                bad = 1;
            }
            int t = frames_down(v, gap), need = span_gap(a->x0, a->x1 + a->w - 1, b->x0, b->x1 + b->w - 1);
            int margin = side_reach(t) - FEET_HW - need;
            if (margin < worst_margin) worst_margin = margin;
            if (margin < 8) {
                CHECK(0, "seed %d: %d px sideways in %d frames at %d m (reach %d)", s, need, t, -b->y / PX_PER_M,
                      side_reach(t));
                bad = 1;
            }
            if (bad) break;
        }
        if (bad) break;
    }
    if (!fails)
        OK("the path of %d seeds to 2500 m: %d steps (leaf %d, moving %d, puff %d, mushroom %d, cloud %d), the largest "
           "%d px, sideways margin at least %d px", seeds, steps, kinds[P_LEAF], kinds[P_MOVE], kinds[P_PUFF],
           kinds[P_SHROOM], kinds[P_CLOUD], maxgap, worst_margin);
}

/* ---- the bot ------------------------------------------------------------------------------------------------------ */
static int bot_run(uint32_t seed, int god, int metres, int max_frames, int *frames)
{
    static world w;
    world_init(&w, seed, 0);
    w.god = (uint8_t)god;
    int f = 0;
    while (!w.over && world_height_m(&w) < metres && f < max_frames) {
        world_step(&w, bot_input(&w));
        f++;
    }
    if (frames) *frames = f;
    return world_height_m(&w);
}

static void test_bot(int seeds)
{
    int all = 1, worst = 1 << 30, wseed = 0;
    long frames = 0;
    for (int s = 1; s <= seeds; s++) {
        int f, m = bot_run((uint32_t)s, 1, 1500, 60 * 60 * 20, &f);
        frames += f;
        if (m < worst) { worst = m; wseed = s; }
        if (m < 1500) all = 0;
    }
    CHECK(all, "the bot (pests harmless) fell on a seed: %d m on seed %d", worst, wseed);
    if (all) OK("the bot climbs 1500 m on %d seeds with the pests harmless (%.1f min a climb on average)", seeds,
                frames / (double)seeds / 3600.0);
    int n = seeds < 40 ? seeds : 40, far = 0, sum = 0, lo = 1 << 30;
    for (int s = 1; s <= n; s++) {
        int m = bot_run((uint32_t)(1000 + s), 0, 3000, 60 * 60 * 30, NULL);
        sum += m;
        if (m >= 600) far++;
        if (m < lo) lo = m;
    }
    CHECK(far * 10 >= n * 7, "with the pests, the bot reaches 600 m on only %d of %d seeds", far, n);
    OK("with the pests, the bot reaches 600 m on %d of %d seeds (average %d m, the lowest %d m)", far, n, sum / n, lo);
}

/* ---- determinism -------------------------------------------------------------------------------------------------- */
static void test_determinism(void)
{
    static world a, b;
    world_init(&a, 77, 120);
    world_init(&b, 77, 120);
    int same = 1;
    for (int f = 0; f < 6000; f++) {
        int in = (f / 37) % 3 == 0 ? IN_LEFT : (f / 37) % 3 == 1 ? IN_RIGHT | ((f & 8) ? IN_SHOOT : 0) : 0;
        world_step(&a, in);
        world_step(&b, in);
        if (world_hash(&a) != world_hash(&b)) { same = 0; break; }
    }
    CHECK(same, "two runs of the same seed and inputs differ");
    world_init(&b, 78, 120);
    CHECK(world_hash(&a) != world_hash(&b), "two seeds give the same world");
    if (same) OK("determinism: the same seed and inputs give the same run frame by frame (6000 frames); another seed another");
}

/* ---- the rules ------------------------------------------------------------------------------------------------------- */
static void empty(world *w)
{
    memset(w->p, 0, sizeof w->p);
    memset(w->c, 0, sizeof w->c);
    memset(w->e, 0, sizeof w->e);
    w->gen_y = -1000000;                                    /* (no more rows) */
}
static plat *put(world *w, int type, int x, int y)
{
    for (int i = 0; i < MAX_PLAT; i++)
        if (!w->p[i].used) {
            plat *p = &w->p[i];
            memset(p, 0, sizeof *p);
            p->used = 1;
            p->type = (uint8_t)type;
            p->w = 32;
            p->x = x * FX;
            p->y = y * FX;
            return p;
        }
    return NULL;
}

static void test_rules(void)
{
    static world w;
    /* the bounce off the ground: 80 px */
    world_init(&w, 5, 0);
    empty(&w);
    int top = 0;
    for (int f = 0; f < 60; f++) { world_step(&w, 0); if (w.h.y / FX < top) top = w.h.y / FX; }
    CHECK(top <= -79 && top >= -82, "a bounce rises %d px (80 expected)", -top);
    /* the wrap */
    world_init(&w, 5, 0);
    empty(&w);
    w.h.x = 3 * FX;
    for (int f = 0; f < 20; f++) world_step(&w, IN_LEFT);
    CHECK(w.h.x / FX > COL_W - 60, "out on the left, in on the right (x %d)", w.h.x / FX);
    /* a dry leaf breaks without a bounce; a spring launches 258 px */
    world_init(&w, 5, 0);
    empty(&w);
    w.cam_y = -540;
    w.h.y = -400 * FX;
    w.h.vy = 0;
    w.h.x = 50 * FX;
    put(&w, P_DRY, 34, -380);
    int broke = 0;
    for (int f = 0; f < 30; f++) { world_step(&w, 0); if (w.events & EV_BREAK) broke = 1; }
    CHECK(broke && w.h.vy > 0, "a dry leaf breaks (%d) and the fall goes on (vy %d)", broke, w.h.vy);
    world_init(&w, 5, 0);
    empty(&w);
    w.cam_y = -540;
    w.h.y = -400 * FX;
    w.h.vy = 0;
    w.h.x = 50 * FX;
    put(&w, P_LEAF, 34, -380)->spring = 1 + 10;
    int sprung = 0, low = 0;
    for (int f = 0; f < 90; f++) {
        world_step(&w, 0);
        if (w.events & EV_SPRING) { sprung = 1; low = w.h.y / FX; }
        if (sprung && w.h.y / FX < low) low = w.h.y / FX;
    }
    CHECK(sprung && -380 - low >= 250, "a spring launches (%d) %d px", sprung, -380 - low);
    /* a pest: stomped from above, deadly from the side, popped by a seed */
    world_init(&w, 5, 0);
    empty(&w);
    w.cam_y = -540;
    w.h.y = -400 * FX;
    w.h.vy = 2 * FX;
    w.h.x = 80 * FX;
    w.e[0] = (pest){80 * FX, -385 * FX, 80, -385, 0, 0, 1000, E_BEE, 1, 0, 0};
    int st = 0;
    for (int f = 0; f < 20 && !st; f++) { world_step(&w, 0); if (w.events & EV_STOMP) st = 1; }
    CHECK(st && w.h.vy < 0 && !w.h.knocked, "a pest stomped from above: the lemur bounces off it");
    world_init(&w, 5, 0);
    empty(&w);
    w.cam_y = -540;
    w.h.y = -400 * FX;
    w.h.vy = -2 * FX;
    w.h.x = 60 * FX;
    w.e[0] = (pest){80 * FX, -412 * FX, 80, -412, 0, 0, 1000, E_BEE, 1, 0, 0};
    for (int f = 0; f < 20; f++) world_step(&w, IN_RIGHT);
    CHECK(w.h.knocked, "a pest met from the side knocks the lemur down");
    world_init(&w, 5, 0);
    empty(&w);
    w.cam_y = -540;
    w.h.y = -400 * FX;
    w.h.vy = 0;
    w.h.x = 60 * FX;
    w.h.shield_t = 100;
    w.e[0] = (pest){60 * FX, -410 * FX, 60, -410, 0, 0, 1000, E_BEE, 1, 0, 0};
    int popped = 0;
    for (int f = 0; f < 4; f++) { world_step(&w, 0); if (w.events & EV_SHIELD_POP) popped = 1; }
    CHECK(popped && !w.h.knocked && !w.h.shield_t, "the shield takes a pest's touch");
    world_init(&w, 5, 0);
    empty(&w);
    w.cam_y = -540;
    w.h.y = -400 * FX;
    w.h.vy = 0;
    w.h.x = 60 * FX;
    w.e[0] = (pest){60 * FX, -520 * FX, 60, -520, 0, 0, 1000, E_BEE, 1, 0, 0};
    int pop = 0, coins = w.coins;
    for (int f = 0; f < 30 && !pop; f++) { world_step(&w, f == 1 ? IN_SHOOT : 0); if (w.events & EV_POP) pop = 1; }
    CHECK(pop && w.coins == coins + PEST_COINS, "a seed pops a pest above (+%d coins)", PEST_COINS);
    /* the propeller */
    world_init(&w, 5, 0);
    empty(&w);
    w.cam_y = -540;
    w.h.y = -400 * FX;
    w.h.vy = 0;
    w.h.x = 50 * FX;
    put(&w, P_LEAF, 34, -380)->item = IT_PROP;
    int y0 = -380;
    for (int f = 0; f < PROP_FRAMES + 60; f++) world_step(&w, 0);
    CHECK(y0 - w.top_y > 700, "the propeller flies the lemur %d px up", y0 - w.top_y);
    /* the fall out ends the run */
    world_init(&w, 5, 0);
    empty(&w);
    w.cam_y = -2100;
    w.h.y = -1900 * FX;
    w.h.vy = 0;
    int fell = 0;
    for (int f = 0; f < 200; f++) { world_step(&w, 0); if (w.events & EV_FALL) fell = 1; }
    CHECK(fell && w.over, "falling out of the stage ends the run");
    if (!fails) OK("the rules: the bounce (80 px), the wrap, a dry leaf, a spring, a stomp, a pest's touch, the shield, "
                   "a seed, the propeller, the fall out");
}

static void trace(uint32_t seed, int god)
{
    static world w;
    world_init(&w, seed, 0);
    w.god = (uint8_t)god;
    static char lines[600][160];
    int n = 0;
    while (!w.over && w.t < 60 * 60 * 20) {
        int in = bot_input(&w);
        world_step(&w, in);
        snprintf(lines[n % 600], 160, "t=%d x=%d feet=%d vx=%d vy=%d in=%d cam=%d m=%d ev=%x", w.t, w.h.x / FX, w.h.y / FX,
                 w.h.vx, w.h.vy, in, w.cam_y, world_height_m(&w), w.events);
        n++;
    }
    for (int i = n > 300 ? n - 300 : 0; i < n; i++) puts(lines[i % 600]);
    for (int i = 0; i < MAX_PLAT; i++)
        if (w.p[i].used && w.p[i].y / FX > w.cam_y - 100 && w.p[i].y / FX < w.cam_y + 400)
            printf("plat %d: x %d y %d type %d state %d vx %d [%d %d] path %d\n", i, w.p[i].x / FX, w.p[i].y / FX,
                   w.p[i].type, w.p[i].state, w.p[i].vx, w.p[i].x0, w.p[i].x1, w.p[i].path);
}

int main(int argc, char **argv)
{
    if (argc > 2 && !strcmp(argv[1], "trace")) { trace((uint32_t)atoi(argv[2]), argc > 3 ? atoi(argv[3]) : 1); return 0; }
    int seeds = argc > 1 ? atoi(argv[1]) : 1000;
    test_rules();
    test_determinism();
    test_path(seeds);
    test_bot(seeds / 10 > 20 ? seeds / 10 : 20);
    printf(fails ? "test_world: %d FAILED\n" : "test_world: all passed\n", fails);
    return fails ? 1 : 0;
}
