/*
 * Pancake Tower: the test bot (--opt bot=1). It plays from the SCREEN STATE only: the sprites in OAM left by the
 * last frame (the slider's and the top pancake's left edges, the syrup bottle, a splash flying in), in its own
 * player's part of the screen, plus its memory of what it saw. It never reads the rules' state.
 *
 * Like a person it sees with a REACTION DELAY: it acts on what was on screen BOT_DELAY frames ago, extrapolated
 * (the slide is steady: it fits a line to the positions it saw, and remembers the speed of the last pass). It
 * commits to a press a little before the slider reaches the spot, and each press lands with a human-like
 * JITTER: a Gaussian spread (sigma BOT_SIGMA_Q4 / 16 frames), clamped, from its own seeded RNG (--opt seed=N).
 * It compensates the syrup's slide when it sees syrup shining on the top pancake (it knows the feel: 6 px on, 4 px in
 * 2 players), as a player learns to drop early.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
 */
#include "pt.h"
#include "assets.h"
#include <string.h>

#define BOT_DELAY    12             /* frames: what it acts on was on screen 200 ms ago */
#define BOT_COMMIT   6              /* it commits to a press this many frames ahead of the spot (+ the delay) */
#define BOT_SIGMA_Q4 13             /* the jitter's sigma: 13/16 = 0.8 frame = 13 ms */
#define BOT_CLAMP    2              /* ... clamped to +-2 frames */
#define HIST 64

typedef struct obs { int slider, sx, top, tx, rx, syrup; } obs;   /* seen: slider (x), top (x), slider - top, syrup */

typedef struct botp {
    obs hist[HIST];                 /* what was on screen, by frame */
    int f;                          /* frames observed */
    int plan;                       /* the frame (f) to press at, -1 none */
    int syrup;                      /* saw syrup poured / splashed since the last press */
    int pass_f0, pass_dir, pass_n;  /* the current pass: its first seen frame, direction, samples */
    int32_t speed_q8;               /* the remembered speed (px/frame, Q8), 0 = unknown */
    int presses;
    int cooldown;                   /* frames after a press: it waits to see its pancake land */
    rs_rng rng;
    int jitters[16];                /* the distribution of the jitters applied (-8..7 frames) */
} botp;

static botp B[MAX_PLAYERS];

void bot_reset(uint32_t seed)
{
    for (int p = 0; p < MAX_PLAYERS; p++) {
        int keep[16];
        memcpy(keep, B[p].jitters, sizeof keep);
        memset(&B[p], 0, sizeof B[p]);
        memcpy(B[p].jitters, keep, sizeof keep);
        B[p].plan = -1;
        rs_rng_seed(&B[p].rng, seed * 2 + (uint32_t)p + 1);
    }
}

static int in_spr(int tile, int id, int frames)
{
    int t0 = pt_spr[id].tile, n = (pt_spr[id].w / 8) * (pt_spr[id].h / 8);
    return tile >= t0 && tile < t0 + n * frames;
}

static obs look(int p)
{
    obs o = {0, 0, 0, 0, 0, 0};
    int first = 0, count = RS_OAM_MAX;
    draw_view_oam(p, &first, &count);
    int slider_tile = OBJ_DYN_TILE + p * 96, top_tile = slider_tile + 16;
    for (int i = first; i < first + count && i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        if (!s || !s->used || (s->flags & RS_SPR_HIDE)) continue;
        if (s->tile == slider_tile) { o.slider = 1; o.sx = s->x + 1; }   /* the box starts 1 px left of the pancake */
        if (s->tile == top_tile) { o.top = 1; o.tx = s->x + 1; }
        if (in_spr(s->tile, SPR_GLAZE, 1)) o.syrup = 1;                 /* syrup shines on the top pancake */
    }
    /* where the slider is against the top: the tower's sway and a shake move both the same */
    if (o.slider && o.top) o.rx = o.sx - o.tx;
    return o;
}

static int gauss_frames(botp *b)
{
    int32_t g = 0;
    for (int k = 0; k < 12; k++) g += rs_rng_range(&b->rng, 4096);
    g -= 6 * 4096;                                           /* ~ N(0, 1) x 4096 */
    int j = (int)((g * BOT_SIGMA_Q4 / 16 + (g >= 0 ? 2048 : -2048)) / 4096);
    j = clampi(j, -BOT_CLAMP, BOT_CLAMP);
    b->jitters[(j + 8) & 15]++;
    return j;
}

int bot_decide(int p)
{
    botp *b = &B[p];
    obs now = look(p);
    b->hist[b->f % HIST] = now;
    int f = b->f++;
    if (f < BOT_DELAY) return 0;
    const obs *d = &b->hist[(f - BOT_DELAY) % HIST];           /* what it perceives now */
    b->syrup = d->syrup;                                     /* it sees the syrup on the top (perceived late) */
    int stop = bot_stop_height();
    if (b->plan >= 0) {
        if (f >= b->plan) {
            b->plan = -1;
            b->presses++;
            b->syrup = 0;
            b->cooldown = BOT_DELAY + DROP_FRAMES;
            b->pass_n = 0;
            return 1;
        }
        return 0;
    }
    if (b->cooldown > 0) { b->cooldown--; b->pass_n = 0; return 0; }
    if (!d->slider || !d->top) { b->pass_n = 0; return 0; }
    if (stop && b->presses >= stop) {                          /* botstop: a deliberate miss, at once */
        b->plan = f + 1;
        return 0;
    }
    /* the current pass: the perceived positions since the slider appeared or turned */
    const obs *pr = &b->hist[(f - BOT_DELAY - 1) % HIST];
    int dir = pr->slider ? (d->rx > pr->rx) - (d->rx < pr->rx) : 0;
    if (pr->slider && (dir == 0 || absi(d->rx - pr->rx) > 12)) {    /* it stopped (dropped), or a new one appeared */
        b->pass_n = 0;
        return 0;
    }
    if (!pr->slider || b->pass_n == 0 || (dir && dir != b->pass_dir)) {
        b->pass_f0 = f - BOT_DELAY;
        b->pass_dir = dir;
        b->pass_n = 1;
        return 0;
    }
    if (!b->pass_dir) b->pass_dir = dir;
    b->pass_n++;
    if (b->pass_n < 4 || !b->pass_dir) return 0;
    /* fit the pass: slope (px/frame, Q8) and the position now; the remembered speed when it agrees */
    int n = b->pass_n < HIST - BOT_DELAY - 2 ? b->pass_n : HIST - BOT_DELAY - 2;
    int64_t st = 0, sx = 0, stt = 0, stx = 0;
    for (int k = 0; k < n; k++) {
        int fr = f - BOT_DELAY - k;
        const obs *o = &b->hist[fr % HIST];
        st += -k, sx += o->rx, stt += (int64_t)k * k, stx += (int64_t)-k * o->rx;
    }
    int64_t den = (int64_t)n * stt - st * st;
    int32_t v = den ? (int32_t)(((int64_t)n * stx - st * sx) * 256 / den) : 0;
    /* the speed it knows from the last passes (the same at a given height: it has the rhythm), unless this pass
     * is clearly faster or slower (the pace went up); then the position fitted with that slope */
    int32_t vf = absi(v);
    if (b->speed_q8) {
        /* a new pace: clearly faster (the steps are +8 %) on a long look; else the known speed, refined */
        int dv = absi(vf - b->speed_q8) * 100 / b->speed_q8;       /* % */
        int newpace = (n >= 12 && dv > 5) || (n >= 8 && dv > 12);
        if (newpace) {
            b->speed_q8 = vf;
        } else {
            if (n >= 12) b->speed_q8 = (b->speed_q8 * 7 + vf) / 8;     /* refined, pass after pass */
            v = b->pass_dir * b->speed_q8;
        }
    } else if (n >= 10) {
        b->speed_q8 = vf;
    } else {
        return 0;                                                 /* too little seen yet */
    }
    if (absi(v) < (pt_players() > 2 ? 120 : 300)) return 0;     /* narrow columns have slower sliders */
    int64_t x0 = (sx * 256 - (int64_t)v * st) / n;           /* Q8: the perceived position (k = 0) */
    /* where it must be: the top's left edge, minus the syrup's slide */
    int slip = pt_players() > 2 ? 2 : pt_players() == 2 ? SYRUP_SLIP_2P : SYRUP_SLIP;
    int32_t target = -(b->syrup ? b->pass_dir * slip * 256 : 0);   /* relative to the top's left edge */
    /* frames from the perceived moment until the slider is there (the frame it perceives is BOT_DELAY old) */
    int32_t ahead = (int32_t)((target - x0) * 256 / v);       /* Q8 frames */
    if (ahead < 0) return 0;                                   /* passed: wait for the way back */
    int frames = (ahead + 128) >> 8;
    int when = f - BOT_DELAY + frames;         /* hist[g] = the picture after update g-1: pressing at g freezes it */
    if (when - f > BOT_COMMIT) return 0;                       /* not yet: it commits close to the spot */
    int j = gauss_frames(b);
    if (rs_option_int("botlog", 0))
        rs_log("bot%d f%d: x0=%d v=%d target=%d tx=%d ahead=%d when=%d jitter=%d syrup=%d n=%d", p, f, (int)(x0 >> 8), v,
               target >> 8, d->tx, ahead, when, j, b->syrup, b->pass_n);
    when += j;
    if (when <= f) when = f;
    b->plan = when;
    if (b->plan == f) {
        b->plan = -1;
        b->presses++;
        b->syrup = 0;
        b->cooldown = BOT_DELAY + DROP_FRAMES;
        b->pass_n = 0;
        return 1;
    }
    return 0;
}

/* the distribution of the jitters applied so far (tests): count of -2..+2 frames */
void bot_jitter_counts(int out[5])
{
    for (int k = 0; k < 5; k++) out[k] = B[0].jitters[(k - 2 + 8) & 15] + B[1].jitters[(k - 2 + 8) & 15];
}

/* ---- save states (main.c) ---- */
void bot_state(void) { rs_state_var("bot.B", &B, sizeof B); }
