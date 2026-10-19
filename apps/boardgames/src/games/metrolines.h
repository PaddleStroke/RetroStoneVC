/*
 * Metro Lines for one player: the rules. A city on a paper map, rivers across it; stations appear one after another,
 * each a shape (circle, triangle, square, then rarer ones: diamond, star, cross, pentagon). Passengers appear at the
 * stations, each wanting to reach a station of some other shape. The player draws coloured lines from station to
 * station; a train runs back and forth on each line (round and round on a loop), carrying passengers to a station of
 * the shape they want, changing lines where two lines share a station. A station holding more passengers than it has
 * room for starts its timer; when the timer fills, the run is over. Each week the player picks one of two gifts: a
 * new line, a carriage for a train, two tunnels (a line crossing water takes one for each crossing), an interchange
 * (a station that holds more and loads faster).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The simulation: deterministic, in whole numbers (no floating point: the same on every device, so a game played again
 * from its record, the tests and the level maker see the same run). Space in units of MTL_FP (a station grid's step:
 * 1 unit); time in ticks of 1/MTL_HZ s. Chance from the level's own seed (puzrand.h): which station a passenger appears
 * at, the shape it wants, the week's gifts; nothing the player does draws from it, so every player meets the same
 * passengers.
 * - A line is a list of stops (stations, each once; a loop: back to the first). Between two stops it runs along one
 *   or two straight pieces, each level, upright or at 45 degrees: the diagonal piece first from the lower-numbered
 *   station, or the straight one, whichever crosses water fewer times. Each water crossing takes a tunnel.
 * - Routes: the fewest stops to a station of each shape, over the lines (a loop one way only, as its train goes).
 *   A passenger boards a train whose next stop is closer to its shape than where it stands, stays on while the next
 *   stop brings it closer, gets off at its shape (delivered) or to change lines.
 * - A train speeds up and slows down at each stop; each passenger off or on takes a moment (less at an interchange).
 * The notation (script, the pictures, the tests): "w<s>" the clock on s seconds (tenths allowed), "t<n>" on n ticks;
 * "n<a>-<b>-..." a new line through those stations (numbered from 1 in the order they appeared); "e<l>h-<s>-..." or
 * "e<l>t-<s>-..." line l (1..) extended at its head or tail; "i<l>.<k>-<s>" station s put between stop k and k+1 of
 * line l (stops from 1); "c<l>h" / "c<l>t" line l's head or tail stop taken off; "x<l>" line l taken away; "g<k>"
 * the week's gift k (1, 2); "p<n>" the carriage put on line n, or the interchange at station n; "@<tick>" waits until
 * that tick (a bot's plan).
 */
#ifndef METROLINES_H
#define METROLINES_H

#include <stdint.h>

#define MTL_FP 1000                             /* steps in a unit */
#define MTL_DIAG 1414                           /* a diagonal unit */
#define MTL_HZ 25                               /* ticks a second */
#define MTL_TICK_MS 40
#define MTL_MAXS 40                             /* stations */
#define MTL_MAXL 7                              /* lines */
#define MTL_STOPS 24                            /* a line's stops */
#define MTL_WAIT 30                             /* passengers a station can hold at all */
#define MTL_ROOM 6                              /* ... before its timer runs */
#define MTL_ROOM_INTER 12                       /* an interchange's */
#define MTL_CAR 6                               /* passengers a car takes */
#define MTL_MAXCARS 4
#define MTL_RIDE (MTL_CAR * MTL_MAXCARS)
#define MTL_SHAPES 7
#define MTL_RIVERS 3
#define MTL_RPTS 10                             /* a river's points as drawn */
#define MTL_RPOLY 80                            /* ... smoothed */
#define MTL_LAKES 3
#define MTL_INF 255
#define MTL_CROWD (30 * MTL_HZ)                 /* a station over its room this long: the end */
#define MTL_EVENTS 128
#define MTL_LEVELS 30
#define MTL_CHAPTERS 5
#define MTL_VARIANTS 3
#define MTL_HIST 400                            /* the plan's actions kept (bot, record) */
#define MTL_ACT 40                              /* an action's text */

enum { MTL_CIRCLE, MTL_TRIANGLE, MTL_SQUARE, MTL_DIAMOND, MTL_STAR, MTL_CROSS, MTL_PENTAGON };
enum { MTL_UP_LINE, MTL_UP_CAR, MTL_UP_TUNNEL, MTL_UP_INTER, MTL_UPS };
enum { MTL_EV_DELIVER, MTL_EV_SPAWN, MTL_EV_APPEAR, MTL_EV_WEEK, MTL_EV_OVER, MTL_EV_WON, MTL_EV_BOARD };

typedef struct { int32_t x, y; } mtl_pt;

typedef struct {
    int8_t x, y, shape;                         /* the grid point; its shape */
    int32_t tick;                               /* when it appears */
} mtl_sched;

typedef struct {
    int w, h;                                   /* grid points 0..w-1 across, 0..h-1 down */
    int nriver, rn[MTL_RIVERS], rhw[MTL_RIVERS];   /* rivers: points (units / 4), half width (units / 4) */
    int8_t rx[MTL_RIVERS][MTL_RPTS], ry[MTL_RIVERS][MTL_RPTS];
    int nlake;                                  /* lakes: ellipses (units / 4) */
    int8_t lx[MTL_LAKES], ly[MTL_LAKES], lrx[MTL_LAKES], lry[MTL_LAKES];
    int ns;
    mtl_sched st[MTL_MAXS];
    int lines0, lines_max;                      /* lines at the start, at most */
    int tunnels0, tunnel_gifts;                 /* tunnels at the start; 1: tunnels among the gifts */
    int week;                                   /* ticks */
    int spawn0, spawn_min, ramp;                /* ticks between passengers: at first, after `ramp` ticks (then fewer
                                                 * still as time goes on) */
    int rush_every, rush_len;                   /* rush hours (passengers twice as often): every, how long; 0 none */
    int goal[3];                                /* passengers for 1, 2, 3 stars (0: endless) */
    uint32_t seed;
} mtl_level;

typedef struct {
    int n;                                      /* stops (0: no line) */
    uint8_t stop[MTL_STOPS];
    uint8_t loop;
    uint8_t bend[MTL_STOPS];                    /* link k (stop k to k + 1): its way (0, 1) */
    uint8_t cross[MTL_STOPS];                   /* ... its water crossings */
    int32_t len[MTL_STOPS];                     /* ... its length */
} mtl_line;

typedef struct {
    int k, dir;                                 /* on link k, going from stop k to k + 1 (dir 1) or back (-1) */
    int32_t pos, v;                             /* how far from the stop it left (FP), its speed (FP a tick) */
    int stopped, at;                            /* stopped at stop `at` */
    int timer, dwell;                           /* ticks to its next action; ticks at this stop */
    int cars;
    int nride;
    uint8_t ride[MTL_RIDE];                     /* the shapes its passengers want */
} mtl_train;

typedef struct {
    int nwait;
    uint8_t wait[MTL_WAIT];                     /* the shapes its passengers want, the first come first */
    int crowd;                                  /* ticks over its room (going down again when under) */
    uint8_t inter;
} mtl_station;

typedef struct { uint8_t kind, a, b; int32_t tick; } mtl_event;

typedef struct {
    mtl_level L;
    int nrp[MTL_RIVERS];
    mtl_pt rp[MTL_RIVERS][MTL_RPOLY];           /* the rivers' middles (FP) */
    int32_t tick;
    uint32_t rng;
    int nst;                                    /* stations shown: the first nst of the schedule */
    mtl_station s[MTL_MAXS];
    mtl_line l[MTL_MAXL];
    mtl_train t[MTL_MAXL];
    int lines, tunnels;                         /* owned */
    int delivered;
    int32_t next_spawn, next_week;
    int weekno;
    int choosing;                               /* the week's gifts offered: opt[0], opt[1] */
    uint8_t opt[2];
    int placing;                                /* a gift to put somewhere: MTL_UP_CAR (a line) or MTL_UP_INTER */
    int over, over_st, won;
    uint8_t dist[MTL_SHAPES][MTL_MAXS];         /* stops to a station of each shape */
    uint8_t xc[MTL_MAXS][MTL_MAXS];             /* stations a < b: the water crossings each way (low 4 bits: bend 0) */
    int nev;
    mtl_event ev[MTL_EVENTS];                   /* the last ones: ev[i % MTL_EVENTS] */
    int edits;                                  /* the network changed so many times */
} mtl_state;

/* geometry */
mtl_pt mtl_station_pt(const mtl_state *s, int st);
/* link a-b's way: up to 3 points (the stations and the bend), the bend to take (0, 1); its length (FP) */
int  mtl_route(const mtl_state *s, int a, int b, int bend, mtl_pt pts[3]);
int  mtl_best_bend(const mtl_state *s, int a, int b, int *crossings);
int32_t mtl_path_len(const mtl_pt *p, int n);
int  mtl_wet(const mtl_state *s, int32_t x, int32_t y);
int  mtl_crossings(const mtl_state *s, const mtl_pt *p, int n);
/* the state */
void mtl_begin(mtl_state *s, const mtl_level *L);
void mtl_step(mtl_state *s);                    /* one tick (nothing while a gift is waiting, or over) */
int  mtl_running(const mtl_state *s);           /* 1: the clock moves (not over, no gift waiting) */
int  mtl_tunnels_used(const mtl_state *s);
int  mtl_lines_used(const mtl_state *s);
int  mtl_free_line(const mtl_state *s);         /* a line slot to draw a new line with, -1 none */
int  mtl_on_line(const mtl_state *s, int l, int st);   /* its stop index, -1 */
int  mtl_room(const mtl_state *s, int st);
int  mtl_capacity(const mtl_state *s, int l);
/* the edits: 1 done, 0 refused (and nothing changed) */
int  mtl_new_line(mtl_state *s, const int *stops, int n);
int  mtl_extend(mtl_state *s, int l, int tail, int st);
int  mtl_insert(mtl_state *s, int l, int k, int st);   /* between stop k and k + 1 */
int  mtl_cut(mtl_state *s, int l, int tail);    /* the end stop taken off (a line of two: taken away) */
int  mtl_remove(mtl_state *s, int l);
int  mtl_set_lines(mtl_state *s, const mtl_line *lines);   /* the whole network (undo) */
/* the week */
int  mtl_choose(mtl_state *s, int k);           /* gift k (0, 1) */
int  mtl_place(mtl_state *s, int target);       /* the carriage on line `target`, the interchange at a station */
/* the end: 1 over (crowded) or won (the last goal reached) */
int  mtl_ended(const mtl_state *s);
int  mtl_stars(const mtl_state *s);             /* 0..3 by the goals */
int  mtl_endless_stars(int variant, int delivered);
/* the notation: one action, or many split by spaces; 0 bad */
int  mtl_do(mtl_state *s, const char *act);
int  mtl_script(mtl_state *s, const char *moves);
/* the bots: what the planner would do now (an action, "" nothing); greedy 1: the naive one */
int  mtl_plan(const mtl_state *s, int greedy, int horizon, char *act);
/* a whole run by a bot from the start: the passengers delivered (its plan, joined by spaces, if plan not NULL; the
 * ticks it lasted) */
int  mtl_bot_run(const mtl_level *L, int greedy, int max_ticks, char *plan, int cap, int *ticks);
extern int mtl_bot_horizon;                     /* the planner looking ahead: how far (ticks) */
/* levels, endless maps (and the level maker's parts: a river across the map, stations placed on land) */
void mtl_variant_level(int variant, uint32_t seed, mtl_level *L);
void mtl_make_river(mtl_level *L, uint32_t *r, int across);
void mtl_make_stations(mtl_level *L, uint32_t *r, int n, int32_t every, int rare);
int  mtl_level_get(int level, mtl_level *L);
int  mtl_parse(const char *text, mtl_level *L);
int  mtl_text(const mtl_level *L, char *out, int cap);
extern const char *const mtl_levels[MTL_LEVELS];
extern const char *const mtl_plans[MTL_LEVELS];   /* a plan reaching the last goal (the tests) */
extern const char *const mtl_icons[MTL_CHAPTERS];
int  mtl_chapter_of(int level);
/* the shapes' points for drawing (unit radius): n points into xy (n * 2 floats), returns n (0: a circle) */
int  mtl_shape_poly(int shape, float *xy);

#endif
