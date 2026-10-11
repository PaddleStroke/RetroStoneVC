/*
 * Janggi (Korean chess) for two players on one device: the rules (moves along the palaces' diagonals, the cannon's
 * jump, check, the pass, mate, the draws). No engine, no clock. Tested by tests/janggi_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 Cho (blue-green, moves first, at the bottom), 1 Han (red). Points: x + 9 * y, x the file from Cho's left
 * (a..i), y the rank from Cho's side (0..9): Cho's general starts on e1, in the middle of its palace, Han's on e8. The
 * palaces are files d..f, ranks 0..2 (Cho) and 7..9 (Han), with their two diagonals. No river.
 * The setup: from each player's left, chariot, horse, elephant, guard, (the general a rank up), guard, elephant, horse,
 * chariot ("ma-sang-sang-ma": the horses outside); the cannons on b and h, a rank in front of the horses; five
 * soldiers.
 * The notation (the scripts, the tests): a move is its two points, "e3f3"; "pass" a pass.
 * The end: a player in check with no move that saves the general loses (mate); a player not in check may always pass
 * (with no legal move, must). A move may not leave the two generals facing each other on an open file. The same
 * position a third time (the same player to move) is a draw (four passes in a row bring it back); so is a game of
 * JG_MAX_PLY plies.
 */
#ifndef JANGGI_H
#define JANGGI_H

#include <stdint.h>

#define JG_W 9
#define JG_H 10
#define JG_PTS (JG_W * JG_H)
#define JG_PT(x, y) ((y) * JG_W + (x))
#define JG_X(p) ((p) % JG_W)
#define JG_Y(p) ((p) / JG_W)

enum { JG_EMPTY, JG_GENERAL, JG_GUARD, JG_ELEPHANT, JG_HORSE, JG_CHARIOT, JG_CANNON, JG_SOLDIER };
#define JG_PC(player, type) ((uint8_t)((player) << 3 | (type)))
#define JG_TYPE(pc)  ((pc) & 7)
#define JG_OWNER(pc) ((pc) >> 3)

typedef struct {
    uint8_t sq[JG_PTS];
    uint8_t side;                               /* who moves */
    uint16_t quiet;                             /* plies since the last capture: how far back a repetition looks */
} jpos;

typedef struct { uint8_t from, to, cap; } jmove;   /* cap: the piece taken (0: none) */
#define JG_PASS 0xFF                            /* a pass: from and to JG_PASS */
#define JG_MAX_MOVES 192                        /* a position has far fewer */

void jg_start(jpos *p);
int  jg_legal(const jpos *p, jmove *out);       /* the legal moves (not the pass); their count */
int  jg_can_pass(const jpos *p);                /* 1: the side to move may pass (not in check) */
void jg_make(jpos *p, jmove m);                 /* plays a (legal) move, or the pass */
int  jg_general(const jpos *p, int player);     /* the general's point (-1: none) */
int  jg_in_check(const jpos *p, int player);    /* its general attacked */
int  jg_facing(const jpos *p);                  /* the generals on one file, nothing between them */
int  jg_point(const char *s);                   /* "e1" -> its point; -1: not a point */
/* the move written in s ("e3f3"): its index in legal, -1 if none */
int  jg_parse(const jmove *legal, int n, const char *s);
/* a position from lists of pieces, a letter and a point: K general, A guard, E elephant, H horse, R chariot,
 * C cannon, P soldier: jg_setup(&p, 0, "Ke1 Rh7", "Ke8 Pc3"); 0: a bad list */
int  jg_setup(jpos *p, int side, const char *pieces0, const char *pieces1);

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { JG_PLAY, JG_MATE, JG_REPETITION, JG_LONG } jstatus;
#define JG_MAX_PLY 2000                         /* a thousand moves each: a draw */
typedef struct {
    jpos pos[JG_MAX_PLY + 1];
    jmove move[JG_MAX_PLY];
    int ply;
} jgame;
void jg_new(jgame *g);
int  jg_play(jgame *g, jmove m);                /* 0: the game is too long */
int  jg_undo(jgame *g);                         /* 0: nothing to undo */
jstatus jg_status(const jgame *g);              /* mate: the side to move lost */
#define jg_pos(g) (&(g)->pos[(g)->ply])

#endif
