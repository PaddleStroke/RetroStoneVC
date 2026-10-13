/*
 * Klondike's levels: src/games/klondike_levels.h (tools/solgen.h: deals shuffled from fixed seeds, each proved
 * winnable by the rules' search, graded by plain games, chosen evenly from the easiest).
 * Four chapters drawing one card at a time, then two drawing three.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/klondike_gen apps/boardgames/tools/klondike_gen.c \
 *       apps/boardgames/src/games/klondike_rules.c && /tmp/klondike_gen > apps/boardgames/src/games/klondike_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/klondike.h"
#include "solgen.h"

int main(int argc, char **argv)
{
    int scale = argc > 1 ? atoi(argv[1]) : 1;   /* a quick look: fewer deals */
    const sggame g = {"Klondike", "klondike", "kl", "KL", &kl_search, 52, sg_pack52, kl_deal_cards,
                      300000, 100, 1500, 0, 0};
    const sggroup groups[] = {
        {KL_DRAW1, 100 / scale, 500 / scale, 0, 1},
        {KL_DRAW3, 50 / scale, 300 / scale, 0, 1},
    };
    sg_run(&g, groups, 2, 6, "four chapters drawing one card, two drawing three");
    return 0;
}
