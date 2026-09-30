/*
 * Bomber Mole: multiplayer (DESIGN.md "Multiplayer"): the split-screen views (SDK viewports), a camera per
 * player, the per-player HUD strips, the shared goal and the live map, the moles' colours and P1-P4 markers,
 * the join screen, co-op story levels and battle rounds (arenas, sudden death, results).
 * All rights reserved, 8BCraft.
 *
 * BG1 map in multiplayer (64 x 128 tiles):
 *   rows  0- 7  the HUD strips of P1..P4 (2 rows each, columns 0-19)
 *   rows  8- 9  the shared goal bar (co-op: grubs per depth), rows 10-11 the battle clock
 *   rows 12-27  the live map (2 x 2 depths of 10 x 7 tiles; the 4th square: the standings)
 *   columns 24-63 of rows 0-31: boxes (start, pause, rounds, results)
 *   rows 32-63, 64-95, 96-127: the marks (grub glow, hazards) of depths 0, 1, 2
 * BG3 and BG4 maps (64 x 128 tiles): depth d in slot d (256 px each), all three drawn at once. A view scrolls
 * to its mole's depth; changing depth slides that view alone, through the earth between the slots.
 */
#include "bm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LW (GW * CELL)                  /* the level in pixels (the cameras work for any size) */
#define LH (GH * CELL)
#define STRIP 16                        /* a HUD strip */
#define SLIDE 36                        /* frames of a depth change */
#define BOX_X 24                        /* BG1 columns of the boxes */
#define VR_OBJ_TILES 2048               /* the sprite tile base (draw.c) */
#define DIVIDER RS_HEX(0x101018)

enum { VK_HUD, VK_PLAY, VK_BOX };       /* what a viewport shows (palette 7, windows) */

typedef struct view {
    int player;                         /* whose view (or -1: a shared one) */
    int x, y, w, h;                     /* the play area on screen */
    int cx, cy;                         /* camera: the level pixel at the top-left, 8.8 */
    int d, from, slide;                 /* shown depth; depth change: the depth left and frames done */
} view;

static view V[MAX_PLAYERS];
static int nviews, merged, box_on, box_w, box_h;
static rs_viewport VP[RS_VIEW_MAX];
static uint8_t vp_kind[RS_VIEW_MAX], vp_view[RS_VIEW_MAX];
static int nvp;
extern int g_fog_eyes;
extern const uint8_t rs_font_5x7[96][8];

static int humans(void)
{
    int n = 0;
    for (int p = 0; p < MP.nplayers; p++) n += !MP.cpu[p];
    return n;
}

/* ---- the moles' colours: one art, only the miner's helmet changes colour (red, blue, green, yellow): the
   helmet's red ramp is recoloured at build time (tools/palette_variants.py), the fur, nose, claws and lamp
   never change. The P1-P4 markers use the same palette (the helmet's lightest shade on the outline). ---- */
static void helmet_palette(int slot, int colour)
{
    for (int i = 1; i < 16; i++) rs_pal_set(RS_PAL_OBJ(slot) + i, bm_mole_helmet_pals[colour & 3][i]);
}

void mp_mole_palettes(void)
{
    for (int p = 0; p < W.nplayers; p++) {
        int slot = W.mole_pal[p];
        if (p > 0 && slot == OBJ_PAL_MOLE) continue;           /* no palette left: it shares the first mole's */
        helmet_palette(slot, MP.colour[p]);
    }
}

/* the P1..P4 markers: "P" and the digit from the font, in the helmet's colour with a dark outline */
static void marker_tiles(void)
{
    for (int p = 0; p < MAX_PLAYERS; p++)
        for (int k = 0; k < 2; k++) {
            int g = k == 0 ? 'P' - 32 : '1' + p - 32;
            uint8_t t[64];
            memset(t, 0, sizeof t);
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++)
                    if (rs_font_5x7[g][y] & (0x80 >> x)) {
                        t[y * 8 + x] = MOLE_HELMET_INK;
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++) {
                                int xx = x + dx, yy = y + dy;
                                if (xx >= 0 && xx < 8 && yy >= 0 && yy < 8 && !t[yy * 8 + xx] &&
                                    !(rs_font_5x7[g][yy] & (0x80 >> xx))) t[yy * 8 + xx] = MOLE_OUTLINE;
                            }
                    }
            rs_tiles_load8(VR_OBJ_TILES + MP_MARK_TILE + p * 2 + k, t, 1);
        }
}

/* ---- cameras: a dead zone around the mole, then a smooth catch-up; clamped to the level ---- */
static void mole_px(const actor *m, int *x, int *y)
{
    *x = m->cx * CELL + (m->tx - m->cx) * m->prog / (SUB / CELL) + CELL / 2;
    *y = m->cy * CELL + (m->ty - m->cy) * m->prog / (SUB / CELL) + CELL / 2;
}

static int clamp_cam(int c, int view, int level)
{
    if (view >= level) return (level - view) / 2;           /* a small level: centred */
    return clampi(c, 0, level - view);
}

static void cam_update(view *v, int snap)
{
    const actor *m = world_player(v->player < 0 ? 0 : v->player);
    if (!m) return;
    int px, py;
    mole_px(m, &px, &py);
    if (merged) {                                          /* between the two moles */
        const actor *o = world_player(1);
        if (o) { int ox, oy; mole_px(o, &ox, &oy); px = (px + ox) / 2; py = (py + oy) / 2; }
    }
    int cx = v->cx >> 8, cy = v->cy >> 8, wx = cx, wy = cy;
    int dzx = v->w / 8, dzy = v->h / 8;
    if (px < cx + v->w / 2 - dzx) wx = px - (v->w / 2 - dzx);
    if (px > cx + v->w / 2 + dzx) wx = px - (v->w / 2 + dzx);
    if (py < cy + v->h / 2 - dzy) wy = py - (v->h / 2 - dzy);
    if (py > cy + v->h / 2 + dzy) wy = py - (v->h / 2 + dzy);
    wx = clamp_cam(wx, v->w, LW);
    wy = clamp_cam(wy, v->h, LH);
    if (snap) { v->cx = wx << 8; v->cy = wy << 8; }
    else { v->cx += ((wx << 8) - v->cx) / 6; v->cy += ((wy << 8) - v->cy) / 6; }
    v->cx = clamp_cam(v->cx >> 8, v->w, LW) << 8 | (v->cx & 255);
    v->cy = clamp_cam(v->cy >> 8, v->h, LH) << 8 | (v->cy & 255);
}

static int ease(int t, int n) { return t * t * (3 * n - 2 * t) / (n * n); }

/* the map row of the view's top edge: the depth's slot, sliding during a depth change */
static int view_scroll_y(const view *v)
{
    int y = v->d * 256 + (v->cy >> 8);
    if (v->slide) y += (v->from - v->d) * 256 * (SLIDE - ease(v->slide, SLIDE)) / SLIDE;
    return y;
}

/* the views: one per human (one per mole when only CPUs play), laid out by the SDK's standard layouts */
static void layout(rs_viewport *rects, int *nrect)
{
    int n = 0, who[MAX_PLAYERS];
    for (int p = 0; p < W.nplayers; p++)
        if (!MP.cpu[p] || humans() == 0) who[n++] = p;
    if (!n) who[n++] = 0;
    /* co-op option: two moles close together on one depth share one full-screen view */
    const actor *a = world_player(0), *b = world_player(1);
    if (MP.merge && W.mode == MODE_COOP && n == 2 && a && b && a->depth == b->depth && a->state != 99 && b->state != 99) {
        int dx = abs(a->cx - b->cx), dy = abs(a->cy - b->cy);
        if (merged ? (dx <= 14 && dy <= 11) : (dx <= 9 && dy <= 8)) merged = 1;
        else merged = 0;
    } else {
        merged = 0;
    }
    int flags = (MP.split_h ? RS_LAYOUT_HSPLIT : 0) | (MP.map3 ? RS_LAYOUT_MAP : 0);
    *nrect = rs_viewport_layout(merged ? 1 : n, flags, rects);
    nviews = merged ? 1 : n;
    for (int i = 0; i < nviews; i++) {
        view *v = &V[i];
        int p = merged ? 0 : who[i];
            int newp = v->player != p, changed = newp || v->w != rects[i].w || v->h != rects[i].h - STRIP;
        v->x = rects[i].x;
        v->y = rects[i].y + STRIP;
        v->w = rects[i].w;
        v->h = rects[i].h - STRIP;
        if (changed) {                              /* a new player snaps; a merge or a split glides */
            if (newp) { v->slide = 0; v->d = world_player(p) ? world_player(p)->depth : 0; }
            v->player = p;
            cam_update(v, newp);
        }
    }
}

/* ---- level start and the frame's update ---- */
void mp_level_start(void)
{
    rs_bg_setup(RS_BG4, 64, 128, 1024);
    rs_bg_setup(RS_BG3, 64, 128, 1024);
    rs_bg_setup(RS_BG1, 64, 128, 0);
    for (int d = 0; d < NDEPTH; d++) draw_playfield_full(d, d);
    mp_mole_palettes();
    marker_tiles();
    merged = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) { V[i].player = -1; V[i].slide = 0; }
    rs_viewport r[RS_VIEW_MAX];
    int nr;
    layout(r, &nr);
    for (int i = 0; i < nviews; i++) cam_update(&V[i], 1);
    for (int p = 0; p < MAX_PLAYERS; p++) W.depth_event[p] = 0;
}

void mp_views_off(void)
{
    rs_viewports(0, NULL, 0);
    spr_camera(0, 0, HUD_H, 0, 0);
    box_on = 0;
}

/* after world_update: depth changes start the views' slides, the cameras follow */
void mp_update_views(void)
{
    for (int i = 0; i < nviews; i++) {
        view *v = &V[i];
        const actor *m = world_player(v->player < 0 ? 0 : v->player);
        if (v->slide && ++v->slide >= SLIDE) v->slide = 0;
        if (m && m->depth != v->d) {                        /* a hole, a ladder, a pipe, the bucket, a respawn */
            v->from = v->d;
            v->d = m->depth;
            v->slide = 1;
            sfx(SFX_DEPTH);
        }
        cam_update(v, 0);
    }
    for (int p = 0; p < MAX_PLAYERS; p++) W.depth_event[p] = 0;
}

/* ---- BG1: HUD strips, the goal bar, the clock, the live map, boxes ---- */
static void meta(int mx, int my, int m) { rs_bg_meta(RS_BG1, mx, my, bm_hud_meta[m]); }

static int battle_wins[MAX_PLAYERS];

static void hud_strip(int p)
{
    const pstats *ps = &W.ps[p];
    const actor *m = world_player(p);
    int y = p;                                              /* metatile row */
    meta(0, y, HUD_PANEL);
    if (m && m->state == 99 && W.mode == MODE_COOP && m->timer > 90) {   /* knocked out: back in N s */
        meta(1, y, HUD_DANGER);
        meta(2, y, HUD_DIGIT + clampi((m->timer + 59) / 60, 0, 9));
    } else {
        meta(1, y, HUD_HEART);
        meta(2, y, HUD_DIGIT + clampi(m && m->state == 99 ? 0 : ps->hearts, 0, 9));
    }
    meta(3, y, HUD_BOMB);  meta(4, y, HUD_DIGIT + clampi(ps->bombs, 0, 9));
    meta(5, y, HUD_FIRE);  meta(6, y, HUD_DIGIT + clampi(ps->range, 0, 9));
    if (W.mode == MODE_BATTLE) {
        meta(7, y, HUD_CHECK); meta(8, y, HUD_DIGIT + clampi(battle_wins[p], 0, 9)); meta(9, y, HUD_PANEL);
    } else {
        int g = clampi(W.grubs_left, 0, 99);
        meta(7, y, HUD_GRUB); meta(8, y, g >= 10 ? HUD_DIGIT + g / 10 : HUD_PANEL); meta(9, y, HUD_DIGIT + g % 10);
    }
}

static void goal_bar(void)
{
    static const int icons[NDEPTH] = {HUD_DEPTH_SURFACE, HUD_DEPTH_UNDER1, HUD_DEPTH_UNDER2};
    for (int d = 0; d < NDEPTH; d++) {
        int n = ui_grubs(d);
        meta(d * 2, 4, icons[d]);
        meta(d * 2 + 1, 4, n ? HUD_DIGIT + clampi(n, 0, 9) : HUD_CHECK);
    }
}

static int round_no, round_left;
static void clock_bar(void)
{
    char s[40];
    if (W.sd_on) snprintf(s, sizeof s, "  SUDDEN DEATH!    ");
    else snprintf(s, sizeof s, " ROUND %d   %d:%02d   ", round_no, round_left / 3600, round_left / 60 % 60);
    rs_text_setup(RS_BG1, 700, 0, 1);                       /* the opaque box font */
    for (int x = 0; x < 20; x++) rs_bg_put(RS_BG1, x, 10, RS_MAP(800, 0, 1, 0, 0));
    rs_text(0, 10, s);
    rs_text_setup(RS_BG1, 0, 0, 1);
}

static void live_map(void)
{
    ui_build_map();
    for (int d = 0; d < NDEPTH; d++)
        for (int ty = 0; ty < 7; ty++)
            for (int tx = 0; tx < 10; tx++)
                rs_bg_put(RS_BG1, (d % 2) * 10 + tx, 12 + (d / 2) * 7 + ty, RS_MAP(UI_T_MAP + d * 70 + ty * 10 + tx, 0, 1, 0, 0));
    for (int y = 19; y < 26; y++)
        for (int x = 10; x < 20; x++) rs_bg_put(RS_BG1, x, y, RS_MAP(800, 0, 1, 0, 0));
    rs_text_setup(RS_BG1, 700, 0, 1);
    if (W.mode == MODE_BATTLE) {
        rs_text(11, 20, "WINS");
        for (int p = 0; p < W.nplayers; p++) rs_textf(11, 21 + p, "P%d  %d", p + 1, battle_wins[p]);
    } else {
        rs_text(11, 20, "GRUBS");
        static const char *const nm[NDEPTH] = {"TOP", "MID", "DEEP"};
        for (int d = 0; d < NDEPTH; d++) {
            int n = ui_grubs(d);
            if (n) rs_textf(11, 21 + d, "%-4s %d", nm[d], n);
            else rs_textf(11, 21 + d, "%-4s OK", nm[d]);
        }
    }
    rs_text_setup(RS_BG1, 0, 0, 1);
}

/* a box in the boxes' area (BG1 columns 24-63), shown centred over the views */
static void box_clear(void)
{
    for (int y = 0; y < 32; y++)
        for (int x = BOX_X; x < 64; x++) rs_bg_put(RS_BG1, x, y, 0);
}

static void box(int w, int h)
{
    box_clear();
    text_box(BOX_X, 0, w, h);
    box_on = 1;
    box_w = w;
    box_h = h;
}

static void box_line(int y, const char *s)
{
    int x = BOX_X + (box_w - (int)strlen(s)) / 2;
    text_at(x, y, s);
}

void mp_box_off(void) { box_on = 0; box_clear(); rs_text_setup(RS_BG1, 0, 0, 1); }

/* ---- the frame's picture ---- */
static void add_vp(int kind, int view_i, int x, int y, int w, int h)
{
    if (nvp >= RS_VIEW_MAX) return;
    rs_viewport *p = &VP[nvp];
    memset(p, 0, sizeof *p);
    p->x = (int16_t)x; p->y = (int16_t)y; p->w = (int16_t)w; p->h = (int16_t)h;
    p->layers = 1 << RS_BG1;
    vp_kind[nvp] = (uint8_t)kind;
    vp_view[nvp] = (uint8_t)view_i;
    nvp++;
}

static int lamp_x[MAX_PLAYERS], lamp_y[MAX_PLAYERS], lamp_r;

static int isqrt(int v) { int r = 0; while ((r + 1) * (r + 1) <= v) r++; return r; }

static void mp_raster(int line, void *u)
{
    (void)u;
    int i = rs_viewport_current();
    if (i < 0) return;
    rs_window(0, 0, 0);                                     /* no HUD band in the views */
    draw_pal7(vp_kind[i] != VK_PLAY);
    if (vp_kind[i] == VK_PLAY && lamp_r) {                  /* night or fog: this view's lamp */
        int v = vp_view[i], dy = line - lamp_y[v], l = 0, r = 0;
        if (dy * dy < lamp_r * lamp_r) { int h = isqrt(lamp_r * lamp_r - dy * dy); l = lamp_x[v] - h; r = lamp_x[v] + h; }
        rs_window(1, l, r);
    } else {
        rs_window(1, 0, RS_SCREEN_W);
    }
}

void mp_draw_play(void)
{
    rs_raster(mp_raster, NULL);
    rs_viewport rects[RS_VIEW_MAX];
    int nr;
    layout(rects, &nr);
    for (int d = 0; d < NDEPTH; d++) {
        draw_cells_dirty(d, d);
        ui_marks(d, 0, 32 * (d + 1));
    }
    ui_glow_pulse(W.t);
    for (int p = 0; p < W.nplayers; p++) hud_strip(p);
    if (W.mode == MODE_COOP) goal_bar();
    if (W.mode == MODE_BATTLE) clock_bar();
    int with_map = nr > nviews;
    if (with_map) live_map();
    rs_oam_clear();
    g_fog_eyes = 0;
    nvp = 0;
    int wsx = 0, wsy = 0;
    draw_weather_scroll(&wsx, &wsy);
    lamp_r = W.def->night ? W.def->night + (((W.t * 37u) % 211u) < 9 ? -6 : (int)((W.t / 6) % 3) - 1) : W.def->fog;
    if (W.def->night) night_set(1, 0, 0, lamp_r);
    else if (W.def->fog) fog_set(1, 0, 0, lamp_r);
    for (int i = 0; i < nviews; i++) {
        view *v = &V[i];
        const rs_viewport *r = &rects[i];
        /* the HUD strip(s): its player's, or both side by side when two moles share the view */
        int strips = merged ? 2 : 1;
        for (int s = 0; s < strips; s++) {
            int p = merged ? s : v->player, sw = r->w / strips;
            add_vp(VK_HUD, i, r->x + s * (sw + (s ? RS_VIEW_DIVIDER : 0)), r->y, sw - (s ? RS_VIEW_DIVIDER : 0), STRIP);
            VP[nvp - 1].sy[RS_BG1] = (int16_t)(p * STRIP);
            VP[nvp - 1].objs = 1;
            VP[nvp - 1].oam_first = (uint16_t)rs_oam_next();
            spr_draw_pal(SPR_MOLE_WALK_DOWN, 0, STRIP - bm_spr[SPR_MOLE_WALK_DOWN].h, 0, 3, W.mole_pal[p]);
            rs_spr(0, 0, MP_MARK_TILE + p * 2, 16, 8, W.mole_pal[p], 3, 0);
            VP[nvp - 1].oam_count = (uint16_t)(rs_oam_next() - VP[nvp - 1].oam_first);
        }
        /* the play area */
        int sy = view_scroll_y(v), cx = v->cx >> 8;
        add_vp(VK_PLAY, i, v->x, v->y, v->w, v->h);
        rs_viewport *p = &VP[nvp - 1];
        p->layers = (1 << RS_BG1) | (1 << RS_BG3) | (1 << RS_BG4) | (v->d == 0 && !v->slide ? 1 << RS_BG2 : 0);
        p->sx[RS_BG4] = p->sx[RS_BG3] = p->sx[RS_BG1] = (int16_t)cx;
        p->sy[RS_BG4] = p->sy[RS_BG3] = (int16_t)sy;
        p->sy[RS_BG1] = (int16_t)(sy + 256);
        p->sx[RS_BG2] = (int16_t)(wsx + cx);
        p->sy[RS_BG2] = (int16_t)(wsy + (v->cy >> 8));
        p->objs = 1;
        p->oam_first = (uint16_t)rs_oam_next();
        const actor *m = world_player(v->player < 0 ? 0 : v->player);
        if (m) {                                            /* its lamp (night, fog) and the eyes in the dark */
            int mx, my;
            mole_px(m, &mx, &my);
            lamp_x[i] = v->x + mx - cx;
            lamp_y[i] = v->y + (m->depth * 256 + my - 2) - sy;
            fog_focus(mx, my - 2 + HUD_H);
        }
        int ds[2] = {v->d, v->slide ? v->from : -1};
        for (int k = 0; k < 2; k++) {
            if (ds[k] < 0) continue;
            spr_camera(cx, sy - ds[k] * 256, 0, v->w, v->h);
            draw_world_sprites(ds[k], 0, 0);
        }
        p->oam_count = (uint16_t)(rs_oam_next() - p->oam_first);
    }
    spr_camera(0, 0, HUD_H, 0, 0);
    /* the live map in the 4th quadrant (3 views), the shared goal bar or the clock over the divider, a box */
    if (with_map) {
        add_vp(VK_HUD, 0, rects[nviews].x, rects[nviews].y, rects[nviews].w, rects[nviews].h);
        VP[nvp - 1].sy[RS_BG1] = 12 * 8 - 4;
    } else if (W.mode == MODE_COOP && nviews > 1) {
        int bx = (RS_SCREEN_W - 96) / 2, by = nviews == 2 && !MP.split_h ? RS_SCREEN_H - STRIP : (RS_SCREEN_H - STRIP) / 2;
        add_vp(VK_HUD, 0, bx, by, 96, STRIP);
        VP[nvp - 1].sy[RS_BG1] = 8 * 8;
    }
    if (W.mode == MODE_BATTLE) {
        int by = nviews == 1 || (nviews == 2 && !MP.split_h) ? RS_SCREEN_H - 8 : RS_SCREEN_H / 2 - 9;   /* over the divider */
        add_vp(VK_HUD, 0, (RS_SCREEN_W - 160) / 2, by, 160, 8);
        VP[nvp - 1].sy[RS_BG1] = 10 * 8;
    }
    if (box_on) {
        add_vp(VK_BOX, 0, (RS_SCREEN_W - box_w * 8) / 2, (RS_SCREEN_H - box_h * 8) / 2, box_w * 8, box_h * 8);
        VP[nvp - 1].sx[RS_BG1] = BOX_X * 8;
    }
    rs_viewports(nvp, VP, DIVIDER);
}

/* dump (tests) */
void mp_dump(void)
{
    char cams[96] = "";
    for (int i = 0; i < nviews && i < MAX_PLAYERS; i++) {
        char c[48];
        snprintf(c, sizeof c, " cam%d=%d,%d,%d", i, V[i].cx >> 8, V[i].cy >> 8, V[i].d);
        strcat(cams, c);
    }
    char mol[96] = "";
    for (int p = 0; p < W.nplayers; p++) {
        const actor *m = world_player(p);
        char c[40];
        snprintf(c, sizeof c, " p%d=%d,%d,%d,%d,%d", p + 1, m ? m->depth : -1, m ? m->cx : -1, m ? m->cy : -1,
                 W.ps[p].hearts, m ? (m->state == 99 ? 9 : m->stun ? 1 : 0) : -1);
        strcat(mol, c);
    }
    rs_log("mp: mode=%d players=%d views=%d merged=%d vp=%d up=%d respawns=%d ffstuns=%d drops=%d holebombs=%d "
           "sd=%d sdcrush=%d cpubombs=%d wins=%d,%d,%d,%d%s%s", W.mode, W.nplayers, nviews, merged, rs_viewport_count(),
           world_moles_up(), W.respawns, W.stat.ff_stuns, W.stat.drops, W.stat.hole_bombs, W.sd_on, W.stat.sd_crushed,
           W.stat.cpu_bombs, battle_wins[0], battle_wins[1], battle_wins[2], battle_wins[3], cams, mol);
}

/* ---- menus: the join screen ---- */
static const char *const SKILL[4] = {"", "EASY", "NORMAL", "HARD"};
static int join_port[MAX_PLAYERS], join_n, join_cpu, join_skill = 2, join_t;

static int colour_free(int c, int except)
{
    for (int p = 0; p < MAX_PLAYERS; p++)
        if (p != except && p < join_n + join_cpu && MP.colour[p] == c) return 0;
    return 1;
}

/* slot p's helmet: its own P-order colour when free, else the next free one */
static void pick_colour(int p)
{
    int c = p & 3;
    for (int k = 0; k < 4 && !colour_free(c, p); k++) c = (c + 1) % 4;
    MP.colour[p] = (uint8_t)c;
}

static void add_cpu(void)
{
    if (join_n + join_cpu >= MAX_PLAYERS) return;
    join_cpu++;
    pick_colour(join_n + join_cpu - 1);
}

/* first_port: the pad that opened the screen, P1 already in (-1: nobody yet) */
void mp_join_begin(int first_port)
{
    join_n = 0;
    join_cpu = 0;
    join_t = 0;
    for (int p = 0; p < MAX_PLAYERS; p++) { join_port[p] = -1; MP.colour[p] = (uint8_t)p; }
    if (first_port >= 0 && first_port < RS_PAD_MAX) { join_port[0] = first_port; join_n = 1; }
}

static int join_start(int mode)
{
    if (mode == MODE_BATTLE) while (join_n + join_cpu < 2) add_cpu();       /* alone: against a CPU mole */
    int total = join_n + join_cpu;
    MP.mode = mode == MODE_BATTLE ? MODE_BATTLE : (join_n > 1 ? MODE_COOP : MODE_SOLO);
    MP.nplayers = total;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        MP.port[p] = (int8_t)(p < join_n ? join_port[p] : -1);
        MP.cpu[p] = (uint8_t)(p >= join_n && p < total ? join_skill : 0);
    }
    prompt_port = MP.port[0];
    rs_log("join: start mode=%d humans=%d cpus=%d ports=%d,%d,%d,%d colours=%d,%d,%d,%d", MP.mode, join_n, join_cpu,
           MP.port[0], MP.port[1], MP.port[2], MP.port[3], MP.colour[0], MP.colour[1], MP.colour[2], MP.colour[3]);
    sfx(SFX_MENU_OK);
    return 1;
}

static const char *device_name(int port)
{
    static char s[16];
    int d = rs_pad_device(port);
    if (d == RS_DEVICE_KEYBOARD) return "KEYBOARD";
    if (d == RS_DEVICE_KEYBOARD2) return "KEYBOARD 2";
    snprintf(s, sizeof s, "PAD %d", port + 1);
    return s;
}

/* A or Start joins any pad not in yet (P1..P4 in the order they join; when nobody is in, Start also starts),
   Left/Right choose the helmet colour, B leaves; in battle P1 adds and removes CPU moles with X/Y and sets their
   skill with L/R; Start from a player begins (a lone battler gets a CPU). Returns 1 to start, -1 to go back. */
int mp_join_update(int mode)
{
    join_t++;
    text_clear_all();
    rs_text_setup(RS_BG1, 0, 0, 1);
    text_at((40 - 14) / 2, 3, mode == MODE_BATTLE ? "BATTLE - JOIN" : "STORY - JOIN ");
    for (int port = 0; port < RS_PAD_MAX; port++) {
        uint16_t pr = rs_pad_pressed(port);
        int slot = -1;
        for (int p = 0; p < join_n; p++) if (join_port[p] == port) slot = p;
        if (slot < 0 && (pr & (RS_BTN_A | RS_BTN_START)) && join_n < MAX_PLAYERS) {
            int first = join_n == 0;
            if (join_n + join_cpu >= MAX_PLAYERS) join_cpu--;           /* full of CPUs: a human takes one's place */
            for (int k = join_n + join_cpu; k > join_n; k--) MP.colour[k] = MP.colour[k - 1];
            join_port[join_n] = port;
            join_n++;
            MP.colour[join_n - 1] = 255;
            pick_colour(join_n - 1);
            sfx(SFX_MENU_OK);
            if (first && (pr & RS_BTN_START)) return join_start(mode);   /* nobody was in: Start joins and starts */
            continue;
        }
        if (slot < 0) continue;
        if (pr & (RS_BTN_LEFT | RS_BTN_RIGHT)) {            /* the next free colour */
            int c = MP.colour[slot];
            for (int k = 0; k < 4; k++) {
                c = (c + ((pr & RS_BTN_LEFT) ? 3 : 1)) % 4;
                if (colour_free(c, slot)) break;
            }
            MP.colour[slot] = (uint8_t)c;
            sfx(SFX_MENU_MOVE);
        }
        if ((pr & RS_BTN_B) && join_t > 10) {               /* leave (the first player backs out) */
            if (slot == 0 && join_n == 1) return -1;
            for (int p = slot; p < join_n - 1; p++) join_port[p] = join_port[p + 1];
            for (int p = slot; p < join_n + join_cpu - 1; p++) MP.colour[p] = MP.colour[p + 1];
            join_n--;
            continue;
            sfx(SFX_MENU_MOVE);
        }
        if (slot == 0 && mode == MODE_BATTLE) {
            if ((pr & RS_BTN_X) && join_n + join_cpu < MAX_PLAYERS) { add_cpu(); sfx(SFX_MENU_OK); }
            if ((pr & RS_BTN_Y) && join_cpu > 0) { join_cpu--; sfx(SFX_MENU_MOVE); }
            if (pr & RS_BTN_L) join_skill = join_skill > 1 ? join_skill - 1 : 3;
            if (pr & RS_BTN_R) join_skill = join_skill < 3 ? join_skill + 1 : 1;
        }
        if (pr & RS_BTN_START) return join_start(mode);              /* any player in starts */
    }
    /* the empty slots name the A button of the pads not in yet (in port order), else a pad's */
    int free_ports[RS_PAD_MAX], nfree = 0;
    for (int port = 0; port < RS_PAD_MAX; port++) {
        int in = 0;
        for (int p = 0; p < join_n; p++) in |= join_port[p] == port;
        if (!in && rs_pad_connected(port)) free_ports[nfree++] = port;
    }
    for (int p = 0; p < MAX_PLAYERS; p++) {
        int y = 7 + p * 4;
        text_box(4, y - 1, 32, 3);
        if (p < join_n) textf_at(6, y, "P%d  %-10s  < %-6s >", p + 1, device_name(join_port[p]), bm_helmet_names[MP.colour[p]]);
        else if (p < join_n + join_cpu) textf_at(6, y, "P%d  CPU %-6s    %-6s", p + 1, SKILL[join_skill], bm_helmet_names[MP.colour[p]]);
        else {
            int k = p - join_n - join_cpu;
            char s[32];
            snprintf(s, sizeof s, "PRESS %s TO JOIN", btn_name(RS_BTN_A, k < nfree ? free_ports[k] : RS_PAD_MAX - 1));
            textf_at(6, y, "P%d  %-26s", p + 1, (join_t / 30) % 2 ? s : "");
        }
    }
    rs_text_setup(RS_BG1, 0, 0, 1);
    int kp = join_n ? join_port[0] : prompt_port;                   /* the help speaks to P1 */
    char s[48];
    text_box(1, 21, 38, 6);                                         /* the help, readable over the hills */
    snprintf(s, sizeof s, "LEFT/RIGHT: HELMET   %s: LEAVE", btn_name(RS_BTN_B, kp));
    text_at((40 - (int)strlen(s)) / 2, 22, s);
    if (mode == MODE_BATTLE) {
        snprintf(s, sizeof s, "%s: ADD CPU  %s: REMOVE", btn_name(RS_BTN_X, kp), btn_name(RS_BTN_Y, kp));
        text_at((40 - (int)strlen(s)) / 2, 23, s);
        snprintf(s, sizeof s, "%s / %s: CPU SKILL", btn_name(RS_BTN_L, kp), btn_name(RS_BTN_R, kp));
        text_at((40 - (int)strlen(s)) / 2, 24, s);
        snprintf(s, sizeof s, "%s: CHOOSE THE ARENA", btn_name(RS_BTN_START, kp));
    } else {
        text_at((40 - 34) / 2, 23, "ALONE: THE STORY, 2-4 MOLES: CO-OP");
        snprintf(s, sizeof s, "%s: PLAY", btn_name(RS_BTN_START, kp));
    }
    text_at((40 - (int)strlen(s)) / 2, 25, s);
    rs_text_setup(RS_BG1, 0, 0, 1);
    return 0;
}

void mp_join_draw(void)
{
    rs_oam_clear();
    for (int p = 0; p < join_n + join_cpu && p < MAX_PLAYERS; p++) {
        /* a preview of each mole with its helmet: the palettes of slots 0-3 while in the menu */
        helmet_palette(p, MP.colour[p]);
        int y = 7 * 8 + p * 32 - 4;
        spr_draw_pal(SPR_MOLE_WALK_DOWN + (int[]){1, 0, 2, 0}[(join_t / 8) % 4], 300 - 26, y - 4, 0, 3, p);
        rs_spr(300 - 26, y - 14, MP_MARK_TILE + p * 2, 16, 8, p, 3, 0);
    }
}

void mp_join_enter(void) { marker_tiles(); }
void mp_join_dump(void)
{
    rs_log("join: humans=%d cpus=%d skill=%d colours=%d,%d,%d,%d ports=%d,%d,%d,%d", join_n, join_cpu, join_skill,
           MP.colour[0], MP.colour[1], MP.colour[2], MP.colour[3], join_port[0], join_port[1], join_port[2], join_port[3]);
}

/* ---- battle: the arena menu, rounds, sudden death, results ---- */
static const char *const ARENAS[6] = {"molehill-maze", "river-duel", "windmill-wars", "ice-rink", "mine-cart-mayhem",
                                      "pumpkin-fort"};
static const char *const ARENA_NAMES[6] = {"MOLEHILL MAZE", "RIVER DUEL", "WINDMILL WARS", "ICE RINK",
                                           "MINE CART MAYHEM", "PUMPKIN FORT"};
enum { B_ARENA, B_INTRO, B_PLAY, B_END, B_RESULTS, B_PAUSE };
static int b_state, b_t, b_arena, b_cursor, b_winner, b_quit;
static level_def AL;

const char *mp_arena_name(int i) { return ARENAS[clampi(i, 0, 5)]; }
int mp_arena_index(const char *name)
{
    for (int i = 0; i < 6; i++)
        if (!strcmp(name, ARENAS[i])) return i;
    return -1;
}

static void round_start(void)
{
    if (arena_load(&AL, ARENAS[b_arena])) { rs_log("battle: %s", AL.error); b_state = B_RESULTS; return; }
    MP.mode = MODE_BATTLE;
    MP.seed = (uint32_t)(round_no * 7919 + rs_frame_count());
    draw_init_vram(AL.season, 0);
    world_start(&AL, NULL);
    draw_variant_pals();
    ui_init_level();
    mp_level_start();
    round_left = MP.round_time;
    b_state = B_INTRO;
    b_t = 0;
    music_play(AL.music);
    if (rs_option_int("sd", 0)) round_left = rs_option_int("sd", 0);   /* tests: the sudden death after N frames */
}

void mp_battle_begin(void)
{
    memset(battle_wins, 0, sizeof battle_wins);
    round_no = 0;
    b_state = B_ARENA;
    b_t = 0;
    b_cursor = 0;
}

void mp_battle_quick(int arena)                            /* headless: straight into round 1 */
{
    memset(battle_wins, 0, sizeof battle_wins);
    b_arena = clampi(arena, 0, 5);
    round_no = 1;
    round_start();
}

static int any_pressed(void)
{
    uint16_t p = 0;
    for (int i = 0; i < MP.nplayers; i++)
        if (MP.port[i] >= 0) p |= rs_pad_pressed(MP.port[i]);
    if (!humans()) p |= rs_pad_pressed(0);
    return p;
}

/* returns 1 when the battle is over (back to the title) */
int mp_battle_update(void)
{
    b_t++;
    uint16_t pr = (uint16_t)any_pressed();
    switch (b_state) {
    case B_ARENA: {
        text_clear_all();
        rs_text_setup(RS_BG1, 0, 0, 1);
        text_at((40 - 13) / 2, 3, "CHOOSE ARENA");
        if (pr & RS_BTN_UP) { b_cursor = (b_cursor + 4) % 5; sfx(SFX_MENU_MOVE); }
        if (pr & RS_BTN_DOWN) { b_cursor = (b_cursor + 1) % 5; sfx(SFX_MENU_MOVE); }
        int lr = (pr & RS_BTN_LEFT) ? -1 : (pr & RS_BTN_RIGHT) ? 1 : 0;
        if (lr) {
            sfx(SFX_MENU_MOVE);
            if (b_cursor == 0) b_arena = (b_arena + 6 + lr) % 6;
            if (b_cursor == 1) MP.rounds = clampi(MP.rounds + lr, 1, 5);
            if (b_cursor == 2) MP.round_time = clampi(MP.round_time + lr * 1800, 1800, 5 * 3600);
            if (b_cursor == 3) MP.hole_bombs ^= 1;
        }
        textf_at(6, 8, "%c ARENA      < %-16s >", b_cursor == 0 ? '>' : ' ', ARENA_NAMES[b_arena]);
        textf_at(6, 10, "%c WINS       < %d >", b_cursor == 1 ? '>' : ' ', MP.rounds);
        textf_at(6, 12, "%c TIME       < %d:%02d >", b_cursor == 2 ? '>' : ' ', MP.round_time / 3600, MP.round_time / 60 % 60);
        textf_at(6, 14, "%c HOLE BOMBS < %s >", b_cursor == 3 ? '>' : ' ', MP.hole_bombs ? "ON " : "OFF");
        textf_at(6, 16, "%c FIGHT!", b_cursor == 4 ? '>' : ' ');
        text_at(4, 20, "BOMBS DROPPED INTO A HOLE FALL ON");
        text_at(4, 21, "THE DEPTH BELOW. LAST MOLE STANDING.");
        {
            char s[48];
            snprintf(s, sizeof s, "%s: FIGHT!   %s: BACK", btn_name(RS_BTN_START, prompt_port), btn_name(RS_BTN_B, prompt_port));
            text_at((40 - (int)strlen(s)) / 2, 24, s);
        }
        if ((pr & (RS_BTN_A | RS_BTN_START)) && (b_cursor == 4 || (pr & RS_BTN_START))) {
            sfx(SFX_MENU_OK);
            text_clear_all();
            round_no = 1;
            round_start();
        }
        if (pr & RS_BTN_B) { text_clear_all(); return 1; }
        return 0;
    }
    case B_INTRO:
        box(18, 5);
        { char s[20]; snprintf(s, sizeof s, "ROUND %d", round_no); box_line(1, s); }
        box_line(3, b_t > 50 ? "GO!" : "READY");
        rs_text_setup(RS_BG1, 0, 0, 1);
        if (b_t >= 70) { mp_box_off(); b_state = B_PLAY; b_t = 0; }
        mp_update_views();
        return 0;
    case B_PAUSE:
        box(34, 8);
        box_line(1, "PAUSED");
        { char s[24]; snprintf(s, sizeof s, "%cRESUME   %cQUIT", b_quit == 0 ? '>' : ' ', b_quit == 1 ? '>' : ' '); box_line(3, s); }
        {
            char s[48];
            snprintf(s, sizeof s, "%s: OK   %s: RESUME", btn_name(RS_BTN_A, prompt_port), btn_name(RS_BTN_B, prompt_port));
            box_line(5, s);
        }
        rs_text_setup(RS_BG1, 0, 0, 1);
        if (pr & (RS_BTN_LEFT | RS_BTN_RIGHT)) { b_quit ^= 1; sfx(SFX_MENU_MOVE); }
        if (pr & RS_BTN_B) { mp_box_off(); b_state = B_PLAY; }
        if (pr & (RS_BTN_A | RS_BTN_START)) {
            sfx(SFX_MENU_OK);
            mp_box_off();
            if (b_quit) { mp_views_off(); return 1; }
            b_state = B_PLAY;
        }
        return 0;
    case B_PLAY: {
        if (pr & RS_BTN_START) { b_state = B_PAUSE; b_quit = 0; sfx(SFX_MENU_OK); return 0; }   /* any player pauses */
        world_update();
        mp_update_views();
        if (round_left > 0 && --round_left == 0) world_sudden_death();
        int up = world_moles_up();
        if (up <= 1) {
            if (b_t < 100000) b_t = 100000;                 /* a short wait: a double knock-out is a draw */
            if (b_t >= 100000 + 60) {
                b_winner = -1;
                for (int p = 0; p < W.nplayers; p++) {
                    const actor *m = world_player(p);
                    if (m && m->alive && m->state != 99) b_winner = p;
                }
                if (b_winner >= 0) battle_wins[b_winner]++;
                b_state = B_END;
                b_t = 0;
                sfx(SFX_EXIT_OPEN);
            }
        }
        return 0;
    }
    case B_END: {
        box(24, 6);
        char s[40];
        if (b_winner >= 0) snprintf(s, sizeof s, "P%d WINS THE ROUND!", b_winner + 1);
        else snprintf(s, sizeof s, "A DRAW!");
        box_line(1, s);
        char t[48] = "";
        for (int p = 0; p < W.nplayers; p++) { char c[8]; snprintf(c, sizeof c, "P%d:%d ", p + 1, battle_wins[p]); strcat(t, c); }
        box_line(3, t);
        rs_text_setup(RS_BG1, 0, 0, 1);
        world_update();
        mp_update_views();
        if (b_t < 150) return 0;
        mp_box_off();
        int champ = -1;
        for (int p = 0; p < W.nplayers; p++) if (battle_wins[p] >= MP.rounds) champ = p;
        if (champ >= 0) { b_state = B_RESULTS; b_t = 0; mp_views_off(); text_clear_all(); return 0; }
        round_no++;
        round_start();
        return 0;
    }
    case B_RESULTS:
    default: {
        rs_oam_clear();
        text_clear_all();
        rs_text_setup(RS_BG1, 0, 0, 1);
        text_box(8, 6, 24, 14);
        text_at((40 - 7) / 2, 7, "RESULTS");
        int best = 0;
        for (int p = 0; p < W.nplayers; p++) if (battle_wins[p] > battle_wins[best]) best = p;
        for (int p = 0; p < W.nplayers; p++)
            textf_at(11, 10 + p * 2, "%c P%d %-6s  WINS %d", p == best ? '*' : ' ', p + 1,
                     MP.cpu[p] ? "CPU" : "PLAYER", battle_wins[p]);
        textf_at(12, 18, "P%d IS THE CHAMPION", best + 1);
        {
            char s[32];
            snprintf(s, sizeof s, "PRESS %s", btn_name(RS_BTN_A, prompt_port));
            text_at((40 - (int)strlen(s)) / 2, 21, s);
        }
        if (b_t > 60 && (pr & (RS_BTN_A | RS_BTN_START))) { text_clear_all(); return 1; }
        return 0;
    }
    }
}

int mp_battle_playing(void) { return b_state != B_ARENA && b_state != B_RESULTS; }

/* ---- co-op: the start box and the pause box ---- */
void mp_start_box(const level_def *L)
{
    box(34, 7);
    box_line(1, L->name);
    char goal[48];
    snprintf(goal, sizeof goal, "COLLECT %d GRUBS TOGETHER", W.grubs_total);
    box_line(3, goal);
    char k[32];
    snprintf(k, sizeof k, "PRESS %s", btn_name(RS_BTN_A, prompt_port));
    box_line(5, k);
    rs_text_setup(RS_BG1, 0, 0, 1);
}

void mp_pause_box(int cursor, int quit_ask)
{
    box(34, 9);
    box_line(1, W.def->name);
    box_line(2, "PAUSED");
    char s[48];
    snprintf(s, sizeof s, "%s: OK   %s: %s", btn_name(RS_BTN_A, prompt_port), btn_name(RS_BTN_B, prompt_port),
             quit_ask ? "NO" : "RESUME");
    box_line(7, s);
    if (quit_ask) snprintf(s, sizeof s, "QUIT TO TITLE?  %cYES  %cNO", quit_ask == 1 ? '>' : ' ', quit_ask == 2 ? '>' : ' ');
    else snprintf(s, sizeof s, "%cRESUME  %cRESTART  %cQUIT", cursor == 0 ? '>' : ' ', cursor == 1 ? '>' : ' ',
                  cursor == 2 ? '>' : ' ');
    box_line(5, s);
    rs_text_setup(RS_BG1, 0, 0, 1);
}

/* ---- headless: rounds of CPUs without the picture (the fairness test) ---- */
void mp_battle_sim(const char *arena, int rounds)
{
    level_def *L = &AL;
    if (arena_load(L, arena)) { rs_log("battlesim: %s", L->error); return; }
    int wins[MAX_PLAYERS] = {0}, draws = 0, sd = 0;
    long frames = 0;
    MP.mode = MODE_BATTLE;
    MP.nplayers = 4;
    for (int p = 0; p < 4; p++) { MP.port[p] = -1; MP.cpu[p] = (uint8_t)clampi(rs_option_int("skill", 2), 1, 3); }
    int time = rs_option_int("roundtime", 30) * 60;
    for (int r = 0; r < rounds; r++) {
        MP.seed = (uint32_t)r + 1;                           /* each round its own luck */
        world_start(L, NULL);
        int left = time, t = 0, end = -1;
        while (t < time + 60 * 60) {
            world_update();
            t++;
            if (left > 0 && --left == 0) { world_sudden_death(); sd++; }
            if (world_moles_up() <= 1) { if (end < 0) end = t; if (t - end >= 60) break; }
        }
        frames += t;
        int w = -1;
        for (int p = 0; p < 4; p++) {
            const actor *m = world_player(p);
            if (m && m->alive && m->state != 99) w = p;
        }
        if (w >= 0 && world_moles_up() == 1) wins[w]++;
        else draws++;
    }
    rs_log("battlesim: arena=%s rounds=%d wins=%d,%d,%d,%d draws=%d avgframes=%ld suddendeaths=%d", arena, rounds,
           wins[0], wins[1], wins[2], wins[3], draws, frames / (rounds ? rounds : 1), sd);
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("mp." #v, &(v), sizeof(v))
void mp_state(void)
{
    S(V); S(nviews); S(merged); S(box_on); S(box_w); S(box_h); S(VP); S(vp_kind); S(vp_view); S(nvp);
    S(battle_wins); S(round_no); S(round_left); S(lamp_x); S(lamp_y); S(lamp_r);
    S(join_port); S(join_n); S(join_cpu); S(join_skill); S(join_t);
    S(b_state); S(b_t); S(b_arena); S(b_cursor); S(b_winner); S(b_quit); S(AL);
    RS_STATE_RASTER(mp_raster);
}
#undef S
