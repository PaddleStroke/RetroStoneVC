/*
 * Pogo Mamie: the screens' text and the HUD, on the house UI kit (games/common/src/house_ui.c, the look of
 * Leady Squid: docs/art-direction.md): the logo, PRESS A with the A glyph, GET READY, the 2P join line, the
 * distance in the kit's big digits, pause, the game-over banner and panel sliding up, the medals.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 */
#include "pm.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

static int shown_state = -1, shown_best = -1, shown_players, shown_score = -1, shown_chain = -1;
/* the 1-player game-over panel's layout (longer translated text): moved left by over_dx columns, widened right by
 * over_ex columns (0, 0: the English layout, a 20 x 12 panel at column 10) */
static int over_dx, over_ex;

void ui_init(void)
{
    hu_config hc = hu_defaults();
    hc.logo_tile = LOGO_TILE;
    hc.logo_pal = PAL_LOGO;
    hc.obj_vram = VR_OBJ;
    hc.obj_tile = pm_obj_tile_count;           /* the kit's sprites after ours */
    hc.obj_pal = 3;
    hc.box_glyphs = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!:-";
    /* the glyph pool (translated text): 160 tiles between the logo (LOGO_TILE.., 117 tiles) and BG2 (VR_BG2 = 640) */
    hc.ext_tile = LOGO_TILE + 160;
    hc.ext_count = VR_BG2 - hc.ext_tile;
    hu_init(&hc);
    shown_state = -1;
}

/* the medals' names (0 none .. 4 the cat caught) in the player's language */
static const int medal_text[5] = {GS_MEDAL_NAME_0, GS_MEDAL_NAME_1, GS_MEDAL_NAME_2, GS_MEDAL_NAME_3, GS_MEDAL_NAME_4};

/* the 1-player game-over panel: labels from column 12, the distance in big digits centred on column 24 (8 columns
 * left for its label), the score and the best right-aligned to column 28, the medal's name from column 12 (15 after
 * its sprite). Longer text moves the panel left (up to column 0), then widens it right. */
static void over_layout(const char *score, const char *best_l, const char *best_v, const char *medal, int md)
{
    int need = hu_width(HTR(DISTANCE), 0) - 8, n;
    if ((n = hu_width(HTR(SCORE), 0) + 1 + hu_width(score, 0) - 17) > need) need = n;
    if ((n = hu_width(best_l, 0) + 1 + hu_width(best_v, 0) - 17) > need) need = n;
    if ((n = (md ? 3 : 0) + hu_width(medal, 0) - 17) > need) need = n;
    if (need < 0) need = 0;
    over_dx = need < 10 ? need : 10;
    over_ex = need - over_dx < 10 ? need - over_dx : 10;
}

/* ---- BG1 --------------------------------------------------------------------------------------------------------- */
void ui_frame(const world *w, int state, int st_t, int best_m, int best_score, int new_best, int paused)
{
    char s[128];
    const mamie *m = &w->m[0];
    int redraw = draw_redraw_ui();              /* a state of another width: the UI layer was set up again */
    if (redraw) shown_state = -1;
    if (state != shown_state || best_m != shown_best || w->players != shown_players) {
        hu_clear();
        shown_score = shown_chain = -1;
        if (state == DS_TITLE) {
            hu_logo("POGO MAMIE", pm_logo_ramps, 2, 2, 0);
            if (best_m > 0) {
                rs_strf(s, sizeof s, TR(TITLE_BEST), best_m, best_score);
                hu_text(hu_center(s, 0), 24, s);
            }
            hu_copyright(28);
        }
        if (state == DS_OVER) {
            if (w->players == 1) {
                hu_banner(4, HTR(GAME_OVER));
                int md = medal_of(m->dist_m);
                char sc[16], bv[64];
                const char *bl = new_best ? HTR(NEW_BEST_EXCL) : HTR(BEST), *mn = rs_tr(&gs_strings, medal_text[md]);
                snprintf(sc, sizeof sc, "%d", m->score);
                rs_strf(bv, sizeof bv, HTR(METRES_N), new_best ? m->dist_m : best_m);
                over_layout(sc, bl, bv, mn, md);
                int x0 = 12 - over_dx, x1 = 29 + over_ex;     /* the labels' column, just past the values' right edge */
                hu_panel(10 - over_dx, 9, 20 + over_dx + over_ex, 12);
                hu_box_text(x0, 11, HTR(DISTANCE));
                hu_box_text(x0, 13, HTR(SCORE));
                hu_box_text(x1 - hu_width(sc, 0), 13, sc);
                hu_box_text(x0, 15, bl);
                hu_box_text(x1 - hu_width(bv, 0), 15, bv);
                hu_box_text(md ? x0 + 3 : x0, 17, mn);         /* the medal's sprite left of its name */
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
        if (m->chain > 1) rs_strf(s, sizeof s, TR(HUD_SCORE_CHAIN), m->score, m->chain);
        else rs_strf(s, sizeof s, TR(HUD_SCORE), m->score);
        hu_clear_rows(4, 1);
        hu_text(hu_center(s, 0), 4, s);
        shown_score = m->score;
        shown_chain = m->chain;
    }
    if (state == DS_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_MENU));
        else {
            hu_standing rank = {.n = w->players};
            hu_retry_line_at(hu_results_retry_row(&rank), st_t, RETRY_LOCK, HTR(BACK_MENU));
        }
    }
    if (redraw && paused) hu_pause(0, 13);        /* ... and PAUSED, if it shows */
    hu_pause(paused, 13);
    /* widescreen: the UI centred on the screen (the stage origin is 0: the UI layer scrolls by -hu_ui_x()) */
    rs_bg_scroll(RS_BG1, -hu_ui_x(), state == DS_OVER ? -hu_slide_in(st_t, 20, 200) : 0);
}

/* ---- sprites: the distance, power-ups, the medal, the A glyph ------------------------------------------------------ */
static void spr(int id, int x, int y)
{
    const pm_sprite_def *d = &pm_spr[id];
    if (x <= rs_screen_left() - (int)d->w || x >= rs_screen_right() || y <= -(int)d->h || y >= RS_SCREEN_H) return;
    rs_spr(x, y, d->tile, d->w, d->h, d->pal, 3, 0);
}

/* the distance in the kit's big digits, centred on cx (UI coordinates), and the m */
static void distance(int n, int cx, int y)
{
    char s[12];
    snprintf(s, sizeof s, "%d", n);
    int len = (int)strlen(s);
    hu_number(n, cx - 7, y, 3);
    spr(SPR_UNIT_M, cx - 7 + len * 6 - 2 + hu_ui_x(), y);      /* (the kit's digits add hu_ui_x() themselves) */
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
            powerups(&w->m[0], rs_screen_left() + 8, 6);  /* the screen's top-left corner (stage x: wide, the left view) */
        } else {
            int values[MAX_PLAYERS];
            for (int p = 0; p < w->players; p++) values[p] = w->m[p].dist_m;
            hu_score_chips(w->players, values, 0);
        }
    }
    if (state == DS_OVER) {
        int oy = hu_slide_in(st_t, 20, 200);
        if (w->players == 1) {
            distance(w->m[0].dist_m, (24 + over_ex) * 8, 11 * 8 - 4 + oy);
            int md = medal_of(w->m[0].dist_m), mx = (11 - over_dx) * 8;
            if (md) {
                spr(SPR_MEDAL + md - 1, mx + 2 + hu_ui_x(), 16 * 8 - 3 + oy);
                if ((st_t / 20) % 3 == 0) hu_sparkle(mx - 3, 16 * 8 - 1 + oy, (st_t / 10) % 2, 3);
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
    S(shown_state); S(shown_best); S(shown_players); S(shown_score); S(shown_chain); S(over_dx); S(over_ex);
    hu_state();
}
#undef S
