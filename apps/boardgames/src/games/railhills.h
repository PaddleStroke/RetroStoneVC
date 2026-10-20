/*
 * Hill Railways for 2 to 4 players: the rules. A hex map of meadows, forests, fields, hills, mountains, a river and
 * lakes, with towns. Each player holds secret contracts (two towns to link); a few public lines are open to all.
 * A turn: build track (up to RH_PTS points: each hex costs its terrain; each new hex must touch your own track or a
 * town; one track a hex), survey a face-down hex (1 point: it and its neighbours turn up), or draw contracts (keep at
 * least one). A contract is delivered (any time in your turn) once its towns are linked by your track, towns and
 * other players' track: you earn its value, and pay each owner a coin a hex of theirs you used. The first to deliver
 * a public line takes it with its bonus. Face-down hexes turn up when track is laid next to them; some hide a hamlet
 * (a coin to the first to build on it). The game ends at the end of the round in which a player lays their last
 * track; contracts not delivered then cost their value. Most coins wins (ties: more contracts delivered).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The map: an island on a grid of RH_W x RH_H cells at most (odd rows shifted half a hex right, pointy-top hexes), a
 * cell's index c = col + row * w. Directions 0..5: east, south-east, south-west, west, north-west, north-east.
 * Moves (script, the bots): "b<c>" build on cell c, "s<c>" survey c, "d<i>" deliver the contract in hand slot i,
 * "p<j>" deliver public line j, "x" draw contracts, "k<mask>" keep the offered ones (bit i: offer i), "e" end the
 * turn. A turn whose only move left is "e" ends by itself.
 */
#ifndef RAILHILLS_H
#define RAILHILLS_H

#include <stdint.h>

#define RH_MAXW 13
#define RH_MAXH 14
#define RH_MAXC (RH_MAXW * RH_MAXH)
#define RH_MAXT 14                              /* towns */
#define RH_MAXK 96                              /* contracts: town pairs */
#define RH_HAND 8                               /* contracts held, at most */
#define RH_PUB 3                                /* public lines face up, at most */
#define RH_OFFER 4
#define RH_NP 4

enum { RH_SEA, RH_MEADOW, RH_FOREST, RH_FIELD, RH_HILL, RH_RIVER, RH_MOUNTAIN, RH_LAKE, RH_TOWN, RH_TERRAINS };
enum { RH_KEEP, RH_PLAY, RH_OVER };             /* phases */

/* moves (ints, for the bots): build, survey, deliver a hand slot, deliver a public line, draw, keep, end */
#define RH_M_BUILD   0
#define RH_M_SURVEY  1000
#define RH_M_DELIVER 2000
#define RH_M_PUBLIC  2100
#define RH_M_DRAW    3000
#define RH_M_KEEP    3100
#define RH_M_END     4000

typedef struct {
    int w, h, towns;                            /* the map */
    int pts;                                    /* build points a turn */
    int track;                                  /* track pieces each player has */
    int rent;                                   /* coins a hex of another player's track used */
    int npub, pub_bonus;                        /* public lines face up; their bonus */
    int pub_refill;                             /* 1: a public line delivered is replaced */
    int deal, deal_keep;                        /* contracts dealt at the start; kept at least */
    int draw, draw_keep;                        /* drawn by the draw action; kept at least */
    int pen_pct;                                /* an undelivered contract costs this share of its value */
    int fog_pct;                                /* face-down share of the hexes not next to a town */
    int hamlets, hamlet_bonus;                  /* hamlets hidden under face-down hexes; their coins */
    int max_rounds;
    int value_add;                              /* a contract's value: its cheapest terrain cost (times value_pct %) plus this */
    int value_pct;
    int min_dist;                               /* contracts: towns at least this far apart */
    int town_dist;                              /* towns: at least this far apart */
    int start_coins;
    int deliver_cost;                           /* build points a delivery takes */
    int blind;                                  /* 1: face-down hexes may be built on (turned up first) */
    int reveal_adj;                             /* 1: building turns up the hexes next to it */
    int discard;                                /* 1: the contracts not kept leave the game */
    int deck_end;                               /* 1: the game also ends at the end of the round the deck runs out */
    int seat_coins;                             /* more for each seat after the first */
} rh_cfg;

typedef struct { uint8_t a, b, value; } rh_con;   /* a contract: two towns (indexes), its value */

typedef struct {
    int16_t coins;
    uint8_t track, done, pubs, nhand;
    uint8_t hand[RH_HAND];                      /* contract ids */
} rh_player;

typedef struct {
    rh_cfg cfg;
    int np, w, h, nc;
    uint8_t terr[RH_MAXC], hidden[RH_MAXC], hamlet[RH_MAXC];
    int8_t owner[RH_MAXC];                      /* -1: no track */
    int16_t nb[RH_MAXC][6];                     /* -1: off the map or sea */
    int8_t town_at[RH_MAXC];                    /* -1: no town */
    uint8_t town[RH_MAXT];
    int ntown;
    rh_con con[RH_MAXK];
    int ncon;
    uint8_t deck[RH_MAXK];                      /* the next contract to draw is deck[ndeck - 1] */
    int ndeck;
    uint8_t pdeck[RH_MAXK];                     /* the public lines still to come */
    int npdeck;
    int16_t pub[RH_PUB];                        /* -1: none */
    rh_player pl[RH_NP];
    int phase, turn, pts, acted, round, ending, passes;
    uint8_t offer[RH_OFFER];
    int noffer, keep_min;
    int last;                                   /* the last cell built (-1) */
    uint32_t seed;
} rh_state;

/* what a move did (the screen): reveals, a delivery's value and rent, a hamlet found */
typedef struct {
    int revealed[16], nrev;
    int con, value, rent, hamlet, ended, failed;
    int paid[RH_NP];                            /* rent paid to each player */
    int path[RH_MAXC], npath;                   /* a delivery's route (cells, town to town) */
} rh_result;

extern const int rh_cost_of[RH_TERRAINS];       /* build points (0: not buildable) */

void rh_config(rh_cfg *c, int players, int variant);
void rh_new(rh_state *s, const rh_cfg *c, int players, uint32_t seed);
int  rh_cost(const rh_state *s, int c);         /* build points on c (0: never) */
int  rh_can_build(const rh_state *s, int p, int c);   /* 1: p may build on c now (points too) */
int  rh_touches(const rh_state *s, int p, int c);     /* 1: c is next to p's track or a town */
/* a contract's route for player p: the cells (town to town) with the least rent; its rent (coins), -1 not linked */
int  rh_route(const rh_state *s, int p, int a, int b, int *path, int *npath);
int  rh_actor(const rh_state *s);               /* the seat to act */
int  rh_moves(const rh_state *s, int *out);     /* every legal move (RH_M_*) */
int  rh_legal(const rh_state *s, int m);
int  rh_play(rh_state *s, int m, rh_result *res);   /* 0: refused */
int  rh_over(const rh_state *s);
int  rh_final(const rh_state *s, int p);        /* p's final score (coins less undelivered contracts) */
unsigned rh_winners(const rh_state *s);         /* bit p: the best final score (ties: most delivered) */
int  rh_hex_dist(const rh_state *s, int a, int b);
/* shortest paths from src: w[c] the cost of entering c (< 0 blocked); dist (RH_FAR: unreached), prev (may be NULL) */
#define RH_FAR 0x3fffffff
void rh_paths(const rh_state *s, const int *w, int src, int *dist, int *prev);
int  rh_con_of(const rh_state *s, int a, int b);   /* the contract between two towns (-1) */

/* text */
int  rh_parse(const char *s, int *m);           /* one move's text; returns the characters read (0: bad) */
void rh_text(int m, char *out);                 /* at most 8 bytes */

/* the bot (railhills_bot.c): the move for the seat to act. level: 0 easy, 1 medium, 2 hard; -1 the greedy planner
 * without noise (the simulations) */
int  rh_bot_move(const rh_state *s, int level, uint64_t seed);
/* its pieces, for the simulations: the greedy planner's move, and the flat Monte Carlo search over plans */
int  rh_greedy_move(const rh_state *s, double noise, uint64_t seed);
int  rh_search_move(const rh_state *s, int rollouts, int horizon, double ms_cap, uint64_t seed);
/* the hidden things a seat cannot see dealt again at random (the others' hands, the decks, face-down hexes) */
void rh_determinise(rh_state *s, int seat, uint64_t seed);

#endif
