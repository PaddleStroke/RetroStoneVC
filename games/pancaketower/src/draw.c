/*
 * Pancake Tower: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1), BG2 the tower (rows drawn at run time by render.c; the wobble is a
 *   per-line scroll), BG3 the near scenery (the tower's world, streamed by rows), BG4 the far layer (1/4 of the
 *   camera), the backdrop a raster sky gradient that follows the altitude.
 *   Sprites: the slider and the top pancake (render.c), the falling pieces, toppings, syrup, butter, the effects,
 *   the sky's critters, the chef, the kit's digits.
 * Two players: two viewports (each its own camera, scenery rows, wobble table and sky, switched by the raster
 * callback); the game-over panel is drawn in two more viewports over them.
 * Nothing here changes the game (tower.c): the cameras, wobble and effects are visual and deterministic.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
 */
#include "pt.h"
#include "assets.h"
#include "render.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>

#define CAM0        (-(240 - BASE_SCREEN_Y))       /* the camera at the start: the plate's top on line 192 */
#define NO_ROW      (-100000)
#define FX_MAX      64
#define PIECES      2
#define DYN_PER_PLAYER 96                          /* OBJ tiles: slider 16, top 16, 2 pieces x 32 */
#define CHEF_X      (-132)                         /* the chef's left, world x (1 player, in the kitchen) */
#define PANO_CX     160                            /* the plate's centre in the near panorama (px) */
#define TOP_MARGIN  40                             /* the camera keeps this much of a ceiling above the tower */

/* the holes the tower breaks: the kitchen ceiling and the roof. Their rows are carved at run time into copies of the
 * scenery's tiles (BG3), pixel-exact around the layer that broke through, with a broken rim; the tower is held by
 * them (no sway below a hole's top), so the hole and the tower always line up. */
enum { H_CEIL, H_ROOF, HOLES };
static const int HOLE_Y0[HOLES] = {CEILING_Y, ROOF_Y}, HOLE_Y1[HOLES] = {CEILING_TOP, ROOF_TOP};
#define CARVE_ROWS  ((CEILING_TOP - CEILING_Y + ROOF_TOP - ROOF_Y) / 8)
#define CARVE_SLOTS 4                              /* run-time tiles per carved row: 2 at each edge */
#define JAG_MAX     3                              /* the broken rim reaches this far past the layer */
#define SHAKE_MAX   3                              /* px: the largest screen shake (hu_shake) */
static int floor8(int y) { return y >= 0 ? y / 8 : -((-y + 7) / 8); }

/* the title logo: "PANCAKE" golden, "TOWER" syrup (house_style ACCENTS pancake, syrup) */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 226, 160), RS_RGB8(240, 184, 96), RS_RGB8(206, 136, 56)},
    {RS_RGB8(236, 150, 84), RS_RGB8(196, 104, 44), RS_RGB8(140, 62, 26)}};

/* ---- the views (one per player) --------------------------------------------------------------------------- */
typedef struct hole { int on, x0, x1, seed; } hole;   /* x: the layer's extent that broke through (world x) */

typedef struct view {
    int cam;                        /* world y of the view's bottom line (line h - 1 shows the strip cam) */
    int sx0;                        /* screen x of the plate's centre (viewport coordinates) */
    int w, h, y0;                   /* the view's size, its top line on the screen */
    int top_line;                   /* the camera keeps the tower's top on this line once it is that high */
    hole hole[HOLES];
    int bg2_cx, bg2_col0, bg2_cols; /* BG2: the plate's centre (map px), the player's columns */
    int near_col0, near_pcol0, near_cols;   /* BG3: map columns <- panorama columns */
    int far_col0;
    int near_row[32], far_row[32];  /* the world / far tile row shown in each ring row */
    int tower_row[32];              /* the layer drawn in each ring row + 1 (0 = empty) */
    int16_t wob_dx[RS_SCREEN_H];
    rs_color line_col[RS_SCREEN_H];
    int col_cam;                    /* the camera the sky was computed for */
    int jiggle;                     /* frames since the last landing */
    int top_key, slider_key;        /* what the dynamic sprites were drawn for */
    int top_squash;
    int chef_pose, chef_t, portrait_t;
    int bottle_t;
    int drip_t;
} view;

typedef struct piece {
    int on, player;
    int32_t x, y, vx, vy;           /* Q8: the centre, world coordinates */
    int a, va, t;                   /* rotation (64 steps per turn, Q4), speed */
    int shown_a, halves;
    cut_piece p;
} piece;

typedef struct fx_obj { int kind, player, t, life, frame; int32_t x, y, vx, vy; } fx_obj;
enum { FX_NONE, FX_PLASTER, FX_DUST, FX_ROOFTILE, FX_BURST, FX_SPARK, FX_DRIP, FX_CRUMBS, FX_PLUS, FX_SPLASH_IN,
       FX_SWEAT, FX_TOPPING, FX_SPLINTER };
enum { CH_IDLE, CH_CHEER, CH_PANIC, CH_DESPAIR };

static view V[MAX_PLAYERS];
static piece PC[MAX_PLAYERS][PIECES];
static fx_obj FXS[FX_MAX];
static int nviews, shown_state = -1, shown_best = -1, shown_players = -1, title_logo_on;
static int near_pal_last;
static int medals_loaded;                          /* the fork medals' tiles (VRAM: only when one is shown) */                          /* the last near palette is in BG palette 5 (not the logo's) */
static rs_viewport vps[4];
static int nvps;
int opt_nodraw_bg;

/* ---- small helpers ---------------------------------------------------------------------------------------- */
static const int16_t SIN64[64] = {
    0, 402, 799, 1189, 1567, 1931, 2276, 2598, 2896, 3166, 3406, 3612, 3784, 3920, 4017, 4076,
    4096, 4076, 4017, 3920, 3784, 3612, 3406, 3166, 2896, 2598, 2276, 1931, 1567, 1189, 799, 402,
    0, -402, -799, -1189, -1567, -1931, -2276, -2598, -2896, -3166, -3406, -3612, -3784, -3920, -4017, -4076,
    -4096, -4076, -4017, -3920, -3784, -3612, -3406, -3166, -2896, -2598, -2276, -1931, -1567, -1189, -799, -402};
static int isin(int a) { return SIN64[a & 63]; }  /* Q12 */

static int line_of(const view *v, int wy) { return v->h - 1 + v->cam - wy; }   /* the view line showing strip wy */

static void spr(int id, int x, int y, int prio, int pal, int flags, int vw)
{
    const pt_sprite_def *d = &pt_spr[id];
    if (x <= -(int)d->w || x >= vw || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

static int dyn_tile(int p, int k) { return OBJ_DYN_TILE + p * DYN_PER_PLAYER + k; }

/* the highest layer held still: the plate's, or the top layer inside a hole the tower broke (it holds the tower) */
static int anchor_layer(const view *v)
{
    if (v->hole[H_ROOF].on) return ROOF_TOP / ROW_H - 1;
    if (v->hole[H_CEIL].on) return CEILING_TOP / ROW_H - 1;
    return 0;
}

/* the wobble of layer i of a tower of n layers (px, visual only): it sways above its anchor */
static int wobble(const view *v, int i, int n, int t)
{
    int a = anchor_layer(v), free = n - 1 - a;
    if (i <= a || free <= 0) return 0;
    int amp = WOBBLE_MAX * 256 * (free < WOBBLE_FULL_H ? free : WOBBLE_FULL_H) / WOBBLE_FULL_H;   /* Q8 */
    int h = (i - a) * 256 / free;                                         /* Q8: 0 at the anchor, 1 at the top */
    int w = amp * h / 256 * h / 256 * isin(t * 64 / WOBBLE_PERIOD) / 4096;
    if (v->jiggle < JIGGLE_FRAMES) {
        int j = JIGGLE_PX * 256 * (JIGGLE_FRAMES - v->jiggle) / JIGGLE_FRAMES;
        w += j * h / 256 * isin(v->jiggle * 6) / 4096;
    }
    return (w + (w >= 0 ? 128 : -128)) / 256;
}

/* ---- the sky: a colour per world height ---------------------------------------------------------------------- */
/* a morning sky through the kitchen window, blue above the roof, a paler band in the clouds, then night */
static const struct { int wy; uint32_t c; } SKY[] = {
    {40, 0xe4f4fa}, {RIDGE_Y, 0x9ed2f4}, {520, 0x62aaec}, {640, 0x82bef4}, {760, 0x4664be}, {900, 0x242c78},
    {1000, 0x121034}, {1100, 0x080818}};

static void sky_colour(int wy, int rgb[3])
{
    int n = (int)(sizeof SKY / sizeof SKY[0]);
    if (wy <= SKY[0].wy) wy = SKY[0].wy;
    int i = 0;
    while (i < n - 1 && wy > SKY[i + 1].wy) i++;
    uint32_t a = SKY[i].c, b = SKY[i < n - 1 ? i + 1 : i].c;
    int span = i < n - 1 ? SKY[i + 1].wy - SKY[i].wy : 1, k = i < n - 1 ? (wy - SKY[i].wy) * 256 / span : 0;
    if (k > 256) k = 256;
    for (int c = 0; c < 3; c++) {
        int sa = (int)(a >> (16 - 8 * c)) & 255, sb = (int)(b >> (16 - 8 * c)) & 255;
        rgb[c] = sa + (sb - sa) * k / 256;
    }
}

static void sky_update(view *v)
{
    if (v->col_cam == v->cam) return;
    v->col_cam = v->cam;
    for (int l = 0; l < v->h; l++) {
        int rgb[3], y = v->y0 + l;
        sky_colour(v->cam + v->h - 1 - l, rgb);
        /* house rule: a 2-line dither step every 8 lines keeps the RGB555 bands soft */
        int d = (y & 4) != 0;
        v->line_col[y] = RS_RGB8(clampi(rgb[0] + (d ? 4 : 0), 0, 255), clampi(rgb[1] + (d ? 2 : 0), 0, 255),
                                 clampi(rgb[2] + (d ? 4 : 0), 0, 255));
    }
}

/* the raster callback: the sky of the view being drawn, its wobble table (both by screen line) */
static void raster(int line, void *user)
{
    (void)user;
    int vi = rs_viewport_current();
    if (vi < 0 || vi >= nviews) vi = 0;          /* 1 player, or the panel viewports */
    rs_pal_set(0, V[vi].line_col[line]);
    if (nviews > 1) rs_bg_line_scroll(RS_BG2, V[vi].wob_dx, NULL);
}

/* ---- set-up ------------------------------------------------------------------------------------------------ */
static int void_tile0(void) { return pt_near_tile_count; }                      /* BG3, relative: a solid hole */
static int carve_tile0(void) { return pt_near_tile_count + NEAR_PALS; }         /* BG3, relative: carved rows */

void draw_init(void)
{
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0 on palette 0, the logo at 320 */
    hc.logo_pal = PAL_LOGO;
    hc.box_glyphs = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!:-.";   /* the panel font: capitals, digits (VRAM) */
    hc.obj_tile = KIT_OBJ_TILE;
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = OBJ_KIT;
    hu_init(&hc);
    rs_bg_setup(RS_BG1, 64, 32, VR_BG1);
    rs_bg_setup(RS_BG2, 32, 32, VR_BG2);
    rs_bg_setup(RS_BG3, 64, 32, VR_BG3);
    rs_bg_setup(RS_BG4, 64, 32, VR_BG4);
    render_init();
    rs_tiles_load(VR_BG3, pt_near_tiles, pt_near_tile_count);
    static const int void_idx[NEAR_PALS] = NEAR_VOID;
    for (int k = 0; k < NEAR_PALS; k++) {        /* the inside of a hole, one per palette (the "dark" colour) */
        uint8_t t[64];
        memset(t, void_idx[k], sizeof t);
        rs_tiles_load8(VR_BG3 + void_tile0() + k, t, 1);
    }
    if (carve_tile0() + MAX_PLAYERS * CARVE_ROWS * CARVE_SLOTS > BG3_TILES)
        rs_log("draw: the carved holes overflow BG3's tiles (%d)", carve_tile0() + MAX_PLAYERS * CARVE_ROWS * CARVE_SLOTS);
    rs_tiles_load(VR_BG4, pt_far_tiles, pt_far_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_TOWER), pt_tower_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_TOPPING), pt_topping_pal, 16);
    static const int near_slots[NEAR_PALS] = NEAR_PAL_SLOTS;
    for (int i = 0; i < NEAR_PALS; i++) rs_pal_load(RS_PAL_BG(near_slots[i]), pt_near_pals + i * 16, 16);
    rs_pal_load(RS_PAL_BG(PAL_FAR), pt_far_pal, 16);
    for (int p = 0; p < 8; p++)
        if (p != OBJ_KIT) rs_pal_load(RS_PAL_OBJ(p), pt_obj_pals + p * 16, 16);
    rs_obj_base(VR_OBJ);
    /* the sprites, but the fork medals (the last group): they are loaded when a 1-player game-over panel shows one */
    rs_tiles_load(VR_OBJ, pt_obj_tiles, pt_spr[SPR_MEDAL].tile);
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_raster(raster, NULL);
}

static void reset_view(view *v, int p, int players)
{
    memset(v, 0, sizeof *v);
    v->cam = CAM0;
    v->col_cam = NO_ROW;
    v->top_key = v->slider_key = -1;
    v->h = RS_SCREEN_H;
    v->top_line = TOP_SCREEN_Y;
    for (int r = 0; r < 32; r++) v->near_row[r] = v->far_row[r] = NO_ROW;
    if (players == 1) {
        v->w = RS_SCREEN_W;
        v->sx0 = RS_SCREEN_W / 2;
        v->bg2_cx = 128, v->bg2_col0 = 0, v->bg2_cols = 32;
        v->near_col0 = 0, v->near_pcol0 = 0, v->near_cols = 40;
        v->far_col0 = 0;
    } else {
        v->w = RS_SCREEN_W / 2 - 1;
        v->sx0 = 80;
        v->bg2_cx = 64 + 128 * p, v->bg2_col0 = 16 * p, v->bg2_cols = 16;
        v->near_col0 = 32 * p, v->near_pcol0 = 10, v->near_cols = 20;
        v->far_col0 = 32 * p;
    }
}

/* the highest the camera may go: the view's top line stays under a ceiling the tower has not broken yet (with
 * TOP_MARGIN of it in view above the tower), so what lies above a ceiling is first seen once the tower is through */
static int cam_limit(const view *v)
{
    if (!v->hole[H_CEIL].on) return CEILING_Y + TOP_MARGIN - v->h;
    if (!v->hole[H_ROOF].on) return ROOF_Y + TOP_MARGIN - v->h;
    return 1 << 30;
}

static int cam_target(const view *v, const tower *tw)
{
    int target = tower_top_y(tw) - (v->h - v->top_line);
    int lim = cam_limit(v);
    if (target > lim) target = lim;
    return target < CAM0 ? CAM0 : target;
}

void draw_reset(const match *m)
{
    nviews = m->players;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        view *v = &V[p];
        reset_view(v, p, m->players);
        if (p >= m->players) continue;
        const tower *tw = &m->tw[p];
        /* a pre-stacked tower (start=N): the holes it made (its layers there, or the first width if they are gone
         * from the ring), then the camera */
        for (int h = 0; h < HOLES; h++) {
            int i = HOLE_Y0[h] / ROW_H;
            if (tw->nlayers <= i) continue;
            hole *ho = &v->hole[h];
            ho->on = 1;
            ho->seed = (int)((uint32_t)(i * 2654435761u + (uint32_t)p * 40503u) >> 20);
            if (i >= tw->nlayers - RING) ho->x0 = tower_layer(tw, i)->x, ho->x1 = ho->x0 + tower_layer(tw, i)->w;
            else ho->x0 = -tw->g.w0 / 2, ho->x1 = tw->g.w0 / 2;
            for (int j = i + 1; j < HOLE_Y1[h] / ROW_H && j < tw->nlayers; j++)
                if (j >= tw->nlayers - RING) {
                    const layer *l = tower_layer(tw, j);
                    if (l->x < ho->x0) ho->x0 = l->x;
                    if (l->x + l->w > ho->x1) ho->x1 = l->x + l->w;
                }
        }
        int target = cam_target(v, tw);
        if (target > v->cam) v->cam = target;
    }
    memset(PC, 0, sizeof PC);
    memset(FXS, 0, sizeof FXS);
    rs_bg_fill(RS_BG2, 0);
    rs_bg_fill(RS_BG3, 0);
    rs_bg_setup(RS_BG4, m->players == 1 ? 32 : 64, 32, VR_BG4);    /* 1 player: 256 px that repeat (VRAM) */
    rs_bg_line_scroll(RS_BG2, V[0].wob_dx, NULL);
    shown_state = -1;
}

int draw_camera(int player) { return V[player].cam; }

/* ---- the holes: carved into BG3's rows ---------------------------------------------------------------------------- */
/* how far the broken rim reaches past the layer, by pixel row (0..JAG_MAX): ragged, the same for a given hole */
static int jag(const hole *ho, int wy, int side)
{
    static const uint8_t J[16] = {1, 2, 3, 2, 1, 0, 1, 2, 2, 3, 2, 1, 1, 0, 1, 2};
    return J[(wy / 2 + side * 5 + ho->seed) & 15];
}

/* the run-time tile of a carved row: its world row k, the slot j of 4 */
static int carve_slot(int p, int k, int j)
{
    int ci = k * ROW_H < ROOF_Y ? (k * ROW_H - CEILING_Y) / ROW_H : (CEILING_TOP - CEILING_Y + k * ROW_H - ROOF_Y) / ROW_H;
    return carve_tile0() + (p * CARVE_ROWS + ci) * CARVE_SLOTS + j;
}

/* one 8x8 tile of the near panorama (world row k, panorama column pc), decoded: palette index per pixel */
static uint16_t near_tile(int k, int pc, uint8_t pix[64])
{
    int seg = (k * 8 - SEG_BASE_Y) >> 8, row = 31 - ((k + 8) & 31);
    if (seg > NEAR_SEGS - 1) seg = NEAR_SEGS - 1;
    uint16_t e = seg >= 0 ? pt_near_map[(seg * NEAR_H + row) * NEAR_W + pc] : 0;
    const uint8_t *t = pt_near_tiles + RS_MAP_TILE(e) * RS_TILE_BYTES;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int sx = (e & RS_MAP_HFLIP) ? 7 - x : x, sy = (e & RS_MAP_VFLIP) ? 7 - y : y;
            uint8_t b = t[sy * 4 + sx / 2];
            pix[y * 8 + x] = (uint8_t)((sx & 1) ? b & 15 : b >> 4);
        }
    return e;
}

/* the hole's pixels in world row k (ring row r) of view v: inside the hole the "dark" colour, the broken rim around */
static void carve_row(view *v, int p, int k, int r)
{
    static const int void_idx[NEAR_PALS] = NEAR_VOID, rim_idx[NEAR_PALS] = NEAR_RIM, slots[NEAR_PALS] = NEAR_PAL_SLOTS;
    for (int h = 0; h < HOLES; h++) {
        const hole *ho = &v->hole[h];
        if (!ho->on || k * ROW_H < HOLE_Y0[h] || k * ROW_H >= HOLE_Y1[h]) continue;
        int ax0 = PANO_CX + ho->x0, ax1 = PANO_CX + ho->x1, used = 0;
        for (int c = 0; c < v->near_cols; c++) {
            int pc = v->near_pcol0 + c, px0 = pc * 8;
            if (px0 + 8 < ax0 - JAG_MAX - 1 || px0 > ax1 + JAG_MAX) continue;
            uint8_t pix[64];
            uint16_t e = near_tile(k, pc, pix);
            int pk = 0;
            while (pk < NEAR_PALS - 1 && slots[pk] != RS_MAP_PAL(e)) pk++;
            int any = 0, all = 1;
            for (int y = 0; y < 8; y++) {
                int wy = k * ROW_H + 7 - y, lo = ax0 - jag(ho, wy, 0), hi = ax1 + jag(ho, wy, 1);
                for (int x = 0; x < 8; x++) {
                    int X = px0 + x, in = X >= lo && X < hi, rim = X == lo - 1 || X == hi;
                    if (in) pix[y * 8 + x] = (uint8_t)void_idx[pk];
                    else if (rim && pix[y * 8 + x]) pix[y * 8 + x] = (uint8_t)rim_idx[pk];
                    any |= in || rim;
                    all &= in;
                }
            }
            if (!any) continue;
            uint16_t ne;
            if (all) ne = RS_MAP(void_tile0() + pk, RS_MAP_PAL(e), 0, 0, 0);
            else {
                if (used >= CARVE_SLOTS) continue;     /* cannot happen: at most 2 tiles at each edge */
                int slot = carve_slot(p, k, used++);
                rs_tiles_load8(VR_BG3 + slot, pix, 1);
                ne = RS_MAP(slot, RS_MAP_PAL(e), 0, 0, 0);
            }
            rs_bg_put(RS_BG3, v->near_col0 + c, r, opt_nodraw_bg ? 0 : ne);
        }
    }
}

/* (re)carve the rows of hole h that are in the ring now */
static void carve_hole(view *v, int p, int h)
{
    for (int k = HOLE_Y0[h] / ROW_H; k < HOLE_Y1[h] / ROW_H; k++) {
        int r = (-(k + 1)) & 31;
        if (v->near_row[r] != k) continue;
        /* back to the scenery's tiles, then the hole (a hole that widens) */
        int seg = (k * 8 - SEG_BASE_Y) >> 8, row = 31 - ((k + 8) & 31);
        const uint16_t *src = pt_near_map + (seg * NEAR_H + row) * NEAR_W;
        for (int c = 0; c < v->near_cols; c++)
            rs_bg_put(RS_BG3, v->near_col0 + c, r, opt_nodraw_bg ? 0 : src[v->near_pcol0 + c]);
        carve_row(v, p, k, r);
    }
}

/* ---- streaming the rows ---------------------------------------------------------------------------------------- */
static void stream_near(view *v, int p)
{
    int k0 = floor8(v->cam - SHAKE_MAX), k1 = floor8(v->cam + v->h - 1 + SHAKE_MAX);   /* the rows a shake may show */
    for (int k = k0; k <= k1; k++) {
        int r = (-(k + 1)) & 31;
        if (v->near_row[r] == k) continue;
        v->near_row[r] = k;
        int seg = (k * 8 - SEG_BASE_Y) >> 8, row = 31 - ((k + 8) & 31);
        if (seg > NEAR_SEGS - 1) seg = NEAR_SEGS - 1;
        const uint16_t *src = seg >= 0 ? pt_near_map + (seg * NEAR_H + row) * NEAR_W : NULL;
        for (int c = 0; c < v->near_cols; c++)
            rs_bg_put(RS_BG3, v->near_col0 + c, r, src && !opt_nodraw_bg ? src[v->near_pcol0 + c] : 0);
        carve_row(v, p, k, r);
    }
}

static int far_base(const view *v) { return (v->cam - CAM0) / 4; }

static void stream_far(view *v)
{
    int fb = far_base(v), k0 = fb / 8, k1 = (fb + v->h - 1) / 8 + 1;
    for (int k = k0; k <= k1; k++) {
        int r = (-(k + 1)) & 31;
        if (v->far_row[r] == k) continue;
        v->far_row[r] = k;
        int kk = k;
        if (kk >= FAR_H) kk = FAR_H - FAR_REPEAT + (kk - FAR_H) % FAR_REPEAT;   /* the stars repeat */
        const uint16_t *src = pt_far_map + (FAR_H - 1 - kk) * FAR_W;
        for (int c = 0; c < 32; c++) rs_bg_put(RS_BG4, v->far_col0 + c, r, opt_nodraw_bg ? 0 : src[c & 31]);
    }
}

static int layer_key(const layer *l, int i) { return (i * 131 + l->x * 7 + l->w * 3 + l->flags * 17 + l->kind) & 0x7fffffff; }

static void stream_tower(view *v, int p, const tower *tw)
{
    int k0 = floor8(v->cam - SHAKE_MAX), k1 = floor8(v->cam + v->h - 1 + SHAKE_MAX);
    for (int k = k0; k <= k1; k++) {
        int r = (-(k + 1)) & 31;
        int baked = k >= 0 && k < tw->nlayers - 1 && k >= tw->nlayers - RING;
        int want = baked ? layer_key(tower_layer(tw, k), k) + 1 : 0;
        if (v->tower_row[r] == want) continue;
        v->tower_row[r] = want;
        if (want) render_row(p, r, tower_layer(tw, k), v->bg2_cx, v->bg2_col0, v->bg2_cols);
        else render_clear_row(r, v->bg2_col0, v->bg2_cols);
    }
}

/* ---- effects ----------------------------------------------------------------------------------------------------- */
static fx_obj *fx_new(int kind, int player, int x, int y)
{
    for (int i = 0; i < FX_MAX; i++)
        if (FXS[i].kind == FX_NONE) {
            fx_obj *f = &FXS[i];
            memset(f, 0, sizeof *f);
            f->kind = kind;
            f->player = player;
            f->x = x * 256;
            f->y = y * 256;
            f->life = 60;
            return f;
        }
    return NULL;
}

static uint32_t fx_seed = 0x1234567u;
static int fx_rand(int n)
{
    fx_seed ^= fx_seed << 13;
    fx_seed ^= fx_seed >> 17;
    fx_seed ^= fx_seed << 5;
    return (int)(fx_seed % (uint32_t)n);
}

/* the tower breaks through hole h ([x0, x1): the layer's extent): chunks of what the hole was made in burst out of its
 * two edges (the ceiling's plaster and joists fall into the kitchen; the roof's tiles and splinters are thrown up),
 * dust puffs at its mouth; big = the first break (a wider layer that widens it makes a smaller one) */
static void break_through(int p, int h, int x0, int x1, int big)
{
    int n = big ? (nviews > 1 ? 6 : 10) : 2;
    for (int i = 0; i < n; i++) {
        int left = i & 1, wy = HOLE_Y0[h] + fx_rand(HOLE_Y1[h] - HOLE_Y0[h]);
        int kind = FX_PLASTER, frame = fx_rand(2);
        if (h == H_CEIL && wy >= CEILING_Y + 20) kind = i % 3 ? FX_PLASTER : FX_SPLINTER, frame = kind == FX_PLASTER ? 2 : fx_rand(2);
        if (h == H_ROOF) kind = wy >= ROOF_Y + 28 ? FX_ROOFTILE : FX_SPLINTER, frame = fx_rand(2);
        fx_obj *f = fx_new(kind, p, left ? x0 - 7 - fx_rand(3) : x1 - 1 + fx_rand(3), wy + 4);
        if (!f) break;
        f->vx = (left ? -1 : 1) * (70 + fx_rand(200));
        f->vy = h == H_CEIL ? 40 + fx_rand(180) : 280 + fx_rand(240);
        f->frame = frame;
        f->life = 110;
    }
    int y = h == H_CEIL ? HOLE_Y0[h] + 2 : HOLE_Y1[h] + 10;   /* the dust: under the ceiling, over the roof */
    for (int i = 0; i < (big ? (nviews > 1 ? 2 : 4) : 2); i++) {
        fx_obj *f = fx_new(FX_DUST, p, (i & 1) ? x0 - 10 : x1 - 6, y + (i / 2) * 6);
        if (f) { f->life = 34; f->vx = (i & 1) ? -40 : 40; f->vy = h == H_CEIL ? -30 : 40; }
    }
}

/* the tower's top layer opens hole h, or widens it */
static void open_hole(view *v, int p, int h, const layer *l, int index)
{
    hole *ho = &v->hole[h];
    if (!ho->on) {
        ho->on = 1;
        ho->x0 = l->x, ho->x1 = l->x + l->w;
        ho->seed = (int)((uint32_t)(index * 2654435761u + (uint32_t)p * 40503u) >> 20);
        break_through(p, h, ho->x0, ho->x1, 1);
    } else if (l->x < ho->x0 || l->x + l->w > ho->x1) {
        if (l->x < ho->x0) ho->x0 = l->x;
        if (l->x + l->w > ho->x1) ho->x1 = l->x + l->w;
        break_through(p, h, ho->x0, ho->x1, 0);
    } else {
        return;
    }
    carve_hole(v, p, h);
}

static void piece_launch(int p, const tower *tw, int whole)
{
    piece *pc = &PC[p][0];
    for (int i = 0; i < PIECES; i++)
        if (!PC[p][i].on) { pc = &PC[p][i]; break; }
    memset(pc, 0, sizeof *pc);
    pc->on = 1;
    pc->player = p;
    pc->p = tw->cut;
    pc->x = (tw->cut.x * 2 + tw->cut.w) * 128;
    pc->y = (tower_top_y(tw) + (whole ? 4 : -4)) * 256;
    pc->vx = tw->cut.side * (whole ? 70 : 90 + 800 / (tw->cut.w + 4));
    pc->vy = whole ? 60 : 0;
    pc->va = tw->cut.side * (tw->cut.w < 24 ? 12 : tw->cut.w < 48 ? 6 : 3);
    pc->shown_a = -1;
}

static void chef_react(view *v, int pose, int frames)
{
    if (v->chef_pose == CH_DESPAIR) return;
    v->chef_pose = pose;
    v->chef_t = frames;
}

void draw_events(const match *m)
{
    for (int p = 0; p < m->players; p++) {
        const tower *tw = &m->tw[p];
        view *v = &V[p];
        int ev = tw->events;
        const layer *top = tower_top(tw);
        int ty = tower_top_y(tw);
        if (ev & EV_LAND) {
            v->jiggle = 0;
            v->top_squash = 5;
            fx_obj *f = fx_new(FX_CRUMBS, p, top->x + top->w / 2 - 4, ty);
            if (f) { f->life = 16; f->vy = 60; }
        }
        if (ev & EV_CUT) {
            piece_launch(p, tw, 0);
            fx_obj *d = fx_new(FX_DRIP, p, tw->cut.side > 0 ? top->x + top->w - 4 : top->x - 4, ty - 2);
            if (d) d->life = 70;
            if (tw->cut.w * 3 >= top->w + tw->cut.w || top->w < NARROW_PX) chef_react(v, CH_PANIC, 50);
        }
        if (ev & EV_MISS) {
            piece_launch(p, tw, 1);
            v->chef_pose = CH_DESPAIR;
            v->chef_t = 100000;
        }
        if (ev & EV_PERFECT) {
            fx_obj *f = fx_new(FX_PLUS, p, top->x + top->w + 4, ty + 4);
            if (f) { f->frame = tw->bonus >= 2; f->life = 40; f->vy = 90; }
            for (int i = 0; i < ((ev & EV_REGROW) ? 4 : 2); i++) {
                fx_obj *s = fx_new(i < 2 ? FX_SPARK : FX_BURST, p, top->x + (i & 1 ? top->w + 2 : -8), ty + 2 + i * 3);
                if (s) s->life = 24 + i * 4;
            }
            chef_react(v, CH_CHEER, 45);
        }
        if (ev & EV_TOPPING) {
            fx_obj *f = fx_new(FX_TOPPING, p, top->x + top->w / 2 - 16, ty + 150);
            if (f) { f->frame = tower_topping_kind(tw->pancakes) - LK_STRAWBERRY; f->life = TOPPING_FRAMES; }
        }
        if (ev & EV_TOPPING_LAND) {
            v->jiggle = 0;
            v->top_squash = 4;
            chef_react(v, CH_CHEER, 40);
            for (int i = 0; i < 3; i++) {
                fx_obj *c = fx_new(FX_CRUMBS, p, top->x + i * top->w / 3, ty);
                if (c) { c->life = 14; c->vy = 80; c->vx = (i - 1) * 60; }
            }
        }
        if (ev & EV_POUR) v->bottle_t = 1;
        if (ev & EV_SPLASHED) {
            int from = p == 0 ? 1 : -1;               /* the rival is on the other side */
            fx_obj *f = fx_new(FX_SPLASH_IN, p, top->x + top->w / 2 + from * 110, ty + 60);
            if (f) { f->life = 24; f->vx = -from * 110 * 256 / 24; f->vy = -60 * 256 / 24; }
        }
        if (ev & EV_CEILING) {
            hu_shake(3, 12);
            chef_react(v, CH_PANIC, 60);
        }
        if (ev & EV_ROOF) hu_shake(2, 10);
        /* the top layer in a ceiling or the roof: it breaks through (EV_CEILING, EV_ROOF), or widens the hole */
        if (ev & (EV_LAND | EV_TOPPING_LAND))
            for (int h = 0; h < HOLES; h++)
                if (ty - ROW_H >= HOLE_Y0[h] && ty - ROW_H < HOLE_Y1[h]) open_hole(v, p, h, top, tw->nlayers - 1);
    }
}

/* per frame, after the step: the cameras, the jiggle, the effects, the chef (called from the game's update) */
void draw_update(const match *m)
{
    for (int p = 0; p < m->players; p++) {
        const tower *tw = &m->tw[p];
        view *v = &V[p];
        int target = cam_target(v, tw);
        if (v->cam < target) v->cam += (target - v->cam + 7) / 8 > CAMERA_SPEED ? CAMERA_SPEED : (target - v->cam + 7) / 8;
        if (v->jiggle < 1000) v->jiggle++;
        if (v->top_squash > 0) v->top_squash--;
        if (v->chef_t > 0 && --v->chef_t == 0) v->chef_pose = CH_IDLE;
        if (v->chef_pose == CH_IDLE && tower_alive(tw) && tower_top(tw)->w < NARROW_PX) v->chef_pose = CH_PANIC, v->chef_t = 2;
        if (v->bottle_t && tw->state != TS_POUR) v->bottle_t = 0;
        else if (v->bottle_t) v->bottle_t++;
        /* syrup drips from a syrupy top */
        const layer *top = tower_top(tw);
        if ((top->flags & LF_SYRUP) && ++v->drip_t % 50 == 1) {
            fx_obj *d = fx_new(FX_DRIP, p, (v->drip_t / 50) & 1 ? top->x + top->w - 3 : top->x - 4, tower_top_y(tw) - 3);
            if (d) d->life = 70;
        }
        for (int i = 0; i < PIECES; i++) {
            piece *pc = &PC[p][i];
            if (!pc->on) continue;
            pc->t++;
            if (pc->t > 4 || pc->p.whole) {        /* it tips over the edge first, then falls */
                pc->vy -= 40;
                pc->y += pc->vy;
            }
            pc->x += pc->vx;
            pc->a += pc->va;
            if (line_of(v, pc->y / 256) > 280 || pc->t > 400) pc->on = 0;
        }
    }
    for (int i = 0; i < FX_MAX; i++) {
        fx_obj *f = &FXS[i];
        if (f->kind == FX_NONE) continue;
        f->t++;
        switch (f->kind) {
        case FX_PLASTER: case FX_ROOFTILE: case FX_CRUMBS: case FX_SPLINTER:
            f->vy -= 26;
            break;
        case FX_DRIP:
            if (f->t > 20) { f->vy -= 8; if (f->vy < -400) f->vy = -400; }
            break;
        case FX_PLUS:
            f->vy = f->vy * 15 / 16;
            break;
        case FX_TOPPING: {
            /* falls onto the top: an ease-in over TOPPING_FRAMES */
            f->vy = -(f->t * 150 * 2 * 256 / (TOPPING_FRAMES * TOPPING_FRAMES));
            break;
        }
        default: break;
        }
        f->x += f->vx;
        f->y += f->vy;
        if (f->t >= f->life) f->kind = FX_NONE;
    }
}

/* ---- sprites ---------------------------------------------------------------------------------------------------- */
static void draw_layer_sprites(view *v, int p, const tower *tw, int t)
{
    int n = tw->nlayers, wtop = wobble(v, n - 1, n, t);
    /* the top of the tower (it squashes on landing) */
    const layer *top = tower_top(tw);
    int mx0 = v->bg2_cx + top->x, box0 = mx0 - 1;     /* the box starts 1 px left of the pancake (the squash) */
    int key = layer_key(top, n - 1) * 2 + (v->top_squash > 0);
    int pal = 0;
    if (key != v->top_key) {
        render_layer_sprite(dyn_tile(p, 16), top, box0, mx0, v->top_squash > 0, &pal);
        v->top_key = key;
    }
    pal = top->kind == LK_PANCAKE ? OBJ_FOOD : OBJ_TOPPING;
    int sx = v->sx0 + (box0 - v->bg2_cx) + wtop + hu_shake_x();
    int sy = line_of(v, (n - 1) * ROW_H + 7) + hu_shake_y();
    for (int s = 0; s < 2 && s * 64 < top->w + 2; s++)
        if (sx + s * 64 < v->w && sx + s * 64 > -64 && sy > -8 && sy < RS_SCREEN_H)
            rs_spr(sx + s * 64, sy, dyn_tile(p, 16) + s * 8, 64, 8, pal, 2, 0);
    /* the butter pat on a perfect top */
    if (top->kind == LK_PANCAKE && (top->flags & LF_BUTTER) && tw->state != TS_MISSED) {
        int big = (top->flags & LF_BIG_BUTTER) != 0;
        spr(big ? SPR_BUTTER_BIG : SPR_BUTTER, v->sx0 + top->x + top->w / 2 - (big ? 12 : 8) + wtop + hu_shake_x(),
            sy - 7 + (v->top_squash > 0), 2, -1, 0, v->w);
    }
    /* the syrup on the top: the next pancake will slide */
    if (top->kind == LK_PANCAKE && (top->flags & LF_SYRUP) && tw->state != TS_MISSED)
        spr(SPR_GLAZE, v->sx0 + top->x + top->w / 2 - 8 + (top->flags & LF_BUTTER ? 10 : 0) + wtop + hu_shake_x(),
            sy - 5 + (v->top_squash > 0), 2, -1, 0, v->w);
    /* the slider */
    if (tw->state == TS_SLIDE || tw->state == TS_DROP || tw->state == TS_SLIP) {
        layer sl = {0, (int16_t)tw->sw, LK_PANCAKE, 0, (uint16_t)(tw->pancakes + 1)};
        int skey = tw->sw * 4096 + tw->pancakes;
        if (skey != v->slider_key) {
            render_layer_sprite(dyn_tile(p, 0), &sl, -1, 0, 0, &pal);
            v->slider_key = skey;
        }
        int x = v->sx0 + tower_slider_x(tw) - 1 + wtop + hu_shake_x();
        int y = line_of(v, tower_slider_y(tw) + 7) + hu_shake_y();
        for (int s = 0; s < 2 && s * 64 < tw->sw + 2; s++) rs_spr(x + s * 64, y, dyn_tile(p, 0) + s * 8, 64, 8, OBJ_FOOD, 2, 0);
    }
}

static void draw_pieces(view *v, int p)
{
    for (int i = 0; i < PIECES; i++) {
        piece *pc = &PC[p][i];
        if (!pc->on) continue;
        int a = (pc->a >> 4) & 63;
        /* big pieces tilt less: the nearest angle that fits the 128x16 box */
        int fit = a;
        for (int k = 0; k < 32 && !render_piece_fits(pc->p.w, fit); k++) fit = (a < 32 ? a - k : a + k) & 63;
        if (!render_piece_fits(pc->p.w, fit)) fit = 0;
        if (fit != pc->shown_a) {
            pc->halves = render_piece(dyn_tile(p, 32 + i * 32), &pc->p, fit);
            pc->shown_a = fit;
        }
        int x = v->sx0 + pc->x / 256 - 64, y = line_of(v, pc->y / 256) - 8;
        int pal = pc->p.kind == LK_PANCAKE ? OBJ_FOOD : OBJ_TOPPING;
        for (int s = 0; s < 2; s++)
            if (((pc->halves >> s) & 1) && x + s * 64 < v->w && x + s * 64 > -64 && y > -16 && y < RS_SCREEN_H)
                rs_spr(x + s * 64, y, dyn_tile(p, 32 + i * 32) + s * 16, 64, 16, pal, 2, 0);
    }
}

static void draw_fx(view *v, int p, int t)
{
    for (int i = 0; i < FX_MAX; i++) {
        const fx_obj *f = &FXS[i];
        if (f->kind == FX_NONE || f->player != p) continue;
        int x = v->sx0 + f->x / 256 + hu_shake_x(), y = line_of(v, f->y / 256) + hu_shake_y();
        switch (f->kind) {
        case FX_PLASTER: spr(SPR_PLASTER + f->frame % 3, x, y, 2, -1, 0, v->w); break;
        case FX_ROOFTILE: spr(SPR_ROOFTILE + (f->t / 6) % 2, x, y, 2, -1, (f->t / 12) % 2 ? RS_SPR_HFLIP : 0, v->w); break;
        case FX_DUST: spr(SPR_DUST + f->t * 3 / (f->life + 1), x - 8, y - 8, 2, -1, 0, v->w); break;
        case FX_BURST: spr(SPR_BURST + (f->t / 6) % 2, x - 4, y - 8, 2, -1, 0, v->w); break;
        case FX_SPARK: hu_sparkle(x, y - 6, (f->t / 5) % 2, 2); break;
        case FX_CRUMBS: spr(SPR_CRUMBS + (f->t / 4) % 2, x, y - 6, 2, -1, 0, v->w); break;
        case FX_DRIP: spr(SPR_DRIP + (f->t < 8 ? 0 : f->t < 18 ? 1 : 2), x, y, 2, -1, 0, v->w); break;
        case FX_PLUS: spr(SPR_PLUS + f->frame, x, y - 8, 2, -1, 0, v->w); break;
        case FX_SPLASH_IN: spr(SPR_SPLASH + (f->t / 6) % 2, x - 8, y - 8, 2, -1, 0, v->w); break;
        case FX_TOPPING: spr(SPR_TOPPING + f->frame, x, y - 16, 2, -1, 0, v->w); break;
        case FX_SPLINTER: spr(SPR_SPLINTER + (f->frame + f->t / 5) % 2, x, y, 2, -1, (f->t / 10) % 2 ? RS_SPR_HFLIP : 0, v->w); break;
        default: break;
        }
    }
    (void)t;
}

static void draw_bottle(view *v, const tower *tw, int t)
{
    if (!v->bottle_t) return;
    const layer *top = tower_top(tw);
    int ty = tower_top_y(tw), bx = v->sx0 + top->x + top->w / 2 - 4 + hu_shake_x();
    int in = v->bottle_t < 10 ? (10 - v->bottle_t) * 6 : 0;
    int out = v->bottle_t > POUR_FRAMES - 8 ? (v->bottle_t - (POUR_FRAMES - 8)) * 6 : 0;
    int by = line_of(v, ty + 44 + in + out);
    spr(SPR_BOTTLE + (t / 10) % 2, bx - 10, by - 16, 2, -1, 0, v->w);
    if (v->bottle_t >= 8 && v->bottle_t < POUR_FRAMES - 6)
        for (int y = by + 8; y < line_of(v, ty) - 2; y += 8) spr(SPR_STREAM, bx - 3, y, 2, -1, 0, v->w);
}

/* the chimney's smoke: puffs rise from the flue, grow and drift to the right (a function of the frame: no state) */
static void draw_smoke(view *v, int t)
{
    const int fx = CHIMNEY_X - PANO_CX, fy = CHIMNEY_TOP;
    if (line_of(v, fy) < -8 || line_of(v, fy + 100) > v->h + 8) return;
    for (int k = 0; k < 5; k++) {
        int age = (t + k * 40) % 200;
        int x = v->sx0 + fx - 8 + age / 6 + isin(age / 2 + k * 13) * 2 / 4096 + hu_shake_x();
        int y = line_of(v, fy + 4 + age * 2 / 5) + hu_shake_y();
        spr(SPR_SMOKE + clampi(age * 4 / 200, 0, 3), x, y - 12, 1, -1, 0, v->w);
    }
}

/* the sky's critters, behind the tower (priority 1: in front of the scenery, behind BG2) */
static void draw_critters(view *v, int t)
{
    int lo = v->cam - 40, hi = v->cam + v->h + 40;
    static const int birds[3][3] = {{432, 1, 90}, {505, -1, 140}, {580, 1, 70}};    /* world y, direction, speed */
    for (int i = 0; i < 3; i++) {
        int wy = birds[i][0] + isin(t / 3 + i * 20) * 6 / 4096;
        if (wy < lo || wy > hi) continue;
        int span = v->w + 64, x = (t * birds[i][2] / 100 + i * 97) % span;
        if (birds[i][1] < 0) x = span - x;
        spr(SPR_BIRD + (t / 8 + i) % 2, x - 32, line_of(v, wy), 1, -1, birds[i][1] < 0 ? RS_SPR_HFLIP : 0, v->w);
    }
    if (690 > lo && 690 < hi) {                                             /* the plane */
        int span = v->w + 400, x = span - (t / 2) % span;
        spr(SPR_PLANE, x - 32, line_of(v, 690), 1, -1, 0, v->w);
    }
    if (790 > lo && 790 < hi)                                               /* the weather balloon */
        spr(SPR_BALLOON, v->w * 3 / 4, line_of(v, 820 + isin(t / 4) * 5 / 4096), 1, -1, 0, v->w);
    if (1110 > lo && 1110 < hi) {                                           /* the satellite */
        int span = v->w + 600, x = (t / 3) % span;
        spr(SPR_SATELLITE, x - 32, line_of(v, 1110), 1, -1, 0, v->w);
    }
    int mx = v->w - 60, my = 1040;
    if (my + 32 > lo && my - 48 < hi) {
        spr(SPR_MOON, mx, line_of(v, my + 32), 1, -1, 0, v->w);
        /* the cow jumps over the moon every 6 s: a parabola from its left to its right */
        int ct = t % 360;
        if (ct < 120) {
            int x = mx - 44 + ct * 110 / 120, h = ct * (120 - ct) * 4 * 56 / (120 * 120);
            spr(SPR_COW + (ct > 20 && ct < 100), x, line_of(v, my + 14 + h), 1, -1, 0, v->w);
        }
    }
}

static void draw_chef(view *v, int p, int t, int players)
{
    static const int body[4] = {0, 2, 3, 4};
    int pose = v->chef_pose;
    int pal = p == 1 ? OBJ_CHEF2 : OBJ_CHEF;
    int kitchen_line = line_of(v, FLOOR_Y + 47);
    int dx = pose == CH_PANIC ? ((t / 3) & 1) : 0, dy = pose == CH_CHEER ? -((t / 6) & 1) * 2 : 0;
    if (players == 1 && kitchen_line < v->h - 40) {
        int f = body[pose];
        if (pose == CH_IDLE && (t % 180) < 8) f = 1;                    /* a blink */
        spr(SPR_CHEF + f, v->sx0 + CHEF_X + dx + hu_shake_x(), kitchen_line + dy + hu_shake_y(), 2, pal, 0, v->w);
        if (pose == CH_PANIC) spr(SPR_SWEAT, v->sx0 + CHEF_X + 26, kitchen_line + 6 + (t / 8) % 4, 2, -1, 0, v->w);
        v->portrait_t = 0;
        return;
    }
    /* the portrait: the chef keeps watching from a plate-rimmed frame in the bottom-left corner (it slides in) */
    if (v->portrait_t < 20) v->portrait_t++;
    int ox = 2, oy = v->h - 66 + hu_slide_in(v->portrait_t, 20, 80);
    int f = body[pose];
    if (pose == CH_IDLE && (t % 180) < 8) f = 1;
    spr(SPR_CHEF + f, ox + 8 + dx, oy + 10 + dy, 2, pal, 0, v->w);
    spr(SPR_RING, ox, oy, 2, -1, 0, v->w);
    spr(SPR_RING, ox + 24, oy, 2, -1, RS_SPR_HFLIP, v->w);
    spr(SPR_RING, ox, oy + 32, 2, -1, RS_SPR_VFLIP, v->w);
    spr(SPR_RING, ox + 24, oy + 32, 2, -1, RS_SPR_HFLIP | RS_SPR_VFLIP, v->w);
    if (pose == CH_PANIC) spr(SPR_SWEAT, ox + 36, oy + 10 + (t / 8) % 4, 2, -1, 0, v->w);
}

/* ---- the UI (the house kit) ------------------------------------------------------------------------------------- */
static void screen_text(const match *m, int state, int st_t, int best, int new_best)
{
    char s[48];
    if (state != shown_state || best != shown_best || m->players != shown_players) {
        hu_clear();
        title_logo_on = 0;
        if (state == DS_TITLE) {
            hu_logo("PANCAKE TOWER", title_ramps, 2, 2, 0);
            title_logo_on = 1;
            near_pal_last = 0;                         /* the logo has BG palette 5 (the kitchen does not use it) */
            snprintf(s, sizeof s, "BEST %d", best);
            if (best > 0) hu_text(hu_center(s, 0), 24, s);
            hu_copyright(28);
        }
        if (state == DS_READY) {
            if (m->players == 1) hu_get_ready(6);
            else {
                hu_big(5, 6, "READY", HU_BIG_FREE);
                hu_big(25, 6, "READY", HU_BIG_FREE);
            }
        }
        if (state == DS_OVER) {
            hu_banner(4, "GAME OVER");
            if (m->players == 1) {
                hu_gameover_panel(1, new_best, m->tw[0].score, 0);
                snprintf(s, sizeof s, "%d PANCAKES", m->tw[0].pancakes);
                hu_box_text(12, 12, s);
            } else {
                /* the kit's 2-player panel, with our winner: the tallest tower (a tie on height: the score) */
                hu_panel(10, 9, 20, 12);
                hu_box_text(12, 11, "PLAYER 1");
                hu_box_text(12, 14, "PLAYER 2");
                int w = match_winner(m);
                const char *win = w == 0 ? "P1 WINS!" : w == 1 ? "P2 WINS!" : "DRAW!";
                hu_box_text(20 - (int)strlen(win) / 2, 17, win);
            }
        }
        shown_state = state;
        shown_best = best;
        shown_players = m->players;
    }
    if (state == DS_TITLE || state == DS_READY) {
        if (m->players == 1 || state == DS_TITLE) hu_prompt(21, "PRESS A TO DROP", st_t);
        else {
            if (hu_blink(st_t)) { hu_text(5, 21, "A: DROP"); hu_text(25, 21, "A: DROP"); }
            else hu_clear_rows(21, 1);
        }
        if (state == DS_READY || m->players == 2) hu_join_line(26, m->players, "VERSUS!");
    }
    if (state == DS_OVER) hu_retry_line(st_t, RETRY_LOCK, "A: STACK AGAIN");
}

/* the kit draws its sprites at screen coordinates; in a viewport they are relative to its corner */
static void shift_oam(int first, int dx, int dy)
{
    for (int i = first; i < rs_oam_next(); i++) {
        rs_sprite *s = rs_oam(i);
        if (s && s->used) { s->x = (int16_t)(s->x - dx); s->y = (int16_t)(s->y - dy); }
    }
}

static int fork_medal_tier(int score)
{
    static const int th[4] = {MEDAL_BRONZE, MEDAL_SILVER, MEDAL_GOLD, MEDAL_PEARL};
    return hu_medal_of(score, th);
}

static void draw_view_sprites(const match *m, int p, int state, int st_t, int t)
{
    view *v = &V[p];
    const tower *tw = &m->tw[p];
    if (state == DS_PLAY) hu_number(tw->pancakes, v->w / 2, 10, 3);
    if (state == DS_READY && m->players == 2) hu_glyph(HU_BTN_A, 40 - 12 + 0, 21 * 8 - 4, (t / 30) % 2, 3);
    draw_chef(v, p, t, m->players);
    draw_fx(v, p, t);
    draw_layer_sprites(v, p, tw, t);
    draw_bottle(v, tw, t);
    draw_pieces(v, p);
    draw_smoke(v, t);
    draw_critters(v, t);
}

void draw_frame(const match *m, int state, int st_t, int best, int new_best, int paused)
{
    int t = (int)rs_frame_count();
    for (int p = 0; p < m->players; p++) {
        view *v = &V[p];
        const tower *tw = &m->tw[p];
        stream_near(v, p);
        stream_far(v);
        stream_tower(v, p, tw);
        sky_update(v);
        int n = tw->nlayers;
        for (int l = 0; l < v->h; l++) {               /* the table is by screen line */
            int wy = v->cam + v->h - 1 - l, i = wy >= 0 ? wy / ROW_H : -1;
            v->wob_dx[v->y0 + l] = (int16_t)(i >= 0 && i < n ? -wobble(v, i, n, t) : 0);
        }
    }
    screen_text(m, state, st_t, best, new_best);
    if (!title_logo_on && !near_pal_last) {            /* the attic, the roof and the sky's palette */
        static const int slots[NEAR_PALS] = NEAR_PAL_SLOTS;
        rs_pal_load(RS_PAL_BG(slots[NEAR_PALS - 1]), pt_near_pals + (NEAR_PALS - 1) * 16, 16);
        near_pal_last = 1;
    }
    int slide = state == DS_OVER ? hu_slide_in(st_t, 20, 200) : 0;     /* the panel slides up, ease-out */
    hu_pause(paused, 13);
    int shx = hu_shake_x(), shy = hu_shake_y();

    rs_oam_clear();
    if (m->players == 1) {
        view *v = &V[0];
        rs_viewports(0, NULL, 0);
        rs_bg_scroll(RS_BG1, 0, -slide);
        rs_bg_scroll(RS_BG2, (v->bg2_cx - v->sx0 - shx) & 255, (-v->cam - v->h - shy) & 255);
        rs_bg_scroll(RS_BG3, (-shx) & 511, (-v->cam - v->h - shy) & 255);
        rs_bg_scroll(RS_BG4, 0, (-far_base(v) - v->h) & 255);
        rs_bg_line_scroll(RS_BG2, v->wob_dx, NULL);
        /* front to back: the UI, then the world */
        if (state == DS_OVER) {
            hu_gameover_sprites(1, m->tw[0].score, 0, best, 0, st_t, slide);
            int medal = fork_medal_tier(m->tw[0].score);
            if (medal && !medals_loaded) {
                int t0 = pt_spr[SPR_MEDAL].tile;
                rs_tiles_load(VR_OBJ + t0, pt_obj_tiles + t0 * RS_TILE_BYTES, pt_obj_tile_count - t0);
                medals_loaded = 1;
            }
            if (medal) {
                hu_box_text(22, 17, " ");               /* the kit wrote "-": our fork medal instead */
                spr(SPR_MEDAL + medal - 1, 22 * 8 - 4, 16 * 8 - 4 + slide, 3, -1, 0, RS_SCREEN_W);
                if ((st_t / 20) % 3 == 0) hu_sparkle(22 * 8 + 12, 16 * 8 - 4 + slide, (st_t / 10) % 2, 3);
            } else {
                hu_box_text(22, 17, "-");
            }
        }
        if (state == DS_TITLE || state == DS_READY)
            hu_glyph(HU_BTN_A, hu_center("PRESS A TO DROP", 0) * 8 - 12, 21 * 8 - 4, (t / 30) % 2, 3);
        draw_view_sprites(m, 0, state, st_t, t);
        return;
    }
    /* two players: two viewports (and the game-over panel in two more over them) */
    int nv = rs_viewport_layout(2, 0, vps);
    for (int p = 0; p < 2; p++) {
        view *v = &V[p];
        rs_viewport *vp = &vps[p];
        vp->layers = 0x0f;
        vp->objs = 1;
        vp->sx[0] = vp->x, vp->sy[0] = 0;
        vp->sx[1] = (int16_t)((v->bg2_cx - v->sx0 - shx) & 255), vp->sy[1] = (int16_t)((-v->cam - v->h - shy) & 255);
        vp->sx[2] = (int16_t)((v->near_col0 * 8 - shx) & 511), vp->sy[2] = (int16_t)((-v->cam - v->h - shy) & 255);
        vp->sx[3] = (int16_t)(v->far_col0 * 8 + 48), vp->sy[3] = (int16_t)((-far_base(v) - v->h) & 255);
        vp->oam_first = (uint16_t)rs_oam_next();
        draw_view_sprites(m, p, state, st_t, t);
        vp->oam_count = (uint16_t)(rs_oam_next() - vp->oam_first);
    }
    nvps = nv;
    if (state == DS_OVER) {
        static const int16_t rect[2][4] = {{80, 32, 160, 32}, {80, 72, 160, 96}};
        for (int k = 0; k < 2; k++) {
            rs_viewport *vp = &vps[nvps++];
            memset(vp, 0, sizeof *vp);
            vp->x = rect[k][0], vp->y = rect[k][1], vp->w = rect[k][2], vp->h = rect[k][3];
            vp->layers = 1;
            vp->sx[0] = vp->x, vp->sy[0] = (int16_t)(vp->y - slide);
            vp->objs = (uint8_t)(k == 1);
            if (k == 1) {
                vp->oam_first = (uint16_t)rs_oam_next();
                hu_gameover_sprites(2, m->tw[0].pancakes, m->tw[1].pancakes, 0, 0, st_t, slide);
                shift_oam(vp->oam_first, vp->x, vp->y);
                vp->oam_count = (uint16_t)(rs_oam_next() - vp->oam_first);
            }
        }
    }
    rs_viewports(nvps, vps, RS_RGB8(22, 18, 40));
    (void)paused;
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(V); S(PC); S(FXS); S(nviews); S(shown_state); S(shown_best); S(shown_players); S(title_logo_on); S(vps); S(nvps);
    S(near_pal_last); S(medals_loaded);
    S(fx_seed); S(opt_nodraw_bg);
    RS_STATE_RASTER(raster);
}
#undef S

/* after a load: the streamed rows and the dynamic sprite tiles are in the saved VRAM already */
void draw_state_loaded(void) {}

/* the player's part of the screen (the bot looks there): its viewport's sprites, or every sprite in 1 player */
void draw_view_oam(int player, int *first, int *count)
{
    if (nviews < 2 || player >= nvps) { *first = 0; *count = RS_OAM_MAX; return; }
    *first = vps[player].oam_first;
    *count = vps[player].oam_count;
}

/* development: log the sprites of a line that has more than 32 (--opt oamlog=1) */
static int oam_logged;
void draw_oam_log(void)
{
    if (oam_logged || !rs_option_int("oamlog", 0)) return;
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int n = 0;
        for (int i = 0; i < RS_OAM_MAX; i++) {
            rs_sprite *s = rs_oam(i);
            int oy = 0;
            for (int k = 0; k < nvps; k++)
                if (i >= vps[k].oam_first && i < vps[k].oam_first + vps[k].oam_count) oy = vps[k].y;
            if (s && s->used && y >= s->y + oy && y < s->y + oy + s->h) n++;
        }
        if (n > 32) {
            oam_logged = 1;
            for (int i = 0; i < RS_OAM_MAX; i++) {
                rs_sprite *s = rs_oam(i);
                if (s && s->used && y >= s->y && y < s->y + s->h) rs_log("oam %d: tile %d at %d,%d %dx%d", i, s->tile, s->x, s->y, s->w, s->h);
            }
            return;
        }
    }
}

/* test hook (tests/test_break.c): where view p is on the screen, its camera and its holes */
void draw_test_view(int p, pt_view_info *o)
{
    const view *v = &V[p];
    memset(o, 0, sizeof *o);
    o->cam = v->cam, o->w = v->w, o->h = v->h, o->sx0 = v->sx0, o->pcol0 = v->near_pcol0, o->ncols = v->near_cols;
    o->x = nviews > 1 && p < nvps ? vps[p].x : 0;
    o->y = nviews > 1 && p < nvps ? vps[p].y : 0;
    o->shx = hu_shake_x(), o->shy = hu_shake_y();
    o->anchor = anchor_layer(v);
    for (int h = 0; h < HOLES; h++) o->hole_on[h] = v->hole[h].on, o->hx0[h] = v->hole[h].x0, o->hx1[h] = v->hole[h].x1;
}
