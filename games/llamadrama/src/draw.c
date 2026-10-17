/*
 * Llama Drama: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1); BG2 the scenery, still: the Andes (a hazy range, snowy peaks, the green
 *   puna, stars) and the meadow, two palettes, each eased from the day to a sunset, the dusk and the night as the heat
 *   rises; the backdrop a raster sky gradient of the same time of day (the raster callback also sets the stars' colour
 *   per line: the sky's own by day, white by night).
 *   Sprites: the llamas (head, neck pieces, body: P1 OBJ 0; P2-P4 OBJ 1, 4, 5, the wool and the tassels recoloured),
 *   the hornets (OBJ 2), the kit's digits, glyphs and medals (OBJ 3), the spit, splats, bonk stars, aim dots and small
 *   digits (OBJ 6), the sun, the moon and the clouds (OBJ 7, recoloured by the time of day, behind the mountains).
 * Widescreen (docs/spec.md "Widescreen"; DESIGN.md): the stage's 320 columns are those of 320, a third of the extra
 * width shows behind the llamas (the rest of the herd) and two thirds ahead (more sky); the UI is centred.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

_Static_assert(HEAD_FRAMES == AIM_FRAMES, "make_art.py HEAD_ANGLES and game.h AIM_FRAMES differ");

/* the title logo: one 3-shade ramp per word (light, mid, dark): LLAMA in warm wool, DRAMA in tassel red */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 240, 210), RS_RGB8(236, 196, 140), RS_RGB8(196, 136, 84)},
    {RS_RGB8(255, 178, 170), RS_RGB8(240, 88, 108), RS_RGB8(176, 38, 78)}};
/* the sky at each time of day, top and horizon (0xRRGGBB): the day, the sunset, the dusk, the night */
static const int SKY[TIMES][2] = {{0x4a90dc, 0xc4e6f8}, {0x4a5cb0, 0xffb07a}, {0x23265c, 0xa05a8c},
                                  {0x080c28, 0x24325e}};
static const int draw_order[MAX_PLAYERS] = DRAW_ORDER;

static rs_color line_col[RS_SCREEN_H], star_col[RS_SCREEN_H];
static int shown_key = -1, shown_best = -1, sky_key = -1;
static int drawn_width;                        /* the width the maps and the UI were set up for */

static void raster(int line, void *user)
{
    (void)user;
    rs_pal_set(0, line_col[line]);
    rs_pal_set(RS_PAL_BG(PAL_MTN) + IDX_STAR, star_col[line]);
}

static int view_left(void) { return (rs_screen_width() - RS_SCREEN_W) * WIDE_LEFT_NUM / WIDE_LEFT_DEN; }
static int map_w(void) { return rs_screen_width() > 512 ? 128 : 64; }

static rs_color rgb_lerp(int a, int b, int k)
{
    int c[3];
    for (int i = 0; i < 3; i++) {
        int sh = 16 - 8 * i;
        c[i] = (((a >> sh) & 255) * (256 - k) + ((b >> sh) & 255) * k) >> 8;
    }
    return RS_RGB8(c[0], c[1], c[2]);
}

/* the sky, the scenery and the sky sprites at time `sky` (0..768: day, sunset, dusk, night) */
static void time_of_day(int sky)
{
    int key = sky / 4;
    if (key == sky_key) return;
    sky_key = key;
    int a = sky / 256, k = sky % 256, b = a < TIMES - 1 ? a + 1 : a;
    int top = (((SKY[a][0] >> 16) & 255) * (256 - k) + ((SKY[b][0] >> 16) & 255) * k) >> 8 << 16 |
              (((SKY[a][0] >> 8) & 255) * (256 - k) + ((SKY[b][0] >> 8) & 255) * k) >> 8 << 8 |
              ((SKY[a][0] & 255) * (256 - k) + (SKY[b][0] & 255) * k) >> 8;
    int hor = (((SKY[a][1] >> 16) & 255) * (256 - k) + ((SKY[b][1] >> 16) & 255) * k) >> 8 << 16 |
              (((SKY[a][1] >> 8) & 255) * (256 - k) + ((SKY[b][1] >> 8) & 255) * k) >> 8 << 8 |
              ((SKY[a][1] & 255) * (256 - k) + (SKY[b][1] & 255) * k) >> 8;
    int stars = sky < 400 ? 0 : sky > 640 ? 256 : (sky - 400) * 256 / 240;
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int f = y < 184 ? y * 256 / 184 : 256;
        int c = 0;
        for (int i = 0; i < 3; i++) {
            int sh = 16 - 8 * i, v = (((top >> sh) & 255) * (256 - f) + ((hor >> sh) & 255) * f) >> 8;
            int d = (y & 4) ? (i == 1 ? 2 : 4) : 0;  /* house rule: a 2-line dither step every 8 lines */
            v = v + d > 255 ? 255 : v + d;
            c |= v << sh;
        }
        line_col[y] = RS_HEX(c);
        star_col[y] = rgb_lerp(c, 0xfffff0, stars);
    }
    for (int i = 1; i < 16; i++) {
        rs_pal_set(RS_PAL_BG(PAL_MTN) + i, hu_lerp_color(gm_mountains_pals[a * 16 + i], gm_mountains_pals[b * 16 + i], k));
        rs_pal_set(RS_PAL_BG(PAL_MEADOW) + i, hu_lerp_color(gm_meadow_pals[a * 16 + i], gm_meadow_pals[b * 16 + i], k));
        rs_pal_set(RS_PAL_OBJ(PAL_SKY) + i, hu_lerp_color(gm_sky_pals[a * 16 + i], gm_sky_pals[b * 16 + i], k));
    }
}

/* ---- the maps ------------------------------------------------------------------------------------------------------
 * 64 tiles wide up to 512 columns, 128 beyond (the size is saved in states: draw_state_loaded sets them up again);
 * the scenery is a 512-px panorama that repeats seamlessly, still (stage x 0 is its column 0 at every width). */
static void maps_setup(void)
{
    int mw = map_w();
    rs_bg_setup(RS_BG1, mw, 32, VR_BG1);
    rs_bg_setup(RS_BG2, mw, 32, VR_BG2);
    rs_bg_fill(RS_BG2, 0);
    for (int x = 0; x < mw; x++) {
        for (int y = 0; y < MOUNTAINS_H; y++)
            rs_bg_put(RS_BG2, x, MOUNTAINS_ROW + y, gm_mountains_map[y * MOUNTAINS_W + x % MOUNTAINS_W]);
        for (int y = 0; y < MEADOW_H && MEADOW_ROW + y < 32; y++)
            rs_bg_put(RS_BG2, x, MEADOW_ROW + y, gm_meadow_map[y * MEADOW_W + x % MEADOW_W]);
    }
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
    rs_tiles_load(VR_BG2, gm_bg2_tiles, gm_bg2_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_MTN), gm_mountains_pals, 16);
    rs_pal_load(RS_PAL_BG(PAL_MEADOW), gm_meadow_pals, 16);
    maps_setup();
    rs_bg_enable(RS_BG1, 1);
    rs_bg_enable(RS_BG2, 1);
    rs_bg_enable(RS_BG3, 0);
    rs_bg_enable(RS_BG4, 0);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* the llama (P1), P2, the hornets */
    rs_pal_load(RS_PAL_OBJ(4), gm_obj_pals + 64, 64);     /* P3, P4, the props, the sky */
    sky_key = -1;
    time_of_day(0);
    rs_raster(raster, NULL);
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
static int px(int32_t q) { return (int)(q >> 16); }

static void small_number(int n, int cx, int y, int play_prio)
{
    char s[8];
    int len = snprintf(s, sizeof s, "%d", n);
    for (int i = 0; i < len; i++) spr(SPR_DIGIT + (s[i] - '0'), cx - len * 3 + i * 6 - 1, y, play_prio, -1, 0);
}

/* one llama: the head (its face), the neck's pieces, the body; aim: the aim drawn (the title's idle look, a droop) */
static void draw_llama(const world *w, int p, int aim, int face, int sx, int sy)
{
    const llama *l = &w->l[p];
    int pal = player_pal(p);
    int32_t qsx, qsy, qbx, qby, hx, hy;
    llama_neck(p, aim, &qsx, &qsy, &qbx, &qby);
    llama_head(p, aim, &hx, &hy);
    int x0 = px(qsx) + sx, y0 = px(qsy) + sy, x1 = px(qbx) + sx, y1 = px(qby) + sy;
    int head = face == 3 ? SPR_HEAD_HIT
               : (face == 1 ? SPR_HEAD_PUFF : face == 2 ? SPR_HEAD_SCARED : SPR_HEAD) + aim_frame(aim);
    spr(head, px(hx) - 16 + sx, px(hy) - 16 + sy, 2, pal, 0);
    if (face == 3 && l->state == LS_HIT) {             /* stung: stars circle the head */
        static const int ox[8] = {0, 5, 7, 5, 0, -5, -7, -5}, oy[8] = {-7, -5, 0, 5, 7, 5, 0, -5};
        for (int i = 0; i < 3; i++) {
            int k = (l->t / 3 + i * 3) & 7;
            spr(SPR_STAR + (i + l->t / 6) % 2, px(hx) - 4 + ox[k] * 2 + sx, px(hy) - 18 + oy[k] / 2 + sy, 2, -1, 0);
        }
    }
    /* the neck: pieces from the head down to the shoulder, leaning with it */
    int n = NECK_ANG0 + aim * NECK_K / 256, lean = (256 - n + 20) / 40;
    int frame = lean <= 0 ? SPR_NECK : lean == 1 ? SPR_NECK + 1 : SPR_NECK2;
    int len = l->state == LS_OFF ? 0 : (int)((qsy - qby) >> 16);
    int pieces = len / NECK_GAP + 1;
    if (pieces < 2) pieces = 2;
    for (int i = 1; i <= pieces; i++) {
        int x = x1 + (x0 - x1) * i / pieces, y = y1 + (y0 - y1) * i / pieces;
        spr(frame, x - 8, y - 12, 2, pal, 0);
    }
    spr(SPR_BODY, x0 - 50, y0 - 10, 2, pal, 0);
}

/* the faint aim cue: three dots along the spit's path */
static void aim_dots(int p, int aim, int sx, int sy, int play_prio)
{
    int32_t mx, my;
    llama_mouth(p, aim, &mx, &my);
    int32_t vx = (int32_t)(((int64_t)GOB_SPEED * icos1024(aim)) >> 14), vy = -(int32_t)(((int64_t)GOB_SPEED * isin1024(aim)) >> 14);
    for (int k = 1; k <= 3; k++) {
        int f = k * 3 + 1;
        int x = px(mx + vx * f), y = px(my + vy * f + GOB_GRAV * (f * (f - 1) / 2));
        spr(SPR_DOT, x - 4 + sx, y - 4 + sy, play_prio, -1, 0);
    }
}

static int llama_face(const llama *l)
{
    if (l->state == LS_HIT || l->state == LS_OUT) return 3;
    if (l->puff) return 1;
    if (l->scared) return 2;
    return 0;
}

/* the aim drawn: a stung llama droops its head and neck down */
static int aim_shown(const llama *l)
{
    if (l->state == LS_HIT || l->state == LS_OUT) {
        int k = l->state == LS_OUT ? 16 : l->t < 16 ? l->t : 16;
        return l->aim + (AIM_MIN - l->aim) * k / 16;
    }
    return l->aim;
}

/* the title: where llama p stands, moved (dx, dy) from its place in the herd, clear of the title's text and slots.
 * One llama: a little up the meadow (its hooves above the join line) and left (its ears under the slots, its chest
 * left of the prompt). 2-4: the herd waits at the edge, TITLE_SHIFT to the left; on a narrow screen each head stops
 * where it is still whole, and stays left of what is beside it (the logo, P1's slot, the prompt's glyph). */
static void title_place(const world *w, int p, int *dx, int *dy)
{
    int32_t hx, hy, qsx, qsy, qbx, qby;
    llama_head(p, AIM_START, &hx, &hy);
    llama_neck(p, AIM_START, &qsx, &qsy, &qbx, &qby);
    int left = px(hx) - TITLE_HEAD_L, right = px(hx) + TITLE_HEAD_R, ui = hu_ui_x(), gx = 0;
    int glyph = hu_title_glyph(&gx, NULL) >= 0 ? ui + gx - 2 : 1 << 20;     /* the prompt's glyph, UI x -> stage */
    int d;
    *dy = 0;
    if (w->players == 1) {
        d = -TITLE_SOLO_DX;
        if (px(qsx) + TITLE_CHEST_R + d > glyph) d = glyph - px(qsx) - TITLE_CHEST_R;   /* the chest */
        *dx = d;
        *dy = -TITLE_SOLO_LIFT;
        return;
    }
    d = -TITLE_SHIFT;
    if (left + d < rs_screen_left()) d = rs_screen_left() - left;          /* the head whole on the screen ... */
    int limit = p == 3 ? ui + TITLE_LOGO_X : p == 1 ? ui + TITLE_SLOT_X : p == 2 ? glyph : 1 << 20;
    if (right + d > limit) d = limit - right;                               /* ... and left of what is beside it */
    *dx = d;
}

static void draw_herd(const world *w, int state, int st_t, int sx, int sy, int play_prio)
{
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int p = draw_order[i];
        if (p >= w->players) continue;
        const llama *l = &w->l[p];
        int aim = aim_shown(l), face = llama_face(l), dx = 0, dy = 0;
        if (state == ST_TITLE) {                       /* the title: the herd looks around, up and down */
            aim = AIM_START + isin1024(st_t * 3 + p * 200) * 40 / 16384;
            face = 0;
            title_place(w, p, &dx, &dy);
        }
        if (l->state == LS_PLAY && state != ST_TITLE) aim_dots(p, aim, sx, sy, play_prio);
        draw_llama(w, p, aim, face, sx + dx, sy + dy);
    }
}

static void draw_hornet(const world *w, const hornet *h, int sx, int sy, int play_prio)
{
    int x = px(h->x) + sx, y = px(h->y) + sy, f = (w->t / 3 + (int)(h - w->hn)) % 2, fl = 0;
    if (h->flash && (h->flash / 2) % 2) return;        /* a big one hit blinks */
    if (h->pat == 255) fl = RS_SPR_HFLIP;               /* buzzing away to the right */
    if (h->pat == PAT_DART && h->aux_t == 1) {          /* a dart about to dash: it shakes, a warning */
        x += (w->t / 2) % 2 ? 1 : -1;
        if ((w->t / 6) % 2 == 0) spr(SPR_WARN, x - 4, y - 18, play_prio, -1, 0);
    }
    switch (h->kind) {
    case HK_DART: spr(SPR_DART + f, x - 8, y - 8, play_prio, -1, fl); break;
    case HK_BIG: spr(SPR_BIG + f, x - 16, y - 16, play_prio, -1, fl); break;
    default: spr(SPR_HORNET + f, x - 8, y - 8, play_prio, -1, fl); break;
    }
}

static void draw_play(const world *w, int sx, int sy, int play_prio)
{
    /* front to back: the points, the bonked hornets, the splats, the spit, the hornets */
    for (int i = 0; i < FX_MAX; i++) {
        const effect *e = &w->fx[i];
        if (e->on && e->kind == FX_POINTS) small_number(e->value, px(e->x) + sx, px(e->y) - e->t / 2 + sy, play_prio);
    }
    for (int i = 0; i < FX_MAX; i++) {
        const effect *e = &w->fx[i];
        if (!e->on) continue;
        int x = px(e->x) + sx, y = px(e->y) + sy;
        switch (e->kind) {
        case FX_FALL: {
            if (e->t > 120 && (e->t / 3) % 2) break;    /* it fades away, blinking */
            int big = e->sub == HK_BIG;
            int id = big ? SPR_BIG_BONK : e->sub == HK_DART ? SPR_DART_BONK : SPR_HORNET_BONK;
            int hw = big ? 16 : 8;
            spr(id, x - hw, y - hw, play_prio, -1, RS_SPR_VFLIP);
            if (e->t < 90)                              /* bonk: stars circle over it */
                for (int k = 0; k < 2; k++) {
                    int a = e->t * 12 + k * 512;
                    spr(SPR_STAR + (e->t / 6 + k) % 2, x - 4 + isin1024(a) * (hw + 2) / 16384,
                        y - hw - 6 + isin1024(a + 256) * 3 / 16384, play_prio, -1, 0);
                }
            break;
        }
        case FX_SPLAT:
        case FX_FIZZLE:
            spr(SPR_SPLAT + (e->t / 4 > 2 ? 2 : e->t / 4), x - 4, y - 4, play_prio, -1, 0);
            break;
        }
    }
    for (int i = 0; i < MAX_PLAYERS * GOBS_MAX; i++) {
        const gob *g = &w->gobs[i];
        if (g->on) spr(SPR_GOB + (g->t / 4) % 2, px(g->x) - 4 + sx, px(g->y) - 4 + sy, play_prio, -1, 0);
    }
    for (int i = 0; i < MAX_HORNETS; i++)
        if (w->hn[i].on) draw_hornet(w, &w->hn[i], sx, sy, play_prio);
}

/* the sun sets, the moon rises; clouds drift (behind the mountains: priority 0) */
static void draw_sky(int sky, int t)
{
    static const int cx[3] = {40, 250, 420}, cy[3] = {34, 58, 22}, ck[3] = {SPR_CLOUD, SPR_CLOUD2, SPR_CLOUD};
    for (int i = 0; i < 3; i++) {
        int x = ((cx[i] - t / (10 + i * 6)) % 640 + 640) % 640 - 160;
        spr(ck[i], x, cy[i], 0, -1, 0);
    }
    if (sky < 600) spr(SPR_SUN, 236 - 16, 30 + sky * 150 / 600 - 16, 0, -1, 0);
    if (sky > 520) spr(SPR_MOON, 270 - 8, 160 - (sky - 520) * 120 / 248 - 8, 0, -1, 0);
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
    for (int p = 0; p < w->players; p++) v[p] = w->l[p].score;
    hu_rank(s, w->players, v, v);
}

/* ---- the BG screens -------------------------------------------------------------------------------------------- */
static void text_centered(int row, const char *s, int big)
{
    if (big && hu_width(s, 1) > 38) big = 0;           /* a long translation: the small letters */
    if (big) hu_big(hu_center(s, 1), row, s, HU_BIG_FREE);
    else hu_text(hu_center(s, 0), row, s);
}

static const char *msg_text(int msg)
{
    switch (msg) {
    case MSG_DART: return TR(NEW_DART);
    case MSG_BIG: return TR(NEW_BIG);
    }
    return TR(NEW_SWARM);
}

static int screen_key(const world *w, int state, int st_t, int touch)
{
    int screen = state == ST_DEAD ? ST_PLAY : state;
    int key = screen * 100000;
    if (screen == ST_PLAY) {
        if (w->msg) key += 1000 + w->msg * 10;
        if (w->started && w->t < HINT_FRAMES && state == ST_PLAY) key += 100 + touch;
    }
    if (state == ST_OVER && st_t >= RETRY_LOCK) key += 1;
    return key;
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs, int touch)
{
    int key = screen_key(w, state, st_t, touch);
    if (key != shown_key || best != shown_best) {
        char s[64];
        hu_clear();
        if (state == ST_TITLE) hu_logo("LLAMA DRAMA", title_ramps, 2, 2, 0);
        if ((state == ST_PLAY || state == ST_DEAD) && w->players >= 3) hu_score_tags(w->players, 0);
        if (state == ST_PLAY || state == ST_DEAD) {
            if (w->msg) text_centered(5, msg_text(w->msg), 1);
            if (key % 1000 >= 100) text_centered(27, touch ? TR(HINT_TOUCH) : TR(HINT_PAD), 0);
        }
        if (state == ST_OVER) {
            if (w->players == 1) {
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->l[0].score, 0);
                rs_strf(s, sizeof s, TRN(POPPED_N, w->l[0].pops), w->l[0].pops);
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

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, int touch_hint)
{
    static const int medals[4] = MEDAL_SCORES;
    int sx = hu_shake_x(), sy = hu_shake_y(), t = state == ST_TITLE ? st_t : w->t;
    hu_standing rs;
    standing(w, &rs);
    time_of_day(state == ST_TITLE ? 0 : w->sky);
    int pm = map_w() * 8 - 1;
    rs_bg_scroll(RS_BG2, (-sx) & pm, (-sy) & 255);

    screen_text(w, state, st_t, best, new_best, &rs, touch_hint);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;     /* the panel slides up, ease-out */
    rs_bg_scroll(RS_BG1, -hu_ui_x(), -slide);          /* the UI centred on the screen (widescreen) */
    hu_pause(paused, 13);
    /* the hornets, the spit, their effects and the aim dots: in play over the text too (priority 3: the new hornets'
     * names, the hint, the corner tags never hide one); paused or under the game-over panel, behind the UI (2) */
    int play_prio = (state == ST_PLAY || state == ST_DEAD) && !paused ? 3 : 2;

    rs_oam_clear();
    /* front to back: the UI, the play, the herd, the sky */
    if (state == ST_PLAY || state == ST_DEAD) {
        int v[MAX_PLAYERS];
        for (int p = 0; p < w->players; p++) v[p] = w->l[p].score;
        hu_score_chips(w->players, v, 0);       /* 1: centred; 2: on the halves; 3-4: the corners */
    }
    if (state == ST_OVER) {
        if (w->players == 1) {
            hu_gameover_sprites(1, w->l[0].score, 0, best, hu_medal_of(w->l[0].score, medals), st_t, slide);
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
        /* a hornet buzzes about on the right, waiting */
        int hx = 250 + isin1024(st_t * 5) * 22 / 16384, hy = 112 + isin1024(st_t * 10) * 10 / 16384;
        spr(SPR_HORNET + (st_t / 3) % 2, hx - 8, hy - 8, 2, -1, icos1024(st_t * 5) > 0 ? 0 : RS_SPR_HFLIP);
    } else {
        draw_play(w, sx, sy, play_prio);
    }
    draw_herd(w, state, st_t, sx, sy, play_prio);
    draw_sky(state == ST_TITLE ? 0 : w->sky, t);
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
    S(line_col); S(star_col); S(shown_key); S(shown_best); S(sky_key); S(drawn_width);
    RS_STATE_RASTER(raster);
}
#undef S
