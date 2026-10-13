/*
 * Tile Rummy, for 2 to 4 players on one device: the rules. No SDL; the deal comes from a seed (cards.h's shuffle).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 106 tiles of the kit (cards.h, PACK_TILES): four colours of 1..13, twice, and two jokers. Each player draws 14;
 * the rest is the pool, face down. The sets on the table: a run, 3 or more numbers in a row of one colour (1 is not
 * after 13), or a group, 3 or 4 tiles of one number in different colours. A joker stands for any tile.
 * A turn: lay tiles from your rack and rearrange the table as you like (split sets, join them, move tiles between
 * them, win a joker back by putting its tile in its place): when the turn ends, every set on the table must be valid,
 * at least one of your tiles must have joined it, and no tile of the table may have gone to a rack. A player who lays
 * nothing (who cannot, or will not) draws one tile from the pool instead; the pool empty, they pass.
 * The first meld: until you have laid sets worth 30 or more in one turn, from your own tiles alone (a joker is worth
 * the tile it stands for), you may not touch the table's sets; after that turn, the table is yours too.
 * The round: the first player to empty their rack wins it and scores the tiles left in the others' racks (numbers at
 * their value, a joker 30), which the others lose. Blocked (the pool empty and everybody passing in turn): the lowest
 * rack wins; each other player loses the difference to it and the winner gains it (shared on a tie).
 * TR_ONE: one round. TR_MATCH: as many rounds as players, each one starting a round in turn (the first player moves
 * on one seat a round). The highest total wins (a tie: shared).
 * Notation (script; the screenshots and the tests): faces r b a k (red, blue, amber, black) and the number, j a joker,
 * t before a face for the table's copy (else the player's rack first); "r5+r6+r7>n" puts tiles in a new set, ">2" in
 * set 2 (0 is the first), ">r" back in the rack; e ends the turn, d draws (or passes), x resets the turn, u undoes the
 * last put, n the next round; a plays a plain turn, "#" plain turns to the end of the round, "*" of the game.
 */
#ifndef TILERUMMY_H
#define TILERUMMY_H

#include <stdint.h>

#define TR_TILES 106
#define TR_MAXP 4
#define TR_DEAL 14
#define TR_MAXSETS 48
#define TR_POOL 200                             /* where a tile is: a set (0..), the pool, a rack (TR_RACK + player) */
#define TR_RACK 210
#define TR_NEWSET 254                           /* tr_put's destinations: a set, a new set, the rack */
#define TR_TORACK 253
#define TR_FIRST 30
#define TR_UNDO 256
#define TR_NONE 255
#define TR_JOKER_PEN 30

enum { TR_ONE, TR_MATCH, TR_VARIANTS };
enum { TP_PLAY, TP_ROUND, TP_OVER };
enum { TS_BAD, TS_RUN, TS_GROUP };              /* a set */
enum { TC_OK, TC_NOTHING, TC_INVALID, TC_TOUCHED, TC_LOW };   /* can the turn end */

typedef struct {
    uint8_t variant, players, phase, turn, first, round;
    uint8_t where[TR_TILES];
    uint8_t nsets;
    uint8_t pool[TR_TILES], npool;              /* its top last */
    uint8_t out[TR_MAXP];                       /* the first meld made */
    uint8_t passes, winner;                     /* passes in a row (the pool empty); the round's winner (TR_NONE) */
    uint8_t lastplay[TR_TILES];                 /* 1: laid by the last turn */
    int16_t total[TR_MAXP], last[TR_MAXP];      /* the totals; the last round's points */
    uint32_t seed;
} tpos;

typedef struct {
    tpos pos;                                   /* the table as it is (the turn under way) */
    tpos start;                                 /* the turn's start */
    tpos undo[TR_UNDO];                         /* the turn's puts, to undo */
    int nundo;
    uint32_t seed;
    uint8_t variant, players;
} tgame;

int  tr_face(int tile);                          /* the tile's face (cards.h DECK_TILES) */
int  tr_is_joker(int tile);
int  tr_penalty(int tile);                       /* left in a rack: its number, a joker 30 */
void tr_new(tgame *g, int variant, int players, uint32_t seed);
/* a set's tiles in their order (a run's jokers in their places); its kind (TS_*) and worth */
int  tr_set(const tpos *p, int set, uint8_t *tiles, int *kind, int *value);
int  tr_rack(const tpos *p, int player, int by_number, uint8_t *tiles);   /* sorted: colour then number, or number */
int  tr_mine(const tgame *g, int tile);          /* 1: from the player's rack at the turn's start */
int  tr_changed(const tgame *g);                 /* 1: the table differs from the turn's start */
int  tr_laid(const tgame *g);                    /* the player's tiles on the table this turn */
int  tr_meld(const tgame *g);                    /* the first meld so far: the worth of the valid new sets */
int  tr_check(const tgame *g);                   /* TC_* */
int  tr_can_put(const tgame *g, int tile, int dest);
int  tr_put(tgame *g, const uint8_t *tiles, int n, int dest);   /* all or none; one step to undo; 0: not allowed */
int  tr_undo(tgame *g);                          /* the last put of the turn; 0: none */
int  tr_reset(tgame *g);                         /* back to the turn's start; 0: nothing to reset */
int  tr_end(tgame *g);                           /* ends the turn (TC_OK); 0: not allowed */
int  tr_draw(tgame *g);                          /* draws (or passes); only with the table untouched; 0: not */
int  tr_next(tgame *g);                          /* the next round */
int  tr_auto(tgame *g);                          /* a plain turn: lays what it finds, else draws */
int  tr_script(tgame *g, const char *moves);     /* 0: an action is bad */
unsigned tr_winners(const tpos *p);              /* the game over: the highest total (all tied for it) */

/* the screen's game (tilerummy.c), for its bot (tilerummy_bot.c) */
const tgame *tilerummy_live(void);             /* read-only: the bot copies it at entry */
int tilerummy_show(const char *move);           /* main thread: one action of the notation, as a tap plays it */

#endif
