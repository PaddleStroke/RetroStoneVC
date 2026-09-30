/*
 * Beaver Rush: the screen layout, shared by draw.c, scene.c and bot.c (the bot reads the screen).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
 */
#ifndef BR_DRAW_H
#define BR_DRAW_H

/* screen lines (1 player; a viewport's lines in versus: the views are full height) */
#define CLOUDS_END 56
#define FOREST_Y   104
#define SKY_H      128            /* the horizon of the sky gradient */
#define DAM_Y      128            /* the dam canvas: lines 128..159 */
#define RIVER_Y    160            /* the river below the dam */
#define NEAR_Y     192            /* the near bank (BG3 rows 24..29) */
#define GROUND_Y   200            /* the beaver's feet, the stump's top: segment 0 is 176..199 */
#define POND_LOW   126            /* the pond's surface line: empty dam .. POND_LOGS logs */
#define POND_HIGH  106
#define POND_LOGS  600

/* the trunk on BG2 (a 32 x 64 map: tree p in rows 32p..32p+31): segment k's rows start at map y
 * TRUNK_MAP_Y0 - 24 (k + 1); the trunk's columns 8..13, the left branch 3..7, the right one 14..18 */
#define TRUNK_MAP_Y0  248
#define TRUNK_SCROLL_Y 48         /* map y - screen y */
#define TRUNK_COL     8
#define LBRANCH_COL   3
#define RBRANCH_COL   14
#define TRUNK_MAP_X   64          /* the trunk's left edge in the map */

/* versus: the two views, and each view's sprites in OAM */
#define VIEW_OAM      56
#define VIEW_BG_X     80          /* a versus view shows the panorama from PANO_X + 80 */

/* scene.c */
void scene_init(void);
void scene_restart(int stage);
void scene_frame(int stage, int dam_logs, int lean_side, int players, int t);
void scene_bg_scroll(int view_sx);
int  scene_stage_shown(void);
int  scene_tod(void);
int  scene_season(void);
int  scene_pond_line(void);
int  scene_lean(void);
void dam_slot(int k, int *x, int *y);
void dam_add(int k);
void dam_reset(int logs);
int  dam_logs_drawn(void);
void scene_state(void);

#endif
