/*
 * Leady Squid: video (VRAM, layers, raster effects, sprites) and the cosmetic
 * effects (bubbles, ink puffs, lead weights). Nothing here changes the game.
 * All rights reserved, 8BCraft.
 */
#include "ls.h"
#include "assets.h"
#include <stdio.h>
#include <string.h>

extern const uint8_t rs_font_5x7[96][8];

/* BG1 tiles (relative to 0): small font 0..95, font on the panel colour 96..191,
 * the 2x-glyph cache 192..287, the panel frame 288..296, the logo LOGO_TILE.. */
#define T_BOXFONT 96
#define T_BIG     192
#define BIG_SLOTS 24
#define T_PANEL   288

/* palette 0 (text and panels) */
static const rs_color text_pal[16] = {
    0, RS_HEX(0xfff6dc), RS_HEX(0x101838), RS_HEX(0x2a4476), RS_HEX(0x78aad2), RS_HEX(0x0c142c),
    RS_HEX(0xfad25a), RS_HEX(0x365488)};

/* water gradient per theme: surface colour, deep colour */
static const uint32_t water[THEMES][2] = {
    {0x48a8c8, 0x104878}, {0x40b0c0, 0x125070}, {0x3480a8, 0x0e3460}, {0x2c5c96, 0x0c1e4c}};

static int16_t back_dx[RS_SCREEN_H];
static rs_color line_col[RS_SCREEN_H];
static int cur_rgb[2][3];                 /* the gradient shown (lerps toward the theme's) */
static int depth;                         /* 0..DEPTH_MAX */
static int paused_now;
static int drawn_index, cleared_col;
static uint32_t drawn_seed;               /* the course whose bodies are on BG2 */
static int big_chars[BIG_SLOTS], big_used;
static rs_rng fx_rng;
static int logo_on;
static int last_dark = -1;                /* the darkness the reef palettes were last written for */

int depth_level(void) { return depth; }

/* ---- text ------------------------------------------------------------------------------------ */
static void glyph_tile(int c, int bg, int dst)
{
    uint8_t t[64];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int on = (rs_font_5x7[c][y] & (0x80 >> x)) != 0;
            int sh = x > 0 && y > 0 && (rs_font_5x7[c][y - 1] & (0x80 >> (x - 1)));
            t[y * 8 + x] = (uint8_t)(on ? 1 : sh ? 2 : bg);
        }
    rs_tiles_load8(dst, t, 1);
}

/* 2x glyphs with a full outline, uploaded on demand into a small cache. Two styles: BIG_FREE, cream
 * on a clear background (over the scene); BIG_BANNER, gold with an outline and a drop shadow on the
 * panel colour (the game-over banner, which hides what is behind it). */
enum { BIG_FREE, BIG_BANNER };

static int big_slot(char ch, int style)
{
    int c = ch - 32;
    if (c < 0 || c >= 96) c = 0;
    int key = c | style << 8;
    for (int i = 0; i < big_used; i++)
        if (big_chars[i] == key) return i;
    if (big_used >= BIG_SLOTS) return 0;
    int s = big_used++;
    big_chars[s] = key;
    uint8_t px[16][16];
    memset(px, 0, sizeof px);
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 6; x++)
            if (rs_font_5x7[c][y] & (0x80 >> x))
                for (int k = 0; k < 4; k++) px[1 + y * 2 + (k >> 1)][1 + x * 2 + (k & 1)] = 1;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            if (px[y][x]) continue;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int yy = y + dy, xx = x + dx;
                    if (yy >= 0 && yy < 16 && xx >= 0 && xx < 16 && px[yy][xx] == 1) px[y][x] = 2;
                }
        }
    if (style == BIG_BANNER) {
        /* the drop shadow: one pixel down-right of the outline */
        for (int y = 15; y > 0; y--)
            for (int x = 15; x > 0; x--)
                if (!px[y][x] && px[y - 1][x - 1] == 2) px[y][x] = 3;
    }
    /* palette 0: 1 cream, 2 outline, 3 panel fill, 5 shadow, 6 gold */
    static const uint8_t col[2][4] = {{0, 1, 2, 0}, {3, 6, 2, 5}};
    for (int q = 0; q < 4; q++) {
        uint8_t t[64];
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) t[y * 8 + x] = col[style][px[(q >> 1) * 8 + y][(q & 1) * 8 + x]];
        rs_tiles_load8(T_BIG + s * 4 + q, t, 1);
    }
    return s;
}

static void text(int x, int y, const char *s)
{
    rs_text_setup(RS_BG1, 0, 0, 1);
    rs_text(x, y, s);
}

static void box_text(int x, int y, const char *s)
{
    rs_text_setup(RS_BG1, T_BOXFONT, 0, 1);
    rs_text(x, y, s);
}

static void big_text_style(int x, int y, const char *s, int style)
{
    for (; *s; s++, x += 2) {
        if (*s == ' ') continue;
        int t = T_BIG + big_slot(*s, style) * 4;
        for (int q = 0; q < 4; q++) rs_bg_put(RS_BG1, x + (q & 1), y + (q >> 1), RS_MAP(t + q, 0, 1, 0, 0));
    }
}

static void big_text(int x, int y, const char *s) { big_text_style(x, y, s, BIG_FREE); }

static int center(const char *s, int big) { return (40 - (int)strlen(s) * (big ? 2 : 1)) / 2; }

static void clear_text(void)
{
    rs_bg_fill(RS_BG1, 0);
}

/* the panel frame: 3x3 tiles (corners, edges, fill) in palette 0 */
static void panel_tiles(void)
{
    for (int k = 0; k < 9; k++) {
        uint8_t t[64];
        int kx = k % 3, ky = k / 3;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                int ex = kx == 0 ? x : kx == 2 ? 7 - x : 8;   /* distance to the outer edge */
                int ey = ky == 0 ? y : ky == 2 ? 7 - y : 8;
                int v = 3;
                if (kx != 1 && ky != 1 && ex + ey < 3) v = 0;         /* rounded corner */
                else if (ex == 0 || ey == 0 || (kx != 1 && ky != 1 && ex + ey == 3)) v = 5;
                else if (ex == 1 || ey == 1 || (kx != 1 && ky != 1 && ex + ey == 4)) v = (ky == 2 || kx == 2) ? 5 : 4;
                else if (ey == 2 && ky == 0) v = 7;
                t[y * 8 + x] = (uint8_t)v;
            }
        rs_tiles_load8(T_PANEL + k, t, 1);
    }
}

static void panel(int x0, int y0, int w, int h)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int kx = x == 0 ? 0 : x == w - 1 ? 2 : 1, ky = y == 0 ? 0 : y == h - 1 ? 2 : 1;
            rs_bg_put(RS_BG1, x0 + x, y0 + y, RS_MAP(T_PANEL + ky * 3 + kx, 0, 1, 0, 0));
        }
}

/* ---- raster: the water gradient, the additive rays band, the pause dim ----------------------------- */
static void raster(int line, void *user)
{
    (void)user;
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
void draw_init(void)
{
    rs_text_load(0, 0);
    /* the font on the panel colour: only the glyphs the panel uses (VRAM) */
    for (const char *p = " SCOREBESTNWMDALPYIG12!:-"; *p; p++) glyph_tile(*p - 32, 3, T_BOXFONT + *p - 32);
    panel_tiles();
    rs_tiles_load(LOGO_TILE, ls_logo_tiles, ls_logo_tile_count);
    rs_pal_load(RS_PAL_BG(0), text_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_SEABED), ls_seabed_pal, 16);
    for (int t = 0; t < THEMES; t++) rs_pal_load(RS_PAL_BG(PAL_THEME0 + t), ls_theme_bg_pal[t], 16);
    rs_pal_load(RS_PAL_BG(PAL_MID), ls_mid_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_BACK), ls_back_pal, 16);
    rs_pal_load(RS_PAL_OBJ(0), ls_obj_pals, 128);

    rs_bg_setup(RS_BG1, 64, 32, VR_BG1);
    rs_bg_setup(RS_BG2, 64, 32, VR_BG2);
    rs_bg_setup(RS_BG3, 64, 32, VR_BG3);
    rs_bg_setup(RS_BG4, 64, 32, VR_BG4);
    rs_tiles_load(VR_BG2, ls_bg2_tiles, ls_bg2_tile_count);
    rs_tiles_load(VR_BG3, ls_bg3_tiles, ls_bg3_tile_count);
    rs_tiles_load(VR_BG4, ls_bg4_tiles, ls_bg4_tile_count);
    for (int y = 0; y < SEABED_H; y++)
        for (int x = 0; x < SEABED_W; x++) rs_bg_put(RS_BG2, x, PLAY_H / 8 + y, ls_seabed_map[y * SEABED_W + x]);
    for (int y = 0; y < MID_H; y++)
        for (int x = 0; x < MID_W; x++) rs_bg_put(RS_BG3, x, MID_Y / 8 + y, ls_mid_map[y * MID_W + x]);
    for (int y = 0; y < BACK_H; y++)
        for (int x = 0; x < BACK_W; x++) rs_bg_put(RS_BG4, x, y, ls_back_map[y * BACK_W + x]);
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
}

/* ---- the course on BG2 ------------------------------------------------------------------------------- */
static void put_column_part(const obstacle *o, int r0, int r1)
{
    int variant = (o->index * 7 + 3) % 2;
    const uint16_t *m = ls_body_map[o->theme] + variant * 6;
    int c0 = o->x / 8;
    for (int r = r0; r < r1; r++)
        for (int k = 0; k < 3; k++) rs_bg_put(RS_BG2, (c0 + k) & 63, r, m[(r & 1) * 3 + k]);
}

void draw_reset_course(const world *w)
{
    for (int y = 0; y < PLAY_H / 8; y++)
        for (int x = 0; x < 64; x++) rs_bg_put(RS_BG2, x, y, 0);
    drawn_index = w->first_index - 1;
    drawn_seed = w->seed;
    cleared_col = world_scroll_px(w) / 8 > 1 ? world_scroll_px(w) / 8 - 1 : 0;
}

static void course_tiles(const world *w)
{
    /* The course is drawn once per obstacle, but the first flap re-rolls it (a new seed from the
     * start frame, world_step): the tiles of the obstacle already drawn during "get ready" (just
     * off-screen) belonged to the old course, while its caps (sprites) follow the new gap. Redraw
     * whenever the course changes. */
    if (w->seed != drawn_seed) draw_reset_course(w);
    int sx = world_scroll_px(w);
    /* clear the columns that left the screen on the left */
    for (; cleared_col < sx / 8 - 1; cleared_col++)
        for (int y = 0; y < PLAY_H / 8; y++) rs_bg_put(RS_BG2, cleared_col & 63, y, 0);
    for (int i = 0; i < w->nob; i++) {
        const obstacle *o = &w->ob[i];
        if (o->index <= drawn_index || o->x > sx + RS_SCREEN_W + 64) continue;
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
            if ((f->x >> 8) < -16) f->kind = FX_NONE;
            break;
        }
    }
    /* ambient bubbles rising from the seabed, now and then */
    if (rs_rng_range(&fx_rng, 100) < 4) bubble_at(8 + rs_rng_range(&fx_rng, 304), SEABED_Y + 4, rs_rng_range(&fx_rng, 3));
}

/* ---- sprites --------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal_override)
{
    const ls_sprite_def *d = &ls_spr[id];
    if (x <= -(int)d->w || x >= RS_SCREEN_W || y <= -(int)d->h || y >= RS_SCREEN_H) return;
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

static void draw_squids(const world *w)
{
    for (int p = 0; p < w->players; p++) {
        const squid *s = &w->sq[p];
        if (s->state == SQ_OFF) continue;
        int cy = (int)((s->y + Q16_ONE / 2) >> 16);
        spr(squid_sprite(s, w->t), s->x - 16, cy - 16, 2, p == 1 ? 1 : -1);
    }
}

static void draw_caps(const world *w)
{
    int sx = world_scroll_px(w);
    for (int i = 0; i < w->nob; i++) {
        const obstacle *o = &w->ob[i];
        int x = o->x - sx;
        if (x <= -OBST_W || x >= RS_SCREEN_W) continue;
        spr(SPR_CAP_KELP + o->theme * 2, x, o->gap_top - CAP_H, 2, -1);
        spr(SPR_CAP_KELP + o->theme * 2 + 1, x, o->gap_top + GAP, 2, -1);
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

static void screen_text(const world *w, int state, int st_t, int best, int new_best)
{
    char s[48];
    if (state != shown_state || (state == DS_OVER && st_t == RETRY_LOCK) || best != shown_best) {
        clear_text();
        big_used = 0;
        logo_on = 0;
        if (state == DS_TITLE) {
            logo_on = 1;
            for (int y = 0; y < LOGO_H; y++)
                for (int x = 0; x < LOGO_W; x++) rs_bg_put(RS_BG1, 4 + x, 2 + y, ls_logo_map[y * LOGO_W + x]);
            rs_pal_load(RS_PAL_BG(PAL_LOGO), ls_logo_pal, 16);
            snprintf(s, sizeof s, "BEST %d", best);
            if (best > 0) text(center(s, 0), 24, s);
            text(center("(C) 2026 8BCRAFT - RETROSTONE VC", 0), 28, "(C) 2026 8BCRAFT - RETROSTONE VC");
        } else {
            rs_pal_load(RS_PAL_BG(PAL_LOGO), ls_theme_bg_pal[PAL_LOGO - PAL_THEME0], 16);
        }
        if (state == DS_READY) big_text(center("GET READY", 1), 6, "GET READY");
        if (state == DS_OVER) {
            /* the title on its own banner, as wide as the score panel below it: nothing shows through */
            panel(10, 4, 20, 4);
            big_text_style(center("GAME OVER", 1), 5, "GAME OVER", BIG_BANNER);
            panel(10, 9, 20, 12);
            if (w->players == 1) {
                box_text(12, 11, "SCORE");
                box_text(12, 14, new_best ? "NEW BEST" : "BEST");
                box_text(12, 17, "MEDAL");
            } else {
                box_text(12, 11, "PLAYER 1");
                box_text(12, 14, "PLAYER 2");
                int a = w->sq[0].score, b = w->sq[1].score;
                const char *win = a > b ? "P1 WINS!" : b > a ? "P2 WINS!" : "DRAW!";
                box_text(20 - (int)strlen(win) / 2, 17, win);
            }
        }
        shown_state = state;
        shown_best = best;
    }
    /* blinking lines */
    int blink = (st_t / 30) % 2 == 0;
    if (state == DS_TITLE || state == DS_READY) {
        const char *h = "PRESS A TO SWIM";
        if (blink) text(center(h, 0) + 1, 21, h);
        else rs_text_clear(0, 21, 40, 1);
        if (w->players == 1 && state == DS_READY) text(center("P2: PRESS A ON PAD 2 TO JOIN", 0), 26, "P2: PRESS A ON PAD 2 TO JOIN");
        if (w->players == 2) text(center("P2 JOINED - RACE!", 0), 26, "P2 JOINED - RACE!  ");
    }
    if (state == DS_OVER && st_t >= RETRY_LOCK) {
        if (blink) box_text(13, 19, "A: SWIM AGAIN");
        else box_text(13, 19, "             ");
    }
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    int sx = world_scroll_px(w);
    int score = w->sq[0].score;
    if (w->players == 2 && w->sq[1].score > score) score = w->sq[1].score;
    gradient_update(world_theme_at(w), score < DEPTH_MAX ? score : DEPTH_MAX);
    course_tiles(w);
    rs_bg_scroll(RS_BG2, sx & 511, 0);
    rs_bg_scroll(RS_BG3, (sx / 2) & 511, 0);
    rs_bg_scroll(RS_BG4, 0, 0);
    int t = (int)rs_frame_count();
    for (int y = 0; y < RS_SCREEN_H; y++) {
        if (y < RAYS_END) {
            int sway = ((y + t / 3) & 63) < 32 ? ((y + t / 3) & 31) : 31 - ((y + t / 3) & 31);
            back_dx[y] = (int16_t)(((sx / 8) + (t / 12) + sway / 12 + (y < 6 ? (t / 4 + y * 3) % 5 : 0)) & 511);
        } else {
            back_dx[y] = (int16_t)((sx / 4) & 511);
        }
    }
    screen_text(w, state, st_t, best, new_best);
    /* the game-over panel slides up */
    int slide = 0;
    if (state == DS_OVER && st_t < 20) slide = (20 - st_t) * (20 - st_t) * 200 / 400;
    rs_bg_scroll(RS_BG1, 0, -slide);
    rs_brightness(paused ? 9 : 15);
    if (paused != paused_now) {
        paused_now = paused;
        if (paused) big_text(center("PAUSED", 1), 13, "PAUSED");
        else rs_text_clear(0, 13, 40, 2);
    }

    rs_oam_clear();
    /* front to back: UI, squids, ink and weights, caps, bubbles */
    if (state == DS_PLAY || state == DS_DEAD) {
        if (w->players == 1) number(w->sq[0].score, RS_SCREEN_W / 2, 10, 3);
        else {
            number(w->sq[0].score, 80, 10, 3);
            number(w->sq[1].score, 240, 10, 3);
        }
    }
    if (state == DS_OVER) {
        int oy = slide;
        if (w->players == 1) {
            number(w->sq[0].score, 25 * 8, 11 * 8 - 4 + oy, 3);
            number(best, 25 * 8, 14 * 8 - 4 + oy, 3);
            int m = medal_of(w->sq[0].score);
            if (m) {
                spr(SPR_MEDAL + m - 1, 22 * 8 - 4, 16 * 8 - 4 + oy, 3, -1);
                if ((st_t / 20) % 3 == 0) spr(SPR_SPARKLE + (st_t / 10) % 2, 22 * 8 + 12, 16 * 8 - 4 + oy, 3, -1);
            } else {
                box_text(22, 17, "-");
            }
        } else {
            number(w->sq[0].score, 25 * 8, 11 * 8 - 4 + oy, 3);
            number(w->sq[1].score, 25 * 8, 14 * 8 - 4 + oy, 3);
        }
    }
    if (state == DS_TITLE || state == DS_READY)
        spr(SPR_HINT + ((t / 30) % 2), center("PRESS A TO SWIM", 0) * 8 - 12, 21 * 8 - 4, 3, -1);
    draw_squids(w);
    draw_fx(1);
    draw_caps(w);
    draw_fx(0);
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(back_dx); S(line_col); S(cur_rgb); S(depth); S(paused_now); S(drawn_index); S(cleared_col); S(drawn_seed);
    S(big_chars); S(big_used); S(fx_rng); S(logo_on); S(last_dark); S(fx); S(shown_state); S(shown_best);
    RS_STATE_RASTER(raster);
}
#undef S
