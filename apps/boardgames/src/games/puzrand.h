/*
 * The puzzles' own chance (Sudoku, Nonograms, the 15 Puzzle): a small xorshift the rules carry with them, seeded once
 * from the shell's ui_random(), so a puzzle is made the same way from the same seed (the tests try many; the pictures
 * repeat). No SDL: the rules and their tests use it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="sudoku" builds
 * sudoku.c and sudoku_*.c), so a shared .c file would be left out of one game's build or another's.
 */
#ifndef PUZRAND_H
#define PUZRAND_H

#include <stdint.h>

static inline uint32_t pr_next(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

static inline int pr_below(uint32_t *s, int n) { return n > 0 ? (int)(pr_next(s) % (uint32_t)n) : 0; }

/* a seed's first numbers are close to each other's for close seeds: stir it first */
static inline uint32_t pr_seed(uint32_t seed)
{
    uint32_t s = seed * 2654435761u + 0x6d2b79f5u;
    for (int i = 0; i < 4; i++) pr_next(&s);
    return s;
}

static inline void pr_shuffle(uint32_t *s, int *a, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = pr_below(s, i + 1), t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}

#endif
