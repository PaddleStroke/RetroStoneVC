/*
 * Baker's Dozen's levels: src/games/bakersdozen_levels.h (tools/solgen.h: deals shuffled from fixed seeds, each proved
 * winnable by the rules' search, graded by plain games, chosen evenly from the easiest).
 * The classic rules.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/bakersdozen_gen apps/boardgames/tools/bakersdozen_gen.c \
 *       apps/boardgames/src/games/bakersdozen_rules.c && /tmp/bakersdozen_gen > apps/boardgames/src/games/bakersdozen_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/bakersdozen.h"
#include "solgen.h"

int main(int argc, char **argv)
{
    int scale = argc > 1 ? atoi(argv[1]) : 1;   /* a quick look: fewer deals */
    const sggame g = {"Baker's Dozen", "bakersdozen", "bkd", "BKD", &bkd_search, 52, sg_pack52, bkd_deal_cards,
                      60000, 100, 1500, 0, 0};
    const sggroup groups[] = {
        {0, 150 / scale, 600 / scale, 0, 1},
    };
    sg_run(&g, groups, 1, 6, "the classic rules");
    return 0;
}
