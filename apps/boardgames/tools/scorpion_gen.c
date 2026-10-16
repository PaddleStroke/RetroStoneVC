/*
 * Scorpion's levels: src/games/scorpion_levels.h (tools/solgen.h: deals shuffled from fixed seeds, each proved
 * winnable by the rules' search, graded by plain games, chosen evenly from the easiest).
 * The classic rules.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/scorpion_gen apps/boardgames/tools/scorpion_gen.c \
 *       apps/boardgames/src/games/scorpion_rules.c && /tmp/scorpion_gen > apps/boardgames/src/games/scorpion_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/scorpion.h"
#include "solgen.h"

int main(int argc, char **argv)
{
    int scale = argc > 1 ? atoi(argv[1]) : 1;   /* a quick look: fewer deals */
    const sggame g = {"Scorpion", "scorpion", "scp", "SCP", &scp_search, 52, sg_pack52, scp_deal_cards,
                      300000, 100, 1500, 0, 0};
    const sggroup groups[] = {
        {0, 150 / scale, 1600 / scale, 0, 1},
    };
    sg_run(&g, groups, 1, 6, "the classic rules");
    return 0;
}
