/*
 * Nutty Branch: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
 *
 * play.c  the rules: the squirrels' balance physics, jumps, collisions, nuts (no drawing, no sound: deterministic)
 * gen.c   the endless course: segments (curves, dips) and obstacle groups, chunk by chunk, from the world's RNG
 * road.c  the pseudo-3D projection: the course in front of the camera -> one branch row per screen line
 * draw.c  the video (the raster callback sets the affine branch layer per line), bot.c the screen-only test bot,
 * main.c  the flow (the title with the players joining, play, game over), input, sound, save RAM, save states.
 * Every number is in tuning.h.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"
#include "tuning.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */

/* ---- the course: objects along the branch ---------------------------------------------------------------------- */
enum ob_kind {
    OB_NONE, OB_ACORN, OB_HAZEL, OB_WALNUT,          /* nuts */
    OB_TWIG, OB_KNOT, OB_WEB, OB_CATER, OB_CONE, OB_OWL,   /* things on (or over) the bark */
    OB_PECKER,                                       /* the woodpecker (on the side) and its hole */
    OB_SAP, OB_GAP, OB_WIND, OB_FORK,                /* stretches */
    OB_KINDS
};
#define OB_MAX 384
typedef struct obj {
    uint8_t kind, taken;        /* taken: a nut eaten */
    int8_t side;                /* fork: the dead branch's side; wind: its push (-1 left, +1 right); caterpillar: its way */
    uint8_t pad;
    int32_t s;                  /* units along the branch (start) */
    int32_t ang;                /* Q12 degrees: the centre (the hole's, for a woodpecker) */
    int32_t len;                /* units: a gap's, a sap patch's, a gust's, a fork's dead branch */
    int32_t t0;                 /* the frame it woke up (caterpillar, owl, pinecone, woodpecker), -1 = asleep */
} obj;

/* ---- the squirrels ------------------------------------------------------------------------------------------------ */
enum sq_state { SQ_READY, SQ_RUN, SQ_HIT, SQ_FALL, SQ_DOWN, SQ_OFF };
enum fall_kind { FALL_SLIP = 1, FALL_GAP, FALL_CRASH, FALL_STUMP };
typedef struct squirrel {
    int state, t;               /* state, frames in it */
    int32_t s;                  /* Q8 units along the branch */
    int32_t ang, angv;          /* Q12 degrees, per frame */
    int32_t h, vh;              /* Q8 units above the bark, per frame */
    int32_t speed;              /* Q8 units/frame (this frame) */
    int air, hold_t, coyote, buffer, land_t, air_t;
    int slow_t, sticky_t;       /* a web's slow-down and stickiness */
    int branch;                 /* 1 on a fork's dead branch */
    int score, nuts, combo, combo_t;
    int fall_kind, fall_dir;
    int steer;                  /* the last input: -1, 0, 1 */
    int on_sap, wind, warn;     /* this frame: on sap, the gust's push, beyond WARN_ANG */
    int32_t dist_m;             /* metres run */
} squirrel;

/* ---- the generator's state -------------------------------------------------------------------------------------- */
typedef struct gen_state {
    int32_t seg;                /* the next segment to write (absolute index) */
    int32_t head, h;            /* the heading (Q8 units per segment) and the height (Q8 units) at that segment */
    int32_t last_fork_s, last_owl_s, last_wind_s;
    int chunks;
} gen_state;

typedef struct world {
    int16_t curve[SEG_RING];    /* Q8: x'' per segment^2 */
    int32_t head[SEG_RING];     /* Q8 units per segment: the heading at the segment's start */
    int32_t hgt[SEG_RING];      /* Q8 units: the height at the segment's start */
    obj ob[OB_MAX];             /* in order of s; a ring of ob_count from ob_first */
    int ob_first, ob_count;
    gen_state g;
    rs_rng rng;
    uint32_t seed;
    squirrel q[MAX_PLAYERS];
    int players, t, started;
    int32_t speed;              /* the course speed (Q8 units/frame) */
    int32_t lead_s;             /* Q8: the leader's s */
    int leader;                 /* the squirrel the camera follows */
    int32_t cam_s, cam_h, cam_head;   /* Q8 units; cam_head in head[] units */
    int32_t drift;              /* Q8 px: the backgrounds' sideways drift from the curves */
    int events[MAX_PLAYERS];    /* EV_* of the last step */
    int ev_world;               /* EVW_* of the last step */
    int32_t start_m;            /* the course starts this far in (tests: the difficulty of later stretches) */
    int ghost;                  /* tests: the squirrels never fall (the fairness planner samples the course) */
} world;
enum { EV_JUMP = 1, EV_NUT = 2, EV_HIT = 4, EV_LAND = 8, EV_START = 16, EV_FALL = 32, EV_CREAK = 64, EV_WEB = 128,
       EV_WALNUT = 256 };
enum { EVW_PECK = 1, EVW_WIND = 2, EVW_HOOT = 4, EVW_CONE = 8 };

/* input of one squirrel for one frame */
typedef struct sq_input { int steer, jump_pressed, jump_held; } sq_input;

/* ---- play.c -------------------------------------------------------------------------------------------------------- */
int32_t sin_q14(int32_t ang);                       /* Q12 degrees -> Q14 */
int32_t cos_q14(int32_t ang);
int32_t course_speed(int32_t metres);               /* Q8 units/frame */
void world_init(world *w, int players, uint32_t seed);
void world_init_at(world *w, int players, uint32_t seed, int32_t start_m);   /* start deeper in (tests) */
void world_start(world *w);                         /* the squirrels set off */
void world_step(world *w, const sq_input in[MAX_PLAYERS]);
int  world_running(const world *w);                 /* someone still runs */
int  world_all_down(const world *w);
/* the balance of one squirrel on the bark for one frame (curve Q8, wind -1/0/1, sap 0/1): exposed for the tests */
int32_t sq_angacc(const squirrel *q, int steer, int32_t curve, int wind, int sap);
void sq_physics(squirrel *q, int steer, int32_t curve, int wind, int sap);
void sq_jump_step(squirrel *q, int held);
/* the course */
int32_t seg_curve(const world *w, int32_t s);       /* at s (units) */
int32_t seg_height(const world *w, int32_t s_q8);   /* Q8 */
int32_t seg_head(const world *w, int32_t s_q8);     /* the heading (head[] units), interpolated */
const obj *ob_get(const world *w, int i);           /* i-th object in order (0 .. ob_count-1) */
int  floor_at(const world *w, int32_t s, int32_t ang, int branch);   /* 1 = bark under this point (s in units) */
int  owl_ang(const world *w, const obj *o, int32_t *ang);      /* 1 while it swoops: its angle */
int  cater_ang(const world *w, const obj *o, int32_t *ang);    /* 1 while it is on the branch */
int  cone_h(const world *w, const obj *o, int32_t *h);         /* 1 once it falls: its height above the bark (Q8) */
int  hole_open(const world *w, const obj *o);                  /* 0..256 */
int32_t ob_solid(const world *w, const obj *o, int32_t *ang, int32_t *half);   /* a solid's height (Q8) now, 0 = none */

/* ---- gen.c ------------------------------------------------------------------------------------------------------- */
void gen_init(world *w);
void gen_fill(world *w, int32_t until_s);           /* generate the course up to s (units) */
void gen_trim(world *w, int32_t behind_s);          /* drop the objects that are all behind s (units) */

/* ---- road.c: the projection --------------------------------------------------------------------------------------- */
typedef struct road_line {
    int32_t z;                  /* Q8 units from the camera (0: no branch on this line) */
    int32_t s;                  /* units along the branch */
    int16_t xc;                 /* screen x of the texture centre (the branch, or the pair's midpoint at a fork) */
    int16_t a;                  /* 8.8 texels per pixel */
    int16_t row;                /* the texture row (ROW_NONE: none) */
    uint8_t ring, haze;
} road_line;
#define ROAD_PTS (DRAW_SEGS + 2)
typedef struct road_view {
    road_line ln[RS_SCREEN_H];
    int npts;
    int32_t ps[ROAD_PTS];       /* the projected points of the main branch: s (Q8), z (Q8), screen x / y (Q8 px) */
    int32_t pz[ROAD_PTS], px[ROAD_PTS], py[ROAD_PTS];
    int32_t pclip[ROAD_PTS];    /* the highest drawn line (Q8 px) before this point's segment: a sprite below it hides */
    int32_t xref;               /* the camera frame (road.c) */
    int nfk;                    /* the forks in the course (a few at most) */
    int32_t fk_s[4], fk_len[4];
    int fk_side[4];
    int top;                    /* the highest line with branch */
} road_view;
void road_project(const world *w, road_view *v);
/* screen position of a point on the branch: s (Q8), angle (Q12 deg), height (Q8), branch (1 = a fork's dead one).
 * Returns 0 when it is out of view (behind the camera, too far); *hidden = behind a crest; scale = px per unit (Q8). */
int  road_point(const world *w, const road_view *v, int32_t s, int32_t ang, int32_t h, int branch,
                int *sx, int *sy, int *scale, int *hidden);
int32_t fork_offset(const world *w, int32_t s_q8, int branch);  /* Q8 units: a branch's lateral offset at s */

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best_m, int new_best, int paused);
void draw_state(void);
/* what the screen shows (the bot reads only this and OAM): the branch layer's per-line registers of the last frame
 * (the HDMA tables): the texture centre's screen x, the scale (8.8 texels per px) and the row; and its time band */
extern int16_t draw_line_x[RS_SCREEN_H], draw_line_a[RS_SCREEN_H], draw_line_row[RS_SCREEN_H];
int draw_band(void);

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_JUMP, SFX_CRUNCH, SFX_CREAK, SFX_FALL, SFX_TOK, SFX_WIND, SFX_HOOT, SFX_LAND, SFX_WEB,
       SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                         /* first slot of the house sounds (ha_init) */

/* ---- bot.c: the test hook (plays from the screen only) ------------------------------------------------------------ */
void bot_decide(int player, sq_input *in);
void bot_reset(void);
void bot_state(void);

#endif
