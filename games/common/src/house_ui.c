/*
 * The 8BCraft house UI kit: font, panels, banners, logo, kit sprites, motion (house_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Extracted from Leady Squid's draw.c and make_art.py: Leady Squid draws its UI with it, pixel for pixel.
 */
#define HOUSE_TEXT_ENGLISH              /* house_text.h: the English, when a game has no text pack */
#include "house_ui.h"
#include "rs_i18n.h"
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
    char prompt[128];           /* UTF-8 */
} lobby_t;

static hu_config cfg;
static int big_chars[BIG_SLOTS], big_used;
static int paused_now;
static int shake_amp, shake_t, shake_len, shake_dx, shake_dy;
static lobby_t lobby;
static uint8_t box_have[12];    /* bit g: the panel font's glyph g is in VRAM (hu_box_text uploads the others) */
static uint8_t font_have[12];   /* ordinary glyphs are uploaded when used, just like the panel font */
static uint8_t dpad_loaded;
/* translated text: the pool of glyph tiles beyond ASCII (see "translated text" below) */
#define EXT_MAX 256
static uint64_t ext_key[EXT_MAX];       /* what each slot shows (0 = free) */
static uint16_t ext_under[EXT_MAX];     /* an overhang: the map entry it covers */
static uint8_t ext_used;                /* the pool has been used since the last clear */
static uint32_t err_seen[32];           /* the layout errors logged (once each) */
static int err_n, err_total;
static int ext_peak;                    /* the most pool tiles in use at once (logged: VRAM) */
static int8_t go_grow;                  /* the game-over panel's growth leftwards (long labels) */

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
    c.ext_tile = 640;
    c.ext_count = HU_EXT_TILES;
    return c;
}

const hu_config *hu_cfg(void) { return &cfg; }

static void font_glyph(int g);

/* ---- widescreen: the kit's 320-px UI area, centred on the screen ------------------------------------------------------ */
int hu_ui_x(void) { return rs_screen_margin() - rs_screen_origin_x(); }
int hu_ui_left(void) { return -rs_screen_margin(); }
int hu_ui_right(void) { return rs_screen_width() - rs_screen_margin(); }
static int fdiv8(int v) { return v >= 0 ? v / 8 : -((7 - v) / 8); }      /* floor(v / 8) */
/* the UI layer's map column of a UI column (columns left of 0 or right of 39 wrap round the map: widescreen corners) */
static int ui_col(int c) { int mw = rs_bg_map_w(cfg.layer); return mw > 0 ? c & (mw - 1) : c; }
/* text at a UI column that may be outside 0..39 */
static void ui_text_wrapped(int x, int y, const char *s)
{
    for (const char *p = s; *p; p++) font_glyph((unsigned char)*p - 32);
    for (int i = 0; s[i]; i++) {
        int c = (unsigned char)s[i];
        if (c < 32 || c > 127) c = '?';
        rs_bg_put(cfg.layer, ui_col(x + i), y, RS_MAP(cfg.tile_base + T_FONT + c - 32, cfg.pal, 1, 0, 0));
    }
}



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

/* the D-pad glyph (no title uses it any more: A starts every game; kept for a game that loads it itself) */
__attribute__((unused)) static void dpad_sprites(void)
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

static void font_glyph(int g)
{
    if (g < 0 || g >= 96) g = '?' - 32;
    if (font_have[g >> 3] & (1 << (g & 7))) return;
    glyph_tile(g, 0, T_FONT + g);
    font_have[g >> 3] |= (uint8_t)(1 << (g & 7));
}

static void lobby_reset(void)
{
    memset(&lobby, 0, sizeof lobby);
    lobby.start = RS_BTN_A | RS_BTN_START;
    lobby.max = 1;
    lobby.n = 1;
    lobby.glyph = HU_BTN_A;
    lobby.slot_row = HU_ROW_SLOTS;
    rs_strf(lobby.prompt, sizeof lobby.prompt, hu_str(HS_PRESS_TO), hu_str(HS_PLAY));
}

/* ---- set-up --------------------------------------------------------------------------------------------------------- */
void hu_init(const hu_config *c)
{
    cfg = *c;
    memset(font_have, 0, sizeof font_have);
    font_glyph(0);                                      /* hu_clear uses the space tile */
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
    if (cfg.ext_count > EXT_MAX) cfg.ext_count = EXT_MAX;
    memset(ext_key, 0, sizeof ext_key);
    ext_used = 0;
    big_used = 0;
    paused_now = 0;
    shake_amp = shake_t = shake_len = shake_dx = shake_dy = 0;
    lobby_reset();
}

#define S(v) rs_state_var("house_ui." #v, &(v), sizeof(v))
void hu_state(void)
{
    S(cfg); S(big_chars); S(big_used); S(paused_now); S(shake_amp); S(shake_t); S(shake_len); S(shake_dx); S(shake_dy);
    S(lobby); S(box_have); S(font_have); S(dpad_loaded);
    S(ext_key); S(ext_under); S(ext_used); S(err_seen); S(err_n); S(err_total); S(ext_peak); S(go_grow);
}
#undef S

/* ---- text ----------------------------------------------------------------------------------------------------------- */
static int bg_entry(int rel, int prio) { return RS_MAP(cfg.tile_base + rel, cfg.pal, prio, 0, 0); }

/* ---- translated text (docs/i18n.md) ----------------------------------------------------------------------------------
 * Strings are UTF-8. ASCII goes through the font tiles above, exactly as before. The rest lives in a pool of tiles
 * (cfg.ext_tile, cfg.ext_count), one slot per thing shown, keyed:
 *   - a letter of the house font beyond ASCII (Cyrillic, Greek, AE...; a letter with a mark below): its 8x8 tile;
 *     a letter that is an ASCII one plus marks reuses the ASCII tile;
 *   - an OVERHANG: a mark above a letter (an accent) or the top half of a 16x16 wide glyph (CJK) sits in the row
 *     ABOVE the text, drawn over what was there (the blank screen or the panel fill), which comes back when the text
 *     goes (ext_under);
 *   - the halves (small text) or quarters (2x text) of a wide glyph, and the quarters of a 2x non-ASCII letter.
 * A full pool frees the slots the UI layer no longer shows (a scan of its map). hu_clear() empties it.
 * Layout errors (text off the screen or over a panel's edge, an overhang over other text, a glyph no font has, a full
 * pool) are counted and logged once each with "i18n:" (the language tests fail on them). */
enum { K_BODY = 1, K_OVER, K_WIDE, K_BIG };                 /* key kinds */
enum { ST_FREE, ST_BOX, ST_BIG_FREE, ST_BIG_BANNER };       /* styles */
enum { UNDER_BLANK, UNDER_FILL };                           /* what an overhang covers */
#define KEY(kind, style, part, under, cp) ((uint64_t)(kind) << 56 | (uint64_t)(style) << 52 | (uint64_t)(part) << 40 | \
                                           (uint64_t)(under) << 36 | (uint64_t)(cp))
#define KEY_KIND(k)  ((int)((k) >> 56))
#define KEY_STYLE(k) ((int)((k) >> 52) & 15)

const char *hu_str(int id)
{
    const rs_i18n_pack *p = rs_i18n();
    if (p && p->kit) return rs_tr(p->kit, id);
    return id >= 0 && id < HS_COUNT && hs_english[id] ? hs_english[id] : "";
}

const char *hu_strn(int id, int n)
{
    const rs_i18n_pack *p = rs_i18n();
    if (p && p->kit) return rs_trn(p->kit, id, n);
    const char *s = id >= 0 && id + 1 < HS_COUNT ? hs_english[id + (n == 1 ? 1 : 0)] : NULL;
    return s ? s : hu_str(id);
}

int hu_text_errors(void) { return err_total; }

static void text_error(const char *what, int x, int y, const char *s)
{
    uint32_t h = 2166136261u;
    for (const char *p = what; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    for (const char *p = s; p && *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    h ^= (uint32_t)(x * 131 + y * 7919);
    err_total++;
    for (int i = 0; i < err_n; i++)
        if (err_seen[i] == h) return;
    if (err_n < (int)(sizeof err_seen / sizeof err_seen[0])) err_seen[err_n++] = h;
    rs_log("i18n: %s at %d,%d (%s, %d px): \"%s\"", what, x, y, rs_lang_code(rs_lang()), rs_screen_width(), s ? s : "");
}

static int is_ascii(const char *s)
{
    for (; *s; s++)
        if ((unsigned char)*s >= 128) return 0;
    return 1;
}

int hu_width(const char *s, int big)
{
    if (!big) return rs_text_cols(s);
    int n = 0;
    while (rs_utf8_next(&s)) n++;
    return 2 * n;
}

/* the pool ------------------------------------------------------------------------------------------------------------ */
static int ext_tile_of(int slot) { return cfg.ext_tile + slot; }
static int ext_slot_of(uint16_t e)
{
    int t = RS_MAP_TILE(e) - cfg.ext_tile;
    return cfg.ext_count > 0 && t >= 0 && t < cfg.ext_count && t < EXT_MAX ? t : -1;
}

static void ext_gc(void)
{
    uint8_t used[EXT_MAX];
    memset(used, 0, sizeof used);
    const uint16_t *m = rs_bg_map(cfg.layer);
    int n = rs_bg_map_w(cfg.layer) * rs_bg_map_h(cfg.layer);
    for (int i = 0; m && i < n; i++) {
        int s = ext_slot_of(m[i]);
        if (s >= 0) used[s] = 1;
    }
    for (int s = 0; s < cfg.ext_count && s < EXT_MAX; s++)
        if (!used[s]) ext_key[s] = 0;
}

/* the slot holding key (made = 1 when it was just allocated: draw it), -1 when the pool is full */
static int ext_get(uint64_t key, int *made)
{
    int lim = cfg.ext_count < EXT_MAX ? cfg.ext_count : EXT_MAX, free_s = -1;
    *made = 0;
    for (int s = 0; s < lim; s++) {
        if (ext_key[s] == key) return s;
        if (!ext_key[s] && free_s < 0) free_s = s;
    }
    if (free_s < 0) {
        ext_gc();
        for (int s = 0; s < lim && free_s < 0; s++)
            if (!ext_key[s]) free_s = s;
    }
    if (free_s < 0) return -1;
    ext_key[free_s] = key;
    *made = 1;
    int n = 0;
    for (int s = 0; s < lim; s++) n += ext_key[s] != 0;
    if (n > ext_peak) {
        ext_peak = n;
        if (n % 16 == 0) rs_log("[i18n] glyph pool: %d of %d tiles in use (%s)", n, lim, rs_lang_code(rs_lang()));
    }
    return free_s;
}

/* pixels: an 8x8 tile from rows (font bit order) with the house shadow, on bg */
static void rows_tile(const uint8_t rows[8], int bg, uint8_t t[64])
{
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int on = (rows[y] >> (7 - x)) & 1;
            int sh = x > 0 && y > 0 && ((rows[y - 1] >> (8 - x)) & 1);
            t[y * 8 + x] = (uint8_t)(on ? HU_CREAM : sh ? HU_OUTLINE : bg);
        }
}

/* a 16x16 wide glyph (32 bytes, bit 15 = left) as a canvas: 1 ink, 2 shadow (small) or outline (big styles) */
static void wide_canvas(const uint8_t *g, int big, uint8_t px[16][16])
{
    memset(px, 0, 256);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            if (g[y * 2 + (x >> 3)] & (0x80 >> (x & 7))) px[y][x] = 1;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            if (px[y][x]) continue;
            if (!big) {
                if (x > 0 && y > 0 && px[y - 1][x - 1] == 1) px[y][x] = 2;
                continue;
            }
            for (int dy = -1; dy <= 1 && !px[y][x]; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int yy = y + dy, xx = x + dx;
                    if (yy >= 0 && yy < 16 && xx >= 0 && xx < 16 && px[yy][xx] == 1) { px[y][x] = 2; break; }
                }
        }
}

/* a 2x glyph canvas from 8 font rows (the 2x font: 1 ink, 2 outline, 3 drop shadow for the banner); ox, oy: where the
 * glyph's pixel (0, 0) goes */
static void big_canvas(const uint8_t rows[8], int nrows, int oy, int banner, uint8_t px[16][16])
{
    memset(px, 0, 256);
    for (int y = 0; y < nrows; y++)
        for (int x = 0; x < 6; x++)
            if ((rows[y] >> (7 - x)) & 1)
                for (int k = 0; k < 4; k++) {
                    int Y = oy + y * 2 + (k >> 1), X = 1 + x * 2 + (k & 1);
                    if (Y >= 0 && Y < 16) px[Y][X] = 1;
                }
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            if (px[y][x]) continue;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int yy = y + dy, xx = x + dx;
                    if (yy >= 0 && yy < 16 && xx >= 0 && xx < 16 && px[yy][xx] == 1) px[y][x] = 2;
                }
        }
    if (banner)
        for (int y = 15; y > 0; y--)
            for (int x = 15; x > 0; x--)
                if (!px[y][x] && px[y - 1][x - 1] == 2) px[y][x] = 3;
}

static const uint8_t big_cols[2][4] = {{0, HU_CREAM, HU_OUTLINE, 0}, {HU_FILL, HU_GOLD, HU_OUTLINE, HU_DARK}};

/* what lies under an overhang at (x, y): UNDER_BLANK / UNDER_FILL, or -1 (other text, a frame, a picture) */
static int under_kind(uint16_t e)
{
    int s = ext_slot_of(e);
    if (s >= 0 && KEY_KIND(ext_key[s]) == K_OVER) e = ext_under[s];
    int t = RS_MAP_TILE(e) - cfg.tile_base;
    if (t == T_FONT) return UNDER_BLANK;
    if (t == T_BOX || t == T_PANEL + 4) return UNDER_FILL;
    return -1;
}

/* takes away the overhang at (x, y) (the text below it changes) */
static void overhang_clear(int x, int y)
{
    if (y < 0 || !ext_used) return;
    uint16_t e = rs_bg_get(cfg.layer, ui_col(x), y);
    int s = ext_slot_of(e);
    if (s >= 0 && KEY_KIND(ext_key[s]) == K_OVER) rs_bg_put(cfg.layer, ui_col(x), y, ext_under[s]);
}

/* draws the overhang part (a mark, or a wide glyph's top-left / top-right) over the cell (x, y) */
static void overhang(int x, int y, int part, uint32_t cp, int style, const char *s)
{
    if (y < 0) { text_error("no room above the text for its accent", x, y, s); return; }
    uint16_t e = rs_bg_get(cfg.layer, ui_col(x), y);
    int under = under_kind(e);
    if (under < 0) { text_error("an accent or a tall glyph over other text", x, y, s); return; }
    int sl = ext_slot_of(e);
    uint16_t below = sl >= 0 && KEY_KIND(ext_key[sl]) == K_OVER ? ext_under[sl] : e;
    int made, slot = ext_get(KEY(K_OVER, style, part, under, cp), &made);
    if (slot < 0) { text_error("the glyph pool is full", x, y, s); return; }
    if (made) {
        uint8_t t[64];
        int bg = under == UNDER_FILL ? HU_FILL : 0;
        if (part < 16) {                                    /* a mark: cp is the mark */
            const uint8_t *m = rs_font_mark((int)cp);
            if (style >= ST_BIG_FREE) {
                /* 2x: the mark's rows 4..6 as rows 1..6, outlined */
                uint8_t r[8] = {m[4], m[5], m[6], 0, 0, 0, 0, 0}, px[16][16];
                big_canvas(r, 3, 2, style == ST_BIG_BANNER, px);
                for (int yy = 0; yy < 8; yy++)
                    for (int xx = 0; xx < 8; xx++) {
                        int v = px[yy][(part & 1) * 8 + xx];
                        t[yy * 8 + xx] = v ? big_cols[style == ST_BIG_BANNER][v] : (uint8_t)bg;
                    }
            } else {
                rows_tile(m, bg, t);
            }
        } else {                                            /* the top half of a wide glyph: part 16 left, 17 right */
            const uint8_t *g = rs_wide_glyph(cp);
            uint8_t px[16][16];
            wide_canvas(g, 0, px);
            for (int yy = 0; yy < 8; yy++)
                for (int xx = 0; xx < 8; xx++) {
                    int v = px[yy][(part & 1) * 8 + xx];
                    t[yy * 8 + xx] = (uint8_t)(v == 1 ? HU_CREAM : v == 2 ? HU_OUTLINE : bg);
                }
        }
        rs_tiles_load8(cfg.vram_base + ext_tile_of(slot), t, 1);
    }
    ext_under[slot] = below;
    ext_used = 1;
    rs_bg_put(cfg.layer, ui_col(x), y, bg_entry(ext_tile_of(slot) - cfg.tile_base, 1));
}

/* checks the cell a character goes to: on the screen; box text: on a panel's inside */
static void check_cell(int x, int y, int box, const char *s)
{
    int c0 = fdiv8(hu_ui_left()), c1 = fdiv8(hu_ui_right() + 7);
    if (x < c0 || x >= c1 || y < 0 || y >= 30) { text_error("text off the screen", x, y, s); return; }
    if (!box) return;
    uint16_t e = rs_bg_get(cfg.layer, ui_col(x), y);
    int t = RS_MAP_TILE(e) - cfg.tile_base, sl = ext_slot_of(e);
    if (sl >= 0 ? KEY_STYLE(ext_key[sl]) == ST_BOX : (t >= T_BOX && t < T_BOX + 96) || t == T_PANEL + 4) return;
    text_error("text over a panel's edge", x, y, s);
}

/* small text, UTF-8: the cells x.. of row y (box: on the panel fill) */
static void put_text(int x, int y, const char *s, int box)
{
    const char *p = s;
    uint32_t cp;
    int style = box ? ST_BOX : ST_FREE, bg = box ? HU_FILL : 0;
    while ((cp = rs_utf8_next(&p)) != 0) {
        check_cell(x, y, box, s);
        overhang_clear(x, y - 1);
        if (cp < 128) {
            int g = cp >= 32 ? (int)cp - 32 : '?' - 32;
            if (box) box_glyph(g); else font_glyph(g);
            rs_bg_put(cfg.layer, ui_col(x), y, bg_entry((box ? T_BOX : T_FONT) + g, 1));
            x++;
            continue;
        }
        if (rs_glyph_wide(cp)) {
            const uint8_t *g = rs_wide_glyph(cp);
            check_cell(x + 1, y, box, s);
            overhang_clear(x + 1, y - 1);
            if (!g) {
                text_error("no glyph for a character", x, y, s);
                for (int k = 0; k < 2; k++) {
                    if (box) box_glyph('?' - 32); else font_glyph('?' - 32);
                    rs_bg_put(cfg.layer, ui_col(x + k), y, bg_entry((box ? T_BOX : T_FONT) + '?' - 32, 1));
                }
                x += 2;
                continue;
            }
            for (int half = 0; half < 2; half++) {
                int made, slot = ext_get(KEY(K_WIDE, style, half, 0, cp), &made);
                if (slot < 0) { text_error("the glyph pool is full", x + half, y, s); continue; }
                if (made) {
                    uint8_t px[16][16], t[64];
                    wide_canvas(g, 0, px);
                    for (int yy = 0; yy < 8; yy++)
                        for (int xx = 0; xx < 8; xx++) {
                            int v = px[8 + yy][half * 8 + xx];
                            t[yy * 8 + xx] = (uint8_t)(v == 1 ? HU_CREAM : v == 2 ? HU_OUTLINE : bg);
                        }
                    rs_tiles_load8(cfg.vram_base + ext_tile_of(slot), t, 1);
                }
                ext_used = 1;
                rs_bg_put(cfg.layer, ui_col(x + half), y, bg_entry(ext_tile_of(slot) - cfg.tile_base, 1));
                overhang(x + half, y - 1, 16 + half, cp, style, s);
            }
            x += 2;
            continue;
        }
        const uint8_t *body;
        int above, below;
        if (!rs_font_ext(cp, &body, &above, &below)) {
            text_error("no glyph for a character", x, y, s);
            body = rs_font_5x7['?' - 32];
            above = below = 0;
        }
        int ascii = body >= rs_font_5x7[0] && body < rs_font_5x7[96];
        if (ascii && !below) {
            int g = (int)(body - rs_font_5x7[0]) / 8;
            if (box) box_glyph(g); else font_glyph(g);
            rs_bg_put(cfg.layer, ui_col(x), y, bg_entry((box ? T_BOX : T_FONT) + g, 1));
        } else {
            int made, slot = ext_get(KEY(K_BODY, style, 0, 0, cp), &made);
            if (slot < 0) text_error("the glyph pool is full", x, y, s);
            else {
                if (made) {
                    uint8_t r[8], t[64];
                    memcpy(r, body, 8);
                    if (below) r[7] |= rs_font_mark(below)[7];
                    rows_tile(r, bg, t);
                    rs_tiles_load8(cfg.vram_base + ext_tile_of(slot), t, 1);
                }
                ext_used = 1;
                rs_bg_put(cfg.layer, ui_col(x), y, bg_entry(ext_tile_of(slot) - cfg.tile_base, 1));
            }
        }
        if (above) overhang(x, y - 1, 0, (uint32_t)above, style, s);
        x++;
    }
}

/* 2x text, UTF-8: non-ASCII characters (2 x 2 tiles each) from the pool */
static void put_big(int x, int y, uint32_t cp, int style, const char *s)
{
    int banner = style == HU_BIG_BANNER, st = banner ? ST_BIG_BANNER : ST_BIG_FREE;
    uint8_t px[16][16];
    int above = 0;
    const uint8_t *wg = rs_glyph_wide(cp) ? rs_wide_glyph(cp) : NULL;
    if (wg) {
        wide_canvas(wg, 1, px);
        if (banner)
            for (int yy = 15; yy > 0; yy--)
                for (int xx = 15; xx > 0; xx--)
                    if (!px[yy][xx] && px[yy - 1][xx - 1] == 2) px[yy][xx] = 3;
    } else {
        const uint8_t *body;
        int below;
        if (!rs_font_ext(cp, &body, &above, &below)) {
            text_error("no glyph for a character", x, y, s);
            body = rs_font_5x7['?' - 32];
            above = below = 0;
        }
        uint8_t r[8];
        memcpy(r, body, 8);
        if (below) r[7] |= rs_font_mark(below)[7];
        big_canvas(r, below ? 8 : 7, 1, banner, px);
    }
    for (int q = 0; q < 4; q++) {
        int made, slot = ext_get(KEY(wg ? K_WIDE : K_BIG, st, 2 + q, 0, cp), &made);
        if (slot < 0) { text_error("the glyph pool is full", x, y, s); return; }
        if (made) {
            uint8_t t[64];
            for (int yy = 0; yy < 8; yy++)
                for (int xx = 0; xx < 8; xx++) t[yy * 8 + xx] = big_cols[banner][px[(q >> 1) * 8 + yy][(q & 1) * 8 + xx]];
            rs_tiles_load8(cfg.vram_base + ext_tile_of(slot), t, 1);
        }
        ext_used = 1;
        rs_bg_put(cfg.layer, ui_col(x + (q & 1)), y + (q >> 1), bg_entry(ext_tile_of(slot) - cfg.tile_base, 1));
    }
    if (above)
        for (int half = 0; half < 2; half++) overhang(x + half, y - 1, half, (uint32_t)above, st, s);
}


static void check_text(int x, int y, const char *s, int box)
{
    for (int i = 0; s[i]; i++) check_cell(x + i, y, box, s);
}

void hu_text(int x, int y, const char *s)
{
    rs_text_setup(cfg.layer, cfg.tile_base + T_FONT, cfg.pal, 1);
    if (!is_ascii(s) || ext_used) { put_text(x, y, s, 0); return; }
    check_text(x, y, s, 0);
    for (const char *p = s; *p; p++) font_glyph((unsigned char)*p - 32);
    rs_text(x, y, s);
}

void hu_box_text(int x, int y, const char *s)
{
    rs_text_setup(cfg.layer, cfg.tile_base + T_BOX, cfg.pal, 1);
    if (!is_ascii(s) || ext_used) { put_text(x, y, s, 1); return; }
    check_text(x, y, s, 1);
    for (const char *p = s; *p; p++) box_glyph((unsigned char)*p - 32);
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
    const char *p = s;
    uint32_t cp;
    for (; (cp = rs_utf8_next(&p)) != 0; x += 2) {
        if (cp == ' ') continue;
        check_cell(x, y, 0, s);
        check_cell(x + 1, y + 1, 0, s);
        if (cp >= 128) { put_big(x, y, cp, style, s); continue; }
        int t = T_BIG + big_slot((char)cp, style) * 4;
        for (int q = 0; q < 4; q++) rs_bg_put(cfg.layer, x + (q & 1), y + (q >> 1), bg_entry(t + q, 1));
    }
}

int hu_center(const char *s, int big) { return (40 - hu_width(s, big)) / 2; }

void hu_clear(void)
{
    rs_bg_fill(cfg.layer, RS_MAP(cfg.tile_base + T_FONT, 0, 0, 0, 0));
    big_used = 0;
    memset(ext_key, 0, sizeof ext_key);
    ext_used = 0;
}

void hu_clear_rows(int y, int h)
{
    rs_text_setup(cfg.layer, cfg.tile_base + T_FONT, cfg.pal, 1);
    int c0 = fdiv8(hu_ui_left()), c1 = fdiv8(hu_ui_right() + 7);
    if (ext_used)                                       /* the accents and tall glyphs of row y, in the row above */
        for (int c = c0; c < c1; c++) overhang_clear(c, y - 1);
    if (rs_screen_width() == RS_SCREEN_W) { rs_text_clear(0, y, 40, h); return; }
    /* widescreen: every column the screen shows (the corners' tags may be outside 0..39) */
    for (int j = 0; j < h; j++)
        for (int c = c0; c < c1; c++) rs_bg_put(cfg.layer, ui_col(c), y + j, RS_MAP(cfg.tile_base + T_FONT, cfg.pal, 0, 0, 0));
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

/* does the text have a mark above a letter (an accent: it takes the row above)? */
static int has_accent(const char *s)
{
    uint32_t cp;
    int above = 0;
    while ((cp = rs_utf8_next(&s)) != 0)
        if (cp >= 128 && !rs_glyph_wide(cp) && rs_font_ext(cp, NULL, &above, NULL) && above) return 1;
    return 0;
}

int hu_tall(const char *s)
{
    const char *p = s;
    uint32_t cp;
    while ((cp = rs_utf8_next(&p)) != 0)
        if (cp >= 128 && rs_glyph_wide(cp)) return 1;
    return has_accent(s);
}

void hu_banner(int y, const char *title)
{
    int w = hu_width(title, 1) + 2, acc = has_accent(title);
    if (w < 20) w = 20;
    if (w > 40) text_error("a banner wider than the screen", 0, y, title);
    hu_panel((40 - w) / 2, y - acc, w, 4 + acc);        /* accents: one more row on top, for them */
    hu_big(hu_center(title, 1), y + 1, title, HU_BIG_BANNER);
}

void hu_get_ready(int y) { hu_big(hu_center(hu_str(HS_GET_READY), 1), y, hu_str(HS_GET_READY), HU_BIG_FREE); }

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
    char s[128];
    if (players >= 2) {
        rs_strf(s, sizeof s, hu_str(HS_JOINED), joined ? joined : hu_str(HS_GO));
        hu_clear_rows(y, 1);
        hu_text(hu_center(s, 0), y, s);
    } else {
        hu_text(hu_center(hu_str(HS_JOIN_PAD2), 0), y, hu_str(HS_JOIN_PAD2));
    }
}

void hu_gameover_panel(int players, int new_best, int score1, int score2)
{
    /* the labels on the left, the values (sprites) centred on column 25: longer labels widen the panel leftwards */
    char a[64], b[64], win[96];
    const char *c = "";
    if (players == 1) {
        snprintf(a, sizeof a, "%s", hu_str(HS_SCORE));
        snprintf(b, sizeof b, "%s", hu_str(new_best ? HS_NEW_BEST : HS_BEST));
        c = hu_str(HS_MEDAL);
    } else {
        rs_strf(a, sizeof a, hu_str(HS_PLAYER_N), 1);
        rs_strf(b, sizeof b, hu_str(HS_PLAYER_N), 2);
    }
    int l = hu_width(a, 0);
    if (hu_width(b, 0) > l) l = hu_width(b, 0);
    if (hu_width(c, 0) > l) l = hu_width(c, 0);
    int d = l > 10 ? (l - 10 < 10 ? l - 10 : 10) : 0;
    go_grow = (int8_t)d;
    hu_panel(10 - d, 9, 20 + d, 12);
    hu_box_text(12 - d, 11, a);
    hu_box_text(12 - d, 14, b);
    if (players == 1) {
        hu_box_text(12 - d, 17, c);
    } else {
        if (score1 != score2) rs_strf(win, sizeof win, hu_str(HS_WINS), score1 > score2 ? 1 : 2);
        else snprintf(win, sizeof win, "%s", hu_str(HS_DRAW));
        hu_box_text(20 - hu_width(win, 0) / 2, 17, win);
    }
}

void hu_gameover_note(const char *s)
{
    /* the panel goes on 2 rows lower (rows 19..22 drawn again: its sides, its bottom), the line on row 21 */
    int x0 = 10 - go_grow, w = 20 + go_grow;
    for (int y = 19; y <= 22; y++)
        for (int x = 0; x < w; x++) {
            int kx = x == 0 ? 0 : x == w - 1 ? 2 : 1, ky = y == 22 ? 2 : 1;
            rs_bg_put(cfg.layer, x0 + x, y, bg_entry(T_PANEL + ky * 3 + kx, 1));
        }
    hu_box_text(x0 + (w - hu_width(s, 0)) / 2, 21, s);
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
    int n = hu_width(text, 0);
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
        if (paused) hu_big(hu_center(hu_str(HS_PAUSED), 1), y, hu_str(HS_PAUSED), HU_BIG_FREE);
        else hu_clear_rows(y, 2);
    }
}

/* ---- sprites -------------------------------------------------------------------------------------------------------------- */
static void kit_spr(int rel, int x, int y, int w, int h, int prio)
{
    x += hu_ui_x();                                     /* UI coordinates -> stage */
    if (cfg.obj_tile < 0 || x <= rs_screen_left() - w || x >= rs_screen_right() || y <= -h || y >= RS_SCREEN_H) return;
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
    uint8_t unique[HU_LOGO_TILES][64];
    for (int tyy = 0; tyy < LOGO_H / 8; tyy++)
        for (int tx = 0; tx < LOGO_W / 8; tx++) {
            uint8_t t[64];
            int any = 0;
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) any |= (t[y * 8 + x] = logo_px[tyy * 8 + y][tx * 8 + x]);
            uint16_t e = (uint16_t)RS_MAP(cfg.tile_base + T_FONT, 0, 0, 0, 0);
            if (any && used < HU_LOGO_TILES) {
                int k = 0;
                while (k < used && memcmp(unique[k], t, sizeof t)) k++;
                if (k == used) {
                    memcpy(unique[used], t, sizeof t);
                    rs_tiles_load8(cfg.vram_base + cfg.logo_tile + used, t, 1);
                    used++;
                }
                e = (uint16_t)RS_MAP(cfg.logo_tile + k, cfg.logo_pal, 1, 0, 0);
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

/* ---- players and the title ----------------------------------------------------------------------------------------------- */
static int clampi_(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void hu_title_setup(const hu_title_cfg *c)
{
    /* A ALWAYS starts the game (and the prompt always reads PRESS A TO <VERB>, with the A glyph); the game may add
     * its natural inputs (the D-pad, B...) to cfg.start. */
    lobby.start = (uint16_t)((c->start ? c->start : RS_BTN_START) | RS_BTN_A);
    lobby.max = (uint8_t)clampi_(c->max_players, 1, HU_MAX_PLAYERS);
    if (lobby.n > lobby.max) lobby.n = lobby.max;
    lobby.slot_row = (uint8_t)(c->slot_row > 0 ? c->slot_row : HU_ROW_SLOTS);
    lobby.glyph = HU_BTN_A;
    rs_strf(lobby.prompt, sizeof lobby.prompt, hu_str(HS_PRESS_TO), c->verb ? c->verb : hu_str(HS_PLAY));
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

/* "P2 / P3 / P4": the free players' tags */
static void free_tags(char *who, size_t size)
{
    who[0] = 0;
    for (int p = lobby.n; p < lobby.max; p++) {
        size_t k = strlen(who);
        if (p > lobby.n) k += (size_t)snprintf(who + k, size - k, " / ");
        if (k < size) rs_strf(who + k, size - k, hu_str(HS_PLAYER_TAG), p + 1);
    }
}

void hu_title_draw(int t, int best)
{
    char s[160];
    lobby.glyph_x = (int16_t)hu_prompt(HU_ROW_PROMPT, lobby.prompt, t);
    hu_clear_rows(HU_ROW_BEST, 1);
    if (best > 0) {
        rs_strf(s, sizeof s, hu_str(HS_BEST_N), best);
        hu_text(hu_center(s, 0), HU_ROW_BEST, s);
    }
    if (lobby.max > 1) {
        hu_clear_rows(lobby.slot_row, 1);
        for (int p = 0; p < lobby.n; p++) {
            rs_strf(s, sizeof s, hu_str(HS_PLAYER_TAG), p + 1);
            hu_text(4 + 9 * p, lobby.slot_row, s);
        }
        hu_clear_rows(HU_ROW_JOIN, 1);
        if (lobby.n < lobby.max) {
            char who[64];
            free_tags(who, sizeof who);
            rs_strf(s, sizeof s, hu_str(lobby.n > 1 ? HS_JOIN_LEAVE : HS_JOIN), who);
        } else {
            rs_strf(s, sizeof s, hu_strn(HS_FULL, lobby.n), lobby.n);
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
    /* UI coordinates; widescreen: the halves and the corners of the actual screen (320: x 80 / 240, tags 1 / 37) */
    int sw = rs_screen_width(), m = rs_screen_margin();
    hu_chip c = {RS_SCREEN_W / 2, 10, 0, -1, -1};
    if (bottom_y <= 0) bottom_y = 220;
    if (players == 2) c.x = p ? 3 * sw / 4 - m : sw / 4 - m;
    else if (players >= 3) {
        c.y = p >= 2 ? bottom_y : 4;
        c.tag_y = (c.y + 4) / 8;
        if (p & 1) { c.tag_x = fdiv8(sw - 24 - m); c.x = c.tag_x * 8 - 4; c.align = 1; }
        else { c.tag_x = fdiv8(8 - m); c.x = c.tag_x * 8 + 20; c.align = -1; }
    }
    return c;
}

void hu_score_tags(int players, int bottom_y)
{
    char s[12];
    for (int p = 0; p < players && p < HU_MAX_PLAYERS; p++) {
        hu_chip c = hu_score_chip(players, p, bottom_y);
        if (c.tag_y < 0) continue;                      /* no tag (1-2 players); tag_x may be negative (widescreen) */
        rs_strf(s, sizeof s, hu_str(HS_PLAYER_TAG), p + 1);
        if (c.tag_x >= 0 && c.tag_x + 2 <= 40) hu_text(c.tag_x, c.tag_y, s);
        else ui_text_wrapped(c.tag_x, c.tag_y, s);
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
    /* the whole screen in stage coordinates (widescreen: wider columns) */
    int sw = rs_screen_width();
    int w = (sw - RS_VIEW_DIVIDER * (n - 1)) / n;
    int x0 = rs_screen_left() + (sw - n * w - RS_VIEW_DIVIDER * (n - 1)) / 2;
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
    char t[96];
    if (!title) {
        int w = hu_winner(s);
        if (w >= 0) rs_strf(t, sizeof t, hu_str(HS_WINS), w + 1);
        else snprintf(t, sizeof t, "%s", hu_str(HS_DRAW));
        title = t;
    }
    hu_banner(3, title);
    hu_panel(8, 8, 24, 3 * s->n + 4);
    for (int i = 0; i < s->n; i++) {
        char tag[16];
        int row = hu_results_row(s, i);
        hu_box_text(10, row, hu_str(HS_RANK_1 + (s->rank[i] & 3)));
        rs_strf(tag, sizeof tag, hu_str(HS_PLAYER_TAG), (s->order[i] & 3) + 1);
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

int hu_over_back(void)
{
    for (int p = 0; p < lobby.n; p++)
        if (rs_pad_pressed(lobby.pad[p]) & (lobby.start | RS_BTN_A | RS_BTN_START | RS_BTN_SELECT)) return 1;
    return 0;
}
