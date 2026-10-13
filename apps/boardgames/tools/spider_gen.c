/*
 * Spider's levels: src/games/spider_levels.h (tools/solgen.h: deals shuffled from fixed seeds, each proved winnable
 * by the rules' search (best first: sp_search's eval), graded by plain games, chosen evenly from the easiest). Four
 * chapters of one suit, two of two suits; four suits stay random (the search proves too few of them within its budget).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/spgen apps/boardgames/tools/spider_gen.c \
 *       apps/boardgames/src/games/spider_rules.c && /tmp/spgen > apps/boardgames/src/games/spider_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/spider.h"
#include "solgen.h"

int main(int argc, char **argv)
{
    int scale = argc > 1 ? atoi(argv[1]) : 1;   /* a quick look: fewer deals */
    const sggame g = {"Spider", "spider", "sp", "SPI", &sp_search, 104, sp_pack, sp_deal_cards,
                      1000000, 60, 1500, 0, 1};
    const sggroup groups[] = {
        {SP_1SUIT, 100 / scale, 200 / scale, 0, 1},
        {SP_2SUITS, 50 / scale, 700 / scale, 0, 1},
    };
    sg_run(&g, groups, 2, 6, "four chapters of one suit, two of two suits");
    return 0;
}
