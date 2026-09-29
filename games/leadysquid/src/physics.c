/*
 * Leady Squid: the physics of one squid and one obstacle (pure functions, no
 * drawing or sound). Every number comes from tuning.h.
 * All rights reserved, 8BCraft.
 */
#include "ls.h"

static const int tilt_angles[TILT_FRAMES] = TILT_ANGLES;
static const int hit_w[TILT_FRAMES] = HIT_W_TABLE;
static const int hit_h[TILT_FRAMES] = HIT_H_TABLE;

void squid_reset(squid *s, int x)
{
    s->state = SQ_READY;
    s->t = 0;
    s->y = SQUID_START_Y * Q16_ONE;
    s->vy = 0;
    s->tilt = 0;
    s->x = x;
    s->score = 0;
    s->next_ob = 0;
    s->flap_t = 1000;
    s->flaps = 0;
}

/* a flap SETS the speed (reference: the same post-tap speed whatever the speed before) */
void squid_flap(squid *s)
{
    s->vy = FLAP_VY;
    s->tilt = TILT_FLAP;
    s->flap_t = 0;
    s->flaps++;
}

static void ceiling(squid *s)
{
    int32_t top = (SURFACE_Y + hit_h[squid_tilt_frame(s->tilt)] / 2) * Q16_ONE;
    if (s->y < top) {
        s->y = top;                 /* bumps into the water surface */
        if (s->vy < 0) s->vy = 0;
    }
}

/* One frame, in the reference's order: the tilt drops, gravity (not on the flap
 * frame), the speed cap, then the move. */
void squid_swim_step(squid *s, int flap)
{
    if (flap) {
        squid_flap(s);
    } else {
        s->vy = min32(s->vy + GRAVITY, MAX_FALL);
        s->tilt = max32(s->tilt - TILT_RATE, TILT_MIN);
    }
    s->y += s->vy;
    ceiling(s);
    s->flap_t++;
}

void squid_sink_step(squid *s)
{
    s->vy = min32(s->vy + DEATH_GRAVITY, DEATH_MAX_FALL);
    s->tilt = max32(s->tilt - DEATH_TILT_RATE, TILT_MIN);
    s->y += s->vy;
    s->flap_t++;
}

int squid_tilt_shown(int32_t tilt)
{
    int32_t v = tilt > TILT_SHOW_MAX ? TILT_SHOW_MAX : tilt;
    return (int)(v >= 0 ? (v + TILT_ONE / 2) / TILT_ONE : -((-v + TILT_ONE / 2) / TILT_ONE));
}

int squid_tilt_frame(int32_t tilt)
{
    int deg = squid_tilt_shown(tilt), best = 0, bd = 1000;
    for (int i = 0; i < TILT_FRAMES; i++) {
        int d = deg - tilt_angles[i];
        if (d < 0) d = -d;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

box squid_box(const squid *s)
{
    int f = squid_tilt_frame(s->tilt);
    int cy = (int)((s->y + Q16_ONE / 2) >> 16);       /* rounded to the pixel shown */
    box b;
    b.x0 = s->x - hit_w[f] / 2;
    b.x1 = b.x0 + hit_w[f];
    b.y0 = cy - hit_h[f] / 2;
    b.y1 = b.y0 + hit_h[f];
    return b;
}

int squid_rest_y(const squid *s)
{
    (void)s;
    return SEABED_Y - hit_h[TILT_FRAMES - 1] / 2 + 2;  /* nose down, a little into the sand */
}

/* triangle wave, BOB_AMPL pixels either side, BOB_PERIOD frames */
int bob_offset(int t)
{
    int p = t % BOB_PERIOD, q = BOB_PERIOD / 4;
    int v;
    if (p < q) v = p;
    else if (p < 3 * q) v = 2 * q - p;
    else v = p - 4 * q;
    return v * BOB_AMPL / q;
}

obstacle obstacle_make(rs_rng *rng, int index)
{
    obstacle o;
    o.index = index;
    o.x = FIRST_OBST_X + index * SPACING;
    o.gap_top = GAP_TOP_MIN + rs_rng_range(rng, GAP_TOP_RANGE);
    o.theme = (index / THEME_BAND) % THEMES;
    return o;
}

int box_hits_obstacle(box b, const obstacle *o, int scroll_px)
{
    int ox = o->x - scroll_px;
    if (b.x1 <= ox || b.x0 >= ox + OBST_W) return 0;
    return b.y0 < o->gap_top || b.y1 > o->gap_top + GAP;
}

int box_hits_seabed(box b) { return b.y1 >= SEABED_Y; }

int medal_of(int score)
{
    return score >= MEDAL_PEARL ? 4 : score >= MEDAL_GOLD ? 3 : score >= MEDAL_SILVER ? 2 : score >= MEDAL_BRONZE ? 1 : 0;
}
