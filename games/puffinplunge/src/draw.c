/*
 * Puffin Plunge: video. The layers (back to front):
 *   BG3       the back wall inside the shaft (the zone's dark rock and its glints), at half the speed: depth
 *   BG2       the shaft (BG palette 1: the zone's rock autotiled from 8x8 quarters, its cap of crystals, coral, moss or
 *             snow, the crystal blocks, the side rooms' doors, the bottom's floor) and the rock beside it (BG palette 2,
 *             darker: the side panels, with the zone's decorations); the map is anchored on the world and written row
 *             by row as the camera goes down (64 rows of 8 px: a phone held upright's 480 lines fit)
 *   sprites   prio 2: the puffin, the critters, the walrus, the shots, the beam, the gems, the effects
 *   BG1       the house kit: the title, the level's card, the shops, the results, PAUSED; prio 3 sprites: the HUD
 *             (hearts, the gems, the bubbles' gauge, the depth), the kit's glyphs and digits, the shops' icons
 * The camera follows the puffin a third of the way down the screen and looks ahead of a fall (world.c's look: the same
 * at every size). Landscape and the console: the shaft (192 px) in the middle, the rock and the HUD on its sides; a
 * wider screen shows more rock. The tall screen (a phone held upright): rs_screen_zoom 480 (main.c), the stage's bottom
 * at the screen's bottom (the touch buttons' band), the shaft fills the columns shown, the HUD at the screen's top.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_tall.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

#define GSTR(id) rs_tr(&gs_strings, (id))
#define SHAFT_X      ((RS_SCREEN_W - SHAFT_W) / 2)     /* the shaft's left edge, stage x (64) */
#define OAM_BUDGET   124            /* the guideline 128, 4 left for safety */
#define LINE_BUDGET  30             /* sprites per 8-line band (the guideline 32 per line) */
#define MAX_PART     56
#define MAX_ZIP      12
#define ZIP_FRAMES   16
#define MAPH         64             /* the shaft's and the back wall's map rows */
#define BLANK        0              /* the shaft's tile 0: clear */

static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 214, 120), RS_RGB8(248, 140, 52), RS_RGB8(200, 64, 54)},       /* PUFFIN: the beak */
    {RS_RGB8(170, 236, 255), RS_RGB8(80, 170, 236), RS_RGB8(40, 90, 180)}};      /* PLUNGE: the bubbles */

/* ---- the drawing's state (saved: draw_state) --------------------------------------------------------------------- */
static int laid_w = -1, laid_h = -1, zone_loaded = -1, ui_key = -1, level_drawn = -1;
static int row_cache[MAPH], back_cache[MAPH];   /* the world tile row each map row shows (-99999: none) */
typedef struct part { int32_t x, y; int16_t vx, vy; uint8_t kind, t, life, a; } part;   /* world, Q8 */
enum { P_FLASH, P_CASING, P_PUFF, P_SPARK, P_SHARD, P_POOF, P_POP, P_DUST };
static part parts[MAX_PART];
static int part_next;
typedef struct zip { int16_t x, y; uint8_t t, big; } zip;    /* a gem flying to the counter: from world x, y */
static zip zips[MAX_ZIP];
static int zip_next, tick, gem_pulse, hurt_t, warn_t, shown_gems;
/* per frame (set by draw_frame, read by the raster: not saved) */
static int r_pin;
static int band[RS_SCREEN_H_MAX / 8 + 2], oam_n;   /* the sprite budget of the frame (scratch) */
static int cam;                                     /* the frame's camera: world y of the screen's first line */

static int tall(void) { return rs_screen_height() > RS_SCREEN_H; }
static int map_w(void) { return rs_screen_width() > 496 ? 128 : 64; }

/* ---- names ---------------------------------------------------------------------------------------------------------- */
const char *upgrade_name(int up)
{
    static const int k[UP_COUNT] = {GS_UP_HEART, GS_UP_HEAL, GS_UP_AMMO, GS_UP_SPREAD, GS_UP_BEAM, GS_UP_BURST,
                                    GS_UP_MAGNET, GS_UP_SPRINGS, GS_UP_FEATHER, GS_UP_SHELL, GS_UP_LUCKY};
    return (unsigned)up < UP_COUNT ? GSTR(k[up]) : "";
}
static const char *upgrade_desc(int up)
{
    static const int k[UP_COUNT] = {GS_D_HEART, GS_D_HEAL, GS_D_AMMO, GS_D_SPREAD, GS_D_BEAM, GS_D_BURST,
                                    GS_D_MAGNET, GS_D_SPRINGS, GS_D_FEATHER, GS_D_SHELL, GS_D_LUCKY};
    return (unsigned)up < UP_COUNT ? GSTR(k[up]) : "";
}
const char *zone_name(int z)
{
    static const int k[ZONES] = {GS_ZONE_1, GS_ZONE_2, GS_ZONE_3, GS_ZONE_4};
    return GSTR(k[z & 3]);
}
static void level_str(char *s, int n, int level)
{
    int l = level < 0 ? 0 : level >= LEVELS ? LEVELS - 1 : level;
    if (n >= 4) { s[0] = (char)('0' + l / LEVELS_PER_ZONE + 1); s[1] = '-'; s[2] = (char)('0' + l % LEVELS_PER_ZONE + 1); s[3] = 0; }
}

/* ---- sprites, within the guidelines (128, 32 a line) ------------------------------------------------------------------ */
static void budget_reset(void) { memset(band, 0, sizeof band); oam_n = 0; }
static int spr_raw(int x, int y, int tile, int w, int h, int pal, int prio, int flags)
{
    if (x <= rs_screen_left() - w || x >= rs_screen_right() || y <= rs_screen_top() - h || y >= rs_screen_bottom())
        return 0;
    if (oam_n >= OAM_BUDGET) return 0;
    int b0 = (y - rs_screen_top()) >> 3, b1 = (y + h - 1 - rs_screen_top()) >> 3, nb = (int)(sizeof band / sizeof band[0]);
    if (b0 < 0) b0 = 0;
    if (b1 >= nb) b1 = nb - 1;
    for (int b = b0; b <= b1; b++)
        if (band[b] >= LINE_BUDGET) return 0;
    for (int b = b0; b <= b1; b++) band[b]++;
    oam_n++;
    rs_spr(x, y, tile, w, h, pal, prio, flags);
    return 1;
}
static void spr(int id, int x, int y, int prio, int flags)
{
    const pp_sprite_def *d = &pp_spr[id];
    spr_raw(x, y, d->tile, d->w, d->h, d->pal, prio, flags);
}
static void spr_pal(int id, int x, int y, int pal, int prio, int flags)
{
    const pp_sprite_def *d = &pp_spr[id];
    spr_raw(x, y, d->tile, d->w, d->h, pal, prio, flags);
}
static void budget_kit(int y, int h)               /* the kit draws its own sprites (digits, glyphs): count them in */
{
    int b0 = (y - rs_screen_top()) >> 3, b1 = (y + h - 1 - rs_screen_top()) >> 3, nb = (int)(sizeof band / sizeof band[0]);
    if (b0 < 0) b0 = 0;
    if (b1 >= nb) b1 = nb - 1;
    for (int b = b0; b <= b1; b++) band[b]++;
    oam_n++;
}
/* small digits (8x8 sprites, 5 px apart): s of "0123456789x+-"; returns the width */
static int small_text(const char *s, int x, int y, int gold, int prio)
{
    int x0 = x;
    for (; *s; s++) {
        int id = -1;
        if (*s >= '0' && *s <= '9') id = (gold ? SPR_G_0 : SPR_D_0) + (*s - '0');
        else if (*s == 'x') id = gold ? SPR_G_X : SPR_D_X;
        else if (*s == '+') id = gold ? SPR_G_PLUS : SPR_D_PLUS;
        else if (*s == '-') id = SPR_D_MINUS;
        if (id >= 0) spr(id, x - 2, y - 1, prio, 0);
        x += 5;
    }
    return x - x0;
}

/* ---- the world on the screen ------------------------------------------------------------------------------------------- */
static int sx(int wx) { return wx + SHAFT_X; }
static int sy(int wy) { return wy - cam + rs_screen_top(); }

static int camera(const world *w, int title)
{
    if (title) return 0;
    int hgt = rs_screen_height(), line = hgt * 3 / 10;
    int c = w->h.y / SUB - line + w->look / SUB;
    int maxc = w->rows * CELL - hgt;
    if (c > maxc) c = maxc;
    if (c < 0) c = 0;
    return c;
}

/* ---- the raster: the UI layer on the stage's lines, or pinned on a tall screen ------------------------------------------- */
static void raster(int line, void *user)
{
    (void)user;
    rs_bg_enable(RS_BG1, ht_pin_line(r_pin, line));
}

/* ---- set-up ----------------------------------------------------------------------------------------------------------- */
static void flash_palette(void)
{
    rs_color c[16];
    for (int i = 0; i < 16; i++) c[i] = RS_RGB8(255, 255, 255);
    c[0] = 0;
    c[1] = RS_RGB8(60, 30, 80);
    rs_pal_load(RS_PAL_OBJ(OBJ_FLASH), c, 16);
}

static void layers_setup(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG2, mw, MAPH, VR_SHAFT);
    rs_bg_setup(RS_BG3, 32, MAPH, VR_SHAFT);
    rs_bg_enable(RS_BG4, 0);
    laid_w = rs_screen_width();
    laid_h = rs_screen_height();
    for (int i = 0; i < MAPH; i++) row_cache[i] = back_cache[i] = -99999;
    ui_key = -1;
    /* the stage centred across; upright, its bottom at the screen's bottom (the touch buttons' band) */
    rs_screen_origin(rs_screen_margin());
    rs_screen_origin_v(tall() ? rs_screen_height() - RS_SCREEN_H : 0);
}

static void load_zone(int z)
{
    if (z == zone_loaded) return;
    static const uint8_t blank[32];
    rs_tiles_load(VR_SHAFT, blank, 1);
    rs_tiles_load(VR_SHAFT + 1, pp_terrain[z], T_COUNT);
    rs_tiles_load(VR_SHAFT + 1 + T_COUNT, pp_back[z], BACK_TILES);
    rs_pal_load(RS_PAL_BG(PAL_ROCK), pp_pal_terrain[z], 16);
    rs_color side[16];
    for (int i = 0; i < 16; i++) side[i] = hu_scale_color(pp_pal_terrain[z][i], 150);   /* the side panels: darker */
    side[1] = pp_pal_terrain[z][1];
    rs_pal_load(RS_PAL_BG(PAL_SIDE), side, 16);
    rs_pal_load(RS_PAL_BG(PAL_BACK), pp_pal_back[z], 16);
    rs_pal_load(RS_PAL_OBJ(OBJ_CRIT), pp_pal_crit[z], 16);
    rs_backdrop(pp_pal_back[z][1]);
    zone_loaded = z;
    for (int i = 0; i < MAPH; i++) row_cache[i] = back_cache[i] = -99999;
}

void draw_init(void)
{
    hu_config hc = hu_defaults();
    hc.obj_tile = KIT_OBJ_TILE;
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = OBJ_KIT;
    hc.logo_tile = LOGO_TILE;
    hc.logo_pal = PAL_LOGO;
    hc.ext_tile = EXT_TILE;
    hc.ext_count = EXT_COUNT;
    hu_init(&hc);
    layers_setup();
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, pp_obj_tiles, pp_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(OBJ_HERO), pp_pal_hero, 16);
    rs_pal_load(RS_PAL_OBJ(OBJ_ITEMS), pp_pal_items, 16);
    rs_pal_load(RS_PAL_OBJ(OBJ_FX), pp_pal_fx, 16);
    rs_pal_load(RS_PAL_OBJ(OBJ_BOSS), pp_pal_walrus, 16);
    flash_palette();
    rs_bg_enable(RS_BG1, 1);
    rs_bg_enable(RS_BG2, 1);
    rs_bg_enable(RS_BG3, 1);
    rs_raster(raster, NULL);
    load_zone(0);
}

void draw_level(const world *w)
{
    load_zone(zone_of(w->level));
    for (int i = 0; i < MAPH; i++) row_cache[i] = back_cache[i] = -99999;
    memset(parts, 0, sizeof parts);
    memset(zips, 0, sizeof zips);
    hurt_t = warn_t = 0;
    shown_gems = w->gems;
    level_drawn = w->level;
    ui_key = -1;
}

void draw_state_loaded(const world *w)
{
    int shape = ht_shape_reset();
    if (!shape && laid_w == rs_screen_width() && laid_h == rs_screen_height()) return;   /* the same screen: as saved */
    layers_setup();
    rs_bg_enable(RS_BG4, 0);
    int z = zone_loaded;
    zone_loaded = -1;
    load_zone(z >= 0 ? z : zone_of(w->level));
    flash_palette();
    hu_pause(0, 13);
}

/* ---- the shaft's map ------------------------------------------------------------------------------------------------------ */
static uint32_t hsh(int a, int b)
{
    uint32_t v = 2166136261u;
    v = (v ^ (uint32_t)a) * 16777619u;
    v = (v ^ (uint32_t)b) * 16777619u;
    v ^= v >> 13;
    v *= 0x5bd1e995u;
    return v ^ (v >> 15);
}
static int fdiv(int a, int b) { return a >= 0 ? a / b : -((b - 1 - a) / b); }

static int cellk(const world *w, int c, int r)
{
    if (c < 0 || c >= COLS || r < 0) return C_ROCK;
    if (r >= w->rows) return C_FLOOR;
    return w->cell[r][c];
}
static int rocky(const world *w, int c, int r)
{
    int k = cellk(w, c, r);
    return k == C_ROCK || k == C_FLOOR || k == C_DOOR || k == C_SHUT;
}
static int deco_at(int c, int r)                      /* a side panel's decoration in cell (c, r): its kind + 1 */
{
    if ((c >= -1 && c <= COLS) || (hsh(c * 3, r * 5) % 11)) return 0;
    if (!(hsh(c, r) & 1) && ((r + c) & 1)) return 0;
    return 1 + (int)(hsh(r, c) % 3);
}

static uint16_t shaft_entry(const world *w, int tx, int ty)
{
    int c = fdiv(tx, 2), r = fdiv(ty, 2), qx = tx & 1, qy = ty & 1, k = cellk(w, c, r);
    int side = c < 0 || c >= COLS, pal = side ? PAL_SIDE : PAL_ROCK;
    if (k == C_EMPTY) return RS_MAP(BLANK, PAL_ROCK, 0, 0, 0);
    if (k == C_CRYSTAL || k == C_CRACKED)
        return RS_MAP(1 + (k == C_CRYSTAL ? T_CRYS0 : T_CRK0) + qy * 2 + qx, PAL_ROCK, 0, 0, 0);
    if (k == C_DOOR || k == C_SHUT) {
        int kind = 2, flip = c == COLS - 1;
        if (k == C_DOOR)
            for (int d = 0; d < w->ndoors; d++)
                if (w->door_row[d] == r && w->door_col[d] == c) kind = w->door_kind[d];
        int base = kind == 0 ? T_DOOR_SHOP0 : kind == 1 ? T_DOOR_GIFT0 : T_DOOR_SHUT0;
        return RS_MAP(1 + base + qy * 2 + (flip ? 1 - qx : qx), PAL_ROCK, 0, flip, 0);
    }
    if (k == C_FLOOR && qy == 0 && cellk(w, c, r - 1) != C_FLOOR && !side)
        return RS_MAP(1 + T_FLOOR_TOP0 + qx, PAL_ROCK, 0, 0, 0);
    if (side) {
        int d = deco_at(c, r);
        if (d) return RS_MAP(1 + T_DECO0_0 + (d - 1) * 4 + qy * 2 + qx, pal, 0, 0, 0);
    }
    /* the rock: a quarter by its two neighbours on that side and the diagonal */
    int dx = qx ? 1 : -1, dy = qy ? 1 : -1;
    int v = rocky(w, c, r + dy), hh = rocky(w, c + dx, r), dg = rocky(w, c + dx, r + dy);
    int type = !v && !hh ? 0 : !v ? 1 : !hh ? 2 : !dg ? 3 : 4;
    int corner = qx + qy * 2;
    int tile = type == 4 ? T_INT0 + (int)(hsh(tx, ty) % 4) : T_Q00 + corner * 5 + type;
    return RS_MAP(1 + tile, pal, 0, 0, 0);
}

static void shaft_rows(const world *w, int force_row)
{
    int mw = map_w(), top = rs_screen_top();
    uint16_t *map = rs_bg_map(RS_BG2);
    int ty0 = fdiv(cam, 8) - 1, ty1 = fdiv(cam + rs_screen_height(), 8) + 1;
    int txl = fdiv(rs_screen_left() - SHAFT_X, 8) - 1;
    for (int ty = ty0; ty <= ty1; ty++) {
        int mr = ty & (MAPH - 1);
        if (row_cache[mr] == ty && fdiv(ty, 2) != force_row) continue;
        row_cache[mr] = ty;
        for (int i = 0; i < mw; i++) {
            int tx = txl + i;
            map[mr * mw + (tx & (mw - 1))] = shaft_entry(w, tx, ty);
        }
    }
    int shx = hu_shake_x(), shy = hu_shake_y();
    rs_bg_scroll(RS_BG2, (-SHAFT_X - shx) & (mw * 8 - 1), (cam - top - shy) & (MAPH * 8 - 1));
}

static void back_rows(void)
{
    uint16_t *map = rs_bg_map(RS_BG3);
    int top = rs_screen_top(), by = cam / 2;
    int ty0 = fdiv(by, 8) - 1, ty1 = fdiv(by + rs_screen_height(), 8) + 1;
    for (int ty = ty0; ty <= ty1; ty++) {
        int mr = ty & (MAPH - 1);
        if (back_cache[mr] == ty) continue;
        back_cache[mr] = ty;
        for (int tx = 0; tx < 32; tx++) {
            uint32_t h = hsh(tx * 7, ty * 13) % 100;
            int patch = hsh(fdiv(tx + (ty / 6) % 2, 3), fdiv(ty, 5)) % 5 < 2;    /* big soft patches of lighter rock */
            int t = (patch ? 4 : 0) + (int)((h >> 2) & 3);
            if (h < 3) t = 8 + (int)(h & 1);            /* the zone's motif */
            else if (h < 4) t = 10;
            else if (h < 6) t = 11;
            map[mr * 32 + tx] = RS_MAP(1 + T_COUNT + t, PAL_BACK, 0, (h & 1), 0);
        }
    }
    rs_bg_scroll(RS_BG3, (-SHAFT_X - hu_shake_x() / 2) & 255, (by - top) & (MAPH * 8 - 1));
}

/* ---- effects (draw_events: after each world step) ------------------------------------------------------------------ */
static void add_part(int kind, int x, int y, int vx, int vy, int life, int a)
{
    part *p = &parts[part_next];
    part_next = (part_next + 1) % MAX_PART;
    *p = (part){x * SUB, y * SUB, (int16_t)vx, (int16_t)vy, (uint8_t)kind, 0, (uint8_t)life, (uint8_t)a};
}

void draw_events(const world *w)
{
    for (int i = 0; i < w->n_dirty; i++) {             /* the cells changed: their map rows again */
        int r = w->dirty[i];
        for (int k = 0; k < MAPH; k++)
            if (fdiv(row_cache[k], 2) == r) row_cache[k] = -99999;
    }
    for (int i = 0; i < w->n_fx; i++) {
        const fxev *f = &w->fx[i];
        int s = (int)(hsh(tick + i, f->x) & 255);
        switch (f->kind) {
        case FX_SHOT:
            add_part(P_FLASH, f->x, f->y, 0, 0, 4, 0);
            add_part(P_CASING, f->x, f->y - 6, (s & 1) ? 300 + s : -300 - s, -420 - s, 26, s & 3);
            break;
        case FX_KILL:
            add_part(P_POOF, f->x, f->y, 0, 0, 12, 0);
            for (int k = 0; k < 4; k++) add_part(P_SPARK, f->x, f->y, (k & 1 ? 1 : -1) * (200 + s), (k & 2 ? 1 : -1) * 260, 12, 0);
            hu_shake(2, 6);
            break;
        case FX_HIT:
            if (f->a == 255) for (int k = 0; k < 2; k++) add_part(P_SHARD, f->x, f->y, (k ? 1 : -1) * 260, -300, 18, k);
            else add_part(P_SPARK, f->x, f->y, 0, -100, 8, 0);
            break;
        case FX_GEM:
            zips[zip_next] = (zip){f->x, f->y, 1, f->a};
            zip_next = (zip_next + 1) % MAX_ZIP;
            break;
        case FX_BREAK:
            for (int k = 0; k < 4; k++) add_part(P_SHARD, f->x, f->y, (k & 1 ? 1 : -1) * (180 + k * 60), -500 + k * 90, 22, k % 3);
            add_part(P_PUFF, f->x, f->y, 0, -60, 10, 0);
            break;
        case FX_LAND:
            add_part(P_DUST, f->x - 5, f->y - 2, -260, -60, 12, 0);
            add_part(P_DUST, f->x + 5, f->y - 2, 260, -60, 12, 0);
            break;
        case FX_JUMP:
            add_part(P_DUST, f->x, f->y - 2, 0, 40, 10, 0);
            break;
        case FX_STOMP:
            add_part(P_POOF, f->x, f->y, 0, 0, 10, 0);
            add_part(P_DUST, f->x - 6, f->y, -300, -100, 10, 0);
            add_part(P_DUST, f->x + 6, f->y, 300, -100, 10, 0);
            hu_shake(1, 5);
            break;
        case FX_HURT:
            hurt_t = 12;
            hu_shake(3, 10);
            add_part(P_POOF, f->x, f->y, 0, 0, 10, 0);
            break;
        case FX_SPORE_POP:
            add_part(P_PUFF, f->x, f->y, 0, -40, 9, 0);
            break;
        default: break;
        }
    }
    for (int i = 0; i < w->n_pops; i++) {
        const pop *p = &w->pops[i];
        add_part(P_POP, p->x, p->y, 0, -170, 40, (uint8_t)((p->kind << 6) | (p->n > 63 ? 63 : p->n)));
    }
    if (w->events & EV_BOSS) warn_t = 150;
    if (w->events & EV_WIN) hu_shake(4, 16);
    /* the effects move (world px: the same at every screen size) */
    for (int i = 0; i < MAX_PART; i++) {
        part *p = &parts[i];
        if (p->t >= p->life) continue;
        p->t++;
        p->x += p->vx;
        p->y += p->vy;
        if (p->kind == P_CASING || p->kind == P_SHARD) p->vy = (int16_t)(p->vy + 40);
        if (p->kind == P_DUST || p->kind == P_POP) p->vy = (int16_t)(p->vy * 7 / 8);
        if (p->kind == P_DUST) p->vx = (int16_t)(p->vx * 7 / 8);
    }
    for (int i = 0; i < MAX_ZIP; i++) if (zips[i].t && ++zips[i].t > ZIP_FRAMES) { zips[i].t = 0; gem_pulse = 8; }
    if (hurt_t) hurt_t--;
    if (warn_t) warn_t--;
    if (gem_pulse) gem_pulse--;
}

/* ---- the world's pictures --------------------------------------------------------------------------------------------------- */
static int en_frame(const enemy *e, int *w16)
{
    int a = ((e->t + e->type * 7) / 10) & 1;
    *w16 = 16;
    switch (e->type) {
    case ET_SNAIL: return SPR_SNAIL + ((e->t / 16) & 1);
    case ET_BAT: case ET_ICEBAT: return SPR_BAT + (e->st == 0 ? 2 : ((e->t / 6) & 1));
    case ET_URCHIN: return SPR_URCHIN + ((e->t / 20) & 1);
    case ET_CRAB: return SPR_CRAB + ((e->t / 8) & 1);
    case ET_FISH: return SPR_FISH + a;
    case ET_JELLY: return SPR_JELLY + ((e->t / 14) & 1);
    case ET_PUFFER: return SPR_PUFFER + (e->st ? 2 : a);
    case ET_BEETLE: return SPR_BEETLE + ((e->t / 9) & 1);
    case ET_MOTH: return SPR_MOTH + ((e->t / 5) & 1);
    case ET_PUFFCAP: return SPR_PUFFCAP + ((e->t % 110) > 50 && (e->t % 110) < 64 ? 1 : 0);
    case ET_SPORE: *w16 = 8; return SPR_SPORE + ((e->t / 6) & 1);
    case ET_GRUB: return SPR_GRUB + ((e->t / 12) & 1);
    case ET_ICICLE: return SPR_ICICLE + (e->st == 1 ? ((e->t / 2) & 1) : 0);
    case ET_WISP: return SPR_WISP + ((e->t / 8) & 1);
    case ET_SNOWBALL: *w16 = 8; return SPR_SNOWBALL + ((e->t / 6) & 1);
    case ET_WALRUS: *w16 = 48; return SPR_WALRUS + (e->flash ? 3 : e->st == 2 ? 2 : (e->t / 10) & 1);
    }
    return SPR_SNAIL;
}

/* the side rooms' signs: a gem (a shop) or a heart (a gift) bobbing over each open door */
static void draw_doors(const world *w)
{
    for (int d = 0; d < w->ndoors; d++) {
        int r = w->door_row[d], c = w->door_col[d];
        if (w->cell[r][c] != C_DOOR) continue;
        int x = sx(c * CELL + 4) + (c ? -6 : 6) + hu_shake_x(), y = sy(r * CELL - 8) + hu_bob(tick + d * 17, 48, 2) + hu_shake_y();
        spr(w->door_kind[d] ? SPR_HEART : SPR_BIGGEM, x, y, 2, 0);
    }
}

static void draw_enemies(const world *w)
{
    int shx = hu_shake_x(), shy = hu_shake_y();
    for (int i = 0; i < w->n_en; i++) {
        const enemy *e = &w->en[i];
        if (!e->alive) continue;
        const enemy_info *in = &EN_INFO[e->type];
        int ex = e->x / SUB, ey = e->y / SUB, sz;
        int fr = en_frame(e, &sz);
        int x = sx(ex) - sz / 2 + shx, y = sy(ey) - sz / 2 + shy;
        if (in->beh == B_WALK || in->beh == B_TURRET) y = sy(ey + in->hh) - 15 + shy;
        if (e->type == ET_WALRUS) y = sy(ey + in->hh) - 46 + shy;
        if (y > rs_screen_bottom() || y < rs_screen_top() - 48) continue;
        int flip = e->face ? RS_SPR_HFLIP : 0;
        if (e->type == ET_URCHIN || e->type == ET_CRAB || e->type == ET_JELLY || e->type == ET_ICICLE) flip = 0;
        if (e->type == ET_WALRUS) flip = e->face ? RS_SPR_HFLIP : 0;
        if (e->type == ET_ICICLE && e->st == 1) x += (e->t & 2) ? 1 : -1;
        int pal = e->flash && (e->flash & 2) ? OBJ_FLASH : -1;
        if (pal >= 0) spr_pal(fr, x, y, pal, 2, flip);
        else spr(fr, x, y, 2, flip);
    }
}

static void draw_hero(const world *w, int x, int y, int title)
{
    const hero *h = &w->h;
    int fr;
    if (title) fr = SPR_IDLE + ((tick / 40) % 4 == 3 ? 2 : (tick / 20) & 1);
    else if (h->dead) fr = SPR_HURT;
    else if (h->hurt_t > HURT_IFRAMES - 16) fr = SPR_HURT;
    else if (h->land_t > 2) fr = SPR_SQUASH;
    else if (h->ground) fr = (h->vx > 64 || h->vx < -64) ? SPR_RUN + (h->walk_t / 5) % 4 : SPR_IDLE + ((tick / 50) % 5 == 4 ? 2 : (tick / 25) & 1);
    else if (h->jump_t > 4) fr = SPR_SQUASH + 1;
    else if (h->fire_held && h->fire_t) fr = SPR_FIRE;
    else fr = h->vy < 0 ? SPR_RISE : SPR_FALL;
    if (!title && h->hurt_t && !h->dead && (h->hurt_t & 4)) return;   /* safe after a hit: blinks */
    spr(fr, x, y, 2, h->face && !title ? RS_SPR_HFLIP : 0);
}

static void draw_parts(void)
{
    int shx = hu_shake_x(), shy = hu_shake_y();
    for (int i = 0; i < MAX_PART; i++) {
        const part *p = &parts[(part_next + MAX_PART - 1 - i) % MAX_PART];   /* the newest first (in front) */
        if (p->t >= p->life) continue;
        int x = sx(p->x / SUB) + shx, y = sy(p->y / SUB) + shy, k = p->t * 3 / (p->life ? p->life : 1);
        switch (p->kind) {
        case P_FLASH: spr(SPR_FLASH + (p->t > 1), x - 8, y - 1, 2, 0); break;
        case P_CASING: spr(SPR_CASING + ((p->t / 3 + p->a) & 3), x - 4, y - 4, 2, 0); break;
        case P_PUFF: case P_DUST: spr(SPR_PUFF + (k > 2 ? 2 : k), x - 4, y - 4, 2, 0); break;
        case P_SPARK: spr(SPR_SPARK + (k > 2 ? 2 : k), x - 4, y - 4, 2, 0); break;
        case P_SHARD: spr(SPR_SHARD + p->a % 3, x - 4, y - 4, 2, (p->t & 4) ? RS_SPR_HFLIP : 0); break;
        case P_POOF: spr(SPR_POOF + (k > 2 ? 2 : k), x - 8, y - 8, 2, 0); break;
        case P_POP: {
            int kind = p->a >> 6, n = p->a & 63;
            char s[8];
            int pop = p->t < 8 ? hu_ease_back(p->t, 8, 3) - 3 : 0;
            if (p->t > 30 && (p->t & 2)) break;
            if (kind == POP_COMBO) {
                snprintf(s, sizeof s, "x%d", n);
                small_text(s, x - (int)strlen(s) * 5 / 2, y + pop, 1, 3);
            } else if (kind == POP_BONUS) {
                snprintf(s, sizeof s, "+%d", n);
                int wd = (int)strlen(s) * 5 + 9;
                spr(SPR_GEM, x - wd / 2, y - 1 + pop, 3, 0);
                small_text(s, x - wd / 2 + 9, y + pop, 1, 3);
            } else {
                spr(SPR_HEART, x - 9, y - 1 + pop, 3, 0);
                small_text("+1", x, y + pop, 1, 3);
            }
            break;
        }
        }
    }
}

static void draw_shots(const world *w)
{
    int shx = hu_shake_x(), shy = hu_shake_y();
    for (int k = 0; k < MAX_SHOT; k++) {
        const struct shot *s = &w->sh[k];
        if (!s->alive) continue;
        spr(SPR_SHOT + s->kind, sx(s->x / SUB) - 4 + shx, sy(s->y / SUB) - 4 + shy, 2, 0);
    }
    if (w->beam_t) {
        int x = sx(w->beam_x) - 4 + shx, fr = w->beam_t < 3;
        for (int y = w->beam_y0; y < w->beam_y1; y += 16) spr(SPR_BEAM + fr, x, sy(y) + shy, 2, 0);
    }
}

static void draw_gems(const world *w)
{
    int shx = hu_shake_x(), shy = hu_shake_y();
    for (int k = 0; k < MAX_GEM; k++) {
        const gem *g = &w->gm[k];
        if (!g->alive) continue;
        int y = sy(g->y / SUB) - 4 + shy;
        if (y < rs_screen_top() - 8 || y > rs_screen_bottom()) continue;
        spr((g->big ? SPR_BIGGEM : SPR_GEM) + (((tick + k * 5) / 12) % 4 == 0), sx(g->x / SUB) - 4 + shx, y, 2, 0);
    }
}

/* ---- the HUD (sprites, prio 3) -------------------------------------------------------------------------------------------- */
typedef struct hudpos { int heart_x, heart_y, heart_dir, gem_x, gem_y, depth_x, depth_y, gauge_x, gauge_y, wpn_x, wpn_y; } hudpos;
static void hud_layout(int narrow, int gems, hudpos *p)
{
    int top = rs_screen_top(), len = gems >= 1000 ? 4 : gems >= 100 ? 3 : gems >= 10 ? 2 : 1;
    if (narrow) {
        int x0 = rs_screen_visible_x0() - rs_screen_origin_x(), x1 = rs_screen_visible_x1() - rs_screen_origin_x();
        p->heart_x = x0 + 30;
        p->heart_y = top + 6;
        p->heart_dir = 1;
        p->gem_x = x1 - 30;
        p->gem_y = top + 4;
        p->depth_x = (x0 + x1) / 2;
        p->depth_y = top + 22;
        p->gauge_x = SHAFT_X + SHAFT_W - 12;
        p->gauge_y = top + 44;
        p->wpn_x = x1 - 30 - 16;
        p->wpn_y = top + 22;
    } else {
        p->heart_x = SHAFT_X - 12;
        p->heart_y = 8;
        p->heart_dir = -1;
        p->gem_x = SHAFT_X + SHAFT_W + 8 + 10 + len * 12;   /* (the digits' right edge: the icon by the wall) */
        p->gem_y = 6;
        p->depth_x = SHAFT_X - 30;
        p->depth_y = 30;
        p->gauge_x = SHAFT_X + SHAFT_W + 4;
        p->gauge_y = 48;
        p->wpn_x = SHAFT_X + SHAFT_W + 8;
        p->wpn_y = 26;
    }
}
/* the gem counter's icon (where the gems fly to) */
static void gem_target(int narrow, int gems, int *x, int *y)
{
    hudpos p;
    char s[8];
    hud_layout(narrow, gems, &p);
    snprintf(s, sizeof s, "%d", gems);
    *x = p.gem_x - (int)strlen(s) * 12 - 10;
    *y = p.gem_y + 4;
}

static void draw_hud(const world *w, int narrow, int gems_only)
{
    hudpos p;
    char s[16];
    hud_layout(narrow, w->gems, &p);
    for (int i = 0; i < w->hpmax && !gems_only; i++) {               /* hearts: 4 a row (8 upright) */
        int per = narrow ? 8 : 4, cx = i % per, cy = i / per;
        int x = p.heart_x + p.heart_dir * cx * 9 - (p.heart_dir < 0 ? 8 : 0);
        int beat = i == w->hp - 1 && w->hp <= 1 && (tick & 16);
        spr(SPR_HEART + (i < w->hp ? 0 : 1), x, p.heart_y + cy * 9 - beat, 3, 0);
    }
    /* the gems: the icon and the big digits, right-aligned; a pulse as one arrives */
    shown_gems = w->gems;
    snprintf(s, sizeof s, "%d", w->gems);
    int len = (int)strlen(s), cx = p.gem_x - len * 6, ux = hu_ui_x();
    hu_number(w->gems, cx - ux, p.gem_y - (gem_pulse > 4 ? 1 : 0), 3);
    budget_kit(p.gem_y, 16);
    spr(SPR_BIGGEM + (gem_pulse ? 1 : 0), p.gem_x - len * 12 - 10, p.gem_y + 4, 3, 0);
    if (gems_only) return;                              /* (a shop upright: its gems) */
    /* the depth: 2-3 */
    level_str(s, sizeof s, w->level);
    small_text(s, p.depth_x - 7, p.depth_y, 0, 3);
    /* the weapon worn */
    static const int wicon[W_COUNT] = {SPR_I_AMMO, SPR_I_SPREAD, SPR_I_BEAM, SPR_I_BURST};
    if (w->weapon) spr(wicon[w->weapon], p.wpn_x, p.wpn_y, 3, 0);
    /* the bubbles' gauge, top down */
    for (int i = 0; i < w->ammo_max; i++)
        spr(SPR_PIP + (i < w->ammo ? 0 : 1), p.gauge_x, p.gauge_y + i * 7, 3, 0);
    /* the combo under way, by the depth */
    if (w->combo >= 2) {
        snprintf(s, sizeof s, "x%d", w->combo);
        int pop = w->combo_t > 32 ? hu_ease_back(40 - w->combo_t, 8, 3) - 3 : 0;
        small_text(s, p.depth_x - 7, p.depth_y + 10 + pop, 1, 3);
    }
    /* the walrus's health, over the shaft */
    if (w->boss >= 0 && w->boss < w->n_en && w->en[w->boss].alive && w->en[w->boss].st > 0) {
        int n = 12, x0 = SHAFT_X + SHAFT_W / 2 - n * 4, y = narrow ? rs_screen_top() + 36 : 4;
        int fill = w->boss_hpmax ? w->en[w->boss].hp * n * 8 / w->boss_hpmax : 0;
        for (int i = 0; i < n; i++) {
            int k = fill - i * 8;
            spr(SPR_BAR + (k < 0 ? 0 : k > 8 ? 8 : k), x0 + i * 8, y, 3, 0);
        }
    }
    /* the gems flying to the counter */
    int tx, ty;
    gem_target(narrow, w->gems, &tx, &ty);
    for (int i = 0; i < MAX_ZIP; i++) {
        const zip *z = &zips[i];
        if (!z->t) continue;
        int x0 = sx(z->x), y0 = sy(z->y);
        int x = x0 + hu_ease_in(z->t, ZIP_FRAMES, tx - x0), y = y0 + hu_ease_in(z->t, ZIP_FRAMES, ty - y0);
        spr(z->big ? SPR_BIGGEM : SPR_GEM, x - 4, y - 4, 3, 0);
    }
}

/* ---- the panels ------------------------------------------------------------------------------------------------------------ */
/* the offers' rows: the name, the line two rows under it (its accents free), the price on the name's row or, upright
 * when it does not fit, two rows under the line; landscape: the gems on row 9, upright: the HUD's counter */
#define SHOP_ROW(n, i) ((n) ? 9 + 6 * (i) : 12 + 5 * (i))
#define SHOP_GO(n, k) ((k) >= 3 ? 27 : SHOP_ROW(n, k) + 1)   /* the last line (k offers) */
static void shop_box(int narrow, int *x, int *w)
{
    *w = narrow ? (ht_ncols() - 2 < 28 ? ht_ncols() - 2 : 28) : 30;   /* (upright: room for the cursor on both sides) */
    *x = narrow ? ht_center_w(*w) : (40 - *w) / 2;
}
static const char *shop_title(int kind)
{
    return kind == 2 ? TR(GIFT) : kind == 1 ? TR(LITTLE_SHOP) : TR(LEVEL_CLEAR);
}
/* where an offer's lines go: its icon, its name, its line, its price (and a gem before it); upright the panel is
 * narrow: the icon beside the name, the line from the panel's edge, the price under them when it does not fit */
typedef struct offer_lay { int icon_x, icon_y, nx, dx, px, prow, gem; } offer_lay;
static void offer_layout(const world *w, int i, int narrow, int x, int wd, offer_lay *l, char *price, int n)
{
    const offer *o = &w->offers[i];
    int r = SHOP_ROW(narrow, i), right = x + wd - 1;
    int nw = hu_width(upgrade_name(o->up), 0), dw = hu_width(upgrade_desc(o->up), 0);
    if (o->sold) snprintf(price, (size_t)n, "%s", TR(SOLD));
    else if (!o->price) snprintf(price, (size_t)n, "%s", TR(FREE));
    else snprintf(price, (size_t)n, "%d", o->price);
    l->gem = !o->sold && o->price;
    l->icon_x = (x + 1) * 8 + 2;
    l->icon_y = narrow ? r * 8 - 9 : r * 8 - 4;
    l->nx = x + 4 + nw > right ? right - nw : x + 4;
    if (l->nx < x + 1) l->nx = x + 1;
    int d0 = narrow ? x + 1 : x + 4;
    l->dx = d0 + dw > right ? right - dw : d0;
    if (l->dx < x + 1) l->dx = x + 1;
    int pw = hu_width(price, 0);
    l->px = right - pw;
    l->prow = l->nx + nw < l->px - l->gem - 1 ? r : narrow ? r + 4 : r + 2;
}
static void shop_panel(const world *w, const view *v, int narrow)
{
    char s[48];
    int x, wd;
    shop_box(narrow, &x, &wd);
    if (narrow) ht_banner(3, shop_title(v->shop_kind));
    else hu_banner(2, shop_title(v->shop_kind));
    int last = SHOP_GO(narrow, w->n_offers);
    hu_panel(x, 7, wd, last - 7 + 2);
    if (!narrow) {
        rs_strf(s, sizeof s, TR(GEMS_N), w->gems);
        hu_box_text(x + (wd - hu_width(s, 0)) / 2, 9, s);
    }
    for (int i = 0; i < w->n_offers; i++) {
        offer_lay l;
        offer_layout(w, i, narrow, x, wd, &l, s, sizeof s);
        int r = SHOP_ROW(narrow, i);
        hu_box_text(l.nx, r, upgrade_name(w->offers[i].up));
        hu_box_text(l.dx, r + 2, upgrade_desc(w->offers[i].up));
        hu_box_text(l.px, l.prow, s);
    }
    const char *go = TR(GO_ON);
    hu_box_text(x + (wd - hu_width(go, 0)) / 2, last, go);
}
static void shop_sprites(const world *w, const view *v, int narrow, int pdy)
{
    int x, wd, ux = hu_ui_x();
    char s[48];
    shop_box(narrow, &x, &wd);
    for (int i = 0; i < w->n_offers; i++) {
        offer_lay l;
        offer_layout(w, i, narrow, x, wd, &l, s, sizeof s);
        spr(SPR_I_HEART + w->offers[i].up, l.icon_x + ux, l.icon_y + pdy, 3, 0);
        if (l.gem) spr(SPR_GEM, (l.px - 1) * 8 + ux - 1, l.prow * 8 + pdy, 3, 0);
    }
    int sel = v->shop_sel, r = sel < w->n_offers ? SHOP_ROW(narrow, sel) : SHOP_GO(narrow, w->n_offers), b = (tick / 8) & 1;
    int y = r * 8 + pdy + (sel < w->n_offers ? 4 : 0);
    spr(SPR_CURSOR + b, x * 8 + ux - 7, y, 3, 0);
    spr(SPR_CURSOR + b, (x + wd) * 8 + ux - 1, y, 3, RS_SPR_HFLIP);
}

/* the title's lines under the logo: a dive in progress (and the two choices), the deepest level */
static int title_lines(const view *v, char out[4][64])
{
    char t[8];
    int n = 0;
    if (v->can_continue) {
        level_str(t, sizeof t, v->run_level);
        rs_strf(out[n++], 64, TR(RUN_AT), t);
        snprintf(out[n++], 64, "%s%s%s", v->title_sel == 0 ? HU_ARROW_RIGHT " " : "", TR(CHOICE_CONTINUE),
                 v->title_sel == 0 ? " " HU_ARROW_LEFT : "");
        snprintf(out[n++], 64, "%s%s%s", v->title_sel == 1 ? HU_ARROW_RIGHT " " : "", TR(CHOICE_NEW),
                 v->title_sel == 1 ? " " HU_ARROW_LEFT : "");
    }
    return n;
}

/* ---- the frame ------------------------------------------------------------------------------------------------------------- */
void draw_frame(const world *w, const view *v)
{
    int narrow = tall() && ht_narrow();
    tick++;
    if (laid_w != rs_screen_width() || laid_h != rs_screen_height()) {
        layers_setup();
        int z = zone_loaded;
        zone_loaded = -1;
        load_zone(z >= 0 ? z : 0);
    }
    load_zone(zone_of(w->level));
    rs_screen_fill(1);
    rs_screen_focus_x(-1);
    int title = v->state == ST_TITLE;
    int panel = v->state == ST_SHOP || v->state == ST_OVER || v->state == ST_PLAY;
    int pin = !tall() ? HT_PIN_OFF : title ? HT_PIN_TITLE : panel ? HT_PIN_PANEL : HT_PIN_OFF;
    rs_bg_pin(RS_BG1, 0, 0, 0);
    if (pin) ht_pin(pin);
    r_pin = pin;
    int pdy = pin ? ht_pin_dy(pin) : 0;

    cam = camera(w, title);
    shaft_rows(w, -1);
    back_rows();
    rs_brightness(15);

    /* the UI layer: redrawn when what it shows changes */
    int card = v->card > 0 && v->state == ST_PLAY, hint = v->first_run && v->state == ST_PLAY && w->level == 0 && w->t < 6 * SEC;
    int key = v->state * 7919 + narrow * 11 + v->paused * 53 + (v->state == ST_OVER && v->st_t >= RETRY_LOCK) * 3 +
              card * 101 + hint * 103 + (warn_t > 0) * 997 + w->level * 31 +
              (title ? v->can_continue * 43 + v->title_sel * 47 + v->best * 3 : 0) +
              (v->state == ST_SHOP ? w->gems * 211 + w->n_offers * 17 + w->offers[0].sold * 19 + w->offers[1].sold * 23 +
                                     w->offers[2].sold * 29 + v->shop_kind * 37 : 0);
    if (key != ui_key) {
        hu_clear();
        char s[64];
        if (title) {
            if (narrow) ht_logo("PUFFIN PLUNGE", title_ramps, 2, HT_ROW_LOGO, 0);
            else hu_logo("PUFFIN PLUNGE", title_ramps, 2, 2, 0);
        } else if (v->state == ST_SHOP) {
            shop_panel(w, v, narrow);
        } else if (v->state == ST_OVER) {
            char t[8], note[64];
            level_str(t, sizeof t, w->level);
            rs_strf(note, sizeof note, TR(RES_NOTE), t, w->best_combo);
            const char *ttl = w->won ? TR(WIN) : HTR(GAME_OVER);
            if (narrow) {
                ht_banner(4, ttl);
                ht_gameover_panel(v->new_best, note, HTR(BACK_TITLE));
            } else {
                hu_banner(4, ttl);
                hu_gameover_panel(1, v->new_best, w->score, 0);
                hu_text(hu_center(note, 0), 23, note);      /* (under the panel: the retry line stays on its row 19) */
            }
        } else {
            if (card) {                                 /* the level's card: LEVEL 2-1 and the zone's name */
                char t[8];
                level_str(t, sizeof t, w->level);
                rs_strf(s, sizeof s, TR(LEVEL_N), t);
                const char *zn = w->boss_level ? TR(BOTTOM) : zone_name(zone_of(w->level));
                if (narrow) { ht_big(8, s, HU_BIG_FREE); ht_text(11, zn, 0); }
                else { hu_big(hu_center(s, 1), 8, s, HU_BIG_FREE); hu_text(hu_center(zn, 0), 11, zn); }
            }
            if (hint) {
                const char *a = v->touch ? TR(HINT_TOUCH) : TR(HINT_PAD), *b = v->touch ? TR(HINT_TOUCH2) : TR(HINT_PAD2);
                if (narrow) { int r = ht_text(20, a, 0); ht_text(r + 2, b, 0); }
                else { hu_text(hu_center(a, 0), 20, a); hu_text(hu_center(b, 0), 22, b); }
            }
            if (warn_t) {
                const char *ws = TR(WALRUS);
                if (narrow) ht_big(8, ws, HU_BIG_FREE);
                else hu_big(hu_center(ws, 1), 8, ws, HU_BIG_FREE);
            }
            if (v->paused) {
                const char *q = TR(PAUSE_QUIT);
                if (narrow) ht_text(17, q, 0);
                else hu_text(hu_center(q, 0), 17, q);
            }
        }
        hu_pause(0, 13);
        ui_key = key;
    }
    int slide = v->state == ST_OVER ? hu_slide_in(v->st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, -hu_ui_x() & (rs_bg_map_w(RS_BG1) * 8 - 1), -slide & 255);
    if (narrow) ht_pause(v->paused, 13);
    else hu_pause(v->paused, 13);

    /* sprites, front to back */
    rs_oam_clear();
    budget_reset();
    if (title) {
        char lines[4][64];
        int n = title_lines(v, lines);
        if (narrow) {
            ht_title tc = {v->can_continue && v->title_sel == 0 ? TR(VERB_CONTINUE) : TR(VERB), 1, 15, 0, NULL};
            ht_title_out to;
            ht_title_draw(&tc, v->st_t, v->best, &to);
            int r = 15;
            for (int i = 0; i < n; i++) { ht_text(r, lines[i], 0); r += ht_text_rows(lines[i], 0) + 1; }
            ht_title_sprites(&tc, &to, v->st_t, NULL, NULL);
            budget_kit(to.glyph_y, 16);
            int gy0 = n ? (r - 1) * 8 + pdy : to.gap_y0, gy1 = to.gap_y1;
            int my = (gy0 + gy1) / 2 - 12 + hu_bob(v->st_t, 64, 2);
            draw_hero(w, ht_mid_x() - 12 + hu_ui_x(), my, 1);
        } else {
            hu_title_draw(v->st_t, v->best);
            for (int i = 0; i < n; i++) hu_text(hu_center(lines[i], 0), 13 + i * 2, lines[i]);
            hu_title_sprites(v->st_t, NULL, NULL);
            budget_kit(HU_ROW_PROMPT * 8 - 4, 16);
            draw_hero(w, 160 - 12 + hu_ui_x(), (n ? 70 : 84) + hu_bob(v->st_t, 64, 2) - (n ? 0 : 0), 1);
        }
        return;
    }
    if (v->state == ST_SHOP) {
        shop_sprites(w, v, narrow, pdy);
        if (narrow) draw_hud(w, narrow, 1);
        return;
    }
    if (v->state == ST_OVER) {
        int medal = v->medal;
        if (narrow) {
            ht_gameover_sprites(w->score, v->best, medal, v->st_t, slide);
            char t[8], note[64];
            level_str(t, sizeof t, w->level);
            rs_strf(note, sizeof note, TR(RES_NOTE), t, w->best_combo);
            if (v->st_t >= RETRY_LOCK) ht_retry_line_at(ht_gameover_retry_row(note), v->st_t, RETRY_LOCK, HTR(BACK_TITLE));
        } else {
            hu_gameover_sprites(1, w->score, 0, v->best, medal, v->st_t, slide);
            hu_retry_line(v->st_t, RETRY_LOCK, HTR(BACK_TITLE));
        }
        budget_kit(rs_screen_top(), 48);
        return;
    }
    /* play */
    draw_hud(w, narrow, 0);
    draw_parts();
    int shx = hu_shake_x(), shy = hu_shake_y();
    const hero *h = &w->h;
    int hx = sx(h->x / SUB) - 12 + shx, hy = sy(h->y / SUB) - 23 + shy;
    if (h->shell && !h->dead) spr(SPR_SHIELD + ((tick / 10) & 1), hx, hy - 1, 2, 0);
    draw_hero(w, hx, hy, 0);
    draw_shots(w);
    draw_enemies(w);
    draw_doors(w);
    draw_gems(w);
    if (hurt_t) rs_brightness(hurt_t > 8 ? 12 : 15);
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(laid_w); S(laid_h); S(zone_loaded); S(ui_key); S(level_drawn); S(row_cache); S(back_cache); S(parts); S(part_next);
    S(zips); S(zip_next); S(tick); S(gem_pulse); S(hurt_t); S(warn_t); S(shown_gems);
    RS_STATE_RASTER(raster);
}
#undef S
