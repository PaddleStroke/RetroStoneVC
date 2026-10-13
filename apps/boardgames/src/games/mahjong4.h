/*
 * Mahjong, the table game for four players on one device: the rules (Hong Kong Old Style). No SDL; the walls come
 * from a seed (cards.h's shuffle), so a hand is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The set: 144 tiles, each of the 34 kinds of the suits (dots, bamboo, characters 1..9), the winds (east, south,
 * west, north) and the dragons (red, green, white) four times, and the four flowers and four seasons once (kinds
 * numbered as Mahjong Solitaire's faces, games/mahjong.h: dots 0..8, bamboo 9..17, characters 18..26, winds 27..30,
 * dragons 31..33, flowers 34..37, seasons 38..41). A tile is its id 0..143: ids 0..135 are kind id / 4, 136..143
 * the flowers and seasons.
 * The players: 0..3 in turn order (counterclockwise round the table). Player 0 deals the first hand (east); a
 * player's seat wind is east for the dealer, then south, west, north in turn order.
 * A hand: the walls (four of 18 stacks, two tiles high) are broken where the dealer's three dice say; 13 tiles each,
 * the dealer 14. The last 14 tiles of the wall are the dead wall: the flowers and seasons (set aside as they come)
 * and the kongs are replaced from its far end, and it takes the live wall's last tile each time, so it keeps 14.
 * A turn: draw (or claim the last discard), then discard one tile; or declare a kong (four of a kind: concealed, or
 * added to a pung of one's own; then a replacement) or a win.
 * Claims on a discard: a win (a complete hand: four sets and a pair, or seven pairs, or thirteen orphans, worth at
 * least M4_MIN faan) before a pung or a kong, before a chow (only the next player may chow). Several wins: the first
 * player after the discarder in turn order takes it. A claimed tile makes an exposed set; the claimer discards (a
 * kong: after its replacement); play goes on after them. An added kong can be robbed: a player who wins with that
 * tile takes it. The live wall used up: the last discard can only be won; else the hand is drawn.
 * The claims' order on the device (the hands are secret: each player decides alone): after a discard, if a player
 * two or three seats after the discarder can claim it (pung, kong or win), both of them are asked, always in that
 * order (so the device's way round does not tell which of them could); then the next player (whose turn it is
 * anyway: claim, or draw) unless a claim already made beats anything they could do. An added kong that someone could
 * rob: all three others are asked, in turn order.
 * Scoring: the faan of the winning hand (the best reading of it), capped at M4_LIMIT; points: M4_POINTS[faan]. On a
 * discard, the discarder pays twice the points, the two others once; self-drawn, all three pay twice the points.
 * The dealer keeps the deal after winning; else it passes on (a drawn hand too). A round: each player deals at least
 * once; M4_EAST: the east round only; M4_FULL: the east and the south rounds. The highest total wins.
 * Notation (script; the screenshots and the tests): tiles 1o..9o (dots), 1b..9b (bamboo), 1c..9c (characters), E S W
 * N, R G P (the red, green and white dragon), F1..F4 S1..S4 (flowers, seasons). x<tile> discard (the drawn tile if it
 * is of that kind, else the first of that kind), w win, u pung, k kong of the discard, K<tile> a kong of one's own,
 * c<tile> chow from that tile up, p pass (the next player: draw), t the first choice offered (a win, a kong, a pung,
 * a chow), n the next hand, a one plain action; plain actions on: "?" to the next choice (a claim, a kong, a win),
 * "!" to the next win, "#" to the end of the hand, "*" to the end of the game.
 */
#ifndef MAHJONG4_H
#define MAHJONG4_H

#include <stdint.h>

#define M4_P 4
#define M4_TILES 144
#define M4_KINDS 34                             /* the kinds that make sets (no flowers nor seasons) */
#define M4_NONE 255
#define M4_DEAD 14                              /* the dead wall */
#define M4_STACKS 72                            /* the wall's stacks, two tiles high */
#define M4_HAND 14
#define M4_POND 40
#define M4_ACTS 1024                            /* a hand's actions */
#define M4_MIN 3                                /* the least faan to win */
#define M4_LIMIT 10                             /* the most */

enum { M4_DOTS = 0, M4_BAMBOO = 9, M4_CHARS = 18, M4_WINDS = 27, M4_DRAGONS = 31, M4_FLOWERS = 34, M4_SEASONS = 38,
       M4_ALLKINDS = 42 };
enum { M4_EAST, M4_FULL, M4_VARIANTS };         /* the game's length: the east round; the east and south rounds */
/* the phases: the turn's player holds 14 (discard, a kong of their own, a win); a discard's claims; an added kong
 * that could be robbed; the hand over (its summary: next); the game over */
enum { MP_DISCARD, MP_CLAIM, MP_ROB, MP_SCORE, MP_OVER };
/* an action: its kind, an argument (a tile id, a kind) */
enum { MA_DISCARD = 1 << 8, MA_WIN = 2 << 8, MA_KONG = 3 << 8, MA_PUNG = 4 << 8, MA_CHOW = 5 << 8, MA_PASS = 6 << 8,
       MA_NEXT = 7 << 8 };
#define MA_KIND(a) ((a) & 0xff00)
#define MA_ARG(a) ((a) & 0xff)
/* a player's options on a discard (bits) */
enum { MO_WIN = 1, MO_PUNG = 2, MO_KONG = 4, MO_CHOW = 8 };
/* a set: a meld (exposed, or a concealed kong), or a part of a winning hand */
enum { MS_CHOW, MS_PUNG, MS_KONG, MS_PAIR };
/* the scoring items */
enum {
    MI_SELF_DRAWN, MI_CONCEALED, MI_ALL_CHOWS, MI_DRAGON_PUNG, MI_SEAT_WIND, MI_ROUND_WIND, MI_OWN_FLOWER,
    MI_NO_FLOWERS, MI_FLOWER_SET, MI_LAST_TILE, MI_REPLACEMENT, MI_ROBBING, MI_ALL_PUNGS, MI_HALF_FLUSH,
    MI_SEVEN_PAIRS, MI_SMALL_DRAGONS, MI_FULL_FLUSH, MI_BIG_DRAGONS,
    /* the limit hands */
    MI_THIRTEEN, MI_ALL_HONOURS, MI_ALL_TERMINALS, MI_SMALL_WINDS, MI_BIG_WINDS, MI_NINE_GATES, MI_FOUR_KONGS,
    MI_FOUR_CONCEALED, MI_ITEMS
};
extern const int M4_POINTS[M4_LIMIT + 1];       /* the points of a hand of n faan */

static inline int m4_kind(int id) { return id < 136 ? id / 4 : M4_FLOWERS + id - 136; }
static inline int m4_bonus(int kind) { return kind >= M4_FLOWERS; }
static inline int m4_honour(int kind) { return kind >= M4_WINDS && kind < M4_FLOWERS; }
static inline int m4_terminal(int kind) { return kind < M4_WINDS && (kind % 9 == 0 || kind % 9 == 8); }
static inline int m4_suit(int kind) { return kind < M4_WINDS ? kind / 9 : -1; }

typedef struct {
    uint8_t type, kind, n, from;                /* MS_*; the kind (a chow: its lowest); 3 or 4 tiles; claimed from
                                                 * (M4_NONE: a concealed kong) */
    uint8_t tile[4];                            /* the tiles' ids */
    uint8_t claimed;                            /* the claimed tile's id (M4_NONE) */
} m4meld;

/* a winning hand to score, by kinds (the tests build these) */
typedef struct {
    uint8_t conc[M4_HAND];                      /* the concealed tiles' kinds, the winning tile with them */
    int nconc;
    struct { uint8_t type, kind, concealed; } meld[4];
    int nmeld;
    uint8_t flower[8];                          /* the flowers' and seasons' kinds */
    int nflower;
    int seat, wind;                             /* the winner's seat wind, the prevailing wind (0 east .. 3 north) */
    int win_kind, self_drawn, last, replacement, robbing;
} m4hand;

typedef struct {
    int faan, raw;                              /* capped at M4_LIMIT; before the cap */
    int n;
    uint8_t item[16], value[16];                /* the items scored (MI_*), their faan */
    int nset;                                   /* the reading: the concealed part's sets and pair (seven pairs:
                                                 * seven pairs; thirteen orphans: none) */
    uint8_t set_type[7], set_kind[7];
} m4score;

/* scores a complete hand: its faan (the best reading), or -1 (not complete) */
int  m4_score_hand(const m4hand *h, m4score *s);

typedef struct {
    uint8_t winner, payer;                      /* M4_NONE: a drawn hand; payer M4_NONE: self-drawn */
    uint8_t tile;                               /* the winning tile */
    m4score score;
    int32_t pay[M4_P];                          /* the points each player won or paid */
} m4result;

typedef struct {
    uint8_t variant, wind, dealer, streak;      /* the prevailing wind (the round); the dealer; their wins in a row */
    uint16_t hands;                             /* hands dealt so far */
    uint32_t seed;
    int32_t total[M4_P];
    /* the hand */
    uint8_t wall[M4_TILES];                     /* in the order they are drawn: the live end at head, the dead wall's
                                                 * far end at tail - 1 */
    int16_t head, tail;
    uint8_t dice[3], start;                     /* the dealer's dice; the stack the deal starts from (clockwise
                                                 * round the table from the front right of player 0's wall) */
    uint8_t hand[M4_P][M4_HAND], nhand[M4_P];   /* the concealed tiles */
    m4meld meld[M4_P][4];
    uint8_t nmeld[M4_P];
    uint8_t flower[M4_P][8], nflower[M4_P];
    uint8_t pond[M4_P][M4_POND], npond[M4_P];   /* the discards still lying there */
    uint8_t phase, turn, drawn;                 /* drawn: the tile just drawn (M4_NONE: a claim, nothing drawn) */
    uint8_t replacement;                        /* the drawn tile came from the dead wall */
    uint8_t discards;                           /* tiles discarded in this hand */
    /* a claim window (MP_CLAIM: the discard; MP_ROB: the tile added to a kong) */
    uint8_t tile, from;                         /* the tile; the discarder (the konger) */
    uint8_t ask[M4_P], nask, k;                 /* the players asked, in order; ask[k] decides now */
    uint8_t opts[M4_P];                         /* each player's options (MO_*) */
    uint8_t pend;                               /* the best claim made so far: its player (M4_NONE) */
    uint16_t pend_act;
    m4result result;                            /* MP_SCORE, MP_OVER: the last hand's */
} m4pos;

typedef struct {
    m4pos pos, start;                           /* now; at the start of this hand (undo replays from there) */
    uint16_t act[M4_ACTS];                      /* this hand's actions */
    int n;
} m4game;

void m4_new(m4game *g, int variant, uint32_t seed);
int  m4_actor(const m4pos *p);                  /* the player to act (MP_SCORE: the next dealer; MP_OVER: -1) */
int  m4_seat(const m4pos *p, int player);       /* the player's seat wind (0 east .. 3 north) */
int  m4_live(const m4pos *p);                   /* tiles left in the live wall */
int  m4_legal(const m4pos *p, int act);
int  m4_play(m4game *g, int act);               /* 0: not allowed */
int  m4_undo(m4game *g);                        /* back one action of this hand (a replay); 0: none */
/* the options of the player asked now (MP_CLAIM, MP_ROB; those beaten by a claim already made left out); the chows
 * (their lowest kinds) into chows[3] */
int  m4_options(const m4pos *p, int *chows, int *nchow);
/* the player's concealed tiles plus `kind` (M4_NONE: none) make a winning hand worth at least M4_MIN: its faan (the
 * score into s), else -1; self: self-drawn (else on a discard), rob: robbing a kong */
int  m4_wins(const m4pos *p, int player, int kind, int self, int rob, m4score *s);
int  m4_kongs(const m4pos *p, int *kinds);      /* MP_DISCARD: the kongs the player may declare (kinds); how many */
int  m4_auto(const m4pos *p);                   /* a plain action */
int  m4_script(m4game *g, const char *moves);   /* 0: an action is bad */
int  m4_parse_tile(const char **s);             /* a tile's kind in the notation (moves s on); -1: none */
unsigned m4_winners(const m4pos *p);            /* the game over: the highest total (all tied for it) */

/* the screen's game (mahjong4.c), for its bot (mahjong4_bot.c) */
const m4game *mahjong4_live(void);              /* read-only: the bot copies it at entry */
int mahjong4_show(const char *move);            /* main thread: one action of the notation, as a tap plays it */

#endif
