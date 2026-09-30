/*
 * @NAME@: video. The house layout (docs/art-direction.md):
 *   BG1 the UI (the house kit, priority 1), BG2 the ground (front), BG3 the far hills (parallax 1/4),
 *   the backdrop a raster sky gradient; sprites: the heroes (OBJ 0, player 2 OBJ 1 = the palette swap),
 *   the props (OBJ 2), the kit's digits, glyphs and medals (OBJ 3).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/@ID@/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>

/* the title logo: one 3-shade ramp per word (light, mid, dark), house_style.py ACCENTS */
static const rs_color title_ramps[2][3] = {
    {RS_RGB8(255, 244, 150), RS_RGB8(252, 214, 64), RS_RGB8(226, 164, 28)},
    {RS_RGB8(255, 208, 226), RS_RGB8(246, 150, 190), RS_RGB8(214, 92, 146)}};
/* the sky: top and horizon colours of the raster gradient */
#define SKY_TOP     0x4898d8
#define SKY_HORIZON 0xc8e8f8

static rs_color line_col[RS_SCREEN_H];
static int shown_state = -1, shown_best = -1;

static void raster(int line, void *user)
{
    (void)user;
    rs_pal_set(0, line_col[line]);
}

static void sky_gradient(void)
{
    int a[3] = {(SKY_TOP >> 16) & 255, (SKY_TOP >> 8) & 255, SKY_TOP & 255};
    int b[3] = {(SKY_HORIZON >> 16) & 255, (SKY_HORIZON >> 8) & 255, SKY_HORIZON & 255};
    for (int y = 0; y < RS_SCREEN_H; y++) {
        int k = y < GROUND_Y ? y * 256 / GROUND_Y : 256, c[3];
        for (int i = 0; i < 3; i++) c[i] = (a[i] * (256 - k) + b[i] * k) / 256;
        /* house rule: a 2-line dither step every 8 lines keeps the RGB555 bands soft */
        line_col[y] = RS_RGB8(c[0] + ((y & 4) ? 4 : 0), c[1] + ((y & 4) ? 2 : 0), c[2] + ((y & 4) ? 4 : 0));
    }
}

void draw_init(void)
{
    hu_config hc = hu_defaults();       /* BG1 at VRAM 0, the kit at 0 on palette 0, the logo at 320 on palette 5 */
    hc.obj_tile = KIT_OBJ_TILE;         /* the kit sprites after the game's */
    hc.obj_vram = VR_OBJ;
    hc.obj_pal = 3;
    hu_init(&hc);
    rs_bg_setup(RS_BG1, 64, 32, VR_BG1);
    rs_bg_setup(RS_BG2, 64, 32, VR_BG2);
    rs_bg_setup(RS_BG3, 64, 32, VR_BG3);
    rs_tiles_load(VR_BG2, gm_bg2_tiles, gm_bg2_tile_count);
    rs_tiles_load(VR_BG3, gm_bg3_tiles, gm_bg3_tile_count);
    rs_pal_load(RS_PAL_BG(PAL_GROUND), gm_ground_pal, 16);
    rs_pal_load(RS_PAL_BG(PAL_HILLS), gm_hills_pal, 16);
    for (int y = 0; y < GROUND_H; y++)
        for (int x = 0; x < 64; x++) rs_bg_put(RS_BG2, x, GROUND_Y / 8 + y, gm_ground_map[y * 64 + x]);
    for (int y = 0; y < HILLS_H; y++)
        for (int x = 0; x < 64; x++) rs_bg_put(RS_BG3, x, HILLS_Y / 8 + y, gm_hills_map[y * 64 + x]);
    for (int l = 0; l < 3; l++) rs_bg_enable(l, 1);
    rs_obj_base(VR_OBJ);
    rs_tiles_load(VR_OBJ, gm_obj_tiles, gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0), gm_obj_pals, 48);          /* hero, hero 2, props (the kit loaded OBJ 3) */
    sky_gradient();
    rs_raster(raster, NULL);
}

static void spr(int id, int x, int y, int prio, int pal)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (x <= -(int)d->w || x >= RS_SCREEN_W || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, pal >= 0 ? pal : d->pal, prio, 0);
}

static int hero_sprite(const hero *h, int t)
{
    switch (h->state) {
    case HS_READY: return SPR_HERO_IDLE + (t / 32) % 2;
    case HS_HIT:
    case HS_DOWN: return SPR_HERO_HIT;
    default:
        /* house squash and stretch: squash on the take-off and the landing, stretch while rising */
        if (h->land_t < 5 || h->air_t < 3) return SPR_HERO_SQUASH;
        if (!hero_on_ground(h)) return h->vy < 0 ? SPR_HERO_STRETCH : SPR_HERO_AIR;
        return SPR_HERO_IDLE + (t / 8) % 2;
    }
}

static void draw_heroes(const world *w, int state, int st_t, int sx, int sy)
{
    for (int p = w->players - 1; p >= 0; p--) {
        const hero *h = &w->h[p];
        if (h->state == HS_OFF) continue;
        int y = (int)(h->y >> 16) - 32 + 2;
        if (h->state == HS_READY) y += hu_bob(st_t + p * 16, 64, READY_BOB) - READY_BOB;
        spr(hero_sprite(h, st_t), h->x - 16 + sx, y + sy, 2, p == 1 ? 1 : -1);
    }
    (void)state;
}

static void screen_text(const world *w, int state, int st_t, int best, int new_best)
{
    char s[48];
    if (state != shown_state || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best) {
        hu_clear();
        if (state == ST_TITLE) {
            hu_logo("@TITLE@", title_ramps, 2, 2, 0);
            snprintf(s, sizeof s, "BEST %d", best);
            if (best > 0) hu_text(hu_center(s, 0), 24, s);
            hu_copyright(28);
        }
        if (state == ST_READY) hu_get_ready(6);
        if (state == ST_OVER) {
            hu_banner(4, "GAME OVER");
            hu_gameover_panel(w->players, new_best, w->h[0].score, w->h[1].score);
        }
        shown_state = state;
        shown_best = best;
    }
    if (state == ST_TITLE || state == ST_READY) {
        hu_prompt(21, "PRESS A TO PLAY", st_t);
        if (state == ST_READY || w->players == 2) hu_join_line(26, w->players, "RACE!");
    }
    if (state == ST_OVER) hu_retry_line(st_t, RETRY_LOCK, "A: PLAY AGAIN");
}

void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused)
{
    static const int medals[4] = MEDAL_SCORES;
    int t = (int)rs_frame_count();
    int scroll = (int)w->scroll, sx = hu_shake_x(), sy = hu_shake_y();
    rs_bg_scroll(RS_BG2, (scroll - sx) & 511, -sy);
    rs_bg_scroll(RS_BG3, (scroll / 4 - sx) & 511, -sy);
    screen_text(w, state, st_t, best, new_best);
    int slide = state == ST_OVER ? hu_slide_in(st_t, 20, 200) : 0;     /* the panel slides up, ease-out */
    rs_bg_scroll(RS_BG1, 0, -slide);
    hu_pause(paused, 13);

    rs_oam_clear();
    /* front to back: the UI, the heroes, the props */
    if (state == ST_PLAY || state == ST_DEAD) {
        if (w->players == 1) hu_number(w->h[0].score, RS_SCREEN_W / 2, 10, 3);
        else {
            hu_number(w->h[0].score, 80, 10, 3);
            hu_number(w->h[1].score, 240, 10, 3);
        }
    }
    if (state == ST_OVER) {
        int m = w->players == 1 ? hu_medal_of(w->h[0].score, medals) : 0;
        hu_gameover_sprites(w->players, w->h[0].score, w->h[1].score, best, m, st_t, slide);
    }
    if (state == ST_TITLE || state == ST_READY)
        hu_glyph(HU_BTN_A, hu_center("PRESS A TO PLAY", 0) * 8 - 12, 21 * 8 - 4, (t / 30) % 2, 3);
    if (state == ST_TITLE) {
        /* the hero bobs under the logo */
        spr(SPR_HERO_IDLE + (st_t / 32) % 2, RS_SCREEN_W / 2 - 16, 96 + hu_bob(st_t, 64, READY_BOB), 2, -1);
    } else {
        draw_heroes(w, state, st_t, sx, sy);
    }
    for (int i = 0; i < w->nb; i++)
        spr(SPR_CRATE, w->bx[i] - scroll + sx, GROUND_Y - BLOCK_H + sy, 2, -1);
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("draw." #v, &(v), sizeof(v))
void draw_state(void)
{
    S(line_col); S(shown_state); S(shown_best);
    RS_STATE_RASTER(raster);
}
#undef S
