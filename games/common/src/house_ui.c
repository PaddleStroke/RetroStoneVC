/*
 * The 8BCraft house UI kit: font, panels, banners, logo, kit sprites, motion (house_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Extracted from Leady Squid's draw.c and make_art.py: Leady Squid draws its UI with it, pixel for pixel.
 */
#include "house_ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t rs_font_5x7[96][8];

#define T_FONT   0
#define T_BOX    96
#define T_BIG    192
#define BIG_SLOTS 24
#define T_PANEL  288

/* the UI palette: Leady Squid's text_pal (entries 1..7), then the extras */
const rs_color hu_ui_palette[16] = {
    0, RS_HEX(0xfff6dc), RS_HEX(0x101838), RS_HEX(0x2a4476), RS_HEX(0x78aad2), RS_HEX(0x0c142c),
    RS_HEX(0xfad25a), RS_HEX(0x365488), RS_HEX(0xe8584c), RS_HEX(0x6cc860), RS_HEX(0x9aa2b4)};

/* the kit sprites' palette (house_style.py UI and SPARK) */
enum { O_OUT = 1, O_WHITE, O_SHADE, O_BD, O_BL, O_SD, O_SL, O_GD, O_GL, O_PD, O_PL, O_PEARL, O_SPARK, O_SPARKW };
const rs_color hu_obj_palette[16] = {
    0, RS_RGB8(22, 18, 40), RS_RGB8(250, 250, 250), RS_RGB8(168, 200, 232), RS_RGB8(138, 76, 38),
    RS_RGB8(210, 142, 82), RS_RGB8(128, 138, 156), RS_RGB8(222, 230, 238), RS_RGB8(186, 128, 22),
    RS_RGB8(252, 216, 72), RS_RGB8(200, 138, 170), RS_RGB8(242, 202, 214), RS_RGB8(255, 250, 238),
    RS_RGB8(255, 238, 150), RS_RGB8(255, 255, 255)};

/* kit sprite tiles, relative to cfg.obj_tile */
#define S_DIGITS 0              /* 10 x 4 */
#define S_GLYPHS 40             /* 4 buttons x 2 frames x 4 */
#define S_MEDALS 72             /* 4 x 9 (24x24) */
#define S_SPARK  108            /* 2 x 1 */

static hu_config cfg;
static int big_chars[BIG_SLOTS], big_used;
static int paused_now;
static int shake_amp, shake_t, shake_len, shake_dx, shake_dy;

hu_config hu_defaults(void)
{
    hu_config c;
    memset(&c, 0, sizeof c);
    c.layer = RS_BG1;
    c.vram_base = 0;
    c.tile_base = 0;
    c.pal = 0;
    c.logo_tile = 320;
    c.logo_pal = 5;
    c.obj_tile = 0;
    c.obj_vram = 3072;
    c.obj_pal = 3;
    c.box_glyphs = NULL;
    return c;
}

const hu_config *hu_cfg(void) { return &cfg; }

/* ---- tiles -------------------------------------------------------------------------------------------------------- */
static void bg_tile(int rel, const uint8_t *px64) { rs_tiles_load8(cfg.vram_base + cfg.tile_base + rel, px64, 1); }

static int font_bit(int c, int x, int y)
{
    if (x < 0 || x > 7 || y < 0 || y > 7) return 0;
    return (rs_font_5x7[c][y] & (0x80 >> x)) != 0;
}

/* the small font on a background colour (bg 3: the panel fill) */
static void glyph_tile(int c, int bg, int rel)
{
    uint8_t t[64];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int on = font_bit(c, x, y);
            int sh = x > 0 && y > 0 && font_bit(c, x - 1, y - 1);
            t[y * 8 + x] = (uint8_t)(on ? HU_CREAM : sh ? HU_OUTLINE : bg);
        }
    bg_tile(rel, t);
}

/* the panel frame: 3x3 tiles (corners, edges, fill) */
static void panel_tiles(void)
{
    for (int k = 0; k < 9; k++) {
        uint8_t t[64];
        int kx = k % 3, ky = k / 3;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                int ex = kx == 0 ? x : kx == 2 ? 7 - x : 8;   /* distance to the outer edge */
                int ey = ky == 0 ? y : ky == 2 ? 7 - y : 8;
                int v = HU_FILL;
                if (kx != 1 && ky != 1 && ex + ey < 3) v = 0;         /* rounded corner */
                else if (ex == 0 || ey == 0 || (kx != 1 && ky != 1 && ex + ey == 3)) v = HU_DARK;
                else if (ex == 1 || ey == 1 || (kx != 1 && ky != 1 && ex + ey == 4))
                    v = (ky == 2 || kx == 2) ? HU_DARK : HU_LIGHT;
                else if (ey == 2 && ky == 0) v = HU_HILITE;
                t[y * 8 + x] = (uint8_t)v;
            }
        bg_tile(T_PANEL + k, t);
    }
}

/* ---- the kit sprites (house_style.py digit, button_glyph, medal, sparkle) -------------------------------------------- */
typedef struct canvas { int w, h; uint8_t p[32][32]; } canvas;

static void cv_init(canvas *c, int w, int h) { memset(c, 0, sizeof *c); c->w = w; c->h = h; }
static void cv_set(canvas *c, int x, int y, int v) { if (x >= 0 && x < c->w && y >= 0 && y < c->h) c->p[y][x] = (uint8_t)v; }
static int  cv_get(const canvas *c, int x, int y) { return x >= 0 && x < c->w && y >= 0 && y < c->h ? c->p[y][x] : 0; }

static void cv_outline(canvas *c, int col, int diag)
{
    uint8_t add[32][32];
    memset(add, 0, sizeof add);
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++) {
            if (c->p[y][x]) continue;
            static const int nb[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
            for (int k = 0; k < (diag ? 8 : 4); k++) {
                int v = cv_get(c, x + nb[k][0], y + nb[k][1]);
                if (v && v != col) { add[y][x] = 1; break; }
            }
        }
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++)
            if (add[y][x]) c->p[y][x] = (uint8_t)col;
}

/* uploads a canvas as consecutive row-major 8x8 sprite tiles */
static void cv_upload(const canvas *c, int rel)
{
    int n = 0;
    for (int ty = 0; ty < c->h / 8; ty++)
        for (int tx = 0; tx < c->w / 8; tx++) {
            uint8_t t[64];
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) t[y * 8 + x] = c->p[ty * 8 + y][tx * 8 + x];
            rs_tiles_load8(cfg.obj_vram + cfg.obj_tile + rel + n++, t, 1);
        }
}

static void make_digit(int n, canvas *c)
{
    cv_init(c, 16, 16);
    int g = '0' + n - 32;
    for (int y = 0; y < 7; y++)
        for (int b = 1; b <= 5; b++)
            if (font_bit(g, b, y))
                for (int k = 0; k < 4; k++)
                    cv_set(c, 2 + (b - 1) * 2 + (k & 1), 1 + y * 2 + (k >> 1), y >= 5 ? O_SHADE : O_WHITE);
    cv_outline(c, O_OUT, 1);
}

static void make_glyph(char label, int frame, canvas *c)
{
    cv_init(c, 16, 16);
    int oy = frame ? 1 : 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            double d = hypot(x + 0.5 - 8, y + 0.5 - 7.5 - oy);
            if (d < 6.2) cv_set(c, x, y, (x + y) < 14 ? O_SL : O_SD);
            else if (d < 7.2 && y > 8 && !frame) cv_set(c, x, y, O_SD);
        }
    int g = label - 32;
    for (int y = 0; y < 7; y++)
        for (int b = 1; b <= 5; b++)
            if (font_bit(g, b, y)) cv_set(c, 6 + b - 1, 4 + y + oy, O_OUT);
    cv_outline(c, O_OUT, 0);
}

static void make_medal(int kind, canvas *c)
{
    static const uint8_t dl[4][2] = {{O_BD, O_BL}, {O_SD, O_SL}, {O_GD, O_GL}, {O_PD, O_PL}};
    int dark = dl[kind][0], light = dl[kind][1];
    const double pi = 3.14159265358979323846;
    cv_init(c, 24, 24);
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 24; x++) {
            double dx = x + 0.5 - 12, dy = y + 0.5 - 17;
            double r = hypot(dx, dy), ang = atan2(-dy, dx);
            if (0.08 < ang && ang < pi - 0.08 && r < 10.5 - 0.8 * fabs(cos(ang * 7))) {
                int rib = (int)((ang / pi) * 7 + 0.5);
                int edge = fabs((ang / pi) * 7 - rib) < 0.16;
                int col = (edge || r > 9.3) ? dark : light;
                if (dx + dy < -6 && !edge) col = r < 7 ? O_WHITE : light;
                cv_set(c, x, y, col);
            }
            if (y >= 16 && y <= 19 && fabs(dx) < 3.2) cv_set(c, x, y, dark);   /* the hinge */
        }
    if (kind == 3) {                                                            /* the pearl */
        for (int y = 0; y < 24; y++)
            for (int x = 0; x < 24; x++)
                if (hypot(x + 0.5 - 12, y + 0.5 - 13) < 3.3) cv_set(c, x, y, (x + y) > 22 ? O_PEARL : O_WHITE);
        cv_set(c, 11, 12, O_WHITE);
    }
    cv_outline(c, O_OUT, 0);
}

static void make_sparkle(int frame, canvas *c)
{
    cv_init(c, 8, 8);
    int r = frame == 0 ? 3 : 2;
    for (int i = -r; i <= r; i++) { cv_set(c, 4 + i, 4, O_SPARK); cv_set(c, 4, 4 + i, O_SPARK); }
    cv_set(c, 4, 4, O_SPARKW);
    if (frame == 1)
        for (int d = -1; d <= 1; d += 2) { cv_set(c, 4 + d, 4 + d, O_SPARK); cv_set(c, 4 + d, 4 - d, O_SPARK); }
}

static void kit_sprites(void)
{
    canvas c;
    for (int n = 0; n < 10; n++) { make_digit(n, &c); cv_upload(&c, S_DIGITS + n * 4); }
    for (int b = 0; b < 4; b++)
        for (int f = 0; f < 2; f++) { make_glyph("ABXY"[b], f, &c); cv_upload(&c, S_GLYPHS + (b * 2 + f) * 4); }
    for (int m = 0; m < 4; m++) { make_medal(m, &c); cv_upload(&c, S_MEDALS + m * 9); }
    for (int f = 0; f < 2; f++) { make_sparkle(f, &c); cv_upload(&c, S_SPARK + f); }
}

/* ---- set-up --------------------------------------------------------------------------------------------------------- */
void hu_init(const hu_config *c)
{
    cfg = *c;
    rs_text_load(cfg.vram_base + cfg.tile_base + T_FONT, 0);
    /* the font on the panel colour (only the glyphs asked for: VRAM) */
    if (cfg.box_glyphs) {
        for (const char *p = cfg.box_glyphs; *p; p++) glyph_tile(*p - 32, HU_FILL, T_BOX + *p - 32);
    } else {
        for (int g = 0; g < 96; g++) glyph_tile(g, HU_FILL, T_BOX + g);
    }
    panel_tiles();
    rs_pal_load(RS_PAL_BG(cfg.pal), hu_ui_palette, 16);
    if (cfg.obj_tile >= 0) {
        kit_sprites();
        rs_pal_load(RS_PAL_OBJ(cfg.obj_pal), hu_obj_palette, 16);
    }
    cfg.box_glyphs = NULL;         /* no pointer in the saved state */
    big_used = 0;
    paused_now = 0;
    shake_amp = shake_t = shake_len = shake_dx = shake_dy = 0;
}

#define S(v) rs_state_var("house_ui." #v, &(v), sizeof(v))
void hu_state(void)
{
    S(cfg); S(big_chars); S(big_used); S(paused_now); S(shake_amp); S(shake_t); S(shake_len); S(shake_dx); S(shake_dy);
}
#undef S

/* ---- text ----------------------------------------------------------------------------------------------------------- */
static int bg_entry(int rel, int prio) { return RS_MAP(cfg.tile_base + rel, cfg.pal, prio, 0, 0); }

void hu_text(int x, int y, const char *s)
{
    rs_text_setup(cfg.layer, cfg.tile_base + T_FONT, cfg.pal, 1);
    rs_text(x, y, s);
}

void hu_box_text(int x, int y, const char *s)
{
    rs_text_setup(cfg.layer, cfg.tile_base + T_BOX, cfg.pal, 1);
    rs_text(x, y, s);
}

/* 2x glyphs with a full outline, uploaded on demand into a small cache. HU_BIG_FREE: cream on a clear
 * background (over the scene); HU_BIG_BANNER: gold with an outline and a drop shadow on the panel colour. */
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
    if (style == HU_BIG_BANNER) {
        /* the drop shadow: one pixel down-right of the outline */
        for (int y = 15; y > 0; y--)
            for (int x = 15; x > 0; x--)
                if (!px[y][x] && px[y - 1][x - 1] == 2) px[y][x] = 3;
    }
    static const uint8_t col[2][4] = {{0, HU_CREAM, HU_OUTLINE, 0}, {HU_FILL, HU_GOLD, HU_OUTLINE, HU_DARK}};
    for (int q = 0; q < 4; q++) {
        uint8_t t[64];
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) t[y * 8 + x] = col[style][px[(q >> 1) * 8 + y][(q & 1) * 8 + x]];
        bg_tile(T_BIG + s * 4 + q, t);
    }
    return s;
}

void hu_big(int x, int y, const char *s, int style)
{
    for (; *s; s++, x += 2) {
        if (*s == ' ') continue;
        int t = T_BIG + big_slot(*s, style) * 4;
        for (int q = 0; q < 4; q++) rs_bg_put(cfg.layer, x + (q & 1), y + (q >> 1), bg_entry(t + q, 1));
    }
}

int hu_center(const char *s, int big) { return (40 - (int)strlen(s) * (big ? 2 : 1)) / 2; }

void hu_clear(void)
{
    rs_bg_fill(cfg.layer, RS_MAP(cfg.tile_base + T_FONT, 0, 0, 0, 0));
    big_used = 0;
}

void hu_clear_rows(int y, int h)
{
    rs_text_setup(cfg.layer, cfg.tile_base + T_FONT, cfg.pal, 1);
    rs_text_clear(0, y, 40, h);
}

/* ---- panels and banners ------------------------------------------------------------------------------------------------ */
void hu_panel(int x0, int y0, int w, int h)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int kx = x == 0 ? 0 : x == w - 1 ? 2 : 1, ky = y == 0 ? 0 : y == h - 1 ? 2 : 1;
            rs_bg_put(cfg.layer, x0 + x, y0 + y, bg_entry(T_PANEL + ky * 3 + kx, 1));
        }
}

void hu_banner(int y, const char *title)
{
    int w = (int)strlen(title) * 2 + 2;
    if (w < 20) w = 20;
    hu_panel((40 - w) / 2, y, w, 4);
    hu_big(hu_center(title, 1), y + 1, title, HU_BIG_BANNER);
}

void hu_get_ready(int y) { hu_big(hu_center("GET READY", 1), y, "GET READY", HU_BIG_FREE); }

int hu_blink(int t) { return (t / 30) % 2 == 0; }

int hu_prompt(int y, const char *text, int blink_t)
{
    int x = hu_center(text, 0);
    if (hu_blink(blink_t)) hu_text(x + 1, y, text);
    else hu_clear_rows(y, 1);
    return x * 8 - 12;
}

void hu_copyright(int y)
{
    static const char c[] = "(C) 2026 8BCRAFT - RETROSTONE VC";
    hu_text(hu_center(c, 0), y, c);
}

void hu_join_line(int y, int players, const char *joined)
{
    static const char j[] = "P2: PRESS A ON PAD 2 TO JOIN";
    char s[48];
    if (players >= 2) {
        snprintf(s, sizeof s, "P2 JOINED - %s", joined ? joined : "GO!");
        hu_clear_rows(y, 1);
        hu_text(hu_center(s, 0), y, s);
    } else {
        hu_text(hu_center(j, 0), y, j);
    }
}

void hu_gameover_panel(int players, int new_best, int score1, int score2)
{
    hu_panel(10, 9, 20, 12);
    if (players == 1) {
        hu_box_text(12, 11, "SCORE");
        hu_box_text(12, 14, new_best ? "NEW BEST" : "BEST");
        hu_box_text(12, 17, "MEDAL");
    } else {
        hu_box_text(12, 11, "PLAYER 1");
        hu_box_text(12, 14, "PLAYER 2");
        const char *win = score1 > score2 ? "P1 WINS!" : score2 > score1 ? "P2 WINS!" : "DRAW!";
        hu_box_text(20 - (int)strlen(win) / 2, 17, win);
    }
}

void hu_gameover_sprites(int players, int score1, int score2, int best, int medal, int t, int slide_px)
{
    int oy = slide_px;
    hu_number(score1, 25 * 8, 11 * 8 - 4 + oy, 3);
    hu_number(players == 1 ? best : score2, 25 * 8, 14 * 8 - 4 + oy, 3);
    if (players != 1) return;
    if (medal) {
        hu_medal(medal, 22 * 8 - 4, 16 * 8 - 4 + oy, 3);
        if ((t / 20) % 3 == 0) hu_sparkle(22 * 8 + 12, 16 * 8 - 4 + oy, (t / 10) % 2, 3);
    } else {
        hu_box_text(22, 17, "-");
    }
}

void hu_retry_line(int t, int lock, const char *text)
{
    if (t < lock) return;
    char blank[41];
    int n = (int)strlen(text);
    if (n > 40) n = 40;
    memset(blank, ' ', (size_t)n);
    blank[n] = 0;
    hu_box_text(hu_center(text, 0), 19, hu_blink(t) ? text : blank);
}

void hu_pause(int paused, int y)
{
    rs_brightness(paused ? 9 : 15);
    if (paused != paused_now) {
        paused_now = paused;
        if (paused) hu_big(hu_center("PAUSED", 1), y, "PAUSED", HU_BIG_FREE);
        else hu_clear_rows(y, 2);
    }
}

/* ---- sprites -------------------------------------------------------------------------------------------------------------- */
static void kit_spr(int rel, int x, int y, int w, int h, int prio)
{
    if (cfg.obj_tile < 0 || x <= -w || x >= RS_SCREEN_W || y <= -h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, cfg.obj_tile + rel, w, h, cfg.obj_pal, prio, 0);
}

void hu_number(int n, int cx, int y, int prio)
{
    char s[12];
    snprintf(s, sizeof s, "%d", n < 0 ? 0 : n);
    int len = (int)strlen(s), x = cx - len * 6;
    for (int i = 0; i < len; i++) kit_spr(S_DIGITS + (s[i] - '0') * 4, x + i * 12 - 2, y, 16, 16, prio);
}

void hu_glyph(int button, int x, int y, int pressed, int prio)
{
    kit_spr(S_GLYPHS + ((button & 3) * 2 + (pressed ? 1 : 0)) * 4, x, y, 16, 16, prio);
}

void hu_medal(int tier, int x, int y, int prio)
{
    if (tier >= 1 && tier <= 4) kit_spr(S_MEDALS + (tier - 1) * 9, x, y, 24, 24, prio);
}

void hu_sparkle(int x, int y, int frame, int prio) { kit_spr(S_SPARK + (frame & 1), x, y, 8, 8, prio); }

int hu_medal_of(int score, const int th[4])
{
    int m = 0;
    for (int i = 0; i < 4; i++)
        if (score >= th[i]) m = i + 1;
    return m;
}

/* ---- the title logo ------------------------------------------------------------------------------------------------------- */
#define LOGO_W 320
#define LOGO_H 64
typedef uint8_t logo_buf[LOGO_H][LOGO_W];

static int logo_mask(logo_buf px, int x, int y) { return x >= 0 && x < LOGO_W && y >= 0 && y < LOGO_H && px[y][x] >= 3; }

int hu_logo(const char *title, const rs_color (*ramps)[3], int nramps, int ty, int rivets)
{
    if (cfg.logo_tile < 0 || !title) return 0;
    int len = (int)strlen(title);
    int sc = (len * (5 * 4 + 2) - 2 <= 304) ? 4 : 3;
    int adv = 5 * sc + 2, total = len * adv - 2;
    int x0 = (LOGO_W - total) / 2, y0 = 14;
    /* scratch on the heap (no static: nothing to save in a state) */
    uint8_t (*logo_px)[LOGO_W] = calloc(2, sizeof(logo_buf));
    if (!logo_px) return 0;
    uint8_t (*tmp)[LOGO_W] = logo_px + LOGO_H;
    /* palette: 1 outline, 2 shadow, 3.. ramps (3 per word, up to 4 words), 15 white */
    rs_pal_set(RS_PAL_BG(cfg.logo_pal) + 1, RS_RGB8(26, 16, 40));
    rs_pal_set(RS_PAL_BG(cfg.logo_pal) + 2, RS_RGB8(20, 34, 70));
    rs_pal_set(RS_PAL_BG(cfg.logo_pal) + 15, RS_RGB8(250, 250, 250));
    int nw = nramps < 1 ? 1 : nramps > 4 ? 4 : nramps;
    for (int w = 0; w < nw; w++)
        for (int k = 0; k < 3; k++) rs_pal_set(RS_PAL_BG(cfg.logo_pal) + 3 + w * 3 + k, ramps[w][k]);
    int word = 0, first_word_end = len;
    for (int i = 0; i < len; i++) {
        char ch = title[i];
        if (ch == ' ') {
            if (word == 0) first_word_end = i;
            word++;
            continue;
        }
        int g = (unsigned char)ch - 32;
        if (g < 0 || g >= 96) g = '?' - 32;
        int rw = word < nw ? word : nw - 1;
        for (int y = 0; y < 7; y++)
            for (int b = 1; b <= 5; b++) {
                if (!font_bit(g, b, y)) continue;
                for (int sy = 0; sy < sc; sy++)
                    for (int sx = 0; sx < sc; sx++) {
                        int X = x0 + i * adv + (b - 1) * sc + sx, Y = y0 + y * sc + sy;
                        int yy = y * sc + sy;
                        int k = yy < 2 * sc ? 0 : 2 * yy < 9 * sc ? 1 : 2;
                        if (X >= 0 && X < LOGO_W && Y < LOGO_H) logo_px[Y][X] = (uint8_t)(3 + rw * 3 + k);
                    }
            }
    }
    /* the drop shadow (3 and 4 px down, half as far right), then the outline, over the transparent pixels */
    memcpy(tmp, logo_px, sizeof(logo_buf));
    for (int y = 0; y < LOGO_H; y++)
        for (int x = 0; x < LOGO_W; x++) {
            if (!logo_mask(logo_px, x, y)) continue;
            for (int d = 3; d <= 4; d++) {
                int X = x + d / 2, Y = y + d;
                if (X < LOGO_W && Y < LOGO_H && !logo_mask(logo_px, X, Y)) tmp[Y][X] = 2;
            }
        }
    for (int y = 0; y < LOGO_H; y++)
        for (int x = 0; x < LOGO_W; x++) {
            if (!logo_mask(logo_px, x, y)) continue;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (x + dx >= 0 && x + dx < LOGO_W && y + dy >= 0 && y + dy < LOGO_H && !logo_mask(logo_px, x + dx, y + dy))
                        tmp[y + dy][x + dx] = 1;
        }
    if (rivets)
        for (int i = 0; i < first_word_end; i++) {
            int cx = x0 + i * adv + 2 * sc;
            if (logo_mask(logo_px, cx, y0 + 2)) tmp[y0 + 2][cx] = 15;
        }
    memcpy(logo_px, tmp, sizeof(logo_buf));
    /* tiles: the blank ones map to the font's space */
    int used = 0;
    for (int tyy = 0; tyy < LOGO_H / 8; tyy++)
        for (int tx = 0; tx < LOGO_W / 8; tx++) {
            uint8_t t[64];
            int any = 0;
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) any |= (t[y * 8 + x] = logo_px[tyy * 8 + y][tx * 8 + x]);
            uint16_t e = (uint16_t)RS_MAP(cfg.tile_base + T_FONT, 0, 0, 0, 0);
            if (any && used < HU_LOGO_TILES) {
                rs_tiles_load8(cfg.vram_base + cfg.logo_tile + used, t, 1);
                e = (uint16_t)RS_MAP(cfg.logo_tile + used, cfg.logo_pal, 1, 0, 0);
                used++;
            }
            rs_bg_put(cfg.layer, tx, ty + tyy, e);
        }
    free(logo_px);
    return used;
}

void hu_logo_hide(void)
{
    for (int y = 0; y < 30; y++)
        for (int x = 0; x < 40; x++) {
            uint16_t e = rs_bg_get(cfg.layer, x, y);
            if (RS_MAP_PAL(e) == cfg.logo_pal && RS_MAP_TILE(e) >= cfg.logo_tile && RS_MAP_TILE(e) < cfg.logo_tile + HU_LOGO_TILES)
                rs_bg_put(cfg.layer, x, y, RS_MAP(cfg.tile_base + T_FONT, 0, 0, 0, 0));
        }
}

/* ---- motion ------------------------------------------------------------------------------------------------------------------ */
static int clamp_t(int t, int dur) { return t < 0 ? 0 : t > dur ? dur : t; }

int hu_ease_linear(int t, int dur, int dist)
{
    if (dur <= 0) return dist;
    return clamp_t(t, dur) * dist / dur;
}

int hu_ease_out(int t, int dur, int dist)
{
    if (dur <= 0) return dist;
    int r = dur - clamp_t(t, dur);
    return dist - r * r * dist / (dur * dur);
}

int hu_ease_in(int t, int dur, int dist)
{
    if (dur <= 0) return dist;
    int c = clamp_t(t, dur);
    return c * c * dist / (dur * dur);
}

int hu_ease_inout(int t, int dur, int dist)
{
    if (dur <= 0) return dist;
    int c = clamp_t(t, dur), h = dur / 2 > 0 ? dur / 2 : 1;
    if (c < h) return hu_ease_in(c, h, dist / 2);
    return dist / 2 + hu_ease_out(c - h, dur - h, dist - dist / 2);
}

int hu_ease_back(int t, int dur, int dist)
{
    /* 0 -> 110% at 70% of the time -> 100% */
    if (dur <= 0) return dist;
    int c = clamp_t(t, dur), k = dur * 7 / 10 > 0 ? dur * 7 / 10 : 1;
    int over = dist + dist / 10;
    if (c < k) return hu_ease_out(c, k, over);
    return over - hu_ease_inout(c - k, dur - k, over - dist);
}

int hu_slide_in(int t, int dur, int dist)
{
    if (t >= dur || dur <= 0) return 0;
    if (t < 0) t = 0;
    return (dur - t) * (dur - t) * dist / (dur * dur);
}

int hu_bob(int t, int period, int ampl)
{
    if (period < 4) return 0;
    int p = t % period, q = period / 4, v;
    if (p < q) v = p;
    else if (p < 3 * q) v = 2 * q - p;
    else v = p - 4 * q;
    return v * ampl / q;
}

void hu_shake(int amp, int frames)
{
    if (amp > 4) amp = 4;
    if (frames > 16) frames = 16;
    if (amp >= shake_amp || shake_t >= shake_len) { shake_amp = amp; shake_len = frames; shake_t = 0; }
}

void hu_shake_step(void)
{
    /* a fixed wobble (no RNG), decaying linearly to 0 */
    static const int8_t wx[8] = {1, -1, 1, 0, -1, 1, -1, 0}, wy[8] = {-1, 1, 0, 1, -1, 0, 1, -1};
    if (shake_t >= shake_len) { shake_dx = shake_dy = 0; return; }
    int a = shake_amp * (shake_len - shake_t) / shake_len;
    if (a < 1) a = 1;
    shake_dx = wx[shake_t & 7] * a;
    shake_dy = wy[shake_t & 7] * a;
    shake_t++;
}

int hu_shake_x(void) { return shake_dx; }
int hu_shake_y(void) { return shake_dy; }

/* ---- colours ---------------------------------------------------------------------------------------------------------------------- */
rs_color hu_lerp_color(rs_color a, rs_color b, int k)
{
    int r = ((a & 31) * (256 - k) + (b & 31) * k) / 256;
    int g = (((a >> 5) & 31) * (256 - k) + ((b >> 5) & 31) * k) / 256;
    int bl = (((a >> 10) & 31) * (256 - k) + ((b >> 10) & 31) * k) / 256;
    return RS_RGB(r, g, bl);
}

rs_color hu_scale_color(rs_color c, int k)
{
    return RS_RGB((c & 31) * k / 256, ((c >> 5) & 31) * k / 256, ((c >> 10) & 31) * k / 256);
}
