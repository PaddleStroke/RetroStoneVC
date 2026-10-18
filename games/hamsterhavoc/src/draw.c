/*
 * Hamster Havoc: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1); BG2 the big cheese moon below and the four cheese shelters (their tiles
 *   are drawn from the world's bitmaps every frame: eroded pixel by pixel); BG3 the space decor (planets, a nebula,
 *   faint and without outline); BG4 the stars (twinkling: two colours set per frame, drifting slowly); the backdrop a
 *   raster gradient, deep navy above, violet down to the moon's horizon.
 *   Sprites: the hamsters' ships (OBJ 0; players 2-4 OBJ 1, 4, 5: the fur recoloured), the mice and their pops
 *   (OBJ 2), the kit's digits, glyphs and medals (OBJ 3), the crumbs and the saucer (OBJ 6), the mothership (OBJ 7).
 * Widescreen (docs/spec.md "Widescreen"): the 320-px stage is centred (rs_screen_origin) and a wider screen shows
 * more space on both sides (planets, a comet, the moon's curve); the play stays in the stage's 320 columns.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

/* the title logo: one 3-shade ramp per word (light, mid, dark): HAMSTER in cheese gold, HAVOC in space violet */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 238, 150), RS_RGB8(250, 196, 64), RS_RGB8(212, 136, 30)},
    {RS_RGB8(220, 204, 255), RS_RGB8(164, 134, 244), RS_RGB8(112, 84, 204)}};
/* the backdrop: deep navy at the top, violet down at the moon's horizon (0xRRGGBB) */
#define SKY_TOP     0x0a0c28
#define SKY_LOW     0x2c1e56
#define TITLE_SHIP_Y 92
#define WAVE_ROW     4          /* the intro's WAVE n: between the score and the formation's top (line 48 at wave 1) */

static const int SHELTER_X[SHELTERS] = SHELTER_XS;
static rs_color line_col[RS_SCREEN_H];
static int shown_key = -1, shown_best = -1;

/* a tall screen (rs_screen_tall: a phone held upright): this frame's scrolls of the stars and the decor, for the
 * raster's lines past the stage (not saved: set by draw_frame before every picture) */
static int star_sx, star_sy, deco_sx, deco_sy;
/* a tall screen's HUD (the score chips and the 3-4 players' tags) at the screen's top: BG1's top HUD_LINES drawn
 * there (rs_bg_pin), the chips two rows (P3 and P4 at line HUD_ROW2, not at the stage's foot); 0: none (set by
 * draw_frame before every picture, not saved) */
#define HUD_LINES  32
#define HUD_ROW2   20
static int hud_lines;
static int hud_bottom(void) { return rs_screen_height() > RS_SCREEN_H ? HUD_ROW2 : 0; }
#define PLANET_X   72           /* the ringed planet above the stage: its centre, stage x ... */
#define PLANET_PX  360          /* ... the decor's (stage x of the margin's planet, on a wide screen) */
#define PLANET_Y0  44           /* the decor's lines shown for it (the planet and its ring) */
#define PLANET_H   64

/* The portrait playfield (game.h, DESIGN.md "Tall screen"): a zoomed screen of ZOOM_H lines whose middle 188..220
 * columns show. The UI is laid out in the narrow columns NC0 .. NC0 + NCOLS - 1 (x 72..247: inside the 188 columns of
 * a 9:21 phone, whatever the phone), the HUD as sprites at the screen's top, the title's logo stacked and pinned at
 * the screen's top (rs_bg_pin). */
#define NC0        9
#define NCOLS      22
#define TPLANET_X  210          /* the ringed planet, stage x, above the stage (behind the saucer's lane) */
#define TPLANET_Y  (-78)        /* its top line */
#define TLOGO_ROWS 13           /* the stacked logo: UI rows 0..12, pinned at screen line TLOGO_Y */
#define TLOGO_Y    6
#define TWAVE_ROW  13
#define TTITLE_MICE_Y 28        /* the title's sprites between the logo and the slots (stage y) */
#define TTITLE_SHIP_Y 76           /* WAVE n under the formation (it flies in at the screen's top) */
static int ui_band;             /* the title's logo band pinned at the screen's top (lines); 0: none (set by
                                 * draw_frame before every picture, not saved) */
static int maps_tall = -1;      /* the layout the BG maps were set up for (saved: a state brings its maps) */
int draw_tall(void) { return rs_screen_height() >= ZOOM_H; }

/* A tall screen's lines past the 240 of the stage (DESIGN.md "Tall screen"). Above: more space, the sky deeper toward
 * the screen's top, the stars again (BG4, its 240 lines repeated, shifted sideways) and the decor's ringed planet
 * (BG3, only its columns: window 2); below: the moon's deep cheese going down, darker with depth. The UI, the moon,
 * the decor's other rows and the sprites are hidden there (window 1); line 0 puts the stage's settings back, so a
 * 240-line screen is unchanged. */
static void raster_out(int line)
{
    int top = rs_screen_top(), bot = rs_screen_bottom(), pm = rs_bg_map_w(RS_BG4) * 8 - 1;
    if (line == top || line == RS_SCREEN_H) {
        rs_window(0, rs_screen_left(), rs_screen_right());
        rs_window(1, PLANET_X - 40, PLANET_X + 40);
        for (int l = RS_BG1; l <= RS_BG4; l++) rs_bg_window(l, RS_WIN1);
        if (line < 0) rs_bg_window(RS_BG4, 0);
        if (line < 0 && hud_lines) rs_bg_window(RS_BG1, 0);    /* the HUD's band at the screen's top (rs_bg_pin) */
        rs_obj_window(line < 0 && hud_lines ? 0 : RS_WIN1);
    }
    if (hud_lines && line == top + hud_lines) rs_bg_window(RS_BG1, RS_WIN1);     /* past the HUD's band ... */
    if (hud_lines && line == top + HUD_ROW2 + 16) rs_obj_window(RS_WIN1);        /* ... and its second row's digits */
    if (line < 0) {
        int k = -line * 120 / -top;                         /* 0 at the stage's top .. 120 at the screen's */
        rs_pal_set(0, hu_scale_color(line_col[0], 256 - k));
        int band = -((239 - line) / 240);                   /* floor(line / 240) */
        rs_bg_scroll(RS_BG4, (star_sx + band * 200) & pm, (star_sy - band * 240) & 255);
        int p0 = top * 11 / 20 - PLANET_H / 2, d = line - p0;   /* the planet a little above the margin's middle */
        if (d == 0 && top <= -PLANET_H - 16) {
            rs_bg_window(RS_BG3, RS_WIN2_OUT);
            rs_bg_scroll(RS_BG3, (deco_sx + PLANET_PX - PLANET_X) & pm, (PLANET_Y0 - line) & 255);
        }
        if (d == PLANET_H) rs_bg_window(RS_BG3, RS_WIN1);
    } else {
        int d = line - RS_SCREEN_H, n = bot - RS_SCREEN_H;
        int k = n > 0 ? d * 150 / n : 0;
        rs_pal_set(0, hu_scale_color(RS_RGB8(136, 88, 38), 256 - k - ((line & 4) ? 8 : 0)));
    }
}

/* The portrait playfield's lines past the stage: above, the play goes on (the sprites shown), over the sky going
 * deeper toward the screen's top, the stars (BG4, shifted sideways) and the ringed planet (BG3, its columns only:
 * window 2); the moon's layer and the UI hidden there (they would repeat), but for the title's logo pinned at the
 * top. Below, the moon's deep cheese, as raster_out. */
static void raster_tall(int line)
{
    int top = rs_screen_top(), pm = rs_bg_map_w(RS_BG4) * 8 - 1;
    if (line == top || line == RS_SCREEN_H) {
        rs_window(0, rs_screen_left(), rs_screen_right());
        rs_window(1, TPLANET_X - 40, TPLANET_X + 40);
        for (int l = RS_BG1; l <= RS_BG4; l++) rs_bg_window(l, RS_WIN1);
        if (line < 0) rs_bg_window(RS_BG4, 0);
        rs_obj_window(line < 0 ? 0 : RS_WIN1);
    }
    if (ui_band && line == top + TLOGO_Y) rs_bg_window(RS_BG1, 0);     /* the title's logo, pinned from there */
    if (line >= 0) {
        raster_out(line);                                   /* the moon's deep cheese */
        return;
    }
    /* the sky's top colour going deeper toward the screen's top (in 8-bit steps, dithered as the stage's: no seam) */
    int k = -line * 112 / -top, dd = (line & 4) ? 1 : 0;
    int r = ((SKY_TOP >> 16) & 255) * (256 - k) / 256, gg = ((SKY_TOP >> 8) & 255) * (256 - k) / 256;
    int b = (SKY_TOP & 255) * (256 - k) / 256;
    rs_pal_set(0, RS_RGB8(r + 4 * dd, gg + 2 * dd, b + 4 * dd));
    rs_bg_scroll(RS_BG4, (star_sx - 200) & pm, (star_sy + 240) & 255);
    int d = line - TPLANET_Y;
    if (d == 0 && !ui_band) {                               /* (not behind the title's logo) */
        rs_bg_window(RS_BG3, RS_WIN2_OUT);
        rs_bg_scroll(RS_BG3, (deco_sx + PLANET_PX - TPLANET_X) & pm, (PLANET_Y0 - line) & 255);
    }
    if (d == PLANET_H) rs_bg_window(RS_BG3, RS_WIN1);
}

static void raster(int line, void *user)
{
    (void)user;
    if (line < 0 || line >= RS_SCREEN_H) {
        if (draw_tall()) raster_tall(line);
        else raster_out(line);
        return;
    }
    if (line == 0) {                            /* back from raster_out (a tall screen): the stage's own settings */
        for (int l = RS_BG1; l <= RS_BG4; l++) rs_bg_window(l, 0);
        rs_obj_window(0);
        rs_bg_scroll(RS_BG4, star_sx, star_sy);
        rs_bg_scroll(RS_BG3, deco_sx, deco_sy);
    }
    rs_pal_set(0, line_col[line]);
}

static void sky_gradient(void)
{
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int k = y < 210 ? y * 256 / 210 : 256, c[3];
        for (int i = 0; i < 3; i++) {
            int a = (SKY_TOP >> (16 - 8 * i)) & 255, b = (SKY_LOW >> (16 - 8 * i)) & 255;
            c[i] = (a * (256 - k) + b * k) >> 8;
        }
        /* house rule: a 2-line dither step every 8 lines keeps the RGB555 bands soft */
        int d = (y & 4) ? 1 : 0;
        line_col[y] = RS_RGB8(c[0] + 4 * d, c[1] + 2 * d, c[2] + 4 * d);
    }
}

static int tri(int t, int period)          /* 0..256..0 */
{
    t %= period;
    return t < period / 2 ? t * 512 / period : (period - t) * 512 / period;
}

/* ---- the maps -------------------------------------------------------------------------------------------------------
 * The moon and the decor are "stage panoramas": PANO_W tiles from stage x PANO_X0 (-128) to 447, so every width up to
 * 560 shows them once (no repeat): map column c holds stage tile c (0..), the last columns the stage's left margin
 * (negative x), as the BG wraps. Maps of 64 tiles up to 512 columns, 128 beyond (the map size is saved in states:
 * draw_state_loaded sets them up again). The stars repeat every 512 px. */
static int map_w(void) { return rs_screen_width() > 512 ? 128 : 64; }

static void put_pano(int layer, const uint16_t *pano, int rows, int row0, int mw)
{
    int lim = mw == 64 ? 52 : 56;           /* stage tiles 0..51 (55) on the right, the rest is the left margin */
    for (int c = 0; c < mw; c++) {
        int st = c < lim ? c : c - mw, i = st - PANO_X0 / 8;
        for (int y = 0; y < rows; y++)
            rs_bg_put(layer, c, row0 + y, i >= 0 && i < PANO_W ? pano[y * PANO_W + i] : 0);
    }
}

static void maps_setup(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG2, mw, 32, VR_BG2);
    rs_bg_setup(RS_BG3, mw, 32, VR_BG3);
    rs_bg_setup(RS_BG4, mw, 32, VR_BG4);
    put_pano(RS_BG2, gm_moon_map, MOON_H, MOON_Y0 / 8, mw);
    put_pano(RS_BG3, gm_decor_map, DECOR_H, 0, mw);
    for (int y = 0; y < STARS_H; y++)
        for (int x = 0; x < mw; x++) rs_bg_put(RS_BG4, x, y, gm_stars_map[y * STARS_W + x % STARS_W]);
    /* the shelters: 3x2 tiles each, redrawn from the world every frame (portrait: three, the moon's layer shifted
     * 4 px right so that they sit centred) */
    static const int tall_x[TALL_SHELTERS + 1] = TALL_SHELTER_XS;
    maps_tall = draw_tall();
    for (int s = 0; s < (maps_tall ? TALL_SHELTERS : SHELTERS); s++)
        for (int ty = 0; ty < 2; ty++)
            for (int tx = 0; tx < 3; tx++)
                rs_bg_put(RS_BG2, (maps_tall ? tall_x[s] - 4 : SHELTER_X[s]) / 8 + tx, SHELTER_Y / 8 + ty,
                          (uint16_t)((SHELTER_TILE + s * 6 + ty * 3 + tx) | (PAL_MOON << 10)));
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more space on both sides */
    /* a tall screen (portrait): the stage low, two thirds of the extra lines above it (space), a third below (the
     * moon); the portrait playfield: its own place (TALL_ORIGIN_V) */
    rs_screen_origin_v(draw_tall() ? TALL_ORIGIN_V : (rs_screen_height() - RS_SCREEN_H) * 2 / 3);
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;         /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    rs_tiles_load(VR_BG2, gm_moon_tiles, gm_moon_tile_count);
    rs_tiles_load(VR_BG3, gm_decor_tiles, gm_decor_tile_count);
    rs_tiles_load(VR_BG4, gm_stars_tiles, gm_stars_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_MOON), gm_moon_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_DECOR), gm_decor_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_STARS), gm_stars_pal, 16);
    maps_setup();
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* the hamster (golden), P2 (white), the mice */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 64);     /* P3 (grey), P4 (ginger), the crumbs and saucer, the boss */
    sky_gradient();
    rs_raster(raster, NULL);
}

/* ---- the shelters' tiles: the wedge's pixels where the world still has its bits -------------------------------- */
static void shelters_upload(const world *w, int show)
{
    uint8_t px[6 * 64];
    for (int s = 0; s < SHELTERS; s++) {
        memset(px, 0, sizeof px);
        if (show)
            for (int y = 0; y < SHELTER_H; y++)
                for (int x = 0; x < SHELTER_W; x++)
                    if (w->shelter[s][y] >> x & 1)
                        px[((y / 8) * 3 + x / 8) * 64 + (y % 8) * 8 + x % 8] = gm_shelter_px[y * SHELTER_W + x];
        rs_tiles_load8(VR_BG2 + SHELTER_TILE + s * 6, px, 6);
    }
}

/* ---- sprites -------------------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal, int flags)
{
    const gm_sprite_def *d = &gm_spr[id];
    int t0 = draw_tall() ? rs_screen_top() : 0, t1 = draw_tall() ? rs_screen_bottom() : RS_SCREEN_H;
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= t0 - (int)d->h || y >= t1) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

/* player p's sprite palette: the house rule (P1 OBJ 0, P2 OBJ 1, P3 OBJ 4, P4 OBJ 5: house_ui.h hu_player_pal) */
static int player_pal(int p) { return p ? hu_player_pal(p) : 0; }

/* small digits (the points of a saucer, the mothership), centred on cx */
static void small_number(int n, int cx, int y)
{
    char s[8];
    int len = snprintf(s, sizeof s, "%d", n);
    for (int i = 0; i < len; i++) spr(SPR_DIGIT + (s[i] - '0'), cx - len * 3 + i * 6 - 1, y, 2, -1, 0);
}

static void draw_ship(const world *w, int p, int sx, int sy)
{
    const hero *h = &w->h[p];
    if (h->state == SH_DOWN || h->state == SH_OFF) return;
    int x = (h->x >> 16) + sx, y = SHIP_Y + sy, fl = h->face < 0 ? RS_SPR_HFLIP : 0, id;
    if (h->state == SH_POP) {                         /* bonk: X eyes, stars circling, the ship wobbling */
        static const int ox[8] = {0, 5, 7, 5, 0, -5, -7, -5}, oy[8] = {-7, -5, 0, 5, 7, 5, 0, -5};
        int wob = (h->t / 4) % 2 ? 1 : -1;
        if (h->t < POP_FRAMES - 8 || (h->t / 2) % 2)
            spr(SPR_SHIP_HIT + (h->t / 8) % 2, x - 16 + (h->t < 20 ? wob : 0), y, 2, player_pal(p), fl);
        for (int i = 0; i < 3 && h->t < 48; i++) {
            int k = (h->t / 3 + i * 3) & 7;
            spr(SPR_SPARKLE + (i + (h->t / 6)) % 2, x - 4 + ox[k] * 2, y - 6 + oy[k] / 2, 2, -1, 0);
        }
        return;
    }
    if (h->fire_t < 5) id = SPR_SHIP_FIRE;
    else id = SPR_SHIP + ((w->t + p * 37) % 150 < 8);     /* the hamster blinks now and then */
    int bob = h->state == SH_READY ? 0 : ((w->t / 16) % 2);
    spr(id, x - 16, y + bob, 2, player_pal(p), fl);
    if (h->shield_t && (h->shield_t > 120 || (h->shield_t / 6) % 2))
        spr(SPR_BUBBLE, x - 16, y - 7 + bob, 2, 0, 0);
    if (h->power && (h->power_t > 120 || (h->power_t / 6) % 2))      /* the power's little badge */
        spr(SPR_BADGE + h->power - 1, x + 8, y - 2 + bob, 2, 0, 0);
}

/* the title's player slots: each joined player's icon, in its colours (hu_title_sprites calls it) */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr(SPR_ICON, cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), 2, player_pal(p), 0);
}

/* 2-4 players: ranked by score */
static void standing(const world *w, hu_standing *s)
{
    int v[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) v[p] = w->h[p].score;
    hu_rank(s, w->players, v, v);
}

/* the three kinds by their points (landscape: row 0, rows 1-2, rows 3-4) */
static int mouse_sprite(const world *w, int row)
{
    int pts = world_geo(w)->row_points[row];
    return pts >= 30 ? SPR_MOUSE_C : pts >= 20 ? SPR_MOUSE_B : SPR_MOUSE_A;
}

/* the intro's slide: the formation (or the mothership) flies in from above */
static int intro_slide(const world *w)
{
    if (w->phase != PH_INTRO) return 0;
    return hu_slide_in(w->phase_t, 60, 150);
}

static void draw_world(const world *w, int sx, int sy)
{
    int slide = intro_slide(w);
    /* the effects (front) */
    for (int i = 0; i < FX_MAX; i++) {
        const effect *e = &w->fxs[i];
        if (!e->on) continue;
        switch (e->kind) {
        case FX_POP:
            spr(SPR_POP + e->t / 6, e->x + sx, e->y + sy, 2, -1, 0);
            break;
        case FX_SPLAT:
            spr(SPR_SPLAT + (e->t >= 6), e->x - 4 + sx, e->y - 4 + sy, 2, -1, 0);
            break;
        case FX_POINTS:
            small_number(e->value, e->x + sx, e->y - e->t / 4 + sy);
            break;
        case FX_BIGPOP: {                             /* the saucer's (and the mothership's, bigger) */
            int n = e->value ? 8 : 4, r = (e->value ? 6 : 3) + e->t / (e->value ? 2 : 3);
            static const int dx[8] = {-1, 1, 1, -1, 0, 2, 0, -2}, dy[8] = {-1, -1, 1, 1, -2, 0, 2, 0};
            if (e->t < 36)
                for (int k = 0; k < n; k++)
                    spr(SPR_POP + (e->t / 12) % 3, e->x - 8 + dx[k] * r / (k < 4 ? 1 : 1) + sx, e->y - 8 + dy[k] * r + sy,
                        2, -1, 0);
            for (int k = 0; k < (e->value ? 6 : 3) && e->t < 50; k++)
                spr(SPR_SPARKLE + (k + e->t / 5) % 2, e->x - 4 + ((k * 37 + e->t * 3) % 48) - 24 + sx,
                    e->y - 4 + ((k * 23) % 24) - 12 + e->t / 3 + sy, 2, -1, 0);
            break;
        }
        case FX_BUBBLE:
            spr(SPR_POP + e->t / 6, e->x - 8 + sx, e->y - 8 + sy, 2, 0, 0);
            break;
        }
    }
    /* the seeds and the crumbs */
    for (int p = 0; p < w->players; p++)
        for (int i = 0; i < MAX_SEEDS; i++) {
            const seed *s = &w->seeds[p][i];
            if (s->on) spr(SPR_SEED, (s->x >> 16) - 4 + sx, (s->y >> 16) - 1 + sy, 2, 0, 0);
        }
    for (int i = 0; i < BOMB_MAX; i++) {
        const bomb *b = &w->bombs[i];
        if (!b->on) continue;
        int f = (w->t / 6 + i) % 2;
        spr(SPR_CRUMB + b->kind * 2 + f, (b->x >> 16) - 4 + sx, (b->y >> 16) + sy, 2, -1, 0);
    }
    /* the hamsters (P1 in front: the lower OAM index is in front) */
    for (int p = 0; p < w->players; p++) draw_ship(w, p, sx, sy);
    if (w->power_on)
        spr(SPR_POWER + w->power_kind - 1, (w->power_x >> 16) - 8 + sx, (w->power_y >> 16) + sy + hu_bob(w->t, 32, 1), 2,
            0, 0);
    /* the formation */
    const geo *g = world_geo(w);
    if (w->alive_n && w->phase != PH_CLEAR) {
        for (int r = 0; r < g->rows; r++)
            for (int c = 0; c < g->cols; c++)
                if (w->alive[r][c])
                    spr(mouse_sprite(w, r) + w->fframe, mouse_x(w, c) + sx, mouse_y(w, r) - slide + sy, 2, -1,
                        (r + c) % 2 && g->row_points[r] != 30 ? RS_SPR_HFLIP : 0);
    }
    /* the saucer */
    if (w->saucer_on) spr(SPR_SAUCER + (w->t / 8) % 2, (w->saucer_x >> 16) - 16 + sx, g->saucer_y + sy, 2, -1, 0);
    /* the mothership and its hits left */
    if (w->boss_on) {
        int bx = (w->boss_x >> 16), by = boss_y(w) - slide;
        spr(SPR_BOSS + (w->t / 16) % 2, bx - 32 + sx, by + sy, 2, -1, 0);
        if (w->phase == PH_PLAY) {
            int segs = 8, full = w->boss_hp * segs * 2 / w->boss_hp0;
            for (int k = 0; k < segs; k++) {
                int f = full >= 2 * k + 2 ? 0 : full == 2 * k + 1 ? 1 : 2;
                spr(SPR_HP + f, bx - segs * 4 + k * 8 + sx, by + 34 + sy, 2, -1, 0);
            }
        }
    }
}

/* the mothership flashes when hit: its palette (OBJ 7) whitened for a few frames */
static void boss_palette(const world *w)
{
    int flash = w->boss_on && w->boss_flash && (w->boss_flash & 2);
    for (int i = 1; i < 16; i++) {
        rs_color c = gm_obj_pals[7 * 16 + i];
        rs_pal_set(RS_PAL_OBJ(7) + i, flash ? hu_lerp_color(c, RS_RGB8(255, 255, 255), 180) : c);
    }
}

/* ---- the BG screens -------------------------------------------------------------------------------------------- */
static void text_centered(int row, const char *s, int big)
{
    if (big && hu_width(s, 1) > 38) big = 0;           /* a long translation: the small letters */
    if (big) hu_big(hu_center(s, 1), row, s, HU_BIG_FREE);
    else hu_text(hu_center(s, 0), row, s);
}

/* ---- the portrait screens: the kit's layouts again in the NCOLS narrow columns ---------------------------------- */
/* s cut at its spaces into lines of at most cols columns (at most 3; the last takes the rest) */
static int nsplit(const char *s, int cols, char l[3][96], int big)
{
    int n = 0;
    l[0][0] = 0;
    while (*s) {
        while (*s == ' ') s++;
        const char *e = strchr(s, ' ');
        int len = e ? (int)(e - s) : (int)strlen(s);
        if (!len) break;
        char buf[96];
        snprintf(buf, sizeof buf, "%s%s%.*s", l[n], l[n][0] ? " " : "", len, s);
        if (l[n][0] && hu_width(buf, big) > cols && n < 2) snprintf(l[++n], 96, "%.*s", len, s);
        else snprintf(l[n], 96, "%s", buf);
        s += len;
    }
    return l[0][0] ? n + 1 : 0;
}

/* s centred in the narrow columns from row on, wrapped (small letters; box: on a panel); returns the rows used */
static int nlines(int row, const char *s, int cols, int box)
{
    char l[3][96];
    int n = nsplit(s, cols, l, 0), st = 1;
    for (int i = 1; i < n; i++)
        if (hu_tall(l[i])) st = 2;
    for (int i = 0; i < n; i++) {
        if (box) hu_box_text(hu_center(l[i], 0), row + i * st, l[i]);
        else hu_text(hu_center(l[i], 0), row + i * st, l[i]);
    }
    return n ? (n - 1) * st + 1 : 0;
}

/* a message: big letters when they fit the narrow columns, else small ones, wrapped */
static void ntext(int row, const char *s, int big)
{
    if (big && hu_width(s, 1) <= NCOLS) hu_big(hu_center(s, 1), row, s, HU_BIG_FREE);
    else nlines(row, s, NCOLS, 0);
}

/* hu_banner in the narrow columns: its big letters if they fit, else a panel of small lines */
static void nbanner(int y, const char *title)
{
    int w = hu_width(title, 1) + 2;
    if (w <= NCOLS) {
        int acc = hu_tall(title);                   /* accents (and CJK): one more row on top */
        if (w < 16) w = 16;
        hu_panel(20 - w / 2, y - acc, w, 4 + acc);
        hu_big(hu_center(title, 1), y + 1, title, HU_BIG_BANNER);
        return;
    }
    char l[3][96];
    int n = nsplit(title, NCOLS - 2, l, 0);
    hu_panel(NC0, y, NCOLS, n * 2 + 2);
    nlines(y + 1 + (n == 1), title, NCOLS - 2, 1);
}

/* the stacked logo: hu_logo at UI row 0, then its words moved to lines 5 rows apart, each centred (the map entries
 * moved: the logo's tiles stay) */
static void nlogo(const char *title)
{
    hu_logo(title, title_ramps, 2, 0, 0);
    int layer = hu_cfg()->layer, len = (int)strlen(title), sc = (len * 22 - 2 <= 304) ? 4 : 3, adv = 5 * sc + 2;
    int x0 = (320 - (len * adv - 2)) / 2, cut[4], nw = 1;
    cut[0] = 0;
    for (int i = 0; i < len && nw < 3; i++)
        if (title[i] == ' ') cut[nw++] = (x0 + i * adv + adv / 2) / 8;
    cut[nw] = 40;
    uint16_t save[8][40], blank = rs_bg_get(layer, 0, 0);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 40; x++) {
            save[y][x] = rs_bg_get(layer, x, y);
            rs_bg_put(layer, x, y, blank);
        }
    for (int k = 0; k < nw; k++) {
        int a = cut[k + 1], b = cut[k];
        for (int x = cut[k]; x < cut[k + 1]; x++)
            for (int y = 0; y < 8; y++)
                if (save[y][x] != blank) {
                    if (x < a) a = x;
                    if (x + 1 > b) b = x + 1;
                }
        int dx = 20 - (a + b) / 2;
        for (int y = 0; y < 8; y++)
            for (int x = a; x < b; x++)
                if (save[y][x] != blank && x + dx >= 0 && x + dx < 40) rs_bg_put(layer, x + dx, y + k * 5, save[y][x]);
    }
}

/* the title's rows, from the bottom: the credits (24, 25: above the moon), the join line ending on row 22, BEST, the
 * prompt, the slots (the kit's hu_title_draw in the narrow columns) */
typedef struct tall_title { int slots, prompt, best, join; } tall_title;
static void tall_title_rows(tall_title *r, char *prompt, char *join, int size)
{
    char who[64];
    int n = hu_players();
    rs_strf(prompt, (size_t)size, HTR(PRESS_TO), HTR(PLAY));
    if (n < MAX_PLAYERS) {
        who[0] = 0;
        for (int p = n; p < MAX_PLAYERS; p++) {
            size_t k = strlen(who);
            if (p > n) k += (size_t)snprintf(who + k, sizeof who - k, " / ");
            if (k < sizeof who) rs_strf(who + k, sizeof who - k, HTR(PLAYER_TAG), p + 1);
        }
        rs_strf(join, (size_t)size, hu_str(n > 1 ? HS_JOIN_LEAVE : HS_JOIN), who);
    } else {
        rs_strf(join, (size_t)size, hu_strn(HS_FULL, n), n);
    }
    char l[3][96];
    int jn = nsplit(join, NCOLS, l, 0), jst = jn > 1 && (hu_tall(l[1]) || (jn > 2 && hu_tall(l[2]))) ? 2 : 1;
    r->join = 23 - ((jn - 1) * jst + 1);
    r->best = r->join - 2;
    int pn = nsplit(prompt, NCOLS - 4, l, 0), pst = pn > 1 && hu_tall(l[1]) ? 2 : 1;
    r->prompt = r->best - 1 - ((pn - 1) * pst + 1);
    r->slots = r->prompt - 3;
}

static void tall_title_draw(int t, int best)
{
    char s[160], prompt[160], join[160];
    tall_title r;
    tall_title_rows(&r, prompt, join, sizeof prompt);
    hu_clear_rows(r.slots, 30 - r.slots);
    for (int p = 0; p < hu_players(); p++) {
        rs_strf(s, sizeof s, HTR(PLAYER_TAG), p + 1);
        hu_text(10 + 5 * p, r.slots, s);
    }
    if (hu_blink(t)) nlines(r.prompt, prompt, NCOLS - 4, 0);
    if (best > 0) {
        rs_strf(s, sizeof s, HTR(BEST_N), best);
        hu_text(hu_center(s, 0), r.best, s);
    }
    nlines(r.join, join, NCOLS, 0);
    hu_text(hu_center("(C) 2026 8BCRAFT", 0), 24, "(C) 2026 8BCRAFT");
    hu_text(hu_center("RETROSTONE VC", 0), 25, "RETROSTONE VC");
}

static void tall_title_sprites(int t)
{
    char prompt[160], join[160], l[3][96];
    tall_title r;
    tall_title_rows(&r, prompt, join, sizeof prompt);
    if (nsplit(prompt, NCOLS - 4, l, 0))
        hu_glyph(HU_BTN_A, hu_center(l[0], 0) * 8 - 20, r.prompt * 8 - 4, (t / 30) % 2, 3);
    for (int p = 0; p < hu_players(); p++) {
        int cx = (10 + 5 * p) * 8 + 28, cy = r.slots * 8 + 4, a = hu_player_age(p);
        if (a < 24) {                                   /* it pops in, with sparkles (as the kit's) */
            hu_sparkle(cx + 8, cy - 14, (a / 6) & 1, 3);
            hu_sparkle(cx - 14, cy - 2, (a / 6 + 1) & 1, 3);
        }
        slot_icon(p, cx, cy + (a < 16 ? 12 - hu_ease_back(a, 16, 12) : 0), t, NULL);
    }
}

/* the game-over panel (1 player) in the narrow columns: the labels on the left, the values right-aligned */
#define TGO_RIGHT 240
static void tall_gameover_panel(const world *w, int new_best)
{
    char s[64];
    nbanner(4, HTR(GAME_OVER));
    hu_panel(NC0, 9, NCOLS, 17);
    hu_box_text(NC0 + 1, 11, HTR(SCORE));
    hu_box_text(NC0 + 1, 14, new_best ? HTR(NEW_BEST) : HTR(BEST));
    hu_box_text(NC0 + 1, 17, HTR(MEDAL));
    rs_strf(s, sizeof s, TR(WAVE_N), w->wave);
    hu_box_text(hu_center(s, 0), 20, s);
}

static int ndigits(int n)
{
    int d = 1;
    while (n >= 10) { n /= 10; d++; }
    return d;
}

static void tall_gameover_sprites(int score, int best, int medal, int t, int oy)
{
    hu_number(score, TGO_RIGHT - ndigits(score) * 6, 11 * 8 - 4 + oy, 3);
    hu_number(best, TGO_RIGHT - ndigits(best) * 6, 14 * 8 - 4 + oy, 3);
    if (medal) {
        hu_medal(medal, TGO_RIGHT - 24, 16 * 8 - 4 + oy, 3);
        if ((t / 20) % 3 == 0) hu_sparkle(TGO_RIGHT - 30, 16 * 8 - 4 + oy, (t / 10) % 2, 3);
    } else {
        hu_box_text(TGO_RIGHT / 8 - 2, 17, "-");
    }
}

/* the ranking (2-4 players) in the narrow columns: the kit's rows and sprites, a narrower panel and banner */
static void tall_results_panel(const hu_standing *s)
{
    char t[96], tag[16];
    int win = hu_winner(s);
    if (win >= 0) rs_strf(t, sizeof t, HTR(WINS), win + 1);
    else snprintf(t, sizeof t, "%s", HTR(DRAW));
    nbanner(3, t);
    hu_panel(NC0, 8, NCOLS, 3 * s->n + 4);
    for (int i = 0; i < s->n; i++) {
        int row = hu_results_row(s, i);
        hu_box_text(NC0 + 1, row, hu_str(HS_RANK_1 + (s->rank[i] & 3)));
        rs_strf(tag, sizeof tag, HTR(PLAYER_TAG), (s->order[i] & 3) + 1);
        hu_box_text(14, row, tag);
    }
}

/* the HUD at the screen's top (sprites): 1 player the score centred; 2 the halves of the middle; 3-4 two rows, each
 * with the player's head (no tags: they would reach the app's back and pause buttons in the corners) */
static void tall_hud(const world *w)
{
    int top = rs_screen_top();
    if (w->players == 1) {
        hu_number_at(w->h[0].score, 160, top + 6, 0, 3);
        return;
    }
    for (int p = 0; p < w->players; p++) {
        int right = p & 1, y = top + (w->players > 2 ? 2 + (p / 2) * 18 : 6);
        if (w->players == 2) {
            hu_number_at(w->h[p].score, right ? 196 : 124, y, 0, 3);
            continue;
        }
        hu_number_at(w->h[p].score, right ? 206 : 114, y, right ? 1 : -1, 3);
        spr(SPR_ICON, right ? 208 : 96, y, 3, player_pal(p), right ? RS_SPR_HFLIP : 0);
    }
}

static int screen_key(const world *w, int state, int st_t)
{
    int screen = state == ST_DEAD ? ST_PLAY : state;
    int key = screen * 1000;
    if (screen == ST_PLAY) {
        if (w->phase == PH_INTRO || (w->phase == PH_READY && w->started == 0)) key += 100 + w->wave % 100;
        if (w->msg) key += 200 + w->msg * 10 + w->msg_player;
        if (w->landed) key = screen * 1000 + 999;
    }
    if (state == ST_OVER && st_t >= RETRY_LOCK) key += 1;
    return key;
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    int key = screen_key(w, state, st_t);
    if (key != shown_key || best != shown_best) {
        char s[64];
        int tall = draw_tall();
        hu_clear();
        if (state == ST_TITLE) {
            if (tall) nlogo("HAMSTER HAVOC");
            else hu_logo("HAMSTER HAVOC", title_ramps, 2, 2, 0);
        }
        if ((state == ST_PLAY || state == ST_DEAD) && w->players >= 3 && !tall) hu_score_tags(w->players, hud_bottom());
        if (state == ST_PLAY || state == ST_DEAD) {
            if (w->landed) {
                if (tall) ntext(13, TR(LANDED), 1);
                else text_centered(13, TR(LANDED), 1);
            } else if (w->phase == PH_INTRO) {
                /* WAVE n above the formation (in the middle it covered the mice); the mothership's waves keep the
                 * middle, under the ship */
                int boss = world_is_boss(w);
                rs_strf(s, sizeof s, TR(WAVE_N), w->wave);
                if (tall) {
                    ntext(boss ? 12 : TWAVE_ROW, s, 1);
                    if (boss) ntext(15, TR(BOSS), 1);
                } else {
                    text_centered(boss ? 12 : WAVE_ROW, s, 1);
                    if (boss) text_centered(15, TR(BOSS), 1);
                }
            }
            if (w->msg && !w->landed) {
                const char *m = w->msg == MSG_DOUBLE ? TR(POWER_DOUBLE) :
                                w->msg == MSG_RAPID ? TR(POWER_RAPID) : TR(POWER_SHIELD);
                if (w->players > 1) {
                    char tag[16];
                    rs_strf(tag, sizeof tag, HTR(PLAYER_TAG), w->msg_player + 1);
                    rs_strf(s, sizeof s, "%s %s", tag, m);
                    m = s;
                }
                if (tall) ntext(18, m, 0);
                else text_centered(18, m, 0);
            }
        }
        if (state == ST_OVER && tall) {
            if (w->players == 1) tall_gameover_panel(w, new_best);
            else tall_results_panel(rs);
        } else if (state == ST_OVER) {
            if (w->players == 1) {
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->h[0].score, 0);
                rs_strf(s, sizeof s, TR(WAVE_N), w->wave);
                hu_gameover_note(s);
            } else {
                hu_results_panel(rs, NULL);            /* P2 WINS! and the ranking */
            }
        }
        shown_key = key;
        shown_best = best;
    }
    if (state == ST_TITLE) {
        if (draw_tall()) tall_title_draw(st_t, best);
        else hu_title_draw(st_t, best);
    }
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line_at(draw_tall() ? 23 : 19, st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    static const int medals[4] = MEDAL_SCORES;
    int sx = hu_shake_x(), sy = hu_shake_y(), t = state == ST_TITLE ? st_t : w->t;
    hu_standing rs;
    standing(w, &rs);

    /* the stars twinkle and drift; the decor and the moon stay */
    int ka = 120 + tri(t, 96) / 2, kb = 120 + tri(t + 48, 96) / 2;
    rs_pal_set(RS_PAL_BG(PAL_STARS) + IDX_STAR_A, hu_lerp_color(RS_RGB8(60, 64, 120), RS_RGB8(255, 252, 220), ka));
    rs_pal_set(RS_PAL_BG(PAL_STARS) + IDX_STAR_B, hu_lerp_color(RS_RGB8(60, 64, 120), RS_RGB8(210, 226, 255), kb));
    int pm = map_w() * 8 - 1;
    star_sx = (t / 24 - sx) & pm;
    star_sy = (-sy) & 255;
    deco_sx = (-sx) & pm;
    deco_sy = (-sy) & 255;
    rs_bg_scroll(RS_BG4, star_sx, star_sy);
    rs_bg_scroll(RS_BG3, deco_sx, deco_sy);
    int tall = draw_tall();
    rs_bg_scroll(RS_BG2, (-sx - (tall ? 4 : 0)) & pm, (-sy) & 255);  /* portrait: 4 px right (the shelters) */
    shelters_upload(w, state == ST_PLAY || state == ST_DEAD);    /* not behind the game-over panels (cut by them) */
    boss_palette(w);

    screen_text(w, state, st_t, best, new_best, &rs);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;     /* the panel slides up, ease-out */
    rs_bg_scroll(RS_BG1, 0, -slide);
    /* a tall screen (DESIGN.md "Tall screen"): during a run the score chips and the tags at the screen's top */
    hud_lines = !tall && rs_screen_height() > RS_SCREEN_H && (state == ST_PLAY || state == ST_DEAD) ? HUD_LINES : 0;
    /* the portrait title: the stacked logo at the screen's top */
    ui_band = tall && state == ST_TITLE ? TLOGO_ROWS * 8 : 0;
    if (tall) rs_bg_pin(RS_BG1, 0, ui_band, TLOGO_Y);
    else rs_bg_pin(RS_BG1, 0, hud_lines, 0);
    hu_pause(paused, 13);

    rs_oam_clear();
    /* front to back: the UI, the play, the decor sprites */
    if ((state == ST_PLAY || state == ST_DEAD) && tall) {
        tall_hud(w);
    } else if (state == ST_PLAY || state == ST_DEAD) {
        int v[MAX_PLAYERS];
        for (int p = 0; p < w->players; p++) v[p] = w->h[p].score;
        /* 1: centred; 2: x 80 / 240; 3-4: the corners (a tall screen: all at the screen's top, two rows) */
        for (int p = 0; p < w->players; p++) {
            hu_chip c = hu_score_chip(w->players, p, hud_bottom());
            hu_number_at(v[p], c.x, c.y + (hud_lines ? rs_screen_top() : 0), c.align, 3);
        }
    }
    if (state == ST_OVER) {
        if (w->players == 1 && tall) {
            tall_gameover_sprites(w->h[0].score, best, hu_medal_of(w->h[0].score, medals), st_t, slide);
        } else if (w->players == 1) {
            hu_gameover_sprites(1, w->h[0].score, 0, best, hu_medal_of(w->h[0].score, medals), st_t, slide);
        } else {
            hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_ICON, cx - 8, cy - 8 + slide, 3, player_pal(rs.order[i]), 0);
            }
        }
    }
    if (state == ST_TITLE && tall) {
        tall_title_sprites(st_t);
        /* between the logo and the slots: a row of mice marching, the hamster's ship bobbing under it, a seed */
        int b = hu_bob(st_t, 64, READY_BOB), f = (st_t / 30) % 2, step = ((st_t / 30) % 8) - 4;
        step = step < 0 ? -step : step;
        for (int i = 0; i < 5; i++)
            spr((i == 2 ? SPR_MOUSE_C : i & 1 ? SPR_MOUSE_B : SPR_MOUSE_A) + f, 160 - 8 + (i - 2) * 24 + step - 2,
                TTITLE_MICE_Y, 2, -1, i > 2 ? RS_SPR_HFLIP : 0);
        spr(SPR_SHIP + (st_t % 150 < 8), 160 - 16, TTITLE_SHIP_Y + b, 2, 0, 0);
        spr(SPR_SEED, 160 - 4, TTITLE_SHIP_Y - 14 - (st_t % 60) / 2 + b, 2, 0, 0);
    } else if (state == ST_TITLE) {
        hu_title_sprites(st_t, slot_icon, NULL);   /* the prompt's glyph, the joined players popping in */
        /* the hamster's ship bobs under the logo, two space mice march beside it */
        int b = hu_bob(st_t, 64, READY_BOB);
        spr(SPR_SHIP + (st_t % 150 < 8), 160 - 16, TITLE_SHIP_Y + b, 2, 0, 0);
        int f = (st_t / 30) % 2, step = ((st_t / 30) % 8) - 4;
        step = step < 0 ? -step : step;
        spr(SPR_MOUSE_B + f, 160 - 76 + step, TITLE_SHIP_Y + 2, 2, -1, 0);
        spr(SPR_MOUSE_C + f, 160 + 60 - step, TITLE_SHIP_Y + 2, 2, -1, RS_SPR_HFLIP);
        spr(SPR_SEED, 160 - 4, TITLE_SHIP_Y - 14 - (st_t % 60) / 2 + b, 2, 0, 0);
    } else if (state != ST_OVER) {                 /* the panels show over the moon and the cheese alone */
        draw_world(w, sx, sy);
    }
}

/* ---- save states (main.c) ---- */
/* a state saved at another width brings maps of another size: set them up for this width, redraw the UI */
void draw_state_loaded(void)
{
    /* (and one saved with the other playfield, portrait or landscape: the shelters elsewhere) */
    if (rs_bg_map_w(RS_BG2) == map_w() && maps_tall == draw_tall()) return;
    maps_setup();
    shown_key = -1;
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(line_col); S(shown_key); S(shown_best); S(maps_tall);
    RS_STATE_RASTER(raster);
}
#undef S
