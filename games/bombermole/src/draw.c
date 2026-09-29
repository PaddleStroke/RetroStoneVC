/*
 * Bomber Mole: video (VRAM layout, maps, sprites, HUD, text, transitions).
 * All rights reserved, 8BCraft.
 *
 * VRAM (tile numbers):   0 font, 96 big font (menus), 512 HUD, 700 box font, 800 box tile  [BG1 base 0]
 *                     1024 terrain, +192 props, +384 explosions, +640 weather            [BG2-4 base 1024]
 *                     2048 sprites, +1536 the level's boss                                [OBJ base 2048]
 * Layers: BG1 HUD/text/glow, BG2 weather, BG3 objects + explosions, BG4 ground (see DESIGN.md).
 */
#include "bm.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define VR_BOX_FONT 700
#define VR_BOX 800
#define VR_PLAY 1024
#define VR_WEATHER 1000         /* relative to VR_PLAY (the title logo uses 640..) */
#define VR_OBJ 2048
#define VR_BOSS 1536            /* relative to VR_OBJ */

int view_slot;
static int g_season, g_boss;
static int iris_on, iris_cx, iris_cy, iris_r;
static int lamp_on, lamp_cx, lamp_cy, lamp_r;
static int hud_lines = 1;
static int16_t weather_dx[240];

/* ---- colours ------------------------------------------------------------------------------- */

static void load_pal(int first, const uint16_t *p, int n)
{
    for (int i = 0; i < n; i++)
        if (i % 16) rs_pal_set(first + i, p[i]);
}

/* BG palettes (DESIGN.md, "BG palettes"): 0 font + weather, 1-4 the season's terrain,
 * 5 props A, 6 explosions, 7 the HUD on lines 0-15 and props B below them (the raster
 * callback rewrites it at line HUD_H and back at line 0, like an HDMA palette write).
 * Props A and B are the colour families (PB_PALS) of the terrain-like props this level uses. */
static uint16_t prop_meta[PB_COUNT][4];   /* bm_propbg_meta with this level's palette slots */
static int prop_b = -1;                   /* family in palette 7 below the HUD band, or -1 */

static void prop_pals_assign(int fa, int fb)
{
    int slot[PB_PALS];
    for (int f = 0; f < PB_PALS; f++) slot[f] = PAL_PROPS;
    if (fb >= 0) slot[fb] = PAL_HUD;
    for (int i = 0; i < PB_COUNT; i++)
        for (int k = 0; k < 4; k++)
            prop_meta[i][k] = (uint16_t)((bm_propbg_meta[i][k] & ~(7 << 10)) | (slot[bm_propbg_family[i]] << 10));
    load_pal(RS_PAL_BG(PAL_PROPS), bm_propbg_pals[fa], 16);
    prop_b = fb;
}


/* ---- raster: HUD band, iris and lamp circles ------------------------------------------------ */
static int isqrt(int v)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return r;
}

int hud_pal_below;      /* intro card / pause screen: HUD icons below the HUD band keep their palette */

static void raster(int line, void *u)
{
    (void)u;
    if (prop_b >= 0 && (line == 0 || line == HUD_H))
        load_pal(RS_PAL_BG(PAL_HUD), (line && !hud_pal_below) ? bm_propbg_pals[prop_b] : bm_hud_pal, 16);
    rs_window(0, 0, (hud_lines && line < HUD_H) ? RS_SCREEN_W : 0);
    if (!iris_on && !lamp_on) return;
    int l = 0, r = RS_SCREEN_W;
    if (line >= HUD_H) {
        l = RS_SCREEN_W; r = 0;
        int first = 1;
        for (int k = 0; k < 2; k++) {
            int on = k ? lamp_on : iris_on;
            if (!on) continue;
            int cx = k ? lamp_cx : iris_cx, cy = k ? lamp_cy : iris_cy, rad = k ? lamp_r : iris_r;
            int dy = line - cy, cl = 0, cr = 0;
            if (dy * dy < rad * rad) {
                int h = isqrt(rad * rad - dy * dy);
                cl = cx - h;
                cr = cx + h;
            }
            if (first) { l = cl; r = cr; first = 0; }
            else { if (cl > l) l = cl; if (cr < r) r = cr; }
        }
        if (l >= r) { l = 0; r = 0; }
    }
    rs_window(1, l, r);
}

void iris_set(int on, int cx, int cy, int r)
{
    iris_on = on; iris_cx = cx; iris_cy = cy; iris_r = r;
    rs_clip_black((iris_on || lamp_on) ? RS_WIN2_OUT : 0);
}

void lamp_set(int on, int cx, int cy, int r)
{
    lamp_on = on; lamp_cx = cx; lamp_cy = cy; lamp_r = r;
    rs_clip_black((iris_on || lamp_on) ? RS_WIN2_OUT : 0);
}

void hud_hide_lines(int on) { hud_lines = on; }

/* ---- VRAM ---------------------------------------------------------------------------------------- */
static void font_and_box(int big)
{
    rs_text_load(0, big);
    /* opaque copy of the small font on colour 3 (text boxes) and a solid box tile */
    uint8_t px[64];
    for (int i = 0; i < 64; i++) px[i] = 3;
    rs_tiles_load8(VR_BOX, px, 1);
    for (int g = 0; g < 96; g++)
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                rs_tile_pixel(VR_BOX_FONT + g, x, y, 3);
            }
    /* re-derive the glyph pixels from the transparent font */
    extern const uint8_t rs_font_5x7[96][8];
    for (int g = 0; g < 96; g++)
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                if (rs_font_5x7[g][y] & (0x80 >> x)) {
                    rs_tile_pixel(VR_BOX_FONT + g, x, y, 1);
                    if (x < 7 && y < 7 && !(rs_font_5x7[g][y + 1] & (0x80 >> (x + 1))))
                        rs_tile_pixel(VR_BOX_FONT + g, x + 1, y + 1, 2);
                }
            }
    rs_pal_set(RS_PAL_BG(0) + 1, RS_HEX(0xffffff));
    rs_pal_set(RS_PAL_BG(0) + 2, RS_HEX(0x101018));
    rs_pal_set(RS_PAL_BG(0) + 3, RS_HEX(0x283050));
}

static void weather_tiles(int season)
{
    uint8_t t[64];
    memset(t, 0, sizeof t);
    static const uint32_t cols[SEASONS][2] = {{0x9ab8ff, 0xd0e0ff}, {0xfff0a0, 0xffffff},
                                              {0xe07030, 0xc04020}, {0xffffff, 0xd8e4ff}};
    rs_pal_set(RS_PAL_BG(0) + 4, RS_HEX(cols[season][0]));
    rs_pal_set(RS_PAL_BG(0) + 5, RS_HEX(cols[season][1]));
    switch (season) {
    case SEASON_SPRING: for (int i = 0; i < 4; i++) t[(2 + i) * 8 + 5 - i] = 4; break;       /* rain */
    case SEASON_SUMMER: t[3 * 8 + 3] = 4; t[3 * 8 + 4] = 5; t[4 * 8 + 3] = 5; break;       /* pollen */
    case SEASON_AUTUMN: t[3 * 8 + 3] = 4; t[3 * 8 + 4] = 4; t[4 * 8 + 4] = 5; t[4 * 8 + 5] = 4; break; /* leaf */
    default: t[2 * 8 + 4] = 4; t[3 * 8 + 3] = 5; t[3 * 8 + 5] = 5; t[4 * 8 + 4] = 4; t[3 * 8 + 4] = 4; break;
    }
    rs_tiles_load8(VR_PLAY + VR_WEATHER + 1, t, 1);
    rs_bg_setup(RS_BG2, 32, 32, VR_PLAY);
    uint32_t s = 12345;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            s = s * 1103515245u + 12345u;
            if ((s >> 16) % 9 == 0) rs_bg_put(RS_BG2, x, y, RS_MAP(VR_WEATHER + 1, 0, 1, (s >> 8) & 1, 0));
        }
}

void draw_init_vram(int season, int boss)
{
    g_season = season;
    g_boss = boss;
    rs_oam_clear();
    rs_raster(raster, NULL);
    font_and_box(0);
    rs_tiles_load(HUD_TILE_BASE, bm_hud_tiles, bm_hud_tile_count);
    load_pal(RS_PAL_BG(PAL_HUD), bm_hud_pal, 16);
    rs_tiles_load(VR_PLAY, bm_terrain_tiles[season], bm_terrain_tile_count[season]);
    load_pal(RS_PAL_BG(1), bm_terrain_pals[season], TERRAIN_PALS * 16);
    rs_tiles_load(VR_PLAY + PROPS_TILE_BASE, bm_propbg_tiles, bm_propbg_tile_count);
    prop_pals_assign(0, -1);                /* the level's families: draw_level_pals() */
    rs_tiles_load(VR_PLAY + FX_TILE_BASE, bm_fx_tiles, bm_fx_tile_count);
    load_pal(RS_PAL_BG(PAL_FX), bm_fx_pal, 16);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, bm_obj_tiles, bm_obj_tile_count);
    load_pal(RS_PAL_OBJ(0), bm_obj_pals, 8 * 16);
    if (boss > 0) {
        rs_tiles_load(VR_OBJ + VR_BOSS, bm_boss_tiles[boss], bm_boss_tile_count[boss]);
        load_pal(RS_PAL_OBJ(3), bm_boss_pals[boss], 16);
    }

    rs_backdrop(RS_HEX(0x181c30));

    rs_bg_setup(RS_BG1, 64, 32, 0);
    rs_bg_setup(RS_BG3, 64, 64, VR_PLAY);
    rs_bg_setup(RS_BG4, 64, 64, VR_PLAY);
    weather_tiles(season);
    for (int l = 0; l < 4; l++) {
        rs_bg_enable(l, 1);
        rs_bg_scroll(l, 0, 0);
        rs_bg_line_scroll(l, NULL, NULL);
        rs_bg_window(l, l == RS_BG1 ? 0 : RS_WIN1);
    }
    rs_bg_affine(RS_BG1, NULL);
    rs_obj_window(RS_WIN1);
    rs_text_setup(RS_BG1, 0, 0, 1);
    rs_math(RS_MATH_ADD | RS_MATH_HALF, RS_MATH_BG2, 0);
    rs_brightness(15);
    hud_lines = 1;
    iris_set(0, 0, 0, 0);
    lamp_set(0, 0, 0, 0);
}

/* menus: big font, spring terrain for decoration, no HUD band */
void draw_title_vram(void)
{
    draw_init_vram(SEASON_SPRING, 0);
    font_and_box(1);
    hud_lines = 0;
    for (int l = 0; l < 4; l++) rs_bg_window(l, 0);
    rs_obj_window(0);
    rs_bg_setup(RS_BG2, 32, 32, VR_PLAY);
    rs_math(RS_MATH_OFF, 0, 0);
}

/* ---- playfield ------------------------------------------------------------------------------------ */
int slot_y(int slot) { return slot * 256; }
void set_scroll_slot(int y)
{
    int sy = y - HUD_H;
    int shake = W.shake ? ((W.shake & 1) ? 2 : -2) : 0;
    rs_bg_scroll(RS_BG4, shake, sy);
    rs_bg_scroll(RS_BG3, shake, sy);
}

static int blocks_light(int t) { return !terrain_walkable(t, 0) && t != TR_WATER && t != TR_EXIT; }

/* 0, 1 or 2: which variant of a common tile a cell shows (a fixed hash of its position, so a
   redraw never changes it) */
static int cell_variant(int d, int x, int y)
{
    uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)(d + 1) * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (int)(h % 3u);
}

static int water_frame(void) { return (W.t >> 5) & 1; }    /* the water shimmer: 2 frames, ~0.5 s each */

/* Two playfield layers (DESIGN.md, "Screen"):
 *   BG4, the base ground, always there and season-dependent: grass or snow on the surface, the tunnel
 *        floor below, water, ice; underground, the soil blocks and thin floors are the ground too;
 *   BG3, EVERYTHING placed on it, drawn with transparent pixels: rocks, stone, dirt mounds (surface),
 *        roots, leaves, holes, ladders, the exit, crates, bridges, gates, plates, levers, vents...
 *        A destroyed object leaves the ground. Explosions use BG3 too: while a cell burns, the blast
 *        replaces its object (a blast destroys what it covers, or passes over flat things for 0.5 s).
 * The glow of hidden grubs and the lever-link flash are on BG1 (ui.c). */
static const uint16_t *floor_meta(int d, int x, int y)
{
    const uint16_t (*T)[4] = bm_terrain_meta[g_season];
    static const int grass_v[3] = {T_GRASS, T_GRASS_V2, T_GRASS_V3};
    static const int tunnel_v[3] = {T_TUNNEL, T_TUNNEL_V2, T_TUNNEL_V3};
    int shadow = y > 0 && blocks_light(W.g[d][y - 1][x].t);       /* the block above casts a shadow */
    if (d == 0) return shadow ? T[T_GRASS_SHADOW] : T[grass_v[cell_variant(d, x, y)]];
    return shadow ? T[T_TUNNEL_SHADOW] : T[tunnel_v[cell_variant(d, x, y)]];
}

static int is_water(int d, int x, int y)
{
    if (!in_grid(x, y)) return 0;
    int t = W.g[d][y][x].t;
    return t == TR_WATER || t == TR_BRIDGE;
}

static const uint16_t *ground_meta(int d, int x, int y)
{
    const cell *c = &W.g[d][y][x];
    const uint16_t (*T)[4] = bm_terrain_meta[g_season];
    static const int dirt_v[3] = {T_SOFT_DIRT, T_SOFT_DIRT_V2, T_SOFT_DIRT_V3};
    switch (c->t) {
    case TR_DIRT:                                   /* underground, the soil IS the ground */
        if (d > 0) return c->state ? T[T_DIRT_CRACK] : T[dirt_v[cell_variant(d, x, y)]];
        break;
    case TR_THIN: return T[T_THIN_FLOOR];
    case TR_WATER: case TR_BRIDGE:
        if (y > 0 && !is_water(d, x, y - 1)) return T[T_WATER_EDGE];
        return water_frame() ? T[T_WATER_F2] : T[T_WATER];
    case TR_ICE: return prop_meta[PB_ICE];
    case TR_THIN_ICE: return prop_meta[PB_THIN_ICE + (c->state ? 1 : 0)];
    }
    return floor_meta(d, x, y);
}

/* the object standing on the ground (BG3, transparent around it), or NULL */
static const uint16_t *object_meta(int d, int x, int y)
{
    const cell *c = &W.g[d][y][x];
    const uint16_t (*T)[4] = bm_terrain_meta[g_season];
    switch (c->t) {
    case TR_FLOOR: return (c->state == 7 && c->timer && !c->regrow) ? prop_meta[PB_SPLAT] : NULL;
    case TR_DIRT: return d > 0 ? NULL : c->state ? T[T_DIRT_MOUND_CRACK] : T[T_DIRT_MOUND];   /* surface mound */
    case TR_STONE: return T[T_STONE];
    case TR_ROCK: return T[T_HARD_ROCK];
    case TR_ROOTS: return T[T_ROOTS];
    case TR_FROZEN: return T[T_FROZEN_DIRT];
    case TR_LEAVES: return T[T_LEAVES];
    case TR_PUDDLE: return T[T_PUDDLE];
    case TR_HOLE_DOWN: return T[T_HOLE_DOWN];
    case TR_HOLE_UP: return T[T_HOLE_UP];
    case TR_LADDER: return T[T_LADDER];
    case TR_EXIT: return W.exit_open ? T[T_EXIT_OPEN] : T[T_EXIT_CLOSED];
    case TR_MUD: return prop_meta[PB_MUD];
    case TR_COVER: return prop_meta[g_season == SEASON_SUMMER ? PB_CORN : PB_TALL_GRASS];
    case TR_BURNT: return prop_meta[PB_BURNT];
    case TR_GATE: return prop_meta[PB_GATE + (c->state ? 1 : 0)];
    case TR_PLATE: return prop_meta[PB_PLATE + (c->state ? 1 : 0)];
    case TR_LEVER: return prop_meta[PB_LEVER + (c->state ? 1 : 0)];
    case TR_VENT: return prop_meta[PB_STEAM_VENT + (c->state ? 1 : 0)];
    case TR_PIPE: return prop_meta[PB_PIPE];
    case TR_CRATE: return prop_meta[PB_CRATE];
    case TR_HIVE: return prop_meta[PB_BEEHIVE];
    case TR_GAS: return prop_meta[PB_GAS_POCKET];
    case TR_BRIDGE:
        /* the planks run across the way over the water: water left or right of the bridge means the
           river runs sideways, so you cross it up/down (bridge_v); otherwise left/right (bridge) */
        return (is_water(d, x - 1, y) || is_water(d, x + 1, y)) && !(is_water(d, x, y - 1) || is_water(d, x, y + 1))
                   ? prop_meta[PB_BRIDGE_V] : prop_meta[PB_BRIDGE];
    }
    return NULL;
}

static void put_meta(int layer, int mx, int my, const uint16_t *m)
{
    rs_bg_meta(layer, mx, my & 31, m);
}

static void draw_cell(int d, int slot, int x, int y)
{
    int my = slot * 16 + y;
    put_meta(RS_BG4, x, my, ground_meta(d, x, y));
    uint8_t b = W.blast[d][y][x];
    static const uint16_t none[4] = {0, 0, 0, 0};
    const uint16_t *o = object_meta(d, x, y);
    if (b) {
        int frame = b > 20 ? 0 : b > 10 ? 1 : 2;
        if (b > 26) frame = 0;
        put_meta(RS_BG3, x, my, bm_fx_meta[W.shape[d][y][x] + frame]);
    } else {
        put_meta(RS_BG3, x, my, o ? o : none);
    }
}

static int water_drawn[4];     /* water frame shown in each slot */

void draw_playfield_full(int d, int slot)
{
    const uint16_t (*T)[4] = bm_terrain_meta[g_season];
    static const uint16_t none[4] = {0, 0, 0, 0};
    water_drawn[slot & 3] = water_frame();
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) draw_cell(d, slot, x, y);
    /* the earth between two depths (seen during slides) and the unused columns */
    for (int y = GH; y < 16; y++)
        for (int x = 0; x < 32; x++) {
            put_meta(RS_BG4, x, slot * 16 + y, T[T_SOFT_DIRT]);
            put_meta(RS_BG3, x, slot * 16 + y, ((x + y) % 5) ? none : T[T_HARD_ROCK]);
        }
    for (int y = 0; y < GH; y++)
        for (int x = GW; x < 32; x++) {
            put_meta(RS_BG4, x, slot * 16 + y, T[T_SOFT_DIRT]);
            put_meta(RS_BG3, x, slot * 16 + y, T[T_STONE]);
        }
    memset(W.cell_dirty[d], 0, sizeof W.cell_dirty[d]);
    W.dirty[d] = 0;
}

void draw_cells_dirty(int d, int slot)
{
    /* the water shimmer: redraw the water cells when the frame changes */
    if (water_drawn[slot & 3] != water_frame()) {
        water_drawn[slot & 3] = water_frame();
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++)
                if (is_water(d, x, y)) draw_cell(d, slot, x, y);
    }
    if (!W.dirty[d]) return;
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            if (W.cell_dirty[d][y][x]) {
                W.cell_dirty[d][y][x] = 0;
                draw_cell(d, slot, x, y);
                /* grass shadows and water banks depend on the cell above */
                if (y + 1 < GH) draw_cell(d, slot, x, y + 1);
            }
    W.dirty[d] = 0;
}

/* ---- sprites ------------------------------------------------------------------------------------- */
void spr_draw_pal(int spr, int x, int y, int flags, int prio, int pal)
{
    const bm_sprite_def *s = &bm_spr[spr];
    int tile = s->block ? VR_BOSS + s->tile : s->tile;
    rs_spr(x, y, tile, s->w, s->h, pal, prio, flags ^ (s->hflip ? RS_SPR_HFLIP : 0));   /* right = mirrored left */
}

void spr_draw(int spr, int x, int y, int flags, int prio)
{
    spr_draw_pal(spr, x, y, flags, prio, bm_spr[spr].pal);
}

/* bottom-centre anchor on a 16x16 cell: sprites of 16, 24 or 32 px overlap upwards */
static void spr_cell_pal(int spr, int px, int py, int yoff, int flags, int pal)
{
    const bm_sprite_def *s = &bm_spr[spr];
    spr_draw_pal(spr, px + CELL / 2 - s->w / 2, HUD_H + py + CELL - s->h + yoff, flags, 2, pal);
}

static void spr_cell(int spr, int px, int py, int yoff, int flags)
{
    const bm_sprite_def *s = &bm_spr[spr];
    spr_draw(spr, px + CELL / 2 - s->w / 2, HUD_H + py + CELL - s->h + yoff, flags, 2);
}

static void apx(const actor *a, int *x, int *y)
{
    *x = a->cx * CELL + (a->tx - a->cx) * a->prog / (SUB / CELL);
    *y = a->cy * CELL + (a->ty - a->cy) * a->prog / (SUB / CELL);
}

static int dir_base(int dir, int down, int up, int left, int right)
{
    switch (dir) {
    case DIR_UP: return up;
    case DIR_LEFT: return left;
    case DIR_RIGHT: return right;
    default: return down;
    }
}

static void draw_actor(const actor *a, int yoff)
{
    int x, y, spr = -1, flags = 0;
    apx(a, &x, &y);
    int walk = a->anim / 6;
    switch (a->kind) {
    case AK_MOLE:
        if (a->state == 99) { spr = SPR_MOLE_DEATH + clampi((90 - a->timer) / 23, 0, 3); break; }
        if (a->invul && (a->invul / 4) % 2) return;
        if (a->stun) { spr = SPR_MOLE_HURT; break; }
        if (a->timer) { spr = SPR_MOLE_PLACE_BOMB; break; }
        if (a->dig) {
            spr = dir_base(a->dir, SPR_MOLE_DIG_DOWN, SPR_MOLE_DIG_UP, SPR_MOLE_DIG_LEFT, SPR_MOLE_DIG_RIGHT) + (a->dig / 6) % 2;
            break;
        }
        spr = dir_base(a->dir, SPR_MOLE_WALK_DOWN, SPR_MOLE_WALK_UP, SPR_MOLE_WALK_LEFT, SPR_MOLE_WALK_RIGHT);
        if (a->moving && !a->forced && !a->sliding) spr += (int[]){1, 0, 2, 0}[walk % 4];
        break;
    case AK_FERRET:
        if (a->stun || a->asleep) { spr = SPR_FERRET_STUNNED; break; }
        spr = dir_base(a->dir, SPR_FERRET_WALK_DOWN, SPR_FERRET_WALK_UP, SPR_FERRET_WALK_LEFT, SPR_FERRET_WALK_RIGHT) + walk % 2;
        break;
    case AK_CAT:
        if (a->state == 1 || a->state == 2) {
            spr = SPR_CAT_POUNCE + (a->state == 2);
            if (a->dir == DIR_LEFT) flags = RS_SPR_HFLIP;
            break;
        }
        spr = dir_base(a->dir, SPR_CAT_WALK_DOWN, SPR_CAT_WALK_UP, SPR_CAT_WALK_LEFT, SPR_CAT_WALK_RIGHT) + walk % 2;
        if (a->stun && (a->anim / 4) % 2) return;
        break;
    case AK_DOG:
        if (a->asleep) { spr = SPR_DOG_SLEEP; break; }
        spr = (a->flip ? SPR_DOG_WALK_LEFT : SPR_DOG_WALK_RIGHT) + walk % 2;
        break;
    case AK_BOSS:
        if (a->invul && (a->invul / 4) % 2 && W.def->boss != BOSS_FARMER) return;
        switch (W.def->boss) {
        case BOSS_FARMER:
            if (a->invul && a->dig) spr = SPR_FARMER + 6 + (a->anim / 8) % 2;
            else if (a->timer) spr = SPR_FARMER + 2 + (a->timer < 10);
            else spr = SPR_FARMER + (a->dig ? 4 : 0) + (a->anim / 20) % 2;
            break;
        case BOSS_FOX: spr = SPR_FOX + (a->state == 2 ? 2 : walk % 2); break;
        case BOSS_OWL: spr = SPR_OWL + (a->state == 2 ? 2 : walk % 2); break;
        case BOSS_BADGER: spr = SPR_BADGER + (a->moving ? walk % 2 : 2); break;
        default: spr = SPR_BOSS + (a->state == 1 ? 2 : a->state == 2 ? 3 : a->moving ? 1 : 0); break;
        }
        if (a->dir == DIR_LEFT && W.def->boss == BOSS_FOX) flags = 0;
        else if (a->dir == DIR_RIGHT && W.def->boss == BOSS_FOX) flags = RS_SPR_HFLIP;
        break;
    }
    if (spr < 0) return;
    if (a->kind == AK_FERRET || a->kind == AK_CAT) {
        if (a->invul && (a->invul / 4) % 2) return;     /* hit once (tier 4) */
        spr_cell_pal(spr, x, y, yoff, flags, a->pal);
    } else {
        spr_cell(spr, x, y, yoff, flags);
    }
}

void draw_world_sprites(int d, int yoff, int first)
{
    if (first) rs_oam_clear();
    uint32_t t = W.t;
    /* actors: players first (in front) */
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < W.na; i++) {
            const actor *a = &W.a[i];
            if (!a->alive || a->depth != d || (pass == 0) != (a->kind == AK_MOLE)) continue;
            draw_actor(a, yoff);
        }
    /* tomatoes in flight: the shadow marks the target */
    for (int i = 0; i < MAX_FX; i++) {
        const fxp *f = &W.fx[i];
        if (!f->life || f->kind != FXP_TOMATO) continue;
        int td = f->depth;
        if (td != d) continue;
        int k = 60 - f->life;
        int sx = f->x / 16, sy = f->y / 16, ex = f->vx * CELL, ey = f->vy * CELL;
        int x = sx + (ex - sx) * k / 60, y = sy + (ey - sy) * k / 60 - (k * (60 - k)) / 20;
        spr_cell(SPR_TOMATO_SHADOW, ex, ey, yoff, 0);
        spr_cell(SPR_TOMATO + (k / 6) % 2, x, y, yoff, 0);
    }
    for (int i = 0; i < MAX_BOMBS; i++) {
        const bomb *b = &W.b[i];
        if (!b->active || b->depth != d) continue;
        int x = b->cx * CELL + (b->tx - b->cx) * b->prog / (SUB / CELL);
        int y = b->cy * CELL + (b->ty - b->cy) * b->prog / (SUB / CELL);
        if (b->owner < MAX_PLAYERS && W.ps[b->owner].remote && b->fuse > 4) {
            spr_cell(SPR_BOMB_REMOTE + (t / 12) % 2, x, y, yoff, 0);     /* no fuse: a blinking antenna */
            continue;
        }
        int rate = b->fuse < 40 ? 3 : 8;
        spr_cell(SPR_BOMB + (t / rate) % 3, x, y, yoff, 0);
    }
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            const cell *c = &W.g[d][y][x];
            if (c->item && terrain_walkable(c->t, 0)) {
                static const int spr_of[IT_COUNT] = {0, SPR_GRUB, SPR_PU_BOMB, SPR_PU_FIRE, SPR_PU_SPEED, SPR_PU_REMOTE, SPR_PU_HEART};
                int s = spr_of[c->item] + (c->item == IT_GRUB ? (int)(t / 16 + x) % 2 : 0);
                spr_cell(s, x * CELL, y * CELL - ((t / 12 + x) % 4 == 0 ? 1 : 0), yoff, 0);
            }
            if (c->t == TR_SPRINKLER) spr_cell(SPR_SPRINKLER + c->state, x * CELL, y * CELL, yoff, 0);
            if (c->t == TR_WINDMILL) {
                /* front view when it blows down or up, side view (sails on the side the wind goes) otherwise */
                int speed = world_gust_on() ? 3 : 10, dir = c->blow ? c->blow - 1 : DIR_DOWN;
                if (dir == DIR_LEFT || dir == DIR_RIGHT)
                    spr_cell(SPR_WINDMILL_SIDE + (t / speed) % 4, x * CELL, y * CELL, yoff,
                             dir == DIR_LEFT ? RS_SPR_HFLIP : 0);
                else
                    spr_cell(SPR_WINDMILL + (t / speed) % 4, x * CELL, y * CELL, yoff, 0);
            }
        }
    /* summer: bee swarms, stun gas, harvesters */
    for (int i = 0; i < MAX_SWARMS; i++) {
        const swarm *b = &W.bees[i];
        if (!b->alive || b->depth != d) continue;
        int x = b->cx * CELL + (b->tx - b->cx) * b->prog / (SUB / CELL);
        int y = b->cy * CELL + (b->ty - b->cy) * b->prog / (SUB / CELL);
        spr_cell(SPR_BEES + (int)(t / 4) % 2, x + ((t / 3) % 3) - 1, y - 4, yoff, 0);
    }
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            if (W.gas[d][y][x] && W.gas[d][y][x] <= GAS_TIME)
                spr_cell(SPR_GAS + (int)((t / 10 + x + y) % 2), x * CELL, y * CELL, yoff, ((x + y) & 1) ? RS_SPR_HFLIP : 0);
    for (int i = 0; i < MAX_HARV; i++) {
        const harvester *h = &W.harv[i];
        if (!h->alive || h->depth != d) continue;
        int x = h->cx * CELL, y = h->cy * CELL;
        if (h->state == HV_MOVE && !(h->cx == h->ex && h->cy == h->ey)) {
            x += DX[h->dir] * h->prog / (SUB / CELL);
            y += DY[h->dir] * h->prog / (SUB / CELL);
        }
        int shake = h->state == HV_WARN ? (int)(t / 2) % 2 : 0;
        spr_cell(SPR_HARVESTER + (h->state == HV_MOVE ? (int)(t / 4) % 2 : 0), x + shake, y, yoff,
                 h->dir == DIR_LEFT ? RS_SPR_HFLIP : 0);
    }
    for (int i = 0; i < MAX_LOGS; i++) {
        const logobj *l = &W.logs[i];
        if (!l->alive || l->depth != d) continue;
        int x = l->cx * CELL + (l->tx - l->cx) * l->prog / (SUB / CELL);
        int y = l->cy * CELL + (l->ty - l->cy) * l->prog / (SUB / CELL);
        const bm_sprite_def *s = &bm_spr[SPR_LOG];
        spr_draw(SPR_LOG, x, HUD_H + y + yoff, 0, 1);
        (void)s;
    }
    for (int i = 0; i < MAX_FX; i++) {
        const fxp *f = &W.fx[i];
        if (!f->life || f->depth != d || f->kind == FXP_TOMATO) continue;
        int x = f->x / 16, y = f->y / 16, s, flags = 0;
        switch (f->kind) {
        case FXP_DUST: s = SPR_DUST + clampi(f->t / 8, 0, 2); break;
        case FXP_WIND:          /* streaks drawn blowing right (and down for the vertical ones) */
            if (f->vy) { s = SPR_WIND_V + (f->t / 5) % 3; if (f->vy < 0) flags = RS_SPR_VFLIP; }
            else { s = SPR_WIND + (f->t / 5) % 3; if (f->vx < 0) flags = RS_SPR_HFLIP; }
            break;
        case FXP_SPRAY: case FXP_SPLASH: s = SPR_SPRAY + (f->t / 4) % 2; break;
        case FXP_STEAM: s = SPR_STEAM; break;
        case FXP_ZZZ: s = SPR_ZZZ + (f->t / 10) % 2; break;
        default: s = SPR_DUST + 2; break;
        }
        spr_cell(s, x, y, yoff, flags);
    }
}

/* ---- HUD --------------------------------------------------------------------------------------- */
static void hud_cell(int mx, int m)
{
    rs_bg_meta(RS_BG1, mx, 0, bm_hud_meta[m]);
}

void draw_hud(void)
{
    const pstats *ps = &W.ps[0];
    int pd = world_player_depth(0);
    hud_cell(0, HUD_HEART);   hud_cell(1, HUD_DIGIT + clampi(ps->hearts, 0, 9));
    hud_cell(2, HUD_BOMB);    hud_cell(3, HUD_DIGIT + clampi(ps->bombs, 0, 9));
    hud_cell(4, HUD_FIRE);    hud_cell(5, HUD_DIGIT + clampi(ps->range, 0, 9));
    hud_cell(6, HUD_SPEED);   hud_cell(7, HUD_DIGIT + clampi(ps->speed, 0, 9));
    hud_cell(8, HUD_GRUB);
    int g = clampi(W.grubs_left, 0, 99);
    hud_cell(9, g >= 10 ? HUD_DIGIT + g / 10 : HUD_PANEL);
    hud_cell(10, HUD_DIGIT + g % 10);
    static const int icons[NDEPTH] = {HUD_DEPTH_SURFACE, HUD_DEPTH_UNDER1, HUD_DEPTH_UNDER2};
    for (int d = 0; d < NDEPTH; d++) {
        int x = 11 + d * 3;
        hud_cell(x, d == pd ? HUD_CURSOR : HUD_PANEL);
        int danger = d != pd && world_danger(d, pd);
        int icon = icons[d];
        if (d == pd && (W.t / 16) % 4 == 3) icon = HUD_PANEL;           /* your depth blinks */
        else if (danger && (W.t / 12) % 2) icon = HUD_DANGER;
        hud_cell(x + 1, icon);
        int n = ui_grubs(d);                                         /* grubs left there */
        hud_cell(x + 2, n ? HUD_DIGIT + clampi(n, 0, 9) : HUD_CHECK);
    }
    ui_hud_extras();                    /* the remote's button glyph */
}

/* ---- weather ------------------------------------------------------------------------------------- */
void draw_weather(int on)
{
    rs_bg_enable(RS_BG2, on);
    if (!on) return;
    static const int V[SEASONS][2] = {{-2, 6}, {1, -1}, {2, 2}, {1, 2}};
    uint32_t t = W.t;
    int sx = (int)(t * (uint32_t)(V[g_season][0] + 8)) / 2 - (int)t * 4;
    int sy = -(int)(t * (uint32_t)(V[g_season][1] + 8)) / 2 + (int)t * 4;
    rs_bg_scroll(RS_BG2, sx, sy);
    if (g_season == SEASON_AUTUMN || g_season == SEASON_SUMMER) {
        /* drifting: a per-line wave (HDMA-style) */
        for (int i = 0; i < 240; i++) {
            int ph = (int)((i * 3 + t * 2) % 64);
            weather_dx[i] = (int16_t)((ph < 32 ? ph : 64 - ph) / 4 - 4);
        }
        rs_bg_line_scroll(RS_BG2, weather_dx, NULL);
    } else {
        rs_bg_line_scroll(RS_BG2, NULL, NULL);
    }
}

/* ---- text ------------------------------------------------------------------------------------- */
void text_clear_all(void)
{
    for (int y = 2; y < 30; y++)
        for (int x = 0; x < 40; x++) rs_bg_put(RS_BG1, x, y, 0);
}

void text_box(int x, int y, int w, int h)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) rs_bg_put(RS_BG1, x + i, y + j, RS_MAP(VR_BOX, 0, 1, 0, 0));
    rs_text_setup(RS_BG1, VR_BOX_FONT, 0, 1);
}

void text_at(int x, int y, const char *s) { rs_text(x, y, s); }

void textf_at(int x, int y, const char *fmt, ...)
{
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    rs_text(x, y, buf);
}

void text_big(int x, int y, const char *s)
{
    rs_text_setup(RS_BG1, 0, 0, 1);
    rs_text_big(x, y, s);
}

void draw_frame_setup(void) {}

/* the colour families of the terrain-like props this level uses (also those it can make:
 * burnt cover, mud under puddles, the farmer's tomato splats) -> props A and props B.
 * tools/check_levels.py checks that no level needs more than two. */
static void draw_prop_pals(void)
{
    unsigned need = 0;
#define USE(pb) (need |= 1u << bm_propbg_family[pb])
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++)
                switch (W.g[d][y][x].t) {
                case TR_BRIDGE: USE(PB_BRIDGE); break;
                case TR_ICE: USE(PB_ICE); break;
                case TR_THIN_ICE: USE(PB_THIN_ICE); break;
                case TR_MUD: case TR_PUDDLE: USE(PB_MUD); break;
                case TR_COVER: USE(g_season == SEASON_SUMMER ? PB_CORN : PB_TALL_GRASS); USE(PB_BURNT); break;
                case TR_BURNT: USE(PB_BURNT); break;
                case TR_GATE: USE(PB_GATE); break;
                case TR_PLATE: USE(PB_PLATE); break;
                case TR_LEVER: USE(PB_LEVER); break;
                case TR_VENT: USE(PB_STEAM_VENT); break;
                case TR_PIPE: USE(PB_PIPE); break;
                case TR_CRATE: USE(PB_CRATE); break;
                case TR_HIVE: USE(PB_BEEHIVE); break;
                case TR_GAS: USE(PB_GAS_POCKET); break;
                default: break;
                }
    for (int i = 0; i < W.na; i++)
        if (W.a[i].kind == AK_BOSS && g_boss == BOSS_FARMER) USE(PB_SPLAT);
#undef USE
    int fa = -1, fb = -1;
    for (int f = 0; f < PB_PALS; f++) {
        if (!(need & (1u << f))) continue;
        if (fa < 0) fa = f;
        else if (fb < 0) fb = f;
        else rs_log("level: props of more than two colour families; family %d shares palette %d", f, PAL_PROPS);
    }
    prop_pals_assign(fa < 0 ? 0 : fa, fb);
}

/* the level's palettes after world_start: enemy variants (sprites) and prop families (BG) */
void draw_variant_pals(void)
{
    for (int v = 0; v < VAR_COUNT; v++)
        if (W.var_slot[v] >= 0) load_pal(RS_PAL_OBJ(W.var_slot[v]), bm_variant_pals[v], 16);
    draw_prop_pals();
}
