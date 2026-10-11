/*
 * Dots and boxes for two players on one device: the rules (lines, boxes, the extra turn, the count). No engine.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: n x n boxes (3, 5 or 7 in the app; the rules take 1..7), (n + 1) x (n + 1) dots. A dot is (x, y), x the
 * column from the left, y the row from the bottom, both 0..n. A line joins two neighbouring dots and is named by its
 * left or lower dot: across (h, to the dot on its right) or up (v, to the dot above). Lines are numbered: the n (n + 1)
 * across first (y * n + x), then the n (n + 1) up ((n + 1) n + y * (n + 1) + x). A box is (x, y), 0..n - 1, its lower
 * left corner at the dot (x, y): y * n + x.
 * The rules: player 0 (ivory) first. A line that closes boxes (one, or two at once) claims them for its player, who
 * draws again; a line that closes none passes the turn. When every line is drawn, the most boxes wins (equal: a draw;
 * with an odd number of boxes, as on the app's boards, there is always a winner).
 * Notation (script, the screenshots and the tests): "h1,1" the line across from the dot in column 1, row 1 (counted
 * from 1, from the lower left corner, as on a chess board), "v2,1" the line up from the dot in column 2, row 1;
 * separated by spaces: "h1,1 v2,1 h1,2 v1,1".
 */
#ifndef DOTS_H
#define DOTS_H

#include <stdint.h>

#define DOTS_MAX 7                              /* boxes on a side, at most */
#define DOTS_MAX_LINES (2 * DOTS_MAX * (DOTS_MAX + 1))
#define DOTS_MAX_BOXES (DOTS_MAX * DOTS_MAX)

typedef struct {
    uint8_t n;                                  /* boxes on a side */
    uint8_t side;                               /* who draws */
    uint8_t drawn;                              /* lines drawn */
    uint8_t boxes[2];                           /* boxes claimed by each player */
    int8_t line[DOTS_MAX_LINES];                /* -1, or the player who drew it */
    int8_t box[DOTS_MAX_BOXES];                 /* -1, or the player who claimed it */
} dpos;

int  dots_lines(int n);                         /* the lines of a board of n x n boxes: 2 n (n + 1) */
int  dots_h(int n, int x, int y);               /* the line across from the dot (x, y); -1 off the board */
int  dots_v(int n, int x, int y);               /* the line up from the dot (x, y); -1 off the board */
void dots_ends(int n, int line, int *x0, int *y0, int *x1, int *y1);   /* its two dots */
int  dots_sides(int n, int box, int *side4);    /* a box's four lines (below, above, left, right); 4 */
void dots_start(dpos *p, int n);
int  dots_make(dpos *p, int line);              /* draws a line: the boxes it claimed (0, 1, 2); -1: not allowed */
int  dots_parse(int n, const char *s, int *len);   /* "h1,1": its line (len: the characters read); -1 */

/* A game: every position since the start, so undo is free. */
typedef enum { DS_PLAY, DS_WIN0, DS_WIN1, DS_DRAW } dstatus;
typedef struct {
    dpos pos[DOTS_MAX_LINES + 1];
    int16_t move[DOTS_MAX_LINES];
    int ply;
} dgame;
void dots_new(dgame *g, int n);
int  dots_play(dgame *g, int line);             /* the boxes it claimed; -1: not allowed */
int  dots_undo(dgame *g);                       /* 0: nothing to undo */
dstatus dots_status(const dgame *g);
int  dots_script(dgame *g, const char *moves);  /* plays lines in the notation above; 0: one is bad */
#define dots_pos(g) (&(g)->pos[(g)->ply])

#endif
