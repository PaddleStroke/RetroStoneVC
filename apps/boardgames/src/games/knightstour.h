/*
 * The Knight's Tour, for one player: a knight visits every square of the board exactly once (the rules, no SDL).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The boards: w x h squares (up to 10 x 10), some of them holes (cut out: never visited). The random tours (the
 * variants): 5 x 5, 6 x 6, 8 x 8, starting on a square drawn from the seed, one from which a whole tour exists
 * (checked by the solver: on 5 x 5 only the 13 squares of the corners' colour have one). The levels
 * (knightstour_levels.h, made by tools/knightstour_gen.c): boards of every shape, with holes, and their start.
 * The knight moves as in chess, onto a square it has not visited; it may be stuck (no such square left: a dead end,
 * counted): undo then. Done: every square visited; a closed tour too when the last square is a knight's move from the
 * first. The stars: 3 with no hint and no dead end, 2 with up to two hints, else 1.
 * The hint: Warnsdorff's rule (go where the fewest onward moves remain; on a tie, the square furthest from the
 * middle), with a search behind it, so it never leads into a dead end: the first move by the rule from which the
 * search still finds a whole tour. When no tour can be finished from here any more, the hint says so (undo).
 * Squares: rank * w + file (a1 = 0, the bottom left).
 * A level: its rows from the top, split by '|': '.' a square, '#' a hole, 'N' the knight's start.
 */
#ifndef KNIGHTSTOUR_H
#define KNIGHTSTOUR_H

#include <stdint.h>

enum { KT_5, KT_6, KT_8, KT_VARIANTS };
#define KT_MAXW 10
#define KT_MAX (KT_MAXW * KT_MAXW)
#define KT_LEVELS 150
#define KT_CHAPTERS 6

typedef struct {
    int w, h, cells;                            /* the board: w x h squares, cells of them to visit (not holes) */
    uint8_t hole[KT_MAX];                       /* 1: a hole */
    uint8_t path[KT_MAX];                       /* the squares visited, in order */
    int len;
    uint8_t order[KT_MAX];                      /* each square's number on the path (1..), 0: not visited */
    int hints, dead_ends;
} kt_game;

extern const char *const kt_levels[KT_LEVELS];

int  kt_size(int variant);
void kt_new(kt_game *g, int variant, uint32_t seed);
void kt_new_at(kt_game *g, int n, int start);   /* an n x n tour from that square (the tests) */
/* a board w x h, its holes (NULL: none), the knight on start; 0: not a board (too big, start on a hole) */
int  kt_setup(kt_game *g, int w, int h, const uint8_t *hole, int start);
int  kt_load(kt_game *g, const char *rows);     /* a level's rows; 0: not a level */
int  kt_level(kt_game *g, int level);           /* level 0..KT_LEVELS-1 */
int  kt_moves(const kt_game *g, uint8_t *to);   /* the squares the knight can go to now; their count */
int  kt_can(const kt_game *g, int sq);
int  kt_move(kt_game *g, int sq);               /* 0: not a move */
int  kt_undo(kt_game *g);                       /* 0: only the start left */
int  kt_done(const kt_game *g);
int  kt_closed(const kt_game *g);               /* done, and the last square a knight's move from the first */
int  kt_stuck(const kt_game *g);                /* not done, and no move left */
int  kt_onward(const kt_game *g, int sq);       /* the moves the knight would have from sq (Warnsdorff's count) */
/* the whole tour from here, into out (the squares after the current one, cells - len of them) within `budget` steps
 * of search: 1 found, 0 none exists, -1 the budget ran out */
int  kt_solve(const kt_game *g, uint8_t *out, long budget);
/* the hint: the square to go to; -1: no tour can be finished from here (undo); -2: done or stuck */
int  kt_hint(kt_game *g);
int  kt_stars(const kt_game *g);                /* the tour done: 1..3 */
/* "c3 d5 b4": squares, each a move (the first may name the start, then it is the current square); "?": the hint
 * followed */
int  kt_script(kt_game *g, const char *s);

#endif
