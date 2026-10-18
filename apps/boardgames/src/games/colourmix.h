/*
 * Colour Mixer for one player: the rules. A white pot and a colour to match: add drops of paint from the tubes
 * (red, yellow, blue, white, black; later premixed ones) until the pot shows the target's colour.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The paints mix as pigments do, not as light: red and yellow make orange, yellow and blue green, red and blue
 * violet, all three a dark brown. The pot's colour is a function of the drops in it (their ratios only: two drops of
 * each give what one of each gives): the coloured paints' share of each primary, scaled so the largest is 1, picks a
 * point of the painters' red-yellow-blue cube (its corners the eight mixtures, the colours in between blended); white
 * then lightens it as its share of the pot, black darkens it (more than its share: a little black goes far). A
 * premixed paint is a fixed mixture of the primaries, white and black: a drop of it brings that mixture. Solved when
 * the pot's colour is within CMX_TOL of the target's (CIE76, the distance in CIELAB); a closeness meter shows how
 * near it is. The par: the fewest drops added that reach it, found by trying every set of drops up to CMX_MAXD.
 * A board: up to CMX_TUBES tubes, each its paint, how many drops it holds (CMX_ANY: no end), the drops already in the
 * pot ("fix this pot"), and the recipe that makes the target (the intended solution, the par's).
 * The text of a board: four fields split by '/': the tubes' paints (a letter each: r y b w k, o orange, g green,
 * v violet, n brown, p pink), their drops ('-' no end, else a digit), the drops in the pot at first and the recipe
 * (a digit a tube each). The notation: a tube's letter in the board's order ('a' the first) adds a drop, "-" takes
 * back the last, "h" a hint (played), "=" the rest of a shortest way.
 */
#ifndef COLOURMIX_H
#define COLOURMIX_H

#include <stdint.h>

#define CMX_PAINTS 10
#define CMX_BASE 5                              /* red, yellow, blue, white, black */
#define CMX_TUBES 7
#define CMX_ANY 255
#define CMX_HIST 96
#define CMX_MAXD 16
#define CMX_TOL 3.0f
#define CMX_LEVELS 150
#define CMX_CHAPTERS 6
#define CMX_VARIANTS 3
#define CMX_TEXT (4 * CMX_TUBES + 8)

enum { CMX_RED, CMX_YELLOW, CMX_BLUE, CMX_WHITE, CMX_BLACK, CMX_ORANGE, CMX_GREEN, CMX_VIOLET, CMX_BROWN, CMX_PINK };

typedef struct {
    int n;                                      /* tubes */
    uint8_t paint[CMX_TUBES], limit[CMX_TUBES], start[CMX_TUBES], recipe[CMX_TUBES];
    float target[3];                            /* sRGB 0..1 */
} cmx_map;

typedef struct {
    uint8_t add[CMX_TUBES];                     /* drops added from each tube */
    uint8_t order[CMX_HIST];                    /* the tubes in the order the drops went in */
    int n;
} cmx_state;

typedef struct {
    cmx_map m;
    cmx_state s;
    int par;
} cmx_game;

typedef struct {
    int tubes;                                  /* bit p: paint p offered */
    int lo, hi;                                 /* the recipe's drops */
    int kinds_lo, kinds_hi;                     /* the tubes it uses */
    int start;                                  /* drops in the pot at first (0: none) */
    int limits;                                 /* tubes holding few drops */
    int need;                                   /* bit p: the recipe must use paint p's kind (1 << CMX_WHITE: white
                                                 * or black; 1 << CMX_ORANGE: a premixed one) */
} cmx_spec;

void  cmx_paint_mix(int paint, float amount[CMX_BASE]);   /* what one drop of it brings */
void  cmx_paint_rgb(int paint, float rgb[3]);   /* its colour in the tube */
/* the colour of paints in these amounts (sRGB 0..1); 0: nothing there */
int   cmx_colour(const float amount[CMX_BASE], float rgb[3]);
float cmx_de(const float a[3], const float b[3]);   /* the distance between two colours in CIELAB */
/* the pot's colour with these drops from each tube (the start's and the added); 0 empty */
int   cmx_pot(const cmx_map *m, const uint8_t *add, float rgb[3]);
float cmx_dist(const cmx_map *m, const uint8_t *add);   /* to the target (an empty pot: 1000) */
int   cmx_solved(const cmx_map *m, const cmx_state *s);
int   cmx_left(const cmx_map *m, const cmx_state *s, int tube);   /* drops left (CMX_ANY: no end) */
int   cmx_drop(cmx_game *g, int tube);          /* 1: a drop added */
int   cmx_undo(cmx_game *g);
int   cmx_count(const cmx_state *s);            /* the drops added */
/* the fewest drops to add to `add` to solve it (-1 none within `most` more); the drops (may be NULL) */
int   cmx_solve(const cmx_map *m, const uint8_t *add, int most, uint8_t *more);
/* how many recipes, other than ratios of the par's, solve it or come near it (within `de`) with at most `drops` */
int   cmx_rivals(const cmx_map *m, int drops, float de);
/* a hint: a tube to add a drop from (*take 0), or one to take a drop back from (*take 1); 0 none */
int   cmx_hint(const cmx_map *m, const cmx_state *s, int *tube, int *take);
/* a greedy player: each step the drop added (depth 2: or two drops; back: or a drop taken back) that brings the pot
 * nearest the target, until none brings it nearer; 1: it solves it (*drops, may be NULL: the drops it ends with) */
int   cmx_greedy(const cmx_map *m, int par, int back, int depth, int *drops);
int   cmx_take(cmx_game *g, int tube);          /* the last drop from that tube taken back */
int   cmx_parse(cmx_map *m, const char *text);
void  cmx_text(const cmx_map *m, char *out);    /* CMX_TEXT bytes */
void  cmx_begin(cmx_game *g);
void  cmx_make(cmx_map *m, const cmx_spec *spec, uint32_t seed, int *par);   /* par -1: none made */
int   cmx_script(cmx_game *g, const char *moves);
typedef struct { const char *text; uint8_t par; } cmx_level_t;
extern const cmx_level_t cmx_levels[CMX_LEVELS];
int   cmx_level(cmx_game *g, int level);        /* 0: none */
void  cmx_variant(int variant, cmx_spec *spec);
void  cmx_random(cmx_game *g, int variant, uint32_t seed);

#endif
