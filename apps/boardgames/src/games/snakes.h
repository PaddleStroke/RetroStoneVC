/*
 * Snakes and ladders for two players on one device: the rules (a board of 100 squares numbered in a zigzag from the
 * bottom left, one die, the ladders up and the snakes down; a 6 throws again; 100 needs the exact number, a bigger
 * throw bounces back from it; the first on 100 wins).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Squares: 1..100; 0 is off the board, before square 1, where both pawns start. Player 0 (ivory) throws first. The
 * rules take the throws as input (the shell's dice give them), so the tests and the screenshots choose them. The
 * moves' notation (script, the screenshots, the tests): the throws in order, "4 6 2 ...".
 */
#ifndef SNAKES_H
#define SNAKES_H

#include <stdint.h>

#define SN_SIDE 10
#define SN_LAST 100
#define SN_LINKS 16                     /* 8 ladders, then 8 snakes */
#define SN_MAX_THROWS 16384             /* a game takes about 80 throws */

enum { SN_PLAY, SN_WIN0, SN_WIN1 };

typedef struct { uint8_t from, to; } sn_link;   /* a ladder's foot and top, or a snake's head and tail */
extern const sn_link sn_links[SN_LINKS];        /* sn_links[i].to > from: a ladder */

typedef struct {
    int player, from, roll;
    int land;                           /* where the pawn lands after the hops (a bounce off 100 included) */
    int to;                             /* where it ends: the top of a ladder, a snake's tail, or the landing */
    int again;                          /* 1: a 6, the same player throws again */
} sn_move;

typedef struct {
    uint8_t roll[SN_MAX_THROWS];        /* the throws, in order; those after n were undone (known: how many) */
    int n, known;
    int pos[2];                         /* each player's square, 0: off the board */
    int turn, winner;                   /* winner -1: nobody yet */
    sn_move last;                       /* the last throw's move (n > 0) */
} sn_game;

void sn_new(sn_game *g);
int  sn_play(sn_game *g, int roll);     /* the player to move throws roll (1..6); 0: not allowed (bad, or over) */
int  sn_undo(sn_game *g);               /* takes the last throw back (it stays known); 0: nothing to undo */
int  sn_next(const sn_game *g);         /* the throw undone at this point (thrown again: never a new one); 0: none */
int  sn_turn(const sn_game *g);
int  sn_status(const sn_game *g);       /* SN_* */

void sn_cell(int square, int *col, int *row);   /* 1..100: column 0..9 from the left, row 0..9 from the bottom */
int  sn_hop(int from, int k);           /* the square k hops after from (bouncing back from 100) */
int  sn_link_at(int square);            /* the link starting on a square, or -1 */

#endif
