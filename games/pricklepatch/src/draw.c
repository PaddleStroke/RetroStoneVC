/*
 * Prickle Patch: video. The layers (back to front):
 *   BG4       the garden (BG palette 1: grass, flowers, dirt patches, cabbages) and the XP gems lying on it: an 8x8 cell's
 *             tile is the gem's; the map is anchored on the world and written around the camera (a cell's look comes
 *             from a hash of its world position: the garden never repeats)
 *   sprites   prio 1: the pickups (apples, magnets, coins, chests) under the pests
 *   BG2       the soft layer (soft.c): the swarm, the bosses, the seeds, the puffs, the gems flying, the poofs
 *   sprites   prio 2: the gardener, its health bar, the tools (bees, mowers, rakes, pumpkins, flames, the splash, the
 *             prickle ring), the number pops; OBJ 4 (the splash, the ring, the touch stick, the glow) added at half
 *   BG1       the house kit: the HUD (the XP bar, the timer, the level, the pests chased), the title, the level-up's and
 *             the chest's panels, the shed, the results, PAUSED; prio 3 sprites: the kit's glyphs and digits, the icons
 * Widescreen: the camera stays on the gardener, a wider screen shows more garden. The tall screen (a phone held upright):
 * rs_screen_zoom (main.c), the HUD pinned at the screen's top, the panels and the title in house_tall.h's layouts.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_tall.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

#define GSTR(id) rs_tr(&gs_strings, (id))
#define HUD_ROWS     5              /* BG1's rows 0..4: the HUD (pinned at a tall screen's top) */
#define POPS         16             /* number pops on screen (sprite tiles POP_TILE..) */
#define POP_LIFE     26
#define POOFS        32
#define SPARKS       24
#define WARN_FRAMES  150
#define OAM_BUDGET   124            /* the guideline 128, 4 left for safety */
#define LINE_BUDGET  30             /* sprites per 8-line band (the guideline 32 per line) */

static const rs_color title_ramps[2][3] = {
    {RS_RGB8(240, 196, 140), RS_RGB8(196, 138, 90), RS_RGB8(130, 84, 56)},      /* PRICKLE: the spines */
    {RS_RGB8(196, 236, 120), RS_RGB8(120, 192, 80), RS_RGB8(60, 128, 56)}};     /* PATCH: the leaves */

/* ---- the drawing's state (saved: draw_state) --------------------------------------------------------------------- */
static int laid_w, laid_h, laid_title = -1;     /* the screen the maps were laid out for; the title's layout */
static int ui_key = -1;
static int gx0 = 0x7fffffff, gy0, gx1, gy1;     /* the garden window's cells, written */
static uint16_t gem_cell[MAX_GEM];              /* the map cells showing a gem (restored next frame) */
static int n_gem_cell;
static int cur_chr = -1, cur_map = -1;
typedef struct fxpop { int16_t x, y, t, slot; uint8_t big, pad; } fxpop;
static fxpop pops[POPS];
static int pop_next;
typedef struct poof { int16_t x, y; uint8_t t, kind; } poof;     /* kind 0: a poof, 1: a spark */
static poof poofs[POOFS];
static int poof_next;
static int tick, warn_t, warn_type, lvl_t, hurt_flash, glint_t;
/* per frame (set by draw_frame, read by the raster and main.c's touches: not saved) */
static int r_pin, r_hud;
hot draw_hot[HOT_MAX];
static int band[RS_SCREEN_H_MAX / 8 + 2], oam_n;   /* the sprite budget of the frame (scratch) */

static int tall(void) { return rs_screen_height() > RS_SCREEN_H; }
static int map_w(void) { return rs_screen_width() > 504 ? 128 : 64; }
static int map_h(void) { return rs_screen_height() > 248 ? 64 : 32; }

/* ---- names ---------------------------------------------------------------------------------------------------------- */
const char *weapon_name(int id, int evo)
{
    static const int k[W_COUNT][2] = {
        {GS_W_SEED, GS_E_SEED}, {GS_W_CAN, GS_E_CAN}, {GS_W_BEES, GS_E_BEES}, {GS_W_MOWER, GS_E_MOWER},
        {GS_W_RAKE, GS_E_RAKE}, {GS_W_PUMPKIN, GS_E_PUMPKIN}, {GS_W_THORNS, GS_E_THORNS}, {GS_W_CHILI, GS_E_CHILI},
        {GS_W_PUFF, GS_E_PUFF}};
    return (unsigned)id < W_COUNT ? GSTR(k[id][evo ? 1 : 0]) : "";
}
static const char *weapon_desc(int id, int evo)
{
    static const int k[W_COUNT][2] = {
        {GS_D_SEED, GS_DE_SEED}, {GS_D_CAN, GS_DE_CAN}, {GS_D_BEES, GS_DE_BEES}, {GS_D_MOWER, GS_DE_MOWER},
        {GS_D_RAKE, GS_DE_RAKE}, {GS_D_PUMPKIN, GS_DE_PUMPKIN}, {GS_D_THORNS, GS_DE_THORNS}, {GS_D_CHILI, GS_DE_CHILI},
        {GS_D_PUFF, GS_DE_PUFF}};
    return (unsigned)id < W_COUNT ? GSTR(k[id][evo ? 1 : 0]) : "";
}
/* what level 2..5 of a tool adds (world.c fire(): the same steps) */
static const char *weapon_up(int id, int level)
{
    enum { DM = GS_UP_DAMAGE, MO = GS_UP_MORE, FA = GS_UP_FASTER, AR = GS_UP_AREA, LO = GS_UP_LONGER, PI = GS_UP_PIERCE };
    static const int k[W_COUNT][4] = {
        {MO, DM, MO, DM}, {MO, DM, AR, DM}, {MO, DM, MO, AR}, {DM, FA, MO, DM}, {DM, MO, DM, MO}, {MO, AR, DM, MO},
        {AR, DM, AR, DM}, {DM, LO, AR, FA}, {MO, DM, MO, LO}};
    if ((unsigned)id >= W_COUNT || level < 2 || level > 5) return "";
    return GSTR(k[id][level - 2]);
}
const char *passive_name(int id)
{
    static const int k[P_COUNT] = {GS_P_COMPOST, GS_P_HAT, GS_P_BOOTS, GS_P_HONEY, GS_P_MAGNET, GS_P_SUNDIAL, GS_P_POUCH,
                                   GS_P_CLOVER};
    return (unsigned)id < P_COUNT ? GSTR(k[id]) : "";
}
static const char *passive_desc(int id)
{
    static const int k[P_COUNT] = {GS_DP_COMPOST, GS_DP_HAT, GS_DP_BOOTS, GS_DP_HONEY, GS_DP_MAGNET, GS_DP_SUNDIAL,
                                   GS_DP_POUCH, GS_DP_CLOVER};
    return (unsigned)id < P_COUNT ? GSTR(k[id]) : "";
}
static const char *char_name(int c)
{
    return c == CHR_TORTOISE ? TR(CHAR_TORTOISE) : c == CHR_HEN ? TR(CHAR_HEN) : TR(CHAR_HEDGEHOG);
}
static const int CHAR_TOOL[NCHAR] = {W_SEED, W_CAN, W_BEES};

int choice_icon(const choice *c)
{
    static const int wi[W_COUNT] = {SPR_ICON_SEED, SPR_ICON_CAN, SPR_QUEEN_BEE, SPR_MOWER, SPR_RAKE, SPR_PUMPKIN,
                                    SPR_ICON_THORNS, SPR_ICON_CHILI, SPR_ICON_DANDELION};
    static const int pi[P_COUNT] = {SPR_I_COMPOST, SPR_I_HAT, SPR_I_BOOTS, SPR_I_HONEY, SPR_MAGNET, SPR_I_SUNDIAL,
                                    SPR_I_POUCH, SPR_I_CLOVER};
    switch (c->kind) {
    case CH_WNEW:
    case CH_WUP: return wi[c->id % W_COUNT];
    case CH_EVOLVE:
        if (c->id == W_SEED) return SPR_ICON_SEED + 1;
        if (c->id == W_CAN) return SPR_ICON_RAINBOW;
        if (c->id == W_THORNS) return SPR_ICON_THORNS + 1;
        if (c->id == W_CHILI) return SPR_ICON_CHILI + 1;
        if (c->id == W_PUFF) return SPR_ICON_DANDELION + 1;
        return wi[c->id % W_COUNT];
    case CH_PNEW:
    case CH_PUP: return pi[c->id % P_COUNT];
    case CH_HEAL: return SPR_APPLE;
    default: return SPR_COIN;
    }
}
static void choice_texts(const choice *c, const char **name, const char **tag, const char **desc, char *buf, int n)
{
    *tag = "";
    switch (c->kind) {
    case CH_WNEW: *name = weapon_name(c->id, 0); *desc = weapon_desc(c->id, 0); *tag = TR(NEW); break;
    case CH_WUP:
        *name = weapon_name(c->id, 0);
        *desc = weapon_up(c->id, c->level);
        rs_strf(buf, (size_t)n, TR(HUD_LV), c->level);
        *tag = buf;
        break;
    case CH_EVOLVE: *name = weapon_name(c->id, 1); *desc = weapon_desc(c->id, 1); *tag = TR(EVOLVE); break;
    case CH_PNEW: *name = passive_name(c->id); *desc = passive_desc(c->id); *tag = TR(NEW); break;
    case CH_PUP:
        *name = passive_name(c->id);
        *desc = passive_desc(c->id);
        rs_strf(buf, (size_t)n, TR(HUD_LV), c->level);
        *tag = buf;
        break;
    case CH_HEAL: *name = TR(HEAL); *desc = TR(HEAL_D); break;
    default: *name = TR(COINS); *desc = TR(COINS_D); break;
    }
}

/* ---- sprites, within the guidelines (128, 32 a line) ------------------------------------------------------------------ */
static void budget_reset(void)
{
    memset(band, 0, sizeof band);
    oam_n = 0;
}
/* a sprite at stage (x, y): drawn only while the frame's budget allows (the most important are drawn first) */
static int spr_raw(int x, int y, int tile, int w, int h, int pal, int prio, int flags)
{
    if (x <= rs_screen_left() - w || x >= rs_screen_right() || y <= rs_screen_top() - h || y >= rs_screen_bottom())
        return 0;
    if (oam_n >= OAM_BUDGET) return 0;
    int b0 = (y - rs_screen_top()) >> 3, b1 = (y + h - 1 - rs_screen_top()) >> 3, nb = (int)(sizeof band / sizeof band[0]);
    if (b0 < 0) b0 = 0;
    if (b1 >= nb) b1 = nb - 1;
    for (int b = b0; b <= b1; b++)
        if (band[b] >= LINE_BUDGET) return 0;
    for (int b = b0; b <= b1; b++) band[b]++;
    oam_n++;
    rs_spr(x, y, tile, w, h, pal, prio, flags);
    return 1;
}
static void spr(int id, int x, int y, int prio, int flags)
{
    const pp_sprite_def *d = &pp_spr[id];
    spr_raw(x, y, d->tile, d->w, d->h, d->pal, prio, flags);
}
/* the kit draws its own sprites (digits, glyphs): count them in */
static void budget_kit(int x, int y, int w, int h)
{
    int b0 = (y - rs_screen_top()) >> 3, b1 = (y + h - 1 - rs_screen_top()) >> 3, nb = (int)(sizeof band / sizeof band[0]);
    if (b0 < 0) b0 = 0;
    if (b1 >= nb) b1 = nb - 1;
    for (int b = b0; b <= b1; b++) band[b]++;
    oam_n++;
    (void)x;
}

/* ---- the raster: the pinned UI on a tall screen ---------------------------------------------------------------------- */
static void raster(int line, void *user)
{
    (void)user;
    if (r_pin) rs_bg_enable(RS_BG1, ht_pin_line(r_pin, line));
    else if (r_hud) rs_bg_enable(RS_BG1, line < rs_screen_top() + HUD_ROWS * 8 + 8);
    else rs_bg_enable(RS_BG1, 1);
}

/* ---- set-up ----------------------------------------------------------------------------------------------------------- */
/* the HUD's tiles: the bars' ends and their fills (0..8 eighths) in the UI palette */
static void hud_tiles(void)
{
    uint8_t t[64];
    for (int k = 0; k < 20; k++) {
        memset(t, 0, sizeof t);
        for (int y = 1; y < 7; y++)
            for (int x = 0; x < 8; x++) {
                int v = 0;
                if (k == 0) v = x == 7 ? HU_OUTLINE : 0;                      /* the left end */
                else if (k == 1) v = x == 0 ? HU_OUTLINE : 0;                 /* the right end */
                else {
                    int fill = (k - 2) % 9, boss = k >= 11;
                    v = y == 1 || y == 6 ? HU_OUTLINE : x < fill ? (boss ? HU_RED : (y == 2 ? HU_CREAM : HU_LIGHT)) : HU_DARK;
                }
                t[y * 8 + x] = (uint8_t)v;
            }
        if (k == 0) t[7] = t[63] = 0;
        rs_tiles_load8(VR_BG1 + UI_TILE + k, t, 1);
    }
}

static void layers_setup(void)
{
    int mw = map_w(), mh = map_h();
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG4, mw, mh, VR_GROUND);
    soft_setup(mw, mh);
    rs_bg_enable(RS_BG3, 0);
    laid_w = rs_screen_width();
    laid_h = rs_screen_height();
    gx0 = 0x7fffffff;
    n_gem_cell = 0;
    ui_key = -1;
}

static void load_char(int c)
{
    if (c == cur_chr) return;
    rs_tiles_load(VR_OBJ, pp_hero_tiles[c], HERO_TILES);
    rs_pal_load(RS_PAL_OBJ(OBJ_HERO), pp_pal_hero[c], 16);
    cur_chr = c;
}
static void load_map(int m)
{
    if (m == cur_map) return;
    rs_pal_load(RS_PAL_BG(PAL_GROUND), m ? pp_pal_ground_night : pp_pal_ground, 16);
    rs_backdrop(m ? RS_RGB8(14, 20, 40) : RS_RGB8(70, 128, 62));
    cur_map = m;
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());              /* the stage centred: the gardener in the screen's middle */
    hu_config hc = hu_defaults();
    hc.obj_tile = KIT_OBJ_TILE;
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = OBJ_KIT;
    hc.logo_tile = LOGO_TILE;
    hc.ext_tile = EXT_TILE;
    hc.ext_count = EXT_COUNT;
    hu_init(&hc);
    layers_setup();
    hud_tiles();
    rs_tiles_load(VR_GROUND, pp_ground_tiles, GT_COUNT);
    rs_pal_load(RS_PAL_BG(PAL_SOFT), pp_pal_soft, 16);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, pp_obj_tiles, pp_obj_tile_count);
    static const uint8_t blank[64];
    for (int i = 0; i < POP_TILES; i++) rs_tiles_load8(VR_OBJ + POP_TILE + i, blank, 1);
    rs_pal_load(RS_PAL_OBJ(OBJ_TOOLS), pp_pal_tools, 16);
    rs_pal_load(RS_PAL_OBJ(OBJ_ITEMS), pp_pal_items, 16);
    rs_pal_load(RS_PAL_OBJ(OBJ_FX), pp_pal_fx, 16);
    rs_math(RS_MATH_ADD | RS_MATH_HALF, RS_MATH_OBJ, 0);   /* OBJ 4-7: see-through */
    rs_bg_enable(RS_BG1, 1);
    rs_bg_enable(RS_BG2, 1);
    rs_bg_enable(RS_BG4, 1);
    rs_raster(raster, NULL);
    load_char(0);
    load_map(0);
}

void draw_state_loaded(void)
{
    int shape = ht_shape_reset();
    if (!shape && laid_w == rs_screen_width() && laid_h == rs_screen_height()) return;
    layers_setup();
    rs_bg_enable(RS_BG3, 0);
    hu_pause(0, 13);
}

void draw_reset_run(void)
{
    memset(pops, 0, sizeof pops);
    memset(poofs, 0, sizeof poofs);
    warn_t = lvl_t = hurt_flash = 0;
}

/* ---- the garden ------------------------------------------------------------------------------------------------------ */
static uint32_t hsh(int a, int b)
{
    uint32_t v = 2166136261u;
    v = (v ^ (uint32_t)a) * 16777619u;
    v = (v ^ (uint32_t)b) * 16777619u;
    v ^= v >> 13;
    v *= 0x5bd1e995u;
    return v ^ (v >> 15);
}
/* the garden's tile of world cell (cx, cy): grass, now and then a flower, a tuft, a pebble, a dirt patch, a cabbage */
static int ground_tile(int cx, int cy)
{
    uint32_t b = hsh(cx >> 1, (cy >> 1) + 7777) % 1000;
    int q = (cx & 1) + (cy & 1) * 2;
    if (b < 5) return GT_DIRT0 + q;
    if (b < 9) return GT_CABBAGE0 + q;
    uint32_t h = hsh(cx, cy) % 1000;
    if (h < 12) return GT_FLOWER0 + (int)(h % 3);      /* (the violet flower stays out: the gems are violet) */
    if (h < 46) return GT_TUFT0 + (int)(h & 1);
    if (h < 52) return GT_PEBBLE0 + (int)(h & 1);
    return GT_GRASS0 + (int)((h >> 3) & 3);
}

static void garden(const world *w, int cam_x, int cam_y)
{
    int mw = map_w(), mh = map_h();
    uint16_t *map = rs_bg_map(RS_BG4);
    int x0 = (cam_x + rs_screen_left()) >> 3, x1 = (cam_x + rs_screen_right() - 1) >> 3;
    int y0 = (cam_y + rs_screen_top()) >> 3, y1 = (cam_y + rs_screen_bottom() - 1) >> 3;
    if (x0 != gx0 || y0 != gy0 || x1 != gx1 || y1 != gy1) {   /* a cell crossed: the whole window again */
        for (int cy = y0; cy <= y1; cy++)
            for (int cx = x0; cx <= x1; cx++)
                map[(cy & (mh - 1)) * mw + (cx & (mw - 1))] = RS_MAP(ground_tile(cx, cy), PAL_GROUND, 0, 0, 0);
        gx0 = x0;
        gy0 = y0;
        gx1 = x1;
        gy1 = y1;
        n_gem_cell = 0;
    } else {                                            /* else the gems of the last frame give their cells back */
        for (int i = 0; i < n_gem_cell; i++) {
            int m = gem_cell[i], cx = m % mw, cy = m / mw;
            /* the world cell shown there (the window is narrower than the map: one only) */
            int wx = x0 + ((cx - x0) & (mw - 1)), wy = y0 + ((cy - y0) & (mh - 1));
            map[m] = RS_MAP(ground_tile(wx, wy), PAL_GROUND, 0, 0, 0);
        }
        n_gem_cell = 0;
    }
    for (int i = 0; i < w->n_gm; i++) {
        const gem *g = &w->gm[i];
        if (g->fly) continue;
        int cx = (int)(g->x / FX) >> 3, cy = (int)(g->y / FX) >> 3;
        if (cx < x0 || cx > x1 || cy < y0 || cy > y1) continue;
        int m = (cy & (mh - 1)) * mw + (cx & (mw - 1));
        map[m] = RS_MAP(g->val >= 20 ? GT_GEM2 : g->val >= 5 ? GT_GEM1 : GT_GEM0, PAL_GROUND, 0, 0, 0);
        gem_cell[n_gem_cell++] = (uint16_t)m;
    }
    rs_bg_scroll(RS_BG4, cam_x & (mw * 8 - 1), cam_y & (mh * 8 - 1));
    /* the gems' glint twinkles */
    int k = (glint_t / 6) % 8;
    rs_pal_set(RS_PAL_BG(PAL_GROUND) + 15, k < 2 ? RS_RGB8(255, 255, 255) : k < 4 ? RS_RGB8(200, 200, 255)
                                            : (cur_map ? RS_RGB8(116, 98, 226) : RS_RGB8(106, 86, 206)));
}

/* ---- effects (draw_events: after each world step) ------------------------------------------------------------------ */
static const uint8_t DIGITS[10][5] = {   /* a 3x5 font: rows, 3 bits */
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 3, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};

static void pop_tiles(int slot, int n, int big)
{
    uint8_t t[2][64], m[8][16];
    memset(m, 0, sizeof m);
    char s[8];
    snprintf(s, sizeof s, "%d", n > 9999 ? 9999 : n);
    int len = (int)strlen(s), x = (16 - (len * 4 - 1)) / 2;
    for (int i = 0; i < len; i++, x += 4)
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 3; c++)
                if (DIGITS[s[i] - '0'][r] & (4 >> c)) m[r + 1][x + c] = big ? 3 : 2;    /* gold or white */
    for (int y = 0; y < 8; y++)                         /* the outline */
        for (int xx = 0; xx < 16; xx++) {
            if (m[y][xx] && m[y][xx] != 1) continue;
            int near = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int yy = y + dy, xq = xx + dx;
                    if (yy >= 0 && yy < 8 && xq >= 0 && xq < 16 && m[yy][xq] > 1) near = 1;
                }
            if (near) m[y][xx] = 1;
        }
    for (int y = 0; y < 8; y++)
        for (int xx = 0; xx < 16; xx++) t[xx / 8][y * 8 + (xx & 7)] = m[y][xx];
    rs_tiles_load8(VR_OBJ + POP_TILE + slot * 2, t[0], 2);
}

void draw_events(const world *w)
{
    for (int i = 0; i < w->n_pops; i++) {
        const pop *p = &w->pops[i];
        int s = pop_next;
        pop_next = (pop_next + 1) % POPS;
        pops[s] = (fxpop){p->x, p->y, POP_LIFE, (int16_t)s, p->big, 0};
        pop_tiles(s, p->n, p->big);
    }
    for (int i = 0; i < w->n_kfx; i++) {
        const killfx *k = &w->kfx[i];
        poofs[poof_next] = (poof){k->x, k->y, 9, 0};
        poof_next = (poof_next + 1) % POOFS;
        if (k->type >= ET_BOSS0) {                      /* a boss: a ring of poofs, sparks rising all round */
            static const int8_t R[12][2] = {{28, 0}, {24, 14}, {14, 24}, {0, 28}, {-14, 24}, {-24, 14}, {-28, 0},
                                            {-24, -14}, {-14, -24}, {0, -28}, {14, -24}, {24, -14}};
            for (int j = 0; j < 12; j++) {
                poofs[poof_next] = (poof){(int16_t)(k->x + R[j][0]), (int16_t)(k->y + R[j][1] * 3 / 4),
                                          (uint8_t)(j & 1 ? 9 : 30 + j), (uint8_t)(j & 1 ? 0 : 1)};
                poof_next = (poof_next + 1) % POOFS;
            }
        } else if (k->elite) {
            for (int j = 0; j < 8; j++) {
                poofs[poof_next] = (poof){(int16_t)(k->x + (j % 3 - 1) * 14), (int16_t)(k->y + (j / 3 - 1) * 12),
                                          (uint8_t)(18 + j), 1};
                poof_next = (poof_next + 1) % POOFS;
            }
        }
    }
    if (w->events & EV_LEVEL) lvl_t = 24;
    if (w->events & EV_HURT) { hu_shake(2, 6); hurt_flash = 8; }
    if (w->events & EV_BOSSKILL) hu_shake(4, 16);
    if (w->events & EV_BOOM) hu_shake(1, 4);
    if (w->warn) { warn_t = WARN_FRAMES; warn_type = w->warn - 1; ui_key = -1; }
    for (int i = 0; i < POPS; i++) if (pops[i].t) pops[i].t--;
    for (int i = 0; i < POOFS; i++) if (poofs[i].t) poofs[i].t--;
    if (warn_t) warn_t--;
    if (lvl_t) lvl_t--;
    if (hurt_flash) hurt_flash--;
}

/* ---- the world's pictures ----------------------------------------------------------------------------------------------- */
static void draw_swarm(const world *w, int cam_x, int cam_y)
{
    /* the window: the columns the display shows (a zoomed tall screen crops its sides), the screen's lines */
    int x0 = cam_x + rs_screen_visible_x0() - rs_screen_origin_x(), x1 = cam_x + rs_screen_visible_x1() - rs_screen_origin_x();
    soft_begin(cam_x, cam_y, x0 - 2, cam_y + rs_screen_top(), x1 + 2, cam_y + rs_screen_bottom());
    /* soft_draw paints under what is there: the first drawn is in front. The poofs and the sparks, the gems flying, the
     * shots, then the pests from the nearest to the gardener to the farthest: when a crowd fills the pool, what is
     * left out is far away, at the screen's edge */
    for (int i = 0; i < POOFS; i++) {
        const poof *p = &poofs[i];
        if (!p->t) continue;
        if (p->kind == 0) soft_draw(SF_POOF + (9 - p->t) / 3, p->x - 8, p->y - 8, 0, 0);
        else if (p->t < 16) soft_draw(SF_SPARK + ((p->t / 4) & 1), p->x - 4, p->y - 4 - (16 - p->t), 0, 0);
    }
    for (int i = 0; i < w->n_gm; i++) {
        const gem *g = &w->gm[i];
        if (!g->fly) continue;
        soft_draw(SF_GEM + (g->val >= 20 ? 2 : g->val >= 5 ? 1 : 0), (int)(g->x / FX) - 4, (int)(g->y / FX) - 4, 0, 0);
    }
    for (int i = 0; i < w->n_sh; i++) {
        const shot *s = &w->sh[i];
        int x = (int)(s->x / FX), y = (int)(s->y / FX);
        if (s->kind == SK_SEED) soft_draw(SF_SEED, x - 3, y - 3, 0, 0);
        else if (s->kind == SK_SUNSEED) soft_draw(SF_SEED + 1, x - 3, y - 3, 0, 0);
        else if (s->kind == SK_PUFF) soft_draw(SF_PUFF + ((s->t / 12) & 1), x - 5, y - 5, 0, 0);
    }
    int16_t head[64], nxt[MAX_EN];                      /* the pests by rings of 8 px round the gardener */
    for (int i = 0; i < 64; i++) head[i] = -1;
    int hx = (int)(w->h.x / FX), hy = (int)(w->h.y / FX);
    for (int i = w->n_en - 1; i >= 0; i--) {
        const enemy *e = &w->en[i];
        int dx = (int)(e->x / FX) - hx, dy = (int)(e->y / FX) - hy;
        dx = dx < 0 ? -dx : dx;
        dy = dy < 0 ? -dy : dy;
        int r = (dx > dy ? dx + dy * 3 / 8 : dy + dx * 3 / 8) >> 3;
        if (r > 63) r = 63;
        nxt[i] = head[r];
        head[r] = (int16_t)i;
    }
    for (int r = 0; r < 64; r++)
        for (int i = head[r]; i >= 0; i = nxt[i]) {
            const enemy *e = &w->en[i];
            const enemy_info *in = &EN_INFO[e->type];
            int boss = e->type >= ET_BOSS0;
            int fr = in->frame + ((e->anim / (boss ? 16 : 10)) & 1);
            const pp_soft_def *d = &pp_soft[fr];
            int x = (int)(e->x / FX) - d->fw / 2, y = (int)(e->y / FX) - d->fh / 2 - (boss ? 6 : 1);
            if (e->type == ET_STAG && e->st == 1) x += (e->st_t & 2) ? 1 : -1;          /* the charge's warning */
            soft_draw(fr, x, y, e->face, e->flash ? REMAP_FLASH : e->elite ? REMAP_ELITE : REMAP_NONE);
        }
    soft_end();
}

static void draw_tools(const world *w, int cam_x, int cam_y)
{
    for (int i = 0; i < w->n_wp; i++) {                 /* the bees */
        const weapon *p = &w->wp[i];
        if (p->id != W_BEES) continue;
        int n = (p->evo ? 6 : p->level >= 5 ? 4 : p->level >= 4 ? 3 : p->level >= 2 ? 2 : 1) + w->amount;
        for (int k = 0; k < n; k++) {
            int x, y;
            bees_pos(w, p, k, n, &x, &y);
            if (p->evo && k == 0) spr(SPR_QUEEN_BEE + ((tick / 3) & 1), x - 8 - cam_x, y - 8 - cam_y, 2, 0);
            else spr(SPR_BEE + ((tick / 3 + k) & 1), x - 4 - cam_x, y - 4 - cam_y, 2, 0);
        }
    }
    for (int i = 0; i < w->n_sh; i++) {
        const shot *s = &w->sh[i];
        int x = (int)(s->x / FX) - cam_x, y = (int)(s->y / FX) - cam_y;
        switch (s->kind) {
        case SK_MOWER: spr(SPR_MOWER + ((s->t / 3) & 1), x - 8, y - 8, 2, s->vx < 0 ? RS_SPR_HFLIP : 0); break;
        case SK_RAKE: spr(SPR_RAKE + ((s->t / 3) & 3), x - 8, y - 8, 2, 0); break;
        case SK_PUMPKIN: {
            int life0 = 36, k = s->t, hgt = k * (life0 - k) * 4 / (life0 * life0 / 8);
            spr(SPR_PUMPKIN, x - 8, y - 10 - hgt, 2, 0);
            spr(SPR_RINGDOT + 1, x - 4, y - 2, 2, 0);
            break;
        }
        case SK_BLAST: {
            int f = s->t < 2 ? 0 : s->t < 4 ? 1 : 2, sc = s->aux2 ? 1 : 0;
            (void)sc;
            spr(SPR_BLAST + f, x - 16, y - 16, 2, 0);
            spr(SPR_BLAST + f, x, y - 16, 2, RS_SPR_HFLIP);
            spr(SPR_BLAST + f, x - 16, y, 2, RS_SPR_VFLIP);
            spr(SPR_BLAST + f, x, y, 2, RS_SPR_HFLIP | RS_SPR_VFLIP);
            break;
        }
        case SK_FLAME: spr(SPR_FLAME + ((s->t / 5 + i) % 3), x - 8, y - 13, 2, 0); break;
        case SK_SPLASH:
            if (s->aux2) {                              /* the rainbow spray: the splash all round */
                for (int a = 0; a < 4; a++) {
                    int dx = a == 0 ? 1 : a == 1 ? -1 : 0, dy = a == 2 ? 1 : a == 3 ? -1 : 0;
                    spr(SPR_SPLASH + ((s->t / 3) & 1), x + dx * 28 - 16, y + dy * 28 - 8, 2,
                        (dx < 0 ? RS_SPR_HFLIP : 0) | (dy < 0 ? RS_SPR_VFLIP : 0));
                }
            } else {
                spr(SPR_SPLASH + ((s->t / 3) & 1), x - 16, y - 10, 2, s->aux < 0 ? RS_SPR_HFLIP : 0);
            }
            break;
        case SK_THORNRING: {                            /* the pulse: thorns out on the ring */
            int r = s->r * (11 - s->life) / 10;
            for (int a = 0; a < 12; a++) {
                static const int8_t C12[12] = {64, 55, 32, 0, -32, -55, -64, -55, -32, 0, 32, 55};
                int ax = C12[a], ay = C12[(a + 9) % 12];
                spr(SPR_THORN + (a & 1), x + ax * r / 64 - 4, y + ay * r / 64 - 4, 2, 0);
            }
            break;
        }
        default: break;
        }
    }
}

static void draw_items(const world *w, int cam_x, int cam_y)
{
    for (int i = 0; i < w->n_it; i++) {
        const item *it = &w->it[i];
        int x = (int)(it->x / FX) - cam_x, y = (int)(it->y / FX) - cam_y, bob = hu_bob(tick + i * 13, 48, 1);
        if (it->t > 50 * SEC && (tick & 4)) continue;  /* about to go: blinks */
        switch (it->kind) {
        case IT_APPLE: spr(SPR_APPLE, x - 8, y - 10 + bob, 1, 0); break;
        case IT_MAGNET: spr(SPR_MAGNET, x - 8, y - 10 + bob, 1, 0); break;
        case IT_COIN: spr(SPR_COIN + (tick / 6 + i) % 4, x - 4, y - 6, 1, 0); break;
        case IT_CHEST:
            spr(SPR_GLOW + ((tick / 8) & 1), x - 8, y - 9, 1, 0);
            spr(SPR_CHEST, x - 8, y - 10 + bob, 1, 0);
            break;
        }
    }
}

static void draw_hero(const world *w, int cam_x, int cam_y, int title_x, int title_y)
{
    const hero *h = &w->h;
    int x = title_x ? title_x : (int)(h->x / FX) - cam_x - 12, y = title_y ? title_y : (int)(h->y / FX) - cam_y - 16;
    if (!title_x && w->dead) {                          /* down: on its back, feet up */
        spr(SPR_HERO, x, y + 4, 2, (h->face ? RS_SPR_HFLIP : 0) | RS_SPR_VFLIP);
        return;
    }
    if (!title_x && h->hurt_t && (h->hurt_t & 4)) return;          /* safe after a hit: blinks */
    int f = h->moving ? (h->walk_t / 7) % 4 : (tick / 30) % 2 ? 1 : 0;
    spr(SPR_HERO + f, x, y, 2, h->face ? RS_SPR_HFLIP : 0);
    if (title_x)                                        /* the title: two bees round the gardener */
        for (int k = 0; k < 2; k++) {
            static const int8_t C16[16] = {0, 8, 14, 18, 20, 18, 14, 8, 0, -8, -14, -18, -20, -18, -14, -8};
            int a = (tick / 4 + k * 8) & 15;
            spr(SPR_BEE + ((tick / 3 + k) & 1), x + 8 + C16[a] * 3 / 2, y + 6 + C16[(a + 4) & 15] / 2, 2, 0);
        }
}

static void draw_hpbar(const world *w, int cam_x, int cam_y)
{
    const hero *h = &w->h;
    int x = (int)(h->x / FX) - cam_x - 8, y = (int)(h->y / FX) - cam_y + 9;
    int k = h->hpmax ? (int)(h->hp * 16 / h->hpmax) : 0;
    if (h->hp > 0 && k < 1) k = 1;
    spr(SPR_HPBAR + (k > 8 ? 8 : k), x, y, 2, 0);
    spr(SPR_HPBAR + (k > 8 ? k - 8 : 0), x + 8, y, 2, 0);
}

static void draw_pops(int cam_x, int cam_y, int dy0)
{
    for (int i = 0; i < POPS; i++) {
        const fxpop *p = &pops[i];
        if (!p->t) continue;
        int age = POP_LIFE - p->t, rise = age < 10 ? age : 10 + (age - 10) / 3;
        spr_raw(p->x - cam_x - 8, p->y - cam_y - rise + dy0, POP_TILE + p->slot * 2, 16, 8, OBJ_ITEMS, 2, 0);
    }
}

/* ---- the HUD ------------------------------------------------------------------------------------------------------------------ */
static void fmt_time(char *s, int n, int frames)
{
    int sec = frames / SEC;
    snprintf(s, (size_t)n, "%02d:%02d", sec / 60 % 100, sec % 60);
}

static void hud_bar(int row, int c0, int c1, int num, int den, int boss)
{
    int n = c1 - c0 - 2;
    if (n < 1) return;
    long fill = den > 0 ? (long)num * n * 8 / den : 0;
    rs_bg_put(RS_BG1, c0 & (rs_bg_map_w(RS_BG1) - 1), row, RS_MAP(UI_TILE + 0, PAL_UI, 1, 0, 0));
    rs_bg_put(RS_BG1, (c1 - 1) & (rs_bg_map_w(RS_BG1) - 1), row, RS_MAP(UI_TILE + 1, PAL_UI, 1, 0, 0));
    for (int i = 0; i < n; i++) {
        long f = fill - i * 8;
        int k = f <= 0 ? 0 : f >= 8 ? 8 : (int)f;
        rs_bg_put(RS_BG1, (c0 + 1 + i) & (rs_bg_map_w(RS_BG1) - 1), row,
                  RS_MAP(UI_TILE + 2 + (boss ? 9 : 0) + k, PAL_UI, 1, 0, 0));
    }
}

/* the UI layer's columns of the HUD: the visible ones (narrow), else the kit's 40 */
static void hud_cols(int narrow, int *c0, int *c1)
{
    if (narrow) ht_cols(c0, c1);
    else { *c0 = 0; *c1 = 40; }
}

static void hud_text(const world *w, int narrow)
{
    char s[32];
    int c0, c1;
    hud_cols(narrow, &c0, &c1);
    hu_clear_rows(1, HUD_ROWS - 1);
    fmt_time(s, sizeof s, w->t);
    hu_big((c0 + c1 - hu_width(s, 1)) / 2, 1, s, HU_BIG_FREE);
    rs_strf(s, sizeof s, TR(HUD_LV), w->level);
    int lr = narrow ? 3 : 1;                             /* narrow: under the timer, clear of the corner buttons */
    hu_text(c0 + (narrow ? 1 : 1), lr, s);
    snprintf(s, sizeof s, "%d", w->kills);
    hu_text(c1 - 1 - hu_width(s, 0), lr, s);
}

/* ---- the panels -------------------------------------------------------------------------------------------------------------- */
static int ui_screen_y(int row, int pin)
{
    int base = pin && tall() ? ht_pin_screen_y(pin) : rs_screen_origin_y();
    return base + row * 8;
}
static void hot_set(int k, int row0, int row1, int pin)
{
    if (k < 0 || k >= HOT_MAX) return;
    draw_hot[k].y0 = ui_screen_y(row0, pin);
    draw_hot[k].y1 = ui_screen_y(row1 + 1, pin);
    draw_hot[k].x0 = 0;
    draw_hot[k].x1 = rs_screen_width();
}

/* the level-up's (or the chest's) panel: the banner (landscape: under the HUD's rows), then each choice in 5 rows (6
 * upright: its line may wrap): its icon, its name and tag (NEW!, LV 3, EVOLUTION!: at the name's end, or the line's
 * when the name is long), its line */
static int ch_base(int narrow) { return narrow ? 0 : 2; }
static int ch_step(int narrow) { return narrow ? 6 : 5; }
static int ch_row(int narrow, int i) { return 10 + ch_base(narrow) + ch_step(narrow) * i; }
static void ch_box(int narrow, int *x, int *w)
{
    *w = narrow ? ht_ncols() : 32;
    *x = narrow ? ht_center_w(*w) : (40 - *w) / 2;
}
static void choices_panel(const choice *c, int n, const char *title, int narrow)
{
    char tb[32], l[HT_MAXL][HT_LINE];
    int x, w, b = ch_base(narrow);
    ch_box(narrow, &x, &w);
    if (narrow) ht_banner(4 + b, title);
    else hu_banner(4 + b, title);
    hu_panel(x, 8 + b, w, ch_step(narrow) * n + 3);
    for (int i = 0; i < n; i++) {
        const char *name, *tag, *desc;
        choice_texts(&c[i], &name, &tag, &desc, tb, sizeof tb);
        int r = ch_row(narrow, i), tx = x + 4, right = x + w - 1, nw = hu_width(name, 0);
        int nx = tx + nw > right ? right - nw : tx;
        hu_box_text(nx < x + 1 ? x + 1 : nx, r, name);
        /* the line: from the name's column, right-aligned when longer, wrapped in two upright when it must */
        int nl = 1, dw = hu_width(desc, 0);
        if (tx + dw > right && narrow) nl = ht_wrap_bal(desc, right - (x + 1), l, 2);
        if (nl < 2) { snprintf(l[0], HT_LINE, "%s", desc); nl = 1; }
        int last_end = 0;
        for (int k = 0; k < nl; k++) {
            int lw = hu_width(l[k], 0), dx = tx + lw > right ? right - lw : tx;
            if (dx < x + 1) dx = x + 1;
            hu_box_text(dx, r + 2 + k * (hu_tall(l[k]) ? 2 : 1), l[k]);       /* (accents: a row between) */
            last_end = dx + lw;
        }
        if (tag[0]) {
            int tw = hu_width(tag, 0);
            if (nx + nw + 1 + tw <= right) hu_box_text(right - tw, r, tag);
            else if (nl == 1 && last_end + 1 + tw <= right) hu_box_text(right - tw, r + 2, tag);
        }
    }
}

static void choices_sprites(const choice *c, int n, int sel, int narrow, int pin, int show_cursor)
{
    int x, w;
    ch_box(narrow, &x, &w);
    int ux = hu_ui_x(), dy = pin && tall() ? ht_pin_dy(pin) : 0;
    for (int i = 0; i < n; i++) {
        int r = ch_row(narrow, i);
        spr(choice_icon(&c[i]), (x + 1) * 8 + ux + 2, r * 8 - 3 + dy, 3, 0);
        if (c[i].kind == CH_EVOLVE) spr(SPR_I_STAR, (x + 1) * 8 + ux + 12, r * 8 - 7 + dy, 3, 0);
        if (show_cursor && i == sel) {                  /* the choice under the cursor: arrows on both sides */
            int b = (tick / 8) & 1;
            spr(SPR_CURSOR + b, x * 8 + ux - 7, r * 8 + dy + 4, 3, 0);
            spr(SPR_CURSOR + b, (x + w) * 8 + ux - 1, r * 8 + dy + 4, 3, RS_SPR_HFLIP);
        }
    }
}

/* the title's lines below the logo: the gardener (arrows), its tool or price, the best run, the run in progress,
 * the shed */
static int title_line(const view *v, int k, char *s, int n)
{
    char t[24], u[64];
    switch (k) {
    case 0: snprintf(s, (size_t)n, "%s %s %s", HU_ARROW_LEFT, char_name(v->chr), HU_ARROW_RIGHT); return 1;
    case 1:
        if (v->unlocked & (1 << v->chr)) rs_strf(s, (size_t)n, TR(STARTS_WITH), weapon_name(CHAR_TOOL[v->chr], 0));
        else if (v->coins >= char_cost(v->chr)) rs_strf(s, (size_t)n, TR(BUY_N), char_cost(v->chr));
        else rs_strf(s, (size_t)n, TR(LOCKED_N), char_cost(v->chr));
        return 1;
    case 2:
        if (v->maps_open < 2) return 0;
        snprintf(s, (size_t)n, "%s %s %s", HU_ARROW_UP, v->map ? TR(MAP_NIGHT) : TR(MAP_DAY), HU_ARROW_DOWN);
        return 1;
    case 3:
        if (!v->best_secs) return 0;
        snprintf(t, sizeof t, "%02d:%02d", v->best_secs / 60 % 100, v->best_secs % 60);
        rs_strf(s, (size_t)n, TR(BEST_RUN), t, v->best_kills);
        return 1;
    case 4:
        if (!v->can_continue) return 0;
        snprintf(t, sizeof t, "%02d:%02d", v->run_secs / 60 % 100, v->run_secs % 60);
        rs_strf(s, (size_t)n, TR(RUN_AT), t, v->run_level);
        return 1;
    case 5:
        rs_strf(u, sizeof u, TR(SHED_LINE), v->coins);
        snprintf(s, (size_t)n, "%s", u);
        return 1;
    case 6:
        if (!v->can_continue) return 0;
        snprintf(s, (size_t)n, "%s", TR(NEW_RUN));
        return 1;
    default: return 0;
    }
}

static void title_text(const view *v, int narrow, int last_row)
{
    char s[128];
    for (int k = 0; k < HOT_MAX; k++) draw_hot[k].y0 = draw_hot[k].y1 = 0;
    if (!narrow) {
        static const int8_t rows[7] = {11, 13, 15, 17, 19, 25, 23};
        hu_clear_rows(10, 10);                         /* (not row 20: the prompt's accents and CJK tops) */
        hu_clear_rows(22, 5);
        for (int k = 0; k < 7; k++) {
            if (!title_line(v, k, s, sizeof s)) continue;
            int r = rows[k];
            hu_text(hu_center(s, 0), r, s);
            if (k == 0) { hot_set(HOT_PREV, r - 1, r + 1, 0); hot_set(HOT_NEXT, r - 1, r + 1, 0); }
            if (k == 5) hot_set(HOT_SHED, r - 1, r, 0);
            if (k == 2) hot_set(HOT_MAP, r - 1, r, 0);
            if (k == 6) hot_set(HOT_NEWRUN, r - 1, r, 0);
        }
        hot_set(HOT_PROMPT, HU_ROW_PROMPT - 1, HU_ROW_PROMPT + 1, 0);
    } else {
        static const int8_t order[7] = {0, 1, 2, 4, 3, 5, 6};
        int r = 15;
        for (int i = 0; i < 7; i++) {
            int k = order[i];
            if (!title_line(v, k, s, sizeof s)) continue;
            int rows = ht_text_rows(s, 0) + (hu_tall(s) ? 1 : 0);
            if (r + rows > last_row) break;
            ht_text(r + (hu_tall(s) ? 1 : 0), s, 0);
            if (k == 0) { hot_set(HOT_PREV, r - 1, r + 1, HT_PIN_TITLE); hot_set(HOT_NEXT, r - 1, r + 1, HT_PIN_TITLE); }
            if (k == 5) hot_set(HOT_SHED, r - 1, r + rows - 1, HT_PIN_TITLE);
            if (k == 2) hot_set(HOT_MAP, r - 1, r + rows - 1, HT_PIN_TITLE);
            if (k == 6) hot_set(HOT_NEWRUN, r - 1, r + rows - 1, HT_PIN_TITLE);
            r += rows + 1;
        }
    }
    /* the arrows' halves of the gardener's line */
    draw_hot[HOT_PREV].x1 = rs_screen_width() / 2;
    draw_hot[HOT_NEXT].x0 = rs_screen_width() / 2;
}

/* the shed: the banner, the coins, the six boosts (each its name, its level in stars), what the one chosen adds and its
 * price, the keys. Every text line has the row above it free (accents, CJK) */
#define SHED_ROW(i)  (10 + 2 * (i))
#define SHED_INFO    23
#define SHED_PRICE   25
#define SHED_KEY_ROW 27
static void shed_box(int narrow, int *x, int *w)
{
    *w = narrow ? ht_ncols() : 28;
    *x = narrow ? ht_center_w(*w) : (40 - *w) / 2;
}
static void shed_panel(const view *v, int narrow)
{
    char s[64], kl[HT_MAXL][HT_LINE];
    int x, w, pin = narrow ? HT_PIN_PANEL : 0;
    shed_box(narrow, &x, &w);
    static const int names[B_COUNT] = {GS_B_MIGHT, GS_B_VIGOR, GS_B_SWIFT, GS_B_REACH, GS_B_GROWTH, GS_B_HASTE};
    static const int descs[B_COUNT] = {GS_BD_MIGHT, GS_BD_VIGOR, GS_BD_SWIFT, GS_BD_REACH, GS_BD_GROWTH, GS_BD_HASTE};
    if (narrow) ht_banner(3, TR(SHED));
    else hu_banner(3, TR(SHED));
    hu_panel(x, 6, w, 24);
    rs_strf(s, sizeof s, TR(COINS_N), v->coins);
    hu_box_text(x + (w - hu_width(s, 0)) / 2, 8, s);
    for (int i = 0; i < B_COUNT; i++) {
        hu_box_text(x + 2, SHED_ROW(i), GSTR(names[i]));
        hot_set(i, SHED_ROW(i) - 1, SHED_ROW(i), pin);
    }
    int sel = v->shed_sel, lv = v->boosts[sel];
    const char *d = GSTR(descs[sel]);
    hu_box_text(x + (w - hu_width(d, 0)) / 2, SHED_INFO, d);
    if (lv >= BOOST_MAX) snprintf(s, sizeof s, "%s", TR(MAX));
    else rs_strf(s, sizeof s, TR(COINS_N), shed_cost(sel, lv));
    hu_box_text(x + (w - hu_width(s, 0)) / 2, SHED_PRICE, s);
    const char *k = TR(SHED_KEYS);
    int nk = hu_width(k, 0) > w - 2 ? ht_wrap_bal(k, w - 2, kl, 2) : 1;        /* (too wide: two rows) */
    if (nk < 2) { snprintf(kl[0], HT_LINE, "%s", k); nk = 1; }
    for (int i = 0; i < nk; i++)
        hu_box_text(x + (w - hu_width(kl[i], 0)) / 2, SHED_KEY_ROW + i, kl[i]);
    hot_set(HOT_MAX - 1, SHED_KEY_ROW - 1, SHED_KEY_ROW + nk, pin);
}
static void shed_sprites(const view *v, int narrow)
{
    int x, w, pin = narrow ? HT_PIN_PANEL : 0;
    shed_box(narrow, &x, &w);
    int ux = hu_ui_x(), dy = pin && tall() ? ht_pin_dy(pin) : 0, b = (tick / 8) & 1, r = SHED_ROW(v->shed_sel);
    spr(SPR_CURSOR + b, x * 8 + ux - 7, r * 8 + dy, 3, 0);
    spr(SPR_CURSOR + b, (x + w) * 8 + ux - 1, r * 8 + dy, 3, RS_SPR_HFLIP);
    for (int i = 0; i < B_COUNT; i++)                   /* the levels: five at the row's end, a star each bought */
        for (int k = 0; k < BOOST_MAX; k++)
            spr(k < v->boosts[i] ? SPR_I_STAR : SPR_RINGDOT + 1, (x + w - 2 - BOOST_MAX + k) * 8 + ux, SHED_ROW(i) * 8 + dy,
                3, 0);
}

/* the results: the banner on row 4, the panel from row 8: TIME, PESTS, LEVEL, COINS, each over its value in 2x gold */
#define RES_ROW(i)  (10 + 4 * (i))
#define RES_RETRY   26
static void results_panel(const world *w, const view *v, int narrow)
{
    char s[64];
    const char *lab[4] = {TR(RES_TIME), TR(RES_PESTS), TR(RES_LEVEL), TR(RES_COINS)};
    int width = narrow ? ht_ncols() : 26, x = narrow ? ht_center_w(width) : (40 - width) / 2;
    const char *title = w->won ? TR(SAVED) : HTR(GAME_OVER);
    if (narrow) ht_banner(4, title);
    else hu_banner(4, title);
    hu_panel(x, 8, width, 20);
    for (int i = 0; i < 4; i++) {
        if (i == 0 && v->new_best) snprintf(s, sizeof s, "%s - %s", lab[i], HTR(NEW_BEST));
        if (i != 0 || !v->new_best || hu_width(s, 0) > width - 2) snprintf(s, sizeof s, "%s", lab[i]);
        hu_box_text(x + (width - hu_width(s, 0)) / 2, RES_ROW(i), s);
    }
    char vals[4][24];
    fmt_time(vals[0], 24, w->t);
    snprintf(vals[1], 24, "%d", w->kills);
    snprintf(vals[2], 24, "%d", w->level);
    snprintf(vals[3], 24, "+%d", v->earned);
    for (int i = 0; i < 4; i++) hu_big(x + (width - hu_width(vals[i], 1)) / 2, RES_ROW(i) + 1, vals[i], HU_BIG_BANNER);
}

/* ---- the frame ------------------------------------------------------------------------------------------------------------- */
void draw_frame(const world *w, const view *v)
{
    int narrow = tall() && ht_narrow();
    tick++;
    glint_t++;
    if (laid_w != rs_screen_width() || laid_h != rs_screen_height() || rs_bg_map_h(RS_BG4) != map_h())
        layers_setup();
    load_char(v->state == ST_TITLE ? v->chr : w->chr);
    load_map(v->state == ST_TITLE ? v->map : w->map);
    rs_screen_fill(1);
    rs_screen_focus_x(-1);
    int title = v->state == ST_TITLE || v->state == ST_SHED;
    int panel = v->state == ST_LEVELUP || v->state == ST_CHEST || v->state == ST_OVER || v->state == ST_SHED;
    int pin = !tall() ? HT_PIN_OFF : v->state == ST_TITLE ? HT_PIN_TITLE : panel ? HT_PIN_PANEL : HT_PIN_OFF;
    int hud = v->state == ST_PLAY || v->state == ST_LEVELUP || v->state == ST_CHEST;
    int hud_pinned = tall() && !pin && hud;
    rs_bg_pin(RS_BG1, 0, hud_pinned ? HUD_ROWS * 8 : 0, 0);
    if (pin) ht_pin(pin);
    r_pin = pin;
    r_hud = hud_pinned;

    /* the camera on the gardener (the title: a slow drift over the garden) */
    int sx = hu_shake_x(), sy = hu_shake_y();
    int cam_x, cam_y;
    if (title) {
        cam_x = (int)(v->st_t / 3) - 160;
        cam_y = -120 - (int)(v->st_t / 7);
    } else {
        cam_x = (int)(w->h.x / FX) - 160 - sx;
        cam_y = (int)(w->h.y / FX) - 120 - sy;
    }
    garden(w, cam_x, cam_y);
    if (!title) draw_swarm(w, cam_x, cam_y);
    else {
        soft_begin(cam_x, cam_y, 0, 0, 0, 0);          /* (the title: the logo's tiles share the pool: keep it empty) */
        soft_end();
    }
    rs_bg_enable(RS_BG2, !title);
    rs_brightness(15);

    /* the UI layer: redrawn when what it shows changes */
    int secs = w->t / SEC;
    int key = v->state * 7919 + narrow * 11 + v->paused * 53 + (v->state == ST_OVER && v->st_t >= RETRY_LOCK) * 3 +
              (title ? v->chr * 31 + v->map * 37 + v->coins * 41 + v->can_continue * 43 + v->unlocked * 47 +
                           v->shed_sel * 59 + v->boosts[0] * 61 + v->boosts[1] * 67 + v->boosts[2] * 71 +
                           v->boosts[3] * 73 + v->boosts[4] * 79 + v->boosts[5] * 83 : 0) +
              (hud ? secs * 101 + w->level * 103 + w->kills * 107 : 0) + (warn_t > 0) * 997 +
              (v->state == ST_LEVELUP ? w->n_offer * 211 + w->offer[0].id * 13 + w->offer[0].kind * 17 + w->level : 0) +
              (v->state == ST_CHEST ? w->n_chest * 223 : 0) + (v->first_run && w->t < 8 * SEC && hud) * 5003;
    if (key != ui_key) {
        hu_clear();
        if (title) {
            if (v->state == ST_SHED) shed_panel(v, narrow);     /* (the shed: its panel alone) */
            else if (narrow) ht_logo("PRICKLE PATCH", title_ramps, 2, HT_ROW_LOGO, 0);
            else hu_logo("PRICKLE PATCH", title_ramps, 2, 2, 0);
        } else if (v->state == ST_OVER) {
            results_panel(w, v, narrow);
        } else {
            if (!narrow || v->state == ST_PLAY) hud_text(w, narrow);   /* (upright: the panels alone) */
            if (v->state == ST_LEVELUP) choices_panel(w->offer, w->n_offer, TR(LEVEL_UP), narrow);
            if (v->state == ST_CHEST) choices_panel(w->chest, w->n_chest, TR(TREASURE), narrow);
            if (v->state == ST_PLAY && warn_t) {
                const char *s = warn_type == ET_QUEEN ? TR(WARN_QUEEN) : warn_type == ET_STAG ? TR(WARN_STAG) : TR(WARN_CROWKING);
                if (narrow) ht_big(12, s, HU_BIG_FREE);
                else hu_big(hu_center(s, 1), 12, s, HU_BIG_FREE);
            }
            if (v->state == ST_PLAY && v->first_run && w->t < 8 * SEC) {
                const char *a = v->stick || v->bot == 2 ? TR(HINT_TOUCH) : TR(HINT_PAD), *b = TR(HINT_TOOLS);
                if (narrow) { int r = ht_text(20, a, 0); ht_text(r + 2, b, 0); }
                else { hu_text(hu_center(a, 0), 22, a); hu_text(hu_center(b, 0), 24, b); }
            }
            if (v->paused) {
                const char *q = TR(PAUSE_QUIT);
                int last = 17;
                if (narrow) last = ht_text(17, q, 0);
                else hu_text(hu_center(q, 0), 17, q);
                hot_set(HOT_QUIT, 16, last + 1, 0);
            }
        }
        hu_pause(0, 13);
        ui_key = key;
    }
    if (hud && (!narrow || v->state == ST_PLAY)) {        /* the bars, every frame */
        int c0, c1;
        hud_cols(narrow, &c0, &c1);
        hud_bar(0, c0, c1, w->xp, w->xp_need, 0);
        if (w->boss >= 0 && w->boss < w->n_en) {
            int bw = (c1 - c0) * 2 / 3;
            hud_bar(4, (c0 + c1 - bw) / 2, (c0 + c1 + bw) / 2, w->en[w->boss].hp, w->boss_hpmax, 1);
        } else {
            for (int c = c0; c < c1; c++) rs_bg_put(RS_BG1, c & (rs_bg_map_w(RS_BG1) - 1), 4, RS_MAP(0, PAL_UI, 1, 0, 0));
        }
    }
    int slide = v->state == ST_OVER ? hu_slide_in(v->st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, -hu_ui_x() & (rs_bg_map_w(RS_BG1) * 8 - 1), -slide & 255);
    if (narrow) ht_pause(v->paused, 13);
    else hu_pause(v->paused, 13);

    /* sprites, front to back */
    rs_oam_clear();
    budget_reset();
    if (title) {
        int pdy = pin ? ht_pin_dy(HT_PIN_TITLE) : 0;
        if (v->state == ST_SHED) {
            shed_sprites(v, narrow);
        } else if (narrow) {
            ht_title tc = {v->can_continue ? TR(VERB_CONTINUE) : TR(VERB), 1, 15, 0, NULL};
            ht_title_out to;
            ht_title_draw(&tc, v->st_t, 0, &to);
            title_text(v, narrow, (to.gap_y1 - pdy) / 8 - 1);
            ht_title_sprites(&tc, &to, v->st_t, NULL, NULL);
            hot_set(HOT_PROMPT, (to.glyph_y - pdy) / 8, (to.glyph_y - pdy) / 8 + 2, HT_PIN_TITLE);
            budget_kit(0, 0, 16, 16);
            int bob = hu_bob(v->st_t, 64, 2);
            draw_hero(w, 0, 0, ht_mid_x() - 12 + hu_ui_x(), pdy - 12 + bob);
        } else {
            hu_title_draw(v->st_t, 0);
            title_text(v, narrow, 29);
            hu_title_sprites(v->st_t, NULL, NULL);
            budget_kit(0, 0, 16, 16);
            int bob = hu_bob(v->st_t, 64, 2);
            draw_hero(w, 0, 0, 160 - 12 + hu_ui_x(), 76 + bob);
        }
        if (!(v->unlocked & (1 << v->chr)))
            spr(SPR_I_LOCK, (narrow ? ht_mid_x() : 160) + hu_ui_x() + 8, (narrow ? pdy - 12 : 76) + 2, 3, 0);
        return;
    }
    if (v->state == ST_OVER) {
        if (v->st_t >= RETRY_LOCK) {
            if (narrow) ht_retry_line_at(RES_RETRY, v->st_t, RETRY_LOCK, HTR(BACK_TITLE));
            else hu_retry_line_at(RES_RETRY, v->st_t, RETRY_LOCK, HTR(BACK_TITLE));
        }
        draw_hero(w, cam_x, cam_y, 0, 0);
        draw_items(w, cam_x, cam_y);
        return;
    }
    /* the HUD's sprites */
    int c0, c1, hdy = hud_pinned ? rs_screen_top() : 0;
    hud_cols(narrow, &c0, &c1);
    if (!narrow || v->state == ST_PLAY) {
        char s[16];
        snprintf(s, sizeof s, "%d", w->kills);
        int lr = narrow ? 3 : 1;
        spr(SPR_I_PEST, (c1 - 2 - hu_width(s, 0)) * 8 + hu_ui_x() - 2, lr * 8 + hdy, 3, 0);
    }
    if (v->state == ST_LEVELUP || v->state == ST_CHEST) {
        const choice *c = v->state == ST_LEVELUP ? w->offer : w->chest;
        int n = v->state == ST_LEVELUP ? w->n_offer : w->n_chest;
        choices_sprites(c, n, v->sel, narrow, pin, v->state == ST_LEVELUP);
        for (int i = 0; i < n; i++) hot_set(i, ch_row(narrow, i) - 2, ch_row(narrow, i) + 2, pin);
        if (v->state == ST_CHEST) hot_set(HOT_MAX - 1, 0, 29, pin);
    }
    if (v->stick) {                                     /* the floating touch stick */
        int ox = rs_screen_origin_x(), oy = rs_screen_origin_y();
        spr(SPR_STICK + 1, v->stick_x - ox - 16, v->stick_y - oy - 16, 3, 0);
        spr(SPR_STICK, v->stick_x0 - ox - 16, v->stick_y0 - oy - 16, 3, 0);
    }
    draw_pops(cam_x, cam_y, 0);
    draw_hero(w, cam_x, cam_y, 0, 0);
    draw_hpbar(w, cam_x, cam_y);
    if (lvl_t)                                          /* the level-up's flash round the gardener */
        for (int k = 0; k < 4; k++) {
            int a = (24 - lvl_t) * 2 + 8;
            int dx = (k & 1 ? 1 : -1) * a, dy = (k & 2 ? 1 : -1) * a / 2;
            spr(SPR_GLOW + (lvl_t < 12), (int)(w->h.x / FX) - cam_x - 8 + dx, (int)(w->h.y / FX) - cam_y - 10 + dy, 2, 0);
        }
    draw_tools(w, cam_x, cam_y);
    draw_items(w, cam_x, cam_y);
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(laid_w); S(laid_h); S(laid_title); S(ui_key); S(gx0); S(gy0); S(gx1); S(gy1); S(gem_cell); S(n_gem_cell); S(cur_chr); S(cur_map);
    S(pops); S(pop_next); S(poofs); S(poof_next); S(tick); S(warn_t); S(warn_type); S(lvl_t); S(hurt_flash); S(glint_t);
    S(draw_hot);
    soft_state();
    RS_STATE_RASTER(raster);
}
#undef S
