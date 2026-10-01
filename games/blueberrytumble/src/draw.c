/*
 * Blueberry Tumble: video. Nothing here changes the game.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 *
 * Layers (docs/art-direction.md): the backdrop is the sky (a raster gradient per biome and time of day, pulsing
 * on the beat); BG4 the far mountains (1/4); BG3 the biome's mid-ground (1/2); BG2 the playfield, an AFFINE layer
 * sheared downward so the flat physics grid is drawn as a mountain slope; BG1 the house UI kit. On top of the shear,
 * the slope has a PROFILE (the course's dy0/dy1, director.c): each column of the map is drawn that many px lower
 * (whole tile rows), and the columns where the height changes are drawn with ramp strips (gentle, steep, a cliff),
 * so the mountainside has steeper and gentler stretches, rises, dips, cliff edges and plateaus. The physics stays on
 * the flat grid: every sprite is drawn with the shear AND the profile at its x, and the camera follows the profile
 * under player 1. The playfield and the mid-ground are streamed column by column.
 * Palettes: BG 0 UI, 1-4 the playfield (one biome each: day, or night in the loop), 5 far, 6-7 mid-ground (by the
 * biome's parity); OBJ 0, 1, 4, 5 the four berries (blueberry, raspberry, blackberry, gooseberry: the kit's player
 * palettes), 2 dew and mushrooms, 3 the kit, 6 cones and leaves, 7 the snowball and the golden blueberries.
 */
#include "bt.h"
#include "assets.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>

#define MAP_W 64                            /* the playfield map: 64 x 64 tiles, wrapping */
#define GROUND_TROW 44                      /* the ground surface's tile row (map px 352) */
#define GROUND_PY (GROUND_TROW * 8)
#define MID_TROW 34                         /* the mid-ground band's first tile row on BG3 (64 rows: no wrap) */
#define FAR_TROW 12                         /* the far band's first tile row on BG4 */

static const int slope256[NBIOMES] = BIOME_SLOPE;
/* the sky per biome and time of day (top, horizon), then the night */
static const uint32_t sky[NBIOMES + 1][2] = {
    {0x5a8ce0, 0xf8c8c0},       /* summit, morning: a pink dawn */
    {0x3c8ce8, 0xc4e6fa},       /* forest, midday */
    {0x4c94dc, 0xffe0a0},       /* meadows, golden afternoon */
    {0x383c8c, 0xf8a060},       /* village, sunset */
    {0x0a0e2c, 0x2c3470},       /* the night loop */
};
/* the title logo: BLUEBERRY in the berry's blues, TUMBLE in leaf greens (3-shade ramps, light to dark) */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(170, 188, 255), RS_RGB8(92, 110, 232), RS_RGB8(50, 54, 164)},
    {RS_RGB8(206, 232, 120), RS_RGB8(128, 184, 72), RS_RGB8(66, 124, 44)}};

/* ---- the drawn state (saved in states: draw_state) --------------------------------------------------------------- */
static rs_color line_col[RS_SCREEN_H];
static int32_t pf_col_lo, pf_col_hi;        /* playfield: the columns [pf_col_lo, pf_col_hi) are on the map */
static uint16_t pf_gone[32];                /* the smashed bits each drawn column was drawn with */
static int32_t mid_col_hi;                  /* BG3 tile columns written */
static int pal_loop[8];                     /* the loop (+1) each playfield palette slot was loaded for */
static int pal_biome[8];                    /* the biome and loop each mid-ground slot was loaded for */
static int cam_up;                          /* px the camera has risen (pads, glides) */
static int shown_state = -1, shown_best = -1, shown_players = -1, logo_on;
static int attempt_on, attempt_x0;          /* "ATTEMPT n" drawn in the world, the camera x it was drawn at */
static int gate_t = 999, gate_biome;        /* the biome name shown after a gate */
static int32_t last_gate_col = -1;
typedef struct fx { int16_t x, y, vx, vy, t, kind; } fx;   /* screen x, y in 1/16 px (juice, snow bits) */
#define FX_MAX 24
static fx fxs[FX_MAX];
static int fx_n;
static int view_hofs, view_vofs, view_c, view_camx;    /* the registers written this frame (the bot reads them) */
static int dew_t[MAX_PLAYERS] = {99, 99, 99, 99};       /* frames since each player's last dew-drop jump (the ring) */
static int32_t dew_id[MAX_PLAYERS];
static int cam_d16;                         /* the camera follows the profile under player 1 (px x 16) */
static const bt_course *view_course;        /* the course drawn this frame (the sprites' profile) */

/* ---- colour helpers ------------------------------------------------------------------------------------------------- */
static rs_color rgb(uint32_t h) { return RS_HEX(h); }

static rs_color night_of(rs_color c)
{
    int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    r = r * 45 / 100 + 1;
    g = g * 50 / 100 + 2;
    b = b * 62 / 100 + 5;
    return RS_RGB(r > 31 ? 31 : r, g > 31 ? 31 : g, b > 31 ? 31 : b);
}

/* the playfield and mid-ground palettes: each slot loaded for the biome (and loop) it shows next */
static void load_slot_palettes(int32_t cam_x)
{
    /* the playfield shows the biomes from the camera's left edge on; the mid-ground (parallax 1/2) those of the
     * world x from cam_x - 320 to cam_x + 320 (stream_mid) */
    int32_t b0 = cam_x < 0 ? 0 : cam_x / (BIOME_CELLS * CELL);
    int32_t bl = cam_x - RS_SCREEN_W < 0 ? 0 : (cam_x - RS_SCREEN_W) / (BIOME_CELLS * CELL);
    for (int s = 0; s < NBIOMES; s++) {
        int32_t bi = b0 + ((s - (int)(b0 % NBIOMES)) + NBIOMES) % NBIOMES;
        int loop = (int)(bi / NBIOMES);
        if (pal_loop[PAL_PF + s] == loop + 1) continue;
        pal_loop[PAL_PF + s] = loop + 1;
        for (int i = 1; i < 16; i++) {
            rs_color c = gm_pf_pal[s * 16 + i];
            rs_pal_set(RS_PAL_BG(PAL_PF + s) + i, loop ? night_of(c) : c);
        }
    }
    for (int s = 0; s < 2; s++) {
        int32_t bi = bl + ((bl & 1) == s ? 0 : 1);
        int loop = (int)(bi / NBIOMES), b = (int)(bi % NBIOMES);
        int key = loop * 16 + b + 1;
        if (pal_biome[PAL_MID + s] == key) continue;
        pal_biome[PAL_MID + s] = key;
        for (int i = 1; i < 16; i++) {
            rs_color c = gm_mid_pal[b * 16 + i];
            rs_pal_set(RS_PAL_BG(PAL_MID + s) + i, loop ? night_of(c) : c);
        }
    }
}

/* the sky and the far mountains at the camera: the biome's colours, cross-faded over 512 px after a gate */
static void sky_and_far(int32_t cam_x, int pulse)
{
    int32_t mid = cam_x + RS_SCREEN_W / 2;
    int32_t bi = mid < 0 ? 0 : mid / (BIOME_CELLS * CELL);
    int32_t into = mid - bi * BIOME_CELLS * CELL;
    int cur = bi >= NBIOMES ? NBIOMES : (int)bi;
    int prev = bi == 0 ? cur : (bi - 1 >= NBIOMES ? NBIOMES : (int)(bi - 1));
    int k = into < 512 ? (int)(into / 2) : 256;
    rs_color top = hu_lerp_color(rgb(sky[prev][0]), rgb(sky[cur][0]), k);
    rs_color hor = hu_lerp_color(rgb(sky[prev][1]), rgb(sky[cur][1]), k);
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int t = y < 176 ? y * 256 / 176 : 256;
        rs_color c = hu_lerp_color(top, hor, t);
        int r = (c & 31) * 8, g = ((c >> 5) & 31) * 8, b = ((c >> 10) & 31) * 8;
        /* the beat pulse, brighter at the top; the house 2-line dither step every 8 lines */
        int p = pulse * (256 - t / 2) / 256;
        r += p + ((y & 4) ? 4 : 0);
        g += p + ((y & 4) ? 2 : 0);
        b += p + ((y & 4) ? 4 : 0);
        line_col[y] = RS_RGB8(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
    }
    /* the far mountains take the haze of the horizon (aerial perspective), darker at night */
    for (int i = 1; i < 16; i++) {
        rs_color c = gm_far_pal[i];
        c = hu_lerp_color(c, hor, 90);
        if (cur == NBIOMES) c = hu_lerp_color(night_of(c), top, 40);
        rs_pal_set(RS_PAL_BG(PAL_FAR) + i, c);
    }
}

static void raster(int line, void *user)
{
    (void)user;
    rs_pal_set(0, line_col[line]);
}

/* ---- set-up ------------------------------------------------------------------------------------------------------------ */
void draw_init(void)
{
    hu_config hc = hu_defaults();
    hc.logo_tile = LOGO_TILE;
    hc.logo_pal = PAL_LOGO;
    hc.obj_tile = KIT_OBJ_TILE;
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hc.box_glyphs = " SCOREBESTNWMDALPYIG0123456789!:-+";   /* only the glyphs the panel uses (VRAM) */
    hu_init(&hc);
    rs_bg_setup(RS_BG1, 64, 32, VR_BG1);
    rs_bg_setup(RS_BG2, MAP_W, 64, VR_BG2);
    rs_bg_setup(RS_BG3, 64, 64, VR_BG3);
    rs_bg_setup(RS_BG4, 64, 32, VR_BG4);
    rs_tiles_load(VR_BG2, gm_pf_tiles, gm_pf_tile_count);
    rs_tiles_load(VR_BG3, gm_mid_tiles, gm_mid_tile_count);
    rs_tiles_load(VR_BG4, gm_far_tiles, gm_far_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_FAR), gm_far_pal, 16);
    /* the far mountains: a static 32-tile panorama twice across the 64-tile map, its bottom row down to the end */
    for (int y = FAR_TROW; y < 32; y++)
        for (int x = 0; x < 64; x++) {
            int yy = y - FAR_TROW < FAR_H ? y - FAR_TROW : FAR_H - 1;
            rs_bg_put(RS_BG4, x, y, gm_far_map[yy * FAR_W + (x % FAR_W)]);
        }
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    for (int p = 0; p < 8; p++)
        if (p != 3) rs_pal_load(RS_PAL_OBJ(p), gm_obj_pals + p * 16, 16);
    memset(pal_loop, 0, sizeof pal_loop);
    memset(pal_biome, 0, sizeof pal_biome);
    pf_col_lo = pf_col_hi = START_COL;
    mid_col_hi = -1000;
    rs_raster(raster, NULL);
}

void draw_reset(void)
{
    /* a new run: the course is new, redraw the playfield and forget the effects */
    pf_col_lo = pf_col_hi = START_COL - 100;
    fx_n = 0;
    last_gate_col = -1;
    gate_t = 999;
    for (int p = 0; p < MAX_PLAYERS; p++) dew_t[p] = 99;
    cam_up = 0;
    cam_d16 = 0;
}

/* ---- the playfield map ------------------------------------------------------------------------------------------------ */
static uint32_t hash32(uint32_t v)
{
    v ^= v >> 16;
    v *= 0x7feb352du;
    v ^= v >> 15;
    v *= 0x846ca68bu;
    return v ^ (v >> 16);
}

static void put_meta(int32_t col, int trow, int biome, int mt)
{
    const uint16_t *m = &gm_pf_meta[(biome * MT_COUNT + mt) * 4];
    int tx = (int)((uint32_t)(col * 2) & (MAP_W - 1));
    for (int q = 0; q < 4; q++) rs_bg_put(RS_BG2, (tx + (q & 1)) & (MAP_W - 1), (trow + (q >> 1)) & 63, m[q]);
}

static void clear_cell(int32_t col, int trow)
{
    int tx = (int)((uint32_t)(col * 2) & (MAP_W - 1));
    for (int q = 0; q < 4; q++) rs_bg_put(RS_BG2, (tx + (q & 1)) & (MAP_W - 1), (trow + (q >> 1)) & 63, 0);
}

static int is_log(const bt_col *k, int r) { return r >= 0 && r < ROWS && k->cell[r] == K_LOG; }

/* one 8x8 quarter of a metatile at map tile (tx, ty) */
static void put_quarter(int32_t col, int half, int ty, int biome, int mt, int q)
{
    int tx = (int)((uint32_t)(col * 2 + half) & (MAP_W - 1));
    rs_bg_put(RS_BG2, tx, ty & 63, mt < 0 ? 0 : gm_pf_meta[(biome * MT_COUNT + mt) * 4 + q]);
}

static int floor8(int v) { return v >= 0 ? v / 8 : -((-v + 7) / 8); }

/* a column whose ground changes height: its ramp strip, the deep soil under it, nothing above */
static int draw_ramp(int32_t col, int b, int dy0, int dy1)
{
    int base = floor8(dy0) * 8, s = dy0 - base, d = dy1 - dy0, i = 0;
    while (i < RAMP_COUNT && !(gm_ramp[i][0] == s && gm_ramp[i][1] == d)) i++;
    if (i == RAMP_COUNT) return 0;                  /* not a ramp the art has: drawn level */
    int top = GROUND_TROW + base / 8 + gm_ramp[i][2];
    for (int r = -38; r < 64 - 38; r++) {           /* the whole column: the sky, the strip, the soil (64 rows) */
        int ty = top + r;
        for (int h = 0; h < 2; h++) {
            int tx = (int)((uint32_t)(col * 2 + h) & (MAP_W - 1));
            uint16_t e = 0;
            if (r >= 0 && r < RAMP_ROWS) e = gm_ramp_map[((b * RAMP_COUNT + i) * RAMP_ROWS + r) * 2 + h];
            else if (r >= RAMP_ROWS) e = gm_pf_meta[(b * MT_COUNT + MT_DEEP) * 4 + ((ty & 1) << 1) + h];
            rs_bg_put(RS_BG2, tx, ty & 63, e);
        }
    }
    return 1;
}

static void draw_column(const bt_course *c, int32_t col)
{
    const bt_col *k = course_col(c, col), *kl = course_col(c, col - 1), *kr = course_col(c, col + 1);
    int b = course_biome_of_col(col);
    /* the scenery varies with the run (the start, seen on the title, stays the same) */
    uint32_t hsh = hash32((uint32_t)col * 2654435761u ^ (col >= START_CELLS ? c->dir.seed : 0));
    if (k->dy0 != k->dy1 && draw_ramp(col, b, k->dy0, k->dy1)) return;
    /* the column drawn k->dy0 px lower (whole tile rows), its 64 map rows: 20 of sky, 24 of cells, the surface (2),
     * the soil (18) */
    int g = GROUND_TROW + floor8(k->dy0);
    for (int ty = g - 2 * ROWS - 20; ty < g - 2 * ROWS; ty++)
        for (int h = 0; h < 2; h++) put_quarter(col, h, ty, b, -1, 0);
    for (int r = 0; r < ROWS; r++) {
        int trow = g - 2 * (r + 1), mt = -1;
        int kind = (k->gone >> r) & 1 ? K_EMPTY : k->cell[r];
        switch (kind) {
        case K_BLOCK: mt = (r + 1 < ROWS && k->cell[r + 1] == K_BLOCK) ? MT_ROCK : MT_ROCK_TOP; break;
        case K_LOG: {
            int left = kl->cell[r] == K_LOG, right = kr->cell[r] == K_LOG;
            if ((is_log(k, r - 1) || is_log(k, r + 1)) && !left && !right) mt = MT_LOG_V;
            else mt = left && right ? MT_LOG_M : left ? MT_LOG_R : right ? MT_LOG_L : MT_LOG_1;
            break;
        }
        case K_THORN: mt = MT_THORN; break;
        case K_HANG: mt = MT_HANG; break;
        case K_PEBBLE: mt = MT_PEBBLE; break;
        default:
            if (r == 0 && (k->flags & F_GATE)) mt = MT_SIGN;
            else if (r == 0 && k->ground == G_GROUND && k->cell[0] == K_EMPTY && (hsh & 7) < 2)
                mt = MT_DECO0 + (int)((hsh >> 3) % 3);
            break;
        }
        if (mt >= 0) put_meta(col, trow, b, mt);
        else clear_cell(col, trow);
    }
    int gap = k->ground == G_GAP, gl = kl->ground == G_GAP, gr = kr->ground == G_GAP;
    int surf;
    if (gap) surf = MT_VOID_TOP;
    else if (k->ground == G_ICE) surf = MT_ICE;
    else if (k->ground == G_SNOW) surf = MT_SNOW;
    else if (k->ground == G_WATER) surf = MT_WATER;
    else if (gr) surf = MT_EDGE_L;
    else if (gl) surf = MT_EDGE_R;
    else surf = MT_SURF0 + (int)((hsh >> 8) % 3);
    put_meta(col, g, b, surf);
    for (int d = 1; d <= 9; d++) {
        int mt = gap ? MT_VOID : gr ? MT_WALL_L : gl ? MT_WALL_R : d <= 2 ? MT_DIRT0 + (int)((hsh >> (12 + d)) & 1) : MT_DEEP;
        put_meta(col, g + 2 * d, b, mt);
    }
}

static void stream_playfield(const bt_course *c, int32_t cam_col)
{
    int32_t want_lo = cam_col - 2, want_hi = cam_col + 23;
    if (pf_col_hi < want_lo || pf_col_hi > want_hi + 1 || want_lo < pf_col_lo) pf_col_hi = want_lo;
    pf_col_lo = want_lo;
    while (pf_col_hi < want_hi) {
        draw_column(c, pf_col_hi);
        pf_gone[(uint32_t)pf_col_hi & 31] = course_col(c, pf_col_hi)->gone;
        pf_col_hi++;
    }
    for (int32_t col = want_lo; col < want_hi; col++) {
        uint16_t g = course_col(c, col)->gone;
        if (pf_gone[(uint32_t)col & 31] != g) {
            pf_gone[(uint32_t)col & 31] = g;
            draw_column(c, col);
        }
    }
}

/* the mid-ground: BG3 tile column u shows the biome whose ground enters the screen with it */
static void stream_mid(int32_t cam_x)
{
    int32_t u_hi = (cam_x / 2 + RS_SCREEN_W) / 8 + 2;
    if (mid_col_hi < u_hi - 60 || mid_col_hi > u_hi) mid_col_hi = u_hi - 44;
    while (mid_col_hi < u_hi) {
        int32_t u = mid_col_hi;
        int32_t wx = u * 16 - RS_SCREEN_W;
        int b = course_biome_of_col(wx < 0 ? 0 : wx / CELL);
        int mx = (int)(((u % MID_W) + MID_W) % MID_W);
        int32_t rep = u >= 0 ? u / MID_W : (u - MID_W + 1) / MID_W;
        int band = b == 2 && rep % 3 != 0 ? 4 : b;  /* the berry family: on one meadow repeat in three */
        for (int y = MID_TROW; y < 64; y++) {
            int yy = y - MID_TROW < MID_H ? y - MID_TROW : MID_H - 1;
            uint16_t e = gm_mid_map[(band * MID_H + yy) * MID_W + mx];
            e = (uint16_t)((e & ~(7 << 10)) | ((PAL_MID + (b & 1)) << 10));
            rs_bg_put(RS_BG3, (int)((uint32_t)u & 63), y, e);
        }
        mid_col_hi++;
    }
}

/* ---- sprites ------------------------------------------------------------------------------------------------------------ */
static void spr(int id, int x, int y, int prio, int pal)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x <= -(int)d->w || x >= RS_SCREEN_W || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, 0);
}

/* the screen y of a world point (height h px above the ground) at screen column sx: the playfield's shear there */
static int screen_y(int h, int sx)
{
    int d = view_course ? course_profile(view_course, sx + view_camx) : 0;
    return GROUND_PY + d - h - view_vofs - ((view_c * (sx - PIVOT_X)) >> 8);
}

static int berry_frame(const berry *b)
{
    if (b->mode != M_GLIDE) {
        if (b->land_t < 4) return SPR_BERRY_SQUASH + ((b->angle + 8192) >> 14) % 4;
        if (!b->grounded && b->air_t < 3 && b->vy > 0) return SPR_BERRY_STRETCH + ((b->angle + 8192) >> 14) % 4;
    }
    return SPR_BERRY + (b->angle >> 12);
}

static void draw_berry(const world *w, int p, int state, int st_t)
{
    const berry *b = &w->b[p];
    int sx = (int)(b->x >> 16) - view_camx, h = b->h >> 16, pal = hu_player_pal(p);
    int bob = state == ST_READY || state == ST_TITLE ? hu_bob(st_t + p * 16, 64, 2) + 2 : 0;    /* waiting: it bobs */
    if (b->dead) {
        int dt = w->t - w->dead_f[p];
        if (dt <= HIT_FREEZE) spr(SPR_BERRY_HIT, sx - 8, screen_y(h, sx) - 15, 2, pal);
        else if (b->cause != D_FALL) spr(SPR_SPLAT + (dt < 14 ? 0 : dt < 24 ? 1 : 2), sx - 8, screen_y(h > 0 ? h : 0, sx) - 15, 2, pal);
        return;
    }
    if (b->mode == M_SNOW) {
        spr(SPR_SNOWPATCH + (b->angle >> 14) % 4, sx - 12, screen_y(h, sx) - 23, 2, pal);   /* in front: the berry inside */
        spr(SPR_SNOWBERRY + (b->angle >> 14) % 4, sx - 12, screen_y(h, sx) - 23, 2, -1);
        return;
    }
    if (b->mode == M_GLIDE) {
        int lf = b->vy > Q16_ONE ? 0 : b->vy < -Q16_ONE ? 2 : 1;
        spr(SPR_BERRY, sx - 8, screen_y(h, sx) - 15 + 2, 2, pal);
        spr(SPR_LEAF + lf, sx - 16, screen_y(h, sx) - 8, 2, -1);
        return;
    }
    spr(berry_frame(b), sx - 8, screen_y(h, sx) - 15 - bob, 2, pal);
}

static void fx_add(int x, int y, int vx, int vy, int kind)
{
    if (fx_n >= FX_MAX) {
        memmove(fxs, fxs + 1, sizeof fxs[0] * (FX_MAX - 1));
        fx_n--;
    }
    fx f = {(int16_t)(x * 16), (int16_t)(y * 16), (int16_t)vx, (int16_t)vy, 0, (int16_t)kind};
    fxs[fx_n++] = f;
}

static void fx_update(int dx)
{
    int n = 0;
    for (int i = 0; i < fx_n; i++) {
        fx *f = &fxs[i];
        f->x = (int16_t)(f->x + f->vx - dx * 16);
        f->y = (int16_t)(f->y + f->vy);
        f->vy = (int16_t)(f->vy + 3);
        f->t++;
        if (f->t < 40 && f->y < RS_SCREEN_H * 16) fxs[n++] = *f;
    }
    fx_n = n;
}

static void fx_draw(void)
{
    for (int i = 0; i < fx_n; i++) {
        const fx *f = &fxs[i];
        int id = f->kind == 0 ? SPR_JUICE + (f->t / 12 < 3 ? f->t / 12 : 2) : SPR_SNOWBIT + (f->t / 8) % 2;
        spr(id, f->x / 16 - 4, f->y / 16 - 4, 2, -1);
    }
}

/* the world's events this frame: juice on a splat, snow crumbs on a smash or a wash (main.c calls it) */
void draw_events(const world *w)
{
    for (int p = 0; p < w->players; p++) {
        int ev = w->events[p];
        const berry *b = &w->b[p];
        int sx = (int)(b->x >> 16) - view_camx, sy = screen_y(b->h >> 16, sx) - 8;
        if (ev & EV_DIE) {
            static const int8_t dir[8][2] = {{-20, -40}, {-12, -52}, {-4, -60}, {6, -56}, {14, -48}, {22, -36},
                                             {-26, -24}, {28, -26}};
            for (int i = 0; i < 8; i++) fx_add(sx, sy, dir[i][0], dir[i][1], 0);
        }
        if (ev & (EV_SMASH | EV_SHRINK))
            for (int i = 0; i < 4; i++) fx_add(sx + 8, sy + 4, 8 + i * 6, -30 - i * 7, 1);
        if (ev & EV_ORB) { dew_t[p] = 0; dew_id[p] = b->orb_used; }
    }
}

static void draw_props(const world *w, int t)
{
    const bt_course *c = &w->course;
    int32_t col0 = view_camx / CELL - 1;
    for (int32_t col = col0; col < col0 + 22; col++) {
        const bt_col *k = course_col(c, col);
        int sx = col * CELL - view_camx + CELL / 2;
        for (int r = 0; r < ROWS; r++) {
            int kind = k->cell[r];
            if ((k->gone >> r) & 1) continue;
            int sy = screen_y(r * CELL, sx);
            if (kind == K_PAD) {
                int used = 0;
                for (int p = 0; p < w->players; p++)
                    if (w->b[p].pad_used == OBJ_ID(col, r) && w->b[p].air_t < 8) used = 1;
                spr(SPR_MUSHROOM + used, sx - 8, sy - 16, 2, -1);
            } else if (kind == K_ORB) {
                int ring = 0;
                for (int p = 0; p < w->players; p++)
                    if (dew_id[p] == OBJ_ID(col, r) && dew_t[p] < 12) ring = 1 + (dew_t[p] >= 6);
                spr(SPR_DEW + ((t / 20) & 1), sx - 8, sy - 16 + hu_bob(t + col * 7, 64, 1), 2, -1);
                if (ring) spr(SPR_DEW_RING + ring - 1, sx - 8, sy - 16, 2, -1);
            } else if (kind == K_COIN) {
                spr(SPR_GOLDEN + ((t / 16) & 1), sx - 8, sy - 16 + hu_bob(t + col * 5, 64, 1), 2, -1);
                if ((t / 20) % 3 == 0) spr(SPR_SPARK + ((t / 10) & 1), sx + 2, sy - 18, 2, -1);
            }
        }
        if (k->flags & (F_LEAF | F_LEAF_END)) {
            /* the gust: little leaves spinning up the column (the glide starts) or blown down (it ends) */
            for (int i = 0; i < 5; i++) {
                int ph = (t * 2 + i * 23) % 96;
                int yy = (k->flags & F_LEAF) ? ph : 96 - ph;
                int xx = sx - 6 + ((i * 5 + ph / 8) % 12);
                spr(SPR_GUST + ((t / 8 + i) & 1), xx - 4, screen_y(yy, xx) - 4, 2, -1);
            }
        }
    }
    int64_t xref = course_x(c, w->f);
    for (int32_t i = c->ncone - 1; i >= 0 && i >= c->ncone - CONE_RING; i--) {
        const bt_cone *k = &c->cone[i % CONE_RING];
        if (k->gone) continue;
        int kx = (int)(cone_x(k, xref) >> 16) - view_camx;
        if (kx < -16 || kx > RS_SCREEN_W + 16) continue;
        spr(SPR_CONE + (((unsigned)(-kx) / 4) & 3), kx - 8, screen_y(0, kx) - 15, 2, -1);
    }
}

/* ---- the UI (the house kit: the title is the only menu, up to 4 players; docs/art-direction.md "Title and players") -- */
/* the title's player slots: each joined player's berry, rolling in its colours (hu_title_sprites calls it) */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr(SPR_BERRY + ((t / 3 + p * 4) & 15), cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), 2, hu_player_pal(p));
}

/* the results of 2-4 players: ranked by score (metres + golden blueberries) */
static void standing(const world *w, hu_standing *s)
{
    int score[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) score[p] = world_score(w, p);
    hu_rank(s, w->players, score, score);
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best, int attempt, const hu_standing *rs)
{
    char s[48];
    int screen = state == ST_DEAD ? ST_PLAY : state;
    if (screen != shown_state || best != shown_best || w->players != shown_players) {
        hu_clear();
        rs_bg_scroll(RS_BG1, 0, 0);
        attempt_on = 0;
        gate_t = 999;
        if (state == ST_TITLE) {
            hu_logo(GAME_TITLE, title_ramps, 2, 2, 0);
            logo_on = 1;
        } else if (logo_on) {
            hu_logo_hide();
            pal_loop[PAL_LOGO] = 0;             /* the logo borrowed that palette: load its biome again */
            logo_on = 0;
        }
        if (screen == ST_PLAY) {
            /* "ATTEMPT n", in the world: it rolls away with the ground */
            snprintf(s, sizeof s, "ATTEMPT %d", attempt);
            hu_text(22, 10, s);
            attempt_on = 1;
            attempt_x0 = view_camx;
            if (w->players >= 3) hu_score_tags(w->players, 0);     /* P1..P4 by the corner chips */
        }
        if (state == ST_OVER) {
            if (w->players == 1) {
                hu_banner(4, "GAME OVER");
                hu_gameover_panel(1, new_best, world_score(w, 0), 0);
                if (w->coins[0]) snprintf(s, sizeof s, "%d M  +%d GOLD", w->metres[0], w->coins[0]);
                else snprintf(s, sizeof s, "%d M", w->metres[0]);
                hu_box_text(12, 12, s);
            } else {
                hu_results_panel(rs, NULL);     /* P2 WINS! (or DRAW!) and the ranking */
            }
        }
        shown_state = screen;
        shown_best = best;
        shown_players = w->players;
    }
    /* the title, every frame: PRESS A TO ROLL (blinking), the player slots, BEST, P2 / P3 / P4: PRESS A TO JOIN */
    if (state == ST_TITLE) hu_title_draw(st_t, best);
    if (screen == ST_PLAY) {
        if (attempt_on) {
            int dx = view_camx - attempt_x0;
            if (dx > 420) {
                hu_clear_rows(10, 1);
                attempt_on = 0;
                rs_bg_scroll(RS_BG1, 0, 0);
            } else {
                rs_bg_scroll(RS_BG1, dx, 0);
            }
        }
        /* a gate: the biome's name for 2.5 s */
        int32_t pcol = (int32_t)((w->b[0].x >> 16) / CELL);
        if (pcol > 0 && pcol != last_gate_col && (course_col(&w->course, pcol)->flags & F_GATE)) {
            last_gate_col = pcol;
            gate_t = 0;
            gate_biome = course_biome_of_col(pcol);
        }
        if (gate_t == 0 && !attempt_on) {
            char line[40];
            snprintf(line, sizeof line, "%s%s", course_loop_of_col(pcol) ? "NIGHT - " : "", gm_biome_names[gate_biome]);
            hu_clear_rows(6, 1);
            hu_text(hu_center(line, 0), 6, line);
        }
        if (gate_t == 150) hu_clear_rows(6, 1);
        if (gate_t < 999) gate_t++;
    }
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, "A: ROLL AGAIN");
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, "A: ROLL AGAIN");
    }
}

/* ---- the frame ------------------------------------------------------------------------------------------------------------------ */
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, int attempt, int medal)
{
    static const int medals[4] = MEDAL_METRES;
    int t = (int)rs_frame_count();
    const bt_course *c = &w->course;
    (void)medal;
    view_course = c;
    hu_standing rs;
    standing(w, &rs);
    /* the camera: player 1 (or the first one still rolling) at the pivot; it rises when a berry goes high, and
     * follows the profile under the pivot (the ground drops at a cliff: the view goes down with it) */
    int lead = 0;
    while (lead < w->players - 1 && w->b[lead].dead) lead++;
    if (w->b[lead].dead) lead = 0;
    int64_t xref = course_x(c, w->f);
    int camx = (int)(xref >> 16) - PIVOT_X - lead * P2_OFFSET;
    int top = 0;
    for (int p = 0; p < w->players; p++)
        if (!w->b[p].dead) {
            int tp = (w->b[p].h >> 16) + berry_size(&w->b[p]);
            if (tp > top) top = tp;
        }
    int want = top - (GROUND_SCREEN_Y - CAM_TOP_MARGIN);
    if (want < 0) want = 0;
    if (want > 72) want = 72;
    int want_d16 = course_profile(c, camx + PIVOT_X) * 16;
    if (!paused) {
        int d = want - cam_up;
        cam_up += d / 6 + (d > 0) - (d < -5);
        int dd = want_d16 - cam_d16;
        cam_d16 += dd / 5 + (dd > 0) - (dd < 0);
    }
    if (state == ST_TITLE || state == ST_READY) { cam_up = 0; cam_d16 = want_d16; }
    int dx = camx - view_camx;
    /* the slope of the biome under the pivot, eased over the first 16 m after a gate */
    int32_t pcol = (camx + PIVOT_X) / CELL;
    if (pcol < 0) pcol = 0;
    int b = course_biome_of_col(pcol);
    int32_t into = pcol % BIOME_CELLS;
    int k256 = slope256[b];
    if (pcol >= BIOME_CELLS && into < 16) {
        int pb = (b + NBIOMES - 1) % NBIOMES;
        k256 = slope256[pb] + (slope256[b] - slope256[pb]) * (int)into / 16;
    }
    int sxk = hu_shake_x(), syk = hu_shake_y(), cd = cam_d16 / 16;
    view_camx = camx;
    view_c = -k256;
    view_vofs = GROUND_PY - GROUND_SCREEN_Y - cam_up - syk + cd;
    view_hofs = (camx - sxk) & (MAP_W * 8 - 1);
    stream_playfield(c, camx < 0 ? -((-camx + CELL - 1) / CELL) : camx / CELL);
    stream_mid(camx);
    load_slot_palettes(camx);
    /* the beat pulse: player 1's x counts the beats (64 px each), the bar's downbeat is stronger */
    int32_t xpx = (int32_t)(xref >> 16);
    int since = (int)(((int64_t)(xpx & 63) * Q16_ONE) / (course_speed(c, w->f) + 1));
    int pulse = (state == ST_PLAY) && since < 10 ? (10 - since) * ((xpx & 255) < 64 ? 3 : 2) : 0;
    sky_and_far(camx, pulse);

    rs_affine m = {256, 0, view_c, 256, view_hofs + PIVOT_X, view_vofs, 1};
    rs_bg_scroll(RS_BG2, view_hofs, view_vofs);
    rs_bg_affine(RS_BG2, &m);
    /* the far layers go down with the profile too, less (parallax): the drop of a cliff reads in depth */
    rs_bg_scroll(RS_BG3, (camx / 2 - sxk) & 511, MID_TROW * 8 - 132 - cam_up / 4 + cd / 2);
    rs_bg_scroll(RS_BG4, (camx / 4) & 511, FAR_TROW * 8 - 60 - cam_up / 8 + cd / 4);

    screen_text(w, state, st_t, best, new_best, attempt, &rs);
    if (state == ST_OVER && st_t == RETRY_LOCK) shown_state = -1;      /* redraw once: the retry line appears */
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    if (state == ST_OVER) rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);
    if (!paused) {
        fx_update(dx);
        for (int p = 0; p < MAX_PLAYERS; p++)
            if (dew_t[p] < 99) dew_t[p]++;
    }

    rs_oam_clear();
    /* front to back: the UI, the berries, the effects, the props */
    if (state == ST_PLAY || state == ST_DEAD) {
        if (w->players == 1) {
            hu_number(w->metres[0], RS_SCREEN_W / 2, 10, 3);
            if (w->coins[0]) {
                spr(SPR_GOLDEN, 266, 10, 3, -1);
                hu_number(w->coins[0], 296, 10, 3);
            }
        } else {
            int score[MAX_PLAYERS];
            for (int p = 0; p < w->players; p++) score[p] = world_score(w, p);
            hu_score_chips(w->players, score, 0);   /* 2: x 80 / 240; 3-4: the corners, tagged P1..P4 */
        }
    }
    if (state == ST_OVER) {
        if (w->players == 1) {
            hu_gameover_sprites(1, world_score(w, 0), 0, best, hu_medal_of(w->metres[0], medals), st_t, slide);
        } else {
            hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_BERRY + ((st_t / 3) & 15), cx - 8, cy - 8 + slide, 3, hu_player_pal(rs.order[i]));
            }
        }
    }
    if (state == ST_TITLE) hu_title_sprites(st_t, slot_icon, NULL);   /* the prompt's A, the joined berries pop in */
    for (int p = w->players - 1; p >= 0; p--) draw_berry(w, p, state, st_t);
    fx_draw();
    draw_props(w, t);
}

/* the playfield registers written this frame (the screen-reading bot reads them, like the PPU's) */
void draw_view(int *hofs, int *vofs, int *c256)
{
    *hofs = view_hofs;
    *vofs = view_vofs;
    *c256 = view_c;
}

/* what a playfield map entry shows (the bot reads the screen): K_BLOCK (any solid), K_THORN, K_HANG, K_PEBBLE for
 * an air cell's top-left tile, 100 + G_* for a ground surface's, -1 for anything else */
static int16_t kind_of[1024];        /* a cache made from the constant metatiles (state_audit.txt) */
static int kind_ready;

int draw_tile_kind(uint16_t e)
{
    if (!kind_ready) {
        for (int i = 0; i < 1024; i++) kind_of[i] = -1;
        for (int b = 0; b < NBIOMES; b++)
            for (int mt = 0; mt < MT_COUNT; mt++) {
                int k = -1;
                switch (mt) {
                case MT_ROCK_TOP: case MT_ROCK: case MT_LOG_L: case MT_LOG_M: case MT_LOG_R: case MT_LOG_1: case MT_LOG_V:
                    k = K_BLOCK; break;
                case MT_THORN: k = K_THORN; break;
                case MT_HANG: k = K_HANG; break;
                case MT_PEBBLE: k = K_PEBBLE; break;
                case MT_VOID_TOP: k = 100 + G_GAP; break;
                case MT_ICE: k = 100 + G_ICE; break;
                case MT_SNOW: k = 100 + G_SNOW; break;
                case MT_WATER: k = 100 + G_WATER; break;
                default: break;
                }
                if (k < 0) continue;
                for (int q = 0; q < 4; q++) {
                    int tile = RS_MAP_TILE(gm_pf_meta[(b * MT_COUNT + mt) * 4 + q]);
                    if (!tile) continue;
                    kind_of[tile] = kind_of[tile] == -1 || kind_of[tile] == k ? (int16_t)k : -2;   /* -2: shared */
                }
            }
        /* a tile also used by a metatile of no kind (dirt, decor, the sign) says nothing */
        for (int b = 0; b < NBIOMES; b++)
            for (int mt = 0; mt < MT_COUNT; mt++) {
                int plain = mt == MT_DIRT0 || mt == MT_DIRT1 || mt == MT_DEEP || mt == MT_WALL_L || mt == MT_WALL_R ||
                            mt == MT_VOID || mt == MT_SIGN || mt == MT_DECO0 || mt == MT_DECO1 || mt == MT_DECO2;
                if (!plain) continue;
                for (int q = 0; q < 4; q++) {
                    int tile = RS_MAP_TILE(gm_pf_meta[(b * MT_COUNT + mt) * 4 + q]);
                    if (tile && kind_of[tile] >= 0) kind_of[tile] = -2;
                }
            }
        kind_ready = 1;
    }
    return kind_of[RS_MAP_TILE(e)];
}

/* where the ground line is on the playfield map at map x (px): its map y, read like the bot reads the screen: the
 * first tile from the top that can show the ground (a surface's top row, a ramp strip), and in it the first drawn
 * pixel of that pixel column; -1: none */
static uint8_t ground_of[1024];     /* a cache made from the constant art (state_audit.txt) */
static int ground_ready;

static int pf_pixel(int tile, int x, int y)
{
    if (tile < 0 || tile >= gm_pf_tile_count) return 0;
    uint8_t v = gm_pf_tiles[tile * 32 + y * 4 + x / 2];
    return (x & 1) ? v & 15 : v >> 4;
}

int draw_ground_y(int map_x)
{
    if (!ground_ready) {
        static const int tops[] = {MT_SURF0, MT_SURF1, MT_SURF2, MT_EDGE_L, MT_EDGE_R, MT_ICE, MT_SNOW, MT_WATER, MT_VOID_TOP};
        memset(ground_of, 0, sizeof ground_of);
        for (int b = 0; b < NBIOMES; b++) {
            for (int i = 0; i < (int)(sizeof tops / sizeof tops[0]); i++)
                for (int q = 0; q < 2; q++) ground_of[RS_MAP_TILE(gm_pf_meta[(b * MT_COUNT + tops[i]) * 4 + q]) & 1023] = 1;
            for (int i = 0; i < RAMP_COUNT * RAMP_ROWS * 2; i++) {
                int tile = RS_MAP_TILE(gm_ramp_map[b * RAMP_COUNT * RAMP_ROWS * 2 + i]), clear = 0;
                for (int k = 0; k < 64 && !clear; k++) clear = !pf_pixel(tile, k & 7, k >> 3);
                if (clear && tile) ground_of[tile] = 1;
            }
        }
        /* a tile also drawn above the ground (rocks, logs, thorns, the sign, the decor) says nothing */
        static const int above[] = {MT_ROCK_TOP, MT_ROCK, MT_LOG_L, MT_LOG_M, MT_LOG_R, MT_LOG_1, MT_LOG_V, MT_THORN,
                                    MT_HANG, MT_PEBBLE, MT_SIGN, MT_DECO0, MT_DECO1, MT_DECO2};
        for (int b = 0; b < NBIOMES; b++)
            for (int i = 0; i < (int)(sizeof above / sizeof above[0]); i++)
                for (int q = 0; q < 4; q++) ground_of[RS_MAP_TILE(gm_pf_meta[(b * MT_COUNT + above[i]) * 4 + q]) & 1023] = 0;
        ground_of[0] = 0;
        ground_ready = 1;
    }
    int tx = (map_x >> 3) & (MAP_W - 1), px = map_x & 7;
    for (int ty = 0; ty < 64; ty++) {
        uint16_t e = rs_bg_get(RS_BG2, tx, ty);
        int tile = RS_MAP_TILE(e);
        if (!ground_of[tile]) continue;
        int x = (e & RS_MAP_HFLIP) ? 7 - px : px;
        for (int y = 0; y < 8; y++)
            if (pf_pixel(tile, x, (e & RS_MAP_VFLIP) ? 7 - y : y)) return ty * 8 + y;
        /* the line runs just under this tile here: the next tile's top pixel */
        uint16_t e2 = rs_bg_get(RS_BG2, tx, (ty + 1) & 63);
        if (pf_pixel(RS_MAP_TILE(e2), (e2 & RS_MAP_HFLIP) ? 7 - px : px, (e2 & RS_MAP_VFLIP) ? 7 : 0)) return (ty + 1) * 8;
    }
    return -1;
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(line_col); S(pf_col_lo); S(pf_col_hi); S(pf_gone); S(mid_col_hi); S(pal_loop); S(pal_biome); S(cam_up);
    S(shown_state); S(shown_best); S(shown_players); S(logo_on); S(attempt_on); S(attempt_x0); S(gate_t);
    S(gate_biome); S(last_gate_col); S(fxs); S(fx_n); S(view_hofs); S(view_vofs); S(view_c); S(view_camx);
    S(dew_t); S(dew_id); S(cam_d16);
    RS_STATE_RASTER(raster);
}
#undef S
