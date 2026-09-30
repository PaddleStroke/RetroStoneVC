/*
 * Duck Parade: video. The house layout (docs/art-direction.md), top-down:
 *   BG1 the UI (the house kit, priority 1); BG3 the lanes (grass, roads, rapids, rails, paths, ponds), drawn
 *   column by column as the camera goes; BG2 the sky band on top (high priority, over the world's sprites; its
 *   sky colour is the house raster gradient) and the cloud shadows drifting over the field (low priority,
 *   subtracted); sprites: the ducks (OBJ 0, Father OBJ 1 = the same tiles recoloured), ducklings and effects
 *   (OBJ 2), the kit (OBJ 3: digits, glyphs; our egg medals), traffic (OBJ 4-5), river things (OBJ 6), the
 *   train, the lights and the fox (OBJ 7).
 * Nothing here changes the game (the effects are cosmetic).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 */
#include "dp.h"
#include "assets.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>

/* the title logo: one 3-shade ramp per word (house_style.py ACCENTS "duck", a band-uniform red) */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 244, 150), RS_RGB8(252, 214, 64), RS_RGB8(226, 164, 28)},
    {RS_RGB8(255, 176, 164), RS_RGB8(236, 90, 78), RS_RGB8(178, 44, 54)}};
/* the sky band: the raster gradient (house rule: lighter at the horizon, a dither step every 8 lines) */
#define SKY_TOP     0x5aa8e6
#define SKY_HORIZON 0xc4e6fa
#define BAND_ROWS   2           /* tile rows of the sky band */

static rs_color line_col[FIELD_Y];
static int16_t bg2_dx[RS_SCREEN_H];
static int32_t drawn_col[32];    /* the world column drawn in each map slot (-1 none) */
static int8_t drawn_shift[32];   /* the rapids' shift drawn there */
static int shown_state = -1, shown_best = -1, shown_players = -1;
static int facing[MAX_PLAYERS];  /* the last hop's direction (the sprite faces it) */
static int math_mode;            /* 0 cloud shadows, 1 the banking flash */
static int flash_k;              /* the flash's strength 0..31 */
static int fog_w;                /* the fox's red edge, px */
static int hud_score = -1, hud_line = -1, hud_best = -1, hud_score2 = -1;
static int photo_t = -1, photo_n, photo_p;
static int scroll_reg;           /* the BG3 scroll register as written (the bot reads the screen through it) */

int draw_scroll_x(void) { return scroll_reg; }

/* ---- the raster: the sky gradient, the band's clouds, the math per region ------------------------------------ */
static void raster(int line, void *user)
{
    (void)user;
    if (line < FIELD_Y) {
        rs_pal_set(RS_PAL_BG(PAL_CLOUD) + SKY_IDX, line_col[line]);
        if (line == 0) rs_math(RS_MATH_OFF, 0, 0);
    } else if (line == FIELD_Y) {
        if (math_mode == 1) rs_math(RS_MATH_ADD | RS_MATH_FIXED, RS_MATH_BG3 | RS_MATH_BG2, RS_RGB(flash_k, flash_k, flash_k));
        else if (math_mode == 2) rs_math(RS_MATH_ADD | RS_MATH_HALF | RS_MATH_FIXED, RS_MATH_BG3 | RS_MATH_BG2, RS_HEX(0xf4f0dc));
        else rs_math(RS_MATH_ADD | RS_MATH_HALF, RS_MATH_BG2, 0);
    }
}

static void sky_gradient(void)
{
    int a[3] = {(SKY_TOP >> 16) & 255, (SKY_TOP >> 8) & 255, SKY_TOP & 255};
    int b[3] = {(SKY_HORIZON >> 16) & 255, (SKY_HORIZON >> 8) & 255, SKY_HORIZON & 255};
    for (int y = 0; y < FIELD_Y; y++) {
        int k = y * 256 / (FIELD_Y - 1), c[3];
        for (int i = 0; i < 3; i++) c[i] = (a[i] * (256 - k) + b[i] * k) / 256;
        line_col[y] = RS_RGB8(c[0] + ((y & 4) ? 4 : 0), c[1] + ((y & 4) ? 2 : 0), c[2] + ((y & 4) ? 4 : 0));
    }
}

/* ---- set-up -------------------------------------------------------------------------------------------------------- */
void draw_init(void)
{
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0 on palette 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    rs_bg_setup(RS_BG1, 64, 32, VR_BG1);
    rs_bg_setup(RS_BG2, 64, 32, VR_FAR);
    rs_bg_setup(RS_BG3, 64, 32, VR_FIELD);
    rs_tiles_load(VR_FIELD, dp_field_tiles, dp_field_tile_count);
    rs_tiles_load(VR_FAR, dp_far_tiles, dp_far_tile_count);
    for (int p = 1; p < 8; p++)
        if (p != PAL_LOGO) rs_pal_load(RS_PAL_BG(p), dp_bg_pals[p], 16);
    /* the sky band (BG2 rows 0-1, high priority: over the world's sprites) and the cloud shadows (rows 2-29) */
    for (int y = 0; y < BAND_ROWS; y++)
        for (int x = 0; x < 64; x++) {
            uint16_t e = dp_sky_map[y * 64 + x];
            rs_bg_put(RS_BG2, x, y, (uint16_t)(e | RS_MAP_PRIO));
        }
    for (int y = BAND_ROWS; y < 32; y++)
        for (int x = 0; x < 64; x++) rs_bg_put(RS_BG2, x, y, dp_shadow_map[((y - BAND_ROWS) & 15) * 32 + (x & 31)]);
    for (int l = 0; l < 3; l++) rs_bg_enable(l, 1);
    rs_bg_enable(RS_BG4, 0);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, dp_obj_tiles, dp_obj_tile_count);
    for (int p = 0; p < 8; p++)
        if (p != 3) rs_pal_load(RS_PAL_OBJ(p), dp_obj_pals + p * 16, 16);
    sky_gradient();
    rs_bg_line_scroll(RS_BG2, bg2_dx, NULL);
    rs_raster(raster, NULL);
    for (int i = 0; i < 32; i++) drawn_col[i] = -1;
}

void draw_reset(const world *w)
{
    (void)w;
    for (int i = 0; i < 32; i++) drawn_col[i] = -1;
    for (int p = 0; p < MAX_PLAYERS; p++) facing[p] = HOP_FWD;
    hud_score = hud_line = hud_best = hud_score2 = -1;
    photo_t = -1;
}

/* the blank tile 0 of the band must be the sky: the band's empty entries use the sky colour's tile */
/* ---- the field: one column of cells into BG3 ------------------------------------------------------------------------ */
static uint32_t hash2(int32_t a, int b) { uint32_t h = (uint32_t)a * 2654435761u ^ (uint32_t)b * 40503u; h ^= h >> 13; h *= 0x5bd1e995u; return h ^ (h >> 15); }

static int kind_at(const world *w, int32_t col)
{
    const lane *l = world_lane(w, col);
    return l ? l->kind : LK_GRASS;
}

static int river_shift(const lane *l, uint32_t t)
{
    int off = lane_offset(l, t) & 15;
    return l->down ? off : (16 - off) & 15;
}

static void put_cell(int slot, int row, const uint16_t m[4])
{
    int tx = slot * 2, ty = BAND_ROWS + row * 2;
    rs_bg_put(RS_BG3, tx, ty, m[0]);
    rs_bg_put(RS_BG3, tx + 1, ty, m[1]);
    rs_bg_put(RS_BG3, tx, ty + 1, m[2]);
    rs_bg_put(RS_BG3, tx + 1, ty + 1, m[3]);
}

static int grass_meta(int32_t col, int row, int deco)
{
    uint32_t h = hash2(col, row + deco * 31);
    return META_GRASS0 + (int)(h % 6);
}

static int blocked_meta(int32_t col, int row)
{
    uint32_t h = hash2(col * 7 + 3, row);
    int k = (int)(h % 10);
    return k < 6 ? META_TREE0 + k % 3 : k < 9 ? META_BUSH0 + (k & 1) : META_ROCK;
}

static void draw_column(const world *w, int32_t col, int slot)
{
    const lane *l = world_lane(w, col);
    int kind = l ? l->kind : LK_GRASS;
    int lk = kind_at(w, col - 1), rk = kind_at(w, col + 1);
    for (int r = 0; r < ROWS; r++) {
        int m = META_GRASS0;
        switch (kind) {
        case LK_HEDGE: m = META_HEDGE0 + (int)(hash2(col, r) & 1); break;
        case LK_GRASS:
            m = l && (l->block >> r & 1) ? blocked_meta(col, r) : grass_meta(col, r, l ? l->deco : 0);
            break;
        case LK_ROAD: {
            int bike = l->deco == 1;
            int left = lk == LK_ROAD ? 1 : 0, right = rk == LK_ROAD ? 1 : 0;   /* 1: another lane beside */
            int dash = !(r & 1);
            static const int road[2][2][2] = {
                {{META_ROAD_KERB_KERB_0, META_ROAD_KERB_KERB_0}, {META_ROAD_KERB_PLAIN_0, META_ROAD_KERB_PLAIN_0}},
                {{META_ROAD_DASH_KERB_0, META_ROAD_DASH_KERB_1}, {META_ROAD_DASH_PLAIN_0, META_ROAD_DASH_PLAIN_1}}};
            static const int bk[2][2][2] = {
                {{META_BIKE_KERB_KERB_0, META_BIKE_KERB_KERB_0}, {META_BIKE_KERB_PLAIN_0, META_BIKE_KERB_PLAIN_0}},
                {{META_BIKE_DASH_KERB_0, META_BIKE_DASH_KERB_1}, {META_BIKE_DASH_PLAIN_0, META_BIKE_DASH_PLAIN_1}}};
            m = bike ? bk[left][right][dash] : road[left][right][dash];
            if (bike && !left && !right && (r == 3 || r == 10)) m = META_BIKE_SYM;
            break;
        }
        case LK_RAIL: m = META_RAIL0 + (int)(hash2(col, r) % 5 == 0); break;
        case LK_PARK: {
            int lg = lk != LK_PARK, rg = rk != LK_PARK;
            static const int pm[2][2] = {{META_PATH_00_0, META_PATH_01_0}, {META_PATH_10_0, META_PATH_11_0}};
            m = pm[lg][rg] + (int)(hash2(col, r) & 1);
            break;
        }
        case LK_POND:
            m = (l->block >> r & 1) ? META_PONDPAD0 + (int)(hash2(col, r) % 3) : META_POND0 + (int)(hash2(col, r) % 3);
            break;
        case LK_NEST:
            m = r < 2 ? META_NEST_REEDS_TOP0 + (r & 1) : r >= ROWS - 2 ? META_NEST_REEDS_BOTTOM0 + (r & 1) :
                r == 6 ? META_NEST_ISLAND_TOP : r == 7 ? META_NEST_ISLAND_BOTTOM : META_NEST_WATER0 + (r & 1);
            break;
        case LK_RIVER: {
            int s = river_shift(l, w->t);
            int lb = lk != LK_RIVER, rb = rk != LK_RIVER;
            put_cell(slot, r, dp_river[(s + r * 0) & 15][lb][rb]);
            continue;
        }
        }
        put_cell(slot, r, dp_meta[m]);
    }
    drawn_col[slot] = col;
    drawn_shift[slot] = (int8_t)(l && kind == LK_RIVER ? river_shift(l, w->t) : -1);
}

static void field(const world *w, int cam)
{
    int c0 = cam >> 4;
    for (int32_t c = c0 - 1; c <= c0 + SCREEN_COLS + 1; c++) {
        if (c < 0) continue;
        int slot = (int)(c & 31);
        const lane *l = world_lane(w, c);
        if (drawn_col[slot] != c) { draw_column(w, c, slot); continue; }
        if (l && l->kind == LK_RIVER && drawn_shift[slot] != river_shift(l, w->t)) draw_column(w, c, slot);
    }
}

/* ---- cosmetic effects ---------------------------------------------------------------------------------------------------- */
typedef struct fx_obj { int16_t kind, t, life; int32_t x, y, vx, vy; } fx_obj;   /* x, y: Q8 world px (field y) */
enum { FX_NONE, FX_FEATHER, FX_SPLASH, FX_PUFF, FX_HEART, FX_NOTE };
#define FX_MAX 40
static fx_obj fx[FX_MAX];
static rs_rng fx_rng;

static void fx_add(int kind, int x, int y, int vx, int vy, int life)
{
    for (int i = 0; i < FX_MAX; i++)
        if (fx[i].kind == FX_NONE) {
            fx[i].kind = (int16_t)kind;
            fx[i].t = 0;
            fx[i].life = (int16_t)life;
            fx[i].x = x << 8;
            fx[i].y = y << 8;
            fx[i].vx = vx;
            fx[i].vy = vy;
            return;
        }
}

static void fx_burst(int kind, int x, int y, int n, int spread)
{
    for (int k = 0; k < n; k++)
        fx_add(kind, x + rs_rng_range(&fx_rng, 9) - 4, y + rs_rng_range(&fx_rng, 7) - 3,
               rs_rng_range(&fx_rng, spread * 2 + 1) - spread, -rs_rng_range(&fx_rng, spread) - 40, 30 + rs_rng_range(&fx_rng, 20));
}

static void fx_events(const world *w)
{
    for (int p = 0; p < w->players; p++) {
        int ev = w->events[p];
        const duck *d = &w->d[p];
        int x, y, lift, sq;
        duck_visual(w, d, &x, &y, &lift, &sq);
        if (ev & EV_HOP) facing[p] = d->h.dir;
        if (ev & EV_LAND) {
            const lane *l = world_lane(w, d->at.col);
            if (l && (l->kind == LK_GRASS || l->kind == LK_PARK)) fx_add(FX_PUFF, x + 4, y + 12, 0, -10, 14);
            if (l && (l->kind == LK_NEST || l->kind == LK_RIVER)) fx_add(FX_SPLASH, x, y + 2, 0, 0, 18);
        }
        if (ev & EV_HIT) fx_burst(FX_FEATHER, d->death_col + 4, d->death_y + 4, 6, 120);
        if (ev & (EV_SWEPT)) fx_add(FX_SPLASH, d->death_col, d->death_y, 0, 0, 24);
        if (ev & EV_KNOCK) fx_burst(FX_FEATHER, w->ev_knock_x[p] + (w->cam >> 8) - 4, y, 3, 90);
        if (ev & EV_PICK) fx_add(FX_HEART, x + 4, y - 4, 0, -40, 40);
        if (ev & EV_BANK) {
            for (int k = 0; k < 5; k++) fx_add(FX_HEART, x - 8 + k * 6, y - 2, (k - 2) * 20, -60 - k * 5, 50);
            fx_add(FX_SPLASH, x, y, 0, 0, 24);
            photo_t = 0;
            photo_n = w->ev_bank_n[p];
            photo_p = p;
        }
    }
}

static void fx_step(const world *w)
{
    (void)w;
    for (int i = 0; i < FX_MAX; i++) {
        fx_obj *f = &fx[i];
        if (f->kind == FX_NONE) continue;
        f->t++;
        f->x += f->vx;
        f->y += f->vy;
        if (f->kind == FX_FEATHER) { f->vy += 6; f->vx = f->vx * 15 / 16; if (f->vy > 60) f->vy = 60; }
        if (f->t >= f->life) f->kind = FX_NONE;
    }
    /* peeps: a waiting duckling now and then shows a note */
    if (rs_rng_range(&fx_rng, 100) < 3)
        for (int k = 0, tries = 0; tries < 6 && k < 1; tries++) {
            const loose *q = &w->ls[rs_rng_range(&fx_rng, LOOSE_MAX)];
            if (q->state == LOOSE_WAIT) { fx_add(FX_NOTE, q->col * CELL + 10, q->row * CELL - 4, 12, -30, 30); k++; }
        }
}

/* ---- sprites ---------------------------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal, int flags)
{
    const dp_sprite_def *d = &dp_spr[id];
    if (x <= -(int)d->w || x >= RS_SCREEN_W || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

static int dir_index(int dir) { return dir == HOP_BACK ? 1 : dir == HOP_UP ? 2 : dir == HOP_DOWN ? 3 : 0; }
static const int duck_base[4] = {SPR_DUCK_RIGHT, SPR_DUCK_LEFT, SPR_DUCK_UP, SPR_DUCK_DOWN};
static const int ling_base[4] = {SPR_LING_RIGHT, SPR_LING_LEFT, SPR_LING_UP, SPR_LING_DOWN};

static int hop_dir_of(const hop *h)
{
    if (h->to.col > h->from.col) return HOP_FWD;
    if (h->to.col < h->from.col) return HOP_BACK;
    return h->to.y < h->from.y ? HOP_UP : HOP_DOWN;
}

static void draw_duck(const world *w, int p, int cam, int sx, int sy, int t)
{
    const duck *d = &w->d[p];
    int pal = p == 1 ? 1 : 0;
    if (d->state == DK_OUT) return;
    if (d->state == DK_HIT) {
        if (d->t < DEATH_ANIM || (t & 4)) spr(SPR_DUCK_HIT, d->death_col - cam + sx, FIELD_Y + d->death_y + sy, 2, pal, 0);
        return;
    }
    if (d->state == DK_SWEPT) {
        int k = d->t < 40 ? (d->t / 10) & 1 : 1;
        if (d->t < 50) spr(SPR_DUCK_SWEPT + k, d->death_col - cam + sx, FIELD_Y + d->death_y + d->t / 8 + sy, 2, pal, 0);
        return;
    }
    if (d->state == DK_CAUGHT) return;                     /* the fox carries her (draw_fox) */
    int x, y, lift, sq;
    duck_visual(w, d, &x, &y, &lift, &sq);
    int di = dir_index(d->h.dir ? d->h.dir : facing[p]);
    int frame = sq > 17 ? 1 : sq < 15 ? 2 : ((t + p * 40) % 180 < 6 && !d->h.dir ? 3 : 0);
    if (d->state == DK_READY) y += hu_bob(t + p * 16, 64, 1);
    spr(duck_base[di] + frame, x - cam + sx, FIELD_Y + y - lift + sy, 2, pal, 0);
    if (lift > 1) spr(SPR_SHADOW + 1, x - cam + sx, FIELD_Y + y + 10 + sy, 2, -1, 0);
}

static void draw_line(const world *w, int p, int cam, int sx, int sy, int t)
{
    const duck *d = &w->d[p];
    for (int k = 0; k < d->nline; k++) {
        const duckling *u = &d->line[k];
        int x, y, lift;
        duckling_visual(w, d, u, &x, &y, &lift);
        int di = u->h.dir ? dir_index(hop_dir_of(&u->h)) : 0;
        if (!u->h.dir && k > 0) {
            /* face the one in front */
            int fx_ = d->line[k - 1].at.col - u->at.col, fy_ = place_field_y(w, &d->line[k - 1].at) - place_field_y(w, &u->at);
            di = fx_ > 0 ? 0 : fx_ < 0 ? 1 : fy_ < 0 ? 2 : fy_ > 0 ? 3 : 0;
        } else if (!u->h.dir) {
            di = dir_index(facing[p]);
        }
        const lane *l = world_lane(w, u->at.col);
        int swim = l && (l->kind == LK_NEST) && !u->h.dir;
        int id = swim ? SPR_LING_PADDLE + ((t / 16 + k) & 1) : ling_base[di] + (lift > 1);
        spr(id, x - cam + sx, FIELD_Y + y - lift + sy + (!u->h.dir && ((t / 16 + k) & 1) ? 0 : 0), 2, -1, 0);
        if (lift > 2) spr(SPR_SHADOW, x - cam + sx, FIELD_Y + y + 10 + sy, 2, -1, 0);
    }
}

static void draw_loose(const world *w, int cam, int sx, int sy, int t)
{
    for (int i = 0; i < LOOSE_MAX; i++) {
        const loose *q = &w->ls[i];
        int x = (q->x >> 8) - cam + sx, y = FIELD_Y + (q->y >> 8) + sy;
        switch (q->state) {
        case LOOSE_WAIT: {
            int k = ((t + q->id * 23) / 20) % 6;
            spr(SPR_LING_PEEP + (k == 2), q->col * CELL - cam + sx, FIELD_Y + q->row * CELL + sy - (k == 3), 2, -1, 0);
            break;
        }
        case LOOSE_TUMBLE:
            spr(SPR_LING_TUMBLE + (q->t / 4) % 4, x, y - (q->t < 10 ? q->t / 2 : (20 - q->t) / 2), 2, -1, 0);
            break;
        case LOOSE_FLY:
            if (q->owner_was & 4) spr(SPR_LING_PADDLE + ((q->t / 8) & 1), x, y, 2, -1, 0);
            else {
                spr(SPR_LING_FLUTTER + ((q->t / 4) & 1), x, y - 6, 2, -1, 0);
                spr(SPR_SHADOW, x, y + 10, 2, -1, 0);
            }
            break;
        case LOOSE_HOME:
            if (q->t >= 0) spr(SPR_LING_PADDLE + ((t / 16 + q->id) & 1), x, y + ((t / 32 + q->id) & 1), 2, -1, 0);
            break;
        }
    }
}

static void draw_fx(int cam, int sx, int sy)
{
    for (int i = 0; i < FX_MAX; i++) {
        const fx_obj *f = &fx[i];
        int x = (f->x >> 8) - cam + sx, y = FIELD_Y + (f->y >> 8) + sy;
        switch (f->kind) {
        case FX_FEATHER: spr(SPR_FEATHER + ((f->t / 6) & 1), x, y, 2, -1, 0); break;
        case FX_SPLASH: spr(SPR_SPLASH + f->t * 3 / (f->life + 1), x, y, 2, -1, 0); break;
        case FX_PUFF: spr(SPR_PUFF + f->t * 3 / (f->life + 1), x, y, 2, -1, 0); break;
        case FX_HEART: if (f->t < f->life - 8 || (f->t & 2)) spr(SPR_HEART, x, y, 2, -1, 0); break;
        case FX_NOTE: spr(SPR_NOTE, x, y, 2, -1, 0); break;
        }
    }
}

static int mover_sprite(int kind, int down, int t, int i, int *dy)
{
    *dy = 0;
    switch (kind) {
    case MV_BIKE: *dy = -2; return SPR_BIKE + (down ? 0 : 2) + ((t / 6 + i) & 1);
    case MV_BUS: return SPR_BUS + !down;
    case MV_JOGGER: *dy = -2; return SPR_JOGGER + (down ? 0 : 4) + (i & 1) * 2 + ((t / 8 + i) & 1);
    case MV_MOWER: *dy = -2; return SPR_MOWER + (down ? 0 : 2) + ((t / 4) & 1);
    case MV_LOG2: return SPR_LOG2;
    case MV_LOG3: return SPR_LOG3;
    case MV_LOG4: return SPR_LOG4;
    case MV_PAD: return SPR_PAD + (i % 3);
    case MV_BOAT: return SPR_BOAT + (down ? 0 : 2) + ((t / 10) & 1);
    default: {
        static const int cars[6] = {SPR_CAR0, SPR_CAR1, SPR_CAR2, SPR_CAR3, SPR_CAR4, SPR_CAR5};
        *dy = -1;
        return cars[clampi(kind - MV_CAR0, 0, 5)] + !down;
    }
    }
}

static void draw_traffic(const world *w, int cam, int sx, int sy, int t, int platforms)
{
    int c0 = cam >> 4;
    for (int32_t c = c0; c <= c0 + SCREEN_COLS; c++) {
        const lane *l = world_lane(w, c);
        if (!l) continue;
        int x = c * CELL - cam + sx;
        if (l->kind == LK_RAIL && !platforms) {
            int y0, y1, ph = train_at(l, w->t, &y0, &y1);
            if (ph == 2) {
                int nc = l->cars;
                for (int k = 0; k < nc; k++) {
                    int y = l->down ? y1 - (k + 1) * TRAIN_CAR_PX : y0 + k * TRAIN_CAR_PX;
                    int id = k == 0 ? SPR_LOCO + !l->down : SPR_CARRIAGE;
                    spr(id, x, FIELD_Y + y + sy, 2, -1, 0);
                }
            }
            continue;
        }
        int is_plat = l->kind == LK_RIVER;
        if (is_plat != platforms || (l->kind != LK_ROAD && l->kind != LK_PARK && l->kind != LK_RIVER)) continue;
        for (int i = 0; i < l->n; i++) {
            int y = field_y_of_loop(mover_pos(l, i, w->t)), dy;
            int id = mover_sprite(l->m[i].kind, l->down, t, i, &dy);
            spr(id, x, FIELD_Y + y + dy + sy, 2, -1, 0);
        }
    }
}

/* the band: crossing lights over the railways, the nest ponds' signs */
static void draw_band(const world *w, int cam, int t)
{
    int c0 = cam >> 4;
    for (int32_t c = c0; c <= c0 + SCREEN_COLS; c++) {
        const lane *l = world_lane(w, c);
        if (!l) continue;
        int x = c * CELL - cam;
        if (l->kind == LK_RAIL) {
            int y0, y1, ph = train_at(l, w->t, &y0, &y1);
            spr(SPR_SIGNAL + (ph ? 1 + ((t / 8) & 1) : 0), x, 0, 3, -1, 0);
        } else if (l->kind == LK_NEST) {
            spr(SPR_NEST_SIGN + ((t / 20) & 1), x, 0 + ((t / 30) & 1), 3, -1, 0);
        }
    }
}

static void draw_fox(const world *w, int cam, int sx, int sy, int t)
{
    int target = 0, st = world_fox_state(w, &target);
    const duck *d = &w->d[target];
    fog_w = 0;
    if (st == 1) {
        int x, y, lift, sq;
        duck_visual(w, d, &x, &y, &lift, &sq);
        int edge = d->at.col * CELL - cam;                /* how close she is: the fox leans in */
        int lean = clampi((FOX_WARN_COLS * CELL + CELL / 2 - edge) / 3, 0, 10);
        spr(SPR_FOX_WATCH + ((t / 24) & 1), -14 + lean + sx, FIELD_Y + y - 6 + sy, 2, -1, 0);
        if ((t / 16) & 1) spr(SPR_EXCLAIM + ((t / 8) & 1), -2 + lean + sx, FIELD_Y + y - 22 + sy, 2, -1, 0);
        fog_w = 4 + ((t / 6) & 3) * 2 + lean;
    } else if (st == 2) {
        int k = d->t;
        int tx = d->death_col - cam, ty = FIELD_Y + d->death_y;
        if (k < FOX_POUNCE) {
            int x = -24 + (tx + 24) * k / FOX_POUNCE;
            spr(SPR_FOX_POUNCE, x + sx, ty - 8 - (k * (FOX_POUNCE - k)) / 20 + sy, 2, -1, 0);
        } else {
            int x = tx - (k - FOX_POUNCE) * 3;
            spr(SPR_FOX_CATCH + ((k / 6) & 1), x - 8 + sx, ty - 8 + sy, 2, -1, RS_SPR_HFLIP);
        }
        fog_w = 10;
    }
}

/* ---- text and the HUD ---------------------------------------------------------------------------------------------------- */
static void hud(const world *w, int state, int best)
{
    char s[24];
    int line = world_line_total(w);
    int score = world_score(w, 0), score2 = w->players > 1 ? world_score(w, 1) : -1;
    if (line != hud_line || best != hud_best || score2 != hud_score2) {
        hu_clear_rows(0, 1);
        hu_clear_rows(1, 1);
        if (state != DS_TITLE) {
            if (w->players == 1) {
                snprintf(s, sizeof s, "%d", line);
                hu_text(3, 1, s);
                snprintf(s, sizeof s, "BEST %d", best);
                hu_text(39 - (int)strlen(s), 1, s);
            } else {
                snprintf(s, sizeof s, "%d", w->d[0].nline);
                hu_text(3, 1, s);
                snprintf(s, sizeof s, "%d", w->d[1].nline);
                hu_text(38 - (int)strlen(s), 1, s);
            }
        }
        hud_line = line;
        hud_best = best;
        hud_score2 = score2;
    }
    hud_score = score;
}

static void hud_sprites(const world *w, int t)
{
    if (w->players == 1) {
        hu_number(world_score(w, 0), RS_SCREEN_W / 2, 0, 3);
        spr(SPR_LING_RIGHT + ((t / 16) & 1), 4, 0, 3, -1, 0);
    } else {
        hu_number(world_score(w, 0), 88, 0, 3);
        hu_number(world_score(w, 1), 232, 0, 3);
        spr(SPR_LING_RIGHT, 4, 0, 3, -1, 0);
        spr(SPR_LING_LEFT, 316 - 24, 0, 3, -1, 0);
        spr(SPR_DUCK_RIGHT, 40, 0, 3, 0, 0);
        spr(SPR_DUCK_LEFT, 264, 0, 3, 1, 0);
    }
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best)
{
    char s[48];
    if (state != shown_state || best != shown_best || w->players != shown_players ||
        (state == DS_OVER && st_t == RETRY_LOCK)) {
        hu_clear();
        hud_line = -1;
        if (state == DS_TITLE) {
            hu_logo("DUCK PARADE", title_ramps, 2, 2, 0);
            snprintf(s, sizeof s, "BEST %d", best);
            if (best > 0) hu_text(hu_center(s, 0), 24, s);
            hu_copyright(28);
        }
        if (state == DS_READY) hu_get_ready(6);
        if (state == DS_OVER) {
            hu_banner(4, "GAME OVER");
            if (w->players == 1) {
                hu_gameover_panel(1, new_best, world_score(w, 0), 0);
            } else {
                hu_panel(10, 9, 20, 12);
                hu_box_text(12, 11, "MOTHER");
                hu_box_text(12, 14, "FATHER");
                hu_box_text(12, 17, "FAMILY");
            }
        }
        shown_state = state;
        shown_best = best;
        shown_players = w->players;
    }
    if (state == DS_TITLE || state == DS_READY) {
        hu_prompt(21, "PRESS A TO HOP", st_t);
        if (state == DS_READY || w->players == 2) hu_join_line(26, w->players, "FAMILY WALK!");
    }
    if (state == DS_OVER) hu_retry_line(st_t, RETRY_LOCK, "A: HOP AGAIN");
}

/* the family photo after a banking: a small panel with the family in it, sliding down, then up */
static void photo(const world *w, int cam, int t)
{
    (void)cam;
    if (photo_t < 0) return;
    int k = photo_t;
    /* the photo: a kit panel (BG1 rows 3-8) with the family standing in it; they pop in one by one */
    if (k == 0) {
        hu_panel(11, 3, 18, 6);
        char s[32];
        snprintf(s, sizeof s, "%d DUCKLINGS HOME!", photo_n);
        hu_box_text(20 - (int)strlen(s) / 2, 7, photo_n ? s : "HOME SWEET HOME");
    }
    int top = 3 * 8 + 6;
    int n = photo_n > 9 ? 9 : photo_n;
    int x0 = 160 - (n + 1) * 7;
    int pop = hu_ease_back(k, 14, 8);
    spr(SPR_DUCK_DOWN + ((t / 20) % 30 == 0 ? 3 : 0), x0 - 4, top + 8 - pop, 3, photo_p == 1 ? 1 : 0, 0);
    for (int i = 0; i < n; i++) {
        int pi = hu_ease_back(k - 3 - i * 2, 12, 8);
        if (k - 3 - i * 2 >= 0) spr(SPR_LING_DOWN + (((t / 10) + i) % 7 == 0), x0 + 12 + i * 14, top + 10 - pi, 3, -1, 0);
    }
    if (photo_n > 9) spr(SPR_HEART, x0 + 12 + 9 * 14, top + 12, 3, -1, 0);
    (void)w;
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    int t = (int)rs_frame_count();
    int cam = world_cam_px(w), sx = hu_shake_x(), sy = hu_shake_y();
    if (!paused) {
        fx_events(w);
        fx_step(w);
    }
    field(w, cam);
    scroll_reg = (cam - sx) & 511;
    rs_bg_scroll(RS_BG3, scroll_reg, -sy);
    /* BG2: the band's clouds drift slowly (1/4 of the camera), the shadows with the camera and the wind */
    int band_dx = (cam / 4 + t / 16) & 511, shade_dx = (cam - sx + t / 4) & 511;
    for (int y = 0; y < RS_SCREEN_H; y++) bg2_dx[y] = (int16_t)(y < FIELD_Y ? band_dx : shade_dx);
    rs_bg_scroll(RS_BG2, 0, 0);
    /* the banking flash: the field brightened, fading */
    if (photo_t >= 0 && photo_t < 10) { math_mode = 1; flash_k = 31 - photo_t * 3; }
    else math_mode = state == DS_TITLE ? 2 : 0;

    screen_text(w, state, st_t, best, new_best);
    if (state == DS_PLAY || state == DS_DEAD || state == DS_READY) hud(w, state, best);
    int slide = state == DS_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);

    rs_oam_clear();
    /* front to back: the HUD and the UI, the photo, the band, the fox, the ducks, the lines, the loose ducklings
     * and the effects, the traffic and the trains, the platforms */
    if (state == DS_PLAY || state == DS_DEAD || state == DS_READY) hud_sprites(w, t);
    if (state == DS_OVER) {
        static const int th[4] = {MEDAL_BRONZE, MEDAL_SILVER, MEDAL_GOLD, MEDAL_PEARL};
        int oy = slide;
        if (w->players == 1) {
            int m = hu_medal_of(world_score(w, 0), th);
            hu_number(world_score(w, 0), 25 * 8, 11 * 8 - 4 + oy, 3);
            hu_number(best, 25 * 8, 14 * 8 - 4 + oy, 3);
            if (m) {
                spr(SPR_EGG + m - 1, 22 * 8 - 4, 16 * 8 - 4 + oy, 3, -1, 0);
                if ((st_t / 20) % 3 == 0) hu_sparkle(22 * 8 + 12, 16 * 8 - 4 + oy, (st_t / 10) % 2, 3);
            } else {
                hu_box_text(22, 17, "-");
            }
        } else {
            hu_number(world_score(w, 0), 25 * 8, 11 * 8 - 4 + oy, 3);
            hu_number(world_score(w, 1), 25 * 8, 14 * 8 - 4 + oy, 3);
            hu_number(world_total(w), 25 * 8, 17 * 8 - 4 + oy, 3);
        }
    }
    if (state == DS_TITLE || state == DS_READY)
        hu_glyph(HU_BTN_A, hu_center("PRESS A TO HOP", 0) * 8 - 12, 21 * 8 - 4, (t / 30) % 2, 3);
    if (state == DS_TITLE) {
        /* Mother and three ducklings march in place under the logo */
        int x = 128, y = 104;
        for (int k = 0; k < 4; k++) {
            int bob = ((t / 8 + k) & 1);
            if (k == 0) spr(SPR_DUCK_RIGHT + (bob ? 1 : 0), x + 40, y - bob * 2, 2, 0, 0);
            else spr(SPR_LING_RIGHT + bob, x + 40 - k * 16, y + 2 - bob * 2, 2, -1, 0);
        }
    }
    if (state != DS_OVER && photo_t >= 0) {
        photo(w, cam, t);
        if (!paused && ++photo_t >= PHOTO_FRAMES) {
            photo_t = -1;
            hu_clear_rows(3, 6);
            hud_line = -1;
            shown_state = -1;          /* redraw the screen's text */
        }
    }
    draw_band(w, cam, t);
    draw_fox(w, cam, sx, sy, t);
    if (fog_w) { rs_window(0, 0, fog_w); rs_fog(RS_WIN1, RS_RGB(28, 6, 4), RS_MATH_BG3); }
    else rs_fog(0, 0, 0);
    for (int p = 0; p < w->players && state != DS_TITLE; p++) draw_duck(w, p, cam, sx, sy, t);
    for (int p = 0; p < w->players; p++) draw_line(w, p, cam, sx, sy, t);
    draw_fx(cam, sx, sy);
    draw_loose(w, cam, sx, sy, t);
    draw_traffic(w, cam, sx, sy, t, 0);
    draw_traffic(w, cam, sx, sy, t, 1);
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(line_col); S(bg2_dx); S(drawn_col); S(drawn_shift); S(shown_state); S(shown_best); S(shown_players); S(facing);
    S(math_mode); S(flash_k); S(fog_w); S(hud_score); S(hud_line); S(hud_best); S(hud_score2); S(photo_t); S(photo_n);
    S(photo_p); S(scroll_reg); S(fx); S(fx_rng);
    RS_STATE_RASTER(raster);
}
#undef S
