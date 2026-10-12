/*
 * Peg Solitaire's rules: the boards' holes, the jumps, undo, the end, known solutions of the classic boards played to
 * one marble in the goal, the starts made at random (each with its way to the goal, followed), the search, the hint
 * (on the known way and off it), the stars, and every level (its way to the goal, the hint followed to the end).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/pegsolitaire.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int cell(const psgame *g, const char *s) { return ps_cell(g, s); }

/* the hint followed to the end: 1 when it ends with one marble in the goal */
static int by_hint(psgame *g)
{
    for (int i = 0; i < PS_MAXH && ps_status(g) == PS_PLAY; i++) {
        psmove m;
        if (ps_hint(g, &m) != 1 || !ps_play(g, m.from, m.to)) return 0;
    }
    return ps_gold(g);
}

int main(void)
{
    psgame g;

    /* the boards: their holes; the classic starts: the hole empty at the start, the goal */
    static const int holes[PS_BOARDS] = {33, 37, 15, 41, 45, 21};
    for (int b = 0; b < PS_BOARDS; b++) {
        ps_new(&g, b);
        CHECK(g.holes == holes[b] && g.pegs == holes[b] - 1 && g.holes <= PS_MAXH);
        int n = 0;
        for (int c = 0; c < PS_CELLS; c++) n += g.peg[c];
        CHECK(n == g.pegs && g.hole[g.goal] && ps_status(&g) == PS_PLAY && g.n == 0 && g.nsol == 0);
    }
    static const char *const empty[3] = {"d4", "d6", "a5"}, *const goal[3] = {"d4", "d2", "a5"};
    for (int b = 0; b < 3; b++) {
        ps_new(&g, b);
        CHECK(!g.peg[cell(&g, empty[b])] && g.goal == cell(&g, goal[b]));
    }
    ps_new(&g, PS_ENGLISH);
    CHECK(cell(&g, "a1") < 0 && cell(&g, "b2") < 0 && cell(&g, "c1") >= 0 && cell(&g, "a3") >= 0 && cell(&g, "h4") < 0);
    ps_new(&g, PS_FRENCH);
    CHECK(cell(&g, "b2") >= 0 && cell(&g, "f6") >= 0 && cell(&g, "a1") < 0);
    ps_new(&g, PS_TRIANGLE);
    CHECK(cell(&g, "e1") >= 0 && cell(&g, "b5") < 0 && cell(&g, "c3") >= 0 && cell(&g, "d3") < 0);
    ps_new(&g, PS_DIAMOND);
    CHECK(cell(&g, "e1") >= 0 && cell(&g, "e9") >= 0 && cell(&g, "a5") >= 0);
    CHECK(cell(&g, "a4") < 0 && cell(&g, "b2") < 0);
    ps_new(&g, PS_WIEGLEB);
    CHECK(cell(&g, "a4") >= 0 && cell(&g, "i6") >= 0 && cell(&g, "c3") < 0 && cell(&g, "d9") >= 0);
    ps_new(&g, PS_TRIANGLE6);
    CHECK(cell(&g, "f1") >= 0 && cell(&g, "a6") >= 0 && cell(&g, "b6") < 0);

    /* the jumps: the English start has four, all into the centre */
    ps_new(&g, PS_ENGLISH);
    psmove ms[PS_MAX_MOVES];
    int n = ps_moves(&g, ms);
    CHECK(n == 4);
    for (int i = 0; i < n; i++) CHECK(ms[i].to == cell(&g, "d4"));
    CHECK(!ps_play(&g, cell(&g, "d5"), cell(&g, "d4")));       /* a step, not a jump */
    CHECK(ps_play(&g, cell(&g, "f4"), cell(&g, "d4")));
    CHECK(!g.peg[cell(&g, "f4")] && !g.peg[cell(&g, "e4")] && g.peg[cell(&g, "d4")] && g.pegs == 31);
    CHECK(!ps_play(&g, cell(&g, "g4"), cell(&g, "e4")));       /* over the empty f4 */
    CHECK(!ps_play(&g, cell(&g, "d2"), cell(&g, "d4")));       /* into a full hole */
    CHECK(!ps_play(&g, cell(&g, "c3"), cell(&g, "e5")));       /* no diagonal jumps on the cross */
    CHECK(ps_undo(&g) && g.pegs == 32 && g.peg[cell(&g, "f4")] && g.peg[cell(&g, "e4")] && !g.peg[cell(&g, "d4")]);
    CHECK(!ps_undo(&g));

    /* the triangle: the jumps along the slants too; solved to its top */
    ps_new(&g, PS_TRIANGLE);
    n = ps_moves(&g, ms);
    CHECK(n == 2);                              /* a3 -> a5 straight up, c3 -> a5 along the slant */
    CHECK(ps_play(&g, cell(&g, "c3"), cell(&g, "a5")) && !g.peg[cell(&g, "b4")]);
    ps_new(&g, PS_TRIANGLE);
    CHECK(ps_script(&g, "a3a5 c3a3 a5c3 a2a4 d2b4 b1b3 d1b1 a1c1 c1c3 b4d2 e1c3 c3a3 a3a5"));
    CHECK(g.pegs == 1 && ps_status(&g) == PS_ONE && ps_gold(&g) && ps_stars(&g) == 3);

    /* the French board's classic start, solved to its goal */
    ps_new(&g, PS_FRENCH);
    CHECK(ps_script(&g, "b6d6 e6c6 b4b6 b6d6 d5b5 d7d5 a5c5 d5b5 f5d5 d4d6 a3a5 a5c5 c4c6 c7c5 f3f5 f6f4 d3f3 b3d3 "
                        "f3f5 g5e5 e4e6 e7e5 c1c3 e1e3 d3f3 g3e3 d1d3 d3f3 f2f4 g4e4 e4e6 e6c6 c6c4 c4c2 b2d2"));
    CHECK(g.pegs == 1 && ps_status(&g) == PS_ONE && ps_gold(&g) && ps_last(&g) == cell(&g, "d2"));

    /* a known solution of the English board: 31 jumps, the last marble in the centre */
    static const char *const SOLUTION = "d6d4 b5d5 c7c5 e7c7 d5b5 a5c5 e5e7 g5e5 c4c6 c7c5 a4c4 c4c6 e4c4 g4e4 e4e6 "
                                        "e7e5 c3c5 c6c4 a3c3 d3b3 e2e4 g3e3 c1c3 c4c2 e1c1 c1c3 b3d3 d3f3 e5e3 f3d3 "
                                        "d2d4";
    ps_new(&g, PS_ENGLISH);
    CHECK(ps_script(&g, SOLUTION));
    CHECK(g.n == 31 && g.pegs == 1 && ps_status(&g) == PS_ONE && ps_gold(&g));
    CHECK(ps_last(&g) == cell(&g, "d4"));
    for (int i = 0; i < 31; i++) CHECK(ps_undo(&g));        /* undo all the way back */
    CHECK(g.pegs == 32 && !g.peg[cell(&g, "d4")] && !ps_undo(&g));

    /* the same, its last jump elsewhere: one marble, solved, but not in the goal: a star less */
    ps_new(&g, PS_ENGLISH);
    char alt[160];
    snprintf(alt, sizeof alt, "%.*s", (int)strlen(SOLUTION) - 5, SOLUTION);   /* all but d2d4: d2, d3 left */
    CHECK(ps_script(&g, alt) && g.pegs == 2);
    CHECK(ps_play(&g, cell(&g, "d3"), cell(&g, "d1")));
    CHECK(ps_status(&g) == PS_ONE && !ps_gold(&g) && ps_last(&g) == cell(&g, "d1") && ps_stars(&g) == 2);
    g.hints = 1;
    CHECK(ps_stars(&g) == 1);

    /* stuck with several marbles: no jump left */
    ps_new(&g, PS_ENGLISH);
    memset(g.peg, 0, sizeof g.peg);
    g.peg[cell(&g, "a3")] = g.peg[cell(&g, "g5")] = g.peg[cell(&g, "d1")] = 1;
    g.pegs = 3;
    CHECK(ps_status(&g) == PS_STUCK && ps_last(&g) < 0 && !ps_gold(&g));
    psmove hm;
    CHECK(ps_hint(&g, &hm) == 0);
    CHECK(!ps_script(&g, "a3a5"));

    /* the notation */
    ps_new(&g, PS_ENGLISH);
    char nm[3];
    ps_name(&g, cell(&g, "c7"), nm);
    CHECK(!strcmp(nm, "c7"));
    CHECK(!ps_script(&g, "d6d5") && !ps_script(&g, "z9d4") && !ps_script(&g, "d6"));

    /* a start set from its bits; its solution checked (it must end in the goal) */
    ps_new(&g, PS_ENGLISH);
    uint64_t full = ps_mask(&g);
    CHECK(ps_index(&g, cell(&g, "d4")) == 16 && ps_index(&g, cell(&g, "c7")) == 0 && ps_index(&g, 0) == -1);
    CHECK(ps_set(&g, PS_ENGLISH, cell(&g, "d4"), full, SOLUTION) && g.nsol == 31 && g.pegs == 32);
    CHECK(ps_mask(&g) == full);
    CHECK(!ps_set(&g, PS_ENGLISH, cell(&g, "d4"), full, alt));   /* two marbles left */
    CHECK(!ps_set(&g, PS_ENGLISH, 0, full, NULL));              /* a1: not a hole */

    /* the search: the English board from its 10th jump, to the centre; the path played */
    {
        ps_new(&g, PS_ENGLISH);
        CHECK(ps_script(&g, "d6d4 b5d5 c7c5 e7c7 d5b5 a5c5 e5e7 g5e5 c4c6 c7c5"));
        psmove path[PS_MAXH];
        clock_t a = clock();
        int r = ps_solve(&g, cell(&g, "d4"), path, 20000000L);
        printf("the English board from its 10th jump: %s in %.3f s\n", r == 1 ? "solved" : "not solved",
               (double)(clock() - a) / CLOCKS_PER_SEC);
        CHECK(r == 1);
        int k = g.pegs - 1;
        for (int i = 0; i < k && r == 1; i++) CHECK(ps_play(&g, path[i].from, path[i].to));
        CHECK(ps_gold(&g));
    }

    /* random starts on every board: their way to the goal followed; the hint on it; off it (a few random jumps), the
     * hint either leads to the goal or says there is no way (checked by a full search) */
    {
        int off_found = 0, off_none = 0;
        double worst = 0;
        for (int b = 0; b < PS_BOARDS; b++)
            for (uint32_t seed = 1; seed <= 60; seed++) {
                ps_random(&g, b, seed, 0);
                CHECK(g.pegs == g.nsol + 1 && g.pegs >= 2 && g.hole[g.goal] && ps_status(&g) == PS_PLAY);
                psgame h = g;
                for (int i = 0; i < g.nsol; i++) CHECK(ps_play(&h, g.sol[i].from, g.sol[i].to));
                CHECK(ps_gold(&h));
                h = g;
                CHECK(by_hint(&h) && h.hints == g.nsol && ps_stars(&h) == 2);
                h = g;
                uint32_t rs = pr_seed(seed * 31 + (uint32_t)b);
                for (int i = 0, k; i < 3 && (k = ps_moves(&h, ms)) > 0; i++) {
                    psmove x = ms[pr_below(&rs, k)];
                    ps_play(&h, x.from, x.to);
                }
                if (ps_status(&h) != PS_PLAY) continue;
                psmove m;
                clock_t a = clock();
                int r = ps_hint(&h, &m);
                double dt = (double)(clock() - a) / CLOCKS_PER_SEC;
                if (dt > worst) worst = dt;
                CHECK(r != -1);
                if (r == 1) {
                    off_found++;
                    CHECK(by_hint(&h));
                } else if (r == 0) {
                    off_none++;
                    CHECK(ps_solve(&h, -1, NULL, 1L << 40) == 0);
                }
            }
        printf("random starts: off their way, the hint found the goal %d times, no way %d times; the slowest hint "
               "%.3f s\n", off_found, off_none, worst);
        CHECK(off_found > 50 && worst < 1.0);
    }

    /* every level: its way to the goal, the marbles in its chapter's range, no level twice; the hint followed to the
     * goal from the start */
    {
        static const int lo[PS_CHAPTERS] = {3, 7, 11, 15, 20, 27}, hi[PS_CHAPTERS] = {6, 10, 14, 19, 26, 44};
        int boards[PS_BOARDS] = {0};
        for (int l = 0; l < PS_LEVELS; l++) {
            if (!ps_level(&g, l)) {
                printf("FAIL level %d: not loaded\n", l + 1);
                fails++;
                continue;
            }
            int c = l / (PS_LEVELS / PS_CHAPTERS);
            CHECK(g.pegs >= lo[c] && g.pegs <= hi[c] && g.nsol == g.pegs - 1);
            boards[g.board]++;
            for (int m = 0; m < l; m++)
                CHECK(ps_levels[m].board != ps_levels[l].board || ps_levels[m].goal != ps_levels[l].goal ||
                      ps_levels[m].pegs != ps_levels[l].pegs);
            psgame h = g;
            CHECK(by_hint(&h) && ps_stars(&h) == 2);
            CHECK(ps_status(&g) == PS_PLAY);
        }
        printf("%d levels: by board %d %d %d %d %d %d\n", PS_LEVELS, boards[0], boards[1], boards[2], boards[3],
               boards[4], boards[5]);
        for (int b = 0; b < PS_BOARDS; b++) CHECK(boards[b] > 0);
    }

    if (fails) { printf("pegsolitaire: %d failed\n", fails); return 1; }
    printf("pegsolitaire: all passed\n");
    return 0;
}
