/*
 * Mines for one player: the rules. A field of covered cells hides some mines. Uncover a cell: a mine ends the try (undo
 * takes it back); else the cell shows how many of its eight neighbours hide a mine, and a cell with none uncovers its
 * neighbours by itself. Flag the cells you know hide a mine. The field is cleared when every cell without a mine is
 * uncovered.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 9 x 9 with 10 mines, 8 x 12 with 15 (made for a phone), 16 x 16 with 40, 16 x 30 with 99; first the
 * four "no guessing" (variants 0..3), then the same four drawn by pure chance (classic: 4..7). The mines are laid
 * at the first uncovering, never on that cell nor round it, so it always opens an area. A no-guessing field is laid
 * again until a reasoner, starting from that first area, uncovers every safe cell without a guess; it reasons as a
 * player would:
 * a number whose mines are all found frees its other neighbours, a number with as many covered neighbours as mines
 * left makes them all mines, two numbers side by side compared (the cells only one of them sees: when one has as many
 * mines more as it sees cells more, those are mines and the other's own cells are safe), and at the end the count of
 * mines left. The same reasoner gives the hint (one safe cell, or a sure mine flagged, or a wrong flag taken away).
 * Cells: x + y * w from the top left. A try on a mine is a "blast": the field waits for undo (never a loss). Undo
 * takes back one action (an uncovering, a flag, a chord), all the way to the start; the mines stay as laid unless the
 * first uncovering is taken back too.
 * The notation (script, the screenshots, the tests): "d3.4" uncovers the cell x 3, y 4 (from 1; an uncovered number
 * whose flags are all set uncovers its other neighbours: a chord), "f3.4" flags or unflags it, "h" a hint, "+n" n
 * hints, "=" hints until cleared, "@" every safe cell uncovered but one (after the first uncovering).
 */
#ifndef MINES_H
#define MINES_H

#include <stdint.h>

#define MNS_VARIANTS 8
#define MNS_SIZES 4
#define MNS_CELLS 480
#define MNS_HIST 4096

enum { MNS_HIDDEN, MNS_OPEN, MNS_FLAG };
enum { MNS_DIG, MNS_MARK, MNS_UNDO };           /* the hint's kinds of step */

typedef struct {
    int variant, w, h, mines, logic;            /* logic: no guessing */
    uint32_t seed;
    int placed, first;                          /* the mines laid, for the first uncovering (first) */
    uint8_t mine[MNS_CELLS], adj[MNS_CELLS], st[MNS_CELLS];
    int blast;                                  /* the mine uncovered; -1 */
    uint16_t act[MNS_HIST];                     /* the actions: cell | kind << 12 */
    int na;
    int moves, opened, flags;
} mns_game;

void mns_size(int variant, int *w, int *h, int *mines);
/* the mines for a first uncovering at `first` (mine[]: 1 a mine); logic: laid again until no guess is needed. The
 * tries it took (1..) */
int  mns_lay(int w, int h, int mines, int logic, uint32_t seed, int first, uint8_t *mine);
/* 1: the reasoner clears the field from the first uncovering without a guess */
int  mns_logical(int w, int h, int mines, const uint8_t *mine, int first);

void mns_new(mns_game *g, int variant, uint32_t seed);
int  mns_dig(mns_game *g, int cell);            /* uncover (a covered cell) or chord (a number); 0: not allowed */
int  mns_flag(mns_game *g, int cell);           /* flag or unflag a covered cell; 0: not allowed */
int  mns_undo(mns_game *g);                     /* 0: nothing to undo */
int  mns_hint(const mns_game *g, int *kind);    /* the hint's cell and kind (MNS_DIG, MNS_MARK: flag or unflag;
                                                 * MNS_UNDO: a blast to take back); -1: none */
int  mns_play_hint(mns_game *g);                /* the hint played: its cell; -1 none */
int  mns_solved(const mns_game *g);
int  mns_flags_around(const mns_game *g, int cell);
int  mns_script(mns_game *g, const char *moves);

#endif
