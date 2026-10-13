/*
 * Yukon's levels: src/games/yukon_levels.h (tools/solgen.h: deals shuffled from fixed seeds, each proved
 * winnable by the rules' search, graded by plain games, chosen evenly from the easiest).
 * The classic rules.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/yukon_gen apps/boardgames/tools/yukon_gen.c \
 *       apps/boardgames/src/games/yukon_rules.c && /tmp/yukon_gen > apps/boardgames/src/games/yukon_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/yukon.h"
#include "solgen.h"

int main(int argc, char **argv)
{
    int scale = argc > 1 ? atoi(argv[1]) : 1;   /* a quick look: fewer deals */
    const sggame g = {"Yukon", "yukon", "yk", "YK", &yk_search, 52, sg_pack52, yk_deal_cards,
                      300000, 100, 1500, 0, 0};
    const sggroup groups[] = {
        {0, 150 / scale, 650 / scale, 0, 1},
    };
    sg_run(&g, groups, 1, 6, "the classic rules");
    return 0;
}
