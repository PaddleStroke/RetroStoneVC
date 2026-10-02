/*
 * Beaver Rush: the scenery. The raster sky and water (one colour per line), the palettes of the season and
 * the time of day (a fade at each milestone), the pond that rises behind the dam, the dam itself (a canvas of
 * unique BG3 tiles: each log is drawn into it, pixel by pixel) and the parallax lean.
 * Nothing here changes the game.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
 */
#include "br.h"
#include "draw.h"
#include "assets.h"
#include "house_ui.h"
#include <string.h>

/* the time of day: the sky (top, horizon), and the tint every scene palette takes (brightness, colour, amount) */
typedef struct tod_look { uint32_t top, horizon; int mul; uint32_t tint; int k; } tod_look;
static const tod_look TOD[TIMES_OF_DAY] = {
    {0x3c50a0, 0xf8b496, 236, 0xffa08c, 44},     /* dawn: pink */
    {0x4896e4, 0xc8e8f8, 256, 0x000000, 0},      /* day */
    {0x3c3278, 0xfc9646, 214, 0xff7832, 70},     /* sunset: orange */
    {0x080a20, 0x242e60, 118, 0x1e3282, 92},     /* night: dark blue */
};

static rs_color line_col[RS_SCREEN_H];
static int16_t bg3_dx[RS_SCREEN_H], bg4_dx[RS_SCREEN_H];
static int scene_from, scene_to, fade_t;       /* scenes (stage numbers), the fade's frame */
static int water_q8;                            /* the pond's surface line, Q8 (eased) */
static int pond_line;                           /* its integer line */
static int dam_drawn;                           /* logs drawn into the dam canvas */
static int lean_q8;                             /* the camera lean, px Q8 */
static int sky_dirty;
/* the four players' beaver colours, tinted like the scene's sprites (load_palettes): OBJ 0 holds P1's; in a split
 * screen the raster loads each viewport's player into OBJ 0 (one beaver palette for four beavers), and on the title
 * OBJ 5 and 6 (gold and bird, unused there) hold P3's and P4's for their icons */
static rs_color fur_tint[MAX_PLAYERS][16];
static int title_pals;
static int8_t view_fur[RS_VIEW_MAX];            /* the player whose colours OBJ 0 shows in each viewport */
static int8_t line_fur[RS_SCREEN_H];            /* ... for a viewport of FUR_BY_LINE: by screen line */

int scene_stage_shown(void) { return scene_to; }
int scene_tod(void) { return (fade_t < SCENE_FADE / 2 ? scene_from : scene_to) % TIMES_OF_DAY; }
int scene_season(void) { return (scene_to / TIMES_OF_DAY) % SEASONS; }
int scene_pond_line(void) { return pond_line; }
int scene_lean(void) { return lean_q8 / 256; }

static int ch(uint32_t c, int i) { return (int)(c >> (16 - 8 * i)) & 255; }

/* a palette entry of a scene: the season's colour, tinted by the time of day (half as much on sprites) */
static rs_color scene_color(rs_color c, int stage, int sprite)
{
    const tod_look *t = &TOD[stage % TIMES_OF_DAY];
    int mul = sprite ? 256 - (256 - t->mul) / 2 : t->mul, k = sprite ? t->k / 2 : t->k;
    rs_color d = hu_scale_color(c, mul);
    if (!k) return d;
    return hu_lerp_color(d, RS_RGB8(ch(t->tint, 0), ch(t->tint, 1), ch(t->tint, 2)), k);
}

static void load_palettes(void)
{
    int k = fade_t >= SCENE_FADE ? 256 : fade_t * 256 / SCENE_FADE;
    int sa = (scene_from / TIMES_OF_DAY) % SEASONS, sb = (scene_to / TIMES_OF_DAY) % SEASONS;
    for (int p = 1; p < 8; p++) {
        if (p != PAL_LOGO)
            for (int i = 1; i < 16; i++) {
                rs_color a = scene_color(br_bg_pals[(sa * 8 + p) * 16 + i], scene_from, 0);
                rs_color b = scene_color(br_bg_pals[(sb * 8 + p) * 16 + i], scene_to, 0);
                rs_pal_set(RS_PAL_BG(p) + i, hu_lerp_color(a, b, k));
            }
    }
    for (int p = 0; p < 8; p++) {
        if (p == OPAL_KIT || p == OPAL_BEAVER) continue;
        int glow = p == OPAL_FX;                    /* sun, moon, stars, fireflies, water: not darkened */
        for (int i = 1; i < 16; i++) {
            rs_color a = br_obj_pals[(sa * 8 + p) * 16 + i], b = br_obj_pals[(sb * 8 + p) * 16 + i];
            if (!glow) { a = scene_color(a, scene_from, 1); b = scene_color(b, scene_to, 1); }
            rs_pal_set(RS_PAL_OBJ(p) + i, hu_lerp_color(a, b, k));
        }
    }
    for (int q = 0; q < MAX_PLAYERS; q++)
        for (int i = 1; i < 16; i++) {
            rs_color c = br_fur_pals[q * 16 + i];
            fur_tint[q][i] = hu_lerp_color(scene_color(c, scene_from, 1), scene_color(c, scene_to, 1), k);
        }
    rs_pal_load(RS_PAL_OBJ(OPAL_BEAVER) + 1, &fur_tint[0][1], 15);
    if (title_pals) {
        rs_pal_load(RS_PAL_OBJ(OPAL_GOLD) + 1, &fur_tint[2][1], 15);
        rs_pal_load(RS_PAL_OBJ(OPAL_BIRD) + 1, &fur_tint[3][1], 15);
    }
}

void scene_title_pals(int on)
{
    if (on == title_pals) return;
    title_pals = on;
    load_palettes();
}

int scene_player_pal(int p)
{
    static const uint8_t title[MAX_PLAYERS] = {OPAL_BEAVER, OPAL_BEAVER2, OPAL_GOLD, OPAL_BIRD};
    return title_pals ? title[p & 3] : OPAL_BEAVER;
}

void scene_view_fur(int view, int player)
{
    if (view >= 0 && view < RS_VIEW_MAX) view_fur[view] = (int8_t)player;
}

void scene_line_fur(int y0, int y1, int player)
{
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < RS_SCREEN_H; y++) line_fur[y] = (int8_t)player;
}

static void fur_load(int p)
{
    rs_pal_load(RS_PAL_OBJ(OPAL_BEAVER) + FUR0, &fur_tint[p & 3][FUR0], FUR1 - FUR0);
}

static void sky_lines(int t)
{
    int k = fade_t >= SCENE_FADE ? 256 : fade_t * 256 / SCENE_FADE;
    const tod_look *A = &TOD[scene_from % TIMES_OF_DAY], *B = &TOD[scene_to % TIMES_OF_DAY];
    int top[3], hor[3];
    for (int i = 0; i < 3; i++) {
        top[i] = (ch(A->top, i) * (256 - k) + ch(B->top, i) * k) / 256;
        hor[i] = (ch(A->horizon, i) * (256 - k) + ch(B->horizon, i) * k) / 256;
    }
    static const int deep[3] = {26, 64, 104}, river[3] = {40, 96, 124};
    int mul = (TOD[scene_from % TIMES_OF_DAY].mul * (256 - k) + TOD[scene_to % TIMES_OF_DAY].mul * k) / 256;
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int c[3];
        if (y < pond_line) {                                    /* the sky: top to the horizon */
            int f = y * 256 / SKY_H;
            for (int i = 0; i < 3; i++) c[i] = (top[i] * (256 - f) + hor[i] * f) / 256;
        } else if (y < RIVER_Y) {                               /* the pond (and through the dam's gaps) */
            int f = 96 + (y - pond_line) * 96 / (RIVER_Y - pond_line + 1);
            for (int i = 0; i < 3; i++) c[i] = (hor[i] * (256 - f) + deep[i] * mul / 256 * f) / 256;
            if (((y + t / 6) % 5) == 0 && y < DAM_Y) for (int i = 0; i < 3; i++) c[i] += 18;   /* shimmer */
        } else {                                                /* the river below the dam */
            int f = 110 + (y - RIVER_Y) * 100 / (RS_SCREEN_H - RIVER_Y);
            for (int i = 0; i < 3; i++) c[i] = (top[i] * (256 - f) + river[i] * mul / 256 * f) / 256;
        }
        for (int i = 0; i < 3; i++) c[i] = clampi(c[i], 0, 247);
        /* the house rule: a 2-line dither step every 8 lines keeps the RGB555 bands soft */
        line_col[y] = RS_RGB8(c[0] + ((y & 4) ? 4 : 0), c[1] + ((y & 4) ? 2 : 0), c[2] + ((y & 4) ? 4 : 0));
    }
}

static void raster(int line, void *user)
{
    (void)user;
    rs_pal_set(0, line_col[line]);
    /* the pond covers the foot of the far layer */
    if (line == pond_line) rs_window(0, 0, RS_SCREEN_W);
    else if (line == DAM_Y || line == 0) rs_window(0, 0, 0);
    /* below the stump's top the trunk layer is hidden (its map wraps there while the trunk drops) */
    if (line == 0) rs_window(1, 0, 0);
    else if (line == GROUND_Y) rs_window(1, 0, RS_SCREEN_W);
    /* the beaver's colours: each viewport its player's (one screen: P1's) */
    int v = rs_viewport_current();
    if (v < 0) {
        if (line == 0) fur_load(0);
    } else {
        int p = view_fur[v] == FUR_BY_LINE ? line_fur[line] : view_fur[v];
        if (p >= 0) fur_load(p);
    }
}

/* ---- the dam canvas -------------------------------------------------------------------------------------------- */
static void dam_px(int x, int y, int v)
{
    if (x < 0 || x >= DAM_W * 8 || y < 0 || y >= DAM_H * 8) return;
    rs_tile_pixel(VR_BG3 + DAM_TILE + (y / 8) * DAM_W + x / 8, x & 7, y & 7, v);
}

void dam_slot(int k, int *x, int *y)
{
    int idx = k % STAGE_LOGS, sec = (k / STAGE_LOGS) % DAM_SECTIONS, r = idx / DAM_COLS, c = idx % DAM_COLS;
    *x = sec * 40 + c * 8 + ((r & 1) ? 4 : 0);
    *y = DAM_H * 8 - 3 * (r + 1);
}

static void dam_draw_log(int k)
{
    int pass = clampi(k / (STAGE_LOGS * DAM_SECTIONS), 0, 2);
    int x, y, sec = (k / STAGE_LOGS) % DAM_SECTIONS;
    dam_slot(k, &x, &y);
    const uint8_t *px = br_dam_log + pass * 24;
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 8; i++) {
            int xx = x + i;
            if (xx >= (sec + 1) * 40) xx -= 40;             /* the odd rows' last log wraps to the section's start */
            dam_px(xx, y + j, px[j * 8 + i]);
        }
    if (k % STAGE_LOGS == STAGE_LOGS - 1 && pass == 0)     /* a completed section gets its grassy crown */
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 40; i++) dam_px(sec * 40 + i, j, br_dam_crown[j * 40 + i]);
}

int dam_logs_drawn(void) { return dam_drawn; }

void dam_add(int k)
{
    dam_draw_log(k);
    if (k + 1 > dam_drawn) dam_drawn = k + 1;
}

void dam_reset(int logs)
{
    static const uint8_t zero[64];
    for (int i = 0; i < DAM_W * DAM_H; i++) rs_tiles_load8(VR_BG3 + DAM_TILE + i, zero, 1);
    dam_drawn = 0;
    for (int k = 0; k < logs; k++) dam_add(k);
}

/* ---- set-up and per frame ------------------------------------------------------------------------------------------ */
void scene_init(void)
{
    rs_bg_setup(RS_BG3, 64, 32, VR_BG3);
    rs_bg_setup(RS_BG4, 64, 32, VR_BG4);
    rs_tiles_load(VR_BG3, br_bg3_tiles, br_bg3_tile_count);
    rs_tiles_load(VR_BG4, br_bg4_tiles, br_bg4_tile_count);
    for (int y = 0; y < 30; y++)
        for (int x = 0; x < 64; x++) rs_bg_put(RS_BG3, x, y, br_bg3_map[y * 64 + x]);
    for (int y = 0; y < DAM_H; y++)
        for (int x = 0; x < DAM_W; x++)
            rs_bg_put(RS_BG3, DAM_COL + x, DAM_ROW + y, RS_MAP(DAM_TILE + y * DAM_W + x, PAL_DAM, 1, 0, 0));
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 64; x++) rs_bg_put(RS_BG4, x, y, br_bg4_map[y * 64 + x]);
    rs_bg_line_scroll(RS_BG3, bg3_dx, NULL);
    rs_bg_line_scroll(RS_BG4, bg4_dx, NULL);
    rs_bg_window(RS_BG4, RS_WIN1);
    rs_bg_window(RS_BG2, RS_WIN2);
    rs_raster(raster, NULL);
    scene_from = scene_to = 0;
    fade_t = SCENE_FADE;
    water_q8 = POND_LOW << 8;
    pond_line = POND_LOW;
    lean_q8 = 0;
    dam_reset(0);
    load_palettes();
    sky_lines(0);
}

void scene_restart(int stage)
{
    scene_from = scene_to = stage;
    fade_t = SCENE_FADE;
    load_palettes();
    sky_dirty = 1;
}

void scene_frame(int stage, int dam_logs, int lean_side, int players, int t)
{
    if (stage != scene_to) {
        scene_from = scene_to;
        scene_to = stage;
        fade_t = 0;
    }
    if (fade_t < SCENE_FADE) {
        fade_t++;
        load_palettes();
        sky_dirty = 1;
    }
    /* the pond rises with the dam */
    int target = (POND_LOW << 8) - clampi(dam_logs, 0, POND_LOGS) * ((POND_LOW - POND_HIGH) << 8) / POND_LOGS;
    if (water_q8 != target) {
        water_q8 += (target - water_q8) / 16 + (target > water_q8 ? 1 : -1);
        sky_dirty = 1;
    }
    pond_line = water_q8 >> 8;
    if (sky_dirty || (t % 6) == 0) sky_lines(t);
    sky_dirty = 0;
    /* the lean: toward the beaver's side (1 player) */
    int want = players == 1 ? (lean_side == SIDE_R ? LEAN_PX : lean_side == SIDE_L ? -LEAN_PX : 0) << 8 : 0;
    lean_q8 += (want - lean_q8) / 8;
    int ln = lean_q8 / 256;
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int d4;
        if (y < CLOUDS_END) d4 = t / 16 + ln;                   /* clouds drift */
        else if (y < FOREST_Y) d4 = ln;                         /* mountains: the farthest */
        else if (y < RIVER_Y) d4 = ln * 3 / 4;                  /* the far forest */
        else d4 = -(t / 5) + ln / 2;                            /* the river's ripples flow */
        bg4_dx[y] = (int16_t)(d4 & 511);
        bg3_dx[y] = (int16_t)((y < NEAR_Y ? ln / 2 : 0) & 511);
    }
}

void scene_bg_scroll(int view_sx)
{
    rs_bg_scroll(RS_BG3, view_sx & 511, 0);
    rs_bg_scroll(RS_BG4, view_sx & 511, 0);
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("scene." #v, &(v), sizeof(v))
void scene_state(void)
{
    S(line_col); S(bg3_dx); S(bg4_dx); S(scene_from); S(scene_to); S(fade_t); S(water_q8); S(pond_line);
    S(dam_drawn); S(lean_q8); S(sky_dirty); S(fur_tint); S(title_pals); S(view_fur); S(line_fur);
    RS_STATE_RASTER(raster);
}
#undef S
