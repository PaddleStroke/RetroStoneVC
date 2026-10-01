/*
 * Duck Parade: THE tuning table. Every gameplay number is here.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 *
 * Reference values are written in the units of their source (DESIGN.md, "Balance and feel sources"): the
 * MIT Crossy Road clone (Expo-Crossy-Road) counts in tiles and tiles per frame at 60 Hz, Frogger in pixels.
 * Our world: 1 tile = 1 cell = 16 px (Frogger's step), 60 Hz. Lane positions are in Q8 (256 = 1 px) and
 * speeds in Q8 px per frame. The float expressions fold to integer constants: no floating point runs.
 */
#ifndef DP_TUNING_H
#define DP_TUNING_H

#define CELL        16          /* px: one lane (column) is a cell wide, a hop is one cell (Frogger: $10) */
#define ROWS        14          /* the play field: rows 0..13 */
#define FIELD_Y     16          /* screen y of row 0 (the top 16 lines are the sky band and the HUD) */
#define FIELD_H     (ROWS * CELL)                /* 224 */
#define SCREEN_COLS 20          /* columns on screen */
#define Q8(px)      ((int32_t)((px) * 256.0 + 0.5))
#define TILE_PX     16.0        /* the clone's tile, in our px */

/* ---- the hop (clone: two 0.1-s tweens, the in-air point at 75% of the move) ------------------------ */
#define REF_HOP_S        0.2
#define HOP_FRAMES       12                 /* REF_HOP_S * 60 (tests/test_rules.c checks it) */
#define HOP_MID          (HOP_FRAMES / 2)   /* from this frame of a hop the duck is in the lane it lands in */
#define HOP_AIR_FRAC     0.75               /* share of the move done at mid-hop */
#define HOP_HEIGHT       6                  /* px of the arc (the clone: half a tile high, 8 px; lowered for a top-down view) */
#define HOP_BUFFER       4                  /* a press in the last frames of a hop starts the next one on landing */
#define BUMP_FRAMES      8                  /* a blocked hop: lean and back */
/* squash and stretch (clone: scale y 1.2, 0.8, back to 1, 0.1 s each), in 1/16: */
#define STRETCH_16       19                 /* 1.2 */
#define SQUASH_16        13                 /* 0.8 */
#define IDLE_PERIOD      36                 /* the idle breathing: 0.3 s down, 0.3 s up */

/* ---- hitboxes (clone: the half widths summed, minus 0.1 tile) -------------------------------------- */
#define FORGIVE_PX       ((int)(0.1 * TILE_PX + 0.5))   /* 2 px */
#define DUCK_HIT_H       (CELL - 2 * FORGIVE_PX)        /* 12: Mother's box along the lane */
#define DUCKLING_HIT_H   8                              /* a duckling (10 px) minus 1 px each side */
#define RIDE_TOLERANCE   3                               /* px: the duck's centre may be this far past a platform's end */

/* ---- lanes: the loop (clone: objects wrap over 22 tiles) ------------------------------------------- */
#define LOOP_CELLS       22
#define LOOP_PX          (LOOP_CELLS * CELL)            /* 352 */
#define LOOP_TOP         (4 * CELL)                     /* loop px of row 0: 4 hidden cells above, 4 below */

/* road traffic (clone: speed 0.02..0.08 tiles/frame, 1..2 cars, 5..8 tiles apart) */
#define REF_CAR_MIN      0.02
#define REF_CAR_MAX      0.08
#define CAR_V_MIN        Q8(REF_CAR_MIN * TILE_PX)       /* 0.32 px/frame */
#define CAR_V_MAX        Q8(REF_CAR_MAX * TILE_PX)       /* 1.28 px/frame, reached at DIFF_LANES */
#define CAR_V_START_MAX  Q8(0.70)                       /* the top speed at lane 0 */
#define CARS_START_MAX   2                               /* cars per loop at lane 0: 1..2 (the clone) */
#define CARS_MAX         3                               /* at DIFF_LANES: 2..3 */
#define CAR_GAP_MIN      5                               /* cells between two cars' fronts (clone: 5..8) */
#define BIKE_V_MUL_16    20                              /* a bike: 1.25 x the lane's car speed */
#define BUS_V_MUL_16     11                              /* a bus: 0.7 x */
#define BUS_CHANCE       12                              /* % of road lanes with a bus */
#define BIKE_CHANCE      15                              /* % of road lanes that are bike lanes */
#define CAR_LEN          22                              /* px along the lane (the 24-px sprite, 1 px rounded off each end) */
#define BUS_LEN          40
#define BIKE_LEN         12
#define CAR_GAP_MAX      8                               /* cells between two cars' fronts at most (the clone: 8) */
#define BIKE_GAP_MIN     4                               /* bikes ride closer together */

/* rivers (clone: logs 0.02..0.07 tiles/frame, 2..3 per lane, 5..8 tiles apart) */
#define REF_LOG_MIN      0.02
#define REF_LOG_MAX      0.07
#define LOG_V_MIN        Q8(REF_LOG_MIN * TILE_PX)       /* 0.32 */
#define LOG_V_MAX        Q8(REF_LOG_MAX * TILE_PX)       /* 1.12, at DIFF_LANES */
#define LOG_V_START_MAX  Q8(0.60)
#define LOG_LEN_START    3                               /* cells: 3..4 at lane 0, 2..3 at DIFF_LANES */
#define PADS_CHANCE      20                              /* % of river lanes of drifting lily pads (1 cell) */
#define BOAT_CHANCE      15                              /* % of river lanes with the swan paddle boat (2 cells) */
#define LOG_WAIT_FRAMES  110                             /* the water between two platforms: a duck waits at most this long */
#define LOG_GAP_MIN_CAP  2                               /* ... but the gap may always be 2 cells */
#define LOG_GAP_MAX_CAP  5                               /* ... and never more than 5 */
#define LOG_GAP_MIN_PX   24                              /* the shortest water between two logs */

/* park paths: joggers (small groups) and a lawnmower */
#define JOG_V_MIN        Q8(0.30)
#define JOG_V_MAX        Q8(0.75)
#define MOWER_V          Q8(0.25)
#define MOWER_CHANCE     30
#define JOGGER_LEN       12
#define JOGGER_SPACING   14                              /* px between the runners of a group */
#define MOWER_LEN        20
#define MOWER_GAP_MIN    9                               /* cells between two mowers */
#define MOWER_GAP_MAX    14

/* lily ponds (clone: static pads, 2..3 per row, 2..3 apart, one in reach of the row before) */
#define POND_PADS_MIN    4
#define POND_PADS_MAX    7

/* the train (clone: 0.8 tiles/frame, 1..3 carriages, the light flashing 15 x 200 ms, 2.2 s before it shows) */
#define REF_TRAIN_V      0.8
#define TRAIN_V          Q8(REF_TRAIN_V * TILE_PX)       /* 12.8 px/frame */
#define TRAIN_WARN       120                             /* frames of light and bell before the loco shows (>= 60 tested) */
#define TRAIN_WARN_MIN   60                              /* the rule: at least 1 s */
#define TRAIN_CARS_MIN   2
#define TRAIN_CARS_MAX   4
#define TRAIN_CAR_PX     48                              /* a carriage; the locomotive too */
#define TRAIN_PERIOD_START_MIN 480                       /* frames between trains at lane 0 (8..10 s) */
#define TRAIN_PERIOD_START_MAX 600
#define TRAIN_PERIOD_MIN 300                             /* at DIFF_LANES (5..8 s; the clone: 4.6 s) */
#define TRAIN_PERIOD_MAX 480

/* ---- difficulty -------------------------------------------------------------------------------------- */
#define DIFF_LANES       300    /* d = lane / DIFF_LANES, capped at 1 */
#define DIFF_EXTRA_LANES 600    /* past DIFF_LANES speeds grow up to +20% at this lane */
#define DIFF_EXTRA_PCT   20
/* steps of the difficulty d (0..256 over DIFF_LANES) */
#define D_TWO_CARS       128    /* at least two cars per road lane from here */
#define D_TWO_MOWERS     160    /* a second lawnmower */
#define D_JOG_GROUPS     128    /* up to three groups of joggers */
#define D_JOG_TRIOS      96     /* joggers in threes */
#define D_SHORT_LOGS     128    /* logs of 2 cells appear */
#define D_SHORTER_LOGS   192    /* no more 4-cell logs */
#define TREES_START_256  128    /* the trees at lane 0: half of TREE_PCT_MAX, all of it at DIFF_LANES */
#define MEADOW_COLS      8      /* the starting meadow: columns 0..7 */
#define START_COL        6
#define START_ROW        7

/* group sizes (lanes) at lane 0 .. at DIFF_LANES */
#define ROAD_GROUP_START 2
#define ROAD_GROUP_MAX   4      /* Frogger: 5 */
#define RIVER_GROUP_START 2
#define RIVER_GROUP_MAX  3
#define RAIL_GROUP_MAX   2
#define PARK_GROUP_MAX   2
#define POND_GROUP_MAX   2
#define GRASS_RUN_MIN    1
#define GRASS_RUN_MAX    3
#define GRASS_RUN_TWO    25     /* % of grass runs one column longer */
#define GRASS_RUN_LONG   8      /* ... and one more */
#define MIXED_FROM       120    /* lanes: groups may mix families from here */
#define MIXED_PCT        35     /* ... each lane after the first takes another family this often */
/* family weights (clone: grass, road-type, water 1/3 each; road-type = railway 1/4) */
#define W_ROAD           25
#define W_RAIL           8
#define W_PARK           9
#define W_RIVER          25
#define W_POND           8
#define TREE_PCT_MAX     35     /* % of a grass column's cells with a tree (at most) */
#define GRASS_FREE_MIN   4      /* free cells kept in every grass column */

/* ---- the judge (fairness) --------------------------------------------------------------------------- */
#define JUDGE_STARTS     4      /* start times tried per group */
#define JUDGE_SPACING    97     /* frames between them */
#define JUDGE_WINDOW     240    /* frames: each start must reach the next grass run within 4 s */
#define JUDGE_TRIES      4      /* re-rolls (easier each time) before the group becomes grass */
#define STRICT_STEP      5      /* the test's strict judge: a start every 5 frames */
#define STRICT_WINDOW    300    /* ... within 5 s, from every single start cell */

/* ---- the camera and the fox (the Crossy Road eagle: ~5 s idle) --------------------------------------- */
#define CAM_ANCHOR_COL   8      /* the camera keeps Mother (co-op: the parents' mean) at this screen column */
#define CAM_EASE_256     8      /* clone: CAMERA_EASING 0.03 per frame (8/256 = 0.031) */
#define CREEP_START      Q8(0.28)  /* px/frame: 8 s of idling from the anchor to the edge */
#define CREEP_MAX        Q8(0.45)  /* at DIFF_LANES: 5 s */
#define FOX_WARN_COLS    3      /* the fox shows when Mother is this close to the left edge */
#define FOX_EDGE_PX      0      /* Mother's centre past (left edge + this) = caught */
#define FOX_POUNCE       24     /* frames of the pounce before the game-over sequence */

/* ---- the parade --------------------------------------------------------------------------------------- */
#define LINE_MAX         24     /* ducklings in one line */
#define FOLLOW_STAGGER   2      /* frames between two ducklings' hops (a ripple) */
#define CATCHUP_DELAY    18     /* frames after a gap opens before the followers close it */
#define CATCHUP_SAFE     (HOP_FRAMES + 6)   /* the cell must stay clear this long */
#define FLUTTER_SPEED    Q8(2.0)            /* a knocked-off duckling flies back to the grass */
#define KNOCK_FRAMES     20     /* its tumble before it flutters */
#define REJOIN_LOCK      30     /* frames before a returned duckling can be picked up again */
#define DUCKLING_CHANCE  28     /* % of grass columns with a lost duckling */
#define DUCKLING_PAIR    15     /* % of those with two */
#define NEST_EVERY_MIN   40     /* lanes between nest ponds */
#define NEST_EVERY_RAND  11
#define FIRST_NEST       30     /* the first nest pond (lanes) */
#define BANK_ANIM        40     /* frames of the banking (the hop-in cascade and the flash) */
#define PHOTO_FRAMES     90     /* the family photo shown */
static inline int bank_points(int n) { return n * n; }

/* ---- score and medals (house tiers: bronze, silver, gold, pearl eggs) --------------------------------- */
#define MEDAL_BRONZE     50
#define MEDAL_SILVER     100
#define MEDAL_GOLD       200
#define MEDAL_PEARL      300

/* ---- death and flow ----------------------------------------------------------------------------------- */
#define DEATH_FREEZE     10     /* hit-stop */
#define DEATH_ANIM       60     /* the death animation before the panel */
#define RETRY_LOCK       36     /* frames the panel ignores the buttons */

/* ---- co-op: 2-4 parents, one camera, one family score (DESIGN.md "Co-op") ------------------------------ */
/* The camera follows the GROUP: it eases (CAM_EASE_256) towards the mean column of the parents still in the
 * run, at CAM_ANCHOR_COL, and still creeps; the leader is never pushed off the right edge: when it is past
 * CAM_LEAD_COL the camera eases towards it much faster, and it is never past CAM_LEAD_MAX_COL. Solo: the mean is
 * the duck: the camera of one player is the same as before. The fox is per parent: a parent within
 * FOX_WARN_COLS of the left edge is warned, one whose centre crosses it is caught; the others go on. */
#define CAM_LEAD_COL      12    /* co-op: the leader is eased back to this screen column ... */
#define CAM_LEAD_EASE_256 32    /* ... an eighth of the distance a frame (4x the group's easing) */
#define CAM_LEAD_MAX_COL  16    /* ... and is never right of this one (4 columns of view ahead) */
/* The sprite budget: a line holds FAMILY_LINE / max(2, parents) ducklings: 24 for 1-2 parents (as before), 16 for
 * 3, 12 for 4: the family's lines never hold more than 48 ducklings (2 x 24, the 2-player worst case). */
#define FAMILY_LINE       48
static inline int line_cap(int players) { int n = FAMILY_LINE / (players > 2 ? players : 2); return n < LINE_MAX ? n : LINE_MAX; }

/* ---- music (a row = 8 frames: MOD speed 8 at tempo 150, one tick per frame) ---------------------------- */
#define MUSIC_ROW_FRAMES 8
#define LAYER1_AT        3      /* ducklings in the line(s) for the trumpet layer */
#define LAYER2_AT        8      /* ... and the glockenspiel */

#endif
