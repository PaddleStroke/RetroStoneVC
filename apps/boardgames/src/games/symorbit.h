/*
 * What the makers of Dice Roll and Paint Roller share: the symmetries of a w x h board, so their puzzles are laid in
 * symmetric sets of cells (a cell and its images: an orbit) and look designed. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The symmetries: 0 a mirror across (left and right), 1 a mirror up and down, 2 a half turn, 3 both mirrors (and so
 * the half turn), 4 a quarter turn (a square board only). Ways as swipepad.h's: 0 up, 1 right, 2 down, 3 left.
 * Written once, here, as static inline functions: the build takes a game's own files only.
 */
#ifndef SYMORBIT_H
#define SYMORBIT_H

/* cell c's image under the step-th move of the symmetry (one step for 0..2; three for 3, 4: for 3 the mirror across,
 * the mirror up and down, the half turn; for 4 a quarter, a half and three quarters of a turn) */
static inline int sym_image(int sym, int w, int h, int c, int step)
{
    int x = c % w, y = c / w;
    switch (sym) {
    case 0: return w - 1 - x + y * w;
    case 1: return x + (h - 1 - y) * w;
    case 2: return w - 1 - x + (h - 1 - y) * w;
    case 3: return step == 0 ? w - 1 - x + y * w : step == 1 ? x + (h - 1 - y) * w : w - 1 - x + (h - 1 - y) * w;
    default:
        for (int k = 0; k <= step; k++) {      /* (x, y) -> (w - 1 - y, x) */
            int nx = w - 1 - y, ny = x;
            x = nx;
            y = ny;
        }
        return x + y * w;
    }
}

/* a way (an arrow's) under the same */
static inline int sym_way(int sym, int dir, int step)
{
    switch (sym) {
    case 0: return dir & 1 ? dir ^ 2 : dir;
    case 1: return dir & 1 ? dir : dir ^ 2;
    case 2: return dir ^ 2;
    case 3: return step == 0 ? (dir & 1 ? dir ^ 2 : dir) : step == 1 ? (dir & 1 ? dir : dir ^ 2) : dir ^ 2;
    default: return (dir + step + 1) & 3;
    }
}

/* the step of the half turn (2, 3, 4) or of the one mirror (0, 1) */
static inline int sym_half(int sym) { return sym == 3 ? 2 : sym == 4 ? 1 : 0; }

/* c's orbit: c and its images, each once, in out (4 at most); step[i]: the step that made it (-1: c itself) */
static inline int sym_orbit(int sym, int w, int h, int c, int *out, int *step)
{
    int n = 0, steps = sym <= 2 ? 1 : 3;
    out[n] = c;
    step[n++] = -1;
    for (int k = 0; k < steps; k++) {
        int e = sym_image(sym, w, h, c, k), dup = 0;
        for (int i = 0; i < n; i++) dup |= out[i] == e;
        if (!dup) { out[n] = e; step[n++] = k; }
    }
    return n;
}

#endif
