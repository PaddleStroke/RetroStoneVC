/*
 * Ladybug Laps: video. The house layout (docs/art-direction.md), seen from above:
 *   BG1 the UI (the house kit, its text at priority 1); BG3 the track's world (the table, the band, the objects), a
 *   32-row map STREAMED from the track's whole map (build_assets.py) round the camera every frame; BG4 what is below
 *   the table (or beyond the garden's frame), a repeated 64-px texture, scrolled at half the speed under a table (far
 *   below: parallax). Each race loads its track's tiles and palettes (BG 1-4 the world, 6 BG4).
 *   Sprites (front to back): the kit's (digits, glyphs, medals), the touch controls, the tags and the warnings, the
 *   leaves over the track (OBJ 6, see-through in the middle), the ladybugs (OBJ 0, 1, 4, 5: red, yellow, blue, green;
 *   in the air in front), the dust, splashes and sparks, the shadows (OBJ 2, the house checker).
 * The camera is the rules' (play.c: it follows the leader); the screen shows the 320-px stage centred on it, and a wider
 * screen shows more of the track round it. The race zone (the left-behind rule) is the 320 x 240 stage at every width:
 * on a wide screen two dotted lines on the UI layer mark its sides.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include "tall_ui.h"
#include <stdio.h>
#include <string.h>

/* the title logo: LADYBUG in ladybug red, LAPS in leaf green (light, mid, dark) */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 170, 150), RS_RGB8(238, 70, 64), RS_RGB8(170, 28, 44)},
    {RS_RGB8(206, 246, 150), RS_RGB8(112, 204, 84), RS_RGB8(48, 140, 60)}};
#define BACKDROP    0x1a1424
#define ZONE_TILE   300             /* the zone's dotted lines (UI layer tiles, after the kit's) */
#define FX_MAX      28
#define TAG_FRAMES  200             /* 1 player: the P1 tag over your ladybug at the start, and after a respawn */
#define MSG_FRAMES  110

typedef struct particle { int16_t x, y, t, kind; } particle;      /* world px, frames left */
enum { PK_DUST, PK_SPLASH, PK_SPARK, PK_PLUS };
static particle fx[FX_MAX];
static int last_fx_t = -1;
static int loaded_track = -1;
static int shown_key = -1, shown_best = -1, text_paused;
static uint32_t shown_text;
static int drawn_width;
static int msg_final, msg_drop, msg_drop_car;   /* frames left of FINAL LAP! and LEFT BEHIND! (and whose) */

static int map_w(void) { return rs_screen_width() > 504 ? 128 : 64; }
/* a tall screen (DESIGN.md "Tall screen"): the UI layer and the streamed world hold its whole height (128 rows: the UI's
 * rows past the stage stay blank) */
static int map_h(void) { return rs_screen_height() > 256 ? 128 : 32; }
static int fdiv8(int v) { return v >= 0 ? v / 8 : -((7 - v) / 8); }

/* a zoomed tall screen (DESIGN.md "Tall screen"): one player racing fills the display's height, the sides cropped, the
 * display panned to keep the ladybug well inside the columns shown (pan: their shift from the middle; the UI layer and
 * the HUD sprites follow it). In a race (any player count) the HUD (BG1's top HUD_LINES: the lap, the standings) is
 * pinned at the screen's top (hud_on), at screen line hud_y: HUD_PIN_Y while one player races (under the frontend's
 * back and pause buttons), HUD_PIN_FIT for 2-4 players (every column shown: the buttons over the bands above). Set by
 * draw_frame before every picture. */
#define HUD_PIN_Y 22
#define HUD_PIN_FIT 4
#define HUD_LINES 64
#define PAN_MARGIN 52               /* px the ladybug keeps from the edges of the columns shown */
static int hud_on, hud_y, pan;
static int tall(void) { return rs_screen_height() > RS_SCREEN_H; }

void view_cols(int *left, int *width)
{
    *left = rs_screen_visible_x0() - rs_screen_origin_x();
    *width = rs_screen_visible_x1() - rs_screen_visible_x0();
}

/* the HUD's columns (UI x): the columns shown with the display in the middle (the UI layer scrolls with the pan) */
static int hud_x0(void)
{
    int l, w;
    view_cols(&l, &w);
    return l - pan;
}
static int hud_x1(void)
{
    int l, w;
    view_cols(&l, &w);
    return l - pan + w;
}

/* the zone's lines: a dashed column at the right edge of a tile (left side) and at the left edge (right side): cream
   dashes 4 px long, outlined in navy on the outer side and at their ends (readable on the pale tracks too) */
static void zone_tiles(void)
{
    uint8_t px[2][64];
    memset(px, 0, sizeof px);
    for (int y = 0; y < 8; y++) {
        int dash = y < 4, cap = y == 4 || y == 7;
        px[0][y * 8 + 7] = dash ? HU_CREAM : cap ? HU_OUTLINE : 0;
        px[1][y * 8] = px[0][y * 8 + 7];
        if (dash || cap) {
            px[0][y * 8 + 6] = HU_OUTLINE;
            px[1][y * 8 + 1] = HU_OUTLINE;
        }
    }
    rs_tiles_load8(VR_BG1 + ZONE_TILE, px[0], 2);
    /* a tall screen: the zone's top (dashes on a tile's bottom row) and bottom (its top row), outlined outside */
    uint8_t ph[2][64];
    memset(ph, 0, sizeof ph);
    for (int x = 0; x < 8; x++) {
        int dash = x < 4, cap = x == 4 || x == 7;
        ph[0][7 * 8 + x] = ph[1][x] = dash ? HU_CREAM : cap ? HU_OUTLINE : 0;
        if (dash || cap) ph[0][6 * 8 + x] = ph[1][8 + x] = HU_OUTLINE;
    }
    rs_tiles_load8(VR_BG1 + ZONE_TILE + 2, ph[0], 2);
}

static void far_fill(void)
{
    if (loaded_track < 0) return;
    const uint16_t *f = gm_tracks[loaded_track].far;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) rs_bg_put(RS_BG4, x, y, f[(y & 7) * 8 + (x & 7)]);
}

static void maps_setup(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG1, mw, map_h(), VR_BG1);
    rs_bg_setup(RS_BG3, mw, map_h(), VR_TRACK);
    rs_bg_setup(RS_BG4, 32, 32, VR_TRACK);
    far_fill();
    shown_key = -1;
    shown_text = 0;
    drawn_width = rs_screen_width();
}

static void load_track(int t)
{
    const gm_track_art *a = &gm_tracks[t];
    rs_tiles_load(VR_TRACK, a->tiles, a->tile_count);
    rs_pal_load(RS_PAL_BG(1), a->pals, 64);           /* the world: BG palettes 1..4 and 7; BG4: 6 */
    rs_pal_load(RS_PAL_BG(6), a->pals + 64, 32);
    loaded_track = t;
    far_fill();
}

void draw_init(void)
{
    rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more track on both sides */
    hu_config hc = hu_defaults();           /* BG1 at VRAM 0, the kit at 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;             /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    zone_tiles();
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* red, yellow, the props (the kit loaded OBJ 3) */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 64);     /* blue, green, the leaves */
    loaded_track = -1;
    maps_setup();
    rs_bg_enable(RS_BG1, 1);
    rs_bg_enable(RS_BG2, 0);
    rs_bg_enable(RS_BG3, 1);
    rs_bg_enable(RS_BG4, 1);
    rs_pal_set(0, RS_HEX(BACKDROP));
}

void draw_new_race(const world *w)
{
    if (loaded_track != w->track) load_track(w->track);
    memset(fx, 0, sizeof fx);
    last_fx_t = -1;
    msg_final = msg_drop = 0;
    shown_text = 0;
}

/* ---- the world: BG3 streamed round the camera ---------------------------------------------------------------- */
static void stream_world(const world *w, int sx0, int sy0)
{
    const gm_track_art *a = &gm_tracks[w->track];
    uint16_t *m = rs_bg_map(RS_BG3);
    int mw = rs_bg_map_w(RS_BG3), mh = rs_bg_map_h(RS_BG3);
    int tx0 = fdiv8(sx0 + rs_screen_left()), tx1 = fdiv8(sx0 + rs_screen_right() - 1) + 1;
    int ty0 = fdiv8(sy0 + rs_screen_top()), ty1 = ty0 + RS_SCREEN_H / 8 + 1;
    if (rs_screen_height() > RS_SCREEN_H) ty1 = fdiv8(sy0 + rs_screen_bottom() - 1) + 1;   /* the whole tall screen */
    for (int ty = ty0; ty <= ty1; ty++) {
        uint16_t *row = m + (ty & (mh - 1)) * mw;
        int in_y = ty >= 0 && ty < a->mh;
        for (int tx = tx0; tx <= tx1; tx++) {
            uint16_t e = (in_y && tx >= 0 && tx < a->mw) ? a->map[ty * a->mw + tx] : 0;
            row[tx & (mw - 1)] = e;
        }
    }
    rs_bg_scroll(RS_BG3, sx0, sy0);
    if (tracks[w->track].table) rs_bg_scroll(RS_BG4, sx0 / 2, sy0 / 2);
    else rs_bg_scroll(RS_BG4, sx0, sy0);
}

/* ---- effects: from the world's events of the last step ------------------------------------------------------------ */
static void fx_add(int x, int y, int kind, int t)
{
    int slot = 0;
    for (int i = 0; i < FX_MAX; i++)
        if (fx[i].t < fx[slot].t) slot = i;
    fx[slot].x = (int16_t)x;
    fx[slot].y = (int16_t)y;
    fx[slot].t = (int16_t)t;
    fx[slot].kind = (int16_t)kind;
}

static void fx_step(const world *w)
{
    for (int i = 0; i < FX_MAX; i++)
        if (fx[i].t > 0) fx[i].t--;
    if (msg_final > 0) msg_final--;
    if (msg_drop > 0) msg_drop--;
    if (w->t == last_fx_t) return;
    last_fx_t = w->t;
    for (int i = 0; i < RACERS; i++) {
        const car *c = &w->c[i];
        int ev = w->events[i], x = c->x / FX, y = c->y / FX, f = car_speed(c);
        if (ev & (EV_BUMP | EV_WALL)) fx_add(x + (i & 1 ? 4 : -4), y - 4, PK_SPARK, 12);
        if (ev & (EV_LAND | EV_SPLASH | EV_JAM)) fx_add(x, y + 3, ev & EV_SPLASH ? PK_SPLASH : PK_DUST, 18);
        if (ev & EV_DROP) {                             /* left behind, out: a puff where it left the zone's box */
            int cx = w->camx / FX, cy = w->camy / FX;
            int px = x < cx - ZONE_HW + 8 ? cx - ZONE_HW + 8 : x > cx + ZONE_HW - 8 ? cx + ZONE_HW - 8 : x;
            int py = y < cy - ZONE_HH + 8 ? cy - ZONE_HH + 8 : y > cy + ZONE_HH - 8 ? cy + ZONE_HH - 8 : y;
            fx_add(px, py, PK_DUST, 18);
            fx_add(px - 5, py + 3, PK_DUST, 14);
            fx_add(px + 5, py + 2, PK_SPARK, 12);
        }
        if (ev & EV_OUT) {                              /* 2-4 players: who is out (+ RACERS: it fell off) */
            msg_drop = MSG_FRAMES;
            msg_drop_car = i + (ev & EV_DROP ? 0 : RACERS);
        }
        if ((ev & EV_LAP) && i == race_focus(w) && c->lap == LAPS - 1) msg_final = MSG_FRAMES;
        if (c->state != CS_RACE || c->z > 0) continue;
        int fast = f > 300 || f < -200;
        if (fast && (w->t + i * 3) % 7 == 0) {         /* dust on the rough, drops on the water, behind it */
            int bx = x - ((icos(c->ang) * 7) >> 14), by = y - ((isin(c->ang) * 7) >> 14);
            if (c->surf == SF_ROUGH || c->surf == SF_JAM) fx_add(bx, by, PK_DUST, 18);
            else if (c->surf == SF_WATER) fx_add(bx, by, PK_SPLASH, 15);
        }
    }
}

/* ---- sprites ------------------------------------------------------------------------------------------------------- */
static int spr(int id, int x, int y, int pal, int prio, int flags)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x >= rs_screen_right() || y >= rs_screen_bottom() || x + d->w <= rs_screen_left() || y + d->h <= rs_screen_top())
        return -1;
    return rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

static int racer_pal(int colour) { return hu_player_pal(colour); }

/* the frame (one of the 9 headings of 0..90 degrees) and the flips of a heading */
static int bug_frame(int ang, int *flags)
{
    int idx = ((ang + 16) >> 5) & 31;
    if (idx <= 8) { *flags = 0; return idx; }
    if (idx <= 16) { *flags = RS_SPR_HFLIP; return 16 - idx; }
    if (idx <= 24) { *flags = RS_SPR_HFLIP | RS_SPR_VFLIP; return idx - 16; }
    *flags = RS_SPR_VFLIP;
    return 32 - idx;
}

/* a ladybug at stage (x, y) (its centre), heading ang, lifted z px */
static void bug_at(int x, int y, int ang, int z, int pal, int prio)
{
    int fl, f = bug_frame(ang, &fl);
    if (z > 0) spr(SPR_HOP_0 + f, x - 12, y - 12 - z, pal, prio, fl);
    else spr(SPR_BUG_0 + f, x - 8, y - 8, pal, prio, fl);
}

typedef struct view { int x0, y0, sh_x, sh_y; } view;     /* the world px at stage 0, 0; the shake */
static int vx(const view *v, int wx) { return wx - v->x0 + v->sh_x; }
static int vy(const view *v, int wy) { return wy - v->y0 + v->sh_y; }

static void draw_cars(const world *w, const view *v, int t)
{
    int ord[RACERS], n = 0;
    for (int i = 0; i < RACERS; i++)
        if (w->c[i].state != CS_OUT) ord[n++] = i;
    for (int i = 1; i < n; i++)                 /* in the air first (in front), then lower on the screen first */
        for (int j = i; j > 0; j--) {
            const car *a = &w->c[ord[j - 1]], *b = &w->c[ord[j]];
            int ka = (a->z > 0 ? 100000 : 0) + a->y / FX, kb = (b->z > 0 ? 100000 : 0) + b->y / FX;
            if (kb > ka) { int s = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = s; }
        }
    for (int k = 0; k < n; k++) {
        const car *c = &w->c[ord[k]];
        int x = vx(v, c->x / FX), y = vy(v, c->y / FX), pal = racer_pal(c->colour);
        if (c->ghost && (t / 3) & 1) continue;                 /* blinking after being put back */
        if (c->state == CS_FALL) {
            int ft = FALL_FRAMES - c->timer;
            if (ft < 12) bug_at(x, y, c->ang, 0, pal, 2);
            else if (ft < 26) spr(SPR_FALL_0 + ((ft / 4) & 1), x - 4, y - 4 + (ft - 12) / 4, pal, 0, 0);
            continue;
        }
        bug_at(x, y, c->ang, c->z / FX, pal, 2);
    }
}

static void draw_shadows(const world *w, const view *v, int t)
{
    for (int i = 0; i < RACERS; i++) {
        const car *c = &w->c[i];
        if (c->state != CS_RACE || (c->ghost && (t / 3) & 1)) continue;
        int z = c->z / FX;
        spr(SPR_SHADOW, vx(v, c->x / FX) - 8 + 2 + z / 4, vy(v, c->y / FX) - 8 + 2 + z / 3, 2, 2, 0);
    }
}

static void draw_fx(const view *v)
{
    for (int i = 0; i < FX_MAX; i++) {
        const particle *p = &fx[i];
        if (p->t <= 0) continue;
        int x = vx(v, p->x), y = vy(v, p->y);
        switch (p->kind) {
        case PK_DUST: spr(SPR_DUST_0 + (p->t > 12 ? 0 : p->t > 6 ? 1 : 2), x - 4, y - 4 - (18 - p->t) / 4, 2, 2, 0); break;
        case PK_SPLASH: spr(SPR_SPLASH_0 + (p->t > 10 ? 0 : p->t > 5 ? 1 : 2), x - 4, y - 4, 2, 2, 0); break;
        case PK_SPARK: spr(SPR_SPARK_0 + (p->t > 6 ? 0 : 1), x - 4, y - 4, 2, 2, 0); break;
        case PK_PLUS: spr(SPR_PLUS, x - 8, y - 4 - (40 - p->t) / 3, 2, 2, 0); break;
        }
    }
}

static void draw_leaves(const world *w, const view *v)
{
    const track_def *t = &tracks[w->track];
    for (int i = 0; i < t->nleaf; i++) {
        const tleaf *l = &t->leaf[i];
        int fl = (l->flip & 1 ? RS_SPR_HFLIP : 0) | (l->flip & 2 ? RS_SPR_VFLIP : 0);
        spr(SPR_LEAF_0 + l->kind, vx(v, l->x) - 16, vy(v, l->y) - 16, 6, 2, fl);
    }
}

/* the tags (P1..P4 over the players), the warnings near the zone's edge, the start lights, the flag */
static void draw_marks(const world *w, const view *v, int t)
{
    for (int i = 0; i < RACERS; i++) {
        const car *c = &w->c[i];
        if (c->state == CS_OUT) continue;
        int x = vx(v, c->x / FX), y = vy(v, c->y / FX) - c->z / FX;
        int show = c->human && (w->humans > 1 || w->t < TAG_FRAMES + COUNTDOWN || c->ghost);
        if (show) spr(SPR_TAG_1 + c->human - 1, x - 7, y - 19, 2, 2, 0);
        if (c->warn && w->phase == PH_RACE && (t / 6) & 1) spr(SPR_WARN, x - 4, y + (show ? -27 : -18), 2, 2, 0);
    }
    if (w->phase == PH_COUNT || (w->phase == PH_RACE && w->phase_t < 50)) {
        for (int k = 0; k < 3; k++) {
            int lit = w->phase == PH_RACE ? 2 : w->phase_t >= k * (COUNTDOWN / 3) + 1 ? 1 : 0;
            spr(SPR_LAMP_0 + lit, RS_SCREEN_W / 2 - 28 + k * 20, 34, 2, 3, 0);
        }
    }
    if (w->phase >= PH_FINISH) {
        spr(SPR_FLAG_0 + ((t / 10) & 1), RS_SCREEN_W / 2 - 64, 76, 2, 3, 0);
        spr(SPR_FLAG_0 + ((t / 10 + 1) & 1), RS_SCREEN_W / 2 + 48, 76, 2, 3, RS_SPR_HFLIP);
    }
}

/* the touch controls: the stick where the finger went down, BRAKE at the bottom right */
static void draw_touch(const touch_view *tv)
{
    tbutton b;
    int vl, vw;
    view_cols(&vl, &vw);                                /* (a zoomed screen: the columns shown) */
    touch_brake_button(vl, vw, &b);
    b.y += rs_screen_bottom() - RS_SCREEN_H;            /* a tall screen: at its bottom (main.c touch_read) */
    if (tv->stick_on) {
        int ox = tv->sx0 - rs_screen_origin_x(), oy = tv->sy0 - rs_screen_origin_y();
        int kx = tv->sx - tv->sx0, ky = tv->sy - tv->sy0, d = isqrt64((int64_t)kx * kx + (int64_t)ky * ky);
        if (d > STICK_R) { kx = kx * STICK_R / d; ky = ky * STICK_R / d; }
        spr(SPR_KNOB, ox + kx - 8, oy + ky - 8, 2, 3, 0);
        spr(SPR_STICK, ox - 24, oy - 24, 2, 3, 0);
        spr(SPR_STICK, ox, oy - 24, 2, 3, RS_SPR_HFLIP);
        spr(SPR_STICK, ox - 24, oy, 2, 3, RS_SPR_VFLIP);
        spr(SPR_STICK, ox, oy, 2, 3, RS_SPR_HFLIP | RS_SPR_VFLIP);
    }
    int y = b.y + (tv->brake_on ? 1 : 0), id = tv->brake_on ? SPR_BTN_ON : SPR_BTN;
    spr(SPR_ICON_BRAKE, b.x - 8, y - 8, 2, 3, 0);
    spr(id, b.x - 24, y - 24, 2, 3, 0);
    spr(id, b.x, y - 24, 2, 3, RS_SPR_HFLIP);
    spr(id, b.x - 24, y, 2, 3, RS_SPR_VFLIP);
    spr(id, b.x, y, 2, 3, RS_SPR_HFLIP | RS_SPR_VFLIP);
}

/* ---- the UI ----------------------------------------------------------------------------------------------------------- */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    bug_at(cx + hu_ui_x(), cy + hu_bob(t + p * 16, 64, 1), 768, 0, racer_pal(p), 3);
}

static uint32_t hash_str(uint32_t h, const char *s, int x, int y)
{
    h = (h ^ (uint32_t)(x * 131 + y)) * 16777619u;
    while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

typedef struct ptext { int x, y, big; char s[56]; } ptext;
#define PTEXTS 16

static void add_text(ptext *t, int *n, int x, int y, int big, const char *s)
{
    if (*n >= PTEXTS) return;
    t[*n].x = x;
    t[*n].y = y;
    t[*n].big = big;
    snprintf(t[*n].s, sizeof t[*n].s, "%s", s);
    (*n)++;
}

static void add_centred(ptext *t, int *n, int y, int big, const char *s) { add_text(t, n, hu_center(s, big), y, big, s); }

static const char *track_name(int track)
{
    switch (track) {
    case 0: return TR(TRACK_TEA);
    case 1: return TR(TRACK_DESK);
    case 2: return TR(TRACK_PICNIC);
    case 3: return TR(TRACK_VEG);
    default: return TR(TRACK_PATIO);
    }
}

static void track_title(const world *w, char *s, size_t n)
{
    if (w->reverse) rs_strf(s, n, TR(REVERSED), track_name(w->track));
    else snprintf(s, n, "%s", track_name(w->track));
}

static void racer_name(const world *w, int i, char *s, size_t n)
{
    if (w->c[i].human) rs_strf(s, n, HTR(PLAYER_TAG), w->c[i].human);
    else snprintf(s, n, "%s", bot_names[w->c[i].colour]);
}

/* the standings' columns (the place, the icon, the points): on 320 a column in from the right edge (as LAP is on the
   left); on a wide screen in the margin, the place clear of the zone's line (column 40); a zoomed screen: in the
   columns shown */
static int stand_right(void)
{
    return fdiv8(tall() ? hud_x1() : hu_ui_right()) - (rs_screen_width() > RS_SCREEN_W ? 1 : 2);
}
static int stand_place(int right) { return rs_screen_width() > RS_SCREEN_W && right - 5 < 41 ? 41 : right - 5; }

/* the zone's sides on a wide screen: dashed lines just outside the 320-px stage (2-4 players: 1 player has no zone) */
static void zone_lines(const world *w)
{
    if (rs_screen_height() > RS_SCREEN_H && race_elimination(w)) {    /* a tall screen: its top and bottom */
        int mh = rs_bg_map_h(RS_BG1);
        for (int x = 0; x < 40; x++) {
            rs_bg_put(RS_BG1, x, (-1) & (mh - 1), RS_MAP(ZONE_TILE + 2, 0, 0, 0, 0));
            rs_bg_put(RS_BG1, x, 30, RS_MAP(ZONE_TILE + 3, 0, 0, 0, 0));
        }
    }
    if (rs_screen_width() <= RS_SCREEN_W || !race_elimination(w)) return;
    int mw = rs_bg_map_w(RS_BG1);
    for (int y = 0; y < 30; y++) {
        rs_bg_put(RS_BG1, (-1) & (mw - 1), y, RS_MAP(ZONE_TILE, 0, 0, 0, 0));
        rs_bg_put(RS_BG1, 40 & (mw - 1), y, RS_MAP(ZONE_TILE + 1, 0, 0, 0, 0));
    }
}

/* the race's texts: the lap, the standings, the messages; drawn again only when they change */
static void race_texts(const world *w, int st_t, int paused, const touch_view *tv, const run_info *ri)
{
    ptext t[PTEXTS];
    int n = 0;
    char s[64], nm[32];
    int lap = w->c[race_focus(w)].lap + 1;          /* 1 player: yours; 2-4: the leader's */
    if (lap < 1) lap = 1;
    if (lap > LAPS) lap = LAPS;
    rs_strf(s, sizeof s, TR(LAP), lap, LAPS);
    add_text(t, &n, tall() ? fdiv8(hud_x0() + 7) + 1 : 1, 1, 0, s);    /* (a zoomed screen: in the columns shown) */
    int right = stand_right();                      /* the standings: the place, the icon (a sprite), the points */
    for (int k = 0; k < RACERS; k++) {              /* (the run's so far, with this race's once it is over) */
        int i = w->order[k];
        const car *c = &w->c[i];
        snprintf(s, sizeof s, "%d", k + 1);
        add_text(t, &n, stand_place(right), 1 + k * 2, 0, s);
        if (c->out) add_text(t, &n, right + 1 - hu_width(TR(OUT_TAG), 0), 1 + k * 2, 0, TR(OUT_TAG));
        else {
            snprintf(s, sizeof s, "%2d", ri->total[i] + c->points);
            add_text(t, &n, right - 1, 1 + k * 2, 0, s);
        }
    }
    if (w->phase == PH_COUNT) {
        track_title(w, s, sizeof s);
        add_centred(t, &n, 19, 0, s);
        rs_strf(s, sizeof s, TR(RACE_N), ri->race + 1, ri->races);
        add_centred(t, &n, 21, 0, s);
    } else if (w->phase == PH_RACE && w->phase_t < 45) {
        add_centred(t, &n, 9, 1, HTR(GO));
    } else if (w->phase >= PH_FINISH) {
        add_centred(t, &n, 9, 1, TR(FINISH));
    } else if (msg_final > 0) {
        add_centred(t, &n, 9, 1, TR(FINAL_LAP));
    }
    if (msg_drop > 0 && w->phase == PH_RACE) {      /* 2-4 players: who went out, and how */
        racer_name(w, msg_drop_car % RACERS, nm, sizeof nm);
        rs_strf(s, sizeof s, msg_drop_car >= RACERS ? TR(FELL_P) : TR(BEHIND_P), nm);
        add_centred(t, &n, 13, 0, s);
    }
    if (ri->race == 0 && w->humans == 1 && !tv->seen && w->t < HINT_FRAMES + COUNTDOWN &&
        (!tall() || hu_width(TR(HINT), 0) * 8 <= hud_x1() - hud_x0() - 8))    /* (where it fits: a zoomed screen) */
        add_centred(t, &n, 27, 0, TR(HINT));
    (void)st_t;
    uint32_t h = 2166136261u + (uint32_t)paused;
    for (int i = 0; i < n; i++) h = hash_str(h, t[i].s, t[i].x, t[i].y * 2 + t[i].big);
    if (h == shown_text) return;
    shown_text = h;
    hu_clear();
    zone_lines(w);
    for (int i = 0; i < n; i++) {
        if (t[i].big) hu_big(t[i].x, t[i].y, t[i].s, HU_BIG_FREE);
        else hu_text(t[i].x, t[i].y, t[i].s);
    }
}

static void race_icons(const world *w)
{
    int right = stand_right();
    for (int k = 0; k < RACERS; k++) {
        const car *c = &w->c[w->order[k]];
        spr(SPR_ICON, (right - 3) * 8 + hu_ui_x() + pan, (1 + k * 2) * 8 + (hud_on ? rs_screen_top() + hud_y : 0),
            racer_pal(c->colour), 3, 0);
    }
}

/* which car is k-th in the results (k = 0..3; ties share a place: in car order) */
static int result_car(const world *w, int k)
{
    int seen = 0;
    for (int p = 1; p <= RACERS; p++)
        for (int j = 0; j < RACERS; j++)
            if (w->final_place[j] == p) {
                if (seen == k) return j;
                seen++;
            }
    return k;
}

static void results_screen(const world *w, int st_t, const run_info *ri, int key)
{
    char s[64], nm[32];
    int out = w->humans == 1 && ri->out;        /* 1 player out: the note in the panel too (readable over any track) */
    int narrow = tu_on(), retry = 23;            /* zoomed: the note may take two lines */
    if (out) {
        char l[2][TU_LINE];
        retry = 25 + (narrow && tu_wrap(TR(OUT), 20, 0, l, 2) > 1 ? 1 + hu_tall(l[1]) : 0);
    }
    if (key != shown_key) {
        hu_clear();
        tu_banner(2, TR(RESULTS));
        track_title(w, s, sizeof s);
        if (narrow) tu_text(7, s, 0, 0, 0);
        else hu_text(hu_center(s, 0), 7, s);
        if (narrow) hu_panel(9, 9, 22, retry - 7);  /* (zoomed: 22 columns, the points right-aligned) */
        else hu_panel(6, 9, 28, out ? 18 : 16); /* the rows 3 apart, each with the row above it free (accents) */
        static const int rk[4] = {HS_RANK_1, HS_RANK_2, HS_RANK_3, HS_RANK_4};
        for (int k = 0; k < RACERS; k++) {
            int i = result_car(w, k), y = 11 + k * 3, p = w->final_place[i];
            hu_box_text(8 + 2 * narrow, y, hu_str(rk[p >= 1 && p <= 4 ? p - 1 : 3]));
            racer_name(w, i, nm, sizeof nm);
            hu_box_text(15 + narrow, y, nm);
            snprintf(s, sizeof s, "+%d", w->c[i].points);
            hu_box_text(narrow ? 27 - hu_width(s, 0) : 25, y, s);
            snprintf(s, sizeof s, "%d", ri->total[i]);
            hu_box_text(32 - 2 * narrow - hu_width(s, 0), y, s);
        }
        if (out) tu_text(23, TR(OUT), narrow ? 20 : 40, 1, 0);
        shown_key = key;
    }
    const char *line = ri->out || ri->race + 1 >= ri->races ? TR(GO_ON) : TR(NEXT);
    hu_retry_line_at(retry, st_t, RESULTS_LOCK, line);
}

static void results_icons(const world *w, int slide)
{
    for (int k = 0; k < RACERS; k++) {
        int i = result_car(w, k);
        int x = tu_on() ? 14 * 8 : 13 * 8 + 4;  /* (zoomed: the 22 columns' icon) */
        bug_at(x + hu_ui_x(), (11 + k * 3) * 8 + 4 + slide, 768, 0, racer_pal(w->c[i].colour), 3);
    }
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, const touch_view *tv,
                const run_info *ri)
{
    static const int medals[4] = MEDAL_SCORES;
    if (drawn_width != rs_screen_width()) maps_setup();
    if (loaded_track != w->track) load_track(w->track);
    fx_step(w);
    int t = (int)rs_frame_count();
    view v;
    v.x0 = w->camx / FX - ZONE_HW;
    v.y0 = w->camy / FX - ZONE_HH;
    v.sh_x = hu_shake_x();
    v.sh_y = hu_shake_y();
    stream_world(w, v.x0 - v.sh_x, v.y0 - v.sh_y);

    /* a zoomed tall screen (see hud_on): one player racing fills the display's height, panned after the ladybug; the
     * title, the results and the panels the same view (laid out in the columns shown: tall_ui.h); only 2-4 players
     * racing (the race zone: the whole stage) show every column */
    int solo = (state == ST_PLAY || state == ST_DEAD) && w->humans <= 1;
    rs_screen_fill(solo || !(state == ST_PLAY || state == ST_DEAD));
    hud_on = tall() && (state == ST_PLAY || state == ST_DEAD);
    hud_y = solo ? HUD_PIN_Y : HUD_PIN_FIT;
    pan = 0;
    int focus = -1;
    if (hud_on && solo) {
        int l, cw;
        view_cols(&l, &cw);
        int half = cw / 2, bx = w->c[0].x / FX - v.x0;
        focus = RS_SCREEN_W / 2;
        if (bx < focus - half + PAN_MARGIN) focus = bx + half - PAN_MARGIN;
        if (bx > focus + half - PAN_MARGIN) focus = bx - half + PAN_MARGIN;
        focus += rs_screen_origin_x();
    }
    rs_screen_focus_x(focus);
    if (hud_on && solo) {
        int l, cw;
        view_cols(&l, &cw);
        pan = l + rs_screen_origin_x() - (rs_screen_width() - cw) / 2;
    }

    /* the UI layer */
    hu_standing rs;
    int score[MAX_PLAYERS] = {0};               /* (the title has no players: hu_rank reads score[0] all the same) */
    for (int p = 0; p < w->humans && p < MAX_PLAYERS; p++) score[p] = ri->score[p];
    hu_rank(&rs, w->humans > 0 ? w->humans : 1, score, score);
    int slide = 0;
    if (state == ST_PLAY || state == ST_DEAD) {
        if (shown_key != ST_PLAY) {
            shown_key = ST_PLAY;
            shown_text = 0;
        }
        if (paused != text_paused) {        /* PAUSED takes rows of the UI layer: the texts are drawn again after */
            text_paused = paused;
            if (!paused) shown_text = 0;
        }
        if (!paused) race_texts(w, st_t, paused, tv, ri);
    } else if (state == ST_RESULTS) {
        results_screen(w, st_t, ri, 100 + ri->race);
        slide = hu_slide_in(st_t, 20, 200);
    } else {
        int key = state * 10 + (state == ST_OVER ? 1 : 0);
        if (key != shown_key || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best) {
            hu_clear();
            shown_text = 0;
            if (state == ST_TITLE) tu_logo("LADYBUG LAPS", title_ramps, 2, 2, 0);   /* (portrait: on two lines) */
            if (state == ST_OVER) {
                if (w->humans <= 1) {
                    char s[64];
                    tu_banner(4, HTR(GAME_OVER));
                    hu_gameover_panel(1, new_best, ri->score[0], 0);
                    if (ri->champion) snprintf(s, sizeof s, "%s", TR(CHAMPION));
                    else rs_strf(s, sizeof s, TR(REACHED), ri->race + 1, ri->races);
                    hu_gameover_note(s);
                } else {
                    tu_results_panel(&rs, NULL);
                }
            }
            shown_key = key;
            shown_best = best;
        }
        if (state == ST_TITLE && tu_on()) tu_title_draw(st_t, best, TR(VERB), MAX_PLAYERS, 0, 0);
        else if (state == ST_TITLE) hu_title_draw(st_t, best);
        if (state == ST_OVER) {
            if (w->humans <= 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
            else hu_retry_line_at(hu_results_retry_row(&rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
        }
        if (state == ST_OVER) slide = hu_slide_in(st_t, 20, 200);
    }
    rs_bg_scroll(RS_BG1, -pan, -slide);
    rs_bg_pin(RS_BG1, 0, hud_on ? HUD_LINES : 0, hud_y);
    tu_pause(paused, 13);

    /* the sprites, front to back */
    rs_oam_clear();
    if (state == ST_PLAY || state == ST_DEAD) {
        race_icons(w);
        if (tv->seen && state == ST_PLAY && w->humans >= 1) draw_touch(tv);
        draw_marks(w, &v, t);
    } else if (state == ST_RESULTS) {
        results_icons(w, slide);
    } else if (state == ST_OVER) {
        if (w->humans <= 1) {
            hu_gameover_sprites(1, ri->score[0], 0, best, hu_medal_of(ri->score[0], medals), st_t, slide);
        } else {
            hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                hu_results_icon_pos(&rs, i, &cx, &cy);
                bug_at(cx + hu_ui_x(), cy + slide, 768, 0, racer_pal(rs.order[i]), 3);
            }
        }
    } else {
        if (tu_on()) tu_title_sprites(st_t, MAX_PLAYERS, 0, slot_icon, NULL);
        else hu_title_sprites(st_t, slot_icon, NULL);
    }
    draw_leaves(w, &v);
    draw_cars(w, &v, t);
    draw_fx(&v);
    draw_shadows(w, &v, t);
}

/* ---- save states (main.c) ---- */
/* a state saved at another width brings maps of another size: set them up for this width, redraw the UI */
void draw_state_loaded(void)
{
    if (rs_bg_map_w(RS_BG1) == map_w() && rs_bg_map_h(RS_BG1) == map_h()) return;
    maps_setup();
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(fx); S(last_fx_t); S(loaded_track); S(shown_key); S(shown_best); S(text_paused); S(shown_text); S(msg_final);
    S(msg_drop); S(msg_drop_car);
}
#undef S
