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
#define S_DPAD   110            /* 2 frames x 4 */

/* the title lobby (saved): who plays, on which pad, since when; the title's set-up */
typedef struct lobby_t {
    uint16_t start;
    uint8_t max, n;
    uint8_t pad[HU_MAX_PLAYERS];
    uint8_t age[HU_MAX_PLAYERS];
    int8_t glyph;               /* HU_BTN_A, HU_BTN_DPAD or -1 */
    uint8_t slot_row;
    int16_t glyph_x;
    char prompt[41];
} lobby_t;

static hu_config cfg;
static int big_chars[BIG_SLOTS], big_used;
static int paused_now;
static int shake_amp, shake_t, shake_len, shake_dx, shake_dy;
static lobby_t lobby;
static uint8_t box_have[12];    /* bit g: the panel font's glyph g is in VRAM (hu_box_text uploads the others) */
static uint8_t dpad_loaded;

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

/* the arrows drawn over '{' '}' '^' '~' (HU_ARROW_*): 5 columns (bit 4 = the left one) x 7 rows */
static const uint8_t arrow_rows[4][7] = {
    {0x00, 0x04, 0x08, 0x1f, 0x08, 0x04, 0x00},     /* left */
    {0x00, 0x04, 0x02, 0x1f, 0x02, 0x04, 0x00},     /* right */
    {0x04, 0x0e, 0x15, 0x04, 0x04, 0x04, 0x04},     /* up */
    {0x04, 0x04, 0x04, 0x04, 0x15, 0x0e, 0x04}};    /* down */

static int arrow_of(int c)
{
    switch (c + 32) {
    case '{': return 0;
    case '}': return 1;
    case '^': return 2;
    case '~': return 3;
    default: return -1;
    }
}

static int font_bit(int c, int x, int y)
{
    if (x < 0 || x > 7 || y < 0 || y > 7) return 0;
    int a = arrow_of(c);
    if (a >= 0) return x >= 1 && x <= 5 && y < 7 && ((arrow_rows[a][y] >> (5 - x)) & 1);
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

/* the D-pad: a silver cross with a dark centre dimple; frame 1 pressed (1 px lower, no under-shade) */
static void make_dpad(int frame, canvas *c)
{
    cv_init(c, 16, 16);
    int oy = frame ? 1 : 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            int yy = y - oy;
            int in = (x >= 6 && x <= 9 && yy >= 2 && yy <= 13) || (yy >= 6 && yy <= 9 && x >= 2 && x <= 13);
            if (in) cv_set(c, x, y, (x + yy) < 15 ? O_SL : O_SD);
            else if (!frame && yy == 14 && x >= 6 && x <= 9) cv_set(c, x, y, O_SD);
            else if (!frame && yy == 10 && ((x >= 2 && x <= 5) || (x >= 10 && x <= 13))) cv_set(c, x, y, O_SD);
        }
    for (int y = 7; y <= 8; y++)
        for (int x = 7; x <= 8; x++) cv_set(c, x, y + oy, O_SD);
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

/* the D-pad glyph: only for the games whose title asks for it (VRAM) */
static void dpad_sprites(void)
{
    canvas c;
    if (dpad_loaded || cfg.obj_tile < 0) return;
    for (int f = 0; f < 2; f++) { make_dpad(f, &c); cv_upload(&c, S_DPAD + f * 4); }
    dpad_loaded = 1;
}

static void box_glyph(int g)
{
    if (g < 0 || g >= 96 || (box_have[g >> 3] & (1 << (g & 7)))) return;
    glyph_tile(g, HU_FILL, T_BOX + g);
    box_have[g >> 3] |= (uint8_t)(1 << (g & 7));
}

static void lobby_reset(void)
{
    memset(&lobby, 0, sizeof lobby);
    lobby.start = RS_BTN_A | RS_BTN_START;
    lobby.max = 1;
    lobby.n = 1;
    lobby.glyph = HU_BTN_A;
    lobby.slot_row = HU_ROW_SLOTS;
    snprintf(lobby.prompt, sizeof lobby.prompt, "PRESS A TO PLAY");
}

/* ---- set-up --------------------------------------------------------------------------------------------------------- */
void hu_init(const hu_config *c)
{
    cfg = *c;
    rs_text_load(cfg.vram_base + cfg.tile_base + T_FONT, 0);
    for (int g = 0; g < 96; g++)                           /* the arrows over four unused glyphs */
        if (arrow_of(g) >= 0) glyph_tile(g, 0, T_FONT + g);
    /* the font on the panel colour (only the glyphs asked for: VRAM; hu_box_text adds the others when needed) */
    memset(box_have, 0, sizeof box_have);
    dpad_loaded = 0;
    if (cfg.box_glyphs) {
        for (const char *p = cfg.box_glyphs; *p; p++) box_glyph(*p - 32);
    } else {
        for (int g = 0; g < 96; g++) box_glyph(g);
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
    lobby_reset();
}

#define S(v) rs_state_var("house_ui." #v, &(v), sizeof(v))
void hu_state(void)
{
    S(cfg); S(big_chars); S(big_used); S(paused_now); S(shake_amp); S(shake_t); S(shake_len); S(shake_dx); S(shake_dy);
    S(lobby); S(box_have); S(dpad_loaded);
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
    for (const char *p = s; *p; p++) box_glyph((unsigned char)*p - 32);
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
            if (font_bit(c, x, y))
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

void hu_retry_line_at(int row, int t, int lock, const char *text)
{
    if (t < lock) return;
    char blank[41];
    int n = (int)strlen(text);
    if (n > 40) n = 40;
    memset(blank, ' ', (size_t)n);
    blank[n] = 0;
    hu_box_text(hu_center(text, 0), row, hu_blink(t) ? text : blank);
}

void hu_retry_line(int t, int lock, const char *text) { hu_retry_line_at(19, t, lock, text); }

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
    if (button == HU_BTN_DPAD) kit_spr(S_DPAD + (pressed ? 4 : 0), x, y, 16, 16, prio);
    else kit_spr(S_GLYPHS + ((button & 3) * 2 + (pressed ? 1 : 0)) * 4, x, y, 16, 16, prio);
}

void hu_number_at(int n, int x, int y, int align, int prio)
{
    char s[12];
    snprintf(s, sizeof s, "%d", n < 0 ? 0 : n);
    int w = (int)strlen(s) * 12;
    hu_number(n, align < 0 ? x + w / 2 : align > 0 ? x - w / 2 : x, y, prio);
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

/* ---- the kit's words (one table: a translation replaces it) ------------------------------------------------------------ */
static const struct {
    const char *press_to;       /* PRESS <input> TO <verb> */
    const char *any_arrow, *start;
    const char *join, *join_leave, *full;
    const char *best, *wins, *draw;
    const char *rank[HU_MAX_PLAYERS];
} words = {"PRESS %s TO %s", "ANY ARROW", "START", "%s: PRESS A TO JOIN", "%s: PRESS A TO JOIN - B: LEAVE",
           "%d PLAYERS - B: LEAVE", "BEST %d", "P%d WINS!", "DRAW!", {"1ST", "2ND", "3RD", "4TH"}};

/* ---- players and the title ----------------------------------------------------------------------------------------------- */
static int clampi_(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void hu_title_setup(const hu_title_cfg *c)
{
    lobby.start = c->start ? c->start : (RS_BTN_A | RS_BTN_START);
    lobby.max = (uint8_t)clampi_(c->max_players, 1, HU_MAX_PLAYERS);
    if (lobby.n > lobby.max) lobby.n = lobby.max;
    lobby.slot_row = (uint8_t)(c->slot_row > 0 ? c->slot_row : HU_ROW_SLOTS);
    const char *what;
    if ((lobby.start & HU_IN_DPAD) == HU_IN_DPAD) { what = words.any_arrow; lobby.glyph = HU_BTN_DPAD; }
    else if ((lobby.start & HU_IN_LR) == HU_IN_LR) { what = HU_ARROW_LEFT "/" HU_ARROW_RIGHT; lobby.glyph = HU_BTN_DPAD; }
    else if (lobby.start & RS_BTN_A) { what = "A"; lobby.glyph = HU_BTN_A; }
    else if (lobby.start & RS_BTN_B) { what = "B"; lobby.glyph = HU_BTN_B; }
    else { what = words.start; lobby.glyph = -1; }
    if (lobby.glyph == HU_BTN_DPAD) dpad_sprites();
    if (c->prompt) snprintf(lobby.prompt, sizeof lobby.prompt, "%s", c->prompt);
    else snprintf(lobby.prompt, sizeof lobby.prompt, words.press_to, what, c->verb ? c->verb : "PLAY");
}

void hu_players_set(int n)
{
    lobby.n = (uint8_t)clampi_(n, 1, lobby.max > 1 ? lobby.max : HU_MAX_PLAYERS);
    for (int p = 0; p < HU_MAX_PLAYERS; p++) {
        lobby.pad[p] = (uint8_t)p;
        lobby.age[p] = 255;
    }
}

int hu_players(void) { return lobby.n; }
int hu_player_pad(int p) { return p >= 0 && p < lobby.n ? lobby.pad[p] : p & 3; }
int hu_player_age(int p) { return p >= 0 && p < lobby.n ? lobby.age[p] : 0; }
uint16_t hu_start_inputs(void) { return lobby.start; }
int hu_start_pressed(int p) { return (rs_pad_pressed(hu_player_pad(p)) & lobby.start) != 0; }
int hu_player_pal(int p) { static const uint8_t pal[4] = {0, 1, 4, 5}; return pal[p & 3]; }

static int pad_joined(int pad)
{
    for (int p = 0; p < lobby.n; p++)
        if (lobby.pad[p] == pad) return 1;
    return 0;
}

int hu_title_update(void)
{
    int ev = 0;
    for (int p = 0; p < lobby.n; p++)
        if (lobby.age[p] < 255) lobby.age[p]++;
    for (int p = lobby.n - 1; p >= 1; p--)
        if (rs_pad_pressed(lobby.pad[p]) & HU_LEAVE_BUTTONS) {
            for (int q = p; q < lobby.n - 1; q++) {
                lobby.pad[q] = lobby.pad[q + 1];
                lobby.age[q] = lobby.age[q + 1];
            }
            lobby.n--;
            ev |= HU_TITLE_LEFT;
        }
    for (int pad = 1; pad < RS_PAD_MAX && lobby.n < lobby.max; pad++)
        if (!pad_joined(pad) && (rs_pad_pressed(pad) & HU_JOIN_BUTTONS)) {
            lobby.pad[lobby.n] = (uint8_t)pad;
            lobby.age[lobby.n] = 0;
            lobby.n++;
            ev |= HU_TITLE_JOINED;
        }
    if (rs_pad_pressed(lobby.pad[0]) & lobby.start) ev |= HU_TITLE_START;
    return ev;
}

void hu_slot_pos(int p, int *cx, int *cy)
{
    int col = 4 + 9 * (p & 3);
    if (cx) *cx = col * 8 + 28;
    if (cy) *cy = lobby.slot_row * 8 + 4;
}

void hu_title_draw(int t, int best)
{
    char s[48];
    lobby.glyph_x = (int16_t)hu_prompt(HU_ROW_PROMPT, lobby.prompt, t);
    hu_clear_rows(HU_ROW_BEST, 1);
    if (best > 0) {
        snprintf(s, sizeof s, words.best, best);
        hu_text(hu_center(s, 0), HU_ROW_BEST, s);
    }
    if (lobby.max > 1) {
        hu_clear_rows(lobby.slot_row, 1);
        for (int p = 0; p < lobby.n; p++) {
            snprintf(s, sizeof s, "P%d", p + 1);
            hu_text(4 + 9 * p, lobby.slot_row, s);
        }
        hu_clear_rows(HU_ROW_JOIN, 1);
        if (lobby.n < lobby.max) {
            char who[24] = "";
            for (int p = lobby.n; p < lobby.max; p++) {
                size_t k = strlen(who);
                snprintf(who + k, sizeof who - k, "%sP%d", p > lobby.n ? " / " : "", p + 1);
            }
            snprintf(s, sizeof s, lobby.n > 1 ? words.join_leave : words.join, who);
        } else {
            snprintf(s, sizeof s, words.full, lobby.n);
        }
        hu_text(hu_center(s, 0), HU_ROW_JOIN, s);
    }
    hu_copyright(HU_ROW_COPYRIGHT);
}

int hu_title_glyph(int *x, int *y)
{
    if (x) *x = lobby.glyph_x;
    if (y) *y = HU_ROW_PROMPT * 8 - 4;
    return lobby.glyph;
}

void hu_title_sprites(int t, hu_icon_fn icon, void *user)
{
    if (lobby.glyph >= 0) hu_glyph(lobby.glyph, lobby.glyph_x, HU_ROW_PROMPT * 8 - 4, (t / 30) % 2, 3);
    if (lobby.max < 2) return;
    for (int p = 0; p < lobby.n; p++) {
        int cx, cy, a = lobby.age[p];
        hu_slot_pos(p, &cx, &cy);
        if (a < 24) {                                   /* it pops in: up from below with a 10% overshoot */
            hu_sparkle(cx + 8, cy - 14, (a / 6) & 1, 3);
            hu_sparkle(cx - 14, cy - 2, (a / 6 + 1) & 1, 3);
        }
        if (icon) icon(p, cx, cy + (a < 16 ? 12 - hu_ease_back(a, 16, 12) : 0), t, user);
    }
}

/* ---- the 4-player HUD ------------------------------------------------------------------------------------------------------ */
hu_chip hu_score_chip(int players, int p, int bottom_y)
{
    hu_chip c = {RS_SCREEN_W / 2, 10, 0, -1, -1};
    if (bottom_y <= 0) bottom_y = 220;
    if (players == 2) c.x = p ? 240 : 80;
    else if (players >= 3) {
        c.y = p >= 2 ? bottom_y : 4;
        c.tag_y = (c.y + 4) / 8;
        if (p & 1) { c.tag_x = 37; c.x = 37 * 8 - 4; c.align = 1; }
        else { c.tag_x = 1; c.x = 3 * 8 + 4; c.align = -1; }
    }
    return c;
}

void hu_score_tags(int players, int bottom_y)
{
    char s[12];
    for (int p = 0; p < players && p < HU_MAX_PLAYERS; p++) {
        hu_chip c = hu_score_chip(players, p, bottom_y);
        if (c.tag_x < 0) continue;
        snprintf(s, sizeof s, "P%d", p + 1);
        hu_text(c.tag_x, c.tag_y, s);
    }
}

void hu_score_chips(int players, const int score[], int bottom_y)
{
    for (int p = 0; p < players && p < HU_MAX_PLAYERS; p++) {
        hu_chip c = hu_score_chip(players, p, bottom_y);
        hu_number_at(score[p], c.x, c.y, c.align, 3);
    }
}

static void view_rect(rs_viewport *v, int x, int y, int w, int h)
{
    memset(v, 0, sizeof *v);
    v->x = (int16_t)x; v->y = (int16_t)y; v->w = (int16_t)w; v->h = (int16_t)h;
    v->layers = 0x0f;
    v->objs = 1;
}

int hu_split(int players, int mode, rs_viewport *out)
{
    int n = clampi_(players, 1, HU_MAX_PLAYERS);
    if (n <= 2) return rs_viewport_layout(n, 0, out);
    if (mode == HU_SPLIT_QUAD) {
        rs_viewport_layout(4, 0, out);
        return n;
    }
    int w = (RS_SCREEN_W - RS_VIEW_DIVIDER * (n - 1)) / n;
    int x0 = (RS_SCREEN_W - n * w - RS_VIEW_DIVIDER * (n - 1)) / 2;
    for (int i = 0; i < n; i++) view_rect(&out[i], x0 + i * (w + RS_VIEW_DIVIDER), 0, w, RS_SCREEN_H);
    return n;
}

void hu_view_oam_begin(rs_viewport *v) { v->oam_first = (uint16_t)rs_oam_next(); v->oam_count = 0; }
void hu_view_oam_end(rs_viewport *v) { v->oam_count = (uint16_t)(rs_oam_next() - v->oam_first); }

/* ---- results: the ranking -------------------------------------------------------------------------------------------------- */
void hu_rank(hu_standing *s, int n, const int key[], const int value[])
{
    memset(s, 0, sizeof *s);
    s->n = clampi_(n, 1, HU_MAX_PLAYERS);
    for (int p = 0; p < s->n; p++) {
        s->order[p] = p;
        s->value[p] = value ? value[p] : key[p];
    }
    for (int i = 1; i < s->n; i++)                     /* insertion sort: stable, best key first */
        for (int j = i; j > 0 && key[s->order[j]] > key[s->order[j - 1]]; j--) {
            int t = s->order[j];
            s->order[j] = s->order[j - 1];
            s->order[j - 1] = t;
        }
    for (int i = 0; i < s->n; i++)
        s->rank[i] = i > 0 && key[s->order[i]] == key[s->order[i - 1]] ? s->rank[i - 1] : i;
}

int hu_winner(const hu_standing *s) { return s->n > 1 && s->rank[1] == 0 ? -1 : s->order[0]; }

int hu_results_row(const hu_standing *s, int place) { return 10 + 3 * place; }
int hu_results_retry_row(const hu_standing *s) { return 8 + 3 * s->n + 2; }
int hu_results_medal(const hu_standing *s, int place) { return s->rank[place] < 3 ? 3 - s->rank[place] : 0; }

void hu_results_icon_pos(const hu_standing *s, int place, int *cx, int *cy)
{
    if (cx) *cx = 19 * 8;
    if (cy) *cy = hu_results_row(s, place) * 8 + 4;
}

void hu_results_panel(const hu_standing *s, const char *title)
{
    char t[24];
    if (!title) {
        int w = hu_winner(s);
        if (w >= 0) snprintf(t, sizeof t, words.wins, w + 1);
        else snprintf(t, sizeof t, "%s", words.draw);
        title = t;
    }
    hu_banner(3, title);
    hu_panel(8, 8, 24, 3 * s->n + 4);
    for (int i = 0; i < s->n; i++) {
        char tag[16];
        int row = hu_results_row(s, i);
        hu_box_text(10, row, words.rank[s->rank[i] & 3]);
        snprintf(tag, sizeof tag, "P%d", (s->order[i] & 3) + 1);
        hu_box_text(14, row, tag);
    }
}

void hu_results_sprites(const hu_standing *s, int t, int slide_px)
{
    for (int i = 0; i < s->n; i++) {
        int y = hu_results_row(s, i) * 8 + slide_px, m = hu_results_medal(s, i);
        hu_number(s->value[s->order[i]], 24 * 8, y - 4, 3);
        if (m) {
            hu_medal(m, 27 * 8, y - 8, 3);
            if (s->rank[i] == 0 && (t / 20) % 3 == 0) hu_sparkle(27 * 8 + 18, y - 8, (t / 10) % 2, 3);
        }
    }
}
