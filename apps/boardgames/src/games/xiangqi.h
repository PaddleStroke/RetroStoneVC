/*
 * Xiangqi (Chinese chess) for two players on one device: the rules (moves, check, the flying general, mate, the draws).
 * No engine, no clock. Tested by tests/xiangqi_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 red (moves first, at the bottom), 1 black. Points: x + 9 * y, x the file from red's left (a..i), y the
 * rank from red's side (0..9): red's general starts on e0, black's on e9. The river runs between ranks 4 and 5; the
 * palaces are files d..f, ranks 0..2 (red) and 7..9 (black).
 * The notation (the scripts, the tests): a move is its two points, "h2e2" (red's cannon to the middle file).
 * The end: a player with no legal move loses, in check (mate) or not (stalemate); the same position a third time
 * (the same player to move) is a draw, and so are 60 moves each without a capture.
 */
#ifndef XIANGQI_H
#define XIANGQI_H

#include <stdint.h>

#define XQ_W 9
#define XQ_H 10
#define XQ_PTS (XQ_W * XQ_H)
#define XQ_PT(x, y) ((y) * XQ_W + (x))
#define XQ_X(p) ((p) % XQ_W)
#define XQ_Y(p) ((p) / XQ_W)

enum { XQ_EMPTY, XQ_GENERAL, XQ_ADVISOR, XQ_ELEPHANT, XQ_HORSE, XQ_CHARIOT, XQ_CANNON, XQ_SOLDIER };
#define XQ_PC(player, type) ((uint8_t)((player) << 3 | (type)))
#define XQ_TYPE(pc)  ((pc) & 7)
#define XQ_OWNER(pc) ((pc) >> 3)

typedef struct {
    uint8_t sq[XQ_PTS];
    uint8_t side;                               /* who moves */
    uint16_t quiet;                             /* plies since the last capture: the draw rule */
} xpos;

typedef struct { uint8_t from, to, cap; } xmove;   /* cap: the piece taken (0: none) */
#define XQ_MAX_MOVES 128                        /* a position has at most about 110 */

void xq_start(xpos *p);
int  xq_legal(const xpos *p, xmove *out);       /* the legal moves; their count */
void xq_make(xpos *p, xmove m);                 /* plays a (legal) move */
int  xq_general(const xpos *p, int player);     /* the general's point (-1: none) */
int  xq_in_check(const xpos *p, int player);    /* its general attacked, or facing the other on an open file */
int  xq_point(const char *s);                   /* "e0" -> its point; -1: not a point */
/* the move written in s ("h2e2"): its index in legal, -1 if none */
int  xq_parse(const xmove *legal, int n, const char *s);
/* a position from lists of pieces, a letter and a point: K general, A advisor, E elephant, H horse, R chariot,
 * C cannon, P soldier: xq_setup(&p, 0, "Ke0 Rh7", "Ke9 Pc3"); 0: a bad list */
int  xq_setup(xpos *p, int side, const char *pieces0, const char *pieces1);

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { XQ_PLAY, XQ_MATE, XQ_STALEMATE, XQ_REPETITION, XQ_QUIET } xstatus;
#define XQ_QUIET_PLIES 120                      /* 60 moves each without a capture: a draw */
#define XQ_MAX_PLY 2048
typedef struct {
    xpos pos[XQ_MAX_PLY + 1];
    xmove move[XQ_MAX_PLY];
    int ply;
} xgame;
void xq_new(xgame *g);
int  xq_play(xgame *g, xmove m);                /* 0: the game is too long */
int  xq_undo(xgame *g);                         /* 0: nothing to undo */
xstatus xq_status(const xgame *g);              /* mate or stalemate: the side to move lost */
#define xq_pos(g) (&(g)->pos[(g)->ply])

#endif
