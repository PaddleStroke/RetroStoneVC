/*
 * Twisty Cube for one player: the rules. A cube of n x n x n small cubes (n 2, 3 or 4), each outer face of it covered
 * with stickers, one colour a face when it is solved. A move turns one layer of small cubes a quarter round one of the
 * three axes (a face's layer or one inside: a slice), either way. Turning the whole cube is only a way of looking at
 * it: no move, nothing in the state. Solved: each face one colour, whichever colour it is (the cube may have been
 * turned whole, by turning all of its layers).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The stickers: 6 faces of n x n (face f: its normal along axis f / 2, + for f even, - for f odd: R L U D F B along x y
 * z), sticker f * n * n + row * n + col. In doubled coordinates (the cube from -n to n on each axis), a sticker's small
 * cube has its centre at 2 i - (n - 1) on each axis; on face f, along axis u = (a + 1) % 3 that is 2 col - (n - 1),
 * along v = (a + 2) % 3 2 row - (n - 1) (a the face's axis).
 * A move (tcb_move, a byte): axis << 3 | layer << 1 | back: a quarter turn of layer 0..n-1 (from the - side) about
 * the axis, counter-clockwise seen from its + side (back 0), or clockwise (back 1).
 * The puzzles: a scramble (a list of moves) played from the solved cube. The levels (twistycube_levels.h, made by
 * tools/twistycube_gen.c): short scrambles whose par is their length (the shortest ones checked by a search: none
 * shorter solves them), then full scrambles (par 0: not known). At random: a full scramble of the size chosen.
 * The notation (script, the screenshots, the tests): a move a word: its axis x y z, its layer 0..n-1, then + or -
 * ("x2+ y0-"); "h" plays the hint.
 */
#ifndef TWISTYCUBE_H
#define TWISTYCUBE_H

#include <stdint.h>

#define TCB_MAXN 4
#define TCB_STICKERS (6 * TCB_MAXN * TCB_MAXN)
#define TCB_MAXSCR 48                           /* a scramble's moves at most */
#define TCB_HIST 1024                           /* the player's moves kept for undo */
enum { TCB_VARIANTS = 3, TCB_LEVELS = 150, TCB_CHAPTERS = 6 };

typedef uint8_t tcb_move;
typedef struct {
    int n;
    uint8_t c[TCB_STICKERS];                    /* each sticker's colour 0..5 (its face's when solved) */
} tcb_cube;

static inline tcb_move tcb_mv(int axis, int layer, int back) { return (tcb_move)(axis << 3 | layer << 1 | (back & 1)); }
static inline int tcb_axis(tcb_move m) { return m >> 3; }
static inline int tcb_layer(tcb_move m) { return m >> 1 & 3; }
static inline int tcb_back(tcb_move m) { return m & 1; }
static inline tcb_move tcb_inverse(tcb_move m) { return (tcb_move)(m ^ 1); }

void tcb_init(tcb_cube *c, int n);
int  tcb_valid(int n, tcb_move m);              /* 1: a move of a cube of n */
void tcb_turn(tcb_cube *c, tcb_move m);
int  tcb_solved(const tcb_cube *c);
/* the stickers' places (doubled coordinates): the small cube's centre and the face's normal (a unit vector) */
void tcb_place(int n, int sticker, int p[3], int nrm[3]);
int  tcb_sticker_at(int n, const int p[3], const int nrm[3]);   /* -1: none */
int  tcb_layer_of(int n, int sticker, int axis);   /* the layer 0..n-1 of the sticker's small cube along an axis */
/* a scramble of `depth` moves at random from the seed's chance (*rng: puzrand.h's), none of them undoing or shortening
 * the ones before (moves along one axis in a row: their layers rising, a half turn as two, never what a turn of the
 * whole cube and fewer moves would do); returns how many (depth) */
int  tcb_scramble(uint32_t *rng, int n, int depth, tcb_move *out);
/* the fewest quarter turns a row of moves along one axis comes to (the cube's turns as a whole free): turns[l] per
 * layer, mod 4 */
int  tcb_axis_cost(int n, const int *turns);

typedef struct {
    tcb_cube cube;
    int n, par, full;                           /* full: a full scramble (no par, no hint) */
    tcb_move scr[TCB_MAXSCR];
    int nscr;
    tcb_move hist[TCB_HIST];                    /* the player's moves (the hints' too) */
    int nh, hints;
} tcb_game;

void tcb_new(tcb_game *g, int n, const tcb_move *scr, int nscr, int par, int full);
int  tcb_level(tcb_game *g, int level);         /* 0: no such level */
int  tcb_level_info(int level, int *n, int *par, int *full);   /* the level's size, par, full; 0: none */
void tcb_random(tcb_game *g, int variant, uint32_t seed);   /* variant 0, 1, 2: 2 x 2, 3 x 3, 4 x 4 */
int  tcb_full_depth(int n);                     /* a full scramble's length */
int  tcb_play(tcb_game *g, tcb_move m);         /* 0: not a move */
int  tcb_undo(tcb_game *g);                     /* the last move taken back: 1; nothing to undo: 0 */
/* the hint: the next move back towards the solved cube (the scramble and the player's moves, what cancels taken
 * out, undone from the end); 0: none (a full scramble, solved) */
int  tcb_hint(const tcb_game *g, tcb_move *m);
int  tcb_moves(const tcb_game *g);              /* the player's moves (the moves made, undo taken off) */
int  tcb_script(tcb_game *g, const char *moves);
int  tcb_parse_move(int n, const char *s, tcb_move *m);   /* one word ("x2+"); 0: bad */
void tcb_move_text(tcb_move m, char out[4]);
/* the shortest path left once what cancels is taken out of a row of moves (moves along one axis in a row merged) */
int  tcb_reduce(int n, const tcb_move *in, int k, tcb_move *out);

#endif
