/*
 * Biscuit Bandit: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1), BG2 the ground (front), BG3 the far hills (parallax 1/4),
 *   the backdrop a raster sky gradient; sprites: the heroes (OBJ 0; players 2-4 OBJ 1, 4, 5 = the palette swaps),
 *   the props (OBJ 2), the kit's digits, glyphs and medals (OBJ 3).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/biscuitbandit/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the title logo: one 3-shade ramp per word (light, mid, dark), house_style.py ACCENTS */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 244, 150), RS_RGB8(252, 214, 64), RS_RGB8(226, 164, 28)},
    {RS_RGB8(255, 208, 226), RS_RGB8(246, 150, 190), RS_RGB8(214, 92, 146)}};
/* the sky: top and horizon colours of the raster gradient */
#define SKY_TOP     0xe0cdb1
#define SKY_HORIZON 0xd0b8a1

static rs_color line_col[RS_SCREEN_H];
static int shown_state = -1, shown_best = -1;

/* A tall screen (rs_screen_zoom: a phone held upright; DESIGN.md "Tall screen", game.h ZOOM_H): the lines past the
 * stage's 240. Above: the cafe's wall carried on up, shaded under the ceiling (the backdrop; the UI and the floor
 * layers hidden there, their maps would wrap). Below: the floor's bricks repeated (BG2's last two rows, 16 px),
 * darker with depth. Line 0 puts the layers, BG2's scroll and the floor's palette back. The room (BG3) is laid out
 * again for the height (tall_room): the belt's rail raised with the belt, the windows tall up to under it. */
#define FLOOR_REPEAT_Y 224
static int tall(void) { return rs_screen_height() > RS_SCREEN_H; }
static int belt_rise(void) { return tall() ? BELT_RISE : 0; }
/* the room picture's rows (art/tiles/hills.png): the belt's rail, the windows' top frame, a band of their plain glass */
#define RAIL_Y      36
#define RAIL_H      16
#define WIN_TOP_Y   60
#define GLASS_Y     68
#define WIN_GAP     12          /* lines of wall between the raised rail and the tall windows' top */
/* A tall screen's room, line by line (raster, every line): the rail's band RAIL_Y.. drawn BELT_RISE lines higher (the
 * stage's own rail hidden: glass there), then the windows' top frame, their glass repeated down to the stage's window
 * frame (a transom bar), only in the windows' columns (window 1 hides the shelf's columns in between). */
static void tall_room(int line)
{
    int r = BELT_RISE, sx = -hu_shake_x(), sy = -hu_shake_y();
    int rail0 = RAIL_Y - r, w0 = rail0 + RAIL_H + WIN_GAP;
    if (line == rs_screen_top() || line == rail0 + RAIL_H) rs_bg_enable(RS_BG3, 0);
    if (line == rail0) {
        rs_bg_enable(RS_BG3, 1);
        rs_bg_scroll(RS_BG3, sx, r + sy);
    }
    if (line >= w0 && line < WIN_TOP_Y) {
        if (line == w0) {
            rs_bg_enable(RS_BG3, 1);
            rs_window(0, 82, 238);              /* the shelf's columns, between the two windows */
            rs_bg_window(RS_BG3, RS_WIN1);
        }
        int y = line < w0 + 4 ? WIN_TOP_Y + line - w0 : GLASS_Y + (line - w0 - 4) % 16;
        rs_bg_enable(RS_BG3, 1);
        rs_bg_scroll(RS_BG3, sx, y - line + sy);
    }
    if (line == WIN_TOP_Y) {
        rs_bg_window(RS_BG3, 0);
        rs_bg_scroll(RS_BG3, sx, sy);
    }
}
/* A tall screen's furnishings (BG4, drawn at start-up by decor_init, its tiles shared): the cafe's wall above the
 * belt and between the tall windows, in front of the backdrop and behind the room. A wooden cornice under the ceiling (the HUD above it),
 * three pendant lamps hanging from it, a long shelf with cookie jars, plates and cups above the belt; between the
 * windows a chalk menu board hung on a nail (doodles, no words) and a round wall clock. BG4 shows from the screen's
 * top to WIN_TOP_Y (raster), its map line 0 at stage line -DEC_OFF. */
#define VR_BG4    (VR_BG3 + 384)        /* past the room's tiles, even the widescreen ones (373) */
#define PAL_DECOR 7
#define DEC_OFF   320
static const rs_color decor_pal[16] = {
    0, RS_HEX(0x4a3434), RS_HEX(0x7a5236), RS_HEX(0x9c6a44), RS_HEX(0xbf8a5a), RS_HEX(0x2f4a44), RS_HEX(0x3b5953),
    RS_HEX(0xe8eee6), RS_HEX(0x9fb5ac), RS_HEX(0x3e6f73), RS_HEX(0x5f9696), RS_HEX(0x8fbfba), RS_HEX(0xfff2b8),
    RS_HEX(0xc89a6a), RS_HEX(0x5a4848), RS_HEX(0xf4ecdc)};
enum { D_OUT = 1, D_WOOD_D, D_WOOD, D_WOOD_L, D_BOARD, D_BOARD_L, D_CHALK, D_CHALK_D, D_TEAL_D, D_TEAL, D_TEAL_L,
       D_BULB, D_COOKIE, D_CORD, D_CREAM };
static int in_disc(int x, int y, int cx, int cy, int r) { return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r; }
/* a 1-px line from (ax, ay) to (bx, by) */
static int on_seg(int x, int y, int ax, int ay, int bx, int by)
{
    int lo = ax < bx ? ax : bx, hi = ax < bx ? bx : ax, l2 = ay < by ? ay : by, h2 = ay < by ? by : ay;
    if (x < lo || x > hi || y < l2 || y > h2) return 0;
    int dx = bx - ax, dy = by - ay, n = (x - ax) * dy - (y - ay) * dx, len = dx * dx + dy * dy;
    return n * n * 4 <= len;
}
static int jar(int x, int y, int x0, int y1, int w, int h)     /* a glass jar of cookies standing on line y1 */
{
    int y0 = y1 - h, x1 = x0 + w;
    if (y >= y0 - 6 && y < y0 - 2 && x >= x0 + 2 && x < x1 - 2) return y == y0 - 6 ? D_WOOD_L : D_WOOD;   /* the lid */
    if (y >= y0 - 9 && y < y0 - 6 && x >= x0 + w / 2 - 2 && x < x0 + w / 2 + 2) return D_WOOD_D;          /* its knob */
    if (y < y0 - 2 || y > y1 || x < x0 || x >= x1) return 0;
    if (x == x0 || x == x1 - 1 || y == y1 || y == y0 - 2) return D_TEAL_D;
    if (x == x0 + 2 && y < y1 - 3) return D_CREAM;                                      /* the glint */
    for (int k = 0; k < 4; k++) {                                                        /* the cookies inside */
        int cy = y1 - 4 - k * 4, cx = x0 + w / 2 + ((k & 1) ? 2 : -2);
        if (cy > y0 + 2 && (x - cx) * (x - cx) * 4 + (y - cy) * (y - cy) * 16 <= w * w / 2)
            return (x * 3 + y * 5 + k) % 7 == 0 ? D_WOOD_D : D_COOKIE;
    }
    return D_TEAL_L;
}
/* a chalk line's wobble: 0, -1, 0, 1 lines over 8 columns */
static int chalk_wave(int i)
{
    static const int8_t w[8] = {0, 0, -1, -1, 0, 0, 1, 1};
    return w[i & 7];
}
static int decor_pix(int x, int y, int top)
{
    /* the cornice where the ceiling (its shade, the HUD over it) meets the wall */
    int cy0 = top + 42;
    if (y >= cy0 && y < cy0 + 6) return y < cy0 + 2 ? D_WOOD_L : y < cy0 + 5 ? D_WOOD : D_WOOD_D;
    if (y == cy0 + 6) return D_OUT;
    /* the pendant lamps: a cord, a teal dome, a warm bulb under it */
    static const int lamps[3] = {64, 160, 256};
    int ly = top + 62;
    for (int i = 0; i < 3; i++) {
        int dx = x - lamps[i], dy = y - ly;
        if (dx == 0 && y > cy0 + 6 && dy < 0) return D_CORD;
        if (dy >= 0 && dy < 12) {
            int hw = 3 + dy * 10 / 11;
            if (dx < -hw || dx > hw) continue;
            if (dy == 0 || dx == -hw || dx == hw || dy == 11) return D_TEAL_D;
            return dx < -hw + 3 ? D_TEAL_L : D_TEAL;
        }
        if (dy >= 12 && dy < 16 && dx >= -3 + (dy - 12) && dx <= 3 - (dy - 12)) return D_BULB;
    }
    /* the shelf above the belt, on two brackets, with jars, plates and cups */
    int sy = -200;
    if (x >= 36 && x < 284 && y >= sy && y < sy + 4) return y == sy ? D_WOOD_L : y < sy + 3 ? D_WOOD : D_WOOD_D;
    for (int b = 0; b < 2; b++) {
        int bx = b ? 262 : 52, k = y - (sy + 4);
        if (k >= 0 && k < 10 && x >= bx && x < bx + 10 - k) return k == 9 || x == bx + 9 - k ? D_WOOD_D : D_WOOD;
    }
    int v;
    if ((v = jar(x, y, 92, sy - 1, 22, 24))) return v;
    if ((v = jar(x, y, 226, sy - 1, 18, 18))) return v;
    for (int k = 0; k < 4; k++) {                                                        /* a stack of plates */
        int py = sy - 2 - k * 3, rx = 11 - (k == 3);
        if (y >= py - 1 && y <= py && x >= 138 - rx && x <= 138 + rx) return y == py ? D_CHALK_D : D_CREAM;
    }
    for (int c = 0; c < 2; c++) {                                                        /* two cups */
        int cx = c ? 196 : 176, cy = sy - 1;
        if (y >= cy - 8 && y <= cy && x >= cx - 5 && x <= cx + 5)
            return x == cx - 5 || x == cx + 5 || y == cy || y == cy - 8 ? D_TEAL_D : x < cx - 2 ? D_TEAL_L : D_TEAL;
        if (x >= cx + 6 && x <= cx + 8 && y >= cy - 6 && y <= cy - 2 && !(x == cx + 7 && y > cy - 6 && y < cy - 2))
            return D_TEAL_D;
    }
    /* the chalk menu board between the windows, hung on a nail */
    int bx0 = 106, bx1 = 214, by0 = -116, by1 = -38;
    if (in_disc(x, y, 160, -128, 1)) return D_OUT;
    if (y < by0 && y > -128 && (on_seg(x, y, 160, -127, bx0 + 8, by0) || on_seg(x, y, 160, -127, bx1 - 8, by0)))
        return D_CORD;
    if (x >= bx0 && x < bx1 && y >= by0 && y < by1) {
        if (x == bx0 || x == bx1 - 1 || y == by0 || y == by1 - 1) return D_OUT;
        if (x < bx0 + 4 || x >= bx1 - 4 || y < by0 + 4 || y >= by1 - 4)
            return x < bx0 + 2 || y < by0 + 2 ? D_WOOD_L : x >= bx1 - 2 || y >= by1 - 2 ? D_WOOD_D : D_WOOD;
        int ix = x - bx0, iy = y - by0;
        /* a wavy chalk heading */
        if (ix >= 28 && ix < 80 && iy == 10 + chalk_wave(ix)) return D_CHALK;
        /* a cookie doodle and a cup doodle, each before two lines of squiggles, dotted leaders and a boxed price */
        int kx = bx0 + 16, ky = by0 + 28;
        if (in_disc(x, y, kx, ky, 6) && !in_disc(x, y, kx, ky, 5)) return D_CHALK;
        if (in_disc(x, y, kx, ky, 4) && (x * 7 + y * 3) % 6 == 0) return D_CHALK_D;
        int cx = bx0 + 16, cy = by0 + 54;
        if ((y == cy + 4 && x >= cx - 5 && x <= cx + 4) || ((x == cx - 5 || x == cx + 4) && y >= cy - 3 && y <= cy + 4) ||
            (x == cx + 6 && y >= cy - 2 && y <= cy + 1) || (x == cx + 5 && (y == cy - 2 || y == cy + 1)) ||
            (y == cy - 3 && x >= cx - 5 && x <= cx + 4))
            return D_CHALK;
        if ((x == cx - 2 || x == cx + 1) && y >= cy - 9 && y <= cy - 5 && ((y + x) & 1)) return D_CHALK_D;   /* steam */
        for (int r = 0; r < 4; r++) {
            int ry = by0 + (r < 2 ? 24 : 50) + (r & 1) * 8, len = 26 + (r * 13) % 17;
            if (y == ry + chalk_wave(ix * 2) && ix >= 30 && ix < 30 + len) return D_CHALK;
            if (y == ry + 1 && ix >= 32 + len && ix < 84 && (ix & 3) == 0) return D_CHALK_D;
            if (y >= ry - 2 && y <= ry + 2 && ix >= 87 && ix < 97 && (ix == 87 || ix == 96 || y == ry - 2 || y == ry + 2))
                return D_CHALK_D;                                                        /* a price, boxed */
            if (y == ry && ix >= 89 && ix < 95 && (ix & 1)) return D_CHALK;
        }
        return (ix * 13 + iy * 7) % 41 == 0 ? D_BOARD_L : D_BOARD;
    }
    /* the wall clock under the board */
    if (in_disc(x, y, 160, 12, 12)) {
        if (!in_disc(x, y, 160, 12, 10)) return in_disc(x - 1, y - 1, 160, 12, 10) ? D_WOOD_D : D_WOOD;
        if (on_seg(x, y, 160, 12, 155, 7) || on_seg(x, y, 160, 12, 167, 8)) return D_OUT;
        if ((x == 160 && (y == 4 || y == 20)) || (y == 12 && (x == 152 || x == 168))) return D_OUT;
        return D_CREAM;
    }
    return 0;
}
static void decor_init(void)
{
    enum { MAXT = VR_OBJ - VR_BG4 };
    uint8_t(*uniq)[64] = malloc((size_t)MAXT * 64);
    if (!uniq) return;
    int n = 1, top = rs_screen_top();
    memset(uniq[0], 0, 64);
    rs_bg_setup(RS_BG4, 64, 64, VR_BG4);
    for (int r = 0; r < 64; r++)
        for (int c = 0; c < 40; c++) {
            uint8_t t[64];
            int id = 0;
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) t[y * 8 + x] = (uint8_t)decor_pix(c * 8 + x, r * 8 + y - DEC_OFF, top);
            while (id < n && memcmp(uniq[id], t, 64)) id++;
            if (id == n) {
                if (n < MAXT) memcpy(uniq[n++], t, 64);
                else id = 0;
            }
            rs_bg_put(RS_BG4, c, r, RS_MAP(id, PAL_DECOR, 0, 0, 0));
        }
    rs_tiles_load8(VR_BG4, uniq[0], n);
    free(uniq);
    rs_pal_load(RS_PAL_BG(PAL_DECOR), decor_pal, 16);
}
static void raster_out(int line)
{
    int top = rs_screen_top(), bot = rs_screen_bottom();
    if (line == top || line == RS_SCREEN_H) {
        rs_bg_enable(RS_BG1, 0);
        rs_bg_enable(RS_BG3, 0);
        rs_bg_enable(RS_BG2, line == RS_SCREEN_H);
        rs_bg_enable(RS_BG4, line == top);
    }
    if (line < 0) tall_room(line);
    if (line < 0) {
        int k = line - top < 48 ? (48 - (line - top)) * 32 / 48 : 0;   /* the ceiling's shade: the top 48 lines */
        rs_color c = RS_HEX(SKY_TOP);                       /* (no dither step: smooth, as the stage on a tall screen) */
        rs_pal_set(0, hu_scale_color(c, 256 - k));
    } else {
        int d = line - RS_SCREEN_H, n = bot - RS_SCREEN_H, k = n > 0 ? d * 120 / n : 0;
        rs_bg_scroll(RS_BG2, -hu_shake_x(), FLOOR_REPEAT_Y + d % 16 - line);
        if ((d & 3) == 0)
            for (int i = 1; i < 16; i++) rs_pal_set(RS_PAL_BG(PAL_GROUND) + i, hu_scale_color(gm_ground_pal[i], 220 - k));
    }
}

static void raster(int line, void *user)
{
    (void)user;
    if (line < 0 || line >= RS_SCREEN_H) {
        raster_out(line);
        return;
    }
    if (line == 0 && rs_screen_height() > RS_SCREEN_H) {  /* back from raster_out: the stage's own settings */
        for (int l = 0; l < 3; l++) rs_bg_enable(l, 1);
        rs_bg_scroll(RS_BG2, -hu_shake_x(), -hu_shake_y());
        rs_pal_load(RS_PAL_BG(PAL_GROUND), gm_ground_pal, 16);
    }
    if (tall() && line <= WIN_TOP_Y) tall_room(line);
    if (tall() && line == WIN_TOP_Y) rs_bg_enable(RS_BG4, 0);   /* the furnishings end above the room's windows */
    rs_pal_set(0, line_col[line]);
}

static void sky_gradient(void)
{
    int a[3] = {(SKY_TOP >> 16) & 255, (SKY_TOP >> 8) & 255, SKY_TOP & 255};
    int b[3] = {(SKY_HORIZON >> 16) & 255, (SKY_HORIZON >> 8) & 255, SKY_HORIZON & 255};
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int k = y < GROUND_Y ? y * 256 / GROUND_Y : 256, c[3];
        for (int i = 0; i < 3; i++) c[i] = (a[i] * (256 - k) + b[i] * k) / 256;
        /* house rule: a 2-line dither step every 8 lines keeps the RGB555 bands soft */
        line_col[y] = RS_RGB8(c[0] + ((y & 4) ? 4 : 0), c[1] + ((y & 4) ? 2 : 0), c[2] + ((y & 4) ? 4 : 0));
        /* a tall screen: no stripes; above the windows' transom (seen between the tall windows), the wall's own colour,
         * as above the stage and in the room's picture (no band) */
        if (tall()) line_col[y] = y < WIN_TOP_Y ? RS_HEX(SKY_TOP) : RS_RGB8(c[0], c[1], c[2]);
    }
}

/* Widescreen (DESIGN.md "Widescreen"): a screen wider than 320 centres the 320-px stage (rs_screen_origin) and shows
 * more of the cafe on both sides: the room's 1024-px picture on BG3 (a 128-tile map, its own tiles and palette: the
 * classic room in the middle, the bread rack and a pot plant on the right, the door, a lamp and a pot plant on the left), the
 * floor on a 128-tile BG2 (the same 512-px pattern twice), the UI layer 128 tiles wide (the corner tags of 3-4
 * players), the belt to the screen's left edge and the oven at its right end, which the cookies come out of. */
static int laid_w;                      /* the width the maps were laid out for (a state of another width: relayout) */
static int wide(void) { return rs_screen_width() > RS_SCREEN_W; }

static void maps_setup(void)
{
    int wd = wide(), mw = wd ? 128 : 64;
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG2, mw, 32, VR_BG2);
    rs_bg_setup(RS_BG3, mw, 32, VR_BG3);
    rs_tiles_load(VR_BG2, gm_bg2_tiles, gm_bg2_tile_count);
    rs_tiles_load(VR_BG3, wd ? gm_hills_wide_tiles : gm_bg3_tiles, wd ? gm_hills_wide_tile_count : gm_bg3_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_GROUND), gm_ground_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_HILLS), wd ? gm_hills_wide_pal : gm_hills_pal, 16);
    for (int y = 0; y < GROUND_H; y++)
        for (int x = 0; x < mw; x++) rs_bg_put(RS_BG2, x, GROUND_Y / 8 + y, gm_ground_map[y * 64 + (x & 63)]);
    for (int y = 0; y < HILLS_H; y++)
        for (int x = 0; x < mw; x++)
            rs_bg_put(RS_BG3, x, HILLS_Y / 8 + y, wd ? gm_hills_wide_map[y * 128 + x] : gm_hills_map[y * 64 + x]);
    laid_w = rs_screen_width();
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, the cafe wider */
    /* a tall screen (portrait, game.h ZOOM_H): the stage low, TALL_BELOW lines of floor under it */
    int below = rs_screen_height() - RS_SCREEN_H < TALL_BELOW ? 0 : TALL_BELOW;
    rs_screen_origin_v(rs_screen_height() - RS_SCREEN_H - below);
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0 on palette 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;         /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hc.ext_count = 256;                 /* the glyph pool: tiles 640..895 (BG2 starts at VRAM 1024): the title's three
                                         * hint lines in Japanese, Chinese or Korean need more than the default 128 */
    hu_init(&hc);
    maps_setup();
    for (int l = 0; l < 3; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* hero, hero 2, props (the kit loaded OBJ 3) */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 48);     /* heroes 3 and 4, the oven (widescreen) */
    if (tall()) decor_init();           /* the wall's furnishings (BG4) */
    sky_gradient();
    rs_raster(raster, NULL);
}

/* After a state was loaded (rs_game.state_loaded): the width is not in a state, and one saved at another width brings
 * that width's maps. Set up this width's (the UI is drawn again on the next frame, PAUSED too). Same width: nothing. */
void draw_relayout(void)
{
    if (laid_w == rs_screen_width()) return;
    maps_setup();
    shown_state = -1;                   /* screen_text clears the UI layer and draws it again */
    hu_pause(0, 13);                    /* ... and PAUSED, if it shows (draw_frame: hu_pause(paused)) */
}

static void spr(int id, int x, int y, int prio, int pal)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= rs_screen_top() - (int)d->h ||
        y >= rs_screen_bottom())
        return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, 0);
}

static int hero_sprite(const hero *h, int t){
 if(h->state>=HS_HIT)return SPR_HERO_HIT;
 return SPR_HERO_IDLE + (h->vx ? (t/8)%2 : (t/40)%2);
}

/* player p's sprite palette: the house rule (P1 OBJ 0, P2 OBJ 1, P3 OBJ 4, P4 OBJ 5: house_ui.h hu_player_pal) */
static int player_pal(int p) { return p ? hu_player_pal(p) : -1; }

static void draw_heroes(const world *w,int st_t,int sx,int sy){
 for(int p=w->players-1;p>=0;p--){
  const hero*h=&w->h[p];if(h->state==HS_OFF)continue;
  int yy=GROUND_Y-32;
  spr(hero_sprite(h,w->t),h->x-16+sx,yy+sy,2,player_pal(p));
  int slope=h->tilt<-2?-1:h->tilt>2?1:0;
  spr(SPR_TRAY+slope+1,h->x-24+sx,TRAY_Y-2+sy,2,-1);
  for(int j=0;j<h->load && j<PILE_MAX;j++){
   if(h->pile[j].type<0)continue;
   int bx=h->pile[j].pos>>16,by=TRAY_Y-5-(j/3)*6+bx*slope/10;
   spr(SPR_BISCUIT+h->pile[j].type,h->x+bx-8+sx,by-4+sy,2,-1);
  }
  if(h->spill_t)spr(SPR_BISCUIT+h->spill_type,(h->spill_x>>16)-8,(h->spill_y>>16)-4,2,-1);
  if(h->catch_t)spr(SPR_PUFF+(h->catch_t/3)%3,h->x-4,TRAY_Y-16,2,-1);
 }
 (void)st_t;
}

/* the title's player slots: each joined player's icon, in its colours (hu_title_sprites calls it) */
/* the results of 2-4 players: ranked by score */
static void standing(const world *w, hu_standing *s)
{
    int score[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) score[p] = w->h[p].score;
    hu_rank(s, w->players, score, score);
}

/* the title's rows: BEST (i = -1) and the 3 hints (i = 0..2): packed (11, 12-14) when the hints are plain ASCII (no
 * accent, no tall glyph), else one free row above each line for its accents (BEST 8, hints 10, 12, 14) */
static int hint_row(int i)
{
    const char *hint[3] = {TR(HINT_MOVE), TR(HINT_DROP), TR(HINT_POINTS)};
    int packed = 1;
    for (int k = 0; k < 3; k++)
        for (const char *c = hint[k]; *c; c++)
            if ((unsigned char)*c >= 128) packed = 0;
    if (packed) return 12 + i;
    return i < 0 ? 8 : 10 + 2 * i;
}

/* the BG screens; PLAY and DEAD are the same screen */
static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    int screen = state == ST_DEAD ? ST_PLAY : state;
    if (screen != shown_state || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best) {
        hu_clear();
        if (state == ST_TITLE){
            const char *hint[3] = {TR(HINT_MOVE), TR(HINT_DROP), TR(HINT_POINTS)};
            hu_logo("BISCUIT BANDIT", title_ramps, 2, 2, 0);
            for (int i = 0; i < 3; i++) hu_text(hu_center(hint[i], 0), hint_row(i), hint[i]);
        }
        if (screen == ST_PLAY && w->players >= 3 && !tall())
            hu_score_tags(w->players, 0);       /* P1..P4 by the corner chips (a tall screen: icons, draw_frame) */
        if (state == ST_OVER) {
            if (w->players == 1) {
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->h[0].score, 0);
            } else {
                hu_results_panel(rs, NULL);                                     /* P2 WINS! and the ranking */
            }
        }
        shown_state = screen;
        shown_best = best;
    }
    /* the title, every frame: the prompt (blinking), the player slots, BEST, the join line, the credits */
    /* the prompt goes up to row 20 (the kit's row 21 is over the hero): row 20 is cleared before the kit draws, so
     * that the kit's accents (in the row above its prompt) never land on our prompt */
    if(state==ST_TITLE){
        char s[128];
        hu_clear_rows(20,1);hu_title_draw(st_t,best);hu_clear_rows(18,1);hu_clear_rows(21,1);hu_clear_rows(24,1);
        for(int p=0;p<w->players;p++){char tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);hu_text((w->h[p].x-8)/8,18,tag);}
        rs_strf(s,sizeof s,HTR(PRESS_TO),HTR(PLAY));   /* the kit's prompt text: its A glyph (hu_title_glyph) fits ours */
        hu_prompt(20,s,st_t);
        if(best>0){rs_strf(s,sizeof s,HTR(BEST_N),best);hu_text(hu_center(s,0),hint_row(-1),s);}
    }
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    static const int medals[4] = MEDAL_SCORES;
    int sx = hu_shake_x(), sy = hu_shake_y();
    hu_standing rs;
    standing(w, &rs);
    rs_bg_scroll(RS_BG2, -sx, -sy);
    rs_bg_scroll(RS_BG3, -sx, -sy);
    if (tall()) rs_bg_scroll(RS_BG4, -sx, DEC_OFF - sy);
    screen_text(w, state, st_t, best, new_best, &rs);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;     /* the panel slides up, ease-out */
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);

    /* a tall screen (game.h ZOOM_H, DESIGN.md "Tall screen"): the play fills the display's height, the sides cropped
     * (every landing point stays in the columns shown); the title and the panels show every column */
    rs_screen_fill(state == ST_PLAY || state == ST_DEAD);
    int rise = belt_rise();

    rs_oam_clear();
    /* front to back: the UI, the heroes, the props */
    if ((state == ST_PLAY || state == ST_DEAD) && tall()) {
        /* the scores at the screen's top, in the columns shown: 1 player centred; 2-4 a row, each after its icon */
        int vx0 = rs_screen_visible_x0() - rs_screen_origin_x(), vw = rs_screen_visible_x1() - rs_screen_visible_x0();
        for (int p = 0; p < w->players; p++) {
            int cx = vx0 + (2 * p + 1) * vw / (2 * w->players), y = rs_screen_top() + 12;
            if (w->players > 1) spr(SPR_HERO_ICON, cx - 26, y, 3, player_pal(p));
            hu_number_at(w->h[p].score, w->players > 1 ? cx - 6 : cx, y, w->players > 1 ? -1 : 0, 3);
        }
    } else if (state == ST_PLAY || state == ST_DEAD) {
        int score[MAX_PLAYERS];
        for (int p = 0; p < w->players; p++) score[p] = w->h[p].score;
        hu_score_chips(w->players, score, 0);   /* 1: centred; 2: x 80 / 240; 3-4: the corners */
    }
    if (state == ST_OVER) {
        if (w->players == 1) {
            hu_gameover_sprites(1, w->h[0].score, 0, best, hu_medal_of(w->h[0].score, medals), st_t, slide);
        } else {
            hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_HERO_ICON, cx - 8, cy - 8 + slide, 3, player_pal(rs.order[i]));
            }
        }
    }
    if(state==ST_TITLE){int x,y;int glyph=hu_title_glyph(&x,&y);if(glyph>=0)hu_glyph(glyph,x,20*8-4,(st_t/30)%2,3);}
    /* widescreen: the oven the cookies come out of, in front of them (a lower OAM index), just right of the stage;
     * the shake moves it left only, so that its mouth always hides a cookie at x 320 (the cookies do not shake) */
    if (wide()) spr(SPR_OVEN, 320 + (sx < 0 ? sx : 0), sy, 2, -1);
    /* Leftward slats move only with world time, freezing on the title and pause (widescreen: from the screen's left
     * edge, on the same 32-px rhythm) */
    for(int x=-(rs_screen_margin()+31)/32*32;x<320;x+=32)spr(SPR_BELT+(w->t/3)%4,x+sx,44-rise+sy,1,-1);
    draw_heroes(w,st_t,sx,sy);
    for(int i=0;i<ITEM_MAX;i++){
      const drop*d=&w->items[i];if(!d->active)continue;
      int ix=d->x,iy=d->y>>16;

      if(d->age<=72){
        ix=328+(d->x-328)*d->age/72;iy=40+(d->age>56?(d->age-56)*12/16:0)-rise;
      }else if(rise&&iy<TRAY_Y){
        /* a tall screen: the belt is higher, the fall drawn longer over the same frames (the rules unchanged) */
        iy-=iy<=52?rise:rise*(TRAY_Y-iy)/(TRAY_Y-52);
      }
      spr(d->bad?SPR_CUP:SPR_BISCUIT+d->id%3,ix-8,iy-4,2,-1);
    }

}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(line_col); S(shown_state); S(shown_best); S(laid_w);
    RS_STATE_RASTER(raster);
}
#undef S





