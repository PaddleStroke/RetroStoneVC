/*
 * Leady Squid: video (VRAM, layers, raster effects, sprites) and the cosmetic
 * effects (bubbles, ink puffs, lead weights). Nothing here changes the game.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
 */
#include "ls.h"
#include "assets.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>

/* BG1 tiles (relative to 0): the house UI kit (games/common/src/house_ui.c: the font, the font on the panel
 * colour, the 2x-glyph cache, the panel frame; HU_BG_TILES), then the logo at LOGO_TILE.. */

/* water gradient per theme: surface colour, deep colour */
static const uint32_t water[THEMES][2] = {
    {0x48a8c8, 0x104878}, {0x40b0c0, 0x125070}, {0x3480a8, 0x0e3460}, {0x2c5c96, 0x0c1e4c}};

static int16_t back_dx[RS_SCREEN_H];
static rs_color line_col[RS_SCREEN_H];
static int cur_rgb[2][3];                 /* the gradient shown (lerps toward the theme's) */
static int depth;                         /* 0..DEPTH_MAX */
static int drawn_index, cleared_col;
static uint32_t drawn_seed;               /* the course whose bodies are on BG2 */
static rs_rng fx_rng;
static int logo_on;
static int last_dark = -1;                /* the darkness the reef palettes were last written for */
/* the caps' OBJ palettes: two slots (6 + theme parity) hold the two themes a course window can show (a theme lasts
 * THEME_BAND obstacles = 720 px, more than the window): OBJ 4 and 5 are players 3 and 4 (the house rule) */
static int cap_theme[2] = {-1, -1};

int depth_level(void) { return depth; }

/* ---- raster: the water gradient, the additive rays band, the pause dim ----------------------------- */
static void raster(int line, void *user)
{
    (void)user;
    rs_pal_set(0, line_col[line]);
    if (line == 0) rs_math(RS_MATH_ADD, RS_MATH_BG4, 0);
    else if (line == RAYS_END) rs_math(RS_MATH_OFF, 0, 0);
}

static void gradient_update(int theme, int target_depth)
{
    for (int k = 0; k < 2; k++) {
        uint32_t c = water[theme][k];
        int tgt[3] = {(int)(c >> 16) & 255, (int)(c >> 8) & 255, (int)c & 255};
        for (int i = 0; i < 3; i++) {
            int d = tgt[i] * 16 - cur_rgb[k][i];
            cur_rgb[k][i] += d > 0 ? (d + 31) / 32 : d / 32;
        }
    }
    if (depth < target_depth) depth++;
    if (depth > target_depth) depth = target_depth;
    int dark = 256 - depth * 96 / DEPTH_MAX;       /* up to 37% darker */
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int k = y < PLAY_H ? y * 256 / PLAY_H : 256;
        int rgb[3];
        for (int i = 0; i < 3; i++) {
            int v = (cur_rgb[0][i] * (256 - k) + cur_rgb[1][i] * k) / 256 / 16;
            rgb[i] = v * dark / 256;
        }
        /* a 2-line dither step every 8 lines keeps the bands soft on RGB555 */
        line_col[y] = RS_RGB8(rgb[0] + ((y & 4) ? 4 : 0), rgb[1] + ((y & 4) ? 2 : 0), rgb[2] + ((y & 4) ? 4 : 0));
    }
    /* the rays, the reef and the mid-ground darken with the water */
    if (dark != last_dark) {
        last_dark = dark;
        for (int i = 1; i < 16; i++) {
            rs_color c = ls_back_pal[i], m = ls_mid_pal[i];
            rs_pal_set(RS_PAL_BG(PAL_BACK) + i, RS_RGB((c & 31) * dark / 256, ((c >> 5) & 31) * dark / 256,
                                                       ((c >> 10) & 31) * dark / 256));
            int dm = 256 - (256 - dark) * 3 / 4;
            rs_pal_set(RS_PAL_BG(PAL_MID) + i, RS_RGB((m & 31) * dm / 256, ((m >> 5) & 31) * dm / 256,
                                                      ((m >> 10) & 31) * dm / 256));
        }
    }
}

/* ---- set-up ---------------------------------------------------------------------------------------- */
void draw_init(void)
{
    /* the house UI kit on BG1 (palette 0); the digits, the A glyph and the medals are sprites of the game's
     * sheet (drawn by games/common/tools/house_style.py), so no kit sprites */
    hu_config hc = hu_defaults();
    hc.logo_tile = LOGO_TILE;
    hc.logo_pal = PAL_LOGO;
    hc.obj_tile = -1;
    hc.box_glyphs = " SCOREBESTNWMDALPYIG12!:-";    /* only the glyphs the panel uses (VRAM) */
    hu_init(&hc);
    rs_tiles_load(LOGO_TILE, ls_logo_tiles, ls_logo_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_SEABED), ls_seabed_pal, 16);
    for (int t = 0; t < THEMES; t++) rs_pal_load(RS_PAL_BG(PAL_THEME0 + t), ls_theme_bg_pal[t], 16);
    rs_pal_load(RS_PAL_BG(PAL_MID), ls_mid_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_BACK), ls_back_pal, 16);
    rs_pal_load(RS_PAL_OBJ(0), ls_obj_pals, 128);

    rs_bg_setup(RS_BG1, 64, 32, VR_BG1);
    rs_bg_setup(RS_BG2, 64, 32, VR_BG2);
    rs_bg_setup(RS_BG3, 64, 32, VR_BG3);
    rs_bg_setup(RS_BG4, 64, 32, VR_BG4);
    rs_tiles_load(VR_BG2, ls_bg2_tiles, ls_bg2_tile_count);
    rs_tiles_load(VR_BG3, ls_bg3_tiles, ls_bg3_tile_count);
    rs_tiles_load(VR_BG4, ls_bg4_tiles, ls_bg4_tile_count);
    for (int y = 0; y < SEABED_H; y++)
        for (int x = 0; x < SEABED_W; x++) rs_bg_put(RS_BG2, x, PLAY_H / 8 + y, ls_seabed_map[y * SEABED_W + x]);
    for (int y = 0; y < MID_H; y++)
        for (int x = 0; x < MID_W; x++) rs_bg_put(RS_BG3, x, MID_Y / 8 + y, ls_mid_map[y * MID_W + x]);
    for (int y = 0; y < BACK_H; y++)
        for (int x = 0; x < BACK_W; x++) rs_bg_put(RS_BG4, x, y, ls_back_map[y * BACK_W + x]);
    for (int l = 0; l < 4; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, ls_obj_tiles, ls_obj_tile_count);
    rs_bg_line_scroll(RS_BG4, back_dx, NULL);
    rs_raster(raster, NULL);
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < 3; i++) cur_rgb[k][i] = (int)((water[0][k] >> (16 - 8 * i)) & 255) * 16;
    rs_rng_seed(&fx_rng, 0xb0bb1e5u);
    gradient_update(0, 0);
    drawn_index = -1;
    cleared_col = 0;
    cap_theme[0] = cap_theme[1] = -1;
}

/* ---- the course on BG2 ------------------------------------------------------------------------------- */
static void put_column_part(const obstacle *o, int r0, int r1)
{
    int variant = (o->index * 7 + 3) % 2;
    const uint16_t *m = ls_body_map[o->theme] + variant * 6;
    int c0 = o->x / 8;
    for (int r = r0; r < r1; r++)
        for (int k = 0; k < 3; k++) rs_bg_put(RS_BG2, (c0 + k) & 63, r, m[(r & 1) * 3 + k]);
}

void draw_reset_course(const world *w)
{
    for (int y = 0; y < PLAY_H / 8; y++)
        for (int x = 0; x < 64; x++) rs_bg_put(RS_BG2, x, y, 0);
    drawn_index = w->first_index - 1;
    drawn_seed = w->seed;
    cleared_col = world_scroll_px(w) / 8 > 1 ? world_scroll_px(w) / 8 - 1 : 0;
}

static void course_tiles(const world *w)
{
    /* The course is drawn once per obstacle, but the first flap re-rolls it (a new seed from the
     * start frame, world_step): the tiles of the obstacle already drawn during "get ready" (just
     * off-screen) belonged to the old course, while its caps (sprites) follow the new gap. Redraw
     * whenever the course changes. */
    if (w->seed != drawn_seed) draw_reset_course(w);
    int sx = world_scroll_px(w);
    /* clear the columns that left the screen on the left */
    for (; cleared_col < sx / 8 - 1; cleared_col++)
        for (int y = 0; y < PLAY_H / 8; y++) rs_bg_put(RS_BG2, cleared_col & 63, y, 0);
    for (int i = 0; i < w->nob; i++) {
        const obstacle *o = &w->ob[i];
        if (o->index <= drawn_index || o->x > sx + RS_SCREEN_W + 64) continue;
        put_column_part(o, 0, o->gap_top / 8);                              /* top: surface .. gap */
        put_column_part(o, (o->gap_top + GAP + 7) / 8, PLAY_H / 8);          /* bottom: gap .. seabed */
        drawn_index = o->index;
    }
}

/* ---- cosmetic effects -------------------------------------------------------------------------------------- */
typedef struct fx_obj { int kind, t, life; int32_t x, y, vx, vy; int frame; } fx_obj;
enum { FX_NONE, FX_BUBBLE, FX_INK, FX_WEIGHT };
#define FX_MAX 48
static fx_obj fx[FX_MAX];

static fx_obj *fx_new(int kind)
{
    for (int i = 0; i < FX_MAX; i++)
        if (fx[i].kind == FX_NONE) {
            memset(&fx[i], 0, sizeof fx[i]);
            fx[i].kind = kind;
            return &fx[i];
        }
    return NULL;
}

static void bubble_at(int x, int y, int size)
{
    fx_obj *b = fx_new(FX_BUBBLE);
    if (!b) return;
    b->x = x << 8;
    b->y = y << 8;
    b->vy = -(80 + rs_rng_range(&fx_rng, 90) + size * 30);
    b->frame = size;
    b->life = 400;
    b->t = rs_rng_range(&fx_rng, 64);
}

void fx_flap(int x, int y)
{
    fx_obj *k = fx_new(FX_INK);
    if (k) {
        k->x = (x - 18) << 8;
        k->y = (y - 2) << 8;
        k->vy = -20;
        k->life = INK_LIFE;
    }
    bubble_at(x - 14, y - 2, 0);
    bubble_at(x - 16, y + 3, 1);
}

void fx_land(int x, int y)
{
    for (int i = 0; i < 3; i++) {
        fx_obj *wgt = fx_new(FX_WEIGHT);
        if (!wgt) return;
        wgt->x = (x - 4 + i * 4) << 8;
        wgt->y = (y - 4) << 8;
        wgt->vx = (i - 1) * 110;
        wgt->vy = -(260 + i * 50);
        wgt->life = 100000;
    }
    for (int i = 0; i < 4; i++) bubble_at(x - 8 + i * 5, y - 6, i & 1);
}

void fx_update(const world *w)
{
    int dx = world_running(w) ? 256 : 0;         /* the world's drift, 8.8 px per frame */
    for (int i = 0; i < FX_MAX; i++) {
        fx_obj *f = &fx[i];
        if (f->kind == FX_NONE) continue;
        f->t++;
        switch (f->kind) {
        case FX_BUBBLE:
            f->y += f->vy;
            f->x -= dx / 2;
            if ((f->y >> 8) < 2 || f->t > f->life) f->kind = FX_NONE;
            break;
        case FX_INK:
            f->x -= dx;
            f->y += f->vy;
            if (f->t >= f->life) f->kind = FX_NONE;
            break;
        case FX_WEIGHT:
            f->x += f->vx - dx;
            f->vy += 40;
            f->y += f->vy;
            if ((f->y >> 8) >= SEABED_Y + 2) {
                f->y = (SEABED_Y + 2) << 8;
                f->vy = f->vy > 200 ? -f->vy / 3 : 0;
                f->vx = f->vx * 2 / 3;
                f->frame = 0;
            } else {
                f->frame = 1;
            }
            if ((f->x >> 8) < -16) f->kind = FX_NONE;
            break;
        }
    }
    /* ambient bubbles rising from the seabed, now and then */
    if (rs_rng_range(&fx_rng, 100) < 4) bubble_at(8 + rs_rng_range(&fx_rng, 304), SEABED_Y + 4, rs_rng_range(&fx_rng, 3));
}

/* ---- sprites --------------------------------------------------------------------------------------------- */
static void spr(int id, int x, int y, int prio, int pal_override)
{
    const ls_sprite_def *d = &ls_spr[id];
    if (x <= -(int)d->w || x >= RS_SCREEN_W || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal_override >= 0 ? pal_override : d->pal, prio, 0);
}

static void number(int n, int cx, int y, int prio)
{
    char s[12];
    snprintf(s, sizeof s, "%d", n);
    int len = (int)strlen(s), x = cx - len * 6;
    for (int i = 0; i < len; i++) spr(SPR_DIGITS + (s[i] - '0'), x + i * 12 - 2, y, prio, -1);
}

static int squid_sprite(const squid *s, int t)
{
    switch (s->state) {
    case SQ_READY: return SPR_SQUID_IDLE + (t / 32) % 2;
    case SQ_HIT: return SPR_SQUID_HIT;
    case SQ_SINK: return SPR_SQUID_SINK + (t / 8) % 2;
    case SQ_REST: return SPR_SQUID_REST;
    default:
        if (s->flap_t < 9) return SPR_SQUID_FLAP + s->flap_t / 3;
        return SPR_SQUID_TILT + squid_tilt_frame(s->tilt);
    }
}

static void draw_squids(const world *w)
{
    for (int p = 0; p < w->players; p++) {
        const squid *s = &w->sq[p];
        if (s->state == SQ_OFF) continue;
        int cy = (int)((s->y + Q16_ONE / 2) >> 16);
        spr(squid_sprite(s, w->t), s->x - 16, cy - 16, 2, p ? hu_player_pal(p) : -1);   /* P2-P4: OBJ 1, 4, 5 */
    }
}

static void draw_caps(const world *w)
{
    int sx = world_scroll_px(w);
    for (int i = 0; i < w->nob; i++) {
        int th = w->ob[i].theme, k = th & 1;
        if (cap_theme[k] != th) {
            rs_pal_load(RS_PAL_OBJ(OBJ_CAPS + k), ls_cap_pals[th], 16);
            cap_theme[k] = th;
        }
    }
    for (int i = 0; i < w->nob; i++) {
        const obstacle *o = &w->ob[i];
        int x = o->x - sx;
        if (x <= -OBST_W || x >= RS_SCREEN_W) continue;
        spr(SPR_CAP_KELP + o->theme * 2, x, o->gap_top - CAP_H, 2, OBJ_CAPS + (o->theme & 1));
        spr(SPR_CAP_KELP + o->theme * 2 + 1, x, o->gap_top + GAP, 2, OBJ_CAPS + (o->theme & 1));
    }
}

static void draw_fx(int prio_front)
{
    for (int i = 0; i < FX_MAX; i++) {
        const fx_obj *f = &fx[i];
        int x = f->x >> 8, y = f->y >> 8;
        if (f->kind == FX_BUBBLE && !prio_front) {
            int wob = ((f->t / 8) % 4 == 1) - ((f->t / 8) % 4 == 3);
            spr(SPR_BUBBLE + f->frame, x + wob - 4, y - 4, 1, -1);
        } else if (f->kind == FX_INK && prio_front) {
            spr(SPR_INK + f->t * 4 / (f->life + 1), x - 8, y - 8, 2, -1);
        } else if (f->kind == FX_WEIGHT && prio_front) {
            spr(SPR_WEIGHT + f->frame, x - 4, y - 7, 2, -1);
        }
    }
}

/* ---- per frame ---------------------------------------------------------------------------------------------- */
static int shown_state = -1, shown_best = -1;

/* the race's ranking (2-4 players): the score, then who lasted longer */
static void standing(const world *w, hu_standing *s)
{
    int key[MAX_PLAYERS], val[MAX_PLAYERS];
    for (int p = 0; p < w->players; p++) {
        val[p] = w->sq[p].score;
        int out = w->out_t[p];                   /* the frame it was hit; still in: the best */
        key[p] = val[p] * 65536 + (out <= 0 ? 0xffff : out < 0xffff ? out : 0xfffe);
    }
    hu_rank(s, w->players, key, val);
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    if (state != shown_state || (state == DS_OVER && st_t == RETRY_LOCK) || best != shown_best) {
        hu_clear();
        logo_on = 0;
        if (state == DS_TITLE) {
            logo_on = 1;
            for (int y = 0; y < LOGO_H; y++)
                for (int x = 0; x < LOGO_W; x++) rs_bg_put(RS_BG1, 4 + x, 2 + y, ls_logo_map[y * LOGO_W + x]);
            rs_pal_load(RS_PAL_BG(PAL_LOGO), ls_logo_pal, 16);
        } else {
            rs_pal_load(RS_PAL_BG(PAL_LOGO), ls_theme_bg_pal[PAL_LOGO - PAL_THEME0], 16);
        }
        if ((state == DS_PLAY || state == DS_DEAD) && w->players >= 3) hu_score_tags(w->players, 0);
        if (state == DS_OVER) {
            if (w->players == 1) {
                /* the title on its own banner, as wide as the score panel below it: nothing shows through */
                hu_banner(4, "GAME OVER");
                hu_gameover_panel(1, new_best, w->sq[0].score, 0);
            } else {
                hu_results_panel(rs, NULL);           /* P3 WINS! and the ranking */
            }
        }
        shown_state = state;
        shown_best = best;
    }
    /* the title, every frame: the prompt (blinking), the player slots, BEST, the join line, the credits */
    if (state == DS_TITLE) hu_title_draw(st_t, best);
    if (state == DS_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, "A: SWIM AGAIN");
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, "A: SWIM AGAIN");
    }
}

static void number_at(int n, int x, int y, int align, int prio)
{
    char s[12];
    snprintf(s, sizeof s, "%d", n);
    int w = (int)strlen(s) * 12;
    number(n, align < 0 ? x + w / 2 : align > 0 ? x - w / 2 : x, y, prio);
}

/* the title's player slots: a small squid in each joined player's colours, popping in with a sparkle */
static void slot_icon(int p, int cx, int cy, int t, void *user)
{
    (void)user;
    int age = hu_player_age(p);
    if (age < 24) spr(SPR_SPARKLE + (age / 6) % 2, cx + 6, cy - 14, 3, -1);
    spr(SPR_SQUID_ICON, cx - 8, cy - 8 + hu_bob(t + p * 16, 64, 1), 2, p ? hu_player_pal(p) : -1);
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    int sx = world_scroll_px(w);
    int score = w->sq[0].score;
    for (int p = 1; p < w->players; p++)
        if (w->sq[p].score > score) score = w->sq[p].score;
    hu_standing rs;
    standing(w, &rs);
    gradient_update(world_theme_at(w), score < DEPTH_MAX ? score : DEPTH_MAX);
    course_tiles(w);
    rs_bg_scroll(RS_BG2, sx & 511, 0);
    rs_bg_scroll(RS_BG3, (sx / 2) & 511, 0);
    rs_bg_scroll(RS_BG4, 0, 0);
    int t = (int)rs_frame_count();
    for (int y = 0; y < RS_SCREEN_H; y++) {
        if (y < RAYS_END) {
            int sway = ((y + t / 3) & 63) < 32 ? ((y + t / 3) & 31) : 31 - ((y + t / 3) & 31);
            back_dx[y] = (int16_t)(((sx / 8) + (t / 12) + sway / 12 + (y < 6 ? (t / 4 + y * 3) % 5 : 0)) & 511);
        } else {
            back_dx[y] = (int16_t)((sx / 4) & 511);
        }
    }
    screen_text(w, state, st_t, best, new_best, &rs);
    /* the game-over panel slides up */
    int slide = state == DS_OVER ? hu_slide_in(st_t, 20, 200) : 0;
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);

    rs_oam_clear();
    /* front to back: UI, squids, ink and weights, caps, bubbles */
    if (state == DS_PLAY || state == DS_DEAD) {
        /* 1 player: centred; 2: x 80 and 240; 3-4: the corners (the kit's chips, our digits) */
        for (int p = 0; p < w->players; p++) {
            hu_chip ch = hu_score_chip(w->players, p, 0);
            number_at(w->sq[p].score, ch.x, ch.y, ch.align, 3);
        }
    }
    if (state == DS_OVER) {
        int oy = slide;
        if (w->players == 1) {
            number(w->sq[0].score, 25 * 8, 11 * 8 - 4 + oy, 3);
            number(best, 25 * 8, 14 * 8 - 4 + oy, 3);
            int m = medal_of(w->sq[0].score);
            if (m) {
                spr(SPR_MEDAL + m - 1, 22 * 8 - 4, 16 * 8 - 4 + oy, 3, -1);
                if ((st_t / 20) % 3 == 0) spr(SPR_SPARKLE + (st_t / 10) % 2, 22 * 8 + 12, 16 * 8 - 4 + oy, 3, -1);
            } else {
                hu_box_text(22, 17, "-");
            }
        } else {
            /* the ranking: the scores, the shells (1st gold, 2nd silver, 3rd bronze), the squids */
            for (int i = 0; i < rs.n; i++) {
                int y = hu_results_row(&rs, i) * 8 + oy, m = hu_results_medal(&rs, i), cx, cy;
                number(rs.value[rs.order[i]], 24 * 8, y - 4, 3);
                if (m) {
                    spr(SPR_MEDAL + m - 1, 27 * 8, y - 8, 3, -1);
                    if (rs.rank[i] == 0 && (st_t / 20) % 3 == 0) spr(SPR_SPARKLE + (st_t / 10) % 2, 27 * 8 + 18, y - 8, 3, -1);
                }
                hu_results_icon_pos(&rs, i, &cx, &cy);
                spr(SPR_SQUID_ICON, cx - 8, cy - 8 + oy, 3, rs.order[i] ? hu_player_pal(rs.order[i]) : -1);
            }
        }
    }
    if (state == DS_TITLE) {
        int gx, gy;
        if (hu_title_glyph(&gx, &gy) == HU_BTN_A) spr(SPR_HINT + ((t / 30) % 2), gx, gy, 3, -1);
        hu_title_sprites(st_t, slot_icon, NULL);
    }
    draw_squids(w);
    draw_fx(1);
    draw_caps(w);
    draw_fx(0);
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(back_dx); S(line_col); S(cur_rgb); S(depth); S(drawn_index); S(cleared_col); S(drawn_seed);
    S(fx_rng); S(logo_on); S(last_dark); S(fx); S(shown_state); S(shown_best); S(cap_theme);
    RS_STATE_RASTER(raster);
    hu_state();                 /* the house UI kit's objects (house_ui.*) */
}
#undef S
