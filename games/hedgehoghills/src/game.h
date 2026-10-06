/* Hedgehog Hills. MIT, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "rs.h"
#define MAX_PLAYERS 4
#define Q16_ONE 65536
#define HILL_SPAN 384
#define HERO_X 96
#define GROUND_Y 192
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define READY_BOB 4
#define BOT_START 20
#define MEDAL_SCORES {100, 250, 500, 1000}
/* Widescreen (DESIGN.md "Widescreen"): the share of the extra width of a view (its columns past those it has at 320)
 * shown BEHIND the hedgehog, on the left: one third (the owner's decision), the other two thirds ahead. WIDE_LEFT(extra)
 * rounds it: 36 px on a 427-px screen, 67 at 520, 80 at 560, 0 at 320. */
#define WIDE_LEFT_NUM 1
#define WIDE_LEFT_DEN 3
#define WIDE_LEFT(extra) (((extra)*WIDE_LEFT_NUM*2+WIDE_LEFT_DEN)/(2*WIDE_LEFT_DEN))
enum hero_state { HS_READY, HS_RUN, HS_HIT, HS_DOWN, HS_OFF };
typedef struct hero {
    int state, t, x, score, air_t, land_t;
    int32_t y, vy, vx;
    int64_t wx;
    int grounded, held, combo, smooth_t, apples, apple_id, bonus, launches, spill;
} hero;
typedef struct world {
    hero h[MAX_PLAYERS];
    int players, t, started, scroll;
    uint32_t seed;
    int64_t frost;
    int events[MAX_PLAYERS];
} world;
enum { EV_JUMP=1, EV_SCORE=2, EV_HIT=4, EV_LAND=8, EV_START=16, EV_APPLE=32 };
void world_init(world *w, int players, uint32_t seed);
void world_step(world *w, const int held[MAX_PLAYERS]);
int world_running(const world *w);
int world_all_down(const world *w);
int hero_on_ground(const hero *h);
int32_t hill_y(const world *w, int x);
int32_t hill_slope(const world *w, int x);
void apple_pos(const world *w, int id, int *x, int *y);
int bot_decide(const world *w, int player);
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
void draw_state(void);
void draw_scenery_colors(int scroll,int height,rs_color *sky,rs_color *soil,rs_color *hills);
int draw_hero_sprite(const hero *h,int t);
int draw_player_pal(int p);
int draw_frozen_pixel(const world *w,int wx,int wy);
int draw_iced(int color);
void draw_split_init(void);
void draw_split_frame(const world *w,int st_t,int paused);
void draw_split_off(void);
int draw_split_raster(int line);
void draw_split_state(void);
int draw_wide(void);                        /* widescreen: the screen is wider than 320 */
uint16_t draw_hills_entry(int row,int col); /* the distant forest's map entry (widescreen: with its mirror image) */
void draw_relayout(void);                   /* after a state of another width was loaded: this width's maps */
int draw_wide_left(int view_w);         /* widescreen: the columns a view of view_w shows behind its hedgehog */
void draw_view_log(const world *w);         /* tests (--opt view=1): the view at exit, "view: width= left= p1_x= next_x=" */
extern int draw_p1_x;                       /* tests: P1's sprite centre on the screen, as last drawn (-999: none) */
int draw_split_view_x(int p);                /* tests: the split view of player p (its stage x; -9999: no split) */
int draw_split_view_w(int p);
int draw_split_scroll(int p);
enum { SFX_JUMP, SFX_SCORE, SFX_HIT, SFX_LAND, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT
#endif
