/*
 * Peg Solitaire's rules: the boards' holes, the jumps, undo, the end, and a known solution of the English board
 * played to one marble in the centre.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/pegsolitaire.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int cell(const psgame *g, const char *s) { return ps_cell(g, s); }

int main(void)
{
    psgame g;

    /* the boards: their holes, the one empty at the start, the goal */
    static const int holes[PS_VARIANTS] = {33, 37, 15};
    static const char *const empty[PS_VARIANTS] = {"d4", "d6", "a5"}, *const goal[PS_VARIANTS] = {"d4", "d2", "a5"};
    for (int v = 0; v < PS_VARIANTS; v++) {
        ps_new(&g, v);
        CHECK(g.holes == holes[v]);
        CHECK(g.pegs == holes[v] - 1);
        int n = 0;
        for (int c = 0; c < PS_CELLS; c++) n += g.peg[c];
        CHECK(n == g.pegs);
        CHECK(!g.peg[cell(&g, empty[v])]);
        CHECK(g.goal == cell(&g, goal[v]));
        CHECK(ps_status(&g) == PS_PLAY);
    }
    ps_new(&g, PS_ENGLISH);
    CHECK(cell(&g, "a1") < 0 && cell(&g, "b2") < 0 && cell(&g, "c1") >= 0 && cell(&g, "a3") >= 0);
    ps_new(&g, PS_FRENCH);
    CHECK(cell(&g, "b2") >= 0 && cell(&g, "f6") >= 0 && cell(&g, "a1") < 0);
    ps_new(&g, PS_TRIANGLE);
    CHECK(cell(&g, "e1") >= 0 && cell(&g, "b5") < 0 && cell(&g, "c3") >= 0 && cell(&g, "d3") < 0);

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

    /* the triangle: the jumps along the slants too */
    ps_new(&g, PS_TRIANGLE);
    n = ps_moves(&g, ms);
    CHECK(n == 2);                              /* a3 -> a5 straight up, c3 -> a5 along the slant */
    CHECK(ps_play(&g, cell(&g, "c3"), cell(&g, "a5")) && !g.peg[cell(&g, "b4")]);
    ps_new(&g, PS_TRIANGLE);
    CHECK(ps_script(&g, "a3a5 c3a3 a5c3 a2a4 d2b4 b1b3 d1b1 a1c1 c1c3 b4d2 e1c3 c3a3 a3a5"));
    CHECK(g.pegs == 1 && ps_status(&g) == PS_ONE && ps_gold(&g));

    /* the French board's classic start, solved to its goal */
    ps_new(&g, PS_FRENCH);
    CHECK(ps_script(&g, "b6d6 e6c6 b4b6 b6d6 d5b5 d7d5 a5c5 d5b5 f5d5 d4d6 a3a5 a5c5 c4c6 c7c5 f3f5 f6f4 d3f3 b3d3 "
                        "f3f5 g5e5 e4e6 e7e5 c1c3 e1e3 d3f3 g3e3 d1d3 d3f3 f2f4 g4e4 e4e6 e6c6 c6c4 c4c2 b2d2"));
    CHECK(g.pegs == 1 && ps_status(&g) == PS_ONE && ps_gold(&g) && ps_last(&g) == cell(&g, "d2"));

    /* a known solution of the English board: 31 jumps, the last marble in the centre */
    ps_new(&g, PS_ENGLISH);
    static const char *const SOLUTION = "d6d4 b5d5 c7c5 e7c7 d5b5 a5c5 e5e7 g5e5 c4c6 c7c5 a4c4 c4c6 e4c4 g4e4 e4e6 "
                                        "e7e5 c3c5 c6c4 a3c3 d3b3 e2e4 g3e3 c1c3 c4c2 e1c1 c1c3 b3d3 d3f3 e5e3 f3d3 "
                                        "d2d4";
    CHECK(ps_script(&g, SOLUTION));
    CHECK(g.n == 31 && g.pegs == 1 && ps_status(&g) == PS_ONE && ps_gold(&g));
    CHECK(ps_last(&g) == cell(&g, "d4"));
    for (int i = 0; i < 31; i++) CHECK(ps_undo(&g));        /* undo all the way back */
    CHECK(g.pegs == 32 && !g.peg[cell(&g, "d4")] && !ps_undo(&g));

    /* the same, its last jump elsewhere: one marble, solved, but not in gold */
    ps_new(&g, PS_ENGLISH);
    char alt[160];
    snprintf(alt, sizeof alt, "%.*s", (int)strlen(SOLUTION) - 5, SOLUTION);   /* all but d2d4: d2, d3 left */
    CHECK(ps_script(&g, alt) && g.pegs == 2);
    CHECK(ps_play(&g, cell(&g, "d3"), cell(&g, "d1")));
    CHECK(ps_status(&g) == PS_ONE && !ps_gold(&g) && ps_last(&g) == cell(&g, "d1"));

    /* the end with several marbles: no jump left */
    ps_new(&g, PS_ENGLISH);
    memset(g.peg, 0, sizeof g.peg);
    g.peg[cell(&g, "a3")] = g.peg[cell(&g, "g5")] = g.peg[cell(&g, "d1")] = 1;
    g.pegs = 3;
    CHECK(ps_status(&g) == PS_STUCK && ps_last(&g) < 0 && !ps_gold(&g));
    CHECK(!ps_script(&g, "a3a5"));

    /* the notation */
    ps_new(&g, PS_ENGLISH);
    char nm[3];
    ps_name(&g, cell(&g, "c7"), nm);
    CHECK(!strcmp(nm, "c7"));
    CHECK(!ps_script(&g, "d6d5") && !ps_script(&g, "z9d4") && !ps_script(&g, "d6"));

    if (fails) { printf("pegsolitaire: %d failed\n", fails); return 1; }
    printf("pegsolitaire: all passed\n");
    return 0;
}
