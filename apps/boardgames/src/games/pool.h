/*
 * Pool's rules (no SDL): a table twice as long as wide, six pockets, the cue ball and the object balls. Two games:
 *   8-ball (variant 0): fifteen balls, 1..7 solids, 9..15 stripes, the 8 black. The table is open after the break;
 *     the first player to pocket a ball (no foul) takes that ball's group. Pocket your group, then the 8. The 8
 *     pocketed early, or on a foul, loses.
 *   9-ball (variant 1): balls 1..9; the cue ball must first strike the lowest ball on the table. Whoever pockets the
 *     9 without a foul wins (a 9 pocketed on a foul comes back to the foot spot).
 * A ball pocketed (no foul) gives another shot (8-ball: one of your group, or any while the table is open). Fouls: the
 * cue ball pocketed, no ball struck or the wrong one first, and after the contact neither a ball pocketed nor a ball
 * at a cushion. The break: a ball pocketed or four at a cushion, else a foul. After a foul the other player has the
 * cue ball in hand: anywhere on the table (the break: behind the head string). Prefix pool_.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The table: 0..100 x 0..50 inside the cushions; the head string x = 25 (the break from behind it), the foot spot
 * (75, 25). A shot: an angle (centi-degrees: 0 along +x, 9000 along +y), a power 1..1000, and with the cue ball in
 * hand its place (hundredths of a unit). Notation (script, the bots): "angle:power", or "angle:power@x,y" in hand,
 * e.g. "0:900@2000,2500".
 */
#ifndef POOL_RULES_H
#define POOL_RULES_H

#include "games/cuetable.h"

#define POOL_W 100.0f
#define POOL_H 50.0f
#define POOL_R 1.27f
#define POOL_HEAD 25.0f
#define POOL_FOOT_X 75.0f
#define POOL_VMAX 300.0f
#define POOL_CORNER 3.4f                        /* a corner pocket's mouth along each edge */
#define POOL_SIDE 2.9f                          /* half a side pocket's mouth */
#define POOL_HIST 600

enum { POOL_EIGHT, POOL_NINE };
enum { POOL_PLAY, POOL_WIN0, POOL_WIN1 };
enum { POOL_OPEN, POOL_SOLIDS, POOL_STRIPES };
enum { POOL_FOUL_NONE, POOL_FOUL_SCRATCH, POOL_FOUL_MISS, POOL_FOUL_WRONG, POOL_FOUL_RAIL, POOL_FOUL_BREAK };

typedef struct {
    cue_world w;                                /* 0 the cue ball, then 1..15 (8-ball) or 1..9 */
    int variant, turn, status;
    int group[2];                               /* POOL_OPEN until taken */
    int in_hand;                                /* the shooter places the cue ball first */
    int brk;                                    /* the next shot is the break (in hand behind the head string) */
    int foul;                                   /* the last shot's foul (POOL_FOUL_*) */
    int shots;
} pool_pos;

typedef struct {
    pool_pos pos[POOL_HIST + 1];
    int ply;
} pool_game;

void pool_table(cue_table *t);
void pool_new(pool_pos *p, int variant, uint64_t seed);
int  pool_balls(const pool_pos *p);             /* the object balls: 15 or 9 */
static inline int pool_group_of(int ball) { return ball >= 1 && ball <= 7 ? POOL_SOLIDS : ball >= 9 && ball <= 15 ? POOL_STRIPES : POOL_OPEN; }
int  pool_left(const pool_pos *p, int group);   /* the group's balls on the table */
/* the balls the shooter may strike first (bit i: ball i) */
uint32_t pool_targets(const pool_pos *p);
/* 1: the cue ball may be placed at (x, y) now (in hand: on the table, free, behind the head string for the break) */
int  pool_place_ok(const pool_pos *p, float x, float y);
/* the world set for a shot (the cue ball placed when in hand, struck): 0 if not legal */
int  pool_ready(const pool_pos *p, int ang, int pow, int placed, float x, float y, cue_world *w);
void pool_apply(pool_pos *p, const cue_events *ev);   /* the rules after a shot ran (p->w at rest) */
int  pool_shot(pool_pos *p, int ang, int pow, int placed, float x, float y, cue_events *ev);
/* "ang:pow" or "ang:pow@x,y"; 0 bad */
int  pool_parse(const char *s, int *ang, int *pow, int *placed, float *x, float *y, int *len);

void pool_game_new(pool_game *g, int variant, uint64_t seed);
int  pool_play(pool_game *g, int ang, int pow, int placed, float x, float y, cue_events *ev);
int  pool_undo(pool_game *g);
static inline const pool_pos *pool_now(const pool_game *g) { return &g->pos[g->ply]; }

/* the bot (pool_bot.c, bots.h) and what the screen (pool.c) gives it; a headless test defines those two */
const pool_game *pool_live(void);
int pool_show(const char *move);
int pool_choose(const pool_pos *p, int level, uint64_t seed, char *move, int cap);   /* 0: none */

#endif
