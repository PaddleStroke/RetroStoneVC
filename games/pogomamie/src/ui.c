/*
 * Pogo Mamie: the screens' text and the HUD, on the house UI kit (games/common/src/house_ui.c, the look of
 * Leady Squid: docs/art-direction.md): the logo, PRESS A with the A glyph, GET READY, the 2P join line, the
 * distance in the kit's big digits, pause, the game-over banner and panel sliding up, the medals.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 */
#include "pm.h"
#include "assets.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>

static int shown_state = -1, shown_best = -1, shown_players, shown_score = -1, shown_chain = -1;

void ui_init(void)
{
    hu_config hc = hu_defaults();
    hc.logo_tile = LOGO_TILE;
    hc.logo_pal = PAL_LOGO;
    hc.obj_vram = VR_OBJ;
    hc.obj_tile = pm_obj_tile_count;           /* the kit's sprites after ours */
    hc.obj_pal = 3;
    hc.box_glyphs = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!:-";
    hu_init(&hc);
    shown_state = -1;
}

static const char *const medal_name[5] = {"NO MEDAL YET", "BRONZE WHISKERS", "SILVER WHISKERS", "GOLD WHISKERS",
                                          "CAT CAUGHT!"};

/* ---- BG1 --------------------------------------------------------------------------------------------------------- */
void ui_frame(const world *w, int state, int st_t, int best_m, int best_score, int new_best, int paused)
{
    char s[48];
    const mamie *m = &w->m[0];
    if (state != shown_state || best_m != shown_best || w->players != shown_players) {
        hu_clear();
        shown_score = shown_chain = -1;
        if (state == DS_TITLE) {
            hu_logo("POGO MAMIE", pm_logo_ramps, 2, 2, 0);
            if (best_m > 0) {
                snprintf(s, sizeof s, "BEST %d M   SCORE %d", best_m, best_score);
                hu_text(hu_center(s, 0), 24, s);
            }
            hu_copyright(28);
        }
        if (state == DS_READY) {
            hu_get_ready(6);
            hu_text(hu_center("LEFT/RIGHT STEER - HOLD A: BIG BOUNCE", 0), 9, "LEFT/RIGHT STEER - HOLD A: BIG BOUNCE");
        }
        if (state == DS_OVER) {
            hu_banner(4, "GAME OVER");
            hu_panel(10, 9, 20, 12);
            if (w->players == 1) {
                hu_box_text(12, 11, "DISTANCE");
                hu_box_text(12, 14, "SCORE");
                snprintf(s, sizeof s, "%d", m->score);
                hu_box_text(28 - (int)strlen(s), 14, s);
                hu_box_text(12, 16, new_best ? "NEW BEST!" : "BEST");
                snprintf(s, sizeof s, "%d M", best_m);
                hu_box_text(28 - (int)strlen(s), 16, s);
                hu_box_text(12, 18, medal_name[medal_of(m->dist_m)]);
            } else {
                hu_box_text(12, 11, "MAMIE");
                hu_box_text(12, 14, "PAPI");
                int a = w->m[0].dist_m, b = w->m[1].dist_m;
                const char *win = a > b ? "MAMIE WINS!" : b > a ? "PAPI WINS!" : "DRAW!";
                hu_box_text(20 - (int)strlen(win) / 2, 17, win);
            }
        }
        shown_state = state;
        shown_best = best_m;
        shown_players = w->players;
    }
    if (state == DS_TITLE || state == DS_READY) {
        hu_prompt(21, "PRESS A TO BOUNCE", st_t);
        if (w->players == 2 || state == DS_READY) hu_join_line(26, w->players, "RACE!");
    }
    if ((state == DS_PLAY || state == DS_FALL) && w->players == 1 && (m->score != shown_score || m->chain != shown_chain)) {
        if (m->chain > 1) snprintf(s, sizeof s, "SCORE %d  X%d", m->score, m->chain);
        else snprintf(s, sizeof s, "SCORE %d", m->score);
        hu_clear_rows(4, 1);
        hu_text(hu_center(s, 0), 4, s);
        shown_score = m->score;
        shown_chain = m->chain;
    }
    if (state == DS_OVER) hu_retry_line(st_t, RETRY_LOCK, "A: BOUNCE AGAIN");
    hu_pause(paused, 13);
    rs_bg_scroll(RS_BG1, 0, state == DS_OVER ? -hu_slide_in(st_t, 20, 200) : 0);
}

/* ---- sprites: the distance, power-ups, the medal, the A glyph ------------------------------------------------------ */
static void spr(int id, int x, int y)
{
    const pm_sprite_def *d = &pm_spr[id];
    if (x <= -(int)d->w || x >= RS_SCREEN_W || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, d->pal, 3, 0);
}

/* the distance in the kit's big digits, centred on cx, and the m */
static void distance(int n, int cx, int y)
{
    char s[12];
    snprintf(s, sizeof s, "%d", n);
    int len = (int)strlen(s);
    hu_number(n, cx - 7, y, 3);
    spr(SPR_UNIT_M, cx - 7 + len * 6 - 2, y);
}

static void powerups(const mamie *m, int x, int y)
{
    int k = 0;
    if (m->umbrella_t > 0 && (m->umbrella_t > 60 || (m->umbrella_t / 4) % 2)) spr(SPR_ICON + IT_UMBRELLA, x + 10 * k++, y);
    if (m->croissant_t > 0 && (m->croissant_t > 60 || (m->croissant_t / 4) % 2)) spr(SPR_ICON + IT_CROISSANT, x + 10 * k++, y);
    if (m->yarn) spr(SPR_ICON + IT_YARN, x + 10 * k++, y);
}

void ui_sprites(const world *w, int state, int st_t, int best_m)
{
    (void)best_m;
    if (state == DS_PLAY || state == DS_FALL) {
        if (w->players == 1) {
            distance(w->m[0].dist_m, RS_SCREEN_W / 2, 10 - 8);
            powerups(&w->m[0], 8, 6);
        } else {
            distance(w->m[0].dist_m, 80, 10 - 8);
            distance(w->m[1].dist_m, 240, 10 - 8);
            powerups(&w->m[0], 8, 6);
            powerups(&w->m[1], RS_SCREEN_W - 40, 6);
        }
    }
    if (state == DS_OVER) {
        int oy = hu_slide_in(st_t, 20, 200);
        if (w->players == 1) {
            distance(w->m[0].dist_m, 24 * 8, 11 * 8 - 4 + oy);
            int md = medal_of(w->m[0].dist_m);
            if (md) {
                spr(SPR_MEDAL + md - 1, 26 * 8 - 4, 15 * 8 - 2 + oy);
                if ((st_t / 20) % 3 == 0) hu_sparkle(26 * 8 + 16, 15 * 8 - 4 + oy, (st_t / 10) % 2, 3);
            }
        } else {
            distance(w->m[0].dist_m, 24 * 8, 11 * 8 - 4 + oy);
            distance(w->m[1].dist_m, 24 * 8, 14 * 8 - 4 + oy);
        }
    }
    if (state == DS_TITLE || state == DS_READY) {
        int x = hu_center("PRESS A TO BOUNCE", 0) * 8 - 12;
        if (hu_blink(st_t)) hu_glyph(HU_BTN_A, x, 21 * 8 - 4, (st_t / 15) % 2, 3);
    }
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("ui." #v, &(v), sizeof(v))
void ui_state(void)
{
    S(shown_state); S(shown_best); S(shown_players); S(shown_score); S(shown_chain);
    hu_state();
}
#undef S
