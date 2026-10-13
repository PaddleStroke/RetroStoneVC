/*
 * Golf's levels: src/games/golfpatience_levels.h (tools/solgen.h: deals shuffled from fixed seeds, each proved
 * winnable by the rules' search, graded by plain games, chosen evenly from the easiest).
 * Three chapters where a king and an ace follow each other, three where they do not.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/golfpatience_gen apps/boardgames/tools/golfpatience_gen.c \
 *       apps/boardgames/src/games/golfpatience_rules.c \
 *       && /tmp/golfpatience_gen > apps/boardgames/src/games/golfpatience_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/golfpatience.h"
#include "solgen.h"

int main(int argc, char **argv)
{
    int scale = argc > 1 ? atoi(argv[1]) : 1;   /* a quick look: fewer deals */
    const sggame g = {"Golf", "golfpatience", "pg", "PG", &pg_search, 52, sg_pack52, pg_deal_cards,
                      300000, 1000, 1500, 1, 0};
    const sggroup groups[] = {
        {PG_WRAP, 75 / scale, 300 / scale, 0, 1},
        {PG_NOWRAP, 75 / scale, 1700 / scale, 0, 1},
    };
    sg_run(&g, groups, 2, 6, "three chapters round the corner, three not");
    return 0;
}
