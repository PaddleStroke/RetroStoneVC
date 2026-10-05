/*
 * RetroStone Arcade: the launcher, itself a RetroStone VC program (the house style of games/common).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The screen (W = rsb_width(), 320..560; 240 high):
 *   header  rows 0-3: a panel with the app's name in big gold letters; round buttons in its corners: "i" (top
 *           left: Credits & licences) and "?" (top right: help)
 *   grid    the cards of every game (rsb_games): its title screen, its name, its best score; the locked games
 *           (the free app) dimmed, with a padlock. 3 columns at 320, up to 5 on a wide screen; the rows scroll
 *           (D-pad, or drag / swipe)
 *   footer  rows 26-29, only when it has something: the free app's "FULL VERSION" pill (small, at the right) and,
 *           when a gamepad is connected, the A glyph and the selected game's name
 *   dialogs (a back arrow in the top-left corner closes them, as Android's Back, a tap outside them, or B):
 *           the panel "Get the full version": a locked game's picture, the paid app's name, ONE big button
 *           "GET IT ON GOOGLE PLAY"; the credits: every component and its licence (the translated list), then a
 *           licence's full text (English, scrolled); the help: touch is player 1, gamepads players 2 to 4, the
 *           in-game pause and back buttons
 * The full app (no locked game) has no "full version" banner and no panel: the same menu otherwise.
 * The pad's glyph hints (A, B) are shown only when a gamepad is connected (rsb_gamepad()): touch players never see
 * them. Input: D-pad (repeat) + A (play / open) + B (close); touch: tap a card = launch, drag scrolls, tap the pill,
 * the corner buttons, the panel's button; Back closes a dialog, else leaves the app.
 *
 * Video: BG1 the UI (house kit, high priority); the card rows on BG2, BG3, BG4 (row r on layer r % 3, each layer
 * scrolled to its row: at most 3 rows are visible); the raster callback loads the palettes of the row under each
 * line (one palette per column: BG 1..5) and the backdrop gradient. Sprites: the gold frame of the selection, the
 * padlocks, the round corner buttons, the kit's glyphs.
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
#define PITCH_X     (RSB_CARD_W + 8)
#define PITCH_Y     (RSB_CARD_H + 8)
#define MAX_COLS    5

/* VRAM (tiles): BG1 the kit at 0 (HU_BG_TILES), the pill and the big button at T_PILL; card layer l (0..2 = BG2..BG4)
 * at VR_CARD(l): a blank tile, the badge glyphs, the cards (or the panel's picture, the licence's font on layer 0);
 * sprites at VR_OBJ */
#define VR_BG1      0
#define T_PILL      320                     /* 3 tiles: the pill's ends */
#define T_BTN       323                     /* 5 tiles: the big button's corner, side, top, fill, play sign */
#define VR_CARD(l)  (1024 * ((l) + 1))
#define T_BADGE     1
#define T_CARDS     32
#define T_TEXT      32                      /* credits: the font of the licence texts, on BG2 (layer 0's tiles) */
#define VR_OBJ      3840
#define OT_KIT      0
#define OT_CORNER   128
#define OT_HEDGE    129                     /* 32x8: 4 tiles */
#define OT_VEDGE    133                     /* 8x32: 4 tiles */
#define OT_LOCK     137                     /* 16x16: 4 tiles */
#define OT_INFO     141                     /* round 16x16 buttons, 2 looks (normal, selected) x 4 tiles: "i" */
#define OT_HELP     149                     /* "?" */
#define OT_BACK     157                     /* the back arrow */
#define OT_PAUSE_ICON 165                   /* the help's pictures of the in-game buttons: pause, back (4 tiles each) */
#define OT_BACK_ICON  169
#define PAL_UI      0
#define PAL_PILL    7
#define OPAL_MINE   2
#define CORNER_Y    8                       /* the round corner buttons: x 5 and W - 21, y 8 */

enum { SEL_PILL = -2, SEL_INFO = -3, SEL_HELP = -4 };

/* ---- state (kept between runs) --------------------------------------------------------------------------------- */
static int W, Wt, cols, nrows, grid_x;      /* layout of this run */
static int grid_bottom = RS_SCREEN_H - FOOT_ROWS * 8, foot;   /* the footer is shown (then the grid stops above it) */
static int sel = 0, sel_mem = 0;            /* the selected card (or SEL_*), the card before a corner button */
static int scroll_q8, vel_q8, target = -1;  /* the grid's scroll (1/256 px), its fling speed, a goal for the D-pad */
static int layer_row[3];
static int panel, panel_game, panel_tour, panel_t;
static int t, rep_btn, rep_t;
static int dragging, drag_scroll, drag_last_y, moved;
static int best[64];
static int shown_sel = -99, shown_panel = -1, shown_panel_game = -1, shown_pad = -1;
static int promo;                           /* some games are locked: the free app (the banner, the panel) */
static int credits, cr_sel, shown_credits;  /* credits: 0 off, 1 the list, 2 a licence's text (cr_sel) */
static int help, shown_help;                /* the help panel */
static int pad;                             /* a gamepad is connected: the glyph hints */

static int max_scroll(void)
{
    int h = nrows * PITCH_Y - 8 - (grid_bottom - GRID_TOP);
    return h > 0 ? h : 0;
}
static int scroll_px(void) { return scroll_q8 >> 8; }
static int locked(int g) { return rsb_games[g].module == NULL; }
static int dialog(void) { return panel || credits || help; }

/* ---- tiles drawn here ------------------------------------------------------------------------------------------------ */
static void tile8(int vram_tile, const uint8_t px[64]) { rs_tiles_load8(vram_tile, px, 1); }

/* a glyph of the house font on a fill colour (the badges: fill 3 navy, ink 1 cream; the licence texts: clear fill) */
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

/* the padlock, the selection frame and the round buttons (OBJ palette 2) */
enum { C_OUT = 1, C_SIL = 2, C_SILD = 3, C_GOLDL = 4, C_GOLD = 5, C_GOLDD = 6 };
static const rs_color MINE_PAL[16] = {
    0, RS_RGB8(22, 18, 40), RS_RGB8(222, 230, 238), RS_RGB8(128, 138, 156),
    RS_RGB8(255, 240, 150), RS_RGB8(252, 216, 72), RS_RGB8(186, 128, 22)};
static const char *LOCK[16] = {
    "................", ".....oooooo.....", "....oSSSSSSo....", "...oSSooooSSo...",
    "...oSo....oTo...", "...oSo....oTo...", "...oSo....oTo...", ".oooooooooooooo.",
    ".oHHHHHHHHHHHGo.", ".oHGGGGGGGGGGDo.", ".oHGGGGooGGGGDo.", ".oHGGGGooGGGGDo.",
    ".oHGGGGGoGGGGDo.", ".oGDDDDDDDDDDDo.", ".oooooooooooooo.", "................"};
/* the symbols of the round buttons and of the help's pictures (16 x 16, '#' = ink) */
static const char *SYM_I[16] = {
    "", "", "", "       ##", "       ##", "", "      ###", "       ##", "       ##", "       ##", "       ##",
    "      ####", "", "", "", ""};
static const char *SYM_Q[16] = {
    "", "", "", "      ####", "     ##  ##", "         ##", "        ##", "       ##", "       ##", "",
    "       ##", "       ##", "", "", "", ""};
static const char *SYM_BACK[16] = {
    "", "", "", "       #", "      ##", "     ###", "    ##########", "   ###########", "    ##########",
    "     ###", "      ##", "       #", "", "", "", ""};
static const char *SYM_PAUSE[16] = {
    "", "", "", "", "     ##  ##", "     ##  ##", "     ##  ##", "     ##  ##", "     ##  ##", "     ##  ##",
    "     ##  ##", "     ##  ##", "", "", "", ""};

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

static int sym_on(const char *const sym[16], int x, int y)
{
    return (int)strlen(sym[y]) > x && sym[y][x] == '#';
}

/* a round gold button with a dark symbol; look 1 (selected): the glowing light gold */
static void round_tiles(int ot, const char *const sym[16])
{
    uint8_t px[64];
    for (int look = 0; look < 2; look++)
        for (int q = 0; q < 4; q++) {
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    int X = (q & 1) * 8 + x, Y = (q >> 1) * 8 + y;
                    int dx = 2 * X - 15, dy = 2 * Y - 15, d = dx * dx + dy * dy;   /* (2x the distance)^2 */
                    int v = 0;
                    if (d <= 15 * 15) v = C_OUT;
                    if (d <= 13 * 13) v = look ? C_GOLDL : (dx + dy < -8 ? C_GOLDL : dx + dy > 10 ? C_GOLDD : C_GOLD);
                    if (sym_on(sym, X, Y)) v = C_OUT;
                    px[y * 8 + x] = (uint8_t)v;
                }
            tile8(VR_OBJ + ot + look * 4 + q, px);
        }
}

/* the in-game buttons as the frontend draws them: a dark rounded square, a light symbol */
static void square_tiles(int ot, const char *const sym[16])
{
    uint8_t px[64];
    for (int q = 0; q < 4; q++) {
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                int X = (q & 1) * 8 + x, Y = (q >> 1) * 8 + y;
                int corner = (X < 2 || X > 13) && (Y < 2 || Y > 13);
                px[y * 8 + x] = (uint8_t)(corner ? 0 : sym_on(sym, X, Y) ? C_SIL : C_OUT);
            }
        tile8(VR_OBJ + ot + q, px);
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
    round_tiles(OT_INFO, SYM_I);
    round_tiles(OT_HELP, SYM_Q);
    round_tiles(OT_BACK, SYM_BACK);
    square_tiles(OT_PAUSE_ICON, SYM_PAUSE);
    square_tiles(OT_BACK_ICON, SYM_BACK);
}

/* ---- the pill (the "FULL VERSION" banner) and the big button (GET IT ON GOOGLE PLAY): the kit's text on the panel
 * fill, its map entries moved to their own palette (the UI palette with the fill gold, the outline dark gold and the ink
 * navy; brighter when selected). Any language: the text is the kit's (UTF-8, accents and CJK use the row above). */
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
    /* the big button: a rounded corner (top left; the others flipped), the left side, the top side, the fill, and the
     * play sign (a red triangle) */
    for (int k = 0; k < 5; k++) {
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                int v = HU_FILL;
                if (k == 0) {
                    int dx = 2 * x + 1 - 16, dy = 2 * y + 1 - 16, d = dx * dx + dy * dy;
                    v = d > 256 ? 0 : d > 156 ? HU_OUTLINE : HU_FILL;
                } else if (k == 1) v = x < 2 ? HU_OUTLINE : HU_FILL;
                else if (k == 2) v = y < 2 ? HU_OUTLINE : HU_FILL;
                else if (k == 4) {
                    int half = y < 4 ? y : 7 - y;                /* 0..3: the triangle's half height at this row */
                    v = y >= 1 && y <= 6 && x >= 1 && x <= 1 + 2 * half ? HU_RED : HU_FILL;
                }
                px[y * 8 + x] = (uint8_t)v;
            }
        tile8(VR_BG1 + T_BTN + k, px);
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

/* the kit's text s at (x, row), moved to the pill palette (with the row above when it uses it) */
static int pill_text(int x, int row, const char *s)
{
    int w = hu_width(s, 0), tall = hu_tall(s);
    hu_box_text(x, row, s);
    for (int r = row - tall; r <= row; r++)
        for (int i = 0; i < w; i++) {
            uint16_t e = rs_bg_get(RS_BG1, x + i, r);
            rs_bg_put(RS_BG1, x + i, r, (uint16_t)((e & ~(7 << 10)) | (PAL_PILL << 10)));
        }
    return w;
}

/* text s as a pill at (x, row): returns its width in tiles (the two ends included) */
static int pill_put(int x, int row, const char *s, int star)
{
    int w = hu_width(s, 0), tall = hu_tall(s);
    pill_text(x + 1, row, s);
    for (int r = row - tall; r <= row; r++) {
        rs_bg_put(RS_BG1, x, r, RS_MAP(T_PILL + (star && r == row ? 0 : 2), PAL_PILL, 1, 0, 0));
        rs_bg_put(RS_BG1, x + w + 1, r, RS_MAP(T_PILL + 1, PAL_PILL, 1, 0, 0));
    }
    return w + 2;
}

/* the big button, 3 rows (row - 1 .. row + 1): the play sign, the text; returns its width in tiles */
static int button_put(int x, int row, const char *s)
{
    int w = hu_width(s, 0), bw = w + 5, tall = hu_tall(s);
    pill_text(x + 3, row, s);                                    /* first: the kit checks it is on a panel */
    for (int c = 0; c < bw; c++) {
        int edge = c == 0 || c == bw - 1, flip = c == bw - 1, text = c >= 3 && c < 3 + w;
        if (!(text && tall)) rs_bg_put(RS_BG1, x + c, row - 1, RS_MAP(T_BTN + (edge ? 0 : 2), PAL_PILL, 1, flip, 0));
        if (!text) rs_bg_put(RS_BG1, x + c, row, RS_MAP(T_BTN + (edge ? 1 : c == 1 ? 4 : 3), PAL_PILL, 1, flip, 0));
        rs_bg_put(RS_BG1, x + c, row + 1, RS_MAP(T_BTN + (edge ? 0 : 2), PAL_PILL, 1, flip, 1));
    }
    return bw;
}

static void pill_draw(int row)
{
    const char *s = TR(FULL_VERSION);
    pill_w = hu_width(s, 0) + 2;
    pill_x = Wt - 2 - pill_w;
    pill_put(pill_x, row, s, 1);
}

/* ---- the header, the footer and the dialogs (BG1) --------------------------------------------------------------------- */
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
    /* the longest run of words that fits in big letters between the two round buttons (3 columns each);
     * "RETROSTONE ARCADE FREE" -> "RETROSTONE ARCADE" */
    while ((int)strlen(name) * 2 > Wt - 6) {
        char *sp = strrchr(name, ' ');
        if (!sp) { name[(Wt - 6) / 2] = 0; break; }
        *sp = 0;
    }
    hu_panel(0, 0, Wt, HEAD_ROWS);
    hu_big(centre(name, 1), 1, name, HU_BIG_BANNER);
}

static int footer_wanted(void) { return promo || pad; }

static void draw_footer(void)
{
    if (!foot) return;
    int y = 30 - FOOT_ROWS;
    hu_panel(0, y, Wt, FOOT_ROWS);
    if (promo) pill_draw(FOOT_TEXT_ROW);
    else pill_x = Wt - 1;                                         /* the full app: no banner */
    if (!pad) return;                                             /* the A glyph and the name: for pad players */
    char line[96] = "";
    int room = pill_x - 1 - 4;
    const char *named = sel == SEL_INFO ? TR(CREDITS) : sel == SEL_HELP ? TR(HELP) : NULL;
    if (sel >= 0) upper(line, sizeof line, rsb_games[sel].name);
    else if (named) { if (hu_width(named, 0) <= room) rs_strf(line, sizeof line, "%s", named); }
    else rs_strf(line, sizeof line, TRN(N_GAMES, rsb_ngames), rsb_ngames);
    while (line[0] && hu_width(line, 0) > room) {                 /* a whole UTF-8 character at a time */
        size_t n = strlen(line);
        while (n > 1 && (line[n - 1] & 0xC0) == 0x80) n--;
        line[n - 1] = 0;
    }
    text_at(4, FOOT_TEXT_ROW, line);
}

/* the dialogs' box: the whole height, 3 columns kept free on the left for the back arrow (at 320) */
static int pan_x, pan_y, pan_w, pan_h, shot_x, shot_y, btn_x, btn_y, btn_w;

static void dialog_box(const char *title)
{
    pan_w = Wt - 6 < 38 ? Wt - 6 : 38;
    pan_h = 30;
    pan_x = (Wt - pan_w) / 2;
    pan_y = 0;
    hu_clear();
    for (int l = 0; l < 3; l++) rs_bg_enable(RS_BG2 + l, 0);
    hu_panel(pan_x, pan_y, pan_w, pan_h);
    if (!rs_lang_wide(rs_lang()) && hu_width(title, 1) <= pan_w - 2) hu_big(centre(title, 1), 2, title, HU_BIG_BANNER);
    else text_at(centre(title, 0), 3, title);
}

/* the panel: "Get the full version" with a locked game's picture (a hole in the panel shows BG2).
 * Rows (every text line has the row above it free: accents, CJK): 0 rim, 2-3 title, 5-19 picture, 20 name,
 * 22 "is in", 24 "N games", 25-27 the button, 29 rim */
static void panel_draw(void)
{
    const rsb_entry *e = &rsb_games[panel_game];
    char s[64], paid[48];
    dialog_box(TR(FULL_VERSION));
    shot_x = (Wt - RSB_SHOT_W / 8) / 2;
    shot_y = 5;
    for (int y = 0; y < RSB_SHOT_H / 8; y++)                     /* the hole */
        for (int x = 0; x < RSB_SHOT_W / 8; x++) rs_bg_put(RS_BG1, shot_x + x, shot_y + y, RS_MAP(0, PAL_UI, 0, 0, 0));
    upper(s, sizeof s, e->name);
    text_at(centre(s, 0), 20, s);
    upper(paid, sizeof paid, rsb_paid_name);
    rs_strf(s, sizeof s, TR(IS_IN), paid);
    text_at(centre(s, 0), 22, s);
    rs_strf(s, sizeof s, TRN(GAMES_IN_APP, rsb_ngames), rsb_ngames);
    text_at(centre(s, 0), 24, s);
    const char *get = TR(GET_IT);
    btn_w = hu_width(get, 0) + 5;
    btn_y = 26;
    btn_x = (Wt - btn_w) / 2;
    button_put(btn_x, btn_y, get);
    /* the picture */
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

static void menu_again(void)                /* a dialog closed: the menu draws itself again */
{
    shown_panel = -1;
    shown_sel = -99;
    layer_row[0] = layer_row[1] = layer_row[2] = -1;
}

static void panel_close(void)
{
    panel = 0;
    ha_play(HA_PAUSE);
}

/* a tap: down and up at the same place, quickly */
static int tapped(const rsb_pointer *p)
{
    return p->released && p->frames < 40 && abs(p->x - p->x0) < 12 && abs(p->y - p->y0) < 12;
}
static int on_back_arrow(const rsb_pointer *p) { return p->x0 < 32 && p->y0 < 32; }
static int outside_box(const rsb_pointer *p)
{
    int tx = p->x0 / 8, ty = p->y0 / 8;
    return tx < pan_x || tx >= pan_x + pan_w || ty < pan_y || ty >= pan_y + pan_h;
}

/* ---- the help ------------------------------------------------------------------------------------------------------------ */
#define HELP_ICON_X (pan_x + 2)
static void help_draw(void)
{
    dialog_box(TR(HELP));
    text_at(centre(TR(HELP_TOUCH), 0), 7, TR(HELP_TOUCH));
    text_at(centre(TR(HELP_PADS), 0), 10, TR(HELP_PADS));
    text_at(centre(TR(HELP_PADS2), 0), 12, TR(HELP_PADS2));
    text_at(HELP_ICON_X + 3, 16, TR(HELP_PAUSE));
    text_at(HELP_ICON_X + 3, 20, TR(HELP_BACK));
    text_at(HELP_ICON_X + 3, 23, TR(HELP_SAVE));
}

static void help_update(uint16_t pr)
{
    const rsb_pointer *p = rsb_pointer_get();
    if ((tapped(p) && (on_back_arrow(p) || outside_box(p))) || (pr & (RS_BTN_A | RS_BTN_B | RS_BTN_START | RS_BTN_SELECT))) {
        help = 0;
        menu_again();
        ha_play(HA_PAUSE);
    }
}

/* ---- the credits ----------------------------------------------------------------------------------------------------------
 * The list (BG1, the kit's text: translated): the owner line, then each component (rsb_credits) with its licence's short
 * name at the right; a tap (or A) opens the full text. The text (English, ASCII): BG2 with the 5x7 font (T_TEXT), the lines
 * wrapped to the screen's width in a ring of 64 map rows, scrolled under the header and the footer (BG1, opaque). */
#define DOC_TOP     32                      /* the text's first screen line (under the 4-row header) */
#define DOC_H       (RS_SCREEN_H - DOC_TOP - FOOT_ROWS * 8)
#define DOC_LINES   2400
static struct { const char *p; uint16_t n; uint8_t ind; } doc[DOC_LINES];
static int ndoc, doc_cols, doc_row[64], doc_pct = -1;
static int doc_q8, doc_vel, doc_target = -1;

static int cr_row(int i) { return i < 2 ? 8 + 2 * i : 9 + 2 * i; }   /* the games' two lines, a gap, the others */
#define CR_HINT_ROW (cr_row(rsb_ncredits - 1) + 4)
static const char *cr_label(int i)
{
    const rsb_credit *c = &rsb_credits[i];
    return c->role == RSB_ROLE_CODE ? TR(CODE) : c->role == RSB_ROLE_ART ? TR(ART) : c->name;
}

static void doc_add(const char *p, int n, int ind)
{
    if (ndoc >= DOC_LINES) return;
    doc[ndoc].p = p;
    doc[ndoc].n = (uint16_t)n;
    doc[ndoc].ind = (uint8_t)ind;
    ndoc++;
}

/* the paragraphs of s ("\n\n" between them; leading spaces: an indent) wrapped to doc_cols; a blank line after each */
static void doc_wrap(const char *s)
{
    while (*s) {
        const char *end = strstr(s, "\n\n");
        if (!end) end = s + strlen(s);
        int ind = 0;
        while (s < end && *s == ' ') { s++; ind++; }
        int room = doc_cols - ind;
        while (s < end) {
            const char *line = s, *brk = NULL;
            int n = 0;
            while (s + n < end && n < room) {
                if (s[n] == ' ') brk = s + n;
                n++;
            }
            if (s + n < end && s[n] != ' ' && brk) n = (int)(brk - s);   /* break at the last space */
            doc_add(line, n, ind);
            s += n;
            while (s < end && *s == ' ') s++;
        }
        doc_add("", 0, 0);
        s = *end ? end + 2 : end;
    }
}

static void doc_open(int i)
{
    const rsb_credit *c = &rsb_credits[i];
    ndoc = 0;
    doc_cols = Wt - 4;
    doc_wrap(c->title);
    doc_wrap(c->text);
    doc_q8 = doc_vel = 0;
    doc_target = -1;
    doc_pct = -1;
    for (int r = 0; r < 64; r++) doc_row[r] = -2;
    credits = 2;
    cr_sel = i;
    ha_play(HA_CONFIRM);
}

static int doc_max(void)
{
    int h = ndoc * 8 - DOC_H;
    return h > 0 ? h : 0;
}

static void credits_open(void)
{
    credits = 1;
    cr_sel = 0;
    dragging = 0;
    ha_play(HA_SWISH);
}

static void credits_close(void)
{
    if (credits == 2) { credits = 1; ha_play(HA_PAUSE); return; }
    credits = 0;
    menu_again();
    ha_play(HA_PAUSE);
}

static void credits_list_draw(void)
{
    dialog_box(TR(CREDITS));
    text_at(centre(rsb_credits_owner, 0), 6, rsb_credits_owner);
    int lx = pan_x + 2, rx = pan_x + pan_w - 2;
    for (int i = 0; i < rsb_ncredits; i++) {
        text_at(lx, cr_row(i), cr_label(i));
        text_at(rx - hu_width(rsb_credits[i].licence, 0), cr_row(i), rsb_credits[i].licence);
    }
    if (pad) text_at((Wt - hu_width(TR(READ), 0)) / 2 + 1, CR_HINT_ROW, TR(READ));
}

static void doc_footer(int pct)
{
    char s[8];
    hu_panel(0, 30 - FOOT_ROWS, Wt, FOOT_ROWS);
    if (pad) text_at(4, FOOT_TEXT_ROW, TR(BACK));
    rs_strf(s, sizeof s, "%d%%", pct);
    text_at(Wt - 2 - hu_width(s, 0), FOOT_TEXT_ROW, s);
    doc_pct = pct;
}

static void credits_doc_draw(void)
{
    char s[96];
    const rsb_credit *c = &rsb_credits[cr_sel];
    hu_clear();
    hu_panel(0, 0, Wt, HEAD_ROWS);
    rs_strf(s, sizeof s, "%s - %s", c->name, c->licence);
    text_at(centre(s, 0), 2, s);
    doc_footer(0);
    for (int i = 0; i < 96; i++) glyph_on(VR_CARD(0) + T_TEXT + i, 32 + i, 0, HU_CREAM, HU_OUTLINE);
    rs_bg_fill(RS_BG2, RS_MAP(0, 0, 0, 0, 0));
    for (int r = 0; r < 64; r++) doc_row[r] = -2;
    rs_bg_enable(RS_BG2, 1);
    rs_bg_enable(RS_BG3, 0);
    rs_bg_enable(RS_BG4, 0);
    layer_row[0] = layer_row[1] = layer_row[2] = -1;
}

/* the text's lines that are on the screen, in their map rows; the layer scrolled to them */
static void doc_layers(void)
{
    int sp = doc_q8 >> 8;
    for (int L = sp / 8; L <= (sp + DOC_H) / 8; L++) {
        int m = L & 63;
        if (doc_row[m] == L) continue;
        for (int x = 0; x < doc_cols + 2; x++) rs_bg_put(RS_BG2, x, m, RS_MAP(0, 0, 0, 0, 0));
        if (L < ndoc)
            for (int k = 0; k < doc[L].n; k++) {
                int ch = (unsigned char)doc[L].p[k];
                if (ch < 32 || ch > 127) ch = '?';
                rs_bg_put(RS_BG2, doc[L].ind + k, m, RS_MAP(T_TEXT + ch - 32, PAL_UI, 0, 0, 0));
            }
        doc_row[m] = L;
    }
    rs_bg_scroll(RS_BG2, -16, ((sp - DOC_TOP) % 512 + 512) % 512);
    int pct = doc_max() ? sp * 100 / doc_max() : 100;
    if (pct != doc_pct) doc_footer(pct < 0 ? 0 : pct > 100 ? 100 : pct);
}

static void credits_update(uint16_t pr, uint16_t held)
{
    const rsb_pointer *p = rsb_pointer_get();
    int tap = tapped(p);
    if (credits == 1) {
        if (tap) {
            int tx = p->x / 8, ty = p->y / 8, hit = -1;
            if (on_back_arrow(p) || outside_box(p)) { credits_close(); return; }
            for (int i = 0; i < rsb_ncredits; i++)
                if (ty >= cr_row(i) - 1 && ty <= cr_row(i) && tx >= pan_x && tx < pan_x + pan_w) hit = i;
            if (hit >= 0) doc_open(hit);
            return;
        }
        if (pr & (RS_BTN_UP | RS_BTN_DOWN)) {
            cr_sel = (cr_sel + (pr & RS_BTN_DOWN ? 1 : rsb_ncredits - 1)) % rsb_ncredits;
            ha_play(HA_SELECT);
        }
        if (pr & (RS_BTN_A | RS_BTN_START)) doc_open(cr_sel);
        else if (pr & (RS_BTN_B | RS_BTN_SELECT)) credits_close();
        return;
    }
    /* the text: drag (and fling), the D-pad (Up / Down scroll, Left / Right / L / R a page), B back; the back arrow */
    if (pr & (RS_BTN_B | RS_BTN_SELECT | RS_BTN_A | RS_BTN_START)) { credits_close(); return; }
    if (tap && on_back_arrow(p)) { credits_close(); return; }
    if (p->pressed) { dragging = 1; drag_scroll = doc_q8; drag_last_y = p->y; doc_vel = 0; doc_target = -1; }
    if (dragging && p->down) {
        doc_q8 = drag_scroll - ((p->y - p->y0) << 8);
        doc_vel = (doc_vel + ((drag_last_y - p->y) << 8)) / 2;
        drag_last_y = p->y;
    }
    if (dragging && p->released) dragging = 0;
    if (!dragging) {
        int sp = doc_q8 >> 8;
        if (pr & (RS_BTN_RIGHT | RS_BTN_R)) doc_target = sp + DOC_H - 16;
        if (pr & (RS_BTN_LEFT | RS_BTN_L)) doc_target = sp - DOC_H + 16;
        if (held & (RS_BTN_UP | RS_BTN_DOWN)) {
            int v = (held & RS_BTN_DOWN ? 1 : -1) * (3 << 8);
            doc_q8 += v;
            doc_vel = 0;
            doc_target = -1;
        } else if (doc_target != -1) {
            if (doc_target < 0) doc_target = 0;
            if (doc_target > doc_max()) doc_target = doc_max();
            int d = (doc_target << 8) - doc_q8;
            doc_q8 += d / 5 + (d > 0 ? 64 : d < 0 ? -64 : 0);
            if (abs((doc_target << 8) - doc_q8) < 128) { doc_q8 = doc_target << 8; doc_target = -1; }
        } else if (doc_vel) {
            doc_q8 += doc_vel;
            doc_vel = doc_vel * 15 / 16;
            if (abs(doc_vel) < 32) doc_vel = 0;
        }
        if (doc_q8 < 0) { doc_q8 = 0; doc_vel = 0; }
        if (doc_q8 > doc_max() << 8) { doc_q8 = doc_max() << 8; doc_vel = 0; }
    }
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
    int first = sp / PITCH_Y, last = (sp + grid_bottom - GRID_TOP - 1) / PITCH_Y;
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
    if (dialog()) return;
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
    int y = (g / cols) * PITCH_Y, h = grid_bottom - GRID_TOP, sp = scroll_px();
    int goal = sp;
    if (y < sp) goal = y;
    if (y + RSB_CARD_H > sp + h) goal = y + RSB_CARD_H - h;
    if (goal < 0) goal = 0;
    if (goal > max_scroll()) goal = max_scroll();
    target = goal;
}

static void activate(int g)
{
    if (g == SEL_INFO) { credits_open(); return; }
    if (g == SEL_HELP) { help = 1; ha_play(HA_SWISH); return; }
    if (g == SEL_PILL) { if (promo) panel_open(next_locked(-1), 1); return; }
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
    } else if (s == SEL_INFO || s == SEL_HELP) {                  /* the round buttons of the header */
        if (dy > 0) s = sel_mem;
        else if (dx > 0) s = SEL_HELP;
        else if (dx < 0) s = SEL_INFO;
    } else {
        int r = s / cols, c = s % cols;
        if (dx) {
            c += dx;
            if (c >= 0 && c < cols && r * cols + c < rsb_ngames) s = r * cols + c;
        }
        if (dy < 0 && r > 0) s -= cols;
        else if (dy < 0) { sel_mem = s; s = c < (cols + 1) / 2 ? SEL_INFO : SEL_HELP; }   /* up from the first row */
        if (dy > 0) {
            if (r + 1 < nrows) s = s + cols < rsb_ngames ? s + cols : rsb_ngames - 1;
            else if (promo) { sel_mem = s; s = SEL_PILL; }
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
    if (y < GRID_TOP || y >= grid_bottom) return -1;
    int gx = x - grid_x, gy = y - GRID_TOP + scroll_px();
    if (gx < 0 || gy < 0) return -1;
    int c = gx / PITCH_X, r = gy / PITCH_Y;
    if (c >= cols || gx % PITCH_X >= RSB_CARD_W || gy % PITCH_Y >= RSB_CARD_H) return -1;
    int g = r * cols + c;
    return g < rsb_ngames ? g : -1;
}

/* ---- the program ---------------------------------------------------------------------------------------------------------- */
static void layout_footer(void)
{
    foot = footer_wanted();
    grid_bottom = RS_SCREEN_H - (foot ? FOOT_ROWS * 8 : 0);
}

static void l_init(void)
{
    W = rs_screen_width();
    Wt = W / 8;
    cols = W / PITCH_X;                       /* 3 at 320, 4 at 16:9, 5 from 520 */
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (cols < 1) cols = 1;
    grid_x = (W - (cols * PITCH_X - 8)) / 2;
    nrows = (rsb_ngames + cols - 1) / cols;
    promo = 0;
    for (int g = 0; g < rsb_ngames; g++) promo |= locked(g);
    for (int g = 0; g < rsb_ngames && g < 64; g++) best[g] = rsb_best(g);
    pad = rsb_gamepad();
    layout_footer();

    hu_config hc = hu_defaults();
    hc.logo_tile = -1;
    hc.obj_tile = OT_KIT;
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hc.ext_count = 256;                       /* the help's Japanese / Chinese / Korean lines need many glyphs */
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
    if (sel >= rsb_ngames || (sel == SEL_PILL && !promo)) sel = 0;
    if (sel >= 0) {
        reveal(sel);
        scroll_q8 = target << 8;
    }
    if (scroll_q8 > max_scroll() << 8) scroll_q8 = max_scroll() << 8;
    shown_sel = -99;
    shown_panel = -1;
    shown_panel_game = -1;
    shown_credits = 0;
    shown_help = 0;
    shown_pad = pad;
    if (credits == 2 && doc_cols != Wt - 4) credits = 1;      /* a new width: back to the list (the text re-wraps) */
    dragging = 0;
    vel_q8 = 0;
}

static void panel_touch(void)
{
    const rsb_pointer *p = rsb_pointer_get();
    if (!tapped(p)) return;
    int tx = p->x / 8, ty = p->y / 8;
    if (ty >= btn_y - 1 && ty <= btn_y + 1 && tx >= btn_x && tx < btn_x + btn_w) {
        ha_play(HA_CONFIRM);
        rsb_open_store();
    } else if (on_back_arrow(p) || outside_box(p)) {
        panel_close();
    } else if (panel_tour) {
        panel_game = next_locked(panel_game);
        panel_t = 0;
    }
}

static void touch_update(void)
{
    const rsb_pointer *p = rsb_pointer_get();
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
            int corner = p->y0 < GRID_TOP ? (p->x0 < 32 ? SEL_INFO : p->x0 >= W - 32 ? SEL_HELP : 0) : 0;
            if (g >= 0) {
                sel = g;
                activate(g);
            } else if (corner) {                                 /* the round buttons in the header's corners */
                if (sel >= 0) sel_mem = sel;
                sel = corner;
                activate(corner);
            } else if (promo && ty >= 30 - FOOT_ROWS && p->x0 / 8 >= pill_x - 1) {
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
    pad = rsb_gamepad();
    if (rsb_back_pressed()) {
        if (credits) credits_close();
        else if (help) { help = 0; menu_again(); ha_play(HA_PAUSE); }
        else if (panel) panel_close();
        else rsb_quit();
        return;
    }
    if (credits) {
        credits_update(pr, held);
        return;
    }
    if (help) {
        help_update(pr);
        return;
    }
    if (panel) {
        panel_touch();
        panel_t++;
        if (!panel) return;
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
    touch_update();
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

/* the dialogs' back arrow (top left), and the B glyph next to it for pad players */
static void back_arrow(void)
{
    rs_spr(5, CORNER_Y, OT_BACK, 16, 16, OPAL_MINE, 3, 0);
    if (pad) hu_glyph(HU_BTN_B, 4 - hu_ui_x(), CORNER_Y + 18, 0, 3);
}

static void l_draw(void)
{
    if (pad != shown_pad) {                     /* a gamepad came or went: the hints, maybe the footer */
        shown_pad = pad;
        shown_panel = -1;
        shown_sel = -99;
        shown_credits = 0;
        shown_help = 0;
        if (footer_wanted() != foot) {
            layout_footer();
            layer_row[0] = layer_row[1] = layer_row[2] = -1;
            if (sel >= 0) reveal(sel);
        }
    }
    rs_pal_set(RS_PAL_OBJ(OPAL_MINE) + C_GOLDL, hu_lerp_color(RS_RGB8(255, 240, 150), RS_RGB8(255, 255, 255),
                                                             hu_bob(t, 64, 64) + 64));
    if (credits) {
        if (credits != shown_credits) {
            if (credits == 1) credits_list_draw();
            else credits_doc_draw();
            shown_credits = credits;
        }
        rs_oam_clear();
        back_arrow();
        if (credits == 1) {
            if (pad) {
                hu_glyph(HU_BTN_A, (pan_x + 2) * 8 - 18 - hu_ui_x(), cr_row(cr_sel) * 8 - 4, 0, 3);
                hu_glyph(HU_BTN_A, ((Wt - hu_width(TR(READ), 0)) / 2 + 1) * 8 - 18 - hu_ui_x(), CR_HINT_ROW * 8 - 4, 0, 3);
            }
        } else {
            doc_layers();
            if (pad) hu_glyph(HU_BTN_B, 4 * 8 - 18 - hu_ui_x(), FOOT_TEXT_ROW * 8 - 4, 0, 3);
        }
        shown_panel = -1;
        return;
    }
    shown_credits = 0;
    if (help) {
        if (!shown_help) {
            help_draw();
            shown_help = 1;
        }
        rs_oam_clear();
        back_arrow();
        rs_spr(HELP_ICON_X * 8, 16 * 8 - 4, OT_PAUSE_ICON, 16, 16, OPAL_MINE, 3, 0);
        rs_spr(HELP_ICON_X * 8, 20 * 8 - 4, OT_BACK_ICON, 16, 16, OPAL_MINE, 3, 0);
        shown_panel = -1;
        return;
    }
    shown_help = 0;
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
        back_arrow();
        if (pad) hu_glyph(HU_BTN_A, btn_x * 8 - 18 - hu_ui_x(), btn_y * 8 - 4, 0, 3);   /* the kit adds its UI offset */
        return;
    }
    grid_layers();
    /* the selection frame (gold, glowing), the padlocks, the round buttons */
    if (sel >= 0) {
        int x, y;
        card_pos(sel, &x, &y);
        frame_sprites(x, y);
    }
    if (pad && foot) hu_glyph(HU_BTN_A, 8 - hu_ui_x(), FOOT_TEXT_ROW * 8 - 4, 0, 3);
    rs_spr(5, CORNER_Y, OT_INFO + (sel == SEL_INFO ? 4 : 0), 16, 16, OPAL_MINE, 3, 0);       /* "i": the credits */
    rs_spr(W - 21, CORNER_Y, OT_HELP + (sel == SEL_HELP ? 4 : 0), 16, 16, OPAL_MINE, 3, 0);  /* "?": the help */
    for (int g = 0; g < rsb_ngames; g++) {
        if (!locked(g)) continue;
        int x, y;
        card_pos(g, &x, &y);
        if (y + RSB_THUMB_H <= GRID_TOP - 8 || y >= grid_bottom) continue;
        rs_spr(x + RSB_CARD_W / 2 - 8, y + RSB_THUMB_H / 2 - 8, OT_LOCK, 16, 16, OPAL_MINE, 2, 0);
    }
}

const rs_game *rsb_launcher_game(void)
{
    static const rs_game g = {"RetroStone Arcade", "launcher", "0.1.0", l_init, l_update, l_draw, NULL, NULL,
                              NULL, NULL, 0, RS_SCREEN_W_MAX};
    return &g;
}
