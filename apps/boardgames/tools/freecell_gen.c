/*
 * FreeCell's levels: src/games/freecell_levels.h (tools/solgen.h: deals shuffled from fixed seeds, each proved
 * winnable by the rules' search, graded by plain games, chosen evenly from the easiest).
 * Our own deals, not the numbered ones.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/freecell_gen apps/boardgames/tools/freecell_gen.c \
 *       apps/boardgames/src/games/freecell_rules.c && /tmp/freecell_gen > apps/boardgames/src/games/freecell_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/freecell.h"
#include "solgen.h"

int main(int argc, char **argv)
{
    int scale = argc > 1 ? atoi(argv[1]) : 1;   /* a quick look: fewer deals */
    const sggame g = {"FreeCell", "freecell", "fc", "FC", &fc_search, 52, sg_pack52, fc_deal_cards,
                      300000, 100, 1500, 0, 0};
    const sggroup groups[] = {
        {0, 150 / scale, 600 / scale, 0, 1},
    };
    sg_run(&g, groups, 1, 6, "four free cells");
    return 0;
}
