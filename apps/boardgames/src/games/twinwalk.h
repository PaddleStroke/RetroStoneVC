/*
 * Twin Walkers for one player: the rules. Two little walkers stand on a floor of tiles among walls. Each move sends
 * both at once: the first walks the way you chose, the second walks the same way up or down but the other way left or
 * right (a mirror). A wall, a tile gone, a shut door or the other walker stops one of them alone: the other still
 * walks. Bring both onto their own exits at the same time.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The tiles of the later chapters:
 * - a crumbling tile bears a walker once: when the walker steps off it, it falls away (a gap no one can enter);
 * - brass plates and the doors of their colour: a door is open while a walker stands on a plate of its colour (or in
 *   the door itself), shut otherwise;
 * - ice: a walker stepping onto ice slides on the way it went until something stops it or it reaches a tile that is
 *   not ice. Both walkers move a tile at a time together, so they may meet on the way.
 * Two walkers never share a tile: two going for the same tile, or for each other's, both stay; one walking into the
 * other that stays stays too.
 * A move: one step (or slide) of both; a move that moves neither is refused. The par: the fewest moves, by a
 * breadth-first search over the walkers' places and the tiles fallen.
 * The puzzles: made by twk_make for the levels (tools/twinwalk_gen.c, offline) and at random alike: walls laid with a
 * symmetry, the special tiles by chance, the walls then changed a pair at a time while the fewest moves to the
 * farthest pair of places grow; the exits put on the farthest pair. The special tiles must matter (taken away, the
 * fewest moves change).
 * A map as text, rows from the top split by '|': '.' a floor tile, '#' a wall, 'A' the first walker's start, 'B' the
 * mirror walker's, 'a' and 'b' their exits, '%' a crumbling tile, 'p' 'q' a plate of colour 1, 2, 'P' 'Q' a door of
 * that colour, '~' ice. Cells x + y * w from the top left. The notation (script, the screenshots, the tests): u r d l
 * a move (the first walker's way), "=" the rest of a shortest way, "=k" its next k moves.
 */
#ifndef TWINWALK_H
#define TWINWALK_H

#include <stdint.h>

#define TWK_MAXW 10
#define TWK_MAXH 12
#define TWK_CELLS (TWK_MAXW * TWK_MAXH)
#define TWK_MAXCR 16                            /* crumbling tiles at most */
#define TWK_GROUPS 2                            /* plates' and doors' colours */
#define TWK_HIST 1024
#define TWK_LONGEST (TWK_MAXW + TWK_MAXH)       /* a slide's steps at most */
#define TWK_LEVELS 150
#define TWK_CHAPTERS 6
#define TWK_VARIANTS 3

enum { TWK_UP, TWK_RIGHT, TWK_DOWN, TWK_LEFT };   /* as swipepad.h's ways */
enum { TWK_FLOOR, TWK_WALL, TWK_CRUMBLE, TWK_PLATE, TWK_DOOR, TWK_ICE };

typedef struct {
    int w, h;
    uint8_t t[TWK_CELLS];                       /* TWK_* tiles */
    uint8_t grp[TWK_CELLS];                     /* a plate's or a door's colour; a crumbling tile's number */
    int start[2], exit[2];
    int ncr;                                    /* crumbling tiles */
} twk_map;

typedef struct {
    uint8_t p[2];                               /* the walkers' places */
    uint16_t gone;                              /* the crumbling tiles fallen (bit k: tile number k) */
} twk_state;

/* a move's steps, for the animation: walker i's place after each tick (both move a tile a tick), n[i] ticks moved;
 * bump[i]: it tried and was stopped at once (a nudge) */
typedef struct {
    int n[2], bump[2];
    uint8_t at[2][TWK_LONGEST];
} twk_trace;

typedef struct {
    twk_map m;
    twk_state s;
    twk_state hist[TWK_HIST];                   /* the state before each move */
    int nh, moves, par;
} twk_game;

typedef struct {
    int w, h;
    int sym;                                    /* the walls' symmetry: 0 none, 1 left-right, 2 a half turn, 3 up-down */
    int walls;                                  /* the walls laid first, percent of the tiles */
    int crumble, plates, ice;                   /* crumbling tiles, colours of plates and doors (0..2), ice tiles */
    int lo, hi;                                 /* the par sought */
    int iters;                                  /* the changes tried */
    int tries;                                  /* the maps begun afresh at most (0: 60) */
} twk_spec;

int  twk_parse(twk_map *m, const char *text);   /* 0: not a map */
void twk_text(const twk_map *m, char *out);     /* TWK_CELLS + TWK_MAXH + 1 bytes */
int  twk_mirror_way(int dir);                   /* the mirror walker's way for a move */
int  twk_door_open(const twk_map *m, const twk_state *s, int cell);
int  twk_passable(const twk_map *m, const twk_state *s, int cell);   /* a tile a walker may step onto (walls,
                                                                     * gaps, shut doors not) */
/* a move from s: 1 and the state after it in *out (tr: its steps, may be NULL); 0 neither walker moves */
int  twk_try(const twk_map *m, const twk_state *s, int dir, twk_state *out, twk_trace *tr);
int  twk_solved(const twk_map *m, const twk_state *s);
/* the fewest moves from s to the exits (0 solved, -1 none, -2 the search too big), the first one's way in *dir (may be
 * NULL); *states: how many states the search went through (may be NULL) */
int  twk_solve(const twk_map *m, const twk_state *s, int *dir, int *states);
void twk_begin(twk_game *g);                    /* the walkers at their starts; no moves */
int  twk_move(twk_game *g, int dir);            /* 1 played (kept for undo), 0 refused */
int  twk_undo(twk_game *g);
int  twk_hint(const twk_game *g);               /* the next move of a shortest way; -1 none from here */
int  twk_script(twk_game *g, const char *moves);
/* a puzzle made by chance to the spec: 1, or 0 when none was found in the par sought (the best one kept) */
int  twk_make(twk_map *m, const twk_spec *spec, uint32_t seed);
void twk_variant(int variant, twk_spec *spec);
void twk_random(twk_game *g, int variant, uint32_t seed);
extern const char *const twk_levels[TWK_LEVELS];
extern const uint8_t twk_level_par[TWK_LEVELS];
int  twk_level(twk_game *g, int level);         /* 0: none */

#endif
