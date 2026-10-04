/*
 * Nutty Branch: video. The house layout (docs/art-direction.md), with the branch as an arcade road:
 *   BG1 the UI (the house kit, priority 1);
 *   BG2 the branch: the AFFINE layer, its matrix set by the raster callback on every line (road.c gives each line a
 *       texture row, a scale and the branch centre: the Mode 7 perspective trick), the bark palette per line (the
 *       rings rushing at you, the haze in the distance);
 *   BG3 the canopy (top) and the far forest (the horizon), one layer, two parallax speeds by a line-scroll table;
 *   the backdrop a raster gradient (the sky above the horizon, the misty depths below);
 *   sprites: the squirrels (body + tail; OBJ 0, players 2-4 OBJ 1, 4, 5), the wood props (OBJ 2), the kit (OBJ 3),
 *   the colourful props (OBJ 6), the birds and the balance meter (OBJ 7); every world sprite at its perspective
 *   size (5 pre-drawn sizes), far to near.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>

/* the title logo: one 3-shade ramp per word (light, mid, dark): acorn gold, squirrel red */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 226, 140), RS_RGB8(236, 172, 70), RS_RGB8(186, 112, 40)},
    {RS_RGB8(250, 168, 100), RS_RGB8(222, 104, 52), RS_RGB8(164, 62, 34)}};

static road_view RV;                                  /* the projection of this frame (scratch: rebuilt each frame) */
int16_t draw_line_x[RS_SCREEN_H], draw_line_a[RS_SCREEN_H], draw_line_row[RS_SCREEN_H];
static int16_t bg2_dx[RS_SCREEN_H], bg3_dx[RS_SCREEN_H];
static rs_color sky[RS_SCREEN_H];
static uint8_t bark_sel[RS_SCREEN_H];
static rs_color bark_pal[HAZE_LEVELS * 4][16];
static int last_sel;                                  /* raster scratch */
static int shown_state = -1, shown_best = -1, band_now;
typedef struct part { int16_t x, y, t, kind; } part;
#define PARTS 16
static part parts[PARTS];

int draw_band(void) { return band_now; }

static void raster(int y, void *user)
{
    (void)user;
    rs_pal_set(0, sky[y]);
    if (draw_line_row[y] >= TEX_H) {
        rs_bg_enable(RS_BG2, 0);
        return;
    }
    rs_bg_enable(RS_BG2, 1);
    rs_affine m = {draw_line_a[y], 0, 0, 0, TEX_CX, draw_line_row[y], 0};
    rs_bg_affine(RS_BG2, &m);
    if (y == 0 || bark_sel[y] != last_sel) {
        last_sel = bark_sel[y];
        rs_pal_load(RS_PAL_BG(PAL_BARK), bark_pal[last_sel], 16);
    }
}

void draw_init(void)
{
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0 on palette 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    rs_bg_setup(RS_BG1, 64, 32, VR_BG1);
    rs_bg_setup(RS_BG2, 64, 32, VR_BG2);
    rs_bg_setup(RS_BG3, 32, 32, VR_BG3);
    rs_tiles_load(VR_BG2, gm_branch_tiles, gm_branch_tile_count);
    rs_tiles_load(VR_BG3, gm_scene_tiles, gm_scene_tile_count);
    memcpy(rs_bg_map(RS_BG2), gm_branch_map, sizeof(uint16_t) * 64 * 32);
    memcpy(rs_bg_map(RS_BG3), gm_scene_map, sizeof(uint16_t) * 32 * 32);
    rs_bg_enable(RS_BG1, 1);
    rs_bg_enable(RS_BG2, 1);
    rs_bg_enable(RS_BG3, 1);
    rs_bg_enable(RS_BG4, 0);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);              /* squirrels 1-2, the wood props */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 64);         /* squirrels 3-4, the colourful props, the birds */
    rs_bg_line_scroll(RS_BG2, bg2_dx, NULL);
    rs_bg_line_scroll(RS_BG3, bg3_dx, NULL);
    for (int y = 0; y < RS_SCREEN_H; y++) draw_line_row[y] = ROW_NONE;
    rs_raster(raster, NULL);
}

/* ---- colours: the time of day and season bands, blended ---------------------------------------------------------- */
static rs_color mix(rs_color a, rs_color b, int k) { return hu_lerp_color(a, b, k); }

static rs_color rgb_of(const uint8_t *p) { return RS_RGB8(p[0], p[1], p[2]); }

static void band_palettes(int32_t metres)
{
    int32_t m = metres < 0 ? 0 : metres;
    int b0 = (m / BAND_M) % BANDS, b1 = (b0 + 1) % BANDS;
    int into = m % BAND_M, k = 0;
    if (into > BAND_M - BAND_BLEND_M) k = (into - (BAND_M - BAND_BLEND_M)) * 256 / BAND_BLEND_M;
    band_now = k > 128 ? b1 : b0;
    rs_color bark[16], scene[16], wood[16], col[16], key[4];
    for (int i = 0; i < 16; i++) {
        bark[i] = mix(gm_band_bark[b0 * 16 + i], gm_band_bark[b1 * 16 + i], k);
        scene[i] = mix(gm_band_scene[b0 * 16 + i], gm_band_scene[b1 * 16 + i], k);
        wood[i] = mix(gm_band_wood[b0 * 16 + i], gm_band_wood[b1 * 16 + i], k);
        col[i] = mix(gm_band_col[b0 * 16 + i], gm_band_col[b1 * 16 + i], k);
    }
    for (int i = 0; i < 4; i++) key[i] = mix(rgb_of(gm_band_sky + b0 * 12 + i * 3), rgb_of(gm_band_sky + b1 * 12 + i * 3), k);
    rs_pal_load(RS_PAL_BG(PAL_SCENE), scene, 16);
    rs_pal_load(RS_PAL_OBJ(2), wood, 16);
    rs_pal_load(RS_PAL_OBJ(6), col, 16);
    /* the bark: 4 ring phases (OutRun's stripes, by palette) x 4 haze levels toward the horizon's colour */
    static const int ring_k[4] = {256, 256, 226, 240}, haze_k[HAZE_LEVELS] = {0, 64, 120, 176};
    for (int hz = 0; hz < HAZE_LEVELS; hz++)
        for (int r = 0; r < 4; r++) {
            rs_color *p = bark_pal[hz * 4 + r];
            for (int i = 0; i < 16; i++) {
                rs_color c = bark[i];
                if (i >= 2 && i <= 9 && hz < 3) c = hu_scale_color(c, ring_k[r]);
                p[i] = mix(c, key[1], haze_k[hz]);
            }
            p[0] = 0;
        }
    /* the sky above the horizon, the misty depths below; the house dither step every 8 lines */
    for (int y = 0; y < RS_SCREEN_H; y++) {
        rs_color c = y < HORIZON_Y ? mix(key[0], key[1], y * 256 / HORIZON_Y)
                                   : mix(key[2], key[3], (y - HORIZON_Y) * 256 / (RS_SCREEN_H - HORIZON_Y));
        if (y & 4) {
            int r = c & 31, g = (c >> 5) & 31, bl = (c >> 10) & 31;
            r = r < 31 ? r + 1 : r;
            bl = bl < 31 ? bl + 1 : bl;
            c = RS_RGB(r, g, bl);
        }
        sky[y] = c;
    }
}

/* ---- sprites --------------------------------------------------------------------------------------------------- */
static int spr_at(int id, int sx, int sy, int prio, int pal, int hflip)
{
    const gm_sprite_def *d = &gm_spr[id];
    int x = hflip ? sx - (d->w - 1 - d->ax) : sx - d->ax, y = sy - d->ay;
    if (x <= -(int)d->w || x >= RS_SCREEN_W || y <= -(int)d->h || y >= RS_SCREEN_H) return 0;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, hflip ? RS_SPR_HFLIP : 0);
    return 1;
}

/* a world sprite: mirrored ones are two halves (the left half drawn, the right one its flip) */
static void spr_world(int id, int sx, int sy, int pal, int hflip)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (!d->mirror) { spr_at(id, sx, sy, 2, pal, hflip); return; }
    int y = sy - d->ay;
    if (y <= -(int)d->h || y >= RS_SCREEN_H) return;
    if (sx - d->w < RS_SCREEN_W && sx > 0) rs_spr(sx - d->w, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, 2, 0);
    if (sx < RS_SCREEN_W && sx + d->w > 0) rs_spr(sx, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, 2, RS_SPR_HFLIP);
}

/* the pre-drawn size nearest a scale (Q8 px per unit): 1.33, 0.9, 0.6, 0.38, 0.22 */
static int lod_of(int scale)
{
    return scale > 280 ? 0 : scale > 188 ? 1 : scale > 122 ? 2 : scale > 74 ? 3 : 4;
}

typedef struct drawable { int32_t z; int16_t kind, idx; } drawable;
#define DRAW_MAX 96
enum { DK_OBJ, DK_SQ };

static int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }

static void draw_obj(const world *w, const obj *o)
{
    int sx, sy, sc, hid;
    int32_t ang = o->ang, h = 0;
    switch (o->kind) {
    case OB_ACORN: case OB_HAZEL: case OB_WALNUT: {
        if (o->taken || !road_point(w, &RV, o->s * 256, ang, 0, 0, &sx, &sy, &sc, &hid) || hid) return;
        int l = lod_of(sc);
        if (l > 3) return;
        int id = o->kind == OB_ACORN ? SPR_ACORN : o->kind == OB_HAZEL ? SPR_HAZEL : SPR_WALNUT;
        spr_world(id + l, sx, sy, -1, 0);
        if (o->kind == OB_WALNUT && l <= 1 && (w->t / 12) % 3 == 0) hu_sparkle(sx + 4, sy - 14, (w->t / 6) % 2, 2);
        return;
    }
    case OB_TWIG: case OB_KNOT: case OB_WEB: {
        if (!road_point(w, &RV, o->s * 256, ang, 0, 0, &sx, &sy, &sc, &hid) || hid) return;
        int id = o->kind == OB_TWIG ? SPR_TWIG : o->kind == OB_KNOT ? SPR_KNOT : SPR_WEB;
        spr_world(id + lod_of(sc), sx, sy, -1, o->kind == OB_TWIG && (o->s & 1));
        return;
    }
    case OB_SAP:
        for (int32_t s = o->s + 14; s < o->s + o->len; s += 28) {
            if (!road_point(w, &RV, s * 256, ang, 0, 0, &sx, &sy, &sc, &hid) || hid) continue;
            spr_world(SPR_SAP + lod_of(sc), sx, sy, -1, 0);
        }
        return;
    case OB_CATER:
        if (!cater_ang(w, o, &ang) || !road_point(w, &RV, o->s * 256, ang, 0, 0, &sx, &sy, &sc, &hid) || hid) return;
        spr_world(SPR_CATER + lod_of(sc) + (o->t0 >= 0 && (w->t / 8) % 2 ? 5 : 0), sx, sy, -1, o->side < 0);
        return;
    case OB_CONE: {
        if (!cone_h(w, o, &h)) return;
        if (!road_point(w, &RV, o->s * 256, ang, 0, 0, &sx, &sy, &sc, &hid) || hid) return;
        int l = lod_of(sc);
        if (h > 0) spr_world(SPR_SHADOW + l, sx, sy, -1, 0);
        road_point(w, &RV, o->s * 256, ang, h, 0, &sx, &sy, &sc, &hid);
        spr_world(SPR_CONE + l, sx, sy, -1, 0);
        return;
    }
    case OB_OWL:
        if (!owl_ang(w, o, &ang) || !road_point(w, &RV, o->s * 256, ang, OWL_H + Q8(6), 0, &sx, &sy, &sc, &hid)) return;
        if (lod_of(sc) <= 3) spr_world(SPR_OWL + lod_of(sc) + ((w->t / 6) % 2 ? 4 : 0), sx, sy, -1, 0);
        return;
    case OB_PECKER: {
        int open = hole_open(w, o);
        if (open > 0 && road_point(w, &RV, (o->s + HOLE_LEN / 2) * 256, ang, 0, 0, &sx, &sy, &sc, &hid) && !hid) {
            int l = lod_of(sc) + (open < 128 ? 2 : open < 256 ? 1 : 0);
            if (l <= 4) spr_world(SPR_HOLE + l, sx, sy, -1, 0);
        }
        if (road_point(w, &RV, o->s * 256, o->side * ANG(68), 0, 0, &sx, &sy, &sc, &hid) && !hid) {
            int l = lod_of(sc);
            if (l > 3) l = 3;
            int peck = o->t0 >= 0 && w->t - o->t0 < HOLE_OPEN + 72 && (w->t - o->t0) % 24 < 6;
            spr_world(SPR_PECKER + l + (peck ? 4 : 0), sx, sy, -1, o->side < 0);
        }
        return;
    }
    case OB_FORK:
        /* the dead branch's broken end: splinters across its tip */
        for (int i = -1; i <= 1; i++)
            if (road_point(w, &RV, (o->s + o->len) * 256, i * ANG(38), 0, 1, &sx, &sy, &sc, &hid) && !hid)
                spr_world(SPR_SPLINTER + lod_of(sc), sx, sy, -1, i > 0);
        return;
    default:
        return;
    }
}

static int body_frame(const squirrel *q, int t, int *hflip, int *lean_deg)
{
    int32_t a = q->ang;
    if (q->state == SQ_FALL || (q->state == SQ_DOWN)) {
        *hflip = q->fall_dir < 0;
        *lean_deg = 0;
        return SPR_SQ_FALL + (q->t / 8) % 2;
    }
    int deg = (int)(iabs32(a) >> 12);
    *hflip = a < 0;
    int b = (deg + 10) / 20;
    if (b > 3) b = 3;
    if (q->warn && (t / 3) % 2) b = b < 3 ? b + 1 : b;        /* the wobble: scrabbling at the edge */
    *lean_deg = (a < 0 ? -1 : 1) * b * 20;
    if (q->air && b == 0) {
        *hflip = 0;
        return SPR_SQ_JUMP + (q->vh < 0);
    }
    if (q->state == SQ_READY) return SPR_SQ_RUN + b * 2;
    int rate = q->speed > Q8(8) ? 3 : q->speed > Q8(6) ? 4 : 5;
    return SPR_SQ_RUN + b * 2 + (q->air ? 0 : (t / rate) % 2);
}

/* the squirrel: the tail (in front, from behind), the body, its shadow in the air, the balance meter */
static void draw_squirrel(const world *w, int p, int st_t)
{
    const squirrel *q = &w->q[p];
    if (q->state == SQ_OFF || (q->state == SQ_DOWN)) return;
    int sx, sy, sc, hid, hflip, lean;
    if (!road_point(w, &RV, q->s, q->ang, q->h, q->branch, &sx, &sy, &sc, &hid)) return;
    int pal = hu_player_pal(p);
    int frame = body_frame(q, w->t + p * 3, &hflip, &lean);
    if (q->state == SQ_HIT) sx += (q->t % 2) ? 1 : -1;
    if (q->warn && q->state == SQ_RUN) sx += ((w->t / 2) % 2) ? 1 : 0;
    if (q->state == SQ_READY) sy += hu_bob(st_t + p * 16, 64, 1);
    /* the meter (front), the tail, the body */
    if (q->state == SQ_RUN && (iabs32(q->ang) > METER_ANG || q->warn)) {
        spr_at(SPR_METER, sx, sy + 3, 2, -1, 0);
        int32_t f = (int32_t)((int64_t)q->ang * 15 / SLIP_ANG);
        if (f > 15) f = 15;
        if (f < -15) f = -15;
        int ny = sy + 3 + 1 + (int)(45 * f * f / 2250);
        spr_at(SPR_NEEDLE, sx + f, ny + 1, 2, -1, 0);
    }
    if (q->state != SQ_FALL) {
        int tail = lean - (int)(q->angv * TAIL_K / 4096);
        if (q->state == SQ_READY) tail = 6 * hu_bob(st_t + p * 16 + 8, 96, 2);
        if (tail > 90) tail = 90;
        if (tail < -90) tail = -90;
        int ti = ((tail < 0 ? -tail : tail) + 9) / 18;
        if (ti > 5) ti = 5;
        int la = lean;
        int rx = sx + (la == 0 ? 0 : la == 20 || la == -20 ? (la > 0 ? 2 : -2) : la == 40 || la == -40 ? (la > 0 ? 4 : -4)
                                                                                                         : (la > 0 ? 5 : -5));
        int ry = sy - (la == 0 || la == 20 || la == -20 ? 6 : la == 40 || la == -40 ? 5 : 3);
        spr_at(SPR_SQ_TAIL + ti, rx, ry, 2, pal, tail < 0);
    }
    spr_at(frame, sx, sy, 2, pal, hflip);
    if (q->air) {
        int bx, by, bs, bh;
        if (road_point(w, &RV, q->s, q->ang, 0, q->branch, &bx, &by, &bs, &bh)) spr_at(SPR_SHADOW, bx, by, 2, -1, 0);
    }
}

/* ---- particles: nut crumbs, landing puffs, the wind's leaves ------------------------------------------------------ */
static void parts_spawn(int x, int y, int kind)
{
    for (int i = 0; i < PARTS; i++)
        if (parts[i].t <= 0) {
            parts[i] = (part){(int16_t)x, (int16_t)y, 14, (int16_t)kind};
            return;
        }
}

static void parts_draw(void)
{
    for (int i = 0; i < PARTS; i++) {
        part *p = &parts[i];
        if (p->t <= 0) continue;
        p->t--;
        if (p->kind == 0) spr_at(SPR_CRUMB + (p->t < 7), p->x + (14 - p->t) * (i % 2 ? 1 : -1), p->y - (14 - p->t) / 2, 2, -1, 0);
        else spr_at(SPR_PUFF + (14 - p->t) / 5, p->x, p->y, 2, -1, 0);
    }
}

static void wind_leaves(const world *w)
{
    for (int i = 0; i < w->ob_count; i++) {
        const obj *o = ob_get(w, i);
        if (o->kind != OB_WIND || o->t0 < 0) continue;
        if ((w->lead_s >> 8) > o->s + o->len + 64) continue;
        int age = w->t - o->t0;
        for (int k = 0; k < 10; k++) {
            int x = (age * (5 + k % 3) + k * 41) % 400 - 40;
            if (o->side < 0) x = RS_SCREEN_W - x;
            int y = 30 + (k * 53) % 170 + ((age + k * 9) / 6) % 5;
            spr_at(SPR_LEAF + ((age / 5 + k) % 2), x, y, 2, -1, 0);
        }
    }
}

/* ---- the frame -------------------------------------------------------------------------------------------------- */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr_at(SPR_SQ_ICON, cx, cy + hu_bob(t + p * 16, 64, 1), 2, hu_player_pal(p), 0);
}

static void standing(const world *w, hu_standing *s)
{
    int key[MAX_PLAYERS], val[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) {
        key[p] = w->q[p].dist_m * 1000 + (w->q[p].score > 999 ? 999 : w->q[p].score);
        val[p] = w->q[p].dist_m;
    }
    hu_rank(s, w->players, key, val);
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    int screen = state == ST_DEAD ? ST_PLAY : state;
    if (screen != shown_state || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best) {
        hu_clear();
        if (state == ST_TITLE) hu_logo(GAME_LOGO, title_ramps, 2, 2, 0);
        if (screen == ST_PLAY && w->players >= 3) hu_score_tags(w->players, 0);
        if (state == ST_OVER) {
            if (w->players == 1) {
                char s[24];
                hu_banner(4, "GAME OVER");
                hu_gameover_panel(1, new_best, w->q[0].dist_m, 0);
                hu_box_text(12, 11, "METRES  ");                    /* the house panel, our labels */
                hu_box_text(12, 14, "NUTS    ");
                snprintf(s, sizeof s, new_best ? "NEW BEST!" : "BEST %dM", best);
                hu_box_text(12, 12, s);                              /* under METRES */
            } else {
                hu_results_panel(rs, NULL);
            }
        }
        shown_state = screen;
        shown_best = best;
    }
    if (state == ST_TITLE) hu_title_draw(st_t, best);
    if (screen == ST_PLAY) {
        /* the distance under the scores */
        char s[16];
        for (int pass = 0; pass < 2; pass++)
            for (int p = 0; p < w->players; p++) {
                hu_chip c = hu_score_chip(w->players, p, 0);
                snprintf(s, sizeof s, "%dM", (int)w->q[p].dist_m);
                int row = c.y < 120 ? c.y / 8 + 3 : c.y / 8 - 2;
                int col = c.align < 0 ? c.x / 8 : c.align > 0 ? (c.x + 7) / 8 - (int)strlen(s) : c.x / 8 - (int)strlen(s) / 2;
                if (col < 0) col = 0;
                if (pass == 0) hu_clear_rows(row, 1);       /* clear the rows first: two chips share a row */
                else hu_text(col, row, s);
            }
    }
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, "A: TITLE");
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, "A: TITLE");
    }
}

void draw_frame(const world *w, int state, int st_t, int best_m, int new_best, int paused)
{
    static const int medals[4] = MEDAL_METRES;
    (void)best_m;
    int sx = hu_shake_x(), sy = hu_shake_y();
    road_project(w, &RV);
    band_palettes((w->lead_s >> 8) / UNITS_PER_M);
    /* the per-line registers of the branch layer (the HDMA tables) and the backdrop's parallax */
    int canopy = (int)(w->drift >> 9), forest = (int)(w->drift >> 10);
    for (int y = 0; y < RS_SCREEN_H; y++) {
        const road_line *L = &RV.ln[y];
        draw_line_row[y] = L->row;
        draw_line_a[y] = L->a;
        draw_line_x[y] = (int16_t)(L->xc + sx);
        bg2_dx[y] = (int16_t)(TEX_CX - L->xc - sx);
        int ring = L->ring, hz = L->haze;
        bark_sel[y] = (uint8_t)(hz * 4 + ring);
        bg3_dx[y] = (int16_t)((y < 64 ? canopy : forest) & 255);
    }
    rs_bg_scroll(RS_BG2, 0, -sy);
    rs_bg_scroll(RS_BG3, 0, 0);
    hu_standing rs;
    standing(w, &rs);
    screen_text(w, state, st_t, best_m, new_best, &rs);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);

    rs_oam_clear();
    /* front to back: the UI, then the world near to far */
    if (state == ST_PLAY || state == ST_DEAD) {
        int score[MAX_PLAYERS];
        for (int p = 0; p < w->players; p++) score[p] = w->q[p].score;
        hu_score_chips(w->players, score, 0);
    }
    if (state == ST_OVER) {
        if (w->players == 1) {
            /* the distance, the nuts, the medal (hu_gameover_sprites' layout, with the nuts on the second line) */
            int medal = hu_medal_of(w->q[0].dist_m, medals);
            hu_number(w->q[0].dist_m, 25 * 8, 11 * 8 - 4 + slide, 3);
            hu_number(w->q[0].score, 25 * 8, 14 * 8 - 4 + slide, 3);
            if (medal) {
                hu_medal(medal, 22 * 8 - 4, 16 * 8 - 4 + slide, 3);
                if ((st_t / 20) % 3 == 0) hu_sparkle(22 * 8 + 12, 16 * 8 - 4 + slide, (st_t / 10) % 2, 3);
            } else {
                hu_box_text(22, 17, "-");
            }
        } else {
            hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr_at(SPR_SQ_ICON, cx, cy + slide, 3, hu_player_pal(rs.order[i]), 0);
            }
        }
    }
    if (state == ST_TITLE) {
        hu_title_sprites(st_t, slot_icon, NULL);
        /* the squirrel bobs under the logo, its tail swaying */
        int bob = hu_bob(st_t, 64, 4);
        int tail = 6 * hu_bob(st_t + 8, 96, 3);
        int ti = ((tail < 0 ? -tail : tail) + 9) / 18;
        spr_at(SPR_SQ_TAIL + (ti > 5 ? 5 : ti), 160, 118 + bob - 6, 2, 0, tail < 0);
        spr_at(SPR_SQ_RUN + (st_t / 32) % 2, 160, 118 + bob, 2, 0, 0);
    }
    /* particles from this step's events */
    for (int p = 0; p < w->players; p++) {
        const squirrel *q = &w->q[p];
        int ex, ey, es, eh;
        if (!(w->events[p] & (EV_NUT | EV_LAND))) continue;
        if (!road_point(w, &RV, q->s, q->ang, 0, q->branch, &ex, &ey, &es, &eh)) continue;
        if (w->events[p] & EV_NUT) parts_spawn(ex, ey - 6, 0);
        if (w->events[p] & EV_LAND) parts_spawn(ex, ey - 2, 1);
    }
    if (state != ST_TITLE || 1) parts_draw();
    if (state != ST_TITLE) wind_leaves(w);
    /* the world, sorted near to far (the lower OAM index is in front) */
    drawable list[DRAW_MAX];
    int n = 0;
    if (state != ST_TITLE)
        for (int p = 0; p < w->players && n < DRAW_MAX; p++)
            list[n++] = (drawable){w->q[p].s - w->cam_s - 1, DK_SQ, (int16_t)p};
    int32_t far = (DRAW_SEGS - 2) * SEG_LEN * 256;
    for (int i = 0; i < w->ob_count && n < DRAW_MAX; i++) {
        const obj *o = ob_get(w, i);
        int32_t z = o->s * 256 - w->cam_s;
        if (o->kind == OB_FORK) z += o->len * 256;
        if (o->kind == OB_SAP || o->kind == OB_GAP || o->kind == OB_WIND) z += o->len * 128;
        if (z < CAM_NEAR * 256 || z > far) continue;
        if (o->kind <= OB_WALNUT && (o->taken || z > 1400 * 256)) continue;
        if (o->kind == OB_GAP || o->kind == OB_WIND) continue;
        list[n++] = (drawable){z, DK_OBJ, (int16_t)i};
    }
    for (int i = 1; i < n; i++) {                       /* insertion sort, near first */
        drawable d = list[i];
        int j = i - 1;
        while (j >= 0 && list[j].z > d.z) { list[j + 1] = list[j]; j--; }
        list[j + 1] = d;
    }
    for (int i = 0; i < n; i++) {
        if (list[i].kind == DK_SQ) draw_squirrel(w, list[i].idx, st_t);
        else draw_obj(w, ob_get(w, list[i].idx));
    }
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(draw_line_x); S(draw_line_a); S(draw_line_row); S(bg2_dx); S(bg3_dx); S(sky); S(bark_sel); S(bark_pal);
    S(shown_state); S(shown_best); S(band_now); S(parts);
    RS_STATE_RASTER(raster);
}
#undef S
