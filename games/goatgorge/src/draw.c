/*
 * Goat Gorge: video. The house layout (docs/art-direction.md):
 *   backdrop  the chapter's sky, a raster gradient (above a tall screen's stage: deeper; below it: the mountain's dark)
 *   BG4       the far mountains (palette 7), BG3 the middle ground (palette 6): parallax, they lean with the kid
 *   BG2       the room: rock (autotiled quarters, the chapter's cap of grass, moss, crystal, sand or snow), spikes,
 *             ledges, crumbling blocks, decorations (palette 1, the props' palette 2)
 *   BG1       the house kit: the title, the HUD, the hints, the results, PAUSED
 *   sprites   the kid (OBJ 0 with its dash, OBJ 1 without: the scarf red or blue), the props (OBJ 2), the kit (OBJ 3),
 *             the dash's trail (OBJ 4, added half), the effects (OBJ 5), the respawn flash (OBJ 6)
 * Widescreen (320..560): the room's 320 columns centred, the mountain carried on past its sides (the border's rock, the
 * opening's sky). A tall screen (a phone held upright, rs_screen_tall: every column): the room a third of the way down,
 * the sky and the HUD above it, the mountain below it (the touch buttons' band, apps/launcher/games.cfg).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_tall.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

#define HUD_ROWS     3              /* BG1's rows 0..2: the HUD (a tall screen: pinned at the screen's top) */
#define HUD_SHOW     150            /* landscape: the HUD shows this long when a room begins (and while paused) */
#define HINT_SHOW    720            /* a room's tip shows this long */
#define HINT_ROW     27
#define BG2_ABOVE    32             /* a tall screen: the room's top rock carried on this many lines above the stage */
#define BG2_BELOW    32             /* ... and below it (then the backdrop: the mountain darkening) */
#define FAR_ROW      8              /* the far mountains' first map row (stage y 64) */
#define MID_ROW      15             /* the middle ground's (stage y 120) */
#define TRAIL_N      5
#define PART_N       40

static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 250, 238), RS_RGB8(226, 210, 184), RS_RGB8(168, 140, 116)},      /* GOAT: wool */
    {RS_RGB8(255, 132, 112), RS_RGB8(228, 56, 64), RS_RGB8(150, 26, 52)}};          /* GORGE: the scarf */

/* ---- the drawing's state (saved: draw_state) -------------------------------------------------------------------- */
static int cur_ch = -1;             /* the chapter whose tiles and colours are in VRAM */
static int laid_w, laid_h;          /* the screen the maps were laid out for */
static int ui_key = -1;             /* what the UI layer shows (redrawn when it changes) */
static uint8_t crumble_shown[MAX_CRUMBLE];
static int title_room = -1;         /* the title's scene is in BG2 (-1: a room is) */
typedef struct part { int16_t x, y, vx, vy; uint8_t t, life, kind, frame; } part;   /* x, y in 1/16 px */
static part parts[PART_N];
typedef struct ghost { int16_t x, y; uint8_t spr, flip, t; } ghost;
static ghost trail[TRAIL_N];
static int trail_k;
static int tick;
static rs_color line_col[RS_SCREEN_H];
static rs_color sky_top, deep_col, dark_col;
/* per frame (set by draw_frame before the picture, read by the raster: not saved) */
static int r_ui_pin, r_hud, r_bg3x, r_bg3y, r_bg4x, r_bg4y, r_bg2x, r_bg2y;

enum { PK_DUST, PK_BURST, PK_SPARK, PK_SHARD, PK_FLAKE };

static int tall(void) { return rs_screen_height() > RS_SCREEN_H; }
static int map_w(void) { return rs_screen_width() > 512 ? 128 : 64; }

/* ---- the raster: the sky, the layers past a tall screen's stage, the pinned UI ------------------------------------- */
static void raster(int line, void *user)
{
    (void)user;
    int top = rs_screen_top();
    if (line < 0) {
        if (line == top) {
            rs_bg_enable(RS_BG2, 0);
            rs_bg_enable(RS_BG3, 0);
            rs_bg_enable(RS_BG4, 0);
        }
        if (line == -BG2_ABOVE) {
            rs_bg_enable(RS_BG2, 1);
            rs_bg_scroll(RS_BG2, r_bg2x, r_bg2y);
        }
        int k = top < 0 ? (-line * 96) / -top : 0;        /* the sky deeper toward the screen's top */
        rs_pal_set(0, hu_lerp_color(sky_top, RS_RGB8(10, 12, 40), k));
        rs_bg_enable(RS_BG1, r_ui_pin ? ht_pin_line(r_ui_pin, line) : (r_hud && line < top + HUD_ROWS * 8 + 8));
        return;
    }
    if (line < RS_SCREEN_H) {
        if (line == 0) {
            rs_bg_enable(RS_BG2, 1);
            rs_bg_enable(RS_BG3, 1);
            rs_bg_enable(RS_BG4, 1);
            rs_bg_scroll(RS_BG2, r_bg2x, r_bg2y);
            rs_bg_scroll(RS_BG3, r_bg3x, r_bg3y);
            rs_bg_scroll(RS_BG4, r_bg4x, r_bg4y);
        }
        rs_pal_set(0, line_col[line]);
        rs_bg_enable(RS_BG1, r_ui_pin ? ht_pin_line(r_ui_pin, line) : !r_hud || line >= HUD_ROWS * 8);
        return;
    }
    if (line == RS_SCREEN_H) {
        rs_bg_enable(RS_BG3, 0);
        rs_bg_enable(RS_BG4, 0);
    }
    if (line == RS_SCREEN_H + BG2_BELOW) rs_bg_enable(RS_BG2, 0);
    /* the mountain's inside below the room: its dark rock, darker toward the screen's bottom (the pits too) */
    int n = rs_screen_bottom() - (RS_SCREEN_H + BG2_BELOW), k = line - (RS_SCREEN_H + BG2_BELOW);
    rs_pal_set(0, k <= 0 || n <= 0 ? deep_col : hu_lerp_color(deep_col, dark_col, k * 256 / n));
    rs_bg_enable(RS_BG1, r_ui_pin ? ht_pin_line(r_ui_pin, line) : 0);
}

/* ---- the chapter's look: its terrain colours, its far and middle layers, its sky ---------------------------------- */
static void layers_setup(void)
{
    int mw = map_w();
    if (rs_bg_map_w(RS_BG1) != mw) rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG2, mw, 64, VR_BG2);
    rs_bg_setup(RS_BG3, mw, 32, VR_BG3);
    rs_bg_setup(RS_BG4, mw, 32, VR_BG4);
    laid_w = rs_screen_width();
    laid_h = rs_screen_height();
}

static void panoramas(int ch)
{
    int mw = map_w();
    const uint16_t *fm = gg_far_map + ch * FAR_W * FAR_H, *mm = gg_mid_map + ch * MID_W * MID_H;
    int far_row = ch == 2 ? 0 : FAR_ROW;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < mw; x++) {
            int fy = y - far_row, my = y - MID_ROW;
            uint16_t f = fy < 0 ? 0 : fm[(fy < FAR_H ? fy : FAR_H - 1) * FAR_W + x % FAR_W];
            uint16_t m = my < 0 ? 0 : mm[(my < MID_H ? my : MID_H - 1) * MID_W + x % MID_W];
            rs_bg_put(RS_BG4, x, y, f);
            rs_bg_put(RS_BG3, x, y, m);
        }
}

static void load_chapter(int ch)
{
    rs_pal_load(RS_PAL_BG(PAL_TERRAIN), gg_terrain_pal + ch * 16, 16);
    rs_pal_load(RS_PAL_BG(PAL_FAR), gg_far_pal + ch * 16, 16);
    rs_pal_load(RS_PAL_BG(PAL_MID), gg_mid_pal + ch * 16, 16);
    rs_tiles_load(VR_BG4, gg_far_tiles + gg_far_tile_off[ch] * 32, gg_far_tile_count[ch]);
    rs_tiles_load(VR_BG3, gg_mid_tiles + gg_mid_tile_off[ch] * 32, gg_mid_tile_count[ch]);
    rs_color top = gg_sky[ch * 2], bottom = gg_sky[ch * 2 + 1];
    for (int y = 0; y < RS_SCREEN_H; y++) {
        rs_color c = hu_lerp_color(top, bottom, y * 256 / RS_SCREEN_H);
        if (y & 4) c = hu_lerp_color(c, bottom, 10);      /* the house's dither step (no RGB555 bands) */
        line_col[y] = c;
    }
    sky_top = top;
    deep_col = gg_terrain_pal[ch * 16 + 2];
    dark_col = gg_terrain_pal[ch * 16 + 1];
    panoramas(ch);
    cur_ch = ch;
}

/* ---- the room's map ----------------------------------------------------------------------------------------------- */
/* a cell as drawn: past the room's sides its border carried on; below it, its bottom row; above, its top row (2 cells) */
static int draw_cell(const room *r, int cx, int cy)
{
    if (cy < -2) return C_EMPTY;
    int bx = cx < 0 ? 0 : cx >= ROOM_W ? ROOM_W - 1 : cx, by = cy < 0 ? 0 : cy >= ROOM_H ? ROOM_H - 1 : cy;
    int c = r->cell[by][bx];
    if (bx != cx || by != cy) return c == C_ROCK ? C_ROCK : C_EMPTY;
    return c;
}

static uint32_t hsh(int a, int b, int c) { uint32_t v = 2166136261u; v = (v ^ (uint32_t)a) * 16777619u;
    v = (v ^ (uint32_t)b) * 16777619u; v = (v ^ (uint32_t)c) * 16777619u; return v ^ (v >> 13); }

/* the map entries of one cell (cx, cy) at map tile (mx, my) */
static void put_cell(const room *r, int cx, int cy, int mx, int my, int salt)
{
    int c = draw_cell(r, cx, cy), mw = map_w();
    uint16_t e[4] = {0, 0, 0, 0};
#define TP(t) RS_MAP((t), PAL_TERRAIN, 0, 0, 0)
#define PP(t) RS_MAP((t), PAL_PROPS, 0, 0, 0)
    if (c == C_ROCK && cy > ROOM_H) {                   /* deep under the room: the mountain's flat dark */
        for (int q = 0; q < 4; q++) e[q] = TP(TT_DEEP);
    } else if (c == C_ROCK) {
        static const int dq[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
        for (int q = 0; q < 4; q++) {
            int sx = dq[q][0], sy = dq[q][1];
            int v = draw_cell(r, cx, cy + sy) == C_ROCK, hz = draw_cell(r, cx + sx, cy) == C_ROCK;
            int dg = draw_cell(r, cx + sx, cy + sy) == C_ROCK;
            int kind = !v && !hz ? 6 : !v ? 4 : !hz ? 5 : !dg ? 3 : (int)(hsh(cx, cy, salt) % 3);
            e[q] = TP(TT_ROCK_0_0 + q * 7 + kind);
        }
    } else if (c == C_SPIKE_U) {
        e[2] = e[3] = TP(TT_SPIKE_UP);
    } else if (c == C_SPIKE_D) {
        e[0] = e[1] = TP(TT_SPIKE_UP) | RS_MAP_VFLIP;
    } else if (c == C_SPIKE_R) {
        e[0] = e[2] = TP(TT_SPIKE_RIGHT);
    } else if (c == C_SPIKE_L) {
        e[1] = e[3] = TP(TT_SPIKE_RIGHT) | RS_MAP_HFLIP;
    } else if (c == C_LEDGE) {
        e[0] = PP(draw_cell(r, cx - 1, cy) == C_LEDGE ? TT_LEDGE_1 : TT_LEDGE_0);
        e[1] = PP(draw_cell(r, cx + 1, cy) == C_LEDGE ? TT_LEDGE_1 : TT_LEDGE_2);
    } else if (c == C_CRUMBLE) {
        for (int q = 0; q < 4; q++) e[q] = PP(TT_CRUMBLE_0 + q);
    } else if (c == C_EMPTY && draw_cell(r, cx, cy + 1) == C_ROCK && cy + 1 < ROOM_H && cy >= 0) {
        uint32_t k = hsh(cx, cy, salt + 7);                 /* a decoration on the rock's top */
        if (k % 100 < 34) e[2] = TP(TT_DECOR_0 + (k >> 8) % 4);
        if ((k >> 12) % 100 < 30) e[3] = TP(TT_DECOR_0 + (k >> 16) % 4) | ((k >> 20) & 1 ? RS_MAP_HFLIP : 0);
    }
#undef TP
#undef PP
    for (int q = 0; q < 4; q++) {
        int x = (mx + (q & 1)) & (mw - 1), y = (my + q / 2) & 63;
        rs_bg_put(RS_BG2, x, y, e[q]);
    }
}

static void room_map(const room *r, int salt)
{
    int mw = map_w(), margin = (mw * 8 - ROOM_PX_W) / 2;  /* the map's columns past the room, half on each side */
    (void)margin;
    /* the room's rows and those past it: map rows 0..29 the stage, 30..55 below it, 56..63 above it */
    for (int my = 0; my < 64; my += 2) {
        int cy = my < 56 ? my / 2 : (my - 64) / 2;
        for (int mx = 0; mx < mw; mx += 2) {
            int sc = mx < 40 + (mw - 40) / 2 ? mx : mx - mw;   /* the stage's tile column (negative: left margin) */
            int cx = sc >= 0 ? sc / 2 : -((1 - sc) / 2);
            put_cell(r, cx, cy, mx, my, salt);
        }
    }
    memset(crumble_shown, 0, sizeof crumble_shown);
}

void draw_room(const room *r, int chapter, int room_no)
{
    if (laid_w != rs_screen_width() || laid_h != rs_screen_height()) layers_setup();
    if (chapter != cur_ch) load_chapter(chapter);
    room_map(r, chapter * 37 + room_no);
    title_room = -1;
    memset(parts, 0, sizeof parts);
    memset(trail, 0, sizeof trail);
    ui_key = -1;
}

/* the crumbling blocks' cells follow their state (there, shaking: a sprite instead, gone: dotted) */
static void crumbles(const world *w, const room *r)
{
    int mw = map_w();
    for (int i = 0; i < r->ncrumble; i++) {
        int s = w->crumble[i] == 0 ? 0 : w->crumble[i] > 0 ? 1 : 2;
        if (crumble_shown[i] == s + 1) continue;
        crumble_shown[i] = (uint8_t)(s + 1);
        int cx = r->crumble_cx[i], cy = r->crumble_cy[i];
        for (int q = 0; q < 4; q++) {
            uint16_t e = s == 0 ? RS_MAP(TT_CRUMBLE_0 + q, PAL_PROPS, 0, 0, 0)
                       : s == 2 ? RS_MAP(TT_CRUMBLE_GONE_0 + q, PAL_PROPS, 0, 0, 0) : 0;
            rs_bg_put(RS_BG2, (cx * 2 + (q & 1)) & (mw - 1), cy * 2 + q / 2, e);
        }
    }
}

/* ---- set-up ------------------------------------------------------------------------------------------------------------ */
void draw_init(void)
{
    if (tall()) rs_screen_origin_v((rs_screen_height() - RS_SCREEN_H) / 3 & ~7);   /* the room a third of the way down */
    rs_screen_origin(rs_screen_margin());              /* the room centred (widescreen) */
    hu_config hc = hu_defaults();
    hc.obj_tile = KIT_OBJ_TILE;
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    layers_setup();
    rs_tiles_load(VR_BG2, gg_terrain_tiles, gg_terrain_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_PROPS), gg_props_pal, 16);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gg_obj_tiles, gg_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gg_obj_pals, 48);
    rs_pal_load(RS_PAL_OBJ(4), gg_obj_pals + 64, 32);
    rs_color flash[16];                                /* OBJ 6: the respawn's flash, the kid all light */
    flash[0] = 0;
    flash[1] = RS_RGB8(255, 255, 255);
    for (int i = 2; i < 16; i++) flash[i] = RS_RGB8(255, 246, 220);
    rs_pal_load(RS_PAL_OBJ(6), flash, 16);
    rs_math(RS_MATH_ADD | RS_MATH_HALF, RS_MATH_OBJ, 0);   /* OBJ 4-7: the trail, the dust, the flash: see-through */
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_raster(raster, NULL);
    load_chapter(0);
}

/* after a state was loaded: one of another width or height (the phone turned) brings that screen's maps and UI */
void draw_relayout(void)
{
    int shape = ht_shape_reset();
    if (!shape && laid_w == rs_screen_width() && laid_h == rs_screen_height()) return;
    ui_key = -1;
    hu_pause(0, 13);
}

/* ---- effects --------------------------------------------------------------------------------------------------------- */
static void spawn(int kind, int x, int y, int vx, int vy, int life)
{
    int best = 0;
    for (int i = 0; i < PART_N; i++) {
        if (!parts[i].life) { best = i; break; }
        if (parts[i].t > parts[best].t) best = i;
    }
    parts[best] = (part){(int16_t)(x * 16), (int16_t)(y * 16), (int16_t)vx, (int16_t)vy, 0, (uint8_t)life,
                         (uint8_t)kind, 0};
}

static int kid_sprite(const world *w);

void draw_events(const world *w, const room *r)
{
    const hero *h = &w->h;
    int ev = (int)w->events, fx = h->x + HB_W / 2, fy = h->y + HB_H;
    (void)r;
    if (ev & (EV_JUMP | EV_SPRING)) {
        spawn(PK_DUST, fx - 5, fy - 3, -10, -4, 14);
        spawn(PK_DUST, fx + 5, fy - 3, 10, -4, 14);
    }
    if (ev & EV_WALLJUMP) {
        int side = h->vx > 0 ? -1 : 1;
        spawn(PK_DUST, fx + side * 7, fy - 8, side * 6, -6, 14);
        spawn(PK_DUST, fx + side * 7, fy - 3, side * 4, 2, 12);
    }
    if (ev & EV_LAND) {
        int n = h->land_v > 700 ? 3 : h->land_v > 350 ? 2 : 1;
        for (int i = 0; i < n; i++) {
            spawn(PK_DUST, fx - 4 - 3 * i, fy - 3, -12 - 6 * i, -3, 12 + 2 * i);
            spawn(PK_DUST, fx + 4 + 3 * i, fy - 3, 12 + 6 * i, -3, 12 + 2 * i);
        }
    }
    if (ev & EV_DASH) hu_shake(2, 8);
    if (ev & EV_DEATH) {
        hu_shake(3, 12);
        for (int i = 0; i < 8; i++) {
            static const int8_t dx[8] = {16, 11, 0, -11, -16, -11, 0, 11}, dy[8] = {0, 11, 16, 11, 0, -11, -16, -11};
            spawn(PK_BURST, fx, fy - 7, dx[i] * 2, dy[i] * 2, 20);
        }
    }
    if (ev & EV_RESPAWN)
        for (int i = 0; i < 6; i++) spawn(PK_SPARK, fx + (i - 3) * 5, fy - 6 - (i & 1) * 6, 0, -6, 18);
    if (ev & EV_BERRY)
        for (int i = 0; i < 5; i++) spawn(PK_SPARK, r->berry_x + (i - 2) * 5, r->berry_y - (i & 1) * 5, (i - 2) * 4, -10, 22);
    if (ev & EV_GEM)
        for (int i = 0; i < 6; i++) spawn(PK_SHARD, fx + (i - 3) * 3, fy - 8, (i - 3) * 8, -14 + (i & 1) * 6, 18);
    if (ev & EV_CRUMBLE) spawn(PK_DUST, fx, fy + 2, 0, 4, 16);
    /* the dash's trail: a ghost every other frame */
    if ((h->dash_t && (h->dash_t & 1)) || (ev & EV_SUPER)) {
        trail[trail_k] = (ghost){(int16_t)(h->x + HB_W / 2 - 12), (int16_t)(h->y + HB_H - 23),
                                 (uint8_t)kid_sprite(w), (uint8_t)(h->facing < 0), 1};
        trail_k = (trail_k + 1) % TRAIL_N;
    }
    if (h->grounded && h->run_t && h->run_t % 10 == 0 && (h->vx > 400 || h->vx < -400))
        spawn(PK_DUST, fx - (h->vx > 0 ? 5 : -5), fy - 3, h->vx > 0 ? -6 : 6, -2, 10);
    if (h->sliding && (w->t & 7) == 0) spawn(PK_DUST, fx + h->wall_dir * 6, fy - 4, 0, -2, 10);
    /* the effects move */
    for (int i = 0; i < PART_N; i++) {
        part *p = &parts[i];
        if (!p->life) continue;
        p->x = (int16_t)(p->x + p->vx);
        p->y = (int16_t)(p->y + p->vy);
        if (p->kind == PK_SHARD || p->kind == PK_SPARK) p->vy = (int16_t)(p->vy + 1);
        else p->vx = (int16_t)(p->vx * 7 / 8);
        if (++p->t >= p->life) p->life = 0;
    }
    for (int i = 0; i < TRAIL_N; i++)
        if (trail[i].t && ++trail[i].t > 12) trail[i].t = 0;
    tick++;
}

/* ---- sprites ---------------------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal, int flags)
{
    const gg_sprite_def *d = &gg_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= rs_screen_top() - (int)d->h ||
        y >= rs_screen_bottom())
        return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

static int kid_sprite(const world *w)
{
    const hero *h = &w->h;
    if (h->dead_t) return SPR_KID_DEAD;
    if (h->freeze_t || h->dash_t) return h->dash_dy < 0 && !h->dash_dx ? SPR_KID_DASH_UP : h->dash_dy < 0 ? SPR_KID_JUMP
                                         : SPR_KID_DASH;
    if (h->land_t < 5 && h->grounded) return SPR_KID_SQUASH;
    if (!h->grounded) {
        if (h->sliding) return SPR_KID_SLIDE;
        if (h->jump_t < 5 && h->vy < 0) return SPR_KID_STRETCH;
        return h->vy < 0 ? SPR_KID_JUMP : SPR_KID_FALL;
    }
    if (h->vx > 96 || h->vx < -96) return SPR_KID_RUN + (h->run_t / 4) % 6;
    return SPR_KID_IDLE + (w->t / 40) % 2;
}

static void draw_kid(const world *w, int sx, int sy)
{
    const hero *h = &w->h;
    int x = h->x + HB_W / 2 - 12 + sx, y = h->y + HB_H - 23 + sy;
    if (h->dead_t > 6) return;                          /* the burst's puffs instead */
    int pal = h->dash_avail ? 0 : 1, flags = h->facing < 0 ? RS_SPR_HFLIP : 0;
    int s = kid_sprite(w);
    if (s == SPR_KID_SLIDE && h->wall_dir) flags = h->wall_dir < 0 ? RS_SPR_HFLIP : 0;
    /* the dash's freeze frame and the respawn: a white glow over the kid (OBJ 6, added half) */
    if (h->freeze_t || (h->spawn_t && (h->spawn_t / 2) % 2)) spr(s, x, y, 2, 6, flags);
    spr(s, x, y, 2, pal, flags);
}

static void draw_props(const world *w, const room *r, int sx, int sy)
{
    for (int i = 0; i < r->nplats; i++) {
        int n = r->plat[i].w / CELL;
        for (int k = 0; k < n; k++)
            spr(SPR_PLANK + (n == 1 ? 1 : k == 0 ? 0 : k == n - 1 ? 2 : 1), w->plat_x[i] + k * CELL + sx, w->plat_y[i] + sy, 2, -1, 0);
    }
    for (int i = 0; i < r->nsprings; i++)
        spr(SPR_SPRING + (w->spring_t[i] > 8 ? 1 : w->spring_t[i] > 0 ? 2 : 0), r->spring_x[i] + sx, r->spring_y[i] + sy, 2, -1, 0);
    for (int i = 0; i < r->ngems; i++) {
        int bob = ((tick / 10 + i * 3) % 8) < 4 ? 0 : 1;
        if (w->gem_t[i]) spr(SPR_GEM + 4, r->gem_x[i] - 8 + sx, r->gem_y[i] - 8 + sy, 2, -1, 0);
        else spr(SPR_GEM + (tick / 12 + i) % 4, r->gem_x[i] - 8 + sx, r->gem_y[i] - 8 - bob + sy, 2, -1, 0);
    }
    if (r->has_berry) {
        int bob = hu_bob(tick + 20, 64, 2);
        if (w->berry_got) {                             /* it follows the kid, bobbing, until the way out */
            const hero *h = &w->h;
            spr(SPR_BERRY + (tick / 16) % 2, h->x + HB_W / 2 - 8 - h->facing * 14 + sx, h->y - 12 + bob + sy, 2, -1, 0);
        } else {
            spr(SPR_BERRY + (tick / 16) % 2, r->berry_x - 8 + sx, r->berry_y - 8 + bob + sy, 2, -1, 0);
        }
    }
    for (int i = 0; i < r->ncrumble; i++)
        if (w->crumble[i] > 0) {
            int j = (w->crumble[i] / 2) % 3 - 1;
            spr(SPR_CRUMBLE, r->crumble_cx[i] * CELL + j + sx, r->crumble_cy[i] * CELL + sy, 2, -1, 0);
        }
}

static void draw_effects(int sx, int sy)
{
    for (int i = 0; i < PART_N; i++) {
        const part *p = &parts[i];
        if (!p->life) continue;
        int x = p->x / 16 - 4 + sx, y = p->y / 16 - 4 + sy, f = p->t * 3 / p->life;
        switch (p->kind) {
        case PK_DUST: spr(SPR_DUST + (f > 2 ? 2 : f), x, y, 2, -1, 0); break;
        case PK_BURST: spr(SPR_BURST + (p->t > p->life / 2), x, y, 2, -1, 0); break;
        case PK_SPARK: hu_sparkle(x - hu_ui_x(), y, p->t / 4, 2); break;
        case PK_SHARD: spr(SPR_FLAKE, x, y, 2, -1, 0); break;
        default: break;
        }
    }
    for (int i = 0; i < TRAIL_N; i++) {
        const ghost *g = &trail[(trail_k + TRAIL_N - 1 - i) % TRAIL_N];
        if (g->t > 2) spr(g->spr, g->x + sx, g->y + sy, 2, 4, g->flip ? RS_SPR_HFLIP : 0);
    }
}

/* the wind's streaks and the chapters' flakes (petals, snow): from the frame count, nothing to save */
static void weather(const world *w, const room *r, int ch)
{
    int top = rs_screen_top(), hgt = rs_screen_bottom() - top, left = rs_screen_left(), wd = rs_screen_right() - left;
    if ((r->wind_x || r->wind_y) && w->wind_now) {
        int dir = r->wind_x >= 0 ? 1 : -1;
        for (int i = 0; i < 14; i++) {
            int sp = 3 + i % 3;
            int x = left + (int)((i * 97 + tick * sp * dir) % wd + wd) % wd;
            int y = top + (i * 53 + (int)(hsh(i, 0, 1) % (unsigned)hgt)) % hgt + (r->wind_y * tick / 256) % 40;
            spr(SPR_STREAK + (i & 1), x, y, 2, -1, dir < 0 ? RS_SPR_HFLIP : 0);
        }
    }
    if (ch == 0 || ch == 4) {                           /* petals in the meadow, snow on the summit */
        int n = ch == 4 ? 16 : 6;
        for (int i = 0; i < n; i++) {
            int x = left + (int)((i * 131 + tick / 3 + (int)((r->wind_x * tick) / 512)) % wd + wd) % wd;
            int y = top + (i * 71 + tick / (ch == 4 ? 2 : 3)) % hgt;
            spr(SPR_FLAKE + (ch == 0 ? 1 : 0), x, y, 2, -1, 0);
        }
    }
}

/* ---- the UI ------------------------------------------------------------------------------------------------------------- */
static void fmt_time(char *s, int n, int frames)
{
    int cs = frames % 60 * 100 / 60, sec = frames / 60;
    snprintf(s, (size_t)n, "%d:%02d.%02d", sec / 60, sec % 60, cs);
}

static const char *chapter_name(int c)
{
    switch (c) {
    case 0: return TR(CH_1);
    case 1: return TR(CH_2);
    case 2: return TR(CH_3);
    case 3: return TR(CH_4);
    default: return TR(CH_5);
    }
}

static const char *hint_text(int k)
{
    switch (k) {
    case 1: return TR(HINT_JUMP);
    case 2: return TR(HINT_WALL);
    case 3: return TR(HINT_DASH);
    case 4: return TR(HINT_SPRING);
    case 5: return TR(HINT_GEM);
    case 6: return TR(HINT_CRUMBLE);
    case 7: return TR(HINT_RAFT);
    case 8: return TR(HINT_WIND);
    case 9: return TR(HINT_BERRY);
    default: return NULL;
    }
}

/* the HUD's line: the room, the deaths, the time, the berries (row 1; the icons are sprites) */
#define HUD_C_ROOM  5
#define HUD_C_DEATH 11
#define HUD_C_TIME  18
#define HUD_C_BERRY 29
static void hud_text(const view *v)
{
    char s[32];
    hu_clear_rows(0, HUD_ROWS);
    snprintf(s, sizeof s, "%d-%d", v->chapter + 1, v->room + 1);
    hu_text(HUD_C_ROOM, 1, s);
    snprintf(s, sizeof s, "%d", v->deaths);
    hu_text(HUD_C_DEATH + 1, 1, s);
    fmt_time(s, sizeof s, v->time);
    hu_text(HUD_C_TIME + 1, 1, s);
    snprintf(s, sizeof s, "%d/%d", v->berries, v->berries_max);
    hu_text(HUD_C_BERRY + 1, 1, s);
}
static void hud_icons(int dy)
{
    int ux = hu_ui_x();
    spr(SPR_ICON + 0, HUD_C_DEATH * 8 + ux, 8 + dy, 3, -1, 0);
    spr(SPR_ICON + 2, HUD_C_TIME * 8 + ux, 8 + dy, 3, -1, 0);
    spr(SPR_ICON + 1, HUD_C_BERRY * 8 + ux, 8 + dy, 3, -1, 0);
}

/* the title's chapter lines (rows y..y+4): < CHAPTER n >, its name, its records or how to open it */
static void chapter_lines(const view *v, int y, int narrow)
{
    char s[96], t[32];
    int c = v->sel;
    rs_strf(s, sizeof s, TR(CHAPTER_N), c + 1);
    char line[128];
    snprintf(line, sizeof line, "%s %s %s", c > 0 ? HU_ARROW_LEFT : " ", s, c < v->unlocked ? HU_ARROW_RIGHT : " ");
    if (narrow) hu_text(ht_center(line, 0), y, line);
    else hu_text(hu_center(line, 0), y, line);
    const char *nm = chapter_name(c);
    if (narrow) ht_text(y + 2, nm, 0);
    else hu_text(hu_center(nm, 0), y + 2, nm);
    if (v->cleared[c]) {
        fmt_time(t, sizeof t, v->best_time[c]);
        rs_strf(s, sizeof s, TR(RECORD), t, v->best_deaths[c]);
    } else {
        snprintf(s, sizeof s, "%s", TR(NOT_YET));
    }
    if (narrow) ht_text(y + 4, s, 0);
    else hu_text(hu_center(s, 0), y + 4, s);
    rs_strf(s, sizeof s, TR(BERRIES_N), v->best_berries[c], v->berry_count[c]);
    if (narrow) ht_text(y + 6, s, 0);
    else hu_text(hu_center(s, 0), y + 6, s);
}

/* the results: the banner on row 4, the panel from row 9: per line its label (and NEW BEST) centred, its value in 2x
 * gold under it (rows 11, 15, 19); the retry line on row 23; laid out alike in landscape and upright */
#define RES_ROW(i)  (11 + 4 * (i))
#define RES_RETRY   23
static int res_center(const char *s, int big, int narrow) { return narrow ? ht_center(s, big) : hu_center(s, big); }

static void results_panel(const view *v, int narrow)
{
    char s[96];
    const char *title = v->chapter == CHAPTERS - 1 ? TR(SUMMIT) : TR(CLEAR);
    const char *lab[3] = {TR(RES_TIME), TR(RES_DEATHS), TR(RES_BERRIES)};
    int news[3] = {v->new_time, v->new_deaths, v->new_berries};
    int w = narrow ? ht_ncols() : 26;
    if (narrow) ht_banner(4, title);
    else hu_banner(4, title);
    hu_panel(narrow ? ht_center_w(w) : (40 - w) / 2, 9, w, 16);
    for (int i = 0; i < 3; i++) {
        if (news[i]) snprintf(s, sizeof s, "%s - %s", lab[i], HTR(NEW_BEST));
        if (!news[i] || hu_width(s, 0) > w - 2) snprintf(s, sizeof s, "%s", lab[i]);
        hu_box_text(res_center(s, 0, narrow), RES_ROW(i), s);
    }
}

static void results_values(const view *v, int narrow)
{
    char s[3][32];
    fmt_time(s[0], 32, v->time);
    snprintf(s[1], 32, "%d", v->deaths);
    snprintf(s[2], 32, "%d/%d", v->berries, v->berries_max);
    for (int i = 0; i < 3; i++) hu_big(res_center(s[i], 1, narrow), RES_ROW(i) + 1, s[i], HU_BIG_BANNER);
}

/* the title's scene: the chapter's sky and layers, a ledge of its rock framed by cliffs (BG2) */
static const char *TITLE_SCENE =
    "room\n"
    "##................##\n" "##................##\n" "##................##\n" "#..................#\n"
    "#..................#\n" "#..................#\n" "#..................#\n" "##................##\n"
    "##................##\n" "###..............###\n" "###..............###\n" "####............####\n"
    "####S...........####\n" "####################\n" "####################\n";

static void title_scene(int ch)
{
    if (title_room == ch) return;
    room tr;
    char err[80];
    if (room_parse(TITLE_SCENE, &tr, err, sizeof err)) return;
    if (laid_w != rs_screen_width() || laid_h != rs_screen_height()) layers_setup();
    if (ch != cur_ch) load_chapter(ch);
    room_map(&tr, 999);
    title_room = ch;
    ui_key = -1;
}

void draw_frame(const world *w, const room *r, const view *v)
{
    int narrow = tall() && ht_narrow();
    int sx = hu_shake_x(), sy = hu_shake_y();
    if (laid_w != rs_screen_width() || laid_h != rs_screen_height()) {
        layers_setup();
        cur_ch = -1;
        title_room = -2;
        ui_key = -1;
    }
    if (v->state == ST_TITLE) title_scene(v->sel);
    else if (title_room != -1 || cur_ch != v->chapter) {
        if (cur_ch != v->chapter) load_chapter(v->chapter);
        room_map(r, v->chapter * 37 + v->room);
        title_room = -1;
    }
    rs_screen_fill(1);
    rs_screen_focus_x(-1);
    /* the UI's pins (a tall screen): the HUD at the screen's top during play, the title and the panels on the screen */
    int pin = !tall() ? HT_PIN_OFF : v->state == ST_TITLE ? HT_PIN_TITLE : v->state == ST_RESULTS ? HT_PIN_PANEL : HT_PIN_OFF;
    int hud_pinned = tall() && v->state == ST_PLAY;
    rs_bg_pin(RS_BG1, 0, hud_pinned ? HUD_ROWS * 8 : 0, 4);
    if (pin) ht_pin(pin);
    r_ui_pin = pin;
    r_hud = hud_pinned;
    /* parallax: the far layers lean with the kid */
    int kx = v->state == ST_TITLE ? ROOM_PX_W / 2 : w->h.x + HB_W / 2, ky = v->state == ST_TITLE ? 120 : w->h.y;
    int drift = tick / 8;
    r_bg4x = ((kx - 160) / 16 + drift / 4 - sx / 2) & (map_w() * 8 - 1);
    r_bg4y = ((ky - 120) / 24) & 255;
    r_bg3x = ((kx - 160) / 6 - sx) & (map_w() * 8 - 1);
    r_bg3y = ((ky - 120) / 12 - sy / 2) & 255;
    r_bg2x = -sx & (map_w() * 8 - 1);
    r_bg2y = -sy & 511;
    rs_bg_scroll(RS_BG2, r_bg2x, r_bg2y);
    rs_bg_scroll(RS_BG3, r_bg3x, r_bg3y);
    rs_bg_scroll(RS_BG4, r_bg4x, r_bg4y);
    rs_brightness(15 - v->fade * 15 / 16 > 0 ? 15 - v->fade * 15 / 16 : 0);
    if (v->state == ST_PLAY) crumbles(w, r);

    /* the UI layer: redrawn when what it shows changes */
    int show_hud = v->state == ST_PLAY && (tall() || v->hud_t < HUD_SHOW || v->paused);
    int hint = v->state == ST_PLAY && v->hud_t < HINT_SHOW && r->hint ? r->hint : 0;
    int key = v->state * 7919 + v->sel * 131 + (v->state == ST_RESULTS && v->st_t >= RETRY_LOCK) * 3 +
              show_hud * 17 + hint * 29 + v->paused * 53 + v->room * 1031 + v->chapter * 7 +
              (show_hud ? (v->deaths * 37 + v->time / 6 * 3 + v->berries * 101) : 0) + narrow * 11;
    if (key != ui_key) {
        hu_clear();
        if (v->state == ST_TITLE) {
            if (narrow) ht_logo("GOAT GORGE", title_ramps, 2, HT_ROW_LOGO, 0);
            else hu_logo("GOAT GORGE", title_ramps, 2, 2, 0);
        } else if (v->state == ST_RESULTS) {
            results_panel(v, narrow);
            results_values(v, narrow);
        } else {
            if (show_hud) hud_text(v);
            if (hint) {
                const char *s = hint_text(hint);
                if (s) {
                    if (narrow) ht_text(HINT_ROW, s, 0);
                    else hu_text(hu_center(s, 0), HINT_ROW, s);
                }
            }
            if (v->paused) {
                const char *q = TR(PAUSE_QUIT);
                if (narrow) ht_text(16, q, 0);
                else hu_text(hu_center(q, 0), 16, q);
            }
        }
        hu_pause(0, 13);
        ui_key = key;
    }
    if (v->state == ST_TITLE && !narrow) {
        hu_title_draw(v->st_t, 0);
        chapter_lines(v, 12, 0);
    }
    if (v->state == ST_RESULTS) {
        if (narrow) ht_retry_line_at(RES_RETRY, v->st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(RES_RETRY, v->st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
    int slide = v->state == ST_RESULTS ? hu_slide_in(v->st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, -hu_ui_x() & (rs_bg_map_w(RS_BG1) * 8 - 1), -slide & 255);
    if (narrow) ht_pause(v->paused, 13);
    else hu_pause(v->paused, 13);

    /* sprites, front to back */
    rs_oam_clear();
    if (v->state == ST_TITLE) {
        int bob = hu_bob(v->st_t, 64, 3);
        if (narrow) {
            ht_title tc = {TR(VERB), 1, 15, 0, NULL};
            ht_title_out to;
            ht_title_draw(&tc, v->st_t, 0, &to);
            chapter_lines(v, 16, 1);
            ht_title_sprites(&tc, &to, v->st_t, NULL, NULL);
            spr(SPR_KID_IDLE + (v->st_t / 32) % 2, ht_mid_x() - 12 + hu_ui_x(), -40 + ht_pin_dy(HT_PIN_TITLE) + bob, 2, 0, 0);
        } else {
            hu_title_sprites(v->st_t, NULL, NULL);
            spr(SPR_KID_IDLE + (v->st_t / 32) % 2, 148 + hu_ui_x(), 208 - 23, 2, 0, 0);
        }
        return;
    }
    if (v->state == ST_RESULTS) {
        if (!narrow) spr(SPR_KID_IDLE + (v->st_t / 32) % 2, 20 + hu_ui_x(), 208 - 23, 2, 0, 0);
        return;
    }
    if (show_hud) hud_icons(hud_pinned ? rs_screen_top() + 4 - 0 : 0);
    draw_kid(w, sx, sy);
    draw_effects(sx, sy);
    draw_props(w, r, sx, sy);
    weather(w, r, v->chapter);
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(cur_ch); S(laid_w); S(laid_h); S(ui_key); S(crumble_shown); S(title_room); S(parts); S(trail); S(trail_k); S(tick);
    S(line_col); S(sky_top); S(deep_col); S(dark_col);
    RS_STATE_RASTER(raster);
}
#undef S
