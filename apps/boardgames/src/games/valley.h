/*
 * Valley Harvest for 2..4 players: the rules, without SDL (valley_rules.c; the screen: valley.c; the bot:
 * valley_bot.c). Each player grows a village in one shared valley of hexes. A turn: send a worker to a free land in
 * or next to your village and harvest it (a forest gives wood, a field grain, a hill stone: one, and one more for
 * each workshop of its kind beside it, anyone's, whose owner then earns a coin, and one more for a field by the
 * river), then build and trade with the market as you like. Roads stretch the village (the land stays free to
 * harvest), houses give a point and a worker, workshops points and their bonus, a manor on a house more points.
 * The middle of the valley starts face down: building beside it turns it up (ruins pay their finder). A round ends
 * when every worker is out; the valley then uses one of each good from the market (prices rise as stocks run low)
 * and the player with the fewest points (then coins) starts the next. After the last round, coins count too; the
 * most points wins.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The map: a hexagon of radius 4 in axial coordinates (61 cells, by rows from the top: r -4..4, q from left to
 * right); a game of two uses the inner radius 3 (37 cells). Directions 0..5: east, south-east, south-west, west,
 * north-west, north-east (pointy-top hexes, y down).
 *
 * Notation (script, the bots, the history), one action each: "h12" harvest cell 12 ("r" rest: no land in reach),
 * "R12" a road, "H12" a house, "S12" a sawmill, "M12" a mill, "Q12" a quarry, "V12" a manor (on a house of yours),
 * "+w" / "+g" / "+s" buy wood, grain, stone, "-w" / "-g" / "-s" sell one, "e" the end of the turn.
 */
#ifndef VALLEY_H
#define VALLEY_H

#include <stdint.h>

#define VL_RMAX 4
#define VL_CELLS 61
#define VL_PLAYERS 4
#define VL_GOODS 3
#define VL_VARIANTS 3                           /* 0: the whole game, 1: a short one, 2: the valley all face up */
#define VL_HIST 2400                            /* actions in a game, at most */
#define VL_STOCK_MAX 6

enum { VL_WOOD, VL_GRAIN, VL_STONE };
enum { VL_FOREST, VL_FIELD, VL_HILL, VL_RIVER, VL_OFF };          /* a cell's land (VL_OFF: outside the valley) */
enum { VL_NONE, VL_ROAD, VL_HOUSE, VL_SAW, VL_MILL, VL_QUARRY, VL_MANOR, VL_BLDS };
enum { VL_HARVEST, VL_ACT };                    /* the turn's two steps */

/* moves: a harvest is the cell (0..60), VL_REST when no land is in reach; building k at cell c is k * 64 + c;
 * the market's buys and sells; the end of the turn */
#define VL_REST 63
#define VL_MV_BUILD(k, c) ((k) * 64 + (c))
#define VL_MV_BUY(g) (1000 + (g))
#define VL_MV_SELL(g) (1010 + (g))
#define VL_MV_END 1020
#define VL_MAXMOVES 400

typedef struct {
    int8_t goods[VL_GOODS];
    int16_t coins;
    uint8_t workers, placed;                    /* workers this round; how many are out */
    int16_t pp;                                 /* points from buildings */
    uint8_t built[VL_BLDS];                     /* pieces built of each kind (the home not counted) */
} vl_player;

typedef struct {
    uint8_t players, radius, rounds;
    uint8_t terr[VL_CELLS];
    uint8_t hidden[VL_CELLS];                   /* 1: face down */
    uint8_t gift[VL_CELLS];                     /* ruins: 1 not found yet, 2 found */
    int8_t bld[VL_CELLS], own[VL_CELLS], wk[VL_CELLS];   /* a building and its owner; a worker's player (-1) */
    vl_player p[VL_PLAYERS];
    uint8_t stock[VL_GOODS];                    /* the market's */
    uint8_t round, start, cur, phase, over;     /* the round (0..), its first player, the player to move */
    uint8_t bought, sold;                       /* this turn: goods bought, sold (bits) */
    int16_t turns;
} vl_state;

/* the rules' numbers */
typedef struct {
    int rounds[VL_VARIANTS];
    int workers0, workers_max;
    int start_goods, start_coins;
    int cost[VL_BLDS][VL_GOODS];
    int pts[VL_BLDS];
    int supply[VL_BLDS];                        /* each player's pieces of a kind (0: no limit; the workshops share
                                                 * VL_SAW's) */
    int coins_per_pt;
    int stock0;
    int price[VL_STOCK_MAX + 1];                /* buying at stock s costs price[s] (s >= 1); selling earns the price
                                                 * of the stock after it, less spread (at least 1) */
    int spread, use;                            /* the valley uses `use` of each good a round */
    int river_bonus, shop_bonus, rent;
    int hidden, reveal_build, explore;          /* the middle face down; building beside it turns it up; a worker
                                                 * may turn up a face-down land in reach and harvest it */
    int gifts, gift_coins;
    int catchup;                                /* 1: the fewest points (then coins) starts a round; 0: in turn */
    int manor_upgrade;                          /* 1: a manor is built on a house of the player's */
} vl_rules;
#ifdef VL_TUNE
extern vl_rules VL;                             /* the simulations' tuning */
#else
extern const vl_rules VL;
#endif

/* what one action did (for the screen) */
typedef struct {
    int player, kind;                           /* kind: 0 harvest, 1 rest, 2 build, 3 buy, 4 sell, 5 end */
    int cell, good, amount;                     /* a harvest's cell, good and amount; a trade's good and coins */
    int rent[VL_PLAYERS];                       /* coins each owner earned from the workshops */
    int rented[6], nrented;                     /* the workshops that paid */
    int revealed[6], nrev, gift;                /* cells turned up; ruins found (coins) */
    int round_end, game_end;
} vl_effect;

extern int vl_nb[VL_CELLS][6];                  /* -1: off the map (set up before main, read only) */
extern int8_t vl_q[VL_CELLS], vl_r[VL_CELLS];
int  vl_cell(int q, int r);                     /* -1: off */
int  vl_dist(int c);                            /* steps from the middle */
int  vl_in(const vl_state *s, int c);
void vl_new(vl_state *s, int players, int variant, uint32_t seed);
int  vl_moves(const vl_state *s, int *out);     /* VL_MAXMOVES */
int  vl_legal(const vl_state *s, int m);
void vl_make(vl_state *s, int m);
void vl_apply(vl_state *s, int m, vl_effect *e);   /* the same, telling what it did (e may be NULL) */
int  vl_harvestable(const vl_state *s, int c, int p);
int  vl_can_build(const vl_state *s, int p, int k, int c);  /* the place (and the supply); not the cost */
int  vl_can_pay(const vl_state *s, int p, int k);
int  vl_left(const vl_state *s, int p, int k);  /* pieces of kind k still in hand (-1: no limit) */
int  vl_yield(const vl_state *s, int c);        /* a harvest there now */
int  vl_reach(const vl_state *s, int p, int c); /* c is in or next to p's village */
int  vl_buy_price(const vl_state *s, int g);    /* 0: none in stock */
int  vl_sell_price(const vl_state *s, int g);   /* 0: the market is full */
int  vl_score(const vl_state *s, int p);        /* points now: buildings + coins */
unsigned vl_winners(const vl_state *s);         /* bit p: the best (more coins break a tie) */
int  vl_shop_of(int terr);                      /* the workshop that helps a land (VL_NONE) */

/* notation */
int  vl_parse(const char *s, int *len);         /* a move, or -1 */
void vl_name(int m, char *out);                 /* 6 bytes */

/* a game: its first state and its actions (undo replays them) */
typedef struct {
    vl_state s, s0;
    int16_t mv[VL_HIST];
    int n, floor;                               /* actions played; undo stops there (a land turned up) */
} vl_game;
void vl_begin(vl_game *g, int players, int variant, uint32_t seed);
int  vl_play(vl_game *g, int m, vl_effect *e);  /* 0: illegal */
int  vl_undo(vl_game *g);                       /* 0: nothing to undo */
int  vl_script(vl_game *g, const char *moves);  /* 0: a bad or illegal move (those before it played) */

/* the bot (valley_bot.c): the action for the player to move at a level (AI_EASY..AI_HARD; -1: none); the screen
 * (valley.c; a headless test defines them) gives it the live game, read only, and plays its move as a tap would */
int  vlb_choose(const vl_state *s, int level, uint64_t seed);
const vl_game *valley_live(void);
int  valley_show(const char *move);

#endif
