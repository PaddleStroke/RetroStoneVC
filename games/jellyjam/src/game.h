/*
 * Jelly Jam: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/jellyjam/LICENSE.
 *
 * play.c is the rules, the computer chefs' brains and the field layout (no drawing, no sound, no screen width:
 * deterministic, testable alone); draw.c is the video (the fields drawn by software into BG2 tiles, the kitchen, the
 * chefs, the UI); main.c the flow (the title with the players joining, the duels and rounds, game over), input (pads
 * and touch), sound, save RAM, options and save states.
 * Made from games/_template (tools/new_game.py): the house style kit is games/common (docs/art-direction.md).
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define MAX_FIELDS  4                   /* 1 player: its field and the chef's; 2-4 players: one each */

/* ---- tuning: every gameplay number is here ------------------------------------------------------------------- */
/* the field: 6 columns, 12 rows on screen and a hidden 13th row above them (row 0 is the bottom) */
#define COLS          6
#define VROWS         12        /* rows on screen: only these melt */
#define ROWS          13        /* + the hidden row (it holds jellies; one above it is lost when the pair lands) */
#define SPAWN_COL     2         /* a pair comes in on the 3rd column ... */
#define SPAWN_ROW     11        /* ... its pivot in the 12th row: a field filled up to there loses */
#define COLOURS       4         /* jelly colours 1..4 (strawberry, lime, blueberry, lemon) */
#define RAIN          5         /* a grey "rain" jelly */
#define CQ            256       /* heights in 1/256 of a cell */
/* the falling pair */
#define FALL_START    10        /* 1/256 cells a frame at the start of a duel or a round (a cell in 26 frames) */
#define FALL_LEVEL    3         /* duels: + this per level */
#define FALL_MAX      64        /* at most (a cell in 4 frames) */
#define SPEEDUP_FRAMES 1200     /* every 20 s of a duel or a round ... */
#define SPEEDUP_STEP  5         /* ... the pairs fall this much faster */
#define SOFT_SPEED    112       /* down held: a cell in 2.3 frames */
#define FAST_SPEED    176       /* touch: a swipe down drops the pair until it lands */
#define LOCK_FRAMES   28        /* a pair that has landed waits this long: it can still slide and turn */
#define LOCK_RESETS   8         /* moves and turns on the ground that restart the wait, at most */
#define DAS_DELAY     10        /* left / right held: the first repeat after this many frames ... */
#define DAS_REPEAT    3         /* ... then every 3 */
#define QUICK_TURN    20        /* a turn blocked on both sides: a second press within this flips the pair */
#define SPAWN_DELAY   6         /* frames between a turn's end and the next pair */
#define ROT_ANIM      5         /* frames of the child swinging round (draw.c) */
#define SLIDE_ANIM    3         /* frames of a step sliding (draw.c) */
/* settling and melting */
#define SETTLE_V0     20        /* jellies falling after a landing or a melt: start speed (1/256 cells a frame) ... */
#define SETTLE_ACCEL  7         /* ... + this a frame ... */
#define SETTLE_VMAX   120       /* ... at most */
#define SQUASH_FRAMES 8         /* a jelly that lands squashes and wobbles back */
#define MELT_MIN      4         /* jellies of one colour touching (up, down, left, right) melt from 4 */
#define MELT_FRAMES   42        /* the melt: they flash for 26 frames, then pop */
#define MELT_FLASH    26
#define POPS_MAX      24        /* the pops a field remembers (draw.c: the jam droplets) */
/* scoring: points = 10 x jellies x (chain power + colour bonus + group bonus), at least 10 x jellies */
#define CHAIN_POWER   {0, 6, 14, 28, 48, 80, 112, 144, 176, 208, 240, 272, 304, 336, 368, 400, 432, 464, 496, 528}
#define CHAIN_POWER_N 20
#define COLOUR_BONUS  {0, 3, 6, 12}                  /* 1, 2, 3, 4 colours melting at once */
#define GROUP_BONUS   {0, 2, 3, 4, 5, 6, 7, 10}      /* a group of 4, 5, 6, ... 11 or more */
#define GROUP_BONUS_N 8
#define SOFT_POINTS   1         /* a point per row soft-dropped */
/* the rain: melting sends grey jellies to an opponent; your own melts first cancel the rain coming to you */
#define RAIN_POINTS   60        /* points per rain jelly */
#define RAIN_MAX_DROP 30        /* at most 5 rows fall at once (the rest waits for the next turn) */
#define RAIN_ICONS    {720, 180, 30, 6, 1}          /* the queue's icons: crown, jar, big cake, big, small */
/* duels (1 player) and rounds (2-4 players) */
#define WINS_NEEDED   2         /* 2-4 players: the first to win 2 rounds (best of 3) */
#define INTRO_FRAMES  110       /* the banner before a duel or a round (GO! for the last 40) */
#define GO_FRAMES     40
#define END_FRAMES    160       /* YOU WIN! / P2 WINS! before the next duel or round */
#define WIN_BONUS     1500      /* a duel won: + 1500 x its level */
#define CHEFS         8         /* the computer chefs, level 1 .. 8; then they come back stronger */
/* the computer chefs: think (frames before the first input), step (frames between inputs), soft (soft-drops into
 * place), ahead (also plans the next pair), ambition (the shortest chain it fires when not pressed), mistake (% of
 * pairs placed at one of its 5 best spots at random), noise (its judgement's wobble), aware (fires early when rain is
 * coming or its field is high) */
#define CPU_TABLE { \
    /* 1 PIP the mouse      */ {64, 20, 0, 0, 1, 45, 100, 0}, \
    /* 2 DOT the duckling   */ {52, 17, 0, 0, 1, 36, 85, 0}, \
    /* 3 HOPSY the bunny    */ {36, 12, 0, 0, 2, 20, 55, 1}, \
    /* 4 MOCHI the cat      */ {28, 10, 1, 0, 2, 14, 40, 1}, \
    /* 5 TRUFFLE the piglet */ {22,  8, 1, 1, 3,  9, 30, 1}, \
    /* 6 BASIL the fox      */ {16,  7, 1, 1, 3,  5, 20, 1}, \
    /* 7 BAO the panda      */ {12,  6, 1, 1, 4,  3, 12, 1}, \
    /* 8 BRUNO the bear     */ { 9,  5, 1, 1, 5,  1,  6, 1}}
#define CPU_PLAYER    {6, 3, 1, 1, 5, 0, 0, 1}      /* the player bot (bot=1): quick, plans ahead, no mistakes */
/* the brains' judgement of a field (play.c eval_board, fire_value): what it likes and dislikes */
#define AI_W_CONN     10        /* two jellies of a colour touching */
#define AI_W_POT      12        /* x (half chains)^2: the longest chain 1 or 2 more jellies would fire */
#define AI_W_HEIGHT   3         /* per jelly of height */
#define AI_HIGH       9         /* a column higher than this ... */
#define AI_W_HIGH     60        /* ... costs this x the square of the excess ... */
#define AI_W_SPAWN    400       /* ... and this x the excess near the spawn column */
#define AI_W_BUMP     2         /* per step between neighbouring columns */
#define AI_W_RAINCELL 4         /* per rain jelly on the field */
#define AI_THREAT_RAIN 6        /* rain coming: an aware brain fires what it has */
#define AI_CROWDED    8         /* a column this high, or ... */
#define AI_CROWDED_N  40        /* ... this many jellies on the field: any brain fires what it has */
#define AI_W_FIRE     40        /* per rain jelly a chain sends */
#define AI_W_CHAIN    150       /* per chain step */
#define AI_W_KILL     5000      /* a chain that buries the opponent */
#define AI_W_WASTE    30        /* per jelly melted by a chain shorter than its ambition */
#define AI_W_TRIG     20        /* a spot where one more jelly would melt a group */
#define AI_POT_TOP    4         /* the potential is worked out for its 4 best spots (one a frame) ... */
#define AI_AHEAD_TOP  3         /* ... the next pair after its 3 best (one a frame) */
#define AI_NOISE_K    1         /* the noise's unit */
#define AI_TRIES      12        /* inputs at most for one pair (then it lets it fall) */
/* the layout (play.c field_layout): 2 fields of 16-px cells, 3-4 of 12-px cells, on the 320-px stage; a wider
 * screen shows more of the kitchen round them */
#define LAYOUT_CELL_BIG   16
#define LAYOUT2_X0        20    /* the two fields' insides: x, and y (their 4-px frames are outside) */
#define LAYOUT2_X1        204
#define LAYOUT2_Y         44
#define LAYOUT_CELL_SMALL 12
#define LAYOUT3_X0        12
#define LAYOUT3_DX        112
#define LAYOUT4_X0        4
#define LAYOUT4_DX        80
#define LAYOUT34_Y        76
/* the flow */
#define PANEL_DELAY   90        /* frames between the lost field and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define BOT_START     20        /* frames on the title before the bot starts the run */
#define HINT_FRAMES   420       /* the controls hint in the first duel */
#define MEDAL_SCORES  {3000, 12000, 35000, 70000}    /* bronze, silver, gold, pearl (1 player: points) */
/* touch (player 1; main.c, play.c touch_column): drag the pair column by column, tap to turn, swipe down to drop */
#define TOUCH_SOFT_PX 28        /* a finger dragged this far down (and more down than sideways): soft drop */

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
typedef struct pair_t {                 /* the falling pair: a pivot and a child that turns round it */
    int32_t y;                          /* the pivot's bottom, 1/256 cells above the field's floor */
    int8_t col, rot;                    /* the pivot's column; the child: 0 up, 1 right, 2 down, 3 left */
    uint8_t c0, c1;                     /* colours: pivot, child */
    int16_t lock_t;                     /* frames on the ground */
    uint8_t resets, qturn;              /* lock restarts used; frames left for a quick turn */
    int8_t rot_from, slide;             /* the last turn's start (draw: the swing), the last step (-1, 1) */
    uint8_t rot_t, slide_t;             /* frames since the turn, the step (draw) */
    uint8_t fast, pad_;                 /* a swipe down: falls fast until it lands */
} pair_t;

typedef struct ai_cand { int32_t v, fire; int8_t col, rot, pad_[2]; } ai_cand;   /* a spot for the pair */
typedef struct ai_t {                   /* a computer chef's (or the bot's) plan for the pair */
    int8_t tcol, trot;                  /* where it wants it */
    uint8_t planned, gave_up, gave_up_n, stage;  /* gave_up: lets the pair fall where it is; inputs used; plan stage */
    int16_t wait, idx, n;               /* frames before its next input; the plan's progress; its spots */
    uint16_t seq;                       /* the pair it planned for */
    int32_t value;                      /* how much it liked it (tests) */
    ai_cand c[24];
} ai_t;

typedef struct pop_t { uint8_t col, row, colour, pad_; } pop_t;

enum { FS_IDLE, FS_SPAWN, FS_PAIR, FS_SETTLE, FS_CHECK, FS_MELT, FS_LOST };
enum { FK_HUMAN, FK_CPU };
typedef struct field {
    uint8_t g[ROWS][COLS];              /* 0 empty, 1..4 a colour, 5 rain */
    int16_t off[ROWS][COLS];            /* still to fall (1/256 cells): drawn this much higher */
    uint8_t melt[ROWS][COLS];           /* melting now */
    uint8_t sq[ROWS][COLS];             /* frames of the landing squash left */
    uint8_t state, kind, alive, rained; /* FS_*, FK_*; rained: the rain fell this turn */
    int16_t t, fall_v;                  /* frames in the state; the settling speed */
    pair_t pr;
    uint16_t seq;                       /* the current pair's number in the shared sequence */
    int32_t score, rain_q, carry;       /* points; rain coming to this field; points not yet sent as rain */
    int32_t sent, drop_acc;             /* rain sent this game; soft drop (1/256 cells) not yet scored */
    int16_t chain, max_chain;           /* the chain now (0: none), the longest */
    int16_t chain_shown, chain_t;       /* the last chain step shown and frames since (draw: "2 CHAIN!") */
    int16_t chain_col, chain_row;       /* where it melted */
    int16_t lost_t, place;              /* frames since it lost; its place (1: the last standing) */
    int16_t wins, target;               /* rounds won (2-4 players); where its last rain went */
    int8_t das_dir, das_t;              /* left / right held */
    int8_t level, pad_;                 /* a chef's level (FK_CPU) */
    int16_t pop_age, npops;             /* the last pop: frames since, the jellies */
    pop_t pops[POPS_MAX];
    int32_t melted;                     /* jellies melted (tests) */
    ai_t ai;
} field;

enum { MODE_DUEL, MODE_VS };            /* 1 player against the chefs; 2-4 players against each other */
enum { PH_INTRO, PH_PLAY, PH_END, PH_OVER };
typedef struct world {
    field f[MAX_FIELDS];
    int nf, players, mode;              /* fields; human players; MODE_* */
    int level;                          /* the duel's level (1..) */
    int round;                          /* 2-4 players: the round (1..) */
    int phase, phase_t, round_t;        /* PH_*; frames in the phase; frames of play this round */
    int winner;                         /* the last round's or duel's winner (-1: a draw) */
    int lost;                           /* 1 player: the duel was lost (game over) */
    int fall;                           /* the pairs' speed now */
    int places;                         /* fields out so far this round */
    uint32_t seed, seq_seed;            /* the run; the pairs of this duel / round */
    rs_rng rng;
    int t, started;
    int events[MAX_FIELDS];             /* EV_* of the last step */
    int duels_won;
} world;
enum { EV_MOVE = 1, EV_TURN = 2, EV_LAND = 4, EV_LOCK = 8, EV_MELT = 16, EV_POP = 32, EV_RAIN = 64, EV_LOSE = 128,
       EV_WIN = 256, EV_SPAWN = 512, EV_SENT = 1024, EV_OFFSET = 2048, EV_START = 4096 };

/* a player's input this frame (pads, touch or a brain) */
typedef struct pin {
    int8_t dir;                         /* left / right held: -1, 0, 1 */
    uint8_t dir_pressed;                /* ... went down this frame */
    int8_t step;                        /* touch: move one column now (-1, 1), no repeat */
    uint8_t down, cw, ccw, fast, pad_;  /* soft drop held; turn clockwise / counter-clockwise (pressed); swipe down */
} pin;

typedef struct ai_params { int16_t think, step; uint8_t soft, ahead, ambition, mistake, noise, aware; } ai_params;

void world_init(world *w, int players, uint32_t seed);              /* the title: no fields in play */
void world_start(world *w, int level);                              /* a run: 1 player: duel at level; else round 1 */
void world_step(world *w, const pin in[MAX_FIELDS]);               /* the humans' inputs; the chefs think */
int  world_over(const world *w);                                   /* the run is over (lost duel, match decided) */
void seq_pair(uint32_t seed, int i, uint8_t *c0, uint8_t *c1);     /* the shared sequence of pairs */
const ai_params *cpu_params(int level);                            /* a chef's brain (level 1..) */
void ai_skill_params(int skill, ai_params *out);                    /* the bot: 0 the player bot, 1..10 human-like */
void ai_play(world *w, int fi, const ai_params *P, pin *out);      /* a brain plays field fi this frame */
int  field_height(const field *f, int col);
int  field_count(const field *f);                                  /* jellies on the field */
int  melt_points(int chain, int jellies, int colours, const int *groups, int ngroups);
/* the bare rules on a grid (tests, the brains): drop a pair, resolve every chain */
int  grid_place(uint8_t g[ROWS][COLS], int col, int rot, int c0, int c1);
int  grid_resolve(uint8_t g[ROWS][COLS], int *points, int *melted);   /* returns the chain's length */
/* the pair's moves (tests) */
int  pair_move(field *f, int dir);
int  pair_turn(field *f, int dir);
int  pair_floor(const field *f, const pair_t *p);
void field_reset(field *f);
int  pick_target(const world *w, int from);                         /* who gets field from's rain */
void rain_send(world *w, int from, int n);                          /* n rain from field from: offset, then sent */

/* the layout: where the fields go on the 320-px stage (draw.c, touch) */
typedef struct flayout { int16_t x, y, cell, w, h; } flayout;       /* a field's inside, stage px */
int  field_layout(int nf, flayout out[MAX_FIELDS]);
int  touch_column(int col0, int dx_px, int cell);                   /* touch: the column a drag of dx px aims at */

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
typedef struct touch_view { uint8_t seen, down, pad_[2]; int16_t x, y; } touch_view;
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, const touch_view *tv);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen and caches: after a state */
extern const char *const chef_names[CHEFS];

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_MOVE, SFX_TURN, SFX_LAND, SFX_POP, SFX_CHAIN, SFX_RAIN, SFX_LOSE, SFX_WIN, SFX_SEND, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
