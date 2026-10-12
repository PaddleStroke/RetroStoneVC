/*
 * The Knight's Tour, for one player: a knight visits every square of the board exactly once (the rules, no SDL).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Three boards: 5 x 5, 6 x 6, 8 x 8. A new tour starts on a square drawn from the seed, one from which a whole tour
 * exists (checked by the solver: on 5 x 5 only the 13 squares of the corners' colour have one). The knight moves as in
 * chess, onto a square it has not visited; it may be stuck (no such square left): undo then. Done: every square
 * visited; a closed tour too when the last square is a knight's move from the first.
 * The hint: Warnsdorff's rule (go where the fewest onward moves remain; on a tie, the square furthest from the
 * middle), with a search behind it, so it never leads into a dead end: the first move by the rule from which the
 * search still finds a whole tour. When no tour can be finished from here any more, the hint says so (undo).
 * Squares: rank * n + file (a1 = 0, the bottom left).
 */
#ifndef KNIGHTSTOUR_H
#define KNIGHTSTOUR_H

#include <stdint.h>

enum { KT_5, KT_6, KT_8, KT_VARIANTS };
#define KT_MAX 64

typedef struct {
    int n;
    int8_t path[KT_MAX];                        /* the squares visited, in order */
    int len;
    int8_t order[KT_MAX];                       /* each square's number on the path (1..), 0: not visited */
    int hints;
} kt_game;

int  kt_size(int variant);
void kt_new(kt_game *g, int variant, uint32_t seed);
void kt_new_at(kt_game *g, int n, int start);   /* a tour from that square (the tests) */
int  kt_moves(const kt_game *g, int8_t *to);    /* the squares the knight can go to now; their count */
int  kt_can(const kt_game *g, int sq);
int  kt_move(kt_game *g, int sq);               /* 0: not a move */
int  kt_undo(kt_game *g);                       /* 0: only the start left */
int  kt_done(const kt_game *g);
int  kt_closed(const kt_game *g);               /* done, and the last square a knight's move from the first */
int  kt_stuck(const kt_game *g);                /* not done, and no move left */
int  kt_onward(const kt_game *g, int sq);       /* the moves the knight would have from sq (Warnsdorff's count) */
/* the whole tour from here, into out (the squares after the current one, n * n - len of them) within `budget`
 * steps of search: 1 found, 0 none exists, -1 the budget ran out */
int  kt_solve(const kt_game *g, int8_t *out, long budget);
/* the hint: the square to go to; -1: no tour can be finished from here (undo); -2: done or stuck */
int  kt_hint(kt_game *g);
/* "c3 d5 b4": squares, each a move (the first may name the start, then it is the current square); "?": the hint
 * followed */
int  kt_script(kt_game *g, const char *s);

#endif
