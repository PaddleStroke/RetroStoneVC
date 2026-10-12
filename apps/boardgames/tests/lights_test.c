/*
 * Lights' rules: the GF(2) solver against a full search on the 3 x 3 board (every one of its 512 boards: the fewest
 * presses found by trying every set of presses) and on random 5 x 5 boards (the four ways tried by hand), the boards
 * made from many seeds (always solvable, within their level's range, fast enough on 7 x 7), presses and undo, hints
 * played to the end, and whole games from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/lights.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static void apply(int n, uint8_t *on, const uint8_t *press)
{
    for (int c = 0; c < n * n; c++)
        if (press[c]) lmp_toggle(n, on, c);
}

static int all_off(int n, const uint8_t *on)
{
    for (int c = 0; c < n * n; c++)
        if (on[c]) return 0;
    return 1;
}

int main(void)
{
    /* 3 x 3: every board, the fewest presses by trying all 512 sets */
    int fewest[512];
    for (int b = 0; b < 512; b++) fewest[b] = 99;
    for (int m = 0; m < 512; m++) {
        uint8_t on[LMP_CELLS] = {0};
        for (int c = 0; c < 9; c++)
            if (m >> c & 1) lmp_toggle(3, on, c);
        int b = 0;
        for (int c = 0; c < 9; c++) b |= on[c] << c;
        if (__builtin_popcount((unsigned)m) < fewest[b]) fewest[b] = __builtin_popcount((unsigned)m);
    }
    int agree = 1;
    for (int b = 0; b < 512; b++) {
        uint8_t on[LMP_CELLS] = {0}, press[LMP_CELLS];
        for (int c = 0; c < 9; c++) on[c] = (uint8_t)(b >> c & 1);
        int k = lmp_solve(3, on, press);
        int w = 0;
        for (int c = 0; c < 9; c++) w += press[c];
        apply(3, on, press);
        if (k != fewest[b] || w != k || !all_off(3, on)) agree = 0;
    }
    CHECK(agree);

    /* 5 x 5: the presses are not independent (two quiet patterns, four ways): random boards, the way found turns
     * them off and no other of the four ways is shorter */
    static const char *Q1 = "10101" "10101" "00000" "10101" "10101", *Q2 = "01110" "10101" "11011" "10101" "01110";
    uint32_t s = pr_seed(99);
    int ok5 = 1, unsolvable = 0;
    for (int t = 0; t < 2000; t++) {
        uint8_t on[LMP_CELLS] = {0}, press[LMP_CELLS], p2[LMP_CELLS];
        for (int c = 0; c < 25; c++) on[c] = (uint8_t)pr_below(&s, 2);
        int k = lmp_solve(5, on, press);
        if (k < 0) { unsolvable++; continue; }
        uint8_t off[LMP_CELLS];
        memcpy(off, on, sizeof off);
        apply(5, off, press);
        if (!all_off(5, off)) ok5 = 0;
        for (int q = 1; q < 4; q++) {           /* the way plus the quiet patterns */
            int w = 0;
            for (int c = 0; c < 25; c++) {
                p2[c] = press[c] ^ (uint8_t)((q & 1) && Q1[c] == '1') ^ (uint8_t)((q & 2) && Q2[c] == '1');
                w += p2[c];
            }
            memcpy(off, on, sizeof off);
            apply(5, off, p2);
            if (!all_off(5, off) || w < k) ok5 = 0;
        }
    }
    CHECK(ok5);
    CHECK(unsolvable > 1000 && unsolvable < 2000);  /* three boards in four cannot be turned off */

    /* the boards made: solvable, within their range, all three sizes over many seeds; timing on 7 x 7 */
    static const int lo[3] = {4, 9, 16}, hi[3] = {6, 15, 30};
    for (int v = 0; v < LMP_VARIANTS; v++) {
        int bad = 0, same = 0;
        clock_t t0 = clock();
        uint8_t prev[LMP_CELLS] = {0};
        for (uint32_t seed = 1; seed <= 500; seed++) {
            lmp_game g;
            lmp_new(&g, v, seed);
            if (g.n != lmp_size(v) || g.best < lo[v] || g.best > hi[v] || lmp_solved(&g)) bad++;
            if (!memcmp(prev, g.on, LMP_CELLS)) same++;
            memcpy(prev, g.on, LMP_CELLS);
            /* played to the end by hints: exactly the fewest presses */
            if (!lmp_script(&g, "=") || !lmp_solved(&g) || g.moves != g.best) bad++;
        }
        double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC / 500;
        printf("lights %dx%d: %.3f ms a board\n", lmp_size(v), lmp_size(v), ms);
        CHECK(bad == 0);
        CHECK(same <= (v ? 0 : 8));             /* 3 x 3 has only 294 boards in its range: a few repeat */
        CHECK(ms < 20);
    }

    /* presses and undo */
    lmp_game g;
    uint8_t on[LMP_CELLS] = {0};
    lmp_load(&g, 5, on);
    CHECK(lmp_solved(&g) && g.best == 0);
    CHECK(lmp_script(&g, "c3"));
    CHECK(lmp_lit(&g) == 5 && g.on[12] && g.on[7] && g.on[11] && g.on[13] && g.on[17]);
    CHECK(lmp_script(&g, "a1"));
    CHECK(lmp_lit(&g) == 8 && g.on[0] && g.on[1] && g.on[5]);
    CHECK(lmp_undo(&g) == 0 && lmp_lit(&g) == 5 && g.moves == 1);
    CHECK(lmp_undo(&g) == 12 && lmp_solved(&g) && g.moves == 0);
    CHECK(lmp_undo(&g) == -1);
    CHECK(!lmp_script(&g, "f1") && !lmp_script(&g, "a6") && !lmp_script(&g, "a0"));
    CHECK(lmp_hint(&g) == -1);

    /* whole games: random presses from a seed, then hints; undo all the way back gives the start again */
    for (uint32_t seed = 1; seed <= 200; seed++) {
        uint32_t r = pr_seed(seed);
        int v = (int)(seed % 3);
        lmp_new(&g, v, seed);
        uint8_t start[LMP_CELLS];
        memcpy(start, g.on, sizeof start);
        int n = g.n, k = pr_below(&r, 40);
        for (int i = 0; i < k; i++) lmp_press(&g, pr_below(&r, n * n));
        while (!lmp_solved(&g)) {
            int c = lmp_hint(&g);
            if (c < 0) break;
            lmp_press(&g, c);
        }
        CHECK(lmp_solved(&g));
        while (lmp_undo(&g) >= 0) {}
        CHECK(!memcmp(start, g.on, LMP_CELLS) && g.moves == 0);
    }

    if (fails) { printf("lights: %d failures\n", fails); return 1; }
    printf("lights: all passed\n");
    return 0;
}
