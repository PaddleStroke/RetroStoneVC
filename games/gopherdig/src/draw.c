/*
 * Gopher Dig: video. The house layout (docs/art-direction.md), the well seen from the side:
 *   BG1 the UI (the house kit, priority 1; the air gauge's tiles after the kit's glyph pool);
 *   BG4 the well: the blocks standing still, the dug-out back wall, the earth around the well with its stone lining,
 *       roots, burrows, side tunnels and the buried things of each stratum, the grass and the clouds on top;
 *   BG3 the blocks shaking (the whole layer jiggles a pixel left and right);
 *   BG2 the blocks falling (the whole layer slides down with the well's fall clock: every falling block of a well moves
 *       a row on the same tick, so one vertical offset draws them all);
 *   the backdrop: the sky above the grass (a raster gradient), the back wall's colour below (round block corners).
 *   The three well layers share one tile bank; each stratum's earth, rocks and things take its palettes (BG 3 and 6),
 *   loaded by the raster callback line by line, so a stratum's border shows where it is, in every view.
 *   Sprites: the gophers (OBJ 0; players 2-4 OBJ 1, 4, 5: the hard hat recoloured) and their drill bits, the air
 *   bubbles, dust, sparkles and stars (OBJ 2), the blocks' crumbs (OBJ 6), the kit's digits, glyphs and medals (OBJ 3).
 * The map: a ring of 16 block rows (32 tile rows) per layer; each player's well has a region of the map's width
 *   (1 player: all of it, 2: half, 3-4: a quarter), the well centred in it; the rows coming into view are written.
 * Views: 1 player the full screen; 2 side by side; 3-4 in quarters (3: the 4th quarter shows the race: who is deepest).
 *   The title and the panels: one full view of player 1's well.
 * Widescreen (docs/spec.md "Widescreen"): the 320-px stage is centred; a wider screen shows more of the earth around the
 *   well (its roots, burrows, tunnels and buried things); the well and the rules are the same; the UI kit centres itself.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

/* the title logo: GOPHER in fur brown, DIG in hard-hat yellow (light, mid, dark) */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(244, 200, 140), RS_RGB8(210, 146, 88), RS_RGB8(156, 96, 52)},
    {RS_RGB8(255, 244, 150), RS_RGB8(252, 212, 64), RS_RGB8(222, 156, 26)}};
#define SKY_TOP     0x3a7cd0        /* the sky: high up, and at the grass */
#define SKY_LOW     0xbfe6f8
#define DIVIDER     0x0c142c
#define TITLE_CAM   (-232)          /* the title's camera: the grass at the bottom */
#define CAM_FOLLOW  4
#define WELL_PX     (COLS * CELL)
#define SPR_BUDGET  120             /* sprites in all (the guideline: 128) */

/* ---- views ------------------------------------------------------------------------------------------------------------- */
int view_layout(int players, vrect out[MAX_PLAYERS])
{
    rs_viewport v[4];
    int n = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    int k = rs_viewport_layout(n >= 3 ? 4 : n, 0, v);
    for (int i = 0; i < k; i++) {
        out[i].x = v[i].x;
        out[i].y = v[i].y;
        out[i].w = v[i].w;
        out[i].h = v[i].h;
    }
    return k;
}

int view_well_x(const vrect *v) { return v->x + (v->w - WELL_PX) / 2; }
int view_gopher_y(const vrect *v) { return v->h >= 200 ? 76 : 34; }

/* ---- state ----------------------------------------------------------------------------------------------------------------- */
typedef struct vline { int32_t cam_y; int16_t y0, theme; } vline;   /* the raster's view of each viewport (cam_y: a
                                                                     * well deeper than 2047 m is past 32767 px) */
static vline VL[RS_VIEW_MAX];
static rs_viewport VP[RS_VIEW_MAX];
static int views_on;
static int cam[MAX_PLAYERS], cam_on[MAX_PLAYERS];
static int map_nreg, map_mw;                 /* the map's layout: regions, width in tiles */
static int16_t slot_row[MAX_PLAYERS][16];    /* the row each ring slot of BG4's sides holds (-32768: none) */
static int ras_theme = -1;                   /* the stratum palettes in CGRAM */
static int shown_key = -1, shown_best = -1, text_paused;
static uint32_t shown_text;
static int drawn_width;                      /* the width the maps were set up for (state_audit.txt) */

/* the effects: from the world's fx list, animated here (well pixels, by player) */
/* x, y in 1/16 px of the well: int32 (in int16 they wrapped from 128 m down, and the effects were lost) */
typedef struct part { int32_t x, y; int16_t vx, vy, t, kind, colour, player; } part;
#define PARTS 64
static part parts[PARTS];
static int last_fx_t = -1;
enum { PT_POP, PT_CRUMB, PT_SPARK, PT_PUFF, PT_DUST };

static int jiggle(int t) { int k = (t / 2) & 3; return k == 1 ? 1 : k == 3 ? -1 : 0; }   /* shaking blocks */

static int fdiv(int a, int b) { return a >= 0 ? a / b : -((b - 1 - a) / b); }

static uint32_t hash3(int a, int b, int c)
{
    uint32_t x = (uint32_t)a * 0x9e3779b1u ^ (uint32_t)b * 0x85ebca6bu ^ (uint32_t)c * 0xc2b2ae35u;
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static int theme_of_row(int r) { return stratum_of_row(r) % STRATA_ART; }

/* ---- the raster: the backdrop (sky or back wall) and the strata palettes, per line and per view ---------------------- */
static rs_color sky_at(int wy)
{
    int k = wy < -240 ? 0 : (wy + 240) * 256 / 240, c[3];
    int a[3] = {(SKY_TOP >> 16) & 255, (SKY_TOP >> 8) & 255, SKY_TOP & 255};
    int b[3] = {(SKY_LOW >> 16) & 255, (SKY_LOW >> 8) & 255, SKY_LOW & 255};
    for (int i = 0; i < 3; i++) c[i] = (a[i] * (256 - k) + b[i] * k) / 256;
    int d = (wy & 4) ? 1 : 0;                   /* house rule: a 2-line dither step every 8 lines */
    return RS_RGB8(c[0] + 4 * d, c[1] + 2 * d, c[2] + 4 * d);
}

static void raster(int line, void *user)
{
    (void)user;
    int v = rs_viewport_current();
    const vline *L = &VL[v < 0 ? 0 : v];
    int wy = L->cam_y + line - L->y0;
    int th = L->theme >= 0 ? L->theme : theme_of_row(fdiv(wy, CELL));
    if (th != ras_theme) {
        rs_pal_load(RS_PAL_BG(PAL_STRATUM), (const rs_color *)gm_strata_pal + th * 16, 16);
        rs_pal_load(RS_PAL_BG(PAL_DECOR), (const rs_color *)gm_decor_pal + th * 16, 16);
        ras_theme = th;
    }
    rs_pal_set(0, wy < 0 ? sky_at(wy) : gm_strata_pal[th * 16 + 6]);
}

/* ---- the map ---------------------------------------------------------------------------------------------------------------- */
static void maps_setup(int nreg)
{
    int mw = (nreg >= 4 || rs_screen_width() > 512) ? 128 : 64;
    map_nreg = nreg;
    map_mw = mw;
    rs_bg_setup(RS_BG1, rs_screen_width() > 512 ? 128 : 64, 32, VR_BG1);
    rs_bg_setup(RS_BG2, mw, 32, VR_WELL);
    rs_bg_setup(RS_BG3, mw, 32, VR_WELL);
    rs_bg_setup(RS_BG4, mw, 32, VR_WELL);
    for (int p = 0; p < MAX_PLAYERS; p++)
        for (int s = 0; s < 16; s++) slot_row[p][s] = -32768;
    shown_key = -1;
    shown_text = 0;
    drawn_width = rs_screen_width();
}

static int region_tiles(void) { return map_mw / map_nreg; }
static int well_tile_x(int p) { return (p % map_nreg) * region_tiles() + (region_tiles() - 2 * COLS) / 2; }

static void put2x2(int layer, int tx, int ty, const uint16_t m[4])
{
    int mask = map_mw - 1;
    rs_bg_put(layer, tx & mask, ty, m[0]);
    rs_bg_put(layer, (tx + 1) & mask, ty, m[1]);
    rs_bg_put(layer, tx & mask, ty + 1, m[2]);
    rs_bg_put(layer, (tx + 1) & mask, ty + 1, m[3]);
}

static const uint16_t ZERO4[4] = {0, 0, 0, 0};

/* a side tunnel on row r (side 0 left, 1 right): its length in cells (0: none) */
static int tunnel_len(int r, int side)
{
    if (r < 4) return 0;
    uint32_t h = hash3(r, side, 77);
    return h % 17 == 0 ? 2 + (int)((h >> 8) % 3) : 0;
}

/* the cell k of row r beside the well (k 0: against the stone lining) */
static const uint16_t *side_cell(int r, int side, int k)
{
    uint32_t h = hash3(r, side * 1000 + k, 5);
    if (r < -1) {
        int rr = -r;                                    /* clouds, in pairs */
        if (rr >= 4 && rr <= 9 && (hash3(r, side, k >> 1) % 9) == 0)
            return gm_cell[((k & 1) ^ side) ? CELL_CLOUD_L : CELL_CLOUD_R];
        return ZERO4;
    }
    if (r == -1) {
        static const int surf[8] = {CELL_TUFT, -1, CELL_FLOWERS0, -1, CELL_TUFT, CELL_FLOWERS1, -1, CELL_MUSHROOM};
        int c = surf[h % 8];
        return c < 0 ? ZERO4 : gm_cell[c];
    }
    if (r == 0) return gm_cell[CELL_GRASS0 + (h & 1)];
    int tl = tunnel_len(r, side);
    if (tl) {
        if (k < tl - 1) return gm_cell[CELL_TUNNEL];
        if (k == tl - 1) return gm_cell[side ? CELL_TUNNEL_END_R : CELL_TUNNEL_END_L];
        if (k == tl) return gm_decor[theme_of_row(r)][3];       /* something buried at the tunnel's end */
    }
    int roll = (int)(h % 100);
    if (roll < 7 && k >= 1) return gm_decor[theme_of_row(r)][(h >> 8) % 3];
    if (roll < 15) {
        static const int gen[4] = {CELL_PEBBLES, CELL_ROOTS, CELL_BURROW, CELL_CRACK};
        int g = gen[(h >> 8) % 4];
        if (g == CELL_ROOTS && r > 40) g = CELL_PEBBLES;        /* roots near the grass only */
        return gm_cell[g];
    }
    return gm_cell[CELL_EARTH0 + (int)((h >> 12) & 3)];
}

/* BG4's sides of ring slot (row r) in player p's region */
static void write_sides(int p, int r)
{
    int rt = region_tiles(), x0 = (p % map_nreg) * rt, wl = well_tile_x(p), ty = (r & 15) * 2;
    int ncell = (wl - 1 - x0) / 2;
    for (int side = 0; side < 2; side++) {
        int ex = side ? wl + 2 * COLS : wl - 1;
        uint16_t e0 = 0, e1 = 0;
        if (r >= 0) {
            int c = tunnel_len(r, side) ? (side ? CELL_EDGE_OPEN_R : CELL_EDGE_OPEN_L)
                                        : (side ? CELL_EDGE_R0 : CELL_EDGE_L0) + (r & 1);
            e0 = gm_cell[c][0];
            e1 = gm_cell[c][2];
        }
        rs_bg_put(RS_BG4, ex & (map_mw - 1), ty, e0);
        rs_bg_put(RS_BG4, ex & (map_mw - 1), ty + 1, e1);
        for (int k = 0; k < ncell; k++) {
            int tx = side ? wl + 2 * COLS + 1 + 2 * k : wl - 3 - 2 * k;
            put2x2(RS_BG4, tx, ty, side_cell(r, side, k));
        }
        /* an odd tile left at the region's edge: earth */
        int used = 1 + 2 * ncell, left = (side ? x0 + rt - (wl + 2 * COLS) : wl - x0) - used;
        for (int i = 0; i < left; i++) {
            int tx = side ? wl + 2 * COLS + used + i : x0 + i;
            const uint16_t *m = side_cell(r, side, ncell);
            rs_bg_put(RS_BG4, tx & (map_mw - 1), ty, m[0]);
            rs_bg_put(RS_BG4, tx & (map_mw - 1), ty + 1, m[2]);
        }
    }
}

/* the blocks: a colour block's map entries by the neighbours it sticks to (same colour, same layer) */
static int same(const well *w, int c, int r, int kind, int st)
{
    const gcell *x = well_cell(w, c, r);
    return x && x->kind == kind && x->st == st;
}

static void block_entries(const well *w, int c, int r, const gcell *x, uint16_t m[4])
{
    int k = x->kind, st = x->st;
    if (k == K_HARD) {
        memcpy(m, gm_cell[CELL_HARD0 + (x->hits < 4 ? x->hits : 4)], sizeof(uint16_t) * 4);
        return;
    }
    int L = same(w, c - 1, r, k, st), R = same(w, c + 1, r, k, st), U = same(w, c, r - 1, k, st),
        D = same(w, c, r + 1, k, st);
    int UL = L && U && same(w, c - 1, r - 1, k, st), UR = R && U && same(w, c + 1, r - 1, k, st);
    int DL = L && D && same(w, c - 1, r + 1, k, st), DR = R && D && same(w, c + 1, r + 1, k, st);
    const uint16_t *q = gm_block_q + (k - K_RED) * 32;
    m[0] = q[0 * 8 + L + 2 * U + 4 * UL];
    m[1] = q[1 * 8 + R + 2 * U + 4 * UR];
    m[2] = q[2 * 8 + L + 2 * D + 4 * DL];
    m[3] = q[3 * 8 + R + 2 * D + 4 * DR];
}

/* the well part of a row: BG4 (still blocks, or the back wall / the sky), BG3 (the shaking blocks) */
static void write_row_still(const well *w, int p, int r)
{
    int wl = well_tile_x(p), ty = (r & 15) * 2;
    for (int c = 0; c < COLS; c++) {
        const gcell *x = well_cell(w, c, r);
        uint16_t m4[4], m3[4] = {0, 0, 0, 0};
        if (r < 0) {
            int rr = -r, pair = c >> 1;
            if (rr >= 5 && rr <= 8 && c < COLS - 1 && hash3(r, 9, pair) % 5 == 0)
                memcpy(m4, gm_cell[(c & 1) ? CELL_CLOUD_R : CELL_CLOUD_L], sizeof m4);
            else
                memset(m4, 0, sizeof m4);
        } else if (x && x->kind != K_EMPTY && x->kind != K_AIR && x->st == C_STILL) {
            block_entries(w, c, r, x, m4);
        } else {
            const gcell *a = well_cell(w, c, r - 1);
            int shadow = a && a->kind != K_EMPTY && a->kind != K_AIR && a->st == C_STILL;
            memcpy(m4, gm_cell[(shadow ? CELL_BACKSH0 : CELL_BACK0) + ((c + r) & 1)], sizeof m4);
        }
        if (x && x->kind != K_EMPTY && x->kind != K_AIR && x->st == C_SHAKE) block_entries(w, c, r, x, m3);
        put2x2(RS_BG4, wl + 2 * c, ty, m4);
        put2x2(RS_BG3, wl + 2 * c, ty, m3);
    }
}

static void write_row_falling(const well *w, int p, int r)
{
    int wl = well_tile_x(p), ty = (r & 15) * 2;
    for (int c = 0; c < COLS; c++) {
        const gcell *x = well_cell(w, c, r);
        uint16_t m[4] = {0, 0, 0, 0};
        if (x && x->kind != K_EMPTY && x->kind != K_AIR && x->st == C_FALL) block_entries(w, c, r, x, m);
        put2x2(RS_BG2, wl + 2 * c, ty, m);
    }
}

/* the rows of player p's well that a view of height vh at camera cy shows */
static void write_view(const well *w, int p, int cy, int vh)
{
    int r0 = fdiv(cy, CELL), r1 = fdiv(cy + vh - 1, CELL);
    for (int r = r0; r <= r1; r++) {
        if (slot_row[p][r & 15] != r) {
            write_sides(p, r);
            slot_row[p][r & 15] = (int16_t)r;
        }
        write_row_still(w, p, r);
    }
    int f = cy + CELL - well_fall_px(w);
    for (int r = fdiv(f, CELL); r <= fdiv(f + vh - 1, CELL); r++) write_row_falling(w, p, r);
}

/* ---- init --------------------------------------------------------------------------------------------------------------- */
/* the air gauge's tiles on BG1, in the UI palette: a bubble, then the bar filled 0..8 px (blue, then red) */
static void bar_tiles(void)
{
    uint8_t px[20 * 64];
    memset(px, 0, sizeof px);
    for (int i = 0; i < 64; i++) {                              /* the bubble */
        int x = i % 8, y = i / 8, dx = 2 * x - 7, dy = 2 * y - 7, d = dx * dx + dy * dy;
        if (d <= 49) px[i] = d >= 25 ? HU_OUTLINE : d >= 9 ? HU_LIGHT : HU_CREAM;
    }
    for (int f = 0; f <= 8; f++)
        for (int k = 0; k < 2; k++) {
            uint8_t *t = px + (1 + k * 9 + f) * 64;
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    int v = (y == 0 || y == 7) ? HU_DARK : x < f ? (y == 1 ? HU_CREAM : k ? HU_RED : HU_LIGHT) : HU_FILL;
                    t[y * 8 + x] = (uint8_t)v;
                }
        }
    uint8_t *cap = px + 19 * 64;                                /* the bar's right end */
    for (int y = 0; y < 8; y++) cap[y * 8] = HU_DARK, cap[y * 8 + 1] = (y > 0 && y < 7) ? HU_DARK : 0;
    rs_tiles_load8(VR_BG1 + BAR_TILE, px, 20);
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());       /* widescreen: the 320-px stage centred, more earth on both sides */
    hu_config hc = hu_defaults();               /* BG1 at VRAM 0, the kit at 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    bar_tiles();
    rs_tiles_load(VR_WELL, gm_well_tiles, gm_well_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_BLOCK_A), (const rs_color *)gm_block_pals, 32);
    rs_pal_load(RS_PAL_BG(PAL_SURFACE), (const rs_color *)gm_surface_pal, 16);
    maps_setup(1);
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), (const rs_color *)gm_obj_pals, 48);     /* P1, P2, the props (the kit loaded OBJ 3) */
    rs_pal_load(RS_PAL_OBJ(4), (const rs_color *)gm_obj_pals + 64, 64);   /* P3, P4, the crumbs */
    rs_raster(raster, NULL);
}

void draw_new_run(const world *w)
{
    (void)w;
    memset(cam_on, 0, sizeof cam_on);
    memset(parts, 0, sizeof parts);
    shown_text = 0;
}

int draw_cam_y(int p) { return cam_on[p] ? cam[p] : TITLE_CAM; }

/* ---- the effects --------------------------------------------------------------------------------------------------------- */
static void add_part(int kind, int x, int y, int vx, int vy, int t, int colour, int player)
{
    int slot = 0;
    for (int i = 0; i < PARTS; i++)
        if (parts[i].t < parts[slot].t) slot = i;
    part *q = &parts[slot];
    q->x = x * 16;
    q->y = y * 16;
    q->vx = (int16_t)vx;
    q->vy = (int16_t)vy;
    q->t = (int16_t)t;
    q->kind = (int16_t)kind;
    q->colour = (int16_t)colour;
    q->player = (int16_t)player;
}

static void parts_step(const world *w)
{
    for (int i = 0; i < PARTS; i++) {
        part *q = &parts[i];
        if (q->t <= 0) continue;
        q->t--;
        q->x += q->vx;
        q->y += q->vy;
        if (q->kind == PT_CRUMB) q->vy = (int16_t)(q->vy + 3);
    }
    if (w->t == last_fx_t) return;
    last_fx_t = w->t;
    for (int f = 0; f < w->nfx; f++) {
        const fx *e = &w->fx[f];
        int x = e->x + 8, y = e->y + 8, s = (e->n * 37 + f * 11) & 15;
        switch (e->kind) {
        case FX_POP:
        case FX_CHAIN:
            add_part(PT_POP, x - 8, y - 8, 0, 0, 10, e->colour, e->player);
            add_part(PT_CRUMB, x - 4, y - 4, (e->n & 1) ? -14 - s : 14 + s, -30 - s, 22, e->colour, e->player);
            break;
        case FX_HARD:
            for (int k = 0; k < 4; k++)
                add_part(PT_CRUMB, x - 4, y - 4, (k - 2) * 12 + 6, -34 + k * 4, 24, 5, e->player);
            add_part(PT_DUST, x - 4, y - 4, 0, -4, 18, 0, e->player);
            break;
        case FX_CRACK:
            add_part(PT_SPARK, x - 4, y - 4, 0, 0, 10, 0, e->player);
            break;
        case FX_AIR:
            for (int k = 0; k < 3; k++) add_part(PT_PUFF, x - 4 + (k - 1) * 5, y - 4, (k - 1) * 3, -10 - k * 3, 30, 0, e->player);
            break;
        case FX_CRUSH:
            add_part(PT_DUST, x - 12, y, -8, -2, 20, 0, e->player);
            add_part(PT_DUST, x + 4, y, 8, -2, 20, 0, e->player);
            break;
        }
    }
}

/* ---- sprites --------------------------------------------------------------------------------------------------------------- */
typedef struct sview { int ox, oy, w, h, wx, cy, budget, lite; } sview;   /* lite: split screen, fewer effects; sprite coords of the view's corner; the well's x */

static int spr_at(sview *s, int id, int x, int y, int pal, int prio, int flags)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x >= s->ox + s->w || y >= s->oy + s->h || x + d->w <= s->ox || y + d->h <= s->oy || s->budget <= 0) return 0;
    s->budget--;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
    return 1;
}

static int player_pal(int p) { return p ? hu_player_pal(p) : -1; }

static int gopher_sprite(const gopher *g, int t)
{
    if (!g->alive) return g->out == OUT_CRUSH ? SPR_FLAT : SPR_FAINT;
    if (g->cheer_t > 0) return SPR_CHEER + (t / 8) % 2;
    switch (g->act) {
    case ACT_WALK: return SPR_WALK + (g->act_t * 2 / (g->act_n ? g->act_n : 1)) % 2;
    case ACT_CLIMB: return SPR_CLIMB;
    case ACT_FALL: return SPR_FALL;
    case ACT_DRILL: return (g->drill_dir == 2 ? SPR_DIG : SPR_DRILL) + (g->act_t / 3) % 2;
    default:
        if (g->aim == AIM_DOWN) return SPR_DIG;
        return SPR_IDLE + ((t / 40) % 3 == 2);
    }
}

/* player p's well in a view: the gopher, its drill, the air bubbles, the effects */
static void draw_well_sprites(sview *s, const world *W, int p, int t, int sx, int sy, int show_gopher)
{
    const well *w = &W->w[p];
    const gopher *g = &w->go;
    int bx = s->ox + s->wx + sx, by = s->oy - s->cy + sy;
    /* effects in front */
    for (int i = 0; i < PARTS; i++) {
        const part *q = &parts[i];
        if (q->t <= 0 || q->player != p) continue;
        int x = bx + q->x / 16, y = by + q->y / 16;
        switch (q->kind) {
        case PT_POP: if (!s->lite || !(i & 1)) spr_at(s, SPR_POP + (q->t < 5), x, y, -1, 2, 0); break;
        case PT_CRUMB: if (!s->lite) spr_at(s, SPR_CRUMB + (q->colour >= K_RED && q->colour <= K_BLUE ? (q->colour - K_RED) * 2 : 8) + ((q->t / 4) & 1), x, y, -1, 2, 0); break;
        case PT_SPARK: spr_at(s, SPR_SPARK + ((q->t / 3) & 1), x, y, -1, 2, 0); break;
        case PT_PUFF: spr_at(s, SPR_PUFF + (q->t < 15), x, y, -1, 2, 0); break;
        case PT_DUST: spr_at(s, SPR_DUST + (q->t < 12) + (q->t < 6), x, y, -1, 2, 0); break;
        }
    }
    if (show_gopher) {
        int gx = bx + g->x, gy = by + g->y;
        int flip = g->face < 0 ? RS_SPR_HFLIP : 0;
        if (!g->alive && g->out == OUT_CRUSH)
            for (int k = 0; k < 3; k++) {                       /* stars around the flattened gopher */
                int a = (t * 3 + k * 85) & 255, ox = (a < 128 ? a : 255 - a) / 8 - 8;
                spr_at(s, SPR_STAR + ((t / 6 + k) & 1), gx + 4 + ox, gy + 2 - (k & 1) * 3, -1, 2, 0);
            }
        if (g->alive && g->air < AIR_LOW && (t % 40) < 24)     /* short of breath: little bubbles */
            spr_at(s, SPR_PUFF + ((t % 40) > 12), gx + (g->face > 0 ? 12 : 0), gy - 2 - (t % 40) / 3, -1, 2, 0);
        if (g->alive && g->act == ACT_DRILL) {
            int f = (g->act_t / 2) & 1;
            if (g->drill_dir == 2) spr_at(s, SPR_BIT_DOWN + f, gx + 4, gy + 15, -1, 1, 0);
            else if (g->drill_dir == 1) spr_at(s, SPR_BIT_SIDE + f, gx + 16, gy + 8, -1, 1, 0);
            else spr_at(s, SPR_BIT_SIDE + f, gx - 8, gy + 8, -1, 1, RS_SPR_HFLIP);
        }
        spr_at(s, gopher_sprite(g, t), gx - 4, gy - 8, player_pal(p), 1, flip);
    }
    /* the air bubbles (blocks of their own, drawn as sprites over the back wall) */
    int r0 = fdiv(s->cy, CELL) - 1, r1 = fdiv(s->cy + s->h, CELL) + 1, fpx = well_fall_px(w);
    int jig = jiggle(t);
    for (int r = r0; r <= r1; r++)
        for (int c = 0; c < COLS; c++) {
            const gcell *x = well_cell(w, c, r);
            if (!x || x->kind != K_AIR) continue;
            int ax = bx + c * CELL + (x->st == C_SHAKE ? jig : 0);
            int ay = by + r * CELL + (x->st == C_FALL ? fpx - CELL : 0) + (x->st == C_STILL ? hu_bob(t + c * 9, 64, 1) : 0);
            spr_at(s, SPR_AIR + ((t / 20 + c) & 1), ax, ay, -1, 1, 0);
        }
}

/* ---- the UI --------------------------------------------------------------------------------------------------------------- */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    rs_spr(cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), gm_spr[SPR_ICON].tile, 16, 16,
           p ? hu_player_pal(p) : gm_spr[SPR_ICON].pal, 2, 0);
}

static void standing(const world *w, hu_standing *s)
{
    int d[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) d[p] = w->w[p].go.depth;
    hu_rank(s, w->players, d, d);
}

static int is_ascii(const char *s)
{
    for (; *s; s++)
        if ((unsigned char)*s >= 128) return 0;
    return 1;
}

static uint32_t hash_str(uint32_t h, const char *s, int x, int y)
{
    h = (h ^ (uint32_t)(x * 131 + y)) * 16777619u;
    while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

typedef struct ptext { int16_t x, y, big; char s[100]; } ptext;
#define PTEXTS 32

static void add_text(ptext *t, int *n, int x, int y, int big, const char *s)
{
    if (*n >= PTEXTS) return;
    t[*n].x = (int16_t)x;
    t[*n].y = (int16_t)y;
    t[*n].big = (int16_t)big;
    snprintf(t[*n].s, sizeof t[*n].s, "%s", s);
    (*n)++;
}

/* centred on the stage columns [x0, x0 + w) */
static void add_centred(ptext *t, int *n, int x0, int w, int y, int big, const char *s)
{
    add_text(t, n, fdiv(x0 + w / 2, 8) - hu_width(s, big) / 2, y, big, s);
}

static const char *stratum_name(int s)
{
    switch (s % STRATA) {
    case 0: return TR(STRATUM_0);
    case 1: return TR(STRATUM_1);
    case 2: return TR(STRATUM_2);
    case 3: return TR(STRATUM_3);
    case 4: return TR(STRATUM_4);
    default: return TR(STRATUM_5);
    }
}

/* the air gauge on BG1: the bubble, n tiles of bar, the end */
static void air_gauge(int tx, int ty, int n, const gopher *g, int t)
{
    int mask = rs_bg_map_w(RS_BG1) - 1, low = g->air < AIR_LOW;
    int fill = (int)((int64_t)g->air * (n * 8) / AIR_FULL);
    if (g->air > 0 && fill == 0) fill = 1;
    int red = low && (!g->alive || (t / 15) % 2 == 0);
    rs_bg_put(RS_BG1, tx & mask, ty, RS_MAP(BAR_TILE, 0, 1, 0, 0));
    for (int i = 0; i < n; i++) {
        int f = fill - i * 8;
        f = f < 0 ? 0 : f > 8 ? 8 : f;
        rs_bg_put(RS_BG1, (tx + 1 + i) & mask, ty, RS_MAP(BAR_TILE + 1 + red * 9 + f, 0, 1, 0, 0));
    }
    rs_bg_put(RS_BG1, (tx + 1 + n) & mask, ty, RS_MAP(BAR_TILE + 19, 0, 1, 0, 0));
}

static int digits(int n)
{
    int d = 1;
    while (n >= 10) { n /= 10; d++; }
    return d;
}

/* the play texts (drawn again only when they change) */
static void play_texts(const world *W, int state, const vrect *v, int nv, int multi)
{
    ptext t[PTEXTS];
    int n = 0;
    char s[96];
    if (!multi) {
        const gopher *g = &W->w[0].go;
        add_centred(t, &n, 0, 88, 3, 0, TR(DEPTH));
        add_text(t, &n, fdiv(44 + digits(g->depth) * 6 + 2, 8), 6, 0, TR(UNIT_M));
        add_centred(t, &n, 232, 88, 3, 0, TR(AIR));
        if (W->t < HINT_FRAMES && state == ST_PLAY) add_centred(t, &n, 0, RS_SCREEN_W, 27, 0, TR(HINT));
    } else {
        for (int p = 0; p < W->players && p < nv; p++) {
            const gopher *g = &W->w[p].go;
            int row = fdiv(v[p].y + v[p].h - 16, 8);
            rs_strf(s, sizeof s, HTR(PLAYER_TAG), p + 1);
            add_text(t, &n, fdiv(v[p].x, 8) + 1, fdiv(v[p].y, 8) + 1, 0, s);
            add_text(t, &n, fdiv(v[p].x + 6 + digits(g->depth) * 12 + 2, 8), row + 1, 0, TR(UNIT_M));
        }
        if (nv == 4 && W->players == 3) {                       /* the race, in the 4th quarter */
            hu_standing rs;
            standing(W, &rs);
            for (int i = 0; i < rs.n; i++) {
                char tag[16];
                int p = rs.order[i];
                rs_strf(tag, sizeof tag, HTR(PLAYER_TAG), p + 1);
                char m[24];
                rs_strf(m, sizeof m, HTR(METRES_N), W->w[p].go.depth);
                snprintf(s, sizeof s, "%s  %s  %s", hu_str(HS_RANK_1 + rs.rank[i]), tag, m);
                add_centred(t, &n, v[3].x, v[3].w, fdiv(v[3].y, 8) + 4 + i * 3, 0, s);
            }
        }
    }
    for (int p = 0; p < W->players && p < nv; p++) {
        const well *w = &W->w[p];
        const gopher *g = &w->go;
        int mid = fdiv(v[p].y + v[p].h / 2, 8);
        if (!g->alive && (multi || state == ST_DEAD)) {
            add_centred(t, &n, v[p].x, v[p].w, mid - 3, 0, g->out == OUT_CRUSH ? TR(CRUSHED) : TR(NO_AIR));
        } else if (w->freeze > 0) {
            rs_strf(s, sizeof s, HTR(METRES_N), g->stratum * STRATUM_M);
            int big = hu_width(s, 1) * 8 <= v[p].w - 8;
            add_centred(t, &n, v[p].x, v[p].w, mid - (multi ? 4 : 6), big, s);
            add_centred(t, &n, v[p].x, v[p].w, mid - (multi ? 1 : 3), 0, stratum_name(g->stratum));
        }
    }
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) h = hash_str(h, t[i].s, t[i].x, t[i].y * 3 + t[i].big);
    if (h == shown_text) return;
    shown_text = h;
    /* widescreen: a view's left part may be at negative UI columns, which the kit's ASCII path does not wrap: those
     * texts are written here, a character at a time at the wrapped column, after the kit has loaded their glyphs */
    for (int i = 0; i < n; i++)
        if (t[i].x < 0 && !t[i].big && is_ascii(t[i].s)) hu_text(0, 29, t[i].s);
    hu_clear();
    for (int i = 0; i < n; i++) {
        if (t[i].big) {
            hu_big(t[i].x < 0 ? 0 : t[i].x, t[i].y, t[i].s, HU_BIG_FREE);
        } else if (t[i].x < 0 && is_ascii(t[i].s)) {
            int mask = rs_bg_map_w(RS_BG1) - 1;
            for (int k = 0; t[i].s[k]; k++) {
                char one[2] = {t[i].s[k], 0};
                rs_text((t[i].x + k) & mask, t[i].y, one);
            }
        } else {
            hu_text(t[i].x, t[i].y, t[i].s);
        }
    }
}

/* the BG screens of the title and the panels */
static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    if (state != shown_key || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best) {
        hu_clear();
        shown_text = 0;
        if (state == ST_TITLE) hu_logo("GOPHER DIG", title_ramps, 2, 2, 0);
        if (state == ST_OVER) {
            if (w->players == 1) {
                const gopher *g = &w->w[0].go;
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, g->depth, 0);
                hu_gameover_note(g->out == OUT_CRUSH ? TR(CRUSHED) : TR(NO_AIR));
            } else {
                hu_results_panel(rs, NULL);
            }
        }
        shown_key = state;
        shown_best = best;
    }
    if (state == ST_TITLE) hu_title_draw(st_t, best);
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
}

static void view_set(rs_viewport *q, const vrect *v, int layers, int p, int cy, int t, int sx, int sy)
{
    memset(q, 0, sizeof *q);
    q->x = (int16_t)v->x;
    q->y = (int16_t)v->y;
    q->w = (int16_t)v->w;
    q->h = (int16_t)v->h;
    q->layers = (uint8_t)layers;
    q->objs = 1;
    int m1 = rs_bg_map_w(RS_BG1) * 8 - 1;
    q->sx[RS_BG1] = (int16_t)(v->x & m1);
    q->sy[RS_BG1] = (int16_t)v->y;
    if (p < 0) return;
    int mw = map_mw * 8 - 1, x = well_tile_x(p) * 8 - (view_well_x(v) - v->x) - sx;
    q->sx[RS_BG4] = (int16_t)(x & mw);
    q->sy[RS_BG4] = (int16_t)((cy - sy) & 255);
    q->sx[RS_BG3] = (int16_t)((x - jiggle(t)) & mw);
    q->sy[RS_BG3] = q->sy[RS_BG4];
    q->sx[RS_BG2] = q->sx[RS_BG4];
    q->sy[RS_BG2] = 0;   /* set by the caller (the fall clock) */
}

static void cam_follow(int p, const well *w, const vrect *v)
{
    int target = w->go.y - view_gopher_y(v);
    if (!cam_on[p]) {
        cam[p] = TITLE_CAM;
        cam_on[p] = 1;
    }
    int d = target - cam[p];
    cam[p] += d / CAM_FOLLOW + (d > 0) - (d < 0);
}

void draw_frame(const world *W, int state, int st_t, int best, int new_best, int paused)
{
    static const int medals[4] = MEDAL_DEPTHS;
    int t = (int)rs_frame_count(), sx = hu_shake_x(), sy = hu_shake_y();
    int multi = (state == ST_PLAY || state == ST_DEAD) && W->players > 1;
    vrect v[MAX_PLAYERS];
    int nv = view_layout(multi ? W->players : 1, v);
    int nreg = multi ? (W->players >= 3 ? 4 : W->players) : 1;
    if (nreg != map_nreg || drawn_width != rs_screen_width()) maps_setup(nreg);
    hu_standing rs;
    standing(W, &rs);
    parts_step(W);

    /* the cameras */
    if (state == ST_PLAY || state == ST_DEAD || state == ST_OVER) {
        for (int p = 0; p < W->players; p++) {
            vrect fv = v[multi && p < nv ? p : 0], one[MAX_PLAYERS];
            if (state == ST_OVER) {
                view_layout(1, one);
                fv = one[0];
            }
            cam_follow(p, &W->w[p], &fv);
        }
    }

    /* the UI layer */
    if (state == ST_PLAY || state == ST_DEAD) {
        if (shown_key != ST_PLAY) {
            shown_key = ST_PLAY;
            shown_text = 0;
        }
        if (paused != text_paused) {
            text_paused = paused;
            if (!paused) shown_text = 0;
        }
        if (!paused) play_texts(W, state, v, nv, multi);
    } else {
        screen_text(W, state, st_t, best, new_best, &rs);
    }
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);
    rs_oam_clear();

    if (!multi) {
        /* one full view: player 1's well (the title: its grass) */
        int cy = state == ST_TITLE ? TITLE_CAM : draw_cam_y(0);
        const well *w = &W->w[0];
        write_view(w, 0, cy - sy, RS_SCREEN_H);
        VL[0].cam_y = cy - sy;
        VL[0].y0 = 0;
        VL[0].theme = -1;
        int x = well_tile_x(0) * 8 - (view_well_x(&v[0]) - rs_screen_left()) - rs_screen_left() - sx;
        int mw = map_mw * 8 - 1, jig = jiggle(t);
        rs_bg_scroll(RS_BG4, x & mw, (cy - sy) & 255);
        rs_bg_scroll(RS_BG3, (x - jig) & mw, (cy - sy) & 255);
        rs_bg_scroll(RS_BG2, x & mw, (cy - sy + CELL - well_fall_px(w)) & 255);
        sview s = {rs_screen_left(), 0, rs_screen_width(), RS_SCREEN_H, view_well_x(&v[0]) - rs_screen_left(), cy, SPR_BUDGET, 0};
        if (state == ST_PLAY || state == ST_DEAD) {
            const gopher *g = &w->go;
            hu_number(g->depth, 44, 34, 3);
            air_gauge(30, 5, 7, g, t);
        } else if (state == ST_OVER) {
            if (W->players == 1) {
                hu_gameover_sprites(1, w->go.depth, 0, best, hu_medal_of(w->go.depth, medals), st_t, slide);
            } else {
                hu_results_sprites(&rs, st_t, slide);
                for (int i = 0; i < rs.n; i++) {
                    int cx, cy2;
                    hu_results_icon_pos(&rs, i, &cx, &cy2);
                    rs_spr(cx - 8, cy2 - 8 + slide, gm_spr[SPR_ICON].tile, 16, 16,
                           rs.order[i] ? hu_player_pal(rs.order[i]) : gm_spr[SPR_ICON].pal, 3, 0);
                }
            }
        } else {
            hu_title_sprites(st_t, slot_icon, NULL);
            rs_spr(RS_SCREEN_W / 2 - 12, 92 + hu_bob(st_t, 64, READY_BOB), gm_spr[SPR_IDLE + (st_t / 40) % 2].tile, 24, 24,
                   gm_spr[SPR_IDLE].pal, 2, 0);
        }
        s.budget = SPR_BUDGET - rs_oam_next();
        draw_well_sprites(&s, W, 0, t, sx, sy, state != ST_TITLE);
        if (views_on) {
            rs_viewports(0, NULL, 0);
            views_on = 0;
        }
        return;
    }

    /* split screen: a viewport per player (3 players: the 4th quarter shows the race), the pause band over them */
    int budget = SPR_BUDGET / (nv == 2 ? 2 : 4);
    for (int i = 0; i < nv; i++) {
        rs_viewport *q = &VP[i];
        int p = i < W->players ? i : -1;
        if (p < 0) {
            view_set(q, &v[i], 1 << RS_BG1, -1, 0, t, 0, 0);
            VL[i].cam_y = 0;
            VL[i].y0 = (int16_t)v[i].y;
            VL[i].theme = 0;
            continue;
        }
        const well *w = &W->w[p];
        int cy = cam[p];
        write_view(w, p, cy - sy, v[i].h);
        view_set(q, &v[i], 0x0f, p, cy, t, sx, sy);
        q->sy[RS_BG2] = (int16_t)((cy - sy + CELL - well_fall_px(w)) & 255);
        VL[i].cam_y = cy - sy;
        VL[i].y0 = (int16_t)v[i].y;
        VL[i].theme = -1;
        hu_view_oam_begin(q);
        const gopher *g = &w->go;
        hu_number(g->depth, 6 + digits(g->depth) * 6, v[i].h - 18, 3);
        air_gauge(fdiv(v[i].x + v[i].w - 4, 8) - 8, fdiv(v[i].y + v[i].h - 12, 8), 6, g, t);
        sview s = {0, 0, v[i].w, v[i].h, view_well_x(&v[i]) - v[i].x, cy, budget - (rs_oam_next() - q->oam_first), 1};
        draw_well_sprites(&s, W, p, t, sx, sy, 1);
        hu_view_oam_end(q);
    }
    int n = nv;
    if (paused) {
        vrect band = {rs_screen_left(), 96, rs_screen_width(), 40};
        view_set(&VP[n], &band, 1 << RS_BG1, -1, 0, t, 0, 0);
        VP[n].objs = 0;
        VL[n].cam_y = -1000;
        VL[n].y0 = 96;
        VL[n].theme = 0;
        n++;
    }
    rs_viewports(n, VP, RS_HEX(DIVIDER));
    views_on = 1;
}

/* ---- save states (main.c) ---- */
/* a state saved at another width brings maps of another size: set them up for this width, redraw the UI */
void draw_state_loaded(void)
{
    if (drawn_width == rs_screen_width() && rs_bg_map_w(RS_BG2) == map_mw) return;
    maps_setup(map_nreg ? map_nreg : 1);
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(VL); S(VP); S(views_on); S(cam); S(cam_on); S(map_nreg); S(map_mw); S(slot_row); S(ras_theme); S(shown_key);
    S(shown_best); S(text_paused); S(shown_text); S(parts); S(last_fx_t);
    RS_STATE_RASTER(raster);
}
#undef S
