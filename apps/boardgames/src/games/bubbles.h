/*
 * Bubble Pop for one player: the rules. Glossy marbles hang from the ceiling of a wooden box, packed in staggered rows
 * (a hexagonal grid: an even row holds w marbles, an odd row w - 1, set half a marble in). A launcher at the bottom
 * shoots the marbles of the level's sequence one at a time along a straight line that bounces off the side walls; a
 * shot marble stops where it touches another one (or the ceiling) and snaps into the nearest empty place beside it.
 * Three or more of one colour touching there pop; then whatever no longer hangs from the ceiling falls. The marble in
 * the launcher and the next one may be swapped (no shot spent). The aim: clear the box, or, where gems are caught in
 * it, free every gem (a gem never pops: it falls with what holds it) in the shots the sequence gives.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Specials: a stone never pops (it only falls, or a bomb breaks it); a locked marble (a brass cage round it) counts in
 * its colour's group, but a pop only breaks its cage; a bomb (shot) blows up the marbles round the place it lands; a
 * rainbow marble (shot) joins any colour touching it (each colour's group of three or more pops), and where none pops
 * it stays as a rainbow that joins any colour later. Some levels lower the ceiling a row after every few shots. A
 * marble left resting past the bottom line, or the shots run out first: the try has failed (undo, or start again).
 *
 * The geometry is whole numbers (the same on every device, so the tests, the level maker and a game played again from
 * its record see the same shots): a marble is BUB_D steps across, rows BUB_ROWH apart; the launcher shoots along
 * (aim, -BUB_AY), aim -BUB_AMAX..BUB_AMAX (10 degrees above the floor each way). A shot's centre runs to the first
 * marble it comes within BUB_HIT of (a little less than a marble: one slips through a gap a little narrower than
 * itself), the ceiling, or a wall (it bounces), found exactly by whole-number roots.
 *
 * A level as text: "<w><rows><every>:<cells>:<sequence>:<solution>" where w is a digit (cells in an even row), rows and
 * every one hex digit each (a marble resting in row rows or below has gone past the line; the ceiling comes down after
 * every `every` shots, 0 never); cells: rows from the top split by '|', '.' empty, r g b y p w the six colours (red,
 * green, blue, yellow, purple, white), their capitals locked, 's' stone, '*' a gem, '?' a rainbow at rest; the
 * sequence: the marbles in order (the colours, '!' a bomb, '@' a rainbow), as many as the shots allowed; the solution:
 * the shots of the fewest found, split by ',', each an aim, '~' before it when the next marble is shot (swapped in).
 * The notation of moves (script): "<aim>" a shot, "~<aim>" the next marble shot (swapped first), "w" a swap, "h" a hint
 * (its shot played), "=" the rest of a solution played.
 */
#ifndef BUBBLES_H
#define BUBBLES_H

#include <stdint.h>

#define BUB_D 1024                              /* a marble across, steps */
#define BUB_R 512
#define BUB_ROWH 887                            /* rows apart: D * sqrt(3) / 2 */
#define BUB_HIT 860                             /* centres this close: they touch */
#define BUB_AY 512                              /* a shot runs along (aim, -BUB_AY) */
#define BUB_AMAX 2904                           /* 10 degrees above the floor */
#define BUB_MAXW 9
#define BUB_ROWS 16
#define BUB_CELLS (BUB_ROWS * BUB_MAXW)
#define BUB_SEQ 40
#define BUB_PATH 24
#define BUB_COLOURS 6
#define BUB_MOVES 96
#define BUB_TEXT 400
#define BUB_CHAPTERS 6
#define BUB_VARIANTS 3

enum { BUB_EMPTY, BUB_STONE = 7, BUB_GEM = 8, BUB_WILD = 9, BUB_BOMB = 10, BUB_RAINBOW = 11 };
#define BUB_LOCK 0x10                           /* a colour's cage */
enum { BUB_PLAYING, BUB_WON, BUB_OVER, BUB_LOW };

typedef struct {
    int w, rows, every;                         /* every: the ceiling comes down a row after every `every` shots */
    int gems;                                   /* gems at the start (0: the aim is to clear the box) */
    int nseq;                                   /* the shots allowed: the sequence's length */
    uint8_t seq[BUB_SEQ];
    uint8_t c[BUB_ROWS][BUB_MAXW];
    int nsol;                                   /* the solution kept (its length: the par) */
    int16_t sol[BUB_SEQ];
    uint8_t solswap[BUB_SEQ];
    int shape, pattern;                         /* the maker's (not in the text): its shape and its colouring */
} bub_level;

typedef struct {
    uint8_t c[BUB_ROWS][BUB_MAXW];
    uint8_t cur, nxt;                           /* the marble in the launcher, the next one (0: none) */
    uint8_t qi, shots, drop, end;               /* the sequence's next to come; shots made; the ceiling's rows down */
} bub_state;

/* what one shot did, for the screen: the centre's path (the launcher, each bounce, where it stopped), the place it
 * snapped into, the marbles popped (each with its ring: how far from the shot, for the pops one after another), the
 * cages broken, the marbles that fell, the ceiling lowered after it */
typedef struct {
    int n, bounces;
    int32_t x[BUB_PATH], y[BUB_PATH];
    int cell;                                   /* r * BUB_MAXW + c, -1 none */
    uint8_t marble;
    int npop, nfall, nunlock, dropped, gems_freed;
    uint8_t pop[BUB_CELLS], popc[BUB_CELLS], popring[BUB_CELLS];
    uint8_t fall[BUB_CELLS], fallc[BUB_CELLS];
    uint8_t unlock[BUB_CELLS];
} bub_shot;

typedef struct { int16_t aim; uint8_t swap, cell; } bub_move;

typedef struct {
    bub_level L;
    bub_state s;
    bub_state hist[BUB_SEQ + 1];                /* the state before each shot */
    int nh, hints;
} bub_game;

int  bub_parse(bub_level *L, const char *text);  /* 0: not a level */
void bub_text(const bub_level *L, char *out);    /* BUB_TEXT bytes */
int  bub_row_len(const bub_level *L, int r);     /* w, or w - 1 for an odd row */
void bub_cell_xy(const bub_level *L, int drop, int r, int c, int32_t *x, int32_t *y);
void bub_launcher(const bub_level *L, int32_t *x, int32_t *y);
int32_t bub_field_h(const bub_level *L);         /* the box's inside, steps from the ceiling's start to its floor */
void bub_begin(const bub_level *L, bub_state *s);
/* the shot's path at this aim, where it would land (no change): its cell, or -1 */
int  bub_trace(const bub_level *L, const bub_state *s, int aim, bub_shot *out);
/* a shot (swap: the next marble shot instead): 1 shot, 0 refused (the try over, no marble) */
int  bub_shoot(const bub_level *L, bub_state *s, int aim, int swap, bub_shot *out);
int  bub_swap(bub_state *s);                    /* 1: swapped */
int  bub_count(const bub_state *s, int what);   /* cells holding `what` (BUB_GEM...); -1 every marble */
/* the shots worth trying from s: one a place a marble can land in (seen with at most two bounces, at the middle of the
 * aims landing there), for the marble in the launcher and for the next one; ordered by aim from the left */
int  bub_moves(const bub_level *L, const bub_state *s, bub_move *out, int max);
/* the fewest shots found from s (a beam search beam wide): its shots in path (may be NULL); -1 none found */
int  bub_solve(const bub_level *L, const bub_state *s, int beam, bub_move *path);
/* the obvious player: each shot the one that clears the most now; the shots it wins in, -1 lost */
int  bub_greedy(const bub_level *L, const bub_state *s);
/* the solution kept with the level, played from the start: the shots it wins in, -1 it does not */
int  bub_check_solution(const bub_level *L);

void bub_new(bub_game *g, const bub_level *L);
int  bub_play(bub_game *g, int aim, int swap, bub_shot *out);   /* a shot kept for undo */
int  bub_undo(bub_game *g);
int  bub_hint(bub_game *g, bub_move *m);         /* the next shot of a way to win (backing up first if needed); 0 none */
int  bub_script(bub_game *g, const char *moves);

/* the maker: a symmetric box of marbles in a pattern, its sequence, solved; for the levels' tool and the random tab */
typedef struct {
    int w, rows, every, colours;
    int hmin, hmax;                             /* the cluster's rows */
    int gems, stones, locks, bombs, rainbows;   /* how many (gems, stones and locks in mirrored pairs or on the axis) */
    int slack;                                  /* shots allowed past the fewest found */
    int beam;                                   /* the search's width */
} bub_spec;
/* a box and its sequence (BUB_SEQ long) from the seed; 0: no good box */
int  bub_make(bub_level *L, const bub_spec *sp, uint32_t seed);
/* the sequence cut to the fewest shots found plus the spec's slack, the solution kept: 0 unsolved */
int  bub_settle(bub_level *L, const bub_spec *sp);

extern const char *const bub_levels[];
extern const int bub_level_count;
int  bub_chapter_of(int level);
int  bub_open(bub_game *g, int level);          /* 0: none */
void bub_variant(int variant, bub_spec *sp);
void bub_random(bub_game *g, int variant, uint32_t seed);

#endif
