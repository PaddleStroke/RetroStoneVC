/*
 * Ring Puzzle for one player: the rules. Two or three circular rings of beads overlap, sharing the beads where they
 * cross. A move turns one ring by one bead's step, either way: every bead on it moves along it, the shared ones too,
 * which takes them out of the other ring's order. Sort the beads back so every place shows its colour (the colours
 * marked on the rings' tracks): solved.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The rings: equal circles of n beads (n 8 or 12), two neighbours crossing one bead's angle off the line between
 * their centres (two beads' angle for 12: room for the beads where they overlap), so where two cross is a
 * bead's place on both. RG_TWO: two side by side; RG_CHAIN: three in a row (n 8; the outer two apart); RG_TRIANGLE:
 * three in a triangle (n 12). A place: a bead's spot, numbered once however many rings pass through it (rg_map).
 * A position: the colour on each place (2 bits each, colours 0..3: ivory, ebony, red lacquer, jade); beads of a
 * colour are alike, so the target is one position. The par: the fewest moves, by a search from both ends at once
 * (the scramble's side keeps its way back for the hint). The levels: scrambles made by the levels' tool
 * (tools/rings_gen.c) and kept with their par; the random puzzles: a scramble of the variant's moves, its par found
 * the same way.
 * The notation: a move is the ring's letter (a b c) and + (counter-clockwise, as the beads' slots count) or -
 * (clockwise): "a+ b- b-"; "h" plays the hint, "=" the shortest way to the end.
 */
#ifndef RINGS_H
#define RINGS_H

#include <stdint.h>

#define RG_RINGS 3
#define RG_N 12                                 /* beads a ring, at most */
#define RG_PLACES 32
#define RG_COLOURS 4
#define RG_HIST 512
#define RG_LEVELS 150
#define RG_CHAPTERS 6
#define RG_VARIANTS 3
#define RG_DEEP 24                              /* the search's depth, at most */

enum { RG_TWO, RG_CHAIN, RG_TRIANGLE };

typedef uint64_t rg_pos;                        /* place i: bits 2i..2i+1 */

typedef struct {
    int kind, n, rings, places;
    float cx[RG_RINGS], cy[RG_RINGS];           /* the centres (the ring's radius 1, y down) */
    int slot[RG_RINGS][RG_N];                   /* ring k's slot i (at angle -2 pi i / n from the right: the screen's
                                                 * counter-clockwise): its place */
    float px[RG_PLACES], py[RG_PLACES];         /* each place's point */
    rg_pos target;
    int colours;
} rg_map;

typedef struct {
    rg_map m;
    rg_pos p, start;
    uint8_t hist[RG_HIST];                      /* the moves made (undo) */
    int nh, par;
} rg_game;

static inline int rg_colour(rg_pos p, int place) { return (int)(p >> (2 * place) & 3); }

int  rg_build(rg_map *m, int kind, int n);      /* the rings and their places (0: no such) */
rg_pos rg_apply(const rg_map *m, rg_pos p, int move);   /* move = ring * 2 + (0: +, 1: -) */
int  rg_moves(const rg_map *m);                 /* how many moves there are (2 a ring) */
static inline int rg_inverse(int move) { return move ^ 1; }
/* the fewest moves from p to the target (-1: none within RG_DEEP or the search's room); *first its first move */
int  rg_solve(const rg_map *m, rg_pos p, int *first);
/* the target of a kind: each ring its colour (place by place, as text "0011..."), patterns by number */
void rg_pattern(rg_map *m, int pattern, int colours);
int  rg_text(const rg_map *m, rg_pos p, char *out);   /* a position as digits, a place each */
int  rg_parse(const rg_map *m, const char *s, rg_pos *p);

void rg_begin(rg_game *g, rg_pos start);
int  rg_move(rg_game *g, int move);             /* 1: made */
int  rg_undo(rg_game *g);
int  rg_solved(const rg_game *g);
int  rg_hint(const rg_game *g);                 /* the next move of a shortest way (-1: solved or none) */
int  rg_script(rg_game *g, const char *moves);

typedef struct {
    uint8_t kind, n, colours, par;
    const char *target, *start, *way;           /* the target's and the start's colours (a digit a place); a
                                                 * shortest way (a letter a move: 'a' + move) */
} rg_level_def;
extern const rg_level_def rg_levels[RG_LEVELS];
int  rg_level(rg_game *g, int level);           /* 0: none */
/* a random puzzle of a variant (a scramble of its moves from its target, the par searched) */
void rg_random(rg_game *g, int variant, uint32_t seed);

#endif
