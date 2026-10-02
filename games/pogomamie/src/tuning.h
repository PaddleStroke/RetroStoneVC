/*
 * Pogo Mamie: THE tuning table. Every gameplay number is here.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 *
 * The bounce comes from the reference feel (Doodle Jump, measured and cloned, see DESIGN.md "Feel sources"),
 * first written in the references' units (their pixels, per frame at 60 Hz), then converted to our world:
 *   - time is kept as is (60 Hz on both sides);
 *   - the arc is scaled by the SCREEN HEIGHT: a bounce covers the same share of the screen's height as in the
 *     reference (the vertical axis keeps gravity in our sideways game), 240 px here;
 *   - horizontal speeds are scaled by the same factor (the reference's pixels at the scale of our screen
 *     height), so the shape of an arc (its width against its height) is the reference's.
 * Positions and speeds are 16.16 fixed point (Q16): 65536 = 1 px (per frame). The float expressions are folded
 * by the compiler into integer constants: no floating point runs in the game.
 */
#ifndef PM_TUNING_H
#define PM_TUNING_H

#define Q16_ONE 65536
#define Q16(px) ((int32_t)((px) * 65536.0 + ((px) < 0 ? -0.5 : 0.5)))

/* ---- the references (60 Hz, their pixels) --------------------------------------------------------------------- */
/* 1: "Basic Doodle Jump" by Steven Lambert (straker), CC0: a 375x667 canvas, requestAnimationFrame (60 Hz) */
#define REF1_H        667.0
#define REF1_GRAVITY  0.33          /* px/frame^2 */
#define REF1_BOUNCE   12.5          /* px/frame, set on every landing */
#define REF1_VX       3.0           /* px/frame while a key is held */
#define REF1_DRAG     0.3           /* px/frame^2 when released */
/* 2: MisaghM/Doodle-Jump (C++/SDL2), MIT: a 400x640 window, values per millisecond (converted to 60 Hz) */
#define REF2_H        640.0
#define REF2_GRAVITY  (0.0014 * (1000.0 / 60) * (1000.0 / 60))   /* 0.389 px/frame^2 */
#define REF2_BOUNCE   (0.7 * 1000.0 / 60)                        /* 11.67 px/frame */
#define REF2_SPRING   (1.3 * 1000.0 / 60)                        /* 21.67 px/frame: the spring */
#define REF2_VX       (0.42 * 1000.0 / 60)                       /* 7 px/frame */
/* 3: video analysis of the original game (dragonflytraining, 2014): g = 10 m/s^2, a 2.3 m jump */
#define REF3_T_APEX   (0.678 * 60)                               /* sqrt(2 * 2.3 / 10) s = 40.7 frames */

/* the apex of a bounce as a share of the screen height (refs 1 and 2: 0.355 and 0.273) and its time (1-3) */
#define REF_APEX_FRAC ((REF1_BOUNCE * REF1_BOUNCE / (2 * REF1_GRAVITY) / REF1_H + \
                        REF2_BOUNCE * REF2_BOUNCE / (2 * REF2_GRAVITY) / REF2_H) / 2)      /* 0.314 */
#define REF_T_APEX    ((REF1_BOUNCE / REF1_GRAVITY + REF2_BOUNCE / REF2_GRAVITY + REF3_T_APEX) / 3)  /* 36.2 */
#define REF_SPRING_FRAC (REF2_SPRING * REF2_SPRING / (2 * REF2_GRAVITY) / REF2_H)           /* 0.94 */

/* ---- our bounce ------------------------------------------------------------------------------------------------- */
#define SCREEN_PLAY_H 240
#define APEX_NORMAL   75            /* px: REF_APEX_FRAC x 240 = 75.4 */
#define T_APEX        36            /* frames: REF_T_APEX = 36.2 (0.60 s) */
#define GRAVITY       Q16(2.0 * APEX_NORMAL / (T_APEX * T_APEX))      /* 0.1157 px/frame^2 */
#define V_NORMAL      Q16(2.0 * APEX_NORMAL / T_APEX)                 /* 4.167 px/frame */
/* holding A on a landing: a higher, longer arc (ours: 1.6 x the height, 1.26 x the time) */
#define APEX_BIG      120
#define V_BIG         Q16(5.2705)   /* sqrt(2 x g x 120) */
/* the awning is the reference's spring: 0.94 of the screen height (ref 2) = 226 px */
#define APEX_SPRING   226
#define V_SPRING      Q16(7.2329)   /* sqrt(2 x g x 226) */
/* the clothesline: it dips under her (an ease-out from her landing speed to a stop at the bottom), then slings her
 * up from the bottom of the dip; then it recoils (a damped spring) and the clothes swing */
#define APEX_SLING    150
#define V_SLING       Q16(5.8926)   /* sqrt(2 x g x 150) */
#define SLING_FRAMES  10            /* frames riding the line down: released at the bottom of the dip */
#define SLING_DEPTH   14            /* px it dips under her at the middle of a long line (w/4 at most on a short one;
                                     * less near a pole: the dip of a loaded string, 4 k (w - k) / w^2) */
#define LINE_REST_SAG(w) (2 + (w) / 16)   /* the rest sag at the middle (px): a shallow catenary (parabola) */
#define LINE_K        21            /* the recoil: a spring of 21/256 per frame^2 (a 22-frame swing), */
#define LINE_DAMP     232           /*   keeping 232/256 of its speed each frame */
#define MAX_FALL      Q16(8.0)      /* terminal speed (the comic fall); a spring arc never reaches it */

/* ---- air control (Left/Right) ------------------------------------------------------------------------------------ */
/* top speed: refs 1 and 2 at the scale of our screen height: 3 x 240/667 = 1.08, 7 x 240/640 = 2.63; mean 1.85 */
#define VX_MAX        Q16(1.875)
#define VX_CRUISE     Q16(1.0)      /* no input: the pogo drifts forward at this speed (ours: an endless run) */
#define VX_BACK       Q16(-1.0)     /* Left brakes, down to a slow backward drift */
/* inertia: ref 1's drag 0.3 x 240/667 = 0.108 px/frame^2 -> 1/8: 0 to top speed in 15 frames (0.25 s) */
#define ACCEL_X       Q16(0.125)
#define BRAKE_X       Q16(0.1875)   /* braking bites harder than accelerating */
#define RELAX_X       Q16(0.03125)  /* back to cruise speed with no input */
/* slopes deflect the bounce: it leaves along the half angle between straight up and the roof's normal (a mix of
 * 1/2 and 1/2), so a 45-degree zinc slope deflects it by 22.5 degrees and a 1:2 tiled slope (26.57 degrees) by
 * 13.28; the speed of the bounce is kept: vy = -V cos, vx += V sin downhill (forward or back) */
#define SLOPE_SIN45   Q16(0.382683) /* sin 22.5 */
#define SLOPE_COS45   Q16(0.923880)
#define SLOPE_SIN27   Q16(0.229753) /* sin 13.28 = sin(atan(1/2) / 2) */
#define SLOPE_COS27   Q16(0.973249)
#define VX_SLOPE_MAX  Q16(3.0)      /* the deflection never pushes her faster than this */

/* ---- Mamie's body ------------------------------------------------------------------------------------------------- */
#define HALF_W        6             /* body box: 12 x 26 px above the feet (the sprite is 24 x 32) */
#define BODY_H        26
#define FOOT_HALF     3             /* the pogo's foot: it lands when x +- 3 is over a surface */
#define STEP_UP       8             /* a wall is solid when its top is more than this above her feet */

/* ---- power-ups ---------------------------------------------------------------------------------------------------- */
#define UMBRELLA_T    240           /* 4 s of slow glide */
#define UMB_GRAVITY   (GRAVITY / 3)
#define UMB_MAX_FALL  Q16(0.75)
#define CROISSANT_T   180           /* 3 s of speed burst */
#define VX_BOOST      Q16(2.75)
#define YARN_REEL_T   36            /* frames reeling her up onto the ledge */

/* ---- hazards ------------------------------------------------------------------------------------------------------ */
/* Hitting an antenna, a pigeon from the side or from below, or a balloon's basket or ropes ENDS THE RUN: she is
 * thrown back, tumbles for TUMBLE_T frames (no control, no roof catches her, whatever is below) and falls into the
 * street (the river): the fall and the game-over panel. Only a stomp from above bounces. */
#define PIGEON_W      12            /* the pigeon's box */
#define PIGEON_H      10
#define PIGEON_SPEED  Q16(0.25)     /* walking on a roof */
#define TUMBLE_VX     Q16(1.5)      /* knocked away from what she hit (it dies away by 1/64 a frame) */
#define TUMBLE_HOP    Q16(2.0)      /* ... popped up a little before dropping */
#define TUMBLE_T      28            /* frames of tumbling before the fall */
#define V_RECOVER     Q16(3.042)    /* the landing after a tumble: a weak 40-px hop, sqrt(2 x g x 40) */
#define ANTENNA_H     28
#define ANTENNA_HALF  3
#define ANTENNA_HOP   14            /* her feet this far before an antenna, she can always hop over it from a stop */
/* flying pigeons (in the gaps): hovering (flapping in place), gliding back and forth, swooping (a U-shaped dive) */
#define FLY_PERIOD_MIN 150          /* frames for one way and back */
#define FLY_PERIOD_MAX 270
#define FLY_HOVER_BOB 5             /* px up and down while flapping in place */
#define FLY_SWOOP_MIN 24            /* px a swoop dives at its middle */
#define FLY_SWOOP_MAX 48
/* hot-air balloons drift left across the sky; the top of the envelope is a big bouncy platform (a spring and
 * PTS_BALLOON), the basket and its ropes are a hazard; the basket stays at least BALLOON_CLEAR above every roof
 * it drifts over, so a normal bounce never touches it (she can always wait for it to drift by) */
#define BALLOON_W     32            /* the envelope (a 32x32 sprite; the basket 16x16 below it) */
#define BALLOON_TOP_X0 8            /* the bouncy top: [x + 8, x + 24) at the envelope's first row */
#define BALLOON_TOP_X1 24
#define BALLOON_HAZ_X0 9            /* the ropes and the basket: [x + 9, x + 23) x [y + 26, y + 46) */
#define BALLOON_HAZ_X1 23
#define BALLOON_HAZ_Y0 26
#define BALLOON_HAZ_Y1 46
#define BALLOON_CLEAR 104           /* px from the basket's bottom down to the highest roof below: > 73 + 26 */
#define BALLOON_V     Q16(0.25)     /* drift, px/frame (to the left) */
#define BALLOON_BACK  1200          /* px behind its spawn point it can drift over before it leaves the screen */
#define GUST_V        Q16(0.375)    /* wind drift, px/frame, added to her motion inside a gust zone */
#define PIT_DEPTH     24            /* a broken skylight: she drops one floor */

/* ---- the world ---------------------------------------------------------------------------------------------------- */
#define WORLD_H       512           /* 64 tiles; y grows downward */
#define STREET_Y      464           /* the sidewalk (or the river) */
#define ROOF_MIN_Y    176
#define ROOF_MAX_Y    400
#define START_X       72            /* Mamie's first roof */
#define START_TOP     320
#define PX_PER_M      8             /* 1 m = 8 px (one tile) */
#define DISTRICT_PX   4096          /* 512 m per district: one BG4 panorama at 1/8 speed */
#define DISTRICTS     4             /* Montmartre, the Seine, the Haussmann boulevards, the Eiffel Tower; then night */

/* ---- the generator ------------------------------------------------------------------------------------------------ */
#define BLD_W_MIN     48
#define TAKEOFF_MARGIN 8            /* the worst-case take-off: 8 px before the edge, from a standstill */
#define LAND_MARGIN   12            /* ... and the landing must be this far past the next building's edge */
#define MAX_DROP      96            /* a roof is at most this much lower than the one before: it is on screen */
#define TAKEOFFS      {8, 24, 40}   /* the take-off points tried (px before the edge): kept free of hazards */
#define EDGE_KEEP     48            /* skylights, and antennas from a roof's right end (the take-off points) */
#define ANTENNA_KEEP_L 20           /* antennas from a roof's left end (the landing: checked by the generator) */
#define ANTENNA_SPACE 56            /* between two antennas */
#define PIGEON_KEEP_L 40            /* pigeons walk this far from the left end */
#define PIGEON_KEEP_R 44            /* ... and from the right end */
#define FLY_KEEP      4             /* a flying pigeon's centre stays this far inside its gap */
#define DIFF_FULL_M   2000          /* difficulty rises linearly to its maximum at 2000 m (and beyond at night) */
#define GAP_MIN       24
#define WARMUP_M      80            /* the first metres: no gap, a warm-up (height steps and hazards only) */
#define GAP_MAX_EASY  72
#define GAP_MAX_HARD  136
#define NOGAP_EASY    25            /* % of neighbours with no gap (a height step) */
#define NOGAP_HARD    5
#define PIGEON_EASY   15            /* % of roofs with a pigeon walking on it */
#define PIGEON_HARD   60
#define FLYER_EASY    18            /* % of gaps with a flying pigeon (from FLYER_FROM_M) */
#define FLYER_HARD    70
#define FLYER_FROM_M  80
#define ANTENNA_EASY  30            /* % of roofs with a TV antenna (a second one: half that, on wide roofs) */
#define ANTENNA_HARD  80
#define BALLOON_FROM_M 120          /* the first hot-air balloon, then one every BALLOON_EVERY_* m on average */
#define BALLOON_EVERY_EASY 300
#define BALLOON_EVERY_HARD 110
#define GUST_EASY     0             /* % of buildings that start a gust zone (none before GUST_FROM_M) */
#define GUST_FROM_M   300
#define GUST_HARD     35
#define HAZARD_EASY   20            /* % chance of a skylight on a roof */
#define HAZARD_HARD   60
#define SLOPED_EASY   40            /* % of houses with a pitched roof (Haussmann buildings: mansards, 70%) */
#define SLOPED_HARD   60
#define PROP_EASY     45            /* % of gaps with a rescue prop (awning, pot, cradle, line, ledge) */
#define PROP_HARD     15
#define ITEM_EVERY    360           /* ~one power-up every this many metres */

/* ---- camera -------------------------------------------------------------------------------------------------------- */
#define CAM_X_SLOW    112           /* the leader's screen x at a standstill */
#define CAM_X_LEAD    24            /* ... minus this per px/frame of speed: the view leads to the right */
#define CAM_SMOOTH_X  8             /* 1/8 of the distance per frame */
#define CAM_SMOOTH_Y  12
#define CAM_FEET_Y    156           /* the feet at the last landing, on screen */
#define CAM_FEET_Y_READY 226        /* on the title and "get ready": low, the sky for the text */
#define CAM_TOP_KEEP  8             /* her head stays this far below the top edge */
#define CAM_LOOK_AHEAD 240          /* the roofs up to this far ahead stay in view: */
#define CAM_HI_KEEP   56            /*   the highest this far below the top edge, */
#define CAM_LO_KEEP   224           /*   the lowest no further down than this (screen y) */
#define CAM_BOT_KEEP  216
#define DROP_OUT_X    24            /* 2-4 players: this far off the left edge, a player drops out */
#define CAM_WAIT_X    32            /* 2-4 players: the camera keeps the others at least this far in... */
#define CAM_LEADER_MAX_X 272        /* ... as long as the leader stays left of this (screen x) */

/* ---- flow ---------------------------------------------------------------------------------------------------------- */
#define PANEL_DELAY   80            /* frames between landing in the street and the panel (the cursing) */
#define RETRY_LOCK    36            /* frames the panel ignores the button */
#define SQUASH_T      10            /* squash and stretch frames after a landing */

/* ---- score ---------------------------------------------------------------------------------------------------------- */
#define PTS_CHIMNEY   50            /* landing on a chimney top (or a bouquiniste's box) */
#define PTS_PIGEON    100           /* bouncing on a pigeon's head (walking) */
#define PTS_PIGEON_FLY 150          /* ... on a flying one */
#define PTS_BALLOON   300           /* bouncing on a balloon's top */
#define CHAIN_MAX     8             /* stunts in a row multiply up to x8 */
/* "cat catch" medals by distance (m): bronze, silver and gold whiskers, then the cat caught at the Eiffel Tower */
#define MEDAL_BRONZE  250
#define MEDAL_SILVER  500
#define MEDAL_GOLD    1000
#define MEDAL_CAT     2000

#endif
