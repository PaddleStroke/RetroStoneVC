/*
 * The Game of the Goose (le jeu de l'oie) for two to six players on one device: the rules. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The classic French board: 63 squares in a spiral, the pawns all starting off it (square 0, the start). Two dice;
 * the pawn moves by their sum. The squares:
 * - the geese, 5 9 14 18 23 27 32 36 41 45 50 54 59 (every nine squares, in two series): the pawn moves again by the
 *   same throw, in the direction it was going (backwards after bouncing off 63), goose after goose;
 * - the bridge, 6: on to 12; the maze, 42: back to 30; death, 58: back to the start;
 * - the inn, 19: the player misses their next two turns;
 * - the well, 31, and the prison, 52: the pawn stays there until another pawn comes to the same square and frees it;
 * - 63, the goose's garden: reached by the exact throw wins; a bigger throw goes to 63 and back by the extra points.
 * The opening throws: a 9 thrown from the start (a pawn's first throw, or after death sent it back) does not go to 9
 * (a goose, and goose after goose to the garden): 6 and 3 go to 26, 5 and 4 to 53 (the squares with the dice).
 * Two pawns never share a square (but the start): a pawn ending its move on a taken square sends the pawn there to
 * the square the mover started from (a swap); the pawn sent back takes no effect of the square it lands on, and is no
 * longer held (the well and the prison free it so, the inn too). Pawns passing over others do nothing.
 * With two players, when both pawns are held, one in the well and the other in the prison, nobody can free them: the
 * next to throw is freed and throws.
 * Player 0 throws first, then 1, 2... round the table, skipping the players who must wait. The first on 63 wins.
 * The notation (script, the screenshots, the tests): the throws in order, two digits each ("63 45 11").
 */
#ifndef GOOSE_H
#define GOOSE_H

#include <stdint.h>

#define GO_LAST 63
#define GO_PLAYERS 6
#define GO_MAX_THROWS 8192
#define GO_STEPS 160
#define GO_HELD (-1)                            /* wait[]: held in the well or the prison until freed */

enum { GK_PLAIN, GK_GOOSE, GK_BRIDGE, GK_INN, GK_WELL, GK_MAZE, GK_PRISON, GK_DEATH, GK_GARDEN, GK_DICE };
enum { GO_HOP, GO_GLIDE, GO_FLY };              /* how the pawn reaches a step's square: one square on, along the
                                                 * spiral (the bridge, the maze), through the air (death, 26, 53) */
enum { GO_PLAY, GO_WIN0 };                      /* go_status: GO_WIN0 + the winner */

typedef struct { uint8_t sq, how; } go_step;

typedef struct {
    int player, from, d1, d2;
    int n;                                      /* the steps of its way */
    go_step step[GO_STEPS];
    int to;                                     /* where it ends */
    int swap;                                   /* the player sent to `from` (-1: none) */
    int freed;                                  /* two held: the one freed after it, to throw next (-1: none) */
} go_move;

typedef struct {
    uint8_t dice[GO_MAX_THROWS][2];             /* the throws, in order; those after n were undone (known: how many) */
    uint8_t who[GO_MAX_THROWS];                 /* who threw each */
    int n, known;
    int players;
    int pos[GO_PLAYERS];                        /* each pawn's square (0: the start) */
    int wait[GO_PLAYERS];                       /* turns still to miss at the inn; GO_HELD */
    int turn, winner;                           /* winner -1: nobody yet */
    go_move last;                               /* the last throw's move (n > 0) */
} go_game;

void go_new(go_game *g, int players);           /* 2..6 */
int  go_play(go_game *g, int d1, int d2);       /* the player to move throws; 0: not allowed (bad dice, or over) */
int  go_undo(go_game *g);                       /* the last throw taken back (it stays known); 0: none */
int  go_next(const go_game *g, int *d1, int *d2);   /* the throw undone at this point; 0: none */
int  go_turn(const go_game *g);
int  go_status(const go_game *g);
int  go_kind(int square);                       /* GK_* */
int  go_last_throw(const go_game *g, int player, int *d1, int *d2);   /* the player's last throw; 0: none yet */
int  go_script(go_game *g, const char *throws); /* 0: a throw is bad */

#endif
