/*
 * Kitty Kibble: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
 *
 * play.c is the game rules (no drawing, no sound: deterministic, testable alone; the bot and the touch swipe too);
 * mazes.c the five rooms' hand-made mazes; draw.c is the video, main.c the flow (the title with the players joining,
 * play, game over), input (pads and touch), sound, save RAM, options and states. Made from games/_template
 * (tools/new_game.py): the house style kit is games/common (docs/art-direction.md).
 *
 * Coordinates: the maze is MAZE_W x MAZE_H cells of CELL px; actors move in maze pixels (Q16), their x, y is the
 * centre of their 16x16 sprite; a cell's centre is (c * 8 + 4, r * 8 + 4). The maze sits at stage (MAZE_X, MAZE_Y).
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define Q16_ONE 65536

/* ---- the maze ---------------------------------------------------------------------------------------------------- */
#define MAZE_W        27        /* cells (corridors 1 cell wide, walls 2 thick: the actors are 16x16) */
#define MAZE_H        24
#define CELL          8         /* px */
#define MAZE_X        52        /* the maze's stage x (centred in the 320 columns: 52 + 216 + 52) */
#define MAZE_Y        28        /* its stage y (the score above it, the 3-4 player chips below it) */
#define ROOMS         5         /* kitchen, living room, bedroom, garden, attic; then again, faster */
#define DOCK_X        13        /* the exit cell, right above the dock's gate (the vacuums come out here) */
#define DOCK_Y        7
#define DOCK_IN_Y     10        /* the dock's middle row inside (the vacuums charge there) */
#define DOCK_SLOTS    {13, 11, 15, 13}    /* each vacuum's x in the dock (0 starts outside, on the exit cell) */
enum { C_WALL, C_EMPTY, C_KIBBLE, C_YARN, C_GATE, C_DOCK, C_TUNNEL };

/* ---- tuning: every gameplay number is here (DESIGN.md "Rules") ------------------------------------------------- */
/* the kittens */
#define KIT_SPEED     75366     /* Q16 px/frame (1.15) in room 1 ... */
#define KIT_SPEED_UP  3         /* ... % faster each room (rooms 2-7; then it stays) */
#define KIT_YARN_UP   8         /* % faster during yarn time */
#define KIBBLE_PAUSE  1         /* frames a kitten stops to eat a kibble (the vacuums gain on a kitten that eats) */
#define YARN_PAUSE    3         /* ... and a ball of yarn */
#define TURN_BUFFER   20        /* frames a direction pressed (and let go) is kept for the next junction */
#define CORNER_PX     3         /* a turn is taken up to this many px before (or after) a junction's centre */
/* the robot vacuums */
#define VACUUMS       4
#define VAC_SPEED     55706     /* Q16 px/frame (0.85) in room 1 ... */
#define VAC_SPEED_UP  7         /* ... % faster each room (rooms 2-7) */
#define VAC_TUNNEL    55        /* % of their speed in the side tunnels (the pet doors) */
#define VAC_SCARED    55        /* % of their speed while scared (yarn time) */
#define VAC_HOME      196608    /* Q16 px/frame (3): a caught vacuum zips back to its dock */
#define VAC_LEAVE     49152     /* Q16 px/frame (0.75): leaving the dock (and going in) */
#define DOCK_RELEASE  {0, 90, 360, 660}   /* frames into a room each vacuum leaves the dock (room 1) ... */
#define DOCK_FASTER   12        /* ... % sooner each room (rooms 2-6) */
#define DOCK_REGEN    90        /* frames a caught vacuum recharges in the dock before it comes out again */
/* the personalities (DESIGN.md "The vacuums"): Dash heads straight for the kitten, Sneak cuts it off ahead, Patrol
 * guards its corner of the house, Wobble wanders at random and backs off when it gets close */
enum { V_DASH, V_SNEAK, V_PATROL, V_WOBBLE };
#define SNEAK_AHEAD   4         /* cells ahead of the kitten Sneak aims at */
#define PATROL_POST_X 6         /* Patrol's post (cell), in its corner (the bottom left) ... */
#define PATROL_POST_Y 19
#define PATROL_RANGE  9         /* ... it chases a kitten that comes this close (cells) to its post */
#define WOBBLE_SHY    6         /* cells: Wobble backs off to its corner from a kitten this close */
#define HOME_CORNERS  {{24, -3}, {2, -3}, {0, 26}, {26, 26}}  /* scatter targets (cells, outside the maze) */
/* scatter / chase: frames of each period (-1: until the room ends), rooms 1-2, then rooms 3+ */
#define MODES_EASY    {420, 1200, 360, 1200, 300, 1500, 240, -1}
#define MODES_HARD    {300, 1320, 240, 1440, 180, 1800, 120, -1}
/* the balls of yarn (4 a room): the vacuums are scared, slow and can be caught */
#define YARN_FRAMES   420       /* frames of yarn time in room 1 ... */
#define YARN_LESS     45        /* ... fewer each room ... */
#define YARN_MIN      150       /* ... at least this */
#define YARN_FLASH    120       /* the last frames: the scared vacuums flash */
#define CATCH_PX      6         /* a kitten and a vacuum closer than this (px, both axes) meet */
#define CATCH_FREEZE  30        /* frames everything stops on a catch (the points show) */
/* points */
#define KIBBLE_POINTS 1
#define YARN_POINTS   5
#define CATCH_POINTS  20        /* the first vacuum caught after a ball of yarn; then 40, 80, 160 */
#define FISH_POINTS   {25, 50, 75, 100, 150, 200}   /* the fish treat, by room (the last one after) */
/* the fish treat: it appears on the room's fish spot when 30% and 70% of the kibble is eaten, for 9 s */
#define FISH_AT       {30, 70}
#define FISH_FRAMES   540
/* the flow */
#define INTRO_FRAMES  120       /* the room's name (ROOM 2 / LIVING ROOM): nothing moves yet */
#define CLEAR_FRAMES  120       /* all the kibble eaten: the maze flashes, then the next room */
#define HIT_FREEZE    30        /* the hit-stop when a vacuum catches the last kitten (the run ends) */
#define HIT_FREEZE_MP 12        /* ... when others still play */
#define CAUGHT_FRAMES 70        /* the caught kitten's dizzy spin, then it is out */
#define PANEL_DELAY   40        /* frames between the end of the spin and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define READY_BOB     4         /* the title kitten's bob amplitude, px */
#define BOT_START     20        /* frames on the title before the bot starts the run */
#define MSG_FRAMES    120       /* P2 CAUGHT! */
#define FX_MAX        12        /* points, sparkles and dust shown */
#define TOUCH_DRAG    12        /* px: a finger moved this far (mostly along one axis) turns the kitten (a swipe) */
#define MEDAL_SCORES  {200, 600, 1500, 3000}    /* bronze, silver, gold, pearl (1 player; tuned with botskill) */

/* ---- mazes.c: the rooms --------------------------------------------------------------------------------------------
 * '#' wall, '.' kibble, 'o' a ball of yarn, ' ' empty, 'T' a tunnel's mouth (the pet doors: the row wraps), '-' the
 * dock's gate (vacuums only), 'D' the dock, '1'..'4' the kittens' start cells, 'f' the fish treat's spot. */
extern const char *const maze_rows[ROOMS][MAZE_H];

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
enum { DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP, DIR_NONE = -1 };
#define DIR_BIT(d) (1 << (d))
enum kitten_state { KS_PLAY, KS_CAUGHT, KS_OUT };
typedef struct kitten {
    int state, t;               /* state, frames in it */
    int32_t x, y;               /* Q16 maze px: the centre */
    int dir, moving, face;      /* the direction of travel (moving: not stopped at a wall), the way it faces */
    int want, want_t;           /* the buffered turn and its frames left (DIR_NONE: none) */
    int score, eaten, catches;  /* points; kibble eaten; vacuums caught */
    int chain;                  /* vacuums caught since the last ball of yarn (the points double) */
    int pause;                  /* frames it stops (eating) */
    int munch_t;                /* frames since the last bite (the munch frame) */
} kitten;
enum vacuum_state { VS_DOCK, VS_LEAVE, VS_ROAM, VS_HOME, VS_ENTER };
typedef struct vacuum {
    int state, t;
    int32_t x, y;               /* Q16 maze px */
    int dir, reverse;           /* the direction; turn round at the next cell (a mode change, yarn) */
    int scared_t;               /* frames of yarn time left (0: not scared) */
    int wait;                   /* frames before it leaves the dock */
    int tx, ty;                 /* its target cell (the last one chosen: tests, the debug view) */
} vacuum;
enum { FX_POINTS, FX_SPARK, FX_DUST };
typedef struct effect { int16_t x, y, value; uint8_t kind, t, on, pad; } effect;
/* one player's input for a frame: the d-pad held and pressed (DIR_BIT masks); start: the title's start press */
typedef struct pin { uint8_t held, pressed, start; } pin;

enum { PH_READY, PH_INTRO, PH_PLAY, PH_CLEAR };
enum { MODE_SCATTER, MODE_CHASE };
typedef struct world {
    uint8_t cell[MAZE_H][MAZE_W];   /* C_*: the room now (the kibble eaten turns C_EMPTY) */
    int room, maze;             /* the room number (1, 2...), its maze (0..ROOMS-1) */
    int kibble_left, kibble_total;
    int phase, phase_t, freeze;
    int mode, mode_i, mode_t;   /* scatter or chase, the period, its frames left (-1: to the end) */
    int fish_on, fish_t, fish_n, fish_x, fish_y;
    int start_x[MAX_PLAYERS], start_y[MAX_PLAYERS];
    kitten k[MAX_PLAYERS];
    vacuum v[VACUUMS];
    effect fxs[FX_MAX];
    int fxs_next;
    rs_rng rng, bot_rng;
    uint32_t seed;
    int players, t, started;
    int msg, msg_t, msg_player;
    int touch_x, touch_y, touch_on;     /* player 1's finger: where the last swipe step was measured from */
    int bot_dir[MAX_PLAYERS], bot_t[MAX_PLAYERS];   /* the human-like bot's last decision and its age */
    int bot_goal[MAX_PLAYERS];  /* the cell the bot heads for (it sticks to it: no dithering) */
    int events[MAX_PLAYERS];    /* EV_* of the last step */
    int wevents;                /* WEV_* of the last step */
} world;
enum { EV_MUNCH = 1, EV_YARN = 2, EV_CATCH = 4, EV_FISH = 8, EV_CAUGHT = 16, EV_OUT = 32, EV_TURN = 64 };
enum { WEV_INTRO = 1, WEV_PLAY = 2, WEV_CLEAR = 4, WEV_ROOM = 8, WEV_FISH = 16, WEV_SCARED = 32, WEV_CALM = 64,
       WEV_HOME = 128, WEV_MODE = 256, WEV_FISH_GONE = 512 };
enum { MSG_NONE, MSG_CAUGHT };

void world_init(world *w, int players, uint32_t seed);
void world_start_room(world *w, int room);      /* the next room (tests, --opt room=N) */
void world_step(world *w, const pin in[MAX_PLAYERS]);
int  world_running(const world *w);             /* a kitten still plays */
int  world_all_out(const world *w);
int  world_yarn_time(const world *w);           /* the longest scared time left (0: none) */
int  cell_at(const world *w, int cx, int cy);   /* C_* (columns wrap: the tunnels; outside rows: walls) */
int  kitten_open(const world *w, int cx, int cy, int dir);   /* a kitten can go from cell (cx, cy) that way */
int  kitten_speed(const world *w, const kitten *k);
int  vacuum_speed(const world *w, const vacuum *v);
int  yarn_frames(int room);
int  dock_release(int room, int v);
void vacuum_target(const world *w, int v, int *tx, int *ty);  /* where vacuum v heads now (its personality) */
int  nearest_kitten(const world *w, int cx, int cy);          /* the closest kitten still playing (-1: none) */
int  fish_points(int room);
/* touch (player 1): a finger down at (x, y) (pressed: it went down this frame); returns the direction of a swipe
 * step (TOUCH_DRAG px, mostly along one axis) or DIR_NONE. Keeps its anchor in the world (the states save it). */
int  touch_swipe_dir(world *w, int down, int pressed, int x, int y);
/* the bot: the d-pad it holds for player p (skill 0 plays as well as it can; 1..10 a human-like player, worse) */
int  bot_decide(world *w, int p, int skill);

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen: after a state saved at another width */

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_MUNCH, SFX_YARN, SFX_SCARED, SFX_CATCH, SFX_ZIP, SFX_FISH, SFX_CAUGHT, SFX_CLEAR, SFX_LEAVE, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
