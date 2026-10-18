/*
 * Unicorn Hop: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1); BG2 the near clouds (parallax 1/2, drifting); BG3 the start scene far
 *   below (the hills and the castle, parallax 1/2) and later the rainbow of the celebrations (the same VRAM, reloaded;
 *   translucent: colour math); BG4 the far wisps and the stars (parallax 1/4); the backdrop a raster sky gradient
 *   that changes with the height: morning pastel, sunset, a starry night with the moon, the aurora, and again.
 *   Sprites: the unicorns (OBJ 0; players 2-4 OBJ 1, 4, 5 = the body recoloured), the clouds (OBJ 2; pink OBJ 6,
 *   grey rain OBJ 7: the same tiles), the kit's digits, glyphs and medals (OBJ 3).
 * Widescreen (docs/spec.md "Widescreen"): the 320-px stage is centred (rs_screen_origin) and a wider screen shows
 * more sky on both sides (the decor maps are wider); the clouds stay in the stage's play band.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/unicornhop/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include "house_tall.h"             /* a phone held upright: the title and the panels in the columns shown */
#include <stdio.h>
#include <string.h>

/* the title logo: one 3-shade ramp per word (light, mid, dark) */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 214, 232), RS_RGB8(246, 150, 196), RS_RGB8(212, 92, 150)},
    {RS_RGB8(214, 230, 255), RS_RGB8(150, 176, 250), RS_RGB8(110, 112, 226)}};

/* the skies, a phase every RAINBOW_EVERY levels (0xRRGGBB): the gradient's top and horizon, the near clouds (light,
 * mid, shade), the far wisps (light, mid), the stars (0..256), the aurora (0..256), the moon (0..256), and how much
 * the play clouds take the night's moonlit tint (0..256) */
typedef struct skydef { int top, hor, n[3], f[2], stars, aurora, moon, dusk; } skydef;
static const skydef SKIES[4] = {
    {0x8ec8f0, 0xffd8e8, {0xf8f6ff, 0xe8e8fa, 0xd6d8f2}, {0xe2eefc, 0xd4e4fa}, 0, 0, 0, 0},          /* morning */
    {0x6a70c4, 0xffb078, {0xffd6c8, 0xf6baba, 0xdea0b0}, {0xf8bebe, 0xe2aaba}, 0, 0, 0, 30},         /* sunset */
    {0x0e1240, 0x2c3c82, {0x4e5c96, 0x404c84, 0x343e72}, {0x3c4a82, 0x303c74}, 256, 0, 256, 80},     /* night */
    {0x0a1a34, 0x1c4c60, {0x467882, 0x386474, 0x2c5468}, {0x32646e, 0x285464}, 220, 256, 256, 70},  /* aurora */
};
#define SCENE_TOP   112     /* the start scene's top line at the camera's start (play), then it sinks at 1/2 */
#define RAINBOW_TOP 52      /* the rainbow's top line when it stands */
#define TINT        0x7882c8    /* the moonlit tint of the play clouds */
#define NEAR_K      120         /* /256: how far the near decor clouds go from the sky's colour to their own */
#define FAR_K       100         /* the same for the far wisps */

static rs_color line_col[RS_SCREEN_H], star_a[RS_SCREEN_H], star_b[RS_SCREEN_H];
static int shown_state = -1, shown_best = -1, shown_rainbow, bg3_content = -1, bg3_on, bg3_top;
static int shown_cols = -1;             /* the columns shown when the UI was drawn (narrow: ht_ncols, playing 1; else 0) */
static ht_title_out title_at;           /* where the narrow title put its glyph and slots (drawn every title frame) */

static int map_w(void);

/* A tall screen (rs_screen_zoom: a phone held upright; DESIGN.md "Tall screen"): the lines past the stage's 240. The
 * sky carried on: above, its top colour deepening toward the screen's top; below, the horizon's colour; the stars and
 * the far and near decor layers go on (their maps wrap, as they always do when the camera climbs). The start scene's
 * land carries on below the stage (its last row repeated); the rainbow stays on the stage; the UI layer is hidden
 * there. The play clouds of every level the screen shows are drawn there too (draw_frame). Line 0 puts the UI layer
 * and BG3's scroll back. */
static void raster_out(int line)
{
    if (line == rs_screen_top() || line == RS_SCREEN_H) rs_bg_enable(RS_BG1, 0);
    if (line < 0) {
        int top = rs_screen_top(), k = -line * 64 / -top;
        rs_pal_set(0, hu_scale_color((line & 4) ? line_col[4] : line_col[0], 256 - k));
        rs_pal_set(RS_PAL_BG(PAL_FAR) + IDX_STAR_A, star_a[0]);
        rs_pal_set(RS_PAL_BG(PAL_FAR) + IDX_STAR_B, star_b[0]);
        rs_bg_enable(RS_BG3, 0);
    } else {
        rs_pal_set(0, (line & 4) ? line_col[236] : line_col[232]);
        rs_pal_set(RS_PAL_BG(PAL_FAR) + IDX_STAR_A, star_a[RS_SCREEN_H - 1]);
        rs_pal_set(RS_PAL_BG(PAL_FAR) + IDX_STAR_B, star_b[RS_SCREEN_H - 1]);
        int on = bg3_content == 0 && bg3_on && line >= bg3_top, last = bg3_top + SCENE_H * 8 - 8;
        rs_bg_enable(RS_BG3, on);
        if (on && line >= last)                     /* the land below the scene: its last row repeated */
            rs_bg_scroll(RS_BG3, (-hu_shake_x()) & (map_w() * 8 - 1), SCENE_H * 8 - 8 + ((line - last) & 7) - line);
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
        rs_bg_enable(RS_BG1, 1);
        rs_bg_scroll(RS_BG3, (-hu_shake_x()) & (map_w() * 8 - 1), (-bg3_top - hu_shake_y()) & 255);
    }
    rs_pal_set(0, line_col[line]);
    rs_pal_set(RS_PAL_BG(PAL_FAR) + IDX_STAR_A, star_a[line]);
    rs_pal_set(RS_PAL_BG(PAL_FAR) + IDX_STAR_B, star_b[line]);
    rs_bg_enable(RS_BG3, bg3_on && line >= bg3_top);
}

/* ---- colours ------------------------------------------------------------------------------------------------------ */
static int mix(int a, int b, int k)        /* 0xRRGGBB, k 0..256 */
{
    int r = 0;
    for (int s = 0; s <= 16; s += 8) {
        int x = (a >> s) & 255, y = (b >> s) & 255;
        r |= ((x * (256 - k) + y * k) >> 8) << s;
    }
    return r;
}

static rs_color rgb(int c) { return RS_RGB8((c >> 16) & 255, (c >> 8) & 255, c & 255); }

static int isin(int a)                     /* a sine, a = 0..1023 a turn, -256..256 */
{
    a &= 1023;
    int x = a & 511, v = x * (512 - x) / 256;
    return a < 512 ? v : -v;
}

static int tri(int t, int period)          /* 0..256..0 */
{
    t %= period;
    return t < period / 2 ? t * 512 / period : (period - t) * 512 / period;
}

/* the sky for an altitude: two phases mixed in the last 5 levels of each */
static skydef sky_at(int alt)
{
    int lv = alt < 0 ? 0 : (int)((int64_t)alt * 256 / LEVEL_H), span = RAINBOW_EVERY * 256;
    int ph = (lv / span) & 3, f = lv % span, from = span - 5 * 256, k = f > from ? (f - from) / 5 : 0;
    const skydef *a = &SKIES[ph], *b = &SKIES[(ph + 1) & 3];
    skydef s;
    s.top = mix(a->top, b->top, k);
    s.hor = mix(a->hor, b->hor, k);
    for (int i = 0; i < 3; i++) s.n[i] = mix(a->n[i], b->n[i], k);
    for (int i = 0; i < 2; i++) s.f[i] = mix(a->f[i], b->f[i], k);
    s.stars = a->stars + (b->stars - a->stars) * k / 256;
    s.aurora = a->aurora + (b->aurora - a->aurora) * k / 256;
    s.moon = a->moon + (b->moon - a->moon) * k / 256;
    s.dusk = a->dusk + (b->dusk - a->dusk) * k / 256;
    return s;
}

static void sky_lines(const skydef *s, int t)
{
    int ka = s->stars * (150 + tri(t, 90) * 106 / 256) / 256, kb = s->stars * (150 + tri(t + 45, 90) * 106 / 256) / 256;
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int c = mix(s->top, s->hor, y * 256 / (RS_SCREEN_H - 1));
        if (s->aurora && y < 180) {                 /* the aurora: green and violet ribbons, waving */
            int fade = y < 150 ? 256 : (180 - y) * 256 / 30;
            int g = isin(y * 7 + t * 3) + isin(y * 3 - t * 2 + 200), v = isin(y * 5 - t * 2 + 600);
            if (g > 120) c = mix(c, 0x58f0a8, (g - 120) * s->aurora / 512 * fade / 256);
            if (v > 150) c = mix(c, 0xb070f0, (v - 150) * s->aurora / 400 * fade / 256);
        }
        int d = (y & 4) ? 1 : 0;                    /* house rule: a 2-line dither step every 8 lines */
        int r = ((c >> 16) & 255) + 4 * d, gg = ((c >> 8) & 255) + 2 * d, b = (c & 255) + 4 * d;
        line_col[y] = RS_RGB8(r > 255 ? 255 : r, gg > 255 ? 255 : gg, b > 255 ? 255 : b);
        star_a[y] = rgb(mix(c, 0xfffbe0, ka));
        star_b[y] = rgb(mix(c, 0xe6f0ff, kb));
    }
}

static void sky_palettes(const skydef *s)
{
    /* the decor melts into the sky (low contrast, no outline): never mistaken for a cloud to land on */
    int mid = mix(s->top, s->hor, 140);
    rs_pal_set(RS_PAL_BG(PAL_NEAR) + IDX_NEAR_LIGHT, rgb(mix(mid, s->n[0], NEAR_K)));
    rs_pal_set(RS_PAL_BG(PAL_NEAR) + IDX_NEAR_MID, rgb(mix(mid, s->n[1], NEAR_K)));
    rs_pal_set(RS_PAL_BG(PAL_NEAR) + IDX_NEAR_SHADE, rgb(mix(mid, s->n[2], NEAR_K)));
    rs_pal_set(RS_PAL_BG(PAL_FAR) + IDX_FAR_LIGHT, rgb(mix(mid, s->f[0], FAR_K)));
    rs_pal_set(RS_PAL_BG(PAL_FAR) + IDX_FAR_MID, rgb(mix(mid, s->f[1], FAR_K)));
    static const int idx[5] = CLOUD_IDX;           /* the play clouds: moonlit at night */
    for (int p = 0; p < 3; p++) {
        int slot = p == 0 ? 2 : p == 1 ? 6 : 7;
        for (int i = 0; i < 5; i++) {
            int e = gm_obj_pals[slot * 16 + idx[i]];
            int c = ((e & 31) << 19) | (((e >> 5) & 31) << 11) | (((e >> 10) & 31) << 3);
            rs_pal_set(RS_PAL_OBJ(slot) + idx[i], rgb(mix(c, TINT, s->dusk)));
        }
    }
}

/* ---- the maps ------------------------------------------------------------------------------------------------------
 * Widescreen: the maps are 64 tiles (512 px) wide, 128 on a screen wider than 512 columns; the second half mirrors
 * the first (the same tiles h-flipped). The map size is saved in states: draw_state_loaded() sets them up again. */
static int map_w(void) { return rs_screen_width() > 512 ? 128 : 64; }

static void put_wide(int layer, int x, int y, uint16_t e, int mw)
{
    rs_bg_put(layer, x, y, e);
    if (mw > 64) rs_bg_put(layer, 127 - x, y, (uint16_t)(e ^ RS_MAP_HFLIP));
}

/* BG3: 0 the start scene, 1 the rainbow (its tiles loaded over the scene's) */
static void bg3_set(int content)
{
    int mw = map_w();
    rs_bg_setup(RS_BG3, mw, 32, VR_BG3);
    if (content == 0) {
        rs_tiles_load(VR_BG3, gm_scene_tiles, gm_scene_tile_count);
        for (int y = 0; y < SCENE_H; y++)       /* repeated, not mirrored: one castle (the hills join at 512) */
            for (int x = 0; x < mw; x++) rs_bg_put(RS_BG3, x, y, gm_scene_map[y * SCENE_W + x % SCENE_W]);
    } else {
        rs_tiles_load(VR_BG3, gm_rainbow_tiles, gm_rainbow_tile_count);
        for (int y = 0; y < RAINBOW_H; y++)
            for (int x = 0; x < RAINBOW_W; x++) rs_bg_put(RS_BG3, x, y, gm_rainbow_map[y * RAINBOW_W + x]);
    }
    bg3_content = content;
}

static void maps_setup(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG2, mw, 32, VR_BG2);
    rs_bg_setup(RS_BG4, mw, 32, VR_BG4);
    for (int y = 0; y < NEAR_H; y++)
        for (int x = 0; x < NEAR_W; x++) put_wide(RS_BG2, x, y, gm_near_map[y * NEAR_W + x], mw);
    for (int y = 0; y < FAR_H; y++)
        for (int x = 0; x < mw; x++) rs_bg_put(RS_BG4, x, y, gm_far_map[y * FAR_W + x % FAR_W]);
    bg3_set(bg3_content < 0 ? 0 : bg3_content);
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more sky on both sides */
    /* a tall screen (portrait, game.h ZOOM_H): the stage low, the levels ahead above it */
    int below = rs_screen_height() - RS_SCREEN_H < TALL_BELOW ? 0 : TALL_BELOW;
    rs_screen_origin_v(rs_screen_height() - RS_SCREEN_H - below);
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;         /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    rs_tiles_load(VR_BG2, gm_near_tiles, gm_near_tile_count);
    rs_tiles_load(VR_BG4, gm_far_tiles, gm_far_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_NEAR), gm_near_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_FAR), gm_far_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_SCENE), gm_scene_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_RAINBOW), gm_rainbow_pal, 16);
    bg3_content = -1;
    maps_setup();
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* the unicorn, P2, the clouds (the kit loaded OBJ 3) */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 64);     /* P3, P4, the pink and the grey clouds */
    skydef s = sky_at(TITLE_CAM);
    sky_lines(&s, 0);
    sky_palettes(&s);
    rs_raster(raster, NULL);
}

/* ---- sprites -------------------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal, int flags)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= rs_screen_top() - (int)d->h ||
        y >= rs_screen_bottom())              /* (a tall screen: its lines past the stage too) */
        return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

/* player p's sprite palette: the house rule (P1 OBJ 0, P2 OBJ 1, P3 OBJ 4, P4 OBJ 5: house_ui.h hu_player_pal) */
static int player_pal(int p) { return p ? hu_player_pal(p) : 0; }

static int camd;                        /* this frame's camera altitude (the title looks lower) */
static int sy_of(int alt) { return STAND_Y - (alt - camd); }

/* a cloud of width w centred on cx with its top at screen line top: the end pieces and the middle ones */
static void draw_cloud(int cx, int top, int w, int look, int pal, int sx, int sy)
{
    int x0 = cx - w / 2 + sx, y = top - 8 + sy, mids = (w - 32) / 16;
    spr(SPR_CLOUD_L, x0, y, 2, pal, 0);
    for (int i = 0; i < mids; i++) spr(SPR_CLOUD_M + (look + i * 5 + i / 2) % 3, x0 + 16 + 16 * i, y, 2, pal, 0);
    spr(SPR_CLOUD_R, x0 + w - 16, y, 2, pal, 0);
}

static void draw_puffs(int cx, int top, int w, int frame, int sx, int sy)
{
    for (int x = cx - w / 2 + 8; x < cx + w / 2; x += 20) spr(SPR_PUFF + frame, x - 8 + sx, top - 6 + sy, 2, 2, 0);
}

static void draw_level(const world *w, const cloud *c, int sx, int sy)
{
    int L = c->index, top = sy_of(L * LEVEL_H), cx = c->x >> 16;
    int pal = c->type == CL_PINK ? 6 : c->type == CL_RAIN ? 7 : 2;
    /* the gap above: the thunder cloud, the star */
    int gy = sy_of(L * LEVEL_H + GAP_Y);
    if (c->thunder) {
        int crackle = ((w->t + L * 13) / 6) % 9 == 0;
        for (int p = 0; p < w->players; p++)
            if (w->h[p].state == HS_ZAP && w->h[p].level == L) crackle = 1;
        spr(SPR_THUNDER + crackle, (c->tx >> 16) - 16 + sx, gy - 9 + sy, 2, 2, 0);
    }
    if (c->star && !c->star_taken) {
        static const int spin[4] = {0, 1, 2, 1};
        int f = spin[((w->t + L * 7) / 7) & 3], bob = hu_bob(w->t + L * 20, 64, 2);
        spr(SPR_STAR + f, c->sx - 8 + sx, gy - 8 + bob + sy, 2, 0, (w->t / 28) & 1 && f == 2 ? RS_SPR_HFLIP : 0);
    } else if (c->star && w->t - c->star_t < 24) {   /* taken: a burst of sparkles */
        int a = w->t - c->star_t;
        static const int dir[6][2] = {{3, 0}, {-3, 0}, {2, -2}, {-2, -2}, {2, 2}, {-2, 2}};
        for (int i = 0; i < 6; i++)
            spr(SPR_SPARKLE + (i % 3) * 2 + (a > 12), c->sx - 4 + dir[i][0] * a / 2 + sx, gy - 4 + dir[i][1] * a / 2 + sy,
                2, 0, 0);
    }
    /* the cloud */
    if (c->gone_t >= 0) {
        if (c->gone_t < 12) draw_puffs(cx, top, c->w, c->gone_t / 4, sx, sy);
        else if (c->gone_t >= RAIN_REFORM - 12) draw_puffs(cx, top, c->w, (RAIN_REFORM - 1 - c->gone_t) / 4, sx, sy);
        return;
    }
    if (c->rain_t >= 0) {                     /* the rain: drops under it; it blinks before melting */
        for (int k = 0; k < 4; k++) {
            int ph = (c->rain_t * 2 + k * 17) % 36;
            if (ph < 28) spr(SPR_DROP, cx - c->w / 2 + 10 + k * (c->w - 20) / 3 - 4 + sx, top + 12 + ph + sy, 2, 2, 0);
        }
        if (c->rain_t > RAIN_MELT - 30 && (c->rain_t / 3) % 2) return;
    }
    draw_cloud(cx, top, c->w, c->look, pal, sx, sy);
}

/* the unicorn's frame and flips */
static void hero_frame(const hero *h, int *id, int *flags)
{
    *flags = h->face < 0 ? RS_SPR_HFLIP : 0;
    switch (h->state) {
    case HS_READY:
    case HS_STAND:
        *id = h->land_t < 5 ? SPR_UNI_SQUASH : SPR_UNI_IDLE + (h->t / 24) % 2;
        break;
    case HS_AIR:
        *id = h->air_t < 3 ? SPR_UNI_SQUASH : h->vy > 0 ? SPR_UNI_STRETCH : SPR_UNI_FALL;
        break;
    case HS_ZAP:
        *id = SPR_UNI_HURT;
        break;
    default: {                                /* the tumble: a quarter turn every 5 frames */
        int q = (h->t / 5) & 3;
        *id = SPR_UNI_HURT + (q & 1);
        *flags = q >= 2 ? RS_SPR_HFLIP | RS_SPR_VFLIP : 0;
        if (h->face < 0) *flags ^= RS_SPR_HFLIP;
        break;
    }
    }
}

static void draw_hero(const world *w, int p, int sx, int sy)
{
    const hero *h = &w->h[p];
    if (h->state == HS_DOWN || h->state == HS_OFF) return;
    int x = (h->x >> 16) + sx, feet = sy_of(hero_alt(h)) + sy, id, fl;
    hero_frame(h, &id, &fl);
    if (h->state == HS_ZAP) {                 /* the zap: the bolt, the unicorn blinking */
        spr(SPR_BOLT, x - 8, feet - 30 - 22, 2, 2, (h->t / 2) & 1 ? RS_SPR_HFLIP : 0);
        if ((h->t / 2) & 1) return;
    }
    if (h->state == HS_FALL) {                /* the sad sparkle */
        int ty = feet - 34 - h->t / 3;
        if (h->t < 40) spr(SPR_TEAR + (h->t / 8) % 2, x + 6 * h->face - 4, ty, 2, 0, 0);
    }
    spr(id, x - 16, feet - 30, 2, player_pal(p), fl);
    if (h->state == HS_AIR) {                 /* sparkles trailing the jump, where it was */
        int32_t v0 = h->big ? BIG_JUMP_VY : JUMP_VY;
        int base = h->level;
        for (int i = 0; i < 6; i++) {
            int ti = i * 3, age = h->air_t - ti;
            if (age < 0 || age >= 14) continue;
            int32_t dy = ti * v0 - GRAVITY * ti * (ti + 1) / 2;
            int alt = base * LEVEL_H + (dy >> 16);
            spr(SPR_SPARKLE + (i % 3) * 2 + (age > 7), x - 4 + (i * 37 % 13) - 6, sy_of(alt) + sy - 6 + age / 3, 2, 0, 0);
        }
    }
    if (h->land_t < 9 && (h->state == HS_STAND || h->state == HS_READY)) {   /* landing puffs */
        int f = h->land_t / 3;
        spr(SPR_PUFF + f, x - 8 - 12 - f * 2, feet - 10, 2, 2, 0);
        spr(SPR_PUFF + f, x - 8 + 12 + f * 2, feet - 10, 2, 2, RS_SPR_HFLIP);
    }
}

/* the title's player slots: each joined player's icon, in its colours (hu_title_sprites calls it) */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr(SPR_UNI_ICON, cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), 2, player_pal(p), 0);
}

/* 2-4 players: ranked by the height climbed (the same height, the same place); the results show the heights */
static void standing(const world *w, hu_standing *s)
{
    int val[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) val[p] = w->h[p].height;
    hu_rank(s, w->players, val, val);
}

/* the game-over note: the stars caught (NULL: none) */
static const char *stars_note(const world *w, char *s, int n)
{
    if (!w->h[0].stars) return NULL;
    rs_strf(s, n, TRN(STARS_N, w->h[0].stars), w->h[0].stars);
    return s;
}

/* the narrow title (house_tall.h, its standard rows): the slots' row; the unicorn on its cloud in the sky above the
 * logo (clear of the title's rows) */
#define HT_TITLE_TOP 15
#define HT_TITLE_FEET (-14)     /* the title unicorn's feet (its cloud's top) */

/* the BG screens; PLAY and DEAD are the same screen */
static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    int screen = state == ST_DEAD ? ST_PLAY : state;
    int rainbow = screen == ST_PLAY && w->rainbow_t >= 0 && w->rainbow_t < RAINBOW_SHOW / 2;
    /* a phone held upright (ht_narrow, DESIGN.md "Tall screen"): the same in the columns shown (house_tall.h); playing,
     * the UI layer is scrolled to the columns shown (draw_frame): laid out on the stage's middle */
    int narrow = ht_narrow(), cols = narrow ? (screen == ST_PLAY ? 1 : ht_ncols()) : 0;
    char note[64];
    if (screen != shown_state || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best ||
        rainbow != shown_rainbow || cols != shown_cols) {
        hu_clear();
        if (state == ST_TITLE) {
            if (narrow) ht_logo("UNICORN HOP", title_ramps, 2, 2, 0);      /* UNICORN over HOP */
            else hu_logo("UNICORN HOP", title_ramps, 2, 2, 0);
        }
        if (screen == ST_PLAY && w->players >= 3 && rs_screen_height() == RS_SCREEN_H)
            hu_score_tags(w->players, 0);       /* P1..P4 by the corner chips (a tall screen: icons, draw_frame) */
        if (rainbow) {
            if (narrow && hu_width(TR(RAINBOW), 1) > ht_ncols())                /* 1x when 2x is wider than shown */
                hu_text(hu_center(TR(RAINBOW), 0), 6, TR(RAINBOW));
            else
                hu_big(hu_center(TR(RAINBOW), 1), 5, TR(RAINBOW), HU_BIG_FREE);
        }
        if (state == ST_OVER) {
            if (w->players == 1) {
                const char *n = stars_note(w, note, sizeof note);              /* the stars caught */
                if (narrow) {
                    ht_banner(4, HTR(GAME_OVER));
                    ht_gameover_panel(new_best, n, HTR(BACK_TITLE));
                } else {
                    hu_banner(4, HTR(GAME_OVER));
                    hu_gameover_panel(1, new_best, w->h[0].score, 0);
                    if (n) hu_gameover_note(n);
                }
            } else if (narrow) {
                ht_results_panel(rs, NULL);
            } else {
                hu_results_panel(rs, NULL);                                     /* P2 WINS! and the ranking */
            }
        }
        shown_state = screen;
        shown_best = best;
        shown_rainbow = rainbow;
        shown_cols = cols;
    }
    if (state == ST_TITLE) {
        if (narrow) {
            ht_title tc = {TR(VERB), MAX_PLAYERS, HT_TITLE_TOP, 0};
            ht_title_draw(&tc, st_t, best, &title_at);
        } else {
            hu_title_draw(st_t, best);
        }
    }
    if (state == ST_OVER) {
        if (w->players == 1) {
            if (narrow)
                ht_retry_line_at(ht_gameover_retry_row(stars_note(w, note, sizeof note)), st_t, RETRY_LOCK,
                                 HTR(BACK_TITLE));
            else
                hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        } else {
            if (narrow) ht_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
            else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
        }
    }
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    static const int medals[4] = MEDAL_SCORES;
    int sx = hu_shake_x(), sy = hu_shake_y();
    camd = cam_px(w) + (state == ST_TITLE ? TITLE_CAM : 0);
    hu_standing rs;
    standing(w, &rs);

    /* the sky, the decor layers */
    skydef sk = sky_at(camd);
    sky_lines(&sk, w->t);
    sky_palettes(&sk);
    int pm = map_w() * 8 - 1;
    rs_bg_scroll(RS_BG2, (w->t / 5 - sx) & pm, (-camd / 2 - sy) & 255);
    rs_bg_scroll(RS_BG4, (w->t / 12 - sx) & pm, (-camd / 4 - sy) & 255);
    int want = state != ST_TITLE && w->rainbow_t >= 0;
    if (want != bg3_content) bg3_set(want);
    if (want) {                                  /* the rainbow rises, stands, then sinks */
        int t = w->rainbow_t, slide = t < 40 ? hu_slide_in(t, 40, 190) : t > RAINBOW_SHOW - 40 ?
                                      hu_ease_in(t - (RAINBOW_SHOW - 40), 40, 190) : 0;
        bg3_top = RAINBOW_TOP + slide;
        bg3_on = bg3_top < RS_SCREEN_H;
        rs_bg_scroll(RS_BG3, (-sx) & pm, (-bg3_top - sy) & 255);
        rs_math(RS_MATH_ADD | RS_MATH_HALF, RS_MATH_BG3, 0);
    } else {
        bg3_top = SCENE_TOP + camd / 2;
        bg3_on = bg3_top < rs_screen_bottom();     /* (a tall screen: the scene may show below the stage) */
        rs_bg_scroll(RS_BG3, (-sx) & pm, (-bg3_top - sy) & 255);
        rs_math(RS_MATH_OFF, 0, 0);
    }

    /* a tall screen (game.h ZOOM_H, DESIGN.md "Tall screen"): the zoomed view, always: the title and the panels too,
     * laid out in the columns shown (screen_text, house_tall.h); one unicorn climbing: the columns shown centred on it
     * (it rides its cloud, it jumps straight up: the view follows it smoothly), its score at the screen's top; 2-4
     * players climbing show every column (the unicorns spread over the band) */
    int climb = state == ST_PLAY || state == ST_DEAD;
    int zoom = rs_screen_height() > RS_SCREEN_H && (w->players == 1 || !climb);
    rs_screen_fill(zoom);
    rs_screen_focus_x(zoom && climb ? (w->h[0].x >> 16) + rs_screen_origin_x() : -1);
    int vx0 = rs_screen_visible_x0() - rs_screen_origin_x(), vx1 = rs_screen_visible_x1() - rs_screen_origin_x();

    screen_text(w, state, st_t, best, new_best, &rs);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;     /* the panel slides up, ease-out */
    /* (zoomed, climbing: the UI layer's middle, PAUSED and RAINBOW!, on the columns shown) */
    int ui_dx = zoom && climb && ht_narrow() ? ht_mid_x() - RS_SCREEN_W / 2 : 0;
    rs_bg_scroll(RS_BG1, -ui_dx, -slide);
    ht_pause_dx(paused, 13, ui_dx);             /* (house_tall.h: PAUSED centred on the columns shown) */

    rs_oam_clear();
    /* front to back: the UI, the unicorns, the clouds, the moon */
    if (zoom && climb) {
        hu_number(w->h[0].score, (vx0 + vx1) / 2 - hu_ui_x(), rs_screen_top() + 10, 3);
    } else if ((state == ST_PLAY || state == ST_DEAD) && rs_screen_height() > RS_SCREEN_H) {
        /* a tall screen, 2-4 players: the heights in a row at the screen's top, each after its player's icon */
        for (int p = 0; p < w->players; p++) {
            int cx = (2 * p + 1) * RS_SCREEN_W / (2 * w->players), y = rs_screen_top() + 10;
            spr(SPR_UNI_ICON, cx - 22, y, 3, player_pal(p), 0);
            hu_number_at(w->h[p].height, cx - 2 - hu_ui_x(), y, -1, 3);
        }
    } else if (state == ST_PLAY || state == ST_DEAD) {
        int v[MAX_PLAYERS];
        for (int p = 0; p < w->players; p++) v[p] = w->players == 1 ? w->h[p].score : w->h[p].height;
        hu_score_chips(w->players, v, 0);       /* 1: the score; 2-4: the heights (the race up) */
    }
    if (state == ST_OVER && ht_narrow()) {        /* a phone held upright: the panels in the columns shown */
        if (w->players == 1) {
            ht_gameover_sprites(w->h[0].score, best, hu_medal_of(w->h[0].score, medals), st_t, slide);
        } else {
            ht_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                ht_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_UNI_ICON, cx - 8 + hu_ui_x(), cy - 8 + slide, 3, player_pal(rs.order[i]), 0);
            }
        }
    } else if (state == ST_OVER) {
        if (w->players == 1) {
            hu_gameover_sprites(1, w->h[0].score, 0, best, hu_medal_of(w->h[0].score, medals), st_t, slide);
        } else {
            hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_UNI_ICON, cx - 8, cy - 8 + slide, 3, player_pal(rs.order[i]), 0);
            }
        }
    }
    if (state == ST_TITLE) {
        int narrow = ht_narrow();
        if (narrow) {                              /* the narrow title's glyph and slots (screen_text) */
            ht_title tc = {TR(VERB), MAX_PLAYERS, HT_TITLE_TOP, 0};
            ht_title_sprites(&tc, &title_at, st_t, slot_icon, NULL);
        } else {
            hu_title_sprites(st_t, slot_icon, NULL);   /* the prompt's glyph, the joined players popping in */
        }
        /* the unicorn bobs on its little cloud under the logo */
        int b = hu_bob(st_t, 64, READY_BOB), feet = (narrow ? HT_TITLE_FEET : 124) + b;
        spr(SPR_UNI_IDLE + (st_t / 24) % 2, CENTER_X - 16, feet - 30, 2, 0, 0);
        if (st_t % 50 < 14) spr(SPR_SPARKLE + ((st_t / 50) % 3) * 2 + (st_t % 50 > 7), CENTER_X + 10, feet - 34, 2, 0, 0);
        draw_cloud(CENTER_X, feet, 48, 1, 2, 0, 0);
    } else {
        for (int p = 0; p < w->players; p++) draw_hero(w, p, sx, sy);
        /* the levels the screen shows (a tall screen: more of them, above and below the stage) */
        int lo = (camd - (rs_screen_bottom() - STAND_Y) - 40) / LEVEL_H;
        int hi = (camd + STAND_Y - rs_screen_top() + 40) / LEVEL_H;
        for (int L = lo < 0 ? 0 : lo; L <= hi; L++) {
            const cloud *c = world_cloud(w, L);
            if (c) draw_level(w, c, sx, sy);
        }
    }
    if (sk.moon > 0) {                             /* the moon comes down at night */
        int my = 22 - (256 - sk.moon) * 70 / 256;
        spr(SPR_MOON, vx1 - 72 < 248 ? vx1 - 72 : 248, my + hu_bob(w->t, 240, 2), 1, 0, 0);
    }
}

/* ---- save states (main.c) ---- */
/* a state saved at another width brings maps of another size: set them up for this width, redraw the UI */
void draw_state_loaded(void)
{
    if (rs_bg_map_w(RS_BG2) == map_w()) return;
    maps_setup();
    shown_state = -1;
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(line_col); S(star_a); S(star_b); S(shown_state); S(shown_best); S(shown_rainbow); S(bg3_content); S(bg3_on);
    S(bg3_top); S(camd); S(shown_cols); S(title_at);
    RS_STATE_RASTER(raster);
}
#undef S
