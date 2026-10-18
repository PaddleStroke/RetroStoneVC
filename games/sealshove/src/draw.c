/*
 * Seal Shove: video. The house layout (docs/art-direction.md), seen from above the sea:
 *   BG1 the UI (the house kit, priority 1); BG2 the floe: a canvas of its own tiles that this file PAINTS pixel by pixel
 *   from the rules' grid (play.c floe_field: the smooth rim, the lit edge, the side below it, the submerged skirt round
 *   it, drifts of snow, the dark crack round a cracking chunk); only the tiles of a chunk that changed are painted
 *   again (the whole floe at a new round). The canvas shares its VRAM with the title's logo (the floe is hidden on the
 *   title): build_assets.py. BG3 the sea panorama (ripples that shimmer by palette cycling, icebergs, ice bits, a
 *   fishing boat); the backdrop a raster gradient of the sea.
 *   Sprites: the seals (OBJ 0, 1, 4, 5: the players' colours on the hat and scarf; the bots take the free colours), a
 *   chunk that broke off drifting away and sinking (OBJ 6, painted into 2 sprite slots of 56 x 56), splashes, puffs,
 *   ice chips, stars, fish, the touch controls (OBJ 2), the penguins watching (OBJ 7), the kit's digits, glyphs and
 *   medals (OBJ 3).
 * Widescreen (docs/spec.md "Widescreen"): the 320-px stage is centred; a wider screen shows more sea (more icebergs,
 *   the penguins, the boat), the floe and the rules are the same; the HUD follows the screen's corners, the DASH button
 *   its bottom-right corner; the UI kit centres itself.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_tall.h"     /* a phone held upright: the title and the panels in the columns shown */
#include "game_text.h"
#include <stdio.h>
#include <string.h>

/* the title logo: SEAL in the hat's red, SHOVE in ice blue (light, mid, dark) */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 186, 150), RS_RGB8(244, 104, 78), RS_RGB8(196, 56, 46)},
    {RS_RGB8(226, 246, 255), RS_RGB8(140, 204, 244), RS_RGB8(72, 140, 214)}};
#define SEA_TOP     0x4aa2dc        /* the backdrop: the sea, lighter at the top */
#define SEA_BOTTOM  0x2a6eb4
#define WARN_A      RS_RGB8(168, 198, 232)  /* a cracking chunk blinks to these */
#define WARN_B      RS_RGB8(126, 164, 212)
#define DRIFT_LIFE  150             /* frames a chunk drifts (then gone) ... */
#define DRIFT_WET   70              /* ... wet from here (sinking) ... */
#define DRIFT_FADE  110             /* ... see-through from here */
#define CHEER_FRAMES 50             /* the penguins cheer after a splash */
/* a tall screen (zoomed): the sea past the stage shows other parts of the 1024-px panorama, moved (x, y) into the
 * columns shown: above, the boat on the left and the lone penguin's iceberg in the middle (under the score); below
 * (from line 256), the three penguins' iceberg on the left (the DASH button on the right) */
#define TALL_ABOVE_X 260
#define TALL_ABOVE_Y 144
#define TALL_BELOW_X (-122)
#define TALL_BELOW_Y 54
#define HUD_TALL_Y   26             /* the seals' rows under the score and the frontend's corner buttons */

static rs_color line_col[RS_SCREEN_H];
static int shown_key = -1, shown_best = -1, text_paused;
static uint32_t shown_text;                     /* the play texts on BG1 (a hash: redrawn when they change) */
static int drawn_width;
static int canvas_gen = -1;                     /* the floe painted on the canvas (-1: the logo is there) */
static uint8_t drawn_rev[MAX_CHUNKS], drawn_st[MAX_CHUNKS];
typedef struct drift { int16_t chunk, t, x, y, dx, dy; uint8_t on, stage, pad_[2]; } drift;
static drift DR[DRIFT_SLOTS];
static int cheer_t = 99;
static int panel_low;                           /* the winner's panel under the winner (play_texts) */
static uint8_t tile_buf[64];                    /* scratch: one tile being painted (state_audit.txt) */

/* A tall screen (rs_screen_tall: a phone held upright; DESIGN.md "Tall screen"): what the stage shows this frame, for
 * the raster's lines past the stage (set by draw_frame before every picture: not saved) */
static int tall_floe, tall_sea_x, tall_sea_y;
static int ui_pin;                              /* the UI layer pinned on the screen (house_tall.h ht_pin: the title, the panels) */
static int sea_w(void);

/* the sea's colour on a line past the stage: the gradient carried on, gentler (a third of its slope) */
static rs_color sea_color_out(int y)
{
    int a[3] = {(SEA_TOP >> 16) & 255, (SEA_TOP >> 8) & 255, SEA_TOP & 255};
    int b[3] = {(SEA_BOTTOM >> 16) & 255, (SEA_BOTTOM >> 8) & 255, SEA_BOTTOM & 255};
    int k = y < 0 ? y * 256 / (RS_SCREEN_H - 1) / 3 : 256 + (y - (RS_SCREEN_H - 1)) * 256 / (RS_SCREEN_H - 1) / 3, c[3];
    for (int i = 0; i < 3; i++) {
        c[i] = (a[i] * (256 - k) + b[i] * k) / 256;
        c[i] = c[i] < 0 ? 0 : c[i] > 248 ? 248 : c[i];
    }
    int d = (y & 4) ? 1 : 0;                    /* the house's 2-line dither step every 8 lines */
    return RS_RGB8(c[0] + 4 * d, c[1] + 2 * d, c[2] + 4 * d);
}

/* The lines past the stage (a tall screen): the sea goes on, its gradient carried on, the panorama's columns that the
 * 320-px stage never shows (above: the boat and an iceberg; below: two icebergs); the UI and the floe hidden there.
 * Line 0 puts the stage's settings back (a 240-line screen: the same values, so the same pixels). */
static void raster(int line, void *user)
{
    (void)user;
    if (line < 0 || line >= RS_SCREEN_H) {
        if (line == rs_screen_top() || line == RS_SCREEN_H) {
            rs_bg_enable(RS_BG1, 0);
            rs_bg_enable(RS_BG2, 0);
        }
        int bx = line < 0 ? TALL_ABOVE_X : line >= 256 ? TALL_BELOW_X : 0;
        int by = line < 0 ? TALL_ABOVE_Y : line >= 256 ? TALL_BELOW_Y : 0;
        rs_bg_scroll(RS_BG3, (tall_sea_x + bx) & (sea_w() * 8 - 1), (tall_sea_y + by) & 255);
        rs_pal_set(0, sea_color_out(line));
        if (ui_pin) rs_bg_enable(RS_BG1, ht_pin_line(ui_pin, line));
        return;
    }
    if (line == 0) {
        rs_bg_enable(RS_BG1, 1);
        rs_bg_enable(RS_BG2, tall_floe);
        rs_bg_scroll(RS_BG3, tall_sea_x, tall_sea_y);
    }
    if (ui_pin) rs_bg_enable(RS_BG1, ht_pin_line(ui_pin, line));
    rs_pal_set(0, line_col[line]);
}

static void sea_gradient(void)
{
    int a[3] = {(SEA_TOP >> 16) & 255, (SEA_TOP >> 8) & 255, SEA_TOP & 255};
    int b[3] = {(SEA_BOTTOM >> 16) & 255, (SEA_BOTTOM >> 8) & 255, SEA_BOTTOM & 255};
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int k = y * 256 / (RS_SCREEN_H - 1), c[3];
        for (int i = 0; i < 3; i++) c[i] = (a[i] * (256 - k) + b[i] * k) / 256;
        int d = (y & 4) ? 1 : 0;                /* house rule: a 2-line dither step every 8 lines */
        line_col[y] = RS_RGB8(c[0] + 4 * d, c[1] + 2 * d, c[2] + 4 * d);
    }
}

/* Widescreen: the UI and sea maps are 64 tiles (512 px) wide, 128 on a screen wider than 512 columns. The map size is
 * saved in states: draw_state_loaded() sets the maps up again after a state of another width. */
static int map_w(void) { return rs_screen_width() > 512 ? 128 : 64; }
/* the sea's map: the whole 1024-px panorama on a tall screen too (the lines past the stage show its far columns) */
static int sea_w(void) { return map_w() > 64 || rs_screen_height() > RS_SCREEN_H ? 128 : 64; }
static int sea_scroll(void) { return sea_w() > 64 ? SEA_X0 : 96; }    /* the sea's x at stage x 0 */

static void maps_setup(void)
{
    int mw = sea_w();
    rs_bg_setup(RS_BG1, map_w(), 32, VR_BG1);
    rs_bg_setup(RS_BG2, 32, 32, VR_BG2);
    rs_bg_setup(RS_BG3, mw, 32, VR_BG3);
    for (int ty = 0; ty < CANVAS_W; ty++)
        for (int tx = 0; tx < CANVAS_W; tx++) {
            int k = gm_canvas_idx[ty * CANVAS_W + tx];
            if (k) rs_bg_put(RS_BG2, tx, ty, RS_MAP(k, PAL_ICE, 0, 0, 0));
        }
    int c0 = mw > 64 ? 0 : (SEA_X0 - 96) / 8;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < mw; x++) rs_bg_put(RS_BG3, x, y, gm_sea_map[y * 128 + c0 + x]);
    shown_key = -1;
    shown_text = 0;
    drawn_width = rs_screen_width();
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more sea on both sides */
    /* a tall screen: the stage TALL_UP lines down (in proportion on a shorter one): the HUD above the floe */
    if (tall_screen()) rs_screen_origin_v((rs_screen_height() - RS_SCREEN_H) * TALL_UP / (ZOOM_H - RS_SCREEN_H));
    hu_config hc = hu_defaults();           /* BG1 at VRAM 0, the kit at 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;             /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hc.ext_tile = EXT_TILE;                 /* the glyph pool after the floe's canvas */
    hc.ext_count = 128;
    hu_init(&hc);
    rs_tiles_load(VR_BG3, gm_sea_tiles, gm_sea_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_ICE), gm_ice_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_SEA), gm_sea_pals, 32);
    maps_setup();
    rs_bg_enable(RS_BG1, 1);
    rs_bg_enable(RS_BG2, 0);
    rs_bg_enable(RS_BG3, 1);
    rs_bg_window(RS_BG2, RS_WIN1_OUT);      /* the canvas map repeats every 256 px: only its own columns show */
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* P1, P2, the props (the kit loaded OBJ 3) */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 64);     /* P3, P4, the ice, the penguins */
    sea_gradient();
    rs_raster(raster, NULL);
}

void draw_new_run(void)
{
    memset(DR, 0, sizeof DR);
    shown_text = 0;
}

/* ---- the floe, painted --------------------------------------------------------------------------------------------- */
static uint32_t hash2(int x, int y, uint32_t k)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + k * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* smooth value noise, 0..255, on a 12-px lattice (the snow drifts) */
static int vnoise(int px, int py, uint32_t k)
{
    int gx = px / 12, gy = py / 12, fx = (px % 12) * 256 / 12, fy = (py % 12) * 256 / 12;
    int a = hash2(gx, gy, k) & 255, b = hash2(gx + 1, gy, k) & 255;
    int c = hash2(gx, gy + 1, k) & 255, d = hash2(gx + 1, gy + 1, k) & 255;
    int top = a * (256 - fx) + b * fx, bot = c * (256 - fx) + d * fx;
    return (top * (256 - fy) + bot * fy) >> 16;
}

static int ice(const world *w, int px, int py, int dx16, int dy16)
{
    return floe_field(w, px * 16 + 8 + dx16, py * 16 + 8 + dy16, 0) >= 128;
}

/* the colour index of the floe at stage pixel (px, py) (0: the sea) */
static int floe_px(const world *w, int px, int py)
{
    int f = floe_field(w, px * 16 + 8, py * 16 + 8, 0);
    if (f < 128) {
        if (ice(w, px, py, 0, -24)) return 3;                   /* the floe's side, under its rim */
        if (ice(w, px, py, 0, -48)) return 2;
        return f >= 36 ? 1 : 0;                                 /* the submerged skirt */
    }
    int c = chunk_at(w, px, py), warn = c >= 0 && w->ch[c].state == CH_WARN;
    if (c >= 0) {
        static const int8_t nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int k = 0; k < 4; k++) {
            int o = chunk_at(w, px + nb[k][0], py + nb[k][1]);
            if (o >= 0 && o != c && w->ch[o].state != CH_GONE && (warn || w->ch[o].state == CH_WARN)) return 8;
        }
    }
    if (!ice(w, px, py, -24, -24) || !ice(w, px, py, 0, -24)) return warn ? 9 : 4;   /* the lit rim, top-left */
    if (!ice(w, px, py, 24, 24) || !ice(w, px, py, 0, 24)) return warn ? 10 : 7;     /* the shade, bottom-right */
    uint32_t k = w->seed + (uint32_t)w->round * 977u, h = hash2(px, py, k) & 1023;
    if (!warn && h < 3) return 11;                                                  /* a twinkle */
    if (!warn && h < 6) return 13;
    int n = vnoise(px, py, k) + (vnoise(px * 2 + 7, py * 2 + 3, k + 1) >> 2);
    return warn ? (n > 190 ? 10 : 9) : (n > 196 ? 6 : 5);
}

static void paint_tile(const world *w, int tx, int ty)
{
    int k = gm_canvas_idx[ty * CANVAS_W + tx];
    if (!k) return;
    int x0 = GRID_X0 + tx * 8, y0 = GRID_Y0 + ty * 8;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) tile_buf[y * 8 + x] = (uint8_t)floe_px(w, x0 + x, y0 + y);
    rs_tiles_load8(VR_BG2 + k, tile_buf, 1);
}

static void paint_box(const world *w, int x0, int y0, int x1, int y1)
{
    int tx0 = (x0 - GRID_X0) / 8, ty0 = (y0 - GRID_Y0) / 8, tx1 = (x1 - GRID_X0 + 7) / 8, ty1 = (y1 - GRID_Y0 + 7) / 8;
    if (tx0 < 0) tx0 = 0;
    if (ty0 < 0) ty0 = 0;
    if (tx1 > CANVAS_W) tx1 = CANVAS_W;
    if (ty1 > CANVAS_W) ty1 = CANVAS_W;
    for (int ty = ty0; ty < ty1; ty++)
        for (int tx = tx0; tx < tx1; tx++) paint_tile(w, tx, ty);
}

/* a chunk alone, for its drifting sprite: the field of its own cells */
static int chunk_field(const world *w, int c, int xq, int yq)
{
    int u = xq - (GRID_X0 * 16 + CELL * 8), v = yq - (GRID_Y0 * 16 + CELL * 8);
    int i = u >> 6, j = v >> 6, fu = (u & 63) * 4, fv = (v & 63) * 4, s[4];
    for (int k = 0; k < 4; k++) {
        int ii = i + (k & 1), jj = j + (k >> 1);
        s[k] = (unsigned)ii < GRID && (unsigned)jj < GRID && w->cell[jj * GRID + ii] == c + 1;
    }
    int top = s[0] * (256 - fu) + s[1] * fu, bot = s[2] * (256 - fu) + s[3] * fu;
    return (top * (256 - fv) + bot * fv) >> 8;
}

static void paint_drift(const world *w, int slot)
{
    const drift *d = &DR[slot];
    int base = VR_OBJ + DRIFT_TILE + slot * (DRIFT_SIZE / 8) * (DRIFT_SIZE / 8), n = DRIFT_SIZE / 8;
    const chunk *ch = &w->ch[d->chunk];
    int ox = ch->cx - DRIFT_SIZE / 2, oy = ch->cy - DRIFT_SIZE / 2;
    for (int ty = 0; ty < n; ty++)
        for (int tx = 0; tx < n; tx++) {
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    int px = ox + tx * 8 + x, py = oy + ty * 8 + y, xq = px * 16 + 8, yq = py * 16 + 8, v = 0, c = d->chunk;
                    int f = chunk_field(w, c, xq, yq);
                    if (f >= 128) {                     /* the same look as on the floe; wet once it sinks */
                        int tl = chunk_field(w, c, xq - 24, yq - 24) < 128 || chunk_field(w, c, xq, yq - 24) < 128;
                        int br = chunk_field(w, c, xq + 24, yq + 24) < 128 || chunk_field(w, c, xq, yq + 24) < 128;
                        uint32_t k = w->seed + (uint32_t)w->round * 977u;
                        int nz = vnoise(px, py, k) + (vnoise(px * 2 + 7, py * 2 + 3, k + 1) >> 2);
                        v = d->stage ? (tl ? 6 : 14) : tl ? 4 : br ? 7 : nz > 196 ? 6 : 5;
                    } else if (chunk_field(w, c, xq, yq - 24) >= 128) {
                        v = d->stage ? 15 : 3;
                    } else if (chunk_field(w, c, xq, yq - 48) >= 128) {
                        v = d->stage ? 15 : 2;
                    } else if (f >= 36 && !d->stage) {
                        v = 1;
                    }
                    if (v && d->stage == 2 && ((px + py) & 1)) v = 0;      /* fading: the house checker */
                    tile_buf[y * 8 + x] = (uint8_t)v;
                }
            rs_tiles_load8(base + ty * n + tx, tile_buf, 1);
        }
}

static void floe_update(const world *w, int show)
{
    if (!show) return;
    if (canvas_gen != w->floe_gen) {
        memset(tile_buf, 0, sizeof tile_buf);
        rs_tiles_load8(VR_BG2, tile_buf, 1);    /* the canvas' blank tile (the logo may have used it) */
        for (int ty = 0; ty < CANVAS_W; ty++)
            for (int tx = 0; tx < CANVAS_W; tx++) paint_tile(w, tx, ty);
        canvas_gen = w->floe_gen;
        for (int c = 0; c < MAX_CHUNKS; c++) {
            drawn_rev[c] = w->ch[c].rev;
            drawn_st[c] = w->ch[c].state;
        }
        memset(DR, 0, sizeof DR);
        return;
    }
    for (int c = 0; c < w->nchunks; c++) {
        const chunk *ch = &w->ch[c];
        if (ch->rev == drawn_rev[c]) continue;
        paint_box(w, ch->x0 - 6, ch->y0 - 6, ch->x1 + 6, ch->y1 + 8);
        if (ch->state == CH_GONE && drawn_st[c] != CH_GONE) {
            int slot = -1;                      /* a free slot, else the oldest drift */
            for (int i = 0; i < DRIFT_SLOTS && slot < 0; i++)
                if (!DR[i].on) slot = i;
            if (slot < 0) {
                slot = 0;
                for (int i = 1; i < DRIFT_SLOTS; i++)
                    if (DR[i].t > DR[slot].t) slot = i;
            }
            drift *d = &DR[slot];
            memset(d, 0, sizeof *d);
            d->on = 1;
            d->chunk = (int16_t)c;
            int dx = ch->cx - FLOE_CX, dy = ch->cy - FLOE_CY, l = isqrt64((int64_t)dx * dx + (int64_t)dy * dy);
            d->dx = (int16_t)(l ? dx * 256 / l : 0);
            d->dy = (int16_t)(l ? dy * 256 / l : 256);
            paint_drift(w, slot);
        }
        drawn_rev[c] = ch->rev;
        drawn_st[c] = ch->state;
    }
    for (int i = 0; i < DRIFT_SLOTS; i++) {
        drift *d = &DR[i];
        if (!d->on) continue;
        if (++d->t >= DRIFT_LIFE) { d->on = 0; continue; }
        int stage = d->t >= DRIFT_FADE ? 2 : d->t >= DRIFT_WET ? 1 : 0;
        if (stage != d->stage) {
            d->stage = (uint8_t)stage;
            paint_drift(w, i);
        }
    }
}

/* ---- palette animation ---------------------------------------------------------------------------------------------- */
static void palettes(const world *w, int t)
{
    /* a cracking chunk blinks, faster as it is about to break */
    int frac = 0;
    for (int c = 0; c < w->nchunks; c++)
        if (w->ch[c].state == CH_WARN && w->ch[c].warn > 0) {
            int f = w->ch[c].t * 256 / w->ch[c].warn;
            if (f > frac) frac = f;
        }
    int half = 14 - frac * 10 / 256, on = (t / (half < 3 ? 3 : half)) & 1;
    rs_pal_set(RS_PAL_BG(PAL_ICE) + 9, on ? WARN_A : gm_ice_pal[5]);
    rs_pal_set(RS_PAL_BG(PAL_ICE) + 10, on ? WARN_B : gm_ice_pal[7]);
    rs_pal_set(RS_PAL_BG(PAL_ICE) + 11, hu_lerp_color(gm_ice_pal[5], RS_RGB8(255, 255, 255), hu_bob(t, 80, 128) + 128));
    for (int i = 0; i < SEA_CYCLE_N; i++)
        rs_pal_set(gm_sea_cycle[i][0], hu_lerp_color(gm_sea_cycle[i][1], RS_RGB8(236, 248, 255),
                                                     hu_bob(t + i * 37, 150, 80) + 80));
}

/* ---- sprites ----------------------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal, int flags)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= rs_screen_top() - (int)d->h || y >= rs_screen_bottom()) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

static const int16_t seal_spr[3][3] = {{SPR_SEAL_SIDE0, SPR_SEAL_SIDE1, SPR_SEAL_SIDE2},
                                       {SPR_SEAL_FRONT0, SPR_SEAL_FRONT1, SPR_SEAL_FRONT2},
                                       {SPR_SEAL_BACK0, SPR_SEAL_BACK1, SPR_SEAL_BACK2}};

/* a seal at its place (x, y: the middle of its body), in the 3/4 view: facing down the front, up the back, else the
 * side (mirrored for the left) */
static void draw_seal_body(const seal *s, int x, int y, int prio, int pal)
{
    /* face 0..7 (E, SE, S, SW, W, NW, N, NE) -> side, front, back; the left ones mirrored */
    static const int8_t which[8] = {0, 0, 1, 0, 0, 0, 2, 0}, flip[8] = {0, 0, 0, 1, 1, 1, 0, 0};
    int sp = (s->vx < 0 ? -s->vx : s->vx) + (s->vy < 0 ? -s->vy : s->vy);
    int fr = s->state == SS_DASH ? 2 : sp > FX / 3 ? (s->walk >> 9) & 1 : 0;
    spr(seal_spr[which[s->face & 7]][fr], x - 16, y - 20, prio, pal, flip[s->face & 7] ? RS_SPR_HFLIP : 0);
}

static void draw_seals(const world *w, int t, int sx, int sy, int show_tags)
{
    int order[SEALS], n = 0;
    for (int i = 0; i < SEALS; i++) order[n++] = i;
    for (int i = 1; i < n; i++) {                       /* the lowest on the screen in front */
        int k = order[i], j = i - 1;
        while (j >= 0 && w->s[order[j]].y < w->s[k].y) { order[j + 1] = order[j]; j--; }
        order[j + 1] = k;
    }
    /* the overlays first (in front): tags, powers, stars, sparkles */
    for (int k = 0; k < n; k++) {
        const seal *s = &w->s[order[k]];
        int i = order[k], x = (int)(s->x / FX) + sx, y = (int)(s->y / FX) + sy;
        if (!seal_standing(s)) continue;
        int pal = hu_player_pal(i);
        if (show_tags && s->human) spr(SPR_TAG1 + i, x - 4, y - 40 + hu_bob(t + i * 16, 48, 1), 2, pal, 0);
        if (s->stun > 0) spr(SPR_STARS0 + ((t / 6) & 1), x - 12, y - 28, 2, -1, 0);
        /* the power's badge, top right of the head (clear of the tag) */
        if (s->power == PW_HEAVY && (s->power_t > 90 || (t / 6) & 1)) spr(SPR_ICON_HEAVY, x + 3, y - 31, 2, -1, 0);
        if (s->power == PW_MEGA && (s->power_t > 90 || (t / 6) & 1)) spr(SPR_ICON_MEGA, x + 3, y - 31, 2, -1, 0);
        if (s->ready_t < 14 && s->state == SS_PLAY) hu_sparkle(x + 8 - hu_ui_x(), y - 20, (s->ready_t / 4) & 1, 2);
    }
    for (int k = 0; k < n; k++) {
        const seal *s = &w->s[order[k]];
        int i = order[k], x = (int)(s->x / FX) + sx, y = (int)(s->y / FX) + sy, pal = hu_player_pal(i);
        if (seal_standing(s)) {
            draw_seal_body(s, x, y, 2, pal);
            if (s->state == SS_DASH)                     /* snow sprayed behind the slide */
                for (int p = 1; p <= 2; p++)
                    spr(SPR_PUFF0 + (p == 2), x - (int)(s->vx * (3 + p * 3) / FX) - 4,
                        y - (int)(s->vy * (3 + p * 3) / FX) - 4 + 2, 2, -1, 0);
        } else if (s->state == SS_FALL) {
            int ft = s->t;
            if (ft < FALL_SPLASH + 2) draw_seal_body(s, x, y + ft / 3, 1, pal);     /* under the floe's rim */
            if (ft >= FALL_SPLASH && ft < FALL_SPLASH + 18) {
                spr(SPR_SPLASH0 + (ft - FALL_SPLASH) / 6, x - 12, y - 12, 2, -1, 0);
                for (int d = 0; d < 4; d++) {
                    int a = d * 64 + 32 + i * 16, r = (ft - FALL_SPLASH) * 2 / 3 + 8;
                    spr(SPR_DROP0 + (ft > FALL_SPLASH + 10), x + icos(a) * r / FX - 4,
                        y + isin(a) * r / FX - 4 - (ft - FALL_SPLASH) / 3, 2, -1, 0);
                }
            }
            if (ft >= 34) {                             /* the head pops up, it swims off */
                int dx = x - sx - FLOE_CX, dy = y - sy - FLOE_CY, l = isqrt64((int64_t)dx * dx + (int64_t)dy * dy);
                int k2 = (ft - 34) / 3;
                int hx = x + (l ? dx * k2 / l : 0), hy = y + (l ? dy * k2 / l : k2);
                spr(SPR_SEAL_HEAD0 + ((ft / 16) & 1), hx - 8, hy - 10, 1, pal, 0);
            }
        }
    }
}

static void draw_world_props(const world *w, int t, int sx, int sy)
{
    /* the fish */
    if (w->fish.on && (w->fish.t < FISH_LIFE - 60 || (t / 4) & 1)) {
        int id = (w->fish.kind == PW_HEAVY ? SPR_FISH_HEAVY0 : SPR_FISH_MEGA0) + ((w->fish.t / 9) & 1);
        spr(id, w->fish.x - 8 + sx, w->fish.y - 9 + sy - ((w->fish.t / 9) & 1), 2, -1, 0);
    }
    /* the drifting chunks, ice chips as they break */
    for (int i = 0; i < DRIFT_SLOTS; i++) {
        const drift *d = &DR[i];
        if (!d->on) continue;
        const chunk *ch = &w->ch[d->chunk];
        int k = d->t * 2 / 5 + 2, bob = hu_bob(d->t + i * 40, 90, 1);
        int x = ch->cx + d->dx * k / 256 + sx, y = ch->cy + d->dy * k / 256 + bob + sy;
        if (d->t < 20)
            for (int c = 0; c < 4; c++)
                spr(SPR_CHIP0 + ((d->t / 5 + c) & 1), ch->cx + ((c & 1) ? 8 : -12) + d->dx * d->t / 256 + sx,
                    ch->cy + ((c & 2) ? 6 : -10) - d->t / 2 + sy, 2, -1, 0);
        rs_spr(x - DRIFT_SIZE / 2, y - DRIFT_SIZE / 2, DRIFT_TILE + i * (DRIFT_SIZE / 8) * (DRIFT_SIZE / 8), DRIFT_SIZE,
               DRIFT_SIZE, OBJ_ICE, 1, 0);
    }
}

static void draw_penguins(int t)
{
    for (int i = 0; i < PENGUIN_N; i++) {
        int f = ((t / 70 + i * 3) % 5) == 0 ? 1 : 0;
        int cheering = cheer_t < CHEER_FRAMES;
        if (cheering) f = ((cheer_t + i * 3) / 6) & 1 ? 2 : 0;
        spr(SPR_PENG0 + f, gm_penguins[i][0] - 8, gm_penguins[i][1] - 15, 1, -1, (i & 1) ? RS_SPR_HFLIP : 0);
        /* a tall screen: the bergs past the stage are other parts of the panorama (raster): their penguins with them
         * (the three of the berg at x -52 below the stage, the lone one at x 392 above it) */
        if (tall_screen() && i >= 2 && i != 5) {
            int ax = i == 6 ? TALL_ABOVE_X : TALL_BELOW_X, ay = i == 6 ? TALL_ABOVE_Y : TALL_BELOW_Y - 256;
            spr(SPR_PENG0 + f, gm_penguins[i][0] - ax - 8, gm_penguins[i][1] - ay - 15, 1, -1,
                (i & 1) ? RS_SPR_HFLIP : 0);
        }
    }
}

/* the HUD: each seal's head and its round pips in the screen's top corners (the first two left, the others right; a
 * tall screen: at its top, in the columns shown, under the frontend's corner buttons) */
static void draw_hud(const world *w, int t)
{
    int y0 = tall_screen() ? rs_screen_top() + HUD_TALL_Y : 0;
    for (int i = 0; i < SEALS; i++) {
        const seal *s = &w->s[i];
        int right = i >= 2, row = y0 + 4 + (i & 1) * 18, pal = hu_player_pal(i);
        int ix = right ? vis_right() - 20 : vis_left() + 4;
        int out = s->state == SS_OUT || s->state == SS_FALL;
        spr(out ? SPR_SEAL_HEAD0 + ((t / 16) & 1) : SPR_SEAL_ICON, ix, row, 3, pal, 0);
        for (int k = 0; k < WIN_ROUNDS; k++) {
            int px = right ? ix - 10 - k * 9 : ix + 18 + k * 9;
            spr(k < s->wins ? SPR_PIP1 : SPR_PIP0, px, row + 4, 3, -1, 0);
        }
        if (w->humans > 1 && s->human) {
            int tx = right ? ix - 10 - WIN_ROUNDS * 9 - 1 : ix + 18 + WIN_ROUNDS * 9 + 1;
            spr(SPR_TAG1 + i, tx, row, 3, pal, 0);
        }
    }
}

/* the touch controls: the stick where the finger went down, the DASH button */
static void draw_touch(const world *w, const touch_view *tv)
{
    tbutton b;
    touch_button(vis_right(), &b);
    b.y += rs_screen_bottom() - RS_SCREEN_H;    /* a tall screen: in the screen's bottom-right corner (main.c) */
    if (tv->stick_on) {
        int ox = tv->sx0 - rs_screen_origin_x(), oy = tv->sy0 - rs_screen_origin_y();
        int kx = tv->sx - tv->sx0, ky = tv->sy - tv->sy0, d = isqrt64((int64_t)kx * kx + (int64_t)ky * ky);
        if (d > STICK_R) { kx = kx * STICK_R / d; ky = ky * STICK_R / d; }
        spr(SPR_KNOB, ox + kx - 8, oy + ky - 8, 3, -1, 0);
        const gm_sprite_def *q = &gm_spr[SPR_STICK];
        spr(SPR_STICK, ox - q->w, oy - q->h, 3, -1, 0);
        spr(SPR_STICK, ox, oy - q->h, 3, -1, RS_SPR_HFLIP);
        spr(SPR_STICK, ox - q->w, oy, 3, -1, RS_SPR_VFLIP);
        spr(SPR_STICK, ox, oy, 3, -1, RS_SPR_HFLIP | RS_SPR_VFLIP);
    }
    int cool = w->s[0].dash_cd > 0 || w->s[0].state == SS_DASH || !seal_standing(&w->s[0]);
    int id = tv->dash_on ? SPR_BTN_ON : cool ? SPR_BTN_CD : SPR_BTN;
    int x = b.x, y = b.y + (tv->dash_on ? 1 : 0);
    const gm_sprite_def *q = &gm_spr[id];
    spr(SPR_ICON_DASH, x - 8, y - 8, 3, -1, 0);
    spr(id, x - q->w, y - q->h, 3, -1, 0);
    spr(id, x, y - q->h, 3, -1, RS_SPR_HFLIP);
    spr(id, x - q->w, y, 3, -1, RS_SPR_VFLIP);
    spr(id, x, y, 3, -1, RS_SPR_HFLIP | RS_SPR_VFLIP);
}

/* ---- the UI ------------------------------------------------------------------------------------------------------------- */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr(SPR_SEAL_ICON, cx - 8 + hu_ui_x(), cy - 8 + hu_bob(t + p * 16, 64, 1), 2, hu_player_pal(p), 0);
}

static void standing(const world *w, hu_standing *s)
{
    int key[MAX_PLAYERS], val[MAX_PLAYERS];
    for (int p = 0; p < w->humans; p++) {
        key[p] = w->s[p].wins * 1000 + w->s[p].kos * 10 + (w->match_winner == p);
        val[p] = w->s[p].wins;
    }
    hu_rank(s, w->humans, key, val);
}

static void name_of(const world *w, int i, char *buf, size_t n)
{
    if (w->s[i].human) rs_strf(buf, n, HTR(PLAYER_TAG), i + 1);
    else snprintf(buf, n, "%s", bot_names[i]);
}

static uint32_t hash_str(uint32_t h, const char *s, int x, int y)
{
    h = (h ^ (uint32_t)(x * 131 + y)) * 16777619u;
    while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

/* the play texts: ROUND n, GO!, the round's and the match's winner (on a panel), the messages, the hint; drawn again
 * only when they change */
static void play_texts(const world *w, int st_t, const touch_view *tv)
{
    char big[48] = "", line[96] = "", top[64] = "", hint[64] = "", nm[24];
    if (w->phase == PH_INTRO) {
        rs_strf(big, sizeof big, TR(ROUND_N), w->round);
        if (w->round == 1 && w->match_no > 1 && w->humans == 1) rs_strf(top, sizeof top, TR(MATCH_N), w->match_no);
    } else if (w->phase == PH_FIGHT && w->phase_t < GO_SHOW) {
        snprintf(big, sizeof big, "%s", HTR(GO));
    } else if (w->phase == PH_END) {
        if (w->round_winner >= 0) {
            name_of(w, w->round_winner, nm, sizeof nm);
            rs_strf(line, sizeof line, TR(ROUND_WIN), nm);
        } else {
            snprintf(line, sizeof line, "%s", TR(ROUND_DRAW));
        }
    } else if (w->phase == PH_MATCH) {
        name_of(w, w->match_winner, nm, sizeof nm);
        rs_strf(line, sizeof line, TR(MATCH_WIN), nm);
    }
    if (w->msg != MSG_NONE && !top[0]) {
        const char *m = w->msg == MSG_DOUBLE ? TR(DOUBLE_KO) : w->msg == MSG_SWEEP ? TR(SWEEP) :
                        w->msg == MSG_HEAVY ? TR(BIG_BELLY) : TR(MEGA_SLIDE);
        if (w->humans > 1 || w->msg_seal != 0) {
            name_of(w, w->msg_seal, nm, sizeof nm);
            snprintf(top, sizeof top, "%s: %s", nm, m);
        } else {
            snprintf(top, sizeof top, "%s", m);
        }
    }
    if (w->humans == 1 && w->match_no == 1 && w->round == 1 && !tv->seen &&
        (w->phase == PH_INTRO || (w->phase == PH_FIGHT && w->phase_t < HINT_FRAMES)))
        snprintf(hint, sizeof hint, "%s", TR(HINT));
    (void)st_t;
    /* the winner's panel goes where it does not hide the winner: under it when it stands in the floe's top half, over
     * it otherwise (a seal is drawn from 20 px above its place to 12 below, its tag from 40 above); a margin keeps the
     * panel from hopping while it waddles */
    int who = w->phase == PH_END ? w->round_winner : w->phase == PH_MATCH ? w->match_winner : -1;
    if (who >= 0 && who < SEALS) {
        int wy = (int)(w->s[who].y / FX);
        if (!panel_low && wy < 144) panel_low = 1;
        else if (panel_low && wy > 160) panel_low = 0;
    }
    int prow = panel_low ? 21 : 10;
    uint32_t h = 2166136261u;
    h = hash_str(h, big, 1, 0);
    h = hash_str(h, line, 2, prow);
    h = hash_str(h, top, 3, 0);
    h = hash_str(h, hint, 4, 0);
    if (h == shown_text) return;
    shown_text = h;
    hu_clear();
    if (ht_narrow()) {                          /* zoomed (house_tall.h): the same texts in the columns shown, wrapped */
        if (big[0]) ht_big(13, big, HU_BIG_FREE);
        if (line[0]) {
            int pw = hu_width(line, 0) + 4, cols = ht_ncols();
            if (pw > cols) pw = cols;
            hu_panel(ht_center_w(pw), prow, pw, 4 + ht_text_rows(line, 1));
            ht_text(prow + 2, line, 1);
        }
        if (top[0]) ht_text(5, top, 0);
        if (hint[0]) { int k = 30 - ht_text_rows(hint, 0); ht_text(k < 28 ? k : 28, hint, 0); }
        return;
    }
    /* ROUND n and GO! in the middle of the ring of seals: between the top one's feet and the side ones' caps */
    if (big[0]) hu_big(hu_center(big, 1), 13, big, HU_BIG_FREE);
    if (line[0]) {
        int wdt = hu_width(line, 0), pw = wdt + 4, px = 20 - pw / 2;
        hu_panel(px, prow, pw, 5);
        hu_box_text(hu_center(line, 0), prow + 2, line);
    }
    if (top[0]) hu_text(hu_center(top, 0), 5, top);
    if (hint[0]) hu_text(hu_center(hint, 0), 28, hint);
}

/* ---- a zoomed tall screen: the title and the panels in the columns shown (house_tall.h, its standard rows) */
static ht_title tall_title(void)
{
    ht_title c = {TR(VERB), MAX_PLAYERS, 15, 0};
    return c;
}
#define TALL_SEAL_Y 252     /* the title's big seal (decor): in the sea under the title's lines */

/* the BG screens of the title and the panels (to: where a zoomed title's glyph and slots went) */
static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs, ht_title_out *to)
{
    int screen = state == ST_DEAD ? ST_PLAY : state, narrow = ht_narrow();
    char note[64], bot_title[48] = "";
    rs_strf(note, sizeof note, TRN(MATCHES_WON, w->s[0].matches), w->s[0].matches);
    if (w->humans > 1 && w->match_winner >= 0 && w->match_winner < SEALS && !w->s[w->match_winner].human)
        rs_strf(bot_title, sizeof bot_title, TR(NAME_WINS), bot_names[w->match_winner]);
    if (narrow && (screen != shown_key || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best)) {
        hu_clear();
        shown_text = 0;
        if (state == ST_TITLE) {
            ht_logo("SEAL SHOVE", title_ramps, 2, 2, 0);   /* SEAL over SHOVE when it does not fit on one line */
            canvas_gen = -1;                    /* the logo's tiles are the floe's: paint it again later */
        }
        if (state == ST_OVER && w->humans == 1) {
            ht_banner(4, HTR(GAME_OVER));
            ht_gameover_panel(new_best, note, HTR(BACK_TITLE));
        } else if (state == ST_OVER) {
            ht_results_panel(rs, bot_title[0] ? bot_title : NULL);
        }
        shown_key = screen;
        shown_best = best;
    }
    if (narrow) {
        if (state == ST_TITLE) {
            ht_title c = tall_title();
            ht_title_draw(&c, st_t, best, to);
        }
        if (state == ST_OVER)
            ht_retry_line_at(w->humans == 1 ? ht_gameover_retry_row(note) : hu_results_retry_row(rs), st_t, RETRY_LOCK,
                             HTR(BACK_TITLE));
        return;
    }
    if (screen != shown_key || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best) {
        hu_clear();
        shown_text = 0;
        if (state == ST_TITLE) {
            hu_logo("SEAL SHOVE", title_ramps, 2, 2, 0);
            canvas_gen = -1;                    /* the logo's tiles are the floe's: paint it again later */
        }
        if (state == ST_OVER) {
            if (w->humans == 1) {
                char s[64];
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->s[0].score, 0);
                rs_strf(s, sizeof s, TRN(MATCHES_WON, w->s[0].matches), w->s[0].matches);
                hu_gameover_note(s);
            } else {
                char title[48] = "";
                if (w->match_winner >= SEALS || w->match_winner < 0 || w->s[w->match_winner].human) {
                    hu_results_panel(rs, NULL);
                } else {
                    rs_strf(title, sizeof title, TR(NAME_WINS), bot_names[w->match_winner]);
                    hu_results_panel(rs, title);
                }
            }
        }
        shown_key = screen;
        shown_best = best;
    }
    if (state == ST_TITLE) hu_title_draw(st_t, best);
    if (state == ST_OVER) {
        if (w->humans == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, const touch_view *tv)
{
    static const int medals[4] = MEDAL_SCORES;
    if (drawn_width != rs_screen_width()) maps_setup();
    int t = (int)rs_frame_count(), sx = hu_shake_x(), sy = hu_shake_y();
    int playing = state == ST_PLAY || state == ST_DEAD;
    rs_screen_fill(playing || tall_screen());   /* a tall screen: one view, the play's (the UI in the columns shown) */
    int narrow = ht_narrow();
    ht_title_out to;                            /* a zoomed title: where its glyph and slots went (screen_text) */
    memset(&to, 0, sizeof to);
    hu_standing rs;
    standing(w, &rs);
    if (!paused) {
        for (int i = 0; i < SEALS; i++)
            if (w->events[i] & EV_SPLASH) cheer_t = 0;
        if (cheer_t < 99) cheer_t++;
    }

    /* the UI layer */
    if (playing) {
        if (shown_key != ST_PLAY) {
            hu_clear();
            shown_key = ST_PLAY;
            shown_text = 0;
        }
        if (paused != text_paused) {        /* PAUSED takes rows of the UI layer: the texts are drawn again after */
            text_paused = paused;
            if (!paused) shown_text = 0;
        }
        if (!paused) play_texts(w, st_t, tv);
    } else {
        screen_text(w, state, st_t, best, new_best, &rs, &to);
    }
    int show_floe = state != ST_TITLE;
    rs_bg_enable(RS_BG2, show_floe);
    floe_update(w, show_floe);
    palettes(w, t);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, 0, -slide);
    rs_bg_scroll(RS_BG2, (-GRID_X0 - sx) & 255, (-GRID_Y0 - sy) & 255);
    rs_window(0, GRID_X0 + sx, GRID_X0 + CANVAS_W * 8 + sx);
    tall_floe = show_floe;                      /* (for the raster's line 0 after the lines above the stage) */
    tall_sea_x = (sea_scroll() - sx) & (sea_w() * 8 - 1);
    tall_sea_y = -sy;
    rs_bg_scroll(RS_BG3, tall_sea_x, tall_sea_y);
    ht_pause(paused, 13);                       /* (hu_pause; zoomed: 1x when PAUSED does not fit at 2x) */
    ui_pin = !narrow ? HT_PIN_OFF : state == ST_TITLE ? HT_PIN_TITLE : state == ST_OVER ? HT_PIN_PANEL : HT_PIN_OFF;
    ht_pin(ui_pin);                             /* the title and the panels at the same place on every game's screen */

    rs_oam_clear();
    /* front to back: the UI, the touch controls, the HUD, the seals and their overlays, the props, the decor */
    if (state == ST_OVER) {
        if (w->humans == 1 && narrow) {
            ht_gameover_sprites(w->s[0].score, best, hu_medal_of(w->s[0].score, medals), st_t, slide);
        } else if (w->humans == 1) {
            hu_gameover_sprites(1, w->s[0].score, 0, best, hu_medal_of(w->s[0].score, medals), st_t, slide);
        } else {
            if (narrow) ht_results_sprites(&rs, st_t, slide);
            else hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                if (narrow) ht_results_icon_pos(&rs, i, &cx, &cy);
                else hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_SEAL_ICON, cx - 8 + hu_ui_x(), cy - 8 + slide, 3, hu_player_pal(rs.order[i]), 0);
            }
        }
    }
    if (state == ST_TITLE) {
        if (narrow) {                           /* (house_tall.h: the slots closer, the glyph by the wrapped prompt) */
            ht_title c = tall_title();
            ht_title_sprites(&c, &to, st_t, slot_icon, NULL);
        } else {
            hu_title_sprites(st_t, slot_icon, NULL);   /* the prompt's glyph, the joined players popping in */
        }
        spr(SPR_SEAL_BIG0 + ((st_t % 150) > 140), RS_SCREEN_W / 2 - 16, (narrow ? TALL_SEAL_Y : 98) + hu_bob(st_t, 64, 4),
            2, -1, 0);
        draw_penguins(t);
        return;
    }
    if (playing && tv->seen && state == ST_PLAY) draw_touch(w, tv);
    if (playing) {
        if (w->humans == 1)
            hu_number(w->s[0].score, (vis_left() + vis_right()) / 2, 6 + (tall_screen() ? rs_screen_top() + 4 : 0), 3);
        draw_hud(w, t);
    }
    draw_seals(w, t, sx, sy, w->humans > 1);
    draw_world_props(w, t, sx, sy);
    draw_penguins(t);
}

/* ---- save states (main.c) ---- */
/* a state saved at another width brings maps of another size: set them up for this width, redraw the UI */
void draw_state_loaded(void)
{
    if (rs_bg_map_w(RS_BG1) == map_w() && rs_bg_map_w(RS_BG3) == sea_w()) return;
    maps_setup();
    shown_key = -1;
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(line_col); S(shown_key); S(shown_best); S(text_paused); S(shown_text); S(canvas_gen); S(drawn_rev); S(drawn_st);
    S(DR); S(cheer_t); S(panel_low);
    RS_STATE_RASTER(raster);
}
#undef S
