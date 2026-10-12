/*
 * Nonograms' rules: the line solver on known lines, the house's pictures (well formed, solvable line by line), the
 * pictures drawn by chance from many seeds (each solvable line by line, so one solution), the player's squares, runs
 * painted, undo, and whole games played from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/nonogram.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a line: "?" unknown, "#" on, "." off */
static void line_of(const char *s, uint8_t *l)
{
    for (int i = 0; s[i]; i++) l[i] = s[i] == '#' ? NG_ON : s[i] == '.' ? NG_OFF : NG_UNKNOWN;
}

static int line_is(const uint8_t *l, const char *s)
{
    for (int i = 0; s[i]; i++)
        if (l[i] != (s[i] == '#' ? NG_ON : s[i] == '.' ? NG_OFF : NG_UNKNOWN)) return 0;
    return 1;
}

int main(void)
{
    uint8_t l[NG_MAX];

    /* the line solver: overlaps, a full line, an empty one, what known squares force, a contradiction */
    static const uint8_t r8[] = {8}, r32[] = {3, 2}, r5[] = {5}, r11[] = {1, 1}, r3[] = {3};
    line_of("??????????", l);
    CHECK(ng_line(10, r8, 1, l) == 6 && line_is(l, "??######??"));
    line_of("?????", l);
    CHECK(ng_line(5, r5, 1, l) == 5 && line_is(l, "#####"));
    line_of("?????", l);
    CHECK(ng_line(5, NULL, 0, l) == 5 && line_is(l, "....."));
    line_of("??????", l);
    CHECK(ng_line(6, r32, 2, l) == 6 && line_is(l, "###.##"));
    line_of("?#???", l);
    CHECK(ng_line(5, r11, 2, l) >= 1 && line_is(l, ".#.??"));
    line_of("?.???", l);
    CHECK(ng_line(5, r3, 1, l) == 4 && line_is(l, "..###"));
    line_of("##.##", l);
    CHECK(ng_line(5, r3, 1, l) < 0);

    /* the house's pictures: square, their letters known, solvable line by line, a few of each size */
    int sizes[16] = {0};
    for (int i = 0; i < ng_npictures; i++) {
        const ng_picture *p = &ng_pictures[i];
        int ok = 1;
        for (int y = 0; y < p->n; y++) ok &= (int)strlen(p->rows[y]) == p->n;
        CHECK(ok);
        if (!ok) continue;
        ng_game g;
        CHECK(ng_load_picture(&g, i));
        int on = 0, coloured = 1;
        for (int s = 0; s < p->n * p->n; s++) {
            on += g.sol[s];
            if (g.sol[s] != (g.colour[s] != 0)) coloured = 0;
        }
        CHECK(coloured);
        CHECK(on > 0);
        int solvable = ng_solvable(p->n, g.sol);
        if (!solvable) printf("picture %d (%dx%d) is not solvable line by line\n", i, p->n, p->n);
        CHECK(solvable);
        sizes[p->n]++;
    }
    CHECK(sizes[5] >= 4 && sizes[10] >= 4 && sizes[15] >= 3);

    /* the pictures from many seeds: solvable line by line (one solution), the same from the same seed */
    int house = 0, drawn = 0;
    for (int v = 0; v < NG_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 150; seed++) {
            ng_game g, h;
            ng_new(&g, v, seed * 2654435761u);
            ng_new(&h, v, seed * 2654435761u);
            CHECK(g.n == (v == 0 ? 5 : v == 1 ? 10 : 15));
            CHECK(!memcmp(g.sol, h.sol, sizeof g.sol));
            CHECK(ng_solvable(g.n, g.sol));
            if (g.pic >= 0) house++;
            else drawn++;
            for (int i = 0; i < g.n; i++) CHECK(g.runs[0][i] <= NG_RUNS && g.runs[1][i] <= NG_RUNS);
        }
    printf("nonogram: %d house pictures, %d drawn by chance\n", house, drawn);
    CHECK(house > 100 && drawn > 100);

    /* the player's squares: a tap fills, again empties; crosses; a run painted in one action; undo */
    ng_game g;
    ng_load_picture(&g, 0);                     /* the heart */
    CHECK(ng_script(&g, "b1") && g.sq[1] == NG_FILL && g.moves == 1);
    CHECK(ng_script(&g, "b1") && g.sq[1] == NG_EMPTY && g.moves == 2);
    CHECK(ng_script(&g, "xa1") && g.sq[0] == NG_CROSS);
    CHECK(ng_script(&g, "a2-e2") && g.moves == 4);
    for (int x = 0; x < 5; x++) CHECK(g.sq[5 + x] == NG_FILL);
    CHECK(ng_line_done(&g, 0, 1) && !ng_line_done(&g, 0, 0));
    CHECK(ng_undo(&g) >= 0 && g.sq[5] == NG_EMPTY && g.sq[9] == NG_EMPTY && g.moves == 3);
    CHECK(!ng_script(&g, "a1-b2"));             /* not a line */
    CHECK(!ng_paint(&g, 0, 0, NG_CROSS));       /* nothing changes */
    CHECK(ng_script(&g, "xc2 a2-e2") && g.sq[7] == NG_CROSS && g.sq[5] == NG_FILL && g.sq[9] == NG_FILL);
    CHECK(ng_undo(&g) >= 0 && ng_undo(&g) >= 0 && g.sq[7] == NG_EMPTY && g.sq[5] == NG_EMPTY);
    CHECK(ng_script(&g, "b1 d1 a2-e2 a3-e3 b4-d4 c5"));
    CHECK(ng_solved(&g));                       /* the cross in a1 is only a note */
    while (ng_undo(&g) >= 0) {}
    CHECK(g.moves == 0);
    for (int s = 0; s < 25; s++) CHECK(g.sq[s] == NG_EMPTY);

    /* whole games from seeds: solved by its picture's squares, not before the last one */
    for (uint32_t seed = 1; seed <= 40; seed++)
        for (int v = 0; v < NG_VARIANTS; v++) {
            ng_new(&g, v, seed);
            int on = 0;
            for (int s = 0; s < g.n * g.n; s++) on += g.sol[s];
            char m[16];
            snprintf(m, sizeof m, "=%d", on - 1);
            CHECK(ng_script(&g, m) && !ng_solved(&g));
            CHECK(ng_script(&g, "=1") && ng_solved(&g));
            CHECK(g.moves == on);
        }
    ng_load_picture(&g, 0);
    CHECK(ng_square(&g, "a1") == 0 && ng_square(&g, "e5") == 24 && ng_square(&g, "f1") < 0 && ng_square(&g, "a6") < 0);
    ng_new(&g, 2, 3);
    CHECK(ng_square(&g, "o15") == 224 && ng_square(&g, "a10") == 135);

    if (fails) {
        printf("nonogram: %d failed\n", fails);
        return 1;
    }
    printf("nonogram: all passed\n");
    return 0;
}
