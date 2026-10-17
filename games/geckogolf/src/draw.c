/*
 * Gecko Golf: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1), BG2 the course (drawn here into tiles as the camera moves), BG3 the far
 *   hills (parallax 1/4, in front of the sky's sprites), the backdrop a raster sky gradient; sprites: the gecko and the
 *   balls (OBJ 0; players 2-4 OBJ 1, 4, 5 = the palette swaps), the props (OBJ 2: the flag, the aim, the gauge), the
 *   kit's digits, glyphs and medals (OBJ 3), the sky (OBJ 6: clouds, sun, moon, stars, behind the hills), the decor
 *   (OBJ 7).
 * The course: one height per px (play.c); each 8-px column of the screen is cut into tiles: blank above the ground,
 * shared deep soil tiles below it (specks, pebbles, two wavy strata), and the tiles the surface crosses drawn pixel by
 * pixel (the dark top line, the grass, sand, ice or mushroom band, thicker on a steep slope, the water, the cup, the
 * cliffs' outline and lit or shaded face, the roots, the topsoil fading into the deep soil), shared when two are the
 * same. They are drawn again only when the camera moves (the pan between holes) or the course changes (a cup dug or
 * filled). Each biome its palette (TERRAIN_PAL): the biomes at the screen's two edges are BG palettes 1 and 2 (by the
 * biome's number along the run), so both show during the pan, and the soil fades from one into the other at their
 * border (BG palette 4 holds both soils, an ordered dither); the sky, the hills and the clouds blend.
 * Widescreen (docs/spec.md "Widescreen", DESIGN.md): the 320-px stage (one hole) is centred (rs_screen_origin) and a
 * wider screen shows more of the course on both sides (the last hole behind, the next one ahead); the UI kit centres
 * itself.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

/* the title logo: one 3-shade ramp per word (light, mid, dark): GECKO in the gecko's orange, GOLF in fairway green */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 214, 130), RS_RGB8(250, 160, 58), RS_RGB8(214, 104, 34)},
    {RS_RGB8(180, 236, 120), RS_RGB8(110, 200, 80), RS_RGB8(60, 150, 60)}};

/* ---- the biomes' colours ---------------------------------------------------------------------------------------------- */
/* the course: 1-3 grass (light, mid, dark), 4-6 soil, 7-8 sand, 9-10 ice, 11 mushroom cap, 12 its spots, 13-14 water,
 * 15 the dark line (the ground's top, the cliffs, the cup) */
static const uint32_t TERRAIN_PAL[BI_COUNT][15] = {
    {0x96d65a, 0x62b240, 0x408c34, 0xb07c50, 0x8c5e3a, 0x68442c, 0xf8e4aa, 0xdec080, 0xe6f6ff, 0xaad6f0,
     0xe24048, 0xfff0dc, 0x8cd2fa, 0x4696dc, 0x283422},        /* meadow */
    {0xfad68c, 0xeab460, 0xd09246, 0xc47848, 0xa65e38, 0x80462c, 0xfff0c8, 0xecd6a0, 0xe6f6ff, 0xaad6f0,
     0xdc5a3c, 0xffecc8, 0x6ed2dc, 0x32a0b4, 0x582e1e},        /* dunes */
    {0xfafcff, 0xdceaf8, 0xb8cee8, 0x96a0b6, 0x747e96, 0x565e78, 0xecdebe, 0xccba96, 0xd6f8ff, 0x82ceec,
     0xd6465a, 0xfaf0f0, 0x6eaadc, 0x3c6eb4, 0x28304a},        /* snowy hills */
    {0x5a9678, 0x3c7460, 0x28564c, 0x423e5a, 0x322e48, 0x242038, 0xb0aa96, 0x8c8274, 0xaad2f0, 0x6e96c8,
     0x78f0dc, 0xdca0ff, 0x3c5aa0, 0x243878, 0x101222},        /* night */
    {0xffc8e2, 0xfa96c4, 0xe268a0, 0x8c563c, 0x70402e, 0x542e22, 0xffecaa, 0xf0ce78, 0xdcfaff, 0x96dcfa,
     0x6ed696, 0xf0fff0, 0xbe8cf0, 0x8c5ad2, 0x461e28}};       /* candy land */
/* the sky's gradient: top, horizon */
static const uint32_t SKY_GRAD[BI_COUNT][2] = {
    {0x4898d8, 0xc8e8f8}, {0x6096d2, 0xffd6a0}, {0x82aad6, 0xe2ecf6}, {0x0e1434, 0x3c3a6e}, {0xb48ce6, 0xffcee6}};
/* the far hills: far light, far shade, near light, near mid, near dark */
static const uint32_t HILL_PAL[BI_COUNT][5] = {
    {0xaacede, 0x96becc, 0x7eb2a0, 0x6aa092, 0x588c82}, {0xe8c4aa, 0xd6b29c, 0xe2aa78, 0xce9668, 0xb8825a},
    {0xd2e0f0, 0xbecee4, 0xe6eef8, 0xc8d6ea, 0xaabcd6}, {0x282c5a, 0x222650, 0x1e2e46, 0x18263c, 0x121e32},
    {0xe6bef0, 0xd6aae6, 0xfab4d2, 0xeca0c4, 0xd68cb4}};
/* the sky's sprites: cloud light, mid, shade, sun core, mid, rim, moon light, shade, star */
static const uint32_t SKY_PAL[BI_COUNT][9] = {
    {0xffffff, 0xecf4fc, 0xcee0f0, 0xfffad2, 0xffe88c, 0xffce6e, 0xf0f0dc, 0xbec4c8, 0xfffac8},
    {0xfffaf0, 0xfaecdc, 0xeccebe, 0xfff4c8, 0xffd278, 0xfaaa5a, 0xf0f0dc, 0xbec4c8, 0xfffac8},
    {0xfafcff, 0xe2eaf6, 0xc4d2e6, 0xfffcec, 0xfaf0c8, 0xece2be, 0xf0f0dc, 0xbec4c8, 0xfffac8},
    {0x464a78, 0x383c68, 0x2c305a, 0xfffad2, 0xffe88c, 0xffce6e, 0xf0f0dc, 0xbec4c8, 0xfffac8},
    {0xfff0fa, 0xfad6ec, 0xecb4dc, 0xfffadc, 0xffdcaa, 0xffb4c8, 0xf0f0dc, 0xbec4c8, 0xfffac8}};

#define WATER_D   10            /* px of water drawn under a pond's surface */
#define POOL_MAX  448           /* the course's tiles that change (shared when the same) */
#define N_PLAIN   4             /* the deep soil's tiles: plain (specks), pebbles, the strata's wave (4 phases) */
#define N_PEBBLE  3
#define N_STRATA  4
#define N_DEEP    (N_PLAIN + N_PEBBLE + N_STRATA)
#define T_DEEP    1             /* the deep soil tiles (1..N_DEEP); the pool after */
#define T_POOL    (T_DEEP + N_DEEP)
#define HASH_N    1024
#define TOPSOIL   14            /* px of lighter topsoil under the grass band, then a dithered fade into the deep soil */
#define BAND_DEEP 22            /* below a column's top (its lip), every pixel is the deep soil's (or a cliff's face) */
/* the soil between two biomes: near the border the tiles of plain soil fade from one biome's soil into the other's, an
 * ordered dither over BLEND_W px, in BG palette 4 (free: docs/art-direction.md) holding both soils, so the ground never
 * shows a straight cut */
#define PAL_BLEND 4
#define BLEND_W   48

static rs_color line_col[RS_SCREEN_H];
static int shown_state = -1, shown_best = -1, shown_ui = -1;
static int32_t drawn_cam = 0x7fffffff;
static uint32_t drawn_rev;
static int drawn_width;
static int fx_t = 99, fx_x, fx_y;           /* the sand puff */
static int sky_key = -1;
/* scratch (state_audit.txt): the tiles of the last rebuild, to share the same ones */
static uint8_t pool_px[POOL_MAX][64];
static int16_t pool_hash[HASH_N];
static int pool_n;

static rs_color hexc(uint32_t h) { return RS_HEX(h); }

static void raster(int line, void *user)
{
    (void)user;
    rs_pal_set(0, line_col[line]);
}

static void sky_gradient(uint32_t top, uint32_t hor)
{
    int a[3] = {(int)(top >> 16) & 255, (int)(top >> 8) & 255, (int)top & 255};
    int b[3] = {(int)(hor >> 16) & 255, (int)(hor >> 8) & 255, (int)hor & 255};
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int k = y < 200 ? y * 256 / 200 : 256, c[3];
        for (int i = 0; i < 3; i++) {
            c[i] = (a[i] * (256 - k) + b[i] * k) / 256 + ((y & 4) ? (i == 1 ? 2 : 4) : 0);
            if (c[i] > 255) c[i] = 255;
        }
        /* house rule: a 2-line dither step every 8 lines keeps the RGB555 bands soft */
        line_col[y] = RS_RGB8(c[0], c[1], c[2]);
    }
}

static uint32_t lerp_hex(uint32_t a, uint32_t b, int k)
{
    uint32_t r = 0;
    for (int s = 0; s < 24; s += 8) {
        int ca = (int)(a >> s) & 255, cb = (int)(b >> s) & 255;
        r |= (uint32_t)((ca * (256 - k) + cb * k) / 256) << s;
    }
    return r;
}

/* Widescreen: the maps are 64 tiles (512 px) wide, 128 on a screen wider than 496 columns (the course needs one
 * column more than the screen). The map size is saved in states: draw_state_loaded() sets the maps up again. */
static int map_w(void) { return rs_screen_width() > 496 ? 128 : 64; }

/* ---- the course's tiles ------------------------------------------------------------------------------------------------ */
/* the deep soil (colours 4-6 only, so its tiles can take a biome blend's palette): dark soil with specks, pebbles lit
 * from the top-left, and a wavy stratum every few rows (a 32-px wave over 4 tiles). Made once by draw_init. */
static uint8_t deep_tiles[N_DEEP][64];

static void deep_make(void)
{
    static const uint8_t SPECK[N_PLAIN][3][2] = {{{2, 1}, {6, 3}, {1, 6}}, {{4, 2}, {7, 5}, {3, 7}},
                                                 {{1, 3}, {5, 6}, {6, 1}}, {{3, 0}, {0, 4}, {5, 4}}};
    /* the pebbles: 0 the soil, 1 the light top-left, 2 the body, 3 its shaded underside (the topsoil's tone) */
    static const char *PEBBLE[N_PEBBLE][8] = {
        {"........", "........", "..112...", ".11222..", ".122223.", "..2233..", "........", "........"},
        {"........", ".....12.", "....1223", ".....33.", "........", ".12.....", "..3.....", "........"},
        {"........", "........", "........", "...112..", "..12222.", "..22233.", "...33...", "........"}};
    for (int v = 0; v < N_DEEP; v++) memset(deep_tiles[v], 6, 64);
    for (int v = 0; v < N_PLAIN; v++)
        for (int i = 0; i < 3; i++) {
            int x = SPECK[v][i][0], y = SPECK[v][i][1];
            deep_tiles[v][y * 8 + x] = 5;
            if (i == 2 && x < 7) deep_tiles[v][y * 8 + x + 1] = 5;      /* a 2-px dash */
        }
    for (int v = 0; v < N_PEBBLE; v++)
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                char c = PEBBLE[v][y][x];
                if (c != '.') deep_tiles[N_PLAIN + v][y * 8 + x] = (uint8_t)(c == '1' ? 4 : 5);
                if (c == '3') deep_tiles[N_PLAIN + v][y * 8 + x] = 6;  /* (the underside reads as the soil's shadow) */
            }
    /* the stratum: a 2-px band of the mid tone along a gentle wave, lit on top where it rises */
    static const int8_t WAVE[32] = {3, 3, 3, 2, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4,
                                    4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 3, 3};
    for (int v = 0; v < N_STRATA; v++) {
        uint8_t *tl = deep_tiles[N_PLAIN + N_PEBBLE + v];
        for (int x = 0; x < 8; x++) {
            int X = v * 8 + x, y0 = WAVE[X], yp = WAVE[(X + 31) & 31];
            tl[y0 * 8 + x] = 5;
            tl[(y0 + 1) * 8 + x] = 5;
            if (y0 < yp || (X % 5) == 1) tl[(y0 - 1) * 8 + x] = 4;     /* the light top edge */
            if ((X % 7) == 3) tl[(y0 + 2) * 8 + x] = 5;                /* crumbs under it */
        }
    }
}

/* the deep tile at tile column tx, row ty: a stratum on rows 16 and 23 (clear of the title's and the hint's text rows), else specks with now and then a pebble */
static int deep_tile(int32_t tx, int ty)
{
    if (ty == 16 || ty == 23) return N_PLAIN + N_PEBBLE + (int)(tx & 3);
    uint32_t h = (uint32_t)tx * 0x9e3779b1u + (uint32_t)ty * 0x85ebca77u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    if (h % 11u == 0) return N_PLAIN + (int)((h >> 8) % N_PEBBLE);
    return (int)((h >> 12) % N_PLAIN);
}

static int deep_px(int32_t wx, int y) { return deep_tiles[deep_tile(wx >> 3, y >> 3)][(y & 7) * 8 + (wx & 7)]; }

/* a column's top as its neighbours see it: a cup's lip, not its floor */
static int lip_y(const terrain *t, int32_t wx)
{
    int y = terrain_y(t, wx);
    return terrain_surf(t, wx) == SF_CUP ? y - CUP_DEPTH : y;
}

/* the band's extra thickness on a slope (the grass keeps its look along a steep slope instead of thinning into steps);
 * none at a cliff's top (a wall in the next two columns) */
static int band_extra(const terrain *t, int32_t wx)
{
    int a = lip_y(t, wx + 2) - lip_y(t, wx - 2);
    if (a < 0) a = -a;
    return a > 16 ? 0 : a / 2 > 6 ? 6 : a / 2;
}

/* a root under the grass at world x: its length (2-5 px), or 0 (about one column in 11) */
static int root_len(int32_t wx)
{
    uint32_t h = (uint32_t)wx * 0x9e3779b1u;
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    return h % 11u == 0 ? 2 + (int)((h >> 8) & 3) : 0;
}

/* the soil at depth d under the band:the topsoil (lighter, specks), an ordered-dither fade, the deep soil */
static int soil_px(int32_t wx, int y, int d)
{
    int top = ((wx * 7 + y * 3) % 19 == 0) ? 4 : ((wx * 11 + y * 5) % 23 == 0) ? 6 : 5;
    if (d < TOPSOIL - 2) return top;
    if (d == TOPSOIL - 2) return ((wx + 2 * y) & 3) == 0 ? 6 : top;
    if (d == TOPSOIL - 1) return ((wx + y) & 1) ? 6 : top;
    if (d == TOPSOIL) return ((wx + 2 * y) & 3) == 0 ? top : deep_px(wx, y);
    return deep_px(wx, y);
}

/* the colour index of the course at world x, screen y */
static int course_px(const terrain *t, int32_t wx, int y)
{
    int top = terrain_y(t, wx), s = terrain_surf(t, wx);
    if (s == SF_CUP) {
        int lip = top - CUP_DEPTH;
        if (y < lip) return 0;
        if (y <= top) return 15;                        /* the cup: dark, its floor too */
        return soil_px(wx, y, y - lip);                 /* under it the soil, level with the green's */
    }
    if (y < top) return 0;
    int d = y - top;
    if (s == SF_WATER) {
        if (d == 0) return 13;
        if (d < WATER_D) return ((wx * 3 + y * 5) % 17 == 0) ? 13 : 14;
        if (d == WATER_D) return 15;
        return soil_px(wx, y, d);
    }
    int nl = lip_y(t, wx - 1), nr = lip_y(t, wx + 1);
    if (d > 0 && (y < nl || y < nr)) return 15;         /* a cliff's side */
    if (d == 0) return 15;
    int ex = band_extra(t, wx), band = 5 + ex;
    if (d <= band) {
        int mid = 1 + (band - 1) / 2;                   /* 3 on the flat */
        switch (s) {
        case SF_SAND: return (d == band || ((wx * 5 + y * 3) % 7 == 0)) ? 8 : 7;
        case SF_ICE: return d == 1 ? 9 : (d < band && ((wx + y * 2) % 9 == 0)) ? 9 : 10;
        case SF_MUSH: return ((wx * 3 + y * 5) % 6 == 0 && d > 1) ? 12 : 11;
        default:
            if (d == 1) return 1;
            if (biome_of_seq(terrain_seq(t, wx)) == BI_CANDY && d >= 2 && d < band && (wx * 7 + y * 5) % 11 == 0)
                return (wx & 1) ? 11 : 9;               /* sprinkles in the frosting */
            if (d <= mid) return 2;
            return (d == mid + 1 && (wx % 3) == 0) ? 2 : 3;
        }
    }
    if (s == SF_MUSH && d <= band + 3) return 12;
    /* a cliff's face, the column inside its outline: lit when it faces left (the light from the top-left), in shade
     * when it faces right */
    if (y < lip_y(t, wx - 2)) return 4;
    if (y < lip_y(t, wx + 2)) return 6;
    /* the grass's roots in the topsoil (the meadow, the night course): a thread down, a step aside at its end */
    if (s == SF_GRASS && d - band <= 6) {
        int bio = biome_of_seq(terrain_seq(t, wx));
        if (bio == BI_MEADOW || bio == BI_NIGHT) {
            int r0 = root_len(wx), r1 = root_len(wx - 1);
            if ((r0 && d - band <= r0) || (r1 && d - band == r1 + 1)) return 6;
        }
    }
    return soil_px(wx, y, d - ex);
}

static int pool_tile(const uint8_t *px)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < 64; i++) h = (h ^ px[i]) * 16777619u;
    int slot = (int)(h & (HASH_N - 1));
    while (pool_hash[slot] >= 0) {
        if (!memcmp(pool_px[pool_hash[slot]], px, 64)) return T_POOL + pool_hash[slot];
        slot = (slot + 1) & (HASH_N - 1);
    }
    if (pool_n >= POOL_MAX) return T_DEEP;              /* never on a real course: the pool is far bigger */
    memcpy(pool_px[pool_n], px, 64);
    pool_hash[slot] = (int16_t)pool_n;
    rs_tiles_load8(VR_BG2 + T_POOL + pool_n, px, 1);
    return T_POOL + pool_n++;
}

/* a tile of plain soil (colours 4-6) at x0 px from a biome's border (the right biome from 0): the right biome's pixels
 * become 7-9 (PAL_BLEND holds the left soil at 4-6, the right soil at 7-9), by a 4x4 ordered dither over BLEND_W px,
 * the border a little ragged from row to row; returns the right biome's pixel count */
static int blend_tile(uint8_t *px, int32_t x0, int r)
{
    static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    int wob = (int)((((uint32_t)r * 2654435761u) >> 27) % 9u) - 4, n = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int dx = (int)x0 + x + wob + BLEND_W / 2, lv = dx <= 0 ? 0 : dx >= BLEND_W ? 17 : dx * 17 / BLEND_W;
            if (BAYER[(y & 3) * 4 + (int)((x0 + x) & 3)] < lv) {
                px[y * 8 + x] = (uint8_t)(px[y * 8 + x] + 3);
                n++;
            }
        }
    return n;
}

static void course_build(const world *w)
{
    const terrain *t = &w->tr;
    int mw = map_w();
    int32_t wx0 = w->view_cam + rs_screen_left();
    int32_t wt0 = wx0 >= 0 ? wx0 / 8 : -((-wx0 + 7) / 8);
    int cols = (rs_screen_width() + 7) / 8 + 1;
    pool_n = 0;
    memset(pool_hash, 0xff, sizeof pool_hash);
    uint8_t px[64];
    for (int c = 0; c < cols; c++) {
        int32_t wt = wt0 + c, bx = wt * 8;
        int lo = 999, hi = -999;
        for (int x = 0; x < 8; x++) {
            int top = lip_y(t, bx + x);
            if (top < lo) lo = top;
            if (top + BAND_DEEP > hi) hi = top + BAND_DEEP;
        }
        /* a cliff next to this column: its side and its face run down to the lower neighbours' tops */
        for (int x = -2; x < 10; x++) {
            int n = terrain_y(t, bx + x);
            if (n > hi) hi = n;
        }
        int seq = terrain_seq(t, bx + 4);
        int pal = (seq & 1) ? PAL_TERRAIN_B : PAL_TERRAIN_A;
        /* near a biome's border, the plain soil fades from one biome's into the other's (ordered dither) */
        int32_t xb = 0;
        int reach = BLEND_W / 2 + 12;
        int sl = terrain_seq(t, bx + 4 - reach), sr = terrain_seq(t, bx + 4 + reach), near = 1;
        if (sl != seq) for (xb = bx + 4 - reach; terrain_seq(t, xb) != seq; xb++) {}
        else if (sr != seq) for (xb = bx + 4; terrain_seq(t, xb) == seq; xb++) {}
        else near = 0;
        int pal_l = (sl & 1) ? PAL_TERRAIN_B : PAL_TERRAIN_A, pal_r = (sr & 1) ? PAL_TERRAIN_B : PAL_TERRAIN_A;
        int mx = (int)(((wt % mw) + mw) % mw);
        for (int r = 0; r < 30; r++) {
            int tile = -1, soil = 1, p = pal;
            if (r * 8 + 7 < lo) tile = 0, soil = 0;
            else if (r * 8 >= hi) {
                tile = T_DEEP + deep_tile(wt, r);
                memcpy(px, deep_tiles[tile - T_DEEP], 64);
            } else {
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++) {
                        int v = course_px(t, bx + x, r * 8 + y);
                        px[y * 8 + x] = (uint8_t)v;
                        if (v < 4 || v > 6) soil = 0;
                    }
            }
            if (near && soil) {
                int n_r = blend_tile(px, bx - xb, r);
                if (n_r == 64) p = pal_r;
                else if (n_r == 0) p = pal_l;
                else p = PAL_BLEND, tile = pool_tile(px);
                if (n_r == 64) for (int i = 0; i < 64; i++) px[i] = (uint8_t)(px[i] - 3);
            }
            if (tile < 0) tile = pool_tile(px);
            rs_bg_put(RS_BG2, mx, r, RS_MAP(tile, p, 0, 0, 0));
        }
    }
    drawn_cam = w->view_cam;
    drawn_rev = t->rev;
    drawn_width = rs_screen_width();
}

static void maps_setup(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG2, mw, 32, VR_BG2);
    rs_bg_setup(RS_BG3, mw, 32, VR_BG3);
    rs_bg_fill(RS_BG2, 0);
    rs_bg_fill(RS_BG3, 0);
    for (int y = 0; y < HILLS_H; y++)
        for (int x = 0; x < 64; x++) {
            uint16_t e = (uint16_t)(gm_hills_map[y * 64 + x] | RS_MAP_PRIO);    /* in front of the sun and clouds */
            rs_bg_put(RS_BG3, x, HILLS_Y / 8 + y, e);
            if (mw > 64) rs_bg_put(RS_BG3, 127 - x, HILLS_Y / 8 + y, (uint16_t)(e ^ RS_MAP_HFLIP));
        }
    drawn_cam = 0x7fffffff;
    shown_state = -1;
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more course on both sides */
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0 on palette 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;         /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    uint8_t blank[64] = {0};
    rs_tiles_load8(VR_BG2, blank, 1);
    deep_make();
    rs_tiles_load8(VR_BG2 + T_DEEP, &deep_tiles[0][0], N_DEEP);
    rs_tiles_load(VR_BG3, gm_bg3_tiles, gm_bg3_tile_count);
    maps_setup();
    for (int l = 0; l < 3; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* the gecko, player 2, the props (the kit loaded OBJ 3) */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 64);     /* players 3 and 4, the sky, the decor */
    sky_gradient(SKY_GRAD[0][0], SKY_GRAD[0][1]);
    rs_raster(raster, NULL);
}

static void spr_f(int id, int x, int y, int prio, int pal, int flags)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

static void spr(int id, int x, int y, int prio, int pal) { spr_f(id, x, y, prio, pal, 0); }

/* player p's sprite palette: the house rule (P1 OBJ 0, P2 OBJ 1, P3 OBJ 4, P4 OBJ 5: house_ui.h hu_player_pal) */
static int player_pal(int p) { return p ? hu_player_pal(p) : 0; }

/* ---- the biome blend: the camera's centre between two biomes ------------------------------------------------------ */
typedef struct blend { int a, b, k, seq_a, seq_b; } blend;   /* biome a -> b, k/256 of b */

static void biome_blend(const world *w, blend *bl)
{
    const terrain *t = &w->tr;
    int32_t cx = w->view_cam + RS_SCREEN_W / 2;
    int qa = terrain_seq(t, cx - 96), qb = terrain_seq(t, cx + 96);
    bl->seq_a = qa;
    bl->seq_b = qb;
    bl->a = biome_of_seq(qa);
    bl->b = biome_of_seq(qb);
    bl->k = 0;
    if (qa == qb) return;
    int32_t sw = cx - 96;
    while (sw < cx + 96 && terrain_seq(t, sw) == qa) sw++;
    int k = (int)(cx - sw + 96) * 256 / 192;
    bl->k = k < 0 ? 0 : k > 256 ? 256 : k;
}

static void palettes(const world *w, const blend *bl)
{
    /* the course: the biomes at the screen's two edges (by their number's parity; the tiles drawn reach 8 px past the
     * edges, a border's blend 40 px further), and the two blends between them for the soil at their border */
    int32_t wx0 = w->view_cam + rs_screen_left();
    int seqs[2] = {terrain_seq(&w->tr, wx0 - 48), terrain_seq(&w->tr, wx0 + rs_screen_width() + 56)};
    for (int i = 0; i < 2; i++) {
        int bio = biome_of_seq(seqs[i]), pal = (seqs[i] & 1) ? PAL_TERRAIN_B : PAL_TERRAIN_A;
        for (int c = 0; c < 15; c++) rs_pal_set(RS_PAL_BG(pal) + 1 + c, hexc(TERRAIN_PAL[bio][c]));
        /* the water shimmers: its light colour breathes */
        int ph = (w->t / 6) % 16, k = ph < 8 ? ph * 10 : (16 - ph) * 10;
        rs_pal_set(RS_PAL_BG(pal) + 13, hexc(lerp_hex(TERRAIN_PAL[bio][12], 0xffffff, k)));
    }
    for (int c = 0; c < 3; c++) {
        rs_pal_set(RS_PAL_BG(PAL_BLEND) + 4 + c, hexc(TERRAIN_PAL[biome_of_seq(seqs[0])][3 + c]));
        rs_pal_set(RS_PAL_BG(PAL_BLEND) + 7 + c, hexc(TERRAIN_PAL[biome_of_seq(seqs[1])][3 + c]));
    }
    for (int c = 0; c < 5; c++) rs_pal_set(RS_PAL_BG(PAL_HILLS) + 1 + c, hexc(lerp_hex(HILL_PAL[bl->a][c], HILL_PAL[bl->b][c], bl->k)));
    for (int c = 0; c < 9; c++) rs_pal_set(RS_PAL_OBJ(OBJ_PAL_SKY) + 1 + c, hexc(lerp_hex(SKY_PAL[bl->a][c], SKY_PAL[bl->b][c], bl->k)));
    int key = bl->a * 4096 + bl->b * 256 + bl->k;
    if (key != sky_key) {
        sky_gradient(lerp_hex(SKY_GRAD[bl->a][0], SKY_GRAD[bl->b][0], bl->k),
                     lerp_hex(SKY_GRAD[bl->a][1], SKY_GRAD[bl->b][1], bl->k));
        sky_key = key;
    }
}

/* ---- the sky: sun or moon, clouds drifting (parallax 1/8), stars at night ------------------------------------------ */
static void sky_sprites(const world *w, const blend *bl)
{
    int bio = bl->k < 128 ? bl->a : bl->b;
    int L = rs_screen_left(), span = rs_screen_width() + 96;
    if (bio == BI_NIGHT) {
        for (int i = 0; i < 18; i++) {
            uint32_t h = (uint32_t)i * 2654435761u;
            int x = L + (int)(h % (uint32_t)rs_screen_width()), y = 6 + (int)((h >> 12) % 92u);
            spr(SPR_STAR + (((w->t >> 4) + i * 3) % 7 == 0), x, y, 0, -1);
        }
        spr(SPR_MOON, 236, 18, 0, -1);
    } else {
        spr(SPR_SUN, bio == BI_DUNES ? 222 : 240, bio == BI_DUNES ? 30 : 16, 0, -1);
    }
    static const int16_t CX[6] = {0, 150, 290, 420, 560, 700}, CY[6] = {20, 46, 14, 58, 30, 40};
    int n = bio == BI_DUNES ? 3 : 6;
    for (int i = 0; i < n; i++) {
        int x = (int)((CX[i] - w->view_cam / 8 - w->t / 24) % span);
        if (x < 0) x += span;
        spr(i % 2 ? SPR_CLOUD_SMALL : SPR_CLOUD_BIG, L - 48 + x, CY[i], 0, -1);
    }
}

/* ---- the course's things ---------------------------------------------------------------------------------------------- */
/* stage x of world x */
static int sx_of(const world *w, int32_t wx) { return (int)(wx - w->view_cam); }

static void decor(const world *w)
{
    static const uint8_t SPRITE[PR_COUNT] = {0, SPR_FLOWER, SPR_TUFT, SPR_CACTUS, SPR_ROCK, SPR_PINE, SPR_SNOWMAN,
                                             SPR_GLOWSHROOM, SPR_FIREFLY, SPR_LOLLIPOP, SPR_CANDYCANE};
    static const uint8_t FRAMES[PR_COUNT] = {0, 2, 1, 2, 1, 2, 1, 1, 2, 2, 1};
    for (int k = w->hole - 2; k < w->tr.gen_hole; k++) {
        if (k < 0) continue;
        const hole_rec *h = world_hole(w, k);
        for (int i = 0; i < PROPS_PER_HOLE; i++) {
            const prop *p = &h->props[i];
            if (p->kind == PR_NONE) continue;
            int id = SPRITE[p->kind], fr = FRAMES[p->kind];
            const gm_sprite_def *d = &gm_spr[id];
            int x = sx_of(w, p->x) - d->w / 2, y = p->y - d->h + 1;
            if (p->kind == PR_FIREFLY) {
                y = p->y - 14 - ((w->t / 3 + p->var * 9) % 24 < 12 ? (w->t / 3 + p->var * 9) % 12 : 12 - (w->t / 3 + p->var * 9) % 12);
                spr(id + ((w->t / 10) & 1), x + ((w->t / 7 + p->var) % 5) - 2, y, 2, -1);
                continue;
            }
            spr_f(id + (fr > 1 ? p->var % fr : 0), x, y, 2, -1, (p->var & 2) && p->kind != PR_SNOWMAN ? RS_SPR_HFLIP : 0);
        }
    }
}

static void ball_spr(const world *w, int p, int ox)
{
    const ball *b = &w->g[p].b;
    if (b->state == BS_OFF || b->state == BS_SPLASH) return;
    int x = (int)(b->x >> 16) + ox, y = (int)(b->y >> 16);
    if (b->state == BS_OB && (x < -4 || x > 324)) return;
    if (y < 2 && (b->state == BS_FLY)) {
        spr(SPR_ARROW, x - 4, 2, 2, -1);                /* above the screen: an arrow at the top */
        return;
    }
    spr(SPR_BALL, x - 4, y - 7, 2, player_pal(p));
}

/* the gecko: at its ball (aiming right: on the left of it, else flipped), in the pose of the moment */
static void gecko_spr(const world *w, int ox)
{
    int p = w->turn;
    const golfer *g = &w->g[p];
    if (g->out && w->phase != PH_SETTLE) return;
    int ph = w->phase, t = w->phase_t;
    if (ph == PH_PAN || ph == PH_OVER) return;
    int at_spot = ph == PH_FLIGHT || ph == PH_SETTLE || ph == PH_HOLED || ph == PH_SPLASH;
    int32_t bx = at_spot ? g->b.lx : g->b.x, by = at_spot ? g->b.ly : g->b.y;
    int x = (int)(bx >> 16) + ox, y = (int)(by >> 16);
    int left = g->aim > 180;
    int feet_x = left ? x + 14 : x - 14;
    int gy = terrain_y(&w->tr, w->hole_cam + feet_x - ox);
    if (gy - y > 10 || y - gy > 10) gy = y;
    int id;
    switch (ph) {
    case PH_CHARGE: id = SPR_GECKO_BACK + (w->gauge * 3 / (GAUGE_MAX + 1)); break;
    case PH_FLIGHT: id = t < 40 ? SPR_GECKO_SWING : SPR_GECKO_IDLE; break;
    case PH_HOLED: id = SPR_GECKO_CHEER + ((t / 8) & 1); break;
    case PH_SPLASH: id = SPR_GECKO_SAD; break;
    case PH_SETTLE: id = (g->strokes <= 0 && !g->done) ? SPR_GECKO_SAD : SPR_GECKO_IDLE; break;
    default: id = SPR_GECKO_IDLE + ((w->t / 40) % 4 == 3); break;
    }
    int sx = left ? x - 4 : x - 27;
    spr_f(id, sx, gy - 30, 2, player_pal(p), left ? RS_SPR_HFLIP : 0);
    if (w->players > 1 && (ph == PH_AIM || ph == PH_CHARGE))
        spr(SPR_TAG + p, sx + 8, gy - 42 + hu_bob(w->t, 64, 1), 2, player_pal(p));
    /* the power gauge, behind the gecko */
    if (ph == PH_CHARGE) {
        int gxp = left ? sx + 30 : sx - 6, gyp = gy - 34;
        int fill = w->gauge * 30 / GAUGE_MAX;
        for (int i = 0; i * 8 < fill; i++) {
            int rows = fill - i * 8 > 8 ? 8 : fill - i * 8;
            int base = i < 2 ? SPR_FILL : i == 2 ? SPR_FILL_Y : SPR_FILL_R;
            spr(base + rows - 1, gxp, gyp + 31 - i * 8 - 8, 2, -1);
        }
        spr(SPR_GAUGE, gxp, gyp, 2, -1);
    }
}

/* the aim: a few dots along the aim; charging, the first frames of the flight */
static void aim_spr(const world *w, int ox)
{
    if (w->phase != PH_AIM && w->phase != PH_CHARGE) return;
    const golfer *g = &w->g[w->turn];
    if (g->out) return;
    int32_t x = g->b.x + (ox << 16), y = g->b.y - (2 << 16);
    if (w->phase == PH_AIM) {
        int c = icos_hd(g->aim), s = isin_hd(g->aim);
        for (int i = 0; i < 4; i++) {
            int d = 9 + i * 6;
            spr(SPR_DOT + (i == 3), (int)((x + c * d) >> 16) - 4, (int)((y - s * d) >> 16) - 4, 2, -1);
        }
        return;
    }
    int32_t vx, vy;
    shot_velocity(g->aim, w->gauge, &vx, &vy);
    for (int f = 1; f <= ARC_FRAMES; f++) {
        vy += GRAVITY;
        x += vx;
        y += vy;
        int sx = (int)(x >> 16), sy = (int)(y >> 16);
        if (sy >= terrain_y(&w->tr, w->view_cam + sx)) break;
        if (f % ARC_STEP == 0) spr(SPR_DOT + (f > ARC_FRAMES * 2 / 3), sx - 4, sy - 4, 2, -1);
    }
}

static void touch_spr(const touch_view *tv)
{
    if (!tv || !tv->on) return;
    int ox = rs_screen_origin_x();
    int ax = tv->x0 - ox, ay = tv->y0, fx = tv->x - ox, fy = tv->y;
    spr(SPR_RING, ax - 8, ay - 8, 3, -1);
    for (int i = 1; i <= 4; i++) spr(SPR_DOT + 1, ax + (fx - ax) * i / 5 - 4, ay + (fy - ay) * i / 5 - 4, 3, -1);
}

/* the title's player slots: each joined player's gecko, in its colours (hu_title_sprites calls it) */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr(SPR_GECKO_ICON, cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), 2, player_pal(p));
}

/* the results of 2-4 players: the most holes, then the fewest strokes */
static void standing(const world *w, hu_standing *s)
{
    int key[MAX_PLAYERS], val[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) {
        int tot = w->g[p].total < 1023 ? w->g[p].total : 1023;
        key[p] = w->g[p].holes * 1024 + 1023 - tot;
        val[p] = w->g[p].holes;
    }
    hu_rank(s, w->players, key, val);
}

/* the pips' row: player p's strokes left, under (or over) its score */
static int pips_cap(const world *w) { return w->players == 1 ? 20 : 10; }

static void pips(const world *w, int p)
{
    hu_chip c = hu_score_chip(w->players, p, 0);
    int n = w->g[p].strokes, cap = pips_cap(w), per = 10;
    if (n > cap) n = cap;
    int y = c.y >= 200 ? c.y - 12 : c.y + 18;
    for (int i = 0; i < n; i++) {
        int row = i / per, col = i % per, cnt = (n - row * per) < per ? n - row * per : per;
        int x;
        if (c.align < 0) x = c.x - 2 + col * 7;
        else if (c.align > 0) x = c.x - 6 - col * 7;
        else x = c.x - cnt * 7 / 2 + col * 7 - 1;
        spr(SPR_PIP, x, y + row * (c.y >= 200 ? -8 : 8), 3, player_pal(p));
    }
}

/* the messages: the hole's result, water, out of bounds, out of strokes, a new course */
static const char *msg_text(int msg, int n)
{
    static const int BIOMES[BI_COUNT] = {GS_MEADOW, GS_DUNES, GS_SNOW, GS_NIGHT, GS_CANDY};
    switch (msg) {
    case MSG_ACE: return TR(HOLE_IN_ONE);
    case MSG_TWO: return TR(IN_TWO);
    case MSG_HOLED: return TR(IN_THE_HOLE);
    case MSG_WATER: return TR(SPLASH);
    case MSG_OB: return TR(OUT_OF_BOUNDS);
    case MSG_OUT: return TR(NO_STROKES);
    case MSG_BIOME: return rs_tr(&gs_strings, BIOMES[n % BI_COUNT]);
    default: return "";
    }
}

static int msg_on(const world *w)
{
    if (!w->msg) return 0;
    return w->msg_t < (w->msg == MSG_BIOME ? BIOME_FRAMES : MSG_FRAMES);
}

/* the BG screens; PLAY and DEAD are the same screen; in play, the texts are drawn again when one changes */
static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs,
                        const touch_view *tv)
{
    int screen = state == ST_DEAD ? ST_PLAY : state;
    int ui = 0;
    if (screen == ST_PLAY) {
        int hint = w->players == 1 && w->shots == 0 && w->hole == 0;
        ui = 1 + (msg_on(w) ? w->msg * 64 + w->msg_n * 2 + 1024 * (w->msg_p + 1) : 0) + hint * 8192 * (1 + (tv && tv->seen))
             + w->hole * 65536;
    }
    if (screen != shown_state || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best || ui != shown_ui) {
        hu_clear();
        if (state == ST_TITLE) hu_logo("GECKO GOLF", title_ramps, 2, 2, 0);
        if (screen == ST_PLAY) {
            char s[64];
            if (w->players >= 3) hu_score_tags(w->players, 0);     /* P1..P4 by the corner chips */
            if (w->players >= 2) {
                rs_strf(s, sizeof s, TR(HOLE_N), w->hole + 1);
                hu_text(hu_center(s, 0), 1, s);
            }
            if (msg_on(w)) {
                const char *m = msg_text(w->msg, w->msg_n);
                if (hu_width(m, 1) <= 38) hu_big(hu_center(m, 1), 6, m, HU_BIG_FREE);
                else hu_text(hu_center(m, 0), 7, m);
                s[0] = 0;
                if ((w->msg == MSG_ACE || w->msg == MSG_TWO) && w->msg_n) rs_strf(s, sizeof s, TRN(STROKES_BACK, w->msg_n), w->msg_n);
                if (w->msg == MSG_WATER || w->msg == MSG_OB) rs_strf(s, sizeof s, "%s", TR(PENALTY));
                if (s[0]) hu_text(hu_center(s, 0), 9, s);
            }
            if (w->players == 1 && w->shots == 0 && w->hole == 0) {
                const char *h = tv && tv->seen ? TR(HINT_TOUCH) : TR(HINT);
                hu_text(hu_center(h, 0), 28, h);
            }
        }
        if (state == ST_OVER) {
            if (w->players == 1) {
                char s[64];
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->g[0].holes, 0);
                rs_strf(s, sizeof s, TR(ACES_N), w->g[0].aces);
                hu_gameover_note(s);
            } else {
                hu_results_panel(rs, NULL);                                     /* P2 WINS! and the ranking */
            }
        }
        shown_state = screen;
        shown_best = best;
        shown_ui = ui;
    }
    /* the title, every frame: the prompt (blinking), the player slots, BEST, the join line, the credits */
    if (state == ST_TITLE) hu_title_draw(st_t, best);
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, const touch_view *tv)
{
    static const int medals[4] = MEDAL_SCORES;
    hu_standing rs;
    standing(w, &rs);
    blend bl;
    biome_blend(w, &bl);
    palettes(w, &bl);
    if (w->view_cam != drawn_cam || w->tr.rev != drawn_rev || rs_screen_width() != drawn_width) course_build(w);
    int pm = map_w() * 8 - 1;
    rs_bg_scroll(RS_BG2, (int)(w->view_cam & pm), 0);
    rs_bg_scroll(RS_BG3, (int)((w->view_cam / 4) & pm), 0);
    screen_text(w, state, st_t, best, new_best, &rs, tv);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;     /* the panel slides up, ease-out */
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);

    rs_oam_clear();
    int ox = (int)(w->hole_cam - w->view_cam);       /* the balls' stage x are the hole's: the pan moves them */
    /* front to back: the UI, the slingshot, the gecko and the balls, the flag, the decor, the sky */
    if (state == ST_PLAY || state == ST_DEAD) {
        int score[MAX_PLAYERS];
        for (int p = 0; p < w->players; p++) score[p] = w->g[p].holes;
        hu_score_chips(w->players, score, 0);   /* 1: centred; 2: x 80 / 240; 3-4: the corners */
        for (int p = 0; p < w->players; p++) if (!w->g[p].out) pips(w, p);
        if (w->msg == MSG_ACE && msg_on(w))
            for (int i = 0; i < 4; i++) {
                int a = (w->msg_t * 6 + i * 180) % 720;
                hu_sparkle(world_cup_sx(w) + ox + (icos_hd(a) * 14 >> 16) - 4,
                           world_hole(w, w->hole)->cup_y - 12 - (isin_hd(a) * 10 >> 16), (w->msg_t / 4 + i) % 4, 3);
            }
    }
    if (state == ST_OVER) {
        if (w->players == 1) {
            hu_gameover_sprites(1, w->g[0].holes, 0, best, hu_medal_of(w->g[0].holes, medals), st_t, slide);
        } else {
            hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_GECKO_ICON, cx - 8, cy - 8 + slide, 3, player_pal(rs.order[i]));
            }
        }
    }
    if (state == ST_TITLE) {
        hu_title_sprites(st_t, slot_icon, NULL);   /* the prompt's glyph, the joined players popping in */
        /* the gecko bobs under the logo */
        spr(SPR_GECKO_IDLE + (st_t / 32) % 2, RS_SCREEN_W / 2 - 16, 92 + hu_bob(st_t, 64, READY_BOB), 2, -1);
    }
    if (state == ST_PLAY || state == ST_DEAD) {
        touch_spr(tv);
        aim_spr(w, ox);
        gecko_spr(w, ox);
    }
    /* the balls: the one in play in front */
    {
        ball_spr(w, w->turn, ox);
        for (int p = w->players - 1; p >= 0; p--)
            if (p != w->turn) ball_spr(w, p, ox);
    }
    /* effects: the splash, the sand puff */
    if (w->phase == PH_SPLASH && w->g[w->turn].b.state == BS_SPLASH && w->phase_t < 24) {
        const ball *b = &w->g[w->turn].b;
        spr(SPR_SPLASH + w->phase_t / 8, (int)(b->x >> 16) + ox - 8, (int)(b->y >> 16) - 14, 2, -1);
    }
    if ((w->events[w->turn] & EV_BOUNCE) && w->bounce_surf == SF_SAND) {
        fx_t = 0;
        fx_x = (int)(w->g[w->turn].b.x >> 16) + ox;
        fx_y = (int)(w->g[w->turn].b.y >> 16);
    }
    if (fx_t < 12) {
        spr(SPR_PUFF + fx_t / 4, fx_x - 4, fx_y - 6, 2, -1);
        fx_t++;
    }
    /* the flag: in the cup, waving; the out of bounds stakes at the stage's edges */
    if (state != ST_TITLE) {
        const hole_rec *h = world_hole(w, w->hole);
        int cx = sx_of(w, h->cup_x);
        spr(SPR_FLAG + ((w->t / 16) & 1), cx - 1, h->cup_y - 31, 2, -1);
        spr(SPR_STAKE, ox - 4 + OB_LEFT - 2, terrain_y(&w->tr, w->hole_cam) - 15, 2, -1);
        spr(SPR_STAKE, ox + OB_RIGHT - 4 + 2, terrain_y(&w->tr, w->hole_cam + RS_SCREEN_W - 1) - 15, 2, -1);
    }
    decor(w);
    sky_sprites(w, &bl);
}

/* ---- save states (main.c) ---- */
/* a state saved at another width brings maps of another size: set them up for this width, redraw the UI; the course's
 * tiles are drawn again at the next frame (the same as they were) */
void draw_state_loaded(void)
{
    if (rs_bg_map_w(RS_BG2) != map_w()) maps_setup();     /* (it draws the UI again: shown_state) */
    drawn_cam = 0x7fffffff;                                /* the course's tiles: drawn again, the same */
    sky_key = -1;
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(line_col); S(shown_state); S(shown_best); S(shown_ui); S(fx_t); S(fx_x); S(fx_y);
    RS_STATE_RASTER(raster);
}
#undef S
