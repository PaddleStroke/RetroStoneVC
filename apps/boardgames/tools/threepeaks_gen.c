/*
 * Three Peaks' levels: src/games/threepeaks_levels.h (tools/solgen.h: deals shuffled from fixed seeds, each proved
 * winnable by the rules' search, graded by plain games, chosen evenly from the easiest).
 * Four chapters where a king and an ace follow each other, two where they do not.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/threepeaks_gen apps/boardgames/tools/threepeaks_gen.c \
 *       apps/boardgames/src/games/threepeaks_rules.c && \
 *     /tmp/threepeaks_gen > apps/boardgames/src/games/threepeaks_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/threepeaks.h"
#include "solgen.h"

int main(int argc, char **argv)
{
    int scale = argc > 1 ? atoi(argv[1]) : 1;   /* a quick look: fewer deals */
    const sggame g = {"Three Peaks", "threepeaks", "tp", "TP", &tp_search, 52, sg_pack52, tp_deal_cards,
                      300000, 1000, 1500, 1, 0};
    const sggroup groups[] = {
        {TP_WRAP, 100 / scale, 500 / scale, 0, 1},
        {TP_NOWRAP, 50 / scale, 400 / scale, 0, 1},
    };
    sg_run(&g, groups, 2, 6, "four chapters round the corner, two not");
    return 0;
}
