/*
 * Mouse Espresso: THE tuning table. Every gameplay number of the flight, the course and its director is here
 * (DESIGN.md "Obstacles", "Patterns", "The director", "Measured difficulty").
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/mouseespresso/LICENSE.
 *
 * Units: world pixels, 60 frames per second; speeds and the mouse's y / vy are 16.16 fixed point (Q16). The course
 * is generated with integers only (no floating point: the same course on every CPU).
 */
#ifndef ME_TUNING_H
#define ME_TUNING_H

/* ---- the room -------------------------------------------------------------------------------------------------- */
#define COUNTER_Y      232      /* the counter top (the floor of the room) */
#define FLIGHT_TOP     16       /* the mouse's centre: highest */
#define FLIGHT_BOTTOM  218      /* lowest (standing on the counter) */
#define HERO_RADIUS_X  8
#define HERO_RADIUS_Y  7

/* ---- the flight (unchanged from the first version: the feel the owner played) ---------------------------------- */
#define THRUST         18022    /* Q16 px/frame^2 up while A is held (0.275) */
#define GRAVITY        11796    /* Q16 px/frame^2 down while released (0.18) */
#define VY_UP_MAX      229376   /* Q16: 3.5 px/frame up */
#define VY_DOWN_MAX    249036   /* Q16: 3.8 px/frame down */

/* ---- the travel speed: 2.3 px/frame at the start, rising smoothly to 3.5 px/frame at SPEED_RAMP_PX ------------ */
#define SPEED_START    150733   /* Q16: 2.3 px/frame (138 px/s) */
#define SPEED_MAX      229376   /* Q16: 3.5 px/frame (210 px/s) */
#define SPEED_RAMP_PX  22000    /* travelled px to reach SPEED_MAX (a score of about 1850) */

/* ---- the course ------------------------------------------------------------------------------------------------ */
#define HAZARD_FIRST   360      /* world x of the first obstacle (the mice start at x 48..132): the same at every
                                   screen width */
#define GEN_AHEAD      1100     /* the director keeps the course generated this far past the camera (wider than the
                                   widest screen; it never looks at the screen width) */
#define KEEP_BEHIND    320      /* items this far behind the camera are dropped from the ring */
#define COURSE_N       256      /* the item ring (the validator checks the peak occupancy) */
#define PATTERN_N      32       /* the pattern ring */

/* ---- the difficulty target (k: 0..256) -------------------------------------------------------------------------- *
 * k(d) = K_START + (K_END - K_START) * d / (d + K_HALF_PX): a saturating ramp with the distance d (px), plus waves:
 * WAVE_PX long, k rising by WAVE_AMP through a wave, then a breather; the first EASY_PX stay at k <= K_EASY. */
#define K_START        18
#define K_END          236
#define K_HALF_PX      4000
#define WAVE_PX        2200
#define WAVE_AMP       52
#define EASY_PX        1300
#define K_EASY         34
#define K_GEOM_MAX     200      /* the patterns' geometry stops tightening here (the speed still rises) */

/* unlocks (px of distance): the new hazards arrive one by one, all of them before a score of 500 */
#define UNLOCK_JETS     1500
#define UNLOCK_CLUSTER  2300
#define UNLOCK_ZIGZAG   3000
#define UNLOCK_LADLES   3700
#define UNLOCK_BOUNCERS 4600
#define UNLOCK_DRIPS    5400
#define UNLOCK_MIXED    6500    /* dynamic hazards inside the static patterns */

/* ---- spacing: the frames to move dy px with the flight's physics (starting while drifting the wrong way at
 * MOVE_AWAY), plus MOVE_SETTLE, times the slack (course.c move_frames) ------------------------------------------- */
#define MOVE_AWAY      98304    /* Q16: 1.5 px/frame */
#define MOVE_SETTLE    8
#define SLACK_EASY     340      /* /256: k = 0 */
#define SLACK_HARD     248      /* /256: k = 256 */

/* ---- gaps (the free height between a ceiling item and a floor item, px; the mouse is 15 px tall) ------------ */
#define GAP_EASY       100
#define GAP_HARD       46
#define GAP_NARROW_EASY 70
#define GAP_NARROW_HARD 40
#define GAP_MIN        38       /* never below */

/* ---- dynamic hazards -------------------------------------------------------------------------------------------- */
#define JET_TELEGRAPH  36       /* frames of harmless puffs and a red light before a jet fires */
#define JET_RISE       6        /* frames to full height */
#define DRIP_FALL      3        /* px/frame */
#define LADLE_BOWL     7        /* radius */

/* ---- score ------------------------------------------------------------------------------------------------------- */
#define SCORE_PX       12       /* one point per 12 px travelled */
#define BISCUIT_POINTS 5

#endif
