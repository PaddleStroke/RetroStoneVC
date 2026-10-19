/*
 * Mental Math's levels: src/games/mentalmath_levels.h. Our own drills, from the game's own maker (mmh_drill,
 * mentalmath_rules.c): six chapters of five steps, five drills a step, each step a spec (the operations, the ranges,
 * the tables, the share of sums missing an operand, how many sums and the time each is worth), the five drills of a
 * step from their own seeds, one sum more for the last two. Chapter 1 plus (to five for two drills, then to seven and
 * five to ten, to ten, crossing ten, to twenty, a missing number), 2 minus and then both, 3 to a hundred (tens, two
 * digits and one, two and two), 4 the times tables a few at a time, 5 division the same way, 6 everything together.
 * The drills are stored as their sums, not as seeds, so a change to the maker never changes a level. Each level's
 * sums, biggest answer, answers past five and missing operands go to stderr.
 * Deterministic (fixed seeds): run it again after a change and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/mmhgen apps/boardgames/tools/mentalmath_gen.c \
 *       apps/boardgames/src/games/mentalmath_rules.c && /tmp/mmhgen > apps/boardgames/src/games/mentalmath_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/mentalmath.h"

#define A (1u << MMH_ADD)
#define S (1u << MMH_SUB)
#define M (1u << MMH_MUL)
#define D (1u << MMH_DIV)
#define T(a) (1u << (a))
#define ALL_T 0x7fcu                            /* the tables of 2 to 10 */

/* {ops, add_lo, add_hi, sub_lo, sub_hi, form, tables, times_hi, missing, count, tenths} */
static const mmh_spec STEP[MMH_CHAPTERS][5] = {
    {{A, 2, 5, 0, 0, 0, 0, 0, 0, 8, 70}, {A, 3, 10, 0, 0, 0, 0, 0, 0, 10, 60}, {A, 8, 14, 0, 0, 0, 0, 0, 0, 10, 60},
     {A, 11, 20, 0, 0, 0, 0, 0, 0, 10, 60}, {A, 5, 20, 0, 0, 0, 0, 0, 30, 12, 60}},
    {{S, 0, 0, 2, 10, 0, 0, 0, 0, 10, 60}, {S, 0, 0, 11, 20, 2, 0, 0, 0, 10, 60}, {S, 0, 0, 5, 20, 0, 0, 0, 0, 12, 55},
     {A | S, 5, 20, 5, 20, 0, 0, 0, 0, 12, 55}, {A | S, 5, 20, 5, 20, 0, 0, 0, 30, 14, 55}},
    {{A | S, 20, 100, 20, 100, 1, 0, 0, 0, 10, 50}, {A, 20, 99, 0, 0, 2, 0, 0, 0, 10, 70},
     {S, 0, 0, 20, 99, 2, 0, 0, 0, 10, 70}, {A | S, 30, 99, 30, 99, 3, 0, 0, 0, 10, 90},
     {A | S, 20, 99, 20, 99, 0, 0, 0, 25, 12, 90}},
    {{M, 0, 0, 0, 0, 0, T(2) | T(10), 10, 0, 10, 50}, {M, 0, 0, 0, 0, 0, T(5) | T(3), 10, 0, 10, 50},
     {M, 0, 0, 0, 0, 0, T(4) | T(6), 10, 0, 12, 50}, {M, 0, 0, 0, 0, 0, T(7) | T(8) | T(9), 10, 0, 12, 55},
     {M, 0, 0, 0, 0, 0, ALL_T, 10, 25, 15, 50}},
    {{D, 0, 0, 0, 0, 0, T(2) | T(10), 10, 0, 10, 55}, {D, 0, 0, 0, 0, 0, T(5) | T(3), 10, 0, 10, 55},
     {D, 0, 0, 0, 0, 0, T(4) | T(6), 10, 0, 12, 55}, {D, 0, 0, 0, 0, 0, T(7) | T(8) | T(9), 10, 0, 12, 60},
     {D, 0, 0, 0, 0, 0, ALL_T, 10, 20, 15, 55}},
    {{A | S | M, 5, 30, 5, 30, 0, ALL_T, 10, 0, 12, 55}, {A | S | M | D, 5, 30, 5, 30, 0, ALL_T, 10, 0, 14, 55},
     {M | D, 0, 0, 0, 0, 0, ALL_T | T(11) | T(12), 12, 15, 15, 60},
     {A | S | M | D, 20, 99, 20, 99, 0, ALL_T, 10, 15, 15, 70},
     {A | S | M | D, 10, 99, 10, 99, 0, ALL_T | T(11) | T(12), 12, 35, 20, 70}},
};

/* the first step ramps: two drills to five, one to seven, then two of sums from five to ten (the results between) */
static const int OPEN_LO[5] = {2, 2, 3, 5, 5}, OPEN_HI[5] = {5, 5, 7, 10, 10};

int main(void)
{
    printf("/*\n"
           " * Mental Math's levels: our own drills, made by tools/mentalmath_gen.c (six chapters of five steps, five\n"
           " * drills a step). Each: \"<par seconds>:\" then its sums, \"7+5=?12\" with '?' before the number to find\n"
           " * (mentalmath.h, mmh_decode).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "static const char *const mmh_levels[MMH_LEVELS] = {\n");
    for (int ch = 0; ch < MMH_CHAPTERS; ch++)
        for (int st = 0; st < 5; st++)
            for (int i = 0; i < 5; i++) {
                mmh_spec sp = STEP[ch][st];
                if (i >= 3) sp.count++;
                if (!ch && !st) sp.add_lo = OPEN_LO[i], sp.add_hi = OPEN_HI[i];
                mmh_game g;
                mmh_drill(&sp, 7919u * (uint32_t)(ch * 25 + st * 5 + i + 1), &g);
                char s[1024];
                if (!mmh_encode(&g, s, sizeof s)) return 1;
                int big = 0, past = 0, miss = 0;
                for (int k = 0; k < g.nq; k++) {
                    int a = g.q[k].n[2] > g.q[k].n[0] ? g.q[k].n[2] : g.q[k].n[0];
                    big = a > big ? a : big, past += a > 5, miss += g.q[k].miss != 2;
                }
                fprintf(stderr, "%4d: %2d sums, biggest %3d, past five %2d, missing %2d\n", ch * 25 + st * 5 + i + 1, g.nq,
                        big, past, miss);
                printf("    \"%s\", /* %d */\n", s, ch * 25 + st * 5 + i + 1);
            }
    printf("};\n");
    return 0;
}
