/*
 * Carrom's rules (no SDL): a square board, a hole in each corner, nine white coins, nine black and the red queen in
 * the middle; each player flicks the striker from their baseline. Pocket your colour's coins; a coin of yours pocketed
 * (and no foul) gives another shot. The queen, pocketed, must be covered: one of your coins pocketed in the same shot
 * or the next, else she goes back to the middle. The striker pocketed is a foul: the coins of yours pocketed in that
 * shot (and the queen) go back, and one more of yours (owed if you have none yet). Your last coin pocketed while the
 * queen is not covered goes back. The first side with its nine coins pocketed, the queen covered, wins.
 * Two players (0 white, at the bottom, starts; 1 black, at the top), or four in two teams (0 and 2 white, 1 and 3
 * black; seats round the board to the right: bottom, right, top, left). Prefix crm_.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 0..100 both ways (y down); seat 0's baseline along the bottom. A shot: the striker's place along the
 * shooter's baseline (-1000..1000, their left to their right), an angle (centi-degrees, the board's: 0 along +x, 9000
 * down) and a power (1..1000). Notation (script, the bots): "pos:angle:power", e.g. "0:27000:700".
 */
#ifndef CARROM_H
#define CARROM_H

#include "games/cuetable.h"

#define CRM_SIZE 100.0f
#define CRM_COIN_R 2.15f
#define CRM_STRIKER_R 2.8f
#define CRM_BASE 84.2f                          /* seat 0's baseline: its centre line, from the top */
#define CRM_BASE_HALF 30.0f                     /* the striker's centre: up to this far either side of the middle */
#define CRM_HOLE_R 3.0f
#define CRM_HOLE_AT 3.3f                        /* a hole's centre from both edges */
#define CRM_VMAX 260.0f                         /* the speed of a shot at full power */
#define CRM_COINS 9
#define CRM_HIST 600                            /* shots kept for undo */

enum { CRM_STRIKER = 0, CRM_WHITE = 1, CRM_BLACK = 10, CRM_QUEEN = 19, CRM_N = 20 };   /* white 1..9, black 10..18 */
enum { CRM_PLAY, CRM_WIN0, CRM_WIN1 };          /* the white side won, the black side */
enum { CRM_Q_BOARD, CRM_Q_PENDING, CRM_Q_COVERED };

typedef struct {
    cue_world w;
    int players;                                /* 2 or 4 */
    int turn;                                   /* the seat to shoot */
    int status;
    int queen, queen_team;                      /* CRM_Q_*; the side that pocketed or covered her (-1) */
    int debt[2];                                /* coins owed back after a foul with none pocketed */
    int foul;                                   /* the last shot pocketed the striker */
    int shots;
} crm_pos;

typedef struct {
    crm_pos pos[CRM_HIST + 1];
    int ply;
} crm_game;

void crm_table(cue_table *t);
void crm_new(crm_pos *p, int players);
static inline int crm_team(const crm_pos *p, int seat) { return p->players == 4 ? seat & 1 : seat; }
static inline int crm_side(const crm_pos *p, int seat) { return p->players == 4 ? seat : seat * 2; }   /* 0 bottom,
                                                                                * 1 right, 2 top, 3 left */
static inline int crm_colour(int i) { return i >= CRM_WHITE && i < CRM_BLACK ? 0 : i >= CRM_BLACK && i < CRM_QUEEN ? 1 : -1; }
int  crm_pocketed(const crm_pos *p, int team);  /* that side's coins pocketed */
/* a point of seat 0's frame turned to a side (0..3) of the board, about its middle */
void crm_turn_pt(int side, float x, float y, float *ox, float *oy);
void crm_spot(const crm_pos *p, int seat, int pos, float *x, float *y);    /* the striker's place */
int  crm_spot_free(const crm_pos *p, int seat, int pos);                   /* 1: no coin lies there */
int  crm_free_pos(const crm_pos *p, int seat, int want);                   /* the free place nearest want */
/* the shot: the striker placed and flicked, run to its end, the rules applied; 0: not a legal shot (the place taken,
 * out of range, the game over) */
int  crm_shot(crm_pos *p, int pos, int ang, int pow, cue_events *ev);
void crm_apply(crm_pos *p, const cue_events *ev);   /* the rules after a shot ran (the world at rest) */
/* the striker set for a shot (the world to run): 0 if not legal */
int  crm_ready(const crm_pos *p, int pos, int ang, int pow, cue_world *w);
int  crm_parse(const char *s, int *pos, int *ang, int *pow, int *len);   /* "pos:ang:pow"; 0 bad */
int  crm_next(const crm_pos *p, int seat);

void crm_game_new(crm_game *g, int players);
int  crm_play(crm_game *g, int pos, int ang, int pow, cue_events *ev);
int  crm_undo(crm_game *g);
static inline const crm_pos *crm_now(const crm_game *g) { return &g->pos[g->ply]; }

/* the bot (carrom_bot.c, bots.h) and what the screen (carrom.c) gives it; a headless test defines those two */
const crm_game *carrom_live(void);
int carrom_show(const char *move);
int crm_choose(const crm_pos *p, int level, uint64_t seed, char *move, int cap);   /* 0: none */

#endif
