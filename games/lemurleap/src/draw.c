/*
 * Lemur Leap: video. The layers (back to front):
 *   backdrop  the sky, one colour per line (the raster): by the altitude of that line, so the sky turns from the
 *             garden's blue to the clouds' pale blue, the sunset's orange and pink, then the night of space
 *   BG4       far clouds (1/4 of the climb; their colours follow the zone: white, pink, a violet nebula)
 *   BG3       the stars and planets (1/8), shown high up, fading in
 *   sprites   prio 2: the leaves, the garden's ground, the coins, the power-ups, the pests, the seeds, the lemur and
 *             its hat, the effects
 *   BG2       the side panels: the giant beanstalk's stalks, their leaves and a vine (1/2), high priority: they hide
 *             the sprites outside the column (the lemur crossing the wrap shows on both sides, cut at the edges)
 *   BG1       the house kit: the HUD (at the sides; a tall screen: pinned at its top), the title, the results, PAUSED;
 *             prio 3 sprites: the kit's digits and glyphs, the HUD's icons
 * The column is 192 px wide, centred (stage x 64..255). Widescreen: wider panels. The tall screen (a phone held
 * upright): rs_screen_zoom (main.c), the stage at the screen's bottom: the lines above it show more of the climb.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/lemurleap/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "rs.h"
#include "house_ui.h"
#include "house_tall.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

#define COL_X        64             /* the column's left edge, stage x */
#define HUD_ROWS     5              /* a tall screen: BG1's rows 0..4 pinned at its top (the HUD) */
#define PARTS        40
#define OAM_BUDGET   124
#define LINE_BUDGET  30
#define BANNER_T     150
static const int MEDALS[4] = {100, 250, 500, 900};       /* metres: bronze, silver, gold, pearl (the zones) */

static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 214, 110), RS_RGB8(246, 160, 52), RS_RGB8(196, 100, 30)},     /* LEMUR: the amber eyes */
    {RS_RGB8(196, 236, 120), RS_RGB8(118, 194, 72), RS_RGB8(58, 130, 52)}};     /* LEAP: the leaves */

/* ---- the drawing's state (saved: draw_state) ------------------------------------------------------------------- */
static int laid_w, laid_h, ui_key = -1, tick, cur_fur = -1, cur_zone = -1;
typedef struct part { int16_t x, y, vx, vy; uint8_t spr, t, life, flags; } part;   /* world px; Q4 speeds */
static part parts[PARTS];
static int part_next;
static int banner_t, banner_kind, hurt_t;
/* per frame (set by draw_frame, read by the raster and main.c's touches: not saved, state_audit.txt) */
static int r_pin, r_hud, r_cam;
hot draw_hot[HOT_MAX];
static int band[RS_SCREEN_H_MAX / 8 + 2], oam_n;

static int tall(void) { return rs_screen_height() > RS_SCREEN_H; }
static int map_w(void) { return rs_screen_width() > 504 ? 128 : 64; }
static int imin(int a, int b) { return a < b ? a : b; }

const char *look_name(int k)
{
    static const int key[LOOKS] = {GS_LOOK_NOHAT, GS_HAT_LEAF, GS_HAT_FLOWER, GS_HAT_PARTY, GS_HAT_BEANIE, GS_HAT_PIRATE,
                                   GS_HAT_CROWN, GS_HAT_HELMET, GS_FUR_GREY, GS_FUR_GOLDEN, GS_FUR_SNOW, GS_FUR_MIDNIGHT};
    return k >= 0 && k < LOOKS ? rs_tr(&gs_strings, key[k]) : "";
}

/* ---- sprites, within the guidelines (128, 32 a line) -------------------------------------------------------------- */
static void budget_reset(void)
{
    memset(band, 0, sizeof band);
    oam_n = 0;
}
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
    const ll_sprite_def *d = &ll_spr[id];
    spr_raw(x, y, d->tile, d->w, d->h, d->pal, prio, flags);
}
static void budget_kit(int y, int h, int n)
{
    int b0 = (y - rs_screen_top()) >> 3, b1 = (y + h - 1 - rs_screen_top()) >> 3, nb = (int)(sizeof band / sizeof band[0]);
    if (b0 < 0) b0 = 0;
    if (b1 >= nb) b1 = nb - 1;
    for (int b = b0; b <= b1; b++) band[b] += n;
    oam_n += n;
}

/* ---- the sky: one colour per line, by its altitude (metres), the house's dither step every 8 lines ------------- */
static const int16_t SKY_M[] = {0, 150, 230, 420, 560, 700, 820, 940, 1100, 1400};
static const uint8_t SKY_RGB[][3] = {{150, 210, 250}, {120, 192, 248}, {186, 224, 250}, {140, 196, 242}, {250, 190, 140},
                                     {236, 128, 140}, {120, 74, 150}, {40, 30, 86}, {16, 14, 48}, {8, 8, 30}};
#define SKY_N ((int)(sizeof SKY_M / sizeof SKY_M[0]))
static rs_color sky_at(int world_y, int line)
{
    int m10 = -world_y;                                     /* tenths of a metre */
    int k = 0;
    while (k < SKY_N - 1 && m10 >= SKY_M[k + 1] * PX_PER_M) k++;
    int r, g, b;
    if (k >= SKY_N - 1) {
        r = SKY_RGB[k][0], g = SKY_RGB[k][1], b = SKY_RGB[k][2];
    } else {
        int a = SKY_M[k] * PX_PER_M, z = SKY_M[k + 1] * PX_PER_M, t = m10 < a ? 0 : (m10 - a) * 256 / (z - a);
        r = SKY_RGB[k][0] + (SKY_RGB[k + 1][0] - SKY_RGB[k][0]) * t / 256;
        g = SKY_RGB[k][1] + (SKY_RGB[k + 1][1] - SKY_RGB[k][1]) * t / 256;
        b = SKY_RGB[k][2] + (SKY_RGB[k + 1][2] - SKY_RGB[k][2]) * t / 256;
    }
    if ((line & 7) >= 6) { r += 4; g += 2; b += 4; }          /* the house's 2-line dither step every 8 lines */
    return RS_RGB8(imin(r, 255), imin(g, 255), imin(b, 255));
}

static void raster(int line, void *user)
{
    (void)user;
    rs_pal_set(0, sky_at(r_cam + line, r_cam + line));
    if (r_pin) rs_bg_enable(RS_BG1, ht_pin_line(r_pin, line));
    else if (r_hud) rs_bg_enable(RS_BG1, line < rs_screen_top() + HUD_ROWS * 8 || line >= 0);   /* (not its wrap above the stage) */
    else rs_bg_enable(RS_BG1, 1);
}

/* ---- set-up ---------------------------------------------------------------------------------------------------------- */
/* the HUD's tiles: a bar's ends and its fills (0..8 eighths) in the UI palette */
static void hud_tiles(void)
{
    uint8_t t[64];
    for (int k = 0; k < 11; k++) {
        memset(t, 0, sizeof t);
        for (int y = 1; y < 7; y++)
            for (int x = 0; x < 8; x++) {
                int v = 0;
                if (k == 0) v = x == 7 ? HU_OUTLINE : 0;
                else if (k == 1) v = x == 0 ? HU_OUTLINE : 0;
                else {
                    int fill = k - 2;
                    v = y == 1 || y == 6 ? HU_OUTLINE : x < fill ? (y == 2 ? HU_CREAM : HU_GOLD) : HU_DARK;
                }
                t[y * 8 + x] = (uint8_t)v;
            }
        rs_tiles_load8(VR_BG1 + UI_TILE + k, t, 1);
    }
}

/* the panels' map: every column of the screen outside the column, the stalk picture (the right side mirrored) */
static void panels_map(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG2, mw, 32, VR_PANEL);
    for (int c = rs_screen_left() / 8 - 1; c <= rs_screen_right() / 8 + 1; c++) {
        int sx = c * 8, mc = c & (mw - 1);
        for (int r = 0; r < 32; r++) {
            uint16_t e = 0;
            if (sx < COL_X) {
                int pc = PANEL_COLS - (COL_X - sx) / 8;          /* the picture's column (its right edge on the column) */
                if (pc < 0) pc = ((pc % 4) + 4) % 4;
                e = ll_panel_map[r * PANEL_COLS + pc];
            } else if (sx >= COL_X + COL_W) {
                int pc = PANEL_COLS - 1 - (sx - COL_X - COL_W) / 8;
                if (pc < 0) pc = ((pc % 4) + 4) % 4;
                e = (uint16_t)(ll_panel_map[r * PANEL_COLS + pc] ^ RS_MAP_HFLIP);
            } else {
                continue;
            }
            rs_bg_put(RS_BG2, mc, r, (uint16_t)(RS_MAP(RS_MAP_TILE(e), PAL_STALK, 1, 0, 0) | (e & (RS_MAP_HFLIP | RS_MAP_VFLIP))));
        }
    }
}

static void layers_setup(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    panels_map();
    rs_bg_setup(RS_BG4, 32, 32, VR_CLOUDS);
    for (int i = 0; i < 32 * 32; i++) {
        uint16_t e = ll_clouds_map[i];
        rs_bg_put(RS_BG4, i % 32, i / 32, (uint16_t)(RS_MAP(RS_MAP_TILE(e), PAL_CLOUDS, 0, 0, 0) | (e & 0xc000)));
    }
    rs_bg_setup(RS_BG3, 32, 64, VR_STARS);
    for (int i = 0; i < 32 * 64; i++) {
        uint16_t e = ll_stars_map[i];
        rs_bg_put(RS_BG3, i % 32, i / 32, (uint16_t)(RS_MAP(RS_MAP_TILE(e), PAL_STARS, 0, 0, 0) | (e & 0xc000)));
    }
    laid_w = rs_screen_width();
    laid_h = rs_screen_height();
    ui_key = -1;
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());              /* the column centred */
    if (tall()) rs_screen_origin_v(rs_screen_height() - RS_SCREEN_H);   /* upright: the stage at the bottom, the climb above */
    hu_config hc = hu_defaults();
    hc.obj_tile = KIT_OBJ_TILE;
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = OBJ_KIT;
    hc.logo_tile = LOGO_TILE;
    hc.ext_tile = EXT_TILE;
    hc.ext_count = EXT_COUNT;
    hu_init(&hc);
    rs_tiles_load(VR_PANEL, ll_panel_tiles, PANEL_TILES);
    rs_tiles_load(VR_CLOUDS, ll_clouds_tiles, CLOUDS_TILES);
    rs_tiles_load(VR_STARS, ll_stars_tiles, STARS_TILES);
    layers_setup();
    hud_tiles();
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, ll_obj_tiles, ll_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(OBJ_HATS), ll_pal_hats, 16);
    rs_pal_load(RS_PAL_OBJ(OBJ_PLATS), ll_pal_plats, 16);
    rs_pal_load(RS_PAL_OBJ(OBJ_ITEMS), ll_pal_items, 16);
    rs_pal_load(RS_PAL_OBJ(OBJ_PESTS), ll_pal_pests, 16);
    rs_pal_load(RS_PAL_OBJ(OBJ_FX), ll_pal_fx, 16);
    rs_pal_load(RS_PAL_BG(PAL_STARS), ll_pal_stars, 16);
    rs_math(RS_MATH_OFF, 0, 0);
    for (int l = 0; l < RS_BG_COUNT; l++) rs_bg_enable(l, 1);
    rs_raster(raster, NULL);
    cur_fur = cur_zone = -1;
}

void draw_state_loaded(void)
{
    int shape = ht_shape_reset();
    if (!shape && laid_w == rs_screen_width() && laid_h == rs_screen_height()) return;
    rs_screen_origin(rs_screen_margin());
    rs_screen_origin_v(tall() ? rs_screen_height() - RS_SCREEN_H : 0);
    layers_setup();
    hu_pause(0, 13);
}

void draw_reset_run(void)
{
    memset(parts, 0, sizeof parts);
    banner_t = hurt_t = 0;
}

/* ---- the zone's colours: the panels and the clouds, blended over 40 m round each border ----------------------------- */
static void zone_colours(int m)
{
    static const int edge[3] = {ZONE_CLOUDS, ZONE_SKY, ZONE_SPACE};
    int z = 0, k = 256;
    while (z < 3 && m >= edge[z] - 20) {
        if (m < edge[z] + 20) { k = (edge[z] + 20 - m) * 256 / 40; break; }
        z++;
    }
    int key = z * 1000 + (z < 3 ? k : 256);
    if (key == cur_zone) return;
    cur_zone = key;
    for (int i = 0; i < 16; i++) {
        if (z < 3 && k < 256) {
            rs_pal_set(RS_PAL_BG(PAL_STALK) + i, hu_lerp_color(ll_pal_stalk[z + 1][i], ll_pal_stalk[z][i], k));
            rs_pal_set(RS_PAL_BG(PAL_CLOUDS) + i, hu_lerp_color(ll_pal_clouds[z + 1][i], ll_pal_clouds[z][i], k));
        } else {
            int zz = z > 3 ? 3 : z;
            rs_pal_set(RS_PAL_BG(PAL_STALK) + i, ll_pal_stalk[zz][i]);
            rs_pal_set(RS_PAL_BG(PAL_CLOUDS) + i, ll_pal_clouds[zz][i]);
        }
    }
}

/* the stars: from 650 m, brighter up to 950 m (blended from the sky) */
static void stars_colours(int cam)
{
    int m = -(cam + 120) / PX_PER_M, k = m < 650 ? 0 : m > 950 ? 256 : (m - 650) * 256 / 300;
    rs_bg_enable(RS_BG3, k > 0);
    if (!k) return;
    rs_color sky = sky_at(cam + 120, 0);
    for (int i = 1; i < 16; i++) rs_pal_set(RS_PAL_BG(PAL_STARS) + i, hu_lerp_color(sky, ll_pal_stars[i], k));
}

/* ---- effects --------------------------------------------------------------------------------------------------------- */
static void part_add(int spr_id, int x, int y, int vx, int vy, int life, int flags)
{
    parts[part_next] = (part){(int16_t)x, (int16_t)y, (int16_t)vx, (int16_t)vy, (uint8_t)spr_id, 0, (uint8_t)life,
                              (uint8_t)flags};
    part_next = (part_next + 1) % PARTS;
}

void draw_events(const world *w)
{
    for (int i = 0; i < w->n_fx; i++) {
        const fxev *f = &w->fx[i];
        int x = f->x, y = f->y;
        switch (f->kind) {
        case FX_DUST:
            part_add(SPR_DUST, x - 6, y - 4, -12, -2, 12, 0);
            part_add(SPR_DUST, x + 6, y - 4, 12, -2, 12, 0);
            if (f->arg) part_add(SPR_SPARK, x, y - 12, 0, -10, 14, 0);
            break;
        case FX_CLOUD:
            for (int k = 0; k < 4; k++) part_add(SPR_DUST, x - 12 + k * 8, y + 2, (k - 2) * 6 + 3, 2, 16, 0);
            break;
        case FX_SPRING:
            part_add(SPR_RING, x - 8, y - 8, 0, 0, 12, 1);
            for (int k = 0; k < 4; k++) part_add(SPR_SPARK, x, y - 6, (k - 2) * 14 + 7, -20 - k * 4, 18, 0);
            hu_shake(1, 4);
            break;
        case FX_BREAK:
            for (int k = 0; k < 4; k++) part_add(SPR_DRYBIT + (k & 1), x - 12 + k * 8, y, (k - 2) * 8 + 4, -6, 24, 2);
            break;
        case FX_PUFF:
            for (int k = 0; k < 6; k++) part_add(SPR_TUFT + (k & 1), x - 14 + k * 6, y - 4, (k - 3) * 6 + 3, -8 - (k % 3) * 4, 40, 3);
            break;
        case FX_COIN:
            part_add(SPR_SPARK, x - 4, y - 4, 0, -4, 10, 0);
            break;
        case FX_POP:
        case FX_STOMP:
        case FX_SHIELD:
            for (int k = 0; k < 5; k++)
                part_add(SPR_STAR, x - 4, y - 4, (k - 2) * 16, -16 + (k % 2) * 10, 20, 2);
            part_add(SPR_RING, x - 8, y - 8, 0, 0, 12, 1);
            if (f->kind != FX_POP) hu_shake(2, 6);
            break;
        case FX_ITEM:
            for (int k = 0; k < 6; k++) part_add(SPR_SPARK, x - 4 + (k - 3) * 4, y - 4, (k - 3) * 6, -18, 20, 0);
            break;
        case FX_FLY:
            part_add(f->arg == IT_ROCKET ? SPR_DUST : SPR_DUST, x - 4 + ((tick >> 2) & 1) * 4 - 2, y - 2, 0, 16, 10, 0);
            break;
        }
    }
    if (w->events & EV_HURT) { hu_shake(3, 10); hurt_t = 12; }
    if (w->events & EV_ZONE) { banner_t = BANNER_T; banner_kind = w->zone; ui_key = -1; }
    if (w->events & EV_BEST) { banner_t = BANNER_T; banner_kind = 10; ui_key = -1; }
    for (int i = 0; i < PARTS; i++) {
        part *p = &parts[i];
        if (p->t >= p->life) continue;
        p->t++;
        p->x = (int16_t)(p->x + p->vx / 16);
        p->y = (int16_t)(p->y + p->vy / 16);
        if (p->flags & 2) p->vy = (int16_t)(p->vy + 4);             /* falling bits */
        if (p->flags & 3) p->vx = (int16_t)(p->vx * 15 / 16);
    }
    if (banner_t) banner_t--;
    if (hurt_t) hurt_t--;
}

static void draw_parts(int cam)
{
    for (int i = 0; i < PARTS; i++) {
        const part *p = &parts[i];
        if (p->t >= p->life) continue;
        int id = p->spr;
        if (id == SPR_DUST) id += imin(2, p->t * 3 / p->life);
        if (id == SPR_RING) id += p->t * 2 / p->life;
        if (id == SPR_SPARK) id += (p->t / 4) & 1;
        if ((p->flags & 3) == 3 && p->t > p->life - 10 && (p->t & 1)) continue;   /* fading seeds blink */
        spr(id, COL_X + p->x, p->y - cam, 2, 0);
    }
}

/* ---- the world's pictures ------------------------------------------------------------------------------------------ */
static int hero_frame(const hero *h, int *flags)
{
    *flags = h->face < 0 ? RS_SPR_HFLIP : 0;
    if (h->knocked) { *flags |= (tick / 6) & 1 ? RS_SPR_VFLIP : 0; return HERO_HURT; }
    if (h->fly) return HERO_FLY;
    if (h->spin_t) {
        static const uint8_t F[4] = {0, RS_SPR_HFLIP, RS_SPR_HFLIP | RS_SPR_VFLIP, RS_SPR_VFLIP};
        *flags = F[(h->spin_t / 4) & 3];
        return HERO_CURL;
    }
    if (h->land_t > 3) return HERO_SQUASH;
    if (h->shoot_t) return HERO_SHOOT;
    if (h->vy < -3 * FX / 2) return HERO_UP;
    return HERO_DOWN;
}

static void draw_hat(int frame, int flags, int x, int y, int hat)
{
    if (hat <= 0 || frame == HERO_CURL) return;
    int ax = ll_hero_head[frame][0], ay = ll_hero_head[frame][1];
    if (flags & RS_SPR_HFLIP) ax = 23 - ax;
    int hy = y + ay - 13 + 1;
    if (flags & RS_SPR_VFLIP) return;                        /* (tumbling: the hat is held on, unseen) */
    if (hat == HATS - 1) hy = y + ay - 4;                    /* the helmet goes round the head */
    spr(HAT_LEAF + hat - 1, x + ax - 8, hy, 2, flags & RS_SPR_HFLIP);
}

/* the lemur at stage (x, y) = its sprite's top-left; extras: the hat, the propeller or the rocket, the bubble */
static void draw_lemur_at(const hero *h, int frame, int flags, int x, int y, int hat, int shield)
{
    if (shield) spr(SPR_BUBBLE, x - 4, y - 6, 2, 0);
    if (h && h->fly == IT_PROP) {
        spr(SPR_PROPELLER + ((tick >> 1) & 1), x + 4, y - 4 - (hat > 0 ? 5 : 0), 2, 0);
    }
    draw_hat(frame, flags, x, y, hat);
    spr(frame, x, y, 2, flags);
    if (h && h->fly == IT_ROCKET) spr(SPR_JET + ((tick >> 1) & 1), x + (h->face < 0 ? 12 : -4), y + 6, 2, 0);
}

static void draw_hero(const world *w, int cam, int hat)
{
    const hero *h = &w->h;
    if (w->over && w->fall_t > FALL_FRAMES) return;
    int flags, frame = hero_frame(h, &flags);
    int hx = h->x / FX, y = h->y / FX - cam - 23;
    int shield = h->shield_t && (h->shield_t > 120 || (tick & 4));
    if (hurt_t & 2) return;
    draw_lemur_at(h, frame, flags, COL_X + hx - 12, y, hat, shield);
    if (hx < 16) draw_lemur_at(h, frame, flags, COL_X + hx + COL_W - 12, y, hat, shield);      /* across the wrap */
    if (hx > COL_W - 16) draw_lemur_at(h, frame, flags, COL_X + hx - COL_W - 12, y, hat, shield);
}

static void draw_plats(const world *w, int cam)
{
    for (int i = 0; i < MAX_PLAT; i++) {
        const plat *p = &w->p[i];
        if (!p->used) continue;
        int x = COL_X + p->x / FX, y = p->y / FX - cam;
        if (y < rs_screen_top() - 24 || y > rs_screen_bottom() + 8) continue;
        if (p->item) {
            int bob = hu_bob(tick + i * 9, 48, 1), id = p->item == IT_PROP ? SPR_PROP + ((tick >> 3) & 1) :
                      p->item == IT_ROCKET ? SPR_ROCKET : p->item == IT_SHIELD ? SPR_SHIELD : SPR_MAGNET;
            spr(id, x + p->w / 2 - 8, y - 17 + bob, 2, 0);
        }
        if (p->spring) spr(SPR_SPRING + (p->t > 0), x + p->spring - 1 - 2, y - 15 + (p->t > 0 ? -2 : 0), 2, 0);
        switch (p->type) {
        case P_LEAF: spr(SPR_LEAF + (p->t > 4), x, y - 1 + (p->t > 4), 2, 0); break;
        case P_MOVE: spr(SPR_MOVE + ((tick >> 3) & 1), x, y - 9 + (p->t > 4), 2, p->vx < 0 ? RS_SPR_HFLIP : 0); break;
        case P_DRY:
            if (p->state == PS_BROKEN) {
                int k = p->fall_v / 96;
                spr(SPR_DRYHALF, x - k, y - 1, 2, 0);
                spr(SPR_DRYHALF + 1, x + 16 + k, y - 1, 2, 0);
            } else {
                spr(SPR_DRY, x, y - 1, 2, 0);
            }
            break;
        case P_PUFF: spr(SPR_PUFF, x, y - 1, 2, 0); break;
        case P_SHROOM: spr(SPR_SHROOM + (p->t > 4), x, y - 2, 2, 0); break;
        case P_CLOUD: spr(SPR_CLOUD + (p->t > 6), x, y - 3 + (p->t > 6), 2, 0); break;
        }
    }
}

static void draw_world_bits(const world *w, int cam)
{
    for (int i = 0; i < MAX_SHOTS; i++)
        if (w->s[i].used) spr(SPR_SEED, COL_X + w->s[i].x / FX - 4, w->s[i].y / FX - cam - 4, 2, 0);
    for (int i = 0; i < MAX_PEST; i++) {
        const pest *e = &w->e[i];
        if (!e->used) continue;
        if (e->dead_t && (e->dead_t & 2)) continue;
        int x = COL_X + e->x / FX - 12, y = e->y / FX - cam - 12;
        int fl = (w->h.x > e->x ? RS_SPR_HFLIP : 0) | (e->dead_t ? RS_SPR_VFLIP : 0);
        if (e->type == E_STORM || e->type == E_UFO) fl &= ~RS_SPR_HFLIP;
        spr(SPR_BEE + e->type * 2 + ((tick >> 3) & 1), x, y, 2, fl);
    }
    for (int i = 0; i < MAX_COIN; i++) {
        const coin *c = &w->c[i];
        if (!c->used) continue;
        spr(SPR_COIN + ((tick / 6 + i) & 3), COL_X + c->x / FX - 4, c->y / FX - cam - 4, 2, 0);
    }
}

static void draw_ground(int cam, int title)
{
    int y = -cam;
    if (y > rs_screen_bottom() || y < rs_screen_top() - 40) return;
    for (int k = 0; k < 3; k++) spr(SPR_GROUND + (k & 1), COL_X + k * 64, y, 2, 0);
    for (int k = 0; k < 3; k++) spr(SPR_FLOWERS, COL_X + k * 64, y - 7, 2, (k & 1) ? RS_SPR_HFLIP : 0);
    (void)title;
}

static void draw_best(const world *w, int cam)
{
    if (w->best_m <= 0) return;
    int y = -w->best_m * PX_PER_M - cam;
    if (y < rs_screen_top() - 16 || y > rs_screen_bottom() + 8) return;
    for (int k = 0; k < 3; k++) spr(SPR_ROPE, COL_X + k * 64, y - 3, 2, 0);
    spr(SPR_FLAG, COL_X + 2, y - 16, 2, 0);
}

/* ---- the HUD -------------------------------------------------------------------------------------------------------- */
static void hud_bar(int row, int c0, int n, int num, int den)
{
    long fill = den > 0 ? (long)num * n * 8 / den : 0;
    int mw = rs_bg_map_w(RS_BG1);
    rs_bg_put(RS_BG1, (c0 - 1) & (mw - 1), row, RS_MAP(UI_TILE + 0, PAL_UI, 1, 0, 0));
    rs_bg_put(RS_BG1, (c0 + n) & (mw - 1), row, RS_MAP(UI_TILE + 1, PAL_UI, 1, 0, 0));
    for (int i = 0; i < n; i++) {
        long f = fill - i * 8;
        int k = f <= 0 ? 0 : f >= 8 ? 8 : (int)f;
        rs_bg_put(RS_BG1, (c0 + i) & (mw - 1), row, RS_MAP(UI_TILE + 2 + k, PAL_UI, 1, 0, 0));
    }
}

/* the power-up running: its kind and its share left (0..256), 0 none */
static int power_left(const hero *h, int *kind)
{
    if (h->fly) { *kind = h->fly; return h->fly_t * 256 / (h->fly == IT_ROCKET ? ROCKET_FRAMES : PROP_FRAMES); }
    if (h->shield_t) { *kind = IT_SHIELD; return h->shield_t * 256 / SHIELD_FRAMES; }
    if (h->magnet_t) { *kind = IT_MAGNET; return h->magnet_t * 256 / MAGNET_FRAMES; }
    *kind = 0;
    return 0;
}
static int item_spr(int k) { return k == IT_PROP ? SPR_PROP : k == IT_ROCKET ? SPR_ROCKET : k == IT_SHIELD ? SPR_SHIELD : SPR_MAGNET; }

/* landscape: the side panels (UI columns 0..7 and 32..39): the height and the record at the left, the coins and the
 * power-up at the right, icons and digits only (no words: every language fits) */
static void hud_side_bg(void)
{
    hu_panel(0, 1, 8, 6);
    hu_panel(0, 8, 8, 4);
    hu_panel(32, 1, 8, 6);
}
static void hud_side_text(const world *w, const view *v)
{
    char s[16];
    snprintf(s, sizeof s, "%d", v->best_m > world_height_m(w) ? v->best_m : world_height_m(w));
    hu_clear_rows(10, 1);
    hu_panel(0, 8, 8, 4);
    hu_box_text(7 - hu_width(s, 0), 10, s);
    int k, left = power_left(&w->h, &k);
    if (k) {
        hu_panel(32, 8, 8, 4);
        hud_bar(10, 35, 3, left, 256);
    } else {
        for (int r = 8; r < 12; r++)
            for (int c = 32; c < 40; c++) rs_bg_put(RS_BG1, c, r, RS_MAP(0, PAL_UI, 0, 0, 0));
    }
}
static void hud_side_sprites(const world *w, const view *v)
{
    int ux = hu_ui_x();
    spr(SPR_HUD_UP, ux + 4, 2 * 8 + 4, 3, 0);
    hu_number(world_height_m(w), 32 + 4, 4 * 8 - 4, 3);
    spr(SPR_FLAG, ux + 3, 9 * 8 + 4, 3, 0);
    spr(SPR_HUD_COIN, ux + 33 * 8 - 4, 2 * 8 + 4, 3, 0);
    hu_number(w->coins, 36 * 8 + 4, 4 * 8 - 4, 3);
    budget_kit(4 * 8 - 4, 16, 8);
    int k;
    if (power_left(&w->h, &k) && k) spr(item_spr(k), ux + 32 * 8 + 6, 9 * 8 + 4, 3, 0);
    (void)v;
}

/* upright: pinned at the screen's top: the height big in the middle, the coins and the record at the sides (clear of
 * the app's corner buttons), the power-up's bar under the height */
static void hud_top_text(const world *w, const view *v)
{
    char s[16];
    int c0, c1;
    ht_cols(&c0, &c1);
    hu_clear_rows(0, HUD_ROWS);
    hu_panel(c0 - 1, 0, c1 - c0 + 2, HUD_ROWS);                      /* a band behind the HUD, over the climb ahead */
    snprintf(s, sizeof s, "%d", w->coins);
    hu_box_text(c0 + 2, 3, s);
    snprintf(s, sizeof s, "%d", v->best_m > world_height_m(w) ? v->best_m : world_height_m(w));
    hu_box_text(c1 - 1 - hu_width(s, 0), 3, s);
    int k, left = power_left(&w->h, &k);
    if (k) hud_bar(4, (c0 + c1) / 2 - 2, 4, left, 256);
}
static void hud_top_sprites(const world *w, int narrow)
{
    int c0, c1, top = rs_screen_top();
    ht_cols(&c0, &c1);
    int ux = hu_ui_x();
    hu_number(world_height_m(w), ht_mid_x(), top + 6, 3);
    budget_kit(top + 6, 16, 5);
    spr(SPR_HUD_COIN, ux + c0 * 8 + 4, top + 3 * 8, 3, 0);
    char s[16];
    snprintf(s, sizeof s, "%d", w->best_m > world_height_m(w) ? w->best_m : world_height_m(w));
    spr(SPR_FLAG, ux + (c1 - 3 - hu_width(s, 0)) * 8 + 2, top + 3 * 8 - 6, 3, 0);
    int k;
    if (power_left(&w->h, &k) && k) spr(item_spr(k), ux + ((c0 + c1) / 2 - 5) * 8, top + 4 * 8 - 4, 3, 0);
    (void)narrow;
}

/* ---- the panels ------------------------------------------------------------------------------------------------------- */
static int ui_screen_y(int row, int pin)
{
    int base = pin && tall() ? ht_pin_screen_y(pin) : rs_screen_origin_y();
    return base + row * 8;
}
static void hot_set(int k, int row0, int row1, int pin)
{
    if (k < 0 || k >= HOT_MAX) return;
    draw_hot[k].y0 = (int16_t)ui_screen_y(row0, pin);
    draw_hot[k].y1 = (int16_t)ui_screen_y(row1 + 1, pin);
    draw_hot[k].x0 = 0;
    draw_hot[k].x1 = (int16_t)rs_screen_width();
}

/* the title's lines under the logo: the look shown (arrows), its state (worn, its price), the coins, the climb kept */
static int title_line(const view *v, int k, char *s, int n)
{
    switch (k) {
    case 0: snprintf(s, (size_t)n, "%s %s %s", HU_ARROW_LEFT, look_name(v->look), HU_ARROW_RIGHT); return 1;
    case 1:
        if (v->owned & (1 << v->look)) snprintf(s, (size_t)n, "%s", TR(WEARING));
        else if (v->coins >= LOOK_COST[v->look]) rs_strf(s, (size_t)n, TR(BUY_N), LOOK_COST[v->look]);
        else rs_strf(s, (size_t)n, TR(LOCKED_N), LOOK_COST[v->look]);
        return 1;
    case 2: rs_strf(s, (size_t)n, TR(COINS_N), v->coins); return 1;
    case 3:
        if (!v->can_continue) return 0;
        rs_strf(s, (size_t)n, TR(RUN_AT), v->run_m);
        return 1;
    case 4:
        if (!v->best_m) return 0;
        rs_strf(s, (size_t)n, TR(BEST_M), v->best_m);
        return 1;
    case 5:
        if (!v->can_continue) return 0;
        snprintf(s, (size_t)n, "%s", TR(NEW_RUN));
        return 1;
    default: return 0;
    }
}

static void title_text(const view *v, int narrow, int last_row)
{
    char s[128];
    for (int k = 0; k < HOT_MAX; k++) draw_hot[k].y0 = draw_hot[k].y1 = 0;
    if (!narrow) {
        static const int8_t rows[6] = {13, 15, 17, 19, 23, 25};
        hu_clear_rows(12, 8);
        hu_clear_rows(22, 5);
        for (int k = 0; k < 6; k++) {
            if (!title_line(v, k, s, sizeof s)) continue;
            int r = rows[k];
            hu_text(hu_center(s, 0), r, s);
            if (k == 0) { hot_set(HOT_PREV, r - 1, r + 1, 0); hot_set(HOT_NEXT, r - 1, r + 1, 0); }
            if (k == 1) hot_set(HOT_BUY, r - 1, r, 0);
            if (k == 5) hot_set(HOT_NEWRUN, r - 1, r, 0);
        }
        hot_set(HOT_PROMPT, HU_ROW_PROMPT - 1, HU_ROW_PROMPT + 1, 0);
    } else {
        /* upright: the look and its state together, the coins, the climb kept (else the record); B: NEW CLIMB goes
         * above the prompt (ht_title's hint) */
        static const int8_t order[2] = {0, 1};
        int r = 15;
        for (int i = 0; i < 3; i++) {
            int k = i < 2 ? order[i] : 3;                  /* (the coins and the record: icons at the screen's top) */
            if (!title_line(v, k, s, sizeof s)) continue;
            int rows = ht_text_rows(s, 0) + (hu_tall(s) ? 1 : 0);
            if (r + rows > last_row) break;
            ht_text(r + (hu_tall(s) ? 1 : 0), s, 0);
            if (k == 0) { hot_set(HOT_PREV, r - 1, r + 1, HT_PIN_TITLE); hot_set(HOT_NEXT, r - 1, r + 1, HT_PIN_TITLE); }
            if (k == 1) hot_set(HOT_BUY, r - 1, r + rows - 1, HT_PIN_TITLE);
            r += rows + (k == 0 ? 0 : 1);
        }
    }
    draw_hot[HOT_PREV].x1 = (int16_t)(rs_screen_width() / 2);
    draw_hot[HOT_NEXT].x0 = (int16_t)(rs_screen_width() / 2);
}

/* the results: the banner on row 4, the panel from row 8: HEIGHT, BEST (or NEW BEST), COINS, each over its value */
#define RES_ROW(i)  (10 + 5 * (i))
#define RES_RETRY   26
static void results_panel(const world *w, const view *v, int narrow)
{
    char s[64], vals[3][24];
    const char *lab[3] = {TR(RES_HEIGHT), v->new_best ? HTR(NEW_BEST) : HTR(BEST), TR(RES_COINS)};
    int width = narrow ? ht_ncols() : 24, x = narrow ? ht_center_w(width) : (40 - width) / 2;
    if (narrow) ht_banner(4, HTR(GAME_OVER));
    else hu_banner(4, HTR(GAME_OVER));
    hu_panel(x, 8, width, 21);
    for (int i = 0; i < 3; i++) {
        snprintf(s, sizeof s, "%s", lab[i]);
        hu_box_text(x + (width - hu_width(s, 0)) / 2, RES_ROW(i), s);
    }
    rs_strf(vals[0], 24, TR(METRES), world_height_m(w));
    rs_strf(vals[1], 24, TR(METRES), v->best_m);
    snprintf(vals[2], 24, "+%d", v->earned);
    for (int i = 0; i < 3; i++) hu_big(x + (width - hu_width(vals[i], 1)) / 2, RES_ROW(i) + 1, vals[i], HU_BIG_BANNER);
}

static int medal_of(int m)
{
    int t = 0;
    for (int i = 0; i < 4; i++) if (m >= MEDALS[i]) t = i + 1;
    return t;
}

/* ---- the frame --------------------------------------------------------------------------------------------------------- */
void draw_frame(const world *w, const view *v)
{
    int narrow = tall() && ht_narrow();
    tick++;
    if (laid_w != rs_screen_width() || laid_h != rs_screen_height()) layers_setup();
    if (v->fur != cur_fur) {
        rs_pal_load(RS_PAL_OBJ(OBJ_HERO), ll_pal_fur[v->fur & 3], 16);
        cur_fur = v->fur;
    }
    rs_screen_fill(1);
    rs_screen_focus_x(-1);
    int title = v->state == ST_TITLE, over = v->state == ST_OVER;
    int pin = !tall() ? HT_PIN_OFF : title ? HT_PIN_TITLE : over ? HT_PIN_PANEL : HT_PIN_OFF;
    int hud = v->state == ST_PLAY;
    int hud_pinned = tall() && !pin && hud;
    rs_bg_pin(RS_BG1, 0, hud_pinned ? HUD_ROWS * 8 : 0, 0);
    if (pin) ht_pin(pin);
    r_pin = pin;
    r_hud = hud_pinned;

    /* the camera: the world's (shaken), the title: the garden's ground at the bottom */
    int cam = title ? -(STAGE_H - 36) : w->cam_y - hu_shake_y();
    r_cam = cam;
    int m_mid = -(cam + 120) / PX_PER_M;
    zone_colours(m_mid < 0 ? 0 : m_mid);
    stars_colours(cam);
    rs_bg_scroll(RS_BG4, (tick / 8) & 255, (cam / 4) & 255);
    rs_bg_scroll(RS_BG3, 0, (cam / 8) & 511);
    rs_bg_scroll(RS_BG2, 0, (cam / 2) & 255);
    rs_brightness(15);

    /* the UI layer: redrawn when what it shows changes */
    int m = world_height_m(w), pk = 0, pl = power_left(&w->h, &pk);
    int key = v->state * 7919 + narrow * 11 + v->paused * 53 + (over && v->st_t >= RETRY_LOCK) * 3 +
              (title ? v->look * 31 + v->coins * 41 + v->can_continue * 43 + v->owned * 47 + v->best_m * 3 : 0) +
              (hud ? m * 101 + w->coins * 103 + pk * 107 + (pl / 16) * 109 + v->best_m * 7 : 0) +
              (banner_t > 0) * 997 + banner_kind * 991 + (v->first_run && hud && w->t < 6 * SEC) * 5003;
    if (key != ui_key) {
        hu_clear();
        if (title) {
            if (narrow) ht_logo("LEMUR LEAP", title_ramps, 2, HT_ROW_LOGO, 0);
            else hu_logo("LEMUR LEAP", title_ramps, 2, 1, 0);
        } else if (over) {
            results_panel(w, v, narrow);
        } else {
            if (narrow) hud_top_text(w, v);
            else { hud_side_bg(); hud_side_text(w, v); }
            if (banner_t) {
                static const int zk[ZONES] = {GS_ZONE_GARDEN, GS_ZONE_CLOUDS, GS_ZONE_SKY, GS_ZONE_SPACE};
                const char *s = banner_kind == 10 ? HTR(NEW_BEST_EXCL) : rs_tr(&gs_strings, zk[banner_kind & 3]);
                if (narrow) ht_big(9, s, HU_BIG_FREE);
                else hu_big(hu_center(s, 1), 7, s, HU_BIG_FREE);
            }
            if (v->first_run && w->t < 6 * SEC && !v->paused && !banner_t) {
                const char *a = v->touch ? TR(HINT_TOUCH) : TR(HINT_PAD), *b = v->touch ? TR(HINT_TOUCH2) : TR(HINT_SHOOT);
                if (narrow) { int r = ht_text_gap(7, a, 0, 1); ht_text_gap(r + 2 + hu_tall(b), b, 0, 1); }
                else { hu_text(hu_center(a, 0), 18, a); hu_text(hu_center(b, 0), 20, b); }
            }
            if (v->paused) {
                const char *q = TR(PAUSE_QUIT);
                int last = 17;
                if (narrow) last = ht_text_gap(17, q, 0, 1);
                else hu_text(hu_center(q, 0), 17, q);
                hot_set(HOT_QUIT, 16, last + 1, 0);
            }
        }
        hu_pause(0, 13);
        ui_key = key;
    }
    int slide = over ? hu_slide_in(v->st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, -hu_ui_x() & (rs_bg_map_w(RS_BG1) * 8 - 1), -slide & 255);
    if (narrow) ht_pause(v->paused, 13);
    else hu_pause(v->paused, 13);

    /* sprites, front to back */
    rs_oam_clear();
    budget_reset();
    if (title) {
        int pdy = pin ? ht_pin_dy(HT_PIN_TITLE) : 0, mx, my;
        int bounce = v->st_t % 48, lift = bounce * (48 - bounce) / 48;          /* a bouncy hop on the spot */
        int frame = bounce < 4 || bounce > 44 ? HERO_SQUASH : bounce < 24 ? HERO_UP : HERO_DOWN;
        if (narrow) {
            ht_title tc = {v->can_continue ? TR(VERB_CONTINUE) : TR(VERB), 1, 15, 0, v->can_continue ? TR(NEW_RUN) : NULL};
            ht_title_out to;
            ht_title_draw(&tc, v->st_t, 0, &to);
            title_text(v, narrow, (to.gap_y1 - pdy) / 8 - 1);
            {                                              /* the coins and the record at the screen's top, icons */
                int top = rs_screen_top() + 36, x0, x1;
                ht_view_px(&x0, &x1);
                int ux = hu_ui_x();
                char b[12];
                snprintf(b, sizeof b, "%d", v->coins);
                spr(SPR_HUD_COIN, ux + x0 + 2, top + 4, 3, 0);
                hu_number(v->coins, x0 + 16 + (int)strlen(b) * 6, top, 3);
                if (v->best_m) {
                    snprintf(b, sizeof b, "%d", v->best_m);
                    int w = (int)strlen(b) * 12;
                    spr(SPR_FLAG, ux + x1 - w - 18, top, 3, 0);
                    hu_number(v->best_m, x1 - w / 2 - 2, top, 3);
                }
                budget_kit(top, 16, 10);
            }
            if (v->can_continue) {                         /* the hint's rows: B: NEW CLIMB (a tap there: a new climb) */
                int hr = (to.gap_y1 - pdy + 2) / 8;
                hot_set(HOT_NEWRUN, hr, hr + ht_text_rows_gap(TR(NEW_RUN), 0, 1), HT_PIN_TITLE);
            }
            ht_title_sprites(&tc, &to, v->st_t, NULL, NULL);
            hot_set(HOT_PROMPT, (to.glyph_y - pdy) / 8, (to.glyph_y - pdy) / 8 + 2, HT_PIN_TITLE);
            budget_kit(to.glyph_y, 16, 1);
            mx = COL_X + COL_W / 2;                        /* upright: hopping on the garden's ground, at the bottom */
            my = STAGE_H - 36 - 25;
        } else {
            hu_title_draw(v->st_t, 0);
            title_text(v, narrow, 29);
            hu_title_sprites(v->st_t, NULL, NULL);
            budget_kit(HU_ROW_PROMPT * 8 - 4, 16, 1);
            mx = 160 + hu_ui_x();
            my = 13 * 8 - 12 - 23;
        }
        int hat = v->look < HATS ? v->look : v->hat;
        if (v->look >= HATS && v->fur != v->look - HATS) {
            rs_pal_load(RS_PAL_OBJ(OBJ_HERO), ll_pal_fur[(v->look - HATS) & 3], 16);   /* the fur shown */
            cur_fur = -1;
        }
        if (!narrow) spr(SPR_LEAF + (frame == HERO_SQUASH), mx - 16, my + 23 + 1, 2, 0);
        else {                                             /* upright: a real bounce off the ground, leaves above it */
            lift = lift * 5;
            static const int16_t LV[3][2] = {{36, 150}, {124, 116}, {72, 84}};
            for (int k = 0; k < 3; k++) spr(SPR_LEAF, COL_X + LV[k][0], LV[k][1], 2, 0);
        }
        draw_lemur_at(NULL, frame, 0, mx - 12, my - lift + 2, hat, 0);
        if (!(v->owned & (1 << v->look))) spr(SPR_LOCK, mx + 12, my + 20, 3, 0);
        draw_ground(cam, 1);
        return;
    }
    if (over) {
        if (v->st_t >= RETRY_LOCK) {
            if (narrow) ht_retry_line_at(RES_RETRY, v->st_t, RETRY_LOCK, HTR(BACK_TITLE));
            else hu_retry_line_at(RES_RETRY, v->st_t, RETRY_LOCK, HTR(BACK_TITLE));
        }
        int md = medal_of(m);
        if (md) {
            int width = narrow ? ht_ncols() : 24, x = narrow ? ht_center_w(width) : (40 - width) / 2;
            int mxp = (x + width) * 8 - 32 + hu_ui_x(), myp = RES_ROW(0) * 8 + 2 + slide + ht_pin_dy(pin);
            hu_medal(md, mxp - hu_ui_x(), myp, 3);
            if ((v->st_t / 20) % 3 == 0) hu_sparkle(mxp - hu_ui_x() + 20, myp, (v->st_t / 10) % 2, 3);
            budget_kit(myp, 24, 2);
        }
        draw_parts(cam);
        draw_plats(w, cam);
        return;
    }
    /* the HUD's sprites */
    if (narrow) hud_top_sprites(w, narrow);
    else hud_side_sprites(w, v);
    draw_parts(cam);
    draw_hero(w, cam, v->hat);
    draw_world_bits(w, cam);
    draw_plats(w, cam);
    draw_best(w, cam);
    draw_ground(cam, 0);
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(laid_w); S(laid_h); S(ui_key); S(tick); S(cur_fur); S(cur_zone); S(parts); S(part_next); S(banner_t);
    S(banner_kind); S(hurt_t); S(draw_hot);
    RS_STATE_RASTER(raster);
}
#undef S
