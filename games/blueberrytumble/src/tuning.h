/*
 * Blueberry Tumble: THE tuning table. Every gameplay number is here (DESIGN.md "Feel sources" and "From the
 * sources to our grid" say where each one comes from).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 *
 * Units: 1 block = 1 cell = 16 px = 1 metre; 1 beat = 4 cells = 64 px; 60 frames per second.
 * Positions and speeds are 16.16 fixed point (Q16): 65536 = 1 px (per frame). The float expressions are folded
 * by the compiler into integer constants: no floating point runs in the game.
 */
#ifndef BT_TUNING_H
#define BT_TUNING_H

/* ---- the name (the owner may rename the game: here, plus the exe name in game.mk) ------------------------------ */
#define GAME_NAME   "Blueberry Tumble"
#define GAME_TITLE  "BLUEBERRY TUMBLE"      /* the logo (capitals, the house logo font) */
#define GAME_ID     "blueberrytumble"
#define GAME_MAGIC  "BBTM"                  /* save RAM */

#define Q16_ONE 65536
#define Q16(px) ((int32_t)((px) * 65536.0 + ((px) < 0 ? -0.5 : 0.5)))

/* ---- the grid -------------------------------------------------------------------------------------------------- */
#define CELL        16          /* px: one block, one metre, a 16th note */
#define BEAT_CELLS  4
#define BAR_CELLS   16
#define BEAT_PX     (CELL * BEAT_CELLS)     /* 64: the scroll covers one beat per beat */
#define ROWS        12          /* cell rows above the ground (row 0 stands on the ground) */

/* ---- the reference (DESIGN.md sources 1-4) ---------------------------------------------------------------------- */
#define REF_SPEED_SLOW    8.3719    /* blocks/s (source 1) */
#define REF_SPEED_NORMAL 10.3859
#define REF_JUMP_BLOCKS   3.6       /* a normal-speed jump's length (source 3) */
#define REF_JUMP_HEIGHT   2.1       /* "a little over 2 blocks" (source 3) */
#define REF_AIR_FRAMES   (REF_JUMP_BLOCKS / REF_SPEED_NORMAL * 60.0)   /* 20.8 frames */

/* ---- tempo tiers: the tempo and the speed step up at each biome (DESIGN.md "Tempo, speed, biomes") ------------ *
 * frames per beat = FPB_NUM / FPB_DEN = 3600 / BPM; the MOD tempo T and speed S give BPM = 6 T / S (4 rows per beat)
 * with a whole tick of 80000 / T samples at 32 kHz. */
#define NTIERS 5
#define TIER_BPM_X100   {12800, 13714, 15000, 16000, 17143}
#define TIER_FPB_NUM    {225, 105, 24, 45, 21}
#define TIER_FPB_DEN    {8, 4, 1, 2, 1}
#define TIER_MOD_TEMPO  {128, 160, 125, 160, 200}
#define TIER_MOD_SPEED  {6, 7, 5, 6, 7}
#define NBIOMES      4          /* summit, forest, meadow, village; then the loop at night (tier 4) */
#define BIOME_BARS   24
#define BIOME_CELLS  (BIOME_BARS * BAR_CELLS)   /* 384 m */
/* the drawn slope per biome (the shear of the playfield), in 1/256 px down per px across */
#define BIOME_SLOPE  {85, 64, 51, 32}           /* 1/3, 1/4, 1/5, 1/8 */
/* the profile on top of it (drawing only; DESIGN.md "The slope"): within PROF_WINDOW columns (a screen) the ground's
 * drawn height spans at most PROF_SPAN px (the playfield map's 64 rows); never more than PROF_MAX off level */
#define PROF_WINDOW  24
#define PROF_SPAN    32
#define PROF_MAX     40

/* ---- the berry's jump (sources 1, 3: fixed in time, 21 frames, about 2.2 blocks high) --------------------------- *
 * Step order: a jump sets vy; then every airborne frame vy -= GRAVITY, h += vy. So h(n) = n V - G n (n+1) / 2:
 * apex at n = V / G = 11 (35.0 px), back on the ground at n = 21. */
#define JUMP_FRAMES  21
#define JUMP_HEIGHT  35.0
#define GRAVITY      Q16(JUMP_HEIGHT / 55.0)               /* 0.636 px/frame^2 */
#define JUMP_V       Q16(11.0 * JUMP_HEIGHT / 55.0)        /* 7.0 px/frame */
#define ORB_V        JUMP_V                                /* a dew drop: a full jump from where you are */
#define PAD_V        Q16(9.9)                              /* a mushroom: 72 px (4.5 blocks) */
#define SNOW_JUMP_V  Q16(5.6)                              /* the snowberry: 22 px (heavier: lower jumps) */
#define MAX_FALL     Q16(12.0)
#define SNAP_UP      3          /* px: running into a top this close below it steps up onto it */
#define FALL_DEATH   40         /* px below the ground: fell into a crevasse */

/* ---- the leaf glider (the flying mode) -------------------------------------------------------------------------- */
#define GLIDE_LIFT   Q16(1.0)   /* held: up (whole px: a small state space for the solver) */
#define GLIDE_SINK   Q16(1.0)   /* released: down */
#define GLIDE_VMAX   Q16(3.0)
#define GLIDE_CEIL   (10 * CELL) /* the berry's top never goes above 10 blocks */

/* ---- hitboxes (px) ------------------------------------------------------------------------------------------------- *
 * The reference: the player's hitbox is one block, walls use a smaller inner box, spikes are smaller than their
 * sprites (source 5). Ours: size = the body's diameter; the body's centre is at h + size / 2. */
#define BERRY_SIZE   14
#define BERRY_HAZ    5          /* hazard box: +-5 around the centre (10 x 10) */
#define BERRY_SOLID  7          /* solid box: +-7 (14 wide): landing and edge support */
#define BERRY_CORE   3          /* core: +-3 (6 x 6): walls and ceilings */
#define SNOW_SIZE    22
#define SNOW_HAZ     8
#define SNOW_SOLID   11
#define SNOW_CORE    5
/* objects, in their cell (x0, x1, y0, y1; y up from the cell's bottom) */
#define THORN_BOX    {5, 11, 0, 8}
#define HANG_BOX     {5, 11, 8, 16}
#define PEBBLE_BOX   {3, 13, 0, 6}
#define PAD_BOX      {2, 14, 0, 5}
#define ORB_R        12         /* the dew drop's box: +-12 around the cell centre (generous, like the genre's orbs) */
#define COIN_R       7
#define CONE_R       5          /* 10 x 10, rolling on the ground */
#define CONE_SLOW    85         /* the cone's speed, 1/256 of the run speed (1/3) */
#define CONE_FAST    128        /* 1/2 */

/* ---- the screen ---------------------------------------------------------------------------------------------------- */
#define PIVOT_X      96         /* player 1's centre, screen x (the shear pivot) */
#define P2_OFFSET    28         /* player 2 rolls this far behind */
#define VIEW_AHEAD   (320 - PIVOT_X)   /* px of course visible ahead of the berry (the reaction time) */
#define GROUND_SCREEN_Y 172     /* the ground at the pivot, screen y, when the camera is at rest */
#define CAM_TOP_MARGIN  56      /* the camera rises when the berry's top goes above this screen y */

/* ---- flow ---------------------------------------------------------------------------------------------------------- */
#define START_CELLS  24         /* flat ground before the first pattern (the run starts at x = 0, bar 0) */
#define HIT_FREEZE   8          /* hit-stop frames (house) */
#define PANEL_DELAY  40         /* frames between the splat and the game-over panel */
#define RETRY_LOCK   36
#define READY_BOB    4

/* ---- score, medals ------------------------------------------------------------------------------------------------- */
#define GOLDEN_BONUS 50
#define MEDAL_METRES {200, 500, 1000, 1600}     /* bronze, silver, gold, pearl (the night) */

/* ---- the director: the difficulty curve (DESIGN.md "Measured difficulty") ---------------------------------------- *
 * target(d) = DIFF_START + (DIFF_MAX - DIFF_START) * d / (d + DIFF_HALF_M)  +  a wave: over each DIFF_WAVE_M the
 * target rises from -DIFF_WAVE_AMP to +DIFF_WAVE_AMP (tension), then a breather. */
#define DIFF_START     14
#define DIFF_MAX       70
#define DIFF_HALF_M    1400
#define DIFF_WAVE_M    144      /* metres per wave (about 15 s) */
#define DIFF_WAVE_AMP  8
#define DIFF_BAND_LO   16       /* the band the director picks in, around its aim: easier patterns stay in the mix */
#define DIFF_BAND_HI   6
#define DIFF_SPIKE     8        /* no instance above target + this */
#define DIFF_STEER     12       /* the aim = the target + (target - the generated mean), at most this far off */
#define DIFF_MEAN_K    5        /* the generated mean: a moving mean over about 2^K instances */
#define DIFF_INTERLUDE 10       /* a pattern all easier than the band: this many times rarer (an interlude) */
#define DIFF_EASY_M    256      /* the first 30 s: easy only */
#define DIFF_EASY_MAX  30
#define DIRECTOR_MEMORY 6       /* patterns: the last ones are avoided (unless nothing else fits) */
#define PAIR_MEMORY    24       /* pairs: "A then B" seen lately is avoided */
#define INST_MEMORY    16       /* instances (a pattern with the same parameters) seen lately are avoided */
#define COIN_CHANCE    16       /* in 256, for a pattern with a coin spot */
#define COIN_MIN_M     150
/* the score formula's weights (percent) and scales */
#define DIFF_W_WINDOW  40
#define DIFF_W_DENSITY 30
#define DIFF_W_SKILLS  15
#define DIFF_W_REACT   15
#define DIFF_WIN_EASY  14       /* frames: a window this wide scores 0 */
#define DIFF_WIN_HARD  3        /* and this narrow 1 */
#define DIFF_DENS_MAX  3        /* presses per second that score 1 */
#define DIFF_REACT_EASY 70      /* frames */
#define DIFF_REACT_HARD 20
#define MIN_WINDOW     3        /* the validator's requirement: every window >= 3 frames */
#define LINK_MARGIN    10       /* frames added to tail(A) - head(B) when sizing a bridge */

#endif
