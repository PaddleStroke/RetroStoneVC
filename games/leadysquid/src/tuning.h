/*
 * Leady Squid: THE tuning table. Every gameplay number is here.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
 *
 * Values are first written in the reference's units (the original flap game as
 * measured and cloned, see DESIGN.md "Balance sources"): pixels of its 288x512
 * screen (a 404-px play height above the ground), per frame at 60 Hz. They are
 * then converted to our world: 320x240, 60 Hz, a 192-px water column.
 *   - time is kept as is (60 Hz on both sides);
 *   - vertical distances are scaled by the play height, K_V = 192 / 404;
 *   - horizontal distances by K_H = 1/2, so that obstacles sit on the 8-px BG
 *     grid and the scroll is exactly 1 px per frame (the time between obstacles,
 *     1.2 s, is unchanged).
 * Positions and speeds are 16.16 fixed point (Q16): 65536 = 1 px (per frame).
 * The float expressions below are folded by the compiler into integer
 * constants: no floating point runs in the game.
 */
#ifndef LS_TUNING_H
#define LS_TUNING_H

#define Q16_ONE 65536
#define Q16(px) ((int32_t)((px) * 65536.0 + ((px) < 0 ? -0.5 : 0.5)))

/* ---- the two worlds ------------------------------------------------------ */
#define REF_PLAY_H   404.0      /* reference: 512 * 0.79, the sky above the ground (FlapPyBird BASEY) */
#define PLAY_H       192        /* ours: water surface at y = 0, seabed at y = 192 */
#define K_V          (PLAY_H / REF_PLAY_H)   /* 0.475: vertical scale */
#define K_H          0.5                     /* horizontal scale (see above) */
#define SURFACE_Y    0          /* the ceiling: the squid's hitbox cannot rise above it */
#define SEABED_Y     PLAY_H     /* touching it (hitbox bottom) is a death */

/* ---- the squid's vertical motion (reference values at 60 Hz) ------------- */
/* FlapPyBird: +1 px/frame^2 at 30 fps = 0.25 at 60 Hz; floppybird: 0.25 at 60 Hz */
#define REF_GRAVITY      0.25
/* a flap SETS the vertical speed (it does not add): FlapPyBird -9 px/frame at
 * 30 fps = -4.5 at 60 Hz (floppybird -4.6). Noschese 2014: the post-tap speed
 * is always the same, whatever the speed before. */
#define REF_FLAP_VY      -4.5
/* maximum fall speed: FlapPyBird 10 px/frame at 30 fps = 5 at 60 Hz */
#define REF_MAX_FALL     5.0

#define GRAVITY    Q16(REF_GRAVITY * K_V)    /* 0.1188 px/frame^2 */
#define FLAP_VY    Q16(REF_FLAP_VY * K_V)    /* -2.139 px/frame */
#define MAX_FALL   Q16(REF_MAX_FALL * K_V)   /* 2.376 px/frame */

/* ---- tilt (degrees in Q8: 256 = 1 degree; + = nose up) --------------------
 * FlapPyBird: a flap sets the rotation to +45, it is SHOWN capped at +20,
 * then it drops 3 degrees per 30-fps frame (1.5 at 60 Hz) down to -90: the
 * squid points up while it rises and dives nose-down as it falls. */
#define TILT_ONE        256
#define TILT_FLAP       (45 * TILT_ONE)
#define TILT_SHOW_MAX   (20 * TILT_ONE)
#define TILT_RATE       (3 * TILT_ONE / 2)   /* per frame */
#define TILT_MIN        (-90 * TILT_ONE)
/* the six pre-drawn angles (frame 0..5) and the hitbox of each: the body only
 * (mantle and head; not the tentacles or the weights). The reference bird is
 * 34x24 px: 16 x 11.4 once scaled. The box turns with the body. */
#define TILT_FRAMES 6
#define TILT_ANGLES  {20, 0, -22, -45, -67, -90}
#define HIT_W_TABLE  {16, 16, 15, 14, 13, 12}
#define HIT_H_TABLE  {12, 12, 13, 14, 15, 16}

/* ---- the scroll and the obstacles --------------------------------------- */
#define REF_SCROLL       2.0     /* FlapPyBird 4 px/frame at 30 fps (floppybird 2.22 at 60 Hz) */
#define REF_SPACING      144.0   /* FlapPyBird: a new pipe every half screen width = 1.2 s */
#define REF_OBST_W       52.0    /* the pipe sprite's width (both clones) */
#define REF_GAP          100.0   /* FlapPyBird PIPEGAPSIZE (floppybird 90, newer FlapPyBird 120) */

#define SCROLL      Q16(REF_SCROLL * K_H)    /* 1 px/frame */
#define SPACING     ((int)(REF_SPACING * K_H))   /* 72 px (a multiple of 8) */
#define OBST_W      24                        /* 26 scaled, rounded to the 8-px grid */
#define GAP         48                        /* 100 * K_V = 47.5 */
/* gap top: FlapPyBird randrange(0, int(0.6 * H - GAP)) + int(0.2 * H) */
#define GAP_TOP_MIN   ((int)(0.2 * PLAY_H))                 /* 38 */
#define GAP_TOP_RANGE ((int)(0.6 * PLAY_H - GAP))           /* 67: top in [38, 104] */
#define FIRST_OBST_X  320       /* world x of the first obstacle: just off-screen when the run starts */
#define THEME_BAND    10        /* obstacles per theme: kelp, coral, masts, chains, then again */
#define THEMES        4
#define CAP_H         16        /* the cap sprite at the gap edge (covers the 8-px step of the tiles) */

/* ---- the squid on screen ------------------------------------------------- */
#define SQUID_X        96       /* centre of the hitbox, screen x (the reference: 0.2 of the width) */
#define SQUID_START_Y  ((PLAY_H / 2) - 8)   /* centre at the start of a run */
#define P2_OFFSET_X    32       /* player 2 swims this far behind player 1 */
/* get-ready bob: FlapPyBird +-8 px, 1 px per 30-fps frame (a 1.07-s period) */
#define BOB_AMPL       4        /* 8 * K_V */
#define BOB_PERIOD     64       /* frames */

/* ---- death -------------------------------------------------------------- */
/* FlapPyBird crash: gravity 2 px/frame^2 at 30 fps, max 15, rotation -7/frame */
#define DEATH_GRAVITY  Q16(0.5 * K_V)
#define DEATH_MAX_FALL Q16(7.5 * K_V)
#define DEATH_TILT_RATE (7 * TILT_ONE / 2)
#define DEATH_HIT_FREEZE 8      /* frames of hit-stop on an obstacle before the sinking */
#define PANEL_DELAY    30       /* frames between landing on the seabed and the panel */
#define RETRY_LOCK     36       /* frames the panel ignores the button (no accidental retry) */

/* ---- score and medals --------------------------------------------------- */
#define MEDAL_BRONZE 10
#define MEDAL_SILVER 20
#define MEDAL_GOLD   30
#define MEDAL_PEARL  40

/* ---- effects (cosmetic, no gameplay effect) ------------------------------ */
#define INK_LIFE     32
#define BUBBLE_MAX   24
#define DEPTH_MAX    40         /* the water darkens with the score up to this many points */

#endif
