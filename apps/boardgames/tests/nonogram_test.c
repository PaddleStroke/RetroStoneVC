/*
 * Nonograms' rules: the line solver on known lines, every level's picture (well formed, solvable line by line, so one
 * solution; no two alike; growing in size; hints alone solve it), the pictures drawn by chance from many seeds (each
 * solvable line by line), the player's squares, runs painted, undo, the hint, and whole games played from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/nonogram.h"
#include "games/nonogram_levels.h"

static const ng_picture HEART = {5, {".r.r.", "rrrrr", "rrrrr", ".rrr.", "..r.."}};

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

    /* every level: square, its letters known, solvable line by line (so one solution), no two the same, never smaller
     * than the one before, 5 x 5 first and 15 x 15 last; hints alone solve it, from a wrong mark too */
    int bad = 0, sizes[NG_MAX + 1] = {0};
    for (int i = 0; i < NG_LEVELS; i++) {
        const ng_picture *p = &ng_levels[i];
        static ng_game g, h;
        int ok = ng_load_picture(&g, p, i), on = 0, coloured = 1;
        for (int s = 0; ok && s < p->n * p->n; s++) {
            on += g.sol[s];
            if (g.sol[s] != (g.colour[s] != 0)) coloured = 0;
        }
        ok = ok && coloured && on > 0 && on < p->n * p->n && ng_solvable(p->n, g.sol) && g.pic == i;
        ok = ok && (!i || p->n >= ng_levels[i - 1].n);
        for (int j = 0; ok && j < i; j++)
            if (ng_levels[j].n == p->n) {
                ng_load_picture(&h, &ng_levels[j], j);
                ok = memcmp(g.sol, h.sol, sizeof g.sol) != 0;
            }
        if (ok) {                               /* a wrong cross first: the hint puts it right */
            int w = 0;
            while (!g.sol[w]) w++;
            ok = ng_paint(&g, w, w, NG_CROSS) && ng_hint(&g) == w && g.sq[w] == NG_FILL;
        }
        for (int guard = 0; ok && !ng_solved(&g) && guard < NG_SQ; guard++) ok = ng_hint(&g) >= 0;
        ok = ok && ng_solved(&g) && ng_hint(&g) < 0 && g.hist.mistakes == 0;
        if (ok)                                 /* the hints filled the picture and crossed only empty squares */
            for (int s = 0; s < p->n * p->n; s++) ok &= g.sq[s] != (g.sol[s] ? NG_CROSS : NG_FILL);
        if (!ok) printf("nonogram level %d (%dx%d) is not right\n", i + 1, p->n, p->n);
        bad += !ok;
        sizes[p->n]++;
    }
    CHECK(bad == 0);
    CHECK(ng_levels[0].n == 5 && ng_levels[NG_LEVELS - 1].n == 15 && sizes[10] >= 20 && sizes[15] >= 25);
    {
        ng_game g;
        static const ng_picture ragged = {5, {".r.r.", "rrrr", "rrrrr", ".rrr.", "..r.."}},
                                odd = {5, {".r.r.", "rrzrr", "rrrrr", ".rrr.", "..r.."}};
        CHECK(!ng_load_picture(&g, &ragged, 0) && !ng_load_picture(&g, &odd, 0) && !ng_load_picture(&g, NULL, 0));
    }

    /* the pictures from many seeds: drawn by chance (never the frame kept for a draw that fails), solvable line by line
     * (one solution), the same from the same seed */
    int frames = 0, drawn = 0;
    for (int v = 0; v < NG_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 150; seed++) {
            ng_game g, h;
            ng_new(&g, v, seed * 2654435761u);
            ng_new(&h, v, seed * 2654435761u);
            CHECK(g.n == (v == 0 ? 5 : v == 1 ? 10 : 15));
            CHECK(!memcmp(g.sol, h.sol, sizeof g.sol));
            CHECK(ng_solvable(g.n, g.sol));
            if (g.pic < 0) drawn++;
            int frame = 1;
            for (int s = 0; s < g.n * g.n; s++)
                if (g.sol[s] != (s % g.n == 0 || s / g.n == 0 || s % g.n == g.n - 1 || s / g.n == g.n - 1)) frame = 0;
            frames += frame;
            for (int i = 0; i < g.n; i++) CHECK(g.runs[0][i] <= NG_RUNS && g.runs[1][i] <= NG_RUNS);
        }
    printf("nonogram: %d levels, %d pictures drawn by chance\n", NG_LEVELS, drawn);
    CHECK(frames == 0 && drawn == NG_VARIANTS * 150);

    /* the player's squares: a tap fills, again empties; crosses; a run painted in one action; undo */
    ng_game g;
    ng_load_picture(&g, &HEART, -1);          /* a heart */
    CHECK(ng_script(&g, "b1") && g.sq[1] == NG_FILL && g.hist.moves == 1);
    CHECK(ng_script(&g, "b1") && g.sq[1] == NG_EMPTY && g.hist.moves == 2);
    CHECK(ng_script(&g, "xa1") && g.sq[0] == NG_CROSS);
    CHECK(ng_script(&g, "a2-e2") && g.hist.moves == 4);
    for (int x = 0; x < 5; x++) CHECK(g.sq[5 + x] == NG_FILL);
    CHECK(ng_line_done(&g, 0, 1) && !ng_line_done(&g, 0, 0));
    CHECK(ng_undo(&g) >= 0 && g.sq[5] == NG_EMPTY && g.sq[9] == NG_EMPTY && g.hist.moves == 3);
    CHECK(!ng_script(&g, "a1-b2"));             /* not a line */
    CHECK(!ng_paint(&g, 0, 0, NG_CROSS));       /* nothing changes */
    CHECK(ng_script(&g, "xc2 a2-e2") && g.sq[7] == NG_CROSS && g.sq[5] == NG_FILL && g.sq[9] == NG_FILL);
    CHECK(ng_undo(&g) >= 0 && ng_undo(&g) >= 0 && g.sq[7] == NG_EMPTY && g.sq[5] == NG_EMPTY);
    CHECK(ng_script(&g, "b1 d1 a2-e2 a3-e3 b4-d4 c5"));
    CHECK(ng_solved(&g));                       /* the cross in a1 is only a note */
    while (ng_undo(&g) >= 0) {}
    CHECK(g.hist.moves == 0);
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
            CHECK(g.hist.moves == on);
        }
    ng_load_picture(&g, &HEART, -1);
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
