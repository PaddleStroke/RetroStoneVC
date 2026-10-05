/*
 * Nutty Branch: video. The house layout (docs/art-direction.md), with the branch as an arcade road:
 *   BG1 the UI (the house kit, priority 1);
 *   BG2 the branch: the AFFINE layer, its matrix set by the raster callback on every line (road.c gives each line a
 *       texture row, a bark patch, a scale and the branch centre: the Mode 7 perspective trick), the bark palette per
 *       line (the rings rushing at you, the haze in the distance, wet bark after rain);
 *   BG3 the canopy (top) and the far forest (the horizon), one layer, two parallax speeds by a line-scroll table;
 *   the backdrop a raster gradient (the sky above the horizon, the misty depths below);
 *   sprites: the squirrels (body + tail; OBJ 0, players 2-4 OBJ 1, 4, 5), the wood props and scenery (OBJ 2), the kit
 *   (OBJ 3), the colourful props (OBJ 6), the birds and beasts and the balance meter (OBJ 7). Every world sprite is
 *   drawn at the nearest of its 8 pre-drawn sizes (LODs) and, round the branch, its nearest tilt, far to near.
 *   The world's sprite tiles are STREAMED: only the frames on screen are in VRAM, in a pool (the SNES way: DMA
 *   of the sprite tiles each frame); the squirrels and the small effects stay resident.
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
int16_t draw_line_x[RS_SCREEN_H], draw_line_a[RS_SCREEN_H], draw_line_row[RS_SCREEN_H], draw_line_tcx[RS_SCREEN_H];
static int16_t bg2_dx[RS_SCREEN_H];
static int bg_style;                                  /* the forest behind (BG_STYLE, --opt bg=a..d) */
static int16_t line_wl[RS_SCREEN_H], line_wr[RS_SCREEN_H];   /* window 1: the branch's span on the line */
static rs_color sky[RS_SCREEN_H];
static uint8_t bark_sel[RS_SCREEN_H];
static rs_color bark_pal[2 * HAZE_LEVELS * 4][16];    /* [wet][haze][ring] */
static int last_sel;                                  /* raster scratch */
static int shown_state = -1, shown_best = -1, band_now;
typedef struct part { int16_t x, y, t, kind, vx, vy; } part;
#define PARTS 20
static part parts[PARTS];
/* the sprite tile pool: where each streamed frame sits (-1: not in VRAM), when it was last drawn */
static int16_t pool_slot[SPR_COUNT];
static uint16_t pool_stamp[SPR_COUNT];
static uint8_t pool_used[POOL_TILES];
static uint16_t pool_now;

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
    rs_affine m = {draw_line_a[y], 0, 0, 0, draw_line_tcx[y], draw_line_row[y], 0};
    rs_bg_affine(RS_BG2, &m);
    rs_window(0, line_wl[y], line_wr[y]);             /* the other bark patches of the row stay hidden */
    if (y == 0 || bark_sel[y] != last_sel) {
        last_sel = bark_sel[y];
        rs_pal_load(RS_PAL_BG(PAL_BARK), bark_pal[last_sel], 16);
    }
}

static void pool_reset(void)
{
    for (int i = 0; i < SPR_COUNT; i++) pool_slot[i] = -1;
    memset(pool_stamp, 0, sizeof pool_stamp);
    memset(pool_used, 0, sizeof pool_used);
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
    const char *bo = rs_option("bg");
    bg_style = bo && bo[0] >= 'a' && bo[0] < 'a' + BG_STYLES ? bo[0] - 'a' : BG_STYLE;
    const gm_bg_style *bs = &gm_bgs[bg_style];
    rs_bg_setup(RS_BG4, 32, 32, VR_BG4);
    rs_tiles_load(VR_BG3, bs->near_tiles, bs->near_count);
    rs_tiles_load(VR_BG4, bs->far_tiles, bs->far_count);
    memcpy(rs_bg_map(RS_BG2), gm_branch_map, sizeof(uint16_t) * 64 * 32);
    memcpy(rs_bg_map(RS_BG3), bs->near_map, sizeof(uint16_t) * 32 * 32);
    memcpy(rs_bg_map(RS_BG4), bs->far_map, sizeof(uint16_t) * 32 * 32);
    rs_bg_enable(RS_BG1, 1);
    rs_bg_enable(RS_BG2, 1);
    rs_bg_enable(RS_BG3, 1);
    rs_bg_enable(RS_BG4, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, RES_TILES);           /* the resident sprites; the others are streamed */
    pool_reset();
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);              /* squirrels 1-2, the wood props */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 64);         /* squirrels 3-4, the colourful props, the beasts */
    rs_bg_line_scroll(RS_BG2, bg2_dx, NULL);
    rs_bg_window(RS_BG2, RS_WIN1_OUT);
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
    /* the bark: 4 ring phases (OutRun's stripes, by palette: subtle) x 4 haze levels toward the horizon's colour, dry
     * and wet (after rain: the lit top shines, the shadows deepen, a cool cast) */
    static const int ring_k[4] = {256, 256, 244, 250}, haze_k[HAZE_LEVELS] = {0, 64, 120, 176};
    for (int wet = 0; wet < 2; wet++)
        for (int hz = 0; hz < HAZE_LEVELS; hz++)
            for (int r = 0; r < 4; r++) {
                rs_color *p = bark_pal[(wet * HAZE_LEVELS + hz) * 4 + r];
                for (int i = 0; i < 16; i++) {
                    rs_color c = bark[i];
                    if (wet && i >= 2 && i <= 4) c = mix(c, bark[15], 110 - (i - 2) * 30);
                    if (wet && i >= 5 && i <= 8) c = mix(hu_scale_color(c, 200), RS_RGB8(40, 60, 90), 40);
                    if (i >= 2 && i <= 8 && hz < 3) c = hu_scale_color(c, ring_k[r]);
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

/* ---- the sprite pool ----------------------------------------------------------------------------------------------- */
static int pool_fit(int n)
{
    for (int i = 0, run = 0; i < POOL_TILES; i++) {
        run = pool_used[i] ? 0 : run + 1;
        if (run == n) return i - n + 1;
    }
    return -1;
}

static void pool_free(int f)
{
    memset(pool_used + pool_slot[f] - RES_TILES, 0, gm_spr[f].ntiles);
    pool_slot[f] = -1;
}

/* the VRAM tile (relative to VR_OBJ) of frame f: resident, or streamed into the pool now (-1: no room this frame) */
static int pool_get(int f)
{
    const gm_sprite_def *d = &gm_spr[f];
    if (d->resident) return d->tile;
    if (pool_slot[f] >= 0) {
        pool_stamp[f] = pool_now;
        return pool_slot[f];
    }
    int at = pool_fit(d->ntiles);
    if (at < 0) {                                      /* full: let go of what this frame has not drawn */
        for (int i = 0; i < SPR_COUNT; i++)
            if (pool_slot[i] >= 0 && pool_stamp[i] != pool_now) pool_free(i);
        at = pool_fit(d->ntiles);
        if (at < 0) {
            if (rs_option_int("pooldebug", 0)) rs_log("pool full: frame %d (%d tiles) not drawn", f, d->ntiles);
            return -1;
        }
    }
    memset(pool_used + at, 1, d->ntiles);
    pool_slot[f] = (int16_t)(RES_TILES + at);
    pool_stamp[f] = pool_now;
    rs_tiles_load(VR_OBJ + RES_TILES + at, gm_obj_tiles + (size_t)d->tile * RS_TILE_BYTES, d->ntiles);
    return pool_slot[f];
}

/* a frame, anchored at (sx, sy), mirrored about the anchor when hflip: its pieces */
static int spr_at(int f, int sx, int sy, int prio, int pal, int hflip)
{
    const gm_sprite_def *d = &gm_spr[f];
    int any = 0;
    for (int i = 0; i < d->npieces && !any; i++) {
        const gm_piece *p = &gm_pieces[d->piece + i];
        int x = hflip ? sx - p->dx - p->w : sx + p->dx, y = sy + p->dy;
        any = x > -(int)p->w && x < RS_SCREEN_W && y > -(int)p->h && y < RS_SCREEN_H;
    }
    if (!any) return 0;
    int base = pool_get(f);
    if (base < 0) return 0;
    for (int i = 0; i < d->npieces; i++) {
        const gm_piece *p = &gm_pieces[d->piece + i];
        int x = hflip ? sx - p->dx - p->w : sx + p->dx, y = sy + p->dy;
        if (x <= -(int)p->w || x >= RS_SCREEN_W || y <= -(int)p->h || y >= RS_SCREEN_H) continue;
        rs_spr(x, y, base + p->toff, p->w, p->h, pal >= 0 ? pal : d->pal, prio, hflip ? RS_SPR_HFLIP : 0);
    }
    return 1;
}

int draw_sprite_id(const rs_sprite *s, int *frame, int *ax, int *ay)
{
    for (int f = 0; f < SPR_COUNT; f++) {
        const gm_sprite_def *d = &gm_spr[f];
        int base = d->resident ? d->tile : pool_slot[f];
        if (base < 0 || s->tile < base || s->tile >= base + d->ntiles) continue;
        for (int i = 0; i < d->npieces; i++) {
            const gm_piece *p = &gm_pieces[d->piece + i];
            int t0 = base + p->toff, n = p->w / 8 * (p->h / 8);
            if (s->tile < t0 || s->tile >= t0 + n) continue;
            *frame = f;
            *ax = (s->flags & RS_SPR_HFLIP) ? s->x + p->dx + p->w : s->x - p->dx;
            *ay = s->y - p->dy;
            return 1;
        }
    }
    return 0;
}

/* the pre-drawn size nearest a scale (Q8 px per unit): 16 sizes from 1.6 down by 18% steps */
static int lod_of(int scale)
{
    int l = 0;
    while (l < LODS - 1 && scale < gm_lod_edge_q8[l]) l++;
    return l;
}

static int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }

/* a frame of a tilted prop: its tilt (0, 18, 36 degrees: the nearest to its angle round the branch) and size */
static int tilted(int base, int32_t ang, int lod) { int d = (int)(iabs32(ang) >> 12); return base + (d < 9 ? 0 : d < 27 ? 1 : 2) * LODS + lod; }

/* the sides of the branch sit lower than its top (road_point drops them); what grows out of the silhouette is put
 * back on the line of its place so that it meets the edge drawn by the texture */
static int32_t undrop(int32_t ang)
{
    return (int32_t)(((int64_t)BRANCH_R * 256 * (16384 - cos_q14(ang)) >> 14) * DROP_K_256 / 256);
}

/* what sits on the bark is drawn on the branch drawn on its line: near the horizon, where a curve turns the branch
 * sideways, one line can hold a long stretch of it but shows one centre; keep the anchor within that line's span */
static int lc_on = -1, lc_objects, lc_steps, lc_jumps, lc_back, lc_far, lc_anchors, lc_off, lc_kept;
static int on_branch(int *sx, int sy)
{
    if (sy < 0 || sy >= RS_SCREEN_H) return 1;
    const road_line *L = &RV.ln[sy];
    if (L->row >= TEX_H || L->tcx == FACE_CX || L->a <= 0) return 0;        /* no bark on its line: not drawn */
    if (L->row >= ROW_FORK) return 1;
    int hw = BRANCH_R * 256 * 17 / 16 / L->a;
    int x = *sx < L->xc - hw ? L->xc - hw : *sx > L->xc + hw ? L->xc + hw : *sx;
    if (x != *sx) lc_kept++;                          /* (tests: counted) */
    *sx = x;
    return 1;
}

static int decor_lod(int lod) { return lod < 1 ? 0 : lod - 1; }   /* the scenery starts at the 2nd size */
static int limb_lod(int lod) { return lod < 2 ? 0 : lod - 2; }    /* the outward branches at the 3rd */

/* ---- tests (--opt lodcheck=1): every object's drawn size must never shrink as it comes (only its nearest pre-drawn
 * size, within 10% of the projected one, or the largest one), and its anchor must lie on the drawn branch ---------- */
#define LODCHECK_N 256
static struct { int32_t s, ang; int16_t kind, len, lod; uint16_t seen; } lc_seen[LODCHECK_N];   /* scratch */
static int lc_worst = 100;                                                /* the worst drawn/true size, % */

/* lod: the size drawn (minl: the object's largest pre-drawn size); anchor: 0 in the air (not checked), 1 on the bark,
 * 2 out of the flank (both: on the branch's span on its line) */
static void lod_note(const obj *o, int lod, int minl, int sc, int sx, int sy, int anchor)
{
    if (lc_on < 0) lc_on = rs_option_int("lodcheck", 0);
    if (!lc_on) return;
    int i = (int)((uint32_t)(o->s * 31 + o->kind * 7 + o->len * 3 + (o->ang >> 12)) % LODCHECK_N);
    /* (an object hidden for a while, behind a crest, comes back at its new size) */
    if (lc_seen[i].s != o->s || lc_seen[i].kind != o->kind || lc_seen[i].len != o->len || lc_seen[i].ang != o->ang ||
        (uint16_t)(pool_now - lc_seen[i].seen) > 1) {
        lc_seen[i].s = o->s;
        lc_seen[i].ang = o->ang;
        lc_seen[i].kind = o->kind;
        lc_seen[i].len = (int16_t)o->len;
        lc_seen[i].lod = (int16_t)lod;
        lc_objects++;
    } else if (lod != lc_seen[i].lod) {
        lc_steps++;
        if (lod > lc_seen[i].lod && lc_back++ < 8)    /* a smaller size than before: it would shrink */
            rs_log("lodcheck: kind %d at s %d shrank from size %d to %d (scale %d)", o->kind, o->s, lc_seen[i].lod, lod, sc);
        if (lod < lc_seen[i].lod - 1 && lc_jumps++ < 8)  /* more than one size (18%) at once */
            rs_log("lodcheck: kind %d at s %d grew from size %d to %d (scale %d)", o->kind, o->s, lc_seen[i].lod, lod, sc);
        lc_seen[i].lod = (int16_t)lod;
    }
    lc_seen[i].seen = pool_now;
    int drawn = gm_lod_q8[lod], ratio = drawn * 100 / (sc > 0 ? sc : 1);
    if ((ratio < 90 && lod > minl) || (ratio > 110 && lod < LODS - 1)) {     /* not the nearest size */
        if (lc_far++ < 8) rs_log("lodcheck: kind %d drawn at size %d for a scale %d (%d%%)", o->kind, lod, sc, ratio);
    }
    if (ratio < lc_worst && lod > minl) lc_worst = ratio;
    if (!anchor) return;
    if (sy >= 0 && sy < RS_SCREEN_H && RV.ln[sy].row < TEX_H && RV.ln[sy].tcx != FACE_CX && RV.ln[sy].a > 0) {
        lc_anchors++;
        int xc = RV.ln[sy].xc, hw = BRANCH_R * 256 / RV.ln[sy].a;
        int d = sx - xc < 0 ? xc - sx : sx - xc;
        if (RV.ln[sy].row < ROW_FORK && d > hw + hw / 8 + 3) {
            lc_off++;
            rs_log("lodcheck: kind %d at s %d anchored at (%d,%d), %d px from the centre, the branch %d wide", o->kind,
                   o->s, sx, sy, d, hw);
        }
    }                                                 /* (on a line with no bark it is not drawn: on_branch) */
}

/* tests: whatever hangs (the swinging branch) must hang from something drawn: its vine reaching the top edge */
static int lc_hang, lc_unhung;
static void hang_note(int sy, int hung)
{
    if (lc_on <= 0 || sy >= RS_SCREEN_H) return;
    lc_hang++;
    if (!hung && lc_unhung++ < 8) rs_log("lodcheck: a hanging thing at y %d whose vine is not drawn", sy);
}

void draw_lodcheck_report(void)
{
    if (lc_on > 0)
        rs_log("lodcheck: %d objects, %d size steps, %d backwards, %d of more than one size, %d not the nearest size, "
               "the smallest drawn/true %d%%; %d anchors, %d off the branch (%d kept on their line by on_branch); %d hanging, "
               "%d not hung", lc_objects, lc_steps, lc_back, lc_jumps, lc_far, lc_worst, lc_anchors, lc_off, lc_kept,
               lc_hang, lc_unhung);
}

typedef struct drawable { int32_t z; int16_t kind, idx; } drawable;
#define DRAW_MAX 200
enum { DK_OBJ, DK_SQ, DK_DROP };

static void draw_obj(const world *w, const obj *o)
{
    int sx, sy, sc, hid, l;
    int32_t ang = o->ang, h = 0;
    switch (o->kind) {
    case OB_ACORN: case OB_HAZEL: case OB_WALNUT: {
        if (o->taken || !road_point(w, &RV, o->s * 256, ang, 0, 0, &sx, &sy, &sc, &hid) || hid) return;
        if ((l = lod_of(sc)) > 5) return;
        if (!on_branch(&sx, sy)) return;
        lod_note(o, l, 0, sc, sx, sy, 1);
        int id = o->kind == OB_ACORN ? SPR_ACORN : o->kind == OB_HAZEL ? SPR_HAZEL : SPR_WALNUT;
        spr_at(id + l, sx, sy, 2, -1, 0);
        if (o->kind == OB_WALNUT && l <= 2 && (w->t / 12) % 3 == 0) hu_sparkle(sx + 4, sy - 14, (w->t / 6) % 2, 2);
        return;
    }
    case OB_TWIG:                                    /* a branch out of one flank (its frames: per kind) */
        if (!road_point(w, &RV, o->s * 256, 0, 0, 0, &sx, &sy, &sc, &hid) || hid) return;
        if (!on_branch(&sx, sy)) return;
        lod_note(o, l = decor_lod(lod_of(sc)) + 1, 1, sc, sx, sy, 1);
        spr_at(SPR_LIMB + (int)o->len * (LODS - 1) + l - 1, sx, sy, 2, -1, o->side < 0);
        return;
    case OB_KNOTHOLE: {
        if (!road_point(w, &RV, o->s * 256 + KNOTHOLE_LEN * 128, ang, 0, 0, &sx, &sy, &sc, &hid) || hid) return;
        int peek = ((w->t + o->s) / 50) % 4 == 0;     /* now and then a mouse peeks out */
        if (!on_branch(&sx, sy)) return;
        lod_note(o, l = lod_of(sc), 0, sc, sx, sy, 1);
        spr_at(tilted(SPR_HOLE, ang, l) + (peek ? TILTS * LODS : 0), sx, sy, 2, -1, ang < 0);
        return;
    }
    case OB_KNOT: case OB_WEB: case OB_SNOW: {
        if (!road_point(w, &RV, o->s * 256, ang, 0, 0, &sx, &sy, &sc, &hid) || hid) return;
        if (!on_branch(&sx, sy)) return;
        lod_note(o, lod_of(sc), 0, sc, sx, sy, 1);
        int id = o->kind == OB_KNOT ? SPR_KNOT : o->kind == OB_WEB ? SPR_WEB : SPR_SNOW;
        spr_at(tilted(id, ang, lod_of(sc)), sx, sy, 2, -1, ang < 0);
        return;
    }
    case OB_SAP:
        for (int32_t s = o->s + 14; s < o->s + o->len; s += 28) {
            if (!road_point(w, &RV, s * 256, ang, 0, 0, &sx, &sy, &sc, &hid) || hid) continue;
            if (!on_branch(&sx, sy)) continue;
            spr_at(tilted(SPR_SAP, ang, lod_of(sc)), sx, sy, 2, -1, ang < 0);
        }
        return;
    case OB_CATER: case OB_SNAKE: {
        int on = o->kind == OB_CATER ? cater_ang(w, o, &ang) : snake_ang(w, o, &ang);
        if (!on || !road_point(w, &RV, o->s * 256, ang, 0, 0, &sx, &sy, &sc, &hid) || hid) return;
        if (!on_branch(&sx, sy)) return;
        lod_note(o, lod_of(sc), 0, sc, sx, sy, 1);
        int base = o->kind == OB_CATER ? SPR_CATER : SPR_SNAKE;
        int fr = o->t0 >= 0 && (w->t / (o->kind == OB_CATER ? 8 : 5)) % 2 ? TILTS * LODS : 0;
        /* it faces the way it goes; mirrored, the tilt mirrors too: the angle's sign decides the flip */
        spr_at(tilted(base, ang, lod_of(sc)) + fr, sx, sy, 2, -1, o->side < 0);
        return;
    }
    case OB_CONE: {
        if (!cone_h(w, o, &h)) return;
        if (!road_point(w, &RV, o->s * 256, ang, 0, 0, &sx, &sy, &sc, &hid) || hid) return;
        if (!on_branch(&sx, sy)) return;
        lod_note(o, l = lod_of(sc), 0, sc, sx, sy, 1);
        if (h > 0) spr_at(SPR_SHADOW + l, sx, sy, 2, -1, 0);
        road_point(w, &RV, o->s * 256, ang, h, 0, &sx, &sy, &sc, &hid);
        spr_at(tilted(SPR_CONE, ang, l), sx, sy, 2, -1, ang < 0);
        return;
    }
    case OB_OWL: {
        if (!owl_ang(w, o, &ang)) return;
        int bx, by, bs, bh;
        if (road_point(w, &RV, o->s * 256, ang, OWL_H + Q8(6), 0, &sx, &sy, &sc, &hid)) {
            lod_note(o, lod_of(sc), 0, sc, sx, sy, 0);
            spr_at(SPR_OWL + lod_of(sc) + ((w->t / 6) % 2 ? LODS : 0), sx, sy, 2, -1, 0);
        }
        if (road_point(w, &RV, o->s * 256, ang, 0, 0, &bx, &by, &bs, &bh) && !bh)
            spr_at(SPR_BIGSHADOW + lod_of(bs), bx, by, 2, -1, 0);     /* its shadow sweeps over the bark */
        return;
    }
    case OB_PECKER: {
        int open = hole_open(w, o);
        if (open > 0 && road_point(w, &RV, (o->s + HOLE_LEN / 2) * 256, ang, 0, 0, &sx, &sy, &sc, &hid) && !hid) {
            l = lod_of(sc) + (open < 128 ? 2 : open < 256 ? 1 : 0);
            if (l < LODS) spr_at(tilted(SPR_HOLE, ang, l), sx, sy, 2, -1, ang < 0);
        }
        if (road_point(w, &RV, o->s * 256, o->side * ANG(68), undrop(ANG(68)), 0, &sx, &sy, &sc, &hid) && !hid) {
            if (on_branch(&sx, sy)) {
                lod_note(o, lod_of(sc), 0, sc, sx, sy, 2);
                int peck = o->t0 >= 0 && w->t - o->t0 < HOLE_OPEN + 72 && (w->t - o->t0) % 24 < 6;
                spr_at(SPR_PECKER + lod_of(sc) + (peck ? LODS : 0), sx, sy, 2, -1, o->side < 0);
            }
        }
        return;
    }
    case OB_FORK:                                     /* the dead branch's broken end */
        if (road_point(w, &RV, (o->s + o->len) * 256, 0, 0, 1, &sx, &sy, &sc, &hid) && !hid) {
            lod_note(o, lod_of(sc), 0, sc, sx, sy, 0);
            spr_at(SPR_BROKEN + lod_of(sc), sx, sy, 2, -1, 0);
        }
        return;
    case OB_GAP:                                      /* the torn ends and the broken face: the branch layer's */
        return;
    case OB_SWING: {
        if (!road_free(w, &RV, o->s * 256, 0, SWING_PIVOT, &sx, &sy, &sc)) return;
        int deg = (int)(swing_deg(w, o) / 4096), ai = (deg + SWING_AMP + 7) / 15;
        if (ai < 0) ai = 0;
        if (ai > 6) ai = 6;
        lod_note(o, decor_lod(lod_of(sc)) + 1, 1, sc, sx, sy, 0);
        spr_at(SPR_SWING + ai * (LODS - 1) + decor_lod(lod_of(sc)), sx, sy, 2, -1, 0);
        /* it hangs from the canopy, out of sight above: its vine from the pivot up past the top of the screen, as
         * thick as the swinging stem at that size */
        int vl = lod_of(sc), vf = SPR_VINE + (vl <= 3 ? 0 : vl <= 8 ? 2 : 4), hung = 1;
        for (int y = sy - 16; y > -16; y -= 16) hung &= spr_at(vf + ((y >> 4) & 1), sx, y, 2, -1, 0);
        hang_note(sy, hung);
        return;
    }
    case OB_BEES: {
        if (!bees_ang(w, o, &ang) || !road_point(w, &RV, o->s * 256, ang, Q8(14), 0, &sx, &sy, &sc, &hid)) return;
        int r = sc * 14 >> 8;                         /* the swarm: 8 bees buzzing round its centre */
        if (r < 2) r = 2;
        for (int i = 0; i < 8; i++) {
            uint32_t hsh = (uint32_t)(i * 2654435761u) ^ (uint32_t)((w->t / 3 + i * 7) * 40503u);
            int dx = (int)(hsh % (2 * r + 1)) - r, dy = (int)((hsh >> 12) % (r + 1)) - r / 2;
            spr_at(SPR_BEE + (w->t / 2 + i) % 2, sx + dx, sy + dy, 2, -1, 0);
        }
        return;
    }
    case OB_CROW: {
        int32_t cs;
        if (!crow_at(w, o, &cs, &h) || !road_point(w, &RV, cs * 256, ang, h, 0, &sx, &sy, &sc, &hid) || hid) return;
        int state = o->t1 >= 0 ? 2 : o->t0 >= 0 && (w->t / 10) % 2 ? 1 : 0;
        spr_at(SPR_CROW + state * LODS + lod_of(sc), sx, sy, 2, -1, 0);
        return;
    }
    case OB_DECOR: {
        int kind = (int)o->len;
        if (kind <= DC_SIDE2 || kind == DC_MUSH || kind == DC_HIVE) {
            if (!road_point(w, &RV, o->s * 256, ang, undrop(ang), 0, &sx, &sy, &sc, &hid) || hid) return;
            if (kind == DC_MUSH) {
                if (!on_branch(&sx, sy)) return;
                lod_note(o, decor_lod(lod_of(sc)) + 1, 1, sc, sx, sy, 2);
                spr_at(SPR_MUSH + decor_lod(lod_of(sc)), sx, sy, 2, -1, o->side < 0);
            } else {
                if (sy < 0 || sy >= RS_SCREEN_H || RV.ln[sy].row >= TEX_H || RV.ln[sy].tcx == FACE_CX) return;   /* only out of the branch drawn */
                l = limb_lod(lod_of(sc));
                if (!on_branch(&sx, sy)) return;
                lod_note(o, l + 2, 2, sc, sx, sy, 2);
                /* behind the branch layer: its root grows out from behind the silhouette */
                spr_at((kind == DC_HIVE ? SPR_HIVE : SPR_SIDE + (kind - DC_SIDE0) * (LODS - 2)) + l, sx, sy, 1, -1,
                       o->side < 0);
            }
        }
        return;
    }
    default:
        return;
    }
}

/* a shower's acorn: falling (its shadow growing under it), then lying on the bark */
static void draw_drop(const world *w, const drop *d)
{
    int sx, sy, sc, hid;
    if (!road_point(w, &RV, d->s * 256, d->ang, 0, 0, &sx, &sy, &sc, &hid) || hid) return;
    int l = lod_of(sc);
    if (d->t < DROP_FALL) {
        int r = DROP_FALL - d->t;
        int32_t h = (int32_t)((int64_t)Q8(130) * r * r / (DROP_FALL * DROP_FALL));
        spr_at(SPR_SHADOW + l, sx, sy, 2, -1, 0);
        road_point(w, &RV, d->s * 256, d->ang, h, 0, &sx, &sy, &sc, &hid);
    }
    if (l <= 5) spr_at(SPR_ACORN + l, sx, sy, 2, -1, 0);
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

/* the squirrel: the tail (in front, from behind), the body, its shadow on the bark, the balance meter */
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
    if (q->state == SQ_RUN || q->state == SQ_READY) {  /* the contact shadow (bigger and lighter in the air) */
        int bx, by, bs, bh;
        if (road_point(w, &RV, q->s, q->ang, 0, q->branch, &bx, &by, &bs, &bh)) { int l = lod_of(bs) + (q->air ? 1 : 0); spr_at(SPR_SHADOW + (l > LODS - 1 ? LODS - 1 : l), bx, by + 1, 2, -1, 0); }
    }
}

/* ---- particles: nut crumbs, landing puffs, the wind's leaves, big leaves sweeping past, rain ------------------- */
enum { PK_CRUMB, PK_PUFF, PK_FGLEAF };

static void parts_spawn(int x, int y, int kind, int vx, int vy, int t)
{
    for (int i = 0; i < PARTS; i++)
        if (parts[i].t <= 0) {
            parts[i] = (part){(int16_t)x, (int16_t)y, (int16_t)t, (int16_t)kind, (int16_t)vx, (int16_t)vy};
            return;
        }
}

static void parts_draw(void)
{
    for (int i = 0; i < PARTS; i++) {
        part *p = &parts[i];
        if (p->t <= 0) continue;
        p->t--;
        if (p->kind == PK_CRUMB) spr_at(SPR_CRUMB + (p->t < 7), p->x + (14 - p->t) * (i % 2 ? 1 : -1), p->y - (14 - p->t) / 2, 2, -1, 0);
        else if (p->kind == PK_PUFF) spr_at(SPR_PUFF + (14 - p->t) / 5, p->x, p->y, 2, -1, 0);
        else {
            p->x += p->vx;
            p->y += p->vy;
            spr_at(SPR_FGLEAF + (p->t / 6) % 3, p->x, p->y, 0, -1, p->vx < 0);
        }
    }
}

static void weather(const world *w)
{
    int32_t lead = w->lead_s >> 8;
    for (int i = 0; i < w->ob_count; i++) {
        const obj *o = ob_get(w, i);
        if (o->s > lead + 2600) break;
        if (o->kind == OB_WIND && o->t0 >= 0 && lead <= o->s + o->len + 64) {
            int age = w->t - o->t0;
            for (int k = 0; k < 10; k++) {
                int x = (age * (5 + k % 3) + k * 41) % 400 - 40;
                if (o->side < 0) x = RS_SCREEN_W - x;
                int y = 30 + (k * 53) % 170 + ((age + k * 9) / 6) % 5;
                spr_at(SPR_LEAF + ((age / 5 + k) % 2), x, y, 2, -1, 0);
            }
        }
        if (o->kind == OB_RAIN && o->t0 >= 0 && lead <= o->s + o->len) {
            int age = w->t - o->t0;
            for (int k = 0; k < 18; k++) {
                int x = (k * 67 + age * 3) % 336 - 8, y = (k * 41 + age * 9) % 256 - 16;
                spr_at(SPR_RAIN + (k + age / 4) % 2, x, y, 2, -1, 0);
            }
        }
    }
}
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
    int sx = hu_shake_x(), sy = hu_shake_y();
    pool_now++;
    road_project(w, &RV);
    band_palettes((w->lead_s >> 8) / UNITS_PER_M);
    /* the per-line registers of the branch layer (the HDMA tables) and the backdrop's parallax */
    int canopy = (int)(w->drift >> 9), forest = (int)(w->drift >> 10);
    for (int y = 0; y < RS_SCREEN_H; y++) {
        const road_line *L = &RV.ln[y];
        draw_line_row[y] = L->row;
        draw_line_a[y] = L->a;
        draw_line_x[y] = (int16_t)(L->xc + sx);
        draw_line_tcx[y] = L->tcx;
        if (L->row < TEX_H && L->a > 0) {
            /* window 1: only the line's own part of the texture row (a bark patch, a fork's pair, a broken end, the
             * face beside the fork rows); the span in 1/16 texels */
            int span = L->tcx == FACE_CX ? BRANCH_R * 16
                     : L->row < ROW_FORK ? BRANCH_R * 17
                     : L->row < ROW_GRAIN ? (FORK_STEP * ((L->row - ROW_FORK) / 8 + 1) / 2 + BRANCH_R + 2) * 16
                     : (BRANCH_R + 2) * 16;
            int hw = span * 16 / L->a + 2;
            line_wl[y] = (int16_t)(L->xc + sx - hw);
            line_wr[y] = (int16_t)(L->xc + sx + hw);
        } else {
            line_wl[y] = 0;
            line_wr[y] = RS_SCREEN_W;
        }
        bg2_dx[y] = (int16_t)(L->tcx - L->xc - sx);
        bark_sel[y] = (uint8_t)((L->wet * HAZE_LEVELS + L->haze) * 4 + L->ring);
    }
    rs_bg_scroll(RS_BG2, 0, -sy);
    rs_bg_scroll(RS_BG3, canopy & 255, 0);              /* the near forest at half the curves' drift ... */
    rs_bg_scroll(RS_BG4, forest & 255, 0);              /* ... the far one at a quarter */
    hu_standing rs;
    standing(w, &rs);
    screen_text(w, state, st_t, best_m, new_best, &rs);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);

    rs_oam_clear();
    /* front to back: the UI, the big leaves sweeping past, then the world near to far */
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
        if (!(w->events[p] & (EV_NUT | EV_LAND | EV_BONK))) continue;
        if (!road_point(w, &RV, q->s, q->ang, 0, q->branch, &ex, &ey, &es, &eh)) continue;
        if (w->events[p] & (EV_NUT | EV_BONK)) parts_spawn(ex, ey - 6, PK_CRUMB, 0, 0, 14);
        if (w->events[p] & EV_LAND) parts_spawn(ex, ey - 2, PK_PUFF, 0, 0, 14);
    }
    parts_draw();
    if (state != ST_TITLE) weather(w);
    /* the world, sorted near to far (the lower OAM index is in front) */
    drawable list[DRAW_MAX];
    int n = 0;
    if (state != ST_TITLE)
        for (int p = 0; p < w->players && n < DRAW_MAX; p++)
            list[n++] = (drawable){w->q[p].s - w->cam_s - 1, DK_SQ, (int16_t)p};
    for (int k = 0; k < DROPS && n < DRAW_MAX; k++)
        if (w->drops[k].live) list[n++] = (drawable){w->drops[k].s * 256 - w->cam_s, DK_DROP, (int16_t)k};
    int32_t far = (DRAW_SEGS - 2) * SEG_LEN * 256;
    for (int i = 0; i < w->ob_count && n < DRAW_MAX; i++) {
        const obj *o = ob_get(w, i);
        int32_t z = o->s * 256 - w->cam_s;
        if (o->kind == OB_FORK) z += o->len * 256;
        if (o->kind == OB_SAP) z += o->len * 128;
        if (o->kind == OB_CROW) z += o->len * 256;
        if (o->kind == OB_GAP) z += o->len * 256;          /* drawn with its far face */
        if (z < CAM_NEAR * 256 || z > far) continue;
        if (o->kind <= OB_WALNUT && (o->taken || z > 1400 * 256)) continue;
        if (o->kind == OB_WIND || o->kind == OB_RAIN || o->kind == OB_SHOWER) continue;
        if (o->kind == OB_DECOR && z < 60 * 256) continue;
        if (o->kind == OB_DECOR && o->len <= DC_SIDE2 && z > 1200 * 256) continue;   /* the far end stays clear */
        /* a side branch passing the camera drops a big leaf sweeping past the edge of the screen */
        if (o->kind == OB_DECOR && o->len <= DC_SIDE2 && z < 150 * 256 && z >= 150 * 256 - w->speed &&
            ((o->s * 7) & 3) != 0)
            parts_spawn(o->side > 0 ? 300 : 20, 20 + (o->s & 63), PK_FGLEAF, o->side * 6, 5, 34);
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
        else if (list[i].kind == DK_DROP) draw_drop(w, &w->drops[list[i].idx]);
        else draw_obj(w, ob_get(w, list[i].idx));
    }
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(draw_line_x); S(draw_line_a); S(draw_line_row); S(draw_line_tcx); S(line_wl); S(line_wr); S(bg2_dx); S(bg_style); S(sky); S(bark_sel);
    S(bark_pal); S(shown_state); S(shown_best); S(band_now); S(parts);
    S(pool_slot); S(pool_stamp); S(pool_used); S(pool_now);
    RS_STATE_RASTER(raster);
}
#undef S
