/*
 * Pogo Mamie: video. The play layer (BG2) is streamed from the world tile by tile as the camera moves in both
 * directions; the mid-ground roofs (BG3) and the district backdrops (BG4, loaded into two VRAM slots as the
 * districts go by) scroll slower; the sky is one colour per line (the raster callback, HDMA-like), tinted per
 * district and darker at night like every palette. Sprites: the players, the cat, pigeons, props, power-ups and
 * the cosmetic effects. Nothing here changes the game.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 */
#include "pm.h"
#include "assets.h"
#include "house_ui.h"
#include <string.h>

/* ---- the look of each district (and the night): sky top, horizon, the foreground tint, the far ink ---------------- */
typedef struct look { int sky_top[3], sky_hor[3], tint[3], ink[3]; } look;
static const look looks[5] = {
    {{78, 96, 176}, {252, 178, 120}, {255, 238, 222}, {92, 60, 112}},     /* Montmartre: sunset */
    {{96, 124, 196}, {255, 212, 144}, {255, 246, 228}, {80, 72, 124}},    /* the Seine: golden hour */
    {{70, 64, 150}, {244, 152, 152}, {246, 224, 232}, {72, 50, 104}},     /* Haussmann: dusk */
    {{44, 44, 122}, {214, 126, 174}, {222, 208, 238}, {50, 40, 92}},      /* the Eiffel Tower: twilight */
    {{8, 12, 40}, {44, 48, 104}, {126, 136, 196}, {14, 16, 40}},          /* night */
};

static int cur_look[12], tgt_look[12], shown_look[12] = {-1};
static rs_color line_col[RS_SCREEN_H];
static uint32_t bg2_key[64][128];               /* the world cell each BG2 map cell holds (+1; 0 = none; tall: 64 rows) */
static int32_t far_col[128];                    /* the BG4 world column each BG4 map column holds */
static int16_t bg3_row[64];                     /* the mid-ground row each BG3 map row holds */
static int far_slot[3];                         /* the district (wide: the panorama) whose tiles are in each BG4 slot */
static int redraw_ui;                           /* transient: a state of another width was loaded (state_audit.txt) */
static int drawn_width;                         /* the screen width the maps were set up for (saved) */

/* Widescreen (DESIGN.md "Widescreen"): the world is revealed wider, a third of the extra width on the left (behind
 * the players: the stage origin, WIDE_LEFT_SHARE) and two thirds on the right; the stage (the camera's 320 columns)
 * is that of 320. At 320 the scenery maps are 64 tiles wide; on a wider screen 128 (the play layer streams up to 71
 * columns; the mid-ground is a 1024-px panorama, its second half other roofs; the backdrops' panoramas follow each
 * other, three of them may show at once: three BG4 slots). The UI layer is 128 tiles wide beyond 512 columns. */
static int view_left(void) { return (rs_screen_width() - RS_SCREEN_W) * WIDE_LEFT_SHARE_NUM / WIDE_LEFT_SHARE_DEN; }
/* what the left view shows must still be in the world: world.c cleanup() keeps the buildings up to 96 px behind the
 * camera (and the things up to 128): the widest left view, its partial tile and the crash's shake fit */
_Static_assert((RS_SCREEN_W_MAX - RS_SCREEN_W) * WIDE_LEFT_SHARE_NUM / WIDE_LEFT_SHARE_DEN + 8 + 4 <= 96,
               "the left view must not reach past the buildings world.c keeps behind the camera");
static int floor8(int v) { return v >= 0 ? v / 8 : -((7 - v) / 8); }     /* v / 8 rounded down (the left view: v < 0) */
static int wide(void) { return rs_screen_width() > RS_SCREEN_W; }
static int scenery_w(void) { return wide() ? 128 : 64; }
static int ui_w(void) { return rs_screen_width() > 512 ? 128 : 64; }
static int screen_cols(void) { return (rs_screen_width() + 7) / 8; }    /* 40 at 320 */
/* A tall screen (a phone held upright, DESIGN.md "Tall screen"): the scenery maps are 64 rows tall (the world's whole
 * height) and the screen shows the world from view_y (world y of its top line; above 0: the sky) down to the street:
 * the stage (the camera's 240 lines) sits TALL_ABOVE lines below the screen's top, lower near the street. */
static int tall(void) { return rs_screen_height() > RS_SCREEN_H; }
static int map_rows(void) { return tall() ? 64 : 32; }
static int fdiv(int v, int d) { return v >= 0 ? v / d : -((d - 1 - v) / d); }   /* v / d rounded down */
static int view_y;                              /* transient: the world y of the screen's top line (state_audit.txt) */
static int view_cam_y;                          /* transient: the camera y of this picture (the shake included) */
static int hud_lines;                           /* transient: the UI layer's lines pinned at the screen's top */
static int ui_dx;                               /* transient: the UI's shift to the middle of the columns shown */
static int mid_a3;                              /* transient: the mid-ground's virtual y at the screen's top */
static int pan_x0 = -1;                         /* a zoomed screen: the first column shown (-1: none yet; saved) */
int draw_ui_dx(void) { return ui_dx; }
int draw_hud_y(void) { return hud_lines ? rs_screen_top() : 0; }
static int32_t sky_redrawn[8];                  /* buildings whose broken skylight was redrawn */
static int sky_redrawn_n;
static int scroll_x, scroll_y;
static int night_blink;
static rs_rng fx_rng;
static int camx_i, camy_i;                      /* the camera of the last frame drawn (the shake included) */

int draw_scroll_x(void) { return scroll_x; }
void draw_camera(int *x, int *y) { *x = camx_i; *y = camy_i; }
int draw_scroll_y(void) { return scroll_y; }

/* ---- palettes ---------------------------------------------------------------------------------------------------- */
static rs_color rgb(int r, int g, int b) { return RS_RGB8(clampi(r, 0, 255), clampi(g, 0, 255), clampi(b, 0, 255)); }
static void split(rs_color c, int *r, int *g, int *b)
{
    *r = (c & 31) * 255 / 31;
    *g = ((c >> 5) & 31) * 255 / 31;
    *b = ((c >> 10) & 31) * 255 / 31;
}

/* the mid-ground's colour i (0..4): the horizon's colour mixed with the district's ink, by tone; shade 256 = as is
 * (a tall screen: the city below the mid-ground's roofs darker with depth, raster_tall) */
static rs_color mid_color(int i, int shade)
{
    static const int mid_mix[5] = {40, 55, 70, 85, 0};
    const int *tint = &cur_look[6], *ink = &cur_look[9], *hor = &cur_look[3];
    int k = mid_mix[i];
    int r = hor[0] + (ink[0] - hor[0]) * k / 100, g = hor[1] + (ink[1] - hor[1]) * k / 100,
        b = hor[2] + (ink[2] - hor[2]) * k / 100;
    if (i == C_MWIN - 1) {
        if (tint[0] < 180) r = 250, g = 214, b = 120;           /* the night: lit windows */
        else r = hor[0], g = hor[1], b = hor[2];
    }
    if (shade < 256) r = r * shade / 256, g = g * shade / 256, b = b * shade / 256;
    return rgb(r, g, b);
}

static void apply_palettes(void)
{
    const int *tint = &cur_look[6], *ink = &cur_look[9], *hor = &cur_look[3];
    int night = tint[0] < 180;                  /* the night's tint: lit windows */
    for (int p = 1; p <= 4; p++)
        for (int i = 1; i < 16; i++) {
            int r, g, b;
            split(pm_bg_pal[p][i], &r, &g, &b);
            rs_color c = rgb(r * tint[0] / 255, g * tint[1] / 255, b * tint[2] / 255);
            if (night && ((p == PAL_STONE && (i == C_STONE_GLASS_D || i == C_STONE_GLASS_L)) ||
                          (p == PAL_HOUSE && (i == C_HOUSE_GLASS_D || i == C_HOUSE_GLASS_L))))
                c = i == C_STONE_GLASS_L || i == C_HOUSE_GLASS_L ? rgb(255, 236, 160) : rgb(236, 176, 80);
            rs_pal_set(RS_PAL_BG(p) + i, c);
        }
    /* the mid-ground and the backdrop: the horizon's colour mixed with the district's ink, by tone */
    static const int far_mix[6] = {18, 30, 42, 54, -40, 30};
    for (int i = 0; i < 5; i++) rs_pal_set(RS_PAL_BG(PAL_MID) + i + 1, mid_color(i, 256));
    for (int i = 0; i < 6; i++) {
        int k = far_mix[i];
        rs_color c = k >= 0 ? rgb(hor[0] + (ink[0] - hor[0]) * k / 100, hor[1] + (ink[1] - hor[1]) * k / 100,
                                  hor[2] + (ink[2] - hor[2]) * k / 100)
                            : rgb(hor[0] + (255 - hor[0]) * -k / 100, hor[1] + (255 - hor[1]) * -k / 100,
                                  hor[2] + (255 - hor[2]) * -k / 100);
        if (i == C_FLIGHT - 1 && night && night_blink) c = rgb(255, 240, 170);
        rs_pal_set(RS_PAL_BG(PAL_FAR) + i + 1, c);
    }
}

static void sky_lines(int camy)
{
    const int *top = &cur_look[0], *hor = &cur_look[3];
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int k = clampi((y + camy / 8) * 256 / 250, 0, 256);
        k = k * k / 256;                        /* the colour gathers toward the horizon */
        int r = top[0] + (hor[0] - top[0]) * k / 256, g = top[1] + (hor[1] - top[1]) * k / 256,
            b = top[2] + (hor[2] - top[2]) * k / 256;
        int d = (y & 4) ? 4 : 0;                /* a 4-line dither step keeps the RGB555 bands soft */
        line_col[y] = rgb(r + d, g + d / 2, b + d);
    }
}

static int test_backdrop;                       /* tests: a flat magenta backdrop, so that no tile can match it */
void draw_test_backdrop(int on) { test_backdrop = on; }

/* A tall screen's lines (DESIGN.md "Tall screen"): the sky's gradient over the screen's height (from the world y of
 * its top, as the stage's from the camera's); the UI layer shows only the stage and the HUD's band pinned at the
 * screen's top; the play layer only from the world's top down (above it the map would wrap: the street) */
static void raster_tall(int line)
{
    int top = rs_screen_top(), sl = line - top;
    if (line == top) {
        rs_bg_enable(RS_BG1, hud_lines > 0);
        rs_bg_enable(RS_BG2, view_y >= 0);
    }
    if (hud_lines && line == top + hud_lines) rs_bg_enable(RS_BG1, 0);
    if (line == -view_cam_y) rs_bg_enable(RS_BG2, 1);   /* the world's top */
    if (line == 0) rs_bg_enable(RS_BG1, 1);
    if (line == RS_SCREEN_H) rs_bg_enable(RS_BG1, 0);
    /* the city below the mid-ground's roofs (its last row, repeated): darker with depth, not a flat band */
    int below = mid_a3 + sl - (MID_Y + 16 + MID_H) * 8;
    if (line == top || below >= 0) {
        int shade = below <= 0 ? 256 : 256 - (below < 192 ? below : 192) * 96 / 192;
        for (int i = 0; i < 5; i++) rs_pal_set(RS_PAL_BG(PAL_MID) + i + 1, mid_color(i, shade));
    }
    if (test_backdrop) {
        rs_pal_set(0, RS_RGB(31, 0, 31));
        return;
    }
    const int *tp = &cur_look[0], *hor = &cur_look[3];
    int k = clampi((sl + fdiv(view_y, 8)) * 256 / 250, 0, 256);
    k = k * k / 256;
    int r = tp[0] + (hor[0] - tp[0]) * k / 256, g = tp[1] + (hor[1] - tp[1]) * k / 256,
        b = tp[2] + (hor[2] - tp[2]) * k / 256, d = (sl & 4) ? 4 : 0;
    rs_pal_set(0, rgb(r + d, g + d / 2, b + d));
}

static void raster(int line, void *user)
{
    (void)user;
    if (tall()) {
        raster_tall(line);
        return;
    }
    rs_pal_set(0, test_backdrop ? RS_RGB(31, 0, 31) : line_col[line]);
}

/* The district's look (the sky, the palettes, the night) is the decor of the whole screen: picked at its centre
 * (widescreen: the centre of the wider screen, so the look changes when the new district fills half of it) */
static void look_update(const world *w, int snap)
{
    int xc = world_camx(w) + rs_screen_left() + rs_screen_width() / 2;
    int idx = night_at(xc) ? 4 : district_at(xc);
    const look *L = &looks[idx];
    const int *src = &L->sky_top[0];
    for (int i = 0; i < 12; i++) {
        tgt_look[i] = src[i];
        int d = tgt_look[i] - cur_look[i];
        cur_look[i] = snap ? tgt_look[i] : cur_look[i] + (d > 0 ? (d + 15) / 16 : d / 16);
    }
    night_blink = idx == 4 && (rs_frame_count() / 20) % 3 == 0;
    int changed = snap || memcmp(cur_look, shown_look, sizeof cur_look) || idx == 4;
    if (changed) {
        memcpy(shown_look, cur_look, sizeof cur_look);
        apply_palettes();
    }
}

/* ---- the play layer: the world, tile by tile --------------------------------------------------------------------- */
static uint16_t T(int tile, int hf) { return RS_MAP(tile, pm_bg2_pal[tile], 0, hf, 0); }

static uint16_t ground_tile(int x, int y)
{
    int ty = (y - STREET_Y) / 8;
    if (district_at(x) == 1) return ty == 0 ? T(T_WATER_TOP, (x / 8) & 1) : T(ty < 3 ? T_WATER : T_WATER2, ((x / 8 + ty) & 1));
    if (ty == 0) return T(T_WALK, 0);
    if (ty == 1) return T(T_CURB, 0);
    return T(((x / 8 * 3 + ty) % 5) ? T_ROAD : T_ROAD2, 0);
}

/* the surface in each pixel column of a tile, relative to the tile's top: the shape of a slope tile */
static uint16_t slope_tile(const bldg *b, int x, int y)
{
    int s0 = bldg_surface(b, x) - y, s7 = bldg_surface(b, x + 7) - y;
    if (s0 >= 8 && s7 >= 8) return 0;
    if (s0 <= 0 && s7 <= 0) return T(b->roof == RF_PITCH ? T_PITCH_FILL : T_MANS_FACE, 0);
    if (b->roof == RF_MANSARD) return T(T_MANS_DIAG, s0 < s7);
    if (s0 > s7) return T(s0 == 7 ? T_PITCH_A : T_PITCH_B, 0);
    return T(s7 == 7 ? T_PITCH_A : T_PITCH_B, 1);
}

static uint16_t bump_tile(const bldg *b, int k, int col, int row)
{
    int n = b->bump_w[k] / 8, last = col == n - 1, end = col == 0 || last;
    switch (b->kind) {
    case BK_QUAY: return T(row == 0 ? (end ? T_BOX_TOP_END : T_BOX_TOP) : (end ? T_BOX_BODY_END : T_BOX_BODY), last);
    case BK_BARGE: return T(row == 0 ? (end ? T_CABIN_TOP_END : T_CABIN_TOP) : (end ? T_CABIN_END : T_CABIN_WIN), last);
    default: return T(row == 0 ? T_CHIM_TOP : T_CHIM_BODY, last);
    }
}

static uint16_t facade_tile(const bldg *b, int col, int ncol, int y, int first_row_y)
{
    int edge = col == 0 || col == ncol - 1, hf = col == ncol - 1;
    int from_street = (STREET_Y - y) / 8;           /* 1 = the row just above the street */
    switch (b->kind) {
    case BK_HAUSS: {
        if (from_street <= 3) {                      /* the shops on the ground floor */
            int door = col == ncol / 2 + (b->style & 1);
            if (from_street == 3) return T(T_ST_BAND, 0);
            if (door) return T(from_street == 2 ? T_ST_DOOR_T : T_ST_DOOR_B, 0);
            if (edge) return T(T_ST_EDGE, hf);
            return T(from_street == 2 ? T_SHOP_T : T_SHOP_B, 0);
        }
        int r = (y - first_row_y) / 8, f = r / 3, q = r % 3;
        int win = !edge && (col & 1) == (b->style >> 1 & 1), balc = f == 1 || f == 4;
        if (edge) return T(T_ST_EDGE, hf);
        if (q == 0) return T(win ? T_ST_WIN_T : T_ST_WALL, 0);
        if (q == 1) return balc ? T(win ? T_ST_BALC : T_ST_BALC_WALL, 0) : T(win ? T_ST_WIN_B : T_ST_WALL, 0);
        return T(balc ? T_ST_BAND : T_ST_WALL, 0);
    }
    case BK_HOUSE: {
        if (from_street == 1) return T(T_H_BASE, 0);
        int door = col == 1 + (b->style & 1);
        if (from_street <= 3 && door) return T(from_street == 3 ? T_H_DOOR_T : T_H_DOOR_B, 0);
        if (edge) return T(T_H_EDGE, hf);
        int r = (y - first_row_y) / 8, q = r % 3;
        int win = col % 3 == 1 + (b->style >> 1 & 1) % 2;
        if (q == 0) return T(win ? T_H_WIN_T : T_H_WALL, 0);
        if (q == 1) return T(win ? T_H_WIN_B : T_H_WALL, 0);
        return T(T_H_WALL, 0);
    }
    case BK_QUAY:
        return T(from_street == 2 && col % 5 == 2 ? T_Q_RING : T_Q_WALL, (col & 1));
    case BK_BRIDGE: {
        int a = col % 4;
        if (from_street <= 3 && a) {                  /* the arches over the river */
            if (from_street == 3) return a == 2 ? T(T_BR_ARCH_C, 0) : T(T_BR_ARCH_S, a == 3);
            return 0;
        }
        return T(T_BR_WALL, 0);
    }
    default:                                          /* a barge */
        return T(from_street == 1 ? T_HULL_WATER : T_HULL, 0);
    }
}

static uint16_t building_tile(const bldg *b, int x, int y)
{
    int col = (x - (int)b->x0) / 8, ncol = (int)(b->x1 - b->x0) / 8, k = x - (int)b->x0;
    /* chimneys, boxes, the cabin */
    for (int i = 0; i < BUMP_MAX; i++)
        if (b->bump_x[i] >= 0 && k >= b->bump_x[i] && k < b->bump_x[i] + b->bump_w[i] && y >= bump_top(b, i) &&
            y < b->top)
            return bump_tile(b, i, (k - b->bump_x[i]) / 8, (y - bump_top(b, i)) / 8);
    if (y < b->top - 8) return 0;
    /* the skylight and, broken, the attic under it */
    if (b->sky1 && k >= b->sky0 && k < b->sky1 && y >= b->top && y <= b->top + PIT_DEPTH) {
        int r = (y - b->top) / 8;
        if (!b->sky_broken) { if (r == 0) return T(T_SKY_GLASS, 0); }
        else return T(r == 0 ? T_SKY_BROKEN : r < PIT_DEPTH / 8 ? T_ATTIC : T_ATTIC_FLOOR, 0);
    }
    int roof_rows = 1;
    switch (b->roof) {
    case RF_MANSARD:
        roof_rows = 2;
        if (y < b->top) return 0;
        if (y < b->top + 16) {
            if (k < MANSARD_W || k >= (int)(b->x1 - b->x0) - MANSARD_W) return slope_tile(b, x, y);
            int dormer = col % 3 == 1 && col > 2 && col < ncol - 3;
            if (y == b->top) return dormer ? T(T_MANS_DORMER_T, 0) : T(T_ZINC_TOP, 0);
            return dormer ? T(T_MANS_DORMER_B, 0) : T(T_MANS_FACE_B, 0);
        }
        break;
    case RF_PITCH: {
        int h = (int)(b->x1 - b->x0) / 4;
        roof_rows = h / 8;
        if (y < b->top) return 0;
        if (y < b->top + h) return slope_tile(b, x, y);
        break;
    }
    default:
        if (y < b->top) return 0;
        if (y == b->top) {
            switch (b->kind) {
            case BK_QUAY: return T(T_Q_TOP, 0);
            case BK_BRIDGE: return T(T_BR_TOP, 0);
            case BK_BARGE: return T(col == 0 || col == ncol - 1 ? T_DECK_END : T_DECK, col == ncol - 1);
            default: return T(col == 0 || col == ncol - 1 ? T_ZINC_TOP_END : T_ZINC_TOP, col == ncol - 1);
            }
        }
        break;
    }
    int below = b->top + roof_rows * 8;                 /* the first facade row */
    if (y == below) {
        if (b->kind == BK_HAUSS) return T(T_CORNICE, 0);
        if (b->kind == BK_HOUSE) return T(T_H_EAVE, 0);
        if (b->kind == BK_BRIDGE) return T(T_BR_BAND, 0);
    }
    return facade_tile(b, col, ncol, y, below + 8);
}

static uint16_t gap_tile(const world *w, int x, int y)
{
    for (int i = 0; i < w->no; i++) {
        const obj *o = &w->o[i];
        if (o->kind != OB_CRADLE || x < o->x || x >= o->x + o->w) continue;
        int c = (int)(x - o->x) / 8;
        if (y == o->y - 8) return T(c == 0 ? T_DAVIT_END : T_DAVIT_ARM, 0);
        if ((c == 0 || c == 3) && y >= o->y && y < o->b + 8) return T(T_ROPE, c == 3);
    }
    return 0;
}

static uint16_t world_tile(const world *w, const bldg *b, int x, int y)
{
    if (y >= STREET_Y) return ground_tile(x, y);
    if (b) return building_tile(b, x, y);
    return gap_tile(w, x, y);
}

/* Behind the start (world x < 0, only on a wide screen's left view: the camera never goes left of 0): decor, the
 * Haussmann building next to the first roof, a little taller, under a mansard. Not part of the world (nothing lands
 * there: the players never go back past the camera's left edge). */
static bldg behind_start(void)
{
    bldg b;
    memset(&b, 0, sizeof b);
    b.x0 = -256;
    b.x1 = 0;
    b.top = START_TOP - 16;
    b.kind = BK_HAUSS;
    b.roof = RF_MANSARD;
    b.style = 1;
    b.index = -1;
    for (int k = 0; k < BUMP_MAX; k++) b.bump_x[k] = -1;
    return b;
}

static void stream_bg2(const world *w, int camx, int camy)
{
    int cm = rs_bg_map_w(RS_BG2) - 1;               /* the map's columns - 1: 63, or 127 on a wide screen */
    /* a broken skylight: redraw its columns */
    for (int i = 0; i < w->nb; i++) {
        const bldg *b = &w->b[i];
        if (!b->sky_broken) continue;
        int seen = 0;
        for (int k = 0; k < sky_redrawn_n; k++) seen |= sky_redrawn[k] == b->index;
        if (seen) continue;
        sky_redrawn[sky_redrawn_n++ & 7] = b->index;
        if (sky_redrawn_n > 8) sky_redrawn_n = 8;
        for (int x = (int)b->x0 + b->sky0; x < b->x0 + b->sky1; x += 8)
            for (int r = 0; r < map_rows(); r++) bg2_key[r][(x / 8) & cm] = 0;
    }
    bldg behind = behind_start();
    int tx0 = floor8(camx + rs_screen_left()), ty0 = camy >> 3;     /* (widescreen: from the left view) */
    int ty1 = ty0 + RS_SCREEN_H / 8, rm = map_rows() - 1;
    if (tall()) {                                   /* a tall screen: its lines, from the world's top at most */
        ty0 = view_y > 0 ? view_y >> 3 : 0;
        ty1 = (view_y + rs_screen_height() - 1) >> 3;
    }
    for (int tx = tx0; tx <= tx0 + screen_cols(); tx++) {
        const bldg *b = world_bldg_at(w, tx * 8);
        if (!b && tx < 0) b = &behind;
        for (int ty = ty0; ty <= ty1 && ty < WORLD_H / 8; ty++) {
            uint32_t key = ((uint32_t)tx << 6 | (uint32_t)ty) + 1;
            uint32_t *k = &bg2_key[ty & rm][tx & cm];
            if (*k == key) continue;
            *k = key;
            rs_bg_put(RS_BG2, tx & cm, ty & rm, world_tile(w, b, tx * 8, ty * 8));
        }
    }
}

/* ---- the mid-ground: a virtual 64-row layer (sky, the roofs panorama, then its last row repeated: the city below)
 * scrolling at half the camera's speed, its rows streamed into the 32-row map as they come into view ------------------ */
#define MID_TOP_ROWS (MID_Y + 16)               /* the virtual layer's sky rows above the roofs (room for the drop) */
static int mid_drop;                            /* px the mid-ground sinks by (the Eiffel Tower district: the tower shows) */

static void stream_bg3(int s3, int lines)
{
    int rm = map_rows() - 1;                        /* (a tall screen: 64 rows, its lines from s3, < 0 the sky) */
    for (int v = fdiv(s3, 8); v <= fdiv(s3, 8) + lines / 8; v++) {
        if (bg3_row[v & rm] == v) continue;
        bg3_row[v & rm] = (int16_t)v;
        int r = v - MID_TOP_ROWS, rr = r < MID_H ? r : MID_H - 1;
        for (int x = 0; x < MID_W; x++)
            rs_bg_put(RS_BG3, x, v & rm, r < 0 ? 0 : pm_mid_map[rr * MID_W + x]);
        if (rs_bg_map_w(RS_BG3) > MID_W)           /* widescreen: the panorama's second half */
            for (int x = 0; x < MID_W; x++)
                rs_bg_put(RS_BG3, MID_W + x, v & rm, r < 0 ? 0 : pm_mid_wide_map[rr * MID_W + x]);
    }
}

/* ---- the backdrop: one panorama per district, 64 BG4 columns each ------------------------------------------------
 * At 320 two VRAM slots (district & 1) suffice: two panoramas at most show at once. A wider screen (up to 71 columns)
 * may show the end of one, the whole next one and the start of a third: three slots of FAR_SLOT_WIDE tiles, panorama
 * k in slot k % 3, its map entries moved to that slot. */
static uint16_t far_entry(uint16_t e, int d, int base)
{
    if (!e) return 0;                               /* the blank tile (each slot's first) */
    return (uint16_t)((e & ~1023) | (RS_MAP_TILE(e) - (d & 1) * FAR_SLOT + base));
}

static void stream_bg4(int camx)
{
    /* the first column on screen (the layer scrolls by camx / 8; widescreen: from the left view, which shows the panorama
     * before the first one at the start: the last district's, k = -1, which the first one follows anyway) */
    int c0 = floor8(camx / 8 + rs_screen_left()), fm = rs_bg_map_w(RS_BG4) - 1, three = fm > 63;
    for (int32_t C = c0; C <= c0 + screen_cols() + 1; C++) {
        if (far_col[C & fm] == C) continue;
        int k = C >= 0 ? (int)(C / 64) : -(int)((63 - C) / 64), d = (k % 4 + 4) % 4;
        int slot = three ? (k % 3 + 3) % 3 : d & 1, key = three ? k + 1 : d;       /* (key >= 0: -1 = none) */
        int base = three ? slot * FAR_SLOT_WIDE : slot * FAR_SLOT;
        if (far_slot[slot] != key) {
            rs_tiles_load(VR_BG4 + base, pm_far_tiles[d], pm_far_tile_count[d]);
            far_slot[slot] = key;
        }
        for (int r = 0; r < FAR_H; r++) {
            uint16_t e = pm_far_map[d][r * FAR_W + (C & 63)];
            rs_bg_put(RS_BG4, (int)(C & fm), FAR_Y + r, three ? far_entry(e, d, base) : e);
        }
        far_col[C & fm] = C;
    }
}

/* ---- set-up ---------------------------------------------------------------------------------------------------------- */
void draw_invalidate(void)
{
    memset(bg2_key, 0, sizeof bg2_key);
    for (int i = 0; i < 128; i++) far_col[i] = INT32_MIN;      /* (none: the left view shows columns < 0) */
    for (int i = 0; i < 64; i++) bg3_row[i] = INT16_MIN;      /* (none: a tall screen streams rows < 0, the sky) */
    sky_redrawn_n = 0;
}

/* the maps for this width (the play layer, the mid-ground and the backdrop are streamed into them) */
static void maps_setup(void)
{
    rs_bg_setup(RS_BG1, ui_w(), 32, VR_BG1);
    rs_bg_setup(RS_BG2, scenery_w(), map_rows(), VR_BG2);   /* (a tall screen: 64 rows, the world's height) */
    rs_bg_setup(RS_BG3, scenery_w(), map_rows(), VR_BG3);
    rs_bg_setup(RS_BG4, scenery_w(), map_rows(), VR_BG4);
}

void draw_init(void)
{
    rs_screen_origin(view_left());              /* widescreen: the left view (0 at 320) */
    for (int p = 1; p < 8; p++)
        if (p != PAL_LOGO) rs_pal_load(RS_PAL_BG(p), pm_bg_pal[p], 16);    /* 0 and 5: the house kit's */
    rs_pal_load(RS_PAL_OBJ(0), pm_obj_pals, 128);
    maps_setup();
    rs_tiles_load(VR_BG2, pm_bg2_tiles, T_COUNT);
    rs_tiles_load(VR_BG3, pm_mid_tiles, wide() ? pm_mid_wide_tile_count : pm_mid_tile_count);
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, pm_obj_tiles, pm_obj_tile_count);
    rs_raster(raster, NULL);
    rs_rng_seed(&fx_rng, 0x9090a1u);
    far_slot[0] = far_slot[1] = far_slot[2] = -1;
    drawn_width = rs_screen_width();
    draw_invalidate();
    memset(shown_look, 0xff, sizeof shown_look);
}

/* A state saved at another width brings the maps of that width (64 or 128 tiles) and the backdrop's slots of that
 * width: set them up for this one (everything streamed again, the UI redrawn on the next frame). Same width: nothing. */
void draw_state_loaded(void)
{
    rs_screen_origin(view_left());              /* (not part of a state: this width's) */
    /* (a state of another height: a tall screen's maps are 64 rows, else 32) */
    if (drawn_width == rs_screen_width() && rs_bg_map_h(RS_BG2) == map_rows()) return;
    drawn_width = rs_screen_width();
    maps_setup();
    if (wide()) rs_tiles_load(VR_BG3, pm_mid_tiles, pm_mid_wide_tile_count);
    far_slot[0] = far_slot[1] = far_slot[2] = -1;
    draw_invalidate();
    redraw_ui = 1;
}

int draw_redraw_ui(void)
{
    int r = redraw_ui;
    redraw_ui = 0;
    return r;
}

/* ---- cosmetic effects ------------------------------------------------------------------------------------------------ */
typedef struct fx_obj { int kind, t, life, frame, n; int32_t x, y, vx, vy; } fx_obj;
enum { FX_NONE, FX_DUST, FX_SHARD, FX_DEBRIS, FX_STAR, FX_POP, FX_SPLASH, FX_FEATHER, FX_STREAK };
#define FX_MAX 48
static fx_obj fx[FX_MAX];

static fx_obj *fx_new(int kind, int x, int y, int life)
{
    for (int i = 0; i < FX_MAX; i++)
        if (fx[i].kind == FX_NONE) {
            memset(&fx[i], 0, sizeof fx[i]);
            fx[i].kind = kind;
            fx[i].x = x * 256;
            fx[i].y = y * 256;
            fx[i].life = life;
            return &fx[i];
        }
    return NULL;
}

static void burst(int kind, int x, int y, int n, int spread, int up)
{
    for (int i = 0; i < n; i++) {
        fx_obj *f = fx_new(kind, x, y, 40 + rs_rng_range(&fx_rng, 20));
        if (!f) return;
        f->vx = (rs_rng_range(&fx_rng, 2 * spread + 1) - spread) * 256 / 16;
        f->vy = -(up + rs_rng_range(&fx_rng, up + 1)) * 256 / 16;
        f->frame = rs_rng_range(&fx_rng, 2);
    }
}

void fx_event(const world *w, int p, int ev, const hit *h)
{
    const mamie *m = &w->m[p];
    int x = (int)(m->x >> 16), y = (int)(m->y >> 16);
    (void)h;
    if (ev & EV_LAND) {
        fx_obj *f = fx_new(FX_DUST, x - 7, y - 4, 12);
        if (f) f->vx = -64;
        f = fx_new(FX_DUST, x + 1, y - 4, 12);
        if (f) f->vx = 64;
    }
    if (ev & EV_GLASS) burst(FX_SHARD, x, y, 8, 20, 24);
    if (ev & (EV_POT | EV_BREAK)) burst(FX_DEBRIS, x, y + 4, 6, 18, 16);
    if (ev & (EV_PIGEON | EV_KNOCK)) burst(FX_FEATHER, x, y - (ev & EV_KNOCK ? 20 : 0), 4, 16, 12);
    if (ev & (EV_STUMBLE | EV_BASKET | EV_KNOCK)) burst(FX_STAR, x, y - 26, 3, 16, 10);   /* seeing stars */
    if ((ev & EV_DEFLECT) && m->deflect) {          /* a slope bounce: streaks the way it throws her */
        for (int k = 0; k < 2; k++) {
            fx_obj *f = fx_new(FX_STREAK, x - 8 - m->deflect * 10, y - 12 - k * 9, 14);
            if (f) { f->vx = m->deflect * 160; f->vy = -96; f->frame = m->deflect < 0; }
        }
        fx_obj *f = fx_new(FX_DUST, x - 4 - m->deflect * 6, y - 4, 12);
        if (f) f->vx = -m->deflect * 96;
    }
    if (ev & EV_STUNT) {
        fx_obj *f = fx_new(FX_POP, x - 8, y - 40, 40);
        if (f) { f->n = m->stunt_pts; f->frame = m->chain; f->vy = -64; }
        burst(FX_STAR, x, y - 8, 3, 20, 16);
    }
    if (ev & EV_ITEM) burst(FX_STAR, x, y - 16, 6, 24, 20);
    if (ev & EV_DOWN) {
        if (m->down_kind) burst(FX_SPLASH, x - 8, y - 12, 3, 16, 20);
        else burst(FX_DUST, x, y - 2, 4, 20, 6);
    }
}

void fx_update(const world *w)
{
    (void)w;
    for (int i = 0; i < FX_MAX; i++) {
        fx_obj *f = &fx[i];
        if (f->kind == FX_NONE) continue;
        f->t++;
        f->x += f->vx;
        f->y += f->vy;
        if (f->kind == FX_SHARD || f->kind == FX_DEBRIS || f->kind == FX_SPLASH) f->vy += 40;
        if (f->kind == FX_FEATHER) { f->vy += 6; f->vx = f->vx * 15 / 16; }
        if (f->t >= f->life) f->kind = FX_NONE;
    }
}

/* ---- sprites ---------------------------------------------------------------------------------------------------------- */

/* a sprite past the screen's edges (a tall screen: its lines above and below the stage too) */
static int off_screen(const pm_sprite_def *d, int sx, int sy)
{
    return sx <= rs_screen_left() - (int)d->w || sx >= rs_screen_right() || sy <= rs_screen_top() - (int)d->h ||
           sy >= rs_screen_bottom();
}

static void spr_at(int id, int sx, int sy, int pal, int flags)
{
    const pm_sprite_def *d = &pm_spr[id];
    if (off_screen(d, sx, sy)) return;
    rs_spr(sx, sy, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, 2, flags);
}

static void spr_w(int id, int wx, int wy, int flags) { spr_at(id, wx - camx_i, wy - camy_i, -1, flags); }

static void spr_w_prio(int id, int wx, int wy, int flags, int prio)
{
    const pm_sprite_def *d = &pm_spr[id];
    int sx = wx - camx_i, sy = wy - camy_i;
    if (off_screen(d, sx, sy)) return;
    rs_spr(sx, sy, d->tile, d->w, d->h, d->pal, prio, flags);
}

static int mamie_frame(const mamie *m, int t)
{
    switch (m->state) {
    case MS_SLING: return m->t < 3 ? MF_SQUASH1 : MF_SQUASH2;
    case MS_REEL: return MF_HANG;
    case MS_FALL: case MS_TUMBLE: return (t / 6) & 1 ? MF_FLAIL2 : MF_FLAIL1;
    case MS_DOWN: return m->down_kind ? MF_FLOAT : MF_SIT;
    default: break;
    }
    if (m->land_t < 3) return m->bounce == BN_BIG || m->bounce == BN_SPRING ? MF_SQUASH2 : MF_SQUASH1;
    if (m->land_t < 6) return MF_SQUASH1;
    if (m->land_t < SQUASH_T) return MF_STRETCH;
    if (m->stumble_t > 0 || m->knock_t > 0) return MF_STUMBLE;
    if ((m->bounce == BN_BIG || m->bounce == BN_SPRING || m->bounce == BN_SLING) && m->vy < 0) return MF_BIG;
    return m->vy < 0 ? MF_RISE : MF_FALL;
}

/* the family: Mamie (OBJ 0), Papi (1, his beret over the headscarf), Tata (4) and Tonton (5, Papi's beret): the
 * house players' palettes (docs/art-direction.md) */
static const int player_pal[MAX_PLAYERS] = {0, 1, 4, 5};
int draw_player_pal(int p) { return player_pal[p & 3]; }

static void draw_player(const world *w, int p)
{
    const mamie *m = &w->m[p];
    if (m->state == MS_OFF) return;
    int x = (int)(m->x >> 16), feet = (int)(m->y >> 16);
    int f = mamie_frame(m, w->t), flip = m->face < 0 ? RS_SPR_HFLIP : 0;
    int sx = x - 12 - camx_i, sy = feet - 32 - camy_i;
    if (m->state == MS_DOWN && m->t > 12) {
        spr_at(SPR_CURSE + (m->t / 12) % 2, sx - 4, sy - 16, -1, 0);
    }
    if (p == 1 || p == 3) {                         /* Papi's (Tonton's) beret over the headscarf */
        int hx = pm_papi_head[f][0], hy = pm_papi_head[f][1];
        spr_at(SPR_PAPI_HEAD, flip ? sx + 24 - hx - 16 : sx + hx, sy + hy, player_pal[p], flip);
    }
    if (m->umbrella_t > 0 && m->state == MS_AIR) {
        int blink = m->umbrella_t < 60 && (m->umbrella_t / 4) % 2;
        if (!blink) spr_at(SPR_UMBRELLA + ((w->t / 16) & 1), sx, sy - 12, -1, 0);
    }
    if (p == 0 && pm_touch_armed() && m->state == MS_AIR) {   /* touch: the big bounce armed, a glint on her pogo */
        const pm_sprite_def *g = &pm_spr[SPR_STAR];
        spr_at(SPR_STAR + (w->t / 4) % 2, sx + 12 - g->w / 2, sy + 30 - g->h, -1, 0);
    }
    spr_at(SPR_MAMIE + f, sx, sy, player_pal[p], flip);
    if (sy + 8 < rs_screen_top() && (m->state == MS_AIR || m->state == MS_TUMBLE))     /* above the screen */
        spr_at(SPR_ARROW, sx + 8, rs_screen_top() + hud_lines + 2, -1, 0);           /* (tall: under the HUD) */
}

/* ---- the clothesline's rope: 8-px chords between points on its curve (obj_line_y), the points 8 px apart and
 * aligned on the load point (where Mamie stands: her pogo's tip exactly on the rope), drawn behind the play layer
 * (OBJ priority 1): the chords that reach past a pole go behind the wall ---------------------------------------- */
int draw_line_points(const obj *o, int *xs, int *ys, int max)
{
    int x0 = (int)o->x, x1 = x0 + o->w;
    int L = o->sag ? x0 + clampi(o->c, 1, o->w - 1) : x0;
    int first = L - (L - x0 + 7) / 8 * 8, n = 0;
    for (int x = first; n < max; x += 8) {
        xs[n] = clampi(x, x0, x1);
        ys[n] = obj_line_y(o, xs[n]);
        n++;
        if (x >= x1) break;
    }
    return n;
}

static void draw_rope(const obj *o, int i)
{
    int xs[24], ys[24], n = draw_line_points(o, xs, ys, 24);
    int x0 = (int)o->x;
    int first = xs[0] == x0 && n > 1 ? xs[1] - 8 : xs[0];
    for (int k = 0; k + 1 < n; k++) {
        /* the chord [xs[k], xs[k + 1]], drawn as an 8-px segment from sx (a short one at a pole is extended into
         * the wall behind it) */
        int sx = k == 0 ? first : xs[k], span = xs[k + 1] - xs[k];
        if (span <= 0) continue;
        int dy = ys[k + 1] - ys[k], s = dy * 8 / span;
        s = clampi(s, -15, 15);
        int sy = ys[k + 1] - s;                     /* the segment ends on the next point */
        if (k > 0) sy = ys[k];                      /* ... and starts exactly on its own */
        if (s >= 0) spr_w_prio(SPR_ROPE + s, sx, sy, 0, 1);
        else spr_w_prio(SPR_ROPE - s, sx, sy - 15, RS_SPR_VFLIP, 1);
    }
    /* the hooks on the walls */
    spr_w_prio(SPR_HOOK, x0 - 6, obj_line_y(o, x0) - 3, 0, 2);
    spr_w_prio(SPR_HOOK, x0 + o->w - 2, obj_line_y(o, x0 + o->w) - 3, RS_SPR_HFLIP, 2);
    /* the clothes, pegged on the rope every 16 px, swinging with it */
    int sw = (int)o->sagv;
    for (int j = 0, ax = x0 + 12; ax + 8 <= x0 + o->w; j++, ax += 16) {
        int k = (j + i) % 4, tilt = 0;
        int ph = sw + ((j & 1) ? 40 : -40) * (o->sag != 0);
        if (ph > 60) tilt = 1;
        else if (ph < -60) tilt = 2;
        spr_w_prio(SPR_CLOTHES + k * 3 + tilt, ax - 4, obj_line_y(o, ax) + 1, 0, 1);
    }
}

/* a pigeon's frame: walking, pecking, flapping, gliding, diving */
static int pigeon_frame(const obj *o, int t)
{
    if (o->state) return 3 + (t / 4) % 2;
    switch (o->var) {
    case PG_HOVER: return 3 + (t / 3) % 2;
    case PG_GLIDE: return (o->t / 20) % 5 == 0 ? 3 + (t / 4) % 2 : 5;
    case PG_SWOOP: {
        int x0, y0, x1, y1;
        pigeon_at_t(o, o->t, &x0, &y0);
        pigeon_at_t(o, o->t + 4, &x1, &y1);
        return y1 > y0 ? 6 : 3 + (t / 4) % 2;
    }
    default:
        return (o->t / 50) % 3 == 2 ? ((o->t / 10) % 2 ? 2 : 0) : (o->t / 8) % 2;
    }
}

static void draw_cat(const world *w)
{
    const cat *c = &w->c;
    if (c->state == 2) return;
    int f = c->state == 1 ? 2 : ((w->t / 40) % 3 == 0 ? 1 : 0);
    if (c->state == 0 && c->t < 8 && w->t > 8) f = 3;
    spr_w(SPR_CAT + f, (int)c->x - 8, (int)c->y - 16, f == 3 ? RS_SPR_HFLIP : 0);
}

static void draw_objects(const world *w)
{
    int t = w->t;
    for (int i = 0; i < w->no; i++) {
        const obj *o = &w->o[i];
        if (o->state == 2) continue;
        switch (o->kind) {
        case OB_PIGEON: {
            int cx, feet;
            pigeon_at(o, &cx, &feet);
            spr_w(SPR_PIGEON + pigeon_frame(o, t), cx - 8, feet - 16, o->dir < 0 ? RS_SPR_HFLIP : 0);
            break;
        }
        case OB_ANTENNA: {
            int f = o->t > 0 ? 1 + (o->t / 4) % 2 : 0;
            spr_w(SPR_BEACON + ((t + (int)o->x) / 24) % 2, (int)o->x - 4 + (f == 1 ? -2 : f == 2 ? 2 : 0), o->y - 32, 0);
            spr_w(SPR_ANTENNA + f, (int)o->x - 8, o->y - 32, 0);
            break;
        }
        case OB_BALLOON: {
            int bx = balloon_x(o), sway = o->t > 16 ? ((o->t / 4) % 2 ? 1 : -1) : 0;
            spr_w(SPR_BALLOON + (o->t > 0 && o->t <= 16), bx, o->y, 0);
            spr_w(SPR_BASKET, bx + 8 + sway, o->y + 30, 0);
            break;
        }
        case OB_AWNING:
            spr_w(SPR_AWNING + (o->t > 0), (int)o->x, o->y - SURF_AWNING, o->var ? RS_SPR_HFLIP : 0);
            break;
        case OB_POT:
            spr_w(SPR_POT, (int)o->x, o->y - SURF_POT, o->var ? RS_SPR_HFLIP : 0);
            break;
        case OB_CRADLE:
            spr_w(SPR_CRADLE, (int)o->x, (int)(o->pos >> 16) - SURF_CRADLE, 0);
            break;
        case OB_LEDGE:
            for (int k = 0; k < o->w / 8; k++)
                spr_w(SPR_LEDGE + (k == 0 ? 0 : k == o->w / 8 - 1 ? 2 : 1), (int)o->x + k * 8,
                      o->y - SURF_LEDGE + (o->state ? k * o->t / 8 : 0), 0);
            break;
        case OB_BAGUETTE:
            for (int k = 0; k < o->w / 8; k++)
                spr_w(SPR_BAGUETTE + (k == 0 ? 0 : k == o->w / 8 - 1 ? 2 : 1), (int)o->x + k * 8, o->y - SURF_BAGUETTE, 0);
            break;
        case OB_LINE:
            draw_rope(o, i);
            break;
        case OB_ITEM: {
            int bob = ((t / 8) % 4 == 1) - ((t / 8) % 4 == 3);
            spr_w(SPR_ITEM + o->var, (int)o->x, o->y - 16 + bob, 0);
            spr_w(SPR_GLOW + (t / 12) % 2, (int)o->x, o->y - 16 + bob, 0);
            break;
        }
        case OB_GUST: {
            /* the columns shown, in the world (the screen's; a zoomed screen's are fewer), and its lines */
            int l = camx_i + rs_screen_visible_x0() - rs_screen_origin_x();
            int r = camx_i + rs_screen_visible_x1() - rs_screen_origin_x();
            int x0 = (int)o->x > l ? (int)o->x : l, x1 = o->x + o->w < r ? (int)(o->x + o->w) : r;
            int y0 = tall() ? camy_i + rs_screen_top() + HUD_LINES + 8 : camy_i + 24;
            int dy = tall() ? (rs_screen_height() - HUD_LINES - 40) / 6 : 31;
            if (x1 - x0 <= 24) break;
            for (int k = 0; k < 6; k++) {
                int span = x1 - x0 - 16;
                int ph = (((t * 3 * (o->dir < 0 ? -1 : 1) + k * 57) % span) + span) % span;
                spr_w(SPR_WIND + (k & 1), x0 + ph, y0 + k * dy + (k * 13 + t / 3) % 9, o->dir < 0 ? RS_SPR_HFLIP : 0);
            }
            break;
        }
        default:
            break;
        }
    }
}

static void draw_fx(void)
{
    for (int i = 0; i < FX_MAX; i++) {
        const fx_obj *f = &fx[i];
        int x = (f->x >> 8) - camx_i, y = (f->y >> 8) - camy_i;
        switch (f->kind) {
        case FX_DUST: spr_at(SPR_DUST + clampi(f->t * 3 / f->life, 0, 2), x, y, -1, 0); break;
        case FX_SHARD: spr_at(SPR_SHARD + f->frame, x, y, -1, 0); break;
        case FX_DEBRIS: spr_at(SPR_DEBRIS + f->frame, x, y, -1, 0); break;
        case FX_FEATHER: spr_at(SPR_FEATHER + (f->t / 6) % 2, x, y, -1, 0); break;
        case FX_STREAK: spr_at(SPR_WIND + (f->t / 4) % 2, x, y, -1, f->frame ? RS_SPR_HFLIP : 0); break;
        case FX_STAR: spr_at(SPR_STAR + (f->t / 4) % 2, x, y, -1, 0); break;
        case FX_SPLASH: spr_at(SPR_SPLASH + clampi(f->t * 3 / f->life, 0, 2), x, y, -1, 0); break;
        case FX_POP: {                                /* "+50", then "x2" for a chain */
            char s[12];
            int n = 0, v = f->n;
            do { s[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 6);
            spr_at(SPR_SMALL + 10, x, y, -1, 0);
            for (int k = 0; k < n; k++) spr_at(SPR_SMALL + (s[n - 1 - k] - '0'), x + 6 + k * 6, y, -1, 0);
            if (f->frame > 1) {
                spr_at(SPR_SMALL + 11, x + 6 + n * 6 + 2, y, -1, 0);
                spr_at(SPR_SMALL + clampi(f->frame, 0, 9), x + 12 + n * 6 + 2, y, -1, 0);
            }
            break;
        }
        default: break;
        }
    }
}

static void draw_cafe(const world *w)
{
    for (int p = 0; p < w->players; p++) {
        const mamie *m = &w->m[p];
        if ((m->state != MS_FALL && m->state != MS_DOWN) || m->down_kind) continue;
        int pressed = m->fall_phase == 1 && m->state == MS_FALL && m->t < 400 && m->vy < 0;
        int x = m->reel_x - 32, y = STREET_Y - 24 - 2;
        spr_w(SPR_CAFE + (pressed ? 2 : 0), x, y, 0);
        spr_w(SPR_CAFE + (pressed ? 2 : 1), x + 32, y, pressed ? RS_SPR_HFLIP : 0);
    }
}

/* ---- per frame ----------------------------------------------------------------------------------------------------- */
void draw_reset(const world *w)
{
    draw_invalidate();
    memset(fx, 0, sizeof fx);
    pan_x0 = -1;
    look_update(w, 1);
}

/* A tall screen's frame (DESIGN.md "Tall screen"). The screen shows the world from TALL_ABOVE lines above the camera
 * down to the street at most (the stage lower then: the screen still, the stage moving in it); the shake moves the
 * playfield, not that view. One player playing (or fallen) fills the display's height, the columns shown panned so
 * that the leader stays near their left (PAN_MIN .. PAN_MAX: more of what comes ahead); the title, "get ready", the
 * panels and a race (2-4 players: every racer must be seen) show every column. The HUD at the screen's top. */
#define TALL_MID_Y 84                           /* the mid-ground's virtual y at the screen's top for a view_y of 0 */
#define TALL_FAR_Y (-16)                        /* the backdrop's map y ... */
#define PAN_MIN    32                           /* the leader's x in the columns shown: kept from PAN_MIN to PAN_MAX */
#define PAN_MAX    60
static void tall_frame(const world *w, int state)
{
    int wy = world_camy(w), play = state == DS_PLAY || state == DS_FALL, one = play && w->players == 1;
    int vy = wy - TALL_ABOVE < WORLD_H - rs_screen_height() ? wy - TALL_ABOVE : WORLD_H - rs_screen_height();
    int o = clampi(wy - vy, 0, rs_screen_height() - RS_SCREEN_H);
    rs_screen_origin_v(o);
    view_y = camy_i - o;                            /* (the shake included: the playfield shakes) */
    view_cam_y = camy_i;
    rs_screen_fill(!play || w->players == 1);   /* zoomed but for 2-4 racing (they spread over every column) */
    hud_lines = play ? HUD_LINES : 0;
    rs_bg_pin(RS_BG1, 0, hud_lines, 0);
    int v = rs_screen_visible_x1() - rs_screen_visible_x0();
    if (one) {
        int lx = (int)(w->m[w->leader].x >> 16) - camx_i;
        if (pan_x0 < 0) pan_x0 = lx - (PAN_MIN + PAN_MAX) / 2;
        else if (lx < pan_x0 + PAN_MIN) pan_x0 -= clampi(pan_x0 + PAN_MIN - lx, 0, 2);
        else if (lx > pan_x0 + PAN_MAX) pan_x0 += clampi(lx - pan_x0 - PAN_MAX, 0, 2);
        pan_x0 = clampi(pan_x0, 0, RS_SCREEN_W - v);
        rs_screen_focus_x(pan_x0 + v / 2);
    } else {
        pan_x0 = -1;
        rs_screen_focus_x(-1);
    }
    ui_dx = one ? (rs_screen_visible_x0() + rs_screen_visible_x1()) / 2 - RS_SCREEN_W / 2 : 0;
}

void draw_frame(const world *w, int state, int st_t, int paused, int best_m)
{
    /* a crash shakes the playfield and the world's sprites (never the UI): house_ui.c hu_shake */
    int camx = world_camx(w) + hu_shake_x(), camy = world_camy(w) + hu_shake_y();
    if (camx < 0) camx = 0;
    if (camy < 0) camy = 0;
    camx_i = camx;
    camy_i = camy;
    if (tall()) tall_frame(w, state);
    look_update(w, 0);
    sky_lines(camy);
    stream_bg2(w, camx, camy);
    stream_bg4(camx);
    int pm = scenery_w() * 8 - 1;                   /* the maps' period - 1: 511, or 1023 on a wide screen */
    int ym = map_rows() * 8 - 1;                    /* ... and their height - 1: 255, or 511 on a tall screen */
    scroll_x = camx & pm;
    scroll_y = camy & ym;
    rs_bg_scroll(RS_BG2, scroll_x, scroll_y);
    int xc = camx + rs_screen_left() + rs_screen_width() / 2, drop_to = district_at(xc) == 3 && !night_at(xc) ? 56 : 0;
    mid_drop += mid_drop < drop_to ? 1 : mid_drop > drop_to ? -1 : 0;
    if (tall()) {
        /* a tall screen: the mid-ground and the backdrop placed from the world y of the screen's top (their parallax
         * the stage's), so that the far landmarks and the roofs stand above the playfield as on the stage */
        int a3 = mid_a3 = fdiv(view_y, 2) + TALL_MID_Y - mid_drop, top = rs_screen_top();
        stream_bg3(a3, rs_screen_height());
        rs_bg_scroll(RS_BG3, (camx / 2) & pm, (a3 - top) & ym);
        rs_bg_scroll(RS_BG4, (camx / 8) & pm, (fdiv(view_y, 4) + TALL_FAR_Y - top) & ym);
    } else {
        int s3 = camy / 2 + 128 - mid_drop;
        stream_bg3(s3, RS_SCREEN_H);
        rs_bg_scroll(RS_BG3, (camx / 2) & pm, s3 & 255);
        rs_bg_scroll(RS_BG4, (camx / 8) & pm, 40 + camy / 4);
    }
    (void)paused;
    /* sprites, front to back: the UI (ui.c), the players, effects, the cat, things */
    rs_oam_clear();
    ui_sprites(w, state, st_t, best_m);
    draw_fx();
    for (int p = 0; p < w->players; p++) draw_player(w, p);
    draw_cat(w);
    draw_objects(w);
    draw_cafe(w);
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(cur_look); S(tgt_look); S(shown_look); S(line_col); S(bg2_key); S(far_col); S(bg3_row); S(mid_drop); S(far_slot); S(sky_redrawn);
    S(sky_redrawn_n); S(test_backdrop); S(scroll_x); S(scroll_y); S(night_blink); S(fx_rng); S(fx); S(camx_i); S(camy_i); S(drawn_width);
    S(pan_x0);
    RS_STATE_RASTER(raster);
}
#undef S
