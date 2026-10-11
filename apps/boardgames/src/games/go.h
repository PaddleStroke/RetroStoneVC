/*
 * Go's rules for RetroStone Board (no SDL): captures, no suicide, positional superko (a move may not bring back any
 * earlier position), two passes in a row then the counting: the players mark the dead groups, area scoring (Chinese:
 * stones on the board + the empty points they surround), komi 7.5 for ivory. Tested by tests/go_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ebony (black, plays first), 1 ivory (white, gets the komi).
 * Points: x + y * n, x from the left, y from the bottom, as go's notation names them: a letter (a, b, ... skipping i)
 * and a number from the bottom: "d4 q16 pass". The moves of a script: points and "pass"; a pass while counting means
 * the players are done (the game is over).
 */
#ifndef GO_H
#define GO_H

#include <stdint.h>

#define GO_MAX 19
#define GO_PTS (GO_MAX * GO_MAX)
#define GO_HIST 1000                            /* moves and passes in a game (a long 19 x 19 game: about 300) */
#define GO_PASS (-1)
#define GO_NONE (-2)
#define GO_KOMI 7.5f

enum { GO_EMPTY, GO_EBONY, GO_IVORY };          /* a point: empty, or player p's stone (p + 1) */
enum { GO_PLAY, GO_COUNT, GO_OVER };            /* the phases */

typedef struct {
    uint8_t pt[GO_PTS];
    int side;                                   /* the player to move */
    int caps[2];                                /* the stones each player captured */
    uint32_t hash;                              /* of the stones (the superko's quick test) */
} gopos;

typedef struct {
    int n, phase, nmoves;
    gopos pos[GO_HIST + 1];                     /* pos[k]: after k moves */
    int move[GO_HIST];                          /* a point or GO_PASS */
    uint8_t dead[GO_PTS];                       /* counting: the stones marked dead */
} gogame;

void go_new(gogame *g, int n);                  /* n: 9, 13 or 19 */
const gopos *go_now(const gogame *g);
int go_last(const gogame *g);                   /* the last move: a point, GO_PASS, or GO_NONE */
/* the position after a stone on p; -1: not allowed, else the captures */
int go_try(const gogame *g, int p, gopos *out);
int go_play(gogame *g, int p);                  /* -1: not allowed, else the stones it captured */
int go_pass(gogame *g);                         /* playing: a pass (the second in a row: counting); counting: done */
int go_undo(gogame *g);                         /* over: back to counting; counting: back before the last pass */
int go_toggle(gogame *g, int p);                /* counting: the group on p dead / alive; 0: no stone there */
/* owner of each point (0, 1, or -1: nobody's) and the scores (ivory's with the komi), the dead stones taken off */
void go_score(const gogame *g, int8_t owner[GO_PTS], float score[2]);
int go_winner(const gogame *g);                 /* over: 0 or 1 (no draw with the komi's half point); else -1 */
int go_point(int n, const char *s, int *len);   /* "d4": the point, "pass": GO_PASS, else GO_NONE; len: its length */
int go_script(gogame *g, const char *s);        /* plays "d4 q16 pass ..."; 0: a bad or illegal move */

#endif
