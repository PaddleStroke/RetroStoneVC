/*
 * Backgammon's rules (no SDL): 24 points, 15 checkers each, the standard start. The opening roll (one die each, the
 * higher plays first with both numbers, a tie throws again), then a turn is two dice (doubles: four moves of that
 * number). A checker lands on an empty point, its own, or a single enemy checker (a blot), which is hit and goes to
 * the bar; checkers on the bar enter first, in the other player's home board. Both numbers must be used if they can
 * be, else the larger one if only one can; a throw with no move passes. Once all 15 are home they bear off (a higher
 * number takes the highest checker when none is higher still); the first to bear off all 15 wins: 1 point, a gammon
 * 2 (the loser bore off none), a backgammon 3 (and still has a checker on the bar or in the winner's home board).
 * No doubling cube. The dice are input (the shell throws them): the tests choose them.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ivory, 1 ebony. Each counts the points from their own side, as in the usual notation: 1..24, the home
 * board 1..6, the bar 25, borne off 0; a player's point p is the other player's point 25 - p. The start, for each:
 * 2 on 24, 5 on 13, 3 on 8, 5 on 6.
 *
 * The notation (bg_moves: the tests, the screenshots): a throw "31:", then its moves "8/5 6/5", the mover's own point
 * numbers, "bar" and "off", a hit marked "*" or not ("13/8*"); a checker going on with the next die "24/18/13" or
 * "24/13", the same move twice "13/7(2)". The opening throw: ivory's die then ebony's ("31:" ivory plays 3 and 1,
 * "13:" ebony does, "44:" a tie, throw again). A throw with no move is just "64:"; the turn passes by itself.
 *   "31: 8/5 6/5 64: 24/18 13/9 ..."
 */
#ifndef BACKGAMMON_H
#define BACKGAMMON_H

#define BG_BAR 25
#define BG_OFF 0
#define BG_MAX 2048                             /* the events kept for undo (a long game drops its first ones) */
#define BG_MAXMOVES 40                          /* the one-die moves of a position, at most */

enum { BG_OPENING, BG_THROW, BG_MOVE, BG_OVER };   /* the phase: the opening roll, a throw, the moves, the end */
enum { BG_PLAY, BG_WIN0, BG_WIN1 };
enum { BG_EV_THROW, BG_EV_MOVE };

typedef struct { signed char from, to, die, hit; } bmove;   /* die: which of the dice (0..3) it uses */

typedef struct {
    signed char n[2][26];                       /* n[p][i]: player p's checkers on their point i (25 the bar, 0 off) */
    signed char side, phase;                    /* the player to throw or to move (the winner at the end) */
    signed char die[4], nd;                     /* the turn's dice (doubles: four), nd of them */
    unsigned char used;                         /* bit i: die i played */
    signed char passed;                         /* the last throw had no move: that player passed (else -1) */
} bpos;

typedef struct {
    signed char kind, player;                   /* BG_EV_*; the thrower (-1: the opening) or the mover */
    bmove m;
} bevent;

typedef struct {
    bpos pos[BG_MAX + 1];                       /* pos[i]: before event i; pos[n] now */
    bevent ev[BG_MAX];
    int n;
} bgame;

int bg_legal(const bpos *p, bmove *out);        /* the moves the player to move may make with one die now */
int bg_find(const bpos *p, int from, int to, bmove *m);   /* the legal move from -> to (the smaller die); 0: none */
int bg_pips(const bpos *p, int player);         /* the pip count */
int bg_points(const bpos *p);                   /* the winner's points (1..3), 0 while the game goes on */

void bg_new(bgame *g);
int bg_throw(bgame *g, int a, int b);           /* 0: not the time to throw (opening: a ivory's die, b ebony's) */
int bg_play(bgame *g, bmove m);                 /* 0: not a legal move */
int bg_undo(bgame *g);                          /* back before the last checker move (0: none); its throw comes back */
int bg_status(const bgame *g);                  /* BG_PLAY, BG_WIN* */
int bg_moves(bgame *g, const char *s);          /* plays the notation; 0: something not legal */
static inline const bpos *bg_pos(const bgame *g) { return &g->pos[g->n]; }

#endif
