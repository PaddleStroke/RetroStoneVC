/*
 * Caterpillar Crawl: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1); BG2 the caterpillars' bodies, one 16x16 metatile per cell, redrawn from the
 *   world every frame (its tiles are the sprites': the segment, the spotted one, the fat one with a leaf going down, each
 *   joined to its two neighbours, and the tail; on the BG palette of the player, 1-4); BG3 the garden's obstacles (stones, bushes, flower pots, mushrooms);
 *   BG4 the garden: the raised bed of soil in its wooden frame, on the lawn.
 *   Sprites: the heads (OBJ 0; players 2-4 OBJ 1, 4, 5: the greens turned pink, blue, orange), sliding from cell to cell
 *   between the steps; the leaf, the flower, the puffs (OBJ 2); the kit's digits, glyphs and medals (OBJ 3); the berry,
 *   the HUD's pips and the small digits (OBJ 6).
 * Widescreen (docs/spec.md "Widescreen"): the 320-px stage is centred (rs_screen_origin) and a wider screen shows more
 * of the lawn on both sides; the plot is the same at every width.
 * Portrait (a zoomed tall screen, DESIGN.md "Tall screen"): the upright bed. The world is the same (18 x 12 cells, the
 * rules untouched); the view turns it a quarter turn clockwise (the plot's x runs down the screen, its y from right
 * to left), so the bed stands 12 cells across and 18 down, on its own upright garden art (garden_tall), the HUD at
 * the screen's top, the panels and the title in the same zoomed view. main.c turns the pads and swipes back
 * (draw_dir_in). Its maps are 64 rows tall; a state saved in the other view sets the maps up again on loading.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

/* the title logo: one 3-shade ramp per word (light, mid, dark): CATERPILLAR in leaf green, CRAWL in petal pink */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(222, 248, 150), RS_RGB8(150, 214, 84), RS_RGB8(84, 160, 56)},
    {RS_RGB8(255, 208, 226), RS_RGB8(246, 150, 190), RS_RGB8(214, 92, 146)}};
#define TITLE_CAT_Y 100         /* the title caterpillar's line (its segments' top) */

static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};
static int shown_key = -1, shown_best = -1;
/* a tall screen (rs_screen_tall: a phone held upright): the garden's scroll this frame, for the lawn the raster draws
 * past the stage's 240 lines (not saved: set by draw_frame before every picture) */
static int lawn_sx, lawn_sy;
/* a tall screen: during a run the HUD (the score, the leaves, the berry; the arena's heads and stars) is drawn in the
 * top HUD_LINES lines of the screen, not of the stage (1: set by draw_frame before every picture, not saved) */
#define HUD_LINES 32
static int hud_top;

/* Portrait: the upright bed (uv), on a zoomed screen of ZOOM_H lines (rs_screen_zoom) at least UV_MIN_H tall (a
 * shorter tall screen keeps the bed as it is, the lawn drawn past it by the raster). Set by draw_init from the screen,
 * fixed for a run (not saved: a state loads into either view). plot_x, plot_y: the plot's top-left on the stage (the
 * screen's left and the stage's line 0); pano_off: the garden_tall line at the screen's top; ui_y: the stage's origin
 * (the UI's 30 rows centred on the bed). */
#define ZOOM_H 512
#define UV_MIN_H 352
static int uv, plot_x = GRID_X, plot_y = GRID_Y, pano_off, ui_y;
#define PLOT_W (uv ? GRID_H : GRID_W)       /* the plot on the screen, in cells */
#define PLOT_H (uv ? GRID_W : GRID_H)
#define LOGO_ROW (-14)                      /* portrait title: the logo's top row on the stage (row 50 of BG1's 64) */
#define UV_CAT_Y 22                         /* the title caterpillar's line on the stage */
#define UV_SLOTS 8                          /* rows: the players' slots, PRESS A, BEST, the join line, the credits */
#define UV_PROMPT 12
#define UV_BEST 15
#define UV_JOIN 18
#define UV_COPY 25
#define UV_COLS 25                          /* the widest line of text, in columns (the columns shown, less a margin) */

int draw_zoom(void) { return rs_screen_zoom(ZOOM_H); }

/* a heading of the plot as the screen shows it, and back (main.c: the pads and swipes are screen directions) */
static int dir_out(int d) { return uv ? (d + 1) & 3 : d; }
int draw_dir_in(int d) { return d < 0 || !uv ? d : (d + 3) & 3; }

/* a cell's column and row on the screen's plot */
static void cell_grid(int cell, int *gx, int *gy)
{
    int x = cell % GRID_W, y = cell / GRID_W;
    *gx = uv ? GRID_H - 1 - y : x;
    *gy = uv ? x : y;
}

/* A tall screen's lines past the stage (DESIGN.md "Tall screen"): more lawn above and below the bed. BG4 shows the
 * lawn of the stage's top 32 lines (above the bed's frame) again, every 32 lines, shifted sideways band by band so
 * its flowers and stones never line up; the other layers and the sprites are hidden there (window 1 over the whole
 * width). Line 0 puts the stage's settings back, so a 240-line screen is unchanged. */
static void raster(int line, void *user)
{
    (void)user;
    if (uv) {                                   /* the upright bed: its maps cover the screen, nothing to hide */
        if (line == rs_screen_top()) {
            for (int l = RS_BG1; l <= RS_BG3; l++) rs_bg_window(l, 0);
            rs_obj_window(0);
        }
        return;
    }
    if (line == 0) {
        for (int l = RS_BG1; l <= RS_BG3; l++) rs_bg_window(l, 0);
        rs_obj_window(0);
        rs_bg_scroll(RS_BG4, lawn_sx, lawn_sy);
        return;
    }
    if (line > 0 && line < RS_SCREEN_H) return;
    if (line == rs_screen_top() || line == RS_SCREEN_H) {
        rs_window(0, rs_screen_left(), rs_screen_right());
        for (int l = RS_BG1; l <= RS_BG3; l++) rs_bg_window(l, RS_WIN1);
        rs_obj_window(line < 0 && hud_top ? 0 : RS_WIN1);   /* the HUD's sprites at the screen's top (hud) */
    }
    if (hud_top && line == rs_screen_top() + HUD_LINES) rs_obj_window(RS_WIN1);
    int band = line >= 0 ? line / 32 : -((31 - line) / 32);     /* floor(line / 32) */
    rs_bg_scroll(RS_BG4, (lawn_sx + band * 168) & (rs_bg_map_w(RS_BG4) * 8 - 1), (lawn_sy - band * 32) & 255);
}

/* ---- the maps -------------------------------------------------------------------------------------------------------
 * The garden is a "stage panorama": PANO_W tiles from stage x PANO_X0 (-128) to 447, so every width up to 560 shows it
 * once (no repeat): map column c holds stage tile c (0..), the last columns the stage's left margin (negative x), as
 * the BG wraps. Maps of 64 tiles up to 512 columns, 128 beyond (the map size is saved in states: draw_state_loaded
 * sets them up again). BG2 and BG3 hold the plot's cells at their stage tiles. */
static int map_w(void) { return rs_screen_width() > 512 ? 128 : 64; }

static void put_pano(int layer, const uint16_t *pano, int rows, int mw)
{
    int lim = mw == 64 ? 52 : 56;           /* stage tiles 0..51 (55) on the right, the rest is the left margin */
    for (int c = 0; c < mw; c++) {
        int st = c < lim ? c : c - mw, i = st - PANO_X0 / 8;
        for (int y = 0; y < rows; y++)
            rs_bg_put(layer, c, y, i >= 0 && i < PANO_W ? pano[y * PANO_W + i] : pano[y * PANO_W]);
    }
}


/* the maps (64 rows in portrait: the screen's 512 lines, never wrapped) and the garden's tiles of this view */
static int map_h(void) { return uv ? 64 : 32; }

static void maps_setup(void)
{
    int mw = map_w(), mh = map_h();
    rs_bg_setup(RS_BG1, mw, mh, VR_BG1);
    rs_bg_setup(RS_BG2, mw, mh, VR_BG2);
    rs_bg_setup(RS_BG3, mw, mh, VR_BG3);
    rs_bg_setup(RS_BG4, mw, mh, VR_BG4);
    if (uv) {
        rs_tiles_load(VR_BG4, gm_tall_tiles, gm_tall_tile_count);
        rs_pal_load(RS_PAL_BG(PAL_GARDEN), gm_tall_pal, 16);
        for (int y = 0; y < TALL_PANO_H; y++)
            for (int x = 0; x < TALL_PANO_W; x++) rs_bg_put(RS_BG4, x, y, gm_tall_map[y * TALL_PANO_W + x]);
    } else {
        rs_tiles_load(VR_BG4, gm_garden_tiles, gm_garden_tile_count);
        rs_pal_load(RS_PAL_BG(PAL_GARDEN), gm_garden_pal, 16);
        put_pano(RS_BG4, gm_garden_map, PANO_H, mw);
    }
}

void draw_init(void)
{
    int h = rs_screen_height();
    uv = h >= UV_MIN_H;
    if (uv) {                               /* portrait: the upright bed, its middle under the HUD's band */
        pano_off = (TALL_PANO_H * 8 - h) / 2;
        plot_x = TALL_X;
        ui_y = TALL_Y - pano_off + GRID_W * CELL / 2 - RS_SCREEN_H / 2;   /* the UI's rows centred on the bed */
        plot_y = TALL_Y - pano_off - ui_y;
        rs_screen_origin_v(ui_y);
    } else {                                /* (the game restarts in this process when the phone turns) */
        pano_off = ui_y = 0;
        plot_x = GRID_X;
        plot_y = GRID_Y;
        rs_screen_origin_v(rs_screen_vmargin());   /* the stage centred (the default) */
    }
    rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more lawn on both sides */
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;         /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    rs_tiles_load(VR_BG3, gm_obst_tiles, gm_obst_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_OBST), gm_obst_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_BODY), gm_body_pals, 64);    /* the four players' bodies: BG 1-4 */
    maps_setup();                                           /* (and the garden's tiles and colours) */
    rs_pal_set(0, gm_garden_pal[1]);                        /* the backdrop (never seen: the lawn covers it) */
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* the caterpillar, P2, the props (the kit loaded OBJ 3) */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 48);     /* P3, P4, the small things */
    rs_raster(raster, NULL);                                /* a tall screen: the lawn past the stage */
}

/* ---- sprites -------------------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal, int flags)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= rs_screen_top() - (int)d->h ||
        y >= rs_screen_bottom())
        return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

/* player p's sprite palette: the house rule (P1 OBJ 0, P2 OBJ 1, P3 OBJ 4, P4 OBJ 5: house_ui.h hu_player_pal) */
static int player_pal(int p) { return p ? hu_player_pal(p) : 0; }

static int cell_x(int cell) { int gx, gy; cell_grid(cell, &gx, &gy); return plot_x + gx * CELL; }
static int cell_y(int cell) { int gx, gy; cell_grid(cell, &gx, &gy); return plot_y + gy * CELL; }

/* an effect's point (play.c: stage pixels of the 18 x 12 plot) on the screen's plot */
static void fx_point(const effect *e, int *x, int *y)
{
    if (!uv) { *x = e->x; *y = e->y; return; }
    *x = plot_x + GRID_H * CELL - (e->y - GRID_Y);
    *y = plot_y + (e->x - GRID_X);
}

/* small digits (the points of a flower, a butterfly), centred on cx: "+5" */
static void small_number(int n, int cx, int y)
{
    char s[8];
    int len = snprintf(s, sizeof s, "+%d", n);
    for (int i = 0; i < len; i++) {
        int g = s[i] == '+' ? 10 : s[i] - '0';
        spr(SPR_DIGIT + g, cx - len * 3 + i * 6 - 1, y, 2, -1, 0);
    }
}

/* ---- BG2: the bodies, one metatile per cell (16x16 sprite tiles: TL, TR, BL, BR) -------------------------------- */
/* (gx, gy: a cell of the screen's plot; the maps' rows wrap: in portrait the plot starts above the stage) */
static void put16(int gx, int gy, int spr_id, int pal, int hf, int vf)
{
    int base = gm_spr[spr_id].tile, col = plot_x / 8 + gx * 2, row = plot_y / 8 + gy * 2, mh = map_h();
    for (int qy = 0; qy < 2; qy++)
        for (int qx = 0; qx < 2; qx++) {
            int src = base + (vf ? 1 - qy : qy) * 2 + (hf ? 1 - qx : qx);
            uint16_t e = (uint16_t)(src | (pal << 10) | (hf ? RS_MAP_HFLIP : 0) | (vf ? RS_MAP_VFLIP : 0));
            rs_bg_put(RS_BG2, col + qx, (row + qy) & (mh - 1), e);
        }
}

/* the side (R 1, D 2, L 4, U 8) of cell a where cell b is, on the screen (0: not a neighbour) */
static int side_of(int a, int b)
{
    int ax, ay, bx, by;
    cell_grid(a, &ax, &ay);
    cell_grid(b, &bx, &by);
    int dx = bx - ax, dy = by - ay;
    return dy == 0 && dx == 1 ? 1 : dx == 0 && dy == 1 ? 2 : dy == 0 && dx == -1 ? 4 : dx == 0 && dy == -1 ? 8 : 0;
}

static void bodies(const world *w, int show)
{
    /* the joined pieces' frames by the sides they join (make_art.py JOINS: L-R, U-D, R-D, D-L, L-U, U-R); one side
     * only (never in play): the straight piece through it */
    static const int8_t join_frame[16] = {0, 0, 1, 2, 0, 0, 3, 0, 1, 5, 1, 0, 4, 0, 0, 0};
    static const int body_spr[3] = {SPR_BODY0, SPR_BODY1, SPR_BODY2};
    uint16_t blank = gm_spr[SPR_BLANK].tile;               /* the blank sprite tile: an empty cell */
    int mh = map_h();
    for (int y = plot_y / 8; y < plot_y / 8 + PLOT_H * 2; y++)
        for (int x = plot_x / 8; x < plot_x / 8 + PLOT_W * 2; x++) rs_bg_put(RS_BG2, x, y & (mh - 1), blank);
    if (!show) return;
    for (int p = 0; p < w->players; p++) {
        const cat *c = &w->c[p];
        if (c->state == CS_OFF || c->state == CS_DOWN) continue;
        int pal = PAL_BODY + p;
        for (int i = 1; i < c->len; i++) {
            int cell = cat_cell(c, i), gx, gy, prev = cat_cell(c, i - 1), px, py;
            cell_grid(cell, &gx, &gy);
            if (i == c->len - 1) {                  /* the tail points away from the segment before it */
                cell_grid(prev, &px, &py);
                int dx = gx - px, dy = gy - py;
                if (dx) put16(gx, gy, SPR_BODYTAIL, pal, dx < 0, 0);
                else put16(gx, gy, SPR_BODYTAIL + 1, pal, 0, dy < 0);
            } else {
                int kind = w->fat[cell] ? 2 : (cell % GRID_W + cell / GRID_W) & 1;
                put16(gx, gy, body_spr[kind] + join_frame[side_of(cell, prev) | side_of(cell, cat_cell(c, i + 1))],
                      pal, 0, 0);
            }
        }
    }
}

/* BG3: the obstacles of the layout (redrawn every frame: 864 map entries, nothing to keep in a state) */
static void obstacles(const world *w, int show)
{
    int mh = map_h();
    for (int cell = 0; cell < CELLS; cell++) {
        int ob = show ? world_obstacle(w, cell) : OB_NONE, gx, gy;
        cell_grid(cell, &gx, &gy);
        int col = plot_x / 8 + gx * 2, row = plot_y / 8 + gy * 2;
        for (int q = 0; q < 4; q++)
            rs_bg_put(RS_BG3, col + (q & 1), (row + q / 2) & (mh - 1), ob ? gm_obst_map[(ob - 1) * 4 + q] : 0);
    }
}

/* ---- the heads ---------------------------------------------------------------------------------------------------- */
static void head_frame(const cat *c, int look, int *id, int *fl)
{
    int f = look == DIR_DOWN ? 1 : look == DIR_UP ? 2 : 0;
    *fl = look == DIR_LEFT ? RS_SPR_HFLIP : 0;
    if (c->eat_t < 12) f += 3;                      /* chomp */
    *id = SPR_HEAD + f;
}

static void draw_head(const world *w, int p, int sx, int sy)
{
    const cat *c = &w->c[p];
    if (c->state == CS_OFF || c->state == CS_DOWN || c->len <= 0) return;
    int cell = cat_cell(c, 0), x = cell_x(cell), y = cell_y(cell), id, fl;
    int look = dir_out(c->nq ? c->queue[0] : c->dir), md = dir_out(c->mdir);     /* as the screen shows them */
    head_frame(c, look, &id, &fl);
    if (c->state == CS_CRAWL && w->phase == PH_PLAY) {   /* sliding in from the cell before */
        int k = (int)((int64_t)w->acc * CELL / Q16_ONE);
        x -= DX[md] * (CELL - k);
        y -= DY[md] * (CELL - k);
        if (md != look && k < CELL / 2) head_frame(c, md, &id, &fl);
    } else if (c->state == CS_HIT || c->state == CS_MELT) {
        id = SPR_HEAD + 6;
        fl = 0;
        if (c->state == CS_HIT) {
            int b = c->t < HIT_FREEZE ? 3 : 3 - (c->t - HIT_FREEZE) / 4;   /* a bump into what it hit, then back */
            if (b < 0) b = 0;
            x += DX[dir_out(c->dir)] * b;
            y += DY[dir_out(c->dir)] * b;
        }
    } else if (c->state == CS_COCOON) {
        if (w->phase_t >= META_SHRINK) {
            if (w->phase_t < META_COCOON) {
                int wig = (w->phase_t / 6) % 2;
                spr(SPR_COCOON + wig, x + (wig ? 1 : 0) + sx, y + sy, 2, player_pal(p), 0);
            }
            return;
        }
        id = SPR_HEAD + 7;
        fl = 0;
    } else if ((c->state == CS_WAIT && w->mode == MODE_ARENA && w->round_winner == p &&
                (w->phase == PH_ROUND_END || w->phase == PH_OVER))) {
        id = SPR_HEAD + 7;                          /* the round's winner beams */
        fl = 0;
        y -= hu_bob(w->phase_t, 24, 2) + 2;
    }
    spr(id, x - 4 + sx, y - 4 + sy, 2, player_pal(p), fl);
}

/* the butterfly flutters up from the cocoon and off the top of the screen, waving left and right */
static void draw_butterfly(const world *w, int sx, int sy)
{
    if (w->phase != PH_META || w->phase_t < META_COCOON) return;
    int t = w->phase_t - META_COCOON, cell = w->meta_cell;
    int x = cell_x(cell) + 8 + hu_bob(t, 48, 18) - 12, y = cell_y(cell) + 8 - t * 3 - 12;
    if (y < rs_screen_top() - 24) return;           /* (up to the screen's top: a tall screen's too) */
    spr(SPR_BUTTERFLY + (t / 5) % 2, x + sx, y + sy, 2, 0, 0);
    if (t % 8 < 4) hu_sparkle(x + 8 - (t % 16), y + 20, (t / 4) % 2, 2);
}

static void draw_world(const world *w, int sx, int sy)
{
    /* the effects (front) */
    for (int i = 0; i < FX_MAX; i++) {
        const effect *e = &w->fx[i];
        int ex, ey;
        fx_point(e, &ex, &ey);
        switch (e->kind) {
        case FX_PUFF:
            spr(SPR_PUFF + e->t / 6, ex - 8 + sx, ey - 8 + sy, 2, -1, 0);
            break;
        case FX_SPARK:
            for (int k = 0; k < 3; k++) {
                static const int ox[3] = {-9, 7, -1}, oy[3] = {-6, -3, 6};
                hu_sparkle(ex - 4 + ox[k] + (k - 1) * e->t / 4 + sx, ey - 4 + oy[k] - e->t / 3 + sy,
                           (e->t / 4 + k) % 2, 2);
            }
            break;
        case FX_POINTS:
            small_number(e->value, ex + sx, ey - 14 - e->t / 3 + sy);
            break;
        case FX_LEAF:
            if (e->t < 6) spr(SPR_LEAF + (e->t / 3) % 2, ex - 8 + sx, ey - 8 + sy, 2, -1, RS_SPR_VFLIP);
            break;
        }
    }
    draw_butterfly(w, sx, sy);
    for (int p = 0; p < w->players; p++) draw_head(w, p, sx, sy);
    /* the leaf and the treat */
    if (w->leaf >= 0)
        spr(SPR_LEAF + (w->t / 30) % 2, cell_x(w->leaf) + sx, cell_y(w->leaf) + sy, 2, -1, 0);
    if (w->treat >= 0 && (TREAT_LIFE - w->treat_t > TREAT_BLINK || (w->treat_t / 6) % 2)) {
        int b = hu_bob(w->t, 32, 1);
        if (w->treat_kind == TREAT_FLOWER)
            spr(SPR_FLOWER + (w->t / 20) % 2, cell_x(w->treat) + sx, cell_y(w->treat) + b + sy, 2, -1, 0);
        else
            spr(SPR_BERRY + ((w->t / 10) % 9 == 0), cell_x(w->treat) + sx, cell_y(w->treat) + b + sy, 2, -1, 0);
    }
}

/* ---- the HUD ---------------------------------------------------------------------------------------------------- */
static void hud(const world *w)
{
    int hy = hud_top ? rs_screen_top() : 0;    /* a tall screen: the screen's top lines, not the stage's */
    if (uv) {
        /* portrait: centred in the columns shown, clear of the app's buttons in the top corners: the score, the
         * leaves under it, the berry on its left; the arena's players two by two */
        if (w->mode == MODE_SOLO) {
            hu_number_at(w->c[0].score, 160, hy + 5, 0, 3);
            for (int i = 0; i < LEAVES_PER_GARDEN; i++)
                spr(SPR_LEAFPIP + (i >= w->garden_leaves), 112 + i * 8, hy + 25, 3, -1, 0);
            if (w->slow_t > 0 && (w->slow_t > 60 || (w->slow_t / 5) % 2)) spr(SPR_BERRY, 96, hy + 5, 3, -1, 0);
            return;
        }
        for (int p = 0; p < w->players; p++) {
            int row = p / 2, n = w->players - row * 2 > 1 ? 2 : 1, k = p % 2;
            int x = 160 + (n == 2 ? (k ? 8 : -51) : -22), y = hy + 4 + row * 18;
            const cat *c = &w->c[p];
            int out = c->state == CS_HIT || c->state == CS_MELT || c->state == CS_DOWN;
            if (!out || (w->t / 8) % 2 || c->state == CS_DOWN) spr(SPR_ICON, x, y, 3, player_pal(p), 0);
            for (int s = 0; s < ARENA_WINS; s++) {
                int won = s < c->wins, pop = won && s == c->wins - 1 && w->round_winner == p &&
                          (w->phase == PH_ROUND_END || w->phase == PH_OVER) && w->phase_t < 16;
                spr(SPR_STARPIP + !won, x + 18 + s * 9, y + 4 - (pop ? hu_ease_back(16 - w->phase_t, 16, 4) : 0), 3,
                    -1, 0);
            }
        }
        return;
    }
    if (w->mode == MODE_SOLO) {
        hu_chip c = hu_score_chip(1, 0, 0);     /* (hu_score_chips' place) */
        hu_number_at(w->c[0].score, c.x, c.y + hy, c.align, 3);
        /* the garden's leaves: eaten, to go (top right) */
        for (int i = 0; i < LEAVES_PER_GARDEN; i++)
            spr(SPR_LEAFPIP + (i >= w->garden_leaves), 212 + i * 8, 14 + hy, 3, -1, 0);
        if (w->slow_t > 0 && (w->slow_t > 60 || (w->slow_t / 5) % 2))   /* the berry's slow time */
            spr(SPR_BERRY, 18, 8 + hy, 3, -1, 0);
        return;
    }
    /* the arena: each player's head and its round wins (stars) */
    for (int p = 0; p < w->players; p++) {
        int cx = 320 * (2 * p + 1) / (2 * w->players), x = cx - (16 + ARENA_WINS * 9) / 2;
        const cat *c = &w->c[p];
        int out = c->state == CS_HIT || c->state == CS_MELT || c->state == CS_DOWN;
        if (!out || (w->t / 8) % 2 || c->state == CS_DOWN)
            spr(SPR_ICON, x, 10 + hy, 3, player_pal(p), 0);
        for (int k = 0; k < ARENA_WINS; k++) {
            int won = k < c->wins, pop = won && k == c->wins - 1 && w->round_winner == p &&
                      (w->phase == PH_ROUND_END || w->phase == PH_OVER) && w->phase_t < 16;
            spr(SPR_STARPIP + !won, x + 18 + k * 9, 14 + hy - (pop ? hu_ease_back(16 - w->phase_t, 16, 4) : 0), 3, -1, 0);
        }
    }
}

/* the title's player slots: each joined player's icon, in its colours (hu_title_sprites calls it) */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr(SPR_ICON, cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), 2, player_pal(p), 0);
}

/* the title: a caterpillar inching along under the logo (its segments ripple) */
static void title_caterpillar(int t)
{
    int x0 = 160 - 40, cy = uv ? UV_CAT_Y : TITLE_CAT_Y;
    for (int i = 0; i < 5; i++) {
        int bob = hu_bob(t * 2 + i * 14, 64, 2);
        if (i == 4) spr(SPR_HEAD + ((t % 180) < 40 ? 7 : 0), x0 + i * 14 - 4, cy - 6 + bob, 2, 0, 0);
        else if (i == 0) spr(SPR_SEG + 3, x0 + i * 14 + 2, cy + bob, 2, 0, RS_SPR_HFLIP);
        else spr(SPR_SEG + (i & 1), x0 + i * 14, cy + bob, 2, 0, 0);
    }
    spr(SPR_LEAF + (t / 30) % 2, x0 + 5 * 14 + 14, cy, 2, -1, 0);
}

/* ---- portrait: the title and the long lines in the columns shown ------------------------------------------------ */
/* s centred on row y, cut at its spaces into lines of at most UV_COLS columns, 2 rows apart (accents go in the row
 * above): after a colon if it can ("P2 / P3 / P4:" over "PRESS A TO JOIN"), else the last space that fits; returns
 * the rows used */
static int text_wrap(int y, const char *s, int box)
{
    char line[160];
    int rows = 0;
    while (*s) {
        int n = (int)strlen(s), cut = n, colon = 0;
        if (hu_width(s, 0) > UV_COLS) {
            cut = 0;
            for (int i = 1; i < n && i < (int)sizeof line; i++)
                if (s[i] == ' ') {
                    memcpy(line, s, (size_t)i);
                    line[i] = 0;
                    if (hu_width(line, 0) > UV_COLS) break;
                    if (!colon || s[i - 1] == ':') cut = i;
                    if (s[i - 1] == ':' && hu_width(s + i + 1, 0) <= UV_COLS) colon = 1;
                }
            if (!cut) cut = n;                      /* one long word: as it is */
        }
        if (cut >= (int)sizeof line) cut = (int)sizeof line - 1;
        memcpy(line, s, (size_t)cut);
        line[cut] = 0;
        if (box) hu_box_text(hu_center(line, 0), y + rows, line);
        else hu_text(hu_center(line, 0), y + rows, line);
        rows += 2;
        s += cut;
        while (*s == ' ') s++;
    }
    return rows;
}

/* the logo on two lines (make_art.py logo_tall) in the kit's logo tiles, above the stage */
static void uv_logo(void)
{
    rs_tiles_load(VR_BG1 + LOGO_TILE, gm_logo_tiles, gm_logo_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_LOGO), gm_logo_pal, 16);
    for (int y = 0; y < LOGO_TH; y++)
        for (int x = 0; x < LOGO_TW; x++)
            rs_bg_put(RS_BG1, (40 - LOGO_TW) / 2 + x, (LOGO_ROW + y) & 63, gm_logo_map[y * LOGO_TW + x] | 0x2000);
}

/* the players' slots, 5 columns apart (the kit's are 9: wider than the columns shown) */
static void uv_slot_pos(int p, int *cx, int *cy)
{
    *cx = (10 + 5 * p) * 8 + 28;
    *cy = UV_SLOTS * 8 + 4;
}

/* hu_title_draw in the columns shown: PRESS A TO CRAWL, BEST, the slots' tags, the join line, the credits */
static int uv_glyph_x;
static void uv_title_draw(int t, int best)
{
    char s[160], who[64] = "";
    int n = hu_players();
    rs_strf(s, sizeof s, HTR(PRESS_TO), TR(VERB));
    uv_glyph_x = hu_prompt(UV_PROMPT, s, t);
    hu_clear_rows(UV_BEST, 1);
    if (best > 0) {
        rs_strf(s, sizeof s, HTR(BEST_N), best);
        hu_text(hu_center(s, 0), UV_BEST, s);
    }
    hu_clear_rows(UV_SLOTS, 1);
    for (int p = 0; p < n; p++) {
        rs_strf(s, sizeof s, HTR(PLAYER_TAG), p + 1);
        hu_text(10 + 5 * p, UV_SLOTS, s);
    }
    hu_clear_rows(UV_JOIN - 1, 6);
    if (n < MAX_PLAYERS) {
        for (int p = n; p < MAX_PLAYERS; p++) {
            size_t k = strlen(who);
            if (p > n) k += (size_t)snprintf(who + k, sizeof who - k, " / ");
            if (k < sizeof who) rs_strf(who + k, sizeof who - k, HTR(PLAYER_TAG), p + 1);
        }
        rs_strf(s, sizeof s, n > 1 ? HTR(JOIN_LEAVE) : HTR(JOIN), who);
    } else {
        rs_strf(s, sizeof s, HTRN(FULL, n), n);
    }
    text_wrap(UV_JOIN, s, 0);
    hu_text(hu_center("(C) 2026 8BCRAFT", 0), UV_COPY, "(C) 2026 8BCRAFT");
    hu_text(hu_center("RETROSTONE VC", 0), UV_COPY + 1, "RETROSTONE VC");
}

/* hu_title_sprites in the columns shown: the prompt's glyph, the joined players popping in */
static void uv_title_sprites(int t)
{
    int gx, gy, g = hu_title_glyph(&gx, &gy);
    if (g >= 0) hu_glyph(g, uv_glyph_x, UV_PROMPT * 8 - 4, (t / 30) % 2, 3);
    for (int p = 0; p < hu_players(); p++) {
        int cx, cy, a = hu_player_age(p);
        uv_slot_pos(p, &cx, &cy);
        if (a < 24) {
            hu_sparkle(cx + 8, cy - 14, (a / 6) & 1, 3);
            hu_sparkle(cx - 14, cy - 2, (a / 6 + 1) & 1, 3);
        }
        slot_icon(p, cx, cy + (a < 16 ? 12 - hu_ease_back(a, 16, 12) : 0), t, NULL);
    }
}

/* a banner (GAME OVER) whose 2x title would be wider than the columns shown: the small letters on a panel */
static void banner(int y, const char *title)
{
    if (!uv || hu_width(title, 1) + 2 <= UV_COLS + 1) {
        hu_banner(y, title);
        return;
    }
    int w = hu_width(title, 0) + 4, tall = hu_tall(title);
    hu_panel((40 - w) / 2, y - tall, w, 3 + tall);
    hu_box_text(hu_center(title, 0), y + 1, title);
}

/* 2-4 players: ranked by rounds won */
static void standing(const world *w, hu_standing *s)
{
    int key[MAX_PLAYERS], v[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) {
        key[p] = world_rank_key(w, p);
        v[p] = w->mode == MODE_ARENA ? w->c[p].wins : w->c[p].score;
    }
    hu_rank(s, w->players, key, v);
}

/* ---- the BG screens -------------------------------------------------------------------------------------------- */
static void text_centered(int row, const char *s, int big)
{
    if (big && hu_width(s, 1) > (uv ? UV_COLS : 38)) big = 0;   /* a long translation: the small letters */
    if (big) hu_big(hu_center(s, 1), row, s, HU_BIG_FREE);
    else if (uv) text_wrap(row, s, 0);                  /* portrait: on two lines if need be */
    else hu_text(hu_center(s, 0), row, s);
}

static int screen_key(const world *w, int state, int st_t)
{
    int screen = state == ST_DEAD ? ST_PLAY : state, key = screen * 100000;
    if (screen == ST_PLAY) {
        if (w->phase == PH_GARDEN) key += 1000 + w->garden;
        if (w->phase == PH_META && w->phase_t >= META_COCOON) key += 2000;
        if (w->phase == PH_ROUND) key += 3000 + w->round;
        if (w->phase == PH_ROUND_END) key += 4000 + w->round * 8 + w->round_winner + 1;
    }
    if (state == ST_OVER && st_t >= RETRY_LOCK) key += 1;
    return key;
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    int key = screen_key(w, state, st_t);
    if (key != shown_key || best != shown_best) {
        char s[96], tag[16];
        hu_clear();
        if (state == ST_TITLE) {
            if (uv) uv_logo();                          /* portrait: the logo on two lines */
            else hu_logo("CATERPILLAR CRAWL", title_ramps, 2, 2, 0);
        }
        if (state == ST_PLAY || state == ST_DEAD) {
            if (w->phase == PH_GARDEN) {
                rs_strf(s, sizeof s, TR(GARDEN_N), w->garden + 1);
                text_centered(13, s, 1);
            } else if (w->phase == PH_META && w->phase_t >= META_COCOON) {
                text_centered(9, TR(BUTTERFLY), 1);
            } else if (w->phase == PH_ROUND) {
                rs_strf(s, sizeof s, TR(ROUND_N), w->round);
                text_centered(12, s, 1);
                if (w->round == 1) {
                    rs_strf(s, sizeof s, TR(FIRST_TO), ARENA_WINS);
                    text_centered(16, s, 0);
                }
            } else if (w->phase == PH_ROUND_END) {
                if (w->round_winner >= 0) {
                    rs_strf(tag, sizeof tag, HTR(PLAYER_TAG), w->round_winner + 1);
                    rs_strf(s, sizeof s, TR(ROUND_WIN), tag);
                    /* on a navy panel: the line stays readable over the bodies that fill the arena by now */
                    int x = hu_center(s, 0), tall = hu_tall(s);
                    if (uv && hu_width(s, 0) > UV_COLS) {   /* portrait: a long translation on two lines */
                        hu_panel(20 - (UV_COLS + 4) / 2, 12 - tall, UV_COLS + 4, 5 + tall);
                        text_wrap(13, s, 1);
                    } else {
                        hu_panel(x - 2, 12 - tall, hu_width(s, 0) + 4, 3 + tall);
                        hu_box_text(x, 13, s);
                    }
                } else {
                    banner(11, HTR(DRAW));
                }
            }
        }
        if (state == ST_OVER) {
            if (w->players == 1) {
                banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->c[0].score, 0);
                rs_strf(s, sizeof s, TRN(BUTTERFLIES_N, w->c[0].butterflies), w->c[0].butterflies);
                hu_gameover_note(s);
            } else {
                hu_results_panel(rs, NULL);            /* P2 WINS! and the ranking by rounds won */
                int win = hu_winner(rs);
                if (win >= 0) rs_strf(s, sizeof s, HTR(WINS), win + 1);
                else rs_strf(s, sizeof s, "%s", HTR(DRAW));
                if (uv && hu_width(s, 1) + 2 > UV_COLS + 1) {   /* portrait: a banner too wide, made narrow */
                    hu_clear_rows(2, 5);
                    banner(3, s);
                }
            }
        }
        shown_key = key;
        shown_best = best;
    }
    if (state == ST_TITLE) {
        if (uv) uv_title_draw(st_t, best);
        else hu_title_draw(st_t, best);
    }
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    static const int medals[4] = MEDAL_SCORES;
    int sx = hu_shake_x(), sy = hu_shake_y(), play = state != ST_TITLE;
    hu_standing rs;
    standing(w, &rs);
    int pm = map_w() * 8 - 1, vm = map_h() * 8 - 1;
    lawn_sx = (-sx) & pm;
    lawn_sy = (-sy) & 255;
    /* portrait: the upright garden's line pano_off at the screen's top (stage y -ui_y) */
    rs_bg_scroll(RS_BG4, lawn_sx, uv ? (pano_off + ui_y - sy) & vm : lawn_sy);
    rs_bg_scroll(RS_BG3, (-sx) & pm, (-sy) & vm);
    rs_bg_scroll(RS_BG2, (-sx) & pm, (-sy) & vm);
    obstacles(w, play);
    bodies(w, play);

    screen_text(w, state, st_t, best, new_best, &rs);
    /* the panel slides up, ease-out (portrait: from the screen's bottom) */
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, uv ? 360 : 200) : 0;
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);

    rs_oam_clear();
    /* a tall screen (DESIGN.md "Tall screen"): during a run the HUD at the screen's top */
    hud_top = rs_screen_height() > RS_SCREEN_H && (state == ST_PLAY || state == ST_DEAD);
    /* front to back: the UI, the play */
    if (state == ST_PLAY || state == ST_DEAD) hud(w);
    if (state == ST_OVER) {
        if (w->players == 1) {
            hu_gameover_sprites(1, w->c[0].score, 0, best, hu_medal_of(w->c[0].score, medals), st_t, slide);
        } else {
            hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_ICON, cx - 8, cy - 8 + slide, 3, player_pal(rs.order[i]), 0);
            }
        }
    }
    if (state == ST_TITLE) {
        if (uv) uv_title_sprites(st_t);             /* portrait: in the columns shown */
        else hu_title_sprites(st_t, slot_icon, NULL);   /* the prompt's glyph, the joined players popping in */
        title_caterpillar(st_t);
    } else if (state != ST_OVER) {
        draw_world(w, sx, sy);
    }
}

/* ---- save states (main.c) ---- */
/* a state saved at another width brings maps of another size, one saved in the other view (portrait or not) maps of
 * another height and the other garden: set them up for this screen, redraw the UI */
void draw_state_loaded(void)
{
    if (rs_bg_map_w(RS_BG2) == map_w() && rs_bg_map_h(RS_BG4) == map_h()) return;
    maps_setup();
    shown_key = -1;
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(shown_key); S(shown_best);
    RS_STATE_RASTER(raster);
}
#undef S
