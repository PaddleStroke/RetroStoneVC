/*
 * Caterpillar Crawl: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE.
 *
 * play.c is the game rules (no drawing, no sound: deterministic, testable alone; the bot too); draw.c is the video,
 * main.c the flow (the title with the players joining, play, game over), input (pads and touch), sound, save RAM,
 * options and save states. Made from games/_template (tools/new_game.py): the house style kit is games/common.
 *
 * The garden plot is a grid of GRID_W x GRID_H cells of CELL px (stage x GRID_X.., y GRID_Y..), the same at every
 * screen width. A caterpillar is a list of cells, head first; it moves one cell per step, all caterpillars on the
 * same clock (the world's step accumulator, Q16 of a cell). Cells are numbered y * GRID_W + x.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define Q16_ONE 65536

/* ---- tuning: every gameplay number is here (DESIGN.md "Rules") ------------------------------------------------ */
/* the plot */
#define GRID_W        18        /* cells across ... */
#define GRID_H        12        /* ... and down (216 cells) */
#define CELL          16        /* px */
#define GRID_X        16        /* the plot's left edge, stage x (a fence of 8 px around it) */
#define GRID_Y        40        /* its top edge, screen y (the HUD band above it) */
#define CELLS         (GRID_W * GRID_H)
/* moving: speeds in hundredths of a cell per second */
#define SPEED_START   600       /* 1 player: 6 cells a second in the first garden, short ... */
#define SPEED_LEN     5         /* ... + 0.05 per segment beyond START_LEN ... */
#define SPEED_GARDEN  40        /* ... + 0.40 per garden after the first ... */
#define SPEED_MAX     1100      /* ... at most 11 cells a second */
#define SLOW_PCT      60        /* the berry: time runs at 60% ... */
#define SLOW_FRAMES   300       /* ... for 5 s */
#define TURN_BUFFER   2         /* turns remembered ahead (a quick double press: a U-turn in two steps) */
/* 1 player: the garden, the leaves, the treats */
#define START_LEN     4         /* segments, head included */
#define START_X       5         /* the head's start cell, crawling right */
#define START_Y       6
#define GROW_PER_LEAF 2         /* segments a leaf adds (one per step, from the next step) */
#define LEAVES_PER_GARDEN 12    /* leaves to eat in a garden before the butterfly */
#define LEAF_MIN_DIST 4         /* a new leaf grows at least this many cells (Manhattan) from the head */
#define FLOWER_EVERY  5         /* a flower blooms after every 5th leaf ... */
#define FLOWER_POINTS 5         /* ... worth 5 points (no growth) */
#define BERRY_EVERY   7         /* a berry after every 7th leaf (when no flower is out): slows time */
#define TREAT_LIFE    420       /* frames a treat stays (it blinks the last TREAT_BLINK) */
#define TREAT_BLINK   120
#define LAYOUTS       6         /* the gardens' obstacle layouts, in rotation (play.c GARDENS) */
/* the butterfly: after LEAVES_PER_GARDEN leaves the caterpillar spins a cocoon and flies off */
#define META_SHRINK   48        /* frames: the body curls into the cocoon */
#define META_COCOON   90        /* the cocoon wiggles until this frame ... */
#define META_FRAMES   180       /* ... then the butterfly flutters up until this one */
#define BUTTERFLY_POINTS 10     /* the bonus for each butterfly */
#define GARDEN_PAUSE  75        /* frames the next garden shows (GARDEN N) before the crawl */
/* 2-4 players: the arena (Tron's light cycles' rules on the plot) */
#define ARENA_SPEED   650       /* 6.5 cells a second, the same for everyone, all match long */
#define ARENA_LEN     3         /* segments at the start of a round */
#define ARENA_GROW_EVERY 2      /* a caterpillar grows one segment every 2 cells it crawls (never shrinks) */
#define ARENA_WINS    3         /* the match: the first to 3 round wins ... */
#define ARENA_MAX_ROUNDS 9      /* ... or the most wins after 9 rounds */
#define ARENA_LAYOUTS 4         /* the arena's layouts, one per round in rotation (play.c ARENAS) */
#define ROUND_PAUSE   90        /* frames: ROUND N, then the crawl */
#define ROUND_END     120       /* frames: the round's winner, then the next round */
#define MELT_EVERY    3         /* frames: an out caterpillar's body vanishes a segment at a time, from the tail */
/* hits */
#define HIT_FREEZE    8         /* hit-stop frames */
#define HIT_FRAMES    40        /* the bonk, then it is down */
/* the flow */
#define PANEL_DELAY   40        /* frames between the end and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define READY_BOB     4         /* the title caterpillar's bob amplitude, px */
#define BOT_START     20        /* frames on the title before the bot starts the run */
#define TOUCH_SWIPE_PX 14       /* px a finger moves (mostly along one axis) to turn the caterpillar */
#define MEDAL_SCORES  {20, 45, 90, 150}          /* bronze, silver, gold, pearl (1 player; tuned with botskill) */

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
enum { DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP };
#define DIR_REV(d) (((d) + 2) & 3)
/* a press (world_step): 1 + DIR_* (a d-pad press or a swipe: a turn), 0 = none, | PRESS_GO (A: starts) */
#define PRESS_DIR(pr) (((pr) & 7) - 1)
#define PRESS_GO 8
enum { MODE_SOLO, MODE_ARENA };
enum cat_state { CS_OFF, CS_READY, CS_CRAWL, CS_HIT, CS_MELT, CS_DOWN, CS_COCOON, CS_WAIT };
enum { HIT_WALL = 1, HIT_ROCK, HIT_SELF, HIT_OTHER };
enum { CELL_FREE = 0, CELL_ROCK = 15 };              /* occ[]: free, 1..4 = a player's body, a rock */
enum { OB_NONE, OB_ROCK, OB_BUSH, OB_POT, OB_SHROOM };   /* what an obstacle looks like (world_obstacle) */
enum { TREAT_NONE, TREAT_FLOWER, TREAT_BERRY };
enum { PH_READY, PH_PLAY, PH_META, PH_GARDEN, PH_ROUND, PH_ROUND_END, PH_OVER };

typedef struct cat {
    int state, t;               /* CS_*, frames in it */
    int dir, mdir;              /* the heading of the next step; the direction of the last step (drawing) */
    int queue[TURN_BUFFER], nq; /* the turns pressed ahead */
    uint8_t body[CELLS];        /* a ring of cells: body[(head + i) % CELLS] is segment i (0 = the head) */
    int head, len, grow;        /* the ring's head index, segments, segments still to grow */
    int score, leaves, flowers, berries, butterflies;
    int wins, crawled, why;     /* arena: rounds won, cells crawled this round; HIT_* */
    int eat_t;                  /* frames since the last leaf (the chomp) */
} cat;

#define FX_MAX 24
enum { FX_PUFF = 1, FX_SPARK, FX_POINTS, FX_LEAF };
typedef struct effect { uint8_t kind, t; int16_t x, y, value; } effect;

typedef struct world {
    int mode, players;
    cat c[MAX_PLAYERS];
    uint8_t occ[CELLS];         /* CELL_FREE, 1 + player, CELL_ROCK */
    uint8_t fat[CELLS];         /* a swallowed leaf's bulge, until the tail passes */
    int layout;                 /* the obstacle layout (GARDENS or ARENAS) */
    int garden;                 /* 1 player: the garden (0 = the first) */
    int phase, phase_t;         /* PH_* */
    int leaf;                   /* the leaf's cell, -1: none */
    int treat, treat_kind, treat_t;   /* the treat's cell (-1), TREAT_*, frames out */
    int slow_t;                 /* the berry's slow time left */
    int garden_leaves;          /* leaves eaten in this garden */
    int meta_len, meta_cell;    /* the butterfly: the body's length when it began, the cocoon's cell */
    int round, round_winner;    /* arena: the round (1..), its winner (-1: a draw) */
    int32_t acc;                /* Q16 cells: the steps' clock */
    int stepped;                /* a step happened this update */
    int t, started;
    rs_rng rng;
    uint32_t seed;
    effect fx[FX_MAX];
    int events[MAX_PLAYERS];    /* EV_* of the last update */
    int wevents;                /* WEV_* */
} world;
enum { EV_START = 1, EV_LEAF = 2, EV_FLOWER = 4, EV_BERRY = 8, EV_HIT = 16, EV_TURN = 32, EV_GROW = 64,
       EV_WIN = 128 };
enum { WEV_STEP = 1, WEV_COCOON = 2, WEV_BUTTERFLY = 4, WEV_GARDEN = 8, WEV_ROUND = 16, WEV_ROUND_END = 32,
       WEV_TREAT = 64, WEV_TREAT_GONE = 128, WEV_OVER = 256, WEV_GO = 512 };

void world_init(world *w, int players, uint32_t seed);
void world_step(world *w, const int press[MAX_PLAYERS]);
int  world_running(const world *w);            /* the run (or the match) goes on */
int  world_over_t(const world *w);              /* frames since it ended (-1: not over) */
int  world_will_step(const world *w);           /* the next update moves the caterpillars */
int  world_speed(const world *w);               /* the current speed, hundredths of a cell per second */
int  world_obstacle(const world *w, int cell);  /* OB_* */
int  cat_cell(const cat *c, int i);             /* segment i's cell (0 = the head) */
int  cat_turn(cat *c, int dir);                 /* buffer a turn (the rules of TURN_BUFFER); 1: taken */
int  world_rank_key(const world *w, int p);     /* arena: the results' key (wins) */
void world_start_garden(world *w, int garden);  /* tests, the opt garden=N */
/* the bot: a press (0 or 1 + DIR_*) for player p; skill 0 = the good bot, 1..10 a human-like one (worse and worse);
 * stop: crash on purpose (the tests' botstop) */
int  bot_decide(const world *w, int p, int skill, int stop);

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen: after a state saved at another width (or in the other view) */
int  draw_zoom(void);               /* first in init: the zoomed tall screen (portrait: the bed upright) */
int  draw_dir_in(int dir);          /* a screen direction (a pad, a swipe) -> the plot's (portrait: turned back) */

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_CHOMP, SFX_TURN, SFX_FLOWER, SFX_BERRY, SFX_BONK, SFX_COCOON, SFX_BUTTERFLY, SFX_POP, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
