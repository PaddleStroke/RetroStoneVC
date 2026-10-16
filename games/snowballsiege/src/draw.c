/*
 * Snowball Siege: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1; the field's map at the bottom), BG2 the snowy battlefield (drawn at run
 *   time from the terrain bitmap: shared inner tiles and a deduplicated pool for the edges, redrawn where a splat
 *   carved), BG3 the far mountains (parallax 1/2), the backdrop a raster sky gradient; sprites: the four teams (OBJ 0,
 *   1, 4, 5: a species each), the throws, splats, hearts and HUD (OBJ 2), the kit's digits, glyphs and medals (OBJ 3),
 *   the pine trees and the map's dots (OBJ 6).
 * Widescreen (docs/spec.md "Widescreen", DESIGN.md): the camera shows more of the same battlefield; the UI kit centres
 *   itself (the 320-px stage is centred: rs_screen_origin).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

/* the title logo: one 3-shade ramp per word: SNOWBALL icy, SIEGE a warm scarf red */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(244, 252, 255), RS_RGB8(168, 214, 248), RS_RGB8(92, 148, 220)},
    {RS_RGB8(255, 196, 176), RS_RGB8(238, 98, 92), RS_RGB8(176, 50, 62)}};
/* the sky: the top and the horizon of the raster gradient (a winter afternoon) */
#define SKY_TOP     0x5a8ccc
#define SKY_HORIZON 0xe2ecf8
#define SKY_LOW     176
#define FCOLS (WORLD_W / 8)                /* the field's tile columns, rows */
#define FROWS (WORLD_H / 8)
#define T_SNOW 1                        /* the shared inner tiles: 4 snow variants, 4 rock ones */
#define T_ROCK 5
#define T_POOL 16                       /* the pool's first tile */
#define MM_TILE 768                     /* the field's map (the minimap): BG1 tiles 768.., 24 x 3 */
#define MM_W 192
#define MM_H 24
#define MM_Y0 96                        /* the map shows the field from y 96, 6 px a row, 4 px a column */
#define MM_COL 8                        /* its UI tile column and row */
#define MM_ROW 27
#define FLAKES 22

static rs_color line_col[RS_SCREEN_H];
static uint16_t cell_map[FROWS][FCOLS];       /* the BG2 entry of each field cell */
static uint32_t pool_h1[TERRAIN_POOL], pool_h2[TERRAIN_POOL];
static uint16_t pool_refs[TERRAIN_POOL];
static uint32_t seen_gen, seen_seq, seen_seed, shown_key = 0xffffffffu;
static int dimmed = -1, pool_full;
static int32_t snow_drift;
static uint8_t speck_tab[16 * 16];      /* a fixed sprinkle of darker specks in the snow (period 16) */
static uint8_t px_buf[64];              /* scratch: one tile's pixels (state_audit.txt) */

/* ---- the sky --------------------------------------------------------------------------------------------------- */
static void raster(int line, void *user)
{
    (void)user;
    rs_pal_set(0, line_col[line]);
}

static void sky_gradient(int k256)
{
    int a[3] = {(SKY_TOP >> 16) & 255, (SKY_TOP >> 8) & 255, SKY_TOP & 255};
    int b[3] = {(SKY_HORIZON >> 16) & 255, (SKY_HORIZON >> 8) & 255, SKY_HORIZON & 255};
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int k = y < SKY_LOW ? y * 256 / SKY_LOW : 256, c[3];
        for (int i = 0; i < 3; i++) c[i] = ((a[i] * (256 - k) + b[i] * k) / 256) * k256 / 256;
        /* house rule: a 2-line dither step every 8 lines keeps the RGB555 bands soft */
        line_col[y] = RS_RGB8(c[0] + ((y & 4) ? 4 : 0), c[1] + ((y & 4) ? 2 : 0), c[2] + ((y & 4) ? 4 : 0));
    }
}

/* the field's palettes, dimmed on the title (the cream text reads over the snow) */
static void field_palettes(int dim)
{
    if (dim == dimmed) return;
    dimmed = dim;
    int k = dim ? 168 : 256;
    rs_color p[16];
    for (int i = 0; i < 16; i++) p[i] = hu_scale_color(gm_terrain_pal[i], k);
    rs_pal_load(RS_PAL_BG(PAL_TERRAIN), p, 16);
    for (int i = 0; i < 16; i++) p[i] = hu_scale_color(gm_far_pal[i], k);
    rs_pal_load(RS_PAL_BG(PAL_FAR), p, 16);
    sky_gradient(k);
}

/* ---- the snowy field (BG2), from the terrain bitmap ------------------------------------------------------------- */
static int speck(int x, int y) { return speck_tab[(y & 15) * 16 + (x & 15)]; }

static int ter_pixel(const world *w, int x, int y)
{
    if (!terrain_solid(w, x, y)) {
        if (y >= POND_Y && in_pond(w, x)) {
            int d = y - POND_Y;
            if (d < 2) return ((x * 37 + d * 11) % 23 == 0) ? 15 : 11;      /* the thin ice, a crack here and there */
            return d < 4 ? 12 : d < 9 ? 13 : 14;
        }
        return 0;
    }
    if (y >= BEDROCK_Y) return !terrain_solid(w, x, y - 1) || speck(x, y) ? 9 : 10;
    int up1 = terrain_solid(w, x, y - 1), down = terrain_solid(w, x, y + 1);
    int left = terrain_solid(w, x - 1, y), right = terrain_solid(w, x + 1, y);
    if (fort_at(w, x, y)) {                                 /* packed snow blocks */
        if (!up1 || !left) return 6;
        if (!down || !right) return 8;
        if (y % 6 == 0 || (x + ((y / 6) & 1) * 4) % 8 == 0) return 8;
        return 7;
    }
    if (!up1) return 4;                                     /* the house's dark top line */
    if (!terrain_solid(w, x, y - 2)) return 1;              /* the light top rows */
    if (!down) return 5;                                    /* an overhang's underside */
    if (!left) return 2;                                    /* lit from the left */
    if (!right) return 4;
    if (!terrain_solid(w, x, y - 3)) return 2;
    return speck(x, y) ? 4 : 3;
}

static uint32_t hash_px(const uint8_t *p, uint32_t h, uint32_t mul)
{
    for (int i = 0; i < 64; i++) h = (h ^ p[i]) * mul;
    return h;
}

/* the kind of a cell without drawing it: 0 empty, T_SNOW.. / T_ROCK.. a shared inner tile, -1 an edge (drawn) */
static int cell_kind(const world *w, int cx, int cy)
{
    int y0 = cy * 8, x0 = cx * 8;
    int full = 1, empty = 1;
    for (int y = y0; y < y0 + 8; y++) {
        uint8_t b = w->ter[y][cx];
        if (b != 0xff) full = 0;
        if (b) empty = 0;
    }
    if (empty) {
        if (y0 + 7 >= POND_Y && (in_pond(w, x0) || in_pond(w, x0 + 7))) return -1;
        return 0;
    }
    if (!full) return -1;
    int v = (cx & 1) + (cy & 1) * 2;
    if (y0 > BEDROCK_Y) return T_ROCK + v;
    if (y0 + 8 > BEDROCK_Y) return -1;
    for (int y = y0 - 3; y <= y0 + 8; y++)
        if (y < 0 || w->ter[y][cx] != 0xff) return -1;
    for (int y = y0; y < y0 + 8; y++)
        if ((cx > 0 && !(w->ter[y][cx - 1] & 0x80)) || (cx < FCOLS - 1 && !(w->ter[y][cx + 1] & 1))) return -1;
    for (int i = 0; i < w->nfort; i++) {
        const rect16 *f = &w->fort[i];
        if (f->x1 >= x0 && f->x0 <= x0 + 7 && f->y1 >= y0 && f->y0 <= y0 + 7) return -1;
    }
    return T_SNOW + v;
}

static void cell_release(int cx, int cy)
{
    int t = RS_MAP_TILE(cell_map[cy][cx]);
    if (t >= T_POOL && pool_refs[t - T_POOL]) pool_refs[t - T_POOL]--;
    cell_map[cy][cx] = 0;
}

static void cell_update(const world *w, int cx, int cy)
{
    cell_release(cx, cy);
    int k = cell_kind(w, cx, cy), tile = k;
    if (k < 0) {
        int solid = 0;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                int v = ter_pixel(w, cx * 8 + x, cy * 8 + y);
                px_buf[y * 8 + x] = (uint8_t)v;
                solid += v != 0;
            }
        uint32_t h1 = hash_px(px_buf, 2166136261u, 16777619u), h2 = hash_px(px_buf, 0x9747b28cu, 0x01000193u ^ 0x5bd1e995u);
        int hit = -1, free_i = -1;
        for (int i = 0; i < TERRAIN_POOL; i++) {
            if (pool_h1[i] == h1 && pool_h2[i] == h2) { hit = i; break; }
            if (free_i < 0 && !pool_refs[i]) free_i = i;
        }
        if (hit >= 0 && !pool_refs[hit]) {
            /* a freed tile with the same pixels: reuse it as it is (still in VRAM) */
        } else if (hit < 0) {
            if (free_i < 0) {
                for (int i = 0; i < TERRAIN_POOL && free_i < 0; i++)
                    if (!pool_refs[i]) free_i = i;
            }
            if (free_i < 0) {                               /* the pool is full: the nearest shared tile */
                if (!pool_full) rs_log("snowballsiege: the field's tile pool is full");
                pool_full = 1;
                cell_map[cy][cx] = RS_MAP(solid >= 32 ? T_SNOW + (cx & 1) + (cy & 1) * 2 : 0, PAL_TERRAIN, 0, 0, 0);
                return;
            }
            hit = free_i;
            rs_tiles_load8(VR_BG2 + T_POOL + hit, px_buf, 1);
            pool_h1[hit] = h1;
            pool_h2[hit] = h2;
        }
        pool_refs[hit]++;
        tile = T_POOL + hit;
    }
    cell_map[cy][cx] = RS_MAP(tile, PAL_TERRAIN, 0, 0, 0);
    rs_bg_put(RS_BG2, cx, cy, cell_map[cy][cx]);
}

/* the field's map: 4 px a column, 6 px a row, on the UI palette */
static void minimap_cols(const world *w, int tx0, int tx1)
{
    for (int tx = tx0; tx <= tx1; tx++)
        for (int ty = 0; ty < MM_H / 8; ty++) {
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    int mx = tx * 8 + x, my = ty * 8 + y;
                    int wx = mx * 4 + 2, wy = MM_Y0 + my * 6 + 3, c;
                    if (terrain_solid(w, wx, wy)) {
                        c = wy >= BEDROCK_Y ? HU_OUTLINE : !terrain_solid(w, wx, wy - 6) ? HU_CREAM : HU_LIGHT;
                    } else {
                        c = wy >= POND_Y && in_pond(w, wx) ? HU_HILITE : HU_FILL;
                    }
                    if (my == 0 || my == MM_H - 1) c = c == HU_FILL ? HU_DARK : c;
                    px_buf[y * 8 + x] = (uint8_t)c;
                }
            rs_tiles_load8(VR_BG1 + MM_TILE + ty * (MM_W / 8) + tx, px_buf, 1);
        }
}

static void minimap_put(void)
{
    int mw = rs_bg_map_w(RS_BG1);
    for (int ty = 0; ty < MM_H / 8; ty++)
        for (int tx = 0; tx < MM_W / 8; tx++)
            rs_bg_put(RS_BG1, (MM_COL + tx) & (mw - 1), MM_ROW + ty,
                      RS_MAP(MM_TILE + ty * (MM_W / 8) + tx, 0, 1, 0, 0));
}

static void field_full(const world *w)
{
    memset(pool_refs, 0, sizeof pool_refs);
    memset(cell_map, 0, sizeof cell_map);
    for (int cy = 0; cy < FROWS; cy++)
        for (int cx = 0; cx < FCOLS; cx++) cell_update(w, cx, cy);
    minimap_cols(w, 0, MM_W / 8 - 1);
}

static void field_sync(const world *w)
{
    if (w->gen != seen_gen || w->seed != seen_seed || w->dirty_seq - seen_seq > DIRTY_N) {
        field_full(w);
        seen_gen = w->gen;
        seen_seed = w->seed;
        seen_seq = w->dirty_seq;
        return;
    }
    for (; seen_seq != w->dirty_seq; seen_seq++) {
        const rect16 *r = &w->dirty[seen_seq % DIRTY_N];
        int cx0 = (r->x0 - 1) >> 3, cx1 = (r->x1 + 1) >> 3, cy0 = (r->y0 - 2) >> 3, cy1 = (r->y1 + 4) >> 3;
        for (int cy = cy0 < 0 ? 0 : cy0; cy <= cy1 && cy < FROWS; cy++)
            for (int cx = cx0 < 0 ? 0 : cx0; cx <= cx1 && cx < FCOLS; cx++) cell_update(w, cx, cy);
        int m0 = (r->x0 / 4) >> 3, m1 = (r->x1 / 4) >> 3;
        minimap_cols(w, m0 < 0 ? 0 : m0, m1 > MM_W / 8 - 1 ? MM_W / 8 - 1 : m1);
    }
}

void draw_new_world(void) { seen_gen = 0xffffffffu; }

/* ---- set-up ------------------------------------------------------------------------------------------------------ */
static int map_w(void) { return rs_screen_width() > 512 ? 128 : 64; }

static void maps_setup(void)
{
    rs_bg_setup(RS_BG1, map_w(), 32, VR_BG1);
    rs_bg_setup(RS_BG2, 128, 32, VR_BG2);
    rs_bg_setup(RS_BG3, FAR_W, 32, VR_BG3);
    for (int y = 0; y < FAR_H; y++)
        for (int x = 0; x < FAR_W; x++) rs_bg_put(RS_BG3, x, FAR_Y / 8 + y, gm_far_map[y * FAR_W + x]);
    for (int cy = 0; cy < FROWS; cy++)
        for (int cx = 0; cx < FCOLS; cx++) rs_bg_put(RS_BG2, cx, cy, cell_map[cy][cx]);
    shown_key = 0xffffffffu;
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more of the field on both sides */
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0 on palette 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;         /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    uint32_t s = 0x5eed5;
    for (int i = 0; i < 256; i++) {
        s = s * 1103515245u + 12345u;
        speck_tab[i] = ((s >> 16) & 15) == 0;
    }
    static const uint8_t zero[64];
    rs_tiles_load8(VR_BG2, zero, 1);
    for (int v = 0; v < 4; v++) {           /* the shared inner tiles */
        int ox = (v & 1) * 8, oy = (v >> 1) * 8;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) px_buf[y * 8 + x] = speck(ox + x, oy + y) ? 4 : 3;
        rs_tiles_load8(VR_BG2 + T_SNOW + v, px_buf, 1);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) px_buf[y * 8 + x] = speck(ox + x, oy + y) ? 9 : 10;
        rs_tiles_load8(VR_BG2 + T_ROCK + v, px_buf, 1);
    }
    rs_tiles_load(VR_BG3, gm_far_tiles, gm_far_tile_count);
    maps_setup();
    for (int l = 0; l < 3; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* bears, penguins, props (the kit loaded OBJ 3) */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 48);     /* foxes, hares, scenery */
    dimmed = -1;
    field_palettes(1);
    rs_raster(raster, NULL);
    seen_gen = 0xffffffffu;
}

/* ---- sprites ----------------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int flags)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, d->pal, prio, flags);
}

static int crit_spr(int species, int pose) { return SPR_BEAR_IDLE0 + species * CRIT_POSES + pose; }
enum { P_IDLE0, P_IDLE1, P_WALK0, P_WALK1, P_THROW, P_HURT, P_AIR, P_MOUND, P_ICON };

void touch_button_pos(int *x, int *y)
{
    *x = rs_screen_width() + BTN_X;
    *y = BTN_Y;
}

/* the title's player slots, the results: each player's team icon (hu_title_sprites calls it) */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr(crit_spr(species_of_player(p), P_ICON), cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), 3, 0);
}

static void draw_critters(const world *w, int vx0, int sx, int sy, int t, int play)
{
    for (int i = 0; i < w->ncrit; i++) {
        const critter *c = &w->c[i];
        if (c->state == CS_SPLASHED) continue;
        int sp = w->s[c->side].species, x = (c->x >> 16) - vx0 + sx, y = (c->y >> 16) + sy;
        int active = play && i == w->cur && w->phase != PH_END && w->phase != PH_DEMO, pose;
        if (c->state == CS_SNOWED) pose = P_MOUND;
        else if (c->state == CS_AIR) pose = P_AIR;
        else if (c->hurt_t < 24) pose = P_HURT;
        else if (active && (w->charging || w->sling_on || (w->thrown && c->anim_t < 18))) pose = P_THROW;
        else if (active && c->anim_t < 6) pose = P_WALK0 + (((c->x >> 16) >> 2) & 1);
        else pose = P_IDLE0 + ((t / 32 + i) & 1);
        if (c->hurt_t < 24 && (c->hurt_t & 4)) continue;    /* a hit blinks */
        spr(crit_spr(sp, pose), x - 8, y - 16, 2, c->facing < 0 ? RS_SPR_HFLIP : 0);
        if (pose != P_MOUND && play) spr(SPR_HEARTS0 + (c->hp < 0 ? 0 : c->hp > HP_MAX ? HP_MAX : c->hp), x - 12,
                                         y - 26, 2, 0);
    }
}

static void draw_aim(const world *w, int vx0, int sx, int sy, int t, const touch_view *tv)
{
    const critter *c = world_active_c(w);
    if (c->state != CS_STAND || w->thrown) return;
    int x = (c->x >> 16) - vx0 + sx, y = (c->y >> 16) + sy, f = c->facing;
    if (w->charging || w->sling_on) {
        int fill = w->power * 32 / POWER_MAX;
        for (int i = 0; i < 4; i++) {
            int k = fill - i * 8;
            spr(SPR_GAUGE0 + (k < 0 ? 0 : k > 8 ? 8 : k), x - 16 + i * 8, y - 38, 2, 0);
        }
    } else {
        spr(SPR_MARKER, x - 4, y - 42 + hu_bob(t, 32, 2), 2, 0);
    }
    for (int i = 0; i < 4; i++) {                       /* the aim line from the hand */
        int d = 12 + i * 7;
        int dx = (int)(((int64_t)icos(w->angle) * d) >> 16) * f, dy = -(int)(((int64_t)isin(w->angle) * d) >> 16);
        spr(SPR_DOT, x + f * 4 + dx - 4, y - 11 + dy - 4, 2, 0);
    }
    if (w->sling_on && tv && tv->sling) {               /* the slingshot's band: from the hand to the finger */
        int fx = tv->fx - rs_screen_origin_x(), fy = tv->fy;
        for (int i = 1; i <= 3; i++)
            spr(SPR_DOT, x + (fx - x) * i / 4 - 4, y - 11 + (fy - (y - 11)) * i / 4 - 4, 2, 0);
    }
}

static void draw_fx(const world *w, int vx0, int sx, int sy)
{
    for (int i = 0; i < MAX_FX; i++) {
        const fx *f = &w->fx[i];
        if (!f->life) continue;
        int x = (f->x >> 8) - vx0 + sx, y = (f->y >> 8) + sy;
        switch (f->kind) {
        case FX_CHUNK: spr(SPR_CHUNK0 + f->t * 3 / f->life, x - 4, y - 4, 2, 0); break;
        case FX_PUFF:
        case FX_TREE: spr(SPR_PUFF0 + f->t * 3 / f->life, x - 8, y - 8, 2, 0); break;
        case FX_SPLASH: spr(SPR_SPLASH0 + f->t * 3 / f->life, x - 8, y - 15, 2, 0); break;
        default: spr(SPR_FLAKE0, x - 4, y - 4, 2, 0); break;
        }
    }
}

static void draw_proj(const world *w, int vx0, int sx, int sy)
{
    if (!w->p.on) return;
    int x = (w->p.x >> 16) - vx0 + sx, y = (w->p.y >> 16) + sy;
    if (y < -4) {                                       /* above the screen: an arrow at the top */
        spr(SPR_UPARROW, x - 4, 2, 3, 0);
        return;
    }
    if (w->p.kind == PK_BIG) spr(SPR_BIGBALL, x - 8, y - 8, 2, 0);
    else spr(w->p.kind == PK_ICE ? SPR_ICE : SPR_BALL, x - 4, y - 4, 2, 0);
}

static void draw_trees(const world *w, int vx0, int sx, int sy)
{
    for (int i = 0; i < w->ntrees; i++) {
        const tree *t = &w->trees[i];
        if (t->on) spr(t->kind ? SPR_TREE1 : SPR_TREE0, t->x - 8 - vx0 + sx, t->y - 32 + 2 + sy, 1, 0);
    }
}

static void draw_flakes(int vx0, int t)
{
    int W = rs_screen_width() + 16;
    for (int i = 0; i < FLAKES; i++) {
        int speed = 2 + i % 3, far = i & 1;
        int y = (i * 53 + t * speed / 4) % 256 - 12;
        int x = (i * 97 + (int)(snow_drift >> 6) * speed / 3 + (int)(isin(t * 2 + i * 40) >> 14) - vx0 * (far ? 1 : 2) / 2)
                % W;
        if (x < 0) x += W;
        spr(SPR_FLAKE0 + far, x - 8 + rs_screen_left(), y, 2, 0);
    }
}

/* the HUD: the active team's icon and the turn's seconds, the throwable, the wind, the score (1 player) */
static void draw_hud(const world *w, int t)
{
    const side *sd = &w->s[w->cur_side];
    if (w->phase == PH_AIM || w->phase == PH_FLIGHT || w->phase == PH_TIMEUP) {
        spr(crit_spr(sd->species, P_ICON), 4, 4, 3, 0);
        if (w->phase == PH_AIM) {
            int secs = (w->timer + 59) / 60;
            if (secs > 5 || hu_blink(t * 2)) hu_number(secs, 36, 4, 3);
        }
        spr(SPR_WPN0 + w->weapon, 4, 22, 3, 0);
    }
    /* the wind: the sock (drooping when calm) and a chevron per 2 levels (rounded up), the way it blows */
    int wd = w->wind, n = ((wd < 0 ? -wd : wd) + 1) / 2;
    spr(wd ? SPR_SOCK : SPR_SOCK_CALM, 152, 3, 3, wd < 0 ? RS_SPR_HFLIP : 0);
    for (int i = 0; i < n; i++) spr(SPR_CHEVRON, wd > 0 ? 170 + i * 6 : 142 - i * 6, 7, 3, wd < 0 ? RS_SPR_HFLIP : 0);
    if (w->players == 1) hu_number_at(w->s[0].score, 314, 4, 1, 3);
}

/* the field's map: the critters and the throw as dots, the camera's view as brackets */
static void draw_minimap(const world *w, const camera *cam)
{
    int x0 = MM_COL * 8;
    for (int i = 0; i < w->ncrit; i++) {
        const critter *c = &w->c[i];
        if (c->state == CS_SNOWED || c->state == CS_SPLASHED) continue;
        int my = ((c->y >> 16) - 6 - MM_Y0) / 6;
        spr(SPR_MAPDOT0 + species_of_player(w->s[c->side].species), x0 + (c->x >> 16) / 4 - 4,
            MM_ROW * 8 + (my < 0 ? 0 : my > MM_H - 1 ? MM_H - 1 : my) - 4, 3, 0);
    }
    if (w->p.on) {
        int my = ((w->p.y >> 16) - MM_Y0) / 6;
        spr(SPR_MAPBALL, x0 + (w->p.x >> 16) / 4 - 4, MM_ROW * 8 + (my < 0 ? 0 : my > MM_H - 1 ? MM_H - 1 : my) - 4, 3, 0);
    }
    int l = camera_left(cam, rs_screen_width()) / 4, r = (camera_left(cam, rs_screen_width()) + rs_screen_width()) / 4;
    spr(SPR_MAPVIEW, x0 + l - 3, MM_ROW * 8, 3, 0);
    spr(SPR_MAPVIEW, x0 + r - 5, MM_ROW * 8, 3, RS_SPR_HFLIP);
}

static void draw_button(const world *w, const touch_view *tv)
{
    int bx, by;
    touch_button_pos(&bx, &by);
    bx -= rs_screen_origin_x();
    int q = tv->btn_on ? SPR_BTN_QP : SPR_BTN_Q, d = tv->btn_on ? 1 : 0;
    spr(SPR_WPN0 + w->weapon, bx - 8, by - 8 + d, 3, 0);
    spr(q, bx - 16, by - 16 + d, 3, 0);
    spr(q, bx, by - 16 + d, 3, RS_SPR_HFLIP);
    spr(q, bx - 16, by + d, 3, RS_SPR_VFLIP);
    spr(q, bx, by + d, 3, RS_SPR_HFLIP | RS_SPR_VFLIP);
}

/* ---- the BG texts ------------------------------------------------------------------------------------------------ */
static void standing(const world *w, hu_standing *s)
{
    int key[MAX_PLAYERS], val[MAX_PLAYERS];
    for (int p = 0; p < w->nsides && p < MAX_PLAYERS; p++) {
        key[p] = side_rank_key(w, p);
        val[p] = w->s[p].score;
    }
    hu_rank(s, w->nsides, key, val);
}

static const char *msg_text(int m)
{
    switch (m) {
    case MSG_SPLASH: return TR(SPLASH);
    case MSG_SNOWED: return TR(SNOWED);
    case MSG_DOUBLE: return TR(DOUBLE);
    case MSG_LONG: return TR(LONG_SHOT);
    case MSG_TIMEUP: return TR(TIME_UP);
    case MSG_FROST: return TR(FROST);
    case MSG_WON: return TR(MATCH_WON);
    case MSG_DRAW: return HTR(DRAW);
    default: return NULL;
    }
}

static int show_hint(const world *w)
{
    return w->level == 1 && w->turns <= w->nsides && world_human_turn(w) >= 0 && w->phase == PH_AIM &&
           !w->s[w->cur_side].damage;
}

/* which texts the screen shows now (a change redraws the UI layer) */
static uint32_t text_key(const world *w, int state, int st_t, int best)
{
    uint32_t screen = state == ST_DEAD ? ST_PLAY : (uint32_t)state, k = screen;
    if (screen == ST_PLAY) {
        int hint = show_hint(w);
        k = k * 7 + (w->banner_t > 0 ? 1 + (w->level & 15) : 0);
        k = k * 31 + w->cur_side * 3 + (w->banner_t > 0);
        k = k * 11 + (w->msg_t > 0 ? w->msg : 0);
        k = k * 3 + hint;
        k = k * 5 + (w->phase == PH_AIM || w->phase == PH_FLIGHT || w->phase == PH_TIMEUP ?
                     (uint32_t)(w->s[w->cur_side].stock[w->weapon] + 2) : 0);
    }
    if (state == ST_OVER) k = k * 2 + (st_t >= RETRY_LOCK);
    return k * 4099u + (uint32_t)best;
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    uint32_t key = text_key(w, state, st_t, best);
    if (key != shown_key) {
        hu_clear();
        if (state == ST_TITLE) hu_logo("SNOWBALL SIEGE", title_ramps, 2, 2, 0);
        if (state == ST_PLAY || state == ST_DEAD) {
            char s[96];
            minimap_put();
            const side *sd = &w->s[w->cur_side];
            if (w->phase == PH_AIM || w->phase == PH_FLIGHT || w->phase == PH_TIMEUP) {
                int n = sd->stock[w->weapon];
                if (n >= 0) {
                    snprintf(s, sizeof s, "X%d", n);
                    hu_text(3, 4, s);
                }
            }
            if (w->banner_t > 0 && w->phase != PH_END) {
                if (w->turns == 1 && w->players == 1) {
                    rs_strf(s, sizeof s, TR(MATCH_N), w->level);
                    hu_big(hu_center(s, 1), 7, s, HU_BIG_FREE);
                }
                if (sd->cpu) snprintf(s, sizeof s, "%s", TR(TURN_CPU));
                else if (w->players == 1) snprintf(s, sizeof s, "%s", TR(TURN_YOU));
                else {
                    char tag[16];
                    rs_strf(tag, sizeof tag, HTR(PLAYER_TAG), sd->player + 1);
                    rs_strf(s, sizeof s, TR(TURN_P), tag);
                }
                hu_text(hu_center(s, 0), 5, s);
            }
            const char *m = w->msg_t > 0 ? msg_text(w->msg) : NULL;
            if (m && !(w->banner_t > 0 && w->turns == 1 && w->players == 1)) hu_big(hu_center(m, 1), 7, m, HU_BIG_FREE);
            if (show_hint(w)) {
                const char *h1 = TR(HINT_AIM), *h2 = TR(HINT_THROW);
                int wd = hu_width(h1, 0) > hu_width(h2, 0) ? hu_width(h1, 0) : hu_width(h2, 0);
                int x0 = 20 - (wd + 3) / 2;
                hu_panel(x0, 20, wd + 3, 6);
                hu_box_text(hu_center(h1, 0), 22, h1);
                hu_box_text(hu_center(h2, 0), 24, h2);
            }
        }
        if (state == ST_OVER) {
            if (w->players == 1) {
                char s[64];
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->s[0].score, 0);
                rs_strf(s, sizeof s, TRN(MATCHES_N, w->wins), w->wins);
                hu_gameover_note(s);
            } else {
                hu_results_panel(rs, NULL);
            }
        }
        shown_key = key;
    }
    if (state == ST_TITLE) hu_title_draw(st_t, best);
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
}

void draw_frame(const world *w, const camera *cam, int state, int st_t, int best, int new_best, int paused,
                const touch_view *tv)
{
    static const int medals[4] = MEDAL_SCORES;
    int t = (int)rs_frame_count(), sx = hu_shake_x(), sy = hu_shake_y();
    int vx0 = cam->x - RS_SCREEN_W / 2;                  /* the world x at stage x 0 */
    hu_standing rs;
    standing(w, &rs);
    field_sync(w);
    field_palettes(state == ST_TITLE);
    rs_bg_scroll(RS_BG2, vx0 - sx, -sy);
    rs_bg_scroll(RS_BG3, (vx0 / 2) & (FAR_W * 8 - 1), 0);
    if (!paused) snow_drift += w->wind * 4 + 6;
    screen_text(w, state, st_t, best, new_best, &rs);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;     /* the panel slides up, ease-out */
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);

    rs_oam_clear();
    int play = state == ST_PLAY || state == ST_DEAD;
    /* front to back: the UI, the HUD and the map, the touch button, the aim, the throw, the critters, effects,
     * the snowflakes, the trees */
    if (state == ST_OVER) {
        if (w->players == 1) {
            hu_gameover_sprites(1, w->s[0].score, 0, best, hu_medal_of(w->s[0].score, medals), st_t, slide);
        } else {
            hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(crit_spr(w->s[rs.order[i]].species, P_ICON), cx - 8, cy - 8 + slide, 3, 0);
            }
        }
    }
    if (state == ST_TITLE) {
        hu_title_sprites(st_t, slot_icon, NULL);   /* the prompt's glyph, the joined players popping in */
        for (int i = 0; i < 4; i++) {              /* the four teams under the logo, throwing in turn */
            int th = ((st_t / 40) & 3) == i && (st_t % 40) < 16;
            spr(crit_spr(i, th ? P_THROW : P_IDLE0 + ((st_t / 32 + i) & 1)), 160 - 46 + i * 24,
                86 + hu_bob(st_t + i * 16, 64, 2), 2, i >= 2 ? RS_SPR_HFLIP : 0);
        }
    }
    if (play) {
        draw_hud(w, t);
        draw_minimap(w, cam);
        if (tv->seen && world_human_turn(w) == 0 && w->phase == PH_AIM) draw_button(w, tv);
        if (w->phase == PH_AIM) draw_aim(w, vx0, sx, sy, t, tv);
        draw_proj(w, vx0, sx, sy);
    }
    draw_critters(w, vx0, sx, sy, t, play);
    draw_fx(w, vx0, sx, sy);
    draw_flakes(cam->x, t);
    draw_trees(w, vx0, sx, sy);
}

/* ---- save states (main.c) ---- */
/* a state saved at another width brings a UI map of another size: set the maps up again, redraw the UI */
void draw_state_loaded(void)
{
    if (rs_bg_map_w(RS_BG1) == map_w()) return;
    maps_setup();
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(line_col); S(cell_map); S(pool_h1); S(pool_h2); S(pool_refs); S(seen_gen); S(seen_seq); S(seen_seed);
    S(shown_key); S(dimmed); S(pool_full); S(snow_drift);
    RS_STATE_RASTER(raster);
}
#undef S
