/*
 * Ludo for two to four players on one device, and its French cousin the petits chevaux: the rules (the track, the
 * yards, the home columns or the staircases, the captures, the throws). No SDL, no dice: the caller throws and gives
 * the rules the value.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The track: 52 squares, numbered clockwise from player 0's start square (0). The four yards, clockwise from player
 * 0's (seats 0..3): player 0, 2, 1, 3; each seat starts 13 squares after the one before (player 1 opposite player 0,
 * at 26, as with two players). The turn goes round the table, clockwise: 0, 2, 1, 3 (two players: 0, 1; three: 0, 2,
 * 1). A token's place is counted from its own start square: -1 in its yard, 0..50 on the track, then its own column.
 * The first to bring all four tokens home wins, and the game ends there (with three or four players too).
 * Ludo (LUDO_CLASSIC), the common, simple rules: 51..55 its home column, 56 home (the centre).
 * - Only a 6 brings a token out of its yard, onto its start square (a 6 may also move a token on the board).
 * - A 6 gives another throw; the third 6 in a row is lost and the turn ends.
 * - Landing on other players' tokens sends them back to their yards, except on a safe square: the four start squares
 *   and the four stars (8 squares after each start), where tokens of all players may stand together.
 * - Two tokens of one player on one square do not block: anyone may pass them or land there (a capture takes both).
 * - After a full lap a token turns into its own home column and reaches the centre with the exact number only.
 * - A throw with no legal move is lost (after a 6, the player still throws again).
 * Petits chevaux (LUDO_CHEVAUX), the common French rules: 50 the foot of its staircase, 51..56 the steps 1..6, 57 the
 * centre.
 * - Only a 6 brings a horse out of its stable, onto its start square; every 6 gives another throw (no limit).
 * - No horse may pass another (of any player), nor stop on one of its own: one horse a square. Stopping on another
 *   player's horse sends it back to its stable (coming out too); there is no safe square.
 * - A horse must stop exactly at the foot of its staircase (a bigger throw cannot move it), then climbs one step at a
 *   time with the step's number (a 1 to step 1, a 2 to step 2... a 6 to step 6), and a 6 again to the centre.
 * - A throw with no legal move is lost (after a 6, the player still throws again).
 * Notation (script, the screenshots and the tests): a digit is a throw, a letter a..d moves that token of the player
 * to move; a throw with no move needs nothing more: "6 a 4 a 3 6 b 2 b".
 */
#ifndef LUDO_H
#define LUDO_H

#include <stdint.h>

#define LUDO_PLAYERS 4
#define LUDO_TOKENS 4
#define LUDO_TRACK 52
#define LUDO_COLUMN 51                          /* the first square of the home column (or step 1) */
#define LUDO_HOME 56                            /* Ludo: the centre */
#define LUDO_STAR 8                             /* a star 8 squares after each start square */
#define LUDO_FOOT 50                            /* petits chevaux: the foot of the staircase (the lap's last square) */
#define LUDO_STEPS 6                            /* its steps, 51..56 */
#define CHEVAUX_HOME (LUDO_FOOT + LUDO_STEPS + 1)   /* petits chevaux: the centre */
#define LUDO_MAX_EVENTS 4096                    /* throws and moves: far more than a game lasts */
#define LUDO_MOVE_EV 16                         /* an event: 1..6 a throw, LUDO_MOVE_EV + token a move */

enum { LUDO_CLASSIC, LUDO_CHEVAUX, LUDO_VARIANTS };
enum { LUDO_THROW, LUDO_MOVE, LUDO_OVER };      /* what comes next: a throw, a token to move, nothing */
enum { LT_MOVE = 1, LT_AGAIN, LT_PASS, LT_SIXES };  /* a throw: choose a token; no move but a 6 (throw again);
                                                 * no move (the turn passes); the third 6 (the turn passes) */

typedef struct {
    int8_t at[LUDO_PLAYERS][LUDO_TOKENS];       /* each token's place, from its own start square (-1: the yard) */
    uint8_t turn, phase, die, sixes;            /* the player to move, LUDO_THROW / MOVE / OVER, the last throw,
                                                 * the 6s thrown in a row this turn */
    uint8_t variant, players;                   /* LUDO_CLASSIC / LUDO_CHEVAUX; how many play (2..4) */
    int8_t winner;                              /* -1 */
    uint16_t throws;                            /* throws since the start */
} lpos;

/* A game: the position before every event, so undo is free. */
typedef struct {
    lpos pos, past[LUDO_MAX_EVENTS];
    uint8_t ev[LUDO_MAX_EVENTS];
    int n;
} lgame;

void ludo_new(lgame *g, int variant, int players);
int  ludo_goal(int variant);                    /* the centre: LUDO_HOME or CHEVAUX_HOME */
int  ludo_seat(int player);                     /* the player's yard, clockwise from player 0's: 0, 2, 1, 3 */
int  ludo_square(int player, int at);           /* the track square (0..51) of a token at `at`; -1 off the track */
int  ludo_safe(int square);                     /* Ludo: a start square or a star */
int  ludo_dest(const lpos *p, int token);       /* where the player to move's token goes with the die; -1: nowhere */
int  ludo_hits(const lpos *p, int token);       /* the tokens that move sends back (bit 4 player + token) */
int  ludo_movable(const lpos *p);               /* the tokens that can move (bit i: token i) */
int  ludo_throw(lgame *g, int value);           /* LT_*; 0: not now */
int  ludo_move(lgame *g, int token);            /* 0: not allowed; else 1 + the tokens it sent home */
int  ludo_undo(lgame *g);                       /* back to the last move's choice (with the throws after it); 0: none */
int  ludo_home(const lpos *p, int player);      /* the player's tokens home */
int  ludo_script(lgame *g, const char *moves);  /* plays throws and moves in the notation above; 0: one is bad */

/* the bot (ludo_bot.c, bots.h) */
const lgame *ludo_live(void);           /* the game on the screen, read-only: the bot copies its position at entry */
/* main thread: plays a move as a tap does (animated): "roll" throws the die (the game's own), a letter moves that
 * token; 0: not now, or not legal */
int  ludo_show(const char *move);

#endif
