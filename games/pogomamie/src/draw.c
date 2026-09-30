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
static uint32_t bg2_key[32][64];                /* the world cell each BG2 map cell holds (+1; 0 = none) */
static int32_t far_col[64];                     /* the BG4 world column each BG4 map column holds */
static int16_t bg3_row[32];                     /* the mid-ground row each BG3 map row holds */
static int far_slot[2];                         /* the district whose tiles are in each BG4 VRAM slot */
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
    static const int mid_mix[5] = {40, 55, 70, 85, 0}, far_mix[6] = {18, 30, 42, 54, -40, 30};
    for (int i = 0; i < 5; i++) {
        int k = mid_mix[i];
        rs_color c = rgb(hor[0] + (ink[0] - hor[0]) * k / 100, hor[1] + (ink[1] - hor[1]) * k / 100,
                         hor[2] + (ink[2] - hor[2]) * k / 100);
        if (i == C_MWIN - 1) c = night ? rgb(250, 214, 120) : rgb(hor[0], hor[1], hor[2]);
        rs_pal_set(RS_PAL_BG(PAL_MID) + i + 1, c);
    }
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

static void raster(int line, void *user)
{
    (void)user;
    rs_pal_set(0, test_backdrop ? RS_RGB(31, 0, 31) : line_col[line]);
}

static void look_update(const world *w, int snap)
{
    int xc = world_camx(w) + RS_SCREEN_W / 2;
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

static void stream_bg2(const world *w, int camx, int camy)
{
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
            for (int r = 0; r < 32; r++) bg2_key[r][(x / 8) & 63] = 0;
    }
    int tx0 = camx >> 3, ty0 = camy >> 3;
    for (int tx = tx0; tx <= tx0 + RS_SCREEN_W / 8; tx++) {
        const bldg *b = world_bldg_at(w, tx * 8);
        for (int ty = ty0; ty <= ty0 + RS_SCREEN_H / 8 && ty < WORLD_H / 8; ty++) {
            uint32_t key = ((uint32_t)tx << 6 | (uint32_t)ty) + 1;
            uint32_t *k = &bg2_key[ty & 31][tx & 63];
            if (*k == key) continue;
            *k = key;
            rs_bg_put(RS_BG2, tx & 63, ty & 31, world_tile(w, b, tx * 8, ty * 8));
        }
    }
}

/* ---- the mid-ground: a virtual 64-row layer (sky, the roofs panorama, then its last row repeated: the city below)
 * scrolling at half the camera's speed, its rows streamed into the 32-row map as they come into view ------------------ */
#define MID_TOP_ROWS (MID_Y + 16)               /* the virtual layer's sky rows above the roofs (room for the drop) */
static int mid_drop;                            /* px the mid-ground sinks by (the Eiffel Tower district: the tower shows) */

static void stream_bg3(int s3)
{
    for (int v = s3 / 8; v <= s3 / 8 + RS_SCREEN_H / 8; v++) {
        if (bg3_row[v & 31] == v) continue;
        bg3_row[v & 31] = (int16_t)v;
        int r = v - MID_TOP_ROWS;
        for (int x = 0; x < MID_W; x++)
            rs_bg_put(RS_BG3, x, v & 31, r < 0 ? 0 : pm_mid_map[(r < MID_H ? r : MID_H - 1) * MID_W + x]);
    }
}

/* ---- the backdrop: one panorama per district, 64 BG4 columns each ------------------------------------------------ */
static void stream_bg4(int camx)
{
    int c0 = camx / 64;
    for (int32_t C = c0; C <= c0 + RS_SCREEN_W / 8 + 1; C++) {
        if (far_col[C & 63] == C) continue;
        int d = (int)((C / 64) % 4), slot = d & 1;
        if (far_slot[slot] != d) {
            rs_tiles_load(VR_BG4 + slot * FAR_SLOT, pm_far_tiles[d], pm_far_tile_count[d]);
            far_slot[slot] = d;
        }
        for (int r = 0; r < FAR_H; r++) rs_bg_put(RS_BG4, (int)(C & 63), FAR_Y + r, pm_far_map[d][r * FAR_W + (C & 63)]);
        far_col[C & 63] = C;
    }
}

/* ---- set-up ---------------------------------------------------------------------------------------------------------- */
void draw_invalidate(void)
{
    memset(bg2_key, 0, sizeof bg2_key);
    for (int i = 0; i < 64; i++) far_col[i] = -1;
    for (int i = 0; i < 32; i++) bg3_row[i] = -1;
    sky_redrawn_n = 0;
}

void draw_init(void)
{
    for (int p = 1; p < 8; p++)
        if (p != PAL_LOGO) rs_pal_load(RS_PAL_BG(p), pm_bg_pal[p], 16);    /* 0 and 5: the house kit's */
    rs_pal_load(RS_PAL_OBJ(0), pm_obj_pals, 128);
    rs_bg_setup(RS_BG1, 64, 32, VR_BG1);
    rs_bg_setup(RS_BG2, 64, 32, VR_BG2);
    rs_bg_setup(RS_BG3, 64, 32, VR_BG3);
    rs_bg_setup(RS_BG4, 64, 32, VR_BG4);
    rs_tiles_load(VR_BG2, pm_bg2_tiles, T_COUNT);
    rs_tiles_load(VR_BG3, pm_mid_tiles, pm_mid_tile_count);
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, pm_obj_tiles, pm_obj_tile_count);
    rs_raster(raster, NULL);
    rs_rng_seed(&fx_rng, 0x9090a1u);
    far_slot[0] = far_slot[1] = -1;
    draw_invalidate();
    memset(shown_look, 0xff, sizeof shown_look);
}

/* ---- cosmetic effects ------------------------------------------------------------------------------------------------ */
typedef struct fx_obj { int kind, t, life, frame, n; int32_t x, y, vx, vy; } fx_obj;
enum { FX_NONE, FX_DUST, FX_SHARD, FX_DEBRIS, FX_STAR, FX_POP, FX_SPLASH, FX_FEATHER };
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
    if (ev & EV_PIGEON) burst(FX_FEATHER, x, y, 4, 16, 12);
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

static void spr_at(int id, int sx, int sy, int pal, int flags)
{
    const pm_sprite_def *d = &pm_spr[id];
    if (sx <= -(int)d->w || sx >= RS_SCREEN_W || sy <= -(int)d->h || sy >= RS_SCREEN_H) return;
    rs_spr(sx, sy, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, 2, flags);
}

static void spr_w(int id, int wx, int wy, int flags) { spr_at(id, wx - camx_i, wy - camy_i, -1, flags); }

static int mamie_frame(const mamie *m, int t)
{
    switch (m->state) {
    case MS_SLING: return MF_SQUASH2;
    case MS_REEL: return MF_HANG;
    case MS_FALL: return (t / 6) & 1 ? MF_FLAIL2 : MF_FLAIL1;
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
    if (p == 1) {                                   /* Papi's beret over the headscarf */
        int hx = pm_papi_head[f][0], hy = pm_papi_head[f][1];
        spr_at(SPR_PAPI_HEAD, flip ? sx + 24 - hx - 16 : sx + hx, sy + hy, 1, flip);
    }
    if (m->umbrella_t > 0 && m->state == MS_AIR) {
        int blink = m->umbrella_t < 60 && (m->umbrella_t / 4) % 2;
        if (!blink) spr_at(SPR_UMBRELLA + ((w->t / 16) & 1), sx, sy - 12, -1, 0);
    }
    spr_at(SPR_MAMIE + f, sx, sy, p == 1 ? 1 : 0, flip);
    if (sy + 8 < 0 && m->state == MS_AIR) spr_at(SPR_ARROW, sx + 8, 2, -1, 0);       /* above the screen */
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
            int cx = (int)o->x + (o->pos >> 16);
            int f = o->state ? 3 + (t / 4) % 2 : (o->t / 50) % 3 == 2 ? ((o->t / 10) % 2 ? 2 : 0) : (o->t / 8) % 2;
            spr_w(SPR_PIGEON + f, cx - 8, o->y - 16, o->dir < 0 ? RS_SPR_HFLIP : 0);
            break;
        }
        case OB_ANTENNA: {
            int f = o->t > 0 ? 1 + (o->t / 4) % 2 : 0;
            spr_w(SPR_ANTENNA + f, (int)o->x - 8, o->y - 32, 0);
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
            for (int k = 0; k < (o->w + 7) / 8; k++) {
                int x0 = (int)o->x + k * 8, y0 = obj_line_y(o, x0), y1 = obj_line_y(o, x0 + 8);
                int s = clampi(y1 - y0, -2, 2);
                spr_w(SPR_LINE + (2 - s), x0, y0 - 3, 0);
                if (k % 2 == 1 && k * 8 + 8 < o->w) spr_w(SPR_CLOTHES + (k / 2 + i) % 4, x0, y0 + 1, 0);
            }
            break;
        case OB_ITEM: {
            int bob = ((t / 8) % 4 == 1) - ((t / 8) % 4 == 3);
            spr_w(SPR_ITEM + o->var, (int)o->x, o->y - 16 + bob, 0);
            spr_w(SPR_GLOW + (t / 12) % 2, (int)o->x, o->y - 16 + bob, 0);
            break;
        }
        case OB_GUST: {
            int x0 = (int)o->x > camx_i ? (int)o->x : camx_i, x1 = o->x + o->w < camx_i + RS_SCREEN_W ? (int)(o->x + o->w)
                                                                                                        : camx_i + RS_SCREEN_W;
            if (x1 - x0 <= 24) break;
            for (int k = 0; k < 6; k++) {
                int span = x1 - x0 - 16;
                int ph = (((t * 3 * (o->dir < 0 ? -1 : 1) + k * 57) % span) + span) % span;
                spr_w(SPR_WIND + (k & 1), x0 + ph, camy_i + 24 + k * 31 + (k * 13 + t / 3) % 9, o->dir < 0 ? RS_SPR_HFLIP : 0);
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
        case FX_FEATHER: spr_at(SPR_DUST, x, y, -1, 0); break;
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
    look_update(w, 1);
}

void draw_frame(const world *w, int state, int st_t, int paused)
{
    /* a crash shakes the playfield and the world's sprites (never the UI): house_ui.c hu_shake */
    int camx = world_camx(w) + hu_shake_x(), camy = world_camy(w) + hu_shake_y();
    if (camx < 0) camx = 0;
    if (camy < 0) camy = 0;
    camx_i = camx;
    camy_i = camy;
    look_update(w, 0);
    sky_lines(camy);
    stream_bg2(w, camx, camy);
    stream_bg4(camx);
    scroll_x = camx & 511;
    scroll_y = camy & 255;
    rs_bg_scroll(RS_BG2, scroll_x, scroll_y);
    int xc = camx + RS_SCREEN_W / 2, drop_to = district_at(xc) == 3 && !night_at(xc) ? 56 : 0;
    mid_drop += mid_drop < drop_to ? 1 : mid_drop > drop_to ? -1 : 0;
    int s3 = camy / 2 + 128 - mid_drop;
    stream_bg3(s3);
    rs_bg_scroll(RS_BG3, (camx / 2) & 511, s3 & 255);
    rs_bg_scroll(RS_BG4, (camx / 8) & 511, 40 + camy / 4);
    (void)paused;
    /* sprites, front to back: the UI (ui.c), the players, effects, the cat, things */
    rs_oam_clear();
    ui_sprites(w, state, st_t, 0);
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
    S(sky_redrawn_n); S(test_backdrop); S(scroll_x); S(scroll_y); S(night_blink); S(fx_rng); S(fx); S(camx_i); S(camy_i);
    RS_STATE_RASTER(raster);
}
#undef S
