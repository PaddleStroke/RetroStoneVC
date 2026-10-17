/*
 * Fishy Feast: video. The house layout (docs/art-direction.md), seen from above the pond:
 *   BG1 the UI (the house kit, priority 1); BG2 the light rays (added light, swaying by line scroll); BG3 the plankton
 *   specks (a screen-space map written every frame: a dot tile at the speck's place on a 2-px grid, its size by the
 *   zoom); BG4 the pond floor, the AFFINE layer (Mode-7-like): it scales with the camera's zoom, set per view and per
 *   line by the raster callback, with the pond's rounded edge cut by window 1 and a rim by window 2 (the backdrop
 *   between them is the stone rim; outside window 2 it is fogged into deep water).
 *   Sprites: the fish (OBJ 0, 1, 4, 5: the players' palettes, the A ramp, their outline the player's colour ring; the
 *   bots in the B ramps of those and in OBJ 6 and 7: 8 colours), the urchins, bubbles and the touch controls (OBJ 2),
 *   the kit's digits, glyphs and medals (OBJ 3).
 * The zoom: each view has a camera that follows its player's fish; its zoom steps out as the fish grows (ZOOM_STEP_TABLE,
 *   eased), also to keep all its pieces in view and so that the biggest fish in sight fits the biggest sprite tier.
 *   Sprites do not scale: a fish is drawn in the size tier nearest its radius on screen (assets.h gm_tier_r: one sprite
 *   up to 24 px, then a body of four quarters (top and bottom, mirrored left-right), a face, a tail and a highlight). Bigger fish are drawn in front.
 * Split screen (play.c view_layout): 1 player full screen; 2 side by side; 3-4 in quarters (3: the 4th shows the whole
 *   pond as a map). Each view is a viewport with its own scroll, sprites and raster state (the floor's matrix and the
 *   pond's windows). One full view (the title, the panels) runs without viewports: the kit's sprites stay in place.
 * Widescreen (docs/spec.md "Widescreen"): the 320-px stage is centred; a wider screen shows more of the pond (the views
 *   are wider), the rules are unchanged; the UI kit centres itself.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/fishyfeast/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

/* the title logo: FISHY in goldfish orange, FEAST in pond aqua (light, mid, dark) */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 230, 160), RS_RGB8(252, 168, 66), RS_RGB8(222, 104, 34)},
    {RS_RGB8(206, 252, 246), RS_RGB8(100, 214, 210), RS_RGB8(36, 148, 168)}};
#define RIM_TOP     0x9aa684        /* the pond's stone rim (the backdrop), top and bottom of the screen */
#define RIM_BOTTOM  0x7a8a70
#define DEEP_WATER  0x0a1430        /* outside the pond (the fog's colour) */
#define DIVIDER     0x0c142c
#define RIM_PX      3               /* the rim's width on screen */
#define TITLE_ZOOM  182             /* the title's camera (1/256) */
#define CAM_FOLLOW  4               /* the camera goes 1/4 of the way to its fish a frame */
#define SPR_BUDGET_1 116            /* sprites per view: 1 view, 2, 3-4 (the guideline: 128 in all) */
#define SPR_BUDGET_2 58
#define SPR_BUDGET_4 29
#define LB_GAP       2              /* px between the leaderboard's rows (lb_lines) */
#define LB_SOLO_FREE_ROW 10         /* 1 player: the first BG1 row shown as it is under the leaderboard */

typedef struct camera { int32_t x, y, zq, step; int32_t on; } camera;   /* Q8 centre, zoom Q16, step 1/256 */
enum { CAM_SOLO = 4 };              /* cam[0..3]: the players' views; cam[4]: the title, the panels */
static camera cam[5];

/* the raster's view of each viewport (rebuilt every frame by draw_frame) */
typedef struct vline {
    int16_t vcx, vcy;               /* the view's centre, stage x and screen line */
    int16_t hofs, vofs, floor_on, pad_;
    int32_t cx, cy, zq;             /* the camera */
    rs_affine aff;
} vline;
static vline VL[RS_VIEW_MAX];
static rs_viewport VP[RS_VIEW_MAX];
static int views_on;
static rs_color rim_col[RS_SCREEN_H];
static int16_t ray_dx[RS_SCREEN_H];             /* BG2's line scroll: the rays sway */
static int16_t lb_dy[RS_SCREEN_H];              /* BG1's line scroll in play: a gap under each leaderboard row */
static int shown_key = -1, shown_best = -1;
static uint32_t shown_text;                     /* the play texts on BG1 (a hash: redrawn when they change) */
static int text_paused;
static int drawn_width;

/* the bubbles: eaten pieces, bursts, merges (from the world's fx list), rising */
typedef struct bub { int16_t x, y, t, kind; } bub;
#define BUBS 32
static bub bubs[BUBS];
static int last_fx_t = -1;

static void raster(int line, void *user);

static int map_w(void) { return rs_screen_width() > 512 ? 128 : 64; }
/* a tall screen (DESIGN.md "Tall screen"): the specks' screen-space map holds the whole height (128 rows) */
static int specks_rows(void) { return rs_screen_height() > 256 ? 128 : 32; }
static int ray_t;                   /* the rays' sway this frame (the raster carries it past the stage's lines) */
static int fdiv8(int v) { return v >= 0 ? v / 8 : -((7 - v) / 8); }
/* a zoomed tall screen while one fish plays (DESIGN.md "Tall screen"): BG1's top HUD_LINES (the leaderboard) pinned at
 * screen line HUD_PIN_Y (under the frontend's pause button), set by draw_frame before every picture; 0: none */
#define HUD_PIN_Y 22
#define HUD_LINES (LB_SOLO_FREE_ROW * 8)
static int hud_lines;
static int tall(void) { return rs_screen_height() > RS_SCREEN_H; }

void view_shown(vrect *v)
{
    int x0 = rs_screen_visible_x0() - rs_screen_origin_x(), x1 = rs_screen_visible_x1() - rs_screen_origin_x();
    if (v->x < x0) v->w -= x0 - v->x, v->x = x0;
    if (v->x + v->w > x1) v->w = x1 - v->x;
}

/* the UI x just past the right edge the HUD keeps to: the screen's, or a zoomed screen's last column shown */
static int ui_right(void) { return tall() ? rs_screen_visible_x1() - rs_screen_origin_x() : hu_ui_right(); }

static void colour_pal(int colour, int *pal, int *variant)
{
    static const uint8_t slot[8][2] = BOT_SLOTS;
    if (colour >= 8) {
        *pal = hu_player_pal(colour - 8);
        *variant = 0;
    } else {
        *pal = slot[colour][0];
        *variant = slot[colour][1];
    }
}

static void rim_gradient(void)
{
    int a[3] = {(RIM_TOP >> 16) & 255, (RIM_TOP >> 8) & 255, RIM_TOP & 255};
    int b[3] = {(RIM_BOTTOM >> 16) & 255, (RIM_BOTTOM >> 8) & 255, RIM_BOTTOM & 255};
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int k = y * 256 / (RS_SCREEN_H - 1), c[3];
        for (int i = 0; i < 3; i++) c[i] = (a[i] * (256 - k) + b[i] * k) / 256;
        int d = (y & 4) ? 1 : 0;               /* house rule: a 2-line dither step every 8 lines */
        rim_col[y] = RS_RGB8(c[0] + 4 * d, c[1] + 2 * d, c[2] + 4 * d);
    }
}

static void maps_setup(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG2, RAYS_MAP, RAYS_MAP, VR_BG2);
    rs_bg_setup(RS_BG3, mw, specks_rows(), VR_BG3);
    rs_bg_setup(RS_BG4, FLOOR_MAP, FLOOR_MAP, VR_BG4);
    for (int i = 0; i < RAYS_MAP * RAYS_MAP; i++) rs_bg_put(RS_BG2, i % RAYS_MAP, i / RAYS_MAP, gm_rays_map[i]);
    for (int i = 0; i < FLOOR_MAP * FLOOR_MAP; i++) rs_bg_put(RS_BG4, i % FLOOR_MAP, i / FLOOR_MAP, gm_floor_map[i]);
    rs_bg_line_scroll(RS_BG2, ray_dx, NULL);
    shown_key = -1;
    shown_text = 0;
    drawn_width = rs_screen_width();
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more pond on both sides */
    hu_config hc = hu_defaults();           /* BG1 at VRAM 0, the kit at 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;             /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    rs_tiles_load(VR_BG2, gm_rays_tiles, gm_rays_tile_count);
    rs_tiles_load(VR_BG3, gm_specks_tiles, gm_specks_tile_count);
    rs_tiles_load(VR_BG4, gm_floor_tiles, gm_floor_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_FLOOR), gm_floor_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_RAYS), gm_rays_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_SPECKS), gm_specks_pals, 48);
    maps_setup();
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* P1, P2, the props (the kit loaded OBJ 3) */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 64);     /* P3, P4, the bots */
    /* the pond: the floor, the rays and the specks only inside window 1; outside window 2 the backdrop is deep water */
    rs_bg_window(RS_BG2, RS_WIN1_OUT);
    rs_bg_window(RS_BG3, RS_WIN1_OUT);
    rs_bg_window(RS_BG4, RS_WIN1_OUT);
    rs_fog(RS_WIN2_OUT, RS_HEX(DEEP_WATER), RS_MATH_BACK);
    rs_math(RS_MATH_ADD, RS_MATH_BG2, 0);                 /* the rays add light */
    rim_gradient();
    rs_raster(raster, NULL);
}

/* ---- the cameras ---------------------------------------------------------------------------------------------------- */
static int32_t to_screen(int32_t w, int32_t c, int32_t zq) { return (int32_t)(((int64_t)(w - c) * zq) >> 24); }

static int step_at_most(int32_t zmax)
{
    for (int i = 0; i < ZOOM_STEPS; i++)
        if (zoom_steps[i] <= zmax) return zoom_steps[i];
    return zoom_steps[ZOOM_STEPS - 1];
}

static void cam_snap(camera *c, int32_t x, int32_t y, int step)
{
    c->x = x;
    c->y = y;
    c->step = step;
    c->zq = step << 8;
    c->on = 1;
}

/* player p's camera in view v: follows the centre of its pieces; the zoom steps out as it grows (with a margin before
 * it steps back in), so that its pieces fit, and so that the biggest fish in sight fits the biggest tier */
static void cam_player(camera *c, const world *w, int p, int kind, const vrect *v)
{
    const owner *ow = &w->own[p];
    int target = view_target_px(kind), maxz = kind == VK_QUAD ? zoom_steps[1] : 256;
    if (!c->on) cam_snap(c, ow->cx, ow->cy, zoom_step_for(fish_radius(ow->mass), target, maxz));
    if (ow->alive) {
        int32_t rt = fish_radius(ow->mass);
        int z = zoom_step_for(rt, target, maxz);
        if (z > c->step && c->step > 0 && (((int64_t)rt * z) >> 16) > target * 85 / 100) z = c->step;
        int32_t ex = 0, ey = 0, rbig = 0;
        int32_t hw = (v->w / 2) * FX * 256 / (c->zq >> 8), hh = (v->h / 2) * FX * 256 / (c->zq >> 8);
        for (int i = 0; i < MAX_CELLS; i++) {
            const cell *q = &w->c[i];
            if (!q->on) continue;
            int32_t dx = q->x - ow->cx, dy = q->y - ow->cy;
            if (q->owner == p) {
                int32_t ax = (dx < 0 ? -dx : dx) + q->r, ay = (dy < 0 ? -dy : dy) + q->r;
                if (ax > ex) ex = ax;
                if (ay > ey) ey = ay;
            }
            if (dx - q->r < hw && dx + q->r > -hw && dy - q->r < hh && dy + q->r > -hh && q->r > rbig) rbig = q->r;
        }
        int32_t zfit = 256;
        if (ex > 0) {
            int32_t a = (int32_t)((int64_t)(v->w / 2 - 6) * FX * 256 / ex), b = (int32_t)((int64_t)(v->h / 2 - 6) * FX * 256 / ey);
            zfit = a < b ? a : b;
        }
        if (rbig > 0) {
            int32_t zb = (int32_t)((int64_t)ZOOM_BIG_MAX * FX * 256 / rbig);
            if (zb < zfit) zfit = zb;
        }
        if (z > zfit) z = step_at_most(zfit);
        c->step = z;
        c->x += (ow->cx - c->x) / CAM_FOLLOW;
        c->y += (ow->cy - c->y) / CAM_FOLLOW;
    }
    int32_t d = (c->step << 8) - c->zq;
    c->zq += d / ZOOM_EASE + (d > 0) - (d < 0);
}

/* the title's camera: a slow tour of the pond */
static int tri(int t, int period)          /* 0..256..0 */
{
    t %= period;
    return t < period / 2 ? t * 512 / period : (period - t) * 512 / period;
}

static void cam_tour(camera *c, int t)
{
    int32_t x = (POND_W / 2 + (tri(t, 5400) - 128) * (POND_W / 2 - 300) / 128) * FX;
    int32_t y = (POND_H / 2 + (tri(t + 900, 3800) - 128) * (POND_H / 2 - 220) / 128) * FX;
    if (!c->on) cam_snap(c, x, y, TITLE_ZOOM);
    c->x += (x - c->x) / 16;
    c->y += (y - c->y) / 16;
    c->step = TITLE_ZOOM;
    int32_t d = (c->step << 8) - c->zq;
    c->zq += d / ZOOM_EASE + (d > 0) - (d < 0);
}

void draw_new_run(const world *w)
{
    (void)w;
    for (int i = 0; i < 4; i++) cam[i].on = 0;
    memset(bubs, 0, sizeof bubs);
    shown_text = 0;
}

/* ---- the raster: per line and per view, the floor's matrix and the pond's windows --------------------------------- */
/* the pond grown by e px (the rim's outer edge): its columns on world row y */
static int pond_span_ex(int y, int e, int *x0, int *x1)
{
    if (y < -e || y >= POND_H + e) return 0;
    int C = POND_CORNER + e, d = y < POND_CORNER ? POND_CORNER - y : y >= POND_H - POND_CORNER ? y - (POND_H - POND_CORNER) + 1 : 0;
    if (d >= C) return 0;
    int h = d ? isqrt64((int64_t)C * C - (int64_t)d * d) : C;
    *x0 = POND_CORNER - h;
    *x1 = POND_W - POND_CORNER + h;
    return 1;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void raster(int line, void *user)
{
    (void)user;
    int v = rs_viewport_current();
    const vline *L = &VL[v < 0 ? 0 : v];
    if (line < 0 || line >= RS_SCREEN_H) {
        /* a tall screen's lines past the stage: more of the pond (the floor, its rim and the specks follow the camera
         * by themselves), the rim's end colours carried on, the rays swaying on (one view: the layer's scroll; the
         * line-scroll table is the stage's), no UI layer but the HUD's band pinned at the top (rs_bg_pin) */
        int top = rs_screen_top() + HUD_PIN_Y;
        rs_bg_enable(RS_BG1, hud_lines && line >= top && line < top + hud_lines);
        rs_pal_set(0, rim_col[line < 0 ? 0 : RS_SCREEN_H - 1]);
        rs_bg_scroll(RS_BG2, ray_t / 3 + hu_bob(((line * 3 + ray_t) % 240 + 240) % 240, 240, 3), 0);
    } else if (line == 0) {
        rs_bg_enable(RS_BG1, 1);                /* the stage's own (a 240-line screen: as it always is) */
        if (rs_screen_height() > RS_SCREEN_H) rs_bg_scroll(RS_BG2, ray_t / 3, 0);
        rs_pal_set(0, rim_col[line]);
    } else {
        rs_pal_set(0, rim_col[line]);
    }
    if (!L->floor_on) {
        rs_window(0, 0, 0);
        rs_window(1, 0, 0);                     /* deep water behind the pause band */
        return;
    }
    int32_t inv = (int32_t)((1 << 24) / L->zq);
    int32_t wyq = L->cy + (line - L->vcy) * inv;
    int wy = wyq >> 8, e = RIM_PX * inv >> 8, x0, x1;
    int lo = -1000, hi = 2000;
    if (pond_span_ex(wy, 0, &x0, &x1))
        rs_window(0, clampi(L->vcx + to_screen(x0 * FX, L->cx, L->zq), lo, hi), clampi(L->vcx + to_screen(x1 * FX, L->cx, L->zq), lo, hi));
    else
        rs_window(0, 0, 0);
    if (pond_span_ex(wy, e, &x0, &x1))
        rs_window(1, clampi(L->vcx + to_screen(x0 * FX, L->cx, L->zq) - RIM_PX, lo, hi),
                  clampi(L->vcx + to_screen(x1 * FX, L->cx, L->zq) + RIM_PX, lo, hi));
    else
        rs_window(1, 0, 0);
    rs_bg_affine(RS_BG4, &L->aff);
    rs_bg_scroll(RS_BG4, L->hofs, L->vofs);
}

static void vline_set(vline *L, const vrect *v, const camera *c, int floor_on)
{
    memset(L, 0, sizeof *L);
    L->vcx = (int16_t)(v->x + v->w / 2);
    L->vcy = (int16_t)(v->y + v->h / 2);
    L->cx = c->x;
    L->cy = c->y;
    L->zq = c->zq < 256 ? 256 : c->zq;
    L->floor_on = (int16_t)floor_on;
    int32_t inv = (int32_t)((1 << 24) / L->zq);
    L->aff.a = L->aff.d = inv;
    L->aff.wrap = 1;
    L->hofs = (int16_t)((((int64_t)c->x * L->zq) >> 24) - L->vcx);
    L->vofs = (int16_t)((((int64_t)c->y * L->zq) >> 24) - L->vcy);
}

/* ---- sprites ---------------------------------------------------------------------------------------------------------- */
typedef struct sview {                  /* where a view's sprites go */
    int ox, oy;                         /* the sprite coordinates of the view's top-left (0, 0 with viewports) */
    int w, h;
    int32_t cx, cy, zq;                 /* its camera */
    int budget;                         /* sprites left */
} sview;

static int spr_at(sview *s, int id, int x, int y, int pal, int prio, int flags)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x >= s->ox + s->w || y >= s->oy + s->h || x + d->w <= s->ox || y + d->h <= s->oy) return 0;
    if (s->budget <= 0) return 0;
    s->budget--;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
    return 1;
}

static int tier_of(int px)
{
    int t = 0;
    while (t + 1 < FISH_TIERS && px * px >= gm_tier_r[t] * gm_tier_r[t + 1]) t++;
    return t;
}

/* a fish of radius px on screen at (x, y) (sprite coordinates of its body's centre), its colour, facing left or not */
static void draw_fish_at(sview *s, int x, int y, int px, int colour, int face, int wag, int prio)
{
    int pal, variant, t = tier_of(px);
    colour_pal(colour, &pal, &variant);
    for (int k = 0; k < gm_tier_nparts[t]; k++) {
        const gm_part *p = &gm_fish_parts[variant][t][k];
        int id = p->spr;
        if (k == 0 && wag && gm_fish_wag[variant][t] >= 0) id = gm_fish_wag[variant][t];
        const gm_sprite_def *d = &gm_spr[id];
        int fl = p->flags & 3;
        int sx = face ? x - (p->ox + d->w) : x + p->ox;
        if (face) fl ^= RS_SPR_HFLIP;
        spr_at(s, id, sx, y + p->oy, pal, prio, fl);
    }
}

static int sx_of(const sview *s, int32_t wx) { return s->ox + s->w / 2 + to_screen(wx, s->cx, s->zq); }
static int sy_of(const sview *s, int32_t wy) { return s->oy + s->h / 2 + to_screen(wy, s->cy, s->zq); }

/* the world in one view: bubbles, the players' markers, then fish and urchins (bigger in front), then the pellets */
static void draw_world(sview *s, const world *w, int me, int t)
{
    const int32_t zq = s->zq, ur = fish_radius(URCHIN_MASS * MQ);
    /* bubbles in front */
    for (int i = 0; i < BUBS; i++) {
        const bub *b = &bubs[i];
        if (b->t <= 0) continue;
        int x = sx_of(s, b->x * FX) + ((b->t / 6 + i) % 3 - 1), y = sy_of(s, b->y * FX) - (60 - b->t) / 3;
        spr_at(s, b->kind ? SPR_BUBBLE_L : SPR_BUBBLE_S, x - 4, y - 4, 2, 2, 0);
    }
    /* the players' markers: a chevron in their ring colour over their biggest piece */
    for (int p = 0; p < w->players; p++) {
        const owner *ow = &w->own[p];
        if (!ow->alive || w->players == 1) continue;
        int big = -1;
        for (int i = 0; i < MAX_CELLS; i++)
            if (w->c[i].on && w->c[i].owner == p && (big < 0 || w->c[i].mass > w->c[big].mass)) big = i;
        if (big < 0) continue;
        int r = (int)(((int64_t)w->c[big].r * zq) >> 24);
        spr_at(s, SPR_MARKER, sx_of(s, w->c[big].x) - 4, sy_of(s, w->c[big].y) - r - 11 + hu_bob(t + p * 16, 48, 1), hu_player_pal(p), 2, 0);
    }
    /* fish and urchins, by size (the biggest first: in front); my own pieces always get their sprites */
    int16_t list[MAX_CELLS + URCHINS_N];
    int32_t key[MAX_CELLS + URCHINS_N];
    int n = 0;
    int32_t hw = (s->w / 2 + 70) * FX * 256 / (zq >> 8), hh = (s->h / 2 + 70) * FX * 256 / (zq >> 8);
    for (int i = 0; i < MAX_CELLS; i++) {
        const cell *c = &w->c[i];
        if (!c->on) continue;
        int32_t dx = c->x - s->cx, dy = c->y - s->cy;
        if (dx - c->r > hw || dx + c->r < -hw || dy - c->r > hh || dy + c->r < -hh) continue;
        list[n] = (int16_t)i;
        key[n++] = c->mass * 2 + 1;
    }
    for (int u = 0; u < URCHINS_N; u++) {
        int32_t dx = w->u[u].x * FX - s->cx, dy = w->u[u].y * FX - s->cy;
        if (dx - ur > hw || dx + ur < -hw || dy - ur > hh || dy + ur < -hh) continue;
        list[n] = (int16_t)(MAX_CELLS + u);
        key[n++] = URCHIN_MASS * MQ * 2;
    }
    for (int i = 1; i < n; i++) {               /* insertion sort, heaviest first */
        int16_t l = list[i];
        int32_t k = key[i];
        int j = i - 1;
        while (j >= 0 && key[j] < k) { list[j + 1] = list[j]; key[j + 1] = key[j]; j--; }
        list[j + 1] = l;
        key[j + 1] = k;
    }
    int mine = 0;
    for (int i = 0; i < n; i++) mine += list[i] < MAX_CELLS && w->c[list[i]].owner == me;
    for (int i = 0; i < n; i++) {
        int id = list[i];
        if (id >= MAX_CELLS) {
            const urchin *u = &w->u[id - MAX_CELLS];
            int px = (int)(((int64_t)ur * zq) >> 24), t2 = 0;
            while (t2 + 1 < URCHIN_TIERS && px * px >= gm_urchin_r[t2] * gm_urchin_r[t2 + 1]) t2++;
            const gm_sprite_def *d = &gm_spr[gm_urchin_spr[t2]];
            int sh = u->shake ? ((u->shake / 2) % 3) - 1 : 0;
            if (s->budget > mine) spr_at(s, gm_urchin_spr[t2], sx_of(s, u->x * FX) - d->w / 2 + sh, sy_of(s, u->y * FX) - d->h / 2, 2, 2, 0);
            continue;
        }
        const cell *c = &w->c[id];
        int own = c->owner == me;
        if (!own && s->budget <= mine + 1) continue;
        int px = (int)(((int64_t)c->r * zq) >> 24);
        int moving = (c->vx > 64 || c->vx < -64 || c->vy > 64 || c->vy < -64);
        draw_fish_at(s, sx_of(s, c->x), sy_of(s, c->y), px, w->own[c->owner].colour, c->face,
                     moving && ((t / 8 + id) & 1), 2);
        if (own) mine--;
    }
    /* pellets behind */
    int32_t pr = fish_radius(EJECT_MASS * MQ);
    int ppx = (int)(((int64_t)pr * zq) >> 24), pt = ppx >= 5 ? 2 : ppx >= 3 ? 1 : 0;
    for (int i = 0; i < MAX_PELLETS; i++) {
        const pellet *p = &w->pl[i];
        if (!p->on) continue;
        int pal, variant;
        colour_pal(p->colour, &pal, &variant);
        int id = gm_pellet_spr[variant][pt];
        const gm_sprite_def *d = &gm_spr[id];
        spr_at(s, id, sx_of(s, p->x) - d->w / 2, sy_of(s, p->y) - d->h / 2, pal, 2, 0);
    }
}

/* the whole pond in a quarter (3 players): every fish a dot, the players a little bigger */
static void draw_map(sview *s, const world *w)
{
    for (int o = 0; o < OWNERS; o++) {
        const owner *ow = &w->own[o];
        if (!ow->alive) continue;
        int px = o < MAX_PLAYERS ? 5 : 3;
        int x = sx_of(s, ow->cx), y = sy_of(s, ow->cy);
        if (o < MAX_PLAYERS) spr_at(s, SPR_MARKER, x - 4, y - 14, hu_player_pal(o), 2, 0);
        draw_fish_at(s, x, y, px, ow->colour, ow->face, 0, 2);
    }
}

/* ---- the plankton specks: BG3, a screen-space map rewritten every frame ------------------------------------------------ */
static void specks_clear(void)
{
    uint16_t *m = rs_bg_map(RS_BG3);
    memset(m, 0, sizeof(uint16_t) * (size_t)(rs_bg_map_w(RS_BG3) * rs_bg_map_h(RS_BG3)));
}

/* the camera's pixel parity in a view: the specks sit on a 2-px grid, BG3 is scrolled by this so that they move with
 * the floor 1 px at a time (not in 2-px steps against it) */
static int zpx(int32_t wq, int32_t zq) { return (int)(((int64_t)wq * zq) >> 24); }
static int speck_ofs_x(const camera *c) { return zpx(c->x, c->zq) & 1; }
static int speck_ofs_y(const camera *c) { return zpx(c->y, c->zq) & 1; }

/* the specks seen in view v (stage rect), camera c (the map holds them shifted by the camera's parity) */
static void specks_view(const world *w, const vrect *v, const camera *c)
{
    uint16_t *m = rs_bg_map(RS_BG3);
    int mw = rs_bg_map_w(RS_BG3), mask = mw * 8 - 1;
    int size = c->zq >= 192 * 256 ? 2 : c->zq >= 115 * 256 ? 1 : 0;
    int32_t hw = (v->w / 2 + 4) * FX * 256 / (c->zq >> 8), hh = (v->h / 2 + 4) * FX * 256 / (c->zq >> 8);
    int vcx = v->x + v->w / 2, vcy = v->y + v->h / 2;
    int ccx = zpx(c->x, c->zq), ccy = zpx(c->y, c->zq), ox = ccx & 1, oy = ccy & 1;
    for (int i = 0; i < PLANKTON_N; i++) {
        const speck *k = &w->sp[i];
        if (!k->on) continue;
        int32_t dx = k->x * FX - c->x, dy = k->y * FX - c->y;
        if (dx > hw || dx < -hw || dy > hh || dy < -hh) continue;
        /* rounded like the floor's scroll: the speck's place on the layer minus the camera's */
        int sx = vcx + zpx(k->x * FX, c->zq) - ccx - 1 + ox, sy = vcy + zpx(k->y * FX, c->zq) - ccy - 1 + oy;
        if (sx < v->x || sy < v->y || sx + 4 > v->x + v->w || sy + 4 > v->y + v->h) continue;
        int mx = (sx & mask) >> 3, my = (sy >> 3) & (rs_bg_map_h(RS_BG3) - 1);
        uint16_t *e = &m[my * mw + mx];
        if (*e) continue;
        *e = RS_MAP(1 + size * 16 + ((sy & 7) >> 1) * 4 + ((sx & 7) >> 1), PAL_SPECKS + k->kind, 0, 0, 0);
    }
}

/* ---- the bubbles: from the world's effects of the last step ------------------------------------------------------- */
static void bubbles_step(const world *w)
{
    for (int i = 0; i < BUBS; i++)
        if (bubs[i].t > 0) bubs[i].t--;
    if (w->t == last_fx_t) return;
    last_fx_t = w->t;
    for (int f = 0; f < w->nfx; f++) {
        int n = w->fx[f].kind == FX_BURST ? 5 : w->fx[f].kind == FX_EAT ? 3 : 2;
        for (int k = 0; k < n; k++) {
            int slot = 0;
            for (int i = 0; i < BUBS; i++)
                if (bubs[i].t < bubs[slot].t) slot = i;
            bub *b = &bubs[slot];
            int r = w->fx[f].r;
            b->x = (int16_t)(w->fx[f].x + ((k * 37 + f * 11) % (2 * r + 1)) - r);
            b->y = (int16_t)(w->fx[f].y + ((k * 53 + f * 7) % (r + 1)) - r / 2);
            b->t = (int16_t)(60 - k * 6);
            b->kind = (int16_t)((k + f) & 1);
        }
    }
}

/* ---- the UI ----------------------------------------------------------------------------------------------------------- */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    sview s = {rs_screen_left(), 0, rs_screen_width(), RS_SCREEN_H, 0, 0, 65536, 8};
    draw_fish_at(&s, cx + 2, cy + hu_bob(t + p * 16, 64, 1), 7, 8 + p, 0, (t / 10) & 1, 3);
}

static void standing(const world *w, hu_standing *s)
{
    int score[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) score[p] = w->own[p].best;
    hu_rank(s, w->players, score, score);
}

static uint32_t hash_str(uint32_t h, const char *s, int x, int y)
{
    h = (h ^ (uint32_t)(x * 131 + y)) * 16777619u;
    while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

typedef struct ptext { int x, y, box; char s[48]; } ptext;

static void add_text(ptext *t, int *n, int x, int y, const char *s)
{
    if (*n >= 24) return;
    t[*n].x = x;
    t[*n].y = y;
    t[*n].box = 0;
    snprintf(t[*n].s, sizeof t[*n].s, "%s", s);
    (*n)++;
}

/* centred on the stage columns [x0, x0 + w) */
static void add_centred(ptext *t, int *n, int x0, int w, int y, const char *s)
{
    add_text(t, n, fdiv8(x0 + w / 2) - hu_width(s, 0) / 2, y, s);
}

static void name_of(const world *w, int o, char *buf, size_t n)
{
    if (o < MAX_PLAYERS) rs_strf(buf, n, HTR(PLAYER_TAG), o + 1);
    else snprintf(buf, n, "%s", fish_names[w->own[o].name]);
}

/* the play texts: the leaderboard, the ranks, the messages; drawn again only when they change */
static void play_texts(const world *w, int state, const vrect *v, int nv, int paused)
{
    ptext t[24];
    int n = 0;
    char s[64], nm[24];
    int top[5], nt = leaderboard(w, top, 5);
    if (w->players == 1) {
        int right = fdiv8(ui_right()) - 1;      /* (a zoomed screen: in the columns shown) */
        int me_in = 0;
        for (int i = 0; i < nt; i++) {
            name_of(w, top[i], nm, sizeof nm);
            snprintf(s, sizeof s, "%d %-8s", i + 1, nm);
            add_text(t, &n, right - hu_width(s, 0), 1 + i, s);
            me_in |= top[i] == 0;
        }
        int r = owner_rank(w, 0);
        if (!me_in && r) {
            name_of(w, 0, nm, sizeof nm);
            snprintf(s, sizeof s, "%d %-8s", r, nm);
            add_text(t, &n, right - hu_width(s, 0), 7, s);
        }
    } else {
        int right = fdiv8(hu_ui_right()) - 1;
        for (int i = 0; i < nt; i++) {
            name_of(w, top[i], nm, sizeof nm);
            snprintf(s, sizeof s, "%d %-8s", i + 1, nm);
            add_text(t, &n, right - hu_width(s, 0), 24 + i, s);
        }
        for (int p = 0; p < w->players && p < nv; p++) {
            int r = owner_rank(w, p);
            if (!r) continue;
            snprintf(s, sizeof s, "#%d", r);
            add_centred(t, &n, v[p].x, v[p].w, fdiv8(v[p].y) + 3, s);
        }
    }
    for (int p = 0; p < w->players && p < nv; p++) {
        const owner *ow = &w->own[p];
        int row = fdiv8(v[p].y + v[p].h / 2);
        if (ow->out && ow->eaten_by >= 0) {
            name_of(w, ow->eaten_by, nm, sizeof nm);
            rs_strf(s, sizeof s, TR(EATEN_BY), nm);
            add_centred(t, &n, v[p].x, v[p].w, row, s);
        } else if (w->msg_top[p] > 0) {
            /* 1 player: under the leaderboard's spaced rows (lb_lines; clear of its row 5 and whole when it is tall) */
            int row = w->players == 1 ? LB_SOLO_FREE_ROW + 1 : fdiv8(v[p].y) + 5;
            add_centred(t, &n, v[p].x, v[p].w, row, TR(BIGGEST));
        }
    }
    if (w->players == 1 && w->t < HINT_FRAMES && state == ST_PLAY && n < 24) {
        add_centred(t, &n, 0, RS_SCREEN_W, 27, TR(HINT));
        t[n - 1].box = 1;                       /* on a panel: readable over the fish and the urchins */
    }
    uint32_t h = 2166136261u + (uint32_t)paused;
    for (int i = 0; i < n; i++) h = hash_str(h, t[i].s, t[i].x, t[i].y + t[i].box * 64);
    if (h == shown_text) return;
    shown_text = h;
    hu_clear();
    for (int i = 0; i < n; i++) {
        if (!t[i].box) {
            hu_text(t[i].x, t[i].y, t[i].s);
            continue;
        }
        /* a navy panel round the line (the row above it too when the text is tall: accents, CJK) */
        int tw = hu_width(t[i].s, 0), pad = tw + 4 <= 40 ? 2 : 1, top = t[i].y - (hu_tall(t[i].s) ? 2 : 1);
        hu_panel(t[i].x - pad, top, tw + 2 * pad, t[i].y + 2 - top);
        hu_box_text(t[i].x, t[i].y, t[i].s);
    }
}

/* the leaderboard's rows on BG1 drawn with a gap under each (BG1's line scroll maps the screen lines to the layer's
 * rows, the gaps to its blank row 31): 1 player rows 1-7 (the top 5, a free row, my rank) from line 8; 2-4 players
 * rows 24-28 from line 190, down to the bottom */
static void lb_lines(int multi)
{
    int r0 = multi ? 24 : 1, n = multi ? 5 : 7, y0 = multi ? 190 : 8;
    int top = multi ? 184 : 8, end = multi ? RS_SCREEN_H : LB_SOLO_FREE_ROW * 8;
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int ly = y;
        if (y >= top && y < end) {
            int k = (y - y0) / (8 + LB_GAP), off = (y - y0) % (8 + LB_GAP);
            ly = y < y0 || k >= n || off >= 8 ? 31 * 8 + (y & 7) : (r0 + k) * 8 + off;
        }
        lb_dy[y] = (int16_t)(ly - y);
    }
}

/* the BG screens of the title and the panels */
static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    int screen = state == ST_DEAD ? ST_PLAY : state;
    if (screen != shown_key || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best) {
        hu_clear();
        shown_text = 0;
        if (state == ST_TITLE) hu_logo("FISHY FEAST", title_ramps, 2, 2, 0);
        if (state == ST_OVER) {
            if (w->players == 1) {
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->own[0].best, 0);
                if (w->own[0].eaten_by >= 0) {
                    char s[64], nm[24];
                    name_of(w, w->own[0].eaten_by, nm, sizeof nm);
                    rs_strf(s, sizeof s, TR(EATEN_BY), nm);
                    hu_gameover_note(s);
                }
            } else {
                hu_results_panel(rs, NULL);
            }
        }
        shown_key = screen;
        shown_best = best;
    }
    if (state == ST_TITLE) hu_title_draw(st_t, best);
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
}

/* the touch controls in player 1's view: the stick where the finger went down, SPLIT and EJECT */
static void draw_touch(sview *s, const vrect *v, const touch_view *tv)
{
    tbutton bs, be;
    vrect vb = *v;
    view_shown(&vb);                            /* a zoomed screen: the buttons in the columns shown (main.c p1_view) */
    touch_buttons(&vb, &bs, &be);
    int dx = s->ox - v->x, dy = s->oy - v->y;      /* stage -> sprite coordinates */
    const struct { const tbutton *b; int on, q, qon, icon; } btn[2] = {
        {&bs, tv->split_on, SPR_BTN_SPLIT, SPR_BTN_SPLIT_ON, SPR_ICON_SPLIT},
        {&be, tv->eject_on, SPR_BTN_EJECT, SPR_BTN_EJECT_ON, SPR_ICON_EJECT}};
    if (tv->stick_on) {
        int ox = tv->sx0 - rs_screen_origin_x() + dx, oy = tv->sy0 - rs_screen_origin_y() + dy;
        int kx = tv->sx - tv->sx0, ky = tv->sy - tv->sy0, d = isqrt64((int64_t)kx * kx + (int64_t)ky * ky);
        if (d > STICK_R) { kx = kx * STICK_R / d; ky = ky * STICK_R / d; }
        spr_at(s, SPR_KNOB, ox + kx - 8, oy + ky - 8, 2, 3, 0);
        const gm_sprite_def *q = &gm_spr[SPR_STICK];
        spr_at(s, SPR_STICK, ox - q->w, oy - q->h, 2, 3, 0);
        spr_at(s, SPR_STICK, ox, oy - q->h, 2, 3, RS_SPR_HFLIP);
        spr_at(s, SPR_STICK, ox - q->w, oy, 2, 3, RS_SPR_VFLIP);
        spr_at(s, SPR_STICK, ox, oy, 2, 3, RS_SPR_HFLIP | RS_SPR_VFLIP);
    }
    for (int i = 0; i < 2; i++) {
        int x = btn[i].b->x + dx, y = btn[i].b->y + dy + (btn[i].on ? 1 : 0), id = btn[i].on ? btn[i].qon : btn[i].q;
        const gm_sprite_def *q = &gm_spr[id];
        spr_at(s, btn[i].icon, x - 8, y - 8, 2, 3, 0);
        spr_at(s, id, x - q->w, y - q->h, 2, 3, 0);
        spr_at(s, id, x, y - q->h, 2, 3, RS_SPR_HFLIP);
        spr_at(s, id, x - q->w, y, 2, 3, RS_SPR_VFLIP);
        spr_at(s, id, x, y, 2, 3, RS_SPR_HFLIP | RS_SPR_VFLIP);
    }
}

static void view_set(rs_viewport *q, const vrect *v, int layers)
{
    memset(q, 0, sizeof *q);
    q->x = (int16_t)v->x;
    q->y = (int16_t)v->y;
    q->w = (int16_t)v->w;
    q->h = (int16_t)v->h;
    q->layers = (uint8_t)layers;
    q->objs = 1;
    int m = map_w() * 8 - 1;
    q->sx[RS_BG1] = (int16_t)(v->x & m);
    q->sy[RS_BG1] = (int16_t)v->y;
    q->sx[RS_BG3] = (int16_t)(v->x & m);
    q->sy[RS_BG3] = (int16_t)v->y;
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, const touch_view *tv)
{
    static const int medals[4] = MEDAL_SCORES;
    if (drawn_width != rs_screen_width()) maps_setup();
    hu_standing rs;
    standing(w, &rs);
    bubbles_step(w);
    int t = (int)rs_frame_count();
    for (int y = 0; y < RS_SCREEN_H; y++) ray_dx[y] = (int16_t)((hu_bob(y * 3 + t, 240, 3)));
    ray_t = t;
    int multi = (state == ST_PLAY || state == ST_DEAD) && w->players > 1;
    vrect v[4], vs[4];
    int nv = view_layout(multi ? w->players : 1, rs_screen_left(), rs_screen_width(), v);
    int kind = view_kind(multi ? w->players : 1);
    /* a tall screen: the views drawn to its top and bottom (more of the pond round each camera); the cameras, their
     * zoom and the texts keep the stage's rects (vs), so the game and the stage's pictures stay the same */
    memcpy(vs, v, sizeof v);
    views_tall(v, nv, rs_screen_top(), rs_screen_bottom());
    int extra = rs_screen_height() > RS_SCREEN_H ? 80 : 0;     /* the sprites of the taller views */
    /* a zoomed tall screen (DESIGN.md "Tall screen"): one fish playing fills the display's height, the sides cropped:
     * its camera fits the columns shown and the whole height, the leaderboard at the screen's top (BG1's top lines
     * pinned, the raster shows them), the mass above it; the title, the panels and a split screen show every column */
    int solo = (state == ST_PLAY || state == ST_DEAD) && w->players == 1;
    rs_screen_fill(solo);
    hud_lines = tall() && solo ? HUD_LINES : 0;
    rs_bg_pin(RS_BG1, 0, hud_lines, HUD_PIN_Y);
    if (hud_lines) {
        vs[0] = v[0];
        view_shown(&vs[0]);
    }

    /* the cameras */
    if (state == ST_PLAY || state == ST_DEAD) {
        for (int p = 0; p < w->players && p < nv; p++) cam_player(&cam[p], w, p, kind, &vs[p]);
        cam[CAM_SOLO] = cam[0];
    } else if (state == ST_TITLE) {
        cam_tour(&cam[CAM_SOLO], st_t);
    } else if (w->players > 1) {
        cam_tour(&cam[CAM_SOLO], st_t + 2000);
    }
    camera mapcam = {0, 0, 65536, 256, 1};
    mapcam.x = POND_W / 2 * FX;
    mapcam.y = POND_H / 2 * FX;
    if (nv == 4) {
        int32_t a = (int32_t)((int64_t)v[3].w * 65536 / (POND_W + 40)), b = (int32_t)((int64_t)v[3].h * 65536 / (POND_H + 40));
        mapcam.zq = a < b ? a : b;
    }

    /* the UI layer */
    if (state == ST_PLAY || state == ST_DEAD) {
        if (shown_key != ST_PLAY) {
            shown_key = ST_PLAY;
            shown_text = 0;
        }
        if (paused != text_paused) {        /* PAUSED takes rows of the UI layer: the texts are drawn again after */
            text_paused = paused;
            if (!paused) shown_text = 0;
        }
        if (!paused) play_texts(w, state, vs, nv, paused);
        lb_lines(multi);
        rs_bg_line_scroll(RS_BG1, NULL, lb_dy);
    } else {
        rs_bg_line_scroll(RS_BG1, NULL, NULL);
        screen_text(w, state, st_t, best, new_best, &rs);
    }
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, 0, -slide);
    rs_bg_scroll(RS_BG2, t / 3, 0);
    rs_bg_scroll(RS_BG3, 0, 0);
    hu_pause(paused, 13);

    /* the specks */
    specks_clear();
    rs_oam_clear();
    if (!multi) {
        /* one full view, no viewports: the kit's sprites are in stage coordinates */
        const camera *c = (state == ST_PLAY || state == ST_DEAD) ? &cam[0] : &cam[CAM_SOLO];
        specks_view(w, &v[0], c);
        rs_bg_scroll(RS_BG3, speck_ofs_x(c), speck_ofs_y(c));
        vline_set(&VL[0], &v[0], c, 1);
        sview s = {v[0].x, v[0].y, v[0].w, v[0].h, c->x, c->y, c->zq, SPR_BUDGET_1 + extra};
        if (state == ST_PLAY || state == ST_DEAD) {
            if (w->own[0].alive) hu_number(owner_mass(w, 0), RS_SCREEN_W / 2, 8 + (hud_lines ? rs_screen_top() : 0), 3);
            if (tv->seen && state == ST_PLAY) draw_touch(&s, &v[0], tv);
        } else if (state == ST_OVER) {
            if (w->players == 1) {
                hu_gameover_sprites(1, w->own[0].best, 0, best, hu_medal_of(w->own[0].best, medals), st_t, slide);
            } else {
                hu_results_sprites(&rs, st_t, slide);
                for (int i = 0; i < rs.n; i++) {
                    int cx, cy;
                    hu_results_icon_pos(&rs, i, &cx, &cy);
                    sview k = {rs_screen_left(), 0, rs_screen_width(), RS_SCREEN_H, 0, 0, 65536, 8};
                    draw_fish_at(&k, cx + 2, cy + slide, 7, 8 + rs.order[i], 0, 0, 3);
                }
            }
        } else {
            hu_title_sprites(st_t, slot_icon, NULL);
            sview k = {rs_screen_left(), 0, rs_screen_width(), RS_SCREEN_H, 0, 0, 65536, 12};
            draw_fish_at(&k, RS_SCREEN_W / 2 + 6, 112 + hu_bob(st_t, 64, 4), 19, 8, 0, (st_t / 10) & 1, 2);
        }
        s.budget = SPR_BUDGET_1 + extra - rs_oam_next();
        draw_world(&s, w, (state == ST_PLAY || state == ST_DEAD) ? 0 : -1, t);
        if (views_on) {
            rs_viewports(0, NULL, 0);
            views_on = 0;
        }
        return;
    }

    /* split screen: a viewport per player (3 players: the 4th quarter is the map), the pause band over them */
    int budget = (nv == 2 ? SPR_BUDGET_2 : SPR_BUDGET_4) + extra / nv;
    for (int i = 0; i < nv; i++) {
        rs_viewport *q = &VP[i];
        /* quarters: no light rays (the A20's budget: the floor's matrix is decoded twice a line with 4 views) */
        view_set(q, &v[i], nv == 4 ? 0x0f & ~(1 << RS_BG2) : 0x0f);
        hu_view_oam_begin(q);
        int p = i < w->players ? i : -1;
        const camera *c = p >= 0 ? &cam[p] : &mapcam;
        q->sx[RS_BG3] = (int16_t)(q->sx[RS_BG3] + speck_ofs_x(c));    /* the specks' 1-px steps (specks_view) */
        q->sy[RS_BG3] = (int16_t)(q->sy[RS_BG3] + speck_ofs_y(c));
        sview s = {0, 0, v[i].w, v[i].h, c->x, c->y, c->zq, budget};
        if (p >= 0) {
            if (w->own[p].alive) {
                char d[12];
                int len = snprintf(d, sizeof d, "%d", owner_mass(w, p));
                hu_number(owner_mass(w, p), v[i].w / 2, 6, 3);
                sview k = s;
                k.budget = 1;
                draw_fish_at(&k, v[i].w / 2 - len * 6 - 12, 13, 5, 8 + p, 0, 0, 3);
            }
            if (p == 0 && tv->seen && state == ST_PLAY) draw_touch(&s, &v[0], tv);
            s.budget = budget - (rs_oam_next() - q->oam_first);
            draw_world(&s, w, p, t);
        } else {
            draw_map(&s, w);
        }
        hu_view_oam_end(q);
        vline_set(&VL[i], &v[i], c, 1);
        specks_view(w, &v[i], c);
        if (p < 0) {                            /* no specks on the map: clear what was written there */
            uint16_t *m = rs_bg_map(RS_BG3);
            int mw = rs_bg_map_w(RS_BG3), mask = mw * 8 - 1;
            for (int y = v[i].y; y < v[i].y + v[i].h; y += 8)
                for (int x = v[i].x; x < v[i].x + v[i].w + 8; x += 8)
                    m[((y >> 3) & (rs_bg_map_h(RS_BG3) - 1)) * mw + ((x & mask) >> 3)] = 0;
        }
    }
    int n = nv;
    if (paused) {
        vrect band = {rs_screen_left(), 96, rs_screen_width(), 40};
        view_set(&VP[n], &band, 1 << RS_BG1);
        VP[n].objs = 0;
        vline_set(&VL[n], &band, &cam[0], 0);
        n++;
    }
    rs_viewports(n, VP, RS_HEX(DIVIDER));
    views_on = 1;
}

/* ---- save states (main.c) ---- */
/* a state saved at another width brings maps of another size: set them up for this width, redraw the UI */
void draw_state_loaded(void)
{
    rs_bg_enable(RS_BG1, 1);                    /* (a tall screen's raster ends its frame with it off) */
    if (rs_bg_map_w(RS_BG1) == map_w() && rs_bg_map_h(RS_BG3) == specks_rows()) return;
    maps_setup();
    shown_key = -1;
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(cam); S(VL); S(VP); S(views_on); S(rim_col); S(ray_dx); S(lb_dy); S(shown_key); S(shown_best); S(shown_text); S(text_paused);
    S(bubs); S(last_fx_t);
    RS_STATE_RASTER(raster);
}
#undef S
