/*
 * Beaver Rush: THE tuning table. Every gameplay number is here.
 * (c) 2026 Pierre-Louis Boyer (8BCraft). All rights reserved: games/beaverrush/LICENSE.
 *
 * The feel follows the reference (Timberman, 2014) as measured by its clones and described by reviews
 * (DESIGN.md "Feel sources"): the reference numbers are written first (REF_*), then converted. Time is
 * 60 Hz on both sides. The timer bar is an integer, BAR_FULL units = a full bar; the float expressions
 * below are folded by the compiler into integer constants: no floating point runs in the game.
 */
#ifndef BR_TUNING_H
#define BR_TUNING_H

/* ---- the trunk ---------------------------------------------------------------------------------- */
#define SEG_H          24       /* px: one segment, the beaver's height */
#define SEG_SHOWN      8        /* segments 0..7 on screen (the reference: 6; its guide: 4 chops of warning) */
#define TRUNK_N        16       /* segments kept (generated ahead) */
#define TRUNK_W        48       /* px */
#define BRANCH_LEN     40       /* px from the trunk's side */
#define DROP_FRAMES    4        /* the drop is SHOWN in 4 frames; the rules are instant */

/* ---- the timer ---------------------------------------------------------------------------------- */
/* Source 1 (Timber, MIT): a 6-s bar drained 1 s per second; a chop adds (2 / score + 0.15) s. The chop
 * rate that holds the bar at score s is then R(s) = s / (2 + 0.15 s) per second (= 20 s / (40 + 3 s)).
 * Timberman raises the DRAIN by level (every 20 chops, source 4); we keep the refill fixed and make the
 * drain of level L = refill x R(20 L + 10): the same pressure curve, in steps. */
#define REF_BAR_SECONDS  6.0
#define LEVEL_LOGS       20     /* logs per drain level (source 4) */
#define LEVEL_MID        10     /* the curve is sampled at the middle of each level */
#define BAR_FULL         (1 << 24)
#define BAR_FRAMES_L0    ((int)(REF_BAR_SECONDS * 60))                 /* 360: a full bar at level 0 */
/* R(s) in Q16, per second */
#define RATE_Q16(s)      ((int64_t)20 * (s) * 65536 / (40 + 3 * (int64_t)(s)))
/* one gnaw refills (1/6 bar per s) / R(10) = 7/120 of the bar (5.83%, 0.35 s of level-0 time) */
#define REFILL           ((int32_t)((int64_t)BAR_FULL * 7 / 120))
#define LEVEL_ENDGAME    15     /* from level 15 (300 logs): our endgame ramp, so every run ends */
#define ENDGAME_RATE_Q16 ((int64_t)65536 * 12 / 100)                  /* +0.12 gnaws/s per level */
#define LEVEL_MAX        60
#define BAR_START        BAR_FULL /* source 1 starts full */

/* ---- generation (per stage of STAGE_LOGS segments) ------------------------------------------------ */
#define START_CLEAR      4      /* the first segments are empty (source 2: 3) */
#define P_BRANCH0        40     /* % at stage 0 (source 1: left 1/5 + right 1/5) */
#define P_BRANCH_STEP    4      /* + per stage */
#define P_BRANCH_MAX     60
#define ZIG_FROM_STAGE   2      /* zig-zag runs from stage 2 */
#define P_ZIG0           10     /* % of chunks at stage 2 */
#define P_ZIG_STEP       4
#define P_ZIG_MAX        26
#define ZIG_MIN          3      /* branches in a zig-zag run */
#define ZIG_MAX          6
#define P_STACK          15     /* % of chunks: a same-side stack of 2-3 */
#define MAX_EMPTY_RUN    3
#define MAX_SAME_RUN     4
#define GOLD_SPACING     30     /* segments after a golden log before the next may appear */
#define GOLD_ODDS        24     /* then 1 in 24 empty segments */
#define GOLD_BONUS       4      /* extra points (a golden log counts 5) */
#define GOLD_REFILL      (BAR_FULL / 4)

/* ---- stages and the dam ------------------------------------------------------------------------- */
#define STAGE_LOGS       50     /* a milestone: a dam section, the next scene (source 6: 50 chops) */
#define DAM_SECTIONS     6
#define DAM_COLS         5      /* logs per row of a section */
#define DAM_ROWS         10
#define TIMES_OF_DAY     4      /* dawn, day, sunset, night */
#define SEASONS          4      /* summer, autumn, winter, spring */
#define WOODPECKER_STAGE 2

/* ---- versus ----------------------------------------------------------------------------------------- */
#define CHIP_SLOT        7      /* a stolen branch appears at segment 7 (the top of the trunk shown) */

/* ---- flow (frames) ------------------------------------------------------------------------------- */
#define HIT_STOP         10     /* the freeze on a bonk */
#define PANEL_DELAY      50     /* from the end of a run to the panel */
#define RETRY_LOCK       36     /* the panel ignores the buttons this long (0.6 s) */
#define VS_END_DELAY     70

/* ---- score and medals --------------------------------------------------------------------------- */
#define MEDAL_T1 50
#define MEDAL_T2 100
#define MEDAL_T3 200
#define MEDAL_T4 300

/* ---- cosmetic ----------------------------------------------------------------------------------- */
#define LEAN_PX          6      /* the camera leans toward the beaver's side */
#define SCENE_FADE       90     /* frames of the palette change at a milestone */

#endif
