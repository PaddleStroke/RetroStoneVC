/*
 * Jelly Jam: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1); BG2 the fields' jellies, PAINTED BY SOFTWARE: each field is a picture of
 *   8x8 tiles (its inside, its frame's width round it and a 16-px band above it for the pair coming in), redrawn every
 *   frame from the world (the jellies at their pixel heights as they fall, joined to their neighbours of the same
 *   colour, squashing as they land, flashing and popping as they melt) and uploaded where it changed; a tile with no
 *   jelly is the blank tile 0, the others come from a pool handed out as needed (so the VRAM used follows the jellies on
 *   screen, not the fields' size); BG3 the kitchen: the tiled wall, the fields' jar frames and their dark insides, the
 *   wooden board between two fields, shelves of jam jars on wide screens; the backdrop dark plum (the frames' corners).
 *   Sprites: the next pairs, the rain queues, jam droplets, round stars (OBJ 6, the jelly palette), the heroes (OBJ 0,
 *   1, 4, 5: the players' palettes), the chef of the duel (OBJ 7, loaded with the chef), the kit's digits, glyphs and
 *   medals (OBJ 3).
 * One palette for every jelly (BG 3: an outline, white, four colours of three shades and the rain's grey): the fields
 * of 3-4 players have 12-px cells, not on the 8-px tile grid, so a tile may hold several colours.
 * Widescreen (docs/spec.md "Widescreen"): the 320-px stage is centred; the fields keep their places and size, a wider
 * screen shows more kitchen (shelves of jars) and, from 427 columns, the hero and the chef move out beside the fields.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/jellyjam/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_tall.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

const char *const chef_names[CHEFS] = {"PIP", "DOT", "HOPSY", "MOCHI", "TRUFFLE", "BASIL", "BAO", "BRUNO"};

/* the title logo: JELLY in strawberry, JAM in apricot (light, mid, dark) */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 210, 226), RS_RGB8(246, 116, 160), RS_RGB8(204, 52, 108)},
    {RS_RGB8(255, 236, 160), RS_RGB8(250, 178, 60), RS_RGB8(214, 110, 30)}};

#define RW_MAX 13               /* a field's picture: tiles across (2 fields: 13, 3-4: 10) ... */
#define RH_MAX 27               /* ... and down (27, 21) */
#define BAND   16               /* px above the frame: the hidden row, the pair coming in */
#define DROPS_MAX 40            /* jam droplets on screen at most */

typedef struct region { int x0, y0, tw, th, ix, iy, cell, w, h; } region;   /* a field's picture, stage px */

static uint8_t pic[RH_MAX][RW_MAX][64];                     /* the picture being painted (scratch) */
static uint8_t shown_pic[MAX_FIELDS][RH_MAX][RW_MAX][64];  /* what each of its VRAM tiles holds (a cache) */
static uint16_t fslot[MAX_FIELDS][RH_MAX][RW_MAX];         /* each picture tile's BG2 tile, 0: blank */
static uint8_t slot_used[FIELD_TILES];                     /* the BG2 tiles handed out */
static uint8_t cache_ok[MAX_FIELDS];                       /* shown_pic is right (not after a state load) */
static int laid_key = -1;                                  /* the layout the maps were set up for */
static int laid_width;                                     /* ... and the width (state_audit.txt) */
static int laid_height;                                    /* ... and the height */
static int chef_loaded = -1;                               /* the chef in the OBJ slot */
static int shown_key = -1, shown_best = -1;
static uint32_t shown_text;
static int text_paused;

static int map_w(void) { return rs_screen_width() > 512 ? 128 : 64; }
static int fdiv8(int v) { return v >= 0 ? v / 8 : -((7 - v) / 8); }

/* A tall screen shorter than PL_MIN_H (below: portrait; DESIGN.md "Tall screen"): the stage's 240 lines in the middle,
 * the kitchen wall going on above and below it, with two shelves of jam jars. BG3's map is 64 rows (512 px) there: the
 * stage's rows 0-31 as before, rows 32-63 the wall and the shelves (TALL_SHELF_A, _B), which the lines past the stage
 * show by themselves (above: 512 + y, rows 63 upward; below: rows 30 on), the wall's pattern seamless. The raster hides
 * the UI, the fields and the sprites there (window 1 over the whole width); line 0 puts them back. */
#define TALL_SHELF_A 42         /* its lip's row: 96 px below the stage, 176 above it */
#define TALL_SHELF_B 56         /* 64 px above the stage, 208 below it */
static int tall(void) { return rs_screen_height() > RS_SCREEN_H; }
static int bg3_rows(void) { return tall() ? 64 : 32; }

/* Portrait (the zoomed screen, rs_screen_zoom(ZOOM_H) in main.c; DESIGN.md "Tall screen"): a screen of PL_MIN_H lines
 * or more gets the portrait layout: the two jars side by side in the middle of the columns shown (no board between
 * them), the HUD on a wooden board above them (the scores, the hero and the chef, the next pairs, the level and the
 * chef's name or the players' stars), the jars standing on a counter, jam jars on shelves below; the title, the
 * banners and the panels laid out inside the columns shown (the logo on two lines, the long lines wrapped). The stage
 * keeps its 240 lines, PL_ABOVE lines below the screen's top. BG3's 64 rows then map one to one to the screen's
 * lines (rows 51-63 above the stage, 30-50 below). 3-4 players need every column: their play shows the whole screen
 * (rs_screen_fill(0)); their title and results stay zoomed. A shorter tall screen keeps the v1 margins. */
#define PL_ABOVE      104       /* the stage's top, screen line */
#define PL_X0         60        /* the two fields' insides (frames: 56..160, 160..264: 208 columns) */
#define PL_X1         164
#define PL_BOARD_TOP  24        /* the HUD board: its lip's screen line (BG3 row 54) ... */
#define PL_BOARD_ROW  ((512 - PL_ABOVE + PL_BOARD_TOP) / 8)
#define PL_PIN_SY     52        /* BG1's rows 0-3 (the HUD's words) drawn from this screen line (rs_bg_pin) */
#define PL_PIN_N      32
#define PL_MID        160       /* the middle of the screen (and of the two jars) */
static int portrait(void) { return rs_screen_height() >= PL_MIN_H; }
static int pl_pin;              /* the HUD's band is pinned this frame (the raster shows BG1 there) */
static int pl_on;               /* portrait this frame (the raster: the sprites everywhere) */
static int ui_pin;              /* the UI layer pinned on the screen (house_tall.h ht_pin: the title, the panels) */

static void raster(int line, void *user)
{
    (void)user;
    if (pl_on) {                /* portrait: BG1 and BG2 past the stage hidden (but the HUD's band); sprites shown */
        int s = line - rs_screen_top();
        if (line == rs_screen_top() || line == RS_SCREEN_H) {
            rs_window(0, rs_screen_left(), rs_screen_right());
            rs_bg_window(RS_BG1, RS_WIN1);
            rs_bg_window(RS_BG2, RS_WIN1);
            rs_obj_window(0);
        }
        if (pl_pin && line < 0 && s == PL_PIN_SY) rs_bg_window(RS_BG1, 0);
        if (pl_pin && line < 0 && s == PL_PIN_SY + PL_PIN_N) rs_bg_window(RS_BG1, RS_WIN1);
        if (line == 0) {
            rs_bg_window(RS_BG1, 0);
            rs_bg_window(RS_BG2, 0);
        }
        if (ui_pin && (line < 0 || line >= RS_SCREEN_H)) rs_bg_window(RS_BG1, ht_pin_line(ui_pin, line) ? 0 : RS_WIN1);
        return;
    }
    if ((line < 0 && line == rs_screen_top()) || line == RS_SCREEN_H) {
        rs_window(0, rs_screen_left(), rs_screen_right());
        rs_bg_window(RS_BG1, RS_WIN1);
        rs_bg_window(RS_BG2, RS_WIN1);
        rs_obj_window(RS_WIN1);
    } else if (line == 0) {     /* always: a state saved on a tall screen may hold the margins' windows */
        rs_bg_window(RS_BG1, 0);
        rs_bg_window(RS_BG2, 0);
        rs_obj_window(0);
    }
    if (ui_pin && (line < 0 || line >= RS_SCREEN_H)) rs_bg_window(RS_BG1, ht_pin_line(ui_pin, line) ? 0 : RS_WIN1);   /* the pinned UI */
}

/* ---- set-up ------------------------------------------------------------------------------------------------------------ */
static void maps_setup(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG2, 64, 32, VR_BG2);
    rs_bg_setup(RS_BG3, mw, bg3_rows(), VR_BG3);
    memset(fslot, 0, sizeof fslot);
    memset(slot_used, 0, sizeof slot_used);
    memset(cache_ok, 0, sizeof cache_ok);
    laid_key = -1;
    shown_key = -1;
    shown_text = 0;
    laid_width = rs_screen_width();
    laid_height = rs_screen_height();
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more kitchen on both sides */
    hu_config hc = hu_defaults();           /* BG1 at VRAM 0, the kit at 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;             /* the kit sprites after ours and the chef's slot */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    static const uint8_t blank[64] = {0};
    rs_tiles_load8(VR_BG2, blank, 1);
    rs_tiles_load(VR_BG3, gm_kitchen_tiles, gm_kitchen_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_WALL), gm_kitchen_pals, 16);
    rs_pal_load(RS_PAL_BG(PAL_FIELD), gm_kitchen_pals + 16, 16);
    rs_pal_load(RS_PAL_BG(PAL_JARS), gm_kitchen_pals + 32, 16);
    rs_pal_load(RS_PAL_BG(PAL_JELLY), gm_jelly_pal, 16);
    maps_setup();
    for (int l = 0; l < 3; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* P1, P2 (OBJ 2 unused; the kit loaded OBJ 3) */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 48);     /* P3, P4, the jellies */
    rs_backdrop(RS_HEX(BACKDROP_RGB));
    rs_raster(raster, NULL);                /* (a tall screen's lines past the stage; nothing on a 240-line one) */
}

/* ---- the kitchen (BG3) ---------------------------------------------------------------------------------------------- */
static void kput(int tx, int ty, int kt)
{
    if (ty < 0 || ty >= rs_bg_map_h(RS_BG3) || kt <= 0 || kt >= KT_COUNT) return;
    int mw = rs_bg_map_w(RS_BG3);
    rs_bg_put(RS_BG3, tx & (mw - 1), ty, (uint16_t)RS_MAP(kt, gm_kitchen_pal_of[kt], 0, 0, 0));
}

static void wall(int tx0, int tx1, int ty0, int ty1)
{
    static const int wt[2][2][2] = {{{KT_WALL0_00, KT_WALL0_01}, {KT_WALL0_10, KT_WALL0_11}},
                                    {{KT_WALL1_00, KT_WALL1_01}, {KT_WALL1_10, KT_WALL1_11}}};
    for (int ty = ty0; ty < ty1; ty++)
        for (int tx = tx0; tx < tx1; tx++) kput(tx, ty, wt[((tx >> 1) + (ty >> 1)) & 1][ty & 1][tx & 1]);
}

/* a jam jar (kind j) at columns tx, tx + 1 standing on the wood whose lip is row ty */
static void jar_at(int tx, int ty, int j)
{
    static const int jt[3][6] = {{KT_JAR0_00, KT_JAR0_01, KT_JAR0_10, KT_JAR0_11, KT_JAR0_20, KT_JAR0_21},
                                 {KT_JAR1_00, KT_JAR1_01, KT_JAR1_10, KT_JAR1_11, KT_JAR1_20, KT_JAR1_21},
                                 {KT_JAR2_00, KT_JAR2_01, KT_JAR2_10, KT_JAR2_11, KT_JAR2_20, KT_JAR2_21}};
    for (int k = 0; k < 6; k++) kput(tx + (k & 1), ty - 3 + k / 2, jt[j][k]);
}

/* a wooden shelf (its lip at row ty) from column tx0 to tx1, with jars standing on it every 3 columns */
static void shelf(int tx0, int tx1, int ty, int jars)
{
    for (int tx = tx0; tx < tx1; tx++) {
        kput(tx, ty, KT_WOOD_LIP);
        kput(tx, ty + 1, KT_WOOD_UNDER);
    }
    if (!jars) return;
    for (int tx = tx0 + 1, j = ((tx0 * 7 + ty) % 3 + 3) % 3; tx + 2 <= tx1; tx += 3, j = (j + 1) % 3) jar_at(tx, ty, j);
}

/* the shelves in the screen's margins (wider than 320) */
static void side_shelves(const int rows[], int n, int bare)   /* bare: the shelf (index) the characters stand on */
{
    int left = fdiv8(rs_screen_left()), right = fdiv8(rs_screen_right() + 7);
    for (int i = 0; i < n; i++) {
        if (left < 0) {
            shelf(left, 0, rows[i], i != bare);
            kput(left - 1, rows[i], KT_WOOD_LIP);   /* on past the edge (a shake) */
            kput(left - 1, rows[i] + 1, KT_WOOD_UNDER);
        }
        if (right > 40) {
            shelf(40, right, rows[i], i != bare);
            kput(right, rows[i], KT_WOOD_LIP);
            kput(right, rows[i] + 1, KT_WOOD_UNDER);
        }
    }
}

/* portrait: a counter's top (its lip at row ty) and its front of boards down to row ty1 - 1, its underside after */
static void counter(int left, int right, int ty, int ty1)
{
    for (int tx = left - 1; tx <= right; tx++)
        for (int y = ty; y <= ty1; y++)
            kput(tx, y, y == ty ? KT_WOOD_LIP : y == ty1 ? KT_WOOD_UNDER : (tx + y) % 4 ? KT_WOOD_BOARD : KT_WOOD_BOARD2);
}

/* portrait: the kitchen past the stage. Above (rows 51-63): the HUD board in play (2 fields), else two shelves of
 * jars; below (rows 30-50): the counter (the jars stand on it), then two shelves of jars (rows 40, 48) */
static void kitchen_portrait(int nf, int title, int left, int right)
{
    if (!title && nf <= 2) {                       /* the HUD board above the jars, as wide as them */
        int bx0 = (PL_X0 - 4) / 8, bx1 = (PL_X1 + COLS * LAYOUT_CELL_BIG + 4) / 8 - 1;
        for (int ty = PL_BOARD_ROW; ty <= 64; ty++)
            for (int tx = bx0; tx <= bx1; tx++)
                kput(tx, ty & 63, ty == 64 ? KT_WOOD_UNDER : ty == PL_BOARD_ROW ? KT_WOOD_LIP : tx == bx0 ? KT_WOOD_EDGE_L
                                  : tx == bx1 ? KT_WOOD_EDGE_R : ((tx + ty) % 5 ? KT_WOOD_BOARD : KT_WOOD_BOARD2));
    } else {
        shelf(left - 1, right + 1, 54, 1);
        shelf(left - 1, right + 1, 61, 1);
    }
    if (title || nf <= 2) counter(left, right, 30, 34);
    else counter(left, right, (LAYOUT34_Y + LAYOUT_CELL_SMALL * VROWS + 4) / 8, 32);   /* (3-4 jars: on it too) */
    shelf(left - 1, right + 1, 40, 1);
    shelf(left - 1, right + 1, 48, 1);
}

static void kitchen(int nf, int title)
{
    int left = fdiv8(rs_screen_left()), right = fdiv8(rs_screen_right() + 7);
    rs_bg_fill(RS_BG3, 0);
    wall(left - 1, right + 1, 0, 32);              /* a tile more all round: a shake shows no backdrop at the edges */
    if (tall()) wall(left - 1, right + 1, 32, 64); /* a tall screen: the wall past the stage ... */
    if (portrait()) kitchen_portrait(nf, title, left, right);
    else if (tall()) {                             /* ... and two shelves of jars */
        shelf(left - 1, right + 1, TALL_SHELF_A, 1);
        shelf(left - 1, right + 1, TALL_SHELF_B, 1);
    }
    if (title && portrait()) return;               /* (portrait: the counter below the stage, under the credits) */
    if (title) {                                   /* a counter at the bottom, jars on it at both ends (mirrored) */
        for (int tx = left - 1; tx <= right; tx++) {
            kput(tx, 27, KT_WOOD_LIP);
            kput(tx, 28, (tx & 3) ? KT_WOOD_BOARD : KT_WOOD_BOARD2);
            kput(tx, 29, KT_WOOD_BOARD);
        }
        for (int k = 0; 1 - 3 * k >= left; k++) {
            jar_at(1 - 3 * k, 27, k % 3);
            jar_at(37 + 3 * k, 27, (k + 1) % 3);
        }
        static const int rows[2] = {12, 20};
        side_shelves(rows, 2, -1);
        return;
    }
    flayout L[MAX_FIELDS];
    int n = screen_layout(nf, L);
    for (int i = 0; i < n; i++) {                  /* the jar frames round the fields, their insides */
        int tx0 = (L[i].x - 4) / 8, tx1 = (L[i].x + L[i].w + 4) / 8 - 1, ty0 = (L[i].y - 4) / 8,
            ty1 = (L[i].y + L[i].h + 4) / 8 - 1;
        for (int ty = ty0; ty <= ty1; ty++)
            for (int tx = tx0; tx <= tx1; tx++) {
                int fx = tx == tx0 ? 0 : tx == tx1 ? 2 : 1, fy = ty == ty0 ? 0 : ty == ty1 ? 2 : 1;
                static const int rt[3][3] = {{KT_RING_TL, KT_RING_T, KT_RING_TR}, {KT_RING_L, KT_RING_IN, KT_RING_R},
                                             {KT_RING_BL, KT_RING_B, KT_RING_BR}};
                kput(tx, ty, rt[fy][fx]);
            }
    }
    if (nf <= 2 && portrait()) {
        /* (portrait: no board between the jars; the HUD board above them) */
    } else if (nf <= 2) {                          /* the wooden board between the two fields */
        int bx0 = (L[0].x + L[0].w + 4) / 8 + 1, bx1 = (L[1].x - 4) / 8 - 1;
        for (int ty = 5; ty < 30; ty++)
            for (int tx = bx0; tx < bx1; tx++)
                kput(tx, ty, ty == 5 ? KT_WOOD_LIP : tx == bx0 ? KT_WOOD_EDGE_L : tx == bx1 - 1 ? KT_WOOD_EDGE_R
                             : ((tx + ty) % 5 ? KT_WOOD_BOARD : KT_WOOD_BOARD2));
        static const int rows[3] = {12, 20, 28};    /* the hero and the chef stand on the middle one */
        side_shelves(rows, 3, nf <= 2 ? 1 : -1);
    } else {
        static const int rows[3] = {11, 19, 27};
        side_shelves(rows, 3, nf <= 2 ? 1 : -1);
    }
}

/* the fields as shown: field_layout, but portrait puts the two side by side (draw, touch, the sounds' pan) */
int screen_layout(int nf, flayout out[MAX_FIELDS])
{
    int n = field_layout(nf, out);
    if (n == 2 && portrait()) {
        out[0].x = PL_X0;
        out[1].x = PL_X1;
    }
    return n;
}

/* ---- the fields' pictures (BG2) ----------------------------------------------------------------------------------- */
static void region_of(const flayout *l, region *r)
{
    r->cell = l->cell;
    r->w = l->w;
    r->h = l->h;
    r->x0 = l->x - 4;
    r->y0 = l->y - 4 - BAND;
    r->tw = (l->w + 8) / 8;
    r->th = (l->h + 8 + BAND) / 8;
    r->ix = 4;
    r->iy = 4 + BAND;
}

static int clip_y = 9999;                       /* painting: no pixel at or below this picture row (a lost field) */
static int pic_w, pic_h;

static inline void put(int x, int y, uint8_t v)
{
    if ((unsigned)x < (unsigned)pic_w && (unsigned)y < (unsigned)pic_h && y < clip_y)
        pic[y >> 3][x >> 3][((y & 7) << 3) | (x & 7)] = v;
}

/* a jelly of S px at (x, y): its body (joins) and face; flash: white; squash: px shorter (anchored at the bottom);
 * size < S: shrunk (a pop), centred */
static void paint_jelly(int S, int colour, int mask, int mood, int x, int y, int flash, int squash, int size)
{
    const uint8_t *b = (S == 16 ? gm_jelly16 : gm_jelly12) + (colour * 16 + (colour < 4 ? mask : 0)) * S * S;
    const uint8_t *f = (S == 16 ? gm_face16 : gm_face12) + (colour * 3 + mood) * S * S;
    if (size <= 0) return;
    if (size < S) {
        int o = (S - size) / 2;
        for (int dy = 0; dy < size; dy++)
            for (int dx = 0; dx < size; dx++) {
                int sx = dx * S / size, sy = dy * S / size, v = f[sy * S + sx] ? f[sy * S + sx] : b[sy * S + sx];
                if (v) put(x + o + dx, y + o + dy, (uint8_t)(flash && v != JELLY_OUTLINE ? JELLY_WHITE : v));
            }
        return;
    }
    int hgt = S - squash;
    for (int dy = 0; dy < hgt; dy++) {
        int sy = squash ? dy * S / hgt : dy;
        const uint8_t *br = b + sy * S, *fr = f + sy * S;
        for (int dx = 0; dx < S; dx++) {
            int v = fr[dx] ? fr[dx] : br[dx];
            if (v) put(x + dx, y + squash + dy, (uint8_t)(flash && v != JELLY_OUTLINE ? JELLY_WHITE : v));
        }
    }
}

/* the child's place round the pivot: 16 steps a turn (0 up, 4 right, 8 down, 12 left), x and y in 1/64 of a cell */
static const int8_t circ_x[16] = {0, 24, 45, 59, 64, 59, 45, 24, 0, -24, -45, -59, -64, -59, -45, -24};
static const int8_t circ_y[16] = {-64, -59, -45, -24, 0, 24, 45, 59, 64, 59, 45, 24, 0, -24, -45, -59};

static void paint_field(const world *w, int fi, const region *R)
{
    const field *f = &w->f[fi];
    const int S = R->cell;
    int t = w->t, lost_dy = 0;
    memset(pic, 0, sizeof pic);
    pic_w = R->tw * 8;
    pic_h = R->th * 8;
    clip_y = 9999;
    if (f->state == FS_LOST || (!f->alive && w->started)) {      /* a lost field: everything slides out of the jar */
        lost_dy = f->lost_t * f->lost_t / 5;
        clip_y = R->iy + R->h;
        if (lost_dy > R->h + S * 2) return;
    }
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int k = f->g[r][c];
            if (!k) continue;
            int x = R->ix + c * S, y = R->iy + (VROWS - 1 - r) * S - f->off[r][c] * S / CQ + lost_dy;
            if (y < 0) continue;            /* rain falling from above the picture: it comes in whole, not sliced */
            int mask = 0;
            if (k <= COLOURS && f->off[r][c] == 0 && r < VROWS) {
                static const int8_t dr[4] = {1, 0, -1, 0}, dc[4] = {0, 1, 0, -1};
                for (int d = 0; d < 4; d++) {
                    int nr = r + dr[d], nc = c + dc[d];
                    if (nr >= 0 && nr < VROWS && nc >= 0 && nc < COLS && f->g[nr][nc] == k && f->off[nr][nc] == 0 &&
                        f->melt[nr][nc] == f->melt[r][c])
                        mask |= 1 << d;
                }
            }
            int mood = (t + c * 37 + r * 53) % 230 < 7 ? 1 : 0, flash = 0, size = S;
            if (lost_dy) mood = 1;
            if (f->melt[r][c] && f->state == FS_MELT) {
                mood = 2;
                if (f->t < MELT_FLASH) flash = (f->t / 4) & 1;
                else size = S * (MELT_FRAMES - f->t) / (MELT_FRAMES - MELT_FLASH);
            }
            int squash = f->sq[r][c] ? (f->sq[r][c] * 2 + SQUASH_FRAMES - 1) / SQUASH_FRAMES : 0;
            paint_jelly(S, k - 1, mask, mood, x, y, flash, squash, size);
        }
    if (f->state == FS_PAIR) {                                     /* the pair: the child swings as it turns */
        const pair_t *p = &f->pr;
        int px = R->ix + p->col * S, py = R->iy + VROWS * S - (int)(p->y * S / CQ) - S;
        if (p->slide_t < SLIDE_ANIM) px -= p->slide * S * (SLIDE_ANIM - p->slide_t) / SLIDE_ANIM;
        int a = p->rot * 4;
        if (p->rot_t < ROT_ANIM) {
            int a0 = p->rot_from * 4, d = ((a - a0 + 8) & 15) - 8;
            a = (a0 + d * p->rot_t / ROT_ANIM) & 15;
        }
        int cx = px + circ_x[a] * S / 64, cy = py + circ_y[a] * S / 64;
        int m0 = 0, m1 = 0;
        if (p->c0 == p->c1 && p->rot_t >= ROT_ANIM && !(p->slide_t < SLIDE_ANIM)) {
            static const int8_t to[4] = {1, 2, 4, 8}, from[4] = {4, 8, 1, 2};
            m0 = to[p->rot];
            m1 = from[p->rot];
        }
        paint_jelly(S, p->c0 - 1, m0, 0, px, py, 0, 0, S);
        paint_jelly(S, p->c1 - 1, m1, 0, cx, cy, 0, 0, S);
    }
}

static int alloc_slot(void)
{
    for (int i = 1; i < FIELD_TILES; i++)
        if (!slot_used[i]) {
            slot_used[i] = 1;
            return i;
        }
    return 0;
}

static void free_field(int fi, const region *R)
{
    uint16_t *map = rs_bg_map(RS_BG2);
    for (int ty = 0; ty < RH_MAX; ty++)
        for (int tx = 0; tx < RW_MAX; tx++) {
            uint16_t *s = &fslot[fi][ty][tx];
            if (!*s) continue;
            slot_used[*s] = 0;
            *s = 0;
            if (R) map[(((R->y0 >> 3) + ty) & 31) * 64 + (((R->x0 >> 3) + tx) & 63)] = 0;
        }
}

/* upload field fi's picture: the tiles with jellies get (or keep) a BG2 tile, the empty ones go back to blank */
static void upload_field(int fi, const region *R)
{
    uint16_t *map = rs_bg_map(RS_BG2);
    for (int ty = 0; ty < R->th; ty++)
        for (int tx = 0; tx < R->tw; tx++) {
            const uint8_t *p = pic[ty][tx];
            uint16_t *s = &fslot[fi][ty][tx];
            uint16_t *e = &map[(((R->y0 >> 3) + ty) & 31) * 64 + (((R->x0 >> 3) + tx) & 63)];
            uint64_t any = 0;
            for (int i = 0; i < 8; i++) {
                uint64_t q;
                memcpy(&q, p + i * 8, 8);
                any |= q;
            }
            if (!any) {
                if (*s) {
                    slot_used[*s] = 0;
                    *s = 0;
                    *e = 0;
                }
                continue;
            }
            int fresh = 0;
            if (!*s) {
                *s = (uint16_t)alloc_slot();
                if (!*s) continue;                  /* the pool is full: this tile waits */
                *e = (uint16_t)RS_MAP(*s, PAL_JELLY, 0, 0, 0);
                fresh = 1;
            }
            if (fresh || !cache_ok[fi] || memcmp(shown_pic[fi][ty][tx], p, 64)) {
                rs_tiles_load8(VR_BG2 + *s, p, 1);
                memcpy(shown_pic[fi][ty][tx], p, 64);
            }
        }
    cache_ok[fi] = 1;
}

/* the maps for this screen (the title, or nf fields): set up again when it changes */
static void layout(const world *w, int title)
{
    int key = title ? 0 : w->nf;
    if (key == laid_key && laid_width == rs_screen_width() && laid_height == rs_screen_height()) return;
    if (laid_width != rs_screen_width() || laid_height != rs_screen_height()) maps_setup();
    flayout L[MAX_FIELDS];
    int n = screen_layout(laid_key > 0 ? laid_key : 0, L);
    for (int i = 0; i < MAX_FIELDS; i++) {
        region R;
        if (i < n) region_of(&L[i], &R);
        free_field(i, i < n ? &R : NULL);
    }
    rs_bg_fill(RS_BG2, 0);
    kitchen(title ? 0 : w->nf, title);
    laid_key = key;
}

/* ---- sprites ------------------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= rs_screen_top() - (int)d->h || y >= rs_screen_bottom()) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, 0);
}

static int player_pal(int p) { return p ? hu_player_pal(p) : 0; }

static void chef_load(int level)
{
    int i = (level - 1) % CHEFS;
    if (i == chef_loaded) return;
    rs_tiles_load(VR_OBJ + CHEF_TILE, gm_chef_tiles + i * CHEF_TILES * 32, CHEF_TILES);
    rs_pal_load(RS_PAL_OBJ(7), gm_chef_pals + i * 16, 16);
    chef_loaded = i;
}

static void chef_spr(int frame, int x, int y)
{
    if (x <= rs_screen_left() - 32 || x >= rs_screen_right()) return;
    rs_spr(x, y, CHEF_TILE + frame * 16, 32, 32, 7, 2, 0);
}

static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr(SPR_HERO_ICON, cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), 2, player_pal(p));
}

/* the rain coming to a field: the biggest icons first, 6 at most, from the field's left */
static void rain_icons(const field *f, int x, int y, int maxw)
{
    static const int unit[5] = RAIN_ICONS;
    static const int ids[5] = {SPR_RAIN_CROWN, SPR_RAIN_JAR, SPR_RAIN_LUMP, SPR_RAIN_BIG, SPR_RAIN_SMALL};
    int n = f->rain_q, k = 0, cx = x;
    while (n > 0 && k < 6) {
        int i = 0;
        while (i < 4 && n < unit[i]) i++;
        const gm_sprite_def *d = &gm_spr[ids[i]];
        if (cx + d->w > x + maxw) break;
        spr(ids[i], cx, y + (16 - d->h) / 2, 1, -1);
        cx += d->w - (i == 4 ? 0 : 2);
        n -= unit[i];
        k++;
    }
}

/* the next pair: the child above the pivot */
static void next_pair(const world *w, const field *f, int x, int y, int S)
{
    uint8_t c0, c1;
    seq_pair(w->seq_seed, f->seq, &c0, &c1);
    int id = S == 16 ? SPR_JELLY16_0 : SPR_JELLY12_0;
    spr(id + c1 - 1, x, y, 2, -1);
    spr(id + c0 - 1, x, y + S, 2, -1);
}

/* jam droplets from the last pop: they fly up and fall (a function of the pop's age: no state) */
static int drops_left;
static void droplets(const field *f, const region *R)
{
    int age = f->pop_age, S = R->cell;
    if (age >= 36 || f->state == FS_LOST) return;
    for (int i = 0; i < f->npops && drops_left > 0; i++) {
        const pop_t *p = &f->pops[i];
        int cx = R->x0 + R->ix + p->col * S + S / 2 - 4, cy = R->y0 + R->iy + (VROWS - 1 - p->row) * S + S / 2 - 4;
        for (int j = 0; j < 2 && drops_left > 0; j++) {
            int vx = ((i * 7 + j * 5) % 9) - 4 + (j ? 2 : -2), vy = -6 - (i + j * 3) % 4;     /* px per 4 frames */
            int x = cx + vx * age / 4, y = cy + vy * age / 4 + age * age / 24;
            if (y > R->y0 + R->iy + R->h) continue;
            if (x < R->x0 - 2 || x > R->x0 + R->tw * 8 - 6) continue;      /* not over a neighbour's jar */
            spr((age < 18 ? SPR_DROP_0 : SPR_DROPSM_0) + p->colour - 1, x, y, 2, -1);
            drops_left--;
        }
    }
}

/* ---- the UI's text (BG1): what this frame shows; drawn again only when it changes ------------------------------- */
typedef struct ptext { int x, y, wrap; char s[48]; } ptext;     /* wrap: centred and wrapped in the columns shown */
typedef struct uiplan { char banner[48]; int banner_row; ptext t[16]; int n; } uiplan;

static void add_text(uiplan *u, int x, int y, const char *s)
{
    if (u->n >= 16) return;
    u->t[u->n].x = x;
    u->t[u->n].y = y;
    u->t[u->n].wrap = 0;
    snprintf(u->t[u->n].s, sizeof u->t[u->n].s, "%s", s);
    u->n++;
}

static void add_centred(uiplan *u, int x0, int w, int y, const char *s) { add_text(u, fdiv8(x0 + w / 2) - hu_width(s, 0) / 2, y, s); }

/* a line centred on the screen: wrapped in the columns shown on a zoomed tall screen (house_tall.h); returns the next
 * free row */
static int add_line(uiplan *u, int y, const char *s)
{
    if (!ht_narrow()) {
        add_centred(u, 0, RS_SCREEN_W, y, s);
        return y + 1;
    }
    add_text(u, 0, y, s);
    u->t[u->n - 1].wrap = 1;
    return y + ht_text_rows(s, 0);
}

static void banner(int y, const char *s)
{
    if (ht_narrow()) ht_banner(y, s);
    else hu_banner(y, s);
}

static uint32_t hash_plan(const uiplan *u, int extra)
{
    uint32_t h = 2166136261u + (uint32_t)extra * 31u + (uint32_t)u->banner_row;
    for (const char *s = u->banner; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    for (int i = 0; i < u->n; i++) {
        h = (h ^ (uint32_t)(u->t[i].x * 131 + u->t[i].y)) * 16777619u;
        for (const char *s = u->t[i].s; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    }
    return h;
}

static void play_texts(const world *w, const flayout *L, int side)
{
    uiplan u;
    char s[48];
    memset(&u, 0, sizeof u);
    if (w->phase == PH_INTRO) {
        if (w->phase_t >= INTRO_FRAMES - GO_FRAMES) snprintf(u.banner, sizeof u.banner, "%s", HTR(GO));
        else if (w->mode == MODE_DUEL) rs_strf(u.banner, sizeof u.banner, TR(LEVEL_N), w->level);
        else rs_strf(u.banner, sizeof u.banner, TR(ROUND_N), w->round);
        u.banner_row = 8;
        if (w->mode == MODE_DUEL && w->phase_t < INTRO_FRAMES - GO_FRAMES) {
            rs_strf(s, sizeof s, TR(VS_NAME), chef_names[(w->level - 1) % CHEFS]);
            add_line(&u, 13, s);
        }
        if (w->mode == MODE_DUEL && w->level == 1 && w->duels_won == 0) {
            int r = add_line(&u, 19, TR(HINT_MOVE));
            add_line(&u, r < 21 ? 21 : r + 1, TR(HINT_TURN));
        }
    } else if (w->phase == PH_END) {
        if (w->mode == MODE_DUEL) snprintf(u.banner, sizeof u.banner, "%s", TR(YOU_WIN));
        else if (w->winner >= 0) rs_strf(u.banner, sizeof u.banner, HTR(WINS), w->winner + 1);
        else snprintf(u.banner, sizeof u.banner, "%s", HTR(DRAW));
        u.banner_row = 9;
    }
    for (int i = 0; i < w->nf; i++) {
        const field *f = &w->f[i];
        if (f->chain_shown >= 2 && f->chain_t < 60 && w->phase != PH_INTRO) {     /* "3 CHAIN!" over the melt */
            int row = fdiv8(L[i].y + (VROWS - 1 - f->chain_row) * L[i].cell) - 1;
            if (row < 1) row = 1;
            rs_strf(s, sizeof s, TR(CHAIN_N), f->chain_shown);
            add_centred(&u, L[i].x, L[i].w, row, s);
        }
        if (w->nf > 2) {                                    /* 3-4 players: the tag and the score above each field */
            rs_strf(s, sizeof s, HTR(PLAYER_TAG), i + 1);
            add_text(&u, fdiv8(L[i].x), 1, s);
            snprintf(s, sizeof s, "%d", f->score);
            add_text(&u, fdiv8(L[i].x + L[i].w) - hu_width(s, 0), 1, s);
        }
    }
    if (w->nf == 2 && portrait()) {                         /* portrait: the HUD board's words (rows 0-3, pinned) */
        if (w->mode == MODE_DUEL) {
            rs_strf(s, sizeof s, TR(LEVEL_N), w->level);
            add_centred(&u, 0, RS_SCREEN_W, 1, s);
            add_centred(&u, 0, RS_SCREEN_W, 3, chef_names[(w->level - 1) % CHEFS]);
        } else {                                            /* the tags over each player's stars */
            for (int p = 0; p < 2; p++) {
                rs_strf(s, sizeof s, HTR(PLAYER_TAG), p + 1);
                add_text(&u, fdiv8((p ? PL_MID + 20 : PL_MID - 20) - hu_width(s, 0) * 4 + 4), 1, s);
            }
        }
    } else if (w->nf == 2 && w->phase != PH_INTRO) {        /* the board between: who plays */
        int bx = L[0].x + L[0].w + 4, bw = L[1].x - 4 - bx;
        if (w->mode == MODE_DUEL) {
            const char *name = chef_names[(w->level - 1) % CHEFS];
            if (side) add_centred(&u, L[1].x + L[1].w + 8, rs_screen_right() - (L[1].x + L[1].w + 8), 22, name);
            else add_centred(&u, bx, bw, 19, name);
            rs_strf(s, sizeof s, TR(LEVEL_N), w->level);
            add_centred(&u, bx, bw, side ? 15 : 21, s);
        } else {                                            /* the tags centred over each player's stars */
            int mid = bx + bw / 2;
            for (int p = 0; p < 2; p++) {
                rs_strf(s, sizeof s, HTR(PLAYER_TAG), p + 1);
                add_text(&u, fdiv8((p ? mid + 18 : mid - 18) - hu_width(s, 0) * 4 + 4), 20, s);
            }
        }
    }
    uint32_t h = hash_plan(&u, text_paused);
    if (h == shown_text) return;
    shown_text = h;
    hu_clear();
    if (u.banner[0]) banner(u.banner_row, u.banner);
    for (int i = 0; i < u.n; i++)
        if (u.t[i].wrap) ht_text(u.t[i].y, u.t[i].s, 0);
        else hu_text(u.t[i].x, u.t[i].y, u.t[i].s);
}

/* the title, the game over and the results */
static void standing(const world *w, hu_standing *s)
{
    int key[MAX_PLAYERS], val[MAX_PLAYERS];
    for (int p = 0; p < w->players && p < MAX_FIELDS; p++) {
        key[p] = w->f[p].wins * 16 + (w->f[p].alive ? 8 : 0) + (MAX_FIELDS + 1 - w->f[p].place);
        val[p] = w->f[p].wins;
    }
    hu_rank(s, w->players, key, val);
}

/* a zoomed tall screen (house_tall.h): the title below the two-line logo; where its glyph and slots went */
static ht_title_out title_out;                  /* set by screen_text every title frame (state_audit.txt) */

static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    int screen = state == ST_DEAD ? ST_PLAY : state, narrow = ht_narrow();
    char note[48];
    rs_strf(note, sizeof note, TR(LEVEL_N), w->level);
    if (screen != shown_key || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best) {
        hu_clear();
        shown_text = 0;
        if (state == ST_TITLE) {
            if (narrow) ht_logo("JELLY JAM", title_ramps, 2, 2, 0);     /* (house_tall.h: its default rows) */
            else hu_logo("JELLY JAM", title_ramps, 2, 2, 0);
        }
        if (state == ST_OVER) {
            if (w->players == 1 && narrow) {
                ht_banner(4, HTR(GAME_OVER));
                ht_gameover_panel(new_best, note, HTR(BACK_TITLE));
            } else if (w->players == 1) {
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->f[0].score, 0);
                hu_gameover_note(note);
            } else if (narrow) {
                ht_results_panel(rs, NULL);
            } else {
                hu_results_panel(rs, NULL);
            }
        }
        shown_key = screen;
        shown_best = best;
    }
    if (state == ST_TITLE && narrow) {
        ht_title cfg = {TR(VERB), MAX_PLAYERS, HT_ROW_SLOTS, 0};
        ht_title_draw(&cfg, st_t, best, &title_out);
    } else if (state == ST_TITLE) {
        hu_title_draw(st_t, best);
    }
    if (state == ST_OVER) {
        if (w->players == 1 && narrow) ht_retry_line_at(ht_gameover_retry_row(note), st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else if (narrow) ht_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
}

/* ---- a frame ------------------------------------------------------------------------------------------------------ */
static int hero_frame(const world *w, int i, int t)
{
    const field *f = &w->f[i];
    if (!f->alive && w->started) return SPR_HERO_SAD;
    if ((w->phase == PH_END || w->phase == PH_OVER) && w->winner == i) return SPR_HERO_HAPPY;
    if (f->chain_shown >= 2 && f->chain_t < 60) return SPR_HERO_HAPPY;
    return (t / 24) & 1 ? SPR_HERO_IDLE1 : SPR_HERO_IDLE0;
}

static int chef_frame(const world *w)
{
    const field *c = &w->f[1];
    return !c->alive ? 2 : (w->lost || (c->chain_shown >= 2 && c->chain_t < 60)) ? 1 : 0;
}

/* wide screens, 2 fields: the hero and the chef (or the two heroes) on the shelves beside the fields; also behind the
 * game-over and results panels, which leave them in sight */
static void side_characters(const world *w, const flayout *L, int t)
{
    int xl = (rs_screen_left() + L[0].x - 4) / 2 - 16;
    int xr = L[1].x + L[1].w + 8 + (rs_screen_right() - (L[1].x + L[1].w + 8)) / 2 - 16;
    if (w->mode == MODE_DUEL) {
        chef_load(w->level);
        chef_spr(chef_frame(w), xr, 128 + hu_bob(t, 64, 2));
        spr(hero_frame(w, 0, t), xl, 128 + hu_bob(t + 20, 64, 2), 2, 0);
    } else {
        for (int i = 0; i < 2; i++) spr(hero_frame(w, i, t), i ? xr : xl, 128 + hu_bob(t + i * 20, 64, 2), 2, player_pal(i));
    }
}

/* portrait, 2 fields: the HUD board above the jars (screen lines PL_BOARD_TOP .. PL_ABOVE): the scores over the jars,
 * the hero and the chef (or the two heroes) at its ends, the next pairs beside them, the rounds won in the middle
 * (the level and the chef's name, or the tags: BG1's pinned rows, play_texts) */
static void hud_board(const world *w, const flayout *L, int t, int nexts)
{
    int y0 = rs_screen_top(), xl = L[0].x + 4, xr = L[1].x + L[1].w - 36;
    for (int i = 0; i < 2; i++) {
        const field *f = &w->f[i];
        hu_number_at(f->score, L[i].x + L[i].w / 2, y0 + 34, 0, 3);
        if (nexts && f->alive && w->phase == PH_PLAY) next_pair(w, f, i ? PL_MID + 40 : PL_MID - 56, y0 + 56, 16);
    }
    if (w->mode == MODE_DUEL) {
        chef_load(w->level);
        chef_spr(chef_frame(w), xr, y0 + 60 + hu_bob(t, 64, 2));
        spr(hero_frame(w, 0, t), xl, y0 + 60 + hu_bob(t + 20, 64, 2), 2, 0);
        return;
    }
    for (int i = 0; i < 2; i++) {
        spr(hero_frame(w, i, t), i ? xr : xl, y0 + 60 + hu_bob(t + i * 20, 64, 2), 2, player_pal(i));
        for (int k = 0; k < WINS_NEEDED; k++)
            spr(w->f[i].wins > k ? SPR_STAR_ON : SPR_STAR_OFF, (i ? PL_MID + 12 : PL_MID - 29) + k * 9, y0 + 77, 2, -1);
    }
}

static void play_sprites(const world *w, const flayout *L, const region *R, int side, int t)
{
    if (w->nf == 2 && portrait()) {
        hud_board(w, L, t, 1);
    } else if (w->nf == 2) {
        int bx = L[0].x + L[0].w + 4, bw = L[1].x - 4 - bx, mid = bx + bw / 2;
        for (int i = 0; i < 2; i++) {
            const field *f = &w->f[i];
            hu_number_at(f->score, L[i].x + L[i].w / 2, 4, 0, 3);
            if (f->alive && w->phase == PH_PLAY) next_pair(w, f, i ? mid + 8 : mid - 24, 48, 16);
        }
        if (side) side_characters(w, L, t);
        if (w->mode == MODE_DUEL) {
            chef_load(w->level);
            if (!side) {
                chef_spr(chef_frame(w), mid - 16, 112 + hu_bob(t, 64, 2));
                spr(hero_frame(w, 0, t), mid - 16, 184 + hu_bob(t + 20, 64, 2), 2, 0);
            }
        } else {
            for (int i = 0; i < 2; i++) {
                if (!side) spr(hero_frame(w, i, t), i ? mid + 2 : mid - 34, 120 + hu_bob(t + i * 20, 64, 2), 2, player_pal(i));
                for (int k = 0; k < WINS_NEEDED; k++)                /* the rounds won */
                    spr(w->f[i].wins > k ? SPR_STAR_ON : SPR_STAR_OFF, (i ? mid + 10 : mid - 26) + k * 9, 172, 2, -1);
            }
        }
    } else {
        for (int i = 0; i < w->nf; i++) {
            const field *f = &w->f[i];
            spr(SPR_HERO_ICON, L[i].x, 30, 2, player_pal(i));
            if (f->alive && w->phase == PH_PLAY) next_pair(w, f, L[i].x + L[i].w - 16, 28, 12);
            for (int k = 0; k < WINS_NEEDED; k++)
                spr(f->wins > k ? SPR_STAR_ON : SPR_STAR_OFF, L[i].x + 18 + k * 9, 34, 2, -1);
        }
    }
    for (int i = 0; i < w->nf; i++)                            /* the rain coming, in the band above each field */
        if (w->f[i].alive) rain_icons(&w->f[i], L[i].x, R[i].y0 + 1, L[i].w);
    drops_left = DROPS_MAX;
    for (int i = 0; i < w->nf; i++) droplets(&w->f[i], &R[i]);
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, const touch_view *tv)
{
    static const int medals[4] = MEDAL_SCORES;
    (void)tv;
    if (laid_width != rs_screen_width() || laid_height != rs_screen_height()) maps_setup();
    int title = state == ST_TITLE || !w->started;
    layout(w, title);
    int pl = portrait(), playing = !title && (state == ST_PLAY || state == ST_DEAD);
    pl_on = pl;
    pl_pin = pl && playing && w->nf == 2;
    if (tall()) {                           /* a phone held upright: zoomed, but 3-4 players' play (every column) */
        rs_screen_fill(!(pl && playing && w->nf > 2));
        if (pl) rs_screen_origin_v(PL_ABOVE);
        rs_bg_pin(RS_BG1, 0, pl_pin ? PL_PIN_N : 0, PL_PIN_SY);
        ui_pin = !ht_narrow() ? HT_PIN_OFF : state == ST_TITLE ? HT_PIN_TITLE : state == ST_OVER ? HT_PIN_PANEL : HT_PIN_OFF;
        if (ui_pin) ht_pin(ui_pin);             /* the title and the panels at the same place on every game's screen */
    }
    hu_standing rs;
    standing(w, &rs);
    int t = (int)rs_frame_count();
    int sx = hu_shake_x(), sy = hu_shake_y();
    rs_bg_scroll(RS_BG2, -sx, -sy);
    rs_bg_scroll(RS_BG3, -sx, -sy);
    flayout L[MAX_FIELDS];
    region R[MAX_FIELDS];
    int n = title ? 0 : screen_layout(w->nf, L);
    for (int i = 0; i < n; i++) {
        region_of(&L[i], &R[i]);
        paint_field(w, i, &R[i]);
        upload_field(i, &R[i]);
    }
    int side = rs_screen_margin() >= 44;
    if (!title && (state == ST_PLAY || state == ST_DEAD)) {
        if (shown_key != ST_PLAY) {
            shown_key = ST_PLAY;
            shown_text = 0;
        }
        if (paused != text_paused) {        /* PAUSED takes rows of the UI layer: the texts are drawn again after */
            text_paused = paused;
            if (!paused) shown_text = 0;
        }
        if (!paused) play_texts(w, L, side);
    } else {
        screen_text(w, state, st_t, best, new_best, &rs);
    }
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, 0, -slide);
    /* PAUSED: under the previews, above the chef and the heroes (in play); below a banner (the intro, a round won) */
    if (ht_narrow()) ht_pause(paused, 13);               /* (house_tall.h: the house row) */
    else hu_pause(paused, !title && (w->phase == PH_INTRO || w->phase == PH_END) ? 13 : 11);

    rs_oam_clear();
    if (state == ST_TITLE) {
        if (ht_narrow()) {                         /* (house_tall.h: the slots closer, the glyph by the wrapped prompt) */
            ht_title cfg = {TR(VERB), MAX_PLAYERS, HT_ROW_SLOTS, 0};
            ht_title_sprites(&cfg, &title_out, st_t, slot_icon, NULL);
        } else {
            hu_title_sprites(st_t, slot_icon, NULL);   /* the prompt's glyph, the joined players popping in */
        }
        spr(hero_frame(w, 0, st_t), RS_SCREEN_W / 2 - 16, (pl ? 102 : 90) + hu_bob(st_t, 64, 4), 2, 0);
        for (int k = 0; k < 4; k++) {              /* jellies bouncing round the hero (portrait: closer, lower) */
            static const int jx[2][4] = {{92, 212, 64, 240}, {104, 200, 76, 228}}, jy[2][4] = {{104, 104, 120, 120}, {112, 112, 124, 124}};
            int b = hu_bob(st_t * 2 + k * 23, 48, 6);
            spr(SPR_JELLY16_0 + k, jx[pl][k], jy[pl][k] - (b < 0 ? -b : b), 2, -1);
        }
        return;
    }
    if (state == ST_OVER) {
        int narrow = ht_narrow();
        if (w->players == 1 && narrow) {
            ht_gameover_sprites(w->f[0].score, best, hu_medal_of(w->f[0].score, medals), st_t, slide);
        } else if (w->players == 1) {
            hu_gameover_sprites(1, w->f[0].score, 0, best, hu_medal_of(w->f[0].score, medals), st_t, slide);
        } else {
            if (narrow) ht_results_sprites(&rs, st_t, slide);
            else hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                if (narrow) ht_results_icon_pos(&rs, i, &cx, &cy);
                else hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_HERO_ICON, cx - 8, cy - 8 + slide, 3, player_pal(rs.order[i]));
            }
        }
        if (side && w->nf == 2) side_characters(w, L, t);   /* (the shelves beside are bare without them) */
        if (pl && w->nf == 2) hud_board(w, L, t, 0);        /* (portrait: the HUD board above the panel) */
    }
    if (!title && state != ST_OVER) play_sprites(w, L, R, side, t);   /* the panels: only the fields behind */
}

/* ---- save states (main.c) ---- */
/* after a load: the VRAM tiles are the state's, the cache of what they hold is not; another width: the maps again */
void draw_state_loaded(void)
{
    memset(cache_ok, 0, sizeof cache_ok);       /* (the registered objects stay as loaded: a save gives the same bytes) */
    if (rs_bg_map_w(RS_BG1) != map_w() || rs_bg_map_h(RS_BG3) != bg3_rows()) {   /* (a state of another height too) */
        maps_setup();
        shown_key = -1;
        chef_loaded = -1;
    }
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(fslot); S(slot_used); S(laid_key); S(chef_loaded); S(shown_key); S(shown_best); S(shown_text); S(text_paused);
    RS_STATE_RASTER(raster);
}
#undef S
