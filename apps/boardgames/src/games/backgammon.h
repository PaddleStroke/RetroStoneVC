/*
 * Backgammon's rules (no SDL), and three of its family on the same board: plakoto, fevga (Greek tavli) and long nardy.
 * 24 points, 15 checkers each. The opening roll (one die each, the higher plays first, a tie throws again), then a
 * turn is two dice (doubles: four moves of that number). Both numbers must be used if they can be, else the larger
 * one if only one can; a throw with no move passes. Once all 15 are home (the player's points 1..6) they bear off (a
 * higher number takes the highest checker when none is higher still); the first to bear off all 15 wins. No doubling
 * cube. The dice are input (the shell throws them): the tests choose them.
 *  - Backgammon: the standard start (2 on 24, 5 on 13, 3 on 8, 5 on 6); the players go opposite ways. The opening's
 *    winner plays the two dice of the opening. A checker lands on an empty point, its own, or a single enemy checker
 *    (a blot), which is hit and goes to the bar; checkers on the bar enter first, in the other player's home board.
 *    1 point, a gammon 2 (the loser bore off none), a backgammon 3 (and still has a checker on the bar or in the
 *    winner's home board).
 *  - Plakoto: all 15 start on the player's 24 (the other's 1 point); opposite ways. No hitting: a checker landing on
 *    a single enemy checker pins it there (it may not move until the pinner leaves; the point is the pinner's, who
 *    may add more). The mother (the last checker on a player's 24): pinned there, its player loses 2 points, unless
 *    the pinner still has checkers on their own 24 (the pin waits; it counts once they have none); both mothers
 *    pinned: a draw. 1 point, 2 if the loser bore off none.
 *  - Fevga: all 15 on the player's 24, the other's start diagonally across (the other's 24 is the player's 12); both
 *    go the same way. No hitting: a single checker holds a point. A second checker may leave the start only once the
 *    first has passed the other's start (the player's 11 or lower). No six points in a row in the player's own
 *    starting table (19..24), and no six in a row anywhere with all 15 of the other's checkers behind them. 1 point,
 *    2 if the loser bore off none.
 *  - Long nardy: the board and the ways of fevga, all 15 on the head (the player's 24). One checker a turn may leave
 *    the head, two on a player's first throw if it is 6-6, 4-4 or 3-3. No hitting: a single checker holds a point. No
 *    six points in a row with all 15 of the other's checkers behind them. 1 point (oyn), 2 if the loser bore off none
 *    (mars).
 *  The three: the opening's winner throws again for the first move.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ivory, 1 ebony. Each counts the points from their own side, as in the usual notation: 1..24, the home
 * board 1..6, the bar 25, borne off 0; a player's point p is the other player's point bg_opp(variant, p) (25 - p, or
 * p +- 12 the same way round).
 *
 * The notation (bg_moves: the tests, the screenshots): a throw "31:", then its moves "8/5 6/5", the mover's own point
 * numbers, "bar" and "off", a hit (or a pin) marked "*" or not ("13/8*"); a checker going on with the next die
 * "24/18/13" or "24/13", the same move twice "13/7(2)". The opening throw: ivory's die then ebony's ("31:" ivory
 * plays 3 and 1 (the others: ivory throws next), "13:" ebony does, "44:" a tie, throw again). A throw with no move is
 * just "64:"; the turn passes by itself.
 *   "31: 8/5 6/5 64: 24/18 13/9 ..."
 */
#ifndef BACKGAMMON_H
#define BACKGAMMON_H

#define BG_BAR 25
#define BG_OFF 0
#define BG_MAX 2048                             /* the events kept for undo (a long game drops its first ones) */
#define BG_MAXMOVES 40                          /* the one-die moves of a position, at most */

enum { BG_BACKGAMMON, BG_PLAKOTO, BG_FEVGA, BG_NARDY, BG_VARIANTS };
enum { BG_OPENING, BG_THROW, BG_MOVE, BG_OVER };   /* the phase: the opening roll, a throw, the moves, the end */
enum { BG_PLAY, BG_WIN0, BG_WIN1, BG_DRAW };
enum { BG_EV_THROW, BG_EV_MOVE };

typedef struct { signed char from, to, die, hit; } bmove;   /* die: which of the dice (0..3); hit: or a pin */

typedef struct {
    signed char n[2][26];                       /* n[p][i]: player p's checkers on their point i (25 the bar, 0 off) */
    signed char pin[2][26];                     /* plakoto: 1, player p's checker on their point i is pinned */
    signed char variant;
    signed char side, phase;                    /* the player to throw or to move (the winner at the end) */
    signed char die[4], nd;                     /* the turn's dice (doubles: four), nd of them */
    unsigned char used;                         /* bit i: die i played */
    signed char passed;                         /* the last throw had no move: that player passed (else -1) */
    signed char head;                           /* long nardy: the checkers that left the head this turn */
    signed char throws[2];                      /* each player's throws so far (1: their first turn), up to 2 */
    signed char tie;                            /* over: a draw (plakoto: both mothers pinned) */
} bpos;

typedef struct {
    signed char kind, player;                   /* BG_EV_*; the thrower (-1: the opening) or the mover */
    bmove m;
} bevent;

typedef struct {
    bpos pos[BG_MAX + 1];                       /* pos[i]: before event i; pos[n] now */
    bevent ev[BG_MAX];
    int n;
    int forgot;                                 /* the events forgotten before ev[0] (a long game) */
} bgame;

int bg_opp(int variant, int point);             /* the other player's number for one of the player's points 1..24 */
int bg_legal(const bpos *p, bmove *out);        /* the moves the player to move may make with one die now */
int bg_find(const bpos *p, int from, int to, bmove *m);   /* the legal move from -> to (the smaller die); 0: none */
int bg_pips(const bpos *p, int player);         /* the pip count */
int bg_points(const bpos *p);                   /* the winner's points (1..3), 0 while the game goes on or a draw */

void bg_new(bgame *g, int variant);
int bg_throw(bgame *g, int a, int b);           /* 0: not the time to throw (opening: a ivory's die, b ebony's) */
int bg_play(bgame *g, bmove m);                 /* 0: not a legal move */
int bg_undo(bgame *g);                          /* back before the last checker move (0: none); its throw comes back */
int bg_status(const bgame *g);                  /* BG_PLAY, BG_WIN*, BG_DRAW */
int bg_moves(bgame *g, const char *s);          /* plays the notation; 0: something not legal */
/* event i since the start (counting those forgotten: g->forgot + g->n of them) in that notation ("31:" a throw,
 * "13/10" a checker, "bar/22" from the bar, "3/off" borne off), in cap bytes (12 are enough); 0: forgotten or no room */
int bg_event_text(const bgame *g, int i, char *out, int cap);
static inline const bpos *bg_pos(const bgame *g) { return &g->pos[g->n]; }

/* the bot (backgammon_bot.c, bots.h) */
int bg_singles(const bpos *p, int di, bmove *out);   /* die di's moves, before the rule of the most dice */
void bg_apply(bpos *p, bmove m);                /* a move on the position alone (the turn does not pass) */
int bg_settle(bpos *p);                         /* after a move: 1, the game over (the winner in side, a draw: tie) */
const bgame *backgammon_live(void);             /* the game on the screen, read-only: the bot copies its position */
/* main thread: plays a move as a tap does (animated): "roll" throws the dice (the game's own), "13/8" (or "bar/22",
 * "6/off", a hit marked "13/8*" or not) moves one checker by one die; 0: not now, or not legal */
int backgammon_show(const char *move);

#endif
