/*
 * RetroStone Arcade: the launcher, itself a RetroStone VC program (the house style of games/common).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The screen (W = rsb_width(), 320..560; 240 high):
 *   header  rows 0-3: a panel with the app's name in big gold letters
 *   grid    the cards of every game (rsb_games): its title screen, its name, its best score; the locked games
 *           (the paid app only) dimmed, with a padlock. 3 columns at 320, up to 5 on a wide screen; the rows
 *           scroll (D-pad, or drag / swipe)
 *   footer  rows 27-29: the A glyph and the selected game, and the "FULL VERSION" pill (one, small)
 *   panel   "Get the full version": a locked game's picture, the paid app's name, the Google Play button
 * Input: D-pad (repeat) + A (play / open the panel) + B (close); touch: tap a card = select and launch, drag
 * scrolls, tap the pill or the panel's buttons; Back closes the panel, else leaves the app.
 *
 * Video: BG1 the UI (house kit, high priority); the card rows on BG2, BG3, BG4 (row r on layer r % 3, each layer
 * scrolled to its row: at most 3 rows are visible); the raster callback loads the palettes of the row under each
 * line (one palette per column: BG 1..5) and the backdrop gradient. Sprites: the gold frame of the selection, the
 * padlocks, the kit's A glyph.
 * The launcher's statics are NOT reset between runs (it is not a module): the selection and the scroll stay.
 */
#include "rsb.h"
#include "house_ui.h"
#include "house_audio.h"
#include "game_text.h"                  /* the launcher's strings: apps/launcher/strings (TR, TRN) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t rs_font_5x7[96][8];

#define HEAD_ROWS   4
#define FOOT_ROWS   4                       /* rim, a free row (accents, CJK), the text, rim */
#define FOOT_TEXT_ROW 28
#define GRID_TOP    (HEAD_ROWS * 8)
#define GRID_BOTTOM (RS_SCREEN_H - FOOT_ROWS * 8)
#define PITCH_X     (RSB_CARD_W + 8)
#define PITCH_Y     (RSB_CARD_H + 8)
#define MAX_COLS    5

/* VRAM (tiles): BG1 the kit at 0 (HU_BG_TILES) and the pill at T_PILL; card layer l (0..2 = BG2..BG4) at
 * VR_CARD(l): a blank tile, the badge glyphs, the cards (or the panel's picture on layer 0); sprites at VR_OBJ */
#define VR_BG1      0
#define T_PILL      320
#define VR_CARD(l)  (1024 * ((l) + 1))
#define T_BADGE     1
#define T_CARDS     32
#define VR_OBJ      3840
#define OT_KIT      0
#define OT_CORNER   128
#define OT_HEDGE    129                     /* 32x8: 4 tiles */
#define OT_VEDGE    133                     /* 8x32: 4 tiles */
#define OT_LOCK     137                     /* 16x16: 4 tiles */
#define PAL_UI      0
#define PAL_PILL    7
#define OPAL_MINE   2

enum { SEL_PILL = -2 };

/* ---- state (kept between runs) --------------------------------------------------------------------------------- */
static int W, Wt, cols, nrows, grid_x;      /* layout of this run */
static int sel = 0, sel_mem = 0;            /* the selected card (or SEL_PILL), the card before the pill */
static int scroll_q8, vel_q8, target = -1;  /* the grid's scroll (1/256 px), its fling speed, a goal for the D-pad */
static int layer_row[3];
static int panel, panel_game, panel_tour, panel_t;
static int t, rep_btn, rep_t;
static int dragging, drag_scroll, drag_last_y, moved;
static int best[64];
static int shown_sel = -99, shown_panel = -1, shown_panel_game = -1;

static int max_scroll(void)
{
    int h = nrows * PITCH_Y - 8 - (GRID_BOTTOM - GRID_TOP);
    return h > 0 ? h : 0;
}
static int scroll_px(void) { return scroll_q8 >> 8; }
static int locked(int g) { return rsb_games[g].module == NULL; }

/* ---- tiles drawn here ------------------------------------------------------------------------------------------------ */
static void tile8(int vram_tile, const uint8_t px[64]) { rs_tiles_load8(vram_tile, px, 1); }

/* a glyph of the house font on a fill colour (the badges: fill 3 navy, ink 1 cream; the pill: its palette) */
static void glyph_on(int vram_tile, int ch, int fill, int ink, int shadow)
{
    uint8_t px[64];
    int c = ch - 32;
    if (c < 0 || c >= 96) c = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int on = rs_font_5x7[c][y] & (0x80 >> x);
            int sh = shadow && x > 0 && y > 0 && (rs_font_5x7[c][y - 1] & (0x80 >> (x - 1)));
            px[y * 8 + x] = (uint8_t)(on ? ink : sh ? shadow : fill);
        }
    tile8(vram_tile, px);
}

/* the star of the best-score badge (gold on the navy fill) */
static const char *STAR[8] = {"   1    ", "   1    ", "  111   ", "1111111 ", " 11111  ", "  111   ", " 11 11  ", " 1   1  "};

static void badge_tiles(int layer)
{
    uint8_t px[64];
    int base = VR_CARD(layer);
    memset(px, 0, sizeof px);
    tile8(base, px);                                            /* tile 0: blank */
    for (int d = 0; d < 10; d++) glyph_on(base + T_BADGE + d, '0' + d, HU_FILL, HU_CREAM, 0);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) px[y * 8 + x] = (uint8_t)(STAR[y][x] == '1' ? HU_GOLD : HU_FILL);
    tile8(base + T_BADGE + 10, px);
}

/* the padlock and the selection frame (OBJ palette 2) */
enum { C_OUT = 1, C_SIL = 2, C_SILD = 3, C_GOLDL = 4, C_GOLD = 5, C_GOLDD = 6 };
static const rs_color MINE_PAL[16] = {
    0, RS_RGB8(22, 18, 40), RS_RGB8(222, 230, 238), RS_RGB8(128, 138, 156),
    RS_RGB8(255, 240, 150), RS_RGB8(252, 216, 72), RS_RGB8(186, 128, 22)};
static const char *LOCK[16] = {
    "................", ".....oooooo.....", "....oSSSSSSo....", "...oSSooooSSo...",
    "...oSo....oTo...", "...oSo....oTo...", "...oSo....oTo...", ".oooooooooooooo.",
    ".oHHHHHHHHHHHGo.", ".oHGGGGGGGGGGDo.", ".oHGGGGooGGGGDo.", ".oHGGGGooGGGGDo.",
    ".oHGGGGGoGGGGDo.", ".oGDDDDDDDDDDDo.", ".oooooooooooooo.", "................"};

static int lock_px(char c)
{
    switch (c) {
    case 'o': return C_OUT;
    case 'S': return C_SIL;
    case 'T': return C_SILD;
    case 'H': return C_GOLDL;
    case 'G': return C_GOLD;
    case 'D': return C_GOLDD;
    default: return 0;
    }
}

static void sprite_tiles(void)
{
    uint8_t px[64];
    /* the lock: 16x16 = tiles TL, TR, BL, BR */
    for (int q = 0; q < 4; q++) {
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) px[y * 8 + x] = (uint8_t)lock_px(LOCK[(q >> 1) * 8 + y][(q & 1) * 8 + x]);
        tile8(VR_OBJ + OT_LOCK + q, px);
    }
    /* the frame, 4 px thick outside the card: outline, light gold, gold, dark gold (outside to inside) */
    static const int band[4] = {C_OUT, C_GOLDL, C_GOLD, C_GOLDD};
    for (int y = 0; y < 8; y++)                                  /* top-left corner */
        for (int x = 0; x < 8; x++) {
            int v = 0;
            int d = x < y ? x : y;                               /* distance to the outer edge */
            if (x < 4 || y < 4) v = band[d < 4 ? d : 3];
            if (x + y < 2) v = 0;                                /* rounded */
            else if (x + y == 2) v = C_OUT;
            if (x >= 4 && y >= 4) v = 0;
            px[y * 8 + x] = (uint8_t)v;
        }
    tile8(VR_OBJ + OT_CORNER, px);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) px[y * 8 + x] = (uint8_t)(y < 4 ? band[y] : 0);
    for (int k = 0; k < 4; k++) tile8(VR_OBJ + OT_HEDGE + k, px);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) px[y * 8 + x] = (uint8_t)(x < 4 ? band[x] : 0);
    for (int k = 0; k < 4; k++) tile8(VR_OBJ + OT_VEDGE + k, px);
}

/* ---- the pills (the "FULL VERSION" banner, the Google Play button): the kit's text on the panel fill, its map entries
 * moved to their own palette (the UI palette with the fill gold and the ink navy; brighter when selected), round ends.
 * Any language: the text is the kit's (UTF-8, accents and CJK use the row above, which gets the pill too). */
static int pill_x, pill_w;                  /* the banner: tile column, width in tiles */

static void pill_tiles(void)
{
    uint8_t px[64];
    for (int side = 0; side < 3; side++) {                       /* the round ends: left with the star, right, left plain */
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                int xx = side == 1 ? 7 - x : x;
                int r = (y < 2 ? 2 - y : y > 5 ? y - 5 : 0);     /* rounded corners */
                int v = xx >= r ? HU_FILL : 0;
                if (!side && STAR[y][x] == '1' && x >= 1) v = HU_RED;
                px[y * 8 + x] = (uint8_t)v;
            }
        tile8(VR_BG1 + T_PILL + side, px);
    }
}

static void pill_colours(int on)
{
    int k = on ? 128 + (hu_blink(t) ? 64 : 0) : 0;
    rs_pal_load(RS_PAL_BG(PAL_PILL), hu_ui_palette, 16);
    rs_pal_set(RS_PAL_BG(PAL_PILL) + HU_FILL, hu_lerp_color(RS_RGB8(250, 210, 90), RS_RGB8(255, 246, 220), k));
    rs_pal_set(RS_PAL_BG(PAL_PILL) + HU_CREAM, RS_RGB8(16, 24, 56));
    rs_pal_set(RS_PAL_BG(PAL_PILL) + HU_OUTLINE, RS_RGB8(186, 128, 22));
    rs_pal_set(RS_PAL_BG(PAL_PILL) + HU_RED, RS_RGB8(200, 60, 40));
}

/* text s as a pill at (x, row): returns its width in tiles (the two ends included) */
static int pill_put(int x, int row, const char *s, int star)
{
    int w = hu_width(s, 0), tall = hu_tall(s);
    hu_box_text(x + 1, row, s);
    for (int r = row - tall; r <= row; r++) {
        for (int i = 1; i <= w; i++) {
            uint16_t e = rs_bg_get(RS_BG1, x + i, r);
            rs_bg_put(RS_BG1, x + i, r, (uint16_t)((e & ~(7 << 10)) | (PAL_PILL << 10)));
        }
        rs_bg_put(RS_BG1, x, r, RS_MAP(T_PILL + (star && r == row ? 0 : 2), PAL_PILL, 1, 0, 0));
        rs_bg_put(RS_BG1, x + w + 1, r, RS_MAP(T_PILL + 1, PAL_PILL, 1, 0, 0));
    }
    return w + 2;
}

static void pill_draw(int row)
{
    const char *s = TR(FULL_VERSION);
    pill_w = hu_width(s, 0) + 2;
    pill_x = Wt - 2 - pill_w;
    pill_put(pill_x, row, s, 1);
}

/* ---- the header, the footer and the panel (BG1) ----------------------------------------------------------------------- */
static void upper(char *out, size_t n, const char *s)
{
    size_t i = 0;
    for (; s[i] && i + 1 < n; i++) out[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    out[i] = 0;
}

static void text_at(int x, int y, const char *s) { hu_box_text(x, y, s); }
static int centre(const char *s, int big) { return (Wt - hu_width(s, big)) / 2; }

static void draw_header(void)
{
    char name[64];
    upper(name, sizeof name, rsb_app_name);
    /* the longest run of words that fits in big letters; "RETROSTONE ARCADE FREE" -> "RETROSTONE ARCADE" */
    while ((int)strlen(name) * 2 > Wt - 4) {
        char *sp = strrchr(name, ' ');
        if (!sp) { name[(Wt - 4) / 2] = 0; break; }
        *sp = 0;
    }
    hu_panel(0, 0, Wt, HEAD_ROWS);
    hu_big(centre(name, 1), 1, name, HU_BIG_BANNER);
}

static void draw_footer(void)
{
    int y = 30 - FOOT_ROWS;
    hu_panel(0, y, Wt, FOOT_ROWS);
    pill_draw(FOOT_TEXT_ROW);
    char line[96] = "";
    if (sel >= 0) upper(line, sizeof line, rsb_games[sel].name);
    else rs_strf(line, sizeof line, TRN(N_GAMES, rsb_ngames), rsb_ngames);
    int room = pill_x - 1 - 4;
    while (line[0] && hu_width(line, 0) > room) line[strlen(line) - 1] = 0;   /* (game names are ASCII) */
    text_at(4, FOOT_TEXT_ROW, line);
}

/* the panel: "Get the full version" with a locked game's picture (a hole in the panel shows BG2) */
static int pan_x, pan_y, pan_w, pan_h, shot_x, shot_y, btn_x, btn_y, btn_w, back_x, back_w;

static void panel_draw(void)
{
    const rsb_entry *e = &rsb_games[panel_game];
    char s[64], paid[48];
    /* rows (every text line has the row above it free: accents, CJK): 1 rim, 3-4 title, 6-20 picture, 21 name,
     * 23 "is in", 25 "N games", 27 the buttons, 28 rim */
    pan_w = Wt - 2 < 38 ? Wt - 2 : 38;
    pan_h = 28;
    pan_x = (Wt - pan_w) / 2;
    pan_y = 1;
    hu_clear();
    hu_panel(pan_x, pan_y, pan_w, pan_h);
    const char *title = TR(FULL_VERSION);
    if (!rs_lang_wide(rs_lang()) && hu_width(title, 1) <= pan_w - 2) hu_big(centre(title, 1), pan_y + 2, title, HU_BIG_BANNER);
    else text_at(centre(title, 0), pan_y + 3, title);
    shot_x = (Wt - RSB_SHOT_W / 8) / 2;
    shot_y = pan_y + 5;
    for (int y = 0; y < RSB_SHOT_H / 8; y++)                     /* the hole */
        for (int x = 0; x < RSB_SHOT_W / 8; x++) rs_bg_put(RS_BG1, shot_x + x, shot_y + y, RS_MAP(0, PAL_UI, 0, 0, 0));
    upper(s, sizeof s, e->name);
    text_at(centre(s, 0), shot_y + 15, s);
    upper(paid, sizeof paid, rsb_paid_name);
    rs_strf(s, sizeof s, TR(IS_IN), paid);
    text_at(centre(s, 0), shot_y + 17, s);
    rs_strf(s, sizeof s, TRN(GAMES_IN_APP, rsb_ngames), rsb_ngames);
    text_at(centre(s, 0), shot_y + 19, s);
    /* the buttons: [A] GET IT ON GOOGLE PLAY (a pill), [B] BACK */
    const char *get = TR(GET_IT), *back = TR(BACK);
    btn_w = hu_width(get, 0) + 2;
    back_w = hu_width(back, 0);
    btn_y = shot_y + 21;
    btn_x = (Wt - (2 + btn_w + 1 + 2 + back_w)) / 2 + 2;         /* glyph, pill, gap, glyph, BACK */
    pill_put(btn_x, btn_y, get, 0);
    back_x = btn_x + btn_w + 3;
    text_at(back_x, btn_y, back);
    /* the picture */
    rs_bg_enable(RS_BG3, 0);
    rs_bg_enable(RS_BG4, 0);
    rs_bg_fill(RS_BG2, RS_MAP(0, 0, 0, 0, 0));
    rs_tiles_load(VR_CARD(0) + T_CARDS, e->shot.tiles, e->shot.w * e->shot.h);
    for (int p = 0; p < e->shot.npal && p < 6; p++) rs_pal_load(RS_PAL_BG(1 + p), e->shot.pals + p * 16, 16);
    for (int y = 0; y < e->shot.h; y++)
        for (int x = 0; x < e->shot.w; x++)
            rs_bg_put(RS_BG2, shot_x + x, shot_y + y,
                      RS_MAP(T_CARDS + y * e->shot.w + x, 1 + e->shot.tile_pal[y * e->shot.w + x], 0, 0, 0));
    rs_bg_scroll(RS_BG2, 0, 0);
    rs_bg_enable(RS_BG2, 1);
    layer_row[0] = layer_row[1] = layer_row[2] = -1;
}

static int prev_locked(int g)
{
    for (int k = 1; k <= rsb_ngames; k++) {
        int i = ((g - k) % rsb_ngames + rsb_ngames) % rsb_ngames;
        if (rsb_games[i].module == NULL) return i;
    }
    return -1;
}

static int next_locked(int g)
{
    for (int k = 1; k <= rsb_ngames; k++) {
        int i = (g + k) % rsb_ngames;
        if (locked(i)) return i;
    }
    return -1;
}

static void panel_open(int g, int tour)
{
    if (g < 0) return;
    panel = 1;
    panel_game = g;
    panel_tour = tour;
    panel_t = 0;
    ha_play(HA_SWISH);
}

static void panel_close(void)
{
    panel = 0;
    ha_play(HA_PAUSE);
}

/* ---- the grid ------------------------------------------------------------------------------------------------------------ */
static void card_pos(int g, int *x, int *y)    /* screen position of card g's top-left corner */
{
    *x = grid_x + (g % cols) * PITCH_X;
    *y = GRID_TOP + (g / cols) * PITCH_Y - scroll_px();
}

static void row_build(int layer, int r)
{
    int l = RS_BG2 + layer, base = VR_CARD(layer);
    rs_bg_fill(l, RS_MAP(0, 0, 0, 0, 0));
    for (int c = 0; c < cols; c++) {
        int g = r * cols + c;
        if (g >= rsb_ngames) break;
        const rsb_image *im = &rsb_games[g].card;
        int t0 = T_CARDS + c * im->w * im->h;
        rs_tiles_load(base + t0, im->tiles, im->w * im->h);
        for (int y = 0; y < im->h; y++)
            for (int x = 0; x < im->w; x++)
                rs_bg_put(l, c * (PITCH_X / 8) + x, y, RS_MAP(t0 + y * im->w + x, 1 + c, 0, 0, 0));
        if (!locked(g) && best[g] > 0) {                         /* the best score: a badge in the corner */
            char s[12];
            int n = snprintf(s, sizeof s, "%d", best[g] > 99999 ? 99999 : best[g]);
            int x0 = c * (PITCH_X / 8) + im->w - n - 1, y0 = RSB_THUMB_H / 8 - 1;
            rs_bg_put(l, x0, y0, RS_MAP(T_BADGE + 10, PAL_UI, 0, 0, 0));
            for (int i = 0; i < n; i++) rs_bg_put(l, x0 + 1 + i, y0, RS_MAP(T_BADGE + s[i] - '0', PAL_UI, 0, 0, 0));
        }
    }
    layer_row[layer] = r;
}

static void grid_layers(void)
{
    int sp = scroll_px();
    int first = sp / PITCH_Y, last = (sp + GRID_BOTTOM - GRID_TOP - 1) / PITCH_Y;
    int used[3] = {0, 0, 0};
    for (int r = first; r <= last && r < nrows; r++) {
        int l = r % 3;
        if (layer_row[l] != r) row_build(l, r);
        rs_bg_scroll(RS_BG2 + l, -grid_x, -(GRID_TOP + r * PITCH_Y - sp));
        rs_bg_enable(RS_BG2 + l, 1);
        used[l] = 1;
    }
    for (int l = 0; l < 3; l++)
        if (!used[l]) rs_bg_enable(RS_BG2 + l, 0);
}

/* ---- the raster: the backdrop gradient, the palettes of the card row under each line ------------------------------------ */
static int raster_row;
static void raster(int line, void *user)
{
    (void)user;
    int k = line * 256 / RS_SCREEN_H;
    int d = (line & 4) ? 3 : 0;                                  /* the house 2-line dither step */
    rs_pal_set(0, RS_RGB8(28 - k * 14 / 256 + d, 44 - k * 22 / 256 + d, 90 - k * 44 / 256 + d));
    if (panel) return;
    if (line == 0) raster_row = -99;
    int r = (line - GRID_TOP + scroll_px());
    r = r < 0 ? -1 : r / PITCH_Y;
    if (r != raster_row && r >= 0 && r < nrows) {
        raster_row = r;
        for (int c = 0; c < cols; c++) {
            int g = r * cols + c;
            if (g < rsb_ngames) rs_pal_load(RS_PAL_BG(1 + c), rsb_games[g].card.pals, 16);
        }
    }
}

/* ---- selection ----------------------------------------------------------------------------------------------------------- */
static void reveal(int g)                       /* scroll so that card g is fully visible */
{
    int y = (g / cols) * PITCH_Y, h = GRID_BOTTOM - GRID_TOP, sp = scroll_px();
    int goal = sp;
    if (y < sp) goal = y;
    if (y + RSB_CARD_H > sp + h) goal = y + RSB_CARD_H - h;
    if (goal < 0) goal = 0;
    if (goal > max_scroll()) goal = max_scroll();
    target = goal;
}

static void activate(int g)
{
    if (g == SEL_PILL) { panel_open(next_locked(-1), 1); return; }
    if (g < 0 || g >= rsb_ngames) return;
    if (locked(g)) { panel_open(g, 0); return; }
    ha_play(HA_CONFIRM);
    rsb_launch(g);
}

static void move(int dx, int dy)
{
    int s = sel;
    if (s == SEL_PILL) {
        if (dy < 0) s = sel_mem;
    } else {
        int r = s / cols, c = s % cols;
        if (dx) {
            c += dx;
            if (c >= 0 && c < cols && r * cols + c < rsb_ngames) s = r * cols + c;
        }
        if (dy < 0 && r > 0) s -= cols;
        if (dy > 0) {
            if (r + 1 < nrows) s = s + cols < rsb_ngames ? s + cols : rsb_ngames - 1;
            else { sel_mem = s; s = SEL_PILL; }
        }
    }
    if (s != sel) {
        sel = s;
        ha_play(HA_SELECT);
        if (sel >= 0) reveal(sel);
    }
}

static int hit_card(int x, int y)
{
    if (y < GRID_TOP || y >= GRID_BOTTOM) return -1;
    int gx = x - grid_x, gy = y - GRID_TOP + scroll_px();
    if (gx < 0 || gy < 0) return -1;
    int c = gx / PITCH_X, r = gy / PITCH_Y;
    if (c >= cols || gx % PITCH_X >= RSB_CARD_W || gy % PITCH_Y >= RSB_CARD_H) return -1;
    int g = r * cols + c;
    return g < rsb_ngames ? g : -1;
}

/* ---- the program ---------------------------------------------------------------------------------------------------------- */
static void l_init(void)
{
    W = rs_screen_width();
    Wt = W / 8;
    cols = W / PITCH_X;                       /* 3 at 320, 4 at 16:9, 5 from 520 */
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (cols < 1) cols = 1;
    grid_x = (W - (cols * PITCH_X - 8)) / 2;
    nrows = (rsb_ngames + cols - 1) / cols;
    for (int g = 0; g < rsb_ngames && g < 64; g++) best[g] = rsb_best(g);

    hu_config hc = hu_defaults();
    hc.logo_tile = -1;
    hc.obj_tile = OT_KIT;
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    rs_bg_setup(RS_BG1, 128, 32, VR_BG1);
    for (int l = 0; l < 3; l++) {
        rs_bg_setup(RS_BG2 + l, 128, 64, VR_CARD(l));
        badge_tiles(l);
        layer_row[l] = -1;
    }
    rs_bg_enable(RS_BG1, 1);
    rs_obj_base(VR_OBJ);
    sprite_tiles();
    pill_tiles();
    rs_pal_load(RS_PAL_OBJ(OPAL_MINE), MINE_PAL, 16);
    pill_colours(0);
    ha_init(0);
    rs_raster(raster, NULL);
    if (rsb_last_game() >= 0) sel = rsb_last_game();
    if (sel >= rsb_ngames) sel = 0;
    if (sel >= 0) {
        reveal(sel);
        scroll_q8 = target << 8;
    }
    if (scroll_q8 > max_scroll() << 8) scroll_q8 = max_scroll() << 8;
    shown_sel = -99;
    shown_panel = -1;
    shown_panel_game = -1;
    dragging = 0;
    vel_q8 = 0;
}

static void touch_update(void)
{
    const rsb_pointer *p = rsb_pointer_get();
    if (panel) {
        if (p->released && p->frames < 40 && abs(p->x - p->x0) < 12 && abs(p->y - p->y0) < 12) {
            int tx = p->x / 8, ty = p->y / 8;
            if (ty >= btn_y - 1 && ty <= btn_y + 1 && tx >= btn_x && tx < btn_x + btn_w) {
                ha_play(HA_CONFIRM);
                rsb_open_store();
            } else if ((ty >= btn_y - 1 && ty <= btn_y + 1 && tx >= back_x - 1 && tx < back_x + back_w) ||
                       tx < pan_x || tx >= pan_x + pan_w || ty < pan_y || ty >= pan_y + pan_h) {
                panel_close();
            } else if (panel_tour) {
                panel_game = next_locked(panel_game);
                panel_t = 0;
            }
        }
        return;
    }
    if (p->pressed) {
        dragging = 1;
        moved = 0;
        drag_scroll = scroll_q8;
        drag_last_y = p->y;
        vel_q8 = 0;
        target = -1;
    }
    if (dragging && p->down) {
        if (abs(p->y - p->y0) > 6 || abs(p->x - p->x0) > 6) moved = 1;
        if (moved) {
            scroll_q8 = drag_scroll - ((p->y - p->y0) << 8);
            vel_q8 = (vel_q8 + ((drag_last_y - p->y) << 8)) / 2;
        }
        drag_last_y = p->y;
    }
    if (dragging && p->released) {
        dragging = 0;
        if (!moved) {
            int g = hit_card(p->x0, p->y0);
            int ty = p->y0 / 8;
            if (g >= 0) {
                sel = g;
                activate(g);
            } else if (ty >= 30 - FOOT_ROWS && p->x0 / 8 >= pill_x - 1) {
                sel_mem = sel >= 0 ? sel : sel_mem;
                sel = SEL_PILL;
                activate(SEL_PILL);
            }
            vel_q8 = 0;
        }
    }
}

static void l_update(void)
{
    t++;
    uint16_t pr = 0, held = 0;
    for (int p = 0; p < RS_PAD_MAX; p++) {
        pr |= rs_pad_pressed(p);
        held |= rs_pad(p);
    }
    if (rsb_back_pressed()) {
        if (panel) panel_close();
        else rsb_quit();
        return;
    }
    touch_update();
    if (panel) {
        panel_t++;
        if (pr & (RS_BTN_A | RS_BTN_START)) { ha_play(HA_CONFIRM); rsb_open_store(); }
        else if (pr & (RS_BTN_B | RS_BTN_SELECT)) panel_close();
        else if (pr & (RS_BTN_LEFT | RS_BTN_RIGHT)) {
            panel_game = (pr & RS_BTN_RIGHT) ? next_locked(panel_game) : prev_locked(panel_game);
            ha_play(HA_SELECT);
            panel_t = 0;
        } else if (panel_tour && panel_t >= 240) {
            panel_game = next_locked(panel_game);
            panel_t = 0;
        }
        return;
    }
    /* the D-pad, with repeat */
    uint16_t dirs = held & (RS_BTN_UP | RS_BTN_DOWN | RS_BTN_LEFT | RS_BTN_RIGHT);
    uint16_t now = pr & dirs;
    if (now) { rep_btn = now; rep_t = 0; }
    else if (dirs & rep_btn) { if (++rep_t >= 18 && (rep_t - 18) % 6 == 0) now = rep_btn; }
    else rep_btn = 0;
    if (now & RS_BTN_LEFT) move(-1, 0);
    if (now & RS_BTN_RIGHT) move(1, 0);
    if (now & RS_BTN_UP) move(0, -1);
    if (now & RS_BTN_DOWN) move(0, 1);
    if (pr & (RS_BTN_A | RS_BTN_START)) activate(sel);
    /* scrolling: a goal (D-pad), the fling, the bounds */
    if (!dragging) {
        if (target >= 0) {
            int goal = target << 8, d = goal - scroll_q8;
            scroll_q8 += d / 5 + (d > 0 ? 64 : d < 0 ? -64 : 0);
            if (abs(goal - scroll_q8) < 128) { scroll_q8 = goal; target = -1; }
        } else if (vel_q8) {
            scroll_q8 += vel_q8;
            vel_q8 = vel_q8 * 15 / 16;
            if (abs(vel_q8) < 32) vel_q8 = 0;
        }
        if (scroll_q8 < 0) { scroll_q8 += (-scroll_q8) / 3 + 1; vel_q8 = 0; }
        if (scroll_q8 > max_scroll() << 8) { scroll_q8 -= (scroll_q8 - (max_scroll() << 8)) / 3 + 1; vel_q8 = 0; }
    }
}

static void frame_sprites(int x, int y)
{
    int w = RSB_CARD_W + 8, h = RSB_CARD_H + 8;
    x -= 4;
    y -= 4;
    int p = 2;
    rs_spr(x, y, OT_CORNER, 8, 8, OPAL_MINE, p, 0);
    rs_spr(x + w - 8, y, OT_CORNER, 8, 8, OPAL_MINE, p, RS_SPR_HFLIP);
    rs_spr(x, y + h - 8, OT_CORNER, 8, 8, OPAL_MINE, p, RS_SPR_VFLIP);
    rs_spr(x + w - 8, y + h - 8, OT_CORNER, 8, 8, OPAL_MINE, p, RS_SPR_HFLIP | RS_SPR_VFLIP);
    for (int k = 0; k < 3; k++) {
        int ex = k < 2 ? x + 8 + k * 32 : x + w - 8 - 32;
        rs_spr(ex, y, OT_HEDGE, 32, 8, OPAL_MINE, p, 0);
        rs_spr(ex, y + h - 8, OT_HEDGE, 32, 8, OPAL_MINE, p, RS_SPR_VFLIP);
        int ey = k < 2 ? y + 8 + k * 32 : y + h - 8 - 32;
        rs_spr(x, ey, OT_VEDGE, 8, 32, OPAL_MINE, p, 0);
        rs_spr(x + w - 8, ey, OT_VEDGE, 8, 32, OPAL_MINE, p, RS_SPR_HFLIP);
    }
}

static void l_draw(void)
{
    /* BG1: redrawn when the screen changes */
    if (panel != shown_panel || (panel && panel_game != shown_panel_game) || (!panel && sel != shown_sel)) {
        if (panel) panel_draw();
        else {
            if (shown_panel != 0) {
                hu_clear();
                draw_header();
            }
            draw_footer();
        }
        shown_panel = panel;
        shown_panel_game = panel_game;
        shown_sel = sel;
    }
    pill_colours(!panel && sel == SEL_PILL);
    rs_oam_clear();
    if (panel) {
        hu_glyph(HU_BTN_A, btn_x * 8 - 18 - hu_ui_x(), btn_y * 8 - 4, 0, 3);   /* the kit adds its UI offset */
        hu_glyph(HU_BTN_B, back_x * 8 - 18 - hu_ui_x(), btn_y * 8 - 4, 0, 3);
        return;
    }
    grid_layers();
    /* the selection frame (gold, glowing), the padlocks */
    rs_pal_set(RS_PAL_OBJ(OPAL_MINE) + C_GOLDL, hu_lerp_color(RS_RGB8(255, 240, 150), RS_RGB8(255, 255, 255),
                                                             hu_bob(t, 64, 64) + 64));
    if (sel >= 0) {
        int x, y;
        card_pos(sel, &x, &y);
        frame_sprites(x, y);
    }
    hu_glyph(HU_BTN_A, 8 - hu_ui_x(), FOOT_TEXT_ROW * 8 - 4, 0, 3);
    for (int g = 0; g < rsb_ngames; g++) {
        if (!locked(g)) continue;
        int x, y;
        card_pos(g, &x, &y);
        if (y + RSB_THUMB_H <= GRID_TOP - 8 || y >= GRID_BOTTOM) continue;
        rs_spr(x + RSB_CARD_W / 2 - 8, y + RSB_THUMB_H / 2 - 8, OT_LOCK, 16, 16, OPAL_MINE, 2, 0);
    }
}

const rs_game *rsb_launcher_game(void)
{
    static const rs_game g = {"RetroStone Arcade", "launcher", "0.1.0", l_init, l_update, l_draw, NULL, NULL,
                              NULL, NULL, 0, RS_SCREEN_W_MAX};
    return &g;
}
