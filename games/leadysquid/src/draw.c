/*
 * Leady Squid: video (VRAM, layers, raster effects, sprites) and the cosmetic
 * effects (bubbles, ink puffs, lead weights). Nothing here changes the game.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
 */
#include "ls.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include "house_tall.h"             /* a phone held upright: the title and the panels in the columns shown */
#include <stdio.h>
#include <string.h>

/* BG1 tiles (relative to 0): the house UI kit (games/common/src/house_ui.c: the font, the font on the panel
 * colour, the 2x-glyph cache, the panel frame; HU_BG_TILES), then the logo at LOGO_TILE.. */

/* water gradient per theme: surface colour, deep colour */
static const uint32_t water[THEMES][2] = {
    {0x48a8c8, 0x104878}, {0x40b0c0, 0x125070}, {0x3480a8, 0x0e3460}, {0x2c5c96, 0x0c1e4c}};

static int16_t back_dx[RS_SCREEN_H];
static rs_color line_col[RS_SCREEN_H];
static int cur_rgb[2][3];                 /* the gradient shown (lerps toward the theme's) */
static int depth;                         /* 0..DEPTH_MAX */
static int drawn_index, cleared_col;
static uint32_t drawn_seed;               /* the course whose bodies are on BG2 */
static rs_rng fx_rng;
static int logo_on;
static int last_dark = -1;                /* the darkness the reef palettes were last written for */
/* the caps' OBJ palettes: two slots (6 + theme parity) hold the two themes a course window can show (a theme lasts
 * THEME_BAND obstacles = 720 px, more than the window): OBJ 4 and 5 are players 3 and 4 (the house rule) */
static int cap_theme[2] = {-1, -1};

int depth_level(void) { return depth; }

/* ---- raster: the water gradient, the additive rays band, the pause dim ----------------------------- */
/* A tall screen (rs_screen_zoom: a phone held upright; DESIGN.md "Tall screen"): the lines past the stage's 240.
 * Above the water's surface (stage line 0): the sky (the score in it), pale at the horizon, bluer toward the
 * screen's top, a bright crest of foam on the surface. Below the seabed: its sand carried on (BG2's last two rows,
 * 16 px, repeated, each copy shifted sideways so that no pebble or starfish lines up), darker with depth. The other layers are hidden there
 * (their maps would wrap); line 0 puts the layers, BG2's scroll and the sand's palette back. */
static int seabed_x;                      /* BG2's scroll x this frame (draw_frame; state_audit.txt) */
static int scenery_w(void);
#define SAND_REPEAT_Y 224
static void raster_out(int line)
{
    int top = rs_screen_top(), bot = rs_screen_bottom();
    if (line == top || line == RS_SCREEN_H) {
        rs_bg_enable(RS_BG1, 0);
        rs_bg_enable(RS_BG3, 0);
        rs_bg_enable(RS_BG4, 0);
        rs_bg_enable(RS_BG2, line == RS_SCREEN_H);
    }
    if (line < 0) {
        int k = top < 0 ? -line * 256 / -top : 0, d = (line & 4) ? 4 : 0;
        rs_color c = hu_lerp_color(RS_RGB8(214 + d, 238, 246), RS_RGB8(110 + d, 178, 226), k);
        if (line >= -2) c = RS_RGB8(236, 250, 255);         /* the foam on the surface */
        else if (line >= -4) c = RS_RGB8(178, 226, 238);
        rs_pal_set(0, c);
    } else {
        int d = line - RS_SCREEN_H, n = bot - RS_SCREEN_H, k = n > 0 ? d * 120 / n : 0;
        rs_bg_scroll(RS_BG2, (seabed_x + d / 16 * 184) & (scenery_w() * 8 - 1), SAND_REPEAT_Y + d % 16 - line);
        if ((d & 3) == 0)
            for (int i = 1; i < 16; i++) rs_pal_set(RS_PAL_BG(PAL_SEABED) + i, hu_scale_color(ls_seabed_pal[i], 236 - k));
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
        for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
        rs_bg_scroll(RS_BG2, seabed_x, 0);
        rs_pal_load(RS_PAL_BG(PAL_SEABED), ls_seabed_pal, 16);
    }
    rs_pal_set(0, line_col[line]);
    if (line == 0) rs_math(RS_MATH_ADD, RS_MATH_BG4, 0);
    else if (line == RAYS_END) rs_math(RS_MATH_OFF, 0, 0);
}

static void gradient_update(int theme, int target_depth)
{
    for (int k = 0; k < 2; k++) {
        uint32_t c = water[theme][k];
        int tgt[3] = {(int)(c >> 16) & 255, (int)(c >> 8) & 255, (int)c & 255};
        for (int i = 0; i < 3; i++) {
            int d = tgt[i] * 16 - cur_rgb[k][i];
            cur_rgb[k][i] += d > 0 ? (d + 31) / 32 : d / 32;
        }
    }
    if (depth < target_depth) depth++;
    if (depth > target_depth) depth = target_depth;
    int dark = 256 - depth * 96 / DEPTH_MAX;       /* up to 37% darker */
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int k = y < PLAY_H ? y * 256 / PLAY_H : 256;
        int rgb[3];
        for (int i = 0; i < 3; i++) {
            int v = (cur_rgb[0][i] * (256 - k) + cur_rgb[1][i] * k) / 256 / 16;
            rgb[i] = v * dark / 256;
        }
        /* a 2-line dither step every 8 lines keeps the bands soft on RGB555 */
        line_col[y] = RS_RGB8(rgb[0] + ((y & 4) ? 4 : 0), rgb[1] + ((y & 4) ? 2 : 0), rgb[2] + ((y & 4) ? 4 : 0));
    }
    /* the rays, the reef and the mid-ground darken with the water */
    if (dark != last_dark) {
        last_dark = dark;
        for (int i = 1; i < 16; i++) {
            rs_color c = ls_back_pal[i], m = ls_mid_pal[i];
            rs_pal_set(RS_PAL_BG(PAL_BACK) + i, RS_RGB((c & 31) * dark / 256, ((c >> 5) & 31) * dark / 256,
                                                       ((c >> 10) & 31) * dark / 256));
            int dm = 256 - (256 - dark) * 3 / 4;
            rs_pal_set(RS_PAL_BG(PAL_MID) + i, RS_RGB((m & 31) * dm / 256, ((m >> 5) & 31) * dm / 256,
                                                      ((m >> 10) & 31) * dm / 256));
        }
    }
}

/* ---- set-up ---------------------------------------------------------------------------------------- */
/* Widescreen (DESIGN.md "Widescreen"): the scenery maps are 64 tiles (512 px) wide at 320 columns, 128 (1024 px) on a
 * wider screen, which would otherwise see the 512-px panoramas repeat (and the course needs more than 512 px). The
 * second half is the first one mirrored (the same tiles h-flipped: no VRAM, seamless at both joins), so the screen
 * never shows a stretch twice; the additive rays (BG4 rows above RAYS_END) are copied, not mirrored (they slant).
 * The UI layer is 128 tiles wide beyond 512 columns (the corner tags of 3-4 players wrap round it). */
static int redraw_pause;                  /* transient: a state of another width was loaded (state_audit.txt) */
static int scenery_w(void) { return rs_screen_width() > RS_SCREEN_W ? 128 : 64; }
static int ui_w(void) { return rs_screen_width() > 512 ? 128 : 64; }

static void put_panorama(int layer, const uint16_t *map, int mw, int mh, int y0, int copy_rows)
{
    int w = scenery_w();
    for (int y = 0; y < mh; y++)
        for (int x = 0; x < mw; x++) {
            uint16_t e = map[y * mw + x];
            rs_bg_put(layer, x, y0 + y, e);
            if (w > mw) {
                if (y < copy_rows) rs_bg_put(layer, mw + x, y0 + y, e);
                else rs_bg_put(layer, 2 * mw - 1 - x, y0 + y, (uint16_t)(e ^ RS_MAP_HFLIP));
            }
        }
}

static void maps_setup(void)
{
    rs_bg_setup(RS_BG1, ui_w(), 32, VR_BG1);
    rs_bg_setup(RS_BG2, scenery_w(), 32, VR_BG2);
    rs_bg_setup(RS_BG3, scenery_w(), 32, VR_BG3);
    rs_bg_setup(RS_BG4, scenery_w(), 32, VR_BG4);
    put_panorama(RS_BG2, ls_seabed_map, SEABED_W, SEABED_H, PLAY_H / 8, 0);
    put_panorama(RS_BG3, ls_mid_map, MID_W, MID_H, MID_Y / 8, 0);
    put_panorama(RS_BG4, ls_back_map, BACK_W, BACK_H, 0, RAYS_END / 8);
}

void draw_init(void)
{
    /* the house UI kit on BG1 (palette 0); the digits, the A glyph and the medals are sprites of the game's
     * sheet (drawn by games/common/tools/house_style.py), so no kit sprites */
    hu_config hc = hu_defaults();
    hc.logo_tile = LOGO_TILE;
    hc.logo_pal = PAL_LOGO;
    hc.obj_tile = -1;
    hc.box_glyphs = " SCOREBESTNWMDALPYIG12!:-";    /* the panel glyphs to preload (English); others load on demand */
    hu_init(&hc);
    rs_tiles_load(LOGO_TILE, ls_logo_tiles, ls_logo_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_SEABED), ls_seabed_pal, 16);
    for (int t = 0; t < THEMES; t++) rs_pal_load(RS_PAL_BG(PAL_THEME0 + t), ls_theme_bg_pal[t], 16);
    rs_pal_load(RS_PAL_BG(PAL_MID), ls_mid_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_BACK), ls_back_pal, 16);
    rs_pal_load(RS_PAL_OBJ(0), ls_obj_pals, 128);

    rs_tiles_load(VR_BG2, ls_bg2_tiles, ls_bg2_tile_count);
    rs_tiles_load(VR_BG3, ls_bg3_tiles, ls_bg3_tile_count);
    rs_tiles_load(VR_BG4, ls_bg4_tiles, ls_bg4_tile_count);
    /* widescreen: a third of the extra width behind the squids (tuning.h WIDE_LEFT): the stage origin; 0 at 320 */
    rs_screen_origin(WIDE_LEFT(rs_screen_width() - RS_SCREEN_W));
    /* a tall screen (portrait, ls.h ZOOM_H): the stage low, TALL_ABOVE lines of sky above the surface */
    rs_screen_origin_v(clampi(rs_screen_height() - RS_SCREEN_H, 0, TALL_ABOVE));
    maps_setup();
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, ls_obj_tiles, ls_obj_tile_count);
    rs_bg_line_scroll(RS_BG4, back_dx, NULL);
    rs_raster(raster, NULL);
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < 3; i++) cur_rgb[k][i] = (int)((water[0][k] >> (16 - 8 * i)) & 255) * 16;
    rs_rng_seed(&fx_rng, 0xb0bb1e5u);
    gradient_update(0, 0);
    drawn_index = -1;
    cleared_col = 0;
    cap_theme[0] = cap_theme[1] = -1;
}

/* ---- the course on BG2 ------------------------------------------------------------------------------- */
static void put_column_part(const obstacle *o, int r0, int r1)
{
    int variant = (o->index * 7 + 3) % 2;
    const uint16_t *m = ls_body_map[o->theme] + variant * 6;
    int c0 = o->x / 8, cm = rs_bg_map_w(RS_BG2) - 1;
    for (int r = r0; r < r1; r++)
        for (int k = 0; k < 3; k++) rs_bg_put(RS_BG2, (c0 + k) & cm, r, m[(r & 1) * 3 + k]);
}

void draw_reset_course(const world *w)
{
    for (int y = 0; y < PLAY_H / 8; y++)
        for (int x = 0; x < rs_bg_map_w(RS_BG2); x++) rs_bg_put(RS_BG2, x, y, 0);
    drawn_index = w->first_index - 1;
    drawn_seed = w->seed;
    int c = (world_scroll_px(w) + rs_screen_left()) / 8;   /* the screen's first column (widescreen: left of x 0) */
    cleared_col = c > 1 ? c - 1 : 0;
}

/* Widescreen: the first obstacle is at the same distance from the squid at every width (world.c), so a wide screen
 * already shows the first gates when the run starts. Before it starts (the title, or waiting for the first flap) the
 * obstacles on the screen are not shown: the title stays clear, and the first flap re-rolls the course anyway (a new
 * seed from the start frame). At 4:3 none is on the screen then (the first one is just off it): nothing changes. */
static int obstacle_hidden(const world *w, const obstacle *o)
{
    return !w->started && o->x < world_scroll_px(w) + rs_screen_width();
}

static void course_tiles(const world *w)
{
    /* The course is drawn once per obstacle, but the first flap re-rolls it (a new seed from the
     * start frame, world_step): the tiles of the obstacle already drawn during "get ready" (just
     * off-screen) belonged to the old course, while its caps (sprites) follow the new gap. Redraw
     * whenever the course changes. */
    if (w->seed != drawn_seed) draw_reset_course(w);
    int sx = world_scroll_px(w), cm = rs_bg_map_w(RS_BG2) - 1;
    /* clear the columns that left the screen on the left (widescreen: its real left edge, rs_screen_left()) */
    for (; cleared_col < (sx + rs_screen_left()) / 8 - 1; cleared_col++)
        for (int y = 0; y < PLAY_H / 8; y++) rs_bg_put(RS_BG2, cleared_col & cm, y, 0);
    for (int i = 0; i < w->nob; i++) {
        const obstacle *o = &w->ob[i];
        if (o->index <= drawn_index || o->x > sx + rs_screen_width() + 64) continue;
        if (obstacle_hidden(w, o)) break;            /* in x order: drawn from the first flap on */
        put_column_part(o, 0, o->gap_top / 8);                              /* top: surface .. gap */
        put_column_part(o, (o->gap_top + GAP + 7) / 8, PLAY_H / 8);          /* bottom: gap .. seabed */
        drawn_index = o->index;
    }
}

/* ---- cosmetic effects -------------------------------------------------------------------------------------- */
typedef struct fx_obj { int kind, t, life; int32_t x, y, vx, vy; int frame; } fx_obj;
enum { FX_NONE, FX_BUBBLE, FX_INK, FX_WEIGHT };
#define FX_MAX 48
static fx_obj fx[FX_MAX];

static fx_obj *fx_new(int kind)
{
    for (int i = 0; i < FX_MAX; i++)
        if (fx[i].kind == FX_NONE) {
            memset(&fx[i], 0, sizeof fx[i]);
            fx[i].kind = kind;
            return &fx[i];
        }
    return NULL;
}

static void bubble_at(int x, int y, int size)
{
    fx_obj *b = fx_new(FX_BUBBLE);
    if (!b) return;
    b->x = x << 8;
    b->y = y << 8;
    b->vy = -(80 + rs_rng_range(&fx_rng, 90) + size * 30);
    b->frame = size;
    b->life = 400;
    b->t = rs_rng_range(&fx_rng, 64);
}

void fx_flap(int x, int y)
{
    fx_obj *k = fx_new(FX_INK);
    if (k) {
        k->x = (x - 18) << 8;
        k->y = (y - 2) << 8;
        k->vy = -20;
        k->life = INK_LIFE;
    }
    bubble_at(x - 14, y - 2, 0);
    bubble_at(x - 16, y + 3, 1);
}

void fx_land(int x, int y)
{
    for (int i = 0; i < 3; i++) {
        fx_obj *wgt = fx_new(FX_WEIGHT);
        if (!wgt) return;
        wgt->x = (x - 4 + i * 4) << 8;
        wgt->y = (y - 4) << 8;
        wgt->vx = (i - 1) * 110;
        wgt->vy = -(260 + i * 50);
        wgt->life = 100000;
    }
    for (int i = 0; i < 4; i++) bubble_at(x - 8 + i * 5, y - 6, i & 1);
}

void fx_update(const world *w)
{
    int dx = world_running(w) ? 256 : 0;         /* the world's drift, 8.8 px per frame */
    for (int i = 0; i < FX_MAX; i++) {
        fx_obj *f = &fx[i];
        if (f->kind == FX_NONE) continue;
        f->t++;
        switch (f->kind) {
        case FX_BUBBLE:
            f->y += f->vy;
            f->x -= dx / 2;
            if ((f->y >> 8) < 2 || f->t > f->life) f->kind = FX_NONE;
            break;
        case FX_INK:
            f->x -= dx;
            f->y += f->vy;
            if (f->t >= f->life) f->kind = FX_NONE;
            break;
        case FX_WEIGHT:
            f->x += f->vx - dx;
            f->vy += 40;
            f->y += f->vy;
            if ((f->y >> 8) >= SEABED_Y + 2) {
                f->y = (SEABED_Y + 2) << 8;
                f->vy = f->vy > 200 ? -f->vy / 3 : 0;
                f->vx = f->vx * 2 / 3;
                f->frame = 0;
            } else {
                f->frame = 1;
            }
            if ((f->x >> 8) < rs_screen_left() - 16) f->kind = FX_NONE;
            break;
        }
    }
    /* ambient bubbles rising from the seabed, now and then */
    if (rs_rng_range(&fx_rng, 100) < 4) bubble_at(rs_screen_left() + 8 + rs_rng_range(&fx_rng, rs_screen_width() - 16), SEABED_Y + 4, rs_rng_range(&fx_rng, 3));
}

/* ---- sprites --------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal_override)
{
    const ls_sprite_def *d = &ls_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= rs_screen_top() - (int)d->h ||
        y >= rs_screen_bottom())
        return;
    rs_spr(x, y, d->tile, d->w, d->h, pal_override >= 0 ? pal_override : d->pal, prio, 0);
}

static void number(int n, int cx, int y, int prio)
{
    char s[12];
    snprintf(s, sizeof s, "%d", n);
    int len = (int)strlen(s), x = cx - len * 6;
    for (int i = 0; i < len; i++) spr(SPR_DIGITS + (s[i] - '0'), x + i * 12 - 2, y, prio, -1);
}

static int squid_sprite(const squid *s, int t)
{
    switch (s->state) {
    case SQ_READY: return SPR_SQUID_IDLE + (t / 32) % 2;
    case SQ_HIT: return SPR_SQUID_HIT;
    case SQ_SINK: return SPR_SQUID_SINK + (t / 8) % 2;
    case SQ_REST: return SPR_SQUID_REST;
    default:
        if (s->flap_t < 9) return SPR_SQUID_FLAP + s->flap_t / 3;
        return SPR_SQUID_TILT + squid_tilt_frame(s->tilt);
    }
}

static int p1_drawn_x;      /* tests (draw_view_log): the screen x of P1's sprite centre, as drawn */
static void draw_squids(const world *w)
{
    for (int p = 0; p < w->players; p++) {
        const squid *s = &w->sq[p];
        if (s->state == SQ_OFF) continue;
        int cy = (int)((s->y + Q16_ONE / 2) >> 16);
        int n = rs_oam_next();
        spr(squid_sprite(s, w->t), s->x - 16, cy - 16, 2, p ? hu_player_pal(p) : -1);   /* P2-P4: OBJ 1, 4, 5 */
        if (p == 0) p1_drawn_x = rs_oam_next() > n ? rs_oam(n)->x + 16 + rs_screen_origin_x() : -999;
    }
}

static void draw_caps(const world *w)
{
    int sx = world_scroll_px(w);
    for (int i = 0; i < w->nob; i++) {
        int x = w->ob[i].x - sx;
        if (x <= rs_screen_left() - OBST_W || x >= rs_screen_right() || obstacle_hidden(w, &w->ob[i])) continue;   /* on screen only */
        int th = w->ob[i].theme, k = th & 1;
        if (cap_theme[k] != th) {
            rs_pal_load(RS_PAL_OBJ(OBJ_CAPS + k), ls_cap_pals[th], 16);
            cap_theme[k] = th;
        }
    }
    for (int i = 0; i < w->nob; i++) {
        const obstacle *o = &w->ob[i];
        int x = o->x - sx;
        if (x <= rs_screen_left() - OBST_W || x >= rs_screen_right() || obstacle_hidden(w, o)) continue;
        spr(SPR_CAP_KELP + o->theme * 2, x, o->gap_top - CAP_H, 2, OBJ_CAPS + (o->theme & 1));
        spr(SPR_CAP_KELP + o->theme * 2 + 1, x, o->gap_top + GAP, 2, OBJ_CAPS + (o->theme & 1));
    }
}

static void draw_fx(int prio_front)
{
    for (int i = 0; i < FX_MAX; i++) {
        const fx_obj *f = &fx[i];
        int x = f->x >> 8, y = f->y >> 8;
        if (f->kind == FX_BUBBLE && !prio_front) {
            int wob = ((f->t / 8) % 4 == 1) - ((f->t / 8) % 4 == 3);
            spr(SPR_BUBBLE + f->frame, x + wob - 4, y - 4, 1, -1);
        } else if (f->kind == FX_INK && prio_front) {
            spr(SPR_INK + f->t * 4 / (f->life + 1), x - 8, y - 8, 2, -1);
        } else if (f->kind == FX_WEIGHT && prio_front) {
            spr(SPR_WEIGHT + f->frame, x - 4, y - 7, 2, -1);
        }
    }
}

/* ---- per frame ---------------------------------------------------------------------------------------------- */
static int shown_state = -1, shown_best = -1;
static int shown_cols = -1;              /* the columns shown when the UI was drawn (a tall screen: ht_ncols, else 0) */
static ht_title_out title_at;            /* where the narrow title put its glyph and slots (drawn every title frame) */

/* the race's ranking (2-4 players): the score, then who lasted longer */
static void standing(const world *w, hu_standing *s)
{
    int key[MAX_PLAYERS], val[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) {
        val[p] = w->sq[p].score;
        int out = w->out_t[p];                   /* the frame it was hit; still in: the best */
        key[p] = val[p] * 65536 + (out <= 0 ? 0xffff : out < 0xffff ? out : 0xfffe);
    }
    hu_rank(s, w->players, key, val);
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    /* a phone held upright (ht_narrow, DESIGN.md "Tall screen"): the same in the columns shown (house_tall.h) */
    int narrow = ht_narrow(), cols = narrow ? ht_ncols() : 0;
    if (state != shown_state || (state == DS_OVER && st_t == RETRY_LOCK) || best != shown_best || cols != shown_cols) {
        hu_clear();
        logo_on = 0;
        if (state == DS_TITLE) {
            logo_on = 1;
            if (narrow)     /* LEADY over SQUID (the logo art's tile columns 0..15, 16..31) on ht_logo's rows 2..7, 8..13 */
                for (int k = 0; k < 2; k++)
                    for (int y = 1; y < 7; y++)
                        for (int x = 0; x < LOGO_W / 2; x++)
                            rs_bg_put(RS_BG1, ht_center_w(LOGO_W / 2) + 1 - k + x, 2 + 6 * k + (y - 1), ls_logo_map[y * LOGO_W + k * LOGO_W / 2 + x]);
            else
                for (int y = 0; y < LOGO_H; y++)
                    for (int x = 0; x < LOGO_W; x++) rs_bg_put(RS_BG1, 4 + x, 2 + y, ls_logo_map[y * LOGO_W + x]);
            rs_pal_load(RS_PAL_BG(PAL_LOGO), ls_logo_pal, 16);
        } else {
            rs_pal_load(RS_PAL_BG(PAL_LOGO), ls_theme_bg_pal[PAL_LOGO - PAL_THEME0], 16);
        }
        if ((state == DS_PLAY || state == DS_DEAD) && w->players >= 3 && rs_screen_height() == RS_SCREEN_H)
            hu_score_tags(w->players, 0);           /* (a tall screen: the icons, draw_frame) */
        if (state == DS_OVER) {
            if (w->players == 1) {
                /* the title on its own banner, as wide as the score panel below it: nothing shows through */
                if (narrow) {
                    ht_banner(4, HTR(GAME_OVER));
                    ht_gameover_panel(new_best, NULL, HTR(BACK_MENU));
                } else {
                    hu_banner(4, HTR(GAME_OVER));
                    hu_gameover_panel(1, new_best, w->sq[0].score, 0);
                }
            } else if (narrow) {
                ht_results_panel(rs, NULL);
            } else {
                hu_results_panel(rs, NULL);           /* P3 WINS! and the ranking */
            }
        }
        shown_state = state;
        shown_best = best;
        shown_cols = cols;
    }
    /* the title, every frame: the prompt (blinking), the player slots, BEST, the join line, the credits */
    if (state == DS_TITLE) {
        if (narrow) {
            ht_title tc = {TR(VERB), MAX_PLAYERS, 15, 0};
            ht_title_draw(&tc, st_t, best, &title_at);
        } else {
            hu_title_draw(st_t, best);
        }
    }
    if (state == DS_OVER) {
        if (w->players == 1) {
            if (narrow) ht_retry_line_at(ht_gameover_retry_row(NULL), st_t, RETRY_LOCK, HTR(BACK_MENU));
            else hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_MENU));
        } else {
            if (narrow) ht_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_MENU));
            else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_MENU));
        }
    }
}

static void number_at(int n, int x, int y, int align, int prio)
{
    char s[12];
    snprintf(s, sizeof s, "%d", n);
    int w = (int)strlen(s) * 12;
    number(n, (align < 0 ? x + w / 2 : align > 0 ? x - w / 2 : x) + hu_ui_x(), y, prio);
}

/* the title's player slots: a small squid in each joined player's colours, popping in with a sparkle */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    int age = hu_player_age(p);
    cx += hu_ui_x();                              /* the kit's UI coordinates -> stage */
    if (age < 24) spr(SPR_SPARKLE + (age / 6) % 2, cx + 6, cy - 14, 3, -1);
    spr(SPR_SQUID_ICON, cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), 2, p ? hu_player_pal(p) : -1);
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    int sx = world_scroll_px(w);
    int score = w->sq[0].score;
    for (int p = 1; p < w->players; p++)
        if (w->sq[p].score > score) score = w->sq[p].score;
    hu_standing rs;
    standing(w, &rs);
    gradient_update(world_theme_at(w), score < DEPTH_MAX ? score : DEPTH_MAX);
    course_tiles(w);
    int pm = scenery_w() * 8 - 1;                   /* the panoramas' period - 1: 511, or 1023 on a wide screen */
    seabed_x = sx & pm;
    rs_bg_scroll(RS_BG2, seabed_x, 0);
    rs_bg_scroll(RS_BG3, (sx / 2) & pm, 0);
    rs_bg_scroll(RS_BG4, 0, 0);
    int t = (int)rs_frame_count();
    for (int y = 0; y < RS_SCREEN_H; y++) {
        if (y < RAYS_END) {
            int sway = ((y + t / 3) & 63) < 32 ? ((y + t / 3) & 31) : 31 - ((y + t / 3) & 31);
            back_dx[y] = (int16_t)(((sx / 8) + (t / 12) + sway / 12 + (y < 6 ? (t / 4 + y * 3) % 5 : 0)) & pm);
        } else {
            back_dx[y] = (int16_t)((sx / 4) & pm);
        }
    }
    /* a tall screen (ls.h ZOOM_H, DESIGN.md "Tall screen"): the zoomed view, always: the title and the panels too, laid
     * out in the columns shown (screen_text, house_tall.h); 1-2 squids swimming: the columns shown from VIEW_BEHIND px
     * behind the hindmost squid; 3-4 squids swimming show every column (the race needs them) */
    int swim = state != DS_TITLE && state != DS_OVER;
    int zoom = rs_screen_height() > RS_SCREEN_H && (w->players <= 2 || !swim);
    rs_screen_fill(zoom);
    int vx0 = rs_screen_visible_x0() - rs_screen_origin_x(), vw = rs_screen_visible_x1() - rs_screen_visible_x0();
    if (zoom && swim) {
        vx0 = squid_start_x(w->players, w->players - 1) - VIEW_BEHIND;
        rs_screen_focus_x(vx0 + vw / 2 + rs_screen_origin_x());
    } else {
        rs_screen_focus_x(-1);
    }
    screen_text(w, state, st_t, best, new_best, &rs);
    /* the game-over panel slides up */
    int slide = state == DS_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    int ui_dx = zoom && swim ? ht_mid_x() - RS_SCREEN_W / 2 : 0;   /* (zoomed, swimming: PAUSED on the columns shown) */
    rs_bg_scroll(RS_BG1, -hu_ui_x() - ui_dx, -slide);   /* the UI centred on the screen (widescreen) */
    if (redraw_pause && paused) hu_pause(0, 13);  /* the UI layer was set up again (a state of another width) */
    redraw_pause = 0;
    ht_pause_dx(paused, 13, ui_dx);              /* (house_tall.h: PAUSED centred on the columns shown) */

    rs_oam_clear();
    /* front to back: UI, squids, ink and weights, caps, bubbles */
    if (state == DS_PLAY || state == DS_DEAD) {
        /* 1 player: centred; 2: x 80 and 240; 3-4: the corners (the kit's chips, our digits) */
        for (int p = 0; p < w->players; p++) {
            hu_chip ch = hu_score_chip(w->players, p, 0);
            if (rs_screen_height() > RS_SCREEN_H) {  /* a tall screen: in the sky, at the top of the columns shown */
                int cx = vx0 + vw * (2 * p + 1) / (2 * w->players), y = rs_screen_top() + 14;
                if (w->players > 1) spr(SPR_SQUID_ICON, cx - 24, y, 3, p ? hu_player_pal(p) : -1);   /* whose */
                number(w->sq[p].score, w->players > 1 ? cx + 6 : cx, y, 3);
                continue;
            }
            number_at(w->sq[p].score, ch.x, ch.y, ch.align, 3);
        }
    }
    if (state == DS_OVER && ht_narrow()) {          /* a phone held upright: the panels in the columns shown */
        int oy = slide, ux = hu_ui_x(), cx, y;
        if (w->players == 1) {
            ht_gameover_pos(0, &cx, &y);
            number(w->sq[0].score, cx + ux, y + oy, 3);
            ht_gameover_pos(1, &cx, &y);
            number(best, cx + ux, y + oy, 3);
            ht_gameover_pos(2, &cx, &y);
            int m = medal_of(w->sq[0].score);
            if (m) {
                spr(SPR_MEDAL + m - 1, cx - 12 + ux, y + oy, 3, -1);
                if ((st_t / 20) % 3 == 0) spr(SPR_SPARKLE + (st_t / 10) % 2, cx + 14 + ux, y + oy, 3, -1);
            } else {
                hu_box_text((cx - 4) / 8, y / 8 + 1, "-");
            }
        } else {
            int ix, vx, mx;
            ht_results_cols(&ix, &vx, &mx);
            for (int i = 0; i < rs.n; i++) {
                int ry = hu_results_row(&rs, i) * 8 + oy, m = hu_results_medal(&rs, i);
                number(rs.value[rs.order[i]], vx + ux, ry - 4, 3);
                if (m) {
                    spr(SPR_MEDAL + m - 1, mx + ux, ry - 8, 3, -1);
                    if (rs.rank[i] == 0 && (st_t / 20) % 3 == 0) spr(SPR_SPARKLE + (st_t / 10) % 2, mx + 18 + ux, ry - 8, 3, -1);
                }
                spr(SPR_SQUID_ICON, ix - 8 + ux, ry - 4, 3, rs.order[i] ? hu_player_pal(rs.order[i]) : -1);
            }
        }
    } else if (state == DS_OVER) {
        int oy = slide, ux = hu_ui_x();
        if (w->players == 1) {
            number(w->sq[0].score, 25 * 8 + ux, 11 * 8 - 4 + oy, 3);
            number(best, 25 * 8 + ux, 14 * 8 - 4 + oy, 3);
            int m = medal_of(w->sq[0].score);
            if (m) {
                spr(SPR_MEDAL + m - 1, 22 * 8 - 4 + ux, 16 * 8 - 4 + oy, 3, -1);
                if ((st_t / 20) % 3 == 0) spr(SPR_SPARKLE + (st_t / 10) % 2, 22 * 8 + 12 + ux, 16 * 8 - 4 + oy, 3, -1);
            } else {
                hu_box_text(22, 17, "-");
            }
        } else {
            /* the ranking: the scores, the shells (1st gold, 2nd silver, 3rd bronze), the squids */
            for (int i = 0; i < rs.n; i++) {
                int y = hu_results_row(&rs, i) * 8 + oy, m = hu_results_medal(&rs, i), cx, cy;
                number(rs.value[rs.order[i]], 24 * 8 + ux, y - 4, 3);
                if (m) {
                    spr(SPR_MEDAL + m - 1, 27 * 8 + ux, y - 8, 3, -1);
                    if (rs.rank[i] == 0 && (st_t / 20) % 3 == 0) spr(SPR_SPARKLE + (st_t / 10) % 2, 27 * 8 + 18 + ux, y - 8, 3, -1);
                }
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_SQUID_ICON, cx - 8 + ux, cy - 8 + oy, 3, rs.order[i] ? hu_player_pal(rs.order[i]) : -1);
            }
        }
    }
    if (state == DS_TITLE && ht_narrow()) {
        ht_title tc = {TR(VERB), MAX_PLAYERS, 15, 0};
        spr(SPR_HINT + ((t / 30) % 2), title_at.glyph_x + hu_ui_x(), title_at.glyph_y, 3, -1);
        ht_title_sprites(&tc, &title_at, st_t, slot_icon, NULL);
    } else if (state == DS_TITLE) {
        int gx, gy;
        if (hu_title_glyph(&gx, &gy) == HU_BTN_A) spr(SPR_HINT + ((t / 30) % 2), gx + hu_ui_x(), gy, 3, -1);
        hu_title_sprites(st_t, slot_icon, NULL);
    }
    draw_squids(w);
    draw_fx(1);
    draw_caps(w);
    draw_fx(0);
}

/* Tests (--opt view=1, main.c at exit): the view of the last frame, "view: width=W left=L p1_x=X next_x=N": L the columns
 * shown behind the squids (the stage origin), X the screen x of P1's sprite centre as drawn, N the screen x of P1's next
 * obstacle (tests/wide_test.sh: X and N - X against 320) */
void draw_view_log(const world *w)
{
    const obstacle *o = world_next_obstacle(w, 0);
    rs_log("view: width=%d left=%d p1_x=%d next_x=%d", rs_screen_width(), rs_screen_origin_x(), p1_drawn_x,
           o ? o->x - world_scroll_px(w) + rs_screen_origin_x() : -999);
}

/* ---- save states (main.c) ----
 * A state saved at another width brings the maps of that width (64 or 128 tiles): set them up for this one (the
 * scenery again, the course redrawn from the obstacles, the UI redrawn on the next frame). Same width: nothing. */
void draw_state_loaded(const world *w)
{
    if (rs_bg_map_w(RS_BG2) == scenery_w() && rs_bg_map_w(RS_BG1) == ui_w()) return;
    maps_setup();
    draw_reset_course(w);
    shown_state = -1;                           /* screen_text clears the UI layer and draws it again */
    redraw_pause = 1;                           /* ... and PAUSED, if it shows */
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(back_dx); S(line_col); S(cur_rgb); S(depth); S(drawn_index); S(cleared_col); S(drawn_seed);
    S(fx_rng); S(logo_on); S(last_dark); S(fx); S(shown_state); S(shown_best); S(cap_theme); S(shown_cols); S(title_at);
    RS_STATE_RASTER(raster);
    hu_state();                 /* the house UI kit's objects (house_ui.*) */
}
#undef S
