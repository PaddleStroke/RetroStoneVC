/*
 * Pyramid's levels: src/games/pyramid_levels.h (tools/solgen.h: deals shuffled from fixed seeds, each proved
 * winnable by the rules' search, graded by plain games, chosen evenly from the easiest).
 * Five chapters going through the stock three times, one going through it once.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/pyramid_gen apps/boardgames/tools/pyramid_gen.c \
 *       apps/boardgames/src/games/pyramid_rules.c && /tmp/pyramid_gen > apps/boardgames/src/games/pyramid_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/pyramid.h"
#include "solgen.h"

int main(int argc, char **argv)
{
    int scale = argc > 1 ? atoi(argv[1]) : 1;   /* a quick look: fewer deals */
    const sggame g = {"Pyramid", "pyramid", "py", "PY", &py_search, 52, sg_pack52, py_deal_cards,
                      300000, 1000, 1500, 0, 0};
    const sggroup groups[] = {
        {PY_THRICE, 125 / scale, 450 / scale, 0, 1},
        {PY_ONCE, 25 / scale, 1300 / scale, 0, 1},
    };
    sg_run(&g, groups, 2, 6, "five chapters through the stock three times, one once");
    return 0;
}
