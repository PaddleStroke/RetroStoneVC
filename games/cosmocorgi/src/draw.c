/*
 * Cosmo Corgi: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1); BG2 the walls (the asteroid belt's rocks, the station's metal, the comets'
 *   ice: made from the world's ring of columns every frame, scrolling with the world); BG3 the nebula clouds and the
 *   planets (1/4 of the scroll; their palette changes with the section); BG4 the stars (two speeds by bands: 1/8 and
 *   1/16, by line scroll; two twinkle colours); the backdrop a raster gradient, the section's colours.
 *   Sprites: the corgis' rockets, pods, shields and shots (OBJ 0; players 2-4 OBJ 1, 4, 5: the paint and the fur
 *   recoloured), the robots and the boss (OBJ 2; the gold groups and the boss's hit flash OBJ 7), the kit's digits,
 *   glyphs and medals (OBJ 3), the capsules, pops, the robots' shots, the boss bar and small digits (OBJ 6). One boss's
 *   tiles at a time are loaded into its slot (BOSS_SLOT) when its section comes.
 * Widescreen (docs/spec.md "Widescreen"; DESIGN.md): the stage's 320 columns are those of 320, a third of the extra
 * width shows behind the corgis and two thirds ahead (the stage origin); the UI is centred on the screen.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

/* the title logo: one 3-shade ramp per word (light, mid, dark): COSMO in starry violet, CORGI in corgi orange */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(214, 206, 255), RS_RGB8(150, 130, 246), RS_RGB8(98, 76, 200)},
    {RS_RGB8(255, 222, 150), RS_RGB8(246, 156, 70), RS_RGB8(206, 98, 40)}};
/* the backdrop of each theme, top and bottom (0xRRGGBB): the pastel nebula, the asteroid belt, the station, the ice */
static const int SKY[4][2] = {{0x1a0c36, 0x4c2462}, {0x100c22, 0x3a2632}, {0x08122a, 0x163c50}, {0x0a1430, 0x2e5482}};
#define TITLE_Y 104

static rs_color line_col[RS_SCREEN_H];
static int16_t star_dx[RS_SCREEN_H];           /* BG4's line scroll: the near star bands */
static int shown_key = -1, shown_best = -1, sky_key = -1, star_key = -1;
static int boss_loaded = -1;                   /* the boss whose tiles are in the slot */
static int drawn_width;                        /* the width the maps and the UI were set up for */

static void raster(int line, void *user)
{
    (void)user;
    rs_pal_set(0, line_col[line]);
}

static int view_left(void) { return (rs_screen_width() - RS_SCREEN_W) * WIDE_LEFT_NUM / WIDE_LEFT_DEN; }
static int map_w(void) { return rs_screen_width() > 512 ? 128 : 64; }
static int fdiv8(int v) { return v >= 0 ? v / 8 : -((7 - v) / 8); }

/* the gradient of theme a blended into theme b by k (0..256) */
static void sky_gradient(int a, int b, int k)
{
    int key = a * 1000 + b * 100 + k / 16;
    if (key == sky_key) return;
    sky_key = key;
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int f = y * 256 / (RS_SCREEN_H - 1), c[3];
        for (int i = 0; i < 3; i++) {
            int sh = 16 - 8 * i;
            int ta = (SKY[a][0] >> sh) & 255, ba = (SKY[a][1] >> sh) & 255;
            int tb = (SKY[b][0] >> sh) & 255, bb = (SKY[b][1] >> sh) & 255;
            int ca = (ta * (256 - f) + ba * f) >> 8, cb = (tb * (256 - f) + bb * f) >> 8;
            c[i] = (ca * (256 - k) + cb * k) >> 8;
        }
        int d = (y & 4) ? 1 : 0;               /* house rule: a 2-line dither step every 8 lines (soft RGB555 bands) */
        line_col[y] = RS_RGB8(c[0] + 4 * d, c[1] + 2 * d, c[2] + 4 * d);
    }
}

static int tri(int t, int period)          /* 0..256..0 */
{
    t %= period;
    return t < period / 2 ? t * 512 / period : (period - t) * 512 / period;
}

/* ---- the maps ------------------------------------------------------------------------------------------------------
 * 64 tiles wide up to 512 columns, 128 beyond (the size is saved in states: draw_state_loaded sets them up again);
 * the clouds and the stars are 512-px panoramas that repeat. */
static void maps_setup(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG2, mw, 32, VR_BG2);
    rs_bg_setup(RS_BG3, mw, 32, VR_BG3);
    rs_bg_setup(RS_BG4, mw, 32, VR_BG4);
    for (int y = 0; y < CLOUDS_H; y++)
        for (int x = 0; x < mw; x++) rs_bg_put(RS_BG3, x, y, gm_clouds_map[y * CLOUDS_W + x % CLOUDS_W]);
    for (int y = 0; y < STARS_H; y++)
        for (int x = 0; x < mw; x++) rs_bg_put(RS_BG4, x, y, gm_stars_map[y * STARS_W + x % STARS_W]);
    rs_bg_line_scroll(RS_BG4, star_dx, NULL);
    shown_key = -1;
    drawn_width = rs_screen_width();
}

void draw_init(void)
{
    rs_screen_origin(view_left());      /* widescreen: a third of the extra width behind, two thirds ahead */
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;         /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    rs_tiles_load(VR_BG2, gm_walls_tiles, gm_walls_tile_count);
    rs_tiles_load(VR_BG3, gm_clouds_tiles, gm_clouds_tile_count);
    rs_tiles_load(VR_BG4, gm_stars_tiles, gm_stars_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_WALLS), gm_walls_pal0, 16);
    rs_pal_load(RS_PAL_BG(PAL_WALLS + 1), gm_walls_pal1, 16);
    rs_pal_load(RS_PAL_BG(PAL_WALLS + 2), gm_walls_pal2, 16);
    rs_pal_load(RS_PAL_BG(PAL_CLOUDS), gm_clouds_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_STARS), gm_stars_pal, 16);
    maps_setup();
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* the corgi (P1), P2, the robots */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 64);     /* P3, P4, the props, the gold robots */
    sky_gradient(0, 0, 0);
    rs_raster(raster, NULL);
}

/* ---- the walls: the visible columns of the world's ring, every frame -------------------------------------------- */
static uint16_t wall_entry(int theme, int top, int lopen, int ropen, int c, int ceil)
{
    int kind, flip = 0;
    if (top) {
        kind = lopen && ropen ? TK_TOP_LR : lopen ? TK_TOP_L : ropen ? TK_TOP_L : (c * 7 % 5 == 0 ? TK_TOP2 : TK_TOP);
        if (ropen && !lopen) flip = RS_MAP_HFLIP;
    } else {
        kind = lopen && ropen ? TK_SIDE_LR : lopen ? TK_SIDE_L : ropen ? TK_SIDE_L : ((c * 5 + 3) % 7 == 0 ? TK_FILL2
                                                                                                       : TK_FILL);
        if (ropen && !lopen) flip = RS_MAP_HFLIP;
    }
    if (ceil) flip |= RS_MAP_VFLIP;
    return (uint16_t)(gm_walls_map[(theme - 1) * TER_KINDS + kind] ^ flip);
}

static void walls_draw(const world *w, int scroll)
{
    int mw = map_w(), c0 = fdiv8(scroll + rs_screen_left()) - 1, c1 = fdiv8(scroll + rs_screen_right()) + 1;
    for (int c = c0; c <= c1; c++) {
        int mc = c & (mw - 1), th = ter_theme_at(w, c);
        int ce = ter_ceil_at(w, c), fl = ter_floor_at(w, c);
        int cl = ter_ceil_at(w, c - 1), cr = ter_ceil_at(w, c + 1), fll = ter_floor_at(w, c - 1), flr = ter_floor_at(w, c + 1);
        for (int r = 0; r < 32; r++) {
            uint16_t e = 0;
            if (th && r < ce) {                               /* the ceiling: row r from the top */
                e = wall_entry(th, r == ce - 1, r >= cl, r >= cr, c, 1);
            } else if (th && r < 30 && 29 - r < fl) {         /* the floor: row 29 - r from the bottom */
                int k = 29 - r;
                e = wall_entry(th, k == fl - 1, k >= fll, k >= flr, c, 0);
            } else if (th && r == 30 && fl) {                 /* the rows the shake may show */
                e = wall_entry(th, 0, 0, 0, c, 0);
            } else if (th && r == 31 && ce) {
                e = wall_entry(th, 0, 0, 0, c, 1);
            }
            rs_bg_put(RS_BG2, mc, r, e);
        }
    }
}

/* ---- sprites -------------------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal, int flags)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, flags);
}

/* player p's sprite palette: the house rule (P1 OBJ 0, P2 OBJ 1, P3 OBJ 4, P4 OBJ 5: house_ui.h hu_player_pal) */
static int player_pal(int p) { return p ? hu_player_pal(p) : 0; }

static void small_number(int n, int cx, int y)
{
    char s[8];
    int len = snprintf(s, sizeof s, "%d", n);
    for (int i = 0; i < len; i++) spr(SPR_DIGIT + (s[i] - '0'), cx - len * 3 + i * 6 - 1, y, 2, -1, 0);
}

static void draw_rocket(const world *w, int p, int sx, int sy)
{
    const hero *h = &w->h[p];
    if (h->state == SH_OUT || h->state == SH_OFF) return;
    int x = (h->x >> 16) + sx, y = (h->y >> 16) + sy, pal = player_pal(p);
    if (h->state == SH_HIT) {                         /* bonk: X eyes, tumbling away, stars circling */
        static const int ox[8] = {0, 5, 7, 5, 0, -5, -7, -5}, oy[8] = {-7, -5, 0, 5, 7, 5, 0, -5};
        if (h->t < HIT_FRAMES - 16 || (h->t / 2) % 2) spr(SPR_ROCKET + 4, x - 16, y - 14, 2, pal, 0);
        for (int i = 0; i < 3; i++) {
            int k = (h->t / 3 + i * 3) & 7;
            spr(SPR_SPARK + (i + h->t / 6) % 2, x - 4 + ox[k] * 2, y - 16 + oy[k] / 2, 2, -1, 0);
        }
        return;
    }
    if (h->invul && (h->invul / 4) % 2) return;       /* the shield popped: it blinks */
    for (int k = 0; k < h->pods; k++) {               /* the puppy pods follow its path */
        int px, py;
        if (pod_pos(w, p, k, &px, &py)) spr(SPR_POD + ((w->t / 16 + k) % 2), px - 8 + sx, py - 8 + sy, 2, pal, 0);
    }
    int f = h->tilt < 0 ? 2 : h->tilt > 0 ? 3 : (w->t / 4) % 2;
    if (h->shield) spr(SPR_SHIELD, x - 24, y - 17, 2, pal, 0);
    spr(SPR_ROCKET + f, x - 16, y - 14, 2, pal, 0);
}

/* the title's player slots: each joined player's icon, in its colours (hu_title_sprites calls it) */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    spr(SPR_ICON, cx - 8 + hu_ui_x(), cy - 8 + hu_bob(t + p * 16, 64, 1), 2, player_pal(p), 0);
}

/* 2-4 players: ranked by score */
static void standing(const world *w, hu_standing *s)
{
    int v[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) v[p] = w->h[p].score;
    hu_rank(s, w->players, v, v);
}

static int enemy_sprite(const world *w, const enemy *e, int *flags)
{
    int t = w->t;
    *flags = 0;
    switch (e->kind) {
    case EK_BUG: return SPR_BUG + (t / 8) % 2;
    case EK_DRONE: return SPR_DRONE + (t / 3) % 2;
    case EK_UFO: return SPR_UFO + (t / 10) % 2;
    case EK_DASHER: return SPR_DASHER + (e->pat == PAT_DASH && e->aux == 2);
    case EK_CARRIER: return SPR_CARRIER + (t / 3) % 2;
    case EK_TANK: return SPR_TANK + (t / 16) % 2;
    case EK_ROCK: return SPR_ROCK;
    case EK_COMET: return SPR_COMET;
    case EK_TURRET: {                                 /* it aims at the nearest corgi */
        int best = -1, bd = 1 << 30;
        for (int p = 0; p < w->players; p++) {
            const hero *h = &w->h[p];
            if (h->state != SH_PLAY) continue;
            int dx = (h->x - e->x) >> 16, dy = (h->y - e->y) >> 16, d = dx * dx + dy * dy;
            if (d < bd) { bd = d; best = p; }
        }
        int dx = best < 0 ? -1 : (w->h[best].x - e->x) >> 16, dy = best < 0 ? 0 : (w->h[best].y - e->y) >> 16;
        if (e->side) dy = -dy;                        /* a ceiling turret is V-flipped: "up" is down */
        if (dx > 0) { *flags |= RS_SPR_HFLIP; dx = -dx; }
        if (e->side) *flags |= RS_SPR_VFLIP;
        int ady = dy < 0 ? -dy : 0;                   /* how much the corgi is "above" it */
        return SPR_TURRET + (ady * 2 < -dx ? 0 : ady > -dx * 2 ? 2 : 1);
    }
    }
    return SPR_BUG;
}

static void draw_world(const world *w, int sx, int sy)
{
    /* the points and the warnings (front) */
    for (int i = 0; i < FX_MAX; i++) {
        const effect *e = &w->fxs[i];
        if (e->on && e->kind == FX_POINTS) small_number(e->value, e->x + sx, e->y - e->t / 4 + sy);
    }
    int warned = 0, warn_y[2] = {-99, -99};
    for (int i = 0; i < MAX_ENEMIES && warned < 2; i++) {
        const enemy *e = &w->en[i];
        if (!e->on || e->pat != PAT_BEHIND || e->t >= 0 || e->t < -70) continue;
        int y = (e->y >> 16), dup = 0;
        for (int k = 0; k < warned; k++) dup |= y > warn_y[k] - 24 && y < warn_y[k] + 24;
        if (dup) continue;
        warn_y[warned++] = y;
        if ((w->t / 8) % 2 == 0) spr(SPR_WARN + (w->t / 16) % 2, rs_screen_left() + 2 + sx, y - 8 + sy, 2, -1, 0);
    }
    /* the robots' shots: on top of the play, the clearest */
    for (int i = 0; i < MAX_EBULLETS; i++) {
        const ebullet *b = &w->eb[i];
        if (!b->on) continue;
        int x = (b->x >> 16) + sx, y = (b->y >> 16) + sy, f = (w->t / 6 + i) % 2;
        if (b->kind == EB_ICICLE) spr(SPR_ICICLE, x - 8, y - 4, 2, -1, 0);
        else spr((b->kind == EB_BIG ? SPR_BIG : SPR_PELLET) + f, x - 4, y - 4, 2, -1, 0);
    }
    /* the corgis (P1 in front: the lower OAM index is in front) */
    for (int p = 0; p < w->players; p++) draw_rocket(w, p, sx, sy);
    for (int i = 0; i < MAX_CAPS; i++) {
        const capsule *c = &w->caps[i];
        if (!c->on) continue;
        int x = (c->x >> 16) + sx, y = (c->y >> 16) + sy + hu_bob(c->t, 48, 2);
        spr(SPR_CAPSULE + c->kind, x - 8, y - 8, 2, -1, 0);
        if ((c->t / 10) % 3 == 0) spr(SPR_SPARK + (c->t / 5) % 2, x + 3, y - 10, 2, -1, 0);
    }
    /* the corgis' shots */
    for (int i = 0; i < MAX_PLAYERS * SHOTS_PER_PLAYER; i++) {
        const shot *s = &w->shots[i];
        if (!s->on) continue;
        int x = (s->x >> 16) + sx, y = (s->y >> 16) + sy, pal = player_pal(s->owner), left = SHOT_RANGE - (s->x >> 16);
        if (left < 16) {                              /* the end of its reach (widescreen): a tiny star, then dust */
            spr(SPR_FADE + (left < 8), x - 4, y - 4, 2, pal, 0);
            continue;
        }
        switch (s->kind) {
        case SK_BARK: spr(SPR_BARK + (w->t / 4 + i) % 2, x - 4, y - 4, 2, pal, 0); break;
        case SK_SPREAD: spr(SPR_SPREAD, x - 4, y - 4, 2, pal, 0); break;
        case SK_BEAM: spr(SPR_BEAM + (w->t / 3) % 2, x - 8, y - 4, 2, pal, 0); break;
        case SK_BONE: spr(SPR_BONE + ((w->t / 4 + i) & 3), x - 4, y - 4, 2, pal, 0); break;
        }
    }
    /* the pops */
    for (int i = 0; i < FX_MAX; i++) {
        const effect *e = &w->fxs[i];
        if (!e->on) continue;
        switch (e->kind) {
        case FX_POP:
            spr(SPR_POP + e->t / 6, e->x - 8 + sx, e->y - 8 + sy, 2, -1, 0);
            break;
        case FX_FIZZLE:
            /* (a shot at its reach has already faded into dust: no fizzle, no dotted line down the screen) */
            if (e->value != 0 || e->x != SHOT_RANGE) spr(SPR_FIZZLE + e->t / 4, e->x - 4 + sx, e->y - 4 + sy, 2, -1, 0);
            break;
        case FX_SPARK:
            spr(SPR_SPARK + (e->t / 6) % 2, e->x - 4 + sx, e->y - 4 - e->t / 3 + sy, 2, -1, 0);
            break;
        case FX_SHIELD: {                             /* the shield pops: a ring of sparkles */
            static const int dx[6] = {-14, -8, 6, 14, 6, -8}, dy[6] = {0, -10, -10, 0, 10, 10};
            for (int k = 0; k < 6; k++)
                spr(SPR_SPARK + (k + e->t / 4) % 2, e->x - 4 + dx[k] * (8 + e->t) / 12 + sx,
                    e->y - 4 + dy[k] * (8 + e->t) / 12 + sy, 2, -1, 0);
            break;
        }
        case FX_BIGPOP: {                             /* the boss: a ring of bursts, sparkles and bolts flying */
            static const int dx[8] = {-1, 1, 1, -1, 0, 2, 0, -2}, dy[8] = {-1, -1, 1, 1, -2, 0, 2, 0};
            int r = 8 + e->t / 2;
            if (e->t < 36)
                for (int k = 0; k < 8; k++)
                    spr(SPR_POP + (e->t / 12) % 3, e->x - 8 + dx[k] * r + sx, e->y - 8 + dy[k] * r + sy, 2, -1, 0);
            for (int k = 0; k < 6 && e->t < 48; k++)
                spr((k & 1 ? SPR_BOLT : SPR_SPARK) + (k + e->t / 5) % 2, e->x - 4 + ((k * 37 + e->t * 3) % 64) - 32 + sx,
                    e->y - 4 + ((k * 23) % 32) - 16 + e->t / 2 + sy, 2, -1, 0);
            break;
        }
        }
    }
    /* the robots */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const enemy *e = &w->en[i];
        if (!e->on) continue;
        int fl, id = enemy_sprite(w, e, &fl);
        const gm_sprite_def *d = &gm_spr[id];
        int x = (e->x >> 16) + sx, y = (e->y >> 16) + sy;
        if (e->pat == PAT_DASH && e->aux == 1) x += ((w->t / 2) % 2) * 2 - 1;      /* winding up: it shakes */
        int pal = e->group || (e->flash & 2) ? PAL_GOLD : -1;
        spr(id, x - d->w / 2, y - d->h / 2, 2, pal, fl);
    }
    /* the boss */
    const boss *b = &w->b;
    if (b->on) {
        int x = (b->x >> 16) + sx, y = (b->y >> 16) + sy, f = (b->t / 20) % 2;
        if (b->phase == BP_DOWN) {
            x += ((b->t / 2) % 2) * 2 - 1;
            if ((b->t / 3) % 2 == 0) f = 0;
        }
        int pal = (b->flash & 2) || (b->phase == BP_DOWN && (b->t / 4) % 2) ? PAL_GOLD : -1;
        spr(SPR_BOSS + f, x - 32, y - 24, 2, pal, 0);
    }
}

/* the boss's strength left: 16 segments at the bottom centre */
static void boss_bar(const world *w)
{
    const boss *b = &w->b;
    if (!b->on || (b->phase != BP_FIGHT && b->phase != BP_ENTER)) return;
    int segs = 16, full = b->hp * segs * 2 / (b->hp0 ? b->hp0 : 1), x0 = 160 - segs * 4 + hu_ui_x();
    for (int k = 0; k < segs; k++) {
        int f = full >= 2 * k + 2 ? 0 : full == 2 * k + 1 ? 1 : 2;
        spr(SPR_HP + f, x0 + k * 8, 226, 1, -1, 0);
    }
}

/* ---- the BG screens -------------------------------------------------------------------------------------------- */
static void text_centered(int row, const char *s, int big)
{
    if (big && hu_width(s, 1) > 38) big = 0;           /* a long translation: the small letters */
    if (big) hu_big(hu_center(s, 1), row, s, HU_BIG_FREE);
    else hu_text(hu_center(s, 0), row, s);
}

static const char *section_name(int sec)
{
    switch (world_theme(sec)) {
    case THEME_ROCK: return TR(SEC_ASTEROIDS);
    case THEME_STATION: return TR(SEC_STATION);
    case THEME_ICE: return TR(SEC_COMETS);
    }
    return TR(SEC_NEBULA);
}

static const char *msg_text(int msg)
{
    switch (msg) {
    case MSG_SPREAD: return TR(POW_SPREAD);
    case MSG_BEAM: return TR(POW_BEAM);
    case MSG_HOMING: return TR(POW_HOMING);
    case MSG_SPEED: return TR(POW_SPEED);
    case MSG_SHIELD: return TR(POW_SHIELD);
    case MSG_POD: return TR(POW_POD);
    }
    return TR(POW_MAX);
}

static int screen_key(const world *w, int state, int st_t)
{
    int screen = state == ST_DEAD ? ST_PLAY : state;
    int key = screen * 100000;
    if (screen == ST_PLAY) {
        if (w->phase == PH_INTRO && w->phase_t < INTRO_FRAMES) key += 10000 + w->sec * 100 + w->loop;
        if (w->phase == PH_WARN) key += 20000;
        if (w->phase == PH_CLEAR && w->phase_t < CLEAR_FRAMES - 30) key += 30000 + w->boss_away;
        if (w->msg) key += 1000 + w->msg * 10 + w->msg_player;
    }
    if (state == ST_OVER && st_t >= RETRY_LOCK) key += 1;
    return key;
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    int key = screen_key(w, state, st_t);
    if (key != shown_key || best != shown_best) {
        char s[64];
        hu_clear();
        if (state == ST_TITLE) hu_logo("COSMO CORGI", title_ramps, 2, 2, 0);
        if ((state == ST_PLAY || state == ST_DEAD) && w->players >= 3) hu_score_tags(w->players, 0);
        if (state == ST_PLAY || state == ST_DEAD) {
            if (w->phase == PH_INTRO && w->phase_t < INTRO_FRAMES) {
                text_centered(11, section_name(w->sec), 1);
                if (w->loop) {
                    rs_strf(s, sizeof s, TR(LOOP_N), w->loop + 1);
                    text_centered(14, s, 0);
                }
            } else if (w->phase == PH_WARN) {
                text_centered(12, TR(BOSS), 1);
            } else if (w->phase == PH_CLEAR && w->phase_t < CLEAR_FRAMES - 30) {
                text_centered(12, w->boss_away ? TR(BOSS_AWAY) : TR(BOSS_DOWN), 1);
            }
            if (w->msg) {
                const char *m = msg_text(w->msg);
                if (w->players > 1) {
                    char tag[16];
                    rs_strf(tag, sizeof tag, HTR(PLAYER_TAG), w->msg_player + 1);
                    rs_strf(s, sizeof s, "%s %s", tag, m);
                    m = s;
                }
                text_centered(19, m, 0);
            }
        }
        if (state == ST_OVER) {
            if (w->players == 1) {
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->h[0].score, 0);
                rs_strf(s, sizeof s, TR(STAGE_N), world_stage(w));
                hu_gameover_note(s);
            } else {
                hu_results_panel(rs, NULL);            /* P2 WINS! and the ranking */
            }
        }
        shown_key = key;
        shown_best = best;
    }
    if (state == ST_TITLE) hu_title_draw(st_t, best);
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
}

/* the boss of this section in its tile slot */
static void boss_tiles(int kind)
{
    if (kind == boss_loaded) return;
    rs_tiles_load(VR_OBJ + BOSS_SLOT, gm_boss_tiles + kind * BOSS_TILES * 32, BOSS_TILES);
    boss_loaded = kind;
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    static const int medals[4] = MEDAL_SCORES;
    int sx = hu_shake_x(), sy = hu_shake_y(), t = state == ST_TITLE ? st_t : w->t;
    int scroll = state == ST_TITLE ? st_t * 2 : (int)(w->scroll >> 16);
    hu_standing rs;
    standing(w, &rs);

    /* the section's colours: blended in from the last section's as it begins */
    int th = state == ST_TITLE ? 0 : world_theme(w->sec), prev = th, k = 256;
    if (state != ST_TITLE && w->phase == PH_INTRO && (w->sec || w->loop) && w->phase_t < 64) {
        prev = world_theme(w->sec + SECTIONS - 1);
        k = w->phase_t * 4;
    }
    sky_gradient(prev, th, k);
    for (int i = 1; i < 16; i++)
        rs_pal_set(RS_PAL_BG(PAL_CLOUDS) + i, hu_lerp_color(gm_clouds_pal[prev * 16 + i], gm_clouds_pal[th * 16 + i], k));
    /* the stars twinkle; the near bands scroll twice as fast (1/8 against 1/16) */
    int ka = 120 + tri(t, 96) / 2, kb = 120 + tri(t + 48, 96) / 2;
    rs_pal_set(RS_PAL_BG(PAL_STARS) + IDX_STAR_A, hu_lerp_color(RS_RGB8(60, 64, 120), RS_RGB8(255, 252, 220), ka));
    rs_pal_set(RS_PAL_BG(PAL_STARS) + IDX_STAR_B, hu_lerp_color(RS_RGB8(60, 64, 120), RS_RGB8(210, 226, 255), kb));
    int pm = map_w() * 8 - 1, near = scroll / 8 - scroll / 16;
    if (near != star_key) {
        star_key = near;
        for (int y = 0; y < RS_SCREEN_H; y++) star_dx[y] = (int16_t)((y / 24) % 2 ? near : 0);
    }
    rs_bg_scroll(RS_BG4, (scroll / 16 - sx) & pm, (-sy) & 255);
    rs_bg_scroll(RS_BG3, (scroll / 4 - sx) & pm, (-sy) & 255);
    rs_bg_scroll(RS_BG2, (scroll - sx) & pm, (-sy) & 255);
    if (state == ST_TITLE) {
        for (int c = 0; c < map_w(); c++)
            for (int r = 0; r < 32; r++) rs_bg_put(RS_BG2, c, r, 0);
    } else {
        walls_draw(w, scroll);
    }
    if (state != ST_TITLE) boss_tiles(world_theme(w->sec));

    screen_text(w, state, st_t, best, new_best, &rs);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;     /* the panel slides up, ease-out */
    rs_bg_scroll(RS_BG1, -hu_ui_x(), -slide);          /* the UI centred on the screen (widescreen) */
    hu_pause(paused, 13);

    rs_oam_clear();
    /* front to back: the UI, the play */
    if (state == ST_PLAY || state == ST_DEAD) {
        int v[MAX_PLAYERS];
        for (int p = 0; p < w->players; p++) v[p] = w->h[p].score;
        hu_score_chips(w->players, v, 0);       /* 1: centred; 2: on the halves; 3-4: the corners */
        boss_bar(w);
    }
    if (state == ST_OVER) {
        if (w->players == 1) {
            hu_gameover_sprites(1, w->h[0].score, 0, best, hu_medal_of(w->h[0].score, medals), st_t, slide);
        } else {
            hu_results_sprites(&rs, st_t, slide);
            for (int i = 0; i < rs.n; i++) {
                int cx, cy;
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_ICON, cx - 8 + hu_ui_x(), cy - 8 + slide, 3, player_pal(rs.order[i]), 0);
            }
        }
    }
    if (state == ST_TITLE) {
        hu_title_sprites(st_t, slot_icon, NULL);   /* the prompt's glyph, the joined players popping in */
        /* the corgi's rocket bobs under the logo, a puppy pod behind it, a bark flying ahead */
        int b = hu_bob(st_t, 64, READY_BOB), x = 160 + hu_ui_x();
        spr(SPR_ROCKET + (st_t / 4) % 2, x - 16, TITLE_Y - 14 + b, 2, 0, 0);
        spr(SPR_POD + (st_t / 16) % 2, x - 44, TITLE_Y - 8 + hu_bob(st_t + 20, 64, READY_BOB), 2, 0, 0);
        spr(SPR_BARK + (st_t / 4) % 2, x + 18 + (st_t % 34) * 2, TITLE_Y - 4 + b, 2, 0, 0);     /* (up to the bug) */
        spr(SPR_BUG + (st_t / 8) % 2, x + 92, TITLE_Y - 8 + hu_bob(st_t + 40, 64, 6), 2, -1, 0);
        /* the logo's extra: stars twinkling round it, each in its turn */
        static const int tw[4][2] = {{-130, 58}, {128, 14}, {-44, 6}, {56, 78}};
        for (int k = 0; k < 4; k++) {
            int ph = (st_t + k * 29) % 96;
            if (ph < 40) spr(SPR_SPARK + (ph < 8 || ph >= 32), x + tw[k][0] - 4, tw[k][1] - 4, 2, -1, 0);
        }
    } else if (state != ST_OVER) {                 /* the panels show over the sky alone */
        draw_world(w, sx, sy);
    }
}

/* ---- save states (main.c) ---- */
/* a state saved at another width brings maps of another size: set them up for this width, redraw the UI */
void draw_state_loaded(void)
{
    rs_screen_origin(view_left());              /* (not part of a state: this width's) */
    if (drawn_width == rs_screen_width()) return;
    maps_setup();                               /* and the UI is drawn again (shown_key: the corners' tags) */
}

#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(line_col); S(star_dx); S(shown_key); S(shown_best); S(sky_key); S(star_key); S(boss_loaded); S(drawn_width);
    RS_STATE_RASTER(raster);
}
#undef S
