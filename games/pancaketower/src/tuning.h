/*
 * Pancake Tower: THE tuning table. Every gameplay number is here.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
 *
 * The reference numbers are first written in the reference's units (the MIT clone open-stack-game, which
 * measures the block in "units", and the published guides; DESIGN.md "Feel sources"), then converted to our
 * pixels: the first pancake is 96 px wide for the clone's 9-unit block, so K_W = 96 / 9 px per unit; time is
 * kept as is (60 Hz). Positions and speeds are 16.16 fixed point (Q16): 65536 = 1 px (per frame). The float
 * expressions below are folded by the compiler into integer constants: no floating point runs in the game.
 */
#ifndef PT_TUNING_H
#define PT_TUNING_H

#define Q16_ONE 65536
#define Q16(px) ((int32_t)((px) * 65536.0 + ((px) < 0 ? -0.5 : 0.5)))

/* ---- the reference (open-stack-game, MIT: BlockConfig / DifficultyConfig / StackConfig assets) ---------- */
#define REF_BLOCK_UNITS    9.0      /* BlockStartingSize 9 x 1 x 9 */
#define REF_SPAWN_UNITS    10.0     /* SpawnDistanceFromStack: the block ping-pongs between +10 and -10 */
#define REF_TRAVERSE_S     1.25     /* BlockPingPongLoopTime: one traverse (+10 -> -10), linear */
#define REF_PERFECT_MARGIN 0.075    /* PerfectPlacementErrorMargin: a share of the block's size */
#define REF_REGROW_CHAIN   8        /* NoConsecutivePerfectPlacements */
#define REF_GROW_UNITS     1.5      /* BlockGrowToScale: per side, capped at the starting size */
/* the original's pace (Level Winner guide): quicker every 15-20 points, then no longer */
#define REF_SPEED_EVERY    15

/* ---- our world (1 player) ------------------------------------------------------------------------------ */
#define ROW_H        8              /* a layer (pancake or topping) is 8 px tall */
#define W0           96             /* the first pancake's width */
#define K_W          (W0 / REF_BLOCK_UNITS)                      /* 10.67 px per unit */
#define SPAWN_DIST   ((int)(REF_SPAWN_UNITS * K_W + 0.5))        /* 107 px each side of the top's centre */
#define TRAVERSE_FRAMES ((int)(REF_TRAVERSE_S * 60 + 0.5))       /* 75 frames per traverse */
#define SLIDE_SPEED0 Q16(2.0 * SPAWN_DIST / TRAVERSE_FRAMES)     /* 2.853 px/frame */
/* the perfect window: 7.5 % of the width (the clone), clamped to a few pixels */
#define PERFECT_PERMILLE ((int)(REF_PERFECT_MARGIN * 1000 + 0.5))   /* 75 */
#define PERFECT_MIN  3              /* never narrower than a frame of motion at top speed (7 px > 4.0 px/frame) */
#define PERFECT_MAX  4
/* the regrow: from the 8th perfect in a row, every perfect adds 1.5/9 of the first width (in total: our
 * side view has one axis, the clone grows two), never above the first width */
#define REGROW_CHAIN REF_REGROW_CHAIN
#define REGROW_PX    ((int)(REF_GROW_UNITS * K_W + 0.5))         /* 16 px: 8 per side */
/* the pace: +8 % every 15 pancakes, 5 steps (x1.4 from pancake 75), then constant */
#define SPEED_EVERY     REF_SPEED_EVERY
#define SPEED_STEP_PCT  8
#define SPEED_STEPS_MAX 5

/* ---- the drop (ours) -------------------------------------------------------------------------------------- */
#define HOVER        4              /* px: the slider rides this far above the top surface */
#define DROP_FRAMES  3              /* frames of the fall onto the tower */

/* ---- the twist (ours) -------------------------------------------------------------------------------------- */
#define TOPPING_EVERY   10          /* a topping after every 10th pancake */
#define TOPPINGS        5           /* strawberry, blueberry, banana, chocolate chips, whipped cream */
#define TOPPING_FRAMES  36          /* the topping's fall onto the tower (the slider waits) */
#define SYRUP_EVERY     10          /* syrup after the 5th, 15th, 25th... pancake */
#define SYRUP_OFFSET    5
#define POUR_FRAMES     40          /* the bottle pours (the slider waits) */
#define SYRUP_SLIP      6           /* px: the next pancake slides on, in its direction of travel */
#define SLIP_FRAMES     12          /* ... easing out over these frames, then it is cut */

/* ---- score and medals ------------------------------------------------------------------------------------ */
#define BONUS_PERFECT   1           /* +1 for a perfect */
#define BONUS_CHAIN_AT  5           /* +2 from the 5th perfect in a row */
#define BONUS_CHAIN     2
#define MEDAL_BRONZE    25
#define MEDAL_SILVER    50
#define MEDAL_GOLD      75
#define MEDAL_PEARL     100

/* ---- 2 players: the same game in a geometry scaled by 2/3 (a tower fits a 159-px viewport) -------------- */
#define K_2P            (2.0 / 3.0)
#define W0_2P           64
#define SPAWN_DIST_2P   ((int)(SPAWN_DIST * K_2P + 0.5))         /* 71 */
#define SLIDE_SPEED0_2P Q16(2.0 * SPAWN_DIST_2P / TRAVERSE_FRAMES)  /* 1.893 px/frame: the same times */
#define PERFECT_MAX_2P  3           /* 4 * 2/3, rounded */
#define REGROW_PX_2P    10          /* 16 * 2/3 = 10.7, even (5 per side) */
#define SYRUP_SLIP_2P   4
#define SPLASH_EVERY    3           /* every 3rd perfect of a chain splashes syrup on the rival */

/* ---- the journey (world y: 0 = the top of the plate, up; tools/make_art.py draws the house to these numbers) -------
 * The kitchen fills the screen at the start, its ceiling at the top. The camera stays below each ceiling until the
 * tower breaks through it (draw.c): what is above a ceiling is only seen once the tower is through. */
#define FLOOR_Y      (-40)          /* the kitchen floor */
#define CEILING_Y    152            /* the kitchen ceiling 152..192 (cornice, plaster, joists, the attic's boards): */
#define CEILING_TOP  192            /*   the 20th layer crashes through it */
#define ROOF_Y       288            /* the attic 192..288, then the roof in section 288..336 (boards, rafters, tiles): */
#define ROOF_TOP     336            /*   the 37th layer breaks out through it */
#define RIDGE_Y      396            /* the roof seen from above 336..396 (the chimney rises to 424), then the sky */
#define SEG_H        256            /* a scenery segment (art): the house is segments 0 and 1 */
#define SEG_BASE_Y   (-64)          /* the bottom of segment 0 */
/* the zones of the journey (the music, the tests) */
#define SEG_KITCHEN  0              /* the house, up to the roof's top */
#define SEG_SKY      1              /* ROOF_TOP .. 448 */
#define SEG_CLOUDS   2              /* 448 .. 704 */
#define SEG_STRATO   3              /* 704 .. 960 */
#define SEG_SPACE    4              /* 960 .. (repeats) */

/* ---- the screen ------------------------------------------------------------------------------------------ */
#define TOP_SCREEN_Y 120            /* the camera keeps the top of the tower here once it is that high */
#define BASE_SCREEN_Y 192           /* the plate's top at the start */
#define CAMERA_SPEED 2              /* px per frame at most */

/* ---- wobble (visual only) -------------------------------------------------------------------------------- */
#define WOBBLE_PERIOD 192           /* frames (3.2 s) */
#define WOBBLE_MAX    3             /* px at the top of a tall tower */
#define WOBBLE_FULL_H 40            /* layers for the full amplitude */
#define JIGGLE_PX     2             /* the jiggle after a landing, dying out */
#define JIGGLE_FRAMES 30

/* ---- flow ------------------------------------------------------------------------------------------------ */
#define OVER_DELAY   70             /* frames between the miss and the panel (the pancake falls) */
#define RETRY_LOCK   36             /* frames the panel ignores the buttons */
#define NARROW_PX    24             /* the chef panics under this width */

#endif
