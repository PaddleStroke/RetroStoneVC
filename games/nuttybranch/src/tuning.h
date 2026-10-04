/*
 * Nutty Branch: THE tuning table. Every gameplay and rendering number is here (DESIGN.md explains them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
 *
 * Units: the world is measured in "units" (1 unit = 1 texel of the branch texture; the branch radius is 64 units,
 * 1 m = 64 units). Distances along the branch and heights are Q8 units (256 = 1 unit); angles around the branch are
 * Q12 degrees (4096 = 1 degree, 0 = the top, > 0 to the right); per-frame rates at 60 Hz. The float expressions are
 * folded by the compiler into integer constants: no floating point runs in the game.
 */
#ifndef NB_TUNING_H
#define NB_TUNING_H

/* ---- the name (the owner may rename the game here: the frontends' name and the title logo) ------------------- */
#define GAME_NAME   "Nutty Branch"
#define GAME_LOGO   "NUTTY BRANCH"
#define GAME_ID     "nuttybranch"
#define GAME_MAGIC  "NUTB"                 /* the save RAM's 4 letters */

#define Q8(u)    ((int32_t)((u) * 256.0 + ((u) < 0 ? -0.5 : 0.5)))
#define ANG(d)   ((int32_t)((d) * 4096.0 + ((d) < 0 ? -0.5 : 0.5)))
#define UNITS_PER_M  64

/* ---- the branch ------------------------------------------------------------------------------------------------ */
#define BRANCH_R      64                   /* radius, units (= texels) */
#define SEG_LEN       16                   /* a segment of the course, units */
#define SEG_RING      1024                 /* segments kept (16384 units: behind the camera and the generator's lead) */
#define GEN_AHEAD     4096                 /* the generator stays this far (units) ahead of the leader */

/* ---- the balance (DESIGN.md "The branch is a cylinder") ---------------------------------------------------------- */
#define STEER_ACC     ANG(0.24)            /* the D-pad's angular push, per frame^2 */
#define DAMP_256      21                   /* angv -= angv * 21 / 256 each frame (about 1/12): momentum, a top speed */
#define PULL          ANG(0.21)            /* gravity's tipping push at 90 degrees (x sin(ang)) */
#define SLIP_ANG      ANG(62)              /* beyond it the squirrel slips off */
#define WARN_ANG      ANG(40)              /* the wobble, the creak, the red meter */
#define METER_ANG     ANG(18)              /* the balance meter shows beyond this */
#define AIR_STEER_256 128                  /* steer x 0.5 in the air */
#define CENT_DIV      710000LL             /* centrifugal: angacc = -curve(Q8) * speed(Q8)^2 / CENT_DIV (Q12 deg) */
#define SAP_GRIP_256  115                  /* sap: steer x 0.45 */
#define SAP_DAMP_256  7                    /* sap: damping / 3 */
#define SAP_PULL_256  346                  /* sap: pull x 1.35 */
#define WEB_SPEED_256 141                  /* a web: speed x 0.55 ... */
#define WEB_RECOVER   90                   /* ... recovering over 90 frames */
#define WEB_STICKY    30                   /* steer x 0.5 for 30 frames */
#define WIND_ACC      ANG(0.085)           /* a gust's push */
#define WIND_FRAMES   90                   /* a gust lasts 1.5 s ... */
#define WIND_WARN     60                   /* ... and its leaves stream across 1 s before */
#define BUMP_ACC      ANG(0.12)            /* two squirrels side by side push apart (the pack jostles) ... */
#define BUMP_LEN      12                   /* ... within 12 units along the branch and 14 degrees round it */
#define TAIL_K        6                    /* the tail swings by -angv * 6 (Q12 deg) */

/* ---- the jump --------------------------------------------------------------------------------------------------- */
#define JUMP_VH        Q8(2.4)             /* take-off speed, units/frame */
#define JUMP_G         Q8(0.16)            /* gravity */
#define JUMP_G_HOLD    Q8(0.07)            /* gravity while A is held and the squirrel rises ... */
#define JUMP_HOLD_MAX  14                  /* ... for up to 14 frames */
#define JUMP_BUFFER    6                   /* a press this early before landing still jumps */
#define COYOTE         4                   /* frames after running off a gap's edge when a jump still works */
#define LAND_SQUASH    5                   /* frames of the landing squash */

/* ---- speed ------------------------------------------------------------------------------------------------------ */
#define SPEED0        Q8(5)                /* units/frame at the start (4.7 m/s) */
#define SPEED_STEP_M  250                  /* +1 unit/frame every 250 m ... */
#define SPEED_MAX     Q8(10)               /* ... up to 10 (9.4 m/s) */
#define CATCH_UP_256  15                   /* a squirrel behind the leader runs +6% until level */
#define PACK_LAG      Q8(36)               /* never more than 36 units behind the leader (the bottom of the screen) */

/* ---- the squirrel's body (collisions) ----------------------------------------------------------------------------- */
#define SQ_HALF_ANG   ANG(7)               /* half its width as an angle on the top */
#define SQ_HALF_LEN   6                    /* half its length along the branch, units */
#define NUT_REACH     ANG(11)              /* a nut is picked up within this angle ... */
#define NUT_REACH_H   Q8(22)               /* ... and below this height */
#define COMBO_FRAMES  45                   /* nuts this close in time make a combo (the crunch rises) */
#define COMBO_MAX     8

/* ---- the camera and the projection (DESIGN.md "Rendering") --------------------------------------------------------- */
#define FOCAL         160                  /* px: a 90-degree field of view on 320 px */
#define HORIZON_Y     96                   /* the screen line of the horizon */
#define CAM_H         72                   /* units above the bark at the leader */
#define CAM_BACK      120                  /* units behind the leader */
#define CAM_NEAR      24                   /* nothing nearer than this is projected */
#define DRAW_SEGS     160                  /* 2560 units of branch drawn */
#define CAM_TURN_SH   3                    /* the camera's heading follows the branch at 1/8 per frame */
#define DROP_K_256    50                   /* things round the side sit lower: R (1 - cos) * F / z * 0.2 */

/* ---- the texture (tools/make_art.py branch_texture) ----------------------------------------------------------------- */
#define TEX_W         512
#define TEX_H         256
#define TEX_CX        256                  /* the centre column of the fork, grain and jagged rows */
#define TEX_VARIANTS  3                    /* three 64-row bark patches side by side (moss, a burl, a scar) ... */
#define TEX_VAR_CX    {85, 256, 427}       /* ... centred on these columns, one picked per metre */
#define ROW_BARK      0                    /* 64 rows of bark along the length (period 64 units: 1 m) */
#define ROW_FORK      64                   /* 22 x 8 rows: two sections, centre to centre 16 * (k + 1) */
#define FORK_STEPS    22
#define ROW_GRAIN     240                  /* 8 rows: the end grain (the far side of a gap) */
#define ROW_JAGGED    248                  /* 8 rows: the broken end (the near side of a gap) */
#define FORK_STEP     16
#define ROW_NONE      1000                 /* outside the texture: transparent */
#define RING_LEN      40                   /* units per ring phase (4 phases: the bark palette per line) */
#define HAZE_LEVELS   4
#define GRAIN_LEN     6                    /* units of end grain at the far side of a gap */
#define JAGGED_LEN    5                    /* units of jagged edge at the near side */

/* ---- the course (gen.c) ------------------------------------------------------------------------------------------ */
#define CURVE_EASY    Q8(0.05)             /* x'' per segment^2 (Jake Gordon's 2 / 4 / 6) */
#define CURVE_MED     Q8(0.10)
#define CURVE_HARD    Q8(0.155)
#define DIP_MAX       Q8(90)               /* the deepest dip, units */
#define FORK_SPREAD_256 102                /* the two branches part at 0.4 units per unit */
#define FORK_DEAD_MIN 420                  /* the dead branch's length after the split, units */
#define FORK_DEAD_RND 200
#define FORK_TAIL     256                  /* units after a dead branch's end that still belong to the fork */
#define REST_FRAMES   45                   /* a quiet stretch after each group, frames at the current speed */
#define REACT_FRAMES  18                   /* the planner's reaction time (fairness test) */
#define LAND_FRAMES   10                   /* frames after landing before the next jump is needed */
#define GAP_FRAMES_MIN 14                  /* a gap's length in frames at the current speed */
#define GAP_FRAMES_MAX 22
#define OWL_SWEEP     48                   /* frames for the owl to cross the branch */
#define CATERPILLAR_V ANG(0.35)            /* its crawl, per frame */
#define CONE_FALL     40                   /* frames a pinecone falls */
#define CONE_LEAD     420                  /* units ahead of the leader where it lands */
#define HOLE_OPEN     90                   /* frames for a woodpecker's hole to open */
#define BAND_M        400                  /* a time of day / season every 400 m */
#define BAND_BLEND_M  32

/* when each kind first appears, metres */
#define UNLOCK_KNOT   60
#define UNLOCK_GAP    120
#define UNLOCK_SAP    180
#define UNLOCK_WEB    250
#define UNLOCK_PECKER 320
#define UNLOCK_CATER  380
#define UNLOCK_WIND   450
#define UNLOCK_CONE   520
#define UNLOCK_FORK   600
#define UNLOCK_CROW   200
#define UNLOCK_SHOWER 280
#define UNLOCK_SNAKE  650
#define UNLOCK_OWL    700                  /* any time of day now (by day a hawk-owl) */
#define UNLOCK_SWING  760
#define UNLOCK_RAIN   850
#define UNLOCK_BEES   950

/* obstacle sizes: half-width (angle) and height (units) */
#define TWIG_HALF     ANG(40)              /* a long side twig grows at 74 degrees and arches over to -6 ... */
#define TWIG_SHORT_HALF ANG(22)            /* ... a short one to 30 (the centre of its span is its angle) */
#define TWIG_H        Q8(10)
#define KNOT_HALF     ANG(16)
#define KNOT_H        Q8(24)
#define WEB_HALF      ANG(22)
#define WEB_H         Q8(36)
#define SAP_HALF      ANG(24)
#define SAP_LEN       56
#define HOLE_HALF     ANG(13)
#define HOLE_LEN      22
#define CATER_HALF    ANG(9)
#define CATER_H       Q8(8)
#define CONE_HALF     ANG(8)
#define CONE_H        Q8(14)
#define OWL_HALF      ANG(18)
#define OWL_H         Q8(12)
#define SNAKE_HALF    ANG(20)              /* a snake lies 44 units across the path ... */
#define SNAKE_H       Q8(9)
#define SNAKE_V       ANG(0.7)             /* ... and slithers across fast */
#define SWING_HALF    ANG(14)              /* the swinging branch's leaves: 14 degrees wide ... */
#define SWING_H       Q8(26)               /* ... up to 26 units over the bark (a held jump clears it) */
#define SWING_PERIOD  150                  /* frames for a swing there and back */
#define SWING_PIVOT   Q8(78)               /* its pivot over the bark; its lowest leaves at 3-17 units */
#define SWING_AMP     45                   /* degrees each way */
#define SWING_LEN     64                   /* units from the pivot to the leaves */
#define BEE_HALF      ANG(15)              /* a bee swarm: 15 degrees wide ... */
#define BEE_H         Q8(40)               /* ... too high to jump over: go round it */
#define BEE_V         ANG(0.3)             /* it drifts across */
#define SNOW_HALF     ANG(13)              /* a lump of snow (winter): a tap jump */
#define SNOW_H        Q8(12)
#define CROW_FLEE     90                   /* units: the crow flies off when a squirrel comes this near */
#define CROW_EAT      14                   /* frames per nut it eats */
#define DROP_FALL     32                   /* frames an acorn of a shower falls ... */
#define DROP_EVERY    9                    /* ... one every 9 frames, landing 60-260 units ahead */
#define DROP_REACH    ANG(9)               /* a falling acorn that lands on you: a bonk (a nudge, a moment slowed) */
#define BONK_KICK     ANG(0.7)
#define WET_GRIP_256  180                  /* wet bark (rain): steer x 0.7 ... */
#define WET_DAMP_256  12                   /* ... damping x 0.6 ... */
#define WET_PULL_256  294                  /* ... pull x 1.15 */
#define RAIN_WARN     90                   /* frames of rain before the wet stretch */
#define OB_LEN        8                    /* the length along the branch of a solid obstacle, units */

/* ---- scoring, the flow ---------------------------------------------------------------------------------------------- */
#define PTS_ACORN     1
#define PTS_HAZEL     2
#define PTS_WALNUT    10
#define MEDAL_METRES  {250, 500, 1000, 1500}     /* bronze, silver, gold, pearl */
#define PANEL_DELAY   50                   /* frames between the last fall and the game-over panel */
#define RETRY_LOCK    36
#define HIT_FREEZE    8                    /* hit-stop frames */
#define FALL_FRAMES   40                   /* the fall animation */
#define BOT_START     20

#endif
