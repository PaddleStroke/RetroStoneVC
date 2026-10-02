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

static const char *const medal_name[5] = {"NO MEDAL YET", "BRONZE WHISKER", "SILVER WHISKER", "GOLD WHISKER",
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
        if (state == DS_OVER) {
            if (w->players == 1) {
                hu_banner(4, "GAME OVER");
                hu_panel(10, 9, 20, 12);
                int md = medal_of(m->dist_m);
                hu_box_text(12, 11, "DISTANCE");
                hu_box_text(12, 13, "SCORE");
                snprintf(s, sizeof s, "%d", m->score);
                hu_box_text(29 - (int)strlen(s), 13, s);
                hu_box_text(12, 15, new_best ? "NEW BEST!" : "BEST");
                snprintf(s, sizeof s, "%d M", new_best ? m->dist_m : best_m);
                hu_box_text(29 - (int)strlen(s), 15, s);
                hu_box_text(md ? 15 : 12, 17, medal_name[md]);    /* the medal's sprite left of its name */
            } else {
                hu_standing rank;
                int values[MAX_PLAYERS], keys[MAX_PLAYERS];
                for (int p = 0; p < w->players; p++)
                    keys[p] = values[p] = w->m[p].dist_m;
                hu_rank(&rank, w->players, keys, values);
                hu_results_panel(&rank, NULL);
            }
        }
        shown_state = state;
        shown_best = best_m;
        shown_players = w->players;
    }
    if (state == DS_TITLE) hu_title_draw(st_t, best_m);
    if ((state == DS_PLAY || state == DS_FALL) && w->players > 2) hu_score_tags(w->players, 0);
    if ((state == DS_PLAY || state == DS_FALL) && w->players == 1 && (m->score != shown_score || m->chain != shown_chain)) {
        if (m->chain > 1) snprintf(s, sizeof s, "SCORE %d  X%d", m->score, m->chain);
        else snprintf(s, sizeof s, "SCORE %d", m->score);
        hu_clear_rows(4, 1);
        hu_text(hu_center(s, 0), 4, s);
        shown_score = m->score;
        shown_chain = m->chain;
    }
    if (state == DS_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, "A: MAIN MENU");
        else {
            hu_standing rank = {.n = w->players};
            hu_retry_line_at(hu_results_retry_row(&rank), st_t, RETRY_LOCK, "A: MAIN MENU");
        }
    }
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
            int values[MAX_PLAYERS];
            for (int p = 0; p < w->players; p++) values[p] = w->m[p].dist_m;
            hu_score_chips(w->players, values, 0);
        }
    }
    if (state == DS_OVER) {
        int oy = hu_slide_in(st_t, 20, 200);
        if (w->players == 1) {
            distance(w->m[0].dist_m, 24 * 8, 11 * 8 - 4 + oy);
            int md = medal_of(w->m[0].dist_m);
            if (md) {
                spr(SPR_MEDAL + md - 1, 11 * 8 + 2, 16 * 8 - 3 + oy);
                if ((st_t / 20) % 3 == 0) hu_sparkle(11 * 8 - 3, 16 * 8 - 1 + oy, (st_t / 10) % 2, 3);
            }
        } else {
            hu_standing rank;
            int values[MAX_PLAYERS];
            for (int p = 0; p < w->players; p++) values[p] = w->m[p].dist_m;
            hu_rank(&rank, w->players, values, values);
            hu_results_sprites(&rank, st_t, oy);
        }
    }
    if (state == DS_TITLE) hu_title_sprites(st_t, NULL, NULL);
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("ui." #v, &(v), sizeof(v))
void ui_state(void)
{
    S(shown_state); S(shown_best); S(shown_players); S(shown_score); S(shown_chain);
    hu_state();
}
#undef S
