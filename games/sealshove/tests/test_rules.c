/*
 * Seal Shove: the rules alone (src/play.c, no video, no sound, no screen width: this program links play.c with the
 * SDK's RNG only): the integer maths, momentum and friction on the ice (top speed, sliding to a stop, turning arcs),
 * bumps (mass and speed), the belly slide and its cooldown, a clash of two slides, falling off and the knock-out
 * credit, the floe cracking (the warning, then the break, outer chunks first, a seal on a broken chunk falls), rounds
 * and the match, the bots' sanity (they keep off the edge, push others off, do not walk off on their own, finish
 * rounds), the fish powers, the touch stick and button, determinism.
 *   sealshove_rules                     the tests
 *   sealshove_rules --medals RUNS SKILL  (tuning) RUNS one-player runs of the player bot at SKILL (0 best .. 10):
 *                                       the score, matches and rounds won of each, and the medal counts
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
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
static pin in[SEALS];
static void idle(void) { memset(in, 0, sizeof in); }

/* a fight with only the seals the test wants (the others out; with 1-3 of them, P4 parked still at the bottom of the
 * floe so that the round goes on), no cracks, no fish, humans everywhere */
static void arena(world *w, int seals)
{
    world_init(w, 4, 1234, 0);
    w->phase = PH_FIGHT;
    w->phase_t = 1;
    w->crack_t = 1000000;
    w->fish.next = 30000;
    for (int i = 0; i < SEALS; i++) {
        seal *s = &w->s[i];
        s->human = 1;
        if (i >= seals) s->state = SS_OUT;
    }
    if (seals < SEALS) {
        seal *s = &w->s[SEALS - 1];
        s->state = SS_PLAY;
        s->x = FLOE_CX * FX;
        s->y = (FLOE_CY + 72) * FX;
        s->vx = s->vy = 0;
    }
}

static void put(world *w, int i, int x, int y)
{
    seal *s = &w->s[i];
    s->x = x * FX;
    s->y = y * FX;
    s->vx = s->vy = 0;
    s->state = SS_PLAY;
    s->dash_cd = s->stun = s->knock = s->brake = 0;
}

static void steps(world *w, int n)
{
    for (int k = 0; k < n; k++) world_step(w, in);
}

static int speed(const seal *s) { return isqrt64((int64_t)s->vx * s->vx + (int64_t)s->vy * s->vy); }

static void test_maths(void)
{
    CHECK(isin(0) == 0 && isin(64) == FX && isin(128) == 0 && isin(192) == -FX, "sine");
    CHECK(icos(0) == FX && icos(128) == -FX, "cosine");
    CHECK(iatan2(0, 1) == 0 && iatan2(1, 0) == 64 && iatan2(0, -1) == 128 && iatan2(-1, 0) == 192, "atan2 axes");
    CHECK(iatan2(5, 5) == 32 && iatan2(-5, -5) == 160, "atan2 diagonals (%d %d)", iatan2(5, 5), iatan2(-5, -5));
    int err = 0;
    for (int a = 0; a < 256; a++) {
        int b = iatan2(isin(a), icos(a)), d = ((b - a + 128) & 255) - 128;
        if (d < 0) d = -d;
        if (d > err) err = d;
    }
    CHECK(err <= 1, "atan2 inverts the sine table (error %d)", err);
    CHECK(dir8(1, 0) == 0 && dir8(0, 1) == 2 && dir8(-1, 0) == 4 && dir8(0, -1) == 6 && dir8(1, 1) == 1, "8 facings");
    CHECK(isqrt64(1000000) == 1000 && isqrt64(99) == 9, "isqrt");
}

static void test_momentum(void)
{
    arena(&W, 1);
    put(&W, 0, 74, 136);
    idle();
    in[0].dx = 256;
    steps(&W, 10);
    int v10 = speed(&W.s[0]);
    steps(&W, 70);
    int top = speed(&W.s[0]);
    CHECK(v10 < top * 2 / 3, "momentum: it gets going slowly (%d after 10 frames, %d top)", v10, top);
    CHECK(top > FX * 125 / 100 && top < FX * 15 / 10, "top waddle speed about 1.4 px a frame (%d/4096)", top);
    CHECK(W.s[0].face == 0, "it faces where it waddles");
    /* let go: it slides on and stops */
    arena(&W, 1);
    put(&W, 0, 74, 136);
    in[0].dx = 256;
    steps(&W, 80);
    int x0 = PX(W.s[0].x);
    idle();
    steps(&W, 120);
    int slide = PX(W.s[0].x) - x0;
    CHECK(slide > 14 && slide < 32, "let go at top speed: it slides %d px and stops", slide);
    CHECK(W.s[0].vx == 0, "...to a full stop");
    /* turning: from right to up, it keeps drifting right in an arc */
    arena(&W, 1);
    put(&W, 0, 90, 170);
    in[0].dx = 256;
    steps(&W, 80);
    x0 = PX(W.s[0].x);
    in[0].dx = 0;
    in[0].dy = -256;
    steps(&W, 20);
    int drift = PX(W.s[0].x) - x0, up = 170 - PX(W.s[0].y);
    CHECK(drift > 10 && up > 4, "turning up: an arc (still %d px to the right, %d up)", drift, up);
}

static int slide_push;      /* a plain slide's shove (test_bumps), against the mega slide's */

static void test_bumps(void)
{
    /* a walking seal bumps a standing one: it goes, the walker nearly stops */
    arena(&W, 2);
    put(&W, 0, 100, 136);
    put(&W, 1, 150, 136);
    idle();
    in[0].dx = 256;
    int bumped = 0;
    for (int k = 0; k < 80 && !bumped; k++) {
        world_step(&W, in);
        bumped = W.bumps;
    }
    CHECK(bumped, "they bump");
    int v1 = W.s[1].vx, v0 = W.s[0].vx;
    CHECK(v1 > FX && v0 < v1 / 3, "the bumped seal goes (%d), the bumper nearly stops (%d)", v1, v0);
    CHECK(W.s[1].hit_by == 0, "the pusher is remembered");
    idle();
    steps(&W, 60);
    int pushed = PX(W.s[1].x) - 150;
    CHECK(pushed > 10 && pushed < 50, "a walking shove: %d px", pushed);
    /* a heavy seal (the big belly) hardly moves */
    arena(&W, 2);
    put(&W, 0, 100, 136);
    put(&W, 1, 150, 136);
    W.s[1].power = PW_HEAVY;
    W.s[1].power_t = 1000;
    in[0].dx = 256;
    bumped = 0;
    for (int k = 0; k < 80 && !bumped; k++) {
        world_step(&W, in);
        bumped = W.bumps;
    }
    CHECK(bumped && W.s[1].vx < v1 * 2 / 3 && W.s[0].vx < 0, "a heavy seal moves less (%d), the bumper bounces back (%d)",
          W.s[1].vx, W.s[0].vx);
    /* a slide hits hard */
    arena(&W, 2);
    put(&W, 0, 100, 136);
    put(&W, 1, 140, 136);
    idle();
    in[0].dx = 256;
    in[0].dash = 1;
    world_step(&W, in);
    idle();
    CHECK(W.s[0].state == SS_DASH && W.dashes == 1, "A: a belly slide");
    bumped = 0;
    for (int k = 0; k < 30 && !bumped; k++) {
        world_step(&W, in);
        bumped = W.bumps;
    }
    CHECK(bumped && (W.events[0] & EV_SMACK), "the slide smacks into it");
    CHECK(W.s[0].state == SS_PLAY, "the slide ends on the hit");
    steps(&W, 90);
    pushed = PX(W.s[1].x) - 140;
    CHECK(pushed > 50, "a slide's shove: %d px", pushed);
    slide_push = pushed;
    /* holding the d-pad against it shortens it */
    arena(&W, 2);
    put(&W, 0, 100, 136);
    put(&W, 1, 140, 136);
    in[0].dx = 256;
    in[0].dash = 1;
    world_step(&W, in);
    idle();
    in[1].dx = -256;
    steps(&W, 100);
    int resisted = PX(W.s[1].x) - 140;
    CHECK(resisted < pushed, "resisting with the d-pad: %d px instead of %d", resisted, pushed);
    /* a clash: two slides head-on bounce back, both dizzy */
    arena(&W, 2);
    put(&W, 0, 120, 136);
    put(&W, 1, 200, 136);
    idle();
    in[0].dx = 256;
    in[0].dash = 1;
    in[1].dx = -256;
    in[1].dash = 1;
    world_step(&W, in);
    idle();
    int cl = 0;
    for (int k = 0; k < 20 && !cl; k++) {
        world_step(&W, in);
        cl = W.clashes;
    }
    CHECK(cl == 1 && (W.events[0] & EV_CLASH) && (W.events[1] & EV_CLASH), "a clash");
    CHECK(W.s[0].vx < -FX && W.s[1].vx > FX, "both bounce back (%d, %d)", W.s[0].vx, W.s[1].vx);
    CHECK(W.s[0].stun > 0 && W.s[1].stun > 0, "both dizzy");
    in[0].dx = 256;
    in[0].dash = 1;
    world_step(&W, in);
    CHECK(W.s[0].state != SS_DASH, "dizzy: no slide");
}

static void test_dash(void)
{
    arena(&W, 1);
    put(&W, 0, 80, 136);
    idle();
    in[0].dx = 256;
    in[0].dash = 1;
    world_step(&W, in);
    CHECK(speed(&W.s[0]) >= DASH_V * 98 / 100, "the slide starts at 4 px a frame (%d)", speed(&W.s[0]));
    idle();
    in[0].dash = 1;
    steps(&W, 5);
    CHECK(W.dashes == 1, "no second slide during the first");
    idle();
    steps(&W, 120);
    int dist = PX(W.s[0].x) - 80;
    CHECK(dist > 65 && dist < 100, "a slide goes %d px", dist);
    in[0].dash = 1;
    W.s[0].dash_cd = 5;
    world_step(&W, in);
    CHECK(W.dashes == 1, "cooldown: no slide yet");
    idle();
    steps(&W, 5);
    in[0].dash = 1;
    world_step(&W, in);
    CHECK(W.dashes == 2, "the slide comes back");
    /* the cooldown runs from the slide's end */
    arena(&W, 1);
    put(&W, 0, 80, 136);
    idle();
    in[0].dx = 256;
    in[0].dash = 1;
    world_step(&W, in);
    idle();
    int n = 0;
    while (W.s[0].state == SS_DASH && n < 100) { world_step(&W, in); n++; }
    CHECK(n >= DASH_FRAMES - 1 && n <= DASH_FRAMES + 1, "it slides %d frames", n);
    int k = 0, ev = 0;
    while (!ev && k < 200) { world_step(&W, in); ev = W.events[0] & EV_READY; k++; }
    CHECK(k >= DASH_COOLDOWN - 1 && k <= DASH_COOLDOWN + 1, "ready again %d frames later", k);
}

static void test_falling(void)
{
    /* pushed off the edge: falls, splashes, out; the pusher's knock-out */
    arena(&W, 2);
    put(&W, 0, 165, 136);
    put(&W, 1, 200, 136);
    idle();
    in[0].dx = 256;
    in[0].dash = 1;
    world_step(&W, in);
    idle();
    int fell = 0, k = 0;
    for (; k < 200 && !fell; k++) {
        world_step(&W, in);
        fell = W.events[1] & EV_FALL;
    }
    CHECK(fell && W.s[1].state == SS_FALL, "pushed off the floe: it falls");
    CHECK(W.s[0].kos == 1 && W.s[0].score == PTS_KO && (W.events[0] & EV_KO), "the pusher's knock-out (+%d)", PTS_KO);
    int splash = 0;
    for (k = 0; k < FALL_FRAMES + 5; k++) {
        world_step(&W, in);
        splash |= W.events[1] & EV_SPLASH;
    }
    CHECK(splash && W.s[1].state == SS_OUT, "a splash, then out");
    /* walking off alone: nobody's knock-out */
    arena(&W, 2);
    put(&W, 0, 100, 136);
    put(&W, 1, 230, 136);
    idle();
    in[1].dx = 256;
    for (k = 0; k < 200 && W.s[1].state == SS_PLAY; k++) world_step(&W, in);
    CHECK(W.s[1].state == SS_FALL && W.s[0].kos == 0, "walking off alone: no knock-out for anyone");
    /* the credit runs out */
    arena(&W, 2);
    put(&W, 0, 100, 136);
    put(&W, 1, 230, 136);
    W.s[1].hit_by = 0;
    W.s[1].hit_t = KO_CREDIT + 1;
    in[1].dx = 256;
    for (k = 0; k < 200 && W.s[1].state == SS_PLAY; k++) world_step(&W, in);
    CHECK(W.s[0].kos == 0, "pushed long ago: no credit");
    /* a double knock-out */
    arena(&W, 3);
    put(&W, 0, 200, 126);
    put(&W, 1, 236, 120);
    put(&W, 2, 236, 150);
    W.s[1].hit_by = W.s[2].hit_by = 0;
    W.s[1].hit_t = W.s[2].hit_t = 0;
    idle();
    in[1].dx = in[2].dx = 256;
    for (k = 0; k < 120 && (W.s[1].state == SS_PLAY || W.s[2].state == SS_PLAY); k++) world_step(&W, in);
    CHECK(W.s[0].kos == 2 && W.s[0].score == 2 * PTS_KO + PTS_DOUBLE && W.msg == MSG_DOUBLE, "DOUBLE KNOCK-OUT! (+%d)",
          PTS_DOUBLE);
}

static int ice_cells(const world *w)
{
    int n = 0;
    for (int i = 0; i < GRID * GRID; i++) n += w->cell[i] && w->ch[w->cell[i] - 1].state != CH_GONE;
    return n;
}

static void test_floe(void)
{
    world_init(&W, 1, 77, 0);
    CHECK(W.nchunks == MAX_CHUNKS, "the floe: %d chunks", W.nchunks);
    int empty = 0, area = ice_cells(&W);
    for (int c = 0; c < W.nchunks; c++) empty += W.ch[c].ncells == 0;
    CHECK(!empty, "no empty chunk");
    CHECK(area > 1600 && area < 2000, "a round floe of radius about %d: %d cells", FLOE_R, area);
    CHECK(on_ice(&W, FLOE_CX * FX, FLOE_CY * FX) && !on_ice(&W, (FLOE_CX + FLOE_R + 6) * FX, FLOE_CY * FX) &&
          !on_ice(&W, 10 * FX, 10 * FX), "the centre is ice, past the rim is water");
    int widest = 0;
    for (int c = 0; c < W.core; c++) {
        int ww = W.ch[c].x1 - W.ch[c].x0, hh = W.ch[c].y1 - W.ch[c].y0;
        if (ww > widest) widest = ww;
        if (hh > widest) widest = hh;
    }
    CHECK(widest <= 56, "every chunk fits a 56-px sprite when it drifts off (%d)", widest);
    world_init(&W2, 1, 78, 0);
    CHECK(memcmp(W.cell, W2.cell, sizeof W.cell) != 0, "every round's floe is its own shape");
    /* the cracks: none during the intro and the first seconds, then warnings, then breaks */
    idle();
    W.s[0].human = 1;
    for (int i = 0; i < SEALS; i++) W.s[i].human = 1;     /* the seals stand still */
    W.fish.next = 30000;
    steps(&W, INTRO_FRAMES);
    CHECK(W.phase == PH_FIGHT, "GO! after the intro");
    int warn_at = -1, c = -1;
    for (int k = 0; k < FIRST_CRACK + 5 && warn_at < 0; k++) {
        world_step(&W, in);
        if (W.gev & GEV_CRACK) warn_at = k;
    }
    CHECK(warn_at >= FIRST_CRACK - 3 && warn_at <= FIRST_CRACK + 1, "the first crack %d frames after GO!", warn_at);
    for (int i = 0; i < W.nchunks; i++)
        if (W.ch[i].state == CH_WARN) c = i;
    CHECK(c >= 0 && W.ch[c].edge, "it is a chunk on the edge");
    int before = ice_cells(&W);
    CHECK(c >= 0 && on_ice(&W, W.ch[c].cx * FX, W.ch[c].cy * FX), "a cracking chunk still holds");
    int broke = -1;
    for (int k = 0; k < CRACK_WARN + 2 && broke < 0; k++) {
        world_step(&W, in);
        if (c >= 0 && W.ch[c].state == CH_GONE) broke = k + 1;
    }
    CHECK(broke >= CRACK_WARN - 1 && broke <= CRACK_WARN + 1, "it breaks off %d frames after the crack", broke);
    CHECK(ice_cells(&W) < before && c >= 0 && !on_ice(&W, W.ch[c].cx * FX, W.ch[c].cy * FX), "the floe is smaller");
    /* a seal standing on a chunk that breaks falls */
    world_init(&W, 4, 91, 0);
    W.phase = PH_FIGHT;
    W.fish.next = 30000;
    W.crack_t = 1;
    world_step(&W, in);
    c = -1;
    for (int i = 0; i < W.nchunks; i++)
        if (W.ch[i].state == CH_WARN) c = i;
    put(&W, 3, W.ch[c].cx, W.ch[c].cy);
    W.crack_t = 100000;
    int fell = 0;
    for (int k = 0; k < CRACK_WARN + 3; k++) {
        world_step(&W, in);
        fell |= W.events[3] & EV_FALL;
    }
    CHECK(fell, "a seal on a chunk that breaks off falls with it");
    /* the whole floe goes, outer rings first, the core last */
    world_init(&W, 4, 5, 0);
    W.phase = PH_FIGHT;
    W.fish.next = 30000;
    for (int i = 0; i < SEALS; i++) W.s[i].state = SS_OUT;
    int order_ok = 1, core_t = -1, n = 0;
    for (int k = 0; k < 20000 && core_t < 0; k++) {
        W.s[0].state = SS_PLAY;                         /* keeps the round going: P1 hovers over the core */
        W.s[0].x = FLOE_CX * FX;
        W.s[0].y = FLOE_CY * FX;
        W.s[1].state = SS_PLAY;
        W.s[1].x = (FLOE_CX + 4) * FX;
        W.s[1].y = FLOE_CY * FX;
        world_step(&W, in);
        if (W.gev & GEV_BREAK) {
            n++;
            for (int i = 0; i < W.core; i++)
                if (W.ch[i].state == CH_GONE && W.ch[i].t == 0 && !W.ch[i].edge && !(W.ch[i].adj & (W.gone & ~(1u << i))))
                    order_ok = 0;
        }
        if (W.ch[W.core].state == CH_GONE) core_t = k;
    }
    CHECK(order_ok, "a chunk breaks only from the edge inwards");
    CHECK(core_t > 0 && n == MAX_CHUNKS, "the core goes last (%d breaks, after %d s)", n, core_t / 60);
}

static void test_rounds(void)
{
    world_init(&W, 1, 3, 0);
    for (int i = 0; i < SEALS; i++) W.s[i].human = 1;
    idle();
    steps(&W, INTRO_FRAMES);
    W.fish.next = 30000;
    /* P2, P3 and P4 fall: P1 wins the round */
    for (int i = 1; i < SEALS; i++) {
        W.s[i].x = 300 * FX;
        W.s[i].y = 20 * FX;
    }
    steps(&W, 2);
    CHECK(W.phase == PH_FIGHT, "the round waits for the splashes");
    steps(&W, FALL_SPLASH + 2);
    CHECK(W.phase == PH_END && W.round_winner == 0 && W.s[0].wins == 1, "the last seal on the floe wins the round");
    CHECK(W.s[0].score == PTS_ROUND, "+%d", PTS_ROUND);
    steps(&W, END_FRAMES);
    CHECK(W.phase == PH_INTRO && W.round == 2 && W.s[1].state == SS_PLAY, "round 2: everyone back on a new floe");
    /* the last two fall together: nobody wins */
    steps(&W, INTRO_FRAMES);
    W.s[2].state = W.s[3].state = SS_OUT;
    W.s[0].x = W.s[1].x = 5 * FX;
    W.s[1].y = 200 * FX;
    steps(&W, FALL_SPLASH + 2);
    CHECK(W.phase == PH_END && W.round_winner < 0, "both last seals fall: a draw round");
    /* first to 3: the match */
    world_init(&W, 1, 4, 0);
    for (int i = 0; i < SEALS; i++) W.s[i].human = 1;
    for (int r = 0; r < WIN_ROUNDS; r++) {
        steps(&W, INTRO_FRAMES + 1);
        for (int i = 0; i < SEALS; i++)
            if (i != 2) W.s[i].x = 2 * FX;
        steps(&W, FALL_SPLASH + 2);
        CHECK(W.round_winner == 2, "round %d to P3", r + 1);
        steps(&W, END_FRAMES);
    }
    CHECK(W.match_winner == 2 && W.phase == PH_MATCH && W.s[2].matches == 1, "3 rounds: P3 wins the match");
    CHECK(W.s[2].score == 3 * PTS_ROUND + PTS_MATCH, "score %d", W.s[2].score);
    steps(&W, MATCH_FRAMES);
    CHECK(W.match_over, "then the match is over");
    world_next_match(&W);
    CHECK(W.match_no == 2 && W.level == 1 && W.s[2].wins == 0 && W.phase == PH_INTRO, "the next match: smarter bots");
}

/* a round of bots only: how it ends */
static int bot_round(world *w, int *len)
{
    int start = w->round;
    for (int k = 0; k < 60 * 120; k++) {
        world_step(w, in);
        if (w->phase == PH_END) {
            *len = w->phase_t;
            int win = w->round_winner;
            while (w->round == start && !w->match_over) world_step(w, in);
            return win;
        }
    }
    *len = -1;
    return -2;
}

static void test_bots(void)
{
    /* a bot near the edge goes back to the middle */
    arena(&W, 2);
    W.level = 3;
    W.s[0].state = SS_OUT;
    W.s[1].human = 0;
    put(&W, 1, FLOE_CX + 78, FLOE_CY);
    idle();
    steps(&W, 60);
    CHECK(W.s[1].state == SS_PLAY && PX(W.s[1].x) < FLOE_CX + 70, "a bot by the water walks back in (x %d)", PX(W.s[1].x));
    /* bots push a still player off, sooner or later */
    int ko = 0, total = 0;
    for (int run = 0; run < 6; run++) {
        world_init(&W, 1, 100 + run, 2);
        idle();
        int k = 0;
        while (k < 60 * 60 && W.s[0].state != SS_FALL && W.s[0].state != SS_OUT) { world_step(&W, in); k++; }
        if (W.s[0].state != SS_PLAY) { ko++; total += k; }
    }
    CHECK(ko >= 5, "level-2 bots push a still player off (%d of 6, after %d s on average)", ko, ko ? total / ko / 60 : 0);
    /* rounds of bots alone: they finish, they mostly do not walk off by themselves, nobody always wins */
    int wins[SEALS] = {0}, draws = 0, stuck = 0, selfs = 0, lens = 0, rounds = 0;
    for (int lv = 0; lv < BOT_LEVELS; lv += 2)
        for (int run = 0; run < 8; run++) {
            world_init(&W, 1, 500 + run * 7 + lv, lv);
            W.s[0].human = 0;
            idle();
            int len, wn = bot_round(&W, &len);
            if (wn == -2) { stuck++; continue; }
            rounds++;
            lens += len < 0 ? 0 : 1;
            if (wn >= 0) wins[wn]++;
            else draws++;
            selfs += W.falls - (W.s[0].kos + W.s[1].kos + W.s[2].kos + W.s[3].kos);
        }
    CHECK(!stuck, "every bot round ends (%d stuck)", stuck);
    CHECK(draws * 4 <= rounds, "few draws (%d of %d)", draws, rounds);
    CHECK(selfs <= rounds * 3 / 2, "bots seldom walk off on their own (%d of %d rounds' falls)", selfs, rounds * 3);
    int top = 0;
    for (int i = 0; i < SEALS; i++)
        if (wins[i] > top) top = wins[i];
    CHECK(top <= rounds / 2, "no seat always wins (%d %d %d %d)", wins[0], wins[1], wins[2], wins[3]);
    /* the player bot beats level-0 bots more often than not */
    int won = 0;
    for (int run = 0; run < 10; run++) {
        world_init(&W, 1, 900 + run, 0);
        bot_par bp;
        bot_par_player(0, &bp);
        while (!W.match_over) {
            pin b[SEALS];
            memset(b, 0, sizeof b);
            bot_think(&W, 0, &bp, &b[0]);
            world_step(&W, b);
        }
        won += W.match_winner == 0;
    }
    CHECK(won >= 5, "the player bot wins %d of 10 matches against level-0 bots", won);
    /* determinism */
    world_init(&W, 1, 42, 3);
    world_init(&W2, 1, 42, 3);
    W2.floe_gen = W.floe_gen;
    W.s[0].human = W2.s[0].human = 0;
    idle();
    for (int k = 0; k < 6000; k++) {
        world_step(&W, in);
        world_step(&W2, in);
    }
    CHECK(!memcmp(&W, &W2, sizeof W), "the same seed, the same matches");
}

static void test_fish(void)
{
    arena(&W, 2);
    put(&W, 0, 120, 136);
    put(&W, 1, 200, 136);
    W.fish.on = 1;
    W.fish.kind = PW_HEAVY;
    W.fish.x = 140;
    W.fish.y = 136;
    idle();
    in[0].dx = 256;
    int got = 0;
    for (int k = 0; k < 60 && !got; k++) {
        world_step(&W, in);
        got = W.events[0] & EV_POWER;
    }
    CHECK(got && W.s[0].power == PW_HEAVY && !W.fish.on && W.msg == MSG_HEAVY, "the blue fish: BIG BELLY");
    CHECK(seal_mass(&W.s[0]) == SEAL_MASS * HEAVY_MASS_PCT / 100 && seal_radius(&W.s[0]) == HEAVY_R, "heavier, bigger");
    idle();
    steps(&W, HEAVY_FRAMES);
    CHECK(W.s[0].power == PW_NONE, "for %d s", HEAVY_FRAMES / 60);
    /* the golden fish: a mega slide */
    arena(&W, 2);
    put(&W, 0, 100, 136);
    put(&W, 1, 140, 136);
    W.s[0].power = PW_MEGA;
    W.s[0].power_t = MEGA_FRAMES;
    in[0].dx = 256;
    in[0].dash = 1;
    world_step(&W, in);
    CHECK(speed(&W.s[0]) > DASH_V * 125 / 100 && W.s[0].power == PW_NONE, "MEGA SLIDE: faster, used up");
    idle();
    steps(&W, 90);
    int mega = PX(W.s[1].x) - 140;
    CHECK(mega > slide_push + 25, "it shoves %d px (a plain slide %d)", mega, slide_push);
    /* fish come and go by themselves */
    world_init(&W, 1, 8, 2);
    W.s[0].human = 0;
    idle();
    int seen = 0;
    for (int k = 0; k < 60 * 40; k++) {
        world_step(&W, in);
        seen += (W.gev & GEV_FISH) != 0;
        if (W.fish.on) {
            CHECK(on_ice(&W, W.fish.x * FX, W.fish.y * FX), "a fish lies on the ice");
            if (!on_ice(&W, W.fish.x * FX, W.fish.y * FX)) break;
        }
    }
    CHECK(seen >= 1, "fish come (%d in 40 s)", seen);
}

static void test_touch(void)
{
    int16_t dx, dy;
    touch_stick(100, 100, 100 + STICK_R, 100, &dx, &dy);
    CHECK(dx == 256 && dy == 0, "the stick at its full length: full speed right");
    touch_stick(100, 100, 100 + STICK_R * 3, 100, &dx, &dy);
    CHECK(dx == 256, "further: still full speed");
    touch_stick(100, 100, 101, 101, &dx, &dy);
    CHECK(dx == 0 && dy == 0, "a tiny drag: nothing");
    touch_stick(100, 100, 100, 100 + STICK_R / 2, &dx, &dy);
    CHECK(dx == 0 && dy > 90 && dy < 170, "half the drag: about half speed down (%d)", dy);
    touch_stick(100, 100, 80, 80, &dx, &dy);
    CHECK(dx < -150 && dy < -150, "a diagonal (%d, %d)", dx, dy);
    tbutton b;
    touch_button(320, &b);
    CHECK(b.x + b.r < 320 && b.y + b.r < 240 && b.x > 240, "the DASH button at the bottom right (%d, %d)", b.x, b.y);
    CHECK(touch_on_button(&b, b.x, b.y) && touch_on_button(&b, b.x + b.r, b.y) && !touch_on_button(&b, b.x - 60, b.y),
          "its area");
    touch_button(373, &b);
    CHECK(b.x == 373 - BTN_DX, "widescreen: it follows the right edge");
    CHECK(touch_stick_zone(0, 320, 20, 120) && touch_stick_zone(0, 320, 170, 200) && !touch_stick_zone(0, 320, 250, 60),
          "the stick: the left part of the screen");
    CHECK(touch_stick_zone(-53, 427, -50, 100) && !touch_stick_zone(-53, 427, 200, 100), "at 427 too");
}

/* tuning: one-player runs of the player bot at a skill, until it loses a match */
static void medals(int runs, int skill)
{
    static const int th[4] = MEDAL_SCORES;
    int tiers[5] = {0}, sum = 0, msum = 0;
    for (int r = 0; r < runs; r++) {
        world_init(&W, 1, 7000 + r * 31, 0);
        bot_par bp;
        bot_par_player(skill, &bp);
        int frames = 0, rw = 0;
        for (;;) {
            pin b[SEALS];
            memset(b, 0, sizeof b);
            bot_think(&W, 0, &bp, &b[0]);
            world_step(&W, b);
            frames++;
            rw += (W.events[0] & EV_WIN_ROUND) != 0;
            if (!W.match_over) continue;
            if (W.match_winner != 0 || W.match_no >= 12) break;
            world_next_match(&W);
        }
        int sc = W.s[0].score, m = 0;
        while (m < 4 && sc >= th[m]) m++;
        tiers[m]++;
        sum += sc;
        msum += W.s[0].matches;
        printf("  run %2d: score %5d, matches won %d, rounds won %2d, %d min\n", r + 1, sc, W.s[0].matches, rw,
               frames / 3600);
    }
    printf("skill %d: mean score %d, mean matches won %.1f; none %d, bronze %d, silver %d, gold %d, pearl %d\n", skill,
           sum / runs, (double)msum / runs, tiers[0], tiers[1], tiers[2], tiers[3], tiers[4]);
}

/* tuning: rounds of 4 bots at a level: how long, how much of the floe breaks, how many falls are their own */
static void rounds(int level, int n)
{
    int frames = 0, breaks = 0, falls = 0, kos = 0, draws = 0, done = 0, bumps = 0, dashes = 0;
    for (int r = 0; r < n; r++) {
        world_init(&W, 1, 300 + r * 17, level);
        W.s[0].human = 0;
        idle();
        int len, wn = bot_round(&W, &len);
        if (wn == -2) continue;
        done++;
        draws += wn == -1;
        frames += W.t;
        breaks += W.breaks;
        falls += W.falls;
        bumps += W.bumps;
        dashes += W.dashes;
        for (int i = 0; i < SEALS; i++) kos += W.s[i].kos;
    }
    if (!done) return;
    printf("level %d: %d rounds, %.1f s each (with the intro), %.1f breaks, %.1f falls (%.1f knock-outs), %d draws, "
           "%.1f bumps, %.1f slides\n", level, done, frames / 60.0 / done, (double)breaks / done, (double)falls / done,
           (double)kos / done, draws, (double)bumps / done, (double)dashes / done);
}

int main(int argc, char **argv)
{
    if (argc >= 4 && !strcmp(argv[1], "--medals")) {
        medals(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "--rounds")) {
        for (int lv = 0; lv < BOT_LEVELS; lv++) rounds(lv, 40);
        return 0;
    }
    test_maths();
    test_momentum();
    test_bumps();
    test_dash();
    test_falling();
    test_floe();
    test_rounds();
    test_bots();
    test_fish();
    test_touch();
    printf("sealshove rules: %d checks, %d failed\n", checks, fails);
    if (!fails) printf("  ok   the rules\n");
    return fails ? 1 : 0;
}
