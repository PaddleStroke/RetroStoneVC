/*
 * Train Switches for one player: the rules. A toy railway lies on a grid of squares: plain track, switches (one track
 * splitting in two), crossings, tunnels, coloured stations and the portals the trains come in by. Set the switches,
 * then send the trains: each one runs along the track, turning at each switch the way it is set, and must stop at a
 * station of its colour. Solve it with as few switch changes as possible.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The run, square by square (a tick a square):
 * - a train entering a switch by its stem leaves by the branch it is set to; entering by a branch, it leaves by the
 *   stem (the switch does not change);
 * - a toggle switch flips after each train that runs over it, either way;
 * - a crossing lets a train straight on; a tunnel mouth takes it to its twin mouth, which it leaves the next tick;
 * - a train reaching a station stops there: it must be the station's colour; running into a portal (a dead end) or
 *   on round the track for too long fails;
 * - one by one (the usual): each train leaves its portal once the one before has stopped, in their order;
 *   together: all of them leave at once, and two trains on one square, or passing each other between two squares,
 *   crash.
 * A move: one switch flipped before a run (undo takes it back); the run itself is not a move. The par: the fewest
 * flips from the switches as given to a setting where every train reaches its colour (a search over the settings).
 * A map as text: rows from the top split by '/', three characters a square: its kind, its open sides (a hex digit:
 * 1 up, 2 right, 4 down, 8 left), and one more: '.' empty, 't' track, 's' a switch ('S' a toggle; then its stem's
 * side 0..3, up right down left), 'x' a crossing, 'p' a station (then its colour 0..4), 'e' a portal, 'u' a tunnel
 * mouth (then its pair 0..3). Then '|', 'S' one by one or 'T' together, '|' the trains in their order (a colour and a
 * portal's number each, the portals counted from the top left, ',' between), '|' the switches' first settings (a '0'
 * or '1' each, the switches counted from the top left). A switch's branches: the two sides after its stem going
 * clockwise; setting 0 the first, 1 the second. The notation (script, the screenshots, the tests): a square ("b3": the
 * second column, the third row from the top) flips the switch there, "g" sends the trains (the whole run at once),
 * "=" the rest of a fewest-flips way and the run, "=k" its next k flips.
 */
#ifndef TRAINS_H
#define TRAINS_H

#include <stdint.h>

#define TRS_MAXW 8
#define TRS_MAXH 10
#define TRS_CELLS (TRS_MAXW * TRS_MAXH)
#define TRS_MAXS 13                             /* switches at most */
#define TRS_MAXT 7                              /* trains at most */
#define TRS_MAXE 4                              /* portals at most */
#define TRS_COLOURS 5
#define TRS_PATH 120                            /* a train's squares at most (else: round and round) */
#define TRS_HIST 512
#define TRS_LEVELS 150
#define TRS_CHAPTERS 6
#define TRS_VARIANTS 3
#define TRS_TEXT (3 * TRS_CELLS + TRS_MAXH + 64)

enum { TRS_UP, TRS_RIGHT, TRS_DOWN, TRS_LEFT };
enum { TRS_EMPTY, TRS_TRACK, TRS_SWITCH, TRS_CROSS, TRS_STATION, TRS_PORTAL, TRS_TUNNEL };
enum { TRS_OK, TRS_WRONG, TRS_CRASH, TRS_DEAD, TRS_LOOP };   /* how a run ends */

typedef struct {
    int w, h;
    uint8_t kind[TRS_CELLS];                    /* TRS_* */
    uint8_t open[TRS_CELLS];                    /* its sides: bit d for side d */
    uint8_t k[TRS_CELLS];                       /* a switch's stem side, a station's colour, a tunnel's pair */
    uint8_t toggle[TRS_CELLS];                  /* a switch: 1 a toggle */
    int together;                               /* 1: the trains leave at once */
    int ns, ne, nt;
    uint8_t sw[TRS_MAXS];                       /* the switches' squares, from the top left */
    uint8_t portal[TRS_MAXE];                   /* the portals' squares */
    uint8_t tcol[TRS_MAXT], tfrom[TRS_MAXT];    /* the trains in their order: colour, portal */
    uint16_t set0;                              /* the switches as given: bit i switch i's setting */
} trs_map;

/* a run, for the animation: train i leaves at tick start[i], is on square cell[i][j] at tick start[i] + j (entered by
 * side in[i][j], 4: its portal; left by out[i][j], 4: stopped there), len[i] squares in all (0: it never left);
 * each toggle flipping at tick flip_t[f] (the train on it then: it shows once that train has gone by); how it ended:
 * at tick `ticks`, `bad` the square where it went wrong, bad_t (and bad_t2 in a crash) the trains */
typedef struct {
    int result, ticks, bad, bad_t, bad_t2;
    int start[TRS_MAXT], len[TRS_MAXT];
    uint8_t cell[TRS_MAXT][TRS_PATH], in[TRS_MAXT][TRS_PATH], out[TRS_MAXT][TRS_PATH];
    int nflip;
    short flip_t[TRS_MAXT * TRS_PATH / 2];
    uint8_t flip_s[TRS_MAXT * TRS_PATH / 2];
} trs_run;

typedef struct {
    trs_map m;
    uint16_t set;                               /* the switches now */
    uint16_t hist[TRS_HIST];
    int nh;                                     /* the flips kept (undo) */
    int moves, par, runs, fails, hints, solved;
} trs_game;

typedef struct {
    int w, h;
    int portals, stations, trains;
    int switches;                               /* branches laid off the line (each makes a switch, two for a loop) */
    int loops, toggles, crossings, tunnels;
    int together;
    int lo, hi;                                 /* the par sought */
    int tries;                                  /* the networks laid at most */
} trs_spec;

int  trs_parse(trs_map *m, const char *text);   /* 0: not a map */
void trs_text(const trs_map *m, char *out);     /* TRS_TEXT bytes */
int  trs_switch_at(const trs_map *m, int cell); /* the switch's number on that square, -1 none */
int  trs_branch(const trs_map *m, int cell, int setting);   /* the side a switch set so sends a train out by */
/* the side a train entering square c by side `in` leaves by (set: the switches); -1 it stops or cannot go on */
int  trs_exit(const trs_map *m, int c, int in, uint16_t set);
int  trs_neighbour(const trs_map *m, int c, int side);   /* -1 off the board */
/* a run from the setting `set`: TRS_OK every train at a station of its colour; r (may be NULL) the details */
int  trs_simulate(const trs_map *m, uint16_t set, trs_run *r);
/* the fewest flips from `set` to a setting that works (-1 none); *to: that setting (may be NULL); *count: how many
 * settings work (may be NULL) */
int  trs_solve(const trs_map *m, uint16_t set, uint16_t *to, int *count);
void trs_begin(trs_game *g);
int  trs_flip(trs_game *g, int sw);             /* 1 flipped (kept for undo), 0 refused */
int  trs_undo(trs_game *g);
int  trs_go(trs_game *g, trs_run *r);           /* the run: TRS_*; solved when TRS_OK */
int  trs_hint(const trs_game *g);               /* a switch to flip next, -1: none (the setting works: send them) */
int  trs_script(trs_game *g, const char *moves);
int  trs_make(trs_map *m, const trs_spec *spec, uint32_t seed);   /* 1: the par in range (else the best kept) */
void trs_variant(int variant, trs_spec *spec);
void trs_random(trs_game *g, int variant, uint32_t seed);
extern const char *const trs_icons[TRS_CHAPTERS];   /* the chapters' pictures: small networks */
extern const char *const trs_levels[TRS_LEVELS];
extern const uint8_t trs_level_par[TRS_LEVELS];
int  trs_level(trs_game *g, int level);

#endif
