/*
 * Pancake Tower: unit tests of the rules (tower.c) against the tuning table: the slide, the cut maths, the
 * perfect window, the regrow, the speed steps, syrup, toppings, the score, the journey's events, 2 players,
 * determinism. No video or sound.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
 */
#include "pt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))

static double px(int32_t q) { return q / 65536.0; }

/* wait (no press) until the slider slides, then drop it with its left edge at x; run until it has landed */
static void settle(tower *tw)
{
    for (int i = 0; i < 400 && tw->state != TS_SLIDE && tw->state != TS_MISSED; i++) tower_step(tw, 0);
}

static int drop_at(tower *tw, int x)
{
    settle(tw);
    if (tw->state != TS_SLIDE) return 0;
    tw->sx = (int32_t)x << 16;
    tower_step(tw, 1);
    int ev = tw->events;
    for (int i = 0; i < 100 && (tw->state == TS_DROP || tw->state == TS_SLIP); i++) {
        tower_step(tw, 0);
        ev |= tw->events;
    }
    return ev;
}

static int top_x(const tower *tw) { return tower_top(tw)->x; }

/* drop off px from the top pancake, compensating a syrup slip as a player would */
static int drop_rel(tower *tw, int off)
{
    settle(tw);
    if (tw->syrup) off -= (tw->vx > 0 ? 1 : -1) * tw->g.slip;
    return drop_at(tw, tower_top(tw)->x + off);
}
static int top_w(const tower *tw) { return tower_top(tw)->w; }

static void test_constants(void)
{
    printf("constants (the reference converted)\n");
    CHECK(SPAWN_DIST == 107, "spawn %d (10 units of 96/9 px)", SPAWN_DIST);
    CHECK(TRAVERSE_FRAMES == 75, "traverse %d frames (1.25 s)", TRAVERSE_FRAMES);
    CHECK(NEAR(px(SLIDE_SPEED0), 2.853, 0.001), "speed %.4f px/frame", px(SLIDE_SPEED0));
    CHECK(NEAR(SPAWN_DIST * 2 / px(SLIDE_SPEED0), 75, 0.05), "a traverse in 75 frames");
    CHECK(PERFECT_PERMILLE == 75, "perfect 7.5 %%");
    CHECK(tower_tolerance(&GEOM_1P, 96) == 4, "tolerance at 96 px: %d", tower_tolerance(&GEOM_1P, 96));
    CHECK(tower_tolerance(&GEOM_1P, 40) == 3, "tolerance at 40 px: %d", tower_tolerance(&GEOM_1P, 40));
    CHECK(tower_tolerance(&GEOM_1P, 12) == 3, "tolerance at 12 px: %d (the floor)", tower_tolerance(&GEOM_1P, 12));
    CHECK(REGROW_PX == 16 && REGROW_CHAIN == 8, "regrow %d px from %d perfects", REGROW_PX, REGROW_CHAIN);
    CHECK(SPAWN_DIST_2P == 71 && W0_2P == 64, "2P: %d px, +-%d", W0_2P, SPAWN_DIST_2P);
    CHECK(NEAR(SPAWN_DIST_2P * 2 / px(SLIDE_SPEED0_2P), 75, 0.05), "2P: the same 75-frame traverse");
    CHECK(tower_tolerance(&GEOM_2P, 64) == 3, "2P tolerance %d", tower_tolerance(&GEOM_2P, 64));
    CHECK(REGROW_PX_2P % 2 == 0 && REGROW_PX % 2 == 0, "regrow split evenly on both sides");
}

static void test_slide(void)
{
    printf("the slide (ping-pong, sides)\n");
    tower tw;
    tower_init(&tw, &GEOM_1P);
    CHECK(tw.nlayers == 1 && top_w(&tw) == 96 && top_x(&tw) == -48, "the first pancake is on the plate");
    CHECK(tw.state == TS_SLIDE && tw.vx > 0, "the first slider comes from the left, moving right");
    int c0 = tower_slider_x(&tw) + tw.sw / 2;
    CHECK(c0 == -SPAWN_DIST, "it starts %d px left of the centre (%d)", SPAWN_DIST, c0);
    int turn = -1, maxc = -1000;
    for (int f = 1; f <= 200; f++) {
        int32_t vx = tw.vx;
        tower_step(&tw, 0);
        int c = tower_slider_x(&tw) + tw.sw / 2;
        if (c > maxc) maxc = c;
        if (turn < 0 && vx > 0 && tw.vx < 0) turn = f;
        CHECK(c >= -SPAWN_DIST && c <= SPAWN_DIST, "frame %d: centre %d inside +-%d", f, c, SPAWN_DIST);
    }
    CHECK(turn >= 75 && turn <= 76, "turns at the right end after %d frames", turn);
    CHECK(maxc == SPAWN_DIST || maxc == SPAWN_DIST - 1, "reaches the end (%d)", maxc);
    /* back at the start after two traverses (150 frames), within a pixel */
    tower_init(&tw, &GEOM_1P);
    for (int f = 0; f < 150; f++) tower_step(&tw, 0);
    int c = tower_slider_x(&tw) + tw.sw / 2;
    CHECK(absi(c - c0) <= 1, "after 150 frames it is back at %d (%d)", c0, c);
    CHECK(tower_side(0) == -1 && tower_side(1) == 1 && tower_side(2) == -1, "sides alternate");
    tower_init(&tw, &GEOM_1P);
    drop_at(&tw, -48);
    settle(&tw);
    CHECK(tw.vx < 0 && tower_slider_x(&tw) + tw.sw / 2 == SPAWN_DIST, "the 2nd pancake comes from the right");
    /* the press freezes the slider where it was drawn */
    tower_init(&tw, &GEOM_1P);
    for (int f = 0; f < 20; f++) tower_step(&tw, 0);
    int shown = tower_slider_x(&tw);
    tower_step(&tw, 1);
    CHECK(tw.state == TS_DROP && tw.drop_x == shown, "drops at the x shown (%d, %d)", tw.drop_x, shown);
    CHECK(tower_slider_y(&tw) == tower_top_y(&tw) + HOVER, "starts at the hover height");
    tower_step(&tw, 0);
    CHECK(tower_slider_y(&tw) < tower_top_y(&tw) + HOVER, "falls");
    tower_step(&tw, 0);
    tower_step(&tw, 0);
    tower_step(&tw, 0);
    CHECK(tw.state != TS_DROP, "lands after %d frames", DROP_FRAMES);
    /* no press: the slider goes on forever */
    tower_init(&tw, &GEOM_1P);
    for (int f = 0; f < 20000; f++) tower_step(&tw, 0);
    CHECK(tw.state == TS_SLIDE && tw.pancakes == 0, "no press: it slides forever");
}

static void test_cut(void)
{
    printf("the cut maths (every offset)\n");
    const int bx = -40, bw = 80;
    for (int off = -bw - 3; off <= bw + 3; off++) {
        int kx, kw;
        cut_piece p;
        int r = cut_resolve(&GEOM_1P, bx + off, bw, bx, bw, &kx, &kw, &p);
        int tol = tower_tolerance(&GEOM_1P, bw);
        if (absi(off) >= bw) {
            CHECK(r == 0 && p.w == bw && p.x == bx + off, "off %d: a miss, the whole pancake falls", off);
        } else if (absi(off) <= tol) {
            CHECK(r == 2 && kx == bx && kw == bw && p.w == 0, "off %d: perfect, snapped", off);
        } else {
            CHECK(r == 1 && kw == bw - absi(off), "off %d: kept %d (%d)", off, bw - absi(off), kw);
            CHECK(kx == (off > 0 ? bx + off : bx), "off %d: kept from %d", off, kx);
            CHECK(p.w == absi(off) && kw + p.w == bw, "off %d: the piece %d + kept = the width", off, p.w);
            CHECK(p.side == (off > 0 ? 1 : -1) && (off > 0 ? p.x == bx + bw : p.x + p.w == bx),
                  "off %d: the piece hangs on the %s", off, off > 0 ? "right" : "left");
        }
    }
    /* the window's edges, at several widths */
    for (int w = 8; w <= 96; w += 4) {
        int t = tower_tolerance(&GEOM_1P, w), kx, kw;
        cut_piece p;
        CHECK(cut_resolve(&GEOM_1P, t, w, 0, w, &kx, &kw, &p) == 2 &&
              cut_resolve(&GEOM_1P, -t, w, 0, w, &kx, &kw, &p) == 2, "w %d: +-%d is perfect", w, t);
        if (t + 1 < w)
            CHECK(cut_resolve(&GEOM_1P, t + 1, w, 0, w, &kx, &kw, &p) == 1 &&
                  cut_resolve(&GEOM_1P, -t - 1, w, 0, w, &kx, &kw, &p) == 1, "w %d: +-%d is a cut", w, t + 1);
    }
    /* in the game: a cut narrows the next pancake; a miss ends the tower */
    tower tw;
    tower_init(&tw, &GEOM_1P);
    int ev = drop_at(&tw, -48 + 10);
    CHECK((ev & EV_CUT) && top_w(&tw) == 86 && top_x(&tw) == -38, "cut 10 px: %d wide at %d", top_w(&tw), top_x(&tw));
    CHECK(tw.cut.w == 10 && tw.cut.side == 1 && tw.cut.x == 48, "the 10-px piece falls on the right");
    settle(&tw);
    CHECK(tw.sw == 86, "the next slider is 86 px wide");
    ev = drop_at(&tw, -38 - 86);
    CHECK((ev & EV_MISS) && tw.state == TS_MISSED && tw.pancakes == 1, "a complete miss: game over");
    CHECK(tw.cut.w == 86 && tw.cut.side == -1, "the missed pancake falls whole, on the left");
    for (int f = 0; f < 100; f++) tower_step(&tw, 1);
    CHECK(tw.state == TS_MISSED && tw.pancakes == 1, "nothing happens after a miss");
}

static void test_perfect_regrow(void)
{
    printf("perfects, chains and the regrow\n");
    tower tw;
    tower_init(&tw, &GEOM_1P);
    drop_at(&tw, -48 + 30);                                 /* 66 wide */
    CHECK(top_w(&tw) == 66, "cut to 66");
    int w = 66;
    for (int k = 1; k <= 14; k++) {
        int x = top_x(&tw);
        int ev = drop_rel(&tw, k & 1 ? 3 : -2);          /* within the 4-px window */
        CHECK(ev & EV_PERFECT, "perfect %d", k);
        CHECK(tw.chain == k, "chain %d (%d)", k, tw.chain);
        if (k >= REGROW_CHAIN && w < W0) {
            int lo = x - REGROW_PX / 2 < -W0 / 2 ? -W0 / 2 : x - REGROW_PX / 2;
            int hi = x + w + REGROW_PX / 2 > W0 / 2 ? W0 / 2 : x + w + REGROW_PX / 2;
            int nw = hi - lo;
            CHECK((ev & EV_REGROW) && top_w(&tw) == nw, "perfect %d grows %d -> %d (%d)", k, w, nw, top_w(&tw));
            CHECK(top_x(&tw) == lo, "grows on both sides, within the first footprint (%d)", top_x(&tw));
            CHECK(top_x(&tw) >= -W0 / 2 && top_x(&tw) + top_w(&tw) <= W0 / 2, "inside [-48, 48]");
            w = nw;
        } else {
            CHECK(!(ev & EV_REGROW) && top_w(&tw) == w, "perfect %d keeps %d (%d)", k, w, top_w(&tw));
        }
    }
    CHECK(w == W0 && top_w(&tw) == W0, "back to the full width, never more (%d)", top_w(&tw));
    int ev = drop_rel(&tw, 9);
    CHECK((ev & EV_CUT) && tw.chain == 0, "a cut breaks the chain");
    ev = drop_rel(&tw, 0);
    CHECK((ev & EV_PERFECT) && tw.chain == 1 && !(ev & EV_REGROW), "a new chain starts at 1");
}

static void test_speed(void)
{
    printf("the pace\n");
    int32_t s0 = tower_speed(&GEOM_1P, 0);
    CHECK(s0 == SLIDE_SPEED0 && tower_speed(&GEOM_1P, 14) == s0, "constant for the first 15");
    CHECK(tower_speed(&GEOM_1P, 15) == (int32_t)((int64_t)s0 * 108 / 100), "+8 %% at 15");
    CHECK(tower_speed(&GEOM_1P, 74) == (int32_t)((int64_t)s0 * 132 / 100), "+32 %% at 74");
    CHECK(tower_speed(&GEOM_1P, 75) == (int32_t)((int64_t)s0 * 140 / 100), "+40 %% at 75");
    CHECK(tower_speed(&GEOM_1P, 500) == tower_speed(&GEOM_1P, 75), "then constant");
    for (int n = 1; n < 200; n++) CHECK(tower_speed(&GEOM_1P, n) >= tower_speed(&GEOM_1P, n - 1), "never slower (%d)", n);
    /* in the game: the slider of pancake 15 moves 10 % faster */
    tower tw;
    tower_init(&tw, &GEOM_1P);
    int sp = 0;
    for (int k = 0; k < 15; k++) sp |= drop_at(&tw, top_x(&tw));
    settle(&tw);
    CHECK((sp & EV_SPEEDUP) && absi(tw.vx) == tower_speed(&GEOM_1P, 15), "the 16th slider is faster");
}

static void test_syrup_toppings(void)
{
    printf("syrup and toppings\n");
    CHECK(tower_topping_kind(10) == LK_STRAWBERRY && tower_topping_kind(20) == LK_BLUEBERRY &&
          tower_topping_kind(30) == LK_BANANA && tower_topping_kind(40) == LK_CHOCOLATE &&
          tower_topping_kind(50) == LK_CREAM && tower_topping_kind(60) == LK_STRAWBERRY, "the rotation");
    CHECK(!tower_topping_kind(0) && !tower_topping_kind(11) && !tower_topping_kind(5), "only every 10th");
    CHECK(tower_syrup_due(5) && tower_syrup_due(15) && !tower_syrup_due(10) && !tower_syrup_due(0), "syrup at 5, 15...");
    CHECK(slip_offset(SYRUP_SLIP, 0) == 0 && slip_offset(SYRUP_SLIP, SLIP_FRAMES) == SYRUP_SLIP, "slip 0 .. %d", SYRUP_SLIP);
    for (int k = 1; k <= SLIP_FRAMES; k++)
        CHECK(slip_offset(SYRUP_SLIP, k) >= slip_offset(SYRUP_SLIP, k - 1), "the slip never goes back (%d)", k);
    CHECK(slip_offset(SYRUP_SLIP, SLIP_FRAMES / 2) > SYRUP_SLIP / 2, "it eases out");

    tower tw;
    tower_init(&tw, &GEOM_1P);
    for (int k = 0; k < 4; k++) drop_at(&tw, top_x(&tw));
    int ev = drop_at(&tw, top_x(&tw));                      /* the 5th */
    CHECK(tw.pancakes == 5 && tw.state == TS_POUR, "after the 5th: the syrup is poured");
    int t = 0;
    while (tw.state == TS_POUR) { tower_step(&tw, 0); t++; }
    CHECK(t == POUR_FRAMES && tw.syrup && (tower_top(&tw)->flags & LF_SYRUP), "the top is syrupy after %d frames", t);
    /* the 6th comes from the right (moving left): dropped exactly above, it slides 6 px left and is cut */
    CHECK(tw.vx < 0, "the 6th moves left");
    int x = top_x(&tw);
    tw.sx = (int32_t)x << 16;
    tower_step(&tw, 1);
    ev = 0;
    int frames = 0;
    while (tw.state == TS_DROP || tw.state == TS_SLIP) { tower_step(&tw, 0); ev |= tw.events; frames++; }
    CHECK(ev & EV_SLIP, "it slips");
    CHECK(frames == DROP_FRAMES + SLIP_FRAMES, "drop + slip = %d frames (%d)", DROP_FRAMES + SLIP_FRAMES, frames);
    CHECK((ev & EV_CUT) && top_w(&tw) == 96 - SYRUP_SLIP && top_x(&tw) == x, "cut by the slip: %d wide", top_w(&tw));
    CHECK(!tw.syrup, "the syrup is used up");
    /* compensated: syrup again (splash), dropped 6 px early: perfect after the slide */
    tower_splash(&tw);
    settle(&tw);
    int dir = tw.vx > 0 ? 1 : -1;
    x = top_x(&tw);
    ev = drop_at(&tw, x - dir * SYRUP_SLIP);
    CHECK((ev & EV_SLIP) && (ev & EV_PERFECT), "dropped %d px early: the slide makes it perfect", SYRUP_SLIP);

    /* toppings: after the 10th pancake a strawberry layer of the same width, then the 11th is cut against it */
    tower_init(&tw, &GEOM_1P);
    for (int k = 0; k < 9; k++) drop_at(&tw, top_x(&tw) + (k == 2 ? 5 : 0));
    settle(&tw);
    ev = drop_at(&tw, top_x(&tw));
    CHECK(tw.pancakes == 10 && tw.state == TS_TOPPING && (ev & EV_TOPPING), "after the 10th: a topping falls");
    int w = top_w(&tw), nl = tw.nlayers;
    t = 0;
    ev = 0;
    while (tw.state == TS_TOPPING) { tower_step(&tw, 0); ev |= tw.events; t++; }
    CHECK(t == TOPPING_FRAMES && (ev & EV_TOPPING_LAND), "it lands after %d frames", t);
    CHECK(tw.nlayers == nl + 1 && tower_top(&tw)->kind == LK_STRAWBERRY && top_w(&tw) == w, "a strawberry layer");
    CHECK(tw.pancakes == 10, "not counted as a pancake");
    ev = drop_at(&tw, top_x(&tw) + 7);
    CHECK((ev & EV_CUT) && top_w(&tw) == w - 7 && tower_top(&tw)->kind == LK_PANCAKE, "the 11th cut against it");
}

static void test_score_journey(void)
{
    printf("the score and the journey\n");
    tower tw;
    tower_init(&tw, &GEOM_1P);
    int expect = 0, ceil_at = -1, roof_at = -1;
    for (int k = 1; k <= 30; k++) {
        int ev = drop_rel(&tw, k % 7 == 0 ? 5 : 0);
        if (ev & EV_CEILING) ceil_at = tw.nlayers;
        if (ev & EV_ROOF) roof_at = tw.nlayers;
        for (int i = 0; i < 100 && tw.state != TS_SLIDE; i++) {
            tower_step(&tw, 0);
            if (tw.events & EV_CEILING) ceil_at = tw.nlayers;
            if (tw.events & EV_ROOF) roof_at = tw.nlayers;
        }
        int perfect = k % 7 != 0;
        expect += 1 + (perfect ? (tw.chain >= BONUS_CHAIN_AT ? BONUS_CHAIN : BONUS_PERFECT) : 0);
        CHECK(tw.score == expect, "pancake %d: score %d (%d)", k, expect, tw.score);
        CHECK(tw.pancakes == k, "height %d", k);
    }
    CHECK(ceil_at == CEILING_Y / ROW_H + 1, "the ceiling crash with the 13th layer (%d)", ceil_at);
    CHECK(roof_at == ROOF_Y / ROW_H + 1, "the roof with the 22nd layer (%d)", roof_at);
    CHECK(segment_of(0) == SEG_KITCHEN && segment_of(191) == SEG_KITCHEN && segment_of(192) == SEG_SKY &&
          segment_of(450) == SEG_CLOUDS && segment_of(800) == SEG_STRATO && segment_of(5000) == SEG_SPACE, "segments");
    CHECK(medal_of(24) == 0 && medal_of(25) == 1 && medal_of(50) == 2 && medal_of(75) == 3 && medal_of(100) == 4,
          "medals at 25/50/75/100");
}

static void run_script(match *m, uint32_t seed, int frames)
{
    match_init(m, 2);
    uint32_t s = seed;
    for (int f = 0; f < frames; f++) {
        s = s * 1103515245u + 12345u;
        int press[2] = {((s >> 16) & 63) == 0, ((s >> 22) & 63) == 1};
        match_step(m, press);
    }
}

static void test_match(void)
{
    printf("two players and determinism\n");
    match a, b;
    run_script(&a, 7, 5000);
    run_script(&b, 7, 5000);
    CHECK(!memcmp(&a, &b, sizeof a), "the same presses give the same match");
    run_script(&b, 8, 5000);
    CHECK(memcmp(&a, &b, sizeof a) != 0, "(other presses, another match)");
    /* the same timings: two towers pressed at the same frames stay identical */
    match m;
    match_init(&m, 2);
    for (int f = 0; f < 3000; f++) {
        int p = (f % 41) == 17;
        int press[2] = {p, p};
        match_step(&m, press);
        tower t0 = m.tw[0], t1 = m.tw[1];
        t0.events = t1.events = 0;
        if (memcmp(&t0, &t1, sizeof t0)) { CHECK(0, "the towers differ at frame %d", f); break; }
    }
    CHECK(1, "same presses, same towers");
    /* the syrup splash: every 3rd perfect of a chain */
    match_init(&m, 2);
    tower *t0 = &m.tw[0], *t1 = &m.tw[1];
    int splashed = 0;
    for (int k = 0; k < 3; k++) {
        for (int i = 0; i < 400 && t0->state != TS_SLIDE; i++) { int pr[2] = {0, 0}; match_step(&m, pr); }
        t0->sx = (int32_t)top_x(t0) << 16;
        int pr[2] = {1, 0};
        match_step(&m, pr);
        for (int i = 0; i < 20; i++) { int z[2] = {0, 0}; match_step(&m, z); splashed |= t1->events & EV_SPLASHED; }
    }
    CHECK(t0->chain == 3 && splashed && t1->syrup && (tower_top(t1)->flags & LF_SYRUP), "3 perfects splash the rival");
    CHECK(t0->g.w0 == 64 && t1->sw == 64, "2 players: 64-px pancakes");
    /* the winner */
    match_init(&m, 2);
    m.tw[0].pancakes = 12; m.tw[1].pancakes = 9;
    CHECK(match_winner(&m) == 0, "the taller wins");
    m.tw[1].pancakes = 12; m.tw[1].score = 20; m.tw[0].score = 15;
    CHECK(match_winner(&m) == 1, "same height: the score");
    m.tw[1].score = 15;
    CHECK(match_winner(&m) == -1, "a draw");
}

int main(void)
{
    test_constants();
    test_slide();
    test_cut();
    test_perfect_regrow();
    test_speed();
    test_syrup_toppings();
    test_score_journey();
    test_match();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails != 0;
}
