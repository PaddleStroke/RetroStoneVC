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

/* the trunk on BG2 (a 64 x 32 map; player p's tree 16 p columns further): segment k's rows start at map y
 * TRUNK_MAP_Y0 - 24 (k + 1); the trunk's columns 8..13, the left branch 3..7, the right one 14..18 (+ 16 p).
 * Widescreen: the map is 128 columns wide and the trees 32 apart (the wider views of a versus show no neighbour's
 * branch): player p's tree starts at column p * tree_cols(), a quarter of the map (the bot reads it there too) */
#define BG2_MAP_W     64
static inline int tree_cols(void) { return rs_bg_map_w(RS_BG2) / 4; }
#define TRUNK_MAP_Y0  248
#define TRUNK_SCROLL_Y 48         /* map y - screen y */
#define TRUNK_COL     8
#define LBRANCH_COL   3
#define RBRANCH_COL   14
#define TRUNK_MAP_X   64          /* the trunk's left edge in the map (player 1) */

/* a phone held upright (DESIGN.md "Tall screen"): a screen of ZOOM_H lines (rs_screen_zoom), its height filling the
 * display's, the sides cropped; the stage low, TALL_BELOW lines of earth under it, the trunk up to the screen's top
 * (at most TALL_ABOVE lines above the stage: segment 15, the last kept, still reaches the top while the trunk drops);
 * BG2's map is TALL_ROWS rows tall there (the 16 segments kept), and the HUD (BG1's top HUD_LINES) at the screen's top */
#define ZOOM_H        440
#define TALL_BELOW    40
#define TALL_ABOVE    160
#define TALL_ROWS     64
#define HUD_LINES     48

/* versus: a view per player (2: halves; 3-4: columns, hu_split HU_SPLIT_COLUMNS), each its own OAM range; a view
 * shows the middle of the panorama: from PANO_X + 160 - its width / 2 (2 players: PANO_X + 80) */

/* scene.c */
void scene_init(void);
void scene_relayout(void);                   /* after a save state of another width: the maps for this one */
void scene_restart(int stage);
void scene_frame(int stage, int dam_logs, int lean_side, int players, int t);
void scene_bg_scroll(int view_sx);
/* a tall screen (rs_screen_zoom): the trunk's left edge (stage x; -1: none, a split screen) and BG2's scroll: the
 * trunk shown above the stage (with its branches: BG2 holds the 16 segments kept) */
void scene_trunk(int x, int sx, int sy);
/* a tall screen: BG1's top lines drawn at the screen's top (draw.c: rs_bg_pin), shown by the raster there (0: none) */
void scene_hud(int lines);
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
/* the four beavers' colours (one beaver palette, OBJ 0, loaded per viewport by the raster; scene.c) */
#define FUR_BY_LINE (-2)
void scene_title_pals(int on);               /* the title: P3's and P4's colours in OBJ 5 and 6 (their icons) */
int  scene_player_pal(int p);                /* the OBJ palette of player p's icon on the title */
void scene_view_fur(int view, int player);   /* viewport view shows player's colours in OBJ 0 (-1: as it is) */
void scene_line_fur(int y0, int y1, int player);   /* ... by line, for a viewport of FUR_BY_LINE */

#endif
