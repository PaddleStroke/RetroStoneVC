/*
 * Pull the Pins for one player: the rules. A tall glass box in a walnut frame holds chambers of glass walls; golden
 * pins (straight bars with a ring at one end, level or slanting) hold things back: coloured balls (gems), grey balls,
 * water and lava. Pull the pins one at a time, in the right order, so that every ball ends in the cup at the bottom,
 * coloured.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The things: gems (a colour each), grey balls (coloured by the first gem they touch, in its colour), water and lava
 * (small drops), stone (lava that water touched; stone touching lava turns it to stone too, one drop after another).
 * A gem or a grey ball touching lava burns: the puzzle is lost. So is a grey ball, a ball of the wrong colour or lava
 * reaching a cup (a cup of a colour takes only gems of that colour; a cup of any colour takes every gem); water and
 * stone may go in. Once everything has come to rest: every ball in a cup is the win; every pin pulled and a ball left
 * out is lost.
 *
 * The physics: a simple deterministic simulation in whole numbers (no floating point: the same on every device, so a
 * game played again from its record, the tests and the level maker all see the same thing). The world is PTP_WU x
 * PTP_HU units of PTP_FP steps; time goes in ticks of 1/PTP_HZ s. Each tick: every ball moves on (its speed kept from
 * its last two places, damped, gravity added), then three passes push apart the balls that overlap (by their weights)
 * and push the balls out of the walls and the pins (round-ended bars), the last pass rubbing their speed along the
 * walls. A pin pulled slides out along its length (towards its ring) over PTP_PULL ticks, then is gone. The world is
 * at rest when nothing has moved faster than a little for a while (or long after the last pull).
 *
 * A level's text: tokens split by spaces, numbers as two hex digits in half units: "C1-" one cup in the middle that
 * takes any colour, "C2rb" two cups (left, right) and their colours (digits, '-' any); "t" + template (two hex) +
 * mirrored (0, 1) the walls and pins of a template, then "b" + kind (g d w l, '-' empty) + colour digit + count (two
 * hex) each of its boxes in turn; or drawn in the text itself: "w" + x0 y0 x1 y1 a glass wall;
 * "p" + ax ay bx by a pin, its ring at a; "f" + kind (g gem, d grey, w water, l lava) + colour digit + count (two hex)
 * + x0 y0 x1 y1 a box filled from its bottom up; "s" + orders that win (pin digits, split by ','). Pins are numbered in
 * the order given. The notation of moves: "p<i>" a pin pulled (and everything left to rest), "r" a restart, "h" a hint
 * (its pin pulled).
 */
#ifndef PINS_H
#define PINS_H

#include <stdint.h>

#define PTP_FP 256                              /* steps in a unit */
#define PTP_WU 64                               /* the world: units across and down */
#define PTP_HU 104
#define PTP_HZ 120                              /* ticks a second */
#define PTP_PULL 34                             /* ticks a pin takes to slide out */
#define PTP_MAXB 150                            /* balls and drops */
#define PTP_MAXWALL 40
#define PTP_MAXSEG (PTP_MAXWALL + 16)           /* with the frame and the cups */
#define PTP_MAXPIN 6
#define PTP_MAXFILL 8
#define PTP_MAXSOL 8
#define PTP_EVENTS 48
#define PTP_TEXT 640
#define PTP_COLOURS 4                           /* the gems: ruby, emerald, sapphire, amber */
#define PTP_LEVELS 133                          /* the first chapter (every order wins) only a start */
#define PTP_CHAPTERS 6
#define PTP_VARIANTS 3
#define PTP_WALL_R 256                          /* a wall's half thickness */
#define PTP_PIN_R 220

enum { PTP_GEM, PTP_DULL, PTP_WATER, PTP_LAVA, PTP_STONE, PTP_KINDS };
/* how a try ends: playing, solved, or lost by ... */
enum { PTP_PLAYING, PTP_WON, PTP_BURNT, PTP_GREY_IN, PTP_WRONG_CUP, PTP_LAVA_IN, PTP_STUCK };
/* what happened, for the screen: a ball into a cup, a grey ball coloured, lava turned to stone, a ball burnt */
enum { PTP_E_CUP, PTP_E_COLOUR, PTP_E_STONE, PTP_E_BURN };

typedef struct { int16_t ax, ay, bx, by; } ptp_line;   /* half units */
typedef struct { uint8_t kind, col, n; int16_t x0, y0, x1, y1; } ptp_fill;

typedef struct {
    int cups;                                   /* 1: one in the middle; 2: left and right */
    int8_t cupcol[2];                           /* a cup's colour, -1 any */
    int tpl, mirror, nbox;                      /* made from a template (tpl -1: drawn in the text itself) */
    uint8_t bkind[PTP_MAXFILL], bcol[PTP_MAXFILL], bn[PTP_MAXFILL];   /* its boxes: kind (255 empty), colour, count */
    int nw, np, nf, nsol;
    ptp_line w[PTP_MAXWALL], p[PTP_MAXPIN];     /* walls; pins (a: the ring's end) */
    ptp_fill f[PTP_MAXFILL];
    uint8_t sol[PTP_MAXSOL][PTP_MAXPIN], sollen[PTP_MAXSOL];
} ptp_map;

/* the world's still parts in steps: segments (walls, the frame, the cups), the pins where they stand, the cups */
typedef struct { int32_t ax, ay, bx, by, r, x0, y0, x1, y1; } ptp_seg;
#define PTP_CS 1536                             /* the grid that finds neighbours: a cell's side in steps */
#define PTP_GX 11
#define PTP_GY 18
#define PTP_CELLSEG 14
typedef struct {
    int nseg, np, cups;
    ptp_seg seg[PTP_MAXSEG];
    uint8_t ncs[PTP_GY][PTP_GX], cs[PTP_GY][PTP_GX][PTP_CELLSEG];   /* the segments near each cell (255: all) */
    int32_t pax[PTP_MAXPIN], pay[PTP_MAXPIN], pbx[PTP_MAXPIN], pby[PTP_MAXPIN], plen[PTP_MAXPIN];
    int32_t rim, cx0[2], cx1[2];                /* the cups' rim (y) and their mouths (x) */
    int8_t cupcol[2];
} ptp_world;

typedef struct { int32_t x, y, px, py, wx, wy; uint8_t kind, col, live, cup, cool; } ptp_ball;   /* wx, wy: at the window's start */
typedef struct { uint8_t type, col; int16_t x, y; int32_t tick; } ptp_event;   /* x, y in sixteenths of a unit */

typedef struct {
    int nb, target;
    ptp_ball b[PTP_MAXB];
    int32_t pulled[PTP_MAXPIN];                 /* the tick a pin was pulled, -1 in place */
    uint8_t order[PTP_MAXPIN];
    int npulled, tick, calm, still, last, rest, end, fx, fy, incup;
    int nev;                                    /* events so far (the last PTP_EVENTS kept, ev[i % PTP_EVENTS]) */
    ptp_event ev[PTP_EVENTS];
} ptp_sim;

typedef struct {
    ptp_map m;
    ptp_world w;
    ptp_sim s, s0;                              /* the try; the world at rest at the start */
    int hints, restarts;
} ptp_game;

int  ptp_parse(ptp_map *m, const char *text);  /* 0: not a level */
void ptp_text(const ptp_map *m, char *out);    /* PTP_TEXT bytes */
void ptp_build(const ptp_map *m, ptp_world *w, ptp_sim *s);   /* the world and the balls at the start */
int  ptp_pull(const ptp_world *w, ptp_sim *s, int pin);       /* 1: pulled now; 0 refused (gone, or the try over) */
void ptp_step(const ptp_world *w, ptp_sim *s);                /* one tick */
void ptp_settle(const ptp_world *w, ptp_sim *s);              /* ticks until rest */
/* pin i at tick t (in steps): where it is (a its ring's end), and how far out (0..1024; 1024 gone); 0: gone */
int  ptp_pin_now(const ptp_world *w, const ptp_sim *s, int i, int t, int32_t *ax, int32_t *ay, int32_t *bx,
                 int32_t *by, int *out);
int  ptp_naive(const ptp_map *m, uint8_t *order);   /* the pins from the top down (then from the left): their order */
/* the end of a whole try of pins in this order (each pulled once all is at rest): PTP_WON or how it was lost, or
 * PTP_PLAYING when the order ran out with the world at rest and pins left */
int  ptp_try(const ptp_map *m, const uint8_t *order, int n, ptp_sim *out);

/* every order of pulling the pins tried (from each rest): the orders that win (up to PTP_MAXSOL kept, shortest
 * first), how many, the share of random orders that win (0..1), the naive order's end */
typedef struct {
    int wins, nkept, naive;
    double share;
    uint8_t ord[PTP_MAXSOL][PTP_MAXPIN], len[PTP_MAXSOL];
    long settles;
} ptp_report;
void ptp_search(const ptp_map *m, ptp_report *r);

void ptp_begin(ptp_game *g);                   /* the try from the start */
int  ptp_hint(const ptp_game *g);              /* the next pin of a winning order, -1 restart first, -2 none */
int  ptp_script(ptp_game *g, const char *moves);

/* the templates: hand drawn chambers with boxes to fill; the maker fills them (a chapter's things) and mirrors them */
typedef struct { const char *geo; int cups; int chapter_min; } ptp_tpl;
#define PTP_TPLS 18
extern const ptp_tpl ptp_tpls[PTP_TPLS];
/* a level from template t mirrored or not, each box's thing (kinds[i]: -1 empty, else PTP_* kind; cols[i]) and how
 * many, the cups' colours (as drawn, before the mirror); 0: no such template */
int  ptp_from_tpl(ptp_map *m, int t, int mirror, const int *kinds, const int *cols, const int *counts,
                  const int *cupcol);
int  ptp_tpl_boxes(int t);                      /* how many boxes it has */
int  ptp_tpl_pins(int t);
int  ptp_tpl_room(int t, int box, int kind);    /* how many of a kind fill box `box` packed */

typedef struct { const char *text; } ptp_level_t;
extern const ptp_level_t ptp_levels[PTP_LEVELS];
int  ptp_chapter_of(int level);
int  ptp_level(ptp_game *g, int level);         /* 0: none */
/* a puzzle at random (variant 0 easy, 1 medium, 2 hard), made and checked by the search; the same from the same seed */
void ptp_random(ptp_game *g, int variant, uint32_t seed);
/* the maker's draw: a level for a chapter-like spec from the seed's chance (no check) */
int  ptp_make(ptp_map *m, int chapter, uint32_t *rs);
/* the maker's check: 1 when the level is good for that chapter (wins one way; past the first chapter the naive order
 * loses); r filled */
int  ptp_judge(ptp_map *m, int chapter, ptp_report *r);

#endif
