/*
 * Hex Stacks for one player: the rules. A honeycomb board of hexagonal cells; a tray offers three stacks of thin
 * coloured hex tiles at a time (each stack one to three bands of a colour, one band on another). Put a stack on an
 * empty cell; when the tray is empty three more come. Then the stacks round it whose top colour matches gather: their
 * top bands pass, a tile at a time, onto one stack, the purest of them (the fewest other tiles under its top band;
 * a tie: the stack just changed). What that uncovers may match its own neighbours in turn: a chain. A stack whose top
 * band reaches HX_CLEAR tiles of one colour clears it: those tiles go, and count.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The chain, exactly: a list of cells to look at (first the cell just filled; a cell is never twice in it). Take the
 * first, X (empty: skip). M: its neighbours whose top colour is X's. M empty: a top band of HX_CLEAR or more clears (X
 * goes back on the list). Else, with below(s) the tiles under s's top band: X no less pure than all of M gathers them
 * all, each in the order of the six directions; otherwise X gives its own top band to the purest of M (a tie: the
 * bigger band, then the first direction). Every stack that changed goes on the list (the one that took first).
 * Cells: some open, some blocked (a stone: never used), some locked (they open once that many tiles have been
 * cleared in all). The board is full when no open cell is empty: the game is over.
 * Two ways to play:
 * - Levels (hexstack_levels.h, made by tools/hexstack_gen.c): a designed board, maybe stacks on it already, a goal
 *   (clear N tiles, or N tiles of one colour) and a fixed tray sequence (dealt three at a time in its order, from its
 *   start again past its end). The goal reached: solved; the board full first: failed. The par: the stacks the
 *   rules' own player (hx_best: the hint) places to reach the goal; the levels' tool keeps a level only when that
 *   player and most players playing a little worse win it with room to spare.
 * - Endless (the random tab): a board of the variant's size, the stacks by chance (the game's own seeded numbers,
 *   puzrand.h, kept with each position: undo brings back the same tray), more colours as the tiles cleared grow. The
 *   score: the tiles cleared; it ends when the board is full. The best score is kept by the UI.
 * Undo takes back the last placement only.
 * Places: the board is HX_COLS x HX_ROWS cells in offset rows, odd rows half a cell to the right; cell c = col + row *
 * HX_COLS. The notation (script, the pictures, the tests): a placement is the tray's place (a b c), the column (a..g)
 * and the row (1..9 from the top): "ad5 bc4"; "=k" plays k placements of the hint ("=": until the end).
 * Board text: rows from the top split by '|', a character a cell: ' ' or '.' none, 'o' open, '#' blocked, '1'..'9'
 * locked (opens at that many tens of tiles cleared), 'S' open with the next stack of the level's own list on it.
 * Stacks as text: a band a letter, its colour (0..5) times 4 plus its count less one (1..4 tiles); the first band of a
 * stack (its bottom) upper case 'A'..'X', the bands above it lower case 'a'..'x'.
 */
#ifndef HEXSTACK_H
#define HEXSTACK_H

#include <stdint.h>

#define HX_COLS 7
#define HX_ROWS 9
#define HX_CELLS (HX_COLS * HX_ROWS)
#define HX_TALL 20                              /* tiles a stack at most (a band moves only as far as it fits) */
#define HX_TRAY 3
#define HX_BANDS 3                              /* bands a tray stack, at most */
#define HX_STACK 12                             /* tiles a tray stack, at most */
#define HX_CLEAR 10
#define HX_COLOURS 6
#define HX_SEQ 100                              /* a level's stacks, at most */
#define HX_EVENTS 400                           /* a chain's steps, at most */
#define HX_LEVELS 150
#define HX_CHAPTERS 6
#define HX_VARIANTS 3
#define HX_NONE 0xff

enum { HX_VOID, HX_OPEN, HX_BLOCK, HX_LOCK };   /* a cell's kind */
enum { HX_ENDLESS, HX_TILES, HX_COLOUR };       /* the goals (endless: none) */
enum { HX_EV_MOVE, HX_EV_CLEAR, HX_EV_UNLOCK }; /* a chain's steps (the UI's animation) */

typedef struct { uint8_t n, t[HX_STACK]; } hx_tray_stack;   /* t[0]: its bottom tile's colour */

typedef struct {
    uint8_t kind[HX_CELLS];                     /* HX_VOID .. HX_LOCK */
    uint8_t need[HX_CELLS];                     /* a lock: the tiles cleared it waits for, in tens */
    uint8_t n[HX_CELLS], t[HX_CELLS][HX_TALL];  /* each stack, its bottom first */
    hx_tray_stack tray[HX_TRAY];                /* n 0: that place used */
    uint16_t cleared, by_colour[HX_COLOURS];
    uint16_t next, placed;                      /* the sequence's next stack; stacks placed */
    uint32_t rng;                               /* endless: the chance to come */
} hx_pos;

typedef struct { uint8_t kind, a, b, colour, count; } hx_event;   /* MOVE a -> b; CLEAR a; UNLOCK a */

typedef struct {
    int goal, colour, target, par, colours;     /* endless: HX_ENDLESS; colours: how many the deals use */
    int variant;                                /* endless: the board's size */
    hx_tray_stack seq[HX_SEQ];
    int nseq;
    hx_pos p, prev;
    int has_prev;
    hx_event ev[HX_EVENTS];                     /* the last placement's chain */
    int nev, last_cell, last_slot;              /* the last placement: its cell and its tray place */
} hx_game;

typedef struct {
    uint8_t goal, colour, colours, par;
    uint16_t target;
    const char *board, *pre, *seq;              /* the board, its stacks already there, the tray's sequence */
} hx_level_def;
extern const hx_level_def hx_levels[HX_LEVELS];

/* ---- the board --------------------------------------------------------------------------------------------------- */
int  hx_neighbours(const hx_pos *p, int c, int out[6]);   /* the cells round c on the board, in direction order */
int  hx_dir(int c, int d);                      /* the cell in direction d (0..5: E, SE, SW, W, NW, NE), -1 off */
int  hx_top(const hx_pos *p, int c, int *band); /* the top tile's colour (-1 empty) and how many of it on top */
int  hx_open_empty(const hx_pos *p);            /* open cells without a stack */
int  hx_full(const hx_pos *p);                  /* no open cell left empty (the game over, unless solved) */
int  hx_tray_empty(const hx_pos *p);

/* ---- a game ------------------------------------------------------------------------------------------------------ */
/* a board from text (its sequence and stacks from text, goal and all): 0 bad */
int  hx_load(hx_game *g, const char *board, const char *pre, const char *seq, int goal, int colour, int target,
             int colours);
int  hx_level(hx_game *g, int level);           /* 0: no such level */
void hx_endless(hx_game *g, int variant, uint32_t seed);
/* a stack put on an empty open cell from the tray's place `slot`: the chain played out (g->ev), the tray dealt again
 * once empty; 0 refused (nothing there, the cell not open or not empty, the game over) */
int  hx_place(hx_game *g, int slot, int cell);
int  hx_undo(hx_game *g);                       /* the last placement taken back (once) */
int  hx_solved(const hx_game *g);
int  hx_over(const hx_game *g);                 /* solved, or the board full */
int  hx_goal_left(const hx_game *g);            /* a level: the tiles still to clear (0: reached) */
int  hx_score(const hx_game *g);                /* the tiles cleared */
int  hx_stars(const hx_game *g);                /* endless: by the score and the board's size */
/* the chain on a position alone (the tests): the events into ev (may be NULL), their count */
int  hx_resolve(hx_pos *p, int cell, hx_event *ev, int cap);
/* the rules' own player: the best placement by its evaluation (noise: a random part added to each value, from *rng;
 * 0 none): 1 found, 0 nothing to place */
int  hx_best(const hx_game *g, int noise, uint32_t *rng, int *slot, int *cell);
int  hx_hint(const hx_game *g, int *slot, int *cell);
/* plays the rules' player to the end (or `most` placements): the placements made, *won solved, *room the fewest empty
 * open cells seen after a placement */
int  hx_play_out(hx_game *g, int noise, uint32_t seed, int most, int *won, int *room);
int  hx_script(hx_game *g, const char *moves);
/* the endless boards (variants), as board text */
extern const char *const hx_boards[HX_VARIANTS];
/* a stack by chance (the deals; the levels' tool): one to three bands of 1..4 tiles (press 0..9: more bands), two
 * bands in a row of different colours, of the first `colours`; its top one of bias[] (nb of them: the board's top
 * colours) now and then */
void hx_chance_stack(uint32_t *r, int colours, int press, const int *bias, int nb, hx_tray_stack *o);
/* stacks as text: n stacks into out (0: bad); one stack written */
int  hx_parse_stacks(const char *s, hx_tray_stack *out, int cap);
void hx_stack_text(const hx_tray_stack *s, char *out);   /* at most 2 * HX_BANDS + 1 bytes */

#endif
